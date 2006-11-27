/*	$OpenBSD: if_nfe.c,v 1.54 2006/04/07 12:38:12 jsg Exp $	*/

/*-
 * Copyright (c) 2006 Shigeaki Tagashira <shigeaki@se.hiroshima-u.ac.jp>
 * Copyright (c) 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2005, 2006 Jonathan Gray <jsg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* Driver for NVIDIA nForce MCP Fast Ethernet and Gigabit Ethernet */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Uncomment the following line to enable polling. */
/* #define	DEVICE_POLLING */

#define	NFE_JUMBO
#define	NFE_CSUM
#define	NFE_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP)
#define	NVLAN 0

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/taskqueue.h>

#include <net/if.h>
#include <net/if_arp.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include <dev/nfe/if_nfereg.h>
#include <dev/nfe/if_nfevar.h>

MODULE_DEPEND(nfe, pci, 1, 1, 1);
MODULE_DEPEND(nfe, ether, 1, 1, 1);
MODULE_DEPEND(nfe, miibus, 1, 1, 1);
#include "miibus_if.h"

static int  nfe_probe(device_t);
static int  nfe_attach(device_t);
static int  nfe_detach(device_t);
static void nfe_shutdown(device_t);
static int  nfe_miibus_readreg(device_t, int, int);
static int  nfe_miibus_writereg(device_t, int, int, int);
static void nfe_miibus_statchg(device_t);
static int  nfe_ioctl(struct ifnet *, u_long, caddr_t);
static void nfe_intr(void *);
static void nfe_txdesc32_sync(struct nfe_softc *, struct nfe_desc32 *, int);
static void nfe_txdesc64_sync(struct nfe_softc *, struct nfe_desc64 *, int);
static void nfe_txdesc32_rsync(struct nfe_softc *, int, int, int);
static void nfe_txdesc64_rsync(struct nfe_softc *, int, int, int);
static void nfe_rxdesc32_sync(struct nfe_softc *, struct nfe_desc32 *, int);
static void nfe_rxdesc64_sync(struct nfe_softc *, struct nfe_desc64 *, int);
static void nfe_rxeof(struct nfe_softc *);
static void nfe_txeof(struct nfe_softc *);
static int  nfe_encap(struct nfe_softc *, struct mbuf *);
static void nfe_setmulti(struct nfe_softc *);
static void nfe_start(struct ifnet *);
static void nfe_start_locked(struct ifnet *);
static void nfe_watchdog(struct ifnet *);
static void nfe_init(void *);
static void nfe_init_locked(void *);
static void nfe_stop(struct ifnet *, int);
static int  nfe_alloc_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static void nfe_reset_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static void nfe_free_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static int  nfe_alloc_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static void nfe_reset_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static void nfe_free_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static int  nfe_ifmedia_upd(struct ifnet *);
static int  nfe_ifmedia_upd_locked(struct ifnet *);
static void nfe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static void nfe_tick(void *);
static void nfe_tick_locked(struct nfe_softc *);
static void nfe_get_macaddr(struct nfe_softc *, u_char *);
static void nfe_set_macaddr(struct nfe_softc *, u_char *);
static void nfe_dma_map_segs	(void *, bus_dma_segment_t *, int, int);
#ifdef DEVICE_POLLING
static void nfe_poll_locked(struct ifnet *, enum poll_cmd, int);
#endif

#ifdef NFE_DEBUG
int nfedebug = 0;
#define	DPRINTF(x)	do { if (nfedebug) printf x; } while (0)
#define	DPRINTFN(n,x)	do { if (nfedebug >= (n)) printf x; } while (0)
#else
#define	DPRINTF(x)
#define	DPRINTFN(n,x)
#endif

#define	NFE_LOCK(_sc)		mtx_lock(&(_sc)->nfe_mtx)
#define	NFE_UNLOCK(_sc)		mtx_unlock(&(_sc)->nfe_mtx)
#define	NFE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->nfe_mtx, MA_OWNED)

#define	letoh16(x) le16toh(x)

#define	NV_RID		0x10

static device_method_t nfe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nfe_probe),
	DEVMETHOD(device_attach,	nfe_attach),
	DEVMETHOD(device_detach,	nfe_detach),
	DEVMETHOD(device_shutdown,	nfe_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	nfe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	nfe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	nfe_miibus_statchg),

	{ 0, 0 }
};

static driver_t nfe_driver = {
	"nfe",
	nfe_methods,
	sizeof(struct nfe_softc)
};

static devclass_t nfe_devclass;

DRIVER_MODULE(nfe, pci, nfe_driver, nfe_devclass, 0, 0);
DRIVER_MODULE(miibus, nfe, miibus_driver, miibus_devclass, 0, 0);

static struct nfe_type nfe_devs[] = {
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE_LAN,
	    "NVIDIA nForce MCP Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE2_LAN,
	    "NVIDIA nForce2 MCP2 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN1,
	    "NVIDIA nForce2 400 MCP4 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE2_400_LAN2,
	    "NVIDIA nForce2 400 MCP5 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN1,
	    "NVIDIA nForce3 MCP3 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_250_LAN,
	    "NVIDIA nForce3 250 MCP6 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN4,
	    "NVIDIA nForce3 MCP7 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE4_LAN1,
	    "NVIDIA nForce4 CK804 MCP8 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE4_LAN2,
	    "NVIDIA nForce4 CK804 MCP9 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN1,
	    "NVIDIA nForce MCP04 Networking Adapter"},		// MCP10
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN2,
	    "NVIDIA nForce MCP04 Networking Adapter"},		// MCP11
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE430_LAN1,
	    "NVIDIA nForce 430 MCP12 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE430_LAN2,
	    "NVIDIA nForce 430 MCP13 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN1,
	    "NVIDIA nForce MCP55 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN2,
	    "NVIDIA nForce MCP55 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN1,
	    "NVIDIA nForce MCP61 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN2,
	    "NVIDIA nForce MCP61 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN3,
	    "NVIDIA nForce MCP61 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN2,
	    "NVIDIA nForce MCP61 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN1,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN2,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN3,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN2,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{0, 0, NULL}
};


/* Probe for supported hardware ID's */
static int
nfe_probe(device_t dev)
{
	struct nfe_type *t;

	t = nfe_devs;
	/* Check for matching PCI DEVICE ID's */
	while (t->name != NULL) {
		if ((pci_get_vendor(dev) == t->vid_id) &&
		    (pci_get_device(dev) == t->dev_id)) {
			device_set_desc(dev, t->name);
			return (0);
		}
		t++;
	}

	return (ENXIO);
}


