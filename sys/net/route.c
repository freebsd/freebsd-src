/*
 * Copyright (c) 1980, 1986, 1991 Regents of the University of California.
 * All rights reserved.
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
 *	from: @(#)route.c	7.22 (Berkeley) 6/27/91
 *	$Id: route.c,v 1.2 1993/10/16 17:43:39 rgrimes Exp $
 */

#include "param.h"
#include "systm.h"
#include "proc.h"
#include "mbuf.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"
#include "ioctl.h"

#include "if.h"
#include "af.h"
#include "route.h"
#include "raw_cb.h"

#include "../netinet/in.h"
#include "../netinet/in_var.h"

#ifdef NS
#include "../netns/ns.h"
#endif
#include "machine/mtpr.h"
#include "netisr.h"

#define	SA(p) ((struct sockaddr *)(p))

int	rttrash;		/* routes not in table but not freed */
struct	sockaddr wildcard;	/* zero valued cookie for wildcard searches */
int	rthashsize = RTHASHSIZ;	/* for netstat, etc. */

static int rtinits_done = 0;
struct radix_node_head *ns_rnhead, *in_rnhead;
struct radix_node *rn_match(), *rn_delete(), *rn_addroute();

rtinitheads()
{
	if (rtinits_done == 0 &&
#ifdef NS
	    rn_inithead(&ns_rnhead, 16, AF_NS) &&
#endif
	    rn_inithead(&in_rnhead, 32, AF_INET))
		rtinits_done = 1;
}

/*
 * Packet routing routines.
 */
rtalloc(ro)
	register struct route *ro;
{
	if (ro->ro_rt && ro->ro_rt->rt_ifp && (ro->ro_rt->rt_flags & RTF_UP))
		return;				 /* XXX */
	ro->ro_rt = rtalloc1(&ro->ro_dst, 1);
}

struct rtentry *
rtalloc1(dst, report)
	register struct sockaddr *dst;
	int  report;
{
	register struct radix_node_head *rnh;
	register struct rtentry *rt;
	register struct radix_node *rn;
	struct rtentry *newrt = 0;
	int  s = splnet(), err = 0, msgtype = RTM_MISS;

	for (rnh = radix_node_head; rnh && (dst->sa_family != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh && rnh->rnh_treetop &&
	    (rn = rn_match((caddr_t)dst, rnh->rnh_treetop)) &&
	    ((rn->rn_flags & RNF_ROOT) == 0)) {
		newrt = rt = (struct rtentry *)rn;
		if (report && (rt->rt_flags & RTF_CLONING)) {
			if ((err = rtrequest(RTM_RESOLVE, dst, SA(0),
					      SA(0), 0, &newrt)) ||
			    ((rt->rt_flags & RTF_XRESOLVE)
			      && (msgtype = RTM_RESOLVE))) /* intended! */
			    goto miss;
		} else
			rt->rt_refcnt++;
	} else {
		rtstat.rts_unreach++;
	miss:	if (report)
			rt_missmsg(msgtype, dst, SA(0), SA(0), SA(0), 0, err);
	}
	splx(s);
	return (newrt);
}

rtfree(rt)
	register struct rtentry *rt;
{
	register struct ifaddr *ifa;
	if (rt == 0)
		panic("rtfree");
	rt->rt_refcnt--;
	if (rt->rt_refcnt <= 0 && (rt->rt_flags & RTF_UP) == 0) {
		rttrash--;
		if (rt->rt_nodes->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtfree 2");
		free((caddr_t)rt, M_RTABLE);
	}
}

/*
 * Force a routing table entry to the specified
 * destination to go through the given gateway.
 * Normally called as a result of a routing redirect
 * message from the network layer.
 *
 * N.B.: must be called at splnet
 *
 */
rtredirect(dst, gateway, netmask, flags, src, rtp)
	struct sockaddr *dst, *gateway, *netmask, *src;
	int flags;
	struct rtentry **rtp;
{
	register struct rtentry *rt = 0;
	int error = 0;
	short *stat = 0;

	/* verify the gateway is directly reachable */
	if (ifa_ifwithnet(gateway) == 0) {
		error = ENETUNREACH;
		goto done;
	}
	rt = rtalloc1(dst, 0);
	/*
	 * If the redirect isn't from our current router for this dst,
	 * it's either old or wrong.  If it redirects us to ourselves,
	 * we have a routing loop, perhaps as a result of an interface
	 * going down recently.
	 */
#define	equal(a1, a2) (bcmp((caddr_t)(a1), (caddr_t)(a2), (a1)->sa_len) == 0)
	if (!(flags & RTF_DONE) && rt && !equal(src, rt->rt_gateway))
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
	if ((rt == 0) || (rt_mask(rt) && rt_mask(rt)->sa_len < 2))
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
			flags |=  RTF_GATEWAY | RTF_DYNAMIC;
			error = rtrequest((int)RTM_ADD, dst, gateway,
				    SA(0), flags,
				    (struct rtentry **)0);
			stat = &rtstat.rts_dynamic;
		} else {
			/*
			 * Smash the current notion of the gateway to
			 * this destination.  Should check about netmask!!!
			 */
			if (gateway->sa_len <= rt->rt_gateway->sa_len) {
				Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
				rt->rt_flags |= RTF_MODIFIED;
				flags |= RTF_MODIFIED;
				stat = &rtstat.rts_newgateway;
			} else
				error = ENOSPC;
		}
	} else
		error = EHOSTUNREACH;
done:
	if (rt) {
		if (rtp && !error)
			*rtp = rt;
		else
			rtfree(rt);
	}
	if (error)
		rtstat.rts_badredirect++;
	else
		(stat && (*stat)++);
	rt_missmsg(RTM_REDIRECT, dst, gateway, netmask, src, flags, error);
}

