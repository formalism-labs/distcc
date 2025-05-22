/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78; -*-
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

namespace distcc
{

int dcc_x_file(fd_t ofd, const File &fname, const char *token, enum dcc_compress compression, off_t *);

int dcc_r_file(fd_t ifd, File &filename, unsigned, enum dcc_compress);
int dcc_r_file_timed(fd_t ifd, File &fname, unsigned size, enum dcc_compress);
int dcc_r_fifo(fd_t ifd, const string &fifo_name, size_t len);

int dcc_r_token_file(fd_t ifd, const char *token, File &fname, unsigned int &size, enum dcc_compress compr);
int dcc_r_token_bulk(fd_t in_fd, const char *token, fd_t out_fd, enum dcc_compress compr);

} // namespace distcc
