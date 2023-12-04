/*-
 * Copyright (c) 2016-2018 Netflix, Inc.
 * Copyright (c) 2016-2021 Mellanox Technologies.
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
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/vnet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/infiniband.h>
#include <net/if_lagg.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet6/in6_pcb.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_hpts.h>
#include <netinet/tcp_log_buf.h>

static void
build_ack_entry(struct tcp_ackent *ae, struct tcphdr *th, struct mbuf *m,
    uint32_t *ts_ptr, uint16_t iptos)
{
	/*
	 * Given a TCP ACK, summarize it down into the small TCP ACK
	 * entry.
	 */
	ae->timestamp = m->m_pkthdr.rcv_tstmp;
	ae->flags = 0;
	if (m->m_flags & M_TSTMP_LRO)
		ae->flags |= TSTMP_LRO;
	else if (m->m_flags & M_TSTMP)
		ae->flags |= TSTMP_HDWR;
	ae->seq = ntohl(th->th_seq);
	ae->ack = ntohl(th->th_ack);
	ae->flags |= tcp_get_flags(th);
	if (ts_ptr != NULL) {
		ae->ts_value = ntohl(ts_ptr[1]);
		ae->ts_echo = ntohl(ts_ptr[2]);
		ae->flags |= HAS_TSTMP;
	}
	ae->win = ntohs(th->th_win);
	ae->codepoint = iptos;
}

static inline bool
tcp_lro_ack_valid(struct mbuf *m, struct tcphdr *th, uint32_t **ppts, bool *other_opts)
{
	/*
	 * This function returns two bits of valuable information.
	 * a) Is what is present capable of being ack-compressed,
	 *    we can ack-compress if there is no options or just
	 *    a timestamp option, and of course the th_flags must
	 *    be correct as well.
	 * b) Our other options present such as SACK. This is
	 *    used to determine if we want to wakeup or not.
	 */
	bool ret = true;

	switch (th->th_off << 2) {
	case (sizeof(*th) + TCPOLEN_TSTAMP_APPA):
		*ppts = (uint32_t *)(th + 1);
		/* Check if we have only one timestamp option. */
		if (**ppts == TCP_LRO_TS_OPTION)
			*other_opts = false;
		else {
			*other_opts = true;
			ret = false;
		}
		break;
	case (sizeof(*th)):
		/* No options. */
		*ppts = NULL;
		*other_opts = false;
		break;
	default:
		*ppts = NULL;
		*other_opts = true;
		ret = false;
		break;
	}
	/* For ACKCMP we only accept ACK, PUSH, ECE and CWR. */
	if ((tcp_get_flags(th) & ~(TH_ACK | TH_PUSH | TH_ECE | TH_CWR)) != 0)
		ret = false;
	/* If it has data on it we cannot compress it */
	if (m->m_pkthdr.lro_tcp_d_len)
		ret = false;

	/* ACK flag must be set. */
	if (!(tcp_get_flags(th) & TH_ACK))
		ret = false;
	return (ret);
}

static bool
tcp_lro_check_wake_status(struct tcpcb *tp)
{

	if (tp->t_fb->tfb_early_wake_check != NULL)
		return ((tp->t_fb->tfb_early_wake_check)(tp));
	return (false);
}

