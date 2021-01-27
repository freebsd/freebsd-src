/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2007, Myricom Inc.
 * Copyright (c) 2008, Intel Corporation.
 * Copyright (c) 2012 The FreeBSD Foundation
 * Copyright (c) 2016 Mellanox Technologies.
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
#include <netinet6/ip6_var.h>

#include <machine/in_cksum.h>

static MALLOC_DEFINE(M_LRO, "LRO", "LRO control structures");

#define	TCP_LRO_UPDATE_CSUM	1
#ifndef	TCP_LRO_UPDATE_CSUM
#define	TCP_LRO_INVALID_CSUM	0x0000
#endif

static void	tcp_lro_rx_done(struct lro_ctrl *lc);
static int	tcp_lro_rx2(struct lro_ctrl *lc, struct mbuf *m,
		    uint32_t csum, int use_hash);

SYSCTL_NODE(_net_inet_tcp, OID_AUTO, lro,  CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "TCP LRO");

static long tcplro_stacks_wanting_mbufq = 0;
counter_u64_t tcp_inp_lro_direct_queue;
counter_u64_t tcp_inp_lro_wokeup_queue;
counter_u64_t tcp_inp_lro_compressed;
counter_u64_t tcp_inp_lro_single_push;
counter_u64_t tcp_inp_lro_locks_taken;
counter_u64_t tcp_inp_lro_sack_wake;
counter_u64_t tcp_extra_mbuf;
counter_u64_t tcp_would_have_but;
counter_u64_t tcp_comp_total;
counter_u64_t tcp_uncomp_total;
counter_u64_t tcp_csum_hardware;
counter_u64_t tcp_csum_hardware_w_ph;
counter_u64_t tcp_csum_software;


static unsigned	tcp_lro_entries = TCP_LRO_ENTRIES;
SYSCTL_UINT(_net_inet_tcp_lro, OID_AUTO, entries,
    CTLFLAG_RDTUN | CTLFLAG_MPSAFE, &tcp_lro_entries, 0,
    "default number of LRO entries");

SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, fullqueue, CTLFLAG_RD,
    &tcp_inp_lro_direct_queue, "Number of lro's fully queued to transport");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, wokeup, CTLFLAG_RD,
    &tcp_inp_lro_wokeup_queue, "Number of lro's where we woke up transport via hpts");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, compressed, CTLFLAG_RD,
    &tcp_inp_lro_compressed, "Number of lro's compressed and sent to transport");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, single, CTLFLAG_RD,
    &tcp_inp_lro_single_push, "Number of lro's sent with single segment");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, lockcnt, CTLFLAG_RD,
    &tcp_inp_lro_locks_taken, "Number of lro's inp_wlocks taken");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, sackwakeups, CTLFLAG_RD,
    &tcp_inp_lro_sack_wake, "Number of wakeups caused by sack/fin");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, extra_mbuf, CTLFLAG_RD,
    &tcp_extra_mbuf, "Number of times we had an extra compressed ack dropped into the tp");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, would_have_but, CTLFLAG_RD,
    &tcp_would_have_but, "Number of times we would have had an extra compressed but out of room");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, with_m_ackcmp, CTLFLAG_RD,
    &tcp_comp_total, "Number of mbufs queued with M_ACKCMP flags set");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, without_m_ackcmp, CTLFLAG_RD,
    &tcp_uncomp_total, "Number of mbufs queued without M_ACKCMP");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, csum_hw, CTLFLAG_RD,
    &tcp_csum_hardware, "Number of checksums processed in hardware");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, csum_hw_ph, CTLFLAG_RD,
    &tcp_csum_hardware_w_ph, "Number of checksums processed in hardware with pseudo header");
