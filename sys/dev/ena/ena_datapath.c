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

#include "opt_rss.h"

#include "ena.h"
#include "ena_datapath.h"
#ifdef DEV_NETMAP
#include "ena_netmap.h"
#endif /* DEV_NETMAP */
#ifdef RSS
#include <net/rss_config.h>
#endif /* RSS */

#include <netinet6/ip6_var.h>

/*********************************************************************
 *  Static functions prototypes
 *********************************************************************/

static int ena_tx_cleanup(struct ena_ring *);
static int ena_rx_cleanup(struct ena_ring *);
static inline int ena_get_tx_req_id(struct ena_ring *tx_ring,
    struct ena_com_io_cq *io_cq, uint16_t *req_id);
static void ena_rx_hash_mbuf(struct ena_ring *, struct ena_com_rx_ctx *,
    struct mbuf *);
static struct mbuf *ena_rx_mbuf(struct ena_ring *, struct ena_com_rx_buf_info *,
    struct ena_com_rx_ctx *, uint16_t *);
static inline void ena_rx_checksum(struct ena_ring *, struct ena_com_rx_ctx *,
    struct mbuf *);
static void ena_tx_csum(struct ena_com_tx_ctx *, struct mbuf *, bool);
static int ena_check_and_collapse_mbuf(struct ena_ring *tx_ring,
    struct mbuf **mbuf);
static int ena_xmit_mbuf(struct ena_ring *, struct mbuf **);
static void ena_start_xmit(struct ena_ring *);

/*********************************************************************
 *  Global functions
 *********************************************************************/

