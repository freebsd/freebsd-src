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
 * $FreeBSD$
 */

/*
 * Internet Group Management Protocol (IGMP) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb 1995.
 * Modified to fully comply to IGMPv2 by Bill Fenner, Oct 1995.
 *
 * MULTICAST Revision: 3.5.1.4
 */

#include "opt_mac.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mac.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/protosw.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/igmp.h>
#include <netinet/igmp_var.h>

#include <machine/in_cksum.h>

static MALLOC_DEFINE(M_IGMP, "igmp", "igmp state");

static struct router_info	*find_rti(struct ifnet *ifp);
static void	igmp_sendpkt(struct in_multi *, int, unsigned long);

static struct igmpstat igmpstat;

SYSCTL_STRUCT(_net_inet_igmp, IGMPCTL_STATS, stats, CTLFLAG_RW, &igmpstat,
    igmpstat, "");

/*
 * igmp_mtx protects all mutable global variables in igmp.c, as well as
 * the data fields in struct router_info.  In general, a router_info
 * structure will be valid as long as the referencing struct in_multi is
 * valid, so no reference counting is used.  We allow unlocked reads of
 * router_info data when accessed via an in_multi read-only.
 */
static struct mtx igmp_mtx;
static SLIST_HEAD(, router_info) router_info_head;
static int igmp_timers_are_running;

/*
 * XXXRW: can we define these such that these can be made const?  In any
 * case, these shouldn't be changed after igmp_init() and therefore don't
 * need locking.
 */
static u_long igmp_all_hosts_group;
static u_long igmp_all_rtrs_group;

static struct mbuf *router_alert;
static struct route igmprt;

#ifdef IGMP_DEBUG
#define	IGMP_PRINTF(x)	printf(x)
#else
#define	IGMP_PRINTF(x)
#endif

void
igmp_init(void)
{
	struct ipoption *ra;

	/*
	 * To avoid byte-swapping the same value over and over again.
	 */
	igmp_all_hosts_group = htonl(INADDR_ALLHOSTS_GROUP);
	igmp_all_rtrs_group = htonl(INADDR_ALLRTRS_GROUP);

	igmp_timers_are_running = 0;

	/*
	 * Construct a Router Alert option to use in outgoing packets
	 */
	MGET(router_alert, M_DONTWAIT, MT_DATA);
	ra = mtod(router_alert, struct ipoption *);
	ra->ipopt_dst.s_addr = 0;
	ra->ipopt_list[0] = IPOPT_RA;	/* Router Alert Option */
	ra->ipopt_list[1] = 0x04;	/* 4 bytes long */
	ra->ipopt_list[2] = 0x00;
	ra->ipopt_list[3] = 0x00;
	router_alert->m_len = sizeof(ra->ipopt_dst) + ra->ipopt_list[1];

	mtx_init(&igmp_mtx, "igmp_mtx", NULL, MTX_DEF);
	SLIST_INIT(&router_info_head);
}

static struct router_info *
find_rti(struct ifnet *ifp)
{
	struct router_info *rti;

	mtx_assert(&igmp_mtx, MA_OWNED);
	IGMP_PRINTF("[igmp.c, _find_rti] --> entering \n");
	SLIST_FOREACH(rti, &router_info_head, rti_list) {
		if (rti->rti_ifp == ifp) {
			IGMP_PRINTF(
			    "[igmp.c, _find_rti] --> found old entry \n");
			return rti;
		}
	}
	/*
	 * XXXRW: return value of malloc not checked, despite M_NOWAIT.
	 */
	MALLOC(rti, struct router_info *, sizeof *rti, M_IGMP, M_NOWAIT);
	rti->rti_ifp = ifp;
	rti->rti_type = IGMP_V2_ROUTER;
	rti->rti_time = 0;
	SLIST_INSERT_HEAD(&router_info_head, rti, rti_list);

	IGMP_PRINTF("[igmp.c, _find_rti] --> created an entry \n");
	return rti;
}

