/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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


              /* The dead cry out with joy when their books are reprinted
               */



#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
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
#include "lzo/minilzo.h"

#include "rvfc/defs.h"

namespace distcc
{

using namespace rvfc;

///////////////////////////////////////////////////////////////////////////////////////////////

// TODO:
// It might be nice if killing a child, or dropping the socket caused its compiler to be killed too.  
// This would probably require running the compiler in a separate process group, and 
// remembering to kill it when the child is signalled.
// I can only think of a few cases where this would be a big deal: 
// a runaway compiler, where you probably can just manually kill that process, and a client 
// interrupt, in which case allowing the compiler to run to completion is not such a big deal.

//---------------------------------------------------------------------------------------------

UINT32 ticks_accept, ticks_spawn, ticks_collect, ticks_endsend, ticks_close, time_comm1, 
	time_compile, time_comm2, time_wrap;

//---------------------------------------------------------------------------------------------

// Main loop for the parent process with the new preforked implementation.
// The parent is just responsible for keeping a pool of children and they
// accept connections themselves.

void
StandaloneServer::preforking_parent()
{
	dcc_log_daemon_started("preforking daemon");

	while (!dcc_term_flag)
	{
#ifndef _WIN32
        pid_t kid;
#endif

		while (dcc_nkids < dcc_max_kids && !dcc_term_flag)
		{
#ifdef _WIN32
			try
			{
				Worker *worker = new Worker(CurrentProcess().programPath(), Uuid(), listen_fd);
				worker->run();
			}
			catch (std::exception &x)
			{
				std::string err = stringf("StandaloneServer: worker creation failed [%s]", x.what());
				rs_log_error("%s", err.c_str());
				throw std::runtime_error(err.c_str());
			}

#else // ! _WIN32
            if ((kid = fork()) == -1) 
			{
				rs_log_error("StandaloneServer: fork failed");
				throw std::runtime_error("StandaloneServer: fork failed");
            }
			if (kid == 0)
			{
				// in child
				WorkerProcess worker(listen_fd);
				worker.run();
				dcc_exit(0);
			}

#endif // ! _WIN32

			++dcc_nkids;
			rs_trace("up to %d children", dcc_nkids);

			// Don't start them too quickly, or we might overwhelm a machine that's having trouble
			sleep(1);

			reap_kids(false);
		}

        // wait for any children to exit, and then start some more
		reap_kids(true);

        // Another little safety brake here: since children should not exit
        // too quickly, pausing before starting them should be harmless.
        sleep(1);
	}
}

//---------------------------------------------------------------------------------------------

// Fork a child to repeatedly accept and handle incoming connections.
// To protect against leaks, we quit after 50 requests and let the parent recreate us.
// (prev. dcc_preforked_child())

#if defined(_WIN32)

//---------------------------------------------------------------------------------------------

WorkerProcess::WorkerProcess(const std::string &serverId) : _context(serverId)
{
	dcc_term_flag = false;
	listen_fd = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, &_context->prot_info, 0, 0);
	if (listen_fd == INVALID_SOCKET)
		throw std::runtime_error("cannot create listening socket");
}

//---------------------------------------------------------------------------------------------

void
WorkerProcess::run()
{
	while (!dcc_term_flag)
	{
		int acc_fd;
		fd_t _acc_fd;
		struct dcc_sockaddr_storage cli_addr;
		socklen_t cli_len = sizeof(cli_addr);

		do 
			acc_fd = accept(listen_fd, (struct sockaddr *) &cli_addr, &cli_len);
		while (acc_fd == -1 && errno == EINTR);

		if (acc_fd == -1) 
		{
			translate_wsaerror();
			if (!dcc_term_flag)				
				rs_log_error("accept failed: %s", socket_error_str());
			continue;
		}

		ticks_accept = GetTickCount();

		_acc_fd = dcc_fd(acc_fd, 1);
		dcc_service_job(_acc_fd, _acc_fd, (struct sockaddr *) &cli_addr, cli_len);

		dcc_close(_acc_fd);
	}
}

//---------------------------------------------------------------------------------------------

#else // ! _WIN32

//---------------------------------------------------------------------------------------------

WorkerProcess::WorkerProcess(int listenFd) : listen_fd(listenFd)
{
	dcc_term_flag = false;
}

//---------------------------------------------------------------------------------------------

void WorkerProcess::run()
{
	const int child_lifetime = 50;

	for (int ireq = 0; ireq < child_lifetime; ireq++) 
	{
		int acc_fd;
		fd_t _acc_fd;
		struct dcc_sockaddr_storage cli_addr;
		socklen_t cli_len = sizeof(cli_addr);

		do 
			acc_fd = accept(listen_fd, (struct sockaddr *) &cli_addr, &cli_len);
		while (acc_fd == -1 && errno == EINTR);

		if (acc_fd == -1) 
		{
			rs_log_error("accept failed: %s", strerror(errno));
			dcc_exit(EXIT_CONNECT_FAILED);
		}

		_acc_fd = dcc_fd(acc_fd, 1);
		dcc_service_job(_acc_fd, _acc_fd, (struct sockaddr *) &cli_addr, cli_len);

		dcc_close(_acc_fd);
	}

	rs_log_info("worn out");
}

//---------------------------------------------------------------------------------------------

#endif // ! _WIN32

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
