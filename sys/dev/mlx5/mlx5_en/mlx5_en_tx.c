/*-
 * Copyright (c) 2015-2019 Mellanox Technologies. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include "opt_kern_tls.h"

#include "en.h"
#include <machine/atomic.h>

static inline bool
mlx5e_do_send_cqe_inline(struct mlx5e_sq *sq)
{
	sq->cev_counter++;
	/* interleave the CQEs */
	if (sq->cev_counter >= sq->cev_factor) {
		sq->cev_counter = 0;
		return (true);
	}
	return (false);
}

bool
mlx5e_do_send_cqe(struct mlx5e_sq *sq)
{

	return (mlx5e_do_send_cqe_inline(sq));
}

void
mlx5e_send_nop(struct mlx5e_sq *sq, u32 ds_cnt)
{
	u16 pi = sq->pc & sq->wq.sz_m1;
	struct mlx5e_tx_wqe *wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);

	memset(&wqe->ctrl, 0, sizeof(wqe->ctrl));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) | MLX5_OPCODE_NOP);
	wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);
	if (mlx5e_do_send_cqe_inline(sq))
		wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;
	else
		wqe->ctrl.fm_ce_se = 0;

	/* Copy data for doorbell */
	memcpy(sq->doorbell.d32, &wqe->ctrl, sizeof(sq->doorbell.d32));

	sq->mbuf[pi].mbuf = NULL;
	sq->mbuf[pi].num_bytes = 0;
	sq->mbuf[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	sq->pc += sq->mbuf[pi].num_wqebbs;
}

#if (__FreeBSD_version >= 1100000)
static uint32_t mlx5e_hash_value;

static void
mlx5e_hash_init(void *arg)
{
	mlx5e_hash_value = m_ether_tcpip_hash_init();
}

/* Make kernel call mlx5e_hash_init after the random stack finished initializing */
SYSINIT(mlx5e_hash_init, SI_SUB_RANDOM, SI_ORDER_ANY, &mlx5e_hash_init, NULL);
#endif

static struct mlx5e_sq *
mlx5e_select_queue_by_send_tag(struct ifnet *ifp, struct mbuf *mb)
{
	struct m_snd_tag *mb_tag;
	struct mlx5e_sq *sq;

	mb_tag = mb->m_pkthdr.snd_tag;

#ifdef KERN_TLS
top:
#endif
	/* get pointer to sendqueue */
	switch (mb_tag->type) {
#ifdef RATELIMIT
	case IF_SND_TAG_TYPE_RATE_LIMIT:
		sq = container_of(mb_tag,
		    struct mlx5e_rl_channel, tag)->sq;
		break;
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS_RATE_LIMIT:
		mb_tag = container_of(mb_tag, struct mlx5e_tls_tag, tag)->rl_tag;
		goto top;
#endif
#endif
	case IF_SND_TAG_TYPE_UNLIMITED:
		sq = &container_of(mb_tag,
		    struct mlx5e_channel, tag)->sq[0];
		KASSERT((mb_tag->refcount > 0),
		    ("mlx5e_select_queue: Channel refs are zero for unlimited tag"));
		break;
#ifdef KERN_TLS
	case IF_SND_TAG_TYPE_TLS:
		mb_tag = container_of(mb_tag, struct mlx5e_tls_tag, tag)->rl_tag;
		goto top;
#endif
	default:
		sq = NULL;
		break;
	}

	/* check if valid */
	if (sq != NULL && READ_ONCE(sq->running) != 0)
		return (sq);

	return (NULL);
}

