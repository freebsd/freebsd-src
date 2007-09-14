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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
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

/* "device miibus" required.  See GENERIC if you get errors here. */
#include "miibus_if.h"

static int  nfe_probe(device_t);
static int  nfe_attach(device_t);
static int  nfe_detach(device_t);
static int  nfe_suspend(device_t);
static int  nfe_resume(device_t);
static void nfe_shutdown(device_t);
static void nfe_power(struct nfe_softc *);
static int  nfe_miibus_readreg(device_t, int, int);
static int  nfe_miibus_writereg(device_t, int, int, int);
static void nfe_miibus_statchg(device_t);
static void nfe_link_task(void *, int);
static void nfe_set_intr(struct nfe_softc *);
static __inline void nfe_enable_intr(struct nfe_softc *);
static __inline void nfe_disable_intr(struct nfe_softc *);
static int  nfe_ioctl(struct ifnet *, u_long, caddr_t);
static void nfe_alloc_msix(struct nfe_softc *, int);
static int nfe_intr(void *);
static void nfe_int_task(void *, int);
static void *nfe_jalloc(struct nfe_softc *);
static void nfe_jfree(void *, void *);
static __inline void nfe_discard_rxbuf(struct nfe_softc *, int);
static __inline void nfe_discard_jrxbuf(struct nfe_softc *, int);
static int nfe_newbuf(struct nfe_softc *, int);
static int nfe_jnewbuf(struct nfe_softc *, int);
static int  nfe_rxeof(struct nfe_softc *, int);
static int  nfe_jrxeof(struct nfe_softc *, int);
static void nfe_txeof(struct nfe_softc *);
static struct mbuf *nfe_defrag(struct mbuf *, int, int);
static int  nfe_encap(struct nfe_softc *, struct mbuf **);
static void nfe_setmulti(struct nfe_softc *);
static void nfe_tx_task(void *, int);
static void nfe_start(struct ifnet *);
static void nfe_watchdog(struct ifnet *);
static void nfe_init(void *);
static void nfe_init_locked(void *);
static void nfe_stop(struct ifnet *);
static int  nfe_alloc_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static void nfe_alloc_jrx_ring(struct nfe_softc *, struct nfe_jrx_ring *);
static int  nfe_init_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static int  nfe_init_jrx_ring(struct nfe_softc *, struct nfe_jrx_ring *);
static void nfe_free_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
static void nfe_free_jrx_ring(struct nfe_softc *, struct nfe_jrx_ring *);
static int  nfe_alloc_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static void nfe_init_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static void nfe_free_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
static int  nfe_ifmedia_upd(struct ifnet *);
static void nfe_ifmedia_sts(struct ifnet *, struct ifmediareq *);
static void nfe_tick(void *);
static void nfe_get_macaddr(struct nfe_softc *, uint8_t *);
static void nfe_set_macaddr(struct nfe_softc *, uint8_t *);
static void nfe_dma_map_segs(void *, bus_dma_segment_t *, int, int);

static int sysctl_int_range(SYSCTL_HANDLER_ARGS, int, int);
static int sysctl_hw_nfe_proc_limit(SYSCTL_HANDLER_ARGS);

#ifdef NFE_DEBUG
static int nfedebug = 0;
#define	DPRINTF(sc, ...)	do {				\
	if (nfedebug)						\
		device_printf((sc)->nfe_dev, __VA_ARGS__);	\
} while (0)
#define	DPRINTFN(sc, n, ...)	do {				\
	if (nfedebug >= (n))					\
		device_printf((sc)->nfe_dev, __VA_ARGS__);	\
} while (0)
#else
#define	DPRINTF(sc, ...)
#define	DPRINTFN(sc, n, ...)
#endif

#define	NFE_LOCK(_sc)		mtx_lock(&(_sc)->nfe_mtx)
#define	NFE_UNLOCK(_sc)		mtx_unlock(&(_sc)->nfe_mtx)
#define	NFE_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->nfe_mtx, MA_OWNED)

#define	NFE_JLIST_LOCK(_sc)	mtx_lock(&(_sc)->nfe_jlist_mtx)
#define	NFE_JLIST_UNLOCK(_sc)	mtx_unlock(&(_sc)->nfe_jlist_mtx)

/* Tunables. */
static int msi_disable = 0;
static int msix_disable = 0;
static int jumbo_disable = 0;
TUNABLE_INT("hw.nfe.msi_disable", &msi_disable);
TUNABLE_INT("hw.nfe.msix_disable", &msix_disable);
TUNABLE_INT("hw.nfe.jumbo_disable", &jumbo_disable);

static device_method_t nfe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		nfe_probe),
	DEVMETHOD(device_attach,	nfe_attach),
	DEVMETHOD(device_detach,	nfe_detach),
	DEVMETHOD(device_suspend,	nfe_suspend),
	DEVMETHOD(device_resume,	nfe_resume),
	DEVMETHOD(device_shutdown,	nfe_shutdown),

	/* bus interface */
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),

	/* MII interface */
	DEVMETHOD(miibus_readreg,	nfe_miibus_readreg),
	DEVMETHOD(miibus_writereg,	nfe_miibus_writereg),
	DEVMETHOD(miibus_statchg,	nfe_miibus_statchg),

	{ NULL, NULL }
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
	    "NVIDIA nForce MCP04 Networking Adapter"},		/* MCP10 */
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN2,
	    "NVIDIA nForce MCP04 Networking Adapter"},		/* MCP11 */
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
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP61_LAN4,
	    "NVIDIA nForce MCP61 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN1,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN2,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN3,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP65_LAN4,
	    "NVIDIA nForce MCP65 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN1,
	    "NVIDIA nForce MCP67 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN2,
	    "NVIDIA nForce MCP67 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN3,
	    "NVIDIA nForce MCP67 Networking Adapter"},
	{PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP67_LAN4,
	    "NVIDIA nForce MCP67 Networking Adapter"},
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
			return (BUS_PROBE_DEFAULT);
		}
		t++;
	}

	return (ENXIO);
}

static void
nfe_alloc_msix(struct nfe_softc *sc, int count)
{
	int rid;

	rid = PCIR_BAR(2);
	sc->nfe_msix_res = bus_alloc_resource_any(sc->nfe_dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->nfe_msix_res == NULL) {
		device_printf(sc->nfe_dev,
		    "couldn't allocate MSIX table resource\n");
		return;
	}
	rid = PCIR_BAR(3);
	sc->nfe_msix_pba_res = bus_alloc_resource_any(sc->nfe_dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->nfe_msix_pba_res == NULL) {
		device_printf(sc->nfe_dev,
		    "couldn't allocate MSIX PBA resource\n");
		bus_release_resource(sc->nfe_dev, SYS_RES_MEMORY, PCIR_BAR(2),
		    sc->nfe_msix_res);
		sc->nfe_msix_res = NULL;
		return;
	}

	if (pci_alloc_msix(sc->nfe_dev, &count) == 0) {
		if (count == NFE_MSI_MESSAGES) {
			if (bootverbose)
				device_printf(sc->nfe_dev,
				    "Using %d MSIX messages\n", count);
			sc->nfe_msix = 1;
		} else {
			if (bootverbose)
				device_printf(sc->nfe_dev,
				    "couldn't allocate MSIX\n");
			pci_release_msi(sc->nfe_dev);
			bus_release_resource(sc->nfe_dev, SYS_RES_MEMORY,
			    PCIR_BAR(3), sc->nfe_msix_pba_res);
			bus_release_resource(sc->nfe_dev, SYS_RES_MEMORY,
			    PCIR_BAR(2), sc->nfe_msix_res);
			sc->nfe_msix_pba_res = NULL;
			sc->nfe_msix_res = NULL;
		}
	}
}