SYSCTL_COUNTER_U64(_net_inet_tcp_lro, OID_AUTO, csum_sw, CTLFLAG_RD,
    &tcp_csum_software, "Number of checksums processed in software");



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
	unsigned i, elements;

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
	if (lro_entries > lro_mbufs)
		elements = lro_entries;
	else
		elements = lro_mbufs;
	lc->lro_hash = phashinit_flags(elements, M_LRO, &lc->lro_hashsz,
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

static struct tcphdr *
tcp_lro_get_th(struct lro_entry *le, struct mbuf *m)
{
	struct ether_header *eh;
	struct tcphdr *th = NULL;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip4 = NULL;		/* Keep compiler happy. */
#endif

	eh = mtod(m, struct ether_header *);
	switch (le->eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(eh + 1);
		th = (struct tcphdr *)(ip6 + 1);
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		ip4 = (struct ip *)(eh + 1);
		th = (struct tcphdr *)(ip4 + 1);
		break;
#endif
	}
	return (th);
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

static void
tcp_lro_rx_done(struct lro_ctrl *lc)
{
	struct lro_entry *le;

	while ((le = LIST_FIRST(&lc->lro_active)) != NULL) {
		tcp_lro_active_remove(le);
		tcp_lro_flush(lc, le);
	}
}

void
tcp_lro_flush_inactive(struct lro_ctrl *lc, const struct timeval *timeout)
{
	struct lro_entry *le, *le_tmp;
	struct timeval tv;

	if (LIST_EMPTY(&lc->lro_active))
		return;

	getmicrouptime(&tv);
	timevalsub(&tv, timeout);
	LIST_FOREACH_SAFE(le, &lc->lro_active, next, le_tmp) {
		if (timevalcmp(&tv, &le->mtime, >=)) {
			tcp_lro_active_remove(le);
			tcp_lro_flush(lc, le);
		}
	}
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

static void
tcp_lro_log(struct tcpcb *tp, struct lro_ctrl *lc,
	    struct lro_entry *le, struct mbuf *m, int frm, int32_t tcp_data_len,
	    uint32_t th_seq , uint32_t th_ack, uint16_t th_win)
{
	if (tp->t_logstate != TCP_LOG_STATE_OFF) {
		union tcp_log_stackspecific log;
		struct timeval tv;
		uint32_t cts;

		cts = tcp_get_usecs(&tv);
		memset(&log, 0, sizeof(union tcp_log_stackspecific));
		log.u_bbr.flex8 = frm;
		log.u_bbr.flex1 = tcp_data_len;
		if (m)
			log.u_bbr.flex2 = m->m_pkthdr.len;
		else
			log.u_bbr.flex2 = 0;
		log.u_bbr.flex3 = le->append_cnt;
		log.u_bbr.flex4 = le->p_len;
		if (le->m_head) {
			log.u_bbr.flex5 = le->m_head->m_pkthdr.len;
			log.u_bbr.delRate = le->m_head->m_flags;
			log.u_bbr.rttProp = le->m_head->m_pkthdr.rcv_tstmp;
		}
		log.u_bbr.inflight = th_seq;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.epoch = le->next_seq;
		log.u_bbr.delivered = th_ack;
		log.u_bbr.lt_epoch = le->ack_seq;
		log.u_bbr.pacing_gain = th_win;
		log.u_bbr.cwnd_gain = le->window;
		log.u_bbr.cur_del_rate = (uintptr_t)m;
		log.u_bbr.bw_inuse = (uintptr_t)le->m_head;
		log.u_bbr.pkts_out = le->mbuf_cnt;	/* Total mbufs added */
		log.u_bbr.applimited = le->ulp_csum;
		log.u_bbr.lost = le->mbuf_appended;
		log.u_bbr.pkt_epoch = le->cmp_ack_cnt;
		log.u_bbr.flex6 = tcp_tv_to_usectick(&lc->lro_last_flush);
		if (in_epoch(net_epoch_preempt))
			log.u_bbr.inhpts = 1;
		else
			log.u_bbr.inhpts = 0;
		TCP_LOG_EVENTP(tp, NULL,
			       &tp->t_inpcb->inp_socket->so_rcv,
			       &tp->t_inpcb->inp_socket->so_snd,
			       TCP_LOG_LRO, 0,
			       0, &log, false, &tv);
	}
}

static void
tcp_flush_out_le(struct tcpcb *tp, struct lro_ctrl *lc, struct lro_entry *le)
{
	if (le->append_cnt > 1) {
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
			uint32_t cl;
			uint16_t c;

			ip4 = le->le_ip4;
			/* Fix IP header checksum for new length. */
			c = ~ip4->ip_sum;
			cl = c;
			c = ~ip4->ip_len;
			cl += c + p_len;
			while (cl > 0xffff)
				cl = (cl >> 16) + (cl & 0xffff);
			c = cl;
			ip4->ip_sum = ~c;
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
		/* Update the TCP header checksum. */
		le->ulp_csum += p_len;
		le->ulp_csum += tcp_lro_csum_th(th);
		while (le->ulp_csum > 0xffff)
			le->ulp_csum = (le->ulp_csum >> 16) +
			    (le->ulp_csum & 0xffff);
		th->th_sum = (le->ulp_csum & 0xffff);
		th->th_sum = ~th->th_sum;
	}
	/*
	 * Break any chain, this is not set to NULL on the singleton
	 * case m_nextpkt points to m_head. Other case set them
	 * m_nextpkt to NULL in push_and_replace.
	 */
	le->m_head->m_nextpkt = NULL;
	le->m_head->m_pkthdr.lro_nsegs = le->append_cnt;
	(*lc->ifp->if_input)(lc->ifp, le->m_head);
	lc->lro_queued += le->append_cnt;
}

static void
tcp_set_le_to_m(struct lro_ctrl *lc, struct lro_entry *le, struct mbuf *m)
{
	struct ether_header *eh;
	void *l3hdr = NULL;		/* Keep compiler happy. */
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip4 = NULL;		/* Keep compiler happy. */
#endif
	uint32_t *ts_ptr;
	int error, l, ts_failed = 0;
	uint16_t tcp_data_len;
	uint16_t csum;

	error = -1;
	eh = mtod(m, struct ether_header *);
	/*
	 * We must reset the other pointers since the mbuf
	 * we were pointing too is about to go away.
	 */
	switch (le->eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		l3hdr = ip6 = (struct ip6_hdr *)(eh + 1);
		error = tcp_lro_rx_ipv6(lc, m, ip6, &th);
		le->le_ip6 = ip6;
		le->source_ip6 = ip6->ip6_src;
		le->dest_ip6 = ip6->ip6_dst;
		le->p_len = m->m_pkthdr.len - ETHER_HDR_LEN - sizeof(*ip6);
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		l3hdr = ip4 = (struct ip *)(eh + 1);
		error = tcp_lro_rx_ipv4(lc, m, ip4, &th);
		le->le_ip4 = ip4;
		le->source_ip4 = ip4->ip_src.s_addr;
		le->dest_ip4 = ip4->ip_dst.s_addr;
		le->p_len = m->m_pkthdr.len - ETHER_HDR_LEN;
		break;
#endif
	}
	KASSERT(error == 0, ("%s: le=%p tcp_lro_rx_xxx failed\n",
				    __func__, le));
	ts_ptr = (uint32_t *)(th + 1);
	l = (th->th_off << 2);
	l -= sizeof(*th);
	if (l != 0 &&
	    (__predict_false(l != TCPOLEN_TSTAMP_APPA) ||
	     (*ts_ptr != ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
			       TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)))) {
		/* We have failed to find a timestamp some other option? */
		ts_failed = 1;
	}
	if ((l != 0) && (ts_failed == 0)) {
		le->timestamp = 1;
		le->tsval = ntohl(*(ts_ptr + 1));
		le->tsecr = *(ts_ptr + 2);
	} else
		le->timestamp = 0;
	le->source_port = th->th_sport;
	le->dest_port = th->th_dport;
	/* Pull out the csum */
	tcp_data_len = m->m_pkthdr.lro_len;
	le->next_seq = ntohl(th->th_seq) + tcp_data_len;
	le->ack_seq = th->th_ack;
	le->window = th->th_win;
	csum = th->th_sum;
	/* Setup the data pointers */
	le->m_head = m;
	le->m_tail = m_last(m);
	le->append_cnt = 0;
	le->ulp_csum = tcp_lro_rx_csum_fixup(le, l3hdr, th, tcp_data_len,
					     ~csum);
	le->append_cnt++;
	th->th_sum = csum;	/* Restore checksum on first packet. */
}

static void
tcp_push_and_replace(struct tcpcb *tp, struct lro_ctrl *lc, struct lro_entry *le, struct mbuf *m)
{
	/*
	 * Push up the stack the current le and replace
	 * it with m.
	 */
	struct mbuf *msave;

	/* Grab off the next and save it */
	msave = le->m_head->m_nextpkt;
	le->m_head->m_nextpkt = NULL;
	/* Now push out the old le entry */
	tcp_flush_out_le(tp, lc, le);
	/*
	 * Now to replace the data properly in the le
	 * we have to reset the tcp header and
	 * other fields.
	 */
	tcp_set_le_to_m(lc, le, m);
	/* Restore the next list */
	m->m_nextpkt = msave;
}

static void
tcp_lro_condense(struct tcpcb *tp, struct lro_ctrl *lc, struct lro_entry *le)
{
	/*
	 * Walk through the mbuf chain we
	 * have on tap and compress/condense
	 * as required.
	 */
	uint32_t *ts_ptr;
	struct mbuf *m;
	struct tcphdr *th;
	uint16_t tcp_data_len, csum_upd;
	int l;

	/*
	 * First we must check the lead (m_head)
	 * we must make sure that it is *not*
	 * something that should be sent up
	 * right away (sack etc).
	 */
again:

	m = le->m_head->m_nextpkt;
	if (m == NULL) {
		/* Just the one left */
		return;
	}
	if (m->m_flags & M_ACKCMP)
		panic("LRO condense lc:%p le:%p reaches with mbuf:%p ackcmp",
		      lc, le, m);
	th = tcp_lro_get_th(le, le->m_head);
	KASSERT(th != NULL,
		("le:%p m:%p th comes back NULL?", le, le->m_head));
	l = (th->th_off << 2);
	l -= sizeof(*th);
	ts_ptr = (uint32_t *)(th + 1);
	if (l != 0 && (__predict_false(l != TCPOLEN_TSTAMP_APPA) ||
		       (*ts_ptr != ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
					 TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)))) {
		/*
		 * Its not the timestamp. We can't
		 * use this guy as the head.
		 */
		le->m_head->m_nextpkt = m->m_nextpkt;
		tcp_push_and_replace(tp, lc, le, m);
		goto again;
	}
	if ((th->th_flags & ~(TH_ACK | TH_PUSH)) != 0) {
		/*
		 * Make sure that previously seen segements/ACKs are delivered
		 * before this segment, e.g. FIN.
		 */
		le->m_head->m_nextpkt = m->m_nextpkt;
		KASSERT(((m->m_flags & M_LRO_EHDRSTRP) == 0) ,
			("tp:%p mbuf:%p has stripped ethernet flags:0x%x", tp,  m, m->m_flags));
		tcp_push_and_replace(tp, lc, le, m);
		goto again;
	}
	while((m = le->m_head->m_nextpkt) != NULL) {
		/*
		 * condense m into le, first
		 * pull m out of the list.
		 */
		KASSERT(((m->m_flags & M_LRO_EHDRSTRP) == 0) ,
			("tp:%p mbuf:%p has stripped ethernet flags:0x%x", tp,  m, m->m_flags));
		KASSERT(((m->m_flags & M_ACKCMP) == 0),
			("LRO condense lc:%p le:%p reaches with mbuf:%p ackcmp", lc, le, m));
		le->m_head->m_nextpkt = m->m_nextpkt;
		m->m_nextpkt = NULL;
		/* Setup my data */
		tcp_data_len = m->m_pkthdr.lro_len;
		th = tcp_lro_get_th(le, m);
		KASSERT(th != NULL,
			("le:%p m:%p th comes back NULL?", le, m));
		ts_ptr = (uint32_t *)(th + 1);
		l = (th->th_off << 2);
		l -= sizeof(*th);
		if (le->append_cnt >= lc->lro_ackcnt_lim) {
			tcp_push_and_replace(tp, lc, le, m);
			goto again;
		}
		if (le->p_len > (lc->lro_length_lim - tcp_data_len)) {
			/* Flush now if appending will result in overflow. */
			tcp_push_and_replace(tp, lc, le, m);
			goto again;
		}
		if (l != 0 && (__predict_false(l != TCPOLEN_TSTAMP_APPA) ||
			       (*ts_ptr != ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
						 TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)))) {
			/*
			 * Maybe a sack in the new one? We need to
			 * start all over after flushing the
			 * current le. We will go up to the beginning
			 * and flush it (calling the replace again possibly
			 * or just returning).
			 */
			tcp_push_and_replace(tp, lc, le, m);
			goto again;
		}
		if ((th->th_flags & ~(TH_ACK | TH_PUSH)) != 0) {
			tcp_push_and_replace(tp, lc, le, m);
			goto again;
		}
		if (l != 0) {
			uint32_t tsval = ntohl(*(ts_ptr + 1));
			/* Make sure timestamp values are increasing. */
			if (TSTMP_GT(le->tsval, tsval))  {
				tcp_push_and_replace(tp, lc, le, m);
				goto again;
			}
			le->tsval = tsval;
			le->tsecr = *(ts_ptr + 2);
		}
		/* Try to append the new segment. */
		if (__predict_false(ntohl(th->th_seq) != le->next_seq ||
				    (tcp_data_len == 0 &&
				     le->ack_seq == th->th_ack &&
				     le->window == th->th_win))) {
			/* Out of order packet or duplicate ACK. */
			tcp_push_and_replace(tp, lc, le, m);
			goto again;
		}
		if (tcp_data_len || SEQ_GT(ntohl(th->th_ack), ntohl(le->ack_seq))) {
			le->next_seq += tcp_data_len;
			le->ack_seq = th->th_ack;
			le->window = th->th_win;
		} else if (th->th_ack == le->ack_seq) {
			le->window = WIN_MAX(le->window, th->th_win);
		}
		csum_upd = m->m_pkthdr.lro_csum;
		le->ulp_csum += csum_upd;
		if (tcp_data_len == 0) {
			le->append_cnt++;
			le->mbuf_cnt--;
			m_freem(m);
			continue;
		}
		le->append_cnt++;
		le->mbuf_appended++;
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
	}
}

