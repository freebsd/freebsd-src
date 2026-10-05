/*-
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#include <dev/clk/clk.h>
#include <dev/hwreset/hwreset.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/dwc/if_dwcvar.h>
#include <dev/dwc/dwc4_reg.h>
#include <dev/dwc/dwc4_dma.h>
#include <dev/dwc/dwc4_mtl.h>

#define	WATCHDOG_TIMEOUT_SECS	5
#define	DMA_RESET_TIMEOUT	100

/* RDES1 Write-back format */
#define	RDES1_IPCE	(1 << 7) /* IP Payload Error;
				  * 16-bit IP payload checksum mismatch.
				  */
#define	RDES1_IPCB	(1 << 6) /* Checksum offload engine is bypassed. */
#define	RDES1_IPHE	(1 << 3) /*
				  * IP Header Error;
				  * 16-bit IP header checksum mismatch.
				  */
/* RX Read format */
#define	RDES3_OWN	(1 << 31) /* DMA owns the descriptor */
#define	RDES3_IOC	(1 << 30) /* Interrupt Enabled on Completion */
#define	RDES3_BUF2V	(1 << 25) /* Buffer 2 Address Valid */
#define	RDES3_BUF1V	(1 << 24) /* Buffer 1 Address Valid */

/* RX Write-back format */
#define	RDES3_FD	(1 << 29) /* First Descriptor */
#define	RDES3_LD	(1 << 28) /* Last Descriptor */
#define	RDES3_RE	(1 << 20) /* Receive Error */
#define	RDES3_PL_S	0 /* Packet Length */
#define	RDES3_PL_M	(0x7fff << RDES3_PL_S)
#define	RDES3_CE	(1 << 24) /* CRC Error */
#define	RDES3_LT_S	16 /* Length/Type Field */
#define	RDES3_LT_M	(0x3 << RDES3_LT_S)

/* TX Read format */
#define	TDES2_IOC	(1 << 31) /* Interrupt on completion */
#define	TDES2_TTSE	(1 << 30) /* Transmit Timestamp Enable */
#define	TDES2_B2L_S	16 /* Buffer 2 Length */
#define	TDES2_B2L_M	(0x3fff << TDES2_B2L_S)
#define	TDES2_VTIR_S	14 /* VLAN Tag Insertion or Replacement */
#define	TDES2_VTIR_M	(0x3 << TDES2_VTIR_S)

#define	TDES3_OWN	(1 << 31) /* the DMA owns the descriptor */
#define	TDES3_CTXT	(1 << 30) /* Context Type */
#define	TDES3_FD	(1 << 29) /* First Descriptor */
#define	TDES3_LD	(1 << 28) /* Last Descriptor */
#define	TDES3_CPC_S		26
#define	TDES3_CPC_M		(0x3 << TDES3_CPC_S)
#define	TDES3_CPC_CRC_PAD	(0x0 << TDES3_CPC_S)
#define	TDES3_CPC_CRC		(0x1 << TDES3_CPC_S)
#define	TDES3_CPC_CRC_DISABLE	(0x2 << TDES3_CPC_S)
#define	TDES3_CPC_CRC_REPLACE	(0x3 << TDES3_CPC_S)
#define	TDES3_SAIC_S	23
#define	TDES3_SAIC_M	(0x7 << TDES3_SAIC_S)
#define	TDES3_TSE	18 /* TCP Segmentation Enable */
#define	TDES3_CIC_S	16
#define	TDES3_CIC_M	(0x3 << TDES3_CIC_S)
#define	TDES3_CIC_HDR	(0x1 << TDES3_CIC_S) /* Only IP header csum inserted */
#define	TDES3_CIC_FULL	(0x2 << TDES3_CIC_S) /* IP header & payload */
#define	TDES3_CIC_FULLP	(0x3 << TDES3_CIC_S) /* IP header, payload, pseudo-hdr*/
#define	TDES3_TPL_S	0 /* TCP Payload Length */
#define	TDES3_TPL_M	(0x3ffff << TDES3_TPL_S)

