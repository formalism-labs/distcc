/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2002, 2003, 2004 by Martin Pool
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


                        /* I just wish I could get caller-IQ on my phones...
                                   -- The Purple People-Eater, NANAE */


#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#ifdef _WIN32
#include <ws2tcpip.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "types.h"
#include "distcc.h"
#include "trace.h"
#include "exitcode.h"
#include "clinet.h"
#include "util.h"
#include "netutil.h"

#include "rvfc/text/defs.h"

namespace distcc
{

using std::string;
using namespace rvfc::Text;

#ifndef h_errno
extern int h_errno;
#endif


const int dcc_connect_timeout = 4; /* seconds */

/*
 * Client-side networking.
 *
 * These are called with an alarm set so we get a single timeout over the
 * whole resolution and connection process.
 *
 * TODO: In error messages, show the name of the relevant host.
 * Should do this even in readx(), etc.
 *
 * TODO: After connecting, perhaps try to read 0 bytes to see if there's an
 * error.
 */


/*
 * Connect to a host given its binary address, with a timeout.
 * 
 * host and port are only here to aid printing debug messages.
 */
static int
dcc_connect_by_addr(struct sockaddr *sa, size_t salen, fd_t &p_fd)
{
    int ret;
    int rc;
	int tries;

    string s;
    dcc_sockaddr_to_string(sa, salen, s);

    rs_trace("started connecting to %s", +s);

    int fd;
    if ((fd = socket(sa->sa_family, SOCK_STREAM, 0)) == -1) 
	{
#if _WIN32
		translate_wsaerror();
#endif
        rs_log_error("failed to create socket: %s", socket_error_str());
        ret = EXIT_CONNECT_FAILED;
        goto out_failed;
    }

    dcc_set_nonblocking(fd);

    // start the nonblocking connect...

#ifdef _WIN32
	tries = 10;
	do
	{
		rc = connect(fd, sa, salen);
		if (rc == 0)
			break;
		if (rc == SOCKET_ERROR && WSAGetLastError() == WSAEINTR)
			continue;
		rs_trace("retrying connect %s (%d)", +s, tries);
		Sleep(1000L);
		--tries;
	}
	while (tries > 0);
#else
	do
        rc = connect(fd, sa, salen);
    while (rc == -1 && errno == EINTR);
#endif

#ifdef _WIN32
	if (rc == -1)
		translate_wsaerror();
	if (rc == -1 && WSAGetLastError() != WSAEINPROGRESS) 
#else
    if (rc == -1 && errno != EINPROGRESS) 
#endif
	{
        rs_log(RS_LOG_ERR|RS_LOG_NONAME, "failed to connect to %s: %s", +s, socket_error_str());
        ret = EXIT_CONNECT_FAILED;
        goto out_failed;
    }

	fd_t _fd = dcc_fd(fd, 1);
    if ((ret = dcc_select_for_write(_fd, dcc_connect_timeout))) 
	{
        rs_log(RS_LOG_ERR|RS_LOG_NONAME, "timeout while connecting to %s", +s);
        goto out_failed;
    }

    p_fd = _fd;
    return 0;
    
out_failed:
    return ret;
}


#if defined(ENABLE_RFC2553)

/**
 * Open a socket to a tcp remote host with the specified port.
 **/
int dcc_connect_by_name(const string &host, int port, fd_t *p_fd)
{
    struct addrinfo hints;
    struct addrinfo *res;
    int error;
    int ret;
    char portname[20];

    rs_trace("connecting to %s port %d", host.c_str(), port);
    
    // Unfortunately for us, getaddrinfo wants the port (service) as a string
    snprintf(portname, sizeof portname, "%d", port);

    memset(&hints, 0, sizeof(hints));
    // set-up hints structure
    hints.ai_family = PF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    error = getaddrinfo(host.c_str(), portname, &hints, &res);
    if (error) 
	{
        rs_log_error("failed to resolve host %s port %d: %s", host.c_str(), port, gai_strerror(error));
        return EXIT_CONNECT_FAILED;
    }

    // Try each of the hosts possible addresses
    do
	{
        ret = dcc_connect_by_addr(res->ai_addr, res->ai_addrlen, p_fd);
    }
	while (ret != 0 && (res = res->ai_next));

    return ret;
}


#else // not ENABLE_RFC2553

/**
 * Open a socket to a tcp remote host with the specified port.
 *
 * @todo Don't try for too long to connect. 
 **/
int dcc_connect_by_name(const string &host, int port, fd_t &p_fd)
{
#ifdef _WIN32
	struct addrinfo hints, *info;
	memset(&hints, 0, sizeof(hints));
//	hints.ai_flags = AI_NUMERICHOST;
	int rc = getaddrinfo(host.c_str(), stringf("%d", port).c_str(), &hints, &info);
	if (rc)
	{
		rs_log_error("failed to look up host \"%s\": %s", host,  gai_strerror(rc));
		return EXIT_CONNECT_FAILED;
	}
	struct sockaddr_in sock_out;
	memcpy(&sock_out, info->ai_addr, sizeof(sock_out));
	freeaddrinfo(info);

	return dcc_connect_by_addr((struct sockaddr *) &sock_out, sizeof(sock_out), p_fd);
#else

    struct sockaddr_in sock_out;
    struct hostent *hp;

    // FIXME: "warning: gethostbyname() leaks memory.
	// Use gethostbyname_r instead!" (or indeed perhaps use getaddrinfo?)
	hp = gethostbyname(host);
	if (!hp) 
	{
#ifdef __linux__
		rs_log_error("failed to look up host \"%s\": %s", host.c_str(), hstrerror(h_errno));
#else
		rs_log_error("failed to look up host \"%s\"", host.c_str());
#endif
		return EXIT_CONNECT_FAILED;
    }

    memcpy(&sock_out.sin_addr, hp->h_addr, (size_t) hp->h_length);
#ifdef __linux__
    sock_out.sin_port = htons((in_port_t) port);
#else
	sock_out.sin_port = htons((u_short) port);
#endif
    sock_out.sin_family = PF_INET;

    return dcc_connect_by_addr((struct sockaddr *) &sock_out, sizeof sock_out, p_fd);
#endif
}

#endif // not ENABLE_RFC2553

} // namespace distcc
