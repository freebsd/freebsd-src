/*
 * Copyright (c) 1995
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
static const char rcsid[] =
	"$Id$";
#endif /* not lint */

#include "defs.h"
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>

/* router advertisement ICMP packet */
struct icmp_ad {
	u_char	icmp_type;		/* type of message */
	u_char	icmp_code;		/* type sub code */
	u_short	icmp_cksum;		/* ones complement cksum of struct */
	u_char	icmp_ad_num;		/* # of following router addresses */
	u_char	icmp_ad_asize;		/* 2--words in each advertisement */
	u_short	icmp_ad_life;		/* seconds of validity */
	struct icmp_ad_info {
	    n_long  icmp_ad_addr;
	    n_long  icmp_ad_pref;
	} icmp_ad_info[1];
};

/* router solicitation ICMP packet */
struct icmp_so {
	u_char	icmp_type;		/* type of message */
	u_char	icmp_code;		/* type sub code */
	u_short	icmp_cksum;		/* ones complement cksum of struct */
	n_long	icmp_so_rsvd;
};

union ad_u {
	struct icmp icmp;
	struct icmp_ad ad;
	struct icmp_so so;
};


int	rdisc_sock = -1;		/* router-discovery raw socket */
struct interface *rdisc_sock_mcast;	/* current multicast interface */

struct timeval rdisc_timer;
int rdisc_ok;				/* using solicited route */


#define MAX_ADS 5
struct dr {				/* accumulated advertisements */
    struct interface *dr_ifp;
    naddr   dr_gate;			/* gateway */
    time_t  dr_ts;			/* when received */
    time_t  dr_life;			/* lifetime */
    n_long  dr_recv_pref;		/* received but biased preference */
    n_long  dr_pref;			/* preference adjusted by metric */
} *cur_drp, drs[MAX_ADS];

/* adjust preference by interface metric without driving it to infinity */
#define PREF(p, ifp) ((p) < (ifp)->int_metric ? ((p) != 0 ? 1 : 0) \
		      : (p) - ((ifp)->int_metric-1))

static void rdisc_sort(void);


/* dump an ICMP Router Discovery Advertisement Message
 */
static void
trace_rdisc(char	*act,
	    naddr	from,
	    naddr	to,
	    struct interface *ifp,
	    union ad_u	*p,
	    u_int	len)
{
	int i;
	n_long *wp, *lim;


	if (ftrace == 0)
		return;

	lastlog();

	if (p->icmp.icmp_type == ICMP_ROUTERADVERT) {
		(void)fprintf(ftrace, "%s Router Ad"
			      " from %s to %s via %s life=%d\n",
			      act, naddr_ntoa(from), naddr_ntoa(to),
			      ifp ? ifp->int_name : "?",
			      p->ad.icmp_ad_life);
		if (!TRACECONTENTS)
			return;

		wp = &p->ad.icmp_ad_info[0].icmp_ad_addr;
		lim = &wp[(len - sizeof(p->ad)) / sizeof(*wp)];
		for (i = 0; i < p->ad.icmp_ad_num && wp <= lim; i++) {
			(void)fprintf(ftrace, "\t%s preference=%#x",
				      naddr_ntoa(wp[0]), ntohl(wp[1]));
			wp += p->ad.icmp_ad_asize;
		}
		(void)fputc('\n',ftrace);

	} else {
		trace_msg("%s Router Solic. from %s to %s via %s"
			  " value=%#x\n",
			  act, naddr_ntoa(from), naddr_ntoa(to),
			  ifp ? ifp->int_name : "?",
			  ntohl(p->so.icmp_so_rsvd));
	}
}


/* Pick multicast group for router-discovery socket
 */
