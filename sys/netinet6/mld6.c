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
 *
 *	$KAME: mld6.c,v 1.27 2001/04/04 05:17:30 itojun Exp $
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/netinet6/mld6.c,v 1.31.2.4.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/syslog.h>
#include <sys/kernel.h>
#include <sys/callout.h>
#include <sys/malloc.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/mld6_var.h>

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

static void mld6_sendpkt(struct in6_multi *, int, const struct in6_addr *);
static void mld_starttimer(struct in6_multi *);
static void mld_stoptimer(struct in6_multi *);
static void mld_timeo(struct in6_multi *);
static u_long mld_timerresid(struct in6_multi *);

void
mld6_init(void)
{
	static u_int8_t hbh_buf[8];
	struct ip6_hbh *hbh = (struct ip6_hbh *)hbh_buf;
	u_int16_t rtalert_code = htons((u_int16_t)IP6OPT_RTALERT_MLD);

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

static void
mld_starttimer(struct in6_multi *in6m)
{
	struct timeval now;

	microtime(&now);
	in6m->in6m_timer_expire.tv_sec = now.tv_sec + in6m->in6m_timer / hz;
	in6m->in6m_timer_expire.tv_usec = now.tv_usec +
	    (in6m->in6m_timer % hz) * (1000000 / hz);
	if (in6m->in6m_timer_expire.tv_usec > 1000000) {
		in6m->in6m_timer_expire.tv_sec++;
		in6m->in6m_timer_expire.tv_usec -= 1000000;
	}

	/* start or restart the timer */
	callout_reset(in6m->in6m_timer_ch, in6m->in6m_timer,
	    (void (*)(void *))mld_timeo, in6m);
}

static void
mld_stoptimer(struct in6_multi *in6m)
{
	if (in6m->in6m_timer == IN6M_TIMER_UNDEF)
		return;

	callout_stop(in6m->in6m_timer_ch);
	in6m->in6m_timer = IN6M_TIMER_UNDEF;
}

static void
mld_timeo(struct in6_multi *in6m)
{
	int s = splnet();

	in6m->in6m_timer = IN6M_TIMER_UNDEF;

	callout_stop(in6m->in6m_timer_ch);

	switch (in6m->in6m_state) {
	case MLD_REPORTPENDING:
		mld6_start_listening(in6m);
		break;
	default:
		mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
		break;
	}

	splx(s);
}

static u_long
mld_timerresid(struct in6_multi *in6m)
{
	struct timeval now, diff;

	microtime(&now);

	if (now.tv_sec > in6m->in6m_timer_expire.tv_sec ||
	    (now.tv_sec == in6m->in6m_timer_expire.tv_sec &&
	    now.tv_usec > in6m->in6m_timer_expire.tv_usec)) {
		return (0);
	}
	diff = in6m->in6m_timer_expire;
	diff.tv_sec -= now.tv_sec;
	diff.tv_usec -= now.tv_usec;
	if (diff.tv_usec < 0) {
		diff.tv_sec--;
		diff.tv_usec += 1000000;
	}

	/* return the remaining time in milliseconds */
	return (diff.tv_sec * 1000 + diff.tv_usec / 1000);
}

void
mld6_start_listening(struct in6_multi *in6m)
{
	struct in6_addr all_in6;
	int s = splnet();

	/*
	 * RFC2710 page 10:
	 * The node never sends a Report or Done for the link-scope all-nodes
	 * address.
	 * MLD messages are never sent for multicast addresses whose scope is 0
	 * (reserved) or 1 (node-local).
	 */
	all_in6 = in6addr_linklocal_allnodes;
	if (in6_setscope(&all_in6, in6m->in6m_ifp, NULL)) {
		/* XXX: this should not happen! */
		in6m->in6m_timer = 0;
		in6m->in6m_state = MLD_OTHERLISTENER;
	}
	if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &all_in6) ||
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) <
	    IPV6_ADDR_SCOPE_LINKLOCAL) {
		in6m->in6m_timer = 0;
		in6m->in6m_state = MLD_OTHERLISTENER;
	} else {
		mld6_sendpkt(in6m, MLD_LISTENER_REPORT, NULL);
		in6m->in6m_timer = arc4random() %
			MLD_UNSOLICITED_REPORT_INTERVAL * hz;
		in6m->in6m_state = MLD_IREPORTEDLAST;

		mld_starttimer(in6m);
	}
	splx(s);
}

