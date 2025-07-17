/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2023-2024 Google LLC
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

#define GVE_GQ_TX_MIN_PKT_DESC_BYTES 182

static int
gve_tx_fifo_init(struct gve_priv *priv, struct gve_tx_ring *tx)
{
	struct gve_queue_page_list *qpl = tx->com.qpl;
	struct gve_tx_fifo *fifo = &tx->fifo;

	fifo->size = qpl->num_pages * PAGE_SIZE;
	fifo->base = qpl->kva;
	atomic_store_int(&fifo->available, fifo->size);
	fifo->head = 0;

	return (0);
}

static void
gve_tx_free_ring_gqi(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;

	if (tx->desc_ring != NULL) {
		gve_dma_free_coherent(&tx->desc_ring_mem);
		tx->desc_ring = NULL;
	}

	if (tx->info != NULL) {
		free(tx->info, M_GVE);
		tx->info = NULL;
	}

	if (com->qpl != NULL) {
		gve_free_qpl(priv, com->qpl);
		com->qpl = NULL;
	}
}

static void
gve_tx_free_ring(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;

	/* Safe to call even if never alloced */
	gve_free_counters((counter_u64_t *)&tx->stats, NUM_TX_STATS);

	if (mtx_initialized(&tx->ring_mtx))
		mtx_destroy(&tx->ring_mtx);

	if (com->q_resources != NULL) {
		gve_dma_free_coherent(&com->q_resources_mem);
		com->q_resources = NULL;
	}

	if (tx->br != NULL) {
		buf_ring_free(tx->br, M_DEVBUF);
		tx->br = NULL;
	}

	if (gve_is_gqi(priv))
		gve_tx_free_ring_gqi(priv, i);
	else
		gve_tx_free_ring_dqo(priv, i);
}

static int
gve_tx_alloc_ring_gqi(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;
	int err;

	err = gve_dma_alloc_coherent(priv,
	    sizeof(union gve_tx_desc) * priv->tx_desc_cnt,
	    CACHE_LINE_SIZE, &tx->desc_ring_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc desc ring for tx ring %d", i);
		goto abort;
	}
	tx->desc_ring = tx->desc_ring_mem.cpu_addr;

	com->qpl = gve_alloc_qpl(priv, i, priv->tx_desc_cnt / GVE_QPL_DIVISOR,
	    /*single_kva=*/true);
	if (com->qpl == NULL) {
		device_printf(priv->dev,
		    "Failed to alloc QPL for tx ring %d\n", i);
		err = ENOMEM;
		goto abort;
	}

	err = gve_tx_fifo_init(priv, tx);
	if (err != 0)
		goto abort;

	tx->info = malloc(
	    sizeof(struct gve_tx_buffer_state) * priv->tx_desc_cnt,
	    M_GVE, M_WAITOK | M_ZERO);
	return (0);

abort:
	gve_tx_free_ring_gqi(priv, i);
	return (err);
}

static int
gve_tx_alloc_ring(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;
	char mtx_name[16];
	int err;

	com->priv = priv;
	com->id = i;

	if (gve_is_gqi(priv))
		err = gve_tx_alloc_ring_gqi(priv, i);
	else
		err = gve_tx_alloc_ring_dqo(priv, i);
	if (err != 0)
		goto abort;

	sprintf(mtx_name, "gvetx%d", i);
	mtx_init(&tx->ring_mtx, mtx_name, NULL, MTX_DEF);

	tx->br = buf_ring_alloc(GVE_TX_BUFRING_ENTRIES, M_DEVBUF,
	    M_WAITOK, &tx->ring_mtx);

	gve_alloc_counters((counter_u64_t *)&tx->stats, NUM_TX_STATS);

	err = gve_dma_alloc_coherent(priv, sizeof(struct gve_queue_resources),
	    PAGE_SIZE, &com->q_resources_mem);
	if (err != 0) {
		device_printf(priv->dev,
		    "Failed to alloc queue resources for tx ring %d", i);
		goto abort;
	}
	com->q_resources = com->q_resources_mem.cpu_addr;

	tx->last_kicked = 0;

	return (0);

abort:
	gve_tx_free_ring(priv, i);
	return (err);
}

