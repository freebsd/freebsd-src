/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2007, Myricom Inc.
 * Copyright (c) 2008, Intel Corporation.
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2016-2021 Mellanox Technologies.
 * All rights reserved.
 *
 * Portions of this software were developed by Bjoern Zeeb
 * under sponsorship from the FreeBSD Foundation.
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
#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/vnet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_private.h>
#include <net/if_types.h>
#include <net/infiniband.h>
#include <net/if_lagg.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_lro.h>
#include <netinet/tcp_var.h>
#include <netinet/tcpip.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_log_buf.h>
#include <netinet/tcp_fsm.h>
#include <netinet/udp.h>
#include <netinet6/ip6_var.h>

#include <machine/in_cksum.h>

static MALLOC_DEFINE(M_LRO, "LRO", "LRO control structures");

static void	tcp_lro_rx_done(struct lro_ctrl *lc);
static int	tcp_lro_rx_common(struct lro_ctrl *lc, struct mbuf *m,
		    uint32_t csum, bool use_hash);
static void	tcp_lro_flush(struct lro_ctrl *lc, struct lro_entry *le);

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, lro,  CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP LRO");

long tcplro_stacks_wanting_mbufq;
int	(*tcp_lro_flush_tcphpts)(struct lro_ctrl *lc, struct lro_entry *le);

counter_u64_t tcp_inp_lro_direct_queue;
counter_u64_t tcp_inp_lro_wokeup_queue;
counter_u64_t tcp_inp_lro_compressed;
counter_u64_t tcp_inp_lro_locks_taken;
counter_u64_t tcp_extra_mbuf;
counter_u64_t tcp_would_have_but;
counter_u64_t tcp_comp_total;
counter_u64_t tcp_uncomp_total;
counter_u64_t tcp_bad_csums;

static unsigned	tcp_lro_entries = TCP_LRO_ENTRIES;
SYSCTL_UINT(_net_inet_tcp_lro, OID_AUTO, entries,
    CTLFLAG_RDTUN | CTLFLAG_MPSAFE, &tcp_lro_entries, 0,
    "default number of LRO entries");

static uint32_t tcp_lro_cpu_set_thresh = TCP_LRO_CPU_DECLARATION_THRESH;
SYSCTL_UINT(_net_inet_tcp_lro, OID_AUTO, lro_cpu_threshold,
    CTLFLAG_RDTUN | CTLFLAG_MPSAFE, &tcp_lro_cpu_set_thresh, 0,
    "Number of interrupts in a row on the same CPU that will make us declare an 'affinity' cpu?");

static uint32_t tcp_less_accurate_lro_ts = 0;
SYSCTL_UINT(_net_inet_tcp_lro, OID_AUTO, lro_less_accurate,
    CTLFLAG_MPSAFE, &tcp_less_accurate_lro_ts, 0,
    "Do we trade off efficency by doing less timestamp operations for time accuracy?");

SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, fullqueue, CTLFLAG_RD,
    &tcp_inp_lro_direct_queue, "Number of lro's fully queued to transport");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, wokeup, CTLFLAG_RD,
    &tcp_inp_lro_wokeup_queue, "Number of lro's where we woke up transport via hpts");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, compressed, CTLFLAG_RD,
    &tcp_inp_lro_compressed, "Number of lro's compressed and sent to transport");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, lockcnt, CTLFLAG_RD,
    &tcp_inp_lro_locks_taken, "Number of lro's inp_wlocks taken");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, extra_mbuf, CTLFLAG_RD,
    &tcp_extra_mbuf, "Number of times we had an extra compressed ack dropped into the tp");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, would_have_but, CTLFLAG_RD,
    &tcp_would_have_but, "Number of times we would have had an extra compressed, but mget failed");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, with_m_ackcmp, CTLFLAG_RD,
    &tcp_comp_total, "Number of mbufs queued with M_ACKCMP flags set");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, without_m_ackcmp, CTLFLAG_RD,
    &tcp_uncomp_total, "Number of mbufs queued without M_ACKCMP");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, lro_badcsum, CTLFLAG_RD,
    &tcp_bad_csums, "Number of packets that the common code saw with bad csums");

void
tcp_lro_reg_mbufq(void)
{
	atomic_fetchadd_long(&tcplro_stacks_wanting_mbufq, 1);
}

void
tcp_lro_dereg_mbufq(void)
{
	atomic_fetchadd_long(&tcplro_stacks_wanting_mbufq, -1);
}

static __inline void
tcp_lro_active_insert(struct lro_ctrl *lc, struct lro_head *bucket,
    struct lro_entry *le)
{

	LIST_INSERT_HEAD(&lc->lro_active, le, next);
	LIST_INSERT_HEAD(bucket, le, hash_next);
}

static __inline void
tcp_lro_active_remove(struct lro_entry *le)
{

	LIST_REMOVE(le, next);		/* active list */
	LIST_REMOVE(le, hash_next);	/* hash bucket */
}

int
tcp_lro_init(struct lro_ctrl *lc)
{
	return (tcp_lro_init_args(lc, NULL, tcp_lro_entries, 0));
}

int
tcp_lro_init_args(struct lro_ctrl *lc, struct ifnet *ifp,
    unsigned lro_entries, unsigned lro_mbufs)
{
	struct lro_entry *le;
	size_t size;
	unsigned i;

	lc->lro_bad_csum = 0;
	lc->lro_queued = 0;
	lc->lro_flushed = 0;
	lc->lro_mbuf_count = 0;
	lc->lro_mbuf_max = lro_mbufs;
	lc->lro_cnt = lro_entries;
	lc->lro_ackcnt_lim = TCP_LRO_ACKCNT_MAX;
	lc->lro_length_lim = TCP_LRO_LENGTH_MAX;
	lc->ifp = ifp;
	LIST_INIT(&lc->lro_free);
	LIST_INIT(&lc->lro_active);

	/* create hash table to accelerate entry lookup */
	lc->lro_hash = phashinit_flags(lro_entries, M_LRO, &lc->lro_hashsz,
	    HASH_NOWAIT);
	if (lc->lro_hash == NULL) {
		memset(lc, 0, sizeof(*lc));
		return (ENOMEM);
	}

	/* compute size to allocate */
	size = (lro_mbufs * sizeof(struct lro_mbuf_sort)) +
	    (lro_entries * sizeof(*le));
	lc->lro_mbuf_data = (struct lro_mbuf_sort *)
	    malloc(size, M_LRO, M_NOWAIT | M_ZERO);

	/* check for out of memory */
	if (lc->lro_mbuf_data == NULL) {
		free(lc->lro_hash, M_LRO);
		memset(lc, 0, sizeof(*lc));
		return (ENOMEM);
	}
	/* compute offset for LRO entries */
	le = (struct lro_entry *)
	    (lc->lro_mbuf_data + lro_mbufs);

	/* setup linked list */
	for (i = 0; i != lro_entries; i++)
		LIST_INSERT_HEAD(&lc->lro_free, le + i, next);

	return (0);
}

