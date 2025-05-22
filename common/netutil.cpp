/* -*- c-file-style: "java"; indent-tabs-mode: nil;  -*-
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

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <netdb.h>
#include <netdb.h>

#include <sys/wait.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/param.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#include <arpa/inet.h>

#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif
#endif // __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/types.h>

#include "common/types.h"
#include "common/exitcode.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/netutil.h"
#include "common/snprintf.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

/* work out what fcntl flag to use for non-blocking */
#ifdef O_NONBLOCK
# define NONBLOCK_FLAG O_NONBLOCK
#elif defined(SYSV)
# define NONBLOCK_FLAG O_NDELAY
#else
# define NONBLOCK_FLAG FNDELAY
#endif

#ifndef AF_UNIX
#  define AF_UNIX AF_LOCAL
#endif


#ifndef HAVE_HSTRERROR
/* Missing on e.g. Solaris 2.6 */
const char *hstrerror(int err) {
    switch (err) {
    case HOST_NOT_FOUND:
        return "Host not found";
    case TRY_AGAIN:
        return "Name server not contacted";
    case NO_RECOVERY:
        return "Non-recoverable error";
    case NO_ADDRESS:
        return "No IP address for host";
    default:
        return "Unknown error";
    }
}
#endif


/**
 * Set a fd into blocking mode
 **/
void dcc_set_blocking(int fd)
{
#ifdef __linux__
    int val = fcntl(fd, F_GETFL, 0);
    if (val == -1)
        return;

    if (val & NONBLOCK_FLAG) 
	{
        val &= ~NONBLOCK_FLAG;
        fcntl(fd, F_SETFL, val);
    }
#endif // __linux__
}


/**
 * Set a fd into nonblocking mode
 **/
void dcc_set_nonblocking(int fd)
{
#ifdef __linux__
    int val = fcntl(fd, F_GETFL, 0);
    if (val == -1)
        return;

    if (!(val & NONBLOCK_FLAG)) 
	{
        val |= NONBLOCK_FLAG;
        fcntl(fd, F_SETFL, val);
    }
#endif // __linux__
}


/* Ask for the server not to be awakened until some data has arrived
 * on the socket.  This works for our protocol because the client
 * sends a request immediately after connection without waiting for
 * anything from the server. */
void dcc_defer_accept(int listen_fd)
{
#ifdef TCP_DEFER_ACCEPT
    int val = 1;
    
    if (!dcc_getenv_bool("DISTCC_TCP_DEFER_ACCEPT", 1)) 
	{
        rs_trace("TCP_DEFER_ACCEPT disabled");
        return;
    }
    
    if (setsockopt(listen_fd, SOL_TCP, TCP_DEFER_ACCEPT, &val, sizeof val) == -1) 
	{
        rs_log_warning("failed to set TCP_DEFER_ACCEPT: %s", strerror(errno));
    }
	else
	{
        rs_trace("TCP_DEFER_ACCEPT turned on");
    }
#endif 
}


#ifdef ENABLE_RFC2553

// TODO: Make the returned strings consistent with the other implementation

int dcc_sockaddr_to_string(struct sockaddr *sa, size_t salen, string &p_buf)
{
    int err;
    char host[1024];
    char port[32];

    if (!sa) 
	{
        p_buf = "NOTSOCKET";
        return 0;
    }
	if (sa->sa_family == AF_INET || sa->sa_family == AF_INET6) 
	{
        err = getnameinfo(sa, salen, host, sizeof host, port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV);
        if (err) 
		{
            rs_log_warning("getnameinfo failed: %s", gai_strerror(err));
            p_buf = "(UNKNOWN)";
            return 0;               // it's still a valid string
        }
        
        p_buf = stringf("%s:%s", host, port);
    }
	else if (sa->sa_family == AF_UNIX) 
	{
        // NB: The word 'sun' is predefined on Solaris
        struct sockaddr_un *sa_un = (struct sockaddr_un *) sa;
        p_buf = stringf("UNIX-DOMAIN %s", sa_un->sun_path);
    }
	else
	{
        p_buf = stringf("UNKNOWN-FAMILY %d", sa->sa_family);
    }

    return 0;
}

#else // ! ENABLE_RFC2553

int dcc_sockaddr_to_string(struct sockaddr *sa, size_t UNUSED(salen), string &p_buf)
{
    if (!sa) 
	{
        p_buf = "NOTSOCKET";
        return 0;
    }
	if (sa->sa_family == AF_INET) 
	{
		struct sockaddr_in *sain = (struct sockaddr_in *) sa;
		p_buf = stringf("%s:%d", inet_ntoa(sain->sin_addr), ntohs(sain->sin_port));
    }
#ifdef __linux__
	else if (sa->sa_family == AF_UNIX) 
	{
        // NB: The word 'sun' is predefined on Solaris
        struct sockaddr_un *sa_un = (struct sockaddr_un *) sa;
        p_buf = stringf("UNIX-DOMAIN %s", sa_un->sun_path);
    }
#endif // __linux__
	else 
	{
        p_buf = stringf("UNKNOWN-FAMILY %d", sa->sa_family);
    }

    return 0;
}

#endif // ! ENABLE_RFC2553

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace discc