int
gve_alloc_tx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx)
{
	int i;
	int err;

	KASSERT(priv->tx != NULL, ("priv->tx is NULL!"));

	for (i = start_idx; i < stop_idx; i++) {
		err = gve_tx_alloc_ring(priv, i);
		if (err != 0)
			goto free_rings;
	}

	return (0);
free_rings:
	gve_free_tx_rings(priv, start_idx, i);
	return (err);
}

void
gve_free_tx_rings(struct gve_priv *priv, uint16_t start_idx, uint16_t stop_idx)
{
	int i;

	for (i = start_idx; i < stop_idx; i++)
		gve_tx_free_ring(priv, i);
}

static void
gve_tx_clear_desc_ring(struct gve_tx_ring *tx)
{
	struct gve_ring_com *com = &tx->com;
	int i;

	for (i = 0; i < com->priv->tx_desc_cnt; i++) {
		tx->desc_ring[i] = (union gve_tx_desc){};
		tx->info[i] = (struct gve_tx_buffer_state){};
		gve_invalidate_timestamp(&tx->info[i].enqueue_time_sec);
	}

	bus_dmamap_sync(tx->desc_ring_mem.tag, tx->desc_ring_mem.map,
	    BUS_DMASYNC_PREWRITE);
}

static void
gve_clear_tx_ring(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_tx_fifo *fifo = &tx->fifo;

	tx->req = 0;
	tx->done = 0;
	tx->mask = priv->tx_desc_cnt - 1;

	atomic_store_int(&fifo->available, fifo->size);
	fifo->head = 0;

	gve_tx_clear_desc_ring(tx);
}

static void
gve_start_tx_ring(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;

	atomic_store_bool(&tx->stopped, false);
	if (gve_is_gqi(priv))
		NET_TASK_INIT(&com->cleanup_task, 0, gve_tx_cleanup_tq, tx);
	else
		NET_TASK_INIT(&com->cleanup_task, 0, gve_tx_cleanup_tq_dqo, tx);
	com->cleanup_tq = taskqueue_create_fast("gve tx", M_WAITOK,
	    taskqueue_thread_enqueue, &com->cleanup_tq);
	taskqueue_start_threads(&com->cleanup_tq, 1, PI_NET, "%s txq %d",
	    device_get_nameunit(priv->dev), i);

	TASK_INIT(&tx->xmit_task, 0, gve_xmit_tq, tx);
	tx->xmit_tq = taskqueue_create_fast("gve tx xmit",
	    M_WAITOK, taskqueue_thread_enqueue, &tx->xmit_tq);
	taskqueue_start_threads(&tx->xmit_tq, 1, PI_NET, "%s txq %d xmit",
	    device_get_nameunit(priv->dev), i);
}

int
gve_create_tx_rings(struct gve_priv *priv)
{
	struct gve_ring_com *com;
	struct gve_tx_ring *tx;
	int err;
	int i;

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_TX_RINGS_OK))
		return (0);

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		if (gve_is_gqi(priv))
			gve_clear_tx_ring(priv, i);
		else
			gve_clear_tx_ring_dqo(priv, i);
	}

	err = gve_adminq_create_tx_queues(priv, priv->tx_cfg.num_queues);
	if (err != 0)
		return (err);

	bus_dmamap_sync(priv->irqs_db_mem.tag, priv->irqs_db_mem.map,
	    BUS_DMASYNC_POSTREAD);

	for (i = 0; i < priv->tx_cfg.num_queues; i++) {
		tx = &priv->tx[i];
		com = &tx->com;

		com->irq_db_offset = 4 * be32toh(priv->irq_db_indices[com->ntfy_id].index);

		bus_dmamap_sync(com->q_resources_mem.tag, com->q_resources_mem.map,
		    BUS_DMASYNC_POSTREAD);
		com->db_offset = 4 * be32toh(com->q_resources->db_index);
		com->counter_idx = be32toh(com->q_resources->counter_index);

		gve_start_tx_ring(priv, i);
	}

	gve_set_state_flag(priv, GVE_STATE_FLAG_TX_RINGS_OK);
	return (0);
}

