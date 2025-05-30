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

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#endif

#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "distcc.h"
#include "trace.h"
#include "daemon.h"
#include "exitcode.h"

namespace distcc
{

#ifdef __linux__

const char *opt_user = "distcc";


/**
 * @file
 *
 * Functions for setting the daemon's persona.
 *
 * It is better to create separate userids for daemons rather than to just use
 * "nobody".
 *
 * Personas may be specified either as a name or an ID.
 **/

/**
 * Try to find an appropriate uid,gid to change to.
 *
 * In order, we try "distcc" or the user on the command line, or "nobody", or
 * failing that the traditional value for nobody of 65534.
 */
static int dcc_preferred_user(uid_t *puid, gid_t *pgid)
{
    struct passwd *pw;

    if ((pw = getpwnam(opt_user))) {
        *puid = pw->pw_uid;
        *pgid = pw->pw_gid;
        return 0;               /* cool */
    }
    /* Note getpwnam() does not set errno */
    rs_log_warning("no such user as \"%s\"", opt_user);
    /* try something else */

    if ((pw = getpwnam("nobody"))) {
        *puid = pw->pw_uid;
        *pgid = pw->pw_gid;
        return 0;               /* cool */
    }

    /* just use traditional value */
    *puid = *pgid = 65534;
    return 0;
}


/**
 * Make sure that distccd never runs as root, by discarding privileges if we
 * have them.
 *
 * This used to also check gid!=0, but on BSD that is group wheel and is
 * apparently common for daemons or users.
 *
 * This is run before dissociating from the calling terminal so any errors go
 * to stdout.
 **/
int dcc_discard_root(void)
{
    uid_t uid;
    gid_t gid;
    int ret;
    
    if (getuid() != 0  &&  geteuid() != 0) {
        /* Already not root.  No worries. */
        return 0;
    }

    if ((ret = dcc_preferred_user(&uid, &gid)) != 0)
        return ret;

    /* GNU C Library Manual says that when run by root, setgid() and setuid()
     * permanently discard privileges: both the real and effective uid are
     * set. */

    if (setgid(gid)) {
        rs_log_error("setgid(%d) failed: %s", (int) gid, strerror(errno));
        return EXIT_SETUID_FAILED;
    }

#ifdef HAVE_SETGROUPS
    /* Get rid of any supplementary groups this process might have
     * inherited. */
    /* XXX: OS X Jaguar broke setgroups so that setting it to 0 fails. */
    if (setgroups(1, &gid)) {
        rs_log_error("setgroups failed: %s", strerror(errno));
        return EXIT_SETUID_FAILED;
    }
#endif

    if (setuid(uid)) {
        rs_log_error("setuid(%d) failed: %s", (int) uid, strerror(errno));
        return EXIT_SETUID_FAILED;
    }

    if (getuid() == 0  ||  geteuid() == 0) {
        rs_log_crit("still have root privileges after trying to discard them!");
        return EXIT_SETUID_FAILED;
    }

    rs_trace("discarded root privileges, changed to uid=%d gid=%d", (int) uid, (int) gid);
    return 0;
}

#endif // __linux__

} // namespace distcc
