/*-
 * Copyright (c) 2011 Jakub Wojciech Klama <jceel@FreeBSD.org>
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
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/bus.h>
#include <sys/socket.h>
#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_var.h>

#include <net/bpf.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <arm/lpc/lpcreg.h>
#include <arm/lpc/lpcvar.h>
#include <arm/lpc/if_lpereg.h>

#include "miibus_if.h"

#ifdef DEBUG
#define debugf(fmt, args...) do { printf("%s(): ", __func__);   \
    printf(fmt,##args); } while (0)
#else
#define debugf(fmt, args...)
#endif

struct lpe_dmamap_arg {
	bus_addr_t		lpe_dma_busaddr;
};

struct lpe_rxdesc {
	struct mbuf *		lpe_rxdesc_mbuf;
	bus_dmamap_t		lpe_rxdesc_dmamap;
};

struct lpe_txdesc {
	int			lpe_txdesc_first;
	struct mbuf *		lpe_txdesc_mbuf;
	bus_dmamap_t		lpe_txdesc_dmamap;
};

struct lpe_chain_data {
	bus_dma_tag_t		lpe_parent_tag;
	bus_dma_tag_t		lpe_tx_ring_tag;
	bus_dmamap_t		lpe_tx_ring_map;
	bus_dma_tag_t		lpe_tx_status_tag;
	bus_dmamap_t		lpe_tx_status_map;
	bus_dma_tag_t		lpe_tx_buf_tag;
	bus_dma_tag_t		lpe_rx_ring_tag;
	bus_dmamap_t		lpe_rx_ring_map;
	bus_dma_tag_t		lpe_rx_status_tag;
	bus_dmamap_t		lpe_rx_status_map;
	bus_dma_tag_t		lpe_rx_buf_tag;
	struct lpe_rxdesc	lpe_rx_desc[LPE_RXDESC_NUM];
	struct lpe_txdesc	lpe_tx_desc[LPE_TXDESC_NUM];
	int			lpe_tx_prod;
	int			lpe_tx_last;
	int			lpe_tx_used;
};

struct lpe_ring_data {
	struct lpe_hwdesc *	lpe_rx_ring;
	struct lpe_hwstatus *	lpe_rx_status;
	bus_addr_t		lpe_rx_ring_phys;
	bus_addr_t		lpe_rx_status_phys;
	struct lpe_hwdesc *	lpe_tx_ring;
	struct lpe_hwstatus *	lpe_tx_status;
	bus_addr_t		lpe_tx_ring_phys;
	bus_addr_t		lpe_tx_status_phys;
};

struct lpe_softc {
	struct ifnet *		lpe_ifp;
	struct mtx		lpe_mtx;
	phandle_t		lpe_ofw;
	device_t		lpe_dev;
	device_t		lpe_miibus;
	uint8_t			lpe_enaddr[6];
	struct resource	*	lpe_mem_res;
	struct resource *	lpe_irq_res;
	void *			lpe_intrhand;
	bus_space_tag_t		lpe_bst;
	bus_space_handle_t	lpe_bsh;
#define	LPE_FLAG_LINK		(1 << 0)
	uint32_t		lpe_flags;
	int			lpe_watchdog_timer;
	struct callout		lpe_tick;
	struct lpe_chain_data	lpe_cdata;
	struct lpe_ring_data	lpe_rdata;
};

static int lpe_probe(device_t);
static int lpe_attach(device_t);
static int lpe_detach(device_t);
static int lpe_miibus_readreg(device_t, int, int);
static int lpe_miibus_writereg(device_t, int, int, int);
static void lpe_miibus_statchg(device_t);

static void lpe_reset(struct lpe_softc *);
static void lpe_init(void *);
static void lpe_init_locked(struct lpe_softc *);
static void lpe_start(struct ifnet *);
static void lpe_start_locked(struct ifnet *);
static void lpe_stop(struct lpe_softc *);
static void lpe_stop_locked(struct lpe_softc *);
static int lpe_ioctl(struct ifnet *, u_long, caddr_t);
static void lpe_set_rxmode(struct lpe_softc *);
static void lpe_set_rxfilter(struct lpe_softc *);
static void lpe_intr(void *);
static void lpe_rxintr(struct lpe_softc *);
static void lpe_txintr(struct lpe_softc *);
static void lpe_tick(void *);
static void lpe_watchdog(struct lpe_softc *);
static int lpe_encap(struct lpe_softc *, struct mbuf **);
static int lpe_dma_alloc(struct lpe_softc *);
static int lpe_dma_alloc_rx(struct lpe_softc *);
static int lpe_dma_alloc_tx(struct lpe_softc *);
static int lpe_init_rx(struct lpe_softc *);
static int lpe_init_rxbuf(struct lpe_softc *, int);
static void lpe_discard_rxbuf(struct lpe_softc *, int);
static void lpe_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static int lpe_ifmedia_upd(struct ifnet *);
static void lpe_ifmedia_sts(struct ifnet *, struct ifmediareq *);

#define	lpe_lock(_sc)		mtx_lock(&(_sc)->lpe_mtx)
#define	lpe_unlock(_sc)		mtx_unlock(&(_sc)->lpe_mtx)
#define	lpe_lock_assert(sc)	mtx_assert(&(_sc)->lpe_mtx, MA_OWNED)

#define	lpe_read_4(_sc, _reg)		\
    bus_space_read_4((_sc)->lpe_bst, (_sc)->lpe_bsh, (_reg))
#define	lpe_write_4(_sc, _reg, _val)	\
    bus_space_write_4((_sc)->lpe_bst, (_sc)->lpe_bsh, (_reg), (_val))

#define	LPE_HWDESC_RXERRS	(LPE_HWDESC_CRCERROR | LPE_HWDESC_SYMBOLERROR | \
    LPE_HWDESC_LENGTHERROR | LPE_HWDESC_ALIGNERROR | LPE_HWDESC_OVERRUN | \
    LPE_HWDESC_RXNODESCR)

#define	LPE_HWDESC_TXERRS	(LPE_HWDESC_EXCDEFER | LPE_HWDESC_EXCCOLL | \
    LPE_HWDESC_LATECOLL | LPE_HWDESC_UNDERRUN | LPE_HWDESC_TXNODESCR)

static int
lpe_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "lpc,ethernet"))
		return (ENXIO);

	device_set_desc(dev, "LPC32x0 10/100 Ethernet");
	return (BUS_PROBE_DEFAULT);
}

static int
lpe_attach(device_t dev)
{
	struct lpe_softc *sc = device_get_softc(dev);
	struct ifnet *ifp;
	int rid, i;
	uint32_t val;

	sc->lpe_dev = dev;
	sc->lpe_ofw = ofw_bus_get_node(dev);

	i = OF_getprop(sc->lpe_ofw, "local-mac-address", (void *)&sc->lpe_enaddr, 6);
	if (i != 6) {
		sc->lpe_enaddr[0] = 0x00;
		sc->lpe_enaddr[1] = 0x11;
		sc->lpe_enaddr[2] = 0x22;
		sc->lpe_enaddr[3] = 0x33;
		sc->lpe_enaddr[4] = 0x44;
		sc->lpe_enaddr[5] = 0x55;
	}

	mtx_init(&sc->lpe_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);

	callout_init_mtx(&sc->lpe_tick, &sc->lpe_mtx, 0);

	rid = 0;
	sc->lpe_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (!sc->lpe_mem_res) {
		device_printf(dev, "cannot allocate memory window\n");
		goto fail;
	}

	sc->lpe_bst = rman_get_bustag(sc->lpe_mem_res);
	sc->lpe_bsh = rman_get_bushandle(sc->lpe_mem_res);

	rid = 0;
	sc->lpe_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_ACTIVE);
	if (!sc->lpe_irq_res) {
		device_printf(dev, "cannot allocate interrupt\n");
		goto fail;
	}

	sc->lpe_ifp = if_alloc(IFT_ETHER);
	if (!sc->lpe_ifp) {
		device_printf(dev, "cannot allocated ifnet\n");
		goto fail;
	}

	ifp = sc->lpe_ifp;

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_start = lpe_start;
	ifp->if_ioctl = lpe_ioctl;
	ifp->if_init = lpe_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_snd.ifq_drv_maxlen = IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, sc->lpe_enaddr);

	if (bus_setup_intr(dev, sc->lpe_irq_res, INTR_TYPE_NET, NULL,
	    lpe_intr, sc, &sc->lpe_intrhand)) {
		device_printf(dev, "cannot establish interrupt handler\n");
		ether_ifdetach(ifp);
		goto fail;
	}

	/* Enable Ethernet clock */
	lpc_pwr_write(dev, LPC_CLKPWR_MACCLK_CTRL,
	    LPC_CLKPWR_MACCLK_CTRL_REG |
	    LPC_CLKPWR_MACCLK_CTRL_SLAVE |
	    LPC_CLKPWR_MACCLK_CTRL_MASTER |
	    LPC_CLKPWR_MACCLK_CTRL_HDWINF(3));

	/* Reset chip */
	lpe_reset(sc);

	/* Initialize MII */
	val = lpe_read_4(sc, LPE_COMMAND);
	lpe_write_4(sc, LPE_COMMAND, val | LPE_COMMAND_RMII);

	if (mii_attach(dev, &sc->lpe_miibus, ifp, lpe_ifmedia_upd,
	    lpe_ifmedia_sts, BMSR_DEFCAPMASK, 0x01, 
	    MII_OFFSET_ANY, 0)) {
		device_printf(dev, "cannot find PHY\n");
		goto fail;
	}

	lpe_dma_alloc(sc);

	return (0);