/*
 * A hardware buffer descriptor.  Rx and Tx buffers have the same descriptor
 * layout, but the bits in the fields have different meanings.
 */
struct dwc_hwdesc
{
	uint32_t tdes0;	/* Header or Buffer 1 Address[31:0] */
	uint32_t tdes1; /* Buffer 2 Address [31:0] or Buffer 1 Address[63:32] */
	uint32_t tdes2;
	uint32_t tdes3;
};

#define	RX_DESC_SIZE	(sizeof(struct dwc_hwdesc) * RX_DESC_COUNT)
#define	TX_DESC_SIZE	(sizeof(struct dwc_hwdesc) * TX_DESC_COUNT)

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
	sc->txdesc_ring[idx].tdes0 = 0;
	sc->txdesc_ring[idx].tdes1 = 0;
	sc->txdesc_ring[idx].tdes2 = 0;
	sc->txdesc_ring[idx].tdes3 = 0;
}

inline static void
txdesc_setup(struct dwc_softc *sc, int idx, bus_addr_t paddr,
  uint32_t len, uint32_t flags, bool first, bool last)
{
	uint32_t tdes0, tdes1, tdes2, tdes3;

	if (!sc->dma_ext_desc) {
		tdes0 = (uint32_t)paddr;
		tdes1 = (uint32_t)((uint64_t)paddr >> 32);
		tdes2 = len | TDES2_TTSE;
		tdes3 = flags;

		if (first)
			tdes3 |= TDES3_FD;
		if (last) {
			tdes2 |= TDES2_IOC;
			tdes3 |= TDES3_LD;
		}
	} else {
		tdes0 = 0;
		tdes1 = 0;
		tdes2 = 0;
		tdes3 = 0;
	}
	++sc->tx_desccount;
	sc->txdesc_ring[idx].tdes0 = tdes0;
	sc->txdesc_ring[idx].tdes1 = tdes1;
	sc->txdesc_ring[idx].tdes2 = tdes2;
	sc->txdesc_ring[idx].tdes3 = tdes3;

	wmb();
	sc->txdesc_ring[idx].tdes3 |= TDES3_OWN;
	wmb();
}

inline static uint32_t
rxdesc_setup(struct dwc_softc *sc, int idx, bus_addr_t paddr)
{

	sc->rxdesc_ring[idx].tdes0 = (uint32_t)paddr;
	sc->rxdesc_ring[idx].tdes1 = (uint32_t)((uint64_t)paddr >> 32);
	sc->rxdesc_ring[idx].tdes2 = 0;
	sc->rxdesc_ring[idx].tdes3 = RDES3_IOC | RDES3_BUF1V;

	wmb();
	sc->rxdesc_ring[idx].tdes3 |= RDES3_OWN;
	wmb();

	return (0);
}

