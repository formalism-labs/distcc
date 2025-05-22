/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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

				/* He who waits until circumstances completely favour *
				 * his undertaking will never accomplish anything.    *
				 *              -- Martin Luther                      */
   

/**
 * @file
 *
 * Actually serve remote requests.  Called from daemon.c.
 *
 * @todo Make sure wait statuses are packed in a consistent format
 * (exit<<8 | signal).  Is there any platform that doesn't do this?
 *
 * @todo The server should catch signals, and terminate the compiler process
 * group before handling them.
 *
 * @todo It might be nice to detect that the client has dropped the
 * connection, and then kill the compiler immediately.  However, we probably
 * won't notice that until we try to do IO.  SIGPIPE won't help because it's
 * not triggered until we try to do IO.  I don't think it matters a lot,
 * though, because the client's not very likely to do that.  The main case is
 * probably somebody getting bored and interrupting compilation.
 *
 * What might help is to select() on the network socket while we're waiting
 * for the child to complete, allowing SIGCHLD to interrupt the select() when
 * the child completes.  However I'm not sure if it's really worth the trouble
 * of doing that just to handle a fairly marginal case.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <dirent.h>

#ifdef HAVE_SYS_SIGNAL_H
#  include <sys/signal.h>
#endif /* HAVE_SYS_SIGNAL_H */
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/wait.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#ifdef _WIN32
#include <io.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/rpc1.h"
#include "common/exitcode.h"
#include "common/snprintf.h"
#include "common/bulk.h"
#include "common/exec.h"
#include "common/hosts.h"
#include "common/compiler.h"

#include "server/dopt.h"
#include "server/srvnet.h"
#include "server/daemon.h"

#include "rvfc/text/defs.h"

namespace distcc
{

using std::string;

using namespace rvfc::Text;

///////////////////////////////////////////////////////////////////////////////////////////////

// We copy all serious distccd messages to this file, as well as sending the
// compiler errors there, so they're visible to the client.

static fd_t dcc_compile_log_fd = { -1, 0 };

//---------------------------------------------------------------------------------------------
// Copy all server messages to the error file, so that they can be echoed back to the client if necessary

static int 
dcc_add_log_to_file(const File &err_file)
{
	const char *err_fname = +err_file.path();
	if (dcc_compile_log_fd.fd != -1) 
	{
		rs_log_crit("compile log already open?");
		return 0; // continue?
	}

	dcc_compile_log_fd.fd = open(err_fname, O_WRONLY|O_CREAT|O_TRUNC, 0600);
	if (dcc_compile_log_fd.fd == -1) 
	{
		rs_log_error("failed to open %s: %s", err_fname, strerror(errno));
		return EXIT_IO_ERROR;
	}

	// Only send fairly serious errors back
	rs_add_logger(rs_logger_file, RS_LOG_WARNING, NULL, dcc_compile_log_fd.fd);

	return 0;
}

//---------------------------------------------------------------------------------------------

static int 
dcc_remove_log_to_file()
{
	if (dcc_compile_log_fd.fd == -1) 
	{
		rs_log_warning("compile log not open?");
		return 0; // continue?
	}

	// must exactly match call in dcc_add_log_to_file
	rs_remove_logger(rs_logger_file, RS_LOG_WARNING, NULL, dcc_compile_log_fd.fd);

	dcc_close(dcc_compile_log_fd);

	dcc_compile_log_fd.fd = -1;

	return 0;
}

//---------------------------------------------------------------------------------------------

class CompilationJob
{
public:
	CompilationJob();
	~CompilationJob();

	int run(fd_t in_fd, fd_t out_fd);

	int result() { return ret; }

	int error, ret;
	File err_fname, out_fname;
	text view_name, session_name;
	Directory compile_dir;

