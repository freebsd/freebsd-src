/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * This file contains code responsible for expiring temporal routes
 * (typically, redirect-originated) from the route tables.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/ck.h>
#include <sys/rmlock.h>
#include <sys/callout.h>

#include <net/if.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/vnet.h>

/*
 * Callback returning 1 for the expired routes.
 * Updates time of the next nearest route expiration as a side effect.
 */
static int
expire_route(const struct rtentry *rt, const struct nhop_object *nh, void *arg)
{
	uint32_t nh_expire = nhop_get_expire(nh);
	time_t *next_callout;

	if (nh_expire == 0)
		return (0);

	if (nh_expire <= time_uptime)
		return (1);

	next_callout = (time_t *)arg;

	/*
	 * Update next_callout to determine the next ts to
	 * run the callback at.
	 */
	if (*next_callout == 0 || *next_callout > nh_expire)
		*next_callout = nh_expire;

	return (0);
}

/*
 * Per-rnh callout function traversing the tree and deleting
 * expired routes. Calculates next callout run by looking at
 * the nh_expire time for the remaining temporal routes.
 */
static void
expire_callout(void *arg)
{
	struct rib_head *rnh;
	time_t next_expire;
	int seconds;

	rnh = (struct rib_head *)arg;

	CURVNET_SET(rnh->rib_vnet);
	next_expire = 0;

	rib_walk_del(rnh->rib_fibnum, rnh->rib_family, expire_route,
	    (void *)&next_expire, 1);

	RIB_WLOCK(rnh);
	if (next_expire > 0) {
		seconds = (next_expire - time_uptime);
		if (seconds < 0)
			seconds = 0;
		callout_reset_sbt(&rnh->expire_callout, SBT_1S * seconds,
		    SBT_1MS * 500, expire_callout, rnh, 0);
		rnh->next_expire = next_expire;
	} else {
		/*
		 * Before resetting next_expire, check that tmproutes_update()
		 * has not kicked in and scheduled another invocation.
		 */
		if (callout_pending(&rnh->expire_callout) == 0)
			rnh->next_expire = 0;
	}
	RIB_WUNLOCK(rnh);
	CURVNET_RESTORE();
}

/*
 * Function responsible for updating the time of the next calllout
 * w.r.t. new temporal routes insertion.
 *
 * Called by the routing code upon adding new temporal route
 * to the tree. RIB_WLOCK must be held.
 */
void
tmproutes_update(struct rib_head *rnh, struct rtentry *rt, struct nhop_object *nh)
{
	int seconds;
	uint32_t nh_expire = nhop_get_expire(nh);

	RIB_WLOCK_ASSERT(rnh);

	if (rnh->next_expire == 0 || rnh->next_expire > nh_expire) {
		/*
		 * Callback is not scheduled, is executing,
		 * or is scheduled for a later time than we need.
		 *
		 * Schedule the one for the current @rt expiration time.
		 */
		seconds = (nh_expire - time_uptime);
		if (seconds < 0)
			seconds = 0;
		callout_reset_sbt(&rnh->expire_callout, SBT_1S * seconds,
		    SBT_1MS * 500, expire_callout, rnh, 0);

		rnh->next_expire = nh_expire;
	}
}

void
tmproutes_init(struct rib_head *rh)
{

	callout_init(&rh->expire_callout, 1);
}

void
tmproutes_destroy(struct rib_head *rh)
{

	callout_drain(&rh->expire_callout);
}
