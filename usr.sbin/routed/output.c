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
static char sccsid[] = "@(#)output.c	8.1 (Berkeley) 6/5/93";
#endif /* not lint */

#ident "$Revision: 1.1.3.1 $"

#include "defs.h"


int update_seqno;


/* walk the tree of routes with this for output
 */
struct {
	struct sockaddr_in to;
	naddr	to_mask;
	naddr	to_net;
	naddr	to_std_mask;
	naddr	to_std_net;
	struct interface *ifp;		/* usually output interface */
	struct ws_buf {			/* for each buffer */
	    struct rip	*buf;
	    struct netinfo *n;
	    struct netinfo *base;
	    struct netinfo *lim;
	    enum output_type type;
	} v2, mcast;
	char	metric;			/* adjust metrics by interface */
	int	npackets;
	int	state;
#define	    WS_ST_FLASH	    0x01	/* send only changed routes */
#define	    WS_ST_RIP2_SAFE 0x02	/* send RIPv2 safe for RIPv1 */
#define	    WS_ST_RIP2_ALL  0x04	/* full featured RIPv2 */
#define	    WS_ST_AG	    0x08	/* ok to aggregate subnets */
#define	    WS_ST_SUPER_AG  0x10	/* ok to aggregate networks */
#define	    WS_ST_QUERY	    0x20	/* responding to a query */
#define	    WS_ST_TO_ON_NET 0x40	/* sending onto one of our nets */
#define	    WS_ST_DEFAULT   0x80	/* faking a default */
} ws;

/* A buffer for what can be heard by both RIPv1 and RIPv2 listeners */
union pkt_buf ripv2_buf;

/* Another for only RIPv2 listeners */
union pkt_buf rip_mcast_buf;



/* Send the contents of the global buffer via the non-multicast socket
 */
int					/* <0 on failure */
output(enum output_type type,
       struct sockaddr_in *dst,		/* send to here */
       struct interface *ifp,
       struct rip *buf,
       int size)			/* this many bytes */
{
	struct sockaddr_in sin;
	int flags;
	char *msg;
	int res, serrno;
	naddr tgt_mcast;
	int soc;

	sin = *dst;
	if (sin.sin_port == 0)
		sin.sin_port = htons(RIP_PORT);
#ifdef _HAVE_SIN_LEN
	if (sin.sin_len == 0)
		sin.sin_len = sizeof(sin);
#endif

	soc = rip_sock;
	flags = 0;
	
	switch (type) {
	case OUT_QUERY:
		msg = "Answer Query";
		if (soc < 0)
			soc = ifp->int_rip_sock;
		break;
	case OUT_UNICAST:
		msg = "Send";
		if (soc < 0)
			soc = ifp->int_rip_sock;
		flags = MSG_DONTROUTE;
		break;
	case OUT_BROADCAST:
		if (ifp->int_if_flags & IFF_POINTOPOINT) {
			msg = "Send pt-to-pt";
		} else {
			msg = "Send";
		}
		flags = MSG_DONTROUTE;
		break;
	case OUT_MULTICAST:
		if (ifp->int_if_flags & IFF_POINTOPOINT) {
			msg = "Send pt-to-pt";
		} else {
			msg = "Send mcast";
			if (rip_sock_mcast != ifp) {
#ifdef MCAST_PPP_BUG
				/* Do not specifiy the primary interface
				 * explicitly if we have the multicast
				 * point-to-point kernel bug, since the
				 * kernel will do the wrong thing if the
				 * local address of a point-to-point link
				 * is the same as the address of an ordinary
				 * interface.
				 */
				if (ifp->int_addr == myaddr) {
					tgt_mcast = 0;
				} else
#endif
				tgt_mcast = ifp->int_addr;
				if (setsockopt(rip_sock,
					       IPPROTO_IP, IP_MULTICAST_IF,
					       &tgt_mcast, sizeof(tgt_mcast)))
					BADERR(1,"setsockopt(rip_sock,"
					       "IP_MULTICAST_IF)");
				rip_sock_mcast = ifp;
			}
			sin.sin_addr.s_addr = htonl(INADDR_RIP_GROUP);
		}
	}

	if (TRACEPACKETS)
		trace_rip(msg, "to", &sin, ifp, buf, size);

	res = sendto(soc, buf, size, flags,
		     (struct sockaddr *)&sin, sizeof(sin));
	if (res < 0) {
		serrno = errno;
		msglog("sendto(%s%s%s.%d): %s",
		       ifp != 0 ? ifp->int_name : "",
		       ifp != 0 ? ", " : "",
		       inet_ntoa(sin.sin_addr),
		       ntohs(sin.sin_port),
		       strerror(errno));
		errno = serrno;
	}

	return res;
}