/*
* Routing table ioctl interface.
*/
rtioctl(req, data, p)
	int req;
	caddr_t data;
	struct proc *p;
{
#ifndef COMPAT_43
	return (EOPNOTSUPP);
#else
	register struct ortentry *entry = (struct ortentry *)data;
	int error;
	struct sockaddr *netmask = 0;

	if (req == SIOCADDRT)
		req = RTM_ADD;
	else if (req == SIOCDELRT)
		req = RTM_DELETE;
	else
		return (EINVAL);

	if (error = suser(p->p_ucred, &p->p_acflag))
		return (error);
#if BYTE_ORDER != BIG_ENDIAN
	if (entry->rt_dst.sa_family == 0 && entry->rt_dst.sa_len < 16) {
		entry->rt_dst.sa_family = entry->rt_dst.sa_len;
		entry->rt_dst.sa_len = 16;
	}
	if (entry->rt_gateway.sa_family == 0 && entry->rt_gateway.sa_len < 16) {
		entry->rt_gateway.sa_family = entry->rt_gateway.sa_len;
		entry->rt_gateway.sa_len = 16;
	}
#else
	if (entry->rt_dst.sa_len == 0)
		entry->rt_dst.sa_len = 16;
	if (entry->rt_gateway.sa_len == 0)
		entry->rt_gateway.sa_len = 16;
#endif
	if ((entry->rt_flags & RTF_HOST) == 0)
		switch (entry->rt_dst.sa_family) {
#ifdef INET
		case AF_INET:
			{
				extern struct sockaddr_in icmpmask;
				struct sockaddr_in *dst_in = 
					(struct sockaddr_in *)&entry->rt_dst;

				in_sockmaskof(dst_in->sin_addr, &icmpmask);
				netmask = (struct sockaddr *)&icmpmask;
			}
			break;
#endif
#ifdef NS
		case AF_NS:
			{
				extern struct sockaddr_ns ns_netmask;
				netmask = (struct sockaddr *)&ns_netmask;
			}
#endif
		}
	error =  rtrequest(req, &(entry->rt_dst), &(entry->rt_gateway), netmask,
				entry->rt_flags, (struct rtentry **)0);
	rt_missmsg((req == RTM_ADD ? RTM_OLDADD : RTM_OLDDEL),
		   &(entry->rt_dst), &(entry->rt_gateway),
		   netmask, SA(0), entry->rt_flags, error);
	return (error);
#endif
}

struct ifaddr *
ifa_ifwithroute(flags, dst, gateway)
int	flags;
struct sockaddr	*dst, *gateway;
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
		if (flags & RTF_HOST) 
			ifa = ifa_ifwithdstaddr(dst);
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
		struct rtentry *rt = rtalloc1(dst, 0);
		if (rt == 0)
			return (0);
		rt->rt_refcnt--;
		if ((ifa = rt->rt_ifa) == 0)
			return (0);
	}
	if (ifa->ifa_addr->sa_family != dst->sa_family) {
		struct ifaddr *oifa = ifa, *ifaof_ifpforaddr();
		ifa = ifaof_ifpforaddr(dst, ifa->ifa_ifp);
		if (ifa == 0)
			ifa = oifa;
	}
	return (ifa);
}

#define ROUNDUP(a) (a>0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

rtrequest(req, dst, gateway, netmask, flags, ret_nrt)
	int req, flags;
	struct sockaddr *dst, *gateway, *netmask;
	struct rtentry **ret_nrt;
{
	int s = splnet(), len, error = 0;
	register struct rtentry *rt;
	register struct radix_node *rn;
	register struct radix_node_head *rnh;
	struct ifaddr *ifa, *ifa_ifwithdstaddr();
	struct sockaddr *ndst;
	u_char af = dst->sa_family;
#define senderr(x) { error = x ; goto bad; }

