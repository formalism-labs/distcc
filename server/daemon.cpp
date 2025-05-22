/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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


                /* "Just like distributed.net, only useful!" */

/**
 * @file
 *
 * distcc volunteer server.  Accepts and serves requests to compile files.
 *
 * May be run from inetd (default if stdin is a socket), or as a daemon by itself.  
 *
 * distcc has an adequate but perhaps not optimal system for deciding where to send files.
 * The general principle is that the server should say how many jobs it is willing to accept, 
 * rather than the client having to know.  
 * This is probably good in two ways: 
 * it allows for people in the future to impose limits on how much work their contributed machine will do, 
 * and secondly it seems better to put this information in one place rather than on every client.
 **/

#include "common/config.h"

#pragma comment(lib, "mpr")

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>

#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#include <arpa/inet.h>

#elif defined(_WIN32)
#include "syslog/syslog.h"
#endif // _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
//#include <io.h>
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "common/exitcode.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/types.h"
#include "common/compiler.h"
#include "common/version.h"

#include "server/server.h"
#include "server/dopt.h"
#include "server/srvnet.h"
#include "server/daemon.h"

#ifdef _WIN32
#include "subproc/sub_proc.h"
#endif

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

static int dcc_should_be_inetd()
{
#ifdef _WIN32
	return 0; // daemon mode

#else
    // Work out if we ought to serve stdin or be a standalone daemon
    if (opt_inetd_mode)
        return 1;
    if (opt_service)
        return 0;

	if (is_a_socket(STDIN_FILENO)) 
	{
        rs_log_info("stdin is socket; assuming --inetd mode");
        return 1;
    }
	
	if (isatty(STDIN_FILENO)) 
	{
        rs_log_info("stdin is a tty; assuming --service mode");
        return 0;
    }

    rs_log_info("stdin is neither a tty nor a socket; assuming --service mode");
    return 0;
#endif // ! _WIN32
}

//---------------------------------------------------------------------------------------------

static int dcc_setup_daemon_path()
{
    int ret;
    const char *path = getenv("DISTCCD_PATH");

    if (path != NULL) 
	{
		if ((ret = dcc_set_path(path)))
			return ret;
        return 0;
    }

	path = getenv("PATH");
    rs_log_info("daemon's PATH is %s", path ? path : "(NULL)");
    return 0;
}

//---------------------------------------------------------------------------------------------

Configuration::Configuration(int argc, char *argv[])
{
	parse_options(argc, argv);

	if (opt_service && !arg_log_file)
		opt_log_stderr = 1;

#ifdef FEATURE_FORCE_OPT_ALLOWED
	// check this before redirecting the logs, so that it's really obvious
	if (!dcc_should_be_inetd())
        if (!opt_allowed) 
		{
            rs_log_error("--allow option is now mandatory; "
				"you must specify which clients are allowed to connect");
			throw std::runtime_error("bad arguments");
		}
#endif // FEATURE_FORCE_OPT_ALLOWED
}

//---------------------------------------------------------------------------------------------

static void net_use(string local, string remote)
{
	NETRESOURCE nr;
	nr.dwType = RESOURCETYPE_DISK;
	nr.lpLocalName = (LPSTR) +local;
	nr.lpRemoteName = (LPSTR) +remote;
	nr.lpProvider = 0;
	int rc = WNetUseConnection(NULL, &nr, NULL, NULL, 0, NULL, NULL, NULL);
	if (rc != NO_ERROR && rc != ERROR_ALREADY_ASSIGNED)
		throw std::runtime_error(stringf("cannot assign network drive %s to %s", +local, +remote));

}

//---------------------------------------------------------------------------------------------

