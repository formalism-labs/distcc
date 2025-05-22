/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003, 2004 by Martin Pool
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


/* pump.c - Transfer of bulk data (source, object code) */


#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/stat.h>
#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
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

char pump_rw_buf[262144];

//---------------------------------------------------------------------------------------------

// Receive either compressed or uncompressed bulk data

int dcc_r_bulk(fd_t ofd, fd_t ifd, unsigned f_size, enum dcc_compress compression)
{
    if (f_size == 0)
        return 0; // don't decompress nothing
    
    if (compression == DCC_COMPRESS_NONE) 
        return dcc_pump_readwrite(ofd, ifd, f_size);

	if (compression == DCC_COMPRESS_LZO1X)
        return dcc_r_bulk_lzo1x(ofd, ifd, f_size);

	rs_log_error("impossible compression %d", compression);
	return EXIT_PROTOCOL_ERROR;
}

//---------------------------------------------------------------------------------------------

/**
 * Copy @p n bytes from @p ifd to @p ofd.
 *
 * Does not use sendfile(), so either one may be a socket.
 *
 * In the current code at least one of the files will always be a regular
 * (disk) file, even though it may not be mmapable.  That should mean that
 * writes to it will always complete immediately.  That in turn means that on
 * each pass through the main loop we ought to either completely fill our
 * buffer, or completely drain it, depending on which one is the disk.
 *
 * In future we may put back the ability to feed the compiler from a fifo, in
 * which case it may be that the writes don't complete.
 *
 * We might try selecting on both buffers and handling whichever is ready.
 * This would require some approximation to a circular buffer though, which
 * might be more complex.
 **/

int dcc_pump_readwrite(fd_t ofd_, fd_t ifd_, size_t n)
{
	char *buf = pump_rw_buf;
    char *p;
    ssize_t r_in, r_out, wanted;
    int ret;
	int ofd = ofd_.fd, ifd = ifd_.fd;

    while (n > 0) 
	{
        wanted = n > sizeof(pump_rw_buf) ? sizeof(pump_rw_buf) : n;

		if (ifd_.socket)
		{
			r_in = recv(ifd, buf, (size_t) wanted, 0);
#ifdef _WIN32
			if (r_in == -1)
				translate_wsaerror();
#endif
		}
		else
			r_in = read(ifd, buf, (size_t) wanted);

		if (r_in == -1 && errno == EAGAIN) 
		{
			if ((ret = dcc_select_for_read(ifd_, dcc_io_timeout)) != 0)
				return ret;
			continue;
        }
		else if (r_in == -1 && errno == EINTR)
		{
            continue;
        }
		else if (r_in == -1)
		{
            rs_log_error("failed to read %ld bytes: %s", (long) wanted, socket_error_str());
            return EXIT_IO_ERROR;
        }
		else if (r_in == 0) 
		{
            rs_log_error("unexpected eof on fd%d", ifd);
            return EXIT_IO_ERROR;
        }

        n -= r_in;
        p = buf;

		// We now have r_in bytes waiting to go out, starting at p.
		// Keep going until they're all written out.

        while (r_in > 0) 
		{
			if (ofd_.socket)
			{
				r_out = send(ofd, p, (size_t) r_in, 0);
#ifdef _WIN32
				if (r_out == -1)
					translate_wsaerror();
#endif
			}
			else
				r_out = write(ofd, p, (size_t) r_in);

            if (r_out == -1 && errno == EAGAIN) 
			{
                if ((ret = dcc_select_for_write(ofd_, dcc_io_timeout)) != 0)
                    return ret;
				continue;
            }
			else if (r_out == -1 && errno == EINTR) 
			{
                continue;
            }
			else if (r_out == -1  ||  r_out == 0) 
			{
                rs_log_error("failed to write: %s", socket_error_str());
                return EXIT_IO_ERROR;
            }
            r_in -= r_out;
            p += r_out;
        }
    }

    return 0;
}
        
///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

