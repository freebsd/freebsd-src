/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2024 Google LLC
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_inet6.h"

#include "gve.h"
#include "gve_dqo.h"

static void
gve_unmap_packet(struct gve_tx_ring *tx,
    struct gve_tx_pending_pkt_dqo *pending_pkt)
{
	bus_dmamap_sync(tx->dqo.buf_dmatag, pending_pkt->dmamap,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(tx->dqo.buf_dmatag, pending_pkt->dmamap);
}

static void
gve_free_tx_mbufs_dqo(struct gve_tx_ring *tx)
{
	struct gve_tx_pending_pkt_dqo *pending_pkt;
	int i;

	for (i = 0; i < tx->dqo.num_pending_pkts; i++) {
		pending_pkt = &tx->dqo.pending_pkts[i];
		if (!pending_pkt->mbuf)
			continue;

		if (gve_is_qpl(tx->com.priv)) {
			pending_pkt->qpl_buf_head = -1;
			pending_pkt->num_qpl_bufs = 0;
		} else
			gve_unmap_packet(tx, pending_pkt);

		m_freem(pending_pkt->mbuf);
		pending_pkt->mbuf = NULL;
	}
}

void
gve_tx_free_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	int j;

	if (tx->dqo.desc_ring != NULL) {
		gve_dma_free_coherent(&tx->desc_ring_mem);
		tx->dqo.desc_ring = NULL;
	}

	if (tx->dqo.compl_ring != NULL) {
		gve_dma_free_coherent(&tx->dqo.compl_ring_mem);
		tx->dqo.compl_ring = NULL;
	}

	if (tx->dqo.pending_pkts != NULL) {
		gve_free_tx_mbufs_dqo(tx);

		if (!gve_is_qpl(priv) && tx->dqo.buf_dmatag) {
			for (j = 0; j < tx->dqo.num_pending_pkts; j++)
				if (tx->dqo.pending_pkts[j].state !=
				    GVE_PACKET_STATE_UNALLOCATED)
					bus_dmamap_destroy(tx->dqo.buf_dmatag,
					    tx->dqo.pending_pkts[j].dmamap);
		}

		free(tx->dqo.pending_pkts, M_GVE);
		tx->dqo.pending_pkts = NULL;
	}

	if (!gve_is_qpl(priv) && tx->dqo.buf_dmatag)
		bus_dma_tag_destroy(tx->dqo.buf_dmatag);

	if (gve_is_qpl(priv) && tx->dqo.qpl_bufs != NULL) {
		free(tx->dqo.qpl_bufs, M_GVE);
		tx->dqo.qpl_bufs = NULL;
	}
}

static int
gve_tx_alloc_rda_fields_dqo(struct gve_tx_ring *tx)
{
	struct gve_priv *priv = tx->com.priv;
	int err;
	int j;

	/*
	 * DMA tag for mapping Tx mbufs
	 * The maxsize, nsegments, and maxsegsize params should match
	 * the if_sethwtso* arguments in gve_setup_ifnet in gve_main.c.
	 */
	err = bus_dma_tag_create(
	    bus_get_dma_tag(priv->dev),	/* parent */
	    1, 0,			/* alignment, bounds */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    GVE_TSO_MAXSIZE_DQO,	/* maxsize */
	    GVE_TX_MAX_DATA_DESCS_DQO,	/* nsegments */
	    GVE_TX_MAX_BUF_SIZE_DQO,	/* maxsegsize */
	    BUS_DMA_ALLOCNOW,		/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &tx->dqo.buf_dmatag);
	if (err != 0) {
		device_printf(priv->dev, "%s: bus_dma_tag_create failed: %d\n",
		    __func__, err);
		return (err);
	}

	for (j = 0; j < tx->dqo.num_pending_pkts; j++) {
		err = bus_dmamap_create(tx->dqo.buf_dmatag, 0,
		    &tx->dqo.pending_pkts[j].dmamap);
		if (err != 0) {
			device_printf(priv->dev,
			    "err in creating pending pkt dmamap %d: %d",
			    j, err);
			return (err);
		}
		tx->dqo.pending_pkts[j].state = GVE_PACKET_STATE_FREE;
	}

	return (0);
}

