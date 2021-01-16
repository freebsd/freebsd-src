/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Advanced Micro Devices, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Contact Information :
 * Rajesh Kumar <rajesh1.kumar@amd.com>
 * Shreyank Amartya <Shreyank.Amartya@amd.com>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "xgbe.h"
#include "xgbe-common.h"

/*
 * IFLIB interfaces
 */
static int axgbe_isc_txd_encap(void *, if_pkt_info_t);
static void axgbe_isc_txd_flush(void *, uint16_t, qidx_t);
static int axgbe_isc_txd_credits_update(void *, uint16_t, bool);
static void axgbe_isc_rxd_refill(void *, if_rxd_update_t);
static void axgbe_isc_rxd_flush(void *, uint16_t, uint8_t, qidx_t);
static int axgbe_isc_rxd_available(void *, uint16_t, qidx_t, qidx_t);
static int axgbe_isc_rxd_pkt_get(void *, if_rxd_info_t);

struct if_txrx axgbe_txrx = {
	.ift_txd_encap = axgbe_isc_txd_encap,
	.ift_txd_flush = axgbe_isc_txd_flush,
	.ift_txd_credits_update = axgbe_isc_txd_credits_update,
	.ift_rxd_available = axgbe_isc_rxd_available,
	.ift_rxd_pkt_get = axgbe_isc_rxd_pkt_get,
	.ift_rxd_refill = axgbe_isc_rxd_refill,
	.ift_rxd_flush = axgbe_isc_rxd_flush,
	.ift_legacy_intr = NULL
};

static void
xgbe_print_pkt_info(struct xgbe_prv_data *pdata, if_pkt_info_t pi)
{

	axgbe_printf(1, "------Packet Info Start------\n");
	axgbe_printf(1, "pi len:  %d qsidx: %d nsegs: %d ndescs: %d flags: %x pidx: %d\n",
               pi->ipi_len, pi->ipi_qsidx, pi->ipi_nsegs, pi->ipi_ndescs, pi->ipi_flags, pi->ipi_pidx);
        axgbe_printf(1, "pi new_pidx: %d csum_flags: %x mflags: %x vtag: %d\n",
               pi->ipi_new_pidx, pi->ipi_csum_flags, pi->ipi_mflags, pi->ipi_vtag);
        axgbe_printf(1, "pi etype: %d ehdrlen: %d ip_hlen: %d ipproto: %d\n",
               pi->ipi_etype, pi->ipi_ehdrlen, pi->ipi_ip_hlen, pi->ipi_ipproto);
        axgbe_printf(1, "pi tcp_hlen: %d tcp_hflags: %x tcp_seq: %d tso_segsz %d\n",
               pi->ipi_tcp_hlen, pi->ipi_tcp_hflags, pi->ipi_tcp_seq, pi->ipi_tso_segsz);
}

static bool
axgbe_ctx_desc_setup(struct xgbe_prv_data *pdata, struct xgbe_ring *ring,
    if_pkt_info_t pi)
{
	struct xgbe_ring_desc	*rdesc;
	struct xgbe_ring_data	*rdata;
	bool inc_cur = false;

	rdata = XGBE_GET_DESC_DATA(ring, ring->cur);
	rdesc = rdata->rdesc;

	axgbe_printf(1, "ipi_tso_segsz %d cur_mss %d idx %d\n",
	    pi->ipi_tso_segsz, ring->tx.cur_mss, ring->cur);

	axgbe_printf(1, "ipi_vtag 0x%x cur_vlan_ctag 0x%x\n",
	    pi->ipi_vtag, ring->tx.cur_vlan_ctag);

	if ((pi->ipi_csum_flags & CSUM_TSO) &&
	    (pi->ipi_tso_segsz != ring->tx.cur_mss)) {
		/* 
		 * Set TSO maximum segment size
		 * Mark as context descriptor
		 * Indicate this descriptor contains MSS
		 */
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_CONTEXT_DESC2,
		    MSS, pi->ipi_tso_segsz);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3, CTXT, 1);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3, TCMSSV, 1);
		ring->tx.cur_mss = pi->ipi_tso_segsz;
		inc_cur = true;
	}

	if (pi->ipi_vtag && (pi->ipi_vtag != ring->tx.cur_vlan_ctag)) {
		/* 
		 * Mark it as context descriptor
		 * Set the VLAN tag
		 * Indicate this descriptor contains the VLAN tag
		 */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3, CTXT, 1);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3,
		    VT, pi->ipi_vtag);
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_CONTEXT_DESC3, VLTV, 1);
		ring->tx.cur_vlan_ctag = pi->ipi_vtag;
		inc_cur = true;
	}

	return (inc_cur);
}

