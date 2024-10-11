/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2016 Matthew Macy <mmacy@mattmacy.io>
 * All rights reserved.
 * Copyright (c) 2021 Rubicon Communications, LLC (Netgate)
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
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "if_igc.h"

#ifdef RSS
#include <net/rss_config.h>
#include <netinet/in_rss.h>
#endif

#ifdef VERBOSE_DEBUG
#define DPRINTF device_printf
#else
#define DPRINTF(...)
#endif

/*********************************************************************
 *  Local Function prototypes
 *********************************************************************/
static int igc_isc_txd_encap(void *arg, if_pkt_info_t pi);
static void igc_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx);
static int igc_isc_txd_credits_update(void *arg, uint16_t txqid, bool clear);

static void igc_isc_rxd_refill(void *arg, if_rxd_update_t iru);

static void igc_isc_rxd_flush(void *arg, uint16_t rxqid, uint8_t flid __unused,
    qidx_t pidx);
static int igc_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx,
    qidx_t budget);

static int igc_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri);

static int igc_tx_ctx_setup(struct tx_ring *txr, if_pkt_info_t pi,
    uint32_t *cmd_type_len, uint32_t *olinfo_status);
static int igc_tso_setup(struct tx_ring *txr, if_pkt_info_t pi,
    uint32_t *cmd_type_len, uint32_t *olinfo_status);

static void igc_rx_checksum(uint32_t staterr, if_rxd_info_t ri, uint32_t ptype);
static int igc_determine_rsstype(uint16_t pkt_info);

extern void igc_if_enable_intr(if_ctx_t ctx);
extern int igc_intr(void *arg);

struct if_txrx igc_txrx = {
	.ift_txd_encap = igc_isc_txd_encap,
	.ift_txd_flush = igc_isc_txd_flush,
	.ift_txd_credits_update = igc_isc_txd_credits_update,
	.ift_rxd_available = igc_isc_rxd_available,
	.ift_rxd_pkt_get = igc_isc_rxd_pkt_get,
	.ift_rxd_refill = igc_isc_rxd_refill,
	.ift_rxd_flush = igc_isc_rxd_flush,
	.ift_legacy_intr = igc_intr
};

void
igc_dump_rs(struct igc_adapter *adapter)
{
	if_softc_ctx_t scctx = adapter->shared;
	struct igc_tx_queue *que;
	struct tx_ring *txr;
	qidx_t i, ntxd, qid, cur;
	int16_t rs_cidx;
	uint8_t status;

	printf("\n");
	ntxd = scctx->isc_ntxd[0];
	for (qid = 0; qid < adapter->tx_num_queues; qid++) {
		que = &adapter->tx_queues[qid];
		txr =  &que->txr;
		rs_cidx = txr->tx_rs_cidx;
		if (rs_cidx != txr->tx_rs_pidx) {
			cur = txr->tx_rsq[rs_cidx];
			status = txr->tx_base[cur].upper.fields.status;
			if (!(status & IGC_TXD_STAT_DD))
				printf("qid[%d]->tx_rsq[%d]: %d clear ", qid, rs_cidx, cur);
		} else {
			rs_cidx = (rs_cidx-1)&(ntxd-1);
			cur = txr->tx_rsq[rs_cidx];
			printf("qid[%d]->tx_rsq[rs_cidx-1=%d]: %d  ", qid, rs_cidx, cur);
		}
		printf("cidx_prev=%d rs_pidx=%d ",txr->tx_cidx_processed, txr->tx_rs_pidx);
		for (i = 0; i < ntxd; i++) {
			if (txr->tx_base[i].upper.fields.status & IGC_TXD_STAT_DD)
				printf("%d set ", i);
		}
		printf("\n");
	}
}

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  adapters using advanced tx descriptors
 *
 **********************************************************************/
