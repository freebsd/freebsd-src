/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2015-2020 Amazon.com, Inc. or its affiliates.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifdef DEV_NETMAP

#include "ena.h"
#include "ena_netmap.h"

#define ENA_NETMAP_MORE_FRAMES		1
#define ENA_NETMAP_NO_MORE_FRAMES	0
#define ENA_MAX_FRAMES			16384

struct ena_netmap_ctx {
	struct netmap_kring *kring;
	struct ena_adapter *adapter;
	struct netmap_adapter *na;
	struct netmap_slot *slots;
	struct ena_ring *ring;
	struct ena_com_io_cq *io_cq;
	struct ena_com_io_sq *io_sq;
	u_int nm_i;
	uint16_t nt;
	uint16_t lim;
};

/* Netmap callbacks */
static int ena_netmap_reg(struct netmap_adapter *, int);
static int ena_netmap_txsync(struct netmap_kring *, int);
static int ena_netmap_rxsync(struct netmap_kring *, int);

/* Helper functions */
static int ena_netmap_tx_frames(struct ena_netmap_ctx *);
static int ena_netmap_tx_frame(struct ena_netmap_ctx *);
static inline uint16_t ena_netmap_count_slots(struct ena_netmap_ctx *);
static inline uint16_t ena_netmap_packet_len(struct netmap_slot *, u_int,
    uint16_t);
static int ena_netmap_copy_data(struct netmap_adapter *, struct netmap_slot *,
    u_int, uint16_t, uint16_t, void *);
static int ena_netmap_map_single_slot(struct netmap_adapter *,
    struct netmap_slot *, bus_dma_tag_t, bus_dmamap_t, void **, uint64_t *);
static int ena_netmap_tx_map_slots(struct ena_netmap_ctx *,
    struct ena_tx_buffer *, void **, uint16_t *, uint16_t *);
static void ena_netmap_unmap_last_socket_chain(struct ena_netmap_ctx *,
    struct ena_tx_buffer *);
static void ena_netmap_tx_cleanup(struct ena_netmap_ctx *);
static uint16_t ena_netmap_tx_clean_one(struct ena_netmap_ctx *, uint16_t);
static inline int validate_tx_req_id(struct ena_ring *, uint16_t);
static int ena_netmap_rx_frames(struct ena_netmap_ctx *);
static int ena_netmap_rx_frame(struct ena_netmap_ctx *);
static int ena_netmap_rx_load_desc(struct ena_netmap_ctx *, uint16_t, int *);
static void ena_netmap_rx_cleanup(struct ena_netmap_ctx *);
static void ena_netmap_fill_ctx(struct netmap_kring *, struct ena_netmap_ctx *,
    uint16_t);

int
ena_netmap_attach(struct ena_adapter *adapter)
{
	struct netmap_adapter na;

	ena_log_nm(adapter->pdev, INFO, "netmap attach\n");

	bzero(&na, sizeof(na));
	na.na_flags = NAF_MOREFRAG;
	na.ifp = adapter->ifp;
	na.num_tx_desc = adapter->requested_tx_ring_size;
	na.num_rx_desc = adapter->requested_rx_ring_size;
	na.num_tx_rings = adapter->num_io_queues;
	na.num_rx_rings = adapter->num_io_queues;
	na.rx_buf_maxsize = adapter->buf_ring_size;
	na.nm_txsync = ena_netmap_txsync;
	na.nm_rxsync = ena_netmap_rxsync;
	na.nm_register = ena_netmap_reg;

	return (netmap_attach(&na));
}

int
ena_netmap_alloc_rx_slot(struct ena_adapter *adapter, struct ena_ring *rx_ring,
    struct ena_rx_buffer *rx_info)
{
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_kring *kring;
	struct netmap_ring *ring;
	struct netmap_slot *slot;
	void *addr;
	uint64_t paddr;
	int nm_i, qid, head, lim, rc;

	/* if previously allocated frag is not used */
	if (unlikely(rx_info->netmap_buf_idx != 0))
		return (0);

	qid = rx_ring->qid;
	kring = na->rx_rings[qid];
	nm_i = kring->nr_hwcur;
	head = kring->rhead;

	ena_log_nm(adapter->pdev, DBG,
	    "nr_hwcur: %d, nr_hwtail: %d, rhead: %d, rcur: %d, rtail: %d\n",
	    kring->nr_hwcur, kring->nr_hwtail, kring->rhead, kring->rcur,
	    kring->rtail);

	if ((nm_i == head) && rx_ring->initialized) {
		ena_log_nm(adapter->pdev, ERR,
		    "No free slots in netmap ring\n");
		return (ENOMEM);
	}

	ring = kring->ring;
	if (ring == NULL) {
		ena_log_nm(adapter->pdev, ERR, "Rx ring %d is NULL\n", qid);
		return (EFAULT);
	}
	slot = &ring->slot[nm_i];

