/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@Freebsd.org>
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
 */

/*
 * TI 3 Port Switch Ethernet (CPSW) Driver
 * Found in TI8148, AM335x SoCs
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/endian.h>
#include <sys/mbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/ethernet.h>
#include <net/bpf.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <sys/sockio.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/cpsw/if_cpswreg.h>
#include <arm/ti/cpsw/if_cpswvar.h>
 
#include <arm/ti/ti_scm.h>

#include "miibus_if.h"

static int cpsw_probe(device_t dev);
static int cpsw_attach(device_t dev);
static int cpsw_detach(device_t dev);
static int cpsw_shutdown(device_t dev);
static int cpsw_suspend(device_t dev);
static int cpsw_resume(device_t dev);

static int cpsw_miibus_readreg(device_t dev, int phy, int reg);
static int cpsw_miibus_writereg(device_t dev, int phy, int reg, int value);

static int cpsw_ifmedia_upd(struct ifnet *ifp);
static void cpsw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);

static void cpsw_init(void *arg);
static void cpsw_init_locked(void *arg);
static void cpsw_start(struct ifnet *ifp);
static void cpsw_start_locked(struct ifnet *ifp);
static void cpsw_stop(struct cpsw_softc *sc);
static int cpsw_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static int cpsw_allocate_dma(struct cpsw_softc *sc);
static int cpsw_free_dma(struct cpsw_softc *sc);
static int cpsw_new_rxbuf(struct cpsw_softc *sc, uint32_t i, uint32_t next);
static void cpsw_watchdog(struct cpsw_softc *sc);

static void cpsw_intr_rx_thresh(void *arg);
static void cpsw_intr_rx(void *arg);
static void cpsw_intr_rx_locked(void *arg);
static void cpsw_intr_tx(void *arg);
static void cpsw_intr_tx_locked(void *arg);
static void cpsw_intr_misc(void *arg);

static void cpsw_ale_read_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry);
static void cpsw_ale_write_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry);
static int cpsw_ale_uc_entry_set(struct cpsw_softc *sc, uint8_t port, uint8_t *mac);
static int cpsw_ale_mc_entry_set(struct cpsw_softc *sc, uint8_t portmap, uint8_t *mac);
#ifdef CPSW_DEBUG
static void cpsw_ale_dump_table(struct cpsw_softc *sc);
#endif

static device_method_t cpsw_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		cpsw_probe),
	DEVMETHOD(device_attach,	cpsw_attach),
	DEVMETHOD(device_detach,	cpsw_detach),
	DEVMETHOD(device_shutdown,	cpsw_shutdown),
	DEVMETHOD(device_suspend,	cpsw_suspend),
	DEVMETHOD(device_resume,	cpsw_resume),
	/* MII interface */
	DEVMETHOD(miibus_readreg,	cpsw_miibus_readreg),
	DEVMETHOD(miibus_writereg,	cpsw_miibus_writereg),
	{ 0, 0 }
};

static driver_t cpsw_driver = {
	"cpsw",
	cpsw_methods,
	sizeof(struct cpsw_softc),
};

static devclass_t cpsw_devclass;


DRIVER_MODULE(cpsw, simplebus, cpsw_driver, cpsw_devclass, 0, 0);
DRIVER_MODULE(miibus, cpsw, miibus_driver, miibus_devclass, 0, 0);
MODULE_DEPEND(cpsw, ether, 1, 1, 1);
MODULE_DEPEND(cpsw, miibus, 1, 1, 1);

static struct resource_spec res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE },
	{ SYS_RES_IRQ, 0, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 1, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 2, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_IRQ, 3, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

static struct {
	driver_intr_t *handler;
	char * description;
} cpsw_intrs[CPSW_INTR_COUNT + 1] = {
	{ cpsw_intr_rx_thresh,"CPSW RX threshold interrupt" },
	{ cpsw_intr_rx,	"CPSW RX interrupt" },
	{ cpsw_intr_tx,	"CPSW TX interrupt" },
	{ cpsw_intr_misc,"CPSW misc interrupt" },
};

/* Locking macros */
#define CPSW_TX_LOCK(sc) do {					\
		mtx_assert(&(sc)->rx_lock, MA_NOTOWNED);		\
		mtx_lock(&(sc)->tx_lock);				\
} while (0)

#define CPSW_TX_UNLOCK(sc)	mtx_unlock(&(sc)->tx_lock)
#define CPSW_TX_LOCK_ASSERT(sc)	mtx_assert(&(sc)->tx_lock, MA_OWNED)

#define CPSW_RX_LOCK(sc) do {					\
		mtx_assert(&(sc)->tx_lock, MA_NOTOWNED);		\
		mtx_lock(&(sc)->rx_lock);				\
} while (0)

#define CPSW_RX_UNLOCK(sc)		mtx_unlock(&(sc)->rx_lock)
#define CPSW_RX_LOCK_ASSERT(sc)	mtx_assert(&(sc)->rx_lock, MA_OWNED)

