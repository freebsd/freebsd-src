/*
 * Copyright (c) 1983, 1988, 1993
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
 */

#ifndef lint
static char sccsid[] = "@(#)tables.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <errno.h>
#include <sys/syslog.h>

#ifndef DEBUG
#define	DEBUG	0
#endif

#ifdef RTM_ADD
#define FIXLEN(s) {if ((s)->sa_len == 0) (s)->sa_len = sizeof *(s);}
#else
#define FIXLEN(s) { }
#endif

int	install = !DEBUG;		/* if 1 call kernel */

/*
 * Lookup dst in the tables for an exact match.
 */
struct rt_entry *
rtlookup(dst)
	struct sockaddr *dst;
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register u_int hash;
	struct afhash h;
	int doinghost = 1;

	if (dst->sa_family >= af_max)
		return (0);
	(*afswitch[dst->sa_family].af_hash)(dst, &h);
	hash = h.afh_hosthash;
	rh = &hosthash[hash & ROUTEHASHMASK];
again:
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (equal(&rt->rt_dst, dst))
			return (rt);
	}
	if (doinghost) {
		doinghost = 0;
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
		goto again;
	}
	return (0);
}

struct sockaddr wildcard;	/* zero valued cookie for wildcard searches */

/*
 * Find a route to dst as the kernel would.
 */
struct rt_entry *
rtfind(dst)
	struct sockaddr *dst;
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register u_int hash;
	struct afhash h;
	int af = dst->sa_family;
	int doinghost = 1, (*match)();

	if (af >= af_max)
		return (0);
	(*afswitch[af].af_hash)(dst, &h);
	hash = h.afh_hosthash;
	rh = &hosthash[hash & ROUTEHASHMASK];

again:
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (doinghost) {
			if (equal(&rt->rt_dst, dst))
				return (rt);
		} else {
			if (rt->rt_dst.sa_family == af &&
			    (*match)(&rt->rt_dst, dst))
				return (rt);
		}
	}
	if (doinghost) {
		doinghost = 0;
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
		match = afswitch[af].af_netmatch;
		goto again;
	}
#ifdef notyet
	/*
	 * Check for wildcard gateway, by convention network 0.
	 */
	if (dst != &wildcard) {
		dst = &wildcard, hash = 0;
		goto again;
	}
#endif
	return (0);
}

rtadd(dst, gate, metric, state)
	struct sockaddr *dst, *gate;
	int metric, state;
{
	struct afhash h;
	register struct rt_entry *rt;
	struct rthash *rh;
	int af = dst->sa_family, flags;
	u_int hash;

	if (af >= af_max)
		return;
	(*afswitch[af].af_hash)(dst, &h);
	flags = (*afswitch[af].af_rtflags)(dst);
	/*
	 * Subnet flag isn't visible to kernel, move to state.	XXX
	 */
	FIXLEN(dst);
	FIXLEN(gate);
	if (flags & RTF_SUBNET) {
		state |= RTS_SUBNET;
		flags &= ~RTF_SUBNET;
	}
	if (flags & RTF_HOST) {
		hash = h.afh_hosthash;
		rh = &hosthash[hash & ROUTEHASHMASK];
	} else {
		hash = h.afh_nethash;
		rh = &nethash[hash & ROUTEHASHMASK];
	}
	rt = (struct rt_entry *)malloc(sizeof (*rt));
	if (rt == 0)
		return;
	rt->rt_hash = hash;
	rt->rt_dst = *dst;
	rt->rt_router = *gate;
	rt->rt_timer = 0;
	rt->rt_flags = RTF_UP | flags;
	rt->rt_state = state | RTS_CHANGED;
	rt->rt_ifp = if_ifwithdstaddr(&rt->rt_dst);
	if (rt->rt_ifp == 0)
		rt->rt_ifp = if_ifwithnet(&rt->rt_router);
	if ((state & RTS_INTERFACE) == 0)
		rt->rt_flags |= RTF_GATEWAY;
	rt->rt_metric = metric;
	insque(rt, rh);
	TRACE_ACTION("ADD", rt);
	/*
	 * If the ioctl fails because the gateway is unreachable
	 * from this host, discard the entry.  This should only
	 * occur because of an incorrect entry in /etc/gateways.
	 */
	if ((rt->rt_state & (RTS_INTERNAL | RTS_EXTERNAL)) == 0 &&
	    rtioctl(ADD, &rt->rt_rt) < 0) {
		if (errno != EEXIST && gate->sa_family < af_max)
			syslog(LOG_ERR,
			"adding route to net/host %s through gateway %s: %m\n",
			   (*afswitch[dst->sa_family].af_format)(dst),
			   (*afswitch[gate->sa_family].af_format)(gate));
		perror("ADD ROUTE");
		if (errno == ENETUNREACH) {
			TRACE_ACTION("DELETE", rt);
			remque(rt);
			free((char *)rt);
		}
	}
}

