/*
 * Copyright (c) 1985, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * Copyright (c) 1995 John Hay.  All rights reserved.
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
 * $FreeBSD$
 */

#ifndef lint
static const char sccsid[] = "@(#)tables.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

/*
 * Routing Table Management Daemon
 */
#include "defs.h"
#include <sys/ioctl.h>
#include <errno.h>
#include <search.h>
#include <stdlib.h>
#include <unistd.h>

#ifndef DEBUG
#define	DEBUG	0
#endif

#define FIXLEN(s) { if ((s)->sa_len == 0) (s)->sa_len = sizeof (*(s));}

int	install = !DEBUG;		/* if 1 call kernel */
int	delete = 1;

struct  rthash nethash[ROUTEHASHSIZ];

/*
 * Lookup dst in the tables for an exact match.
 */
struct rt_entry *
rtlookup(struct sockaddr *dst)
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register u_int hash;
	struct afhash h;

	if (dst->sa_family >= AF_MAX)
		return (0);
	(*afswitch[dst->sa_family].af_hash)(dst, &h);
	hash = h.afh_nethash;
	rh = &nethash[hash & ROUTEHASHMASK];
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (equal(&rt->rt_dst, dst))
			return (rt);
	}
	return (0);
}

/*
 * Find a route to dst as the kernel would.
 */
struct rt_entry *
rtfind(struct sockaddr *dst)
{
	register struct rt_entry *rt;
	register struct rthash *rh;
	register u_int hash;
	struct afhash h;
	int af = dst->sa_family;
	int (*match)() = 0;

	if (af >= AF_MAX)
		return (0);
	(*afswitch[af].af_hash)(dst, &h);

	hash = h.afh_nethash;
	rh = &nethash[hash & ROUTEHASHMASK];
	match = afswitch[af].af_netmatch;
	for (rt = rh->rt_forw; rt != (struct rt_entry *)rh; rt = rt->rt_forw) {
		if (rt->rt_hash != hash)
			continue;
		if (rt->rt_dst.sa_family == af &&
		    (*match)(&rt->rt_dst, dst))
			return (rt);
	}
	return (0);
}

void
rtadd(struct sockaddr *dst, struct sockaddr *gate, short metric,
    short ticks, int state)
{
	struct afhash h;
	register struct rt_entry *rt;
	struct rthash *rh;
	int af = dst->sa_family, flags;
	u_int hash;

	FIXLEN(dst);
	FIXLEN(gate);
	if (af >= AF_MAX)
		return;
	(*afswitch[af].af_hash)(dst, &h);
	flags = (*afswitch[af].af_ishost)(dst) ? RTF_HOST : 0;
	hash = h.afh_nethash;
	rh = &nethash[hash & ROUTEHASHMASK];
	rt = (struct rt_entry *)malloc(sizeof (*rt));
	if (rt == 0)
		return;
	rt->rt_hash = hash;
	rt->rt_dst = *dst;
	rt->rt_router = *gate;
	rt->rt_metric = metric;
	rt->rt_ticks = ticks;
	rt->rt_timer = 0;
	rt->rt_flags = RTF_UP | flags;
	rt->rt_state = state | RTS_CHANGED;
	rt->rt_ifp = if_ifwithnet(&rt->rt_router);
	rt->rt_clone = NULL;
	if (metric)
		rt->rt_flags |= RTF_GATEWAY;
	insque(rt, rh);
	TRACE_ACTION("ADD", rt);
	/*
	 * If the ioctl fails because the gateway is unreachable
	 * from this host, discard the entry.  This should only
	 * occur because of an incorrect entry in /etc/gateways.
	 */
	if (install && rtioctl(ADD, &rt->rt_rt) < 0) {
		if (errno != EEXIST)
			perror("SIOCADDRT");
		if (errno == ENETUNREACH) {
			TRACE_ACTION("DELETE", rt);
			remque(rt);
			free((char *)rt);
		}
	}
}

void
rtadd_clone(struct rt_entry *ort, struct sockaddr *dst,
    struct sockaddr *gate, short metric, short ticks, int state)
{
	struct afhash h;
	register struct rt_entry *rt;
	int af = dst->sa_family, flags;
	u_int hash;

