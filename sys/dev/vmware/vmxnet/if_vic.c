/*-
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
 * Copyright (c) 2006 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2012 Bryan Venteicher <bryanv@freebsd.org>
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
 *
 * $OpenBSD: if_vic.c,v 1.77 2011/11/29 11:53:25 jsing Exp $
 */

/* Driver for VMware Virtual NIC ("vmxnet") devices. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/ethernet.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/bpf.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "if_vicreg.h"
#include "if_vicvar.h"

/*
 * The behavior of the second Rx queue is a bit uncertain. It appears to
 * be capable holding fragments from the first Rx queue. It also appears
 * capable of containing entire packets. IMO the former behavior is more
 * desirable, but I cannot determine how to enable it, so default to the
 * later. The two behaviors cannot be used simultaneously.
 */
#define VIC_NOFRAG_RXQUEUE

static int	vic_probe(device_t);
static int	vic_attach(device_t);
static int	vic_detach(device_t);
static int	vic_shutdown(device_t);

static int	vic_query(struct vic_softc *);
static uint32_t	vic_read(struct vic_softc *, bus_size_t);
static void	vic_write(struct vic_softc *, bus_size_t, uint32_t);
static uint32_t	vic_read_cmd(struct vic_softc *, uint32_t);

static int	vic_alloc_ring_bufs(struct vic_softc *);
static void	vic_init_shared_mem(struct vic_softc *);
static int	vic_alloc_data(struct vic_softc *);
static void	vic_free_data(struct vic_softc *);
static void	vic_dmamap_cb(void *, bus_dma_segment_t *, int, int);
static int	vic_alloc_dma(struct vic_softc *);
static void	vic_free_dma(struct vic_softc *);

static int	vic_init_rings(struct vic_softc *sc);
static void	vic_init_locked(struct vic_softc *);
static void	vic_init(void *);

static int 	vic_encap_load_mbuf(struct vic_softc *, struct mbuf **, int,
		    bus_dmamap_t, bus_dma_segment_t [], int *);
static void 	vic_assign_sge(struct vic_sg *, bus_dma_segment_t *);
static int	vic_encap(struct vic_softc *, struct mbuf **);
static void	vic_start_locked(struct ifnet *);
static void	vic_start(struct ifnet *);
static void	vic_watchdog(struct vic_softc *);

static void	vic_free_rx_rings(struct vic_softc *);
static void	vic_free_tx_ring(struct vic_softc *);
static void	vic_tx_quiesce_wait(struct vic_softc *);
static void	vic_stop(struct vic_softc *);

static int	vic_newbuf(struct vic_softc *, struct vic_rxqueue *, int);
static void	vic_rxeof_discard(struct vic_softc *, struct vic_rxqueue *,
		    int);
static void	vic_rxeof_discard_frags(struct vic_softc *);
static int 	vic_rxeof_frag(struct vic_softc *, struct mbuf *);
static void	vic_rxeof(struct vic_softc *, int);
static void	vic_txeof(struct vic_softc *);
static void	vic_intr(void *);

static void	vic_set_ring_sizes(struct vic_softc *);
static void	vic_link_state(struct vic_softc *);
static void	vic_set_rxfilter(struct vic_softc *);
static void	vic_get_lladdr(struct vic_softc *);
static void	vic_set_lladdr(struct vic_softc *);
static int	vic_media_change(struct ifnet *);
static void	vic_media_status(struct ifnet *, struct ifmediareq *);
static int	vic_ioctl(struct ifnet *, u_long, caddr_t);
static void	vic_tick(void *);

static int	vic_pcnet_masquerade(device_t);
static void	vic_pcnet_restore(struct vic_softc *);
static int	vic_pcnet_transform(struct vic_softc *);

static void	vic_sysctl_node(struct vic_softc *);

static void	vic_barrier(struct vic_softc *, int);

#define VIC_VMWARE_VENDORID	0x15AD
#define VIC_VMWARE_DEVICEID	0x0720
#define VIC_PCNET_VENDORID	0x1022 /* PCN_VENDORID */
#define VIC_PCNET_DEVICEID	0x2000 /* PCN_DEVICEID_PCNET */

static device_method_t vic_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		vic_probe),
	DEVMETHOD(device_attach,	vic_attach),
	DEVMETHOD(device_detach,	vic_detach),
	DEVMETHOD(device_shutdown,	vic_shutdown),

	DEVMETHOD_END
};

static driver_t vic_driver = {
	"vic", vic_methods, sizeof(struct vic_softc)
};

static devclass_t vic_devclass;
DRIVER_MODULE(vic, pci, vic_driver, vic_devclass, 0, 0);

MODULE_DEPEND(vic, pci, 1, 1, 1);
MODULE_DEPEND(vic, ether, 1, 1, 1);

static int
vic_probe(device_t dev)
{
	uint16_t vendorid, deviceid;

	vendorid = pci_get_vendor(dev);
	deviceid = pci_get_device(dev);

	if (vendorid == VIC_VMWARE_VENDORID &&
	    deviceid == VIC_VMWARE_DEVICEID) {
		device_set_desc(dev, "VMWare Ethernet Adapter");
		return (BUS_PROBE_DEFAULT);
	}

	if (vendorid == VIC_PCNET_VENDORID &&
	    deviceid == VIC_PCNET_DEVICEID) {
		/*
		 * The hypervisor can present us with a PCNet device
		 * that we can transform to a vmxnet interface.
		 */
		if (vic_pcnet_masquerade(dev) == 0) {
			device_set_desc(dev,
			    "VMWare (Flexible) Ethernet Adapter");
			return (BUS_PROBE_VENDOR);
		}
	}

	return (ENXIO);
}

static int
vic_attach(device_t dev)
{
	struct vic_softc *sc;
	struct ifnet *ifp;
	int rid, error;

	sc = device_get_softc(dev);
	sc->vic_dev = dev;

	VIC_LOCK_INIT(sc, device_get_nameunit(dev));
	VIC_RX_LOCK_INIT(sc, device_get_nameunit(dev));
	VIC_TX_LOCK_INIT(sc, device_get_nameunit(dev));
	callout_init_mtx(&sc->vic_tick, &sc->vic_mtx, 0);

	rid = PCIR_BAR(VIC_PCI_BAR);
	sc->vic_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid,
	    RF_ACTIVE);
	if (sc->vic_res == NULL) {
		device_printf(dev, "could not map BAR(VIC_PCI_BAR) memory\n");
		error = ENXIO;
		goto fail;
	}

	sc->vic_iot = rman_get_bustag(sc->vic_res);
	sc->vic_ioh = rman_get_bushandle(sc->vic_res);

	rid = 0;
	sc->vic_irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->vic_irq == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		error = ENXIO;
		goto fail;
	}

	if (pci_get_vendor(dev) == VIC_PCNET_VENDORID &&
	    pci_get_device(dev) == VIC_PCNET_DEVICEID) {
		/* Turn this 'flexible' adapter into a vmxnet device. */
		error = vic_pcnet_transform(sc);
		if (error)
			goto fail;
	}

	if (vic_query(sc) != 0) {
		error = ENXIO;
		goto fail;
	}

	if (vic_alloc_data(sc) != 0) {
		error = ENXIO;
		goto fail;
	}

	ifp = sc->vic_ifp = if_alloc(IFT_ETHER);
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	if_initbaudrate(ifp, IF_Gbps(1));
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = vic_init;
	ifp->if_ioctl = vic_ioctl;
	ifp->if_start = vic_start;
	ifp->if_snd.ifq_drv_maxlen = sc->vic_tx_nbufs - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->vic_tx_nbufs - 1);
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, sc->vic_lladdr);

	/* Tell the upper layer(s) we support long frames. */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	if (sc->vic_cap & VIC_CMD_HWCAP_VLAN)
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	if (sc->vic_cap & VIC_CMD_HWCAP_CSUM) {
		ifp->if_capabilities |= IFCAP_RXCSUM | IFCAP_TXCSUM;
		ifp->if_hwassist |= VIC_CSUM_FEATURES;
	}

	ifp->if_capenable = ifp->if_capabilities;

	if (sc->vic_flags & VIC_FLAGS_TSO) {
		ifp->if_hwassist |= CSUM_TSO;
		if (sc->vic_cap & VIC_CMD_HWCAP_TSO)
			ifp->if_capabilities |= IFCAP_TSO4;
	}
	if (sc->vic_flags & VIC_FLAGS_LRO)
		ifp->if_capabilities |= IFCAP_LRO;

	ifmedia_init(&sc->vic_media, 0, vic_media_change, vic_media_status);
	ifmedia_add(&sc->vic_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->vic_media, IFM_ETHER | IFM_AUTO);

	error = bus_setup_intr(dev, sc->vic_irq,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, vic_intr, sc, &sc->vic_intrhand);
	if (error) {
		ether_ifdetach(ifp);
		device_printf(dev, "could not set up interrupt\n");
		goto fail;
	}

	if (bootverbose) {
		device_printf(dev,
		    "feature 0x%b cap 0x%b rxbuf %d/%d txbuf %d\n",
		    sc->vic_feature, VIC_CMD_FEATURE_BITS, sc->vic_cap,
		    VIC_CMD_HWCAP_BITS, sc->vic_rxq[0].nbufs,
		    sc->vic_rxq[1].nbufs, sc->vic_tx_nbufs);
	}

	vic_sysctl_node(sc);

