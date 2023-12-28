/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#include "opt_rss.h"

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
#include <sys/smp.h>
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
#include <net/iflib.h>
#ifdef RSS
#include <net/rss_config.h>
#endif

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

#include "ifdi_if.h"
#include "enic.h"

#include "opt_inet.h"
#include "opt_inet6.h"

static SYSCTL_NODE(_hw, OID_AUTO, enic, CTLFLAG_RW | CTLFLAG_MPSAFE, 0,
    "ENIC");

static const pci_vendor_info_t enic_vendor_info_array[] =
{
	PVID(CISCO_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET,
	     DRV_DESCRIPTION),
		PVID(CISCO_VENDOR_ID, PCI_DEVICE_ID_CISCO_VIC_ENET_VF,
		     DRV_DESCRIPTION " VF"),
	/* required last entry */

		PVID_END
};

static void *enic_register(device_t);
static int enic_attach_pre(if_ctx_t);
static int enic_msix_intr_assign(if_ctx_t, int);

static int enic_attach_post(if_ctx_t);
static int enic_detach(if_ctx_t);

static int enic_tx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static int enic_rx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int, int);
static void enic_queues_free(if_ctx_t);
static int enic_rxq_intr(void *);
static int enic_event_intr(void *);
static int enic_err_intr(void *);
static void enic_stop(if_ctx_t);
static void enic_init(if_ctx_t);
static void enic_multi_set(if_ctx_t);
static int enic_mtu_set(if_ctx_t, uint32_t);
static void enic_media_status(if_ctx_t, struct ifmediareq *);
static int enic_media_change(if_ctx_t);
static int enic_promisc_set(if_ctx_t, int);
static uint64_t enic_get_counter(if_ctx_t, ift_counter);
static void enic_update_admin_status(if_ctx_t);
static void enic_txq_timer(if_ctx_t, uint16_t);
static int enic_link_is_up(struct enic_softc *);
static void enic_link_status(struct enic_softc *);
static void enic_set_lladdr(struct enic_softc *);
static void enic_setup_txq_sysctl(struct vnic_wq *, int, struct sysctl_ctx_list *,
    struct sysctl_oid_list *);
static void enic_setup_rxq_sysctl(struct vnic_rq *, int,  struct sysctl_ctx_list *,
    struct sysctl_oid_list *);
static void enic_setup_sysctl(struct enic_softc *);
static int enic_tx_queue_intr_enable(if_ctx_t, uint16_t);
static int enic_rx_queue_intr_enable(if_ctx_t, uint16_t);
static void enic_enable_intr(struct enic_softc *, int);
static void enic_disable_intr(struct enic_softc *, int);
static void enic_intr_enable_all(if_ctx_t);
static void enic_intr_disable_all(if_ctx_t);
static int enic_dev_open(struct enic *);
static int enic_dev_init(struct enic *);
static void *enic_alloc_consistent(void *, size_t, bus_addr_t *,
    struct iflib_dma_info *, u8 *);
static void enic_free_consistent(void *, size_t, void *, bus_addr_t,
    struct iflib_dma_info *);
static int enic_pci_mapping(struct enic_softc *);
static void enic_pci_mapping_free(struct enic_softc *);
static int enic_dev_wait(struct vnic_dev *, int (*) (struct vnic_dev *, int),
    int (*) (struct vnic_dev *, int *), int arg);
static int enic_map_bar(struct enic_softc *, struct enic_bar_info *, int, bool);
static void enic_update_packet_filter(struct enic *enic);
static bool enic_if_needs_restart(if_ctx_t, enum iflib_restart_event);

typedef enum {
	ENIC_BARRIER_RD,
	ENIC_BARRIER_WR,
	ENIC_BARRIER_RDWR,
} enic_barrier_t;

static device_method_t enic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, enic_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend, iflib_device_suspend),
	DEVMETHOD(device_resume, iflib_device_resume),
	DEVMETHOD_END
};

static driver_t enic_driver = {
	"enic", enic_methods, sizeof(struct enic_softc)
};

DRIVER_MODULE(enic, pci, enic_driver, 0, 0);
IFLIB_PNP_INFO(pci, enic, enic_vendor_info_array);
MODULE_VERSION(enic, 2);

MODULE_DEPEND(enic, pci, 1, 1, 1);
MODULE_DEPEND(enic, ether, 1, 1, 1);
MODULE_DEPEND(enic, iflib, 1, 1, 1);

static device_method_t enic_iflib_methods[] = {
	DEVMETHOD(ifdi_tx_queues_alloc, enic_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, enic_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, enic_queues_free),

	DEVMETHOD(ifdi_attach_pre, enic_attach_pre),
	DEVMETHOD(ifdi_attach_post, enic_attach_post),
	DEVMETHOD(ifdi_detach, enic_detach),

	DEVMETHOD(ifdi_init, enic_init),
	DEVMETHOD(ifdi_stop, enic_stop),
	DEVMETHOD(ifdi_multi_set, enic_multi_set),
	DEVMETHOD(ifdi_mtu_set, enic_mtu_set),
	DEVMETHOD(ifdi_media_status, enic_media_status),
	DEVMETHOD(ifdi_media_change, enic_media_change),
	DEVMETHOD(ifdi_promisc_set, enic_promisc_set),
	DEVMETHOD(ifdi_get_counter, enic_get_counter),
	DEVMETHOD(ifdi_update_admin_status, enic_update_admin_status),
	DEVMETHOD(ifdi_timer, enic_txq_timer),

	DEVMETHOD(ifdi_tx_queue_intr_enable, enic_tx_queue_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, enic_rx_queue_intr_enable),
	DEVMETHOD(ifdi_intr_enable, enic_intr_enable_all),
	DEVMETHOD(ifdi_intr_disable, enic_intr_disable_all),
	DEVMETHOD(ifdi_msix_intr_assign, enic_msix_intr_assign),

	DEVMETHOD(ifdi_needs_restart, enic_if_needs_restart),

	DEVMETHOD_END
};

static driver_t enic_iflib_driver = {
	"enic", enic_iflib_methods, sizeof(struct enic_softc)
};

extern struct if_txrx enic_txrx;

