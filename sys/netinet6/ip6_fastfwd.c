/*-
 * Copyright (c) 2014 Andrey V. Elsukov <ae@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet6.h"
#include "opt_ipstealth.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/route.h>
#include <net/pfil.h>
#include <net/vnet.h>

#include <netinet/in.h>
#include <netinet/ip_var.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet6/in6_var.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>

static VNET_DEFINE(int, ip6_fastforward_active);
#define	V_ip6_fastforward_active	VNET(ip6_fastforward_active)

SYSCTL_DECL(_net_inet6);
SYSCTL_DECL(_net_inet6_ip6);
SYSCTL_VNET_INT(_net_inet6_ip6, OID_AUTO, fastforwarding, CTLFLAG_RW,
    &VNET_NAME(ip6_fastforward_active), 0, "Enable fast IPv6 forwarding");

struct mbuf*
ip6_fastforward(struct mbuf *m)
{
	struct route_in6 ro;
	struct m_tag *fwd_tag;
	struct ip6_hdr *ip6;
	struct ifnet *rcvif, *oif;
	struct mbuf *mcopy;
	uint32_t plen;
	int mflags, mnext;

	/*
	 * Save these variables for statistics accounting.
	 * For correct accounting we use `goto drop;', when we are
	 * going to exit, even if there are already nothing to free.
	 * And we use `return (m)', when we want leave statistics
	 * accounting for ip6_input.
	 * XXX: there is one path inside IP6_EXTHDR_CHECK(),  where we
	 * can lost some accounting.
	 */
	mflags = m->m_flags;
	mnext = (m->m_next != NULL);
	rcvif = m->m_pkthdr.rcvif;
	mcopy = NULL;
	/*
	 * Drop the packet if IPv6 operation is disabled on the interface.
	 */
	if ((ND_IFINFO(rcvif)->flags & ND6_IFF_IFDISABLED))
		goto dropin;

	if (!V_ip6_fastforward_active || !V_ip6_forwarding)
		return (m);
#ifndef PULLDOWN_TEST
	/*
	 * L2 bridge code and some other code can return mbuf chain
	 * that does not conform to KAME requirement.  too bad.
	 * XXX: fails to join if interface MTU > MCLBYTES.  jumbogram?
	 */
	if (m && m->m_next != NULL && m->m_pkthdr.len < MCLBYTES) {
		struct mbuf *n;

		MGETHDR(n, M_DONTWAIT, MT_HEADER);
		if (n)
			M_MOVE_PKTHDR(n, m);
		if (n && n->m_pkthdr.len > MHLEN) {
			MCLGET(n, M_DONTWAIT);
			if ((n->m_flags & M_EXT) == 0) {
				m_freem(n);
				n = NULL;
			}
		}
		if (n == NULL)
			goto dropin;

		m_copydata(m, 0, n->m_pkthdr.len, mtod(n, caddr_t));
		n->m_len = n->m_pkthdr.len;
		m_freem(m);
		m = n;
	}
	IP6_EXTHDR_CHECK(m, 0, sizeof(struct ip6_hdr), NULL);
#else
	/*
	 * Check for packet drop condition and do sanity checks.
	 */
	if (m->m_len < sizeof(struct ip6_hdr)) {
		if ((m = m_pullup(m, sizeof(struct ip6_hdr))) == NULL) {
			IP6STAT_INC(ip6s_tooshort);
			in6_ifstat_inc(rcvif, ifs6_in_hdrerr);
			goto dropin;
		}
	}
