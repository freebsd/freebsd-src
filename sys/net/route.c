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
/************************************************************************
 * Note: In this file a 'fib' is a "forwarding information base"	*
 * Which is the new name for an in kernel routing (next hop) table.	*
 ***********************************************************************/

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_mrouting.h"
#include "opt_mpath.h"
#include "opt_route.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/eventhandler.h>
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
#include <net/route/shared.h>
#include <net/vnet.h>

#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/ip_mroute.h>

#include <vm/uma.h>

#define	RT_MAXFIBS	UINT16_MAX

/* Kernel config default option. */
#ifdef ROUTETABLES
#if ROUTETABLES <= 0
#error "ROUTETABLES defined too low"
#endif
#if ROUTETABLES > RT_MAXFIBS
#error "ROUTETABLES defined too big"
#endif
#define	RT_NUMFIBS	ROUTETABLES
#endif /* ROUTETABLES */
/* Initialize to default if not otherwise set. */
#ifndef	RT_NUMFIBS
#define	RT_NUMFIBS	1
#endif

/* This is read-only.. */
u_int rt_numfibs = RT_NUMFIBS;
SYSCTL_UINT(_net, OID_AUTO, fibs, CTLFLAG_RDTUN, &rt_numfibs, 0, "");

/*
 * By default add routes to all fibs for new interfaces.
 * Once this is set to 0 then only allocate routes on interface
 * changes for the FIB of the caller when adding a new set of addresses
 * to an interface.  XXX this is a shotgun aproach to a problem that needs
 * a more fine grained solution.. that will come.
 * XXX also has the problems getting the FIB from curthread which will not
 * always work given the fib can be overridden and prefixes can be added
 * from the network stack context.
 */
VNET_DEFINE(u_int, rt_add_addr_allfibs) = 1;
SYSCTL_UINT(_net, OID_AUTO, add_addr_allfibs, CTLFLAG_RWTUN | CTLFLAG_VNET,
    &VNET_NAME(rt_add_addr_allfibs), 0, "");

VNET_PCPUSTAT_DEFINE(struct rtstat, rtstat);

VNET_PCPUSTAT_SYSINIT(rtstat);
#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(rtstat);
#endif

VNET_DEFINE(struct rib_head *, rt_tables);
#define	V_rt_tables	VNET(rt_tables)


EVENTHANDLER_LIST_DEFINE(rt_addrmsg);

static int rt_ifdelroute(const struct rtentry *rt, const struct nhop_object *,
    void *arg);
static int rt_exportinfo(struct rtentry *rt, struct rt_addrinfo *info,
    int flags);

/*
 * handler for net.my_fibnum
 */
static int
sysctl_my_fibnum(SYSCTL_HANDLER_ARGS)
{
        int fibnum;
        int error;
 
        fibnum = curthread->td_proc->p_fibnum;
        error = sysctl_handle_int(oidp, &fibnum, 0, req);
        return (error);
}

SYSCTL_PROC(_net, OID_AUTO, my_fibnum,
    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, 0,
    &sysctl_my_fibnum, "I",
    "default FIB of caller");

static __inline struct rib_head **
rt_tables_get_rnh_ptr(int table, int fam)
{
	struct rib_head **rnh;

	KASSERT(table >= 0 && table < rt_numfibs,
	    ("%s: table out of bounds (0 <= %d < %d)", __func__, table,
	     rt_numfibs));
	KASSERT(fam >= 0 && fam < (AF_MAX + 1),
	    ("%s: fam out of bounds (0 <= %d < %d)", __func__, fam, AF_MAX+1));

	/* rnh is [fib=0][af=0]. */
	rnh = (struct rib_head **)V_rt_tables;
	/* Get the offset to the requested table and fam. */
	rnh += table * (AF_MAX+1) + fam;

	return (rnh);
}

struct rib_head *
rt_tables_get_rnh(int table, int fam)
{

	return (*rt_tables_get_rnh_ptr(table, fam));
}

u_int
rt_tables_get_gen(int table, int fam)
{
	struct rib_head *rnh;

	rnh = *rt_tables_get_rnh_ptr(table, fam);
	KASSERT(rnh != NULL, ("%s: NULL rib_head pointer table %d fam %d",
	    __func__, table, fam));
	return (rnh->rnh_gen);
}


/*
 * route initialization must occur before ip6_init2(), which happenas at
 * SI_ORDER_MIDDLE.
 */
