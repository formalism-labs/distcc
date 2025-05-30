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

                    /*
                     * Tell me what you need, and I'll tell you how to 
                     * get along without it.                -- Dilbert
                     */

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "distcc.h"
#include "trace.h"

#include "version.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

int dcc_trace_version(void)
{
    rs_trace("%s %s %s; built %s %s",
		rs_program_name, PACKAGE_VERSION, GNU_HOST, __DATE__, __TIME__);
    return 0;
}

//---------------------------------------------------------------------------------------------

int dcc_show_version(const char *prog)
{
	// The "built" message is the time this file was built, which may not be
	// completely accurate for the program as a whole unless you do "make clean".
	// Message looks like the one from "gcc --version".
    printf(
"%s %s %s (protocols 1 and 2) (default port %d)\n"
"  built %s %s\n"
"Copyright (C) 2002, 2003, 2004 by Martin Pool.\n"
"Includes miniLZO (C) 1996-2002 by Markus Franz Xaver Johannes Oberhumer.\n"
"\n"
"distcc comes with ABSOLUTELY NO WARRANTY.  distcc is free software, and\n"
"you may use, modify and redistribute it under the terms of the GNU \n"
"General Public License version 2 or later.\n"
"\n",
	prog, PACKAGE_VERSION, GNU_HOST, DISTCC_DEFAULT_PORT, __DATE__, __TIME__);
    return 0;
}	

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
