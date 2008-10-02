/*-
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
 * 4. Neither the name of the University nor the names of its contributors
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
#include "opt_route.h"
#include "opt_mrouting.h"
#include "opt_mpath.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/sysproto.h>
#include <sys/proc.h>
#include <sys/domain.h>
#include <sys/kernel.h>
#include <sys/vimage.h>

#include <net/if.h>
#include <net/route.h>

#ifdef RADIX_MPATH
#include <net/radix_mpath.h>
#endif

#include <netinet/in.h>
#include <netinet/ip_mroute.h>

#include <vm/uma.h>

u_int rt_numfibs = RT_NUMFIBS;
SYSCTL_INT(_net, OID_AUTO, fibs, CTLFLAG_RD, &rt_numfibs, 0, "");
/*
 * Allow the boot code to allow LESS than RT_MAXFIBS to be used.
 * We can't do more because storage is statically allocated for now.
 * (for compatibility reasons.. this will change).
 */
TUNABLE_INT("net.fibs", &rt_numfibs);

/*
 * By default add routes to all fibs for new interfaces.
 * Once this is set to 0 then only allocate routes on interface
 * changes for the FIB of the caller when adding a new set of addresses
 * to an interface.  XXX this is a shotgun aproach to a problem that needs
 * a more fine grained solution.. that will come.
 */
u_int rt_add_addr_allfibs = 1;
SYSCTL_INT(_net, OID_AUTO, add_addr_allfibs, CTLFLAG_RW,
    &rt_add_addr_allfibs, 0, "");
TUNABLE_INT("net.add_addr_allfibs", &rt_add_addr_allfibs);

static struct rtstat rtstat;

/* by default only the first 'row' of tables will be accessed. */
/* 
 * XXXMRT When we fix netstat, and do this differnetly,
 * we can allocate this dynamically. As long as we are keeping
 * things backwards compaitble we need to allocate this 
 * statically.
 */
struct radix_node_head *rt_tables[RT_MAXFIBS][AF_MAX+1];

static int	rttrash;		/* routes not in table but not freed */

static void rt_maskedcopy(struct sockaddr *,
	    struct sockaddr *, struct sockaddr *);

/* compare two sockaddr structures */
#define	sa_equal(a1, a2) (bcmp((a1), (a2), (a1)->sa_len) == 0)

/*
 * Convert a 'struct radix_node *' to a 'struct rtentry *'.
 * The operation can be done safely (in this code) because a
 * 'struct rtentry' starts with two 'struct radix_node''s, the first
 * one representing leaf nodes in the routing tree, which is
 * what the code in radix.c passes us as a 'struct radix_node'.
 *
 * But because there are a lot of assumptions in this conversion,
 * do not cast explicitly, but always use the macro below.
 */
#define RNTORT(p)	((struct rtentry *)(p))

static uma_zone_t rtzone;		/* Routing table UMA zone. */

#if 0
/* default fib for tunnels to use */
u_int tunnel_fib = 0;
SYSCTL_INT(_net, OID_AUTO, tunnelfib, CTLFLAG_RD, &tunnel_fib, 0, "");
#endif

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

SYSCTL_PROC(_net, OID_AUTO, my_fibnum, CTLTYPE_INT|CTLFLAG_RD,
            NULL, 0, &sysctl_my_fibnum, "I", "default FIB of caller");

static void
route_init(void)
{
	int table;
	struct domain *dom;
	int fam;

	/* whack the tunable ints into  line. */
	if (rt_numfibs > RT_MAXFIBS)
		rt_numfibs = RT_MAXFIBS;
	if (rt_numfibs == 0)
		rt_numfibs = 1;
	rtzone = uma_zcreate("rtentry", sizeof(struct rtentry), NULL, NULL,
	    NULL, NULL, UMA_ALIGN_PTR, 0);
	rn_init();	/* initialize all zeroes, all ones, mask table */

	for (dom = domains; dom; dom = dom->dom_next) {
		if (dom->dom_rtattach)  {
			for  (table = 0; table < rt_numfibs; table++) {
				if ( (fam = dom->dom_family) == AF_INET ||
				    table == 0) {
 			        	/* for now only AF_INET has > 1 table */
					/* XXX MRT 
					 * rtattach will be also called
					 * from vfs_export.c but the
					 * offset will be 0
					 * (only for AF_INET and AF_INET6
					 * which don't need it anyhow)
					 */
					dom->dom_rtattach(
				    	    (void **)&V_rt_tables[table][fam],
				    	    dom->dom_rtoffset);
				} else {
					break;
				}
			}
		}
	}
}

#ifndef _SYS_SYSPROTO_H_
struct setfib_args {
	int     fibnum;
};
#endif
int
setfib(struct thread *td, struct setfib_args *uap)
{
	if (uap->fibnum < 0 || uap->fibnum >= rt_numfibs)
		return EINVAL;
	td->td_proc->p_fibnum = uap->fibnum;
	return (0);
}

/*
 * Packet routing routines.
 */
void
rtalloc(struct route *ro)
{
	rtalloc_ign_fib(ro, 0UL, 0);
}

void
rtalloc_fib(struct route *ro, u_int fibnum)
{
	rtalloc_ign_fib(ro, 0UL, fibnum);
}

void
rtalloc_ign(struct route *ro, u_long ignore)
{
	struct rtentry *rt;

	if ((rt = ro->ro_rt) != NULL) {
		if (rt->rt_ifp != NULL && rt->rt_flags & RTF_UP)
			return;
		RTFREE(rt);
		ro->ro_rt = NULL;
	}
	ro->ro_rt = rtalloc1_fib(&ro->ro_dst, 1, ignore, 0);
	if (ro->ro_rt)
		RT_UNLOCK(ro->ro_rt);
}

void
rtalloc_ign_fib(struct route *ro, u_long ignore, u_int fibnum)
{
	struct rtentry *rt;

	if ((rt = ro->ro_rt) != NULL) {
		if (rt->rt_ifp != NULL && rt->rt_flags & RTF_UP)
			return;
		RTFREE(rt);
		ro->ro_rt = NULL;
	}
	ro->ro_rt = rtalloc1_fib(&ro->ro_dst, 1, ignore, fibnum);
	if (ro->ro_rt)
		RT_UNLOCK(ro->ro_rt);
}

/*
 * Look up the route that matches the address given
 * Or, at least try.. Create a cloned route if needed.
 *
 * The returned route, if any, is locked.
 */
