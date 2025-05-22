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


			/* 15 Every one that is found shall be thrust
			 * through; and every one that is joined unto
			 * them shall fall by the sword.
			 *		-- Isaiah 13 */


/**
 * @file
 *
 * Very simple RPC-like layer.  Requests and responses are build of
 * little packets each containing a 4-byte ascii token, an 8-byte hex
 * value or length, and optionally data corresponding to the length.
 *
 * 'x' means transmit, and 'r' means receive. 
 *
 * This builds on top of io.c and is called by the various routines
 * that handle communication.
 **/


#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/socket.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <ctype.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <string>

#include <sys/stat.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/rpc1.h"
// 

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Transmit token name (4 characters) and value (32-bit int, as 8 hex characters).

dcc_exitcode dcc_x_token_int(fd_t ofd, const char *token, unsigned param)
{
    char buf[13];
    int shift;
    char *p;
    static const char *hex = "0123456789abcdef";

    if (strlen(token) != 4) 
	{
        rs_log_crit("token \"%s\" seems wrong", token);
        return EXIT_PROTOCOL_ERROR;
    }
    memcpy(buf, token, 4);

    // Quick and dirty int->hex.  The only standard way is to call snprintf (?), which is 
	// undesirably slow for such a frequently-called function.
    for (shift=28, p = &buf[4]; shift >= 0; shift -= 4, p++) 
	{
        *p = hex[(param >> shift) & 0xf];
    }
    buf[12] = '\0';

	if (strncmp("ARGV", buf, 4))
		rs_trace("send %s", buf);
    return dcc_writex(ofd, buf, 12);
}

//---------------------------------------------------------------------------------------------
// Send start of a result: DONE <version>

