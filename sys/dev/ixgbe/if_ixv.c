/*****************************************************************************

  Copyright (c) 2001-2017, Intel Corporation
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

   1. Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

   2. Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.

   3. Neither the name of the Intel Corporation nor the names of its
      contributors may be used to endorse or promote products derived from
      this software without specific prior written permission.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
  POSSIBILITY OF SUCH DAMAGE.

*****************************************************************************/


#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"

#include "ixgbe.h"
#include "ifdi_if.h"

#include <net/netmap.h>
#include <dev/netmap/netmap_kern.h>

/************************************************************************
 * Driver version
 ************************************************************************/
static const char ixv_driver_version[] = "2.0.1-k";

/************************************************************************
 * PCI Device ID Table
 *
 *   Used by probe to select devices to load on
 *   Last field stores an index into ixv_strings
 *   Last entry must be all 0s
 *
 *   { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 ************************************************************************/
static const pci_vendor_info_t ixv_vendor_info_array[] =
{
	PVID(IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_VF,
	    "Intel(R) X520 82599 Virtual Function"),
	PVID(IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540_VF,
	    "Intel(R) X540 Virtual Function"),
	PVID(IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550_VF,
	    "Intel(R) X550 Virtual Function"),
	PVID(IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_VF,
	    "Intel(R) X552 Virtual Function"),
	PVID(IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_A_VF,
	    "Intel(R) X553 Virtual Function"),
	/* required last entry */
	PVID_END
};

/************************************************************************
 * Function prototypes
 ************************************************************************/
static void     *ixv_register(device_t);
static int      ixv_if_attach_pre(if_ctx_t);
static int      ixv_if_attach_post(if_ctx_t);
static int      ixv_if_detach(if_ctx_t);

static int      ixv_if_rx_queue_intr_enable(if_ctx_t, uint16_t);
static int      ixv_if_tx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int,
    int);
static int      ixv_if_rx_queues_alloc(if_ctx_t, caddr_t *, uint64_t *, int,
    int);
static void     ixv_if_queues_free(if_ctx_t);
static void     ixv_identify_hardware(if_ctx_t);
static void     ixv_init_device_features(struct ixgbe_softc *);
static int      ixv_allocate_pci_resources(if_ctx_t);
static void     ixv_free_pci_resources(if_ctx_t);
static int      ixv_setup_interface(if_ctx_t);
static void     ixv_if_media_status(if_ctx_t, struct ifmediareq *);
static int      ixv_if_media_change(if_ctx_t);
static void     ixv_if_update_admin_status(if_ctx_t);
static int      ixv_if_msix_intr_assign(if_ctx_t, int);

static int      ixv_if_mtu_set(if_ctx_t, uint32_t);
static void     ixv_if_init(if_ctx_t);
static void     ixv_if_local_timer(if_ctx_t, uint16_t);
static void     ixv_if_stop(if_ctx_t);
static int      ixv_negotiate_api(struct ixgbe_softc *);

static void     ixv_initialize_transmit_units(if_ctx_t);
static void     ixv_initialize_receive_units(if_ctx_t);
static void     ixv_initialize_rss_mapping(struct ixgbe_softc *);

static void     ixv_setup_vlan_support(if_ctx_t);
static void     ixv_configure_ivars(struct ixgbe_softc *);
static void     ixv_if_enable_intr(if_ctx_t);
static void     ixv_if_disable_intr(if_ctx_t);
static void     ixv_if_multi_set(if_ctx_t);

static void     ixv_if_register_vlan(if_ctx_t, u16);
static void     ixv_if_unregister_vlan(if_ctx_t, u16);

static uint64_t ixv_if_get_counter(if_ctx_t, ift_counter);
static bool	ixv_if_needs_restart(if_ctx_t, enum iflib_restart_event);

static void     ixv_save_stats(struct ixgbe_softc *);
static void     ixv_init_stats(struct ixgbe_softc *);
static void     ixv_update_stats(struct ixgbe_softc *);
static void     ixv_add_stats_sysctls(struct ixgbe_softc *);

static int      ixv_sysctl_debug(SYSCTL_HANDLER_ARGS);
static void     ixv_set_ivar(struct ixgbe_softc *, u8, u8, s8);

static u8       *ixv_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

/* The MSI-X Interrupt handlers */
static int      ixv_msix_que(void *);
static int      ixv_msix_mbx(void *);

/************************************************************************
 * FreeBSD Device Interface Entry Points
 ************************************************************************/
static device_method_t ixv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, ixv_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD_END
};

static driver_t ixv_driver = {
	"ixv", ixv_methods, sizeof(struct ixgbe_softc),
};

DRIVER_MODULE(ixv, pci, ixv_driver, 0, 0);
IFLIB_PNP_INFO(pci, ixv_driver, ixv_vendor_info_array);
MODULE_DEPEND(ixv, iflib, 1, 1, 1);
MODULE_DEPEND(ixv, pci, 1, 1, 1);
MODULE_DEPEND(ixv, ether, 1, 1, 1);

static device_method_t ixv_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, ixv_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, ixv_if_attach_post),
	DEVMETHOD(ifdi_detach, ixv_if_detach),
	DEVMETHOD(ifdi_init, ixv_if_init),
	DEVMETHOD(ifdi_stop, ixv_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, ixv_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, ixv_if_enable_intr),
	DEVMETHOD(ifdi_intr_disable, ixv_if_disable_intr),
	DEVMETHOD(ifdi_tx_queue_intr_enable, ixv_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, ixv_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queues_alloc, ixv_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, ixv_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, ixv_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, ixv_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, ixv_if_multi_set),
	DEVMETHOD(ifdi_mtu_set, ixv_if_mtu_set),
	DEVMETHOD(ifdi_media_status, ixv_if_media_status),
	DEVMETHOD(ifdi_media_change, ixv_if_media_change),
	DEVMETHOD(ifdi_timer, ixv_if_local_timer),
	DEVMETHOD(ifdi_vlan_register, ixv_if_register_vlan),
	DEVMETHOD(ifdi_vlan_unregister, ixv_if_unregister_vlan),
	DEVMETHOD(ifdi_get_counter, ixv_if_get_counter),
	DEVMETHOD(ifdi_needs_restart, ixv_if_needs_restart),
	DEVMETHOD_END
};

static driver_t ixv_if_driver = {
  "ixv_if", ixv_if_methods, sizeof(struct ixgbe_softc)
};

/*
 * TUNEABLE PARAMETERS:
 */

/* Flow control setting, default to full */
static int ixv_flow_control = ixgbe_fc_full;
TUNABLE_INT("hw.ixv.flow_control", &ixv_flow_control);

/*
 * Header split: this causes the hardware to DMA
 * the header into a separate mbuf from the payload,
 * it can be a performance win in some workloads, but
 * in others it actually hurts, its off by default.
 */
static int ixv_header_split = false;
TUNABLE_INT("hw.ixv.hdr_split", &ixv_header_split);

extern struct if_txrx ixgbe_txrx;

static struct if_shared_ctx ixv_sctx_init = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,/* max(DBA_ALIGN, PAGE_SIZE) */
	.isc_tx_maxsize = IXGBE_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tx_maxsegsize = PAGE_SIZE,
	.isc_tso_maxsize = IXGBE_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = PAGE_SIZE,
	.isc_rx_maxsize = MJUM16BYTES,
	.isc_rx_nsegments = 1,
	.isc_rx_maxsegsize = MJUM16BYTES,
	.isc_nfl = 1,
	.isc_ntxqs = 1,
	.isc_nrxqs = 1,
	.isc_admin_intrcnt = 1,
	.isc_vendor_info = ixv_vendor_info_array,
	.isc_driver_version = ixv_driver_version,
	.isc_driver = &ixv_if_driver,
	.isc_flags = IFLIB_IS_VF | IFLIB_TSO_INIT_IP,

	.isc_nrxd_min = {MIN_RXD},
	.isc_ntxd_min = {MIN_TXD},
	.isc_nrxd_max = {MAX_RXD},
	.isc_ntxd_max = {MAX_TXD},
	.isc_nrxd_default = {DEFAULT_RXD},
	.isc_ntxd_default = {DEFAULT_TXD},
};

