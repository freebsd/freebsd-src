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
#include "gve.h"
#include "gve_adminq.h"
#include "gve_dqo.h"

static void
gve_free_rx_mbufs_dqo(struct gve_rx_ring *rx)
{
	struct gve_rx_buf_dqo *buf;
	int i;

	if (gve_is_qpl(rx->com.priv))
		return;

	for (i = 0; i < rx->dqo.buf_cnt; i++) {
		buf = &rx->dqo.bufs[i];
		if (!buf->mbuf)
			continue;

		bus_dmamap_sync(rx->dqo.buf_dmatag, buf->dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rx->dqo.buf_dmatag, buf->dmamap);
		m_freem(buf->mbuf);
		buf->mbuf = NULL;
	}
}

void
gve_rx_free_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_rx_ring *rx = &priv->rx[i];
	int j;

	if (rx->dqo.compl_ring != NULL) {
		gve_dma_free_coherent(&rx->dqo.compl_ring_mem);
		rx->dqo.compl_ring = NULL;
	}

	if (rx->dqo.desc_ring != NULL) {
		gve_dma_free_coherent(&rx->desc_ring_mem);
		rx->dqo.desc_ring = NULL;
	}

	if (rx->dqo.bufs != NULL) {
		gve_free_rx_mbufs_dqo(rx);

		if (!gve_is_qpl(priv) && rx->dqo.buf_dmatag) {
			for (j = 0; j < rx->dqo.buf_cnt; j++)
				if (rx->dqo.bufs[j].mapped)
					bus_dmamap_destroy(rx->dqo.buf_dmatag,
					    rx->dqo.bufs[j].dmamap);
		}

		free(rx->dqo.bufs, M_GVE);
		rx->dqo.bufs = NULL;
	}

	if (!gve_is_qpl(priv) && rx->dqo.buf_dmatag)
		bus_dma_tag_destroy(rx->dqo.buf_dmatag);
}

int
gve_rx_alloc_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_rx_ring *rx = &priv->rx[i];
	int err;
	int j;

	err = gve_dma_alloc_coherent(priv,
	    sizeof(struct gve_rx_desc_dqo) * priv->rx_desc_cnt,
	    CACHE_LINE_SIZE, &rx->desc_ring_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc desc ring for rx ring %d", i);
		goto abort;
	}
	rx->dqo.desc_ring = rx->desc_ring_mem.cpu_addr;
	rx->dqo.mask = priv->rx_desc_cnt - 1;

	err = gve_dma_alloc_coherent(priv,
	    sizeof(struct gve_rx_compl_desc_dqo) * priv->rx_desc_cnt,
	    CACHE_LINE_SIZE, &rx->dqo.compl_ring_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc compl ring for rx ring %d", i);
		goto abort;
	}
	rx->dqo.compl_ring = rx->dqo.compl_ring_mem.cpu_addr;
	rx->dqo.mask = priv->rx_desc_cnt - 1;

	rx->dqo.buf_cnt = gve_is_qpl(priv) ? GVE_RX_NUM_QPL_PAGES_DQO :
	    priv->rx_desc_cnt;
	rx->dqo.bufs = malloc(rx->dqo.buf_cnt * sizeof(struct gve_rx_buf_dqo),
	    M_GVE, M_WAITOK | M_ZERO);

	if (gve_is_qpl(priv)) {
		rx->com.qpl = &priv->qpls[priv->tx_cfg.max_queues + i];
		if (rx->com.qpl == NULL) {
			device_printf(priv->dev, "No QPL left for rx ring %d", i);
			return (ENOMEM);
		}
		return (0);
	}

	err = bus_dma_tag_create(
	    bus_get_dma_tag(priv->dev),	/* parent */
	    1, 0,			/* alignment, bounds */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES,			/* maxsize */
	    1,				/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL,			/* lockfunc */
	    NULL,			/* lockarg */
	    &rx->dqo.buf_dmatag);
	if (err != 0) {
		device_printf(priv->dev,
		    "%s: bus_dma_tag_create failed: %d\n",
		    __func__, err);
		goto abort;
	}

	for (j = 0; j < rx->dqo.buf_cnt; j++) {
		err = bus_dmamap_create(rx->dqo.buf_dmatag, 0,
		    &rx->dqo.bufs[j].dmamap);
		if (err != 0) {
			device_printf(priv->dev,
			    "err in creating rx buf dmamap %d: %d",
			    j, err);
			goto abort;
		}
		rx->dqo.bufs[j].mapped = true;
	}

	return (0);