struct vxlan_header {
	uint32_t	vxlh_flags;
	uint32_t	vxlh_vni;
};

static inline void *
tcp_lro_low_level_parser(void *ptr, struct lro_parser *parser, bool update_data, bool is_vxlan, int mlen)
{
	const struct ether_vlan_header *eh;
	void *old;
	uint16_t eth_type;

	if (update_data)
		memset(parser, 0, sizeof(*parser));

	old = ptr;

	if (is_vxlan) {
		const struct vxlan_header *vxh;
		vxh = ptr;
		ptr = (uint8_t *)ptr + sizeof(*vxh);
		if (update_data) {
			parser->data.vxlan_vni =
			    vxh->vxlh_vni & htonl(0xffffff00);
		}
	}

	eh = ptr;
	if (__predict_false(eh->evl_encap_proto == htons(ETHERTYPE_VLAN))) {
		eth_type = eh->evl_proto;
		if (update_data) {
			/* strip priority and keep VLAN ID only */
			parser->data.vlan_id = eh->evl_tag & htons(EVL_VLID_MASK);
		}
		/* advance to next header */
		ptr = (uint8_t *)ptr + ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
		mlen -= (ETHER_HDR_LEN  + ETHER_VLAN_ENCAP_LEN);
	} else {
		eth_type = eh->evl_encap_proto;
		/* advance to next header */
		mlen -= ETHER_HDR_LEN;
		ptr = (uint8_t *)ptr + ETHER_HDR_LEN;
	}
	if (__predict_false(mlen <= 0))
		return (NULL);
	switch (eth_type) {
#ifdef INET
	case htons(ETHERTYPE_IP):
		parser->ip4 = ptr;
		if (__predict_false(mlen < sizeof(struct ip)))
			return (NULL);
		/* Ensure there are no IPv4 options. */
		if ((parser->ip4->ip_hl << 2) != sizeof (*parser->ip4))
			break;
		/* .. and the packet is not fragmented. */
		if (parser->ip4->ip_off & htons(IP_MF|IP_OFFMASK))
			break;
		/* .. and the packet has valid src/dst addrs */
		if (__predict_false(parser->ip4->ip_src.s_addr == INADDR_ANY ||
			parser->ip4->ip_dst.s_addr == INADDR_ANY))
			break;
		ptr = (uint8_t *)ptr + (parser->ip4->ip_hl << 2);
		mlen -= sizeof(struct ip);
		if (update_data) {
			parser->data.s_addr.v4 = parser->ip4->ip_src;
			parser->data.d_addr.v4 = parser->ip4->ip_dst;
		}
		switch (parser->ip4->ip_p) {
		case IPPROTO_UDP:
			if (__predict_false(mlen < sizeof(struct udphdr)))
				return (NULL);
			parser->udp = ptr;
			if (update_data) {
				parser->data.lro_type = LRO_TYPE_IPV4_UDP;
				parser->data.s_port = parser->udp->uh_sport;
				parser->data.d_port = parser->udp->uh_dport;
			} else {
				MPASS(parser->data.lro_type == LRO_TYPE_IPV4_UDP);
			}
			ptr = ((uint8_t *)ptr + sizeof(*parser->udp));
			parser->total_hdr_len = (uint8_t *)ptr - (uint8_t *)old;
			return (ptr);
		case IPPROTO_TCP:
			parser->tcp = ptr;
			if (__predict_false(mlen < sizeof(struct tcphdr)))
				return (NULL);
			if (update_data) {
				parser->data.lro_type = LRO_TYPE_IPV4_TCP;
				parser->data.s_port = parser->tcp->th_sport;
				parser->data.d_port = parser->tcp->th_dport;
			} else {
				MPASS(parser->data.lro_type == LRO_TYPE_IPV4_TCP);
			}
			if (__predict_false(mlen < (parser->tcp->th_off << 2)))
				return (NULL);
			ptr = (uint8_t *)ptr + (parser->tcp->th_off << 2);
			parser->total_hdr_len = (uint8_t *)ptr - (uint8_t *)old;
			return (ptr);
		default:
			break;
		}
		break;
#endif
#ifdef INET6
	case htons(ETHERTYPE_IPV6):
		parser->ip6 = ptr;
		if (__predict_false(mlen < sizeof(struct ip6_hdr)))
			return (NULL);
		/* Ensure the packet has valid src/dst addrs */
		if (__predict_false(IN6_IS_ADDR_UNSPECIFIED(&parser->ip6->ip6_src) ||
			IN6_IS_ADDR_UNSPECIFIED(&parser->ip6->ip6_dst)))
			return (NULL);
		ptr = (uint8_t *)ptr + sizeof(*parser->ip6);
		if (update_data) {
			parser->data.s_addr.v6 = parser->ip6->ip6_src;
			parser->data.d_addr.v6 = parser->ip6->ip6_dst;
		}
		mlen -= sizeof(struct ip6_hdr);
		switch (parser->ip6->ip6_nxt) {
		case IPPROTO_UDP:
			if (__predict_false(mlen < sizeof(struct udphdr)))
				return (NULL);
			parser->udp = ptr;
			if (update_data) {
				parser->data.lro_type = LRO_TYPE_IPV6_UDP;
				parser->data.s_port = parser->udp->uh_sport;
				parser->data.d_port = parser->udp->uh_dport;
			} else {
				MPASS(parser->data.lro_type == LRO_TYPE_IPV6_UDP);
			}
			ptr = (uint8_t *)ptr + sizeof(*parser->udp);
			parser->total_hdr_len = (uint8_t *)ptr - (uint8_t *)old;
			return (ptr);
		case IPPROTO_TCP:
			if (__predict_false(mlen < sizeof(struct tcphdr)))
				return (NULL);
			parser->tcp = ptr;
			if (update_data) {
				parser->data.lro_type = LRO_TYPE_IPV6_TCP;
				parser->data.s_port = parser->tcp->th_sport;
				parser->data.d_port = parser->tcp->th_dport;
			} else {
				MPASS(parser->data.lro_type == LRO_TYPE_IPV6_TCP);
			}
			if (__predict_false(mlen < (parser->tcp->th_off << 2)))
				return (NULL);
			ptr = (uint8_t *)ptr + (parser->tcp->th_off << 2);
			parser->total_hdr_len = (uint8_t *)ptr - (uint8_t *)old;
			return (ptr);
		default:
			break;
		}
		break;
#endif
	default:
		break;
	}
	/* Invalid packet - cannot parse */
	return (NULL);
}

