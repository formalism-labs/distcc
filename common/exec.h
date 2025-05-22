/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
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

// exec.c

#include "common/arg.h"
#include "rvfc/filesys/defs.h"

namespace distcc
{

using rvfc::File;
using rvfc::Directory;

///////////////////////////////////////////////////////////////////////////////////////////////

int dcc_spawn_child(const Arguments &argv, proc_t &pid, const Directory *cwd,
	const File *in_file, const File *out_fname, const File *err_fname);

int dcc_collect_child(const string &what, const proc_t &pid, int &wait_status);
int dcc_critique_status(int status, const string &command, const Path &input_fname, const string &hostname, bool verbose);

int dcc_new_pgrp();
void dcc_reset_signal(int whichsig);

#ifndef W_EXITCODE
#define W_EXITCODE(exit, signal) ((exit)<<8 | (signal))
#endif 

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