abort:
	gve_rx_free_ring_dqo(priv, i);
	return (err);
}

static void
gve_rx_clear_desc_ring_dqo(struct gve_rx_ring *rx)
{
	struct gve_ring_com *com = &rx->com;
	int entries;
	int i;

	entries = com->priv->rx_desc_cnt;
	for (i = 0; i < entries; i++)
		rx->dqo.desc_ring[i] = (struct gve_rx_desc_dqo){};

	bus_dmamap_sync(rx->desc_ring_mem.tag, rx->desc_ring_mem.map,
	    BUS_DMASYNC_PREWRITE);
}

static void
gve_rx_clear_compl_ring_dqo(struct gve_rx_ring *rx)
{
	struct gve_ring_com *com = &rx->com;
	int i;

	for (i = 0; i < com->priv->rx_desc_cnt; i++)
		rx->dqo.compl_ring[i] = (struct gve_rx_compl_desc_dqo){};

	bus_dmamap_sync(rx->dqo.compl_ring_mem.tag, rx->dqo.compl_ring_mem.map,
	    BUS_DMASYNC_PREWRITE);
}

void
gve_clear_rx_ring_dqo(struct gve_priv *priv, int i)
{
	struct gve_rx_ring *rx = &priv->rx[i];
	int j;

	rx->fill_cnt = 0;
	rx->cnt = 0;
	rx->dqo.mask = priv->rx_desc_cnt - 1;
	rx->dqo.head = 0;
	rx->dqo.tail = 0;
	rx->dqo.cur_gen_bit = 0;

	gve_rx_clear_desc_ring_dqo(rx);
	gve_rx_clear_compl_ring_dqo(rx);

	gve_free_rx_mbufs_dqo(rx);

	if (gve_is_qpl(priv)) {
		SLIST_INIT(&rx->dqo.free_bufs);
		STAILQ_INIT(&rx->dqo.used_bufs);

		for (j = 0; j < rx->dqo.buf_cnt; j++) {
			struct gve_rx_buf_dqo *buf = &rx->dqo.bufs[j];

			vm_page_t page = rx->com.qpl->pages[buf - rx->dqo.bufs];
			u_int ref_count = atomic_load_int(&page->ref_count);

			/*
			 * An ifconfig down+up might see pages still in flight
			 * from the previous innings.
			 */
			if (VPRC_WIRE_COUNT(ref_count) == 1)
				SLIST_INSERT_HEAD(&rx->dqo.free_bufs,
				    buf, slist_entry);
			else
				STAILQ_INSERT_TAIL(&rx->dqo.used_bufs,
				    buf, stailq_entry);

			buf->num_nic_frags = 0;
			buf->next_idx = 0;
		}
	} else {
		SLIST_INIT(&rx->dqo.free_bufs);
		for (j = 0; j < rx->dqo.buf_cnt; j++)
			SLIST_INSERT_HEAD(&rx->dqo.free_bufs,
			    &rx->dqo.bufs[j], slist_entry);
	}
}

int
gve_rx_intr_dqo(void *arg)
{
	struct gve_rx_ring *rx = arg;
	struct gve_priv *priv = rx->com.priv;
	struct gve_ring_com *com = &rx->com;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return (FILTER_STRAY);

	/* Interrupts are automatically masked */
	taskqueue_enqueue(com->cleanup_tq, &com->cleanup_task);
	return (FILTER_HANDLED);
}

static void
gve_rx_advance_head_dqo(struct gve_rx_ring *rx)
{
	rx->dqo.head = (rx->dqo.head + 1) & rx->dqo.mask;
	rx->fill_cnt++; /* rx->fill_cnt is just a sysctl counter */

	if ((rx->dqo.head & (GVE_RX_BUF_THRESH_DQO - 1)) == 0) {
		bus_dmamap_sync(rx->desc_ring_mem.tag, rx->desc_ring_mem.map,
		    BUS_DMASYNC_PREWRITE);
		gve_db_bar_dqo_write_4(rx->com.priv, rx->com.db_offset,
		    rx->dqo.head);
	}
}

static void
gve_rx_post_buf_dqo(struct gve_rx_ring *rx, struct gve_rx_buf_dqo *buf)
{
	struct gve_rx_desc_dqo *desc;

	bus_dmamap_sync(rx->dqo.buf_dmatag, buf->dmamap,
	    BUS_DMASYNC_PREREAD);

	desc = &rx->dqo.desc_ring[rx->dqo.head];
	desc->buf_id = htole16(buf - rx->dqo.bufs);
	desc->buf_addr = htole64(buf->addr);

	gve_rx_advance_head_dqo(rx);
}

