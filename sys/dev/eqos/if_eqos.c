/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Soren Schmidt <sos@deepcore.dk>
 * Copyright (c) 2022 Jared McNeill <jmcneill@invisible.ca>
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $Id: eqos.c 1059 2022-12-08 19:32:32Z sos $
 */

/*
 * DesignWare Ethernet Quality-of-Service controller
 */

#include "opt_platform.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/endian.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/systm.h>
#include <machine/bus.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/bpf.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include "miibus_if.h"
#include "if_eqos_if.h"

#ifdef FDT
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/clk/clk.h>
#endif

#include <dev/eqos/if_eqos_reg.h>
#include <dev/eqos/if_eqos_var.h>

#define	DESC_BOUNDARY		(1ULL << 32)
#define	DESC_ALIGN		sizeof(struct eqos_dma_desc)
#define	DESC_OFFSET(n)		((n) * sizeof(struct eqos_dma_desc))

#define	TX_DESC_COUNT		EQOS_DMA_DESC_COUNT
#define	TX_DESC_SIZE		(TX_DESC_COUNT * DESC_ALIGN)
#define	TX_MAX_SEGS		(TX_DESC_COUNT / 2)
#define	TX_NEXT(n)		(((n) + 1 ) % TX_DESC_COUNT)
#define	TX_QUEUED(h, t)		((((h) - (t)) + TX_DESC_COUNT) % TX_DESC_COUNT)

#define	RX_DESC_COUNT		EQOS_DMA_DESC_COUNT
#define	RX_DESC_SIZE		(RX_DESC_COUNT * DESC_ALIGN)
#define	RX_NEXT(n)		(((n) + 1) % RX_DESC_COUNT)

#define	MII_BUSY_RETRY		1000
#define	WATCHDOG_TIMEOUT_SECS	3

#define	EQOS_LOCK(sc)		mtx_lock(&(sc)->lock)
#define	EQOS_UNLOCK(sc)		mtx_unlock(&(sc)->lock)
#define	EQOS_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->lock, MA_OWNED)

#define	RD4(sc, o)		bus_read_4(sc->res[EQOS_RES_MEM], (o))
#define	WR4(sc, o, v)		bus_write_4(sc->res[EQOS_RES_MEM], (o), (v))


static struct resource_spec eqos_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static void eqos_tick(void *softc);


static int
eqos_miibus_readreg(device_t dev, int phy, int reg)
{
	struct eqos_softc *sc = device_get_softc(dev);
	uint32_t addr;
	int retry, val;

	addr = sc->csr_clock_range |
	    (phy << GMAC_MAC_MDIO_ADDRESS_PA_SHIFT) |
	    (reg << GMAC_MAC_MDIO_ADDRESS_RDA_SHIFT) |
	    GMAC_MAC_MDIO_ADDRESS_GOC_READ | GMAC_MAC_MDIO_ADDRESS_GB;
	WR4(sc, GMAC_MAC_MDIO_ADDRESS, addr);

	DELAY(100);

	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		addr = RD4(sc, GMAC_MAC_MDIO_ADDRESS);
		if (!(addr & GMAC_MAC_MDIO_ADDRESS_GB)) {
			val = RD4(sc, GMAC_MAC_MDIO_DATA) & 0xFFFF;
			break;
		}
		DELAY(10);
	}
	if (!retry) {
		device_printf(dev, "phy read timeout, phy=%d reg=%d\n",
		    phy, reg);
		return (ETIMEDOUT);
	}
	return (val);
}

static int
eqos_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct eqos_softc *sc = device_get_softc(dev);
	uint32_t addr;
	int retry;

	WR4(sc, GMAC_MAC_MDIO_DATA, val);

	addr = sc->csr_clock_range |
	    (phy << GMAC_MAC_MDIO_ADDRESS_PA_SHIFT) |
	    (reg << GMAC_MAC_MDIO_ADDRESS_RDA_SHIFT) |
	    GMAC_MAC_MDIO_ADDRESS_GOC_WRITE | GMAC_MAC_MDIO_ADDRESS_GB;
	WR4(sc, GMAC_MAC_MDIO_ADDRESS, addr);

	DELAY(100);

	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		addr = RD4(sc, GMAC_MAC_MDIO_ADDRESS);
		if (!(addr & GMAC_MAC_MDIO_ADDRESS_GB))
			break;
		DELAY(10);
	}
	if (!retry) {
		device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		    phy, reg);
		return (ETIMEDOUT);
	}
	return (0);
}

static void
eqos_miibus_statchg(device_t dev)
{
	struct eqos_softc *sc = device_get_softc(dev);
	struct mii_data *mii = device_get_softc(sc->miibus);
	uint32_t reg;

	EQOS_ASSERT_LOCKED(sc);

	if (mii->mii_media_status & IFM_ACTIVE)
		sc->link_up = true;
	else
		sc->link_up = false;

	reg = RD4(sc, GMAC_MAC_CONFIGURATION);

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_10_T:
		reg |= GMAC_MAC_CONFIGURATION_PS;
		reg &= ~GMAC_MAC_CONFIGURATION_FES;
		break;
	case IFM_100_TX:
		reg |= GMAC_MAC_CONFIGURATION_PS;
		reg |= GMAC_MAC_CONFIGURATION_FES;
		break;
	case IFM_1000_T:
        case IFM_1000_SX:
		reg &= ~GMAC_MAC_CONFIGURATION_PS;
		reg &= ~GMAC_MAC_CONFIGURATION_FES;
		break;
	case IFM_2500_T:
	case IFM_2500_SX:
		reg &= ~GMAC_MAC_CONFIGURATION_PS;
		reg |= GMAC_MAC_CONFIGURATION_FES;
		break;
	default:
		sc->link_up = false;
		return;
	}

	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX))
		reg |= GMAC_MAC_CONFIGURATION_DM;
	else
		reg &= ~GMAC_MAC_CONFIGURATION_DM;

	WR4(sc, GMAC_MAC_CONFIGURATION, reg);

	IF_EQOS_SET_SPEED(dev, IFM_SUBTYPE(mii->mii_media_active));

	WR4(sc, GMAC_MAC_1US_TIC_COUNTER, (sc->csr_clock / 1000000) - 1);
}