rtchange(rt, gate, metric)
	struct rt_entry *rt;
	struct sockaddr *gate;
	short metric;
{
	int add = 0, delete = 0, newgateway = 0;
	struct rtuentry oldroute;

	FIXLEN(gate);
	FIXLEN(&(rt->rt_router));
	FIXLEN(&(rt->rt_dst));
	if (!equal(&rt->rt_router, gate)) {
		newgateway++;
		TRACE_ACTION("CHANGE FROM ", rt);
	} else if (metric != rt->rt_metric)
		TRACE_NEWMETRIC(rt, metric);
	if ((rt->rt_state & RTS_INTERNAL) == 0) {
		/*
		 * If changing to different router, we need to add
		 * new route and delete old one if in the kernel.
		 * If the router is the same, we need to delete
		 * the route if has become unreachable, or re-add
		 * it if it had been unreachable.
		 */
		if (newgateway) {
			add++;
			if (rt->rt_metric != HOPCNT_INFINITY)
				delete++;
		} else if (metric == HOPCNT_INFINITY)
			delete++;
		else if (rt->rt_metric == HOPCNT_INFINITY)
			add++;
	}
	if (delete)
		oldroute = rt->rt_rt;
	if ((rt->rt_state & RTS_INTERFACE) && delete) {
		rt->rt_state &= ~RTS_INTERFACE;
		rt->rt_flags |= RTF_GATEWAY;
		if (metric > rt->rt_metric && delete)
			syslog(LOG_ERR, "%s route to interface %s (timed out)",
			    add? "changing" : "deleting",
			    rt->rt_ifp ? rt->rt_ifp->int_name : "?");
	}
	if (add) {
		rt->rt_router = *gate;
		rt->rt_ifp = if_ifwithdstaddr(&rt->rt_router);
		if (rt->rt_ifp == 0)
			rt->rt_ifp = if_ifwithnet(&rt->rt_router);
	}
	rt->rt_metric = metric;
	rt->rt_state |= RTS_CHANGED;
	if (newgateway)
		TRACE_ACTION("CHANGE TO   ", rt);
#ifndef RTM_ADD
	if (add && rtioctl(ADD, &rt->rt_rt) < 0)
		perror("ADD ROUTE");
	if (delete && rtioctl(DELETE, &oldroute) < 0)
		perror("DELETE ROUTE");
#else
	if (delete && !add) {
		if (rtioctl(DELETE, &oldroute) < 0)
			perror("DELETE ROUTE");
	} else if (!delete && add) {
		if (rtioctl(ADD, &rt->rt_rt) < 0)
			perror("ADD ROUTE");
	} else if (delete && add) {
		if (rtioctl(CHANGE, &rt->rt_rt) < 0)
			perror("CHANGE ROUTE");
	}
#endif
}

rtdelete(rt)
	struct rt_entry *rt;
{

	TRACE_ACTION("DELETE", rt);
	FIXLEN(&(rt->rt_router));
	FIXLEN(&(rt->rt_dst));
	if (rt->rt_metric < HOPCNT_INFINITY) {
	    if ((rt->rt_state & (RTS_INTERFACE|RTS_INTERNAL)) == RTS_INTERFACE)
		syslog(LOG_ERR,
		    "deleting route to interface %s? (timed out?)",
		    rt->rt_ifp->int_name);
	    if ((rt->rt_state & (RTS_INTERNAL | RTS_EXTERNAL)) == 0 &&
					    rtioctl(DELETE, &rt->rt_rt) < 0)
		    perror("rtdelete");
	}
	remque(rt);
	free((char *)rt);
}

