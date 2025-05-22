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

#include "config.h"
#include "rvfc/defs.h"

namespace distcc
{

// daemon.c
int dcc_refuse_root();
int dcc_set_lifetime();
int dcc_log_daemon_started(const char *role);

// service.c
void dcc_win32_service(int argc, char *argv[]);

// dsignal.c
void dcc_ignore_sighup();
void dcc_daemon_catch_signals();

// dparent.c
void dcc_remove_pid();

// prefork.c
#ifdef _WIN32
extern UINT32 ticks_accept, ticks_spawn, ticks_collect, ticks_endsend, ticks_close, 
	time_comm1, time_compile, time_comm2, time_wrap;
#endif

int dcc_preforking_parent(int listen_fd);

// serve.c
//struct sockaddr;
int dcc_service_job(fd_t in_fd, fd_t out_fd, struct sockaddr *, int);

// setuid.c
int dcc_discard_root();
extern const char *opt_user;
#ifdef _WIN32
extern HANDLE *child_threads;
#endif

extern volatile pid_t dcc_master_pid;

using rvfc::NewProcess;
using rvfc::Job;
using rvfc::SharedObject;
using rvfc::HandleX;
using namespace rvfc::Text;

///////////////////////////////////////////////////////////////////////////////////////////////

class Log
{
public:
	Log();

	void setupRealLog();
};

//---------------------------------------------------------------------------------------------

class Configuration
{
	bool _exit;

public:
	Configuration(int argc, char *argv[]);

	void parse_options(int argc, char *argv[]);

	bool exit() const { return _exit; }
};

//---------------------------------------------------------------------------------------------

class GeneralServer
{
public:
	GeneralServer();
};

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

struct WorkerContext
{
	WorkerContext() {}

	WSAPROTOCOL_INFO prot_info;
};

#endif // _WIN32

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

class Worker : public NewProcess
{
	typedef WorkerContext Context;

	std::string context_name;
	SharedObject<Context> _context;

public:
	Worker(const std::string &prog, const std::string &id, int listen_fd) : 
		NewProcess(stringf("%s --worker=%s", prog.c_str(), id.c_str())),
		context_name(id), _context(id)
	{
		if (WSADuplicateSocket(listen_fd, NewProcess::id(), &_context->prot_info) != 0)
			throw std::runtime_error("cannot duplicate listening socket");
	}

	virtual ~Worker() {}
};

#endif // _WIN32

//---------------------------------------------------------------------------------------------

class StandaloneServer : public GeneralServer
{
	bool no_fork, run_as_service;

	int dcc_nkids;
	// In forking or prefork mode, the maximum number of connections we want to allow at any time
	int dcc_max_kids;

	int listen_fd;

    int n_cpus;

#ifdef _WIN32
	Job _job;
#endif

	bool dcc_term_flag;
#ifdef _WIN32
	HandleX termination_event;
#endif

	void setup_listen_socket();

	void determine_worker_count();

	void nofork_parent();
	void preforking_parent();

	void reap_kids(bool must_reap);

public:
	StandaloneServer(bool noFork, bool runAsService);
	~StandaloneServer();

	void run();
	void terminate();
};

//---------------------------------------------------------------------------------------------

class InetdServer : public GeneralServer
{
public:
	InetdServer() {}
	~InetdServer() {}

	void run();
};

//---------------------------------------------------------------------------------------------

class WorkerProcess
{
#ifdef _WIN32
	typedef WorkerContext Context;
	SharedObject<Context> _context;
#endif

	int listen_fd;
	bool dcc_term_flag;

public:
#if defined(_WIN32)
	WorkerProcess(const std::string &serverId);
#else
	WorkerProcess(int listen_fd);
#endif

	void run();
};

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

class Win32Service : public GeneralServer
{
	static SERVICE_TABLE_ENTRY service_table[];

public:
	Win32Service();

	void run();
};

#endif // _WIN32

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
