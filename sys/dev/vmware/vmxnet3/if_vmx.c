/*-
 * Copyright (c) 2013 Tsubai Masanari
 * Copyright (c) 2013 Bryan Venteicher <bryanv@FreeBSD.org>
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
 * $OpenBSD: src/sys/dev/pci/if_vmx.c,v 1.11 2013/06/22 00:28:10 uebayasi Exp $
 */

/* Driver for VMware vmxnet3 virtual ethernet devices. */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/eventhandler.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_var.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/if_vlan_var.h>

#include <net/bpf.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

#include "if_vmxreg.h"
#include "if_vmxvar.h"

#include "opt_inet.h"
#include "opt_inet6.h"

/* Always enable for now - useful for queue hangs. */
#define VMXNET3_DEBUG_SYSCTL

#ifdef VMXNET3_FAILPOINTS
#include <sys/fail.h>
static SYSCTL_NODE(DEBUG_FP, OID_AUTO, vmxnet3, CTLFLAG_RW, 0,
    "vmxnet3 fail points");
#define VMXNET3_FP	_debug_fail_point_vmxnet3
#endif

static int	vmxnet3_probe(device_t);
static int	vmxnet3_attach(device_t);
static int	vmxnet3_detach(device_t);
static int	vmxnet3_shutdown(device_t);

static int	vmxnet3_alloc_resources(struct vmxnet3_softc *);
static void	vmxnet3_free_resources(struct vmxnet3_softc *);
static int	vmxnet3_check_version(struct vmxnet3_softc *);
static void	vmxnet3_initial_config(struct vmxnet3_softc *);

static int	vmxnet3_alloc_msix_interrupts(struct vmxnet3_softc *);
static int	vmxnet3_alloc_msi_interrupts(struct vmxnet3_softc *);
static int	vmxnet3_alloc_legacy_interrupts(struct vmxnet3_softc *);
static int	vmxnet3_alloc_interrupt(struct vmxnet3_softc *, int, int,
		    struct vmxnet3_interrupt *);
static int	vmxnet3_alloc_intr_resources(struct vmxnet3_softc *);
static int	vmxnet3_setup_msix_interrupts(struct vmxnet3_softc *);
static int	vmxnet3_setup_legacy_interrupt(struct vmxnet3_softc *);
static int	vmxnet3_setup_interrupts(struct vmxnet3_softc *);
static int	vmxnet3_alloc_interrupts(struct vmxnet3_softc *);

static void	vmxnet3_free_interrupt(struct vmxnet3_softc *,
		    struct vmxnet3_interrupt *);
static void	vmxnet3_free_interrupts(struct vmxnet3_softc *);

static int	vmxnet3_init_rxq(struct vmxnet3_softc *, int);
static int	vmxnet3_init_txq(struct vmxnet3_softc *, int);
static int	vmxnet3_alloc_rxtx_queues(struct vmxnet3_softc *);
static void	vmxnet3_destroy_rxq(struct vmxnet3_rxqueue *);
static void	vmxnet3_destroy_txq(struct vmxnet3_txqueue *);
static void	vmxnet3_free_rxtx_queues(struct vmxnet3_softc *);

static int	vmxnet3_alloc_shared_data(struct vmxnet3_softc *);
static void	vmxnet3_free_shared_data(struct vmxnet3_softc *);
static int	vmxnet3_alloc_txq_data(struct vmxnet3_softc *);
static void	vmxnet3_free_txq_data(struct vmxnet3_softc *);
static int	vmxnet3_alloc_rxq_data(struct vmxnet3_softc *);
static void	vmxnet3_free_rxq_data(struct vmxnet3_softc *);
static int	vmxnet3_alloc_queue_data(struct vmxnet3_softc *);
static void	vmxnet3_free_queue_data(struct vmxnet3_softc *);
static int	vmxnet3_alloc_mcast_table(struct vmxnet3_softc *);
static void	vmxnet3_init_shared_data(struct vmxnet3_softc *);
static void	vmxnet3_reinit_interface(struct vmxnet3_softc *);
static void	vmxnet3_reinit_shared_data(struct vmxnet3_softc *);
static int	vmxnet3_alloc_data(struct vmxnet3_softc *);
static void	vmxnet3_free_data(struct vmxnet3_softc *);
static int	vmxnet3_setup_interface(struct vmxnet3_softc *);

static void	vmxnet3_evintr(struct vmxnet3_softc *);
static void	vmxnet3_txq_eof(struct vmxnet3_txqueue *);
static void	vmxnet3_rx_csum(struct vmxnet3_rxcompdesc *, struct mbuf *);
static int	vmxnet3_newbuf(struct vmxnet3_softc *, struct vmxnet3_rxring *);
static void	vmxnet3_rxq_eof_discard(struct vmxnet3_rxqueue *,
		    struct vmxnet3_rxring *, int);
static void	vmxnet3_rxq_eof(struct vmxnet3_rxqueue *);
static void	vmxnet3_legacy_intr(void *);
static void	vmxnet3_txq_intr(void *);
static void	vmxnet3_rxq_intr(void *);
static void	vmxnet3_event_intr(void *);

static void	vmxnet3_txstop(struct vmxnet3_softc *, struct vmxnet3_txqueue *);
static void	vmxnet3_rxstop(struct vmxnet3_softc *, struct vmxnet3_rxqueue *);
static void	vmxnet3_stop(struct vmxnet3_softc *);

static void	vmxnet3_txinit(struct vmxnet3_softc *, struct vmxnet3_txqueue *);
static int	vmxnet3_rxinit(struct vmxnet3_softc *, struct vmxnet3_rxqueue *);
static int	vmxnet3_reinit_queues(struct vmxnet3_softc *);
static int	vmxnet3_enable_device(struct vmxnet3_softc *);
static void	vmxnet3_reinit_rxfilters(struct vmxnet3_softc *);
static int	vmxnet3_reinit(struct vmxnet3_softc *);
static void	vmxnet3_init_locked(struct vmxnet3_softc *);
static void	vmxnet3_init(void *);

static int	vmxnet3_txq_offload_ctx(struct mbuf *, int *, int *, int *);
static int	vmxnet3_txq_load_mbuf(struct vmxnet3_txqueue *, struct mbuf **,
		    bus_dmamap_t, bus_dma_segment_t [], int *);
static void	vmxnet3_txq_unload_mbuf(struct vmxnet3_txqueue *, bus_dmamap_t);
static int	vmxnet3_txq_encap(struct vmxnet3_txqueue *, struct mbuf **);
static void	vmxnet3_start_locked(struct ifnet *);
static void	vmxnet3_start(struct ifnet *);

static void	vmxnet3_update_vlan_filter(struct vmxnet3_softc *, int,
		    uint16_t);
static void	vmxnet3_register_vlan(void *, struct ifnet *, uint16_t);
static void	vmxnet3_unregister_vlan(void *, struct ifnet *, uint16_t);
static void	vmxnet3_set_rxfilter(struct vmxnet3_softc *);
static int	vmxnet3_change_mtu(struct vmxnet3_softc *, int);
static int	vmxnet3_ioctl(struct ifnet *, u_long, caddr_t);

static int	vmxnet3_watchdog(struct vmxnet3_txqueue *);
static void	vmxnet3_tick(void *);
static void	vmxnet3_link_status(struct vmxnet3_softc *);
static void	vmxnet3_media_status(struct ifnet *, struct ifmediareq *);
static int	vmxnet3_media_change(struct ifnet *);
static void	vmxnet3_set_lladdr(struct vmxnet3_softc *);
static void	vmxnet3_get_lladdr(struct vmxnet3_softc *);

static void	vmxnet3_setup_txq_sysctl(struct vmxnet3_txqueue *,
		    struct sysctl_ctx_list *, struct sysctl_oid_list *);
static void	vmxnet3_setup_rxq_sysctl(struct vmxnet3_rxqueue *,
		    struct sysctl_ctx_list *, struct sysctl_oid_list *);
static void	vmxnet3_setup_queue_sysctl(struct vmxnet3_softc *,
		    struct sysctl_ctx_list *, struct sysctl_oid_list *);
static void	vmxnet3_setup_sysctl(struct vmxnet3_softc *);

static void	vmxnet3_write_bar0(struct vmxnet3_softc *, bus_size_t,
		    uint32_t);
static uint32_t	vmxnet3_read_bar1(struct vmxnet3_softc *, bus_size_t);
static void	vmxnet3_write_bar1(struct vmxnet3_softc *, bus_size_t,
		    uint32_t);
static void	vmxnet3_write_cmd(struct vmxnet3_softc *, uint32_t);
static uint32_t	vmxnet3_read_cmd(struct vmxnet3_softc *, uint32_t);

static void	vmxnet3_enable_intr(struct vmxnet3_softc *, int);
static void	vmxnet3_disable_intr(struct vmxnet3_softc *, int);
static void	vmxnet3_enable_all_intrs(struct vmxnet3_softc *);
static void	vmxnet3_disable_all_intrs(struct vmxnet3_softc *);

static int	vmxnet3_dma_malloc(struct vmxnet3_softc *, bus_size_t,
		    bus_size_t, struct vmxnet3_dma_alloc *);
static void	vmxnet3_dma_free(struct vmxnet3_softc *,
		    struct vmxnet3_dma_alloc *);
static int	vmxnet3_tunable_int(struct vmxnet3_softc *,
		    const char *, int);

typedef enum {
	VMXNET3_BARRIER_RD,
	VMXNET3_BARRIER_WR,
	VMXNET3_BARRIER_RDWR,
} vmxnet3_barrier_t;

static void	vmxnet3_barrier(struct vmxnet3_softc *, vmxnet3_barrier_t);

/* Tunables. */
static int vmxnet3_default_txndesc = VMXNET3_DEF_TX_NDESC;
TUNABLE_INT("hw.vmx.txndesc", &vmxnet3_default_txndesc);
static int vmxnet3_default_rxndesc = VMXNET3_DEF_RX_NDESC;
TUNABLE_INT("hw.vmx.rxndesc", &vmxnet3_default_rxndesc);

static device_method_t vmxnet3_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,		vmxnet3_probe),
	DEVMETHOD(device_attach,	vmxnet3_attach),
	DEVMETHOD(device_detach,	vmxnet3_detach),
	DEVMETHOD(device_shutdown,	vmxnet3_shutdown),

	DEVMETHOD_END
};

static driver_t vmxnet3_driver = {
	"vmx", vmxnet3_methods, sizeof(struct vmxnet3_softc)
};

static devclass_t vmxnet3_devclass;
DRIVER_MODULE(vmx, pci, vmxnet3_driver, vmxnet3_devclass, 0, 0);

MODULE_DEPEND(vmx, pci, 1, 1, 1);
MODULE_DEPEND(vmx, ether, 1, 1, 1);

#define VMXNET3_VMWARE_VENDOR_ID	0x15AD
#define VMXNET3_VMWARE_DEVICE_ID	0x07B0

static int
vmxnet3_probe(device_t dev)
{

	if (pci_get_vendor(dev) == VMXNET3_VMWARE_VENDOR_ID &&
	    pci_get_device(dev) == VMXNET3_VMWARE_DEVICE_ID) {
		device_set_desc(dev, "VMware VMXNET3 Ethernet Adapter");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
vmxnet3_attach(device_t dev)
{
	struct vmxnet3_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->vmx_dev = dev;

	pci_enable_busmaster(dev);

	VMXNET3_CORE_LOCK_INIT(sc, device_get_nameunit(dev));
	callout_init_mtx(&sc->vmx_tick, &sc->vmx_mtx, 0);

	vmxnet3_initial_config(sc);

	error = vmxnet3_alloc_resources(sc);
	if (error)
		goto fail;

	error = vmxnet3_check_version(sc);
	if (error)
		goto fail;

	error = vmxnet3_alloc_rxtx_queues(sc);
	if (error)
		goto fail;

	error = vmxnet3_alloc_interrupts(sc);
	if (error)
		goto fail;

	error = vmxnet3_alloc_data(sc);
	if (error)
		goto fail;

	error = vmxnet3_setup_interface(sc);
	if (error)
		goto fail;

	error = vmxnet3_setup_interrupts(sc);
	if (error) {
		ether_ifdetach(sc->vmx_ifp);
		device_printf(dev, "could not set up interrupt\n");
		goto fail;
	}

	vmxnet3_setup_sysctl(sc);
	vmxnet3_link_status(sc);

fail:
	if (error)
		vmxnet3_detach(dev);

	return (error);
}

static int
vmxnet3_detach(device_t dev)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;

	sc = device_get_softc(dev);
	ifp = sc->vmx_ifp;

	if (device_is_attached(dev)) {
		ether_ifdetach(ifp);
		VMXNET3_CORE_LOCK(sc);
		vmxnet3_stop(sc);
		VMXNET3_CORE_UNLOCK(sc);
		callout_drain(&sc->vmx_tick);
	}

	if (sc->vmx_vlan_attach != NULL) {
		EVENTHANDLER_DEREGISTER(vlan_config, sc->vmx_vlan_attach);
		sc->vmx_vlan_attach = NULL;
	}
	if (sc->vmx_vlan_detach != NULL) {
		EVENTHANDLER_DEREGISTER(vlan_config, sc->vmx_vlan_detach);
		sc->vmx_vlan_detach = NULL;
	}

	vmxnet3_free_interrupts(sc);

	if (ifp != NULL) {
		if_free(ifp);
		sc->vmx_ifp = NULL;
	}

	ifmedia_removeall(&sc->vmx_media);

	vmxnet3_free_data(sc);
	vmxnet3_free_resources(sc);
	vmxnet3_free_rxtx_queues(sc);

	VMXNET3_CORE_LOCK_DESTROY(sc);

	return (0);
}

static int
vmxnet3_shutdown(device_t dev)
{

	return (0);
}

static int
vmxnet3_alloc_resources(struct vmxnet3_softc *sc)
{
	device_t dev;
	int rid;

	dev = sc->vmx_dev;

	rid = PCIR_BAR(0);
	sc->vmx_res0 = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->vmx_res0 == NULL) {
		device_printf(dev,
		    "could not map BAR0 memory\n");
		return (ENXIO);
	}

	sc->vmx_iot0 = rman_get_bustag(sc->vmx_res0);
	sc->vmx_ioh0 = rman_get_bushandle(sc->vmx_res0);

	rid = PCIR_BAR(1);
	sc->vmx_res1 = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->vmx_res1 == NULL) {
		device_printf(dev,
		    "could not map BAR1 memory\n");
		return (ENXIO);
	}

	sc->vmx_iot1 = rman_get_bustag(sc->vmx_res1);
	sc->vmx_ioh1 = rman_get_bushandle(sc->vmx_res1);

	if (pci_find_cap(dev, PCIY_MSIX, NULL) == 0) {
		rid = PCIR_BAR(2);
		sc->vmx_msix_res = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
	}

	if (sc->vmx_msix_res == NULL)
		sc->vmx_flags |= VMXNET3_FLAG_NO_MSIX;

	return (0);
}