static void
gve_stop_tx_ring(struct gve_priv *priv, int i)
{
	struct gve_tx_ring *tx = &priv->tx[i];
	struct gve_ring_com *com = &tx->com;

	if (com->cleanup_tq != NULL) {
		taskqueue_quiesce(com->cleanup_tq);
		taskqueue_free(com->cleanup_tq);
		com->cleanup_tq = NULL;
	}

	if (tx->xmit_tq != NULL) {
		taskqueue_quiesce(tx->xmit_tq);
		taskqueue_free(tx->xmit_tq);
		tx->xmit_tq = NULL;
	}
}

int
gve_destroy_tx_rings(struct gve_priv *priv)
{
	int err;
	int i;

	for (i = 0; i < priv->tx_cfg.num_queues; i++)
		gve_stop_tx_ring(priv, i);

	if (gve_get_state_flag(priv, GVE_STATE_FLAG_TX_RINGS_OK)) {
		err = gve_adminq_destroy_tx_queues(priv, priv->tx_cfg.num_queues);
		if (err != 0)
			return (err);
		gve_clear_state_flag(priv, GVE_STATE_FLAG_TX_RINGS_OK);
	}

	return (0);
}

int
gve_check_tx_timeout_gqi(struct gve_priv *priv, struct gve_tx_ring *tx)
{
	struct gve_tx_buffer_state *info;
	uint32_t pkt_idx;
	int num_timeouts;

	num_timeouts = 0;

	for (pkt_idx = 0; pkt_idx < priv->tx_desc_cnt; pkt_idx++) {
		info = &tx->info[pkt_idx];

		if (!gve_timestamp_valid(&info->enqueue_time_sec))
			continue;

		if (__predict_false(
		    gve_seconds_since(&info->enqueue_time_sec) >
		    GVE_TX_TIMEOUT_PKT_SEC))
			num_timeouts += 1;
	}

	return (num_timeouts);
}

int
gve_tx_intr(void *arg)
{
	struct gve_tx_ring *tx = arg;
	struct gve_priv *priv = tx->com.priv;
	struct gve_ring_com *com = &tx->com;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return (FILTER_STRAY);

	gve_db_bar_write_4(priv, com->irq_db_offset, GVE_IRQ_MASK);
	taskqueue_enqueue(com->cleanup_tq, &com->cleanup_task);
	return (FILTER_HANDLED);
}

static uint32_t
gve_tx_load_event_counter(struct gve_priv *priv, struct gve_tx_ring *tx)
{
	bus_dmamap_sync(priv->counter_array_mem.tag, priv->counter_array_mem.map,
	    BUS_DMASYNC_POSTREAD);
	uint32_t counter = priv->counters[tx->com.counter_idx];
	return (be32toh(counter));
}

static void
gve_tx_free_fifo(struct gve_tx_fifo *fifo, size_t bytes)
{
	atomic_add_int(&fifo->available, bytes);
}

void
gve_tx_cleanup_tq(void *arg, int pending)
{
	struct gve_tx_ring *tx = arg;
	struct gve_priv *priv = tx->com.priv;
	uint32_t nic_done = gve_tx_load_event_counter(priv, tx);
	uint32_t todo = nic_done - tx->done;
	size_t space_freed = 0;
	int i, j;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return;

	for (j = 0; j < todo; j++) {
		uint32_t idx = tx->done & tx->mask;
		struct gve_tx_buffer_state *info = &tx->info[idx];
		struct mbuf *mbuf = info->mbuf;

		tx->done++;
		if (mbuf == NULL)
			continue;

		gve_invalidate_timestamp(&info->enqueue_time_sec);

		info->mbuf = NULL;

		counter_enter();
		counter_u64_add_protected(tx->stats.tbytes, mbuf->m_pkthdr.len);
		counter_u64_add_protected(tx->stats.tpackets, 1);
		counter_exit();
		m_freem(mbuf);

		for (i = 0; i < GVE_TX_MAX_DESCS; i++) {
			space_freed += info->iov[i].iov_len + info->iov[i].iov_padding;
			info->iov[i].iov_len = 0;
			info->iov[i].iov_padding = 0;
		}
	}

	gve_tx_free_fifo(&tx->fifo, space_freed);

	gve_db_bar_write_4(priv, tx->com.irq_db_offset,
	    GVE_IRQ_ACK | GVE_IRQ_EVENT);

	/*
	 * Completions born before this barrier MAY NOT cause the NIC to send an
	 * interrupt but they will still be handled by the enqueue below.
	 * Completions born after the barrier WILL trigger an interrupt.
	 */
	atomic_thread_fence_seq_cst();

	nic_done = gve_tx_load_event_counter(priv, tx);
	todo = nic_done - tx->done;
	if (todo != 0) {
		gve_db_bar_write_4(priv, tx->com.irq_db_offset, GVE_IRQ_MASK);
		taskqueue_enqueue(tx->com.cleanup_tq, &tx->com.cleanup_task);
	}

	if (atomic_load_bool(&tx->stopped) && space_freed) {
		atomic_store_bool(&tx->stopped, false);
		taskqueue_enqueue(tx->xmit_tq, &tx->xmit_task);
	}
}