static int
nfe_attach(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;
	int unit, error = 0, rid;

	sc = device_get_softc(dev);
	unit = device_get_unit(dev);
	sc->nfe_dev = dev;
	sc->nfe_unit = unit;

	mtx_init(&sc->nfe_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF | MTX_RECURSE);
	callout_init_mtx(&sc->nfe_stat_ch, &sc->nfe_mtx, 0);

	pci_enable_busmaster(dev);

	rid = NV_RID;
	sc->nfe_res = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid,
	    0, ~0, 1, RF_ACTIVE);

	if (sc->nfe_res == NULL) {
		printf ("nfe%d: couldn't map ports/memory\n", unit);
		error = ENXIO;
		goto fail;
	}

	sc->nfe_memt = rman_get_bustag(sc->nfe_res);
	sc->nfe_memh = rman_get_bushandle(sc->nfe_res);

	/* Allocate interrupt */
	rid = 0;
	sc->nfe_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
	    0, ~0, 1, RF_SHAREABLE | RF_ACTIVE);

	if (sc->nfe_irq == NULL) {
		printf("nfe%d: couldn't map interrupt\n", unit);
		error = ENXIO;
		goto fail;
	}

	nfe_get_macaddr(sc, sc->eaddr);

	sc->nfe_flags = 0;

	switch (pci_get_device(dev)) {
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN2:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN3:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN4:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN5:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_HW_CSUM;
		break;
	case PCI_PRODUCT_NVIDIA_MCP51_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP51_LAN2:
		sc->nfe_flags |= NFE_40BIT_ADDR;
		break;
	case PCI_PRODUCT_NVIDIA_CK804_LAN1:
	case PCI_PRODUCT_NVIDIA_CK804_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN2:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM;
		break;
	case PCI_PRODUCT_NVIDIA_MCP55_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP55_LAN2:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM |
		    NFE_HW_VLAN;
		break;
	case PCI_PRODUCT_NVIDIA_MCP61_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN4:
		sc->nfe_flags |= NFE_40BIT_ADDR;
		break;
	case PCI_PRODUCT_NVIDIA_MCP65_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN4:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR | NFE_HW_CSUM;
		break;
	}

	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
#define	NFE_NSEG_NEW 32
	error = bus_dma_tag_create(NULL,	/* parent */
	    1, 0,				/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    MAXBSIZE, NFE_NSEG_NEW,		/* maxsize, nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
	    BUS_DMA_ALLOCNOW,			/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->nfe_parent_tag);
	if (error)
		goto fail;

	ifp = sc->nfe_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		printf("nfe%d: can not if_alloc()\n", unit);
		error = ENOSPC;
		goto fail;
	}
	sc->nfe_mtu = ifp->if_mtu = ETHERMTU;

	/*
	 * Allocate Tx and Rx rings.
	 */
	if (nfe_alloc_tx_ring(sc, &sc->txq) != 0) {
		printf("nfe%d: could not allocate Tx ring\n", unit);
		error = ENXIO;
		goto fail;
	}

	if (nfe_alloc_rx_ring(sc, &sc->rxq) != 0) {
		printf("nfe%d: could not allocate Rx ring\n", unit);
		nfe_free_tx_ring(sc, &sc->txq);
		error = ENXIO;
		goto fail;
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nfe_ioctl;
	ifp->if_start = nfe_start;
	/* ifp->if_hwassist = NFE_CSUM_FEATURES; */
	ifp->if_watchdog = nfe_watchdog;
	ifp->if_init = nfe_init;
	ifp->if_baudrate = IF_Gbps(1);
	ifp->if_snd.ifq_maxlen = NFE_IFQ_MAXLEN;

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#ifdef NFE_JUMBO
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;
#else
	ifp->if_capabilities &= ~IFCAP_JUMBO_MTU;
	sc->nfe_flags &= ~NFE_JUMBO_SUP;
#endif

#if NVLAN > 0
	if (sc->nfe_flags & NFE_HW_VLAN)
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif
#ifdef NFE_CSUM
	if (sc->nfe_flags & NFE_HW_CSUM) {
		ifp->if_capabilities |= IFCAP_HWCSUM;
	}
#endif
	ifp->if_capenable = ifp->if_capabilities;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->nfe_miibus, nfe_ifmedia_upd,
	    nfe_ifmedia_sts)) {
		printf("nfe%d: MII without any phy!\n", unit);
		error = ENXIO;
		goto fail;
	}

	ether_ifattach(ifp, sc->eaddr);

	error = bus_setup_intr(dev, sc->nfe_irq, INTR_TYPE_NET | INTR_MPSAFE,
	    nfe_intr, sc, &sc->nfe_intrhand);

	if (error) {
		printf("nfe%d: couldn't set up irq\n", unit);
		ether_ifdetach(ifp);
		goto fail;
	}

fail:
	if (error)
		nfe_detach(dev);

	return (error);
}


static int
nfe_detach(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;
	u_char eaddr[ETHER_ADDR_LEN];
	int i;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->nfe_mtx), ("nfe mutex not initialized"));
	ifp = sc->nfe_ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		eaddr[i] = sc->eaddr[5 - i];
	}
	nfe_set_macaddr(sc, eaddr);

	if (device_is_attached(dev)) {
		NFE_LOCK(sc);
		nfe_stop(ifp, 1);
		ifp->if_flags &= ~IFF_UP;
		NFE_UNLOCK(sc);
		callout_drain(&sc->nfe_stat_ch);
		ether_ifdetach(ifp);
	}

	if (ifp)
		if_free(ifp);
	if (sc->nfe_miibus)
		device_delete_child(dev, sc->nfe_miibus);
	bus_generic_detach(dev);

	if (sc->nfe_intrhand)
		bus_teardown_intr(dev, sc->nfe_irq, sc->nfe_intrhand);
	if (sc->nfe_irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->nfe_irq);
	if (sc->nfe_res)
		bus_release_resource(dev, SYS_RES_MEMORY, NV_RID, sc->nfe_res);

	nfe_free_tx_ring(sc, &sc->txq);
	nfe_free_rx_ring(sc, &sc->rxq);

	if (sc->nfe_parent_tag)
		bus_dma_tag_destroy(sc->nfe_parent_tag);

	mtx_destroy(&sc->nfe_mtx);

	return (0);
}


static void
nfe_miibus_statchg(device_t dev)
{
	struct nfe_softc *sc;
	struct mii_data *mii;
	u_int32_t phy, seed, misc = NFE_MISC1_MAGIC, link = NFE_MEDIA_SET;

	sc = device_get_softc(dev);
	mii = device_get_softc(sc->nfe_miibus);

	phy = NFE_READ(sc, NFE_PHY_IFACE);
	phy &= ~(NFE_PHY_HDX | NFE_PHY_100TX | NFE_PHY_1000T);

	seed = NFE_READ(sc, NFE_RNDSEED);
	seed &= ~NFE_SEED_MASK;

	if ((mii->mii_media_active & IFM_GMASK) == IFM_HDX) {
		phy  |= NFE_PHY_HDX;	/* half-duplex */
		misc |= NFE_MISC1_HDX;
	}

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:	/* full-duplex only */
		link |= NFE_MEDIA_1000T;
		seed |= NFE_SEED_1000T;
		phy  |= NFE_PHY_1000T;
		break;
	case IFM_100_TX:
		link |= NFE_MEDIA_100TX;
		seed |= NFE_SEED_100TX;
		phy  |= NFE_PHY_100TX;
		break;
	case IFM_10_T:
		link |= NFE_MEDIA_10T;
		seed |= NFE_SEED_10T;
		break;
	}

	NFE_WRITE(sc, NFE_RNDSEED, seed);	/* XXX: gigabit NICs only? */

	NFE_WRITE(sc, NFE_PHY_IFACE, phy);
	NFE_WRITE(sc, NFE_MISC1, misc);
	NFE_WRITE(sc, NFE_LINKSPEED, link);
}


