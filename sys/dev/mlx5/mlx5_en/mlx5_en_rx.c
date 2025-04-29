/*-
 * Copyright (c) 2015-2021 Mellanox Technologies. All rights reserved.
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
 */

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <dev/mlx5/mlx5_en/en.h>
#include <netinet/ip_var.h>
#include <machine/in_cksum.h>
#include <dev/mlx5/mlx5_accel/ipsec.h>

static inline int
mlx5e_alloc_rx_wqe(struct mlx5e_rq *rq,
    struct mlx5e_rx_wqe *wqe, u16 ix)
{
	bus_dma_segment_t segs[MLX5E_MAX_BUSDMA_RX_SEGS];
	struct mbuf *mb;
	int nsegs;
	int err;
	struct mbuf *mb_head;
	int i;

	if (rq->mbuf[ix].mbuf != NULL)
		return (0);

	mb_head = mb = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, rq->wqe_sz);
	if (unlikely(mb == NULL))
		return (-ENOMEM);

	mb->m_len = rq->wqe_sz;
	mb->m_pkthdr.len = rq->wqe_sz;

	for (i = 1; i < rq->nsegs; i++) {
		mb = mb->m_next = m_getjcl(M_NOWAIT, MT_DATA, 0, rq->wqe_sz);
		if (unlikely(mb == NULL)) {
			m_freem(mb_head);
			return (-ENOMEM);
		}
		mb->m_len = rq->wqe_sz;
		mb_head->m_pkthdr.len += rq->wqe_sz;
	}
	/* rewind to first mbuf in chain */
	mb = mb_head;

	/* get IP header aligned */
	m_adj(mb, MLX5E_NET_IP_ALIGN);

	err = mlx5_accel_ipsec_rx_tag_add(rq->ifp, &rq->mbuf[ix]);
	if (err)
		goto err_free_mbuf;
	err = -bus_dmamap_load_mbuf_sg(rq->dma_tag, rq->mbuf[ix].dma_map,
	    mb, segs, &nsegs, BUS_DMA_NOWAIT);
	if (err != 0)
		goto err_free_mbuf;
	if (unlikely(nsegs == 0)) {
		bus_dmamap_unload(rq->dma_tag, rq->mbuf[ix].dma_map);
		err = -ENOMEM;
		goto err_free_mbuf;
	}
	wqe->data[0].addr = cpu_to_be64(segs[0].ds_addr);
	wqe->data[0].byte_count = cpu_to_be32(segs[0].ds_len |
	    MLX5_HW_START_PADDING);
	for (i = 1; i != nsegs; i++) {
		wqe->data[i].addr = cpu_to_be64(segs[i].ds_addr);
		wqe->data[i].byte_count = cpu_to_be32(segs[i].ds_len);
	}
	for (; i < rq->nsegs; i++) {
		wqe->data[i].addr = 0;
		wqe->data[i].byte_count = 0;
	}

	rq->mbuf[ix].mbuf = mb;
	rq->mbuf[ix].data = mb->m_data;

	bus_dmamap_sync(rq->dma_tag, rq->mbuf[ix].dma_map,
	    BUS_DMASYNC_PREREAD);
	return (0);

err_free_mbuf:
	m_freem(mb);
	return (err);
}

static void
mlx5e_post_rx_wqes(struct mlx5e_rq *rq)
{
	if (unlikely(rq->enabled == 0))
		return;

	while (!mlx5_wq_ll_is_full(&rq->wq)) {
		struct mlx5e_rx_wqe *wqe = mlx5_wq_ll_get_wqe(&rq->wq, rq->wq.head);

		if (unlikely(mlx5e_alloc_rx_wqe(rq, wqe, rq->wq.head))) {
			callout_reset_curcpu(&rq->watchdog, 1, (void *)&mlx5e_post_rx_wqes, rq);
			break;
		}
		mlx5_wq_ll_push(&rq->wq, be16_to_cpu(wqe->next.next_wqe_index));
	}

	/* ensure wqes are visible to device before updating doorbell record */
	atomic_thread_fence_rel();

	mlx5_wq_ll_update_db_record(&rq->wq);
}