static uint16_t
axgbe_calculate_tx_parms(struct xgbe_prv_data *pdata, if_pkt_info_t pi,
    struct xgbe_packet_data *packet)
{
	uint32_t tcp_payload_len = 0, bytes = 0;
	uint16_t max_len, hlen, payload_len, pkts = 0;

	packet->tx_packets = packet->tx_bytes = 0;

	hlen = pi->ipi_ehdrlen + pi->ipi_ip_hlen + pi->ipi_tcp_hlen;
	if (pi->ipi_csum_flags & CSUM_TSO) {

		tcp_payload_len = pi->ipi_len - hlen;
		axgbe_printf(1, "%s: ipi_len %x elen %d iplen %d tcplen %d\n",
		    __func__, pi->ipi_len, pi->ipi_ehdrlen, pi->ipi_ip_hlen,
		    pi->ipi_tcp_hlen);
	
		max_len = if_getmtu(pdata->netdev) + ETH_HLEN;
		if (pi->ipi_vtag)
			max_len += VLAN_HLEN;

		while (tcp_payload_len) {

			payload_len = max_len - hlen;
			payload_len = min(payload_len, tcp_payload_len);
			tcp_payload_len -= payload_len;
			pkts++;
			bytes += (hlen + payload_len);
			axgbe_printf(1, "%s: max_len %d payload_len %d "
			    "tcp_len %d\n", __func__, max_len, payload_len,
			    tcp_payload_len);
		}
	} else {
		pkts = 1;
		bytes = pi->ipi_len;
	}

	packet->tx_packets = pkts;
	packet->tx_bytes = bytes;

	axgbe_printf(1, "%s: packets %d bytes %d hlen %d\n", __func__,
	    packet->tx_packets, packet->tx_bytes, hlen);

	return (hlen);
}

