/*-
 * Copyright (c) 2014 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/socket.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc1000_reg.h>
#include <dev/dwc/dwc1000_dma.h>

#define	WATCHDOG_TIMEOUT_SECS	5
#define	DMA_RESET_TIMEOUT	100

/* TX descriptors - TDESC0 is almost unified */
#define	TDESC0_OWN		(1U << 31)
#define	TDESC0_IHE		(1U << 16)	/* IP Header Error */
#define	TDESC0_ES		(1U << 15)	/* Error Summary */
#define	TDESC0_JT		(1U << 14)	/* Jabber Timeout */
#define	TDESC0_FF		(1U << 13)	/* Frame Flushed */
#define	TDESC0_PCE		(1U << 12)	/* Payload Checksum Error */
#define	TDESC0_LOC		(1U << 11)	/* Loss of Carrier */
#define	TDESC0_NC		(1U << 10)	/* No Carrier */
#define	TDESC0_LC		(1U <<  9)	/* Late Collision */
#define	TDESC0_EC		(1U <<  8)	/* Excessive Collision */
#define	TDESC0_VF		(1U <<  7)	/* VLAN Frame */
#define	TDESC0_CC_MASK		0xf
#define	TDESC0_CC_SHIFT		3		/* Collision Count */
#define	TDESC0_ED		(1U <<  2)	/* Excessive Deferral */
#define	TDESC0_UF		(1U <<  1)	/* Underflow Error */
#define	TDESC0_DB		(1U <<  0)	/* Deferred Bit */
/* TX descriptors - TDESC0 extended format only */
#define	ETDESC0_IC		(1U << 30)	/* Interrupt on Completion */
#define	ETDESC0_LS		(1U << 29)	/* Last Segment */
#define	ETDESC0_FS		(1U << 28)	/* First Segment */
#define	ETDESC0_DC		(1U << 27)	/* Disable CRC */
#define	ETDESC0_DP		(1U << 26)	/* Disable Padding */
#define	ETDESC0_CIC_NONE	(0U << 22)	/* Checksum Insertion Control */
#define	ETDESC0_CIC_HDR		(1U << 22)
#define	ETDESC0_CIC_SEG 	(2U << 22)
#define	ETDESC0_CIC_FULL	(3U << 22)
#define	ETDESC0_TER		(1U << 21)	/* Transmit End of Ring */
#define	ETDESC0_TCH		(1U << 20)	/* Second Address Chained */

/* TX descriptors - TDESC1 normal format */
#define	NTDESC1_IC		(1U << 31)	/* Interrupt on Completion */
#define	NTDESC1_LS		(1U << 30)	/* Last Segment */
#define	NTDESC1_FS		(1U << 29)	/* First Segment */
#define	NTDESC1_CIC_NONE	(0U << 27)	/* Checksum Insertion Control */
#define	NTDESC1_CIC_HDR		(1U << 27)
#define	NTDESC1_CIC_SEG 	(2U << 27)
#define	NTDESC1_CIC_FULL	(3U << 27)
#define	NTDESC1_DC		(1U << 26)	/* Disable CRC */
#define	NTDESC1_TER		(1U << 25)	/* Transmit End of Ring */
#define	NTDESC1_TCH		(1U << 24)	/* Second Address Chained */
/* TX descriptors - TDESC1 extended format */
#define	ETDESC1_DP		(1U << 23)	/* Disable Padding */
#define	ETDESC1_TBS2_MASK	0x7ff
#define	ETDESC1_TBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	ETDESC1_TBS1_MASK	0x7ff
#define	ETDESC1_TBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptor - RDESC0 is unified */
#define	RDESC0_OWN		(1U << 31)
#define	RDESC0_AFM		(1U << 30)	/* Dest. Address Filter Fail */
#define	RDESC0_FL_MASK		0x3fff
#define	RDESC0_FL_SHIFT		16		/* Frame Length */
#define	RDESC0_ES		(1U << 15)	/* Error Summary */
#define	RDESC0_DE		(1U << 14)	/* Descriptor Error */
#define	RDESC0_SAF		(1U << 13)	/* Source Address Filter Fail */
#define	RDESC0_LE		(1U << 12)	/* Length Error */
#define	RDESC0_OE		(1U << 11)	/* Overflow Error */
#define	RDESC0_VLAN		(1U << 10)	/* VLAN Tag */
#define	RDESC0_FS		(1U <<  9)	/* First Descriptor */
#define	RDESC0_LS		(1U <<  8)	/* Last Descriptor */
#define	RDESC0_ICE		(1U <<  7)	/* IPC Checksum Error */
#define	RDESC0_LC		(1U <<  6)	/* Late Collision */
#define	RDESC0_FT		(1U <<  5)	/* Frame Type */
#define	RDESC0_RWT		(1U <<  4)	/* Receive Watchdog Timeout */
#define	RDESC0_RE		(1U <<  3)	/* Receive Error */
#define	RDESC0_DBE		(1U <<  2)	/* Dribble Bit Error */
#define	RDESC0_CE		(1U <<  1)	/* CRC Error */
#define	RDESC0_PCE		(1U <<  0)	/* Payload Checksum Error */
#define	RDESC0_RXMA		(1U <<  0)	/* Rx MAC Address */