struct rtentry *
rtalloc1(struct sockaddr *dst, int report, u_long ignflags)
{
	return (rtalloc1_fib(dst, report, ignflags, 0));
}

struct rtentry *
rtalloc1_fib(struct sockaddr *dst, int report, u_long ignflags,
		    u_int fibnum)
{
	INIT_VNET_NET(curvnet);
	struct radix_node_head *rnh;
	struct rtentry *rt;
	struct radix_node *rn;
	struct rtentry *newrt;
	struct rt_addrinfo info;
	u_long nflags;
	int err = 0, msgtype = RTM_MISS;

	KASSERT((fibnum < rt_numfibs), ("rtalloc1_fib: bad fibnum"));
	if (dst->sa_family != AF_INET)	/* Only INET supports > 1 fib now */
		fibnum = 0;
	rnh = V_rt_tables[fibnum][dst->sa_family];
	newrt = NULL;
	/*
	 * Look up the address in the table for that Address Family
	 */
	if (rnh == NULL) {
		V_rtstat.rts_unreach++;
		goto miss2;
	}
	RADIX_NODE_HEAD_LOCK(rnh);
	if ((rn = rnh->rnh_matchaddr(dst, rnh)) &&
	    (rn->rn_flags & RNF_ROOT) == 0) {
		/*
		 * If we find it and it's not the root node, then
		 * get a reference on the rtentry associated.
		 */
		newrt = rt = RNTORT(rn);
		nflags = rt->rt_flags & ~ignflags;
		if (report && (nflags & RTF_CLONING)) {
			/*
			 * We are apparently adding (report = 0 in delete).
			 * If it requires that it be cloned, do so.
			 * (This implies it wasn't a HOST route.)
			 */
			err = rtrequest_fib(RTM_RESOLVE, dst, NULL,
					      NULL, 0, &newrt, fibnum);
			if (err) {
				/*
				 * If the cloning didn't succeed, maybe
				 * what we have will do. Return that.
				 */
				newrt = rt;		/* existing route */
				RT_LOCK(newrt);
				RT_ADDREF(newrt);
				goto miss;
			}
			KASSERT(newrt, ("no route and no error"));
			RT_LOCK(newrt);
			if (newrt->rt_flags & RTF_XRESOLVE) {
				/*
				 * If the new route specifies it be
				 * externally resolved, then go do that.
				 */
				msgtype = RTM_RESOLVE;
				goto miss;
			}
			/* Inform listeners of the new route. */
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = rt_key(newrt);
			info.rti_info[RTAX_NETMASK] = rt_mask(newrt);
			info.rti_info[RTAX_GATEWAY] = newrt->rt_gateway;
			if (newrt->rt_ifp != NULL) {
				info.rti_info[RTAX_IFP] =
				    newrt->rt_ifp->if_addr->ifa_addr;
				info.rti_info[RTAX_IFA] = newrt->rt_ifa->ifa_addr;
			}
			rt_missmsg(RTM_ADD, &info, newrt->rt_flags, 0);
		} else {
			RT_LOCK(newrt);
			RT_ADDREF(newrt);
		}
		RADIX_NODE_HEAD_UNLOCK(rnh);
	} else {
		/*
		 * Either we hit the root or couldn't find any match,
		 * Which basically means
		 * "caint get there frm here"
		 */
		V_rtstat.rts_unreach++;
	miss:
		RADIX_NODE_HEAD_UNLOCK(rnh);
	miss2:	if (report) {
			/*
			 * If required, report the failure to the supervising
			 * Authorities.
			 * For a delete, this is not an error. (report == 0)
			 */
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			rt_missmsg(msgtype, &info, 0, err);
		}
	}
	if (newrt)
		RT_LOCK_ASSERT(newrt);
	return (newrt);
}

/*
 * Remove a reference count from an rtentry.
 * If the count gets low enough, take it out of the routing table
 */
void
rtfree(struct rtentry *rt)
{
	INIT_VNET_NET(curvnet);
	struct radix_node_head *rnh;

	KASSERT(rt != NULL,("%s: NULL rt", __func__));
	rnh = V_rt_tables[rt->rt_fibnum][rt_key(rt)->sa_family];
	KASSERT(rnh != NULL,("%s: NULL rnh", __func__));

	RT_LOCK_ASSERT(rt);

	/*
	 * The callers should use RTFREE_LOCKED() or RTFREE(), so
	 * we should come here exactly with the last reference.
	 */
	RT_REMREF(rt);
	if (rt->rt_refcnt > 0) {
		printf("%s: %p has %lu refs\n", __func__, rt, rt->rt_refcnt);
		goto done;
	}

	/*
	 * On last reference give the "close method" a chance
	 * to cleanup private state.  This also permits (for
	 * IPv4 and IPv6) a chance to decide if the routing table
	 * entry should be purged immediately or at a later time.
	 * When an immediate purge is to happen the close routine
	 * typically calls rtexpunge which clears the RTF_UP flag
	 * on the entry so that the code below reclaims the storage.
	 */
	if (rt->rt_refcnt == 0 && rnh->rnh_close)
		rnh->rnh_close((struct radix_node *)rt, rnh);

	/*
	 * If we are no longer "up" (and ref == 0)
	 * then we can free the resources associated
	 * with the route.
	 */
	if ((rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic("rtfree 2");
		/*
		 * the rtentry must have been removed from the routing table
		 * so it is represented in rttrash.. remove that now.
		 */
		V_rttrash--;
#ifdef	DIAGNOSTIC
		if (rt->rt_refcnt < 0) {
			printf("rtfree: %p not freed (neg refs)\n", rt);
			goto done;
		}
#endif
		/*
		 * release references on items we hold them on..
		 * e.g other routes and ifaddrs.
		 */
		if (rt->rt_ifa)
			IFAFREE(rt->rt_ifa);
		rt->rt_parent = NULL;		/* NB: no refcnt on parent */

		/*
		 * The key is separatly alloc'd so free it (see rt_setgate()).
		 * This also frees the gateway, as they are always malloc'd
		 * together.
		 */
		Free(rt_key(rt));

		/*
		 * and the rtentry itself of course
		 */
		RT_LOCK_DESTROY(rt);
		uma_zfree(rtzone, rt);
		return;
	}
done:
	RT_UNLOCK(rt);
}


/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 */
void
rtredirect(struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct sockaddr *src)
{
	rtredirect_fib(dst, gateway, netmask, flags, src, 0);
}

void
rtredirect_fib(struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct sockaddr *src,
	u_int fibnum)
{
	INIT_VNET_NET(curvnet);
	struct rtentry *rt, *rt0 = NULL;
	int error = 0;
	short *stat = NULL;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == NULL) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1_fib(dst, 0, 0UL, fibnum);	/* NB: rt is locked */
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
	if (!(flags & RTF_DONE) && rt &&
	     (!sa_equal(src, rt->rt_gateway) || rt->rt_ifa != ifa))
		error = EINVAL;
	else if (ifa_ifwithaddr(gateway))
		error = EHOSTUNREACH;
	if (error)
		goto done;
	/*
	 * Create a new entry if we just got back a wildcard entry
	 * or the the lookup failed.  This is necessary for hosts
	 * which use routing redirects generated by smart gateways
	 * to dynamically build the routing tables.
	 */
	if (rt == NULL || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
		goto create;
	/*
	 * Don't listen to the redirect if it's
	 * for a route to an interface.
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		if (((rt->rt_flags & RTF_HOST) == 0) && (flags & RTF_HOST)) {
			/*
			 * Changing from route to net => route to host.
			 * Create new route, rather than smashing route to net.
			 */
		create:
			rt0 = rt;
			rt = NULL;
		
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			info.rti_info[RTAX_GATEWAY] = gateway;
			info.rti_info[RTAX_NETMASK] = netmask;
			info.rti_ifa = ifa;
			info.rti_flags = flags;
			error = rtrequest1_fib(RTM_ADD, &info, &rt, fibnum);
			if (rt != NULL) {
				RT_LOCK(rt);
				EVENTHANDLER_INVOKE(route_redirect_event, rt0, rt, dst);
				flags = rt->rt_flags;
			}
			if (rt0)
				RTFREE_LOCKED(rt0);
			
			stat = &V_rtstat.rts_dynamic;
		} else {
			struct rtentry *gwrt;

			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &V_rtstat.rts_newgateway;
			/*
			 * add the key and gateway (in one malloc'd chunk).
			 */
			rt_setgate(rt, rt_key(rt), gateway);
			gwrt = rtalloc1(gateway, 1, 0);
			EVENTHANDLER_INVOKE(route_redirect_event, rt, gwrt, dst);
			RTFREE_LOCKED(gwrt);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt)
		RTFREE_LOCKED(rt);
out:
	if (error)
		V_rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
}