#endif
	ip6 = mtod(m, struct ip6_hdr *);
	if ((ip6->ip6_vfc & IPV6_VERSION_MASK) != IPV6_VERSION) {
		IP6STAT_INC(ip6s_badvers);
		in6_ifstat_inc(rcvif, ifs6_in_hdrerr);
		goto dropin;
	}
	/*
	 *  Check against address spoofing/corruption.
	 */
	if (IN6_IS_ADDR_MULTICAST(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_src) ||
	    IN6_IS_ADDR_V4MAPPED(&ip6->ip6_dst)) {
		IP6STAT_INC(ip6s_badscope);
		in6_ifstat_inc(rcvif, ifs6_in_addrerr);
		goto dropin;
	}
	/*
	 * Fallback conditions to ip6_input for slow path processing.
	 */
	if (ip6->ip6_nxt == IPPROTO_HOPOPTS ||
	    IN6_IS_ADDR_MULTICAST(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_dst) ||
	    IN6_IS_ADDR_LINKLOCAL(&ip6->ip6_src) ||
	    IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src) ||
	    in6_localip(&ip6->ip6_dst))
		return (m);
	/*
	 * Check that the amount of data in the buffers
	 * is as at least much as the IPv6 header would have us expect.
	 * Trim mbufs if longer than we expect.
	 * Drop packet if shorter than we expect.
	 */
	plen = ntohs(ip6->ip6_plen);
	if (plen == 0) {
		/*
		 * Jumbograms must have hop-by-hop header and go via
		 * slow path.
		 */
		IP6STAT_INC(ip6s_badoptions);
		goto dropin;
	}
	if (m->m_pkthdr.len - sizeof(struct ip6_hdr) < plen) {
		IP6STAT_INC(ip6s_tooshort);
		in6_ifstat_inc(rcvif, ifs6_in_truncated);
		goto dropin;
	}
	if (m->m_pkthdr.len > sizeof(struct ip6_hdr) + plen) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = sizeof(struct ip6_hdr) + plen;
			m->m_pkthdr.len = sizeof(struct ip6_hdr) + plen;
		} else
			m_adj(m, sizeof(struct ip6_hdr) + plen -
			    m->m_pkthdr.len);
	}
	IP6STAT_INC(ip6s_nxthist[ip6->ip6_nxt]);
#ifdef ALTQ
	if (altq_input != NULL && (*altq_input)(m, AF_INET6) == 0) {
		/* packet is dropped by traffic conditioner */
		m = NULL;
		goto dropin;
	}
#endif
	/*
	 * Hop limit.
	 */
#ifdef IPSTEALTH
	if (!V_ip6stealth)
