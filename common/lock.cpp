/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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


                /* Power is nothing without control
                 *      -- Pirelli tyre advertisment. */


/**
 * @file
 *
 * @brief Manage lockfiles.
 *
 * distcc uses a simple disk-based lockfile system to keep track of how many
 * jobs are queued on various machines.  These locks might be used for
 * something else in the future.
 *
 * We use locks rather than e.g. a database or a central daemon because we
 * want to make sure that the lock will be removed if the client terminates
 * unexpectedly.  
 *
 * The files themselves (as opposed to the lock on them) are never cleaned up;
 * since locking & creation is nonatomic I can't think of a clean way to do
 * it.  There shouldn't be many of them, and dead ones will be caught by the
 * tmpreaper.  In any case they're zero bytes.
 *
 * Sys V semaphores might work well here, but the interface is a bit ugly and
 * they are probably not portable to Cygwin.  In particular they can leak if
 * the process is abruptly terminated, which is likely to happen to distcc.
 */


#include "config.h"

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

#include <string>

#include <sys/stat.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/hosts.h"
#include "common/lock.h"
#include "common/exitcode.h"

#include "rvfc/text/defs.h"

namespace distcc
{

using namespace rvfc;

///////////////////////////////////////////////////////////////////////////////////////////////

LocalHostDefs local_hostdefs;
dcc_hostdef dcc_hostdef_local = local_hostdefs.host();
Directory lock_dir;

//---------------------------------------------------------------------------------------------
// Returns a newly allocated buffer

Path dcc_hostdef::make_lock_filename(const text &lockname, int iter) const
{
	File file;
	switch (mode)
	{
	case DCC_MODE_LOCAL:
		file = lock_dir/stringf("%s_localhost_%d", +lockname, iter);
		break;

	case DCC_MODE_TCP:
		file = lock_dir/stringf("%s_tcp_%s_%d_%d", +lockname, +hostname, port, iter);
		break;

	case  DCC_MODE_SSH:
		file = lock_dir/stringf("%s_ssh_%s_%d", +lockname, +hostname, iter);
		break;

	default:
		throw "dcc_make_lock_filename failed";
    }

    return file.path();
}

//---------------------------------------------------------------------------------------------

// Get an exclusive, non-blocking lock on a file using whatever method is available on this system.

// @retval 0 if we got the lock
// @retval -1 with errno set if the file is already locked.

static int sys_lock(int fd, int block)
{
#if defined(F_SETLK)
    struct flock lockparam;

    lockparam.l_type = F_WRLCK;
    lockparam.l_whence = SEEK_SET;
    lockparam.l_start = 0;
    lockparam.l_len = 0; // whole file
    
    return fcntl(fd, block ? F_SETLKW : F_SETLK, &lockparam);

#elif defined(HAVE_FLOCK)
    return flock(fd, LOCK_EX | (block ? 0 : LOCK_NB));

#elif defined(HAVE_LOCKF)
    return lockf(fd, block ? F_LOCK : F_TLOCK, 0);

#elif defined(_WIN32)
	return _locking(fd, block ? _LK_LOCK : _LK_NBLCK, LONG_MAX);
#else
#error No supported lock method.  Please port this code.
#endif
}

//---------------------------------------------------------------------------------------------

int dcc_unlock(int lock_fd)
{
    rs_trace("release lock fd%d", lock_fd);
    // All our current locks can just be closed
    if (!close(lock_fd)) 
		return 0;

	rs_log_error("close failed: %s", strerror(errno));
	return EXIT_IO_ERROR;
}

//---------------------------------------------------------------------------------------------

// Open a lockfile, creating if it does not exist.

int dcc_open_lockfile(const string &fname)
{
    /* Create if it doesn't exist.  We don't actually do anything with
     * the file except lock it.
     *
     * The file is created with the loosest permissions allowed by the user's
     * umask, to give the best chance of avoiding problems if they should
     * happen to use a shared lock dir. */
    /* FIXME: If we fail to open with EPERM or something similar, try deleting
     * the file and try again.  That might fix problems with root-owned files
     * in user home directories. */
    int lock_fd = open(fname.c_str(), O_WRONLY|O_CREAT, 0666);
    if (lock_fd == -1 && errno != EEXIST) 
	{
        rs_log_error("failed to creat %s: %s", fname, strerror(errno));
        throw "dcc_open_lockfile failed";
    }

    return lock_fd;
}

//---------------------------------------------------------------------------------------------

/**
 * Lock a server slot, in either blocking or nonblocking mode.
 *
 * In blocking mode, this function will not return until either the lock has
 * been acquired, or an error occured.  In nonblocking mode, it will instead
 * return EXIT_BUSY if some other process has this slot locked.
 *
 * @param slot 0-based index of available slots on this host.
 * @param block True for blocking mode.
 *
 * @param lock_fd On return, contains the lock file descriptor to allow
 * it to be closed.
 **/

int dcc_hostdef::lock(const char *lockname, int slot, int block, int &lock_fd)
{
    int ret;

	string fname = make_lock_filename(lockname, slot);
    lock_fd = dcc_open_lockfile(+fname);
    if (sys_lock(lock_fd, block) == 0) 
	{
        rs_trace("got %s lock on %s slot %d as fd%d", lockname, +hostdef_string, slot, lock_fd);
        return 0;
    }

	fd_t _lock_fd = dcc_fd(lock_fd, 0);

	switch (errno) 
	{
#if defined(EWOULDBLOCK) && EWOULDBLOCK != EAGAIN
	case EWOULDBLOCK:
#endif
	case EAGAIN:
	case EACCES: // HP-UX and Cygwin give this for exclusion
		rs_trace("%s is busy", +fname);
		ret = EXIT_BUSY;
		break;

    default:
        rs_log_error("lock %s failed: %s", +fname, strerror(errno));
        ret = EXIT_IO_ERROR;
        break;
    }

    dcc_close(_lock_fd);
    return ret;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
