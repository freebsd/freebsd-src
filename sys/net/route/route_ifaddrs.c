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

#include "opt_mpath.h"
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

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
static inline  int
rtinit1(struct ifaddr *ifa, int cmd, int flags, int fibnum)
{
	RIB_RLOCK_TRACKER;
	struct epoch_tracker et;
	struct sockaddr *dst;
	struct sockaddr *netmask;
	struct rib_cmd_info rc;
	struct rt_addrinfo info;
	int error = 0;
	int startfib, endfib;
	struct sockaddr_storage ss;
	int didwork = 0;
	int a_failure = 0;
	struct sockaddr_dl_short sdl;
	struct rib_head *rnh;

	if (flags & RTF_HOST) {
		dst = ifa->ifa_dstaddr;
		netmask = NULL;
	} else {
		dst = ifa->ifa_addr;
		netmask = ifa->ifa_netmask;
	}
	if (dst->sa_len == 0)
		return(EINVAL);
	switch (dst->sa_family) {
	case AF_INET6:
	case AF_INET:
		/* We support multiple FIBs. */
		break;
	default:
		fibnum = RT_DEFAULT_FIB;
		break;
	}
	if (fibnum == RT_ALL_FIBS) {
		if (V_rt_add_addr_allfibs == 0 && cmd == (int)RTM_ADD)
			startfib = endfib = ifa->ifa_ifp->if_fib;
		else {
			startfib = 0;
			endfib = rt_numfibs - 1;
		}
	} else {
		KASSERT((fibnum < rt_numfibs), ("rtinit1: bad fibnum"));
		startfib = fibnum;
		endfib = fibnum;
	}

	/*
	 * If it's a delete, check that if it exists,
	 * it's on the correct interface or we might scrub
	 * a route to another ifa which would
	 * be confusing at best and possibly worse.
	 */
	if (cmd == RTM_DELETE) {
		/*
		 * It's a delete, so it should already exist..
		 * If it's a net, mask off the host bits
		 * (Assuming we have a mask)
		 * XXX this is kinda inet specific..
		 */
		if (netmask != NULL) {
			rt_maskedcopy(dst, (struct sockaddr *)&ss, netmask);
			dst = (struct sockaddr *)&ss;
		}
	}
	bzero(&sdl, sizeof(struct sockaddr_dl_short));
	sdl.sdl_family = AF_LINK;
	sdl.sdl_len = sizeof(struct sockaddr_dl_short);
	sdl.sdl_type = ifa->ifa_ifp->if_type;
	sdl.sdl_index = ifa->ifa_ifp->if_index;
	/*
	 * Now go through all the requested tables (fibs) and do the
	 * requested action. Realistically, this will either be fib 0
	 * for protocols that don't do multiple tables or all the
	 * tables for those that do.
	 */
	for ( fibnum = startfib; fibnum <= endfib; fibnum++) {
		if (cmd == RTM_DELETE) {
			struct radix_node *rn;
			/*
			 * Look up an rtentry that is in the routing tree and
			 * contains the correct info.
			 */
			rnh = rt_tables_get_rnh(fibnum, dst->sa_family);
			if (rnh == NULL)
				/* this table doesn't exist but others might */
				continue;
			RIB_RLOCK(rnh);
			rn = rnh->rnh_lookup(dst, netmask, &rnh->head);
			error = (rn == NULL ||
			    (rn->rn_flags & RNF_ROOT) ||
			    RNTORT(rn)->rt_nhop->nh_ifa != ifa);
			RIB_RUNLOCK(rnh);
			if (error) {
				/* this is only an error if bad on ALL tables */
				continue;
			}
		}
		/*
		 * Do the actual request
		 */
		bzero((caddr_t)&info, sizeof(info));
		info.rti_ifa = ifa;
		info.rti_flags = flags |
		    (ifa->ifa_flags & ~IFA_RTSELF) | RTF_PINNED;
		info.rti_info[RTAX_DST] = dst;
		info.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&sdl;
		info.rti_info[RTAX_NETMASK] = netmask;
		NET_EPOCH_ENTER(et);
		error = rib_action(fibnum, cmd, &info, &rc);
		if (error == 0 && rc.rc_rt != NULL) {
			/*
			 * notify any listening routing agents of the change
			 */

			/* TODO: interface routes/aliases */
			rt_newaddrmsg_fib(cmd, ifa, rc.rc_rt, fibnum);
			didwork = 1;
		}
		NET_EPOCH_EXIT(et);
		if (error)
			a_failure = error;
	}
	if (cmd == RTM_DELETE) {
		if (didwork) {
			error = 0;
		} else {
			/* we only give an error if it wasn't in any table */
			error = ((flags & RTF_HOST) ?
			    EHOSTUNREACH : ENETUNREACH);
		}
	} else {
		if (a_failure) {
			/* return an error if any of them failed */
			error = a_failure;
		}
	}
	return (error);
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	struct sockaddr *dst;
	int fib = RT_DEFAULT_FIB;

	if (flags & RTF_HOST) {
		dst = ifa->ifa_dstaddr;
	} else {
		dst = ifa->ifa_addr;
	}

	switch (dst->sa_family) {
	case AF_INET6:
	case AF_INET:
		/* We do support multiple FIBs. */
		fib = RT_ALL_FIBS;
		break;
	}
	return (rtinit1(ifa, cmd, flags, fib));
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
	struct ifaddr *rti_ifa = NULL;

	ifp = ifa->ifa_ifp;

	NET_EPOCH_ENTER(et);
	bzero(&info, sizeof(info));
	if (cmd != RTM_DELETE)
		info.rti_ifp = V_loif;
	if (cmd == RTM_ADD) {
		/* explicitly specify (loopback) ifa */
		if (info.rti_ifp != NULL) {
			rti_ifa = ifaof_ifpforaddr(ifa->ifa_addr, info.rti_ifp);
			if (rti_ifa != NULL)
				ifa_ref(rti_ifa);
			info.rti_ifa = rti_ifa;
		}
	}
	info.rti_flags = ifa->ifa_flags | RTF_HOST | RTF_STATIC | RTF_PINNED;
	info.rti_info[RTAX_DST] = ia;
	info.rti_info[RTAX_GATEWAY] = (struct sockaddr *)&null_sdl;
	link_init_sdl(ifp, (struct sockaddr *)&null_sdl, ifp->if_type);

	error = rib_action(ifp->if_fib, cmd, &info, &rc);
	NET_EPOCH_EXIT(et);

	if (rti_ifa != NULL)
		ifa_free(rti_ifa);

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


