/*-
 * Copyright (C) 2006-2008 Semihalf
 * All rights reserved.
 *
 * Written by: Piotr Kruszynski <ppk@semihalf.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Freescale integrated Three-Speed Ethernet Controller (TSEC) driver.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>
#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <net/ethernet.h>
#include <net/if_arp.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/ocpbus.h>

#include <dev/tsec/if_tsec.h>
#include <dev/tsec/if_tsecreg.h>

#include "miibus_if.h"

#define TSEC_DEBUG

#ifdef TSEC_DEBUG
#define PDEBUG(a) {printf("%s:%d: ", __func__, __LINE__), printf a; printf("\n");}
#else
#define PDEBUG(a) /* nop */
#endif

static int	tsec_probe(device_t dev);
static int	tsec_attach(device_t dev);
static int	tsec_setup_intr(device_t dev, struct resource **ires,
    void **ihand, int *irid, driver_intr_t handler, const char *iname);
static void	tsec_release_intr(device_t dev, struct resource *ires,
    void *ihand, int irid, const char *iname);
static void	tsec_free_dma(struct tsec_softc *sc);
static int	tsec_detach(device_t dev);
static void	tsec_shutdown(device_t dev);
static int	tsec_suspend(device_t dev); /* XXX */
static int	tsec_resume(device_t dev); /* XXX */

static void	tsec_init(void *xsc);
static void	tsec_init_locked(struct tsec_softc *sc);
static void	tsec_set_mac_address(struct tsec_softc *sc);
static void	tsec_dma_ctl(struct tsec_softc *sc, int state);
static void	tsec_intrs_ctl(struct tsec_softc *sc, int state);
static void	tsec_reset_mac(struct tsec_softc *sc);

static void	tsec_watchdog(struct tsec_softc *sc);
static void	tsec_start(struct ifnet *ifp);
static void	tsec_start_locked(struct ifnet *ifp);
static int	tsec_encap(struct tsec_softc *sc,
    struct mbuf *m_head);
static void	tsec_setfilter(struct tsec_softc *sc);
static int	tsec_ioctl(struct ifnet *ifp, u_long command,
    caddr_t data);
static int	tsec_ifmedia_upd(struct ifnet *ifp);
static void	tsec_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
static int	tsec_new_rxbuf(bus_dma_tag_t tag, bus_dmamap_t map,
    struct mbuf **mbufp, uint32_t *paddr);
static void	tsec_map_dma_addr(void *arg, bus_dma_segment_t *segs,
    int nseg, int error);
static int	tsec_alloc_dma_desc(device_t dev, bus_dma_tag_t *dtag,
    bus_dmamap_t *dmap, bus_size_t dsize, void **vaddr, void *raddr,
    const char *dname);
static void	tsec_free_dma_desc(bus_dma_tag_t dtag, bus_dmamap_t dmap,
    void *vaddr);

static void	tsec_stop(struct tsec_softc *sc);

static void	tsec_receive_intr(void *arg);
static void	tsec_transmit_intr(void *arg);
static void	tsec_error_intr(void *arg);

static void	tsec_tick(void *arg);
static int	tsec_miibus_readreg(device_t dev, int phy, int reg);
static void	tsec_miibus_writereg(device_t dev, int phy, int reg, int value);
static void	tsec_miibus_statchg(device_t dev);

static struct tsec_softc *tsec0_sc = NULL; /* XXX ugly hack! */

static device_method_t tsec_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tsec_probe),
	DEVMETHOD(device_attach,	tsec_attach),
	DEVMETHOD(device_detach,	tsec_detach),
	DEVMETHOD(device_shutdown,	tsec_shutdown),
	DEVMETHOD(device_suspend,	tsec_suspend),
	DEVMETHOD(device_resume,	tsec_resume),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	tsec_miibus_readreg),
	DEVMETHOD(miibus_writereg,	tsec_miibus_writereg),
	DEVMETHOD(miibus_statchg,	tsec_miibus_statchg),
	{ 0, 0 }
};

static driver_t tsec_driver = {
	"tsec",
	tsec_methods,
	sizeof(struct tsec_softc),
};

static devclass_t tsec_devclass;