static int
axgbe_isc_txd_encap(void *arg, if_pkt_info_t pi)
{
	struct axgbe_if_softc	*sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel	*channel;
	struct xgbe_ring	*ring;
	struct xgbe_ring_desc	*rdesc;
	struct xgbe_ring_data	*rdata;
	struct xgbe_packet_data *packet;
	unsigned int cur, start, tx_set_ic;
	uint16_t offset, hlen, datalen, tcp_payload_len = 0;
	int cur_seg = 0;

	xgbe_print_pkt_info(pdata, pi);

	channel = pdata->channel[pi->ipi_qsidx];	
	ring = channel->tx_ring;
	packet = &ring->packet_data;
	cur = start = ring->cur;

	axgbe_printf(1, "--> %s: txq %d cur %d dirty %d\n",
	    __func__, pi->ipi_qsidx, ring->cur, ring->dirty);

	MPASS(pi->ipi_len != 0);
	if (__predict_false(pi->ipi_len == 0)) {
		axgbe_error("empty packet received from stack\n");
		return (0);	
	}

	MPASS(ring->cur == pi->ipi_pidx);
	if (__predict_false(ring->cur != pi->ipi_pidx)) {
		axgbe_error("--> %s: cur(%d) ne pidx(%d)\n", __func__,
		    ring->cur, pi->ipi_pidx);
	}

	/* Determine if an interrupt should be generated for this Tx:
	 *   Interrupt:
	 *     - Tx frame count exceeds the frame count setting
	 *     - Addition of Tx frame count to the frame count since the
	 *       last interrupt was set exceeds the frame count setting
	 *   No interrupt:
	 *     - No frame count setting specified (ethtool -C ethX tx-frames 0)
	 *     - Addition of Tx frame count to the frame count since the
	 *       last interrupt was set does not exceed the frame count setting
	 */
	memset(packet, 0, sizeof(*packet));
	hlen = axgbe_calculate_tx_parms(pdata, pi, packet);
	axgbe_printf(1, "%s: ipi_len %d tx_pkts %d tx_bytes %d hlen %d\n",
	    __func__, pi->ipi_len, packet->tx_packets, packet->tx_bytes, hlen);

	ring->coalesce_count += packet->tx_packets;
	if (!pdata->tx_frames)
		tx_set_ic = 0;
	else if (packet->tx_packets > pdata->tx_frames)
		tx_set_ic = 1;
	else if ((ring->coalesce_count % pdata->tx_frames) < (packet->tx_packets))
		tx_set_ic = 1;
	else
		tx_set_ic = 0;

	/* Add Context descriptor if needed (for TSO, VLAN cases) */
	if (axgbe_ctx_desc_setup(pdata, ring, pi))
		cur++;

	rdata = XGBE_GET_DESC_DATA(ring, cur);
	rdesc = rdata->rdesc;

	axgbe_printf(1, "%s: cur %d lo 0x%lx hi 0x%lx ds_len 0x%x "
	    "ipi_len 0x%x\n", __func__, cur,
	    lower_32_bits(pi->ipi_segs[cur_seg].ds_addr),
	    upper_32_bits(pi->ipi_segs[cur_seg].ds_addr),
	    (int)pi->ipi_segs[cur_seg].ds_len, pi->ipi_len);

	/* Update buffer address (for TSO this is the header) */
	rdesc->desc0 = cpu_to_le32(lower_32_bits(pi->ipi_segs[cur_seg].ds_addr));
	rdesc->desc1 = cpu_to_le32(upper_32_bits(pi->ipi_segs[cur_seg].ds_addr));

	/* Update the buffer length */
	if (hlen == 0)
		hlen = pi->ipi_segs[cur_seg].ds_len;
	XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, HL_B1L, hlen);

	/* VLAN tag insertion check */
	if (pi->ipi_vtag) {
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, VTIR,
		    TX_NORMAL_DESC2_VLAN_INSERT);
	}

	/* Mark it as First Descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, FD, 1);

	/* Mark it as a NORMAL descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CTXT, 0);

	/* 
	 * Set the OWN bit if this is not the first descriptor. For first
	 * descriptor, OWN bit will be set at last so that hardware will
	 * process the descriptors only after the OWN bit for the first
	 * descriptor is set
	 */	
	if (cur != start)
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

	if (pi->ipi_csum_flags & CSUM_TSO) {
		/* Enable TSO */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TSE, 1);

		tcp_payload_len = pi->ipi_len - hlen;

		/* Set TCP payload length*/
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TCPPL,
		    tcp_payload_len);

		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, TCPHDRLEN,
		    pi->ipi_tcp_hlen/4);

		axgbe_printf(1, "tcp_payload %d tcp_hlen %d\n", tcp_payload_len,
		    pi->ipi_tcp_hlen/4);
	} else {
		/* Enable CRC and Pad Insertion */	
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CPC, 0);

		/* Enable HW CSUM*/
		if (pi->ipi_csum_flags)
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CIC, 0x3);

		/* Set total length to be transmitted */
		XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, FL, pi->ipi_len);
	}

	cur++;

	for (cur_seg = 0 ; cur_seg < pi->ipi_nsegs ; cur_seg++) {

		if (cur_seg == 0) {
			offset = hlen;
			datalen = pi->ipi_segs[cur_seg].ds_len - hlen;
		} else {
			offset = 0;
			datalen = pi->ipi_segs[cur_seg].ds_len;
		}

		if (datalen) {
			rdata = XGBE_GET_DESC_DATA(ring, cur);
			rdesc = rdata->rdesc;


			/* Update buffer address */
			rdesc->desc0 =
			    cpu_to_le32(lower_32_bits(pi->ipi_segs[cur_seg].ds_addr + offset));
			rdesc->desc1 =
			    cpu_to_le32(upper_32_bits(pi->ipi_segs[cur_seg].ds_addr + offset));

			/* Update the buffer length */
			XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, HL_B1L, datalen);

			/* Set OWN bit */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

			/* Mark it as NORMAL descriptor */
			XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CTXT, 0);

			/* Enable HW CSUM*/
			if (pi->ipi_csum_flags)
				XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, CIC, 0x3);
	
			axgbe_printf(1, "%s: cur %d lo 0x%lx hi 0x%lx ds_len 0x%x "
			    "ipi_len 0x%x\n", __func__, cur,
			    lower_32_bits(pi->ipi_segs[cur_seg].ds_addr),
			    upper_32_bits(pi->ipi_segs[cur_seg].ds_addr),
			    (int)pi->ipi_segs[cur_seg].ds_len, pi->ipi_len);

			cur++;
		}
	}

	/* Set LAST bit for the last descriptor */
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, LD, 1);

	/* Set IC bit based on Tx coalescing settings */
	if (tx_set_ic)
		XGMAC_SET_BITS_LE(rdesc->desc2, TX_NORMAL_DESC2, IC, 1);

	wmb();

	/* Set OWN bit for the first descriptor */
	rdata = XGBE_GET_DESC_DATA(ring, start);
	rdesc = rdata->rdesc;
	XGMAC_SET_BITS_LE(rdesc->desc3, TX_NORMAL_DESC3, OWN, 1);

	ring->cur = pi->ipi_new_pidx = (cur & (ring->rdesc_count - 1));

	axgbe_printf(1, "<-- %s: end cur %d dirty %d\n", __func__, ring->cur,
	    ring->dirty);

	return (0);
}

