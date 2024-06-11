/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
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

/**
 * @file ice_iflib_txrx.c
 * @brief iflib Tx/Rx hotpath
 *
 * Main location for the iflib Tx/Rx hotpath implementation.
 *
 * Contains the implementation for the iflib function callbacks and the
 * if_txrx ops structure.
 */

#include "ice_iflib.h"

/* Tx/Rx hotpath utility functions */
#include "ice_common_txrx.h"

/*
 * Driver private implementations
 */
static int _ice_ift_txd_encap(struct ice_tx_queue *txq, if_pkt_info_t pi);
static int _ice_ift_txd_credits_update(struct ice_softc *sc, struct ice_tx_queue *txq, bool clear);
static int _ice_ift_rxd_available(struct ice_rx_queue *rxq, qidx_t pidx, qidx_t budget);
static int _ice_ift_rxd_pkt_get(struct ice_rx_queue *rxq, if_rxd_info_t ri);
static void _ice_ift_rxd_refill(struct ice_rx_queue *rxq, uint32_t pidx,
				uint64_t *paddrs, uint16_t count);
static void _ice_ift_rxd_flush(struct ice_softc *sc, struct ice_rx_queue *rxq,
			       uint32_t pidx);

/*
 * iflib txrx method declarations
 */
static int ice_ift_txd_encap(void *arg, if_pkt_info_t pi);
static int ice_ift_rxd_pkt_get(void *arg, if_rxd_info_t ri);
static void ice_ift_txd_flush(void *arg, uint16_t txqid, qidx_t pidx);
static int ice_ift_txd_credits_update(void *arg, uint16_t txqid, bool clear);
static int ice_ift_rxd_available(void *arg, uint16_t rxqid, qidx_t pidx, qidx_t budget);
static void ice_ift_rxd_flush(void *arg, uint16_t rxqid, uint8_t flidx, qidx_t pidx);
static void ice_ift_rxd_refill(void *arg, if_rxd_update_t iru);
static qidx_t ice_ift_queue_select(void *arg, struct mbuf *m, if_pkt_info_t pi);
static int ice_ift_txd_credits_update_subif(void *arg, uint16_t txqid, bool clear);
static int ice_ift_txd_encap_subif(void *arg, if_pkt_info_t pi);
static void ice_ift_txd_flush_subif(void *arg, uint16_t txqid, qidx_t pidx);
static int ice_ift_rxd_available_subif(void *arg, uint16_t rxqid, qidx_t pidx, qidx_t budget);
static int ice_ift_rxd_pkt_get_subif(void *arg, if_rxd_info_t ri);
static void ice_ift_rxd_refill_subif(void *arg, if_rxd_update_t iru);
static void ice_ift_rxd_flush_subif(void *arg, uint16_t rxqid, uint8_t flidx, qidx_t pidx);

/* Macro to help extract the NIC mode flexible Rx descriptor fields from the
 * advanced 32byte Rx descriptors.
 */
#define RX_FLEX_NIC(desc, field) \
	(((struct ice_32b_rx_flex_desc_nic *)desc)->field)

/**
 * @var ice_txrx
 * @brief Tx/Rx operations for the iflib stack
 *
 * Structure defining the Tx and Rx related operations that iflib can request
 * the driver to perform. These are the main entry points for the hot path of
 * the transmit and receive paths in the iflib driver.
 */
struct if_txrx ice_txrx = {
	.ift_txd_encap = ice_ift_txd_encap,
	.ift_txd_flush = ice_ift_txd_flush,
	.ift_txd_credits_update = ice_ift_txd_credits_update,
	.ift_rxd_available = ice_ift_rxd_available,
	.ift_rxd_pkt_get = ice_ift_rxd_pkt_get,
	.ift_rxd_refill = ice_ift_rxd_refill,
	.ift_rxd_flush = ice_ift_rxd_flush,
	.ift_txq_select_v2 = ice_ift_queue_select,
};

/**
 * @var ice_subif_txrx
 * @brief Tx/Rx operations for the iflib stack, for subinterfaces
 *
 * Structure defining the Tx and Rx related operations that iflib can request
 * the subinterface driver to perform. These are the main entry points for the
 * hot path of the transmit and receive paths in the iflib driver.
 */