fail:
	if (error)
		vic_detach(dev);

	return (error);
}

static int
vic_detach(device_t dev)
{
	struct vic_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->vic_ifp;

	if (device_is_attached(dev)) {
		ether_ifdetach(ifp);
		VIC_LOCK(sc);
		vic_stop(sc);
		VIC_UNLOCK(sc);
		callout_drain(&sc->vic_tick);
	}

	if (sc->vic_intrhand != NULL) {
		bus_teardown_intr(dev, sc->vic_irq, sc->vic_intrhand);
		sc->vic_intrhand = NULL;
	}

	if (ifp != NULL) {
		if_free(ifp);
		sc->vic_ifp = NULL;
	}

	vic_free_data(sc);

	if (sc->vic_irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->vic_irq);
		sc->vic_irq = NULL;
	}

	if (sc->vic_res != NULL) {
		if (sc->vic_flags & VIC_FLAGS_MORPHED_PCNET)
			vic_pcnet_restore(sc);

		bus_release_resource(dev, SYS_RES_IOPORT,
		    PCIR_BAR(VIC_PCI_BAR), sc->vic_res);
		sc->vic_res = NULL;
	}

	VIC_TX_LOCK_DESTROY(sc);
	VIC_RX_LOCK_DESTROY(sc);
	VIC_LOCK_DESTROY(sc);

	return (0);
}

static int
vic_shutdown(device_t dev)
{

	return (0);
}

static int
vic_query(struct vic_softc *sc)
{
	device_t dev;
	uint32_t major, minor;

	dev = sc->vic_dev;
	major = vic_read(sc, VIC_VERSION_MAJOR);
	minor = vic_read(sc, VIC_VERSION_MINOR);

	/* Check for a supported version. */
	if ((major & VIC_VERSION_MAJOR_M) !=
	    (VIC_MAGIC & VIC_VERSION_MAJOR_M)) {
		device_printf(dev, "magic mismatch\n");
		return (1);
	}

	if (VIC_MAGIC > major || VIC_MAGIC < minor) {
		device_printf(dev, "unsupported version (%#X)\n",
		    major & ~VIC_VERSION_MAJOR_M);
		return (1);
	}

	sc->vic_cap = vic_read_cmd(sc, VIC_CMD_HWCAP);
	sc->vic_feature = vic_read_cmd(sc, VIC_CMD_FEATURE);

	vic_get_lladdr(sc);

	if (sc->vic_feature & VIC_CMD_FEATURE_JUMBO)
		sc->vic_flags |= VIC_FLAGS_JUMBO;

	if (sc->vic_cap & VIC_CMD_HWCAP_LPD &&
	    sc->vic_cap & VIC_CMD_HWCAP_RX_CHAIN &&
	    sc->vic_feature & VIC_CMD_FEATURE_LPD)
		sc->vic_flags |= VIC_FLAGS_LRO;

	if (sc->vic_cap & VIC_CMD_HWCAP_SG)
		sc->vic_sg_max = VIC_SG_MAX;
	else
		sc->vic_sg_max = 1;

	if (sc->vic_cap & VIC_CMD_HWCAP_SG &&
	    sc->vic_cap & VIC_CMD_HWCAP_TSO &&
	    sc->vic_cap & VIC_CMD_HWCAP_TX_CHAIN &&
	    sc->vic_feature & VIC_CMD_FEATURE_TSO)
		sc->vic_flags |= VIC_FLAGS_TSO;

	if (sc->vic_flags & VIC_VMXNET2_FLAGS)
		sc->vic_flags |= VIC_FLAGS_ENHANCED;

	vic_set_ring_sizes(sc);

	return (0);
}