	addr = PNMB(na, slot, &paddr);
	if (addr == NETMAP_BUF_BASE(na)) {
		ena_log_nm(adapter->pdev, ERR, "Bad buff in slot\n");
		return (EFAULT);
	}

	rc = netmap_load_map(na, adapter->rx_buf_tag, rx_info->map, addr);
	if (rc != 0) {
		ena_log_nm(adapter->pdev, WARN, "DMA mapping error\n");
		return (rc);
	}
	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map, BUS_DMASYNC_PREREAD);

	rx_info->ena_buf.paddr = paddr;
	rx_info->ena_buf.len = ring->nr_buf_size;
	rx_info->mbuf = NULL;
	rx_info->netmap_buf_idx = slot->buf_idx;

	slot->buf_idx = 0;

	lim = kring->nkr_num_slots - 1;
	kring->nr_hwcur = nm_next(nm_i, lim);

	return (0);
}

void
ena_netmap_free_rx_slot(struct ena_adapter *adapter, struct ena_ring *rx_ring,
    struct ena_rx_buffer *rx_info)
{
	struct netmap_adapter *na;
	struct netmap_kring *kring;
	struct netmap_slot *slot;
	int nm_i, qid, lim;

	na = NA(adapter->ifp);
	if (na == NULL) {
		ena_log_nm(adapter->pdev, ERR, "netmap adapter is NULL\n");
		return;
	}

	if (na->rx_rings == NULL) {
		ena_log_nm(adapter->pdev, ERR, "netmap rings are NULL\n");
		return;
	}

	qid = rx_ring->qid;
	kring = na->rx_rings[qid];
	if (kring == NULL) {
		ena_log_nm(adapter->pdev, ERR,
		    "netmap kernel ring %d is NULL\n", qid);
		return;
	}

	lim = kring->nkr_num_slots - 1;
	nm_i = nm_prev(kring->nr_hwcur, lim);

	if (kring->nr_mode != NKR_NETMAP_ON)
		return;

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	netmap_unload_map(na, adapter->rx_buf_tag, rx_info->map);

	KASSERT(kring->ring == NULL, ("Netmap Rx ring is NULL\n"));

	slot = &kring->ring->slot[nm_i];

	ENA_WARN(slot->buf_idx != 0, adapter->ena_dev, "Overwrite slot buf\n");
	slot->buf_idx = rx_info->netmap_buf_idx;
	slot->flags = NS_BUF_CHANGED;

	rx_info->netmap_buf_idx = 0;
	kring->nr_hwcur = nm_i;
}

static bool
ena_ring_in_netmap(struct ena_adapter *adapter, int qid, enum txrx x)
{
	struct netmap_adapter *na;
	struct netmap_kring *kring;

	if (adapter->ifp->if_capenable & IFCAP_NETMAP) {
		na = NA(adapter->ifp);
		kring = (x == NR_RX) ? na->rx_rings[qid] : na->tx_rings[qid];
		if (kring->nr_mode == NKR_NETMAP_ON)
			return true;
	}
	return false;
}

bool
ena_tx_ring_in_netmap(struct ena_adapter *adapter, int qid)
{
	return ena_ring_in_netmap(adapter, qid, NR_TX);
}

bool
ena_rx_ring_in_netmap(struct ena_adapter *adapter, int qid)
{
	return ena_ring_in_netmap(adapter, qid, NR_RX);
}

static void
ena_netmap_reset_ring(struct ena_adapter *adapter, int qid, enum txrx x)
{
	if (!ena_ring_in_netmap(adapter, qid, x))
		return;

	netmap_reset(NA(adapter->ifp), x, qid, 0);
	ena_log_nm(adapter->pdev, INFO, "%s ring %d is in netmap mode\n",
	    (x == NR_TX) ? "Tx" : "Rx", qid);
}

void
ena_netmap_reset_rx_ring(struct ena_adapter *adapter, int qid)
{
	ena_netmap_reset_ring(adapter, qid, NR_RX);
}

void
ena_netmap_reset_tx_ring(struct ena_adapter *adapter, int qid)
{
	ena_netmap_reset_ring(adapter, qid, NR_TX);
}