struct if_txrx ice_subif_txrx = {
	.ift_txd_credits_update = ice_ift_txd_credits_update_subif,
	.ift_txd_encap = ice_ift_txd_encap_subif,
	.ift_txd_flush = ice_ift_txd_flush_subif,
	.ift_rxd_available = ice_ift_rxd_available_subif,
	.ift_rxd_pkt_get = ice_ift_rxd_pkt_get_subif,
	.ift_rxd_refill = ice_ift_rxd_refill_subif,
	.ift_rxd_flush = ice_ift_rxd_flush_subif,
	.ift_txq_select_v2 = NULL,
};

/**
 * _ice_ift_txd_encap - prepare Tx descriptors for a packet
 * @txq: driver's TX queue context
 * @pi: packet info
 *
 * Prepares and encapsulates the given packet into into Tx descriptors, in
 * preparation for sending to the transmit engine. Sets the necessary context
 * descriptors for TSO and other offloads, and prepares the last descriptor
 * for the writeback status.
 *
 * Return 0 on success, non-zero error code on failure.
 */
static int
_ice_ift_txd_encap(struct ice_tx_queue *txq, if_pkt_info_t pi)
{
	int nsegs = pi->ipi_nsegs;
	bus_dma_segment_t *segs = pi->ipi_segs;
	struct ice_tx_desc *txd = NULL;
	int i, j, mask, pidx_last;
	u32 cmd, off;

	cmd = off = 0;
	i = pi->ipi_pidx;

	/* Set up the TSO/CSUM offload */
	if (pi->ipi_csum_flags & ICE_CSUM_OFFLOAD) {
		/* Set up the TSO context descriptor if required */
		if (pi->ipi_csum_flags & CSUM_TSO) {
			if (ice_tso_detect_sparse(pi))
				return (EFBIG);
			i = ice_tso_setup(txq, pi);
		}
		ice_tx_setup_offload(txq, pi, &cmd, &off);
	}
	if (pi->ipi_mflags & M_VLANTAG)
		cmd |= ICE_TX_DESC_CMD_IL2TAG1;

	mask = txq->desc_count - 1;
	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;

		txd = &txq->tx_base[i];
		seglen = segs[j].ds_len;

		txd->buf_addr = htole64(segs[j].ds_addr);
		txd->cmd_type_offset_bsz =
		    htole64(ICE_TX_DESC_DTYPE_DATA
		    | ((u64)cmd  << ICE_TXD_QW1_CMD_S)
		    | ((u64)off << ICE_TXD_QW1_OFFSET_S)
		    | ((u64)seglen  << ICE_TXD_QW1_TX_BUF_SZ_S)
		    | ((u64)htole16(pi->ipi_vtag) << ICE_TXD_QW1_L2TAG1_S));

		txq->stats.tx_bytes += seglen;
		pidx_last = i;
		i = (i+1) & mask;
	}

	/* Set the last descriptor for report */
#define ICE_TXD_CMD (ICE_TX_DESC_CMD_EOP | ICE_TX_DESC_CMD_RS)
	txd->cmd_type_offset_bsz |=
	    htole64(((u64)ICE_TXD_CMD << ICE_TXD_QW1_CMD_S));

	/* Add to report status array */
	txq->tx_rsq[txq->tx_rs_pidx] = pidx_last;
	txq->tx_rs_pidx = (txq->tx_rs_pidx+1) & mask;
	MPASS(txq->tx_rs_pidx != txq->tx_rs_cidx);

	pi->ipi_new_pidx = i;

	++txq->stats.tx_packets;
	return (0);
}

/**
 * ice_ift_txd_encap - prepare Tx descriptors for a packet
 * @arg: the iflib softc structure pointer
 * @pi: packet info
 *
 * Prepares and encapsulates the given packet into Tx descriptors, in
 * preparation for sending to the transmit engine. Sets the necessary context
 * descriptors for TSO and other offloads, and prepares the last descriptor
 * for the writeback status.
 *
 * Return 0 on success, non-zero error code on failure.
 */
static int
ice_ift_txd_encap(void *arg, if_pkt_info_t pi)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_tx_queue *txq = &sc->pf_vsi.tx_queues[pi->ipi_qsidx];

	return _ice_ift_txd_encap(txq, pi);
}

/**
 * ice_ift_txd_flush - Flush Tx descriptors to hardware
 * @arg: device specific softc pointer
 * @txqid: the Tx queue to flush
 * @pidx: descriptor index to advance tail to
 *
 * Advance the Transmit Descriptor Tail (TDT). This indicates to hardware that
 * frames are available for transmit.
 */
