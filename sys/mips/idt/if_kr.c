/*-
 * Copyright (C) 2007 
 *	Oleksandr Tymoshenko <gonzo@freebsd.org>. All rights reserved.
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $Id: $
 * 
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * RC32434 Ethernet interface driver
 */
#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

MODULE_DEPEND(kr, ether, 1, 1, 1);
MODULE_DEPEND(kr, miibus, 1, 1, 1);

#include "miibus_if.h"

#include <mips/idt/if_krreg.h>

#define KR_DEBUG

static int kr_attach(device_t);
static int kr_detach(device_t);
static int kr_ifmedia_upd(struct ifnet *);
static void kr_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static int kr_ioctl(struct ifnet *, u_long, caddr_t);
static void kr_init(void *);
static void kr_init_locked(struct kr_softc *);
static void kr_link_task(void *, int);
static int kr_miibus_readreg(device_t, int, int);
static void kr_miibus_statchg(device_t);
static int kr_miibus_writereg(device_t, int, int, int);
static int kr_probe(device_t);
static void kr_reset(struct kr_softc *);
static int kr_resume(device_t);
static int kr_rx_ring_init(struct kr_softc *);
static int kr_tx_ring_init(struct kr_softc *);
static int kr_shutdown(device_t);
static void kr_start(struct ifnet *);
static void kr_start_locked(struct ifnet *);
static void kr_stop(struct kr_softc *);
static int kr_suspend(device_t);

static void kr_rx(struct kr_softc *);
static void kr_tx(struct kr_softc *);
static void kr_rx_intr(void *);
static void kr_tx_intr(void *);
static void kr_rx_und_intr(void *);
static void kr_tx_ovr_intr(void *);
static void kr_tick(void *);

static void kr_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static int kr_dma_alloc(struct kr_softc *);
static void kr_dma_free(struct kr_softc *);
static int kr_newbuf(struct kr_softc *, int);
static __inline void kr_fixup_rx(struct mbuf *);

static device_method_t kr_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		kr_probe),
	DEVMETHOD(device_attach,	kr_attach),
	DEVMETHOD(device_detach,	kr_detach),
	DEVMETHOD(device_suspend,	kr_suspend),
	DEVMETHOD(device_resume,	kr_resume),
	DEVMETHOD(device_shutdown,	kr_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	kr_miibus_readreg),
	DEVMETHOD(miibus_writereg,	kr_miibus_writereg),
	DEVMETHOD(miibus_statchg,	kr_miibus_statchg),

	{ 0, 0 }
};

static driver_t kr_driver = {
	"kr",
	kr_methods,
	sizeof(struct kr_softc)
};

static devclass_t kr_devclass;

DRIVER_MODULE(kr, obio, kr_driver, kr_devclass, 0, 0);
DRIVER_MODULE(miibus, kr, miibus_driver, miibus_devclass, 0, 0);

static int 
kr_probe(device_t dev)
{

	device_set_desc(dev, "RC32434 Ethernet interface");
	return (0);
}

