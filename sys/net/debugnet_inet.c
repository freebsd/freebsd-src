/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Isilon Systems, LLC.
 * Copyright (c) 2005-2014 Sandvine Incorporated. All rights reserved.
 * Copyright (c) 2000 Darrell Anderson
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/ip_options.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>

#include <machine/in_cksum.h>
#include <machine/pcb.h>

#include <net/debugnet.h>
#define	DEBUGNET_INTERNAL
#include <net/debugnet_int.h>

int debugnet_arp_nretries = 3;
SYSCTL_INT(_net_debugnet, OID_AUTO, arp_nretries, CTLFLAG_RWTUN,
    &debugnet_arp_nretries, 0,
    "Number of ARP attempts before giving up");

/*
 * Handler for IP packets: checks their sanity and then processes any debugnet
 * ACK packets it finds.
 *
 * It needs to partially replicate the behaviour of ip_input() and udp_input().
 *
 * Parameters:
 *	pcb	a pointer to the live debugnet PCB
 *	mb	a pointer to an mbuf * containing the packet received
 *		Updates *mb if m_pullup et al change the pointer
 *		Assumes the calling function will take care of freeing the mbuf
 */
void
debugnet_handle_ip(struct debugnet_pcb *pcb, struct mbuf **mb)
{
	struct ip *ip;
	struct mbuf *m;
	unsigned short hlen;

	if (pcb->dp_state < DN_STATE_HAVE_GW_MAC)
		return;

	/* IP processing. */
	m = *mb;
	if (m->m_pkthdr.len < sizeof(struct ip)) {
		DNETDEBUG("dropping packet too small for IP header\n");
		return;
	}
	if (m->m_len < sizeof(struct ip)) {
		m = m_pullup(m, sizeof(struct ip));
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("m_pullup failed\n");
			return;
		}
	}
	ip = mtod(m, struct ip *);

	/* IP version. */
	if (ip->ip_v != IPVERSION) {
		DNETDEBUG("bad IP version %d\n", ip->ip_v);
		return;
	}

	/* Header length. */
	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip)) {
		DNETDEBUG("bad IP header length (%hu)\n", hlen);
		return;
	}
	if (hlen > m->m_len) {
		m = m_pullup(m, hlen);
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("m_pullup failed\n");
			return;
		}
		ip = mtod(m, struct ip *);
	}
	/* Ignore packets with IP options. */
	if (hlen > sizeof(struct ip)) {
		DNETDEBUG("drop packet with IP options\n");
		return;
	}

#ifdef INVARIANTS
	if ((IN_LOOPBACK(ntohl(ip->ip_dst.s_addr)) ||
	    IN_LOOPBACK(ntohl(ip->ip_src.s_addr))) &&
	    (m->m_pkthdr.rcvif->if_flags & IFF_LOOPBACK) == 0) {
		DNETDEBUG("Bad IP header (RFC1122)\n");
		return;
	}
