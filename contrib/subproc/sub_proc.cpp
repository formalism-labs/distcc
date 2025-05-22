/* Process handling for Windows.
Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
2006, 2007 2009 Free Software Foundation, Inc.
This file is part of GNU Make.

GNU Make is free software; you can redistribute it and/or modify it under the
terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

GNU Make is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE.  See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program.  If not, see <http://www.gnu.org/licenses/>.  */

#include <stdlib.h>
#include <stdio.h>
#include <process.h> // for msvc _beginthreadex, _endthreadex
#include <signal.h>
#include <winsock2.h>
#include <windows.h>
#include <stdexcept>

#ifdef FEATURE_GROUP_RECIPE_OUTPUT
#include <io.h>
#endif

#include "sub_proc.h"
#include "proc.h"
#include "w32err.h"
#include "config.h"
//#include "debug.h"

///////////////////////////////////////////////////////////////////////////////////////////////

static char *make_command_line(char *shell_name, char *exec_path, char **argv);

// keep track of children so we can implement a waitpid-like routine
static __declspec(thread) sub_process *proc_array[MAXIMUM_WAIT_OBJECTS];
static __declspec(thread) int proc_index = 0;
static __declspec(thread) int fake_exits_pending = 0;

void subproc_error(char const *fmt, ...);

//---------------------------------------------------------------------------------------------

void *
xmalloc (unsigned int size)
{
	// Make sure we don't allocate 0, for pre-ANSI libraries
	void *result = malloc (size ? size : 1);
	if (result == 0)
	{
		subproc_error("virtual memory exhausted");
		exit(2);
	}
	return result;
}
 
//---------------------------------------------------------------------------------------------
// When a process has been waited for, adjust the wait state array so that we don't wait for it again

static void
process_adjust_wait_state(sub_process* proc)
{
	int i;

	if (!proc_index)
		return;

	for (i = 0; i < proc_index; i++)
		if (proc_array[i]->handle == proc->handle)
			break;
#if 1
	if (i >= proc_index)
		return;
	if (--proc_index != i)
		proc_array[i] = proc_array[proc_index];
	proc_array[proc_index] = NULL;
#else
	if (i < proc_index) 
	{
		proc_index--;
		if (i != proc_index)
			memmove(&proc_array[i], &proc_array[i+1], (proc_index-i) * sizeof(sub_process*));
		proc_array[proc_index] = NULL;
	}
#endif
}

//---------------------------------------------------------------------------------------------
// Waits for any of the registered child processes to finish

static sub_process *
process_wait_for_any_private(unsigned int timeout)
{
	HANDLE handles[MAXIMUM_WAIT_OBJECTS];
	DWORD retval, which;
	int i;

	if (!proc_index)
		return NULL;

	// build array of handles to wait for
	for (i = 0; i < proc_index; i++) 
	{
		handles[i] = proc_array[i]->handle;
		if (fake_exits_pending && proc_array[i]->exit_code)
			break;
	}

	// wait for someone to exit
	if (!fake_exits_pending) 
	{
		retval = WaitForMultipleObjects(proc_index, handles, FALSE, (DWORD) timeout);
		which = retval - WAIT_OBJECT_0;
	}
	else 
	{
		fake_exits_pending--;
		retval = !WAIT_FAILED;
		which = i;
	}

	// return pointer to process
	if (retval == WAIT_FAILED || retval == WAIT_TIMEOUT) 
	{
		subproc_error("WaitForMultipleObjets failed: %s", map_windows32_error_to_string(GetLastError()));
		return NULL;
	}

	sub_process *proc = proc_array[which];
	process_adjust_wait_state(proc);
	return proc;
}

//---------------------------------------------------------------------------------------------
// Terminate a process

BOOL
process_kill(sub_process *proc, int signal)
{
	proc->signal = signal;
	return TerminateProcess(proc->handle, signal);
}

//---------------------------------------------------------------------------------------------
// Use this function to register processes you wish to wait for by
// calling process_file_io(NULL) or process_wait_any(). 
// This must be done because it is possible for callers of this library to reuse the same
// handle for multiple processes launches :-(

void
process_register(sub_process *proc)
{
	if (proc_index < MAXIMUM_WAIT_OBJECTS)
		proc_array[proc_index++] = proc;
}

//---------------------------------------------------------------------------------------------

// Return the number of processes that we are still waiting for

int
process_used_slots(void)
{
	return proc_index;
}