int
rtioctl(u_long req, caddr_t data)
{
	return (rtioctl_fib(req, data, 0));
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
ifa_ifwithroute(int flags, struct sockaddr *dst, struct sockaddr *gateway)
{
	return (ifa_ifwithroute_fib(flags, dst, gateway, 0));
}

struct ifaddr *
ifa_ifwithroute_fib(int flags, struct sockaddr *dst, struct sockaddr *gateway,
				u_int fibnum)
{
	register struct ifaddr *ifa;
	int not_found = 0;

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
			ifa = ifa_ifwithdstaddr(dst);
		if (ifa == NULL)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == NULL)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == NULL) {
		struct rtentry *rt = rtalloc1_fib(gateway, 0, 0UL, fibnum);
		if (rt == NULL)
			return (NULL);
		/*
		 * dismiss a gateway that is reachable only
		 * through the default router
		 */
		switch (gateway->sa_family) {
		case AF_INET:
			if (satosin(rt_key(rt))->sin_addr.s_addr == INADDR_ANY)
				not_found = 1;
			break;
		case AF_INET6:
			if (IN6_IS_ADDR_UNSPECIFIED(&satosin6(rt_key(rt))->sin6_addr))
				not_found = 1;
			break;
		default:
			break;
		}
		RT_REMREF(rt);
		RT_UNLOCK(rt);
		if (not_found)
			return (NULL);
		if ((ifa = rt->rt_ifa) == NULL)
			return (NULL);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == NULL)
			ifa = oifa;
	}
	return (ifa);
}

static walktree_f_t rt_fixdelete;
static walktree_f_t rt_fixchange;

struct rtfc_arg {
	struct rtentry *rt0;
	struct radix_node_head *rnh;
};

/*
 * Do appropriate manipulations of a routing tree given
 * all the bits of info needed
 */
int
rtrequest(int req,
	struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct rtentry **ret_nrt)
{
	return (rtrequest_fib(req, dst, gateway, netmask, flags, ret_nrt, 0));
}

int
rtrequest_fib(int req,
	struct sockaddr *dst,
	struct sockaddr *gateway,
	struct sockaddr *netmask,
	int flags,
	struct rtentry **ret_nrt,
	u_int fibnum)
{
	struct rt_addrinfo info;

	if (dst->sa_len == 0)
		return(EINVAL);

	bzero((caddr_t)&info, sizeof(info));
	info.rti_flags = flags;
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	return rtrequest1_fib(req, &info, ret_nrt, fibnum);
}

/*
 * These (questionable) definitions of apparent local variables apply
 * to the next two functions.  XXXXXX!!!
 */
#define	dst	info->rti_info[RTAX_DST]
#define	gateway	info->rti_info[RTAX_GATEWAY]
#define	netmask	info->rti_info[RTAX_NETMASK]
#define	ifaaddr	info->rti_info[RTAX_IFA]
#define	ifpaddr	info->rti_info[RTAX_IFP]
#define	flags	info->rti_flags

int
rt_getifa(struct rt_addrinfo *info)
{
	return (rt_getifa_fib(info, 0));
}

int
rt_getifa_fib(struct rt_addrinfo *info, u_int fibnum)
{
	struct ifaddr *ifa;
	int error = 0;

	/*
	 * ifp may be specified by sockaddr_dl
	 * when protocol address is ambiguous.
	 */
	if (info->rti_ifp == NULL && ifpaddr != NULL &&
	    ifpaddr->sa_family == AF_LINK &&
	    (ifa = ifa_ifwithnet(ifpaddr)) != NULL)
		info->rti_ifp = ifa->ifa_ifp;
	if (info->rti_ifa == NULL && ifaaddr != NULL)
		info->rti_ifa = ifa_ifwithaddr(ifaaddr);
	if (info->rti_ifa == NULL) {
		struct sockaddr *sa;

		sa = ifaaddr != NULL ? ifaaddr :
		    (gateway != NULL ? gateway : dst);
		if (sa != NULL && info->rti_ifp != NULL)
			info->rti_ifa = ifaof_ifpforaddr(sa, info->rti_ifp);
		else if (dst != NULL && gateway != NULL)
			info->rti_ifa = ifa_ifwithroute_fib(flags, dst, gateway,
							fibnum);
		else if (sa != NULL)
			info->rti_ifa = ifa_ifwithroute_fib(flags, sa, sa,
							fibnum);
	}
	if ((ifa = info->rti_ifa) != NULL) {
		if (info->rti_ifp == NULL)
			info->rti_ifp = ifa->ifa_ifp;
	} else
		error = ENETUNREACH;
	return (error);
}