static void
eqos_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct eqos_softc *sc = if_getsoftc(ifp);
	struct mii_data *mii = device_get_softc(sc->miibus);

	EQOS_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	EQOS_UNLOCK(sc);
}

static int
eqos_media_change(if_t ifp)
{
	struct eqos_softc *sc = if_getsoftc(ifp);
	int error;

	EQOS_LOCK(sc);
	error = mii_mediachg(device_get_softc(sc->miibus)); 
	EQOS_UNLOCK(sc);
	return (error);
}

static void
eqos_setup_txdesc(struct eqos_softc *sc, int index, int flags,
    bus_addr_t paddr, u_int len, u_int total_len)
{
	uint32_t tdes2, tdes3;

	if (!paddr || !len) {
		tdes2 = 0;
		tdes3 = flags;
	} else {
		tdes2 = (flags & EQOS_TDES3_LD) ? EQOS_TDES2_IOC : 0;
		tdes3 = flags;
	}
	bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map, BUS_DMASYNC_PREWRITE);
	sc->tx.desc_ring[index].des0 = htole32((uint32_t)paddr);
	sc->tx.desc_ring[index].des1 = htole32((uint32_t)(paddr >> 32));
	sc->tx.desc_ring[index].des2 = htole32(tdes2 | len);
	sc->tx.desc_ring[index].des3 = htole32(tdes3 | total_len);
}

static int
eqos_setup_txbuf(struct eqos_softc *sc, struct mbuf *m)
{
	bus_dma_segment_t segs[TX_MAX_SEGS];
	int first = sc->tx.head;
	int error, nsegs, idx;
	uint32_t flags;

	error = bus_dmamap_load_mbuf_sg(sc->tx.buf_tag,
	    sc->tx.buf_map[first].map, m, segs, &nsegs, 0);
	if (error == EFBIG) {
		struct mbuf *mb;

		device_printf(sc->dev, "TX packet too big trying defrag\n");
		bus_dmamap_unload(sc->tx.buf_tag, sc->tx.buf_map[first].map);
		if (!(mb = m_defrag(m, M_NOWAIT)))
			return (ENOMEM);
		m = mb;
		error = bus_dmamap_load_mbuf_sg(sc->tx.buf_tag,
		    sc->tx.buf_map[first].map, m, segs, &nsegs, 0);
	}
	if (error)
		return (ENOMEM);

	if (TX_QUEUED(sc->tx.head, sc->tx.tail) + nsegs > TX_DESC_COUNT) {
		bus_dmamap_unload(sc->tx.buf_tag, sc->tx.buf_map[first].map);
		device_printf(sc->dev, "TX packet no more queue space\n");
		return (ENOMEM);
	}

	bus_dmamap_sync(sc->tx.buf_tag, sc->tx.buf_map[first].map,
	    BUS_DMASYNC_PREWRITE);

	sc->tx.buf_map[first].mbuf = m;

	for (flags = EQOS_TDES3_FD, idx = 0; idx < nsegs; idx++) {
		if (idx == (nsegs - 1))
			flags |= EQOS_TDES3_LD;
		eqos_setup_txdesc(sc, sc->tx.head, flags, segs[idx].ds_addr,
		    segs[idx].ds_len, m->m_pkthdr.len);
		flags &= ~EQOS_TDES3_FD;
		flags |= EQOS_TDES3_OWN;
		sc->tx.head = TX_NEXT(sc->tx.head);
	}

	/*
	 * Defer setting OWN bit on the first descriptor
	 * until all descriptors have been updated
	 */
	bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map, BUS_DMASYNC_PREWRITE);
	sc->tx.desc_ring[first].des3 |= htole32(EQOS_TDES3_OWN);

	return (0);
}

static void
eqos_setup_rxdesc(struct eqos_softc *sc, int index, bus_addr_t paddr)
{

	sc->rx.desc_ring[index].des0 = htole32((uint32_t)paddr);
	sc->rx.desc_ring[index].des1 = htole32((uint32_t)(paddr >> 32));
	sc->rx.desc_ring[index].des2 = htole32(0);
	bus_dmamap_sync(sc->rx.desc_tag, sc->rx.desc_map, BUS_DMASYNC_PREWRITE);
	sc->rx.desc_ring[index].des3 = htole32(EQOS_RDES3_OWN | EQOS_RDES3_IOC |
	    EQOS_RDES3_BUF1V);
}

static int
eqos_setup_rxbuf(struct eqos_softc *sc, int index, struct mbuf *m)
{
	struct bus_dma_segment seg;
	int error, nsegs;

	m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->rx.buf_tag,
	    sc->rx.buf_map[index].map, m, &seg, &nsegs, 0);
	if (error)
		return (error);

	bus_dmamap_sync(sc->rx.buf_tag, sc->rx.buf_map[index].map,
	    BUS_DMASYNC_PREREAD);

	sc->rx.buf_map[index].mbuf = m;
	eqos_setup_rxdesc(sc, index, seg.ds_addr);

	return (0);
}

