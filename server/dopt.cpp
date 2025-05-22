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

#include "common/types.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/exitcode.h"

#include "server/access.h"
#include "server/dopt.h"
#include "server/daemon.h"

#include "contrib/popt/popt.h"

#include "rvfc/exceptions/defs.h"
#include "rvfc/text/defs.h"

namespace distcc
{

using namespace rvfc::Text;
using rvfc::error;

///////////////////////////////////////////////////////////////////////////////////////////////

int opt_niceness = 5;           // default

// Number of children running jobs on this machine.  
// If zero (recommended), then dynamically set from the number of CPUs.
int arg_max_jobs = 0;

int arg_port = DISTCC_DEFAULT_PORT;

// If true, serve all requests directly from listening process without forking.  Better for debugging.
int opt_no_fork = 0;

int opt_worker = 0;
char *opt_server_id = 0;
int opt_inetd_mode = 0;
int opt_no_fifo = 0;

// If non-NULL, listen on only this address
char *opt_listen_addr = NULL;

list<dcc_allow_spec> opt_allowed;

// If true, run as service (linux: detach from the parent)
int opt_service = 0;

// If true, don't detach from the parent.
// This is probably necessary for use with daemontools or other monitoring programs, 
// and is also used by the test suite.
int opt_no_detach = 0; 

int opt_log_stderr = 0;

// Daemon exits after this many seconds.  
// Intended mainly for testing, to make sure daemons don't persist for too long.
int opt_lifetime = 0;

char *arg_pid_file = NULL;
char *arg_log_file = NULL;

// Enumeration values for options that don't have single-letter name.  
// These must be numerically above all the ascii letters.
enum 
{
    opt_log_to_file = 300,
    opt_log_level
};

//---------------------------------------------------------------------------------------------

const struct poptOption options[] = 
{
    { "allow", 'a',      POPT_ARG_STRING, 0, 'a', 0, 0 },
    { "jobs", 'j',       POPT_ARG_INT, &arg_max_jobs, 'j', 0, 0 },
#ifdef _WIN32
	{ "worker", 0,       POPT_ARG_STRING, &opt_server_id, 0, 0, 0 },
#endif
    { "help", 0,         POPT_ARG_NONE, 0, '?', 0, 0 },
    { "inetd", 0,        POPT_ARG_NONE, &opt_inetd_mode, 0, 0, 0 },
    { "lifetime", 0,     POPT_ARG_INT, &opt_lifetime, 0, 0, 0 },
    { "listen", 0,       POPT_ARG_STRING, &opt_listen_addr, 0, 0, 0 },
    { "log-file", 0,     POPT_ARG_STRING, &arg_log_file, 0, 0, 0 },
    { "log-level", 0,    POPT_ARG_STRING, 0, opt_log_level, 0, 0 },
    { "log-stderr", 0,   POPT_ARG_NONE, &opt_log_stderr, 0, 0, 0 },
    { "nice", 'N',       POPT_ARG_INT,  &opt_niceness,  0, 0, 0 },
#ifndef _WIN32
    { "no-detach", 0,    POPT_ARG_NONE, &opt_no_detach, 0, 0, 0 },
#endif
    { "no-fifo", 0,      POPT_ARG_NONE, &opt_no_fifo, 0, 0, 0 },
    { "no-fork", 0,      POPT_ARG_NONE, &opt_no_fork, 0, 0, 0 },
    { "pid-file", 'P',   POPT_ARG_STRING, &arg_pid_file, 0, 0, 0 },
    { "port", 'p',       POPT_ARG_INT, &arg_port,      0, 0, 0 },
    { "service", 0,      POPT_ARG_NONE, &opt_service, 0, 0, 0 },
#ifndef _WIN32
	{ "user", 0,         POPT_ARG_STRING, &opt_user, 'u', 0, 0 },
#endif
    { "verbose", 0,      POPT_ARG_NONE, 0, 'v', 0, 0 },
    { "version", 0,      POPT_ARG_NONE, 0, 'V', 0, 0 },
    { "wizard", 'W',     POPT_ARG_NONE, 0, 'W', 0, 0 },
    { 0, 0, 0, 0, 0, 0, 0 }
};

//---------------------------------------------------------------------------------------------

static void distccd_show_usage()
{
    dcc_show_version("distccd");
    printf (
"Usage:\n"
"   distccd [OPTIONS]\n"
"\n"
"Options:\n"
"    --help                     explain usage and exit\n"
"    --version                  show version and exit\n"
"    -P, --pid-file FILE        save daemon process id to file\n"
"    -N, --nice LEVEL           lower priority, 20=most nice\n"
"    --user USER                if run by root, change to this persona\n"
"    --jobs, -j LIMIT           maximum tasks at any time\n"
"  Networking:\n"
"    -p, --port PORT            TCP port to listen on\n"
"    --listen ADDRESS           IP address to listen on\n"
"    -a, --allow IP[/BITS]      client address access control\n"
"  Debug and trace:\n"
"    --log-level=LEVEL          set detail level for log file\n"
"      levels: critical, error, warning, notice, info, debug\n"
"    --verbose                  set log level to \"debug\"\n"
"    --no-detach                don't detach from parent (for daemontools, etc)\n" 
"    --log-file=FILE            send messages here instead of syslog\n"
"    --log-stderr               send messages to stderr\n"
"    --wizard                   for running under gdb\n"
"  Mode of operation:\n"
"    --inetd                    serve client connected to stdin\n"
"    --service                  run as service\n"
"\n"
"distccd runs either from inetd or as a standalone daemon to compile\n"
"files submitted by the distcc client.\n"
"\n"
"distccd should only run on trusted networks.\n"
);
}

//---------------------------------------------------------------------------------------------

class Popt
{
    poptContext po;
	int _opt;

public:
	Popt(const char *name, int argc, char *argv[])
	{
	    po = poptGetContext("distccd", argc, (const char **) argv, options, 0);
		next();
	}