static void
tcp_lro_log(struct tcpcb *tp, const struct lro_ctrl *lc,
    const struct lro_entry *le, const struct mbuf *m,
    int frm, int32_t tcp_data_len, uint32_t th_seq,
    uint32_t th_ack, uint16_t th_win)
{
	if (tcp_bblogging_on(tp)) {
		union tcp_log_stackspecific log;
		struct timeval tv, btv;
		uint32_t cts;

		cts = tcp_get_usecs(&tv);
		memset(&log, 0, sizeof(union tcp_log_stackspecific));
		log.u_bbr.flex8 = frm;
		log.u_bbr.flex1 = tcp_data_len;
		if (m)
			log.u_bbr.flex2 = m->m_pkthdr.len;
		else
			log.u_bbr.flex2 = 0;
		if (le->m_head) {
			log.u_bbr.flex3 = le->m_head->m_pkthdr.lro_nsegs;
			log.u_bbr.flex4 = le->m_head->m_pkthdr.lro_tcp_d_len;
			log.u_bbr.flex5 = le->m_head->m_pkthdr.len;
			log.u_bbr.delRate = le->m_head->m_flags;
			log.u_bbr.rttProp = le->m_head->m_pkthdr.rcv_tstmp;
		}
		log.u_bbr.inflight = th_seq;
		log.u_bbr.delivered = th_ack;
		log.u_bbr.timeStamp = cts;
		log.u_bbr.epoch = le->next_seq;
		log.u_bbr.lt_epoch = le->ack_seq;
		log.u_bbr.pacing_gain = th_win;
		log.u_bbr.cwnd_gain = le->window;
		log.u_bbr.lost = curcpu;
		log.u_bbr.cur_del_rate = (uintptr_t)m;
		log.u_bbr.bw_inuse = (uintptr_t)le->m_head;
		bintime2timeval(&lc->lro_last_queue_time, &btv);
		log.u_bbr.flex6 = tcp_tv_to_usectick(&btv);
		log.u_bbr.flex7 = le->compressed;
		log.u_bbr.pacing_gain = le->uncompressed;
		if (in_epoch(net_epoch_preempt))
			log.u_bbr.inhpts = 1;
		else
			log.u_bbr.inhpts = 0;
		TCP_LOG_EVENTP(tp, NULL, &tptosocket(tp)->so_rcv,
		    &tptosocket(tp)->so_snd,
		    TCP_LOG_LRO, 0, 0, &log, false, &tv);
	}
}

static struct mbuf *
tcp_lro_get_last_if_ackcmp(struct lro_ctrl *lc, struct lro_entry *le,
    struct tcpcb *tp, int32_t *new_m, bool can_append_old_cmp)
{
	struct mbuf *m;

	/* Look at the last mbuf if any in queue */
	if (can_append_old_cmp) {
		m = STAILQ_LAST(&tp->t_inqueue, mbuf, m_stailqpkt);
		if (m != NULL && (m->m_flags & M_ACKCMP) != 0) {
			if (M_TRAILINGSPACE(m) >= sizeof(struct tcp_ackent)) {
				tcp_lro_log(tp, lc, le, NULL, 23, 0, 0, 0, 0);
				*new_m = 0;
				counter_u64_add(tcp_extra_mbuf, 1);
				return (m);
			} else {
				/* Mark we ran out of space */
				tp->t_flags2 |= TF2_MBUF_L_ACKS;
			}
		}
	}
	/* Decide mbuf size. */
	tcp_lro_log(tp, lc, le, NULL, 21, 0, 0, 0, 0);
	if (tp->t_flags2 & TF2_MBUF_L_ACKS)
		m = m_getcl(M_NOWAIT, MT_DATA, M_ACKCMP | M_PKTHDR);
	else
		m = m_gethdr(M_NOWAIT, MT_DATA);

	if (__predict_false(m == NULL)) {
		counter_u64_add(tcp_would_have_but, 1);
		return (NULL);
	}
	counter_u64_add(tcp_comp_total, 1);
	m->m_pkthdr.rcvif = lc->ifp;
	m->m_flags |= M_ACKCMP;
	*new_m = 1;
	return (m);
}

/*
 * Do BPF tap for either ACK_CMP packets or MBUF QUEUE type packets
 * and strip all, but the IPv4/IPv6 header.
 */
static bool
do_bpf_strip_and_compress(struct tcpcb *tp, struct lro_ctrl *lc,
    struct lro_entry *le, struct mbuf **pp, struct mbuf **cmp,
    struct mbuf **mv_to, bool *should_wake, bool bpf_req, bool lagg_bpf_req,
    struct ifnet *lagg_ifp, bool can_append_old_cmp)
{
	union {
		void *ptr;
		struct ip *ip4;
		struct ip6_hdr *ip6;
	} l3;
	struct mbuf *m;
	struct mbuf *nm;
	struct tcphdr *th;
	struct tcp_ackent *ack_ent;
	uint32_t *ts_ptr;
	int32_t n_mbuf;
	bool other_opts, can_compress;
	uint8_t lro_type;
	uint16_t iptos;
	int tcp_hdr_offset;
	int idx;

	/* Get current mbuf. */
	m = *pp;