static struct if_shared_ctx enic_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = 512,

	.isc_tx_maxsize = ENIC_TX_MAX_PKT_SIZE,
	.isc_tx_maxsegsize = PAGE_SIZE,

	/*
	 * These values are used to configure the busdma tag used for receive
	 * descriptors.  Each receive descriptor only points to one buffer.
	 */
	.isc_rx_maxsize = ENIC_DEFAULT_RX_MAX_PKT_SIZE,	/* One buf per
							 * descriptor */
	.isc_rx_nsegments = 1,	/* One mapping per descriptor */
	.isc_rx_maxsegsize = ENIC_DEFAULT_RX_MAX_PKT_SIZE,
	.isc_admin_intrcnt = 3,
	.isc_vendor_info = enic_vendor_info_array,
	.isc_driver_version = "1",
	.isc_driver = &enic_iflib_driver,
	.isc_flags = IFLIB_HAS_RXCQ | IFLIB_HAS_TXCQ,

	/*
	 * Number of receive queues per receive queue set, with associated
	 * descriptor settings for each.
	 */

	.isc_nrxqs = 2,
	.isc_nfl = 1,		/* one free list for each receive command
				 * queue */
	.isc_nrxd_min = {16, 16},
	.isc_nrxd_max = {2048, 2048},
	.isc_nrxd_default = {64, 64},

	/*
	 * Number of transmit queues per transmit queue set, with associated
	 * descriptor settings for each.
	 */
	.isc_ntxqs = 2,
	.isc_ntxd_min = {16, 16},
	.isc_ntxd_max = {2048, 2048},
	.isc_ntxd_default = {64, 64},
};

static void *
enic_register(device_t dev)
{
	return (&enic_sctx_init);
}