void
ena_cleanup(void *arg, int pending)
{
	struct ena_que *que = arg;
	struct ena_adapter *adapter = que->adapter;
	if_t ifp = adapter->ifp;
	struct ena_ring *tx_ring;
	struct ena_ring *rx_ring;
	struct ena_com_io_cq *io_cq;
	struct ena_eth_io_intr_reg intr_reg;
	int qid, ena_qid;
	int txc, rxc, i;

	if (unlikely((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
		return;

	ena_log_io(adapter->pdev, DBG, "MSI-X TX/RX routine\n");

	tx_ring = que->tx_ring;
	rx_ring = que->rx_ring;
	qid = que->id;
	ena_qid = ENA_IO_TXQ_IDX(qid);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];

	atomic_store_8(&tx_ring->first_interrupt, true);
	atomic_store_8(&rx_ring->first_interrupt, true);

	for (i = 0; i < CLEAN_BUDGET; ++i) {
		rxc = ena_rx_cleanup(rx_ring);
		txc = ena_tx_cleanup(tx_ring);

		if (unlikely((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0))
			return;

		if ((txc != TX_BUDGET) && (rxc != RX_BUDGET))
			break;
	}

	/* Signal that work is done and unmask interrupt */
	ena_com_update_intr_reg(&intr_reg, RX_IRQ_INTERVAL, TX_IRQ_INTERVAL,
	    true);
	counter_u64_add(tx_ring->tx_stats.unmask_interrupt_num, 1);
	ena_com_unmask_intr(io_cq, &intr_reg);
}

void
ena_deferred_mq_start(void *arg, int pending)
{
	struct ena_ring *tx_ring = (struct ena_ring *)arg;
	struct ifnet *ifp = tx_ring->adapter->ifp;

	while (!drbr_empty(ifp, tx_ring->br) && tx_ring->running &&
	    (if_getdrvflags(ifp) & IFF_DRV_RUNNING) != 0) {
		ENA_RING_MTX_LOCK(tx_ring);
		ena_start_xmit(tx_ring);
		ENA_RING_MTX_UNLOCK(tx_ring);
	}
}

int
ena_mq_start(if_t ifp, struct mbuf *m)
{
	struct ena_adapter *adapter = ifp->if_softc;
	struct ena_ring *tx_ring;
	int ret, is_drbr_empty;
	uint32_t i;
#ifdef RSS
	uint32_t bucket_id;
#endif

	if (unlikely((if_getdrvflags(adapter->ifp) & IFF_DRV_RUNNING) == 0))
		return (ENODEV);

	/* Which queue to use */
	/*
	 * If everything is setup correctly, it should be the
	 * same bucket that the current CPU we're on is.
	 * It should improve performance.
	 */
	if (M_HASHTYPE_GET(m) != M_HASHTYPE_NONE) {
#ifdef RSS
		if (rss_hash2bucket(m->m_pkthdr.flowid, M_HASHTYPE_GET(m),
		    &bucket_id) == 0)
			i = bucket_id % adapter->num_io_queues;
		else
#endif
			i = m->m_pkthdr.flowid % adapter->num_io_queues;
	} else {
		i = curcpu % adapter->num_io_queues;
	}
	tx_ring = &adapter->tx_ring[i];

	/* Check if drbr is empty before putting packet */
	is_drbr_empty = drbr_empty(ifp, tx_ring->br);
	ret = drbr_enqueue(ifp, tx_ring->br, m);
	if (unlikely(ret != 0)) {
		taskqueue_enqueue(tx_ring->enqueue_tq, &tx_ring->enqueue_task);
		return (ret);
	}

	if (is_drbr_empty && (ENA_RING_MTX_TRYLOCK(tx_ring) != 0)) {
		ena_start_xmit(tx_ring);
		ENA_RING_MTX_UNLOCK(tx_ring);
	} else {
		taskqueue_enqueue(tx_ring->enqueue_tq, &tx_ring->enqueue_task);
	}

	return (0);
}

void
ena_qflush(if_t ifp)
{
	struct ena_adapter *adapter = ifp->if_softc;
	struct ena_ring *tx_ring = adapter->tx_ring;
	int i;

	for (i = 0; i < adapter->num_io_queues; ++i, ++tx_ring)
		if (!drbr_empty(ifp, tx_ring->br)) {
			ENA_RING_MTX_LOCK(tx_ring);
			drbr_flush(ifp, tx_ring->br);
			ENA_RING_MTX_UNLOCK(tx_ring);
		}

	if_qflush(ifp);
}

/*********************************************************************
 *  Static functions
 *********************************************************************/

static inline int
ena_get_tx_req_id(struct ena_ring *tx_ring, struct ena_com_io_cq *io_cq,
    uint16_t *req_id)
{
	struct ena_adapter *adapter = tx_ring->adapter;
	int rc;

	rc = ena_com_tx_comp_req_id_get(io_cq, req_id);
	if (rc == ENA_COM_TRY_AGAIN)
		return (EAGAIN);

	if (unlikely(rc != 0)) {
		ena_log(adapter->pdev, ERR, "Invalid req_id %hu in qid %hu\n",
		    *req_id, tx_ring->qid);
		counter_u64_add(tx_ring->tx_stats.bad_req_id, 1);
		goto err;
	}

	if (tx_ring->tx_buffer_info[*req_id].mbuf != NULL)
		return (0);

	ena_log(adapter->pdev, ERR,
	    "tx_info doesn't have valid mbuf. qid %hu req_id %hu\n",
	    tx_ring->qid, *req_id);
err:
	ena_trigger_reset(adapter, ENA_REGS_RESET_INV_TX_REQ_ID);

	return (EFAULT);
}

/**
 * ena_tx_cleanup - clear sent packets and corresponding descriptors
 * @tx_ring: ring for which we want to clean packets
 *
 * Once packets are sent, we ask the device in a loop for no longer used
 * descriptors. We find the related mbuf chain in a map (index in an array)
 * and free it, then update ring state.
 * This is performed in "endless" loop, updating ring pointers every
 * TX_COMMIT. The first check of free descriptor is performed before the actual
 * loop, then repeated at the loop end.
 **/
static int
ena_tx_cleanup(struct ena_ring *tx_ring)
{
	struct ena_adapter *adapter;
	struct ena_com_io_cq *io_cq;
	uint16_t next_to_clean;
	uint16_t req_id;
	uint16_t ena_qid;
	unsigned int total_done = 0;
	int rc;
	int commit = TX_COMMIT;
	int budget = TX_BUDGET;
	int work_done;
	bool above_thresh;

	adapter = tx_ring->que->adapter;
	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
	next_to_clean = tx_ring->next_to_clean;

#ifdef DEV_NETMAP
	if (netmap_tx_irq(adapter->ifp, tx_ring->qid) != NM_IRQ_PASS)
		return (0);
#endif /* DEV_NETMAP */

	do {
		struct ena_tx_buffer *tx_info;
		struct mbuf *mbuf;

		rc = ena_get_tx_req_id(tx_ring, io_cq, &req_id);
		if (unlikely(rc != 0))
			break;

		tx_info = &tx_ring->tx_buffer_info[req_id];

		mbuf = tx_info->mbuf;

		tx_info->mbuf = NULL;
		bintime_clear(&tx_info->timestamp);

		bus_dmamap_sync(adapter->tx_buf_tag, tx_info->dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(adapter->tx_buf_tag, tx_info->dmamap);

		ena_log_io(adapter->pdev, DBG, "tx: q %d mbuf %p completed\n",
		    tx_ring->qid, mbuf);

		m_freem(mbuf);

		total_done += tx_info->tx_descs;

		tx_ring->free_tx_ids[next_to_clean] = req_id;
		next_to_clean = ENA_TX_RING_IDX_NEXT(next_to_clean,
		    tx_ring->ring_size);

		if (unlikely(--commit == 0)) {
			commit = TX_COMMIT;
			/* update ring state every TX_COMMIT descriptor */
			tx_ring->next_to_clean = next_to_clean;
			ena_com_comp_ack(
			    &adapter->ena_dev->io_sq_queues[ena_qid],
			    total_done);
			ena_com_update_dev_comp_head(io_cq);
			total_done = 0;
		}
	} while (likely(--budget));

	work_done = TX_BUDGET - budget;

	ena_log_io(adapter->pdev, DBG, "tx: q %d done. total pkts: %d\n",
	    tx_ring->qid, work_done);

	/* If there is still something to commit update ring state */
	if (likely(commit != TX_COMMIT)) {
		tx_ring->next_to_clean = next_to_clean;
		ena_com_comp_ack(&adapter->ena_dev->io_sq_queues[ena_qid],
		    total_done);
		ena_com_update_dev_comp_head(io_cq);
	}

	/*
	 * Need to make the rings circular update visible to
	 * ena_xmit_mbuf() before checking for tx_ring->running.
	 */
	mb();

	above_thresh = ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
	    ENA_TX_RESUME_THRESH);
	if (unlikely(!tx_ring->running && above_thresh)) {
		ENA_RING_MTX_LOCK(tx_ring);
		above_thresh = ena_com_sq_have_enough_space(
		    tx_ring->ena_com_io_sq, ENA_TX_RESUME_THRESH);
		if (!tx_ring->running && above_thresh) {
			tx_ring->running = true;
			counter_u64_add(tx_ring->tx_stats.queue_wakeup, 1);
			taskqueue_enqueue(tx_ring->enqueue_tq,
			    &tx_ring->enqueue_task);
		}
		ENA_RING_MTX_UNLOCK(tx_ring);
	}

	tx_ring->tx_last_cleanup_ticks = ticks;

	return (work_done);
}

static void
ena_rx_hash_mbuf(struct ena_ring *rx_ring, struct ena_com_rx_ctx *ena_rx_ctx,
    struct mbuf *mbuf)
{
	struct ena_adapter *adapter = rx_ring->adapter;

	if (likely(ENA_FLAG_ISSET(ENA_FLAG_RSS_ACTIVE, adapter))) {
		mbuf->m_pkthdr.flowid = ena_rx_ctx->hash;

#ifdef RSS
		/*
		 * Hardware and software RSS are in agreement only when both are
		 * configured to Toeplitz algorithm.  This driver configures
		 * that algorithm only when software RSS is enabled and uses it.
		 */
		if (adapter->ena_dev->rss.hash_func != ENA_ADMIN_TOEPLITZ &&
		    ena_rx_ctx->l3_proto != ENA_ETH_IO_L3_PROTO_UNKNOWN) {
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
			return;
		}
#endif

		if (ena_rx_ctx->frag &&
		    (ena_rx_ctx->l3_proto != ENA_ETH_IO_L3_PROTO_UNKNOWN)) {
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
			return;
		}

		switch (ena_rx_ctx->l3_proto) {
		case ENA_ETH_IO_L3_PROTO_IPV4:
			switch (ena_rx_ctx->l4_proto) {
			case ENA_ETH_IO_L4_PROTO_TCP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV4);
				break;
			case ENA_ETH_IO_L4_PROTO_UDP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV4);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV4);
			}
			break;
		case ENA_ETH_IO_L3_PROTO_IPV6:
			switch (ena_rx_ctx->l4_proto) {
			case ENA_ETH_IO_L4_PROTO_TCP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_TCP_IPV6);
				break;
			case ENA_ETH_IO_L4_PROTO_UDP:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_UDP_IPV6);
				break;
			default:
				M_HASHTYPE_SET(mbuf, M_HASHTYPE_RSS_IPV6);
			}
			break;
		case ENA_ETH_IO_L3_PROTO_UNKNOWN:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_NONE);
			break;
		default:
			M_HASHTYPE_SET(mbuf, M_HASHTYPE_OPAQUE_HASH);
		}
	} else {
		mbuf->m_pkthdr.flowid = rx_ring->qid;
		M_HASHTYPE_SET(mbuf, M_HASHTYPE_NONE);
	}
}

