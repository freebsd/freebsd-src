/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Alexander V. Chernikov
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include "opt_inet.h"
#include "opt_route.h"

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/rwlock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_fib.h>

#include <net/route/nhop_utils.h>
#include <net/route/nhop.h>
#include <net/route/nhop_var.h>

/*
 * This file contains the supporting functions for adding/deleting/updating
 *  multipath routes to the routing table.
 */

SYSCTL_DECL(_net_route);

/*
 * Tries to add @rnd_add nhop to the existing set of nhops (@nh_orig) for the
 * prefix specified by @rt.
 *
 * Return 0 ans consumes rt / rnd_add nhop references. @rc gets populated
 *   with the operation result.
 * Otherwise errno is returned.
 *
 * caller responsibility is to unlock/free rt and
 *  rt->rt_nhop.
 */
int
add_route_mpath(struct rib_head *rnh, struct rt_addrinfo *info,
    struct rtentry *rt, struct route_nhop_data *rnd_add,
    struct route_nhop_data *rnd_orig, struct rib_cmd_info *rc)
{
	RIB_RLOCK_TRACKER;
	struct route_nhop_data rnd_new;
	int error = 0;

	/*
	 * It is possible that multiple rtsock speakers will try to update
	 * the same route simultaneously. Reduce the chance of failing the
	 * request by retrying the cycle multiple times.
	 */
	for (int i = 0; i < RIB_MAX_RETRIES; i++) {
		error = nhgrp_get_addition_group(rnh, rnd_orig, rnd_add,
		    &rnd_new);
		if (error != 0) {
			if (error != EAGAIN)
				break;

			/*
			 * Group creation failed, most probably because
			 * @rnd_orig data got scheduled for deletion.
			 * Refresh @rnd_orig data and retry.
			 */
			RIB_RLOCK(rnh);
			lookup_prefix(rnh, info, rnd_orig);
			RIB_RUNLOCK(rnh);
			continue;
		}

		error = change_route_conditional(rnh, rt, info, rnd_orig,
		    &rnd_new, rc);
		if (error != EAGAIN)
			break;
		RTSTAT_INC(rts_add_retry);
	}

	return (error);
}

struct rt_match_info {
	struct rt_addrinfo *info;
	struct rtentry *rt;
};

static bool
gw_filter_func(const struct nhop_object *nh, void *_data)
{
	struct rt_match_info *ri = (struct rt_match_info *)_data;

	return (check_info_match_nhop(ri->info, ri->rt, nh) == 0);
}

/*
 * Tries to delete matching paths from @nhg.
 * Returns 0 on success and updates operation result in @rc.
 */
int
del_route_mpath(struct rib_head *rh, struct rt_addrinfo *info,
    struct rtentry *rt, struct nhgrp_object *nhg,
    struct rib_cmd_info *rc)
{
	struct route_nhop_data rnd;
	struct rt_match_info ri = { .info = info, .rt = rt };
	int error;

	RIB_WLOCK_ASSERT(rh);

	/*
	 * Require gateway to delete multipath routes, to forbid
	 *  deleting all paths at once.
	 * If the filter function is provided, skip gateway check to
	 *  allow rib_walk_del() delete routes for any criteria based
	 *  on provided callback.
	 */
	if ((info->rti_info[RTAX_GATEWAY] == NULL) && (info->rti_filter == NULL))
		return (ESRCH);

	error = nhgrp_get_filtered_group(rh, nhg, gw_filter_func, (void *)&ri,
	    &rnd);
	if (error == 0)
		error = change_route_nhop(rh, rt, info, &rnd, rc);
	return (error);
}

