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


                /* I put the shotgun in an Adidas bag and padded it
                 * out with four pairs of tennis socks, not my style
                 * at all, but that was what I was aiming for: If they
                 * think you're crude, go technical; if they think
                 * you're technical, go crude.  I'm a very technical
                 * boy.  So I decided to get as crude as possible.
                 * These days, though, you have to be pretty technical
                 * before you can even aspire to crudeness.
                 *              -- William Gibson, "Johnny Mnemonic" */

    
/**
 * @file
 *
 * Routines to decide on which machine to run a distributable job.
 *
 * The current algorithm (new in 1.2 and subject to change) is as follows.
 *
 * CPU lock is held until the job is complete.
 *
 * Once the request has been transmitted, the lock is released and a second
 * job can be sent.
 *
 * Servers which wish to limit their load can defer accepting jobs, and the
 * client will block with that lock held.
 *
 * cpp is probably cheap enough that we can allow it to run unlocked.  However
 * that is not true for local compilation or linking.
 *
 * @todo Write a test harness for the host selection algorithm.  Perhaps a
 * really simple simulation of machines taking different amounts of time to
 * build stuff?
 */

#include "common/config.h"

#ifdef __linux__
#include <unistd.h>
#include <sys/file.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <sys/stat.h>

#include <vector>

#include "common/distcc.h"
#include "common/trace.h"
#include "common/util.h"
#include "common/hosts.h"
#include "common/lock.h"
#include "common/exitcode.h"
#include "common/compiler.h"

#include "client/client.h"
#include "client/where.h"

namespace distcc
{

using std::vector;

///////////////////////////////////////////////////////////////////////////////////////////////

#if 0

dcc_hostdef dcc_pick_host_from_list(int &cpu_lock_fd)
{
	HostDefs hosts;
	if (!hosts)
		throw "no hosts exist";

	hosts.remove_disliked();
    if (!hosts)
		throw "no hosts exist";
    
    return hosts.lock_one(cpu_lock_fd);
}

#endif // 0

//---------------------------------------------------------------------------------------------

void HostDefs::lock_pause()
{
	// This could do with some tuning.
	//
	// My assumption basically is that polling a little too often is
	// relatively cheap; sleeping when we should be working is bad.
	// However, if we hit this code at all we're overloaded, so sleeping a while is perhaps OK.
	//
	// We don't use exponential backoff, because that would tend to prefer later arrivals and
	// penalize jobs that have been waiting for a long time.
	// This would mean more compiler processes hanging around than is really necessary, and 
	// also by making jobs complete very-out-of-order is more likely to find Makefile bugs.

    unsigned pause_time = 1;

    dcc_note_state(DCC_PHASE_BLOCKED);
    
    rs_trace("nothing available, sleeping %us...", pause_time);

    sleep(pause_time);
}

//---------------------------------------------------------------------------------------------

// Find a host that can run a distributed compilation by examining local state.
// It can be either a remote server or localhost (if that is in the list).

// This function does not return (except for errors) until a host has been selected.  
// If necessary it sleeps until one is free.

// @todo We don't need transmit locks for local operations.

dcc_hostdef HostDefs::lock_one(int &cpu_lock_fd)
{
    int ret;

	int num_hosts = _hosts.size();

    for (;;)
	{
        for (int cpu = 0; cpu < 50; ++cpu) 
		{
			vector<dcc_hostdef*> hosts_tab(num_hosts);
			int i = 0;
			for (HostsList::iterator host_i = _hosts.begin(); host_i != _hosts.end(); ++host_i)
				hosts_tab[i++] = &*host_i;

            for (int m = num_hosts - 1; m >= 0; --m) 
			{
				int k = randint(0, m);
				dcc_hostdef *h = hosts_tab[k];

                if (cpu < h->n_slots)
				{
					ret = h->lock("cpu", cpu, 0, cpu_lock_fd);
					if (ret == 0) 
					{
						dcc_note_state_slot(cpu);
						return *h;
					}
					else if (ret != EXIT_BUSY) 
					{
						rs_log_error("failed to lock");
						throw "cannot lock host";
					}
				}

				hosts_tab[k] = hosts_tab[m];
			}
        }
        
        lock_pause();
    }

	throw "cannot lock host";
}

//---------------------------------------------------------------------------------------------

// Lock localhost. Used to get the right balance of jobs when some of them must be local.

dcc_hostdef Client::lock_local(int &cpu_lock_fd)
{
    return local_hostdefs.lock_one(cpu_lock_fd);
}

///////////////////////////////////////////////////////////////////////////////////////////////

} // namespace distcc