/**
 * ena_rx_mbuf - assemble mbuf from descriptors
 * @rx_ring: ring for which we want to clean packets
 * @ena_bufs: buffer info
 * @ena_rx_ctx: metadata for this packet(s)
 * @next_to_clean: ring pointer, will be updated only upon success
 *
 **/
static struct mbuf *
ena_rx_mbuf(struct ena_ring *rx_ring, struct ena_com_rx_buf_info *ena_bufs,
    struct ena_com_rx_ctx *ena_rx_ctx, uint16_t *next_to_clean)
{
	struct mbuf *mbuf;
	struct ena_rx_buffer *rx_info;
	struct ena_adapter *adapter;
	device_t pdev;
	unsigned int descs = ena_rx_ctx->descs;
	uint16_t ntc, len, req_id, buf = 0;

	ntc = *next_to_clean;
	adapter = rx_ring->adapter;
	pdev = adapter->pdev;

	len = ena_bufs[buf].len;
	req_id = ena_bufs[buf].req_id;
	rx_info = &rx_ring->rx_buffer_info[req_id];
	if (unlikely(rx_info->mbuf == NULL)) {
		ena_log(pdev, ERR, "NULL mbuf in rx_info");
		return (NULL);
	}

	ena_log_io(pdev, DBG, "rx_info %p, mbuf %p, paddr %jx\n", rx_info,
	    rx_info->mbuf, (uintmax_t)rx_info->ena_buf.paddr);

	bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
	    BUS_DMASYNC_POSTREAD);
	mbuf = rx_info->mbuf;
	mbuf->m_flags |= M_PKTHDR;
	mbuf->m_pkthdr.len = len;
	mbuf->m_len = len;
	/* Only for the first segment the data starts at specific offset */
	mbuf->m_data = mtodo(mbuf, ena_rx_ctx->pkt_offset);
	ena_log_io(pdev, DBG, "Mbuf data offset=%u\n", ena_rx_ctx->pkt_offset);
	mbuf->m_pkthdr.rcvif = rx_ring->que->adapter->ifp;

	/* Fill mbuf with hash key and it's interpretation for optimization */
	ena_rx_hash_mbuf(rx_ring, ena_rx_ctx, mbuf);

	ena_log_io(pdev, DBG, "rx mbuf 0x%p, flags=0x%x, len: %d\n", mbuf,
	    mbuf->m_flags, mbuf->m_pkthdr.len);

	/* DMA address is not needed anymore, unmap it */
	bus_dmamap_unload(rx_ring->adapter->rx_buf_tag, rx_info->map);

	rx_info->mbuf = NULL;
	rx_ring->free_rx_ids[ntc] = req_id;
	ntc = ENA_RX_RING_IDX_NEXT(ntc, rx_ring->ring_size);

	/*
	 * While we have more than 1 descriptors for one rcvd packet, append
	 * other mbufs to the main one
	 */
	while (--descs) {
		++buf;
		len = ena_bufs[buf].len;
		req_id = ena_bufs[buf].req_id;
		rx_info = &rx_ring->rx_buffer_info[req_id];

		if (unlikely(rx_info->mbuf == NULL)) {
			ena_log(pdev, ERR, "NULL mbuf in rx_info");
			/*
			 * If one of the required mbufs was not allocated yet,
			 * we can break there.
			 * All earlier used descriptors will be reallocated
			 * later and not used mbufs can be reused.
			 * The next_to_clean pointer will not be updated in case
			 * of an error, so caller should advance it manually
			 * in error handling routine to keep it up to date
			 * with hw ring.
			 */
			m_freem(mbuf);
			return (NULL);
		}

		bus_dmamap_sync(adapter->rx_buf_tag, rx_info->map,
		    BUS_DMASYNC_POSTREAD);
		if (unlikely(m_append(mbuf, len, rx_info->mbuf->m_data) == 0)) {
			counter_u64_add(rx_ring->rx_stats.mbuf_alloc_fail, 1);
			ena_log_io(pdev, WARN, "Failed to append Rx mbuf %p\n",
			    mbuf);
		}

		ena_log_io(pdev, DBG, "rx mbuf updated. len %d\n",
		    mbuf->m_pkthdr.len);

		/* Free already appended mbuf, it won't be useful anymore */
		bus_dmamap_unload(rx_ring->adapter->rx_buf_tag, rx_info->map);
		m_freem(rx_info->mbuf);
		rx_info->mbuf = NULL;

		rx_ring->free_rx_ids[ntc] = req_id;
		ntc = ENA_RX_RING_IDX_NEXT(ntc, rx_ring->ring_size);
	}

	*next_to_clean = ntc;

	return (mbuf);
}

/**
 * ena_rx_checksum - indicate in mbuf if hw indicated a good cksum
 **/
static inline void
ena_rx_checksum(struct ena_ring *rx_ring, struct ena_com_rx_ctx *ena_rx_ctx,
    struct mbuf *mbuf)
{
	device_t pdev = rx_ring->adapter->pdev;

	/* if IP and error */
	if (unlikely((ena_rx_ctx->l3_proto == ENA_ETH_IO_L3_PROTO_IPV4) &&
	    ena_rx_ctx->l3_csum_err)) {
		/* ipv4 checksum error */
		mbuf->m_pkthdr.csum_flags = 0;
		counter_u64_add(rx_ring->rx_stats.csum_bad, 1);
		ena_log_io(pdev, DBG, "RX IPv4 header checksum error\n");
		return;
	}

	/* if TCP/UDP */
	if ((ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_TCP) ||
	    (ena_rx_ctx->l4_proto == ENA_ETH_IO_L4_PROTO_UDP)) {
		if (ena_rx_ctx->l4_csum_err) {
			/* TCP/UDP checksum error */
			mbuf->m_pkthdr.csum_flags = 0;
			counter_u64_add(rx_ring->rx_stats.csum_bad, 1);
			ena_log_io(pdev, DBG, "RX L4 checksum error\n");
		} else {
			mbuf->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mbuf->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			counter_u64_add(rx_ring->rx_stats.csum_good, 1);
		}
	}
}

/**
 * ena_rx_cleanup - handle rx irq
 * @arg: ring for which irq is being handled
 **/