#define CPSW_GLOBAL_LOCK(sc) do {					\
		if ((mtx_owned(&(sc)->tx_lock) ? 1 : 0) !=	\
		    (mtx_owned(&(sc)->rx_lock) ? 1 : 0)) {		\
			panic("cpsw deadlock possibility detection!");	\
		}							\
		mtx_lock(&(sc)->tx_lock);				\
		mtx_lock(&(sc)->rx_lock);				\
} while (0)

#define CPSW_GLOBAL_UNLOCK(sc) do {					\
		CPSW_RX_UNLOCK(sc);				\
		CPSW_TX_UNLOCK(sc);				\
} while (0)

#define CPSW_GLOBAL_LOCK_ASSERT(sc) do {				\
		CPSW_TX_LOCK_ASSERT(sc);				\
		CPSW_RX_LOCK_ASSERT(sc);				\
} while (0)


static int
cpsw_probe(device_t dev)
{

	if (!ofw_bus_is_compatible(dev, "ti,cpsw"))
		return (ENXIO);

	device_set_desc(dev, "3-port Switch Ethernet Subsystem");
	return (BUS_PROBE_DEFAULT);
}

static int
cpsw_attach(device_t dev)
{
	struct cpsw_softc *sc;
	struct mii_softc *miisc;
	struct ifnet *ifp;
	uint8_t mac_addr[ETHER_ADDR_LEN];
	int i, error, phy;
	uint32_t reg;

	sc = device_get_softc(dev);
	sc->dev = dev;
	memcpy(sc->mac_addr, mac_addr, ETHER_ADDR_LEN);
	sc->node = ofw_bus_get_node(dev);

	/* Get phy address from fdt */
	if (fdt_get_phyaddr(sc->node, sc->dev, &phy, (void **)&sc->phy_sc) != 0) {
		device_printf(dev, "failed to get PHY address from FDT\n");
		return (ENXIO);
	}
	/* Initialize mutexes */
	mtx_init(&sc->tx_lock, device_get_nameunit(dev),
		"cpsw TX lock", MTX_DEF);
	mtx_init(&sc->rx_lock, device_get_nameunit(dev),
		"cpsw RX lock", MTX_DEF);

	/* Allocate IO and IRQ resources */
	error = bus_alloc_resources(dev, res_spec, sc->res);
	if (error) {
		device_printf(dev, "could not allocate resources\n");
		cpsw_detach(dev);
		return (ENXIO);
	}

	reg = cpsw_read_4(CPSW_SS_IDVER);
	device_printf(dev, "Version %d.%d (%d)\n", (reg >> 8 & 0x7),
		reg & 0xFF, (reg >> 11) & 0x1F);

	/* Allocate DMA, buffers, buffer descriptors */
	error = cpsw_allocate_dma(sc);
	if (error) {
		cpsw_detach(dev);
		return (ENXIO);
	}

	//cpsw_add_sysctls(sc); TODO

	/* Allocate network interface */
	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "if_alloc() failed\n");
		cpsw_detach(dev);
		return (ENOMEM);
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_SIMPLEX | IFF_MULTICAST | IFF_BROADCAST;
	ifp->if_capabilities = IFCAP_VLAN_MTU | IFCAP_HWCSUM; //FIXME VLAN?
	ifp->if_capenable = ifp->if_capabilities;

	ifp->if_init = cpsw_init;
	ifp->if_start = cpsw_start;
	ifp->if_ioctl = cpsw_ioctl;

	ifp->if_snd.ifq_drv_maxlen = CPSW_MAX_TX_BUFFERS - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	IFQ_SET_READY(&ifp->if_snd);

	/* Get low part of MAC address from control module (mac_id0_lo) */
	ti_scm_reg_read_4(0x630, &reg);
	mac_addr[0] = (reg >>  8) & 0xFF;
	mac_addr[1] = reg & 0xFF;

	/* Get high part of MAC address from control module (mac_id0_hi) */
	ti_scm_reg_read_4(0x634, &reg);
	mac_addr[2] = (reg >> 24) & 0xFF;
	mac_addr[3] = (reg >> 16) & 0xFF;
	mac_addr[4] = (reg >>  8) & 0xFF;
	mac_addr[5] = reg & 0xFF;

	ether_ifattach(ifp, sc->mac_addr);
	callout_init(&sc->wd_callout, 0);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(MDIOCONTROL, (1<<30) | (1<<18) | 0xFF);

	/* Attach PHY(s) */
	error = mii_attach(dev, &sc->miibus, ifp, cpsw_ifmedia_upd,
	    cpsw_ifmedia_sts, BMSR_DEFCAPMASK, phy, MII_OFFSET_ANY, 0);
	if (error) {
		device_printf(dev, "attaching PHYs failed\n");
		cpsw_detach(dev);
		return (error);
	}
	sc->mii = device_get_softc(sc->miibus);

	/* Tell the MAC where to find the PHY so autoneg works */
	miisc = LIST_FIRST(&sc->mii->mii_phys);

	/* Select PHY and enable interrupts */
	cpsw_write_4(MDIOUSERPHYSEL0, (1 << 6) | (miisc->mii_phy & 0x1F));

	/* Attach interrupt handlers */
	for (i = 1; i <= CPSW_INTR_COUNT; ++i) {
		error = bus_setup_intr(dev, sc->res[i],
		    INTR_TYPE_NET | INTR_MPSAFE,
		    NULL, *cpsw_intrs[i - 1].handler,
		    sc, &sc->ih_cookie[i - 1]);
		if (error) {
			device_printf(dev, "could not setup %s\n",
			    cpsw_intrs[i].description);
			cpsw_detach(dev);
			return (error);
		}
	}

	return (0);
}