static struct mlx5e_sq *
mlx5e_select_queue(struct ifnet *ifp, struct mbuf *mb)
{
	struct mlx5e_priv *priv = ifp->if_softc;
	struct mlx5e_sq *sq;
	u32 ch;
	u32 tc;

	/* obtain VLAN information if present */
	if (mb->m_flags & M_VLANTAG) {
		tc = (mb->m_pkthdr.ether_vtag >> 13);
		if (tc >= priv->num_tc)
			tc = priv->default_vlan_prio;
	} else {
		tc = priv->default_vlan_prio;
	}

	ch = priv->params.num_channels;

	/* check if flowid is set */
	if (M_HASHTYPE_GET(mb) != M_HASHTYPE_NONE) {
#ifdef RSS
		u32 temp;

		if (rss_hash2bucket(mb->m_pkthdr.flowid,
		    M_HASHTYPE_GET(mb), &temp) == 0)
			ch = temp % ch;
		else
#endif
			ch = (mb->m_pkthdr.flowid % 128) % ch;
	} else {
#if (__FreeBSD_version >= 1100000)
		ch = m_ether_tcpip_hash(MBUF_HASHFLAG_L3 |
		    MBUF_HASHFLAG_L4, mb, mlx5e_hash_value) % ch;
#else
		/*
		 * m_ether_tcpip_hash not present in stable, so just
		 * throw unhashed mbufs on queue 0
		 */
		ch = 0;
#endif
	}

	/* check if send queue is running */
	sq = &priv->channel[ch].sq[tc];
	if (likely(READ_ONCE(sq->running) != 0))
		return (sq);
	return (NULL);
}

static inline u16
mlx5e_get_l2_header_size(struct mlx5e_sq *sq, struct mbuf *mb)
{
	struct ether_vlan_header *eh;
	uint16_t eth_type;
	int min_inline;

	eh = mtod(mb, struct ether_vlan_header *);
	if (unlikely(mb->m_len < ETHER_HDR_LEN)) {
		goto max_inline;
	} else if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		if (unlikely(mb->m_len < (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN)))
			goto max_inline;
		eth_type = ntohs(eh->evl_proto);
		min_inline = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		eth_type = ntohs(eh->evl_encap_proto);
		min_inline = ETHER_HDR_LEN;
	}

	switch (eth_type) {
	case ETHERTYPE_IP:
	case ETHERTYPE_IPV6:
		/*
		 * Make sure the TOS(IPv4) or traffic class(IPv6)
		 * field gets inlined. Else the SQ may stall.
		 */
		min_inline += 4;
		break;
	default:
		goto max_inline;
	}

	/*
	 * m_copydata() will be used on the remaining header which
	 * does not need to reside within the first m_len bytes of
	 * data:
	 */
	if (mb->m_pkthdr.len < min_inline)
		goto max_inline;
	return (min_inline);

max_inline:
	return (MIN(mb->m_pkthdr.len, sq->max_inline));
}

/*
 * This function parse IPv4 and IPv6 packets looking for TCP and UDP
 * headers.
 *
 * Upon return the pointer at which the "ppth" argument points, is set
 * to the location of the TCP header. NULL is used if no TCP header is
 * present.
 *
 * The return value indicates the number of bytes from the beginning
 * of the packet until the first byte after the TCP or UDP header. If
 * this function returns zero, the parsing failed.
 */