static uint32_t
csum_reduce(uint32_t val)
{
	while (val > 0xffff)
		val = (val >> 16) + (val & 0xffff);
	return (val);
}

static u_short
csum_buf(uint32_t val, void *buf, int len)
{
	u_short x;

	MPASS(len % 2 == 0);
	for (int i = 0; i < len; i += 2) {
		bcopy((char *)buf + i, &x, 2);
		val = csum_reduce(val + x);
	}
	return (val);
}

static void
mlx5e_lro_update_hdr(struct mbuf *mb, struct mlx5_cqe64 *cqe)
{
	/* TODO: consider vlans, ip options, ... */
	struct ether_header *eh;
	uint16_t eh_type;
	uint16_t tot_len;
	struct ip6_hdr *ip6 = NULL;
	struct ip *ip4 = NULL;
	struct tcphdr *th;
	uint32_t *ts_ptr;
	uint32_t tcp_csum;
	uint8_t l4_hdr_type;
	int tcp_ack;

	eh = mtod(mb, struct ether_header *);
	eh_type = ntohs(eh->ether_type);

	l4_hdr_type = get_cqe_l4_hdr_type(cqe);
	tcp_ack = ((CQE_L4_HDR_TYPE_TCP_ACK_NO_DATA == l4_hdr_type) ||
	    (CQE_L4_HDR_TYPE_TCP_ACK_AND_DATA == l4_hdr_type));

	/* TODO: consider vlan */
	tot_len = be32_to_cpu(cqe->byte_cnt) - ETHER_HDR_LEN;

	switch (eh_type) {
	case ETHERTYPE_IP:
		ip4 = (struct ip *)(eh + 1);
		th = (struct tcphdr *)(ip4 + 1);
		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(eh + 1);
		th = (struct tcphdr *)(ip6 + 1);
		break;
	default:
		return;
	}

	ts_ptr = (uint32_t *)(th + 1);

	if (get_cqe_lro_tcppsh(cqe))
		tcp_set_flags(th, tcp_get_flags(th) | TH_PUSH);

	if (tcp_ack) {
		tcp_set_flags(th, tcp_get_flags(th) | TH_ACK);
		th->th_ack = cqe->lro_ack_seq_num;
		th->th_win = cqe->lro_tcp_win;

		/*
		 * FreeBSD handles only 32bit aligned timestamp right after
		 * the TCP hdr
		 * +--------+--------+--------+--------+
		 * |   NOP  |  NOP   |  TSopt |   10   |
		 * +--------+--------+--------+--------+
		 * |          TSval   timestamp        |
		 * +--------+--------+--------+--------+
		 * |          TSecr   timestamp        |
		 * +--------+--------+--------+--------+
		 */
		if (get_cqe_lro_timestamp_valid(cqe) &&
		    (__predict_true(*ts_ptr == ntohl(TCPOPT_NOP << 24 |
		    TCPOPT_NOP << 16 | TCPOPT_TIMESTAMP << 8 |
		    TCPOLEN_TIMESTAMP)))) {
			/*
			 * cqe->timestamp is 64bit long.
			 * [0-31] - timestamp.
			 * [32-64] - timestamp echo replay.
			 */
			ts_ptr[2] = *((uint32_t *)&cqe->timestamp + 1);
		}
	}
	if (ip4) {
		struct ipovly io;

		ip4->ip_ttl = cqe->lro_min_ttl;
		ip4->ip_len = cpu_to_be16(tot_len);
		ip4->ip_sum = 0;
		ip4->ip_sum = in_cksum_skip(mb, (ip4->ip_hl << 2) +
		    ETHER_HDR_LEN, ETHER_HDR_LEN);

		/* TCP checksum: data */
		tcp_csum = cqe->check_sum;

		/* TCP checksum: IP pseudoheader */
		bzero(io.ih_x1, sizeof(io.ih_x1));
		io.ih_pr = IPPROTO_TCP;
		io.ih_len = htons(ntohs(ip4->ip_len) - sizeof(*ip4));
		io.ih_src = ip4->ip_src;
		io.ih_dst = ip4->ip_dst;
		tcp_csum = csum_buf(tcp_csum, &io, sizeof(io));

		/* TCP checksum: TCP header */
		th->th_sum = 0;
		tcp_csum = csum_buf(tcp_csum, th, th->th_off * 4);
		th->th_sum = ~tcp_csum & 0xffff;
	} else {
		ip6->ip6_hlim = cqe->lro_min_ttl;
		ip6->ip6_plen = cpu_to_be16(tot_len -
		    sizeof(struct ip6_hdr));

		/* TCP checksum */
		th->th_sum = 0;
		tcp_csum = ~in6_cksum_partial_l2(mb, IPPROTO_TCP,
		    sizeof(struct ether_header),
		    sizeof(struct ether_header) + sizeof(struct ip6_hdr),
		    tot_len - sizeof(struct ip6_hdr), th->th_off * 4) & 0xffff;
		tcp_csum = csum_reduce(tcp_csum + cqe->check_sum);
		th->th_sum = ~tcp_csum & 0xffff;
	}
}