static void
vmxnet3_free_resources(struct vmxnet3_softc *sc)
{
	device_t dev;
	int rid;

	dev = sc->vmx_dev;

	if (sc->vmx_res0 != NULL) {
		rid = PCIR_BAR(0);
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->vmx_res0);
		sc->vmx_res0 = NULL;
	}

	if (sc->vmx_res1 != NULL) {
		rid = PCIR_BAR(1);
		bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->vmx_res1);
		sc->vmx_res1 = NULL;
	}

	if (sc->vmx_msix_res != NULL) {
		rid = PCIR_BAR(2);
		bus_release_resource(dev, SYS_RES_MEMORY, rid,
		    sc->vmx_msix_res);
		sc->vmx_msix_res = NULL;
	}
}

static int
vmxnet3_check_version(struct vmxnet3_softc *sc)
{
	device_t dev;
	uint32_t version;

	dev = sc->vmx_dev;

	version = vmxnet3_read_bar1(sc, VMXNET3_BAR1_VRRS);
	if ((version & 0x01) == 0) {
		device_printf(dev, "unsupported hardware version %#x\n",
		    version);
		return (ENOTSUP);
	}
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_VRRS, 1);

	version = vmxnet3_read_bar1(sc, VMXNET3_BAR1_UVRS);
	if ((version & 0x01) == 0) {
		device_printf(dev, "unsupported UPT version %#x\n", version);
		return (ENOTSUP);
	}
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_UVRS, 1);

	return (0);
}

static void
vmxnet3_initial_config(struct vmxnet3_softc *sc)
{
	int ndesc;

	/*
	 * BMV Much of the work is already done, but this driver does
	 * not support multiqueue yet.
	 */
	sc->vmx_ntxqueues = VMXNET3_TX_QUEUES;
	sc->vmx_nrxqueues = VMXNET3_RX_QUEUES;

	ndesc = vmxnet3_tunable_int(sc, "txd", vmxnet3_default_txndesc);
	if (ndesc > VMXNET3_MAX_TX_NDESC || ndesc < VMXNET3_MIN_TX_NDESC)
		ndesc = VMXNET3_DEF_TX_NDESC;
	if (ndesc & VMXNET3_MASK_TX_NDESC)
		ndesc &= ~VMXNET3_MASK_TX_NDESC;
	sc->vmx_ntxdescs = ndesc;

	ndesc = vmxnet3_tunable_int(sc, "rxd", vmxnet3_default_rxndesc);
	if (ndesc > VMXNET3_MAX_RX_NDESC || ndesc < VMXNET3_MIN_RX_NDESC)
		ndesc = VMXNET3_DEF_RX_NDESC;
	if (ndesc & VMXNET3_MASK_RX_NDESC)
		ndesc &= ~VMXNET3_MASK_RX_NDESC;
	sc->vmx_nrxdescs = ndesc;
	sc->vmx_max_rxsegs = VMXNET3_MAX_RX_SEGS;
}

static int
vmxnet3_alloc_msix_interrupts(struct vmxnet3_softc *sc)
{
	device_t dev;
	int nmsix, cnt, required;

	dev = sc->vmx_dev;

	if (sc->vmx_flags & VMXNET3_FLAG_NO_MSIX)
		return (1);

	/* Allocate an additional vector for the events interrupt. */
	required = sc->vmx_nrxqueues + sc->vmx_ntxqueues + 1;

	nmsix = pci_msix_count(dev);
	if (nmsix < required)
		return (1);

	cnt = required;
	if (pci_alloc_msix(dev, &cnt) == 0 && cnt >= required) {
		sc->vmx_nintrs = required;
		return (0);
	} else
		pci_release_msi(dev);

	return (1);
}

static int
vmxnet3_alloc_msi_interrupts(struct vmxnet3_softc *sc)
{
	device_t dev;
	int nmsi, cnt, required;

	dev = sc->vmx_dev;
	required = 1;

	nmsi = pci_msi_count(dev);
	if (nmsi < required)
		return (1);

	cnt = required;
	if (pci_alloc_msi(dev, &cnt) == 0 && cnt >= required) {
		sc->vmx_nintrs = 1;
		return (0);
	} else
		pci_release_msi(dev);

	return (1);
}

static int
vmxnet3_alloc_legacy_interrupts(struct vmxnet3_softc *sc)
{

	sc->vmx_nintrs = 1;
	return (0);
}

static int
vmxnet3_alloc_interrupt(struct vmxnet3_softc *sc, int rid, int flags,
    struct vmxnet3_interrupt *intr)
{
	struct resource *irq;

	irq = bus_alloc_resource_any(sc->vmx_dev, SYS_RES_IRQ, &rid, flags);
	if (irq == NULL)
		return (ENXIO);

	intr->vmxi_irq = irq;
	intr->vmxi_rid = rid;

	return (0);
}

static int
vmxnet3_alloc_intr_resources(struct vmxnet3_softc *sc)
{
	int i, rid, flags, error;

	rid = 0;
	flags = RF_ACTIVE;

	if (sc->vmx_intr_type == VMXNET3_IT_LEGACY)
		flags |= RF_SHAREABLE;
	else
		rid = 1;

	for (i = 0; i < sc->vmx_nintrs; i++, rid++) {
		error = vmxnet3_alloc_interrupt(sc, rid, flags,
		    &sc->vmx_intrs[i]);
		if (error)
			return (error);
	}

	return (0);
}

/*
 * NOTE: We only support the simple case of each Rx and Tx queue on its
 * own MSIX vector. This is good enough until we support mulitqueue.
 */
static int
vmxnet3_setup_msix_interrupts(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_interrupt *intr;
	enum intr_type type;
	int i, error;

	dev = sc->vmx_dev;
	intr = &sc->vmx_intrs[0];
	type = INTR_TYPE_NET | INTR_MPSAFE;

	for (i = 0; i < sc->vmx_ntxqueues; i++, intr++) {
		txq = &sc->vmx_txq[i];
		error = bus_setup_intr(dev, intr->vmxi_irq, type, NULL,
		     vmxnet3_txq_intr, txq, &intr->vmxi_handler);
		if (error)
			return (error);
		txq->vxtxq_intr_idx = intr->vmxi_rid - 1;
	}

	for (i = 0; i < sc->vmx_nrxqueues; i++, intr++) {
		rxq = &sc->vmx_rxq[i];
		error = bus_setup_intr(dev, intr->vmxi_irq, type, NULL,
		    vmxnet3_rxq_intr, rxq, &intr->vmxi_handler);
		if (error)
			return (error);
		rxq->vxrxq_intr_idx = intr->vmxi_rid - 1;
	}

	error = bus_setup_intr(dev, intr->vmxi_irq, type, NULL,
	    vmxnet3_event_intr, sc, &intr->vmxi_handler);
	if (error)
		return (error);
	sc->vmx_event_intr_idx = intr->vmxi_rid - 1;

	return (0);
}

static int
vmxnet3_setup_legacy_interrupt(struct vmxnet3_softc *sc)
{
	struct vmxnet3_interrupt *intr;
	int i, error;

	intr = &sc->vmx_intrs[0];
	error = bus_setup_intr(sc->vmx_dev, intr->vmxi_irq,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, vmxnet3_legacy_intr, sc,
	    &intr->vmxi_handler);

	for (i = 0; i < sc->vmx_ntxqueues; i++)
		sc->vmx_txq[i].vxtxq_intr_idx = 0;
	for (i = 0; i < sc->vmx_nrxqueues; i++)
		sc->vmx_rxq[i].vxrxq_intr_idx = 0;
	sc->vmx_event_intr_idx = 0;

	return (error);
}

/*
 * XXX BMV Should probably reorganize the attach and just do
 * this in vmxnet3_init_shared_data().
 */
static void
vmxnet3_set_interrupt_idx(struct vmxnet3_softc *sc)
{
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txq_shared *txs;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_rxq_shared *rxs;
	int i;

	sc->vmx_ds->evintr = sc->vmx_event_intr_idx;

	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		txq = &sc->vmx_txq[i];
		txs = txq->vxtxq_ts;
		txs->intr_idx = txq->vxtxq_intr_idx;
	}

	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		rxq = &sc->vmx_rxq[i];
		rxs = rxq->vxrxq_rs;
		rxs->intr_idx = rxq->vxrxq_intr_idx;
	}
}

static int
vmxnet3_setup_interrupts(struct vmxnet3_softc *sc)
{
	int error;

	error = vmxnet3_alloc_intr_resources(sc);
	if (error)
		return (error);

	switch (sc->vmx_intr_type) {
	case VMXNET3_IT_MSIX:
		error = vmxnet3_setup_msix_interrupts(sc);
		break;
	case VMXNET3_IT_MSI:
	case VMXNET3_IT_LEGACY:
		error = vmxnet3_setup_legacy_interrupt(sc);
		break;
	default:
		panic("%s: invalid interrupt type %d", __func__,
		    sc->vmx_intr_type);
	}

	if (error == 0)
		vmxnet3_set_interrupt_idx(sc);

	return (error);
}

static int
vmxnet3_alloc_interrupts(struct vmxnet3_softc *sc)
{
	device_t dev;
	uint32_t config;
	int error;

	dev = sc->vmx_dev;
	config = vmxnet3_read_cmd(sc, VMXNET3_CMD_GET_INTRCFG);

	sc->vmx_intr_type = config & 0x03;
	sc->vmx_intr_mask_mode = (config >> 2) & 0x03;

	switch (sc->vmx_intr_type) {
	case VMXNET3_IT_AUTO:
		sc->vmx_intr_type = VMXNET3_IT_MSIX;
		/* FALLTHROUGH */
	case VMXNET3_IT_MSIX:
		error = vmxnet3_alloc_msix_interrupts(sc);
		if (error == 0)
			break;
		sc->vmx_intr_type = VMXNET3_IT_MSI;
		/* FALLTHROUGH */
	case VMXNET3_IT_MSI:
		error = vmxnet3_alloc_msi_interrupts(sc);
		if (error == 0)
			break;
		sc->vmx_intr_type = VMXNET3_IT_LEGACY;
		/* FALLTHROUGH */
	case VMXNET3_IT_LEGACY:
		error = vmxnet3_alloc_legacy_interrupts(sc);
		if (error == 0)
			break;
		/* FALLTHROUGH */
	default:
		sc->vmx_intr_type = -1;
		device_printf(dev, "cannot allocate any interrupt resources\n");
		return (ENXIO);
	}

	return (error);
}

static void
vmxnet3_free_interrupt(struct vmxnet3_softc *sc,
    struct vmxnet3_interrupt *intr)
{
	device_t dev;

	dev = sc->vmx_dev;

	if (intr->vmxi_handler != NULL) {
		bus_teardown_intr(dev, intr->vmxi_irq, intr->vmxi_handler);
		intr->vmxi_handler = NULL;
	}

	if (intr->vmxi_irq != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, intr->vmxi_rid,
		    intr->vmxi_irq);
		intr->vmxi_irq = NULL;
		intr->vmxi_rid = -1;
	}
}

static void
vmxnet3_free_interrupts(struct vmxnet3_softc *sc)
{
	int i;

	for (i = 0; i < sc->vmx_nintrs; i++)
		vmxnet3_free_interrupt(sc, &sc->vmx_intrs[i]);

	if (sc->vmx_intr_type == VMXNET3_IT_MSI ||
	    sc->vmx_intr_type == VMXNET3_IT_MSIX)
		pci_release_msi(sc->vmx_dev);
}

static int
vmxnet3_init_rxq(struct vmxnet3_softc *sc, int q)
{
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_rxring *rxr;
	int i;

	rxq = &sc->vmx_rxq[q];

	snprintf(rxq->vxrxq_name, sizeof(rxq->vxrxq_name), "%s-rx%d",
	    device_get_nameunit(sc->vmx_dev), q);
	mtx_init(&rxq->vxrxq_mtx, rxq->vxrxq_name, NULL, MTX_DEF);

	rxq->vxrxq_sc = sc;
	rxq->vxrxq_id = q;

	for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
		rxr = &rxq->vxrxq_cmd_ring[i];
		rxr->vxrxr_rid = i;
		rxr->vxrxr_ndesc = sc->vmx_nrxdescs;
		rxr->vxrxr_rxbuf = malloc(rxr->vxrxr_ndesc *
		    sizeof(struct vmxnet3_rxbuf), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (rxr->vxrxr_rxbuf == NULL)
			return (ENOMEM);

		rxq->vxrxq_comp_ring.vxcr_ndesc += sc->vmx_nrxdescs;
	}

	return (0);
}