int
mlx5e_get_full_header_size(const struct mbuf *mb, const struct tcphdr **ppth)
{
	const struct ether_vlan_header *eh;
	const struct tcphdr *th;
	const struct ip *ip;
	int ip_hlen, tcp_hlen;
	const struct ip6_hdr *ip6;
	uint16_t eth_type;
	int eth_hdr_len;

	eh = mtod(mb, const struct ether_vlan_header *);
	if (unlikely(mb->m_len < ETHER_HDR_LEN))
		goto failure;
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		if (unlikely(mb->m_len < (ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN)))
			goto failure;
		eth_type = ntohs(eh->evl_proto);
		eth_hdr_len = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		eth_type = ntohs(eh->evl_encap_proto);
		eth_hdr_len = ETHER_HDR_LEN;
	}

	switch (eth_type) {
	case ETHERTYPE_IP:
		ip = (const struct ip *)(mb->m_data + eth_hdr_len);
		if (unlikely(mb->m_len < eth_hdr_len + sizeof(*ip)))
			goto failure;
		switch (ip->ip_p) {
		case IPPROTO_TCP:
			ip_hlen = ip->ip_hl << 2;
			eth_hdr_len += ip_hlen;
			goto tcp_packet;
		case IPPROTO_UDP:
			ip_hlen = ip->ip_hl << 2;
			eth_hdr_len += ip_hlen + 8;
			th = NULL;
			goto udp_packet;
		default:
			goto failure;
		}
		break;
	case ETHERTYPE_IPV6:
		ip6 = (const struct ip6_hdr *)(mb->m_data + eth_hdr_len);
		if (unlikely(mb->m_len < eth_hdr_len + sizeof(*ip6)))
			goto failure;
		switch (ip6->ip6_nxt) {
		case IPPROTO_TCP:
			eth_hdr_len += sizeof(*ip6);
			goto tcp_packet;
		case IPPROTO_UDP:
			eth_hdr_len += sizeof(*ip6) + 8;
			th = NULL;
			goto udp_packet;
		default:
			goto failure;
		}
		break;
	default:
		goto failure;
	}
tcp_packet:
	if (unlikely(mb->m_len < eth_hdr_len + sizeof(*th))) {
		const struct mbuf *m_th = mb->m_next;
		if (unlikely(mb->m_len != eth_hdr_len ||
		    m_th == NULL || m_th->m_len < sizeof(*th)))
			goto failure;
		th = (const struct tcphdr *)(m_th->m_data);
	} else {
		th = (const struct tcphdr *)(mb->m_data + eth_hdr_len);
	}
	tcp_hlen = th->th_off << 2;
	eth_hdr_len += tcp_hlen;
udp_packet:
	/*
	 * m_copydata() will be used on the remaining header which
	 * does not need to reside within the first m_len bytes of
	 * data:
	 */
	if (unlikely(mb->m_pkthdr.len < eth_hdr_len))
		goto failure;
	if (ppth != NULL)
		*ppth = th;
	return (eth_hdr_len);
failure:
	if (ppth != NULL)
		*ppth = NULL;
	return (0);
}

struct mlx5_wqe_dump_seg {
	struct mlx5_wqe_ctrl_seg ctrl;
	struct mlx5_wqe_data_seg data;
} __aligned(MLX5_SEND_WQE_BB);

CTASSERT(DIV_ROUND_UP(2, MLX5_SEND_WQEBB_NUM_DS) == 1);