static void *
ixv_register(device_t dev)
{
	return (&ixv_sctx_init);
}

/************************************************************************
 * ixv_if_tx_queues_alloc
 ************************************************************************/
static int
ixv_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int ntxqs, int ntxqsets)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	struct ix_tx_queue *que;
	int i, j, error;

	MPASS(sc->num_tx_queues == ntxqsets);
	MPASS(ntxqs == 1);

	/* Allocate queue structure memory */
	sc->tx_queues =
	    (struct ix_tx_queue *)malloc(sizeof(struct ix_tx_queue) *
	    ntxqsets, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!sc->tx_queues) {
		device_printf(iflib_get_dev(ctx),
		    "Unable to allocate TX ring memory\n");
		return (ENOMEM);
	}

	for (i = 0, que = sc->tx_queues; i < ntxqsets; i++, que++) {
		struct tx_ring *txr = &que->txr;

		txr->me = i;
		txr->sc =  que->sc = sc;

		/* Allocate report status array */
		if (!(txr->tx_rsq = (qidx_t *)malloc(sizeof(qidx_t) *
		    scctx->isc_ntxd[0], M_DEVBUF, M_NOWAIT | M_ZERO))) {
			error = ENOMEM;
			goto fail;
		}
		for (j = 0; j < scctx->isc_ntxd[0]; j++)
			txr->tx_rsq[j] = QIDX_INVALID;
		/* get virtual and physical address of the hardware queues */
		txr->tail = IXGBE_VFTDT(txr->me);
		txr->tx_base = (union ixgbe_adv_tx_desc *)vaddrs[i*ntxqs];
		txr->tx_paddr = paddrs[i*ntxqs];

		txr->bytes = 0;
		txr->total_packets = 0;

	}

	device_printf(iflib_get_dev(ctx), "allocated for %d queues\n",
	    sc->num_tx_queues);

	return (0);

 fail:
	ixv_if_queues_free(ctx);

	return (error);
} /* ixv_if_tx_queues_alloc */

/************************************************************************
 * ixv_if_rx_queues_alloc
 ************************************************************************/
static int
ixv_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int nrxqs, int nrxqsets)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ix_rx_queue *que;
	int i, error;

	MPASS(sc->num_rx_queues == nrxqsets);
	MPASS(nrxqs == 1);

	/* Allocate queue structure memory */
	sc->rx_queues =
	    (struct ix_rx_queue *)malloc(sizeof(struct ix_rx_queue) *
	    nrxqsets, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!sc->rx_queues) {
		device_printf(iflib_get_dev(ctx),
		    "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto fail;
	}

	for (i = 0, que = sc->rx_queues; i < nrxqsets; i++, que++) {
		struct rx_ring *rxr = &que->rxr;
		rxr->me = i;
		rxr->sc = que->sc = sc;


		/* get the virtual and physical address of the hw queues */
		rxr->tail = IXGBE_VFRDT(rxr->me);
		rxr->rx_base = (union ixgbe_adv_rx_desc *)vaddrs[i];
		rxr->rx_paddr = paddrs[i*nrxqs];
		rxr->bytes = 0;
		rxr->que = que;
	}

	device_printf(iflib_get_dev(ctx), "allocated for %d rx queues\n",
	    sc->num_rx_queues);

	return (0);

fail:
	ixv_if_queues_free(ctx);

	return (error);
} /* ixv_if_rx_queues_alloc */

/************************************************************************
 * ixv_if_queues_free
 ************************************************************************/
static void
ixv_if_queues_free(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ix_tx_queue *que = sc->tx_queues;
	int i;

	if (que == NULL)
		goto free;

	for (i = 0; i < sc->num_tx_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		if (txr->tx_rsq == NULL)
			break;

		free(txr->tx_rsq, M_DEVBUF);
		txr->tx_rsq = NULL;
	}
	if (sc->tx_queues != NULL)
		free(sc->tx_queues, M_DEVBUF);
free:
	if (sc->rx_queues != NULL)
		free(sc->rx_queues, M_DEVBUF);
	sc->tx_queues = NULL;
	sc->rx_queues = NULL;
} /* ixv_if_queues_free */

/************************************************************************
 * ixv_if_attach_pre - Device initialization routine
 *
 *   Called when the driver is being loaded.
 *   Identifies the type of hardware, allocates all resources
 *   and initializes the hardware.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static int
ixv_if_attach_pre(if_ctx_t ctx)
{
	struct ixgbe_softc *sc;
	device_t dev;
	if_softc_ctx_t scctx;
	struct ixgbe_hw *hw;
	int error = 0;

	INIT_DEBUGOUT("ixv_attach: begin");

	/* Allocate, clear, and link in our sc structure */
	dev = iflib_get_dev(ctx);
	sc = iflib_get_softc(ctx);
	sc->dev = dev;
	sc->ctx = ctx;
	sc->hw.back = sc;
	scctx = sc->shared = iflib_get_softc_ctx(ctx);
	sc->media = iflib_get_media(ctx);
	hw = &sc->hw;

	/* Do base PCI setup - map BAR0 */
	if (ixv_allocate_pci_resources(ctx)) {
		device_printf(dev, "ixv_allocate_pci_resources() failed!\n");
		error = ENXIO;
		goto err_out;
	}

	/* SYSCTL APIs */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO, "debug",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    sc, 0, ixv_sysctl_debug, "I", "Debug Info");

	/* Determine hardware revision */
	ixv_identify_hardware(ctx);
	ixv_init_device_features(sc);

	/* Initialize the shared code */
	error = ixgbe_init_ops_vf(hw);
	if (error) {
		device_printf(dev, "ixgbe_init_ops_vf() failed!\n");
		error = EIO;
		goto err_out;
	}

	/* Setup the mailbox */
	ixgbe_init_mbx_params_vf(hw);

	error = hw->mac.ops.reset_hw(hw);
	if (error == IXGBE_ERR_RESET_FAILED)
		device_printf(dev, "...reset_hw() failure: Reset Failed!\n");
	else if (error)
		device_printf(dev, "...reset_hw() failed with error %d\n",
		    error);
	if (error) {
		error = EIO;
		goto err_out;
	}

	error = hw->mac.ops.init_hw(hw);
	if (error) {
		device_printf(dev, "...init_hw() failed with error %d\n",
		    error);
		error = EIO;
		goto err_out;
	}

	/* Negotiate mailbox API version */
	error = ixv_negotiate_api(sc);
	if (error) {
		device_printf(dev,
		    "Mailbox API negotiation failed during attach!\n");
		goto err_out;
	}

	/* Check if VF was disabled by PF */
	error = hw->mac.ops.get_link_state(hw, &sc->link_enabled);
	if (error) {
		/* PF is not capable of controlling VF state. Enable link. */
		sc->link_enabled = true;
	}

	/* If no mac address was assigned, make a random one */
	if (!ixv_check_ether_addr(hw->mac.addr)) {
		ether_gen_addr(iflib_get_ifp(ctx),
		    (struct ether_addr *)hw->mac.addr);
		bcopy(hw->mac.addr, hw->mac.perm_addr,
		    sizeof(hw->mac.perm_addr));
	}

	/* Most of the iflib initialization... */

	iflib_set_mac(ctx, hw->mac.addr);
	switch (sc->hw.mac.type) {
	case ixgbe_mac_X550_vf:
	case ixgbe_mac_X550EM_x_vf:
	case ixgbe_mac_X550EM_a_vf:
		scctx->isc_ntxqsets_max = scctx->isc_nrxqsets_max = 2;
		break;
	default:
		scctx->isc_ntxqsets_max = scctx->isc_nrxqsets_max = 1;
	}
	scctx->isc_txqsizes[0] =
	    roundup2(scctx->isc_ntxd[0] * sizeof(union ixgbe_adv_tx_desc) +
	    sizeof(u32), DBA_ALIGN);
	scctx->isc_rxqsizes[0] =
	    roundup2(scctx->isc_nrxd[0] * sizeof(union ixgbe_adv_rx_desc),
	    DBA_ALIGN);
	/* XXX */
	scctx->isc_tx_csum_flags = CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_TSO |
	    CSUM_IP6_TCP | CSUM_IP6_UDP | CSUM_IP6_TSO;
	scctx->isc_tx_nsegments = IXGBE_82599_SCATTER;
	scctx->isc_msix_bar = pci_msix_table_bar(dev);
	scctx->isc_tx_tso_segments_max = scctx->isc_tx_nsegments;
	scctx->isc_tx_tso_size_max = IXGBE_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = PAGE_SIZE;

	scctx->isc_txrx = &ixgbe_txrx;

	/*
	 * Tell the upper layer(s) we support everything the PF
	 * driver does except...
	 *   Wake-on-LAN
	 */
	scctx->isc_capabilities = IXGBE_CAPS;
	scctx->isc_capabilities ^= IFCAP_WOL;
	scctx->isc_capenable = scctx->isc_capabilities;

	INIT_DEBUGOUT("ixv_if_attach_pre: end");

	return (0);