	File pdb_fname, dotd_fname, orig_input, orig_output;
};

//---------------------------------------------------------------------------------------------

// Read and execute a job to/from socket.
// This is the common entry point no matter what mode the daemon is running in: 
// preforked, nonforked, or ssh/inetd.

int 
dcc_service_job(fd_t in_fd, fd_t out_fd, struct sockaddr *cli_addr, int cli_len)
{
	// Log client name and check access if appropriate.  
	// For ssh connections the client comes from a unix-domain socket and that's always allowed.
	if (dcc_check_client(cli_addr, cli_len, opt_allowed))
		return EXIT_ACCESS_DENIED;

	CompilationJob job;
	try
	{
		job.run(in_fd, out_fd);
	}
	catch (const char *x)
	{
		rs_trace("compilation job failed: %s", x);
	}

	return job.result();
}

//---------------------------------------------------------------------------------------------

static File dcc_input_tmpnam(const Compiler &compiler, const string &orig_input)
{
	rs_trace("input file %s", +orig_input);
	string ext = dcc_find_extension(orig_input);
	ext = ext.empty() ? ".tmp" : compiler.preproc_exten(ext);
	return dcc_make_tmpnam("distccd", +ext);
}

//---------------------------------------------------------------------------------------------

// Find the absolute path for the first occurrence of @p compiler_name on the PATH.
// Print a warning if it looks like a symlink to distcc.
//
// We want to guard against somebody accidentally running the server with a masqueraded compiler on its $PATH.  
// The worst that's likely to happen here is wasting some time running a distcc or 
// ccache client that does nothing, so it's not a big deal.  
// (This could be easy to do if it's on the default PATH and they start the daemon from the command line.)
//
// At the moment we don't look for the compiler too.

static int 
dcc_check_compiler_masq(const text &compiler_name)
{
	if (compiler_name[0] == '/') 
		return 0;
	
	const char *envpath = getenv("PATH");
	if (!envpath) 
	{
		rs_trace("PATH seems not to be defined");
		return 0;
	}

	string buf;
	const char *n, *p;
	for (n = p = envpath; *n; p = n) 
	{
		n = strchr(p, ':');
		int len;
		if (n)
			len = n++ - p;
		else 
		{
			len = strlen(p);
			n = p + len;
		}

		buf = stringf("%.*s/%s", len, p, +compiler_name);

#ifndef _WIN32
		struct stat sb;
		char linkbuf[MAXPATHLEN];

		if (lstat(buf, &sb) == -1)
			continue; // ENOENT, EACCESS, etc
		if (!S_ISLNK(sb.st_mode)) 
		{
			rs_trace("%s is not a symlink", buf);
			break; // found it
		}

		if ((len = readlink(buf, linkbuf, sizeof linkbuf)) <= 0)
			continue;
		linkbuf[len] = '\0';
		
		if (strstr(linkbuf, "distcc")) 
		{
			rs_log_warning("%s on distccd's path is %s and really a link to %s",
				compiler_name, buf, linkbuf);
			break; // but use it anyhow
		}
		else 
		{
			rs_trace("%s is a safe symlink to %s", buf, linkbuf);
			break; // found it
		}
#endif // _WIN32
	}

	return 0;
}

//---------------------------------------------------------------------------------------------

#if 0

static bool is_dir_exists(const char *dir)
{
#ifdef _WIN32
	DWORD attr = GetFileAttributes(dir);
	return attr != INVALID_FILE_ATTRIBUTES && !!(attr & FILE_ATTRIBUTE_DIRECTORY);
#else
	DIR *d = opendir(dir);
	if (d)
	{
		closedir(d);
		return true;
	}
	return false;
#endif
}

#endif // 0

//---------------------------------------------------------------------------------------------

CompilationJob::CompilationJob()
{
	error = true;
	log_context = ++serial_log_context;
}

//---------------------------------------------------------------------------------------------
// Read a request, run the compiler, and send a response

int CompilationJob::run(fd_t in_fd, fd_t out_fd)
{
	File err_fname = dcc_make_tmpnam("distccd", ".stderr");
	File out_fname = dcc_make_tmpnam("distccd", ".stdout");
	
	err_fname.remove();
	out_fname.remove();

	// Capture any messages relating to this compilation to the same file as
	// compiler errors so that they can all be sent back to the client.
	dcc_add_log_to_file(err_fname);

	// Ignore SIGPIPE; we consistently check error codes and will see the EPIPE.
	// Note that it is set back to the default behavior when spawning a child, 
	// to handle cases like the assembler dying while its being fed from the compiler.
	dcc_ignore_sigpipe(1);

	// Allow output to accumulate into big packets
	tcp_cork_sock(out_fd, 1);

	enum dcc_protover protover;
	text session_name;
	unsigned cmd_flags;
	if ((ret = dcc_r_request_header(in_fd, protover))
		|| (ret = dcc_r_session_name(in_fd, session_name))
		|| (ret = dcc_r_flags(in_fd, cmd_flags)))
	{
		throw "CompilationJob: error";
	}
	
	Arguments args = dcc_r_argv(in_fd);

	bool on_server = !!(cmd_flags & CMD_FLAGS_ON_SERVER);
	dcc_set_compiler(args, 0);

	try
	{
		dcc_compiler->scan_args(args, on_server);
	}
	catch (...)
	{
		throw "CompilationJob: error";;
	}

	rs_trace("output file %s", +args.output_file);

	File temp_o, temp_d, temp_pdb;
	temp_o = dcc_make_tmpnam("distccd", ".o");
	if (!!dotd_fname)
		temp_d = dcc_make_tmpnam("distccd", ".d");
	if (!!pdb_fname)
		temp_pdb = dcc_make_tmpnam("distccd", ".pdb");

	enum dcc_compress compr = protover == 2 ? DCC_COMPRESS_LZO1X : DCC_COMPRESS_NONE;

	view_name = "";
	compile_dir = Directory();

	if (! (cmd_flags & CMD_FLAGS_ON_SERVER))
	{
		File temp_i = dcc_input_tmpnam(*dcc_compiler, args.input_file);

		try
		{
			unsigned int size_i;
			if ((ret = dcc_r_token_file(in_fd, "DOTI", temp_i, size_i, compr)))
				throw "CompilationJob: error";;
			dcc_compiler->set_input(args, temp_i.path());
		}
		catch (std::exception &x)
		{
			throw "CompilationJob: error";;
		}
	}
	else
	{
		if ((ret = dcc_r_view_name(in_fd, view_name))
			|| (ret = dcc_r_compile_dir(in_fd, compile_dir)))
			throw "CompilationJob: error";;
	}

	if ((ret = dcc_compiler->set_output(args, temp_o.path(), temp_d.path(), temp_pdb.path())))
		throw "CompilationJob: error";;

	if ((ret = dcc_check_compiler_masq(args[0])))
		throw "CompilationJob: error";

	dcc_set_compiler(args, 0);

	if (!!view_name)
	{
#ifdef _WIN32
		Directory viewdir = stringf("m:/%s", +view_name);
#else
		Directory viewdir = stringf("/view/%s/vobs", +view_name);
#endif
		if (!viewdir.exist())
		{
			int rc = system(+stringf("cleartool startview %s", +view_name));
			// rc is ignored due to false negatives
			if (!viewdir.exist())
			{
				rs_log_error("failed to start view %s (rc=%d)", +view_name, rc);
				ret = EXIT_BAD_ARGUMENTS;
				throw "CompilationJob: error";;
			}
		}
	}

#ifdef _WIN32
	ticks_spawn = GetTickCount();
#endif
	int compile_ret, status;
	proc_t cc_pid;
	File devnull(DEV_NULL);
	if ((compile_ret = dcc_spawn_child(args, cc_pid, &compile_dir, &devnull, &out_fname, &err_fname))
		|| (compile_ret = dcc_collect_child(args[0], cc_pid, status))) 
	{
		// We didn't get around to finding a wait status from the actual compiler
		status = W_EXITCODE(compile_ret, 0);
	}
#ifdef _WIN32
	ticks_collect = GetTickCount();
#endif

	if (on_server && !!temp_d)
		fix_dotd_file(temp_d, temp_o);

	if ((ret = dcc_x_result_header(out_fd, protover))
		|| (ret = dcc_x_cc_status(out_fd, status))
		|| (ret = dcc_x_file(out_fd, err_fname, "SERR", compr, NULL))
		|| (ret = dcc_x_file(out_fd, out_fname, "SOUT", compr, NULL)))
	{
		throw "CompilationJob: error";;
	}

	{
		// if compilation failed, DOTO carries a nonexistent file notification
		int failed = WIFSIGNALED(status) || WEXITSTATUS(status);
		off_t size_o;

		if ((ret = dcc_x_file(out_fd, failed ? File() : temp_o, "DOTO", compr, &size_o))
			|| (ret = dcc_x_file(out_fd, temp_d, "DOTD", compr, NULL))
			|| (ret = dcc_x_file(out_fd, temp_pdb, ".PDB", compr, NULL)))
		{
			throw "CompilationJob: error";;
		}
	}

#ifdef _WIN32
	ticks_endsend = GetTickCount();
#endif

	dcc_critique_status(status, args[0], args.input_file, "localhost", false);
	tcp_cork_sock(out_fd, 0);

	error = false;
	return ret;
}

//---------------------------------------------------------------------------------------------

CompilationJob::~CompilationJob()
{
	if (error)
		rs_log_error("error occurred during compilation of %s", +orig_input.path());

	dcc_remove_log_to_file();
	dcc_cleanup_tempfiles();

#ifdef _WIN32
	ticks_close = GetTickCount();

	time_comm1 = ticks_spawn - ticks_accept;
	time_compile = ticks_collect - ticks_spawn;
	time_comm2 = ticks_endsend - ticks_collect;
	time_wrap = ticks_close - ticks_endsend;

	rs_log(RS_LOG_INFO|RS_LOG_NONAME, "job complete %s: c>s=%-5d cc=%-5d s>c=%-5d wrap=%-5d", 
		+orig_input.path(), time_comm1, time_compile, time_comm2, time_wrap);
#endif
}
	
///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