static int
ena_rx_cleanup(struct ena_ring *rx_ring)
{
	struct ena_adapter *adapter;
	device_t pdev;
	struct mbuf *mbuf;
	struct ena_com_rx_ctx ena_rx_ctx;
	struct ena_com_io_cq *io_cq;
	struct ena_com_io_sq *io_sq;
	enum ena_regs_reset_reason_types reset_reason;
	if_t ifp;
	uint16_t ena_qid;
	uint16_t next_to_clean;
	uint32_t refill_required;
	uint32_t refill_threshold;
	uint32_t do_if_input = 0;
	unsigned int qid;
	int rc, i;
	int budget = RX_BUDGET;
#ifdef DEV_NETMAP
	int done;
#endif /* DEV_NETMAP */

	adapter = rx_ring->que->adapter;
	pdev = adapter->pdev;
	ifp = adapter->ifp;
	qid = rx_ring->que->id;
	ena_qid = ENA_IO_RXQ_IDX(qid);
	io_cq = &adapter->ena_dev->io_cq_queues[ena_qid];
	io_sq = &adapter->ena_dev->io_sq_queues[ena_qid];
	next_to_clean = rx_ring->next_to_clean;

#ifdef DEV_NETMAP
	if (netmap_rx_irq(adapter->ifp, rx_ring->qid, &done) != NM_IRQ_PASS)
		return (0);
#endif /* DEV_NETMAP */

	ena_log_io(pdev, DBG, "rx: qid %d\n", qid);

	do {
		ena_rx_ctx.ena_bufs = rx_ring->ena_bufs;
		ena_rx_ctx.max_bufs = adapter->max_rx_sgl_size;
		ena_rx_ctx.descs = 0;
		ena_rx_ctx.pkt_offset = 0;

		bus_dmamap_sync(io_cq->cdesc_addr.mem_handle.tag,
		    io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_POSTREAD);
		rc = ena_com_rx_pkt(io_cq, io_sq, &ena_rx_ctx);
		if (unlikely(rc != 0)) {
			if (rc == ENA_COM_NO_SPACE) {
				counter_u64_add(rx_ring->rx_stats.bad_desc_num,
				    1);
				reset_reason = ENA_REGS_RESET_TOO_MANY_RX_DESCS;
			} else {
				counter_u64_add(rx_ring->rx_stats.bad_req_id,
				    1);
				reset_reason = ENA_REGS_RESET_INV_RX_REQ_ID;
			}
			ena_trigger_reset(adapter, reset_reason);
			return (0);
		}

		if (unlikely(ena_rx_ctx.descs == 0))
			break;

		ena_log_io(pdev, DBG,
		    "rx: q %d got packet from ena. descs #: %d l3 proto %d l4 proto %d hash: %x\n",
		    rx_ring->qid, ena_rx_ctx.descs, ena_rx_ctx.l3_proto,
		    ena_rx_ctx.l4_proto, ena_rx_ctx.hash);

		/* Receive mbuf from the ring */
		mbuf = ena_rx_mbuf(rx_ring, rx_ring->ena_bufs, &ena_rx_ctx,
		    &next_to_clean);
		bus_dmamap_sync(io_cq->cdesc_addr.mem_handle.tag,
		    io_cq->cdesc_addr.mem_handle.map, BUS_DMASYNC_PREREAD);
		/* Exit if we failed to retrieve a buffer */
		if (unlikely(mbuf == NULL)) {
			for (i = 0; i < ena_rx_ctx.descs; ++i) {
				rx_ring->free_rx_ids[next_to_clean] =
				    rx_ring->ena_bufs[i].req_id;
				next_to_clean = ENA_RX_RING_IDX_NEXT(
				    next_to_clean, rx_ring->ring_size);
			}
			break;
		}

		if (((ifp->if_capenable & IFCAP_RXCSUM) != 0) ||
		    ((ifp->if_capenable & IFCAP_RXCSUM_IPV6) != 0)) {
			ena_rx_checksum(rx_ring, &ena_rx_ctx, mbuf);
		}

		counter_enter();
		counter_u64_add_protected(rx_ring->rx_stats.bytes,
		    mbuf->m_pkthdr.len);
		counter_u64_add_protected(adapter->hw_stats.rx_bytes,
		    mbuf->m_pkthdr.len);
		counter_exit();
		/*
		 * LRO is only for IP/TCP packets and TCP checksum of the packet
		 * should be computed by hardware.
		 */
		do_if_input = 1;
		if (((ifp->if_capenable & IFCAP_LRO) != 0) &&
		    ((mbuf->m_pkthdr.csum_flags & CSUM_IP_VALID) != 0) &&
		    (ena_rx_ctx.l4_proto == ENA_ETH_IO_L4_PROTO_TCP)) {
			/*
			 * Send to the stack if:
			 *  - LRO not enabled, or
			 *  - no LRO resources, or
			 *  - lro enqueue fails
			 */
			if ((rx_ring->lro.lro_cnt != 0) &&
			    (tcp_lro_rx(&rx_ring->lro, mbuf, 0) == 0))
				do_if_input = 0;
		}
		if (do_if_input != 0) {
			ena_log_io(pdev, DBG,
			    "calling if_input() with mbuf %p\n", mbuf);
			(*ifp->if_input)(ifp, mbuf);
		}

		counter_enter();
		counter_u64_add_protected(rx_ring->rx_stats.cnt, 1);
		counter_u64_add_protected(adapter->hw_stats.rx_packets, 1);
		counter_exit();
	} while (--budget);

	rx_ring->next_to_clean = next_to_clean;

	refill_required = ena_com_free_q_entries(io_sq);
	refill_threshold = min_t(int,
	    rx_ring->ring_size / ENA_RX_REFILL_THRESH_DIVIDER,
	    ENA_RX_REFILL_THRESH_PACKET);

	if (refill_required > refill_threshold) {
		ena_com_update_dev_comp_head(rx_ring->ena_com_io_cq);
		ena_refill_rx_bufs(rx_ring, refill_required);
	}

	tcp_lro_flush_all(&rx_ring->lro);

	return (RX_BUDGET - budget);
}

