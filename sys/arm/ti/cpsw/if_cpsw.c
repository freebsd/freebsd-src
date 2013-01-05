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

#include "if_cpswreg.h"
#include "if_cpswvar.h"
 
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
static void cpsw_stop_locked(struct cpsw_softc *sc);
static int cpsw_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
static int cpsw_init_slot_lists(struct cpsw_softc *sc);
static void cpsw_free_slot(struct cpsw_softc *sc, struct cpsw_slot *slot);
static void cpsw_fill_rx_queue_locked(struct cpsw_softc *sc);
static void cpsw_tx_watchdog(struct cpsw_softc *sc);

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
	{ cpsw_intr_rx_thresh, "CPSW RX threshold interrupt" },
	{ cpsw_intr_rx,	"CPSW RX interrupt" },
	{ cpsw_intr_tx,	"CPSW TX interrupt" },
	{ cpsw_intr_misc, "CPSW misc interrupt" },
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


#include <machine/stdarg.h>
static void
cpsw_debugf_head(const char *funcname)
{
	int t = (int)(time_second % (24 * 60 * 60));

	printf("%02d:%02d:%02d %s ", t / (60 * 60), (t / 60) % 60, t % 60, funcname);
}

static void
cpsw_debugf(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");

}

#define CPSW_DEBUGF(a) do {						\
		if (sc->cpsw_if_flags & IFF_DEBUG) {			\
			cpsw_debugf_head(__func__);			\
			cpsw_debugf a;					\
		}							\
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
	struct cpsw_softc *sc = device_get_softc(dev);
	struct mii_softc *miisc;
	struct ifnet *ifp;
	void *phy_sc;
	int i, error, phy;
	uint32_t reg;

	CPSW_DEBUGF((""));

	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	/* Get phy address from fdt */
	if (fdt_get_phyaddr(sc->node, sc->dev, &phy, &phy_sc) != 0) {
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

	//cpsw_add_sysctls(sc); TODO

	/* Allocate a busdma tag and DMA safe memory for mbufs. */
	error = bus_dma_tag_create(
		bus_get_dma_tag(sc->dev),	/* parent */
		1, 0,				/* alignment, boundary */
		BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		BUS_SPACE_MAXADDR,		/* highaddr */
		NULL, NULL,			/* filtfunc, filtfuncarg */
		MCLBYTES, 1,			/* maxsize, nsegments */
		MCLBYTES, 0,			/* maxsegsz, flags */
		NULL, NULL,			/* lockfunc, lockfuncarg */
		&sc->mbuf_dtag);		/* dmatag */
	if (error) {
		device_printf(dev, "bus_dma_tag_create failed\n");
		cpsw_detach(dev);
		return (ENOMEM);
	}

	/* Initialize the tx_avail and rx_avail lists. */
	error = cpsw_init_slot_lists(sc);
	if (error) {
		device_printf(dev, "failed to allocate dmamaps\n");
		cpsw_detach(dev);
		return (ENOMEM);
	}

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

	/* Get high part of MAC address from control module (mac_id0_hi) */
	ti_scm_reg_read_4(0x634, &reg);
	sc->mac_addr[0] = reg & 0xFF;
	sc->mac_addr[1] = (reg >>  8) & 0xFF;
	sc->mac_addr[2] = (reg >> 16) & 0xFF;
	sc->mac_addr[3] = (reg >> 24) & 0xFF;

	/* Get low part of MAC address from control module (mac_id0_lo) */
	ti_scm_reg_read_4(0x630, &reg);
	sc->mac_addr[4] = reg & 0xFF;
	sc->mac_addr[5] = (reg >>  8) & 0xFF;

	ether_ifattach(ifp, sc->mac_addr);
	callout_init(&sc->wd_callout, 0);

	/* Initialze MDIO - ENABLE, PREAMBLE=0, FAULTENB, CLKDIV=0xFF */
	/* TODO Calculate MDCLK=CLK/(CLKDIV+1) */
	cpsw_write_4(MDIOCONTROL, 1 << 30 | 1 << 18 | 0xFF);

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
	cpsw_write_4(MDIOUSERPHYSEL0, 1 << 6 | (miisc->mii_phy & 0x1F));

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
	struct cpsw_softc *sc = device_get_softc(dev);
	int error, i;

	CPSW_DEBUGF((""));

	/* Stop controller and free TX queue */
	if (device_is_attached(dev)) {
		ether_ifdetach(sc->ifp);
		CPSW_GLOBAL_LOCK(sc);
		cpsw_stop_locked(sc);
		CPSW_GLOBAL_UNLOCK(sc);
		callout_drain(&sc->wd_callout);
	}

	bus_generic_detach(dev);
	device_delete_child(dev, sc->miibus);

	/* Stop and release all interrupts */
	for (i = 0; i < CPSW_INTR_COUNT; ++i) {
		if (!sc->ih_cookie[i])
			continue;

		error = bus_teardown_intr(dev, sc->res[1 + i], sc->ih_cookie[i]);
		if (error)
			device_printf(dev, "could not release %s\n",
			    cpsw_intrs[i + 1].description);
	}

	/* Free dmamaps and mbufs */
	for (i = 0; i < CPSW_MAX_TX_BUFFERS; i++) {
		cpsw_free_slot(sc, &sc->_tx_slots[i]);
	}
	for (i = 0; i < CPSW_MAX_RX_BUFFERS; i++) {
		cpsw_free_slot(sc, &sc->_rx_slots[i]);
	}

	/* Free DMA tag */
	error = bus_dma_tag_destroy(sc->mbuf_dtag);
	KASSERT(error == 0, ("Unable to destroy DMA tag"));

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
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK(sc);
	cpsw_stop_locked(sc);
	CPSW_GLOBAL_UNLOCK(sc);
	return (0);
}

