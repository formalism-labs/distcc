/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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


			/* 18 Their bows also shall dash the young men
			 * to pieces; and they shall have no pity on
			 * the fruit of the womb; their eyes shall not
			 * spare children.
			 *		-- Isaiah 13 */

/**
 * @file
 *
 * Run compilers or preprocessors.
 *
 * The whole server is run in a separate process group and normally in a
 * separate session.  (It is not a separate session in --no-detach debug
 * mode.)  This allows us to cleanly kill off all children and all compilers
 * when the parent is terminated.
 *
 * @todo On Cygwin, fork() must be emulated and therefore will be
 * slow.  It would be faster to just use their spawn() call, rather
 * than fork/exec.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/types.h>
#include <sys/stat.h>

#include "rvfc/defs.h"
#include "rvfc/text/defs.h"

#ifdef _WIN32
#include "subproc/sub_proc.h"
#include "subproc/w32err.h"
#endif

#include "common/distcc.h"
#include "common/arg.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/exitcode.h"
#include "common/exec.h"
#include "common/lock.h"
#include "common/hosts.h"

namespace distcc
{

using rvfc::Path;

///////////////////////////////////////////////////////////////////////////////////////////////

static void dcc_inside_child(Arguments &args, const File *stdin_file,
	const File *stdout_file, const File *stderr_file) NORETURN;

static void dcc_execvp(char **argv) NORETURN;

//---------------------------------------------------------------------------------------------

void dcc_hostdef::note_execution(const Arguments &args)
{
    rs_log(RS_LOG_INFO|RS_LOG_NONAME, "exec on %s: %s", +hostdef_string, +args.toString());
}

//---------------------------------------------------------------------------------------------

/**
 * Redirect stdin/out/err.  Filenames may be NULL to leave them untouched.
 *
 * This is called when running a job remotely, but *not* when running
 * it locally, because people might e.g. want cpp to read from stdin.
 **/

#ifdef __linux__

static int 
dcc_redirect_fds(const File *stdin_file, const File *stdout_file, const File *stderr_file)
{
    int ret;
    
    if (stdin_file)
	{
		ret = dcc_redirect_fd(STDIN_FILENO, *stdin_file, O_RDONLY);
        if (ret)
            return ret;
	}
    
    if (stdout_file) 
	{
		ret = dcc_redirect_fd(STDOUT_FILENO, *stdout_file, O_WRONLY | O_CREAT | O_TRUNC);
        if (ret)
            return ret;
    }
    
    if (stderr_file) 
	{
		// Open in append mode, because the server will dump its own error
		// messages into the compiler's error file.
		ret = dcc_redirect_fd(STDERR_FILENO, *stderr_file, O_WRONLY | O_CREAT | O_APPEND);
        if (ret)
            return ret;
    }

    return 0;
}

#endif // __linux__

//---------------------------------------------------------------------------------------------

#ifdef __linux__

/**
 * Replace this program with another in the same process.
 *
 * Does not return, either execs the compiler in place, or exits with
 * a message.
 **/
static void 
dcc_execvp(char **argv)
{
    char *slash;
    
    execvp(argv[0], argv);

    /* If we're still running, the program was not found on the path.  One
     * thing that might have happened here is that the client sent an absolute
     * compiler path, but the compiler's located somewhere else on the server.
     * In the absence of anything better to do, we search the path for its
     * basename.
     *
     * Actually this code is called on both the client and server, which might
     * cause unintnded behaviour in contrived cases, like giving a full path
     * to a file that doesn't exist.  I don't think that's a problem. */

    slash = strrchr(argv[0], '/');
    if (slash)
        execvp(slash + 1, argv);
    
    /* shouldn't be reached */
    rs_log_error("failed to exec %s: %s", argv[0], strerror(errno));

    dcc_exit(EXIT_COMPILER_MISSING); /* a generalization, i know */
}

#endif // __linux__

//---------------------------------------------------------------------------------------------

#ifdef __linux__

