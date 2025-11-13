/*
 * aQuantia Corporation Network Driver
 * Copyright (C) 2014-2018 aQuantia Corporation. All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   (1) Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer.
 *
 *   (2) Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the following
 *   disclaimer in the documentation and/or other materials provided
 *   with the distribution.
 *
 *   (3)The name of the author may not be used to endorse or promote
 *   products derived from this software without specific prior
 *   written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <machine/param.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/bitstring.h>
#include <net/if.h>
#include <net/if_media.h>
#include <net/if_var.h>
#include <net/if_dl.h>
#include <net/ethernet.h>
#include <net/iflib.h>
#include <netinet/in.h>

#include "aq_common.h"

#include "aq_ring.h"
#include "aq_dbg.h"
#include "aq_device.h"
#include "aq_hw.h"
#include "aq_hw_llh.h"

/* iflib txrx interface prototypes */
static int aq_isc_txd_encap(void *arg, if_pkt_info_t pi);
static void aq_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx);
static int aq_isc_txd_credits_update(void *arg, uint16_t txqid, bool clear);
static void aq_ring_rx_refill(void* arg, if_rxd_update_t iru);
static void aq_isc_rxd_flush(void *arg, uint16_t rxqid, uint8_t flid __unused, qidx_t pidx);
static int aq_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx, qidx_t budget);
static int aq_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri);

struct if_txrx aq_txrx = {
	.ift_txd_encap = aq_isc_txd_encap,
	.ift_txd_flush = aq_isc_txd_flush,
	.ift_txd_credits_update = aq_isc_txd_credits_update,
	.ift_rxd_available = aq_isc_rxd_available,
	.ift_rxd_pkt_get = aq_isc_rxd_pkt_get,
	.ift_rxd_refill = aq_ring_rx_refill,
	.ift_rxd_flush = aq_isc_rxd_flush,
	.ift_legacy_intr = NULL
};


static inline uint32_t
aq_next(uint32_t i, uint32_t lim)
{
    return (i == lim) ? 0 : i + 1;
}