static int
cpsw_detach(device_t dev)
{
	struct cpsw_softc *sc;
	int error,i;

	sc = device_get_softc(dev);

	/* Stop controller and free TX queue */
	if (sc->ifp)
		cpsw_shutdown(dev);

	/* Wait for stopping ticks */
        callout_drain(&sc->wd_callout);

	/* Stop and release all interrupts */
	for (i = 0; i < CPSW_INTR_COUNT; ++i) {
		if (!sc->ih_cookie[i])
			continue;

		error = bus_teardown_intr(dev, sc->res[1 + i], sc->ih_cookie[i]);
		if (error)
			device_printf(dev, "could not release %s\n",
			    cpsw_intrs[i + 1].description);
	}

	/* Detach network interface */
	if (sc->ifp) {
		ether_ifdetach(sc->ifp);
		if_free(sc->ifp);
	}

	/* Free DMA resources */
	cpsw_free_dma(sc);

	/* Free IO memory handler */
	bus_release_resources(dev, res_spec, sc->res);

	/* Destroy mutexes */
	mtx_destroy(&sc->rx_lock);
	mtx_destroy(&sc->tx_lock);

	return (0);
}

static int
cpsw_suspend(device_t dev)
{

	device_printf(dev, "%s\n", __FUNCTION__);
	return (0);
}

static int
cpsw_resume(device_t dev)
{

	device_printf(dev, "%s\n", __FUNCTION__);
	return (0);
}

static int
cpsw_shutdown(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_GLOBAL_LOCK(sc);

	cpsw_stop(sc);

	CPSW_GLOBAL_UNLOCK(sc);

	return (0);
}

static int
cpsw_miibus_readreg(device_t dev, int phy, int reg)
{
	struct cpsw_softc *sc;
	uint32_t r;
	uint32_t retries = CPSW_MIIBUS_RETRIES;

	sc = device_get_softc(dev);

	/* Wait until interface is ready by watching GO bit */
	while(--retries && (cpsw_read_4(MDIOUSERACCESS0) & (1 << 31)) )
		DELAY(CPSW_MIIBUS_DELAY);
	if (!retries)
		device_printf(dev, "Timeout while waiting for MDIO.\n");

	/* Set GO, phy and reg */
	cpsw_write_4(MDIOUSERACCESS0, (1 << 31) |
		((reg & 0x1F) << 21) | ((phy & 0x1F) << 16));

	while(--retries && (cpsw_read_4(MDIOUSERACCESS0) & (1 << 31)) )
		DELAY(CPSW_MIIBUS_DELAY);
	if (!retries)
		device_printf(dev, "Timeout while waiting for MDIO.\n");

	r = cpsw_read_4(MDIOUSERACCESS0);
	/* Check for ACK */
	if(r & (1<<29)) {
		return (r & 0xFFFF);
	}
	device_printf(dev, "Failed to read from PHY.\n");
	return 0;
}

static int
cpsw_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct cpsw_softc *sc;
	uint32_t retries = CPSW_MIIBUS_RETRIES;

	sc = device_get_softc(dev);

	/* Wait until interface is ready by watching GO bit */
	while(--retries && (cpsw_read_4(MDIOUSERACCESS0) & (1 << 31)) )
		DELAY(CPSW_MIIBUS_DELAY);
	if (!retries)
		device_printf(dev, "Timeout while waiting for MDIO.\n");

	/* Set GO, WRITE, phy, reg and value */
	cpsw_write_4(MDIOUSERACCESS0, (value & 0xFFFF) | (3 << 30) |
		((reg & 0x1F) << 21) | ((phy & 0x1F) << 16));

	while(--retries && (cpsw_read_4(MDIOUSERACCESS0) & (1 << 31)) )
		DELAY(CPSW_MIIBUS_DELAY);
	if (!retries)
		device_printf(dev, "Timeout while waiting for MDIO.\n");

	/* Check for ACK */
	if(cpsw_read_4(MDIOUSERACCESS0) & (1<<29)) {
		return 0;
	}
	device_printf(dev, "Failed to write to PHY.\n");

	return 0;
}