static void dcc_system_setup()
{
#ifdef _WIN32
	//@@ this is one big ugly hack
	if (!getenv("WIND_BASE"))
		_putenv("WIND_BASE=r:/build/dev/libs/vxworks/6.3");
	if (!getenv("LM_LICENSE_FILE"))
		_putenv("LM_LICENSE_FILE=27000@bsp_server");

	net_use("r:", "\\\\storage\\nbu");
	net_use("y:", "\\\\vbu-dfs\\legacy");

/*
	NETRESOURCE nr;
	nr.dwType = RESOURCETYPE_DISK;
	nr.lpLocalName = "r:";
	nr.lpRemoteName = "\\\\storage\\nbu";
	nr.lpProvider = 0;
	int rc = WNetUseConnection(NULL, &nr, NULL, NULL, 0, NULL, NULL, NULL);
	if (rc != NO_ERROR && rc != ERROR_ALREADY_ASSIGNED)
		throw std::runtime_error("cannot assign network drive");
*/

#endif // _WIN32

    // Do everything from root directory.  
	// Allows start directory to be unmounted, should make accidental writing of local files cause a failure...
	string tmp = dcc_get_tmp_top();
    if (chdir(tmp.c_str()) == -1) 
	{
        rs_log_error("failed to chdir to %s: %s", +tmp, strerror(errno));
		throw "dcc_system_setup: cannot chdir to temp dir";
    }

	rs_trace("chdir to %s", tmp.c_str());
}

//---------------------------------------------------------------------------------------------

GeneralServer::GeneralServer()
{
	dcc_system_setup();

#ifdef __linux__
    if (dcc_set_lifetime() != 0)
        throw "dcc_set_lifetime failed";

    // do this before giving away root
    if (nice(opt_niceness) == -1) 
	{
        rs_log_warning("nice %d failed: %s", opt_niceness, strerror(errno));
        // continue anyhow
    }

    if (dcc_discard_root() != 0)
        throw "dcc_discard_root failed";
#endif // __linux__

	rs_log_info("--------------------------------------------------------------------------------");
    dcc_setup_daemon_path();
}

//---------------------------------------------------------------------------------------------
// If a --lifetime options was specified, set up a timer that will kill the daemon when it expires

int dcc_set_lifetime()
{
#ifdef __linux__
    if (opt_lifetime) 
	{
        alarm(opt_lifetime);
		// rs_trace("set alarm for %+d seconds", opt_lifetime);
    }
#endif
    return 0;
}

//---------------------------------------------------------------------------------------------
// Serve a single file on stdin, and then exit

void InetdServer::run()
{
	int ret, close_ret;
	struct dcc_sockaddr_storage ss;
	struct sockaddr *psa = (struct sockaddr *) &ss;
	socklen_t len = sizeof(ss);
    
	dcc_log_daemon_started("inetd server");

    if ((getpeername(STDIN_FILENO, psa, &len) == -1)) 
	{
        // This can fail with ENOTSOCK if e.g. sshd has started us on a pipe, not on a socket.
		// I think it's harmless.
        rs_log_notice("failed to get peer name: %s", strerror(errno));
        psa = NULL; // make sure we don't refer to uninitialized mem
        len = 0;
    }

    ret = dcc_service_job(dcc_fd(STDIN_FILENO, 0), dcc_fd(STDOUT_FILENO, 0), psa, len);

    close_ret = dcc_close(dcc_fd(STDIN_FILENO, 0));

	//return ret ? ret : close_ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

///////////////////////////////////////////////////////////////////////////////////////////////

using namespace distcc;

bool g_dbg = false;

// for trace.c
char const *rs_program_name = "distccd";

//---------------------------------------------------------------------------------------------

int main(int argc, char *argv[])
{
	try
	{
#ifdef _WIN32
		dcc_win32_startup();
#endif

		Log log;

		Configuration cfg(argc, argv);
		if (cfg.exit())
			exit(0);

		log.setupRealLog();

#if defined(_WIN32)
		if (opt_service)
		{
			Win32Service service;
			service.run();
		}
		else if (opt_server_id)
		{
			while (g_dbg)
				sleep(1);
			WorkerProcess worker(opt_server_id);
			worker.run();
		}
		else
		{
			StandaloneServer server(!!opt_no_fork, !!opt_service);
			server.run();
		}

#elif defined(__linux__)
		if (dcc_should_be_inetd())
		{
			InetdServer server;
			server.run();
		}
		else
		{
			StandaloneServer server(!!opt_no_fork, !!opt_service);
			server.run();
		}
#endif

		exit(0);
	}
	catch (std::exception &x)
	{
		rs_trace("distccd: exception: %s", x.what());
		dcc_exit(-1);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

