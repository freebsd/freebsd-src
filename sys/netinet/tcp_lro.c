/*-
 * Copyright (c) 2007, Myricom Inc.
 * Copyright (c) 2008, Intel Corporation.
 * Copyright (c) 2012 The FreeBSD Foundation
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
__FBSDID("$FreeBSD$");

#include "opt_inet.h"
#include "opt_inet6.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/vnet.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>

#include <netinet6/ip6_var.h>

#include <machine/in_cksum.h>

#ifndef LRO_ENTRIES
#define	LRO_ENTRIES	8	/* # of LRO entries per RX queue. */
#endif

#define	TCP_LRO_UPDATE_CSUM	1
#ifndef	TCP_LRO_UPDATE_CSUM
#define	TCP_LRO_INVALID_CSUM	0x0000
#endif

int
tcp_lro_init(struct lro_ctrl *lc)
{
	struct lro_entry *le;
	int error, i;

	lc->lro_bad_csum = 0;
	lc->lro_queued = 0;
	lc->lro_flushed = 0;
	lc->lro_cnt = 0;
	SLIST_INIT(&lc->lro_free);
	SLIST_INIT(&lc->lro_active);

	error = 0;
	for (i = 0; i < LRO_ENTRIES; i++) {
		le = (struct lro_entry *)malloc(sizeof(*le), M_DEVBUF,
		    M_NOWAIT | M_ZERO);
                if (le == NULL) {
			if (i == 0)
				error = ENOMEM;
                        break;
                }
		lc->lro_cnt = i + 1;
		SLIST_INSERT_HEAD(&lc->lro_free, le, next);
        }

	return (error);
}

void
tcp_lro_free(struct lro_ctrl *lc)
{
	struct lro_entry *le;

	while (!SLIST_EMPTY(&lc->lro_free)) {
		le = SLIST_FIRST(&lc->lro_free);
		SLIST_REMOVE_HEAD(&lc->lro_free, next);
		free(le, M_DEVBUF);
	}
}

#ifdef TCP_LRO_UPDATE_CSUM
static uint16_t
tcp_lro_csum_th(struct tcphdr *th)
{
	uint32_t ch;
	uint16_t *p, l;

	ch = th->th_sum = 0x0000;
	l = th->th_off;
	p = (uint16_t *)th;
	while (l > 0) {
		ch += *p;
		p++;
		ch += *p;
		p++;
		l--;
	}
	while (ch > 0xffff)
		ch = (ch >> 16) + (ch & 0xffff);

	return (ch & 0xffff);
}

static uint16_t
tcp_lro_rx_csum_fixup(struct lro_entry *le, void *l3hdr, struct tcphdr *th,
    uint16_t tcp_data_len, uint16_t csum)
{
	uint32_t c;
	uint16_t cs;

	c = csum;

	/* Remove length from checksum. */
	switch (le->eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
	{
		struct ip6_hdr *ip6;

		ip6 = (struct ip6_hdr *)l3hdr;
		if (le->append_cnt == 0)
			cs = ip6->ip6_plen;
		else {
			uint32_t cx;

			cx = ntohs(ip6->ip6_plen);
			cs = in6_cksum_pseudo(ip6, cx, ip6->ip6_nxt, 0);
		}
		break;
	}
#endif
#ifdef INET
	case ETHERTYPE_IP:
	{
		struct ip *ip4;

		ip4 = (struct ip *)l3hdr;
		if (le->append_cnt == 0)
			cs = ip4->ip_len;
		else {
			cs = in_addword(ntohs(ip4->ip_len) - sizeof(*ip4),
			    IPPROTO_TCP);
			cs = in_pseudo(ip4->ip_src.s_addr, ip4->ip_dst.s_addr,
			    htons(cs));
		}
		break;
	}
#endif
	default:
		cs = 0;		/* Keep compiler happy. */
	}

	cs = ~cs;
	c += cs;

	/* Remove TCP header csum. */
	cs = ~tcp_lro_csum_th(th);
	c += cs;
	while (c > 0xffff)
		c = (c >> 16) + (c & 0xffff);

	return (c & 0xffff);
}
#endif

void
tcp_lro_flush_inactive(struct lro_ctrl *lc, const struct timeval *timeout)
{
	struct lro_entry *le, *le_tmp;
	struct timeval tv;

	if (SLIST_EMPTY(&lc->lro_active))
		return;

	getmicrotime(&tv);
	timevalsub(&tv, timeout);
	SLIST_FOREACH_SAFE(le, &lc->lro_active, next, le_tmp) {
		if (timevalcmp(&tv, &le->mtime, >=)) {
			SLIST_REMOVE(&lc->lro_active, le, lro_entry, next);
			tcp_lro_flush(lc, le);
		}
	}
}