static int
cpsw_resume(device_t dev)
{
	/* XXX TODO XXX */
	device_printf(dev, "%s\n", __FUNCTION__);
	return (0);
}

static int
cpsw_shutdown(device_t dev)
{
	struct cpsw_softc *sc = device_get_softc(dev);

	CPSW_DEBUGF((""));
	CPSW_GLOBAL_LOCK(sc);
	cpsw_stop_locked(sc);
	CPSW_GLOBAL_UNLOCK(sc);
	return (0);
}

static int
cpsw_miibus_ready(struct cpsw_softc *sc)
{
	uint32_t r, retries = CPSW_MIIBUS_RETRIES;

	while (--retries) {
		r = cpsw_read_4(MDIOUSERACCESS0);
		if ((r & 1 << 31) == 0)
			return 1;
		DELAY(CPSW_MIIBUS_DELAY);
	}
	return 0;
}

static int
cpsw_miibus_readreg(device_t dev, int phy, int reg)
{
	struct cpsw_softc *sc = device_get_softc(dev);
	uint32_t cmd, r;

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO not ready to read\n");
		return 0;
	}

	/* Set GO, reg, phy */
	cmd = 1 << 31 | (reg & 0x1F) << 21 | (phy & 0x1F) << 16;
	cpsw_write_4(MDIOUSERACCESS0, cmd);

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO timed out during read\n");
		return 0;
	}

	r = cpsw_read_4(MDIOUSERACCESS0);
	if((r & 1 << 29) == 0) {
		device_printf(dev, "Failed to read from PHY.\n");
		r = 0;
	}
	return (r & 0xFFFF);
}

static int
cpsw_miibus_writereg(device_t dev, int phy, int reg, int value)
{
	struct cpsw_softc *sc = device_get_softc(dev);
	uint32_t cmd;

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO not ready to write\n");
		return 0;
	}

	/* Set GO, WRITE, reg, phy, and value */
	cmd = 3 << 30 | (reg & 0x1F) << 21 | (phy & 0x1F) << 16
	    | (value & 0xFFFF);
	cpsw_write_4(MDIOUSERACCESS0, cmd);

	if (!cpsw_miibus_ready(sc)) {
		device_printf(dev, "MDIO timed out during write\n");
		return 0;
	}

	if((cpsw_read_4(MDIOUSERACCESS0) & (1 << 29)) == 0)
		device_printf(dev, "Failed to write to PHY.\n");

	return 0;
}

static int
cpsw_init_slot_lists(struct cpsw_softc *sc)
{
	int i;

	STAILQ_INIT(&sc->rx_active);
	STAILQ_INIT(&sc->rx_avail);
	STAILQ_INIT(&sc->tx_active);
	STAILQ_INIT(&sc->tx_avail);

	/* Put the slot descriptors onto the avail lists. */
	for (i = 0; i < CPSW_MAX_TX_BUFFERS; i++) {
		struct cpsw_slot *slot = &sc->_tx_slots[i];
		slot->index = i;
		/* XXX TODO: Remove this from here; allocate dmamaps lazily
		   in the encap routine to reduce memory usage. */
		if (bus_dmamap_create(sc->mbuf_dtag, 0, &slot->dmamap)) {
			if_printf(sc->ifp, "failed to create dmamap for tx mbuf\n");
			return (ENOMEM);
		}
		STAILQ_INSERT_TAIL(&sc->tx_avail, slot, next);
	}

	for (i = 0; i < CPSW_MAX_RX_BUFFERS; i++) {
		struct cpsw_slot *slot = &sc->_rx_slots[i];
		slot->index = i;
		if (bus_dmamap_create(sc->mbuf_dtag, 0, &slot->dmamap)) {
			if_printf(sc->ifp, "failed to create dmamap for rx mbuf\n");
			return (ENOMEM);
		}
		STAILQ_INSERT_TAIL(&sc->rx_avail, slot, next);
	}

	return (0);
}