/* RX descriptors - RDESC1 normal format */
#define	NRDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	NRDESC1_RER		(1U << 25)	/* Receive End of Ring */
#define	NRDESC1_RCH		(1U << 24)	/* Second Address Chained */
#define	NRDESC1_RBS2_MASK	0x7ff
#define	NRDESC1_RBS2_SHIFT	11		/* Receive Buffer 2 Size */
#define	NRDESC1_RBS1_MASK	0x7ff
#define	NRDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/* RX descriptors - RDESC1 enhanced format */
#define	ERDESC1_DIC		(1U << 31)	/* Disable Intr on Completion */
#define	ERDESC1_RBS2_MASK	0x7ffff
#define	ERDESC1_RBS2_SHIFT	16		/* Receive Buffer 2 Size */
#define	ERDESC1_RER		(1U << 15)	/* Receive End of Ring */
#define	ERDESC1_RCH		(1U << 14)	/* Second Address Chained */
#define	ERDESC1_RBS1_MASK	0x7ffff
#define	ERDESC1_RBS1_SHIFT	0		/* Receive Buffer 1 Size */

/*
 * The hardware imposes alignment restrictions on various objects involved in
 * DMA transfers.  These values are expressed in bytes (not bits).
 */
#define	DWC_DESC_RING_ALIGN	2048

static inline uint32_t
next_txidx(struct dwc_softc *sc, uint32_t curidx)
{

	return ((curidx + 1) % TX_DESC_COUNT);
}

static inline uint32_t
next_rxidx(struct dwc_softc *sc, uint32_t curidx)
{

	return ((curidx + 1) % RX_DESC_COUNT);
}

static void
dwc_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

inline static void
txdesc_clear(struct dwc_softc *sc, int idx)
{

	sc->tx_desccount--;
	sc->txdesc_ring[idx].addr1 = (uint32_t)(0);
	sc->txdesc_ring[idx].desc0 = 0;
	sc->txdesc_ring[idx].desc1 = 0;
}

inline static void
txdesc_setup(struct dwc_softc *sc, int idx, bus_addr_t paddr,
  uint32_t len, uint32_t flags, bool first, bool last)
{
	uint32_t desc0, desc1;

	if (!sc->dma_ext_desc) {
		desc0 = 0;
		desc1 = NTDESC1_TCH | len | flags;
		if (first)
			desc1 |=  NTDESC1_FS;
		if (last)
			desc1 |= NTDESC1_LS | NTDESC1_IC;
	} else {
		desc0 = ETDESC0_TCH | flags;
		if (first)
			desc0 |= ETDESC0_FS;
		if (last)
			desc0 |= ETDESC0_LS | ETDESC0_IC;
		desc1 = len;
	}
	++sc->tx_desccount;
	sc->txdesc_ring[idx].addr1 = (uint32_t)(paddr);
	sc->txdesc_ring[idx].desc0 = desc0;
	sc->txdesc_ring[idx].desc1 = desc1;
	wmb();
	sc->txdesc_ring[idx].desc0 |= TDESC0_OWN;
	wmb();
}

inline static uint32_t
rxdesc_setup(struct dwc_softc *sc, int idx, bus_addr_t paddr)
{
	uint32_t nidx;

	sc->rxdesc_ring[idx].addr1 = (uint32_t)paddr;
	nidx = next_rxidx(sc, idx);
	sc->rxdesc_ring[idx].addr2 = sc->rxdesc_ring_paddr +
	    (nidx * sizeof(struct dwc_hwdesc));
	if (!sc->dma_ext_desc)
		sc->rxdesc_ring[idx].desc1 = NRDESC1_RCH |
		    MIN(MCLBYTES, NRDESC1_RBS1_MASK);
	else
		sc->rxdesc_ring[idx].desc1 = ERDESC1_RCH |
		    MIN(MCLBYTES, ERDESC1_RBS1_MASK);

	wmb();
	sc->rxdesc_ring[idx].desc0 = RDESC0_OWN;
	wmb();
	return (nidx);
}