static void
route_init(void)
{

	/* whack the tunable ints into  line. */
	if (rt_numfibs > RT_MAXFIBS)
		rt_numfibs = RT_MAXFIBS;
	if (rt_numfibs == 0)
		rt_numfibs = 1;
	nhops_init();
}
SYSINIT(route_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, route_init, NULL);

static void
vnet_route_init(const void *unused __unused)
{
	struct domain *dom;
	struct rib_head **rnh;
	int table;
	int fam;

	V_rt_tables = malloc(rt_numfibs * (AF_MAX+1) *
	    sizeof(struct rib_head *), M_RTABLE, M_WAITOK|M_ZERO);

	vnet_rtzone_init();
	for (dom = domains; dom; dom = dom->dom_next) {
		if (dom->dom_rtattach == NULL)
			continue;

		for  (table = 0; table < rt_numfibs; table++) {
			fam = dom->dom_family;
			if (table != 0 && fam != AF_INET6 && fam != AF_INET)
				break;

			rnh = rt_tables_get_rnh_ptr(table, fam);
			if (rnh == NULL)
				panic("%s: rnh NULL", __func__);
			*rnh = dom->dom_rtattach(table);
		}
	}
}
VNET_SYSINIT(vnet_route_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_FOURTH,
    vnet_route_init, 0);

#ifdef VIMAGE
static void
vnet_route_uninit(const void *unused __unused)
{
	int table;
	int fam;
	struct domain *dom;
	struct rib_head **rnh;

	for (dom = domains; dom; dom = dom->dom_next) {
		if (dom->dom_rtdetach == NULL)
			continue;

		for (table = 0; table < rt_numfibs; table++) {
			fam = dom->dom_family;

			if (table != 0 && fam != AF_INET6 && fam != AF_INET)
				break;

			rnh = rt_tables_get_rnh_ptr(table, fam);
			if (rnh == NULL)
				panic("%s: rnh NULL", __func__);
			dom->dom_rtdetach(*rnh);
		}
	}

	/*
	 * dom_rtdetach calls rt_table_destroy(), which
	 *  schedules deletion for all rtentries, nexthops and control
	 *  structures. Wait for the destruction callbacks to fire.
	 * Note that this should result in freeing all rtentries, but
	 *  nexthops deletions will be scheduled for the next epoch run
	 *  and will be completed after vnet teardown.
	 */
	epoch_drain_callbacks(net_epoch_preempt);

	free(V_rt_tables, M_RTABLE);
	vnet_rtzone_destroy();
}
VNET_SYSUNINIT(vnet_route_uninit, SI_SUB_PROTO_DOMAIN, SI_ORDER_FIRST,
    vnet_route_uninit, 0);
#endif

struct rib_head *
rt_table_init(int offset, int family, u_int fibnum)
{
	struct rib_head *rh;

	rh = malloc(sizeof(struct rib_head), M_RTABLE, M_WAITOK | M_ZERO);

	/* TODO: These details should be hidded inside radix.c */
	/* Init masks tree */
	rn_inithead_internal(&rh->head, rh->rnh_nodes, offset);
	rn_inithead_internal(&rh->rmhead.head, rh->rmhead.mask_nodes, 0);
	rh->head.rnh_masks = &rh->rmhead;

	/* Save metadata associated with this routing table. */
	rh->rib_family = family;
	rh->rib_fibnum = fibnum;
#ifdef VIMAGE
	rh->rib_vnet = curvnet;
#endif

	tmproutes_init(rh);

	/* Init locks */
	RIB_LOCK_INIT(rh);

	nhops_init_rib(rh);

	/* Init subscription system */
	rib_init_subscriptions(rh);

	/* Finally, set base callbacks */
	rh->rnh_addaddr = rn_addroute;
	rh->rnh_deladdr = rn_delete;
	rh->rnh_matchaddr = rn_match;
	rh->rnh_lookup = rn_lookup;
	rh->rnh_walktree = rn_walktree;
	rh->rnh_walktree_from = rn_walktree_from;

	return (rh);
}

static int
rt_freeentry(struct radix_node *rn, void *arg)
{
	struct radix_head * const rnh = arg;
	struct radix_node *x;

	x = (struct radix_node *)rn_delete(rn + 2, NULL, rnh);
	if (x != NULL)
		R_Free(x);
	return (0);
}

void
rt_table_destroy(struct rib_head *rh)
{

	tmproutes_destroy(rh);

	rn_walktree(&rh->rmhead.head, rt_freeentry, &rh->rmhead.head);

	nhops_destroy_rib(rh);

	rib_destroy_subscriptions(rh);

	/* Assume table is already empty */
	RIB_LOCK_DESTROY(rh);
	free(rh, M_RTABLE);
}