static const int vxlan_csum = CSUM_INNER_L3_CALC | CSUM_INNER_L3_VALID |
    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID;

static inline struct lro_parser *
tcp_lro_parser(struct mbuf *m, struct lro_parser *po, struct lro_parser *pi, bool update_data)
{
	void *data_ptr;

	/* Try to parse outer headers first. */
	data_ptr = tcp_lro_low_level_parser(m->m_data, po, update_data, false, m->m_len);
	if (data_ptr == NULL || po->total_hdr_len > m->m_len)
		return (NULL);

	if (update_data) {
		/* Store VLAN ID, if any. */
		if (__predict_false(m->m_flags & M_VLANTAG)) {
			po->data.vlan_id =
			    htons(m->m_pkthdr.ether_vtag) & htons(EVL_VLID_MASK);
		}
		/* Store decrypted flag, if any. */
		if (__predict_false((m->m_pkthdr.csum_flags &
		    CSUM_TLS_MASK) == CSUM_TLS_DECRYPTED))
			po->data.lro_flags |= LRO_FLAG_DECRYPTED;
	}

	switch (po->data.lro_type) {
	case LRO_TYPE_IPV4_UDP:
	case LRO_TYPE_IPV6_UDP:
		/* Check for VXLAN headers. */
		if ((m->m_pkthdr.csum_flags & vxlan_csum) != vxlan_csum)
			break;

		/* Try to parse inner headers. */
		data_ptr = tcp_lro_low_level_parser(data_ptr, pi, update_data, true,
						    (m->m_len - ((caddr_t)data_ptr - m->m_data)));
		if (data_ptr == NULL || (pi->total_hdr_len + po->total_hdr_len) > m->m_len)
			break;

		/* Verify supported header types. */
		switch (pi->data.lro_type) {
		case LRO_TYPE_IPV4_TCP:
		case LRO_TYPE_IPV6_TCP:
			return (pi);
		default:
			break;
		}
		break;
	case LRO_TYPE_IPV4_TCP:
	case LRO_TYPE_IPV6_TCP:
		if (update_data)
			memset(pi, 0, sizeof(*pi));
		return (po);
	default:
		break;
	}
	return (NULL);
}

static inline int
tcp_lro_trim_mbuf_chain(struct mbuf *m, const struct lro_parser *po)
{
	int len;

	switch (po->data.lro_type) {
#ifdef INET
	case LRO_TYPE_IPV4_TCP:
		len = ((uint8_t *)po->ip4 - (uint8_t *)m->m_data) +
		    ntohs(po->ip4->ip_len);
		break;
#endif
#ifdef INET6
	case LRO_TYPE_IPV6_TCP:
		len = ((uint8_t *)po->ip6 - (uint8_t *)m->m_data) +
		    ntohs(po->ip6->ip6_plen) + sizeof(*po->ip6);
		break;
#endif
	default:
		return (TCP_LRO_CANNOT);
	}

	/*
	 * If the frame is padded beyond the end of the IP packet,
	 * then trim the extra bytes off:
	 */
	if (__predict_true(m->m_pkthdr.len == len)) {
		return (0);
	} else if (m->m_pkthdr.len > len) {
		m_adj(m, len - m->m_pkthdr.len);
		return (0);
	}
	return (TCP_LRO_CANNOT);
}

static void
lro_free_mbuf_chain(struct mbuf *m)
{
	struct mbuf *save;

	while (m) {
		save = m->m_nextpkt;
		m->m_nextpkt = NULL;
		m_freem(m);
		m = save;
	}
}

void
tcp_lro_free(struct lro_ctrl *lc)
{
	struct lro_entry *le;
	unsigned x;

	/* reset LRO free list */
	LIST_INIT(&lc->lro_free);

	/* free active mbufs, if any */
	while ((le = LIST_FIRST(&lc->lro_active)) != NULL) {
		tcp_lro_active_remove(le);
		lro_free_mbuf_chain(le->m_head);
	}

	/* free hash table */
	free(lc->lro_hash, M_LRO);
	lc->lro_hash = NULL;
	lc->lro_hashsz = 0;

	/* free mbuf array, if any */
	for (x = 0; x != lc->lro_mbuf_count; x++)
		m_freem(lc->lro_mbuf_data[x].mb);
	lc->lro_mbuf_count = 0;

	/* free allocated memory, if any */
	free(lc->lro_mbuf_data, M_LRO);
	lc->lro_mbuf_data = NULL;
}

static uint16_t
tcp_lro_rx_csum_tcphdr(const struct tcphdr *th)
{
	const uint16_t *ptr;
	uint32_t csum;
	uint16_t len;

	csum = -th->th_sum;	/* exclude checksum field */
	len = th->th_off;
	ptr = (const uint16_t *)th;
	while (len--) {
		csum += *ptr;
		ptr++;
		csum += *ptr;
		ptr++;
	}
	while (csum > 0xffff)
		csum = (csum >> 16) + (csum & 0xffff);

	return (csum);
}

static uint16_t
tcp_lro_rx_csum_data(const struct lro_parser *pa, uint16_t tcp_csum)
{
	uint32_t c;
	uint16_t cs;

	c = tcp_csum;

	switch (pa->data.lro_type) {
#ifdef INET6
	case LRO_TYPE_IPV6_TCP:
		/* Compute full pseudo IPv6 header checksum. */
		cs = in6_cksum_pseudo(pa->ip6, ntohs(pa->ip6->ip6_plen), pa->ip6->ip6_nxt, 0);
		break;
#endif
#ifdef INET
	case LRO_TYPE_IPV4_TCP:
		/* Compute full pseudo IPv4 header checsum. */
		cs = in_addword(ntohs(pa->ip4->ip_len) - sizeof(*pa->ip4), IPPROTO_TCP);
		cs = in_pseudo(pa->ip4->ip_src.s_addr, pa->ip4->ip_dst.s_addr, htons(cs));
		break;
#endif
	default:
		cs = 0;		/* Keep compiler happy. */
		break;
	}

	/* Complement checksum. */
	cs = ~cs;
	c += cs;

	/* Remove TCP header checksum. */
	cs = ~tcp_lro_rx_csum_tcphdr(pa->tcp);
	c += cs;

	/* Compute checksum remainder. */
	while (c > 0xffff)
		c = (c >> 16) + (c & 0xffff);

	return (c);
}

static void
tcp_lro_rx_done(struct lro_ctrl *lc)
{
	struct lro_entry *le;

	while ((le = LIST_FIRST(&lc->lro_active)) != NULL) {
		tcp_lro_active_remove(le);
		tcp_lro_flush(lc, le);
	}
}