void
set_rdisc_mg(struct interface *ifp,
	     int on) {			/* 0=turn it off */
	struct ip_mreq m;

	if (rdisc_sock == -1
	    || !(ifp->int_if_flags & IFF_MULTICAST)
	    || (ifp->int_state & IS_ALIAS)) {
		ifp->int_state &= ~(IS_ALL_HOSTS | IS_ALL_ROUTERS);
		return;
	}

#ifdef MCAST_PPP_BUG
	if (ifp->int_if_flags & IFF_POINTOPOINT)
		return;
#endif
	bzero(&m, sizeof(m));
	m.imr_interface.s_addr = ((ifp->int_if_flags & IFF_POINTOPOINT)
				  ? ifp->int_dstaddr
				  : ifp->int_addr);
	if (supplier
	    || (ifp->int_state & IS_NO_ADV_IN)
	    || !on) {
		/* stop listening to advertisements */
		if (ifp->int_state & IS_ALL_HOSTS) {
			m.imr_multiaddr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
			if (setsockopt(rdisc_sock, IPPROTO_IP,
				       IP_DROP_MEMBERSHIP,
				       &m, sizeof(m)) < 0)
				DBGERR(1,"IP_DROP_MEMBERSHIP ALLHOSTS");
			ifp->int_state &= ~IS_ALL_HOSTS;
		}

	} else if (!(ifp->int_state & IS_ALL_HOSTS)) {
		/* start listening to advertisements */
		m.imr_multiaddr.s_addr = htonl(INADDR_ALLHOSTS_GROUP);
		if (setsockopt(rdisc_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			       &m, sizeof(m)) < 0)
			DBGERR(1,"IP_ADD_MEMBERSHIP ALLHOSTS");
		ifp->int_state |= IS_ALL_HOSTS;
	}

	if (!supplier
	    || (ifp->int_state & IS_NO_ADV_OUT)
	    || !on) {
		/* stop listening to solicitations */
		if (ifp->int_state & IS_ALL_ROUTERS) {
			m.imr_multiaddr.s_addr=htonl(INADDR_ALLROUTERS_GROUP);
			if (setsockopt(rdisc_sock, IPPROTO_IP,
				       IP_DROP_MEMBERSHIP,
				       &m, sizeof(m)) < 0)
				DBGERR(1,"IP_DROP_MEMBERSHIP ALLROUTERS");
			ifp->int_state &= ~IS_ALL_ROUTERS;
		}

	} else if (!(ifp->int_state & IS_ALL_ROUTERS)) {
		/* start hearing solicitations */
		m.imr_multiaddr.s_addr=htonl(INADDR_ALLROUTERS_GROUP);
		if (setsockopt(rdisc_sock, IPPROTO_IP, IP_ADD_MEMBERSHIP,
			       &m, sizeof(m)) < 0)
			DBGERR(1,"IP_ADD_MEMBERSHIP ALLROUTERS");
		ifp->int_state |= IS_ALL_ROUTERS;
	}
}


/* start supplying routes
 */
void
set_supplier(void)
{
	struct interface *ifp;
	struct dr *drp;

	if (supplier_set)
		return;

	trace_msg("start suppying routes\n");

	/* Forget discovered routes.
	 */
	for (drp = drs; drp < &drs[MAX_ADS]; drp++) {
		drp->dr_recv_pref = 0;
		drp->dr_life = 0;
	}
	rdisc_age(0);

	supplier_set = 1;
	supplier = 1;

	/* Do not start advertising until we have heard some RIP routes */
	LIM_SEC(rdisc_timer, now.tv_sec+MIN_WAITTIME);

	/* Switch router discovery multicast groups from soliciting
	 * to advertising.
	 */
	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (ifp->int_state & IS_BROKE)
			continue;
		ifp->int_rdisc_cnt = 0;
		ifp->int_rdisc_timer.tv_usec = rdisc_timer.tv_usec;
		ifp->int_rdisc_timer.tv_sec = now.tv_sec+MIN_WAITTIME;
		set_rdisc_mg(ifp, 1);
	}
}


/* age discovered routes and find the best one
 */
