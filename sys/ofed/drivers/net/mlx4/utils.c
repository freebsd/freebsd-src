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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/module.h>
#include <sys/priv.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/hash.h>
#include <sys/lock.h>
#include <sys/rmlock.h>
#include <sys/taskqueue.h>
#include <sys/eventhandler.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_clone.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_llc.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>
#include <net/bpf.h>

#if defined(INET) || defined(INET6)
#include <netinet/in.h>
#endif
#ifdef INET
#include <netinet/in_systm.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#endif

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/in6_var.h>
#include <netinet6/in6_ifattach.h>
#endif

#include <net/if_vlan_var.h>

#include "utils.h"

/* XXX this code should be factored out */
/* XXX copied from if_lagg.c */

static const void *
mlx4_en_gethdr(struct mbuf *m, u_int off, u_int len, void *buf)
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
mlx4_en_hashmbuf(uint32_t flags, struct mbuf *m, uint32_t key)
{
	uint16_t etype;
	uint32_t p = key;
	int off;
	struct ether_header *eh;
	const struct ether_vlan_header *vlan;
#ifdef INET
	const struct ip *ip;
	const uint32_t *ports;
	int iphlen;
#endif
#ifdef INET6
	const struct ip6_hdr *ip6;
	uint32_t flow;
#endif
	union {
#ifdef INET
		struct ip ip;
#endif
#ifdef INET6
		struct ip6_hdr ip6;
#endif
		struct ether_vlan_header vlan;
		uint32_t port;
	} buf;


	off = sizeof(*eh);
	if (m->m_len < off)
		goto out;
	eh = mtod(m, struct ether_header *);
	etype = ntohs(eh->ether_type);
	if (flags & MLX4_F_HASHL2) {
		p = hash32_buf(&eh->ether_shost, ETHER_ADDR_LEN, p);
		p = hash32_buf(&eh->ether_dhost, ETHER_ADDR_LEN, p);
	}

	/* Special handling for encapsulating VLAN frames */
	if ((m->m_flags & M_VLANTAG) && (flags & MLX4_F_HASHL2)) {
		p = hash32_buf(&m->m_pkthdr.ether_vtag,
		    sizeof(m->m_pkthdr.ether_vtag), p);
	} else if (etype == ETHERTYPE_VLAN) {
		vlan = mlx4_en_gethdr(m, off,  sizeof(*vlan), &buf);
		if (vlan == NULL)
			goto out;

		if (flags & MLX4_F_HASHL2)
			p = hash32_buf(&vlan->evl_tag, sizeof(vlan->evl_tag), p);
		etype = ntohs(vlan->evl_proto);
		off += sizeof(*vlan) - sizeof(*eh);
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = mlx4_en_gethdr(m, off, sizeof(*ip), &buf);
		if (ip == NULL)
			goto out;

		if (flags & MLX4_F_HASHL3) {
			p = hash32_buf(&ip->ip_src, sizeof(struct in_addr), p);
			p = hash32_buf(&ip->ip_dst, sizeof(struct in_addr), p);
		}
		if (!(flags & MLX4_F_HASHL4))
			break;
		switch (ip->ip_p) {
			case IPPROTO_TCP:
			case IPPROTO_UDP:
			case IPPROTO_SCTP:
				iphlen = ip->ip_hl << 2;
				if (iphlen < sizeof(*ip))
					break;
				off += iphlen;
				ports = mlx4_en_gethdr(m, off, sizeof(*ports), &buf);
				if (ports == NULL)
					break;
				p = hash32_buf(ports, sizeof(*ports), p);
				break;
		}
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		if (!(flags & MLX4_F_HASHL3))
			break;
		ip6 = mlx4_en_gethdr(m, off, sizeof(*ip6), &buf);
		if (ip6 == NULL)
			goto out;

		p = hash32_buf(&ip6->ip6_src, sizeof(struct in6_addr), p);
		p = hash32_buf(&ip6->ip6_dst, sizeof(struct in6_addr), p);
		flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
		p = hash32_buf(&flow, sizeof(flow), p);	/* IPv6 flow label */
		break;
#endif
	}
out:
	return (p);
}