static void
gve_dma_sync_for_device(struct gve_queue_page_list *qpl,
			uint64_t iov_offset, uint64_t iov_len)
{
	uint64_t last_page = (iov_offset + iov_len - 1) / PAGE_SIZE;
	uint64_t first_page = iov_offset / PAGE_SIZE;
	struct gve_dma_handle *dma;
	uint64_t page;

	for (page = first_page; page <= last_page; page++) {
		dma = &(qpl->dmas[page]);
		bus_dmamap_sync(dma->tag, dma->map, BUS_DMASYNC_PREWRITE);
	}
}

static void
gve_tx_fill_mtd_desc(struct gve_tx_mtd_desc *mtd_desc, struct mbuf *mbuf)
{
	mtd_desc->type_flags = GVE_TXD_MTD | GVE_MTD_SUBTYPE_PATH;
	mtd_desc->path_state = GVE_MTD_PATH_STATE_DEFAULT | GVE_MTD_PATH_HASH_L4;
	mtd_desc->path_hash = htobe32(mbuf->m_pkthdr.flowid);
	mtd_desc->reserved0 = 0;
	mtd_desc->reserved1 = 0;
}

static void
gve_tx_fill_pkt_desc(struct gve_tx_pkt_desc *pkt_desc, bool is_tso,
    uint16_t l4_hdr_offset, uint32_t desc_cnt,
    uint16_t first_seg_len, uint64_t addr, bool has_csum_flag,
    int csum_offset, uint16_t pkt_len)
{
	if (is_tso) {
		pkt_desc->type_flags = GVE_TXD_TSO | GVE_TXF_L4CSUM;
		pkt_desc->l4_csum_offset = csum_offset >> 1;
		pkt_desc->l4_hdr_offset = l4_hdr_offset >> 1;
	} else if (has_csum_flag) {
		pkt_desc->type_flags = GVE_TXD_STD | GVE_TXF_L4CSUM;
		pkt_desc->l4_csum_offset = csum_offset >> 1;
		pkt_desc->l4_hdr_offset = l4_hdr_offset >> 1;
	} else {
		pkt_desc->type_flags = GVE_TXD_STD;
		pkt_desc->l4_csum_offset = 0;
		pkt_desc->l4_hdr_offset = 0;
	}
	pkt_desc->desc_cnt = desc_cnt;
	pkt_desc->len = htobe16(pkt_len);
	pkt_desc->seg_len = htobe16(first_seg_len);
	pkt_desc->seg_addr = htobe64(addr);
}

static void
gve_tx_fill_seg_desc(struct gve_tx_seg_desc *seg_desc,
    bool is_tso, uint16_t len, uint64_t addr,
    bool is_ipv6, uint8_t l3_off, uint16_t tso_mss)
{
	seg_desc->type_flags = GVE_TXD_SEG;
	if (is_tso) {
		if (is_ipv6)
			seg_desc->type_flags |= GVE_TXSF_IPV6;
		seg_desc->l3_offset = l3_off >> 1;
		seg_desc->mss = htobe16(tso_mss);
	}
	seg_desc->seg_len = htobe16(len);
	seg_desc->seg_addr = htobe64(addr);
}