static int
kr_attach(device_t dev)
{
	uint8_t			eaddr[ETHER_ADDR_LEN];
	struct ifnet		*ifp;
	struct kr_softc		*sc;
	int			error = 0, rid;
	int			unit;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->kr_dev = dev;

	mtx_init(&sc->kr_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	callout_init_mtx(&sc->kr_stat_callout, &sc->kr_mtx, 0);
	TASK_INIT(&sc->kr_link_task, 0, kr_link_task, sc);
	pci_enable_busmaster(dev);

	/* Map control/status registers. */
	sc->kr_rid = 0;
	sc->kr_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sc->kr_rid, 
	    RF_ACTIVE);

	if (sc->kr_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->kr_btag = rman_get_bustag(sc->kr_res);
	sc->kr_bhandle = rman_get_bushandle(sc->kr_res);

	/* Allocate interrupts */
	rid = 0;
	sc->kr_rx_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, KR_RX_IRQ,
	    KR_RX_IRQ, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->kr_rx_irq == NULL) {
		device_printf(dev, "couldn't map rx interrupt\n");
		error = ENXIO;
		goto fail;
	}

	rid = 0;
	sc->kr_tx_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, KR_TX_IRQ,
	    KR_TX_IRQ, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->kr_tx_irq == NULL) {
		device_printf(dev, "couldn't map tx interrupt\n");
		error = ENXIO;
		goto fail;
	}

	rid = 0;
	sc->kr_rx_und_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 
	    KR_RX_UND_IRQ, KR_RX_UND_IRQ, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->kr_rx_und_irq == NULL) {
		device_printf(dev, "couldn't map rx underrun interrupt\n");
		error = ENXIO;
		goto fail;
	}

	rid = 0;
	sc->kr_tx_ovr_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 
	    KR_TX_OVR_IRQ, KR_TX_OVR_IRQ, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->kr_tx_ovr_irq == NULL) {
		device_printf(dev, "couldn't map tx overrun interrupt\n");
		error = ENXIO;
		goto fail;
	}

	/* Allocate ifnet structure. */
	ifp = sc->kr_ifp = if_alloc(IFT_ETHER);

	if (ifp == NULL) {
		device_printf(dev, "couldn't allocate ifnet structure\n");
		error = ENOSPC;
		goto fail;
	}
	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = kr_ioctl;
	ifp->if_start = kr_start;
	ifp->if_init = kr_init;

	/* XXX: add real size */
	IFQ_SET_MAXLEN(&ifp->if_snd, 9);
	ifp->if_snd.ifq_maxlen = 9;
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capenable = ifp->if_capabilities;

	eaddr[0] = 0x00;
	eaddr[1] = 0x0C;
	eaddr[2] = 0x42;
	eaddr[3] = 0x09;
	eaddr[4] = 0x5E;
	eaddr[5] = 0x6B;

	if (kr_dma_alloc(sc) != 0) {
		error = ENXIO;
		goto fail;
	}

	/* TODO: calculate prescale */
	CSR_WRITE_4(sc, KR_ETHMCP, (165000000 / (1250000 + 1)) & ~1);

	CSR_WRITE_4(sc, KR_MIIMCFG, KR_MIIMCFG_R);
	DELAY(1000);
	CSR_WRITE_4(sc, KR_MIIMCFG, 0);

	/* Do MII setup. */
	error = mii_attach(dev, &sc->kr_miibus, ifp, kr_ifmedia_upd,
	    kr_ifmedia_sts, BMSR_DEFCAPMASK, MII_PHY_ANY, MII_OFFSET_ANY, 0);
	if (error != 0) {
		device_printf(dev, "attaching PHYs failed\n");
		goto fail;
	}

	/* Call MI attach routine. */
	ether_ifattach(ifp, eaddr);

	/* Hook interrupt last to avoid having to lock softc */
	error = bus_setup_intr(dev, sc->kr_rx_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, kr_rx_intr, sc, &sc->kr_rx_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up rx irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

	error = bus_setup_intr(dev, sc->kr_tx_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    NULL, kr_tx_intr, sc, &sc->kr_tx_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up tx irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

	error = bus_setup_intr(dev, sc->kr_rx_und_irq, 
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, kr_rx_und_intr, sc, 
	    &sc->kr_rx_und_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up rx underrun irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

	error = bus_setup_intr(dev, sc->kr_tx_ovr_irq, 
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, kr_tx_ovr_intr, sc, 
	    &sc->kr_tx_ovr_intrhand);

	if (error) {
		device_printf(dev, "couldn't set up tx overrun irq\n");
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error) 
		kr_detach(dev);

	return (error);
}

static int
kr_detach(device_t dev)
{
	struct kr_softc		*sc = device_get_softc(dev);
	struct ifnet		*ifp = sc->kr_ifp;

	KASSERT(mtx_initialized(&sc->kr_mtx), ("vr mutex not initialized"));

	/* These should only be active if attach succeeded */
	if (device_is_attached(dev)) {
		KR_LOCK(sc);
		sc->kr_detach = 1;
		kr_stop(sc);
		KR_UNLOCK(sc);
		taskqueue_drain(taskqueue_swi, &sc->kr_link_task);
		ether_ifdetach(ifp);
	}
	if (sc->kr_miibus)
		device_delete_child(dev, sc->kr_miibus);
	bus_generic_detach(dev);

	if (sc->kr_rx_intrhand)
		bus_teardown_intr(dev, sc->kr_rx_irq, sc->kr_rx_intrhand);
	if (sc->kr_rx_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->kr_rx_irq);
	if (sc->kr_tx_intrhand)
		bus_teardown_intr(dev, sc->kr_tx_irq, sc->kr_tx_intrhand);
	if (sc->kr_tx_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->kr_tx_irq);
	if (sc->kr_rx_und_intrhand)
		bus_teardown_intr(dev, sc->kr_rx_und_irq, 
		    sc->kr_rx_und_intrhand);
	if (sc->kr_rx_und_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->kr_rx_und_irq);
	if (sc->kr_tx_ovr_intrhand)
		bus_teardown_intr(dev, sc->kr_tx_ovr_irq, 
		    sc->kr_tx_ovr_intrhand);
	if (sc->kr_tx_ovr_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->kr_tx_ovr_irq);

	if (sc->kr_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->kr_rid, 
		    sc->kr_res);

	if (ifp)
		if_free(ifp);

	kr_dma_free(sc);

	mtx_destroy(&sc->kr_mtx);

	return (0);

}

static int
kr_suspend(device_t dev)
{

	panic("%s", __func__);
	return 0;
}

static int
kr_resume(device_t dev)
{

	panic("%s", __func__);
	return 0;
}

static int
kr_shutdown(device_t dev)
{
	struct kr_softc	*sc;

	sc = device_get_softc(dev);

	KR_LOCK(sc);
	kr_stop(sc);
	KR_UNLOCK(sc);

	return (0);
}

static int
kr_miibus_readreg(device_t dev, int phy, int reg)
{
	struct kr_softc * sc = device_get_softc(dev);
	int i, result;

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii is busy %d:%d\n", phy, reg);

	CSR_WRITE_4(sc, KR_MIIMADDR, (phy << 8) | reg);

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii is busy %d:%d\n", phy, reg);

	CSR_WRITE_4(sc, KR_MIIMCMD, KR_MIIMCMD_RD);

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii read is timed out %d:%d\n", phy, 
		    reg);

	if (CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_NV)
		printf("phy mii readreg failed %d:%d: data not valid\n",
		    phy, reg);

	result = CSR_READ_4(sc , KR_MIIMRDD);
	CSR_WRITE_4(sc, KR_MIIMCMD, 0);

	return (result);
}

static int
kr_miibus_writereg(device_t dev, int phy, int reg, int data)
{
	struct kr_softc * sc = device_get_softc(dev);
	int i;

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii is busy %d:%d\n", phy, reg);

	CSR_WRITE_4(sc, KR_MIIMADDR, (phy << 8) | reg);

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii is busy %d:%d\n", phy, reg);

	CSR_WRITE_4(sc, KR_MIIMWTD, data);

	i = KR_MII_TIMEOUT;
	while ((CSR_READ_4(sc, KR_MIIMIND) & KR_MIIMIND_BSY) && i)
		i--;

	if (i == 0)
		device_printf(dev, "phy mii is busy %d:%d\n", phy, reg);

	return (0);
}

static void
kr_miibus_statchg(device_t dev)
{
	struct kr_softc		*sc;

	sc = device_get_softc(dev);
	taskqueue_enqueue(taskqueue_swi, &sc->kr_link_task);
}

static void
kr_link_task(void *arg, int pending)
{
	struct kr_softc		*sc;
	struct mii_data		*mii;
	struct ifnet		*ifp;
	/* int			lfdx, mfdx; */

	sc = (struct kr_softc *)arg;

	KR_LOCK(sc);
	mii = device_get_softc(sc->kr_miibus);
	ifp = sc->kr_ifp;
	if (mii == NULL || ifp == NULL ||
	    (ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		KR_UNLOCK(sc);
		return;
	}

	if (mii->mii_media_status & IFM_ACTIVE) {
		if (IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
			sc->kr_link_status = 1;
	} else
		sc->kr_link_status = 0;

	KR_UNLOCK(sc);
}

static void
kr_reset(struct kr_softc *sc)
{
	int		i;

	CSR_WRITE_4(sc, KR_ETHINTFC, 0);

	for (i = 0; i < KR_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, KR_ETHINTFC) & ETH_INTFC_RIP))
			break;
	}

	if (i == KR_TIMEOUT)
		device_printf(sc->kr_dev, "reset time out\n");
}