static uint64_t
mlx5e_mbuf_tstmp(struct mlx5e_priv *priv, uint64_t hw_tstmp)
{
	struct mlx5e_clbr_point *cp, dcp;
	uint64_t tstmp_sec, tstmp_nsec;
	uint64_t hw_clocks;
	uint64_t rt_cur_to_prev, res_s, res_n, res_s_modulo, res;
	uint64_t hw_clk_div;
	u_int gen;

	do {
		cp = &priv->clbr_points[priv->clbr_curr];
		gen = atomic_load_acq_int(&cp->clbr_gen);
		if (gen == 0)
			return (0);
		dcp = *cp;
		atomic_thread_fence_acq();
	} while (gen != dcp.clbr_gen);
	/*
	 * Our goal here is to have a result that is:
	 *
	 * (                             (cur_time - prev_time)   )
	 * ((hw_tstmp - hw_prev) *  ----------------------------- ) + prev_time
	 * (                             (hw_cur - hw_prev)       )
	 *
	 * With the constraints that we cannot use float and we
	 * don't want to overflow the uint64_t numbers we are using.
	 *
	 * The plan is to take the clocking value of the hw timestamps
	 * and split them into seconds and nanosecond equivalent portions.
	 * Then we operate on the two portions seperately making sure to
	 * bring back the carry over from the seconds when we divide.
	 *
	 * First up lets get the two divided into separate entities
	 * i.e. the seconds. We use the clock frequency for this.
	 * Note that priv->cclk was setup with the clock frequency
	 * in hz so we are all set to go.
	 */
	hw_clocks = hw_tstmp - dcp.clbr_hw_prev;
	tstmp_sec = hw_clocks / priv->cclk;
	tstmp_nsec = hw_clocks % priv->cclk;
	/* Now work with them separately */
	rt_cur_to_prev = (dcp.base_curr - dcp.base_prev);
	res_s = tstmp_sec * rt_cur_to_prev;
	res_n = tstmp_nsec * rt_cur_to_prev;
	/* Now lets get our divider */
	hw_clk_div = dcp.clbr_hw_curr - dcp.clbr_hw_prev;
	/* Make sure to save the remainder from the seconds divide */
	res_s_modulo = res_s % hw_clk_div;
	res_s /= hw_clk_div;
	/* scale the remainder to where it should be */
	res_s_modulo *= priv->cclk;
	/* Now add in the remainder */
	res_n += res_s_modulo;
	/* Now do the divide */
	res_n /= hw_clk_div;
	res_s *= priv->cclk;
	/* Recombine the two */
	res = res_s + res_n;
	/* And now add in the base time to get to the real timestamp */
	res += dcp.base_prev;
	return (res);
}

static inline void
mlx5e_build_rx_mbuf(struct mlx5_cqe64 *cqe, struct mlx5e_rq *rq,
    struct mbuf *mb, struct mlx5e_rq_mbuf *mr, u32 cqe_bcnt)
{
	if_t ifp = rq->ifp;
	struct mlx5e_channel *c;
	struct mbuf *mb_head;
	int lro_num_seg;	/* HW LRO session aggregated packets counter */
	uint64_t tstmp;

	lro_num_seg = be32_to_cpu(cqe->srqn) >> 24;
	if (lro_num_seg > 1) {
		mlx5e_lro_update_hdr(mb, cqe);
		rq->stats.lro_packets++;
		rq->stats.lro_bytes += cqe_bcnt;
	}