static void
cpsw_free_slot(struct cpsw_softc *sc, struct cpsw_slot *slot)
{
	int error;

	if (slot->dmamap) {
		error = bus_dmamap_destroy(sc->mbuf_dtag, slot->dmamap);
		KASSERT(error == 0, ("Mapping still active"));
		slot->dmamap = NULL;
	}
	if (slot->mbuf) {
		m_freem(slot->mbuf);
		slot->mbuf = NULL;
	}
}

/*
 * Pad the packet to the minimum length for Ethernet.
 * (CPSW hardware doesn't do this for us.)
 */
static int
cpsw_pad(struct mbuf *m)
{
	int padlen = ETHER_MIN_LEN - m->m_pkthdr.len;
	struct mbuf *last, *n;

	if (padlen <= 0)
		return (0);

	/* If there's only the packet-header and we can pad there, use it. */
	if (m->m_pkthdr.len == m->m_len && M_WRITABLE(m) &&
	    M_TRAILINGSPACE(m) >= padlen) {
		last = m;
	} else {
		/*
		 * Walk packet chain to find last mbuf. We will either
		 * pad there, or append a new mbuf and pad it.
		 */
		for (last = m; last->m_next != NULL; last = last->m_next)
			;
		if (!(M_WRITABLE(last) && M_TRAILINGSPACE(last) >= padlen)) {
			/* Allocate new empty mbuf, pad it. Compact later. */
			MGET(n, M_NOWAIT, MT_DATA);
			if (n == NULL)
				return (ENOBUFS);
			n->m_len = 0;
			last->m_next = n;
			last = n;
		}
	}

	/* Now zero the pad area. */
	memset(mtod(last, caddr_t) + last->m_len, 0, padlen);
	last->m_len += padlen;
	m->m_pkthdr.len += padlen;

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
	bus_dma_segment_t seg[1];
	struct cpsw_cpdma_bd bd;
	struct cpsw_softc *sc = ifp->if_softc;
	struct cpsw_queue newslots = STAILQ_HEAD_INITIALIZER(newslots);
	struct cpsw_slot *slot, *prev_slot = NULL, *first_new_slot;
	struct mbuf *m0, *mtmp;
	int error, nsegs, enqueued = 0;

	CPSW_TX_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	/* Pull pending packets from IF queue and prep them for DMA. */
	for (;;) {
		slot = STAILQ_FIRST(&sc->tx_avail);
		if (slot == NULL) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		IF_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if ((error = cpsw_pad(m0))) {
			if_printf(ifp,
			    "%s: Dropping packet; could not pad\n", __func__);
			m_freem(m0);
			continue;
		}

		/* TODO: don't defragment here, queue each
		   packet fragment as a separate entry. */
		mtmp = m_defrag(m0, M_NOWAIT);
		if (mtmp)
			m0 = mtmp;

		slot->mbuf = m0;
		/* Create mapping in DMA memory */
		error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, slot->dmamap,
		    m0, seg, &nsegs, BUS_DMA_NOWAIT);
		KASSERT(nsegs == 1, ("More than one segment (nsegs=%d)", nsegs));
		KASSERT(error == 0, ("DMA error (error=%d)", error));
		if (error != 0 || nsegs != 1) {
			if_printf(ifp,
			    "%s: Can't load packet for DMA (nsegs=%d, error=%d), dropping packet\n",
			    __func__, nsegs, error);
			bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
			m_freem(m0);
			break;
		}
		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap,
				BUS_DMASYNC_PREWRITE);

		if (prev_slot != NULL)
			cpsw_cpdma_write_txbd_next(prev_slot->index,
			    cpsw_cpdma_txbd_paddr(slot->index));
		bd.next = 0;
		bd.bufptr = seg->ds_addr;
		bd.bufoff = 0;
		bd.buflen = seg->ds_len;
		bd.pktlen = seg->ds_len;
		bd.flags = 7 << 13;	/* Set OWNERSHIP, SOP, EOP */
		cpsw_cpdma_write_txbd(slot->index, &bd);
		++enqueued;

		prev_slot = slot;
		STAILQ_REMOVE_HEAD(&sc->tx_avail, next);
		STAILQ_INSERT_TAIL(&newslots, slot, next);
		BPF_MTAP(ifp, m0);
	}

	if (STAILQ_EMPTY(&newslots))
		return;

	/* Attach new segments to the hardware TX queue. */
	prev_slot = STAILQ_LAST(&sc->tx_active, cpsw_slot, next);
	first_new_slot = STAILQ_FIRST(&newslots);
	STAILQ_CONCAT(&sc->tx_active, &newslots);
	if (prev_slot == NULL) {
		/* Start the TX queue fresh. */
		cpsw_write_4(CPSW_CPDMA_TX_HDP(0),
			     cpsw_cpdma_txbd_paddr(first_new_slot->index));
	} else {
		/* Add packets to current queue. */
		/* Race: The hardware might have sent the last packet
		 * on the queue and stopped the transmitter just
		 * before we got here.  In that case, this is a no-op,
		 * but it also means there's a TX interrupt waiting
		 * to be processed as soon as we release the lock here.
		 * That TX interrupt can detect and recover from this
		 * situation; see cpsw_intr_tx_locked.
		 */
		cpsw_cpdma_write_txbd_next(prev_slot->index,
		   cpsw_cpdma_txbd_paddr(first_new_slot->index));
	}
	sc->tx_enqueues += enqueued;
	sc->tx_queued += enqueued;
	if (sc->tx_queued > sc->tx_max_queued) {
		sc->tx_max_queued = sc->tx_queued;
		CPSW_DEBUGF(("New TX high water mark %d", sc->tx_queued));
	}
}

