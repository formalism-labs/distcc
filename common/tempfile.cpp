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


                /* "More computing sins are committed in the name of
                 * efficiency (without necessarily achieving it) than
                 * for any other single reason - including blind
                 * stupidity."  -- W.A. Wulf
                 */

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#include <sys/stat.h>
#include <sys/types.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "distcc.h"
#include "trace.h"
#include "util.h"

#include "exitcode.h"

#include "rvfc/text/defs.h"

namespace distcc
{

using namespace rvfc::Text;

/**
 * @file
 *
 * Routines for naming, generating and removing temporary files.
 *
 * Temporary files are stored under $TMPDIR or /tmp.
 *
 * From 2.10 on, our lock and state files are not stored in there.
 *
 * It would be nice if we could use a standard function, but I don't
 * think any of them are adequate: we need to control the extension
 * and know the filename.  tmpfile() does not give back the filename.
 * tmpnam() is insecure.  mkstemp() does not allow us to set the
 * extension.
 *
 * It sucks that there is no standard function.  The implementation
 * below is inspired by the __gen_tempname() code in glibc; hopefully
 * it will be secure on all platforms.
 *
 * We need to touch the filename before running commands on it,
 * because we cannot be sure that the compiler will create it
 * securely.
 *
 * Even with all this, we are not necessarily secure in the presence
 * of a tmpreaper if the attacker can play timing tricks.  However,
 * since we are not setuid and since there is no completely safe way
 * to write tmpreapers, this is left alone for now.
 *
 * If you're really paranoid, you should just use per-user TMPDIRs.
 *
 * @sa http://www.dwheeler.com/secure-programs/Secure-Programs-HOWTO/avoid-race.html#TEMPORARY-FILES
 **/

///////////////////////////////////////////////////////////////////////////////////////////////

string dcc_get_tmp_top()
{
	const char *dirs[] =
	{
		"DISTCC_TMPDIR",
#if defined(__linux__)
		"TMPDIR",
#elif defined(_WIN32)
		"TEMP",
		"TMP"
#endif
	};

	for (int i = 0; i < sizeof(dirs)/sizeof(*dirs); ++i)
	{
		const char *d = getenv(dirs[i]);
		if (d && d[0] != '\0')
			return d;
	}
#ifdef _WIN32
	return "c:/windows/temp";
#else
	return "/tmp";
#endif
}

//---------------------------------------------------------------------------------------------
// Create the directory @p path.  If it already exists as a directory we succeed.

void dcc_mkdir(const string &path)
{
	const char *p = path.c_str();
#ifdef __linux__
    if (mkdir(p, 0777) == -1 && errno != EEXIST) 
#elif defined(_WIN32)
    if (_mkdir(p) == -1 && errno != EEXIST) 
#else
#error Please port this
#endif
	{
        rs_log_error("mkdir %s failed: %s", p, strerror(errno));
        throw "dcc_mkdir failed";
    }
}

//---------------------------------------------------------------------------------------------

// Create a file inside the temporary directory and register it for later cleanup, and return its name.
// The file will be reopened later, possibly in a child. 
// But we know that it exists with appropriately tight permissions.

File dcc_make_tmpnam(const char *prefix, const char *suffix)
{
    string tempdir = dcc_get_tmp_top();

#ifdef __linux__
    if (access(+tempdir, W_OK|X_OK) == -1) 
#elif defined(_WIN32)
	if (_access(+tempdir, W_OK) == -1) 
#else
#error Please port this
#endif
	{
        rs_log_error("cannot use TMPDIR \"%s\": %s", +tempdir, strerror(errno));
		throw "dcc_make_tmpnam: cannot use TMPDIR";
    }

    unsigned long random_bits;
#ifdef _WIN32
    random_bits = (unsigned long) GetCurrentThreadId() << 16;
#elif defined(__linux__)
    random_bits = (unsigned long) getpid() << 16;
#else
#error Please port this
#endif

# if HAVE_GETTIMEOFDAY
    {
        struct timeval tv;
        gettimeofday(&tv, NULL);
//        random_bits ^= tv.tv_usec << 16;
	    unsigned long r1 = 0;
		r1 ^= tv.tv_usec % 0xffff;
        r1 ^= tv.tv_sec % 0xffff;
		random_bits |= r1 & 0xffff;
    }
# else
    random_bits ^= time(NULL);
# endif

#if 0
	random_bits = 0; // FOR TESTING
#endif

    string s;
    for (;;)
	{
		s = stringf("%s/%s_%08lx%s", +tempdir, prefix, random_bits & 0xffffffffUL, suffix);

        // Note that if the name already exists as a symlink, this open call will fail.
		// The permissions are tight because nobody but this process and our children should do anything with it.
        int fd = open(s.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (fd == -1) 
		{
#ifdef _WIN32
			if (errno == EMFILE || errno == ENFILE)
			{
				//@@ workaround for critical condition: too many open files
				int n = 0;
				n = n/n; // results in core dump and service restart
				ExitProcess(-1); // safety
			}
#endif

            // try again
            rs_trace("failed to create %s: %s", +s, strerror(errno));
            random_bits += 7777; // fairly prime
            continue;
        }

        if (close(fd) == -1)
		{
            rs_log_warning("failed to close %s: %s", +s, strerror(errno));
			throw "dcc_make_tmpnam: error closing file (strange!)";
        }
        
        break;
    }

	dcc_add_cleanup(s);
	return s;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