static uint32_t
vic_read(struct vic_softc *sc, bus_size_t r)
{

	r += sc->vic_ioadj;

	bus_space_barrier(sc->vic_iot, sc->vic_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	return (bus_space_read_4(sc->vic_iot, sc->vic_ioh, r));
}

static void
vic_write(struct vic_softc *sc, bus_size_t r, uint32_t v)
{

	r += sc->vic_ioadj;

	bus_space_write_4(sc->vic_iot, sc->vic_ioh, r, v);
	bus_space_barrier(sc->vic_iot, sc->vic_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

static uint32_t
vic_read_cmd(struct vic_softc *sc, u_int32_t cmd)
{

	vic_write(sc, VIC_CMD, cmd);
	return (vic_read(sc, VIC_CMD));
}

static int
vic_alloc_ring_bufs(struct vic_softc *sc)
{
	device_t dev;
	struct vic_rxqueue *rxq;
	int q;

	dev = sc->vic_dev;

	for (q = 0; q < VIC_NRXRINGS; q++) {
		rxq = &sc->vic_rxq[q];

		if (q == 0)
			rxq->pktlen = MCLBYTES;
		else
			rxq->pktlen = MJUMPAGESIZE;

		rxq->bufs = malloc(sizeof(struct vic_rxbuf) * rxq->nbufs,
		    M_DEVBUF, M_NOWAIT | M_ZERO);
		if (rxq->bufs == NULL) {
			device_printf(dev,
			    "unable to allocate rxbuf for ring %d\n", q);
			return (ENOMEM);
		}
	}

	sc->vic_txbuf = malloc(sizeof(struct vic_txbuf) * sc->vic_tx_nbufs,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->vic_txbuf == NULL) {
		device_printf(dev, "unable to allocate txbuf\n");
		return (ENOMEM);
	}

	return (0);
}

static void
vic_init_shared_mem(struct vic_softc *sc)
{
	uint8_t *kva;
	u_int offset;
	int q;

	kva = sc->vic_dma_kva;
	sc->vic_data = (struct vic_data *) kva;

	sc->vic_data->vd_magic = VIC_MAGIC;
	sc->vic_data->vd_length = sc->vic_dma_size;

	offset = sizeof(struct vic_data);

	for (q = 0; q < VIC_NRXRINGS; q++) {
		sc->vic_rxq[q].slots = (struct vic_rxdesc *) &kva[offset];
		sc->vic_data->vd_rx_offset[q] = offset;
		sc->vic_data->vd_rx[q].length = sc->vic_rxq[q].nbufs;

		offset += sc->vic_rxq[q].nbufs * sizeof(struct vic_rxdesc);
	}

	sc->vic_txq = (struct vic_txdesc *) &kva[offset];
	sc->vic_data->vd_tx_offset = offset;
	sc->vic_data->vd_tx_length = sc->vic_tx_nbufs;

	if (sc->vic_flags & VIC_FLAGS_TSO)
		sc->vic_data->vd_tx_maxfrags = VIC_TSO_MAXSEGS;
	else
		sc->vic_data->vd_tx_maxfrags = sc->vic_sg_max;
}

static int
vic_alloc_data(struct vic_softc *sc)
{
	int error;

	error = vic_alloc_ring_bufs(sc);
	if (error)
		return (error);

	error = vic_alloc_dma(sc);
	if (error)
		return (error);

	vic_init_shared_mem(sc);

	return (0);
}

static void
vic_free_data(struct vic_softc *sc)
{
	int q;

	vic_free_dma(sc);

	if (sc->vic_txbuf != NULL) {
		free(sc->vic_txbuf, M_DEVBUF);
		sc->vic_txbuf = NULL;
	}

	for (q = 0; q < VIC_NRXRINGS; q++) {
		if (sc->vic_rxq[q].bufs != NULL) {
			free(sc->vic_rxq[q].bufs, M_DEVBUF);
			sc->vic_rxq[q].bufs = NULL;
		}
	}
}

static void
vic_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *baddr = arg;

	if (error == 0)
		*baddr = segs->ds_addr;
}

static int
vic_alloc_dma(struct vic_softc *sc)
{
	device_t dev;
	struct vic_rxbuf *rxb;
	struct vic_txbuf *txb;
	struct vic_rxqueue *rxq;
	size_t size;
	bus_size_t txmaxsz, txnsegs;
	int q, i, error;

	dev = sc->vic_dev;

	/*
	 * Calculate the size of all the structures shared with the
	 * host. This allocation must be physically contiguous.
	 */
	size = sizeof(struct vic_data);
	for (q = 0; q < VIC_NRXRINGS; q++)
		size += sc->vic_rxq[q].nbufs * sizeof(struct vic_rxdesc);
	size += sc->vic_tx_nbufs * sizeof(struct vic_txdesc);
	sc->vic_dma_size = size;

	error = bus_dma_tag_create(bus_get_dma_tag(dev),
	    PAGE_SIZE, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    sc->vic_dma_size,		/* maxsize */
	    1,				/* nsegments */
	    sc->vic_dma_size,		/* maxsegsize */
	    BUS_DMA_ALLOCNOW,		/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->vic_dma_tag);
	if (error) {
		device_printf(dev, "cannot create dma tag\n");
		return (error);
	}

	error = bus_dmamem_alloc(sc->vic_dma_tag, (void **) &sc->vic_dma_kva,
	    BUS_DMA_NOWAIT | BUS_DMA_ZERO, &sc->vic_dma_map);
	if (error) {
		device_printf(dev, "cannot allocate dma memory\n");
		return (error);
	}

	error = bus_dmamap_load(sc->vic_dma_tag, sc->vic_dma_map,
	    sc->vic_dma_kva, sc->vic_dma_size, vic_dmamap_cb,
	    &sc->vic_dma_paddr, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "cannot load dmamap\n");
		return (error);
	}

	for (q = 0; q < VIC_NRXRINGS; q++) {
		rxq = &sc->vic_rxq[q];

		error = bus_dma_tag_create(bus_get_dma_tag(dev),
		    1, 0,			/* alignment, boundary */
		    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
		    BUS_SPACE_MAXADDR,		/* highaddr */
		    NULL, NULL,			/* filter, filterarg */
		    rxq->pktlen,		/* maxsize */
		    1,				/* nsegments */
		    rxq->pktlen,		/* maxsegsize */
		    0,				/* flags */
		    NULL, NULL,			/* lockfunc, lockarg */
		    &rxq->tag);
		if (error) {
			device_printf(dev,
			    "cannot create Rx buffer tag for ring %d\n", q);
			return (error);
		}

		error = bus_dmamap_create(rxq->tag, 0, &rxq->spare_dmamap);
		if (error) {
			device_printf(dev, "unable to create spare dmamap "
			    "for ring %d\n", q);
			return (error);
		}

		for (i = 0; i < rxq->nbufs; i++) {
			rxb = &rxq->bufs[i];

			error = bus_dmamap_create(rxq->tag, 0,
			    &rxb->rxb_dmamap);
			if (error) {
				device_printf(dev, "unable to create dmamap "
				    "for ring %d slot %d\n", q, i);
				return (error);
			}
		}
	}

	if (sc->vic_flags & VIC_FLAGS_TSO) {
		txmaxsz = VIC_TSO_MAXSIZE;
		txnsegs = VIC_TSO_MAXSEGS;
	} else {
		txmaxsz = sc->vic_sg_max * VIC_TX_MAXSEGSIZE;
		txnsegs = sc->vic_sg_max;
	}

	error = bus_dma_tag_create(bus_get_dma_tag(dev),
	    1, 0,			/* alignment, boundary */
	    BUS_SPACE_MAXADDR_32BIT,	/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    txmaxsz,			/* maxsize */
	    txnsegs,			/* nsegments */
	    VIC_TX_MAXSEGSIZE,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &sc->vic_tx_tag);
	if (error) {
		device_printf(dev, "unable to create Tx buffer tag\n");
		return (error);
	}

	for (i = 0; i < sc->vic_tx_nbufs; i++) {
		txb = &sc->vic_txbuf[i];

		error = bus_dmamap_create(sc->vic_tx_tag, 0, &txb->txb_dmamap);
		if (error) {
			device_printf(dev,
			    "unable to create dmamap for tx %d\n", i);
			return (error);
		}
	}

	return (0);
}

static void
vic_free_dma(struct vic_softc *sc)
{
	struct vic_txbuf *txb;
	struct vic_rxbuf *rxb;
	struct vic_rxqueue *rxq;
	int q, i;

	if (sc->vic_tx_tag != NULL) {
		for (i = 0; i < sc->vic_tx_nbufs; i++) {
			txb = &sc->vic_txbuf[i];

			if (txb->txb_dmamap != NULL) {
				bus_dmamap_destroy(sc->vic_tx_tag,
				    txb->txb_dmamap);
				txb->txb_dmamap = NULL;
			}
		}

		bus_dma_tag_destroy(sc->vic_tx_tag);
		sc->vic_tx_tag = NULL;
	}

	for (q = 0; q < VIC_NRXRINGS; q++) {
		rxq = &sc->vic_rxq[q];

		if (rxq->tag == NULL)
			continue;

		if (rxq->spare_dmamap != NULL) {
			bus_dmamap_destroy(rxq->tag, rxq->spare_dmamap);
			rxq->spare_dmamap = NULL;
		}

		for (i = 0; i < rxq->nbufs; i++) {
			rxb = &rxq->bufs[i];

			if (rxb->rxb_dmamap != NULL) {
				bus_dmamap_destroy(rxq->tag, rxb->rxb_dmamap);
				rxb->rxb_dmamap = NULL;
			}
		}

		bus_dma_tag_destroy(rxq->tag);
		rxq->tag = NULL;
	}

	if (sc->vic_dma_tag != NULL) {
		if (sc->vic_dma_map != NULL)
			bus_dmamap_unload(sc->vic_dma_tag,
			    sc->vic_dma_map);
		if (sc->vic_dma_map != NULL && sc->vic_dma_kva != NULL)
			bus_dmamem_free(sc->vic_dma_tag, sc->vic_dma_kva,
			    sc->vic_dma_map);
		sc->vic_dma_kva = NULL;
		sc->vic_dma_map = NULL;

		bus_dma_tag_destroy(sc->vic_dma_tag);
		sc->vic_dma_tag = NULL;
	}
}

static int
vic_init_rings(struct vic_softc *sc)
{
	struct vic_rxqueue *rxq;
	struct vic_txdesc *txd;
	int q, i, error;

	for (q = 0; q < VIC_NRXRINGS; q++) {
		rxq = &sc->vic_rxq[q];

		sc->vic_data->vd_rx[q].nextidx = 0;
		sc->vic_data->vd_rx_saved_nextidx[q] = 0;

		for (i = 0; i < rxq->nbufs; i++) {
			error = vic_newbuf(sc, rxq, i);
			if (error)
				return (error);
		}
	}

	for (i = 0; i < sc->vic_tx_nbufs; i++) {
		txd = &sc->vic_txq[i];

		txd->tx_flags = 0;
		txd->tx_tsomss = 0;
		txd->tx_owner = VIC_OWNER_DRIVER;
	}

	sc->vic_data->vd_tx_curidx = 0;
	sc->vic_data->vd_tx_nextidx = 0;
	sc->vic_data->vd_tx_stopped = 0;
	sc->vic_data->vd_tx_queued = 0;
	sc->vic_data->vd_tx_saved_nextidx = 0;

	return (0);
}

static void
vic_init_locked(struct vic_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vic_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	vic_stop(sc);

	if (vic_init_rings(sc) != 0) {
		vic_stop(sc);
		return;
	}

	bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	vic_write(sc, VIC_DATA_ADDR, sc->vic_dma_paddr);
	vic_write(sc, VIC_DATA_LENGTH, sc->vic_dma_size);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	vic_set_rxfilter(sc);
	vic_write(sc, VIC_CMD, VIC_CMD_INTR_ENABLE);

	callout_reset(&sc->vic_tick, hz, vic_tick, sc);
}

static void
vic_init(void *xsc)
{
	struct vic_softc *sc;

	sc = xsc;

	VIC_LOCK(sc);
	vic_init_locked(sc);
	VIC_UNLOCK(sc);
}

static int
vic_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vic_softc *sc;
	struct ifreq *ifr;
#if defined(INET) || defined(INET6)
	struct ifaddr *ifa = data;
	int avoid_reset = 0;
#endif
	int mask, error;

	sc = ifp->if_softc;
	ifr = (struct ifreq *) data;
	error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			avoid_reset = 1;
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6)
			avoid_reset = 1;