int
gve_tx_alloc_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	uint16_t num_pending_pkts;
	int err;

	/* Descriptor ring */
	err = gve_dma_alloc_coherent(priv,
	    sizeof(union gve_tx_desc_dqo) * priv->tx_desc_cnt,
	    CACHE_LINE_SIZE, &tx->desc_ring_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc desc ring for tx ring %d", i);
		goto abort;
	}
	tx->dqo.desc_ring = tx->desc_ring_mem.cpu_addr;

	/* Completion ring */
	err = gve_dma_alloc_coherent(priv,
	    sizeof(struct gve_tx_compl_desc_dqo) * priv->tx_desc_cnt,
	    CACHE_LINE_SIZE, &tx->dqo.compl_ring_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc compl ring for tx ring %d", i);
		goto abort;
	}
	tx->dqo.compl_ring = tx->dqo.compl_ring_mem.cpu_addr;

	/*
	 * pending_pkts array
	 *
	 * The max number of pending packets determines the maximum number of
	 * descriptors which maybe written to the completion queue.
	 *
	 * We must set the number small enough to make sure we never overrun the
	 * completion queue.
	 */
	num_pending_pkts = priv->tx_desc_cnt;
	/*
	 * Reserve space for descriptor completions, which will be reported at
	 * most every GVE_TX_MIN_RE_INTERVAL packets.
	 */
	num_pending_pkts -= num_pending_pkts / GVE_TX_MIN_RE_INTERVAL;

	tx->dqo.num_pending_pkts = num_pending_pkts;
	tx->dqo.pending_pkts = malloc(
	    sizeof(struct gve_tx_pending_pkt_dqo) * num_pending_pkts,
	    M_GVE, M_WAITOK | M_ZERO);

	if (gve_is_qpl(priv)) {
		int qpl_buf_cnt;

		tx->com.qpl = &priv->qpls[i];
		qpl_buf_cnt = GVE_TX_BUFS_PER_PAGE_DQO *
		    tx->com.qpl->num_pages;

		tx->dqo.qpl_bufs = malloc(
		    sizeof(*tx->dqo.qpl_bufs) * qpl_buf_cnt,
		    M_GVE, M_WAITOK | M_ZERO);
	} else
		gve_tx_alloc_rda_fields_dqo(tx);
	return (0);

abort:
	gve_tx_free_ring_dqo(priv, i);
	return (err);
}

static void
gve_extract_tx_metadata_dqo(const struct mbuf *mbuf,
    struct gve_tx_metadata_dqo *metadata)
{
	uint32_t hash = mbuf->m_pkthdr.flowid;
	uint16_t path_hash;

	metadata->version = GVE_TX_METADATA_VERSION_DQO;
	if (hash) {
		path_hash = hash ^ (hash >> 16);

		path_hash &= (1 << 15) - 1;
		if (__predict_false(path_hash == 0))
			path_hash = ~path_hash;

		metadata->path_hash = path_hash;
	}
}

static void
gve_tx_fill_pkt_desc_dqo(struct gve_tx_ring *tx,
    uint32_t *desc_idx, uint32_t len, uint64_t addr,
    int16_t compl_tag, bool eop, bool csum_enabled)
{
	while (len > 0) {
		struct gve_tx_pkt_desc_dqo *desc =
		    &tx->dqo.desc_ring[*desc_idx].pkt;
		uint32_t cur_len = MIN(len, GVE_TX_MAX_BUF_SIZE_DQO);
		bool cur_eop = eop && cur_len == len;

		*desc = (struct gve_tx_pkt_desc_dqo){
			.buf_addr = htole64(addr),
			.dtype = GVE_TX_PKT_DESC_DTYPE_DQO,
			.end_of_packet = cur_eop,
			.checksum_offload_enable = csum_enabled,
			.compl_tag = htole16(compl_tag),
			.buf_size = cur_len,
		};

		addr += cur_len;
		len -= cur_len;
		*desc_idx = (*desc_idx + 1) & tx->dqo.desc_mask;
	}
}

static void
gve_tx_fill_tso_ctx_desc(struct gve_tx_tso_context_desc_dqo *desc,
    const struct mbuf *mbuf, const struct gve_tx_metadata_dqo *metadata,
    int header_len)
{
	*desc = (struct gve_tx_tso_context_desc_dqo){
		.header_len = header_len,
		.cmd_dtype = {
			.dtype = GVE_TX_TSO_CTX_DESC_DTYPE_DQO,
			.tso = 1,
		},
		.flex0 = metadata->bytes[0],
		.flex5 = metadata->bytes[5],
		.flex6 = metadata->bytes[6],
		.flex7 = metadata->bytes[7],
		.flex8 = metadata->bytes[8],
		.flex9 = metadata->bytes[9],
		.flex10 = metadata->bytes[10],
		.flex11 = metadata->bytes[11],
	};
	desc->tso_total_len = mbuf->m_pkthdr.len - header_len;
	desc->mss = mbuf->m_pkthdr.tso_segsz;
}

