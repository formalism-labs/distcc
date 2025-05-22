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

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#include <memory>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/util.h"
#include "common/hosts.h"
#include "common/bulk.h"
#include "common/exec.h"
#include "common/lock.h"
#include "common/timeval.h"
#include "common/compiler.h"

#include "client/client.h"
#include "client/implicit.h"
#include "client/where.h"
#include "client/compile.h"
#include "client/dopt.h"

namespace distcc
{

using std::auto_ptr;

///////////////////////////////////////////////////////////////////////////////////////////////

// Invoke a compiler locally.  This is, obviously, the alternative to dcc_compile_remote().

// The server does basically the same thing, but it doesn't call this routine because it wants 
// to overlap execution of the compiler with copying the input from the network.
//
// This routine used to exec() the compiler in place of distcc.  
// That is slightly more efficient, because it avoids the need to create, schedule, etc another process.  
// The problem is that in that case we can't clean up our temporary files, and (not so important) we can't
// log our resource usage.
//
// This is called with a lock on localhost already held.

int Client::compile_local(const Arguments &args)
{
    int ret;

    dcc_hostdef_local.note_execution(args);
    dcc_note_state(DCC_PHASE_COMPILE, File(args.input_file), "localhost");

    // We don't do any redirection of file descriptors when running locally,
    // so if for example cpp is being used in a pipeline we should be fine
    proc_t pid;
    if ((ret = dcc_spawn_child(args, pid, 0, 0, 0, 0)) != 0)
        return ret;

    int status;
    if ((ret = dcc_collect_child("cc", pid, status)))
        return ret;

    return dcc_critique_status(status, "compile", args.input_file, "localhost", true);
}

//---------------------------------------------------------------------------------------------

/**
 * Execute the commands in argv remotely or locally as appropriate.
 *
 * We may need to run cpp locally; we can do that in the background
 * while trying to open a remote connection.
 *
 * This function is slightly inefficient when it falls back to running
 * gcc locally, because cpp may be run twice.  Perhaps we could adjust
 * the command line to pass in the .i file.  On the other hand, if
 * something has gone wrong, we should probably take the most
 * conservative course and run the command unaltered.  It should not
 * be a big performance problem because this should occur only rarely.
 *
 * @param argv Command to execute.  Does not include 0='distcc'.
 * Treated as read-only, because it is a pointer to the program's real
 * argv.
 *
 * @param status On return, contains the waitstatus of the compiler or
 * preprocessor.  This function can succeed (in running the compiler) even if
 * the compiler itself fails.  If either the compiler or preprocessor fails,
 * @p status is guaranteed to hold a failure value.
 **/

int
Client::build_somewhere(Arguments &args, int sg_level, int &status)
{
    if (sg_level)
		return compile_local(args);

	if (!dcc_compiler)
		return EXIT_NO_COMPILER_SETTING;

	try
	{
		dcc_compiler->scan_args(args, config.on_server);
	}
	catch (const char *x)
    {
        // we need to scan the arguments even if we already know it's
        // local, so that we can pick up distcc client options
		int cpu_lock_fd;
		lock_local(cpu_lock_fd);
		return compile_local(args);
    }

#if 0
	// turned off because we never spend long in this state
	dcc_note_state(DCC_PHASE_STARTUP, input_fname, NULL);
#endif

	auto_ptr<dcc_hostdef> host;
	try
	{
		HostDefs hosts;
		int cpu_lock_fd;
		host.reset(new dcc_hostdef(hosts.lock_one(cpu_lock_fd)));
		
		if (host->mode == DCC_MODE_LOCAL)
		{
			// We picked localhost and already have a lock on it so no need to lock it now
			return compile_local(args);
		}

		int ret;
		proc_t cpp_pid;
		File cpp_fname;
		if ((ret = cpp_maybe(args, cpp_fname, cpp_pid) != 0))
			throw "cpp failed";

		Arguments args_stripped = args;
		dcc_compiler->strip_local_args(args_stripped, config.on_server);

		if ((ret = compile_remote(args_stripped, cpp_fname, cpp_pid, *host, status)) != 0) 
		{
			// Returns zero if we successfully ran the compiler, even if the compiler itself bombed out
			throw "remote compilation failed";
		}

		host->enjoyed_host();

		dcc_unlock(cpu_lock_fd);

		ret = dcc_critique_status(status, "compile", args.input_file, host->hostname, true);
		if (ret < 128)
			// either worked, or remote compile just simply failed,
			// e.g. with syntax error.  don't bother retrying
			return ret;
	}
	catch (const char *x)
	{
		if (host.get())
			host->disliked_host();
		build_fallback(args, host.get());
    }

	return 0;
}

//---------------------------------------------------------------------------------------------

int Client::build_fallback(Arguments &args, dcc_hostdef *host)
{
    if (!dcc_getenv_bool("DISTCC_FALLBACK", 1)) 
	{
        rs_log_warning("failed to distribute and fallbacks are disabled");
        return EXIT_DISTCC_FAILED;
    }

    // "You guys are so lazy!  Do I have to do all the work myself??"
    if (host)
	{
        rs_log(RS_LOG_WARNING|RS_LOG_NONAME,
           "failed to distribute '%s' to '%s', running locally instead",
           +args.input_file, host->hostdef_string);
    }
	else
	{
        rs_log_warning("failed to distribute, running locally instead");
    }

	int cpu_lock_fd;
	lock_local(cpu_lock_fd);
	return compile_local(args);
}

//---------------------------------------------------------------------------------------------

int Client::build_somewhere_timed(Arguments &args, int sg_level, int &status)
{
    int ret;
    struct timeval before, after, delta;

    if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    ret = build_somewhere(args, sg_level, status);

    if (gettimeofday(&after, NULL)) 
	{
        rs_log_warning("gettimeofday failed");
    }
	else 
	{
        // TODO: Show rate based on cpp size?  Is that meaningful?
        timeval_subtract(delta, after, before);

        rs_log(RS_LOG_INFO|RS_LOG_NONAME, "elapsed compilation time %ld.%06lds",
			delta.tv_sec, delta.tv_usec);
    }

    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