err_out:
	ixv_free_pci_resources(ctx);

	return (error);
} /* ixv_if_attach_pre */

static int
ixv_if_attach_post(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	int error = 0;

	/* Setup OS specific network interface */
	error = ixv_setup_interface(ctx);
	if (error) {
		device_printf(dev, "Interface setup failed: %d\n", error);
		goto end;
	}

	/* Do the stats setup */
	ixv_save_stats(sc);
	ixv_init_stats(sc);
	ixv_add_stats_sysctls(sc);

end:
	return error;
} /* ixv_if_attach_post */

/************************************************************************
 * ixv_detach - Device removal routine
 *
 *   Called when the driver is being removed.
 *   Stops the adapter and deallocates all the resources
 *   that were allocated for driver operation.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static int
ixv_if_detach(if_ctx_t ctx)
{
	INIT_DEBUGOUT("ixv_detach: begin");

	ixv_free_pci_resources(ctx);

	return (0);
} /* ixv_if_detach */

/************************************************************************
 * ixv_if_mtu_set
 ************************************************************************/
static int
ixv_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int error = 0;

	IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
	if (mtu > IXGBE_MAX_FRAME_SIZE - IXGBE_MTU_HDR) {
		error = EINVAL;
	} else {
		if_setmtu(ifp, mtu);
		sc->max_frame_size = if_getmtu(ifp) + IXGBE_MTU_HDR;
	}

	return error;
} /* ixv_if_mtu_set */

/************************************************************************
 * ixv_if_init - Init entry point
 *
 *   Used in two ways: It is used by the stack as an init entry
 *   point in network interface structure. It is also used
 *   by the driver as a hw/sw initialization routine to get
 *   to a consistent state.
 *
 *   return 0 on success, positive on failure
 ************************************************************************/
static void
ixv_if_init(if_ctx_t ctx)
{
	struct ixgbe_softc  *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	device_t dev = iflib_get_dev(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	int error = 0;

	INIT_DEBUGOUT("ixv_if_init: begin");
	hw->adapter_stopped = false;
	hw->mac.ops.stop_adapter(hw);

	/* reprogram the RAR[0] in case user changed it. */
	hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	bcopy(if_getlladdr(ifp), hw->mac.addr, IXGBE_ETH_LENGTH_OF_ADDRESS);
	hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, 1);

	/* Reset VF and renegotiate mailbox API version */
	hw->mac.ops.reset_hw(hw);
	hw->mac.ops.start_hw(hw);
	error = ixv_negotiate_api(sc);
	if (error) {
		device_printf(dev,
		    "Mailbox API negotiation failed in if_init!\n");
		return;
	}

	ixv_initialize_transmit_units(ctx);

	/* Setup Multicast table */
	ixv_if_multi_set(ctx);

	sc->rx_mbuf_sz = iflib_get_rx_mbuf_sz(ctx);

	/* Configure RX settings */
	ixv_initialize_receive_units(ctx);

	/* Set up VLAN offload and filter */
	ixv_setup_vlan_support(ctx);

	/* Set up MSI-X routing */
	ixv_configure_ivars(sc);

	/* Set up auto-mask */
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAM, IXGBE_EICS_RTX_QUEUE);

	/* Set moderation on the Link interrupt */
	IXGBE_WRITE_REG(hw, IXGBE_VTEITR(sc->vector), IXGBE_LINK_ITR);

	/* Stats init */
	ixv_init_stats(sc);

	/* Config/Enable Link */
	error = hw->mac.ops.get_link_state(hw, &sc->link_enabled);
	if (error) {
		/* PF is not capable of controlling VF state. Enable the link. */
		sc->link_enabled = true;
	} else if (sc->link_enabled == false)
		device_printf(dev, "VF is disabled by PF\n");

	hw->mac.ops.check_link(hw, &sc->link_speed, &sc->link_up,
	    false);

	/* And now turn on interrupts */
	ixv_if_enable_intr(ctx);

	return;
} /* ixv_if_init */

/************************************************************************
 * ixv_enable_queue
 ************************************************************************/
static inline void
ixv_enable_queue(struct ixgbe_softc *sc, u32 vector)
{
	struct ixgbe_hw *hw = &sc->hw;
	u32 queue = 1 << vector;
	u32 mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);
} /* ixv_enable_queue */

/************************************************************************
 * ixv_disable_queue
 ************************************************************************/
static inline void
ixv_disable_queue(struct ixgbe_softc *sc, u32 vector)
{
	struct ixgbe_hw *hw = &sc->hw;
	u64 queue = (u64)(1 << vector);
	u32 mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMC, mask);
} /* ixv_disable_queue */


/************************************************************************
 * ixv_msix_que - MSI-X Queue Interrupt Service routine
 ************************************************************************/
static int
ixv_msix_que(void *arg)
{
	struct ix_rx_queue *que = arg;
	struct ixgbe_softc *sc = que->sc;

	ixv_disable_queue(sc, que->msix);
	++que->irqs;

	return (FILTER_SCHEDULE_THREAD);
} /* ixv_msix_que */

/************************************************************************
 * ixv_msix_mbx
 ************************************************************************/
static int
ixv_msix_mbx(void *arg)
{
	struct ixgbe_softc *sc = arg;
	struct ixgbe_hw *hw = &sc->hw;
	u32 reg;

	++sc->link_irq;

	/* First get the cause */
	reg = IXGBE_READ_REG(hw, IXGBE_VTEICS);
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_VTEICR, reg);

	/* Link status change */
	if (reg & IXGBE_EICR_LSC)
		iflib_admin_intr_deferred(sc->ctx);

	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, IXGBE_EIMS_OTHER);

	return (FILTER_HANDLED);
} /* ixv_msix_mbx */