static void
gve_tx_fill_general_ctx_desc(struct gve_tx_general_context_desc_dqo *desc,
    const struct gve_tx_metadata_dqo *metadata)
{
	*desc = (struct gve_tx_general_context_desc_dqo){
		.flex0 = metadata->bytes[0],
		.flex1 = metadata->bytes[1],
		.flex2 = metadata->bytes[2],
		.flex3 = metadata->bytes[3],
		.flex4 = metadata->bytes[4],
		.flex5 = metadata->bytes[5],
		.flex6 = metadata->bytes[6],
		.flex7 = metadata->bytes[7],
		.flex8 = metadata->bytes[8],
		.flex9 = metadata->bytes[9],
		.flex10 = metadata->bytes[10],
		.flex11 = metadata->bytes[11],
		.cmd_dtype = {.dtype = GVE_TX_GENERAL_CTX_DESC_DTYPE_DQO},
	};
}

#define PULLUP_HDR(m, len)				\
do {							\
	if (__predict_false((m)->m_len < (len))) {	\
		(m) = m_pullup((m), (len));		\
		if ((m) == NULL)			\
			return (EINVAL);		\
	}						\
} while (0)

static int
gve_prep_tso(struct mbuf *mbuf, int *header_len)
{
	uint8_t l3_off, l4_off = 0;
	struct ether_header *eh;
	struct tcphdr *th;
	u_short csum;

	PULLUP_HDR(mbuf, sizeof(*eh));
	eh = mtod(mbuf, struct ether_header *);
	KASSERT(eh->ether_type != ETHERTYPE_VLAN,
	    ("VLAN-tagged packets not supported"));
	l3_off = ETHER_HDR_LEN;

#ifdef INET6
	if (ntohs(eh->ether_type) == ETHERTYPE_IPV6) {
		struct ip6_hdr *ip6;

		PULLUP_HDR(mbuf, l3_off + sizeof(*ip6));
		ip6 = (struct ip6_hdr *)(mtodo(mbuf, l3_off));
		l4_off = l3_off + sizeof(struct ip6_hdr);
		csum = in6_cksum_pseudo(ip6, /*len=*/0, IPPROTO_TCP,
		    /*csum=*/0);
	} else
#endif
	if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		struct ip *ip;

		PULLUP_HDR(mbuf, l3_off + sizeof(*ip));
		ip = (struct ip *)(mtodo(mbuf, l3_off));
		l4_off = l3_off + (ip->ip_hl << 2);
		csum = in_pseudo(ip->ip_src.s_addr, ip->ip_dst.s_addr,
		    htons(IPPROTO_TCP));
	}

	PULLUP_HDR(mbuf, l4_off + sizeof(struct tcphdr *));
	th = (struct tcphdr *)(mtodo(mbuf, l4_off));
	*header_len = l4_off + (th->th_off << 2);

	/*
	 * Hardware requires the th->th_sum to not include the TCP payload,
	 * hence we recompute the csum with it excluded.
	 */
	th->th_sum = csum;

	return (0);
}

static int
gve_tx_fill_ctx_descs(struct gve_tx_ring *tx, struct mbuf *mbuf,
    bool is_tso, uint32_t *desc_idx)
{
	struct gve_tx_general_context_desc_dqo *gen_desc;
	struct gve_tx_tso_context_desc_dqo *tso_desc;
	struct gve_tx_metadata_dqo metadata;
	int header_len;
	int err;

	metadata = (struct gve_tx_metadata_dqo){0};
	gve_extract_tx_metadata_dqo(mbuf, &metadata);

	if (is_tso) {
		err = gve_prep_tso(mbuf, &header_len);
		if (__predict_false(err)) {
			counter_enter();
			counter_u64_add_protected(
			    tx->stats.tx_delayed_pkt_tsoerr, 1);
			counter_exit();
			return (err);
		}

		tso_desc = &tx->dqo.desc_ring[*desc_idx].tso_ctx;
		gve_tx_fill_tso_ctx_desc(tso_desc, mbuf, &metadata, header_len);

		*desc_idx = (*desc_idx + 1) & tx->dqo.desc_mask;
		counter_enter();
		counter_u64_add_protected(tx->stats.tso_packet_cnt, 1);
		counter_exit();
	}

	gen_desc = &tx->dqo.desc_ring[*desc_idx].general_ctx;
	gve_tx_fill_general_ctx_desc(gen_desc, &metadata);
	*desc_idx = (*desc_idx + 1) & tx->dqo.desc_mask;
	return (0);
}