#ifdef TCPHPTS
static void
tcp_queue_pkts(struct tcpcb *tp, struct lro_entry *le)
{
	if (tp->t_in_pkt == NULL) {
		/* Nothing yet there */
		tp->t_in_pkt = le->m_head;
		tp->t_tail_pkt = le->m_last_mbuf;
	} else {
		/* Already some there */
		tp->t_tail_pkt->m_nextpkt = le->m_head;
		tp->t_tail_pkt = le->m_last_mbuf;
	}
	le->m_head = NULL;
	le->m_last_mbuf = NULL;
}

static struct mbuf *
tcp_lro_get_last_if_ackcmp(struct lro_ctrl *lc, struct lro_entry *le, struct inpcb *inp)
{
	struct mbuf *m = NULL;
	struct tcpcb *tp;

	tp = intotcpcb(inp);
	if (tp) {
		/* Look at the last mbuf if any in queue */
		if ((tp->t_tail_pkt) &&
		    (tp->t_tail_pkt->m_flags & M_ACKCMP)) {
			if (M_TRAILINGSPACE(tp->t_tail_pkt) >= sizeof(struct tcp_ackent)) {
				tcp_lro_log(tp, lc, le, NULL, 23, 0, 0, 0, 0);
				m = tp->t_tail_pkt;
			} else {
				if ((inp->inp_flags2 & INP_MBUF_L_ACKS) == 0) {
					counter_u64_add(tcp_would_have_but, 1);
					inp->inp_flags2 |= INP_MBUF_L_ACKS;
				}
			}
		}
	}
	return (m);
}

static struct inpcb *
tcp_lro_lookup(struct lro_ctrl *lc, struct lro_entry *le)
{
	struct inpcb *inp = NULL;

	NET_EPOCH_ASSERT();
	switch (le->eh_type) {
#ifdef INET6
	case ETHERTYPE_IPV6:
		inp = in6_pcblookup(&V_tcbinfo, &le->source_ip6,
				    le->source_port, &le->dest_ip6,le->dest_port,
				    INPLOOKUP_WLOCKPCB,
				    lc->ifp);
		break;
#endif
#ifdef INET
	case ETHERTYPE_IP:
		inp = in_pcblookup(&V_tcbinfo, le->le_ip4->ip_src,
				   le->source_port, le->le_ip4->ip_dst, le->dest_port,
				   INPLOOKUP_WLOCKPCB,
				   lc->ifp);
		break;
#endif
	}
	return (inp);
}

#endif
#ifdef NO
static void
stack_guard_prep(uint32_t *sg, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		sg[i] = 0xdeadc0de;
	}
}