static int
ena_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct ena_adapter *adapter = ifp->if_softc;
	device_t pdev = adapter->pdev;
	struct netmap_kring *kring;
	enum txrx t;
	int rc, i;

	ENA_LOCK_LOCK();
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	ena_down(adapter);

	if (onoff) {
		ena_log_nm(pdev, INFO, "netmap on\n");
		for_rx_tx(t) {
			for (i = 0; i <= nma_get_nrings(na, t); i++) {
				kring = NMR(na, t)[i];
				if (nm_kring_pending_on(kring)) {
					kring->nr_mode = NKR_NETMAP_ON;
				}
			}
		}
		nm_set_native_flags(na);
	} else {
		ena_log_nm(pdev, INFO, "netmap off\n");
		nm_clear_native_flags(na);
		for_rx_tx(t) {
			for (i = 0; i <= nma_get_nrings(na, t); i++) {
				kring = NMR(na, t)[i];
				if (nm_kring_pending_off(kring)) {
					kring->nr_mode = NKR_NETMAP_OFF;
				}
			}
		}
	}

	rc = ena_up(adapter);
	if (rc != 0) {
		ena_log_nm(pdev, WARN, "ena_up failed with rc=%d\n", rc);
		adapter->reset_reason = ENA_REGS_RESET_DRIVER_INVALID_STATE;
		nm_clear_native_flags(na);
		ena_destroy_device(adapter, false);
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
		rc = ena_restore_device(adapter);
	}
	ENA_LOCK_UNLOCK();

	return (rc);
}

static int
ena_netmap_txsync(struct netmap_kring *kring, int flags)
{
	struct ena_netmap_ctx ctx;
	int rc = 0;

	ena_netmap_fill_ctx(kring, &ctx, ENA_IO_TXQ_IDX(kring->ring_id));
	ctx.ring = &ctx.adapter->tx_ring[kring->ring_id];

	ENA_RING_MTX_LOCK(ctx.ring);
	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_DEV_UP, ctx.adapter)))
		goto txsync_end;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, ctx.adapter)))
		goto txsync_end;

	rc = ena_netmap_tx_frames(&ctx);
	ena_netmap_tx_cleanup(&ctx);

txsync_end:
	ENA_RING_MTX_UNLOCK(ctx.ring);
	return (rc);
}

static int
ena_netmap_tx_frames(struct ena_netmap_ctx *ctx)
{
	struct ena_ring *tx_ring = ctx->ring;
	int rc = 0;

	ctx->nm_i = ctx->kring->nr_hwcur;
	ctx->nt = ctx->ring->next_to_use;

	__builtin_prefetch(&ctx->slots[ctx->nm_i]);

	while (ctx->nm_i != ctx->kring->rhead) {
		if ((rc = ena_netmap_tx_frame(ctx)) != 0) {
			/*
			 * When there is no empty space in Tx ring, error is
			 * still being returned. It should not be passed to the
			 * netmap, as application knows current ring state from
			 * netmap ring pointers. Returning error there could
			 * cause application to exit, but the Tx ring is
			 * commonly being full.
			 */
			if (rc == ENA_COM_NO_MEM)
				rc = 0;
			break;
		}
		tx_ring->acum_pkts++;
	}

	/* If any packet was sent... */
	if (likely(ctx->nm_i != ctx->kring->nr_hwcur)) {
		/* ...send the doorbell to the device. */
		ena_ring_tx_doorbell(tx_ring);

		ctx->ring->next_to_use = ctx->nt;
		ctx->kring->nr_hwcur = ctx->nm_i;
	}

	return (rc);
}

static int
ena_netmap_tx_frame(struct ena_netmap_ctx *ctx)
{
	struct ena_com_tx_ctx ena_tx_ctx;
	struct ena_adapter *adapter;
	struct ena_ring *tx_ring;
	struct ena_tx_buffer *tx_info;
	uint16_t req_id;
	uint16_t header_len;
	uint16_t packet_len;
	int nb_hw_desc;
	int rc;
	void *push_hdr;

	adapter = ctx->adapter;
	if (ena_netmap_count_slots(ctx) > adapter->max_tx_sgl_size) {
		ena_log_nm(adapter->pdev, WARN, "Too many slots per packet\n");
		return (EINVAL);
	}

	tx_ring = ctx->ring;

	req_id = tx_ring->free_tx_ids[ctx->nt];
	tx_info = &tx_ring->tx_buffer_info[req_id];
	tx_info->num_of_bufs = 0;
	tx_info->nm_info.sockets_used = 0;

	rc = ena_netmap_tx_map_slots(ctx, tx_info, &push_hdr, &header_len,
	    &packet_len);
	if (unlikely(rc != 0)) {
		ena_log_nm(adapter->pdev, ERR, "Failed to map Tx slot\n");
		return (rc);
	}

	bzero(&ena_tx_ctx, sizeof(struct ena_com_tx_ctx));
	ena_tx_ctx.ena_bufs = tx_info->bufs;
	ena_tx_ctx.push_header = push_hdr;
	ena_tx_ctx.num_bufs = tx_info->num_of_bufs;
	ena_tx_ctx.req_id = req_id;
	ena_tx_ctx.header_len = header_len;
	ena_tx_ctx.meta_valid = adapter->disable_meta_caching;

	/* There are no any offloads, as the netmap doesn't support them */

	if (tx_ring->acum_pkts == DB_THRESHOLD ||
	    ena_com_is_doorbell_needed(ctx->io_sq, &ena_tx_ctx))
		ena_ring_tx_doorbell(tx_ring);

	rc = ena_com_prepare_tx(ctx->io_sq, &ena_tx_ctx, &nb_hw_desc);
	if (unlikely(rc != 0)) {
		if (likely(rc == ENA_COM_NO_MEM)) {
			ena_log_nm(adapter->pdev, DBG,
			    "Tx ring[%d] is out of space\n", tx_ring->que->id);
		} else {
			ena_log_nm(adapter->pdev, ERR,
			    "Failed to prepare Tx bufs\n");
			ena_trigger_reset(adapter,
			    ENA_REGS_RESET_DRIVER_INVALID_STATE);
		}
		counter_u64_add(tx_ring->tx_stats.prepare_ctx_err, 1);

		ena_netmap_unmap_last_socket_chain(ctx, tx_info);
		return (rc);
	}

	counter_enter();
	counter_u64_add_protected(tx_ring->tx_stats.cnt, 1);
	counter_u64_add_protected(tx_ring->tx_stats.bytes, packet_len);
	counter_u64_add_protected(adapter->hw_stats.tx_packets, 1);
	counter_u64_add_protected(adapter->hw_stats.tx_bytes, packet_len);
	counter_exit();

	tx_info->tx_descs = nb_hw_desc;

	ctx->nt = ENA_TX_RING_IDX_NEXT(ctx->nt, ctx->ring->ring_size);

	for (unsigned int i = 0; i < tx_info->num_of_bufs; i++)
		bus_dmamap_sync(adapter->tx_buf_tag,
		    tx_info->nm_info.map_seg[i], BUS_DMASYNC_PREWRITE);

	return (0);
}