fail:
	if (sc->lpe_ifp)
		if_free(sc->lpe_ifp);
	if (sc->lpe_intrhand)
		bus_teardown_intr(dev, sc->lpe_irq_res, sc->lpe_intrhand);
	if (sc->lpe_irq_res)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->lpe_irq_res);
	if (sc->lpe_mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lpe_mem_res);
	return (ENXIO);
}

static int
lpe_detach(device_t dev)
{
	struct lpe_softc *sc = device_get_softc(dev);

	lpe_stop(sc);

	if_free(sc->lpe_ifp);
	bus_teardown_intr(dev, sc->lpe_irq_res, sc->lpe_intrhand);
	bus_release_resource(dev, SYS_RES_IRQ, 0, sc->lpe_irq_res);
	bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->lpe_mem_res);

	return (0);
}

static int
lpe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct lpe_softc *sc = device_get_softc(dev);
	uint32_t val;
	int result;

	lpe_write_4(sc, LPE_MCMD, LPE_MCMD_READ);
	lpe_write_4(sc, LPE_MADR, 
	    (reg & LPE_MADR_REGMASK) << LPE_MADR_REGSHIFT |
	    (phy & LPE_MADR_PHYMASK) << LPE_MADR_PHYSHIFT);

	val = lpe_read_4(sc, LPE_MIND);

	/* Wait until request is completed */
	while (val & LPE_MIND_BUSY) {
		val = lpe_read_4(sc, LPE_MIND);
		DELAY(10);
	}

	if (val & LPE_MIND_INVALID)
		return (0);

	lpe_write_4(sc, LPE_MCMD, 0);
	result = (lpe_read_4(sc, LPE_MRDD) & LPE_MRDD_DATAMASK);
	debugf("phy=%d reg=%d result=0x%04x\n", phy, reg, result);

	return (result);
}

