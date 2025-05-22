/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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


/**
 * @file
 *
 * Common low-level IO utilities.
 *
 * This code is not meant to know about our protocol, only to provide
 * a more comfortable layer on top of Unix IO.
 *
 * @todo Perhaps write things out using writev() to reduce the number
 * of system calls, and the risk of small packets when not using
 * TCP_CORK.
 */

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#endif

#include <sys/types.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/exitcode.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef _WIN32
static __thread char wsa_error[256];
#endif

//---------------------------------------------------------------------------------------------

const char *socket_error_str()
{
#ifdef _WIN32
	char *message;
	int err = WSAGetLastError();
	FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &message, 0, NULL);
	if (message)
	{
		int m = strlen(message);
		if (m > 1 && (message[m-2] == '\r' || message[m-2] == '\n'))
			message[m-2]='\0'; 
		strncpy(wsa_error, message, sizeof(wsa_error));
		wsa_error[sizeof(wsa_error)-1] = '\0';
		LocalFree(message);
		return wsa_error;
	}
	else
		return "unknown error";

#else
	return strerror(errno);
#endif
}

//---------------------------------------------------------------------------------------------

// Timeout for all IO other than opening connections.
// Much longer, because compiling files can take a long time.

const int dcc_io_timeout = 300; // seconds

//---------------------------------------------------------------------------------------------

fd_t dcc_fd(int fd, int sock)
{
	fd_t x;
	x.fd = fd;
	x.socket = !!sock;
	return x;
}

//---------------------------------------------------------------------------------------------
// @todo Perhaps only apply the timeout for initial connections, not when doing regular IO.