static struct mbuf *
eqos_alloc_mbufcl(struct eqos_softc *sc)
{
	struct mbuf *m;

	if ((m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR)))
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;
	return (m);
}

static void
eqos_enable_intr(struct eqos_softc *sc)
{

	WR4(sc, GMAC_DMA_CHAN0_INTR_ENABLE,
	    GMAC_DMA_CHAN0_INTR_ENABLE_NIE | GMAC_DMA_CHAN0_INTR_ENABLE_AIE |
	    GMAC_DMA_CHAN0_INTR_ENABLE_FBE | GMAC_DMA_CHAN0_INTR_ENABLE_RIE |
	    GMAC_DMA_CHAN0_INTR_ENABLE_TIE);
}

static void
eqos_disable_intr(struct eqos_softc *sc)
{

	WR4(sc, GMAC_DMA_CHAN0_INTR_ENABLE, 0);
}

static uint32_t
eqos_bitrev32(uint32_t x)
{

	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));
	return ((x >> 16) | (x << 16));
}

static u_int
eqos_hash_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	uint32_t crc, *hash = arg;

	crc = ether_crc32_le(LLADDR(sdl), ETHER_ADDR_LEN);
	crc &= 0x7f;
	crc = eqos_bitrev32(~crc) >> 26;
	hash[crc >> 5] |= 1 << (crc & 0x1f);
	return (1);
}

static void
eqos_setup_rxfilter(struct eqos_softc *sc)
{
	if_t ifp = sc->ifp;
	uint32_t pfil, hash[2];
	const uint8_t *eaddr;
	uint32_t val;

	EQOS_ASSERT_LOCKED(sc);

	pfil = RD4(sc, GMAC_MAC_PACKET_FILTER);
	pfil &= ~(GMAC_MAC_PACKET_FILTER_PR |
	    GMAC_MAC_PACKET_FILTER_PM |
	    GMAC_MAC_PACKET_FILTER_HMC |
	    GMAC_MAC_PACKET_FILTER_PCF_MASK);
	hash[0] = hash[1] = 0xffffffff;

	if ((if_getflags(ifp) & IFF_PROMISC)) {
		pfil |= GMAC_MAC_PACKET_FILTER_PR |
		    GMAC_MAC_PACKET_FILTER_PCF_ALL;
	} else if ((if_getflags(ifp) & IFF_ALLMULTI)) {
		pfil |= GMAC_MAC_PACKET_FILTER_PM;
	} else {
		hash[0] = hash[1] = 0;
		pfil |= GMAC_MAC_PACKET_FILTER_HMC;
		if_foreach_llmaddr(ifp, eqos_hash_maddr, hash);
	}

	/* Write our unicast address */
	eaddr = if_getlladdr(ifp);
	val = eaddr[4] | (eaddr[5] << 8);
	WR4(sc, GMAC_MAC_ADDRESS0_HIGH, val);
	val = eaddr[0] | (eaddr[1] << 8) | (eaddr[2] << 16) |
	    (eaddr[3] << 24);
	WR4(sc, GMAC_MAC_ADDRESS0_LOW, val);

	/* Multicast hash filters */
	WR4(sc, GMAC_MAC_HASH_TABLE_REG0, hash[0]);
	WR4(sc, GMAC_MAC_HASH_TABLE_REG1, hash[1]);

	/* Packet filter config */
	WR4(sc, GMAC_MAC_PACKET_FILTER, pfil);
}

static int
eqos_reset(struct eqos_softc *sc)
{
	uint32_t val;
	int retry;

	WR4(sc, GMAC_DMA_MODE, GMAC_DMA_MODE_SWR);
	for (retry = 2000; retry > 0; retry--) {
		DELAY(1000);
		val = RD4(sc, GMAC_DMA_MODE);
		if (!(val & GMAC_DMA_MODE_SWR))
			return (0);
	}
	return (ETIMEDOUT);
}

static void
eqos_init_rings(struct eqos_softc *sc)
{

	WR4(sc, GMAC_DMA_CHAN0_TX_BASE_ADDR_HI,
	    (uint32_t)(sc->tx.desc_ring_paddr >> 32));
	WR4(sc, GMAC_DMA_CHAN0_TX_BASE_ADDR,
	    (uint32_t)sc->tx.desc_ring_paddr);
	WR4(sc, GMAC_DMA_CHAN0_TX_RING_LEN, TX_DESC_COUNT - 1);

	WR4(sc, GMAC_DMA_CHAN0_RX_BASE_ADDR_HI,
	    (uint32_t)(sc->rx.desc_ring_paddr >> 32));
	WR4(sc, GMAC_DMA_CHAN0_RX_BASE_ADDR,
	    (uint32_t)sc->rx.desc_ring_paddr);
	WR4(sc, GMAC_DMA_CHAN0_RX_RING_LEN, RX_DESC_COUNT - 1);

	WR4(sc, GMAC_DMA_CHAN0_RX_END_ADDR,
	    (uint32_t)sc->rx.desc_ring_paddr + DESC_OFFSET(RX_DESC_COUNT));
}