static inline uint32_t
gve_tx_avail(struct gve_tx_ring *tx)
{
	return (tx->mask + 1 - (tx->req - tx->done));
}

static bool
gve_tx_fifo_can_alloc(struct gve_tx_fifo *fifo, size_t bytes)
{
	return (atomic_load_int(&fifo->available) >= bytes);
}

static inline bool
gve_can_tx(struct gve_tx_ring *tx, int bytes_required)
{
	return (gve_tx_avail(tx) >= (GVE_TX_MAX_DESCS + 1) &&
	    gve_tx_fifo_can_alloc(&tx->fifo, bytes_required));
}

static int
gve_tx_fifo_pad_alloc_one_frag(struct gve_tx_fifo *fifo, size_t bytes)
{
	return (fifo->head + bytes < fifo->size) ? 0 : fifo->size - fifo->head;
}

static inline int
gve_fifo_bytes_required(struct gve_tx_ring *tx, uint16_t first_seg_len,
    uint16_t pkt_len)
{
	int pad_bytes, align_hdr_pad;
	int bytes;

	pad_bytes = gve_tx_fifo_pad_alloc_one_frag(&tx->fifo, first_seg_len);
	/* We need to take into account the header alignment padding. */
	align_hdr_pad = roundup2(first_seg_len, CACHE_LINE_SIZE) - first_seg_len;
	bytes = align_hdr_pad + pad_bytes + pkt_len;

	return (bytes);
}

static int
gve_tx_alloc_fifo(struct gve_tx_fifo *fifo, size_t bytes,
    struct gve_tx_iovec iov[2])
{
	size_t overflow, padding;
	uint32_t aligned_head;
	int nfrags = 0;

	if (bytes == 0)
		return (0);

	/*
	 * This check happens before we know how much padding is needed to
	 * align to a cacheline boundary for the payload, but that is fine,
	 * because the FIFO head always start aligned, and the FIFO's boundaries
	 * are aligned, so if there is space for the data, there is space for
	 * the padding to the next alignment.
	 */
	KASSERT(gve_tx_fifo_can_alloc(fifo, bytes),
	    ("Allocating gve tx fifo when there is no room"));

	nfrags++;

	iov[0].iov_offset = fifo->head;
	iov[0].iov_len = bytes;
	fifo->head += bytes;

	if (fifo->head > fifo->size) {
		/*
		 * If the allocation did not fit in the tail fragment of the
		 * FIFO, also use the head fragment.
		 */
		nfrags++;
		overflow = fifo->head - fifo->size;
		iov[0].iov_len -= overflow;
		iov[1].iov_offset = 0;	/* Start of fifo*/
		iov[1].iov_len = overflow;

		fifo->head = overflow;
	}

	/* Re-align to a cacheline boundary */
	aligned_head = roundup2(fifo->head, CACHE_LINE_SIZE);
	padding = aligned_head - fifo->head;
	iov[nfrags - 1].iov_padding = padding;
	atomic_add_int(&fifo->available, -(bytes + padding));
	fifo->head = aligned_head;

	if (fifo->head == fifo->size)
		fifo->head = 0;

	return (nfrags);
}