static void
kr_init(void *xsc)
{
	struct kr_softc	 *sc = xsc;

	KR_LOCK(sc);
	kr_init_locked(sc);
	KR_UNLOCK(sc);
}

static void
kr_init_locked(struct kr_softc *sc)
{
	struct ifnet		*ifp = sc->kr_ifp;
	struct mii_data		*mii;

	KR_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->kr_miibus);

	kr_stop(sc);
	kr_reset(sc);

	CSR_WRITE_4(sc, KR_ETHINTFC, ETH_INTFC_EN);

	/* Init circular RX list. */
	if (kr_rx_ring_init(sc) != 0) {
		device_printf(sc->kr_dev,
		    "initialization failed: no memory for rx buffers\n");
		kr_stop(sc);
		return;
	}

	/* Init tx descriptors. */
	kr_tx_ring_init(sc);

	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_S, 0);
	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_NDPTR, 0);
	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_DPTR, 
	    sc->kr_rdata.kr_rx_ring_paddr);


	KR_DMA_CLEARBITS_REG(KR_DMA_RXCHAN, DMA_SM, 
	    DMA_SM_H | DMA_SM_E | DMA_SM_D) ;

	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_S, 0);
	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_NDPTR, 0);
	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_DPTR, 0);
	KR_DMA_CLEARBITS_REG(KR_DMA_TXCHAN, DMA_SM, 
	    DMA_SM_F | DMA_SM_E);


	/* Accept only packets destined for THIS Ethernet device address */
	CSR_WRITE_4(sc, KR_ETHARC, 1);

	/* 
	 * Set all Ethernet address registers to the same initial values
	 * set all four addresses to 66-88-aa-cc-dd-ee 
	 */
	CSR_WRITE_4(sc, KR_ETHSAL0, 0x42095E6B);
	CSR_WRITE_4(sc, KR_ETHSAH0, 0x0000000C);

	CSR_WRITE_4(sc, KR_ETHSAL1, 0x42095E6B);
	CSR_WRITE_4(sc, KR_ETHSAH1, 0x0000000C);

	CSR_WRITE_4(sc, KR_ETHSAL2, 0x42095E6B);
	CSR_WRITE_4(sc, KR_ETHSAH2, 0x0000000C);

	CSR_WRITE_4(sc, KR_ETHSAL3, 0x42095E6B);
	CSR_WRITE_4(sc, KR_ETHSAH3, 0x0000000C);

	CSR_WRITE_4(sc, KR_ETHMAC2, 
	    KR_ETH_MAC2_PEN | KR_ETH_MAC2_CEN | KR_ETH_MAC2_FD);

	CSR_WRITE_4(sc, KR_ETHIPGT, KR_ETHIPGT_FULL_DUPLEX);
	CSR_WRITE_4(sc, KR_ETHIPGR, 0x12); /* minimum value */

	CSR_WRITE_4(sc, KR_MIIMCFG, KR_MIIMCFG_R);
	DELAY(1000);
	CSR_WRITE_4(sc, KR_MIIMCFG, 0);

	/* TODO: calculate prescale */
	CSR_WRITE_4(sc, KR_ETHMCP, (165000000 / (1250000 + 1)) & ~1);

	/* FIFO Tx threshold level */
	CSR_WRITE_4(sc, KR_ETHFIFOTT, 0x30);

	CSR_WRITE_4(sc, KR_ETHMAC1, KR_ETH_MAC1_RE);

	sc->kr_link_status = 0;
	mii_mediachg(mii);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&sc->kr_stat_callout, hz, kr_tick, sc);
}