/************************************************************************
 * ixv_media_status - Media Ioctl callback
 *
 *   Called whenever the user queries the status of
 *   the interface using ifconfig.
 ************************************************************************/
static void
ixv_if_media_status(if_ctx_t ctx, struct ifmediareq * ifmr)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);

	INIT_DEBUGOUT("ixv_media_status: begin");

	iflib_admin_intr_deferred(ctx);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	switch (sc->link_speed) {
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= IFM_10G_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_100_FULL:
			ifmr->ifm_active |= IFM_100_TX | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10_FULL:
			ifmr->ifm_active |= IFM_10_T | IFM_FDX;
			break;
	}
} /* ixv_if_media_status */

/************************************************************************
 * ixv_if_media_change - Media Ioctl callback
 *
 *   Called when the user changes speed/duplex using
 *   media/mediopt option with ifconfig.
 ************************************************************************/
static int
ixv_if_media_change(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ifmedia *ifm = iflib_get_media(ctx);

	INIT_DEBUGOUT("ixv_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		break;
	default:
		device_printf(sc->dev, "Only auto media type\n");
		return (EINVAL);
	}

	return (0);
} /* ixv_if_media_change */


/************************************************************************
 * ixv_negotiate_api
 *
 *   Negotiate the Mailbox API with the PF;
 *   start with the most featured API first.
 ************************************************************************/
static int
ixv_negotiate_api(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	int mbx_api[] = {
		ixgbe_mbox_api_12,
		ixgbe_mbox_api_11,
		ixgbe_mbox_api_10,
		ixgbe_mbox_api_unknown
	};
	int i = 0;

	while (mbx_api[i] != ixgbe_mbox_api_unknown) {
		if (ixgbevf_negotiate_api_version(hw, mbx_api[i]) == 0)
			return (0);
		i++;
	}

	return (EINVAL);
} /* ixv_negotiate_api */


static u_int
ixv_if_multi_set_cb(void *cb_arg, struct sockaddr_dl *addr, u_int cnt)
{
	bcopy(LLADDR(addr),
	    &((u8 *)cb_arg)[cnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
	    IXGBE_ETH_LENGTH_OF_ADDRESS);

	return (++cnt);
}

/************************************************************************
 * ixv_if_multi_set - Multicast Update
 *
 *   Called whenever multicast address list is updated.
 ************************************************************************/
static void
ixv_if_multi_set(if_ctx_t ctx)
{
	u8 mta[MAX_NUM_MULTICAST_ADDRESSES * IXGBE_ETH_LENGTH_OF_ADDRESS];
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	u8 *update_ptr;
	if_t ifp = iflib_get_ifp(ctx);
	int mcnt = 0;

	IOCTL_DEBUGOUT("ixv_if_multi_set: begin");

	mcnt = if_foreach_llmaddr(ifp, ixv_if_multi_set_cb, mta);

	update_ptr = mta;

	sc->hw.mac.ops.update_mc_addr_list(&sc->hw, update_ptr, mcnt,
	    ixv_mc_array_itr, true);
} /* ixv_if_multi_set */

/************************************************************************
 * ixv_mc_array_itr
 *
 *   An iterator function needed by the multicast shared code.
 *   It feeds the shared code routine the addresses in the
 *   array of ixv_set_multi() one by one.
 ************************************************************************/
static u8 *
ixv_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
{
	u8 *addr = *update_ptr;
	u8 *newptr;

	*vmdq = 0;

	newptr = addr + IXGBE_ETH_LENGTH_OF_ADDRESS;
	*update_ptr = newptr;

	return addr;
} /* ixv_mc_array_itr */

/************************************************************************
 * ixv_if_local_timer - Timer routine
 *
 *   Checks for link status, updates statistics,
 *   and runs the watchdog check.
 ************************************************************************/
static void
ixv_if_local_timer(if_ctx_t ctx, uint16_t qid)
{
	if (qid != 0)
		return;

	/* Fire off the adminq task */
	iflib_admin_intr_deferred(ctx);
} /* ixv_if_local_timer */

/************************************************************************
 * ixv_if_update_admin_status - Update OS on link state
 *
 * Note: Only updates the OS on the cached link state.
 *       The real check of the hardware only happens with
 *       a link interrupt.
 ************************************************************************/
static void
ixv_if_update_admin_status(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	s32 status;

	sc->hw.mac.get_link_status = true;

	status = ixgbe_check_link(&sc->hw, &sc->link_speed,
	    &sc->link_up, false);

	if (status != IXGBE_SUCCESS && sc->hw.adapter_stopped == false) {
		/* Mailbox's Clear To Send status is lost or timeout occurred.
		 * We need reinitialization. */
		if_init(iflib_get_ifp(ctx), ctx);
	}

	if (sc->link_up && sc->link_enabled) {
		if (sc->link_active == false) {
			if (bootverbose)
				device_printf(dev, "Link is up %d Gbps %s \n",
				    ((sc->link_speed == 128) ? 10 : 1),
				    "Full Duplex");
			sc->link_active = true;
			iflib_link_state_change(ctx, LINK_STATE_UP,
			    ixgbe_link_speed_to_baudrate(sc->link_speed));
		}
	} else { /* Link down */
		if (sc->link_active == true) {
			if (bootverbose)
				device_printf(dev, "Link is Down\n");
			iflib_link_state_change(ctx, LINK_STATE_DOWN,  0);
			sc->link_active = false;
		}
	}

	/* Stats Update */
	ixv_update_stats(sc);
} /* ixv_if_update_admin_status */


/************************************************************************
 * ixv_if_stop - Stop the hardware
 *
 *   Disables all traffic on the adapter by issuing a
 *   global reset on the MAC and deallocates TX/RX buffers.
 ************************************************************************/
static void
ixv_if_stop(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;

	INIT_DEBUGOUT("ixv_stop: begin\n");

	ixv_if_disable_intr(ctx);

	hw->mac.ops.reset_hw(hw);
	sc->hw.adapter_stopped = false;
	hw->mac.ops.stop_adapter(hw);

	/* Update the stack */
	sc->link_up = false;
	ixv_if_update_admin_status(ctx);

	/* reprogram the RAR[0] in case user changed it. */
	hw->mac.ops.set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);
} /* ixv_if_stop */


/************************************************************************
 * ixv_identify_hardware - Determine hardware revision.
 ************************************************************************/
static void
ixv_identify_hardware(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	struct ixgbe_hw *hw = &sc->hw;

	/* Save off the information about this board */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_get_revid(dev);
	hw->subsystem_vendor_id = pci_get_subvendor(dev);
	hw->subsystem_device_id = pci_get_subdevice(dev);

	/* A subset of set_mac_type */
	switch (hw->device_id) {
	case IXGBE_DEV_ID_82599_VF:
		hw->mac.type = ixgbe_mac_82599_vf;
		break;
	case IXGBE_DEV_ID_X540_VF:
		hw->mac.type = ixgbe_mac_X540_vf;
		break;
	case IXGBE_DEV_ID_X550_VF:
		hw->mac.type = ixgbe_mac_X550_vf;
		break;
	case IXGBE_DEV_ID_X550EM_X_VF:
		hw->mac.type = ixgbe_mac_X550EM_x_vf;
		break;
	case IXGBE_DEV_ID_X550EM_A_VF:
		hw->mac.type = ixgbe_mac_X550EM_a_vf;
		break;
	default:
		device_printf(dev, "unknown mac type\n");
		hw->mac.type = ixgbe_mac_unknown;
		break;
	}
} /* ixv_identify_hardware */

