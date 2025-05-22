/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Martin Pool <mbp@samba.org>
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
 * @brief Track timeouts by setting the mtime of a file.
 *
 * distcc needs to set timeouts to backoff from unreachable hosts.  As a very
 * simple and robust way of keeping track of this, we simply touch a file in
 * our state directory, whenever we fail to connect.  Future invocations can
 * look at how recently the host failed when deciding whether to use it again.
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
#ifdef _WIN32
#include <io.h>
#endif

#include <sys/stat.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"

#include "lock.h"
#include "timefile.h"
#include "hosts.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Record the current time against the specified function and host.
 **/
int dcc_hostdef::mark_timefile(const string &lockname)
{
	Path filename = make_lock_filename(lockname, 0);
	fd_t fd = dcc_fd(dcc_open_lockfile(filename), 0);

    // Merely opening it with O_WRONLY is not necessarily enough to set its mtime to the current time
    if (write(fd.fd, "x", 1) != 1) 
	{
        rs_log_error("write to %s failed: %s", +lockname, strerror(errno));
        dcc_close(fd);
        return EXIT_IO_ERROR;
    }

    dcc_close(fd);

    rs_trace("mark %s", filename);

    return 0;
}

//---------------------------------------------------------------------------------------------

/**
 * Remove the specified timestamp.
 **/
void dcc_hostdef::remove_timefile(const string &lockname)
{
    File file = make_lock_filename(lockname, 0);
	if (!file.remove())
		rs_log_error("unlink %s failed: %s", +file.path(), strerror(errno));
	else
		rs_trace("removed %s", +file.path());
}

//---------------------------------------------------------------------------------------------

/**
 * Return the mtime for a timestamp file.
 *
 * If the timestamp doesn't exist then we count it as time zero.
 **/
int dcc_hostdef::check_timefile(const string &lockname, time_t &mtime) const
{
    Path filename = make_lock_filename(lockname, 0);

    struct stat sb;
    if (stat(+filename, &sb) == -1) 
	{
        mtime = (time_t) 0;
        if (errno == ENOENT) 
		{
            // just no record for this file; that's fine.
            return 0;
        }
		else
		{
            rs_log_error("stat %s failed: %s", filename, strerror(errno));
            return EXIT_IO_ERROR;
        }
    }

    mtime = sb.st_mtime;

    return 0;
}    

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