//---------------------------------------------------------------------------------------------
/*
 * Public function which works kind of like waitpid(). Wait for any
 * of the children to die and return results. To call this function,
 * you must do 1 of things:
 *
 * 	x = process_easy(...);
 *
 * or
 *
 *	x = process_init_fd();
 *	process_register(x);
 *
 * or
 *
 *	x = process_init();
 *	process_register(x);
 *
 * You must NOT then call process_pipe_io() because this function is
 * not capable of handling automatic notification of any child
 * death.
 */

sub_process *
process_wait_for_any(unsigned int timeout)
{
	sub_process *proc = process_wait_for_any_private(timeout);
	if (!proc)
		return NULL;

	// Ouch! can't tell caller if this fails directly. 
	// Caller will have to use process_last_err().

	process_file_io(proc);
	return proc;
}

//---------------------------------------------------------------------------------------------

long
process_signal(sub_process *proc)
{
	if (!proc)
		return 0;
	return ((sub_process *)proc)->signal;
}

//---------------------------------------------------------------------------------------------

long
process_last_err(sub_process *proc)
{
	if (!proc)
		return ERROR_INVALID_HANDLE;
	return proc->last_err;
}

//---------------------------------------------------------------------------------------------

long
process_exit_code(sub_process *proc)
{
	if (!proc)
		return EXIT_FAILURE;
	return proc->exit_code;
}

//---------------------------------------------------------------------------------------------

/*
2006-02:
All the following functions are currently unused.
All of them would crash gmake if called with argument INVALID_HANDLE_VALUE.
Hence whoever wants to use one of this functions must invent and implement
a reasonable error handling for this function.

char *
process_outbuf(HANDLE proc)
{
	return ((sub_process *)proc)->outp;
}

char *
process_errbuf(HANDLE proc)
{
	return ((sub_process *)proc)->errp;
}

int
process_outcnt(HANDLE proc)
{
	return ((sub_process *)proc)->outcnt;
}

int
process_errcnt(HANDLE proc)
{
	return ((sub_process *)proc)->errcnt;
}

void
process_pipes(HANDLE proc, int pipes[3])
{
	pipes[0] = ((sub_process *)proc)->sv_stdin[0];
	pipes[1] = ((sub_process *)proc)->sv_stdout[0];
	pipes[2] = ((sub_process *)proc)->sv_stderr[0];
}
*/

//---------------------------------------------------------------------------------------------

sub_process::sub_process()
{
	// Open file descriptors for attaching stdin/stdout/sterr
	HANDLE stdin_pipes[2];
	HANDLE stdout_pipes[2];
	HANDLE stderr_pipes[2];

	SECURITY_ATTRIBUTES inherit;
	BYTE sd[SECURITY_DESCRIPTOR_MIN_LENGTH];

	memset(this, 0, sizeof(*this));

	// We can't use NULL for lpSecurityDescriptor because that uses the default security descriptor of the calling process.
	// Instead we use a security descriptor with no DACL.  
	// This allows nonrestricted access to the associated objects.

	if (!InitializeSecurityDescriptor((PSECURITY_DESCRIPTOR) &sd, SECURITY_DESCRIPTOR_REVISION)) 
	{
		last_err = GetLastError();
		lerrno = E_SCALL;
		throw std::runtime_error("sub_process: error creating security descriptor");
	}

	inherit.nLength = sizeof(inherit);
	inherit.lpSecurityDescriptor = (PSECURITY_DESCRIPTOR)(&sd);
	inherit.bInheritHandle = TRUE;

	// By convention, parent gets pipe[0], and child gets pipe[1]
	// This means the READ side of stdin pipe goes into pipe[1]
	// and the WRITE side of the stdout and stderr pipes go into pipe[1]
	if (CreatePipe(&stdin_pipes[1], &stdin_pipes[0], &inherit, 0) == FALSE ||
		CreatePipe(&stdout_pipes[0], &stdout_pipes[1], &inherit, 0) == FALSE ||
		CreatePipe(&stderr_pipes[0], &stderr_pipes[1], &inherit, 0) == FALSE) 
	{
		last_err = GetLastError();
		lerrno = E_SCALL;
		throw std::runtime_error("sub_process: error creating pipes");
	}

	//
	// Mark the parent sides of the pipes as non-inheritable
	//
	if (SetHandleInformation(stdin_pipes[0], HANDLE_FLAG_INHERIT, 0) == FALSE ||
		SetHandleInformation(stdout_pipes[0], HANDLE_FLAG_INHERIT, 0) == FALSE ||
		SetHandleInformation(stderr_pipes[0], HANDLE_FLAG_INHERIT, 0) == FALSE) 
	{

		last_err = GetLastError();
		lerrno = E_SCALL;
		throw std::runtime_error("sub_process: error configuring pipes");
	}

	sv_stdin[0]  = (int) stdin_pipes[0];
	sv_stdin[1]  = (int) stdin_pipes[1];
	sv_stdout[0] = (int) stdout_pipes[0];
	sv_stdout[1] = (int) stdout_pipes[1];
	sv_stderr[0] = (int) stderr_pipes[0];
	sv_stderr[1] = (int) stderr_pipes[1];

	using_pipes = 1;
	lerrno = 0;
}