static int
gve_map_mbuf_dqo(struct gve_tx_ring *tx,
    struct mbuf **mbuf, bus_dmamap_t dmamap,
    bus_dma_segment_t *segs, int *nsegs, int attempt)
{
	struct mbuf *m_new = NULL;
	int err;

	err = bus_dmamap_load_mbuf_sg(tx->dqo.buf_dmatag, dmamap,
	    *mbuf, segs, nsegs, BUS_DMA_NOWAIT);

	switch (err) {
	case __predict_true(0):
		break;
	case EFBIG:
		if (__predict_false(attempt > 0))
			goto abort;

		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_mbuf_collapse, 1);
		counter_exit();

		/* Try m_collapse before m_defrag */
		m_new = m_collapse(*mbuf, M_NOWAIT,
		    GVE_TX_MAX_DATA_DESCS_DQO);
		if (m_new == NULL) {
			counter_enter();
			counter_u64_add_protected(
			    tx->stats.tx_mbuf_defrag, 1);
			counter_exit();
			m_new = m_defrag(*mbuf, M_NOWAIT);
		}

		if (__predict_false(m_new == NULL)) {
			counter_enter();
			counter_u64_add_protected(
			    tx->stats.tx_mbuf_defrag_err, 1);
			counter_exit();

			m_freem(*mbuf);
			*mbuf = NULL;
			err = ENOMEM;
			goto abort;
		} else {
			*mbuf = m_new;
			return (gve_map_mbuf_dqo(tx, mbuf, dmamap,
			    segs, nsegs, ++attempt));
		}
	case ENOMEM:
		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_mbuf_dmamap_enomem_err, 1);
		counter_exit();
		goto abort;
	default:
		goto abort;
	}

	return (0);

abort:
	counter_enter();
	counter_u64_add_protected(tx->stats.tx_mbuf_dmamap_err, 1);
	counter_exit();
	return (err);
}

static uint32_t
num_avail_desc_ring_slots(const struct gve_tx_ring *tx)
{
	uint32_t num_used = (tx->dqo.desc_tail - tx->dqo.desc_head) &
	    tx->dqo.desc_mask;

	return (tx->dqo.desc_mask - num_used);
}

static struct gve_tx_pending_pkt_dqo *
gve_alloc_pending_packet(struct gve_tx_ring *tx)
{
	int32_t index = tx->dqo.free_pending_pkts_csm;
	struct gve_tx_pending_pkt_dqo *pending_pkt;

	/*
	 * No pending packets available in the consumer list,
	 * try to steal the producer list.
	 */
	if (__predict_false(index == -1)) {
		tx->dqo.free_pending_pkts_csm = atomic_swap_32(
		    &tx->dqo.free_pending_pkts_prd, -1);

		index = tx->dqo.free_pending_pkts_csm;
		if (__predict_false(index == -1))
			return (NULL);
	}

	pending_pkt = &tx->dqo.pending_pkts[index];

	/* Remove pending_pkt from the consumer list */
	tx->dqo.free_pending_pkts_csm = pending_pkt->next;
	pending_pkt->state = GVE_PACKET_STATE_PENDING_DATA_COMPL;

	return (pending_pkt);
}

static void
gve_free_pending_packet(struct gve_tx_ring *tx,
    struct gve_tx_pending_pkt_dqo *pending_pkt)
{
	int index = pending_pkt - tx->dqo.pending_pkts;
	int32_t old_head;

	pending_pkt->state = GVE_PACKET_STATE_FREE;

	/* Add pending_pkt to the producer list */
	while (true) {
		old_head = atomic_load_acq_32(&tx->dqo.free_pending_pkts_prd);

		pending_pkt->next = old_head;
		if (atomic_cmpset_32(&tx->dqo.free_pending_pkts_prd,
		    old_head, index))
			break;
	}
}

/*
 * Has the side-effect of retrieving the value of the last desc index
 * processed by the NIC. hw_tx_head is written to by the completions-processing
 * taskqueue upon receiving descriptor-completions.
 */
static bool
gve_tx_has_desc_room_dqo(struct gve_tx_ring *tx, int needed_descs)
{
	if (needed_descs <= num_avail_desc_ring_slots(tx))
		return (true);

	tx->dqo.desc_head = atomic_load_acq_32(&tx->dqo.hw_tx_head);
	if (needed_descs > num_avail_desc_ring_slots(tx)) {
		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_delayed_pkt_nospace_descring, 1);
		counter_exit();
		return (false);
	}

	return (0);
}

static void
gve_tx_request_desc_compl(struct gve_tx_ring *tx, uint32_t desc_idx)
{
	uint32_t last_report_event_interval;
	uint32_t last_desc_idx;

	last_desc_idx = (desc_idx - 1) & tx->dqo.desc_mask;
	last_report_event_interval =
	    (last_desc_idx - tx->dqo.last_re_idx) & tx->dqo.desc_mask;

	if (__predict_false(last_report_event_interval >=
	    GVE_TX_MIN_RE_INTERVAL)) {
		tx->dqo.desc_ring[last_desc_idx].pkt.report_event = true;
		tx->dqo.last_re_idx = last_desc_idx;
	}
}