static inline uint16_t
ena_netmap_count_slots(struct ena_netmap_ctx *ctx)
{
	uint16_t slots = 1;
	uint16_t nm = ctx->nm_i;

	while ((ctx->slots[nm].flags & NS_MOREFRAG) != 0) {
		slots++;
		nm = nm_next(nm, ctx->lim);
	}

	return slots;
}

static inline uint16_t
ena_netmap_packet_len(struct netmap_slot *slots, u_int slot_index,
    uint16_t limit)
{
	struct netmap_slot *nm_slot;
	uint16_t packet_size = 0;

	do {
		nm_slot = &slots[slot_index];
		packet_size += nm_slot->len;
		slot_index = nm_next(slot_index, limit);
	} while ((nm_slot->flags & NS_MOREFRAG) != 0);

	return packet_size;
}

static int
ena_netmap_copy_data(struct netmap_adapter *na, struct netmap_slot *slots,
    u_int slot_index, uint16_t limit, uint16_t bytes_to_copy, void *destination)
{
	struct netmap_slot *nm_slot;
	void *slot_vaddr;
	uint16_t packet_size;
	uint16_t data_amount;

	packet_size = 0;
	do {
		nm_slot = &slots[slot_index];
		slot_vaddr = NMB(na, nm_slot);
		if (unlikely(slot_vaddr == NULL))
			return (EINVAL);

		data_amount = min_t(uint16_t, bytes_to_copy, nm_slot->len);
		memcpy(destination, slot_vaddr, data_amount);
		bytes_to_copy -= data_amount;

		slot_index = nm_next(slot_index, limit);
	} while ((nm_slot->flags & NS_MOREFRAG) != 0 && bytes_to_copy > 0);

	return (0);
}

static int
ena_netmap_map_single_slot(struct netmap_adapter *na, struct netmap_slot *slot,
    bus_dma_tag_t dmatag, bus_dmamap_t dmamap, void **vaddr, uint64_t *paddr)
{
	device_t pdev;
	int rc;

	pdev = ((struct ena_adapter *)na->ifp->if_softc)->pdev;

	*vaddr = PNMB(na, slot, paddr);
	if (unlikely(vaddr == NULL)) {
		ena_log_nm(pdev, ERR, "Slot address is NULL\n");
		return (EINVAL);
	}

	rc = netmap_load_map(na, dmatag, dmamap, *vaddr);
	if (unlikely(rc != 0)) {
		ena_log_nm(pdev, ERR, "Failed to map slot %d for DMA\n",
		    slot->buf_idx);
		return (EINVAL);
	}

	return (0);
}