	mb->m_pkthdr.len = cqe_bcnt;
	for (mb_head = mb; mb != NULL; mb = mb->m_next) {
		if (mb->m_len > cqe_bcnt)
			mb->m_len = cqe_bcnt;
		cqe_bcnt -= mb->m_len;
		if (likely(cqe_bcnt == 0)) {
			if (likely(mb->m_next != NULL)) {
				/* trim off empty mbufs */
				m_freem(mb->m_next);
				mb->m_next = NULL;
			}
			break;
		}
	}
	/* rewind to first mbuf in chain */
	mb = mb_head;

	/* check if a Toeplitz hash was computed */
	if (cqe->rss_hash_type != 0) {
		mb->m_pkthdr.flowid = be32_to_cpu(cqe->rss_hash_result);
#ifdef RSS
		/* decode the RSS hash type */
		switch (cqe->rss_hash_type &
		    (CQE_RSS_DST_HTYPE_L4 | CQE_RSS_DST_HTYPE_IP)) {
		/* IPv4 */
		case (CQE_RSS_DST_HTYPE_TCP | CQE_RSS_DST_HTYPE_IPV4):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_TCP_IPV4);
			break;
		case (CQE_RSS_DST_HTYPE_UDP | CQE_RSS_DST_HTYPE_IPV4):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_UDP_IPV4);
			break;
		case CQE_RSS_DST_HTYPE_IPV4:
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_IPV4);
			break;
		/* IPv6 */
		case (CQE_RSS_DST_HTYPE_TCP | CQE_RSS_DST_HTYPE_IPV6):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_TCP_IPV6);
			break;
		case (CQE_RSS_DST_HTYPE_UDP | CQE_RSS_DST_HTYPE_IPV6):
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_UDP_IPV6);
			break;
		case CQE_RSS_DST_HTYPE_IPV6:
			M_HASHTYPE_SET(mb, M_HASHTYPE_RSS_IPV6);
			break;
		default:	/* Other */
			M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE_HASH);
			break;
		}
#else
		M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE_HASH);
#endif
#ifdef M_HASHTYPE_SETINNER
		if (cqe_is_tunneled(cqe))
			M_HASHTYPE_SETINNER(mb);
#endif
	} else {
		mb->m_pkthdr.flowid = rq->ix;
		M_HASHTYPE_SET(mb, M_HASHTYPE_OPAQUE);
	}
	mb->m_pkthdr.rcvif = ifp;
	mb->m_pkthdr.leaf_rcvif = ifp;

	if (cqe_is_tunneled(cqe)) {
		/*
		 * CQE can be tunneled only if TIR is configured to
		 * enable parsing of tunneled payload, so no need to
		 * check for capabilities.
		 */
		if (((cqe->hds_ip_ext & (CQE_L2_OK | CQE_L3_OK)) ==
		    (CQE_L2_OK | CQE_L3_OK))) {
			mb->m_pkthdr.csum_flags |=
			    CSUM_INNER_L3_CALC | CSUM_INNER_L3_VALID |
			    CSUM_IP_CHECKED | CSUM_IP_VALID |
			    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			mb->m_pkthdr.csum_data = htons(0xffff);

			if (likely((cqe->hds_ip_ext & CQE_L4_OK) == CQE_L4_OK)) {
				mb->m_pkthdr.csum_flags |=
				    CSUM_INNER_L4_CALC | CSUM_INNER_L4_VALID;
			}
		} else {
			rq->stats.csum_none++;
		}
	} else if (likely((if_getcapenable(ifp) & (IFCAP_RXCSUM |
	    IFCAP_RXCSUM_IPV6)) != 0) &&
	    ((cqe->hds_ip_ext & (CQE_L2_OK | CQE_L3_OK | CQE_L4_OK)) ==
	    (CQE_L2_OK | CQE_L3_OK | CQE_L4_OK))) {
		mb->m_pkthdr.csum_flags =
		    CSUM_IP_CHECKED | CSUM_IP_VALID |
		    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
		mb->m_pkthdr.csum_data = htons(0xffff);
	} else {
		rq->stats.csum_none++;
	}

	if (cqe_has_vlan(cqe)) {
		mb->m_pkthdr.ether_vtag = be16_to_cpu(cqe->vlan_info);
		mb->m_flags |= M_VLANTAG;
	}

	c = container_of(rq, struct mlx5e_channel, rq);
	if (c->priv->clbr_done >= 2) {
		tstmp = mlx5e_mbuf_tstmp(c->priv, be64_to_cpu(cqe->timestamp));
		if ((tstmp & MLX5_CQE_TSTMP_PTP) != 0) {
			/*
			 * Timestamp was taken on the packet entrance,
			 * instead of the cqe generation.
			 */
			tstmp &= ~MLX5_CQE_TSTMP_PTP;
			mb->m_flags |= M_TSTMP_HPREC;
		}
		if (tstmp != 0) {
			mb->m_pkthdr.rcv_tstmp = tstmp;
			mb->m_flags |= M_TSTMP;
		}
	}
	switch (get_cqe_tls_offload(cqe)) {
	case CQE_TLS_OFFLOAD_DECRYPTED:
		/* set proper checksum flag for decrypted packets */
		mb->m_pkthdr.csum_flags |= CSUM_TLS_DECRYPTED;
		rq->stats.decrypted_ok_packets++;
		break;
	case CQE_TLS_OFFLOAD_ERROR:
		rq->stats.decrypted_error_packets++;
		break;
	default:
		break;
	}

	mlx5e_accel_ipsec_handle_rx(mb, cqe, mr);
}