static void
axgbe_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx)
{
	struct axgbe_if_softc	*sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel	*channel = pdata->channel[txqid];
	struct xgbe_ring	*ring = channel->tx_ring;
	struct xgbe_ring_data	*rdata = XGBE_GET_DESC_DATA(ring, pidx);

	axgbe_printf(1, "--> %s: flush txq %d pidx %d cur %d dirty %d\n",
	    __func__, txqid, pidx, ring->cur, ring->dirty);

	/* Ring Doorbell */
	XGMAC_DMA_IOWRITE(channel, DMA_CH_TDTR_LO,
	    lower_32_bits(rdata->rdata_paddr));
}

static int
axgbe_isc_txd_credits_update(void *arg, uint16_t txqid, bool clear)
{
	struct axgbe_if_softc   *sc = (struct axgbe_if_softc*)arg;
	struct xgbe_hw_if	*hw_if = &sc->pdata.hw_if;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel     *channel = pdata->channel[txqid];
	struct xgbe_ring	*ring = channel->tx_ring;
	struct xgbe_ring_data	*rdata;
	int processed = 0;

	axgbe_printf(1, "%s: txq %d clear %d cur %d dirty %d\n",
	    __func__, txqid, clear, ring->cur, ring->dirty);

	if (__predict_false(ring->cur == ring->dirty)) {
		axgbe_printf(1, "<-- %s: cur(%d) equals dirty(%d)\n",
		    __func__, ring->cur, ring->dirty);
		return (0);
	}

	/* Check whether the first dirty descriptor is Tx complete */
	rdata = XGBE_GET_DESC_DATA(ring, ring->dirty);
	if (!hw_if->tx_complete(rdata->rdesc)) {
		axgbe_printf(1, "<-- %s: (dirty %d)\n", __func__, ring->dirty);
		return (0);
	}

	/*
	 * If clear is false just let the caller know that there
	 * are descriptors to reclaim
	 */
	if (!clear) {
		axgbe_printf(1, "<-- %s: (!clear)\n", __func__);
		return (1);
	}

	do {
		hw_if->tx_desc_reset(rdata);
		processed++;
		ring->dirty = (ring->dirty + 1) & (ring->rdesc_count - 1);

		/*
		 * tx_complete will return true for unused descriptors also.
		 * so, check tx_complete only until used descriptors.
		 */
		if (ring->cur == ring->dirty)
			break;

		rdata = XGBE_GET_DESC_DATA(ring, ring->dirty);
	} while (hw_if->tx_complete(rdata->rdesc));

	axgbe_printf(1, "<-- %s: processed %d cur %d dirty %d\n", __func__,
	    processed, ring->cur, ring->dirty);

	return (processed);
}