/*
 * Expunges references to a route that's about to be reclaimed.
 * The route must be locked.
 */
int
rtexpunge(struct rtentry *rt)
{
	INIT_VNET_NET(curvnet);
	struct radix_node *rn;
	struct radix_node_head *rnh;
	struct ifaddr *ifa;
	int error = 0;

	RT_LOCK_ASSERT(rt);
#if 0
	/*
	 * We cannot assume anything about the reference count
	 * because protocols call us in many situations; often
	 * before unwinding references to the table entry.
	 */
	KASSERT(rt->rt_refcnt <= 1, ("bogus refcnt %ld", rt->rt_refcnt));
#endif
	/*
	 * Find the correct routing tree to use for this Address Family
	 */
	rnh = V_rt_tables[rt->rt_fibnum][rt_key(rt)->sa_family];
	if (rnh == NULL)
		return (EAFNOSUPPORT);

	RADIX_NODE_HEAD_LOCK(rnh);

	/*
	 * Remove the item from the tree; it should be there,
	 * but when callers invoke us blindly it may not (sigh).
	 */
	rn = rnh->rnh_deladdr(rt_key(rt), rt_mask(rt), rnh);
	if (rn == NULL) {
		error = ESRCH;
		goto bad;
	}
	KASSERT((rn->rn_flags & (RNF_ACTIVE | RNF_ROOT)) == 0,
		("unexpected flags 0x%x", rn->rn_flags));
	KASSERT(rt == RNTORT(rn),
		("lookup mismatch, rt %p rn %p", rt, rn));

	rt->rt_flags &= ~RTF_UP;

	/*
	 * Now search what's left of the subtree for any cloned
	 * routes which might have been formed from this node.
	 */
	if ((rt->rt_flags & RTF_CLONING) && rt_mask(rt))
		rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt),
				       rt_fixdelete, rt);

	/*
	 * Remove any external references we may have.
	 * This might result in another rtentry being freed if
	 * we held its last reference.
	 */
	if (rt->rt_gwroute) {
		RTFREE(rt->rt_gwroute);
		rt->rt_gwroute = NULL;
	}

	/*
	 * Give the protocol a chance to keep things in sync.
	 */
	if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest) {
		struct rt_addrinfo info;

		bzero((caddr_t)&info, sizeof(info));
		info.rti_flags = rt->rt_flags;
		info.rti_info[RTAX_DST] = rt_key(rt);
		info.rti_info[RTAX_GATEWAY] = rt->rt_gateway;
		info.rti_info[RTAX_NETMASK] = rt_mask(rt);
		ifa->ifa_rtrequest(RTM_DELETE, rt, &info);
	}

	/*
	 * one more rtentry floating around that is not
	 * linked to the routing table.
	 */
	V_rttrash++;
bad:
	RADIX_NODE_HEAD_UNLOCK(rnh);
	return (error);
}

int
rtrequest1(int req, struct rt_addrinfo *info, struct rtentry **ret_nrt)
{
	return (rtrequest1_fib(req, info, ret_nrt, 0));
}

int
rtrequest1_fib(int req, struct rt_addrinfo *info, struct rtentry **ret_nrt,
				u_int fibnum)
{
	INIT_VNET_NET(curvnet);
	int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;
#define senderr(x) { error = x ; goto bad; }

	KASSERT((fibnum < rt_numfibs), ("rtrequest1_fib: bad fibnum"));
	if (dst->sa_family != AF_INET)	/* Only INET supports > 1 fib now */
		fibnum = 0;
	/*
	 * Find the correct routing tree to use for this Address Family
	 */
	rnh = V_rt_tables[fibnum][dst->sa_family];
	if (rnh == NULL)
		return (EAFNOSUPPORT);
	RADIX_NODE_HEAD_LOCK(rnh);
	/*
	 * If we are adding a host route then we don't want to put
	 * a netmask in the tree, nor do we want to clone it.
	 */
	if (flags & RTF_HOST) {
		netmask = NULL;
		flags &= ~RTF_CLONING;
	}
	switch (req) {
	case RTM_DELETE:
#ifdef RADIX_MPATH
		/*
		 * if we got multipath routes, we require users to specify
		 * a matching RTAX_GATEWAY.
		 */
		if (rn_mpath_capable(rnh)) {
			struct rtentry *rto = NULL;

			rn = rnh->rnh_matchaddr(dst, rnh);
			if (rn == NULL)
				senderr(ESRCH);
 			rto = rt = RNTORT(rn);
			rt = rt_mpath_matchgate(rt, gateway);
			if (!rt)
				senderr(ESRCH);
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
					if (gateway &&
					    (rt->rt_gateway->sa_len != gateway->sa_len ||
					    memcmp(rt->rt_gateway, gateway, gateway->sa_len)))
						senderr(ESRCH);
				}
				/*
				 * use the normal delete code to remove
				 * the first entry
				 */
				goto normal_rtdel;
			}
			/*
			 * if the entry is 2nd and on up
			 */
			if (!rt_mpath_deldup(rto, rt))
				panic ("rtrequest1: rt_mpath_deldup");
			RT_LOCK(rt);
			RT_ADDREF(rt);
			rt->rt_flags &= ~RTF_UP;
			goto deldone;  /* done with the RTM_DELETE command */
		}

