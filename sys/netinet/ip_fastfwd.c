/*-
 * Copyright (c) 2003 Andre Oppermann, Internet Business Solutions AG
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
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * ip_fastforward gets its speed from processing the forwarded packet to
 * completion (if_output on the other side) without any queues or netisr's.
 * The receiving interface DMAs the packet into memory, the upper half of
 * driver calls ip_fastforward, we do our routing table lookup and directly
 * send it off to the outgoing interface which DMAs the packet to the
 * network card. The only part of the packet we touch with the CPU is the
 * IP header (unless there are complex firewall rules touching other parts
 * of the packet, but that is up to you). We are essentially limited by bus
 * bandwidth and how fast the network card/driver can set up receives and
 * transmits.
 *
 * We handle basic errors, ip header errors, checksum errors,
 * destination unreachable, fragmentation and fragmentation needed and
 * report them via icmp to the sender.
 *
 * Else if something is not pure IPv4 unicast forwarding we fall back to
 * the normal ip_input processing path. We should only be called from
 * interfaces connected to the outside world.
 *
 * Firewalling is fully supported including divert, ipfw fwd and ipfilter
 * ipnat and address rewrite.
 *
 * IPSEC is not supported if this host is a tunnel broker. IPSEC is
 * supported for connections to/from local host.
 *
 * We try to do the least expensive (in CPU ops) checks and operations
 * first to catch junk with as little overhead as possible.
 * 
 * We take full advantage of hardware support for ip checksum and
 * fragmentation offloading.
 *
 * We don't do ICMP redirect in the fast forwarding path. I have had my own
 * cases where two core routers with Zebra routing suite would send millions
 * ICMP redirects to connected hosts if the router to dest was not the default
 * gateway. In one case it was filling the routing table of a host with close
 * 300'000 cloned redirect entries until it ran out of kernel memory. However
 * the networking code proved very robust and it didn't crash or went ill
 * otherwise.
 */

/*
 * Many thanks to Matt Thomas of NetBSD for basic structure of ip_flow.c which
 * is being followed here.
 */

#include "opt_ipfw.h"
#include "opt_ipstealth.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/pfil.h>
#include <net/if.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_icmp.h>

#include <machine/in_cksum.h>

static int ipfastforward_active = 0;
SYSCTL_INT(_net_inet_ip, OID_AUTO, fastforwarding, CTLFLAG_RW,
    &ipfastforward_active, 0, "Enable fast IP forwarding");

static struct sockaddr_in *
ip_findroute(struct route *ro, struct in_addr dest, struct mbuf *m)
{
	struct sockaddr_in *dst;
	struct rtentry *rt;

	/*
	 * Find route to destination.
	 */
	bzero(ro, sizeof(*ro));
	dst = (struct sockaddr_in *)&ro->ro_dst;
	dst->sin_family = AF_INET;
	dst->sin_len = sizeof(*dst);
	dst->sin_addr.s_addr = dest.s_addr;
	rtalloc_ign(ro, RTF_CLONING);

	/*
	 * Route there and interface still up?
	 */
	rt = ro->ro_rt;
	if (rt && (rt->rt_flags & RTF_UP) &&
	    (rt->rt_ifp->if_flags & IFF_UP) &&
	    (rt->rt_ifp->if_flags & IFF_RUNNING)) {
		if (rt->rt_flags & RTF_GATEWAY)
			dst = (struct sockaddr_in *)rt->rt_gateway;
	} else {
		ipstat.ips_noroute++;
		ipstat.ips_cantforward++;
		if (rt)
			RTFREE(rt);
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, NULL);
		return NULL;
	}
	return dst;
}

/*
 * Try to forward a packet based on the destination address.
 * This is a fast path optimized for the plain forwarding case.
 * If the packet is handled (and consumed) here then we return 1;
 * otherwise 0 is returned and the packet should be delivered
 * to ip_input for full processing.
 */