DRIVER_MODULE(tsec, ocpbus, tsec_driver, tsec_devclass, 0, 0);
DRIVER_MODULE(miibus, tsec, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(tsec, ether, 1, 1, 1);
MODULE_DEPEND(tsec, miibus, 1, 1, 1);

static void
tsec_get_hwaddr(struct tsec_softc *sc, uint8_t *addr)
{
	union {
		uint32_t reg[2];
		uint8_t addr[6];
	} curmac;
	uint32_t a[6];
	device_t parent;
	uintptr_t macaddr;
	int i;

	parent = device_get_parent(sc->dev);
	if (BUS_READ_IVAR(parent, sc->dev, OCPBUS_IVAR_MACADDR,
	    &macaddr) == 0) {
		bcopy((uint8_t *)macaddr, addr, 6);
		return;
	}

	/*
	 * Fall back -- use the currently programmed address in the hope that
	 * it was set be firmware...
	 */
	curmac.reg[0] = TSEC_READ(sc, TSEC_REG_MACSTNADDR1);
	curmac.reg[1] = TSEC_READ(sc, TSEC_REG_MACSTNADDR2);
	for (i = 0; i < 6; i++)
		a[5-i] = curmac.addr[i];

	addr[0] = a[0];
	addr[1] = a[1];
	addr[2] = a[2];
	addr[3] = a[3];
	addr[4] = a[4];
	addr[5] = a[5];
}

static void
tsec_init(void *xsc)
{
	struct tsec_softc *sc = xsc;

	TSEC_GLOBAL_LOCK(sc);
	tsec_init_locked(sc);
	TSEC_GLOBAL_UNLOCK(sc);
}

static void
tsec_init_locked(struct tsec_softc *sc)
{
	struct tsec_desc *tx_desc = sc->tsec_tx_vaddr;
	struct tsec_desc *rx_desc = sc->tsec_rx_vaddr;
	struct ifnet *ifp = sc->tsec_ifp;
	uint32_t timeout;
	uint32_t val;
	uint32_t i;

	TSEC_GLOBAL_LOCK_ASSERT(sc);
	tsec_stop(sc);

	/*
	 * These steps are according to the MPC8555E PowerQUICCIII RM:
	 * 14.7 Initialization/Application Information
	 */

	/* Step 1: soft reset MAC */
	tsec_reset_mac(sc);

	/* Step 2: Initialize MACCFG2 */
	TSEC_WRITE(sc, TSEC_REG_MACCFG2,
	    TSEC_MACCFG2_FULLDUPLEX |	/* Full Duplex = 1 */
	    TSEC_MACCFG2_PADCRC |	/* PAD/CRC append */
	    TSEC_MACCFG2_GMII |		/* I/F Mode bit */
	    TSEC_MACCFG2_PRECNT		/* Preamble count = 7 */
	);

	/* Step 3: Initialize ECNTRL
	 * While the documentation states that R100M is ignored if RPM is
	 * not set, it does seem to be needed to get the orange boxes to
	 * work (which have a Marvell 88E1111 PHY). Go figure.
	 */

	/*
	 * XXX kludge - use circumstancial evidence to program ECNTRL
	 * correctly. Ideally we need some board information to guide
	 * us here.
	 */
	i = TSEC_READ(sc, TSEC_REG_ID2);
	val = (i & 0xffff)
	    ? (TSEC_ECNTRL_TBIM | TSEC_ECNTRL_SGMIIM)	/* Sumatra */
	    : TSEC_ECNTRL_R100M;			/* Orange + CDS */
	TSEC_WRITE(sc, TSEC_REG_ECNTRL, TSEC_ECNTRL_STEN | val);

	/* Step 4: Initialize MAC station address */
	tsec_set_mac_address(sc);

	/*
	 * Step 5: Assign a Physical address to the TBI so as to not conflict
	 * with the external PHY physical address
	 */
	TSEC_WRITE(sc, TSEC_REG_TBIPA, 5);

	/* Step 6: Reset the management interface */
	TSEC_WRITE(tsec0_sc, TSEC_REG_MIIMCFG, TSEC_MIIMCFG_RESETMGMT);

	/* Step 7: Setup the MII Mgmt clock speed */
	TSEC_WRITE(tsec0_sc, TSEC_REG_MIIMCFG, TSEC_MIIMCFG_CLKDIV28);

	/* Step 8: Read MII Mgmt indicator register and check for Busy = 0 */
	timeout = TSEC_READ_RETRY;
	while (--timeout && (TSEC_READ(tsec0_sc, TSEC_REG_MIIMIND) &
	    TSEC_MIIMIND_BUSY))
		DELAY(TSEC_READ_DELAY);
	if (timeout == 0) {
		if_printf(ifp, "tsec_init_locked(): Mgmt busy timeout\n");
		return;
	}

	/* Step 9: Setup the MII Mgmt */
	mii_mediachg(sc->tsec_mii);

	/* Step 10: Clear IEVENT register */
	TSEC_WRITE(sc, TSEC_REG_IEVENT, 0xffffffff);

	/* Step 11: Initialize IMASK */
	tsec_intrs_ctl(sc, 1);

	/* Step 12: Initialize IADDRn */
	TSEC_WRITE(sc, TSEC_REG_IADDR0, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR1, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR2, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR3, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR4, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR5, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR6, 0);
	TSEC_WRITE(sc, TSEC_REG_IADDR7, 0);

	/* Step 13: Initialize GADDRn */
	TSEC_WRITE(sc, TSEC_REG_GADDR0, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR1, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR2, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR3, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR4, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR5, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR6, 0);
	TSEC_WRITE(sc, TSEC_REG_GADDR7, 0);

	/* Step 14: Initialize RCTRL */
	TSEC_WRITE(sc, TSEC_REG_RCTRL, 0);

	/* Step 15: Initialize DMACTRL */
	tsec_dma_ctl(sc, 1);

	/* Step 16: Initialize FIFO_PAUSE_CTRL */
	TSEC_WRITE(sc, TSEC_REG_FIFO_PAUSE_CTRL, TSEC_FIFO_PAUSE_CTRL_EN);

	/*
	 * Step 17: Initialize transmit/receive descriptor rings.
	 * Initialize TBASE and RBASE.
	 */
	TSEC_WRITE(sc, TSEC_REG_TBASE, sc->tsec_tx_raddr);
	TSEC_WRITE(sc, TSEC_REG_RBASE, sc->tsec_rx_raddr);

	for (i = 0; i < TSEC_TX_NUM_DESC; i++) {
		tx_desc[i].bufptr = 0;
		tx_desc[i].length = 0;
		tx_desc[i].flags = ((i == TSEC_TX_NUM_DESC-1) ? TSEC_TXBD_W : 0);
	}
	bus_dmamap_sync(sc->tsec_tx_dtag, sc->tsec_tx_dmap, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < TSEC_RX_NUM_DESC; i++) {
		rx_desc[i].bufptr = sc->rx_data[i].paddr;
		rx_desc[i].length = 0;
		rx_desc[i].flags = TSEC_RXBD_E | TSEC_RXBD_I |
		    ((i == TSEC_RX_NUM_DESC-1) ? TSEC_RXBD_W : 0);
	}
	bus_dmamap_sync(sc->tsec_rx_dtag, sc->tsec_rx_dmap, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	/* Step 18: Initialize the maximum and minimum receive buffer length */
	TSEC_WRITE(sc, TSEC_REG_MRBLR, TSEC_DEFAULT_MAX_RX_BUFFER_SIZE);
	TSEC_WRITE(sc, TSEC_REG_MINFLR, TSEC_DEFAULT_MIN_RX_BUFFER_SIZE);

	/* Step 19: Enable Rx and RxBD sdata snooping */
	TSEC_WRITE(sc, TSEC_REG_ATTR, TSEC_ATTR_RDSEN | TSEC_ATTR_RBDSEN);
	TSEC_WRITE(sc, TSEC_REG_ATTRELI, 0);

	/* Step 20: Reset collision counters in hardware */
	TSEC_WRITE(sc, TSEC_REG_MON_TSCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TMCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TLCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TXCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TNCL, 0);

	/* Step 21: Mask all CAM interrupts */
	TSEC_WRITE(sc, TSEC_REG_MON_CAM1, 0xffffffff);
	TSEC_WRITE(sc, TSEC_REG_MON_CAM2, 0xffffffff);

	/* Step 22: Enable Rx and Tx */
	val = TSEC_READ(sc, TSEC_REG_MACCFG1);
	val |= (TSEC_MACCFG1_RX_EN | TSEC_MACCFG1_TX_EN);
	TSEC_WRITE(sc, TSEC_REG_MACCFG1, val);

	/* Step 23: Reset TSEC counters for Tx and Rx rings */
	TSEC_TX_RX_COUNTERS_INIT(sc);

	/* Step 24: Activate network interface */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->tsec_if_flags = ifp->if_flags;
	sc->tsec_watchdog = 0;

	/* Schedule watchdog timeout */
	callout_reset(&sc->tsec_callout, hz, tsec_tick, sc);
}

static void
tsec_set_mac_address(struct tsec_softc *sc)
{
	uint32_t macbuf[2] = { 0, 0 };
	int i;
	char *macbufp;
	char *curmac;

	TSEC_GLOBAL_LOCK_ASSERT(sc);

	KASSERT((ETHER_ADDR_LEN <= sizeof(macbuf)),
	    ("tsec_set_mac_address: (%d <= %d",
	    ETHER_ADDR_LEN, sizeof(macbuf)));

	macbufp = (char *)macbuf;
	curmac = (char *)IF_LLADDR(sc->tsec_ifp);

	/* Correct order of MAC address bytes */
	for (i = 1; i <= ETHER_ADDR_LEN; i++)
		macbufp[ETHER_ADDR_LEN-i] = curmac[i-1];

	/* Initialize MAC station address MACSTNADDR2 and MACSTNADDR1 */
	TSEC_WRITE(sc, TSEC_REG_MACSTNADDR2, macbuf[1]);
	TSEC_WRITE(sc, TSEC_REG_MACSTNADDR1, macbuf[0]);
}

/*
 * DMA control function, if argument state is:
 * 0 - DMA engine will be disabled
 * 1 - DMA engine will be enabled
 */
static void
tsec_dma_ctl(struct tsec_softc *sc, int state)
{
	device_t dev;
	uint32_t dma_flags;
	uint32_t timeout;

	dev = sc->dev;

	dma_flags = TSEC_READ(sc, TSEC_REG_DMACTRL);

	switch (state) {
	case 0:
		/* Temporarily clear stop graceful stop bits. */
		tsec_dma_ctl(sc, 1000);

		/* Set it again */
		dma_flags |= (TSEC_DMACTRL_GRS | TSEC_DMACTRL_GTS);
		break;
	case 1000:
	case 1:
		/* Set write with response (WWR), wait (WOP) and snoop bits */
		dma_flags |= (TSEC_DMACTRL_TDSEN | TSEC_DMACTRL_TBDSEN |
		    DMACTRL_WWR | DMACTRL_WOP);

		/* Clear graceful stop bits */
		dma_flags &= ~(TSEC_DMACTRL_GRS | TSEC_DMACTRL_GTS);
		break;
	default:
		device_printf(dev, "tsec_dma_ctl(): unknown state value: %d\n",
		    state);
	}

	TSEC_WRITE(sc, TSEC_REG_DMACTRL, dma_flags);

	switch (state) {
	case 0:
		/* Wait for DMA stop */
		timeout = TSEC_READ_RETRY;
		while (--timeout && (!(TSEC_READ(sc, TSEC_REG_IEVENT) &
		    (TSEC_IEVENT_GRSC | TSEC_IEVENT_GTSC))))
			DELAY(TSEC_READ_DELAY);

		if (timeout == 0)
			device_printf(dev, "tsec_dma_ctl(): timeout!\n");
		break;
	case 1:
		/* Restart transmission function */
		TSEC_WRITE(sc, TSEC_REG_TSTAT, TSEC_TSTAT_THLT);
	}
}

/*
 * Interrupts control function, if argument state is:
 * 0 - all TSEC interrupts will be masked
 * 1 - all TSEC interrupts will be unmasked
 */
static void
tsec_intrs_ctl(struct tsec_softc *sc, int state)
{
	device_t dev;

	dev = sc->dev;

	switch (state) {
	case 0:
		TSEC_WRITE(sc, TSEC_REG_IMASK, 0);
		break;
	case 1:
		TSEC_WRITE(sc, TSEC_REG_IMASK, TSEC_IMASK_BREN |
		    TSEC_IMASK_RXCEN | TSEC_IMASK_BSYEN |
		    TSEC_IMASK_EBERREN | TSEC_IMASK_BTEN |
		    TSEC_IMASK_TXEEN | TSEC_IMASK_TXBEN |
		    TSEC_IMASK_TXFEN | TSEC_IMASK_XFUNEN |
		    TSEC_IMASK_RXFEN
		  );
		break;
	default:
		device_printf(dev, "tsec_intrs_ctl(): unknown state value: %d\n",
		    state);
	}
}

static void
tsec_reset_mac(struct tsec_softc *sc)
{
	uint32_t maccfg1_flags;

	/* Set soft reset bit */
	maccfg1_flags = TSEC_READ(sc, TSEC_REG_MACCFG1);
	maccfg1_flags |= TSEC_MACCFG1_SOFT_RESET;
	TSEC_WRITE(sc, TSEC_REG_MACCFG1, maccfg1_flags);

	/* Clear soft reset bit */
	maccfg1_flags = TSEC_READ(sc, TSEC_REG_MACCFG1);
	maccfg1_flags &= ~TSEC_MACCFG1_SOFT_RESET;
	TSEC_WRITE(sc, TSEC_REG_MACCFG1, maccfg1_flags);
}

static void
tsec_watchdog(struct tsec_softc *sc)
{
	struct ifnet *ifp;

	TSEC_GLOBAL_LOCK_ASSERT(sc);

	if (sc->tsec_watchdog == 0 || --sc->tsec_watchdog > 0)
		return;

	ifp = sc->tsec_ifp;
	ifp->if_oerrors++;
	if_printf(ifp, "watchdog timeout\n");

	tsec_stop(sc);
	tsec_init_locked(sc);
}

static void
tsec_start(struct ifnet *ifp)
{
	struct tsec_softc *sc = ifp->if_softc;

	TSEC_TRANSMIT_LOCK(sc);
	tsec_start_locked(ifp);
	TSEC_TRANSMIT_UNLOCK(sc);
}

static void
tsec_start_locked(struct ifnet *ifp)
{
	struct tsec_softc *sc;
	struct mbuf *m0;
	struct mbuf *mtmp;
	unsigned int queued = 0;

	sc = ifp->if_softc;

	TSEC_TRANSMIT_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	if (sc->tsec_link == 0)
		return;

	bus_dmamap_sync(sc->tsec_tx_dtag, sc->tsec_tx_dmap,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (;;) {
		/* Get packet from the queue */
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		mtmp = m_defrag(m0, M_DONTWAIT);
		if (mtmp)
			m0 = mtmp;

		if (tsec_encap(sc, m0)) {
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		queued++;
		BPF_MTAP(ifp, m0);
	}
	bus_dmamap_sync(sc->tsec_tx_dtag, sc->tsec_tx_dmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (queued) {
		/* Enable transmitter and watchdog timer */
		TSEC_WRITE(sc, TSEC_REG_TSTAT, TSEC_TSTAT_THLT);
		sc->tsec_watchdog = 5;
	}
}

static int
tsec_encap(struct tsec_softc *sc, struct mbuf *m0)
{
	struct tsec_desc *tx_desc = NULL;
	struct ifnet *ifp;
	bus_dma_segment_t segs[TSEC_TX_NUM_DESC];
	bus_dmamap_t *mapp;
	int error;
	int seg, nsegs;

	TSEC_TRANSMIT_LOCK_ASSERT(sc);

	ifp = sc->tsec_ifp;

	if (TSEC_FREE_TX_DESC(sc) == 0) {
		/* No free descriptors */
		return (-1);
	}

	/* Fetch unused map */
	mapp = TSEC_ALLOC_TX_MAP(sc);

	/* Create mapping in DMA memory */
	error = bus_dmamap_load_mbuf_sg(sc->tsec_tx_mtag,
	   *mapp, m0, segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0 || nsegs > TSEC_FREE_TX_DESC(sc) || nsegs <= 0) {
		bus_dmamap_unload(sc->tsec_tx_mtag, *mapp);
		TSEC_FREE_TX_MAP(sc, mapp);
		return ((error != 0) ? error : -1);
	}
	bus_dmamap_sync(sc->tsec_tx_mtag, *mapp, BUS_DMASYNC_PREWRITE);

	if ((ifp->if_flags & IFF_DEBUG) && (nsegs > 1))
		if_printf(ifp, "TX buffer has %d segments\n", nsegs);

	/* Everything is ok, now we can send buffers */
	for (seg = 0; seg < nsegs; seg++) {
		tx_desc = TSEC_GET_CUR_TX_DESC(sc);

		tx_desc->length = segs[seg].ds_len;
		tx_desc->bufptr = segs[seg].ds_addr;

		tx_desc->flags =
		    (tx_desc->flags & TSEC_TXBD_W) | /* wrap */
		    TSEC_TXBD_I |		/* interrupt */
		    TSEC_TXBD_R |		/* ready to send */
		    TSEC_TXBD_TC |		/* transmit the CRC sequence
						 * after the last data byte */
		    ((seg == nsegs-1) ? TSEC_TXBD_L : 0);/* last in frame */
	}

	/* Save mbuf and DMA mapping for release at later stage */
	TSEC_PUT_TX_MBUF(sc, m0);
	TSEC_PUT_TX_MAP(sc, mapp);

	return (0);
}

static void
tsec_setfilter(struct tsec_softc *sc)
{
	struct ifnet *ifp;
	uint32_t flags;

	ifp = sc->tsec_ifp;
	flags = TSEC_READ(sc, TSEC_REG_RCTRL);

	/* Promiscuous mode */
	if (ifp->if_flags & IFF_PROMISC)
		flags |= TSEC_RCTRL_PROM;
	else
		flags &= ~TSEC_RCTRL_PROM;

	TSEC_WRITE(sc, TSEC_REG_RCTRL, flags);
}

static int
tsec_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct tsec_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	device_t dev;
	int error = 0;

	dev = sc->dev;

	switch (command) {
	case SIOCSIFFLAGS:
		TSEC_GLOBAL_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((sc->tsec_if_flags ^ ifp->if_flags) &
				    IFF_PROMISC)
					tsec_setfilter(sc);
			} else
				tsec_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				tsec_stop(sc);
		}
		sc->tsec_if_flags = ifp->if_flags;
		TSEC_GLOBAL_UNLOCK(sc);
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->tsec_mii->mii_media,
		    command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
	}

	/* Flush buffers if not empty */
	if (ifp->if_flags & IFF_UP)
		tsec_start(ifp);
	return (error);
}

static int
tsec_ifmedia_upd(struct ifnet *ifp)
{
	struct tsec_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	TSEC_TRANSMIT_LOCK(sc);

	mii = sc->tsec_mii;
	mii_mediachg(mii);

	TSEC_TRANSMIT_UNLOCK(sc);
	return (0);
}

static void
tsec_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct tsec_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	TSEC_TRANSMIT_LOCK(sc);

	mii = sc->tsec_mii;
	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	TSEC_TRANSMIT_UNLOCK(sc);
}

static int
tsec_new_rxbuf(bus_dma_tag_t tag, bus_dmamap_t map, struct mbuf **mbufp,
	       uint32_t *paddr)
{
	struct mbuf *new_mbuf;
	bus_dma_segment_t seg[1];
	int error;
	int nsegs;

	KASSERT(mbufp != NULL, ("NULL mbuf pointer!"));

	new_mbuf = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (new_mbuf == NULL)
		return (ENOBUFS);
	new_mbuf->m_len = new_mbuf->m_pkthdr.len = new_mbuf->m_ext.ext_size;

	if (*mbufp) {
		bus_dmamap_sync(tag, map, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(tag, map);
	}

	error = bus_dmamap_load_mbuf_sg(tag, map, new_mbuf, seg, &nsegs,
	    BUS_DMA_NOWAIT);
	KASSERT(nsegs == 1, ("Too many segments returned!"));
	if (nsegs != 1 || error)
		panic("tsec_new_rxbuf(): nsegs(%d), error(%d)", nsegs, error);

#if 0
	if (error) {
		printf("tsec: bus_dmamap_load_mbuf_sg() returned: %d!\n",
			error);
		m_freem(new_mbuf);
		return (ENOBUFS);
	}
#endif

#if 0
	KASSERT(((seg->ds_addr) & (TSEC_RXBUFFER_ALIGNMENT-1)) == 0,
		("Wrong alignment of RX buffer!"));
#endif
	bus_dmamap_sync(tag, map, BUS_DMASYNC_PREREAD);

	(*mbufp) = new_mbuf;
	(*paddr) = seg->ds_addr;
	return (0);
}

static void
tsec_map_dma_addr(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	u_int32_t *paddr;

	KASSERT(nseg == 1, ("wrong number of segments, should be 1"));
	paddr = arg;
	*paddr = segs->ds_addr;
}

static int
tsec_alloc_dma_desc(device_t dev, bus_dma_tag_t *dtag, bus_dmamap_t *dmap,
    bus_size_t dsize, void **vaddr, void *raddr, const char *dname)
{
	int error;

	/* Allocate a busdma tag and DMA safe memory for TX/RX descriptors. */
	error = bus_dma_tag_create(NULL,	/* parent */
	    PAGE_SIZE, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    dsize, 1,				/* maxsize, nsegments */
	    dsize, 0,				/* maxsegsz, flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    dtag);				/* dmat */

	if (error) {
		device_printf(dev, "failed to allocate busdma %s tag\n", dname);
		(*vaddr) = NULL;
		return (ENXIO);
	}

	error = bus_dmamem_alloc(*dtag, vaddr, BUS_DMA_NOWAIT | BUS_DMA_ZERO,
	    dmap);
	if (error) {
		device_printf(dev, "failed to allocate %s DMA safe memory\n",
		    dname);
		bus_dma_tag_destroy(*dtag);
		(*vaddr) = NULL;
		return (ENXIO);
	}

	error = bus_dmamap_load(*dtag, *dmap, *vaddr, dsize, tsec_map_dma_addr,
	    raddr, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "cannot get address of the %s descriptors\n",
		    dname);
		bus_dmamem_free(*dtag, *vaddr, *dmap);
		bus_dma_tag_destroy(*dtag);
		(*vaddr) = NULL;
		return (ENXIO);
	}

	return (0);
}

static void
tsec_free_dma_desc(bus_dma_tag_t dtag, bus_dmamap_t dmap, void *vaddr)
{

	if (vaddr == NULL)
		return;

	/* Unmap descriptors from DMA memory */
	bus_dmamap_sync(dtag, dmap, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dtag, dmap);

	/* Free descriptors memory */
	bus_dmamem_free(dtag, vaddr, dmap);

	/* Destroy descriptors tag */
	bus_dma_tag_destroy(dtag);
}

static int
tsec_probe(device_t dev)
{
	struct tsec_softc *sc;
	device_t parent;
	uintptr_t devtype;
	int error;
	uint32_t id;

	parent = device_get_parent(dev);

	error = BUS_READ_IVAR(parent, dev, OCPBUS_IVAR_DEVTYPE, &devtype);
	if (error)
		return (error);
	if (devtype != OCPBUS_DEVTYPE_TSEC)
		return (ENXIO);

	sc = device_get_softc(dev);

	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->sc_rrid,
	    0ul, ~0ul, TSEC_IO_SIZE, RF_ACTIVE);
	if (sc->sc_rres == NULL)
		return (ENXIO);

	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	/* Check that we actually have a TSEC at this address */
	id = TSEC_READ(sc, TSEC_REG_ID) | TSEC_READ(sc, TSEC_REG_ID2);

	bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid, sc->sc_rres);

	if (id == 0)
		return (ENXIO);

	device_set_desc(dev, "Three-Speed Ethernet Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
tsec_attach(device_t dev)
{
	uint8_t hwaddr[ETHER_ADDR_LEN];
	struct tsec_softc *sc;
	struct ifnet *ifp;
	bus_dmamap_t *map_ptr;
	bus_dmamap_t **map_pptr;
	int error = 0;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (device_get_unit(dev) == 0)
		tsec0_sc = sc; /* XXX */

	callout_init(&sc->tsec_callout, 1);
	mtx_init(&sc->transmit_lock, device_get_nameunit(dev), "TSEC TX lock",
	    MTX_DEF);
	mtx_init(&sc->receive_lock, device_get_nameunit(dev), "TSEC RX lock",
	    MTX_DEF);

	/* Reset all TSEC counters */
	TSEC_TX_RX_COUNTERS_INIT(sc);

	/* Allocate IO memory for TSEC registers */
	sc->sc_rrid = 0;
	sc->sc_rres = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->sc_rrid,
	    0ul, ~0ul, TSEC_IO_SIZE, RF_ACTIVE);
	if (sc->sc_rres == NULL) {
		device_printf(dev, "could not allocate IO memory range!\n");
		tsec_detach(dev);
		return (ENXIO);
	}
	sc->sc_bas.bsh = rman_get_bushandle(sc->sc_rres);
	sc->sc_bas.bst = rman_get_bustag(sc->sc_rres);

	/* Stop DMA engine if enabled by firmware */
	tsec_dma_ctl(sc, 0);

	/* Reset MAC */
	tsec_reset_mac(sc);

	/* Disable interrupts for now */
	tsec_intrs_ctl(sc, 0);

	/* Allocate a busdma tag and DMA safe memory for TX descriptors. */
	error = tsec_alloc_dma_desc(dev, &sc->tsec_tx_dtag, &sc->tsec_tx_dmap,
	    sizeof(*sc->tsec_tx_vaddr) * TSEC_TX_NUM_DESC,
	    (void **)&sc->tsec_tx_vaddr, &sc->tsec_tx_raddr, "TX");
	if (error) {
		tsec_detach(dev);
		return (ENXIO);
	}

	/* Allocate a busdma tag and DMA safe memory for RX descriptors. */
	error = tsec_alloc_dma_desc(dev, &sc->tsec_rx_dtag, &sc->tsec_rx_dmap,
	    sizeof(*sc->tsec_rx_vaddr) * TSEC_RX_NUM_DESC,
	    (void **)&sc->tsec_rx_vaddr, &sc->tsec_rx_raddr, "RX");
	if (error) {
		tsec_detach(dev);
		return (ENXIO);
	}

	/* Allocate a busdma tag for TX mbufs. */
	error = bus_dma_tag_create(NULL,	/* parent */
	    TSEC_TXBUFFER_ALIGNMENT, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MCLBYTES * (TSEC_TX_NUM_DESC - 1),	/* maxsize */
	    TSEC_TX_NUM_DESC - 1,		/* nsegments */
	    MCLBYTES, 0,			/* maxsegsz, flags */
	    NULL, NULL,				/* lockfunc, lockfuncarg */
	    &sc->tsec_tx_mtag);			/* dmat */
	if (error) {
		device_printf(dev, "failed to allocate busdma tag(tx mbufs)\n");
		tsec_detach(dev);
		return (ENXIO);
	}

	/* Allocate a busdma tag for RX mbufs. */
	error = bus_dma_tag_create(NULL,	/* parent */
	    TSEC_RXBUFFER_ALIGNMENT, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filtfunc, filtfuncarg */
	    MCLBYTES,				/* maxsize */
	    1,					/* nsegments */
	    MCLBYTES, 0,				/* maxsegsz, flags */
	    NULL, NULL,			/* lockfunc, lockfuncarg */
	    &sc->tsec_rx_mtag);			/* dmat */
	if (error) {
		device_printf(dev, "failed to allocate busdma tag(rx mbufs)\n");
		tsec_detach(dev);
		return (ENXIO);
	}

	/* Create TX busdma maps */
	map_ptr = sc->tx_map_data;
	map_pptr = sc->tx_map_unused_data;

	for (i = 0; i < TSEC_TX_NUM_DESC; i++) {
		map_pptr[i] = &map_ptr[i];
		error = bus_dmamap_create(sc->tsec_tx_mtag, 0,
		    map_pptr[i]);
		if (error) {
			device_printf(dev, "failed to init TX ring\n");
			tsec_detach(dev);
			return (ENXIO);
		}
	}

	/* Create RX busdma maps and zero mbuf handlers */
	for (i = 0; i < TSEC_RX_NUM_DESC; i++) {
		error = bus_dmamap_create(sc->tsec_rx_mtag, 0,
		    &sc->rx_data[i].map);
		if (error) {
			device_printf(dev, "failed to init RX ring\n");
			tsec_detach(dev);
			return (ENXIO);
		}
		sc->rx_data[i].mbuf = NULL;
	}

	/* Create mbufs for RX buffers */
	for (i = 0; i < TSEC_RX_NUM_DESC; i++) {
		error = tsec_new_rxbuf(sc->tsec_rx_mtag, sc->rx_data[i].map,
		    &sc->rx_data[i].mbuf, &sc->rx_data[i].paddr);
		if (error) {
			device_printf(dev, "can't load rx DMA map %d, error = "
			    "%d\n", i, error);
			tsec_detach(dev);
			return (error);
		}
	}

	/* Create network interface for upper layers */
	ifp = sc->tsec_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "if_alloc() failed\n");
		tsec_detach(dev);
		return (ENOMEM);
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST;
	ifp->if_init = tsec_init;
	ifp->if_start = tsec_start;
	ifp->if_ioctl = tsec_ioctl;

	IFQ_SET_MAXLEN(&ifp->if_snd, TSEC_TX_NUM_DESC - 1);
	ifp->if_snd.ifq_drv_maxlen = TSEC_TX_NUM_DESC - 1;
	IFQ_SET_READY(&ifp->if_snd);

	/* XXX No special features of TSEC are supported currently */
	ifp->if_capabilities = 0;
	ifp->if_capenable = ifp->if_capabilities;

	/* Probe PHY(s) */
	error = mii_phy_probe(dev, &sc->tsec_miibus, tsec_ifmedia_upd,
	    tsec_ifmedia_sts);
	if (error) {
		device_printf(dev, "MII failed to find PHY!\n");
		if_free(ifp);
		sc->tsec_ifp = NULL;
		tsec_detach(dev);
		return (error);
	}
	sc->tsec_mii = device_get_softc(sc->tsec_miibus);

	tsec_get_hwaddr(sc, hwaddr);
	ether_ifattach(ifp, hwaddr);

	/* Interrupts configuration (TX/RX/ERR) */
	sc->sc_transmit_irid = OCP_TSEC_RID_TXIRQ;
	error = tsec_setup_intr(dev, &sc->sc_transmit_ires,
	    &sc->sc_transmit_ihand, &sc->sc_transmit_irid,
	    tsec_transmit_intr, "TX");
	if (error) {
		tsec_detach(dev);
		return (error);
	}

	sc->sc_receive_irid = OCP_TSEC_RID_RXIRQ;
	error = tsec_setup_intr(dev, &sc->sc_receive_ires,
	    &sc->sc_receive_ihand, &sc->sc_receive_irid,
	    tsec_receive_intr, "RX");
	if (error) {
		tsec_detach(dev);
		return (error);
	}

	sc->sc_error_irid = OCP_TSEC_RID_ERRIRQ;
	error = tsec_setup_intr(dev, &sc->sc_error_ires,
	    &sc->sc_error_ihand, &sc->sc_error_irid,
	    tsec_error_intr, "ERR");
	if (error) {
		tsec_detach(dev);
		return (error);
	}

	return (0);
}