normal_rtdel:
#endif
		/*
		 * Remove the item from the tree and return it.
		 * Complain if it is not there and do no more processing.
		 */
		rn = rnh->rnh_deladdr(dst, netmask, rnh);
		if (rn == NULL)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = RNTORT(rn);
		RT_LOCK(rt);
		RT_ADDREF(rt);
		rt->rt_flags &= ~RTF_UP;

		/*
		 * Now search what's left of the subtree for any cloned
		 * routes which might have been formed from this node.
		 */
		if ((rt->rt_flags & RTF_CLONING) &&
		    rt_mask(rt)) {
			rnh->rnh_walktree_from(rnh, dst, rt_mask(rt),
					       rt_fixdelete, rt);
		}

		/*
		 * Remove any external references we may have.
		 * This might result in another rtentry being freed if
		 * we held its last reference.
		 */
		if (rt->rt_gwroute) {
			RTFREE(rt->rt_gwroute);
			rt->rt_gwroute = NULL;
		}

		/*
		 * give the protocol a chance to keep things in sync.
		 */
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, info);

#ifdef RADIX_MPATH
deldone:
#endif
		/*
		 * One more rtentry floating around that is not
		 * linked to the routing table. rttrash will be decremented
		 * when RTFREE(rt) is eventually called.
		 */
		V_rttrash++;

		/*
		 * If the caller wants it, then it can have it,
		 * but it's up to it to free the rtentry as we won't be
		 * doing it.
		 */
		if (ret_nrt) {
			*ret_nrt = rt;
			RT_UNLOCK(rt);
		} else
			RTFREE_LOCKED(rt);
		break;

	case RTM_RESOLVE:
		if (ret_nrt == NULL || (rt = *ret_nrt) == NULL)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		/* XXX locking? */
		flags = rt->rt_flags &
		    ~(RTF_CLONING | RTF_STATIC);
		flags |= RTF_WASCLONED;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == NULL)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((flags & RTF_GATEWAY) && !gateway)
			senderr(EINVAL);
		if (dst && gateway && (dst->sa_family != gateway->sa_family) && 
		    (gateway->sa_family != AF_UNSPEC) && (gateway->sa_family != AF_LINK))
			senderr(EINVAL);

		if (info->rti_ifa == NULL && (error = rt_getifa_fib(info, fibnum)))
			senderr(error);
		ifa = info->rti_ifa;

	makeroute:
		rt = uma_zalloc(rtzone, M_NOWAIT | M_ZERO);
		if (rt == NULL)
			senderr(ENOBUFS);
		RT_LOCK_INIT(rt);
		rt->rt_flags = RTF_UP | flags;
		rt->rt_fibnum = fibnum;
		/*
		 * Add the gateway. Possibly re-malloc-ing the storage for it
		 * also add the rt_gwroute if possible.
		 */
		RT_LOCK(rt);
		if ((error = rt_setgate(rt, dst, gateway)) != 0) {
			RT_LOCK_DESTROY(rt);
			uma_zfree(rtzone, rt);
			senderr(error);
		}

		/*
		 * point to the (possibly newly malloc'd) dest address.
		 */
		ndst = (struct sockaddr *)rt_key(rt);

		/*
		 * make sure it contains the value we want (masked if needed).
		 */
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			bcopy(dst, ndst, dst->sa_len);

		/*
		 * Note that we now have a reference to the ifa.
		 * This moved from below so that rnh->rnh_addaddr() can
		 * examine the ifa and  ifa->ifa_ifp if it so desires.
		 */
		IFAREF(ifa);
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;

#ifdef RADIX_MPATH
		/* do not permit exactly the same dst/mask/gw pair */
		if (rn_mpath_capable(rnh) &&
			rt_mpath_conflict(rnh, rt, netmask)) {
			if (rt->rt_gwroute)
				RTFREE(rt->rt_gwroute);
			if (rt->rt_ifa) {
				IFAFREE(rt->rt_ifa);
			}
			Free(rt_key(rt));
			RT_LOCK_DESTROY(rt);
			uma_zfree(rtzone, rt);
			senderr(EEXIST);
		}
#endif

		/* XXX mtu manipulation will be done in rnh_addaddr -- itojun */
		rn = rnh->rnh_addaddr(ndst, netmask, rnh, rt->rt_nodes);
		if (rn == NULL) {
			struct rtentry *rt2;
			/*
			 * Uh-oh, we already have one of these in the tree.
			 * We do a special hack: if the route that's already
			 * there was generated by the cloning mechanism
			 * then we just blow it away and retry the insertion
			 * of the new one.
			 */
			rt2 = rtalloc1_fib(dst, 0, 0, fibnum);
			if (rt2 && rt2->rt_parent) {
				rtexpunge(rt2);
				RT_UNLOCK(rt2);
				rn = rnh->rnh_addaddr(ndst, netmask,
						      rnh, rt->rt_nodes);
			} else if (rt2) {
				/* undo the extra ref we got */
				RTFREE_LOCKED(rt2);
			}
		}

		/*
		 * If it still failed to go into the tree,
		 * then un-make it (this should be a function)
		 */
		if (rn == NULL) {
			if (rt->rt_gwroute)
				RTFREE(rt->rt_gwroute);
			if (rt->rt_ifa)
				IFAFREE(rt->rt_ifa);
			Free(rt_key(rt));
			RT_LOCK_DESTROY(rt);
			uma_zfree(rtzone, rt);
			senderr(EEXIST);
		}

		rt->rt_parent = NULL;

		/*
		 * If we got here from RESOLVE, then we are cloning
		 * so clone the rest, and note that we
		 * are a clone (and increment the parent's references)
		 */
		if (req == RTM_RESOLVE) {
			KASSERT(ret_nrt && *ret_nrt,
				("no route to clone from"));
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
			rt->rt_rmx.rmx_pksent = 0; /* reset packet counter */
			if ((*ret_nrt)->rt_flags & RTF_CLONING) {
				/*
				 * NB: We do not bump the refcnt on the parent
				 * entry under the assumption that it will
				 * remain so long as we do.  This is
				 * important when deleting the parent route
				 * as this operation requires traversing
				 * the tree to delete all clones and futzing
				 * with refcnts requires us to double-lock
				 * parent through this back reference.
				 */
				rt->rt_parent = *ret_nrt;
			}
		}

		/*
		 * If this protocol has something to add to this then
		 * allow it to do that as well.
		 */
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, info);

		/*
		 * We repeat the same procedure from rt_setgate() here because
		 * it doesn't fire when we call it there because the node
		 * hasn't been added to the tree yet.
		 */
		if (req == RTM_ADD &&
		    !(rt->rt_flags & RTF_HOST) && rt_mask(rt) != NULL) {
			struct rtfc_arg arg;
			arg.rnh = rnh;
			arg.rt0 = rt;
			rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt),
					       rt_fixchange, &arg);
		}

		/*
		 * actually return a resultant rtentry and
		 * give the caller a single reference.
		 */
		if (ret_nrt) {
			*ret_nrt = rt;
			RT_ADDREF(rt);
		}
		RT_UNLOCK(rt);
		break;
	default:
		error = EOPNOTSUPP;
	}