	FIXLEN(dst);
	FIXLEN(gate);
	if (af >= AF_MAX)
		return;
	(*afswitch[af].af_hash)(dst, &h);
	flags = (*afswitch[af].af_ishost)(dst) ? RTF_HOST : 0;
	hash = h.afh_nethash;
	rt = (struct rt_entry *)malloc(sizeof (*rt));
	if (rt == 0)
		return;
	rt->rt_hash = hash;
	rt->rt_dst = *dst;
	rt->rt_router = *gate;
	rt->rt_metric = metric;
	rt->rt_ticks = ticks;
	rt->rt_timer = 0;
	rt->rt_flags = RTF_UP | flags;
	rt->rt_state = state | RTS_CHANGED;
	rt->rt_ifp = if_ifwithnet(&rt->rt_router);
	rt->rt_clone = NULL;
	rt->rt_forw = NULL;
	rt->rt_back = NULL;
	if (metric)
		rt->rt_flags |= RTF_GATEWAY;

	while(ort->rt_clone != NULL)
		ort = ort->rt_clone;
	ort->rt_clone = rt;
	TRACE_ACTION("ADD_CLONE", rt);
}

void
rtchange(struct rt_entry *rt, struct sockaddr *gate, short metric,
    short ticks)
{
	int doioctl = 0, metricchanged = 0;

	FIXLEN(gate);
	/*
 	 * Handling of clones.
 	 * When the route changed and it had clones, handle it special.
 	 * 1. If the new route is cheaper than the clone(s), free the clones.
	 * 2. If the new route is the same cost, it may be one of the clones,
	 *    search for it and free it.
 	 * 3. If the new route is more expensive than the clone(s), use the
 	 *    values of the clone(s).
 	 */
	if (rt->rt_clone) {
		if ((ticks < rt->rt_clone->rt_ticks) ||
		    ((ticks == rt->rt_clone->rt_ticks) &&
		     (metric < rt->rt_clone->rt_metric))) {
			/*
			 * Free all clones.
			 */
			struct rt_entry *trt, *nrt;

			trt = rt->rt_clone;
			rt->rt_clone = NULL;
			while(trt) {
				nrt = trt->rt_clone;
				free((char *)trt);
				trt = nrt;
			}
		} else if ((ticks == rt->rt_clone->rt_ticks) &&
		     (metric == rt->rt_clone->rt_metric)) {
			struct rt_entry *prt, *trt;

			prt = rt;
			trt = rt->rt_clone;

			while(trt) {
				if (equal(&trt->rt_router, gate)) {
					prt->rt_clone = trt->rt_clone;
					free(trt);
					trt = prt->rt_clone;
				} else {
					prt = trt;
					trt = trt->rt_clone;
				}
			}
		} else {
			/*
			 * Use the values of the first clone. 
			 * Delete the corresponding clone.
			 */
			struct rt_entry *trt;

			trt = rt->rt_clone;
			rt->rt_clone = rt->rt_clone->rt_clone;
			metric = trt->rt_metric;
			ticks = trt->rt_ticks;
			*gate = trt->rt_router;
			free((char *)trt);
		}
	}

	if (!equal(&rt->rt_router, gate))
		doioctl++;
	if ((metric != rt->rt_metric) || (ticks != rt->rt_ticks))
		metricchanged++;
	if (doioctl || metricchanged) {
		TRACE_ACTION("CHANGE FROM", rt);
		if (doioctl) {
			rt->rt_router = *gate;
		}
		rt->rt_metric = metric;
		rt->rt_ticks = ticks;
		if ((rt->rt_state & RTS_INTERFACE) && metric) {
			rt->rt_state &= ~RTS_INTERFACE;
			if(rt->rt_ifp) 
				syslog(LOG_ERR,
				"changing route from interface %s (timed out)",
				rt->rt_ifp->int_name);
			else
				syslog(LOG_ERR,
				"changing route from interface ??? (timed out)");
		}
		if (metric)
			rt->rt_flags |= RTF_GATEWAY;
		else
			rt->rt_flags &= ~RTF_GATEWAY;
		rt->rt_ifp = if_ifwithnet(&rt->rt_router);
		rt->rt_state |= RTS_CHANGED;
		TRACE_ACTION("CHANGE TO", rt);
	}
	if (doioctl && install) {
#ifndef RTM_ADD
		if (rtioctl(ADD, &rt->rt_rt) < 0)
		  syslog(LOG_ERR, "rtioctl ADD dst %s, gw %s: %m",
		   ipx_ntoa(&((struct sockaddr_ipx *)&rt->rt_dst)->sipx_addr),
		   ipx_ntoa(&((struct sockaddr_ipx *)&rt->rt_router)->sipx_addr));
		if (delete && rtioctl(DELETE, &oldroute) < 0)
			perror("rtioctl DELETE");
#else
		if (delete == 0) {
			if (rtioctl(ADD, &rt->rt_rt) >= 0)
				return;
		} else {
			if (rtioctl(CHANGE, &rt->rt_rt) >= 0)
				return;
		}
	        syslog(LOG_ERR, "rtioctl ADD dst %s, gw %s: %m",
		   ipxdp_ntoa(&((struct sockaddr_ipx *)&rt->rt_dst)->sipx_addr),
		   ipxdp_ntoa(&((struct sockaddr_ipx *)&rt->rt_router)->sipx_addr));
#endif
	}
}

