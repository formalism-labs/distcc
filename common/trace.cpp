/*= -*- c-basic-offset: 4; indent-tabs-mode: nil; -*-
 *
 * ecolog - Reusable application logging library.
 *
 * Copyright (C) 2000 - 2003 by Martin Pool <mbp@samba.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

                                     /*
                                      | Finality is death.
                                      | Perfection is finality.
                                      | Nothing is perfect.
                                      | There are lumps in it.
                                      */


#include "config.h"

#ifdef __linux__
#include <unistd.h>
#include <syslog.h>
#include <sys/file.h>

#elif defined(_WIN32)
#include "syslog/syslog.h"
#endif

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif

#include "trace.h"


///////////////////////////////////////////////////////////////////////////////////////////////

struct rs_logger_list 
{
    rs_logger_fn			*fn;
    void					*private_ptr;
    int						private_int;
    int						max_level;
    struct rs_logger_list	*next;
};

static struct rs_logger_list *logger_list = NULL;

int rs_trace_level = RS_LOG_INFO;

#ifdef UNUSED
// nothing
#elif defined(__GNUC__)
#define UNUSED(x) x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#else
#define UNUSED(x) x
#endif

static void rs_log_va(int level, char const *file, int line, char const *fmt, va_list va);

int __thread log_context = 0;

rvfc::AtomicUINT32 serial_log_context = 0;

#if SIZEOF_SIZE_T > SIZEOF_LONG
#  warning size_t is larger than a long integer, values in trace messages may be wrong
#endif

// Log severity strings, if any.  Must match ordering in ::rs_loglevel.

static const char *rs_severities[] = 
{
    "EMERGENCY! ", "ALERT! ", "CRITICAL! ", "ERROR: ", "Warning: ", "Notice: ", "", ""
};

//---------------------------------------------------------------------------------------------
// Functions for manipulating the list of loggers
//---------------------------------------------------------------------------------------------

void rs_remove_all_loggers(void)
{
    struct rs_logger_list *l, *next;

	LOCK(log);
    for (l = logger_list; l; l = next) 
	{
        next = l -> next; // save before destruction
        free(l);
    }
    logger_list = NULL;
	UNLOCK(log);
}

//---------------------------------------------------------------------------------------------

void rs_add_logger(rs_logger_fn fn, int max_level, void *private_ptr, int private_int)
{
    struct rs_logger_list *l = (struct rs_logger_list *) malloc(sizeof(*l));
    if (l == NULL)
        return;

    l->fn = fn;
    l->max_level = max_level;
    l->private_ptr = private_ptr;
    l->private_int = private_int;

	LOCK(log);
    l->next = logger_list;
    logger_list = l;
	UNLOCK(log);
}

//---------------------------------------------------------------------------------------------
// Remove only the logger that exactly matches the specified parameters

void rs_remove_logger(rs_logger_fn fn, int max_level, void *private_ptr, int private_int)
{
    struct rs_logger_list *l, **pl;

	LOCK(log);
    for (pl = &logger_list; *pl; pl = &((*pl)->next)) 
	{
        l = *pl;
        if (l->fn == fn
            && l->max_level == max_level
            && l->private_ptr == private_ptr
            && l->private_int == private_int) 
		{
			// unhook from list by adjusting whoever points to this
            *pl = l->next;
            free(l);
			UNLOCK(log);
            return;
        }
    }
	UNLOCK(log);
}

//---------------------------------------------------------------------------------------------
// Set the least important message severity that will be output

void
rs_trace_set_level(rs_loglevel level)
{
	LOCK(log);
    rs_trace_level = level;
	UNLOCK(log);
}

//---------------------------------------------------------------------------------------------

// Work out a log level from a string name. 
// Returns -1 for invalid names.

rs_loglevel
rs_loglevel_from_name(const char *name)
{
    if (!strcmp(name, "emerg") || !strcmp(name, "emergency"))
        return RS_LOG_EMERG;
    if (!strcmp(name, "alert"))
        return RS_LOG_ALERT;
    if (!strcmp(name, "critical") || !strcmp(name, "crit"))
        return RS_LOG_CRIT;
    if (!strcmp(name, "error") || !strcmp(name, "err"))
        return RS_LOG_ERR;
    if (!strcmp(name, "warning") || !strcmp(name, "warn"))
        return RS_LOG_WARNING;
    if (!strcmp(name, "notice") || !strcmp(name, "note"))
        return RS_LOG_NOTICE;
    if (!strcmp(name, "info"))
        return RS_LOG_INFO;
    if (!strcmp(name, "debug"))
        return RS_LOG_DEBUG;
    
    return RS_LOG__INVALID;
}

//---------------------------------------------------------------------------------------------
// If you don't initialize a logger before first logging, then we write to stderr by default