#endif
	if (ip6->ip6_hlim <= IPV6_HLIMDEC) {
		icmp6_error(m, ICMP6_TIME_EXCEEDED,
		    ICMP6_TIME_EXCEED_TRANSIT, 0);
		m = NULL;
		goto dropin;
	}
	/*
	 * Save at most ICMPV6_PLD_MAXLEN (= the min IPv6 MTU -
	 * size of IPv6 + ICMPv6 headers) bytes of the packet in case
	 * we need to generate an ICMP6 message to the src.
	 * Thanks to M_EXT, in most cases copy will not occur.
	 */
	mcopy = m_copy(m, 0, imin(m->m_pkthdr.len, ICMPV6_PLD_MAXLEN));
	/*
	 * Initialize route.
	 * First we plan to find route to ip6_dst.
	 */
	bzero(&ro, sizeof(ro));
	ro.ro_dst.sin6_len = sizeof(struct sockaddr_in6);
	ro.ro_dst.sin6_family = AF_INET6;
	ro.ro_dst.sin6_addr = ip6->ip6_dst;
	/*
	 * Incoming packet firewall processing.
	 */
	if (!PFIL_HOOKED(&V_inet6_pfil_hook))
		goto passin;
	if (pfil_run_hooks(&V_inet6_pfil_hook, &m, rcvif, PFIL_IN,
	    NULL) != 0 || m == NULL)
		goto dropin;
	/*
	 * A packet filter could change the destination address.
	 * XXX: Currently we don't have IPv6 NAT - skip these checks.
	 *
	 * If packet filter sets the M_FASTFWD_OURS flag, this means
	 * that new destination or next hop is our local address.
	 * So, we can just go back to netisr.
	 * XXX: should we decrement ip6_hlim in such case?
	 *
	 * Also it can forward packet to another destination, e.g.
	 * M_IP6_NEXTHOP flag is set and fwd_tag is attached to mbuf.
	 */
	if (m->m_flags & M_FASTFWD_OURS)
		goto freecopy;
	if ((m->m_flags & M_IP6_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		/*
		 * Now we will find route to forwarded by pfil destination.
		 * XXX: Address shouldn't be LLA.
		 */
		bcopy((fwd_tag + 1), &ro.ro_dst, sizeof(struct sockaddr_in6));
		m->m_flags &= ~M_IP6_NEXTHOP;
		m_tag_delete(m, fwd_tag);
	}
passin:
#ifdef IPSTEALTH
	if (!V_ip6stealth)
#endif
	ip6->ip6_hlim -= IPV6_HLIMDEC;
again:
	/*
	 * Find route to destination.
	 */
	in6_rtalloc(&ro, M_GETFIB(m));
	if (ro.ro_rt == NULL || (ro.ro_rt->rt_flags & RTF_REJECT)) {
		IP6STAT_INC(ip6s_noroute);
		in6_ifstat_inc(rcvif, ifs6_in_noroute);
		icmp6_error(mcopy, ICMP6_DST_UNREACH,
		    ICMP6_DST_UNREACH_NOROUTE, 0);
		RO_RTFREE(&ro);
		mcopy = NULL;
		goto dropin;
	}
	if (!(ro.ro_rt->rt_flags & RTF_UP)) {
		IP6STAT_INC(ip6s_noroute);
		in6_ifstat_inc(rcvif, ifs6_in_noroute);
		icmp6_error(mcopy, ICMP6_DST_UNREACH,
		    ICMP6_DST_UNREACH_ADDR, 0);
		RO_RTFREE(&ro);
		mcopy = NULL;
		goto dropin;
	}
	oif = ro.ro_rt->rt_ifp;
	/*
	 * We use slow path processing for packets with scoped addresses.
	 * So, scope checks aren't needed here.
	 */
	if (m->m_pkthdr.len > IN6_LINKMTU(oif)) {
		in6_ifstat_inc(oif, ifs6_in_toobig);
		icmp6_error(mcopy, ICMP6_PACKET_TOO_BIG, 0, IN6_LINKMTU(oif));
		RO_RTFREE(&ro);
		mcopy = NULL;
		goto dropout;
	}
	/*
	 * Save gateway's address in the ro_dst and free route.
	 */
	if (ro.ro_rt->rt_flags & RTF_GATEWAY)
		ro.ro_dst = *(struct sockaddr_in6 *)ro.ro_rt->rt_gateway;
	RO_RTFREE(&ro);
	/*
	 * Outgoing packet firewall processing.
	 */
	if (!PFIL_HOOKED(&V_inet6_pfil_hook))
		goto passout;
	if (pfil_run_hooks(&V_inet6_pfil_hook, &m, oif, PFIL_OUT,
	    NULL) != 0 || m == NULL)
		goto dropout;
	/*
	 * Again. A packet filter could change the destination address.
	 * XXX: Currently we don't have IPv6 NAT - skip these checks.
	 *
	 * If packet filter sets the M_FASTFWD_OURS flag, this means
	 * that new destination or next hop is our local address.
	 * So, we can just go back to netisr.
	 * XXX: Should we do something with checksums?
	 *
	 * Also it can forward packet to another destination, e.g.
	 * M_IP6_NEXTHOP flag is set and fwd_tag is attached to mbuf.
	 */
	if (m->m_flags & M_FASTFWD_OURS)
		goto freecopy;
	if ((m->m_flags & M_IP6_NEXTHOP) &&
	    (fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL)) != NULL) {
		bcopy((fwd_tag + 1), &ro.ro_dst, sizeof(struct sockaddr_in6));
		m->m_flags |= M_SKIP_FIREWALL;
		m->m_flags &= ~M_IP6_NEXTHOP;
		m_tag_delete(m, fwd_tag);
		goto again;
	}
passout:
	if (nd6_output(oif, rcvif, m, &ro.ro_dst, NULL) != 0) {
		in6_ifstat_inc(oif, ifs6_out_discard);
		IP6STAT_INC(ip6s_cantforward);
	} else {
		in6_ifstat_inc(oif, ifs6_out_forward);
		IP6STAT_INC(ip6s_forward);
	}
	m = NULL;
	goto freecopy;
dropin:
	in6_ifstat_inc(rcvif, ifs6_in_discard);
	goto drop;
dropout:
	in6_ifstat_inc(oif, ifs6_out_discard);
drop:
	if (m != NULL) {
		m_freem(m);
		m = NULL;
	}
freecopy:
	if (mcopy != NULL)
		m_freem(mcopy);
	/*
	 * Update statistics.
	 */
	if (mflags & M_EXT) {
		if (mnext)
			IP6STAT_INC(ip6s_mext2m);
		else
			IP6STAT_INC(ip6s_mext1);
	} else {
		if (mnext) {
			if (mflags & M_LOOP) {
				IP6STAT_INC(ip6s_m2m[V_loif->if_index]);
			} else if (rcvif->if_index < IP6S_M2MMAX)
				IP6STAT_INC(ip6s_m2m[rcvif->if_index]);
			else
				IP6STAT_INC(ip6s_m2m[0]);
		} else
			IP6STAT_INC(ip6s_m1);
	}
	in6_ifstat_inc(rcvif, ifs6_in_receive);
	IP6STAT_INC(ip6s_total);
	return (m);
}