static inline void
mlx5e_read_cqe_slot(struct mlx5e_cq *cq, u32 cc, void *data)
{
	memcpy(data, mlx5_cqwq_get_wqe(&cq->wq, (cc & cq->wq.sz_m1)),
	    sizeof(struct mlx5_cqe64));
}

static inline void
mlx5e_write_cqe_slot(struct mlx5e_cq *cq, u32 cc, void *data)
{
	memcpy(mlx5_cqwq_get_wqe(&cq->wq, cc & cq->wq.sz_m1),
	    data, sizeof(struct mlx5_cqe64));
}

static inline void
mlx5e_decompress_cqe(struct mlx5e_cq *cq, struct mlx5_cqe64 *title,
    struct mlx5_mini_cqe8 *mini,
    u16 wqe_counter, int i)
{
	/*
	 * NOTE: The fields which are not set here are copied from the
	 * initial and common title. See memcpy() in
	 * mlx5e_write_cqe_slot().
	 */
	title->byte_cnt = mini->byte_cnt;
	title->wqe_counter = cpu_to_be16((wqe_counter + i) & cq->wq.sz_m1);
	title->rss_hash_result = mini->rx_hash_result;
	/*
	 * Since we use MLX5_CQE_FORMAT_HASH when creating the RX CQ,
	 * the value of the checksum should be ignored.
	 */
	title->check_sum = 0;
	title->op_own = (title->op_own & 0xf0) |
	    (((cq->wq.cc + i) >> cq->wq.log_sz) & 1);
}

#define MLX5E_MINI_ARRAY_SZ 8
/* Make sure structs are not packet differently */
CTASSERT(sizeof(struct mlx5_cqe64) ==
    sizeof(struct mlx5_mini_cqe8) * MLX5E_MINI_ARRAY_SZ);
static void
mlx5e_decompress_cqes(struct mlx5e_cq *cq)
{
	struct mlx5_mini_cqe8 mini_array[MLX5E_MINI_ARRAY_SZ];
	struct mlx5_cqe64 title;
	u32 cqe_count;
	u32 i = 0;
	u16 title_wqe_counter;

	mlx5e_read_cqe_slot(cq, cq->wq.cc, &title);
	title_wqe_counter = be16_to_cpu(title.wqe_counter);
	cqe_count = be32_to_cpu(title.byte_cnt);

	/* Make sure we won't overflow */
	KASSERT(cqe_count <= cq->wq.sz_m1,
	    ("%s: cqe_count %u > cq->wq.sz_m1 %u", __func__,
	    cqe_count, cq->wq.sz_m1));

	mlx5e_read_cqe_slot(cq, cq->wq.cc + 1, mini_array);
	while (true) {
		mlx5e_decompress_cqe(cq, &title,
		    &mini_array[i % MLX5E_MINI_ARRAY_SZ],
		    title_wqe_counter, i);
		mlx5e_write_cqe_slot(cq, cq->wq.cc + i, &title);
		i++;

		if (i == cqe_count)
			break;
		if (i % MLX5E_MINI_ARRAY_SZ == 0)
			mlx5e_read_cqe_slot(cq, cq->wq.cc + i, mini_array);
	}
}