/* Only error this returns is ENOBUFS when the tx fifo is short of space */
static int
gve_xmit(struct gve_tx_ring *tx, struct mbuf *mbuf)
{
	bool is_tso, has_csum_flag, is_ipv6 = false, is_tcp = false, is_udp = false;
	int csum_flags, csum_offset, mtd_desc_nr, offset, copy_offset;
	uint16_t tso_mss, l4_off, l4_data_off, pkt_len, first_seg_len;
	int pad_bytes, hdr_nfrags, payload_nfrags;
	struct gve_tx_pkt_desc *pkt_desc;
	struct gve_tx_seg_desc *seg_desc;
	struct gve_tx_mtd_desc *mtd_desc;
	struct gve_tx_buffer_state *info;
	uint32_t idx = tx->req & tx->mask;
	struct ether_header *eh;
	struct mbuf *mbuf_next;
	int payload_iov = 2;
	int bytes_required;
	struct ip6_hdr *ip6;
	struct tcphdr *th;
	uint32_t next_idx;
	uint8_t l3_off;
	struct ip *ip;
	int i;

	info = &tx->info[idx];
	csum_flags = mbuf->m_pkthdr.csum_flags;
	pkt_len = mbuf->m_pkthdr.len;
	is_tso = csum_flags & CSUM_TSO;
	has_csum_flag = csum_flags & (CSUM_TCP | CSUM_UDP |
	    CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_TSO);
	mtd_desc_nr = M_HASHTYPE_GET(mbuf) != M_HASHTYPE_NONE ? 1 : 0;
	tso_mss = is_tso ? mbuf->m_pkthdr.tso_segsz : 0;

	eh = mtod(mbuf, struct ether_header *);
	KASSERT(eh->ether_type != ETHERTYPE_VLAN,
	    ("VLAN-tagged packets not supported"));

	is_ipv6 = ntohs(eh->ether_type) == ETHERTYPE_IPV6;
	l3_off = ETHER_HDR_LEN;
	mbuf_next = m_getptr(mbuf, l3_off, &offset);

	if (is_ipv6) {
		ip6 = (struct ip6_hdr *)(mtodo(mbuf_next, offset));
		l4_off = l3_off + sizeof(struct ip6_hdr);
		is_tcp = (ip6->ip6_nxt == IPPROTO_TCP);
		is_udp = (ip6->ip6_nxt == IPPROTO_UDP);
		mbuf_next = m_getptr(mbuf, l4_off, &offset);
	} else if (ntohs(eh->ether_type) == ETHERTYPE_IP) {
		ip = (struct ip *)(mtodo(mbuf_next, offset));
		l4_off = l3_off + (ip->ip_hl << 2);
		is_tcp = (ip->ip_p == IPPROTO_TCP);
		is_udp = (ip->ip_p == IPPROTO_UDP);
		mbuf_next = m_getptr(mbuf, l4_off, &offset);
	}

	l4_data_off = 0;
	if (is_tcp) {
		th = (struct tcphdr *)(mtodo(mbuf_next, offset));
		l4_data_off = l4_off + (th->th_off << 2);
	} else if (is_udp)
		l4_data_off = l4_off + sizeof(struct udphdr);

	if (has_csum_flag) {
		if ((csum_flags & (CSUM_TSO | CSUM_TCP | CSUM_IP6_TCP)) != 0)
			csum_offset = offsetof(struct tcphdr, th_sum);
		else
			csum_offset = offsetof(struct udphdr, uh_sum);
	}

	/*
	 * If this packet is neither a TCP nor a UDP packet, the first segment,
	 * the one represented by the packet descriptor, will carry the
	 * spec-stipulated minimum of 182B.
	 */
	if (l4_data_off != 0)
		first_seg_len = l4_data_off;
	else
		first_seg_len = MIN(pkt_len, GVE_GQ_TX_MIN_PKT_DESC_BYTES);

	bytes_required = gve_fifo_bytes_required(tx, first_seg_len, pkt_len);
	if (__predict_false(!gve_can_tx(tx, bytes_required))) {
		counter_enter();
		counter_u64_add_protected(tx->stats.tx_delayed_pkt_nospace_device, 1);
		counter_exit();
		return (ENOBUFS);
	}

	/* So that the cleanup taskqueue can free the mbuf eventually. */
	info->mbuf = mbuf;

	gve_set_timestamp(&info->enqueue_time_sec);

	/*
	 * We don't want to split the header, so if necessary, pad to the end
	 * of the fifo and then put the header at the beginning of the fifo.
	 */
	pad_bytes = gve_tx_fifo_pad_alloc_one_frag(&tx->fifo, first_seg_len);
	hdr_nfrags = gve_tx_alloc_fifo(&tx->fifo, first_seg_len + pad_bytes,
	    &info->iov[0]);
	KASSERT(hdr_nfrags > 0, ("Number of header fragments for gve tx is 0"));
	payload_nfrags = gve_tx_alloc_fifo(&tx->fifo, pkt_len - first_seg_len,
	    &info->iov[payload_iov]);

	pkt_desc = &tx->desc_ring[idx].pkt;
	gve_tx_fill_pkt_desc(pkt_desc, is_tso, l4_off,
	    1 + mtd_desc_nr + payload_nfrags, first_seg_len,
	    info->iov[hdr_nfrags - 1].iov_offset, has_csum_flag, csum_offset,
	    pkt_len);

	m_copydata(mbuf, 0, first_seg_len,
	    (char *)tx->fifo.base + info->iov[hdr_nfrags - 1].iov_offset);
	gve_dma_sync_for_device(tx->com.qpl,
	    info->iov[hdr_nfrags - 1].iov_offset,
	    info->iov[hdr_nfrags - 1].iov_len);
	copy_offset = first_seg_len;

	if (mtd_desc_nr == 1) {
		next_idx = (tx->req + 1) & tx->mask;
		mtd_desc = &tx->desc_ring[next_idx].mtd;
		gve_tx_fill_mtd_desc(mtd_desc, mbuf);
	}

	for (i = payload_iov; i < payload_nfrags + payload_iov; i++) {
		next_idx = (tx->req + 1 + mtd_desc_nr + i - payload_iov) & tx->mask;
		seg_desc = &tx->desc_ring[next_idx].seg;

		gve_tx_fill_seg_desc(seg_desc, is_tso, info->iov[i].iov_len,
		    info->iov[i].iov_offset, is_ipv6, l3_off, tso_mss);

		m_copydata(mbuf, copy_offset, info->iov[i].iov_len,
		    (char *)tx->fifo.base + info->iov[i].iov_offset);
		gve_dma_sync_for_device(tx->com.qpl,
		    info->iov[i].iov_offset, info->iov[i].iov_len);
		copy_offset += info->iov[i].iov_len;
	}

	tx->req += (1 + mtd_desc_nr + payload_nfrags);
	if (is_tso) {
		counter_enter();
		counter_u64_add_protected(tx->stats.tso_packet_cnt, 1);
		counter_exit();
	}
	return (0);
}