static int
lpe_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct lpe_softc *sc = device_get_softc(dev);
	uint32_t val;

	debugf("phy=%d reg=%d data=0x%04x\n", phy, reg, data);

	lpe_write_4(sc, LPE_MCMD, LPE_MCMD_WRITE);
	lpe_write_4(sc, LPE_MADR, 
	    (reg & LPE_MADR_REGMASK) << LPE_MADR_REGSHIFT |
	    (phy & LPE_MADR_PHYMASK) << LPE_MADR_PHYSHIFT);

	lpe_write_4(sc, LPE_MWTD, (data & LPE_MWTD_DATAMASK));

	val = lpe_read_4(sc, LPE_MIND);

	/* Wait until request is completed */
	while (val & LPE_MIND_BUSY) {
		val = lpe_read_4(sc, LPE_MIND);
		DELAY(10);
	}

	return (0);
}

static void
lpe_miibus_statchg(device_t dev)
{
	struct lpe_softc *sc = device_get_softc(dev);
	struct mii_data *mii = device_get_softc(sc->lpe_miibus);

	lpe_lock(sc);

	if ((mii->mii_media_status & IFM_ACTIVE) &&
	    (mii->mii_media_status & IFM_AVALID))
		sc->lpe_flags |= LPE_FLAG_LINK;
	else
		sc->lpe_flags &= ~LPE_FLAG_LINK;

	lpe_unlock(sc);
}

static void
lpe_reset(struct lpe_softc *sc)
{
	uint32_t mac1;

	/* Enter soft reset mode */
	mac1 = lpe_read_4(sc, LPE_MAC1);
	lpe_write_4(sc, LPE_MAC1, mac1 | LPE_MAC1_SOFTRESET | LPE_MAC1_RESETTX |
	    LPE_MAC1_RESETMCSTX | LPE_MAC1_RESETRX | LPE_MAC1_RESETMCSRX);

	/* Reset registers, Tx path and Rx path */
	lpe_write_4(sc, LPE_COMMAND, LPE_COMMAND_REGRESET |
	    LPE_COMMAND_TXRESET | LPE_COMMAND_RXRESET);

	/* Set station address */
	lpe_write_4(sc, LPE_SA2, sc->lpe_enaddr[1] << 8 | sc->lpe_enaddr[0]);
	lpe_write_4(sc, LPE_SA1, sc->lpe_enaddr[3] << 8 | sc->lpe_enaddr[2]);
	lpe_write_4(sc, LPE_SA0, sc->lpe_enaddr[5] << 8 | sc->lpe_enaddr[4]);

	/* Leave soft reset mode */
	mac1 = lpe_read_4(sc, LPE_MAC1);
	lpe_write_4(sc, LPE_MAC1, mac1 & ~(LPE_MAC1_SOFTRESET | LPE_MAC1_RESETTX |
	    LPE_MAC1_RESETMCSTX | LPE_MAC1_RESETRX | LPE_MAC1_RESETMCSRX));
}

static void
lpe_init(void *arg)
{
	struct lpe_softc *sc = (struct lpe_softc *)arg;

	lpe_lock(sc);
	lpe_init_locked(sc);
	lpe_unlock(sc);
}