/**
 * Called inside the newly-spawned child process to execute a command.
 * Either executes it, or returns an appropriate error.
 *
 * This routine also takes a lock on localhost so that it's counted
 * against the process load.  That lock will go away when the process
 * exits.
 *
 * In this current version locks are taken without regard to load limitation
 * on the current machine.  The main impact of this is that cpp running on
 * localhost will cause jobs to be preferentially distributed away from
 * localhost, but it should never cause the machine to deadlock waiting for
 * localhost slots.
 *
 * @param what Type of process to be run here (cpp, cc, ...)
 **/

static void 
dcc_inside_child(Arguments &args, const File *stdin_file, const File *stdout_file, const File *stderr_file) 
{
	int ret;
    
	ret = dcc_ignore_sigpipe(0);
	if (ret)
		goto fail; // set handler back to default

	// Ignore failure
	dcc_increment_safeguard();

	// do this last, so that any errors from previous operations are visible
	ret = dcc_redirect_fds(stdin_file, stdout_file, stderr_file);
	if (ret)
		goto fail;
    
	dcc_execvp(args.argv());

	ret = EXIT_DISTCC_FAILED;

fail:
	dcc_exit(ret);
}

#endif // __linux__

//---------------------------------------------------------------------------------------------

int 
dcc_new_pgrp()
{
#ifdef __linux__
    // If we're a session group leader, then we are not able to call setpgid().
	// However, setsid will implicitly have put us into a new process group, so we don't have to do anything.

    // Does everyone have getpgrp()?  It's in POSIX.1.
	// We used to call getpgid(0), but that is not available on BSD/OS
    if (getpgrp() == getpid()) 
	{
        rs_trace("already a process group leader");
        return 0;
    }
    
    if (setpgid(0, 0) == 0) 
	{
        rs_trace("entered process group");
        return 0;
    }

	rs_trace("setpgid(0, 0) failed: %s", strerror(errno));
	return EXIT_DISTCC_FAILED;

#else // ! __linux__
	return 0;

#endif // !__linux__
}

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

TemporaryFile::TemporaryFile(const File &file, bool create)
{
	if (!file)
		return;

	Path path = file.path();
//	SecurityDescriptor sd;
	SECURITY_ATTRIBUTES sd = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
	_h = CreateFile((LPCTSTR) +path,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		&sd,
		create ? CREATE_NEW : OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL /*| FILE_ATTRIBUTE_TEMPORARY /*| FILE_FLAG_DELETE_ON_CLOSE*/,
		NULL);

	if (_h == INVALID_HANDLE_VALUE)
	{
		char *msg;
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
			NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &msg, 0, NULL);
		rs_log_error("failed to open file: %s [%s]", +path, msg);
		LocalFree(msg);
	}
}

//---------------------------------------------------------------------------------------------

TemporaryFile::~TemporaryFile()
{
	if (_h != INVALID_HANDLE_VALUE)
		CloseHandle(_h);
}

#endif // _WIN32

//---------------------------------------------------------------------------------------------

#if 0

HANDLE open_temp_file(const char *name, bool create)
{
	SECURITY_ATTRIBUTES sec = { sizeof(SECURITY_ATTRIBUTES), NULL, TRUE };
	HANDLE h = CreateFile((LPCTSTR) name,
		GENERIC_READ | GENERIC_WRITE,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		&sec,
		create ? CREATE_NEW : OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY /*| FILE_FLAG_DELETE_ON_CLOSE*/,
		NULL);
	if (h == INVALID_HANDLE_VALUE)
		rs_log_error("failed to open file: %s", name);
	return h;
}

//---------------------------------------------------------------------------------------------

void close_temp_file(HANDLE h)
{
	if (h != INVALID_HANDLE_VALUE)
		CloseHandle(h);
}

#endif // 0

//---------------------------------------------------------------------------------------------

/**
 * Run @p argv in a child asynchronously.
 *
 * stdin, stdout and stderr are redirected as shown, unless those
 * filenames are NULL.  In that case they are left alone.
 *
 * @warning When called on the daemon, where stdin/stdout may refer to random
 * network sockets, all of the standard file descriptors must be redirected!
 **/

#ifdef _WIN32