static bool
gve_tx_have_enough_qpl_bufs(struct gve_tx_ring *tx, int num_bufs)
{
	uint32_t available = tx->dqo.qpl_bufs_produced_cached -
	    tx->dqo.qpl_bufs_consumed;

	if (__predict_true(available >= num_bufs))
		return (true);

	tx->dqo.qpl_bufs_produced_cached = atomic_load_acq_32(
	    &tx->dqo.qpl_bufs_produced);
	available = tx->dqo.qpl_bufs_produced_cached -
	    tx->dqo.qpl_bufs_consumed;

	if (__predict_true(available >= num_bufs))
		return (true);
	return (false);
}

static int32_t
gve_tx_alloc_qpl_buf(struct gve_tx_ring *tx)
{
	int32_t buf = tx->dqo.free_qpl_bufs_csm;

	if (__predict_false(buf == -1)) {
		tx->dqo.free_qpl_bufs_csm = atomic_swap_32(
		    &tx->dqo.free_qpl_bufs_prd, -1);
		buf = tx->dqo.free_qpl_bufs_csm;
		if (__predict_false(buf == -1))
			return (-1);
	}

	tx->dqo.free_qpl_bufs_csm = tx->dqo.qpl_bufs[buf];
	tx->dqo.qpl_bufs_consumed++;
	return (buf);
}

/*
 * Tx buffer i corresponds to
 * qpl_page_id = i / GVE_TX_BUFS_PER_PAGE_DQO
 * qpl_page_offset = (i % GVE_TX_BUFS_PER_PAGE_DQO) * GVE_TX_BUF_SIZE_DQO
 */
static void
gve_tx_buf_get_addr_dqo(struct gve_tx_ring *tx,
    int32_t index, void **va, bus_addr_t *dma_addr)
{
	int page_id = index >> (PAGE_SHIFT - GVE_TX_BUF_SHIFT_DQO);
	int offset = (index & (GVE_TX_BUFS_PER_PAGE_DQO - 1)) <<
	    GVE_TX_BUF_SHIFT_DQO;

	*va = (char *)tx->com.qpl->dmas[page_id].cpu_addr + offset;
	*dma_addr = tx->com.qpl->dmas[page_id].bus_addr + offset;
}

static struct gve_dma_handle *
gve_get_page_dma_handle(struct gve_tx_ring *tx, int32_t index)
{
	int page_id = index >> (PAGE_SHIFT - GVE_TX_BUF_SHIFT_DQO);

	return (&tx->com.qpl->dmas[page_id]);
}

static void
gve_tx_copy_mbuf_and_write_pkt_descs(struct gve_tx_ring *tx,
    struct mbuf *mbuf, struct gve_tx_pending_pkt_dqo *pkt,
    bool csum_enabled, int16_t completion_tag,
    uint32_t *desc_idx)
{
	int32_t pkt_len = mbuf->m_pkthdr.len;
	struct gve_dma_handle *dma;
	uint32_t copy_offset = 0;
	int32_t prev_buf = -1;
	uint32_t copy_len;
	bus_addr_t addr;
	int32_t buf;
	void *va;

	MPASS(pkt->num_qpl_bufs == 0);
	MPASS(pkt->qpl_buf_head == -1);

	while (copy_offset < pkt_len) {
		buf = gve_tx_alloc_qpl_buf(tx);
		/* We already checked for availability */
		MPASS(buf != -1);

		gve_tx_buf_get_addr_dqo(tx, buf, &va, &addr);
		copy_len = MIN(GVE_TX_BUF_SIZE_DQO, pkt_len - copy_offset);
		m_copydata(mbuf, copy_offset, copy_len, va);
		copy_offset += copy_len;

		dma = gve_get_page_dma_handle(tx, buf);
		bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);

		gve_tx_fill_pkt_desc_dqo(tx, desc_idx,
		    copy_len, addr, completion_tag,
		    /*eop=*/copy_offset == pkt_len,
		    csum_enabled);

		/* Link all the qpl bufs for a packet */
		if (prev_buf == -1)
			pkt->qpl_buf_head = buf;
		else
			tx->dqo.qpl_bufs[prev_buf] = buf;

		prev_buf = buf;
		pkt->num_qpl_bufs++;
	}

	tx->dqo.qpl_bufs[buf] = -1;
}