dcc_exitcode dcc_select_for_read(fd_t fd_, int timeout)
{
    fd_set fds;
    int rs;
    struct timeval tv;
	int fd;

	fd = fd_.fd;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    for (;;) 
	{
        FD_ZERO(&fds);
        FD_SET(fd, &fds);

        // Linux updates the timeval to reflect the remaining time, but other OSs may not.  
		// So on other systems, we may wait a bit too long if the client is 
		// interrupted -- but that won't happen very often so it's no big deal.

        rs_trace("select for read on fd%d for %ds", fd, (int) tv.tv_sec);
        rs = select(fd+1, &fds, NULL, NULL, &tv);
#ifdef _WIN32
		if (rs == -1)
			translate_wsaerror();
#endif
        if (rs == -1 && errno == EINTR) 
		{
            rs_trace("select was interrupted");
            continue;
        }
		else if (rs == -1) 
		{
            rs_log_error("select() failed: %s", socket_error_str());
            return EXIT_IO_ERROR;
        }
		else if (rs == 0) 
		{
            rs_log_error("IO timeout");
            return EXIT_IO_ERROR;
        }
		else if (!FD_ISSET(fd, &fds)) 
		{
            rs_log_error("how did fd not get set?");
            continue;
        }
		else 
		{
            break; // woot
        }
    }
    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_select_for_write(fd_t fd_, int timeout)
{
    fd_set fds;
    int rs;
	int fd;

    struct timeval tv;

	fd = fd_.fd;

    tv.tv_sec = timeout;
    tv.tv_usec = 0;

    for (;;)
	{
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        rs_trace("select for write on fd%d", fd);
        
        rs = select(fd + 1, NULL, &fds, &fds, &tv);
#ifdef _WIN32
		if (rs == -1)
			translate_wsaerror();
#endif

        if (rs == -1 && errno == EINTR) 
		{
            rs_trace("select was interrupted");
            continue;
        }
		else if (rs == -1) 
		{
            rs_log_error("select failed: %s", socket_error_str());
            return EXIT_IO_ERROR;
        }
		else 
		{
            return EXIT_OK;
        }
    }
}

//---------------------------------------------------------------------------------------------
// Read exactly @p len bytes from a file.

dcc_exitcode dcc_readx(fd_t fd_, void *buf, size_t len)
{
    ssize_t r;
    dcc_exitcode ret;
	int fd = fd_.fd;
	int len0 = len;

    while (len > 0) 
	{
		if (fd_.socket)
		{
			r = recv(fd, (char *) buf, len, 0);
#ifdef _WIN32
			if (r == -1)
				translate_wsaerror();
#endif
		}
		else
			r = read(fd, buf, len);

        if (r == -1 && errno == EAGAIN) 
		{
            if ((ret = dcc_select_for_read(fd_, dcc_io_timeout)))
                return ret;
            else
                continue;
        }
		else if (r == -1 && errno == EAGAIN) 
		{
            continue;
        }
		else if (r == -1) 
		{
			rs_log_error("failed to read: %s", socket_error_str());
			return EXIT_IO_ERROR;
		}
		else if (r == 0) 
		{
			rs_log_error("unexpected eof on fd%d (received=%d)", fd, len0 - len);
			return EXIT_TRUNCATED;
		} 
		else 
		{
			buf = &((char *) buf)[r];
			len -= r;
		}
	}

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------
// Write bytes to an fd.  Keep writing until we're all done or something goes wrong.
// @returns 0 or exit code.

dcc_exitcode dcc_writex(fd_t fd_, const void *buf, size_t len)
{
    ssize_t r;
    dcc_exitcode ret;
	int fd = fd_.fd;
	
    while (len > 0) 
	{
		if (fd_.socket)
		{
			r = send(fd, (const char *) buf, len, 0);
#ifdef _WIN32
			if (r == -1)
				translate_wsaerror();
#endif
		}
		else
			r = write(fd, buf, len);

        if (r == -1 && errno == EAGAIN) 
		{
            if ((ret = dcc_select_for_write(fd_, dcc_io_timeout)))
                return ret;
            else
                continue;
        }
		else if (r == -1 && errno == EINTR) 
		{
            continue;
        }
		else if (r == -1) 
		{
			rs_log_error("failed to write: %s", socket_error_str());
            return EXIT_IO_ERROR;
        }
		else if (r == 0) 
		{
            rs_log_error("unexpected eof on fd%d", fd);
            return EXIT_TRUNCATED;
        }
		else 
		{
            buf = &((char *) buf)[r];
            len -= r;
        }
    }

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------
// Stick a TCP cork in the socket.  It's not clear that this will help performance, but it might.
// This is a no-op if we don't think this platform has corks.

int tcp_cork_sock(fd_t fd, int corked)
{
#ifdef TCP_CORK 
	if (!dcc_getenv_bool("DISTCC_TCP_CORK", 1) || !fd.socket)
		return 0;
    
	if (setsockopt(fd.fd, SOL_TCP, TCP_CORK, &corked, sizeof(corked)) == -1) 
	{
		if (errno == ENOSYS || errno == ENOTSUP) 
		{
			if (corked)
				rs_trace("no corks allowed on fd%d", fd);
			// no need to complain about not uncorking
		} 
		else 
		{
			rs_log_warning("setsockopt(corked=%d) failed: %s", corked, strerror(errno));
			// continue anyhow
		}
	}
#endif // TCP_CORK
	return 0;
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_close(fd_t fd_)
{
	int rc, fd;
	fd = fd_.fd;

	if (fd_.socket)
	{
#ifdef _WIN32
		DWORD t0 = GetTickCount();

		rs_trace("shutting down socket fd%d", fd);
		shutdown(fd, SD_SEND);
		char buf[64];
		while (recv(fd, buf, sizeof(buf), 0) > 0)
			;

		rs_trace("closed socket fd%d (time=%d ms)", fd, GetTickCount() - t0);
		rc = closesocket(fd);
#else
		rs_trace("closed socket fd%d", fd);
		rc = close(fd);
#endif // ! _WIN32
	}
	else
		rc = close(fd);

	if (rc != 0)
	{
        rs_log_error("failed to close fd%d: %s", fd, socket_error_str());
        return EXIT_IO_ERROR;
    }
    return EXIT_OK;
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