/************************************************************************
 * ixv_if_msix_intr_assign - Setup MSI-X Interrupt resources and handlers
 ************************************************************************/
static int
ixv_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	struct ix_rx_queue *rx_que = sc->rx_queues;
	struct ix_tx_queue *tx_que;
	int error, rid, vector = 0;
	char buf[16];

	for (int i = 0; i < sc->num_rx_queues; i++, vector++, rx_que++) {
		rid = vector + 1;

		snprintf(buf, sizeof(buf), "rxq%d", i);
		error = iflib_irq_alloc_generic(ctx, &rx_que->que_irq, rid,
		    IFLIB_INTR_RXTX, ixv_msix_que, rx_que, rx_que->rxr.me,
		    buf);

		if (error) {
			device_printf(iflib_get_dev(ctx),
			    "Failed to allocate que int %d err: %d",
			    i, error);
			sc->num_rx_queues = i + 1;
			goto fail;
		}

		rx_que->msix = vector;
	}

	for (int i = 0; i < sc->num_tx_queues; i++) {
		snprintf(buf, sizeof(buf), "txq%d", i);
		tx_que = &sc->tx_queues[i];
		tx_que->msix = i % sc->num_rx_queues;
		iflib_softirq_alloc_generic(ctx,
		    &sc->rx_queues[tx_que->msix].que_irq,
		    IFLIB_INTR_TX, tx_que, tx_que->txr.me, buf);
	}
	rid = vector + 1;
	error = iflib_irq_alloc_generic(ctx, &sc->irq, rid,
	    IFLIB_INTR_ADMIN, ixv_msix_mbx, sc, 0, "aq");
	if (error) {
		device_printf(iflib_get_dev(ctx),
		    "Failed to register admin handler");
		return (error);
	}

	sc->vector = vector;
	/*
	 * Due to a broken design QEMU will fail to properly
	 * enable the guest for MSIX unless the vectors in
	 * the table are all set up, so we must rewrite the
	 * ENABLE in the MSIX control register again at this
	 * point to cause it to successfully initialize us.
	 */
	if (sc->hw.mac.type == ixgbe_mac_82599_vf) {
		int msix_ctrl;
		if (pci_find_cap(dev, PCIY_MSIX, &rid)) {
			device_printf(dev,
			    "Finding MSIX capability failed\n");
		} else {
			rid += PCIR_MSIX_CTRL;
			msix_ctrl = pci_read_config(dev, rid, 2);
			msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
			pci_write_config(dev, rid, msix_ctrl, 2);
		}
	}

	return (0);

fail:
	iflib_irq_free(ctx, &sc->irq);
	rx_que = sc->rx_queues;
	for (int i = 0; i < sc->num_rx_queues; i++, rx_que++)
		iflib_irq_free(ctx, &rx_que->que_irq);

	return (error);
} /* ixv_if_msix_intr_assign */

/************************************************************************
 * ixv_allocate_pci_resources
 ************************************************************************/
static int
ixv_allocate_pci_resources(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);
	int rid;

	rid = PCIR_BAR(0);
	sc->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);

	if (!(sc->pci_mem)) {
		device_printf(dev,
		    "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	sc->osdep.mem_bus_space_tag = rman_get_bustag(sc->pci_mem);
	sc->osdep.mem_bus_space_handle = rman_get_bushandle(sc->pci_mem);
	sc->hw.hw_addr = (u8 *)&sc->osdep.mem_bus_space_handle;

	return (0);
} /* ixv_allocate_pci_resources */

/************************************************************************
 * ixv_free_pci_resources
 ************************************************************************/
static void
ixv_free_pci_resources(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ix_rx_queue *que = sc->rx_queues;
	device_t dev = iflib_get_dev(ctx);

	/* Release all MSI-X queue resources */
	if (sc->intr_type == IFLIB_INTR_MSIX)
		iflib_irq_free(ctx, &sc->irq);

	if (que != NULL) {
		for (int i = 0; i < sc->num_rx_queues; i++, que++) {
			iflib_irq_free(ctx, &que->que_irq);
		}
	}

	if (sc->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->pci_mem), sc->pci_mem);
} /* ixv_free_pci_resources */

/************************************************************************
 * ixv_setup_interface
 *
 *   Setup networking device structure and register an interface.
 ************************************************************************/
static int
ixv_setup_interface(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx = sc->shared;
	if_t ifp = iflib_get_ifp(ctx);

	INIT_DEBUGOUT("ixv_setup_interface: begin");

	if_setbaudrate(ifp, IF_Gbps(10));
	if_setsendqlen(ifp, scctx->isc_ntxd[0] - 2);


	sc->max_frame_size = if_getmtu(ifp) + IXGBE_MTU_HDR;
	ifmedia_add(sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(sc->media, IFM_ETHER | IFM_AUTO);

	return 0;
} /* ixv_setup_interface */

/************************************************************************
 * ixv_if_get_counter
 ************************************************************************/
static uint64_t
ixv_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (sc->ipackets);
	case IFCOUNTER_OPACKETS:
		return (sc->opackets);
	case IFCOUNTER_IBYTES:
		return (sc->ibytes);
	case IFCOUNTER_OBYTES:
		return (sc->obytes);
	case IFCOUNTER_IMCASTS:
		return (sc->imcasts);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
} /* ixv_if_get_counter */

/* ixv_if_needs_restart - Tell iflib when the driver needs to be reinitialized
 * @ctx: iflib context
 * @event: event code to check
 *
 * Defaults to returning true for every event.
 *
 * @returns true if iflib needs to reinit the interface
 */
static bool
ixv_if_needs_restart(if_ctx_t ctx __unused, enum iflib_restart_event event)
{
	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
		/* XXX: This may not need to return true */
	default:
		return (true);
	}
}

/************************************************************************
 * ixv_initialize_transmit_units - Enable transmit unit.
 ************************************************************************/
static void
ixv_initialize_transmit_units(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	if_softc_ctx_t scctx = sc->shared;
	struct ix_tx_queue *que = sc->tx_queues;
	int i;

	for (i = 0; i < sc->num_tx_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		u64 tdba = txr->tx_paddr;
		u32 txctrl, txdctl;
		int j = txr->me;

		/* Set WTHRESH to 8, burst writeback */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(j));
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(j), txdctl);

		/* Set the HW Tx Head and Tail indices */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_VFTDH(j), 0);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_VFTDT(j), 0);

		/* Set Tx Tail register */
		txr->tail = IXGBE_VFTDT(j);

		txr->tx_rs_cidx = txr->tx_rs_pidx;
		/* Initialize the last processed descriptor to be the end of
		 * the ring, rather than the start, so that we avoid an
		 * off-by-one error when calculating how many descriptors are
		 * done in the credits_update function.
		 */
		txr->tx_cidx_processed = scctx->isc_ntxd[0] - 1;
		for (int k = 0; k < scctx->isc_ntxd[0]; k++)
			txr->tx_rsq[k] = QIDX_INVALID;

		/* Set Ring parameters */
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(j),
		    (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(j), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(j),
		    scctx->isc_ntxd[0] * sizeof(struct ixgbe_legacy_tx_desc));
		txctrl = IXGBE_READ_REG(hw, IXGBE_VFDCA_TXCTRL(j));
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(j), txctrl);

		/* Now enable */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(j));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(j), txdctl);
	}

	return;
} /* ixv_initialize_transmit_units */

/************************************************************************
 * ixv_initialize_rss_mapping
 ************************************************************************/