static int
nfe_attach(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;
	bus_addr_t dma_addr_max;
	int error = 0, i, msic, reg, rid;

	sc = device_get_softc(dev);
	sc->nfe_dev = dev;

	mtx_init(&sc->nfe_mtx, device_get_nameunit(dev), MTX_NETWORK_LOCK,
	    MTX_DEF);
	mtx_init(&sc->nfe_jlist_mtx, "nfe_jlist_mtx", NULL, MTX_DEF);
	callout_init_mtx(&sc->nfe_stat_ch, &sc->nfe_mtx, 0);
	TASK_INIT(&sc->nfe_link_task, 0, nfe_link_task, sc);
	SLIST_INIT(&sc->nfe_jfree_listhead);
	SLIST_INIT(&sc->nfe_jinuse_listhead);

	pci_enable_busmaster(dev);

	rid = PCIR_BAR(0);
	sc->nfe_res[0] = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->nfe_res[0] == NULL) {
		device_printf(dev, "couldn't map memory resources\n");
		mtx_destroy(&sc->nfe_mtx);
		return (ENXIO);
	}

	if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
		uint16_t v, width;

		v = pci_read_config(dev, reg + 0x08, 2);
		/* Change max. read request size to 4096. */
		v &= ~(7 << 12);
		v |= (5 << 12);
		pci_write_config(dev, reg + 0x08, v, 2);

		v = pci_read_config(dev, reg + 0x0c, 2);
		/* link capability */
		v = (v >> 4) & 0x0f;
		width = pci_read_config(dev, reg + 0x12, 2);
		/* negotiated link width */
		width = (width >> 4) & 0x3f;
		if (v != width)
			device_printf(sc->nfe_dev,
			    "warning, negotiated width of link(x%d) != "
			    "max. width of link(x%d)\n", width, v);
	}

	/* Allocate interrupt */
	if (msix_disable == 0 || msi_disable == 0) {
		if (msix_disable == 0 &&
		    (msic = pci_msix_count(dev)) == NFE_MSI_MESSAGES)
			nfe_alloc_msix(sc, msic);
		if (msi_disable == 0 && sc->nfe_msix == 0 &&
		    (msic = pci_msi_count(dev)) == NFE_MSI_MESSAGES &&
		    pci_alloc_msi(dev, &msic) == 0) {
			if (msic == NFE_MSI_MESSAGES) {
				if (bootverbose)
					device_printf(dev,
					    "Using %d MSI messages\n", msic);
				sc->nfe_msi = 1;
			} else
				pci_release_msi(dev);
		}
	}

	if (sc->nfe_msix == 0 && sc->nfe_msi == 0) {
		rid = 0;
		sc->nfe_irq[0] = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (sc->nfe_irq[0] == NULL) {
			device_printf(dev, "couldn't allocate IRQ resources\n");
			error = ENXIO;
			goto fail;
		}
	} else {
		for (i = 0, rid = 1; i < NFE_MSI_MESSAGES; i++, rid++) {
			sc->nfe_irq[i] = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &rid, RF_ACTIVE);
			if (sc->nfe_irq[i] == NULL) {
				device_printf(dev,
				    "couldn't allocate IRQ resources for "
				    "message %d\n", rid);
				error = ENXIO;
				goto fail;
			}
		}
		/* Map interrupts to vector 0. */
		if (sc->nfe_msix != 0) {
			NFE_WRITE(sc, NFE_MSIX_MAP0, 0);
			NFE_WRITE(sc, NFE_MSIX_MAP1, 0);
		} else if (sc->nfe_msi != 0) {
			NFE_WRITE(sc, NFE_MSI_MAP0, 0);
			NFE_WRITE(sc, NFE_MSI_MAP1, 0);
		}
	}

	/* Set IRQ status/mask register. */
	sc->nfe_irq_status = NFE_IRQ_STATUS;
	sc->nfe_irq_mask = NFE_IRQ_MASK;
	sc->nfe_intrs = NFE_IRQ_WANTED;
	sc->nfe_nointrs = 0;
	if (sc->nfe_msix != 0) {
		sc->nfe_irq_status = NFE_MSIX_IRQ_STATUS;
		sc->nfe_nointrs = NFE_IRQ_WANTED;
	} else if (sc->nfe_msi != 0) {
		sc->nfe_irq_mask = NFE_MSI_IRQ_MASK;
		sc->nfe_intrs = NFE_MSI_VECTOR_0_ENABLED;
	}

	sc->nfe_devid = pci_get_device(dev);
	sc->nfe_revid = pci_get_revid(dev);
	sc->nfe_flags = 0;

	switch (sc->nfe_devid) {
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN2:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN3:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN4:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN5:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_HW_CSUM;
		break;
	case PCI_PRODUCT_NVIDIA_MCP51_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP51_LAN2:
		sc->nfe_flags |= NFE_40BIT_ADDR | NFE_PWR_MGMT;
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
		    NFE_HW_VLAN | NFE_PWR_MGMT | NFE_TX_FLOW_CTRL;
		break;

	case PCI_PRODUCT_NVIDIA_MCP61_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP61_LAN4:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP67_LAN4:
		sc->nfe_flags |= NFE_40BIT_ADDR | NFE_PWR_MGMT |
		    NFE_TX_FLOW_CTRL;
		break;
	case PCI_PRODUCT_NVIDIA_MCP65_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN3:
	case PCI_PRODUCT_NVIDIA_MCP65_LAN4:
		sc->nfe_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR |
		    NFE_PWR_MGMT | NFE_TX_FLOW_CTRL;
		break;
	}

	nfe_power(sc);
	/* Check for reversed ethernet address */
	if ((NFE_READ(sc, NFE_TX_UNK) & NFE_MAC_ADDR_INORDER) != 0)
		sc->nfe_flags |= NFE_CORRECT_MACADDR;
	nfe_get_macaddr(sc, sc->eaddr);
	/*
	 * Allocate the parent bus DMA tag appropriate for PCI.
	 */
	dma_addr_max = BUS_SPACE_MAXADDR_32BIT;
	if ((sc->nfe_flags & NFE_40BIT_ADDR) != 0)
		dma_addr_max = NFE_DMA_MAXADDR;
	error = bus_dma_tag_create(
	    bus_get_dma_tag(sc->nfe_dev),	/* parent */
	    1, 0,				/* alignment, boundary */
	    dma_addr_max,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    BUS_SPACE_MAXSIZE_32BIT, 0,		/* maxsize, nsegments */
	    BUS_SPACE_MAXSIZE_32BIT,		/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &sc->nfe_parent_tag);
	if (error)
		goto fail;

	ifp = sc->nfe_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not if_alloc()\n");
		error = ENOSPC;
		goto fail;
	}
	TASK_INIT(&sc->nfe_tx_task, 1, nfe_tx_task, ifp);

	/*
	 * Allocate Tx and Rx rings.
	 */
	if ((error = nfe_alloc_tx_ring(sc, &sc->txq)) != 0)
		goto fail;

	if ((error = nfe_alloc_rx_ring(sc, &sc->rxq)) != 0)
		goto fail;

	nfe_alloc_jrx_ring(sc, &sc->jrxq);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "process_limit", CTLTYPE_INT | CTLFLAG_RW,
	    &sc->nfe_process_limit, 0, sysctl_hw_nfe_proc_limit, "I",
	    "max number of Rx events to process");

	sc->nfe_process_limit = NFE_PROC_DEFAULT;
	error = resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "process_limit", &sc->nfe_process_limit);
	if (error == 0) {
		if (sc->nfe_process_limit < NFE_PROC_MIN ||
		    sc->nfe_process_limit > NFE_PROC_MAX) {
			device_printf(dev, "process_limit value out of range; "
			    "using default: %d\n", NFE_PROC_DEFAULT);
			sc->nfe_process_limit = NFE_PROC_DEFAULT;
		}
	}

	ifp->if_softc = sc;
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nfe_ioctl;
	ifp->if_start = nfe_start;
	ifp->if_hwassist = 0;
	ifp->if_capabilities = 0;
	ifp->if_watchdog = NULL;
	ifp->if_init = nfe_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, NFE_TX_RING_COUNT - 1);
	ifp->if_snd.ifq_drv_maxlen = NFE_TX_RING_COUNT - 1;
	IFQ_SET_READY(&ifp->if_snd);

	if (sc->nfe_flags & NFE_HW_CSUM) {
		ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_TSO4;
		ifp->if_hwassist |= NFE_CSUM_FEATURES | CSUM_TSO;
	}
	ifp->if_capenable = ifp->if_capabilities;

	sc->nfe_framesize = ifp->if_mtu + NFE_RX_HEADERS;
	/* VLAN capability setup. */
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	if ((sc->nfe_flags & NFE_HW_VLAN) != 0) {
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
		if ((ifp->if_capabilities & IFCAP_HWCSUM) != 0)
			ifp->if_capabilities |= IFCAP_VLAN_HWCSUM;
	}
	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Tell the upper layer(s) we support long frames.
	 * Must appear after the call to ether_ifattach() because
	 * ether_ifattach() sets ifi_hdrlen to the default value.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/* Do MII setup */
	if (mii_phy_probe(dev, &sc->nfe_miibus, nfe_ifmedia_upd,
	    nfe_ifmedia_sts)) {
		device_printf(dev, "MII without any phy!\n");
		error = ENXIO;
		goto fail;
	}
	ether_ifattach(ifp, sc->eaddr);

	TASK_INIT(&sc->nfe_int_task, 0, nfe_int_task, sc);
	sc->nfe_tq = taskqueue_create_fast("nfe_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &sc->nfe_tq);
	taskqueue_start_threads(&sc->nfe_tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->nfe_dev));
	error = 0;
	if (sc->nfe_msi == 0 && sc->nfe_msix == 0) {
		error = bus_setup_intr(dev, sc->nfe_irq[0],
		    INTR_TYPE_NET | INTR_MPSAFE, nfe_intr, NULL, sc,
		    &sc->nfe_intrhand[0]);
	} else {
		for (i = 0; i < NFE_MSI_MESSAGES; i++) {
			error = bus_setup_intr(dev, sc->nfe_irq[i],
			    INTR_TYPE_NET | INTR_MPSAFE, nfe_intr, NULL, sc,
			    &sc->nfe_intrhand[i]);
			if (error != 0)
				break;
		}
	}
	if (error) {
		device_printf(dev, "couldn't set up irq\n");
		taskqueue_free(sc->nfe_tq);
		sc->nfe_tq = NULL;
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
	uint8_t eaddr[ETHER_ADDR_LEN];
	int i, rid;

	sc = device_get_softc(dev);
	KASSERT(mtx_initialized(&sc->nfe_mtx), ("nfe mutex not initialized"));
	ifp = sc->nfe_ifp;

#ifdef DEVICE_POLLING
	if (ifp != NULL && ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif
	if (device_is_attached(dev)) {
		NFE_LOCK(sc);
		nfe_stop(ifp);
		ifp->if_flags &= ~IFF_UP;
		NFE_UNLOCK(sc);
		callout_drain(&sc->nfe_stat_ch);
		taskqueue_drain(taskqueue_fast, &sc->nfe_tx_task);
		taskqueue_drain(taskqueue_swi, &sc->nfe_link_task);
		ether_ifdetach(ifp);
	}

	if (ifp) {
		/* restore ethernet address */
		if ((sc->nfe_flags & NFE_CORRECT_MACADDR) == 0) {
			for (i = 0; i < ETHER_ADDR_LEN; i++) {
				eaddr[i] = sc->eaddr[5 - i];
			}
		} else
			bcopy(sc->eaddr, eaddr, ETHER_ADDR_LEN);
		nfe_set_macaddr(sc, eaddr);
		if_free(ifp);
	}
	if (sc->nfe_miibus)
		device_delete_child(dev, sc->nfe_miibus);
	bus_generic_detach(dev);
	if (sc->nfe_tq != NULL) {
		taskqueue_drain(sc->nfe_tq, &sc->nfe_int_task);
		taskqueue_free(sc->nfe_tq);
		sc->nfe_tq = NULL;
	}

	for (i = 0; i < NFE_MSI_MESSAGES; i++) {
		if (sc->nfe_intrhand[i] != NULL) {
			bus_teardown_intr(dev, sc->nfe_irq[i],
			    sc->nfe_intrhand[i]);
			sc->nfe_intrhand[i] = NULL;
		}
	}

	if (sc->nfe_msi == 0 && sc->nfe_msix == 0) {
		if (sc->nfe_irq[0] != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, 0,
			    sc->nfe_irq[0]);
	} else {
		for (i = 0, rid = 1; i < NFE_MSI_MESSAGES; i++, rid++) {
			if (sc->nfe_irq[i] != NULL) {
				bus_release_resource(dev, SYS_RES_IRQ, rid,
				    sc->nfe_irq[i]);
				sc->nfe_irq[i] = NULL;
			}
		}
		pci_release_msi(dev);
	}
	if (sc->nfe_msix_pba_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(3),
		    sc->nfe_msix_pba_res);
		sc->nfe_msix_pba_res = NULL;
	}
	if (sc->nfe_msix_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(2),
		    sc->nfe_msix_res);
		sc->nfe_msix_res = NULL;
	}
	if (sc->nfe_res[0] != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    sc->nfe_res[0]);
		sc->nfe_res[0] = NULL;
	}

	nfe_free_tx_ring(sc, &sc->txq);
	nfe_free_rx_ring(sc, &sc->rxq);
	nfe_free_jrx_ring(sc, &sc->jrxq);

	if (sc->nfe_parent_tag) {
		bus_dma_tag_destroy(sc->nfe_parent_tag);
		sc->nfe_parent_tag = NULL;
	}

	mtx_destroy(&sc->nfe_jlist_mtx);
	mtx_destroy(&sc->nfe_mtx);

	return (0);
}


static int
nfe_suspend(device_t dev)
{
	struct nfe_softc *sc;

	sc = device_get_softc(dev);

	NFE_LOCK(sc);
	nfe_stop(sc->nfe_ifp);
	sc->nfe_suspended = 1;
	NFE_UNLOCK(sc);

	return (0);
}


static int
nfe_resume(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);

	NFE_LOCK(sc);
	ifp = sc->nfe_ifp;
	if (ifp->if_flags & IFF_UP)
		nfe_init_locked(sc);
	sc->nfe_suspended = 0;
	NFE_UNLOCK(sc);

	return (0);
}


/* Take PHY/NIC out of powerdown, from Linux */
static void
nfe_power(struct nfe_softc *sc)
{
	uint32_t pwr;

	if ((sc->nfe_flags & NFE_PWR_MGMT) == 0)
		return;
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | NFE_RXTX_BIT2);
	NFE_WRITE(sc, NFE_MAC_RESET, NFE_MAC_RESET_MAGIC);
	DELAY(100);
	NFE_WRITE(sc, NFE_MAC_RESET, 0);
	DELAY(100);
	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_BIT2);
	pwr = NFE_READ(sc, NFE_PWR2_CTL);
	pwr &= ~NFE_PWR2_WAKEUP_MASK;
	if (sc->nfe_revid >= 0xa3 &&
	    (sc->nfe_devid == PCI_PRODUCT_NVIDIA_NFORCE430_LAN1 ||
	    sc->nfe_devid == PCI_PRODUCT_NVIDIA_NFORCE430_LAN2))
		pwr |= NFE_PWR2_REVA3;
	NFE_WRITE(sc, NFE_PWR2_CTL, pwr);
}