static int
igc_tso_setup(struct tx_ring *txr, if_pkt_info_t pi, uint32_t *cmd_type_len,
    uint32_t *olinfo_status)
{
	struct igc_adv_tx_context_desc *TXD;
	uint32_t type_tucmd_mlhl = 0, vlan_macip_lens = 0;
	uint32_t mss_l4len_idx = 0;
	uint32_t paylen;

	switch(pi->ipi_etype) {
	case ETHERTYPE_IPV6:
		type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_IPV6;
		break;
	case ETHERTYPE_IP:
		type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_IPV4;
		/* Tell transmit desc to also do IPv4 checksum. */
		*olinfo_status |= IGC_TXD_POPTS_IXSM << 8;
		break;
	default:
		panic("%s: CSUM_TSO but no supported IP version (0x%04x)",
		      __func__, ntohs(pi->ipi_etype));
		break;
	}

	TXD = (struct igc_adv_tx_context_desc *) &txr->tx_base[pi->ipi_pidx];

	/* This is used in the transmit desc in encap */
	paylen = pi->ipi_len - pi->ipi_ehdrlen - pi->ipi_ip_hlen - pi->ipi_tcp_hlen;

	/* VLAN MACLEN IPLEN */
	if (pi->ipi_mflags & M_VLANTAG) {
		vlan_macip_lens |= (pi->ipi_vtag << IGC_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= pi->ipi_ehdrlen << IGC_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= pi->ipi_ip_hlen;
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IGC_ADVTXD_DCMD_DEXT | IGC_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_L4T_TCP;
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);

	/* MSS L4LEN IDX */
	mss_l4len_idx |= (pi->ipi_tso_segsz << IGC_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (pi->ipi_tcp_hlen << IGC_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);
	*cmd_type_len |= IGC_ADVTXD_DCMD_TSE;
	*olinfo_status |= IGC_TXD_POPTS_TXSM << 8;
	*olinfo_status |= paylen << IGC_ADVTXD_PAYLEN_SHIFT;

	return (1);
}

/*********************************************************************
 *
 *  Advanced Context Descriptor setup for VLAN, CSUM or TSO
 *
 **********************************************************************/
static int
igc_tx_ctx_setup(struct tx_ring *txr, if_pkt_info_t pi, uint32_t *cmd_type_len,
    uint32_t *olinfo_status)
{
	struct igc_adv_tx_context_desc *TXD;
	uint32_t vlan_macip_lens, type_tucmd_mlhl;
	uint32_t mss_l4len_idx;
	mss_l4len_idx = vlan_macip_lens = type_tucmd_mlhl = 0;

	/* First check if TSO is to be used */
	if (pi->ipi_csum_flags & CSUM_TSO)
		return (igc_tso_setup(txr, pi, cmd_type_len, olinfo_status));

	/* Indicate the whole packet as payload when not doing TSO */
	*olinfo_status |= pi->ipi_len << IGC_ADVTXD_PAYLEN_SHIFT;

	/* Now ready a context descriptor */
	TXD = (struct igc_adv_tx_context_desc *) &txr->tx_base[pi->ipi_pidx];

	/*
	** In advanced descriptors the vlan tag must
	** be placed into the context descriptor. Hence
	** we need to make one even if not doing offloads.
	*/
	if (pi->ipi_mflags & M_VLANTAG) {
		vlan_macip_lens |= (pi->ipi_vtag << IGC_ADVTXD_VLAN_SHIFT);
	} else if ((pi->ipi_csum_flags & IGC_CSUM_OFFLOAD) == 0) {
		return (0);
	}

	/* Set the ether header length */
	vlan_macip_lens |= pi->ipi_ehdrlen << IGC_ADVTXD_MACLEN_SHIFT;

	switch(pi->ipi_etype) {
	case ETHERTYPE_IP:
		type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_IPV4;
		break;
	case ETHERTYPE_IPV6:
		type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_IPV6;
		break;
	default:
		break;
	}

	vlan_macip_lens |= pi->ipi_ip_hlen;
	type_tucmd_mlhl |= IGC_ADVTXD_DCMD_DEXT | IGC_ADVTXD_DTYP_CTXT;

	switch (pi->ipi_ipproto) {
	case IPPROTO_TCP:
		if (pi->ipi_csum_flags & (CSUM_IP_TCP | CSUM_IP6_TCP)) {
			type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_L4T_TCP;
			*olinfo_status |= IGC_TXD_POPTS_TXSM << 8;
		}
		break;
	case IPPROTO_UDP:
		if (pi->ipi_csum_flags & (CSUM_IP_UDP | CSUM_IP6_UDP)) {
			type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_L4T_UDP;
			*olinfo_status |= IGC_TXD_POPTS_TXSM << 8;
		}
		break;
	case IPPROTO_SCTP:
		if (pi->ipi_csum_flags & (CSUM_IP_SCTP | CSUM_IP6_SCTP)) {
			type_tucmd_mlhl |= IGC_ADVTXD_TUCMD_L4T_SCTP;
			*olinfo_status |= IGC_TXD_POPTS_TXSM << 8;
		}
		break;
	default:
		break;
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens = htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl = htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	return (1);
}

static int
igc_isc_txd_encap(void *arg, if_pkt_info_t pi)
{
	struct igc_adapter *sc = arg;
	if_softc_ctx_t scctx = sc->shared;
	struct igc_tx_queue *que = &sc->tx_queues[pi->ipi_qsidx];
	struct tx_ring *txr = &que->txr;
	int nsegs = pi->ipi_nsegs;
	bus_dma_segment_t *segs = pi->ipi_segs;
	union igc_adv_tx_desc *txd = NULL;
	int i, j, pidx_last;
	uint32_t olinfo_status, cmd_type_len, txd_flags;
	qidx_t ntxd;

	pidx_last = olinfo_status = 0;
	/* Basic descriptor defines */
	cmd_type_len = (IGC_ADVTXD_DTYP_DATA |
			IGC_ADVTXD_DCMD_IFCS | IGC_ADVTXD_DCMD_DEXT);

	if (pi->ipi_mflags & M_VLANTAG)
		cmd_type_len |= IGC_ADVTXD_DCMD_VLE;

	i = pi->ipi_pidx;
	ntxd = scctx->isc_ntxd[0];
	txd_flags = pi->ipi_flags & IPI_TX_INTR ? IGC_ADVTXD_DCMD_RS : 0;
	/* Consume the first descriptor */
	i += igc_tx_ctx_setup(txr, pi, &cmd_type_len, &olinfo_status);
	if (i == scctx->isc_ntxd[0])
		i = 0;

	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;
		bus_addr_t segaddr;

		txd = (union igc_adv_tx_desc *)&txr->tx_base[i];
		seglen = segs[j].ds_len;
		segaddr = htole64(segs[j].ds_addr);

		txd->read.buffer_addr = segaddr;
		txd->read.cmd_type_len = htole32(IGC_ADVTXD_DCMD_IFCS |
		    cmd_type_len | seglen);
		txd->read.olinfo_status = htole32(olinfo_status);
		pidx_last = i;
		if (++i == scctx->isc_ntxd[0]) {
			i = 0;
		}
	}
	if (txd_flags) {
		txr->tx_rsq[txr->tx_rs_pidx] = pidx_last;
		txr->tx_rs_pidx = (txr->tx_rs_pidx+1) & (ntxd-1);
		MPASS(txr->tx_rs_pidx != txr->tx_rs_cidx);
	}

	txd->read.cmd_type_len |= htole32(IGC_ADVTXD_DCMD_EOP | txd_flags);
	pi->ipi_new_pidx = i;

	/* Sent data accounting for AIM */
	txr->tx_bytes += pi->ipi_len;
	++txr->tx_packets;

	return (0);
}

static void
igc_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx)
{
	struct igc_adapter *adapter	= arg;
	struct igc_tx_queue *que	= &adapter->tx_queues[txqid];
	struct tx_ring *txr	= &que->txr;

	IGC_WRITE_REG(&adapter->hw, IGC_TDT(txr->me), pidx);
}

static int
igc_isc_txd_credits_update(void *arg, uint16_t txqid, bool clear)
{
	struct igc_adapter *adapter = arg;
	if_softc_ctx_t scctx = adapter->shared;
	struct igc_tx_queue *que = &adapter->tx_queues[txqid];
	struct tx_ring *txr = &que->txr;

	qidx_t processed = 0;
	int updated;
	qidx_t cur, prev, ntxd, rs_cidx;
	int32_t delta;
	uint8_t status;

	rs_cidx = txr->tx_rs_cidx;
	if (rs_cidx == txr->tx_rs_pidx)
		return (0);
	cur = txr->tx_rsq[rs_cidx];
	status = ((union igc_adv_tx_desc *)&txr->tx_base[cur])->wb.status;
	updated = !!(status & IGC_TXD_STAT_DD);

	if (!updated)
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
		prev  = cur;
		rs_cidx = (rs_cidx + 1) & (ntxd-1);
		if (rs_cidx  == txr->tx_rs_pidx)
			break;
		cur = txr->tx_rsq[rs_cidx];
		status = ((union igc_adv_tx_desc *)&txr->tx_base[cur])->wb.status;
	} while ((status & IGC_TXD_STAT_DD));

	txr->tx_rs_cidx = rs_cidx;
	txr->tx_cidx_processed = prev;
	return (processed);
}

