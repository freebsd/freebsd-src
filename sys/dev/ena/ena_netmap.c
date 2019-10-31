/*-
 * BSD LICENSE
 *
 * Copyright (c) 2015-2019 Amazon.com, Inc. or its affiliates.
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
static int ena_netmap_rx_frames(struct ena_netmap_ctx *);
static int ena_netmap_rx_frame(struct ena_netmap_ctx *);
static int ena_netmap_rx_load_desc(struct ena_netmap_ctx *, uint16_t,
    int *);
static void ena_netmap_rx_cleanup(struct ena_netmap_ctx *);
static void ena_netmap_fill_ctx(struct netmap_kring *,
    struct ena_netmap_ctx *, uint16_t);

int
ena_netmap_attach(struct ena_adapter *adapter)
{
	struct netmap_adapter na;

	ena_trace(ENA_NETMAP, "netmap attach\n");

	bzero(&na, sizeof(na));
	na.na_flags = NAF_MOREFRAG;
	na.ifp = adapter->ifp;
	na.num_tx_desc = adapter->tx_ring_size;
	na.num_rx_desc = adapter->rx_ring_size;
	na.num_tx_rings = adapter->num_queues;
	na.num_rx_rings = adapter->num_queues;
	na.rx_buf_maxsize = adapter->buf_ring_size;
	na.nm_txsync = ena_netmap_txsync;
	na.nm_rxsync = ena_netmap_rxsync;
	na.nm_register = ena_netmap_reg;

	return (netmap_attach(&na));
}

int
ena_netmap_alloc_rx_slot(struct ena_adapter *adapter,
    struct ena_ring *rx_ring, struct ena_rx_buffer *rx_info)
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

	ena_trace(ENA_NETMAP | ENA_DBG, "nr_hwcur: %d, nr_hwtail: %d, "
	    "rhead: %d, rcur: %d, rtail: %d\n", kring->nr_hwcur,
	    kring->nr_hwtail, kring->rhead, kring->rcur, kring->rtail);

	if ((nm_i == head) && rx_ring->initialized) {
		ena_trace(ENA_NETMAP, "No free slots in netmap ring\n");
		return (ENOMEM);
	}

	ring = kring->ring;
	if (ring == NULL) {
		device_printf(adapter->pdev, "Rx ring %d is NULL\n", qid);
		return (EFAULT);
	}
	slot = &ring->slot[nm_i];

	addr = PNMB(na, slot, &paddr);
	if (addr == NETMAP_BUF_BASE(na)) {
		device_printf(adapter->pdev, "Bad buff in slot\n");
		return (EFAULT);
	}

	rc = netmap_load_map(na, adapter->rx_buf_tag, rx_info->map, addr);
	if (rc != 0) {
		ena_trace(ENA_WARNING, "DMA mapping error\n");
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
ena_netmap_free_rx_slot(struct ena_adapter *adapter,
    struct ena_ring *rx_ring, struct ena_rx_buffer *rx_info)
{
	struct netmap_adapter *na;
	struct netmap_kring *kring;
	struct netmap_slot *slot;
	int nm_i, qid, lim;

	na = NA(adapter->ifp);
	if (na == NULL) {
		device_printf(adapter->pdev, "netmap adapter is NULL\n");
		return;
	}

	if (na->rx_rings == NULL) {
		device_printf(adapter->pdev, "netmap rings are NULL\n");
		return;
	}

	qid = rx_ring->qid;
	kring = na->rx_rings[qid];
	if (kring == NULL) {
		device_printf(adapter->pdev,
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

	slot = &kring->ring->slot[nm_i];

	ENA_ASSERT(slot->buf_idx == 0, "Overwrite slot buf\n");
	slot->buf_idx = rx_info->netmap_buf_idx;
	slot->flags = NS_BUF_CHANGED;

	rx_info->netmap_buf_idx = 0;
	kring->nr_hwcur = nm_i;
}

void
ena_netmap_reset_rx_ring(struct ena_adapter *adapter, int qid)
{
	if (adapter->ifp->if_capenable & IFCAP_NETMAP)
		netmap_reset(NA(adapter->ifp), NR_RX, qid, 0);
}

static int
ena_netmap_reg(struct netmap_adapter *na, int onoff)
{
	struct ifnet *ifp = na->ifp;
	struct ena_adapter* adapter = ifp->if_softc;
	int rc;

	sx_xlock(&adapter->ioctl_sx);
	ENA_FLAG_CLEAR_ATOMIC(ENA_FLAG_TRIGGER_RESET, adapter);
	ena_down(adapter);

	if (onoff) {
		ena_trace(ENA_NETMAP, "netmap on\n");
		nm_set_native_flags(na);
	} else {
		ena_trace(ENA_NETMAP, "netmap off\n");
		nm_clear_native_flags(na);
	}

	rc = ena_up(adapter);
	if (rc != 0) {
		ena_trace(ENA_WARNING, "ena_up failed with rc=%d\n", rc);
		adapter->reset_reason = ENA_REGS_RESET_DRIVER_INVALID_STATE;
		nm_clear_native_flags(na);
		ena_destroy_device(adapter, false);
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_DEV_UP_BEFORE_RESET, adapter);
		rc = ena_restore_device(adapter);
	}
	sx_unlock(&adapter->ioctl_sx);

	return (rc);
}

static int
ena_netmap_txsync(struct netmap_kring *kring, int flags)
{
	ena_trace(ENA_NETMAP, "netmap txsync\n");
	return (0);
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

	while((rc = ena_netmap_rx_frame(ctx)) == ENA_NETMAP_MORE_FRAMES) {
		frames_counter++;
		/* In case of multiple frames, it is not an error. */
		rc = 0;
		if (frames_counter > ENA_MAX_FRAMES) {
			device_printf(ctx->adapter->pdev,
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
	int rc, len = 0;
	uint16_t buf, nm;

	ena_rx_ctx.ena_bufs = ctx->ring->ena_bufs;
	ena_rx_ctx.max_bufs = ctx->adapter->max_rx_sgl_size;
	bus_dmamap_sync(ctx->io_cq->cdesc_addr.mem_handle.tag,
	    ctx->io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_POSTREAD);

	rc = ena_com_rx_pkt(ctx->io_cq, ctx->io_sq, &ena_rx_ctx);
	if (unlikely(rc != 0)) {
		ena_trace(ENA_ALERT, "Too many desc from the device.\n");
		counter_u64_add(ctx->ring->rx_stats.bad_desc_num, 1);
		ctx->adapter->reset_reason = ENA_REGS_RESET_TOO_MANY_RX_DESCS;
		ENA_FLAG_SET_ATOMIC(ENA_FLAG_TRIGGER_RESET, ctx->adapter);
		return (rc);
	}
	if (unlikely(ena_rx_ctx.descs == 0))
		return (ENA_NETMAP_NO_MORE_FRAMES);

        ena_trace(ENA_NETMAP | ENA_DBG, "Rx: q %d got packet from ena. descs #:"
	    " %d l3 proto %d l4 proto %d hash: %x\n", ctx->ring->qid,
	    ena_rx_ctx.descs, ena_rx_ctx.l3_proto, ena_rx_ctx.l4_proto,
	    ena_rx_ctx.hash);

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
	while(buf--) {
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
	int rc;

	req_id = ctx->ring->ena_bufs[buf].req_id;
	rc = validate_rx_req_id(ctx->ring, req_id);
	if (unlikely(rc != 0))
		return (rc);

	rx_info = &ctx->ring->rx_buffer_info[req_id];
	bus_dmamap_sync(ctx->adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	netmap_unload_map(ctx->na, ctx->adapter->rx_buf_tag, rx_info->map);

	ENA_ASSERT(ctx->slots[ctx->nm_i].buf_idx == 0, "Rx idx is not 0.\n");

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
	ena_trace(ENA_DBG, "rx_info %p, buf_idx %d, paddr %jx, nm: %d\n",
	    rx_info, ctx->slots[ctx->nm_i].buf_idx,
	    (uintmax_t)rx_info->ena_buf.paddr, ctx->nm_i);

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

#endif /* DEV_NETMAP */
