/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003 by Martin Pool <mbp@samba.org>
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

#ifndef _distcc_server_access_h_
#define _distcc_server_access_h_

#include <string>
#include <list>

#include "common/types.h"

#include "rvfc/net/defs.h"

namespace distcc
{

using std::string;
using std::list;

///////////////////////////////////////////////////////////////////////////////////////////////

class dcc_allow_spec
{
public:
	dcc_allow_spec(const string &spec);

	bool allowed(in_addr_t client) const;

    in_addr_t addr, mask;
};


typedef list<dcc_allow_spec> dcc_allow_list;

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc

#endif // _distcc_server_access_h_
