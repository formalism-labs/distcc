/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool <mbp@samba.org>
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

/**
 * @file
 *
 * @brief Client-side RPC functions.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/wait.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/exec.h"
#include "common/rpc1.h"
#include "common/exitcode.h"
#include "common/util.h"
#include "common/bulk.h"
#include "common/hosts.h"
#include "common/state.h"

#include "client/client.h"
#include "client/clinet.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Transmit header for whole request

dcc_exitcode dcc_x_req_header(fd_t fd, enum dcc_protover protover)
{
     return dcc_x_token_int(fd, "DIST", protover);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_x_session_name(fd_t fd, const string &session)
{
	return dcc_x_token_string(fd, "SSNN", session);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_x_flags(fd_t fd, unsigned flag)
{
     return dcc_x_token_int(fd, "FLGS", flag);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_x_view_name(fd_t fd, const string &view)
{
	return dcc_x_token_string(fd, "VIEW", view);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_x_compile_dir(fd_t fd, const Directory &cdir)
{
	return dcc_x_token_string(fd, "CDIR", cdir.path());
}

//---------------------------------------------------------------------------------------------
// Transmit an argv array

dcc_exitcode dcc_x_argv(fd_t fd, const Arguments &args)
{
    dcc_exitcode ret;
    
    int argc = args.count();
    
    if (dcc_x_token_int(fd, "ARGC", (unsigned) argc))
        return EXIT_PROTOCOL_ERROR;
    
    for (int i = 0; i < argc; i++) 
	{
        if ((ret = dcc_x_token_string(fd, "ARGV", args[i])))
            return ret;
    }

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------
// Read the "DONE" token from the network that introduces a response

dcc_exitcode dcc_r_result_header(fd_t ifd, enum dcc_protover expect_ver)
{
    unsigned vers;
    dcc_exitcode ret;
    
    if ((ret = dcc_r_token_int(ifd, "DONE", vers)))
        return ret;

    if (vers != expect_ver) 
	{
        rs_log_error("got version %d not %d in response from server", vers, expect_ver);
        return EXIT_PROTOCOL_ERROR;
    }

    rs_trace("got response header");

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_cc_status(fd_t ifd, int &status)
{
    unsigned u_status;
    dcc_exitcode ret = dcc_r_token_int(ifd, "STAT", u_status);
    status = u_status;
    return ret;
}

//---------------------------------------------------------------------------------------------
// The second half of the client protocol: retrieve all results from the server

int Client::retrieve_results(fd_t net_fd, int &status, Arguments &args, dcc_hostdef &host)
{
    int ret;

	if ((ret = dcc_r_result_header(net_fd, host.protover)))
		return ret;

	// We've started to see the response, so the server is done compiling
	dcc_note_state(DCC_PHASE_RECEIVE);

    unsigned o_len, d_len, pdb_len;
	if ((ret = dcc_r_cc_status(net_fd, status))
		|| (ret = dcc_r_token_bulk(net_fd, "SERR", dcc_fd(STDERR_FILENO, 0), host.compr))
		|| (ret = dcc_r_token_bulk(net_fd, "SOUT", dcc_fd(STDOUT_FILENO, 0), host.compr))
		|| (ret = dcc_r_token_file(net_fd, "DOTO", File(args.output_file), o_len, host.compr))
		|| (ret = dcc_r_token_file(net_fd, "DOTD", File(args.dotd_file), d_len, host.compr))
		|| (ret = dcc_r_token_file(net_fd, ".PDB", File(args.pdb_file), pdb_len, host.compr)))
	{
        return ret;
	}

    // compiler succeeded, output is invalid (empty or nonexistent file)
    if (status == 0 && (o_len == 0 || (int) o_len == -1))
	{
		rs_log_error("remote compiler succeeded but output is invalid");
        return EXIT_IO_ERROR;
	}

	// compiler failed, there is an output file: it is safest to remove it
	if (status != 0 && o_len > 0 && (int) o_len != -1) 
	{
		rs_log_error("remote compiler failed but also returned output: removing file");
		File(args.output_file).remove();
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