static int
nfe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct nfe_softc *sc = device_get_softc(dev);
	u_int32_t val;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_CTL, (phy << NFE_PHYADD_SHIFT) | reg);

	for (ntries = 0; ntries < 1000; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
	if (ntries == 1000) {
		DPRINTFN(2, ("nfe%d: timeout waiting for PHY\n", sc->nfe_unit));
		return 0;
	}

	if (NFE_READ(sc, NFE_PHY_STATUS) & NFE_PHY_ERROR) {
		DPRINTFN(2, ("nfe%d: could not read PHY\n", sc->nfe_unit));
		return 0;
	}

	val = NFE_READ(sc, NFE_PHY_DATA);
	if (val != 0xffffffff && val != 0)
		sc->mii_phyaddr = phy;

	DPRINTFN(2, ("nfe%d: mii read phy %d reg 0x%x ret 0x%x\n",
	    sc->nfe_unit, phy, reg, val));

	return val;
}


static int
nfe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct nfe_softc *sc = device_get_softc(dev);
	u_int32_t ctl;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_DATA, val);
	ctl = NFE_PHY_WRITE | (phy << NFE_PHYADD_SHIFT) | reg;
	NFE_WRITE(sc, NFE_PHY_CTL, ctl);

	for (ntries = 0; ntries < 1000; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
#ifdef NFE_DEBUG
	if (nfedebug >= 2 && ntries == 1000)
		printf("could not write to PHY\n");
#endif
	return 0;
}


static int
nfe_alloc_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	void **desc;
	bus_addr_t physaddr;
	int i, error, descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = (void **)&ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->cur = ring->next = 0;
	ring->bufsz = (sc->nfe_mtu + NFE_RX_HEADERS <= MCLBYTES) ?
	    MCLBYTES : MJUM9BYTES;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	   PAGE_SIZE, 0,			/* alignment, boundary */
	   BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	   BUS_SPACE_MAXADDR,			/* highaddr */
	   NULL, NULL,				/* filter, filterarg */
	   NFE_RX_RING_COUNT * descsize, 1,	/* maxsize, nsegments */
	   NFE_RX_RING_COUNT * descsize,	/* maxsegsize */
	   BUS_DMA_ALLOCNOW,			/* flags */
	   NULL, NULL,				/* lockfunc, lockarg */
	   &ring->rx_desc_tag);
	if (error != 0) {
		printf("nfe%d: could not create desc DMA tag\n", sc->nfe_unit);
		goto fail;
	}

	/* allocate memory to desc */
	error = bus_dmamem_alloc(ring->rx_desc_tag, (void **)desc,
	    BUS_DMA_NOWAIT, &ring->rx_desc_map);
	if (error != 0) {
		printf("nfe%d: could not create desc DMA map\n", sc->nfe_unit);
		goto fail;
	}

	/* map desc to device visible address space */
	error = bus_dmamap_load(ring->rx_desc_tag, ring->rx_desc_map, *desc,
	    NFE_RX_RING_COUNT * descsize, nfe_dma_map_segs,
	    &ring->rx_desc_segs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("nfe%d: could not load desc DMA map\n", sc->nfe_unit);
		goto fail;
	}

	bzero(*desc, NFE_RX_RING_COUNT * descsize);
	ring->rx_desc_addr = ring->rx_desc_segs.ds_addr;
	ring->physaddr = ring->rx_desc_addr;

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		data = &sc->rxq.data[i];

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("nfe%d: could not allocate rx mbuf\n",
			    sc->nfe_unit);
			error = ENOMEM;
			goto fail;
		}

			error = bus_dma_tag_create(sc->nfe_parent_tag,
			    ETHER_ALIGN, 0,	       /* alignment, boundary */
			    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
			    BUS_SPACE_MAXADDR,		/* highaddr */
			    NULL, NULL,		/* filter, filterarg */
			    MCLBYTES, 1,		/* maxsize, nsegments */
			    MCLBYTES,			/* maxsegsize */
			    BUS_DMA_ALLOCNOW,		/* flags */
			    NULL, NULL,		/* lockfunc, lockarg */
			    &data->rx_data_tag);
			if (error != 0) {
				printf("nfe%d: could not create DMA map\n",
				    sc->nfe_unit);
				goto fail;
			}

			error = bus_dmamap_create(data->rx_data_tag, 0,
			    &data->rx_data_map);
			if (error != 0) {
				printf("nfe%d: could not allocate mbuf cluster\n",
				    sc->nfe_unit);
				goto fail;
			}

			MCLGET(data->m, M_DONTWAIT);
			if (!(data->m->m_flags & M_EXT)) {
				error = ENOMEM;
				goto fail;
			}

			error = bus_dmamap_load(data->rx_data_tag,
			    data->rx_data_map, mtod(data->m, void *),
			    /*DEO,MCLBYTES*/ring->bufsz, nfe_dma_map_segs, &data->rx_data_segs,
			    BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("nfe%d: could not load rx buf DMA map\n",
				    sc->nfe_unit);
				goto fail;
			}

			data->rx_data_addr = data->rx_data_segs.ds_addr;
			physaddr = data->rx_data_addr;


		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->rxq.desc64[i];
#if defined(__LP64__)
			desc64->physaddr[0] = htole32(physaddr >> 32);
#endif
			desc64->physaddr[1] = htole32(physaddr & 0xffffffff);
			desc64->length = htole16(sc->rxq.bufsz);
			desc64->flags = htole16(NFE_RX_READY);
		} else {
			desc32 = &sc->rxq.desc32[i];
			desc32->physaddr = htole32(physaddr);
			desc32->length = htole16(sc->rxq.bufsz);
			desc32->flags = htole16(NFE_RX_READY);
		}

	}

	bus_dmamap_sync(ring->rx_desc_tag, ring->rx_desc_map,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	nfe_free_rx_ring(sc, ring);

	return error;
}


static void
nfe_reset_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	int i;

	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			ring->desc64[i].length = htole16(ring->bufsz);
			ring->desc64[i].flags = htole16(NFE_RX_READY);
		} else {
			ring->desc32[i].length = htole16(ring->bufsz);
			ring->desc32[i].flags = htole16(NFE_RX_READY);
		}
	}

	bus_dmamap_sync(ring->rx_desc_tag, ring->rx_desc_map,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}


