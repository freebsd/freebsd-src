/*	$FreeBSD$	*/
/*	$KAME: mld6.c,v 1.27 2001/04/04 05:17:30 itojun Exp $	*/

/*-
 * Copyright (C) 1998 WIDE Project.
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
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1988 Stephen Deering.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Stephen Deering of Stanford University.
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
 *	@(#)igmp.c	8.1 (Berkeley) 7/19/93
 */

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/syslog.h>
#include <sys/malloc.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/mld6_var.h>

#include <net/net_osdep.h>

/*
 * Protocol constants
 */

/* denotes that the MLD max response delay field specifies time in milliseconds */
#define MLD_TIMER_SCALE	1000
/*
 * time between repetitions of a node's initial report of interest in a
 * multicast address(in seconds)
 */
#define MLD_UNSOLICITED_REPORT_INTERVAL	10

static struct ip6_pktopts ip6_opts;
static int mld6_timers_are_running;
/* XXX: These are necessary for KAME's link-local hack */
static struct in6_addr mld6_all_nodes_linklocal = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
static struct in6_addr mld6_all_routers_linklocal = IN6ADDR_LINKLOCAL_ALLROUTERS_INIT;

static void mld6_sendpkt(struct in6_multi *, int, const struct in6_addr *);

void
mld6_init()
{
	static u_int8_t hbh_buf[8];
	struct ip6_hbh *hbh = (struct ip6_hbh *)hbh_buf;
	u_int16_t rtalert_code = htons((u_int16_t)IP6OPT_RTALERT_MLD);

	mld6_timers_are_running = 0;

	/* ip6h_nxt will be fill in later */
	hbh->ip6h_len = 0;	/* (8 >> 3) - 1 */

	/* XXX: grotty hard coding... */
	hbh_buf[2] = IP6OPT_PADN;	/* 2 byte padding */
	hbh_buf[3] = 0;
	hbh_buf[4] = IP6OPT_ROUTER_ALERT;
	hbh_buf[5] = IP6OPT_RTALERT_LEN - 2;
	bcopy((caddr_t)&rtalert_code, &hbh_buf[6], sizeof(u_int16_t));

	ip6_initpktopts(&ip6_opts);
	ip6_opts.ip6po_hbh = hbh;
}

void
mld6_start_listening(in6m)
	struct in6_multi *in6m;
{
	int s = splnet();

	/*
	 * RFC2710 page 10:
	 * The node never sends a Report or Done for the link-scope all-nodes
	 * address.
	 * MLD messages are never sent for multicast addresses whose scope is 0
	 * (reserved) or 1 (node-local).
	 */
	mld6_all_nodes_linklocal.s6_addr16[1] =
		htons(in6m->in6m_ifp->if_index); /* XXX */
	if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &mld6_all_nodes_linklocal) ||
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) < IPV6_ADDR_SCOPE_LINKLOCAL) {
		in6m->in6m_timer = 0;
		in6m->in6m_state = MLD_OTHERLISTENER;
	} else {
		mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
		in6m->in6m_timer =
			MLD_RANDOM_DELAY(MLD_UNSOLICITED_REPORT_INTERVAL *
					 PR_FASTHZ);
		in6m->in6m_state = MLD_IREPORTEDLAST;
		mld6_timers_are_running = 1;
	}
	splx(s);
}

void
mld6_stop_listening(in6m)
	struct in6_multi *in6m;
{
	mld6_all_nodes_linklocal.s6_addr16[1] =
		htons(in6m->in6m_ifp->if_index); /* XXX */
	mld6_all_routers_linklocal.s6_addr16[1] =
		htons(in6m->in6m_ifp->if_index); /* XXX: necessary when mrouting */

	if (in6m->in6m_state == MLD_IREPORTEDLAST &&
	    (!IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &mld6_all_nodes_linklocal)) &&
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) > IPV6_ADDR_SCOPE_INTFACELOCAL)
		mld6_sendpkt(in6m, MLD_LISTENER_DONE,
			     &mld6_all_routers_linklocal);
}

void
mld6_input(m, off)
	struct mbuf *m;
	int off;
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct mld_hdr *mldh;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct in6_multi *in6m;
	struct in6_ifaddr *ia;
	struct ifmultiaddr *ifma;
	int timer;		/* timer value in the MLD query header */