static int
tsec_setup_intr(device_t dev, struct resource **ires, void **ihand, int *irid,
    driver_intr_t handler, const char *iname)
{
	struct tsec_softc *sc;
	int error;

	sc = device_get_softc(dev);

	(*ires) = bus_alloc_resource_any(dev, SYS_RES_IRQ, irid, RF_ACTIVE);
	if ((*ires) == NULL) {
		device_printf(dev, "could not allocate %s IRQ\n", iname);
		return (ENXIO);
	}
	error = bus_setup_intr(dev, *ires, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, handler, sc, ihand);
	if (error) {
		device_printf(dev, "failed to set up %s IRQ\n", iname);
		if (bus_release_resource(dev, SYS_RES_IRQ, *irid, *ires))
			device_printf(dev, "could not release %s IRQ\n", iname);
		(*ires) = NULL;
		return (error);
	}
	return (0);
}

static void
tsec_release_intr(device_t dev, struct resource *ires, void *ihand, int irid,
    const char *iname)
{
	int error;

	if (ires == NULL)
		return;

	error = bus_teardown_intr(dev, ires, ihand);
	if (error)
		device_printf(dev, "bus_teardown_intr() failed for %s intr"
		    ", error %d\n", iname, error);

	error = bus_release_resource(dev, SYS_RES_IRQ, irid, ires);
	if (error)
		device_printf(dev, "bus_release_resource() failed for %s intr"
		    ", error %d\n", iname, error);
}