int aq_ring_rx_init(struct aq_hw *hw, struct aq_ring *ring)
/*                     uint64_t ring_addr,
                     u32 ring_size,
                     u32 ring_idx,
                     u32 interrupt_cause,
                     u32 cpu_idx) */
{
    int err;
    u32 dma_desc_addr_lsw = (u32)ring->rx_descs_phys & 0xffffffff;
    u32 dma_desc_addr_msw = (u32)(ring->rx_descs_phys >> 32);

    AQ_DBG_ENTERA("[%d]", ring->index);

    rdm_rx_desc_en_set(hw, false, ring->index);

    rdm_rx_desc_head_splitting_set(hw, 0U, ring->index);

    reg_rx_dma_desc_base_addresslswset(hw, dma_desc_addr_lsw, ring->index);

    reg_rx_dma_desc_base_addressmswset(hw, dma_desc_addr_msw, ring->index);

    rdm_rx_desc_len_set(hw, ring->rx_size / 8U, ring->index);

    device_printf(ring->dev->dev, "ring %d: __PAGESIZE=%d MCLBYTES=%d hw->max_frame_size=%d\n",
				  ring->index, PAGE_SIZE, MCLBYTES, ring->rx_max_frame_size);
    rdm_rx_desc_data_buff_size_set(hw, ring->rx_max_frame_size / 1024U, ring->index);

    rdm_rx_desc_head_buff_size_set(hw, 0U, ring->index);
    rdm_rx_desc_head_splitting_set(hw, 0U, ring->index);
    rpo_rx_desc_vlan_stripping_set(hw, 0U, ring->index);

    /* Rx ring set mode */

    /* Mapping interrupt vector */
    itr_irq_map_rx_set(hw, ring->msix, ring->index);
    itr_irq_map_en_rx_set(hw, true, ring->index);

    rdm_cpu_id_set(hw, 0, ring->index);
    rdm_rx_desc_dca_en_set(hw, 0U, ring->index);
    rdm_rx_head_dca_en_set(hw, 0U, ring->index);
    rdm_rx_pld_dca_en_set(hw, 0U, ring->index);

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_ring_tx_init(struct aq_hw *hw, struct aq_ring *ring)
/*                     uint64_t ring_addr,
                     u32 ring_size,
                     u32 ring_idx,
                     u32 interrupt_cause,
                     u32 cpu_idx) */
{
    int err;
    u32 dma_desc_addr_lsw = (u32)ring->tx_descs_phys & 0xffffffff;
    u32 dma_desc_addr_msw = (u64)(ring->tx_descs_phys >> 32);

    AQ_DBG_ENTERA("[%d]", ring->index);

    tdm_tx_desc_en_set(hw, 0U, ring->index);

    reg_tx_dma_desc_base_addresslswset(hw, dma_desc_addr_lsw, ring->index);

    reg_tx_dma_desc_base_addressmswset(hw, dma_desc_addr_msw, ring->index);

    tdm_tx_desc_len_set(hw, ring->tx_size / 8U, ring->index);

    aq_ring_tx_tail_update(hw, ring, 0U);

    /* Set Tx threshold */
    tdm_tx_desc_wr_wb_threshold_set(hw, 0U, ring->index);

    /* Mapping interrupt vector */
    itr_irq_map_tx_set(hw, ring->msix, ring->index);
    itr_irq_map_en_tx_set(hw, true, ring->index);

    tdm_cpu_id_set(hw, 0, ring->index);
    tdm_tx_desc_dca_en_set(hw, 0U, ring->index);

    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_ring_tx_tail_update(struct aq_hw *hw, struct aq_ring *ring, u32 tail)
{
    AQ_DBG_ENTERA("[%d]", ring->index);
    reg_tx_dma_desc_tail_ptr_set(hw, tail, ring->index);
    AQ_DBG_EXIT(0);
    return (0);
}

int aq_ring_tx_start(struct aq_hw *hw, struct aq_ring *ring)
{
    int err;

    AQ_DBG_ENTERA("[%d]", ring->index);
    tdm_tx_desc_en_set(hw, 1U, ring->index);
    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_ring_rx_start(struct aq_hw *hw, struct aq_ring *ring)
{
    int err;

    AQ_DBG_ENTERA("[%d]", ring->index);
    rdm_rx_desc_en_set(hw, 1U, ring->index);
    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_ring_tx_stop(struct aq_hw *hw, struct aq_ring *ring)
{
    int err;

    AQ_DBG_ENTERA("[%d]", ring->index);
    tdm_tx_desc_en_set(hw, 0U, ring->index);
    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

int aq_ring_rx_stop(struct aq_hw *hw, struct aq_ring *ring)
{
    int err;

    AQ_DBG_ENTERA("[%d]", ring->index);
    rdm_rx_desc_en_set(hw, 0U, ring->index);
    /* Invalidate Descriptor Cache to prevent writing to the cached
     * descriptors and to the data pointer of those descriptors
     */
    rdm_rx_dma_desc_cache_init_tgl(hw);
    err = aq_hw_err_from_flags(hw);
    AQ_DBG_EXIT(err);
    return (err);
}

static void aq_ring_rx_refill(void* arg, if_rxd_update_t iru)
{
	aq_dev_t *aq_dev = arg;
	aq_rx_desc_t *rx_desc;
	struct aq_ring *ring;
	qidx_t i, pidx;

	AQ_DBG_ENTERA("ring=%d iru_pidx=%d iru_count=%d iru->iru_buf_size=%d",
				  iru->iru_qsidx, iru->iru_pidx, iru->iru_count, iru->iru_buf_size);

	ring = aq_dev->rx_rings[iru->iru_qsidx];
	pidx = iru->iru_pidx;

	for (i = 0; i < iru->iru_count; i++) {
		rx_desc = (aq_rx_desc_t *) &ring->rx_descs[pidx];
		rx_desc->read.buf_addr = htole64(iru->iru_paddrs[i]);
		rx_desc->read.hdr_addr = 0;

		pidx=aq_next(pidx, ring->rx_size - 1);
	}

	AQ_DBG_EXIT(0);
}

static void aq_isc_rxd_flush(void *arg, uint16_t rxqid, uint8_t flid __unused,
							 qidx_t pidx)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring = aq_dev->rx_rings[rxqid];

	AQ_DBG_ENTERA("[%d] tail=%u", ring->index, pidx);
	reg_rx_dma_desc_tail_ptr_set(&aq_dev->hw, pidx, ring->index);
	AQ_DBG_EXIT(0);
}

static int aq_isc_rxd_available(void *arg, uint16_t rxqid, qidx_t idx, qidx_t budget)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring = aq_dev->rx_rings[rxqid];
	aq_rx_desc_t *rx_desc = (aq_rx_desc_t *) ring->rx_descs;
	int cnt, i, iter;

	AQ_DBG_ENTERA("[%d] head=%u, budget %d", ring->index, idx, budget);

	for (iter = 0, cnt = 0, i = idx; iter < ring->rx_size && cnt <= budget;) {
		trace_aq_rx_descr(ring->index, i, (volatile u64*)&rx_desc[i]);
		if (!rx_desc[i].wb.dd)
			break;

		if (rx_desc[i].wb.eop) {
			iter++;
			i = aq_next(i, ring->rx_size - 1);

			cnt++;
		} else {
			/* LRO/Jumbo: wait for whole packet be in the ring */
			if (rx_desc[i].wb.rsc_cnt) {
				i = rx_desc[i].wb.next_desp;
				iter++;
				continue;
			} else {
				iter++;
				i = aq_next(i, ring->rx_size - 1);
				continue;
			}
		}
	}

	AQ_DBG_EXIT(cnt);
	return (cnt);
}

static void aq_rx_set_cso_flags(aq_rx_desc_t *rx_desc,  if_rxd_info_t ri)
{
	if ((rx_desc->wb.pkt_type & 0x3) == 0) { //IPv4
		if (rx_desc->wb.rx_cntl & BIT(0)){ // IPv4 csum checked
			ri->iri_csum_flags |= CSUM_IP_CHECKED;
			if (!(rx_desc->wb.rx_stat & BIT(1)))
				ri->iri_csum_flags |= CSUM_IP_VALID;
		}
	}
	if (rx_desc->wb.rx_cntl & BIT(1)) { // TCP/UDP csum checked
		ri->iri_csum_flags |= CSUM_L4_CALC;
		if (!(rx_desc->wb.rx_stat & BIT(2)) && // L4 csum error
			(rx_desc->wb.rx_stat & BIT(3))) {  // L4 csum valid
			ri->iri_csum_flags |= CSUM_L4_VALID;
			ri->iri_csum_data = htons(0xffff);
		}
	}
}

static uint8_t bsd_rss_type[16] = {
	[AQ_RX_RSS_TYPE_IPV4]=M_HASHTYPE_RSS_IPV4,
	[AQ_RX_RSS_TYPE_IPV6]=M_HASHTYPE_RSS_IPV6,
	[AQ_RX_RSS_TYPE_IPV4_TCP]=M_HASHTYPE_RSS_TCP_IPV4,
	[AQ_RX_RSS_TYPE_IPV6_TCP]=M_HASHTYPE_RSS_TCP_IPV6,
	[AQ_RX_RSS_TYPE_IPV4_UDP]=M_HASHTYPE_RSS_UDP_IPV4,
	[AQ_RX_RSS_TYPE_IPV6_UDP]=M_HASHTYPE_RSS_UDP_IPV6,
};



static int aq_isc_rxd_pkt_get(void *arg, if_rxd_info_t ri)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring = aq_dev->rx_rings[ri->iri_qsidx];
	aq_rx_desc_t *rx_desc;
	if_t ifp;
	int cidx, rc = 0, i;
	size_t len, total_len;

	AQ_DBG_ENTERA("[%d] start=%d", ring->index, ri->iri_cidx);
	cidx = ri->iri_cidx;
	ifp = iflib_get_ifp(aq_dev->ctx);
	i = 0;

	do {
		rx_desc = (aq_rx_desc_t *) &ring->rx_descs[cidx];

		trace_aq_rx_descr(ring->index, cidx, (volatile u64*)rx_desc);

		if ((rx_desc->wb.rx_stat & BIT(0)) != 0) {
			ring->stats.rx_err++;
			rc = (EBADMSG);
			goto exit;
		}

		if (!rx_desc->wb.eop) {
			len = ring->rx_max_frame_size;
		} else {
			total_len = le32toh(rx_desc->wb.pkt_len);
			len = total_len & (ring->rx_max_frame_size - 1);
		}
		ri->iri_frags[i].irf_flid = 0;
		ri->iri_frags[i].irf_idx = cidx;
		ri->iri_frags[i].irf_len = len;

		if ((rx_desc->wb.pkt_type & 0x60) != 0) {
			ri->iri_flags |= M_VLANTAG;
			ri->iri_vtag = le32toh(rx_desc->wb.vlan);
		}

		i++;
		cidx = aq_next(cidx, ring->rx_size - 1);
	} while (!rx_desc->wb.eop);

	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0) {
		aq_rx_set_cso_flags(rx_desc, ri);
	}
	ri->iri_rsstype = bsd_rss_type[rx_desc->wb.rss_type & 0xF];
	if (ri->iri_rsstype != M_HASHTYPE_NONE) {
		ri->iri_flowid = le32toh(rx_desc->wb.rss_hash);
	}

	ri->iri_len = total_len;
	ri->iri_nfrags = i;

	ring->stats.rx_bytes += total_len;
	ring->stats.rx_pkts++;

exit:
	AQ_DBG_EXIT(rc);
	return (rc);
}

/*****************************************************************************/
/*                                                                           */
/*****************************************************************************/

static void aq_setup_offloads(aq_dev_t *aq_dev, if_pkt_info_t pi, aq_tx_desc_t *txd, u32 tx_cmd)
{
    AQ_DBG_ENTER();
    txd->cmd |= tx_desc_cmd_fcs;
    txd->cmd |= (pi->ipi_csum_flags & (CSUM_IP|CSUM_TSO)) ? tx_desc_cmd_ipv4 : 0;
    txd->cmd |= (pi->ipi_csum_flags &
				 (CSUM_IP_TCP | CSUM_IP6_TCP | CSUM_IP_UDP | CSUM_IP6_UDP)
				) ? tx_desc_cmd_l4cs : 0;
    txd->cmd |= (pi->ipi_flags & IPI_TX_INTR) ? tx_desc_cmd_wb : 0;
    txd->cmd |= tx_cmd;
    AQ_DBG_EXIT(0);
}

static int aq_ring_tso_setup(aq_dev_t *aq_dev, if_pkt_info_t pi, uint32_t *hdrlen, aq_txc_desc_t *txc)
{
	uint32_t tx_cmd = 0;

	AQ_DBG_ENTER();
	if (pi->ipi_csum_flags & CSUM_TSO) {
		AQ_DBG_PRINT("aq_tso_setup(): TSO enabled");
		tx_cmd |= tx_desc_cmd_lso | tx_desc_cmd_l4cs;

		if (pi->ipi_ipproto != IPPROTO_TCP) {
			AQ_DBG_PRINT("aq_tso_setup not a tcp");
			AQ_DBG_EXIT(0);
			return (0);
		}

		txc->cmd = 0x4; /* TCP */

		if (pi->ipi_csum_flags & CSUM_IP6_TCP)
		    txc->cmd |= 0x2;

		txc->l2_len = pi->ipi_ehdrlen;
		txc->l3_len = pi->ipi_ip_hlen;
		txc->l4_len = pi->ipi_tcp_hlen;
		txc->mss_len = pi->ipi_tso_segsz;
		*hdrlen = txc->l2_len + txc->l3_len + txc->l4_len;
	}

	// Set VLAN tag
	if (pi->ipi_mflags & M_VLANTAG) {
		tx_cmd |= tx_desc_cmd_vlan;
		txc->vlan_tag = htole16(pi->ipi_vtag);
	}

	if (tx_cmd) {
		txc->type = tx_desc_type_ctx;
		txc->idx = 0;
	}

	AQ_DBG_EXIT(tx_cmd);
	return (tx_cmd);
}

static int aq_isc_txd_encap(void *arg, if_pkt_info_t pi)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring;
	aq_txc_desc_t *txc;
	aq_tx_desc_t *txd = NULL;
	bus_dma_segment_t *segs;
	qidx_t pidx;
	uint32_t hdrlen=0, pay_len;
	uint8_t tx_cmd = 0;
	int i, desc_count = 0;

	AQ_DBG_ENTERA("[%d] start=%d", pi->ipi_qsidx, pi->ipi_pidx);
	ring = aq_dev->tx_rings[pi->ipi_qsidx];

	segs = pi->ipi_segs;
	pidx = pi->ipi_pidx;
	txc = (aq_txc_desc_t *)&ring->tx_descs[pidx];
	AQ_DBG_PRINT("txc at 0x%p, txd at 0x%p len %d", txc, txd, pi->ipi_len);

	pay_len = pi->ipi_len;

	txc->flags1 = 0U;
	txc->flags2 = 0U;

	tx_cmd = aq_ring_tso_setup(aq_dev, pi, &hdrlen, txc);
	AQ_DBG_PRINT("tx_cmd = 0x%x", tx_cmd);

	if (tx_cmd) {
		trace_aq_tx_context_descr(ring->index, pidx, (volatile void*)txc);
		/* We've consumed the first desc, adjust counters */
		pidx = aq_next(pidx, ring->tx_size - 1);

		txd = &ring->tx_descs[pidx];
		txd->flags = 0U;
	} else {
		txd = (aq_tx_desc_t *)txc;
	}
	AQ_DBG_PRINT("txc at 0x%p, txd at 0x%p", txc, txd);

	txd->ct_en = !!tx_cmd;

	txd->type = tx_desc_type_desc;

	aq_setup_offloads(aq_dev, pi, txd, tx_cmd);

	if (tx_cmd) {
		txd->ct_idx = 0;
	}

	pay_len -= hdrlen;

	txd->pay_len = pay_len;

	AQ_DBG_PRINT("num_frag[%d] pay_len[%d]", pi->ipi_nsegs, pay_len);
	for (i = 0; i < pi->ipi_nsegs; i++) {
		if (desc_count > 0) {
			txd = &ring->tx_descs[pidx];
			txd->flags = 0U;
		}

		txd->buf_addr = htole64(segs[i].ds_addr);

		txd->type = tx_desc_type_desc;
		txd->len = segs[i].ds_len;
		txd->pay_len = pay_len;
		if (i < pi->ipi_nsegs - 1)
			trace_aq_tx_descr(ring->index, pidx, (volatile void*)txd);

		pidx = aq_next(pidx, ring->tx_size - 1);

		desc_count++;
	}
	// Last descriptor requires EOP and WB
	txd->eop = 1U;

	AQ_DBG_DUMP_DESC(txd);
	trace_aq_tx_descr(ring->index, pidx, (volatile void*)txd);
	ring->tx_tail = pidx;

	ring->stats.tx_pkts++;
	ring->stats.tx_bytes += pay_len;

	pi->ipi_new_pidx = pidx;

	AQ_DBG_EXIT(0);
	return (0);
}