void
mld6_stop_listening(struct in6_multi *in6m)
{
	struct in6_addr allnode, allrouter;

	allnode = in6addr_linklocal_allnodes;
	if (in6_setscope(&allnode, in6m->in6m_ifp, NULL)) {
		/* XXX: this should not happen! */
		return;
	}
	allrouter = in6addr_linklocal_allrouters;
	if (in6_setscope(&allrouter, in6m->in6m_ifp, NULL)) {
		/* XXX impossible */
		return;
	}
	if (in6m->in6m_state == MLD_IREPORTEDLAST &&
	    !IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &allnode) &&
	    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) >
	    IPV6_ADDR_SCOPE_INTFACELOCAL) {
		mld6_sendpkt(in6m, MLD_LISTENER_DONE, &allrouter);
	}
}

void
mld6_input(struct mbuf *m, int off)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct mld_hdr *mldh;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct in6_multi *in6m;
	struct in6_addr mld_addr, all_in6;
	struct in6_ifaddr *ia;
	struct ifmultiaddr *ifma;
	u_long timer;		/* timer value in the MLD query header */

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
		char ip6bufs[INET6_ADDRSTRLEN], ip6bufg[INET6_ADDRSTRLEN];
		log(LOG_ERR,
		    "mld6_input: src %s is not link-local (grp=%s)\n",
		    ip6_sprintf(ip6bufs, &ip6->ip6_src),
		    ip6_sprintf(ip6bufg, &mldh->mld_addr));
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
	 * make a copy for local work (in6_setscope() may modify the 1st arg)
	 */
	mld_addr = mldh->mld_addr;
	if (in6_setscope(&mld_addr, ifp, NULL)) {
		/* XXX: this should not happen! */
		m_free(m);
		return;
	}

	/*
	 * In the MLD6 specification, there are 3 states and a flag.
	 *
	 * In Non-Listener state, we simply don't have a membership record.
	 * In Delaying Listener state, our timer is running (in6m->in6m_timer)
	 * In Idle Listener state, our timer is not running
	 * (in6m->in6m_timer==IN6M_TIMER_UNDEF)
	 *
	 * The flag is in6m->in6m_state, it is set to MLD_OTHERLISTENER if
	 * we have heard a report from another member, or MLD_IREPORTEDLAST
	 * if we sent the last report.
	 */
	switch(mldh->mld_type) {
	case MLD_LISTENER_QUERY:
		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN6_IS_ADDR_UNSPECIFIED(&mld_addr) &&
		    !IN6_IS_ADDR_MULTICAST(&mld_addr))
			break;	/* print error or log stat? */

		all_in6 = in6addr_linklocal_allnodes;
		if (in6_setscope(&all_in6, ifp, NULL)) {
			/* XXX: this should not happen! */
			break;
		}

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
		timer = ntohs(mldh->mld_maxdelay);

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

		IF_ADDR_LOCK(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_INET6)
				continue;
			in6m = (struct in6_multi *)ifma->ifma_protospec;

			if (IN6_ARE_ADDR_EQUAL(&in6m->in6m_addr, &all_in6) ||
			    IPV6_ADDR_MC_SCOPE(&in6m->in6m_addr) <
			    IPV6_ADDR_SCOPE_LINKLOCAL)
				continue;

			if (IN6_IS_ADDR_UNSPECIFIED(&mld_addr) ||
			    IN6_ARE_ADDR_EQUAL(&mld_addr, &in6m->in6m_addr)) {
				if (timer == 0) {
					/* send a report immediately */
					mld_stoptimer(in6m);
					mld6_sendpkt(in6m, MLD_LISTENER_REPORT,
						NULL);
					in6m->in6m_timer = 0; /* reset timer */
					in6m->in6m_state = MLD_IREPORTEDLAST;
				}
				else if (in6m->in6m_timer == IN6M_TIMER_UNDEF ||
				    mld_timerresid(in6m) > timer) {
					in6m->in6m_timer =
					   1 + (arc4random() % timer) * hz / 1000;
					mld_starttimer(in6m);
				}
			}
		}
		IF_ADDR_UNLOCK(ifp);
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

		if (!IN6_IS_ADDR_MULTICAST(&mld_addr))
			break;

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN6_LOOKUP_MULTI(mld_addr, ifp, in6m);
		if (in6m) {
			in6m->in6m_timer = 0; /* transit to idle state */
			in6m->in6m_state = MLD_OTHERLISTENER; /* clear flag */
		}
		break;
	default:		/* this is impossible */
		log(LOG_ERR, "mld6_input: illegal type(%d)", mldh->mld_type);
		break;
	}

	m_freem(m);
}