static int
mlx5e_poll_rx_cq(struct mlx5e_rq *rq, int budget)
{
	struct pfil_head *pfil;
	int i, rv;

	CURVNET_SET_QUIET(if_getvnet(rq->ifp));
	pfil = rq->channel->priv->pfil;
	for (i = 0; i < budget; i++) {
		struct mlx5e_rx_wqe *wqe;
		struct mlx5_cqe64 *cqe;
		struct mbuf *mb;
		__be16 wqe_counter_be;
		u16 wqe_counter;
		u32 byte_cnt, seglen;

		cqe = mlx5e_get_cqe(&rq->cq);
		if (!cqe)
			break;

		if (mlx5_get_cqe_format(cqe) == MLX5_COMPRESSED)
			mlx5e_decompress_cqes(&rq->cq);

		mlx5_cqwq_pop(&rq->cq.wq);

		wqe_counter_be = cqe->wqe_counter;
		wqe_counter = be16_to_cpu(wqe_counter_be);
		wqe = mlx5_wq_ll_get_wqe(&rq->wq, wqe_counter);
		byte_cnt = be32_to_cpu(cqe->byte_cnt);

		bus_dmamap_sync(rq->dma_tag,
		    rq->mbuf[wqe_counter].dma_map,
		    BUS_DMASYNC_POSTREAD);

		if (unlikely((cqe->op_own >> 4) != MLX5_CQE_RESP_SEND)) {
			mlx5e_dump_err_cqe(&rq->cq, rq->rqn, (const void *)cqe);
			rq->stats.wqe_err++;
			goto wq_ll_pop;
		}
		if (pfil != NULL && PFIL_HOOKED_IN(pfil)) {
			seglen = MIN(byte_cnt, MLX5E_MAX_RX_BYTES);
			rv = pfil_mem_in(rq->channel->priv->pfil,
			    rq->mbuf[wqe_counter].data, seglen, rq->ifp, &mb);

			switch (rv) {
			case PFIL_DROPPED:
			case PFIL_CONSUMED:
				/*
				 * Filter dropped or consumed it. In
				 * either case, we can just recycle
				 * buffer; there is no more work to do.
				 */
				rq->stats.packets++;
				goto wq_ll_pop;
			case PFIL_REALLOCED:
				/*
				 * Filter copied it; recycle buffer
				 * and receive the new mbuf allocated
				 * by the Filter
				 */
				goto rx_common;
			default:
				/*
				 * The Filter said it was OK, so
				 * receive like normal.
				 */
				KASSERT(rv == PFIL_PASS,
					("Filter returned %d!\n", rv));
			}
		}
		if (!mlx5e_accel_ipsec_flow(cqe) /* tag is already assigned
						    to rq->mbuf */ &&
		    MHLEN - MLX5E_NET_IP_ALIGN >= byte_cnt &&
		    (mb = m_gethdr(M_NOWAIT, MT_DATA)) != NULL) {
			/* set maximum mbuf length */
			mb->m_len = MHLEN - MLX5E_NET_IP_ALIGN;
			/* get IP header aligned */
			mb->m_data += MLX5E_NET_IP_ALIGN;

			bcopy(rq->mbuf[wqe_counter].data, mtod(mb, caddr_t),
			    byte_cnt);
		} else {
			mb = rq->mbuf[wqe_counter].mbuf;
			rq->mbuf[wqe_counter].mbuf = NULL;	/* safety clear */

			bus_dmamap_unload(rq->dma_tag,
			    rq->mbuf[wqe_counter].dma_map);
		}
rx_common:
		mlx5e_build_rx_mbuf(cqe, rq, mb, &rq->mbuf[wqe_counter],
		    byte_cnt);
		rq->stats.bytes += byte_cnt;
		rq->stats.packets++;
#ifdef NUMA
		mb->m_pkthdr.numa_domain = if_getnumadomain(rq->ifp);
#endif

#if !defined(HAVE_TCP_LRO_RX)
		tcp_lro_queue_mbuf(&rq->lro, mb);
#else
		if (mb->m_pkthdr.csum_flags == 0 ||
		    (if_getcapenable(rq->ifp) & IFCAP_LRO) == 0 ||
		    rq->lro.lro_cnt == 0 ||
		    tcp_lro_rx(&rq->lro, mb, 0) != 0) {
			if_input(rq->ifp, mb);
		}
#endif
wq_ll_pop:
		mlx5_wq_ll_pop(&rq->wq, wqe_counter_be,
		    &wqe->next.next_wqe_index);
	}
	CURVNET_RESTORE();

	mlx5_cqwq_update_db_record(&rq->cq.wq);

	/* ensure cq space is freed before enabling more cqes */
	atomic_thread_fence_rel();
	return (i);
}

