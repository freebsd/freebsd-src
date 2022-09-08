/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
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
 *	$KAME: ip6_forward.c,v 1.69 2001/05/17 03:48:30 itojun Exp $
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_ipsec.h"
#include "opt_ipstealth.h"
#include "opt_sctp.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/errno.h>
#include <sys/time.h>
#include <sys/kernel.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/netisr.h>
#include <net/route.h>
#include <net/route/nhop.h>
#include <net/pfil.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/in6_fib.h>
#include <netinet6/ip6_var.h>
#include <netinet6/scope6_var.h>
#include <netinet/icmp6.h>
#include <netinet6/nd6.h>

#include <netinet/in_pcb.h>

#include <netipsec/ipsec_support.h>

/*
 * Forward a packet.  If some error occurs return the sender
 * an icmp packet.  Note we can't always generate a meaningful
 * icmp message because icmp doesn't have a large enough repertoire
 * of codes and types.
 *
 * If not forwarding, just drop the packet.  This could be confusing
 * if ipforwarding was zero but some routing protocol was advancing
 * us as a gateway to somewhere.  However, we must let the routing
 * protocol deal with that.
 *
 */
void
ip6_forward(struct mbuf *m, int srcrt)
{
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct sockaddr_in6 dst;
	struct nhop_object *nh = NULL;
	int error, type = 0, code = 0;
	struct mbuf *mcopy = NULL;
	struct ifnet *origifp;	/* maybe unnecessary */
	u_int32_t inzone, outzone;
	struct in6_addr odst;
	struct m_tag *fwd_tag;
	char ip6bufs[INET6_ADDRSTRLEN], ip6bufd[INET6_ADDRSTRLEN];

	/*
	 * Do not forward packets to multicast destination (should be handled
	 * by ip6_mforward().
	 * Do not forward packets with unspecified source.  It was discussed
	 * in July 2000, on the ipngwg mailing list.
	 */
	if ((m->m_flags & (M_BCAST|M_MCAST)) != 0 ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
		IP6STAT_INC(ip6s_cantforward);
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		if (V_ip6_log_time + V_ip6_log_interval < time_uptime) {
			V_ip6_log_time = time_uptime;
			log(LOG_DEBUG,
			    "cannot forward "
			    "from %s to %s nxt %d received on %s\n",
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			    ip6->ip6_nxt,
			    if_name(m->m_pkthdr.rcvif));
		}
		m_freem(m);
		return;
	}

	if (
#ifdef IPSTEALTH
	    V_ip6stealth == 0 &&
#endif
	    ip6->ip6_hlim <= IPV6_HLIMDEC) {
		/* XXX in6_ifstat_inc(rt->rt_ifp, ifs6_in_discard) */
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
		    ICMP6_TIME_EXCEED_TRANSIT, 0);
		return;
	}

	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 *
	 * It is important to save it before IPsec processing as IPsec
	 * processing may modify the mbuf.
	 */
	mcopy = m_copym(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN),
	    M_NOWAIT);
#ifdef IPSTEALTH
	if (V_ip6stealth == 0)
#endif
		ip6->ip6_hlim -= IPV6_HLIMDEC;

#if defined(IPSEC) || defined(IPSEC_SUPPORT)
	if (IPSEC_ENABLED(ipv6)) {
		if ((error = IPSEC_FORWARD(ipv6, m)) != 0) {
			/* mbuf consumed by IPsec */
			m_freem(mcopy);
			if (error != EINPROGRESS)
				IP6STAT_INC(ip6s_cantforward);
			return;
		}
		/* No IPsec processing required */
	}
#endif
	/*
	 * ip6_forward() operates with IPv6 addresses with deembedded scope.
	 *
	 * There are 3 sources of IPv6 destination address:
	 *
	 * 1) ip6_input(), where ip6_dst contains deembedded address.
	 *   In order to deal with forwarding of link-local packets,
	 *   calculate the scope based on input interface (RFC 4007, clause 9).
	 * 2) packet filters changing ip6_dst directly. It would embed scope
	 *   for LL addresses, so in6_localip() performs properly.
	 * 3) packet filters attaching PACKET_TAG_IPFORWARD would embed
	 *   scope for the nexthop.
	 */
	bzero(&dst, sizeof(struct sockaddr_in6));
	dst.sin6_family = AF_INET6;
	dst.sin6_addr = ip6->ip6_dst;
	dst.sin6_scope_id = in6_get_unicast_scopeid(&ip6->ip6_dst, m->m_pkthdr.rcvif);
