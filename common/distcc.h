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

/* distcc.h -- common internal-use header file */

#ifndef _DISTCC_CLIENT_DISTCC_H_
#define _DISTCC_CLIENT_DISTCC_H_

#include <string>
#include <list>

#include "rvfc/defs.h"

#include <sys/types.h>
#ifdef __linux__
#include <sys/time.h>
#endif

#include "common/exitcode.h"
#include "common/state.h"
#include "common/arg.h"

namespace distcc
{

///////////////////////////////////////////////////////////////////////////////////////////////

#ifdef NORETURN
/* nothing */
#elif defined(__GNUC__)
#  define NORETURN __attribute__((noreturn))
#elif defined(__LCLINT__)
#  define NORETURN /*@noreturn@*/ x
#else                           /* !__GNUC__ && !__LCLINT__ */
#  define NORETURN
#endif                          /* !__GNUC__ && !__LCLINT__ */

#ifdef UNUSED
/* nothing */
#elif defined(__GNUC__)
#  define UNUSED(x) UNUSED_ ## x __attribute__((unused))
#elif defined(__LCLINT__)
#  define UNUSED(x) /*@unused@*/ x
#else				/* !__GNUC__ && !__LCLINT__ */
#  define UNUSED(x) x
#endif				/* !__GNUC__ && !__LCLINT__ */


#if defined(__GNUC__) && ((__GNUC__ > 3) || (__GNUC__ == 3 && __GNUC_MINOR__ > 3))
/* This works on Gentoo's (patched?) gcc 3.3.3 but not 3.2.3, and not Debian's
 * 3.3.4.  It should be standard on 3.4. */
#  define WARN_UNUSED  __attribute__((warn_unused_result))
#else
#define WARN_UNUSED
#endif

#ifndef TRUE
#define TRUE 1
#define FALSE 0
#endif

using std::string;
using std::list;

using rvfc::File;

class dcc_hostdef;

//---------------------------------------------------------------------------------------------

#ifdef _WIN32

class WinFile
{
protected:
	HANDLE _h;

public:
	WinFile() : _h(INVALID_HANDLE_VALUE) {}
	virtual ~WinFile() {}

	HANDLE handle() const { return _h; }
	operator HANDLE() const { return _h; }
};

class TemporaryFile : public WinFile
{
public:
	TemporaryFile(const File &file, bool create = true);
	~TemporaryFile();
};

#endif // _WIN32

//---------------------------------------------------------------------------------------------

class proc_t
{
public:
#ifdef _WIN32
	DWORD pid;
	HANDLE handle;
//	rvfc::ptr<TemporaryFile> in_file, out_file;

	proc_t(DWORD pid = -1, HANDLE handle = INVALID_HANDLE_VALUE) : pid(pid), handle(handle) {}

	~proc_t()
	{
//		if (!!in_file)
//			in_file = rvfc::null;
//		if (!!out_file)
//			out_file = rvfc::null;
	}

	bool operator!() const { return pid == -1 || handle == INVALID_HANDLE_VALUE; }

#else // ! _WIN32

	pid_t pid;

	proc_t(pid_t pid = -1) : pid(pid) {}