int
gve_xmit_dqo_qpl(struct gve_tx_ring *tx, struct mbuf *mbuf)
{
	uint32_t desc_idx = tx->dqo.desc_tail;
	struct gve_tx_pending_pkt_dqo *pkt;
	int total_descs_needed;
	int16_t completion_tag;
	bool has_csum_flag;
	int csum_flags;
	bool is_tso;
	int nsegs;
	int err;

	csum_flags = mbuf->m_pkthdr.csum_flags;
	has_csum_flag = csum_flags & (CSUM_TCP | CSUM_UDP |
	    CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_TSO);
	is_tso = csum_flags & CSUM_TSO;

	nsegs = howmany(mbuf->m_pkthdr.len, GVE_TX_BUF_SIZE_DQO);
	/* Check if we have enough room in the desc ring */
	total_descs_needed = 1 +     /* general_ctx_desc */
	    nsegs +		     /* pkt_desc */
	    (is_tso ? 1 : 0);        /* tso_ctx_desc */
	if (__predict_false(!gve_tx_has_desc_room_dqo(tx, total_descs_needed)))
		return (ENOBUFS);

	if (!gve_tx_have_enough_qpl_bufs(tx, nsegs)) {
		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_delayed_pkt_nospace_qpl_bufs, 1);
		counter_exit();
		return (ENOBUFS);
	}

	pkt = gve_alloc_pending_packet(tx);
	if (pkt == NULL) {
		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_delayed_pkt_nospace_compring, 1);
		counter_exit();
		return (ENOBUFS);
	}
	completion_tag = pkt - tx->dqo.pending_pkts;
	pkt->mbuf = mbuf;

	err = gve_tx_fill_ctx_descs(tx, mbuf, is_tso, &desc_idx);
	if (err)
		goto abort;

	gve_tx_copy_mbuf_and_write_pkt_descs(tx, mbuf, pkt,
	    has_csum_flag, completion_tag, &desc_idx);

	/* Remember the index of the last desc written */
	tx->dqo.desc_tail = desc_idx;

	/*
	 * Request a descriptor completion on the last descriptor of the
	 * packet if we are allowed to by the HW enforced interval.
	 */
	gve_tx_request_desc_compl(tx, desc_idx);

	tx->req += total_descs_needed; /* tx->req is just a sysctl counter */
	return (0);

abort:
	pkt->mbuf = NULL;
	gve_free_pending_packet(tx, pkt);
	return (err);
}

int
gve_xmit_dqo(struct gve_tx_ring *tx, struct mbuf **mbuf_ptr)
{
	bus_dma_segment_t segs[GVE_TX_MAX_DATA_DESCS_DQO];
	uint32_t desc_idx = tx->dqo.desc_tail;
	struct gve_tx_pending_pkt_dqo *pkt;
	struct mbuf *mbuf = *mbuf_ptr;
	int total_descs_needed;
	int16_t completion_tag;
	bool has_csum_flag;
	int csum_flags;
	bool is_tso;
	int nsegs;
	int err;
	int i;

	csum_flags = mbuf->m_pkthdr.csum_flags;
	has_csum_flag = csum_flags & (CSUM_TCP | CSUM_UDP |
	    CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_TSO);
	is_tso = csum_flags & CSUM_TSO;

	/*
	 * This mbuf might end up needing more than 1 pkt desc.
	 * The actual number, `nsegs` is known only after the
	 * expensive gve_map_mbuf_dqo call. This check beneath
	 * exists to fail early when the desc ring is really full.
	 */
	total_descs_needed = 1 +     /* general_ctx_desc */
	    1 +			     /* pkt_desc */
	    (is_tso ? 1 : 0);        /* tso_ctx_desc */
	if (__predict_false(!gve_tx_has_desc_room_dqo(tx, total_descs_needed)))
		return (ENOBUFS);

	pkt = gve_alloc_pending_packet(tx);
	if (pkt == NULL) {
		counter_enter();
		counter_u64_add_protected(
		    tx->stats.tx_delayed_pkt_nospace_compring, 1);
		counter_exit();
		return (ENOBUFS);
	}
	completion_tag = pkt - tx->dqo.pending_pkts;

	err = gve_map_mbuf_dqo(tx, mbuf_ptr, pkt->dmamap,
	    segs, &nsegs, /*attempt=*/0);
	if (err)
		goto abort;
	mbuf = *mbuf_ptr;  /* gve_map_mbuf_dqo might replace the mbuf chain */
	pkt->mbuf = mbuf;

	total_descs_needed = 1 + /* general_ctx_desc */
	    nsegs +              /* pkt_desc */
	    (is_tso ? 1 : 0);    /* tso_ctx_desc */
	if (__predict_false(
	    !gve_tx_has_desc_room_dqo(tx, total_descs_needed))) {
		err = ENOBUFS;
		goto abort_with_dma;
	}

	err = gve_tx_fill_ctx_descs(tx, mbuf, is_tso, &desc_idx);
	if (err)
		goto abort_with_dma;

	bus_dmamap_sync(tx->dqo.buf_dmatag, pkt->dmamap, BUS_DMASYNC_PREWRITE);
	for (i = 0; i < nsegs; i++) {
		gve_tx_fill_pkt_desc_dqo(tx, &desc_idx,
		    segs[i].ds_len, segs[i].ds_addr,
		    completion_tag, /*eop=*/i == (nsegs - 1),
		    has_csum_flag);
	}

	/* Remember the index of the last desc written */
	tx->dqo.desc_tail = desc_idx;

	/*
	 * Request a descriptor completion on the last descriptor of the
	 * packet if we are allowed to by the HW enforced interval.
	 */
	gve_tx_request_desc_compl(tx, desc_idx);

	tx->req += total_descs_needed; /* tx->req is just a sysctl counter */
	return (0);

abort_with_dma:
	gve_unmap_packet(tx, pkt);
abort:
	pkt->mbuf = NULL;
	gve_free_pending_packet(tx, pkt);
	return (err);
}