#ifndef _SYS_SYSPROTO_H_
struct setfib_args {
	int     fibnum;
};
#endif
int
sys_setfib(struct thread *td, struct setfib_args *uap)
{
	if (uap->fibnum < 0 || uap->fibnum >= rt_numfibs)
		return EINVAL;
	td->td_proc->p_fibnum = uap->fibnum;
	return (0);
}

/*
 * Adds a temporal redirect entry to the routing table.
 * @fibnum: fib number
 * @dst: destination to install redirect to
 * @gateway: gateway to go via
 * @author: sockaddr of originating router, can be NULL
 * @ifp: interface to use for the redirected route
 * @flags: set of flags to add. Allowed: RTF_GATEWAY
 * @lifetime_sec: time in seconds to expire this redirect.
 *
 * Retuns 0 on success, errno otherwise.
 */
int
rib_add_redirect(u_int fibnum, struct sockaddr *dst, struct sockaddr *gateway,
    struct sockaddr *author, struct ifnet *ifp, int flags, int lifetime_sec)
{
	struct rib_cmd_info rc;
	int error;
	struct rt_addrinfo info;
	struct rt_metrics rti_rmx;
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();

	if (rt_tables_get_rnh(fibnum, dst->sa_family) == NULL)
		return (EAFNOSUPPORT);

	/* Verify the allowed flag mask. */
	KASSERT(((flags & ~(RTF_GATEWAY)) == 0),
	    ("invalid redirect flags: %x", flags));

	/* Get the best ifa for the given interface and gateway. */
	if ((ifa = ifaof_ifpforaddr(gateway, ifp)) == NULL)
		return (ENETUNREACH);
	ifa_ref(ifa);
	
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_ifa = ifa;
	info.rti_ifp = ifp;
	info.rti_flags = flags | RTF_HOST | RTF_DYNAMIC;

	/* Setup route metrics to define expire time. */
	bzero(&rti_rmx, sizeof(rti_rmx));
	/* Set expire time as absolute. */
	rti_rmx.rmx_expire = lifetime_sec + time_second;
	info.rti_mflags |= RTV_EXPIRE;
	info.rti_rmx = &rti_rmx;

	error = rib_action(fibnum, RTM_ADD, &info, &rc);
	ifa_free(ifa);

	if (error != 0) {
		/* TODO: add per-fib redirect stats. */
		return (error);
	}

	RT_LOCK(rc.rc_rt);
	flags = rc.rc_rt->rt_flags;
	RT_UNLOCK(rc.rc_rt);

	RTSTAT_INC(rts_dynamic);

	/* Send notification of a route addition to userland. */
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_AUTHOR] = author;
	rt_missmsg_fib(RTM_REDIRECT, &info, flags, error, fibnum);

	return (0);
}

/*
 * Routing table ioctl interface.
 */
int
rtioctl_fib(u_long req, caddr_t data, u_int fibnum)
{

	/*
	 * If more ioctl commands are added here, make sure the proper
	 * super-user checks are being performed because it is possible for
	 * prison-root to make it this far if raw sockets have been enabled
	 * in jails.
	 */
#ifdef INET
	/* Multicast goop, grrr... */
	return mrt_ioctl ? mrt_ioctl(req, data, fibnum) : EOPNOTSUPP;
#else /* INET */
	return ENXIO;
#endif /* INET */
}

struct ifaddr *
ifa_ifwithroute(int flags, const struct sockaddr *dst,
    const struct sockaddr *gateway, u_int fibnum)
{
	struct ifaddr *ifa;

	NET_EPOCH_ASSERT();
	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = NULL;
		if (flags & RTF_HOST)
			ifa = ifa_ifwithdstaddr(dst, fibnum);
		if (ifa == NULL)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway, fibnum);
	}
	if (ifa == NULL)
		ifa = ifa_ifwithnet(gateway, 0, fibnum);
	if (ifa == NULL) {
		struct nhop_object *nh;

		nh = rib_lookup(fibnum, gateway, NHR_NONE, 0);

		/*
		 * dismiss a gateway that is reachable only
		 * through the default router
		 */
		if ((nh == NULL) || (nh->nh_flags & NHF_DEFAULT))
			return (NULL);
		ifa = nh->nh_ifa;
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == NULL)
			ifa = oifa;
	}

	return (ifa);
}