	/* Let the BPF see the packet */
	if (__predict_false(bpf_req))
		ETHER_BPF_MTAP(lc->ifp, m);

	if (__predict_false(lagg_bpf_req))
		ETHER_BPF_MTAP(lagg_ifp, m);

	tcp_hdr_offset = m->m_pkthdr.lro_tcp_h_off;
	lro_type = le->inner.data.lro_type;
	switch (lro_type) {
	case LRO_TYPE_NONE:
		lro_type = le->outer.data.lro_type;
		switch (lro_type) {
		case LRO_TYPE_IPV4_TCP:
			tcp_hdr_offset -= sizeof(*le->outer.ip4);
			m->m_pkthdr.lro_etype = ETHERTYPE_IP;
			break;
		case LRO_TYPE_IPV6_TCP:
			tcp_hdr_offset -= sizeof(*le->outer.ip6);
			m->m_pkthdr.lro_etype = ETHERTYPE_IPV6;
			break;
		default:
			goto compressed;
		}
		break;
	case LRO_TYPE_IPV4_TCP:
		tcp_hdr_offset -= sizeof(*le->outer.ip4);
		m->m_pkthdr.lro_etype = ETHERTYPE_IP;
		break;
	case LRO_TYPE_IPV6_TCP:
		tcp_hdr_offset -= sizeof(*le->outer.ip6);
		m->m_pkthdr.lro_etype = ETHERTYPE_IPV6;
		break;
	default:
		goto compressed;
	}

	MPASS(tcp_hdr_offset >= 0);

	m_adj(m, tcp_hdr_offset);
	m->m_flags |= M_LRO_EHDRSTRP;
	m->m_flags &= ~M_ACKCMP;
	m->m_pkthdr.lro_tcp_h_off -= tcp_hdr_offset;

	th = tcp_lro_get_th(m);

	th->th_sum = 0;		/* TCP checksum is valid. */

	/* Check if ACK can be compressed */
	can_compress = tcp_lro_ack_valid(m, th, &ts_ptr, &other_opts);

	/* Now lets look at the should wake states */
	if ((other_opts == true) &&
	    ((tp->t_flags2 & TF2_DONT_SACK_QUEUE) == 0)) {
		/*
		 * If there are other options (SACK?) and the
		 * tcp endpoint has not expressly told us it does
		 * not care about SACKS, then we should wake up.
		 */
		*should_wake = true;
	} else if (*should_wake == false) {
		/* Wakeup override check if we are false here  */
		*should_wake = tcp_lro_check_wake_status(tp);
	}
	/* Is the ack compressable? */
	if (can_compress == false)
		goto done;
	/* Does the TCP endpoint support ACK compression? */
	if ((tp->t_flags2 & TF2_MBUF_ACKCMP) == 0)
		goto done;

	/* Lets get the TOS/traffic class field */
	l3.ptr = mtod(m, void *);
	switch (lro_type) {
	case LRO_TYPE_IPV4_TCP:
		iptos = l3.ip4->ip_tos;
		break;
	case LRO_TYPE_IPV6_TCP:
		iptos = IPV6_TRAFFIC_CLASS(l3.ip6);
		break;
	default:
		iptos = 0;	/* Keep compiler happy. */
		break;
	}
	/* Now lets get space if we don't have some already */
	if (*cmp == NULL) {
new_one:
		nm = tcp_lro_get_last_if_ackcmp(lc, le, tp, &n_mbuf,
		    can_append_old_cmp);
		if (__predict_false(nm == NULL))
			goto done;
		*cmp = nm;
		if (n_mbuf) {
			/*
			 *  Link in the new cmp ack to our in-order place,
			 * first set our cmp ack's next to where we are.
			 */
			nm->m_nextpkt = m;
			(*pp) = nm;
			/*
			 * Set it up so mv_to is advanced to our
			 * compressed ack. This way the caller can
			 * advance pp to the right place.
			 */
			*mv_to = nm;
			/*
			 * Advance it here locally as well.
			 */
			pp = &nm->m_nextpkt;
		}
	} else {
		/* We have one already we are working on */
		nm = *cmp;
		if (M_TRAILINGSPACE(nm) < sizeof(struct tcp_ackent)) {
			/* We ran out of space */
			tp->t_flags2 |= TF2_MBUF_L_ACKS;
			goto new_one;
		}
	}
	MPASS(M_TRAILINGSPACE(nm) >= sizeof(struct tcp_ackent));
	counter_u64_add(tcp_inp_lro_compressed, 1);
	le->compressed++;
	/* We can add in to the one on the tail */
	ack_ent = mtod(nm, struct tcp_ackent *);
	idx = (nm->m_len / sizeof(struct tcp_ackent));
	build_ack_entry(&ack_ent[idx], th, m, ts_ptr, iptos);