static void
ixv_initialize_rss_mapping(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	u32 reta = 0, mrqc, rss_key[10];
	int queue_id;
	int i, j;
	u32 rss_hash_config;

	if (sc->feat_en & IXGBE_FEATURE_RSS) {
		/* Fetch the configured RSS key */
		rss_getkey((uint8_t *)&rss_key);
	} else {
		/* set up random bits */
		arc4rand(&rss_key, sizeof(rss_key), 0);
	}

	/* Now fill out hash function seeds */
	for (i = 0; i < 10; i++)
		IXGBE_WRITE_REG(hw, IXGBE_VFRSSRK(i), rss_key[i]);

	/* Set up the redirection table */
	for (i = 0, j = 0; i < 64; i++, j++) {
		if (j == sc->num_rx_queues)
			j = 0;

		if (sc->feat_en & IXGBE_FEATURE_RSS) {
			/*
			 * Fetch the RSS bucket id for the given indirection
			 * entry. Cap it at the number of configured buckets
			 * (which is num_rx_queues.)
			 */
			queue_id = rss_get_indirection_to_bucket(i);
			queue_id = queue_id % sc->num_rx_queues;
		} else
			queue_id = j;

		/*
		 * The low 8 bits are for hash value (n+0);
		 * The next 8 bits are for hash value (n+1), etc.
		 */
		reta >>= 8;
		reta |= ((uint32_t)queue_id) << 24;
		if ((i & 3) == 3) {
			IXGBE_WRITE_REG(hw, IXGBE_VFRETA(i >> 2), reta);
			reta = 0;
		}
	}

	/* Perform hash on these packet types */
	if (sc->feat_en & IXGBE_FEATURE_RSS)
		rss_hash_config = rss_gethashconfig();
	else {
		/*
		 * Disable UDP - IP fragments aren't currently being handled
		 * and so we end up with a mix of 2-tuple and 4-tuple
		 * traffic.
		 */
		rss_hash_config = RSS_HASHTYPE_RSS_IPV4
		                | RSS_HASHTYPE_RSS_TCP_IPV4
		                | RSS_HASHTYPE_RSS_IPV6
		                | RSS_HASHTYPE_RSS_TCP_IPV6;
	}

	mrqc = IXGBE_MRQC_RSSEN;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6;
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_TCP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		device_printf(sc->dev,
		    "%s: RSS_HASHTYPE_RSS_IPV6_EX defined,"
		    " but not supported\n", __func__);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6_EX)
		device_printf(sc->dev,
		    "%s: RSS_HASHTYPE_RSS_TCP_IPV6_EX defined,"
		    " but not supported\n", __func__);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV4_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
		mrqc |= IXGBE_MRQC_RSS_FIELD_IPV6_UDP;
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6_EX)
		device_printf(sc->dev,
		    "%s: RSS_HASHTYPE_RSS_UDP_IPV6_EX defined,"
		    " but not supported\n", __func__);
	IXGBE_WRITE_REG(hw, IXGBE_VFMRQC, mrqc);
} /* ixv_initialize_rss_mapping */

#define BSIZEPKT_ROUNDUP ((1<<IXGBE_SRRCTL_BSIZEPKT_SHIFT)-1)
/************************************************************************
 * ixv_initialize_receive_units - Setup receive registers and features.
 ************************************************************************/
static void
ixv_initialize_receive_units(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	if_softc_ctx_t scctx;
	struct ixgbe_hw *hw = &sc->hw;
#ifdef DEV_NETMAP
	if_t ifp = iflib_get_ifp(ctx);
#endif
	struct ix_rx_queue *que = sc->rx_queues;
	u32 bufsz, psrtype;

	bufsz = (sc->rx_mbuf_sz + BSIZEPKT_ROUNDUP) >>
	    IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	psrtype = IXGBE_PSRTYPE_TCPHDR |
	    IXGBE_PSRTYPE_UDPHDR |
	    IXGBE_PSRTYPE_IPV4HDR |
	    IXGBE_PSRTYPE_IPV6HDR |
	    IXGBE_PSRTYPE_L2HDR;

	if (sc->num_rx_queues > 1)
		psrtype |= 1 << 29;

	IXGBE_WRITE_REG(hw, IXGBE_VFPSRTYPE, psrtype);

	/* Tell PF our max_frame size */
	if (ixgbevf_rlpml_set_vf(hw, sc->max_frame_size) != 0) {
		device_printf(sc->dev,
		    "There is a problem with the PF setup.  It is likely the"
		    " receive unit for this VF will not function correctly."
		    "\n");
	}
	scctx = sc->shared;

	for (int i = 0; i < sc->num_rx_queues; i++, que++) {
		struct rx_ring *rxr = &que->rxr;
		u64 rdba = rxr->rx_paddr;
		u32 reg, rxdctl;
		int j = rxr->me;

		/* Disable the queue */
		rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(j));
		rxdctl &= ~IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(j), rxdctl);
		for (int k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(j)) &
			    IXGBE_RXDCTL_ENABLE)
				msec_delay(1);
			else
				break;
		}
		wmb();
		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAL(j),
		    (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAH(j), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDLEN(j),
		    scctx->isc_nrxd[0] * sizeof(union ixgbe_adv_rx_desc));

		/* Reset the ring indices */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDH(rxr->me), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me), 0);

		/* Set up the SRRCTL register */
		reg = IXGBE_READ_REG(hw, IXGBE_VFSRRCTL(j));
		reg &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		reg &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		reg |= bufsz;
		reg |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
		IXGBE_WRITE_REG(hw, IXGBE_VFSRRCTL(j), reg);

		/* Capture Rx Tail index */
		rxr->tail = IXGBE_VFRDT(rxr->me);

		/* Do the queue enabling last */
		rxdctl |= IXGBE_RXDCTL_ENABLE | IXGBE_RXDCTL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(j), rxdctl);
		for (int l = 0; l < 10; l++) {
			if (IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(j)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			msec_delay(1);
		}
		wmb();

		/* Set the Tail Pointer */
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, we must preserve the buffers made
		 * available to userspace before the if_init()
		 * (this is true by default on the TX side, because
		 * init makes all buffers available to userspace).
		 *
		 * netmap_reset() and the device specific routines
		 * (e.g. ixgbe_setup_receive_rings()) map these
		 * buffers at the end of the NIC ring, so here we
		 * must set the RDT (tail) register to make sure
		 * they are not overwritten.
		 *
		 * In this driver the NIC ring starts at RDH = 0,
		 * RDT points to the last slot available for reception (?),
		 * so RDT = num_rx_desc - 1 means the whole ring is available.
		 */
		if (if_getcapenable(ifp) & IFCAP_NETMAP) {
			struct netmap_adapter *na = NA(ifp);
			struct netmap_kring *kring = na->rx_rings[j];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);

			IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me), t);
		} else
#endif /* DEV_NETMAP */
			IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me),
			    scctx->isc_nrxd[0] - 1);
	}

	/*
	 * Do not touch RSS and RETA settings for older hardware
	 * as those are shared among PF and all VF.
	 */
	if (sc->hw.mac.type >= ixgbe_mac_X550_vf)
		ixv_initialize_rss_mapping(sc);
} /* ixv_initialize_receive_units */

/************************************************************************
 * ixv_setup_vlan_support
 ************************************************************************/
