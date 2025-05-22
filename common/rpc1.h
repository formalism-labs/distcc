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

                /* His hand is stretched out, and who shall turn it back?
                 * -- Isaiah 14:27 */
#ifndef _DISTCC_RPC1_H
#define _DISTCC_RPC1_H

#include <string>

#include "common/arg.h"

#include "rvfc/filesys/defs.h"

namespace distcc
{

using rvfc::Directory;

///////////////////////////////////////////////////////////////////////////////////////////////

dcc_exitcode dcc_x_result_header(fd_t ofd, enum dcc_protover);
dcc_exitcode dcc_r_result_header(fd_t ofd, enum dcc_protover);

dcc_exitcode dcc_x_cc_status(fd_t, int);
dcc_exitcode dcc_r_cc_status(fd_t, int &);

dcc_exitcode dcc_x_token_int(fd_t ofd, const char *token, unsigned param);
dcc_exitcode dcc_r_token_int(fd_t ifd, const char *expected, unsigned int &val);
unsigned int dcc_r_token_int(fd_t ifd, const char *expected);

dcc_exitcode dcc_x_token_string(fd_t fd, const char *token, const string &buf);
dcc_exitcode dcc_r_token_string(fd_t ifd, const char *expect_token, string &str);
std::string dcc_r_token_string(fd_t ifd, const char *expect_token);

// srvrpc.c, clirpc.c

dcc_exitcode dcc_r_request_header(fd_t ifd, enum dcc_protover &);
dcc_exitcode dcc_x_req_header(fd_t fd, enum dcc_protover protover);

dcc_exitcode dcc_r_session_name(fd_t ifd, string &session_name);
dcc_exitcode dcc_x_session_name(fd_t fd, const string &session);

dcc_exitcode dcc_r_flags(fd_t ifd, unsigned &flag);
dcc_exitcode dcc_x_flags(fd_t ifd, unsigned flag);

dcc_exitcode dcc_r_compile_dir(fd_t ifd, Directory &cdir);
dcc_exitcode dcc_x_compile_dir(fd_t fd, const Directory &cdir);

dcc_exitcode dcc_r_view_name(fd_t ifd, string &view_name);
dcc_exitcode dcc_x_view_name(fd_t fd, const string &view);

dcc_exitcode dcc_r_argv(fd_t ifd, Arguments &args);
Arguments dcc_r_argv(fd_t ifd);
dcc_exitcode dcc_x_argv(fd_t fd, const Arguments &args);

enum dcc_command_flags
{
	CMD_FLAGS_ON_SERVER = 0x1,
	CMD_FLAGS_NEED_PDB = 0x2,
	CMD_FLAGS_NEED_DOTI = 0x4
};

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif //  _DISTCC_RPC1_H
