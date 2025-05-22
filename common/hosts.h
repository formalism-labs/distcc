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

/**
 * @file
 *
 * Declarations for distcc host selection stuff.
 **/

// A simple linked list of host definitions.  All strings are malloc-ed.

#ifndef _DISTCC_HOSTS_H_
#define _DISTCC_HOSTS_H_

#include <string>

#include "rvfc/filesys/defs.h"
#include "rvfc/text/defs.h"

#include "common/arg.h"

namespace distcc
{

using std::string;

using namespace rvfc::Text;
using rvfc::Sext;
using rvfc::Path;

///////////////////////////////////////////////////////////////////////////////////////////////

enum dcc_mode
{
    DCC_MODE_TCP = 1,
    DCC_MODE_SSH,
    DCC_MODE_LOCAL
};

//---------------------------------------------------------------------------------------------

class dcc_hostdef 
{
	void set_params(const text *port_, const text *slots, const Sext *options)
	{
		int cpus;
		if (!dcc_ncpus(cpus))
			n_slots = cpus;
		else
			n_slots = 2;

		n_slots = !slots || !*slots ? 2 : slots->to_int("host slots");
		port = !port_ || !*port_ ? DISTCC_DEFAULT_PORT : port_->to_int("port");
		if (!options || !(*options)["lzo"])
		{
			compr = DCC_COMPRESS_NONE;
			protover = DCC_VER_1;
		}
		else
		{
			compr = DCC_COMPRESS_LZO1X;
			protover = DCC_VER_2;
		}
	}

public:
	dcc_hostdef(const text &slots = "")
	{
		mode = DCC_MODE_LOCAL;
		hostname = "localhost";
		hostdef_string = hostname;
		set_params(0, !slots ? 0 : &slots, 0);
	}

	dcc_hostdef(const text &host, const text &port_, const text &slots, const Sext &options)
	{
		hostname = host;
		hostdef_string = hostname;
		mode = DCC_MODE_TCP;
		set_params(!port_ ? 0 : &port_, !slots ? 0 : &slots, &options);
	}

	dcc_hostdef(const text &user_, const text &host, const text &slots, const text &command, const Sext &options)
	{
		user = user_;
		hostname = host;
		hostdef_string = hostname;
		mode = DCC_MODE_SSH;
		ssh_command = command;
		set_params(0, !slots ? 0 : &slots, &options);
	}

	enum dcc_mode mode;
    string user;
    string hostname;
    int port;
    string ssh_command;

    // Number of tasks that can be dispatched concurrently to this machine
    int n_slots;

    // The full name of this host, taken verbatim from the host definition
    string hostdef_string;

    enum dcc_protover protover;

    // The kind of compression to use for this host
    enum dcc_compress compr;

	void enjoyed_host();
	void disliked_host();

	int check_backoff() const;

	int mark_timefile(const string &lockname);
	void remove_timefile(const string &lockname);
	int check_timefile(const string &lockname, time_t &mtime) const;

	Path make_lock_filename(const text &lockname, int iter) const;
	int lock(const char *lockname, int slot, int block, int &lock_fd);

	void note_execution(const Arguments &args);

	int remote_connect(fd_t &to_net_fd, fd_t &from_net_fd, pid_t &ssh_pid);
};

//---------------------------------------------------------------------------------------------

class HostDefs
{
protected:
	typedef list<dcc_hostdef> HostsList;
	HostsList _hosts;

	HostsList parse_hosts_file(const File &fname);
	HostsList parse_hosts(const string &spec);

	void remove_disliked();
	static void lock_pause();

protected:
	struct Empty {};
	HostDefs(Empty) {}

public:
	HostDefs(const string &hosts_class = string(""));

	bool operator!() const { return _hosts.empty(); }

	dcc_hostdef lock_one(int &cpu_lock_fd);
};

//---------------------------------------------------------------------------------------------

class LocalHostDefs : public HostDefs
{
public:
	LocalHostDefs();

	dcc_hostdef host() { return _hosts.front(); }
};

//---------------------------------------------------------------------------------------------

// Static definition of localhost
extern LocalHostDefs local_hostdefs;
extern dcc_hostdef dcc_hostdef_local;

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _DISTCC_HOSTS_H_