static void
nfe_miibus_statchg(device_t dev)
{
	struct nfe_softc *sc;

	sc = device_get_softc(dev);
	taskqueue_enqueue(taskqueue_swi, &sc->nfe_link_task);
}


static void
nfe_link_task(void *arg, int pending)
{
	struct nfe_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;
	uint32_t phy, seed, misc = NFE_MISC1_MAGIC, link = NFE_MEDIA_SET;
	uint32_t gmask, rxctl, txctl, val;

	sc = (struct nfe_softc *)arg;

	NFE_LOCK(sc);

	mii = device_get_softc(sc->nfe_miibus);
	ifp = sc->nfe_ifp;
	if (mii == NULL || ifp == NULL) {
		NFE_UNLOCK(sc);
		return;
	}

	if (mii->mii_media_status & IFM_ACTIVE) {
		if (IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
			sc->nfe_link = 1;
	} else
		sc->nfe_link = 0;

	phy = NFE_READ(sc, NFE_PHY_IFACE);
	phy &= ~(NFE_PHY_HDX | NFE_PHY_100TX | NFE_PHY_1000T);

	seed = NFE_READ(sc, NFE_RNDSEED);
	seed &= ~NFE_SEED_MASK;

	if (((mii->mii_media_active & IFM_GMASK) & IFM_FDX) == 0) {
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

	if ((phy & 0x10000000) != 0) {
		if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T)
			val = NFE_R1_MAGIC_1000;
		else
			val = NFE_R1_MAGIC_10_100;
	} else
		val = NFE_R1_MAGIC_DEFAULT;
	NFE_WRITE(sc, NFE_SETUP_R1, val);

	NFE_WRITE(sc, NFE_RNDSEED, seed);	/* XXX: gigabit NICs only? */

	NFE_WRITE(sc, NFE_PHY_IFACE, phy);
	NFE_WRITE(sc, NFE_MISC1, misc);
	NFE_WRITE(sc, NFE_LINKSPEED, link);

	gmask = mii->mii_media_active & IFM_GMASK;
	if ((gmask & IFM_FDX) != 0) {
		/* It seems all hardwares supports Rx pause frames. */
		val = NFE_READ(sc, NFE_RXFILTER);
		if ((gmask & IFM_FLAG0) != 0)
			val |= NFE_PFF_RX_PAUSE;
		else
			val &= ~NFE_PFF_RX_PAUSE;
		NFE_WRITE(sc, NFE_RXFILTER, val);
		if ((sc->nfe_flags & NFE_TX_FLOW_CTRL) != 0) {
			val = NFE_READ(sc, NFE_MISC1);
			if ((gmask & IFM_FLAG1) != 0) {
				NFE_WRITE(sc, NFE_TX_PAUSE_FRAME,
				    NFE_TX_PAUSE_FRAME_ENABLE);
				val |= NFE_MISC1_TX_PAUSE;
			} else {
				val &= ~NFE_MISC1_TX_PAUSE;
				NFE_WRITE(sc, NFE_TX_PAUSE_FRAME,
				    NFE_TX_PAUSE_FRAME_DISABLE);
			}
			NFE_WRITE(sc, NFE_MISC1, val);
		}
	} else {
		/* disable rx/tx pause frames */
		val = NFE_READ(sc, NFE_RXFILTER);
		val &= ~NFE_PFF_RX_PAUSE;
		NFE_WRITE(sc, NFE_RXFILTER, val);
		if ((sc->nfe_flags & NFE_TX_FLOW_CTRL) != 0) {
			NFE_WRITE(sc, NFE_TX_PAUSE_FRAME,
			    NFE_TX_PAUSE_FRAME_DISABLE);
			val = NFE_READ(sc, NFE_MISC1);
			val &= ~NFE_MISC1_TX_PAUSE;
			NFE_WRITE(sc, NFE_MISC1, val);
		}
	}

	txctl = NFE_READ(sc, NFE_TX_CTL);
	rxctl = NFE_READ(sc, NFE_RX_CTL);
	if (sc->nfe_link != 0 && (ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
		txctl |= NFE_TX_START;
		rxctl |= NFE_RX_START;
	} else {
		txctl &= ~NFE_TX_START;
		rxctl &= ~NFE_RX_START;
	}
	NFE_WRITE(sc, NFE_TX_CTL, txctl);
	NFE_WRITE(sc, NFE_RX_CTL, rxctl);

	NFE_UNLOCK(sc);
}


static int
nfe_miibus_readreg(device_t dev, int phy, int reg)
{
	struct nfe_softc *sc = device_get_softc(dev);
	uint32_t val;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_CTL, (phy << NFE_PHYADD_SHIFT) | reg);

	for (ntries = 0; ntries < NFE_TIMEOUT; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
	if (ntries == NFE_TIMEOUT) {
		DPRINTFN(sc, 2, "timeout waiting for PHY\n");
		return 0;
	}

	if (NFE_READ(sc, NFE_PHY_STATUS) & NFE_PHY_ERROR) {
		DPRINTFN(sc, 2, "could not read PHY\n");
		return 0;
	}

	val = NFE_READ(sc, NFE_PHY_DATA);
	if (val != 0xffffffff && val != 0)
		sc->mii_phyaddr = phy;

	DPRINTFN(sc, 2, "mii read phy %d reg 0x%x ret 0x%x\n", phy, reg, val);

	return (val);
}


static int
nfe_miibus_writereg(device_t dev, int phy, int reg, int val)
{
	struct nfe_softc *sc = device_get_softc(dev);
	uint32_t ctl;
	int ntries;

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

	if (NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		DELAY(100);
	}

	NFE_WRITE(sc, NFE_PHY_DATA, val);
	ctl = NFE_PHY_WRITE | (phy << NFE_PHYADD_SHIFT) | reg;
	NFE_WRITE(sc, NFE_PHY_CTL, ctl);

	for (ntries = 0; ntries < NFE_TIMEOUT; ntries++) {
		DELAY(100);
		if (!(NFE_READ(sc, NFE_PHY_CTL) & NFE_PHY_BUSY))
			break;
	}
#ifdef NFE_DEBUG
	if (nfedebug >= 2 && ntries == NFE_TIMEOUT)
		device_printf(sc->nfe_dev, "could not write to PHY\n");
#endif
	return (0);
}

/*
 * Allocate a jumbo buffer.
 */
static void *
nfe_jalloc(struct nfe_softc *sc)
{
	struct nfe_jpool_entry *entry;

	NFE_JLIST_LOCK(sc);

	entry = SLIST_FIRST(&sc->nfe_jfree_listhead);

	if (entry == NULL) {
		NFE_JLIST_UNLOCK(sc);
		return (NULL);
	}

	SLIST_REMOVE_HEAD(&sc->nfe_jfree_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->nfe_jinuse_listhead, entry, jpool_entries);

	NFE_JLIST_UNLOCK(sc);

	return (sc->jrxq.jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
static void
nfe_jfree(void *buf, void *args)
{
	struct nfe_softc *sc;
	struct nfe_jpool_entry *entry;
	int i;

	/* Extract the softc struct pointer. */
	sc = (struct nfe_softc *)args;
	KASSERT(sc != NULL, ("%s: can't find softc pointer!", __func__));

	NFE_JLIST_LOCK(sc);
	/* Calculate the slot this buffer belongs to. */
	i = ((vm_offset_t)buf
	     - (vm_offset_t)sc->jrxq.jpool) / NFE_JLEN;
	KASSERT(i >= 0 && i < NFE_JSLOTS,
	    ("%s: asked to free buffer that we don't manage!", __func__));

	entry = SLIST_FIRST(&sc->nfe_jinuse_listhead);
	KASSERT(entry != NULL, ("%s: buffer not in use!", __func__));
	entry->slot = i;
	SLIST_REMOVE_HEAD(&sc->nfe_jinuse_listhead, jpool_entries);
	SLIST_INSERT_HEAD(&sc->nfe_jfree_listhead, entry, jpool_entries);
	if (SLIST_EMPTY(&sc->nfe_jinuse_listhead))
		wakeup(sc);

	NFE_JLIST_UNLOCK(sc);
}

struct nfe_dmamap_arg {
	bus_addr_t nfe_busaddr;
};

static int
nfe_alloc_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_dmamap_arg ctx;
	struct nfe_rx_data *data;
	void *desc;
	int i, error, descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    NFE_RING_ALIGN, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    NFE_RX_RING_COUNT * descsize, 1,	/* maxsize, nsegments */
	    NFE_RX_RING_COUNT * descsize,	/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &ring->rx_desc_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	/* allocate memory to desc */
	error = bus_dmamem_alloc(ring->rx_desc_tag, &desc, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &ring->rx_desc_map);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create desc DMA map\n");
		goto fail;
	}
	if (sc->nfe_flags & NFE_40BIT_ADDR)
		ring->desc64 = desc;
	else
		ring->desc32 = desc;

	/* map desc to device visible address space */
	ctx.nfe_busaddr = 0;
	error = bus_dmamap_load(ring->rx_desc_tag, ring->rx_desc_map, desc,
	    NFE_RX_RING_COUNT * descsize, nfe_dma_map_segs, &ctx, 0);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not load desc DMA map\n");
		goto fail;
	}
	ring->physaddr = ctx.nfe_busaddr;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    MCLBYTES, 1,		/* maxsize, nsegments */
	    MCLBYTES,			/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &ring->rx_data_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create Rx DMA tag\n");
		goto fail;
	}

	error = bus_dmamap_create(ring->rx_data_tag, 0, &ring->rx_spare_map);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not create Rx DMA spare map\n");
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		data = &sc->rxq.data[i];
		data->rx_data_map = NULL;
		data->m = NULL;
		error = bus_dmamap_create(ring->rx_data_tag, 0,
		    &data->rx_data_map);
		if (error != 0) {
			device_printf(sc->nfe_dev,
			    "could not create Rx DMA map\n");
			goto fail;
		}
	}

fail:
	return (error);
}