static int
enic_attach_pre(if_ctx_t ctx)
{
	if_softc_ctx_t	scctx;
	struct enic_softc *softc;
	struct vnic_dev *vdev;
	struct enic *enic;
	device_t dev;

	int err = -1;
	int rc = 0;
	int i;
	u64 a0 = 0, a1 = 0;
	int wait = 1000;
	struct vnic_stats *stats;
	int ret;

	dev = iflib_get_dev(ctx);
	softc = iflib_get_softc(ctx);
	softc->dev = dev;
	softc->ctx = ctx;
	softc->sctx = iflib_get_sctx(ctx);
	softc->scctx = iflib_get_softc_ctx(ctx);
	softc->ifp = iflib_get_ifp(ctx);
	softc->media = iflib_get_media(ctx);
	softc->mta = malloc(sizeof(u8) * ETHER_ADDR_LEN *
		ENIC_MAX_MULTICAST_ADDRESSES, M_DEVBUF,
		     M_NOWAIT | M_ZERO);
	if (softc->mta == NULL)
		return (ENOMEM);
	scctx = softc->scctx;

	mtx_init(&softc->enic_lock, "ENIC Lock", NULL, MTX_DEF);

	pci_enable_busmaster(softc->dev);
	if (enic_pci_mapping(softc))
		return (ENXIO);

	enic = &softc->enic;
	enic->softc = softc;
	vdev = &softc->vdev;
	vdev->softc = softc;
	enic->vdev = vdev;
	vdev->priv = enic;

	ENIC_LOCK(softc);
	vnic_dev_register(vdev, &softc->mem, 1);
	enic->vdev = vdev;
	vdev->devcmd = vnic_dev_get_res(vdev, RES_TYPE_DEVCMD, 0);

	vnic_dev_cmd(vdev, CMD_INIT_v1, &a0, &a1, wait);
	vnic_dev_cmd(vdev, CMD_GET_MAC_ADDR, &a0, &a1, wait);

	bcopy((u_int8_t *) & a0, softc->mac_addr, ETHER_ADDR_LEN);
	iflib_set_mac(ctx, softc->mac_addr);

	vnic_register_cbacks(enic->vdev, enic_alloc_consistent,
	    enic_free_consistent);

	/*
	 * Allocate the consistent memory for stats and counters upfront so
	 * both primary and secondary processes can access them.
	 */
	ENIC_UNLOCK(softc);
	err = vnic_dev_alloc_stats_mem(enic->vdev);
	ENIC_LOCK(softc);
	if (err) {
		dev_err(enic, "Failed to allocate cmd memory, aborting\n");
		goto err_out_unregister;
	}
	vnic_dev_stats_clear(enic->vdev);
	ret = vnic_dev_stats_dump(enic->vdev, &stats);
	if (ret) {
		dev_err(enic, "Error in getting stats\n");
		goto err_out_unregister;
	}
	err = vnic_dev_alloc_counter_mem(enic->vdev);
	if (err) {
		dev_err(enic, "Failed to allocate counter memory, aborting\n");
		goto err_out_unregister;
	}

	/* Issue device open to get device in known state */
	err = enic_dev_open(enic);
	if (err) {
		dev_err(enic, "vNIC dev open failed, aborting\n");
		goto err_out_unregister;
	}

	/* Set ingress vlan rewrite mode before vnic initialization */
	enic->ig_vlan_rewrite_mode = IG_VLAN_REWRITE_MODE_UNTAG_DEFAULT_VLAN;
	err = vnic_dev_set_ig_vlan_rewrite_mode(enic->vdev,
						enic->ig_vlan_rewrite_mode);
	if (err) {
		dev_err(enic,
		    "Failed to set ingress vlan rewrite mode, aborting.\n");
		goto err_out_dev_close;
	}

	/*
	 * Issue device init to initialize the vnic-to-switch link. We'll
	 * start with carrier off and wait for link UP notification later to
	 * turn on carrier.  We don't need to wait here for the
	 * vnic-to-switch link initialization to complete; link UP
	 * notification is the indication that the process is complete.
	 */

	err = vnic_dev_init(enic->vdev, 0);
	if (err) {
		dev_err(enic, "vNIC dev init failed, aborting\n");
		goto err_out_dev_close;
	}

	err = enic_dev_init(enic);
	if (err) {
		dev_err(enic, "Device initialization failed, aborting\n");
		goto err_out_dev_close;
	}
	ENIC_UNLOCK(softc);

	enic->port_mtu = vnic_dev_mtu(enic->vdev);

	softc->scctx = iflib_get_softc_ctx(ctx);
	scctx = softc->scctx;
	scctx->isc_txrx = &enic_txrx;
	scctx->isc_capabilities = scctx->isc_capenable = 0;
	scctx->isc_tx_csum_flags = 0;
	scctx->isc_max_frame_size = enic->config.mtu + ETHER_HDR_LEN + \
		ETHER_CRC_LEN;
	scctx->isc_nrxqsets_max = enic->conf_rq_count;
	scctx->isc_ntxqsets_max = enic->conf_wq_count;
	scctx->isc_nrxqsets = enic->conf_rq_count;
	scctx->isc_ntxqsets = enic->conf_wq_count;
	for (i = 0; i < enic->conf_wq_count; i++) {
		scctx->isc_ntxd[i] = enic->config.wq_desc_count;
		scctx->isc_txqsizes[i] = sizeof(struct cq_enet_wq_desc)
			* scctx->isc_ntxd[i];
		scctx->isc_ntxd[i + enic->conf_wq_count] =
		    enic->config.wq_desc_count;
		scctx->isc_txqsizes[i + enic->conf_wq_count] =
		    sizeof(struct cq_desc) * scctx->isc_ntxd[i +
		    enic->conf_wq_count];
	}
	for (i = 0; i < enic->conf_rq_count; i++) {
		scctx->isc_nrxd[i] = enic->config.rq_desc_count;
		scctx->isc_rxqsizes[i] = sizeof(struct cq_enet_rq_desc) *
		    scctx->isc_nrxd[i];
		scctx->isc_nrxd[i + enic->conf_rq_count] =
		    enic->config.rq_desc_count;
		scctx->isc_rxqsizes[i + enic->conf_rq_count] = sizeof(struct
		    cq_desc) * scctx->isc_nrxd[i + enic->conf_rq_count];
	}
	scctx->isc_tx_nsegments = 31;

	scctx->isc_vectors = enic->conf_cq_count;
	scctx->isc_msix_bar = -1;

	ifmedia_add(softc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_add(softc->media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	ifmedia_add(softc->media, IFM_ETHER | IFM_10_FL, 0, NULL);

	/*
	 * Allocate the CQ here since TX is called first before RX for now
	 * assume RX and TX are the same
	 */
	if (softc->enic.cq == NULL)
		softc->enic.cq = malloc(sizeof(struct vnic_cq) *
		     softc->enic.wq_count + softc->enic.rq_count, M_DEVBUF,
		     M_NOWAIT | M_ZERO);
	if (softc->enic.cq == NULL)
		return (ENOMEM);

	softc->enic.cq->ntxqsets = softc->enic.wq_count + softc->enic.rq_count;

	/*
	 * Allocate the consistent memory for stats and counters upfront so
	 * both primary and secondary processes can access them.
	 */
	err = vnic_dev_alloc_stats_mem(enic->vdev);
	if (err) {
		dev_err(enic, "Failed to allocate cmd memory, aborting\n");
	}

	return (rc);

err_out_dev_close:
	vnic_dev_close(enic->vdev);
err_out_unregister:
	free(softc->vdev.devcmd, M_DEVBUF);
	free(softc->enic.intr_queues, M_DEVBUF);
	free(softc->enic.cq, M_DEVBUF);
	free(softc->mta, M_DEVBUF);
	rc = -1;
	pci_disable_busmaster(softc->dev);
	enic_pci_mapping_free(softc);
	mtx_destroy(&softc->enic_lock);
	return (rc);
}

static int
enic_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct enic_softc *softc;
	struct enic *enic;
	if_softc_ctx_t scctx;

	int error;
	int i;
	char irq_name[16];

	softc = iflib_get_softc(ctx);
	enic = &softc->enic;
	scctx = softc->scctx;

	ENIC_LOCK(softc);
	vnic_dev_set_intr_mode(enic->vdev, VNIC_DEV_INTR_MODE_MSIX);
	ENIC_UNLOCK(softc);

	enic->intr_queues = malloc(sizeof(*enic->intr_queues) *
	    enic->conf_intr_count, M_DEVBUF, M_NOWAIT | M_ZERO);
	enic->intr = malloc(sizeof(*enic->intr) * msix, M_DEVBUF, M_NOWAIT
	    | M_ZERO);
	for (i = 0; i < scctx->isc_nrxqsets; i++) {
		snprintf(irq_name, sizeof(irq_name), "erxq%d:%d", i,
		    device_get_unit(softc->dev));

		error = iflib_irq_alloc_generic(ctx,
		    &enic->intr_queues[i].intr_irq, i + 1, IFLIB_INTR_RX,
		    enic_rxq_intr, &enic->rq[i], i, irq_name);
		if (error) {
			device_printf(iflib_get_dev(ctx),
			    "Failed to register rxq %d interrupt handler\n", i);
			return (error);
		}
		enic->intr[i].index = i;
		enic->intr[i].vdev = enic->vdev;
		ENIC_LOCK(softc);
		enic->intr[i].ctrl = vnic_dev_get_res(enic->vdev,
		    RES_TYPE_INTR_CTRL, i);
		vnic_intr_mask(&enic->intr[i]);
		ENIC_UNLOCK(softc);
	}

	for (i = scctx->isc_nrxqsets; i < scctx->isc_nrxqsets + scctx->isc_ntxqsets; i++) {
		snprintf(irq_name, sizeof(irq_name), "etxq%d:%d", i -
		    scctx->isc_nrxqsets, device_get_unit(softc->dev));


		iflib_softirq_alloc_generic(ctx, &enic->intr_queues[i].intr_irq, IFLIB_INTR_TX, &enic->wq[i - scctx->isc_nrxqsets], i - scctx->isc_nrxqsets, irq_name);


		enic->intr[i].index = i;
		enic->intr[i].vdev = enic->vdev;
		ENIC_LOCK(softc);
		enic->intr[i].ctrl = vnic_dev_get_res(enic->vdev,
		    RES_TYPE_INTR_CTRL, i);
		vnic_intr_mask(&enic->intr[i]);
		ENIC_UNLOCK(softc);
	}

	i = scctx->isc_nrxqsets + scctx->isc_ntxqsets;
	error = iflib_irq_alloc_generic(ctx, &softc->enic_event_intr_irq,
		 i + 1, IFLIB_INTR_ADMIN, enic_event_intr, softc, 0, "event");
	if (error) {
		device_printf(iflib_get_dev(ctx),
		    "Failed to register event interrupt handler\n");
		return (error);
	}

	enic->intr[i].index = i;
	enic->intr[i].vdev = enic->vdev;
	ENIC_LOCK(softc);
	enic->intr[i].ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_INTR_CTRL,
	    i);
	vnic_intr_mask(&enic->intr[i]);
	ENIC_UNLOCK(softc);

	i++;
	error = iflib_irq_alloc_generic(ctx, &softc->enic_err_intr_irq,
		   i + 1, IFLIB_INTR_ADMIN, enic_err_intr, softc, 0, "err");
	if (error) {
		device_printf(iflib_get_dev(ctx),
		    "Failed to register event interrupt handler\n");
		return (error);
	}
	enic->intr[i].index = i;
	enic->intr[i].vdev = enic->vdev;
	ENIC_LOCK(softc);
	enic->intr[i].ctrl = vnic_dev_get_res(enic->vdev, RES_TYPE_INTR_CTRL,
	    i);
	vnic_intr_mask(&enic->intr[i]);
	ENIC_UNLOCK(softc);

	enic->intr_count = msix;

	return (0);
}

