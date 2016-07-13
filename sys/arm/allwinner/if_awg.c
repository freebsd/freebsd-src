/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
 * $FreeBSD$
 */

/*
 * Allwinner Gigabit Ethernet MAC (EMAC) controller
 */

#include "opt_device_polling.h"

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/module.h>
#include <sys/taskqueue.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/allwinner/if_awgreg.h>
#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/hwreset/hwreset.h>
#include <dev/extres/regulator/regulator.h>

#include "miibus_if.h"

#define	RD4(sc, reg)		bus_read_4((sc)->res[0], (reg))
#define	WR4(sc, reg, val)	bus_write_4((sc)->res[0], (reg), (val))

#define	AWG_LOCK(sc)		mtx_lock(&(sc)->mtx)
#define	AWG_UNLOCK(sc)		mtx_unlock(&(sc)->mtx);
#define	AWG_ASSERT_LOCKED(sc)	mtx_assert(&(sc)->mtx, MA_OWNED)
#define	AWG_ASSERT_UNLOCKED(sc)	mtx_assert(&(sc)->mtx, MA_NOTOWNED)

#define	DESC_ALIGN		4
#define	TX_DESC_COUNT		1024
#define	TX_DESC_SIZE		(sizeof(struct emac_desc) * TX_DESC_COUNT)
#define	RX_DESC_COUNT		256
#define	RX_DESC_SIZE		(sizeof(struct emac_desc) * RX_DESC_COUNT)

#define	DESC_OFF(n)		((n) * sizeof(struct emac_desc))
#define	TX_NEXT(n)		(((n) + 1) & (TX_DESC_COUNT - 1))
#define	TX_SKIP(n, o)		(((n) + (o)) & (TX_DESC_COUNT - 1))
#define	RX_NEXT(n)		(((n) + 1) & (RX_DESC_COUNT - 1))

#define	TX_MAX_SEGS		10

#define	SOFT_RST_RETRY		1000
#define	MII_BUSY_RETRY		1000
#define	MDIO_FREQ		2500000

#define	BURST_LEN_DEFAULT	8
#define	RX_TX_PRI_DEFAULT	0
#define	PAUSE_TIME_DEFAULT	0x400
#define	TX_INTERVAL_DEFAULT	64
#define	RX_BATCH_DEFAULT	64

/* Burst length of RX and TX DMA transfers */
static int awg_burst_len = BURST_LEN_DEFAULT;
TUNABLE_INT("hw.awg.burst_len", &awg_burst_len);

/* RX / TX DMA priority. If 1, RX DMA has priority over TX DMA. */
static int awg_rx_tx_pri = RX_TX_PRI_DEFAULT;
TUNABLE_INT("hw.awg.rx_tx_pri", &awg_rx_tx_pri);

/* Pause time field in the transmitted control frame */
static int awg_pause_time = PAUSE_TIME_DEFAULT;
TUNABLE_INT("hw.awg.pause_time", &awg_pause_time);

/* Request a TX interrupt every <n> descriptors */
static int awg_tx_interval = TX_INTERVAL_DEFAULT;
TUNABLE_INT("hw.awg.tx_interval", &awg_tx_interval);

/* Maximum number of mbufs to send to if_input */
static int awg_rx_batch = RX_BATCH_DEFAULT;
TUNABLE_INT("hw.awg.rx_batch", &awg_rx_batch);

static struct ofw_compat_data compat_data[] = {
	{ "allwinner,sun8i-a83t-emac",		1 },
	{ NULL,					0 }
};

struct awg_bufmap {
	bus_dmamap_t		map;
	struct mbuf		*mbuf;
};

struct awg_txring {
	bus_dma_tag_t		desc_tag;
	bus_dmamap_t		desc_map;
	struct emac_desc	*desc_ring;
	bus_addr_t		desc_ring_paddr;
	bus_dma_tag_t		buf_tag;
	struct awg_bufmap	buf_map[TX_DESC_COUNT];
	u_int			cur, next, queued;
};

struct awg_rxring {
	bus_dma_tag_t		desc_tag;
	bus_dmamap_t		desc_map;
	struct emac_desc	*desc_ring;
	bus_addr_t		desc_ring_paddr;
	bus_dma_tag_t		buf_tag;
	struct awg_bufmap	buf_map[RX_DESC_COUNT];
	u_int			cur;
};

struct awg_softc {
	struct resource		*res[2];
	struct mtx		mtx;
	if_t			ifp;
	device_t		miibus;
	struct callout		stat_ch;
	struct task		link_task;
	void			*ih;
	u_int			mdc_div_ratio_m;
	int			link;
	int			if_flags;

	struct awg_txring	tx;
	struct awg_rxring	rx;
};

static struct resource_spec awg_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ SYS_RES_IRQ,		0,	RF_ACTIVE },
	{ -1, 0 }
};

static int
awg_miibus_readreg(device_t dev, int phy, int reg)
{
	struct awg_softc *sc;
	int retry, val;

	sc = device_get_softc(dev);
	val = 0;

	WR4(sc, EMAC_MII_CMD,
	    (sc->mdc_div_ratio_m << MDC_DIV_RATIO_M_SHIFT) |
	    (phy << PHY_ADDR_SHIFT) |
	    (reg << PHY_REG_ADDR_SHIFT) |
	    MII_BUSY);
	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		if ((RD4(sc, EMAC_MII_CMD) & MII_BUSY) == 0) {
			val = RD4(sc, EMAC_MII_DATA);
			break;
		}
		DELAY(10);
	}

	if (retry == 0)
		device_printf(dev, "phy read timeout, phy=%d reg=%d\n",
		    phy, reg);

	return (val);
}

