/*
 * Copyright (c) 1988, 1991 Regents of the University of California.
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
 *	from: @(#)rtsock.c	7.18 (Berkeley) 6/27/91
 *	$Id: rtsock.c,v 1.6 1993/12/19 00:52:08 wollman Exp $
 */

#include "param.h"
#include "systm.h"
#include "mbuf.h"
#include "proc.h"
#include "socket.h"
#include "socketvar.h"
#include "domain.h"
#include "protosw.h"

#include "if.h"
#include "route.h"
#include "raw_cb.h"

#include "machine/mtpr.h"

static void rt_setmetrics(u_long, struct rt_metrics *, struct rt_metrics *);

struct route_cb route_cb;
struct sockaddr route_dst = { 2, PF_ROUTE, };
struct sockaddr route_src = { 2, PF_ROUTE, };
struct sockproto route_proto = { PF_ROUTE, };

/*ARGSUSED*/
int
route_usrreq(so, req, m, nam, control)
	register struct socket *so;
	int req;
	struct mbuf *m, *nam, *control;
{
	register int error = 0;
	register struct rawcb *rp = sotorawcb(so);
	int s;
	if (req == PRU_ATTACH) {
		MALLOC(rp, struct rawcb *, sizeof(*rp), M_PCB, M_WAITOK);
		if (so->so_pcb = (caddr_t)rp)
			bzero(so->so_pcb, sizeof(*rp));

	}
	if (req == PRU_DETACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (af == AF_INET)
			route_cb.ip_count--;
		else if (af == AF_NS)
			route_cb.ns_count--;
		else if (af == AF_ISO)
			route_cb.iso_count--;
		route_cb.any_count--;
	}
	s = splnet();
	error = raw_usrreq(so, req, m, nam, control, 0);
	rp = sotorawcb(so);
	if (req == PRU_ATTACH && rp) {
		int af = rp->rcb_proto.sp_protocol;
		if (error) {
			free((caddr_t)rp, M_PCB);
			splx(s);
			return (error);
		}
		if (af == AF_INET)
			route_cb.ip_count++;
		else if (af == AF_NS)
			route_cb.ns_count++;
		else if (af == AF_ISO)
			route_cb.iso_count++;
		rp->rcb_faddr = &route_src;
		route_cb.any_count++;
		soisconnected(so);
		so->so_options |= SO_USELOOPBACK;
	}
	splx(s);
	return (error);
}
#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/*ARGSUSED*/
int
route_output(m, so)
	register struct mbuf *m;
	struct socket *so;
{
	register struct rt_msghdr *rtm = 0;
	register struct rtentry *rt = 0;
	struct rtentry *saved_nrt = 0;
	struct sockaddr *dst = 0, *gate = 0, *netmask = 0, *genmask = 0;
	struct sockaddr *ifpaddr = 0, *ifaaddr = 0;
	caddr_t cp, lim;
	int len, error = 0;
	struct ifnet *ifp = 0;
	struct ifaddr *ifa = 0;
	struct ifaddr *ifaof_ifpforaddr(), *ifa_ifwithroute();

#define senderr(e) { error = e; goto flush;}
	if (m == 0 || m->m_len < sizeof(long))
		return (ENOBUFS);
	if ((m = m_pullup(m, sizeof(long))) == 0)
		return (ENOBUFS);
	if ((m->m_flags & M_PKTHDR) == 0)
		panic("route_output");
	len = m->m_pkthdr.len;
	if (len < sizeof(*rtm) ||
	    len != mtod(m, struct rt_msghdr *)->rtm_msglen)
		senderr(EINVAL);
	R_Malloc(rtm, struct rt_msghdr *, len);
	if (rtm == 0)
		senderr(ENOBUFS);
	m_copydata(m, 0, len, (caddr_t)rtm);
	if (rtm->rtm_version != RTM_VERSION)
		senderr(EPROTONOSUPPORT);
	rtm->rtm_pid = curproc->p_pid;
	lim = len + (caddr_t) rtm;
	cp = (caddr_t) (rtm + 1);
	if (rtm->rtm_addrs & RTA_DST) {
		dst = (struct sockaddr *)cp;
		ADVANCE(cp, dst);
	} else
		senderr(EINVAL);
	if ((rtm->rtm_addrs & RTA_GATEWAY) && cp < lim)  {
		gate = (struct sockaddr *)cp;
		ADVANCE(cp, gate);
	}
	if ((rtm->rtm_addrs & RTA_NETMASK) && cp < lim)  {
		netmask = (struct sockaddr *)cp;
		ADVANCE(cp, netmask);
	}
	if ((rtm->rtm_addrs & RTA_GENMASK) && cp < lim)  {
		struct radix_node *t, *rn_addmask();
		genmask = (struct sockaddr *)cp;
		ADVANCE(cp, genmask);
		t = rn_addmask(genmask, 1, 2);
		if (t && Bcmp(genmask, t->rn_key, *(u_char *)genmask) == 0)
			genmask = (struct sockaddr *)(t->rn_key);
		else
			senderr(ENOBUFS);
	}
	if ((rtm->rtm_addrs & RTA_IFP) && cp < lim)  {
		ifpaddr = (struct sockaddr *)cp;
		ADVANCE(cp, ifpaddr);
	}
	if ((rtm->rtm_addrs & RTA_IFA) && cp < lim)  {
		ifaaddr = (struct sockaddr *)cp;
	}
	switch (rtm->rtm_type) {
	case RTM_ADD:
		if (gate == 0)
			senderr(EINVAL);
		error = rtrequest(RTM_ADD, dst, gate, netmask,
					rtm->rtm_flags, &saved_nrt);
		if (error == 0 && saved_nrt) {
			rt_setmetrics(rtm->rtm_inits,
				&rtm->rtm_rmx, &saved_nrt->rt_rmx);
			saved_nrt->rt_refcnt--;
			saved_nrt->rt_genmask = genmask;
		}
		break;

	case RTM_DELETE:
		error = rtrequest(RTM_DELETE, dst, gate, netmask,
				rtm->rtm_flags, (struct rtentry **)0);
		break;

	case RTM_GET:
	case RTM_CHANGE:
	case RTM_LOCK:
		rt = rtalloc1(dst, 0);
		if (rt == 0)
			senderr(ESRCH);
		if (rtm->rtm_type != RTM_GET) {
			if (Bcmp(dst, rt_key(rt), dst->sa_len) != 0)
				senderr(ESRCH);
			if (rt->rt_nodes->rn_dupedkey &&
			    (netmask == 0 ||
			     Bcmp(netmask, rt_mask(rt), netmask->sa_len)))
				senderr(ETOOMANYREFS);
		}
		switch(rtm->rtm_type) {

		case RTM_GET:
			dst = rt_key(rt); len = sizeof(*rtm);
			ADVANCE(len, dst);
			rtm->rtm_addrs |= RTA_DST;
			if (gate = rt->rt_gateway) {
				ADVANCE(len, gate);
				rtm->rtm_addrs |= RTA_GATEWAY;
			} else
				rtm->rtm_addrs &= ~RTA_GATEWAY;
			if (netmask = rt_mask(rt)) {
				ADVANCE(len, netmask);
				rtm->rtm_addrs |= RTA_NETMASK;
			} else
				rtm->rtm_addrs &= ~RTA_NETMASK;
			if (genmask = rt->rt_genmask) {
				ADVANCE(len, genmask);
				rtm->rtm_addrs |= RTA_GENMASK;
			} else
				rtm->rtm_addrs &= ~RTA_GENMASK;
			if (rtm->rtm_addrs & (RTA_IFP | RTA_IFA)) {
				if (rt->rt_ifp == 0)
					goto badif;
				for (ifa = rt->rt_ifp->if_addrlist;
				    ifa && ifa->ifa_addr->sa_family != AF_LINK;
				     ifa = ifa->ifa_next){}
				if (ifa && rt->rt_ifa) {
					ifpaddr = ifa->ifa_addr;
					ADVANCE(len, ifpaddr);
					ifaaddr = rt->rt_ifa->ifa_addr;
					ADVANCE(len, ifaaddr);
					rtm->rtm_addrs |= RTA_IFP | RTA_IFA;
				} else {
				badif:	ifpaddr = 0;
					rtm->rtm_addrs &= ~(RTA_IFP | RTA_IFA);
				}
			}
			if (len > rtm->rtm_msglen) {
				struct rt_msghdr *new_rtm;
				R_Malloc(new_rtm, struct rt_msghdr *, len);
				if (new_rtm == 0)
					senderr(ENOBUFS);
				Bcopy(rtm, new_rtm, rtm->rtm_msglen);
				Free(rtm); rtm = new_rtm;
			}
			rtm->rtm_msglen = len;
			rtm->rtm_flags = rt->rt_flags;
			rtm->rtm_rmx = rt->rt_rmx;
			cp = (caddr_t) (1 + rtm);
			len = ROUNDUP(dst->sa_len); 
			Bcopy(dst, cp, len); cp += len;
			if (gate) {
			    len = ROUNDUP(gate->sa_len);
			    Bcopy(gate, cp, len); cp += len;
			}
			if (netmask) {
			    len = ROUNDUP(netmask->sa_len);
			    Bcopy(netmask, cp, len); cp += len;
			}
			if (genmask) {
			    len = ROUNDUP(genmask->sa_len);
			    Bcopy(genmask, cp, len); cp += len;
			}
			if (ifpaddr) {
			    len = ROUNDUP(ifpaddr->sa_len);
			    Bcopy(ifpaddr, cp, len); cp += len;
			    len = ROUNDUP(ifaaddr->sa_len);
			    Bcopy(ifaaddr, cp, len); cp += len;
			}
			break;

		case RTM_CHANGE:
			if (gate &&
			    (gate->sa_len > (len = rt->rt_gateway->sa_len)))
				senderr(EDQUOT);
			/* new gateway could require new ifaddr, ifp;
			   flags may also be different; ifp may be specified
			   by ll sockaddr when protocol address is ambiguous */
			if (ifpaddr && (ifa = ifa_ifwithnet(ifpaddr)) &&
			    (ifp = ifa->ifa_ifp))
				ifa = ifaof_ifpforaddr(ifaaddr ? ifaaddr : gate,
							ifp);
			else if ((ifaaddr && (ifa = ifa_ifwithaddr(ifaaddr))) ||
				 (ifa = ifa_ifwithroute(rt->rt_flags,
							rt_key(rt), gate)))
				ifp = ifa->ifa_ifp;
			if (ifa) {
				register struct ifaddr *oifa = rt->rt_ifa;
				if (oifa != ifa) {
				    if (oifa && oifa->ifa_rtrequest)
					oifa->ifa_rtrequest(RTM_DELETE,
								rt, gate);
				    rt->rt_ifa = ifa;
				    rt->rt_ifp = ifp;
				}
			}
			if (gate)
				Bcopy(gate, rt->rt_gateway, len);
			rt_setmetrics(rtm->rtm_inits, &rtm->rtm_rmx,
					&rt->rt_rmx);
			if (rt->rt_ifa && rt->rt_ifa->ifa_rtrequest)
			       rt->rt_ifa->ifa_rtrequest(RTM_ADD, rt, gate);
			if (genmask)
				rt->rt_genmask = genmask;
			/*
			 * Fall into
			 */
		case RTM_LOCK:
			rt->rt_rmx.rmx_locks |=
				(rtm->rtm_inits & rtm->rtm_rmx.rmx_locks);
			rt->rt_rmx.rmx_locks &= ~(rtm->rtm_inits);
			break;
		}
		goto cleanup;

	default:
		senderr(EOPNOTSUPP);
	}

flush:
	if (rtm) {
		if (error)
			rtm->rtm_errno = error;
		else 
			rtm->rtm_flags |= RTF_DONE;
	}
cleanup:
	if (rt)
		rtfree(rt);
    {
	register struct rawcb *rp = 0;
	/*
	 * Check to see if we don't want our own messages.
	 */
	if ((so->so_options & SO_USELOOPBACK) == 0) {
		if (route_cb.any_count <= 1) {
			if (rtm)
				Free(rtm);
			m_freem(m);
			return (error);
		}
		/* There is another listener, so construct message */
		rp = sotorawcb(so);
	}
	if (rtm) {
		m_copyback(m, 0, rtm->rtm_msglen, (caddr_t)rtm);
		Free(rtm);
	}
	if (rp)
		rp->rcb_proto.sp_family = 0; /* Avoid us */
	if (dst)
		route_proto.sp_protocol = dst->sa_family;
	raw_input(m, &route_proto, &route_src, &route_dst);
	if (rp)
		rp->rcb_proto.sp_family = PF_ROUTE;
    }
	return (error);
}