static int
gve_rx_post_new_mbuf_dqo(struct gve_rx_ring *rx, int how)
{
	struct gve_rx_buf_dqo *buf;
	bus_dma_segment_t segs[1];
	int nsegs;
	int err;

	buf = SLIST_FIRST(&rx->dqo.free_bufs);
	if (__predict_false(!buf)) {
		device_printf(rx->com.priv->dev,
		    "Unexpected empty free bufs list\n");
		return (ENOBUFS);
	}
	SLIST_REMOVE_HEAD(&rx->dqo.free_bufs, slist_entry);

	buf->mbuf = m_getcl(how, MT_DATA, M_PKTHDR);
	if (__predict_false(!buf->mbuf)) {
		err = ENOMEM;
		counter_enter();
		counter_u64_add_protected(rx->stats.rx_mbuf_mclget_null, 1);
		counter_exit();
		goto abort_with_buf;
	}
	buf->mbuf->m_len = MCLBYTES;

	err = bus_dmamap_load_mbuf_sg(rx->dqo.buf_dmatag, buf->dmamap,
	    buf->mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
	KASSERT(nsegs == 1, ("dma segs for a cluster mbuf is not 1"));
	if (__predict_false(err != 0)) {
		counter_enter();
		counter_u64_add_protected(rx->stats.rx_mbuf_dmamap_err, 1);
		counter_exit();
		goto abort_with_mbuf;
	}
	buf->addr = segs[0].ds_addr;

	gve_rx_post_buf_dqo(rx, buf);
	return (0);

abort_with_mbuf:
	m_freem(buf->mbuf);
	buf->mbuf = NULL;
abort_with_buf:
	SLIST_INSERT_HEAD(&rx->dqo.free_bufs, buf, slist_entry);
	return (err);
}

static struct gve_dma_handle *
gve_get_page_dma_handle(struct gve_rx_ring *rx, struct gve_rx_buf_dqo *buf)
{
	return (&(rx->com.qpl->dmas[buf - rx->dqo.bufs]));
}

static void
gve_rx_post_qpl_buf_dqo(struct gve_rx_ring *rx, struct gve_rx_buf_dqo *buf,
    uint8_t frag_num)
{
	struct gve_rx_desc_dqo *desc = &rx->dqo.desc_ring[rx->dqo.head];
	union gve_rx_qpl_buf_id_dqo composed_id;
	struct gve_dma_handle *page_dma_handle;

	composed_id.buf_id = buf - rx->dqo.bufs;
	composed_id.frag_num = frag_num;
	desc->buf_id = htole16(composed_id.all);

	page_dma_handle = gve_get_page_dma_handle(rx, buf);
	bus_dmamap_sync(page_dma_handle->tag, page_dma_handle->map,
	    BUS_DMASYNC_PREREAD);
	desc->buf_addr = htole64(page_dma_handle->bus_addr +
	    frag_num * GVE_DEFAULT_RX_BUFFER_SIZE);

	buf->num_nic_frags++;
	gve_rx_advance_head_dqo(rx);
}

static void
gve_rx_maybe_extract_from_used_bufs(struct gve_rx_ring *rx, bool just_one)
{
	struct gve_rx_buf_dqo *hol_blocker = NULL;
	struct gve_rx_buf_dqo *buf;
	u_int ref_count;
	vm_page_t page;

	while (true) {
		buf = STAILQ_FIRST(&rx->dqo.used_bufs);
		if (__predict_false(buf == NULL))
			break;

		page = rx->com.qpl->pages[buf - rx->dqo.bufs];
		ref_count = atomic_load_int(&page->ref_count);

		if (VPRC_WIRE_COUNT(ref_count) != 1) {
			/* Account for one head-of-line blocker */
			if (hol_blocker != NULL)
				break;
			hol_blocker = buf;
			STAILQ_REMOVE_HEAD(&rx->dqo.used_bufs,
			    stailq_entry);
			continue;
		}

		STAILQ_REMOVE_HEAD(&rx->dqo.used_bufs,
		    stailq_entry);
		SLIST_INSERT_HEAD(&rx->dqo.free_bufs,
		    buf, slist_entry);
		if (just_one)
			break;
	}

	if (hol_blocker != NULL)
		STAILQ_INSERT_HEAD(&rx->dqo.used_bufs,
		    hol_blocker, stailq_entry);
}

static int
gve_rx_post_new_dqo_qpl_buf(struct gve_rx_ring *rx)
{
	struct gve_rx_buf_dqo *buf;

	buf = SLIST_FIRST(&rx->dqo.free_bufs);
	if (__predict_false(buf == NULL)) {
		gve_rx_maybe_extract_from_used_bufs(rx, /*just_one=*/true);
		buf = SLIST_FIRST(&rx->dqo.free_bufs);
		if (__predict_false(buf == NULL))
			return (ENOBUFS);
	}

	gve_rx_post_qpl_buf_dqo(rx, buf, buf->next_idx);
	if (buf->next_idx == GVE_DQ_NUM_FRAGS_IN_PAGE - 1)
		buf->next_idx = 0;
	else
		buf->next_idx++;

	/*
	 * We have posted all the frags in this buf to the NIC.
	 * - buf will enter used_bufs once the last completion arrives.
	 * - It will renter free_bufs in gve_rx_maybe_extract_from_used_bufs
	 *   when its wire count drops back to 1.
	 */
	if (buf->next_idx == 0)
		SLIST_REMOVE_HEAD(&rx->dqo.free_bufs, slist_entry);
	return (0);
}

static void
gve_rx_post_buffers_dqo(struct gve_rx_ring *rx, int how)
{
	uint32_t num_pending_bufs;
	uint32_t num_to_post;
	uint32_t i;
	int err;

	num_pending_bufs = (rx->dqo.head - rx->dqo.tail) & rx->dqo.mask;
	num_to_post = rx->dqo.mask - num_pending_bufs;

	for (i = 0; i < num_to_post; i++) {
		if (gve_is_qpl(rx->com.priv))
			err = gve_rx_post_new_dqo_qpl_buf(rx);
		else
			err = gve_rx_post_new_mbuf_dqo(rx, how);
		if (err)
			break;
	}
}

void
gve_rx_prefill_buffers_dqo(struct gve_rx_ring *rx)
{
	gve_rx_post_buffers_dqo(rx, M_WAITOK);
}

static void
gve_rx_set_hashtype_dqo(struct mbuf *mbuf, struct gve_ptype *ptype, bool *is_tcp)
{
	switch (ptype->l3_type) {
	case GVE_L3_TYPE_IPV4:
		switch (ptype->l4_type) {
		case GVE_L4_TYPE_TCP:
			*is_tcp = true;
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV4);
			break;
		case GVE_L4_TYPE_UDP:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV4);
			break;
		default:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV4);
		}
		break;
	case GVE_L3_TYPE_IPV6:
		switch (ptype->l4_type) {
		case GVE_L4_TYPE_TCP:
			*is_tcp = true;
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV6);
			break;
		case GVE_L4_TYPE_UDP:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV6);
			break;
		default:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV6);
		}
		break;
	default:
		M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
	}
}