static void
nfe_free_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_rx_data *data;
	void *desc;
	int i, descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	if (desc != NULL) {
		bus_dmamap_sync(ring->rx_desc_tag, ring->rx_desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->rx_desc_tag, ring->rx_desc_map);
		bus_dmamem_free(ring->rx_desc_tag, desc, ring->rx_desc_map);
		bus_dma_tag_destroy(ring->rx_desc_tag);
	}


		for (i = 0; i < NFE_RX_RING_COUNT; i++) {
			data = &ring->data[i];

			if (data->rx_data_map != NULL) {
				bus_dmamap_sync(data->rx_data_tag,
				    data->rx_data_map, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(data->rx_data_tag,
				    data->rx_data_map);
				bus_dmamap_destroy(data->rx_data_tag,
				    data->rx_data_map);
				bus_dma_tag_destroy(data->rx_data_tag);
			}

			if (data->m != NULL)
				m_freem(data->m);
		}
}


static int
nfe_alloc_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	int i, error;
	void **desc;
	int descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = (void **)&ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	   PAGE_SIZE, 0,			/* alignment, boundary */
	   BUS_SPACE_MAXADDR_32BIT,		/* lowaddr */
	   BUS_SPACE_MAXADDR,			/* highaddr */
	   NULL, NULL,				/* filter, filterarg */
	   NFE_TX_RING_COUNT * descsize, 1,	/* maxsize, nsegments */
	   NFE_TX_RING_COUNT * descsize,	/* maxsegsize */
	   BUS_DMA_ALLOCNOW,			/* flags */
	   NULL, NULL,				/* lockfunc, lockarg */
	   &ring->tx_desc_tag);
	if (error != 0) {
		printf("nfe%d: could not create desc DMA tag\n", sc->nfe_unit);
		goto fail;
	}

	error = bus_dmamem_alloc(ring->tx_desc_tag, (void **)desc,
	    BUS_DMA_NOWAIT, &ring->tx_desc_map);
	if (error != 0) {
		printf("nfe%d: could not create desc DMA map\n", sc->nfe_unit);
		goto fail;
	}

	error = bus_dmamap_load(ring->tx_desc_tag, ring->tx_desc_map, *desc,
	    NFE_TX_RING_COUNT * descsize, nfe_dma_map_segs, &ring->tx_desc_segs,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("nfe%d: could not load desc DMA map\n", sc->nfe_unit);
		goto fail;
	}

	bzero(*desc, NFE_TX_RING_COUNT * descsize);

	ring->tx_desc_addr = ring->tx_desc_segs.ds_addr;
	ring->physaddr = ring->tx_desc_addr;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	   ETHER_ALIGN, 0,
	   BUS_SPACE_MAXADDR_32BIT,
	   BUS_SPACE_MAXADDR,
	   NULL, NULL,
	   NFE_JBYTES, NFE_MAX_SCATTER,
	   NFE_JBYTES,
	   BUS_DMA_ALLOCNOW,
	   NULL, NULL,
	   &ring->tx_data_tag);
	if (error != 0) {
	  printf("nfe%d: could not create DMA tag\n", sc->nfe_unit);
	  goto fail;
	}

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		error = bus_dmamap_create(ring->tx_data_tag, 0,
		    &ring->data[i].tx_data_map);
		if (error != 0) {
			printf("nfe%d: could not create DMA map\n",
			    sc->nfe_unit);
			goto fail;
		}
	}

	return 0;

fail:	nfe_free_tx_ring(sc, ring);
	return error;
}


static void
nfe_reset_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_tx_data *data;
	int i;

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		if (sc->nfe_flags & NFE_40BIT_ADDR)
			ring->desc64[i].flags = 0;
		else
			ring->desc32[i].flags = 0;

		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->tx_data_tag, data->active,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->tx_data_tag, data->active);
			m_freem(data->m);
			data->m = NULL;
		}
	}

	bus_dmamap_sync(ring->tx_desc_tag, ring->tx_desc_map,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = 0;
}


static void
nfe_free_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_tx_data *data;
	void *desc;
	int i, descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	if (desc != NULL) {
		bus_dmamap_sync(ring->tx_desc_tag, ring->tx_desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->tx_desc_tag, ring->tx_desc_map);
		bus_dmamem_free(ring->tx_desc_tag, desc, ring->tx_desc_map);
		bus_dma_tag_destroy(ring->tx_desc_tag);
	}

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->tx_data_tag, data->active,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->tx_data_tag, data->active);
			m_freem(data->m);
		}
	}

	/* ..and now actually destroy the DMA mappings */
	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		data = &ring->data[i];
		if (data->tx_data_map == NULL)
			continue;
		bus_dmamap_destroy(ring->tx_data_tag, data->tx_data_map);
	}

	bus_dma_tag_destroy(ring->tx_data_tag);
}

#ifdef DEVICE_POLLING
static poll_handler_t nfe_poll;


static void
nfe_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct nfe_softc *sc = ifp->if_softc;

	NFE_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		nfe_poll_locked(ifp, cmd, count);
	NFE_UNLOCK(sc);
}


static void
nfe_poll_locked(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct nfe_softc *sc = ifp->if_softc;
	u_int32_t r;

	NFE_LOCK_ASSERT(sc);

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		return;
	}

	sc->rxcycles = count;
	nfe_rxeof(sc);
	nfe_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		nfe_start_locked(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) {
		if ((r = NFE_READ(sc, NFE_IRQ_STATUS)) == 0) {
			return;
		}
		NFE_WRITE(sc, NFE_IRQ_STATUS, r);

		if (r & NFE_IRQ_LINK) {
			NFE_READ(sc, NFE_PHY_STATUS);
			NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);
			DPRINTF(("nfe%d: link state changed\n", sc->nfe_unit));
		}
	}
}
#endif /* DEVICE_POLLING */