static void
ixv_setup_vlan_support(if_ctx_t ctx)
{
	if_t ifp = iflib_get_ifp(ctx);
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	u32 ctrl, vid, vfta, retry;

	/*
	 * We get here thru if_init, meaning
	 * a soft reset, this has already cleared
	 * the VFTA and other state, so if there
	 * have been no vlan's registered do nothing.
	 */
	if (sc->num_vlans == 0)
		return;

	if (if_getcapenable(ifp) & IFCAP_VLAN_HWTAGGING) {
		/* Enable the queues */
		for (int i = 0; i < sc->num_rx_queues; i++) {
			ctrl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), ctrl);
			/*
			 * Let Rx path know that it needs to store VLAN tag
			 * as part of extra mbuf info.
			 */
			sc->rx_queues[i].rxr.vtag_strip = true;
		}
	}

	/*
	 * If filtering VLAN tags is disabled,
	 * there is no need to fill VLAN Filter Table Array (VFTA).
	 */
	if ((if_getcapenable(ifp) & IFCAP_VLAN_HWFILTER) == 0)
		return;

	/*
	 * A soft reset zero's out the VFTA, so
	 * we need to repopulate it now.
	 */
	for (int i = 0; i < IXGBE_VFTA_SIZE; i++) {
		if (sc->shadow_vfta[i] == 0)
			continue;
		vfta = sc->shadow_vfta[i];
		/*
		 * Reconstruct the vlan id's
		 * based on the bits set in each
		 * of the array ints.
		 */
		for (int j = 0; j < 32; j++) {
			retry = 0;
			if ((vfta & (1 << j)) == 0)
				continue;
			vid = (i * 32) + j;
			/* Call the shared code mailbox routine */
			while (hw->mac.ops.set_vfta(hw, vid, 0, true, false)) {
				if (++retry > 5)
					break;
			}
		}
	}
} /* ixv_setup_vlan_support */

/************************************************************************
 * ixv_if_register_vlan
 *
 *   Run via a vlan config EVENT, it enables us to use the
 *   HW Filter table since we can get the vlan id. This just
 *   creates the entry in the soft version of the VFTA, init
 *   will repopulate the real table.
 ************************************************************************/
static void
ixv_if_register_vlan(if_ctx_t ctx, u16 vtag)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	u16 index, bit;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	sc->shadow_vfta[index] |= (1 << bit);
	++sc->num_vlans;
} /* ixv_if_register_vlan */

/************************************************************************
 * ixv_if_unregister_vlan
 *
 *   Run via a vlan unconfig EVENT, remove our entry
 *   in the soft vfta.
 ************************************************************************/
static void
ixv_if_unregister_vlan(if_ctx_t ctx, u16 vtag)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	u16 index, bit;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	sc->shadow_vfta[index] &= ~(1 << bit);
	--sc->num_vlans;
} /* ixv_if_unregister_vlan */

/************************************************************************
 * ixv_if_enable_intr
 ************************************************************************/
static void
ixv_if_enable_intr(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ixgbe_hw *hw = &sc->hw;
	struct ix_rx_queue *que = sc->rx_queues;
	u32 mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);

	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);

	mask = IXGBE_EIMS_ENABLE_MASK;
	mask &= ~(IXGBE_EIMS_OTHER | IXGBE_EIMS_LSC);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAC, mask);

	for (int i = 0; i < sc->num_rx_queues; i++, que++)
		ixv_enable_queue(sc, que->msix);

	IXGBE_WRITE_FLUSH(hw);
} /* ixv_if_enable_intr */

/************************************************************************
 * ixv_if_disable_intr
 ************************************************************************/
static void
ixv_if_disable_intr(if_ctx_t ctx)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	IXGBE_WRITE_REG(&sc->hw, IXGBE_VTEIAC, 0);
	IXGBE_WRITE_REG(&sc->hw, IXGBE_VTEIMC, ~0);
	IXGBE_WRITE_FLUSH(&sc->hw);
} /* ixv_if_disable_intr */

/************************************************************************
 * ixv_if_rx_queue_intr_enable
 ************************************************************************/
static int
ixv_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct ixgbe_softc *sc = iflib_get_softc(ctx);
	struct ix_rx_queue *que = &sc->rx_queues[rxqid];

	ixv_enable_queue(sc, que->rxr.me);

	return (0);
} /* ixv_if_rx_queue_intr_enable */

/************************************************************************
 * ixv_set_ivar
 *
 *   Setup the correct IVAR register for a particular MSI-X interrupt
 *    - entry is the register array entry
 *    - vector is the MSI-X vector for this queue
 *    - type is RX/TX/MISC
 ************************************************************************/
static void
ixv_set_ivar(struct ixgbe_softc *sc, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &sc->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	if (type == -1) { /* MISC IVAR */
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR_MISC);
		ivar &= ~0xFF;
		ivar |= vector;
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR_MISC, ivar);
	} else {          /* RX/TX IVARS */
		index = (16 * (entry & 1)) + (8 * type);
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR(entry >> 1));
		ivar &= ~(0xFF << index);
		ivar |= (vector << index);
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR(entry >> 1), ivar);
	}
} /* ixv_set_ivar */

/************************************************************************
 * ixv_configure_ivars
 ************************************************************************/
static void
ixv_configure_ivars(struct ixgbe_softc *sc)
{
	struct ix_rx_queue *que = sc->rx_queues;

	MPASS(sc->num_rx_queues == sc->num_tx_queues);

	for (int i = 0; i < sc->num_rx_queues; i++, que++) {
		/* First the RX queue entry */
		ixv_set_ivar(sc, i, que->msix, 0);
		/* ... and the TX */
		ixv_set_ivar(sc, i, que->msix, 1);
		/* Set an initial value in EITR */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_VTEITR(que->msix),
		    IXGBE_EITR_DEFAULT);
	}

	/* For the mailbox interrupt */
	ixv_set_ivar(sc, 1, sc->vector, -1);
} /* ixv_configure_ivars */

/************************************************************************
 * ixv_save_stats
 *
 *   The VF stats registers never have a truly virgin
 *   starting point, so this routine tries to make an
 *   artificial one, marking ground zero on attach as
 *   it were.
 ************************************************************************/
static void
ixv_save_stats(struct ixgbe_softc *sc)
{
	if (sc->stats.vf.vfgprc || sc->stats.vf.vfgptc) {
		sc->stats.vf.saved_reset_vfgprc +=
		    sc->stats.vf.vfgprc - sc->stats.vf.base_vfgprc;
		sc->stats.vf.saved_reset_vfgptc +=
		    sc->stats.vf.vfgptc - sc->stats.vf.base_vfgptc;
		sc->stats.vf.saved_reset_vfgorc +=
		    sc->stats.vf.vfgorc - sc->stats.vf.base_vfgorc;
		sc->stats.vf.saved_reset_vfgotc +=
		    sc->stats.vf.vfgotc - sc->stats.vf.base_vfgotc;
		sc->stats.vf.saved_reset_vfmprc +=
		    sc->stats.vf.vfmprc - sc->stats.vf.base_vfmprc;
	}
} /* ixv_save_stats */

/************************************************************************
 * ixv_init_stats
 ************************************************************************/
static void
ixv_init_stats(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;

	sc->stats.vf.last_vfgprc = IXGBE_READ_REG(hw, IXGBE_VFGPRC);
	sc->stats.vf.last_vfgorc = IXGBE_READ_REG(hw, IXGBE_VFGORC_LSB);
	sc->stats.vf.last_vfgorc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGORC_MSB))) << 32);

	sc->stats.vf.last_vfgptc = IXGBE_READ_REG(hw, IXGBE_VFGPTC);
	sc->stats.vf.last_vfgotc = IXGBE_READ_REG(hw, IXGBE_VFGOTC_LSB);
	sc->stats.vf.last_vfgotc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGOTC_MSB))) << 32);

	sc->stats.vf.last_vfmprc = IXGBE_READ_REG(hw, IXGBE_VFMPRC);

	sc->stats.vf.base_vfgprc = sc->stats.vf.last_vfgprc;
	sc->stats.vf.base_vfgorc = sc->stats.vf.last_vfgorc;
	sc->stats.vf.base_vfgptc = sc->stats.vf.last_vfgptc;
	sc->stats.vf.base_vfgotc = sc->stats.vf.last_vfgotc;
	sc->stats.vf.base_vfmprc = sc->stats.vf.last_vfmprc;
} /* ixv_init_stats */