int
mlx5e_sq_dump_xmit(struct mlx5e_sq *sq, struct mlx5e_xmit_args *parg, struct mbuf **mbp)
{
	bus_dma_segment_t segs[MLX5E_MAX_TX_MBUF_FRAGS];
	struct mlx5_wqe_dump_seg *wqe;
	struct mlx5_wqe_dump_seg *wqe_last;
	int nsegs;
	int xsegs;
	u32 off;
	u32 msb;
	int err;
	int x;
	struct mbuf *mb;
	const u32 ds_cnt = 2;
	u16 pi;
	const u8 opcode = MLX5_OPCODE_DUMP;

	/* get pointer to mbuf */
	mb = *mbp;

	/* get producer index */
	pi = sq->pc & sq->wq.sz_m1;

	sq->mbuf[pi].num_bytes = mb->m_pkthdr.len;
	sq->mbuf[pi].num_wqebbs = 0;

	/* check number of segments in mbuf */
	err = bus_dmamap_load_mbuf_sg(sq->dma_tag, sq->mbuf[pi].dma_map,
	    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	if (err == EFBIG) {
		/* update statistics */
		sq->stats.defragged++;
		/* too many mbuf fragments */
		mb = m_defrag(*mbp, M_NOWAIT);
		if (mb == NULL) {
			mb = *mbp;
			goto tx_drop;
		}
		/* try again */
		err = bus_dmamap_load_mbuf_sg(sq->dma_tag, sq->mbuf[pi].dma_map,
		    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	}

	if (err != 0)
		goto tx_drop;

	/* make sure all mbuf data, if any, is visible to the bus */
	bus_dmamap_sync(sq->dma_tag, sq->mbuf[pi].dma_map,
	    BUS_DMASYNC_PREWRITE);

	/* compute number of real DUMP segments */
	msb = sq->priv->params_ethtool.hw_mtu_msb;
	for (x = xsegs = 0; x != nsegs; x++)
		xsegs += howmany((u32)segs[x].ds_len, msb);

	/* check if there are no segments */
	if (unlikely(xsegs == 0)) {
		bus_dmamap_unload(sq->dma_tag, sq->mbuf[pi].dma_map);
		m_freem(mb);
		*mbp = NULL;	/* safety clear */
		return (0);
	}

	/* return ENOBUFS if the queue is full */
	if (unlikely(!mlx5e_sq_has_room_for(sq, xsegs))) {
		sq->stats.enobuf++;
		bus_dmamap_unload(sq->dma_tag, sq->mbuf[pi].dma_map);
		m_freem(mb);
		*mbp = NULL;	/* safety clear */
		return (ENOBUFS);
	}

	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);
	wqe_last = mlx5_wq_cyc_get_wqe(&sq->wq, sq->wq.sz_m1);

	for (x = 0; x != nsegs; x++) {
		for (off = 0; off < segs[x].ds_len; off += msb) {
			u32 len = segs[x].ds_len - off;

			/* limit length */
			if (likely(len > msb))
				len = msb;

			memset(&wqe->ctrl, 0, sizeof(wqe->ctrl));

			/* fill control segment */
			wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) | opcode);
			wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);
			wqe->ctrl.imm = cpu_to_be32(parg->tisn << 8);

			/* fill data segment */
			wqe->data.addr = cpu_to_be64((uint64_t)segs[x].ds_addr + off);
			wqe->data.lkey = sq->mkey_be;
			wqe->data.byte_count = cpu_to_be32(len);

			/* advance to next building block */
			if (unlikely(wqe == wqe_last))
				wqe = mlx5_wq_cyc_get_wqe(&sq->wq, 0);
			else
				wqe++;

			sq->mbuf[pi].num_wqebbs++;
			sq->pc++;
		}
	}

	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);
	wqe_last = mlx5_wq_cyc_get_wqe(&sq->wq, (sq->pc - 1) & sq->wq.sz_m1);

	/* put in place data fence */
	wqe->ctrl.fm_ce_se |= MLX5_FENCE_MODE_INITIATOR_SMALL;

	/* check if we should generate a completion event */
	if (mlx5e_do_send_cqe_inline(sq))
		wqe_last->ctrl.fm_ce_se |= MLX5_WQE_CTRL_CQ_UPDATE;

	/* copy data for doorbell */
	memcpy(sq->doorbell.d32, wqe_last, sizeof(sq->doorbell.d32));

	/* store pointer to mbuf */
	sq->mbuf[pi].mbuf = mb;
	sq->mbuf[pi].p_refcount = parg->pref;
	atomic_add_int(parg->pref, 1);

	/* count all traffic going out */
	sq->stats.packets++;
	sq->stats.bytes += sq->mbuf[pi].num_bytes;

	*mbp = NULL;	/* safety clear */
	return (0);

tx_drop:
	sq->stats.dropped++;
	*mbp = NULL;
	m_freem(mb);
	return err;
}