static int
vmxnet3_init_txq(struct vmxnet3_softc *sc, int q)
{
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txring *txr;

	txq = &sc->vmx_txq[q];
	txr = &txq->vxtxq_cmd_ring;

	snprintf(txq->vxtxq_name, sizeof(txq->vxtxq_name), "%s-tx%d",
	    device_get_nameunit(sc->vmx_dev), q);
	mtx_init(&txq->vxtxq_mtx, txq->vxtxq_name, NULL, MTX_DEF);

	txq->vxtxq_sc = sc;
	txq->vxtxq_id = q;

	txr->vxtxr_ndesc = sc->vmx_ntxdescs;
	txr->vxtxr_txbuf = malloc(txr->vxtxr_ndesc *
	    sizeof(struct vmxnet3_txbuf), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (txr->vxtxr_txbuf == NULL)
		return (ENOMEM);

	txq->vxtxq_comp_ring.vxcr_ndesc = sc->vmx_ntxdescs;

	return (0);
}

static int
vmxnet3_alloc_rxtx_queues(struct vmxnet3_softc *sc)
{
	int i, error;

	sc->vmx_rxq = malloc(sizeof(struct vmxnet3_rxqueue) *
	    sc->vmx_nrxqueues, M_DEVBUF, M_NOWAIT | M_ZERO);
	sc->vmx_txq = malloc(sizeof(struct vmxnet3_txqueue) *
	    sc->vmx_ntxqueues, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sc->vmx_rxq == NULL || sc->vmx_txq == NULL)
		return (ENOMEM);

	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		error = vmxnet3_init_rxq(sc, i);
		if (error)
			return (error);
	}

	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		error = vmxnet3_init_txq(sc, i);
		if (error)
			return (error);
	}

	return (0);
}

static void
vmxnet3_destroy_rxq(struct vmxnet3_rxqueue *rxq)
{
	struct vmxnet3_rxring *rxr;
	int i;

	rxq->vxrxq_sc = NULL;
	rxq->vxrxq_id = -1;

	for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
		rxr = &rxq->vxrxq_cmd_ring[i];

		if (rxr->vxrxr_rxbuf != NULL) {
			free(rxr->vxrxr_rxbuf, M_DEVBUF);
			rxr->vxrxr_rxbuf = NULL;
		}
	}

	if (mtx_initialized(&rxq->vxrxq_mtx) != 0)
		mtx_destroy(&rxq->vxrxq_mtx);
}

static void
vmxnet3_destroy_txq(struct vmxnet3_txqueue *txq)
{
	struct vmxnet3_txring *txr;

	txr = &txq->vxtxq_cmd_ring;

	txq->vxtxq_sc = NULL;
	txq->vxtxq_id = -1;

	if (txr->vxtxr_txbuf != NULL) {
		free(txr->vxtxr_txbuf, M_DEVBUF);
		txr->vxtxr_txbuf = NULL;
	}

	if (mtx_initialized(&txq->vxtxq_mtx) != 0)
		mtx_destroy(&txq->vxtxq_mtx);
}

static void
vmxnet3_free_rxtx_queues(struct vmxnet3_softc *sc)
{
	int i;

	if (sc->vmx_rxq != NULL) {
		for (i = 0; i < sc->vmx_nrxqueues; i++)
			vmxnet3_destroy_rxq(&sc->vmx_rxq[i]);
		free(sc->vmx_rxq, M_DEVBUF);
		sc->vmx_rxq = NULL;
	}

	if (sc->vmx_txq != NULL) {
		for (i = 0; i < sc->vmx_ntxqueues; i++)
			vmxnet3_destroy_txq(&sc->vmx_txq[i]);
		free(sc->vmx_txq, M_DEVBUF);
		sc->vmx_txq = NULL;
	}
}

static int
vmxnet3_alloc_shared_data(struct vmxnet3_softc *sc)
{
	device_t dev;
	uint8_t *kva;
	size_t size;
	int i, error;

	dev = sc->vmx_dev;

	size = sizeof(struct vmxnet3_driver_shared);
	error = vmxnet3_dma_malloc(sc, size, 1, &sc->vmx_ds_dma);
	if (error) {
		device_printf(dev, "cannot alloc shared memory\n");
		return (error);
	}
	sc->vmx_ds = (struct vmxnet3_driver_shared *) sc->vmx_ds_dma.dma_vaddr;

	size = sc->vmx_ntxqueues * sizeof(struct vmxnet3_txq_shared) +
	    sc->vmx_nrxqueues * sizeof(struct vmxnet3_rxq_shared);
	error = vmxnet3_dma_malloc(sc, size, 128, &sc->vmx_qs_dma);
	if (error) {
		device_printf(dev, "cannot alloc queue shared memory\n");
		return (error);
	}
	sc->vmx_qs = (void *) sc->vmx_qs_dma.dma_vaddr;
	kva = sc->vmx_qs;

	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		sc->vmx_txq[i].vxtxq_ts = (struct vmxnet3_txq_shared *) kva;
		kva += sizeof(struct vmxnet3_txq_shared);
	}
	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		sc->vmx_rxq[i].vxrxq_rs = (struct vmxnet3_rxq_shared *) kva;
		kva += sizeof(struct vmxnet3_rxq_shared);
	}

	return (0);
}

static void
vmxnet3_free_shared_data(struct vmxnet3_softc *sc)
{

	if (sc->vmx_qs != NULL) {
		vmxnet3_dma_free(sc, &sc->vmx_qs_dma);
		sc->vmx_qs = NULL;
	}

	if (sc->vmx_ds != NULL) {
		vmxnet3_dma_free(sc, &sc->vmx_ds_dma);
		sc->vmx_ds = NULL;
	}
}

static int
vmxnet3_alloc_txq_data(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txring *txr;
	struct vmxnet3_comp_ring *txc;
	size_t descsz, compsz;
	int i, q, error;

	dev = sc->vmx_dev;

	for (q = 0; q < sc->vmx_ntxqueues; q++) {
		txq = &sc->vmx_txq[q];
		txr = &txq->vxtxq_cmd_ring;
		txc = &txq->vxtxq_comp_ring;

		descsz = txr->vxtxr_ndesc * sizeof(struct vmxnet3_txdesc);
		compsz = txr->vxtxr_ndesc * sizeof(struct vmxnet3_txcompdesc);

		error = bus_dma_tag_create(bus_get_dma_tag(dev),
		    1, 0,			/* alignment, boundary */
		    BUS_SPACE_MAXADDR,		/* lowaddr */
		    BUS_SPACE_MAXADDR,		/* highaddr */
		    NULL, NULL,			/* filter, filterarg */
		    VMXNET3_TSO_MAXSIZE,	/* maxsize */
		    VMXNET3_TX_MAXSEGS,		/* nsegments */
		    VMXNET3_TX_MAXSEGSIZE,	/* maxsegsize */
		    0,				/* flags */
		    NULL, NULL,			/* lockfunc, lockarg */
		    &txr->vxtxr_txtag);
		if (error) {
			device_printf(dev,
			    "unable to create Tx buffer tag for queue %d\n", q);
			return (error);
		}

		error = vmxnet3_dma_malloc(sc, descsz, 512, &txr->vxtxr_dma);
		if (error) {
			device_printf(dev, "cannot alloc Tx descriptors for "
			    "queue %d error %d\n", q, error);
			return (error);
		}
		txr->vxtxr_txd =
		    (struct vmxnet3_txdesc *) txr->vxtxr_dma.dma_vaddr;

		error = vmxnet3_dma_malloc(sc, compsz, 512, &txc->vxcr_dma);
		if (error) {
			device_printf(dev, "cannot alloc Tx comp descriptors "
			   "for queue %d error %d\n", q, error);
			return (error);
		}
		txc->vxcr_u.txcd =
		    (struct vmxnet3_txcompdesc *) txc->vxcr_dma.dma_vaddr;

		for (i = 0; i < txr->vxtxr_ndesc; i++) {
			error = bus_dmamap_create(txr->vxtxr_txtag, 0,
			    &txr->vxtxr_txbuf[i].vtxb_dmamap);
			if (error) {
				device_printf(dev, "unable to create Tx buf "
				    "dmamap for queue %d idx %d\n", q, i);
				return (error);
			}
		}
	}

	return (0);
}

static void
vmxnet3_free_txq_data(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txring *txr;
	struct vmxnet3_comp_ring *txc;
	struct vmxnet3_txbuf *txb;
	int i, q;

	dev = sc->vmx_dev;

	for (q = 0; q < sc->vmx_ntxqueues; q++) {
		txq = &sc->vmx_txq[q];
		txr = &txq->vxtxq_cmd_ring;
		txc = &txq->vxtxq_comp_ring;

		for (i = 0; i < txr->vxtxr_ndesc; i++) {
			txb = &txr->vxtxr_txbuf[i];
			if (txb->vtxb_dmamap != NULL) {
				bus_dmamap_destroy(txr->vxtxr_txtag,
				    txb->vtxb_dmamap);
				txb->vtxb_dmamap = NULL;
			}
		}

		if (txc->vxcr_u.txcd != NULL) {
			vmxnet3_dma_free(sc, &txc->vxcr_dma);
			txc->vxcr_u.txcd = NULL;
		}

		if (txr->vxtxr_txd != NULL) {
			vmxnet3_dma_free(sc, &txr->vxtxr_dma);
			txr->vxtxr_txd = NULL;
		}

		if (txr->vxtxr_txtag != NULL) {
			bus_dma_tag_destroy(txr->vxtxr_txtag);
			txr->vxtxr_txtag = NULL;
		}
	}
}

static int
vmxnet3_alloc_rxq_data(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;
	int descsz, compsz;
	int i, j, q, error;

	dev = sc->vmx_dev;

	for (q = 0; q < sc->vmx_nrxqueues; q++) {
		rxq = &sc->vmx_rxq[q];
		rxc = &rxq->vxrxq_comp_ring;
		compsz = 0;

		for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
			rxr = &rxq->vxrxq_cmd_ring[i];

			descsz = rxr->vxrxr_ndesc *
			    sizeof(struct vmxnet3_rxdesc);
			compsz += rxr->vxrxr_ndesc *
			    sizeof(struct vmxnet3_rxcompdesc);

			error = bus_dma_tag_create(bus_get_dma_tag(dev),
			    1, 0,		/* alignment, boundary */
			    BUS_SPACE_MAXADDR,	/* lowaddr */
			    BUS_SPACE_MAXADDR,	/* highaddr */
			    NULL, NULL,		/* filter, filterarg */
			    MJUMPAGESIZE,	/* maxsize */
			    1,			/* nsegments */
			    MJUMPAGESIZE,	/* maxsegsize */
			    0,			/* flags */
			    NULL, NULL,		/* lockfunc, lockarg */
			    &rxr->vxrxr_rxtag);
			if (error) {
				device_printf(dev,
				    "unable to create Rx buffer tag for "
				    "queue %d\n", q);
				return (error);
			}

			error = vmxnet3_dma_malloc(sc, descsz, 512,
			    &rxr->vxrxr_dma);
			if (error) {
				device_printf(dev, "cannot allocate Rx "
				    "descriptors for queue %d/%d error %d\n",
				    i, q, error);
				return (error);
			}
			rxr->vxrxr_rxd =
			    (struct vmxnet3_rxdesc *) rxr->vxrxr_dma.dma_vaddr;
		}

		error = vmxnet3_dma_malloc(sc, compsz, 512, &rxc->vxcr_dma);
		if (error) {
			device_printf(dev, "cannot alloc Rx comp descriptors "
			    "for queue %d error %d\n", q, error);
			return (error);
		}
		rxc->vxcr_u.rxcd =
		    (struct vmxnet3_rxcompdesc *) rxc->vxcr_dma.dma_vaddr;

		for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
			rxr = &rxq->vxrxq_cmd_ring[i];

			error = bus_dmamap_create(rxr->vxrxr_rxtag, 0,
			    &rxr->vxrxr_spare_dmap);
			if (error) {
				device_printf(dev, "unable to create spare "
				    "dmamap for queue %d/%d error %d\n",
				    q, i, error);
				return (error);
			}

			for (j = 0; j < rxr->vxrxr_ndesc; j++) {
				error = bus_dmamap_create(rxr->vxrxr_rxtag, 0,
				    &rxr->vxrxr_rxbuf[j].vrxb_dmamap);
				if (error) {
					device_printf(dev, "unable to create "
					    "dmamap for queue %d/%d slot %d "
					    "error %d\n",
					    q, i, j, error);
					return (error);
				}
			}
		}
	}

	return (0);
}

static void
vmxnet3_free_rxq_data(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;
	struct vmxnet3_rxbuf *rxb;
	int i, j, q;

	dev = sc->vmx_dev;

	for (q = 0; q < sc->vmx_nrxqueues; q++) {
		rxq = &sc->vmx_rxq[q];
		rxc = &rxq->vxrxq_comp_ring;

		for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
			rxr = &rxq->vxrxq_cmd_ring[i];

			if (rxr->vxrxr_spare_dmap != NULL) {
				bus_dmamap_destroy(rxr->vxrxr_rxtag,
				    rxr->vxrxr_spare_dmap);
				rxr->vxrxr_spare_dmap = NULL;
			}

			for (j = 0; j < rxr->vxrxr_ndesc; j++) {
				rxb = &rxr->vxrxr_rxbuf[j];
				if (rxb->vrxb_dmamap != NULL) {
					bus_dmamap_destroy(rxr->vxrxr_rxtag,
					    rxb->vrxb_dmamap);
					rxb->vrxb_dmamap = NULL;
				}
			}
		}

		if (rxc->vxcr_u.rxcd != NULL) {
			vmxnet3_dma_free(sc, &rxc->vxcr_dma);
			rxc->vxcr_u.rxcd = NULL;
		}

		for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
			rxr = &rxq->vxrxq_cmd_ring[i];

			if (rxr->vxrxr_rxd != NULL) {
				vmxnet3_dma_free(sc, &rxr->vxrxr_dma);
				rxr->vxrxr_rxd = NULL;
			}

			if (rxr->vxrxr_rxtag != NULL) {
				bus_dma_tag_destroy(rxr->vxrxr_rxtag);
				rxr->vxrxr_rxtag = NULL;
			}
		}
	}
}