void
tcp_lro_flush(struct lro_ctrl *lc, struct lro_entry *le)
{

	if (le->append_cnt > 0) {
		struct tcphdr *th;
		uint16_t p_len;

		p_len = htons(le->p_len);
		switch (le->eh_type) {
#ifdef INET6
		case ETHERTYPE_IPV6:
		{
			struct ip6_hdr *ip6;

			ip6 = le->le_ip6;
			ip6->ip6_plen = p_len;
			th = (struct tcphdr *)(ip6 + 1);
			le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR;
			le->p_len += ETHER_HDR_LEN + sizeof(*ip6);
			break;
		}
#endif
#ifdef INET
		case ETHERTYPE_IP:
		{
			struct ip *ip4;
#ifdef TCP_LRO_UPDATE_CSUM
			uint32_t cl;
			uint16_t c;
#endif

			ip4 = le->le_ip4;
#ifdef TCP_LRO_UPDATE_CSUM
			/* Fix IP header checksum for new length. */
			c = ~ip4->ip_sum;
			cl = c;
			c = ~ip4->ip_len;
			cl += c + p_len;
			while (cl > 0xffff)
				cl = (cl >> 16) + (cl & 0xffff);
			c = cl;
			ip4->ip_sum = ~c;
#else
			ip4->ip_sum = TCP_LRO_INVALID_CSUM;
#endif
			ip4->ip_len = p_len;
			th = (struct tcphdr *)(ip4 + 1);
			le->m_head->m_pkthdr.csum_flags = CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR | CSUM_IP_CHECKED | CSUM_IP_VALID;
			le->p_len += ETHER_HDR_LEN;
			break;
		}
#endif
		default:
			th = NULL;	/* Keep compiler happy. */
		}
		le->m_head->m_pkthdr.csum_data = 0xffff;
		le->m_head->m_pkthdr.len = le->p_len;

		/* Incorporate the latest ACK into the TCP header. */
		th->th_ack = le->ack_seq;
		th->th_win = le->window;
		/* Incorporate latest timestamp into the TCP header. */
		if (le->timestamp != 0) {
			uint32_t *ts_ptr;

			ts_ptr = (uint32_t *)(th + 1);
			ts_ptr[1] = htonl(le->tsval);
			ts_ptr[2] = le->tsecr;
		}
#ifdef TCP_LRO_UPDATE_CSUM
		/* Update the TCP header checksum. */
		le->ulp_csum += p_len;
		le->ulp_csum += tcp_lro_csum_th(th);
		while (le->ulp_csum > 0xffff)
			le->ulp_csum = (le->ulp_csum >> 16) +
			    (le->ulp_csum & 0xffff);
		th->th_sum = (le->ulp_csum & 0xffff);
		th->th_sum = ~th->th_sum;
#else
		th->th_sum = TCP_LRO_INVALID_CSUM;
#endif
	}

	(*lc->ifp->if_input)(lc->ifp, le->m_head);
	lc->lro_queued += le->append_cnt + 1;
	lc->lro_flushed++;
	bzero(le, sizeof(*le));
	SLIST_INSERT_HEAD(&lc->lro_free, le, next);
}

#ifdef INET6
static int
tcp_lro_rx_ipv6(struct lro_ctrl *lc, struct mbuf *m, struct ip6_hdr *ip6,
    struct tcphdr **th)
{

	/* XXX-BZ we should check the flow-label. */

	/* XXX-BZ We do not yet support ext. hdrs. */
	if (ip6->ip6_nxt != IPPROTO_TCP)
		return (TCP_LRO_NOT_SUPPORTED);

	/* Find the TCP header. */
	*th = (struct tcphdr *)(ip6 + 1);

	return (0);
}
#endif

#ifdef INET
static int
tcp_lro_rx_ipv4(struct lro_ctrl *lc, struct mbuf *m, struct ip *ip4,
    struct tcphdr **th)
{
	int csum_flags;
	uint16_t csum;

	if (ip4->ip_p != IPPROTO_TCP)
		return (TCP_LRO_NOT_SUPPORTED);

	/* Ensure there are no options. */
	if ((ip4->ip_hl << 2) != sizeof (*ip4))
		return (TCP_LRO_CANNOT);

	/* .. and the packet is not fragmented. */
	if (ip4->ip_off & htons(IP_MF|IP_OFFMASK))
		return (TCP_LRO_CANNOT);

	/* Legacy IP has a header checksum that needs to be correct. */
	csum_flags = m->m_pkthdr.csum_flags;
	if (csum_flags & CSUM_IP_CHECKED) {
		if (__predict_false((csum_flags & CSUM_IP_VALID) == 0)) {
			lc->lro_bad_csum++;
			return (TCP_LRO_CANNOT);
		}
	} else {
		csum = in_cksum_hdr(ip4);
		if (__predict_false((csum) != 0)) {
			lc->lro_bad_csum++;
			return (TCP_LRO_CANNOT);
		}
	}