int
dma4_setup_txbuf(struct dwc_softc *sc, int idx, struct mbuf **mp)
{
	struct bus_dma_segment segs[TX_MAP_MAX_SEGS];
	int error, nsegs;
	struct mbuf * m;
	uint32_t flags;
	uint32_t csum_flags;
	int i;
	int last;

	flags = 0;
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
		error = bus_dmamap_load_mbuf_sg(sc->txbuf_tag,
		    sc->txbuf_map[idx].map, *mp, segs, &nsegs, 0);
	}
	if (error != 0)
		return (ENOMEM);

	if (sc->tx_desccount + nsegs > TX_DESC_COUNT) {
		bus_dmamap_unload(sc->txbuf_tag, sc->txbuf_map[idx].map);
		return (ENOMEM);
	}

	m = *mp;

	csum_flags = CSUM_TCP | CSUM_UDP | CSUM_IP6_TCP | CSUM_IP6_UDP;
	if ((m->m_pkthdr.csum_flags & csum_flags) != 0) {
		if (!sc->dma_ext_desc)
			flags = TDES3_CIC_FULLP;
		else
			panic("implement me");
	} else if ((m->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		if (!sc->dma_ext_desc)
			flags = TDES3_CIC_HDR;
		else
			panic("implement me");
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

	wmb();

	WRITE4(sc, ETH_DMACTXDTPR(0), sc->txdesc_ring_paddr +
	    sc->tx_desc_head * sizeof(struct dwc_hwdesc));

	return (0);
}

static int
dma4_setup_rxbuf(struct dwc_softc *sc, int idx, struct mbuf *m)
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
	uint32_t rdes1;
	uint32_t rdes3;

	m = map->mbuf;
	ifp = sc->ifp;
	rdes1 = desc->tdes1;
	rdes3 = desc->tdes3;

	if ((rdes3 & (RDES3_FD | RDES3_LD)) != (RDES3_FD | RDES3_LD)) {
		/*
		 * Something very wrong happens. The whole packet should be
		 * received in one descriptor. Report problem.
		 */
		device_printf(sc->dev,
		    "%s: RX descriptor without FIRST and LAST bit set: 0x%08X",
		    __func__, rdes3);
		return (NULL);
	}

	len = (rdes3 & RDES3_PL_M) >> RDES3_PL_S;

	/* Allocate new buffer */
	m0 = dwc_alloc_mbufcl(sc);
	if (m0 == NULL) {
		device_printf(sc->dev, "no mbufs\n");

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

	if ((if_getcapenable(ifp) & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6)) != 0 &&
	    (rdes1 & RDES1_IPCB) == 0) {
		m->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
		if ((rdes1 & RDES1_IPCE) == 0)
			m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		if ((rdes1 & RDES1_IPHE) == 0) {
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
dma4_txfinish_locked(struct dwc_softc *sc)
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
			if ((desc->tdes3 & TDES3_OWN) != 0) {
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
dma4_txstart(struct dwc_softc *sc)
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
		if (dma4_setup_txbuf(sc, sc->tx_map_head, &m) != 0) {
			if_sendq_prepend(sc->ifp, m);
			if_setdrvflagbits(sc->ifp, IFF_DRV_OACTIVE, 0);
			break;
		}
		bpf_mtap_if(sc->ifp, m);
		sc->tx_map_head = next_txidx(sc, sc->tx_map_head);
		sc->tx_mapcount++;
		++enqueued;
	}

	if (enqueued != 0)
		sc->tx_watchdog_count = WATCHDOG_TIMEOUT_SECS;
}

void
dma4_rxfinish_locked(struct dwc_softc *sc)
{
	struct mbuf *m;
	int error, idx;
	struct dwc_hwdesc *desc;

	DWC_ASSERT_LOCKED(sc);

	for (;;) {
		idx = sc->rx_idx;
		desc = sc->rxdesc_ring + idx;
		if (desc->tdes3 & RDES3_OWN)
			break;
		m = dwc_rxfinish_one(sc, desc, sc->rxbuf_map + idx);
		if (m == NULL) {
			desc->tdes3 = RDES3_BUF1V | RDES3_IOC | RDES3_OWN;
			wmb();
		} else {
			/* We cannot create hole in RX ring */
			error = dma4_setup_rxbuf(sc, idx, m);
			if (error != 0)
				panic("dma4_setup_rxbuf failed:  error %d\n",
				    error);
		}

		WRITE4(sc, ETH_DMACRXDTPR(0), sc->rxdesc_ring_paddr +
		    idx * sizeof(struct dwc_hwdesc));

		sc->rx_idx = next_rxidx(sc, sc->rx_idx);
	}
}

/*
 * Start the DMA controller
 */
void
dma4_start(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Initialize DMA and enable transmitters */

	/* Configure TX channel 0 */
	reg = READ4(sc, ETH_DMACTXCR(0));
	reg |= DMACTXCR_OSF;
	WRITE4(sc, ETH_DMACTXCR(0), reg);

	reg = MTLTXQOMR_TSF; /* Transmit Store and Forward */
	reg |= MTLTXQOMR_EN;
	reg |= MTLTXQOMR_TQS_8K;
	WRITE4(sc, ETH_MTLTXQOMR(0), reg);

	/* Configure RX channel 0 */
	reg = MTLRXQOMR_RSF;
	reg |= MTLRXQOMR_EHFC;
	reg |= MTLRXQOMR_RQS_4K;
	WRITE4(sc, ETH_MTLRXQOMR(0), reg);

	reg = READ4(sc, ETH_MACRXQC0R(0));
	reg |= MACRXQC0R_RXQ0EN_GEN;
	WRITE4(sc, ETH_MACRXQC0R(0), reg);

	/* Enable Interrupts. */
	reg = DMACIER_TIE | DMACIER_TXSE | DMACIER_TBUE | DMACIER_RIE |
	    DMACIER_RBUE | DMACIER_RSE | DMACIER_RWTE | DMACIER_ETIE |
	    DMACIER_ERIE | DMACIER_FBEE | DMACIER_CDEE | DMACIER_AIE |
	    DMACIER_NIE;
	WRITE4(sc, ETH_DMACIER(0), reg);

	/* Start DMA RX. */
	reg = READ4(sc, ETH_DMACRXCR(0));
	reg |= DMACRXCR_SR;
	WRITE4(sc, ETH_DMACRXCR(0), reg);

	/* Start DMA TX. */
	reg = READ4(sc, ETH_DMACTXCR(0));
	reg |= DMACTXCR_ST;
	WRITE4(sc, ETH_DMACTXCR(0), reg);
}

/*
 * Stop the DMA controller
 */
void
dma4_stop(struct dwc_softc *sc)
{
	uint32_t reg;

	DWC_ASSERT_LOCKED(sc);

	/* Stop DMA TX */
	reg = READ4(sc, ETH_DMACTXCR(0));
	reg &= ~DMACTXCR_ST;
	WRITE4(sc, ETH_DMACTXCR(0), reg);

	/* Flush TX */
	reg = READ4(sc, ETH_MTLTXQOMR(0));
	reg |= MTLTXQOMR_FTQ;
	WRITE4(sc, ETH_MTLTXQOMR(0), reg);

	/* Stop DMA RX */
	reg = READ4(sc, ETH_DMACRXCR(0));
	reg &= ~DMACRXCR_SR;
	WRITE4(sc, ETH_DMACRXCR(0), reg);
}

int
dma4_reset(struct dwc_softc *sc)
{
	uint32_t reg;
	int i;

	reg = READ4(sc, ETH_DMAMR);
	reg |= DMAMR_SWR;
	WRITE4(sc, ETH_DMAMR, reg);

	for (i = 0; i < DMA_RESET_TIMEOUT; i++) {
		if ((READ4(sc, ETH_DMAMR) & DMAMR_SWR) == 0)
			break;
		DELAY(10);
	}
	if (i >= DMA_RESET_TIMEOUT)
		return (ENXIO);

	return (0);
}

/*
 * Create the bus_dma resources
 */
int
dma4_init(struct dwc_softc *sc)
{
	struct mbuf *m;
	uint32_t reg;
	int error;
	int idx;

	WRITE4(sc, ETH_MACQ0TXFCR, 0xffff << 16 | 1);
	WRITE4(sc, ETH_MACRXFCR, 1);

	reg = READ4(sc, ETH_DMACCR(0));
	if (sc->nopblx8)
		reg &= ~DMACCR_PBLX8;
	else
		reg |= DMACCR_PBLX8;
	WRITE4(sc, ETH_DMACCR(0), reg);

	WRITE4(sc, ETH_DMACTXCR(0), 0);

	reg = (MCLBYTES << DMACRXCR_RBSZ_S);
	WRITE4(sc, ETH_DMACRXCR(0), reg);

	reg = DMASBMR_BLEN4;
	reg |= DMASBMR_BLEN8;
	reg |= DMASBMR_BLEN16;
	reg |= DMASBMR_EAME;
	WRITE4(sc, ETH_DMASBMR, reg);

	reg = READ4(sc, ETH_DMASBMR);
	if (sc->fixed_burst)
		reg |= DMASBMR_FB;
	else
		reg &= ~DMASBMR_FB;
	if (sc->aal)
		reg |= DMASBMR_AAL;
	else
		reg &= ~DMASBMR_AAL;
	WRITE4(sc, ETH_DMASBMR, reg);

	/* No support in the driver. */
	sc->dma_ext_desc = false;

	/*
	 * DMA must be stop while changing descriptor list addresses.
	 */

	/* Stop DMA TX */
	reg = READ4(sc, ETH_DMACTXCR(0));
	reg &= ~DMACTXCR_ST;
	WRITE4(sc, ETH_DMACTXCR(0), reg);

	/* Stop DMA RX */
	reg = READ4(sc, ETH_DMACRXCR(0));
	reg &= ~DMACRXCR_SR;
	WRITE4(sc, ETH_DMACRXCR(0), reg);

	/*
	 * Set up TX descriptor ring, descriptors, and dma maps.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
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

	error = bus_dmamem_alloc(sc->txdesc_tag, (void **)&sc->txdesc_ring,
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

	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES * TX_MAP_MAX_SEGS,	/* maxsize */
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
		error = bus_dmamap_create(sc->txbuf_tag, 0,
		    &sc->txbuf_map[idx].map);
		if (error != 0) {
			device_printf(sc->dev,
			    "could not create TX buffer DMA map.\n");
			goto out;
		}
	}

	for (idx = 0; idx < TX_DESC_COUNT; idx++)
		txdesc_clear(sc, idx);

	WRITE4(sc, ETH_DMACTXDLAR_HI(0),
	    (uint32_t)((uint64_t)sc->txdesc_ring_paddr >> 32));
	WRITE4(sc, ETH_DMACTXDLAR(0), (uint32_t)sc->txdesc_ring_paddr);
	WRITE4(sc, ETH_DMACTXRLR(0), TX_DESC_COUNT - 1);

	/*
	 * Set up RX descriptor ring, descriptors, dma maps, and mbufs.
	 */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->dev),	/* Parent tag. */
	    DWC_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
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
	    BUS_SPACE_MAXADDR,		/* lowaddr */
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

	for (idx = 0; idx < RX_MAP_COUNT; idx++) {
		error = bus_dmamap_create(sc->rxbuf_tag, 0,
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
		if ((error = dma4_setup_rxbuf(sc, idx, m)) != 0) {
			device_printf(sc->dev,
			    "could not create new RX buffer.\n");
			goto out;
		}
	}

	WRITE4(sc, ETH_DMACRXDLAR_HI(0),
	    (uint32_t)((uint64_t)sc->rxdesc_ring_paddr >> 32));
	WRITE4(sc, ETH_DMACRXDLAR(0), (uint32_t)sc->rxdesc_ring_paddr);

	WRITE4(sc, ETH_DMACRXDTPR(0), sc->rxdesc_ring_paddr +
	    (RX_DESC_COUNT - 1) * sizeof(struct dwc_hwdesc));
	WRITE4(sc, ETH_DMACRXRLR(0), RX_DESC_COUNT - 1);

out:
	if (error != 0)
		return (ENXIO);

	return (0);
}

/*
 * Free the bus_dma resources
 */
void
dma4_free(struct dwc_softc *sc)
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
dma4_intr(struct dwc_softc *sc)
{
	uint32_t reg;
	int rv;

	rv = 0;

	DWC_ASSERT_LOCKED(sc);

	reg = READ4(sc, ETH_DMACSR(0));

	if (reg & DMACSR_NIS) {
		if (reg & DMACSR_RI)
			dma4_rxfinish_locked(sc);

		if (reg & DMACSR_TI) {
			dma4_txfinish_locked(sc);
			dma4_txstart(sc);
		}
	}

	if (reg & DMACSR_AIS) {
		if (reg & DMACSR_FBE) {
			/* Fatal Bus Error. */
			rv = EIO;
		}
	}

	WRITE4(sc, ETH_DMACSR(0), reg);

	return (rv);
}