static void
ice_ift_txd_flush(void *arg, uint16_t txqid, qidx_t pidx)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_tx_queue *txq = &sc->pf_vsi.tx_queues[txqid];
	struct ice_hw *hw = &sc->hw;

	wr32(hw, txq->tail, pidx);
}

/**
 * _ice_ift_txd_credits_update - cleanup Tx descriptors
 * @sc: device private softc
 * @txq: the Tx queue to update
 * @clear: if false, only report, do not actually clean
 *
 * If clear is false, iflib is asking if we *could* clean up any Tx
 * descriptors.
 *
 * If clear is true, iflib is requesting to cleanup and reclaim used Tx
 * descriptors.
 *
 * Called by other txd_credits_update functions passed to iflib.
 */
static int
_ice_ift_txd_credits_update(struct ice_softc *sc __unused, struct ice_tx_queue *txq, bool clear)
{
	qidx_t processed = 0;
	qidx_t cur, prev, ntxd, rs_cidx;
	int32_t delta;
	bool is_done;

	rs_cidx = txq->tx_rs_cidx;
	if (rs_cidx == txq->tx_rs_pidx)
		return (0);
	cur = txq->tx_rsq[rs_cidx];
	MPASS(cur != QIDX_INVALID);
	is_done = ice_is_tx_desc_done(&txq->tx_base[cur]);

	if (!is_done)
		return (0);
	else if (clear == false)
		return (1);

	prev = txq->tx_cidx_processed;
	ntxd = txq->desc_count;
	do {
		MPASS(prev != cur);
		delta = (int32_t)cur - (int32_t)prev;
		if (delta < 0)
			delta += ntxd;
		MPASS(delta > 0);
		processed += delta;
		prev = cur;
		rs_cidx = (rs_cidx + 1) & (ntxd-1);
		if (rs_cidx == txq->tx_rs_pidx)
			break;
		cur = txq->tx_rsq[rs_cidx];
		MPASS(cur != QIDX_INVALID);
		is_done = ice_is_tx_desc_done(&txq->tx_base[cur]);
	} while (is_done);

	txq->tx_rs_cidx = rs_cidx;
	txq->tx_cidx_processed = prev;

	return (processed);
}

/**
 * ice_ift_txd_credits_update - cleanup PF VSI Tx descriptors
 * @arg: device private softc
 * @txqid: the Tx queue to update
 * @clear: if false, only report, do not actually clean
 *
 * Wrapper for _ice_ift_txd_credits_update() meant for TX queues that
 * belong to the PF VSI.
 *
 * @see _ice_ift_txd_credits_update()
 */
static int
ice_ift_txd_credits_update(void *arg, uint16_t txqid, bool clear)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_tx_queue *txq = &sc->pf_vsi.tx_queues[txqid];

	return _ice_ift_txd_credits_update(sc, txq, clear);
}

/**
 * _ice_ift_rxd_available - Return number of available Rx packets
 * @rxq: RX queue driver structure
 * @pidx: descriptor start point
 * @budget: maximum Rx budget
 *
 * Determines how many Rx packets are available on the queue, up to a maximum
 * of the given budget.
 */
static int
_ice_ift_rxd_available(struct ice_rx_queue *rxq, qidx_t pidx, qidx_t budget)
{
	union ice_32b_rx_flex_desc *rxd;
	uint16_t status0;
	int cnt, i, nrxd;

	nrxd = rxq->desc_count;

	for (cnt = 0, i = pidx; cnt < nrxd - 1 && cnt < budget;) {
		rxd = &rxq->rx_base[i];
		status0 = le16toh(rxd->wb.status_error0);

		if ((status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_DD_S)) == 0)
			break;
		if (++i == nrxd)
			i = 0;
		if (status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_EOF_S))
			cnt++;
	}

	return (cnt);
}

/**
 * ice_ift_rxd_available - Return number of available Rx packets
 * @arg: device private softc
 * @rxqid: the Rx queue id
 * @pidx: descriptor start point
 * @budget: maximum Rx budget
 *
 * Wrapper for _ice_ift_rxd_available() that provides a function pointer
 * that iflib requires for RX processing.
 */
static int
ice_ift_rxd_available(void *arg, uint16_t rxqid, qidx_t pidx, qidx_t budget)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_rx_queue *rxq = &sc->pf_vsi.rx_queues[rxqid];

	return _ice_ift_rxd_available(rxq, pidx, budget);
}

/**
 * ice_ift_rxd_pkt_get - Called by iflib to send data to upper layer
 * @arg: device specific softc
 * @ri: receive packet info
 *
 * Wrapper function for _ice_ift_rxd_pkt_get() that provides a function pointer
 * used by iflib for RX packet processing.
 */