static void
igc_isc_rxd_refill(void *arg, if_rxd_update_t iru)
{
	struct igc_adapter *sc = arg;
	if_softc_ctx_t scctx = sc->shared;
	uint16_t rxqid = iru->iru_qsidx;
	struct igc_rx_queue *que = &sc->rx_queues[rxqid];
	union igc_adv_rx_desc *rxd;
	struct rx_ring *rxr = &que->rxr;
	uint64_t *paddrs;
	uint32_t next_pidx, pidx;
	uint16_t count;
	int i;

	paddrs = iru->iru_paddrs;
	pidx = iru->iru_pidx;
	count = iru->iru_count;

	for (i = 0, next_pidx = pidx; i < count; i++) {
		rxd = (union igc_adv_rx_desc *)&rxr->rx_base[next_pidx];

		rxd->read.pkt_addr = htole64(paddrs[i]);
		if (++next_pidx == scctx->isc_nrxd[0])
			next_pidx = 0;
	}
}

static void
igc_isc_rxd_flush(void *arg, uint16_t rxqid, uint8_t flid __unused, qidx_t pidx)
{
	struct igc_adapter *sc = arg;
	struct igc_rx_queue *que = &sc->rx_queues[rxqid];
	struct rx_ring *rxr = &que->rxr;

	IGC_WRITE_REG(&sc->hw, IGC_RDT(rxr->me), pidx);
}