static void
mld6_sendpkt(struct in6_multi *in6m, int type, const struct in6_addr *dst)
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
	in6_clearscope(&mldh->mld_addr); /* XXX */
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
struct in6_multi *
in6_addmulti(struct in6_addr *maddr6, struct ifnet *ifp,
    int *errorp, int delay)
{
	struct in6_multi *in6m;

	*errorp = 0;
	in6m = NULL;

	IFF_LOCKGIANT(ifp);
	/*IN6_MULTI_LOCK();*/

	IN6_LOOKUP_MULTI(*maddr6, ifp, in6m);
	if (in6m != NULL) {
		/*
		 * If we already joined this group, just bump the
		 * refcount and return it.
		 */
		KASSERT(in6m->in6m_refcount >= 1,
		    ("%s: bad refcount %d", __func__, in6m->in6m_refcount));
		++in6m->in6m_refcount;
	} else do {
		struct in6_multi *nin6m;
		struct ifmultiaddr *ifma;
		struct sockaddr_in6 sa6;

		bzero(&sa6, sizeof(sa6));
		sa6.sin6_family = AF_INET6;
		sa6.sin6_len = sizeof(struct sockaddr_in6);
		sa6.sin6_addr = *maddr6;

		*errorp = if_addmulti(ifp, (struct sockaddr *)&sa6, &ifma);
		if (*errorp)
			break;

		/*
		 * If ifma->ifma_protospec is null, then if_addmulti() created
		 * a new record.  Otherwise, bump refcount, and we are done.
		 */
		if (ifma->ifma_protospec != NULL) {
			in6m = ifma->ifma_protospec;
			++in6m->in6m_refcount;
			break;
		}

		nin6m = malloc(sizeof(*nin6m), M_IP6MADDR, M_NOWAIT | M_ZERO);
		if (nin6m == NULL) {
			if_delmulti_ifma(ifma);
			break;
		}

		nin6m->in6m_addr = *maddr6;
		nin6m->in6m_ifp = ifp;
		nin6m->in6m_refcount = 1;
		nin6m->in6m_ifma = ifma;
		ifma->ifma_protospec = nin6m;

		nin6m->in6m_timer_ch = malloc(sizeof(*nin6m->in6m_timer_ch),
		    M_IP6MADDR, M_NOWAIT);
		if (nin6m->in6m_timer_ch == NULL) {
			free(nin6m, M_IP6MADDR);
			if_delmulti_ifma(ifma);
			break;
		}

		LIST_INSERT_HEAD(&in6_multihead, nin6m, in6m_entry);

		callout_init(nin6m->in6m_timer_ch, 0);
		nin6m->in6m_timer = delay;
		if (nin6m->in6m_timer > 0) {
			nin6m->in6m_state = MLD_REPORTPENDING;
			mld_starttimer(nin6m);
		}

		mld6_start_listening(nin6m);

		in6m = nin6m;

	} while (0);

	/*IN6_MULTI_UNLOCK();*/
	IFF_UNLOCKGIANT(ifp);

	return (in6m);
}

/*
 * Delete a multicast address record.
 *
 * TODO: Locking, as per netinet.
 */
void
in6_delmulti(struct in6_multi *in6m)
{
	struct ifmultiaddr *ifma;

	KASSERT(in6m->in6m_refcount >= 1, ("%s: freeing freed in6m", __func__));

	if (--in6m->in6m_refcount == 0) {
		mld_stoptimer(in6m);
		mld6_stop_listening(in6m);

		ifma = in6m->in6m_ifma;
		KASSERT(ifma->ifma_protospec == in6m,
		    ("%s: ifma_protospec != in6m", __func__));
		ifma->ifma_protospec = NULL;

		LIST_REMOVE(in6m, in6m_entry);
		free(in6m->in6m_timer_ch, M_IP6MADDR);
		free(in6m, M_IP6MADDR);

		if_delmulti_ifma(ifma);
	}
}
