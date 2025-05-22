
#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>

#include <sys/wait.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <netinet/in.h>
#include <netinet/tcp.h>

#ifdef HAVE_ARPA_NAMESER_H
#include <arpa/nameser.h>
#endif

#include <arpa/inet.h>

#elif defined(_WIN32)
#include "syslog/syslog.h"
#endif // _WIN32

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#ifdef _WIN32
#include <io.h>
#endif
#include <time.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "exitcode.h"
#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "dopt.h"
#include "srvnet.h"
#include "daemon.h"
#include "types.h"
#include "compiler.h"
#include "version.h"

namespace distcc
{

//---------------------------------------------------------------------------------------------

// Errors during startup (e.g. bad options) need to be reported somewhere,
// although we have not yet parsed the options to work out where the user wants them.
//
// In inetd mode, we can't write to stderr because that will corrupt the stream, 
// so if it looks like stderr is a socket we go to syslog instead.

Log::Log()
{
    rs_trace_set_level(RS_LOG_INFO);
    if (!is_a_socket(STDERR_FILENO)) 
	{
		rs_add_logger(rs_logger_file, RS_LOG_DEBUG, 0, STDERR_FILENO);
    }
	// openlog("distccd", LOG_PID, LOG_DAEMON);
	// rs_add_logger(rs_logger_syslog, RS_LOG_INFO, NULL, 0);
}

//---------------------------------------------------------------------------------------------
// Set log to the final destination after options have been read

void Log::setupRealLog()
{
    // Even in inetd mode, we might want to log to stderr, because that will work OK for ssh connections
    
    if (opt_log_stderr) 
	{
        rs_remove_all_loggers();
        rs_add_logger(rs_logger_file, RS_LOG_DEBUG, 0, STDERR_FILENO);
        return;
    }
    
    if (arg_log_file)
	{
        // Don't remove loggers yet, in case this fails and needs to go to the default
		int fd = open(arg_log_file, O_CREAT|O_APPEND|O_WRONLY, 0666);
        if (fd == -1) 
		{
            rs_log_error("failed to open %s: %s", arg_log_file, strerror(errno));
            // continue and use syslog
        }
		else 
		{
            rs_remove_all_loggers();
            rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, fd);
            return;
        }
    }
    
	rs_remove_all_loggers();
	openlog("distccd", LOG_PID, LOG_DAEMON);
	rs_add_logger(rs_logger_syslog, RS_LOG_DEBUG, NULL, 0);
}

//---------------------------------------------------------------------------------------------

int dcc_log_daemon_started(const char *role)
{
    rs_log_info("%s started (%s %s, built %s %s)",
		role, PACKAGE_VERSION, GNU_HOST,  __DATE__, __TIME__);

    return 0;
}


} // namespace distcc
