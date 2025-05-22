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


                /* The lyf so short, the craft so long to lerne.
                 * -- Chaucer */


    
/**
 * @file
 *
 * Routines to parse <tt>$DISTCC_HOSTS</tt>.  Actual decisions about
 * where to run a job are in where.c.
 *
 * The grammar of this variable is, informally:
 *
  DISTCC_HOSTS = HOSTSPEC ...
  HOSTSPEC = LOCAL_HOST | SSH_HOST | TCP_HOST | OLDSTYLE_TCP_HOST
  LOCAL_HOST = localhost[/LIMIT]
  SSH_HOST = [USER]@HOSTID[/LIMIT][:COMMAND][OPTIONS]
  TCP_HOST = HOSTID[:PORT][/LIMIT][OPTIONS]
  OLDSTYLE_TCP_HOST = HOSTID[/LIMIT][:PORT][OPTIONS]
  HOSTID = HOSTNAME | IPV4
  OPTIONS = ,OPTION[OPTIONS]
  OPTION = lzo
 *
 * Any amount of whitespace may be present between hosts.
 *
 * The command specified for SSH defines the location of the remote
 * server, e.g. "/usr/local/bin/distccd".  This is provided as a
 * convenience who have trouble getting their PATH set correctly for
 * sshd to find distccd, and should not normally be needed.
 *
 * If you need to specify special options for ssh, they should be put
 * in ~/.ssh/config and referenced by the hostname.
 *
 * The TCP port defaults to 3632 and should not normally need to be
 * overridden.
 *
 * IPv6 literals are not supported yet.  They will need to be
 * surrounded by square brackets because they may contain a colon,
 * which would otherwise be ambiguous.  This is consistent with other
 * URL-like schemes.
 */


/*
       Alexandre Oliva writes

        I take this opportunity to plead people to consider such issues when
        proposing additional syntax for DISTCC_HOSTS: if it was possible to
        handle DISTCC_HOSTS as a single shell word (perhaps after turning
        blanks into say commas), without the risk of any shell active
        characters such as {, }, ~, $, quotes getting in the way, outputting
        distcc commands that override DISTCC_HOSTS would be far
        simpler.

  TODO: Perhaps entries in the host list that "look like files" (start
    with '/' or '~') should be read in as files?  This could even be
    recursive.
*/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#endif

#include <string>
#include <regex>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/hosts.h"
#include "client/dopt.h"
#include "client/config.h"

#include "rvfc/text/defs.h"
#include "rvfc/types/defs.h"

namespace distcc
{

using namespace std::tr1;
using namespace rvfc::Text;
//using rvfc::text;
using rvfc::Sext;

///////////////////////////////////////////////////////////////////////////////////////////////

const int dcc_default_port = DISTCC_DEFAULT_PORT;

//---------------------------------------------------------------------------------------------

/**
 * Get a list of hosts to use.
 *
 * Hosts are taken from DISTCC_HOSTS, if that exists.  Otherwise, they are
 * taken from $DISTCC_DIR/hosts, if that exists.  Otherwise, they are taken
 * from ${sysconfdir}/distcc/hosts, if that exists.  Otherwise, we fail.
 **/

/* TODO: Perhaps in the future allow filenames to be given in environment
 * variables to cause other files to be "included". */

HostDefs::HostDefs(const string &hosts_class)
{
	string hosts_var = "DISTCC_HOSTS";
	if (!!hosts_class)
	{
		hosts_var += "_";
		hosts_var += hosts_class;
	}

	char *spec = getenv(+hosts_var);
    if (spec != NULL)
	{
        rs_trace("read hosts from environment");
		_hosts = parse_hosts(spec);
		return;
    }

	try
	{
		ClientConfig cfg;
		File hosts_file = cfg.hostsFile();
		_hosts = parse_hosts_file(hosts_file);
	}
	catch (...)
	{
	    // FIXME: Clearer message?
		rs_log_warning("no hostlist is set; can't distribute work");
		throw "no hostlist is set; can't distribute work";
	}

	remove_disliked();
    if (_hosts.empty())
		throw "no hosts exist";
}

//---------------------------------------------------------------------------------------------

/**
 * Return a hostlist read from fname (possibly recursively.)
 **/
list<dcc_hostdef> HostDefs::parse_hosts_file(const File &fname)
{
    rs_trace("load hosts from %s", fname.name().c_str());

	list<dcc_hostdef> hosts;
	list<string> host_lines = dcc_load_file_lines(fname);
	for (list<string>::const_iterator i = host_lines.begin(); i != host_lines.end(); ++i)
	{
		list<dcc_hostdef> hosts1 = parse_hosts(*i);
		hosts.splice(hosts.end(), hosts1); // append hosts1 to hosts
	}

	return hosts;
}

//---------------------------------------------------------------------------------------------

class HostsParser
{
	list<dcc_hostdef> _hosts;