static void
axgbe_isc_rxd_refill(void *arg, if_rxd_update_t iru)
{
 	struct axgbe_if_softc   *sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel     *channel = pdata->channel[iru->iru_qsidx];
	struct xgbe_ring	*ring = channel->rx_ring;
	struct xgbe_ring_data	*rdata;
	struct xgbe_ring_desc	*rdesc;
	unsigned int rx_usecs = pdata->rx_usecs;
	unsigned int rx_frames = pdata->rx_frames;
	unsigned int inte;
	uint8_t	count = iru->iru_count;
	int i, j;
	bool config_intr = false;

	axgbe_printf(1, "--> %s: rxq %d fl %d pidx %d count %d ring cur %d "
	    "dirty %d\n", __func__, iru->iru_qsidx, iru->iru_flidx,
	    iru->iru_pidx, count, ring->cur, ring->dirty);

	for (i = iru->iru_pidx, j = 0 ; j < count ; i++, j++) {

		if (i == sc->scctx->isc_nrxd[0])
			i = 0;

		rdata = XGBE_GET_DESC_DATA(ring, i);
		rdesc = rdata->rdesc;

		if (__predict_false(XGMAC_GET_BITS_LE(rdesc->desc3,
		    RX_NORMAL_DESC3, OWN))) {
			axgbe_error("%s: refill clash, cur %d dirty %d index %d"
			    "pidx %d\n", __func__, ring->cur, ring->dirty, j, i);
		}

		if (pdata->sph_enable) {
			if (iru->iru_flidx == 0) {

				/* Fill header/buffer1 address */
				rdesc->desc0 =
				    cpu_to_le32(lower_32_bits(iru->iru_paddrs[j]));
				rdesc->desc1 =
				    cpu_to_le32(upper_32_bits(iru->iru_paddrs[j]));
			} else {

				/* Fill data/buffer2 address */
				rdesc->desc2 =
				    cpu_to_le32(lower_32_bits(iru->iru_paddrs[j]));
				rdesc->desc3 =
				    cpu_to_le32(upper_32_bits(iru->iru_paddrs[j]));

				config_intr = true;
			}
		} else {
			/* Fill header/buffer1 address */
			rdesc->desc0 = rdesc->desc2 =
			    cpu_to_le32(lower_32_bits(iru->iru_paddrs[j]));
			rdesc->desc1 = rdesc->desc3 =
			    cpu_to_le32(upper_32_bits(iru->iru_paddrs[j]));

			config_intr = true;
		}

		if (config_intr) {

			if (!rx_usecs && !rx_frames) {
				/* No coalescing, interrupt for every descriptor */
				inte = 1;
			} else {
				/* Set interrupt based on Rx frame coalescing setting */
				if (rx_frames && !((ring->dirty + 1) % rx_frames))
					inte = 1;
				else
					inte = 0;
			}

			XGMAC_SET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, INTE, inte);

			XGMAC_SET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, OWN, 1);

			wmb();

			ring->dirty = ((ring->dirty + 1) & (ring->rdesc_count - 1));

			config_intr = false;
		}
	}

	axgbe_printf(1, "<-- %s: rxq: %d cur: %d dirty: %d\n", __func__,
	    channel->queue_index, ring->cur, ring->dirty);
}

static void
axgbe_isc_rxd_flush(void *arg, uint16_t qsidx, uint8_t flidx, qidx_t pidx)
{
 	struct axgbe_if_softc   *sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data	*pdata = &sc->pdata;
	struct xgbe_channel     *channel = pdata->channel[qsidx];
	struct xgbe_ring	*ring = channel->rx_ring;
	struct xgbe_ring_data 	*rdata;

	axgbe_printf(1, "--> %s: rxq %d fl %d pidx %d cur %d dirty %d\n",
	    __func__, qsidx, flidx, pidx, ring->cur, ring->dirty);

	rdata = XGBE_GET_DESC_DATA(ring, pidx);

	/*
	 * update RX descriptor tail pointer in hardware to indicate
	 * that new buffers are present in the allocated memory region
	 */
	if (!pdata->sph_enable || flidx == 1) {
		XGMAC_DMA_IOWRITE(channel, DMA_CH_RDTR_LO,
		    lower_32_bits(rdata->rdata_paddr));
	}
}

