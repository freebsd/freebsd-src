/*
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
 *	@(#)igmp.c	8.1 (Berkeley) 7/19/93
 * $Id: igmp.c,v 1.14 1995/12/02 19:37:52 bde Exp $
 */

/*
 * Internet Group Management Protocol (IGMP) routines.
 *
 * Written by Steve Deering, Stanford, May 1988.
 * Modified by Rosen Sharma, Stanford, Aug 1994.
 * Modified by Bill Fenner, Xerox PARC, Feb 1995.
 *
 * MULTICAST Revision: 3.3.1.2
 */

#include <sys/param.h>
#include <sys/systm.h>
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

static int	fill_rti __P((struct in_multi *inm));
static struct router_info *
		find_rti __P((struct ifnet *ifp));

static struct igmpstat igmpstat;

SYSCTL_STRUCT(_net_inet_igmp, IGMPCTL_STATS, stats, CTLFLAG_RD,
	&igmpstat, igmpstat, "");

static int igmp_timers_are_running;
static u_long igmp_all_hosts_group;
static u_long igmp_local_group;
static u_long igmp_local_group_mask;
static struct router_info *Head;

static void igmp_sendpkt(struct in_multi *, int);
static void igmp_sendleave(struct in_multi *);

void
igmp_init()
{
	/*
	 * To avoid byte-swapping the same value over and over again.
	 */
	igmp_all_hosts_group = htonl(INADDR_ALLHOSTS_GROUP);
	igmp_local_group = htonl(0xe0000000);		/* 224.0.0.0 */
	igmp_local_group_mask = htonl(0xffffff00);	/* ........^ */

	igmp_timers_are_running = 0;

	Head = (struct router_info *) 0;
}

static int
fill_rti(inm)
	struct in_multi *inm;
{
	register struct router_info *rti = Head;

#ifdef IGMP_DEBUG
	printf("[igmp.c, _fill_rti] --> entering \n");
#endif
	while (rti) {
		if (rti->ifp == inm->inm_ifp) {
			inm->inm_rti  = rti;
#ifdef IGMP_DEBUG
			printf("[igmp.c, _fill_rti] --> found old entry \n");
#endif
			if (rti->type == IGMP_OLD_ROUTER) 
				return IGMP_HOST_MEMBERSHIP_REPORT;
			else
				return IGMP_HOST_NEW_MEMBERSHIP_REPORT;
		}
		rti = rti->next;
	}
	MALLOC(rti, struct router_info *, sizeof *rti, M_MRTABLE, M_NOWAIT);
	rti->ifp = inm->inm_ifp;
	rti->type = IGMP_NEW_ROUTER;
	rti->time = IGMP_AGE_THRESHOLD;
	rti->next = Head;
	Head = rti;	
	inm->inm_rti = rti;
#ifdef IGMP_DEBUG
	printf("[igmp.c, _fill_rti] --> created new entry \n");
#endif
	return IGMP_HOST_NEW_MEMBERSHIP_REPORT;
}

static struct router_info *
find_rti(ifp)
	struct ifnet *ifp;
{
        register struct router_info *rti = Head;

#ifdef IGMP_DEBUG
	printf("[igmp.c, _find_rti] --> entering \n");
#endif
        while (rti) {
                if (rti->ifp == ifp) {
#ifdef IGMP_DEBUG
			printf("[igmp.c, _find_rti] --> found old entry \n");
#endif
                        return rti;
                }
                rti = rti->next;
        }
	MALLOC(rti, struct router_info *, sizeof *rti, M_MRTABLE, M_NOWAIT);
        rti->ifp = ifp;
        rti->type = IGMP_NEW_ROUTER;
        rti->time = IGMP_AGE_THRESHOLD;
        rti->next = Head;
        Head = rti;
#ifdef IGMP_DEBUG
	printf("[igmp.c, _find_rti] --> created an entry \n");
#endif
        return rti;
}

