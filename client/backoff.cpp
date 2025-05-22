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
 * Keep track of hosts which are, or are not, usable.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/file.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#include <sys/stat.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/exitcode.h"
#include "common/lock.h"
#include "common/hosts.h"

#include "client/timefile.h"

#include "rvfc/text/defs.h"

namespace distcc
{

using namespace rvfc;

///////////////////////////////////////////////////////////////////////////////////////////////

const int dcc_backoff_period = 60; // seconds

//---------------------------------------------------------------------------------------------
// Remember that this host is working OK.
// For the moment this just means removing any backoff timer scored against it.

void dcc_hostdef::enjoyed_host()
{
    remove_timefile("backoff");
}

//---------------------------------------------------------------------------------------------

void dcc_hostdef::disliked_host()
{
	return; //@
    // i hate you (but only for 60 seconds)
    mark_timefile("backoff");
}


//---------------------------------------------------------------------------------------------

int dcc_hostdef::check_backoff() const
{
    int ret;
    time_t mtime;

    if ((ret = check_timefile("backoff", mtime)))
        return ret;

    if (difftime(time(NULL), mtime) < (double) dcc_backoff_period) 
	{
        rs_trace("still in backoff period for %s", +hostdef_string);
        return EXIT_BUSY;
    }

    return 0;
}

//---------------------------------------------------------------------------------------------
// Walk through @p hostlist and remove any hosts that are marked unavailable
 
void HostDefs::remove_disliked()
{
	for (HostsList::iterator i = _hosts.begin(); i != _hosts.end();)
	{
        if (i->check_backoff() != 0) 
		{
            rs_trace("remove %s from list", +i->hostdef_string);
			_hosts.erase(i);
        }
		else
			++i;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