static int
ice_ift_rxd_pkt_get(void *arg, if_rxd_info_t ri)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_rx_queue *rxq = &sc->pf_vsi.rx_queues[ri->iri_qsidx];

	return _ice_ift_rxd_pkt_get(rxq, ri);
}

/**
 * _ice_ift_rxd_pkt_get - Called by iflib to send data to upper layer
 * @rxq: RX queue driver structure
 * @ri: receive packet info
 *
 * This function is called by iflib, and executes in ithread context. It is
 * called by iflib to obtain data which has been DMA'ed into host memory.
 * Returns zero on success, and EBADMSG on failure.
 */
static int
_ice_ift_rxd_pkt_get(struct ice_rx_queue *rxq, if_rxd_info_t ri)
{
	union ice_32b_rx_flex_desc *cur;
	u16 status0, plen, ptype;
	bool eop;
	size_t cidx;
	int i;

	cidx = ri->iri_cidx;
	i = 0;
	do {
		/* 5 descriptor receive limit */
		MPASS(i < ICE_MAX_RX_SEGS);

		cur = &rxq->rx_base[cidx];
		status0 = le16toh(cur->wb.status_error0);
		plen = le16toh(cur->wb.pkt_len) &
			ICE_RX_FLX_DESC_PKT_LEN_M;

		/* we should never be called without a valid descriptor */
		MPASS((status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_DD_S)) != 0);

		ri->iri_len += plen;

		cur->wb.status_error0 = 0;
		eop = (status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_EOF_S));

		ri->iri_frags[i].irf_flid = 0;
		ri->iri_frags[i].irf_idx = cidx;
		ri->iri_frags[i].irf_len = plen;
		if (++cidx == rxq->desc_count)
			cidx = 0;
		i++;
	} while (!eop);

	/* End of Packet reached; cur is eop/last descriptor */

	/* Make sure packets with bad L2 values are discarded.
	 * This bit is only valid in the last descriptor.
	 */
	if (status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_RXE_S)) {
		rxq->stats.desc_errs++;
		return (EBADMSG);
	}

	/* Get VLAN tag information if one is in descriptor */
	if (status0 & BIT(ICE_RX_FLEX_DESC_STATUS0_L2TAG1P_S)) {
		ri->iri_vtag = le16toh(cur->wb.l2tag1);
		ri->iri_flags |= M_VLANTAG;
	}

	/* Capture soft statistics for this Rx queue */
	rxq->stats.rx_packets++;
	rxq->stats.rx_bytes += ri->iri_len;

	/* Get packet type and set checksum flags */
	ptype = le16toh(cur->wb.ptype_flex_flags0) &
		ICE_RX_FLEX_DESC_PTYPE_M;
	if ((if_getcapenable(ri->iri_ifp) & IFCAP_RXCSUM) != 0)
		ice_rx_checksum(rxq, &ri->iri_csum_flags,
				&ri->iri_csum_data, status0, ptype);

	/* Set remaining iflib RX descriptor info fields */
	ri->iri_flowid = le32toh(RX_FLEX_NIC(&cur->wb, rss_hash));
	ri->iri_rsstype = ice_ptype_to_hash(ptype);
	ri->iri_nfrags = i;
	return (0);
}

/**
 * ice_ift_rxd_refill - Prepare Rx descriptors for re-use by hardware
 * @arg: device specific softc structure
 * @iru: the Rx descriptor update structure
 *
 * Wrapper function for _ice_ift_rxd_refill() that provides a function pointer
 * used by iflib for RX packet processing.
 */
static void
ice_ift_rxd_refill(void *arg, if_rxd_update_t iru)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_rx_queue *rxq;
	uint64_t *paddrs;
	uint32_t pidx;
	uint16_t qsidx, count;

	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;
	qsidx = iru->iru_qsidx;
	count = iru->iru_count;

	rxq = &(sc->pf_vsi.rx_queues[qsidx]);

	_ice_ift_rxd_refill(rxq, pidx, paddrs, count);
}

/**
 * _ice_ift_rxd_refill - Prepare Rx descriptors for re-use by hardware
 * @rxq: RX queue driver structure
 * @pidx: first index to refill
 * @paddrs: physical addresses to use
 * @count: number of descriptors to refill
 *
 * Update the Rx descriptor indices for a given queue, assigning new physical
 * addresses to the descriptors, preparing them for re-use by the hardware.
 */