static int
cpsw_allocate_dma(struct cpsw_softc *sc)
{
	int err;
	int i;

	/* Allocate a busdma tag and DMA safe memory for tx mbufs. */
	err = bus_dma_tag_create(
		bus_get_dma_tag(sc->dev),	/* parent */
		1, 0,				/* alignment, boundary */
		BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filtfunc, filtfuncarg */
		MCLBYTES, 1,			/* maxsize, nsegments */
		MCLBYTES, 0,			/* maxsegsz, flags */
		NULL, NULL,			/* lockfunc, lockfuncarg */
		&sc->mbuf_dtag);		/* dmatag */

	if (err)
		return (ENOMEM);
	for (i = 0; i < CPSW_MAX_TX_BUFFERS; i++) {
		if ( bus_dmamap_create(sc->mbuf_dtag, 0, &sc->tx_dmamap[i])) {
			if_printf(sc->ifp, "failed to create dmamap for rx mbuf\n");
			return (ENOMEM);
		}
	}

	for (i = 0; i < CPSW_MAX_RX_BUFFERS; i++) {
		if ( bus_dmamap_create(sc->mbuf_dtag, 0, &sc->rx_dmamap[i])) {
			if_printf(sc->ifp, "failed to create dmamap for rx mbuf\n");
			return (ENOMEM);
		}
	}

	return (0);
}

static int
cpsw_free_dma(struct cpsw_softc *sc)
{
	// TODO
	return 0;
}

static int
cpsw_new_rxbuf(struct cpsw_softc *sc, uint32_t i, uint32_t next)
{
	bus_dma_segment_t seg[1];
	struct cpsw_cpdma_bd bd;
	int error;
	int nsegs;

	if (sc->rx_mbuf[i]) {
		bus_dmamap_sync(sc->mbuf_dtag, sc->rx_dmamap[i], BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mbuf_dtag, sc->rx_dmamap[i]);
	}

	sc->rx_mbuf[i] = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (sc->rx_mbuf[i] == NULL)
		return (ENOBUFS);

	sc->rx_mbuf[i]->m_len = sc->rx_mbuf[i]->m_pkthdr.len = sc->rx_mbuf[i]->m_ext.ext_size;

	error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, sc->rx_dmamap[i], 
		sc->rx_mbuf[i], seg, &nsegs, BUS_DMA_NOWAIT);

	KASSERT(nsegs == 1, ("Too many segments returned!"));
	if (nsegs != 1 || error)
		panic("%s: nsegs(%d), error(%d)",__func__, nsegs, error);

	bus_dmamap_sync(sc->mbuf_dtag, sc->rx_dmamap[i], BUS_DMASYNC_PREREAD);

	/* Create and submit new rx descriptor*/
	bd.next = next;
	bd.bufptr = seg->ds_addr;
	bd.buflen = MCLBYTES-1;
	bd.bufoff = 2; /* make IP hdr aligned with 4 */
	bd.pktlen = 0;
	bd.flags = CPDMA_BD_OWNER;
	cpsw_cpdma_write_rxbd(i, &bd);

	return (0);
}


static int
cpsw_encap(struct cpsw_softc *sc, struct mbuf *m0)
{
	bus_dma_segment_t seg[1];
	struct cpsw_cpdma_bd bd;
	int error;
	int nsegs;
	int idx;

	if (sc->txbd_queue_size == CPSW_MAX_TX_BUFFERS)
		return (ENOBUFS);

	idx = sc->txbd_head + sc->txbd_queue_size;

	if (idx >= (CPSW_MAX_TX_BUFFERS) )
		idx -= CPSW_MAX_TX_BUFFERS;

	/* Create mapping in DMA memory */
	error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, sc->tx_dmamap[idx], m0, seg, &nsegs,
	    BUS_DMA_NOWAIT);
	sc->tc[idx]++;
	if (error != 0 || nsegs != 1 ) {
		bus_dmamap_unload(sc->mbuf_dtag, sc->tx_dmamap[idx]);
		return ((error != 0) ? error : -1);
	}
	bus_dmamap_sync(sc->mbuf_dtag, sc->tx_dmamap[idx], BUS_DMASYNC_PREWRITE);

	/* Fill descriptor data */
	bd.next = 0;
	bd.bufptr = seg->ds_addr;
	bd.bufoff = 0;
	bd.buflen = (seg->ds_len < 64 ? 64 : seg->ds_len);
	bd.pktlen = (seg->ds_len < 64 ? 64 : seg->ds_len);
	/* Set OWNERSHIP, SOP, EOP */
	bd.flags = (7<<13);

	/* Write descriptor */
	cpsw_cpdma_write_txbd(idx, &bd);

	/* Previous descriptor should point to us */
	cpsw_cpdma_write_txbd_next(((idx-1<0)?(CPSW_MAX_TX_BUFFERS-1):(idx-1)),
		cpsw_cpdma_txbd_paddr(idx));

	sc->txbd_queue_size++;

	return (0);
}

static void
cpsw_start(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;

	CPSW_TX_LOCK(sc);
	cpsw_start_locked(ifp);
	CPSW_TX_UNLOCK(sc);
}