static int
awg_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct awg_softc *sc;
	int retry;

	sc = device_get_softc(dev);

	WR4(sc, EMAC_MII_DATA, val);
	WR4(sc, EMAC_MII_CMD,
	    (sc->mdc_div_ratio_m << MDC_DIV_RATIO_M_SHIFT) |
	    (phy << PHY_ADDR_SHIFT) |
	    (reg << PHY_REG_ADDR_SHIFT) |
	    MII_WR | MII_BUSY);
	for (retry = MII_BUSY_RETRY; retry > 0; retry--) {
		if ((RD4(sc, EMAC_MII_CMD) & MII_BUSY) == 0)
			break;
		DELAY(10);
	}

	if (retry == 0)
		device_printf(dev, "phy write timeout, phy=%d reg=%d\n",
		    phy, reg);

	return (0);
}

static void
awg_update_link_locked(struct awg_softc *sc)
{
	struct mii_data *mii;
	uint32_t val;

	AWG_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(sc->ifp) & IFF_DRV_RUNNING) == 0)
		return;
	mii = device_get_softc(sc->miibus);

	if ((mii->mii_media_status & (IFM_ACTIVE | IFM_AVALID)) ==
	    (IFM_ACTIVE | IFM_AVALID)) {
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_T:
		case IFM_1000_SX:
		case IFM_100_TX:
		case IFM_10_T:
			sc->link = 1;
			break;
		default:
			sc->link = 0;
			break;
		}
	} else
		sc->link = 0;

	if (sc->link == 0)
		return;

	val = RD4(sc, EMAC_BASIC_CTL_0);
	val &= ~(BASIC_CTL_SPEED | BASIC_CTL_DUPLEX);

	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T ||
	    IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_SX)
		val |= BASIC_CTL_SPEED_1000 << BASIC_CTL_SPEED_SHIFT;
	else if (IFM_SUBTYPE(mii->mii_media_active) == IFM_100_TX)
		val |= BASIC_CTL_SPEED_100 << BASIC_CTL_SPEED_SHIFT;
	else
		val |= BASIC_CTL_SPEED_10 << BASIC_CTL_SPEED_SHIFT;

	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
		val |= BASIC_CTL_DUPLEX;

	WR4(sc, EMAC_BASIC_CTL_0, val);

	val = RD4(sc, EMAC_RX_CTL_0);
	val &= ~RX_FLOW_CTL_EN;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_RXPAUSE) != 0)
		val |= RX_FLOW_CTL_EN;
	WR4(sc, EMAC_RX_CTL_0, val);

	val = RD4(sc, EMAC_TX_FLOW_CTL);
	val &= ~(PAUSE_TIME|TX_FLOW_CTL_EN);
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_ETH_TXPAUSE) != 0)
		val |= TX_FLOW_CTL_EN;
	if ((IFM_OPTIONS(mii->mii_media_active) & IFM_FDX) != 0)
		val |= awg_pause_time << PAUSE_TIME_SHIFT;
	WR4(sc, EMAC_TX_FLOW_CTL, val);
}

static void
awg_link_task(void *arg, int pending)
{
	struct awg_softc *sc;

	sc = arg;

	AWG_LOCK(sc);
	awg_update_link_locked(sc);
	AWG_UNLOCK(sc);
}

static void
awg_miibus_statchg(device_t dev)
{
	struct awg_softc *sc;

	sc = device_get_softc(dev);

	taskqueue_enqueue(taskqueue_swi, &sc->link_task);
}

static void
awg_media_status(if_t ifp, struct ifmediareq *ifmr)
{
	struct awg_softc *sc;
	struct mii_data *mii;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);

	AWG_LOCK(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	AWG_UNLOCK(sc);
}

static int
awg_media_change(if_t ifp)
{
	struct awg_softc *sc;
	struct mii_data *mii;
	int error;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);

	AWG_LOCK(sc);
	error = mii_mediachg(mii);
	AWG_UNLOCK(sc);

	return (error);
}

static void
awg_setup_txdesc(struct awg_softc *sc, int index, int flags, bus_addr_t paddr,
    u_int len)
{
	uint32_t status, size;

	if (paddr == 0 || len == 0) {
		status = 0;
		size = 0;
		--sc->tx.queued;
	} else {
		status = TX_DESC_CTL;
		size = flags | len;
		if ((index & (awg_tx_interval - 1)) == 0)
			size |= TX_INT_CTL;
		++sc->tx.queued;
	}

	sc->tx.desc_ring[index].addr = htole32((uint32_t)paddr);
	sc->tx.desc_ring[index].size = htole32(size);
	sc->tx.desc_ring[index].status = htole32(status);
}

static int
awg_setup_txbuf(struct awg_softc *sc, int index, struct mbuf **mp)
{
	bus_dma_segment_t segs[TX_MAX_SEGS];
	int error, nsegs, cur, i, flags;
	u_int csum_flags;
	struct mbuf *m;

	m = *mp;
	error = bus_dmamap_load_mbuf_sg(sc->tx.buf_tag,
	    sc->tx.buf_map[index].map, m, segs, &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		m = m_collapse(m, M_NOWAIT, TX_MAX_SEGS);
		if (m == NULL)
			return (0);
		*mp = m;
		error = bus_dmamap_load_mbuf_sg(sc->tx.buf_tag,
		    sc->tx.buf_map[index].map, m, segs, &nsegs, BUS_DMA_NOWAIT);
	}
	if (error != 0)
		return (0);

	bus_dmamap_sync(sc->tx.buf_tag, sc->tx.buf_map[index].map,
	    BUS_DMASYNC_PREWRITE);

	flags = TX_FIR_DESC;
	if ((m->m_pkthdr.csum_flags & CSUM_IP) != 0) {
		if ((m->m_pkthdr.csum_flags & (CSUM_TCP|CSUM_UDP)) != 0)
			csum_flags = TX_CHECKSUM_CTL_FULL;
		else
			csum_flags = TX_CHECKSUM_CTL_IP;
		flags |= (csum_flags << TX_CHECKSUM_CTL_SHIFT);
	}

	for (cur = index, i = 0; i < nsegs; i++) {
		sc->tx.buf_map[cur].mbuf = (i == 0 ? m : NULL);
		if (i == nsegs - 1)
			flags |= TX_LAST_DESC;
		awg_setup_txdesc(sc, cur, flags, segs[i].ds_addr,
		    segs[i].ds_len);
		flags &= ~TX_FIR_DESC;
		cur = TX_NEXT(cur);
	}

	return (nsegs);
}