void
rdisc_age(naddr bad_gate)
{
	time_t sec;
	struct dr *drp;


	if (supplier) {
		/* If only adverising, then do only that. */
		rdisc_adv();
		return;
	}

	/* If we are being told about a bad router,
	 * then age the discovered default route, and if there is
	 * no alternative, solicite a replacement.
	 */
	if (bad_gate != 0) {
		/* Look for the bad discovered default route.
		 * Age it and note its interface.
		 */
		for (drp = drs; drp < &drs[MAX_ADS]; drp++) {
			if (drp->dr_ts == 0)
				continue;

			/* When we find the bad router, then age the route
			 * to at most SUPPLY_INTERVAL.
			 * This is contrary to RFC 1256, but defends against
			 * black holes.
			 */
			if (drp->dr_gate == bad_gate) {
				sec = (now.tv_sec - drp->dr_life
				       + SUPPLY_INTERVAL);
				if (drp->dr_ts > sec) {
					trace_msg("age 0.0.0.0 --> %s"
						  " via %s\n",
						  naddr_ntoa(drp->dr_gate),
						  drp->dr_ifp->int_name);
					drp->dr_ts = sec;
				}
				break;
			}
		}
	}

	/* delete old redirected routes to keep the kernel table small
	 */
	sec = (cur_drp == 0) ? MaxMaxAdvertiseInterval : cur_drp->dr_life;
	del_redirects(bad_gate, now.tv_sec-sec);

	rdisc_sol();

	rdisc_sort();
}


/* zap all routes discovered via an interface that has gone bad
 */
void
ifbad_rdisc(struct interface *ifp)
{
	struct dr *drp;

	for (drp = drs; drp < &drs[MAX_ADS]; drp++) {
		if (drp->dr_ifp != ifp)
			continue;
		drp->dr_recv_pref = 0;
		drp->dr_life = 0;
	}

	rdisc_sort();
}


/* mark an interface ok for router discovering.
 */
void
ifok_rdisc(struct interface *ifp)
{
	set_rdisc_mg(ifp, 1);

	ifp->int_rdisc_cnt = 0;
	ifp->int_rdisc_timer.tv_sec = now.tv_sec + (supplier
						    ? MIN_WAITTIME
						    : MAX_SOLICITATION_DELAY);
	if (timercmp(&rdisc_timer, &ifp->int_rdisc_timer, >))
		rdisc_timer = ifp->int_rdisc_timer;
}


/* get rid of a dead discovered router
 */
static void
del_rdisc(struct dr *drp)
{
	struct interface *ifp;
	int i;


	del_redirects(drp->dr_gate, 0);
	drp->dr_ts = 0;
	drp->dr_life = 0;


	/* Count the other discovered routes on the interface.
	 */
	i = 0;
	ifp = drp->dr_ifp;
	for (drp = drs; drp < &drs[MAX_ADS]; drp++) {
		if (drp->dr_ts != 0
		    && drp->dr_ifp == ifp)
			i++;
	}

	/* If that was the last good discovered router on the interface,
	 * then solicit a new one.
	 * This is contrary to RFC 1256, but defends against black holes.
	 */
	if (i == 0
	    && ifp->int_rdisc_cnt >= MAX_SOLICITATIONS) {
		trace_msg("re-solicit routers via %s\n", ifp->int_name);
		ifp->int_rdisc_cnt = 0;
		ifp->int_rdisc_timer.tv_sec = 0;
		rdisc_sol();
	}
}


/* Find the best discovered route,
 * and discard stale routers.
 */