static void
gve_rx_set_csum_flags_dqo(struct mbuf *mbuf,
    struct gve_rx_compl_desc_dqo *desc,
    struct gve_ptype *ptype)
{
	/* HW did not identify and process L3 and L4 headers. */
	if (__predict_false(!desc->l3_l4_processed))
		return;

	if (ptype->l3_type == GVE_L3_TYPE_IPV4) {
		if (__predict_false(desc->csum_ip_err ||
		    desc->csum_external_ip_err))
			return;
	} else if (ptype->l3_type == GVE_L3_TYPE_IPV6) {
		/* Checksum should be skipped if this flag is set. */
		if (__predict_false(desc->ipv6_ex_add))
			return;
	}

	if (__predict_false(desc->csum_l4_err))
		return;

	switch (ptype->l4_type) {
	case GVE_L4_TYPE_TCP:
	case GVE_L4_TYPE_UDP:
	case GVE_L4_TYPE_ICMP:
	case GVE_L4_TYPE_SCTP:
		mbuf->m_pkthdr.csum_flags = CSUM_IP_CHECKED |
					    CSUM_IP_VALID |
					    CSUM_DATA_VALID |
					    CSUM_PSEUDO_HDR;
		mbuf->m_pkthdr.csum_data = 0xffff;
		break;
	default:
		break;
	}
}

static void
gve_rx_input_mbuf_dqo(struct gve_rx_ring *rx,
    struct gve_rx_compl_desc_dqo *compl_desc)
{
	struct mbuf *mbuf = rx->ctx.mbuf_head;
	if_t ifp = rx->com.priv->ifp;
	struct gve_ptype *ptype;
	bool do_if_input = true;
	bool is_tcp = false;