static void
cpsw_stop_locked(struct cpsw_softc *sc)
{
	struct ifnet *ifp;
	int i;

	CPSW_DEBUGF((""));

	CPSW_GLOBAL_LOCK_ASSERT(sc);

	ifp = sc->ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	/* Disable interface */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* Stop tick engine */
	callout_stop(&sc->wd_callout);

	/* Wait for hardware to clear pending ops. */
	CPSW_GLOBAL_UNLOCK(sc);
	CPSW_DEBUGF(("starting RX and TX teardown"));
	cpsw_write_4(CPSW_CPDMA_RX_TEARDOWN, 0);
	cpsw_write_4(CPSW_CPDMA_TX_TEARDOWN, 0);
	i = 0;
	cpsw_intr_rx(sc); // Try clearing without delay.
	cpsw_intr_tx(sc);
	while (sc->rx_running || sc->tx_running) {
		DELAY(10);
		cpsw_intr_rx(sc);
		cpsw_intr_tx(sc);
		++i;
	}
	CPSW_DEBUGF(("finished RX and TX teardown (%d tries)", i));
	CPSW_GLOBAL_LOCK(sc);

	/* All slots are now available */
	STAILQ_CONCAT(&sc->rx_avail, &sc->rx_active);
	STAILQ_CONCAT(&sc->tx_avail, &sc->tx_active);
	CPSW_DEBUGF(("%d buffers dropped at TX reset", sc->tx_queued));
	sc->tx_queued = 0;

	/* Reset writer */
	cpsw_write_4(CPSW_WR_SOFT_RESET, 1);
	while (cpsw_read_4(CPSW_WR_SOFT_RESET) & 1)
		;

	/* Reset SS */
	cpsw_write_4(CPSW_SS_SOFT_RESET, 1);
	while (cpsw_read_4(CPSW_SS_SOFT_RESET) & 1)
		;

	/* Reset Sliver port 1 and 2 */
	for (i = 0; i < 2; i++) {
		/* Reset */
		cpsw_write_4(CPSW_SL_SOFT_RESET(i), 1);
		while (cpsw_read_4(CPSW_SL_SOFT_RESET(i)) & 1)
			;
	}

	/* Reset CPDMA */
	cpsw_write_4(CPSW_CPDMA_SOFT_RESET, 1);
	while (cpsw_read_4(CPSW_CPDMA_SOFT_RESET) & 1)
		;

	/* Disable TX & RX DMA */
	cpsw_write_4(CPSW_CPDMA_TX_CONTROL, 0);
	cpsw_write_4(CPSW_CPDMA_RX_CONTROL, 0);

	/* Disable TX and RX interrupts for all cores. */
	for (i = 0; i < 3; ++i) {
		cpsw_write_4(CPSW_WR_C_TX_EN(i), 0x00);
		cpsw_write_4(CPSW_WR_C_RX_EN(i), 0x00);
		cpsw_write_4(CPSW_WR_C_MISC_EN(i), 0x00);
	}

	/* Clear all interrupt Masks */
	cpsw_write_4(CPSW_CPDMA_RX_INTMASK_CLEAR, 0xFFFFFFFF);
	cpsw_write_4(CPSW_CPDMA_TX_INTMASK_CLEAR, 0xFFFFFFFF);
}

static void
cpsw_set_promisc(struct cpsw_softc *sc, int set)
{
	if (set) {
		printf("Promiscuous mode unimplemented\n");
	}
}

static void
cpsw_set_allmulti(struct cpsw_softc *sc, int set)
{
	if (set) {
		printf("All-multicast mode unimplemented\n");
	}
}