void
igmp_input(m, iphlen)
	register struct mbuf *m;
	register int iphlen;
{
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
	rti = find_rti(ifp);

	switch (igmp->igmp_type) {

	case IGMP_HOST_MEMBERSHIP_QUERY:
		++igmpstat.igps_rcv_queries;

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (igmp->igmp_code == 0) {

			rti->type = IGMP_OLD_ROUTER; rti->time = 0;

			/*
			** Do exactly as RFC 1112 says
			*/

			if (ip->ip_dst.s_addr != igmp_all_hosts_group) {
				++igmpstat.igps_rcv_badqueries;
				m_freem(m);
				return;
			}

			/*
			 * Start the timers in all of our membership records for
			 * the interface on which the query arrived, except those
			 * that are  already running and those that belong to a
			 * "local" group (224.0.0.X).
			 */
			IN_FIRST_MULTI(step, inm);
			while (inm != NULL) {
				if (inm->inm_ifp == ifp 
				    && inm->inm_timer == 0
				    && ((inm->inm_addr.s_addr 
					 & igmp_local_group_mask) 
					!= igmp_local_group)) {

					inm->inm_state = IGMP_DELAYING_MEMBER;
					inm->inm_timer = IGMP_RANDOM_DELAY(
				IGMP_MAX_HOST_REPORT_DELAY * PR_FASTHZ );

					igmp_timers_are_running = 1;
				}
				IN_NEXT_MULTI(step, inm);
			}
		} else {
		    /*
		    ** New Router
		    */
		    
		    if (!(m->m_flags & M_MCAST)) {
			++igmpstat.igps_rcv_badqueries;
			m_freem(m);
			return;
		    }
		    
		    /*
		     * - Start the timers in all of our membership records
		     *   that the query applies to for the interface on
		     *   which the query arrived excl. those that belong
		     *   to a "local" group (224.0.0.X)
		     * - For timers already running check if they need to
		     *   be reset.
		     * - Use the igmp->igmp_code field as the maximum 
		     *   delay possible
		     */
		    IN_FIRST_MULTI(step, inm);
		    while (inm != NULL) {
			if (inm->inm_ifp == ifp &&
			    (inm->inm_addr.s_addr & igmp_local_group_mask) !=
				igmp_local_group &&
			    (ip->ip_dst.s_addr == igmp_all_hosts_group ||
			     ip->ip_dst.s_addr == inm->inm_addr.s_addr)) {
			    switch(inm->inm_state) {
			      case IGMP_IDLE_MEMBER:
			      case IGMP_LAZY_MEMBER:
			      case IGMP_AWAKENING_MEMBER:
				    inm->inm_timer = IGMP_RANDOM_DELAY(timer);
				    igmp_timers_are_running = 1;
				    inm->inm_state = IGMP_DELAYING_MEMBER;
				    break;
			      case IGMP_DELAYING_MEMBER:
				if (inm->inm_timer > timer) {
				    inm->inm_timer = IGMP_RANDOM_DELAY(timer);
				    igmp_timers_are_running = 1;
				    inm->inm_state = IGMP_DELAYING_MEMBER;
				}
				break;
			      case IGMP_SLEEPING_MEMBER:
				inm->inm_state = IGMP_AWAKENING_MEMBER;
				break;
			    }
			}
			IN_NEXT_MULTI(step, inm);
		}
	    }

		break;

	case IGMP_HOST_MEMBERSHIP_REPORT:
		/*
		 * an old report
		 */
		++igmpstat.igps_rcv_reports;

		if (ifp->if_flags & IFF_LOOPBACK)
			break;

		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr)) ||
		    igmp->igmp_group.s_addr != ip->ip_dst.s_addr) {
			++igmpstat.igps_rcv_badreports;
			m_freem(m);
			return;
		}

		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0) {
			IFP_TO_IA(ifp, ia);
			if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);
		}

		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN_LOOKUP_MULTI(igmp->igmp_group, ifp, inm);

		if (inm != NULL) {
		  inm->inm_timer = 0;
		  ++igmpstat.igps_rcv_ourreports;
		  
		  switch(inm->inm_state){
		  case IGMP_IDLE_MEMBER:
		  case IGMP_LAZY_MEMBER:
		  case IGMP_AWAKENING_MEMBER:
		  case IGMP_SLEEPING_MEMBER:
		    inm->inm_state = IGMP_SLEEPING_MEMBER;
		    break;
		  case IGMP_DELAYING_MEMBER:
		    if (inm->inm_rti->type == IGMP_OLD_ROUTER)
			inm->inm_state = IGMP_LAZY_MEMBER;
		    else
			inm->inm_state = IGMP_SLEEPING_MEMBER;
		    break;
		  }
		}
	      
		break;

	      case IGMP_HOST_NEW_MEMBERSHIP_REPORT:
		/*
		 * a new report
		 */

		/*
		 * We can get confused and think there's someone
		 * else out there if we are a multicast router.
		 * For fast leave to work, we have to know that
		 * we are the only member.
		 */
		IFP_TO_IA(ifp, ia);
		if (ia && ip->ip_src.s_addr == IA_SIN(ia)->sin_addr.s_addr)
			break;

		++igmpstat.igps_rcv_reports;
    
		if (ifp->if_flags & IFF_LOOPBACK)
		  break;
		
		if (!IN_MULTICAST(ntohl(igmp->igmp_group.s_addr)) ||
		    igmp->igmp_group.s_addr != ip->ip_dst.s_addr) {
		  ++igmpstat.igps_rcv_badreports;
		  m_freem(m);
		  return;
		}
		
		/*
		 * KLUDGE: if the IP source address of the report has an
		 * unspecified (i.e., zero) subnet number, as is allowed for
		 * a booting host, replace it with the correct subnet number
		 * so that a process-level multicast routing demon can
		 * determine which subnet it arrived from.  This is necessary
		 * to compensate for the lack of any way for a process to
		 * determine the arrival interface of an incoming packet.
		 */
		if ((ntohl(ip->ip_src.s_addr) & IN_CLASSA_NET) == 0) {
/* #ifndef MROUTING XXX - I don't think the ifdef is necessary */
		  IFP_TO_IA(ifp, ia);
/* #endif */
		  if (ia) ip->ip_src.s_addr = htonl(ia->ia_subnet);
		}
		
		/*
		 * If we belong to the group being reported, stop
		 * our timer for that group.
		 */
		IN_LOOKUP_MULTI(igmp->igmp_group, ifp, inm);
		if (inm != NULL) {
		  inm->inm_timer = 0;
		  ++igmpstat.igps_rcv_ourreports;
		  
		  switch(inm->inm_state){
		  case IGMP_DELAYING_MEMBER:
		  case IGMP_IDLE_MEMBER:
		    inm->inm_state = IGMP_LAZY_MEMBER;
		    break;
		  case IGMP_AWAKENING_MEMBER:
		    inm->inm_state = IGMP_LAZY_MEMBER;
		    break;
		  case IGMP_LAZY_MEMBER:
		  case IGMP_SLEEPING_MEMBER:
		    break;
		  }
		}
	}

	/*
	 * Pass all valid IGMP packets up to any process(es) listening
	 * on a raw IGMP socket.
	 */
	rip_input(m);
}