#endif
#if defined(INET) || defined(INET6)
		if (avoid_reset != 0) {
			ifp->if_flags |= IFF_UP;
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				vic_init(adapter);
			if ((ifp->if_flags & IFF_NOARP) == 0)
				arp_ifinit(ifp, ifa);
		} else
			error = ether_ioctl(ifp, command, data);
#endif
		break;

	case SIOCSIFMTU:
		VIC_LOCK(sc);
		if (ifr->ifr_mtu < ETHERMIN)
			error = EINVAL;
		else if (ifr->ifr_mtu > ETHERMTU &&
		    (sc->vic_flags & VIC_FLAGS_JUMBO) == 0)
			error = EINVAL;
		else if (ifr->ifr_mtu > VIC_JUMBO_MTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		VIC_UNLOCK(sc);
		break;

	case SIOCSIFFLAGS:
		VIC_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ sc->vic_if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					vic_set_rxfilter(sc);
				}
			} else
				vic_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				vic_stop(sc);
		}
		sc->vic_if_flags = ifp->if_flags;
		VIC_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		VIC_LOCK(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			vic_set_rxfilter(sc);
		VIC_UNLOCK(sc);
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->vic_media, cmd);
		break;

	case SIOCSIFCAP:
		VIC_LOCK(sc);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if ((mask & IFCAP_TXCSUM) != 0 &&
		    (ifp->if_capabilities & IFCAP_TXCSUM) != 0) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			if (ifp->if_capenable & IFCAP_TXCSUM)
				ifp->if_hwassist |= VIC_CSUM_FEATURES;
			else
				ifp->if_hwassist &= ~VIC_CSUM_FEATURES;
		}

		if ((mask & IFCAP_RXCSUM) != 0 &&
		    (ifp->if_capabilities & IFCAP_RXCSUM) != 0) {
			/*
			 * We cannot seem to be able to disable this on the
			 * host, but we can just ignore the checksum advice
			 * it provides. Depending on the behavior of VMWare,
			 * this could cause breakages when the source is a
			 * VM in the same host.
			 */
			ifp->if_capenable ^= IFCAP_RXCSUM;
		}

		if ((mask & IFCAP_TSO4) != 0 &&
		    (ifp->if_capabilities & IFCAP_TSO4) != 0) {
			ifp->if_capenable ^= IFCAP_TSO4;
			if ((ifp->if_capenable & IFCAP_TSO4) != 0)
				ifp->if_hwassist |= CSUM_TSO;
			else
				ifp->if_hwassist &= ~CSUM_TSO;
		}

		if ((mask & IFCAP_LRO) != 0 &&
		    (ifp->if_capabilities & IFCAP_LRO) != 0) {
			ifp->if_capenable ^= IFCAP_LRO;
			if (ifp->if_capenable & IFCAP_LRO)
				sc->vic_data->vd_features |=
				    VIC_CMD_FEATURE_LPD;
			else
				sc->vic_data->vd_features &=
				    ~VIC_CMD_FEATURE_LPD;
		}

		VIC_UNLOCK(sc);
		VLAN_CAPABILITIES(ifp);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	return (error);
}

static int
vic_encap_load_mbuf(struct vic_softc *sc, struct mbuf **m0, int tso,
    bus_dmamap_t dmap, bus_dma_segment_t segs[], int *nsegs)
{
	struct mbuf *m;
	bus_dma_tag_t tag;
	int maxsegs, error;

	m = *m0;
	tag = sc->vic_tx_tag;
	maxsegs = tso ? VIC_TSO_MAXSEGS : sc->vic_sg_max;

	error = bus_dmamap_load_mbuf_sg(tag, dmap, m, segs, nsegs, 0);
	if (error == 0) {
		/*
		 * When TSO is available, the Tx DMA map is set up to hold the
		 * maximum possible packet size. But for non-TSO packets, we
		 * don't want to have to chain Tx descriptors together.
		 */
		if (*nsegs <= maxsegs)
			return (0);

		/* Collapse the mbuf and retry. */
		bus_dmamap_unload(tag, dmap);

	} else if (error != EFBIG)
		return (error);

	m = m_collapse(m, M_NOWAIT, maxsegs);
	if (m != NULL) {
		*m0 = m;
		error = bus_dmamap_load_mbuf_sg(tag, dmap, m, segs, nsegs, 0);
	} else
		error = ENOBUFS;

	if (error) {
		m_freem(*m0);
		*m0 = NULL;
	}

	return (error);
}

static void
vic_assign_sge(struct vic_sg *sge, bus_dma_segment_t *seg)
{

	sge->sg_addr_low = seg->ds_addr & 0xFFFFFFFF;
#if defined(__amd64__) || defined(PAE)
	sge->sg_addr_high = (seg->ds_addr >> 32) & 0xFFFF;
#endif
	sge->sg_length = seg->ds_len;
}