bad:
	RADIX_NODE_HEAD_UNLOCK(rnh);
	return (error);
#undef senderr
}

#undef dst
#undef gateway
#undef netmask
#undef ifaaddr
#undef ifpaddr
#undef flags

/*
 * Called from rtrequest(RTM_DELETE, ...) to fix up the route's ``family''
 * (i.e., the routes related to it by the operation of cloning).  This
 * routine is iterated over all potential former-child-routes by way of
 * rnh->rnh_walktree_from() above, and those that actually are children of
 * the late parent (passed in as VP here) are themselves deleted.
 */
static int
rt_fixdelete(struct radix_node *rn, void *vp)
{
	struct rtentry *rt = RNTORT(rn);
	struct rtentry *rt0 = vp;

	if (rt->rt_parent == rt0 &&
	    !(rt->rt_flags & (RTF_PINNED | RTF_CLONING))) {
		return rtrequest_fib(RTM_DELETE, rt_key(rt), NULL, rt_mask(rt),
				 rt->rt_flags, NULL, rt->rt_fibnum);
	}
	return 0;
}

/*
 * This routine is called from rt_setgate() to do the analogous thing for
 * adds and changes.  There is the added complication in this case of a
 * middle insert; i.e., insertion of a new network route between an older
 * network route and (cloned) host routes.  For this reason, a simple check
 * of rt->rt_parent is insufficient; each candidate route must be tested
 * against the (mask, value) of the new route (passed as before in vp)
 * to see if the new route matches it.
 *
 * XXX - it may be possible to do fixdelete() for changes and reserve this
 * routine just for adds.  I'm not sure why I thought it was necessary to do
 * changes this way.
 */

static int
rt_fixchange(struct radix_node *rn, void *vp)
{
	struct rtentry *rt = RNTORT(rn);
	struct rtfc_arg *ap = vp;
	struct rtentry *rt0 = ap->rt0;
	struct radix_node_head *rnh = ap->rnh;
	u_char *xk1, *xm1, *xk2, *xmp;
	int i, len, mlen;

	/* make sure we have a parent, and route is not pinned or cloning */
	if (!rt->rt_parent ||
	    (rt->rt_flags & (RTF_PINNED | RTF_CLONING)))
		return 0;

	if (rt->rt_parent == rt0)	/* parent match */
		goto delete_rt;
	/*
	 * There probably is a function somewhere which does this...
	 * if not, there should be.
	 */
	len = imin(rt_key(rt0)->sa_len, rt_key(rt)->sa_len);

	xk1 = (u_char *)rt_key(rt0);
	xm1 = (u_char *)rt_mask(rt0);
	xk2 = (u_char *)rt_key(rt);

	/* avoid applying a less specific route */
	xmp = (u_char *)rt_mask(rt->rt_parent);
	mlen = rt_key(rt->rt_parent)->sa_len;
	if (mlen > rt_key(rt0)->sa_len)		/* less specific route */
		return 0;
	for (i = rnh->rnh_treetop->rn_offset; i < mlen; i++)
		if ((xmp[i] & ~(xmp[i] ^ xm1[i])) != xmp[i])
			return 0;	/* less specific route */

	for (i = rnh->rnh_treetop->rn_offset; i < len; i++)
		if ((xk2[i] & xm1[i]) != xk1[i])
			return 0;	/* no match */

	/*
	 * OK, this node is a clone, and matches the node currently being
	 * changed/added under the node's mask.  So, get rid of it.
	 */
delete_rt:
	return rtrequest_fib(RTM_DELETE, rt_key(rt), NULL,
			 rt_mask(rt), rt->rt_flags, NULL, rt->rt_fibnum);
}

int
rt_setgate(struct rtentry *rt, struct sockaddr *dst, struct sockaddr *gate)
{
	INIT_VNET_NET(curvnet);
	/* XXX dst may be overwritten, can we move this to below */
	struct radix_node_head *rnh =
	    V_rt_tables[rt->rt_fibnum][dst->sa_family];
	int dlen = SA_SIZE(dst), glen = SA_SIZE(gate);

again:
	RT_LOCK_ASSERT(rt);

	/*
	 * A host route with the destination equal to the gateway
	 * will interfere with keeping LLINFO in the routing
	 * table, so disallow it.
	 */
	if (((rt->rt_flags & (RTF_HOST|RTF_GATEWAY|RTF_LLINFO)) ==
					(RTF_HOST|RTF_GATEWAY)) &&
	    dst->sa_len == gate->sa_len &&
	    bcmp(dst, gate, dst->sa_len) == 0) {
		/*
		 * The route might already exist if this is an RTM_CHANGE
		 * or a routing redirect, so try to delete it.
		 */
		if (rt_key(rt))
			rtexpunge(rt);
		return EADDRNOTAVAIL;
	}

	/*
	 * Cloning loop avoidance in case of bad configuration.
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		struct rtentry *gwrt;

		RT_UNLOCK(rt);		/* XXX workaround LOR */
		gwrt = rtalloc1_fib(gate, 1, 0, rt->rt_fibnum);
		if (gwrt == rt) {
			RT_REMREF(rt);
			return (EADDRINUSE); /* failure */
		}
		/*
		 * Try to reacquire the lock on rt, and if it fails,
		 * clean state and restart from scratch.
		 */
		if (!RT_TRYLOCK(rt)) {
			RTFREE_LOCKED(gwrt);
			RT_LOCK(rt);
			goto again;
		}
		/*
		 * If there is already a gwroute, then drop it. If we
		 * are asked to replace route with itself, then do
		 * not leak its refcounter.
		 */
		if (rt->rt_gwroute != NULL) {
			if (rt->rt_gwroute == gwrt) {
				RT_REMREF(rt->rt_gwroute);
			} else
				RTFREE(rt->rt_gwroute);
		}

		if ((rt->rt_gwroute = gwrt) != NULL)
			RT_UNLOCK(rt->rt_gwroute);
	}

	/*
	 * Prepare to store the gateway in rt->rt_gateway.
	 * Both dst and gateway are stored one after the other in the same
	 * malloc'd chunk. If we have room, we can reuse the old buffer,
	 * rt_gateway already points to the right place.
	 * Otherwise, malloc a new block and update the 'dst' address.
	 */
	if (rt->rt_gateway == NULL || glen > SA_SIZE(rt->rt_gateway)) {
		caddr_t new;

		R_Malloc(new, caddr_t, dlen + glen);
		if (new == NULL)
			return ENOBUFS;
		/*
		 * XXX note, we copy from *dst and not *rt_key(rt) because
		 * rt_setgate() can be called to initialize a newly
		 * allocated route entry, in which case rt_key(rt) == NULL
		 * (and also rt->rt_gateway == NULL).
		 * Free()/free() handle a NULL argument just fine.
		 */
		bcopy(dst, new, dlen);
		Free(rt_key(rt));	/* free old block, if any */
		rt_key(rt) = (struct sockaddr *)new;
		rt->rt_gateway = (struct sockaddr *)(new + dlen);
	}

	/*
	 * Copy the new gateway value into the memory chunk.
	 */
	bcopy(gate, rt->rt_gateway, glen);

	/*
	 * This isn't going to do anything useful for host routes, so
	 * don't bother.  Also make sure we have a reasonable mask
	 * (we don't yet have one during adds).
	 */
	if (!(rt->rt_flags & RTF_HOST) && rt_mask(rt) != 0) {
		struct rtfc_arg arg;

		arg.rnh = rnh;
		arg.rt0 = rt;
		RT_UNLOCK(rt);		/* XXX workaround LOR */
		RADIX_NODE_HEAD_LOCK(rnh);
		RT_LOCK(rt);
		rnh->rnh_walktree_from(rnh, rt_key(rt), rt_mask(rt),
				       rt_fixchange, &arg);
		RADIX_NODE_HEAD_UNLOCK(rnh);
	}

	return 0;
}

