/* -*- c-file-style: "java"; indent-tabs-mode: nil; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003 by Martin Pool <mbp@samba.org>
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
                 * A servant will not be corrected by words: for
                 * though he understand he will not answer.
                 *      -- Proverbs 29:19
                 */

/**
 * @file
 * @brief Daemon signal handling.
 *
 * Signals are handled differently in the daemon parent and its children.
 *
 * When the parent is killed, the entire process group is shut down, and the
 * pid file (if any) is removed.
 *
 * For both cases any temporary files created by the process are removed.
 **/

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/wait.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "dopt.h"
#include "exec.h"
#include "daemon.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// This stores the pid of the parent daemon.
// It's used to make sure that we only run the whole-group cleanup from inside the parent.
// Remains 0 before parent initialization is complete and when run from inetd.

volatile pid_t dcc_master_pid = 0;

static RETSIGTYPE dcc_daemon_terminate(int);

//---------------------------------------------------------------------------------------------
// Catch all relevant termination signals. Set up in parent and also applies to children.

void dcc_daemon_catch_signals()
{
    // SIGALRM is caught to allow for built-in timeouts when running test cases
 
    signal(SIGTERM, &dcc_daemon_terminate);
    signal(SIGINT, &dcc_daemon_terminate);

#ifdef __linux__
    signal(SIGHUP, &dcc_daemon_terminate);
    signal(SIGALRM, &dcc_daemon_terminate);
#endif
}

//---------------------------------------------------------------------------------------------

// Ignore hangup signal.
// This is only used in detached mode to make sure the daemon does not quit when whoever started it closes their terminal.  
// In nondetached mode, the signal is logged and causes an exit as normal.

void dcc_ignore_sighup(void)
{
#ifdef __linux__
    signal(SIGHUP, SIG_IGN);

    rs_trace("ignoring SIGHUP");
#endif
}

//---------------------------------------------------------------------------------------------

// Just log, remove pidfile, and exit.
// Called when a daemon gets a fatal signal.
// Some cleanup is done only if we're the master/parent daemon.

static RETSIGTYPE dcc_daemon_terminate(int whichsig)
{
    int am_parent;

    // Make sure to remove handler before re-raising signal
    signal(whichsig, SIG_DFL);

    am_parent = getpid() == dcc_master_pid;

    if (am_parent) 
	{
#ifdef HAVE_STRSIGNAL
        rs_log_info("%s", strsignal(whichsig));
#else
        rs_log_info("terminated by signal %d", whichsig);
#endif
    }

    dcc_cleanup_tempfiles();

    if (am_parent) 
	{
        dcc_remove_pid();

#ifndef _WIN32
		// kill whole group
        kill(0, whichsig);
#endif
    }

    raise(whichsig);
}

///////////////////////////////////////////////////////////////////////////////////////////////


} // namespace distcc