static void
stack_guard_check(struct lro_ctrl *lc, struct lro_entry *le, uint32_t *sg, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (sg[i] != 0xdeadc0de)
			panic("Stack guard fails sg[%d] = 0x%x le:%p lc:%p sg:%p\n",
			      i, sg[i], le, lc, sg);
	}
}
#endif

void
tcp_lro_flush(struct lro_ctrl *lc, struct lro_entry *le)
{
	struct tcpcb *tp = NULL;
#ifdef TCPHPTS
	struct inpcb *inp = NULL;
	int need_wakeup = 0, can_queue = 0;

	/* Now lets lookup the inp first */
	CURVNET_SET(lc->ifp->if_vnet);
	/*
	 * XXXRRS Currently the common input handler for
	 * mbuf queuing cannot handle VLAN Tagged. This needs
	 * to be fixed and the or condition removed (i.e. the
	 * common code should do the right lookup for the vlan
	 * tag and anything else that the vlan_input() does).
	 */
	if (le->m_head == NULL) {
		/*
		 * Everything was pushed up to the stack nothing to do
		 * but release the reference and be done.
		 */
		if (le->inp) {
			INP_WLOCK(le->inp);
			if (in_pcbrele_wlocked(le->inp) == 0) {
				/*
				 * We released it and still
				 * have the lock.
				 */
				INP_WUNLOCK(le->inp);
			}
		}
		goto done;
	}
	if ((tcplro_stacks_wanting_mbufq == 0) || (le->m_head->m_flags & M_VLANTAG))
		goto skip_lookup;

	if (le->inp == NULL) {
		le->inp = inp = tcp_lro_lookup(lc, le);
		if (inp && ((inp->inp_flags & (INP_DROPPED|INP_TIMEWAIT)) ||
			    (inp->inp_flags2 & INP_FREED))) {
			/*
			 * We can't present these to the inp since
			 * it will not support the stripped ethernet
			 * header that these have nor if a compressed
			 * ack is presnet.
			 */
			INP_WUNLOCK(inp);
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
		if ((le->flags & HAS_COMP_ENTRIES) &&
		    ((inp->inp_flags2 & INP_MBUF_ACKCMP) == 0)) {
			/*
			 * It swapped to off, must be a stack
			 * switch. We need to ditch all the packets
			 * and the peer will just have to retransmit.
			 */
			INP_WUNLOCK(inp);
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
	} else {
		/* We have a reference on the inp lets lock and release it */
		inp = le->inp;
		INP_WLOCK(inp);
		if (in_pcbrele_wlocked(inp)) {
			/*
			 * We lost the inp. We can't present these to the inp since
			 * it will not support the stripped off etherent header.
			 */
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
		if (inp && ((inp->inp_flags & (INP_DROPPED|INP_TIMEWAIT)) ||
			    (inp->inp_flags2 & INP_FREED))) {
			/*
			 * We can't present these to the inp since
			 * it may not support them.
			 */
			INP_WUNLOCK(inp);
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
		if ((le->flags & HAS_COMP_ENTRIES) &&
		    ((inp->inp_flags2 & INP_MBUF_ACKCMP) == 0)) {
			/*
			 * It swapped to off, must be a stack
			 * switch. We need to ditch all the packets
			 * and the peer will just have to retransmit.
			 */
			INP_WUNLOCK(inp);
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
	}
	if (inp && ((inp->inp_flags2 & INP_SUPPORTS_MBUFQ) ||
		    (inp->inp_flags2 & INP_MBUF_ACKCMP))) {
		/* The transport supports mbuf queuing */
		can_queue = 1;
		if (le->need_wakeup ||
		    ((inp->inp_in_input == 0) &&
		     ((inp->inp_flags2 & INP_MBUF_QUEUE_READY) == 0))) {
			/*
			 * Either the transport is off on a keep-alive
			 * (it has the queue_ready flag clear and its
			 *  not already been woken) or the entry has
			 * some urgent thing (FIN or possibly SACK blocks).
			 * This means we need to wake the transport up by
			 * putting it on the input pacer.
			 */
			need_wakeup = 1;
			if ((inp->inp_flags2 & INP_DONT_SACK_QUEUE) &&
			    (le->need_wakeup != 1)) {
				/*
				 * Prohibited from a sack wakeup.
				 */
				need_wakeup = 0;
			}
		}
		/* Do we need to be awoken due to lots of data or acks? */
		if ((le->tcp_tot_p_len >= lc->lro_length_lim) ||
		    (le->mbuf_cnt >= lc->lro_ackcnt_lim))
			need_wakeup = 1;
	}
	if (inp)
		tp = intotcpcb(inp);
	else
		tp = NULL;
	if (can_queue) {
		counter_u64_add(tcp_inp_lro_direct_queue, 1);
		tcp_lro_log(tp, lc, le, NULL, 22, need_wakeup,
			    inp->inp_flags2, inp->inp_in_input, le->need_wakeup);
		tcp_queue_pkts(tp, le);
		if (need_wakeup) {
			/*
			 * We must get the guy to wakeup via
			 * hpts.
			 */
			NET_EPOCH_ASSERT();
			if (le->need_wakeup == 2) {
				/*
				 * The value 2 is set if the
				 * options are unrecognized i.e.
				 * not just a timestamp. So really
				 * sack is usually what it is but
				 * it might be some other option (CWR
				 * etc).
				 */
				counter_u64_add(tcp_inp_lro_sack_wake, 1);
			}
			counter_u64_add(tcp_inp_lro_wokeup_queue, 1);
			if ((*tp->t_fb->tfb_do_queued_segments)(inp->inp_socket, tp, 0)) {
				inp = NULL;
			}
		}
	}
	if (inp) {
		/* Unlock it */
		tp = NULL;
		counter_u64_add(tcp_inp_lro_locks_taken, 1);
		INP_WUNLOCK(inp);
	}
	if (can_queue == 0) {
skip_lookup:
		if (le->strip_cnt) {
			/* 
			 * We have stripped mbufs, the connection
			 * must have changed underneath us. You
			 * loose the packets as a penalty.
			 */
			lro_free_mbuf_chain(le->m_head);
			goto done;
		}
#endif /* TCPHPTS */
		/* Old fashioned lro method */
		if (le->m_head != le->m_last_mbuf)  {
			counter_u64_add(tcp_inp_lro_compressed, 1);
			tcp_lro_condense(tp, lc, le);
		} else
			counter_u64_add(tcp_inp_lro_single_push, 1);
		tcp_flush_out_le(tp, lc, le);
#ifdef TCPHPTS
	}
done:
	CURVNET_RESTORE();
#endif
	lc->lro_flushed++;
	bzero(le, sizeof(*le));
	LIST_INSERT_HEAD(&lc->lro_free, le, next);
}

#ifdef HAVE_INLINE_FLSLL
#define	tcp_lro_msb_64(x) (1ULL << (flsll(x) - 1))
#else
static inline uint64_t
tcp_lro_msb_64(uint64_t x)
{
	x |= (x >> 1);
	x |= (x >> 2);
	x |= (x >> 4);
	x |= (x >> 8);
	x |= (x >> 16);
	x |= (x >> 32);
	return (x & ~(x >> 1));
}
#endif

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

	/* check if no mbufs to flush */
	if (lc->lro_mbuf_count == 0)
		goto done;

	microuptime(&lc->lro_last_flush);
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
		if (tcp_lro_rx2(lc, mb, 0, 0) != 0) {
			/* input packet to network layer */
			(*lc->ifp->if_input)(lc->ifp, mb);
			lc->lro_queued++;
			lc->lro_flushed++;
		}
	}
done:
	/* flush active streams */
	tcp_lro_rx_done(lc);

	lc->lro_mbuf_count = 0;
}

static void
lro_set_mtime(struct timeval *tv, struct timespec *ts)
{
	tv->tv_sec = ts->tv_sec;
	tv->tv_usec = ts->tv_nsec / 1000;
}

static void
build_ack_entry(struct tcp_ackent *ae, struct tcphdr *th, struct mbuf *m, uint16_t hdr_len, uint16_t iptos)
{
	/*
	 * Given a TCP ack, summarize it down into the small tcp
	 * ack entry.
	 */
	u_char *cp;


	KASSERT(((th->th_flags & ~(TH_ACK | TH_PUSH | TH_CWR | TH_ECE)) == 0),
		("tcphdr:%p mbuf:%p has unallowed bits %x", th, m, th->th_flags));
	ae->timestamp = m->m_pkthdr.rcv_tstmp;
	if (m->m_flags & M_TSTMP_LRO)
		ae->flags = TSTMP_LRO;
	else if (m->m_flags & M_TSTMP)
		ae->flags = TSTMP_HDWR;
	ae->seq = ntohl(th->th_seq);
	ae->ack = ntohl(th->th_ack);
	ae->flags |= th->th_flags;
	if (hdr_len) {
		/* We have a timestamp options get out the bits */
		cp = (u_char *)(th + 1);
		/* Skip the two NOP's at the front */
		while (*cp == TCPOPT_NOP)
			cp++;
		KASSERT(((*cp == TCPOPT_TIMESTAMP) &&
			 (cp[1] == TCPOLEN_TIMESTAMP)),
			("At %p in tcphdr:%p options of %d not timestamp",
			 cp, th, hdr_len));
		bcopy((char *)cp + 2,
		      (char *)&ae->ts_value, sizeof(uint32_t));
		ae->ts_value = ntohl(ae->ts_value);
		bcopy((char *)cp + 6,
		      (char *)&ae->ts_echo, sizeof(uint32_t));
		ae->ts_echo = ntohl(ae->ts_echo);
		ae->flags |= HAS_TSTMP;
	}
	ae->win = ntohs(th->th_win);
	ae->codepoint = iptos;
}

static struct mbuf *
do_bpf_and_csum(struct inpcb *inp, struct lro_ctrl *lc, struct lro_entry *le,
	struct ether_header *eh, struct mbuf *m, int bpf_req, int locked)
{
	/*
	 * Do TCP/IP checksum and BPF tap for either ACK_CMP packets or
	 * MBUF QUEUE type packets.
	 */
	struct tcphdr *th;
#ifdef INET6
	struct ip6_hdr *ip6 = NULL;	/* Keep compiler happy. */
#endif
#ifdef INET
	struct ip *ip = NULL;		/* Keep compiler happy. */
#endif

	uint16_t drop_hdrlen;
	int etype, tlen;
	uint8_t iptos;

	/* Let the BPF see the packet */
	if (bpf_req && lc->ifp)
		ETHER_BPF_MTAP(lc->ifp, m);
	/* Get type and Trim off the ethernet header */
	m->m_pkthdr.lro_etype = etype = ntohs(eh->ether_type);
	m_adj(m,  sizeof(*eh));
	m->m_flags |= M_LRO_EHDRSTRP;
	switch (etype) {
#ifdef INET6
	case ETHERTYPE_IPV6:
	{
		if (m->m_len < (sizeof(*ip6) + sizeof(*th))) {
			m = m_pullup(m, sizeof(*ip6) + sizeof(*th));
			if (m == NULL) {
				TCPSTAT_INC(tcps_rcvshort);
				m_freem(m);
				return (NULL);
			}
		}
		ip6 = (struct ip6_hdr *)(eh + 1);
		th = (struct tcphdr *)(ip6 + 1);
		tlen = ntohs(ip6->ip6_plen);
		drop_hdrlen = sizeof(*ip6);
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID_IPV6) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				counter_u64_add(tcp_csum_hardware_w_ph, 1);
				th->th_sum = m->m_pkthdr.csum_data;
			} else {
				counter_u64_add(tcp_csum_hardware, 1);
				th->th_sum = in6_cksum_pseudo(ip6, tlen,
							      IPPROTO_TCP, m->m_pkthdr.csum_data);
			}
			th->th_sum ^= 0xffff;
		} else {
			counter_u64_add(tcp_csum_software, 1);
			th->th_sum = in6_cksum(m, IPPROTO_TCP, drop_hdrlen, tlen);
		}
		if (th->th_sum) {
			TCPSTAT_INC(tcps_rcvbadsum);
			if (locked) {
				/* Log the bad news */
				struct tcpcb *tp = intotcpcb(inp);

				tcp_lro_log(tp, lc, le, m, 13, tlen, m->m_pkthdr.csum_flags, drop_hdrlen, th->th_sum);
			}
			m_freem(m);
			return (NULL);
		}




		/*
		 * Be proactive about unspecified IPv6 address in source.
		 * As we use all-zero to indicate unbounded/unconnected pcb,
		 * unspecified IPv6 address can be used to confuse us.
		 *
		 * Note that packets with unspecified IPv6 destination is
		 * already dropped in ip6_input.
		 */
		if (IN6_IS_ADDR_UNSPECIFIED(&ip6->ip6_src)) {
			/* XXX stat */
			m_freem(m);
			return (NULL);
		}
		break;
	}
#endif
#ifdef INET
	case ETHERTYPE_IP:
	{
		if (m->m_len < sizeof (struct tcpiphdr)) {
			if ((m = m_pullup(m, sizeof (struct tcpiphdr)))
			    == NULL) {
				TCPSTAT_INC(tcps_rcvshort);
				m_freem(m);
				return (NULL);
			}
		}
		ip = (struct ip *)(eh + 1);
		th = (struct tcphdr *)(ip + 1);
		iptos = ip->ip_tos;
		drop_hdrlen = sizeof(*ip);
		tlen = ntohs(ip->ip_len) - sizeof(struct ip);
		if (m->m_pkthdr.csum_flags & CSUM_DATA_VALID) {
			if (m->m_pkthdr.csum_flags & CSUM_PSEUDO_HDR) {
				counter_u64_add(tcp_csum_hardware_w_ph, 1);
				th->th_sum = m->m_pkthdr.csum_data;
			} else {
				counter_u64_add(tcp_csum_hardware, 1);
				th->th_sum = in_pseudo(ip->ip_src.s_addr,
						       ip->ip_dst.s_addr,
						       htonl(m->m_pkthdr.csum_data + tlen +
							     IPPROTO_TCP));
			}
			th->th_sum ^= 0xffff;
		} else {
			int len;
			struct ipovly *ipov = (struct ipovly *)ip;
			/*
			 * Checksum extended TCP header and data.
			 */
			counter_u64_add(tcp_csum_software, 1);
			len = drop_hdrlen + tlen;
			bzero(ipov->ih_x1, sizeof(ipov->ih_x1));
			ipov->ih_len = htons(tlen);
			th->th_sum = in_cksum(m, len);
			/* Reset length for SDT probes. */
			ip->ip_len = htons(len);
			/* Reset TOS bits */
			ip->ip_tos = iptos;
			/* Re-initialization for later version check */
			ip->ip_v = IPVERSION;
			ip->ip_hl = sizeof(*ip) >> 2;
		}
		if (th->th_sum) {
			TCPSTAT_INC(tcps_rcvbadsum);
			if (locked) {
				/* Log the bad news */
				struct tcpcb *tp = intotcpcb(inp);

				tcp_lro_log(tp, lc, le, m, 13, tlen, m->m_pkthdr.csum_flags, drop_hdrlen, th->th_sum);
			}
			m_freem(m);
			return (NULL);
		}
		break;
	}
#endif
	} /* end switch */
	return (m);
}

