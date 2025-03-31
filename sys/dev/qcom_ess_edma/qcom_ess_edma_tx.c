/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/endian.h>
#include <sys/smp.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/qcom_ess_edma/qcom_ess_edma_var.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_reg.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_hw.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_desc.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_tx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>

/*
 * Map the given TX queue to a given CPU.
 *
 * The current mapping in the if_transmit() path
 * will map mp_ncpu groups of flowids to the TXQs.
 * So for a 4 CPU system the first four will be CPU 0,
 * the second four will be CPU 1, etc.
 */
int
qcom_ess_edma_tx_queue_to_cpu(struct qcom_ess_edma_softc *sc, int queue)
{

	return (queue / mp_ncpus);
}

int
qcom_ess_edma_tx_ring_setup(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{
	struct qcom_ess_edma_sw_desc_tx *txd;
	int i, ret;

	for (i = 0; i < EDMA_TX_RING_SIZE; i++) {
		txd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, i);
		if (txd == NULL) {
			device_printf(sc->sc_dev,
			    "ERROR; couldn't get sw desc (idx %d)\n", i);
			return (EINVAL);
		}
		txd->m = NULL;
		ret = bus_dmamap_create(ring->buffer_dma_tag,
		    BUS_DMA_NOWAIT,
		    &txd->m_dmamap);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to create dmamap (%d)\n",
			    __func__, ret);
		}
	}

	return (0);
}

int
qcom_ess_edma_tx_ring_clean(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{
	device_printf(sc->sc_dev, "%s: TODO\n", __func__);
	return (0);
}

/*
 * Clear the sw/hw descriptor entries, unmap/free the mbuf chain that's
 * part of this.
 */
static int
qcom_ess_edma_tx_unmap_and_clean(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, uint16_t idx)
{
	struct qcom_ess_edma_sw_desc_tx *txd;
	struct qcom_ess_edma_tx_desc *ds;

	/* Get the software/hardware descriptors we're going to update */
	txd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, idx);
	if (txd == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR; couldn't get sw desc (idx %d)\n", idx);
		return (EINVAL);
	}

	ds = qcom_ess_edma_desc_ring_get_hw_desc(sc, ring, idx);
	if (ds == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR; couldn't get hw desc (idx %d)\n", idx);
		return (EINVAL);
	}

	if (txd->m != NULL) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_RING,
		    "%s:   idx %d, unmap/free\n", __func__, idx);
		bus_dmamap_unload(ring->buffer_dma_tag, txd->m_dmamap);
		m_freem(txd->m);
		txd->m = NULL;
		txd->is_first = txd->is_last = 0;
	}

#ifdef	ESS_EDMA_DEBUG_CLEAR_DESC
	/* This is purely for debugging/testing right now; it's slow! */
	memset(ds, 0, sizeof(struct qcom_ess_edma_tx_desc));
#endif

	return (0);
}

/*
 * Run through the TX ring, complete/free frames.
 */
int
qcom_ess_edma_tx_ring_complete(struct qcom_ess_edma_softc *sc, int queue)
{
	struct qcom_ess_edma_desc_ring *ring;
	uint32_t n;
	uint16_t sw_next_to_clean, hw_next_to_clean;

	ring = &sc->sc_tx_ring[queue];

	EDMA_RING_LOCK_ASSERT(ring);

	qcom_ess_edma_desc_ring_flush_postupdate(sc, ring);

	sw_next_to_clean = ring->next_to_clean;
	hw_next_to_clean = 0;
	n = 0;

	/* Get the current hardware completion index */
	(void) qcom_ess_edma_hw_tx_read_tpd_cons_idx(sc, queue,
	    &hw_next_to_clean);

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_RING,
	    "%s: called; sw=%d, hw=%d\n", __func__,
	    sw_next_to_clean, hw_next_to_clean);

	/* clean the buffer chain and descriptor(s) here */
	while (sw_next_to_clean != hw_next_to_clean) {
		qcom_ess_edma_tx_unmap_and_clean(sc, ring, sw_next_to_clean);
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_RING,
		    "%s  cleaning %d\n", __func__, sw_next_to_clean);
		sw_next_to_clean++;
		if (sw_next_to_clean >= ring->ring_count)
			sw_next_to_clean = 0;
		n++;
	}

	ring->stats.num_cleaned += n;
	ring->stats.num_tx_complete++;

	ring->next_to_clean = sw_next_to_clean;

	/* update the TPD consumer index register */
	qcom_ess_edma_hw_tx_update_cons_idx(sc, queue, sw_next_to_clean);

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_RING_COMPLETE,
	    "%s: cleaned %d descriptors\n", __func__, n);

	return (0);
}