	/* Find the TCP header (we assured there are no IP options). */
	*th = (struct tcphdr *)(ip4 + 1);

	return (0);
}
#endif

int
tcp_lro_rx(struct lro_ctrl *lc, struct mbuf *m, uint32_t csum)
{
	struct lro_entry *le;
	struct ether_header *eh;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip4 = NULL;		/* Keep compiler happy. */
#endif
	struct tcphdr *th;
	void *l3hdr = NULL;		/* Keep compiler happy. */
	uint32_t *ts_ptr;
	tcp_seq seq;
	int error, ip_len, l;
	uint16_t eh_type, tcp_data_len;

	/* We expect a contiguous header [eh, ip, tcp]. */

	eh = mtod(m, struct ether_header *);
	eh_type = ntohs(eh->ether_type);
	switch (eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
	{
		CURVNET_SET(lc->ifp->if_vnet);
		if (V_ip6_forwarding != 0) {
			/* XXX-BZ stats but changing lro_ctrl is a problem. */
			CURVNET_RESTORE();
			return (TCP_LRO_CANNOT);
		}
		CURVNET_RESTORE();
		l3hdr = ip6 = (struct ip6_hdr *)(eh + 1);
		error = tcp_lro_rx_ipv6(lc, m, ip6, &th);
		if (error != 0)
			return (error);
		tcp_data_len = ntohs(ip6->ip6_plen);
		ip_len = sizeof(*ip6) + tcp_data_len;
		break;
	}
#endif
#ifdef INET
	case ETHERTYPE_IP:
	{
		CURVNET_SET(lc->ifp->if_vnet);
		if (V_ipforwarding != 0) {
			/* XXX-BZ stats but changing lro_ctrl is a problem. */
			CURVNET_RESTORE();
			return (TCP_LRO_CANNOT);
		}
		CURVNET_RESTORE();
		l3hdr = ip4 = (struct ip *)(eh + 1);
		error = tcp_lro_rx_ipv4(lc, m, ip4, &th);
		if (error != 0)
			return (error);
		ip_len = ntohs(ip4->ip_len);
		tcp_data_len = ip_len - sizeof(*ip4);
		break;
	}
#endif
	/* XXX-BZ what happens in case of VLAN(s)? */
	default:
		return (TCP_LRO_NOT_SUPPORTED);
	}

	/*
	 * If the frame is padded beyond the end of the IP packet, then we must
	 * trim the extra bytes off.
	 */
	l = m->m_pkthdr.len - (ETHER_HDR_LEN + ip_len);
	if (l != 0) {
		if (l < 0)
			/* Truncated packet. */
			return (TCP_LRO_CANNOT);

		m_adj(m, -l);
	}

	/*
	 * Check TCP header constraints.
	 */
	/* Ensure no bits set besides ACK or PSH. */
	if ((th->th_flags & ~(TH_ACK | TH_PUSH)) != 0)
		return (TCP_LRO_CANNOT);

	/* XXX-BZ We lose a AKC|PUSH flag concatinating multiple segments. */
	/* XXX-BZ Ideally we'd flush on PUSH? */

	/*
	 * Check for timestamps.
	 * Since the only option we handle are timestamps, we only have to
	 * handle the simple case of aligned timestamps.
	 */
	l = (th->th_off << 2);
	tcp_data_len -= l;
	l -= sizeof(*th);
	ts_ptr = (uint32_t *)(th + 1);
	if (l != 0 && (__predict_false(l != TCPOLEN_TSTAMP_APPA) ||
	    (*ts_ptr != ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
	    TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP))))
		return (TCP_LRO_CANNOT);

	/* If the driver did not pass in the checksum, set it now. */
	if (csum == 0x0000)
		csum = th->th_sum;

	seq = ntohl(th->th_seq);

	/* Try to find a matching previous segment. */
	SLIST_FOREACH(le, &lc->lro_active, next) {
		if (le->eh_type != eh_type)
			continue;
		if (le->source_port != th->th_sport ||
		    le->dest_port != th->th_dport)
			continue;
		switch (eh_type) {
#ifdef INET6
		case ETHERTYPE_IPV6:
			if (bcmp(&le->source_ip6, &ip6->ip6_src,
			    sizeof(struct in6_addr)) != 0 ||
			    bcmp(&le->dest_ip6, &ip6->ip6_dst,
			    sizeof(struct in6_addr)) != 0)
				continue;
			break;
#endif
#ifdef INET
		case ETHERTYPE_IP:
			if (le->source_ip4 != ip4->ip_src.s_addr ||
			    le->dest_ip4 != ip4->ip_dst.s_addr)
				continue;
			break;
#endif
		}

		/* Flush now if appending will result in overflow. */
		if (le->p_len > (65535 - tcp_data_len)) {
			SLIST_REMOVE(&lc->lro_active, le, lro_entry, next);
			tcp_lro_flush(lc, le);
			break;
		}

		/* Try to append the new segment. */
		if (__predict_false(seq != le->next_seq ||
		    (tcp_data_len == 0 && le->ack_seq == th->th_ack))) {
			/* Out of order packet or duplicate ACK. */
			SLIST_REMOVE(&lc->lro_active, le, lro_entry, next);
			tcp_lro_flush(lc, le);
			return (TCP_LRO_CANNOT);
		}

		if (l != 0) {
			uint32_t tsval = ntohl(*(ts_ptr + 1));
			/* Make sure timestamp values are increasing. */
			/* XXX-BZ flip and use TSTMP_GEQ macro for this? */
			if (__predict_false(le->tsval > tsval ||
			    *(ts_ptr + 2) == 0))
				return (TCP_LRO_CANNOT);
			le->tsval = tsval;
			le->tsecr = *(ts_ptr + 2);
		}

		le->next_seq += tcp_data_len;
		le->ack_seq = th->th_ack;
		le->window = th->th_win;
		le->append_cnt++;

#ifdef TCP_LRO_UPDATE_CSUM
		le->ulp_csum += tcp_lro_rx_csum_fixup(le, l3hdr, th,
		    tcp_data_len, ~csum);
#endif

		if (tcp_data_len == 0) {
			m_freem(m);
			return (0);
		}

		le->p_len += tcp_data_len;

		/*
		 * Adjust the mbuf so that m_data points to the first byte of
		 * the ULP payload.  Adjust the mbuf to avoid complications and
		 * append new segment to existing mbuf chain.
		 */
		m_adj(m, m->m_pkthdr.len - tcp_data_len);
		m_demote_pkthdr(m);

		le->m_tail->m_next = m;
		le->m_tail = m_last(m);

		/*
		 * If a possible next full length packet would cause an
		 * overflow, pro-actively flush now.
		 */
		if (le->p_len > (65535 - lc->ifp->if_mtu)) {
			SLIST_REMOVE(&lc->lro_active, le, lro_entry, next);
			tcp_lro_flush(lc, le);
		} else
			getmicrotime(&le->mtime);

		return (0);
	}

	/* Try to find an empty slot. */
	if (SLIST_EMPTY(&lc->lro_free))
		return (TCP_LRO_CANNOT);

	/* Start a new segment chain. */
	le = SLIST_FIRST(&lc->lro_free);
	SLIST_REMOVE_HEAD(&lc->lro_free, next);
	SLIST_INSERT_HEAD(&lc->lro_active, le, next);
	getmicrotime(&le->mtime);

	/* Start filling in details. */
	switch (eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		le->le_ip6 = ip6;
		le->source_ip6 = ip6->ip6_src;
		le->dest_ip6 = ip6->ip6_dst;
		le->eh_type = eh_type;
		le->p_len = m->m_pkthdr.len - ETHER_HDR_LEN - sizeof(*ip6);
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		le->le_ip4 = ip4;
		le->source_ip4 = ip4->ip_src.s_addr;
		le->dest_ip4 = ip4->ip_dst.s_addr;
		le->eh_type = eh_type;
		le->p_len = m->m_pkthdr.len - ETHER_HDR_LEN;
		break;
#endif
	}
	le->source_port = th->th_sport;
	le->dest_port = th->th_dport;

	le->next_seq = seq + tcp_data_len;
	le->ack_seq = th->th_ack;
	le->window = th->th_win;
	if (l != 0) {
		le->timestamp = 1;
		le->tsval = ntohl(*(ts_ptr + 1));
		le->tsecr = *(ts_ptr + 2);
	}

#ifdef TCP_LRO_UPDATE_CSUM
	/*
	 * Do not touch the csum of the first packet.  However save the
	 * "adjusted" checksum of just the source and destination addresses,
	 * the next header and the TCP payload.  The length and TCP header
	 * parts may change, so we remove those from the saved checksum and
	 * re-add with final values on tcp_lro_flush() if needed.
	 */
	KASSERT(le->ulp_csum == 0, ("%s: le=%p le->ulp_csum=0x%04x\n",
	    __func__, le, le->ulp_csum));

	le->ulp_csum = tcp_lro_rx_csum_fixup(le, l3hdr, th, tcp_data_len,
	    ~csum);
	th->th_sum = csum;	/* Restore checksum on first packet. */
#endif

	le->m_head = m;
	le->m_tail = m_last(m);

	return (0);
}

/* end */