void
rtdelete(struct rt_entry *rt)
{

	struct sockaddr *sa = &(rt->rt_router);
	FIXLEN(sa);
	sa = &(rt->rt_dst);
	FIXLEN(sa);
	if (rt->rt_clone) {
		/*
		 * If there is a clone we just do a rt_change to it.
		 */
		struct rt_entry *trt = rt->rt_clone;
		rtchange(rt, &trt->rt_router, trt->rt_metric, trt->rt_ticks);
		return;
	}
	if (rt->rt_state & RTS_INTERFACE) {
		if (rt->rt_ifp)
			syslog(LOG_ERR, 
				"deleting route to interface %s (timed out)",
				rt->rt_ifp->int_name);
		else
			syslog(LOG_ERR, 
				"deleting route to interface ??? (timed out)");
	}
	TRACE_ACTION("DELETE", rt);
	if (install && rtioctl(DELETE, &rt->rt_rt) < 0)
		perror("rtioctl DELETE");
	remque(rt);
	free((char *)rt);
}

void
rtinit(void)
{
	register struct rthash *rh;

	for (rh = nethash; rh < &nethash[ROUTEHASHSIZ]; rh++)
		rh->rt_forw = rh->rt_back = (struct rt_entry *)rh;
}
int seqno;

int
rtioctl(int action, struct rtuentry *ort)
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
		struct sockaddr w_dst;
		struct sockaddr w_gate;
		struct sockaddr_ipx w_netmask;
	} w;
#define rtm w.w_rtm

	bzero((char *)&w, sizeof(w));
	rtm.rtm_msglen = sizeof(w);
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = (action == ADD ? RTM_ADD :
				(action == DELETE ? RTM_DELETE : RTM_CHANGE));
	rtm.rtm_flags = ort->rtu_flags;
	rtm.rtm_seq = ++seqno;
	rtm.rtm_addrs = RTA_DST|RTA_GATEWAY;
	bcopy((char *)&ort->rtu_dst, (char *)&w.w_dst, sizeof(w.w_dst));
	bcopy((char *)&ort->rtu_router, (char *)&w.w_gate, sizeof(w.w_gate));
	w.w_gate.sa_family = w.w_dst.sa_family = AF_IPX;
	w.w_gate.sa_len = w.w_dst.sa_len = sizeof(w.w_dst);
	if (rtm.rtm_flags & RTF_HOST) {
		rtm.rtm_msglen -= sizeof(w.w_netmask);
	} else {
		rtm.rtm_addrs |= RTA_NETMASK;
		w.w_netmask = ipx_netmask;
		rtm.rtm_msglen -= sizeof(w.w_netmask) - ipx_netmask.sipx_len;
	}
	errno = 0;
	return write(r, (char *)&w, rtm.rtm_msglen);
#endif  /* RTM_ADD */
}
