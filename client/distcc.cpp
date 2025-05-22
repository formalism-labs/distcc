/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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

			/* 4: The noise of a multitude in the
			 * mountains, like as of a great people; a
			 * tumultuous noise of the kingdoms of nations
			 * gathered together: the LORD of hosts
			 * mustereth the host of the battle.
			 *		-- Isaiah 13 */

/**
* @file
*
* Entry point for the distcc client.
*
* There are three methods of use for distcc: explicit (distcc gcc -c
* foo.c), implicit (distcc -c foo.c) and masqueraded (gcc -c foo.c,
* where gcc is really a link to distcc).
*
* Detecting these is relatively easy by examining the first one or
* two words of the command.  We also need to make sure that when we
* go to run the compiler, we run the one intended by the user.
*
* In particular, for masqueraded mode, we want to make sure that we
* don't invoke distcc recursively.
**/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include "common/distcc.h"
#include "common/arg.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/util.h"
#include "common/hosts.h"
#include "common/bulk.h"
#include "common/compiler.h"

#include "client/client.h"
#include "client/dopt.h"
#include "client/implicit.h"
#include "client/compile.h"

using namespace distcc;

///////////////////////////////////////////////////////////////////////////////////////////////

// Name of this program, for trace.c
const char *rs_program_name = "distcc";

//---------------------------------------------------------------------------------------------

// distcc client entry point.
//
// This is typically called by make in place of the real compiler.
//
// Performs basic setup and checks for distcc arguments, and then kicks off dcc_build_somewhere().

int main(int argc, char *argv[])
{
	int ret;

	try
	{
#ifdef _WIN32
		dcc_win32_startup();
#endif

		Arguments args(argc, argv);
		ClientConfig config;
		if (!config.parse_options(args))
			return 0;

		Client client(config);
		ret = client.run(args);
	}
	catch (const char *x)
	{
		//@ log x
		ret = -1;
	}
	catch (...)
	{
		ret = -1;
	}

	dcc_exit(ret);
}

///////////////////////////////////////////////////////////////////////////////////////////////
