/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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
			/* I will make a man more precious than fine
			 * gold; even a man than the golden wedge of
			 * Ophir.
			 *		-- Isaiah 13:12 */


#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <netdb.h>
#include <sys/time.h>
#include <sys/param.h>

#ifdef HAVE_SYS_RESOURCE_H
#include <sys/resource.h>
#endif
#endif // __linux__

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <stdarg.h>
#ifdef _WIN32
#include <io.h>
#endif

#include <sys/stat.h>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/exitcode.h"

#include "rvfc/filesys/defs.h"

namespace distcc
{

using rvfc::File;

///////////////////////////////////////////////////////////////////////////////////////////////

void dcc_exit(int exitcode)
{
#if HAVE_SYS_RESOURCE_H
    struct rusage self_ru, children_ru;

    if (getrusage(RUSAGE_SELF, &self_ru)) 
	{
        rs_log_warning("getrusage(RUSAGE_SELF) failed: %s", strerror(errno));
        memset(&self_ru, 0, sizeof self_ru);
    }
    if (getrusage(RUSAGE_CHILDREN, &children_ru)) 
	{
        rs_log_warning("getrusage(RUSAGE_CHILDREN) failed: %s", strerror(errno));
        memset(&children_ru, 0, sizeof children_ru);
    }

	// NB fields must match up for microseconds
	rs_log(RS_LOG_INFO,
		"exit: code %d; self: %d.%06d user %d.%06d sys; children: %d.%06d user %d.%06d sys",
		exitcode,
		(int) self_ru.ru_utime.tv_sec, (int) self_ru.ru_utime.tv_usec,
		(int) self_ru.ru_stime.tv_sec, (int) self_ru.ru_stime.tv_usec,
		(int) children_ru.ru_utime.tv_sec, (int) children_ru.ru_utime.tv_usec,
		(int) children_ru.ru_stime.tv_sec, (int)  children_ru.ru_stime.tv_usec);
#endif // HAVE_SYS_RESOURCE_H

    exit(exitcode);
}

//---------------------------------------------------------------------------------------------

#if 0

//---------------------------------------------------------------------------------------------

int str_endswith(const char *tail, const char *tiger)
{
        size_t len_tail = strlen(tail);
	size_t len_tiger = strlen(tiger);

	if (len_tail > len_tiger)
		return 0;

	return !strcmp(tiger + len_tiger - len_tail, tail);
}


//---------------------------------------------------------------------------------------------

bool str_endswith(const string &s, const string &sub)
{
	size_t i = s.rfind(sub);
	return i != string::npos && i == s.length() - sub.length();
}

//---------------------------------------------------------------------------------------------

bool str_endswith(const string &s, const char *sub)
{
	size_t i = s.rfind(sub);
	return i != string::npos && i == s.length() - strlen(sub);
}

//---------------------------------------------------------------------------------------------

int str_startswith(const char *head, const char *worm)
{
    return !strncmp(head, worm, strlen(head));
}

//---------------------------------------------------------------------------------------------

bool str_startswith(const string &s, const string &sub)
{
	return s.find(sub) == 0;
}

//---------------------------------------------------------------------------------------------

bool str_startswith(const string &s, const char *sub)
{
	return s.find(sub) == 0;
}

//---------------------------------------------------------------------------------------------

bool str_equalsto_one_of(const string &str, ...)
{
	bool rc = false;
	va_list args;
	va_start(args, str);
	for (;;)
	{
		char *p = va_arg(args, char*);
		if (!p)
			break;
		if (str == p)
		{
			rc = true;
			break;
		}
	}

	va_end(args);
	return rc;
}

//---------------------------------------------------------------------------------------------

bool str_startswith_one_of(const string &str, ...)
{
	bool rc = false;
	va_list args;
	va_start(args, str);
	for (;;)
	{
		char *head = va_arg(args, char*);
		if (!head)
			break;
		if (str_startswith(str, head))
		{
			rc = true;
			break;
		}
	}

	va_end(args);
	return rc;
}

//---------------------------------------------------------------------------------------------

bool str_contains(const string &s, const char *sub)
{
	return s.find(sub) != string::npos;
}
//---------------------------------------------------------------------------------------------

// Skim through NULL-terminated @p argv, looking for @p s.

int argv_contains(char **argv, const char *s)
{
	while (*argv) 
	{
		if (!strcmp(*argv, s))
			return 1;
		argv++;
	}
	return 0;
}

//---------------------------------------------------------------------------------------------

#endif // 0

//---------------------------------------------------------------------------------------------

// Redirect a file descriptor into (or out of) a file.
//
// Used, for example, to catch compiler error messages into a temporary file.

int dcc_redirect_fd(int fd, const File &file, int mode)
{
    // ignore errors
    close(fd);
    
    int newfd = open(+file.path(), mode, 0666);
    if (newfd == -1)
	{
        rs_log_crit("failed to reopen fd%d onto %s: %s", fd, +file.path(), strerror(errno));
        return EXIT_IO_ERROR;
    }
	if (newfd != fd) 
	{
        rs_log_crit("oops, reopened fd%d onto fd%d?", fd, newfd);
        return EXIT_IO_ERROR;
    }

    return 0;
}

//---------------------------------------------------------------------------------------------

char *dcc_gethostname()
{
    static char myname[100] = "\0";

    if (!myname[0])
	{
        if (gethostname(myname, sizeof myname - 1) == -1)
            strcpy(myname, "UNKNOWN");
    }

    return myname;
}

//---------------------------------------------------------------------------------------------

// Look up a boolean environment option, which must be either "0" or "1".  
// The default, if it's not set or is empty, is @p default.

int dcc_getenv_bool(const char *name, int default_value)
{
    const char *e;

    e = getenv(name);
    if (!e || !*e)
        return default_value;
    if (!strcmp(e, "1"))
        return 1;
    else if (!strcmp(e, "0"))
        return 0;
    else
        return default_value;
}

//---------------------------------------------------------------------------------------------

// Set the `FD_CLOEXEC' flag of DESC if VALUE is nonzero, or clear the flag if VALUE is 0.

// From the GNU C Library examples.
// @returns 0 on success, or -1 on error with `errno' set.

int set_cloexec_flag (int desc, int value)
{
#ifndef _WIN32
    int oldflags = fcntl (desc, F_GETFD, 0);
    // If reading the flags failed, return error indication now
    if (oldflags < 0)
        return oldflags;
    // Set just the flag we want to set
    if (value != 0)
        oldflags |= FD_CLOEXEC;
    else
        oldflags &= ~FD_CLOEXEC;
    // Store modified flag word in the descriptor
    return fcntl (desc, F_SETFD, oldflags);
#else
	return 0;
#endif // _WIN32
}

//---------------------------------------------------------------------------------------------

// Ignore or unignore SIGPIPE.
//
// The server and child ignore it, because distcc code wants to see EPIPE errors if something 
// goes wrong.  However, for invoked children it is set back to the default value, because 
// they may not handle the error properly.

int dcc_ignore_sigpipe(int val)
{
#ifdef __linux__
    if (signal(SIGPIPE, val ? SIG_IGN : SIG_DFL) == SIG_ERR) 
	{
		rs_log_warning("signal(SIGPIPE, %s) failed: %s", 
			val ? "ignore" : "default",strerror(errno));
        return EXIT_DISTCC_FAILED;
    }
#endif // __linux__

    return 0;
}

//---------------------------------------------------------------------------------------------

// Search through the $PATH looking for a directory containing a file called @p compiler_name, 
// which is a symbolic link containing the string "distcc".
//
// Trim the path to just after the *last* such directory.
//
// If we find a distcc masquerade dir on the PATH, remove all the dirs up to that point.

int dcc_trim_path(const char *compiler_name)
{
    const char *envpath, *newpath = NULL;
	char *buf;
#ifdef __linux__
	const char *p, *n;
    char linkbuf[MAXPATHLEN];
    struct stat sb;
    size_t len;
#endif

    if (!(envpath = getenv("PATH"))) 
	{
        rs_trace("PATH seems not to be defined");
        return 0;
    }

    rs_trace("original PATH %s", envpath);
    rs_trace("looking for \"%s\"", compiler_name);

    // Allocate a buffer that will let us append "/cc" onto any PATH
    // element, even if there is only one item in the PATH.
	buf = (char *) malloc(strlen(envpath) + 1 + strlen(compiler_name) + 1);
    if (!buf)
	{
        rs_log_error("failed to allocate buffer for PATH munging");
        return EXIT_OUT_OF_MEMORY;
    }

#ifdef __linux__
    for (n = p = envpath, newpath = NULL; *n; p = n) 
	{
        n = strchr(p, ':');
        if (n)
            len = n++ - p;
        else 
		{
            len = strlen(p);
            n = p + len;
        }
        strncpy(buf, p, len);

        sprintf(buf + len, "/%s", compiler_name);
        if (lstat(buf, &sb) == -1)
            continue; // ENOENT, EACCESS, etc
        if (!S_ISLNK(sb.st_mode))
            break;
        if ((len = readlink(buf, linkbuf, sizeof linkbuf)) <= 0)
            continue;
        linkbuf[len] = '\0';
        if (strstr(linkbuf, "distcc")) 
		{
            // Set newpath to the part of the PATH past our match
            newpath = n;
        }
    }
#endif // __linux__

    if (newpath) 
	{
        int ret = dcc_set_path(newpath);
        if (ret)
            return ret;
    }
	else
        rs_trace("not modifying PATH");

    free(buf);
    return 0;
}

//---------------------------------------------------------------------------------------------

// Set the PATH environment variable to the indicated value

int dcc_set_path(const char *newpath)
{
    string buf = stringf("PATH=%s", newpath);

    rs_trace("setting %s", +buf);
    if (putenv(+buf) < 0) 
	{
        rs_log_error("putenv PATH failed");
        return EXIT_FAILURE;
    }
    // We must leave "buf" allocated
    return 0;
}

//---------------------------------------------------------------------------------------------

// Return the supplied path with the current-working directory prefixed (if needed) and 
// all "dir/.." references removed.  Supply path_len if you want to use only a substring of 
// the path string, otherwise make it 0.

char *dcc_abspath(const char *path, int path_len)
{
    static char buf[MAXPATHLEN];
    unsigned len;
    char *p, *slash;

    if (*path == '/')
        len = 0;
    else 
	{
#ifdef HAVE_GETCWD
        getcwd(buf, sizeof buf);
#else
        getwd(buf);
#endif
        len = strlen(buf);
        if (len >= sizeof buf) 
            rs_log_crit("getwd overflowed in dcc_abspath()");

		buf[len++] = '/';
    }
    if (path_len <= 0)
        path_len = strlen(path);
    if (path_len >= 2 && *path == '.' && path[1] == '/') 
	{
		path += 2;
		path_len -= 2;
    }
    if (len + (unsigned)path_len >= sizeof buf) 
	{
        rs_log_error("path overflowed in dcc_abspath()");
        exit(EXIT_OUT_OF_MEMORY);
    }
    strncpy(buf + len, path, path_len);
    buf[len + path_len] = '\0';
    for (p = buf+len-(len > 0); (p = strstr(p, "/../")) != NULL; p = slash) 
	{
		*p = '\0';
		if (!(slash = strrchr(buf, '/')))
			slash = p;
		strcpy(slash, p+3);
    }
    return buf;
}

//---------------------------------------------------------------------------------------------

// Duplicate the part of the string @p psrc up to a character in @p sep (or end of string), 
// storing the result in @p pdst.  @p psrc is updated to point to the terminator.  
// (If the terminator is not found it will therefore point to \0).
//
// If there is no more string, then @p pdst is instead set to NULL, no memory is allocated, 
// and @p psrc is not advanced.

class StringParts
{
	string _str, _sep, _part;
	char sep_ch;
	int _i;

public:
	StringParts(const string &str, const string &sep) : _str(str), _sep(sep), _i(0), sep_ch('\0') { next(); }