void
rt_setmetrics(which, in, out)
	u_long which;
	register struct rt_metrics *in, *out;
{
#define metric(f, e) if (which & (f)) out->e = in->e;
	metric(RTV_RPIPE, rmx_recvpipe);
	metric(RTV_SPIPE, rmx_sendpipe);
	metric(RTV_SSTHRESH, rmx_ssthresh);
	metric(RTV_RTT, rmx_rtt);
	metric(RTV_RTTVAR, rmx_rttvar);
	metric(RTV_HOPCOUNT, rmx_hopcount);
	metric(RTV_MTU, rmx_mtu);
	metric(RTV_EXPIRE, rmx_expire);
#undef metric
}

/* 
 * The miss message and losing message are very similar.
 */

void
rt_missmsg(type, dst, gate, mask, src, flags, error)
	int type;
	register struct sockaddr *dst;
	struct sockaddr *gate;
	struct sockaddr *mask;
	struct sockaddr *src;
	int flags;
	int error;
{
	register struct rt_msghdr *rtm;
	register struct mbuf *m;
	int dlen = ROUNDUP(dst->sa_len);
	int len = dlen + sizeof(*rtm);

	if (route_cb.any_count == 0)
		return;
	m = m_gethdr(M_DONTWAIT, MT_DATA);
	if (m == 0)
		return;
	m->m_pkthdr.len = m->m_len = min(len, MHLEN);
	m->m_pkthdr.rcvif = 0;
	rtm = mtod(m, struct rt_msghdr *);
	bzero((caddr_t)rtm, sizeof(*rtm)); /*XXX assumes sizeof(*rtm) < MHLEN*/
	rtm->rtm_flags = RTF_DONE | flags;
	rtm->rtm_msglen = len;
	rtm->rtm_version = RTM_VERSION;
	rtm->rtm_type = type;
	rtm->rtm_addrs = RTA_DST;
	if (type == RTM_OLDADD || type == RTM_OLDDEL) {
		rtm->rtm_pid = curproc->p_pid;
	}
	m_copyback(m, sizeof (*rtm), dlen, (caddr_t)dst);
	if (gate) {
		dlen = ROUNDUP(gate->sa_len);
		m_copyback(m, len ,  dlen, (caddr_t)gate);
		len += dlen;
		rtm->rtm_addrs |= RTA_GATEWAY;
	}
	if (mask) {
		dlen = ROUNDUP(mask->sa_len);
		m_copyback(m, len ,  dlen, (caddr_t)mask);
		len += dlen;
		rtm->rtm_addrs |= RTA_NETMASK;
	}
	if (src) {
		dlen = ROUNDUP(src->sa_len);
		m_copyback(m, len ,  dlen, (caddr_t)src);
		len += dlen;
		rtm->rtm_addrs |= RTA_AUTHOR;
	}
	if (m->m_pkthdr.len != len) {
		m_freem(m);
		return;
	}
	rtm->rtm_errno = error;
	rtm->rtm_msglen = len;
	route_proto.sp_protocol = dst->sa_family;
	raw_input(m, &route_proto, &route_src, &route_dst);
}

