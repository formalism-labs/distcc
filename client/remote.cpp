/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/*
 * Send a compilation request to a remote server.
 */


#include "common/config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>
#ifdef __linux__
#include <sys/time.h>
#endif

#include "common/distcc.h"
#include "common/trace.h"
#include "common/rpc1.h"
#include "common/exitcode.h"
#include "common/util.h"
#include "common/hosts.h"
#include "common/exec.h"
#include "common/lock.h"
#include "common/bulk.h"

#include "client/client.h"
#include "client/clinet.h"
#include "client/compile.h"
#include "client/dopt.h"

#include "rvfc/text/defs.h"
#include "rvfc/filesys/defs.h"

namespace distcc
{

using namespace rvfc::Text;
using rvfc::CurrentDirectory;

///////////////////////////////////////////////////////////////////////////////////////////////

// TODO: If cpp finishes early and fails then perhaps break out of trying to connect.

// TODO: If we abort, perhaps kill the SSH child rather than closing the socket.  
// Closing while a lot of stuff has been written through might make us block until the 
// other side reads all the data.

//---------------------------------------------------------------------------------------------
// Open a connection using either a TCP socket or SSH.  
// Return input and output file descriptors (which may or may not be different.)

int 
dcc_hostdef::remote_connect(fd_t &to_net_fd, fd_t &from_net_fd, pid_t &ssh_pid)
{
    if (mode == DCC_MODE_TCP) 
	{
        ssh_pid = 0;
		int ret = dcc_connect_by_name(hostname, port, to_net_fd);
		if (ret != 0)
            return ret;
		from_net_fd = to_net_fd;
        return 0;
    }

#ifdef FEATURE_SSH_MODE
	if (mode == DCC_MODE_SSH) 
	{
		return dcc_ssh_connect(NULL, user, hostname, sh_command, from_net_fd, to_net_fd, ssh_pid);
    }
#endif // FEATURE_SSH_MODE

    rs_log_crit("impossible host mode");
    return EXIT_DISTCC_FAILED;
}

//---------------------------------------------------------------------------------------------

static int 
dcc_wait_for_cpp(const proc_t &cpp_pid, int &status, const File &input_fname)
{
    if (!cpp_pid)
		return 0;

    dcc_note_state(DCC_PHASE_CPP);
    // Wait for cpp to finish (if not already done), check the result, then send the .i file
    
	int ret = dcc_collect_child("cpp", cpp_pid, status);
    if (ret)
        return ret;

	// Although cpp failed, there is no need to try running the command locally, 
	// because we'd presumably get the same result.  
	// Therefore critique the command and log a message and return an indication
	// that compilation is complete.
    if (dcc_critique_status(status, "cpp", input_fname.path(), "localhost", false))
        return 0;

    return 0;
}

//---------------------------------------------------------------------------------------------
// Send a request across to the already-open server

// CPP_PID is the PID of the preprocessor running in the background.
// We wait for it to complete before reading its output.

static int
dcc_send_header(fd_t net_fd, const Arguments &args, dcc_hostdef &host, bool on_server, const string &session)
{
    int ret;
	unsigned flags = on_server ? CMD_FLAGS_ON_SERVER : 0;

    tcp_cork_sock(net_fd, 1);

	if ((ret = dcc_x_req_header(net_fd, host.protover))
		|| (ret = dcc_x_session_name(net_fd, +session))
		|| (ret = dcc_x_flags(net_fd, flags))
        || (ret = dcc_x_argv(net_fd, args)))
	{
        return ret;
	}

    return 0;
}

//---------------------------------------------------------------------------------------------

/**
 * Pass a compilation across the network.
 *
 * When this function is called, the preprocessor has already been
 * started in the background.  It may have already completed, or it
 * may still be running.  The goal is that preprocessing will overlap
 * with setting up the network connection, which may take some time
 * but little CPU.
 *
 * If this function fails, compilation will be retried on the local machine.
 *
 * @param argv Compiler command to run.
 *
 * @param cpp_fname Filename of preprocessed source.  May not be complete yet,
 * depending on @p cpp_pid.
 *
 * @param output_fname File that the object code should be delivered to.
 * 
 * @param cpp_pid If nonzero, the pid of the preprocessor.  Must be
 * allowed to complete before we send the input file.
 *
 * @param host Definition of host to send this job to.
 *
 * @param status on return contains the wait-status of the remote compiler.
 *
 * Returns 0 on success, otherwise error.  Returning nonzero does not
 * necessarily imply the remote compiler itself succeeded, only that
 * there were no communications problems.
 */

int
Client::compile_remote(Arguments &args, const File &cpp_fname,
	proc_t cpp_pid, dcc_hostdef &host, int &status)
{
    fd_t to_net_fd, from_net_fd;
    int ret;
    pid_t ssh_pid = 0;
    off_t doti_size = 0;

	struct timeval before;
    if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    host.note_execution(args);
    dcc_note_state(DCC_PHASE_CONNECT, args.input_file, host.hostname);

	// For ssh support, we need to allow for separate fds writing to and
	// reading from the network, because our connection to the ssh client may
	// be over pipes, which are one-way connections.

    status = 0;
    if ((ret = host.remote_connect(to_net_fd, from_net_fd, ssh_pid)))
        goto out;
    
    dcc_note_state(DCC_PHASE_SEND);

	// This waits for cpp and puts its status in *status.  If cpp failed, then
	// the connection will have been dropped and we need not bother trying to
	// get any response from the server.
    ret = dcc_send_header(to_net_fd, args, host, config.on_server, config.session);

	if (!config.on_server)
	{
		if ((ret = dcc_wait_for_cpp(cpp_pid, status, args.input_file))
			|| (ret = dcc_x_file(to_net_fd, cpp_fname, "DOTI", host.compr, &doti_size)))
			goto out;
	}
	else
	{
		if (config.on_server && !config.view)
		{
			rs_log_error("on server compilation requested but view was not specified");
			ret = EXIT_BAD_ARGUMENTS;
			goto out;
		}

		CurrentDirectory cwd;		
		if ((ret = dcc_x_view_name(to_net_fd, config.view))
			|| (ret = dcc_x_compile_dir(to_net_fd, cwd)))
			goto out; 
	}

    rs_trace("client finished sending request to server");
    tcp_cork_sock(to_net_fd, 0);
	// but it might not have been read in by the server yet; there's
	// 100kB or more of buffers in the two kernels.

	// OK, now all of the source has at least made it into the
	// client's TCP transmission queue, sometime soon the server will
	// start compiling it.
	dcc_note_state(DCC_PHASE_COMPILE, File(), host.hostname);

    if (dcc_fd_cmp(to_net_fd, from_net_fd)) 
	{
        // in ssh mode, we can start closing down early
        dcc_close(to_net_fd);
    }

    // If cpp failed, just abandon the connection, without trying to receive results
    if (ret == 0 && status == 0) 
        ret = retrieve_results(from_net_fd, status, args, host);

	// Close socket so that the server can terminate, rather than
	// making it wait until we've finished our work.
    dcc_close(from_net_fd);

	struct timeval after;
    if (gettimeofday(&after, NULL)) 
	{
        rs_log_warning("gettimeofday failed");
    }
	else
	{
        double secs, rate;
		dcc_calc_rate(doti_size, before, after, secs, rate);
		rs_log(RS_LOG_INFO|RS_LOG_NONAME,
			"%lu bytes from %s compiled on %s in %.4fs, rate %.0fkB/s",
			(unsigned long) doti_size, +args.input_file, +host.hostname, secs, rate);
    }

out:
    // Collect the SSH child.
	// Strictly this is unnecessary; it might slow the client down a little when things 
	// could otherwise be proceeding in the background.
	// But it helps make sure that we don't assume we succeeded when something possibly 
	// went wrong, and it allows us to account for the cost of the ssh child.
    int ssh_status;
    if (ssh_pid) 
		dcc_collect_child("ssh", ssh_pid, ssh_status); // ignore failure
    
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
