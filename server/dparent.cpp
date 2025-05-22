/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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

                    /* Near is thy forgetfulness of all things; and near the
                     * forgetfulness of thee by all.
                     *                 -- Marcus Aurelius
                     */


/**
 * @file
 *
 * Daemon parent.  Accepts connections, forks, etc.
 *
 * @todo Quite soon we need load management.  Basically when we think
 * we're "too busy" we should stop accepting connections.  This could
 * be because of the load average, or because too many jobs are
 * running, or perhaps just because of a signal from the administrator
 * of this machine.  In that case we want to do a blocking wait() to
 * find out when the current jobs are done, or perhaps a sleep() if
 * we're waiting for the load average to go back down.  However, we
 * probably ought to always keep at least one job running so that we
 * can make progress through the queue.  If you don't want any work
 * done, you should kill the daemon altogether.
 **/

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#elif defined(_WIN32)
#include "contrib/subproc/w32err.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <fcntl.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "dopt.h"
#include "exec.h"
#include "srvnet.h"
#include "types.h"
#include "daemon.h"
#include "netutil.h"

namespace distcc
{

using namespace rvfc::Text;
using rvfc::CurrentProcess;
using rvfc::Seconds;

///////////////////////////////////////////////////////////////////////////////////////////////

static void dcc_detach();
static void dcc_save_pid(pid_t);

#ifdef _WIN32
HANDLE *child_threads = 0;
#endif

//---------------------------------------------------------------------------------------------

StandaloneServer::StandaloneServer(bool noFork, bool runAsService) : no_fork(noFork), run_as_service(runAsService)
{
	dcc_term_flag = false;
#ifdef _WIN32
	termination_event = CreateEvent(0, TRUE, FALSE, 0); 
#endif
	setup_listen_socket();
	determine_worker_count();

#ifdef _WIN32
	_job += CurrentProcess();
#endif
}

//---------------------------------------------------------------------------------------------

StandaloneServer::~StandaloneServer()
{
#ifdef _WIN32
	if (listen_fd != -1)
		closesocket(listen_fd);
	termination_event.close();
#endif
}

//---------------------------------------------------------------------------------------------

void 
StandaloneServer::setup_listen_socket()
{
	if (dcc_socket_listen(arg_port, &listen_fd, opt_listen_addr) != 0)
		throw std::runtime_error("listen to socket failed");
	dcc_defer_accept(listen_fd);
	set_cloexec_flag(listen_fd, 1);
}

//---------------------------------------------------------------------------------------------

void
StandaloneServer::determine_worker_count()
{
	if (dcc_ncpus(n_cpus) == 0)
		rs_log_info("%d CPU%s online on this server", n_cpus, n_cpus == 1 ? "" : "s");

	// By default, allow one job per CPU, plus two for the pot.  
	// The extra ones are started to allow for a bit of extra concurrency so that the
	// machine is not idle waiting for disk or network IO.
	dcc_nkids = 0;
	dcc_max_kids = arg_max_jobs ? arg_max_jobs : 2 + n_cpus;

#ifdef _WIN32
	child_threads = (HANDLE *) calloc(dcc_max_kids, sizeof(HANDLE));
#endif

	rs_log_info("allowing up to %d active jobs", dcc_max_kids);
}

//---------------------------------------------------------------------------------------------
// Be a standalone server, with responsibility for sockets and forking children.
// Puts the daemon in the background and detaches from the controlling tty.

void
StandaloneServer::run()
{
#ifndef _WIN32
	if (!opt_no_detach) 
	{
		// Don't go into the background until we're listening and ready.  
		// This is useful for testing -- when the daemon detaches, 
		// we know we can go ahead and try to connect.
		dcc_detach();
    }
	else 
	{
        // Still create a new process group, even if not detached
        rs_trace("not detaching");
		int ret = dcc_new_pgrp();
        if (ret != 0)
			throw std::runtime_error("cannot create process group");
        dcc_save_pid(getpid());
    }

    // Don't catch signals until we've detached or created a process group
    dcc_daemon_catch_signals();

    // This is called in the master daemon, whether that is detached or not
    dcc_master_pid = getpid();
#endif // ! _WIN32

    if (no_fork) 
		nofork_parent();
	else 
		preforking_parent();

#ifdef _WIN32
	SetEvent(termination_event);
#endif
}

//---------------------------------------------------------------------------------------------

void
StandaloneServer::terminate()
{
	dcc_term_flag = true;

#ifdef _WIN32
	if (listen_fd != -1)
	{
		closesocket(listen_fd);
		listen_fd = -1;
	}
	WaitForSingleObject(termination_event, INFINITE);
	_job.terminate();
#endif // _WIN32
}

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

static void dcc_log_child_exited(pid_t kid)
{
	rs_log_info("child [%d] exited", (int) kid);
}

#else // ! _WIN32

static void dcc_log_child_exited(pid_t kid, int status)
{
    if (WIFSIGNALED(status)) 
	{
        int sig = WTERMSIG(status);
        int severity = sig == SIGTERM ? RS_LOG_INFO : RS_LOG_ERR;
        
        rs_log(severity, "child %d: signal %d (%s)", (int) kid, sig,
               WCOREDUMP(status) ? "core dumped" : "no core");
    }
	else if (WIFEXITED(status)) 
	{
        rs_log_info("child [%d] exited: exit status %d", (int) kid, WEXITSTATUS(status));
    }
}

#endif // ! _WIN32

//---------------------------------------------------------------------------------------------

/**
 * @sa dcc_wait_child(), which is used by a process that wants to do a blocking
 * wait for some task like cpp or gcc.
 *
 * @param must_reap If True, don't return until at least one child has been
 * collected.  Used when e.g. all our process slots are full.  In either case
 * we keep going until all outstanding zombies are collected.
 *
 * FIXME: Are blocking waits meant to collect all of them, or just one?  At
 * the moment it waits until all children have exited.
 **/

void 
StandaloneServer::reap_kids(bool must_reap)
{
#ifdef _WIN32
	while (!dcc_term_flag)
	{
		// NewProcess *proc = must_reap ? NewProcess::waitForAny() : NewProcess::waitForAny(Seconds(1));
		NewProcess *proc = NewProcess::waitForAny(Seconds(1));
		if (!proc)
		{
			if (must_reap)
				continue;
			break;
		}

		dcc_log_child_exited(proc->id());

		delete proc;
		--dcc_nkids;
		rs_trace("down to %d children", dcc_nkids);

		// If there are more children keep looking, but don't block once we've collected at least one
		must_reap = false;
	}

#else // ! _WIN32
    for (;;)
	{
        int status;
        pid_t kid = waitpid(WAIT_ANY, &status, must_reap ? 0 : WNOHANG);
        if (kid == 0) 
		{
            // nobody has exited
            break;
        }
		else if (kid != -1) 
		{
            // child exited
            --dcc_nkids;
            rs_trace("down to %d children", dcc_nkids);

            dcc_log_child_exited(kid, status);
        }
		else if (errno == ECHILD) 
		{
            // No children left?  That's ok, we'll go back to waiting for new connections
            break;          
        }
		else if (errno == EINTR) 
		{
            // If we got a SIGTERM or something, then on the next pass through the loop we'll find no children done, 
			// and we'll return to the top loop at which point we'll exit.
			// So no special action is required here.
            continue; // loop again
        }
		else
		{
            rs_log_error("wait failed: %s", strerror(errno));
            // e.g. too many open files; nothing we can do
            dcc_exit(EXIT_DISTCC_FAILED);
        }

        // If there are more children keep looking, but don't block once we've collected at least one
        must_reap = FALSE;
    }

#endif // ! _WIN32
}

//---------------------------------------------------------------------------------------------

// Main loop for no-fork mode.
// Much slower and may leak.  Should only be used when you want to run gdb on distccd.

void StandaloneServer::nofork_parent()
{
	dcc_log_daemon_started("non-forking daemon");   

	for (;;)
	{
        int acc_fd;
		fd_t _acc_fd;
        struct dcc_sockaddr_storage cli_addr;
        socklen_t cli_len;

        rs_log_info("waiting to accept connection");

        cli_len = sizeof cli_addr;
        acc_fd = accept(listen_fd, (struct sockaddr *) &cli_addr, &cli_len);
#ifdef _WIN32
		if (acc_fd == -1)
			translate_wsaerror();
#endif
		_acc_fd = dcc_fd(acc_fd, 1);
        if (acc_fd == -1 && errno == EINTR) 
		{
            ; 
        }
		else if (acc_fd == -1) 
		{
            rs_log_error("accept failed: %s", socket_error_str());
            dcc_exit(EXIT_CONNECT_FAILED);
        }
		else
		{
            dcc_service_job(_acc_fd, _acc_fd, (struct sockaddr *) &cli_addr, cli_len);
            dcc_close(_acc_fd);
        }
    }
}

//---------------------------------------------------------------------------------------------

// Save the pid of the child process into the pid file, if any.

// This is called from the parent so that we have the invariant that the pid file exists 
// before the parent exits, hich is useful for test harnesses.
// Otherwise, there is a race where the parent has exited and they try to go ahead and 
// read the child's pid, but it's not there yet.

static void dcc_save_pid(pid_t pid)
{
    FILE *fp;
    
    if (!arg_pid_file)
        return;

	fp = fopen(arg_pid_file, "wt");
    if (!fp) 
	{
        rs_log_error("failed to open pid file: %s: %s", arg_pid_file, strerror(errno));
        return;
    }

    fprintf(fp, "%ld\n", (long) pid);

    if (fclose(fp) == -1) 
	{
        rs_log_error("failed to close pid file: %s: %s", arg_pid_file, strerror(errno));
        return;
    }

    atexit(dcc_remove_pid);
}

//---------------------------------------------------------------------------------------------

// Remove our pid file on exit.
// Must be reentrant -- called from signal handler.

void dcc_remove_pid()
{
    if (!arg_pid_file)
        return;

    if (unlink(arg_pid_file)) 
        rs_log_warning("failed to remove pid file %s: %s", arg_pid_file, strerror(errno));
}

//---------------------------------------------------------------------------------------------

// Become a daemon, discarding the controlling terminal.
// Borrowed from rsync.
// This function returns in the child, but not in the parent.

#ifndef _WIN32

static void dcc_detach()
{
    int i;
    pid_t pid;
    pid_t sid;
    
    dcc_ignore_sighup();

    if ((pid = fork()) == -1) 
	{
        rs_log_error("fork failed: %s", strerror(errno));
        exit(EXIT_DISTCC_FAILED);
    }
	else if (pid != 0) 
	{
        // In the parent.  
		// This guy is about to go away so as to detach from the controlling process, but first save the child's pid.
        dcc_save_pid(pid);
        _exit(0);
    }
    
    // This is called in the detached child

    // detach from the terminal
#ifdef HAVE_SETSID
    if ((sid = setsid()) == -1)
        rs_log_error("setsid failed: %s", strerror(errno));
	else
        rs_trace("setsid to session %d", (int) sid);

#elif defined(TIOCNOTTY)
    i = open("/dev/tty", O_RDWR);
    if (i >= 0) 
	{
        ioctl(i, (int) TIOCNOTTY, (char *)0);      
        close(i);
    }
#endif // TIOCNOTTY
    
    // make sure that stdin, stdout an stderr don't stuff things up (library functions, for example)
    for (i = 0; i < 3; i++) 
	{
        close(i); 
        open(DEV_NULL, O_RDWR);
    }

    // If there's a lifetime limit on this server (for testing) then it needs to apply after detaching as well
    dcc_set_lifetime();
}

#endif // ! _WIN32

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
