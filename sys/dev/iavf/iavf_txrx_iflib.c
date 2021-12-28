/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

/**
 * @file iavf_txrx_iflib.c
 * @brief Tx/Rx hotpath implementation for the iflib driver
 *
 * Contains functions used to implement the Tx and Rx hotpaths of the iflib
 * driver implementation.
 */
#include "iavf_iflib.h"
#include "iavf_txrx_common.h"

#ifdef RSS
#include <net/rss_config.h>
#endif

/* Local Prototypes */
static void	iavf_rx_checksum(if_rxd_info_t ri, u32 status, u32 error, u8 ptype);

static int	iavf_isc_txd_encap(void *arg, if_pkt_info_t pi);
static void	iavf_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx);
static int	iavf_isc_txd_credits_update_hwb(void *arg, uint16_t txqid, bool clear);
static int	iavf_isc_txd_credits_update_dwb(void *arg, uint16_t txqid, bool clear);

static void	iavf_isc_rxd_refill(void *arg, if_rxd_update_t iru);
static void	iavf_isc_rxd_flush(void *arg, uint16_t rxqid, uint8_t flid __unused,
				  qidx_t pidx);
static int	iavf_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx,
				      qidx_t budget);
static int	iavf_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri);

/**
 * @var iavf_txrx_hwb
 * @brief iflib Tx/Rx operations for head write back
 *
 * iflib ops structure for when operating the device in head write back mode.
 */
struct if_txrx iavf_txrx_hwb = {
	iavf_isc_txd_encap,
	iavf_isc_txd_flush,
	iavf_isc_txd_credits_update_hwb,
	iavf_isc_rxd_available,
	iavf_isc_rxd_pkt_get,
	iavf_isc_rxd_refill,
	iavf_isc_rxd_flush,
	NULL
};

/**
 * @var iavf_txrx_dwb
 * @brief iflib Tx/Rx operations for descriptor write back
 *
 * iflib ops structure for when operating the device in descriptor write back
 * mode.
 */
struct if_txrx iavf_txrx_dwb = {
	iavf_isc_txd_encap,
	iavf_isc_txd_flush,
	iavf_isc_txd_credits_update_dwb,
	iavf_isc_rxd_available,
	iavf_isc_rxd_pkt_get,
	iavf_isc_rxd_refill,
	iavf_isc_rxd_flush,
	NULL
};

/**
 * iavf_is_tx_desc_done - Check if a Tx descriptor is ready
 * @txr: the Tx ring to check in
 * @idx: ring index to check
 *
 * @returns true if the descriptor has been written back by hardware, and
 * false otherwise.
 */
static bool
iavf_is_tx_desc_done(struct tx_ring *txr, int idx)
{
	return (((txr->tx_base[idx].cmd_type_offset_bsz >> IAVF_TXD_QW1_DTYPE_SHIFT)
	    & IAVF_TXD_QW1_DTYPE_MASK) == IAVF_TX_DESC_DTYPE_DESC_DONE);
}


/**
 * iavf_tso_detect_sparse - detect TSO packets with too many segments
 * @segs: packet segments array
 * @nsegs: number of packet segments
 * @pi: packet information
 *
 * Hardware only transmits packets with a maximum of 8 descriptors. For TSO
 * packets, hardware needs to be able to build the split packets using 8 or
 * fewer descriptors. Additionally, the header must be contained within at
 * most 3 descriptors.
 *
 * To verify this, we walk the headers to find out how many descriptors the
 * headers require (usually 1). Then we ensure that, for each TSO segment, its
 * data plus the headers are contained within 8 or fewer descriptors.
 *
 * @returns zero if the packet is valid, one otherwise.
 */