static int
igc_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx, qidx_t budget)
{
	struct igc_adapter *sc = arg;
	if_softc_ctx_t scctx = sc->shared;
	struct igc_rx_queue *que = &sc->rx_queues[rxqid];
	struct rx_ring *rxr = &que->rxr;
	union igc_adv_rx_desc *rxd;
	uint32_t staterr = 0;
	int cnt, i;

	for (cnt = 0, i = idx; cnt < scctx->isc_nrxd[0] && cnt <= budget;) {
		rxd = (union igc_adv_rx_desc *)&rxr->rx_base[i];
		staterr = le32toh(rxd->wb.upper.status_error);

		if ((staterr & IGC_RXD_STAT_DD) == 0)
			break;
		if (++i == scctx->isc_nrxd[0])
			i = 0;
		if (staterr & IGC_RXD_STAT_EOP)
			cnt++;
	}
	return (cnt);
}

/****************************************************************
 * Routine sends data which has been dma'ed into host memory
 * to upper layer. Initialize ri structure.
 *
 * Returns 0 upon success, errno on failure
 ***************************************************************/

static int
igc_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri)
{
	struct igc_adapter *adapter = arg;
	if_softc_ctx_t scctx = adapter->shared;
	struct igc_rx_queue *que = &adapter->rx_queues[ri->iri_qsidx];
	struct rx_ring *rxr = &que->rxr;
	union igc_adv_rx_desc *rxd;

	uint16_t pkt_info, len;
	uint32_t ptype, staterr;
	int i, cidx;
	bool eop;

	staterr = i = 0;
	cidx = ri->iri_cidx;

	do {
		rxd = (union igc_adv_rx_desc *)&rxr->rx_base[cidx];
		staterr = le32toh(rxd->wb.upper.status_error);
		pkt_info = le16toh(rxd->wb.lower.lo_dword.hs_rss.pkt_info);

		MPASS ((staterr & IGC_RXD_STAT_DD) != 0);

		len = le16toh(rxd->wb.upper.length);
		ptype = le32toh(rxd->wb.lower.lo_dword.data) &  IGC_PKTTYPE_MASK;

		ri->iri_len += len;
		rxr->rx_bytes += ri->iri_len;

		rxd->wb.upper.status_error = 0;
		eop = ((staterr & IGC_RXD_STAT_EOP) == IGC_RXD_STAT_EOP);

		/* Make sure bad packets are discarded */
		if (eop && ((staterr & IGC_RXDEXT_STATERR_RXE) != 0)) {
			adapter->dropped_pkts++;
			++rxr->rx_discarded;
			return (EBADMSG);
		}
		ri->iri_frags[i].irf_flid = 0;
		ri->iri_frags[i].irf_idx = cidx;
		ri->iri_frags[i].irf_len = len;

		if (++cidx == scctx->isc_nrxd[0])
			cidx = 0;
#ifdef notyet
		if (rxr->hdr_split == true) {
			ri->iri_frags[i].irf_flid = 1;
			ri->iri_frags[i].irf_idx = cidx;
			if (++cidx == scctx->isc_nrxd[0])
				cidx = 0;
		}
#endif
		i++;
	} while (!eop);

	rxr->rx_packets++;

	if ((scctx->isc_capenable & IFCAP_RXCSUM) != 0)
		igc_rx_checksum(staterr, ri, ptype);

	if (staterr & IGC_RXD_STAT_VP) {
		ri->iri_vtag = le16toh(rxd->wb.upper.vlan);
		ri->iri_flags |= M_VLANTAG;
	}

	ri->iri_flowid =
		le32toh(rxd->wb.lower.hi_dword.rss);
	ri->iri_rsstype = igc_determine_rsstype(pkt_info);
	ri->iri_nfrags = i;

	return (0);
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
igc_rx_checksum(uint32_t staterr, if_rxd_info_t ri, uint32_t ptype)
{
	uint16_t status = (uint16_t)staterr;
	uint8_t errors = (uint8_t)(staterr >> 24);

	if (__predict_false(status & IGC_RXD_STAT_IXSM))
		return;

	/* If there is a layer 3 or 4 error we are done */
	if (__predict_false(errors & (IGC_RXD_ERR_IPE | IGC_RXD_ERR_TCPE)))
		return;

	/* IP Checksum Good */
	if (status & IGC_RXD_STAT_IPCS)
		ri->iri_csum_flags = (CSUM_IP_CHECKED | CSUM_IP_VALID);

	/* Valid L4E checksum */
	if (__predict_true(status &
	    (IGC_RXD_STAT_TCPCS | IGC_RXD_STAT_UDPCS))) {
		/* SCTP header present */
		if (__predict_false((ptype & IGC_RXDADV_PKTTYPE_ETQF) == 0 &&
		    (ptype & IGC_RXDADV_PKTTYPE_SCTP) != 0)) {
			ri->iri_csum_flags |= CSUM_SCTP_VALID;
		} else {
			ri->iri_csum_flags |= CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			ri->iri_csum_data = htons(0xffff);
		}
	}
}

/********************************************************************
 *
 *  Parse the packet type to determine the appropriate hash
 *
 ******************************************************************/
static int
igc_determine_rsstype(uint16_t pkt_info)
{
	switch (pkt_info & IGC_RXDADV_RSSTYPE_MASK) {
	case IGC_RXDADV_RSSTYPE_IPV4_TCP:
		return M_HASHTYPE_RSS_TCP_IPV4;
	case IGC_RXDADV_RSSTYPE_IPV4:
		return M_HASHTYPE_RSS_IPV4;
	case IGC_RXDADV_RSSTYPE_IPV6_TCP:
		return M_HASHTYPE_RSS_TCP_IPV6;
	case IGC_RXDADV_RSSTYPE_IPV6_EX:
		return M_HASHTYPE_RSS_IPV6_EX;
	case IGC_RXDADV_RSSTYPE_IPV6:
		return M_HASHTYPE_RSS_IPV6;
	case IGC_RXDADV_RSSTYPE_IPV6_TCP_EX:
		return M_HASHTYPE_RSS_TCP_IPV6_EX;
	default:
		return M_HASHTYPE_OPAQUE;
	}
}