/*
 * Attempt to enqueue a single frame.
 *
 * This is the MVP required to send a single ethernet mbuf / mbuf chain.
 * VLAN tags are added/required as the default switch configuration
 * from device-tree uses both the port bitmap and VLAN IDs for
 * controlling LAN/WAN/etc interface traffic.
 *
 * Note, this does NOT update the transmit pointer to the hardware;
 * that must be done after calling this function one or more times.
 *
 * The mbuf is either consumed into the ring or it is returned
 * unsent.  If we've modifide it in any way then the caller should
 * use what's returned back in m0 (eg to pushback.)
 */
int
qcom_ess_edma_tx_ring_frame(struct qcom_ess_edma_softc *sc, int queue,
    struct mbuf **m0, uint16_t port_bitmap, int default_vlan)
{
	struct qcom_ess_edma_sw_desc_tx *txd_first;
	struct qcom_ess_edma_desc_ring *ring;
	struct ether_vlan_header *eh;
	bus_dma_segment_t txsegs[QCOM_ESS_EDMA_MAX_TXFRAGS];
	uint32_t word1, word3;
	uint32_t eop;
	int vlan_id;
	int num_left, ret, nsegs, i;
	uint16_t next_to_fill;
	uint16_t svlan_tag;
	struct mbuf *m;

	ring = &sc->sc_tx_ring[queue];

	EDMA_RING_LOCK_ASSERT(ring);

	m = *m0;