/* install authentication if appropriate
 */
static void
set_auth(struct ws_buf *w)
{
	if (ws.ifp != 0
	    && ws.ifp->int_passwd[0] != '\0'
	    && (ws.state & WS_ST_RIP2_SAFE)) {
		w->n->n_family = RIP_AF_AUTH;
		((struct netauth*)w->n)->a_type = RIP_AUTH_PW;
		bcopy(ws.ifp->int_passwd, ((struct netauth*)w->n)->au.au_pw,
		      sizeof(((struct netauth*)w->n)->au.au_pw));
		w->n++;
	}
}


/* Send the buffer
 */
static void
supply_write(struct ws_buf *w)
{
	/* Output multicast only if legal.
	 * If we would multcast and it would be illegal, then discard the
	 * packet.
	 */
	if (w != &ws.mcast
	    || ((ws.state & WS_ST_RIP2_SAFE)
		&& (ws.ifp == 0
		    || (ws.ifp->int_if_flags & IFF_MULTICAST)))) {
		if (output(w->type, &ws.to, ws.ifp, w->buf,
			   ((char *)w->n - (char*)w->buf)) < 0
		    && ws.ifp != 0)
			ifbad(ws.ifp, 0);
		ws.npackets++;
	}

	bzero(w->n = w->base, sizeof(*w->n)*NETS_LEN);

	if (w->buf->rip_vers == RIPv2)
		set_auth(w);
}


/* put an entry into the packet
 */
static void
supply_out(struct ag_info *ag)
{
	int i;
	naddr mask, v1_mask, s_mask, dst_h, ddst_h;
	struct ws_buf *w;


	/* Skip this route if doing a flash update and it and the routes
	 * it aggregates have not changed recently.
	 */
	if (ag->ag_seqno <= update_seqno
	    && (ws.state & WS_ST_FLASH))
		return;

	dst_h = ag->ag_dst_h;
	mask = ag->ag_mask;
	v1_mask = ripv1_mask_host(htonl(dst_h),
				  (ws.state & WS_ST_TO_ON_NET) ? ws.ifp : 0,
				  0);
	s_mask = std_mask(htonl(dst_h));
	i = 0;

	/* If we are sending RIPv2 packets that cannot (or must not) be
	 * heard by RIPv1 listeners, do not worry about sub- or supernets.
	 * Subnets (from other networks) can only be sent via multicast.
	 */
	if ((ws.state & WS_ST_RIP2_ALL)
	    || ((ag->ag_state & AGS_RIPV2)
		&& v1_mask != mask)) {
		w = &ws.mcast;		/* use the multicast-only buffer */

	} else {
		w = &ws.v2;

		/* Convert supernet route into corresponding set of network
		 * routes for RIPv1, but leave non-contiguous netmasks
		 * to ag_check().
		 */
		if (v1_mask > mask
		    && mask + (mask & -mask) == 0) {
			ddst_h = v1_mask & -v1_mask;
			i = (v1_mask & ~mask)/ddst_h;

			if (i >= 1024) {
				/* Punt if we would have to generate an
				 * unreasonable number of routes.
				 */
#ifdef DEBUG
				msglog("sending %s to %s as-is instead"
				       " of as %d routes",
				       addrname(htonl(dst_h),mask,0),
				       naddr_ntoa(ws.to.sin_addr.s_addr), i);
#endif
				i = 0;

			} else {
				mask = v1_mask;
			}
		}
	}

	do {
		w->n->n_family = RIP_AF_INET;
		w->n->n_dst = htonl(dst_h);
		w->n->n_metric = stopint ? HOPCNT_INFINITY : ag->ag_metric;
		HTONL(w->n->n_metric);
		if (w->buf->rip_vers == RIPv2) {
			w->n->n_nhop = ag->ag_gate;
			if ((ws.state & WS_ST_RIP2_ALL)
			    || mask != s_mask)
				w->n->n_mask = htonl(mask);
			w->n->n_tag = ag->ag_tag;
		}
		dst_h += ddst_h;

		if (++w->n >= w->lim)
			supply_write(w);
	} while (i-- != 0);
}