static void
lpe_init_locked(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;
	uint32_t cmd, mac1;

	lpe_lock_assert(sc);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	/* Enable Tx and Rx */
	cmd = lpe_read_4(sc, LPE_COMMAND);
	lpe_write_4(sc, LPE_COMMAND, cmd | LPE_COMMAND_RXENABLE |
	    LPE_COMMAND_TXENABLE | LPE_COMMAND_PASSRUNTFRAME);

	/* Enable receive */
	mac1 = lpe_read_4(sc, LPE_MAC1);
	lpe_write_4(sc, LPE_MAC1, /*mac1 |*/ LPE_MAC1_RXENABLE | LPE_MAC1_PASSALL);

	lpe_write_4(sc, LPE_MAC2, LPE_MAC2_CRCENABLE | LPE_MAC2_PADCRCENABLE |
	    LPE_MAC2_FULLDUPLEX);

	lpe_write_4(sc, LPE_MCFG, LPE_MCFG_CLKSEL(7));

	/* Set up Rx filter */
	lpe_set_rxmode(sc);

	/* Enable interrupts */
	lpe_write_4(sc, LPE_INTENABLE, LPE_INT_RXOVERRUN | LPE_INT_RXERROR |
	    LPE_INT_RXFINISH | LPE_INT_RXDONE | LPE_INT_TXUNDERRUN | 
	    LPE_INT_TXERROR | LPE_INT_TXFINISH | LPE_INT_TXDONE);

	sc->lpe_cdata.lpe_tx_prod = 0;
	sc->lpe_cdata.lpe_tx_last = 0;
	sc->lpe_cdata.lpe_tx_used = 0;

	lpe_init_rx(sc);

	/* Initialize Rx packet and status descriptor heads */
	lpe_write_4(sc, LPE_RXDESC, sc->lpe_rdata.lpe_rx_ring_phys);
	lpe_write_4(sc, LPE_RXSTATUS, sc->lpe_rdata.lpe_rx_status_phys);
	lpe_write_4(sc, LPE_RXDESC_NUMBER, LPE_RXDESC_NUM - 1);
	lpe_write_4(sc, LPE_RXDESC_CONS, 0);

	/* Initialize Tx packet and status descriptor heads */
	lpe_write_4(sc, LPE_TXDESC, sc->lpe_rdata.lpe_tx_ring_phys);
	lpe_write_4(sc, LPE_TXSTATUS, sc->lpe_rdata.lpe_tx_status_phys);
	lpe_write_4(sc, LPE_TXDESC_NUMBER, LPE_TXDESC_NUM - 1);
	lpe_write_4(sc, LPE_TXDESC_PROD, 0);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->lpe_tick, hz, lpe_tick, sc);
}

static void
lpe_start(struct ifnet *ifp)
{
	struct lpe_softc *sc = (struct lpe_softc *)ifp->if_softc;

	lpe_lock(sc);
	lpe_start_locked(ifp);
	lpe_unlock(sc);
}

static void
lpe_start_locked(struct ifnet *ifp)
{
	struct lpe_softc *sc = (struct lpe_softc *)ifp->if_softc;
	struct mbuf *m_head;
	int encap = 0;

	lpe_lock_assert(sc);

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		if (lpe_read_4(sc, LPE_TXDESC_PROD) ==
		    lpe_read_4(sc, LPE_TXDESC_CONS) - 5)
			break;

		/* Dequeue first packet */
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (!m_head)
			break;

		lpe_encap(sc, &m_head);

		encap++;
	}

	/* Submit new descriptor list */
	if (encap) {
		lpe_write_4(sc, LPE_TXDESC_PROD, sc->lpe_cdata.lpe_tx_prod);
		sc->lpe_watchdog_timer = 5;
	}
	
}