//---------------------------------------------------------------------------------------------

sub_process::sub_process(HANDLE stdinh, HANDLE stdouth, HANDLE stderrh)
{
	memset(this, 0, sizeof(*this));

	// Just pass the provided file handles to the 'child side' of the pipe, bypassing pipes altogether.
	sv_stdin[1]  = (int) stdinh;
	sv_stdout[1] = (int) stdouth;
	sv_stderr[1] = (int) stderrh;

	last_err = lerrno = 0;
}


//---------------------------------------------------------------------------------------------

static HANDLE
find_file(const char *exec_path, const char *path_var, char *full_fname, DWORD full_len)
{
	HANDLE exec_handle;
	char *fname;
	char *ext;
	DWORD req_len;
	int i;
	static const char *extensions[] =
	  // Should .com come before no-extension case?
	  { ".exe", ".cmd", ".bat", "", ".com", NULL };

	fname = (char *) xmalloc(strlen(exec_path) + 5);
	strcpy(fname, exec_path);
	ext = fname + strlen(fname);

	for (i = 0; extensions[i]; i++)
	{
		strcpy(ext, extensions[i]);
		if (((req_len = SearchPath (path_var, fname, NULL, full_len, full_fname, NULL)) > 0
			// For compatibility with previous code, which used OpenFile, and with Windows 
			// operation in general, also look in various default locations, such as Windows 
			// directory and Windows System directory.
			// Warning: this also searches PATH in the Make's environment, which might not be what 
			// the Makefile wants, but it seems to be OK as a fallback, after the previous SearchPath 
			// failed to find on child's PATH.
			|| (req_len = SearchPath (NULL, fname, NULL, full_len, full_fname, NULL)) > 0)
				&& req_len <= full_len
				&& (exec_handle = CreateFile(full_fname,
						GENERIC_READ,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_EXISTING,
						FILE_ATTRIBUTE_NORMAL,
						NULL)) != INVALID_HANDLE_VALUE) 
		{
			free(fname);
			return(exec_handle);
		}
	}

	free(fname);
	return INVALID_HANDLE_VALUE;
}

//---------------------------------------------------------------------------------------------

// Description:   Create the child process to be helped
// Returns: success <=> 0