static int
vic_encap(struct vic_softc *sc, struct mbuf **m0)
{
	struct ifnet *ifp;
	struct mbuf *m;
	struct vic_sg *sge;
	struct vic_txbuf *txb, *htxb;
	struct vic_txdesc *txd, *htxd;
	bus_dmamap_t dmap;
	bus_dma_segment_t txsegs[VIC_TSO_MAXSEGS];
	int i, idx, tso, avail, nsegs, sgidx, error;

	M_ASSERTPKTHDR(*m0);

	ifp = sc->vic_ifp;
	tso = ((*m0)->m_pkthdr.csum_flags & CSUM_TSO) != 0;

	idx = sc->vic_data->vd_tx_nextidx;
	if (idx >= sc->vic_data->vd_tx_length) {
		ifp->if_oerrors++;
		if_printf(ifp, "tx idx is corrupt\n");
		return (ENXIO);
	}

	txd = &sc->vic_txq[idx];
	txb = &sc->vic_txbuf[idx];
	dmap = txb->txb_dmamap;

	if (txb->txb_m != NULL) {
		sc->vic_data->vd_tx_stopped = 1;
		ifp->if_oerrors++;
		if_printf(ifp, "tx ring is corrupt\n");
		return (ENXIO);
	}

	if (tso != 0) {
		/*
		 * We don't know how many descriptors will be required
		 * until after the load_mbuf() below, so check for the
		 * worse case here.
		 */
		avail = sc->vic_tx_nbufs - sc->vic_tx_pending;
		if (avail < VIC_TSO_MAX_CHAINED)
			return (ENOBUFS);
	}

	error = vic_encap_load_mbuf(sc, m0, tso, dmap, txsegs, &nsegs);
	if (error)
		return (error);
	KASSERT(nsegs <= VIC_TSO_MAXSEGS,
	    ("%s: mbuf %p with too many segments %d", __func__, *m0, nsegs));

	txb->txb_m = m = *m0;
	bus_dmamap_sync(sc->vic_tx_tag, dmap, BUS_DMASYNC_PREWRITE);
	txd->tx_flags = 0;

	/*
	 * Fill the SG arrays of the Tx descriptor chain.
	 */
	htxd = txd;
	htxb = txb;
	for (i = 0, sgidx = 0; i < nsegs; i++, sgidx++) {
		/* Advance to the next descriptor if this one is full. */
		if (sgidx == VIC_SG_MAX) {
			txd->tx_sa.sa_length = VIC_SG_MAX;
			txd->tx_sa.sa_addr_type = VIC_SG_ADDR_PHYS;
			txd->tx_flags = VIC_TX_FLAGS_CHAINED;

			sc->vic_tx_pending++;
			VIC_INC(sc->vic_data->vd_tx_nextidx,
			    sc->vic_data->vd_tx_length);

			idx = sc->vic_data->vd_tx_nextidx;
			txd = &sc->vic_txq[idx];
			txb = &sc->vic_txbuf[idx];
			KASSERT(txb->txb_m == NULL,
			    ("%s: desc %p with mbuf", __func__, txb));
			KASSERT(txd->tx_owner == VIC_OWNER_DRIVER,
			    ("%s: desc %p bad owner %d", __func__, txd,
			    txd->tx_owner));
			txd->tx_owner = VIC_OWNER_NIC;
			txd->tx_flags = 0;
			txd->tx_tsomss = 0;
			sgidx = 0;
		}

		sge = &txd->tx_sa.sa_sg[sgidx];
		vic_assign_sge(sge, &txsegs[i]);
	}
	txd->tx_sa.sa_length = sgidx;
	txd->tx_sa.sa_addr_type = VIC_SG_ADDR_PHYS;

	/*
	 * Finish filling in the first Tx descriptor.
	 */
	if (m->m_pkthdr.csum_flags & VIC_CSUM_FEATURES)
		htxd->tx_flags |= VIC_TX_FLAGS_CSUMHW | VIC_TX_FLAGS_KEEP;
	else
		htxd->tx_flags |= VIC_TX_FLAGS_KEEP;
	if (tso != 0) {
		htxd->tx_flags |= VIC_TX_FLAGS_TSO;
		htxd->tx_tsomss = m->m_pkthdr.tso_segsz;
	} else
		htxd->tx_tsomss = 0;

	sc->vic_tx_pending++;
	if (VIC_TXURN_WARN(sc))
		htxd->tx_flags |= VIC_TX_FLAGS_TXURN;

	vic_barrier(sc, BUS_SPACE_BARRIER_WRITE);
	htxd->tx_owner = VIC_OWNER_NIC;

	bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	VIC_INC(sc->vic_data->vd_tx_nextidx,
	    sc->vic_data->vd_tx_length);

	return (0);
}

static void
vic_start_locked(struct ifnet *ifp)
{
	struct vic_softc *sc;
	struct mbuf *m_head;
	int tx;

	sc = ifp->if_softc;
	tx = 0;

	VIC_TX_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	for (;;) {
		if (VIC_TXURN(sc))
			break;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (vic_encap(sc, &m_head) != 0) {
			if (m_head == NULL)
				break;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		tx++;
		ETHER_BPF_MTAP(ifp, m_head);
	}

	if (tx > 0) {
		bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
		    BUS_DMASYNC_PREWRITE);
		vic_read(sc, VIC_Tx_ADDR);
		sc->vic_watchdog_timer = VIC_WATCHDOG_TIMEOUT;
	}
}

static void
vic_start(struct ifnet *ifp)
{
	struct vic_softc *sc;

	sc = ifp->if_softc;

	VIC_TX_LOCK(sc);
	vic_start_locked(ifp);
	VIC_TX_UNLOCK(sc);
}

static void
vic_watchdog(struct vic_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vic_ifp;

	VIC_TX_LOCK(sc);
	if (sc->vic_watchdog_timer == 0 || --sc->vic_watchdog_timer) {
		VIC_TX_UNLOCK(sc);
		return;
	}
	VIC_TX_UNLOCK(sc);

	if_printf(ifp, "watchdog timeout -- resetting\n");
	ifp->if_oerrors++;
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	/*
	 * XXX BMV Due to an apparent lack of support on the host
	 * side, I doubt this will actually unwedge the device.
	 */
	sc->vic_flags |= VIC_FLAGS_WDTIMEOUT;
	vic_init_locked(sc);
	sc->vic_flags &= ~VIC_FLAGS_WDTIMEOUT;
}

static void
vic_free_rx_rings(struct vic_softc *sc)
{
	struct vic_rxbuf *rxb;
	struct vic_rxdesc *rxd;
	struct vic_rxqueue *rxq;
	int i, q;

	for (q = 0; q < VIC_NRXRINGS; q++) {
		rxq = &sc->vic_rxq[q];

		for (i = 0; i < rxq->nbufs; i++) {
			rxb = &rxq->bufs[i];
			rxd = &rxq->slots[i];

			if (rxb->rxb_m == NULL)
				continue;

			bus_dmamap_sync(rxq->tag, rxb->rxb_dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxq->tag, rxb->rxb_dmamap);
			m_freem(rxb->rxb_m);
			rxb->rxb_m = NULL;
		}
	}
}

static void
vic_free_tx_ring(struct vic_softc *sc)
{
	struct vic_txbuf *txb;
	int i;

	for (i = 0; i < sc->vic_tx_nbufs; i++) {
		txb = &sc->vic_txbuf[i];

		if (txb->txb_m != NULL) {
			bus_dmamap_sync(sc->vic_tx_tag, txb->txb_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->vic_tx_tag, txb->txb_dmamap);
			m_freem(txb->txb_m);
			txb->txb_m = NULL;
		}
	}
}