static int
cpsw_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error;
	uint32_t changed;

	CPSW_DEBUGF(("command=0x%lx", command));
	error = 0;

	switch (command) {
	case SIOCSIFFLAGS:
		CPSW_GLOBAL_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				changed = ifp->if_flags ^ sc->cpsw_if_flags;
				CPSW_DEBUGF(("SIOCSIFFLAGS: UP & RUNNING (changed=0x%x)", changed));
				if (changed & IFF_PROMISC)
					cpsw_set_promisc(sc,
					    ifp->if_flags & IFF_PROMISC);
				if (changed & IFF_ALLMULTI)
					cpsw_set_allmulti(sc,
					    ifp->if_flags & IFF_ALLMULTI);
			} else {
				CPSW_DEBUGF(("SIOCSIFFLAGS: UP but not RUNNING"));
				cpsw_init_locked(sc);
			}
		} else if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			CPSW_DEBUGF(("SIOCSIFFLAGS: not UP but RUNNING"));
			cpsw_stop_locked(sc);
		}

		sc->cpsw_if_flags = ifp->if_flags;
		CPSW_GLOBAL_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
		CPSW_DEBUGF(("SIOCADDMULTI unimplemented"));
		break;
	case SIOCDELMULTI:
		CPSW_DEBUGF(("SIOCDELMULTI unimplemented"));
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->mii->mii_media, command);
		break;
	default:
		CPSW_DEBUGF(("ether ioctl"));
		error = ether_ioctl(ifp, command, data);
	}
	return (error);
}

static void
cpsw_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct cpsw_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	CPSW_DEBUGF((""));
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

	CPSW_DEBUGF((""));
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
	struct cpsw_softc *sc = arg;
	CPSW_DEBUGF((""));
}

static void
cpsw_intr_rx(void *arg)
{
	struct cpsw_softc *sc = arg;

	CPSW_RX_LOCK(sc);
	cpsw_intr_rx_locked(arg);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 1);
	CPSW_RX_UNLOCK(sc);
}

static void
cpsw_intr_rx_locked(void *arg)
{
	struct cpsw_softc *sc = arg;
	struct cpsw_cpdma_bd bd;
	struct cpsw_slot *slot, *last_slot = NULL;
	struct ifnet *ifp;

	ifp = sc->ifp;
	if (!sc->rx_running)
		return;

	/* Pull completed packets off hardware RX queue. */
	slot = STAILQ_FIRST(&sc->rx_active);
	while (slot != NULL) {
		cpsw_cpdma_read_rxbd(slot->index, &bd);
		if (bd.flags & CPDMA_BD_OWNER)
			break; /* Still in use by hardware */

		if (bd.flags & CPDMA_BD_TDOWNCMPLT) {
			CPSW_DEBUGF(("RX teardown in progress"));
			cpsw_write_4(CPSW_CPDMA_RX_CP(0), 0xfffffffc);
			sc->rx_running = 0;
			return;
		}

		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);

		/* Fill mbuf */
		/* TODO: track SOP/EOP bits to assemble a full mbuf
		   out of received fragments. */
		slot->mbuf->m_hdr.mh_data += bd.bufoff;
		slot->mbuf->m_hdr.mh_len = bd.pktlen - 4;
		slot->mbuf->m_pkthdr.len = bd.pktlen - 4;
		slot->mbuf->m_flags |= M_PKTHDR;
		slot->mbuf->m_pkthdr.rcvif = ifp;

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			/* check for valid CRC by looking into pkt_err[5:4] */
			if ((bd.flags & CPDMA_BD_PKT_ERR_MASK) == 0) {
				slot->mbuf->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				slot->mbuf->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				slot->mbuf->m_pkthdr.csum_data = 0xffff;
			}
		}

		/* Handover packet */
		CPSW_RX_UNLOCK(sc);
		(*ifp->if_input)(ifp, slot->mbuf);
		slot->mbuf = NULL;
		CPSW_RX_LOCK(sc);

		last_slot = slot;
		STAILQ_REMOVE_HEAD(&sc->rx_active, next);
		STAILQ_INSERT_TAIL(&sc->rx_avail, slot, next);
		slot = STAILQ_FIRST(&sc->rx_active);
	}

	/* Tell hardware last slot we processed. */
	if (last_slot)
		cpsw_write_4(CPSW_CPDMA_RX_CP(0),
		    cpsw_cpdma_rxbd_paddr(last_slot->index));

	/* Repopulate hardware RX queue. */
	cpsw_fill_rx_queue_locked(sc);
}