static void
enic_free_irqs(struct enic_softc *softc)
{
	if_softc_ctx_t	scctx;

	struct enic    *enic;
	int		i;

	scctx = softc->scctx;
	enic = &softc->enic;

	for (i = 0; i < scctx->isc_nrxqsets + scctx->isc_ntxqsets; i++) {
		iflib_irq_free(softc->ctx, &enic->intr_queues[i].intr_irq);
	}

	iflib_irq_free(softc->ctx, &softc->enic_event_intr_irq);
	iflib_irq_free(softc->ctx, &softc->enic_err_intr_irq);
	free(enic->intr_queues, M_DEVBUF);
	free(enic->intr, M_DEVBUF);
}

static int
enic_attach_post(if_ctx_t ctx)
{
	struct enic *enic;
	struct enic_softc *softc;
	int error = 0;

	softc = iflib_get_softc(ctx);
	enic = &softc->enic;

	enic_setup_sysctl(softc);

	enic_init_vnic_resources(enic);
	enic_setup_finish(enic);

	ifmedia_add(softc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(softc->media, IFM_ETHER | IFM_AUTO);

	return (error);
}

static int
enic_detach(if_ctx_t ctx)
{
	struct enic_softc *softc;
	struct enic *enic;

	softc = iflib_get_softc(ctx);
	enic = &softc->enic;

	vnic_dev_notify_unset(enic->vdev);

	enic_free_irqs(softc);

	ENIC_LOCK(softc);
	vnic_dev_close(enic->vdev);
	free(softc->vdev.devcmd, M_DEVBUF);
	pci_disable_busmaster(softc->dev);
	enic_pci_mapping_free(softc);
	ENIC_UNLOCK(softc);

	return 0;
}

static int
enic_tx_queues_alloc(if_ctx_t ctx, caddr_t * vaddrs, uint64_t * paddrs,
		     int ntxqs, int ntxqsets)
{
	struct enic_softc *softc;
	int q;

	softc = iflib_get_softc(ctx);
	/* Allocate the array of transmit queues */
	softc->enic.wq = malloc(sizeof(struct vnic_wq) *
				ntxqsets, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (softc->enic.wq == NULL)
		return (ENOMEM);

	/* Initialize driver state for each transmit queue */

	/*
	 * Allocate queue state that is shared with the device.  This check
	 * and call is performed in both enic_tx_queues_alloc() and
	 * enic_rx_queues_alloc() so that we don't have to care which order
	 * iflib invokes those routines in.
	 */

	/* Record descriptor ring vaddrs and paddrs */
	ENIC_LOCK(softc);
	for (q = 0; q < ntxqsets; q++) {
		struct vnic_wq *wq;
		struct vnic_cq *cq;
		unsigned int	cq_wq;

		wq = &softc->enic.wq[q];
		cq_wq = enic_cq_wq(&softc->enic, q);
		cq = &softc->enic.cq[cq_wq];

		/* Completion ring */
		wq->vdev = softc->enic.vdev;
		wq->index = q;
		wq->ctrl = vnic_dev_get_res(softc->enic.vdev, RES_TYPE_WQ,
		    wq->index);
		vnic_wq_disable(wq);

		wq->ring.desc_size = sizeof(struct wq_enet_desc);
		wq->ring.desc_count = softc->scctx->isc_ntxd[q];
		wq->ring.desc_avail = wq->ring.desc_count - 1;
		wq->ring.last_count = wq->ring.desc_count;
		wq->head_idx = 0;
		wq->tail_idx = 0;

		wq->ring.size = wq->ring.desc_count * wq->ring.desc_size;
		wq->ring.descs = vaddrs[q * ntxqs + 0];
		wq->ring.base_addr = paddrs[q * ntxqs + 0];

		/* Command ring */
		cq->vdev = softc->enic.vdev;
		cq->index = cq_wq;
		cq->ctrl = vnic_dev_get_res(softc->enic.vdev,
					    RES_TYPE_CQ, cq->index);
		cq->ring.desc_size = sizeof(struct cq_enet_wq_desc);
		cq->ring.desc_count = softc->scctx->isc_ntxd[q];
		cq->ring.desc_avail = cq->ring.desc_count - 1;

		cq->ring.size = cq->ring.desc_count * cq->ring.desc_size;
		cq->ring.descs = vaddrs[q * ntxqs + 1];
		cq->ring.base_addr = paddrs[q * ntxqs + 1];

	}

	ENIC_UNLOCK(softc);

	return (0);
}



static int
enic_rx_queues_alloc(if_ctx_t ctx, caddr_t * vaddrs, uint64_t * paddrs,
		     int nrxqs, int nrxqsets)
{
	struct enic_softc *softc;
	int q;

	softc = iflib_get_softc(ctx);
	/* Allocate the array of receive queues */
	softc->enic.rq = malloc(sizeof(struct vnic_rq) * nrxqsets, M_DEVBUF,
	    M_NOWAIT | M_ZERO);
	if (softc->enic.rq == NULL)
		return (ENOMEM);

	/* Initialize driver state for each receive queue */

	/*
	 * Allocate queue state that is shared with the device.  This check
	 * and call is performed in both enic_tx_queues_alloc() and
	 * enic_rx_queues_alloc() so that we don't have to care which order
	 * iflib invokes those routines in.
	 */

	/* Record descriptor ring vaddrs and paddrs */
	ENIC_LOCK(softc);
	for (q = 0; q < nrxqsets; q++) {
		struct vnic_rq *rq;
		struct vnic_cq *cq;
		unsigned int	cq_rq;

		rq = &softc->enic.rq[q];
		cq_rq = enic_cq_rq(&softc->enic, q);
		cq = &softc->enic.cq[cq_rq];

		/* Completion ring */
		cq->vdev = softc->enic.vdev;
		cq->index = cq_rq;
		cq->ctrl = vnic_dev_get_res(softc->enic.vdev, RES_TYPE_CQ,
		    cq->index);
		cq->ring.desc_size = sizeof(struct cq_enet_wq_desc);
		cq->ring.desc_count = softc->scctx->isc_nrxd[1];
		cq->ring.desc_avail = cq->ring.desc_count - 1;

		cq->ring.size = cq->ring.desc_count * cq->ring.desc_size;
		cq->ring.descs = vaddrs[q * nrxqs + 0];
		cq->ring.base_addr = paddrs[q * nrxqs + 0];

		/* Command ring(s) */
		rq->vdev = softc->enic.vdev;

		rq->index = q;
		rq->ctrl = vnic_dev_get_res(softc->enic.vdev,
					    RES_TYPE_RQ, rq->index);
		vnic_rq_disable(rq);

		rq->ring.desc_size = sizeof(struct rq_enet_desc);
		rq->ring.desc_count = softc->scctx->isc_nrxd[0];
		rq->ring.desc_avail = rq->ring.desc_count - 1;

		rq->ring.size = rq->ring.desc_count * rq->ring.desc_size;
		rq->ring.descs = vaddrs[q * nrxqs + 1];
		rq->ring.base_addr = paddrs[q * nrxqs + 1];
		rq->need_initial_post = true;
	}

	ENIC_UNLOCK(softc);

	return (0);
}

static void
enic_queues_free(if_ctx_t ctx)
{
	struct enic_softc *softc;
	softc = iflib_get_softc(ctx);

	free(softc->enic.rq, M_DEVBUF);
	free(softc->enic.wq, M_DEVBUF);
	free(softc->enic.cq, M_DEVBUF);
}

static int
enic_rxq_intr(void *rxq)
{
	struct vnic_rq *rq;
	if_t ifp;

	rq = (struct vnic_rq *)rxq;
	ifp = iflib_get_ifp(rq->vdev->softc->ctx);
	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		return (FILTER_HANDLED);

	return (FILTER_SCHEDULE_THREAD);
}

static int
enic_event_intr(void *vsc)
{
	struct enic_softc *softc;
	struct enic    *enic;
	uint32_t mtu;

	softc = vsc;
	enic = &softc->enic;

	mtu = vnic_dev_mtu(enic->vdev);
	if (mtu && mtu != enic->port_mtu) {
		enic->port_mtu = mtu;
	}

	enic_link_status(softc);

	return (FILTER_HANDLED);
}

static int
enic_err_intr(void *vsc)
{
	struct enic_softc *softc;

	softc = vsc;

	enic_stop(softc->ctx);
	enic_init(softc->ctx);

	return (FILTER_HANDLED);
}

static void
enic_stop(if_ctx_t ctx)
{
	struct enic_softc *softc;
	struct enic    *enic;
	if_softc_ctx_t	scctx;
	unsigned int	index;

	softc = iflib_get_softc(ctx);
	scctx = softc->scctx;
	enic = &softc->enic;

	if (softc->stopped)
		return;
	softc->link_active = 0;
	softc->stopped = 1;

	for (index = 0; index < scctx->isc_ntxqsets; index++) {
		enic_stop_wq(enic, index);
		vnic_wq_clean(&enic->wq[index]);
		vnic_cq_clean(&enic->cq[enic_cq_rq(enic, index)]);
	}

	for (index = 0; index < scctx->isc_nrxqsets; index++) {
		vnic_rq_clean(&enic->rq[index]);
		vnic_cq_clean(&enic->cq[enic_cq_wq(enic, index)]);
	}

	for (index = 0; index < scctx->isc_vectors; index++) {
		vnic_intr_clean(&enic->intr[index]);
	}
}

static void
enic_init(if_ctx_t ctx)
{
	struct enic_softc *softc;
	struct enic *enic;
	if_softc_ctx_t scctx;
	unsigned int index;

	softc = iflib_get_softc(ctx);
	scctx = softc->scctx;
	enic = &softc->enic;

	for (index = 0; index < scctx->isc_ntxqsets; index++)
		enic_prep_wq_for_simple_tx(&softc->enic, index);

	for (index = 0; index < scctx->isc_ntxqsets; index++)
		enic_start_wq(enic, index);

	for (index = 0; index < scctx->isc_nrxqsets; index++)
		enic_start_rq(enic, index);

	/* Use the current MAC address. */
	bcopy(if_getlladdr(softc->ifp), softc->lladdr, ETHER_ADDR_LEN);
	enic_set_lladdr(softc);

	ENIC_LOCK(softc);
	vnic_dev_enable_wait(enic->vdev);
	ENIC_UNLOCK(softc);

	enic_link_status(softc);
}

static void
enic_del_mcast(struct enic_softc *softc) {
	struct enic *enic;
	int i;

	enic = &softc->enic;
	for (i=0; i < softc->mc_count; i++) {
		vnic_dev_del_addr(enic->vdev, &softc->mta[i * ETHER_ADDR_LEN]);
	}
	softc->multicast = 0;
	softc->mc_count = 0;
}

static void
enic_add_mcast(struct enic_softc *softc) {
	struct enic *enic;
	int i;

	enic = &softc->enic;
	for (i=0; i < softc->mc_count; i++) {
		vnic_dev_add_addr(enic->vdev, &softc->mta[i * ETHER_ADDR_LEN]);
	}
	softc->multicast = 1;
}

static u_int
enic_copy_maddr(void *arg, struct sockaddr_dl *sdl, u_int idx)
{
	uint8_t *mta = arg;

	if (idx == ENIC_MAX_MULTICAST_ADDRESSES)
		return (0);

	bcopy(LLADDR(sdl), &mta[idx * ETHER_ADDR_LEN], ETHER_ADDR_LEN);
	return (1);
}

static void
enic_multi_set(if_ctx_t ctx)
{
	if_t ifp;
	struct enic_softc *softc;
	u_int count;

	softc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);

	ENIC_LOCK(softc);
	enic_del_mcast(softc);
	count = if_foreach_llmaddr(ifp, enic_copy_maddr, softc->mta);
	softc->mc_count = count;
	enic_add_mcast(softc);
	ENIC_UNLOCK(softc);

	if (if_getflags(ifp) & IFF_PROMISC) {
		softc->promisc = 1;
	} else {
		softc->promisc = 0;
	}
	if (if_getflags(ifp) & IFF_ALLMULTI) {
		softc->allmulti = 1;
	} else {
		softc->allmulti = 0;
	}
	enic_update_packet_filter(&softc->enic);
}

static int
enic_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct enic_softc *softc;
	struct enic *enic;
	if_softc_ctx_t scctx = iflib_get_softc_ctx(ctx);

	softc = iflib_get_softc(ctx);
	enic = &softc->enic;

	if (mtu > enic->port_mtu){
		return (EINVAL);
	}

	enic->config.mtu = mtu;
	scctx->isc_max_frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	return (0);
}