static int
nfe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct mii_data *mii;
	int error = 0;

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu == ifp->if_mtu) {
			error = EINVAL;
			break;
		}
		if ((sc->nfe_flags & NFE_JUMBO_SUP) && (ifr->ifr_mtu >=
		    ETHERMIN && ifr->ifr_mtu <= NV_PKTLIMIT_2)) {
			NFE_LOCK(sc);
			sc->nfe_mtu = ifp->if_mtu = ifr->ifr_mtu;
			nfe_stop(ifp, 1);
			nfe_free_tx_ring(sc, &sc->txq);
			nfe_free_rx_ring(sc, &sc->rxq);
			NFE_UNLOCK(sc);

			/* Reallocate Tx and Rx rings. */
			if (nfe_alloc_tx_ring(sc, &sc->txq) != 0) {
				printf("nfe%d: could not allocate Tx ring\n",
				    sc->nfe_unit);
				error = ENXIO;
				break;
			}

			if (nfe_alloc_rx_ring(sc, &sc->rxq) != 0) {
				printf("nfe%d: could not allocate Rx ring\n",
				    sc->nfe_unit);
				nfe_free_tx_ring(sc, &sc->txq);
				error = ENXIO;
				break;
			}
			NFE_LOCK(sc);
			nfe_init_locked(sc);
			NFE_UNLOCK(sc);
		} else {
			error = EINVAL;
		}
		break;
	case SIOCSIFFLAGS:
		NFE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			/*
			 * If only the PROMISC or ALLMULTI flag changes, then
			 * don't do a full re-init of the chip, just update
			 * the Rx filter.
			 */
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
			    ((ifp->if_flags ^ sc->nfe_if_flags) &
			     (IFF_ALLMULTI | IFF_PROMISC)) != 0)
				nfe_setmulti(sc);
			else
				nfe_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				nfe_stop(ifp, 1);
		}
		sc->nfe_if_flags = ifp->if_flags;
		NFE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			NFE_LOCK(sc);
			nfe_setmulti(sc);
			NFE_UNLOCK(sc);
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		mii = device_get_softc(sc->nfe_miibus);
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, cmd);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(nfe_poll, ifp);
				if (error)
					return(error);
				NFE_LOCK(sc);
				NFE_WRITE(sc, NFE_IRQ_MASK, 0);
				ifp->if_capenable |= IFCAP_POLLING;
				NFE_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				NFE_LOCK(sc);
				NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED);
				ifp->if_capenable &= ~IFCAP_POLLING;
				NFE_UNLOCK(sc);
			}
		}
#endif /* DEVICE_POLLING */
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if (IFCAP_HWCSUM & ifp->if_capenable &&
			    IFCAP_HWCSUM & ifp->if_capabilities)
				ifp->if_hwassist = NFE_CSUM_FEATURES;
			else
				ifp->if_hwassist = 0;
		}
	}
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return error;
}


static void
nfe_intr(void *arg)
{
	struct nfe_softc *sc = arg;
	struct ifnet *ifp = sc->nfe_ifp;
	u_int32_t r;

	NFE_LOCK(sc);

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		NFE_UNLOCK(sc);
		return;
	}
#endif

	if ((r = NFE_READ(sc, NFE_IRQ_STATUS)) == 0) {
		NFE_UNLOCK(sc);
		return;	/* not for us */
	}
	NFE_WRITE(sc, NFE_IRQ_STATUS, r);

	DPRINTFN(5, ("nfe_intr: interrupt register %x\n", r));

	NFE_WRITE(sc, NFE_IRQ_MASK, 0);

	if (r & NFE_IRQ_LINK) {
		NFE_READ(sc, NFE_PHY_STATUS);
		NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);
		DPRINTF(("nfe%d: link state changed\n", sc->nfe_unit));
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		/* check Rx ring */
		nfe_rxeof(sc);
		/* check Tx ring */
		nfe_txeof(sc);
	}

	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		nfe_start_locked(ifp);

	NFE_UNLOCK(sc);

	return;
}


static void
nfe_txdesc32_sync(struct nfe_softc *sc, struct nfe_desc32 *desc32, int ops)
{

	bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map, ops);
}


static void
nfe_txdesc64_sync(struct nfe_softc *sc, struct nfe_desc64 *desc64, int ops)
{

	bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map, ops);
}


static void
nfe_txdesc32_rsync(struct nfe_softc *sc, int start, int end, int ops)
{

	bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map, ops);
}


static void
nfe_txdesc64_rsync(struct nfe_softc *sc, int start, int end, int ops)
{

	bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map, ops);
}


static void
nfe_rxdesc32_sync(struct nfe_softc *sc, struct nfe_desc32 *desc32, int ops)
{

	bus_dmamap_sync(sc->rxq.rx_desc_tag, sc->rxq.rx_desc_map, ops);
}


static void
nfe_rxdesc64_sync(struct nfe_softc *sc, struct nfe_desc64 *desc64, int ops)
{

	bus_dmamap_sync(sc->rxq.rx_desc_tag, sc->rxq.rx_desc_map, ops);
}


static void
nfe_rxeof(struct nfe_softc *sc)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct nfe_desc32 *desc32=NULL;
	struct nfe_desc64 *desc64=NULL;
	struct nfe_rx_data *data;
	struct mbuf *m, *mnew;
	bus_addr_t physaddr;
	u_int16_t flags;
	int error, len;
#if NVLAN > 1
	u_int16_t vlan_tag = 0;
	int have_tag = 0;