static void
rt_maskedcopy(struct sockaddr *src, struct sockaddr *dst, struct sockaddr *netmask)
{
	register u_char *cp1 = (u_char *)src;
	register u_char *cp2 = (u_char *)dst;
	register u_char *cp3 = (u_char *)netmask;
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
	INIT_VNET_NET(curvnet);
	struct sockaddr *dst;
	struct sockaddr *netmask;
	struct rtentry *rt = NULL;
	struct rt_addrinfo info;
	int error = 0;
	int startfib, endfib;
	char tempbuf[_SOCKADDR_TMPSIZE];
	int didwork = 0;
	int a_failure = 0;

	if (flags & RTF_HOST) {
		dst = ifa->ifa_dstaddr;
		netmask = NULL;
	} else {
		dst = ifa->ifa_addr;
		netmask = ifa->ifa_netmask;
	}
	if ( dst->sa_family != AF_INET)
		fibnum = 0;
	if (fibnum == -1) {
		if (rt_add_addr_allfibs == 0 && cmd == (int)RTM_ADD) {
			startfib = endfib = curthread->td_proc->p_fibnum;
		} else {
			startfib = 0;
			endfib = rt_numfibs - 1;
		}
	} else {
		KASSERT((fibnum < rt_numfibs), ("rtinit1: bad fibnum"));
		startfib = fibnum;
		endfib = fibnum;
	}
	if (dst->sa_len == 0)
		return(EINVAL);

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
	}
	/*
	 * Now go through all the requested tables (fibs) and do the
	 * requested action. Realistically, this will either be fib 0
	 * for protocols that don't do multiple tables or all the
	 * tables for those that do. XXX For this version only AF_INET.
	 * When that changes code should be refactored to protocol
	 * independent parts and protocol dependent parts.
	 */
	for ( fibnum = startfib; fibnum <= endfib; fibnum++) {
		if (cmd == RTM_DELETE) {
			struct radix_node_head *rnh;
			struct radix_node *rn;
			/*
			 * Look up an rtentry that is in the routing tree and
			 * contains the correct info.
			 */
			if ((rnh = V_rt_tables[fibnum][dst->sa_family]) == NULL)
				/* this table doesn't exist but others might */
				continue;
			RADIX_NODE_HEAD_LOCK(rnh);
#ifdef RADIX_MPATH
			if (rn_mpath_capable(rnh)) {

				rn = rnh->rnh_matchaddr(dst, rnh);
				if (rn == NULL) 
					error = ESRCH;
				else {
					rt = RNTORT(rn);
					/*
					 * for interface route the
					 * rt->rt_gateway is sockaddr_intf
					 * for cloning ARP entries, so
					 * rt_mpath_matchgate must use the
					 * interface address
					 */
					rt = rt_mpath_matchgate(rt,
					    ifa->ifa_addr);
					if (!rt) 
						error = ESRCH;
				}
			}
			else
#endif
			rn = rnh->rnh_lookup(dst, netmask, rnh);
			error = (rn == NULL ||
			    (rn->rn_flags & RNF_ROOT) ||
			    RNTORT(rn)->rt_ifa != ifa ||
			    !sa_equal((struct sockaddr *)rn->rn_key, dst));
			RADIX_NODE_HEAD_UNLOCK(rnh);
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
		info.rti_flags = flags | ifa->ifa_flags;
		info.rti_info[RTAX_DST] = dst;
		info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
		info.rti_info[RTAX_NETMASK] = netmask;
		error = rtrequest1_fib(cmd, &info, &rt, fibnum);
		if (error == 0 && rt != NULL) {
			/*
			 * notify any listening routing agents of the change
			 */
			RT_LOCK(rt);
#ifdef RADIX_MPATH
			/*
			 * in case address alias finds the first address
			 * e.g. ifconfig bge0 192.103.54.246/24
			 * e.g. ifconfig bge0 192.103.54.247/24
			 * the address set in the route is 192.103.54.246
			 * so we need to replace it with 192.103.54.247
			 */
			if (memcmp(rt->rt_ifa->ifa_addr,
			    ifa->ifa_addr, ifa->ifa_addr->sa_len)) {
				IFAFREE(rt->rt_ifa);
				IFAREF(ifa);
				rt->rt_ifp = ifa->ifa_ifp;
				rt->rt_ifa = ifa;
			}
#endif
			rt_newaddrmsg(cmd, ifa, error, rt);
			if (cmd == RTM_DELETE) {
				/*
				 * If we are deleting, and we found an entry,
				 * then it's been removed from the tree..
				 * now throw it away.
				 */
				RTFREE_LOCKED(rt);
			} else {
				if (cmd == RTM_ADD) {
					/*
					 * We just wanted to add it..
					 * we don't actually need a reference.
					 */
					RT_REMREF(rt);
				}
				RT_UNLOCK(rt);
			}
			didwork = 1;
		}
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

/* special one for inet internal use. may not use. */
int
rtinit_fib(struct ifaddr *ifa, int cmd, int flags)
{
	return (rtinit1(ifa, cmd, flags, -1));
}

/*
 * Set up a routing table entry, normally
 * for an interface.
 */
int
rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	struct sockaddr *dst;
	int fib = 0;

	if (flags & RTF_HOST) {
		dst = ifa->ifa_dstaddr;
	} else {
		dst = ifa->ifa_addr;
	}

	if (dst->sa_family == AF_INET)
		fib = -1;
	return (rtinit1(ifa, cmd, flags, fib));
}

/*
 * rt_check() is invoked on each layer 2 output path, prior to
 * encapsulating outbound packets.
 *
 * The function is mostly used to find a routing entry for the gateway,
 * which in some protocol families could also point to the link-level
 * address for the gateway itself (the side effect of revalidating the
 * route to the destination is rather pointless at this stage, we did it
 * already a moment before in the pr_output() routine to locate the ifp
 * and gateway to use).
 *
 * When we remove the layer-3 to layer-2 mapping tables from the
 * routing table, this function can be removed.
 *
 * === On input ===
 *   *dst is the address of the NEXT HOP (which coincides with the
 *	final destination if directly reachable);
 *   *lrt0 points to the cached route to the final destination;
 *   *lrt is not meaningful;
 *	(*lrt0 has no ref held on it by us so REMREF is not needed.
 *	Refs only account for major structural references and not usages,
 * 	which is actually a bit of a problem.)
 *
 * === Operation ===
 * If the route is marked down try to find a new route.  If the route
 * to the gateway is gone, try to setup a new route.  Otherwise,
 * if the route is marked for packets to be rejected, enforce that.
 * Note that rtalloc returns an rtentry with an extra REF that we may
 * need to lose.
 *
 * === On return ===
 *   *dst is unchanged;
 *   *lrt0 points to the (possibly new) route to the final destination
 *   *lrt points to the route to the next hop   [LOCKED]
 *
 * Their values are meaningful ONLY if no error is returned.
 *
 * To follow this you have to remember that:
 * RT_REMREF reduces the reference count by 1 but doesn't check it for 0 (!)
 * RTFREE_LOCKED includes an RT_REMREF (or an rtfree if refs == 1)
 *    and an RT_UNLOCK
 * RTFREE does an RT_LOCK and an RTFREE_LOCKED
 * The gwroute pointer counts as a reference on the rtentry to which it points.
 * so when we add it we use the ref that rtalloc gives us and when we lose it
 * we need to remove the reference.
 * RT_TEMP_UNLOCK does an RT_ADDREF before freeing the lock, and
 * RT_RELOCK locks it (it can't have gone away due to the ref) and
 * drops the ref, possibly freeing it and zeroing the pointer if
 * the ref goes to 0 (unlocking in the process).
 */
int
rt_check(struct rtentry **lrt, struct rtentry **lrt0, struct sockaddr *dst)
{
	struct rtentry *rt;
	struct rtentry *rt0;
	u_int fibnum;

	KASSERT(*lrt0 != NULL, ("rt_check"));
	rt0 = *lrt0;
	rt = NULL;
	fibnum = rt0->rt_fibnum;

	/* NB: the locking here is tortuous... */
	RT_LOCK(rt0);
retry:
	if (rt0 && (rt0->rt_flags & RTF_UP) == 0) {
		/* Current rt0 is useless, try get a replacement. */
		RT_UNLOCK(rt0);
		rt0 = NULL;
	}
	if (rt0 == NULL) {
		rt0 = rtalloc1_fib(dst, 1, 0UL, fibnum);
		if (rt0 == NULL) {
			return (EHOSTUNREACH);
		}
		RT_REMREF(rt0); /* don't need the reference. */
	}

	if (rt0->rt_flags & RTF_GATEWAY) {
		if ((rt = rt0->rt_gwroute) != NULL) {
			RT_LOCK(rt);		/* NB: gwroute */
			if ((rt->rt_flags & RTF_UP) == 0) {
				/* gw route is dud. ignore/lose it */
				RTFREE_LOCKED(rt); /* unref (&unlock) gwroute */
				rt = rt0->rt_gwroute = NULL;
			}
		}
		
		if (rt == NULL) {  /* NOT AN ELSE CLAUSE */
			RT_TEMP_UNLOCK(rt0); /* MUST return to undo this */
			rt = rtalloc1_fib(rt0->rt_gateway, 1, 0UL, fibnum);
			if ((rt == rt0) || (rt == NULL)) {
				/* the best we can do is not good enough */
				if (rt) {
					RT_REMREF(rt); /* assumes ref > 0 */
					RT_UNLOCK(rt);
				}
				RTFREE(rt0); /* lock, unref, (unlock) */
				return (ENETUNREACH);
			}
			/*
			 * Relock it and lose the added reference.
			 * All sorts of things could have happenned while we
			 * had no lock on it, so check for them.
			 */
			RT_RELOCK(rt0);
			if (rt0 == NULL || ((rt0->rt_flags & RTF_UP) == 0))
				/* Ru-roh.. what we had is no longer any good */
				goto retry;
			/* 
			 * While we were away, someone replaced the gateway.
			 * Since a reference count is involved we can't just
			 * overwrite it.
			 */
			if (rt0->rt_gwroute) {
				if (rt0->rt_gwroute != rt) {
					RTFREE_LOCKED(rt);
					goto retry;
				}
			} else {
				rt0->rt_gwroute = rt;
			}
		}
		RT_LOCK_ASSERT(rt);
		RT_UNLOCK(rt0);
	} else {
		/* think of rt as having the lock from now on.. */
		rt = rt0;
	}
	/* XXX why are we inspecting rmx_expire? */
	if ((rt->rt_flags & RTF_REJECT) &&
	    (rt->rt_rmx.rmx_expire == 0 ||
	    time_uptime < rt->rt_rmx.rmx_expire)) {
		RT_UNLOCK(rt);
		return (rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}

	*lrt = rt;
	*lrt0 = rt0;
	return (0);
}

/* This must be before ip6_init2(), which is now SI_ORDER_MIDDLE */
SYSINIT(route, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, route_init, 0);