static int
lpe_encap(struct lpe_softc *sc, struct mbuf **m_head)
{
	struct lpe_txdesc *txd;
	struct lpe_hwdesc *hwd;
	bus_dma_segment_t segs[LPE_MAXFRAGS];
	int i, err, nsegs, prod;

	lpe_lock_assert(sc);
	M_ASSERTPKTHDR((*m_head));

	prod = sc->lpe_cdata.lpe_tx_prod;
	txd = &sc->lpe_cdata.lpe_tx_desc[prod];

	debugf("starting with prod=%d\n", prod);

	err = bus_dmamap_load_mbuf_sg(sc->lpe_cdata.lpe_tx_buf_tag,
	    txd->lpe_txdesc_dmamap, *m_head, segs, &nsegs, BUS_DMA_NOWAIT);

	if (err)
		return (err);

	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

        bus_dmamap_sync(sc->lpe_cdata.lpe_tx_buf_tag, txd->lpe_txdesc_dmamap,
          BUS_DMASYNC_PREREAD);
        bus_dmamap_sync(sc->lpe_cdata.lpe_tx_ring_tag, sc->lpe_cdata.lpe_tx_ring_map,
            BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	txd->lpe_txdesc_first = 1;
	txd->lpe_txdesc_mbuf = *m_head;

	for (i = 0; i < nsegs; i++) {
		hwd = &sc->lpe_rdata.lpe_tx_ring[prod];
		hwd->lhr_data = segs[i].ds_addr;
		hwd->lhr_control = segs[i].ds_len - 1;

		if (i == nsegs - 1) {
			hwd->lhr_control |= LPE_HWDESC_LASTFLAG;
			hwd->lhr_control |= LPE_HWDESC_INTERRUPT;
			hwd->lhr_control |= LPE_HWDESC_CRC;
			hwd->lhr_control |= LPE_HWDESC_PAD;
		}

		LPE_INC(prod, LPE_TXDESC_NUM);
	}

	bus_dmamap_sync(sc->lpe_cdata.lpe_tx_ring_tag, sc->lpe_cdata.lpe_tx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->lpe_cdata.lpe_tx_used += nsegs;
	sc->lpe_cdata.lpe_tx_prod = prod;

	return (0);
}

static void
lpe_stop(struct lpe_softc *sc)
{
	lpe_lock(sc);
	lpe_stop_locked(sc);
	lpe_unlock(sc);
}

static void
lpe_stop_locked(struct lpe_softc *sc)
{
	lpe_lock_assert(sc);

	callout_stop(&sc->lpe_tick);

	/* Disable interrupts */
	lpe_write_4(sc, LPE_INTCLEAR, 0xffffffff);

	/* Stop EMAC */
	lpe_write_4(sc, LPE_MAC1, 0);
	lpe_write_4(sc, LPE_MAC2, 0);
	lpe_write_4(sc, LPE_COMMAND, 0);

	sc->lpe_ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->lpe_ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
}

static int
lpe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct lpe_softc *sc = ifp->if_softc;
	struct mii_data *mii = device_get_softc(sc->lpe_miibus);
	struct ifreq *ifr = (struct ifreq *)data;
	int err = 0;

	switch (cmd) {
	case SIOCSIFFLAGS:
		lpe_lock(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				lpe_set_rxmode(sc);
				lpe_set_rxfilter(sc);
			} else
				lpe_init_locked(sc);
		} else
			lpe_stop(sc);
		lpe_unlock(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			lpe_lock(sc);
			lpe_set_rxfilter(sc);
			lpe_unlock(sc);
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		err = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	default:
		err = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (err);
}

static void lpe_set_rxmode(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;
	uint32_t rxfilt;

	rxfilt = LPE_RXFILTER_UNIHASH | LPE_RXFILTER_MULTIHASH | LPE_RXFILTER_PERFECT;

	if (ifp->if_flags & IFF_BROADCAST)
		rxfilt |= LPE_RXFILTER_BROADCAST;

	if (ifp->if_flags & IFF_PROMISC)
		rxfilt |= LPE_RXFILTER_UNICAST | LPE_RXFILTER_MULTICAST;

	if (ifp->if_flags & IFF_ALLMULTI)
		rxfilt |= LPE_RXFILTER_MULTICAST;

	lpe_write_4(sc, LPE_RXFILTER_CTRL, rxfilt);
}

static void lpe_set_rxfilter(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;
	struct ifmultiaddr *ifma;
	int index;
	uint32_t hashl, hashh;

	hashl = 0;
	hashh = 0;

	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		index = ether_crc32_be(LLADDR((struct sockaddr_dl *)
		    ifma->ifma_addr), ETHER_ADDR_LEN) >> 23 & 0x3f;

		if (index > 31)
			hashh |= (1 << (index - 32));
		else
			hashl |= (1 << index);
	}
	if_maddr_runlock(ifp);

	/* Program new hash filter */
	lpe_write_4(sc, LPE_HASHFILTER_L, hashl);
	lpe_write_4(sc, LPE_HASHFILTER_H, hashh);
}

static void
lpe_intr(void *arg)
{
	struct lpe_softc *sc = (struct lpe_softc *)arg;
	uint32_t intstatus;

	debugf("status=0x%08x\n", lpe_read_4(sc, LPE_INTSTATUS));

	lpe_lock(sc);

	while ((intstatus = lpe_read_4(sc, LPE_INTSTATUS))) {
		if (intstatus & LPE_INT_RXDONE)
			lpe_rxintr(sc);

		if (intstatus & LPE_INT_TXDONE)
			lpe_txintr(sc);
	
		lpe_write_4(sc, LPE_INTCLEAR, 0xffff);
	}

	lpe_unlock(sc);
}

static void
lpe_rxintr(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;
	struct lpe_hwdesc *hwd;
	struct lpe_hwstatus *hws;
	struct lpe_rxdesc *rxd;
	struct mbuf *m;
	int prod, cons;

	for (;;) {
		prod = lpe_read_4(sc, LPE_RXDESC_PROD);
		cons = lpe_read_4(sc, LPE_RXDESC_CONS);
		
		if (prod == cons)
			break;

		rxd = &sc->lpe_cdata.lpe_rx_desc[cons];
		hwd = &sc->lpe_rdata.lpe_rx_ring[cons];
		hws = &sc->lpe_rdata.lpe_rx_status[cons];

		/* Check received frame for errors */
		if (hws->lhs_info & LPE_HWDESC_RXERRS) {
			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			lpe_discard_rxbuf(sc, cons);
			lpe_init_rxbuf(sc, cons);
			goto skip;
		}

		m = rxd->lpe_rxdesc_mbuf;
		m->m_pkthdr.rcvif = ifp;
		m->m_data += 2;

		if_inc_counter(ifp, IFCOUNTER_IPACKETS, 1);

		lpe_unlock(sc);
		(*ifp->if_input)(ifp, m);	
		lpe_lock(sc);

		lpe_init_rxbuf(sc, cons);
skip:
		LPE_INC(cons, LPE_RXDESC_NUM);
		lpe_write_4(sc, LPE_RXDESC_CONS, cons);
	}
}

static void
lpe_txintr(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;
	struct lpe_hwdesc *hwd;
	struct lpe_hwstatus *hws;
	struct lpe_txdesc *txd;
	int cons, last;

	for (;;) {
		cons = lpe_read_4(sc, LPE_TXDESC_CONS);
		last = sc->lpe_cdata.lpe_tx_last;
		
		if (cons == last)
			break;

		txd = &sc->lpe_cdata.lpe_tx_desc[last];
		hwd = &sc->lpe_rdata.lpe_tx_ring[last];
		hws = &sc->lpe_rdata.lpe_tx_status[last];

		bus_dmamap_sync(sc->lpe_cdata.lpe_tx_buf_tag,
		    txd->lpe_txdesc_dmamap, BUS_DMASYNC_POSTWRITE);

		if_inc_counter(ifp, IFCOUNTER_COLLISIONS, LPE_HWDESC_COLLISIONS(hws->lhs_info));

		if (hws->lhs_info & LPE_HWDESC_TXERRS)
			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
		else
			if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);

		if (txd->lpe_txdesc_first) {
			bus_dmamap_unload(sc->lpe_cdata.lpe_tx_buf_tag,
			    txd->lpe_txdesc_dmamap);	

			m_freem(txd->lpe_txdesc_mbuf);
			txd->lpe_txdesc_mbuf = NULL;
			txd->lpe_txdesc_first = 0;
		}

		sc->lpe_cdata.lpe_tx_used--;
		LPE_INC(sc->lpe_cdata.lpe_tx_last, LPE_TXDESC_NUM);
	}

	if (!sc->lpe_cdata.lpe_tx_used)
		sc->lpe_watchdog_timer = 0;
}