static int
iavf_tso_detect_sparse(bus_dma_segment_t *segs, int nsegs, if_pkt_info_t pi)
{
	int	count, curseg, i, hlen, segsz, seglen, tsolen;

	if (nsegs <= IAVF_MAX_TX_SEGS-2)
		return (0);
	segsz = pi->ipi_tso_segsz;
	curseg = count = 0;

	hlen = pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen;
	tsolen = pi->ipi_len - hlen;

	i = 0;
	curseg = segs[0].ds_len;
	while (hlen > 0) {
		count++;
		if (count > IAVF_MAX_TX_SEGS - 2)
			return (1);
		if (curseg == 0) {
			i++;
			if (__predict_false(i == nsegs))
				return (1);

			curseg = segs[i].ds_len;
		}
		seglen = min(curseg, hlen);
		curseg -= seglen;
		hlen -= seglen;
	}
	while (tsolen > 0) {
		segsz = pi->ipi_tso_segsz;
		while (segsz > 0 && tsolen != 0) {
			count++;
			if (count > IAVF_MAX_TX_SEGS - 2) {
				return (1);
			}
			if (curseg == 0) {
				i++;
				if (__predict_false(i == nsegs)) {
					return (1);
				}
				curseg = segs[i].ds_len;
			}
			seglen = min(curseg, segsz);
			segsz -= seglen;
			curseg -= seglen;
			tsolen -= seglen;
		}
		count = 0;
	}

	return (0);
}

/**
 * iavf_tx_setup_offload - Setup Tx offload parameters
 * @que: pointer to the Tx queue
 * @pi: Tx packet info
 * @cmd: pointer to command descriptor value
 * @off: pointer to offset descriptor value
 *
 * Based on packet type and Tx offloads requested, sets up the command and
 * offset values for a Tx descriptor to enable the requested offloads.
 */
static void
iavf_tx_setup_offload(struct iavf_tx_queue *que __unused,
    if_pkt_info_t pi, u32 *cmd, u32 *off)
{
	switch (pi->ipi_etype) {
#ifdef INET
		case ETHERTYPE_IP:
			if (pi->ipi_csum_flags & IAVF_CSUM_IPV4)
				*cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM;
			else
				*cmd |= IAVF_TX_DESC_CMD_IIPT_IPV4;
			break;
#endif
#ifdef INET6
		case ETHERTYPE_IPV6:
			*cmd |= IAVF_TX_DESC_CMD_IIPT_IPV6;
			break;
#endif
		default:
			break;
	}

	*off |= (pi->ipi_ehdrlen >> 1) << IAVF_TX_DESC_LENGTH_MACLEN_SHIFT;
	*off |= (pi->ipi_ip_hlen >> 2) << IAVF_TX_DESC_LENGTH_IPLEN_SHIFT;

	switch (pi->ipi_ipproto) {
		case IPPROTO_TCP:
			if (pi->ipi_csum_flags & IAVF_CSUM_TCP) {
				*cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_TCP;
				*off |= (pi->ipi_tcp_hlen >> 2) <<
				    IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
				/* Check for NO_HEAD MDD event */
				MPASS(pi->ipi_tcp_hlen != 0);
			}
			break;
		case IPPROTO_UDP:
			if (pi->ipi_csum_flags & IAVF_CSUM_UDP) {
				*cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_UDP;
				*off |= (sizeof(struct udphdr) >> 2) <<
				    IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
			}
			break;
		case IPPROTO_SCTP:
			if (pi->ipi_csum_flags & IAVF_CSUM_SCTP) {
				*cmd |= IAVF_TX_DESC_CMD_L4T_EOFT_SCTP;
				*off |= (sizeof(struct sctphdr) >> 2) <<
				    IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT;
			}
			/* Fall Thru */
		default:
			break;
	}
}

/**
 * iavf_tso_setup - Setup TSO context descriptor
 * @txr: the Tx ring to process
 * @pi: packet info structure
 *
 * Enable hardware segmentation offload (TSO) for a given packet by creating
 * a context descriptor with the necessary details for offloading.
 *
 * @returns the new ring index to use for the data descriptor.
 */