/*
 * Copy most of @rt data into @info.
 *
 * If @flags contains NHR_COPY, copies dst,netmask and gw to the
 * pointers specified by @info structure. Assume such pointers
 * are zeroed sockaddr-like structures with sa_len field initialized
 * to reflect size of the provided buffer. if no NHR_COPY is specified,
 * point dst,netmask and gw @info fields to appropriate @rt values.
 *
 * if @flags contains NHR_REF, do refcouting on rt_ifp and rt_ifa.
 *
 * Returns 0 on success.
 */
int
rt_exportinfo(struct rtentry *rt, struct rt_addrinfo *info, int flags)
{
	struct rt_metrics *rmx;
	struct sockaddr *src, *dst;
	struct nhop_object *nh;
	int sa_len;

	if (flags & NHR_COPY) {
		/* Copy destination if dst is non-zero */
		src = rt_key(rt);
		dst = info->rti_info[RTAX_DST];
		sa_len = src->sa_len;
		if (dst != NULL) {
			if (src->sa_len > dst->sa_len)
				return (ENOMEM);
			memcpy(dst, src, src->sa_len);
			info->rti_addrs |= RTA_DST;
		}

		/* Copy mask if set && dst is non-zero */
		src = rt_mask(rt);
		dst = info->rti_info[RTAX_NETMASK];
		if (src != NULL && dst != NULL) {

			/*
			 * Radix stores different value in sa_len,
			 * assume rt_mask() to have the same length
			 * as rt_key()
			 */
			if (sa_len > dst->sa_len)
				return (ENOMEM);
			memcpy(dst, src, src->sa_len);
			info->rti_addrs |= RTA_NETMASK;
		}

		/* Copy gateway is set && dst is non-zero */
		src = &rt->rt_nhop->gw_sa;
		dst = info->rti_info[RTAX_GATEWAY];
		if ((rt->rt_flags & RTF_GATEWAY) && src != NULL && dst != NULL){
			if (src->sa_len > dst->sa_len)
				return (ENOMEM);
			memcpy(dst, src, src->sa_len);
			info->rti_addrs |= RTA_GATEWAY;
		}
	} else {
		info->rti_info[RTAX_DST] = rt_key(rt);
		info->rti_addrs |= RTA_DST;
		if (rt_mask(rt) != NULL) {
			info->rti_info[RTAX_NETMASK] = rt_mask(rt);
			info->rti_addrs |= RTA_NETMASK;
		}
		if (rt->rt_flags & RTF_GATEWAY) {
			info->rti_info[RTAX_GATEWAY] = &rt->rt_nhop->gw_sa;
			info->rti_addrs |= RTA_GATEWAY;
		}
	}

	nh = rt->rt_nhop;
	rmx = info->rti_rmx;
	if (rmx != NULL) {
		info->rti_mflags |= RTV_MTU;
		rmx->rmx_mtu = nh->nh_mtu;
	}

	info->rti_flags = rt->rt_flags | nhop_get_rtflags(nh);
	info->rti_ifp = nh->nh_ifp;
	info->rti_ifa = nh->nh_ifa;
	if (flags & NHR_REF) {
		if_ref(info->rti_ifp);
		ifa_ref(info->rti_ifa);
	}

	return (0);
}

/*
 * Lookups up route entry for @dst in RIB database for fib @fibnum.
 * Exports entry data to @info using rt_exportinfo().
 *
 * If @flags contains NHR_REF, refcouting is performed on rt_ifp and rt_ifa.
 * All references can be released later by calling rib_free_info().
 *
 * Returns 0 on success.
 * Returns ENOENT for lookup failure, ENOMEM for export failure.
 */
int
rib_lookup_info(uint32_t fibnum, const struct sockaddr *dst, uint32_t flags,
    uint32_t flowid, struct rt_addrinfo *info)
{
	RIB_RLOCK_TRACKER;
	struct rib_head *rh;
	struct radix_node *rn;
	struct rtentry *rt;
	int error;

	KASSERT((fibnum < rt_numfibs), ("rib_lookup_rte: bad fibnum"));
	rh = rt_tables_get_rnh(fibnum, dst->sa_family);
	if (rh == NULL)
		return (ENOENT);

	RIB_RLOCK(rh);
	rn = rh->rnh_matchaddr(__DECONST(void *, dst), &rh->head);
	if (rn != NULL && ((rn->rn_flags & RNF_ROOT) == 0)) {
		rt = RNTORT(rn);
		/* Ensure route & ifp is UP */
		if (RT_LINK_IS_UP(rt->rt_nhop->nh_ifp)) {
			flags = (flags & NHR_REF) | NHR_COPY;
			error = rt_exportinfo(rt, info, flags);
			RIB_RUNLOCK(rh);

			return (error);
		}
	}
	RIB_RUNLOCK(rh);

	return (ENOENT);
}