static int
gve_xmit_mbuf(struct gve_tx_ring *tx,
    struct mbuf **mbuf)
{
	if (gve_is_gqi(tx->com.priv))
		return (gve_xmit(tx, *mbuf));

	if (gve_is_qpl(tx->com.priv))
		return (gve_xmit_dqo_qpl(tx, *mbuf));

	/*
	 * gve_xmit_dqo might attempt to defrag the mbuf chain.
	 * The reference is passed in so that in the case of
	 * errors, the new mbuf chain is what's put back on the br.
	 */
	return (gve_xmit_dqo(tx, mbuf));
}

/*
 * Has the side-effect of stopping the xmit queue by setting tx->stopped
 */
static int
gve_xmit_retry_enobuf_mbuf(struct gve_tx_ring *tx,
    struct mbuf **mbuf)
{
	int err;

	atomic_store_bool(&tx->stopped, true);

	/*
	 * Room made in the queue BEFORE the barrier will be seen by the
	 * gve_xmit_mbuf retry below.
	 *
	 * If room is made in the queue AFTER the barrier, the cleanup tq
	 * iteration creating the room will either see a tx->stopped value
	 * of 0 or the 1 we just wrote:
	 *
	 *   If it sees a 1, then it would enqueue the xmit tq. Enqueue
	 *   implies a retry on the waiting pkt.
	 *
	 *   If it sees a 0, then that implies a previous iteration overwrote
	 *   our 1, and that iteration would enqueue the xmit tq. Enqueue
	 *   implies a retry on the waiting pkt.
	 */
	atomic_thread_fence_seq_cst();

	err = gve_xmit_mbuf(tx, mbuf);
	if (err == 0)
		atomic_store_bool(&tx->stopped, false);

	return (err);
}