static int
vmxnet3_alloc_queue_data(struct vmxnet3_softc *sc)
{
	int error;

	error = vmxnet3_alloc_txq_data(sc);
	if (error)
		return (error);

	error = vmxnet3_alloc_rxq_data(sc);
	if (error)
		return (error);

	return (0);
}

static void
vmxnet3_free_queue_data(struct vmxnet3_softc *sc)
{

	if (sc->vmx_rxq != NULL)
		vmxnet3_free_rxq_data(sc);

	if (sc->vmx_txq != NULL)
		vmxnet3_free_txq_data(sc);
}

static int
vmxnet3_alloc_mcast_table(struct vmxnet3_softc *sc)
{
	int error;

	error = vmxnet3_dma_malloc(sc, VMXNET3_MULTICAST_MAX * ETHER_ADDR_LEN,
	    32, &sc->vmx_mcast_dma);
	if (error)
		device_printf(sc->vmx_dev, "unable to alloc multicast table\n");
	else
		sc->vmx_mcast = sc->vmx_mcast_dma.dma_vaddr;

	return (error);
}

static void
vmxnet3_free_mcast_table(struct vmxnet3_softc *sc)
{

	if (sc->vmx_mcast != NULL) {
		vmxnet3_dma_free(sc, &sc->vmx_mcast_dma);
		sc->vmx_mcast = NULL;
	}
}

static void
vmxnet3_init_shared_data(struct vmxnet3_softc *sc)
{
	struct vmxnet3_driver_shared *ds;
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txq_shared *txs;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_rxq_shared *rxs;
	int i;

	ds = sc->vmx_ds;

	/*
	 * Initialize fields of the shared data that remains the same across
	 * reinits. Note the shared data is zero'd when allocated.
	 */

	ds->magic = VMXNET3_REV1_MAGIC;

	/* DriverInfo */
	ds->version = VMXNET3_DRIVER_VERSION;
	ds->guest = VMXNET3_GOS_FREEBSD |
#ifdef __LP64__
	    VMXNET3_GOS_64BIT;
#else
	    VMXNET3_GOS_32BIT;
#endif
	ds->vmxnet3_revision = 1;
	ds->upt_version = 1;

	/* Misc. conf */
	ds->driver_data = vtophys(sc);
	ds->driver_data_len = sizeof(struct vmxnet3_softc);
	ds->queue_shared = sc->vmx_qs_dma.dma_paddr;
	ds->queue_shared_len = sc->vmx_qs_dma.dma_size;
	ds->nrxsg_max = sc->vmx_max_rxsegs;

	/* Interrupt control. */
	ds->automask = sc->vmx_intr_mask_mode == VMXNET3_IMM_AUTO;
	ds->nintr = sc->vmx_nintrs;
	ds->evintr = sc->vmx_event_intr_idx;
	ds->ictrl = VMXNET3_ICTRL_DISABLE_ALL;

	for (i = 0; i < sc->vmx_nintrs; i++)
		ds->modlevel[i] = UPT1_IMOD_ADAPTIVE;

	/* Receive filter. */
	ds->mcast_table = sc->vmx_mcast_dma.dma_paddr;
	ds->mcast_tablelen = sc->vmx_mcast_dma.dma_size;

	/* Tx queues */
	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		txq = &sc->vmx_txq[i];
		txs = txq->vxtxq_ts;

		txs->cmd_ring = txq->vxtxq_cmd_ring.vxtxr_dma.dma_paddr;
		txs->cmd_ring_len = txq->vxtxq_cmd_ring.vxtxr_ndesc;
		txs->comp_ring = txq->vxtxq_comp_ring.vxcr_dma.dma_paddr;
		txs->comp_ring_len = txq->vxtxq_comp_ring.vxcr_ndesc;
		txs->driver_data = vtophys(txq);
		txs->driver_data_len = sizeof(struct vmxnet3_txqueue);
	}

	/* Rx queues */
	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		rxq = &sc->vmx_rxq[i];
		rxs = rxq->vxrxq_rs;

		rxs->cmd_ring[0] = rxq->vxrxq_cmd_ring[0].vxrxr_dma.dma_paddr;
		rxs->cmd_ring_len[0] = rxq->vxrxq_cmd_ring[0].vxrxr_ndesc;
		rxs->cmd_ring[1] = rxq->vxrxq_cmd_ring[1].vxrxr_dma.dma_paddr;
		rxs->cmd_ring_len[1] = rxq->vxrxq_cmd_ring[1].vxrxr_ndesc;
		rxs->comp_ring = rxq->vxrxq_comp_ring.vxcr_dma.dma_paddr;
		rxs->comp_ring_len = rxq->vxrxq_comp_ring.vxcr_ndesc;
		rxs->driver_data = vtophys(rxq);
		rxs->driver_data_len = sizeof(struct vmxnet3_rxqueue);
	}
}

static void
vmxnet3_reinit_interface(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vmx_ifp;

	/* Use the current MAC address. */
	bcopy(IF_LLADDR(sc->vmx_ifp), sc->vmx_lladdr, ETHER_ADDR_LEN);
	vmxnet3_set_lladdr(sc);

	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= VMXNET3_CSUM_OFFLOAD;
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= VMXNET3_CSUM_OFFLOAD_IPV6;
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TSO6)
		ifp->if_hwassist |= CSUM_TSO; /* No CSUM_TSO_IPV6. */
}

static void
vmxnet3_reinit_shared_data(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;
	struct vmxnet3_driver_shared *ds;

	ifp = sc->vmx_ifp;
	ds = sc->vmx_ds;

	ds->upt_features = 0;
	if (ifp->if_capenable & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6))
		ds->upt_features |= UPT1_F_CSUM;
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		ds->upt_features |= UPT1_F_VLAN;
	if (ifp->if_capenable & IFCAP_LRO)
		ds->upt_features |= UPT1_F_LRO;

	ds->mtu = ifp->if_mtu;
	ds->ntxqueue = sc->vmx_ntxqueues;
	ds->nrxqueue = sc->vmx_nrxqueues;

	vmxnet3_write_bar1(sc, VMXNET3_BAR1_DSL, sc->vmx_ds_dma.dma_paddr);
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_DSH,
	    (uint64_t) sc->vmx_ds_dma.dma_paddr >> 32);
}

static int
vmxnet3_alloc_data(struct vmxnet3_softc *sc)
{
	int error;

	error = vmxnet3_alloc_shared_data(sc);
	if (error)
		return (error);

	error = vmxnet3_alloc_queue_data(sc);
	if (error)
		return (error);

	error = vmxnet3_alloc_mcast_table(sc);
	if (error)
		return (error);

	vmxnet3_init_shared_data(sc);

	return (0);
}

static void
vmxnet3_free_data(struct vmxnet3_softc *sc)
{

	vmxnet3_free_mcast_table(sc);
	vmxnet3_free_queue_data(sc);
	vmxnet3_free_shared_data(sc);
}

static int
vmxnet3_setup_interface(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct ifnet *ifp;

	dev = sc->vmx_dev;

	ifp = sc->vmx_ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "cannot allocate ifnet structure\n");
		return (ENOSPC);
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
#if __FreeBSD_version < 1000025
	ifp->if_baudrate = 1000000000;
#else
	if_initbaudrate(ifp, IF_Gbps(10));
#endif
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = vmxnet3_init;
	ifp->if_ioctl = vmxnet3_ioctl;
	ifp->if_start = vmxnet3_start;
	ifp->if_snd.ifq_drv_maxlen = sc->vmx_ntxdescs - 1;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->vmx_ntxdescs - 1);
	IFQ_SET_READY(&ifp->if_snd);

	vmxnet3_get_lladdr(sc);
	ether_ifattach(ifp, sc->vmx_lladdr);

	ifp->if_capabilities |= IFCAP_RXCSUM | IFCAP_TXCSUM;
	ifp->if_capabilities |= IFCAP_RXCSUM_IPV6 | IFCAP_TXCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_TSO4 | IFCAP_TSO6;
	ifp->if_capabilities |= IFCAP_VLAN_MTU | IFCAP_VLAN_HWTAGGING |
	    IFCAP_VLAN_HWCSUM;
	ifp->if_capenable = ifp->if_capabilities;

	/* These capabilities are not enabled by default. */
	ifp->if_capabilities |= IFCAP_LRO | IFCAP_VLAN_HWFILTER;

	sc->vmx_vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    vmxnet3_register_vlan, sc, EVENTHANDLER_PRI_FIRST);
	sc->vmx_vlan_detach = EVENTHANDLER_REGISTER(vlan_config,
	    vmxnet3_unregister_vlan, sc, EVENTHANDLER_PRI_FIRST);

	ifmedia_init(&sc->vmx_media, 0, vmxnet3_media_change,
	    vmxnet3_media_status);
	ifmedia_add(&sc->vmx_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->vmx_media, IFM_ETHER | IFM_AUTO);

	return (0);
}

static void
vmxnet3_evintr(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct ifnet *ifp;
	struct vmxnet3_txq_shared *ts;
	struct vmxnet3_rxq_shared *rs;
	uint32_t event;
	int reset;

	dev = sc->vmx_dev;
	ifp = sc->vmx_ifp;
	reset = 0;

	VMXNET3_CORE_LOCK(sc);

	/* Clear events. */
	event = sc->vmx_ds->event;
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_EVENT, event);

	if (event & VMXNET3_EVENT_LINK)
		vmxnet3_link_status(sc);

	if (event & (VMXNET3_EVENT_TQERROR | VMXNET3_EVENT_RQERROR)) {
		reset = 1;
		vmxnet3_read_cmd(sc, VMXNET3_CMD_GET_STATUS);
		ts = sc->vmx_txq[0].vxtxq_ts;
		if (ts->stopped != 0)
			device_printf(dev, "Tx queue error %#x\n", ts->error);
		rs = sc->vmx_rxq[0].vxrxq_rs;
		if (rs->stopped != 0)
			device_printf(dev, "Rx queue error %#x\n", rs->error);
		device_printf(dev, "Rx/Tx queue error event ... resetting\n");
	}

	if (event & VMXNET3_EVENT_DIC)
		device_printf(dev, "device implementation change event\n");
	if (event & VMXNET3_EVENT_DEBUG)
		device_printf(dev, "debug event\n");

	if (reset != 0) {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		vmxnet3_init_locked(sc);
	}

	VMXNET3_CORE_UNLOCK(sc);
}

static void
vmxnet3_txq_eof(struct vmxnet3_txqueue *txq)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;
	struct vmxnet3_txring *txr;
	struct vmxnet3_comp_ring *txc;
	struct vmxnet3_txcompdesc *txcd;
	struct vmxnet3_txbuf *txb;
	u_int sop;

	sc = txq->vxtxq_sc;
	ifp = sc->vmx_ifp;
	txr = &txq->vxtxq_cmd_ring;
	txc = &txq->vxtxq_comp_ring;

	VMXNET3_TXQ_LOCK_ASSERT(txq);

	for (;;) {
		txcd = &txc->vxcr_u.txcd[txc->vxcr_next];
		if (txcd->gen != txc->vxcr_gen)
			break;
		vmxnet3_barrier(sc, VMXNET3_BARRIER_RD);

		if (++txc->vxcr_next == txc->vxcr_ndesc) {
			txc->vxcr_next = 0;
			txc->vxcr_gen ^= 1;
		}

		sop = txr->vxtxr_next;
		txb = &txr->vxtxr_txbuf[sop];

		if (txb->vtxb_m != NULL) {
			bus_dmamap_sync(txr->vxtxr_txtag, txb->vtxb_dmamap,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->vxtxr_txtag, txb->vtxb_dmamap);

			m_freem(txb->vtxb_m);
			txb->vtxb_m = NULL;

			ifp->if_opackets++;
		}

		txr->vxtxr_next = (txcd->eop_idx + 1) % txr->vxtxr_ndesc;
	}

	if (txr->vxtxr_head == txr->vxtxr_next)
		txq->vxtxq_watchdog = 0;
}

