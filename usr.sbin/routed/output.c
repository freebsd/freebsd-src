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

#if !defined(lint) && !defined(sgi) && !defined(__NetBSD__)
static char sccsid[] = "@(#)output.c	8.1 (Berkeley) 6/5/93";
#elif defined(__NetBSD__)
static char rcsid[] = "$NetBSD$";
#endif
#ident "$Revision: 1.1.3.3 $"

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
	struct ws_buf {			/* info for each buffer */
	    struct rip	*buf;
	    struct netinfo *n;
	    struct netinfo *base;
	    struct netinfo *lim;
	    enum output_type type;
	} v12, v2;
	char	metric;			/* adjust metrics by interface */
	int	npackets;
	int	gen_limit;
	u_int	state;
#define	    WS_ST_FLASH	    0x001	/* send only changed routes */
#define	    WS_ST_RIP2_SAFE 0x002	/* send RIPv2 safe for RIPv1 */
#define	    WS_ST_RIP2_ALL  0x004	/* send full featured RIPv2 */
#define	    WS_ST_AG	    0x008	/* ok to aggregate subnets */
#define	    WS_ST_SUPER_AG  0x010	/* ok to aggregate networks */
#define	    WS_ST_SUB_AG    0x020	/* aggregate subnets in odd case */
#define	    WS_ST_QUERY	    0x040	/* responding to a query */
#define	    WS_ST_TO_ON_NET 0x080	/* sending onto one of our nets */
#define	    WS_ST_DEFAULT   0x100	/* faking a default */
#define	    WS_ST_PM_RDISC  0x200	/* poor-man's router discovery */
} ws;

/* A buffer for what can be heard by both RIPv1 and RIPv2 listeners */
union pkt_buf ripv12_buf;