static int
iavf_tso_setup(struct tx_ring *txr, if_pkt_info_t pi)
{
	if_softc_ctx_t			scctx;
	struct iavf_tx_context_desc	*TXD;
	u32				cmd, mss, type, tsolen;
	int				idx, total_hdr_len;
	u64				type_cmd_tso_mss;

	idx = pi->ipi_pidx;
	TXD = (struct iavf_tx_context_desc *) &txr->tx_base[idx];
	total_hdr_len = pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen;
	tsolen = pi->ipi_len - total_hdr_len;
	scctx = txr->que->vsi->shared;

	type = IAVF_TX_DESC_DTYPE_CONTEXT;
	cmd = IAVF_TX_CTX_DESC_TSO;
	/*
	 * TSO MSS must not be less than 64; this prevents a
	 * BAD_LSO_MSS MDD event when the MSS is too small.
	 */
	if (pi->ipi_tso_segsz < IAVF_MIN_TSO_MSS) {
		txr->mss_too_small++;
		pi->ipi_tso_segsz = IAVF_MIN_TSO_MSS;
	}
	mss = pi->ipi_tso_segsz;

	/* Check for BAD_LS0_MSS MDD event (mss too large) */
	MPASS(mss <= IAVF_MAX_TSO_MSS);
	/* Check for NO_HEAD MDD event (header lengths are 0) */
	MPASS(pi->ipi_ehdrlen != 0);
	MPASS(pi->ipi_ip_hlen != 0);
	/* Partial check for BAD_LSO_LEN MDD event */
	MPASS(tsolen != 0);
	/* Partial check for WRONG_SIZE MDD event (during TSO) */
	MPASS(total_hdr_len + mss <= IAVF_MAX_FRAME);

	type_cmd_tso_mss = ((u64)type << IAVF_TXD_CTX_QW1_DTYPE_SHIFT) |
	    ((u64)cmd << IAVF_TXD_CTX_QW1_CMD_SHIFT) |
	    ((u64)tsolen << IAVF_TXD_CTX_QW1_TSO_LEN_SHIFT) |
	    ((u64)mss << IAVF_TXD_CTX_QW1_MSS_SHIFT);
	TXD->type_cmd_tso_mss = htole64(type_cmd_tso_mss);

	TXD->tunneling_params = htole32(0);
	txr->que->tso++;

	return ((idx + 1) & (scctx->isc_ntxd[0]-1));
}

#define IAVF_TXD_CMD (IAVF_TX_DESC_CMD_EOP | IAVF_TX_DESC_CMD_RS)

/**
 * iavf_isc_txd_encap - Encapsulate a Tx packet into descriptors
 * @arg: void pointer to the VSI structure
 * @pi: packet info to encapsulate
 *
 * This routine maps the mbufs to tx descriptors, allowing the
 * TX engine to transmit the packets.
 *
 * @returns 0 on success, positive on failure
 */
