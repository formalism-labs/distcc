/* Definitions for Windows process invocation.
Copyright (C) 1996, 1997, 1998, 1999, 2000, 2001, 2002, 2003, 2004, 2005,
2006, 2007 Free Software Foundation, Inc.
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

#ifndef SUB_PROC_H
#define SUB_PROC_H

#define FEATURE_PROC_EASY_OUT_HANDLE

class sub_process
{
public:
	int sv_stdin[2];
	int sv_stdout[2];
	int sv_stderr[2];
	int using_pipes;
	char *inp;
	DWORD incnt;
	char * volatile outp;
	volatile DWORD outcnt;
	char * volatile errp;
	volatile DWORD errcnt;
	DWORD pid;
	HANDLE handle;
	int exit_code;
	int signal;
	long last_err;
	long lerrno;
	HANDLE in_file, out_file, err_file;

	sub_process();
	sub_process(HANDLE stdinh, HANDLE stdouth, HANDLE stderrh);
};

long process_begin(sub_process *proc, char **argv, char **envp, const char *cwd, char *exec_path, char *as_user);
long process_pipe_io(sub_process *proc, char *stdin_data, int stdin_data_len);
long process_file_io(sub_process *proc);
void process_cleanup(sub_process *proc);
sub_process *process_wait_for_any(unsigned int timeout);
void process_register(sub_process *proc);
sub_process *process_easy(char** argv, char** env, const char *cwd,
	HANDLE in_file, HANDLE out_file, HANDLE err_file);

BOOL process_kill(sub_process *proc, int signal);
int process_used_slots();

// support routines
long process_errno(sub_process *proc);
long process_last_err(sub_process *proc);
long process_exit_code(sub_process *proc);
long process_signal(sub_process *proc);
char * process_outbuf(sub_process *proc);
char * process_errbuf(sub_process *proc);
int process_outcnt(sub_process *proc);
int process_errcnt(sub_process *proc);
void process_pipes(sub_process *proc, int pipes[3]);

#endif