	if (rtinits_done == 0)
		rtinitheads();
	for (rnh = radix_node_head; rnh && (af != rnh->rnh_af); )
		rnh = rnh->rnh_next;
	if (rnh == 0)
		senderr(ESRCH);
	if (flags & RTF_HOST)
		netmask = 0;
	switch (req) {
	case RTM_DELETE:
		if (ret_nrt && (rt = *ret_nrt)) {
			RTFREE(rt);
			*ret_nrt = 0;
		}
		if ((rn = rn_delete((caddr_t)dst, (caddr_t)netmask, 
					rnh->rnh_treetop)) == 0)
			senderr(ESRCH);
		if (rn->rn_flags & (RNF_ACTIVE | RNF_ROOT))
			panic ("rtrequest delete");
		rt = (struct rtentry *)rn;
		rt->rt_flags &= ~RTF_UP;
		if ((ifa = rt->rt_ifa) && ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(RTM_DELETE, rt, SA(0));
		rttrash++;
		if (rt->rt_refcnt <= 0)
			rtfree(rt);
		break;

	case RTM_RESOLVE:
		if (ret_nrt == 0 || (rt = *ret_nrt) == 0)
			senderr(EINVAL);
		ifa = rt->rt_ifa;
		flags = rt->rt_flags & ~RTF_CLONING;
		gateway = rt->rt_gateway;
		if ((netmask = rt->rt_genmask) == 0)
			flags |= RTF_HOST;
		goto makeroute;

	case RTM_ADD:
		if ((ifa = ifa_ifwithroute(flags, dst, gateway)) == 0)
			senderr(ENETUNREACH);
	makeroute:
		len = sizeof (*rt) + ROUNDUP(gateway->sa_len)
		    + ROUNDUP(dst->sa_len);
		R_Malloc(rt, struct rtentry *, len);
		if (rt == 0)
			senderr(ENOBUFS);
		Bzero(rt, len);
		ndst = (struct sockaddr *)(rt + 1);
		if (netmask) {
			rt_maskedcopy(dst, ndst, netmask);
		} else
			Bcopy(dst, ndst, dst->sa_len);
		rn = rn_addroute((caddr_t)ndst, (caddr_t)netmask,
					rnh->rnh_treetop, rt->rt_nodes);
		if (rn == 0) {
			free((caddr_t)rt, M_RTABLE);
			senderr(EEXIST);
		}
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
		rt->rt_flags = RTF_UP | flags;
		rt->rt_gateway = (struct sockaddr *)
					(rn->rn_key + ROUNDUP(dst->sa_len));
		Bcopy(gateway, rt->rt_gateway, gateway->sa_len);
		if (req == RTM_RESOLVE)
			rt->rt_rmx = (*ret_nrt)->rt_rmx; /* copy metrics */
		if (ifa->ifa_rtrequest)
			ifa->ifa_rtrequest(req, rt, SA(ret_nrt ? *ret_nrt : 0));
		if (ret_nrt) {
			*ret_nrt = rt;
			rt->rt_refcnt++;
		}
		break;
	}
bad:
	splx(s);
	return (error);
}

rt_maskedcopy(src, dst, netmask)
struct sockaddr *src, *dst, *netmask;
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
rtinit(ifa, cmd, flags)
	register struct ifaddr *ifa;
	int cmd, flags;
{
	register struct rtentry *rt;
	register struct sockaddr *dst;
	register struct sockaddr *deldst;
	struct mbuf *m = 0;
	int error;

	dst = flags & RTF_HOST ? ifa->ifa_dstaddr : ifa->ifa_addr;
	if (ifa->ifa_flags & IFA_ROUTE) {
		if ((rt = ifa->ifa_rt) && (rt->rt_flags & RTF_UP) == 0) {
			RTFREE(rt);
			ifa->ifa_rt = 0;
		}
	}
	if (cmd == RTM_DELETE) {
		if ((flags & RTF_HOST) == 0 && ifa->ifa_netmask) {
			m = m_get(M_WAIT, MT_SONAME);
			deldst = mtod(m, struct sockaddr *);
			rt_maskedcopy(dst, deldst, ifa->ifa_netmask);
			dst = deldst;
		}
		if (rt = rtalloc1(dst, 0)) {
			rt->rt_refcnt--;
			if (rt->rt_ifa != ifa) {
				if (m)
					(void) m_free(m);
				return (flags & RTF_HOST ? EHOSTUNREACH
							: ENETUNREACH);
			}
		}
	}
	error = rtrequest(cmd, dst, ifa->ifa_addr, ifa->ifa_netmask,
			flags | ifa->ifa_flags, &ifa->ifa_rt);
	if (m)
		(void) m_free(m);
	if (cmd == RTM_ADD && error == 0 && (rt = ifa->ifa_rt)
						&& rt->rt_ifa != ifa) {
		rt->rt_ifa = ifa;
		rt->rt_ifp = ifa->ifa_ifp;
	}
	return (error);
}