static int
iavf_isc_txd_encap(void *arg, if_pkt_info_t pi)
{
	struct iavf_vsi		*vsi = arg;
	if_softc_ctx_t		scctx = vsi->shared;
	struct iavf_tx_queue	*que = &vsi->tx_queues[pi->ipi_qsidx];
	struct tx_ring		*txr = &que->txr;
	int			nsegs = pi->ipi_nsegs;
	bus_dma_segment_t *segs = pi->ipi_segs;
	struct iavf_tx_desc	*txd = NULL;
	int			i, j, mask, pidx_last;
	u32			cmd, off, tx_intr;

	if (__predict_false(pi->ipi_len < IAVF_MIN_FRAME)) {
		que->pkt_too_small++;
		return (EINVAL);
	}

	cmd = off = 0;
	i = pi->ipi_pidx;

	tx_intr = (pi->ipi_flags & IPI_TX_INTR);

	/* Set up the TSO/CSUM offload */
	if (pi->ipi_csum_flags & CSUM_OFFLOAD) {
		/* Set up the TSO context descriptor if required */
		if (pi->ipi_csum_flags & CSUM_TSO) {
			/* Prevent MAX_BUFF MDD event (for TSO) */
			if (iavf_tso_detect_sparse(segs, nsegs, pi))
				return (EFBIG);
			i = iavf_tso_setup(txr, pi);
		}
		iavf_tx_setup_offload(que, pi, &cmd, &off);
	}
	if (pi->ipi_mflags & M_VLANTAG)
		cmd |= IAVF_TX_DESC_CMD_IL2TAG1;

	cmd |= IAVF_TX_DESC_CMD_ICRC;
	mask = scctx->isc_ntxd[0] - 1;
	/* Check for WRONG_SIZE MDD event */
	MPASS(pi->ipi_len >= IAVF_MIN_FRAME);
#ifdef INVARIANTS
	if (!(pi->ipi_csum_flags & CSUM_TSO))
		MPASS(pi->ipi_len <= IAVF_MAX_FRAME);
#endif
	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;

		txd = &txr->tx_base[i];
		seglen = segs[j].ds_len;

		/* Check for ZERO_BSIZE MDD event */
		MPASS(seglen != 0);

		txd->buffer_addr = htole64(segs[j].ds_addr);
		txd->cmd_type_offset_bsz =
		    htole64(IAVF_TX_DESC_DTYPE_DATA
		    | ((u64)cmd  << IAVF_TXD_QW1_CMD_SHIFT)
		    | ((u64)off << IAVF_TXD_QW1_OFFSET_SHIFT)
		    | ((u64)seglen  << IAVF_TXD_QW1_TX_BUF_SZ_SHIFT)
	            | ((u64)htole16(pi->ipi_vtag) << IAVF_TXD_QW1_L2TAG1_SHIFT));

		txr->tx_bytes += seglen;
		pidx_last = i;
		i = (i+1) & mask;
	}
	/* Set the last descriptor for report */
	txd->cmd_type_offset_bsz |=
	    htole64(((u64)IAVF_TXD_CMD << IAVF_TXD_QW1_CMD_SHIFT));
	/* Add to report status array (if using TX interrupts) */
	if (!vsi->enable_head_writeback && tx_intr) {
		txr->tx_rsq[txr->tx_rs_pidx] = pidx_last;
		txr->tx_rs_pidx = (txr->tx_rs_pidx+1) & mask;
		MPASS(txr->tx_rs_pidx != txr->tx_rs_cidx);
	}
	pi->ipi_new_pidx = i;

	++txr->tx_packets;
	return (0);
}

/**
 * iavf_isc_txd_flush - Flush Tx ring
 * @arg: void pointer to the VSI
 * @txqid: the Tx queue to flush
 * @pidx: the ring index to flush to
 *
 * Advance the Transmit Descriptor Tail (Tdt), this tells the
 * hardware that this frame is available to transmit.
 */
static void
iavf_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx)
{
	struct iavf_vsi *vsi = arg;
	struct tx_ring *txr = &vsi->tx_queues[txqid].txr;

	/* Check for ENDLESS_TX MDD event */
	MPASS(pidx < vsi->shared->isc_ntxd[0]);
	wr32(vsi->hw, txr->tail, pidx);
}

/**
 * iavf_init_tx_ring - Initialize queue Tx ring
 * @vsi: pointer to the VSI
 * @que: pointer to queue to initialize
 *
 * (Re)Initialize a queue transmit ring by clearing its memory.
 */
void
iavf_init_tx_ring(struct iavf_vsi *vsi, struct iavf_tx_queue *que)
{
	struct tx_ring *txr = &que->txr;

	/* Clear the old ring contents */
	bzero((void *)txr->tx_base,
	      (sizeof(struct iavf_tx_desc)) *
	      (vsi->shared->isc_ntxd[0] + (vsi->enable_head_writeback ? 1 : 0)));

	wr32(vsi->hw, txr->tail, 0);
}

/**
 * iavf_get_tx_head - Get the index of the head of a ring
 * @que: queue to read
 *
 * Retrieve the value from the location the HW records its HEAD index
 *
 * @returns the index of the HW head of the Tx queue
 */
static inline u32
iavf_get_tx_head(struct iavf_tx_queue *que)
{
	if_softc_ctx_t          scctx = que->vsi->shared;
	struct tx_ring  *txr = &que->txr;
	void *head = &txr->tx_base[scctx->isc_ntxd[0]];

	return LE32_TO_CPU(*(volatile __le32 *)head);
}

