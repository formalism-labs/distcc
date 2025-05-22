/* -*- c-file-style: "java"; indent-tabs-mode: nil; fill-column: 78 -*-
 * 
 * distcc -- A simple distributed compiler system
 *
 * Copyright (C) 2003, 2004 by Martin Pool <mbp@samba.org>
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
                     * They that forsake the law praise the wicked: but such
                     * as keep the law contend with them.
                     *        -- Proverbs 28:4
                     */


#include "common/config.h"

#ifdef __linux__
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>

#include "common/types.h"
#include "common/distcc.h"
#include "common/trace.h"
#include "common/exitcode.h"

#include "server/access.h"

#include "rvfc/exceptions/defs.h"
#include "rvfc/text/defs.h"

namespace distcc
{

using rvfc::error;

///////////////////////////////////////////////////////////////////////////////////////////////

/**
 * @file
 *
 * Simple IP-based access-control system
 */

static const in_addr_t allones = 0xffffffffUL;

//---------------------------------------------------------------------------------------------

// Parses a "HOST/BITS" mask specification.

dcc_allow_spec::dcc_allow_spec(const string &spec_)
{
	const char *spec = +spec_;
    int value_len;
    struct in_addr ia;
    int mask_bits;
    char *value_str;
    int matched;
    const char *mask_str;

    value_len = strcspn(spec, "/");

    // get bit before slash
    value_str = strdup(spec);
    value_str[value_len] = '\0';
    matched = inet_aton(value_str, &ia);
    
    // extract and parse value part
    if (!matched) 
	{
        rs_log_error("can't parse internet address '%s'", value_str);
        free(value_str);
		throw error("bad arguments");
    }
    free(value_str);
    addr = ia.s_addr;

    mask_str = &spec[value_len + 1];
    if (spec[value_len] && *mask_str) 
	{
        // find mask length as a number of bits
        mask_bits = atoi(mask_str);
        if (mask_bits < 0 || mask_bits > 32) 
		{
            rs_log_error("invalid mask \"%s\"", mask_str);
            throw error("bad arguments");
        }

        // Make a network-endian mask with the top mask_bits set
        if (mask_bits == 32)
            mask = allones;
        else
            mask = htonl(~(allones >> mask_bits));
    }
	else 
	{
        mask = allones;
    }
}

//---------------------------------------------------------------------------------------------

// Check whether a client ought to be allowed.

bool dcc_allow_spec::allowed(in_addr_t client) const
{
    if ((client & mask) == (addr & mask)) 
	{
        rs_trace("match client %#lx, value %#lx, mask %#lx", (long) client, (long) addr, (long) mask);
        return true;
    }
	else
	{
        rs_trace("deny client %#lx, value %#lx, mask %#lx", (long) client, (long) addr, (long) mask);
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
