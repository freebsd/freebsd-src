/*	$OpenBSD: if_trunk.c,v 1.30 2007/01/31 06:20:19 reyk Exp $	*/

/*
 * Copyright (c) 2005, 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2007 Andrew Thompson <thompsa@FreeBSD.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/cdefs.h>
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/fnv_hash.h>

#include <net/ethernet.h>
#include <net/infiniband.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#endif

#ifdef INET
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#endif

static const void *
m_common_hash_gethdr(const struct mbuf *m, const u_int off,
    const u_int len, void *buf)
{

	if (m->m_pkthdr.len < (off + len)) {
		return (NULL);
	} else if (m->m_len < (off + len)) {
		m_copydata(m, off, len, buf);
		return (buf);
	}
	return (mtod(m, char *) + off);
}

uint32_t
m_ether_tcpip_hash_init(void)
{
	uint32_t seed;

	seed = arc4random();
	return (fnv_32_buf(&seed, sizeof(seed), FNV1_32_INIT));
}

uint32_t
m_infiniband_tcpip_hash_init(void)
{
	uint32_t seed;

	seed = arc4random();
	return (fnv_32_buf(&seed, sizeof(seed), FNV1_32_INIT));
}

static inline uint32_t
m_tcpip_hash(const uint32_t flags, const struct mbuf *m,
    uint32_t p, int off, const uint16_t etype)
{
#if defined(INET) || defined(INET6)
	union {
#ifdef INET
		struct ip ip;
#endif
#ifdef INET6
		struct ip6_hdr ip6;
#endif
		uint32_t port;
	} buf;
#ifdef INET
	const struct ip *ip;
#endif
#ifdef INET6
	const struct ip6_hdr *ip6;
#endif
#endif
	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = m_common_hash_gethdr(m, off, sizeof(*ip), &buf);
		if (ip == NULL)
			break;
		if (flags & MBUF_HASHFLAG_L3) {
			p = fnv_32_buf(&ip->ip_src, sizeof(struct in_addr), p);
			p = fnv_32_buf(&ip->ip_dst, sizeof(struct in_addr), p);
		}
		if (flags & MBUF_HASHFLAG_L4) {
			const uint32_t *ports;
			int iphlen;

			switch (ip->ip_p) {
			case IPPROTO_TCP:
			case IPPROTO_UDP:
			case IPPROTO_SCTP:
				iphlen = ip->ip_hl << 2;
				if (iphlen < sizeof(*ip))
					break;
				off += iphlen;
				ports = m_common_hash_gethdr(m,
				    off, sizeof(*ports), &buf);
				if (ports == NULL)
					break;
				p = fnv_32_buf(ports, sizeof(*ports), p);
				break;
			default:
				break;
			}
		}
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = m_common_hash_gethdr(m, off, sizeof(*ip6), &buf);
		if (ip6 == NULL)
			break;
		if (flags & MBUF_HASHFLAG_L3) {
			p = fnv_32_buf(&ip6->ip6_src, sizeof(struct in6_addr), p);
			p = fnv_32_buf(&ip6->ip6_dst, sizeof(struct in6_addr), p);
		}
		if (flags & MBUF_HASHFLAG_L4) {
			uint32_t flow;

			/* IPv6 flow label */
			flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
			p = fnv_32_buf(&flow, sizeof(flow), p);
		}
		break;
#endif
	default:
		break;
	}
	return (p);
}

uint32_t
m_ether_tcpip_hash(const uint32_t flags, const struct mbuf *m,
    uint32_t p)
{
	union {
		struct ether_vlan_header vlan;
	} buf;
	const struct ether_header *eh;
	const struct ether_vlan_header *vlan;
	int off;
	uint16_t etype;

	off = sizeof(*eh);
	if (m->m_len < off)
		return (p);
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (flags & MBUF_HASHFLAG_L2) {
		p = fnv_32_buf(&eh->ether_shost, ETHER_ADDR_LEN, p);
		p = fnv_32_buf(&eh->ether_dhost, ETHER_ADDR_LEN, p);
	}
	/* Special handling for encapsulating VLAN frames */
	if ((m->m_flags & M_VLANTAG) && (flags & MBUF_HASHFLAG_L2)) {
		p = fnv_32_buf(&m->m_pkthdr.ether_vtag,
		    sizeof(m->m_pkthdr.ether_vtag), p);
	} else if (etype == ETHERTYPE_VLAN) {
		vlan = m_common_hash_gethdr(m, off, sizeof(*vlan), &buf);
		if (vlan == NULL)
			return (p);

		if (flags & MBUF_HASHFLAG_L2)
			p = fnv_32_buf(&vlan->evl_tag, sizeof(vlan->evl_tag), p);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}
	return (m_tcpip_hash(flags, m, p, off, etype));
}

uint32_t
m_infiniband_tcpip_hash(const uint32_t flags, const struct mbuf *m,
    uint32_t p)
{
	const struct infiniband_header *ibh;
	int off;
	uint16_t etype;

	off = sizeof(*ibh);
	if (m->m_len < off)
		return (p);
	ibh = mtod(m, struct infiniband_header *);
	etype = ntohs(ibh->ib_protocol);
	if (flags & MBUF_HASHFLAG_L2)
		p = fnv_32_buf(&ibh->ib_hwaddr, INFINIBAND_ADDR_LEN, p);

	return (m_tcpip_hash(flags, m, p, off, etype));
}