static void
kr_start(struct ifnet *ifp)
{
	struct kr_softc	 *sc;

	sc = ifp->if_softc;

	KR_LOCK(sc);
	kr_start_locked(ifp);
	KR_UNLOCK(sc);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int
kr_encap(struct kr_softc *sc, struct mbuf **m_head)
{
	struct kr_txdesc	*txd;
	struct kr_desc		*desc, *prev_desc;
	bus_dma_segment_t	txsegs[KR_MAXFRAGS];
	uint32_t		link_addr;
	int			error, i, nsegs, prod, si, prev_prod;

	KR_LOCK_ASSERT(sc);

	prod = sc->kr_cdata.kr_tx_prod;
	txd = &sc->kr_cdata.kr_txdesc[prod];
	error = bus_dmamap_load_mbuf_sg(sc->kr_cdata.kr_tx_tag, txd->tx_dmamap,
	    *m_head, txsegs, &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		panic("EFBIG");
	} else if (error != 0)
		return (error);
	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	/* Check number of available descriptors. */
	if (sc->kr_cdata.kr_tx_cnt + nsegs >= (KR_TX_RING_CNT - 1)) {
		bus_dmamap_unload(sc->kr_cdata.kr_tx_tag, txd->tx_dmamap);
		return (ENOBUFS);
	}

	txd->tx_m = *m_head;
	bus_dmamap_sync(sc->kr_cdata.kr_tx_tag, txd->tx_dmamap,
	    BUS_DMASYNC_PREWRITE);

	si = prod;

	/* 
	 * Make a list of descriptors for this packet. DMA controller will
	 * walk through it while kr_link is not zero. The last one should
	 * have COF flag set, to pickup next chain from NDPTR
	 */
	prev_prod = prod;
	desc = prev_desc = NULL;
	for (i = 0; i < nsegs; i++) {
		desc = &sc->kr_rdata.kr_tx_ring[prod];
		desc->kr_ctl = KR_DMASIZE(txsegs[i].ds_len) | KR_CTL_IOF;
		if (i == 0)
			desc->kr_devcs = KR_DMATX_DEVCS_FD;
		desc->kr_ca = txsegs[i].ds_addr;
		desc->kr_link = 0;
		/* link with previous descriptor */
		if (prev_desc)
			prev_desc->kr_link = KR_TX_RING_ADDR(sc, prod);

		sc->kr_cdata.kr_tx_cnt++;
		prev_desc = desc;
		KR_INC(prod, KR_TX_RING_CNT);
	}

	/* 
	 * Set COF for last descriptor and mark last fragment with LD flag
	 */
	if (desc) {
		desc->kr_ctl |=  KR_CTL_COF;
		desc->kr_devcs |= KR_DMATX_DEVCS_LD;
	}

	/* Update producer index. */
	sc->kr_cdata.kr_tx_prod = prod;

	/* Sync descriptors. */
	bus_dmamap_sync(sc->kr_cdata.kr_tx_ring_tag,
	    sc->kr_cdata.kr_tx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Start transmitting */
	/* Check if new list is queued in NDPTR */
	if (KR_DMA_READ_REG(KR_DMA_TXCHAN, DMA_NDPTR) == 0) {
		/* NDPTR is not busy - start new list */
		KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_NDPTR, 
		    KR_TX_RING_ADDR(sc, si));
	}
	else {
		link_addr = KR_TX_RING_ADDR(sc, si);
		/* Get previous descriptor */
		si = (si + KR_TX_RING_CNT - 1) % KR_TX_RING_CNT;
		desc = &sc->kr_rdata.kr_tx_ring[si];
		desc->kr_link = link_addr;
	}

	return (0);
}

static void
kr_start_locked(struct ifnet *ifp)
{
	struct kr_softc		*sc;
	struct mbuf		*m_head;
	int			enq;

	sc = ifp->if_softc;

	KR_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || sc->kr_link_status == 0 )
		return;

	for (enq = 0; !IFQ_DRV_IS_EMPTY(&ifp->if_snd) &&
	    sc->kr_cdata.kr_tx_cnt < KR_TX_RING_CNT - 2; ) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (kr_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		enq++;
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		ETHER_BPF_MTAP(ifp, m_head);
	}
}

static void
kr_stop(struct kr_softc *sc)
{
	struct ifnet	    *ifp;

	KR_LOCK_ASSERT(sc);


	ifp = sc->kr_ifp;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	callout_stop(&sc->kr_stat_callout);

	/* mask out RX interrupts */
	KR_DMA_SETBITS_REG(KR_DMA_RXCHAN, DMA_SM, 
	    DMA_SM_D | DMA_SM_H | DMA_SM_E);

	/* mask out TX interrupts */
	KR_DMA_SETBITS_REG(KR_DMA_TXCHAN, DMA_SM, 
	    DMA_SM_F | DMA_SM_E);

	/* Abort RX DMA transactions */
	if (KR_DMA_READ_REG(KR_DMA_RXCHAN, DMA_C) & DMA_C_R) {
		/* Set ABORT bit if trunsuction is in progress */
		KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_C, DMA_C_ABORT);
		/* XXX: Add timeout */
		while ((KR_DMA_READ_REG(KR_DMA_RXCHAN, DMA_S) & DMA_S_H) == 0)
			DELAY(10);
		KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_S, 0);
	}
	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_DPTR, 0);
	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_NDPTR, 0);

	/* Abort TX DMA transactions */
	if (KR_DMA_READ_REG(KR_DMA_TXCHAN, DMA_C) & DMA_C_R) {
		/* Set ABORT bit if trunsuction is in progress */
		KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_C, DMA_C_ABORT);
		/* XXX: Add timeout */
		while ((KR_DMA_READ_REG(KR_DMA_TXCHAN, DMA_S) & DMA_S_H) == 0)
			DELAY(10);
		KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_S, 0);
	}
	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_DPTR, 0);
	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_NDPTR, 0);

	CSR_WRITE_4(sc, KR_ETHINTFC, 0);
}


static int
kr_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct kr_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			error;

	switch (command) {
	case SIOCSIFFLAGS:
#if 0
		KR_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				if ((ifp->if_flags ^ sc->kr_if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI))
					kr_set_filter(sc);
			} else {
				if (sc->kr_detach == 0)
					kr_init_locked(sc);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				kr_stop(sc);
		}
		sc->kr_if_flags = ifp->if_flags;
		KR_UNLOCK(sc);
#endif
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
#if 0
		KR_LOCK(sc);
		kr_set_filter(sc);
		KR_UNLOCK(sc);
#endif
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = device_get_softc(sc->kr_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	case SIOCSIFCAP:
		error = 0;
#if 0
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if ((mask & IFCAP_HWCSUM) != 0) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if ((IFCAP_HWCSUM & ifp->if_capenable) &&
			    (IFCAP_HWCSUM & ifp->if_capabilities))
				ifp->if_hwassist = KR_CSUM_FEATURES;
			else
				ifp->if_hwassist = 0;
		}
		if ((mask & IFCAP_VLAN_HWTAGGING) != 0) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			if (IFCAP_VLAN_HWTAGGING & ifp->if_capenable &&
			    IFCAP_VLAN_HWTAGGING & ifp->if_capabilities &&
			    ifp->if_drv_flags & IFF_DRV_RUNNING) {
				KR_LOCK(sc);
				kr_vlan_setup(sc);
				KR_UNLOCK(sc);
			}
		}
		VLAN_CAPABILITIES(ifp);