static void rs_lazy_default(void)
{
    static int called = 0;

    if (called)
        return;

    called = 1;
    if (logger_list == NULL)
        rs_add_logger(rs_logger_file, RS_LOG_DEBUG, NULL, STDERR_FILENO);
}

//---------------------------------------------------------------------------------------------
// Heart of the matter

static void
rs_log_va(int flags, char const *file, int line, char const *fmt, va_list va)
{
    int level = flags & RS_LOG_PRIMASK;
	struct rs_logger_list *l;

    rs_lazy_default();

    if (level > rs_trace_level)
        return;

	LOCK(log);
    for (l = logger_list; l; l = l->next)
        if (level <= l->max_level)
		{
			UNLOCK(log);
            l->fn(flags, file, line, fmt, va, l->private_ptr, l->private_int);
			LOCK(log);
		}
	UNLOCK(log);
}

//---------------------------------------------------------------------------------------------

void rs_format_msg(char *buf, size_t buf_len, int flags, const char *file, int line, 
	const char *fmt, va_list va)
{
    unsigned level = flags & RS_LOG_PRIMASK;
    int len;
    const char *sv;

    *buf = '\0';
    len = 0;

    if (!(flags & RS_LOG_NO_PROGRAM)) 
	{
        strcpy(buf, rs_program_name);
        len = strlen(buf);
    }

    if (!(flags & RS_LOG_NO_PID)) 
	{
        // You might like to cache the pid, but that would cause trouble when we fork
#ifdef _WIN32
        sprintf(buf + len, "[%d %x] ", (int) getpid(), (int) GetCurrentThreadId());
#else
        sprintf(buf + len, "[%d] ", (int) getpid());
#endif
    }
	else if (~flags & RS_LOG_NO_PROGRAM) 
	{
        strcat(buf+len, ": ");
    }
    len = strlen(buf);

    if (!(flags & RS_LOG_NONAME) && file) 
	{
        sprintf(buf + len, "(%s:%d) ", file, line);
        len = strlen(buf);
    }

    sv = rs_severities[level];
    if (*sv) 
	{
        strcpy(buf + len, sv);
        len = strlen(buf);
    }

    vsnprintf(buf + len, buf_len - len, fmt, va);
}

//---------------------------------------------------------------------------------------------
// Called by a macro, used on platforms where we can't determine the calling function name

void
rs_log0_nofn(int level, char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	rs_log_va(level, NULL, 0, fmt, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------

void rs_log0(int level, char const *file, int line, char const *fmt, ...)
{
	va_list va;
	va_start(va, fmt);
	rs_log_va(level, file, line, fmt, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------

void
rs_logger_syslog(int flags, const char *file, int line, char const *fmt, va_list va,
	void * UNUSED(private_ptr), int UNUSED(private_int))
{
	// NOTE NO TRAILING NUL
	char buf[4096];

	// you're never going to want program or pid in a syslog message, because it's redundant
	rs_format_msg(buf, sizeof(buf), flags | RS_LOG_NO_PROGRAM | RS_LOG_NO_PID, file, line, fmt, va);
	syslog(flags & RS_LOG_PRIMASK, "[%d] %s", log_context, buf);
}

//---------------------------------------------------------------------------------------------

void
rs_logger_file(int flags, const char *file, int line, char const *fmt, va_list va,
	void * UNUSED(private_ptr), int log_fd)
{
	// NOTE NO TRAILING NUL
	char buf[4096];
	size_t len;

	rs_format_msg(buf, sizeof(buf), flags, file, line, fmt, va);

	len = strlen(buf);
	if (len > (int) sizeof buf - 2)
		len = (int) sizeof buf - 2;
	strcpy(&buf[len], "\n");

	(void) write(log_fd, buf, len+1);
}

//---------------------------------------------------------------------------------------------
// functions for handling compilers without varargs macros
//---------------------------------------------------------------------------------------------

// This is called directly if the machine doesn't allow varargs macros

void
rs_log_fatal_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_CRIT, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_error_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_ERR, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_warning_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_WARNING, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_critical_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_CRIT, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_info_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_INFO, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_notice_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_NOTICE, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// This is called directly if the machine doesn't allow varargs macros

void
rs_log_trace_nofn(char const *s, ...) 
{
	va_list	va;
	va_start(va, s);
	rs_log_va(RS_LOG_DEBUG, NULL, 0, s, va);
	va_end(va);
}

//---------------------------------------------------------------------------------------------
// Return true if the library contains trace code; otherwise false.
// If this returns false, then trying to turn trace on will achieve nothing.

int
rs_supports_trace()
{
#ifdef DO_RS_TRACE
    return 1;
#else
    return 0;
#endif
}

//---------------------------------------------------------------------------------------------

void
subproc_error(char const *fmt, ...) 
{
	va_list	args;
	va_start(args, fmt);
	rs_log_va(RS_LOG_ERR, NULL, 0, fmt, args);
	va_end(args);
}

///////////////////////////////////////////////////////////////////////////////////////////////