static int
ena_netmap_tx_map_slots(struct ena_netmap_ctx *ctx,
    struct ena_tx_buffer *tx_info, void **push_hdr, uint16_t *header_len,
    uint16_t *packet_len)
{
	struct netmap_slot *slot;
	struct ena_com_buf *ena_buf;
	struct ena_adapter *adapter;
	struct ena_ring *tx_ring;
	struct ena_netmap_tx_info *nm_info;
	bus_dmamap_t *nm_maps;
	void *vaddr;
	uint64_t paddr;
	uint32_t *nm_buf_idx;
	uint32_t slot_head_len;
	uint32_t frag_len;
	uint32_t remaining_len;
	uint16_t push_len;
	uint16_t delta;
	int rc;

	adapter = ctx->adapter;
	tx_ring = ctx->ring;
	ena_buf = tx_info->bufs;
	nm_info = &tx_info->nm_info;
	nm_maps = nm_info->map_seg;
	nm_buf_idx = nm_info->socket_buf_idx;
	slot = &ctx->slots[ctx->nm_i];

	slot_head_len = slot->len;
	*packet_len = ena_netmap_packet_len(ctx->slots, ctx->nm_i, ctx->lim);
	remaining_len = *packet_len;
	delta = 0;

	__builtin_prefetch(&ctx->slots[ctx->nm_i + 1]);
	if (tx_ring->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		/*
		 * When the device is in LLQ mode, the driver will copy
		 * the header into the device memory space.
		 * The ena_com layer assumes that the header is in a linear
		 * memory space.
		 * This assumption might be wrong since part of the header
		 * can be in the fragmented buffers.
		 * First, check if header fits in the first slot. If not, copy
		 * it to separate buffer that will be holding linearized data.
		 */
		push_len = min_t(uint32_t, *packet_len,
		    tx_ring->tx_max_header_size);
		*header_len = push_len;
		/* If header is in linear space, just point to socket's data. */
		if (likely(push_len <= slot_head_len)) {
			*push_hdr = NMB(ctx->na, slot);
			if (unlikely(push_hdr == NULL)) {
				ena_log_nm(adapter->pdev, ERR,
				    "Slot vaddress is NULL\n");
				return (EINVAL);
			}
		/*
		 * Otherwise, copy whole portion of header from multiple
		 * slots to intermediate buffer.
		 */
		} else {
			rc = ena_netmap_copy_data(ctx->na, ctx->slots,
			    ctx->nm_i, ctx->lim, push_len,
			    tx_ring->push_buf_intermediate_buf);
			if (unlikely(rc)) {
				ena_log_nm(adapter->pdev, ERR,
				    "Failed to copy data from slots to push_buf\n");
				return (EINVAL);
			}

			*push_hdr = tx_ring->push_buf_intermediate_buf;
			counter_u64_add(tx_ring->tx_stats.llq_buffer_copy, 1);

			delta = push_len - slot_head_len;
		}

		ena_log_nm(adapter->pdev, DBG,
		    "slot: %d header_buf->vaddr: %p push_len: %d\n",
		    slot->buf_idx, *push_hdr, push_len);

		/*
		 * If header was in linear memory space, map for the dma rest of
		 * the data in the first mbuf of the mbuf chain.
		 */
		if (slot_head_len > push_len) {
			rc = ena_netmap_map_single_slot(ctx->na, slot,
			    adapter->tx_buf_tag, *nm_maps, &vaddr, &paddr);
			if (unlikely(rc != 0)) {
				ena_log_nm(adapter->pdev, ERR,
				    "DMA mapping error\n");
				return (rc);
			}
			nm_maps++;

			ena_buf->paddr = paddr + push_len;
			ena_buf->len = slot->len - push_len;
			ena_buf++;

			tx_info->num_of_bufs++;
		}

		remaining_len -= slot->len;

		/* Save buf idx before advancing */
		*nm_buf_idx = slot->buf_idx;
		nm_buf_idx++;
		slot->buf_idx = 0;

		/* Advance to the next socket */
		ctx->nm_i = nm_next(ctx->nm_i, ctx->lim);
		slot = &ctx->slots[ctx->nm_i];
		nm_info->sockets_used++;

		/*
		 * If header is in non linear space (delta > 0), then skip mbufs
		 * containing header and map the last one containing both header
		 * and the packet data.
		 * The first segment is already counted in.
		 */
		while (delta > 0) {
			__builtin_prefetch(&ctx->slots[ctx->nm_i + 1]);
			frag_len = slot->len;

			/*
			 * If whole segment contains header just move to the
			 * next one and reduce delta.
			 */
			if (unlikely(delta >= frag_len)) {
				delta -= frag_len;
			} else {
				/*
				 * Map the data and then assign it with the
				 * offsets
				 */
				rc = ena_netmap_map_single_slot(ctx->na, slot,
				    adapter->tx_buf_tag, *nm_maps, &vaddr,
				    &paddr);
				if (unlikely(rc != 0)) {
					ena_log_nm(adapter->pdev, ERR,
					    "DMA mapping error\n");
					goto error_map;
				}
				nm_maps++;

				ena_buf->paddr = paddr + delta;
				ena_buf->len = slot->len - delta;
				ena_buf++;

				tx_info->num_of_bufs++;
				delta = 0;
			}

			remaining_len -= slot->len;

			/* Save buf idx before advancing */
			*nm_buf_idx = slot->buf_idx;
			nm_buf_idx++;
			slot->buf_idx = 0;

			/* Advance to the next socket */
			ctx->nm_i = nm_next(ctx->nm_i, ctx->lim);
			slot = &ctx->slots[ctx->nm_i];
			nm_info->sockets_used++;
		}
	} else {
		*push_hdr = NULL;
		/*
		 * header_len is just a hint for the device. Because netmap is
		 * not giving us any information about packet header length and
		 * it is not guaranteed that all packet headers will be in the
		 * 1st slot, setting header_len to 0 is making the device ignore
		 * this value and resolve header on it's own.
		 */
		*header_len = 0;
	}

	/* Map all remaining data (regular routine for non-LLQ mode) */
	while (remaining_len > 0) {
		__builtin_prefetch(&ctx->slots[ctx->nm_i + 1]);

		rc = ena_netmap_map_single_slot(ctx->na, slot,
		    adapter->tx_buf_tag, *nm_maps, &vaddr, &paddr);
		if (unlikely(rc != 0)) {
			ena_log_nm(adapter->pdev, ERR, "DMA mapping error\n");
			goto error_map;
		}
		nm_maps++;

		ena_buf->paddr = paddr;
		ena_buf->len = slot->len;
		ena_buf++;

		tx_info->num_of_bufs++;

		remaining_len -= slot->len;

		/* Save buf idx before advancing */
		*nm_buf_idx = slot->buf_idx;
		nm_buf_idx++;
		slot->buf_idx = 0;

		/* Advance to the next socket */
		ctx->nm_i = nm_next(ctx->nm_i, ctx->lim);
		slot = &ctx->slots[ctx->nm_i];
		nm_info->sockets_used++;
	}

	return (0);

error_map:
	ena_netmap_unmap_last_socket_chain(ctx, tx_info);

	return (rc);
}

