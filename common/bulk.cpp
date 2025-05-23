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

                /* "A new contraption to capture a dandelion in one
                 * piece has been put together by the crew."
                 *      -- Boards of Canada, "Geogaddi" */


/**
 * @file
 *
 * Bulk file transfer, used for sending .i, .o files etc.
 *
 * Files are always sent in the standard IO format: stream name,
 * length, bytes.  This implies that we can deliver to a fifo (just
 * keep writing), but we can't send from a fifo, because we wouldn't
 * know how many bytes were coming.
 *
 * @note We don't time transmission of files: because the write returns when
 * they've just been written into the OS buffer, we don't really get
 * meaningful numbers except for files that are very large.
 **/ 

#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef _WIN32
#include <io.h>
#endif

#include "distcc.h"
#include "trace.h"
#include "rpc1.h"
#include "bulk.h"
#include "time.h"
#include "exitcode.h"
#include "timeval.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Open a file for read, and also put its size into @p fsize.
//
// If the file does not exist, then returns 0, but @p ifd is -1 and @p fsize is zero.
// If @p fsize is zero, the caller should not try to read from the file.
//
// This strange behavior for ENOENT is useful because if there is e.g. no output file from 
// the compiler, we don't want to abort, but rather just send nothing. 
// The receiver has the corresponding behaviour of not creating zero-length files.
//
// Using fstat() helps avoid a race condition -- not a security issue, but possibly a failure.
// Shouldn't be very likely though.
//
// The caller is responsible for closing @p ifd.

static int dcc_open_read(const File &file, fd_t &ifd, off_t &fsize)
{
	const char *fname = +file.path();

	ifd.socket = 0;
    ifd.fd = open(fname, O_RDONLY | O_BINARY);
    if (ifd.fd == -1)
	{
        int save_errno = errno;
        if (save_errno == ENOENT) 
		{
            // that's OK, just assume it's empty
            fsize = 0;
            return 0;
        }
		else
		{
            rs_log_error("failed to open %s: %s", fname, strerror(save_errno));
            return EXIT_IO_ERROR;
        }
    }

	struct stat buf;
	if (fstat(ifd.fd, &buf) == -1)
	{
		rs_log_error("fstat %s failed: %s", fname, strerror(errno));
		dcc_close(ifd);
		return EXIT_IO_ERROR;
	}

	fsize = buf.st_size;

    return 0;
}

//---------------------------------------------------------------------------------------------

void dcc_calc_rate(off_t size_out, struct timeval &before, struct timeval &after,
	double &secs, double &rate)
{
    struct timeval delta;

    // FIXME: Protect against division by zero and other floating-point exceptions
    
    timeval_subtract(delta, after, before);

    secs = (double) delta.tv_sec + (double) delta.tv_usec / 1e6;

    if (secs == 0.0)
        rate = 0.0;
    else
        rate = ((double) size_out / secs) / 1024.0;
}

//---------------------------------------------------------------------------------------------

static int dcc_x_file_lzo1x(fd_t out_fd, fd_t in_fd, const char *token, unsigned in_len)
{
    int ret;
    char *out_buf = NULL;
    size_t out_len;

    // As a special case, send 0 as 0
    if (in_len == 0)
	{
        if ((ret = dcc_x_token_int(out_fd, token, 0)))
            goto out;
    }
	else
	{
        if ((ret = dcc_compress_file_lzo1x(in_fd, in_len, &out_buf, &out_len)))
            goto out;

        if ((ret = dcc_x_token_int(out_fd, token, out_len)))
            goto out;

        if ((ret = dcc_writex(out_fd, out_buf, out_len)))
            goto out;
    }

    ret = 0;

out:
    free(out_buf);
    return ret;
}

//---------------------------------------------------------------------------------------------

// Transmit from a local file to the network.  
// Sends TOKEN, LENGTH, BODY, where the length is the appropriate compressed length.
// Does compression if needed.
//
// @param ofd File descriptor for the network connection.
// @param fname Name of the file to send.
// @param token Token for this file, e.g. "DOTO".