static void
vic_tx_quiesce_wait(struct vic_softc *sc)
{
	int i, tries;

	/*
	 * Do not bother waiting any more if this is a watchdog timeout
	 * since we have already waited at least 5 seconds.
	 */
	tries = sc->vic_flags & VIC_FLAGS_WDTIMEOUT ? 0 : 1500;

	for (i = 0; i < tries && sc->vic_tx_pending > 0; i++) {
		vic_write(sc, VIC_CMD, VIC_CMD_Tx_DONE); /* XXX */
		DELAY(1000);
		vic_txeof(sc);
	}

	if (i == tries) {
		device_printf(sc->vic_dev,
		    "dropping %d transmits still outstanding\n",
		    sc->vic_tx_pending);
		sc->vic_tx_pending = 0;
	}
}

static void
vic_stop(struct vic_softc *sc)
{
	struct ifnet *ifp;

	VIC_LOCK_ASSERT(sc);

	ifp = sc->vic_ifp;

	callout_stop(&sc->vic_tick);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;

	/* Disable interrupts. */
	vic_write(sc, VIC_CMD, VIC_CMD_INTR_DISABLE);

	/*
	 * Wait for all the in-flight Tx buffers to complete.
	 *
	 * XXX BMV Is there a way to just halt the interface?
	 * This make the watchdog basically worthless.
	 */
	VIC_TX_LOCK(sc);
	if (sc->vic_tx_pending > 0)
		vic_tx_quiesce_wait(sc);
	sc->vic_data->vd_tx_stopped = 1;
	VIC_TX_UNLOCK(sc);

	sc->vic_data->vd_iff = 0;
	vic_write(sc, VIC_CMD, VIC_CMD_IFF);

	vic_write(sc, VIC_DATA_ADDR, 0);

	vic_free_rx_rings(sc);
	vic_free_tx_ring(sc);
}

static int
vic_newbuf(struct vic_softc *sc, struct vic_rxqueue *rxq, int idx)
{
	struct mbuf *m;
	struct vic_rxbuf *rxb;
	struct vic_rxdesc *rxd;
	bus_dmamap_t map;
	bus_dma_segment_t segs[1];
	int nsegs, frag, error;

	rxb = &rxq->bufs[idx];
	rxd = &rxq->slots[idx];
	map = rxq->spare_dmamap;
#ifdef VIC_NOFRAG_RXQUEUE
	frag = 0;
#else
	frag = &sc->vic_rxq[0] != rxq;
#endif

	m = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, rxq->pktlen);
	if (m == NULL)
		return (ENOBUFS);
	m->m_len = m->m_pkthdr.len = rxq->pktlen;
	if (frag == 0)
		m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf_sg(rxq->tag, map, m, &segs[0], &nsegs,
	    BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		return (error);
	}
	KASSERT(nsegs == 1,
	    ("%s: mbuf %p with too many segments %d", __func__, m, nsegs));

	if (rxb->rxb_m != NULL) {
		bus_dmamap_sync(rxq->tag, rxb->rxb_dmamap,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rxq->tag, rxb->rxb_dmamap);
	}

	rxq->spare_dmamap = rxb->rxb_dmamap;
	rxb->rxb_dmamap = map;
	rxb->rxb_m = m;

	rxd->rx_physaddr = segs[0].ds_addr;
	rxd->rx_buflength = segs[0].ds_len;
	rxd->rx_length = 0;
	rxd->rx_flags = 0;
	vic_barrier(sc, BUS_SPACE_BARRIER_WRITE);
	if (frag == 0)
		rxd->rx_owner = VIC_OWNER_NIC;
	else
		rxd->rx_owner = VIC_OWNER_NIC_FRAG;

	bus_dmamap_sync(rxq->tag, map, BUS_DMASYNC_PREREAD);

	return (0);
}

static void
vic_rxeof_discard(struct vic_softc *sc, struct vic_rxqueue *rxq, int idx)
{
	struct vic_rxdesc *rxd;
	int frag;

#ifdef VIC_NOFRAG_RXQUEUE
	frag = 0;
#else
	frag = &sc->vic_rxq[0] != rxq;
#endif

	rxd = &rxq->slots[idx];
	rxd->rx_flags = 0;
	rxd->rx_length = 0;
	vic_barrier(sc, BUS_SPACE_BARRIER_WRITE);
	if (frag == 0)
		rxd->rx_owner = VIC_OWNER_NIC;
	else
		rxd->rx_owner = VIC_OWNER_NIC_FRAG;
}

static void
vic_rxeof_discard_frags(struct vic_softc *sc)
{
	struct vic_rxqueue *rxq;
	struct vic_rxdesc *rxd;
	int q, flags, idx;

	q = VIC_FRAG_RXRING_IDX;
	rxq = &sc->vic_rxq[q];

	for (;;) {
		idx = sc->vic_data->vd_rx[q].nextidx;
		if (idx >= sc->vic_data->vd_rx[q].length)
			break;
		rxd = &rxq->slots[idx];

		vic_barrier(sc,
		    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
		KASSERT(rxd->rx_owner == VIC_OWNER_DRIVER_FRAG,
		    ("%s: desc %p bad owner %d", __func__, rxd,
		    rxd->rx_owner));
		flags = rxd->rx_flags;
		vic_rxeof_discard(sc, rxq, idx);

		VIC_INC(sc->vic_data->vd_rx[q].nextidx,
		    sc->vic_data->vd_rx[q].length);

		if (flags & VIC_RX_FLAGS_FRAG_EOP)
			break;
	}
}

static int
vic_rxeof_frag(struct vic_softc *sc, struct mbuf *m_head)
{
	struct ifnet *ifp;
	struct vic_rxqueue *rxq;
	struct vic_rxbuf *rxb;
	struct vic_rxdesc *rxd;
	struct mbuf *m, *m_tail;
	int q, idx, flags, length;

	ifp = sc->vic_ifp;
	q = VIC_FRAG_RXRING_IDX;
	rxq = &sc->vic_rxq[q];
	m_tail = m_head;

	for (;;) {
		idx = sc->vic_data->vd_rx[q].nextidx;
		if (idx >= sc->vic_data->vd_rx[q].length) {
			if_printf(ifp, "bogus frag receive index %d\n", idx);
			goto fail;
		}

		rxd = &rxq->slots[idx];
		rxb = &rxq->bufs[idx];

		if (rxd->rx_owner != VIC_OWNER_DRIVER_FRAG) {
			if_printf(ifp, "incorrect frag owner %d",
			    rxd->rx_owner);
			vic_rxeof_discard_frags(sc);
			goto fail;
		}

		m = rxb->rxb_m;
		KASSERT(m != NULL,
		    ("%s: queue %d idx %d without mbuf", __func__, q, idx));

		vic_barrier(sc, BUS_SPACE_BARRIER_READ);
		flags = rxd->rx_flags;
		length = rxd->rx_length;

		if (length == 0) {
			/*
			 * XXX BMV This should not happen, but we seem to
			 * have bad luck with zero length'ed fragments in
			 * the primary queue.
			 *
			 * Do not fail - just ignore this mbuf for now.
			 */
			vic_rxeof_discard(sc, rxq, idx);
			goto nextp;
		}

		if (vic_newbuf(sc, rxq, idx) != 0) {
			vic_rxeof_discard_frags(sc);
			goto fail;
		}

		m->m_len = length;
		m->m_flags &= ~M_PKTHDR;

		m_head->m_pkthdr.len += length;
		m_tail->m_next = m;
		m_tail = m;

nextp:
		bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
		    BUS_DMASYNC_PREWRITE);
		VIC_INC(sc->vic_data->vd_rx[q].nextidx,
		    sc->vic_data->vd_rx[q].length);

		if (flags & VIC_RX_FLAGS_FRAG_EOP)
			break;
	}

	return (0);

fail:
	m_freem(m_head);

	return (1);
}