static void
awg_setup_rxdesc(struct awg_softc *sc, int index, bus_addr_t paddr)
{
	uint32_t status, size;

	status = RX_DESC_CTL;
	size = MCLBYTES - 1;

	sc->rx.desc_ring[index].addr = htole32((uint32_t)paddr);
	sc->rx.desc_ring[index].size = htole32(size);
	sc->rx.desc_ring[index].next =
	    htole32(sc->rx.desc_ring_paddr + DESC_OFF(RX_NEXT(index)));
	sc->rx.desc_ring[index].status = htole32(status);
}

static int
awg_setup_rxbuf(struct awg_softc *sc, int index, struct mbuf *m)
{
	bus_dma_segment_t seg;
	int error, nsegs;

	m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(sc->rx.buf_tag,
	    sc->rx.buf_map[index].map, m, &seg, &nsegs, 0);
	if (error != 0)
		return (error);

	bus_dmamap_sync(sc->rx.buf_tag, sc->rx.buf_map[index].map,
	    BUS_DMASYNC_PREREAD);

	sc->rx.buf_map[index].mbuf = m;
	awg_setup_rxdesc(sc, index, seg.ds_addr);

	return (0);
}

static struct mbuf *
awg_alloc_mbufcl(struct awg_softc *sc)
{
	struct mbuf *m;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m != NULL)
		m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	return (m);
}