int
mlx5e_sq_xmit(struct mlx5e_sq *sq, struct mbuf **mbp)
{
	bus_dma_segment_t segs[MLX5E_MAX_TX_MBUF_FRAGS];
	struct mlx5e_xmit_args args = {};
	struct mlx5_wqe_data_seg *dseg;
	struct mlx5e_tx_wqe *wqe;
	struct ifnet *ifp;
	int nsegs;
	int err;
	int x;
	struct mbuf *mb;
	u16 ds_cnt;
	u16 pi;
	u8 opcode;

#ifdef KERN_TLS
top:
#endif
	/* Return ENOBUFS if the queue is full */
	if (unlikely(!mlx5e_sq_has_room_for(sq, 2 * MLX5_SEND_WQE_MAX_WQEBBS))) {
		sq->stats.enobuf++;
		return (ENOBUFS);
	}

	/* Align SQ edge with NOPs to avoid WQE wrap around */
	pi = ((~sq->pc) & sq->wq.sz_m1);
	if (pi < (MLX5_SEND_WQE_MAX_WQEBBS - 1)) {
		/* Send one multi NOP message instead of many */
		mlx5e_send_nop(sq, (pi + 1) * MLX5_SEND_WQEBB_NUM_DS);
		pi = ((~sq->pc) & sq->wq.sz_m1);
		if (pi < (MLX5_SEND_WQE_MAX_WQEBBS - 1)) {
			sq->stats.enobuf++;
			return (ENOMEM);
		}
	}

#ifdef KERN_TLS
	/* Special handling for TLS packets, if any */
	switch (mlx5e_sq_tls_xmit(sq, &args, mbp)) {
	case MLX5E_TLS_LOOP:
		goto top;
	case MLX5E_TLS_FAILURE:
		mb = *mbp;
		err = ENOMEM;
		goto tx_drop;
	case MLX5E_TLS_DEFERRED:
		return (0);
	case MLX5E_TLS_CONTINUE:
	default:
		break;
	}
#endif

	/* Setup local variables */
	pi = sq->pc & sq->wq.sz_m1;
	wqe = mlx5_wq_cyc_get_wqe(&sq->wq, pi);
	ifp = sq->ifp;

	memset(wqe, 0, sizeof(*wqe));

	/* get pointer to mbuf */
	mb = *mbp;

	/* Send a copy of the frame to the BPF listener, if any */
	if (ifp != NULL && ifp->if_bpf != NULL)
		ETHER_BPF_MTAP(ifp, mb);

	if (mb->m_pkthdr.csum_flags & (CSUM_IP | CSUM_TSO)) {
		wqe->eth.cs_flags |= MLX5_ETH_WQE_L3_CSUM;
	}
	if (mb->m_pkthdr.csum_flags & (CSUM_TCP | CSUM_UDP | CSUM_UDP_IPV6 | CSUM_TCP_IPV6 | CSUM_TSO)) {
		wqe->eth.cs_flags |= MLX5_ETH_WQE_L4_CSUM;
	}
	if (wqe->eth.cs_flags == 0) {
		sq->stats.csum_offload_none++;
	}
	if (mb->m_pkthdr.csum_flags & CSUM_TSO) {
		u32 payload_len;
		u32 mss = mb->m_pkthdr.tso_segsz;
		u32 num_pkts;

		wqe->eth.mss = cpu_to_be16(mss);
		opcode = MLX5_OPCODE_LSO;
		if (args.ihs == 0)
			args.ihs = mlx5e_get_full_header_size(mb, NULL);
		if (unlikely(args.ihs == 0)) {
			err = EINVAL;
			goto tx_drop;
		}
		payload_len = mb->m_pkthdr.len - args.ihs;
		if (payload_len == 0)
			num_pkts = 1;
		else
			num_pkts = DIV_ROUND_UP(payload_len, mss);
		sq->mbuf[pi].num_bytes = payload_len + (num_pkts * args.ihs);

		sq->stats.tso_packets++;
		sq->stats.tso_bytes += payload_len;
	} else {
		opcode = MLX5_OPCODE_SEND;

		if (args.ihs == 0) {
			switch (sq->min_inline_mode) {
			case MLX5_INLINE_MODE_IP:
			case MLX5_INLINE_MODE_TCP_UDP:
				args.ihs = mlx5e_get_full_header_size(mb, NULL);
				if (unlikely(args.ihs == 0))
					args.ihs = mlx5e_get_l2_header_size(sq, mb);
				break;
			case MLX5_INLINE_MODE_L2:
				args.ihs = mlx5e_get_l2_header_size(sq, mb);
				break;
			case MLX5_INLINE_MODE_NONE:
				/* FALLTHROUGH */
			default:
				if ((mb->m_flags & M_VLANTAG) != 0 &&
				    (sq->min_insert_caps & MLX5E_INSERT_VLAN) != 0) {
					/* inlining VLAN data is not required */
					wqe->eth.vlan_cmd = htons(0x8000); /* bit 0 CVLAN */
					wqe->eth.vlan_hdr = htons(mb->m_pkthdr.ether_vtag);
					args.ihs = 0;
				} else if ((mb->m_flags & M_VLANTAG) == 0 &&
				    (sq->min_insert_caps & MLX5E_INSERT_NON_VLAN) != 0) {
					/* inlining non-VLAN data is not required */
					args.ihs = 0;
				} else {
					/* we are forced to inlining L2 header, if any */
					args.ihs = mlx5e_get_l2_header_size(sq, mb);
				}
				break;
			}
		}
		sq->mbuf[pi].num_bytes = max_t (unsigned int,
		    mb->m_pkthdr.len, ETHER_MIN_LEN - ETHER_CRC_LEN);
	}

	if (likely(args.ihs == 0)) {
		/* nothing to inline */
	} else if ((mb->m_flags & M_VLANTAG) != 0) {
		struct ether_vlan_header *eh = (struct ether_vlan_header *)
		    wqe->eth.inline_hdr_start;

		/* Range checks */
		if (unlikely(args.ihs > (sq->max_inline - ETHER_VLAN_ENCAP_LEN))) {
			if (mb->m_pkthdr.csum_flags & CSUM_TSO) {
				err = EINVAL;
				goto tx_drop;
			}
			args.ihs = (sq->max_inline - ETHER_VLAN_ENCAP_LEN);
		} else if (unlikely(args.ihs < ETHER_HDR_LEN)) {
			err = EINVAL;
			goto tx_drop;
		}
		m_copydata(mb, 0, ETHER_HDR_LEN, (caddr_t)eh);
		m_adj(mb, ETHER_HDR_LEN);
		/* Insert 4 bytes VLAN tag into data stream */
		eh->evl_proto = eh->evl_encap_proto;
		eh->evl_encap_proto = htons(ETHERTYPE_VLAN);
		eh->evl_tag = htons(mb->m_pkthdr.ether_vtag);
		/* Copy rest of header data, if any */
		m_copydata(mb, 0, args.ihs - ETHER_HDR_LEN, (caddr_t)(eh + 1));
		m_adj(mb, args.ihs - ETHER_HDR_LEN);
		/* Extend header by 4 bytes */
		args.ihs += ETHER_VLAN_ENCAP_LEN;
		wqe->eth.inline_hdr_sz = cpu_to_be16(args.ihs);
	} else {
		/* check if inline header size is too big */
		if (unlikely(args.ihs > sq->max_inline)) {
			if (unlikely(mb->m_pkthdr.csum_flags & CSUM_TSO)) {
				err = EINVAL;
				goto tx_drop;
			}
			args.ihs = sq->max_inline;
		}
		m_copydata(mb, 0, args.ihs, wqe->eth.inline_hdr_start);
		m_adj(mb, args.ihs);
		wqe->eth.inline_hdr_sz = cpu_to_be16(args.ihs);
	}

	ds_cnt = sizeof(*wqe) / MLX5_SEND_WQE_DS;
	if (args.ihs > sizeof(wqe->eth.inline_hdr_start)) {
		ds_cnt += DIV_ROUND_UP(args.ihs - sizeof(wqe->eth.inline_hdr_start),
		    MLX5_SEND_WQE_DS);
	}
	dseg = ((struct mlx5_wqe_data_seg *)&wqe->ctrl) + ds_cnt;

	err = bus_dmamap_load_mbuf_sg(sq->dma_tag, sq->mbuf[pi].dma_map,
	    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	if (err == EFBIG) {
		/* Update statistics */
		sq->stats.defragged++;
		/* Too many mbuf fragments */
		mb = m_defrag(*mbp, M_NOWAIT);
		if (mb == NULL) {
			mb = *mbp;
			goto tx_drop;
		}
		/* Try again */
		err = bus_dmamap_load_mbuf_sg(sq->dma_tag, sq->mbuf[pi].dma_map,
		    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	}
	/* Catch errors */
	if (err != 0)
		goto tx_drop;

	/* Make sure all mbuf data, if any, is visible to the bus */
	if (nsegs != 0) {
		bus_dmamap_sync(sq->dma_tag, sq->mbuf[pi].dma_map,
		    BUS_DMASYNC_PREWRITE);
	} else {
		/* All data was inlined, free the mbuf. */
		bus_dmamap_unload(sq->dma_tag, sq->mbuf[pi].dma_map);
		m_freem(mb);
		mb = NULL;
	}

	for (x = 0; x != nsegs; x++) {
		if (segs[x].ds_len == 0)
			continue;
		dseg->addr = cpu_to_be64((uint64_t)segs[x].ds_addr);
		dseg->lkey = sq->mkey_be;
		dseg->byte_count = cpu_to_be32((uint32_t)segs[x].ds_len);
		dseg++;
	}

	ds_cnt = (dseg - ((struct mlx5_wqe_data_seg *)&wqe->ctrl));

	wqe->ctrl.opmod_idx_opcode = cpu_to_be32((sq->pc << 8) | opcode);
	wqe->ctrl.qpn_ds = cpu_to_be32((sq->sqn << 8) | ds_cnt);
	wqe->ctrl.imm = cpu_to_be32(args.tisn << 8);

	if (mlx5e_do_send_cqe_inline(sq))
		wqe->ctrl.fm_ce_se = MLX5_WQE_CTRL_CQ_UPDATE;
	else
		wqe->ctrl.fm_ce_se = 0;

	/* Copy data for doorbell */
	memcpy(sq->doorbell.d32, &wqe->ctrl, sizeof(sq->doorbell.d32));

	/* Store pointer to mbuf */
	sq->mbuf[pi].mbuf = mb;
	sq->mbuf[pi].num_wqebbs = DIV_ROUND_UP(ds_cnt, MLX5_SEND_WQEBB_NUM_DS);
	sq->mbuf[pi].p_refcount = args.pref;
	if (unlikely(args.pref != NULL))
		atomic_add_int(args.pref, 1);
	sq->pc += sq->mbuf[pi].num_wqebbs;

	/* Count all traffic going out */
	sq->stats.packets++;
	sq->stats.bytes += sq->mbuf[pi].num_bytes;

	*mbp = NULL;	/* safety clear */
	return (0);

tx_drop:
	sq->stats.dropped++;
	*mbp = NULL;
	m_freem(mb);
	return err;
}

static void
mlx5e_poll_tx_cq(struct mlx5e_sq *sq, int budget)
{
	u16 sqcc;

	/*
	 * sq->cc must be updated only after mlx5_cqwq_update_db_record(),
	 * otherwise a cq overrun may occur
	 */
	sqcc = sq->cc;

	while (budget > 0) {
		struct mlx5_cqe64 *cqe;
		struct mbuf *mb;
		u16 x;
		u16 ci;

		cqe = mlx5e_get_cqe(&sq->cq);
		if (!cqe)
			break;

		mlx5_cqwq_pop(&sq->cq.wq);

		/* update budget according to the event factor */
		budget -= sq->cev_factor;

		for (x = 0; x != sq->cev_factor; x++) {
			ci = sqcc & sq->wq.sz_m1;
			mb = sq->mbuf[ci].mbuf;
			sq->mbuf[ci].mbuf = NULL;

			if (unlikely(sq->mbuf[ci].p_refcount != NULL)) {
				atomic_add_int(sq->mbuf[ci].p_refcount, -1);
				sq->mbuf[ci].p_refcount = NULL;
			}

			if (mb == NULL) {
				if (sq->mbuf[ci].num_bytes == 0) {
					/* NOP */
					sq->stats.nop++;
				}
			} else {
				bus_dmamap_sync(sq->dma_tag, sq->mbuf[ci].dma_map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sq->dma_tag, sq->mbuf[ci].dma_map);

				/* Free transmitted mbuf */
				m_freem(mb);
			}
			sqcc += sq->mbuf[ci].num_wqebbs;
		}
	}

	mlx5_cqwq_update_db_record(&sq->cq.wq);

	/* Ensure cq space is freed before enabling more cqes */
	atomic_thread_fence_rel();

	sq->cc = sqcc;
}

static int
mlx5e_xmit_locked(struct ifnet *ifp, struct mlx5e_sq *sq, struct mbuf *mb)
{
	int err = 0;

	if (unlikely((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    READ_ONCE(sq->running) == 0)) {
		m_freem(mb);
		return (ENETDOWN);
	}

	/* Do transmit */
	if (mlx5e_sq_xmit(sq, &mb) != 0) {
		/* NOTE: m_freem() is NULL safe */
		m_freem(mb);
		err = ENOBUFS;
	}

	/* Check if we need to write the doorbell */
	if (likely(sq->doorbell.d64 != 0)) {
		mlx5e_tx_notify_hw(sq, sq->doorbell.d32, 0);
		sq->doorbell.d64 = 0;
	}

	/*
	 * Check if we need to start the event timer which flushes the
	 * transmit ring on timeout:
	 */
	if (unlikely(sq->cev_next_state == MLX5E_CEV_STATE_INITIAL &&
	    sq->cev_factor != 1)) {
		/* start the timer */
		mlx5e_sq_cev_timeout(sq);
	} else {
		/* don't send NOPs yet */
		sq->cev_next_state = MLX5E_CEV_STATE_HOLD_NOPS;
	}
	return (err);
}

int
mlx5e_xmit(struct ifnet *ifp, struct mbuf *mb)
{
	struct mlx5e_sq *sq;
	int ret;

	if (mb->m_pkthdr.csum_flags & CSUM_SND_TAG) {
		MPASS(mb->m_pkthdr.snd_tag->ifp == ifp);
		sq = mlx5e_select_queue_by_send_tag(ifp, mb);
		if (unlikely(sq == NULL)) {
			goto select_queue;
		}
	} else {
select_queue:
		sq = mlx5e_select_queue(ifp, mb);
		if (unlikely(sq == NULL)) {
			/* Free mbuf */
			m_freem(mb);

			/* Invalid send queue */
			return (ENXIO);
		}
	}

	mtx_lock(&sq->lock);
	ret = mlx5e_xmit_locked(ifp, sq, mb);
	mtx_unlock(&sq->lock);

	return (ret);
}

void
mlx5e_tx_cq_comp(struct mlx5_core_cq *mcq)
{
	struct mlx5e_sq *sq = container_of(mcq, struct mlx5e_sq, cq.mcq);

	mtx_lock(&sq->comp_lock);
	mlx5e_poll_tx_cq(sq, MLX5E_BUDGET_MAX);
	mlx5e_cq_arm(&sq->cq, MLX5_GET_DOORBELL_LOCK(&sq->priv->doorbell_lock));
	mtx_unlock(&sq->comp_lock);
}