static void
enic_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	struct enic_softc *softc;
	struct ifmedia_entry *next;
	uint32_t speed;
	uint64_t target_baudrate;

	softc = iflib_get_softc(ctx);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (enic_link_is_up(softc) != 0) {
		ENIC_LOCK(softc);
		speed = vnic_dev_port_speed(&softc->vdev);
		ENIC_UNLOCK(softc);
		target_baudrate = 1000ull * speed;
		LIST_FOREACH(next, &(iflib_get_media(ctx)->ifm_list), ifm_list) {
			if (ifmedia_baudrate(next->ifm_media) == target_baudrate) {
				ifmr->ifm_active |= next->ifm_media;
			}
		}

		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_AUTO;
	} else
		ifmr->ifm_active |= IFM_NONE;
}

static int
enic_media_change(if_ctx_t ctx)
{
	return (ENODEV);
}

static int
enic_promisc_set(if_ctx_t ctx, int flags)
{
	if_t ifp;
	struct enic_softc *softc;

	softc = iflib_get_softc(ctx);
	ifp = iflib_get_ifp(ctx);

	if (if_getflags(ifp) & IFF_PROMISC) {
		softc->promisc = 1;
	} else {
		softc->promisc = 0;
	}
	if (if_getflags(ifp) & IFF_ALLMULTI) {
		softc->allmulti = 1;
	} else {
		softc->allmulti = 0;
	}
	enic_update_packet_filter(&softc->enic);

	return (0);
}

