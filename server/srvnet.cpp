/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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

                /* "Happy is the man who finds wisdom, and the man who
                 * gets understanding; for the gain from it is better
                 * than gain from silver and its profit better than
                 * gold." -- Proverbs 3:13 */


/**
 * @file
 *
 * Server-side networking.
 **/

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <netdb.h>

#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>

#ifdef HAVE_ARPA_NAMESER_H
#  include <arpa/nameser.h>
#endif

#include <arpa/inet.h>

#ifdef HAVE_RESOLV_H
#include <resolv.h>
#endif

#include <netinet/in.h>
#include <netinet/tcp.h>

#endif // __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
//#include <io.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <list>

#include "common/types.h"
#include "common/exitcode.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/netutil.h"

#include "server/srvnet.h"
#include "server/access.h"
#include "server/dopt.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Listen on a predetermined address (often the passive address).  
// The way in which we get the address depends on the resolver API in use.

static int dcc_listen_by_addr(int fd, struct sockaddr *sa, size_t salen)
{
    int one = 1;
    string sa_buf;

    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(one));

    dcc_sockaddr_to_string(sa, salen, sa_buf);

    // now we've got a socket - we need to bind it
    if (bind(fd, sa, salen) == -1) 
	{
#ifdef _WIN32
		translate_wsaerror();
#endif
		rs_log_error("bind of %s failed: %s", !!sa_buf ? +sa_buf : "UNKNOWN", socket_error_str());
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		return EXIT_BIND_FAILED;
    }

    rs_log_info("listening on %s", !!sa_buf ? +sa_buf : "UNKNOWN");

    if (listen(fd, 100)) 
	{
#ifdef _WIN32
		translate_wsaerror();
#endif
        rs_log_error("listen failed: %s", socket_error_str());
#ifdef _WIN32
		closesocket(fd);
#else
		close(fd);
#endif
        return EXIT_BIND_FAILED;
    }

    return 0;
}

//---------------------------------------------------------------------------------------------

#if defined(ENABLE_RFC2553)

// This version uses getaddrinfo.  
// It will probably use IPv6 if that's supported by your configuration, kernel, and library.

int dcc_socket_listen(int port, int *fd_out, const char *listen_addr)
{
    char portname[20];
    struct addrinfo hints;
    struct addrinfo *res, *ai;
    int error;
    int ret;

    // getaddrinfo() ought to check for this, but some versions do not (Debian Bug#192876.)
    if (port < 1 || port > 65535) 
	{
        rs_log_error("port number out of range: %d", port);
        return EXIT_BAD_ARGUMENTS;
    }

    // getaddrinfo wants a string for the service name
    snprintf(portname, sizeof portname, "%d", port);

    // Set-up hints structure
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;

    if (listen_addr == NULL)
        hints.ai_flags = AI_PASSIVE; // bind all

    error = getaddrinfo(listen_addr, portname, &hints, &res);
    
    if (error) 
	{
        rs_log_error("getaddrinfo failed for host %s service %s: %s",
			listen_addr ? listen_addr : "(passive)", portname, gai_strerror(error));
        return EXIT_BIND_FAILED;
    }

    // The first sockaddr returned will typically be an IPv6 socket.
	// Some kernels might not support that.
    for (ai = res; ai; ai=ai->ai_next) 
	{
        int af = ai->ai_addr->sa_family;
        if ((*fd_out = socket(af, SOCK_STREAM, 0)) == -1) 
		{
#ifdef _WIN32
			translate_wsaerror();
#endif

            if (errno == EAFNOSUPPORT) 
			{
                rs_log_notice("socket address family %d not supported", af);
                continue;
            }

			rs_log_error("socket creation failed: %s", socket_error_str());
			return EXIT_BIND_FAILED;
        }

		ret = dcc_listen_by_addr(*fd_out, res->ai_addr, res->ai_addrlen);
        freeaddrinfo(res);
        return ret;
    }

    rs_log_error("failed to find any supported socket family");
    return EXIT_BIND_FAILED;
}

//---------------------------------------------------------------------------------------------
#else // ! ENABLE_RFC2553

// This version uses inet_aton
int dcc_socket_listen(int port, int *listen_fd, const char *listen_addr)
{
    struct sockaddr_in sock;

    if (port < 1 || port > 65535) 
	{
        // htons() will truncate, not check
        rs_log_error("port number out of range: %d", port);
        return EXIT_BAD_ARGUMENTS;
    }

    memset((char *) &sock, 0, sizeof(sock));
    sock.sin_port = htons(port);
    sock.sin_family = PF_INET;

    if (listen_addr) 
	{
        if (!inet_aton(listen_addr, &sock.sin_addr)) 
		{
            rs_log_error("listen address \"%s\" is not a valid IPv4 address", listen_addr);
            return EXIT_BAD_ARGUMENTS;
        }
    } 
	else 
	{
        sock.sin_addr.s_addr = INADDR_ANY;
    }

    if ((*listen_fd = socket(PF_INET, SOCK_STREAM, 0)) == -1) 
	{
		rs_log_error("socket creation failed: %s", strerror(errno));
		return EXIT_BIND_FAILED;
    }

    return dcc_listen_by_addr(*listen_fd, (struct sockaddr *) &sock, sizeof sock);
}
#endif  // ! ENABLE_RFC2553

//---------------------------------------------------------------------------------------------
// Determine if a file descriptor is in fact a socket

int is_a_socket(int fd)
{
    int v, l = sizeof(int);
    return getsockopt(fd, SOL_SOCKET, SO_TYPE, (char *) &v, (socklen_t *) &l) == 0;
}

//---------------------------------------------------------------------------------------------

// Log client IP address and perform access control checks.
// Note that PSA may be NULL if the sockaddr is unknown.

int dcc_check_client(struct sockaddr *psa, int salen, dcc_allow_list &allowed_list)
{
	string client_ip;
	bool allowed = false;
	int ret;
    
    if ((ret = dcc_sockaddr_to_string(psa, salen, client_ip)) != 0)
        return ret;
    
    rs_log_info("connection from %s", +client_ip);

    if (!psa)
        // if no sockaddr, must be a pipe or something
        return 0;

    if (!allowed)
        // if no ACL, default open
        return 0;
    
	for (dcc_allow_list::const_iterator l = allowed_list.begin(); l != allowed_list.end(); ++l)
	{
        if (psa->sa_family == AF_INET) 
		{
            in_addr_t cli_inaddr;
            cli_inaddr = ((struct sockaddr_in *) psa)->sin_addr.s_addr;
                
            if (l->allowed(cli_inaddr))
			{
				allowed = true;
                break;
			}
        }
#ifdef ENABLE_RFC2553
		else if (psa->sa_family == AF_INET6) 
		{
            const struct sockaddr_in6 *sa6 = (const struct sockaddr_in6 *) psa;
            const struct in6_addr *a6 = &sa6->sin6_addr;
            const in_addr_t *a4;
            
            if (IN6_IS_ADDR_V4MAPPED(a6) || IN6_IS_ADDR_V4COMPAT(a6)) 
			{
                a4 = (const in_addr_t *) &a6->s6_addr[12];
                if ((ret = dcc_check_address(*a4, l->addr, l->mask)) == 0)
                    break;
            }
			else 
			{
                rs_log_notice("ipv6 ACLs not implemented");
                return EXIT_ACCESS_DENIED;
            }
        }
#endif // ENABLE_RFC2553
		else 
		{
            allowed = false;
            rs_log_notice("access denied from unknown address family %d", psa->sa_family);
            break;
        }
    }

    if (!allowed)
        rs_log_error("connection denied by access list");

	return allowed;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