int
dma1000_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp)
{
	struct bus_dma_segment segs[TX_MAP_MAX_SEGS];
	int error, nsegs;
	struct mbuf * m;
	uint32_t flags = 0;
	int i;
	int last;

	error = bus_dmamap_load_mbuf_sg(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    *mp, segs, &nsegs, 0);
	if (error == EFBIG) {
		/*
		 * The map may be partially mapped from the first call.
		 * Make sure to reset it.
		 */
		bus_dmamap_unload(sc->txbuf_tag, sc->txbuf_map[idx].map);
		if ((m = m_defrag(*mp, M_NOWAIT)) == NULL)
			return (ENOMEM);
		*mp = m;
		error = bus_dmamap_load_mbuf_sg(sc->txbuf_tag, sc->txbuf_map[idx].map,
		    *mp, segs, &nsegs, 0);
	}
	if (error != 0)
		return (ENOMEM);

	if (sc->tx_desccount + nsegs > TX_DESC_COUNT) {
		bus_dmamap_unload(sc->txbuf_tag, sc->txbuf_map[idx].map);
		return (ENOMEM);
	}

	m = *mp;

	if ((m->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		if ((m->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_UDP)) != 0) {
			if (!sc->dma_ext_desc)
				flags = NTDESC1_CIC_FULL;
			else
				flags = ETDESC0_CIC_FULL;
		} else {
			if (!sc->dma_ext_desc)
				flags = NTDESC1_CIC_HDR;
			else
				flags = ETDESC0_CIC_HDR;
		}
	}

	bus_dmamap_sync(sc->txbuf_tag, sc->txbuf_map[idx].map,
	    BUS_DMASYNC_PREWRITE);

	sc->txbuf_map[idx].mbuf = m;

	for (i = 0; i < nsegs; i++) {
		txdesc_setup(sc, sc->tx_desc_head,
		    segs[i].ds_addr, segs[i].ds_len,
		    (i == 0) ? flags : 0, /* only first desc needs flags */
		    (i == 0),
		    (i == nsegs - 1));
		last = sc->tx_desc_head;
		sc->tx_desc_head = next_txidx(sc, sc->tx_desc_head);
	}

	sc->txbuf_map[idx].last_desc_idx = last;

	return (0);
}

static int
dma1000_setup_rxbuf(struct dwc_softc *sc, int idx, struct mbuf *m)
{
	struct bus_dma_segment seg;
	int error, nsegs;

	m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	    m, &seg, &nsegs, 0);
	if (error != 0)
		return (error);

	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	bus_dmamap_sync(sc->rxbuf_tag, sc->rxbuf_map[idx].map,
	    BUS_DMASYNC_PREREAD);

	sc->rxbuf_map[idx].mbuf = m;
	rxdesc_setup(sc, idx, seg.ds_addr);

	return (0);
}

static struct mbuf *
dwc_alloc_mbufcl(struct dwc_softc *sc)
{
	struct mbuf *m;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m != NULL)
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	return (m);
}

static struct mbuf *
dwc_rxfinish_one(struct dwc_softc *sc, struct dwc_hwdesc *desc,
    struct dwc_bufmap *map)
{
	if_t ifp;
	struct mbuf *m, *m0;
	int len;
	uint32_t rdesc0;

	m = map->mbuf;
	ifp = sc->ifp;
	rdesc0 = desc ->desc0;

	if ((rdesc0 & (RDESC0_FS | RDESC0_LS)) !=
		    (RDESC0_FS | RDESC0_LS)) {
		/*
		 * Something very wrong happens. The whole packet should be
		 * recevied in one descriptr. Report problem.
		 */
		device_printf(sc->dev,
		    "%s: RX descriptor without FIRST and LAST bit set: 0x%08X",
		    __func__, rdesc0);
		return (NULL);
	}

	len = (rdesc0 >> RDESC0_FL_SHIFT) & RDESC0_FL_MASK;
	if (len < 64) {
		/*
		 * Lenght is invalid, recycle old mbuf
		 * Probably impossible case
		 */
		return (NULL);
	}