static void
vic_rxeof(struct vic_softc *sc, int q)
{
	struct ifnet *ifp;
	struct vic_rxbuf *rxb;
	struct vic_rxdesc *rxd;
	struct vic_rxqueue *rxq;
	struct mbuf *m;
	int idx, flags, length;

	ifp = sc->vic_ifp;
	rxq = &sc->vic_rxq[q];

	VIC_RX_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (;;) {
		idx = sc->vic_data->vd_rx[q].nextidx;
		if (idx >= sc->vic_data->vd_rx[q].length) {
			if_printf(ifp, "bogus receive index %d\n", idx);
			ifp->if_ierrors++;
			break;
		}

		rxd = &rxq->slots[idx];
		rxb = &rxq->bufs[idx];

		if (rxd->rx_owner != VIC_OWNER_DRIVER)
			break;

		m = rxb->rxb_m;
		KASSERT(m != NULL,
		    ("%s: queue %d idx %d without mbuf", __func__, q, idx));

		vic_barrier(sc, BUS_SPACE_BARRIER_READ);
		flags = rxd->rx_flags;
		length = rxd->rx_length;

		if (length < VIC_MIN_FRAMELEN) {
			/*
			 * XXX BMV This only seems to occur on VMware Fusion,
			 * often with every other received frame. ESXi works
			 * fine. No clue as to why.
			 *
			 * if_printf(ifp,
			 *    "rx queue %d idx %d short length %d\n",
			 *    q, idx, length);
			 */
			vic_rxeof_discard(sc, rxq, idx);
			if (flags & VIC_RX_FLAGS_FRAG)
				vic_rxeof_discard_frags(sc);
			ifp->if_iqdrops++;
			goto nextp;
		}

		if (vic_newbuf(sc, rxq, idx) != 0) {
			vic_rxeof_discard(sc, rxq, idx);
			if (flags & VIC_RX_FLAGS_FRAG)
				vic_rxeof_discard_frags(sc);
			ifp->if_iqdrops++;
			goto nextp;
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = length;

		if (flags & VIC_RX_FLAGS_FRAG) {
			if (vic_rxeof_frag(sc, m) != 0) {
				/* vic_rxeof_frag() freed m. */
				ifp->if_iqdrops++;
				goto nextp;
			}
		}

		if (ifp->if_capenable & IFCAP_RXCSUM &&
		    flags & VIC_RX_FLAGS_CSUMHW_OK) {
			m->m_pkthdr.csum_data = 0xFFFF;
			m->m_pkthdr.csum_flags |= CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR;
		}

		ifp->if_ipackets++;
		(*ifp->if_input)(ifp, m);

nextp:
		bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
		    BUS_DMASYNC_PREWRITE);
		VIC_INC(sc->vic_data->vd_rx[q].nextidx,
		    sc->vic_data->vd_rx[q].length);
	}
}

static void
vic_txeof(struct vic_softc *sc)
{
	struct ifnet *ifp;
	struct vic_txdesc *txd;
	struct vic_txbuf *txb;
	int idx, deq;

	ifp = sc->vic_ifp;
	deq = 0;

	VIC_TX_LOCK_ASSERT(sc);

	bus_dmamap_sync(sc->vic_dma_tag, sc->vic_dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	while (sc->vic_tx_pending > 0) {
		idx = sc->vic_data->vd_tx_curidx;
		if (idx >= sc->vic_data->vd_tx_length) {
			ifp->if_oerrors++;
			break;
		}

		txd = &sc->vic_txq[idx];
		vic_barrier(sc, BUS_SPACE_BARRIER_READ);
		if (txd->tx_owner != VIC_OWNER_DRIVER)
			break;

		txb = &sc->vic_txbuf[idx];
		if (txb->txb_m != NULL) {
			bus_dmamap_sync(sc->vic_tx_tag, txb->txb_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->vic_tx_tag, txb->txb_dmamap);

			m_freem(txb->txb_m);
			txb->txb_m = NULL;

			ifp->if_opackets++;
		}

		deq++;
		sc->vic_tx_pending--;
		VIC_INC(sc->vic_data->vd_tx_curidx,
		    sc->vic_data->vd_tx_length);
	}

	if (deq > 0) {
		sc->vic_data->vd_tx_stopped = 0;
		if (sc->vic_tx_pending == 0)
			sc->vic_watchdog_timer = 0;
	}
}

static void
vic_intr(void *arg)
{
	struct vic_softc *sc;
	struct ifnet *ifp;
	int q, nrxrings;

	sc = arg;
	ifp = sc->vic_ifp;

#ifdef VIC_NOFRAG_RXQUEUE
	nrxrings = VIC_NRXRINGS;
#else
	nrxrings = 1;
#endif
	VIC_RX_LOCK(sc);
	for (q = 0; q < nrxrings; q++)
		vic_rxeof(sc, q);
	VIC_RX_UNLOCK(sc);

	VIC_TX_LOCK(sc);
	vic_txeof(sc);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vic_start_locked(ifp);
	VIC_TX_UNLOCK(sc);

	/* Re-enable interrupts. */
	vic_write(sc, VIC_CMD, VIC_CMD_INTR_ACK);
}

static void
vic_set_ring_sizes(struct vic_softc *sc)
{
	int q, enhanced;
	uint32_t nrxbufs;
	static const int maxnrxbuf[] = { VIC_NBUF, VIC_ENHANCED_NRXBUF };
	static const int maxntxbuf[] = { VIC_NBUF, VIC_ENHANCED_NTXBUF };

	enhanced = (sc->vic_flags & VIC_FLAGS_ENHANCED) ? 1 : 0;

	nrxbufs = vic_read_cmd(sc, VIC_CMD_NUM_Rx_BUF);
	if (nrxbufs > maxnrxbuf[enhanced] || nrxbufs == 0)
		nrxbufs = maxnrxbuf[enhanced];
	sc->vic_rxq[0].nbufs = nrxbufs;

	/* Initialize the remaining receive rings. */
	for (q = 1; q < VIC_NRXRINGS; q++) {
		if (sc->vic_flags & (VIC_FLAGS_JUMBO | VIC_FLAGS_LRO))
			sc->vic_rxq[q].nbufs = nrxbufs;
		else
			sc->vic_rxq[q].nbufs = 1;
	}

	sc->vic_tx_nbufs = vic_read_cmd(sc, VIC_CMD_NUM_Tx_BUF);
	if (sc->vic_tx_nbufs > maxntxbuf[enhanced] || sc->vic_tx_nbufs == 0)
		sc->vic_tx_nbufs = maxntxbuf[enhanced];
}

static void
vic_link_state(struct vic_softc *sc)
{
	struct ifnet *ifp;
	uint32_t status;
	int link_state;

	ifp = sc->vic_ifp;
	link_state = LINK_STATE_DOWN;
	status = vic_read(sc, VIC_STATUS);

	if (status & VIC_STATUS_CONNECTED)
		link_state = LINK_STATE_UP;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp, link_state);
	}
}