#endif

	NFE_LOCK_ASSERT(sc);

	for (;;) {

#ifdef DEVICE_POLLING
		if (ifp->if_capenable & IFCAP_POLLING) {
			if (sc->rxcycles <= 0)
				break;
			sc->rxcycles--;
		}
#endif

		data = &sc->rxq.data[sc->rxq.cur];

		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->rxq.desc64[sc->rxq.cur];
			nfe_rxdesc64_sync(sc, desc64, BUS_DMASYNC_POSTREAD);

			flags = letoh16(desc64->flags);
			len = letoh16(desc64->length) & 0x3fff;

#if NVLAN > 1
			if (flags & NFE_TX_VLAN_TAG) {
				have_tag = 1;
				vlan_tag = desc64->vtag;
			}
#endif

		} else {
			desc32 = &sc->rxq.desc32[sc->rxq.cur];
			nfe_rxdesc32_sync(sc, desc32, BUS_DMASYNC_POSTREAD);

			flags = letoh16(desc32->flags);
			len = letoh16(desc32->length) & 0x3fff;
		}

		if (flags & NFE_RX_READY)
			break;

		if ((sc->nfe_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if (!(flags & NFE_RX_VALID_V1))
				goto skip;
			if ((flags & NFE_RX_FIXME_V1) == NFE_RX_FIXME_V1) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		} else {
			if (!(flags & NFE_RX_VALID_V2))
				goto skip;

			if ((flags & NFE_RX_FIXME_V2) == NFE_RX_FIXME_V2) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		}

		if (flags & NFE_RX_ERROR) {
			ifp->if_ierrors++;
			goto skip;
		}

		/*
		 * Try to allocate a new mbuf for this ring element and load
		 * it before processing the current mbuf. If the ring element
		 * cannot be loaded, drop the received packet and reuse the
		 * old mbuf. In the unlikely case that the old mbuf can't be
		 * reloaded either, explicitly panic.
		 */
		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			ifp->if_ierrors++;
			goto skip;
		}

			MCLGET(mnew, M_DONTWAIT);
			if (!(mnew->m_flags & M_EXT)) {
				m_freem(mnew);
				ifp->if_ierrors++;
				goto skip;
			}

			bus_dmamap_sync(data->rx_data_tag, data->rx_data_map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(data->rx_data_tag, data->rx_data_map);
			error = bus_dmamap_load(data->rx_data_tag,
			    data->rx_data_map, mtod(mnew, void *), MCLBYTES,
			    nfe_dma_map_segs, &data->rx_data_segs,
			    BUS_DMA_NOWAIT);
			if (error != 0) {
				m_freem(mnew);

				/* try to reload the old mbuf */
				error = bus_dmamap_load(data->rx_data_tag,
				    data->rx_data_map, mtod(data->m, void *),
				    MCLBYTES, nfe_dma_map_segs,
				    &data->rx_data_segs, BUS_DMA_NOWAIT);
				if (error != 0) {
					/* very unlikely that it will fail.. */
				      panic("nfe%d: could not load old rx mbuf",
					    sc->nfe_unit);
				}
				ifp->if_ierrors++;
				goto skip;
			}
			data->rx_data_addr = data->rx_data_segs.ds_addr;
			physaddr = data->rx_data_addr;

		/*
		 * New mbuf successfully loaded, update Rx ring and continue
		 * processing.
		 */
		m = data->m;
		data->m = mnew;

		/* finalize mbuf */
		m->m_pkthdr.len = m->m_len = len;
		m->m_pkthdr.rcvif = ifp;


#if defined(NFE_CSUM)
		if ((sc->nfe_flags & NFE_HW_CSUM) && (flags & NFE_RX_CSUMOK)) {
			m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
			if (flags & NFE_RX_IP_CSUMOK_V2) {
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			}
			if (flags & NFE_RX_UDP_CSUMOK_V2 ||
			    flags & NFE_RX_TCP_CSUMOK_V2) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}
#endif

#if NVLAN > 1
		if (have_tag) {
			m->m_pkthdr.ether_vtag = vlan_tag;
			m->m_flags |= M_VLANTAG;
		}
#endif

		ifp->if_ipackets++;

		NFE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		NFE_LOCK(sc);

		/* update mapping address in h/w descriptor */
		if (sc->nfe_flags & NFE_40BIT_ADDR) {
#if defined(__LP64__)
			desc64->physaddr[0] = htole32(physaddr >> 32);
#endif
			desc64->physaddr[1] = htole32(physaddr & 0xffffffff);
		} else {
			desc32->physaddr = htole32(physaddr);
		}

skip:		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64->length = htole16(sc->rxq.bufsz);
			desc64->flags = htole16(NFE_RX_READY);

			nfe_rxdesc64_sync(sc, desc64, BUS_DMASYNC_PREWRITE);
		} else {
			desc32->length = htole16(sc->rxq.bufsz);
			desc32->flags = htole16(NFE_RX_READY);

			nfe_rxdesc32_sync(sc, desc32, BUS_DMASYNC_PREWRITE);
		}

		sc->rxq.cur = (sc->rxq.cur + 1) % NFE_RX_RING_COUNT;
	}
}


static void
nfe_txeof(struct nfe_softc *sc)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_tx_data *data = NULL;
	u_int16_t flags;

	NFE_LOCK_ASSERT(sc);

	while (sc->txq.next != sc->txq.cur) {
		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[sc->txq.next];
			nfe_txdesc64_sync(sc, desc64, BUS_DMASYNC_POSTREAD);

			flags = letoh16(desc64->flags);
		} else {
			desc32 = &sc->txq.desc32[sc->txq.next];
			nfe_txdesc32_sync(sc, desc32, BUS_DMASYNC_POSTREAD);

			flags = letoh16(desc32->flags);
		}

		if (flags & NFE_TX_VALID)
			break;

		data = &sc->txq.data[sc->txq.next];

		if ((sc->nfe_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if (!(flags & NFE_TX_LASTFRAG_V1) && data->m == NULL)
				goto skip;

			if ((flags & NFE_TX_ERROR_V1) != 0) {
				printf("nfe%d: tx v1 error 0x%4b\n",
				    sc->nfe_unit, flags, NFE_V1_TXERR);

				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		} else {
			if (!(flags & NFE_TX_LASTFRAG_V2) && data->m == NULL)
				goto skip;

			if ((flags & NFE_TX_ERROR_V2) != 0) {
				printf("nfe%d: tx v1 error 0x%4b\n",
				    sc->nfe_unit, flags, NFE_V2_TXERR);

				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		}

		if (data->m == NULL) {	/* should not get there */
			printf("nfe%d: last fragment bit w/o associated mbuf!\n",
			    sc->nfe_unit);
			goto skip;
		}

		/* last fragment of the mbuf chain transmitted */
		bus_dmamap_sync(sc->txq.tx_data_tag, data->active,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txq.tx_data_tag, data->active);
		m_freem(data->m);
		data->m = NULL;

		ifp->if_timer = 0;

skip:		sc->txq.queued--;
		sc->txq.next = (sc->txq.next + 1) % NFE_TX_RING_COUNT;
	}

	if (data != NULL) {	/* at least one slot freed */
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		nfe_start_locked(ifp);
	}
}


static int
nfe_encap(struct nfe_softc *sc, struct mbuf *m0)
{
	struct nfe_desc32 *desc32=NULL;
	struct nfe_desc64 *desc64=NULL;
	struct nfe_tx_data *data=NULL;
	bus_dmamap_t map;
	bus_dma_segment_t segs[NFE_MAX_SCATTER];
	int error, i, nsegs;
	u_int16_t flags = NFE_TX_VALID;

	map = sc->txq.data[sc->txq.cur].tx_data_map;

	error = bus_dmamap_load_mbuf_sg(sc->txq.tx_data_tag, map, m0, segs,
	    &nsegs, BUS_DMA_NOWAIT);

	if (error != 0) {
		printf("nfe%d: could not map mbuf (error %d)\n", sc->nfe_unit,
		    error);
		return error;
	}

	if (sc->txq.queued + nsegs >= NFE_TX_RING_COUNT - 1) {
		bus_dmamap_unload(sc->txq.tx_data_tag, map);
		return ENOBUFS;
	}


#ifdef NFE_CSUM
	if (m0->m_pkthdr.csum_flags & CSUM_IP)
		flags |= NFE_TX_IP_CSUM;
	if (m0->m_pkthdr.csum_flags & CSUM_TCP)
		flags |= NFE_TX_TCP_CSUM;
	if (m0->m_pkthdr.csum_flags & CSUM_UDP)
		flags |= NFE_TX_TCP_CSUM;
#endif

	for (i = 0; i < nsegs; i++) {
		data = &sc->txq.data[sc->txq.cur];

		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[sc->txq.cur];
#if defined(__LP64__)
			desc64->physaddr[0] = htole32(segs[i].ds_addr >> 32);
#endif
			desc64->physaddr[1] = htole32(segs[i].ds_addr &
			    0xffffffff);
			desc64->length = htole16(segs[i].ds_len - 1);
			desc64->flags = htole16(flags);
#if NVLAN > 0
			if (m0->m_flags & M_VLANTAG)
				desc64->vtag = htole32(NFE_TX_VTAG |
				    m0->m_pkthdr.ether_vtag);
#endif
		} else {
			desc32 = &sc->txq.desc32[sc->txq.cur];

			desc32->physaddr = htole32(segs[i].ds_addr);
			desc32->length = htole16(segs[i].ds_len - 1);
			desc32->flags = htole16(flags);
		}

		/* csum flags and vtag belong to the first fragment only */
		if (nsegs > 1) {
			flags &= ~(NFE_TX_IP_CSUM | NFE_TX_TCP_CSUM);
		}

		sc->txq.queued++;
		sc->txq.cur = (sc->txq.cur + 1) % NFE_TX_RING_COUNT;
	}

	/* the whole mbuf chain has been DMA mapped, fix last descriptor */
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		flags |= NFE_TX_LASTFRAG_V2;
		desc64->flags = htole16(flags);
	} else {
		if (sc->nfe_flags & NFE_JUMBO_SUP)
			flags |= NFE_TX_LASTFRAG_V2;
		else
			flags |= NFE_TX_LASTFRAG_V1;
		desc32->flags = htole16(flags);
	}

	data->m = m0;
	data->active = map;
	data->nsegs = nsegs;

	bus_dmamap_sync(sc->txq.tx_data_tag, map, BUS_DMASYNC_PREWRITE);

	return 0;
}