static void
ena_tx_csum(struct ena_com_tx_ctx *ena_tx_ctx, struct mbuf *mbuf,
    bool disable_meta_caching)
{
	struct ena_com_tx_meta *ena_meta;
	struct ether_vlan_header *eh;
	struct mbuf *mbuf_next;
	u32 mss;
	bool offload;
	uint16_t etype;
	int ehdrlen;
	struct ip *ip;
	int ipproto;
	int iphlen;
	struct tcphdr *th;
	int offset;

	offload = false;
	ena_meta = &ena_tx_ctx->ena_meta;
	mss = mbuf->m_pkthdr.tso_segsz;

	if (mss != 0)
		offload = true;

	if ((mbuf->m_pkthdr.csum_flags & CSUM_TSO) != 0)
		offload = true;

	if ((mbuf->m_pkthdr.csum_flags & CSUM_OFFLOAD) != 0)
		offload = true;

	if ((mbuf->m_pkthdr.csum_flags & CSUM6_OFFLOAD) != 0)
		offload = true;

	if (!offload) {
		if (disable_meta_caching) {
			memset(ena_meta, 0, sizeof(*ena_meta));
			ena_tx_ctx->meta_valid = 1;
		} else {
			ena_tx_ctx->meta_valid = 0;
		}
		return;
	}

	/* Determine where frame payload starts. */
	eh = mtod(mbuf, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	mbuf_next = m_getptr(mbuf, ehdrlen, &offset);

	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(mtodo(mbuf_next, offset));
		iphlen = ip->ip_hl << 2;
		ipproto = ip->ip_p;
		ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV4;
		if ((ip->ip_off & htons(IP_DF)) != 0)
			ena_tx_ctx->df = 1;
		break;
	case ETHERTYPE_IPV6:
		ena_tx_ctx->l3_proto = ENA_ETH_IO_L3_PROTO_IPV6;
		iphlen = ip6_lasthdr(mbuf, ehdrlen, IPPROTO_IPV6, &ipproto);
		iphlen -= ehdrlen;
		ena_tx_ctx->df = 1;
		break;
	default:
		iphlen = 0;
		ipproto = 0;
		break;
	}

	mbuf_next = m_getptr(mbuf, iphlen + ehdrlen, &offset);
	th = (struct tcphdr *)(mtodo(mbuf_next, offset));

	if ((mbuf->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		ena_tx_ctx->l3_csum_enable = 1;
	}
	if ((mbuf->m_pkthdr.csum_flags & CSUM_TSO) != 0) {
		ena_tx_ctx->tso_enable = 1;
		ena_meta->l4_hdr_len = (th->th_off);
	}

	if (ipproto == IPPROTO_TCP) {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_TCP;
		if ((mbuf->m_pkthdr.csum_flags &
		    (CSUM_IP_TCP | CSUM_IP6_TCP)) != 0)
			ena_tx_ctx->l4_csum_enable = 1;
		else
			ena_tx_ctx->l4_csum_enable = 0;
	} else if (ipproto == IPPROTO_UDP) {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UDP;
		if ((mbuf->m_pkthdr.csum_flags &
		    (CSUM_IP_UDP | CSUM_IP6_UDP)) != 0)
			ena_tx_ctx->l4_csum_enable = 1;
		else
			ena_tx_ctx->l4_csum_enable = 0;
	} else {
		ena_tx_ctx->l4_proto = ENA_ETH_IO_L4_PROTO_UNKNOWN;
		ena_tx_ctx->l4_csum_enable = 0;
	}

	ena_meta->mss = mss;
	ena_meta->l3_hdr_len = iphlen;
	ena_meta->l3_hdr_offset = ehdrlen;
	ena_tx_ctx->meta_valid = 1;
}

static int
ena_check_and_collapse_mbuf(struct ena_ring *tx_ring, struct mbuf **mbuf)
{
	struct ena_adapter *adapter;
	struct mbuf *collapsed_mbuf;
	int num_frags;

	adapter = tx_ring->adapter;
	num_frags = ena_mbuf_count(*mbuf);

	/* One segment must be reserved for configuration descriptor. */
	if (num_frags < adapter->max_tx_sgl_size)
		return (0);

	if ((num_frags == adapter->max_tx_sgl_size) &&
	    ((*mbuf)->m_pkthdr.len < tx_ring->tx_max_header_size))
		return (0);

	counter_u64_add(tx_ring->tx_stats.collapse, 1);

	collapsed_mbuf = m_collapse(*mbuf, M_NOWAIT,
	    adapter->max_tx_sgl_size - 1);
	if (unlikely(collapsed_mbuf == NULL)) {
		counter_u64_add(tx_ring->tx_stats.collapse_err, 1);
		return (ENOMEM);
	}

	/* If mbuf was collapsed succesfully, original mbuf is released. */
	*mbuf = collapsed_mbuf;

	return (0);
}

static int
ena_tx_map_mbuf(struct ena_ring *tx_ring, struct ena_tx_buffer *tx_info,
    struct mbuf *mbuf, void **push_hdr, u16 *header_len)
{
	struct ena_adapter *adapter = tx_ring->adapter;
	struct ena_com_buf *ena_buf;
	bus_dma_segment_t segs[ENA_BUS_DMA_SEGS];
	size_t iseg = 0;
	uint32_t mbuf_head_len;
	uint16_t offset;
	int rc, nsegs;

	mbuf_head_len = mbuf->m_len;
	tx_info->mbuf = mbuf;
	ena_buf = tx_info->bufs;

	/*
	 * For easier maintaining of the DMA map, map the whole mbuf even if
	 * the LLQ is used. The descriptors will be filled using the segments.
	 */
	rc = bus_dmamap_load_mbuf_sg(adapter->tx_buf_tag,
	    tx_info->dmamap, mbuf, segs, &nsegs, BUS_DMA_NOWAIT);
	if (unlikely((rc != 0) || (nsegs == 0))) {
		ena_log_io(adapter->pdev, WARN,
		    "dmamap load failed! err: %d nsegs: %d\n", rc, nsegs);
		goto dma_error;
	}

	if (tx_ring->tx_mem_queue_type == ENA_ADMIN_PLACEMENT_POLICY_DEV) {
		/*
		 * When the device is LLQ mode, the driver will copy
		 * the header into the device memory space.
		 * the ena_com layer assumes the header is in a linear
		 * memory space.
		 * This assumption might be wrong since part of the header
		 * can be in the fragmented buffers.
		 * First check if header fits in the mbuf. If not, copy it to
		 * separate buffer that will be holding linearized data.
		 */
		*header_len = min_t(uint32_t, mbuf->m_pkthdr.len,
		    tx_ring->tx_max_header_size);

		/* If header is in linear space, just point into mbuf's data. */
		if (likely(*header_len <= mbuf_head_len)) {
			*push_hdr = mbuf->m_data;
		/*
		 * Otherwise, copy whole portion of header from multiple
		 * mbufs to intermediate buffer.
		 */
		} else {
			m_copydata(mbuf, 0, *header_len,
			    tx_ring->push_buf_intermediate_buf);
			*push_hdr = tx_ring->push_buf_intermediate_buf;

			counter_u64_add(tx_ring->tx_stats.llq_buffer_copy, 1);
		}

		ena_log_io(adapter->pdev, DBG,
		    "mbuf: %p header_buf->vaddr: %p push_len: %d\n",
		    mbuf, *push_hdr, *header_len);

		/* If packet is fitted in LLQ header, no need for DMA segments. */
		if (mbuf->m_pkthdr.len <= tx_ring->tx_max_header_size) {
			return (0);
		} else {
			offset = tx_ring->tx_max_header_size;
			/*
			 * As Header part is mapped to LLQ header, we can skip
			 * it and just map the residuum of the mbuf to DMA
			 * Segments.
			 */
			while (offset > 0) {
				if (offset >= segs[iseg].ds_len) {
					offset -= segs[iseg].ds_len;
				} else {
					ena_buf->paddr = segs[iseg].ds_addr +
					    offset;
					ena_buf->len = segs[iseg].ds_len -
					    offset;
					ena_buf++;
					tx_info->num_of_bufs++;
					offset = 0;
				}
				iseg++;
			}
		}
	} else {
		*push_hdr = NULL;
		/*
		 * header_len is just a hint for the device. Because FreeBSD is
		 * not giving us information about packet header length and it
		 * is not guaranteed that all packet headers will be in the 1st
		 * mbuf, setting header_len to 0 is making the device ignore
		 * this value and resolve header on it's own.
		 */
		*header_len = 0;
	}

	/* Map rest of the mbuf */
	while (iseg < nsegs) {
		ena_buf->paddr = segs[iseg].ds_addr;
		ena_buf->len = segs[iseg].ds_len;
		ena_buf++;
		iseg++;
		tx_info->num_of_bufs++;
	}

	return (0);

dma_error:
	counter_u64_add(tx_ring->tx_stats.dma_mapping_err, 1);
	tx_info->mbuf = NULL;
	return (rc);
}

static int
ena_xmit_mbuf(struct ena_ring *tx_ring, struct mbuf **mbuf)
{
	struct ena_adapter *adapter;
	device_t pdev;
	struct ena_tx_buffer *tx_info;
	struct ena_com_tx_ctx ena_tx_ctx;
	struct ena_com_dev *ena_dev;
	struct ena_com_io_sq *io_sq;
	void *push_hdr;
	uint16_t next_to_use;
	uint16_t req_id;
	uint16_t ena_qid;
	uint16_t header_len;
	int rc;
	int nb_hw_desc;

	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);
	adapter = tx_ring->que->adapter;
	pdev = adapter->pdev;
	ena_dev = adapter->ena_dev;
	io_sq = &ena_dev->io_sq_queues[ena_qid];

	rc = ena_check_and_collapse_mbuf(tx_ring, mbuf);
	if (unlikely(rc != 0)) {
		ena_log_io(pdev, WARN, "Failed to collapse mbuf! err: %d\n",
		    rc);
		return (rc);
	}

	ena_log_io(pdev, DBG, "Tx: %d bytes\n", (*mbuf)->m_pkthdr.len);

	next_to_use = tx_ring->next_to_use;
	req_id = tx_ring->free_tx_ids[next_to_use];
	tx_info = &tx_ring->tx_buffer_info[req_id];
	tx_info->num_of_bufs = 0;

	ENA_WARN(tx_info->mbuf != NULL, adapter->ena_dev,
	    "mbuf isn't NULL for req_id %d\n", req_id);

	rc = ena_tx_map_mbuf(tx_ring, tx_info, *mbuf, &push_hdr, &header_len);
	if (unlikely(rc != 0)) {
		ena_log_io(pdev, WARN, "Failed to map TX mbuf\n");
		return (rc);
	}
	memset(&ena_tx_ctx, 0x0, sizeof(struct ena_com_tx_ctx));
	ena_tx_ctx.ena_bufs = tx_info->bufs;
	ena_tx_ctx.push_header = push_hdr;
	ena_tx_ctx.num_bufs = tx_info->num_of_bufs;
	ena_tx_ctx.req_id = req_id;
	ena_tx_ctx.header_len = header_len;

	/* Set flags and meta data */
	ena_tx_csum(&ena_tx_ctx, *mbuf, adapter->disable_meta_caching);

	if (tx_ring->acum_pkts == DB_THRESHOLD ||
	    ena_com_is_doorbell_needed(tx_ring->ena_com_io_sq, &ena_tx_ctx)) {
		ena_log_io(pdev, DBG,
		    "llq tx max burst size of queue %d achieved, writing doorbell to send burst\n",
		    tx_ring->que->id);
		ena_ring_tx_doorbell(tx_ring);
	}

	/* Prepare the packet's descriptors and send them to device */
	rc = ena_com_prepare_tx(io_sq, &ena_tx_ctx, &nb_hw_desc);
	if (unlikely(rc != 0)) {
		if (likely(rc == ENA_COM_NO_MEM)) {
			ena_log_io(pdev, DBG, "tx ring[%d] is out of space\n",
			    tx_ring->que->id);
		} else {
			ena_log(pdev, ERR, "failed to prepare tx bufs\n");
			ena_trigger_reset(adapter,
			    ENA_REGS_RESET_DRIVER_INVALID_STATE);
		}
		counter_u64_add(tx_ring->tx_stats.prepare_ctx_err, 1);
		goto dma_error;
	}

	counter_enter();
	counter_u64_add_protected(tx_ring->tx_stats.cnt, 1);
	counter_u64_add_protected(tx_ring->tx_stats.bytes,
	    (*mbuf)->m_pkthdr.len);

	counter_u64_add_protected(adapter->hw_stats.tx_packets, 1);
	counter_u64_add_protected(adapter->hw_stats.tx_bytes,
	    (*mbuf)->m_pkthdr.len);
	counter_exit();

	tx_info->tx_descs = nb_hw_desc;
	getbinuptime(&tx_info->timestamp);
	tx_info->print_once = true;

	tx_ring->next_to_use = ENA_TX_RING_IDX_NEXT(next_to_use,
	    tx_ring->ring_size);

	/* stop the queue when no more space available, the packet can have up
	 * to sgl_size + 2. one for the meta descriptor and one for header
	 * (if the header is larger than tx_max_header_size).
	 */
	if (unlikely(!ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
	    adapter->max_tx_sgl_size + 2))) {
		ena_log_io(pdev, DBG, "Stop queue %d\n", tx_ring->que->id);

		tx_ring->running = false;
		counter_u64_add(tx_ring->tx_stats.queue_stop, 1);

		/* There is a rare condition where this function decides to
		 * stop the queue but meanwhile tx_cleanup() updates
		 * next_to_completion and terminates.
		 * The queue will remain stopped forever.
		 * To solve this issue this function performs mb(), checks
		 * the wakeup condition and wakes up the queue if needed.
		 */
		mb();

		if (ena_com_sq_have_enough_space(tx_ring->ena_com_io_sq,
		    ENA_TX_RESUME_THRESH)) {
			tx_ring->running = true;
			counter_u64_add(tx_ring->tx_stats.queue_wakeup, 1);
		}
	}

	bus_dmamap_sync(adapter->tx_buf_tag, tx_info->dmamap,
	    BUS_DMASYNC_PREWRITE);

	return (0);