static void
gve_xmit_br(struct gve_tx_ring *tx)
{
	struct gve_priv *priv = tx->com.priv;
	struct ifnet *ifp = priv->ifp;
	struct mbuf *mbuf;
	int err;

	while ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0 &&
	    (mbuf = drbr_peek(ifp, tx->br)) != NULL) {
		err = gve_xmit_mbuf(tx, &mbuf);

		/*
		 * We need to stop this taskqueue when we can't xmit the pkt due
		 * to lack of space in the NIC ring (ENOBUFS). The retry exists
		 * to guard against a TOCTTOU bug that could end up freezing the
		 * queue forever.
		 */
		if (__predict_false(mbuf != NULL && err == ENOBUFS))
			err = gve_xmit_retry_enobuf_mbuf(tx, &mbuf);

		if (__predict_false(err != 0 && mbuf != NULL)) {
			if (err == EINVAL) {
				drbr_advance(ifp, tx->br);
				m_freem(mbuf);
			} else
				drbr_putback(ifp, tx->br, mbuf);
			break;
		}

		drbr_advance(ifp, tx->br);
		BPF_MTAP(ifp, mbuf);

		bus_dmamap_sync(tx->desc_ring_mem.tag, tx->desc_ring_mem.map,
		    BUS_DMASYNC_PREWRITE);

		if (gve_is_gqi(priv))
			gve_db_bar_write_4(priv, tx->com.db_offset, tx->req);
		else
			gve_db_bar_dqo_write_4(priv, tx->com.db_offset,
			    tx->dqo.desc_tail);
	}
}

void
gve_xmit_tq(void *arg, int pending)
{
	struct gve_tx_ring *tx = (struct gve_tx_ring *)arg;

	GVE_RING_LOCK(tx);
	gve_xmit_br(tx);
	GVE_RING_UNLOCK(tx);
}

static bool
is_vlan_tagged_pkt(struct mbuf *mbuf)
{
	struct ether_header *eh;

	eh = mtod(mbuf, struct ether_header *);
	return (ntohs(eh->ether_type) == ETHERTYPE_VLAN);
}

int
gve_xmit_ifp(if_t ifp, struct mbuf *mbuf)
{
	struct gve_priv *priv = if_getsoftc(ifp);
	struct gve_tx_ring *tx;
	bool is_br_empty;
	int err;
	uint32_t i;

	if (__predict_false((if_getdrvflags(priv->ifp) & IFF_DRV_RUNNING) == 0))
		return (ENODEV);

	if (M_HASHTYPE_GET(mbuf) != M_HASHTYPE_NONE)
		i = mbuf->m_pkthdr.flowid % priv->tx_cfg.num_queues;
	else
		i = curcpu % priv->tx_cfg.num_queues;
	tx = &priv->tx[i];

	if (__predict_false(is_vlan_tagged_pkt(mbuf))) {
		counter_enter();
		counter_u64_add_protected(tx->stats.tx_dropped_pkt_vlan, 1);
		counter_u64_add_protected(tx->stats.tx_dropped_pkt, 1);
		counter_exit();
		m_freem(mbuf);
		return (ENODEV);
	}

	is_br_empty = drbr_empty(ifp, tx->br);
	err = drbr_enqueue(ifp, tx->br, mbuf);
	if (__predict_false(err != 0)) {
		if (!atomic_load_bool(&tx->stopped))
			taskqueue_enqueue(tx->xmit_tq, &tx->xmit_task);
		counter_enter();
		counter_u64_add_protected(tx->stats.tx_dropped_pkt_nospace_bufring, 1);
		counter_u64_add_protected(tx->stats.tx_dropped_pkt, 1);
		counter_exit();
		return (err);
	}

	/*
	 * If the mbuf we just enqueued is the only one on the ring, then
	 * transmit it right away in the interests of low latency.
	 */
	if (is_br_empty && (GVE_RING_TRYLOCK(tx) != 0)) {
		gve_xmit_br(tx);
		GVE_RING_UNLOCK(tx);
	} else if (!atomic_load_bool(&tx->stopped))
		taskqueue_enqueue(tx->xmit_tq, &tx->xmit_task);

	return (0);
}

void
gve_qflush(if_t ifp)
{
	struct gve_priv *priv = if_getsoftc(ifp);
	struct gve_tx_ring *tx;
	int i;

	for (i = 0; i < priv->tx_cfg.num_queues; ++i) {
		tx = &priv->tx[i];
		if (drbr_empty(ifp, tx->br) == 0) {
			GVE_RING_LOCK(tx);
			drbr_flush(ifp, tx->br);
			GVE_RING_UNLOCK(tx);
		}
	}

	if_qflush(ifp);
}