static void
awg_start_locked(struct awg_softc *sc)
{
	struct mbuf *m;
	uint32_t val;
	if_t ifp;
	int cnt, nsegs;

	AWG_ASSERT_LOCKED(sc);

	if (!sc->link)
		return;

	ifp = sc->ifp;

	if ((if_getdrvflags(ifp) & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	for (cnt = 0; ; cnt++) {
		if (sc->tx.queued >= TX_DESC_COUNT - TX_MAX_SEGS) {
			if_setdrvflagbits(ifp, IFF_DRV_OACTIVE, 0);
			break;
		}

		m = if_dequeue(ifp);
		if (m == NULL)
			break;

		nsegs = awg_setup_txbuf(sc, sc->tx.cur, &m);
		if (nsegs == 0) {
			if_sendq_prepend(ifp, m);
			break;
		}
		if_bpfmtap(ifp, m);
		sc->tx.cur = TX_SKIP(sc->tx.cur, nsegs);
	}

	if (cnt != 0) {
		bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		/* Start and run TX DMA */
		val = RD4(sc, EMAC_TX_CTL_1);
		WR4(sc, EMAC_TX_CTL_1, val | TX_DMA_START);
	}
}

static void
awg_start(if_t ifp)
{
	struct awg_softc *sc;

	sc = if_getsoftc(ifp);

	AWG_LOCK(sc);
	awg_start_locked(sc);
	AWG_UNLOCK(sc);
}

static void
awg_tick(void *softc)
{
	struct awg_softc *sc;
	struct mii_data *mii;
	if_t ifp;
	int link;

	sc = softc;
	ifp = sc->ifp;
	mii = device_get_softc(sc->miibus);

	AWG_ASSERT_LOCKED(sc);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		return;

	link = sc->link;
	mii_tick(mii);
	if (sc->link && !link)
		awg_start_locked(sc);

	callout_reset(&sc->stat_ch, hz, awg_tick, sc);
}

/* Bit Reversal - http://aggregate.org/MAGIC/#Bit%20Reversal */
static uint32_t
bitrev32(uint32_t x)
{
	x = (((x & 0xaaaaaaaa) >> 1) | ((x & 0x55555555) << 1));
	x = (((x & 0xcccccccc) >> 2) | ((x & 0x33333333) << 2));
	x = (((x & 0xf0f0f0f0) >> 4) | ((x & 0x0f0f0f0f) << 4));
	x = (((x & 0xff00ff00) >> 8) | ((x & 0x00ff00ff) << 8));

	return (x >> 16) | (x << 16);
}

static void
awg_setup_rxfilter(struct awg_softc *sc)
{
	uint32_t val, crc, hashreg, hashbit, hash[2], machi, maclo;
	int mc_count, mcnt, i;
	uint8_t *eaddr, *mta;
	if_t ifp;

	AWG_ASSERT_LOCKED(sc);

	ifp = sc->ifp;
	val = 0;
	hash[0] = hash[1] = 0;

	mc_count = if_multiaddr_count(ifp, -1);

	if (if_getflags(ifp) & IFF_PROMISC)
		val |= DIS_ADDR_FILTER;
	else if (if_getflags(ifp) & IFF_ALLMULTI) {
		val |= RX_ALL_MULTICAST;
		hash[0] = hash[1] = ~0;
	} else if (mc_count > 0) {
		val |= HASH_MULTICAST;

		mta = malloc(sizeof(unsigned char) * ETHER_ADDR_LEN * mc_count,
		    M_DEVBUF, M_NOWAIT);
		if (mta == NULL) {
			if_printf(ifp,
			    "failed to allocate temporary multicast list\n");
			return;
		}

		if_multiaddr_array(ifp, mta, &mcnt, mc_count);
		for (i = 0; i < mcnt; i++) {
			crc = ether_crc32_le(mta + (i * ETHER_ADDR_LEN),
			    ETHER_ADDR_LEN) & 0x7f;
			crc = bitrev32(~crc) >> 26;
			hashreg = (crc >> 5);
			hashbit = (crc & 0x1f);
			hash[hashreg] |= (1 << hashbit);
		}

		free(mta, M_DEVBUF);
	}

	/* Write our unicast address */
	eaddr = IF_LLADDR(ifp);
	machi = (eaddr[5] << 8) | eaddr[4];
	maclo = (eaddr[3] << 24) | (eaddr[2] << 16) | (eaddr[1] << 8) |
	   (eaddr[0] << 0);
	WR4(sc, EMAC_ADDR_HIGH(0), machi);
	WR4(sc, EMAC_ADDR_LOW(0), maclo);

	/* Multicast hash filters */
	WR4(sc, EMAC_RX_HASH_0, hash[1]);
	WR4(sc, EMAC_RX_HASH_1, hash[0]);

	/* RX frame filter config */
	WR4(sc, EMAC_RX_FRM_FLT, val);
}

static void
awg_enable_intr(struct awg_softc *sc)
{
	/* Enable interrupts */
	WR4(sc, EMAC_INT_EN, RX_INT_EN | TX_INT_EN | TX_BUF_UA_INT_EN);
}

static void
awg_disable_intr(struct awg_softc *sc)
{
	/* Disable interrupts */
	WR4(sc, EMAC_INT_EN, 0);
}

static void
awg_init_locked(struct awg_softc *sc)
{
	struct mii_data *mii;
	uint32_t val;
	if_t ifp;

	mii = device_get_softc(sc->miibus);
	ifp = sc->ifp;

	AWG_ASSERT_LOCKED(sc);

	if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
		return;

	awg_setup_rxfilter(sc);

	/* Configure DMA burst length and priorities */
	val = awg_burst_len << BASIC_CTL_BURST_LEN_SHIFT;
	if (awg_rx_tx_pri)
		val |= BASIC_CTL_RX_TX_PRI;
	WR4(sc, EMAC_BASIC_CTL_1, val);

	/* Enable interrupts */
#ifdef DEVICE_POLLING
	if ((if_getcapenable(ifp) & IFCAP_POLLING) == 0)
		awg_enable_intr(sc);
	else
		awg_disable_intr(sc);
#else
	awg_enable_intr(sc);
#endif

	/* Enable transmit DMA */
	val = RD4(sc, EMAC_TX_CTL_1);
	WR4(sc, EMAC_TX_CTL_1, val | TX_DMA_EN | TX_MD | TX_NEXT_FRAME);

	/* Enable receive DMA */
	val = RD4(sc, EMAC_RX_CTL_1);
	WR4(sc, EMAC_RX_CTL_1, val | RX_DMA_EN | RX_MD);

	/* Enable transmitter */
	val = RD4(sc, EMAC_TX_CTL_0);
	WR4(sc, EMAC_TX_CTL_0, val | TX_EN);

	/* Enable receiver */
	val = RD4(sc, EMAC_RX_CTL_0);
	WR4(sc, EMAC_RX_CTL_0, val | RX_EN | CHECK_CRC);

	if_setdrvflagbits(ifp, IFF_DRV_RUNNING, IFF_DRV_OACTIVE);

	mii_mediachg(mii);
	callout_reset(&sc->stat_ch, hz, awg_tick, sc);
}

static void
awg_init(void *softc)
{
	struct awg_softc *sc;

	sc = softc;

	AWG_LOCK(sc);
	awg_init_locked(sc);
	AWG_UNLOCK(sc);
}

static void
awg_stop(struct awg_softc *sc)
{
	if_t ifp;
	uint32_t val;

	AWG_ASSERT_LOCKED(sc);

	ifp = sc->ifp;

	callout_stop(&sc->stat_ch);

	/* Stop transmit DMA and flush data in the TX FIFO */
	val = RD4(sc, EMAC_TX_CTL_1);
	val &= ~TX_DMA_EN;
	val |= FLUSH_TX_FIFO;
	WR4(sc, EMAC_TX_CTL_1, val);

	/* Disable transmitter */
	val = RD4(sc, EMAC_TX_CTL_0);
	WR4(sc, EMAC_TX_CTL_0, val & ~TX_EN);

	/* Disable receiver */
	val = RD4(sc, EMAC_RX_CTL_0);
	WR4(sc, EMAC_RX_CTL_0, val & ~RX_EN);

	/* Disable interrupts */
	awg_disable_intr(sc);

	/* Disable transmit DMA */
	val = RD4(sc, EMAC_TX_CTL_1);
	WR4(sc, EMAC_TX_CTL_1, val & ~TX_DMA_EN);

	/* Disable receive DMA */
	val = RD4(sc, EMAC_RX_CTL_1);
	WR4(sc, EMAC_RX_CTL_1, val & ~RX_DMA_EN);

	sc->link = 0;

	if_setdrvflagbits(ifp, 0, IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}

static int
awg_rxintr(struct awg_softc *sc)
{
	if_t ifp;
	struct mbuf *m, *m0, *mh, *mt;
	int error, index, len, cnt, npkt;
	uint32_t status;

	ifp = sc->ifp;
	mh = mt = NULL;
	cnt = 0;
	npkt = 0;

	bus_dmamap_sync(sc->rx.desc_tag, sc->rx.desc_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (index = sc->rx.cur; ; index = RX_NEXT(index)) {
		status = le32toh(sc->rx.desc_ring[index].status);
		if ((status & RX_DESC_CTL) != 0)
			break;

		bus_dmamap_sync(sc->rx.buf_tag, sc->rx.buf_map[index].map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rx.buf_tag, sc->rx.buf_map[index].map);

		len = (status & RX_FRM_LEN) >> RX_FRM_LEN_SHIFT;
		if (len != 0) {
			m = sc->rx.buf_map[index].mbuf;
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = len;
			m->m_len = len;
			if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

			if ((if_getcapenable(ifp) & IFCAP_RXCSUM) != 0 &&
			    (status & RX_FRM_TYPE) != 0) {
				m->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
				if ((status & RX_HEADER_ERR) == 0)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				if ((status & RX_PAYLOAD_ERR) == 0) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
					m->m_pkthdr.csum_data = 0xffff;
				}
			}

			m->m_nextpkt = NULL;
			if (mh == NULL)
				mh = m;
			else
				mt->m_nextpkt = m;
			mt = m;
			++cnt;
			++npkt;

			if (cnt == awg_rx_batch) {
				AWG_UNLOCK(sc);
				if_input(ifp, mh);
				AWG_LOCK(sc);
				mh = mt = NULL;
				cnt = 0;
			}
			
		}

		if ((m0 = awg_alloc_mbufcl(sc)) != NULL) {
			error = awg_setup_rxbuf(sc, index, m0);
			if (error != 0) {
				/* XXX hole in RX ring */
			}
		} else
			if_inc_counter(ifp, IFCOUNTER_IQDROPS, 1);
	}

	if (index != sc->rx.cur) {
		bus_dmamap_sync(sc->rx.desc_tag, sc->rx.desc_map,
		    BUS_DMASYNC_PREWRITE);
	}

	if (mh != NULL) {
		AWG_UNLOCK(sc);
		if_input(ifp, mh);
		AWG_LOCK(sc);
	}

	sc->rx.cur = index;

	return (npkt);
}

static void
awg_txintr(struct awg_softc *sc)
{
	struct awg_bufmap *bmap;
	struct emac_desc *desc;
	uint32_t status;
	if_t ifp;
	int i;

	AWG_ASSERT_LOCKED(sc);

	bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	ifp = sc->ifp;
	for (i = sc->tx.next; sc->tx.queued > 0; i = TX_NEXT(i)) {
		desc = &sc->tx.desc_ring[i];
		status = le32toh(desc->status);
		if ((status & TX_DESC_CTL) != 0)
			break;
		bmap = &sc->tx.buf_map[i];
		if (bmap->mbuf != NULL) {
			bus_dmamap_sync(sc->tx.buf_tag, bmap->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->tx.buf_tag, bmap->map);
			m_freem(bmap->mbuf);
			bmap->mbuf = NULL;
		}
		awg_setup_txdesc(sc, i, 0, 0, 0);
		if_setdrvflagbits(ifp, 0, IFF_DRV_OACTIVE);
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
	}

	sc->tx.next = i;

	bus_dmamap_sync(sc->tx.desc_tag, sc->tx.desc_map,
	    BUS_DMASYNC_PREWRITE);
}

static void
awg_intr(void *arg)
{
	struct awg_softc *sc;
	uint32_t val;

	sc = arg;

	AWG_LOCK(sc);
	val = RD4(sc, EMAC_INT_STA);
	WR4(sc, EMAC_INT_STA, val);

	if (val & RX_INT)
		awg_rxintr(sc);

	if (val & (TX_INT|TX_BUF_UA_INT)) {
		awg_txintr(sc);
		if (!if_sendq_empty(sc->ifp))
			awg_start_locked(sc);
	}

	AWG_UNLOCK(sc);
}

#ifdef DEVICE_POLLING
static int
awg_poll(if_t ifp, enum poll_cmd cmd, int count)
{
	struct awg_softc *sc;
	uint32_t val;
	int rx_npkts;

	sc = if_getsoftc(ifp);
	rx_npkts = 0;

	AWG_LOCK(sc);

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0) {
		AWG_UNLOCK(sc);
		return (0);
	}

	rx_npkts = awg_rxintr(sc);
	awg_txintr(sc);
	if (!if_sendq_empty(ifp))
		awg_start_locked(sc);

	if (cmd == POLL_AND_CHECK_STATUS) {
		val = RD4(sc, EMAC_INT_STA);
		if (val != 0)
			WR4(sc, EMAC_INT_STA, val);
	}

	AWG_UNLOCK(sc);

	return (rx_npkts);
}
#endif

static int
awg_ioctl(if_t ifp, u_long cmd, caddr_t data)
{
	struct awg_softc *sc;
	struct mii_data *mii;
	struct ifreq *ifr;
	int flags, mask, error;

	sc = if_getsoftc(ifp);
	mii = device_get_softc(sc->miibus);
	ifr = (struct ifreq *)data;
	error = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		AWG_LOCK(sc);
		if (if_getflags(ifp) & IFF_UP) {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
				flags = if_getflags(ifp) ^ sc->if_flags;
				if ((flags & (IFF_PROMISC|IFF_ALLMULTI)) != 0)
					awg_setup_rxfilter(sc);
			} else
				awg_init_locked(sc);
		} else {
			if (if_getdrvflags(ifp) & IFF_DRV_RUNNING)
				awg_stop(sc);
		}
		sc->if_flags = if_getflags(ifp);
		AWG_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (if_getdrvflags(ifp) & IFF_DRV_RUNNING) {
			AWG_LOCK(sc);
			awg_setup_rxfilter(sc);
			AWG_UNLOCK(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
		mask = ifr->ifr_reqcap ^ if_getcapenable(ifp);
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if ((ifr->ifr_reqcap & IFCAP_POLLING) != 0) {
				error = ether_poll_register(awg_poll, ifp);
				if (error != 0)
					break;
				AWG_LOCK(sc);
				awg_disable_intr(sc);
				if_setcapenablebit(ifp, IFCAP_POLLING, 0);
				AWG_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				AWG_LOCK(sc);
				awg_enable_intr(sc);
				if_setcapenablebit(ifp, 0, IFCAP_POLLING);
				AWG_UNLOCK(sc);
			}
		}
#endif
		if (mask & IFCAP_VLAN_MTU)
			if_togglecapenable(ifp, IFCAP_VLAN_MTU);
		if (mask & IFCAP_RXCSUM)
			if_togglecapenable(ifp, IFCAP_RXCSUM);
		if (mask & IFCAP_TXCSUM)
			if_togglecapenable(ifp, IFCAP_TXCSUM);
		if ((if_getcapenable(ifp) & (IFCAP_RXCSUM|IFCAP_TXCSUM)) != 0)
			if_sethwassistbits(ifp, CSUM_IP, 0);
		else
			if_sethwassistbits(ifp, 0, CSUM_IP);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static int
awg_setup_extres(device_t dev)
{
	struct awg_softc *sc;
	hwreset_t rst_ahb;
	clk_t clk_ahb, clk_tx, clk_tx_parent;
	regulator_t reg;
	const char *tx_parent_name;
	char *phy_type;
	phandle_t node;
	uint64_t freq;
	int error, div;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);
	rst_ahb = NULL;
	clk_ahb = NULL;
	clk_tx = NULL;
	clk_tx_parent = NULL;
	reg = NULL;
	phy_type = NULL;

	/* Get AHB clock and reset resources */
	error = hwreset_get_by_ofw_name(dev, 0, "ahb", &rst_ahb);
	if (error != 0) {
		device_printf(dev, "cannot get ahb reset\n");
		goto fail;
	}
	error = clk_get_by_ofw_name(dev, 0, "ahb", &clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot get ahb clock\n");
		goto fail;
	}
	
	/* Configure PHY for MII or RGMII mode */
	if (OF_getprop_alloc(node, "phy-mode", 1, (void **)&phy_type)) {
		if (bootverbose)
			device_printf(dev, "PHY type: %s\n", phy_type);

		if (strcmp(phy_type, "rgmii") == 0)
			tx_parent_name = "emac_int_tx";
		else
			tx_parent_name = "mii_phy_tx";
		OF_prop_free(phy_type);

		/* Get the TX clock */
		error = clk_get_by_ofw_name(dev, 0, "tx", &clk_tx);
		if (error != 0) {
			device_printf(dev, "cannot get tx clock\n");
			goto fail;
		}

		/* Find the desired parent clock based on phy-mode property */
		error = clk_get_by_name(dev, tx_parent_name, &clk_tx_parent);
		if (error != 0) {
			device_printf(dev, "cannot get clock '%s'\n",
			    tx_parent_name);
			goto fail;
		}

		/* Set TX clock parent */
		error = clk_set_parent_by_clk(clk_tx, clk_tx_parent);
		if (error != 0) {
			device_printf(dev, "cannot set tx clock parent\n");
			goto fail;
		}

		/* Enable TX clock */
		error = clk_enable(clk_tx);
		if (error != 0) {
			device_printf(dev, "cannot enable tx clock\n");
			goto fail;
		}
	}

	/* Enable AHB clock */
	error = clk_enable(clk_ahb);
	if (error != 0) {
		device_printf(dev, "cannot enable ahb clock\n");
		goto fail;
	}

	/* De-assert reset */
	error = hwreset_deassert(rst_ahb);
	if (error != 0) {
		device_printf(dev, "cannot de-assert ahb reset\n");
		goto fail;
	}

	/* Enable PHY regulator if applicable */
	if (regulator_get_by_ofw_property(dev, 0, "phy-supply", &reg) == 0) {
		error = regulator_enable(reg);
		if (error != 0) {
			device_printf(dev, "cannot enable PHY regulator\n");
			goto fail;
		}
	}

	/* Determine MDC clock divide ratio based on AHB clock */
	error = clk_get_freq(clk_ahb, &freq);
	if (error != 0) {
		device_printf(dev, "cannot get AHB clock frequency\n");
		goto fail;
	}
	div = freq / MDIO_FREQ;
	if (div <= 16)
		sc->mdc_div_ratio_m = MDC_DIV_RATIO_M_16;
	else if (div <= 32)
		sc->mdc_div_ratio_m = MDC_DIV_RATIO_M_32;
	else if (div <= 64)
		sc->mdc_div_ratio_m = MDC_DIV_RATIO_M_64;
	else if (div <= 128)
		sc->mdc_div_ratio_m = MDC_DIV_RATIO_M_128;
	else {
		device_printf(dev, "cannot determine MDC clock divide ratio\n");
		error = ENXIO;
		goto fail;
	}

	if (bootverbose)
		device_printf(dev, "AHB frequency %llu Hz, MDC div: 0x%x\n",
		    freq, sc->mdc_div_ratio_m);

	return (0);

fail:
	OF_prop_free(phy_type);

	if (reg != NULL)
		regulator_release(reg);
	if (clk_tx_parent != NULL)
		clk_release(clk_tx_parent);
	if (clk_tx != NULL)
		clk_release(clk_tx);
	if (clk_ahb != NULL)
		clk_release(clk_ahb);
	if (rst_ahb != NULL)
		hwreset_release(rst_ahb);
	return (error);
}

static void 
awg_get_eaddr(device_t dev, uint8_t *eaddr)
{
	struct awg_softc *sc;
	uint32_t maclo, machi, rnd;

	sc = device_get_softc(dev);

	machi = RD4(sc, EMAC_ADDR_HIGH(0)) & 0xffff;
	maclo = RD4(sc, EMAC_ADDR_LOW(0));

	if (maclo == 0xffffffff && machi == 0xffff) {
		/* MAC address in hardware is invalid, create one */
		rnd = arc4random();
		maclo = 0x00f2 | (rnd & 0xffff0000);
		machi = rnd & 0xffff;
	}

	eaddr[0] = maclo & 0xff;
	eaddr[1] = (maclo >> 8) & 0xff;
	eaddr[2] = (maclo >> 16) & 0xff;
	eaddr[3] = (maclo >> 24) & 0xff;
	eaddr[4] = machi & 0xff;
	eaddr[5] = (machi >> 8) & 0xff;
}

#ifdef AWG_DEBUG
static void
awg_dump_regs(device_t dev)
{
	static const struct {
		const char *name;
		u_int reg;
	} regs[] = {
		{ "BASIC_CTL_0", EMAC_BASIC_CTL_0 },
		{ "BASIC_CTL_1", EMAC_BASIC_CTL_1 },
		{ "INT_STA", EMAC_INT_STA },
		{ "INT_EN", EMAC_INT_EN },
		{ "TX_CTL_0", EMAC_TX_CTL_0 },
		{ "TX_CTL_1", EMAC_TX_CTL_1 },
		{ "TX_FLOW_CTL", EMAC_TX_FLOW_CTL },
		{ "TX_DMA_LIST", EMAC_TX_DMA_LIST },
		{ "RX_CTL_0", EMAC_RX_CTL_0 },
		{ "RX_CTL_1", EMAC_RX_CTL_1 },
		{ "RX_DMA_LIST", EMAC_RX_DMA_LIST },
		{ "RX_FRM_FLT", EMAC_RX_FRM_FLT },
		{ "RX_HASH_0", EMAC_RX_HASH_0 },
		{ "RX_HASH_1", EMAC_RX_HASH_1 },
		{ "MII_CMD", EMAC_MII_CMD },
		{ "ADDR_HIGH0", EMAC_ADDR_HIGH(0) },
		{ "ADDR_LOW0", EMAC_ADDR_LOW(0) },
		{ "TX_DMA_STA", EMAC_TX_DMA_STA },
		{ "TX_DMA_CUR_DESC", EMAC_TX_DMA_CUR_DESC },
		{ "TX_DMA_CUR_BUF", EMAC_TX_DMA_CUR_BUF },
		{ "RX_DMA_STA", EMAC_RX_DMA_STA },
		{ "RX_DMA_CUR_DESC", EMAC_RX_DMA_CUR_DESC },
		{ "RX_DMA_CUR_BUF", EMAC_RX_DMA_CUR_BUF },
		{ "RGMII_STA", EMAC_RGMII_STA },
	};
	struct awg_softc *sc;
	unsigned int n;

	sc = device_get_softc(dev);

	for (n = 0; n < nitems(regs); n++)
		device_printf(dev, "  %-20s %08x\n", regs[n].name,
		    RD4(sc, regs[n].reg));
}
#endif

static int
awg_reset(device_t dev)
{
	struct awg_softc *sc;
	int retry;

	sc = device_get_softc(dev);

	/* Soft reset all registers and logic */
	WR4(sc, EMAC_BASIC_CTL_1, BASIC_CTL_SOFT_RST);

	/* Wait for soft reset bit to self-clear */
	for (retry = SOFT_RST_RETRY; retry > 0; retry--) {
		if ((RD4(sc, EMAC_BASIC_CTL_1) & BASIC_CTL_SOFT_RST) == 0)
			break;
		DELAY(10);
	}
	if (retry == 0) {
		device_printf(dev, "soft reset timed out\n");
#ifdef AWG_DEBUG
		awg_dump_regs(dev);
#endif
		return (ETIMEDOUT);
	}

	return (0);
}

static void
awg_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error != 0)
		return;
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

static int
awg_setup_dma(device_t dev)
{
	struct awg_softc *sc;
	struct mbuf *m;
	int error, i;

	sc = device_get_softc(dev);

	/* Setup TX ring */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    TX_DESC_SIZE, 1,		/* maxsize, nsegs */
	    TX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->tx.desc_tag);
	if (error != 0) {
		device_printf(dev, "cannot create TX descriptor ring tag\n");
		return (error);
	}

	error = bus_dmamem_alloc(sc->tx.desc_tag, (void **)&sc->tx.desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->tx.desc_map);
	if (error != 0) {
		device_printf(dev, "cannot allocate TX descriptor ring\n");
		return (error);
	}

	error = bus_dmamap_load(sc->tx.desc_tag, sc->tx.desc_map,
	    sc->tx.desc_ring, TX_DESC_SIZE, awg_dmamap_cb,
	    &sc->tx.desc_ring_paddr, 0);
	if (error != 0) {
		device_printf(dev, "cannot load TX descriptor ring\n");
		return (error);
	}

	for (i = 0; i < TX_DESC_COUNT; i++)
		sc->tx.desc_ring[i].next =
		    htole32(sc->tx.desc_ring_paddr + DESC_OFF(TX_NEXT(i)));

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, TX_MAX_SEGS,	/* maxsize, nsegs */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->tx.buf_tag);
	if (error != 0) {
		device_printf(dev, "cannot create TX buffer tag\n");
		return (error);
	}

	sc->tx.queued = TX_DESC_COUNT;
	for (i = 0; i < TX_DESC_COUNT; i++) {
		error = bus_dmamap_create(sc->tx.buf_tag, 0,
		    &sc->tx.buf_map[i].map);
		if (error != 0) {
			device_printf(dev, "cannot create TX buffer map\n");
			return (error);
		}
		awg_setup_txdesc(sc, i, 0, 0, 0);
	}

	/* Setup RX ring */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    RX_DESC_SIZE, 1,		/* maxsize, nsegs */
	    RX_DESC_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rx.desc_tag);
	if (error != 0) {
		device_printf(dev, "cannot create RX descriptor ring tag\n");
		return (error);
	}

	error = bus_dmamem_alloc(sc->rx.desc_tag, (void **)&sc->rx.desc_ring,
	    BUS_DMA_COHERENT | BUS_DMA_WAITOK | BUS_DMA_ZERO, &sc->rx.desc_map);
	if (error != 0) {
		device_printf(dev, "cannot allocate RX descriptor ring\n");
		return (error);
	}

	error = bus_dmamap_load(sc->rx.desc_tag, sc->rx.desc_map,
	    sc->rx.desc_ring, RX_DESC_SIZE, awg_dmamap_cb,
	    &sc->rx.desc_ring_paddr, 0);
	if (error != 0) {
		device_printf(dev, "cannot load RX descriptor ring\n");
		return (error);
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),	/* Parent tag */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1,		/* maxsize, nsegs */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->rx.buf_tag);
	if (error != 0) {
		device_printf(dev, "cannot create RX buffer tag\n");
		return (error);
	}

	for (i = 0; i < RX_DESC_COUNT; i++) {
		error = bus_dmamap_create(sc->rx.buf_tag, 0,
		    &sc->rx.buf_map[i].map);
		if (error != 0) {
			device_printf(dev, "cannot create RX buffer map\n");
			return (error);
		}
		if ((m = awg_alloc_mbufcl(sc)) == NULL) {
			device_printf(dev, "cannot allocate RX mbuf\n");
			return (ENOMEM);
		}
		error = awg_setup_rxbuf(sc, i, m);
		if (error != 0) {
			device_printf(dev, "cannot create RX buffer\n");
			return (error);
		}
	}
	bus_dmamap_sync(sc->rx.desc_tag, sc->rx.desc_map,
	    BUS_DMASYNC_PREWRITE);

	/* Write transmit and receive descriptor base address registers */
	WR4(sc, EMAC_TX_DMA_LIST, sc->tx.desc_ring_paddr);
	WR4(sc, EMAC_RX_DMA_LIST, sc->rx.desc_ring_paddr);

	return (0);
}