static uint64_t
enic_get_counter(if_ctx_t ctx, ift_counter cnt) {
	if_t ifp = iflib_get_ifp(ctx);

	if (cnt < IFCOUNTERS)
		return if_get_counter_default(ifp, cnt);

	return (0);
}

static void
enic_update_admin_status(if_ctx_t ctx)
{
	struct enic_softc *softc;

	softc = iflib_get_softc(ctx);

	enic_link_status(softc);
}

static void
enic_txq_timer(if_ctx_t ctx, uint16_t qid)
{

	struct enic_softc *softc;
	struct enic *enic;
	struct vnic_stats *stats;
	int ret;

	softc = iflib_get_softc(ctx);
	enic = &softc->enic;

	ENIC_LOCK(softc);
	ret = vnic_dev_stats_dump(enic->vdev, &stats);
	ENIC_UNLOCK(softc);
	if (ret) {
		dev_err(enic, "Error in getting stats\n");
	}
}

static int
enic_link_is_up(struct enic_softc *softc)
{
	return (vnic_dev_link_status(&softc->vdev) == 1);
}

static void
enic_link_status(struct enic_softc *softc)
{
	if_ctx_t ctx;
	uint64_t speed;
	int link;

	ctx = softc->ctx;
	link = enic_link_is_up(softc);
	speed = IF_Gbps(10);

	ENIC_LOCK(softc);
	speed = vnic_dev_port_speed(&softc->vdev);
	ENIC_UNLOCK(softc);

	if (link != 0 && softc->link_active == 0) {
		softc->link_active = 1;
		iflib_link_state_change(ctx, LINK_STATE_UP, speed);
	} else if (link == 0 && softc->link_active != 0) {
		softc->link_active = 0;
		iflib_link_state_change(ctx, LINK_STATE_DOWN, speed);
	}
}

static void
enic_set_lladdr(struct enic_softc *softc)
{
	struct enic *enic;
	enic = &softc->enic;

	ENIC_LOCK(softc);
	vnic_dev_add_addr(enic->vdev, softc->lladdr);
	ENIC_UNLOCK(softc);
}


static void
enic_setup_txq_sysctl(struct vnic_wq *wq, int i, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child)
{
	struct sysctl_oid *txsnode;
	struct sysctl_oid_list *txslist;
	struct vnic_stats *stats = wq[i].vdev->stats;

	txsnode = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "hstats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Host Statistics");
	txslist = SYSCTL_CHILDREN(txsnode);

	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_frames_ok", CTLFLAG_RD,
	   &stats->tx.tx_frames_ok, "TX Frames OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_unicast_frames_ok", CTLFLAG_RD,
	   &stats->tx.tx_unicast_frames_ok, "TX unicast frames OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_multicast_frames_ok", CTLFLAG_RD,
	    &stats->tx.tx_multicast_frames_ok, "TX multicast framse OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_broadcast_frames_ok", CTLFLAG_RD,
	    &stats->tx.tx_broadcast_frames_ok, "TX Broadcast frames OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_bytes_ok", CTLFLAG_RD,
	    &stats->tx.tx_bytes_ok, "TX bytes OK ");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_unicast_bytes_ok", CTLFLAG_RD,
	    &stats->tx.tx_unicast_bytes_ok, "TX unicast bytes OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_multicast_bytes_ok", CTLFLAG_RD,
	    &stats->tx.tx_multicast_bytes_ok, "TX multicast bytes OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_broadcast_bytes_ok", CTLFLAG_RD,
	    &stats->tx.tx_broadcast_bytes_ok, "TX broadcast bytes OK");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_drops", CTLFLAG_RD,
	    &stats->tx.tx_drops, "TX drops");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_errors", CTLFLAG_RD,
	    &stats->tx.tx_errors, "TX errors");
	SYSCTL_ADD_UQUAD(ctx, txslist, OID_AUTO, "tx_tso", CTLFLAG_RD,
	    &stats->tx.tx_tso, "TX TSO");
}

static void
enic_setup_rxq_sysctl(struct vnic_rq *rq, int i, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child)
{
	struct sysctl_oid *rxsnode;
	struct sysctl_oid_list *rxslist;
	struct vnic_stats *stats = rq[i].vdev->stats;

	rxsnode = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "hstats",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Host Statistics");
	rxslist = SYSCTL_CHILDREN(rxsnode);

	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_ok", CTLFLAG_RD,
	    &stats->rx.rx_frames_ok, "RX Frames OK");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_total", CTLFLAG_RD,
	    &stats->rx.rx_frames_total, "RX frames total");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_unicast_frames_ok", CTLFLAG_RD,
	    &stats->rx.rx_unicast_frames_ok, "RX unicast frames ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_multicast_frames_ok", CTLFLAG_RD,
	    &stats->rx.rx_multicast_frames_ok, "RX multicast Frames ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_broadcast_frames_ok", CTLFLAG_RD,
	    &stats->rx.rx_broadcast_frames_ok, "RX broadcast frames ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_bytes_ok", CTLFLAG_RD,
	    &stats->rx.rx_bytes_ok, "RX bytes ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_unicast_bytes_ok", CTLFLAG_RD,
	    &stats->rx.rx_unicast_bytes_ok, "RX unicast bytes ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_multicast_bytes_ok", CTLFLAG_RD,
	    &stats->rx.rx_multicast_bytes_ok, "RX multicast bytes ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_broadcast_bytes_ok", CTLFLAG_RD,
	    &stats->rx.rx_broadcast_bytes_ok, "RX broadcast bytes ok");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_drop", CTLFLAG_RD,
	    &stats->rx.rx_drop, "RX drop");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_errors", CTLFLAG_RD,
	    &stats->rx.rx_errors, "RX errors");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_rss", CTLFLAG_RD,
	    &stats->rx.rx_rss, "RX rss");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_crc_errors", CTLFLAG_RD,
	    &stats->rx.rx_crc_errors, "RX crc errors");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_64", CTLFLAG_RD,
	    &stats->rx.rx_frames_64, "RX frames 64");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_127", CTLFLAG_RD,
	    &stats->rx.rx_frames_127, "RX frames 127");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_255", CTLFLAG_RD,
	    &stats->rx.rx_frames_255, "RX frames 255");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_511", CTLFLAG_RD,
	    &stats->rx.rx_frames_511, "RX frames 511");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_1023", CTLFLAG_RD,
	    &stats->rx.rx_frames_1023, "RX frames 1023");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_1518", CTLFLAG_RD,
	    &stats->rx.rx_frames_1518, "RX frames 1518");
	SYSCTL_ADD_UQUAD(ctx, rxslist, OID_AUTO, "rx_frames_to_max", CTLFLAG_RD,
	    &stats->rx.rx_frames_to_max, "RX frames to max");
}