static int
tcp_lro_rx2(struct lro_ctrl *lc, struct mbuf *m, uint32_t csum, int use_hash)
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
	int error, ip_len, hdr_len, locked = 0;
	uint16_t eh_type, tcp_data_len, need_flush;
#ifdef TCPHPTS
	uint16_t iptos;
#endif
	struct lro_head *bucket;
	struct timespec arrv;

	/* Clear the flags we may use to communicate with TCP */
	m->m_flags &= ~(M_ACKCMP|M_LRO_EHDRSTRP);

	/* We expect a contiguous header [eh, ip, tcp]. */
	if ((m->m_flags & (M_TSTMP_LRO|M_TSTMP)) == 0) {
		/* If no hardware or arrival stamp on the packet add arrival */
		nanouptime(&arrv);
		m->m_pkthdr.rcv_tstmp = (arrv.tv_sec * 1000000000) + arrv.tv_nsec;
		m->m_flags |= M_TSTMP_LRO;
	}
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
#ifdef TCPHPTS
		iptos = (ntohl(ip6->ip6_flow) >> 20) & 0xff;
#endif
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
#ifdef TCPHPTS
		iptos = ip4->ip_tos;
#endif
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
	hdr_len = m->m_pkthdr.len - (ETHER_HDR_LEN + ip_len);
	if (hdr_len != 0) {
		if (hdr_len < 0)
			/* Truncated packet. */
			return (TCP_LRO_CANNOT);

		m_adj(m, -hdr_len);
	}
	/*
	 * Check TCP header constraints.
	 */
	hdr_len = (th->th_off << 2);
	ts_ptr = (uint32_t *)(th + 1);
	tcp_data_len -= hdr_len;
	hdr_len -= sizeof(*th);
	if (th->th_flags & TH_SYN)
		return (TCP_LRO_CANNOT);
	if ((th->th_flags & ~(TH_ACK | TH_PUSH)) != 0) {
		need_flush = 1;
	} else
		need_flush = 0;
	if (hdr_len != 0 && (__predict_false(hdr_len != TCPOLEN_TSTAMP_APPA) ||
			     (*ts_ptr != ntohl(TCPOPT_NOP<<24|TCPOPT_NOP<<16|
					 TCPOPT_TIMESTAMP<<8|TCPOLEN_TIMESTAMP)))) {
		/*
		 * We have an option besides Timestamps, maybe
		 * it is a sack (most likely) which means we
		 * will probably need to wake up a sleeper (if
		 * the guy does queueing).
		 */
		need_flush = 2;
	}
	/* If the driver did not pass in the checksum, set it now. */
	if (csum == 0x0000)
		csum = th->th_sum;
	seq = ntohl(th->th_seq);
	if (!use_hash) {
		bucket = &lc->lro_hash[0];
	} else if (M_HASHTYPE_ISHASH(m)) {
		bucket = &lc->lro_hash[m->m_pkthdr.flowid % lc->lro_hashsz];
	} else {
		uint32_t hash;

		switch (eh_type) {
#ifdef INET
		case ETHERTYPE_IP:
			hash = ip4->ip_src.s_addr + ip4->ip_dst.s_addr;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			hash = ip6->ip6_src.s6_addr32[0] +
				ip6->ip6_dst.s6_addr32[0];
			hash += ip6->ip6_src.s6_addr32[1] +
				ip6->ip6_dst.s6_addr32[1];
			hash += ip6->ip6_src.s6_addr32[2] +
				ip6->ip6_dst.s6_addr32[2];
			hash += ip6->ip6_src.s6_addr32[3] +
				ip6->ip6_dst.s6_addr32[3];
			break;
#endif
		default:
			hash = 0;
			break;
		}
		hash += th->th_sport + th->th_dport;
		bucket = &lc->lro_hash[hash % lc->lro_hashsz];
	}

	/* Try to find a matching previous segment. */
	LIST_FOREACH(le, bucket, hash_next) {
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
		if (tcp_data_len || SEQ_GT(ntohl(th->th_ack), ntohl(le->ack_seq)) ||
		    (th->th_ack == le->ack_seq)) {
			m->m_pkthdr.lro_len = tcp_data_len;
		} else {
			/* no data and old ack */
			m_freem(m);
			return (0);
		}
#ifdef TCPHPTS
		if ((tcplro_stacks_wanting_mbufq == 0) || (m->m_flags & M_VLANTAG))
			goto skip_lookup_a;
		if (le->inp == NULL) {
			CURVNET_SET(lc->ifp->if_vnet);
			le->inp = tcp_lro_lookup(lc, le);
			if (le->inp) {
				in_pcbref(le->inp);
				locked = 1;
			}
			CURVNET_RESTORE();
		} else if (le->inp) {
			INP_WLOCK(le->inp);
			locked = 1;
		}
		if (locked && ((le->inp->inp_flags & (INP_DROPPED|INP_TIMEWAIT)) ||
			       (le->inp->inp_flags2 & INP_FREED))) {
			/* We can't present these to the inp since
			 * its dead Jim.
			 */
			int ret;

			ret = in_pcbrele_wlocked(le->inp);
			if (ret == 0)
				INP_WUNLOCK(le->inp);
			le->inp = NULL;
			locked = 0;
			tcp_lro_active_remove(le);
			if (le->strip_cnt && le->m_head) {
				/* 
				 * If we have any stripped packets we
				 * just dump the whole chain. The
				 * tcp_lro_flush code knows how
				 * to handle things when le->m_head is NULL
				 * and even le->inp is NULL.
				 */
				lro_free_mbuf_chain(le->m_head);
				le->m_head = NULL;
			}
			tcp_lro_flush(lc, le);
			return (TCP_LRO_CANNOT);
		}
		/* See if it has been switched on */
		if (le->inp && (le->inp->inp_flags2 & INP_MBUF_ACKCMP))
			le->flags |= CAN_USE_ACKCMP;

		if ((need_flush == 1) &&
		    le->inp &&
		    (le->inp->inp_flags2 & (INP_MBUF_ACKCMP|INP_SUPPORTS_MBUFQ)) &&
		    ((th->th_flags & ~(TH_ACK | TH_PUSH | TH_ECE | TH_CWR)) == 0)) {
			/*
			 * For MBUF queuing or ACKCMP we can accept ECE and CWR
			 * since each packet is sent to the transport (or the
			 * compressed state including the ECN bits).
			 */
			need_flush = 0;
		}
skip_lookup_a:
#endif
		if (need_flush)
			le->need_wakeup = need_flush;
		/* Save of the data only csum */
		m->m_pkthdr.rcvif = lc->ifp;
		m->m_pkthdr.lro_csum = tcp_lro_rx_csum_fixup(le, l3hdr, th,
						      tcp_data_len, ~csum);
		th->th_sum = csum;	/* Restore checksum */
#ifdef TCPHPTS
		if ((le->flags & CAN_USE_ACKCMP) ||
		    (le->inp &&
		     (le->inp->inp_flags2 & (INP_MBUF_ACKCMP|INP_SUPPORTS_MBUFQ)))) {
			/*
			 * Mbuf queued and ACKCMP packets have their BPF and csum
			 * done here in LRO. They will still end up looking at the
			 * headers and such (IP/TCP) but we don't want to proceed
			 * with any bad csum!
			 */
			m = do_bpf_and_csum(le->inp, lc, le, eh, m, bpf_peers_present(lc->ifp->if_bpf), locked);
			if (m == NULL) {
				/* Bad csum, accounting already done  */
				if (locked) {
					INP_WUNLOCK(le->inp);
				}
				return (0);
			}
			le->strip_cnt++;
		}
		if ((need_flush == 0) &&
		    (th->th_flags & TH_ACK) &&
		    (tcp_data_len == 0) &&
		    (le->flags & CAN_USE_ACKCMP)) {
			/*
			 * Ok this is a pure ack lets find out if our
			 * last packet already has one of these.
			 */
			struct mbuf *nm;
			struct tcp_ackent *ack_ent;
			int idx;

			INP_WLOCK_ASSERT(le->inp);
			if (le->m_head == NULL) {
				/* Ok can we still use the end of the inp's? */
				nm = tcp_lro_get_last_if_ackcmp(lc, le, le->inp);
				if (nm == NULL) {
					/* gone or full */
					goto new_one;
				}
				/* We can add in to the one on the tail */
				ack_ent = mtod(nm, struct tcp_ackent *);
				idx = (nm->m_len / sizeof(struct tcp_ackent));
				build_ack_entry(&ack_ent[idx], th, m, hdr_len, iptos);
				/* Bump the size of both pkt-hdr and len */
				nm->m_len += sizeof(struct tcp_ackent);
				nm->m_pkthdr.len += sizeof(struct tcp_ackent);
				le->ack_seq = th->th_ack;
				le->window = th->th_win;
				m_freem(m);
				counter_u64_add(tcp_extra_mbuf, 1);
				INP_WUNLOCK(le->inp);
				return (0);
			} else if (le->m_last_mbuf->m_flags & M_ACKCMP) {
				/* Yes we might be able to be appended to */
				nm = le->m_last_mbuf;
				if (M_TRAILINGSPACE(nm) < sizeof(struct tcp_ackent)) {
					if ((le->inp->inp_flags2 & INP_MBUF_L_ACKS) == 0) {
						counter_u64_add(tcp_would_have_but, 1);
						le->inp->inp_flags2 |= INP_MBUF_L_ACKS;
					}
					goto new_one;
				}
				/* we have room */
				ack_ent = mtod(nm, struct tcp_ackent *);
				idx = (nm->m_len / sizeof(struct tcp_ackent));
				build_ack_entry(&ack_ent[idx], th, m, hdr_len, iptos);
				/* Bump the size of both pkt-hdr and len */
				nm->m_len += sizeof(struct tcp_ackent);
				nm->m_pkthdr.len += sizeof(struct tcp_ackent);
				m_freem(m);
				le->flags |= HAS_COMP_ENTRIES;
				le->cmp_ack_cnt++;
				goto compressed;
			} else {
				/* Nope we need a new one */
new_one:
				if (le->inp->inp_flags2 & INP_MBUF_L_ACKS)
					nm = m_getcl(M_NOWAIT, MT_DATA, (M_ACKCMP|M_PKTHDR));
				else {
					nm = m_gethdr(M_NOWAIT, MT_DATA);
					nm->m_flags |= M_ACKCMP;
				}
				if (nm) {
					nm->m_pkthdr.rcvif = lc->ifp;
					ack_ent = mtod(nm, struct tcp_ackent *);
					build_ack_entry(ack_ent, th, m, hdr_len, iptos);
					m_freem(m);
					m = nm;
					m->m_pkthdr.len = m->m_len = sizeof(struct tcp_ackent);
					le->flags |= HAS_COMP_ENTRIES;
					le->cmp_ack_cnt++;
				}
				/* We fall through and append */
			}
		}
		if (m->m_flags & M_ACKCMP) {
			counter_u64_add(tcp_comp_total, 1);
		} else {
			counter_u64_add(tcp_uncomp_total, 1);
		}
#endif
		/* Save off the tail I am appending too (prev) */
		m->m_nextpkt = NULL;
		if (le->m_head == NULL) {
			/*
			 * Case where we wer chaining off the inp
			 * and now no-longer can.
			 */
			le->m_head = m;
			le->m_tail = m_last(m);
			le->m_last_mbuf = m;
			le->m_prev_last = NULL;
		} else {
			le->m_prev_last = le->m_last_mbuf;
			/* Mark me in the last spot */
			le->m_last_mbuf->m_nextpkt = m;
			/* Now set the tail to me  */
			le->m_last_mbuf = m;
			le->tcp_tot_p_len += tcp_data_len;
		}
#ifdef TCPHPTS
compressed:
#endif
		le->mbuf_cnt++;
		/* Add to the total size of data */
		lro_set_mtime(&le->mtime, &arrv);
		if (locked)
			INP_WUNLOCK(le->inp);
		return (0);
	}
	/* Try to find an empty slot. */
	if (LIST_EMPTY(&lc->lro_free))
		return (TCP_LRO_NO_ENTRIES);

	/* Start a new segment chain. */
	le = LIST_FIRST(&lc->lro_free);
	LIST_REMOVE(le, next);
	tcp_lro_active_insert(lc, bucket, le);
	lro_set_mtime(&le->mtime, &arrv);

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
	if (hdr_len != 0) {
		le->timestamp = 1;
		le->tsval = ntohl(*(ts_ptr + 1));
		le->tsecr = *(ts_ptr + 2);
	}
	KASSERT(le->ulp_csum == 0, ("%s: le=%p le->ulp_csum=0x%04x\n",
				    __func__, le, le->ulp_csum));

	le->append_cnt = 0;
	le->ulp_csum = tcp_lro_rx_csum_fixup(le, l3hdr, th, tcp_data_len,
					     ~csum);
	le->append_cnt++;
	th->th_sum = csum;	/* Restore checksum */
	m->m_pkthdr.rcvif = lc->ifp;
	m->m_pkthdr.lro_len = tcp_data_len;
	le->mbuf_cnt = 1;
	le->cmp_ack_cnt = 0;
	le->flags = 0;