static void
_ice_ift_rxd_refill(struct ice_rx_queue *rxq, uint32_t pidx,
		    uint64_t *paddrs, uint16_t count)
{
	uint32_t next_pidx;
	int i;

	for (i = 0, next_pidx = pidx; i < count; i++) {
		rxq->rx_base[next_pidx].read.pkt_addr = htole64(paddrs[i]);
		if (++next_pidx == (uint32_t)rxq->desc_count)
			next_pidx = 0;
	}
}

/**
 * ice_ift_rxd_flush - Flush Rx descriptors to hardware
 * @arg: device specific softc pointer
 * @rxqid: the Rx queue to flush
 * @flidx: unused parameter
 * @pidx: descriptor index to advance tail to
 *
 * Wrapper function for _ice_ift_rxd_flush() that provides a function pointer
 * used by iflib for RX packet processing.
 */
static void
ice_ift_rxd_flush(void *arg, uint16_t rxqid, uint8_t flidx __unused,
		  qidx_t pidx)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_rx_queue *rxq = &sc->pf_vsi.rx_queues[rxqid];

	_ice_ift_rxd_flush(sc, rxq, (uint32_t)pidx);
}

/**
 * _ice_ift_rxd_flush - Flush Rx descriptors to hardware
 * @sc: device specific softc pointer
 * @rxq: RX queue driver structure
 * @pidx: descriptor index to advance tail to
 *
 * Advance the Receive Descriptor Tail (RDT). This indicates to hardware that
 * software is done with the descriptor and it can be recycled.
 */
static void
_ice_ift_rxd_flush(struct ice_softc *sc, struct ice_rx_queue *rxq, uint32_t pidx)
{
	wr32(&sc->hw, rxq->tail, pidx);
}

/**
 * ice_ift_queue_select - Select queue index to transmit packet on
 * @arg: device specific softc
 * @m: transmit packet data
 * @pi: transmit packet metadata
 *
 * Called by iflib to determine which queue index to transmit the packet
 * pointed to by @m on. In particular, ensures packets go out on the right
 * queue index for the right transmit class when multiple traffic classes are
 * enabled in the driver.
 */
static qidx_t
ice_ift_queue_select(void *arg, struct mbuf *m, if_pkt_info_t pi)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_dcbx_cfg *local_dcbx_cfg;
	struct ice_vsi *vsi = &sc->pf_vsi;
	u16 tc_base_queue, tc_qcount;
	u8 up, tc;

#ifdef ALTQ
	/* Included to match default iflib behavior */
	/* Only go out on default queue if ALTQ is enabled */
	struct ifnet *ifp = (struct ifnet *)iflib_get_ifp(sc->ctx);
	if (if_altq_is_enabled(ifp))
		return (0);
#endif

	if (!ice_test_state(&sc->state, ICE_STATE_MULTIPLE_TCS)) {
		if (M_HASHTYPE_GET(m)) {
			/* Default iflib queue selection method */
			return (m->m_pkthdr.flowid % sc->pf_vsi.num_tx_queues);
		} else
			return (0);
	}

	/* Use default TC unless overridden later */
	tc = 0; /* XXX: Get default TC for traffic if >1 TC? */

	local_dcbx_cfg = &sc->hw.port_info->qos_cfg.local_dcbx_cfg;

#if defined(INET) || defined(INET6)
	if ((local_dcbx_cfg->pfc_mode == ICE_QOS_MODE_DSCP) &&
	    (pi->ipi_flags & (IPI_TX_IPV4 | IPI_TX_IPV6))) {
		u8 dscp_val = pi->ipi_ip_tos >> 2;
		tc = local_dcbx_cfg->dscp_map[dscp_val];
	} else
#endif /* defined(INET) || defined(INET6) */
	if (m->m_flags & M_VLANTAG) { /* ICE_QOS_MODE_VLAN */
		up = EVL_PRIOFTAG(m->m_pkthdr.ether_vtag);
		tc = local_dcbx_cfg->etscfg.prio_table[up];
	}

	tc_base_queue = vsi->tc_info[tc].qoffset;
	tc_qcount = vsi->tc_info[tc].qcount_tx;

	if (M_HASHTYPE_GET(m))
		return ((m->m_pkthdr.flowid % tc_qcount) + tc_base_queue);
	else
		return (tc_base_queue);
}