static void
tsec_free_dma(struct tsec_softc *sc)
{
	int i;

	/* Free TX maps */
	for (i = 0; i < TSEC_TX_NUM_DESC; i++)
		if (sc->tx_map_data[i] != NULL)
			bus_dmamap_destroy(sc->tsec_tx_mtag,
			    sc->tx_map_data[i]);
	/* Destroy tag for Tx mbufs */
	bus_dma_tag_destroy(sc->tsec_tx_mtag);

	/* Free RX mbufs and maps */
	for (i = 0; i < TSEC_RX_NUM_DESC; i++) {
		if (sc->rx_data[i].mbuf) {
			/* Unload buffer from DMA */
			bus_dmamap_sync(sc->tsec_rx_mtag, sc->rx_data[i].map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->tsec_rx_mtag, sc->rx_data[i].map);

			/* Free buffer */
			m_freem(sc->rx_data[i].mbuf);
		}
		/* Destroy map for this buffer */
		if (sc->rx_data[i].map != NULL)
			bus_dmamap_destroy(sc->tsec_rx_mtag,
			    sc->rx_data[i].map);
	}
	/* Destroy tag for Rx mbufs */
	bus_dma_tag_destroy(sc->tsec_rx_mtag);

	/* Unload TX/RX descriptors */
	tsec_free_dma_desc(sc->tsec_tx_dtag, sc->tsec_tx_dmap,
	    sc->tsec_tx_vaddr);
	tsec_free_dma_desc(sc->tsec_rx_dtag, sc->tsec_rx_dmap,
	    sc->tsec_rx_vaddr);
}