	bool operator!() const { return _i == -1; }

	bool next()
	{
		if (_i == -1)
			return false;

		int len = strcspn(_str.c_str() + _i, _sep.c_str());
		if (len == 0) 
		{
			_part = "";
			_i = -1;
			return false;
		}

		sep_ch = _str[len];
		_part = _str.substr(_i, len);
		_i += len;
		return true;
	}

	char sep() const { return sep_ch; }

	StringParts &operator++() { next(); return *this; }

	string part() const { return _part;	}
};

int dcc_dup_part(const char **psrc, char **pdst, const char *sep)
{
    int len;

    len = strcspn(*psrc, sep);
    if (len == 0) 
	{
        *pdst = NULL;
    }
	else 
	{
		*pdst = (char *) malloc(len + 1);
        if (! *pdst) 
		{
            rs_log_error("failed to allocate string duplicate: %d", (int) len);
            return EXIT_OUT_OF_MEMORY;
        }
        strncpy(*pdst, *psrc, len);
        (*pdst)[len] = '\0';
        (*psrc) += len;
    }

    return 0;
}

//---------------------------------------------------------------------------------------------

#if 0

int dcc_remove_if_exists(const string &fname)
{
	const char *fname_p = fname.c_str();
    if (unlink(fname_p) && errno != ENOENT) 
	{
        rs_log_warning("failed to unlink %s: %s", fname_p, strerror(errno));
        return EXIT_IO_ERROR;
    }
    return 0;
}

#endif // 0

//---------------------------------------------------------------------------------------------

#ifndef HAVE_STRLCPY

// like strncpy but does not 0 fill the buffer and always null terminates. 
// bufsize is the size of the destination buffer.