static void
cpsw_fill_rx_queue_locked(struct cpsw_softc *sc)
{
	bus_dma_segment_t seg[1];
	struct cpsw_queue tmpqueue = STAILQ_HEAD_INITIALIZER(tmpqueue);
	struct cpsw_cpdma_bd bd;
	struct cpsw_slot *slot, *prev_slot, *next_slot;
	int error, nsegs;

	/* Try to allocate new mbufs. */
	STAILQ_FOREACH(slot, &sc->rx_avail, next) {
		if (slot->mbuf != NULL)
			continue;
		slot->mbuf = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
		if (slot->mbuf == NULL) {
			if_printf(sc->ifp, "Unable to fill RX queue\n");
			break;
		}
		slot->mbuf->m_len = slot->mbuf->m_pkthdr.len = slot->mbuf->m_ext.ext_size;
	}

	/* Register new mbufs with hardware. */
	prev_slot = NULL;
	while (!STAILQ_EMPTY(&sc->rx_avail)) {
		slot = STAILQ_FIRST(&sc->rx_avail);
		if (slot->mbuf == NULL)
			break;

		error = bus_dmamap_load_mbuf_sg(sc->mbuf_dtag, slot->dmamap,
		    slot->mbuf, seg, &nsegs, BUS_DMA_NOWAIT);

		KASSERT(nsegs == 1, ("More than one segment (nsegs=%d)", nsegs));
		KASSERT(error == 0, ("DMA error (error=%d)", error));
		if (nsegs != 1 || error) {
			if_printf(sc->ifp,
			    "%s: Can't prep RX buf for DMA (nsegs=%d, error=%d)\n",
			    __func__, nsegs, error);
			m_freem(slot->mbuf);
			slot->mbuf = NULL;
			break;
		}

		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap, BUS_DMASYNC_PREREAD);

		/* Create and submit new rx descriptor*/
		bd.next = 0;
		bd.bufptr = seg->ds_addr;
		bd.buflen = MCLBYTES-1;
		bd.bufoff = 2; /* make IP hdr aligned with 4 */
		bd.pktlen = 0;
		bd.flags = CPDMA_BD_OWNER;
		cpsw_cpdma_write_rxbd(slot->index, &bd);

		if (prev_slot) {
			cpsw_cpdma_write_rxbd_next(prev_slot->index,
			    cpsw_cpdma_rxbd_paddr(slot->index));
		}
		prev_slot = slot;
		STAILQ_REMOVE_HEAD(&sc->rx_avail, next);
		STAILQ_INSERT_TAIL(&tmpqueue, slot, next);
	}

	/* Link new entries to hardware RX queue. */
	prev_slot = STAILQ_LAST(&sc->rx_active, cpsw_slot, next);
	next_slot = STAILQ_FIRST(&tmpqueue);
	if (next_slot == NULL) {
		return;
	} else if (prev_slot == NULL) {
		/* Start a fresh RX queue. */
		cpsw_write_4(CPSW_CPDMA_RX_HDP(0),
		    cpsw_cpdma_rxbd_paddr(next_slot->index));
	} else {
		/* Extend an existing RX queue. */
		cpsw_cpdma_write_rxbd_next(prev_slot->index,
		    cpsw_cpdma_rxbd_paddr(next_slot->index));
		/* XXX Order matters: Previous write must complete
		   before next read begins in order to avoid an
		   end-of-queue race.  I think bus_write and bus_read have
		   sufficient barriers built-in to ensure this. XXX */
		/* If old RX queue was stopped, restart it. */
		if (cpsw_cpdma_read_rxbd_flags(prev_slot->index) & CPDMA_BD_EOQ) {
			cpsw_write_4(CPSW_CPDMA_RX_HDP(0),
			    cpsw_cpdma_rxbd_paddr(next_slot->index));
		}
	}
	STAILQ_CONCAT(&sc->rx_active, &tmpqueue);
}

static void
cpsw_intr_tx(void *arg)
{
	struct cpsw_softc *sc = arg;
	CPSW_TX_LOCK(sc);
	cpsw_intr_tx_locked(arg);
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 2);
	CPSW_TX_UNLOCK(sc);
}