#ifdef TCPHPTS
	/*
	 * Lets find out if we can use the mbuf-compression.
	 */
	if ((tcplro_stacks_wanting_mbufq == 0) || (m->m_flags & M_VLANTAG))
		goto skip_lookup_b;
	CURVNET_SET(lc->ifp->if_vnet);
	le->inp = tcp_lro_lookup(lc, le);
	if (le->inp && ((le->inp->inp_flags & (INP_DROPPED|INP_TIMEWAIT)) ||
			(le->inp->inp_flags2 & INP_FREED))) {
		INP_WUNLOCK(le->inp);
		le->inp = NULL;
	}
	if (le->inp) {
		if ((need_flush == 1) &&
		    (le->inp->inp_flags2 & (INP_MBUF_ACKCMP|INP_SUPPORTS_MBUFQ)) &&
		    ((th->th_flags & ~(TH_ACK | TH_PUSH | TH_ECE | TH_CWR)) == 0)) {
			/*
			 * For MBUF queuing or ACKCMP we can accept ECE and CWR
			 * since each packet is sent to the transport (or the
			 * compressed state including the ECN bits).
			 */
			need_flush = 0;
		}
		locked = 1;
		if (le->inp->inp_flags2 & INP_MBUF_ACKCMP)
			le->flags |= CAN_USE_ACKCMP;
		if ((le->flags & CAN_USE_ACKCMP) ||
		    (le->inp &&
		     (le->inp->inp_flags2 & (INP_MBUF_ACKCMP|INP_SUPPORTS_MBUFQ)))) {
			m = do_bpf_and_csum(le->inp, lc, le, eh, m, bpf_peers_present(lc->ifp->if_bpf), locked);
			if (m == NULL) {
				/* Bad csum, accounting already done  */
				INP_WUNLOCK(le->inp);
				le->inp = NULL;
				return (0);
			}
			le->strip_cnt++;
		}
		in_pcbref(le->inp);
	}
	CURVNET_RESTORE();
	if ((need_flush == 0) &&
	    (th->th_flags & TH_ACK) &&
	    (tcp_data_len == 0) &&
	    (le->flags & CAN_USE_ACKCMP)) {
		/* Ok this is a pure ack lets build our special COMPRESS mbuf */
		struct mbuf *nm;
		struct tcp_ackent *ack_ent;

		/* Question what is going on with the last mbuf on the inp queue, can we use it? */
		INP_WLOCK_ASSERT(le->inp);
		nm = tcp_lro_get_last_if_ackcmp(lc, le, le->inp);
		if (nm) {
			int idx;

			/* We can add in to the one on the tail */
			ack_ent = mtod(nm, struct tcp_ackent *);
			idx = (nm->m_len / sizeof(struct tcp_ackent));
			build_ack_entry(&ack_ent[idx], th, m, hdr_len, iptos);
			nm->m_len += sizeof(struct tcp_ackent);
			nm->m_pkthdr.len += sizeof(struct tcp_ackent);
			le->ack_seq = th->th_ack;
			le->window = th->th_win;
			m_freem(m);
			counter_u64_add(tcp_extra_mbuf, 1);
			le->m_head = NULL;
			le->m_tail = NULL;
			le->m_last_mbuf = NULL;
			le->m_prev_last = NULL;
			INP_WUNLOCK(le->inp);
			return (0);
		} else {
			if (le->inp->inp_flags2 & INP_MBUF_L_ACKS)
				nm = m_getcl(M_NOWAIT, MT_DATA, (M_ACKCMP|M_PKTHDR));
			else {
				nm = m_gethdr(M_NOWAIT, MT_DATA);
				nm->m_flags |= M_ACKCMP;
			}
			if (nm) {
				nm->m_pkthdr.rcvif = lc->ifp;
				ack_ent = mtod(nm, struct tcp_ackent *);
				build_ack_entry(ack_ent, th, m, hdr_len, iptos);
				m_freem(m);
				m = nm;
				m->m_pkthdr.len = m->m_len = sizeof(struct tcp_ackent);
				le->flags |= HAS_COMP_ENTRIES;
				le->cmp_ack_cnt++;
			}
		}
	}
	if (m->m_flags & M_ACKCMP) {
		counter_u64_add(tcp_comp_total, 1);
	} else {
		counter_u64_add(tcp_uncomp_total, 1);
	}