#endif
		break;
	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

/*
 * Set media options.
 */
static int
kr_ifmedia_upd(struct ifnet *ifp)
{
	struct kr_softc		*sc;
	struct mii_data		*mii;
	struct mii_softc	*miisc;
	int			error;

	sc = ifp->if_softc;
	KR_LOCK(sc);
	mii = device_get_softc(sc->kr_miibus);
	if (mii->mii_instance) {
		LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
			mii_phy_reset(miisc);
	}
	error = mii_mediachg(mii);
	KR_UNLOCK(sc);

	return (error);
}

/*
 * Report current media status.
 */
static void
kr_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct kr_softc		*sc = ifp->if_softc;
	struct mii_data		*mii;

	mii = device_get_softc(sc->kr_miibus);
	KR_LOCK(sc);
	mii_pollstat(mii);
	KR_UNLOCK(sc);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

struct kr_dmamap_arg {
	bus_addr_t	kr_busaddr;
};

static void
kr_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct kr_dmamap_arg	*ctx;

	if (error != 0)
		return;
	ctx = arg;
	ctx->kr_busaddr = segs[0].ds_addr;
}

static int
kr_dma_alloc(struct kr_softc *sc)
{
	struct kr_dmamap_arg	ctx;
	struct kr_txdesc	*txd;
	struct kr_rxdesc	*rxd;
	int			error, i;

	/* Create parent DMA tag. */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->kr_dev),	/* parent */
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsize */
	    0,				/* nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->kr_cdata.kr_parent_tag);
	if (error != 0) {
		device_printf(sc->kr_dev, "failed to create parent DMA tag\n");
		goto fail;
	}
	/* Create tag for Tx ring. */
	error = bus_dma_tag_create(
	    sc->kr_cdata.kr_parent_tag,	/* parent */
	    KR_RING_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    KR_TX_RING_SIZE,		/* maxsize */
	    1,				/* nsegments */
	    KR_TX_RING_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->kr_cdata.kr_tx_ring_tag);
	if (error != 0) {
		device_printf(sc->kr_dev, "failed to create Tx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx ring. */
	error = bus_dma_tag_create(
	    sc->kr_cdata.kr_parent_tag,	/* parent */
	    KR_RING_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    KR_RX_RING_SIZE,		/* maxsize */
	    1,				/* nsegments */
	    KR_RX_RING_SIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->kr_cdata.kr_rx_ring_tag);
	if (error != 0) {
		device_printf(sc->kr_dev, "failed to create Rx ring DMA tag\n");
		goto fail;
	}

	/* Create tag for Tx buffers. */
	error = bus_dma_tag_create(
	    sc->kr_cdata.kr_parent_tag,	/* parent */
	    sizeof(uint32_t), 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES * KR_MAXFRAGS,	/* maxsize */
	    KR_MAXFRAGS,		/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->kr_cdata.kr_tx_tag);
	if (error != 0) {
		device_printf(sc->kr_dev, "failed to create Tx DMA tag\n");
		goto fail;
	}

	/* Create tag for Rx buffers. */
	error = bus_dma_tag_create(
	    sc->kr_cdata.kr_parent_tag,	/* parent */
	    KR_RX_ALIGN, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES,			/* maxsize */
	    1,				/* nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->kr_cdata.kr_rx_tag);
	if (error != 0) {
		device_printf(sc->kr_dev, "failed to create Rx DMA tag\n");
		goto fail;
	}

	/* Allocate DMA'able memory and load the DMA map for Tx ring. */
	error = bus_dmamem_alloc(sc->kr_cdata.kr_tx_ring_tag,
	    (void **)&sc->kr_rdata.kr_tx_ring, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->kr_cdata.kr_tx_ring_map);
	if (error != 0) {
		device_printf(sc->kr_dev,
		    "failed to allocate DMA'able memory for Tx ring\n");
		goto fail;
	}

	ctx.kr_busaddr = 0;
	error = bus_dmamap_load(sc->kr_cdata.kr_tx_ring_tag,
	    sc->kr_cdata.kr_tx_ring_map, sc->kr_rdata.kr_tx_ring,
	    KR_TX_RING_SIZE, kr_dmamap_cb, &ctx, 0);
	if (error != 0 || ctx.kr_busaddr == 0) {
		device_printf(sc->kr_dev,
		    "failed to load DMA'able memory for Tx ring\n");
		goto fail;
	}
	sc->kr_rdata.kr_tx_ring_paddr = ctx.kr_busaddr;

	/* Allocate DMA'able memory and load the DMA map for Rx ring. */
	error = bus_dmamem_alloc(sc->kr_cdata.kr_rx_ring_tag,
	    (void **)&sc->kr_rdata.kr_rx_ring, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &sc->kr_cdata.kr_rx_ring_map);
	if (error != 0) {
		device_printf(sc->kr_dev,
		    "failed to allocate DMA'able memory for Rx ring\n");
		goto fail;
	}

	ctx.kr_busaddr = 0;
	error = bus_dmamap_load(sc->kr_cdata.kr_rx_ring_tag,
	    sc->kr_cdata.kr_rx_ring_map, sc->kr_rdata.kr_rx_ring,
	    KR_RX_RING_SIZE, kr_dmamap_cb, &ctx, 0);
	if (error != 0 || ctx.kr_busaddr == 0) {
		device_printf(sc->kr_dev,
		    "failed to load DMA'able memory for Rx ring\n");
		goto fail;
	}
	sc->kr_rdata.kr_rx_ring_paddr = ctx.kr_busaddr;

	/* Create DMA maps for Tx buffers. */
	for (i = 0; i < KR_TX_RING_CNT; i++) {
		txd = &sc->kr_cdata.kr_txdesc[i];
		txd->tx_m = NULL;
		txd->tx_dmamap = NULL;
		error = bus_dmamap_create(sc->kr_cdata.kr_tx_tag, 0,
		    &txd->tx_dmamap);
		if (error != 0) {
			device_printf(sc->kr_dev,
			    "failed to create Tx dmamap\n");
			goto fail;
		}
	}
	/* Create DMA maps for Rx buffers. */
	if ((error = bus_dmamap_create(sc->kr_cdata.kr_rx_tag, 0,
	    &sc->kr_cdata.kr_rx_sparemap)) != 0) {
		device_printf(sc->kr_dev,
		    "failed to create spare Rx dmamap\n");
		goto fail;
	}
	for (i = 0; i < KR_RX_RING_CNT; i++) {
		rxd = &sc->kr_cdata.kr_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->rx_dmamap = NULL;
		error = bus_dmamap_create(sc->kr_cdata.kr_rx_tag, 0,
		    &rxd->rx_dmamap);
		if (error != 0) {
			device_printf(sc->kr_dev,
			    "failed to create Rx dmamap\n");
			goto fail;
		}
	}

fail:
	return (error);
}

static void
kr_dma_free(struct kr_softc *sc)
{
	struct kr_txdesc	*txd;
	struct kr_rxdesc	*rxd;
	int			i;

	/* Tx ring. */
	if (sc->kr_cdata.kr_tx_ring_tag) {
		if (sc->kr_cdata.kr_tx_ring_map)
			bus_dmamap_unload(sc->kr_cdata.kr_tx_ring_tag,
			    sc->kr_cdata.kr_tx_ring_map);
		if (sc->kr_cdata.kr_tx_ring_map &&
		    sc->kr_rdata.kr_tx_ring)
			bus_dmamem_free(sc->kr_cdata.kr_tx_ring_tag,
			    sc->kr_rdata.kr_tx_ring,
			    sc->kr_cdata.kr_tx_ring_map);
		sc->kr_rdata.kr_tx_ring = NULL;
		sc->kr_cdata.kr_tx_ring_map = NULL;
		bus_dma_tag_destroy(sc->kr_cdata.kr_tx_ring_tag);
		sc->kr_cdata.kr_tx_ring_tag = NULL;
	}
	/* Rx ring. */
	if (sc->kr_cdata.kr_rx_ring_tag) {
		if (sc->kr_cdata.kr_rx_ring_map)
			bus_dmamap_unload(sc->kr_cdata.kr_rx_ring_tag,
			    sc->kr_cdata.kr_rx_ring_map);
		if (sc->kr_cdata.kr_rx_ring_map &&
		    sc->kr_rdata.kr_rx_ring)
			bus_dmamem_free(sc->kr_cdata.kr_rx_ring_tag,
			    sc->kr_rdata.kr_rx_ring,
			    sc->kr_cdata.kr_rx_ring_map);
		sc->kr_rdata.kr_rx_ring = NULL;
		sc->kr_cdata.kr_rx_ring_map = NULL;
		bus_dma_tag_destroy(sc->kr_cdata.kr_rx_ring_tag);
		sc->kr_cdata.kr_rx_ring_tag = NULL;
	}
	/* Tx buffers. */
	if (sc->kr_cdata.kr_tx_tag) {
		for (i = 0; i < KR_TX_RING_CNT; i++) {
			txd = &sc->kr_cdata.kr_txdesc[i];
			if (txd->tx_dmamap) {
				bus_dmamap_destroy(sc->kr_cdata.kr_tx_tag,
				    txd->tx_dmamap);
				txd->tx_dmamap = NULL;
			}
		}
		bus_dma_tag_destroy(sc->kr_cdata.kr_tx_tag);
		sc->kr_cdata.kr_tx_tag = NULL;
	}
	/* Rx buffers. */
	if (sc->kr_cdata.kr_rx_tag) {
		for (i = 0; i < KR_RX_RING_CNT; i++) {
			rxd = &sc->kr_cdata.kr_rxdesc[i];
			if (rxd->rx_dmamap) {
				bus_dmamap_destroy(sc->kr_cdata.kr_rx_tag,
				    rxd->rx_dmamap);
				rxd->rx_dmamap = NULL;
			}
		}
		if (sc->kr_cdata.kr_rx_sparemap) {
			bus_dmamap_destroy(sc->kr_cdata.kr_rx_tag,
			    sc->kr_cdata.kr_rx_sparemap);
			sc->kr_cdata.kr_rx_sparemap = 0;
		}
		bus_dma_tag_destroy(sc->kr_cdata.kr_rx_tag);
		sc->kr_cdata.kr_rx_tag = NULL;
	}

	if (sc->kr_cdata.kr_parent_tag) {
		bus_dma_tag_destroy(sc->kr_cdata.kr_parent_tag);
		sc->kr_cdata.kr_parent_tag = NULL;
	}
}

/*
 * Initialize the transmit descriptors.
 */
static int
kr_tx_ring_init(struct kr_softc *sc)
{
	struct kr_ring_data	*rd;
	struct kr_txdesc	*txd;
	bus_addr_t		addr;
	int			i;

	sc->kr_cdata.kr_tx_prod = 0;
	sc->kr_cdata.kr_tx_cons = 0;
	sc->kr_cdata.kr_tx_cnt = 0;
	sc->kr_cdata.kr_tx_pkts = 0;

	rd = &sc->kr_rdata;
	bzero(rd->kr_tx_ring, KR_TX_RING_SIZE);
	for (i = 0; i < KR_TX_RING_CNT; i++) {
		if (i == KR_TX_RING_CNT - 1)
			addr = KR_TX_RING_ADDR(sc, 0);
		else
			addr = KR_TX_RING_ADDR(sc, i + 1);
		rd->kr_tx_ring[i].kr_ctl = KR_CTL_IOF;
		rd->kr_tx_ring[i].kr_ca = 0;
		rd->kr_tx_ring[i].kr_devcs = 0;
		rd->kr_tx_ring[i].kr_link = 0;
		txd = &sc->kr_cdata.kr_txdesc[i];
		txd->tx_m = NULL;
	}

	bus_dmamap_sync(sc->kr_cdata.kr_tx_ring_tag,
	    sc->kr_cdata.kr_tx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
static int
kr_rx_ring_init(struct kr_softc *sc)
{
	struct kr_ring_data	*rd;
	struct kr_rxdesc	*rxd;
	bus_addr_t		addr;
	int			i;

	sc->kr_cdata.kr_rx_cons = 0;

	rd = &sc->kr_rdata;
	bzero(rd->kr_rx_ring, KR_RX_RING_SIZE);
	for (i = 0; i < KR_RX_RING_CNT; i++) {
		rxd = &sc->kr_cdata.kr_rxdesc[i];
		rxd->rx_m = NULL;
		rxd->desc = &rd->kr_rx_ring[i];
		if (i == KR_RX_RING_CNT - 1)
			addr = KR_RX_RING_ADDR(sc, 0);
		else
			addr = KR_RX_RING_ADDR(sc, i + 1);
		rd->kr_rx_ring[i].kr_ctl = KR_CTL_IOD;
		if (i == KR_RX_RING_CNT - 1)
			rd->kr_rx_ring[i].kr_ctl |= KR_CTL_COD;
		rd->kr_rx_ring[i].kr_devcs = 0;
		rd->kr_rx_ring[i].kr_ca = 0;
		rd->kr_rx_ring[i].kr_link = addr;
		if (kr_newbuf(sc, i) != 0)
			return (ENOBUFS);
	}

	bus_dmamap_sync(sc->kr_cdata.kr_rx_ring_tag,
	    sc->kr_cdata.kr_rx_ring_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
static int
kr_newbuf(struct kr_softc *sc, int idx)
{
	struct kr_desc		*desc;
	struct kr_rxdesc	*rxd;
	struct mbuf		*m;
	bus_dma_segment_t	segs[1];
	bus_dmamap_t		map;
	int			nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, sizeof(uint64_t));

	if (bus_dmamap_load_mbuf_sg(sc->kr_cdata.kr_rx_tag,
	    sc->kr_cdata.kr_rx_sparemap, m, segs, &nsegs, 0) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	rxd = &sc->kr_cdata.kr_rxdesc[idx];
	if (rxd->rx_m != NULL) {
		bus_dmamap_sync(sc->kr_cdata.kr_rx_tag, rxd->rx_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->kr_cdata.kr_rx_tag, rxd->rx_dmamap);
	}
	map = rxd->rx_dmamap;
	rxd->rx_dmamap = sc->kr_cdata.kr_rx_sparemap;
	sc->kr_cdata.kr_rx_sparemap = map;
	bus_dmamap_sync(sc->kr_cdata.kr_rx_tag, rxd->rx_dmamap,
	    BUS_DMASYNC_PREREAD);
	rxd->rx_m = m;
	desc = rxd->desc;
	desc->kr_ca = segs[0].ds_addr;
	desc->kr_ctl |= KR_DMASIZE(segs[0].ds_len);
	rxd->saved_ca = desc->kr_ca ;
	rxd->saved_ctl = desc->kr_ctl ;

	return (0);
}

static __inline void
kr_fixup_rx(struct mbuf *m)
{
        int		i;
        uint16_t	*src, *dst;

	src = mtod(m, uint16_t *);
	dst = src - 1;

	for (i = 0; i < (m->m_len / sizeof(uint16_t) + 1); i++)
		*dst++ = *src++;

	m->m_data -= ETHER_ALIGN;
}


static void
kr_tx(struct kr_softc *sc)
{
	struct kr_txdesc	*txd;
	struct kr_desc		*cur_tx;
	struct ifnet		*ifp;
	uint32_t		ctl, devcs;
	int			cons, prod;

	KR_LOCK_ASSERT(sc);

	cons = sc->kr_cdata.kr_tx_cons;
	prod = sc->kr_cdata.kr_tx_prod;
	if (cons == prod)
		return;

	bus_dmamap_sync(sc->kr_cdata.kr_tx_ring_tag,
	    sc->kr_cdata.kr_tx_ring_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	ifp = sc->kr_ifp;
	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (; cons != prod; KR_INC(cons, KR_TX_RING_CNT)) {
		cur_tx = &sc->kr_rdata.kr_tx_ring[cons];
		ctl = cur_tx->kr_ctl;
		devcs = cur_tx->kr_devcs;
		/* Check if descriptor has "finished" flag */
		if ((ctl & KR_CTL_F) == 0)
			break;

		sc->kr_cdata.kr_tx_cnt--;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		txd = &sc->kr_cdata.kr_txdesc[cons];

		if (devcs & KR_DMATX_DEVCS_TOK)
			ifp->if_opackets++;
		else {
			ifp->if_oerrors++;
			/* collisions: medium busy, late collision */
			if ((devcs & KR_DMATX_DEVCS_EC) || 
			    (devcs & KR_DMATX_DEVCS_LC))
				ifp->if_collisions++;
		}

		bus_dmamap_sync(sc->kr_cdata.kr_tx_tag, txd->tx_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->kr_cdata.kr_tx_tag, txd->tx_dmamap);

		/* Free only if it's first descriptor in list */
		if (txd->tx_m)
			m_freem(txd->tx_m);
		txd->tx_m = NULL;

		/* reset descriptor */
		cur_tx->kr_ctl = KR_CTL_IOF;
		cur_tx->kr_devcs = 0;
		cur_tx->kr_ca = 0;
		cur_tx->kr_link = 0; 
	}

	sc->kr_cdata.kr_tx_cons = cons;

	bus_dmamap_sync(sc->kr_cdata.kr_tx_ring_tag,
	    sc->kr_cdata.kr_tx_ring_map, BUS_DMASYNC_PREWRITE);
}


static void
kr_rx(struct kr_softc *sc)
{
	struct kr_rxdesc	*rxd;
	struct ifnet		*ifp = sc->kr_ifp;
	int			cons, prog, packet_len, count, error;
	struct kr_desc		*cur_rx;
	struct mbuf		*m;

	KR_LOCK_ASSERT(sc);

	cons = sc->kr_cdata.kr_rx_cons;

	bus_dmamap_sync(sc->kr_cdata.kr_rx_ring_tag,
	    sc->kr_cdata.kr_rx_ring_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (prog = 0; prog < KR_RX_RING_CNT; KR_INC(cons, KR_RX_RING_CNT)) {
		cur_rx = &sc->kr_rdata.kr_rx_ring[cons];
		rxd = &sc->kr_cdata.kr_rxdesc[cons];
		m = rxd->rx_m;

		if ((cur_rx->kr_ctl & KR_CTL_D) == 0)
		       break;	

		prog++;

		packet_len = KR_PKTSIZE(cur_rx->kr_devcs);
		count = m->m_len - KR_DMASIZE(cur_rx->kr_ctl);
		/* Assume it's error */
		error = 1;

		if (packet_len != count)
			ifp->if_ierrors++;
		else if (count < 64)
			ifp->if_ierrors++;
		else if ((cur_rx->kr_devcs & KR_DMARX_DEVCS_LD) == 0)
			ifp->if_ierrors++;
		else if ((cur_rx->kr_devcs & KR_DMARX_DEVCS_ROK) != 0) {
			error = 0;
			bus_dmamap_sync(sc->kr_cdata.kr_rx_tag, rxd->rx_dmamap,
			    BUS_DMASYNC_PREREAD);
			m = rxd->rx_m;
			kr_fixup_rx(m);
			m->m_pkthdr.rcvif = ifp;
			/* Skip 4 bytes of CRC */
			m->m_pkthdr.len = m->m_len = packet_len - ETHER_CRC_LEN;
			ifp->if_ipackets++;

			KR_UNLOCK(sc);
			(*ifp->if_input)(ifp, m);
			KR_LOCK(sc);
		}

		if (error) {
			/* Restore CONTROL and CA values, reset DEVCS */
			cur_rx->kr_ctl = rxd->saved_ctl;
			cur_rx->kr_ca = rxd->saved_ca;
			cur_rx->kr_devcs = 0;
		}
		else {
			/* Reinit descriptor */
			cur_rx->kr_ctl = KR_CTL_IOD;
			if (cons == KR_RX_RING_CNT - 1)
				cur_rx->kr_ctl |= KR_CTL_COD;
			cur_rx->kr_devcs = 0;
			cur_rx->kr_ca = 0;
			if (kr_newbuf(sc, cons) != 0) {
				device_printf(sc->kr_dev, 
				    "Failed to allocate buffer\n");
				break;
			}
		}

		bus_dmamap_sync(sc->kr_cdata.kr_rx_ring_tag,
		    sc->kr_cdata.kr_rx_ring_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	}

	if (prog > 0) {
		sc->kr_cdata.kr_rx_cons = cons;

		bus_dmamap_sync(sc->kr_cdata.kr_rx_ring_tag,
		    sc->kr_cdata.kr_rx_ring_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
}

static void
kr_rx_intr(void *arg)
{
	struct kr_softc		*sc = arg;
	uint32_t		status;

	KR_LOCK(sc);

	/* mask out interrupts */
	KR_DMA_SETBITS_REG(KR_DMA_RXCHAN, DMA_SM, 
	    DMA_SM_D | DMA_SM_H | DMA_SM_E);

	status = KR_DMA_READ_REG(KR_DMA_RXCHAN, DMA_S);
	if (status & (DMA_S_D | DMA_S_E | DMA_S_H)) {
		kr_rx(sc);

		if (status & DMA_S_E)
			device_printf(sc->kr_dev, "RX DMA error\n");
	}

	/* Reread status */
	status = KR_DMA_READ_REG(KR_DMA_RXCHAN, DMA_S);

	/* restart DMA RX  if it has been halted */
	if (status & DMA_S_H) {
		KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_DPTR, 
		    KR_RX_RING_ADDR(sc, sc->kr_cdata.kr_rx_cons));
	}

	KR_DMA_WRITE_REG(KR_DMA_RXCHAN, DMA_S, ~status);

	/* Enable F, H, E interrupts */
	KR_DMA_CLEARBITS_REG(KR_DMA_RXCHAN, DMA_SM, 
	    DMA_SM_D | DMA_SM_H | DMA_SM_E);

	KR_UNLOCK(sc);
}

static void
kr_tx_intr(void *arg)
{
	struct kr_softc		*sc = arg;
	uint32_t		status;

	KR_LOCK(sc);

	/* mask out interrupts */
	KR_DMA_SETBITS_REG(KR_DMA_TXCHAN, DMA_SM, 
	    DMA_SM_F | DMA_SM_E);

	status = KR_DMA_READ_REG(KR_DMA_TXCHAN, DMA_S);
	if (status & (DMA_S_F | DMA_S_E)) {
		kr_tx(sc);
		if (status & DMA_S_E)
			device_printf(sc->kr_dev, "DMA error\n");
	}

	KR_DMA_WRITE_REG(KR_DMA_TXCHAN, DMA_S, ~status);

	/* Enable F, E interrupts */
	KR_DMA_CLEARBITS_REG(KR_DMA_TXCHAN, DMA_SM, 
	    DMA_SM_F | DMA_SM_E);

	KR_UNLOCK(sc);

}

static void
kr_rx_und_intr(void *arg)
{

	panic("interrupt: %s\n", __func__);
}

static void
kr_tx_ovr_intr(void *arg)
{

	panic("interrupt: %s\n", __func__);
}

static void
kr_tick(void *xsc)
{
	struct kr_softc		*sc = xsc;
	struct mii_data		*mii;

	KR_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->kr_miibus);
	mii_tick(mii);
	callout_reset(&sc->kr_stat_callout, hz, kr_tick, sc);
}