/*
 * Releases all references acquired by rib_lookup_info() when
 * called with NHR_REF flags.
 */
void
rib_free_info(struct rt_addrinfo *info)
{

	ifa_free(info->rti_ifa);
	if_rele(info->rti_ifp);
}

/*
 * Iterates over all existing fibs in system calling
 *  @setwa_f function prior to traversing each fib.
 *  Calls @wa_f function for each element in current fib.
 * If af is not AF_UNSPEC, iterates over fibs in particular
 * address family.
 */
void
rt_foreach_fib_walk(int af, rt_setwarg_t *setwa_f, rt_walktree_f_t *wa_f,
    void *arg)
{
	struct rib_head *rnh;
	uint32_t fibnum;
	int i;

	for (fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		/* Do we want some specific family? */
		if (af != AF_UNSPEC) {
			rnh = rt_tables_get_rnh(fibnum, af);
			if (rnh == NULL)
				continue;
			if (setwa_f != NULL)
				setwa_f(rnh, fibnum, af, arg);

			RIB_WLOCK(rnh);
			rnh->rnh_walktree(&rnh->head, (walktree_f_t *)wa_f,arg);
			RIB_WUNLOCK(rnh);
			continue;
		}

		for (i = 1; i <= AF_MAX; i++) {
			rnh = rt_tables_get_rnh(fibnum, i);
			if (rnh == NULL)
				continue;
			if (setwa_f != NULL)
				setwa_f(rnh, fibnum, i, arg);

			RIB_WLOCK(rnh);
			rnh->rnh_walktree(&rnh->head, (walktree_f_t *)wa_f,arg);
			RIB_WUNLOCK(rnh);
		}
	}
}

/*
 * Iterates over all existing fibs in system and deletes each element
 *  for which @filter_f function returns non-zero value.
 * If @family is not AF_UNSPEC, iterates over fibs in particular
 * address family.
 */
void
rt_foreach_fib_walk_del(int family, rt_filter_f_t *filter_f, void *arg)
{
	u_int fibnum;
	int i, start, end;

	for (fibnum = 0; fibnum < rt_numfibs; fibnum++) {
		/* Do we want some specific family? */
		if (family != AF_UNSPEC) {
			start = family;
			end = family;
		} else {
			start = 1;
			end = AF_MAX;
		}

		for (i = start; i <= end; i++) {
			if (rt_tables_get_rnh(fibnum, i) == NULL)
				continue;

			rib_walk_del(fibnum, i, filter_f, arg, 0);
		}
	}
}

/*
 * Delete Routes for a Network Interface
 *
 * Called for each routing entry via the rnh->rnh_walktree() call above
 * to delete all route entries referencing a detaching network interface.
 *
 * Arguments:
 *	rt	pointer to rtentry
 *	nh	pointer to nhop
 *	arg	argument passed to rnh->rnh_walktree() - detaching interface
 *
 * Returns:
 *	0	successful
 *	errno	failed - reason indicated
 */
static int
rt_ifdelroute(const struct rtentry *rt, const struct nhop_object *nh, void *arg)
{
	struct ifnet	*ifp = arg;

	if (nh->nh_ifp != ifp)
		return (0);

	/*
	 * Protect (sorta) against walktree recursion problems
	 * with cloned routes
	 */
	if ((rt->rt_flags & RTF_UP) == 0)
		return (0);

	return (1);
}

/*
 * Delete all remaining routes using this interface
 * Unfortuneatly the only way to do this is to slog through
 * the entire routing table looking for routes which point
 * to this interface...oh well...
 */
void
rt_flushifroutes_af(struct ifnet *ifp, int af)
{
	KASSERT((af >= 1 && af <= AF_MAX), ("%s: af %d not >= 1 and <= %d",
	    __func__, af, AF_MAX));

	rt_foreach_fib_walk_del(af, rt_ifdelroute, ifp);
}

void
rt_flushifroutes(struct ifnet *ifp)
{

	rt_foreach_fib_walk_del(AF_UNSPEC, rt_ifdelroute, ifp);
}

/*
 * Look up rt_addrinfo for a specific fib.  Note that if rti_ifa is defined,
 * it will be referenced so the caller must free it.
 *
 * Assume basic consistency checks are executed by callers:
 * RTAX_DST exists, if RTF_GATEWAY is set, RTAX_GATEWAY exists as well.
 */