static void
tcp_lro_flush_active(struct lro_ctrl *lc)
{
	struct lro_entry *le, *le_tmp;

	/*
	 * Walk through the list of le entries, and
	 * any one that does have packets flush. This
	 * is called because we have an inbound packet
	 * (e.g. SYN) that has to have all others flushed
	 * in front of it. Note we have to do the remove
	 * because tcp_lro_flush() assumes that the entry
	 * is being freed. This is ok it will just get
	 * reallocated again like it was new.
	 */
	LIST_FOREACH_SAFE(le, &lc->lro_active, next, le_tmp) {
		if (le->m_head != NULL) {
			tcp_lro_active_remove(le);
			tcp_lro_flush(lc, le);
		}
	}
}

void
tcp_lro_flush_inactive(struct lro_ctrl *lc, const struct timeval *timeout)
{
	struct lro_entry *le, *le_tmp;
	uint64_t now, tov;
	struct bintime bt;

	NET_EPOCH_ASSERT();
	if (LIST_EMPTY(&lc->lro_active))
		return;

	/* get timeout time and current time in ns */
	binuptime(&bt);
	now = bintime2ns(&bt);
	tov = ((timeout->tv_sec * 1000000000) + (timeout->tv_usec * 1000));
	LIST_FOREACH_SAFE(le, &lc->lro_active, next, le_tmp) {
		if (now >= (bintime2ns(&le->alloc_time) + tov)) {
			tcp_lro_active_remove(le);
			tcp_lro_flush(lc, le);
		}
	}
}

#ifdef INET
static int
tcp_lro_rx_ipv4(struct lro_ctrl *lc, struct mbuf *m, struct ip *ip4)
{
	uint16_t csum;

	/* Legacy IP has a header checksum that needs to be correct. */
	if (m->m_pkthdr.csum_flags & CSUM_IP_CHECKED) {
		if (__predict_false((m->m_pkthdr.csum_flags & CSUM_IP_VALID) == 0)) {
			lc->lro_bad_csum++;
			return (TCP_LRO_CANNOT);
		}
	} else {
		csum = in_cksum_hdr(ip4);
		if (__predict_false(csum != 0)) {
			lc->lro_bad_csum++;
			return (TCP_LRO_CANNOT);
		}
	}
	return (0);
}
#endif

static inline void
tcp_lro_assign_and_checksum_16(uint16_t *ptr, uint16_t value, uint16_t *psum)
{
	uint32_t csum;

	csum = 0xffff - *ptr + value;
	while (csum > 0xffff)
		csum = (csum >> 16) + (csum & 0xffff);
	*ptr = value;
	*psum = csum;
}

static uint16_t
tcp_lro_update_checksum(const struct lro_parser *pa, const struct lro_entry *le,
    uint16_t payload_len, uint16_t delta_sum)
{
	uint32_t csum;
	uint16_t tlen;
	uint16_t temp[5] = {};

	switch (pa->data.lro_type) {
	case LRO_TYPE_IPV4_TCP:
		/* Compute new IPv4 length. */
		tlen = (pa->ip4->ip_hl << 2) + (pa->tcp->th_off << 2) + payload_len;
		tcp_lro_assign_and_checksum_16(&pa->ip4->ip_len, htons(tlen), &temp[0]);

		/* Subtract delta from current IPv4 checksum. */
		csum = pa->ip4->ip_sum + 0xffff - temp[0];
		while (csum > 0xffff)
			csum = (csum >> 16) + (csum & 0xffff);
		tcp_lro_assign_and_checksum_16(&pa->ip4->ip_sum, csum, &temp[1]);
		goto update_tcp_header;

	case LRO_TYPE_IPV6_TCP:
		/* Compute new IPv6 length. */
		tlen = (pa->tcp->th_off << 2) + payload_len;
		tcp_lro_assign_and_checksum_16(&pa->ip6->ip6_plen, htons(tlen), &temp[0]);
		goto update_tcp_header;

	case LRO_TYPE_IPV4_UDP:
		/* Compute new IPv4 length. */
		tlen = (pa->ip4->ip_hl << 2) + sizeof(*pa->udp) + payload_len;
		tcp_lro_assign_and_checksum_16(&pa->ip4->ip_len, htons(tlen), &temp[0]);

		/* Subtract delta from current IPv4 checksum. */
		csum = pa->ip4->ip_sum + 0xffff - temp[0];
		while (csum > 0xffff)
			csum = (csum >> 16) + (csum & 0xffff);
		tcp_lro_assign_and_checksum_16(&pa->ip4->ip_sum, csum, &temp[1]);
		goto update_udp_header;

	case LRO_TYPE_IPV6_UDP:
		/* Compute new IPv6 length. */
		tlen = sizeof(*pa->udp) + payload_len;
		tcp_lro_assign_and_checksum_16(&pa->ip6->ip6_plen, htons(tlen), &temp[0]);
		goto update_udp_header;

	default:
		return (0);
	}

update_tcp_header:
	/* Compute current TCP header checksum. */
	temp[2] = tcp_lro_rx_csum_tcphdr(pa->tcp);

	/* Incorporate the latest ACK into the TCP header. */
	pa->tcp->th_ack = le->ack_seq;
	pa->tcp->th_win = le->window;

	/* Incorporate latest timestamp into the TCP header. */
	if (le->timestamp != 0) {
		uint32_t *ts_ptr;

		ts_ptr = (uint32_t *)(pa->tcp + 1);
		ts_ptr[1] = htonl(le->tsval);
		ts_ptr[2] = le->tsecr;
	}

	/* Compute new TCP header checksum. */
	temp[3] = tcp_lro_rx_csum_tcphdr(pa->tcp);

	/* Compute new TCP checksum. */
	csum = pa->tcp->th_sum + 0xffff - delta_sum +
	    0xffff - temp[0] + 0xffff - temp[3] + temp[2];
	while (csum > 0xffff)
		csum = (csum >> 16) + (csum & 0xffff);

	/* Assign new TCP checksum. */
	tcp_lro_assign_and_checksum_16(&pa->tcp->th_sum, csum, &temp[4]);

	/* Compute all modififications affecting next checksum. */
	csum = temp[0] + temp[1] + 0xffff - temp[2] +
	    temp[3] + temp[4] + delta_sum;
	while (csum > 0xffff)
		csum = (csum >> 16) + (csum & 0xffff);

	/* Return delta checksum to next stage, if any. */
	return (csum);

update_udp_header:
	tlen = sizeof(*pa->udp) + payload_len;
	/* Assign new UDP length and compute checksum delta. */
	tcp_lro_assign_and_checksum_16(&pa->udp->uh_ulen, htons(tlen), &temp[2]);

	/* Check if there is a UDP checksum. */
	if (__predict_false(pa->udp->uh_sum != 0)) {
		/* Compute new UDP checksum. */
		csum = pa->udp->uh_sum + 0xffff - delta_sum +
		    0xffff - temp[0] + 0xffff - temp[2];
		while (csum > 0xffff)
			csum = (csum >> 16) + (csum & 0xffff);
		/* Assign new UDP checksum. */
		tcp_lro_assign_and_checksum_16(&pa->udp->uh_sum, csum, &temp[3]);
	}

	/* Compute all modififications affecting next checksum. */
	csum = temp[0] + temp[1] + temp[2] + temp[3] + delta_sum;
	while (csum > 0xffff)
		csum = (csum >> 16) + (csum & 0xffff);

	/* Return delta checksum to next stage, if any. */
	return (csum);
}