/* supply one route from the table
 */
/* ARGSUSED */
static int
walk_supply(struct radix_node *rn,
	    struct walkarg *w)
{
#define RT ((struct rt_entry *)rn)
	u_short ags;
	char metric, pref;
	naddr dst, gate;

	/* Do not advertise the loopback interface
	 * or external remote interfaces
	 */
	if (RT->rt_ifp != 0
	    && ((RT->rt_ifp->int_if_flags & IFF_LOOPBACK)
		|| (RT->rt_ifp->int_state & IS_EXTERNAL)))
		return 0;

	/* Do not send a route back to where it came from, except in
	 * response to a query.  This is "split-horizon".
	 *
	 * That means not advertising back to the same network
	 *	and so via the same interface.
	 */
	if (RT->rt_ifp == ws.ifp && ws.ifp != 0
	    && !(ws.state & WS_ST_QUERY)
	    && (ws.state & WS_ST_TO_ON_NET)
	    && !(RT->rt_state & RS_IF))
		return 0;

	dst = RT->rt_dst;

	/* If being quiet about our ability to forward, then
	 * do not say anything except our own host number,
	 * unless responding to a query.
	 */
	if (!supplier
	    && (!mhome || myaddr != dst)
	    && !(ws.state & WS_ST_QUERY))
		return 0;

	ags = 0;

	/* do not override the fake default route */
	if (dst == RIP_DEFAULT
	    && (ws.state & WS_ST_DEFAULT))
		return 0;

	if (RT_ISHOST(RT)) {
		/* We should always aggregate the host routes
		 * for the local end of our point-to-point links.
		 * If we are suppressing host routes, then do so.
		 */
		if ((RT->rt_state & RS_LOCAL)
		    || ridhosts)
			ags |= AGS_SUPPRESS;

	} else if (ws.state & WS_ST_AG) {
		/* Aggregate network routes, if we are allowed.
		 */
		ags |= AGS_SUPPRESS;

		/* Generate supernets if allowed.
		 * If we can be heard by RIPv1 systems, we will
		 * later convert back to ordinary nets.  This unifies
		 * dealing with received supernets.
		 */
		if ((RT->rt_state & RS_SUBNET)
		    || (ws.state & WS_ST_SUPER_AG))
			ags |= AGS_PROMOTE;
	}

	/* Never aggregate our own interfaces,
	 * or the host route for multi-homed servers.
	 */
	if (0 != (RT->rt_state & (RS_IF | RS_MHOME)))
		ags &= ~(AGS_SUPPRESS | AGS_PROMOTE);