#include "kinfo.h"
struct walkarg {
	int	w_op, w_arg;
	int	w_given, w_needed;
	caddr_t	w_where;
	struct	{
		struct rt_msghdr m_rtm;
		char	m_sabuf[128];
	} w_m;
#define w_rtm w_m.m_rtm
};
/*
 * This is used in dumping the kernel table via getkinfo().
 */
int
rt_dumpentry(rn, w)
	struct radix_node *rn;
	register struct walkarg *w;
{
	register struct sockaddr *sa;
	int n, error;

    for (; rn; rn = rn->rn_dupedkey) {
	int count = 0, size = sizeof(w->w_rtm);
	register struct rtentry *rt = (struct rtentry *)rn;

	if (rn->rn_flags & RNF_ROOT)
		continue;
	if (w->w_op == KINFO_RT_FLAGS && !(rt->rt_flags & w->w_arg))
		continue;
#define next(a, l) {size += (l); w->w_rtm.rtm_addrs |= (a); }
	w->w_rtm.rtm_addrs = 0;
	if (sa = rt_key(rt))
		next(RTA_DST, ROUNDUP(sa->sa_len));
	if (sa = rt->rt_gateway)
		next(RTA_GATEWAY, ROUNDUP(sa->sa_len));
	if (sa = rt_mask(rt))
		next(RTA_NETMASK, ROUNDUP(sa->sa_len));
	if (sa = rt->rt_genmask)
		next(RTA_GENMASK, ROUNDUP(sa->sa_len));
	w->w_needed += size;
	if (w->w_where == NULL || w->w_needed > 0)
		continue;
	w->w_rtm.rtm_msglen = size;
	w->w_rtm.rtm_flags = rt->rt_flags;
	w->w_rtm.rtm_use = rt->rt_use;
	w->w_rtm.rtm_rmx = rt->rt_rmx;
	w->w_rtm.rtm_index = rt->rt_ifp->if_index;
#undef next
#define next(l) {n = (l); Bcopy(sa, cp, n); cp += n;}
	if (size <= sizeof(w->w_m)) {
		register caddr_t cp = (caddr_t)(w->w_m.m_sabuf);
		if (sa = rt_key(rt))
			next(ROUNDUP(sa->sa_len));
		if (sa = rt->rt_gateway)
			next(ROUNDUP(sa->sa_len));
		if (sa = rt_mask(rt))
			next(ROUNDUP(sa->sa_len));
		if (sa = rt->rt_genmask)
			next(ROUNDUP(sa->sa_len));
#undef next
#define next(s, l) {n = (l); \
    if (error = copyout((caddr_t)(s), w->w_where, n)) return (error); \
    w->w_where += n;}

		next(&w->w_m, size); /* Copy rtmsg and sockaddrs back */
		continue;
	}
	next(&w->w_rtm, sizeof(w->w_rtm));
	if (sa = rt_key(rt))
		next(sa, ROUNDUP(sa->sa_len));
	if (sa = rt->rt_gateway)
		next(sa, ROUNDUP(sa->sa_len));
	if (sa = rt_mask(rt))
		next(sa, ROUNDUP(sa->sa_len));
	if (sa = rt->rt_genmask)
		next(sa, ROUNDUP(sa->sa_len));
    }
	return (0);