void
igmp_input(register struct mbuf *m, int off)
{
	register int iphlen = off;
	register struct igmp *igmp;
	register struct ip *ip;
	register int igmplen;
	register struct ifnet *ifp = m->m_pkthdr.rcvif;
	register int minlen;
	register struct in_multi *inm;
	register struct in_ifaddr *ia;
	struct in_multistep step;
	struct router_info *rti;
	int timer; /** timer value in the igmp query header **/

	++igmpstat.igps_rcv_total;

	ip = mtod(m, struct ip *);
	igmplen = ip->ip_len;

	/*
	 * Validate lengths
	 */
	if (igmplen < IGMP_MINLEN) {
		++igmpstat.igps_rcv_tooshort;
		m_freem(m);
		return;
	}
	minlen = iphlen + IGMP_MINLEN;
	if ((m->m_flags & M_EXT || m->m_len < minlen) &&
	    (m = m_pullup(m, minlen)) == 0) {
		++igmpstat.igps_rcv_tooshort;
		return;
	}

	/*
	 * Validate checksum
	 */
	m->m_data += iphlen;
	m->m_len -= iphlen;
	igmp = mtod(m, struct igmp *);
	if (in_cksum(m, igmplen)) {
		++igmpstat.igps_rcv_badsum;
		m_freem(m);
		return;
	}
	m->m_data -= iphlen;
	m->m_len += iphlen;

	ip = mtod(m, struct ip *);
	timer = igmp->igmp_code * PR_FASTHZ / IGMP_TIMER_SCALE;
	if (timer == 0)
		timer = 1;

	/*
	 * In the IGMPv2 specification, there are 3 states and a flag.
	 *
	 * In Non-Member state, we simply don't have a membership record.
	 * In Delaying Member state, our timer is running (inm->inm_timer)
	 * In Idle Member state, our timer is not running (inm->inm_timer==0)
	 *
	 * The flag is inm->inm_state, it is set to IGMP_OTHERMEMBER if
	 * we have heard a report from another member, or IGMP_IREPORTEDLAST
	 * if I sent the last report.
	 */
	switch (igmp->igmp_type) {
	case IGMP_MEMBERSHIP_QUERY:
		++igmpstat.igps_rcv_queries;

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (igmp->igmp_code == 0) {
			/*
			 * Old router.  Remember that the querier on this
			 * interface is old, and set the timer to the
			 * value in RFC 1112.
			 */

			mtx_lock(&igmp_mtx);
			rti = find_rti(ifp);
			rti->rti_type = IGMP_V1_ROUTER;
			rti->rti_time = 0;
			mtx_unlock(&igmp_mtx);

			timer = IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ;

			if (ip->ip_dst.s_addr != igmp_all_hosts_group ||
			    igmp->igmp_group.s_addr != 0) {
				++igmpstat.igps_rcv_badqueries;
				m_freem(m);
				return;
			}
		} else {
			/*
			 * New router.  Simply do the new validity check.
			 */
			
			if (igmp->igmp_group.s_addr != 0 &&
			    !IN_MULTICAST(ntohl(igmp->igmp_group.s_addr))) {
				++igmpstat.igps_rcv_badqueries;
				m_freem(m);
				return;
			}
		}

		/*
		 * - Start the timers in all of our membership records
		 *   that the query applies to for the interface on
		 *   which the query arrived excl. those that belong
		 *   to the "all-hosts" group (224.0.0.1).
		 * - Restart any timer that is already running but has
		 *   a value longer than the requested timeout.
		 * - Use the value specified in the query message as
		 *   the maximum timeout.
		 */
		IN_FIRST_MULTI(step, inm);
		while (inm != NULL) {
			if (inm->inm_ifp == ifp &&
			    inm->inm_addr.s_addr != igmp_all_hosts_group &&
			    (igmp->igmp_group.s_addr == 0 ||
			     igmp->igmp_group.s_addr == inm->inm_addr.s_addr)) {
				if (inm->inm_timer == 0 ||
				    inm->inm_timer > timer) {
					inm->inm_timer =
						IGMP_RANDOM_DELAY(timer);
					igmp_timers_are_running = 1;
				}
			}
			IN_NEXT_MULTI(step, inm);
		}

		break;

	case IGMP_V1_MEMBERSHIP_REPORT:
	case IGMP_V2_MEMBERSHIP_REPORT:
		/*
		 * For fast leave to work, we have to know that we are the
		 * last person to send a report for this group.  Reports
		 * can potentially get looped back if we are a multicast
		 * router, so discard reports sourced by me.
		 */
		IFP_TO_IA(ifp, ia);
		if (ia && ip->ip_src.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;

		++igmpstat.igps_rcv_reports;

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr))) {
			++igmpstat.igps_rcv_badreports;
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing daemon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0)
			if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN_LOOKUP_MULTI(igmp->igmp_group, ifp, inm);

		if (inm != NULL) {
			inm->inm_timer = 0;
			++igmpstat.igps_rcv_ourreports;

			inm->inm_state = IGMP_OTHERMEMBER;
		}

		break;
	}

	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket.
	 */
	rip_input(m, off);
}

