/*
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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

#include "opt_inet.h"
#include "opt_mrouting.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/domain.h>
#include <sys/kernel.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip_mroute.h>

#define	SA(p) ((struct sockaddr *)(p))

static struct rtstat rtstat;
struct radix_node_head *rt_tables[AF_MAX+1];

static int	rttrash;		/* routes not in table but not freed */

static void rt_maskedcopy(struct sockaddr *,
	    struct sockaddr *, struct sockaddr *);
static void rtable_init(void **);

static void
rtable_init(void **table)
{
	struct domain *dom;
	for (dom = domains; dom; dom = dom->dom_next)
		if (dom->dom_rtattach)
			dom->dom_rtattach(&table[dom->dom_family],
			    dom->dom_rtoffset);
}

void
route_init()
{
	rn_init();	/* initialize all zeroes, all ones, mask table */
	rtable_init((void **)rt_tables);
}

/*
 * Packet routing routines.
 */
void
rtalloc(struct route *ro)
{
	rtalloc_ign(ro, 0UL);
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
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1, ignore);
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
	struct radix_node_head *rnh = rt_tables[dst->sa_family];
	struct rtentry *rt;
	struct radix_node *rn;
	struct rtentry *newrt;
	struct rt_addrinfo info;
	u_long nflags;
	int err = 0, msgtype = RTM_MISS;

	newrt = 0;
	bzero(&info, sizeof(info));
	/*
	 * Look up the address in the table for that Address Family
	 */
	if (rnh == NULL) {
		rtstat.rts_unreach++;
		goto miss2;
	}
	RADIX_NODE_HEAD_LOCK(rnh);
	if ((rn = rnh->rnh_matchaddr(dst, rnh)) &&
	    (rn->rn_flags & RNF_ROOT) == 0) {
		/*
		 * If we find it and it's not the root node, then
		 * get a refernce on the rtentry associated.
		 */
		newrt = rt = (struct rtentry *)rn;
		nflags = rt->rt_flags & ~ignflags;
		if (report && (nflags & (RTF_CLONING | RTF_PRCLONING))) {
			/*
			 * We are apparently adding (report = 0 in delete).
			 * If it requires that it be cloned, do so.
			 * (This implies it wasn't a HOST route.)
			 */
			err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt);
			if (err) {
				/*
				 * If the cloning didn't succeed, maybe
				 * what we have will do. Return that.
				 */
				newrt = rt;		/* existing route */
				RT_LOCK(newrt);
				newrt->rt_refcnt++;
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
			info.rti_info[RTAX_DST] = rt_key(newrt);
			info.rti_info[RTAX_NETMASK] = rt_mask(newrt);
			info.rti_info[RTAX_GATEWAY] = newrt->rt_gateway;
			if (newrt->rt_ifp != NULL) {
				info.rti_info[RTAX_IFP] =
				    TAILQ_FIRST(&newrt->rt_ifp->if_addrhead)->ifa_addr;
				info.rti_info[RTAX_IFA] = newrt->rt_ifa->ifa_addr;
			}
			rt_missmsg(RTM_ADD, &info, newrt->rt_flags, 0);
		} else {
			KASSERT(rt == newrt, ("locking wrong route"));
			RT_LOCK(newrt);
			newrt->rt_refcnt++;
		}
		RADIX_NODE_HEAD_UNLOCK(rnh);
	} else {
		/*
		 * Either we hit the root or couldn't find any match,
		 * Which basically means
		 * "caint get there frm here"
		 */
		rtstat.rts_unreach++;
	miss:
		RADIX_NODE_HEAD_UNLOCK(rnh);
	miss2:	if (report) {
			/*
			 * If required, report the failure to the supervising
			 * Authorities.
			 * For a delete, this is not an error. (report == 0)
			 */
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
	/*
	 * find the tree for that address family
	 */
	struct radix_node_head *rnh = rt_tables[rt_key(rt)->sa_family];

	if (rt == 0 || rnh == 0)
		panic("rtfree");

	RT_LOCK_ASSERT(rt);

	/*
	 * decrement the reference count by one and if it reaches 0,
	 * and there is a close function defined, call the close function
	 */
	if (--rt->rt_refcnt > 0)
		goto done;
	/* XXX refcount==0? */
	if (rt->rt_refcnt == 0 && rnh->rnh_close)
		rnh->rnh_close((struct radix_node *)rt, rnh);

	/*
	 * If we are no longer "up" (and ref == 0)
	 * then we can free the resources associated
	 * with the route.
	 */
	if ((rt->rt_flags & RTF_UP) == 0) {
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		/*
		 * the rtentry must have been removed from the routing table
		 * so it is represented in rttrash.. remove that now.
		 */
		rttrash--;
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
		Free(rt);
		return;
	}
done:
	RT_UNLOCK(rt);
}

/* compare two sockaddr structures */
#define	sa_equal(a1, a2) (bcmp((a1), (a2), (a1)->sa_len) == 0)

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
	struct rtentry *rt;
	int error = 0;
	short *stat = 0;
	struct rt_addrinfo info;
	struct ifaddr *ifa;

	/* verify the gateway is directly reachable */
	if ((ifa = ifa_ifwithnet(gateway)) == 0) {
		error = ENETUNREACH;
		goto out;
	}
	rt = rtalloc1(dst, 0, 0UL);	/* NB: rt is locked */
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
	if (rt == 0 || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
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
			if (rt)
				rtfree(rt);
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			bzero((caddr_t)&info, sizeof(info));
			info.rti_info[RTAX_DST] = dst;
			info.rti_info[RTAX_GATEWAY] = gateway;
			info.rti_info[RTAX_NETMASK] = netmask;
			info.rti_ifa = ifa;
			info.rti_flags = flags;
			rt = NULL;
			error = rtrequest1(RTM_ADD, &info, &rt);
			if (rt != NULL) {
				RT_LOCK(rt);
				flags = rt->rt_flags;
			}
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			rt->rt_flags |= RTF_MODIFIED;
			flags |= RTF_MODIFIED;
			stat = &rtstat.rts_newgateway;
			/*
			 * add the key and gateway (in one malloc'd chunk).
			 */
			rt_setgate(rt, rt_key(rt), gateway);
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt)
		rtfree(rt);
out:
	if (error)
		rtstat.rts_badredirect++;
	else if (stat != NULL)
		(*stat)++;
	bzero((caddr_t)&info, sizeof(info));
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	info.rti_info[RTAX_AUTHOR] = src;
	rt_missmsg(RTM_REDIRECT, &info, flags, error);
}