	/*
	 * Do we have ANY space? If not, return ENOBUFS, let the
	 * caller decide what to do with the mbuf.
	 */
	num_left = qcom_ess_edma_desc_ring_get_num_available(sc, ring);
	if (num_left < 2) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s: num_left=%d\n", __func__, num_left);
		ring->stats.num_enqueue_full++;
		return (ENOBUFS);
	}

	/*
	 * Get the current sw/hw descriptor offset; we'll use its
	 * dmamap and then switch it out with the last one when
	 * the mbuf is put there.
	 */
	next_to_fill = ring->next_to_fill;
	txd_first = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring,
	    next_to_fill);
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
	    "%s: starting at idx %d\n", __func__, next_to_fill);

	/*
	 * Do the initial mbuf load; see how many fragments we
	 * have.  If we don't have enough descriptors available
	 * then immediately unmap and return an error.
	 */
	ret = bus_dmamap_load_mbuf_sg(ring->buffer_dma_tag,
	    txd_first->m_dmamap,
	    m,
	    txsegs,
	    &nsegs,
	    BUS_DMA_NOWAIT);
	if (ret != 0) {
		ring->stats.num_tx_mapfail++;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s: map failed (%d)\n", __func__, ret);
		return (ENOBUFS);
	}
	if (nsegs == 0) {
		ring->stats.num_tx_maxfrags++;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s: too many segs\n", __func__);
		return (ENOBUFS);
	}

	if (nsegs + 2 > num_left) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s: nsegs=%d, num_left=%d\n", __func__, nsegs, num_left);
		bus_dmamap_unload(ring->buffer_dma_tag, txd_first->m_dmamap);
		ring->stats.num_enqueue_full++;
		return (ENOBUFS);
	}

	bus_dmamap_sync(ring->buffer_dma_tag, txd_first->m_dmamap,
	    BUS_DMASYNC_PREWRITE);

	/*
	 * At this point we're committed to sending the frame.
	 *
	 * Get rid of the rcvif that is being used to track /send/ ifnet.
	 */
	m->m_pkthdr.rcvif = NULL;

	/*
	 *
	 * Configure up the various header fields that are shared
	 * between descriptors.
	 */
	svlan_tag = 0; /* 802.3ad tag? */
	/* word1 - tx checksum, v4/v6 TSO, pppoe, 802.3ad vlan flag */
	word1 = 0;
	/*
	 * word3 - insert default vlan; vlan tag/flag, CPU/STP/RSTP stuff,
	 * port map
	 */
	word3 = 0;
	word3 |= (port_bitmap << EDMA_TPD_PORT_BITMAP_SHIFT);

	/*
	 * If VLAN offload is enabled, we can enable inserting a CVLAN
	 * tag here for the default VLAN, or the VLAN interface.
	 * The default switch configuration requires both a port_bitmap
	 * and 802.1q VLANs configured.
	 *
	 * If there's a VLAN tag on the mbuf then we leave it alone.
	 * I don't want to try and strip out the VLAN header from a packet
	 * here.
	 *
	 * There's no 802.1ad support in here yet.
	 */
	eh = mtod(m, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		/* Don't add a tag, just use what's here */
		vlan_id = -1;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s:   no vlan id\n", __func__);

	} else if ((m->m_flags & M_VLANTAG) != 0) {
		/* We have an offload VLAN tag, use it */
		vlan_id = m->m_pkthdr.ether_vtag & 0x0fff;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s:   header tag vlan id=%d\n", __func__, vlan_id);
	} else {
		/* No VLAN tag, no VLAN header; default VLAN */
		vlan_id = default_vlan;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s:   no vlan tag/hdr; vlan id=%d\n", __func__,
		    vlan_id);
	}

	/*
	 * Only add the offload tag if we need to.
	 */
	if (vlan_id != -1) {
		word3 |= (1U << EDMA_TX_INS_CVLAN);
		word3 |= (vlan_id << EDMA_TX_CVLAN_TAG_SHIFT);
	}

	/* End of frame flag */
	eop = 0;

	/*
	 * Walk the mbuf segment list, and allocate descriptor
	 * entries.  Put the mbuf in the last descriptor entry
	 * and then switch out the first/last dmamap entries.
	 */
	for (i = 0; i < nsegs; i++) {
		struct qcom_ess_edma_sw_desc_tx *txd;
		struct qcom_ess_edma_tx_desc *ds;
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s:   filling idx %d\n", __func__, next_to_fill);
		txd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, next_to_fill);
		ds = qcom_ess_edma_desc_ring_get_hw_desc(sc, ring, next_to_fill);
		txd->m = NULL;
		if (i == 0) {
			txd->is_first = 1;
		}
		if (i == (nsegs - 1)) {
			bus_dmamap_t dm;

			txd->is_last = 1;
			eop = EDMA_TPD_EOP;
			/*
			 * Put the txmap and the mbuf in the last swdesc.
			 * That way it isn't freed until we've transmitted
			 * all the descriptors of this frame, in case the
			 * hardware decides to notify us of some half-sent
			 * stuff.
			 *
			 * Moving the pointers around here sucks a little
			 * but it DOES beat not freeing the dmamap entries
			 * correctly.
			 */
			txd->m = m;
			dm = txd_first->m_dmamap;
			txd_first->m_dmamap = txd->m_dmamap;
			txd->m_dmamap = dm;
		}
		ds->word1 = word1 | eop;
		ds->word3 = word3;
		ds->svlan_tag = svlan_tag;
		ds->addr = htole32(txsegs[i].ds_addr);
		ds->len = htole16(txsegs[i].ds_len);

		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_TX_FRAME,
		    "%s:   addr=0x%lx len=%ld eop=0x%x\n",
		    __func__,
		    txsegs[i].ds_addr,
		    txsegs[i].ds_len,
		    eop);

		next_to_fill++;
		if (next_to_fill >= ring->ring_count)
			next_to_fill = 0;
	}

	ring->stats.num_added += nsegs;

	/* Finish, update ring tracking */
	ring->next_to_fill = next_to_fill;

	ring->stats.num_tx_ok++;

	return (0);
}

/*
 * Update the hardware with the new state of the transmit ring.
 */
int
qcom_ess_edma_tx_ring_frame_update(struct qcom_ess_edma_softc *sc, int queue)
{
	struct qcom_ess_edma_desc_ring *ring;

	ring = &sc->sc_tx_ring[queue];

	EDMA_RING_LOCK_ASSERT(ring);

	qcom_ess_edma_desc_ring_flush_preupdate(sc, ring);

	(void) qcom_ess_edma_hw_tx_update_tpd_prod_idx(sc, queue,
	    ring->next_to_fill);

	/* XXX keep stats for this specific call? */
	return (0);
}