static void
ena_netmap_unmap_last_socket_chain(struct ena_netmap_ctx *ctx,
    struct ena_tx_buffer *tx_info)
{
	struct ena_netmap_tx_info *nm_info;
	int n;

	nm_info = &tx_info->nm_info;

	/**
	 * As the used sockets must not be equal to the buffers used in the LLQ
	 * mode, they must be treated separately.
	 * First, unmap the DMA maps.
	 */
	n = tx_info->num_of_bufs;
	while (n--) {
		netmap_unload_map(ctx->na, ctx->adapter->tx_buf_tag,
		    nm_info->map_seg[n]);
	}
	tx_info->num_of_bufs = 0;

	/* Next, retain the sockets back to the userspace */
	n = nm_info->sockets_used;
	while (n--) {
		ctx->slots[ctx->nm_i].buf_idx = nm_info->socket_buf_idx[n];
		ctx->slots[ctx->nm_i].flags = NS_BUF_CHANGED;
		nm_info->socket_buf_idx[n] = 0;
		ctx->nm_i = nm_prev(ctx->nm_i, ctx->lim);
	}
	nm_info->sockets_used = 0;
}

static void
ena_netmap_tx_cleanup(struct ena_netmap_ctx *ctx)
{
	uint16_t req_id;
	uint16_t total_tx_descs = 0;

	ctx->nm_i = ctx->kring->nr_hwtail;
	ctx->nt = ctx->ring->next_to_clean;

	/* Reclaim buffers for completed transmissions */
	while (ena_com_tx_comp_req_id_get(ctx->io_cq, &req_id) >= 0) {
		if (validate_tx_req_id(ctx->ring, req_id) != 0)
			break;
		total_tx_descs += ena_netmap_tx_clean_one(ctx, req_id);
	}

	ctx->kring->nr_hwtail = ctx->nm_i;

	if (total_tx_descs > 0) {
		/* acknowledge completion of sent packets */
		ctx->ring->next_to_clean = ctx->nt;
		ena_com_comp_ack(ctx->ring->ena_com_io_sq, total_tx_descs);
		ena_com_update_dev_comp_head(ctx->ring->ena_com_io_cq);
	}
}

static uint16_t
ena_netmap_tx_clean_one(struct ena_netmap_ctx *ctx, uint16_t req_id)
{
	struct ena_tx_buffer *tx_info;
	struct ena_netmap_tx_info *nm_info;
	int n;

	tx_info = &ctx->ring->tx_buffer_info[req_id];
	nm_info = &tx_info->nm_info;

	/**
	 * As the used sockets must not be equal to the buffers used in the LLQ
	 * mode, they must be treated separately.
	 * First, unmap the DMA maps.
	 */
	n = tx_info->num_of_bufs;
	for (n = 0; n < tx_info->num_of_bufs; n++) {
		netmap_unload_map(ctx->na, ctx->adapter->tx_buf_tag,
		    nm_info->map_seg[n]);
	}
	tx_info->num_of_bufs = 0;

	/* Next, retain the sockets back to the userspace */
	for (n = 0; n < nm_info->sockets_used; n++) {
		ctx->nm_i = nm_next(ctx->nm_i, ctx->lim);
		ENA_WARN(ctx->slots[ctx->nm_i].buf_idx != 0,
		    ctx->adapter->ena_dev, "Tx idx is not 0.\n");
		ctx->slots[ctx->nm_i].buf_idx = nm_info->socket_buf_idx[n];
		ctx->slots[ctx->nm_i].flags = NS_BUF_CHANGED;
		nm_info->socket_buf_idx[n] = 0;
	}
	nm_info->sockets_used = 0;

	ctx->ring->free_tx_ids[ctx->nt] = req_id;
	ctx->nt = ENA_TX_RING_IDX_NEXT(ctx->nt, ctx->lim);

	return tx_info->tx_descs;
}

