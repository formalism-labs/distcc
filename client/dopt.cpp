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


// dopt.c -- Parse and apply server options

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common/distcc.h"
#include "common/exitcode.h"
#include "common/trace.h"
#include "common/util.h"

#include "client/dopt.h"
#include "client/config.h"

#include "contrib/popt/popt.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

#if 0

int cc_argc = 0;
const char **cc_argv;


#endif // 0

// Enumeration values for options that don't have single-letter name.  
// These must be numerically above all the ascii letters.
enum 
{
    opt_log_to_file = 300,
    opt_log_level,
	opt_compiler,
	opt_session,
	opt_view,
	opt_on_server
};

//---------------------------------------------------------------------------------------------

static void dcc_show_usage()
{
    dcc_show_version("distcc");
    printf (
"Usage:\n"
"   distccd [OPTIONS --] [COMPILER] [compile options]\n"
"\n"
"Options:\n"
"    -?, --help                 explain usage and exit\n"
"    -V, --version              show version and exit\n"
"  Networking:\n"
"    -p, --port PORT            TCP port to listen on\n"
"  Debug and trace:\n"
"    --log-level=LEVEL          set detail level for log file\n"
"      levels: critical, error, warning, notice, info, debug\n"
"    --verbose                  set log level to \"debug\"\n"
"    --no-detach                don't detach from parent (for daemontools, etc)\n"
"    --log-file=FILE            send messages to FILE\n"
"    --log-stderr               send messages to stderr\n"
"    --log-syslog               send messages to syslog\n"
"  Mode of operation:\n"
"    --compiler=CC              use compiler CC (one of gcc, msc, diab)\n"
"    --session=SESSION          part of session SESSION\n"
"    --view=VIEW                part of view VIEW\n"
"    --on-server                compile on server, do not preprocess\n"
"\n"
"Environment variables:\n"
"   See the manual page for a complete list.\n"
"   DISTCC_VERBOSE=1           give debug messages\n"
"   DISTCC_LOG                 send messages to file, not stderr\n"
"   DISTCC_SSH                 command to run to open SSH connections\n"
"   DISTCC_DIR                 directory for host list and locks\n"
"\n"
"Server specification:\n"
"A list of servers is taken from the environment variable $DISTCC_HOSTS, or\n"
"$DISTCC_DIR/hosts, or ~/.distcc/hosts, or %s/distcc/hosts.\n"
"Each host can be given in any of these forms, see the manual for details:\n"
"\n"
"   localhost                  run in place\n"
"   HOST                       TCP connection, port %d\n"
"   HOST:PORT                  TCP connection, specified port\n"
"   @HOST                      SSH connection\n"
"   USER@HOST                  SSH connection to specified host\n"
"\n"
"distcc distributes compilation jobs across volunteer machines running\n"
"distccd.  Jobs that cannot be distributed, such as linking or \n"
"preprocessing are run locally.  distcc should be used with make's -jN\n"
"option to execute in parallel on several machines.\n",
    SYSCONFDIR,
    DISTCC_DEFAULT_PORT);
}

//---------------------------------------------------------------------------------------------

class Popt
{
	poptContext ctx;
	int argc;
	char **argv;

public:
	int opt;

	Popt(const char *prog, const Arguments &args, const struct poptOption *options)
	{
		argc = args.count();
		argv = args.argv();

		ctx = poptGetContext(prog, argc, (const char **) argv, options, 0);
	}

	~Popt()
	{
		delete[] argv;
		poptFreeContext(ctx);
	}

	bool next()
	{
		opt = poptGetNextOpt(ctx);
		return opt != -1;
	}

	const char *arg()
	{
		return poptGetOptArg(ctx);
	}
	
	const char *error()
	{
		return poptStrerror(opt);
	}

	const char *bad_option()
	{
		return poptBadOption(ctx, POPT_BADOPTION_NOALIAS);
	}
};

//---------------------------------------------------------------------------------------------
// returns false if program should exit immediately.