	/* Bump the size of both pkt-hdr and len */
	nm->m_len += sizeof(struct tcp_ackent);
	nm->m_pkthdr.len += sizeof(struct tcp_ackent);
compressed:
	/* Advance to next mbuf before freeing. */
	*pp = m->m_nextpkt;
	m->m_nextpkt = NULL;
	m_freem(m);
	return (true);
done:
	counter_u64_add(tcp_uncomp_total, 1);
	le->uncompressed++;
	return (false);
}

static void
tcp_queue_pkts(struct tcpcb *tp, struct lro_entry *le)
{

	INP_WLOCK_ASSERT(tptoinpcb(tp));

	STAILQ_HEAD(, mbuf) q = { le->m_head,
	    &STAILQ_NEXT(le->m_last_mbuf, m_stailqpkt) };
	STAILQ_CONCAT(&tp->t_inqueue, &q);
	le->m_head = NULL;
	le->m_last_mbuf = NULL;
}

static struct tcpcb *
tcp_lro_lookup(struct ifnet *ifp, struct lro_parser *pa)
{
	struct inpcb *inp;

	CURVNET_SET(ifp->if_vnet);
	switch (pa->data.lro_type) {
#ifdef INET6
	case LRO_TYPE_IPV6_TCP:
		inp = in6_pcblookup(&V_tcbinfo,
		    &pa->data.s_addr.v6,
		    pa->data.s_port,
		    &pa->data.d_addr.v6,
		    pa->data.d_port,
		    INPLOOKUP_WLOCKPCB,
		    ifp);
		break;
#endif
#ifdef INET
	case LRO_TYPE_IPV4_TCP:
		inp = in_pcblookup(&V_tcbinfo,
		    pa->data.s_addr.v4,
		    pa->data.s_port,
		    pa->data.d_addr.v4,
		    pa->data.d_port,
		    INPLOOKUP_WLOCKPCB,
		    ifp);
		break;
#endif
	default:
		CURVNET_RESTORE();
		return (NULL);
	}
	CURVNET_RESTORE();

	return (intotcpcb(inp));
}