static void
nfe_setmulti(struct nfe_softc *sc)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct ifmultiaddr *ifma;
	int i;
	u_int32_t filter = NFE_RXFILTER_MAGIC;
	u_int8_t addr[ETHER_ADDR_LEN], mask[ETHER_ADDR_LEN];
	u_int8_t etherbroadcastaddr[ETHER_ADDR_LEN] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};

	NFE_LOCK_ASSERT(sc);

	if ((ifp->if_flags & (IFF_ALLMULTI | IFF_PROMISC)) != 0) {
		bzero(addr, ETHER_ADDR_LEN);
		bzero(mask, ETHER_ADDR_LEN);
		goto done;
	}

	bcopy(etherbroadcastaddr, addr, ETHER_ADDR_LEN);
	bcopy(etherbroadcastaddr, mask, ETHER_ADDR_LEN);

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		u_char *addrp;

		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		addrp = LLADDR((struct sockaddr_dl *) ifma->ifma_addr);
		for (i = 0; i < ETHER_ADDR_LEN; i++) {
			u_int8_t mcaddr = addrp[i];
			addr[i] &= mcaddr;
			mask[i] &= ~mcaddr;
		}
	}
	IF_ADDR_UNLOCK(ifp);

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		mask[i] |= addr[i];
	}

done:
	addr[0] |= 0x01;	/* make sure multicast bit is set */

	NFE_WRITE(sc, NFE_MULTIADDR_HI,
	    addr[3] << 24 | addr[2] << 16 | addr[1] << 8 | addr[0]);
	NFE_WRITE(sc, NFE_MULTIADDR_LO,
	    addr[5] <<  8 | addr[4]);
	NFE_WRITE(sc, NFE_MULTIMASK_HI,
	    mask[3] << 24 | mask[2] << 16 | mask[1] << 8 | mask[0]);
	NFE_WRITE(sc, NFE_MULTIMASK_LO,
	    mask[5] <<  8 | mask[4]);

	filter |= (ifp->if_flags & IFF_PROMISC) ? NFE_PROMISC : NFE_U2M;
	NFE_WRITE(sc, NFE_RXFILTER, filter);
}


static void
nfe_start(struct ifnet *ifp)
{
	struct nfe_softc *sc;

	sc = ifp->if_softc;
	NFE_LOCK(sc);
	nfe_start_locked(ifp);
	NFE_UNLOCK(sc);
}


static void
nfe_start_locked(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int old = sc->txq.cur;

	if (!sc->nfe_link || ifp->if_drv_flags & IFF_DRV_OACTIVE) {
		return;
	}

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (nfe_encap(sc, m0) != 0) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}

		/* packet put in h/w queue, remove from s/w queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		BPF_MTAP(ifp, m0);
	}
	if (sc->txq.cur == old)	{ /* nothing sent */
		return;
	}

	if (sc->nfe_flags & NFE_40BIT_ADDR)
		nfe_txdesc64_rsync(sc, old, sc->txq.cur, BUS_DMASYNC_PREWRITE);
	else
		nfe_txdesc32_rsync(sc, old, sc->txq.cur, BUS_DMASYNC_PREWRITE);

	/* kick Tx */
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_KICKTX | sc->rxtxctl);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}


static void
nfe_watchdog(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;

	printf("nfe%d: watchdog timeout\n", sc->nfe_unit);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	nfe_init(sc);
	ifp->if_oerrors++;

	return;
}


static void
nfe_init(void *xsc)
{
	struct nfe_softc *sc = xsc;

	NFE_LOCK(sc);
	nfe_init_locked(sc);
	NFE_UNLOCK(sc);

	return;
}


static void
nfe_init_locked(void *xsc)
{
	struct nfe_softc *sc = xsc;
	struct ifnet *ifp = sc->nfe_ifp;
	struct mii_data *mii;
	u_int32_t tmp;

	NFE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->nfe_miibus);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		return;
	}

	nfe_stop(ifp, 0);

	NFE_WRITE(sc, NFE_TX_UNK, 0);
	NFE_WRITE(sc, NFE_STATUS, 0);

	sc->rxtxctl = NFE_RXTX_BIT2;
	if (sc->nfe_flags & NFE_40BIT_ADDR)
		sc->rxtxctl |= NFE_RXTX_V3MAGIC;
	else if (sc->nfe_flags & NFE_JUMBO_SUP)
		sc->rxtxctl |= NFE_RXTX_V2MAGIC;
#ifdef NFE_CSUM
	if (sc->nfe_flags & NFE_HW_CSUM)
		sc->rxtxctl |= NFE_RXTX_RXCSUM;
#endif

#if NVLAN > 0
	/*
	 * Although the adapter is capable of stripping VLAN tags from received
	 * frames (NFE_RXTX_VTAG_STRIP), we do not enable this functionality on
	 * purpose.  This will be done in software by our network stack.
	 */
	if (sc->nfe_flags & NFE_HW_VLAN)
		sc->rxtxctl |= NFE_RXTX_VTAG_INSERT;
#endif

	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | sc->rxtxctl);
	DELAY(10);
	NFE_WRITE(sc, NFE_RXTX_CTL, sc->rxtxctl);

#if NVLAN
	if (sc->nfe_flags & NFE_HW_VLAN)
		NFE_WRITE(sc, NFE_VTAG_CTL, NFE_VTAG_ENABLE);
#endif

	NFE_WRITE(sc, NFE_SETUP_R6, 0);

	/* set MAC address */
	nfe_set_macaddr(sc, sc->eaddr);

	/* tell MAC where rings are in memory */