/*
 * Routing table ioctl interface.
 */
int
rtioctl(u_long req, caddr_t data)
{
#ifdef INET
	/* Multicast goop, grrr... */
	return mrt_ioctl ? mrt_ioctl(req, data) : EOPNOTSUPP;
#else /* INET */
	return ENXIO;
#endif /* INET */
}

struct ifaddr *
ifa_ifwithroute(int flags, struct sockaddr *dst, struct sockaddr *gateway)
{
	register struct ifaddr *ifa;

	if ((flags & RTF_GATEWAY) == 0) {
		/*
		 * If we are adding a route to an interface,
		 * and the interface is a pt to pt link
		 * we should search for the destination
		 * as our clue to the interface.  Otherwise
		 * we can use the local address.
		 */
		ifa = 0;
		if (flags & RTF_HOST) {
			ifa = ifa_ifwithdstaddr(dst);
		}
		if (ifa == 0)
			ifa = ifa_ifwithaddr(gateway);
	} else {
		/*
		 * If we are adding a route to a remote net
		 * or host, the gateway may still be on the
		 * other end of a pt to pt link.
		 */
		ifa = ifa_ifwithdstaddr(gateway);
	}
	if (ifa == 0)
		ifa = ifa_ifwithnet(gateway);
	if (ifa == 0) {
		struct rtentry *rt = rtalloc1(gateway, 0, 0UL);
		if (rt == 0)
			return (0);
		--rt->rt_refcnt;
		RT_UNLOCK(rt);
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa;
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	return (ifa);
}

static int rt_fixdelete(struct radix_node *, void *);
static int rt_fixchange(struct radix_node *, void *);

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
	struct rt_addrinfo info;

	bzero((caddr_t)&info, sizeof(info));
	info.rti_flags = flags;
	info.rti_info[RTAX_DST] = dst;
	info.rti_info[RTAX_GATEWAY] = gateway;
	info.rti_info[RTAX_NETMASK] = netmask;
	return rtrequest1(req, &info, ret_nrt);
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
			info->rti_ifa = ifa_ifwithroute(flags, dst, gateway);
		else if (sa != NULL)
			info->rti_ifa = ifa_ifwithroute(flags, sa, sa);
	}
	if ((ifa = info->rti_ifa) != NULL) {
		if (info->rti_ifp == NULL)
			info->rti_ifp = ifa->ifa_ifp;
	} else
		error = ENETUNREACH;
	return (error);
}