/* Another for only RIPv2 listeners */
union pkt_buf rip_v2_buf;



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
	int res;
	naddr tgt_mcast;
	int soc;
	int serrno;

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
			msg = "Send";
		} else {
			msg = "Send bcast";
		}
		flags = MSG_DONTROUTE;
		break;
	case OUT_MULTICAST:
		if (ifp->int_if_flags & IFF_POINTOPOINT) {
			msg = "Send pt-to-pt";
		} else if (ifp->int_state & IS_DUP) {
			trace_act("abort multicast output via %s"
				  " with duplicate address\n",
				  ifp->int_name);
			return 0;
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
				if (0 > setsockopt(rip_sock,
						   IPPROTO_IP, IP_MULTICAST_IF,
						   &tgt_mcast,
						   sizeof(tgt_mcast))) {
					serrno = errno;
					LOGERR("setsockopt(rip_sock,"
					       "IP_MULTICAST_IF)");
					errno = serrno;
					ifp = 0;
					return -1;
				}
				rip_sock_mcast = ifp;
			}
			sin.sin_addr.s_addr = htonl(INADDR_RIP_GROUP);
		}

	case NO_OUT_MULTICAST:
	case NO_OUT_RIPV2:
		break;
	}

	trace_rip(msg, "to", &sin, ifp, buf, size);

	res = sendto(soc, buf, size, flags,
		     (struct sockaddr *)&sin, sizeof(sin));
	if (res < 0
	    && (ifp == 0 || !(ifp->int_state & IS_BROKE))) {
		serrno = errno;
		msglog("%s sendto(%s%s%s.%d): %s", msg,
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
supply_write(struct ws_buf *wb)
{
	/* Output multicast only if legal.
	 * If we would multcast and it would be illegal, then discard the
	 * packet.
	 */
	switch (wb->type) {
	case NO_OUT_MULTICAST:
		trace_pkt("skip multicast to %s because impossible\n",
			  naddr_ntoa(ws.to.sin_addr.s_addr));
		break;
	case NO_OUT_RIPV2:
		break;
	default:
		if (output(wb->type, &ws.to, ws.ifp, wb->buf,
			   ((char *)wb->n - (char*)wb->buf)) < 0
		    && ws.ifp != 0)
			if_sick(ws.ifp);
		ws.npackets++;
		break;
	}

	bzero(wb->n = wb->base, sizeof(*wb->n)*NETS_LEN);
	if (wb->buf->rip_vers == RIPv2)
		set_auth(wb);
}


/* put an entry into the packet
 */
static void
supply_out(struct ag_info *ag)
{
	int i;
	naddr mask, v1_mask, s_mask, dst_h, ddst_h;
	struct ws_buf *wb;


	/* Skip this route if doing a flash update and it and the routes
	 * it aggregates have not changed recently.
	 */
	if (ag->ag_seqno < update_seqno
	    && (ws.state & WS_ST_FLASH))
		return;

	/* Skip this route if required by split-horizon
	 */
	if (ag->ag_state & AGS_SPLIT_HZ)
		return;

	dst_h = ag->ag_dst_h;
	mask = ag->ag_mask;
	v1_mask = ripv1_mask_host(htonl(dst_h),
				  (ws.state & WS_ST_TO_ON_NET) ? ws.ifp : 0);
	s_mask = std_mask(htonl(dst_h));
	i = 0;

	/* If we are sending RIPv2 packets that cannot (or must not) be
	 * heard by RIPv1 listeners, do not worry about sub- or supernets.
	 * Subnets (from other networks) can only be sent via multicast.
	 * A pair of subnet routes might have been promoted so that they
	 * are legal to send by RIPv1.
	 * If RIPv1 is off, use the multicast buffer, unless this is the
	 * fake default route and it is acting as a poor-man's router-
	 * discovery mechanism.
	 */
	if (((ws.state & WS_ST_RIP2_ALL)
	     && (dst_h != RIP_DEFAULT || !(ws.state & WS_ST_PM_RDISC)))
	    || ((ag->ag_state & AGS_RIPV2) && v1_mask != mask)) {
		/* use the RIPv2-only buffer */
		wb = &ws.v2;

	} else {
		/* use the RIPv1-or-RIPv2 buffer */
		wb = &ws.v12;

		/* Convert supernet route into corresponding set of network
		 * routes for RIPv1, but leave non-contiguous netmasks
		 * to ag_check().
		 */
		if (v1_mask > mask
		    && mask + (mask & -mask) == 0) {
			ddst_h = v1_mask & -v1_mask;
			i = (v1_mask & ~mask)/ddst_h;

			if (i > ws.gen_limit) {
				/* Punt if we would have to generate an
				 * unreasonable number of routes.
				 */
#ifdef DEBUG
				msglog("sending %s to %s as 1 instead"
				       " of %d routes",
				       addrname(htonl(dst_h),mask,1),
				       naddr_ntoa(ws.to.sin_addr.s_addr),
				       i+1);
#endif
				i = 0;

			} else {
				mask = v1_mask;
				ws.gen_limit -= i;
			}
		}
	}

	do {
		wb->n->n_family = RIP_AF_INET;
		wb->n->n_dst = htonl(dst_h);
		/* If the route is from router-discovery or we are
		 * shutting down, admit only a bad metric.
		 */
		wb->n->n_metric = ((stopint || ag->ag_metric < 1)
				   ? HOPCNT_INFINITY
				   : ag->ag_metric);
		HTONL(wb->n->n_metric);
		if (wb->buf->rip_vers == RIPv2) {
			if (ag->ag_nhop != 0
			    && (ws.state & WS_ST_RIP2_SAFE)
			    && ((ws.state & WS_ST_QUERY)
				|| (ag->ag_nhop != ws.ifp->int_addr
				    && on_net(ag->ag_nhop,
					      ws.ifp->int_net,
					      ws.ifp->int_mask))))
				wb->n->n_nhop = ag->ag_nhop;
			if ((ws.state & WS_ST_RIP2_ALL)
			    || mask != s_mask)
				wb->n->n_mask = htonl(mask);
			wb->n->n_tag = ag->ag_tag;
		}
		dst_h += ddst_h;

		if (++wb->n >= wb->lim)
			supply_write(wb);
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
	u_short ags = 0;
	char metric, pref;
	naddr dst, nhop;


	/* Do not advertise the loopback interface
	 * or external remote interfaces
	 */
	if (RT->rt_ifp != 0
	    && ((RT->rt_ifp->int_if_flags & IFF_LOOPBACK)
		|| (RT->rt_ifp->int_state & IS_EXTERNAL))
	    && !(RT->rt_state & RS_MHOME))
		return 0;

	/* If being quiet about our ability to forward, then
	 * do not say anything unless responding to a query.
	 */
	if (!supplier && !(ws.state & WS_ST_QUERY))
		return 0;

	dst = RT->rt_dst;

	/* do not collide with the fake default route */
	if (dst == RIP_DEFAULT
	    && (ws.state & WS_ST_DEFAULT))
		return 0;

	if (RT->rt_state & RS_NET_SYN) {
		if (RT->rt_state & RS_NET_INT) {
			/* Do not send manual synthetic network routes
			 * into the subnet.
			 */
			if (on_net(ws.to.sin_addr.s_addr,
				   ntohl(dst), RT->rt_mask))
				return 0;

		} else {
			/* Do not send automatic synthetic network routes
			 * if they are not needed becaus no RIPv1 listeners
			 * can hear them.
			 */
			if (ws.state & WS_ST_RIP2_ALL)
				return 0;

			/* Do not send automatic synthetic network routes to
			 * the real subnet.
			 */
			if (on_net(ws.to.sin_addr.s_addr,
				   ntohl(dst), RT->rt_mask))
				return 0;
		}
		nhop = 0;

	} else {
		/* Advertise the next hop if this is not a route for one
		 * of our interfaces and the next hop is on the same
		 * network as the target.
		 */
		if (!(RT->rt_state & RS_IF)
		    && RT->rt_gate != myaddr
		    && RT->rt_gate != loopaddr)
			nhop = RT->rt_gate;
		else
			nhop = 0;
	}

	/* Adjust the outgoing metric by the cost of the link.
	 */
	pref = metric = RT->rt_metric + ws.metric;
	if (pref < HOPCNT_INFINITY) {
		/* Keep track of the best metric with which the
		 * route has been advertised recently.
		 */
		if (RT->rt_poison_metric >= metric
		    || RT->rt_poison_time <= now_garbage) {
			RT->rt_poison_time = now.tv_sec;
			RT->rt_poison_metric = RT->rt_metric;
		}

	} else {
		/* Do not advertise stable routes that will be ignored,
		 * unless they are being held down and poisoned.  If the
		 * route recently was advertised with a metric that would
		 * have been less than infinity through this interface, we
		 * need to continue to advertise it in order to poison it.
		 */
		pref = RT->rt_poison_metric + ws.metric;
		if (pref >= HOPCNT_INFINITY)
			return 0;

		metric = HOPCNT_INFINITY;
	}

	if (RT->rt_state & RS_MHOME) {
		/* retain host route of multi-homed servers */
		;

	} else if (RT_ISHOST(RT)) {
		/* We should always aggregate the host routes
		 * for the local end of our point-to-point links.
		 * If we are suppressing host routes in general, then do so.
		 * Avoid advertising host routes onto their own network,
		 * where they should be handled by proxy-ARP.
		 */
		if ((RT->rt_state & RS_LOCAL)
		    || ridhosts
		    || (ws.state & WS_ST_SUPER_AG)
		    || on_net(dst, ws.to_net, ws.to_mask))
			ags |= AGS_SUPPRESS;

		if (ws.state & WS_ST_SUPER_AG)
			ags |= AGS_PROMOTE;

	} else if (ws.state & WS_ST_AG) {
		/* Aggregate network routes, if we are allowed.
		 */
		ags |= AGS_SUPPRESS;

		/* Generate supernets if allowed.
		 * If we can be heard by RIPv1 systems, we will
		 * later convert back to ordinary nets.
		 * This unifies dealing with received supernets.
		 */
		if ((RT->rt_state & RS_SUBNET)
		    || (ws.state & WS_ST_SUPER_AG))
			ags |= AGS_PROMOTE;

	}

	/* Do not send RIPv1 advertisements of subnets to other
	 * networks. If possible, multicast them by RIPv2.
	 */
	if ((RT->rt_state & RS_SUBNET)
	    && !(ws.state & WS_ST_RIP2_ALL)
	    && !on_net(dst, ws.to_std_net, ws.to_std_mask)) {
		ags |= AGS_RIPV2 | AGS_PROMOTE;
		if (ws.state & WS_ST_SUB_AG)
			ags |= AGS_SUPPRESS;
	}

	/* Do not send a route back to where it came from, except in
	 * response to a query.  This is "split-horizon".  That means not
	 * advertising back to the same network	and so via the same interface.
	 *
	 * We want to suppress routes that might have been fragmented
	 * from this route by a RIPv1 router and sent back to us, and so we
	 * cannot forget this route here.  Let the split-horizon route
	 * aggregate (suppress) the fragmented routes and then itself be
	 * forgotten.
	 *
	 * Include the routes for both ends of point-to-point interfaces
	 * since the other side presumably knows them as well as we do.
	 */
	if (RT->rt_ifp == ws.ifp && ws.ifp != 0
	    && !(ws.state & WS_ST_QUERY)
	    && (ws.state & WS_ST_TO_ON_NET)
	    && (!(RT->rt_state & RS_IF)
		|| ws.ifp->int_if_flags & IFF_POINTOPOINT)) {
		ags |= AGS_SPLIT_HZ;
		ags &= ~(AGS_PROMOTE | AGS_SUPPRESS);
	}

	ag_check(dst, RT->rt_mask, 0, nhop, metric, pref,
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


	ws.state = 0;
	ws.gen_limit = 1024;

	ws.to = *dst;
	ws.to_std_mask = std_mask(ws.to.sin_addr.s_addr);
	ws.to_std_net = ntohl(ws.to.sin_addr.s_addr) & ws.to_std_mask;

	if (ifp != 0) {
		ws.to_mask = ifp->int_mask;
		ws.to_net = ifp->int_net;
		if (on_net(ws.to.sin_addr.s_addr, ws.to_net, ws.to_mask))
			ws.state |= WS_ST_TO_ON_NET;

	} else {
		ws.to_mask = ripv1_mask_net(ws.to.sin_addr.s_addr, 0);
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
		ws.metric = 1;
	} else {
		/* Adjust the advertised metric by the outgoing interface
		 * metric.
		 */
		ws.metric = ifp->int_metric+1;
	}

	if (init) {
		init = 0;

		bzero(&ripv12_buf, sizeof(ripv12_buf));
		ripv12_buf.rip.rip_cmd = RIPCMD_RESPONSE;
		ws.v12.buf = &ripv12_buf.rip;
		ws.v12.base = &ws.v12.buf->rip_nets[0];
		ws.v12.lim = ws.v12.base + NETS_LEN;

		bzero(&rip_v2_buf, sizeof(rip_v2_buf));
		rip_v2_buf.rip.rip_cmd = RIPCMD_RESPONSE;
		rip_v2_buf.rip.rip_vers = RIPv2;
		ws.v2.buf = &rip_v2_buf.rip;
		ws.v2.base = &ws.v2.buf->rip_nets[0];
		ws.v2.lim = ws.v2.base + NETS_LEN;
	}
	ripv12_buf.rip.rip_vers = vers;

	ws.v12.n = ws.v12.base;
	set_auth(&ws.v12);
	ws.v2.n = ws.v2.base;
	set_auth(&ws.v2);

	switch (type) {
	case OUT_BROADCAST:
		ws.v2.type = ((ws.ifp != 0
			       && (ws.ifp->int_if_flags & IFF_MULTICAST))
			      ? OUT_MULTICAST
			      : NO_OUT_MULTICAST);
		ws.v12.type = OUT_BROADCAST;
		break;
	case OUT_MULTICAST:
		ws.v2.type = ((ws.ifp != 0
			       && (ws.ifp->int_if_flags & IFF_MULTICAST))
			      ? OUT_MULTICAST
			      : NO_OUT_MULTICAST);
		ws.v12.type = OUT_BROADCAST;
		break;
	case OUT_UNICAST:
	case OUT_QUERY:
		ws.v2.type = (vers == RIPv2) ? type : NO_OUT_RIPV2;
		ws.v12.type = type;
		break;
	default:
		ws.v2.type = type;
		ws.v12.type = type;
		break;
	}

	if (vers == RIPv2) {
		/* if asked to send RIPv2, send at least that which can
		 * be safely heard by RIPv1 listeners.
		 */
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

	} else if (ws.ifp == 0 || !(ws.ifp->int_state & IS_NO_AG)) {
		ws.state |= WS_ST_SUB_AG;
	}

	if (supplier) {
		/*  Fake a default route if asked, and if there is not
		 * a better, real default route.
		 */
		if (ifp->int_d_metric != 0
		    && (0 == (rt = rtget(RIP_DEFAULT, 0))
			|| rt->rt_metric+ws.metric >= ifp->int_d_metric)) {
			ws.state |= WS_ST_DEFAULT;
			ag_check(0, 0, 0, 0,
				 ifp->int_d_metric,ifp->int_d_metric,
				 0, 0, 0, supply_out);
		}
		if ((ws.state & WS_ST_RIP2_ALL)
		    && (ifp->int_state & IS_PM_RDISC)) {
			ws.state |= WS_ST_PM_RDISC;
			ripv12_buf.rip.rip_vers = RIPv1;
		}
	}

	(void)rn_walktree(rhead, walk_supply, 0);
	ag_flush(0,0,supply_out);

	/* Flush the packet buffers, provided they are not empty and
	 * do not contain only the password.
	 */
	if (ws.v12.n != ws.v12.base
	    && (ws.v12.n > ws.v12.base+1
		|| ws.v12.n->n_family != RIP_AF_AUTH))
		supply_write(&ws.v12);
	if (ws.v2.n != ws.v2.base
	    && (ws.v2.n > ws.v2.base+1
		|| ws.v2.n->n_family != RIP_AF_AUTH))
		supply_write(&ws.v2);

	/* If we sent nothing and this is an answer to a query, send
	 * an empty buffer.
	 */
	if (ws.npackets == 0
	    && (ws.state & WS_ST_QUERY))
		supply_write(&ws.v12);
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

	trace_act("send %s and inhibit dynamic updates for %.3f sec\n",
		  flash ? "dynamic update" : "all routes",
		  rtime.tv_sec + ((float)rtime.tv_usec)/1000000.0);

	for (ifp = ifnet; ifp != 0; ifp = ifp->int_next) {
		/* skip interfaces not doing RIP, those already queried,
		 * and aliases.  Do try broken interfaces to see
		 * if they have healed.
		 */
		if (0 != (ifp->int_state & (IS_PASSIVE | IS_ALIAS)))
			continue;

		/* skip turned off interfaces */
		if (!iff_alive(ifp->int_if_flags))
			continue;

		/* default to RIPv1 output */
		if (ifp->int_state & IS_NO_RIPV1_OUT) {
			/* Say nothing if this interface is turned off */
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
			if (vers == RIPv2
			    && (ifp->int_state & IS_NO_RIPV1_OUT)) {
				type = OUT_MULTICAST;
			} else {
				type = OUT_BROADCAST;
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
					    | IS_PASSIVE | IS_ALIAS)))
			continue;

		/* skip turned off interfaces */
		if (!iff_alive(ifp->int_if_flags))
			continue;

		/* default to RIPv1 output */
		if (ifp->int_state & IS_NO_RIPV2_OUT) {
			/* Say nothing if this interface is turned off */
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
			if (buf.rip_vers == RIPv2
			    && (ifp->int_state & IS_NO_RIPV1_OUT)) {
				type = OUT_MULTICAST;
			} else {
				type = OUT_BROADCAST;
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
			if_sick(ifp);
	}
}
