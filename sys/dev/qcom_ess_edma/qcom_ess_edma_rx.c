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
#include <dev/qcom_ess_edma/qcom_ess_edma_rx.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>

/*
 * Map the given RX queue to a given CPU.
 */
int
qcom_ess_edma_rx_queue_to_cpu(struct qcom_ess_edma_softc *sc, int queue)
{
	return (queue % mp_ncpus);
}

int
qcom_ess_edma_rx_ring_setup(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{
	struct qcom_ess_edma_sw_desc_rx *rxd;
	int i, ret;

	for (i = 0; i < EDMA_RX_RING_SIZE; i++) {
		rxd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, i);
		if (rxd == NULL) {
			device_printf(sc->sc_dev,
			    "ERROR; couldn't get sw desc (idx %d)\n", i);
			return (EINVAL);
		}
		rxd->m = NULL;
		ret = bus_dmamap_create(ring->buffer_dma_tag,
		    BUS_DMA_NOWAIT,
		    &rxd->m_dmamap);
		if (ret != 0) {
			device_printf(sc->sc_dev,
			    "%s: failed to create dmamap (%d)\n",
			    __func__, ret);
		}
	}

	return (0);
}

int
qcom_ess_edma_rx_ring_clean(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{
	device_printf(sc->sc_dev, "%s: TODO\n", __func__);
	return (0);
}

/*
 * Allocate a receive buffer for the given ring/index, setup DMA.
 *
 * The caller must have called the ring prewrite routine in order
 * to flush the ring memory if needed before writing to it.
 * It's not done here so we don't do it on /every/ ring update.
 *
 * Returns an error if the slot is full or unable to fill it;
 * the caller should then figure out how to cope.
 */
int
qcom_ess_edma_rx_buf_alloc(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, int idx)
{
	struct mbuf *m;
	struct qcom_ess_edma_sw_desc_rx *rxd;
	struct qcom_ess_edma_rx_free_desc *ds;
	bus_dma_segment_t segs[1];
	int error;
	int nsegs;

	/* Get the software/hardware descriptors we're going to update */
	rxd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, idx);
	if (rxd == NULL) {
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

	/* If this ring has an mbuf already then return error */
	if (rxd->m != NULL) {
		device_printf(sc->sc_dev,
		    "ERROR: sw desc idx %d already has an mbuf\n",
		    idx);
		return (EINVAL); /* XXX */
	}

	/* Allocate mbuf */
	m = m_get2(sc->sc_config.rx_buf_size, M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		/* XXX keep statistics */
		device_printf(sc->sc_dev, "ERROR: failed to allocate mbuf\n");
		return (ENOMEM);
	}

	/* Load dma map, get physical memory address of mbuf */
	nsegs = 1;
	m->m_pkthdr.len = m->m_len = sc->sc_config.rx_buf_size;

	/* ETHER_ALIGN hack */
	if (sc->sc_config.rx_buf_ether_align)
		m_adj(m, ETHER_ALIGN);
	error = bus_dmamap_load_mbuf_sg(ring->buffer_dma_tag, rxd->m_dmamap,
	    m, segs, &nsegs, 0);
	if (error != 0 || nsegs != 1) {
		device_printf(sc->sc_dev,
		    "ERROR: couldn't load mbuf dmamap (%d) (nsegs=%d)\n", error, nsegs);
		m_freem(m);
		return (error);
	}

	/* Populate sw and hw desc */
	rxd->m = m;
	rxd->m_physaddr = segs[0].ds_addr;

	ds->addr = htole32(segs[0].ds_addr);

	ring->stats.num_added++;

	return (0);
}

/*
 * Remove a receive buffer from the given ring/index.
 *
 * This clears the software/hardware index and unmaps the mbuf;
 * the returned mbuf will be owned by the caller.
 */
struct mbuf *
qcom_ess_edma_rx_buf_clean(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, int idx)
{
	struct mbuf *m;
	struct qcom_ess_edma_sw_desc_rx *rxd;
	struct qcom_ess_edma_rx_free_desc *ds;

	/* Get the software/hardware descriptors we're going to update */
	rxd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring, idx);
	if (rxd == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR; couldn't get sw desc (idx %d)\n", idx);
		return (NULL);
	}
	ds = qcom_ess_edma_desc_ring_get_hw_desc(sc, ring, idx);
	if (ds == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR; couldn't get hw desc (idx %d)\n", idx);
		return (NULL);
	}

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING,
	    "%s: idx=%u, rxd=%p, ds=0x%p, maddr=0x%08x/0x%08lx\n",
	    __func__, idx, rxd, ds, ds->addr, rxd->m_physaddr);

	/* No mbuf? return null; it's fine */
	if (rxd->m == NULL) {
		return (NULL);
	}

	/* Flush mbuf */
	bus_dmamap_sync(ring->buffer_dma_tag, rxd->m_dmamap,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* Unload */
	bus_dmamap_unload(ring->buffer_dma_tag, rxd->m_dmamap);

	/* Remove sw/hw desc entries */
	m = rxd->m;
	rxd->m = NULL;

#ifdef	ESS_EDMA_DEBUG_CLEAR_DESC
	/*
	 * Note: removing hw entries is purely for correctness; it may be
	 * VERY SLOW!
	 */
	ds->addr = 0;
#endif

	ring->stats.num_cleaned++;

	return (m);
}

