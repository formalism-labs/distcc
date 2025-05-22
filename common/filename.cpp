/* -*- c-file-style: "java"; indent-tabs-mode: nil -*- 
 *
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool
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

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#endif
#include <string.h>
#include <errno.h>
#include <string>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"



/**
 * @file
 *
 * Everything we know about C filenames.
 *
 * We need to have some heuristics about input and output filenames to
 * understand command lines, because that's what cc does.
 *
 * @note As of 0.10, .s and .S files are never distributed, because
 * they might contain '.include' pseudo-operations, which are resolved
 * by the assembler.
 */

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Return a pointer to the extension, including the dot, or NULL

string dcc_find_extension(const Path &file)
{
	return file.ext();
}

//---------------------------------------------------------------------------------------------

string dcc_find_filename_stem(const Path &fname)
{
	if (!fname)
		return fname;
	const char *p = +fname;
	const char *dot = strrchr(p, '.');

	// filename without ext
	if (!dot || dot[1] == '\0') 
        return fname;
	// .name
	if (dot == p)
		return fname;

    return string(p, dot - p);
}

//---------------------------------------------------------------------------------------------

// Return a pointer to the basename of the file (everything after the last slash.)  
// If there is no slash, return the whole filename, which is presumably in the current directory.

string dcc_find_basename(const Path &file)
{
	return file.basename();
}

//---------------------------------------------------------------------------------------------

static int dcc_set_file_extension(const string &sfile, const string &new_ext, string &ofile)
{
	string stem = dcc_find_filename_stem(sfile);
	if (stem == sfile)
	{
        rs_log_error("couldn't find extension in \"%s\"", +sfile);
        return EXIT_DISTCC_FAILED;
    }

	ofile = stem + new_ext;
    rs_log_info("Changed extension from %s to %s", +sfile, +ofile);
	return 0;
}

//---------------------------------------------------------------------------------------------

// Work out the default object file name the compiler would use if -o was not specified.  
// We don't need to worry about "a.out" because we've already determined that -c or -S was specified.
//
// However, the compiler does put the output file in the current directory even if the source 
// file is elsewhere, so we need to strip off all leading directories.
//
// @param sfile Source filename.  Assumed to match one of the recognized patterns, otherwise bad things might happen.

int dcc_output_from_source(const string &fname, const string &out_extn, string &ofile)
{
    const char *slash, *sfile = +fname;
    
    if ((slash = strrchr(sfile, '/')))
        sfile = slash+1;
    if (strlen(sfile) < 3) 
	{
        rs_log_error("source file %s is bogus", sfile);
        return EXIT_DISTCC_FAILED;
    }

    return dcc_set_file_extension(sfile, out_extn, ofile);
}


///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