	if (RT->rt_state & RS_SUBNET) {
		/* Do not send authority routes into the subnet,
		 * or when RIP is off.
		 */
		if ((RT->rt_state & RS_NET_INT)
		    && (on_net(dst, ws.to_net, ws.to_mask)
			|| rip_sock < 0))
			return 0;

		/* Do not send RIPv1 advertisements of subnets to
		 * other networks.
		 *
		 * If possible, multicast them by RIPv2.
		 */
		if (!(ws.state & WS_ST_RIP2_ALL)
		    && !on_net(dst, ws.to_std_net, ws.to_std_mask))
			ags |= AGS_RIPV2;

	} else if (RT->rt_state & RS_NET_SUB) {
		/* do not send synthetic network routes if no RIPv1
		 * listeners might hear.
		 */
		if (ws.state & WS_ST_RIP2_ALL)
			return 0;

		/* Do not send synthetic network routes on the real subnet */
		if (on_net(dst, ws.to_std_net, ws.to_std_mask))
			return 0;
	}

	/* forget synthetic routes when RIP is off */
	if (rip_sock < 0 && 0 != (RT->rt_state & RS_NET_S))
		return 0;


	/* Adjust outgoing metric by the cost of the link.
	 * Interface routes have already been adjusted.
	 */
	pref = metric = RT->rt_metric + ws.metric;
	if (metric >= HOPCNT_INFINITY) {
		metric = HOPCNT_INFINITY;
		pref = ((RT->rt_hold_down > now.tv_sec)
			? RT->rt_hold_metric
			: metric);
	}

	/* Advertise the next hop if this is not a route for one
	 * of our interfaces and the next hop is on the same
	 * network as the target.
	 */
	if ((ws.state & WS_ST_RIP2_SAFE)
	    && !(RT->rt_state & RS_IF)
	    && ((ws.state & WS_ST_QUERY)
		|| (on_net(RT->rt_gate, ws.ifp->int_net, ws.ifp->int_mask)
		    && RT->rt_gate != ws.ifp->int_addr))) {
		gate = RT->rt_gate;
	} else {
		gate = 0;
	}

	ag_check(dst, RT->rt_mask, gate, metric, pref,
		 RT->rt_seqno, RT->rt_tag, ags, supply_out);
	return 0;
#undef RT
}


/* Supply dst with the contents of the routing tables.
 * If this won't fit in one packet, chop it up into several.
 */