	/* Allocate new buffer */
	m0 = dwc_alloc_mbufcl(sc);
	if (m0 == NULL) {
		/* no new mbuf available, recycle old */
		if_inc_counter(sc->ifp, IFCOUNTER_IQDROPS, 1);
		return (NULL);
	}
	/* Do dmasync for newly received packet */
	bus_dmamap_sync(sc->rxbuf_tag, map->map, BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->rxbuf_tag, map->map);

	/* Received packet is valid, process it */
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = len;
	m->m_len = len;
	if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

	if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0 &&
	  (rdesc0 & RDESC0_FT) != 0) {
		m->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
		if ((rdesc0 & RDESC0_ICE) == 0)
			m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		if ((rdesc0 & RDESC0_PCE) == 0) {
			m->m_pkthdr.csum_flags |=
				CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xffff;
		}
	}

	/* Remove trailing FCS */
	m_adj(m, -ETHER_CRC_LEN);

	DWC_UNLOCK(sc);
	if_input(ifp, m);
	DWC_LOCK(sc);
	return (m0);
}

void
dma1000_txfinish_locked(struct dwc_softc *sc)
{
	struct dwc_bufmap *bmap;
	struct dwc_hwdesc *desc;
	if_t ifp;
	int idx, last_idx;
	bool map_finished;

	DWC_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	/* check if all descriptors of the map are done */
	while (sc->tx_map_tail != sc->tx_map_head) {
		map_finished = true;
		bmap = &sc->txbuf_map[sc->tx_map_tail];
		idx = sc->tx_desc_tail;
		last_idx = next_txidx(sc, bmap->last_desc_idx);
		while (idx != last_idx) {
			desc = &sc->txdesc_ring[idx];
			if ((desc->desc0 & TDESC0_OWN) != 0) {
				map_finished = false;
				break;
			}
			idx = next_txidx(sc, idx);
		}

		if (!map_finished)
			break;
		bus_dmamap_sync(sc->txbuf_tag, bmap->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txbuf_tag, bmap->map);
		m_freem(bmap->mbuf);
		bmap->mbuf = NULL;
		sc->tx_mapcount--;
		while (sc->tx_desc_tail != last_idx) {
			txdesc_clear(sc, sc->tx_desc_tail);
			sc->tx_desc_tail = next_txidx(sc, sc->tx_desc_tail);
		}
		sc->tx_map_tail = next_txidx(sc, sc->tx_map_tail);
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	}

	/* If there are no buffers outstanding, muzzle the watchdog. */
	if (sc->tx_desc_tail == sc->tx_desc_head) {
		sc->tx_watchdog_count = 0;
	}
}

void
dma1000_txstart(struct dwc_softc *sc)
{
	int enqueued;
	struct mbuf *m;

	enqueued = 0;

	for (;;) {
		if (sc->tx_desccount > (TX_DESC_COUNT - TX_MAP_MAX_SEGS  + 1)) {
			if_setdrvflagbits(sc->ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		if (sc->tx_mapcount == (TX_MAP_COUNT - 1)) {
			if_setdrvflagbits(sc->ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		m = if_dequeue(sc->ifp);
		if (m == NULL)
			break;
		if (dma1000_setup_txbuf(sc, sc->tx_map_head, &m) != 0) {
			if_sendq_prepend(sc->ifp, m);
			if_setdrvflagbits(sc->ifp, IFF_DRV_OACTIVE, 0);
			break;
		}
		bpf_mtap_if(sc->ifp, m);
		sc->tx_map_head = next_txidx(sc, sc->tx_map_head);
		sc->tx_mapcount++;
		++enqueued;
	}

	if (enqueued != 0) {
		WRITE4(sc, TRANSMIT_POLL_DEMAND, 0x1);
		sc->tx_watchdog_count = WATCHDOG_TIMEOUT_SECS;
	}
}

void
dma1000_rxfinish_locked(struct dwc_softc *sc)
{
	struct mbuf *m;
	int error, idx;
	struct dwc_hwdesc *desc;

	DWC_ASSERT_LOCKED(sc);
	for (;;) {
		idx = sc->rx_idx;
		desc = sc->rxdesc_ring + idx;
		if ((desc->desc0 & RDESC0_OWN) != 0)
			break;

		m = dwc_rxfinish_one(sc, desc, sc->rxbuf_map + idx);
		if (m == NULL) {
			wmb();
			desc->desc0 = RDESC0_OWN;
			wmb();
		} else {
			/* We cannot create hole in RX ring */
			error = dma1000_setup_rxbuf(sc, idx, m);
			if (error != 0)
				panic("dma1000_setup_rxbuf failed:  error %d\n",
				    error);

		}
		sc->rx_idx = next_rxidx(sc, sc->rx_idx);
	}
}

/*
 * Start the DMA controller
 */
void
dma1000_start(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Initializa DMA and enable transmitters */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_TSF | MODE_OSF | MODE_FUF);
	reg &= ~(MODE_RSF);
	reg |= (MODE_RTC_LEV32 << MODE_RTC_SHIFT);
	WRITE4(sc, OPERATION_MODE, reg);

	WRITE4(sc, INTERRUPT_ENABLE, INT_EN_DEFAULT);

	/* Start DMA */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_ST | MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);
}

/*
 * Stop the DMA controller
 */
void
dma1000_stop(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Stop DMA TX */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_ST);
	WRITE4(sc, OPERATION_MODE, reg);

	/* Flush TX */
	reg = READ4(sc, OPERATION_MODE);
	reg |= (MODE_FTF);
	WRITE4(sc, OPERATION_MODE, reg);

	/* Stop DMA RX */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);
}