static int
_tcp_lro_flush_tcphpts(struct lro_ctrl *lc, struct lro_entry *le)
{
	struct tcpcb *tp;
	struct mbuf **pp, *cmp, *mv_to;
	struct ifnet *lagg_ifp;
	bool bpf_req, lagg_bpf_req, should_wake, can_append_old_cmp;

	/* Check if packet doesn't belongs to our network interface. */
	if ((tcplro_stacks_wanting_mbufq == 0) ||
	    (le->outer.data.vlan_id != 0) ||
	    (le->inner.data.lro_type != LRO_TYPE_NONE))
		return (TCP_LRO_CANNOT);

#ifdef INET6
	/*
	 * Be proactive about unspecified IPv6 address in source. As
	 * we use all-zero to indicate unbounded/unconnected pcb,
	 * unspecified IPv6 address can be used to confuse us.
	 *
	 * Note that packets with unspecified IPv6 destination is
	 * already dropped in ip6_input.
	 */
	if (__predict_false(le->outer.data.lro_type == LRO_TYPE_IPV6_TCP &&
	    IN6_IS_ADDR_UNSPECIFIED(&le->outer.data.s_addr.v6)))
		return (TCP_LRO_CANNOT);

	if (__predict_false(le->inner.data.lro_type == LRO_TYPE_IPV6_TCP &&
	    IN6_IS_ADDR_UNSPECIFIED(&le->inner.data.s_addr.v6)))
		return (TCP_LRO_CANNOT);
#endif
	/* Lookup inp, if any.  Returns locked TCP inpcb. */
	tp = tcp_lro_lookup(lc->ifp,
	    (le->inner.data.lro_type == LRO_TYPE_NONE) ? &le->outer : &le->inner);
	if (tp == NULL)
		return (TCP_LRO_CANNOT);

	counter_u64_add(tcp_inp_lro_locks_taken, 1);

	/* Check if the inp is dead, Jim. */
	if (tp->t_state == TCPS_TIME_WAIT) {
		INP_WUNLOCK(tptoinpcb(tp));
		return (TCP_LRO_CANNOT);
	}
	if (tp->t_lro_cpu == HPTS_CPU_NONE && lc->lro_cpu_is_set == 1)
		tp->t_lro_cpu = lc->lro_last_cpu;
	/* Check if the transport doesn't support the needed optimizations. */
	if ((tp->t_flags2 & (TF2_SUPPORTS_MBUFQ | TF2_MBUF_ACKCMP)) == 0) {
		INP_WUNLOCK(tptoinpcb(tp));
		return (TCP_LRO_CANNOT);
	}

	if (tp->t_flags2 & TF2_MBUF_QUEUE_READY)
		should_wake = false;
	else
		should_wake = true;
	/* Check if packets should be tapped to BPF. */
	bpf_req = bpf_peers_present(lc->ifp->if_bpf);
	lagg_bpf_req = false;
	lagg_ifp = NULL;
	if (lc->ifp->if_type == IFT_IEEE8023ADLAG ||
	    lc->ifp->if_type == IFT_INFINIBANDLAG) {
		struct lagg_port *lp = lc->ifp->if_lagg;
		struct lagg_softc *sc = lp->lp_softc;

		lagg_ifp = sc->sc_ifp;
		if (lagg_ifp != NULL)
			lagg_bpf_req = bpf_peers_present(lagg_ifp->if_bpf);
	}

	/* Strip and compress all the incoming packets. */
	can_append_old_cmp = true;
	cmp = NULL;
	for (pp = &le->m_head; *pp != NULL; ) {
		mv_to = NULL;
		if (do_bpf_strip_and_compress(tp, lc, le, pp, &cmp, &mv_to,
		    &should_wake, bpf_req, lagg_bpf_req, lagg_ifp,
		    can_append_old_cmp) == false) {
			/* Advance to next mbuf. */
			pp = &(*pp)->m_nextpkt;
			/*
			 * Once we have appended we can't look in the pending
			 * inbound packets for a compressed ack to append to.
			 */
			can_append_old_cmp = false;
			/*
			 * Once we append we also need to stop adding to any
			 * compressed ack we were remembering. A new cmp
			 * ack will be required.
			 */
			cmp = NULL;
			tcp_lro_log(tp, lc, le, NULL, 25, 0, 0, 0, 0);
		} else if (mv_to != NULL) {
			/* We are asked to move pp up */
			pp = &mv_to->m_nextpkt;
			tcp_lro_log(tp, lc, le, NULL, 24, 0, 0, 0, 0);
		} else
			tcp_lro_log(tp, lc, le, NULL, 26, 0, 0, 0, 0);
	}
	/* Update "m_last_mbuf", if any. */
	if (pp == &le->m_head)
		le->m_last_mbuf = *pp;
	else
		le->m_last_mbuf = __containerof(pp, struct mbuf, m_nextpkt);

	/* Check if any data mbufs left. */
	if (le->m_head != NULL) {
		counter_u64_add(tcp_inp_lro_direct_queue, 1);
		tcp_lro_log(tp, lc, le, NULL, 22, 1, tp->t_flags2, 0, 1);
		tcp_queue_pkts(tp, le);
	}
	if (should_wake) {
		/* Wakeup */
		counter_u64_add(tcp_inp_lro_wokeup_queue, 1);
		if ((*tp->t_fb->tfb_do_queued_segments)(tp, 0))
			/* TCP cb gone and unlocked. */
			return (0);
	}
	INP_WUNLOCK(tptoinpcb(tp));

	return (0);	/* Success. */
}

void
tcp_lro_hpts_init(void)
{
	tcp_lro_flush_tcphpts = _tcp_lro_flush_tcphpts;
}