#ifndef PULLDOWN_TEST
	IP6_EXTHDR_CHECK(m, off, sizeof(*mldh),);
	mldh = (struct mld_hdr *)(mtod(m, caddr_t) + off);
#else
	IP6_EXTHDR_GET(mldh, struct mld_hdr *, m, off, sizeof(*mldh));
	if (mldh == NULL) {
		icmp6stat.icp6s_tooshort++;
		return;
	}
#endif

	/* source address validation */
	ip6 = mtod(m, struct ip6_hdr *); /* in case mpullup */
	if (!IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_src)) {
		log(LOG_ERR,
		    "mld6_input: src %s is not link-local (grp=%s)\n",
		    ip6_sprintf(&ip6->ip6_src),
		    ip6_sprintf(&mldh->mld_addr));
		/*
		 * spec (RFC2710) does not explicitly
		 * specify to discard the packet from a non link-local
		 * source address. But we believe it's expected to do so.
		 * XXX: do we have to allow :: as source?
		 */
		m_freem(m);
		return;
	}

	/*
	 * In the MLD6 specification, there are 3 states and a flag.
	 *
	 * In Non-Listener state, we simply don't have a membership record.
	 * In Delaying Listener state, our timer is running (in6m->in6m_timer)
	 * In Idle Listener state, our timer is not running (in6m->in6m_timer==0)
	 *
	 * The flag is in6m->in6m_state, it is set to MLD_OTHERLISTENER if
	 * we have heard a report from another member, or MLD_IREPORTEDLAST
	 * if we sent the last report.
	 */
	switch(mldh->mld_type) {
	case MLD_LISTENER_QUERY:
		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN6_IS_ADDR_UNSPECIFIED(&mldh->mld_addr) &&
		    !IN6_IS_ADDR_MULTICAST(&mldh->mld_addr))
			break;	/* print error or log stat? */
		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] =
				htons(ifp->if_index); /* XXX */

		/*
		 * - Start the timers in all of our membership records
		 *   that the query applies to for the interface on
		 *   which the query arrived excl. those that belong
		 *   to the "all-nodes" group (ff02::1).
		 * - Restart any timer that is already running but has
		 *   A value longer than the requested timeout.
		 * - Use the value specified in the query message as
		 *   the maximum timeout.
		 */
		IFP_TO_IA6(ifp, ia);
		if (ia == NULL)
			break;

		/*
		 * XXX: System timer resolution is too low to handle Max
		 * Response Delay, so set 1 to the internal timer even if
		 * the calculated value equals to zero when Max Response
		 * Delay is positive.
		 */
		timer = ntohs(mldh->mld_maxdelay) * PR_FASTHZ / MLD_TIMER_SCALE;
		if (timer == 0 && mldh->mld_maxdelay)
			timer = 1;
		mld6_all_nodes_linklocal.s6_addr16[1] =
			htons(ifp->if_index); /* XXX */

		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_INET6)
				continue;
			in6m = (struct in6_multi *)ifma->ifma_protospec;

			if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr,
					&mld6_all_nodes_linklocal) ||
			    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) <
			    IPV6_ADDR_SCOPE_LINKLOCAL)
				continue;

			if (IN6_IS_ADDR_UNSPECIFIED(&mldh->mld_addr) ||
			    IN6_ARE_ADDR_EQUAL(&mldh->mld_addr,
						&in6m->in6m_addr))
			{
				if (timer == 0) {
					/* send a report immediately */
					mld6_sendpkt(in6m, MLD_LISTENER_REPORT,
						NULL);
					in6m->in6m_timer = 0; /* reset timer */
					in6m->in6m_state = MLD_IREPORTEDLAST;
				}
				else if (in6m->in6m_timer == 0 || /*idle state*/
					in6m->in6m_timer > timer) {
					in6m->in6m_timer =
						MLD_RANDOM_DELAY(timer);
					mld6_timers_are_running = 1;
				}
			}
		}
		IF_ADDR_UNLOCK(ifp);

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] = 0; /* XXX */
		break;

	case MLD_LISTENER_REPORT:
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 * Note that it is impossible to check IFF_LOOPBACK flag of
		 * ifp for this purpose, since ip6_mloopback pass the physical
		 * interface to looutput.
		 */
		if (m->m_flags & M_LOOP) /* XXX: grotty flag, but efficient */
			break;

		if (!IN6_IS_ADDR_MULTICAST(&mldh->mld_addr))
			break;

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] =
				htons(ifp->if_index); /* XXX */
		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN6_LOOKUP_MULTI(mldh->mld_addr, ifp, in6m);
		if (in6m) {
			in6m->in6m_timer = 0; /* transit to idle state */
			in6m->in6m_state = MLD_OTHERLISTENER; /* clear flag */
		}

		if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
			mldh->mld_addr.s6_addr16[1] = 0; /* XXX */
		break;
	default:		/* this is impossible */
		log(LOG_ERR, "mld6_input: illegal type(%d)", mldh->mld_type);
		break;
	}

	m_freem(m);
}