static void
lpe_tick(void *arg)
{
	struct lpe_softc *sc = (struct lpe_softc *)arg;
	struct mii_data *mii = device_get_softc(sc->lpe_miibus);

	lpe_lock_assert(sc);
	
	mii_tick(mii);
	lpe_watchdog(sc);

	callout_reset(&sc->lpe_tick, hz, lpe_tick, sc);
}

static void
lpe_watchdog(struct lpe_softc *sc)
{
	struct ifnet *ifp = sc->lpe_ifp;

	lpe_lock_assert(sc);

	if (sc->lpe_watchdog_timer == 0 || sc->lpe_watchdog_timer--)
		return;

	/* Chip has stopped responding */
	device_printf(sc->lpe_dev, "WARNING: chip hangup, restarting...\n");
	lpe_stop_locked(sc);
	lpe_init_locked(sc);

	/* Try to resend packets */
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		lpe_start_locked(ifp);
}

static int
lpe_dma_alloc(struct lpe_softc *sc)
{
	int err;

	/* Create parent DMA tag */
	err = bus_dma_tag_create(
	    bus_get_dma_tag(sc->lpe_dev),
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT, 0,	/* maxsize, nsegments */
	    BUS_SPACE_MAXSIZE_32BIT, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_parent_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create parent DMA tag\n");
		return (err);
	}

	err = lpe_dma_alloc_rx(sc);
	if (err)
		return (err);

	err = lpe_dma_alloc_tx(sc);
	if (err)
		return (err);

	return (0);
}