static void
enic_setup_queue_sysctl(struct enic_softc *softc, struct sysctl_ctx_list *ctx,
    struct sysctl_oid_list *child)
{
	enic_setup_txq_sysctl(softc->enic.wq, 0, ctx, child);
	enic_setup_rxq_sysctl(softc->enic.rq, 0, ctx, child);
}

static void
enic_setup_sysctl(struct enic_softc *softc)
{
	device_t dev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;

	dev = softc->dev;
	ctx = device_get_sysctl_ctx(dev);
	tree = device_get_sysctl_tree(dev);
	child = SYSCTL_CHILDREN(tree);

	enic_setup_queue_sysctl(softc, ctx, child);
}

static void
enic_enable_intr(struct enic_softc *softc, int irq)
{
	struct enic *enic = &softc->enic;

	vnic_intr_unmask(&enic->intr[irq]);
	vnic_intr_return_all_credits(&enic->intr[irq]);
}

static void
enic_disable_intr(struct enic_softc *softc, int irq)
{
	struct enic *enic = &softc->enic;

	vnic_intr_mask(&enic->intr[irq]);
	vnic_intr_masked(&enic->intr[irq]);	/* flush write */
}

static int
enic_tx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct enic_softc *softc;
	if_softc_ctx_t scctx;

	softc = iflib_get_softc(ctx);
	scctx = softc->scctx;

	enic_enable_intr(softc, qid + scctx->isc_nrxqsets);

	return 0;
}

static int
enic_rx_queue_intr_enable(if_ctx_t ctx, uint16_t qid)
{
	struct enic_softc *softc;

	softc = iflib_get_softc(ctx);
	enic_enable_intr(softc, qid);

	return 0;
}

static void
enic_intr_enable_all(if_ctx_t ctx)
{
	struct enic_softc *softc;
	if_softc_ctx_t scctx;
	int i;

	softc = iflib_get_softc(ctx);
	scctx = softc->scctx;

	for (i = 0; i < scctx->isc_vectors; i++) {
		enic_enable_intr(softc, i);
	}
}

static void
enic_intr_disable_all(if_ctx_t ctx)
{
	struct enic_softc *softc;
	if_softc_ctx_t scctx;
	int i;

	softc = iflib_get_softc(ctx);
	scctx = softc->scctx;
	/*
	 * iflib may invoke this routine before enic_attach_post() has run,
	 * which is before the top level shared data area is initialized and
	 * the device made aware of it.
	 */

	for (i = 0; i < scctx->isc_vectors; i++) {
		enic_disable_intr(softc, i);
	}
}

static int
enic_dev_open(struct enic *enic)
{
	int err;
	int flags = CMD_OPENF_IG_DESCCACHE;

	err = enic_dev_wait(enic->vdev, vnic_dev_open,
			    vnic_dev_open_done, flags);
	if (err)
		dev_err(enic_get_dev(enic),
			"vNIC device open failed, err %d\n", err);

	return err;
}

static int
enic_dev_init(struct enic *enic)
{
	int err;

	vnic_dev_intr_coal_timer_info_default(enic->vdev);

	/*
	 * Get vNIC configuration
	 */
	err = enic_get_vnic_config(enic);
	if (err) {
		dev_err(dev, "Get vNIC configuration failed, aborting\n");
		return err;
	}

	/* Get available resource counts */
	enic_get_res_counts(enic);

	/* Queue counts may be zeros. rte_zmalloc returns NULL in that case. */
	enic->intr_queues = malloc(sizeof(*enic->intr_queues) *
	    enic->conf_intr_count, M_DEVBUF, M_NOWAIT | M_ZERO);

	vnic_dev_set_reset_flag(enic->vdev, 0);
	enic->max_flow_counter = -1;

	/* set up link status checking */
	vnic_dev_notify_set(enic->vdev, -1);	/* No Intr for notify */

	enic->overlay_offload = false;
	if (enic->disable_overlay && enic->vxlan) {
		/*
		 * Explicitly disable overlay offload as the setting is
		 * sticky, and resetting vNIC does not disable it.
		 */
		if (vnic_dev_overlay_offload_ctrl(enic->vdev,
		    OVERLAY_FEATURE_VXLAN, OVERLAY_OFFLOAD_DISABLE)) {
			dev_err(enic, "failed to disable overlay offload\n");
		} else {
			dev_info(enic, "Overlay offload is disabled\n");
		}
	}
	if (!enic->disable_overlay && enic->vxlan &&
	/* 'VXLAN feature' enables VXLAN, NVGRE, and GENEVE. */
	    vnic_dev_overlay_offload_ctrl(enic->vdev,
	    OVERLAY_FEATURE_VXLAN, OVERLAY_OFFLOAD_ENABLE) == 0) {
		enic->overlay_offload = true;
		enic->vxlan_port = ENIC_DEFAULT_VXLAN_PORT;
		dev_info(enic, "Overlay offload is enabled\n");
		/*
		 * Reset the vxlan port to the default, as the NIC firmware
		 * does not reset it automatically and keeps the old setting.
		 */
		if (vnic_dev_overlay_offload_cfg(enic->vdev,
		   OVERLAY_CFG_VXLAN_PORT_UPDATE, ENIC_DEFAULT_VXLAN_PORT)) {
			dev_err(enic, "failed to update vxlan port\n");
			return -EINVAL;
		}
	}
	return 0;
}

static void    *
enic_alloc_consistent(void *priv, size_t size, bus_addr_t * dma_handle,
    struct iflib_dma_info *res, u8 * name)
{
	void	       *vaddr;
	*dma_handle = 0;
	struct enic    *enic = (struct enic *)priv;
	int		rz;

