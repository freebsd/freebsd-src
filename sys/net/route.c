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
#include <sys/devctl.h>
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
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_mroute.h>
#include <netinet6/in6_var.h>

VNET_PCPUSTAT_DEFINE(struct rtstat, rtstat);

VNET_PCPUSTAT_SYSINIT(rtstat);
#ifdef VIMAGE
VNET_PCPUSTAT_SYSUNINIT(rtstat);
#endif

EVENTHANDLER_LIST_DEFINE(rt_addrmsg);

static int rt_ifdelroute(const struct rtentry *rt, const struct nhop_object *,
    void *arg);

/*
 * route initialization must occur before ip6_init2(), which happenas at
 * SI_ORDER_MIDDLE.
 */
static void
route_init(void)
{

	nhops_init();
}
SYSINIT(route_init, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, route_init, NULL);

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

	RIB_WLOCK(rh);
	rh->rib_dying = true;
	RIB_WUNLOCK(rh);

#ifdef FIB_ALGO
	fib_destroy_rib(rh);
#endif

	tmproutes_destroy(rh);

	rn_walktree(&rh->rmhead.head, rt_freeentry, &rh->rmhead.head);

	nhops_destroy_rib(rh);

	rib_destroy_subscriptions(rh);

	/* Assume table is already empty */
	RIB_LOCK_DESTROY(rh);
	free(rh, M_RTABLE);
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
	flags |= RTF_HOST | RTF_DYNAMIC;

	/* Get the best ifa for the given interface and gateway. */
	if ((ifa = ifaof_ifpforaddr(gateway, ifp)) == NULL)
		return (ENETUNREACH);

	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_ifa = ifa;
	info.rti_ifp = ifp;
	info.rti_flags = flags;

	/* Setup route metrics to define expire time. */
	bzero(&rti_rmx, sizeof(rti_rmx));
	/* Set expire time as absolute. */
	rti_rmx.rmx_expire = lifetime_sec + time_second;
	info.rti_mflags |= RTV_EXPIRE;
	info.rti_rmx = &rti_rmx;

	error = rib_action(fibnum, RTM_ADD, &info, &rc);

	if (error != 0) {
		/* TODO: add per-fib redirect stats. */
		return (error);
	}

	RTSTAT_INC(rts_dynamic);

	/* Send notification of a route addition to userland. */
	bzero(&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_AUTHOR] = author;
	rt_missmsg_fib(RTM_REDIRECT, &info, flags | RTF_UP, error, fibnum);

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
	if ((rt->rte_flags & RTF_UP) == 0)
		return (0);

	return (1);
}

void
rt_flushifroutes(struct ifnet *ifp)
{

	rib_foreach_table_walk_del(AF_UNSPEC, rt_ifdelroute, ifp);
}

/*
 * Tries to extract interface from RTAX_IFP passed in rt_addrinfo.
 * Interface can be specified ether as interface index (sdl_index) or
 * the interface name (sdl_data).
 *
 * Returns found ifp or NULL
 */
static struct ifnet *
info_get_ifp(struct rt_addrinfo *info)
{
	const struct sockaddr_dl *sdl;

	sdl = (const struct sockaddr_dl *)info->rti_info[RTAX_IFP];
	if (sdl->sdl_family != AF_LINK)
		return (NULL);

	if (sdl->sdl_index != 0)
		return (ifnet_byindex(sdl->sdl_index));
	if (sdl->sdl_nlen > 0) {
		char if_name[IF_NAMESIZE];
		if (sdl->sdl_nlen + offsetof(struct sockaddr_dl, sdl_data) > sdl->sdl_len)
			return (NULL);
		if (sdl->sdl_nlen >= IF_NAMESIZE)
			return (NULL);
		bzero(if_name, sizeof(if_name));
		memcpy(if_name, sdl->sdl_data, sdl->sdl_nlen);
		return (ifunit(if_name));
	}

	return (NULL);
}

/*
 * Calculates proper ifa/ifp for the cases when gateway AF is different
 * from dst AF.
 *
 * Returns 0 on success.
 */
__noinline static int
rt_getifa_family(struct rt_addrinfo *info, uint32_t fibnum)
{
	if (info->rti_ifp == NULL) {
		struct ifaddr *ifa = NULL;
		/*
		 * No transmit interface specified. Guess it by checking gw sa.
		 */
		const struct sockaddr *gw = info->rti_info[RTAX_GATEWAY];
		ifa = ifa_ifwithroute(RTF_GATEWAY, gw, gw, fibnum);
		if (ifa == NULL)
			return (ENETUNREACH);
		info->rti_ifp = ifa->ifa_ifp;
	}

	/* Prefer address from outgoing interface */
	info->rti_ifa = ifaof_ifpforaddr(info->rti_info[RTAX_DST], info->rti_ifp);
#ifdef INET
	if (info->rti_ifa == NULL) {
		/* Use first found IPv4 address */
		bool loopback_ok = info->rti_ifp->if_flags & IFF_LOOPBACK;
		info->rti_ifa = (struct ifaddr *)in_findlocal(fibnum, loopback_ok);
	}
#endif
	if (info->rti_ifa == NULL)
		return (ENETUNREACH);
	return (0);
}