static int
lpe_dma_alloc_rx(struct lpe_softc *sc)
{
	struct lpe_rxdesc *rxd;
	struct lpe_dmamap_arg ctx;
	int err, i;

	/* Create tag for Rx ring */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    LPE_RXDESC_SIZE, 1,		/* maxsize, nsegments */
	    LPE_RXDESC_SIZE, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_rx_ring_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Rx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx status ring */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    LPE_RXSTATUS_SIZE, 1,	/* maxsize, nsegments */
	    LPE_RXSTATUS_SIZE, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_rx_status_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Rx status ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx buffers */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES * LPE_RXDESC_NUM,	/* maxsize */
	    LPE_RXDESC_NUM,		/* segments */
	    MCLBYTES, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_rx_buf_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Rx buffers DMA tag\n");
		goto fail;
	}

	/* Allocate Rx DMA ring */
	err = bus_dmamem_alloc(sc->lpe_cdata.lpe_rx_ring_tag,
	    (void **)&sc->lpe_rdata.lpe_rx_ring, BUS_DMA_WAITOK | BUS_DMA_COHERENT |
	    BUS_DMA_ZERO, &sc->lpe_cdata.lpe_rx_ring_map);

	err = bus_dmamap_load(sc->lpe_cdata.lpe_rx_ring_tag, 
	    sc->lpe_cdata.lpe_rx_ring_map, sc->lpe_rdata.lpe_rx_ring,
	    LPE_RXDESC_SIZE, lpe_dmamap_cb, &ctx, 0);

	sc->lpe_rdata.lpe_rx_ring_phys = ctx.lpe_dma_busaddr;

	/* Allocate Rx status ring */
	err = bus_dmamem_alloc(sc->lpe_cdata.lpe_rx_status_tag,
	    (void **)&sc->lpe_rdata.lpe_rx_status, BUS_DMA_WAITOK | BUS_DMA_COHERENT |
	    BUS_DMA_ZERO, &sc->lpe_cdata.lpe_rx_status_map);

	err = bus_dmamap_load(sc->lpe_cdata.lpe_rx_status_tag, 
	    sc->lpe_cdata.lpe_rx_status_map, sc->lpe_rdata.lpe_rx_status,
	    LPE_RXDESC_SIZE, lpe_dmamap_cb, &ctx, 0);

	sc->lpe_rdata.lpe_rx_status_phys = ctx.lpe_dma_busaddr;


	/* Create Rx buffers DMA map */
	for (i = 0; i < LPE_RXDESC_NUM; i++) {
		rxd = &sc->lpe_cdata.lpe_rx_desc[i];
		rxd->lpe_rxdesc_mbuf = NULL;
		rxd->lpe_rxdesc_dmamap = NULL;

		err = bus_dmamap_create(sc->lpe_cdata.lpe_rx_buf_tag, 0,
		    &rxd->lpe_rxdesc_dmamap);

		if (err) {
			device_printf(sc->lpe_dev, "cannot create Rx DMA map\n");
			return (err);
		}
	}

	return (0);
fail:
	return (err);
}