static void
tcp_flush_out_entry(struct lro_ctrl *lc, struct lro_entry *le)
{
	/* Check if we need to recompute any checksums. */
	if (le->needs_merge) {
		uint16_t csum;

		switch (le->inner.data.lro_type) {
		case LRO_TYPE_IPV4_TCP:
			csum = tcp_lro_update_checksum(&le->inner, le,
			    le->m_head->m_pkthdr.lro_tcp_d_len,
			    le->m_head->m_pkthdr.lro_tcp_d_csum);
			csum = tcp_lro_update_checksum(&le->outer, NULL,
			    le->m_head->m_pkthdr.lro_tcp_d_len +
			    le->inner.total_hdr_len, csum);
			le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR | CSUM_IP_CHECKED | CSUM_IP_VALID;
			le->m_head->m_pkthdr.csum_data = 0xffff;
			if (__predict_false(le->outer.data.lro_flags & LRO_FLAG_DECRYPTED))
				le->m_head->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
			break;
		case LRO_TYPE_IPV6_TCP:
			csum = tcp_lro_update_checksum(&le->inner, le,
			    le->m_head->m_pkthdr.lro_tcp_d_len,
			    le->m_head->m_pkthdr.lro_tcp_d_csum);
			csum = tcp_lro_update_checksum(&le->outer, NULL,
			    le->m_head->m_pkthdr.lro_tcp_d_len +
			    le->inner.total_hdr_len, csum);
			le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR;
			le->m_head->m_pkthdr.csum_data = 0xffff;
			if (__predict_false(le->outer.data.lro_flags & LRO_FLAG_DECRYPTED))
				le->m_head->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
			break;
		case LRO_TYPE_NONE:
			switch (le->outer.data.lro_type) {
			case LRO_TYPE_IPV4_TCP:
				csum = tcp_lro_update_checksum(&le->outer, le,
				    le->m_head->m_pkthdr.lro_tcp_d_len,
				    le->m_head->m_pkthdr.lro_tcp_d_csum);
				le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
				    CSUM_PSEUDO_HDR | CSUM_IP_CHECKED | CSUM_IP_VALID;
				le->m_head->m_pkthdr.csum_data = 0xffff;
				if (__predict_false(le->outer.data.lro_flags & LRO_FLAG_DECRYPTED))
					le->m_head->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
				break;
			case LRO_TYPE_IPV6_TCP:
				csum = tcp_lro_update_checksum(&le->outer, le,
				    le->m_head->m_pkthdr.lro_tcp_d_len,
				    le->m_head->m_pkthdr.lro_tcp_d_csum);
				le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
				    CSUM_PSEUDO_HDR;
				le->m_head->m_pkthdr.csum_data = 0xffff;
				if (__predict_false(le->outer.data.lro_flags & LRO_FLAG_DECRYPTED))
					le->m_head->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
				break;
			default:
				break;
			}
			break;
		default:
			break;
		}
	}

	/*
	 * Break any chain, this is not set to NULL on the singleton
	 * case m_nextpkt points to m_head. Other case set them
	 * m_nextpkt to NULL in push_and_replace.
	 */
	le->m_head->m_nextpkt = NULL;
	lc->lro_queued += le->m_head->m_pkthdr.lro_nsegs;
	(*lc->ifp->if_input)(lc->ifp, le->m_head);
}

static void
tcp_set_entry_to_mbuf(struct lro_ctrl *lc, struct lro_entry *le,
    struct mbuf *m, struct tcphdr *th)
{
	uint32_t *ts_ptr;
	uint16_t tcp_data_len;
	uint16_t tcp_opt_len;

	ts_ptr = (uint32_t *)(th + 1);
	tcp_opt_len = (th->th_off << 2);
	tcp_opt_len -= sizeof(*th);

	/* Check if there is a timestamp option. */
	if (tcp_opt_len == 0 ||
	    __predict_false(tcp_opt_len != TCPOLEN_TSTAMP_APPA ||
	    *ts_ptr != TCP_LRO_TS_OPTION)) {
		/* We failed to find the timestamp option. */
		le->timestamp = 0;
	} else {
		le->timestamp = 1;
		le->tsval = ntohl(*(ts_ptr + 1));
		le->tsecr = *(ts_ptr + 2);
	}

	tcp_data_len = m->m_pkthdr.lro_tcp_d_len;

	/* Pull out TCP sequence numbers and window size. */
	le->next_seq = ntohl(th->th_seq) + tcp_data_len;
	le->ack_seq = th->th_ack;
	le->window = th->th_win;
	le->flags = tcp_get_flags(th);
	le->needs_merge = 0;

	/* Setup new data pointers. */
	le->m_head = m;
	le->m_tail = m_last(m);
}

static void
tcp_push_and_replace(struct lro_ctrl *lc, struct lro_entry *le, struct mbuf *m)
{
	struct lro_parser *pa;

	/*
	 * Push up the stack of the current entry
	 * and replace it with "m".
	 */
	struct mbuf *msave;

	/* Grab off the next and save it */
	msave = le->m_head->m_nextpkt;
	le->m_head->m_nextpkt = NULL;

	/* Now push out the old entry */
	tcp_flush_out_entry(lc, le);

	/* Re-parse new header, should not fail. */
	pa = tcp_lro_parser(m, &le->outer, &le->inner, false);
	KASSERT(pa != NULL,
	    ("tcp_push_and_replace: LRO parser failed on m=%p\n", m));

	/*
	 * Now to replace the data properly in the entry
	 * we have to reset the TCP header and
	 * other fields.
	 */
	tcp_set_entry_to_mbuf(lc, le, m, pa->tcp);

	/* Restore the next list */
	m->m_nextpkt = msave;
}