/*
 * Fills in rti_ifp and rti_ifa for the provided fib.
 *
 * Assume basic consistency checks are executed by callers:
 * RTAX_DST exists, if RTF_GATEWAY is set, RTAX_GATEWAY exists as well.
 */
int
rt_getifa_fib(struct rt_addrinfo *info, u_int fibnum)
{
	const struct sockaddr *dst, *gateway, *ifaaddr;
	int error, flags;

	dst = info->rti_info[RTAX_DST];
	gateway = info->rti_info[RTAX_GATEWAY];
	ifaaddr = info->rti_info[RTAX_IFA];
	flags = info->rti_flags;

	/*
	 * ifp may be specified by sockaddr_dl
	 * when protocol address is ambiguous.
	 */
	error = 0;

	/* If we have interface specified by RTAX_IFP address, try to use it */
	if ((info->rti_ifp == NULL) && (info->rti_info[RTAX_IFP] != NULL))
		info->rti_ifp = info_get_ifp(info);
	/*
	 * If we have source address specified, try to find it
	 * TODO: avoid enumerating all ifas on all interfaces.
	 */
	if (info->rti_ifa == NULL && ifaaddr != NULL)
		info->rti_ifa = ifa_ifwithaddr(ifaaddr);
	if ((info->rti_ifa == NULL) && ((info->rti_flags & RTF_GATEWAY) != 0) &&
	    (gateway->sa_family != dst->sa_family))
		return (rt_getifa_family(info, fibnum));
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
	if (info->rti_ifa != NULL) {
		if (info->rti_ifp == NULL)
			info->rti_ifp = info->rti_ifa->ifa_ifp;
	} else
		error = ENETUNREACH;
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

void
rt_maskedcopy(const struct sockaddr *src, struct sockaddr *dst,
    const struct sockaddr *netmask)
{
	const u_char *cp1 = (const u_char *)src;
	u_char *cp2 = (u_char *)dst;
	const u_char *cp3 = (const u_char *)netmask;
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
 * Announce interface address arrival/withdraw
 * Returns 0 on success.
 */
int
rt_addrmsg(int cmd, struct ifaddr *ifa, int fibnum)
{
#if defined(INET) || defined(INET6)
	struct sockaddr *sa = ifa->ifa_addr;
	struct ifnet *ifp = ifa->ifa_ifp;
#endif

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
	    ("unexpected cmd %d", cmd));
	KASSERT((fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	EVENTHANDLER_DIRECT_INVOKE(rt_addrmsg, ifa, cmd);

#ifdef INET
	if (sa->sa_family == AF_INET) {
		char addrstr[INET_ADDRSTRLEN];
		char strbuf[INET_ADDRSTRLEN + 12];

		inet_ntoa_r(((struct sockaddr_in *)sa)->sin_addr, addrstr);
		snprintf(strbuf, sizeof(strbuf), "address=%s", addrstr);
		devctl_notify("IFNET", ifp->if_xname,
		    (cmd == RTM_ADD) ? "ADDR_ADD" : "ADDR_DEL", strbuf);
	}
#endif
#ifdef INET6
	if (sa->sa_family == AF_INET6) {
		char addrstr[INET6_ADDRSTRLEN];
		char strbuf[INET6_ADDRSTRLEN + 12];

		ip6_sprintf(addrstr, IFA_IN6(ifa));
		snprintf(strbuf, sizeof(strbuf), "address=%s", addrstr);
		devctl_notify("IFNET", ifp->if_xname,
		    (cmd == RTM_ADD) ? "ADDR_ADD" : "ADDR_DEL", strbuf);
	}
#endif

	if (V_rt_add_addr_allfibs)
		fibnum = RT_ALL_FIBS;
	return (rtsock_addrmsg(cmd, ifa, fibnum));
}

/*
 * Announce kernel-originated route addition/removal to rtsock based on @rt data.
 * cmd: RTM_ cmd
 * @rt: valid rtentry
 * @nh: nhop object to announce
 * @fibnum: fib id or RT_ALL_FIBS
 *
 * Returns 0 on success.
 */
int
rt_routemsg(int cmd, struct rtentry *rt, struct nhop_object *nh,
    int fibnum)
{

	KASSERT(cmd == RTM_ADD || cmd == RTM_DELETE,
	    ("unexpected cmd %d", cmd));

	KASSERT(fibnum == RT_ALL_FIBS || (fibnum >= 0 && fibnum < rt_numfibs),
	    ("%s: fib out of range 0 <=%d<%d", __func__, fibnum, rt_numfibs));

	KASSERT(rt_key(rt) != NULL, (":%s: rt_key must be supplied", __func__));

	return (rtsock_routemsg(cmd, rt, nh, fibnum));
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