dcc_exitcode dcc_x_result_header(fd_t ofd, enum dcc_protover protover)
{
    return dcc_x_token_int(ofd, "DONE", protover);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_x_cc_status(fd_t ofd, int status)
{
    return dcc_x_token_int(ofd, "STAT", (unsigned) status);
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_token(fd_t ifd, char *buf)
{
    return dcc_readx(ifd, buf, 4);
}

//---------------------------------------------------------------------------------------------

// We got a mismatch on a token, which indicates either a bug in distcc, or that somebody 
// (inetd?) is interfering with our network stream, or perhaps some other network problem.  
// Whatever's happened, a bit more debugging information would be handy.

static int dcc_explain_mismatch(const char *buf, size_t buflen, fd_t ifd)
{
    ssize_t ret;
    char extrabuf[200];
    char *p;
    size_t l;

    memcpy(extrabuf, buf, buflen);
    
    // Read a bit more context, and find the printable prefix
	if (ifd.socket)
		ret = recv(ifd.fd, extrabuf + buflen, sizeof(extrabuf) - 1 - buflen, 0);
	else
		ret = read(ifd.fd, extrabuf + buflen, sizeof(extrabuf) - 1 - buflen);
    if (ret == -1) 
        ret = 0; // pah, use what we've got

    l = buflen + ret;

    extrabuf[l] = '\0';
    for (p = extrabuf; *p; p++)
        if (!(isprint(*p) || *p == ' ' || *p == '\t')) 
		{
            *p = '\0';
            break;
        }
    
    rs_log_error("error context: \"%s\"", extrabuf);

    return 0; // i just feel really sad...
}

//---------------------------------------------------------------------------------------------

// Read a token and value.
// The receiver always knows what token name is expected next -- indeed the names are 
// really only there as a sanity check and to aid debugging.
//
// @param ifd      fd to read from
// @param expected 4-char token that is expected to come in next
// @param val      receives the parameter value

dcc_exitcode dcc_r_token_int(fd_t ifd, const char *expected, unsigned &val)
{
    char buf[13], *bum;
    dcc_exitcode ret;
    
    if (strlen(expected) != 4) 
	{
        rs_log_error("expected token \"%s\" seems wrong", expected);
        return EXIT_PROTOCOL_ERROR;
    }

    if ((ret = dcc_readx(ifd, buf, 12))) 
	{
        rs_log_error("read failed while waiting for token \"%s\"", expected);
        return ret;
    }
    
    if (memcmp(buf, expected, 4)) 
	{
        rs_log_error("protocol derailment: expected token \"%s\"", expected);
        dcc_explain_mismatch(buf, 12, ifd);
        return EXIT_PROTOCOL_ERROR;
    }

    buf[12] = '\0'; // terminate

    val = strtoul(&buf[4], &bum, 16);
    if (bum != &buf[12]) 
	{
        rs_log_error("failed to parse parameter of token \"%s\"",  expected);
        dcc_explain_mismatch(buf, 12, ifd);
        return EXIT_PROTOCOL_ERROR;
    }

    //@ rs_trace("got %s", buf);

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

unsigned int dcc_r_token_int(fd_t ifd, const char *expected)
{
    char buf[13], *bum;
    dcc_exitcode ret;
    
    if (strlen(expected) != 4) 
	{
        rs_log_error("expected token \"%s\" seems wrong", expected);
        throw "protocol error";
    }

    if ((ret = dcc_readx(ifd, buf, 12))) 
	{
        rs_log_error("read failed while waiting for token \"%s\"", expected);
        throw "dcc_readx failed";
    }
    
    if (memcmp(buf, expected, 4)) 
	{
        rs_log_error("protocol derailment: expected token \"%s\"", expected);
        dcc_explain_mismatch(buf, 12, ifd);
        throw "protocol error";
    }

    buf[12] = '\0'; // terminate

    unsigned int val = strtoul(&buf[4], &bum, 16);
    if (bum != &buf[12]) 
	{
        rs_log_error("failed to parse parameter of token \"%s\"",  expected);
        dcc_explain_mismatch(buf, 12, ifd);
        throw "protocol error";
    }

    //@ rs_trace("got %s", buf);

    return val;
}

//---------------------------------------------------------------------------------------------
// Read a byte string of length @p l into a newly allocated buffer, returned in @p buf.

dcc_exitcode dcc_r_str_alloc(fd_t fd, unsigned l, string &buf)
{
	// rs_trace("read %d byte string", l);

	 buf.resize(l + 1);
     char *s = (char *) +buf;
     if (dcc_readx(fd, s, (size_t) l))
          return EXIT_OUT_OF_MEMORY;
     s[l] = 0;

     return EXIT_OK;
}

//---------------------------------------------------------------------------------------------
// Read a byte string of length @p l into a newly allocated buffer, returned in @p buf.

string dcc_r_str(fd_t fd, unsigned l)
{
	// rs_trace("read %d byte string", l);

	char *p = new char[l + 1];
	if (dcc_readx(fd, p, (size_t) l))
	  throw "dcc_readx failed";

	p[l] = '\0';
	string s = p;
	delete[] p;

	return s;
}

//---------------------------------------------------------------------------------------------

// Write a token, and then the string @p buf.
// The length of buf is determined by its nul delimiter, but the \0 is not sent.

dcc_exitcode dcc_x_token_string(fd_t fd, const char *token, const string &buf)
{
    dcc_exitcode ret;

    size_t len = buf.length();
    if ((ret = dcc_x_token_int(fd, token, (unsigned) len)))
        return ret;
    if ((ret = dcc_writex(fd, +buf, len)))
        return ret;

    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

dcc_exitcode dcc_r_token_string(fd_t ifd, const char *expect_token, string &str)
{
    unsigned a_len;
    dcc_exitcode ret;
        
    if ((ret = dcc_r_token_int(ifd, expect_token, a_len)))
        return ret;

    if ((ret = dcc_r_str_alloc(ifd, a_len, str)))
        return ret;
    
    return EXIT_OK;
}

//---------------------------------------------------------------------------------------------

std::string dcc_r_token_string(fd_t ifd, const char *expect_token)
{
    unsigned int len = dcc_r_token_int(ifd, expect_token);
	return dcc_r_str(ifd, len);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