void
supply(struct sockaddr_in *dst,
       struct interface *ifp,		/* output interface */
       enum output_type type,
       int flash,			/* 1=flash update */
       int vers)			/* RIP version */
{
	static int init = 1;
	struct rt_entry *rt;
	int metric;


	ws.state = 0;

	ws.to = *dst;
	ws.to_std_mask = std_mask(ws.to.sin_addr.s_addr);
	ws.to_std_net = ntohl(ws.to.sin_addr.s_addr) & ws.to_std_mask;

	if (ifp != 0) {
		ws.to_mask = ifp->int_mask;
		ws.to_net = ifp->int_net;
		if (on_net(ws.to.sin_addr.s_addr, ws.to_net, ws.to_mask))
			ws.state |= WS_ST_TO_ON_NET;

	} else {
		ws.to_mask = ripv1_mask_net(ws.to.sin_addr.s_addr, 0, 0);
		ws.to_net = ntohl(ws.to.sin_addr.s_addr) & ws.to_mask;
		rt = rtfind(dst->sin_addr.s_addr);
		if (rt)
			ifp = rt->rt_ifp;
	}

	ws.npackets = 0;
	if (flash)
		ws.state |= WS_ST_FLASH;
	if (type == OUT_QUERY)
		ws.state |= WS_ST_QUERY;

	if ((ws.ifp = ifp) == 0) {
		ws.metric = 0;
	} else {
		/* Adjust the advertised metric by the outgoing interface
		 * metric, but reduced by 1 to avoid counting this hop
		 * twice.
		 */
		ws.metric = ifp->int_metric;
		if (ws.metric > 0)
			ws.metric--;
	}

	if (init) {
		init = 0;

		bzero(&ripv2_buf, sizeof(ripv2_buf));
		ripv2_buf.rip.rip_cmd = RIPCMD_RESPONSE;
		ws.v2.buf = &ripv2_buf.rip;
		ws.v2.base = &ws.v2.buf->rip_nets[0];
		ws.v2.lim = ws.v2.base + NETS_LEN;

		bzero(&rip_mcast_buf, sizeof(rip_mcast_buf));
		rip_mcast_buf.rip.rip_cmd = RIPCMD_RESPONSE;
		rip_mcast_buf.rip.rip_vers = RIPv2;
		ws.mcast.buf = &rip_mcast_buf.rip;
		ws.mcast.base = &ws.mcast.buf->rip_nets[0];
		ws.mcast.lim = ws.mcast.base + NETS_LEN;
	}
	ripv2_buf.rip.rip_vers = vers;

	ws.v2.type = type;
	ws.v2.n = ws.v2.base;
	set_auth(&ws.v2);

	ws.mcast.type = (type == OUT_BROADCAST) ? OUT_MULTICAST : type;
	ws.mcast.n = ws.mcast.base;
	set_auth(&ws.mcast);

	if (vers == RIPv2) {
		ws.state |= WS_ST_RIP2_SAFE;
		/* full RIPv2 only if cannot be heard by RIPv1 listeners */
		if (type != OUT_BROADCAST)
			ws.state |= WS_ST_RIP2_ALL;
		if (!(ws.state & WS_ST_TO_ON_NET)) {
			ws.state |= (WS_ST_AG | WS_ST_SUPER_AG);
		} else if (ws.ifp == 0 || !(ws.ifp->int_state & IS_NO_AG)) {
			ws.state |= WS_ST_AG;
			if (type != OUT_BROADCAST
			    && (ws.ifp == 0
				|| !(ws.ifp->int_state & IS_NO_SUPER_AG)))
				ws.state |= WS_ST_SUPER_AG;
		}
	}

	/* send the routes
	 */
	if ((metric = ifp->int_d_metric) != 0) {
		/* Fake a default route if asked */
		ws.state |= WS_ST_DEFAULT;

		/* Use the metric of a real default, if there is one.
		 */
		rt = rtget(RIP_DEFAULT, 0);
		if (rt != 0
		    && rt->rt_metric+ws.metric < metric)
			metric = rt->rt_metric+ws.metric;

		if (metric < HOPCNT_INFINITY)
			ag_check(0, 0, 0, metric,metric, 0, 0, 0, supply_out);
	}
	(void)rn_walktree(rhead, walk_supply, 0);
	ag_flush(0,0,supply_out);

	/* Flush the packet buffers */
	if (ws.v2.n != ws.v2.base
	    && (ws.v2.n > ws.v2.base+1
		|| ws.v2.n->n_family != RIP_AF_AUTH))
		supply_write(&ws.v2);
	if (ws.mcast.n != ws.mcast.base
	    && (ws.mcast.n > ws.mcast.base+1
		|| ws.mcast.n->n_family != RIP_AF_AUTH))
		supply_write(&ws.mcast);

	/* If we sent nothing and this is an answer to a query, send
	 * an empty buffer.
	 */
	if (ws.npackets == 0
	    && (ws.state & WS_ST_QUERY))
		supply_write(&ws.v2);
}


/* send all of the routing table or just do a flash update
 */