WinFile *in_file = 0, *out_file = 0, *err_file = 0;

int 
dcc_spawn_child(const Arguments &args, proc_t &pid, const Directory *cwd,
	const File *stdin_fname, const File *stdout_fname, const File *stderr_fname)
{
//	HANDLE in_file = INVALID_HANDLE_VALUE, out_file = INVALID_HANDLE_VALUE, err_file = INVALID_HANDLE_VALUE;

    args.trace("forking to execute");

	in_file = new TemporaryFile(stdin_fname ? *stdin_fname : File(), false);
	out_file = new TemporaryFile(stdout_fname ? *stdout_fname : File(), true);
	err_file = new TemporaryFile(stderr_fname ? *stderr_fname : File(), false);

#if 0 //@
	printf("in=%s out=%s err=%s\n", stdin_file, stdout_file, stderr_file);
	print_args("args", 0, argv);
#endif

	char **argv = args.argv();
	sub_process *proc = process_easy(argv, 0, cwd ? +cwd->path() : 0, *in_file, *out_file, *err_file);
	delete[] argv;
	if (!proc) 
	{
		rs_log_error("failed to execute: %s", map_windows32_error_to_string(GetLastError()));
        return EXIT_OUT_OF_MEMORY; // probably
    }

	proc->in_file = *in_file;
	proc->out_file = *out_file;
	proc->err_file = *err_file;

	pid = proc_t(proc->pid, proc->handle);
	rs_trace("child started as pid=%d", pid.pid);
	return 0;
}

#else // ! _WIN32

int
dcc_spawn_child(const Arguments &args, proc_t &pid, const Directory *cwd,
	const File *stdin_fname, const File *stdout_fname, const File *stderr_fname)
{
    args.trace("forking to execute");
    
    pid_t pid = fork();
    if (pid == -1) 
	{
        rs_log_error("failed to fork: %s", strerror(errno));
        return EXIT_OUT_OF_MEMORY; // probably
    }
	if (pid == 0) 
	{
	    if (cwd)
	    	chdir(cwd);
        dcc_inside_child(args, stdin_fname, stdout_fname, stderr_fname);
        // !! NEVER RETURN FROM HERE !!
    }
	else
	{
        procid = pid;
        rs_trace("child started as pid%d", (int) pid);
        return 0;
    }
}

#endif // ! _WIN32

//---------------------------------------------------------------------------------------------

void 
dcc_reset_signal(int whichsig)
{
#ifdef __linux__
    struct sigaction act_dfl;
    memset(&act_dfl, 0, sizeof act_dfl);
    act_dfl.sa_handler = SIG_DFL;
    sigaction(whichsig, &act_dfl, NULL);
    // might be called from signal handler, therefore no IO to log a message
#endif // __linux__
}

//---------------------------------------------------------------------------------------------

#ifdef __linux__

static pid_t
sys_wait4(pid_t pid, int &status, int options, struct rusage *rusage)
{
#ifdef HAVE_WAIT4
    return wait4(pid, &status, options, rusage);

#elif HAVE_WAITPID
    // Just doing getrusage(children) is not sufficient, because other children may have exited previously
    memset(rusage, 0, sizeof(*rusage));
    return waitpid(pid, &status, options);

#else
#error Please port this
#endif
}

#endif // __linux__

//---------------------------------------------------------------------------------------------

// Blocking wait for a child to exit.  This is used when waiting for cpp, gcc, etc.
//
// This is not used by the daemon-parent; it has its own implementation in dcc_reap_kids().
// They could be unified, but the parent only waits when it thinks a child has exited; 
// the child waits all the time.