#define UPDATE_STAT_32(reg, last, count)                \
{                                                       \
	u32 current = IXGBE_READ_REG(hw, reg);          \
	if (current < last)                             \
		count += 0x100000000LL;                 \
	last = current;                                 \
	count &= 0xFFFFFFFF00000000LL;                  \
	count |= current;                               \
}

#define UPDATE_STAT_36(lsb, msb, last, count)           \
{                                                       \
	u64 cur_lsb = IXGBE_READ_REG(hw, lsb);          \
	u64 cur_msb = IXGBE_READ_REG(hw, msb);          \
	u64 current = ((cur_msb << 32) | cur_lsb);      \
	if (current < last)                             \
		count += 0x1000000000LL;                \
	last = current;                                 \
	count &= 0xFFFFFFF000000000LL;                  \
	count |= current;                               \
}

/************************************************************************
 * ixv_update_stats - Update the board statistics counters.
 ************************************************************************/
void
ixv_update_stats(struct ixgbe_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	struct ixgbevf_hw_stats *stats = &sc->stats.vf;

	UPDATE_STAT_32(IXGBE_VFGPRC, sc->stats.vf.last_vfgprc,
	    sc->stats.vf.vfgprc);
	UPDATE_STAT_32(IXGBE_VFGPTC, sc->stats.vf.last_vfgptc,
	    sc->stats.vf.vfgptc);
	UPDATE_STAT_36(IXGBE_VFGORC_LSB, IXGBE_VFGORC_MSB,
	    sc->stats.vf.last_vfgorc, sc->stats.vf.vfgorc);
	UPDATE_STAT_36(IXGBE_VFGOTC_LSB, IXGBE_VFGOTC_MSB,
	    sc->stats.vf.last_vfgotc, sc->stats.vf.vfgotc);
	UPDATE_STAT_32(IXGBE_VFMPRC, sc->stats.vf.last_vfmprc,
	    sc->stats.vf.vfmprc);

	/* Fill out the OS statistics structure */
	IXGBE_SET_IPACKETS(sc, stats->vfgprc);
	IXGBE_SET_OPACKETS(sc, stats->vfgptc);
	IXGBE_SET_IBYTES(sc, stats->vfgorc);
	IXGBE_SET_OBYTES(sc, stats->vfgotc);
	IXGBE_SET_IMCASTS(sc, stats->vfmprc);
} /* ixv_update_stats */

/************************************************************************
 * ixv_add_stats_sysctls - Add statistic sysctls for the VF.
 ************************************************************************/
static void
ixv_add_stats_sysctls(struct ixgbe_softc *sc)
{
	device_t dev = sc->dev;
	struct ix_tx_queue *tx_que = sc->tx_queues;
	struct ix_rx_queue *rx_que = sc->rx_queues;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct ixgbevf_hw_stats *stats = &sc->stats.vf;
	struct sysctl_oid *stat_node, *queue_node;
	struct sysctl_oid_list *stat_list, *queue_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
	    CTLFLAG_RD, &sc->watchdog_events, "Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "link_irq",
	    CTLFLAG_RD, &sc->link_irq, "Link MSI-X IRQ Handled");

	for (int i = 0; i < sc->num_tx_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tso_tx",
		    CTLFLAG_RD, &(txr->tso_tx), "TSO Packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_packets",
		    CTLFLAG_RD, &(txr->total_packets), "TX Packets");
	}

	for (int i = 0; i < sc->num_rx_queues; i++, rx_que++) {
		struct rx_ring *rxr = &rx_que->rxr;
		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
		    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
		    CTLFLAG_RD, &(rx_que->irqs), "IRQs on queue");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
		    CTLFLAG_RD, &(rxr->rx_packets), "RX packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
		    CTLFLAG_RD, &(rxr->rx_bytes), "RX bytes");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_discarded",
		    CTLFLAG_RD, &(rxr->rx_discarded), "Discarded RX packets");
	}

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "VF Statistics (read from HW registers)");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_rcvd",
	    CTLFLAG_RD, &stats->vfgprc, "Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_rcvd",
	    CTLFLAG_RD, &stats->vfgorc, "Good Octets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_rcvd",
	    CTLFLAG_RD, &stats->vfmprc, "Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
	    CTLFLAG_RD, &stats->vfgptc, "Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
	    CTLFLAG_RD, &stats->vfgotc, "Good Octets Transmitted");
} /* ixv_add_stats_sysctls */

/************************************************************************
 * ixv_print_debug_info
 *
 *   Called only when em_display_debug_stats is enabled.
 *   Provides a way to take a look at important statistics
 *   maintained by the driver and hardware.
 ************************************************************************/
static void
ixv_print_debug_info(struct ixgbe_softc *sc)
{
	device_t dev = sc->dev;
	struct ixgbe_hw *hw = &sc->hw;

	device_printf(dev, "Error Byte Count = %u \n",
	    IXGBE_READ_REG(hw, IXGBE_ERRBC));

	device_printf(dev, "MBX IRQ Handled: %lu\n", (long)sc->link_irq);
} /* ixv_print_debug_info */

/************************************************************************
 * ixv_sysctl_debug
 ************************************************************************/
static int
ixv_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	struct ixgbe_softc *sc;
	int error, result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct ixgbe_softc *)arg1;
		ixv_print_debug_info(sc);
	}

	return error;
} /* ixv_sysctl_debug */

/************************************************************************
 * ixv_init_device_features
 ************************************************************************/
static void
ixv_init_device_features(struct ixgbe_softc *sc)
{
	sc->feat_cap = IXGBE_FEATURE_NETMAP |
	    IXGBE_FEATURE_VF |
	    IXGBE_FEATURE_LEGACY_TX;

	/* A tad short on feature flags for VFs, atm. */
	switch (sc->hw.mac.type) {
	case ixgbe_mac_82599_vf:
		break;
	case ixgbe_mac_X540_vf:
		break;
	case ixgbe_mac_X550_vf:
	case ixgbe_mac_X550EM_x_vf:
	case ixgbe_mac_X550EM_a_vf:
		sc->feat_cap |= IXGBE_FEATURE_NEEDS_CTXD;
		sc->feat_cap |= IXGBE_FEATURE_RSS;
		break;
	default:
		break;
	}

	/* Enabled by default... */
	/* Is a virtual function (VF) */
	if (sc->feat_cap & IXGBE_FEATURE_VF)
		sc->feat_en |= IXGBE_FEATURE_VF;
	/* Netmap */
	if (sc->feat_cap & IXGBE_FEATURE_NETMAP)
		sc->feat_en |= IXGBE_FEATURE_NETMAP;
	/* Receive-Side Scaling (RSS) */
	if (sc->feat_cap & IXGBE_FEATURE_RSS)
		sc->feat_en |= IXGBE_FEATURE_RSS;
	/* Needs advanced context descriptor regardless of offloads req'd */
	if (sc->feat_cap & IXGBE_FEATURE_NEEDS_CTXD)
		sc->feat_en |= IXGBE_FEATURE_NEEDS_CTXD;
} /* ixv_init_device_features */