static void
nfe_alloc_jrx_ring(struct nfe_softc *sc, struct nfe_jrx_ring *ring)
{
	struct nfe_dmamap_arg ctx;
	struct nfe_rx_data *data;
	void *desc;
	struct nfe_jpool_entry *entry;
	uint8_t *ptr;
	int i, error, descsize;

	if ((sc->nfe_flags & NFE_JUMBO_SUP) == 0)
		return;
	if (jumbo_disable != 0) {
		device_printf(sc->nfe_dev, "disabling jumbo frame support\n");
		sc->nfe_jumbo_disable = 1;
		return;
	}

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->jdesc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->jdesc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->jcur = ring->jnext = 0;

	/* Create DMA tag for jumbo Rx ring. */
	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    NFE_RING_ALIGN, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    NFE_JUMBO_RX_RING_COUNT * descsize,	/* maxsize */
	    1, 					/* nsegments */
	    NFE_JUMBO_RX_RING_COUNT * descsize,	/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &ring->jrx_desc_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not create jumbo ring DMA tag\n");
		goto fail;
	}

	/* Create DMA tag for jumbo buffer blocks. */
	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    PAGE_SIZE, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    NFE_JMEM,				/* maxsize */
	    1, 					/* nsegments */
	    NFE_JMEM,				/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &ring->jrx_jumbo_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not create jumbo Rx buffer block DMA tag\n");
		goto fail;
	}

	/* Create DMA tag for jumbo Rx buffers. */
	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    PAGE_SIZE, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    NFE_JLEN,				/* maxsize */
	    1,					/* nsegments */
	    NFE_JLEN,				/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &ring->jrx_data_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not create jumbo Rx buffer DMA tag\n");
		goto fail;
	}

	/* Allocate DMA'able memory and load the DMA map for jumbo Rx ring. */
	error = bus_dmamem_alloc(ring->jrx_desc_tag, &desc, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &ring->jrx_desc_map);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not allocate DMA'able memory for jumbo Rx ring\n");
		goto fail;
	}
	if (sc->nfe_flags & NFE_40BIT_ADDR)
		ring->jdesc64 = desc;
	else
		ring->jdesc32 = desc;

	ctx.nfe_busaddr = 0;
	error = bus_dmamap_load(ring->jrx_desc_tag, ring->jrx_desc_map, desc,
	    NFE_JUMBO_RX_RING_COUNT * descsize, nfe_dma_map_segs, &ctx, 0);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not load DMA'able memory for jumbo Rx ring\n");
		goto fail;
	}
	ring->jphysaddr = ctx.nfe_busaddr;

	/* Create DMA maps for jumbo Rx buffers. */
	error = bus_dmamap_create(ring->jrx_data_tag, 0, &ring->jrx_spare_map);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not create jumbo Rx DMA spare map\n");
		goto fail;
	}

	for (i = 0; i < NFE_JUMBO_RX_RING_COUNT; i++) {
		data = &sc->jrxq.jdata[i];
		data->rx_data_map = NULL;
		data->m = NULL;
		error = bus_dmamap_create(ring->jrx_data_tag, 0,
		    &data->rx_data_map);
		if (error != 0) {
			device_printf(sc->nfe_dev,
			    "could not create jumbo Rx DMA map\n");
			goto fail;
		}
	}

	/* Allocate DMA'able memory and load the DMA map for jumbo buf. */
	error = bus_dmamem_alloc(ring->jrx_jumbo_tag, (void **)&ring->jpool,
	    BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &ring->jrx_jumbo_map);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not allocate DMA'able memory for jumbo pool\n");
		goto fail;
	}

	ctx.nfe_busaddr = 0;
	error = bus_dmamap_load(ring->jrx_jumbo_tag, ring->jrx_jumbo_map,
	    ring->jpool, NFE_JMEM, nfe_dma_map_segs, &ctx, 0);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "could not load DMA'able memory for jumbo pool\n");
		goto fail;
	}

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = ring->jpool;
	for (i = 0; i < NFE_JSLOTS; i++) {
		ring->jslots[i] = ptr;
		ptr += NFE_JLEN;
		entry = malloc(sizeof(struct nfe_jpool_entry), M_DEVBUF,
		    M_WAITOK);
		if (entry == NULL) {
			device_printf(sc->nfe_dev,
			    "no memory for jumbo buffers!\n");
			error = ENOMEM;
			goto fail;
		}
		entry->slot = i;
		SLIST_INSERT_HEAD(&sc->nfe_jfree_listhead, entry,
		    jpool_entries);
	}

	return;

fail:
	/*
	 * Running without jumbo frame support is ok for most cases
	 * so don't fail on creating dma tag/map for jumbo frame.
	 */
	nfe_free_jrx_ring(sc, ring);
	device_printf(sc->nfe_dev, "disabling jumbo frame support due to "
	    "resource shortage\n");
	sc->nfe_jumbo_disable = 1;
}


static int
nfe_init_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	void *desc;
	size_t descsize;
	int i;

	ring->cur = ring->next = 0;
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}
	bzero(desc, descsize * NFE_RX_RING_COUNT);
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		if (nfe_newbuf(sc, i) != 0)
			return (ENOBUFS);
	}

	bus_dmamap_sync(ring->rx_desc_tag, ring->rx_desc_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}


static int
nfe_init_jrx_ring(struct nfe_softc *sc, struct nfe_jrx_ring *ring)
{
	void *desc;
	size_t descsize;
	int i;

	ring->jcur = ring->jnext = 0;
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->jdesc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->jdesc32;
		descsize = sizeof (struct nfe_desc32);
	}
	bzero(desc, descsize * NFE_RX_RING_COUNT);
	for (i = 0; i < NFE_JUMBO_RX_RING_COUNT; i++) {
		if (nfe_jnewbuf(sc, i) != 0)
			return (ENOBUFS);
	}

	bus_dmamap_sync(ring->jrx_desc_tag, ring->jrx_desc_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
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

	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		data = &ring->data[i];
		if (data->rx_data_map != NULL) {
			bus_dmamap_destroy(ring->rx_data_tag,
			    data->rx_data_map);
			data->rx_data_map = NULL;
		}
		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
	}
	if (ring->rx_data_tag != NULL) {
		if (ring->rx_spare_map != NULL) {
			bus_dmamap_destroy(ring->rx_data_tag,
			    ring->rx_spare_map);
			ring->rx_spare_map = NULL;
		}
		bus_dma_tag_destroy(ring->rx_data_tag);
		ring->rx_data_tag = NULL;
	}

	if (desc != NULL) {
		bus_dmamap_unload(ring->rx_desc_tag, ring->rx_desc_map);
		bus_dmamem_free(ring->rx_desc_tag, desc, ring->rx_desc_map);
		ring->desc64 = NULL;
		ring->desc32 = NULL;
		ring->rx_desc_map = NULL;
	}
	if (ring->rx_desc_tag != NULL) {
		bus_dma_tag_destroy(ring->rx_desc_tag);
		ring->rx_desc_tag = NULL;
	}
}


static void
nfe_free_jrx_ring(struct nfe_softc *sc, struct nfe_jrx_ring *ring)
{
	struct nfe_jpool_entry *entry;
	struct nfe_rx_data *data;
	void *desc;
	int i, descsize;

	if ((sc->nfe_flags & NFE_JUMBO_SUP) == 0)
		return;

	NFE_JLIST_LOCK(sc);
	while ((entry = SLIST_FIRST(&sc->nfe_jinuse_listhead))) {
		device_printf(sc->nfe_dev,
		    "asked to free buffer that is in use!\n");
		SLIST_REMOVE_HEAD(&sc->nfe_jinuse_listhead, jpool_entries);
		SLIST_INSERT_HEAD(&sc->nfe_jfree_listhead, entry,
		    jpool_entries);
	}

	while (!SLIST_EMPTY(&sc->nfe_jfree_listhead)) {
		entry = SLIST_FIRST(&sc->nfe_jfree_listhead);
		SLIST_REMOVE_HEAD(&sc->nfe_jfree_listhead, jpool_entries);
		free(entry, M_DEVBUF);
	}
        NFE_JLIST_UNLOCK(sc);

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->jdesc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->jdesc32;
		descsize = sizeof (struct nfe_desc32);
	}

	for (i = 0; i < NFE_JUMBO_RX_RING_COUNT; i++) {
		data = &ring->jdata[i];
		if (data->rx_data_map != NULL) {
			bus_dmamap_destroy(ring->jrx_data_tag,
			    data->rx_data_map);
			data->rx_data_map = NULL;
		}
		if (data->m != NULL) {
			m_freem(data->m);
			data->m = NULL;
		}
	}
	if (ring->jrx_data_tag != NULL) {
		if (ring->jrx_spare_map != NULL) {
			bus_dmamap_destroy(ring->jrx_data_tag,
			    ring->jrx_spare_map);
			ring->jrx_spare_map = NULL;
		}
		bus_dma_tag_destroy(ring->jrx_data_tag);
		ring->jrx_data_tag = NULL;
	}

	if (desc != NULL) {
		bus_dmamap_unload(ring->jrx_desc_tag, ring->jrx_desc_map);
		bus_dmamem_free(ring->jrx_desc_tag, desc, ring->jrx_desc_map);
		ring->jdesc64 = NULL;
		ring->jdesc32 = NULL;
		ring->jrx_desc_map = NULL;
	}
	/* Destroy jumbo buffer block. */
	if (ring->jrx_jumbo_map != NULL)
		bus_dmamap_unload(ring->jrx_jumbo_tag, ring->jrx_jumbo_map);
	if (ring->jrx_jumbo_map != NULL) {
		bus_dmamem_free(ring->jrx_jumbo_tag, ring->jpool,
		    ring->jrx_jumbo_map);
		ring->jpool = NULL;
		ring->jrx_jumbo_map = NULL;
	}
	if (ring->jrx_desc_tag != NULL) {
		bus_dma_tag_destroy(ring->jrx_desc_tag);
		ring->jrx_desc_tag = NULL;
	}
}


static int
nfe_alloc_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_dmamap_arg ctx;
	int i, error;
	void *desc;
	int descsize;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}

	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    NFE_RING_ALIGN, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR,			/* lowaddr */
	    BUS_SPACE_MAXADDR,			/* highaddr */
	    NULL, NULL,				/* filter, filterarg */
	    NFE_TX_RING_COUNT * descsize, 1,	/* maxsize, nsegments */
	    NFE_TX_RING_COUNT * descsize,	/* maxsegsize */
	    0,					/* flags */
	    NULL, NULL,				/* lockfunc, lockarg */
	    &ring->tx_desc_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create desc DMA tag\n");
		goto fail;
	}

	error = bus_dmamem_alloc(ring->tx_desc_tag, &desc, BUS_DMA_WAITOK |
	    BUS_DMA_COHERENT | BUS_DMA_ZERO, &ring->tx_desc_map);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create desc DMA map\n");
		goto fail;
	}
	if (sc->nfe_flags & NFE_40BIT_ADDR)
		ring->desc64 = desc;
	else
		ring->desc32 = desc;

	ctx.nfe_busaddr = 0;
	error = bus_dmamap_load(ring->tx_desc_tag, ring->tx_desc_map, desc,
	    NFE_TX_RING_COUNT * descsize, nfe_dma_map_segs, &ctx, 0);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not load desc DMA map\n");
		goto fail;
	}
	ring->physaddr = ctx.nfe_busaddr;

	error = bus_dma_tag_create(sc->nfe_parent_tag,
	    1, 0,
	    BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR,
	    NULL, NULL,
	    NFE_TSO_MAXSIZE,
	    NFE_MAX_SCATTER,
	    NFE_TSO_MAXSGSIZE,
	    0,
	    NULL, NULL,
	    &ring->tx_data_tag);
	if (error != 0) {
		device_printf(sc->nfe_dev, "could not create Tx DMA tag\n");
		goto fail;
	}

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		error = bus_dmamap_create(ring->tx_data_tag, 0,
		    &ring->data[i].tx_data_map);
		if (error != 0) {
			device_printf(sc->nfe_dev,
			    "could not create Tx DMA map\n");
			goto fail;
		}
	}

fail:
	return (error);
}