static void
vic_set_rxfilter(struct vic_softc *sc)
{
	struct ifnet *ifp;
	struct ifmultiaddr *ifma;
	uint32_t crc;
	uint16_t *mcastfil;
	u_int flags;

	ifp = sc->vic_ifp;
	mcastfil = (uint16_t *) sc->vic_data->vd_mcastfil;

	/* Always accept broadcast frames. */
	flags = VIC_CMD_IFF_BROADCAST;

	if (ifp->if_flags & IFF_PROMISC)
		flags |= VIC_CMD_IFF_PROMISC;

	if (ifp->if_flags & IFF_ALLMULTI) {
		flags |= VIC_CMD_IFF_MULTICAST;
		memset(&sc->vic_data->vd_mcastfil, 0xFF,
		    sizeof(sc->vic_data->vd_mcastfil));
	} else {
		bzero(&sc->vic_data->vd_mcastfil,
		    sizeof(sc->vic_data->vd_mcastfil));

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;

			flags |= VIC_CMD_IFF_MULTICAST;
			crc = ether_crc32_le(
			    LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
			    ETHER_ADDR_LEN);
			crc >>= 26;
			mcastfil[crc >> 4] |= htole16(1 << (crc & 0xF));
		}
		if_maddr_runlock(ifp);
	}

	vic_write(sc, VIC_CMD, VIC_CMD_MCASTFIL);
	sc->vic_data->vd_iff = flags;
	vic_write(sc, VIC_CMD, VIC_CMD_IFF);
}

static void
vic_get_lladdr(struct vic_softc *sc)
{
	uint32_t r;

	r = (sc->vic_cap & VIC_CMD_HWCAP_VPROM) ? VIC_VPROM : VIC_LLADDR;
	r += sc->vic_ioadj;

	bus_space_barrier(sc->vic_iot, sc->vic_ioh, r, ETHER_ADDR_LEN,
	    BUS_SPACE_BARRIER_READ);
	bus_space_read_region_1(sc->vic_iot, sc->vic_ioh, r, sc->vic_lladdr,
	    ETHER_ADDR_LEN);

	/* Update the MAC address register. */
	if (sc->vic_cap & VIC_CMD_HWCAP_VPROM)
		vic_set_lladdr(sc);
}

static void
vic_set_lladdr(struct vic_softc *sc)
{
	uint32_t r;

	r = VIC_LLADDR + sc->vic_ioadj;

	bus_space_write_region_1(sc->vic_iot, sc->vic_ioh, r,
	    sc->vic_lladdr, ETHER_ADDR_LEN);
	bus_space_barrier(sc->vic_iot, sc->vic_ioh, r,
	    ETHER_ADDR_LEN, BUS_SPACE_BARRIER_WRITE);
}

static int
vic_media_change(struct ifnet *ifp)
{

	/* Ignore. */
	return (0);
}

static void
vic_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct vic_softc *sc;

	sc = ifp->if_softc;

	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	vic_link_state(sc);

	if ((ifp->if_link_state & LINK_STATE_UP) && (ifp->if_flags & IFF_UP))
		imr->ifm_status |= IFM_ACTIVE;
}

#define VIC_SYSCTL_STAT_ADD32(c, h, n, p, d) \
    SYSCTL_ADD_UINT(c, h, OID_AUTO, n, CTLFLAG_RD, p, 0, d)

static void
vic_sysctl_node(struct vic_softc *sc)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *child;
	struct sysctl_oid *tree;
	struct vic_stats *stats;

	dev = sc->vic_dev;
	ctx = device_get_sysctl_ctx(dev);
	child = SYSCTL_CHILDREN(device_get_sysctl_tree(dev));
	stats = &sc->vic_data->vd_stats;

	/*
	 * XXX BMV We should free the 'stats' tree before vic_free_dma()
	 * since we free the vic-data memory then.
	 */
	tree = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "stats", CTLFLAG_RD,
	    NULL, "VIC statistics");
	child = SYSCTL_CHILDREN(tree);

	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_count",
	    &stats->vs_tx_count, "Tx count");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_packets",
	    &stats->vs_tx_packets, "Tx packets");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_0copy",
	    &stats->vs_tx_0copy, "Tx zero copy");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_copy",
	    &stats->vs_tx_copy, "Tx copy");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_maxpending",
	    &stats->vs_tx_maxpending, "Tx max pending");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_stopped",
	    &stats->vs_tx_stopped, "Tx stopped");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "tx_overrun",
	    &stats->vs_tx_overrun, "Tx overruns");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "intr",
	    &stats->vs_intr, "Interrupts");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "rx_packets",
	    &stats->vs_rx_packets, "Rx packets");
	VIC_SYSCTL_STAT_ADD32(ctx, child, "rx_underrun",
	    &stats->vs_rx_underrun, "Rx underrun");
}

#undef VIC_SYSCTL_STAT_ADD32

static void
vic_tick(void *xsc)
{
	struct vic_softc *sc;

	sc = xsc;

	vic_link_state(sc);
	vic_watchdog(sc);

	callout_reset(&sc->vic_tick, hz, vic_tick, sc);
}

static int
vic_pcnet_masquerade(device_t dev)
{
	struct resource *res;
	int pcnet, rid;

	pcnet = 0;
	rid = PCIR_BAR(VIC_PCI_BAR);

	res = bus_alloc_resource_any(dev, SYS_RES_IOPORT, &rid, RF_ACTIVE);
	if (res != NULL) {
		if (rman_get_size(res) >= VIC_LANCE_MINLEN)
			pcnet = 1;
		bus_release_resource(dev, SYS_RES_IOPORT, rid, res);
	}

	return (pcnet ? 0 : ENXIO);
}

static void
vic_pcnet_restore(struct vic_softc *sc)
{
	uint32_t morph;

	sc->vic_flags &= ~VIC_FLAGS_MORPHED_PCNET;
	sc->vic_ioadj = 0;

	morph = vic_read(sc, VIC_LANCE_SIZE);
	morph &= ~VIC_MORPH_MASK;
	morph |= VIC_MORPH_LANCE;
	vic_write(sc, VIC_LANCE_SIZE, morph);
}

static int
vic_pcnet_transform(struct vic_softc *sc)
{
	device_t dev;
	uint32_t morph;

	dev = sc->vic_dev;

	if (rman_get_size(sc->vic_res) < VIC_LANCE_MINLEN)
		return (ENOSPC);

	morph = vic_read(sc, VIC_LANCE_SIZE);
	if ((morph & VIC_MORPH_MASK) == VIC_MORPH_VMXNET)
		goto morphed;

	if ((morph & VIC_MORPH_MASK) != VIC_MORPH_LANCE) {
		device_printf(dev, "unsupported morph value %#X\n", morph);
		return (ENOTSUP);
	}

	morph &= ~VIC_MORPH_MASK;
	morph |= VIC_MORPH_VMXNET;
	vic_write(sc, VIC_LANCE_SIZE, morph);

	/* Check that the change stuck. */
	morph = vic_read(sc, VIC_LANCE_SIZE);
	if ((morph & VIC_MORPH_MASK) != VIC_MORPH_VMXNET) {
		device_printf(dev, "unable to morph PCNet to VMXNET\n");
		return (ENXIO);
	}

morphed:
	sc->vic_flags |= VIC_FLAGS_MORPHED_PCNET;
	sc->vic_ioadj = VIC_LANCE_SIZE + VIC_MORPH_SIZE;
	if (bootverbose)
		device_printf(dev, "transformed PCNet into VMXNET\n");

	return (0);
}

static inline void
vic_barrier(struct vic_softc *sc, int flags)
{

#ifdef notyet
	/*
	 * The tag, handle, offset, and length do not matter for
	 * x86. This is more heavy than we really require.
	 */
	bus_space_barrier(sc->vic_iot, sc->vic_ioh, 0, 4, flags);
#else
	switch (flags) {
	case BUS_SPACE_BARRIER_READ:
		rmb();
		break;
	case BUS_SPACE_BARRIER_WRITE:
		wmb();
		break;
	case BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE:
		mb();
		break;
	default:
		panic("%s: bogus flags %#X", __func__, flags);
		/* NOT REACHED */
	}
#endif
}