static int
axgbe_isc_rxd_available(void *arg, uint16_t qsidx, qidx_t idx, qidx_t budget)
{
	struct axgbe_if_softc   *sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	struct xgbe_channel     *channel = pdata->channel[qsidx];
	struct xgbe_ring	*ring = channel->rx_ring;
	struct xgbe_ring_data   *rdata;
	struct xgbe_ring_desc   *rdesc;
	unsigned int cur;
	int count = 0;
	uint8_t incomplete = 1, context_next = 0, running = 0;

	axgbe_printf(1, "--> %s: rxq %d idx %d budget %d cur %d dirty %d\n",
	    __func__, qsidx, idx, budget, ring->cur, ring->dirty);

	if (__predict_false(test_bit(XGBE_DOWN, &pdata->dev_state))) {
		axgbe_printf(0, "%s: Polling when XGBE_DOWN\n", __func__);
		return (count);
	}

	cur = ring->cur;
	for (count = 0; count <= budget; ) {

		rdata = XGBE_GET_DESC_DATA(ring, cur);
		rdesc = rdata->rdesc;

		if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, OWN))
			break;

		running = 1;

		if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, LD))
			incomplete = 0;

		if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, CDA))
			context_next = 1;

		if (XGMAC_GET_BITS_LE(rdesc->desc3, RX_NORMAL_DESC3, CTXT))
			context_next = 0;

		cur = (cur + 1) & (ring->rdesc_count - 1);

		if (incomplete || context_next)
			continue;

		/* Increment pkt count & reset variables for next full packet */
		count++;
		incomplete = 1;
		context_next = 0;
		running = 0;
	}

	axgbe_printf(1, "--> %s: rxq %d cur %d incomp %d con_next %d running %d "
	    "count %d\n", __func__, qsidx, cur, incomplete, context_next,
	    running, count);

	return (count);
}

static unsigned int
xgbe_rx_buf1_len(struct xgbe_prv_data *pdata, struct xgbe_ring_data *rdata,
    struct xgbe_packet_data *packet)
{
	unsigned int ret = 0;

	if (pdata->sph_enable) {
		/* Always zero if not the first descriptor */
		if (!XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, FIRST)) {
			axgbe_printf(1, "%s: Not First\n", __func__);
			return (0);
		}
	}

	/* First descriptor with split header, return header length */
	if (rdata->rx.hdr_len) {
		axgbe_printf(1, "%s: hdr_len %d\n", __func__, rdata->rx.hdr_len);
		return (rdata->rx.hdr_len);
	}

	/* First descriptor but not the last descriptor and no split header,
	 * so the full buffer was used, 256 represents the hardcoded value of
	 * a max header split defined in the hardware
	 */
	if (!XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, LAST)) {
		axgbe_printf(1, "%s: Not last %d\n", __func__,
		    pdata->rx_buf_size);
		if (pdata->sph_enable) {
			return (256);
		} else {
			return (pdata->rx_buf_size);
		}
	}

	/* First descriptor and last descriptor and no split header, so
	 * calculate how much of the buffer was used, we can return the
	 * segment length or the remaining bytes of the packet
	 */
	axgbe_printf(1, "%s: pkt_len %d buf_size %d\n", __func__, rdata->rx.len,
	    pdata->rx_buf_size);

	if (pdata->sph_enable) {
		ret = min_t(unsigned int, 256, rdata->rx.len);
	} else {
		ret = rdata->rx.len;
	}

	return (ret);
}

static unsigned int
xgbe_rx_buf2_len(struct xgbe_prv_data *pdata, struct xgbe_ring_data *rdata,
    struct xgbe_packet_data *packet, unsigned int len)
{

	/* Always the full buffer if not the last descriptor */
	if (!XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, LAST)) {
		axgbe_printf(1, "%s: Not last %d\n", __func__, pdata->rx_buf_size);
		return (pdata->rx_buf_size);
	}

	/* Last descriptor so calculate how much of the buffer was used
	 * for the last bit of data
	 */
	return ((rdata->rx.len != 0)? (rdata->rx.len - len) : 0);
}

static inline void
axgbe_add_frag(struct xgbe_prv_data *pdata, if_rxd_info_t ri, int idx, int len,
    int pos, int flid)
{
	axgbe_printf(2, "idx %d len %d pos %d flid %d\n", idx, len, pos, flid);
	ri->iri_frags[pos].irf_flid = flid;
	ri->iri_frags[pos].irf_idx = idx;
	ri->iri_frags[pos].irf_len = len;
}