int
dma1000_reset(struct dwc_softc *sc)
{
	uint32_t reg;
	int i;

	reg = READ4(sc, BUS_MODE);
	reg |= (BUS_MODE_SWR);
	WRITE4(sc, BUS_MODE, reg);

	for (i = 0; i < DMA_RESET_TIMEOUT; i++) {
		if ((READ4(sc, BUS_MODE) & BUS_MODE_SWR) == 0)
			break;
		DELAY(10);
	}
	if (i >= DMA_RESET_TIMEOUT) {
		return (ENXIO);
	}

	return (0);
}

/*
 * Create the bus_dma resources
 */
int
dma1000_init(struct dwc_softc *sc)
{
	struct mbuf *m;
	uint32_t reg;
	int error;
	int nidx;
	int idx;

	reg = BUS_MODE_USP;
	if (!sc->nopblx8)
		reg |= BUS_MODE_EIGHTXPBL;
	reg |= (sc->txpbl << BUS_MODE_PBL_SHIFT);
	reg |= (sc->rxpbl << BUS_MODE_RPBL_SHIFT);
	if (sc->fixed_burst)
		reg |= BUS_MODE_FIXEDBURST;
	if (sc->mixed_burst)
		reg |= BUS_MODE_MIXEDBURST;
	if (sc->aal)
		reg |= BUS_MODE_AAL;

	WRITE4(sc, BUS_MODE, reg);

	reg = READ4(sc, HW_FEATURE);
	if (reg & HW_FEATURE_EXT_DESCRIPTOR)
		sc->dma_ext_desc = true;

	/*
	 * DMA must be stop while changing descriptor list addresses.
	 */
	reg = READ4(sc, OPERATION_MODE);
	reg &= ~(MODE_ST | MODE_SR);
	WRITE4(sc, OPERATION_MODE, reg);

	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    TX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    TX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->txdesc_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->txdesc_tag, (void**)&sc->txdesc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->txdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate TX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->txdesc_tag, sc->txdesc_map,
	    sc->txdesc_ring, TX_DESC_SIZE, dwc_get1paddr,
	    &sc->txdesc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not load TX descriptor ring map.\n");
		goto out;
	}

	for (idx = 0; idx < TX_DESC_COUNT; idx++) {
		nidx = next_txidx(sc, idx);
		sc->txdesc_ring[idx].addr2 = sc->txdesc_ring_paddr +
		    (nidx * sizeof(struct dwc_hwdesc));
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES*TX_MAP_MAX_SEGS,	/* maxsize */
	    TX_MAP_MAX_SEGS,		/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->txbuf_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create TX ring DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < TX_MAP_COUNT; idx++) {
		error = bus_dmamap_create(sc->txbuf_tag, BUS_DMA_COHERENT,
		    &sc->txbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create TX buffer DMA map.\n");
			goto out;
		}
	}

	for (idx = 0; idx < TX_DESC_COUNT; idx++)
		txdesc_clear(sc, idx);

	WRITE4(sc, TX_DESCR_LIST_ADDR, sc->txdesc_ring_paddr);

	/*
	 * Set up RX descriptor ring, descriptors, dma maps, and mbufs.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RX_DESC_SIZE, 1, 		/* maxsize, nsegments */
	    RX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxdesc_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create RX ring DMA tag.\n");
		goto out;
	}

	error = bus_dmamem_alloc(sc->rxdesc_tag, (void **)&sc->rxdesc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->rxdesc_map);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not allocate RX descriptor ring.\n");
		goto out;
	}

	error = bus_dmamap_load(sc->rxdesc_tag, sc->rxdesc_map,
	    sc->rxdesc_ring, RX_DESC_SIZE, dwc_get1paddr,
	    &sc->rxdesc_ring_paddr, 0);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not load RX descriptor ring map.\n");
		goto out;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1, 		/* maxsize, nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rxbuf_tag);
	if (error != 0) {
		device_printf(sc->dev,
		    "could not create RX buf DMA tag.\n");
		goto out;
	}

	for (idx = 0; idx < RX_DESC_COUNT; idx++) {
		error = bus_dmamap_create(sc->rxbuf_tag, BUS_DMA_COHERENT,
		    &sc->rxbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create RX buffer DMA map.\n");
			goto out;
		}
		if ((m = dwc_alloc_mbufcl(sc)) == NULL) {
			device_printf(sc->dev, "Could not alloc mbuf\n");
			error = ENOMEM;
			goto out;
		}
		if ((error = dma1000_setup_rxbuf(sc, idx, m)) != 0) {
			device_printf(sc->dev,
			    "could not create new RX buffer.\n");
			goto out;
		}
	}
	WRITE4(sc, RX_DESCR_LIST_ADDR, sc->rxdesc_ring_paddr);

out:
	if (error != 0)
		return (ENXIO);

	return (0);
}

/*
 * Free the bus_dma resources
 */