void
mld6_fasttimeo()
{
	struct in6_multi *in6m;
	struct in6_multistep step;
	int s;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */
	if (!mld6_timers_are_running)
		return;

	s = splnet();

	mld6_timers_are_running = 0;
	IN6_FIRST_MULTI(step, in6m);
	while (in6m != NULL) {
		if (in6m->in6m_timer == 0) {
			/* do nothing */
		} else if (--in6m->in6m_timer == 0) {
			mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
			in6m->in6m_state = MLD_IREPORTEDLAST;
		} else {
			mld6_timers_are_running = 1;
		}
		IN6_NEXT_MULTI(step, in6m);
	}

	splx(s);
}

static void
mld6_sendpkt(in6m, type, dst)
	struct in6_multi *in6m;
	int type;
	const struct in6_addr *dst;
{
	struct mbuf *mh, *md;
	struct mld_hdr *mldh;
	struct ip6_hdr *ip6;
	struct ip6_moptions im6o;
	struct in6_ifaddr *ia;
	struct ifnet *ifp = in6m->in6m_ifp;
	struct ifnet *outif = NULL;

	/*
	 * At first, find a link local address on the outgoing interface
	 * to use as the source address of the MLD packet.
	 */
	if ((ia = in6ifa_ifpforlinklocal(ifp, IN6_IFF_NOTREADY|IN6_IFF_ANYCAST))
	    == NULL)
		return;

	/*
	 * Allocate mbufs to store ip6 header and MLD header.
	 * We allocate 2 mbufs and make chain in advance because
	 * it is more convenient when inserting the hop-by-hop option later.
	 */
	MGETHDR(mh, M_DONTWAIT, MT_HEADER);
	if (mh == NULL)
		return;
	MGET(md, M_DONTWAIT, MT_DATA);
	if (md == NULL) {
		m_free(mh);
		return;
	}
	mh->m_next = md;

	mh->m_pkthdr.rcvif = NULL;
	mh->m_pkthdr.len = sizeof(struct ip6_hdr) + sizeof(struct mld_hdr);
	mh->m_len = sizeof(struct ip6_hdr);
	MH_ALIGN(mh, sizeof(struct ip6_hdr));

	/* fill in the ip6 header */
	ip6 = mtod(mh, struct ip6_hdr *);
	ip6->ip6_flow = 0;
	ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
	ip6->ip6_vfc |= IPV6_VERSION;
	/* ip6_plen will be set later */
	ip6->ip6_nxt = IPPROTO_ICMPV6;
	/* ip6_hlim will be set by im6o.im6o_multicast_hlim */
	ip6->ip6_src = ia->ia_addr.sin6_addr;
	ip6->ip6_dst = dst ? *dst : in6m->in6m_addr;

	/* fill in the MLD header */
	md->m_len = sizeof(struct mld_hdr);
	mldh = mtod(md, struct mld_hdr *);
	mldh->mld_type = type;
	mldh->mld_code = 0;
	mldh->mld_cksum = 0;
	/* XXX: we assume the function will not be called for query messages */
	mldh->mld_maxdelay = 0;
	mldh->mld_reserved = 0;
	mldh->mld_addr = in6m->in6m_addr;
	if (IN6_IS_ADDR_MC_LINKLOCAL(&mldh->mld_addr))
		mldh->mld_addr.s6_addr16[1] = 0; /* XXX */
	mldh->mld_cksum = in6_cksum(mh, IPPROTO_ICMPV6, sizeof(struct ip6_hdr),
				    sizeof(struct mld_hdr));

	/* construct multicast option */
	bzero(&im6o, sizeof(im6o));
	im6o.im6o_multicast_ifp = ifp;
	im6o.im6o_multicast_hlim = 1;

	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing daemon can hear it.
	 */
	im6o.im6o_multicast_loop = (ip6_mrouter != NULL);

	/* increment output statictics */
	icmp6stat.icp6s_outhist[type]++;

	ip6_output(mh, &ip6_opts, NULL, 0, &im6o, &outif, NULL);
	if (outif) {
		icmp6_ifstat_inc(outif, ifs6_out_msg);
		switch (type) {
		case MLD_LISTENER_QUERY:
			icmp6_ifstat_inc(outif, ifs6_out_mldquery);
			break;
		case MLD_LISTENER_REPORT:
			icmp6_ifstat_inc(outif, ifs6_out_mldreport);
			break;
		case MLD_LISTENER_DONE:
			icmp6_ifstat_inc(outif, ifs6_out_mlddone);
			break;
		}
	}
}