static void
gve_reap_qpl_bufs_dqo(struct gve_tx_ring *tx,
    struct gve_tx_pending_pkt_dqo *pkt)
{
	int32_t buf = pkt->qpl_buf_head;
	struct gve_dma_handle *dma;
	int32_t qpl_buf_tail;
	int32_t old_head;
	int i;

	for (i = 0; i < pkt->num_qpl_bufs; i++) {
		dma = gve_get_page_dma_handle(tx, buf);
		bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_POSTWRITE);
		qpl_buf_tail = buf;
		buf = tx->dqo.qpl_bufs[buf];
	}
	MPASS(buf == -1);
	buf = qpl_buf_tail;

	while (true) {
		old_head = atomic_load_32(&tx->dqo.free_qpl_bufs_prd);
		tx->dqo.qpl_bufs[buf] = old_head;

		/*
		 * The "rel" ensures that the update to dqo.free_qpl_bufs_prd
		 * is visible only after the linked list from this pkt is
		 * attached above to old_head.
		 */
		if (atomic_cmpset_rel_32(&tx->dqo.free_qpl_bufs_prd,
		    old_head, pkt->qpl_buf_head))
			break;
	}
	/*
	 * The "rel" ensures that the update to dqo.qpl_bufs_produced is
	 * visible only adter the update to dqo.free_qpl_bufs_prd above.
	 */
	atomic_add_rel_32(&tx->dqo.qpl_bufs_produced, pkt->num_qpl_bufs);

	pkt->qpl_buf_head = -1;
	pkt->num_qpl_bufs = 0;
}

static uint64_t
gve_handle_packet_completion(struct gve_priv *priv,
    struct gve_tx_ring *tx, uint16_t compl_tag)
{
	struct gve_tx_pending_pkt_dqo *pending_pkt;
	int32_t pkt_len;

	if (__predict_false(compl_tag >= tx->dqo.num_pending_pkts)) {
		device_printf(priv->dev, "Invalid TX completion tag: %d\n",
		    compl_tag);
		return (0);
	}

	pending_pkt = &tx->dqo.pending_pkts[compl_tag];

	/* Packet is allocated but not pending data completion. */
	if (__predict_false(pending_pkt->state !=
	    GVE_PACKET_STATE_PENDING_DATA_COMPL)) {
		device_printf(priv->dev,
		    "No pending data completion: %d\n", compl_tag);
		return (0);
	}

	pkt_len = pending_pkt->mbuf->m_pkthdr.len;

	if (gve_is_qpl(priv))
		gve_reap_qpl_bufs_dqo(tx, pending_pkt);
	else
		gve_unmap_packet(tx, pending_pkt);

	m_freem(pending_pkt->mbuf);
	pending_pkt->mbuf = NULL;
	gve_free_pending_packet(tx, pending_pkt);
	return (pkt_len);
}

int
gve_tx_intr_dqo(void *arg)
{
	struct gve_tx_ring *tx = arg;
	struct gve_priv *priv = tx->com.priv;
	struct gve_ring_com *com = &tx->com;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return (FILTER_STRAY);

	/* Interrupts are automatically masked */
	taskqueue_enqueue(com->cleanup_tq, &com->cleanup_task);
	return (FILTER_HANDLED);
}

static void
gve_tx_clear_desc_ring_dqo(struct gve_tx_ring *tx)
{
	struct gve_ring_com *com = &tx->com;
	int i;

	for (i = 0; i < com->priv->tx_desc_cnt; i++)
		tx->dqo.desc_ring[i] = (union gve_tx_desc_dqo){};

	bus_dmamap_sync(tx->desc_ring_mem.tag, tx->desc_ring_mem.map,
	    BUS_DMASYNC_PREWRITE);
}

static void
gve_tx_clear_compl_ring_dqo(struct gve_tx_ring *tx)
{
	struct gve_ring_com *com = &tx->com;
	int entries;
	int i;

	entries = com->priv->tx_desc_cnt;
	for (i = 0; i < entries; i++)
		tx->dqo.compl_ring[i] = (struct gve_tx_compl_desc_dqo){};

	bus_dmamap_sync(tx->dqo.compl_ring_mem.tag, tx->dqo.compl_ring_mem.map,
	    BUS_DMASYNC_PREWRITE);
}