static void
cpsw_intr_tx_locked(void *arg)
{
	struct cpsw_softc *sc = arg;
	struct cpsw_slot *slot, *last_slot = NULL;
	uint32_t flags, last_flags = 0, retires = 0;

	if (!sc->tx_running)
		return;

	slot = STAILQ_FIRST(&sc->tx_active);
	if (slot == NULL &&
	    cpsw_read_4(CPSW_CPDMA_TX_CP(0)) == 0xfffffffc) {
		CPSW_DEBUGF(("TX teardown of an empty queue"));
		cpsw_write_4(CPSW_CPDMA_TX_CP(0), 0xfffffffc);
		sc->tx_running = 0;
		return;
	}

	/* Pull completed segments off the hardware TX queue. */
	while (slot != NULL) {
		flags = cpsw_cpdma_read_txbd_flags(slot->index);
		if (flags & CPDMA_BD_OWNER)
			break; /* Hardware is still using this. */

		if (flags & CPDMA_BD_TDOWNCMPLT) {
			CPSW_DEBUGF(("TX teardown in progress"));
			cpsw_write_4(CPSW_CPDMA_TX_CP(0), 0xfffffffc);
			sc->tx_running = 0;
			return;
		}

		/* Release dmamap, free mbuf. */
		bus_dmamap_sync(sc->mbuf_dtag, slot->dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->mbuf_dtag, slot->dmamap);
		m_freem(slot->mbuf);
		slot->mbuf = NULL;

		STAILQ_REMOVE_HEAD(&sc->tx_active, next);
		STAILQ_INSERT_TAIL(&sc->tx_avail, slot, next);
		sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

		last_slot = slot;
		last_flags = flags;
		++retires;
		slot = STAILQ_FIRST(&sc->tx_active);
	}

	if (retires != 0) {
		/* Tell hardware the last item we dequeued. */
		cpsw_write_4(CPSW_CPDMA_TX_CP(0),
		     cpsw_cpdma_txbd_paddr(last_slot->index));
		/* If transmitter stopped and there's more, restart it. */
		/* This resolves the race described in tx_start above. */
		if ((last_flags & CPDMA_BD_EOQ) && (slot != NULL)) {
			cpsw_write_4(CPSW_CPDMA_TX_HDP(0),
			     cpsw_cpdma_txbd_paddr(slot->index));
		}
		sc->tx_retires += retires;
		sc->tx_queued -= retires;
	}
}

static void
cpsw_intr_misc(void *arg)
{
	struct cpsw_softc *sc = arg;
	uint32_t stat = cpsw_read_4(CPSW_WR_C_MISC_STAT(0));

	CPSW_DEBUGF(("stat=%x", stat));
	/* EOI_RX_PULSE */
	cpsw_write_4(CPSW_CPDMA_CPDMA_EOI_VECTOR, 3);
}

static void
cpsw_tick(void *msc)
{
	struct cpsw_softc *sc = msc;

	/* Check for TX timeout */
	cpsw_tx_watchdog(sc);

	mii_tick(sc->mii);

	/* Check for media type change */
	if(sc->cpsw_media_status != sc->mii->mii_media.ifm_media) {
		printf("%s: media type changed (ifm_media=%x)\n", __func__, 
			sc->mii->mii_media.ifm_media);
		cpsw_ifmedia_upd(sc->ifp);
	}

	/* Schedule another timeout one second from now */
	callout_reset(&sc->wd_callout, hz, cpsw_tick, sc);
}

static void
cpsw_tx_watchdog(struct cpsw_softc *sc)
{
	struct ifnet *ifp = sc->ifp;

	CPSW_GLOBAL_LOCK(sc);
	if (sc->tx_retires > sc->tx_retires_at_last_tick) {
		sc->tx_wd_timer = 0;  /* Stuff got sent. */
	} else if (sc->tx_queued == 0) {
		sc->tx_wd_timer = 0; /* Nothing to send. */
	} else {
		/* There was something to send but we didn't. */
		++sc->tx_wd_timer;
		if (sc->tx_wd_timer > 3) {
			sc->tx_wd_timer = 0;
			ifp->if_oerrors++;
			if_printf(ifp, "watchdog timeout\n");
			cpsw_stop_locked(sc);
			cpsw_init_locked(sc);
			CPSW_DEBUGF(("watchdog reset completed\n"));
		}
	}
	sc->tx_retires_at_last_tick = sc->tx_retires;
	CPSW_GLOBAL_UNLOCK(sc);
}