	rz = iflib_dma_alloc(enic->softc->ctx, size, res, BUS_DMA_NOWAIT);
	if (rz) {
		pr_err("%s : Failed to allocate memory requested for %s\n",
		    __func__, name);
		return NULL;
	}

	vaddr = res->idi_vaddr;
	*dma_handle = res->idi_paddr;

	return vaddr;
}

static void
enic_free_consistent(void *priv, size_t size, void *vaddr,
    bus_addr_t dma_handle, struct iflib_dma_info *res)
{
	iflib_dma_free(res);
}

static int
enic_pci_mapping(struct enic_softc *softc)
{
	int rc;

	rc = enic_map_bar(softc, &softc->mem, 0, true);
	if (rc)
		return rc;

	rc = enic_map_bar(softc, &softc->io, 2, false);

	return rc;
}

static void
enic_pci_mapping_free(struct enic_softc *softc)
{
	if (softc->mem.res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
				     softc->mem.rid, softc->mem.res);
	softc->mem.res = NULL;

	if (softc->io.res != NULL)
		bus_release_resource(softc->dev, SYS_RES_MEMORY,
				     softc->io.rid, softc->io.res);
	softc->io.res = NULL;
}

static int
enic_dev_wait(struct vnic_dev *vdev, int (*start) (struct vnic_dev *, int),
    int (*finished) (struct vnic_dev *, int *), int arg)
{
	int done;
	int err;
	int i;

	err = start(vdev, arg);
	if (err)
		return err;

	/* Wait for func to complete...2 seconds max */
	for (i = 0; i < 2000; i++) {
		err = finished(vdev, &done);
		if (err)
			return err;
		if (done)
			return 0;
		usleep(1000);
	}
	return -ETIMEDOUT;
}

static int
enic_map_bar(struct enic_softc *softc, struct enic_bar_info *bar, int bar_num,
    bool shareable)
{
	uint32_t flag;

	if (bar->res != NULL) {
		device_printf(softc->dev, "Bar %d already mapped\n", bar_num);
		return EDOOFUS;
	}

	bar->rid = PCIR_BAR(bar_num);
	flag = RF_ACTIVE;
	if (shareable)
		flag |= RF_SHAREABLE;

	if ((bar->res = bus_alloc_resource_any(softc->dev,
	   SYS_RES_MEMORY, &bar->rid, flag)) == NULL) {
		device_printf(softc->dev,
			      "PCI BAR%d mapping failure\n", bar_num);
		return (ENXIO);
	}
	bar->tag = rman_get_bustag(bar->res);
	bar->handle = rman_get_bushandle(bar->res);
	bar->size = rman_get_size(bar->res);

	return 0;
}

void
enic_init_vnic_resources(struct enic *enic)
{
	unsigned int error_interrupt_enable = 1;
	unsigned int error_interrupt_offset = 0;
	unsigned int rxq_interrupt_enable = 0;
	unsigned int rxq_interrupt_offset = ENICPMD_RXQ_INTR_OFFSET;
	unsigned int txq_interrupt_enable = 0;
	unsigned int txq_interrupt_offset = ENICPMD_RXQ_INTR_OFFSET;
	unsigned int index = 0;
	unsigned int cq_idx;
	if_softc_ctx_t scctx;

	scctx = enic->softc->scctx;


	rxq_interrupt_enable = 1;
	txq_interrupt_enable = 1;

	rxq_interrupt_offset = 0;
	txq_interrupt_offset = enic->intr_count - 2;
	txq_interrupt_offset = 1;

	for (index = 0; index < enic->intr_count; index++) {
		vnic_intr_alloc(enic->vdev, &enic->intr[index], index);
	}

	for (index = 0; index < scctx->isc_nrxqsets; index++) {
		cq_idx = enic_cq_rq(enic, index);

		vnic_rq_clean(&enic->rq[index]);
		vnic_rq_init(&enic->rq[index], cq_idx, error_interrupt_enable,
		    error_interrupt_offset);

		vnic_cq_clean(&enic->cq[cq_idx]);
		vnic_cq_init(&enic->cq[cq_idx],
		    0 /* flow_control_enable */ ,
		    1 /* color_enable */ ,
		    0 /* cq_head */ ,
		    0 /* cq_tail */ ,
		    1 /* cq_tail_color */ ,
		    rxq_interrupt_enable,
		    1 /* cq_entry_enable */ ,
		    0 /* cq_message_enable */ ,
		    rxq_interrupt_offset,
		    0 /* cq_message_addr */ );
		if (rxq_interrupt_enable)
			rxq_interrupt_offset++;
	}

	for (index = 0; index < scctx->isc_ntxqsets; index++) {
		cq_idx = enic_cq_wq(enic, index);
		vnic_wq_clean(&enic->wq[index]);
		vnic_wq_init(&enic->wq[index], cq_idx, error_interrupt_enable,
		    error_interrupt_offset);
		/* Compute unsupported ol flags for enic_prep_pkts() */
		enic->wq[index].tx_offload_notsup_mask = 0;

		vnic_cq_clean(&enic->cq[cq_idx]);
		vnic_cq_init(&enic->cq[cq_idx],
		   0 /* flow_control_enable */ ,
		   1 /* color_enable */ ,
		   0 /* cq_head */ ,
		   0 /* cq_tail */ ,
		   1 /* cq_tail_color */ ,
		   txq_interrupt_enable,
		   1,
		   0,
		   txq_interrupt_offset,
		   0 /* (u64)enic->wq[index].cqmsg_rz->iova */ );

	}

	for (index = 0; index < enic->intr_count; index++) {
		vnic_intr_init(&enic->intr[index], 125,
		    enic->config.intr_timer_type, /* mask_on_assertion */ 1);
	}
}

static void
enic_update_packet_filter(struct enic *enic)
{
	struct enic_softc *softc = enic->softc;

	ENIC_LOCK(softc);
	vnic_dev_packet_filter(enic->vdev,
	    softc->directed,
	    softc->multicast,
	    softc->broadcast,
	    softc->promisc,
	    softc->allmulti);
	ENIC_UNLOCK(softc);
}

static bool
enic_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
	default:
		return (false);
	}
}

int
enic_setup_finish(struct enic *enic)
{
	struct enic_softc *softc = enic->softc;

	/* Default conf */
	softc->directed = 1;
	softc->multicast = 0;
	softc->broadcast = 1;
	softc->promisc = 0;
	softc->allmulti = 1;
	enic_update_packet_filter(enic);

	return 0;
}