static void
tcp_lro_mbuf_append_pkthdr(struct lro_entry *le, const struct mbuf *p)
{
	struct mbuf *m;
	uint32_t csum;

	m = le->m_head;
	if (m->m_pkthdr.lro_nsegs == 1) {
		/* Compute relative checksum. */
		csum = p->m_pkthdr.lro_tcp_d_csum;
	} else {
		/* Merge TCP data checksums. */
		csum = (uint32_t)m->m_pkthdr.lro_tcp_d_csum +
		    (uint32_t)p->m_pkthdr.lro_tcp_d_csum;
		while (csum > 0xffff)
			csum = (csum >> 16) + (csum & 0xffff);
	}

	/* Update various counters. */
	m->m_pkthdr.len += p->m_pkthdr.lro_tcp_d_len;
	m->m_pkthdr.lro_tcp_d_csum = csum;
	m->m_pkthdr.lro_tcp_d_len += p->m_pkthdr.lro_tcp_d_len;
	m->m_pkthdr.lro_nsegs += p->m_pkthdr.lro_nsegs;
	le->needs_merge = 1;
}

static void
tcp_lro_condense(struct lro_ctrl *lc, struct lro_entry *le)
{
	/*
	 * Walk through the mbuf chain we
	 * have on tap and compress/condense
	 * as required.
	 */
	uint32_t *ts_ptr;
	struct mbuf *m;
	struct tcphdr *th;
	uint32_t tcp_data_len_total;
	uint32_t tcp_data_seg_total;
	uint16_t tcp_data_len;
	uint16_t tcp_opt_len;

	/*
	 * First we must check the lead (m_head)
	 * we must make sure that it is *not*
	 * something that should be sent up
	 * right away (sack etc).
	 */
again:
	m = le->m_head->m_nextpkt;
	if (m == NULL) {
		/* Just one left. */
		return;
	}

	th = tcp_lro_get_th(m);
	tcp_opt_len = (th->th_off << 2);
	tcp_opt_len -= sizeof(*th);
	ts_ptr = (uint32_t *)(th + 1);

	if (tcp_opt_len != 0 && __predict_false(tcp_opt_len != TCPOLEN_TSTAMP_APPA ||
	    *ts_ptr != TCP_LRO_TS_OPTION)) {
		/*
		 * Its not the timestamp. We can't
		 * use this guy as the head.
		 */
		le->m_head->m_nextpkt = m->m_nextpkt;
		tcp_push_and_replace(lc, le, m);
		goto again;
	}
	if ((tcp_get_flags(th) & ~(TH_ACK | TH_PUSH)) != 0) {
		/*
		 * Make sure that previously seen segments/ACKs are delivered
		 * before this segment, e.g. FIN.
		 */
		le->m_head->m_nextpkt = m->m_nextpkt;
		tcp_push_and_replace(lc, le, m);
		goto again;
	}
	while((m = le->m_head->m_nextpkt) != NULL) {
		/*
		 * condense m into le, first
		 * pull m out of the list.
		 */
		le->m_head->m_nextpkt = m->m_nextpkt;
		m->m_nextpkt = NULL;
		/* Setup my data */
		tcp_data_len = m->m_pkthdr.lro_tcp_d_len;
		th = tcp_lro_get_th(m);
		ts_ptr = (uint32_t *)(th + 1);
		tcp_opt_len = (th->th_off << 2);
		tcp_opt_len -= sizeof(*th);
		tcp_data_len_total = le->m_head->m_pkthdr.lro_tcp_d_len + tcp_data_len;
		tcp_data_seg_total = le->m_head->m_pkthdr.lro_nsegs + m->m_pkthdr.lro_nsegs;

		if (tcp_data_seg_total >= lc->lro_ackcnt_lim ||
		    tcp_data_len_total >= lc->lro_length_lim) {
			/* Flush now if appending will result in overflow. */
			tcp_push_and_replace(lc, le, m);
			goto again;
		}
		if (tcp_opt_len != 0 &&
		    __predict_false(tcp_opt_len != TCPOLEN_TSTAMP_APPA ||
		    *ts_ptr != TCP_LRO_TS_OPTION)) {
			/*
			 * Maybe a sack in the new one? We need to
			 * start all over after flushing the
			 * current le. We will go up to the beginning
			 * and flush it (calling the replace again possibly
			 * or just returning).
			 */
			tcp_push_and_replace(lc, le, m);
			goto again;
		}
		if ((tcp_get_flags(th) & ~(TH_ACK | TH_PUSH)) != 0) {
			tcp_push_and_replace(lc, le, m);
			goto again;
		}
		if (tcp_opt_len != 0) {
			uint32_t tsval = ntohl(*(ts_ptr + 1));
			/* Make sure timestamp values are increasing. */
			if (TSTMP_GT(le->tsval, tsval))  {
				tcp_push_and_replace(lc, le, m);
				goto again;
			}
			le->tsval = tsval;
			le->tsecr = *(ts_ptr + 2);
		}
		/* Try to append the new segment. */
		if (__predict_false(ntohl(th->th_seq) != le->next_seq ||
				    ((tcp_get_flags(th) & TH_ACK) !=
				      (le->flags & TH_ACK)) ||
				    (tcp_data_len == 0 &&
				     le->ack_seq == th->th_ack &&
				     le->window == th->th_win))) {
			/* Out of order packet, non-ACK + ACK or dup ACK. */
			tcp_push_and_replace(lc, le, m);
			goto again;
		}
		if (tcp_data_len != 0 ||
		    SEQ_GT(ntohl(th->th_ack), ntohl(le->ack_seq))) {
			le->next_seq += tcp_data_len;
			le->ack_seq = th->th_ack;
			le->window = th->th_win;
			le->needs_merge = 1;
		} else if (th->th_ack == le->ack_seq) {
			if (WIN_GT(th->th_win, le->window)) {
				le->window = th->th_win;
				le->needs_merge = 1;
			}
		}

		if (tcp_data_len == 0) {
			m_freem(m);
			continue;
		}

		/* Merge TCP data checksum and length to head mbuf. */
		tcp_lro_mbuf_append_pkthdr(le, m);

		/*
		 * Adjust the mbuf so that m_data points to the first byte of
		 * the ULP payload.  Adjust the mbuf to avoid complications and
		 * append new segment to existing mbuf chain.
		 */
		m_adj(m, m->m_pkthdr.len - tcp_data_len);
		m_demote_pkthdr(m);
		le->m_tail->m_next = m;
		le->m_tail = m_last(m);
	}
}