static void
cpsw_init(void *arg)
{
	struct cpsw_softc *sc = arg;

	CPSW_DEBUGF((""));
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
	uint32_t i;

	CPSW_DEBUGF((""));
	ifp = sc->ifp;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
		return;

	/* Reset writer */
	cpsw_write_4(CPSW_WR_SOFT_RESET, 1);
	while (cpsw_read_4(CPSW_WR_SOFT_RESET) & 1)
		;

	/* Reset SS */
	cpsw_write_4(CPSW_SS_SOFT_RESET, 1);
	while (cpsw_read_4(CPSW_SS_SOFT_RESET) & 1)
		;

	/* Clear table (30) and enable ALE(31) */
	if (once)
		cpsw_write_4(CPSW_ALE_CONTROL, 3 << 30);
	else
		cpsw_write_4(CPSW_ALE_CONTROL, 1 << 31);
	once = 0; // FIXME

	/* Reset and init Sliver port 1 and 2 */
	for (i = 0; i < 2; i++) {
		/* Reset */
		cpsw_write_4(CPSW_SL_SOFT_RESET(i), 1);
		while (cpsw_read_4(CPSW_SL_SOFT_RESET(i)) & 1)
			;
		/* Set Slave Mapping */
		cpsw_write_4(CPSW_SL_RX_PRI_MAP(i), 0x76543210);
		cpsw_write_4(CPSW_PORT_P_TX_PRI_MAP(i + 1), 0x33221100);
		cpsw_write_4(CPSW_SL_RX_MAXLEN(i), 0x5f2);
		/* Set MAC Address */
		cpsw_write_4(CPSW_PORT_P_SA_HI(i + 1),
			sc->mac_addr[3] << 24 |
			sc->mac_addr[2] << 16 |
			sc->mac_addr[1] << 8 |
			sc->mac_addr[0]);
		cpsw_write_4(CPSW_PORT_P_SA_LO(i+1),
			sc->mac_addr[5] << 8 |
			sc->mac_addr[4]);

		/* Set MACCONTROL for ports 0,1: FULLDUPLEX(1), GMII_EN(5),
		   IFCTL_A(15), IFCTL_B(16) FIXME */
		cpsw_write_4(CPSW_SL_MACCONTROL(i), 1 << 15 | 1 << 5 | 1);

		/* Set ALE port to forwarding(3) */
		cpsw_write_4(CPSW_ALE_PORTCTL(i + 1), 3);
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
	while (cpsw_read_4(CPSW_CPDMA_SOFT_RESET) & 1)
		;

	/* Make IP hdr aligned with 4 */
	cpsw_write_4(CPSW_CPDMA_RX_BUFFER_OFFSET, 2);

	for (i = 0; i < 8; i++) {
		cpsw_write_4(CPSW_CPDMA_TX_HDP(i), 0);
		cpsw_write_4(CPSW_CPDMA_RX_HDP(i), 0);
		cpsw_write_4(CPSW_CPDMA_TX_CP(i), 0);
		cpsw_write_4(CPSW_CPDMA_RX_CP(i), 0);
	}

	/* Initialize RX Buffer Descriptors */
	cpsw_write_4(CPSW_CPDMA_RX_FREEBUFFER(0), 0);
	cpsw_fill_rx_queue_locked(sc);

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
	cpsw_write_4(MDIOCONTROL, 1 << 30 | 1 << 18 | 0xFF);

	/* Select MII in GMII_SEL, Internal Delay mode */
	//ti_scm_reg_write_4(0x650, 0);

	/* Activate network interface */
	sc->rx_running = 1;
	sc->tx_running = 1;
	sc->tx_wd_timer = 0;
	callout_reset(&sc->wd_callout, hz, cpsw_tick, sc);
	sc->ifp->if_drv_flags |= IFF_DRV_RUNNING;
	sc->ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
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
	cpsw_write_4(CPSW_ALE_TBLCTL, 1 << 31 | (idx & 1023));
}

static int
cpsw_ale_find_entry_by_mac(struct cpsw_softc *sc, uint8_t *mac)
{
	int i;
	uint32_t ale_entry[3];
	for (i = 0; i < CPSW_MAX_ALE_ENTRIES; i++) {
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
	for (i = 0; i < CPSW_MAX_ALE_ENTRIES; i++) {
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
	ale_entry[0] = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
	ale_entry[1] = mac[0] << 8 | mac[1];

	/* Entry type[61:60] is addr entry(1) */
	ale_entry[1] |= 0x10 << 24;

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
	ale_entry[0] = mac[2] << 24 | mac[3] << 16 | mac[4] << 8 | mac[5];
	ale_entry[1] = mac[0] << 8 | mac[1];

	/* Entry type[61:60] is addr entry(1), Mcast fwd state[63:62] is fw(3)*/
	ale_entry[1] |= 0xd0 << 24;

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
	for (i = 0; i < CPSW_MAX_ALE_ENTRIES; i++) {
		cpsw_ale_read_entry(sc, i, ale_entry);
		if (ale_entry[0] || ale_entry[1] || ale_entry[2]) {
			printf("ALE[%4u] %08x %08x %08x ", i, ale_entry[0],
				ale_entry[1], ale_entry[2]);
			printf("mac: %02x:%02x:%02x:%02x:%02x:%02x ",
				(ale_entry[1] >> 8) & 0xFF,
				(ale_entry[1] >> 0) & 0xFF,
				(ale_entry[0] >>24) & 0xFF,
				(ale_entry[0] >>16) & 0xFF,
				(ale_entry[0] >> 8) & 0xFF,
				(ale_entry[0] >> 0) & 0xFF);
			printf(((ale_entry[1] >> 8) & 1) ? "mcast " : "ucast ");
			printf("type: %u ", (ale_entry[1] >> 28) & 3);
			printf("port: %u ", (ale_entry[2] >> 2) & 7);
			printf("\n");
		}
	}
}
#endif