static void
nfe_init_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	void *desc;
	size_t descsize;

	sc->nfe_force_tx = 0;
	ring->queued = 0;
	ring->cur = ring->next = 0;
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc = ring->desc64;
		descsize = sizeof (struct nfe_desc64);
	} else {
		desc = ring->desc32;
		descsize = sizeof (struct nfe_desc32);
	}
	bzero(desc, descsize * NFE_TX_RING_COUNT);

	bus_dmamap_sync(ring->tx_desc_tag, ring->tx_desc_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
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

	for (i = 0; i < NFE_TX_RING_COUNT; i++) {
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(ring->tx_data_tag, data->tx_data_map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(ring->tx_data_tag, data->tx_data_map);
			m_freem(data->m);
			data->m = NULL;
		}
		if (data->tx_data_map != NULL) {
			bus_dmamap_destroy(ring->tx_data_tag,
			    data->tx_data_map);
			data->tx_data_map = NULL;
		}
	}

	if (ring->tx_data_tag != NULL) {
		bus_dma_tag_destroy(ring->tx_data_tag);
		ring->tx_data_tag = NULL;
	}

	if (desc != NULL) {
		bus_dmamap_sync(ring->tx_desc_tag, ring->tx_desc_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->tx_desc_tag, ring->tx_desc_map);
		bus_dmamem_free(ring->tx_desc_tag, desc, ring->tx_desc_map);
		ring->desc64 = NULL;
		ring->desc32 = NULL;
		ring->tx_desc_map = NULL;
		bus_dma_tag_destroy(ring->tx_desc_tag);
		ring->tx_desc_tag = NULL;
	}
}

#ifdef DEVICE_POLLING
static poll_handler_t nfe_poll;


static void
nfe_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct nfe_softc *sc = ifp->if_softc;
	uint32_t r;

	NFE_LOCK(sc);

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		NFE_UNLOCK(sc);
		return;
	}

	if (sc->nfe_framesize > MCLBYTES - ETHER_HDR_LEN)
		nfe_jrxeof(sc, count);
	else
		nfe_rxeof(sc, count);
	nfe_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		taskqueue_enqueue_fast(taskqueue_fast, &sc->nfe_tx_task);

	if (cmd == POLL_AND_CHECK_STATUS) {
		if ((r = NFE_READ(sc, sc->nfe_irq_status)) == 0) {
			NFE_UNLOCK(sc);
			return;
		}
		NFE_WRITE(sc, sc->nfe_irq_status, r);

		if (r & NFE_IRQ_LINK) {
			NFE_READ(sc, NFE_PHY_STATUS);
			NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);
			DPRINTF(sc, "link state changed\n");
		}
	}
	NFE_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

static void
nfe_set_intr(struct nfe_softc *sc)
{

	if (sc->nfe_msi != 0)
		NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED);
}


/* In MSIX, a write to mask reegisters behaves as XOR. */
static __inline void
nfe_enable_intr(struct nfe_softc *sc)
{

	if (sc->nfe_msix != 0) {
		/* XXX Should have a better way to enable interrupts! */
		if (NFE_READ(sc, sc->nfe_irq_mask) == 0)
			NFE_WRITE(sc, sc->nfe_irq_mask, sc->nfe_intrs);
	} else
		NFE_WRITE(sc, sc->nfe_irq_mask, sc->nfe_intrs);
}


static __inline void
nfe_disable_intr(struct nfe_softc *sc)
{

	if (sc->nfe_msix != 0) {
		/* XXX Should have a better way to disable interrupts! */
		if (NFE_READ(sc, sc->nfe_irq_mask) != 0)
			NFE_WRITE(sc, sc->nfe_irq_mask, sc->nfe_nointrs);
	} else
		NFE_WRITE(sc, sc->nfe_irq_mask, sc->nfe_nointrs);
}


static int
nfe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nfe_softc *sc;
	struct ifreq *ifr;
	struct mii_data *mii;
	int error, init, mask;

	sc = ifp->if_softc;
	ifr = (struct ifreq *) data;
	error = 0;
	init = 0;
	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > NFE_JUMBO_MTU)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu) {
			if ((((sc->nfe_flags & NFE_JUMBO_SUP) == 0) ||
			    (sc->nfe_jumbo_disable != 0)) &&
			    ifr->ifr_mtu > ETHERMTU)
				error = EINVAL;
			else {
				NFE_LOCK(sc);
				ifp->if_mtu = ifr->ifr_mtu;
				if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0)
					nfe_init_locked(sc);
				NFE_UNLOCK(sc);
			}
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
				nfe_stop(ifp);
		}
		sc->nfe_if_flags = ifp->if_flags;
		NFE_UNLOCK(sc);
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
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
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if ((mask & IFCAP_POLLING) != 0) {
			if ((ifr->ifr_reqcap & IFCAP_POLLING) != 0) {
				error = ether_poll_register(nfe_poll, ifp);
				if (error)
					break;
				NFE_LOCK(sc);
				nfe_disable_intr(sc);
				ifp->if_capenable |= IFCAP_POLLING;
				NFE_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				NFE_LOCK(sc);
				nfe_enable_intr(sc);
				ifp->if_capenable &= ~IFCAP_POLLING;
				NFE_UNLOCK(sc);
			}
		}
#endif /* DEVICE_POLLING */
		if ((sc->nfe_flags & NFE_HW_CSUM) != 0 &&
		    (mask & IFCAP_HWCSUM) != 0) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			if ((IFCAP_TXCSUM & ifp->if_capenable) != 0 &&
			    (IFCAP_TXCSUM & ifp->if_capabilities) != 0)
				ifp->if_hwassist |= NFE_CSUM_FEATURES;
			else
				ifp->if_hwassist &= ~NFE_CSUM_FEATURES;
			init++;
		}
		if ((sc->nfe_flags & NFE_HW_VLAN) != 0 &&
		    (mask & IFCAP_VLAN_HWTAGGING) != 0) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			init++;
		}
		/*
		 * XXX
		 * It seems that VLAN stripping requires Rx checksum offload.
		 * Unfortunately FreeBSD has no way to disable only Rx side
		 * VLAN stripping. So when we know Rx checksum offload is
		 * disabled turn entire hardware VLAN assist off.
		 */
		if ((sc->nfe_flags & (NFE_HW_CSUM | NFE_HW_VLAN)) ==
		    (NFE_HW_CSUM | NFE_HW_VLAN)) {
			if ((ifp->if_capenable & IFCAP_RXCSUM) == 0)
				ifp->if_capenable &= ~IFCAP_VLAN_HWTAGGING;
		}

		if ((sc->nfe_flags & NFE_HW_CSUM) != 0 &&
		    (mask & IFCAP_TSO4) != 0) {
			ifp->if_capenable ^= IFCAP_TSO4;
			if ((IFCAP_TSO4 & ifp->if_capenable) != 0 &&
			    (IFCAP_TSO4 & ifp->if_capabilities) != 0)
				ifp->if_hwassist |= CSUM_TSO;
			else
				ifp->if_hwassist &= ~CSUM_TSO;
		}

		if (init > 0 && (ifp->if_drv_flags & IFF_DRV_RUNNING) != 0) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			nfe_init(sc);
		}
		if ((sc->nfe_flags & NFE_HW_VLAN) != 0)
			VLAN_CAPABILITIES(ifp);
		break;
	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}


static int
nfe_intr(void *arg)
{
	struct nfe_softc *sc;
	uint32_t status;

	sc = (struct nfe_softc *)arg;

	status = NFE_READ(sc, sc->nfe_irq_status);
	if (status == 0 || status == 0xffffffff)
		return (FILTER_STRAY);
	nfe_disable_intr(sc);
	taskqueue_enqueue_fast(taskqueue_fast, &sc->nfe_int_task);

	return (FILTER_HANDLED);
}


static void
nfe_int_task(void *arg, int pending)
{
	struct nfe_softc *sc = arg;
	struct ifnet *ifp = sc->nfe_ifp;
	uint32_t r;
	int domore;

	NFE_LOCK(sc);

	if ((r = NFE_READ(sc, sc->nfe_irq_status)) == 0) {
		nfe_enable_intr(sc);
		NFE_UNLOCK(sc);
		return;	/* not for us */
	}
	NFE_WRITE(sc, sc->nfe_irq_status, r);

	DPRINTFN(sc, 5, "nfe_intr: interrupt register %x\n", r);

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		NFE_UNLOCK(sc);
		return;
	}
#endif

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		NFE_UNLOCK(sc);
		nfe_enable_intr(sc);
		return;
	}

	if (r & NFE_IRQ_LINK) {
		NFE_READ(sc, NFE_PHY_STATUS);
		NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);
		DPRINTF(sc, "link state changed\n");
	}

	domore = 0;
	/* check Rx ring */
	if (sc->nfe_framesize > MCLBYTES - ETHER_HDR_LEN)
		domore = nfe_jrxeof(sc, sc->nfe_process_limit);
	else
		domore = nfe_rxeof(sc, sc->nfe_process_limit);
	/* check Tx ring */
	nfe_txeof(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		taskqueue_enqueue_fast(taskqueue_fast, &sc->nfe_tx_task);

	NFE_UNLOCK(sc);

	if (domore || (NFE_READ(sc, sc->nfe_irq_status) != 0)) {
		taskqueue_enqueue_fast(taskqueue_fast, &sc->nfe_int_task);
		return;
	}

	/* Reenable interrupts. */
	nfe_enable_intr(sc);
}


static __inline void
nfe_discard_rxbuf(struct nfe_softc *sc, int idx)
{
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct mbuf *m;

	data = &sc->rxq.data[idx];
	m = data->m;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc64 = &sc->rxq.desc64[idx];
		/* VLAN packet may have overwritten it. */
		desc64->physaddr[0] = htole32(NFE_ADDR_HI(data->paddr));
		desc64->physaddr[1] = htole32(NFE_ADDR_LO(data->paddr));
		desc64->length = htole16(m->m_len);
		desc64->flags = htole16(NFE_RX_READY);
	} else {
		desc32 = &sc->rxq.desc32[idx];
		desc32->length = htole16(m->m_len);
		desc32->flags = htole16(NFE_RX_READY);
	}
}


static __inline void
nfe_discard_jrxbuf(struct nfe_softc *sc, int idx)
{
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct mbuf *m;

	data = &sc->jrxq.jdata[idx];
	m = data->m;

	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc64 = &sc->jrxq.jdesc64[idx];
		/* VLAN packet may have overwritten it. */
		desc64->physaddr[0] = htole32(NFE_ADDR_HI(data->paddr));
		desc64->physaddr[1] = htole32(NFE_ADDR_LO(data->paddr));
		desc64->length = htole16(m->m_len);
		desc64->flags = htole16(NFE_RX_READY);
	} else {
		desc32 = &sc->jrxq.jdesc32[idx];
		desc32->length = htole16(m->m_len);
		desc32->flags = htole16(NFE_RX_READY);
	}
}