bool ClientConfig::parse_options(const Arguments &args)
{
	int popt_port = DISTCC_DEFAULT_PORT;

	rs_loglevel popt_log_level = RS_LOG__INVALID;
	int popt_verbose = 0;

	int popt_log_stderr = 0;
	int popt_log_syslog = 0;

	char *popt_log_file = "";

	char *popt_compiler = "";
	char *popt_session = "";
	char *popt_view = "";
	char *popt_hosts_class = "";

	int popt_on_server = 0;

	const struct poptOption options[] = 
	{
		{ "log-file", 0,     POPT_ARG_STRING,	&popt_log_file,		0, 0, 0 },
		{ "log-level", 0,    POPT_ARG_STRING,	0,					opt_log_level, 0, 0 },
		{ "log-stderr", 0,   POPT_ARG_NONE,		&popt_log_stderr,	0, 0, 0 },
		{ "log-syslog", 0,   POPT_ARG_NONE,		&popt_log_syslog,	0, 0, 0 },
		{ "port", 'p',       POPT_ARG_INT,		&popt_port,			0, 0, 0 },
		{ "compiler", 0,     POPT_ARG_STRING,	&popt_compiler,		opt_compiler, 0, 0 },
		{ "session", 0,      POPT_ARG_STRING,	&popt_session,		opt_session, 0, 0 },
		{ "view", 0,         POPT_ARG_STRING,	&popt_view,			opt_view, 0, 0 },
		{ "on-server", 0,    POPT_ARG_NONE,		&popt_on_server,		opt_on_server, 0, 0 },
		{ "hosts-class", 0,  POPT_ARG_STRING,	&popt_hosts_class,	0, 0, 0 },
		{ "verbose", 0,      POPT_ARG_NONE,		&popt_verbose,		'v', 0, 0 },
		{ "help", 0,         POPT_ARG_NONE,		0,					'?', 0, 0 },
		{ "version", 0,      POPT_ARG_NONE,		0,					'V', 0, 0 },
		{ 0, 0, 0, 0, 0, 0, 0 }
	};

	if (args.count() <= 1 || args[1].equalsto_one_of("--help", "-?", 0)) 
	{
		dcc_show_usage();
		return false;
    }

    if (args[1].equalsto_one_of("--version", "-V", 0)) 
	{
		dcc_show_version("distcc");
		return false;
    }

	cc_args = args;
	Arguments::ConstIterator j = args.find("--");
	if (!j)
	{
		// no --, just remote program name
		cc_args.pop_front(1);
		return true;
	}

	Arguments args1 = cc_args.pop_front(j.position());
	cc_args.pop_front(1); // remove --

	Popt popt("distcc", args1, options);

    while (popt.next()) 
	{
        switch (popt.opt) 
		{
        case '?':
            dcc_show_usage();
			return false;

        case 'V':
			dcc_show_version("distcc");
			return false;

        case opt_log_level:
            {
				const char *level_name = popt.arg();
				rs_loglevel level = rs_loglevel_from_name(level_name);
				if (level == RS_LOG__INVALID)
					rs_log_warning("invalid --log-level argument \"%s\"", level_name);
				else
					arg_log_level = level;
            }
            break;

		case opt_compiler:
			if (!compiler.equalsto_one_of("gcc", "msc", "diab", 0))
			{
				rs_log_error("invalid --compiler argument \"%s\".", +compiler);
				throw "bad args";
			}
			break;

        default: // bad?
			if (popt.opt > 0)
				continue;
            rs_log(RS_LOG_NONAME|RS_LOG_ERR|RS_LOG_NO_PID, "%s: %s", popt.bad_option(), popt.error());
			throw "bad args";
        }
    }

	port = popt_port;

	arg_log_level = popt_log_level;
	verbose = !!popt_verbose;

	log_stderr = !!popt_log_stderr;
	log_syslog = !!popt_log_syslog;

	log_file = popt_log_file;

	compiler = popt_compiler;
	session = popt_session;
	view = popt_view;
	hosts_class = popt_hosts_class;

	on_server = !!popt_on_server;

	return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