static inline int
validate_tx_req_id(struct ena_ring *tx_ring, uint16_t req_id)
{
	struct ena_adapter *adapter = tx_ring->adapter;

	if (likely(req_id < tx_ring->ring_size))
		return (0);

	ena_log_nm(adapter->pdev, WARN, "Invalid req_id %hu in qid %hu\n",
	    req_id, tx_ring->qid);
	counter_u64_add(tx_ring->tx_stats.bad_req_id, 1);

	ena_trigger_reset(adapter, ENA_REGS_RESET_INV_TX_REQ_ID);

	return (EFAULT);
}

static int
ena_netmap_rxsync(struct netmap_kring *kring, int flags)
{
	struct ena_netmap_ctx ctx;
	int rc;

	ena_netmap_fill_ctx(kring, &ctx, ENA_IO_RXQ_IDX(kring->ring_id));
	ctx.ring = &ctx.adapter->rx_ring[kring->ring_id];

	if (ctx.kring->rhead > ctx.lim) {
		/* Probably not needed to release slots from RX ring. */
		return (netmap_ring_reinit(ctx.kring));
	}

	if (unlikely((if_getdrvflags(ctx.na->ifp) & IFF_DRV_RUNNING) == 0))
		return (0);

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, ctx.adapter)))
		return (0);

	if ((rc = ena_netmap_rx_frames(&ctx)) != 0)
		return (rc);

	ena_netmap_rx_cleanup(&ctx);

	return (0);
}

static inline int
ena_netmap_rx_frames(struct ena_netmap_ctx *ctx)
{
	int rc = 0;
	int frames_counter = 0;

	ctx->nt = ctx->ring->next_to_clean;
	ctx->nm_i = ctx->kring->nr_hwtail;

	while ((rc = ena_netmap_rx_frame(ctx)) == ENA_NETMAP_MORE_FRAMES) {
		frames_counter++;
		/* In case of multiple frames, it is not an error. */
		rc = 0;
		if (frames_counter > ENA_MAX_FRAMES) {
			ena_log_nm(ctx->adapter->pdev, ERR,
			    "Driver is stuck in the Rx loop\n");
			break;
		}
	};

	ctx->kring->nr_hwtail = ctx->nm_i;
	ctx->kring->nr_kflags &= ~NKR_PENDINTR;
	ctx->ring->next_to_clean = ctx->nt;

	return (rc);
}

static inline int
ena_netmap_rx_frame(struct ena_netmap_ctx *ctx)
{
	struct ena_com_rx_ctx ena_rx_ctx;
	enum ena_regs_reset_reason_types reset_reason;
	int rc, len = 0;
	uint16_t buf, nm;

	ena_rx_ctx.ena_bufs = ctx->ring->ena_bufs;
	ena_rx_ctx.max_bufs = ctx->adapter->max_rx_sgl_size;
	bus_dmamap_sync(ctx->io_cq->cdesc_addr.mem_handle.tag,
	    ctx->io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_POSTREAD);

	rc = ena_com_rx_pkt(ctx->io_cq, ctx->io_sq, &ena_rx_ctx);
	if (unlikely(rc != 0)) {
		ena_log_nm(ctx->adapter->pdev, ERR,
		    "Failed to read pkt from the device with error: %d\n", rc);
		if (rc == ENA_COM_NO_SPACE) {
			counter_u64_add(ctx->ring->rx_stats.bad_desc_num, 1);
			reset_reason = ENA_REGS_RESET_TOO_MANY_RX_DESCS;
		} else {
			counter_u64_add(ctx->ring->rx_stats.bad_req_id, 1);
			reset_reason = ENA_REGS_RESET_INV_RX_REQ_ID;
		}
		ena_trigger_reset(ctx->adapter, reset_reason);
		return (rc);
	}
	if (unlikely(ena_rx_ctx.descs == 0))
		return (ENA_NETMAP_NO_MORE_FRAMES);

	ena_log_nm(ctx->adapter->pdev, DBG,
	    "Rx: q %d got packet from ena. descs #:"
	    " %d l3 proto %d l4 proto %d hash: %x\n",
	    ctx->ring->qid, ena_rx_ctx.descs, ena_rx_ctx.l3_proto,
	    ena_rx_ctx.l4_proto, ena_rx_ctx.hash);

	for (buf = 0; buf < ena_rx_ctx.descs; buf++)
		if ((rc = ena_netmap_rx_load_desc(ctx, buf, &len)) != 0)
			break;
	/*
	 * ena_netmap_rx_load_desc doesn't know the number of descriptors.
	 * It just set flag NS_MOREFRAG to all slots, then here flag of
	 * last slot is cleared.
	 */
	ctx->slots[nm_prev(ctx->nm_i, ctx->lim)].flags = NS_BUF_CHANGED;

	if (rc != 0) {
		goto rx_clear_desc;
	}

	bus_dmamap_sync(ctx->io_cq->cdesc_addr.mem_handle.tag,
	    ctx->io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_PREREAD);

	counter_enter();
	counter_u64_add_protected(ctx->ring->rx_stats.bytes, len);
	counter_u64_add_protected(ctx->adapter->hw_stats.rx_bytes, len);
	counter_u64_add_protected(ctx->ring->rx_stats.cnt, 1);
	counter_u64_add_protected(ctx->adapter->hw_stats.rx_packets, 1);
	counter_exit();

	return (ENA_NETMAP_MORE_FRAMES);

rx_clear_desc:
	nm = ctx->nm_i;

	/* Remove failed packet from ring */
	while (buf--) {
		ctx->slots[nm].flags = 0;
		ctx->slots[nm].len = 0;
		nm = nm_prev(nm, ctx->lim);
	}

	return (rc);
}