long
process_begin(sub_process *proc, char **argv, char **envp, const char *cwd, char *exec_path, 
	char *as_user)
{
	char *shell_name = 0;
	int file_not_found=0;
	HANDLE exec_handle;
	char exec_fname[MAX_PATH];
	const char *path_var = NULL;
	char **ep;
	char buf[256];
	DWORD bytes_returned;
	DWORD flags;
	char *command_line;
	STARTUPINFO startInfo;
	PROCESS_INFORMATION procInfo;
	char *envblk = NULL;
	char path_str[6];
	int j;

	// Shell script detection...  if the exec_path starts with #! then
	// we want to exec shell-script-name exec-path, not just exec-path
	// NT doesn't recognize #!/bin/sh or #!/etc/Tivoli/bin/perl.  
	// We do not hard-code the path to the shell or perl or whatever:  
	// Instead, we assume it's in the path somewhere (generally, the NT tools bin directory)

	// Use the Makefile's value of PATH to look for the program to execute, 
	// because it could be different from Make's PATH (e.g., if the target sets its own value.
	for (ep = envp; ep && *ep; ep++) 
	{
		strncpy(path_str, *ep, 5);
		path_str[5] = '\0';
		for (j = 0; j < 4; ++j)
			path_str[j] = toupper(path_str[j]);
		if (strncmp (path_str, "PATH=", 5) == 0) 
		{
			path_var = *ep + 5;
			break;
		}
	}
	exec_handle = find_file(exec_path, path_var, exec_fname, sizeof(exec_fname));

	// If we couldn't open the file, just assume that Windows will be somehow able to find and execute it
	if (exec_handle == INVALID_HANDLE_VALUE) 
	{
		file_not_found++;
	}
	else 
	{
		// Attempt to read the first line of the file
		if (ReadFile(exec_handle,
				buf, sizeof(buf) - 1, /* leave room for trailing NULL */
				&bytes_returned, 0) == FALSE || bytes_returned < 2) 
		{
			proc->last_err = GetLastError();
			proc->lerrno = E_IO;
			CloseHandle(exec_handle);
			return(-1);
		}

		if (buf[0] == '#' && buf[1] == '!') 
		{
			// This is a shell script...  Change the command line from
			// exec_path args to shell_name exec_path args
			char *p;

			//  Make sure buf is NULL terminated
			buf[bytes_returned] = 0;

			// Depending on the file system type, etc. the first line
			// of the shell script may end with newline or newline-carriage-return
			// Whatever it ends with, cut it off.
			p = strchr(buf, '\n');
			if (p)
				*p = 0;
			p = strchr(buf, '\r');
			if (p)
				*p = 0;

			// Find base name of shell
			shell_name = strrchr( buf, '/');
			if (shell_name) 
				shell_name++;
			else 
				shell_name = &buf[2]; // skipping "#!"
		}

		CloseHandle(exec_handle);
	}

	flags = 0;

	command_line = make_command_line(shell_name, file_not_found ? exec_path : exec_fname, argv);
	if (command_line == NULL)
	{
		proc->last_err = 0;
		proc->lerrno = E_NO_MEM;
		return -1;
	}

	if (envp && arr2envblk(envp, &envblk) == FALSE)
	{
		proc->last_err = 0;
		proc->lerrno = E_NO_MEM;
		free( command_line );
		return -1;
	}

	if (shell_name || file_not_found) 
		exec_path = 0;	// Search for the program in %Path%
	else
		exec_path = exec_fname;

	// Set up inherited stdin, stdout, stderr for child
	GetStartupInfo(&startInfo);
	startInfo.dwFlags = STARTF_USESTDHANDLES;
	startInfo.lpReserved = 0;
	startInfo.cbReserved2 = 0;
	startInfo.lpReserved2 = 0;
	startInfo.lpTitle = shell_name ? shell_name : exec_path;
	startInfo.hStdInput = (HANDLE) proc->sv_stdin[1];
	startInfo.hStdOutput = (HANDLE) proc->sv_stdout[1];
	startInfo.hStdError = (HANDLE) proc->sv_stderr[1];

	if (as_user) 
	{
		if (envblk)
			free(envblk);
		return -1;
	} 

//	DB (DB_JOBS, ("CreateProcess(%s,%s,...)\n",
//		exec_path ? exec_path : "NULL", command_line ? command_line : "NULL"));
	if (CreateProcess(
		exec_path,
		command_line,
		NULL,
		0, // default security attributes for thread
		TRUE, // inherit handles (e.g. helper pipes, oserv socket)
		flags,
		envblk,
		cwd, // default starting directory
		&startInfo,
		&procInfo) == FALSE) 
	{
		proc->last_err = GetLastError();
		proc->lerrno = E_FORK;
		subproc_error("process_begin: CreateProcess(%s, %s, ...) failed",
			exec_path ? exec_path : "NULL", command_line);
		if (envblk)
			free(envblk);
		free(command_line);
		return -1;
	}

	proc->handle = procInfo.hProcess;
	proc->pid = procInfo.dwProcessId;
	if (proc->pid == 0)
		subproc_error("process_begin: pid == 0"); //@

	// Close the thread handle -- we'll just watch the process
	CloseHandle(procInfo.hThread);

	// Close the halves of the pipes we don't need
    CloseHandle((HANDLE)proc->sv_stdin[1]);
    CloseHandle((HANDLE)proc->sv_stdout[1]);
    CloseHandle((HANDLE)proc->sv_stderr[1]);
    proc->sv_stdin[1] = 0;
    proc->sv_stdout[1] = 0;
    proc->sv_stderr[1] = 0;

	free(command_line);
	if (envblk)
		free(envblk);
	proc->lerrno = 0;
	return 0;
}

//---------------------------------------------------------------------------------------------