dma_error:
	tx_info->mbuf = NULL;
	bus_dmamap_unload(adapter->tx_buf_tag, tx_info->dmamap);

	return (rc);
}

static void
ena_start_xmit(struct ena_ring *tx_ring)
{
	struct mbuf *mbuf;
	struct ena_adapter *adapter = tx_ring->adapter;
	int ena_qid;
	int ret = 0;

	ENA_RING_MTX_ASSERT(tx_ring);

	if (unlikely((if_getdrvflags(adapter->ifp) & IFF_DRV_RUNNING) == 0))
		return;

	if (unlikely(!ENA_FLAG_ISSET(ENA_FLAG_LINK_UP, adapter)))
		return;

	ena_qid = ENA_IO_TXQ_IDX(tx_ring->que->id);

	while ((mbuf = drbr_peek(adapter->ifp, tx_ring->br)) != NULL) {
		ena_log_io(adapter->pdev, DBG,
		    "\ndequeued mbuf %p with flags %#x and header csum flags %#jx\n",
		    mbuf, mbuf->m_flags, (uint64_t)mbuf->m_pkthdr.csum_flags);

		if (unlikely(!tx_ring->running)) {
			drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			break;
		}

		if (unlikely((ret = ena_xmit_mbuf(tx_ring, &mbuf)) != 0)) {
			if (ret == ENA_COM_NO_MEM) {
				drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			} else if (ret == ENA_COM_NO_SPACE) {
				drbr_putback(adapter->ifp, tx_ring->br, mbuf);
			} else {
				m_freem(mbuf);
				drbr_advance(adapter->ifp, tx_ring->br);
			}

			break;
		}

		drbr_advance(adapter->ifp, tx_ring->br);

		if (unlikely((if_getdrvflags(adapter->ifp) & IFF_DRV_RUNNING) == 0))
			return;

		tx_ring->acum_pkts++;

		BPF_MTAP(adapter->ifp, mbuf);
	}

	if (likely(tx_ring->acum_pkts != 0)) {
		/* Trigger the dma engine */
		ena_ring_tx_doorbell(tx_ring);
	}

	if (unlikely(!tx_ring->running))
		taskqueue_enqueue(tx_ring->que->cleanup_tq,
		    &tx_ring->que->cleanup_task);
}
