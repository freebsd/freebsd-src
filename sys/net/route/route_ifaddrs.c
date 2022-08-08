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
 *
 *	@(#)route.c	8.3.1.1 (Berkeley) 2/23/95
 * $FreeBSD$
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

static void
report_operation(uint32_t fibnum, struct rib_cmd_info *rc)
{
	struct nhop_object *nh;

	if (rc->rc_cmd == RTM_DELETE)
		nh = nhop_select(rc->rc_nh_old, 0);
	else
		nh = nhop_select(rc->rc_nh_new, 0);
	rt_routemsg(rc->rc_cmd, rc->rc_rt, nh, fibnum);
}

int
rib_add_kernel_px(uint32_t fibnum, struct sockaddr *dst, int plen,
    struct route_nhop_data *rnd, int op_flags)
{
	struct rib_cmd_info rc = {};

	NET_EPOCH_ASSERT();

	int error = rib_add_route_px(fibnum, dst, plen, rnd, op_flags, &rc);
	if (error != 0)
		return (error);
	report_operation(fibnum, &rc);

	if (V_rt_add_addr_allfibs != 0) {
		for (int i = 0; i < V_rt_numfibs; i++) {
			if (i == fibnum)
				continue;
			struct rib_head *rnh = rt_tables_get_rnh(fibnum, dst->sa_family);
			/* Don't care much about the errors in non-primary fib */
			if (rnh != NULL) {
				if (rib_copy_route(rc.rc_rt, rnd, rnh, &rc) == 0)
					report_operation(i, &rc);
			}
		}
	}

	return (error);
}

int
rib_del_kernel_px(uint32_t fibnum, struct sockaddr *dst, int plen,
    rib_filter_f_t *filter_func, void *filter_arg, int op_flags)
{
	struct rib_cmd_info rc = {};

	NET_EPOCH_ASSERT();

	int error = rib_del_route_px(fibnum, dst, plen, filter_func, filter_arg,
	    op_flags, &rc);
	if (error != 0)
		return (error);
	report_operation(fibnum, &rc);

	if (V_rt_add_addr_allfibs != 0) {
		for (int i = 0; i < V_rt_numfibs; i++) {
			if (i == fibnum)
				continue;
			/* Don't care much about the errors in non-primary fib */
			if (rib_del_route_px(fibnum, dst, plen, filter_func, filter_arg,
			    op_flags, &rc) == 0)
				report_operation(i, &rc);
		}
	}

	return (error);
}

static int
add_loopback_route_flags(struct ifaddr *ifa, struct sockaddr *ia, int op_flags)
{
	struct rib_cmd_info rc;
	int error;

	NET_EPOCH_ASSERT();

	struct ifnet *ifp = ifa->ifa_ifp;
	struct nhop_object *nh = nhop_alloc(ifp->if_fib, ia->sa_family);
	struct route_nhop_data rnd = { .rnd_weight = RT_DEFAULT_WEIGHT };
	if (nh == NULL)
		return (ENOMEM);

	nhop_set_direct_gw(nh, ifp);
	nhop_set_transmit_ifp(nh, V_loif);
	nhop_set_src(nh, ifaof_ifpforaddr(ifa->ifa_addr, ifp));
	nhop_set_pinned(nh, true);
	nhop_set_rtflags(nh, RTF_STATIC);
	nhop_set_pxtype_flag(nh, NHF_HOST);
	rnd.rnd_nhop = nhop_get_nhop(nh, &error);
	if (error != 0)
		return (error);
	error = rib_add_route_px(ifp->if_fib, ia, -1, &rnd, op_flags, &rc);

	if (error != 0)
		log(LOG_DEBUG, "%s: failed to update interface %s route: %u\n",
		    __func__, if_name(ifp), error);

	return (error);
}

int
ifa_add_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	int error = add_loopback_route_flags(ifa, ia, RTM_F_CREATE | RTM_F_FORCE);
	NET_EPOCH_EXIT(et);

	return (error);
}

int
ifa_switch_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{
	struct epoch_tracker et;

	NET_EPOCH_ENTER(et);
	int error = add_loopback_route_flags(ifa, ia, RTM_F_REPLACE | RTM_F_FORCE);
	NET_EPOCH_EXIT(et);

	return (error);
}

int
ifa_del_loopback_route(struct ifaddr *ifa, struct sockaddr *ia)
{
	struct ifnet *ifp = ifa->ifa_ifp;
	struct sockaddr_dl link_sdl;
	struct sockaddr *gw = (struct sockaddr *)&link_sdl;
	struct rib_cmd_info rc;
	struct epoch_tracker et;
	int error;

	NET_EPOCH_ENTER(et);

	link_init_sdl(ifp, gw, ifp->if_type);
	error = rib_del_route_px_gw(ifp->if_fib, ia, -1, gw, RTM_F_FORCE, &rc);

	NET_EPOCH_EXIT(et);

	if (error != 0)
		log(LOG_DEBUG, "%s: failed to delete interface %s route: %u\n",
		    __func__,  if_name(ifp), error);

	return (error);
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