int
ip_fastforward(struct mbuf *m)
{
	struct ip *ip;
	struct mbuf *m0 = NULL;
	struct route ro;
	struct sockaddr_in *dst = NULL;
	struct in_ifaddr *ia = NULL;
	struct ifaddr *ifa = NULL;
	struct ifnet *ifp;
	struct in_addr odest, dest;
	u_short sum, ip_len;
	int error = 0;
	int hlen, mtu;
#ifdef IPFIREWALL_FORWARD
	struct m_tag *fwd_tag;
#endif

	/*
	 * Are we active and forwarding packets?
	 */
	if (!ipfastforward_active || !ipforwarding)
		return 0;

	M_ASSERTVALID(m);
	M_ASSERTPKTHDR(m);

	ro.ro_rt = NULL;

	/*
	 * Step 1: check for packet drop conditions (and sanity checks)
	 */

	/*
	 * Is entire packet big enough?
	 */
	if (m->m_pkthdr.len < sizeof(struct ip)) {
		ipstat.ips_tooshort++;
		goto drop;
	}

	/*
	 * Is first mbuf large enough for ip header and is header present?
	 */
	if (m->m_len < sizeof (struct ip) &&
	   (m = m_pullup(m, sizeof (struct ip))) == 0) {
		ipstat.ips_toosmall++;
		goto drop;
	}

	ip = mtod(m, struct ip *);

	/*
	 * Is it IPv4?
	 */
	if (ip->ip_v != IPVERSION) {
		ipstat.ips_badvers++;
		goto drop;
	}

	/*
	 * Is IP header length correct and is it in first mbuf?
	 */
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {	/* minimum header length */
		ipstat.ips_badlen++;
		goto drop;
	}
	if (hlen > m->m_len) {
		if ((m = m_pullup(m, hlen)) == 0) {
			ipstat.ips_badhlen++;
			goto drop;
		}
		ip = mtod(m, struct ip *);
	}

	/*
	 * Checksum correct?
	 */
	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED)
		sum = !(m->m_pkthdr.csum_flags & CSUM_IP_VALID);
	else {
		if (hlen == sizeof(struct ip))
			sum = in_cksum_hdr(ip);
		else
			sum = in_cksum(m, hlen);
	}
	if (sum) {
		ipstat.ips_badsum++;
		goto drop;
	}
	m->m_pkthdr.csum_flags |= (CSUM_IP_CHECKED | CSUM_IP_VALID);

	ip_len = ntohs(ip->ip_len);

	/*
	 * Is IP length longer than packet we have got?
	 */
	if (m->m_pkthdr.len < ip_len) {
		ipstat.ips_tooshort++;
		goto drop;
	}

	/*
	 * Is packet longer than IP header tells us? If yes, truncate packet.
	 */
	if (m->m_pkthdr.len > ip_len) {
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip_len;
			m->m_pkthdr.len = ip_len;
		} else
			m_adj(m, ip_len - m->m_pkthdr.len);
	}

	/*
	 * Is packet from or to 127/8?
	 */
	if ((ntohl(ip->ip_dst.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET ||
	    (ntohl(ip->ip_src.s_addr) >> IN_CLASSA_NSHIFT) == IN_LOOPBACKNET) {
		ipstat.ips_badaddr++;
		goto drop;
	}

#ifdef ALTQ
	/*
	 * Is packet dropped by traffic conditioner?
	 */
	if (altq_input != NULL && (*altq_input)(m, AF_INET) == 0)
		return 1;
#endif

	/*
	 * Step 2: fallback conditions to normal ip_input path processing
	 */

	/*
	 * Only IP packets without options
	 */
	if (ip->ip_hl != (sizeof(struct ip) >> 2)) {
		if (ip_doopts == 1)
			return 0;
		else if (ip_doopts == 2) {
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_FILTER_PROHIB,
				0, NULL);
			return 1;
		}
		/* else ignore IP options and continue */
	}

	/*
	 * Only unicast IP, not from loopback, no L2 or IP broadcast,
	 * no multicast, no INADDR_ANY
	 *
	 * XXX: Probably some of these checks could be direct drop
	 * conditions.  However it is not clear whether there are some
	 * hacks or obscure behaviours which make it neccessary to
	 * let ip_input handle it.  We play safe here and let ip_input
	 * deal with it until it is proven that we can directly drop it.
	 */
	if ((m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) ||
	    ntohl(ip->ip_src.s_addr) == (u_long)INADDR_BROADCAST ||
	    ntohl(ip->ip_dst.s_addr) == (u_long)INADDR_BROADCAST ||
	    IN_MULTICAST(ntohl(ip->ip_src.s_addr)) ||
	    IN_MULTICAST(ntohl(ip->ip_dst.s_addr)) ||
	    ip->ip_dst.s_addr == INADDR_ANY )
		return 0;

	/*
	 * Is it for a local address on this host?
	 */
	if (in_localip(ip->ip_dst))
		return 0;

	/*
	 * Or is it for a local IP broadcast address on this host?
	 */
	if ((m->m_flags & M_BCAST) &&
	    (m->m_pkthdr.rcvif->if_flags & IFF_BROADCAST)) {
	        TAILQ_FOREACH(ifa, &m->m_pkthdr.rcvif->if_addrhead, ifa_link) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			ia = ifatoia(ifa);
			if (ia->ia_netbroadcast.s_addr == ip->ip_dst.s_addr)
				return 0;
			if (satosin(&ia->ia_broadaddr)->sin_addr.s_addr ==
			    ip->ip_dst.s_addr)
				return 0;
		}
	}
	ipstat.ips_total++;

	/*
	 * Step 3: incoming packet firewall processing
	 */

	/*
	 * Convert to host representation
	 */
	ip->ip_len = ntohs(ip->ip_len);
	ip->ip_off = ntohs(ip->ip_off);

	odest.s_addr = dest.s_addr = ip->ip_dst.s_addr;

	/*
	 * Run through list of ipfilter hooks for input packets
	 */
	if (inet_pfil_hook.ph_busy_count == -1)
		goto passin;

	if (pfil_run_hooks(&inet_pfil_hook, &m, m->m_pkthdr.rcvif, PFIL_IN, NULL) ||
	    m == NULL)
		return 1;

	M_ASSERTVALID(m);
	M_ASSERTPKTHDR(m);

	ip = mtod(m, struct ip *);	/* m may have changed by pfil hook */
	dest.s_addr = ip->ip_dst.s_addr;

	/*
	 * Destination address changed?
	 */
	if (odest.s_addr != dest.s_addr) {
		/*
		 * Is it now for a local address on this host?
		 */
		if (in_localip(dest))
			goto forwardlocal;
		/*
		 * Go on with new destination address
		 */
	}