void
dma1000_free(struct dwc_softc *sc)
{
	bus_dmamap_t map;
	int idx;

	/* Clean up RX DMA resources and free mbufs. */
	for (idx = 0; idx < RX_DESC_COUNT; ++idx) {
		if ((map = sc->rxbuf_map[idx].map) != NULL) {
			bus_dmamap_unload(sc->rxbuf_tag, map);
			bus_dmamap_destroy(sc->rxbuf_tag, map);
			m_freem(sc->rxbuf_map[idx].mbuf);
		}
	}
	if (sc->rxbuf_tag != NULL)
		bus_dma_tag_destroy(sc->rxbuf_tag);
	if (sc->rxdesc_map != NULL) {
		bus_dmamap_unload(sc->rxdesc_tag, sc->rxdesc_map);
		bus_dmamem_free(sc->rxdesc_tag, sc->rxdesc_ring,
		    sc->rxdesc_map);
	}
	if (sc->rxdesc_tag != NULL)
		bus_dma_tag_destroy(sc->rxdesc_tag);

	/* Clean up TX DMA resources. */
	for (idx = 0; idx < TX_DESC_COUNT; ++idx) {
		if ((map = sc->txbuf_map[idx].map) != NULL) {
			/* TX maps are already unloaded. */
			bus_dmamap_destroy(sc->txbuf_tag, map);
		}
	}
	if (sc->txbuf_tag != NULL)
		bus_dma_tag_destroy(sc->txbuf_tag);
	if (sc->txdesc_map != NULL) {
		bus_dmamap_unload(sc->txdesc_tag, sc->txdesc_map);
		bus_dmamem_free(sc->txdesc_tag, sc->txdesc_ring,
		    sc->txdesc_map);
	}
	if (sc->txdesc_tag != NULL)
		bus_dma_tag_destroy(sc->txdesc_tag);
}

/*
 * Interrupt function
 */

int
dma1000_intr(struct dwc_softc *sc)
{
	uint32_t reg;
	int rv;

	DWC_ASSERT_LOCKED(sc);

	rv = 0;
	reg = READ4(sc, DMA_STATUS);
	if (reg & DMA_STATUS_NIS) {
		if (reg & DMA_STATUS_RI)
			dma1000_rxfinish_locked(sc);

		if (reg & DMA_STATUS_TI) {
			dma1000_txfinish_locked(sc);
			dma1000_txstart(sc);
		}
	}

	if (reg & DMA_STATUS_AIS) {
		if (reg & DMA_STATUS_FBI) {
			/* Fatal bus error */
			rv = EIO;
		}
	}

	WRITE4(sc, DMA_STATUS, reg & DMA_STATUS_INTR_MASK);
	return (rv);
}