static void
rdisc_sort(void)
{
	struct dr *drp, *new_drp;
	struct rt_entry *rt;
	struct interface *ifp;
	time_t sec;


	/* find the best discovered route
	 */
	new_drp = 0;
	for (drp = drs; drp < &drs[MAX_ADS]; drp++) {
		if (drp->dr_ts == 0)
			continue;
		ifp = drp->dr_ifp;

		/* Get rid of expired discovered routes.
		 * Routes received over PPP links do not die until
		 * the link has been active long enough to be certain
		 * we should have heard from the router.
		 */
		if (drp->dr_ts + drp->dr_life <= now.tv_sec) {
			if (drp->dr_recv_pref == 0
			    || !ppp_noage
			    || !(ifp->int_if_flags & IFF_POINTOPOINT)
			    || !(ifp->int_state & IS_QUIET)
			    || (ifp->int_quiet_time
				+ (sec = MIN(MaxMaxAdvertiseInterval,
					     drp->dr_life)) <= now.tv_sec)) {
				del_rdisc(drp);
				continue;
			}

			/* If the PPP link is quiet, keep checking
			 * in case the link becomes active.
			 * After the link is active, the timer on the
			 * discovered route might force its deletion.
			 */
			sec += now.tv_sec+1;
		} else {
			sec = drp->dr_ts+drp->dr_life+1;
		}
		LIM_SEC(rdisc_timer, sec);

		/* Update preference with possibly changed interface
		 * metric.
		 */
		drp->dr_pref = PREF(drp->dr_recv_pref, ifp);

		/* Prefer the current route to prevent thrashing.
		 * Prefer shorter lifetimes to speed the detection of
		 * bad routers.
		 */
		if (new_drp == 0
		    || new_drp->dr_pref < drp->dr_pref
		    || (new_drp->dr_pref == drp->dr_pref
			&& (drp == cur_drp
			    || (new_drp != cur_drp
				&& new_drp->dr_life > drp->dr_life))))
			new_drp = drp;
	}

	/* switch to a better default route
	 */
	if (new_drp != cur_drp) {
		rt = rtget(RIP_DEFAULT, 0);

		/* Stop using discovered routes if they are all bad
		 */
		if (new_drp == 0) {
			trace_msg("turn off Router Discovery\n");
			rdisc_ok = 0;

			if (rt != 0
			    && (rt->rt_state & RS_RDISC)) {
				rtchange(rt, rt->rt_state,
					 rt->rt_gate, rt->rt_router,
					 HOPCNT_INFINITY, 0, rt->rt_ifp,
					 now.tv_sec - GARBAGE_TIME, 0);
				rtswitch(rt, 0);
			}

			/* turn on RIP if permitted */
			rip_on(0);

		} else {
			if (cur_drp == 0) {
				trace_msg("turn on Router Discovery using"
					  " %s via %s\n",
					  naddr_ntoa(new_drp->dr_gate),
					  new_drp->dr_ifp->int_name);

				rdisc_ok = 1;
				rip_off();

			} else {
				trace_msg("switch Router Discovery from"
					  " %s via %s to %s via %s\n",
					  naddr_ntoa(cur_drp->dr_gate),
					  cur_drp->dr_ifp->int_name,
					  naddr_ntoa(new_drp->dr_gate),
					  new_drp->dr_ifp->int_name);
			}

			if (rt != 0) {
				rtchange(rt, rt->rt_state | RS_RDISC,
					 new_drp->dr_gate, new_drp->dr_gate,
					 0,0, new_drp->dr_ifp,
					 now.tv_sec, 0);
			} else {
				rtadd(RIP_DEFAULT, 0,
				      new_drp->dr_gate, new_drp->dr_gate,
				      0, 0, RS_RDISC, new_drp->dr_ifp);
			}
		}

		cur_drp = new_drp;
	}
}


/* handle a single address in an advertisement
 */
static void
parse_ad(naddr from,
	 naddr gate,
	 n_long pref,
	 int life,
	 struct interface *ifp)
{
	static naddr bad_gate;
	struct dr *drp, *new_drp;


	NTOHL(gate);
	if (gate == RIP_DEFAULT
	    || !check_dst(gate)) {
		if (bad_gate != from) {
			msglog("router %s advertising bad gateway %s",
			       naddr_ntoa(from),
			       naddr_ntoa(gate));
			bad_gate = from;
		}
		return;
	}

	/* ignore pointers to ourself and routes via unreachable networks
	 */
	if (ifwithaddr(gate, 1, 0) != 0) {
		if (TRACEPACKETS)
			trace_msg("discard our own packet\n");
		return;
	}
	if (!on_net(gate, ifp->int_net, ifp->int_mask)) {
		if (TRACEPACKETS)
			trace_msg("discard packet from unreachable net\n");
		return;
	}

	/* Convert preference to an unsigned value
	 * and bias it by the metric of the interface.
	 */
	pref = ntohl(pref) ^ MIN_PreferenceLevel;

	for (new_drp = drs, drp = drs; drp < &drs[MAX_ADS]; drp++) {
		if (drp->dr_ts == 0) {
			new_drp = drp;
			continue;
		}

		if (drp->dr_gate == gate) {
			/* Zap an entry we are being told is kaput */
			if (pref == 0 || life == 0) {
				drp->dr_recv_pref = 0;
				drp->dr_life = 0;
				return;
			}
			new_drp = drp;
			break;
		}

		/* look for least valueable entry */
		if (new_drp->dr_pref > drp->dr_pref)
			new_drp = drp;
	}