int
rt_getifa_fib(struct rt_addrinfo *info, u_int fibnum)
{
	const struct sockaddr *dst, *gateway, *ifpaddr, *ifaaddr;
	struct epoch_tracker et;
	int needref, error, flags;

	dst = info->rti_info[RTAX_DST];
	gateway = info->rti_info[RTAX_GATEWAY];
	ifpaddr = info->rti_info[RTAX_IFP];
	ifaaddr = info->rti_info[RTAX_IFA];
	flags = info->rti_flags;

	/*
	 * ifp may be specified by sockaddr_dl
	 * when protocol address is ambiguous.
	 */
	error = 0;
	needref = (info->rti_ifa == NULL);
	NET_EPOCH_ENTER(et);

	/* If we have interface specified by the ifindex in the address, use it */
	if (info->rti_ifp == NULL && ifpaddr != NULL &&
	    ifpaddr->sa_family == AF_LINK) {
	    const struct sockaddr_dl *sdl = (const struct sockaddr_dl *)ifpaddr;
	    if (sdl->sdl_index != 0)
		    info->rti_ifp = ifnet_byindex(sdl->sdl_index);
	}
	/*
	 * If we have source address specified, try to find it
	 * TODO: avoid enumerating all ifas on all interfaces.
	 */
	if (info->rti_ifa == NULL && ifaaddr != NULL)
		info->rti_ifa = ifa_ifwithaddr(ifaaddr);
	if (info->rti_ifa == NULL) {
		const struct sockaddr *sa;

		/*
		 * Most common use case for the userland-supplied routes.
		 *
		 * Choose sockaddr to select ifa.
		 * -- if ifp is set --
		 * Order of preference:
		 * 1) IFA address
		 * 2) gateway address
		 *   Note: for interface routes link-level gateway address 
		 *     is specified to indicate the interface index without
		 *     specifying RTF_GATEWAY. In this case, ignore gateway
		 *   Note: gateway AF may be different from dst AF. In this case,
		 *   ignore gateway
		 * 3) final destination.
		 * 4) if all of these fails, try to get at least link-level ifa.
		 * -- else --
		 * try to lookup gateway or dst in the routing table to get ifa
		 */
		if (info->rti_info[RTAX_IFA] != NULL)
			sa = info->rti_info[RTAX_IFA];
		else if ((info->rti_flags & RTF_GATEWAY) != 0 &&
		    gateway->sa_family == dst->sa_family)
			sa = gateway;
		else
			sa = dst;
		if (info->rti_ifp != NULL) {
			info->rti_ifa = ifaof_ifpforaddr(sa, info->rti_ifp);
			/* Case 4 */
			if (info->rti_ifa == NULL && gateway != NULL)
				info->rti_ifa = ifaof_ifpforaddr(gateway, info->rti_ifp);
		} else if (dst != NULL && gateway != NULL)
			info->rti_ifa = ifa_ifwithroute(flags, dst, gateway,
							fibnum);
		else if (sa != NULL)
			info->rti_ifa = ifa_ifwithroute(flags, sa, sa,
							fibnum);
	}
	if (needref && info->rti_ifa != NULL) {
		if (info->rti_ifp == NULL)
			info->rti_ifp = info->rti_ifa->ifa_ifp;
		ifa_ref(info->rti_ifa);
	} else
		error = ENETUNREACH;
	NET_EPOCH_EXIT(et);
	return (error);
}

void
rt_updatemtu(struct ifnet *ifp)
{
	struct rib_head *rnh;
	int mtu;
	int i, j;

	/*
	 * Try to update rt_mtu for all routes using this interface
	 * Unfortunately the only way to do this is to traverse all
	 * routing tables in all fibs/domains.
	 */
	for (i = 1; i <= AF_MAX; i++) {
		mtu = if_getmtu_family(ifp, i);
		for (j = 0; j < rt_numfibs; j++) {
			rnh = rt_tables_get_rnh(j, i);
			if (rnh == NULL)
				continue;
			nhops_update_ifmtu(rnh, ifp, mtu);
		}
	}
}


#if 0
int p_sockaddr(char *buf, int buflen, struct sockaddr *s);
int rt_print(char *buf, int buflen, struct rtentry *rt);