int 
dcc_collect_child(const string &what, const proc_t &pid, int &wait_status)
{
#ifdef _WIN32

	DWORD last_err = 0;
	for (;;)
	{
		sub_process *proc = process_wait_for_any(INFINITE);
		if (proc == NULL)
		{
			rs_trace("process_wait_for_any returned NULL");
			goto fail;
		}

		if (proc->handle == pid.handle)
		{
			DWORD exit_code;

			delete in_file;
			delete out_file;
			delete err_file;
			in_file = out_file = err_file = 0;

			//@close_temp_file(proc->in_file);
			//@close_temp_file(proc->out_file);
			//close_temp_file(proc->err_file);

			if (!GetExitCodeProcess(proc->handle, &exit_code))
			{
				last_err = proc->last_err;
				rs_trace("GetExitCodeProcess returned error");
				goto fail;
			}

			wait_status = (int) exit_code;

			rs_trace("%s child %ld terminated", +what, proc->pid);
			process_cleanup(proc);

			return 0;
		}

		rs_trace("process_wait_for_any was interrupted; retrying");
	}

fail:
	if (last_err)
		rs_log_error("dcc_collect_child(pid=%d) failed: %s", (int) pid.pid,  map_windows32_error_to_string(last_err));
	else
		rs_log_error("dcc_collect_child(pid=%d) failed", (int) pid.pid);
	return EXIT_DISTCC_FAILED;

#else // __linux__

    struct rusage ru;
    pid_t ret_pid;
    
    for (;;)
	{
		ret_pid = sys_wait4(pid.pid, wait_status, 0, &ru);
        if (ret_pid != -1) 
		{
            // This is not the main user-visible message, that comes from critique_status()
			rs_trace("%s child %ld terminated with status %#x", what, (long) ret_pid, wait_status);

			rs_log_info("%s times: user %ld.%06lds, system %ld.%06lds, "
				"%ld minflt, %ld majflt",
				+what,
				ru.ru_utime.tv_sec, ru.ru_utime.tv_usec,
				ru.ru_stime.tv_sec, ru.ru_stime.tv_usec,
				ru.ru_minflt, ru.ru_majflt);

            return 0;
        }

		if (errno != EINTR) 
		{
			rs_log_error("sys_wait4(pid=%d) borked: %s", (int) pid.pid, strerror(errno));
			return EXIT_DISTCC_FAILED;
        }

		rs_trace("wait4 was interrupted; retrying");
    }

#endif // __linux__
}

//---------------------------------------------------------------------------------------------

/**
 * Analyze and report to the user on a command's exit code.  
 *
 * @param command short human-readable description of the command (perhaps
 * argv[0])
 *
 * @returns 0 if the command succeeded; 128+SIGNAL if it stopped on a
 * signal; otherwise the command's exit code.
 **/

int 
dcc_critique_status(int status, const string &command, const Path &input_fname,
	const string &hostname, bool verbose)
{
	// verbose mode is only used for executions that the user is likely to particularly need to know about
	int logmode = (verbose ? RS_LOG_ERR : RS_LOG_INFO) | RS_LOG_NONAME;

#ifdef __linux__
	if (WIFSIGNALED(status)) 
	{
#ifdef HAVE_STRSIGNAL
		rs_log(logmode, "%s %s on %s:%s %s",
			+command, +input_fname, +hostname,
			strsignal(WTERMSIG(status)),
			WCOREDUMP(status) ? " (core dumped)" : "");
#else
		rs_log(logmode, "%s %s on %s terminated by signal %d%s",
			+command, +input_fname, +hostname,
			WTERMSIG(status), WCOREDUMP(status) ? " (core dumped)" : "");
#endif
		// Unix convention is to return 128+signal when a subprocess crashes
		return 128 + WTERMSIG(status);
    }
#endif // __linux__

	if (WEXITSTATUS(status) == 1) 
	{
		// Normal failure gives exit code 1, so handle that specially
		rs_log(logmode, "%s %s on %s failed", +command, +input_fname, +hostname);
		return WEXITSTATUS(status);
	}

	if (WEXITSTATUS(status)) 
	{
		// This is a tough call; we don't really want to clutter the client's
		// error stream, but if we don't say where the compilation failed then
		// people may find it hard to work things out.

		rs_log(logmode, "%s %s on %s failed with exit code %d",
			+command, +input_fname, +hostname, WEXITSTATUS(status));
		return WEXITSTATUS(status);
	}

	rs_log(RS_LOG_INFO | RS_LOG_NONAME, "%s %s on %s completed ok", 
		+command, +input_fname, +hostname);
	return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