	/* ignore zap of an entry we do not know about. */
	if (pref == 0 || life == 0)
		return;

	new_drp->dr_ifp = ifp;
	new_drp->dr_gate = gate;
	new_drp->dr_ts = now.tv_sec;
	new_drp->dr_life = ntohl(life);
	new_drp->dr_recv_pref = pref;
	new_drp->dr_pref = PREF(pref,ifp);

	ifp->int_rdisc_cnt = MAX_SOLICITATIONS;
}


/* Compute the IP checksum
 *	This assumes the packet is less than 32K long.
 */
static u_short
in_cksum(u_short *p,
	 u_int len)
{
	u_int sum = 0;
	int nwords = len >> 1;

	while (nwords-- != 0)
		sum += *p++;

	if (len & 1)
		sum += *(u_char *)p;

	/* end-around-carry */
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return (~sum);
}


/* Send a router discovery advertisement or solicitation ICMP packet.
 */
static void
send_rdisc(union ad_u *p,
	   int p_size,
	   struct interface *ifp,
	   naddr dst,			/* 0 or unicast destination */
	   int	type)			/* 0=unicast, 1=bcast, 2=mcast */
{
	struct sockaddr_in sin;
	int flags;
	char *msg;
	naddr tgt_mcast;


	bzero(&sin, sizeof(sin));
	sin.sin_addr.s_addr = dst;
	flags = MSG_DONTROUTE;

	switch (type) {
	case 0:				/* unicast */
		msg = "Send";
		break;

	case 1:				/* broadcast */
		if (ifp->int_if_flags & IFF_POINTOPOINT) {
			msg = "Send pt-to-pt";
			sin.sin_addr.s_addr = ifp->int_dstaddr;
		} else {
			msg = "Broadcast";
			sin.sin_addr.s_addr = ifp->int_brdaddr;
		}
		break;

	case 2:				/* multicast */
		msg = "Multicast";
		if (rdisc_sock_mcast != ifp) {
			/* select the right interface. */
#ifdef MCAST_PPP_BUG
			/* Do not specifiy the primary interface explicitly
			 * if we have the multicast point-to-point kernel
			 * bug, since the kernel will do the wrong thing
			 * if the local address of a point-to-point link
			 * is the same as the address of an ordinary
			 * interface.
			 */
			if (ifp->int_addr == myaddr) {
				tgt_mcast = 0;
			} else
#endif
			tgt_mcast = ifp->int_addr;
			if (setsockopt(rdisc_sock,
				       IPPROTO_IP, IP_MULTICAST_IF,
				       &tgt_mcast, sizeof(tgt_mcast))) {
				DBGERR(1,"setsockopt(rdisc_sock,"
				       "IP_MULTICAST_IF)");
				return;
			}
			rdisc_sock_mcast = ifp;
		}
		flags = 0;
		break;
	}

	if (TRACEPACKETS)
		trace_rdisc(msg, ifp->int_addr, sin.sin_addr.s_addr, ifp,
			    p, p_size);

	if (0 > sendto(rdisc_sock, p, p_size, flags,
		       (struct sockaddr *)&sin, sizeof(sin))) {
		msglog("sendto(%s%s%s): %s",
		       ifp != 0 ? ifp->int_name : "",
		       ifp != 0 ? ", " : "",
		       inet_ntoa(sin.sin_addr),
		       strerror(errno));
		if (ifp != 0)
			ifbad(ifp, 0);
	}
}


/* Send an advertisement
 */