#endif

	/* Checksum. */
	if ((m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) != 0) {
		if ((m->m_pkthdr.csum_flags & CSUM_IP_VALID) == 0) {
			DNETDEBUG("bad IP checksum\n");
			return;
		}
	} else {
		/* XXX */ ;
	}

	/* Convert fields to host byte order. */
	ip->ip_len = ntohs(ip->ip_len);
	if (ip->ip_len < hlen) {
		DNETDEBUG("IP packet smaller (%hu) than header (%hu)\n",
		    ip->ip_len, hlen);
		return;
	}
	if (m->m_pkthdr.len < ip->ip_len) {
		DNETDEBUG("IP packet bigger (%hu) than ethernet packet (%d)\n",
		    ip->ip_len, m->m_pkthdr.len);
		return;
	}
	if (m->m_pkthdr.len > ip->ip_len) {
		/* Truncate the packet to the IP length. */
		if (m->m_len == m->m_pkthdr.len) {
			m->m_len = ip->ip_len;
			m->m_pkthdr.len = ip->ip_len;
		} else
			m_adj(m, ip->ip_len - m->m_pkthdr.len);
	}

	ip->ip_off = ntohs(ip->ip_off);

	/* Check that the source is the server's IP. */
	if (ip->ip_src.s_addr != pcb->dp_server) {
		DNETDEBUG("drop packet not from server (from 0x%x)\n",
		    ip->ip_src.s_addr);
		return;
	}

	/* Check if the destination IP is ours. */
	if (ip->ip_dst.s_addr != pcb->dp_client) {
		DNETDEBUGV("drop packet not to our IP\n");
		return;
	}

	if (ip->ip_p != IPPROTO_UDP) {
		DNETDEBUG("drop non-UDP packet\n");
		return;
	}

	/* Do not deal with fragments. */
	if ((ip->ip_off & (IP_MF | IP_OFFMASK)) != 0) {
		DNETDEBUG("drop fragmented packet\n");
		return;
	}

	if ((m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) != 0) {
		if ((m->m_pkthdr.csum_flags & CSUM_DATA_VALID) == 0) {
			DNETDEBUG("bad UDP checksum\n");
			return;
		}
	} else {
		/* XXX */ ;
	}

	/* UDP custom is to have packet length not include IP header. */
	ip->ip_len -= hlen;

	/* Checked above before decoding IP header. */
	MPASS(m->m_pkthdr.len >= sizeof(struct ipovly));

	/* Put the UDP header at start of chain. */
	m_adj(m, sizeof(struct ipovly));
	debugnet_handle_udp(pcb, mb);
}

/*
 * Builds and sends a single ARP request to locate the L2 address for a given
 * INET address.
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
static int
debugnet_send_arp(struct debugnet_pcb *pcb, in_addr_t dst)
{
	struct ether_addr bcast;
	struct arphdr *ah;
	struct ifnet *ifp;
	struct mbuf *m;
	int pktlen;

	ifp = pcb->dp_ifp;

	/* Fill-up a broadcast address. */
	memset(&bcast, 0xFF, ETHER_ADDR_LEN);
	m = m_gethdr(M_NOWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: Out of mbufs\n", __func__);
		return (ENOBUFS);
	}
	pktlen = arphdr_len2(ETHER_ADDR_LEN, sizeof(struct in_addr));
	m->m_len = pktlen;
	m->m_pkthdr.len = pktlen;
	MH_ALIGN(m, pktlen);
	ah = mtod(m, struct arphdr *);
	ah->ar_hrd = htons(ARPHRD_ETHER);
	ah->ar_pro = htons(ETHERTYPE_IP);
	ah->ar_hln = ETHER_ADDR_LEN;
	ah->ar_pln = sizeof(struct in_addr);
	ah->ar_op = htons(ARPOP_REQUEST);
	memcpy(ar_sha(ah), IF_LLADDR(ifp), ETHER_ADDR_LEN);
	((struct in_addr *)ar_spa(ah))->s_addr = pcb->dp_client;
	bzero(ar_tha(ah), ETHER_ADDR_LEN);
	((struct in_addr *)ar_tpa(ah))->s_addr = dst;
	return (debugnet_ether_output(m, ifp, bcast, ETHERTYPE_ARP));
}

/*
 * Handler for ARP packets: checks their sanity and then
 * 1. If the ARP is a request for our IP, respond with our MAC address
 * 2. If the ARP is a response from our server, record its MAC address
 *
 * It needs to replicate partially the behaviour of arpintr() and
 * in_arpinput().
 *
 * Parameters:
 *	pcb	a pointer to the live debugnet PCB
 *	mb	a pointer to an mbuf * containing the packet received
 *		Updates *mb if m_pullup et al change the pointer
 *		Assumes the calling function will take care of freeing the mbuf
 */
