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

#include "common/trace.h"
#include "common/arg.h"

namespace distcc
{

// dopt.c
int distcc_parse_options(const Arguments &args);

// extern int cc_argc;
#if 0
extern Arguments cc_args;
extern text arg_compiler;
extern text arg_session;
extern text arg_view;
extern text arg_hosts_class;
extern int arg_on_server;
extern rs_loglevel arg_log_level;
extern int arg_log_stderr, arg_log_syslog;
extern int arg_verbose;
#endif

} // namespace distcc