static void
send_adv(struct interface *ifp,
	 naddr	dst,			/* 0 or unicast destination */
	 int	type)			/* 0=unicast, 1=bcast, 2=mcast */
{
	union ad_u u;
	n_long pref;


	bzero(&u,sizeof(u.ad));

	u.ad.icmp_type = ICMP_ROUTERADVERT;
	u.ad.icmp_ad_num = 1;
	u.ad.icmp_ad_asize = sizeof(u.ad.icmp_ad_info[0])/4;

	u.ad.icmp_ad_life = stopint ? 0 : htonl(ifp->int_rdisc_int*3);

	u.ad.icmp_ad_life = stopint ? 0 : htonl(ifp->int_rdisc_int*3);
	pref = ifp->int_rdisc_pref ^ MIN_PreferenceLevel;
	pref = PREF(pref, ifp) ^ MIN_PreferenceLevel;
	u.ad.icmp_ad_info[0].icmp_ad_pref = htonl(pref);

	u.ad.icmp_ad_info[0].icmp_ad_addr = ifp->int_addr;

	u.ad.icmp_cksum = in_cksum((u_short*)&u.ad, sizeof(u.ad));

	send_rdisc(&u, sizeof(u.ad), ifp, dst, type);
}


/* Advertise for Router Discovery
 */
void
rdisc_adv(void)
{
	struct interface *ifp;


	rdisc_timer.tv_sec = now.tv_sec + NEVER;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (0 != (ifp->int_state & (IS_NO_ADV_OUT
					    | IS_PASSIVE
					    | IS_ALIAS
					    | IS_BROKE)))
			continue;

		if (!timercmp(&ifp->int_rdisc_timer, &now, >)
		    || stopint) {
			send_adv(ifp, INADDR_ALLHOSTS_GROUP,
				 (ifp->int_if_flags&IS_BCAST_RDISC) ? 1 : 2);
			ifp->int_rdisc_cnt++;

			intvl_random(&ifp->int_rdisc_timer,
				     (ifp->int_rdisc_int*3)/4,
				     ifp->int_rdisc_int);
			if (ifp->int_rdisc_cnt < MAX_INITIAL_ADVERTS
			    && (ifp->int_rdisc_timer.tv_sec
				> MAX_INITIAL_ADVERT_INTERVAL)) {
				ifp->int_rdisc_timer.tv_sec
				= MAX_INITIAL_ADVERT_INTERVAL;
			}
			timevaladd(&ifp->int_rdisc_timer, &now);
		}

		if (timercmp(&rdisc_timer, &ifp->int_rdisc_timer, >))
			rdisc_timer = ifp->int_rdisc_timer;
	}
}


/* Solicit for Router Discovery
 */
void
rdisc_sol(void)
{
	struct interface *ifp;
	union ad_u u;


	rdisc_timer.tv_sec = now.tv_sec + NEVER;

	for (ifp = ifnet; ifp; ifp = ifp->int_next) {
		if (0 != (ifp->int_state & (IS_NO_SOL_OUT
					    | IS_PASSIVE
					    | IS_ALIAS
					    | IS_BROKE))
		    || ifp->int_rdisc_cnt >= MAX_SOLICITATIONS)
			continue;

		if (!timercmp(&ifp->int_rdisc_timer, &now, >)) {
			bzero(&u,sizeof(u.so));
			u.so.icmp_type = ICMP_ROUTERSOLICIT;
			u.so.icmp_cksum = in_cksum((u_short*)&u.so,
						   sizeof(u.so));
			send_rdisc(&u, sizeof(u.so), ifp,
				   htonl(INADDR_ALLROUTERS_GROUP),
				   ((ifp->int_if_flags & IS_BCAST_RDISC)
				    ? 1 : 2));

			if (++ifp->int_rdisc_cnt >= MAX_SOLICITATIONS)
				continue;

			ifp->int_rdisc_timer.tv_sec = SOLICITATION_INTERVAL;
			ifp->int_rdisc_timer.tv_usec = 0;
			timevaladd(&ifp->int_rdisc_timer, &now);
		}

		if (timercmp(&rdisc_timer, &ifp->int_rdisc_timer, >))
			rdisc_timer = ifp->int_rdisc_timer;
	}
}