void
gve_clear_tx_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	int j;

	tx->dqo.desc_head = 0;
	tx->dqo.desc_tail = 0;
	tx->dqo.desc_mask = priv->tx_desc_cnt - 1;
	tx->dqo.last_re_idx = 0;

	tx->dqo.compl_head = 0;
	tx->dqo.compl_mask = priv->tx_desc_cnt - 1;
	atomic_store_32(&tx->dqo.hw_tx_head, 0);
	tx->dqo.cur_gen_bit = 0;

	gve_free_tx_mbufs_dqo(tx);

	for (j = 0; j < tx->dqo.num_pending_pkts - 1; j++) {
		tx->dqo.pending_pkts[j].next = j + 1;
		tx->dqo.pending_pkts[j].state = GVE_PACKET_STATE_FREE;
	}
	tx->dqo.pending_pkts[tx->dqo.num_pending_pkts - 1].next = -1;
	tx->dqo.free_pending_pkts_csm = 0;
	atomic_store_rel_32(&tx->dqo.free_pending_pkts_prd, -1);

	if (gve_is_qpl(priv)) {
		int qpl_buf_cnt = GVE_TX_BUFS_PER_PAGE_DQO *
		    tx->com.qpl->num_pages;

		for (j = 0; j < qpl_buf_cnt - 1; j++)
			tx->dqo.qpl_bufs[j] = j + 1;
		tx->dqo.qpl_bufs[j] = -1;

		tx->dqo.free_qpl_bufs_csm = 0;
		atomic_store_32(&tx->dqo.free_qpl_bufs_prd, -1);
		atomic_store_32(&tx->dqo.qpl_bufs_produced, qpl_buf_cnt);
		tx->dqo.qpl_bufs_produced_cached = qpl_buf_cnt;
		tx->dqo.qpl_bufs_consumed = 0;
	}

	gve_tx_clear_desc_ring_dqo(tx);
	gve_tx_clear_compl_ring_dqo(tx);
}

static bool
gve_tx_cleanup_dqo(struct gve_priv *priv, struct gve_tx_ring *tx, int budget)
{
	struct gve_tx_compl_desc_dqo *compl_desc;
	uint64_t bytes_done = 0;
	uint64_t pkts_done = 0;
	uint16_t compl_tag;
	int work_done = 0;
	uint16_t tx_head;
	uint16_t type;

	while (work_done < budget) {
		bus_dmamap_sync(tx->dqo.compl_ring_mem.tag, tx->dqo.compl_ring_mem.map,
		    BUS_DMASYNC_POSTREAD);

		compl_desc = &tx->dqo.compl_ring[tx->dqo.compl_head];
		if (compl_desc->generation == tx->dqo.cur_gen_bit)
			break;

		/*
		 * Prevent generation bit from being read after the rest of the
		 * descriptor.
		 */
		rmb();
		type = compl_desc->type;

		if (type == GVE_COMPL_TYPE_DQO_DESC) {
			/* This is the last descriptor fetched by HW plus one */
			tx_head = le16toh(compl_desc->tx_head);
			atomic_store_rel_32(&tx->dqo.hw_tx_head, tx_head);
		} else if (type == GVE_COMPL_TYPE_DQO_PKT) {
			compl_tag = le16toh(compl_desc->completion_tag);
			bytes_done += gve_handle_packet_completion(priv,
			    tx, compl_tag);
			pkts_done++;
		}

		tx->dqo.compl_head = (tx->dqo.compl_head + 1) &
		    tx->dqo.compl_mask;
		/* Flip the generation bit when we wrap around */
		tx->dqo.cur_gen_bit ^= tx->dqo.compl_head == 0;
		work_done++;
	}

	/*
	 * Waking the xmit taskqueue has to occur after room has been made in
	 * the queue.
	 */
	atomic_thread_fence_seq_cst();
	if (atomic_load_bool(&tx->stopped) && work_done) {
		atomic_store_bool(&tx->stopped, false);
		taskqueue_enqueue(tx->xmit_tq, &tx->xmit_task);
	}

	tx->done += work_done; /* tx->done is just a sysctl counter */
	counter_enter();
	counter_u64_add_protected(tx->stats.tbytes, bytes_done);
	counter_u64_add_protected(tx->stats.tpackets, pkts_done);
	counter_exit();

	return (work_done == budget);
}

void
gve_tx_cleanup_tq_dqo(void *arg, int pending)
{
	struct gve_tx_ring *tx = arg;
	struct gve_priv *priv = tx->com.priv;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return;

	if (gve_tx_cleanup_dqo(priv, tx, /*budget=*/1024)) {
		taskqueue_enqueue(tx->com.cleanup_tq, &tx->com.cleanup_task);
		return;
	}

	gve_db_bar_dqo_write_4(priv, tx->com.irq_db_offset,
	    GVE_ITR_NO_UPDATE_DQO | GVE_ITR_ENABLE_BIT_DQO);
}