static void
tcp_lro_flush(struct lro_ctrl *lc, struct lro_entry *le)
{

	/* Only optimise if there are multiple packets waiting. */
	NET_EPOCH_ASSERT();
	if (tcp_lro_flush_tcphpts == NULL ||
	    tcp_lro_flush_tcphpts(lc, le) != 0) {
		tcp_lro_condense(lc, le);
		tcp_flush_out_entry(lc, le);
	}
	lc->lro_flushed++;
	bzero(le, sizeof(*le));
	LIST_INSERT_HEAD(&lc->lro_free, le, next);
}

#define	tcp_lro_msb_64(x) (1ULL << (flsll(x) - 1))

/*
 * The tcp_lro_sort() routine is comparable to qsort(), except it has
 * a worst case complexity limit of O(MIN(N,64)*N), where N is the
 * number of elements to sort and 64 is the number of sequence bits
 * available. The algorithm is bit-slicing the 64-bit sequence number,
 * sorting one bit at a time from the most significant bit until the
 * least significant one, skipping the constant bits. This is
 * typically called a radix sort.
 */
static void
tcp_lro_sort(struct lro_mbuf_sort *parray, uint32_t size)
{
	struct lro_mbuf_sort temp;
	uint64_t ones;
	uint64_t zeros;
	uint32_t x;
	uint32_t y;

repeat:
	/* for small arrays insertion sort is faster */
	if (size <= 12) {
		for (x = 1; x < size; x++) {
			temp = parray[x];
			for (y = x; y > 0 && temp.seq < parray[y - 1].seq; y--)
				parray[y] = parray[y - 1];
			parray[y] = temp;
		}
		return;
	}

	/* compute sequence bits which are constant */
	ones = 0;
	zeros = 0;
	for (x = 0; x != size; x++) {
		ones |= parray[x].seq;
		zeros |= ~parray[x].seq;
	}

	/* compute bits which are not constant into "ones" */
	ones &= zeros;
	if (ones == 0)
		return;

	/* pick the most significant bit which is not constant */
	ones = tcp_lro_msb_64(ones);

	/*
	 * Move entries having cleared sequence bits to the beginning
	 * of the array:
	 */
	for (x = y = 0; y != size; y++) {
		/* skip set bits */
		if (parray[y].seq & ones)
			continue;
		/* swap entries */
		temp = parray[x];
		parray[x] = parray[y];
		parray[y] = temp;
		x++;
	}

	KASSERT(x != 0 && x != size, ("Memory is corrupted\n"));

	/* sort zeros */
	tcp_lro_sort(parray, x);

	/* sort ones */
	parray += x;
	size -= x;
	goto repeat;
}

void
tcp_lro_flush_all(struct lro_ctrl *lc)
{
	uint64_t seq;
	uint64_t nseq;
	unsigned x;

	NET_EPOCH_ASSERT();
	/* check if no mbufs to flush */
	if (lc->lro_mbuf_count == 0)
		goto done;
	if (lc->lro_cpu_is_set == 0) {
		if (lc->lro_last_cpu == curcpu) {
			lc->lro_cnt_of_same_cpu++;
			/* Have we reached the threshold to declare a cpu? */
			if (lc->lro_cnt_of_same_cpu > tcp_lro_cpu_set_thresh)
				lc->lro_cpu_is_set = 1;
		} else {
			lc->lro_last_cpu = curcpu;
			lc->lro_cnt_of_same_cpu = 0;
		}
	}
	CURVNET_SET(lc->ifp->if_vnet);

	/* get current time */
	binuptime(&lc->lro_last_queue_time);

	/* sort all mbufs according to stream */
	tcp_lro_sort(lc->lro_mbuf_data, lc->lro_mbuf_count);

	/* input data into LRO engine, stream by stream */
	seq = 0;
	for (x = 0; x != lc->lro_mbuf_count; x++) {
		struct mbuf *mb;

		/* get mbuf */
		mb = lc->lro_mbuf_data[x].mb;

		/* get sequence number, masking away the packet index */
		nseq = lc->lro_mbuf_data[x].seq & (-1ULL << 24);

		/* check for new stream */
		if (seq != nseq) {
			seq = nseq;

			/* flush active streams */
			tcp_lro_rx_done(lc);
		}

		/* add packet to LRO engine */
		if (tcp_lro_rx_common(lc, mb, 0, false) != 0) {
 			/* Flush anything we have acummulated */
 			tcp_lro_flush_active(lc);
			/* input packet to network layer */
			(*lc->ifp->if_input)(lc->ifp, mb);
			lc->lro_queued++;
			lc->lro_flushed++;
		}
	}
	CURVNET_RESTORE();
done:
	/* flush active streams */
	tcp_lro_rx_done(lc);
	tcp_hpts_softclock();
	lc->lro_mbuf_count = 0;
}

static struct lro_head *
tcp_lro_rx_get_bucket(struct lro_ctrl *lc, struct mbuf *m, struct lro_parser *parser)
{
	u_long hash;

	if (M_HASHTYPE_ISHASH(m)) {
		hash = m->m_pkthdr.flowid;
	} else {
		for (unsigned i = hash = 0; i != LRO_RAW_ADDRESS_MAX; i++)
			hash += parser->data.raw[i];
	}
	return (&lc->lro_hash[hash % lc->lro_hashsz]);
}