static void
eqos_init(void *if_softc)
{
	struct eqos_softc *sc = if_softc;
	if_t ifp = sc->ifp;
	struct mii_data *mii = device_get_softc(sc->miibus);
	uint32_t val;

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	EQOS_LOCK(sc);

	eqos_init_rings(sc);

	eqos_setup_rxfilter(sc);

	WR4(sc, GMAC_MAC_1US_TIC_COUNTER, (sc->csr_clock / 1000000) - 1);

	/* Enable transmit and receive DMA */
	val = RD4(sc, GMAC_DMA_CHAN0_CONTROL);
	val &= ~GMAC_DMA_CHAN0_CONTROL_DSL_MASK;
	val |= ((DESC_ALIGN - 16) / 8) << GMAC_DMA_CHAN0_CONTROL_DSL_SHIFT;
	val |= GMAC_DMA_CHAN0_CONTROL_PBLX8;
	WR4(sc, GMAC_DMA_CHAN0_CONTROL, val);
	val = RD4(sc, GMAC_DMA_CHAN0_TX_CONTROL);
	val |= GMAC_DMA_CHAN0_TX_CONTROL_OSP;
	val |= GMAC_DMA_CHAN0_TX_CONTROL_START;
	WR4(sc, GMAC_DMA_CHAN0_TX_CONTROL, val);
	val = RD4(sc, GMAC_DMA_CHAN0_RX_CONTROL);
	val &= ~GMAC_DMA_CHAN0_RX_CONTROL_RBSZ_MASK;
	val |= (MCLBYTES << GMAC_DMA_CHAN0_RX_CONTROL_RBSZ_SHIFT);
	val |= GMAC_DMA_CHAN0_RX_CONTROL_START;
	WR4(sc, GMAC_DMA_CHAN0_RX_CONTROL, val);

	/* Disable counters */
	WR4(sc, GMAC_MMC_CONTROL,
	    GMAC_MMC_CONTROL_CNTFREEZ |
	    GMAC_MMC_CONTROL_CNTPRST |
	    GMAC_MMC_CONTROL_CNTPRSTLVL);

	/* Configure operation modes */
	WR4(sc, GMAC_MTL_TXQ0_OPERATION_MODE,
	    GMAC_MTL_TXQ0_OPERATION_MODE_TSF |
	    GMAC_MTL_TXQ0_OPERATION_MODE_TXQEN_EN);
	WR4(sc, GMAC_MTL_RXQ0_OPERATION_MODE,
	    GMAC_MTL_RXQ0_OPERATION_MODE_RSF |
	    GMAC_MTL_RXQ0_OPERATION_MODE_FEP |
	    GMAC_MTL_RXQ0_OPERATION_MODE_FUP);

	/* Enable flow control */
	val = RD4(sc, GMAC_MAC_Q0_TX_FLOW_CTRL);
	val |= 0xFFFFU << GMAC_MAC_Q0_TX_FLOW_CTRL_PT_SHIFT;
	val |= GMAC_MAC_Q0_TX_FLOW_CTRL_TFE;
	WR4(sc, GMAC_MAC_Q0_TX_FLOW_CTRL, val);
	val = RD4(sc, GMAC_MAC_RX_FLOW_CTRL);
	val |= GMAC_MAC_RX_FLOW_CTRL_RFE;
	WR4(sc, GMAC_MAC_RX_FLOW_CTRL, val);

	/* set RX queue mode. must be in DCB mode. */
	WR4(sc, GMAC_RXQ_CTRL0, (GMAC_RXQ_CTRL0_EN_MASK << 16) |
	    GMAC_RXQ_CTRL0_EN_DCB);

	/* Enable transmitter and receiver */
	val = RD4(sc, GMAC_MAC_CONFIGURATION);
	val |= GMAC_MAC_CONFIGURATION_BE;
	val |= GMAC_MAC_CONFIGURATION_JD;
	val |= GMAC_MAC_CONFIGURATION_JE;
	val |= GMAC_MAC_CONFIGURATION_DCRS;
	val |= GMAC_MAC_CONFIGURATION_TE;
	val |= GMAC_MAC_CONFIGURATION_RE;
	WR4(sc, GMAC_MAC_CONFIGURATION, val);

	eqos_enable_intr(sc);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	mii_mediachg(mii);
	callout_reset(&sc->callout, hz, eqos_tick, sc);

	EQOS_UNLOCK(sc);
}

static void
eqos_start_locked(if_t ifp)
{
	struct eqos_softc *sc = if_getsoftc(ifp);
	struct mbuf *m;
	int pending = 0;

	if (!sc->link_up)
		return;

	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	while (true) {
		if (TX_QUEUED(sc->tx.head, sc->tx.tail) >=
		    TX_DESC_COUNT - TX_MAX_SEGS) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		if (!(m = if_dequeue(ifp)))
			break;

		if (eqos_setup_txbuf(sc, m)) {
			if_sendq_prepend(ifp, m);
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}
		bpf_mtap_if(ifp, m);
		pending++;
	}

	if (pending) {
		bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Start and run TX DMA */
		WR4(sc, GMAC_DMA_CHAN0_TX_END_ADDR,
		    (uint32_t)sc->tx.desc_ring_paddr + DESC_OFFSET(sc->tx.head));
		sc->tx_watchdog = WATCHDOG_TIMEOUT_SECS;
	}
}

static void
eqos_start(if_t ifp)
{
	struct eqos_softc *sc = if_getsoftc(ifp);

	EQOS_LOCK(sc);
	eqos_start_locked(ifp);
	EQOS_UNLOCK(sc);
}

