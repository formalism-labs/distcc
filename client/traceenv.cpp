/* -*- c-file-style: "java"; indent-tabs-mode: nil -*-
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


#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>

#elif defined(_WIN32)
#include "syslog/syslog.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "common/distcc.h"
#include "common/trace.h"
#include "common/exitcode.h"
#include "common/util.h"

#include "client/client.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

// Setup client error/trace output.
//
// Trace goes to the file specified by DISTCC_LOG, if any.
// Otherwise, it goes to stderr, except that UNCACHED_ERR_FD can redirect it elsewhere, 
// for use under ccache.
//
// The exact setting of log level is a little strange, but for a good reason: 
// if you ask for verbose, you get everything. 
// Otherwise, if you set a file, you get INFO and above.
// Otherwise, you only get WARNING messages.
// In practice this seems to be a nice balance.

void Client::configure_trace_level()
{
    const char *logfile, *logfd_name;
    int fd;
	rs_loglevel log_level = RS_LOG__INVALID;

    if ((logfile = getenv("DISTCC_LOG")) && logfile[0]) 
	{
		log_level = RS_LOG_INFO;

        fd = open(logfile, O_WRONLY|O_APPEND|O_CREAT, 0666);
        if (fd == -1) 
		{
            // use stderr instead
            int save_errno = errno;
            
            log_level = RS_LOG_WARNING;
            rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, STDERR_FILENO);

            rs_log_error("failed to open logfile %s: %s", logfile, strerror(save_errno));
        }
		else
		{
			rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, fd);
			log_level = RS_LOG_INFO;
        }
    }
	else
	{
		if ((logfd_name = getenv("UNCACHED_ERR_FD")) == NULL || (fd = atoi(logfd_name)) == 0) 
			fd = STDERR_FILENO;
            
		log_level = RS_LOG_WARNING;
		rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, fd);
    }

	if (config.arg_log_level != RS_LOG__INVALID)
		rs_trace_set_level(config.arg_log_level);
	else if (log_level != RS_LOG__INVALID)
		rs_trace_set_level(log_level);

	if (config.verbose || dcc_getenv_bool("DISTCC_VERBOSE", 0))
		rs_trace_set_level(RS_LOG_DEBUG);

    if (config.log_stderr) 
	{
        rs_remove_all_loggers();
        rs_add_logger(rs_logger_file, RS_LOG_DEBUG, 0, STDERR_FILENO);
    }

	if (config.log_syslog)
	{
		openlog("distcc", LOG_PID, LOG_DAEMON);
		rs_add_logger(rs_logger_syslog, RS_LOG_DEBUG, NULL, 0);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