/*
 * Add an address to the list of IP6 multicast addresses for a given interface.
 * Add source addresses to the list also, if upstream router is MLDv2 capable
 * and the number of source is not 0.
 */
struct	in6_multi *
in6_addmulti(maddr6, ifp, errorp)
	struct in6_addr *maddr6;
	struct ifnet *ifp;
	int *errorp;
{
	struct in6_multi *in6m;
	struct ifmultiaddr *ifma;
	struct sockaddr_in6 sa6;
	int	s = splnet();

	*errorp = 0;

	/*
	 * Call generic routine to add membership or increment
	 * refcount.  It wants addresses in the form of a sockaddr,
	 * so we build one here (being careful to zero the unused bytes).
	 */
	bzero(&sa6, sizeof(sa6));
	sa6.sin6_family = AF_INET6;
	sa6.sin6_len = sizeof(struct sockaddr_in6);
	sa6.sin6_addr = *maddr6;
	*errorp = if_addmulti(ifp, (struct sockaddr *)&sa6, &ifma);
	if (*errorp) {
		splx(s);
		return 0;
	}

	/*
	 * If ifma->ifma_protospec is null, then if_addmulti() created
	 * a new record.  Otherwise, we are done.
	 */
	if (ifma->ifma_protospec != 0) {
		splx(s);
		return ifma->ifma_protospec;
	}

	/* XXX - if_addmulti uses M_WAITOK.  Can this really be called
	   at interrupt time?  If so, need to fix if_addmulti. XXX */
	in6m = (struct in6_multi *)malloc(sizeof(*in6m), M_IPMADDR, M_NOWAIT);
	if (in6m == NULL) {
		splx(s);
		return (NULL);
	}

	bzero(in6m, sizeof *in6m);
	in6m->in6m_addr = *maddr6;
	in6m->in6m_ifp = ifp;
	in6m->in6m_refcount = 1;
	in6m->in6m_ifma = ifma;
	ifma->ifma_protospec = in6m;
	LIST_INSERT_HEAD(&in6_multihead, in6m, in6m_entry);

	/*
	 * Let MLD6 know that we have joined a new IPv6 multicast
	 * group.
	 */
	mld6_start_listening(in6m);
	splx(s);
	return (in6m);
}

/*
 * Delete a multicast address record.
 */
void
in6_delmulti(in6m)
	struct in6_multi *in6m;
{
	struct ifmultiaddr *ifma = in6m->in6m_ifma;
	int	s = splnet();

	if (ifma->ifma_refcount == 1) {
		/*
		 * No remaining claims to this record; let MLD6 know
		 * that we are leaving the multicast group.
		 */
		mld6_stop_listening(in6m);
		ifma->ifma_protospec = 0;
		LIST_REMOVE(in6m, in6m_entry);
		free(in6m, M_IPMADDR);
	}
	/* XXX - should be separate API for when we have an ifma? */
	if_delmulti(ifma->ifma_ifp, ifma->ifma_addr);
	splx(s);
}