 size_t strlcpy(char *d, const char *s, size_t bufsize)
{
	size_t len = strlen(s);
	size_t ret = len;
	if (bufsize <= 0) return 0;
	if (len >= bufsize) len = bufsize-1;
	memcpy(d, s, len);
	d[len] = 0;
	return ret;
}

#endif // HAVE_STRLCPY

//---------------------------------------------------------------------------------------------

#ifndef HAVE_STRNDUP

// Copy at most @p size characters from @p src, plus a terminating nul.

static char *strndup(const char *src, size_t size)
{
    char *dst = (char *) malloc(size + 1);
    if (dst == NULL)
        return NULL;
    strncpy(dst, src, size);
    dst[size] = '\0';

    return dst;
}
#endif

//---------------------------------------------------------------------------------------------

void print_args(const char *title, int argc, char *argv[])
{
	char s[8192], *p = s;
	char **a, *q;
	for (a = argv; a && *a; ++a)
	{
		for (q = *a; q && *q; )
			*p++ = *q++;
		*p++ = ' ';
	}
	*p = '\0';
	if (title && *title)
		printf("\n%s: %s\n", title, s);
	else
		printf("%s\n", s);
}

//---------------------------------------------------------------------------------------------

int randint(int m, int M)
{
	int r = rand();
	return m + (int) (1.0 * (M - m) * (r / (RAND_MAX + 1.0)));
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