skip_lookup_b:
#endif
	if (need_flush)
		le->need_wakeup = need_flush;
	else
		le->need_wakeup = 0;
	m->m_nextpkt = NULL;
 	le->m_head = m;
	le->m_tail = m_last(m);
	le->m_last_mbuf = m;
	le->m_prev_last = NULL;
	/*
	 * We keep the total size here for cross checking when we may need
	 * to flush/wakeup in the MBUF_QUEUE case.
	 */
	le->tcp_tot_p_len = tcp_data_len;
	if (locked)
		INP_WUNLOCK(le->inp);
	return (0);
}

int
tcp_lro_rx(struct lro_ctrl *lc, struct mbuf *m, uint32_t csum)
{

	return tcp_lro_rx2(lc, m, csum, 1);
}

void
tcp_lro_queue_mbuf(struct lro_ctrl *lc, struct mbuf *mb)
{
	struct timespec arrv;

	/* sanity checks */
	if (__predict_false(lc->ifp == NULL || lc->lro_mbuf_data == NULL ||
	    lc->lro_mbuf_max == 0)) {
		/* packet drop */
		m_freem(mb);
		return;
	}

	/* check if packet is not LRO capable */
	if (__predict_false(mb->m_pkthdr.csum_flags == 0 ||
	    (lc->ifp->if_capenable & IFCAP_LRO) == 0)) {
		/* input packet to network layer */
		(*lc->ifp->if_input) (lc->ifp, mb);
		return;
	}
	/* Arrival Stamp the packet */

	if ((mb->m_flags & M_TSTMP) == 0) {
		/* If no hardware or arrival stamp on the packet add arrival */
		nanouptime(&arrv);
		mb->m_pkthdr.rcv_tstmp = ((arrv.tv_sec * 1000000000) +
			                  arrv.tv_nsec);
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