void
rip_bcast(int flash)
{
#ifdef _HAVE_SIN_LEN
	static struct sockaddr_in dst = {sizeof(dst), AF_INET};
#else
	static struct sockaddr_in dst = {AF_INET};
#endif
	struct interface *ifp;
	enum output_type type;
	int vers;
	struct timeval rtime;


	need_flash = 0;
	intvl_random(&rtime, MIN_WAITTIME, MAX_WAITTIME);
	no_flash = rtime;
	timevaladd(&no_flash, &now);

	if (rip_sock < 0)
		return;

	trace_msg("send %s and inhibit dynamic updates for %.3f sec\n",
		  flash ? "dynamic update" : "all routes",
		  rtime.tv_sec + ((float)rtime.tv_usec)/1000000.0);

	for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
		/* skip interfaces not doing RIP, those already queried,
		 * and aliases.  Do try broken interfaces to see
		 * if they have healed.
		 */
		if (0 != (ifp->int_state & (IS_PASSIVE
					    | IS_ALIAS)))
			continue;

		/* skip turned off interfaces */
		if (!iff_alive(ifp->int_if_flags))
			continue;

		/* Prefer RIPv1 announcements unless RIPv2 is on and
		 * RIPv2 is off.
		 */
		if (ifp->int_state & IS_NO_RIPV1_OUT) {
			if (ifp->int_state & IS_NO_RIPV2_OUT)
				continue;
			vers = RIPv2;
		} else {
			vers = RIPv1;
		}

		if (ifp->int_if_flags & IFF_BROADCAST) {
			/* ordinary, hardware interface */
			dst.sin_addr.s_addr = ifp->int_brdaddr;
			/* if RIPv1 is not turned off, then broadcast so
			 * that RIPv1 listeners can hear.
			 */
			if (!(ifp->int_state & IS_NO_RIPV1_OUT)) {
				type = OUT_BROADCAST;
			} else {
				type = OUT_MULTICAST;
			}

		} else if (ifp->int_if_flags & IFF_POINTOPOINT) {
			/* point-to-point hardware interface */
			dst.sin_addr.s_addr = ifp->int_dstaddr;
			type = OUT_UNICAST;

		} else {
			/* remote interface */
			dst.sin_addr.s_addr = ifp->int_addr;
			type = OUT_UNICAST;
		}

		supply(&dst, ifp, type, flash, vers);
	}

	update_seqno++;			/* all routes are up to date */
}


/* Ask for routes
 * Do it only once to an interface, and not even after the interface
 * was broken and recovered.
 */
void
rip_query(void)
{
#ifdef _HAVE_SIN_LEN
	static struct sockaddr_in dst = {sizeof(dst), AF_INET};
#else
	static struct sockaddr_in dst = {AF_INET};
#endif
	struct interface *ifp;
	struct rip buf;
	enum output_type type;


	if (rip_sock < 0)
		return;

	bzero(&buf, sizeof(buf));

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		/* skip interfaces not doing RIP, those already queried,
		 * and aliases.  Do try broken interfaces to see
		 * if they have healed.
		 */
		if (0 != (ifp->int_state & (IS_RIP_QUERIED
					    | IS_PASSIVE
					    | IS_ALIAS)))
			continue;

		/* skip turned off interfaces */
		if (!iff_alive(ifp->int_if_flags))
			continue;

		/* prefer RIPv2 queries */
		if (ifp->int_state & IS_NO_RIPV2_OUT) {
			if (ifp->int_state & IS_NO_RIPV1_OUT)
				continue;
			buf.rip_vers = RIPv1;
		} else {
			buf.rip_vers = RIPv2;
		}

		buf.rip_cmd = RIPCMD_REQUEST;
		buf.rip_nets[0].n_family = RIP_AF_UNSPEC;
		buf.rip_nets[0].n_metric = htonl(HOPCNT_INFINITY);

		if (ifp->int_if_flags & IFF_BROADCAST) {
			/* ordinary, hardware interface */
			dst.sin_addr.s_addr = ifp->int_brdaddr;
			/* if RIPv1 is not turned off, then broadcast so
			 * that RIPv1 listeners can hear.
			 */
			if (!(ifp->int_state & IS_NO_RIPV1_OUT)) {
				type = OUT_BROADCAST;
			} else {
				type = OUT_MULTICAST;
			}

		} else if (ifp->int_if_flags & IFF_POINTOPOINT) {
			/* point-to-point hardware interface */
			dst.sin_addr.s_addr = ifp->int_dstaddr;
			type = OUT_UNICAST;

		} else {
			/* remote interface */
			dst.sin_addr.s_addr = ifp->int_addr;
			type = OUT_UNICAST;
		}

		ifp->int_state |= IS_RIP_QUERIED;
		if (output(type, &dst, ifp, &buf, sizeof(buf)) < 0)
			ifbad(ifp,0);
	}
}