int
p_sockaddr(char *buf, int buflen, struct sockaddr *s)
{
	void *paddr = NULL;

	switch (s->sa_family) {
	case AF_INET:
		paddr = &((struct sockaddr_in *)s)->sin_addr;
		break;
	case AF_INET6:
		paddr = &((struct sockaddr_in6 *)s)->sin6_addr;
		break;
	}

	if (paddr == NULL)
		return (0);

	if (inet_ntop(s->sa_family, paddr, buf, buflen) == NULL)
		return (0);
	
	return (strlen(buf));
}

int
rt_print(char *buf, int buflen, struct rtentry *rt)
{
	struct sockaddr *addr, *mask;
	int i = 0;

	addr = rt_key(rt);
	mask = rt_mask(rt);

	i = p_sockaddr(buf, buflen, addr);
	if (!(rt->rt_flags & RTF_HOST)) {
		buf[i++] = '/';
		i += p_sockaddr(buf + i, buflen - i, mask);
	}

	if (rt->rt_flags & RTF_GATEWAY) {
		buf[i++] = '>';
		i += p_sockaddr(buf + i, buflen - i, &rt->rt_nhop->gw_sa);
	}

	return (i);
}
#endif

#ifdef RADIX_MPATH
/*
 * Deletes key for single-path routes, unlinks rtentry with
 * gateway specified in @info from multi-path routes.
 *
 * Returnes unlinked entry. In case of failure, returns NULL
 * and sets @perror to ESRCH.
 */
struct radix_node *
rt_mpath_unlink(struct rib_head *rnh, struct rt_addrinfo *info,
    struct rtentry *rto, int *perror)
{
	/*
	 * if we got multipath routes, we require users to specify
	 * a matching RTAX_GATEWAY.
	 */
	struct rtentry *rt; // *rto = NULL;
	struct radix_node *rn;
	struct sockaddr *gw;

	gw = info->rti_info[RTAX_GATEWAY];
	rt = rt_mpath_matchgate(rto, gw);
	if (rt == NULL) {
		*perror = ESRCH;
		return (NULL);
	}

	/*
	 * this is the first entry in the chain
	 */
	if (rto == rt) {
		rn = rn_mpath_next((struct radix_node *)rt);
		/*
		 * there is another entry, now it's active
		 */
		if (rn) {
			rto = RNTORT(rn);
			RT_LOCK(rto);
			rto->rt_flags |= RTF_UP;
			RT_UNLOCK(rto);
		} else if (rt->rt_flags & RTF_GATEWAY) {
			/*
			 * For gateway routes, we need to 
			 * make sure that we we are deleting
			 * the correct gateway. 
			 * rt_mpath_matchgate() does not 
			 * check the case when there is only
			 * one route in the chain.  
			 */
			if (gw &&
			    (rt->rt_nhop->gw_sa.sa_len != gw->sa_len ||
				memcmp(&rt->rt_nhop->gw_sa, gw, gw->sa_len))) {
				*perror = ESRCH;
				return (NULL);
			}
		}

		/*
		 * use the normal delete code to remove
		 * the first entry
		 */
		rn = rnh->rnh_deladdr(info->rti_info[RTAX_DST],
					info->rti_info[RTAX_NETMASK],
					&rnh->head);
		if (rn != NULL) {
			*perror = 0;
		} else {
			*perror = ESRCH;
		}
		return (rn);
	}
		
	/*
	 * if the entry is 2nd and on up
	 */
	if (rt_mpath_deldup(rto, rt) == 0)
		panic ("rtrequest1: rt_mpath_deldup");
	*perror = 0;
	rn = (struct radix_node *)rt;
	return (rn);
}
#endif

void
rt_setmetrics(const struct rt_addrinfo *info, struct rtentry *rt)
{

	if (info->rti_mflags & RTV_WEIGHT)
		rt->rt_weight = info->rti_rmx->rmx_weight;
	/* Kernel -> userland timebase conversion. */
	if (info->rti_mflags & RTV_EXPIRE)
		rt->rt_expire = info->rti_rmx->rmx_expire ?
		    info->rti_rmx->rmx_expire - time_second + time_uptime : 0;
}