static int
tsec_detach(device_t dev)
{
	struct tsec_softc *sc;
	int error;

	sc = device_get_softc(dev);

	/* Stop TSEC controller and free TX queue */
	if (sc->sc_rres && sc->tsec_ifp)
		tsec_shutdown(dev);

	/* Wait for stopping TSEC ticks */
	callout_drain(&sc->tsec_callout);

	/* Stop and release all interrupts */
	tsec_release_intr(dev, sc->sc_transmit_ires, sc->sc_transmit_ihand,
	    sc->sc_transmit_irid, "TX");
	tsec_release_intr(dev, sc->sc_receive_ires, sc->sc_receive_ihand,
	    sc->sc_receive_irid, "RX");
	tsec_release_intr(dev, sc->sc_error_ires, sc->sc_error_ihand,
	    sc->sc_error_irid, "ERR");

	/* Detach network interface */
	if (sc->tsec_ifp) {
		ether_ifdetach(sc->tsec_ifp);
		if_free(sc->tsec_ifp);
		sc->tsec_ifp = NULL;
	}

	/* Free DMA resources */
	tsec_free_dma(sc);

	/* Free IO memory handler */
	if (sc->sc_rres) {
		error = bus_release_resource(dev, SYS_RES_MEMORY, sc->sc_rrid,
		    sc->sc_rres);
		if (error)
			device_printf(dev, "bus_release_resource() failed for"
			    " IO memory, error %d\n", error);
	}

	/* Destroy locks */
	mtx_destroy(&sc->receive_lock);
	mtx_destroy(&sc->transmit_lock);
	return (0);
}

