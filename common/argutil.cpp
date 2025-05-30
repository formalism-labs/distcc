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



/*
 * Utilities for dealing with argv[]-style strings.
 *
 * These rules might not yet be consistently applied in distcc, but they
 * should be in the future:
 *
 * For simplicity in managing memory we try to keep all argv structures
 * malloc'd, without any shared structure.  It is then possible to just free
 * the whole thing whenever we're finished with it.
 *
 * One exception is of course the argv used to invoke the program, which is
 * treated as read-only.
 */


#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#include <unistd.h>
#endif

#include <string.h>
#include <fcntl.h>
#include <errno.h>

#include <sys/stat.h>

#include "distcc.h"
#include "trace.h"
#include "util.h"
#include "exitcode.h"

///////////////////////////////////////////////////////////////////////////////////////////////

///////////////////////////////////////////////////////////////////////////////////////////////