/**
 * iavf_isc_txd_credits_update_hwb - Update Tx ring credits
 * @arg: void pointer to the VSI
 * @qid: the queue id to update
 * @clear: whether to update or only report current status
 *
 * Checks the number of packets in the queue that could be cleaned up.
 *
 * if clear is true, the iflib stack has cleaned the packets and is
 * notifying the driver to update its processed ring pointer.
 *
 * @returns the number of packets in the ring that can be cleaned.
 *
 * @remark this function is intended for the head write back mode.
 */
static int
iavf_isc_txd_credits_update_hwb(void *arg, uint16_t qid, bool clear)
{
	struct iavf_vsi          *vsi = arg;
	if_softc_ctx_t          scctx = vsi->shared;
	struct iavf_tx_queue     *que = &vsi->tx_queues[qid];
	struct tx_ring		*txr = &que->txr;
	int			 head, credits;

	/* Get the Head WB value */
	head = iavf_get_tx_head(que);

	credits = head - txr->tx_cidx_processed;
	if (credits < 0)
		credits += scctx->isc_ntxd[0];
	if (clear)
		txr->tx_cidx_processed = head;

	return (credits);
}

/**
 * iavf_isc_txd_credits_update_dwb - Update Tx ring credits
 * @arg: void pointer to the VSI
 * @txqid: the queue id to update
 * @clear: whether to update or only report current status
 *
 * Checks the number of packets in the queue that could be cleaned up.
 *
 * if clear is true, the iflib stack has cleaned the packets and is
 * notifying the driver to update its processed ring pointer.
 *
 * @returns the number of packets in the ring that can be cleaned.
 *
 * @remark this function is intended for the descriptor write back mode.
 */
static int
iavf_isc_txd_credits_update_dwb(void *arg, uint16_t txqid, bool clear)
{
	struct iavf_vsi *vsi = arg;
	struct iavf_tx_queue *tx_que = &vsi->tx_queues[txqid];
	if_softc_ctx_t scctx = vsi->shared;
	struct tx_ring *txr = &tx_que->txr;

	qidx_t processed = 0;
	qidx_t cur, prev, ntxd, rs_cidx;
	int32_t delta;
	bool is_done;

	rs_cidx = txr->tx_rs_cidx;
	if (rs_cidx == txr->tx_rs_pidx)
		return (0);
	cur = txr->tx_rsq[rs_cidx];
	MPASS(cur != QIDX_INVALID);
	is_done = iavf_is_tx_desc_done(txr, cur);

	if (!is_done)
		return (0);

	/* If clear is false just let caller know that there
	 * are descriptors to reclaim */
	if (!clear)
		return (1);

	prev = txr->tx_cidx_processed;
	ntxd = scctx->isc_ntxd[0];
	do {
		MPASS(prev != cur);
		delta = (int32_t)cur - (int32_t)prev;
		if (delta < 0)
			delta += ntxd;
		MPASS(delta > 0);
		processed += delta;
		prev = cur;
		rs_cidx = (rs_cidx + 1) & (ntxd-1);
		if (rs_cidx == txr->tx_rs_pidx)
			break;
		cur = txr->tx_rsq[rs_cidx];
		MPASS(cur != QIDX_INVALID);
		is_done = iavf_is_tx_desc_done(txr, cur);
	} while (is_done);

	txr->tx_rs_cidx = rs_cidx;
	txr->tx_cidx_processed = prev;

	return (processed);
}

/**
 * iavf_isc_rxd_refill - Prepare descriptors for re-use
 * @arg: void pointer to the VSI
 * @iru: the Rx descriptor update structure
 *
 * Update Rx descriptors for a given queue so that they can be re-used by
 * hardware for future packets.
 */