/* check the IP header of a possible Router Discovery ICMP packet */
static struct interface *		/* 0 if bad */
ck_icmp(char	*act,
	naddr	from,
	naddr	to,
	union ad_u *p,
	u_int	len)
{
	struct interface *ifp;
	char *type;


	/* If we could tell the interface on which a packet from address 0
	 * arrived, we could deal with such solicitations.
	 */

	ifp = ((from == 0) ? 0 : iflookup(from));

	if (p->icmp.icmp_type == ICMP_ROUTERADVERT) {
		type = "advertisement";
	} else if (p->icmp.icmp_type == ICMP_ROUTERSOLICIT) {
		type = "solicitation";
	} else {
		return 0;
	}

	if (p->icmp.icmp_code != 0) {
		if (TRACEPACKETS)
			msglog("unrecognized ICMP Router"
			       " %s code=%d from %s to %s\n",
			       type, p->icmp.icmp_code,
			       naddr_ntoa(from), naddr_ntoa(to));
		return 0;
	}

	if (TRACEPACKETS)
		trace_rdisc(act, from, to, ifp, p, len);

	if (ifp == 0 && TRACEPACKETS)
		msglog("unknown interface for router-discovery %s"
		       " from %s to %s",
		       type, naddr_ntoa(from), naddr_ntoa(to));

	return ifp;
}


/* read packets from the router discovery socket
 */
void
read_d(void)
{
	static naddr bad_asize, bad_len;
	struct sockaddr_in from;
	int n, fromlen, cc, hlen;
	union {
		struct ip ip;
		u_short s[512/2];
		u_char	b[512];
	} pkt;
	union ad_u *p;
	n_long *wp;
	struct interface *ifp;


	for (;;) {
		fromlen = sizeof(from);
		cc = recvfrom(rdisc_sock, &pkt, sizeof(pkt), 0,
			      (struct sockaddr*)&from,
			      &fromlen);
		if (cc <= 0) {
			if (cc < 0 && errno != EWOULDBLOCK)
				LOGERR("recvfrom(rdisc_sock)");
			break;
		}
		if (fromlen != sizeof(struct sockaddr_in))
			logbad(1,"impossible recvfrom(rdisc_sock) fromlen=%d",
			       fromlen);

		hlen = pkt.ip.ip_hl << 2;
		if (cc < hlen + ICMP_MINLEN)
			continue;
		p = (union ad_u *)&pkt.b[hlen];
		cc -= hlen;

		ifp = ck_icmp("Recv",
			      from.sin_addr.s_addr, pkt.ip.ip_dst.s_addr,
			      p, cc);
		if (ifp == 0)
			continue;
		if (ifwithaddr(from.sin_addr.s_addr, 0, 0)) {
			trace_msg("\tdiscard our own packet\n");
			continue;
		}

		switch (p->icmp.icmp_type) {
		case ICMP_ROUTERADVERT:
			if (p->ad.icmp_ad_asize*4
			    < sizeof(p->ad.icmp_ad_info[0])) {
				if (bad_asize != from.sin_addr.s_addr) {
					msglog("intolerable rdisc address"
					       " size=%d",
					       p->ad.icmp_ad_asize);
					bad_asize = from.sin_addr.s_addr;
				}
				continue;
			}
			if (p->ad.icmp_ad_num == 0) {
				if (TRACEPACKETS)
					trace_msg("\tempty?\n");
				continue;
			}
			if (cc != (sizeof(p->ad) - sizeof(p->ad.icmp_ad_info)
				   + (p->ad.icmp_ad_num
				      * sizeof(p->ad.icmp_ad_info[0])))) {
				if (bad_len != from.sin_addr.s_addr) {
					msglog("rdisc length %d does not"
					       " match ad_num %d",
					       cc, p->ad.icmp_ad_num);
					bad_len = from.sin_addr.s_addr;
				}
				continue;
			}
			if (supplier)
				continue;
			if (ifp->int_state & IS_NO_ADV_IN)
				continue;

			wp = &p->ad.icmp_ad_info[0].icmp_ad_addr;
			for (n = 0; n < p->ad.icmp_ad_num; n++) {
				parse_ad(from.sin_addr.s_addr,
					 wp[0], wp[1],
					 p->ad.icmp_ad_life,
					 ifp);
				wp += p->ad.icmp_ad_asize;
			}
			break;


		case ICMP_ROUTERSOLICIT:
			if (!supplier)
				continue;
			if (ifp->int_state & IS_NO_ADV_OUT)
				continue;

			/* XXX
			 * We should handle messages from address 0.
			 */

			/* Respond with a point-to-point advertisement */
			send_adv(ifp, from.sin_addr.s_addr, 0);
			break;
		}
	}

	rdisc_sort();
}