	ptype = &rx->com.priv->ptype_lut_dqo->ptypes[compl_desc->packet_type];
	gve_rx_set_hashtype_dqo(mbuf, ptype, &is_tcp);
	mbuf->m_pkthdr.flowid = le32toh(compl_desc->hash);
	gve_rx_set_csum_flags_dqo(mbuf, compl_desc, ptype);

	mbuf->m_pkthdr.rcvif = ifp;
	mbuf->m_pkthdr.len = rx->ctx.total_size;

	if (((if_getcapenable(rx->com.priv->ifp) & IFCAP_LRO) != 0) &&
	    is_tcp &&
	    (rx->lro.lro_cnt != 0) &&
	    (tcp_lro_rx(&rx->lro, mbuf, 0) == 0))
		do_if_input = false;

	if (do_if_input)
		if_input(ifp, mbuf);

	counter_enter();
	counter_u64_add_protected(rx->stats.rbytes, rx->ctx.total_size);
	counter_u64_add_protected(rx->stats.rpackets, 1);
	counter_exit();

	rx->ctx = (struct gve_rx_ctx){};
}

static int
gve_rx_copybreak_dqo(struct gve_rx_ring *rx, void *va,
    struct gve_rx_compl_desc_dqo *compl_desc, uint16_t frag_len)
{
	struct mbuf *mbuf;

	mbuf = m_get2(frag_len, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (__predict_false(mbuf == NULL))
		return (ENOMEM);

	counter_enter();
	counter_u64_add_protected(rx->stats.rx_copybreak_cnt, 1);
	counter_exit();

	m_copyback(mbuf, 0, frag_len, va);
	mbuf->m_len = frag_len;

	rx->ctx.mbuf_head = mbuf;
	rx->ctx.mbuf_tail = mbuf;
	rx->ctx.total_size += frag_len;

	gve_rx_input_mbuf_dqo(rx, compl_desc);
	return (0);
}

static void
gve_rx_dqo(struct gve_priv *priv, struct gve_rx_ring *rx,
    struct gve_rx_compl_desc_dqo *compl_desc,
    int *work_done)
{
	bool is_last_frag = compl_desc->end_of_packet != 0;
	struct gve_rx_ctx *ctx = &rx->ctx;
	struct gve_rx_buf_dqo *buf;
	uint32_t num_pending_bufs;
	uint16_t frag_len;
	uint16_t buf_id;
	int err;

	buf_id = le16toh(compl_desc->buf_id);
	if (__predict_false(buf_id >= rx->dqo.buf_cnt)) {
		device_printf(priv->dev, "Invalid rx buf id %d on rxq %d, issuing reset\n",
		    buf_id, rx->com.id);
		gve_schedule_reset(priv);
		goto drop_frag_clear_ctx;
	}
	buf = &rx->dqo.bufs[buf_id];
	if (__predict_false(buf->mbuf == NULL)) {
		device_printf(priv->dev, "Spurious completion for buf id %d on rxq %d, issuing reset\n",
		    buf_id, rx->com.id);
		gve_schedule_reset(priv);
		goto drop_frag_clear_ctx;
	}

	if (__predict_false(ctx->drop_pkt))
		goto drop_frag;

	if (__predict_false(compl_desc->rx_error)) {
		counter_enter();
		counter_u64_add_protected(rx->stats.rx_dropped_pkt_desc_err, 1);
		counter_exit();
		goto drop_frag;
	}

	bus_dmamap_sync(rx->dqo.buf_dmatag, buf->dmamap,
	    BUS_DMASYNC_POSTREAD);

	frag_len = compl_desc->packet_len;
	if (frag_len <= priv->rx_copybreak && !ctx->mbuf_head && is_last_frag) {
		err = gve_rx_copybreak_dqo(rx, mtod(buf->mbuf, char*),
		    compl_desc, frag_len);
		if (__predict_false(err != 0))
			goto drop_frag;
		(*work_done)++;
		gve_rx_post_buf_dqo(rx, buf);
		return;
	}

	/*
	 * Although buffer completions may arrive out of order, buffer
	 * descriptors are consumed by the NIC in order. That is, the
	 * buffer at desc_ring[tail] might not be the buffer we got the
	 * completion compl_ring[tail] for: but we know that desc_ring[tail]
	 * has already been read by the NIC.
	 */
	num_pending_bufs = (rx->dqo.head - rx->dqo.tail) & rx->dqo.mask;

	/*
	 * For every fragment received, try to post a new buffer.
	 *
	 * Failures are okay but only so long as the number of outstanding
	 * buffers is above a threshold.
	 *
	 * Beyond that we drop new packets to reuse their buffers.
	 * Without ensuring a minimum number of buffers for the NIC to
	 * put packets in, we run the risk of getting the queue stuck
	 * for good.
	 */
	err = gve_rx_post_new_mbuf_dqo(rx, M_NOWAIT);
	if (__predict_false(err != 0 &&
	    num_pending_bufs <= GVE_RX_DQO_MIN_PENDING_BUFS)) {
		counter_enter();
		counter_u64_add_protected(
		    rx->stats.rx_dropped_pkt_mbuf_alloc_fail, 1);
		counter_exit();
		goto drop_frag;
	}

	buf->mbuf->m_len = frag_len;
	ctx->total_size += frag_len;
	if (ctx->mbuf_tail == NULL) {
		ctx->mbuf_head = buf->mbuf;
		ctx->mbuf_tail = buf->mbuf;
	} else {
		buf->mbuf->m_flags &= ~M_PKTHDR;
		ctx->mbuf_tail->m_next = buf->mbuf;
		ctx->mbuf_tail = buf->mbuf;
	}

	/*
	 * Disassociate the mbuf from buf and surrender buf to the free list to
	 * be used by a future mbuf.
	 */
	bus_dmamap_unload(rx->dqo.buf_dmatag, buf->dmamap);
	buf->mbuf = NULL;
	buf->addr = 0;
	SLIST_INSERT_HEAD(&rx->dqo.free_bufs, buf, slist_entry);

	if (is_last_frag) {
		gve_rx_input_mbuf_dqo(rx, compl_desc);
		(*work_done)++;
	}
	return;

drop_frag:
	/* Clear the earlier frags if there were any */
	m_freem(ctx->mbuf_head);
	rx->ctx = (struct gve_rx_ctx){};
	/* Drop the rest of the pkt if there are more frags */
	ctx->drop_pkt = true;
	/* Reuse the dropped frag's buffer */
	gve_rx_post_buf_dqo(rx, buf);

	if (is_last_frag)
		goto drop_frag_clear_ctx;
	return;

drop_frag_clear_ctx:
	counter_enter();
	counter_u64_add_protected(rx->stats.rx_dropped_pkt, 1);
	counter_exit();
	m_freem(ctx->mbuf_head);
	rx->ctx = (struct gve_rx_ctx){};
}

static void *
gve_get_cpu_addr_for_qpl_buf(struct gve_rx_ring *rx,
    struct gve_rx_buf_dqo *buf, uint8_t buf_frag_num)
{
	int page_idx = buf - rx->dqo.bufs;
	void *va = rx->com.qpl->dmas[page_idx].cpu_addr;