static void
cpsw_start_locked(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct mbuf *m0, *mtmp;
	uint32_t queued = 0;

	CPSW_TX_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	for (;;) {
		/* Get packet from the queue */
		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		mtmp = m_defrag(m0, M_DONTWAIT);
		if (mtmp)
			m0 = mtmp;

		if (cpsw_encap(sc, m0)) {
			IF_PREPEND(&ifp->if_snd, m0);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		queued++;
		BPF_MTAP(ifp, m0);
	}

	if (!queued)
		return;

	if (sc->eoq) {
		cpsw_write_4(CPSW_CPDMA_TX_HDP(0), cpsw_cpdma_txbd_paddr(sc->txbd_head));
		sc->eoq = 0;
	}
	sc->wd_timer = 5;
}

static void
cpsw_stop(struct cpsw_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	/* Stop tick engine */
	callout_stop(&sc->wd_callout);

	/* Disable interface */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	sc->wd_timer = 0;

	/* Disable interrupts  TODO */

}

static int
cpsw_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error;
	uint32_t flags;

	error = 0;

	// FIXME
	switch (command) {
	case SIOCSIFFLAGS:
		CPSW_GLOBAL_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				flags = ifp->if_flags ^ sc->cpsw_if_flags;
				if (flags & IFF_PROMISC)
					printf("%s: SIOCSIFFLAGS "
						"IFF_PROMISC unimplemented\n",
						__func__);

				if (flags & IFF_ALLMULTI)
					printf("%s: SIOCSIFFLAGS "
						"IFF_ALLMULTI unimplemented\n",
						__func__);
			} else {
				printf("%s: SIOCSIFFLAGS cpsw_init_locked\n", __func__);
				//cpsw_init_locked(sc);
			}
		}
		else if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			cpsw_stop(sc);

		sc->cpsw_if_flags = ifp->if_flags;
		CPSW_GLOBAL_UNLOCK(sc);
		break;
		printf("%s: SIOCSIFFLAGS\n",__func__);
		break;
	case SIOCADDMULTI:
		printf("%s: SIOCADDMULTI\n",__func__);
		break;
	case SIOCDELMULTI:
		printf("%s: SIOCDELMULTI\n",__func__);
		break;
	case SIOCSIFCAP:
		printf("%s: SIOCSIFCAP\n",__func__);
		break;
	case SIOCGIFMEDIA: /* fall through */
		printf("%s: SIOCGIFMEDIA\n",__func__);
	case SIOCSIFMEDIA:
		printf("%s: SIOCSIFMEDIA\n",__func__);
		error = ifmedia_ioctl(ifp, ifr, &sc->mii->mii_media, command);
		break;
	default:
		error = ether_ioctl(ifp, command, data);
	}
	return (error);
}

static void
cpsw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	CPSW_TX_LOCK(sc);

	mii = sc->mii;
	mii_pollstat(mii);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	CPSW_TX_UNLOCK(sc);
}


static int
cpsw_ifmedia_upd(struct ifnet *ifp)
{
	struct cpsw_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_UP) {
		CPSW_GLOBAL_LOCK(sc);

		sc->cpsw_media_status = sc->mii->mii_media.ifm_media;
		mii_mediachg(sc->mii);
		cpsw_init_locked(sc);

		CPSW_GLOBAL_UNLOCK(sc);
	}

	return (0);
}

static void
cpsw_intr_rx_thresh(void *arg)
{
}

static void
cpsw_intr_rx(void *arg)
{
	struct cpsw_softc *sc = arg;
	CPSW_RX_LOCK(sc);
	cpsw_intr_rx_locked(arg);
	CPSW_RX_UNLOCK(sc);
}

static void
cpsw_intr_rx_locked(void *arg)
{
	struct cpsw_softc *sc = arg;
	struct cpsw_cpdma_bd bd;
	struct ifnet *ifp;
	int i;

	ifp = sc->ifp;

	i = sc->rxbd_head;
	cpsw_cpdma_read_rxbd(i, &bd);

	while (bd.flags & CPDMA_BD_SOP) {
		cpsw_write_4(CPSW_CPDMA_RX_CP(0), cpsw_cpdma_rxbd_paddr(i));

		bus_dmamap_sync(sc->mbuf_dtag, sc->rx_dmamap[i], BUS_DMASYNC_POSTREAD);

		/* Fill mbuf */
		sc->rx_mbuf[i]->m_hdr.mh_data +=2;
		sc->rx_mbuf[i]->m_len = bd.pktlen-2;
		sc->rx_mbuf[i]->m_pkthdr.len = bd.pktlen-2;
		sc->rx_mbuf[i]->m_flags |= M_PKTHDR;
		sc->rx_mbuf[i]->m_pkthdr.rcvif = ifp;

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/* check for valid CRC by looking into pkt_err[5:4] */
			if ( (bd.flags & CPDMA_BD_PKT_ERR_MASK) == 0  ) {
				sc->rx_mbuf[i]->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				sc->rx_mbuf[i]->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				sc->rx_mbuf[i]->m_pkthdr.csum_data = 0xffff;
			}
		}

		/* Handover packet */
		CPSW_RX_UNLOCK(sc);
		(*ifp->if_input)(ifp, sc->rx_mbuf[i]);
		CPSW_RX_LOCK(sc);

		/* Allocate new buffer for current descriptor */
		cpsw_new_rxbuf(sc, i, 0);

		/* we are not at tail so old tail BD should point to new one */
		cpsw_cpdma_write_rxbd_next(sc->rxbd_tail,
			cpsw_cpdma_rxbd_paddr(i));

		/* Check if EOQ is reached */
		if (cpsw_cpdma_read_rxbd_flags(sc->rxbd_tail) & CPDMA_BD_EOQ) {
			cpsw_write_4(CPSW_CPDMA_RX_HDP(0), cpsw_cpdma_rxbd_paddr(i));
		}
		sc->rxbd_tail = i;

		/* read next descriptor */
		if (++i == CPSW_MAX_RX_BUFFERS)
			i = 0;
		cpsw_cpdma_read_rxbd(i, &bd);
		sc->rxbd_head = i;
	}

	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 1);
}