static inline int
ena_netmap_rx_load_desc(struct ena_netmap_ctx *ctx, uint16_t buf, int *len)
{
	struct ena_rx_buffer *rx_info;
	uint16_t req_id;

	req_id = ctx->ring->ena_bufs[buf].req_id;
	rx_info = &ctx->ring->rx_buffer_info[req_id];
	bus_dmamap_sync(ctx->adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	netmap_unload_map(ctx->na, ctx->adapter->rx_buf_tag, rx_info->map);

	ENA_WARN(ctx->slots[ctx->nm_i].buf_idx != 0, ctx->adapter->ena_dev,
	    "Rx idx is not 0.\n");

	ctx->slots[ctx->nm_i].buf_idx = rx_info->netmap_buf_idx;
	rx_info->netmap_buf_idx = 0;
	/*
	 * Set NS_MOREFRAG to all slots.
	 * Then ena_netmap_rx_frame clears it from last one.
	 */
	ctx->slots[ctx->nm_i].flags |= NS_MOREFRAG | NS_BUF_CHANGED;
	ctx->slots[ctx->nm_i].len = ctx->ring->ena_bufs[buf].len;
	*len += ctx->slots[ctx->nm_i].len;
	ctx->ring->free_rx_ids[ctx->nt] = req_id;
	ena_log_nm(ctx->adapter->pdev, DBG,
	    "rx_info %p, buf_idx %d, paddr %jx, nm: %d\n", rx_info,
	    ctx->slots[ctx->nm_i].buf_idx, (uintmax_t)rx_info->ena_buf.paddr,
	    ctx->nm_i);

	ctx->nm_i = nm_next(ctx->nm_i, ctx->lim);
	ctx->nt = ENA_RX_RING_IDX_NEXT(ctx->nt, ctx->ring->ring_size);

	return (0);
}

static inline void
ena_netmap_rx_cleanup(struct ena_netmap_ctx *ctx)
{
	int refill_required;

	refill_required = ctx->kring->rhead - ctx->kring->nr_hwcur;
	if (ctx->kring->nr_hwcur != ctx->kring->nr_hwtail)
		refill_required -= 1;

	if (refill_required == 0)
		return;
	else if (refill_required < 0)
		refill_required += ctx->kring->nkr_num_slots;

	ena_refill_rx_bufs(ctx->ring, refill_required);
}

static inline void
ena_netmap_fill_ctx(struct netmap_kring *kring, struct ena_netmap_ctx *ctx,
    uint16_t ena_qid)
{
	ctx->kring = kring;
	ctx->na = kring->na;
	ctx->adapter = ctx->na->ifp->if_softc;
	ctx->lim = kring->nkr_num_slots - 1;
	ctx->io_cq = &ctx->adapter->ena_dev->io_cq_queues[ena_qid];
	ctx->io_sq = &ctx->adapter->ena_dev->io_sq_queues[ena_qid];
	ctx->slots = kring->ring->slot;
}

void
ena_netmap_unload(struct ena_adapter *adapter, bus_dmamap_t map)
{
	struct netmap_adapter *na = NA(adapter->ifp);

	netmap_unload_map(na, adapter->tx_buf_tag, map);
}

#endif /* DEV_NETMAP */