static int
axgbe_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri)
{
 	struct axgbe_if_softc   *sc = (struct axgbe_if_softc*)arg;
	struct xgbe_prv_data 	*pdata = &sc->pdata;
	struct xgbe_hw_if	*hw_if = &pdata->hw_if;
	struct xgbe_channel     *channel = pdata->channel[ri->iri_qsidx];
	struct xgbe_ring	*ring = channel->rx_ring;
	struct xgbe_packet_data *packet = &ring->packet_data;
	struct xgbe_ring_data	*rdata;
	unsigned int last, context_next, context;
	unsigned int buf1_len, buf2_len, max_len, len = 0, prev_cur;
	int i = 0;

	axgbe_printf(2, "%s: rxq %d cidx %d cur %d dirty %d\n", __func__,
	    ri->iri_qsidx, ri->iri_cidx, ring->cur, ring->dirty);

	memset(packet, 0, sizeof(struct xgbe_packet_data));

	while (1) {

read_again:
		if (hw_if->dev_read(channel)) {
			axgbe_printf(2, "<-- %s: OWN bit seen on %d\n",
		    	    __func__, ring->cur);
			break;
		}

		rdata = XGBE_GET_DESC_DATA(ring, ring->cur);
		prev_cur = ring->cur;
		ring->cur = (ring->cur + 1) & (ring->rdesc_count - 1);

		last = XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES,
		    LAST);

		context_next = XGMAC_GET_BITS(packet->attributes,
		    RX_PACKET_ATTRIBUTES, CONTEXT_NEXT);

		context = XGMAC_GET_BITS(packet->attributes,
		    RX_PACKET_ATTRIBUTES, CONTEXT);

		if (!context) {
			/* Get the data length in the descriptor buffers */
			buf1_len = xgbe_rx_buf1_len(pdata, rdata, packet);
			len += buf1_len;
			if (pdata->sph_enable) {
				buf2_len = xgbe_rx_buf2_len(pdata, rdata, packet, len);
				len += buf2_len;
			}
		} else
			buf1_len = buf2_len = 0;

		if (packet->errors)
			axgbe_printf(1, "%s: last %d context %d con_next %d buf1 %d "
			    "buf2 %d len %d frags %d error %d\n", __func__, last, context,
			    context_next, buf1_len, buf2_len, len, i, packet->errors);

		axgbe_add_frag(pdata, ri, prev_cur, buf1_len, i, 0);
		i++;
		if (pdata->sph_enable) {
			axgbe_add_frag(pdata, ri, prev_cur, buf2_len, i, 1);
			i++;
		}

		if (!last || context_next)
			goto read_again;

		break;
	}

	if (XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, CSUM_DONE)) {
		ri->iri_csum_flags |= CSUM_IP_CHECKED;
		ri->iri_csum_flags |= CSUM_IP_VALID;
		axgbe_printf(2, "%s: csum flags 0x%x\n", __func__, ri->iri_csum_flags);
	}

	max_len = if_getmtu(pdata->netdev) + ETH_HLEN;
	if (XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, VLAN_CTAG)) {
		ri->iri_flags |= M_VLANTAG;
		ri->iri_vtag = packet->vlan_ctag;
		max_len += VLAN_HLEN;
		axgbe_printf(2, "%s: iri_flags 0x%x vtag 0x%x\n", __func__,
		    ri->iri_flags, ri->iri_vtag);
	}


	if (XGMAC_GET_BITS(packet->attributes, RX_PACKET_ATTRIBUTES, RSS_HASH)) {
		ri->iri_flowid = packet->rss_hash;
		ri->iri_rsstype = packet->rss_hash_type;
		axgbe_printf(2, "%s: hash 0x%x/0x%x rsstype 0x%x/0x%x\n",
		    __func__, packet->rss_hash, ri->iri_flowid,
		    packet->rss_hash_type, ri->iri_rsstype);
	}

	if (__predict_false(len == 0))
		axgbe_printf(1, "%s: Discarding Zero len packet\n", __func__);

	if (__predict_false(len > max_len))
		axgbe_error("%s: Big packet %d/%d\n", __func__, len, max_len);

	if (__predict_false(packet->errors))
		axgbe_printf(1, "<-- %s: rxq: %d len: %d frags: %d cidx %d cur: %d "
		    "dirty: %d error 0x%x\n", __func__, ri->iri_qsidx, len, i,
		    ri->iri_cidx, ring->cur, ring->dirty, packet->errors);

	axgbe_printf(1, "%s: Packet len %d frags %d\n", __func__, len, i);

	ri->iri_len = len;
	ri->iri_nfrags = i;

	return (0);
}