static void
cpsw_intr_tx(void *arg)
{
	struct cpsw_softc *sc = arg;
	CPSW_TX_LOCK(sc);
	cpsw_intr_tx_locked(arg);
	CPSW_TX_UNLOCK(sc);
}

static void
cpsw_intr_tx_locked(void *arg)
{
	struct cpsw_softc *sc = arg;
	uint32_t flags;

	if(sc->txbd_head == -1)
		return;

	if(sc->txbd_queue_size<1) {
		/* in some casses interrupt happens even when there is no
		   data in transmit queue */
		return;
	}

	/* Disable watchdog */
	sc->wd_timer = 0;

	flags = cpsw_cpdma_read_txbd_flags(sc->txbd_head);

	/* After BD is transmitted CPDMA will set OWNER to 0 */
	if (flags & CPDMA_BD_OWNER)
		return;

	if(flags & CPDMA_BD_EOQ)
		sc->eoq=1;

	/* release dmamap and mbuf */
	bus_dmamap_sync(sc->mbuf_dtag, sc->tx_dmamap[sc->txbd_head],
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->mbuf_dtag, sc->tx_dmamap[sc->txbd_head]);
	m_freem(sc->tx_mbuf[sc->txbd_head]);

	cpsw_write_4(CPSW_CPDMA_TX_CP(0), cpsw_cpdma_txbd_paddr(sc->txbd_head));

	if (++sc->txbd_head == CPSW_MAX_TX_BUFFERS)
		sc->txbd_head = 0;

	--sc->txbd_queue_size;

	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 2);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 1);
}

static void
cpsw_intr_misc(void *arg)
{
	struct cpsw_softc *sc = arg;
	uint32_t stat = cpsw_read_4(CPSW_WR_C_MISC_STAT(0));
	printf("%s: stat=%x\n",__func__,stat);
	/* EOI_RX_PULSE */
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 3);
}

static void
cpsw_tick(void *msc)
{
	struct cpsw_softc *sc = msc;

	/* Check for TX timeout */
	cpsw_watchdog(sc);

	mii_tick(sc->mii);

	/* Check for media type change */
	if(sc->cpsw_media_status != sc->mii->mii_media.ifm_media) {
		printf("%s: media type changed (ifm_media=%x)\n",__func__, 
			sc->mii->mii_media.ifm_media);
		cpsw_ifmedia_upd(sc->ifp);
	}

	/* Schedule another timeout one second from now */
	callout_reset(&sc->wd_callout, hz, cpsw_tick, sc);
}

static void
cpsw_watchdog(struct cpsw_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->ifp;

	CPSW_GLOBAL_LOCK(sc);

	if (sc->wd_timer == 0 || --sc->wd_timer) {
		CPSW_GLOBAL_UNLOCK(sc);
		return;
	}

	ifp->if_oerrors++;
	if_printf(ifp, "watchdog timeout\n");

	cpsw_stop(sc);
	cpsw_init_locked(sc);

	CPSW_GLOBAL_UNLOCK(sc);
}

static void
cpsw_init(void *arg)
{
	struct cpsw_softc *sc = arg;
	CPSW_GLOBAL_LOCK(sc);
	cpsw_init_locked(arg);
	CPSW_GLOBAL_UNLOCK(sc);
}

int once = 1;