static int
lpe_dma_alloc_tx(struct lpe_softc *sc)
{
	struct lpe_txdesc *txd;
	struct lpe_dmamap_arg ctx;
	int err, i;

	/* Create tag for Tx ring */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    LPE_TXDESC_SIZE, 1,		/* maxsize, nsegments */
	    LPE_TXDESC_SIZE, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_tx_ring_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Tx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Tx status ring */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    LPE_TXSTATUS_SIZE, 1,	/* maxsize, nsegments */
	    LPE_TXSTATUS_SIZE, 0,	/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_tx_status_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Tx status ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Tx buffers */
	err = bus_dma_tag_create(
	    sc->lpe_cdata.lpe_parent_tag,
	    LPE_DESC_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES * LPE_TXDESC_NUM,	/* maxsize */
	    LPE_TXDESC_NUM,		/* segments */
	    MCLBYTES, 0,		/* maxsegsize, flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->lpe_cdata.lpe_tx_buf_tag);

	if (err) {
		device_printf(sc->lpe_dev, "cannot create Tx buffers DMA tag\n");
		goto fail;
	}

	/* Allocate Tx DMA ring */
	err = bus_dmamem_alloc(sc->lpe_cdata.lpe_tx_ring_tag,
	    (void **)&sc->lpe_rdata.lpe_tx_ring, BUS_DMA_WAITOK | BUS_DMA_COHERENT |
	    BUS_DMA_ZERO, &sc->lpe_cdata.lpe_tx_ring_map);

	err = bus_dmamap_load(sc->lpe_cdata.lpe_tx_ring_tag, 
	    sc->lpe_cdata.lpe_tx_ring_map, sc->lpe_rdata.lpe_tx_ring,
	    LPE_RXDESC_SIZE, lpe_dmamap_cb, &ctx, 0);

	sc->lpe_rdata.lpe_tx_ring_phys = ctx.lpe_dma_busaddr;

	/* Allocate Tx status ring */
	err = bus_dmamem_alloc(sc->lpe_cdata.lpe_tx_status_tag,
	    (void **)&sc->lpe_rdata.lpe_tx_status, BUS_DMA_WAITOK | BUS_DMA_COHERENT |
	    BUS_DMA_ZERO, &sc->lpe_cdata.lpe_tx_status_map);

	err = bus_dmamap_load(sc->lpe_cdata.lpe_tx_status_tag, 
	    sc->lpe_cdata.lpe_tx_status_map, sc->lpe_rdata.lpe_tx_status,
	    LPE_RXDESC_SIZE, lpe_dmamap_cb, &ctx, 0);

	sc->lpe_rdata.lpe_tx_status_phys = ctx.lpe_dma_busaddr;


	/* Create Tx buffers DMA map */
	for (i = 0; i < LPE_TXDESC_NUM; i++) {
		txd = &sc->lpe_cdata.lpe_tx_desc[i];
		txd->lpe_txdesc_mbuf = NULL;
		txd->lpe_txdesc_dmamap = NULL;
		txd->lpe_txdesc_first = 0;

		err = bus_dmamap_create(sc->lpe_cdata.lpe_tx_buf_tag, 0,
		    &txd->lpe_txdesc_dmamap);

		if (err) {
			device_printf(sc->lpe_dev, "cannot create Tx DMA map\n");
			return (err);
		}
	}

	return (0);
fail:
	return (err);
}

static int
lpe_init_rx(struct lpe_softc *sc)
{
	int i, err;

	for (i = 0; i < LPE_RXDESC_NUM; i++) {
		err = lpe_init_rxbuf(sc, i);
		if (err)
			return (err);
	}

	return (0);
}

static int
lpe_init_rxbuf(struct lpe_softc *sc, int n)
{
	struct lpe_rxdesc *rxd;
	struct lpe_hwdesc *hwd;
	struct lpe_hwstatus *hws;
	struct mbuf *m;
	bus_dma_segment_t segs[1];
	int nsegs;

	rxd = &sc->lpe_cdata.lpe_rx_desc[n];
	hwd = &sc->lpe_rdata.lpe_rx_ring[n];
	hws = &sc->lpe_rdata.lpe_rx_status[n];
	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);

	if (!m) {
		device_printf(sc->lpe_dev, "WARNING: mbufs exhausted!\n");
		return (ENOBUFS);
	}

	m->m_len = m->m_pkthdr.len = MCLBYTES;

	bus_dmamap_unload(sc->lpe_cdata.lpe_rx_buf_tag, rxd->lpe_rxdesc_dmamap);

	if (bus_dmamap_load_mbuf_sg(sc->lpe_cdata.lpe_rx_buf_tag, 
	    rxd->lpe_rxdesc_dmamap, m, segs, &nsegs, 0)) {
		m_freem(m);
		return (ENOBUFS);
	}

	bus_dmamap_sync(sc->lpe_cdata.lpe_rx_buf_tag, rxd->lpe_rxdesc_dmamap, 
	    BUS_DMASYNC_PREREAD);

	rxd->lpe_rxdesc_mbuf = m;
	hwd->lhr_data = segs[0].ds_addr + 2;
	hwd->lhr_control = (segs[0].ds_len - 1) | LPE_HWDESC_INTERRUPT;

	return (0);
}

static void
lpe_discard_rxbuf(struct lpe_softc *sc, int n)
{
	struct lpe_rxdesc *rxd;
	struct lpe_hwdesc *hwd;

	rxd = &sc->lpe_cdata.lpe_rx_desc[n];
	hwd = &sc->lpe_rdata.lpe_rx_ring[n];

	bus_dmamap_unload(sc->lpe_cdata.lpe_rx_buf_tag, rxd->lpe_rxdesc_dmamap);

	hwd->lhr_data = 0;
	hwd->lhr_control = 0;

	if (rxd->lpe_rxdesc_mbuf) {
		m_freem(rxd->lpe_rxdesc_mbuf); 
		rxd->lpe_rxdesc_mbuf = NULL;
	}
}

static void
lpe_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct lpe_dmamap_arg *ctx;

	if (error)
		return;

	ctx = (struct lpe_dmamap_arg *)arg;
	ctx->lpe_dma_busaddr = segs[0].ds_addr;
}

static int
lpe_ifmedia_upd(struct ifnet *ifp)
{
	return (0);
}

static void
lpe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct lpe_softc *sc = ifp->if_softc;
	struct mii_data *mii = device_get_softc(sc->lpe_miibus);

	lpe_lock(sc);
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
	lpe_unlock(sc);
}

static device_method_t lpe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		lpe_probe),
	DEVMETHOD(device_attach,	lpe_attach),
	DEVMETHOD(device_detach,	lpe_detach),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	lpe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	lpe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	lpe_miibus_statchg),
	{ 0, 0 }
};

static driver_t lpe_driver = {
	"lpe",
	lpe_methods,
	sizeof(struct lpe_softc),
};

static devclass_t lpe_devclass;

DRIVER_MODULE(lpe, simplebus, lpe_driver, lpe_devclass, 0, 0);
DRIVER_MODULE(miibus, lpe, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(lpe, obio, 1, 1, 1);
MODULE_DEPEND(lpe, miibus, 1, 1, 1);
MODULE_DEPEND(lpe, ether, 1, 1, 1);