static void
eqos_stop(struct eqos_softc *sc)
{
	if_t ifp = sc->ifp;
	uint32_t val;
	int retry;

	EQOS_LOCK(sc);

	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->callout);

	/* Disable receiver */
	val = RD4(sc, GMAC_MAC_CONFIGURATION);
	val &= ~GMAC_MAC_CONFIGURATION_RE;
	WR4(sc, GMAC_MAC_CONFIGURATION, val);

	/* Stop receive DMA */
	val = RD4(sc, GMAC_DMA_CHAN0_RX_CONTROL);
	val &= ~GMAC_DMA_CHAN0_RX_CONTROL_START;
	WR4(sc, GMAC_DMA_CHAN0_RX_CONTROL, val);

	/* Stop transmit DMA */
	val = RD4(sc, GMAC_DMA_CHAN0_TX_CONTROL);
	val &= ~GMAC_DMA_CHAN0_TX_CONTROL_START;
	WR4(sc, GMAC_DMA_CHAN0_TX_CONTROL, val);

	/* Flush data in the TX FIFO */
	val = RD4(sc, GMAC_MTL_TXQ0_OPERATION_MODE);
	val |= GMAC_MTL_TXQ0_OPERATION_MODE_FTQ;
	WR4(sc, GMAC_MTL_TXQ0_OPERATION_MODE, val);
	for (retry = 10000; retry > 0; retry--) {
		val = RD4(sc, GMAC_MTL_TXQ0_OPERATION_MODE);
		if (!(val & GMAC_MTL_TXQ0_OPERATION_MODE_FTQ))
			break;
		DELAY(10);
	}
	if (!retry)
		device_printf(sc->dev, "timeout flushing TX queue\n");

	/* Disable transmitter */
	val = RD4(sc, GMAC_MAC_CONFIGURATION);
	val &= ~GMAC_MAC_CONFIGURATION_TE;
	WR4(sc, GMAC_MAC_CONFIGURATION, val);

	eqos_disable_intr(sc);

	EQOS_UNLOCK(sc);
}

static void
eqos_rxintr(struct eqos_softc *sc)
{
	if_t ifp = sc->ifp;
	struct mbuf *m;
	uint32_t rdes3;
	int error, length;

	while (true) {
		rdes3 = le32toh(sc->rx.desc_ring[sc->rx.head].des3);
		if ((rdes3 & EQOS_RDES3_OWN))
			break;

		if (rdes3 & (EQOS_RDES3_OE | EQOS_RDES3_RE))
			printf("Receive error rdes3=%08x\n", rdes3);

		bus_dmamap_sync(sc->rx.buf_tag,
		    sc->rx.buf_map[sc->rx.head].map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rx.buf_tag,
		    sc->rx.buf_map[sc->rx.head].map);

		length = rdes3 & EQOS_RDES3_LENGTH_MASK;
		if (length) {
			m = sc->rx.buf_map[sc->rx.head].mbuf;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = length;
			m->m_len = length;
			m->m_nextpkt = NULL;

			/* Remove trailing FCS */
			m_adj(m, -ETHER_CRC_LEN);

			EQOS_UNLOCK(sc);
			if_input(ifp, m);
			EQOS_LOCK(sc);
		}

		if ((m = eqos_alloc_mbufcl(sc))) {
			if ((error = eqos_setup_rxbuf(sc, sc->rx.head, m)))
				printf("ERROR: Hole in RX ring!!\n");
		}
		else
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);

		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

		WR4(sc, GMAC_DMA_CHAN0_RX_END_ADDR,
		    (uint32_t)sc->rx.desc_ring_paddr + DESC_OFFSET(sc->rx.head));

		sc->rx.head = RX_NEXT(sc->rx.head);
	}
}

static void
eqos_txintr(struct eqos_softc *sc)
{
	if_t ifp = sc->ifp;
	struct eqos_bufmap *bmap;
	uint32_t tdes3;

	EQOS_ASSERT_LOCKED(sc);

	while (sc->tx.tail != sc->tx.head) {
		tdes3 = le32toh(sc->tx.desc_ring[sc->tx.tail].des3);
		if ((tdes3 & EQOS_TDES3_OWN))
			break;

		bmap = &sc->tx.buf_map[sc->tx.tail];
		if (bmap->mbuf) {
			bus_dmamap_sync(sc->tx.buf_tag, bmap->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->tx.buf_tag, bmap->map);
			m_freem(bmap->mbuf);
			bmap->mbuf = NULL;
		}

		eqos_setup_txdesc(sc, sc->tx.tail, 0, 0, 0, 0);

		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);

		/* Last descriptor in a packet contains DMA status */
		if ((tdes3 & EQOS_TDES3_LD)) {
			if ((tdes3 & EQOS_TDES3_DE)) {
				if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			} else if ((tdes3 & EQOS_TDES3_ES)) {
				if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			} else {
				if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
			}
		}
		sc->tx.tail = TX_NEXT(sc->tx.tail);
	}
	if (sc->tx.tail == sc->tx.head)
		sc->tx_watchdog = 0;
	eqos_start_locked(sc->ifp);
}

static void
eqos_intr_mtl(struct eqos_softc *sc, uint32_t mtl_status)
{
	uint32_t mtl_istat = 0;

	if ((mtl_status & GMAC_MTL_INTERRUPT_STATUS_Q0IS)) {
		uint32_t mtl_clear = 0;

		mtl_istat = RD4(sc, GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS);
		if ((mtl_istat & GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_RXOVFIS)) {
			mtl_clear |= GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_RXOVFIS;
		}
		if ((mtl_istat & GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_TXUNFIS)) {
			mtl_clear |= GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_TXUNFIS;
		}
		if (mtl_clear) {
			mtl_clear |= (mtl_istat &
			    (GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_RXOIE |
			    GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS_TXUIE));
			WR4(sc, GMAC_MTL_Q0_INTERRUPT_CTRL_STATUS, mtl_clear);
		}
	}
	if (bootverbose)
		device_printf(sc->dev,
		    "GMAC_MTL_INTERRUPT_STATUS = 0x%08X, "
		    "GMAC_MTL_INTERRUPT_STATUS_Q0IS = 0x%08X\n",
		    mtl_status, mtl_istat);
}