static void
cpsw_init_locked(void *arg)
{
	struct ifnet *ifp;
	struct cpsw_softc *sc = arg;
	uint8_t  broadcast_address[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
	uint32_t next_bdp;
	uint32_t i;

	ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	printf("%s: start\n",__func__);

	/* Reset writer */
	cpsw_write_4(CPSW_WR_SOFT_RESET, 1);
	while(cpsw_read_4(CPSW_WR_SOFT_RESET) & 1);

	/* Reset SS */
	cpsw_write_4(CPSW_SS_SOFT_RESET, 1);
	while(cpsw_read_4(CPSW_SS_SOFT_RESET) & 1);

	/* Clear table (30) and enable ALE(31) */
	if (once)
		cpsw_write_4(CPSW_ALE_CONTROL, (3 << 30));
	else
		cpsw_write_4(CPSW_ALE_CONTROL, (1 << 31));
	once = 0; // FIXME

	/* Reset and init Sliver port 1 and 2 */
	for(i=0;i<2;i++) {
		/* Reset */
		cpsw_write_4(CPSW_SL_SOFT_RESET(i), 1);
		while(cpsw_read_4(CPSW_SL_SOFT_RESET(i)) & 1);
		/* Set Slave Mapping */
		cpsw_write_4(CPSW_SL_RX_PRI_MAP(i),0x76543210);
		cpsw_write_4(CPSW_PORT_P_TX_PRI_MAP(i+1),0x33221100);
		cpsw_write_4(CPSW_SL_RX_MAXLEN(i),0x5f2);
		/* Set MAC Address */
		cpsw_write_4(CPSW_PORT_P_SA_HI(i+1), sc->mac_addr[0] |
			(sc->mac_addr[1] <<  8) |
			(sc->mac_addr[2] << 16) |
			(sc->mac_addr[3] << 24));
		cpsw_write_4(CPSW_PORT_P_SA_LO(i+1), sc->mac_addr[4] |
			(sc->mac_addr[5] <<  8));

		/* Set MACCONTROL for ports 0,1: FULLDUPLEX(1), GMII_EN(5),
		   IFCTL_A(15), IFCTL_B(16) FIXME */
		cpsw_write_4(CPSW_SL_MACCONTROL(i), 1 | (1<<5) | (1<<15));

		/* Set ALE port to forwarding(3) */
		cpsw_write_4(CPSW_ALE_PORTCTL(i+1), 3);
	}

	/* Set Host Port Mapping */
	cpsw_write_4(CPSW_PORT_P0_CPDMA_TX_PRI_MAP, 0x76543210);
	cpsw_write_4(CPSW_PORT_P0_CPDMA_RX_CH_MAP, 0);

	/* Set ALE port to forwarding(3)*/
	cpsw_write_4(CPSW_ALE_PORTCTL(0), 3);

	/* Add own MAC address and broadcast to ALE */
	cpsw_ale_uc_entry_set(sc, 0, sc->mac_addr);
	cpsw_ale_mc_entry_set(sc, 7, broadcast_address);

	cpsw_write_4(CPSW_SS_PTYPE, 0);
	/* Enable statistics for ports 0, 1 and 2 */
	cpsw_write_4(CPSW_SS_STAT_PORT_EN, 7);

	/* Reset CPDMA */
	cpsw_write_4(CPSW_CPDMA_SOFT_RESET, 1);
	while(cpsw_read_4(CPSW_CPDMA_SOFT_RESET) & 1);

        for(i = 0; i < 8; i++) {
		cpsw_write_4(CPSW_CPDMA_TX_HDP(i), 0);
		cpsw_write_4(CPSW_CPDMA_RX_HDP(i), 0);
		cpsw_write_4(CPSW_CPDMA_TX_CP(i), 0);
		cpsw_write_4(CPSW_CPDMA_RX_CP(i), 0);
        }

	cpsw_write_4(CPSW_CPDMA_RX_FREEBUFFER(0), 0);

	/* Initialize RX Buffer Descriptors */
	i = CPSW_MAX_RX_BUFFERS;
	next_bdp = 0;
	while (i--) {
		cpsw_new_rxbuf(sc, i, next_bdp);
		/* Increment number of free RX buffers */
		//cpsw_write_4(CPSW_CPDMA_RX_FREEBUFFER(0), 1);
		next_bdp = cpsw_cpdma_rxbd_paddr(i);
	}

	sc->rxbd_head = 0;
	sc->rxbd_tail = CPSW_MAX_RX_BUFFERS-1;
	sc->txbd_head = 0;
	sc->eoq = 1;
	sc->txbd_queue_size = 0;

	/* Make IP hdr aligned with 4 */
	cpsw_write_4(CPSW_CPDMA_RX_BUFFER_OFFSET, 2);
	/* Write channel 0 RX HDP */
	cpsw_write_4(CPSW_CPDMA_RX_HDP(0), cpsw_cpdma_rxbd_paddr(0));

	/* Clear all interrupt Masks */
	cpsw_write_4(CPSW_CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	cpsw_write_4(CPSW_CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);

	/* Enable TX & RX DMA */
	cpsw_write_4(CPSW_CPDMA_TX_CONTROL, 1);
	cpsw_write_4(CPSW_CPDMA_RX_CONTROL, 1);

	/* Enable TX and RX interrupt receive for core 0 */
	cpsw_write_4(CPSW_WR_C_TX_EN(0), 0xFF);
	cpsw_write_4(CPSW_WR_C_RX_EN(0), 0xFF);
	//cpsw_write_4(CPSW_WR_C_MISC_EN(0), 0x3F);

	/* Enable host Error Interrupt */
	cpsw_write_4(CPSW_CPDMA_DMA_INTMASK_SET, 1);

	/* Enable interrupts for TX and RX Channel 0 */
	cpsw_write_4(CPSW_CPDMA_TX_INTMASK_SET, 1);
	cpsw_write_4(CPSW_CPDMA_RX_INTMASK_SET, 1);

	/* Ack stalled irqs */
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 0);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 1);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 2);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 3);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(MDIOCONTROL, (1<<30) | (1<<18) | 0xFF);

	/* Select MII in GMII_SEL, Internal Delay mode */
	//ti_scm_reg_write_4(0x650, 0);

	/* Activate network interface */
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	sc->wd_timer = 0;
	callout_reset(&sc->wd_callout, hz, cpsw_tick, sc);
}