void
rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst, struct sockaddr *netmask)
{
	u_char *cp1 = (u_char *)src;
	u_char *cp2 = (u_char *)dst;
	u_char *cp3 = (u_char *)netmask;
	u_char *cplim = cp2 + *cp3;
	u_char *cplim2 = cp2 + *cp1;

	*cp2++ = *cp1++; *cp2++ = *cp1++; /* copies sa_len & sa_family */
	cp3 += 2;
	if (cplim > cplim2)
		cplim = cplim2;
	while (cp2 < cplim)
		*cp2++ = *cp1++ & *cp3++;
	if (cp2 < cplim2)
		bzero((caddr_t)cp2, (unsigned)(cplim2 - cp2));
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
#define _SOCKADDR_TMPSIZE 128 /* Not too big.. kernel stack size is limited */
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
	char tempbuf[_SOCKADDR_TMPSIZE];
	int didwork = 0;
	int a_failure = 0;
	struct sockaddr_dl_short *sdl = NULL;
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
			rt_maskedcopy(dst, (struct sockaddr *)tempbuf, netmask);
			dst = (struct sockaddr *)tempbuf;
		}
	} else if (cmd == RTM_ADD) {
		sdl = (struct sockaddr_dl_short *)tempbuf;
		bzero(sdl, sizeof(struct sockaddr_dl_short));
		sdl->sdl_family = AF_LINK;
		sdl->sdl_len = sizeof(struct sockaddr_dl_short);
		sdl->sdl_type = ifa->ifa_ifp->if_type;
		sdl->sdl_index = ifa->ifa_ifp->if_index;
        }
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
#ifdef RADIX_MPATH
			if (rt_mpath_capable(rnh)) {

				if (rn == NULL) 
					error = ESRCH;
				else {
					struct rtentry *rt = RNTORT(rn);
					/*
					 * for interface route the gateway
					 * gateway is sockaddr_dl, so
					 * rt_mpath_matchgate must use the
					 * interface address
					 */
					rt = rt_mpath_matchgate(rt,
					    ifa->ifa_addr);
					if (rt == NULL) 
						error = ESRCH;
				}
			}
#endif
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
		/* 
		 * doing this for compatibility reasons
		 */
		if (cmd == RTM_ADD)
			info.rti_info[RTAX_GATEWAY] = (struct sockaddr *)sdl;
		else
			info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
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

/*
 * Announce interface address arrival/withdraw
 * Returns 0 on success.
 */
int
rt_addrmsg(int cmd, struct ifaddr *ifa, int fibnum)
{

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
	    ("unexpected cmd %d", cmd));
	KASSERT(fibnum == RT_ALL_FIBS || (fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	EVENTHANDLER_DIRECT_INVOKE(rt_addrmsg, ifa, cmd);
	return (rtsock_addrmsg(cmd, ifa, fibnum));
}

/*
 * Announce kernel-originated route addition/removal to rtsock based on @rt data.
 * cmd: RTM_ cmd
 * @rt: valid rtentry
 * @ifp: target route interface
 * @fibnum: fib id or RT_ALL_FIBS
 *
 * Returns 0 on success.
 */
int
rt_routemsg(int cmd, struct rtentry *rt, struct ifnet *ifp, int rti_addrs,
    int fibnum)
{

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
	    ("unexpected cmd %d", cmd));
	
	KASSERT(fibnum == RT_ALL_FIBS || (fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	KASSERT(rt_key(rt) != NULL, (":%s: rt_key must be supplied", __func__));

	return (rtsock_routemsg(cmd, rt, ifp, 0, fibnum));
}

/*
 * Announce kernel-originated route addition/removal to rtsock based on @rt data.
 * cmd: RTM_ cmd
 * @info: addrinfo structure with valid data.
 * @fibnum: fib id or RT_ALL_FIBS
 *
 * Returns 0 on success.
 */
int
rt_routemsg_info(int cmd, struct rt_addrinfo *info, int fibnum)
{

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE || cmd == RTM_CHANGE,
	    ("unexpected cmd %d", cmd));
	
	KASSERT(fibnum == RT_ALL_FIBS || (fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	KASSERT(info->rti_info[RTAX_DST] != NULL, (":%s: RTAX_DST must be supplied", __func__));

	return (rtsock_routemsg_info(cmd, info, fibnum));
}


/*
 * This is called to generate messages from the routing socket
 * indicating a network interface has had addresses associated with it.
 */
void
rt_newaddrmsg_fib(int cmd, struct ifaddr *ifa, struct rtentry *rt, int fibnum)
{

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
		("unexpected cmd %u", cmd));
	KASSERT(fibnum == RT_ALL_FIBS || (fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	if (cmd == RTM_ADD) {
		rt_addrmsg(cmd, ifa, fibnum);
		if (rt != NULL)
			rt_routemsg(cmd, rt, ifa->ifa_ifp, 0, fibnum);
	} else {
		if (rt != NULL)
			rt_routemsg(cmd, rt, ifa->ifa_ifp, 0, fibnum);
		rt_addrmsg(cmd, ifa, fibnum);
	}
}