static int
vmxnet3_newbuf(struct vmxnet3_softc *sc, struct vmxnet3_rxring *rxr)
{
	struct ifnet *ifp;
	struct mbuf *m;
	struct vmxnet3_rxdesc *rxd;
	struct vmxnet3_rxbuf *rxb;
	bus_dma_tag_t tag;
	bus_dmamap_t dmap;
	bus_dma_segment_t segs[1];
	int idx, clsize, btype, flags, nsegs, error;

	ifp = sc->vmx_ifp;
	tag = rxr->vxrxr_rxtag;
	dmap = rxr->vxrxr_spare_dmap;
	idx = rxr->vxrxr_fill;
	rxd = &rxr->vxrxr_rxd[idx];
	rxb = &rxr->vxrxr_rxbuf[idx];

#ifdef VMXNET3_FAILPOINTS
	KFAIL_POINT_CODE(VMXNET3_FP, newbuf, return ENOBUFS);
	if (rxr->vxrxr_rid != 0)
		KFAIL_POINT_CODE(VMXNET3_FP, newbuf_body_only, return ENOBUFS);
#endif

	if (rxr->vxrxr_rid == 0 && (idx % sc->vmx_rx_max_chain) == 0) {
		flags = M_PKTHDR;
		clsize = MCLBYTES;
		btype = VMXNET3_BTYPE_HEAD;
	} else {
#if __FreeBSD_version < 902001
		/*
		 * These mbufs will never be used for the start of a frame.
		 * Roughly prior to branching releng/9.2, the load_mbuf_sg()
		 * required the mbuf to always be a packet header. Avoid
		 * unnecessary mbuf initialization in newer versions where
		 * that is not the case.
		 */
		flags = M_PKTHDR;
#else
		flags = 0;
#endif
		clsize = MJUMPAGESIZE;
		btype = VMXNET3_BTYPE_BODY;
	}

	m = m_getjcl(M_NOWAIT, MT_DATA, flags, clsize);
	if (m == NULL) {
		sc->vmx_stats.vmst_mgetcl_failed++;
		return (ENOBUFS);
	}

	if (btype == VMXNET3_BTYPE_HEAD) {
		m->m_len = m->m_pkthdr.len = clsize;
		m_adj(m, ETHER_ALIGN);
	} else
		m->m_len = clsize;

	error = bus_dmamap_load_mbuf_sg(tag, dmap, m, &segs[0], &nsegs,
	    BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		sc->vmx_stats.vmst_mbuf_load_failed++;
		return (error);
	}
	KASSERT(nsegs == 1,
	    ("%s: mbuf %p with too many segments %d", __func__, m, nsegs));
#if __FreeBSD_version < 902001
	if (btype == VMXNET3_BTYPE_BODY)
		m->m_flags &= ~M_PKTHDR;
#endif

	if (rxb->vrxb_m != NULL) {
		bus_dmamap_sync(tag, rxb->vrxb_dmamap, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(tag, rxb->vrxb_dmamap);
	}

	rxr->vxrxr_spare_dmap = rxb->vrxb_dmamap;
	rxb->vrxb_dmamap = dmap;
	rxb->vrxb_m = m;

	rxd->addr = segs[0].ds_addr;
	rxd->len = segs[0].ds_len;
	rxd->btype = btype;
	rxd->gen = rxr->vxrxr_gen;

	vmxnet3_rxr_increment_fill(rxr);
	return (0);
}

static void
vmxnet3_rxq_eof_discard(struct vmxnet3_rxqueue *rxq,
    struct vmxnet3_rxring *rxr, int idx)
{
	struct vmxnet3_rxdesc *rxd;

	rxd = &rxr->vxrxr_rxd[idx];
	rxd->gen = rxr->vxrxr_gen;
	vmxnet3_rxr_increment_fill(rxr);
}

static void
vmxnet3_rxq_discard_chain(struct vmxnet3_rxqueue *rxq)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;
	struct vmxnet3_rxcompdesc *rxcd;
	int idx, eof;

	sc = rxq->vxrxq_sc;
	rxc = &rxq->vxrxq_comp_ring;

	do {
		rxcd = &rxc->vxcr_u.rxcd[rxc->vxcr_next];
		if (rxcd->gen != rxc->vxcr_gen)
			break;		/* Not expected. */
		vmxnet3_barrier(sc, VMXNET3_BARRIER_RD);

		if (++rxc->vxcr_next == rxc->vxcr_ndesc) {
			rxc->vxcr_next = 0;
			rxc->vxcr_gen ^= 1;
		}

		idx = rxcd->rxd_idx;
		eof = rxcd->eop;
		if (rxcd->qid < sc->vmx_nrxqueues)
			rxr = &rxq->vxrxq_cmd_ring[0];
		else
			rxr = &rxq->vxrxq_cmd_ring[1];
		vmxnet3_rxq_eof_discard(rxq, rxr, idx);
	} while (!eof);
}

static void
vmxnet3_rx_csum(struct vmxnet3_rxcompdesc *rxcd, struct mbuf *m)
{

	if (rxcd->ipv4) {
		m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
		if (rxcd->ipcsum_ok)
			m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
	}

	if (!rxcd->fragment) {
		if (rxcd->csum_ok && (rxcd->tcp || rxcd->udp)) {
			m->m_pkthdr.csum_flags |= CSUM_DATA_VALID |
			    CSUM_PSEUDO_HDR;
			m->m_pkthdr.csum_data = 0xFFFF;
		}
	}
}

static void
vmxnet3_rxq_input(struct vmxnet3_rxqueue *rxq,
    struct vmxnet3_rxcompdesc *rxcd, struct mbuf *m)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;

	sc = rxq->vxrxq_sc;
	ifp = sc->vmx_ifp;

	if (rxcd->error) {
		ifp->if_ierrors++;
		m_freem(m);
		return;
	}

	if (!rxcd->no_csum)
		vmxnet3_rx_csum(rxcd, m);
	if (rxcd->vlan) {
		m->m_flags |= M_VLANTAG;
		m->m_pkthdr.ether_vtag = rxcd->vtag;
	}

	ifp->if_ipackets++;
	VMXNET3_RXQ_UNLOCK(rxq);
	(*ifp->if_input)(ifp, m);
	VMXNET3_RXQ_LOCK(rxq);
}

static void
vmxnet3_rxq_eof(struct vmxnet3_rxqueue *rxq)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;
	struct vmxnet3_rxdesc *rxd;
	struct vmxnet3_rxcompdesc *rxcd;
	struct mbuf *m, *m_head, *m_tail;
	int idx, length;

	sc = rxq->vxrxq_sc;
	ifp = sc->vmx_ifp;
	rxc = &rxq->vxrxq_comp_ring;
	m_head = m_tail = NULL;

	VMXNET3_RXQ_LOCK_ASSERT(rxq);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	for (;;) {
		rxcd = &rxc->vxcr_u.rxcd[rxc->vxcr_next];
		if (rxcd->gen != rxc->vxcr_gen)
			break;
		vmxnet3_barrier(sc, VMXNET3_BARRIER_RD);

		if (++rxc->vxcr_next == rxc->vxcr_ndesc) {
			rxc->vxcr_next = 0;
			rxc->vxcr_gen ^= 1;
		}

		idx = rxcd->rxd_idx;
		length = rxcd->len;
		if (rxcd->qid < sc->vmx_nrxqueues)
			rxr = &rxq->vxrxq_cmd_ring[0];
		else
			rxr = &rxq->vxrxq_cmd_ring[1];
		rxd = &rxr->vxrxr_rxd[idx];

		m = rxr->vxrxr_rxbuf[idx].vrxb_m;
		KASSERT(m != NULL, ("%s: queue %d idx %d without mbuf",
		    __func__, rxcd->qid, idx));

		/*
		 * The host may skip descriptors. We detect this when this
		 * descriptor does not match the previous fill index. Catch
		 * up with the host now.
		 */
		if (__predict_false(rxr->vxrxr_fill != idx)) {
			while (rxr->vxrxr_fill != idx) {
				rxr->vxrxr_rxd[rxr->vxrxr_fill].gen =
				    rxr->vxrxr_gen;
				vmxnet3_rxr_increment_fill(rxr);
			}
		}

		if (rxcd->sop) {
			KASSERT(rxd->btype == VMXNET3_BTYPE_HEAD,
			    ("%s: start of frame w/o head buffer", __func__));
			KASSERT(rxr == &rxq->vxrxq_cmd_ring[0],
			    ("%s: start of frame not in ring 0", __func__));
			KASSERT((idx % sc->vmx_rx_max_chain) == 0,
			    ("%s: start of frame at unexcepted index %d (%d)",
			     __func__, idx, sc->vmx_rx_max_chain));
			KASSERT(m_head == NULL,
			    ("%s: duplicate start of frame?", __func__));

			if (length == 0) {
				/* Just ignore this descriptor. */
				vmxnet3_rxq_eof_discard(rxq, rxr, idx);
				goto nextp;
			}

			if (vmxnet3_newbuf(sc, rxr) != 0) {
				ifp->if_iqdrops++;
				vmxnet3_rxq_eof_discard(rxq, rxr, idx);
				if (!rxcd->eop)
					vmxnet3_rxq_discard_chain(rxq);
				goto nextp;
			}

			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = length;
			m->m_pkthdr.csum_flags = 0;
			m_head = m_tail = m;

		} else {
			KASSERT(rxd->btype == VMXNET3_BTYPE_BODY,
			    ("%s: non start of frame w/o body buffer", __func__));
			KASSERT(m_head != NULL,
			    ("%s: frame not started?", __func__));

			if (vmxnet3_newbuf(sc, rxr) != 0) {
				ifp->if_iqdrops++;
				vmxnet3_rxq_eof_discard(rxq, rxr, idx);
				if (!rxcd->eop)
					vmxnet3_rxq_discard_chain(rxq);
				m_freem(m_head);
				m_head = m_tail = NULL;
				goto nextp;
			}

			m->m_len = length;
			m_head->m_pkthdr.len += length;
			m_tail->m_next = m;
			m_tail = m;
		}

		if (rxcd->eop) {
			vmxnet3_rxq_input(rxq, rxcd, m_head);
			m_head = m_tail = NULL;

			/* Must recheck after dropping the Rx lock. */
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				break;
		}

nextp:
		if (__predict_false(rxq->vxrxq_rs->update_rxhead)) {
			int qid = rxcd->qid;
			bus_size_t r;

			idx = (idx + 1) % rxr->vxrxr_ndesc;
			if (qid >= sc->vmx_nrxqueues) {
				qid -= sc->vmx_nrxqueues;
				r = VMXNET3_BAR0_RXH2(qid);
			} else
				r = VMXNET3_BAR0_RXH1(qid);
			vmxnet3_write_bar0(sc, r, idx);
		}
	}
}

static void
vmxnet3_legacy_intr(void *xsc)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_txqueue *txq;
	struct ifnet *ifp;

	sc = xsc;
	rxq = &sc->vmx_rxq[0];
	txq = &sc->vmx_txq[0];
	ifp = sc->vmx_ifp;

	if (sc->vmx_intr_type == VMXNET3_IT_LEGACY) {
		if (vmxnet3_read_bar1(sc, VMXNET3_BAR1_INTR) == 0)
			return;
	}
	if (sc->vmx_intr_mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_all_intrs(sc);

	if (sc->vmx_ds->event != 0)
		vmxnet3_evintr(sc);

	VMXNET3_RXQ_LOCK(rxq);
	vmxnet3_rxq_eof(rxq);
	VMXNET3_RXQ_UNLOCK(rxq);

	VMXNET3_TXQ_LOCK(txq);
	vmxnet3_txq_eof(txq);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vmxnet3_start_locked(ifp);
	VMXNET3_TXQ_UNLOCK(txq);

	vmxnet3_enable_all_intrs(sc);
}

static void
vmxnet3_txq_intr(void *xtxq)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_txqueue *txq;
	struct ifnet *ifp;

	txq = xtxq;
	sc = txq->vxtxq_sc;
	ifp = sc->vmx_ifp;

	if (sc->vmx_intr_mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(sc, txq->vxtxq_intr_idx);

	VMXNET3_TXQ_LOCK(txq);
	vmxnet3_txq_eof(txq);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		vmxnet3_start_locked(ifp);
	VMXNET3_TXQ_UNLOCK(txq);

	vmxnet3_enable_intr(sc, txq->vxtxq_intr_idx);
}

static void
vmxnet3_rxq_intr(void *xrxq)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_rxqueue *rxq;

	rxq = xrxq;
	sc = rxq->vxrxq_sc;

	if (sc->vmx_intr_mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(sc, rxq->vxrxq_intr_idx);

	VMXNET3_RXQ_LOCK(rxq);
	vmxnet3_rxq_eof(rxq);
	VMXNET3_RXQ_UNLOCK(rxq);

	vmxnet3_enable_intr(sc, rxq->vxrxq_intr_idx);
}

static void
vmxnet3_event_intr(void *xsc)
{
	struct vmxnet3_softc *sc;

	sc = xsc;

	if (sc->vmx_intr_mask_mode == VMXNET3_IMM_ACTIVE)
		vmxnet3_disable_intr(sc, sc->vmx_event_intr_idx);

	if (sc->vmx_ds->event != 0)
		vmxnet3_evintr(sc);

	vmxnet3_enable_intr(sc, sc->vmx_event_intr_idx);
}

static void
vmxnet3_txstop(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *txq)
{
	struct vmxnet3_txring *txr;
	struct vmxnet3_txbuf *txb;
	int i;

	txr = &txq->vxtxq_cmd_ring;

	for (i = 0; i < txr->vxtxr_ndesc; i++) {
		txb = &txr->vxtxr_txbuf[i];

		if (txb->vtxb_m == NULL)
			continue;

		bus_dmamap_sync(txr->vxtxr_txtag, txb->vtxb_dmamap,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(txr->vxtxr_txtag, txb->vtxb_dmamap);
		m_freem(txb->vtxb_m);
		txb->vtxb_m = NULL;
	}
}

static void
vmxnet3_rxstop(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rxq)
{
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_rxbuf *rxb;
	int i, j;

	for (i = 0; i < VMXNET3_RXRINGS_PERQ; i++) {
		rxr = &rxq->vxrxq_cmd_ring[i];

		for (j = 0; j < rxr->vxrxr_ndesc; j++) {
			rxb = &rxr->vxrxr_rxbuf[j];

			if (rxb->vrxb_m == NULL)
				continue;
			bus_dmamap_sync(rxr->vxrxr_rxtag, rxb->vrxb_dmamap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->vxrxr_rxtag, rxb->vrxb_dmamap);
			m_freem(rxb->vrxb_m);
			rxb->vrxb_m = NULL;
		}
	}
}

static void
vmxnet3_stop_rendezvous(struct vmxnet3_softc *sc)
{
	struct vmxnet3_rxqueue *rxq;
	struct vmxnet3_txqueue *txq;
	int i;

	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		rxq = &sc->vmx_rxq[i];
		VMXNET3_RXQ_LOCK(rxq);
		VMXNET3_RXQ_UNLOCK(rxq);
	}

	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		txq = &sc->vmx_txq[i];
		VMXNET3_TXQ_LOCK(txq);
		VMXNET3_TXQ_UNLOCK(txq);
	}
}