/*
 * Fill the current ring, up to 'num' entries (or the ring is full.)
 * It will also update the producer index for the given queue.
 *
 * Returns 0 if OK, error if there's a problem.
 */
int
qcom_ess_edma_rx_ring_fill(struct qcom_ess_edma_softc *sc,
    int queue, int num)
{
	struct qcom_ess_edma_desc_ring *ring;
	int num_fill;
	int idx;
	int error;
	int prod_index;
	int n = 0;


	ring = &sc->sc_rx_ring[queue];

	EDMA_RING_LOCK_ASSERT(ring);

	num_fill = num;
	if (num_fill > ring->ring_count)
		num_fill = ring->ring_count - 1;
	idx = ring->next_to_fill;

	while (num_fill != 0) {
		error = qcom_ess_edma_rx_buf_alloc(sc, ring, idx);
		if (error != 0) {
			device_printf(sc->sc_dev,
			    "ERROR: queue %d: failed to alloc rx buf (%d)\n",
			    queue, error);
			break;
		}
		num_fill--;

		/* Update ring index, wrap at ring_count */
		idx++;
		if (idx >= ring->ring_count)
			idx = 0;
		n++;
	}

	ring->next_to_fill = idx;

	/* Flush ring updates before HW index is updated */
	qcom_ess_edma_desc_ring_flush_preupdate(sc, ring);

	/* producer index is the ring number, minus 1 (ie the slot BEFORE) */
	if (idx == 0)
		prod_index = ring->ring_count - 1;
	else
		prod_index = idx - 1;
	(void) qcom_ess_edma_hw_rfd_prod_index_update(sc, queue, prod_index);

	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING,
	    "%s: queue %d: added %d bufs, prod_idx=%u\n",
	    __func__, queue, n, prod_index);

	return (0);
}

/*
 * fetch, unmap the given mbuf
 *
struct mbuf *
qcom_ess_edma_rx_buf_clean(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, int idx)
*/


/*
 * Run through the RX ring, complete frames.
 *
 * For now they're simply freed and the ring is re-filled.
 * Once that logic is working soundly we'll want to populate an
 * mbuf list for the caller with completed mbufs so they can be
 * dispatched up to the network stack.
 */
int
qcom_ess_edma_rx_ring_complete(struct qcom_ess_edma_softc *sc, int queue,
    struct mbufq *mq)
{
	struct qcom_ess_edma_desc_ring *ring;
	struct qcom_ess_edma_sw_desc_rx *rxd;
	int n, cleaned_count, len;
	uint16_t sw_next_to_clean, hw_next_to_clean;
	struct mbuf *m;
	struct qcom_edma_rx_return_desc *rrd;
	int num_rfds, port_id, priority, hash_type, hash_val, flow_cookie, vlan;
	bool rx_checksum = 1;
	int port_vlan = -1;

	ring = &sc->sc_rx_ring[queue];

	EDMA_RING_LOCK_ASSERT(ring);

	qcom_ess_edma_desc_ring_flush_postupdate(sc, ring);

	sw_next_to_clean = ring->next_to_clean;
	hw_next_to_clean = 0;
	cleaned_count = 0;