	bool parse(const text &s)
	{
		return 
			   parse_local(s) 
			|| parse_ssh(s) 
			|| parse_tcp(s) 
			|| parse_old_tcp(s);
	}

	// localhost[/limit]
	bool parse_local(const text &s)
	{
		text::regex::match m;
		if (!s.match("localhost(/\\d+)?", m))
			return false;

		text limit = m[1];
		_hosts.push_back(dcc_hostdef(limit(1)));
		return true;
	}

	// user@host[/limit][:command][options]
	bool parse_ssh(const text &s)
	{
		text::regex::match m;
		if (!s.match("(\\w+)@([^/:,]+)(/\\d+)?(:[^,]+)?((,\\w+)*)", m))
			return false;
		text user = m[1];
		text host = m[2];
		text limit = m[3];
		text command = m[4];
		text options = m[5];

		_hosts.push_back(dcc_hostdef(user, host, limit(1), command(1), parse_options(options)));
		return true;
	}

	// host[:port][/limit][options]
	bool parse_tcp(const text &s)
	{
		text::regex::match m;
		if (!s.match("([^/:,]+)(/\\d+)?(:\\d+)?((,\\w+)*)", m))
			return false;
		text host = m[1];
		text port = m[2];
		text limit = m[3];
		text options = m[4];
		_hosts.push_back(dcc_hostdef(host,  port.empty()? port : port(1), 
											limit.empty()? limit :limit(1), parse_options(options)));
		return true;
	}

	// host[/limit][:port][options]
	bool parse_old_tcp(const text &s)
	{
		text::regex::match m;
		if (!s.match("([^/:,]+)(:\\d+)?(/\\d+)?((,\\w+)*)", m))
			return false;
		text host = m[1];
		text limit = m[2];
		text port = m[3];
		text options = m[4];
		_hosts.push_back(dcc_hostdef(host, port.empty()? port : port(1), 
										   limit.empty()? limit :limit(1), parse_options(options)));
		return true;
	}

	static Sext parse_options(const text &options)
	{
		Sext set;
		for (text::regex::iterator i = options.scan(",([^,]+)"); !!i; ++i)
		{
			//text opt = i.match(1);
			//if (opt == "lzo")
			//	set << opt;
			set << text(i.match(1));
		}
		return set;
	}

public:
	HostsParser(const text &spec)
	{
		for (text::regex::iterator i = spec.scan("(\\S+)"); !!i; ++i)
			parse(i[1]);
	}

	list<dcc_hostdef> &hosts() { return _hosts; }
};


//---------------------------------------------------------------------------------------------

/**
 * @p where is the host list, taken either from the environment or file.
 *
 * @return list of dcc_hostdef elements
 **/
list<dcc_hostdef>
HostDefs::parse_hosts(const string &spec)
{
	HostsParser parser(spec);
	return parser.hosts();
}

///////////////////////////////////////////////////////////////////////////////////////////////

LocalHostDefs::LocalHostDefs() : HostDefs(Empty())
{
	dcc_hostdef local;
	_hosts.push_back(local);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