void
igmp_joingroup(inm)
	struct in_multi *inm;
{
	int s = splnet();

	inm->inm_state = IGMP_IDLE_MEMBER;

	if ((inm->inm_addr.s_addr & igmp_local_group_mask) == igmp_local_group
	    || inm->inm_ifp->if_flags & IFF_LOOPBACK)
		inm->inm_timer = 0;
	else {
		igmp_sendpkt(inm,fill_rti(inm));
		inm->inm_timer = IGMP_RANDOM_DELAY(
					IGMP_MAX_HOST_REPORT_DELAY*PR_FASTHZ);
		inm->inm_state = IGMP_DELAYING_MEMBER;
		igmp_timers_are_running = 1;
	}
	splx(s);
}

void
igmp_leavegroup(inm)
	struct in_multi *inm;
{
         switch(inm->inm_state) {
	 case IGMP_DELAYING_MEMBER:
	 case IGMP_IDLE_MEMBER:
	   if (((inm->inm_addr.s_addr & igmp_local_group_mask) 
		!= igmp_local_group)
	       && !(inm->inm_ifp->if_flags & IFF_LOOPBACK))
	       if (inm->inm_rti->type != IGMP_OLD_ROUTER)
		   igmp_sendleave(inm);
	   break;
	 case IGMP_LAZY_MEMBER:
	 case IGMP_AWAKENING_MEMBER:
	 case IGMP_SLEEPING_MEMBER:
	   break;
	 }
}