static void
eqos_tick(void *softc)
{
	struct eqos_softc *sc = softc;
	struct mii_data *mii = device_get_softc(sc->miibus);
	bool link_status;

	EQOS_ASSERT_LOCKED(sc);

	if (sc->tx_watchdog > 0)
		if (!--sc->tx_watchdog) {
			device_printf(sc->dev, "watchdog timeout\n");
			eqos_txintr(sc);
		}

	link_status = sc->link_up;
	mii_tick(mii);
	if (sc->link_up && !link_status)
		eqos_start_locked(sc->ifp);

	callout_reset(&sc->callout, hz, eqos_tick, sc);
}

static void
eqos_intr(void *arg)
{
	struct eqos_softc *sc = arg;
	uint32_t mac_status, mtl_status, dma_status, rx_tx_status;

	mac_status = RD4(sc, GMAC_MAC_INTERRUPT_STATUS);
	mac_status &= RD4(sc, GMAC_MAC_INTERRUPT_ENABLE);

	if (mac_status)
		device_printf(sc->dev, "MAC interrupt\n");

	if ((mtl_status = RD4(sc, GMAC_MTL_INTERRUPT_STATUS)))
		eqos_intr_mtl(sc, mtl_status);

	dma_status = RD4(sc, GMAC_DMA_CHAN0_STATUS);
	dma_status &= RD4(sc, GMAC_DMA_CHAN0_INTR_ENABLE);

	if (dma_status)
		WR4(sc, GMAC_DMA_CHAN0_STATUS, dma_status);

	EQOS_LOCK(sc);

	if (dma_status & GMAC_DMA_CHAN0_STATUS_RI)
		eqos_rxintr(sc);

	if (dma_status & GMAC_DMA_CHAN0_STATUS_TI)
		eqos_txintr(sc);

	EQOS_UNLOCK(sc);

	if (!(mac_status | mtl_status | dma_status)) {
		device_printf(sc->dev,
		    "spurious interrupt mac=%08x mtl=%08x dma=%08x\n",
		    RD4(sc, GMAC_MAC_INTERRUPT_STATUS),
		    RD4(sc, GMAC_MTL_INTERRUPT_STATUS),
		    RD4(sc, GMAC_DMA_CHAN0_STATUS));
	}
	if ((rx_tx_status = RD4(sc, GMAC_MAC_RX_TX_STATUS)))
		device_printf(sc->dev, "RX/TX status interrupt\n");
}

static int
eqos_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct eqos_softc *sc = if_getsoftc(ifp);
	struct ifreq *ifr = (struct ifreq *)data;
	struct mii_data *mii;
	int flags, mask;
	int error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				flags = if_getflags(ifp);
				if ((flags & (IFF_PROMISC|IFF_ALLMULTI))) {
					EQOS_LOCK(sc);
					eqos_setup_rxfilter(sc);
					EQOS_UNLOCK(sc);
				}
			}
			else {
				eqos_init(sc);
			}
		}
		else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				eqos_stop(sc);
		}
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			EQOS_LOCK(sc);
			eqos_setup_rxfilter(sc);
			EQOS_UNLOCK(sc);
		}
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;

	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
		if (mask & IFCAP_VLAN_MTU)
			if_togglecapenable(ifp, IFCAP_VLAN_MTU);
		if (mask & IFCAP_RXCSUM)
			if_togglecapenable(ifp, IFCAP_RXCSUM);
		if (mask & IFCAP_TXCSUM)
			if_togglecapenable(ifp, IFCAP_TXCSUM);
		if ((if_getcapenable(ifp) & IFCAP_TXCSUM))
			if_sethwassistbits(ifp,
			    CSUM_IP | CSUM_UDP | CSUM_TCP, 0);
		else
			if_sethwassistbits(ifp,
			    0, CSUM_IP | CSUM_UDP | CSUM_TCP);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static void
eqos_get_eaddr(struct eqos_softc *sc, uint8_t *eaddr)
{
	uint32_t maclo, machi;

	maclo = htobe32(RD4(sc, GMAC_MAC_ADDRESS0_LOW));
	machi = htobe16(RD4(sc, GMAC_MAC_ADDRESS0_HIGH) & 0xFFFF);

	/* if no valid MAC address generate random */
	if (maclo == 0xffffffff && machi == 0xffff) {
		maclo = 0xf2 | (arc4random() & 0xffff0000);
		machi = arc4random() & 0x0000ffff;
	}
	eaddr[0] = maclo & 0xff;
	eaddr[1] = (maclo >> 8) & 0xff;
	eaddr[2] = (maclo >> 16) & 0xff;
	eaddr[3] = (maclo >> 24) & 0xff;
	eaddr[4] = machi & 0xff;
	eaddr[5] = (machi >> 8) & 0xff;
}

static void
eqos_axi_configure(struct eqos_softc *sc)
{
	uint32_t val;

	val = RD4(sc, GMAC_DMA_SYSBUS_MODE);

	/* Max Write Outstanding Req Limit */
	val &= ~GMAC_DMA_SYSBUS_MODE_WR_OSR_LMT_MASK;
	val |= 0x03 << GMAC_DMA_SYSBUS_MODE_WR_OSR_LMT_SHIFT;

	/* Max Read Outstanding Req Limit */
	val &= ~GMAC_DMA_SYSBUS_MODE_RD_OSR_LMT_MASK;
	val |= 0x07 << GMAC_DMA_SYSBUS_MODE_RD_OSR_LMT_SHIFT;

	/* Allowed Burst Length's */
	val |= GMAC_DMA_SYSBUS_MODE_BLEN16;
	val |= GMAC_DMA_SYSBUS_MODE_BLEN8;
	val |= GMAC_DMA_SYSBUS_MODE_BLEN4;

	/* Fixed Burst Length */
	val |= GMAC_DMA_SYSBUS_MODE_MB;

	WR4(sc, GMAC_DMA_SYSBUS_MODE, val);
}

