/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 1980, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/rmlock.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_private.h>
#include <net/if_dl.h>
#include <net/route.h>
#include <net/route/route_ctl.h>
#include <net/route/route_var.h>
#include <net/route/nhop.h>
#include <net/vnet.h>

#include <netinet/in.h>

/*
 * Control interface address fib propagation.
 * By default, interface address routes are added to the fib of the interface.
 * Once set to non-zero, adds interface address route to all fibs.
 */
VNET_DEFINE(u_int, rt_add_addr_allfibs) = 0;
SYSCTL_UINT(_net, OID_AUTO, add_addr_allfibs, CTLFLAG_RWTUN | CTLFLAG_VNET,
    &VNET_NAME(rt_add_addr_allfibs), 0, "");

/*
 * Executes routing tables change specified by @cmd and @info for the fib
 * @fibnum. Generates routing message on success.
 * Note: it assumes there is only single route (interface route) for the
 * provided prefix.
 * Returns 0 on success or errno.
 */
static int
rib_handle_ifaddr_one(uint32_t fibnum, int cmd, struct rt_addrinfo *info)
{
	struct rib_cmd_info rc;
	struct nhop_object *nh;
	int error;

	error = rib_action(fibnum, cmd, info, &rc);
	if (error == 0) {
		if (cmd == RTM_ADD)
			nh = nhop_select(rc.rc_nh_new, 0);
		else
			nh = nhop_select(rc.rc_nh_old, 0);
		rt_routemsg(cmd, rc.rc_rt, nh, fibnum);
	}

	return (error);
}

/*
 * Adds/deletes interface prefix specified by @info to the routing table.
 * If V_rt_add_addr_allfibs is set, iterates over all existing routing
 * tables, otherwise uses fib in @fibnum. Generates routing message for
 *  each table.
 * Returns 0 on success or errno.
 */
int
rib_handle_ifaddr_info(uint32_t fibnum, int cmd, struct rt_addrinfo *info)
{
	int error = 0, last_error = 0;
	bool didwork = false;

	if (V_rt_add_addr_allfibs == 0) {
		error = rib_handle_ifaddr_one(fibnum, cmd, info);
		didwork = (error == 0);
	} else {
		for (fibnum = 0; fibnum < V_rt_numfibs; fibnum++) {
			error = rib_handle_ifaddr_one(fibnum, cmd, info);
			if (error == 0)
				didwork = true;
			else
				last_error = error;
		}
	}

	if (cmd == RTM_DELETE) {
		if (didwork) {
			error = 0;
		} else {
			/* we only give an error if it wasn't in any table */
			error = ((info->rti_flags & RTF_HOST) ?
			    EHOSTUNREACH : ENETUNREACH);
		}
	} else {
		if (last_error != 0) {
			/* return an error if any of them failed */
			error = last_error;
		}
	}
	return (error);
}

static int
ifa_maintain_loopback_route(int cmd, const char *otype, struct ifaddr *ifa,
    struct sockaddr *ia)
{
	struct rib_cmd_info rc;
	struct epoch_tracker et;
	int error;
	struct rt_addrinfo info;
	struct sockaddr_dl null_sdl;
	struct ifnet *ifp;

	ifp = ifa->ifa_ifp;

	NET_EPOCH_ENTER(et);
	bzero(&info, sizeof(info));
	if (cmd != RTM_DELETE)
		info.rti_ifp = V_loif;
	if (cmd == RTM_ADD) {
		/* explicitly specify (loopback) ifa */
		if (info.rti_ifp != NULL)
			info.rti_ifa = ifaof_ifpforaddr(ifa->ifa_addr, info.rti_ifp);
	}
	info.rti_flags = ifa->ifa_flags | RTF_HOST | RTF_STATIC | RTF_PINNED;
	info.rti_info[RTAX_DST] = ia;
	info.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&null_sdl;
	link_init_sdl(ifp, (struct sockaddr *)&null_sdl, ifp->if_type);

	error = rib_action(ifp->if_fib, cmd, &info, &rc);
	NET_EPOCH_EXIT(et);

	if (error == 0 ||
	    (cmd == RTM_ADD && error == EEXIST) ||
	    (cmd == RTM_DELETE && (error == ENOENT || error == ESRCH)))
		return (error);

	log(LOG_DEBUG, "%s: %s failed for interface %s: %u\n",
		__func__, otype, if_name(ifp), error);

	return (error);
}

int
ifa_add_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{

	return (ifa_maintain_loopback_route(RTM_ADD, "insertion", ifa, ia));
}

int
ifa_del_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{

	return (ifa_maintain_loopback_route(RTM_DELETE, "deletion", ifa, ia));
}

int
ifa_switch_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{

	return (ifa_maintain_loopback_route(RTM_CHANGE, "switch", ifa, ia));
}

static bool
match_kernel_route(const struct rtentry *rt, struct nhop_object *nh)
{
	if (!NH_IS_NHGRP(nh) && (nhop_get_rtflags(nh) & RTF_PINNED) &&
	    nh->nh_aifp->if_fib == nhop_get_fibnum(nh))
		return (true);
	return (false);
}

static int
pick_kernel_route(struct rtentry *rt, void *arg)
{
	struct nhop_object *nh = rt->rt_nhop;
	struct rib_head *rh_dst = (struct rib_head *)arg;

	if (match_kernel_route(rt, nh)) {
		struct rib_cmd_info rc = {};
		struct route_nhop_data rnd = {
			.rnd_nhop = nh,
			.rnd_weight = rt->rt_weight,
		};
		rib_copy_route(rt, &rnd, rh_dst, &rc);
	}
	return (0);
}

/*
 * Tries to copy kernel routes matching pattern from @rh_src to @rh_dst.
 *
 * Note: as this function acquires locks for both @rh_src and @rh_dst,
 *  it needs to be called under RTABLES_LOCK() to avoid deadlocking
 * with multiple ribs.
 */
void
rib_copy_kernel_routes(struct rib_head *rh_src, struct rib_head *rh_dst)
{
	struct epoch_tracker et;

	if (V_rt_add_addr_allfibs == 0)
		return;

	NET_EPOCH_ENTER(et);
	rib_walk_ext_internal(rh_src, false, pick_kernel_route, NULL, rh_dst);
	NET_EPOCH_EXIT(et);
}