void
igmp_fasttimo()
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
		  if (inm->inm_state == IGMP_DELAYING_MEMBER) {
		    if (inm->inm_rti->type == IGMP_OLD_ROUTER)
			igmp_sendpkt(inm, IGMP_HOST_MEMBERSHIP_REPORT);
		    else
			igmp_sendpkt(inm, IGMP_HOST_NEW_MEMBERSHIP_REPORT);
		    inm->inm_state = IGMP_IDLE_MEMBER;
		  }
		} else {
			igmp_timers_are_running = 1;
		}
		IN_NEXT_MULTI(step, inm);
	}
	splx(s);
}

void
igmp_slowtimo()
{
	int s = splnet();
	register struct router_info *rti =  Head;

#ifdef IGMP_DEBUG
	printf("[igmp.c,_slowtimo] -- > entering \n");
#endif
	while (rti) {
		rti->time ++;
		if (rti->time >= IGMP_AGE_THRESHOLD){
			rti->type = IGMP_NEW_ROUTER;
			rti->time = IGMP_AGE_THRESHOLD;
		}
		rti = rti->next;
	}
#ifdef IGMP_DEBUG	
	printf("[igmp.c,_slowtimo] -- > exiting \n");
#endif
	splx(s);
}

static void
igmp_sendpkt(inm, type)
	struct in_multi *inm;
	int type;
{
        struct mbuf *m;
        struct igmp *igmp;
        struct ip *ip;
        struct ip_moptions *imo;

        MGETHDR(m, M_DONTWAIT, MT_HEADER);
        if (m == NULL)
                return;

	MALLOC(imo, struct ip_moptions *, sizeof *imo, M_IPMOPTS, M_DONTWAIT);
	if (!imo) {
		m_free(m);
		return;
	}

	m->m_pkthdr.rcvif = loif;
	m->m_pkthdr.len = sizeof(struct ip) + IGMP_MINLEN;
	MH_ALIGN(m, IGMP_MINLEN + sizeof(struct ip));
	m->m_data += sizeof(struct ip);
        m->m_len = IGMP_MINLEN;
        igmp = mtod(m, struct igmp *);
        igmp->igmp_type   = type;
        igmp->igmp_code   = 0;
        igmp->igmp_group  = inm->inm_addr;
        igmp->igmp_cksum  = 0;
        igmp->igmp_cksum  = in_cksum(m, IGMP_MINLEN);

        m->m_data -= sizeof(struct ip);
        m->m_len += sizeof(struct ip);
        ip = mtod(m, struct ip *);
        ip->ip_tos        = 0;
        ip->ip_len        = sizeof(struct ip) + IGMP_MINLEN;
        ip->ip_off        = 0;
        ip->ip_p          = IPPROTO_IGMP;
        ip->ip_src.s_addr = INADDR_ANY;
        ip->ip_dst        = igmp->igmp_group;

        imo->imo_multicast_ifp  = inm->inm_ifp;
        imo->imo_multicast_ttl  = 1;
	imo->imo_multicast_vif  = -1;
        /*
         * Request loopback of the report if we are acting as a multicast
         * router, so that the process-level routing demon can hear it.
         */
        imo->imo_multicast_loop = (ip_mrouter != NULL);

        ip_output(m, (struct mbuf *)0, (struct route *)0, 0, imo);

	FREE(imo, M_IPMOPTS);
        ++igmpstat.igps_snd_reports;
}

static void
igmp_sendleave(inm)
	struct in_multi *inm;
{
	igmp_sendpkt(inm, IGMP_HOST_LEAVE_MESSAGE);
}