static void
vmxnet3_stop(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;
	int q;

	ifp = sc->vmx_ifp;
	VMXNET3_CORE_LOCK_ASSERT(sc);

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->vmx_link_active = 0;
	callout_stop(&sc->vmx_tick);

	/* Disable interrupts. */
	vmxnet3_disable_all_intrs(sc);
	vmxnet3_write_cmd(sc, VMXNET3_CMD_DISABLE);

	vmxnet3_stop_rendezvous(sc);

	for (q = 0; q < sc->vmx_ntxqueues; q++)
		vmxnet3_txstop(sc, &sc->vmx_txq[q]);
	for (q = 0; q < sc->vmx_nrxqueues; q++)
		vmxnet3_rxstop(sc, &sc->vmx_rxq[q]);

	vmxnet3_write_cmd(sc, VMXNET3_CMD_RESET);
}

static void
vmxnet3_txinit(struct vmxnet3_softc *sc, struct vmxnet3_txqueue *txq)
{
	struct vmxnet3_txring *txr;
	struct vmxnet3_comp_ring *txc;

	txr = &txq->vxtxq_cmd_ring;
	txr->vxtxr_head = 0;
	txr->vxtxr_next = 0;
	txr->vxtxr_gen = VMXNET3_INIT_GEN;
	bzero(txr->vxtxr_txd,
	    txr->vxtxr_ndesc * sizeof(struct vmxnet3_txdesc));

	txc = &txq->vxtxq_comp_ring;
	txc->vxcr_next = 0;
	txc->vxcr_gen = VMXNET3_INIT_GEN;
	bzero(txc->vxcr_u.txcd,
	    txc->vxcr_ndesc * sizeof(struct vmxnet3_txcompdesc));
}

static int
vmxnet3_rxinit(struct vmxnet3_softc *sc, struct vmxnet3_rxqueue *rxq)
{
	struct ifnet *ifp;
	struct vmxnet3_rxring *rxr;
	struct vmxnet3_comp_ring *rxc;
	int i, populate, idx, frame_size, error;

	ifp = sc->vmx_ifp;
	frame_size = ETHER_ALIGN + sizeof(struct ether_vlan_header) +
	    ifp->if_mtu;

	/*
	 * If the MTU causes us to exceed what a regular sized cluster can
	 * handle, we allocate a second MJUMPAGESIZE cluster after it in
	 * ring 0. If in use, ring 1 always contains MJUMPAGESIZE clusters.
	 *
	 * Keep rx_max_chain a divisor of the maximum Rx ring size to make
	 * our life easier. We do not support changing the ring size after
	 * the attach.
	 */
	if (frame_size <= MCLBYTES)
		sc->vmx_rx_max_chain = 1;
	else
		sc->vmx_rx_max_chain = 2;

	/*
	 * Only populate ring 1 if the configuration will take advantage
	 * of it. That is either when LRO is enabled or the frame size
	 * exceeds what ring 0 can contain.
	 */
	if ((ifp->if_capenable & IFCAP_LRO) == 0 &&
	    frame_size <= MCLBYTES + MJUMPAGESIZE)
		populate = 1;
	else
		populate = VMXNET3_RXRINGS_PERQ;

	for (i = 0; i < populate; i++) {
		rxr = &rxq->vxrxq_cmd_ring[i];
		rxr->vxrxr_fill = 0;
		rxr->vxrxr_gen = VMXNET3_INIT_GEN;
		bzero(rxr->vxrxr_rxd,
		    rxr->vxrxr_ndesc * sizeof(struct vmxnet3_rxdesc));

		for (idx = 0; idx < rxr->vxrxr_ndesc; idx++) {
			error = vmxnet3_newbuf(sc, rxr);
			if (error)
				return (error);
		}
	}

	for (/**/; i < VMXNET3_RXRINGS_PERQ; i++) {
		rxr = &rxq->vxrxq_cmd_ring[i];
		rxr->vxrxr_fill = 0;
		rxr->vxrxr_gen = 0;
		bzero(rxr->vxrxr_rxd,
		    rxr->vxrxr_ndesc * sizeof(struct vmxnet3_rxdesc));
	}

	rxc = &rxq->vxrxq_comp_ring;
	rxc->vxcr_next = 0;
	rxc->vxcr_gen = VMXNET3_INIT_GEN;
	bzero(rxc->vxcr_u.rxcd,
	    rxc->vxcr_ndesc * sizeof(struct vmxnet3_rxcompdesc));

	return (0);
}

static int
vmxnet3_reinit_queues(struct vmxnet3_softc *sc)
{
	device_t dev;
	int q, error;

	dev = sc->vmx_dev;

	for (q = 0; q < sc->vmx_ntxqueues; q++)
		vmxnet3_txinit(sc, &sc->vmx_txq[q]);

	for (q = 0; q < sc->vmx_nrxqueues; q++) {
		error = vmxnet3_rxinit(sc, &sc->vmx_rxq[q]);
		if (error) {
			device_printf(dev, "cannot populate Rx queue %d\n", q);
			return (error);
		}
	}

	return (0);
}

static int
vmxnet3_enable_device(struct vmxnet3_softc *sc)
{
	int q;

	if (vmxnet3_read_cmd(sc, VMXNET3_CMD_ENABLE) != 0) {
		device_printf(sc->vmx_dev, "device enable command failed!\n");
		return (1);
	}

	/* Reset the Rx queue heads. */
	for (q = 0; q < sc->vmx_nrxqueues; q++) {
		vmxnet3_write_bar0(sc, VMXNET3_BAR0_RXH1(q), 0);
		vmxnet3_write_bar0(sc, VMXNET3_BAR0_RXH2(q), 0);
	}

	return (0);
}

static void
vmxnet3_reinit_rxfilters(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vmx_ifp;

	vmxnet3_set_rxfilter(sc);

	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
		bcopy(sc->vmx_vlan_filter, sc->vmx_ds->vlan_filter,
		    sizeof(sc->vmx_ds->vlan_filter));
	else
		bzero(sc->vmx_ds->vlan_filter,
		    sizeof(sc->vmx_ds->vlan_filter));
	vmxnet3_write_cmd(sc, VMXNET3_CMD_VLAN_FILTER);
}

static int
vmxnet3_reinit(struct vmxnet3_softc *sc)
{

	vmxnet3_reinit_interface(sc);
	vmxnet3_reinit_shared_data(sc);

	if (vmxnet3_reinit_queues(sc) != 0)
		return (ENXIO);

	if (vmxnet3_enable_device(sc) != 0)
		return (ENXIO);

	vmxnet3_reinit_rxfilters(sc);

	return (0);
}

static void
vmxnet3_init_locked(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;

	ifp = sc->vmx_ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		return;

	vmxnet3_stop(sc);

	if (vmxnet3_reinit(sc) != 0) {
		vmxnet3_stop(sc);
		return;
	}

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	vmxnet3_link_status(sc);

	vmxnet3_enable_all_intrs(sc);
	callout_reset(&sc->vmx_tick, hz, vmxnet3_tick, sc);
}

static void
vmxnet3_init(void *xsc)
{
	struct vmxnet3_softc *sc;

	sc = xsc;

	VMXNET3_CORE_LOCK(sc);
	vmxnet3_init_locked(sc);
	VMXNET3_CORE_UNLOCK(sc);
}

/*
 * BMV: Much of this can go away once we finally have offsets in
 * the mbuf packet header. Bug andre@.
 */
static int
vmxnet3_txq_offload_ctx(struct mbuf *m, int *etype, int *proto, int *start)
{
	struct ether_vlan_header *evh;
	int offset;

	evh = mtod(m, struct ether_vlan_header *);
	if (evh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		/* BMV: We should handle nested VLAN tags too. */
		*etype = ntohs(evh->evl_proto);
		offset = sizeof(struct ether_vlan_header);
	} else {
		*etype = ntohs(evh->evl_encap_proto);
		offset = sizeof(struct ether_header);
	}

	switch (*etype) {
#if defined(INET)
	case ETHERTYPE_IP: {
		struct ip *ip, iphdr;
		if (__predict_false(m->m_len < offset + sizeof(struct ip))) {
			m_copydata(m, offset, sizeof(struct ip),
			    (caddr_t) &iphdr);
			ip = &iphdr;
		} else
			ip = (struct ip *)(m->m_data + offset);
		*proto = ip->ip_p;
		*start = offset + (ip->ip_hl << 2);
		break;
	}
#endif
#if defined(INET6)
	case ETHERTYPE_IPV6:
		*proto = -1;
		*start = ip6_lasthdr(m, offset, IPPROTO_IPV6, proto);
		/* Assert the network stack sent us a valid packet. */
		KASSERT(*start > offset,
		    ("%s: mbuf %p start %d offset %d proto %d", __func__, m,
		    *start, offset, *proto));
		break;
#endif
	default:
		return (EINVAL);
	}

	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		struct tcphdr *tcp, tcphdr;

		if (__predict_false(*proto != IPPROTO_TCP)) {
			/* Likely failed to correctly parse the mbuf. */
			return (EINVAL);
		}

		if (m->m_len < *start + sizeof(struct tcphdr)) {
			m_copydata(m, offset, sizeof(struct tcphdr),
			    (caddr_t) &tcphdr);
			tcp = &tcphdr;
		} else
			tcp = (struct tcphdr *)(m->m_data + *start);

		/*
		 * For TSO, the size of the protocol header is also
		 * included in the descriptor header size.
		 */
		*start += (tcp->th_off << 2);
	}

	return (0);
}

static int
vmxnet3_txq_load_mbuf(struct vmxnet3_txqueue *txq, struct mbuf **m0,
    bus_dmamap_t dmap, bus_dma_segment_t segs[], int *nsegs)
{
	struct vmxnet3_txring *txr;
	struct mbuf *m;
	bus_dma_tag_t tag;
	int maxsegs, error;

	txr = &txq->vxtxq_cmd_ring;
	m = *m0;
	tag = txr->vxtxr_txtag;
	maxsegs = VMXNET3_TX_MAXSEGS;

	error = bus_dmamap_load_mbuf_sg(tag, dmap, m, segs, nsegs, 0);
	if (error == 0 || error != EFBIG)
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
	} else
		txq->vxtxq_sc->vmx_stats.vmst_collapsed++;

	return (error);
}

static void
vmxnet3_txq_unload_mbuf(struct vmxnet3_txqueue *txq, bus_dmamap_t dmap)
{
	struct vmxnet3_txring *txr;

	txr = &txq->vxtxq_cmd_ring;
	bus_dmamap_unload(txr->vxtxr_txtag, dmap);
}

static int
vmxnet3_txq_encap(struct vmxnet3_txqueue *txq, struct mbuf **m0)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;
	struct vmxnet3_txring *txr;
	struct vmxnet3_txdesc *txd, *sop;
	struct mbuf *m;
	bus_dmamap_t dmap;
	bus_dma_segment_t segs[VMXNET3_TX_MAXSEGS];
	int i, gen, nsegs, etype, proto, start, error;

	sc = txq->vxtxq_sc;
	ifp = sc->vmx_ifp;
	start = 0;
	txd = NULL;
	txr = &txq->vxtxq_cmd_ring;
	dmap = txr->vxtxr_txbuf[txr->vxtxr_head].vtxb_dmamap;

	error = vmxnet3_txq_load_mbuf(txq, m0, dmap, segs, &nsegs);
	if (error)
		return (error);

	m = *m0;
	M_ASSERTPKTHDR(m);
	KASSERT(nsegs <= VMXNET3_TX_MAXSEGS,
	    ("%s: mbuf %p with too many segments %d", __func__, m, nsegs));

	if (VMXNET3_TXRING_AVAIL(txr) < nsegs) {
		txq->vxtxq_stats.vtxrs_full++;
		vmxnet3_txq_unload_mbuf(txq, dmap);
		return (ENOSPC);
	} else if (m->m_pkthdr.csum_flags & VMXNET3_CSUM_ALL_OFFLOAD) {
		error = vmxnet3_txq_offload_ctx(m, &etype, &proto, &start);
		if (error) {
			txq->vxtxq_stats.vtxrs_offload_failed++;
			vmxnet3_txq_unload_mbuf(txq, dmap);
			m_freem(m);
			*m0 = NULL;
			return (error);
		}
	}

	txr->vxtxr_txbuf[txr->vxtxr_head].vtxb_m = m = *m0;
	sop = &txr->vxtxr_txd[txr->vxtxr_head];
	gen = txr->vxtxr_gen ^ 1;	/* Owned by cpu (yet) */

	for (i = 0; i < nsegs; i++) {
		txd = &txr->vxtxr_txd[txr->vxtxr_head];

		txd->addr = segs[i].ds_addr;
		txd->len = segs[i].ds_len;
		txd->gen = gen;
		txd->dtype = 0;
		txd->offload_mode = VMXNET3_OM_NONE;
		txd->offload_pos = 0;
		txd->hlen = 0;
		txd->eop = 0;
		txd->compreq = 0;
		txd->vtag_mode = 0;
		txd->vtag = 0;

		if (++txr->vxtxr_head == txr->vxtxr_ndesc) {
			txr->vxtxr_head = 0;
			txr->vxtxr_gen ^= 1;
		}
		gen = txr->vxtxr_gen;
	}
	txd->eop = 1;
	txd->compreq = 1;

	if (m->m_flags & M_VLANTAG) {
		sop->vtag_mode = 1;
		sop->vtag = m->m_pkthdr.ether_vtag;
	}

	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		sop->offload_mode = VMXNET3_OM_TSO;
		sop->hlen = start;
		sop->offload_pos = m->m_pkthdr.tso_segsz;
	} else if (m->m_pkthdr.csum_flags & (VMXNET3_CSUM_OFFLOAD |
	    VMXNET3_CSUM_OFFLOAD_IPV6)) {
		sop->offload_mode = VMXNET3_OM_CSUM;
		sop->hlen = start;
		sop->offload_pos = start + m->m_pkthdr.csum_data;
	}

	/* Finally, change the ownership. */
	vmxnet3_barrier(sc, VMXNET3_BARRIER_WR);
	sop->gen ^= 1;

	if (++txq->vxtxq_ts->npending >= txq->vxtxq_ts->intr_threshold) {
		txq->vxtxq_ts->npending = 0;
		vmxnet3_write_bar0(sc, VMXNET3_BAR0_TXH(txq->vxtxq_id),
		    txr->vxtxr_head);
	}

	return (0);
}