static void
iavf_isc_rxd_refill(void *arg, if_rxd_update_t iru)
{
	struct iavf_vsi *vsi = arg;
	if_softc_ctx_t scctx = vsi->shared;
	struct rx_ring *rxr = &((vsi->rx_queues[iru->iru_qsidx]).rxr);
	uint64_t *paddrs;
	uint16_t next_pidx, pidx;
	uint16_t count;
	int i;

	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;
	count = iru->iru_count;

	for (i = 0, next_pidx = pidx; i < count; i++) {
		rxr->rx_base[next_pidx].read.pkt_addr = htole64(paddrs[i]);
		if (++next_pidx == scctx->isc_nrxd[0])
			next_pidx = 0;
	}
}

/**
 * iavf_isc_rxd_flush - Notify hardware of new Rx descriptors
 * @arg: void pointer to the VSI
 * @rxqid: Rx queue to update
 * @flid: unused parameter
 * @pidx: ring index to update to
 *
 * Updates the tail pointer of the Rx ring, notifying hardware of new
 * descriptors available for receiving packets.
 */
static void
iavf_isc_rxd_flush(void * arg, uint16_t rxqid, uint8_t flid __unused, qidx_t pidx)
{
	struct iavf_vsi		*vsi = arg;
	struct rx_ring		*rxr = &vsi->rx_queues[rxqid].rxr;

	wr32(vsi->hw, rxr->tail, pidx);
}

/**
 * iavf_isc_rxd_available - Calculate number of available Rx descriptors
 * @arg: void pointer to the VSI
 * @rxqid: Rx queue to check
 * @idx: starting index to check from
 * @budget: maximum Rx budget
 *
 * Determines how many packets are ready to be processed in the Rx queue, up
 * to the specified budget.
 *
 * @returns the number of packets ready to be processed, up to the budget.
 */
static int
iavf_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx, qidx_t budget)
{
	struct iavf_vsi *vsi = arg;
	struct rx_ring *rxr = &vsi->rx_queues[rxqid].rxr;
	union iavf_rx_desc *rxd;
	u64 qword;
	uint32_t status;
	int cnt, i, nrxd;

	nrxd = vsi->shared->isc_nrxd[0];

	for (cnt = 0, i = idx; cnt < nrxd - 1 && cnt <= budget;) {
		rxd = &rxr->rx_base[i];
		qword = le64toh(rxd->wb.qword1.status_error_len);
		status = (qword & IAVF_RXD_QW1_STATUS_MASK)
			>> IAVF_RXD_QW1_STATUS_SHIFT;

		if ((status & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)) == 0)
			break;
		if (++i == nrxd)
			i = 0;
		if (status & (1 << IAVF_RX_DESC_STATUS_EOF_SHIFT))
			cnt++;
	}

	return (cnt);
}

/**
 * iavf_isc_rxd_pkt_get - Decapsulate packet from Rx descriptors
 * @arg: void pointer to the VSI
 * @ri: packet info structure
 *
 * Read packet data from the Rx ring descriptors and fill in the packet info
 * structure so that the iflib stack can process the packet.
 *
 * @remark this routine executes in ithread context.
 *
 * @returns zero success, or EBADMSG if the packet is corrupted.
 */