static void
tsec_shutdown(device_t dev)
{
	struct tsec_softc *sc;

	sc = device_get_softc(dev);

	TSEC_GLOBAL_LOCK(sc);
	tsec_stop(sc);
	TSEC_GLOBAL_UNLOCK(sc);
}

static int
tsec_suspend(device_t dev)
{

	/* TODO not implemented! */
	return (ENODEV);
}

static int
tsec_resume(device_t dev)
{

	/* TODO not implemented! */
	return (ENODEV);
}

static void
tsec_stop(struct tsec_softc *sc)
{
	struct ifnet *ifp;
	struct mbuf *m0;
	bus_dmamap_t *mapp;
	uint32_t tmpval;

	TSEC_GLOBAL_LOCK_ASSERT(sc);

	ifp = sc->tsec_ifp;

	/* Stop tick engine */
	callout_stop(&sc->tsec_callout);

	/* Disable interface and watchdog timer */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->tsec_watchdog = 0;

	/* Disable all interrupts and stop DMA */
	tsec_intrs_ctl(sc, 0);
	tsec_dma_ctl(sc, 0);

	/* Remove pending data from TX queue */
	while (!TSEC_EMPTYQ_TX_MBUF(sc)) {
		m0 = TSEC_GET_TX_MBUF(sc);
		mapp = TSEC_GET_TX_MAP(sc);

		bus_dmamap_sync(sc->tsec_tx_mtag, *mapp, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->tsec_tx_mtag, *mapp);

		TSEC_FREE_TX_MAP(sc, mapp);
		m_freem(m0);
	}

	/* Disable Rx and Tx */
	tmpval = TSEC_READ(sc, TSEC_REG_MACCFG1);
	tmpval &= ~(TSEC_MACCFG1_RX_EN | TSEC_MACCFG1_TX_EN);
	TSEC_WRITE(sc, TSEC_REG_MACCFG1, tmpval);
	DELAY(10);
}