void
mlx5e_rx_cq_comp(struct mlx5_core_cq *mcq, struct mlx5_eqe *eqe __unused)
{
	struct mlx5e_channel *c = container_of(mcq, struct mlx5e_channel, rq.cq.mcq);
	struct mlx5e_rq *rq = container_of(mcq, struct mlx5e_rq, cq.mcq);
	int i = 0;

#ifdef HAVE_PER_CQ_EVENT_PACKET
#if (MHLEN < 15)
#error "MHLEN is too small"
#endif
	struct mbuf *mb = m_gethdr(M_NOWAIT, MT_DATA);

	if (mb != NULL) {
		/* this code is used for debugging purpose only */
		mb->m_pkthdr.len = mb->m_len = 15;
		memset(mb->m_data, 255, 14);
		mb->m_data[14] = rq->ix;
		mb->m_pkthdr.rcvif = rq->ifp;
		mb->m_pkthdr.leaf_rcvif = rq->ifp;
		if_input(rq->ifp, mb);
	}
#endif
	for (int j = 0; j != MLX5E_MAX_TX_NUM_TC; j++) {
		mtx_lock(&c->sq[j].lock);
		c->sq[j].db_inhibit++;
		mtx_unlock(&c->sq[j].lock);
	}

	mtx_lock(&c->iq.lock);
	c->iq.db_inhibit++;
	mtx_unlock(&c->iq.lock);

	mtx_lock(&rq->mtx);
	if (rq->enabled == 0)
		goto out;
	rq->processing++;

	/*
	 * Polling the entire CQ without posting new WQEs results in
	 * lack of receive WQEs during heavy traffic scenarios.
	 */
	while (1) {
		if (mlx5e_poll_rx_cq(rq, MLX5E_RX_BUDGET_MAX) !=
		    MLX5E_RX_BUDGET_MAX)
			break;
		i += MLX5E_RX_BUDGET_MAX;
		if (i >= MLX5E_BUDGET_MAX)
			break;
		mlx5e_post_rx_wqes(rq);
	}
	mlx5e_post_rx_wqes(rq);
	/* check for dynamic interrupt moderation callback */
	if (rq->dim.mode != NET_DIM_CQ_PERIOD_MODE_DISABLED)
		net_dim(&rq->dim, rq->stats.packets, rq->stats.bytes);
	mlx5e_cq_arm(&rq->cq, MLX5_GET_DOORBELL_LOCK(&rq->channel->priv->doorbell_lock));
	tcp_lro_flush_all(&rq->lro);
	rq->processing--;
out:
	mtx_unlock(&rq->mtx);

	for (int j = 0; j != MLX5E_MAX_TX_NUM_TC; j++) {
		mtx_lock(&c->sq[j].lock);
		c->sq[j].db_inhibit--;
		/* Update the doorbell record, if any. */
		mlx5e_tx_notify_hw(c->sq + j, true);
		mtx_unlock(&c->sq[j].lock);
	}

	mtx_lock(&c->iq.lock);
	c->iq.db_inhibit--;
	mlx5e_iq_notify_hw(&c->iq);
	mtx_unlock(&c->iq.lock);
}