static int
nfe_newbuf(struct nfe_softc *sc, int idx)
{
	struct nfe_rx_data *data;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct mbuf *m;
	bus_dma_segment_t segs[1];
	bus_dmamap_t map;
	int nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL)
		return (ENOBUFS);

	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, ETHER_ALIGN);

	if (bus_dmamap_load_mbuf_sg(sc->rxq.rx_data_tag, sc->rxq.rx_spare_map,
	    m, segs, &nsegs, BUS_DMA_NOWAIT) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	data = &sc->rxq.data[idx];
	if (data->m != NULL) {
		bus_dmamap_sync(sc->rxq.rx_data_tag, data->rx_data_map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxq.rx_data_tag, data->rx_data_map);
	}
	map = data->rx_data_map;
	data->rx_data_map = sc->rxq.rx_spare_map;
	sc->rxq.rx_spare_map = map;
	bus_dmamap_sync(sc->rxq.rx_data_tag, data->rx_data_map,
	    BUS_DMASYNC_PREREAD);
	data->paddr = segs[0].ds_addr;
	data->m = m;
	/* update mapping address in h/w descriptor */
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc64 = &sc->rxq.desc64[idx];
		desc64->physaddr[0] = htole32(NFE_ADDR_HI(segs[0].ds_addr));
		desc64->physaddr[1] = htole32(NFE_ADDR_LO(segs[0].ds_addr));
		desc64->length = htole16(segs[0].ds_len);
		desc64->flags = htole16(NFE_RX_READY);
	} else {
		desc32 = &sc->rxq.desc32[idx];
		desc32->physaddr = htole32(NFE_ADDR_LO(segs[0].ds_addr));
		desc32->length = htole16(segs[0].ds_len);
		desc32->flags = htole16(NFE_RX_READY);
	}

	return (0);
}


static int
nfe_jnewbuf(struct nfe_softc *sc, int idx)
{
	struct nfe_rx_data *data;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct mbuf *m;
	bus_dma_segment_t segs[1];
	bus_dmamap_t map;
	int nsegs;
	void *buf;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	buf = nfe_jalloc(sc);
	if (buf == NULL) {
		m_freem(m);
		return (ENOBUFS);
	}
	/* Attach the buffer to the mbuf. */
	MEXTADD(m, buf, NFE_JLEN, nfe_jfree, (struct nfe_softc *)sc, 0,
	    EXT_NET_DRV);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	m->m_pkthdr.len = m->m_len = NFE_JLEN;
	m_adj(m, ETHER_ALIGN);

	if (bus_dmamap_load_mbuf_sg(sc->jrxq.jrx_data_tag,
	    sc->jrxq.jrx_spare_map, m, segs, &nsegs, BUS_DMA_NOWAIT) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	KASSERT(nsegs == 1, ("%s: %d segments returned!", __func__, nsegs));

	data = &sc->jrxq.jdata[idx];
	if (data->m != NULL) {
		bus_dmamap_sync(sc->jrxq.jrx_data_tag, data->rx_data_map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->jrxq.jrx_data_tag, data->rx_data_map);
	}
	map = data->rx_data_map;
	data->rx_data_map = sc->jrxq.jrx_spare_map;
	sc->jrxq.jrx_spare_map = map;
	bus_dmamap_sync(sc->jrxq.jrx_data_tag, data->rx_data_map,
	    BUS_DMASYNC_PREREAD);
	data->paddr = segs[0].ds_addr;
	data->m = m;
	/* update mapping address in h/w descriptor */
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc64 = &sc->jrxq.jdesc64[idx];
		desc64->physaddr[0] = htole32(NFE_ADDR_HI(segs[0].ds_addr));
		desc64->physaddr[1] = htole32(NFE_ADDR_LO(segs[0].ds_addr));
		desc64->length = htole16(segs[0].ds_len);
		desc64->flags = htole16(NFE_RX_READY);
	} else {
		desc32 = &sc->jrxq.jdesc32[idx];
		desc32->physaddr = htole32(NFE_ADDR_LO(segs[0].ds_addr));
		desc32->length = htole16(segs[0].ds_len);
		desc32->flags = htole16(NFE_RX_READY);
	}

	return (0);
}


static int
nfe_rxeof(struct nfe_softc *sc, int count)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct mbuf *m;
	uint16_t flags;
	int len, prog;
	uint32_t vtag = 0;

	NFE_LOCK_ASSERT(sc);

	bus_dmamap_sync(sc->rxq.rx_desc_tag, sc->rxq.rx_desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (prog = 0;;NFE_INC(sc->rxq.cur, NFE_RX_RING_COUNT), vtag = 0) {
		if (count <= 0)
			break;
		count--;

		data = &sc->rxq.data[sc->rxq.cur];

		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->rxq.desc64[sc->rxq.cur];
			vtag = le32toh(desc64->physaddr[1]);
			flags = le16toh(desc64->flags);
			len = le16toh(desc64->length) & NFE_RX_LEN_MASK;
		} else {
			desc32 = &sc->rxq.desc32[sc->rxq.cur];
			flags = le16toh(desc32->flags);
			len = le16toh(desc32->length) & NFE_RX_LEN_MASK;
		}

		if (flags & NFE_RX_READY)
			break;
		prog++;
		if ((sc->nfe_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if (!(flags & NFE_RX_VALID_V1)) {
				ifp->if_ierrors++;
				nfe_discard_rxbuf(sc, sc->rxq.cur);
				continue;
			}
			if ((flags & NFE_RX_FIXME_V1) == NFE_RX_FIXME_V1) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		} else {
			if (!(flags & NFE_RX_VALID_V2)) {
				ifp->if_ierrors++;
				nfe_discard_rxbuf(sc, sc->rxq.cur);
				continue;
			}

			if ((flags & NFE_RX_FIXME_V2) == NFE_RX_FIXME_V2) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		}

		if (flags & NFE_RX_ERROR) {
			ifp->if_ierrors++;
			nfe_discard_rxbuf(sc, sc->rxq.cur);
			continue;
		}

		m = data->m;
		if (nfe_newbuf(sc, sc->rxq.cur) != 0) {
			ifp->if_iqdrops++;
			nfe_discard_rxbuf(sc, sc->rxq.cur);
			continue;
		}

		if ((vtag & NFE_RX_VTAG) != 0 &&
		    (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0) {
			m->m_pkthdr.ether_vtag = vtag & 0xffff;
			m->m_flags |= M_VLANTAG;
		}

		m->m_pkthdr.len = m->m_len = len;
		m->m_pkthdr.rcvif = ifp;

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			if ((flags & NFE_RX_IP_CSUMOK) != 0) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				if ((flags & NFE_RX_TCP_CSUMOK) != 0 ||
				    (flags & NFE_RX_UDP_CSUMOK) != 0) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
					m->m_pkthdr.csum_data = 0xffff;
				}
			}
		}

		ifp->if_ipackets++;

		NFE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		NFE_LOCK(sc);
	}

	if (prog > 0)
		bus_dmamap_sync(sc->rxq.rx_desc_tag, sc->rxq.rx_desc_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (count > 0 ? 0 : EAGAIN);
}


static int
nfe_jrxeof(struct nfe_softc *sc, int count)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_rx_data *data;
	struct mbuf *m;
	uint16_t flags;
	int len, prog;
	uint32_t vtag = 0;

	NFE_LOCK_ASSERT(sc);

	bus_dmamap_sync(sc->jrxq.jrx_desc_tag, sc->jrxq.jrx_desc_map,
	    BUS_DMASYNC_POSTREAD);

	for (prog = 0;;NFE_INC(sc->jrxq.jcur, NFE_JUMBO_RX_RING_COUNT),
	    vtag = 0) {
		if (count <= 0)
			break;
		count--;

		data = &sc->jrxq.jdata[sc->jrxq.jcur];

		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->jrxq.jdesc64[sc->jrxq.jcur];
			vtag = le32toh(desc64->physaddr[1]);
			flags = le16toh(desc64->flags);
			len = le16toh(desc64->length) & NFE_RX_LEN_MASK;
		} else {
			desc32 = &sc->jrxq.jdesc32[sc->jrxq.jcur];
			flags = le16toh(desc32->flags);
			len = le16toh(desc32->length) & NFE_RX_LEN_MASK;
		}

		if (flags & NFE_RX_READY)
			break;
		prog++;
		if ((sc->nfe_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if (!(flags & NFE_RX_VALID_V1)) {
				ifp->if_ierrors++;
				nfe_discard_jrxbuf(sc, sc->jrxq.jcur);
				continue;
			}
			if ((flags & NFE_RX_FIXME_V1) == NFE_RX_FIXME_V1) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		} else {
			if (!(flags & NFE_RX_VALID_V2)) {
				ifp->if_ierrors++;
				nfe_discard_jrxbuf(sc, sc->jrxq.jcur);
				continue;
			}

			if ((flags & NFE_RX_FIXME_V2) == NFE_RX_FIXME_V2) {
				flags &= ~NFE_RX_ERROR;
				len--;	/* fix buffer length */
			}
		}

		if (flags & NFE_RX_ERROR) {
			ifp->if_ierrors++;
			nfe_discard_jrxbuf(sc, sc->jrxq.jcur);
			continue;
		}

		m = data->m;
		if (nfe_jnewbuf(sc, sc->jrxq.jcur) != 0) {
			ifp->if_iqdrops++;
			nfe_discard_jrxbuf(sc, sc->jrxq.jcur);
			continue;
		}

		if ((vtag & NFE_RX_VTAG) != 0 &&
		    (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0) {
			m->m_pkthdr.ether_vtag = vtag & 0xffff;
			m->m_flags |= M_VLANTAG;
		}

		m->m_pkthdr.len = m->m_len = len;
		m->m_pkthdr.rcvif = ifp;

		if ((ifp->if_capenable & IFCAP_RXCSUM) != 0) {
			if ((flags & NFE_RX_IP_CSUMOK) != 0) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
				if ((flags & NFE_RX_TCP_CSUMOK) != 0 ||
				    (flags & NFE_RX_UDP_CSUMOK) != 0) {
					m->m_pkthdr.csum_flags |=
					    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
					m->m_pkthdr.csum_data = 0xffff;
				}
			}
		}

		ifp->if_ipackets++;

		NFE_UNLOCK(sc);
		(*ifp->if_input)(ifp, m);
		NFE_LOCK(sc);
	}

	if (prog > 0)
		bus_dmamap_sync(sc->jrxq.jrx_desc_tag, sc->jrxq.jrx_desc_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (count > 0 ? 0 : EAGAIN);
}


static void
nfe_txeof(struct nfe_softc *sc)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct nfe_desc32 *desc32;
	struct nfe_desc64 *desc64;
	struct nfe_tx_data *data = NULL;
	uint16_t flags;
	int cons, prog;

	NFE_LOCK_ASSERT(sc);

	bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map,
	    BUS_DMASYNC_POSTREAD);

	prog = 0;
	for (cons = sc->txq.next; cons != sc->txq.cur;
	    NFE_INC(cons, NFE_TX_RING_COUNT)) {
		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[cons];
			flags = le16toh(desc64->flags);
		} else {
			desc32 = &sc->txq.desc32[cons];
			flags = le16toh(desc32->flags);
		}

		if (flags & NFE_TX_VALID)
			break;

		prog++;
		sc->txq.queued--;
		data = &sc->txq.data[cons];

		if ((sc->nfe_flags & (NFE_JUMBO_SUP | NFE_40BIT_ADDR)) == 0) {
			if ((flags & NFE_TX_LASTFRAG_V1) == 0)
				continue;
			if ((flags & NFE_TX_ERROR_V1) != 0) {
				device_printf(sc->nfe_dev,
				    "tx v1 error 0x%4b\n", flags, NFE_V1_TXERR);

				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		} else {
			if ((flags & NFE_TX_LASTFRAG_V2) == 0)
				continue;
			if ((flags & NFE_TX_ERROR_V2) != 0) {
				device_printf(sc->nfe_dev,
				    "tx v2 error 0x%4b\n", flags, NFE_V2_TXERR);
				ifp->if_oerrors++;
			} else
				ifp->if_opackets++;
		}

		/* last fragment of the mbuf chain transmitted */
		KASSERT(data->m != NULL, ("%s: freeing NULL mbuf!", __func__));
		bus_dmamap_sync(sc->txq.tx_data_tag, data->tx_data_map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->txq.tx_data_tag, data->tx_data_map);
		m_freem(data->m);
		data->m = NULL;
	}

	if (prog > 0) {
		sc->nfe_force_tx = 0;
		sc->txq.next = cons;
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if (sc->txq.queued == 0)
			sc->nfe_watchdog_timer = 0;
	}
}