#ifdef IPFIREWALL_FORWARD
	if (m->m_flags & M_FASTFWD_OURS) {
		/*
		 * ipfw changed it for a local address on this host.
		 */
		goto forwardlocal;
	}
#endif /* IPFIREWALL_FORWARD */

passin:
	/*
	 * Step 4: decrement TTL and look up route
	 */

	/*
	 * Check TTL
	 */
#ifdef IPSTEALTH
	if (!ipstealth) {
#endif
	if (ip->ip_ttl <= IPTTLDEC) {
		icmp_error(m, ICMP_TIMXCEED, ICMP_TIMXCEED_INTRANS, 0, NULL);
		return 1;
	}

	/*
	 * Decrement the TTL and incrementally change the checksum.
	 * Don't bother doing this with hw checksum offloading.
	 */
	ip->ip_ttl -= IPTTLDEC;
	if (ip->ip_sum >= (u_int16_t) ~htons(IPTTLDEC << 8))
		ip->ip_sum -= ~htons(IPTTLDEC << 8);
	else
		ip->ip_sum += htons(IPTTLDEC << 8);
#ifdef IPSTEALTH
	}
#endif

	/*
	 * Find route to destination.
	 */
	if ((dst = ip_findroute(&ro, dest, m)) == NULL)
		return 1;	/* icmp unreach already sent */
	ifp = ro.ro_rt->rt_ifp;

	/*
	 * Step 5: outgoing firewall packet processing
	 */

	/*
	 * Run through list of hooks for output packets.
	 */
	if (inet_pfil_hook.ph_busy_count == -1)
		goto passout;

	if (pfil_run_hooks(&inet_pfil_hook, &m, ifp, PFIL_OUT, NULL) || m == NULL) {
		goto consumed;
	}

	M_ASSERTVALID(m);
	M_ASSERTPKTHDR(m);

	ip = mtod(m, struct ip *);
	dest.s_addr = ip->ip_dst.s_addr;

	/*
	 * Destination address changed?
	 */
