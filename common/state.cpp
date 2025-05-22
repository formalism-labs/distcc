/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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
 * This file provides a way for distcc processes to make little notes
 * about what they're up to that can be read by a monitor process.
 *
 * State is stored as follows.
 *
 * Within our temporary directory, we create a subdirectory called "state".
 *
 * Each process creates a file named "binstate%d", for its pid.  We
 * always rewrite this file from the beginning.
 *
 * Inside each of these, we store a binary struct in the native host
 * encoding.  Ugly, but quick and easy both in code and CPU time.
 *
 * Any process reading these files needs to handle the fact that they may be
 * truncated or otherwise incorrect.
 *
 * When the process exits, it removes its state file.  If you didn't
 * notice it already, it's too late now.
 *
 * In addition, if the process identified by the file no longer
 * exists, then the file must be orphaned by a process that suddenly
 * terminated.  The file is ignored and can be deleted by the first
 * process that notices it.
 *
 * The reader interface for these files is in mon.c
 *
 * These files are considered a private format, and they may change
 * between distcc releases.  The only supported way to read them is
 * through mon.c.
 **/

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "types.h"
#include "distcc.h"
#include "rpc1.h"
#include "trace.h"
#include "exitcode.h"

#include "util.h"

#include <exception>

#include "rvfc/filesys/defs.h"

namespace distcc
{

using namespace rvfc;

///////////////////////////////////////////////////////////////////////////////////////////////

const char *dcc_state_prefix = "binstate_";
Directory dcc_state_dir;

#ifdef _WIN32
__declspec (thread)
#endif
struct dcc_task_state my_state;

//---------------------------------------------------------------------------------------------
// Return newly allocated buffer holding the name of this process's state file.
// (This can't reliably be static because we might fork...)

static Path dcc_get_state_filename()
{
	return stringf("%s/%s%ld", +dcc_state_dir.path(), dcc_state_prefix, (long) getpid());
}

//---------------------------------------------------------------------------------------------

const char *dcc_get_phase_name(enum dcc_phase phase)
{
    switch (phase) 
	{
    case DCC_PHASE_STARTUP:
        return "Startup";
    case DCC_PHASE_BLOCKED:
        return "Blocked";
    case DCC_PHASE_COMPILE:
        return "Compile";
    case DCC_PHASE_CPP:
        return "Preprocess";
    case DCC_PHASE_CONNECT:
        return "Connect";
    case DCC_PHASE_SEND:
        return "Send";
    case DCC_PHASE_RECEIVE:
        return "Receive";
    case DCC_PHASE_DONE:
        return "Done";
    default:
        return "Unknown";
    }
}

//---------------------------------------------------------------------------------------------
// Get a file descriptor for writing to this process's state file

class StateFile
{
	fd_t _fd;

public:
	StateFile(const string &fname)
	{
		_fd.socket = 0;
		_fd.fd = open(+fname, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, 0666);
		if (_fd.fd == -1)
		{
			rs_log_error("failed to open %s: %s", fname, strerror(errno));
			throw "failed to open state file";
		}
	}

	~StateFile()
	{
		dcc_close(_fd);
	}

	fd_t fd() const { return _fd; }
};

//---------------------------------------------------------------------------------------------

static int dcc_open_state(fd_t &fd, const char *fname)
{
	fd.socket = 0;
	fd.fd = open(fname, O_CREAT|O_WRONLY|O_TRUNC|O_BINARY, 0666);
	if (fd.fd == -1)
	{
		rs_log_error("failed to open %s: %s", fname, strerror(errno));
		return EXIT_IO_ERROR;
	}

	return 0;
}

//---------------------------------------------------------------------------------------------

// Remove the state file for this process.
// This can be called from atexit().

void dcc_remove_state_file()
{
    string fname = dcc_get_state_filename();
	if (unlink(+fname) == -1) 
	{
        // It's OK if we never created it
        if (errno != ENOENT) 
		{
            rs_log_warning("failed to unlink %s: %s", fname, strerror(errno));
            // ret = EXIT_IO_ERROR;
        }
    }
}

//---------------------------------------------------------------------------------------------

static int dcc_write_state(fd_t fd)
{
    // Write out as one big blob.  fd is positioned at the start of the file.

    int ret = dcc_writex(fd, &my_state, sizeof my_state);
    if (ret)
        return ret;

    return 0;
}

//---------------------------------------------------------------------------------------------

// Record the state of this process.
// The filename is trimmed down to its basename.
// If the source_file or host are NULL, then are left unchanged from their previous value.

int dcc_note_state(enum dcc_phase state, const File &source_file, const string &host)
{
    int ret = 0;

    my_state.struct_size = sizeof(my_state);
    my_state.magic = DCC_STATE_MAGIC;
    my_state.cpid = (unsigned long) getpid();

    Path fname = dcc_get_state_filename();

    Path source_base = source_file.path().basename();
    if (!!source_base)
        strlcpy(my_state.file, +source_base, sizeof(my_state.file));

    if (!host.empty())
        strlcpy(my_state.host, +host, sizeof(my_state.host));

    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) 
	{
        rs_log_error("gettimeofday failed: %s", strerror(errno));
		// throw "dcc_note_state: gettimeofday failed";
		return -1;
    }
    my_state.curr_phase = state;

    rs_trace("note state %d, file \"%s\", host \"%s\"", state, +source_base, +host);

	try
	{
		StateFile state_file(fname);
		return dcc_write_state(state_file.fd());
	}
	catch (std::exception &x)
	{
		return -1;
	}

    return 0;
}

//---------------------------------------------------------------------------------------------

int dcc_note_state(enum dcc_phase state)
{
    int ret = 0;

    my_state.struct_size = sizeof(my_state);
    my_state.magic = DCC_STATE_MAGIC;
    my_state.cpid = (unsigned long) getpid();

    Path fname = dcc_get_state_filename();

    struct timeval tv;
    if (gettimeofday(&tv, NULL) == -1) 
	{
        rs_log_error("gettimeofday failed: %s", strerror(errno));
		// throw "dcc_note_state: gettimeofday failed";
		return -1;
    }
    my_state.curr_phase = state;

    rs_trace("note state %d", state);

	try
	{
		StateFile state_file(fname);
		return dcc_write_state(state_file.fd());
	}
	catch (std::exception &x)
	{
		return -1;
	}

    return 0;
}

//---------------------------------------------------------------------------------------------

void dcc_note_state_slot(int slot)
{
    my_state.slot = slot;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