/*
 * It's copy of ath_defrag(ath(4)).
 *
 * Defragment an mbuf chain, returning at most maxfrags separate
 * mbufs+clusters.  If this is not possible NULL is returned and
 * the original mbuf chain is left in it's present (potentially
 * modified) state.  We use two techniques: collapsing consecutive
 * mbufs and replacing consecutive mbufs by a cluster.
 */
static struct mbuf *
nfe_defrag(struct mbuf *m0, int how, int maxfrags)
{
	struct mbuf *m, *n, *n2, **prev;
	u_int curfrags;

	/*
	 * Calculate the current number of frags.
	 */
	curfrags = 0;
	for (m = m0; m != NULL; m = m->m_next)
		curfrags++;
	/*
	 * First, try to collapse mbufs.  Note that we always collapse
	 * towards the front so we don't need to deal with moving the
	 * pkthdr.  This may be suboptimal if the first mbuf has much
	 * less data than the following.
	 */
	m = m0;
again:
	for (;;) {
		n = m->m_next;
		if (n == NULL)
			break;
		if ((m->m_flags & M_RDONLY) == 0 &&
		    n->m_len < M_TRAILINGSPACE(m)) {
			bcopy(mtod(n, void *), mtod(m, char *) + m->m_len,
				n->m_len);
			m->m_len += n->m_len;
			m->m_next = n->m_next;
			m_free(n);
			if (--curfrags <= maxfrags)
				return (m0);
		} else
			m = n;
	}
	KASSERT(maxfrags > 1,
		("maxfrags %u, but normal collapse failed", maxfrags));
	/*
	 * Collapse consecutive mbufs to a cluster.
	 */
	prev = &m0->m_next;		/* NB: not the first mbuf */
	while ((n = *prev) != NULL) {
		if ((n2 = n->m_next) != NULL &&
		    n->m_len + n2->m_len < MCLBYTES) {
			m = m_getcl(how, MT_DATA, 0);
			if (m == NULL)
				goto bad;
			bcopy(mtod(n, void *), mtod(m, void *), n->m_len);
			bcopy(mtod(n2, void *), mtod(m, char *) + n->m_len,
				n2->m_len);
			m->m_len = n->m_len + n2->m_len;
			m->m_next = n2->m_next;
			*prev = m;
			m_free(n);
			m_free(n2);
			if (--curfrags <= maxfrags)	/* +1 cl -2 mbufs */
				return m0;
			/*
			 * Still not there, try the normal collapse
			 * again before we allocate another cluster.
			 */
			goto again;
		}
		prev = &n->m_next;
	}
	/*
	 * No place where we can collapse to a cluster; punt.
	 * This can occur if, for example, you request 2 frags
	 * but the packet requires that both be clusters (we
	 * never reallocate the first mbuf to avoid moving the
	 * packet header).
	 */
bad:
	return (NULL);
}


static int
nfe_encap(struct nfe_softc *sc, struct mbuf **m_head)
{
	struct nfe_desc32 *desc32 = NULL;
	struct nfe_desc64 *desc64 = NULL;
	bus_dmamap_t map;
	bus_dma_segment_t segs[NFE_MAX_SCATTER];
	int error, i, nsegs, prod, si;
	uint32_t tso_segsz;
	uint16_t cflags, flags;
	struct mbuf *m;

	prod = si = sc->txq.cur;
	map = sc->txq.data[prod].tx_data_map;

	error = bus_dmamap_load_mbuf_sg(sc->txq.tx_data_tag, map, *m_head, segs,
	    &nsegs, BUS_DMA_NOWAIT);
	if (error == EFBIG) {
		m = nfe_defrag(*m_head, M_DONTWAIT, NFE_MAX_SCATTER);
		if (m == NULL) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
		*m_head = m;
		error = bus_dmamap_load_mbuf_sg(sc->txq.tx_data_tag, map,
		    *m_head, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(*m_head);
			*m_head = NULL;
			return (ENOBUFS);
		}
	} else if (error != 0)
		return (error);
	if (nsegs == 0) {
		m_freem(*m_head);
		*m_head = NULL;
		return (EIO);
	}

	if (sc->txq.queued + nsegs >= NFE_TX_RING_COUNT - 2) {
		bus_dmamap_unload(sc->txq.tx_data_tag, map);
		return (ENOBUFS);
	}

	m = *m_head;
	cflags = flags = 0;
	tso_segsz = 0;
	if ((m->m_pkthdr.csum_flags & NFE_CSUM_FEATURES) != 0) {
		if ((m->m_pkthdr.csum_flags & CSUM_IP) != 0)
			cflags |= NFE_TX_IP_CSUM;
		if ((m->m_pkthdr.csum_flags & CSUM_TCP) != 0)
			cflags |= NFE_TX_TCP_UDP_CSUM;
		if ((m->m_pkthdr.csum_flags & CSUM_UDP) != 0)
			cflags |= NFE_TX_TCP_UDP_CSUM;
	}
	if ((m->m_pkthdr.csum_flags & CSUM_TSO) != 0) {
		tso_segsz = (uint32_t)m->m_pkthdr.tso_segsz <<
		    NFE_TX_TSO_SHIFT;
		cflags &= ~(NFE_TX_IP_CSUM | NFE_TX_TCP_UDP_CSUM);
		cflags |= NFE_TX_TSO;
	}

	for (i = 0; i < nsegs; i++) {
		if (sc->nfe_flags & NFE_40BIT_ADDR) {
			desc64 = &sc->txq.desc64[prod];
			desc64->physaddr[0] =
			    htole32(NFE_ADDR_HI(segs[i].ds_addr));
			desc64->physaddr[1] =
			    htole32(NFE_ADDR_LO(segs[i].ds_addr));
			desc64->vtag = 0;
			desc64->length = htole16(segs[i].ds_len - 1);
			desc64->flags = htole16(flags);
		} else {
			desc32 = &sc->txq.desc32[prod];
			desc32->physaddr =
			    htole32(NFE_ADDR_LO(segs[i].ds_addr));
			desc32->length = htole16(segs[i].ds_len - 1);
			desc32->flags = htole16(flags);
		}

		/*
		 * Setting of the valid bit in the first descriptor is
		 * deferred until the whole chain is fully setup.
		 */
		flags |= NFE_TX_VALID;

		sc->txq.queued++;
		NFE_INC(prod, NFE_TX_RING_COUNT);
	}

	/*
	 * the whole mbuf chain has been DMA mapped, fix last/first descriptor.
	 * csum flags, vtag and TSO belong to the first fragment only.
	 */
	if (sc->nfe_flags & NFE_40BIT_ADDR) {
		desc64->flags |= htole16(NFE_TX_LASTFRAG_V2);
		desc64 = &sc->txq.desc64[si];
		if ((m->m_flags & M_VLANTAG) != 0)
			desc64->vtag = htole32(NFE_TX_VTAG |
			    m->m_pkthdr.ether_vtag);
		if (tso_segsz != 0) {
			/*
			 * XXX
			 * The following indicates the descriptor element
			 * is a 32bit quantity.
			 */
			desc64->length |= htole16((uint16_t)tso_segsz);
			desc64->flags |= htole16(tso_segsz >> 16);
		}
		/*
		 * finally, set the valid/checksum/TSO bit in the first
		 * descriptor.
		 */
		desc64->flags |= htole16(NFE_TX_VALID | cflags);
	} else {
		if (sc->nfe_flags & NFE_JUMBO_SUP)
			desc32->flags |= htole16(NFE_TX_LASTFRAG_V2);
		else
			desc32->flags |= htole16(NFE_TX_LASTFRAG_V1);
		desc32 = &sc->txq.desc32[si];
		if (tso_segsz != 0) {
			/*
			 * XXX
			 * The following indicates the descriptor element
			 * is a 32bit quantity.
			 */
			desc32->length |= htole16((uint16_t)tso_segsz);
			desc32->flags |= htole16(tso_segsz >> 16);
		}
		/*
		 * finally, set the valid/checksum/TSO bit in the first
		 * descriptor.
		 */
		desc32->flags |= htole16(NFE_TX_VALID | cflags);
	}

	sc->txq.cur = prod;
	prod = (prod + NFE_TX_RING_COUNT - 1) % NFE_TX_RING_COUNT;
	sc->txq.data[si].tx_data_map = sc->txq.data[prod].tx_data_map;
	sc->txq.data[prod].tx_data_map = map;
	sc->txq.data[prod].m = m;

	bus_dmamap_sync(sc->txq.tx_data_tag, map, BUS_DMASYNC_PREWRITE);

	return (0);
}


static void
nfe_setmulti(struct nfe_softc *sc)
{
	struct ifnet *ifp = sc->nfe_ifp;
	struct ifmultiaddr *ifma;
	int i;
	uint32_t filter;
	uint8_t addr[ETHER_ADDR_LEN], mask[ETHER_ADDR_LEN];
	uint8_t etherbroadcastaddr[ETHER_ADDR_LEN] = {
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

	filter = NFE_READ(sc, NFE_RXFILTER);
	filter &= NFE_PFF_RX_PAUSE;
	filter |= NFE_RXFILTER_MAGIC;
	filter |= (ifp->if_flags & IFF_PROMISC) ? NFE_PFF_PROMISC : NFE_PFF_U2M;
	NFE_WRITE(sc, NFE_RXFILTER, filter);
}


static void
nfe_tx_task(void *arg, int pending)
{
	struct ifnet *ifp;

	ifp = (struct ifnet *)arg;
	nfe_start(ifp);
}


static void
nfe_start(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mbuf *m0;
	int enq;

	NFE_LOCK(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || sc->nfe_link == 0) {
		NFE_UNLOCK(sc);
		return;
	}

	for (enq = 0; !IFQ_DRV_IS_EMPTY(&ifp->if_snd);) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		if (nfe_encap(sc, &m0) != 0) {
			if (m0 == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m0);
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		enq++;
		ETHER_BPF_MTAP(ifp, m0);
	}

	if (enq > 0) {
		bus_dmamap_sync(sc->txq.tx_desc_tag, sc->txq.tx_desc_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* kick Tx */
		NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_KICKTX | sc->rxtxctl);

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		sc->nfe_watchdog_timer = 5;
	}

	NFE_UNLOCK(sc);
}


static void
nfe_watchdog(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;

	if (sc->nfe_watchdog_timer == 0 || --sc->nfe_watchdog_timer)
		return;

	/* Check if we've lost Tx completion interrupt. */
	nfe_txeof(sc);
	if (sc->txq.queued == 0) {
		if_printf(ifp, "watchdog timeout (missed Tx interrupts) "
		    "-- recovering\n");
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			taskqueue_enqueue_fast(taskqueue_fast,
			    &sc->nfe_tx_task);
		return;
	}
	/* Check if we've lost start Tx command. */
	sc->nfe_force_tx++;
	if (sc->nfe_force_tx <= 3) {
		/*
		 * If this is the case for watchdog timeout, the following
		 * code should go to nfe_txeof().
		 */
		NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_KICKTX | sc->rxtxctl);
		return;
	}
	sc->nfe_force_tx = 0;

	if_printf(ifp, "watchdog timeout\n");

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	ifp->if_oerrors++;
	nfe_init_locked(sc);
}