static void
vmxnet3_start_locked(struct ifnet *ifp)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_txqueue *txq;
	struct vmxnet3_txring *txr;
	struct mbuf *m_head;
	int tx, avail;

	sc = ifp->if_softc;
	txq = &sc->vmx_txq[0];
	txr = &txq->vxtxq_cmd_ring;
	tx = 0;

	VMXNET3_TXQ_LOCK_ASSERT(txq);

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0 ||
	    sc->vmx_link_active == 0)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		if ((avail = VMXNET3_TXRING_AVAIL(txr)) < 2)
			break;

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Assume worse case if this mbuf is the head of a chain. */
		if (m_head->m_next != NULL && avail < VMXNET3_TX_MAXSEGS) {
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		if (vmxnet3_txq_encap(txq, &m_head) != 0) {
			if (m_head != NULL)
				IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		tx++;
		ETHER_BPF_MTAP(ifp, m_head);
	}

	if (tx > 0) {
		if (txq->vxtxq_ts->npending > 0) {
			txq->vxtxq_ts->npending = 0;
			vmxnet3_write_bar0(sc, VMXNET3_BAR0_TXH(txq->vxtxq_id),
			    txr->vxtxr_head);
		}
		txq->vxtxq_watchdog = VMXNET3_WATCHDOG_TIMEOUT;
	}
}

static void
vmxnet3_start(struct ifnet *ifp)
{
	struct vmxnet3_softc *sc;
	struct vmxnet3_txqueue *txq;

	sc = ifp->if_softc;
	txq = &sc->vmx_txq[0];

	VMXNET3_TXQ_LOCK(txq);
	vmxnet3_start_locked(ifp);
	VMXNET3_TXQ_UNLOCK(txq);
}

static void
vmxnet3_update_vlan_filter(struct vmxnet3_softc *sc, int add, uint16_t tag)
{
	struct ifnet *ifp;
	int idx, bit;

	ifp = sc->vmx_ifp;
	idx = (tag >> 5) & 0x7F;
	bit = tag & 0x1F;

	if (tag == 0 || tag > 4095)
		return;

	VMXNET3_CORE_LOCK(sc);

	/* Update our private VLAN bitvector. */
	if (add)
		sc->vmx_vlan_filter[idx] |= (1 << bit);
	else
		sc->vmx_vlan_filter[idx] &= ~(1 << bit);

	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER) {
		if (add)
			sc->vmx_ds->vlan_filter[idx] |= (1 << bit);
		else
			sc->vmx_ds->vlan_filter[idx] &= ~(1 << bit);
		vmxnet3_write_cmd(sc, VMXNET3_CMD_VLAN_FILTER);
	}

	VMXNET3_CORE_UNLOCK(sc);
}

static void
vmxnet3_register_vlan(void *arg, struct ifnet *ifp, uint16_t tag)
{

	if (ifp->if_softc == arg)
		vmxnet3_update_vlan_filter(arg, 1, tag);
}

static void
vmxnet3_unregister_vlan(void *arg, struct ifnet *ifp, uint16_t tag)
{

	if (ifp->if_softc == arg)
		vmxnet3_update_vlan_filter(arg, 0, tag);
}

static void
vmxnet3_set_rxfilter(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;
	struct vmxnet3_driver_shared *ds;
	struct ifmultiaddr *ifma;
	u_int mode;

	ifp = sc->vmx_ifp;
	ds = sc->vmx_ds;

	mode = VMXNET3_RXMODE_UCAST;
	if (ifp->if_flags & IFF_BROADCAST)
		mode |= VMXNET3_RXMODE_BCAST;
	if (ifp->if_flags & IFF_PROMISC)
		mode |= VMXNET3_RXMODE_PROMISC;
	if (ifp->if_flags & IFF_ALLMULTI)
		mode |= VMXNET3_RXMODE_ALLMULTI;
	else {
		int cnt = 0, overflow = 0;

		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
			if (ifma->ifma_addr->sa_family != AF_LINK)
				continue;
			else if (cnt == VMXNET3_MULTICAST_MAX) {
				overflow = 1;
				break;
			}

			bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
			   &sc->vmx_mcast[cnt*ETHER_ADDR_LEN], ETHER_ADDR_LEN);
			cnt++;
		}
		if_maddr_runlock(ifp);

		if (overflow != 0) {
			cnt = 0;
			mode |= VMXNET3_RXMODE_ALLMULTI;
		} else if (cnt > 0)
			mode |= VMXNET3_RXMODE_MCAST;
		ds->mcast_tablelen = cnt * ETHER_ADDR_LEN;
	}

	ds->rxmode = mode;

	vmxnet3_write_cmd(sc, VMXNET3_CMD_SET_FILTER);
	vmxnet3_write_cmd(sc, VMXNET3_CMD_SET_RXMODE);
}

static int
vmxnet3_change_mtu(struct vmxnet3_softc *sc, int mtu)
{
	struct ifnet *ifp;

	ifp = sc->vmx_ifp;

	if (mtu < VMXNET3_MIN_MTU || mtu > VMXNET3_MAX_MTU)
		return (EINVAL);

	ifp->if_mtu = mtu;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		vmxnet3_init_locked(sc);
	}

	return (0);
}

static int
vmxnet3_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vmxnet3_softc *sc;
	struct ifreq *ifr;
	int reinit, mask, error;

	sc = ifp->if_softc;
	ifr = (struct ifreq *) data;
	error = 0;

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifp->if_mtu != ifr->ifr_mtu) {
			VMXNET3_CORE_LOCK(sc);
			error = vmxnet3_change_mtu(sc, ifr->ifr_mtu);
			VMXNET3_CORE_UNLOCK(sc);
		}
		break;

	case SIOCSIFFLAGS:
		VMXNET3_CORE_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ sc->vmx_if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					vmxnet3_set_rxfilter(sc);
				}
			} else
				vmxnet3_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				vmxnet3_stop(sc);
		}
		sc->vmx_if_flags = ifp->if_flags;
		VMXNET3_CORE_UNLOCK(sc);
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		VMXNET3_CORE_LOCK(sc);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			vmxnet3_set_rxfilter(sc);
		VMXNET3_CORE_UNLOCK(sc);
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->vmx_media, cmd);
		break;

	case SIOCSIFCAP:
		VMXNET3_CORE_LOCK(sc);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (mask & IFCAP_TXCSUM)
			ifp->if_capenable ^= IFCAP_TXCSUM;
		if (mask & IFCAP_TXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_TXCSUM_IPV6;
		if (mask & IFCAP_TSO4)
			ifp->if_capenable ^= IFCAP_TSO4;
		if (mask & IFCAP_TSO6)
			ifp->if_capenable ^= IFCAP_TSO6;

		if (mask & (IFCAP_RXCSUM | IFCAP_RXCSUM_IPV6 | IFCAP_LRO |
		    IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_HWFILTER)) {
			/* Changing these features requires us to reinit. */
			reinit = 1;

			if (mask & IFCAP_RXCSUM)
				ifp->if_capenable ^= IFCAP_RXCSUM;
			if (mask & IFCAP_RXCSUM_IPV6)
				ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
			if (mask & IFCAP_LRO)
				ifp->if_capenable ^= IFCAP_LRO;
			if (mask & IFCAP_VLAN_HWTAGGING)
				ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			if (mask & IFCAP_VLAN_HWFILTER)
				ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
		} else
			reinit = 0;

		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;

		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
			ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			vmxnet3_init_locked(sc);
		}

		VMXNET3_CORE_UNLOCK(sc);
		VLAN_CAPABILITIES(ifp);
		break;

	default:
		error = ether_ioctl(ifp, cmd, data);
		break;
	}

	VMXNET3_CORE_LOCK_ASSERT_NOTOWNED(sc);

	return (error);
}

static int
vmxnet3_watchdog(struct vmxnet3_txqueue *txq)
{
	struct vmxnet3_softc *sc;

	sc = txq->vxtxq_sc;

	VMXNET3_TXQ_LOCK(txq);
	if (txq->vxtxq_watchdog == 0 || --txq->vxtxq_watchdog) {
		VMXNET3_TXQ_UNLOCK(txq);
		return (0);
	}
	VMXNET3_TXQ_UNLOCK(txq);

	if_printf(sc->vmx_ifp, "watchdog timeout on queue %d\n",
	    txq->vxtxq_id);
	return (1);
}

static void
vmxnet3_refresh_stats(struct vmxnet3_softc *sc)
{

	vmxnet3_write_cmd(sc, VMXNET3_CMD_GET_STATS);
}

static void
vmxnet3_tick(void *xsc)
{
	struct vmxnet3_softc *sc;
	struct ifnet *ifp;
	int i, timedout;

	sc = xsc;
	ifp = sc->vmx_ifp;
	timedout = 0;

	VMXNET3_CORE_LOCK_ASSERT(sc);
	vmxnet3_refresh_stats(sc);

	for (i = 0; i < sc->vmx_ntxqueues; i++)
		timedout |= vmxnet3_watchdog(&sc->vmx_txq[i]);

	if (timedout != 0) {
		ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
		vmxnet3_init_locked(sc);
	} else
		callout_reset(&sc->vmx_tick, hz, vmxnet3_tick, sc);
}

static int
vmxnet3_link_is_up(struct vmxnet3_softc *sc)
{
	uint32_t status;

	/* Also update the link speed while here. */
	status = vmxnet3_read_cmd(sc, VMXNET3_CMD_GET_LINK);
	sc->vmx_link_speed = status >> 16;
	return !!(status & 0x1);
}

static void
vmxnet3_link_status(struct vmxnet3_softc *sc)
{
	struct ifnet *ifp;
	int link;

	ifp = sc->vmx_ifp;
	link = vmxnet3_link_is_up(sc);

	if (link != 0 && sc->vmx_link_active == 0) {
		sc->vmx_link_active = 1;
		if_link_state_change(ifp, LINK_STATE_UP);
	} else if (link == 0 && sc->vmx_link_active != 0) {
		sc->vmx_link_active = 0;
		if_link_state_change(ifp, LINK_STATE_DOWN);
	}
}

static void
vmxnet3_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct vmxnet3_softc *sc;

	sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER | IFM_AUTO;
	ifmr->ifm_status = IFM_AVALID;

	VMXNET3_CORE_LOCK(sc);
	if (vmxnet3_link_is_up(sc) != 0)
		ifmr->ifm_status |= IFM_ACTIVE;
	else
		ifmr->ifm_status |= IFM_NONE;
	VMXNET3_CORE_UNLOCK(sc);
}

static int
vmxnet3_media_change(struct ifnet *ifp)
{

	/* Ignore. */
	return (0);
}

static void
vmxnet3_set_lladdr(struct vmxnet3_softc *sc)
{
	uint32_t ml, mh;

	ml  = sc->vmx_lladdr[0];
	ml |= sc->vmx_lladdr[1] << 8;
	ml |= sc->vmx_lladdr[2] << 16;
	ml |= sc->vmx_lladdr[3] << 24;
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_MACL, ml);

	mh  = sc->vmx_lladdr[4];
	mh |= sc->vmx_lladdr[5] << 8;
	vmxnet3_write_bar1(sc, VMXNET3_BAR1_MACH, mh);
}

static void
vmxnet3_get_lladdr(struct vmxnet3_softc *sc)
{
	uint32_t ml, mh;

	ml = vmxnet3_read_cmd(sc, VMXNET3_CMD_GET_MACL);
	mh = vmxnet3_read_cmd(sc, VMXNET3_CMD_GET_MACH);

	sc->vmx_lladdr[0] = ml;
	sc->vmx_lladdr[1] = ml >> 8;
	sc->vmx_lladdr[2] = ml >> 16;
	sc->vmx_lladdr[3] = ml >> 24;
	sc->vmx_lladdr[4] = mh;
	sc->vmx_lladdr[5] = mh >> 8;
}