again:
	nh = fib6_lookup(M_GETFIB(m), &dst.sin6_addr, dst.sin6_scope_id,
	    NHR_REF, m->m_pkthdr.flowid);
	if (nh == NULL) {
		IP6STAT_INC(ip6s_noroute);
		in6_ifstat_inc(m->m_pkthdr.rcvif, ifs6_in_noroute);
		if (mcopy) {
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
			ICMP6_DST_UNREACH_NOROUTE, 0);
		}
		goto bad;
	}

	/*
	 * Source scope check: if a packet can't be delivered to its
	 * destination for the reason that the destination is beyond the scope
	 * of the source address, discard the packet and return an icmp6
	 * destination unreachable error with Code 2 (beyond scope of source
	 * address).
	 * [draft-ietf-ipngwg-icmp-v3-04.txt, Section 3.1]
	 */
	outzone = in6_get_unicast_scopeid(&ip6->ip6_src, nh->nh_ifp);
	inzone = in6_get_unicast_scopeid(&ip6->ip6_src, m->m_pkthdr.rcvif);
	if (inzone != outzone) {
		IP6STAT_INC(ip6s_cantforward);
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(nh->nh_ifp, ifs6_in_discard);

		if (V_ip6_log_time + V_ip6_log_interval < time_uptime) {
			V_ip6_log_time = time_uptime;
			log(LOG_DEBUG,
			    "cannot forward "
			    "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			    ip6_sprintf(ip6bufs, &ip6->ip6_src),
			    ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			    ip6->ip6_nxt,
			    if_name(m->m_pkthdr.rcvif), if_name(nh->nh_ifp));
		}
		if (mcopy)
			icmp6_error(mcopy, ICMP6_DST_UNREACH,
				    ICMP6_DST_UNREACH_BEYONDSCOPE, 0);
		goto bad;
	}

	/*
	 * Destination scope check: if a packet is going to break the scope
	 * zone of packet's destination address, discard it.  This case should
	 * usually be prevented by appropriately-configured routing table, but
	 * we need an explicit check because we may mistakenly forward the
	 * packet to a different zone by (e.g.) a default route.
	 */
	inzone = in6_get_unicast_scopeid(&ip6->ip6_dst, m->m_pkthdr.rcvif);
	outzone = in6_get_unicast_scopeid(&ip6->ip6_dst, nh->nh_ifp);

	if (inzone != outzone) {
		IP6STAT_INC(ip6s_cantforward);
		IP6STAT_INC(ip6s_badscope);
		goto bad;
	}

	if (nh->nh_flags & NHF_GATEWAY) {
		/* Store gateway address in deembedded form */
		dst.sin6_addr = nh->gw6_sa.sin6_addr;
		dst.sin6_scope_id = ntohs(in6_getscope(&dst.sin6_addr));
		in6_clearscope(&dst.sin6_addr);
	}

	/*
	 * If we are to forward the packet using the same interface
	 * as one we got the packet from, perhaps we should send a redirect
	 * to sender to shortcut a hop.
	 * Only send redirect if source is sending directly to us,
	 * and if packet was not source routed (or has any options).
	 * Also, don't send redirect if forwarding using a route
	 * modified by a redirect.
	 */
	if (V_ip6_sendredirects && nh->nh_ifp == m->m_pkthdr.rcvif && !srcrt &&
	    (nh->nh_flags & NHF_REDIRECT) == 0)
		type = ND_REDIRECT;

	/*
	 * Fake scoped addresses. Note that even link-local source or
	 * destinaion can appear, if the originating node just sends the
	 * packet to us (without address resolution for the destination).
	 * Since both icmp6_error and icmp6_redirect_output fill the embedded
	 * link identifiers, we can do this stuff after making a copy for
	 * returning an error.
	 */
	if ((nh->nh_ifp->if_flags & IFF_LOOPBACK) != 0) {
		/*
		 * See corresponding comments in ip6_output.
		 * XXX: but is it possible that ip6_forward() sends a packet
		 *      to a loopback interface? I don't think so, and thus
		 *      I bark here. (jinmei@kame.net)
		 * XXX: it is common to route invalid packets to loopback.
		 *	also, the codepath will be visited on use of ::1 in
		 *	rthdr. (itojun)
		 */
#if 1
		if (0)
#else
		if ((rt->rt_flags & (RTF_BLACKHOLE|RTF_REJECT)) == 0)
#endif
		{
			printf("ip6_forward: outgoing interface is loopback. "
			       "src %s, dst %s, nxt %d, rcvif %s, outif %s\n",
			       ip6_sprintf(ip6bufs, &ip6->ip6_src),
			       ip6_sprintf(ip6bufd, &ip6->ip6_dst),
			       ip6->ip6_nxt, if_name(m->m_pkthdr.rcvif),
			       if_name(nh->nh_ifp));
		}

		/* we can just use rcvif in forwarding. */
		origifp = m->m_pkthdr.rcvif;
	}
	else
		origifp = nh->nh_ifp;
	/*
	 * clear embedded scope identifiers if necessary.
	 * in6_clearscope will touch the addresses only when necessary.
	 */
	in6_clearscope(&ip6->ip6_src);
	in6_clearscope(&ip6->ip6_dst);

	/* Jump over all PFIL processing if hooks are not active. */
	if (!PFIL_HOOKED_OUT(V_inet6_pfil_head))
		goto pass;

	odst = ip6->ip6_dst;
	/* Run through list of hooks for forwarded packets. */
	if (pfil_mbuf_out(V_inet6_pfil_head, &m, nh->nh_ifp,
	    NULL) != PFIL_PASS)
		goto freecopy;
	ip6 = mtod(m, struct ip6_hdr *);

	/* See if destination IP address was changed by packet filter. */
	if (!IN6_ARE_ADDR_EQUAL(&odst, &ip6->ip6_dst)) {
		m->m_flags |= M_SKIP_FIREWALL;
		/* If destination is now ourself drop to ip6_input(). */
		if (in6_localip(&ip6->ip6_dst))
			m->m_flags |= M_FASTFWD_OURS;
		else {
			NH_FREE(nh);

			/* Update address and scopeid. Assume scope is embedded */
			dst.sin6_scope_id = ntohs(in6_getscope(&ip6->ip6_dst));
			dst.sin6_addr = ip6->ip6_dst;
			in6_clearscope(&dst.sin6_addr);
			goto again;	/* Redo the routing table lookup. */
		}
	}

	/* See if local, if yes, send it to netisr. */
	if (m->m_flags & M_FASTFWD_OURS) {
		if (m->m_pkthdr.rcvif == NULL)
			m->m_pkthdr.rcvif = V_loif;
		if (m->m_pkthdr.csum_flags & CSUM_DELAY_DATA_IPV6) {
			m->m_pkthdr.csum_flags |=
			    CSUM_DATA_VALID_IPV6 | CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
#if defined(SCTP) || defined(SCTP_SUPPORT)
		if (m->m_pkthdr.csum_flags & CSUM_SCTP_IPV6)
			m->m_pkthdr.csum_flags |= CSUM_SCTP_VALID;
#endif
		error = netisr_queue(NETISR_IPV6, m);
		goto out;
	}
	/* Or forward to some other address? */
	if ((m->m_flags & M_IP6_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		struct sockaddr_in6 *gw6 = (struct sockaddr_in6 *)(fwd_tag + 1);

		/* Update address and scopeid. Assume scope is embedded */
		dst.sin6_scope_id = ntohs(in6_getscope(&gw6->sin6_addr));
		dst.sin6_addr = gw6->sin6_addr;
		in6_clearscope(&dst.sin6_addr);

		m->m_flags |= M_SKIP_FIREWALL;
		m->m_flags &= ~M_IP6_NEXTHOP;
		m_tag_delete(m, fwd_tag);
		NH_FREE(nh);
		goto again;
	}

pass:
	/* See if the size was changed by the packet filter. */
	/* TODO: change to nh->nh_mtu */
	if (m->m_pkthdr.len > IN6_LINKMTU(nh->nh_ifp)) {
		in6_ifstat_inc(nh->nh_ifp, ifs6_in_toobig);
		if (mcopy)
			icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0,
			    IN6_LINKMTU(nh->nh_ifp));
		goto bad;
	}

	/* Currently LLE layer stores embedded IPv6 addresses */
	if (IN6_IS_SCOPE_LINKLOCAL(&dst.sin6_addr)) {
		in6_set_unicast_scopeid(&dst.sin6_addr, dst.sin6_scope_id);
		dst.sin6_scope_id = 0;
	}
	error = nd6_output_ifp(nh->nh_ifp, origifp, m, &dst, NULL);
	if (error) {
		in6_ifstat_inc(nh->nh_ifp, ifs6_out_discard);
		IP6STAT_INC(ip6s_cantforward);
	} else {
		IP6STAT_INC(ip6s_forward);
		in6_ifstat_inc(nh->nh_ifp, ifs6_out_forward);
		if (type)
			IP6STAT_INC(ip6s_redirectsent);
		else {
			if (mcopy)
				goto freecopy;
		}
	}

	if (mcopy == NULL)
		goto out;
	switch (error) {
	case 0:
		if (type == ND_REDIRECT) {
			icmp6_redirect_output(mcopy, nh);
			goto out;
		}
		goto freecopy;

	case EMSGSIZE:
		/* xxx MTU is constant in PPP? */
		goto freecopy;

	case ENOBUFS:
		/* Tell source to slow down like source quench in IP? */
		goto freecopy;

	case ENETUNREACH:	/* shouldn't happen, checked above */
	case EHOSTUNREACH:
	case ENETDOWN:
	case EHOSTDOWN:
	default:
		type = ICMP6_DST_UNREACH;
		code = ICMP6_DST_UNREACH_ADDR;
		break;
	}
	icmp6_error(mcopy, type, code, 0);
	goto out;

 freecopy:
	m_freem(mcopy);
	goto out;
bad:
	m_freem(m);
out:
	if (nh != NULL)
		NH_FREE(nh);
}
