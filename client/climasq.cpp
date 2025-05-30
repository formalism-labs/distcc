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


                         /* 
                          * And the magicians did so with their enchantments,
                          * and brought up frogs upon the land of Egypt.
                          *            --  Exodus 8:7
                          */



#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
//#include <io.h>

#include "common/distcc.h"
#include "common/util.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/arg.h"

#include "client/client.h"

#include "rvfc/filesys/defs.h"

namespace distcc
{

using rvfc::Path;

///////////////////////////////////////////////////////////////////////////////////////////////

/**
 * For masquerade mode, change the path to remove the directory containing the
 * distcc mask, so that invoking the same name will find the underlying
 * compiler instead.
 *
 * @param progname basename under which distcc was introduced.  If we reached
 * this point, then it's the same as the name of the real compiler, e.g. "cc".
 *
 * @param did_masquerade specifies an integer that will be set to 1 if the
 * path was changed.
 *
 * @return 0 or standard error.
 **/
int Client::support_masquerade(const Arguments &args, const string &progname, int &did_masquerade)
{
	//@ currently disabled
#if 0
    const char *envpath, *findpath, *p, *n;
    char *buf;
    size_t len;
    size_t findlen;

    if (!(envpath = getenv("PATH")))
		// strange but true
        return 0;

	buf = (char *) malloc(strlen(envpath) + 1 + strlen(progname) + 1);
	if (!buf) 
	{
        rs_log_error("failed to allocate buffer for new PATH");
        return EXIT_OUT_OF_MEMORY;
    }

	// Filter PATH to contain only the part that is past our dir.
	// If we were called explicitly, find the named dir on the PATH.
    if (progname != args[0]) 
	{
        findpath = Path(args[0]).real();
        findlen = strlen(findpath);
    }
	else 
	{
        findpath = NULL;
        findlen = 0;
    }

    for (n = p = envpath; *n; p = n) 
	{
        // Find the length of this component of the path
        n = strchr(p, ':');
        if (n)
            len = n++ - p;
        else 
		{
            len = strlen(p);
            n = p + len;
        }

        if (findpath) 
		{
            // Looking for a component in the path equal to findpath

			// FIXME: This won't catch paths that are in fact the same, but
			// that are not the same string.  This might happen if you have
			// multiple slashes, or dots, or symlinks.
			if (len != findlen || strncmp(p, findpath, findlen) != 0)
				continue;
        }
		else 
		{
            // Looking for a component in the path containing a file progname

            // FIXME: This gets a false match if you have a subdirectory that
            // happens to be of the right name, e.g. /usr/bin/distcc.
            strncpy(buf, p, (size_t) len);
            sprintf(buf + len, "/%s", progname);
#ifdef __linux__
            if (access(buf, X_OK) != 0)
                continue;
#endif
        }
        // Set p to the part of the path past our match
        p = n;
        break;
    }

    if (*p != '\0') 
	{
        int ret = dcc_set_path(p);
        if (ret)
            return ret;
        *did_masquerade = 1;
    }
    else 
	{
        rs_trace("not modifying PATH");
        *did_masquerade = 0;
    }

    free(buf);
#endif // 0

    return 0;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