static int
iavf_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri)
{
	struct iavf_vsi		*vsi = arg;
	if_softc_ctx_t		scctx = vsi->shared;
	struct iavf_rx_queue	*que = &vsi->rx_queues[ri->iri_qsidx];
	struct rx_ring		*rxr = &que->rxr;
	union iavf_rx_desc	*cur;
	u32		status, error;
	u16		plen, vtag;
	u64		qword;
	u8		ptype;
	bool		eop;
	int i, cidx;

	cidx = ri->iri_cidx;
	i = 0;
	do {
		/* 5 descriptor receive limit */
		MPASS(i < IAVF_MAX_RX_SEGS);

		cur = &rxr->rx_base[cidx];
		qword = le64toh(cur->wb.qword1.status_error_len);
		status = (qword & IAVF_RXD_QW1_STATUS_MASK)
		    >> IAVF_RXD_QW1_STATUS_SHIFT;
		error = (qword & IAVF_RXD_QW1_ERROR_MASK)
		    >> IAVF_RXD_QW1_ERROR_SHIFT;
		plen = (qword & IAVF_RXD_QW1_LENGTH_PBUF_MASK)
		    >> IAVF_RXD_QW1_LENGTH_PBUF_SHIFT;
		ptype = (qword & IAVF_RXD_QW1_PTYPE_MASK)
		    >> IAVF_RXD_QW1_PTYPE_SHIFT;

		/* we should never be called without a valid descriptor */
		MPASS((status & (1 << IAVF_RX_DESC_STATUS_DD_SHIFT)) != 0);

		ri->iri_len += plen;
		rxr->rx_bytes += plen;

		cur->wb.qword1.status_error_len = 0;
		eop = (status & (1 << IAVF_RX_DESC_STATUS_EOF_SHIFT));
		if (status & (1 << IAVF_RX_DESC_STATUS_L2TAG1P_SHIFT))
			vtag = le16toh(cur->wb.qword0.lo_dword.l2tag1);
		else
			vtag = 0;

		/*
		** Make sure bad packets are discarded,
		** note that only EOP descriptor has valid
		** error results.
		*/
		if (eop && (error & (1 << IAVF_RX_DESC_ERROR_RXE_SHIFT))) {
			rxr->desc_errs++;
			return (EBADMSG);
		}
		ri->iri_frags[i].irf_flid = 0;
		ri->iri_frags[i].irf_idx = cidx;
		ri->iri_frags[i].irf_len = plen;
		if (++cidx == vsi->shared->isc_nrxd[0])
			cidx = 0;
		i++;
	} while (!eop);

	/* capture data for dynamic ITR adjustment */
	rxr->packets++;
	rxr->rx_packets++;

	if ((scctx->isc_capenable & IFCAP_RXCSUM) != 0)
		iavf_rx_checksum(ri, status, error, ptype);
	ri->iri_flowid = le32toh(cur->wb.qword0.hi_dword.rss);
	ri->iri_rsstype = iavf_ptype_to_hash(ptype);
	ri->iri_vtag = vtag;
	ri->iri_nfrags = i;
	if (vtag)
		ri->iri_flags |= M_VLANTAG;
	return (0);
}

/**
 * iavf_rx_checksum - Handle Rx hardware checksum indication
 * @ri: Rx packet info structure
 * @status: status from Rx descriptor
 * @error: error from Rx descriptor
 * @ptype: packet type
 *
 * Verify that the hardware indicated that the checksum is valid.
 * Inform the stack about the status of checksum so that stack
 * doesn't spend time verifying the checksum.
 */
static void
iavf_rx_checksum(if_rxd_info_t ri, u32 status, u32 error, u8 ptype)
{
	struct iavf_rx_ptype_decoded decoded;

	ri->iri_csum_flags = 0;

	/* No L3 or L4 checksum was calculated */
	if (!(status & (1 << IAVF_RX_DESC_STATUS_L3L4P_SHIFT)))
		return;

	decoded = decode_rx_desc_ptype(ptype);

	/* IPv6 with extension headers likely have bad csum */
	if (decoded.outer_ip == IAVF_RX_PTYPE_OUTER_IP &&
	    decoded.outer_ip_ver == IAVF_RX_PTYPE_OUTER_IPV6) {
		if (status &
		    (1 << IAVF_RX_DESC_STATUS_IPV6EXADD_SHIFT)) {
			ri->iri_csum_flags = 0;
			return;
		}
	}

	ri->iri_csum_flags |= CSUM_L3_CALC;

	/* IPv4 checksum error */
	if (error & (1 << IAVF_RX_DESC_ERROR_IPE_SHIFT))
		return;

	ri->iri_csum_flags |= CSUM_L3_VALID;
	ri->iri_csum_flags |= CSUM_L4_CALC;

	/* L4 checksum error */
	if (error & (1 << IAVF_RX_DESC_ERROR_L4E_SHIFT))
		return;

	ri->iri_csum_flags |= CSUM_L4_VALID;
	ri->iri_csum_data |= htons(0xffff);
}
