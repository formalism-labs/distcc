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
 * Run the preprocessor.  Client-side only.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/distcc.h"
#include "common/arg.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/util.h"
#include "common/exec.h"
#include "common/compiler.h"

#include "client/client.h"
#include "client/implicit.h"
#include "client/dopt.h"

#include "rvfc/filesys/defs.h"

namespace distcc
{

using namespace rvfc::Text;
using rvfc::File;
using rvfc::Path;

///////////////////////////////////////////////////////////////////////////////////////////////

// If the input filename is a plain source file rather than a preprocessed source file, 
// then preprocess it to a temporary file and return the name in @p cpp_fname.
//
// The preprocessor may still be running when we return; you have to wait for @p cpp_fid to 
// exit before the output is complete.
// This allows us to overlap opening the TCP socket, which probably doesn't use many cycles, 
// with running the preprocessor.

int Client::cpp_maybe(Arguments &args, File &cpp_fname, proc_t &cpp_pid)
{
	if (config.on_server)
	{
		rs_trace("requested server mode: will not preprocess");
		return 0;
	}

	if (!dcc_compiler)
		return EXIT_NO_COMPILER_SETTING;

    if (dcc_compiler->is_preprocessed(args.input_file))
	{
        // TODO: Perhaps also consider the option that says not to use cpp. Would anyone do that?
        rs_trace("input is already preprocessed");
		cpp_fname = args.input_file;
        return 0;
    }

    string input_exten = dcc_find_extension(args.input_file);
    string output_exten = dcc_compiler->preproc_exten(input_exten);

	cpp_fname = dcc_make_tmpnam("distcc", +output_exten);

	// We strip the -o option and allow cpp to write to stdout, which is caught in a file.  
	// Sun cc doesn't understand -E -o, and gcc screws up -MD -E -o.
	//
	// There is still a problem here with -MD -E -o, gcc writes dependencies to a file 
	// determined by the source filename.  We could fix it by generating a -MF option, 
	// but that would break compilation with older versions of gcc.  
	// This is only a problem for people who have the source and objects in different 
	// directories, and who don't specify -MF.  They can fix it by specifying -MF.

	Arguments cpp_args = args;
	int ret;
	dcc_compiler->strip_dasho(cpp_args);
	if ((ret = dcc_compiler->set_action_opt(cpp_args, dcc_compiler->preproc)))
		return ret;

	cpp_pid = 0;
	File null(DEV_NULL);
	return dcc_spawn_child(cpp_args, cpp_pid, 0, &null, &cpp_fname, 0);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