static void
vmxnet3_setup_txq_sysctl(struct vmxnet3_txqueue *txq,
    struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child)
{
	struct sysctl_oid *node, *txsnode;
	struct sysctl_oid_list *list, *txslist;
	struct vmxnet3_txq_stats *stats;
	struct UPT1_TxStats *txstats;
	char namebuf[16];

	stats = &txq->vxtxq_stats;
	txstats = &txq->vxtxq_ts->stats;

	snprintf(namebuf, sizeof(namebuf), "txq%d", txq->vxtxq_id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf, CTLFLAG_RD,
	    NULL, "Transmit Queue");
	txq->vxtxq_sysctl = list = SYSCTL_CHILDREN(node);

	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "ringfull", CTLFLAG_RD,
	    &stats->vtxrs_full, "Tx ring full");
	SYSCTL_ADD_UQUAD(ctx, list, OID_AUTO, "offload_failed", CTLFLAG_RD,
	    &stats->vtxrs_offload_failed, "Tx checksum offload failed");

	/*
	 * Add statistics reported by the host. These are updated once
	 * per second.
	 */
	txsnode = SYSCTL_ADD_NODE(ctx, list, OID_AUTO, "hstats", CTLFLAG_RD,
	    NULL, "Host Statistics");
	txslist = SYSCTL_CHILDREN(txsnode);
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tso_packets", CTLFLAG_RD,
	    &txstats->TSO_packets, "TSO packets");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tso_bytes", CTLFLAG_RD,
	    &txstats->TSO_bytes, "TSO bytes");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "ucast_packets", CTLFLAG_RD,
	    &txstats->ucast_packets, "Unicast packets");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "unicast_bytes", CTLFLAG_RD,
	    &txstats->ucast_bytes, "Unicast bytes");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "mcast_packets", CTLFLAG_RD,
	    &txstats->mcast_packets, "Multicast packets");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "mcast_bytes", CTLFLAG_RD,
	    &txstats->mcast_bytes, "Multicast bytes");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "error", CTLFLAG_RD,
	    &txstats->error, "Errors");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "discard", CTLFLAG_RD,
	    &txstats->discard, "Discards");
}

static void
vmxnet3_setup_rxq_sysctl(struct vmxnet3_rxqueue *rxq,
    struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child)
{
	struct sysctl_oid *node, *rxsnode;
	struct sysctl_oid_list *list, *rxslist;
	struct vmxnet3_rxq_stats *stats;
	struct UPT1_RxStats *rxstats;
	char namebuf[16];

	stats = &rxq->vxrxq_stats;
	rxstats = &rxq->vxrxq_rs->stats;

	snprintf(namebuf, sizeof(namebuf), "rxq%d", rxq->vxrxq_id);
	node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf, CTLFLAG_RD,
	    NULL, "Receive Queue");
	rxq->vxrxq_sysctl = list = SYSCTL_CHILDREN(node);

	/*
	 * Add statistics reported by the host. These are updated once
	 * per second.
	 */
	rxsnode = SYSCTL_ADD_NODE(ctx, list, OID_AUTO, "hstats", CTLFLAG_RD,
	    NULL, "Host Statistics");
	rxslist = SYSCTL_CHILDREN(rxsnode);
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "lro_packets", CTLFLAG_RD,
	    &rxstats->LRO_packets, "LRO packets");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "lro_bytes", CTLFLAG_RD,
	    &rxstats->LRO_bytes, "LRO bytes");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "ucast_packets", CTLFLAG_RD,
	    &rxstats->ucast_packets, "Unicast packets");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "unicast_bytes", CTLFLAG_RD,
	    &rxstats->ucast_bytes, "Unicast bytes");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "mcast_packets", CTLFLAG_RD,
	    &rxstats->mcast_packets, "Multicast packets");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "mcast_bytes", CTLFLAG_RD,
	    &rxstats->mcast_bytes, "Multicast bytes");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "bcast_packets", CTLFLAG_RD,
	    &rxstats->bcast_packets, "Broadcast packets");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "bcast_bytes", CTLFLAG_RD,
	    &rxstats->bcast_bytes, "Broadcast bytes");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "nobuffer", CTLFLAG_RD,
	    &rxstats->nobuffer, "No buffer");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "error", CTLFLAG_RD,
	    &rxstats->error, "Errors");
}

#ifdef VMXNET3_DEBUG_SYSCTL
static void
vmxnet3_setup_debug_sysctl(struct vmxnet3_softc *sc,
    struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child)
{
	struct sysctl_oid *node;
	struct sysctl_oid_list *list;
	int i;

	for (i = 0; i < sc->vmx_ntxqueues; i++) {
		struct vmxnet3_txqueue *txq = &sc->vmx_txq[i];

		node = SYSCTL_ADD_NODE(ctx, txq->vxtxq_sysctl, OID_AUTO,
		    "debug", CTLFLAG_RD, NULL, "");
		list = SYSCTL_CHILDREN(node);

		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd_head", CTLFLAG_RD,
		    &txq->vxtxq_cmd_ring.vxtxr_head, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd_next", CTLFLAG_RD,
		    &txq->vxtxq_cmd_ring.vxtxr_next, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd_ndesc", CTLFLAG_RD,
		    &txq->vxtxq_cmd_ring.vxtxr_ndesc, 0, "");
		SYSCTL_ADD_INT(ctx, list, OID_AUTO, "cmd_gen", CTLFLAG_RD,
		    &txq->vxtxq_cmd_ring.vxtxr_gen, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "comp_next", CTLFLAG_RD,
		    &txq->vxtxq_comp_ring.vxcr_next, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "comp_ndesc", CTLFLAG_RD,
		    &txq->vxtxq_comp_ring.vxcr_ndesc, 0,"");
		SYSCTL_ADD_INT(ctx, list, OID_AUTO, "comp_gen", CTLFLAG_RD,
		    &txq->vxtxq_comp_ring.vxcr_gen, 0, "");
	}

	for (i = 0; i < sc->vmx_nrxqueues; i++) {
		struct vmxnet3_rxqueue *rxq = &sc->vmx_rxq[i];

		node = SYSCTL_ADD_NODE(ctx, rxq->vxrxq_sysctl, OID_AUTO,
		    "debug", CTLFLAG_RD, NULL, "");
		list = SYSCTL_CHILDREN(node);

		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd0_fill", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[0].vxrxr_fill, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd0_ndesc", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[0].vxrxr_ndesc, 0, "");
		SYSCTL_ADD_INT(ctx, list, OID_AUTO, "cmd0_gen", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[0].vxrxr_gen, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd1_fill", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[1].vxrxr_fill, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "cmd1_ndesc", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[1].vxrxr_ndesc, 0, "");
		SYSCTL_ADD_INT(ctx, list, OID_AUTO, "cmd1_gen", CTLFLAG_RD,
		    &rxq->vxrxq_cmd_ring[1].vxrxr_gen, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "comp_next", CTLFLAG_RD,
		    &rxq->vxrxq_comp_ring.vxcr_next, 0, "");
		SYSCTL_ADD_UINT(ctx, list, OID_AUTO, "comp_ndesc", CTLFLAG_RD,
		    &rxq->vxrxq_comp_ring.vxcr_ndesc, 0,"");
		SYSCTL_ADD_INT(ctx, list, OID_AUTO, "comp_gen", CTLFLAG_RD,
		    &rxq->vxrxq_comp_ring.vxcr_gen, 0, "");
	}
}
#endif

static void
vmxnet3_setup_queue_sysctl(struct vmxnet3_softc *sc,
    struct sysctl_ctx_list *ctx, struct sysctl_oid_list *child)
{
	int i;

	for (i = 0; i < sc->vmx_ntxqueues; i++)
		vmxnet3_setup_txq_sysctl(&sc->vmx_txq[i], ctx, child);
	for (i = 0; i < sc->vmx_nrxqueues; i++)
		vmxnet3_setup_rxq_sysctl(&sc->vmx_rxq[i], ctx, child);

#ifdef VMXNET3_DEBUG_SYSCTL
	vmxnet3_setup_debug_sysctl(sc, ctx, child);
#endif
}

static void
vmxnet3_setup_sysctl(struct vmxnet3_softc *sc)
{
	device_t dev;
	struct vmxnet3_statistics *stats;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = sc->vmx_dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "ntxqueues", CTLFLAG_RD,
	    &sc->vmx_ntxqueues, 0, "Number of Tx queues");
	SYSCTL_ADD_INT(ctx, child, OID_AUTO, "nrxqueues", CTLFLAG_RD,
	    &sc->vmx_nrxqueues, 0, "Number of Rx queues");

	stats = &sc->vmx_stats;
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "collapsed", CTLFLAG_RD,
	    &stats->vmst_collapsed, 0, "Tx mbuf chains collapsed");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "mgetcl_failed", CTLFLAG_RD,
	    &stats->vmst_mgetcl_failed, 0, "mbuf cluster allocation failed");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "mbuf_load_failed", CTLFLAG_RD,
	    &stats->vmst_mbuf_load_failed, 0, "mbuf load segments failed");

	vmxnet3_setup_queue_sysctl(sc, ctx, child);
}

static void
vmxnet3_write_bar0(struct vmxnet3_softc *sc, bus_size_t r, uint32_t v)
{

	bus_space_write_4(sc->vmx_iot0, sc->vmx_ioh0, r, v);
}

static uint32_t
vmxnet3_read_bar1(struct vmxnet3_softc *sc, bus_size_t r)
{

	return (bus_space_read_4(sc->vmx_iot1, sc->vmx_ioh1, r));
}

static void
vmxnet3_write_bar1(struct vmxnet3_softc *sc, bus_size_t r, uint32_t v)
{

	bus_space_write_4(sc->vmx_iot1, sc->vmx_ioh1, r, v);
}

static void
vmxnet3_write_cmd(struct vmxnet3_softc *sc, uint32_t cmd)
{

	vmxnet3_write_bar1(sc, VMXNET3_BAR1_CMD, cmd);
}

static uint32_t
vmxnet3_read_cmd(struct vmxnet3_softc *sc, uint32_t cmd)
{

	vmxnet3_write_cmd(sc, cmd);
	bus_space_barrier(sc->vmx_iot1, sc->vmx_ioh1, 0, 0,
	    BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE);
	return (vmxnet3_read_bar1(sc, VMXNET3_BAR1_CMD));
}

static void
vmxnet3_enable_intr(struct vmxnet3_softc *sc, int irq)
{

	vmxnet3_write_bar0(sc, VMXNET3_BAR0_IMASK(irq), 0);
}

static void
vmxnet3_disable_intr(struct vmxnet3_softc *sc, int irq)
{

	vmxnet3_write_bar0(sc, VMXNET3_BAR0_IMASK(irq), 1);
}

static void
vmxnet3_enable_all_intrs(struct vmxnet3_softc *sc)
{
	int i;

	sc->vmx_ds->ictrl &= ~VMXNET3_ICTRL_DISABLE_ALL;
	for (i = 0; i < sc->vmx_nintrs; i++)
		vmxnet3_enable_intr(sc, i);
}

static void
vmxnet3_disable_all_intrs(struct vmxnet3_softc *sc)
{
	int i;

	sc->vmx_ds->ictrl |= VMXNET3_ICTRL_DISABLE_ALL;
	for (i = 0; i < sc->vmx_nintrs; i++)
		vmxnet3_disable_intr(sc, i);
}

static void
vmxnet3_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	bus_addr_t *baddr = arg;

	if (error == 0)
		*baddr = segs->ds_addr;
}

static int
vmxnet3_dma_malloc(struct vmxnet3_softc *sc, bus_size_t size, bus_size_t align,
    struct vmxnet3_dma_alloc *dma)
{
	device_t dev;
	int error;

	dev = sc->vmx_dev;
	bzero(dma, sizeof(struct vmxnet3_dma_alloc));

	error = bus_dma_tag_create(bus_get_dma_tag(dev),
	    align, 0,		/* alignment, bounds */
	    BUS_SPACE_MAXADDR,	/* lowaddr */
	    BUS_SPACE_MAXADDR,	/* highaddr */
	    NULL, NULL,		/* filter, filterarg */
	    size,		/* maxsize */
	    1,			/* nsegments */
	    size,		/* maxsegsize */
	    BUS_DMA_ALLOCNOW,	/* flags */
	    NULL,		/* lockfunc */
	    NULL,		/* lockfuncarg */
	    &dma->dma_tag);
	if (error) {
		device_printf(dev, "bus_dma_tag_create failed: %d\n", error);
		goto fail;
	}

	error = bus_dmamem_alloc(dma->dma_tag, (void **)&dma->dma_vaddr,
	    BUS_DMA_ZERO | BUS_DMA_NOWAIT, &dma->dma_map);
	if (error) {
		device_printf(dev, "bus_dmamem_alloc failed: %d\n", error);
		goto fail;
	}

	error = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
	    size, vmxnet3_dmamap_cb, &dma->dma_paddr, BUS_DMA_NOWAIT);
	if (error) {
		device_printf(dev, "bus_dmamap_load failed: %d\n", error);
		goto fail;
	}

	dma->dma_size = size;

fail:
	if (error)
		vmxnet3_dma_free(sc, dma);

	return (error);
}

static void
vmxnet3_dma_free(struct vmxnet3_softc *sc, struct vmxnet3_dma_alloc *dma)
{

	if (dma->dma_tag != NULL) {
		if (dma->dma_map != NULL) {
			bus_dmamap_sync(dma->dma_tag, dma->dma_map,
			    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		}

		if (dma->dma_vaddr != NULL) {
			bus_dmamem_free(dma->dma_tag, dma->dma_vaddr,
			    dma->dma_map);
		}

		bus_dma_tag_destroy(dma->dma_tag);
	}
	bzero(dma, sizeof(struct vmxnet3_dma_alloc));
}

static int
vmxnet3_tunable_int(struct vmxnet3_softc *sc, const char *knob, int def)
{
	char path[64];

	snprintf(path, sizeof(path),
	    "hw.vmx.%d.%s", device_get_unit(sc->vmx_dev), knob);
	TUNABLE_INT_FETCH(path, &def);

	return (def);
}

/*
 * Since this is a purely paravirtualized device, we do not have
 * to worry about DMA coherency. But at times, we must make sure
 * both the compiler and CPU do not reorder memory operations.
 */
static inline void
vmxnet3_barrier(struct vmxnet3_softc *sc, vmxnet3_barrier_t type)
{

	switch (type) {
	case VMXNET3_BARRIER_RD:
		rmb();
		break;
	case VMXNET3_BARRIER_WR:
		wmb();
		break;
	case VMXNET3_BARRIER_RDWR:
		mb();
		break;
	default:
		panic("%s: bad barrier type %d", __func__, type);
	}
}
