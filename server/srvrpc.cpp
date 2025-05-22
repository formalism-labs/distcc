/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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
 * Server-specific RPC code.
  **/

#include "common/config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>

#include "common/arg.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/rpc1.h"
#include "common/exitcode.h"
#include "common/hosts.h"

#include "server/dopt.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

dcc_exitcode dcc_r_request_header(fd_t ifd, enum dcc_protover &ver_ret)
{
    unsigned int vers;
    dcc_exitcode ret = dcc_r_token_int(ifd, "DIST", vers);
    
	if (!!ret) 
	{
        rs_log_error("client did not provide distcc magic fairy dust");
        return ret;
    }

    if (vers != 1 && vers != 2) 
	{
        rs_log_error("can't handle requested protocol version is %d", vers);
        return EXIT_PROTOCOL_ERROR;
    }

    ver_ret = (enum dcc_protover) vers;
    
    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_session_name(fd_t ifd, string &session_name)
{
	return dcc_r_token_string(ifd, "SSNN", session_name);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_flags(fd_t ifd, unsigned &flags)
{
	return dcc_r_token_int(ifd, "FLGS", flags);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_view_name(fd_t ifd, string &view_name)
{
	return dcc_r_token_string(ifd, "VIEW", view_name);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_compile_dir(fd_t ifd, Directory &cdir)
{
	string dir;
	dcc_exitcode ret = dcc_r_token_string(ifd, "CDIR", dir);
	cdir = Directory(dir);
	return ret;
}

//---------------------------------------------------------------------------------------------
// Read an argv[] vector from the network

dcc_exitcode dcc_r_argv(fd_t ifd, /*@out@*/ Arguments &args)
{
	unsigned int argc;
	if (dcc_r_token_int(ifd, "ARGC", argc))
		return EXIT_PROTOCOL_ERROR;

	rs_trace("reading %d arguments from job submission", argc);

	for (unsigned int i = 0; i < argc; i++)
	{
		string a;
		dcc_exitcode ret = dcc_r_token_string(ifd, "ARGV", a);
		if (ret)
            return ret;
		args << a;

        //@ rs_trace("argv[%d] = \"%s\"", i, a.c_str());
    }

	args.trace("got arguments");

	return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

Arguments dcc_r_argv(fd_t ifd)
{
	try
	{
		int argc = dcc_r_token_int(ifd, "ARGC");

		rs_trace("reading %d arguments from job submission", argc);

		Arguments args;
		for (int i = 0; i < argc; i++)
		{
			string arg = dcc_r_token_string(ifd, "ARGV");
			args << arg;

			//@ rs_trace("argv[%d] = \"%s\"", i, a[i]);
		}

		args.trace("got arguments");

		return args;
	}
	catch (...)
	{
		throw "protocol error";
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