int dcc_x_file(fd_t ofd, const File &file, const char *token, enum dcc_compress compression, off_t *f_size_out)
{
	const char *fname = +file.path();
	if (!file)
	{
		rs_trace("send nonexistent file %s with token %s", fname, token);
		if (f_size_out)
			*f_size_out = 0;
		return dcc_x_token_int(ofd, token, (unsigned) -1);
	}

	fd_t ifd;
	off_t f_size;
    if (dcc_open_read(file, ifd, f_size))
        return EXIT_IO_ERROR;

    if (f_size_out)
        *f_size_out = f_size;

    rs_trace("send %lu byte file %s with token %s", (unsigned long) f_size, fname, token);

	int ret;

	if (compression == DCC_COMPRESS_NONE) 
	{
		if ((ret = dcc_x_token_int(ofd, token, f_size)))
			goto failed;

		// FIXME: These could get truncated if the file was very large (>4G).
		// That seems pretty unlikely.
#ifdef HAVE_SENDFILE
		ret = dcc_pump_sendfile(ofd, ifd, (size_t) f_size);
#else
		ret = dcc_pump_readwrite(ofd, ifd, (size_t) f_size);
#endif
    }
	else if (compression == DCC_COMPRESS_LZO1X) 
	{
		ret = dcc_x_file_lzo1x(ofd, ifd, token, f_size);        
	}
	else 
	{
		rs_log_error("invalid compression");
		ret = EXIT_PROTOCOL_ERROR;
		goto failed;
	}

	if (ifd.fd != -1)
		dcc_close(ifd);
	return 0;

failed:
	if (ifd.fd != -1)
		dcc_close(ifd);
	return ret;
}

//---------------------------------------------------------------------------------------------

// Receive a file stream from the network into a local file.  
//
// Can handle compression.
//
// @param len Compressed length of the incoming file.
// @param filename local filename to create.  

int dcc_r_file(fd_t ifd, File &file, unsigned len, enum dcc_compress compr)
{
	// This is meant to behave similarly to the output routines in bfd/cache.c in gnu binutils, 
	// because makefiles or configure scripts may depend on it for edge cases.
	//
	// We try to remove the output file first, if its size is not 0.  
	// That should make the newly created file be owned by the current user; 
	// it might also help in the dangerous case of some other process still reading from the file.
	//
	// Checking for size 0 means that we won't unlink special files like /dev/null or fifos.
	//
	// However, failure to remove the file does not cause a warning; 
	// we may not have write permission on the directory, but +w for the file.

	const char *filename = +file.path();

	struct stat s;
	if (stat(filename, &s) == 0) 
	{
		if (s.st_size != 0) 
		{
			if (!file.remove()) 
			{
				rs_trace("failed to remove %s: %s", filename, strerror(errno));
				// continue
			}
		}
	}
	else 
	{
		if (errno != ENOENT)
			rs_trace("stat %s failed: %s", filename, strerror(errno));
		// continue
	}

	fd_t ofd;
	ofd.socket = 0;
	ofd.fd = open(filename, O_TRUNC|O_WRONLY|O_CREAT|O_BINARY, 0666);
	if (ofd.fd == -1) 
	{
		rs_log_error("failed to create %s: %s", filename, strerror(errno));
		return EXIT_IO_ERROR;
	}

	int ret = 0;
	if (len > 0)
		ret = dcc_r_bulk(ofd, ifd, len, compr);

	int close_ret = dcc_close(ofd);
	if (!ret && !close_ret) 
	{
		rs_trace("received %d bytes to file %s", len, filename);
		return 0;
    }

	rs_trace("failed to receive %s, removing it", filename);
	if (file.remove()) 
		rs_log_error("failed to unlink %s after failed transfer: %s", filename, strerror(errno));

	return EXIT_IO_ERROR;
}

//---------------------------------------------------------------------------------------------

// Receive a file and print timing statistics.  Only used for big files.
//
// Wrapper around dcc_r_file(). 

int dcc_r_file_timed(fd_t ifd, File &fname, unsigned size, enum dcc_compress compr)
{
    struct timeval before, after;

	if (gettimeofday(&before, NULL))
        rs_log_warning("gettimeofday failed");

    int ret = dcc_r_file(ifd, fname, size, compr);

    if (gettimeofday(&after, NULL)) 
	{
        rs_log_warning("gettimeofday failed");
    }
	else 
	{
        double secs, rate;
        dcc_calc_rate(size, before, after, secs, rate);
        rs_log_info("%ld bytes received in %.6fs, rate %.0fkB/s", (long) size, secs, rate);
    }

    return ret;
}

//---------------------------------------------------------------------------------------------

int dcc_r_token_file(fd_t ifd, const char *token, File &file, 
	unsigned &size, enum dcc_compress compr)
{
	int ret = dcc_r_token_int(ifd, token, size);
	if (ret)
		return ret;

	// file is nonexistent and should not be created
	if ((int) size == -1)
		return 0;

	if ((ret = dcc_r_file_timed(ifd, file, (size_t) size, compr)))
		return ret;

	return 0;
}

//---------------------------------------------------------------------------------------------

int dcc_r_token_bulk(fd_t ifd, const char *token, fd_t ofd, enum dcc_compress compr)
{
	unsigned int size;
	int ret = dcc_r_token_int(ifd, token, size);
	if (ret)
		return ret;
	return dcc_r_bulk(ofd, ifd, size, compr);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