static int
tcp_lro_rx_common(struct lro_ctrl *lc, struct mbuf *m, uint32_t csum, bool use_hash)
{
	struct lro_parser pi;	/* inner address data */
	struct lro_parser po;	/* outer address data */
	struct lro_parser *pa;	/* current parser for TCP stream */
	struct lro_entry *le;
	struct lro_head *bucket;
	struct tcphdr *th;
	int tcp_data_len;
	int tcp_opt_len;
	int error;
	uint16_t tcp_data_sum;

#ifdef INET
	/* Quickly decide if packet cannot be LRO'ed */
	if (__predict_false(V_ipforwarding != 0))
		return (TCP_LRO_CANNOT);
#endif
#ifdef INET6
	/* Quickly decide if packet cannot be LRO'ed */
	if (__predict_false(V_ip6_forwarding != 0))
		return (TCP_LRO_CANNOT);
#endif
	if (((m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) !=
	     ((CSUM_DATA_VALID | CSUM_PSEUDO_HDR))) || 
	    (m->m_pkthdr.csum_data != 0xffff)) {
		/* 
		 * The checksum either did not have hardware offload
		 * or it was a bad checksum. We can't LRO such
		 * a packet.
		 */
		counter_u64_add(tcp_bad_csums, 1);
		return (TCP_LRO_CANNOT);
	}
	/* We expect a contiguous header [eh, ip, tcp]. */
	pa = tcp_lro_parser(m, &po, &pi, true);
	if (__predict_false(pa == NULL))
		return (TCP_LRO_NOT_SUPPORTED);

	/* We don't expect any padding. */
	error = tcp_lro_trim_mbuf_chain(m, pa);
	if (__predict_false(error != 0))
		return (error);

#ifdef INET
	switch (pa->data.lro_type) {
	case LRO_TYPE_IPV4_TCP:
		error = tcp_lro_rx_ipv4(lc, m, pa->ip4);
		if (__predict_false(error != 0))
			return (error);
		break;
	default:
		break;
	}
#endif
	/* If no hardware or arrival stamp on the packet add timestamp */
	if ((m->m_flags & (M_TSTMP_LRO | M_TSTMP)) == 0) {
		m->m_pkthdr.rcv_tstmp = bintime2ns(&lc->lro_last_queue_time); 
		m->m_flags |= M_TSTMP_LRO;
	}

	/* Get pointer to TCP header. */
	th = pa->tcp;

	/* Don't process SYN packets. */
	if (__predict_false(tcp_get_flags(th) & TH_SYN))
		return (TCP_LRO_CANNOT);

	/* Get total TCP header length and compute payload length. */
	tcp_opt_len = (th->th_off << 2);
	tcp_data_len = m->m_pkthdr.len - ((uint8_t *)th -
	    (uint8_t *)m->m_data) - tcp_opt_len;
	tcp_opt_len -= sizeof(*th);

	/* Don't process invalid TCP headers. */
	if (__predict_false(tcp_opt_len < 0 || tcp_data_len < 0))
		return (TCP_LRO_CANNOT);

	/* Compute TCP data only checksum. */
	if (tcp_data_len == 0)
		tcp_data_sum = 0;	/* no data, no checksum */
	else if (__predict_false(csum != 0))
		tcp_data_sum = tcp_lro_rx_csum_data(pa, ~csum);
	else
		tcp_data_sum = tcp_lro_rx_csum_data(pa, ~th->th_sum);

	/* Save TCP info in mbuf. */
	m->m_nextpkt = NULL;
	m->m_pkthdr.rcvif = lc->ifp;
	m->m_pkthdr.lro_tcp_d_csum = tcp_data_sum;
	m->m_pkthdr.lro_tcp_d_len = tcp_data_len;
	m->m_pkthdr.lro_tcp_h_off = ((uint8_t *)th - (uint8_t *)m->m_data);
	m->m_pkthdr.lro_nsegs = 1;

	/* Get hash bucket. */
	if (!use_hash) {
		bucket = &lc->lro_hash[0];
	} else {
		bucket = tcp_lro_rx_get_bucket(lc, m, pa);
	}

	/* Try to find a matching previous segment. */
	LIST_FOREACH(le, bucket, hash_next) {
		/* Compare addresses and ports. */
		if (lro_address_compare(&po.data, &le->outer.data) == false ||
		    lro_address_compare(&pi.data, &le->inner.data) == false)
			continue;

		/* Check if no data and old ACK. */
		if (tcp_data_len == 0 &&
		    SEQ_LT(ntohl(th->th_ack), ntohl(le->ack_seq))) {
			m_freem(m);
			return (0);
		}

		/* Mark "m" in the last spot. */
		le->m_last_mbuf->m_nextpkt = m;
		/* Now set the tail to "m". */
		le->m_last_mbuf = m;
		return (0);
	}

	/* Try to find an empty slot. */
	if (LIST_EMPTY(&lc->lro_free))
		return (TCP_LRO_NO_ENTRIES);

	/* Start a new segment chain. */
	le = LIST_FIRST(&lc->lro_free);
	LIST_REMOVE(le, next);
	tcp_lro_active_insert(lc, bucket, le);

	/* Make sure the headers are set. */
	le->inner = pi;
	le->outer = po;

	/* Store time this entry was allocated. */
	le->alloc_time = lc->lro_last_queue_time;

	tcp_set_entry_to_mbuf(lc, le, m, th);

	/* Now set the tail to "m". */
	le->m_last_mbuf = m;

	return (0);
}

int
tcp_lro_rx(struct lro_ctrl *lc, struct mbuf *m, uint32_t csum)
{
	int error;

	if (((m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) !=
	     ((CSUM_DATA_VALID | CSUM_PSEUDO_HDR))) || 
	    (m->m_pkthdr.csum_data != 0xffff)) {
		/* 
		 * The checksum either did not have hardware offload
		 * or it was a bad checksum. We can't LRO such
		 * a packet.
		 */
		counter_u64_add(tcp_bad_csums, 1);
		return (TCP_LRO_CANNOT);
	}
	/* get current time */
	binuptime(&lc->lro_last_queue_time);
	CURVNET_SET(lc->ifp->if_vnet);
	error = tcp_lro_rx_common(lc, m, csum, true);
	if (__predict_false(error != 0)) {
		/*
		 * Flush anything we have acummulated
		 * ahead of this packet that can't
		 * be LRO'd. This preserves order.
		 */
		tcp_lro_flush_active(lc);
	}
	CURVNET_RESTORE();

	return (error);
}

void
tcp_lro_queue_mbuf(struct lro_ctrl *lc, struct mbuf *mb)
{
	NET_EPOCH_ASSERT();
	/* sanity checks */
	if (__predict_false(lc->ifp == NULL || lc->lro_mbuf_data == NULL ||
	    lc->lro_mbuf_max == 0)) {
		/* packet drop */
		m_freem(mb);
		return;
	}

	/* check if packet is not LRO capable */
	if (__predict_false((lc->ifp->if_capenable & IFCAP_LRO) == 0)) {
		/* input packet to network layer */
		(*lc->ifp->if_input) (lc->ifp, mb);
		return;
	}

 	/* If no hardware or arrival stamp on the packet add timestamp */
 	if ((tcplro_stacks_wanting_mbufq > 0) &&
 	    (tcp_less_accurate_lro_ts == 0) &&
 	    ((mb->m_flags & M_TSTMP) == 0)) {
 		/* Add in an LRO time since no hardware */
 		binuptime(&lc->lro_last_queue_time);
 		mb->m_pkthdr.rcv_tstmp = bintime2ns(&lc->lro_last_queue_time); 
 		mb->m_flags |= M_TSTMP_LRO;
 	}

	/* create sequence number */
	lc->lro_mbuf_data[lc->lro_mbuf_count].seq =
	    (((uint64_t)M_HASHTYPE_GET(mb)) << 56) |
	    (((uint64_t)mb->m_pkthdr.flowid) << 24) |
	    ((uint64_t)lc->lro_mbuf_count);

	/* enter mbuf */
	lc->lro_mbuf_data[lc->lro_mbuf_count].mb = mb;

	/* flush if array is full */
	if (__predict_false(++lc->lro_mbuf_count == lc->lro_mbuf_max))
		tcp_lro_flush_all(lc);
}

/* end */