#ifndef IPFIREWALL_FORWARD
	if (odest.s_addr != dest.s_addr) {
#else
	fwd_tag = m_tag_find(m, PACKET_TAG_IPFORWARD, NULL);
	if (odest.s_addr != dest.s_addr || fwd_tag != NULL) {
#endif /* IPFIREWALL_FORWARD */
		/*
		 * Is it now for a local address on this host?
		 */
#ifndef IPFIREWALL_FORWARD
		if (in_localip(dest)) {
#else
		if (in_localip(dest) || m->m_flags & M_FASTFWD_OURS) {
#endif /* IPFIREWALL_FORWARD */
forwardlocal:
			/*
			 * Return packet for processing by ip_input().
			 * Keep host byte order as expected at ip_input's
			 * "ours"-label.
			 */
			m->m_flags |= M_FASTFWD_OURS;
			if (ro.ro_rt)
				RTFREE(ro.ro_rt);
			return 0;
		}
		/*
		 * Redo route lookup with new destination address
		 */
#ifdef IPFIREWALL_FORWARD
		if (fwd_tag) {
			if (!in_localip(ip->ip_src) && !in_localaddr(ip->ip_dst))
				dest.s_addr = ((struct sockaddr_in *)(fwd_tag+1))->sin_addr.s_addr;
			m_tag_delete(m, fwd_tag);
		}
#endif /* IPFIREWALL_FORWARD */
		RTFREE(ro.ro_rt);
		if ((dst = ip_findroute(&ro, dest, m)) == NULL)
			return 1;	/* icmp unreach already sent */
		ifp = ro.ro_rt->rt_ifp;
	}

passout:
	/*
	 * Step 6: send off the packet
	 */

	/*
	 * Check if route is dampned (when ARP is unable to resolve)
	 */
	if ((ro.ro_rt->rt_flags & RTF_REJECT) &&
	    ro.ro_rt->rt_rmx.rmx_expire >= time_second) {
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, NULL);
		goto consumed;
	}

#ifndef ALTQ
	/*
	 * Check if there is enough space in the interface queue
	 */
	if ((ifp->if_snd.ifq_len + ip->ip_len / ifp->if_mtu + 1) >=
	    ifp->if_snd.ifq_maxlen) {
		ipstat.ips_odropped++;
		/* would send source quench here but that is depreciated */
		goto drop;
	}
#endif

	/*
	 * Check if media link state of interface is not down
	 */
	if (ifp->if_link_state == LINK_STATE_DOWN) {
		icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_HOST, 0, NULL);
		goto consumed;
	}

	/*
	 * Check if packet fits MTU or if hardware will fragement for us
	 */
	if (ro.ro_rt->rt_rmx.rmx_mtu)
		mtu = min(ro.ro_rt->rt_rmx.rmx_mtu, ifp->if_mtu);
	else
		mtu = ifp->if_mtu;

	if (ip->ip_len <= mtu ||
	    (ifp->if_hwassist & CSUM_FRAGMENT && (ip->ip_off & IP_DF) == 0)) {
		/*
		 * Restore packet header fields to original values
		 */
		ip->ip_len = htons(ip->ip_len);
		ip->ip_off = htons(ip->ip_off);
		/*
		 * Send off the packet via outgoing interface
		 */
		error = (*ifp->if_output)(ifp, m,
				(struct sockaddr *)dst, ro.ro_rt);
	} else {
		/*
		 * Handle EMSGSIZE with icmp reply needfrag for TCP MTU discovery
		 */
		if (ip->ip_off & IP_DF) {
			ipstat.ips_cantfrag++;
			icmp_error(m, ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG,
				0, ifp);
			goto consumed;
		} else {
			/*
			 * We have to fragement the packet
			 */
			m->m_pkthdr.csum_flags |= CSUM_IP;
			/*
			 * ip_fragment expects ip_len and ip_off in host byte
			 * order but returns all packets in network byte order
			 */
			if (ip_fragment(ip, &m, mtu, ifp->if_hwassist,
					(~ifp->if_hwassist & CSUM_DELAY_IP))) {
				goto drop;
			}
			KASSERT(m != NULL, ("null mbuf and no error"));
			/*
			 * Send off the fragments via outgoing interface
			 */
			error = 0;
			do {
				m0 = m->m_nextpkt;
				m->m_nextpkt = NULL;

				error = (*ifp->if_output)(ifp, m,
					(struct sockaddr *)dst, ro.ro_rt);
				if (error)
					break;
			} while ((m = m0) != NULL);
			if (error) {
				/* Reclaim remaining fragments */
				for (; m; m = m0) {
					m0 = m->m_nextpkt;
					m->m_nextpkt = NULL;
					m_freem(m);
				}
			} else
				ipstat.ips_fragmented++;
		}
	}

	if (error != 0)
		ipstat.ips_odropped++;
	else {
		ro.ro_rt->rt_rmx.rmx_pksent++;
		ipstat.ips_forward++;
		ipstat.ips_fastforward++;
	}
consumed:
	RTFREE(ro.ro_rt);
	return 1;
drop:
	if (m)
		m_freem(m);
	if (ro.ro_rt)
		RTFREE(ro.ro_rt);
	return 1;
}