#ifdef __LP64__
	NFE_WRITE(sc, NFE_RX_RING_ADDR_HI, sc->rxq.physaddr >> 32);
#endif
	NFE_WRITE(sc, NFE_RX_RING_ADDR_LO, sc->rxq.physaddr & 0xffffffff);
#ifdef __LP64__
	NFE_WRITE(sc, NFE_TX_RING_ADDR_HI, sc->txq.physaddr >> 32);
#endif
	NFE_WRITE(sc, NFE_TX_RING_ADDR_LO, sc->txq.physaddr & 0xffffffff);

	NFE_WRITE(sc, NFE_RING_SIZE,
	    (NFE_RX_RING_COUNT - 1) << 16 |
	    (NFE_TX_RING_COUNT - 1));

	NFE_WRITE(sc, NFE_RXBUFSZ, sc->rxq.bufsz);

	/* force MAC to wakeup */
	tmp = NFE_READ(sc, NFE_PWR_STATE);
	NFE_WRITE(sc, NFE_PWR_STATE, tmp | NFE_PWR_WAKEUP);
	DELAY(10);
	tmp = NFE_READ(sc, NFE_PWR_STATE);
	NFE_WRITE(sc, NFE_PWR_STATE, tmp | NFE_PWR_VALID);

#if 1
	/* configure interrupts coalescing/mitigation */
	NFE_WRITE(sc, NFE_IMTIMER, NFE_IM_DEFAULT);
#else
	/* no interrupt mitigation: one interrupt per packet */
	NFE_WRITE(sc, NFE_IMTIMER, 970);
#endif

	NFE_WRITE(sc, NFE_SETUP_R1, NFE_R1_MAGIC);
	NFE_WRITE(sc, NFE_SETUP_R2, NFE_R2_MAGIC);
	NFE_WRITE(sc, NFE_SETUP_R6, NFE_R6_MAGIC);

	/* update MAC knowledge of PHY; generates a NFE_IRQ_LINK interrupt */
	NFE_WRITE(sc, NFE_STATUS, sc->mii_phyaddr << 24 | NFE_STATUS_MAGIC);

	NFE_WRITE(sc, NFE_SETUP_R4, NFE_R4_MAGIC);
	NFE_WRITE(sc, NFE_WOL_CTL, NFE_WOL_MAGIC);

	sc->rxtxctl &= ~NFE_RXTX_BIT2;
	NFE_WRITE(sc, NFE_RXTX_CTL, sc->rxtxctl);
	DELAY(10);
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_BIT1 | sc->rxtxctl);

	/* set Rx filter */
	nfe_setmulti(sc);

	nfe_ifmedia_upd(ifp);

	nfe_tick_locked(sc);

	/* enable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, NFE_RX_START);

	/* enable Tx */
	NFE_WRITE(sc, NFE_TX_CTL, NFE_TX_START);

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		NFE_WRITE(sc, NFE_IRQ_MASK, 0);
	else
#endif
	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED); /* enable interrupts */

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->nfe_link = 0;

	return;
}


static void
nfe_stop(struct ifnet *ifp, int disable)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mii_data  *mii;

	NFE_LOCK_ASSERT(sc);

	ifp->if_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	mii = device_get_softc(sc->nfe_miibus);

	callout_stop(&sc->nfe_stat_ch);

	/* abort Tx */
	NFE_WRITE(sc, NFE_TX_CTL, 0);

	/* disable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, 0);

	/* disable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, 0);

	sc->nfe_link = 0;

	/* reset Tx and Rx rings */
	nfe_reset_tx_ring(sc, &sc->txq);
	nfe_reset_rx_ring(sc, &sc->rxq);

	return;
}


static int
nfe_ifmedia_upd(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;

	NFE_LOCK(sc);
	nfe_ifmedia_upd_locked(ifp);
	NFE_UNLOCK(sc);
	return (0);
}


static int
nfe_ifmedia_upd_locked(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	NFE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->nfe_miibus);

	if (mii->mii_instance) {
		struct mii_softc *miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		    miisc = LIST_NEXT(miisc, mii_list)) {
			mii_phy_reset(miisc);
		}
	}
	mii_mediachg(mii);

	return (0);
}


static void
nfe_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nfe_softc *sc;
	struct mii_data *mii;

	sc = ifp->if_softc;

	NFE_LOCK(sc);
	mii = device_get_softc(sc->nfe_miibus);
	mii_pollstat(mii);
	NFE_UNLOCK(sc);

	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;

	return;
}


static void
nfe_tick(void *xsc)
{
	struct nfe_softc *sc;

	sc = xsc;

	NFE_LOCK(sc);
	nfe_tick_locked(sc);
	NFE_UNLOCK(sc);
}


void
nfe_tick_locked(struct nfe_softc *arg)
{
	struct nfe_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;

	sc = arg;

	NFE_LOCK_ASSERT(sc);

	ifp = sc->nfe_ifp;

	mii = device_get_softc(sc->nfe_miibus);
	mii_tick(mii);

	if (!sc->nfe_link) {
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->nfe_link++;
			if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T
			    && bootverbose)
				if_printf(sc->nfe_ifp, "gigabit link up\n");
					if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
						nfe_start_locked(ifp);
		}
	}
	callout_reset(&sc->nfe_stat_ch, hz, nfe_tick, sc);

	return;
}


static void
nfe_shutdown(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);

	NFE_LOCK(sc);
	ifp = sc->nfe_ifp;
	nfe_stop(ifp,0);
	/* nfe_reset(sc); */
	NFE_UNLOCK(sc);

	return;
}


static void
nfe_get_macaddr(struct nfe_softc *sc, u_char *addr)
{
	uint32_t tmp;

	tmp = NFE_READ(sc, NFE_MACADDR_LO);
	addr[0] = (tmp >> 8) & 0xff;
	addr[1] = (tmp & 0xff);

	tmp = NFE_READ(sc, NFE_MACADDR_HI);
	addr[2] = (tmp >> 24) & 0xff;
	addr[3] = (tmp >> 16) & 0xff;
	addr[4] = (tmp >>  8) & 0xff;
	addr[5] = (tmp & 0xff);
}


static void
nfe_set_macaddr(struct nfe_softc *sc, u_char *addr)
{

	NFE_WRITE(sc, NFE_MACADDR_LO, addr[5] <<  8 | addr[4]);
	NFE_WRITE(sc, NFE_MACADDR_HI, addr[3] << 24 | addr[2] << 16 |
	    addr[1] << 8 | addr[0]);
}


/*
 * Map a single buffer address.
 */

static void
nfe_dma_map_segs(arg, segs, nseg, error)
	void *arg;
	bus_dma_segment_t *segs;
	int error, nseg;
{

	if (error)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	*(bus_dma_segment_t *)arg = *segs;

	return;
}