static void aq_isc_txd_flush(void *arg, uint16_t txqid, qidx_t pidx)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring = aq_dev->tx_rings[txqid];
	AQ_DBG_ENTERA("[%d] tail=%d", ring->index, pidx);

	// Update the write pointer - submits packet for transmission
	aq_ring_tx_tail_update(&aq_dev->hw, ring, pidx);
	AQ_DBG_EXIT(0);
}


static inline unsigned int aq_avail_desc(int a, int b, int size)
{
    return (((b >= a)) ? ((size ) - b + a) : (a - b));
}

static int aq_isc_txd_credits_update(void *arg, uint16_t txqid, bool clear)
{
	aq_dev_t *aq_dev = arg;
	struct aq_ring *ring = aq_dev->tx_rings[txqid];
	uint32_t head;
	int avail;

	AQ_DBG_ENTERA("[%d] clear=%d", ring->index, clear);
	avail = 0;
	head = tdm_tx_desc_head_ptr_get(&aq_dev->hw, ring->index);
	AQ_DBG_PRINT("swhead %d hwhead %d", ring->tx_head, head);

	if (ring->tx_head == head) {
		avail = 0; //ring->tx_size;
		goto done;
	}

	avail = aq_avail_desc(head, ring->tx_head, ring->tx_size);
	if (clear)
		ring->tx_head = head;

done:
	AQ_DBG_EXIT(avail);
	return (avail);
}