#undef next
}

int
kinfo_rtable(op, where, given, arg, needed)
	int	op, arg;
	caddr_t	where;
	int	*given, *needed;
{
	register struct radix_node_head *rnh;
	int	s, error = 0;
	u_char  af = ki_af(op);
	struct	walkarg w;

	op &= 0xffff;
	if (op != KINFO_RT_DUMP && op != KINFO_RT_FLAGS)
		return (EINVAL);

	Bzero(&w, sizeof(w));
	if ((w.w_where = where) && given)
		w.w_given = *given;
	w.w_needed = 0 - w.w_given;
	w.w_arg = arg;
	w.w_op = op;
	w.w_rtm.rtm_version = RTM_VERSION;
	w.w_rtm.rtm_type = RTM_GET;

	s = splnet();
	for (rnh = radix_node_head; rnh; rnh = rnh->rnh_next) {
		if (rnh->rnh_af == 0)
			continue;
		if (af && af != rnh->rnh_af)
			continue;
		error = rt_walk(rnh->rnh_treetop, rt_dumpentry, &w);
		if (error)
			break;
	}
	w.w_needed += w.w_given;
	if (where && given)
		*given = w.w_where - where;
	else
		w.w_needed = (11 * w.w_needed) / 10;
	*needed = w.w_needed;
	splx(s);
	return (error);
}

int
rt_walk(rn, f, w)
	register struct radix_node *rn;
	register int (*f)();
	struct walkarg *w;
{
	int error;
	for (;;) {
		while (rn->rn_b >= 0)
			rn = rn->rn_l;	/* First time through node, go left */
		if (error = (*f)(rn, w))
			return (error);	/* Process Leaf */
		while (rn->rn_p->rn_r == rn) {	/* if coming back from right */
			rn = rn->rn_p;		/* go back up */
			if (rn->rn_flags & RNF_ROOT)
				return 0;
		}
		rn = rn->rn_p->rn_r;		/* otherwise, go right*/
	}
}

/*
 * Definitions of protocols supported in the ROUTE domain.
 */

extern	struct domain routedomain;		/* or at least forward */

struct protosw routesw[] = {
{ SOCK_RAW,	&routedomain,	0,		PR_ATOMIC|PR_ADDR,
  raw_input,	route_output,	raw_ctlinput,	0,
  route_usrreq,
  raw_init,	0,		0,		0,
}
};

int	unp_externalize(), unp_dispose();

struct domain routedomain =
    { PF_ROUTE, "route", 0, 0, 0,
      routesw, &routesw[sizeof(routesw)/sizeof(routesw[0])] };