	~Popt()
	{
	    poptFreeContext(po);
	}

	int next()
	{
		return _opt = poptGetNextOpt(po);
	}

	bool operator!() const { return _opt == -1; }

	int opt()
	{
		return _opt;
	}

	const char *arg()
	{
		return poptGetOptArg(po);
	}

	const char *badOpt()
	{
		return poptBadOption(po, POPT_BADOPTION_NOALIAS);
	}

	const char *error()
	{
		return poptStrerror(_opt);
	}
};

//---------------------------------------------------------------------------------------------

void
Configuration::parse_options(int argc, char *argv[])
{
	_exit = false;
	Popt popt("distccd", argc, argv);

    for (; !!popt; popt.next()) 
	{
        switch (popt.opt()) 
		{
        case '?':
            distccd_show_usage();
			_exit = true;
			return;

        case 'a': 
		{
            // TODO: Allow this to be a hostname, which is resolved to an address
			try
			{
				opt_allowed.push_back(dcc_allow_spec(popt.arg()));
			}
			catch (const char *x)
			{
				throw error("bad arguments: %s", x);
			}
            break;
		}

		case 'j':
            if (arg_max_jobs < 1 || arg_max_jobs > 200) 
			{
                rs_log_error("--jobs argument must be between 1 and 200");
                throw std::runtime_error("bad arguments");
            }
            break;

        case 'u':
#ifdef __linux__
			if (getuid() != 0 && geteuid() != 0) 
			{
                rs_log_warning("--user is ignored when distccd is not run by root");
                // continue
            }
#endif // __linux__
            break;

        case 'V':
			dcc_show_version("distccd");
			_exit = true;
			return;

        case opt_log_level:
            {
                const char *level_name = popt.arg();
                rs_loglevel level = rs_loglevel_from_name(level_name);
                if (level == RS_LOG__INVALID)
                    rs_log_warning("invalid --log-level argument \"%s\"", level_name);
                else
                    rs_trace_set_level(level);
            }
            break;

        case 'v':
            rs_trace_set_level(RS_LOG_DEBUG);
            break;

        case 'W':
            // catchall for running under debugger
#ifdef _WIN32
			opt_log_stderr = 1;
#else // __linux__
            opt_service = 0;
			opt_no_detach = 1;
            opt_no_fork = 1;
            opt_no_fifo = 1;
#endif // __linux__
            opt_service = 0;
            rs_trace_set_level(RS_LOG_DEBUG);
            break;

        default:                /* bad? */
            rs_log(RS_LOG_NONAME|RS_LOG_ERR|RS_LOG_NO_PID, "%s: %s", popt.badOpt(), popt.error());
			throw std::runtime_error("bad argument");
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