/**
 * ice_ift_txd_credits_update_subif - cleanup subinterface VSI Tx descriptors
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @txqid: the Tx queue to update
 * @clear: if false, only report, do not actually clean
 *
 * Wrapper for _ice_ift_txd_credits_update() meant for TX queues that
 * do not belong to the PF VSI.
 *
 * See _ice_ift_txd_credits_update().
 */
static int
ice_ift_txd_credits_update_subif(void *arg, uint16_t txqid, bool clear)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_softc *sc = mif->back;
	struct ice_tx_queue *txq = &mif->vsi->tx_queues[txqid];

	return _ice_ift_txd_credits_update(sc, txq, clear);
}

/**
 * ice_ift_txd_encap_subif - prepare Tx descriptors for a packet
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @pi: packet info
 *
 * Wrapper for _ice_ift_txd_encap_subif() meant for TX queues that
 * do not belong to the PF VSI.
 *
 * See _ice_ift_txd_encap_subif().
 */
static int
ice_ift_txd_encap_subif(void *arg, if_pkt_info_t pi)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_tx_queue *txq = &mif->vsi->tx_queues[pi->ipi_qsidx];

	return _ice_ift_txd_encap(txq, pi);
}

/**
 * ice_ift_txd_flush_subif - Flush Tx descriptors to hardware
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @txqid: the Tx queue to flush
 * @pidx: descriptor index to advance tail to
 *
 * Advance the Transmit Descriptor Tail (TDT). Functionally identical to
 * the ice_ift_txd_encap() meant for the main PF VSI, but provides a function
 * pointer to iflib for use with non-main-PF VSI TX queues.
 */
static void
ice_ift_txd_flush_subif(void *arg, uint16_t txqid, qidx_t pidx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_tx_queue *txq = &mif->vsi->tx_queues[txqid];
	struct ice_hw *hw = &mif->back->hw;

	wr32(hw, txq->tail, pidx);
}

/**
 * ice_ift_rxd_available_subif - Return number of available Rx packets
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @rxqid: the Rx queue id
 * @pidx: descriptor start point
 * @budget: maximum Rx budget
 *
 * Determines how many Rx packets are available on the queue, up to a maximum
 * of the given budget.
 *
 * See _ice_ift_rxd_available().
 */
static int
ice_ift_rxd_available_subif(void *arg, uint16_t rxqid, qidx_t pidx, qidx_t budget)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_rx_queue *rxq = &mif->vsi->rx_queues[rxqid];

	return _ice_ift_rxd_available(rxq, pidx, budget);
}

/**
 * ice_ift_rxd_pkt_get_subif - Called by iflib to send data to upper layer
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @ri: receive packet info
 *
 * Wrapper function for _ice_ift_rxd_pkt_get() that provides a function pointer
 * used by iflib for RX packet processing, for iflib subinterfaces.
 */
static int
ice_ift_rxd_pkt_get_subif(void *arg, if_rxd_info_t ri)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_rx_queue *rxq = &mif->vsi->rx_queues[ri->iri_qsidx];

	return _ice_ift_rxd_pkt_get(rxq, ri);
}

/**
 * ice_ift_rxd_refill_subif - Prepare Rx descriptors for re-use by hardware
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @iru: the Rx descriptor update structure
 *
 * Wrapper function for _ice_ift_rxd_refill() that provides a function pointer
 * used by iflib for RX packet processing, for iflib subinterfaces.
 */
static void
ice_ift_rxd_refill_subif(void *arg, if_rxd_update_t iru)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_rx_queue *rxq = &mif->vsi->rx_queues[iru->iru_qsidx];

	uint64_t *paddrs;
	uint32_t pidx;
	uint16_t count;

	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;
	count = iru->iru_count;

	_ice_ift_rxd_refill(rxq, pidx, paddrs, count);
}

/**
 * ice_ift_rxd_flush_subif - Flush Rx descriptors to hardware
 * @arg: subinterface private structure (struct ice_mirr_if)
 * @rxqid: the Rx queue to flush
 * @flidx: unused parameter
 * @pidx: descriptor index to advance tail to
 *
 * Wrapper function for _ice_ift_rxd_flush() that provides a function pointer
 * used by iflib for RX packet processing.
 */
static void
ice_ift_rxd_flush_subif(void *arg, uint16_t rxqid, uint8_t flidx __unused,
			qidx_t pidx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)arg;
	struct ice_rx_queue *rxq = &mif->vsi->rx_queues[rxqid];

	_ice_ift_rxd_flush(mif->back, rxq, pidx);
}