void
debugnet_handle_arp(struct debugnet_pcb *pcb, struct mbuf **mb)
{
	char buf[INET_ADDRSTRLEN];
	struct in_addr isaddr, itaddr;
	struct ether_addr dst;
	struct mbuf *m;
	struct arphdr *ah;
	struct ifnet *ifp;
	uint8_t *enaddr;
	int req_len, op;

	m = *mb;
	ifp = m->m_pkthdr.rcvif;
	if (m->m_len < sizeof(struct arphdr)) {
		m = m_pullup(m, sizeof(struct arphdr));
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("runt packet: m_pullup failed\n");
			return;
		}
	}

	ah = mtod(m, struct arphdr *);
	if (ntohs(ah->ar_hrd) != ARPHRD_ETHER) {
		DNETDEBUG("unknown hardware address 0x%2D)\n",
		    (unsigned char *)&ah->ar_hrd, "");
		return;
	}
	if (ntohs(ah->ar_pro) != ETHERTYPE_IP) {
		DNETDEBUG("drop ARP for unknown protocol %d\n",
		    ntohs(ah->ar_pro));
		return;
	}
	req_len = arphdr_len2(ifp->if_addrlen, sizeof(struct in_addr));
	if (m->m_len < req_len) {
		m = m_pullup(m, req_len);
		*mb = m;
		if (m == NULL) {
			DNETDEBUG("runt packet: m_pullup failed\n");
			return;
		}
	}
	ah = mtod(m, struct arphdr *);

	op = ntohs(ah->ar_op);
	memcpy(&isaddr, ar_spa(ah), sizeof(isaddr));
	memcpy(&itaddr, ar_tpa(ah), sizeof(itaddr));
	enaddr = (uint8_t *)IF_LLADDR(ifp);

	if (memcmp(ar_sha(ah), enaddr, ifp->if_addrlen) == 0) {
		DNETDEBUG("ignoring ARP from myself\n");
		return;
	}

	if (isaddr.s_addr == pcb->dp_client) {
		printf("%s: %*D is using my IP address %s!\n", __func__,
		    ifp->if_addrlen, (u_char *)ar_sha(ah), ":",
		    inet_ntoa_r(isaddr, buf));
		return;
	}

	if (memcmp(ar_sha(ah), ifp->if_broadcastaddr, ifp->if_addrlen) == 0) {
		DNETDEBUG("ignoring ARP from broadcast address\n");
		return;
	}

	if (op == ARPOP_REPLY) {
		if (isaddr.s_addr != pcb->dp_gateway &&
		    isaddr.s_addr != pcb->dp_server) {
			inet_ntoa_r(isaddr, buf);
			DNETDEBUG("ignoring ARP reply from %s (not configured"
			    " server or gateway)\n", buf);
			return;
		}
		if (pcb->dp_state >= DN_STATE_HAVE_GW_MAC) {
			inet_ntoa_r(isaddr, buf);
			DNETDEBUG("ignoring server ARP reply from %s (already"
			    " have gateway address)\n", buf);
			return;
		}
		MPASS(pcb->dp_state == DN_STATE_INIT);
		memcpy(pcb->dp_gw_mac.octet, ar_sha(ah),
		    min(ah->ar_hln, ETHER_ADDR_LEN));
		
		DNETDEBUG("got server MAC address %6D\n",
		    pcb->dp_gw_mac.octet, ":");

		pcb->dp_state = DN_STATE_HAVE_GW_MAC;
		return;
	}

	if (op != ARPOP_REQUEST) {
		DNETDEBUG("ignoring ARP non-request/reply\n");
		return;
	}

	if (itaddr.s_addr != pcb->dp_client) {
		DNETDEBUG("ignoring ARP not to our IP\n");
		return;
	}

	memcpy(ar_tha(ah), ar_sha(ah), ah->ar_hln);
	memcpy(ar_sha(ah), enaddr, ah->ar_hln);
	memcpy(ar_tpa(ah), ar_spa(ah), ah->ar_pln);
	memcpy(ar_spa(ah), &itaddr, ah->ar_pln);
	ah->ar_op = htons(ARPOP_REPLY);
	ah->ar_pro = htons(ETHERTYPE_IP);
	m->m_flags &= ~(M_BCAST|M_MCAST);
	m->m_len = arphdr_len(ah);
	m->m_pkthdr.len = m->m_len;

	memcpy(dst.octet, ar_tha(ah), ETHER_ADDR_LEN);
	debugnet_ether_output(m, ifp, dst, ETHERTYPE_ARP);
	*mb = NULL;
}