	va = (char *)va + (buf_frag_num * GVE_DEFAULT_RX_BUFFER_SIZE);
	return (va);
}

static int
gve_rx_add_clmbuf_to_ctx(struct gve_rx_ring *rx,
    struct gve_rx_ctx *ctx, struct gve_rx_buf_dqo *buf,
    uint8_t buf_frag_num, uint16_t frag_len)
{
	void *va = gve_get_cpu_addr_for_qpl_buf(rx, buf, buf_frag_num);
	struct mbuf *mbuf;

	if (ctx->mbuf_tail == NULL) {
		mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (mbuf == NULL)
			return (ENOMEM);
		ctx->mbuf_head = mbuf;
		ctx->mbuf_tail = mbuf;
	} else {
		mbuf = m_getcl(M_NOWAIT, MT_DATA, 0);
		if (mbuf == NULL)
			return (ENOMEM);
		ctx->mbuf_tail->m_next = mbuf;
		ctx->mbuf_tail = mbuf;
	}

	mbuf->m_len = frag_len;
	ctx->total_size += frag_len;

	m_copyback(mbuf, 0, frag_len, va);
	counter_enter();
	counter_u64_add_protected(rx->stats.rx_frag_copy_cnt, 1);
	counter_exit();
	return (0);
}

static int
gve_rx_add_extmbuf_to_ctx(struct gve_rx_ring *rx,
    struct gve_rx_ctx *ctx, struct gve_rx_buf_dqo *buf,
    uint8_t buf_frag_num, uint16_t frag_len)
{
	struct mbuf *mbuf;
	void *page_addr;
	vm_page_t page;
	int page_idx;
	void *va;

	if (ctx->mbuf_tail == NULL) {
		mbuf = m_gethdr(M_NOWAIT, MT_DATA);
		if (mbuf == NULL)
			return (ENOMEM);
		ctx->mbuf_head = mbuf;
		ctx->mbuf_tail = mbuf;
	} else {
		mbuf = m_get(M_NOWAIT, MT_DATA);
		if (mbuf == NULL)
			return (ENOMEM);
		ctx->mbuf_tail->m_next = mbuf;
		ctx->mbuf_tail = mbuf;
	}

	mbuf->m_len = frag_len;
	ctx->total_size += frag_len;

	page_idx = buf - rx->dqo.bufs;
	page = rx->com.qpl->pages[page_idx];
	page_addr = rx->com.qpl->dmas[page_idx].cpu_addr;
	va = (char *)page_addr + (buf_frag_num * GVE_DEFAULT_RX_BUFFER_SIZE);

	/*
	 * Grab an extra ref to the page so that gve_mextadd_free
	 * does not end up freeing the page while the interface exists.
	 */
	vm_page_wire(page);

	counter_enter();
	counter_u64_add_protected(rx->stats.rx_frag_flip_cnt, 1);
	counter_exit();

	MEXTADD(mbuf, va, frag_len,
	    gve_mextadd_free, page, page_addr,
	    0, EXT_NET_DRV);
	return (0);
}

static void
gve_rx_dqo_qpl(struct gve_priv *priv, struct gve_rx_ring *rx,
    struct gve_rx_compl_desc_dqo *compl_desc,
    int *work_done)
{
	bool is_last_frag = compl_desc->end_of_packet != 0;
	union gve_rx_qpl_buf_id_dqo composed_id;
	struct gve_dma_handle *page_dma_handle;
	struct gve_rx_ctx *ctx = &rx->ctx;
	struct gve_rx_buf_dqo *buf;
	uint32_t num_pending_bufs;
	uint8_t buf_frag_num;
	uint16_t frag_len;
	uint16_t buf_id;
	int err;

	composed_id.all = le16toh(compl_desc->buf_id);
	buf_id = composed_id.buf_id;
	buf_frag_num = composed_id.frag_num;

	if (__predict_false(buf_id >= rx->dqo.buf_cnt)) {
		device_printf(priv->dev, "Invalid rx buf id %d on rxq %d, issuing reset\n",
		    buf_id, rx->com.id);
		gve_schedule_reset(priv);
		goto drop_frag_clear_ctx;
	}
	buf = &rx->dqo.bufs[buf_id];
	if (__predict_false(buf->num_nic_frags == 0 ||
	    buf_frag_num > GVE_DQ_NUM_FRAGS_IN_PAGE - 1)) {
		device_printf(priv->dev, "Spurious compl for buf id %d on rxq %d "
		    "with buf_frag_num %d and num_nic_frags %d, issuing reset\n",
		    buf_id, rx->com.id, buf_frag_num, buf->num_nic_frags);
		gve_schedule_reset(priv);
		goto drop_frag_clear_ctx;
	}

	buf->num_nic_frags--;

	if (__predict_false(ctx->drop_pkt))
		goto drop_frag;

	if (__predict_false(compl_desc->rx_error)) {
		counter_enter();
		counter_u64_add_protected(rx->stats.rx_dropped_pkt_desc_err, 1);
		counter_exit();
		goto drop_frag;
	}

	page_dma_handle = gve_get_page_dma_handle(rx, buf);
	bus_dmamap_sync(page_dma_handle->tag, page_dma_handle->map,
	    BUS_DMASYNC_POSTREAD);

	frag_len = compl_desc->packet_len;
	if (frag_len <= priv->rx_copybreak && !ctx->mbuf_head && is_last_frag) {
		void *va = gve_get_cpu_addr_for_qpl_buf(rx, buf, buf_frag_num);

		err = gve_rx_copybreak_dqo(rx, va, compl_desc, frag_len);
		if (__predict_false(err != 0))
			goto drop_frag;
		(*work_done)++;
		gve_rx_post_qpl_buf_dqo(rx, buf, buf_frag_num);
		return;
	}

	num_pending_bufs = (rx->dqo.head - rx->dqo.tail) & rx->dqo.mask;
	err = gve_rx_post_new_dqo_qpl_buf(rx);
	if (__predict_false(err != 0 &&
	    num_pending_bufs <= GVE_RX_DQO_MIN_PENDING_BUFS)) {
		/*
		 * Resort to copying this fragment into a cluster mbuf
		 * when the above threshold is breached and repost the
		 * incoming buffer. If we cannot find cluster mbufs,
		 * just drop the packet (to repost its buffer).
		 */
		err = gve_rx_add_clmbuf_to_ctx(rx, ctx, buf,
		    buf_frag_num, frag_len);
		if (err != 0) {
			counter_enter();
			counter_u64_add_protected(
			    rx->stats.rx_dropped_pkt_buf_post_fail, 1);
			counter_exit();
			goto drop_frag;
		}
		gve_rx_post_qpl_buf_dqo(rx, buf, buf_frag_num);
	} else {
		err = gve_rx_add_extmbuf_to_ctx(rx, ctx, buf,
		    buf_frag_num, frag_len);
		if (__predict_false(err != 0)) {
			counter_enter();
			counter_u64_add_protected(
			    rx->stats.rx_dropped_pkt_mbuf_alloc_fail, 1);
			counter_exit();
			goto drop_frag;
		}
	}

	/*
	 * Both the counts need to be checked.
	 *
	 * num_nic_frags == 0 implies no pending completions
	 * but not all frags may have yet been posted.
	 *
	 * next_idx == 0 implies all frags have been posted
	 * but there might be pending completions.
	 */
	if (buf->num_nic_frags == 0 && buf->next_idx == 0)
		STAILQ_INSERT_TAIL(&rx->dqo.used_bufs, buf, stailq_entry);

	if (is_last_frag) {
		gve_rx_input_mbuf_dqo(rx, compl_desc);
		(*work_done)++;
	}
	return;

drop_frag:
	/* Clear the earlier frags if there were any */
	m_freem(ctx->mbuf_head);
	rx->ctx = (struct gve_rx_ctx){};
	/* Drop the rest of the pkt if there are more frags */
	ctx->drop_pkt = true;
	/* Reuse the dropped frag's buffer */
	gve_rx_post_qpl_buf_dqo(rx, buf, buf_frag_num);

	if (is_last_frag)
		goto drop_frag_clear_ctx;
	return;

drop_frag_clear_ctx:
	counter_enter();
	counter_u64_add_protected(rx->stats.rx_dropped_pkt, 1);
	counter_exit();
	m_freem(ctx->mbuf_head);
	rx->ctx = (struct gve_rx_ctx){};
}

static bool
gve_rx_cleanup_dqo(struct gve_priv *priv, struct gve_rx_ring *rx, int budget)
{
	struct gve_rx_compl_desc_dqo *compl_desc;
	uint32_t work_done = 0;

	NET_EPOCH_ASSERT();

	while (work_done < budget) {
		bus_dmamap_sync(rx->dqo.compl_ring_mem.tag, rx->dqo.compl_ring_mem.map,
		    BUS_DMASYNC_POSTREAD);

		compl_desc = &rx->dqo.compl_ring[rx->dqo.tail];
		if (compl_desc->generation == rx->dqo.cur_gen_bit)
			break;
		/*
		 * Prevent generation bit from being read after the rest of the
		 * descriptor.
		 */
		rmb();

		rx->cnt++;
		rx->dqo.tail = (rx->dqo.tail + 1) & rx->dqo.mask;
		rx->dqo.cur_gen_bit ^= (rx->dqo.tail == 0);

		if (gve_is_qpl(priv))
			gve_rx_dqo_qpl(priv, rx, compl_desc, &work_done);
		else
			gve_rx_dqo(priv, rx, compl_desc, &work_done);
	}

	if (work_done != 0)
		tcp_lro_flush_all(&rx->lro);

	gve_rx_post_buffers_dqo(rx, M_NOWAIT);
	if (gve_is_qpl(priv))
		gve_rx_maybe_extract_from_used_bufs(rx, /*just_one=*/false);
	return (work_done == budget);
}

void
gve_rx_cleanup_tq_dqo(void *arg, int pending)
{
	struct gve_rx_ring *rx = arg;
	struct gve_priv *priv = rx->com.priv;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return;

	if (gve_rx_cleanup_dqo(priv, rx, /*budget=*/64)) {
		taskqueue_enqueue(rx->com.cleanup_tq, &rx->com.cleanup_task);
		return;
	}

	gve_db_bar_dqo_write_4(priv, rx->com.irq_db_offset,
	    GVE_ITR_NO_UPDATE_DQO | GVE_ITR_ENABLE_BIT_DQO);
}