int
rtrequest1(int req, struct rt_addrinfo *info, struct rtentry **ret_nrt)
{
	int error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa;
	struct sockaddr *ndst;
#define senderr(x) { error = x ; goto bad; }

	/*
	 * Find the correct routing tree to use for this Address Family
	 */
	rnh = rt_tables[dst->sa_family];
	if (rnh == 0)
		return (EAFNOSUPPORT);
	RADIX_NODE_HEAD_LOCK(rnh);
	/*
	 * If we are adding a host route then we don't want to put
	 * a netmask in the tree, nor do we want to clone it.
	 */
	if (flags & RTF_HOST) {
		netmask = 0;
		flags &= ~(RTF_CLONING | RTF_PRCLONING);
	}
	switch (req) {
	case RTM_DELETE:
		/*
		 * Remove the item from the tree and return it.
		 * Complain if it is not there and do no more processing.
		 */
		rn = rnh->rnh_deladdr(dst, netmask, rnh);
		if (rn == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		RT_LOCK(rt);
		rt->rt_refcnt++;
		rt->rt_flags &= ~RTF_UP;

		/*
		 * Now search what's left of the subtree for any cloned
		 * routes which might have been formed from this node.
		 */
		if ((rt->rt_flags & (RTF_CLONING | RTF_PRCLONING)) &&
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
			struct rtentry *gwrt = rt->rt_gwroute;
			RTFREE(gwrt);
			rt->rt_gwroute = 0;
		}

		/*
		 * give the protocol a chance to keep things in sync.
		 */
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, info);

		/*
		 * one more rtentry floating around that is not
		 * linked to the routing table.
		 */
		rttrash++;

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
		if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		/* XXX locking? */
		flags = rt->rt_flags &
		    ~(RTF_CLONING | RTF_PRCLONING | RTF_STATIC);
		flags |= RTF_WASCLONED;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == 0)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((flags & RTF_GATEWAY) && !gateway)
			panic("rtrequest: GATEWAY but no gateway");

		if (info->rti_ifa == NULL && (error = rt_getifa(info)))
			senderr(error);
		ifa = info->rti_ifa;

	makeroute:
		R_Zalloc(rt, struct rtentry *, sizeof(*rt));
		if (rt == 0)
			senderr(ENOBUFS);
		RT_LOCK_INIT(rt);
		rt->rt_flags = RTF_UP | flags;
		/*
		 * Add the gateway. Possibly re-malloc-ing the storage for it
		 * also add the rt_gwroute if possible.
		 */
		RT_LOCK(rt);
		if ((error = rt_setgate(rt, dst, gateway)) != 0) {
			RT_LOCK_DESTROY(rt);
			Free(rt);
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
			Bcopy(dst, ndst, dst->sa_len);

		/*
		 * Note that we now have a reference to the ifa.
		 * This moved from below so that rnh->rnh_addaddr() can
		 * examine the ifa and  ifa->ifa_ifp if it so desires.
		 */
		IFAREF(ifa);
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;

		/* XXX mtu manipulation will be done in rnh_addaddr -- itojun */
		rn = rnh->rnh_addaddr(ndst, netmask, rnh, rt->rt_nodes);
		if (rn == 0) {
			struct rtentry *rt2;
			/*
			 * Uh-oh, we already have one of these in the tree.
			 * We do a special hack: if the route that's already
			 * there was generated by the protocol-cloning
			 * mechanism, then we just blow it away and retry
			 * the insertion of the new one.
			 */
			rt2 = rtalloc1(dst, 0, RTF_PRCLONING);
			if (rt2 && rt2->rt_parent) {
				RT_UNLOCK(rt2);		/* XXX recursive lock */
				rtrequest(RTM_DELETE,
					  rt_key(rt2),
					  rt2->rt_gateway,
					  rt_mask(rt2), rt2->rt_flags, 0);
				RTFREE(rt2);
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
		if (rn == 0) {
			if (rt->rt_gwroute)
				RTFREE(rt->rt_gwroute);
			if (rt->rt_ifa)
				IFAFREE(rt->rt_ifa);
			Free(rt_key(rt));
			RT_LOCK_DESTROY(rt);
			Free(rt);
			senderr(EEXIST);
		}

		rt->rt_parent = 0;

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
			if ((*ret_nrt)->rt_flags & (RTF_CLONING | RTF_PRCLONING)) {
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
		 * if this protocol has something to add to this then
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
		    !(rt->rt_flags & RTF_HOST) && rt_mask(rt) != 0) {
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
			rt->rt_refcnt++;
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
	struct rtentry *rt = (struct rtentry *)rn;
	struct rtentry *rt0 = vp;

	if (rt->rt_parent == rt0 &&
	    !(rt->rt_flags & (RTF_PINNED | RTF_CLONING | RTF_PRCLONING))) {
		return rtrequest(RTM_DELETE, rt_key(rt),
				 (struct sockaddr *)0, rt_mask(rt),
				 rt->rt_flags, (struct rtentry **)0);
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
#ifdef DEBUG
static int rtfcdebug = 0;
#endif

static int
rt_fixchange(struct radix_node *rn, void *vp)
{
	struct rtentry *rt = (struct rtentry *)rn;
	struct rtfc_arg *ap = vp;
	struct rtentry *rt0 = ap->rt0;
	struct radix_node_head *rnh = ap->rnh;
	u_char *xk1, *xm1, *xk2, *xmp;
	int i, len, mlen;

#ifdef DEBUG
	if (rtfcdebug)
		printf("rt_fixchange: rt %p, rt0 %p\n", rt, rt0);
#endif

	if (!rt->rt_parent ||
	    (rt->rt_flags & (RTF_PINNED | RTF_CLONING | RTF_PRCLONING))) {
#ifdef DEBUG
		if(rtfcdebug) printf("no parent, pinned or cloning\n");
#endif
		return 0;
	}

	if (rt->rt_parent == rt0) {
#ifdef DEBUG
		if(rtfcdebug) printf("parent match\n");
#endif
		return rtrequest(RTM_DELETE, rt_key(rt),
				 (struct sockaddr *)0, rt_mask(rt),
				 rt->rt_flags, (struct rtentry **)0);
	}

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
	if (mlen > rt_key(rt0)->sa_len) {
#ifdef DEBUG
		if (rtfcdebug)
			printf("rt_fixchange: inserting a less "
			       "specific route\n");
#endif
		return 0;
	}
	for (i = rnh->rnh_treetop->rn_offset; i < mlen; i++) {
		if ((xmp[i] & ~(xmp[i] ^ xm1[i])) != xmp[i]) {
#ifdef DEBUG
			if (rtfcdebug)
				printf("rt_fixchange: inserting a less "
				       "specific route\n");
#endif
			return 0;
		}
	}

	for (i = rnh->rnh_treetop->rn_offset; i < len; i++) {
		if ((xk2[i] & xm1[i]) != xk1[i]) {
#ifdef DEBUG
			if(rtfcdebug) printf("no match\n");
#endif
			return 0;
		}
	}

	/*
	 * OK, this node is a clone, and matches the node currently being
	 * changed/added under the node's mask.  So, get rid of it.
	 */
#ifdef DEBUG
	if(rtfcdebug) printf("deleting\n");
#endif
	return rtrequest(RTM_DELETE, rt_key(rt), (struct sockaddr *)0,
			 rt_mask(rt), rt->rt_flags, (struct rtentry **)0);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

int
rt_setgate(struct rtentry *rt, struct sockaddr *dst, struct sockaddr *gate)
{
	/* XXX dst may be overwritten, can we move this to below */
	struct radix_node_head *rnh = rt_tables[dst->sa_family];
	caddr_t new, old;
	int dlen = ROUNDUP(dst->sa_len), glen = ROUNDUP(gate->sa_len);

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
			rtrequest(RTM_DELETE, rt_key(rt),
			    rt->rt_gateway, rt_mask(rt), rt->rt_flags, 0);
		return EADDRNOTAVAIL;
	}

	/*
	 * Both dst and gateway are stored in the same malloc'd chunk
	 * (If I ever get my hands on....)
	 * if we need to malloc a new chunk, then keep the old one around
	 * till we don't need it any more.
	 */
	if (rt->rt_gateway == 0 || glen > ROUNDUP(rt->rt_gateway->sa_len)) {
		old = (caddr_t)rt_key(rt);
		R_Malloc(new, caddr_t, dlen + glen);
		if (new == 0)
			return ENOBUFS;
		rt_key(rt) = new;
	} else {
		/*
		 * otherwise just overwrite the old one
		 */
		new = (caddr_t)rt_key(rt);
		old = 0;
	}

	/*
	 * copy the new gateway value into the memory chunk
	 */
	Bcopy(gate, (rt->rt_gateway = (struct sockaddr *)(new + dlen)), glen);

	/*
	 * if we are replacing the chunk (or it's new) we need to
	 * replace the dst as well
	 */
	if (old) {
		Bcopy(dst, new, dlen);
		Free(old);
		old = 0;
	}

	/*
	 * If there is already a gwroute, it's now almost definitly wrong
	 * so drop it.
	 */
	if (rt->rt_gwroute != NULL) {
		RTFREE(rt->rt_gwroute);
		rt->rt_gwroute = NULL;
	}
	/*
	 * Cloning loop avoidance:
	 * In the presence of protocol-cloning and bad configuration,
	 * it is possible to get stuck in bottomless mutual recursion
	 * (rtrequest rt_setgate rtalloc1).  We avoid this by not allowing
	 * protocol-cloning to operate for gateways (which is probably the
	 * correct choice anyway), and avoid the resulting reference loops
	 * by disallowing any route to run through itself as a gateway.
	 * This is obviously mandatory when we get rt->rt_output().
	 */
	if (rt->rt_flags & RTF_GATEWAY) {
		rt->rt_gwroute = rtalloc1(gate, 1, RTF_PRCLONING);
		if (rt->rt_gwroute == rt) {
			RTFREE_LOCKED(rt->rt_gwroute);
			rt->rt_gwroute = 0;
			return EDQUOT; /* failure */
		}
		if (rt->rt_gwroute != NULL)
			RT_UNLOCK(rt->rt_gwroute);
	}

	/*
	 * This isn't going to do anything useful for host routes, so
	 * don't bother.  Also make sure we have a reasonable mask
	 * (we don't yet have one during adds).
	 */
	if (!(rt->rt_flags & RTF_HOST) && rt_mask(rt) != 0) {
		struct rtfc_arg arg;

		arg.rnh = rnh;
		arg.rt0 = rt;
		RADIX_NODE_HEAD_LOCK(rnh);
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
int
rtinit(struct ifaddr *ifa, int cmd, int flags)
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct sockaddr *netmask;
	struct mbuf *m = 0;
	struct rtentry *nrt = 0;
	struct radix_node_head *rnh;
	struct radix_node *rn;
	int error;
	struct rt_addrinfo info;

	if (flags & RTF_HOST) {
		dst = ifa->ifa_dstaddr;
		netmask = NULL;
	} else {
		dst = ifa->ifa_addr;
		netmask = ifa->ifa_netmask;
	}
	/*
	 * If it's a delete, check that if it exists, it's on the correct
	 * interface or we might scrub a route to another ifa which would
	 * be confusing at best and possibly worse.
	 */
	if (cmd == RTM_DELETE) {
		/*
		 * It's a delete, so it should already exist..
		 * If it's a net, mask off the host bits
		 * (Assuming we have a mask)
		 */
		if (netmask != NULL) {
			m = m_get(M_DONTWAIT, MT_SONAME);
			if (m == NULL)
				return(ENOBUFS);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, netmask);
			dst = deldst;
		}
		/*
		 * Look up an rtentry that is in the routing tree and
		 * contains the correct info.
		 */
		if ((rnh = rt_tables[dst->sa_family]) == NULL)
			goto bad;
		RADIX_NODE_HEAD_LOCK(rnh);
		error = ((rn = rnh->rnh_lookup(dst, netmask, rnh)) == NULL ||
		    (rn->rn_flags & RNF_ROOT) ||
		    ((struct rtentry *)rn)->rt_ifa != ifa ||
		    !sa_equal(SA(rn->rn_key), dst));
		RADIX_NODE_HEAD_UNLOCK(rnh);
		if (error) {
bad:
			if (m)
				(void) m_free(m);
			return (flags & RTF_HOST ? EHOSTUNREACH : ENETUNREACH);
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
	error = rtrequest1(cmd, &info, &nrt);
	if (error == 0 && (rt = nrt) != NULL) {
		/*
		 * notify any listening routing agents of the change
		 */
		RT_LOCK(rt);
		rt_newaddrmsg(cmd, ifa, error, rt);
		if (cmd == RTM_DELETE) {
			/*
			 * If we are deleting, and we found an entry, then
			 * it's been removed from the tree.. now throw it away.
			 */
			RTFREE_LOCKED(rt);
		} else {
			if (cmd == RTM_ADD) {
				/*
				 * We just wanted to add it.. we don't actually
				 * need a reference.
				 */
				rt->rt_refcnt--;
			}
			RT_UNLOCK(rt);
		}
	}
	if (m)
		(void) m_free(m);
	return (error);
}

/*
 * Validate the route rt0 to the specified destination.  If the
 * route is marked down try to find a new route.  If the route
 * to the gateway is gone, try to setup a new route.  Otherwise,
 * if the route is marked for packets to be rejected, enforce that.
 *
 * On return lrt contains the route to the destination and lrt0
 * contains the route to the next hop.  Their values are meaningul
 * ONLY if no error is returned.
 *
 * This routine is invoked on each layer 2 output path, prior to
 * encapsulating outbound packets.
 */
int
rt_check(struct rtentry **lrt, struct rtentry **lrt0, struct sockaddr *dst)
{
#define senderr(x) { error = x ; goto bad; }
	struct rtentry *rt;
	struct rtentry *rt0;
	int error;

	rt0 = *lrt0;
	rt = rt0;
	if (rt) {
		/* NB: the locking here is tortuous... */
		RT_LOCK(rt);
		if ((rt->rt_flags & RTF_UP) == 0) {
			RT_UNLOCK(rt);
			rt = rtalloc1(dst, 1, 0UL);
			if (rt != NULL) {
				rt->rt_refcnt--;
				RT_UNLOCK(rt);
			} else
				senderr(EHOSTUNREACH);
			rt0 = rt;
		}
		/* XXX BSD/OS checks dst->sa_family != AF_NS */
		if (rt->rt_flags & RTF_GATEWAY) {
			if (rt->rt_gwroute == 0)
				goto lookup;
			rt = rt->rt_gwroute;
			RT_LOCK(rt);		/* NB: gwroute */
			if ((rt->rt_flags & RTF_UP) == 0) {
				rtfree(rt);	/* unlock gwroute */
				rt = rt0;
			lookup:
				RT_UNLOCK(rt0);
				rt = rtalloc1(rt->rt_gateway, 1, 0UL);
				RT_LOCK(rt0);
				rt0->rt_gwroute = rt;
				if (rt == 0) {
					RT_UNLOCK(rt0);
					senderr(EHOSTUNREACH);
				}
			}
			RT_UNLOCK(rt0);
		}
		/* XXX why are we inspecting rmx_expire? */
		error = (rt->rt_flags & RTF_REJECT) &&
			(rt->rt_rmx.rmx_expire == 0 ||
				time_second < rt->rt_rmx.rmx_expire);
		RT_UNLOCK(rt);
		if (error)
			senderr(rt == rt0 ? EHOSTDOWN : EHOSTUNREACH);
	}
	*lrt = rt;		/* NB: return unlocked */
	*lrt0 = rt0;
	return (0);
bad:
	/* NB: lrt and lrt0 should not be interpreted if error is non-zero */
	return (error);
#undef senderr
}

/* This must be before ip6_init2(), which is now SI_ORDER_MIDDLE */
SYSINIT(route, SI_SUB_PROTO_DOMAIN, SI_ORDER_THIRD, route_init, 0);