static DWORD
proc_stdin_thread(sub_process *proc)
{
	DWORD in_done;
	for (;;) 
	{
		if (WriteFile( (HANDLE) proc->sv_stdin[0], proc->inp, proc->incnt, &in_done, NULL) == FALSE)
			_endthreadex(0);

		// This if should never be true for anonymous pipes, but gives
		// us a chance to change I/O mechanisms later
		if (in_done < proc->incnt) 
		{
			proc->incnt -= in_done;
			proc->inp += in_done;
		}
		else 
		{
			_endthreadex(0);
		}
	}
	return 0; // for compiler warnings only.. not reached
}

//---------------------------------------------------------------------------------------------

static DWORD
proc_stdout_thread(sub_process *proc)
{
	DWORD bufsize = 1024;
	char c;
	DWORD nread;
	proc->outp = (char *) malloc(bufsize);
	if (proc->outp == NULL)
		_endthreadex(0);
	proc->outcnt = 0;

	for (;;) 
	{
		if (ReadFile( (HANDLE)proc->sv_stdout[0], &c, 1, &nread, NULL) == FALSE) 
		{
			// map_windows32_error_to_string(GetLastError());
			_endthreadex(0);
		}
		if (nread == 0)
			_endthreadex(0);
		if (proc->outcnt + nread > bufsize) 
		{
			bufsize += nread + 512;
			proc->outp = (char *) realloc(proc->outp, bufsize);
			if (proc->outp == NULL) 
			{
				proc->outcnt = 0;
				_endthreadex(0);
			}
		}
		proc->outp[proc->outcnt++] = c;
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

static DWORD
proc_stderr_thread(sub_process *proc)
{
	DWORD bufsize = 1024;
	char c;
	DWORD nread;
	proc->errp = (char *) malloc(bufsize);
	if (proc->errp == NULL)
		_endthreadex(0);
	proc->errcnt = 0;

	for (;;) 
	{
		if (ReadFile( (HANDLE)proc->sv_stderr[0], &c, 1, &nread, NULL) == FALSE) 
		{
			map_windows32_error_to_string(GetLastError());
			_endthreadex(0);
		}
		if (nread == 0)
			_endthreadex(0);
		if (proc->errcnt + nread > bufsize) 
		{
			bufsize += nread + 512;
			proc->errp = (char *) realloc(proc->errp, bufsize);
			if (proc->errp == NULL) 
			{
				proc->errcnt = 0;
				_endthreadex(0);
			}
		}
		proc->errp[proc->errcnt++] = c;
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

// Collects output from child process and returns results

long
process_pipe_io(sub_process *proc, char *stdin_data, int stdin_data_len)
{
	bool_t stdin_eof = FALSE, stdout_eof = FALSE, stderr_eof = FALSE;
	HANDLE childhand = proc->handle;
	HANDLE tStdin = NULL, tStdout = NULL, tStderr = NULL;
	unsigned int dwStdin, dwStdout, dwStderr;
	HANDLE wait_list[4];
	DWORD wait_count;
	DWORD wait_return;
	HANDLE ready_hand;
	bool_t child_dead = FALSE;
	BOOL GetExitCodeResult;

	// Create stdin thread, if needed

	proc->inp = stdin_data;
	proc->incnt = stdin_data_len;
	if (!proc->inp) 
	{
		stdin_eof = TRUE;
		CloseHandle((HANDLE)proc->sv_stdin[0]);
		proc->sv_stdin[0] = 0;
	}
	else 
	{
		tStdin = (HANDLE) _beginthreadex(0, 1024, (unsigned (__stdcall *) (void *))proc_stdin_thread, 
			proc, 0, &dwStdin);
		if (tStdin == 0) 
		{
			proc->last_err = GetLastError();
			proc->lerrno = E_SCALL;
			goto done;
		}
	}

	// Assume child will produce stdout and stderr

	tStdout = (HANDLE) _beginthreadex( 0, 1024,
		(unsigned (__stdcall *) (void *))proc_stdout_thread, proc, 0, &dwStdout);
	tStderr = (HANDLE) _beginthreadex( 0, 1024, 
		(unsigned (__stdcall *) (void *))proc_stderr_thread, proc, 0, &dwStderr);

	if (tStdout == 0 || tStderr == 0) 
	{
		proc->last_err = GetLastError();
		proc->lerrno = E_SCALL;
		goto done;
	}

	// Wait for all I/O to finish and for the child process to exit

	while (!stdin_eof || !stdout_eof || !stderr_eof || !child_dead) 
	{
		wait_count = 0;

		if (!stdin_eof)
			wait_list[wait_count++] = tStdin;
		if (!stdout_eof)
			wait_list[wait_count++] = tStdout;
		if (!stderr_eof)
			wait_list[wait_count++] = tStderr;
		if (!child_dead)
			wait_list[wait_count++] = childhand;

		wait_return = WaitForMultipleObjects(wait_count, wait_list,
			 FALSE, /* don't wait for all: one ready will do */
			 child_dead ? 1000 : INFINITE); 
		// after the child dies, subthreads have one second to collect all remaining output

		if (wait_return == WAIT_FAILED || wait_return != WAIT_TIMEOUT) 
		{
			// map_windows32_error_to_string(GetLastError());
			proc->last_err = GetLastError();
			proc->lerrno = E_SCALL;
			goto done;
		}

		ready_hand = wait_list[wait_return - WAIT_OBJECT_0];

		if (ready_hand == tStdin) 
		{
			CloseHandle((HANDLE)proc->sv_stdin[0]);
			proc->sv_stdin[0] = 0;
			CloseHandle(tStdin);
			tStdin = 0;
			stdin_eof = TRUE;
		} 
		else if (ready_hand == tStdout) 
		{
		  	CloseHandle((HANDLE)proc->sv_stdout[0]);
			proc->sv_stdout[0] = 0;
			CloseHandle(tStdout);
			tStdout = 0;
		  	stdout_eof = TRUE;
		}
		else if (ready_hand == tStderr) 
		{
			CloseHandle((HANDLE)proc->sv_stderr[0]);
			proc->sv_stderr[0] = 0;
			CloseHandle(tStderr);
			tStderr = 0;
			stderr_eof = TRUE;

		}
		else if (ready_hand == childhand) 
		{
			DWORD ierr;
			GetExitCodeResult = GetExitCodeProcess(childhand, &ierr);
			if (ierr == CONTROL_C_EXIT)
				proc->signal = SIGINT;
			else
				proc->exit_code = ierr;

			if (GetExitCodeResult == FALSE) 
			{
				proc->last_err = GetLastError();
				proc->lerrno = E_SCALL;
				goto done;
			}
			child_dead = TRUE;
		}
		else 
		{
			// ?? Got back a handle we didn't query ??
			proc->last_err = 0;
			proc->lerrno = E_FAIL;
			goto done;
		}
	}

done:
	if (tStdin != 0)
		CloseHandle(tStdin);
	if (tStdout != 0)
		CloseHandle(tStdout);
	if (tStderr != 0)
		CloseHandle(tStderr);

	return proc->lerrno ? -1 : 0;
}

//---------------------------------------------------------------------------------------------

// collects output from child process and returns results

long
process_file_io(sub_process *proc)
{
	HANDLE childhand;
	DWORD wait_return;
	BOOL GetExitCodeResult;
	DWORD ierr;

	if (proc == NULL)
		proc = process_wait_for_any_private(INFINITE);

	// some sort of internal error
	if (!proc)
		return -1;

	childhand = proc->handle;

	// This function is poorly named, and could also be used just to wait
	// for child death if you're doing your own pipe I/O.  
	// If that is the case, close the pipe handles here.
	if (proc->sv_stdin[0]) 
	{
		CloseHandle((HANDLE)proc->sv_stdin[0]);
		proc->sv_stdin[0] = 0;
	}
	if (proc->sv_stdout[0]) 
	{
		CloseHandle((HANDLE)proc->sv_stdout[0]);
		proc->sv_stdout[0] = 0;
	}
	if (proc->sv_stderr[0]) 
	{
		CloseHandle((HANDLE)proc->sv_stderr[0]);
		proc->sv_stderr[0] = 0;
	}

	// Wait for the child process to exit

	wait_return = WaitForSingleObject(childhand, INFINITE);

	if (wait_return != WAIT_OBJECT_0) 
	{
		// map_windows32_error_to_string(GetLastError());
		proc->last_err = GetLastError();
		proc->lerrno = E_SCALL;
		goto done2;
	}

	GetExitCodeResult = GetExitCodeProcess(childhand, &ierr);
	if (ierr == CONTROL_C_EXIT)
		proc->signal = SIGINT;
	else
		proc->exit_code = ierr;

	if (GetExitCodeResult == FALSE) 
	{
		proc->last_err = GetLastError();
		proc->lerrno = E_SCALL;
	}

done2:
	return proc->lerrno ? -1 : 0;
}

//---------------------------------------------------------------------------------------------

// Clean up any leftover handles, etc.  
// It is up to the caller to manage and free the input, ouput, and stderr buffers.

void
process_cleanup(sub_process *proc)
{
	if (proc->using_pipes) 
	{
		for (int i = 0; i <= 1; i++) 
		{
			if ((HANDLE)proc->sv_stdin[i])
				CloseHandle((HANDLE)proc->sv_stdin[i]);
			if ((HANDLE)proc->sv_stdout[i])
				CloseHandle((HANDLE)proc->sv_stdout[i]);
			if ((HANDLE)proc->sv_stderr[i])
				CloseHandle((HANDLE)proc->sv_stderr[i]);
		}
	}
	if (proc->handle)
		CloseHandle(proc->handle);

	free(proc);
}

//---------------------------------------------------------------------------------------------

/*
 * Description:
 *	 Create a command line buffer to pass to CreateProcess
 *
 * Returns:  the buffer or NULL for failure
 *	Shell case:  sh_name a:/full/path/to/script argv[1] argv[2] ...
 *  Otherwise:   argv[0] argv[1] argv[2] ...
 *
 * Notes/Dependencies:
 *   CreateProcess does not take an argv, so this command creates a command line for the executable.
 */

static char *
make_command_line( char *shell_name, char *full_exec_path, char **argv)
{
	int		argc = 0;
	char**		argvi;
	int*		enclose_in_quotes = NULL;
	int*		enclose_in_quotes_i;
	unsigned int	bytes_required = 0;
	char*		command_line;
	char*		command_line_i;
	int  cygwin_mode = 0; // HAVE_CYGWIN_SHELL
	int have_sh = 0; // HAVE_CYGWIN_SHELL

#ifdef HAVE_CYGWIN_SHELL
	have_sh = (shell_name != NULL || strstr(full_exec_path, "sh.exe"));
	cygwin_mode = 1;
#endif

	if (shell_name && full_exec_path) 
	{
		bytes_required = strlen(shell_name) + 1 + strlen(full_exec_path);

		// Skip argv[0] if any, when shell_name is given.
		if (*argv) argv++;
			// Add one for the intervening space
			if (*argv) bytes_required++;
	}

	argvi = argv;
	while (*(argvi++)) 
		argc++;

	if (argc) 
	{
		enclose_in_quotes = (int*) calloc(1, argc * sizeof(int));
		if (!enclose_in_quotes) 
			return NULL;
	}

	// We have to make one pass through each argv[i] to see if we need to enclose it in ",
	// so we might as well figure out how much memory we'll need on the same pass.

	argvi = argv;
	enclose_in_quotes_i = enclose_in_quotes;
	while(*argvi) 
	{
		char* p = *argvi;
		unsigned int backslash_count = 0;

		// We have to enclose empty arguments in ".
		if (!(*p)) 
			*enclose_in_quotes_i = 1;

		while (*p) 
		{
			switch (*p) 
			{
			case '\"':
				// We have to insert a backslash for each " and each \ that precedes the "
				bytes_required += (backslash_count + 1);
				backslash_count = 0;
				break;

#if !defined(HAVE_MKS_SHELL) && !defined(HAVE_CYGWIN_SHELL)
			case '\\':
				backslash_count++;
				break;
#endif
	/*
	 * At one time we set *enclose_in_quotes_i for '*' or '?' to suppress
	 * wildcard expansion in programs linked with MSVC's SETARGV.OBJ so
	 * that argv in always equals argv out. This was removed.  Say you have
	 * such a program named glob.exe.  You enter
	 * glob '*'
	 * at the sh command prompt.  Obviously the intent is to make glob do the
	 * wildcarding instead of sh.  If we set *enclose_in_quotes_i for '*' or '?',
	 * then the command line that glob would see would be
	 * glob "*"
	 * and the _setargv in SETARGV.OBJ would _not_ expand the *.
	 */
			case ' ':
			case '\t':
				*enclose_in_quotes_i = 1;
				// fall through

			default:
				backslash_count = 0;
				break;
			}

			// Add one for each character in argv[i].
			bytes_required++;

			p++;
		}

		if (*enclose_in_quotes_i) 
		{
			// Add one for each enclosing ", and one for each \ that precedes the closing "
			bytes_required += (backslash_count + 2);
		}

		// Add one for the intervening space
		if (*(++argvi))
			bytes_required++;
		enclose_in_quotes_i++;
	}

	// Add one for the terminating NULL
	bytes_required++;

	command_line = (char*) malloc(bytes_required);

	if (!command_line) 
	{
		if (enclose_in_quotes) 
			free(enclose_in_quotes);
		return NULL;
	}

	command_line_i = command_line;

	if (shell_name && full_exec_path) 
	{
		while(*shell_name) 
			*(command_line_i++) = *(shell_name++);

		*(command_line_i++) = ' ';

		while(*full_exec_path) 
			*(command_line_i++) = *(full_exec_path++);

		if (*argv)
			*(command_line_i++) = ' ';
	}

	argvi = argv;
	enclose_in_quotes_i = enclose_in_quotes;

	while(*argvi) 
	{
		char* p = *argvi;
		unsigned int backslash_count = 0;

		if (*enclose_in_quotes_i) 
			*(command_line_i++) = '\"';

		while(*p) 
		{
			if (*p == '\"') 
			{
				if (cygwin_mode && have_sh) 
				{
					// HAVE_CYGWIN_SHELL
					// instead of a \", cygwin likes ""
					*(command_line_i++) = '\"';
				}
				else 
				{
					// We have to insert a backslash for the " and each \ that precedes the "
					backslash_count++;

					while(backslash_count) 
					{
						*(command_line_i++) = '\\';
						backslash_count--;
					}
				}
			} 
#if !defined(HAVE_MKS_SHELL) && !defined(HAVE_CYGWIN_SHELL)
			else if (*p == '\\') 
				backslash_count++;
			else
				backslash_count = 0;
#endif
			// Copy the character
			*(command_line_i++) = *(p++);
		}

		if (*enclose_in_quotes_i) 
		{
#if !defined(HAVE_MKS_SHELL) && !defined(HAVE_CYGWIN_SHELL)
			// Add one \ for each \ that precedes the closing "
			while(backslash_count--) 
				*(command_line_i++) = '\\';
#endif
			*(command_line_i++) = '\"';
		}

		// Append an intervening space
		if (*(++argvi))
			*(command_line_i++) = ' ';

		enclose_in_quotes_i++;
	}

	// Append the terminating NULL
	*command_line_i = '\0';

	if (enclose_in_quotes)
		free(enclose_in_quotes);
	return command_line;
}

//---------------------------------------------------------------------------------------------

static int
duplicate_handle(HANDLE h, DWORD std_handle, HANDLE *handle, const char *name)
{
//	HANDLE h = file ? _get_osfhandle(_fileno(file)) : INVALID_HANDLE_VALUE;
	if (DuplicateHandle(GetCurrentProcess(), h != INVALID_HANDLE_VALUE ? h : GetStdHandle(std_handle),
		GetCurrentProcess(), handle,
		0, TRUE, DUPLICATE_SAME_ACCESS) == FALSE)
	{
		DWORD err = GetLastError();
		subproc_error("process_easy: DuplicateHandle(%s) failed (e=%ld)", name, GetLastError());
		return 0;
	}
	return 1;
}

//---------------------------------------------------------------------------------------------

// Given an argv and optional envp, launch the process using the default stdin, stdout, and stderr handles.
// Also, register process so that process_wait_for_any_private() can be used via 
// process_file_io(NULL) or process_wait_for_any().

sub_process *
process_easy(char **argv, char **envp, const char *cwd, 
	HANDLE in_file, HANDLE out_file, HANDLE err_file)
{
	if (proc_index >= MAXIMUM_WAIT_OBJECTS) 
	{
//		DB (DB_JOBS, ("process_easy: All process slots used up\n"));
		return 0;
	}

	HANDLE hIn;
	if (!duplicate_handle(in_file, STD_INPUT_HANDLE, &hIn, "In"))
		return 0;
	HANDLE hOut;
	if (!duplicate_handle(out_file, STD_OUTPUT_HANDLE, &hOut, "Out"))
		return 0;
	HANDLE hErr;
	if (!duplicate_handle(err_file, STD_ERROR_HANDLE, &hErr, "Err"))
		return 0;

	sub_process *proc = new sub_process(hIn, hOut, hErr);

	if (process_begin(proc, argv, envp, cwd, argv[0], NULL)) 
	{
//@		fake_exits_pending++;
		// process_begin() failed: make a note of that
		if (!proc->last_err)
			proc->last_err = -1;
		proc->exit_code = process_last_err(proc);

		// close up unused handles
		CloseHandle(hIn);
		CloseHandle(hOut);
		CloseHandle(hErr);

		delete proc;
		return 0;
	}

	process_register(proc);

	return proc;
}

///////////////////////////////////////////////////////////////////////////////////////////////
 