rtdeleteall(sig)
	int sig;
{
	register struct rthash *rh;
	register struct rt_entry *rt;
	struct rthash *base = hosthash;
	int doinghost = 1;

again:
	for (rh = base; rh < &base[ROUTEHASHSIZ]; rh++) {
		rt = rh->rt_forw;
		for (; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
			if (rt->rt_state & RTS_INTERFACE ||
			    rt->rt_metric >= HOPCNT_INFINITY)
				continue;
			TRACE_ACTION("DELETE", rt);
			if ((rt->rt_state & (RTS_INTERNAL|RTS_EXTERNAL)) == 0 &&
			    rtioctl(DELETE, &rt->rt_rt) < 0)
				perror("rtdeleteall");
		}
	}
	if (doinghost) {
		doinghost = 0;
		base = nethash;
		goto again;
	}
	exit(sig);
}

/*
 * If we have an interface to the wide, wide world,
 * add an entry for an Internet default route (wildcard) to the internal
 * tables and advertise it.  This route is not added to the kernel routes,
 * but this entry prevents us from listening to other people's defaults
 * and installing them in the kernel here.
 */
rtdefault()
{
	extern struct sockaddr inet_default;

	rtadd(&inet_default, &inet_default, 1,
		RTS_CHANGED | RTS_PASSIVE | RTS_INTERNAL);
}

rtinit()
{
	register struct rthash *rh;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		rh->rt_forw = rh->rt_back = (struct rt_entry *)rh;
	for (rh = hosthash; rh < &hosthash[ROUTEHASHSIZ]; rh++)
		rh->rt_forw = rh->rt_back = (struct rt_entry *)rh;
}

rtioctl(action, ort)
	int action;
	struct rtuentry *ort;
{
#ifndef RTM_ADD
	if (install == 0)
		return (errno = 0);
	ort->rtu_rtflags = ort->rtu_flags;
	switch (action) {

	case ADD:
		return (ioctl(s, SIOCADDRT, (char *)ort));

	case DELETE:
		return (ioctl(s, SIOCDELRT, (char *)ort));

	default:
		return (-1);
	}
#else /* RTM_ADD */
	struct {
		struct rt_msghdr w_rtm;
		struct sockaddr_in w_dst;
		struct sockaddr w_gate;
		struct sockaddr_in w_netmask;
	} w;
#define rtm w.w_rtm

	bzero((char *)&w, sizeof(w));
	rtm.rtm_msglen = sizeof(w);
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = (action == ADD ? RTM_ADD :
				(action == DELETE ? RTM_DELETE : RTM_CHANGE));
#undef rt_dst
	rtm.rtm_flags = ort->rtu_flags;
	rtm.rtm_seq = ++seqno;
	rtm.rtm_addrs = RTA_DST|RTA_GATEWAY;
	bcopy((char *)&ort->rtu_dst, (char *)&w.w_dst, sizeof(w.w_dst));
	bcopy((char *)&ort->rtu_router, (char *)&w.w_gate, sizeof(w.w_gate));
	w.w_dst.sin_family = AF_INET;
	w.w_dst.sin_len = sizeof(w.w_dst);
	w.w_gate.sa_family = AF_INET;
	w.w_gate.sa_len = sizeof(w.w_gate);
	if (rtm.rtm_flags & RTF_HOST) {
		rtm.rtm_msglen -= sizeof(w.w_netmask);
	} else {
		register char *cp;
		int len;

		rtm.rtm_addrs |= RTA_NETMASK;
		w.w_netmask.sin_addr.s_addr =
			inet_maskof(w.w_dst.sin_addr.s_addr);
		for (cp = (char *)(1 + &w.w_netmask.sin_addr);
				    --cp > (char *) &w.w_netmask; )
			if (*cp)
				break;
		len = cp - (char *)&w.w_netmask;
		if (len) {
			len++;
			w.w_netmask.sin_len = len;
			len = 1 + ((len - 1) | (sizeof(long) - 1));
		} else 
			len = sizeof(long);
		rtm.rtm_msglen -= (sizeof(w.w_netmask) - len);
	}
	errno = 0;
	return (install ? write(r, (char *)&w, rtm.rtm_msglen) : (errno = 0));
#endif  /* RTM_ADD */
}