	bool operator!() const { return pid == -1; }

#endif // ! _WIN32
};

typedef struct 
{
	int fd;
	unsigned int socket:1;
} fd_t;

#define dcc_fd_cmp(fd0, fd1) ((fd0).fd == (fd1).fd && (fd0).socket == (fd1).socket ? 0 : 1)

//---------------------------------------------------------------------------------------------

enum dcc_compress
{
    // wierd values to catch errors
    DCC_COMPRESS_NONE     = 69,
    DCC_COMPRESS_LZO1X
};

enum dcc_protover 
{
    DCC_VER_1   = 1,            /**< vanilla */
    DCC_VER_2   = 2             /**< ditto with LZO sprinkles */
};

//---------------------------------------------------------------------------------------------

// A macro so that we get the right __FUNCTION__ in the trace message.
// We condition on rs_trace_enabled so that we don't do the to-string conversion unless 
// the user will actually see the result, because it's a little expensive.

// help.c
int dcc_trace_version();
int dcc_show_version(const char *prog);

// ncpu.c
int dcc_ncpus(int &cpus);

// ssh.c
#ifdef FEATURE_SSH_MODE
int dcc_ssh_connect(char *ssh_cmd, char *user, char *machine, char *path, int *f_in, int *f_out, pid_t *ssh_pid);
#endif

// safeguard.c
int dcc_increment_safeguard();
int dcc_recursion_safeguard();

// climasq.c
// int dcc_support_masquerade(const Arguments &args, const string &progname, int &did_masquerade);

// backoff.c
// int dcc_enjoyed_host(const dcc_hostdef *host);
// int dcc_disliked_host(const dcc_hostdef *host);
// int dcc_remove_disliked(dcc_hostdef **hostlist);

#define DISTCC_DEFAULT_PORT 3632

#ifndef WAIT_ANY
#define WAIT_ANY (-1)
#endif

// If --enable-rfc2553 was given, then we will try to enable compile-time IPv6 support.
// This means we must have a sockaddr_storage large enough to hold IPv6 addresses.
// If not, we'll just use a plain sockaddr, which is more likely to compile correctly.
#ifdef ENABLE_RFC2553
#ifndef HAVE_SOCKADDR_STORAGE
#error You can't use RFC2553 because you don't have a sockaddr_storage type
#endif /* HAVE_SOCKADDR_STORAGE */
#define dcc_sockaddr_storage sockaddr_storage
#else // ! ENABLE_RFC2553
#define dcc_sockaddr_storage sockaddr
#endif // ! ENABLE_RFC2553

#ifndef O_BINARY
#define O_BINARY 0
#endif

// compress.c
// extern __declspec(thread) char *compress_work_mem;

int dcc_r_bulk_lzo1x(fd_t outf_fd, fd_t in_fd, unsigned in_len);
int dcc_compress_file_lzo1x(fd_t in_fd, size_t in_len, char **out_buf, size_t *out_len);

// bulk.h
void dcc_calc_rate(off_t size_out, struct timeval &before, struct timeval &after, double &secs, double &rate);

// arg.c
void dcc_note_compiled(const Path &input_file, const Path &output_file);

// tempfile.c
// int dcc_get_tempdir(const char **);
File dcc_make_tmpnam(const char *, const char *suffix);
string dcc_get_tmp_top();

void dcc_mkdir(const string &path);

// cleanup.c
void dcc_cleanup_tempfiles();
void dcc_add_cleanup(const string &filename);

// filename.c
string dcc_find_extension(const Path &filename);
int dcc_output_from_source(const string &fname, const string &out_extn, string &ofile);

// string dcc_find_basename(const string &sfile);

// io.c
fd_t dcc_fd(int fd, int sock);

dcc_exitcode dcc_writex(fd_t fd, const void *buf, size_t len);

dcc_exitcode dcc_r_token(fd_t ifd, char *token);

dcc_exitcode dcc_readx(fd_t fd, void *buf, size_t len);
int dcc_pump_sendfile(fd_t ofd, fd_t ifd, size_t n);
dcc_exitcode dcc_r_str_alloc(fd_t fd, unsigned len, char **buf);

int tcp_cork_sock(fd_t fd, int corked);
dcc_exitcode dcc_close(fd_t fd);
int dcc_want_mmap();

dcc_exitcode dcc_select_for_write(fd_t fd, int timeout);
dcc_exitcode dcc_select_for_read(fd_t fd, int timeout);

const char *socket_error_str();

// loadfile.c
string dcc_load_file_string(const File &filename);
list<string> dcc_load_file_lines(const File &filename);

extern const int dcc_connect_timeout, dcc_io_timeout;

// pump.c
//extern __declspec(thread) char *pump_rw_buf;

int dcc_r_bulk(fd_t ofd, fd_t ifd, unsigned f_size, enum dcc_compress compression);
int dcc_pump_readwrite(fd_t ofd, fd_t ifd, size_t n);

// mapfile.c
int dcc_map_input_file(int in_fd, off_t in_size, char **buf_ret);

// XXX: Kind of kludgy, we should do dynamic allocation.  But this will do for now.
#ifndef MAXPATHLEN
#define MAXPATHLEN 4096
#endif

#ifndef WCOREDUMP
#define WCOREDUMP(status) 0
#endif

int dcc_socket_listen(int port, int *fd_out, const char *listen_addr);

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _DISTCC_CLIENT_DISTCC_H_