/*
 * Sends ARP requests to locate the server and waits for a response.
 * We first try to ARP the server itself, and fall back to the provided
 * gateway if the server appears to be off-link.
 *
 * Return value:
 *	0 on success
 *	errno on error
 */
int
debugnet_arp_gw(struct debugnet_pcb *pcb)
{
	in_addr_t dst;
	int error, polls, retries;

	dst = pcb->dp_server;
restart:
	for (retries = 0; retries < debugnet_arp_nretries; retries++) {
		error = debugnet_send_arp(pcb, dst);
		if (error != 0)
			return (error);
		for (polls = 0; polls < debugnet_npolls &&
		    pcb->dp_state < DN_STATE_HAVE_GW_MAC; polls++) {
			debugnet_network_poll(pcb);
			DELAY(500);
		}
		if (pcb->dp_state >= DN_STATE_HAVE_GW_MAC)
			break;
		printf("(ARP retry)");
	}
	if (pcb->dp_state >= DN_STATE_HAVE_GW_MAC)
		return (0);
	if (dst == pcb->dp_server) {
		printf("\nFailed to ARP server");
		if (pcb->dp_gateway != INADDR_ANY) {
			printf(", trying to reach gateway...\n");
			dst = pcb->dp_gateway;
			goto restart;
		} else
			printf(".\n");
	} else
		printf("\nFailed to ARP gateway.\n");

	return (ETIMEDOUT);
}

/*
 * Unreliable IPv4 transmission of an mbuf chain to the debugnet server
 * Note: can't handle fragmentation; fails if the packet is larger than
 *	 ifp->if_mtu after adding the UDP/IP headers
 *
 * Parameters:
 *	pcb	The debugnet context block
 *	m	mbuf chain
 *
 * Returns:
 *	int	see errno.h, 0 for success
 */
int
debugnet_ip_output(struct debugnet_pcb *pcb, struct mbuf *m)
{
	struct udphdr *udp;
	struct ifnet *ifp;
	struct ip *ip;

	MPASS(pcb->dp_state >= DN_STATE_HAVE_GW_MAC);

	ifp = pcb->dp_ifp;

	M_PREPEND(m, sizeof(*ip), M_NOWAIT);
	if (m == NULL) {
		printf("%s: out of mbufs\n", __func__);
		return (ENOBUFS);
	}

	if (m->m_pkthdr.len > ifp->if_mtu) {
		printf("%s: Packet is too big: %d > MTU %u\n", __func__,
		    m->m_pkthdr.len, ifp->if_mtu);
		m_freem(m);
		return (ENOBUFS);
	}

	ip = mtod(m, void *);
	udp = (void *)(ip + 1);

	memset(ip, 0, offsetof(struct ip, ip_p));
	ip->ip_p = IPPROTO_UDP;
	ip->ip_sum = udp->uh_ulen;
	ip->ip_src = (struct in_addr) { pcb->dp_client };
	ip->ip_dst = (struct in_addr) { pcb->dp_server };

	/* Compute UDP-IPv4 checksum. */
	udp->uh_sum = in_cksum(m, m->m_pkthdr.len);
	if (udp->uh_sum == 0)
		udp->uh_sum = 0xffff;

	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(*ip) >> 2;
	ip->ip_tos = 0;
	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_id = 0;
	ip->ip_off = htons(IP_DF);
	ip->ip_ttl = 255;
	ip->ip_sum = 0;
	ip->ip_sum = in_cksum(m, sizeof(struct ip));

	return (debugnet_ether_output(m, ifp, pcb->dp_gw_mac, ETHERTYPE_IP));
}