static void
tsec_receive_intr(void *arg)
{
	struct mbuf *rcv_mbufs[TSEC_RX_NUM_DESC];
	struct tsec_softc *sc = arg;
	struct tsec_desc *rx_desc;
	struct ifnet *ifp;
	struct rx_data_type *rx_data;
	struct mbuf *m;
	device_t dev;
	uint32_t i;
	int count;
	int c1 = 0;
	int c2;
	uint16_t flags;
	uint16_t length;

	ifp = sc->tsec_ifp;
	rx_data = sc->rx_data;
	dev = sc->dev;

	/* Confirm the interrupt was received by driver */
	TSEC_WRITE(sc, TSEC_REG_IEVENT, TSEC_IEVENT_RXB | TSEC_IEVENT_RXF);

	TSEC_RECEIVE_LOCK(sc);

	bus_dmamap_sync(sc->tsec_rx_dtag, sc->tsec_rx_dmap, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	for (count = 0; /* count < TSEC_RX_NUM_DESC */; count++) {
		rx_desc = TSEC_GET_CUR_RX_DESC(sc);
		flags = rx_desc->flags;

		/* Check if there is anything to receive */
		if ((flags & TSEC_RXBD_E) || (count >= TSEC_RX_NUM_DESC)) {
			/*
			 * Avoid generating another interrupt
			 */
			if (flags & TSEC_RXBD_E)
				TSEC_WRITE(sc, TSEC_REG_IEVENT,
				    TSEC_IEVENT_RXB | TSEC_IEVENT_RXF);
			/*
			 * We didn't consume current descriptor and have to
			 * return it to the queue
			 */
			TSEC_BACK_CUR_RX_DESC(sc);
			break;
		}

		if (flags & (TSEC_RXBD_LG | TSEC_RXBD_SH | TSEC_RXBD_NO |
		    TSEC_RXBD_CR | TSEC_RXBD_OV | TSEC_RXBD_TR)) {
			rx_desc->length = 0;
			rx_desc->flags = (rx_desc->flags &
			    ~TSEC_RXBD_ZEROONINIT) | TSEC_RXBD_E | TSEC_RXBD_I;
			continue;
		}

		if ((flags & TSEC_RXBD_L) == 0)
			device_printf(dev, "buf is not the last in frame!\n");

		/* Ok... process frame */
		length = rx_desc->length - ETHER_CRC_LEN;
		i = TSEC_GET_CUR_RX_DESC_CNT(sc);

		m = rx_data[i].mbuf;

		if (tsec_new_rxbuf(sc->tsec_rx_mtag, rx_data[i].map,
		    &rx_data[i].mbuf, &rx_data[i].paddr)) {
			ifp->if_ierrors++;
			continue;
		}
		/* Attach new buffer to descriptor, and clear flags */
		rx_desc->bufptr = rx_data[i].paddr;
		rx_desc->length = 0;
		rx_desc->flags = (rx_desc->flags & ~TSEC_RXBD_ZEROONINIT) |
		    TSEC_RXBD_E | TSEC_RXBD_I;

		/* Prepare buffer for upper layers */
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = length;

		/* Save it for push */
		rcv_mbufs[c1++] = m;
	}

	bus_dmamap_sync(sc->tsec_rx_dtag, sc->tsec_rx_dmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	TSEC_RECEIVE_UNLOCK(sc);

	/* Push it now */
	for (c2 = 0; c2 < c1; c2++)
		(*ifp->if_input)(ifp, rcv_mbufs[c2]);
}

static void
tsec_transmit_intr(void *arg)
{
	struct tsec_softc *sc = arg;
	struct tsec_desc *tx_desc;
	struct ifnet *ifp;
	struct mbuf *m0;
	bus_dmamap_t *mapp;
	int send = 0;

	ifp = sc->tsec_ifp;

	/* Confirm the interrupt was received by driver */
	TSEC_WRITE(sc, TSEC_REG_IEVENT, TSEC_IEVENT_TXB | TSEC_IEVENT_TXF);

	TSEC_TRANSMIT_LOCK(sc);

	/* Update collision statistics */
	ifp->if_collisions += TSEC_READ(sc, TSEC_REG_MON_TNCL);

	/* Reset collision counters in hardware */
	TSEC_WRITE(sc, TSEC_REG_MON_TSCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TMCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TLCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TXCL, 0);
	TSEC_WRITE(sc, TSEC_REG_MON_TNCL, 0);

	bus_dmamap_sync(sc->tsec_tx_dtag, sc->tsec_tx_dmap, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	while (TSEC_CUR_DIFF_DIRTY_TX_DESC(sc)) {
		tx_desc = TSEC_GET_DIRTY_TX_DESC(sc);
		if (tx_desc->flags & TSEC_TXBD_R) {
			TSEC_BACK_DIRTY_TX_DESC(sc);
			break;
		}

		if ((tx_desc->flags & TSEC_TXBD_L) == 0)
			continue;

		/*
		 * This is the last buf in this packet, so unmap and free it.
		 */
		m0 = TSEC_GET_TX_MBUF(sc);
		mapp = TSEC_GET_TX_MAP(sc);

		bus_dmamap_sync(sc->tsec_tx_mtag, *mapp, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->tsec_tx_mtag, *mapp);

		TSEC_FREE_TX_MAP(sc, mapp);
		m_freem(m0);

		ifp->if_opackets++;
		send = 1;
	}
	bus_dmamap_sync(sc->tsec_tx_dtag, sc->tsec_tx_dmap,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	if (send) {
		/* Now send anything that was pending */
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		tsec_start_locked(ifp);

		/* Stop watchdog if all sent */
		if (TSEC_EMPTYQ_TX_MBUF(sc))
			sc->tsec_watchdog = 0;
	}
	TSEC_TRANSMIT_UNLOCK(sc);
}

static void
tsec_error_intr(void *arg)
{
	struct tsec_softc *sc = arg;
	struct ifnet *ifp;
	uint32_t eflags;

	ifp = sc->tsec_ifp;

	eflags = TSEC_READ(sc, TSEC_REG_IEVENT);

	if (ifp->if_flags & IFF_DEBUG)
		if_printf(ifp, "tsec_error_intr(): event flags: 0x%x\n", eflags);

	/* Clear events bits in hardware */
	TSEC_WRITE(sc, TSEC_REG_IEVENT, TSEC_IEVENT_RXC | TSEC_IEVENT_BSY |
	    TSEC_IEVENT_EBERR | TSEC_IEVENT_MSRO | TSEC_IEVENT_BABT |
	    TSEC_IEVENT_TXC | TSEC_IEVENT_TXE | TSEC_IEVENT_LC |
	    TSEC_IEVENT_CRL | TSEC_IEVENT_XFUN);

	if (eflags & TSEC_IEVENT_EBERR)
		if_printf(ifp, "System bus error occurred during"
		    " a DMA transaction (flags: 0x%x)\n", eflags);

	/* Check transmitter errors */
	if (eflags & TSEC_IEVENT_TXE) {
		ifp->if_oerrors++;

		if (eflags & TSEC_IEVENT_LC)
			ifp->if_collisions++;

		TSEC_WRITE(sc, TSEC_REG_TSTAT, TSEC_TSTAT_THLT);
	}
	if (eflags & TSEC_IEVENT_BABT)
		ifp->if_oerrors++;

	/* Check receiver errors */
	if (eflags & TSEC_IEVENT_BSY) {
		ifp->if_ierrors++;
		ifp->if_iqdrops++;

		/* Get data from RX buffers */
		tsec_receive_intr(arg);

		/* Make receiver again active */
		TSEC_WRITE(sc, TSEC_REG_RSTAT, TSEC_RSTAT_QHLT);
	}
	if (eflags & TSEC_IEVENT_BABR)
		ifp->if_ierrors++;
}

static void
tsec_tick(void *xsc)
{
	struct tsec_softc *sc = xsc;
	struct ifnet *ifp;
	int link;

	TSEC_GLOBAL_LOCK(sc);

	tsec_watchdog(sc);

	ifp = sc->tsec_ifp;
	link = sc->tsec_link;

	mii_tick(sc->tsec_mii);

	if (link == 0 && sc->tsec_link == 1 && (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)))
		tsec_start_locked(ifp);

	/* Schedule another timeout one second from now. */
	callout_reset(&sc->tsec_callout, hz, tsec_tick, sc);

	TSEC_GLOBAL_UNLOCK(sc);
}

static int
tsec_miibus_readreg(device_t dev, int phy, int reg)
{
	struct tsec_softc *sc;
	uint32_t timeout;

	sc = device_get_softc(dev);

	if (device_get_unit(dev) != phy)
		return (0);

	sc = tsec0_sc;

	TSEC_WRITE(sc, TSEC_REG_MIIMADD, (phy << 8) | reg);
	TSEC_WRITE(sc, TSEC_REG_MIIMCOM, 0);
	TSEC_WRITE(sc, TSEC_REG_MIIMCOM, TSEC_MIIMCOM_READCYCLE);

	timeout = TSEC_READ_RETRY;
	while (--timeout && TSEC_READ(sc, TSEC_REG_MIIMIND) &
	    (TSEC_MIIMIND_NOTVALID | TSEC_MIIMIND_BUSY))
		DELAY(TSEC_READ_DELAY);

	if (timeout == 0)
		device_printf(dev, "Timeout while reading from PHY!\n");

	return (TSEC_READ(sc, TSEC_REG_MIIMSTAT));
}

static void
tsec_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct tsec_softc *sc;
	uint32_t timeout;

	sc = device_get_softc(dev);

	if (device_get_unit(dev) != phy)
		device_printf(dev, "Trying to write to an alien PHY(%d)\n", phy);

	sc = tsec0_sc;

	TSEC_WRITE(sc, TSEC_REG_MIIMADD, (phy << 8) | reg);
	TSEC_WRITE(sc, TSEC_REG_MIIMCON, value);

	timeout = TSEC_READ_RETRY;
	while (--timeout && (TSEC_READ(sc, TSEC_REG_MIIMIND) & TSEC_MIIMIND_BUSY))
		DELAY(TSEC_READ_DELAY);

	if (timeout == 0)
		device_printf(dev, "Timeout while writing to PHY!\n");
}

static void
tsec_miibus_statchg(device_t dev)
{
	struct tsec_softc *sc;
	struct mii_data *mii;
	uint32_t ecntrl, id, tmp;
	int link;

	sc = device_get_softc(dev);
	mii = sc->tsec_mii;
	link = ((mii->mii_media_status & IFM_ACTIVE) ? 1 : 0);

	tmp = TSEC_READ(sc, TSEC_REG_MACCFG2) & ~TSEC_MACCFG2_IF;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		tmp |= TSEC_MACCFG2_FULLDUPLEX;
	else
		tmp &= ~TSEC_MACCFG2_FULLDUPLEX;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
	case IFM_1000_SX:
		tmp |= TSEC_MACCFG2_GMII;
		sc->tsec_link = link;
		break;
	case IFM_100_TX:
	case IFM_10_T:
		tmp |= TSEC_MACCFG2_MII;
		sc->tsec_link = link;
		break;
	case IFM_NONE:
		if (link)
			device_printf(dev, "No speed selected but link active!\n");
		sc->tsec_link = 0;
		return;
	default:
		sc->tsec_link = 0;
		device_printf(dev, "Unknown speed (%d), link %s!\n",
		    IFM_SUBTYPE(mii->mii_media_active),
		    ((link) ? "up" : "down"));
		return;
	}
	TSEC_WRITE(sc, TSEC_REG_MACCFG2, tmp);

	/* XXX kludge - use circumstantial evidence for reduced mode. */
	id = TSEC_READ(sc, TSEC_REG_ID2);
	if (id & 0xffff) {
		ecntrl = TSEC_READ(sc, TSEC_REG_ECNTRL) & ~TSEC_ECNTRL_R100M;
		ecntrl |= (tmp & TSEC_MACCFG2_MII) ? TSEC_ECNTRL_R100M : 0;
		TSEC_WRITE(sc, TSEC_REG_ECNTRL, ecntrl);
	}
}