static void
eqos_get1paddr(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{

	if (!error)
		*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
eqos_setup_dma(struct eqos_softc *sc)
{
	struct mbuf *m;
	int error, i;

	/* Set up TX descriptor ring, descriptors, and dma maps */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev),
					DESC_ALIGN, DESC_BOUNDARY,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					TX_DESC_SIZE, 1, TX_DESC_SIZE, 0,
					NULL, NULL, &sc->tx.desc_tag))) {
		device_printf(sc->dev, "could not create TX ring DMA tag\n");
		return (error);
	}

	if ((error = bus_dmamem_alloc(sc->tx.desc_tag,
	    (void**)&sc->tx.desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->tx.desc_map))) {
		device_printf(sc->dev,
		    "could not allocate TX descriptor ring.\n");
		return (error);
	}

	if ((error = bus_dmamap_load(sc->tx.desc_tag, sc->tx.desc_map,
	    sc->tx.desc_ring,
	    TX_DESC_SIZE, eqos_get1paddr, &sc->tx.desc_ring_paddr, 0))) {
		device_printf(sc->dev,
		    "could not load TX descriptor ring map.\n");
		return (error);
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					MCLBYTES*TX_MAX_SEGS, TX_MAX_SEGS,
					MCLBYTES, 0, NULL, NULL,
					&sc->tx.buf_tag))) {
		device_printf(sc->dev, "could not create TX buffer DMA tag.\n");
		return (error);
	}

	for (i = 0; i < TX_DESC_COUNT; i++) {
		if ((error = bus_dmamap_create(sc->tx.buf_tag, BUS_DMA_COHERENT,
		    &sc->tx.buf_map[i].map))) {
			device_printf(sc->dev, "cannot create TX buffer map\n");
			return (error);
		}
		eqos_setup_txdesc(sc, i, EQOS_TDES3_OWN, 0, 0, 0);
	}

	/* Set up RX descriptor ring, descriptors, dma maps, and mbufs */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev),
					DESC_ALIGN, DESC_BOUNDARY,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					RX_DESC_SIZE, 1, RX_DESC_SIZE, 0,
					NULL, NULL, &sc->rx.desc_tag))) {
		device_printf(sc->dev, "could not create RX ring DMA tag.\n");
		return (error);
	}

	if ((error = bus_dmamem_alloc(sc->rx.desc_tag,
	    (void **)&sc->rx.desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO,
	    &sc->rx.desc_map))) {
		device_printf(sc->dev,
		    "could not allocate RX descriptor ring.\n");
		return (error);
	}

	if ((error = bus_dmamap_load(sc->rx.desc_tag, sc->rx.desc_map,
	    sc->rx.desc_ring, RX_DESC_SIZE, eqos_get1paddr,
	    &sc->rx.desc_ring_paddr, 0))) {
		device_printf(sc->dev,
		    "could not load RX descriptor ring map.\n");
		return (error);
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(sc->dev), 1, 0,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					MCLBYTES, 1,
					MCLBYTES, 0, NULL, NULL,
					&sc->rx.buf_tag))) {
		device_printf(sc->dev, "could not create RX buf DMA tag.\n");
		return (error);
	}

	for (i = 0; i < RX_DESC_COUNT; i++) {
		if ((error = bus_dmamap_create(sc->rx.buf_tag, BUS_DMA_COHERENT,
		    &sc->rx.buf_map[i].map))) {
			device_printf(sc->dev, "cannot create RX buffer map\n");
			return (error);
		}
		if (!(m = eqos_alloc_mbufcl(sc))) {
			device_printf(sc->dev, "cannot allocate RX mbuf\n");
			return (ENOMEM);
		}
		if ((error = eqos_setup_rxbuf(sc, i, m))) {
			device_printf(sc->dev, "cannot create RX buffer\n");
			return (error);
		}
	}

	if (bootverbose)
		device_printf(sc->dev, "TX ring @ 0x%lx, RX ring @ 0x%lx\n",
		    sc->tx.desc_ring_paddr, sc->rx.desc_ring_paddr);
	return (0);
}