void
igmp_joingroup(struct in_multi *inm)
{
	int s = splnet();

	if (inm->inm_addr.s_addr == igmp_all_hosts_group
	    || inm->inm_ifp->if_flags & IFF_LOOPBACK) {
		inm->inm_timer = 0;
		inm->inm_state = IGMP_OTHERMEMBER;
	} else {
		mtx_lock(&igmp_mtx);
		inm->inm_rti = find_rti(inm->inm_ifp);
		mtx_unlock(&igmp_mtx);
		igmp_sendpkt(inm, inm->inm_rti->rti_type, 0);
		inm->inm_timer = IGMP_RANDOM_DELAY(
					IGMP_MAX_HOST_REPORT_DELAY*PR_FASTHZ);
		inm->inm_state = IGMP_IREPORTEDLAST;
		igmp_timers_are_running = 1;
	}
	splx(s);
}

void
igmp_leavegroup(struct in_multi *inm)
{

	if (inm->inm_state == IGMP_IREPORTEDLAST &&
	    inm->inm_addr.s_addr != igmp_all_hosts_group &&
	    !(inm->inm_ifp->if_flags & IFF_LOOPBACK) &&
	    inm->inm_rti->rti_type != IGMP_V1_ROUTER)
		igmp_sendpkt(inm, IGMP_V2_LEAVE_GROUP, igmp_all_rtrs_group);
}

void
igmp_fasttimo(void)
{
	register struct in_multi *inm;
	struct in_multistep step;
	int s;

	/*
	 * Quick check to see if any work needs to be done, in order
	 * to minimize the overhead of fasttimo processing.
	 */

	if (!igmp_timers_are_running)
		return;

	s = splnet();
	igmp_timers_are_running = 0;
	IN_FIRST_MULTI(step, inm);
	while (inm != NULL) {
		if (inm->inm_timer == 0) {
			/* do nothing */
		} else if (--inm->inm_timer == 0) {
			igmp_sendpkt(inm, inm->inm_rti->rti_type, 0);
			inm->inm_state = IGMP_IREPORTEDLAST;
		} else {
			igmp_timers_are_running = 1;
		}
		IN_NEXT_MULTI(step, inm);
	}
	splx(s);
}

void
igmp_slowtimo(void)
{
	int s = splnet();
	struct router_info *rti;

	IGMP_PRINTF("[igmp.c,_slowtimo] -- > entering \n");
	mtx_lock(&igmp_mtx);
	SLIST_FOREACH(rti, &router_info_head, rti_list) {
		if (rti->rti_type == IGMP_V1_ROUTER) {
			rti->rti_time++;
			if (rti->rti_time >= IGMP_AGE_THRESHOLD)
				rti->rti_type = IGMP_V2_ROUTER;
		}
	}
	mtx_unlock(&igmp_mtx);
	IGMP_PRINTF("[igmp.c,_slowtimo] -- > exiting \n");
	splx(s);
}

static void
igmp_sendpkt(struct in_multi *inm, int type, unsigned long addr)
{
	struct mbuf *m;
	struct igmp *igmp;
	struct ip *ip;
	struct ip_moptions imo;

	MGETHDR(m, M_DONTWAIT, MT_HEADER);
	if (m == NULL)
		return;

	m->m_pkthdr.rcvif = loif;
#ifdef MAC
	mac_create_mbuf_linklayer(inm->inm_ifp, m);
#endif
	m->m_pkthdr.len = sizeof(struct ip) + IGMP_MINLEN;
	MH_ALIGN(m, IGMP_MINLEN + sizeof(struct ip));
	m->m_data += sizeof(struct ip);
	m->m_len = IGMP_MINLEN;
	igmp = mtod(m, struct igmp *);
	igmp->igmp_type = type;
	igmp->igmp_code = 0;
	igmp->igmp_group = inm->inm_addr;
	igmp->igmp_cksum = 0;
	igmp->igmp_cksum = in_cksum(m, IGMP_MINLEN);

	m->m_data -= sizeof(struct ip);
	m->m_len += sizeof(struct ip);
	ip = mtod(m, struct ip *);
	ip->ip_tos = 0;
	ip->ip_len = sizeof(struct ip) + IGMP_MINLEN;
	ip->ip_off = 0;
	ip->ip_p = IPPROTO_IGMP;
	ip->ip_src.s_addr = INADDR_ANY;
	ip->ip_dst.s_addr = addr ? addr : igmp->igmp_group.s_addr;

	imo.imo_multicast_ifp  = inm->inm_ifp;
	imo.imo_multicast_ttl  = 1;
	imo.imo_multicast_vif  = -1;
	/*
	 * Request loopback of the report if we are acting as a multicast
	 * router, so that the process-level routing daemon can hear it.
	 */
	imo.imo_multicast_loop = (ip_mrouter != NULL);

	/*
	 * XXX
	 * Do we have to worry about reentrancy here?  Don't think so.
	 */
	ip_output(m, router_alert, &igmprt, 0, &imo, NULL);

	++igmpstat.igps_snd_reports;
}