static void
cpsw_ale_read_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry)
{
	cpsw_write_4(CPSW_ALE_TBLCTL, idx & 1023);
	ale_entry[0] = cpsw_read_4(CPSW_ALE_TBLW0);
	ale_entry[1] = cpsw_read_4(CPSW_ALE_TBLW1);
	ale_entry[2] = cpsw_read_4(CPSW_ALE_TBLW2);
}

static void
cpsw_ale_write_entry(struct cpsw_softc *sc, uint16_t idx, uint32_t *ale_entry)
{
	cpsw_write_4(CPSW_ALE_TBLW0, ale_entry[0]);
	cpsw_write_4(CPSW_ALE_TBLW1, ale_entry[1]);
	cpsw_write_4(CPSW_ALE_TBLW2, ale_entry[2]);
	cpsw_write_4(CPSW_ALE_TBLCTL, (idx & 1023) | (1 << 31));
}

static int
cpsw_ale_find_entry_by_mac(struct cpsw_softc *sc, uint8_t *mac)
{
	int i;
	uint32_t ale_entry[3];
	for(i=0; i< CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		if ((((ale_entry[1] >> 8) & 0xFF) == mac[0]) &&
		    (((ale_entry[1] >> 0) & 0xFF) == mac[1]) &&
		    (((ale_entry[0] >>24) & 0xFF) == mac[2]) &&
		    (((ale_entry[0] >>16) & 0xFF) == mac[3]) &&
		    (((ale_entry[0] >> 8) & 0xFF) == mac[4]) &&
		    (((ale_entry[0] >> 0) & 0xFF) == mac[5])) {
			return (i);
		}
	}
	return CPSW_MAX_ALE_ENTRIES;
}

static int
cpsw_ale_find_free_entry(struct cpsw_softc *sc)
{
	int i;
	uint32_t ale_entry[3];
	for(i=0; i< CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		/* Entry Type[61:60] is 0 for free entry */ 
		if (((ale_entry[1] >> 28) & 3) == 0) {
			return i;
		}
	}
	return CPSW_MAX_ALE_ENTRIES;
}


static int
cpsw_ale_uc_entry_set(struct cpsw_softc *sc, uint8_t port, uint8_t *mac)
{
	int i;
	uint32_t ale_entry[3];

	if ((i = cpsw_ale_find_entry_by_mac(sc, mac)) == CPSW_MAX_ALE_ENTRIES) {
		i = cpsw_ale_find_free_entry(sc);
	}

	if (i == CPSW_MAX_ALE_ENTRIES)
		return (ENOMEM);

	/* Set MAC address */
	ale_entry[0] = mac[2]<<24 | mac[3]<<16 | mac[4]<<8 | mac[5];
	ale_entry[1] = mac[0]<<8 | mac[1];

	/* Entry type[61:60] is addr entry(1) */
	ale_entry[1] |= 0x10<<24;

	/* Set portmask [67:66] */
	ale_entry[2] = (port & 3) << 2;

	cpsw_ale_write_entry(sc, i, ale_entry);

	return 0;
}

static int
cpsw_ale_mc_entry_set(struct cpsw_softc *sc, uint8_t portmap, uint8_t *mac)
{
	int i;
	uint32_t ale_entry[3];

	if ((i = cpsw_ale_find_entry_by_mac(sc, mac)) == CPSW_MAX_ALE_ENTRIES) {
		i = cpsw_ale_find_free_entry(sc);
	}

	if (i == CPSW_MAX_ALE_ENTRIES)
		return (ENOMEM);

	/* Set MAC address */
	ale_entry[0] = mac[2]<<24 | mac[3]<<16 | mac[4]<<8 | mac[5];
	ale_entry[1] = mac[0]<<8 | mac[1];

	/* Entry type[61:60] is addr entry(1), Mcast fwd state[63:62] is fw(3)*/
	ale_entry[1] |= 0xd0<<24;

	/* Set portmask [68:66] */
	ale_entry[2] = (portmap & 7) << 2;

	cpsw_ale_write_entry(sc, i, ale_entry);

	return 0;
}

#ifdef CPSW_DEBUG
static void
cpsw_ale_dump_table(struct cpsw_softc *sc) {
	int i;
	uint32_t ale_entry[3];
	for(i=0; i< CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		if (ale_entry[0] || ale_entry[1] || ale_entry[2]) {
			printf("ALE[%4u] %08x %08x %08x ", i, ale_entry[0],
				ale_entry[1],ale_entry[2]);
			printf("mac: %02x:%02x:%02x:%02x:%02x:%02x ",
				(ale_entry[1] >> 8) & 0xFF,
				(ale_entry[1] >> 0) & 0xFF,
				(ale_entry[0] >>24) & 0xFF,
				(ale_entry[0] >>16) & 0xFF,
				(ale_entry[0] >> 8) & 0xFF,
				(ale_entry[0] >> 0) & 0xFF);
			printf( ((ale_entry[1]>>8)&1) ? "mcast " : "ucast ");
			printf("type: %u ", (ale_entry[1]>>28)&3);
			printf("port: %u ", (ale_entry[2]>>2)&7);
			printf("\n");
		}
	}
}
#endif