static int
awg_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Gigabit Ethernet");
	return (BUS_PROBE_DEFAULT);
}

static int
awg_attach(device_t dev)
{
	uint8_t eaddr[ETHER_ADDR_LEN];
	struct awg_softc *sc;
	phandle_t node;
	int error;

	sc = device_get_softc(dev);
	node = ofw_bus_get_node(dev);

	if (bus_alloc_resources(dev, awg_spec, sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK, MTX_DEF);
	callout_init_mtx(&sc->stat_ch, &sc->mtx, 0);
	TASK_INIT(&sc->link_task, 0, awg_link_task, sc);

	/* Setup clocks and regulators */
	error = awg_setup_extres(dev);
	if (error != 0)
		return (error);

	/* Read MAC address before resetting the chip */
	awg_get_eaddr(dev, eaddr);

	/* Soft reset EMAC core */
	error = awg_reset(dev);
	if (error != 0)
		return (error);

	/* Setup DMA descriptors */
	error = awg_setup_dma(dev);
	if (error != 0)
		return (error);

	/* Install interrupt handler */
	error = bus_setup_intr(dev, sc->res[1], INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, awg_intr, sc, &sc->ih);
	if (error != 0) {
		device_printf(dev, "cannot setup interrupt handler\n");
		return (error);
	}

	/* Setup ethernet interface */
	sc->ifp = if_alloc(IFT_ETHER);
	if_setsoftc(sc->ifp, sc);
	if_initname(sc->ifp, device_get_name(dev), device_get_unit(dev));
	if_setflags(sc->ifp, IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST);
	if_setstartfn(sc->ifp, awg_start);
	if_setioctlfn(sc->ifp, awg_ioctl);
	if_setinitfn(sc->ifp, awg_init);
	if_setsendqlen(sc->ifp, TX_DESC_COUNT - 1);
	if_setsendqready(sc->ifp);
	if_sethwassist(sc->ifp, CSUM_IP | CSUM_UDP | CSUM_TCP);
	if_setcapabilities(sc->ifp, IFCAP_VLAN_MTU | IFCAP_HWCSUM);
	if_setcapenable(sc->ifp, if_getcapabilities(sc->ifp));
#ifdef DEVICE_POLLING
	if_setcapabilitiesbit(sc->ifp, IFCAP_POLLING, 0);
#endif

	/* Attach MII driver */
	error = mii_attach(dev, &sc->miibus, sc->ifp, awg_media_change,
	    awg_media_status, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY,
	    MIIF_DOPAUSE);
	if (error != 0) {
		device_printf(dev, "cannot attach PHY\n");
		return (error);
	}

	/* Attach ethernet interface */
	ether_ifattach(sc->ifp, eaddr);

	return (0);
}

static device_method_t awg_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		awg_probe),
	DEVMETHOD(device_attach,	awg_attach),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	awg_miibus_readreg),
	DEVMETHOD(miibus_writereg,	awg_miibus_writereg),
	DEVMETHOD(miibus_statchg,	awg_miibus_statchg),

	DEVMETHOD_END
};

static driver_t awg_driver = {
	"awg",
	awg_methods,
	sizeof(struct awg_softc),
};

static devclass_t awg_devclass;

DRIVER_MODULE(awg, simplebus, awg_driver, awg_devclass, 0, 0);
DRIVER_MODULE(miibus, awg, miibus_driver, miibus_devclass, 0, 0);

MODULE_DEPEND(awg, ether, 1, 1, 1);
MODULE_DEPEND(awg, miibus, 1, 1, 1);