	for (n = 0; n < EDMA_RX_RING_SIZE - 1; n++) {
		rxd = qcom_ess_edma_desc_ring_get_sw_desc(sc, ring,
		    sw_next_to_clean);
		if (rxd == NULL) {
			device_printf(sc->sc_dev,
			    "ERROR; couldn't get sw desc (idx %d)\n",
			        sw_next_to_clean);
			return (EINVAL);
		}

		hw_next_to_clean = qcom_ess_edma_hw_rfd_get_cons_index(sc,
		    queue);
		if (hw_next_to_clean == sw_next_to_clean)
			break;

		/* Unmap the mbuf at this index */
		m = qcom_ess_edma_rx_buf_clean(sc, ring, sw_next_to_clean);
		sw_next_to_clean = (sw_next_to_clean + 1) % ring->ring_count;
		cleaned_count++;

		/* Get the RRD header */
		rrd = mtod(m, struct qcom_edma_rx_return_desc *);
		if (rrd->rrd7 & EDMA_RRD_DESC_VALID) {
			len = rrd->rrd6 & EDMA_RRD_PKT_SIZE_MASK;
			num_rfds = rrd->rrd1 & EDMA_RRD_NUM_RFD_MASK;;
			port_id = (rrd->rrd1 >> EDMA_PORT_ID_SHIFT)
			    & EDMA_PORT_ID_MASK;
			priority = (rrd->rrd1 >> EDMA_RRD_PRIORITY_SHIFT)
			    & EDMA_RRD_PRIORITY_MASK;
			hash_type = (rrd->rrd5 >> EDMA_HASH_TYPE_SHIFT)
			    & EDMA_HASH_TYPE_MASK;
			hash_val = rrd->rrd2;
			flow_cookie = rrd->rrd3 & EDMA_RRD_FLOW_COOKIE_MASK;
			vlan = rrd->rrd4;
			QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_FRAME,
			    "%s: len=%d, num_rfds=%d, port_id=%d,"
			    " priority=%d, hash_type=%d, hash_val=%d,"
			    " flow_cookie=%d, vlan=%d\n",
			    __func__,
			    len,
			    num_rfds,
			    port_id,
			    priority,
			    hash_type,
			    hash_val,
			    flow_cookie,
			    vlan);
			QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_FRAME,
			    "%s:   flags: L4 checksum"
			    " fail=%d, 802.1q vlan=%d, 802.1ad vlan=%d\n",
			    __func__,
			    !! (rrd->rrd6 & EDMA_RRD_CSUM_FAIL_MASK),
			    !! (rrd->rrd7 & EDMA_RRD_CVLAN),
			    !! (rrd->rrd1 & EDMA_RRD_SVLAN));
		} else {
			len = 0;
		}

		/* Payload starts after the RRD header */
		m_adj(m, sizeof(struct qcom_edma_rx_return_desc));

		/* Set mbuf length now */
		m->m_len = m->m_pkthdr.len = len;

		/*
		 * Set rcvif to the relevant GMAC ifp; GMAC receive will
		 * check the field to receive it to the right place, or
		 * if it's NULL it'll drop it for us.
		 */
		m->m_pkthdr.rcvif = NULL;
		if (sc->sc_gmac_port_map[port_id] != -1) {
			struct qcom_ess_edma_gmac *gmac;
			gmac = &sc->sc_gmac[sc->sc_gmac_port_map[port_id]];
			QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_FRAME,
			    "%s:  port_id=%d gmac=%d\n", __func__,
			    port_id, gmac->id);
			if (gmac->enabled == true) {
				m->m_pkthdr.rcvif = gmac->ifp;
				if ((if_getcapenable(gmac->ifp) & IFCAP_RXCSUM) != 0)
					rx_checksum = true;
			}
			port_vlan = gmac->vlan_id;
		}

		/* XXX TODO: handle multi-frame packets (ie, jumbos!) */
		/* XXX TODO: handle 802.1ad VLAN offload field */
		/* XXX TODO: flow offload */

		/*
		 * For now we don't support disabling VLAN offload.
		 * Instead, tags are stripped by the hardware.
		 * Handle the outer VLAN tag; worry about 802.1ad
		 * later on (and hopefully by something other than
		 * adding another mbuf.)
		 */
		if ((rrd->rrd7 & EDMA_RRD_CVLAN) != 0) {
			/*
			 * There's an outer VLAN tag that has been
			 * decaped by the hardware. Compare it to the
			 * current port vlan, and if they don't match,
			 * add an offloaded VLAN tag to the mbuf.
			 *
			 * And yes, care about the priority field too.
			 */
			 if ((port_vlan == -1) || (port_vlan != vlan)) {
			 	m->m_pkthdr.ether_vtag = (vlan & 0xfff)
				    | ((priority < 1) & 0xf);
				m->m_flags |= M_VLANTAG;
			 }
		}

		/*
		 * Store the hash info in the mbuf if it's there.
		 *
		 * XXX TODO: decode the RSS field and translate it to
		 * the mbuf hash entry.  For now, just treat as OPAQUE.
		 */
		if (hash_type != EDMA_RRD_RSS_TYPE_NONE) {
			m->m_pkthdr.flowid = hash_val;
			M_HASHTYPE_SET(m, M_HASHTYPE_OPAQUE);
		}

		/*
		 * Check the RX checksum flag if the destination ifp
		 * has the RXCSUM flag set.
		 */
		if (rx_checksum) {
			if (rrd->rrd6 & EDMA_RRD_CSUM_FAIL_MASK) {
				/* Fail */
				ring->stats.num_rx_csum_fail++;
			} else {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED
				    | CSUM_IP_VALID
				    | CSUM_DATA_VALID
				    | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
				ring->stats.num_rx_csum_ok++;
			}
		}


		/*
		 * Finally enqueue into the incoming receive queue
		 * to push up into the networking stack.
		 */
		if (mbufq_enqueue(mq, m) != 0) {
			ring->stats.num_enqueue_full++;
			m_freem(m);
		}
	}
	ring->next_to_clean = sw_next_to_clean;

	/* Refill ring if needed */
	if (cleaned_count > 0) {
		QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_RX_RING,
		    "%s: ring=%d, cleaned=%d\n",
		    __func__, queue, cleaned_count);
		(void) qcom_ess_edma_rx_ring_fill(sc, queue, cleaned_count);
		(void) qcom_ess_edma_hw_rfd_sw_cons_index_update(sc, queue,
		    ring->next_to_clean);
	}

	return (0);
}
