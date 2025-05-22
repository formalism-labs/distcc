/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
 *
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2006 Tom Aratyn (themystic.ca@gmail.com) & 
 *                    Cesar Oliveira (cesar.d.o@vif.com)
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
          * And suddenly there came a sound from heaven 
          * as of a rushing mighty wind, and it filled 
          * all the house where they were sitting. And 
          * there appeared unto them cloven tongues 
          * like as of fire, and it sat upon each of them.
          * And they were all filled with the Holy Ghost,
          * and began to speak with other tongues, as the
          * Spirit gave them utterance.
          *                    -- Acts 2:2 - 2:4
          */

#include "config.h"

#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "common/distcc.h"
#include "common/compiler.h"
#include "common/hosts.h"
#include "common/exitcode.h"
#include "common/trace.h"
#include "common/util.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

MscCompiler msc_compiler;
DiabCompiler diab_compiler;
GccCompiler gcc_compiler;
extern Compiler *dcc_compiler = 0;

///////////////////////////////////////////////////////////////////////////////////////////////

bool cmp_compiler_name(const string &p, const string &name)
{
	return p == name || p == name + ".exe" || p == name + ".exe\"";
}

//---------------------------------------------------------------------------------------------

void dcc_set_compiler (const Arguments &args, int first_arg)
{
    // The second parameter should be the compiler name or a file or an option.
	// If its a file or an option, than we default to gcc anyways

	string name = Path(args[first_arg]).basename();
        
    // rs_log_info("the string is: %s %s (:. %s)  %s", argv[0], argv[1], name, argv[2]);

	if (cmp_compiler_name(name, "cl"))
	{
		rs_log_info("Using msc settings.");
		dcc_compiler = &msc_compiler;
    }
    else if (cmp_compiler_name(name, "dcc") || cmp_compiler_name(name, "dplus"))
	{
		rs_log_info("Using diab settings.");
		dcc_compiler = &diab_compiler;
	}
	else
	{
		// Assume masquerade mode. 
		// The only compiler allowed to do this is gcc to maintain backwards compatibility.

		rs_log_info("Using gcc settings.");
		dcc_compiler = &gcc_compiler;
    }
}

//---------------------------------------------------------------------------------------------

int fix_dotd_file(const File &temp_d_fname, const File &temp_o_fname)
{
	char line[1024];
	int ret = 0;

	FILE *d_file = temp_d_fname.update();
	if (!d_file)
		return 0;

	int i;
	fpos_t pos;
	for (i = 0; i < 5 && !fgetpos(d_file, &pos) && fgets(line, sizeof(line), d_file); ++i)
	{
		char *p = strstr(line, +temp_o_fname.path());
		if (!p)
			continue;

		memset(p, ' ', temp_o_fname.path().length());
		if (fsetpos(d_file, &pos))
			goto end;

		if (fputs(line, d_file) < 0)
			goto end;

		ret = 1;
		break;
	}

end:
	fclose(d_file);
	return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