static void
nfe_init(void *xsc)
{
	struct nfe_softc *sc = xsc;

	NFE_LOCK(sc);
	nfe_init_locked(sc);
	NFE_UNLOCK(sc);
}


static void
nfe_init_locked(void *xsc)
{
	struct nfe_softc *sc = xsc;
	struct ifnet *ifp = sc->nfe_ifp;
	struct mii_data *mii;
	uint32_t val;
	int error;

	NFE_LOCK_ASSERT(sc);

	mii = device_get_softc(sc->nfe_miibus);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	nfe_stop(ifp);

	sc->nfe_framesize = ifp->if_mtu + NFE_RX_HEADERS;

	nfe_init_tx_ring(sc, &sc->txq);
	if (sc->nfe_framesize > (MCLBYTES - ETHER_HDR_LEN))
		error = nfe_init_jrx_ring(sc, &sc->jrxq);
	else
		error = nfe_init_rx_ring(sc, &sc->rxq);
	if (error != 0) {
		device_printf(sc->nfe_dev,
		    "initialization failed: no memory for rx buffers\n");
		nfe_stop(ifp);
		return;
	}

	val = 0;
	if ((sc->nfe_flags & NFE_CORRECT_MACADDR) != 0)
		val |= NFE_MAC_ADDR_INORDER;
	NFE_WRITE(sc, NFE_TX_UNK, val);
	NFE_WRITE(sc, NFE_STATUS, 0);

	if ((sc->nfe_flags & NFE_TX_FLOW_CTRL) != 0)
		NFE_WRITE(sc, NFE_TX_PAUSE_FRAME, NFE_TX_PAUSE_FRAME_DISABLE);

	sc->rxtxctl = NFE_RXTX_BIT2;
	if (sc->nfe_flags & NFE_40BIT_ADDR)
		sc->rxtxctl |= NFE_RXTX_V3MAGIC;
	else if (sc->nfe_flags & NFE_JUMBO_SUP)
		sc->rxtxctl |= NFE_RXTX_V2MAGIC;

	if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
		sc->rxtxctl |= NFE_RXTX_RXCSUM;
	if ((ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0)
		sc->rxtxctl |= NFE_RXTX_VTAG_INSERT | NFE_RXTX_VTAG_STRIP;

	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | sc->rxtxctl);
	DELAY(10);
	NFE_WRITE(sc, NFE_RXTX_CTL, sc->rxtxctl);

	if ((ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0)
		NFE_WRITE(sc, NFE_VTAG_CTL, NFE_VTAG_ENABLE);
	else
		NFE_WRITE(sc, NFE_VTAG_CTL, 0);

	NFE_WRITE(sc, NFE_SETUP_R6, 0);

	/* set MAC address */
	nfe_set_macaddr(sc, IF_LLADDR(ifp));

	/* tell MAC where rings are in memory */
	if (sc->nfe_framesize > MCLBYTES - ETHER_HDR_LEN) {
		NFE_WRITE(sc, NFE_RX_RING_ADDR_HI,
		    NFE_ADDR_HI(sc->jrxq.jphysaddr));
		NFE_WRITE(sc, NFE_RX_RING_ADDR_LO,
		    NFE_ADDR_LO(sc->jrxq.jphysaddr));
	} else {
		NFE_WRITE(sc, NFE_RX_RING_ADDR_HI,
		    NFE_ADDR_HI(sc->rxq.physaddr));
		NFE_WRITE(sc, NFE_RX_RING_ADDR_LO,
		    NFE_ADDR_LO(sc->rxq.physaddr));
	}
	NFE_WRITE(sc, NFE_TX_RING_ADDR_HI, NFE_ADDR_HI(sc->txq.physaddr));
	NFE_WRITE(sc, NFE_TX_RING_ADDR_LO, NFE_ADDR_LO(sc->txq.physaddr));

	NFE_WRITE(sc, NFE_RING_SIZE,
	    (NFE_RX_RING_COUNT - 1) << 16 |
	    (NFE_TX_RING_COUNT - 1));

	NFE_WRITE(sc, NFE_RXBUFSZ, sc->nfe_framesize);

	/* force MAC to wakeup */
	val = NFE_READ(sc, NFE_PWR_STATE);
	if ((val & NFE_PWR_WAKEUP) == 0)
		NFE_WRITE(sc, NFE_PWR_STATE, val | NFE_PWR_WAKEUP);
	DELAY(10);
	val = NFE_READ(sc, NFE_PWR_STATE);
	NFE_WRITE(sc, NFE_PWR_STATE, val | NFE_PWR_VALID);

#if 1
	/* configure interrupts coalescing/mitigation */
	NFE_WRITE(sc, NFE_IMTIMER, NFE_IM_DEFAULT);
#else
	/* no interrupt mitigation: one interrupt per packet */
	NFE_WRITE(sc, NFE_IMTIMER, 970);
#endif

	NFE_WRITE(sc, NFE_SETUP_R1, NFE_R1_MAGIC_10_100);
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

	/* enable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, NFE_RX_START);

	/* enable Tx */
	NFE_WRITE(sc, NFE_TX_CTL, NFE_TX_START);

	NFE_WRITE(sc, NFE_PHY_STATUS, 0xf);

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		nfe_disable_intr(sc);
	else
#endif
	nfe_set_intr(sc);
	nfe_enable_intr(sc); /* enable interrupts */

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	sc->nfe_link = 0;
	mii_mediachg(mii);

	callout_reset(&sc->nfe_stat_ch, hz, nfe_tick, sc);
}


static void
nfe_stop(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct nfe_rx_ring *rx_ring;
	struct nfe_jrx_ring *jrx_ring;
	struct nfe_tx_ring *tx_ring;
	struct nfe_rx_data *rdata;
	struct nfe_tx_data *tdata;
	int i;

	NFE_LOCK_ASSERT(sc);

	sc->nfe_watchdog_timer = 0;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	callout_stop(&sc->nfe_stat_ch);

	/* abort Tx */
	NFE_WRITE(sc, NFE_TX_CTL, 0);

	/* disable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, 0);

	/* disable interrupts */
	nfe_disable_intr(sc);

	sc->nfe_link = 0;

	/* free Rx and Tx mbufs still in the queues. */
	rx_ring = &sc->rxq;
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		rdata = &rx_ring->data[i];
		if (rdata->m != NULL) {
			bus_dmamap_sync(rx_ring->rx_data_tag,
			    rdata->rx_data_map, BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rx_ring->rx_data_tag,
			    rdata->rx_data_map);
			m_freem(rdata->m);
			rdata->m = NULL;
		}
	}

	if ((sc->nfe_flags & NFE_JUMBO_SUP) != 0) {
		jrx_ring = &sc->jrxq;
		for (i = 0; i < NFE_JUMBO_RX_RING_COUNT; i++) {
			rdata = &jrx_ring->jdata[i];
			if (rdata->m != NULL) {
				bus_dmamap_sync(jrx_ring->jrx_data_tag,
				    rdata->rx_data_map, BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(jrx_ring->jrx_data_tag,
				    rdata->rx_data_map);
				m_freem(rdata->m);
				rdata->m = NULL;
			}
		}
	}

	tx_ring = &sc->txq;
	for (i = 0; i < NFE_RX_RING_COUNT; i++) {
		tdata = &tx_ring->data[i];
		if (tdata->m != NULL) {
			bus_dmamap_sync(tx_ring->tx_data_tag,
			    tdata->tx_data_map, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(tx_ring->tx_data_tag,
			    tdata->tx_data_map);
			m_freem(tdata->m);
			tdata->m = NULL;
		}
	}
}


static int
nfe_ifmedia_upd(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mii_data *mii;

	NFE_LOCK(sc);
	mii = device_get_softc(sc->nfe_miibus);
	mii_mediachg(mii);
	NFE_UNLOCK(sc);

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
}


void
nfe_tick(void *xsc)
{
	struct nfe_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;

	sc = (struct nfe_softc *)xsc;

	NFE_LOCK_ASSERT(sc);

	ifp = sc->nfe_ifp;

	mii = device_get_softc(sc->nfe_miibus);
	mii_tick(mii);
	nfe_watchdog(ifp);
	callout_reset(&sc->nfe_stat_ch, hz, nfe_tick, sc);
}


static void
nfe_shutdown(device_t dev)
{
	struct nfe_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);

	NFE_LOCK(sc);
	ifp = sc->nfe_ifp;
	nfe_stop(ifp);
	/* nfe_reset(sc); */
	NFE_UNLOCK(sc);
}


static void
nfe_get_macaddr(struct nfe_softc *sc, uint8_t *addr)
{
	uint32_t val;

	if ((sc->nfe_flags & NFE_CORRECT_MACADDR) == 0) {
		val = NFE_READ(sc, NFE_MACADDR_LO);
		addr[0] = (val >> 8) & 0xff;
		addr[1] = (val & 0xff);

		val = NFE_READ(sc, NFE_MACADDR_HI);
		addr[2] = (val >> 24) & 0xff;
		addr[3] = (val >> 16) & 0xff;
		addr[4] = (val >>  8) & 0xff;
		addr[5] = (val & 0xff);
	} else {
		val = NFE_READ(sc, NFE_MACADDR_LO);
		addr[5] = (val >> 8) & 0xff;
		addr[4] = (val & 0xff);

		val = NFE_READ(sc, NFE_MACADDR_HI);
		addr[3] = (val >> 24) & 0xff;
		addr[2] = (val >> 16) & 0xff;
		addr[1] = (val >>  8) & 0xff;
		addr[0] = (val & 0xff);
	}
}


static void
nfe_set_macaddr(struct nfe_softc *sc, uint8_t *addr)
{

	NFE_WRITE(sc, NFE_MACADDR_LO, addr[5] <<  8 | addr[4]);
	NFE_WRITE(sc, NFE_MACADDR_HI, addr[3] << 24 | addr[2] << 16 |
	    addr[1] << 8 | addr[0]);
}


/*
 * Map a single buffer address.
 */

static void
nfe_dma_map_segs(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct nfe_dmamap_arg *ctx;

	if (error != 0)
		return;

	KASSERT(nseg == 1, ("too many DMA segments, %d should be 1", nseg));

	ctx = (struct nfe_dmamap_arg *)arg;
	ctx->nfe_busaddr = segs[0].ds_addr;
}


static int
sysctl_int_range(SYSCTL_HANDLER_ARGS, int low, int high)
{
	int error, value;

	if (!arg1)
		return (EINVAL);
	value = *(int *)arg1;
	error = sysctl_handle_int(oidp, &value, 0, req);
	if (error || !req->newptr)
		return (error);
	if (value < low || value > high)
		return (EINVAL);
	*(int *)arg1 = value;

	return (0);
}


static int
sysctl_hw_nfe_proc_limit(SYSCTL_HANDLER_ARGS)
{

	return (sysctl_int_range(oidp, arg1, arg2, req, NFE_PROC_MIN,
	    NFE_PROC_MAX));
}