static int
eqos_attach(device_t dev)
{
	struct eqos_softc *sc = device_get_softc(dev);
	if_t ifp;
	uint32_t ver;
	uint8_t eaddr[ETHER_ADDR_LEN];
	u_int userver, snpsver;
	int error;
	int n;

	/* setup resources */
	if (bus_alloc_resources(dev, eqos_spec, sc->res)) {
		device_printf(dev, "Could not allocate resources\n");
		bus_release_resources(dev, eqos_spec, sc->res);
		return (ENXIO);
	}

	if ((error = IF_EQOS_INIT(dev)))
		return (error);

	sc->dev = dev;
	ver  = RD4(sc, GMAC_MAC_VERSION);
	userver = (ver & GMAC_MAC_VERSION_USERVER_MASK) >>
	    GMAC_MAC_VERSION_USERVER_SHIFT;
	snpsver = ver & GMAC_MAC_VERSION_SNPSVER_MASK;

	if (snpsver != 0x51) {
		device_printf(dev, "EQOS version 0x%02x not supported\n",
		    snpsver);
		return (ENXIO);
	}

	for (n = 0; n < 4; n++)
		sc->hw_feature[n] = RD4(sc, GMAC_MAC_HW_FEATURE(n));

	if (bootverbose) {
		device_printf(dev, "DesignWare EQOS ver 0x%02x (0x%02x)\n",
		    snpsver, userver);
		device_printf(dev, "hw features %08x %08x %08x %08x\n",
		    sc->hw_feature[0], sc->hw_feature[1],
		    sc->hw_feature[2], sc->hw_feature[3]);
	}

	mtx_init(&sc->lock, "eqos lock", MTX_NETWORK_LOCK, MTX_DEF);
	callout_init_mtx(&sc->callout, &sc->lock, 0);

	eqos_get_eaddr(sc, eaddr);
	if (bootverbose)
		device_printf(sc->dev, "Ethernet address %6D\n", eaddr, ":");

	/* Soft reset EMAC core */
	if ((error = eqos_reset(sc))) {
		device_printf(sc->dev, "reset timeout!\n");
		return (error);
	}

	/* Configure AXI Bus mode parameters */
	eqos_axi_configure(sc);

	/* Setup DMA descriptors */
	if (eqos_setup_dma(sc)) {
		device_printf(sc->dev, "failed to setup DMA descriptors\n");
		return (EINVAL);
	}

	/* setup interrupt delivery */
	if ((bus_setup_intr(dev, sc->res[EQOS_RES_IRQ0], EQOS_INTR_FLAGS,
	    NULL, eqos_intr, sc, &sc->irq_handle))) {
		device_printf(dev, "unable to setup 1st interrupt\n");
		bus_release_resources(dev, eqos_spec, sc->res);
		return (ENXIO);
	}

	/* Setup ethernet interface */
	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if_setsoftc(ifp, sc);
	if_initname(ifp, device_get_name(sc->dev), device_get_unit(sc->dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(ifp, eqos_start);
	if_setioctlfn(ifp, eqos_ioctl);
	if_setinitfn(ifp, eqos_init);
	if_setsendqlen(ifp, TX_DESC_COUNT - 1);
	if_setsendqready(ifp);
	if_setcapabilities(ifp, IFCAP_VLAN_MTU /*| IFCAP_HWCSUM*/);
	if_setcapenable(ifp, if_getcapabilities(ifp));

	/* Attach MII driver */
	if ((error = mii_attach(sc->dev, &sc->miibus, ifp, eqos_media_change,
	    eqos_media_status, BMSR_DEFCAPMASK, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0))) {
		device_printf(sc->dev, "PHY attach failed\n");
		return (ENXIO);
	}

	/* Attach ethernet interface */
	ether_ifattach(ifp, eaddr);

	return (0);
}

static int
eqos_detach(device_t dev)
{
	struct eqos_softc *sc = device_get_softc(dev);
	int i;

	if (device_is_attached(dev)) {
		EQOS_LOCK(sc);
		eqos_stop(sc);
		EQOS_UNLOCK(sc);
		if_setflagbits(sc->ifp, 0, IFF_UP);
		ether_ifdetach(sc->ifp);
	}

	if (sc->miibus)
		device_delete_child(dev, sc->miibus);
	bus_generic_detach(dev);

	if (sc->irq_handle)
		bus_teardown_intr(dev, sc->res[EQOS_RES_IRQ0],
		    sc->irq_handle);

	if (sc->ifp)
		if_free(sc->ifp);

	bus_release_resources(dev, eqos_spec, sc->res);

	if (sc->tx.desc_tag) {
		if (sc->tx.desc_map) {
			bus_dmamap_unload(sc->tx.desc_tag, sc->tx.desc_map);
			bus_dmamem_free(sc->tx.desc_tag, sc->tx.desc_ring,
			    sc->tx.desc_map);
		}
		bus_dma_tag_destroy(sc->tx.desc_tag);
	}
	if (sc->tx.buf_tag) {
		for (i = 0; i < TX_DESC_COUNT; i++) {
			m_free(sc->tx.buf_map[i].mbuf);
			bus_dmamap_destroy(sc->tx.buf_tag,
			    sc->tx.buf_map[i].map);
		}
		bus_dma_tag_destroy(sc->tx.buf_tag);
	}

	if (sc->rx.desc_tag) {
		if (sc->rx.desc_map) {
			bus_dmamap_unload(sc->rx.desc_tag, sc->rx.desc_map);
			bus_dmamem_free(sc->rx.desc_tag, sc->rx.desc_ring,
			    sc->rx.desc_map);
		}
		bus_dma_tag_destroy(sc->rx.desc_tag);
	}
	if (sc->rx.buf_tag) {
		for (i = 0; i < RX_DESC_COUNT; i++) {
			m_free(sc->rx.buf_map[i].mbuf);
			bus_dmamap_destroy(sc->rx.buf_tag,
			    sc->rx.buf_map[i].map);
		}
		bus_dma_tag_destroy(sc->rx.buf_tag);
	}

	mtx_destroy(&sc->lock);

	return (0);
}


static device_method_t eqos_methods[] = {
	/* Device Interface */
	DEVMETHOD(device_attach,	eqos_attach),
	DEVMETHOD(device_detach,	eqos_detach),

	/* MII Interface */
	DEVMETHOD(miibus_readreg,	eqos_miibus_readreg),
	DEVMETHOD(miibus_writereg,	eqos_miibus_writereg),
	DEVMETHOD(miibus_statchg,	eqos_miibus_statchg),

	DEVMETHOD_END
};

driver_t eqos_driver = {
	"eqos",
	eqos_methods,
	sizeof(struct eqos_softc),
};

DRIVER_MODULE(miibus, eqos, miibus_driver, 0, 0);
