/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file if_ice_iflib.c
 * @brief iflib driver implementation
 *
 * Contains the main entry point for the iflib driver implementation. It
 * implements the various ifdi driver methods, and sets up the module and
 * driver values to load an iflib driver.
 */

#include "ice_iflib.h"
#include "ice_drv_info.h"
#include "ice_switch.h"
#include "ice_sched.h"

#include <sys/module.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

/*
 * Device method prototypes
 */

static void *ice_register(device_t);
static int  ice_if_attach_pre(if_ctx_t);
static int  ice_attach_pre_recovery_mode(struct ice_softc *sc);
static int  ice_if_attach_post(if_ctx_t);
static void ice_attach_post_recovery_mode(struct ice_softc *sc);
static int  ice_if_detach(if_ctx_t);
static int  ice_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int ntxqs, int ntxqsets);
static int  ice_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int nqs, int nqsets);
static int ice_if_msix_intr_assign(if_ctx_t ctx, int msix);
static void ice_if_queues_free(if_ctx_t ctx);
static int ice_if_mtu_set(if_ctx_t ctx, uint32_t mtu);
static void ice_if_intr_enable(if_ctx_t ctx);
static void ice_if_intr_disable(if_ctx_t ctx);
static int ice_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid);
static int ice_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid);
static int ice_if_promisc_set(if_ctx_t ctx, int flags);
static void ice_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr);
static int ice_if_media_change(if_ctx_t ctx);
static void ice_if_init(if_ctx_t ctx);
static void ice_if_timer(if_ctx_t ctx, uint16_t qid);
static void ice_if_update_admin_status(if_ctx_t ctx);
static void ice_if_multi_set(if_ctx_t ctx);
static void ice_if_vlan_register(if_ctx_t ctx, u16 vtag);
static void ice_if_vlan_unregister(if_ctx_t ctx, u16 vtag);
static void ice_if_stop(if_ctx_t ctx);
static uint64_t ice_if_get_counter(if_ctx_t ctx, ift_counter counter);
static int ice_if_priv_ioctl(if_ctx_t ctx, u_long command, caddr_t data);
static int ice_if_i2c_req(if_ctx_t ctx, struct ifi2creq *req);
static int ice_if_suspend(if_ctx_t ctx);
static int ice_if_resume(if_ctx_t ctx);
static bool ice_if_needs_restart(if_ctx_t ctx, enum iflib_restart_event event);
static void ice_init_link(struct ice_softc *sc);
static int ice_setup_mirror_vsi(struct ice_mirr_if *mif);
static int ice_wire_mirror_intrs(struct ice_mirr_if *mif);
static void ice_free_irqvs_subif(struct ice_mirr_if *mif);
static void *ice_subif_register(device_t);
static void ice_subif_setup_scctx(struct ice_mirr_if *mif);
static int ice_subif_rebuild(struct ice_softc *sc);
static int ice_subif_rebuild_vsi_qmap(struct ice_softc *sc);

/* Iflib API */
static int ice_subif_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int ntxqs, int ntxqsets);
static int ice_subif_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs,
    uint64_t *paddrs, int nrxqs, int nrxqsets);
static int ice_subif_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid);
static int ice_subif_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid);
static void ice_subif_if_intr_enable(if_ctx_t ctx);
static int ice_subif_if_msix_intr_assign(if_ctx_t ctx, int msix);
static void ice_subif_if_init(if_ctx_t ctx);
static void ice_subif_if_stop(if_ctx_t ctx);
static void ice_subif_if_queues_free(if_ctx_t ctx);
static int ice_subif_if_attach_pre(if_ctx_t);
static int ice_subif_if_attach_post(if_ctx_t);
static void ice_subif_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr);
static int ice_subif_if_promisc_set(if_ctx_t ctx, int flags);

static int ice_msix_que(void *arg);
static int ice_msix_admin(void *arg);

/*
 * Helper function prototypes
 */
static int ice_pci_mapping(struct ice_softc *sc);
static void ice_free_pci_mapping(struct ice_softc *sc);
static void ice_update_link_status(struct ice_softc *sc, bool update_media);
static void ice_init_device_features(struct ice_softc *sc);
static void ice_init_tx_tracking(struct ice_vsi *vsi);
static void ice_handle_reset_event(struct ice_softc *sc);
static void ice_handle_pf_reset_request(struct ice_softc *sc);
static void ice_prepare_for_reset(struct ice_softc *sc);
static int ice_rebuild_pf_vsi_qmap(struct ice_softc *sc);
static void ice_rebuild(struct ice_softc *sc);
static void ice_rebuild_recovery_mode(struct ice_softc *sc);
static void ice_free_irqvs(struct ice_softc *sc);
static void ice_update_rx_mbuf_sz(struct ice_softc *sc);
static void ice_poll_for_media_avail(struct ice_softc *sc);
static void ice_setup_scctx(struct ice_softc *sc);
static int ice_allocate_msix(struct ice_softc *sc);
static void ice_admin_timer(void *arg);
static void ice_transition_recovery_mode(struct ice_softc *sc);
static void ice_transition_safe_mode(struct ice_softc *sc);
static void ice_set_default_promisc_mask(ice_bitmap_t *promisc_mask);

/*
 * Device Interface Declaration
 */

/**
 * @var ice_methods
 * @brief ice driver method entry points
 *
 * List of device methods implementing the generic device interface used by
 * the device stack to interact with the ice driver. Since this is an iflib
 * driver, most of the methods point to the generic iflib implementation.
 */
static device_method_t ice_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, ice_register),
	DEVMETHOD(device_probe,    iflib_device_probe_vendor),
	DEVMETHOD(device_attach,   iflib_device_attach),
	DEVMETHOD(device_detach,   iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD(device_suspend,  iflib_device_suspend),
	DEVMETHOD(device_resume,   iflib_device_resume),
	DEVMETHOD_END
};

/**
 * @var ice_iflib_methods
 * @brief iflib method entry points
 *
 * List of device methods used by the iflib stack to interact with this
 * driver. These are the real main entry points used to interact with this
 * driver.
 */
static device_method_t ice_iflib_methods[] = {
	DEVMETHOD(ifdi_attach_pre, ice_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, ice_if_attach_post),
	DEVMETHOD(ifdi_detach, ice_if_detach),
	DEVMETHOD(ifdi_tx_queues_alloc, ice_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, ice_if_rx_queues_alloc),
	DEVMETHOD(ifdi_msix_intr_assign, ice_if_msix_intr_assign),
	DEVMETHOD(ifdi_queues_free, ice_if_queues_free),
	DEVMETHOD(ifdi_mtu_set, ice_if_mtu_set),
	DEVMETHOD(ifdi_intr_enable, ice_if_intr_enable),
	DEVMETHOD(ifdi_intr_disable, ice_if_intr_disable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, ice_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, ice_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_promisc_set, ice_if_promisc_set),
	DEVMETHOD(ifdi_media_status, ice_if_media_status),
	DEVMETHOD(ifdi_media_change, ice_if_media_change),
	DEVMETHOD(ifdi_init, ice_if_init),
	DEVMETHOD(ifdi_stop, ice_if_stop),
	DEVMETHOD(ifdi_timer, ice_if_timer),
	DEVMETHOD(ifdi_update_admin_status, ice_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, ice_if_multi_set),
	DEVMETHOD(ifdi_vlan_register, ice_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, ice_if_vlan_unregister),
	DEVMETHOD(ifdi_get_counter, ice_if_get_counter),
	DEVMETHOD(ifdi_priv_ioctl, ice_if_priv_ioctl),
	DEVMETHOD(ifdi_i2c_req, ice_if_i2c_req),
	DEVMETHOD(ifdi_suspend, ice_if_suspend),
	DEVMETHOD(ifdi_resume, ice_if_resume),
	DEVMETHOD(ifdi_needs_restart, ice_if_needs_restart),
	DEVMETHOD_END
};

/**
 * @var ice_driver
 * @brief driver structure for the generic device stack
 *
 * driver_t definition used to setup the generic device methods.
 */
static driver_t ice_driver = {
	.name = "ice",
	.methods = ice_methods,
	.size = sizeof(struct ice_softc),
};

/**
 * @var ice_iflib_driver
 * @brief driver structure for the iflib stack
 *
 * driver_t definition used to setup the iflib device methods.
 */
static driver_t ice_iflib_driver = {
	.name = "ice",
	.methods = ice_iflib_methods,
	.size = sizeof(struct ice_softc),
};

extern struct if_txrx ice_txrx;
extern struct if_txrx ice_recovery_txrx;

/**
 * @var ice_sctx
 * @brief ice driver shared context
 *
 * Structure defining shared values (context) that is used by all instances of
 * the device. Primarily used to setup details about how the iflib stack
 * should treat this driver. Also defines the default, minimum, and maximum
 * number of descriptors in each ring.
 */
static struct if_shared_ctx ice_sctx = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,

	.isc_tx_maxsize = ICE_MAX_FRAME_SIZE,
	/* We could technically set this as high as ICE_MAX_DMA_SEG_SIZE, but
	 * that doesn't make sense since that would be larger than the maximum
	 * size of a single packet.
	 */
	.isc_tx_maxsegsize = ICE_MAX_FRAME_SIZE,

	/* XXX: This is only used by iflib to ensure that
	 * scctx->isc_tx_tso_size_max + the VLAN header is a valid size.
	 */
	.isc_tso_maxsize = ICE_TSO_SIZE + sizeof(struct ether_vlan_header),
	/* XXX: This is used by iflib to set the number of segments in the TSO
	 * DMA tag. However, scctx->isc_tx_tso_segsize_max is used to set the
	 * related ifnet parameter.
	 */
	.isc_tso_maxsegsize = ICE_MAX_DMA_SEG_SIZE,

	.isc_rx_maxsize = ICE_MAX_FRAME_SIZE,
	.isc_rx_nsegments = ICE_MAX_RX_SEGS,
	.isc_rx_maxsegsize = ICE_MAX_FRAME_SIZE,

	.isc_nfl = 1,
	.isc_ntxqs = 1,
	.isc_nrxqs = 1,

	.isc_admin_intrcnt = 1,
	.isc_vendor_info = ice_vendor_info_array,
	.isc_driver_version = __DECONST(char *, ice_driver_version),
	.isc_driver = &ice_iflib_driver,

	/*
	 * IFLIB_NEED_SCRATCH ensures that mbufs have scratch space available
	 * for hardware checksum offload
	 *
	 * IFLIB_TSO_INIT_IP ensures that the TSO packets have zeroed out the
	 * IP sum field, required by our hardware to calculate valid TSO
	 * checksums.
	 *
	 * IFLIB_ADMIN_ALWAYS_RUN ensures that the administrative task runs
	 * even when the interface is down.
	 *
	 * IFLIB_SKIP_MSIX allows the driver to handle allocating MSI-X
	 * vectors manually instead of relying on iflib code to do this.
	 */
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP |
		IFLIB_ADMIN_ALWAYS_RUN | IFLIB_SKIP_MSIX,

	.isc_nrxd_min = {ICE_MIN_DESC_COUNT},
	.isc_ntxd_min = {ICE_MIN_DESC_COUNT},
	.isc_nrxd_max = {ICE_IFLIB_MAX_DESC_COUNT},
	.isc_ntxd_max = {ICE_IFLIB_MAX_DESC_COUNT},
	.isc_nrxd_default = {ICE_DEFAULT_DESC_COUNT},
	.isc_ntxd_default = {ICE_DEFAULT_DESC_COUNT},
};

DRIVER_MODULE(ice, pci, ice_driver, ice_module_event_handler, NULL);

MODULE_VERSION(ice, 1);
MODULE_DEPEND(ice, pci, 1, 1, 1);
MODULE_DEPEND(ice, ether, 1, 1, 1);
MODULE_DEPEND(ice, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, ice, ice_vendor_info_array);

/* Static driver-wide sysctls */
#include "ice_iflib_sysctls.h"

/**
 * ice_pci_mapping - Map PCI BAR memory
 * @sc: device private softc
 *
 * Map PCI BAR 0 for device operation.
 */
static int
ice_pci_mapping(struct ice_softc *sc)
{
	int rc;

	/* Map BAR0 */
	rc = ice_map_bar(sc->dev, &sc->bar0, 0);
	if (rc)
		return rc;

	return 0;
}

/**
 * ice_free_pci_mapping - Release PCI BAR memory
 * @sc: device private softc
 *
 * Release PCI BARs which were previously mapped by ice_pci_mapping().
 */
static void
ice_free_pci_mapping(struct ice_softc *sc)
{
	/* Free BAR0 */
	ice_free_bar(sc->dev, &sc->bar0);
}

/*
 * Device methods
 */

/**
 * ice_register - register device method callback
 * @dev: the device being registered
 *
 * Returns a pointer to the shared context structure, which is used by iflib.
 */
static void *
ice_register(device_t dev __unused)
{
	return &ice_sctx;
} /* ice_register */

/**
 * ice_setup_scctx - Setup the iflib softc context structure
 * @sc: the device private structure
 *
 * Setup the parameters in if_softc_ctx_t structure used by the iflib stack
 * when loading.
 */
static void
ice_setup_scctx(struct ice_softc *sc)
{
	if_softc_ctx_t scctx = sc->scctx;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	bool safe_mode, recovery_mode;

	safe_mode = ice_is_bit_set(sc->feat_en, ICE_FEATURE_SAFE_MODE);
	recovery_mode = ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE);

	/*
	 * If the driver loads in Safe mode or Recovery mode, limit iflib to
	 * a single queue pair.
	 */
	if (safe_mode || recovery_mode) {
		scctx->isc_ntxqsets = scctx->isc_nrxqsets = 1;
		scctx->isc_ntxqsets_max = 1;
		scctx->isc_nrxqsets_max = 1;
	} else {
		/*
		 * iflib initially sets the isc_ntxqsets and isc_nrxqsets to
		 * the values of the override sysctls. Cache these initial
		 * values so that the driver can be aware of what the iflib
		 * sysctl value is when setting up MSI-X vectors.
		 */
		sc->ifc_sysctl_ntxqs = scctx->isc_ntxqsets;
		sc->ifc_sysctl_nrxqs = scctx->isc_nrxqsets;

		if (scctx->isc_ntxqsets == 0)
			scctx->isc_ntxqsets = hw->func_caps.common_cap.rss_table_size;
		if (scctx->isc_nrxqsets == 0)
			scctx->isc_nrxqsets = hw->func_caps.common_cap.rss_table_size;

		scctx->isc_ntxqsets_max = hw->func_caps.common_cap.num_txq;
		scctx->isc_nrxqsets_max = hw->func_caps.common_cap.num_rxq;

		/*
		 * Sanity check that the iflib sysctl values are within the
		 * maximum supported range.
		 */
		if (sc->ifc_sysctl_ntxqs > scctx->isc_ntxqsets_max)
			sc->ifc_sysctl_ntxqs = scctx->isc_ntxqsets_max;
		if (sc->ifc_sysctl_nrxqs > scctx->isc_nrxqsets_max)
			sc->ifc_sysctl_nrxqs = scctx->isc_nrxqsets_max;
	}

	scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0]
	    * sizeof(struct ice_tx_desc), DBA_ALIGN);
	scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0]
	    * sizeof(union ice_32b_rx_flex_desc), DBA_ALIGN);

	scctx->isc_tx_nsegments = ICE_MAX_TX_SEGS;
	scctx->isc_tx_tso_segments_max = ICE_MAX_TSO_SEGS;
	scctx->isc_tx_tso_size_max = ICE_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = ICE_MAX_DMA_SEG_SIZE;

	scctx->isc_msix_bar = pci_msix_table_bar(dev);
	scctx->isc_rss_table_size = hw->func_caps.common_cap.rss_table_size;

	/*
	 * If the driver loads in recovery mode, disable Tx/Rx functionality
	 */
	if (recovery_mode)
		scctx->isc_txrx = &ice_recovery_txrx;
	else
		scctx->isc_txrx = &ice_txrx;

	/*
	 * If the driver loads in Safe mode or Recovery mode, disable
	 * advanced features including hardware offloads.
	 */
	if (safe_mode || recovery_mode) {
		scctx->isc_capenable = ICE_SAFE_CAPS;
		scctx->isc_tx_csum_flags = 0;
	} else {
		scctx->isc_capenable = ICE_FULL_CAPS;
		scctx->isc_tx_csum_flags = ICE_CSUM_OFFLOAD;
	}

	scctx->isc_capabilities = scctx->isc_capenable;
} /* ice_setup_scctx */

/**
 * ice_if_attach_pre - Early device attach logic
 * @ctx: the iflib context structure
 *
 * Called by iflib during the attach process. Earliest main driver entry
 * point which performs necessary hardware and driver initialization. Called
 * before the Tx and Rx queues are allocated.
 */
static int
ice_if_attach_pre(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	enum ice_fw_modes fw_mode;
	int status;
	if_softc_ctx_t scctx;
	struct ice_hw *hw;
	device_t dev;
	int err;

	device_printf(iflib_get_dev(ctx), "Loading the iflib ice driver\n");

	ice_set_state(&sc->state, ICE_STATE_ATTACHING);

	sc->ctx = ctx;
	sc->media = iflib_get_media(ctx);
	sc->sctx = iflib_get_sctx(ctx);
	sc->iflib_ctx_lock = iflib_ctx_lock_get(ctx);
	sc->ifp = iflib_get_ifp(ctx);

	dev = sc->dev = iflib_get_dev(ctx);
	scctx = sc->scctx = iflib_get_softc_ctx(ctx);

	hw = &sc->hw;
	hw->back = sc;

	snprintf(sc->admin_mtx_name, sizeof(sc->admin_mtx_name),
		 "%s:admin", device_get_nameunit(dev));
	mtx_init(&sc->admin_mtx, sc->admin_mtx_name, NULL, MTX_DEF);
	callout_init_mtx(&sc->admin_timer, &sc->admin_mtx, 0);

	ASSERT_CTX_LOCKED(sc);

	if (ice_pci_mapping(sc)) {
		err = (ENXIO);
		goto destroy_admin_timer;
	}

	/* Save off the PCI information */
	ice_save_pci_info(hw, dev);

	/* create tunables as early as possible */
	ice_add_device_tunables(sc);

	/* Setup ControlQ lengths */
	ice_set_ctrlq_len(hw);

reinit_hw:

	fw_mode = ice_get_fw_mode(hw);
	if (fw_mode == ICE_FW_MODE_REC) {
		device_printf(dev, "Firmware recovery mode detected. Limiting functionality. Refer to Intel(R) Ethernet Adapters and Devices User Guide for details on firmware recovery mode.\n");

		err = ice_attach_pre_recovery_mode(sc);
		if (err)
			goto free_pci_mapping;

		return (0);
	}

	/* Initialize the hw data structure */
	status = ice_init_hw(hw);
	if (status) {
		if (status == ICE_ERR_FW_API_VER) {
			/* Enter recovery mode, so that the driver remains
			 * loaded. This way, if the system administrator
			 * cannot update the driver, they may still attempt to
			 * downgrade the NVM.
			 */
			err = ice_attach_pre_recovery_mode(sc);
			if (err)
				goto free_pci_mapping;

			return (0);
		} else {
			err = EIO;
			device_printf(dev, "Unable to initialize hw, err %s aq_err %s\n",
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
		}
		goto free_pci_mapping;
	}

	ice_init_device_features(sc);

	/* Keep flag set by default */
	ice_set_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN);

	/* Notify firmware of the device driver version */
	err = ice_send_version(sc);
	if (err)
		goto deinit_hw;

	/*
	 * Success indicates a change was made that requires a reinitialization
	 * of the hardware
	 */
	err = ice_load_pkg_file(sc);
	if (!err) {
		ice_deinit_hw(hw);
		goto reinit_hw;
	}

	err = ice_init_link_events(sc);
	if (err) {
		device_printf(dev, "ice_init_link_events failed: %s\n",
			      ice_err_str(err));
		goto deinit_hw;
	}

	/* Initialize VLAN mode in FW; if dual VLAN mode is supported by the package
	 * and firmware, this will force them to use single VLAN mode.
	 */
	status = ice_set_vlan_mode(hw);
	if (status) {
		err = EIO;
		device_printf(dev, "Unable to initialize VLAN mode, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		goto deinit_hw;
	}

	ice_print_nvm_version(sc);

	/* Setup the MAC address */
	iflib_set_mac(ctx, hw->port_info->mac.lan_addr);

	/* Setup the iflib softc context structure */
	ice_setup_scctx(sc);

	/* Initialize the Tx queue manager */
	err = ice_resmgr_init(&sc->tx_qmgr, hw->func_caps.common_cap.num_txq);
	if (err) {
		device_printf(dev, "Unable to initialize Tx queue manager: %s\n",
			      ice_err_str(err));
		goto deinit_hw;
	}

	/* Initialize the Rx queue manager */
	err = ice_resmgr_init(&sc->rx_qmgr, hw->func_caps.common_cap.num_rxq);
	if (err) {
		device_printf(dev, "Unable to initialize Rx queue manager: %s\n",
			      ice_err_str(err));
		goto free_tx_qmgr;
	}

	/* Initialize the PF device interrupt resource manager */
	err = ice_alloc_intr_tracking(sc);
	if (err)
		/* Errors are already printed */
		goto free_rx_qmgr;

	/* Determine maximum number of VSIs we'll prepare for */
	sc->num_available_vsi = min(ICE_MAX_VSI_AVAILABLE,
				    hw->func_caps.guar_num_vsi);

	if (!sc->num_available_vsi) {
		err = EIO;
		device_printf(dev, "No VSIs allocated to host\n");
		goto free_intr_tracking;
	}

	/* Allocate storage for the VSI pointers */
	sc->all_vsi = (struct ice_vsi **)
		malloc(sizeof(struct ice_vsi *) * sc->num_available_vsi,
		       M_ICE, M_WAITOK | M_ZERO);
	if (!sc->all_vsi) {
		err = ENOMEM;
		device_printf(dev, "Unable to allocate VSI array\n");
		goto free_intr_tracking;
	}

	/*
	 * Prepare the statically allocated primary PF VSI in the softc
	 * structure. Other VSIs will be dynamically allocated as needed.
	 */
	ice_setup_pf_vsi(sc);

	ice_alloc_vsi_qmap(&sc->pf_vsi, scctx->isc_ntxqsets_max,
	    scctx->isc_nrxqsets_max);

	/* Allocate MSI-X vectors (due to isc_flags IFLIB_SKIP_MSIX) */
	err = ice_allocate_msix(sc);
	if (err)
		goto free_main_vsi;

	return 0;

free_main_vsi:
	/* ice_release_vsi will free the queue maps if they were allocated */
	ice_release_vsi(&sc->pf_vsi);
	free(sc->all_vsi, M_ICE);
	sc->all_vsi = NULL;
free_intr_tracking:
	ice_free_intr_tracking(sc);
free_rx_qmgr:
	ice_resmgr_destroy(&sc->rx_qmgr);
free_tx_qmgr:
	ice_resmgr_destroy(&sc->tx_qmgr);
deinit_hw:
	ice_deinit_hw(hw);
free_pci_mapping:
	ice_free_pci_mapping(sc);
destroy_admin_timer:
	mtx_lock(&sc->admin_mtx);
	callout_stop(&sc->admin_timer);
	mtx_unlock(&sc->admin_mtx);
	mtx_destroy(&sc->admin_mtx);
	return err;
} /* ice_if_attach_pre */

/**
 * ice_attach_pre_recovery_mode - Limited driver attach_pre for FW recovery
 * @sc: the device private softc
 *
 * Loads the device driver in limited Firmware Recovery mode, intended to
 * allow users to update the firmware to attempt to recover the device.
 *
 * @remark We may enter recovery mode in case either (a) the firmware is
 * detected to be in an invalid state and must be re-programmed, or (b) the
 * driver detects that the loaded firmware has a non-compatible API version
 * that the driver cannot operate with.
 */
static int
ice_attach_pre_recovery_mode(struct ice_softc *sc)
{
	ice_set_state(&sc->state, ICE_STATE_RECOVERY_MODE);

	/* Setup the iflib softc context */
	ice_setup_scctx(sc);

	/* Setup the PF VSI back pointer */
	sc->pf_vsi.sc = sc;

	/*
	 * We still need to allocate MSI-X vectors since we need one vector to
	 * run the administrative admin interrupt
	 */
	return ice_allocate_msix(sc);
}

/**
 * ice_update_link_status - notify OS of link state change
 * @sc: device private softc structure
 * @update_media: true if we should update media even if link didn't change
 *
 * Called to notify iflib core of link status changes. Should be called once
 * during attach_post, and whenever link status changes during runtime.
 *
 * This call only updates the currently supported media types if the link
 * status changed, or if update_media is set to true.
 */
static void
ice_update_link_status(struct ice_softc *sc, bool update_media)
{
	struct ice_hw *hw = &sc->hw;
	int status;

	/* Never report link up when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	/* Report link status to iflib only once each time it changes */
	if (!ice_testandset_state(&sc->state, ICE_STATE_LINK_STATUS_REPORTED)) {
		if (sc->link_up) { /* link is up */
			uint64_t baudrate = ice_aq_speed_to_rate(sc->hw.port_info);

			if (!(hw->port_info->phy.link_info_old.link_info & ICE_AQ_LINK_UP))
				ice_set_default_local_lldp_mib(sc);

			iflib_link_state_change(sc->ctx, LINK_STATE_UP, baudrate);
			ice_rdma_link_change(sc, LINK_STATE_UP, baudrate);

			ice_link_up_msg(sc);
		} else { /* link is down */
			iflib_link_state_change(sc->ctx, LINK_STATE_DOWN, 0);
			ice_rdma_link_change(sc, LINK_STATE_DOWN, 0);
		}
		update_media = true;
	}

	/* Update the supported media types */
	if (update_media && !ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		status = ice_add_media_types(sc, sc->media);
		if (status)
			device_printf(sc->dev, "Error adding device media types: %s aq_err %s\n",
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
	}
}

/**
 * ice_if_attach_post - Late device attach logic
 * @ctx: the iflib context structure
 *
 * Called by iflib to finish up attaching the device. Performs any attach
 * logic which must wait until after the Tx and Rx queues have been
 * allocated.
 */
static int
ice_if_attach_post(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	if_t ifp = iflib_get_ifp(ctx);
	int status;
	int err;

	ASSERT_CTX_LOCKED(sc);

	/* We don't yet support loading if MSI-X is not supported */
	if (sc->scctx->isc_intr != IFLIB_INTR_MSIX) {
		device_printf(sc->dev, "The ice driver does not support loading without MSI-X\n");
		return (ENOTSUP);
	}

	/* The ifnet structure hasn't yet been initialized when the attach_pre
	 * handler is called, so wait until attach_post to setup the
	 * isc_max_frame_size.
	 */
	sc->scctx->isc_max_frame_size = if_getmtu(ifp) +
		ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN;

	/*
	 * If we are in recovery mode, only perform a limited subset of
	 * initialization to support NVM recovery.
	 */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE)) {
		ice_attach_post_recovery_mode(sc);
		return (0);
	}

	sc->pf_vsi.max_frame_size = sc->scctx->isc_max_frame_size;

	err = ice_initialize_vsi(&sc->pf_vsi);
	if (err) {
		device_printf(sc->dev, "Unable to initialize Main VSI: %s\n",
			      ice_err_str(err));
		return err;
	}

	/* Enable FW health event reporting */
	ice_init_health_events(sc);

	/* Configure the main PF VSI for RSS */
	err = ice_config_rss(&sc->pf_vsi);
	if (err) {
		device_printf(sc->dev,
			      "Unable to configure RSS for the main VSI, err %s\n",
			      ice_err_str(err));
		return err;
	}

	/* Configure switch to drop transmitted LLDP and PAUSE frames */
	err = ice_cfg_pf_ethertype_filters(sc);
	if (err)
		return err;

	ice_get_and_print_bus_info(sc);

	ice_set_link_management_mode(sc);

	ice_init_saved_phy_cfg(sc);

	ice_cfg_pba_num(sc);

	/* Set a default value for PFC mode on attach since the FW state is unknown
	 * before sysctl tunables are executed and it can't be queried. This fixes an
	 * issue when loading the driver with the FW LLDP agent enabled but the FW
	 * was previously in DSCP PFC mode.
	 */
	status = ice_aq_set_pfc_mode(&sc->hw, ICE_AQC_PFC_VLAN_BASED_PFC, NULL);
	if (status)
		device_printf(sc->dev, "Setting pfc mode failed, status %s\n", ice_status_str(status));

	ice_add_device_sysctls(sc);

	/* Get DCBX/LLDP state and start DCBX agent */
	ice_init_dcb_setup(sc);

	/* Setup link, if PHY FW is ready */
	ice_init_link(sc);

	/* Configure interrupt causes for the administrative interrupt */
	ice_configure_misc_interrupts(sc);

	/* Enable ITR 0 right away, so that we can handle admin interrupts */
	ice_enable_intr(&sc->hw, sc->irqvs[0].me);

	err = ice_rdma_pf_attach(sc);
	if (err)
		return (err);

	/* Start the admin timer */
	mtx_lock(&sc->admin_mtx);
	callout_reset(&sc->admin_timer, hz/2, ice_admin_timer, sc);
	mtx_unlock(&sc->admin_mtx);

	if (ice_test_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN) &&
		 !ice_test_state(&sc->state, ICE_STATE_NO_MEDIA))
		ice_set_state(&sc->state, ICE_STATE_FIRST_INIT_LINK);

	ice_clear_state(&sc->state, ICE_STATE_ATTACHING);

	return 0;
} /* ice_if_attach_post */

/**
 * ice_attach_post_recovery_mode - Limited driver attach_post for FW recovery
 * @sc: the device private softc
 *
 * Performs minimal work to prepare the driver to recover an NVM in case the
 * firmware is in recovery mode.
 */
static void
ice_attach_post_recovery_mode(struct ice_softc *sc)
{
	/* Configure interrupt causes for the administrative interrupt */
	ice_configure_misc_interrupts(sc);

	/* Enable ITR 0 right away, so that we can handle admin interrupts */
	ice_enable_intr(&sc->hw, sc->irqvs[0].me);

	/* Start the admin timer */
	mtx_lock(&sc->admin_mtx);
	callout_reset(&sc->admin_timer, hz/2, ice_admin_timer, sc);
	mtx_unlock(&sc->admin_mtx);

	ice_clear_state(&sc->state, ICE_STATE_ATTACHING);
}

/**
 * ice_free_irqvs - Free IRQ vector memory
 * @sc: the device private softc structure
 *
 * Free IRQ vector memory allocated during ice_if_msix_intr_assign.
 */
static void
ice_free_irqvs(struct ice_softc *sc)
{
	struct ice_vsi *vsi = &sc->pf_vsi;
	if_ctx_t ctx = sc->ctx;
	int i;

	/* If the irqvs array is NULL, then there are no vectors to free */
	if (sc->irqvs == NULL)
		return;

	/* Free the IRQ vectors */
	for (i = 0; i < sc->num_irq_vectors; i++)
		iflib_irq_free(ctx, &sc->irqvs[i].irq);

	/* Clear the irqv pointers */
	for (i = 0; i < vsi->num_rx_queues; i++)
		vsi->rx_queues[i].irqv = NULL;

	for (i = 0; i < vsi->num_tx_queues; i++)
		vsi->tx_queues[i].irqv = NULL;

	/* Release the vector array memory */
	free(sc->irqvs, M_ICE);
	sc->irqvs = NULL;
	sc->num_irq_vectors = 0;
}

/**
 * ice_if_detach - Device driver detach logic
 * @ctx: iflib context structure
 *
 * Perform device shutdown logic to detach the device driver.
 *
 * Note that there is no guarantee of the ordering of ice_if_queues_free() and
 * ice_if_detach(). It is possible for the functions to be called in either
 * order, and they must not assume to have a strict ordering.
 */
static int
ice_if_detach(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	int status;
	int i;

	ASSERT_CTX_LOCKED(sc);

	/* Indicate that we're detaching */
	ice_set_state(&sc->state, ICE_STATE_DETACHING);

	/* Stop the admin timer */
	mtx_lock(&sc->admin_mtx);
	callout_stop(&sc->admin_timer);
	mtx_unlock(&sc->admin_mtx);
	mtx_destroy(&sc->admin_mtx);

	/* Remove additional interfaces if they exist */
	if (sc->mirr_if)
		ice_destroy_mirror_interface(sc);
	ice_rdma_pf_detach(sc);

	/* Free allocated media types */
	ifmedia_removeall(sc->media);

	/* Free the Tx and Rx sysctl contexts, and assign NULL to the node
	 * pointers. Note, the calls here and those in ice_if_queues_free()
	 * are *BOTH* necessary, as we cannot guarantee which path will be
	 * run first
	 */
	ice_vsi_del_txqs_ctx(vsi);
	ice_vsi_del_rxqs_ctx(vsi);

	/* Release MSI-X resources */
	ice_free_irqvs(sc);

	for (i = 0; i < sc->num_available_vsi; i++) {
		if (sc->all_vsi[i])
			ice_release_vsi(sc->all_vsi[i]);
	}

	if (sc->all_vsi) {
		free(sc->all_vsi, M_ICE);
		sc->all_vsi = NULL;
	}

	/* Release MSI-X memory */
	pci_release_msi(sc->dev);

	if (sc->msix_table != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
				     rman_get_rid(sc->msix_table),
				     sc->msix_table);
		sc->msix_table = NULL;
	}

	ice_free_intr_tracking(sc);

	/* Destroy the queue managers */
	ice_resmgr_destroy(&sc->tx_qmgr);
	ice_resmgr_destroy(&sc->rx_qmgr);

	if (!ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		ice_deinit_hw(&sc->hw);

	IFLIB_CTX_UNLOCK(sc);
	status = ice_reset(&sc->hw, ICE_RESET_PFR);
	IFLIB_CTX_LOCK(sc);
	if (status) {
		device_printf(sc->dev, "device PF reset failed, err %s\n",
			      ice_status_str(status));
	}

	ice_free_pci_mapping(sc);

	return 0;
} /* ice_if_detach */

/**
 * ice_if_tx_queues_alloc - Allocate Tx queue memory
 * @ctx: iflib context structure
 * @vaddrs: virtual addresses for the queue memory
 * @paddrs: physical addresses for the queue memory
 * @ntxqs: the number of Tx queues per set (should always be 1)
 * @ntxqsets: the number of Tx queue sets to allocate
 *
 * Called by iflib to allocate Tx queues for the device. Allocates driver
 * memory to track each queue, the status arrays used for descriptor
 * status reporting, and Tx queue sysctls.
 */
static int
ice_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
		       int __invariant_only ntxqs, int ntxqsets)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_tx_queue *txq;
	int err, i, j;

	MPASS(ntxqs == 1);
	MPASS(sc->scctx->isc_ntxd[0] <= ICE_MAX_DESC_COUNT);
	ASSERT_CTX_LOCKED(sc);

	/* Do not bother allocating queues if we're in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (0);

	/* Allocate queue structure memory */
	if (!(vsi->tx_queues =
	      (struct ice_tx_queue *) malloc(sizeof(struct ice_tx_queue) * ntxqsets, M_ICE, M_NOWAIT | M_ZERO))) {
		device_printf(sc->dev, "Unable to allocate Tx queue memory\n");
		return (ENOMEM);
	}

	/* Allocate report status arrays */
	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		if (!(txq->tx_rsq =
		      (uint16_t *) malloc(sizeof(uint16_t) * sc->scctx->isc_ntxd[0], M_ICE, M_NOWAIT))) {
			device_printf(sc->dev, "Unable to allocate tx_rsq memory\n");
			err = ENOMEM;
			goto free_tx_queues;
		}
		/* Initialize report status array */
		for (j = 0; j < sc->scctx->isc_ntxd[0]; j++)
			txq->tx_rsq[j] = QIDX_INVALID;
	}

	/* Assign queues from PF space to the main VSI */
	err = ice_resmgr_assign_contiguous(&sc->tx_qmgr, vsi->tx_qmap, ntxqsets);
	if (err) {
		device_printf(sc->dev, "Unable to assign PF queues: %s\n",
			      ice_err_str(err));
		goto free_tx_queues;
	}
	vsi->qmap_type = ICE_RESMGR_ALLOC_CONTIGUOUS;

	/* Add Tx queue sysctls context */
	ice_vsi_add_txqs_ctx(vsi);

	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		/* q_handle == me when only one TC */
		txq->me = txq->q_handle = i;
		txq->vsi = vsi;

		/* store the queue size for easier access */
		txq->desc_count = sc->scctx->isc_ntxd[0];

		/* get the virtual and physical address of the hardware queues */
		txq->tail = QTX_COMM_DBELL(vsi->tx_qmap[i]);
		txq->tx_base = (struct ice_tx_desc *)vaddrs[i];
		txq->tx_paddr = paddrs[i];

		ice_add_txq_sysctls(txq);
	}

	vsi->num_tx_queues = ntxqsets;

	return (0);

free_tx_queues:
	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		if (txq->tx_rsq != NULL) {
			free(txq->tx_rsq, M_ICE);
			txq->tx_rsq = NULL;
		}
	}
	free(vsi->tx_queues, M_ICE);
	vsi->tx_queues = NULL;
	return err;
}

/**
 * ice_if_rx_queues_alloc - Allocate Rx queue memory
 * @ctx: iflib context structure
 * @vaddrs: virtual addresses for the queue memory
 * @paddrs: physical addresses for the queue memory
 * @nrxqs: number of Rx queues per set (should always be 1)
 * @nrxqsets: number of Rx queue sets to allocate
 *
 * Called by iflib to allocate Rx queues for the device. Allocates driver
 * memory to track each queue, as well as sets up the Rx queue sysctls.
 */
static int
ice_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
		       int __invariant_only nrxqs, int nrxqsets)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_rx_queue *rxq;
	int err, i;

	MPASS(nrxqs == 1);
	MPASS(sc->scctx->isc_nrxd[0] <= ICE_MAX_DESC_COUNT);
	ASSERT_CTX_LOCKED(sc);

	/* Do not bother allocating queues if we're in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (0);

	/* Allocate queue structure memory */
	if (!(vsi->rx_queues =
	      (struct ice_rx_queue *) malloc(sizeof(struct ice_rx_queue) * nrxqsets, M_ICE, M_NOWAIT | M_ZERO))) {
		device_printf(sc->dev, "Unable to allocate Rx queue memory\n");
		return (ENOMEM);
	}

	/* Assign queues from PF space to the main VSI */
	err = ice_resmgr_assign_contiguous(&sc->rx_qmgr, vsi->rx_qmap, nrxqsets);
	if (err) {
		device_printf(sc->dev, "Unable to assign PF queues: %s\n",
			      ice_err_str(err));
		goto free_rx_queues;
	}
	vsi->qmap_type = ICE_RESMGR_ALLOC_CONTIGUOUS;

	/* Add Rx queue sysctls context */
	ice_vsi_add_rxqs_ctx(vsi);

	for (i = 0, rxq = vsi->rx_queues; i < nrxqsets; i++, rxq++) {
		rxq->me = i;
		rxq->vsi = vsi;

		/* store the queue size for easier access */
		rxq->desc_count = sc->scctx->isc_nrxd[0];

		/* get the virtual and physical address of the hardware queues */
		rxq->tail = QRX_TAIL(vsi->rx_qmap[i]);
		rxq->rx_base = (union ice_32b_rx_flex_desc *)vaddrs[i];
		rxq->rx_paddr = paddrs[i];

		ice_add_rxq_sysctls(rxq);
	}

	vsi->num_rx_queues = nrxqsets;

	return (0);

free_rx_queues:
	free(vsi->rx_queues, M_ICE);
	vsi->rx_queues = NULL;
	return err;
}

/**
 * ice_if_queues_free - Free queue memory
 * @ctx: the iflib context structure
 *
 * Free queue memory allocated by ice_if_tx_queues_alloc() and
 * ice_if_rx_queues_alloc().
 *
 * There is no guarantee that ice_if_queues_free() and ice_if_detach() will be
 * called in the same order. It's possible for ice_if_queues_free() to be
 * called prior to ice_if_detach(), and vice versa.
 *
 * For this reason, the main VSI is a static member of the ice_softc, which is
 * not free'd until after iflib finishes calling both of these functions.
 *
 * Thus, care must be taken in how we manage the memory being freed by this
 * function, and in what tasks it can and must perform.
 */
static void
ice_if_queues_free(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_tx_queue *txq;
	int i;

	/* Free the Tx and Rx sysctl contexts, and assign NULL to the node
	 * pointers. Note, the calls here and those in ice_if_detach()
	 * are *BOTH* necessary, as we cannot guarantee which path will be
	 * run first
	 */
	ice_vsi_del_txqs_ctx(vsi);
	ice_vsi_del_rxqs_ctx(vsi);

	/* Release MSI-X IRQ vectors, if not yet released in ice_if_detach */
	ice_free_irqvs(sc);

	if (vsi->tx_queues != NULL) {
		/* free the tx_rsq arrays */
		for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++) {
			if (txq->tx_rsq != NULL) {
				free(txq->tx_rsq, M_ICE);
				txq->tx_rsq = NULL;
			}
		}
		free(vsi->tx_queues, M_ICE);
		vsi->tx_queues = NULL;
		vsi->num_tx_queues = 0;
	}
	if (vsi->rx_queues != NULL) {
		free(vsi->rx_queues, M_ICE);
		vsi->rx_queues = NULL;
		vsi->num_rx_queues = 0;
	}
}

/**
 * ice_msix_que - Fast interrupt handler for MSI-X receive queues
 * @arg: The Rx queue memory
 *
 * Interrupt filter function for iflib MSI-X interrupts. Called by iflib when
 * an MSI-X interrupt for a given queue is triggered. Currently this just asks
 * iflib to schedule the main Rx thread.
 */
static int
ice_msix_que(void *arg)
{
	struct ice_rx_queue __unused *rxq = (struct ice_rx_queue *)arg;

	/* TODO: dynamic ITR algorithm?? */

	return (FILTER_SCHEDULE_THREAD);
}

/**
 * ice_msix_admin - Fast interrupt handler for MSI-X admin interrupt
 * @arg: pointer to device softc memory
 *
 * Called by iflib when an administrative interrupt occurs. Should perform any
 * fast logic for handling the interrupt cause, and then indicate whether the
 * admin task needs to be queued.
 */
static int
ice_msix_admin(void *arg)
{
	struct ice_softc *sc = (struct ice_softc *)arg;
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u32 oicr;

	/* There is no safe way to modify the enabled miscellaneous causes of
	 * the OICR vector at runtime, as doing so would be prone to race
	 * conditions. Reading PFINT_OICR will unmask the associated interrupt
	 * causes and allow future interrupts to occur. The admin interrupt
	 * vector will not be re-enabled until after we exit this function,
	 * but any delayed tasks must be resilient against possible "late
	 * arrival" interrupts that occur while we're already handling the
	 * task. This is done by using state bits and serializing these
	 * delayed tasks via the admin status task function.
	 */
	oicr = rd32(hw, PFINT_OICR);

	/* Processing multiple controlq interrupts on a single vector does not
	 * provide an indication of which controlq triggered the interrupt.
	 * We might try reading the INTEVENT bit of the respective PFINT_*_CTL
	 * registers. However, the INTEVENT bit is not guaranteed to be set as
	 * it gets automatically cleared when the hardware acknowledges the
	 * interrupt.
	 *
	 * This means we don't really have a good indication of whether or
	 * which controlq triggered this interrupt. We'll just notify the
	 * admin task that it should check all the controlqs.
	 */
	ice_set_state(&sc->state, ICE_STATE_CONTROLQ_EVENT_PENDING);

	if (oicr & PFINT_OICR_VFLR_M) {
		ice_set_state(&sc->state, ICE_STATE_VFLR_PENDING);
	}

	if (oicr & PFINT_OICR_MAL_DETECT_M) {
		ice_set_state(&sc->state, ICE_STATE_MDD_PENDING);
	}

	if (oicr & PFINT_OICR_GRST_M) {
		u32 reset;

		reset = (rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_RESET_TYPE_M) >>
			GLGEN_RSTAT_RESET_TYPE_S;

		if (reset == ICE_RESET_CORER)
			sc->soft_stats.corer_count++;
		else if (reset == ICE_RESET_GLOBR)
			sc->soft_stats.globr_count++;
		else
			sc->soft_stats.empr_count++;

		/* There are a couple of bits at play for handling resets.
		 * First, the ICE_STATE_RESET_OICR_RECV bit is used to
		 * indicate that the driver has received an OICR with a reset
		 * bit active, indicating that a CORER/GLOBR/EMPR is about to
		 * happen. Second, we set hw->reset_ongoing to indicate that
		 * the hardware is in reset. We will set this back to false as
		 * soon as the driver has determined that the hardware is out
		 * of reset.
		 *
		 * If the driver wishes to trigger a request, it can set one of
		 * the ICE_STATE_RESET_*_REQ bits, which will trigger the
		 * correct type of reset.
		 */
		if (!ice_testandset_state(&sc->state, ICE_STATE_RESET_OICR_RECV)) {
			hw->reset_ongoing = true;
			/*
			 * During the NVM update process, there is a driver reset and link
			 * goes down and then up. The below if-statement prevents a second
			 * link flap from occurring in ice_if_init().
			 */
			if (if_getflags(sc->ifp) & IFF_UP)
				ice_set_state(&sc->state, ICE_STATE_FIRST_INIT_LINK);
		}
	}

	if (oicr & PFINT_OICR_ECC_ERR_M) {
		device_printf(dev, "ECC Error detected!\n");
		ice_set_state(&sc->state, ICE_STATE_RESET_PFR_REQ);
	}

	if (oicr & (PFINT_OICR_PE_CRITERR_M | PFINT_OICR_HMC_ERR_M)) {
		if (oicr & PFINT_OICR_HMC_ERR_M)
			/* Log the HMC errors */
			ice_log_hmc_error(hw, dev);
		ice_rdma_notify_pe_intr(sc, oicr);
	}

	if (oicr & PFINT_OICR_PCI_EXCEPTION_M) {
		device_printf(dev, "PCI Exception detected!\n");
		ice_set_state(&sc->state, ICE_STATE_RESET_PFR_REQ);
	}

	return (FILTER_SCHEDULE_THREAD);
}

/**
 * ice_allocate_msix - Allocate MSI-X vectors for the interface
 * @sc: the device private softc
 *
 * Map the MSI-X bar, and then request MSI-X vectors in a two-stage process.
 *
 * First, determine a suitable total number of vectors based on the number
 * of CPUs, RSS buckets, the administrative vector, and other demands such as
 * RDMA.
 *
 * Request the desired amount of vectors, and see how many we obtain. If we
 * don't obtain as many as desired, reduce the demands by lowering the number
 * of requested queues or reducing the demand from other features such as
 * RDMA.
 *
 * @remark This function is required because the driver sets the
 * IFLIB_SKIP_MSIX flag indicating that the driver will manage MSI-X vectors
 * manually.
 *
 * @remark This driver will only use MSI-X vectors. If this is not possible,
 * neither MSI or legacy interrupts will be tried.
 *
 * @remark if it exists, os_imgr is initialized here for keeping track of
 * the assignments of extra MSIX vectors.
 *
 * @post on success this function must set the following scctx parameters:
 * isc_vectors, isc_nrxqsets, isc_ntxqsets, and isc_intr.
 *
 * @returns zero on success or an error code on failure.
 */
static int
ice_allocate_msix(struct ice_softc *sc)
{
	bool iflib_override_queue_count = false;
	if_softc_ctx_t scctx = sc->scctx;
	device_t dev = sc->dev;
	cpuset_t cpus;
	int bar, queues, vectors, requested;
	int err = 0;
	int rdma;

	/* Allocate the MSI-X bar */
	bar = scctx->isc_msix_bar;
	sc->msix_table = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &bar, RF_ACTIVE);
	if (!sc->msix_table) {
		device_printf(dev, "Unable to map MSI-X table\n");
		return (ENOMEM);
	}

	/* Check if the iflib queue count sysctls have been set */
	if (sc->ifc_sysctl_ntxqs || sc->ifc_sysctl_nrxqs)
		iflib_override_queue_count = true;

	err = bus_get_cpus(dev, INTR_CPUS, sizeof(cpus), &cpus);
	if (err) {
		device_printf(dev, "%s: Unable to fetch the CPU list: %s\n",
			      __func__, ice_err_str(err));
		CPU_COPY(&all_cpus, &cpus);
	}

	/* Attempt to mimic behavior of iflib_msix_init */
	if (iflib_override_queue_count) {
		/*
		 * If the override sysctls have been set, limit the queues to
		 * the number of logical CPUs.
		 */
		queues = mp_ncpus;
	} else {
		/*
		 * Otherwise, limit the queue count to the CPUs associated
		 * with the NUMA node the device is associated with.
		 */
		queues = CPU_COUNT(&cpus);
	}

	/* Clamp to the number of RSS buckets */
	queues = imin(queues, rss_getnumbuckets());

	/*
	 * Clamp the number of queue pairs to the minimum of the requested Tx
	 * and Rx queues.
	 */
	queues = imin(queues, sc->ifc_sysctl_ntxqs ?: scctx->isc_ntxqsets);
	queues = imin(queues, sc->ifc_sysctl_nrxqs ?: scctx->isc_nrxqsets);

	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_RDMA)) {
		/*
		 * Choose a number of RDMA vectors based on the number of CPUs
		 * up to a maximum
		 */
		rdma = min(CPU_COUNT(&cpus), ICE_RDMA_MAX_MSIX);

		/* Further limit by the user configurable tunable */
		rdma = min(rdma, ice_rdma_max_msix);
	} else {
		rdma = 0;
	}

	/*
	 * Determine the number of vectors to request. Note that we also need
	 * to allocate one vector for administrative tasks.
	 */
	requested = rdma + queues + 1;
	/* Add extra vectors requested by the user for later subinterface
	 * creation.
	 */
	if_ctx_t ctx = sc->ctx;
	u32 extra_vectors = iflib_get_extra_msix_vectors_sysctl(ctx);
	requested += extra_vectors;

	vectors = requested;
	err = pci_alloc_msix(dev, &vectors);
	if (err) {
		device_printf(dev, "Failed to allocate %d MSI-X vectors, err %s\n",
			      vectors, ice_err_str(err));
		goto err_free_msix_table;
	}

	/* If we don't receive enough vectors, reduce demands */
	if (vectors < requested) {
		int diff = requested - vectors;

		device_printf(dev, "Requested %d MSI-X vectors, but got only %d\n",
			      requested, vectors);

		diff += extra_vectors;
		extra_vectors = 0;
		/*
		 * The OS didn't grant us the requested number of vectors.
		 * Check to see if we can reduce demands by limiting the
		 * number of vectors allocated to certain features.
		 */

		if (rdma >= diff) {
			/* Reduce the number of RDMA vectors we reserve */
			rdma -= diff;
			diff = 0;
		} else {
			/* Disable RDMA and reduce the difference */
			ice_clear_bit(ICE_FEATURE_RDMA, sc->feat_cap);
			diff -= rdma;
			rdma = 0;
		}

		/*
		 * If we still have a difference, we need to reduce the number
		 * of queue pairs.
		 *
		 * However, we still need at least one vector for the admin
		 * interrupt and one queue pair.
		 */
		if (queues <= diff) {
			device_printf(dev, "Unable to allocate sufficient MSI-X vectors\n");
			err = (ERANGE);
			goto err_pci_release_msi;
		}

		queues -= diff;
	}

	device_printf(dev, "Using %d Tx and Rx queues\n", queues);
	if (rdma)
		device_printf(dev, "Reserving %d MSI-X interrupts for iRDMA\n",
			      rdma);
	device_printf(dev, "Using MSI-X interrupts with %d vectors\n",
		      vectors);

	/* Split resulting vectors back into requested splits */
	scctx->isc_vectors = vectors;
	scctx->isc_nrxqsets = queues;
	scctx->isc_ntxqsets = queues;
	scctx->isc_intr = IFLIB_INTR_MSIX;

	sc->irdma_vectors = rdma;

	/* Interrupt allocation tracking isn't required in recovery mode,
	 * since neither RDMA nor VFs are enabled.
	 */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (0);

	/* Keep track of which interrupt indices are being used for what */
	sc->lan_vectors = vectors - rdma;
	sc->lan_vectors -= extra_vectors;
	err = ice_resmgr_assign_contiguous(&sc->dev_imgr, sc->pf_imap, sc->lan_vectors);
	if (err) {
		device_printf(dev, "Unable to assign PF interrupt mapping: %s\n",
			      ice_err_str(err));
		goto err_pci_release_msi;
	}
	err = ice_resmgr_assign_contiguous(&sc->dev_imgr, sc->rdma_imap, rdma);
	if (err) {
		device_printf(dev, "Unable to assign PF RDMA interrupt mapping: %s\n",
			      ice_err_str(err));
		goto err_release_pf_imap;
	}
	sc->extra_vectors = extra_vectors;
	/* Setup another resource manager to track the assignments of extra OS
	 * vectors. These OS interrupt allocations don't need to be contiguous,
	 * unlike the ones that come from the device.
	 */
	err = ice_resmgr_init(&sc->os_imgr, sc->extra_vectors);
	if (err) {
		device_printf(dev, "Unable to initialize OS extra interrupt manager: %s\n",
			      ice_err_str(err));
		ice_resmgr_release_map(&sc->dev_imgr, sc->rdma_imap,
					    rdma);
		goto err_release_pf_imap;
	}
	return (0);

err_release_pf_imap:
	ice_resmgr_release_map(&sc->dev_imgr, sc->pf_imap,
				    sc->lan_vectors);
err_pci_release_msi:
	pci_release_msi(dev);
err_free_msix_table:
	if (sc->msix_table != NULL) {
		bus_release_resource(sc->dev, SYS_RES_MEMORY,
				rman_get_rid(sc->msix_table),
				sc->msix_table);
		sc->msix_table = NULL;
	}

	return (err);
}

/**
 * ice_if_msix_intr_assign - Assign MSI-X interrupt vectors to queues
 * @ctx: the iflib context structure
 * @msix: the number of vectors we were assigned
 *
 * Called by iflib to assign MSI-X vectors to queues. Currently requires that
 * we get at least the same number of vectors as we have queues, and that we
 * always have the same number of Tx and Rx queues.
 *
 * Tx queues use a softirq instead of using their own hardware interrupt.
 */
static int
ice_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	int err, i, vector;

	ASSERT_CTX_LOCKED(sc);

	if (vsi->num_rx_queues != vsi->num_tx_queues) {
		device_printf(sc->dev,
			      "iflib requested %d Tx queues, and %d Rx queues, but the driver isn't able to support a differing number of Tx and Rx queues\n",
			      vsi->num_tx_queues, vsi->num_rx_queues);
		return (EOPNOTSUPP);
	}

	if (msix < (vsi->num_rx_queues + 1)) {
		device_printf(sc->dev,
			      "Not enough MSI-X vectors to assign one vector to each queue pair\n");
		return (EOPNOTSUPP);
	}

	/* Save the number of vectors for future use */
	sc->num_irq_vectors = vsi->num_rx_queues + 1;

	/* Allocate space to store the IRQ vector data */
	if (!(sc->irqvs =
	      (struct ice_irq_vector *) malloc(sizeof(struct ice_irq_vector) * (sc->num_irq_vectors),
					       M_ICE, M_NOWAIT))) {
		device_printf(sc->dev,
			      "Unable to allocate irqv memory\n");
		return (ENOMEM);
	}

	/* Administrative interrupt events will use vector 0 */
	err = iflib_irq_alloc_generic(ctx, &sc->irqvs[0].irq, 1, IFLIB_INTR_ADMIN,
				      ice_msix_admin, sc, 0, "admin");
	if (err) {
		device_printf(sc->dev,
			      "Failed to register Admin queue handler: %s\n",
			      ice_err_str(err));
		goto free_irqvs;
	}
	sc->irqvs[0].me = 0;

	/* Do not allocate queue interrupts when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (0);

	int rid;
	for (i = 0, vector = 1; i < vsi->num_rx_queues; i++, vector++) {
		struct ice_rx_queue *rxq = &vsi->rx_queues[i];
		struct ice_tx_queue *txq = &vsi->tx_queues[i];
		char irq_name[16];

		rid = vector + 1;

		snprintf(irq_name, sizeof(irq_name), "rxq%d", i);
		err = iflib_irq_alloc_generic(ctx, &sc->irqvs[vector].irq, rid,
					      IFLIB_INTR_RXTX, ice_msix_que,
					      rxq, rxq->me, irq_name);
		if (err) {
			device_printf(sc->dev,
				      "Failed to allocate q int %d err: %s\n",
				      i, ice_err_str(err));
			vector--;
			i--;
			goto fail;
		}
		sc->irqvs[vector].me = vector;
		rxq->irqv = &sc->irqvs[vector];

		bzero(irq_name, sizeof(irq_name));

		snprintf(irq_name, sizeof(irq_name), "txq%d", i);
		iflib_softirq_alloc_generic(ctx, &sc->irqvs[vector].irq,
					    IFLIB_INTR_TX, txq,
					    txq->me, irq_name);
		txq->irqv = &sc->irqvs[vector];
	}

	/* For future interrupt assignments */
	sc->last_rid = rid + sc->irdma_vectors;

	return (0);
fail:
	for (; i >= 0; i--, vector--)
		iflib_irq_free(ctx, &sc->irqvs[vector].irq);
	iflib_irq_free(ctx, &sc->irqvs[0].irq);
free_irqvs:
	free(sc->irqvs, M_ICE);
	sc->irqvs = NULL;
	return err;
}

/**
 * ice_if_mtu_set - Set the device MTU
 * @ctx: iflib context structure
 * @mtu: the MTU requested
 *
 * Called by iflib to configure the device's Maximum Transmission Unit (MTU).
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static int
ice_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	ASSERT_CTX_LOCKED(sc);

	/* Do not support configuration when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	if (mtu < ICE_MIN_MTU || mtu > ICE_MAX_MTU)
		return (EINVAL);

	sc->scctx->isc_max_frame_size = mtu +
		ETHER_HDR_LEN + ETHER_CRC_LEN + ETHER_VLAN_ENCAP_LEN;

	sc->pf_vsi.max_frame_size = sc->scctx->isc_max_frame_size;

	return (0);
}

/**
 * ice_if_intr_enable - Enable device interrupts
 * @ctx: iflib context structure
 *
 * Called by iflib to request enabling device interrupts.
 */
static void
ice_if_intr_enable(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;

	ASSERT_CTX_LOCKED(sc);

	/* Enable ITR 0 */
	ice_enable_intr(hw, sc->irqvs[0].me);

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	/* Enable all queue interrupts */
	for (int i = 0; i < vsi->num_rx_queues; i++)
		ice_enable_intr(hw, vsi->rx_queues[i].irqv->me);
}

/**
 * ice_if_intr_disable - Disable device interrupts
 * @ctx: iflib context structure
 *
 * Called by iflib to request disabling device interrupts.
 */
static void
ice_if_intr_disable(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_hw *hw = &sc->hw;
	unsigned int i;

	ASSERT_CTX_LOCKED(sc);

	/* IFDI_INTR_DISABLE may be called prior to interrupts actually being
	 * assigned to queues. Instead of assuming that the interrupt
	 * assignment in the rx_queues structure is valid, just disable all
	 * possible interrupts
	 *
	 * Note that we choose not to disable ITR 0 because this handles the
	 * AdminQ interrupts, and we want to keep processing these even when
	 * the interface is offline.
	 */
	for (i = 1; i < hw->func_caps.common_cap.num_msix_vectors; i++)
		ice_disable_intr(hw, i);
}

/**
 * ice_if_rx_queue_intr_enable - Enable a specific Rx queue interrupt
 * @ctx: iflib context structure
 * @rxqid: the Rx queue to enable
 *
 * Enable a specific Rx queue interrupt.
 *
 * This function is not protected by the iflib CTX lock.
 */
static int
ice_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	ice_enable_intr(hw, vsi->rx_queues[rxqid].irqv->me);
	return (0);
}

/**
 * ice_if_tx_queue_intr_enable - Enable a specific Tx queue interrupt
 * @ctx: iflib context structure
 * @txqid: the Tx queue to enable
 *
 * Enable a specific Tx queue interrupt.
 *
 * This function is not protected by the iflib CTX lock.
 */
static int
ice_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_hw *hw = &sc->hw;

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	ice_enable_intr(hw, vsi->tx_queues[txqid].irqv->me);
	return (0);
}

/**
 * ice_set_default_promisc_mask - Set default config for promisc settings
 * @promisc_mask: bitmask to setup
 *
 * The ice_(set|clear)_vsi_promisc() function expects a mask of promiscuous
 * modes to operate on. The mask used in here is the default one for the
 * driver, where promiscuous is enabled/disabled for all types of
 * non-VLAN-tagged/VLAN 0 traffic.
 */
static void
ice_set_default_promisc_mask(ice_bitmap_t *promisc_mask)
{
	ice_zero_bitmap(promisc_mask, ICE_PROMISC_MAX);
	ice_set_bit(ICE_PROMISC_UCAST_TX, promisc_mask);
	ice_set_bit(ICE_PROMISC_UCAST_RX, promisc_mask);
	ice_set_bit(ICE_PROMISC_MCAST_TX, promisc_mask);
	ice_set_bit(ICE_PROMISC_MCAST_RX, promisc_mask);
}

/**
 * ice_if_promisc_set - Set device promiscuous mode
 * @ctx: iflib context structure
 * @flags: promiscuous flags to configure
 *
 * Called by iflib to configure device promiscuous mode.
 *
 * @remark Calls to this function will always overwrite the previous setting
 */
static int
ice_if_promisc_set(if_ctx_t ctx, int flags)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int status;
	bool promisc_enable = flags & IFF_PROMISC;
	bool multi_enable = flags & IFF_ALLMULTI;
	ice_declare_bitmap(promisc_mask, ICE_PROMISC_MAX);

	/* Do not support configuration when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);
	
	ice_set_default_promisc_mask(promisc_mask);

	if (multi_enable)
		return (EOPNOTSUPP);

	if (promisc_enable) {
		status = ice_set_vsi_promisc(hw, sc->pf_vsi.idx,
					     promisc_mask, 0);
		if (status && status != ICE_ERR_ALREADY_EXISTS) {
			device_printf(dev,
				      "Failed to enable promiscuous mode for PF VSI, err %s aq_err %s\n",
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	} else {
		status = ice_clear_vsi_promisc(hw, sc->pf_vsi.idx,
					       promisc_mask, 0);
		if (status) {
			device_printf(dev,
				      "Failed to disable promiscuous mode for PF VSI, err %s aq_err %s\n",
				      ice_status_str(status),
				      ice_aq_str(hw->adminq.sq_last_status));
			return (EIO);
		}
	}

	return (0);
}

/**
 * ice_if_media_change - Change device media
 * @ctx: device ctx structure
 *
 * Called by iflib when a media change is requested. This operation is not
 * supported by the hardware, so we just return an error code.
 */
static int
ice_if_media_change(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	device_printf(sc->dev, "Media change is not supported.\n");
	return (ENODEV);
}

/**
 * ice_if_media_status - Report current device media
 * @ctx: iflib context structure
 * @ifmr: ifmedia request structure to update
 *
 * Updates the provided ifmr with current device media status, including link
 * status and media type.
 */
static void
ice_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ice_link_status *li = &sc->hw.port_info->phy.link_info;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	/* Never report link up or media types when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	if (!sc->link_up)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_FDX;

	if (li->phy_type_low)
		ifmr->ifm_active |= ice_get_phy_type_low(li->phy_type_low);
	else if (li->phy_type_high)
		ifmr->ifm_active |= ice_get_phy_type_high(li->phy_type_high);
	else
		ifmr->ifm_active |= IFM_UNKNOWN;

	/* Report flow control status as well */
	if (li->an_info & ICE_AQ_LINK_PAUSE_TX)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;
	if (li->an_info & ICE_AQ_LINK_PAUSE_RX)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;
}

/**
 * ice_init_tx_tracking - Initialize Tx queue software tracking values
 * @vsi: the VSI to initialize
 *
 * Initialize Tx queue software tracking values, including the Report Status
 * queue, and related software tracking values.
 */
static void
ice_init_tx_tracking(struct ice_vsi *vsi)
{
	struct ice_tx_queue *txq;
	size_t j;
	int i;

	for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++) {

		txq->tx_rs_cidx = txq->tx_rs_pidx = 0;

		/* Initialize the last processed descriptor to be the end of
		 * the ring, rather than the start, so that we avoid an
		 * off-by-one error in ice_ift_txd_credits_update for the
		 * first packet.
		 */
		txq->tx_cidx_processed = txq->desc_count - 1;

		for (j = 0; j < txq->desc_count; j++)
			txq->tx_rsq[j] = QIDX_INVALID;
	}
}

/**
 * ice_update_rx_mbuf_sz - Update the Rx buffer size for all queues
 * @sc: the device softc
 *
 * Called to update the Rx queue mbuf_sz parameter for configuring the receive
 * buffer sizes when programming hardware.
 */
static void
ice_update_rx_mbuf_sz(struct ice_softc *sc)
{
	uint32_t mbuf_sz = iflib_get_rx_mbuf_sz(sc->ctx);
	struct ice_vsi *vsi = &sc->pf_vsi;

	MPASS(mbuf_sz <= UINT16_MAX);
	vsi->mbuf_sz = mbuf_sz;
}

/**
 * ice_if_init - Initialize the device
 * @ctx: iflib ctx structure
 *
 * Called by iflib to bring the device up, i.e. ifconfig ice0 up. Initializes
 * device filters and prepares the Tx and Rx engines.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_init(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	device_t dev = sc->dev;
	int err;

	ASSERT_CTX_LOCKED(sc);

	/*
	 * We've seen an issue with 11.3/12.1 where sideband routines are
	 * called after detach is called.  This would call routines after
	 * if_stop, causing issues with the teardown process.  This has
	 * seemingly been fixed in STABLE snapshots, but it seems like a
	 * good idea to have this guard here regardless.
	 */
	if (ice_driver_is_detaching(sc))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RESET_FAILED)) {
		device_printf(sc->dev, "request to start interface cannot be completed as the device failed to reset\n");
		return;
	}

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(sc->dev, "request to start interface while device is prepared for impending reset\n");
		return;
	}

	ice_update_rx_mbuf_sz(sc);

	/* Update the MAC address... User might use a LAA */
	err = ice_update_laa_mac(sc);
	if (err) {
		device_printf(dev,
			      "LAA address change failed, err %s\n",
			      ice_err_str(err));
		return;
	}

	/* Initialize software Tx tracking values */
	ice_init_tx_tracking(&sc->pf_vsi);

	err = ice_cfg_vsi_for_tx(&sc->pf_vsi);
	if (err) {
		device_printf(dev,
			      "Unable to configure the main VSI for Tx: %s\n",
			      ice_err_str(err));
		return;
	}

	err = ice_cfg_vsi_for_rx(&sc->pf_vsi);
	if (err) {
		device_printf(dev,
			      "Unable to configure the main VSI for Rx: %s\n",
			      ice_err_str(err));
		goto err_cleanup_tx;
	}

	err = ice_control_all_rx_queues(&sc->pf_vsi, true);
	if (err) {
		device_printf(dev,
			      "Unable to enable Rx rings for transmit: %s\n",
			      ice_err_str(err));
		goto err_cleanup_tx;
	}

	err = ice_cfg_pf_default_mac_filters(sc);
	if (err) {
		device_printf(dev,
			      "Unable to configure default MAC filters: %s\n",
			      ice_err_str(err));
		goto err_stop_rx;
	}

	/* We use software interrupts for Tx, so we only program the hardware
	 * interrupts for Rx.
	 */
	ice_configure_all_rxq_interrupts(&sc->pf_vsi);
	ice_configure_rx_itr(&sc->pf_vsi);

	/* Configure promiscuous mode */
	ice_if_promisc_set(ctx, if_getflags(sc->ifp));

	if (!ice_testandclear_state(&sc->state, ICE_STATE_FIRST_INIT_LINK))
		if (!sc->link_up && ((if_getflags(sc->ifp) & IFF_UP) ||
			 ice_test_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN)))
			ice_set_link(sc, true);

	ice_rdma_pf_init(sc);

	ice_set_state(&sc->state, ICE_STATE_DRIVER_INITIALIZED);

	if (sc->mirr_if && ice_testandclear_state(&mif->state, ICE_STATE_SUBIF_NEEDS_REINIT)) {
		ice_clear_state(&mif->state, ICE_STATE_DRIVER_INITIALIZED);
		iflib_request_reset(sc->mirr_if->subctx);
		iflib_admin_intr_deferred(sc->mirr_if->subctx);
	}

	return;

err_stop_rx:
	ice_control_all_rx_queues(&sc->pf_vsi, false);
err_cleanup_tx:
	ice_vsi_disable_tx(&sc->pf_vsi);
}

/**
 * ice_poll_for_media_avail - Re-enable link if media is detected
 * @sc: device private structure
 *
 * Intended to be called from the driver's timer function, this function
 * sends the Get Link Status AQ command and re-enables HW link if the
 * command says that media is available.
 *
 * If the driver doesn't have the "NO_MEDIA" state set, then this does nothing,
 * since media removal events are supposed to be sent to the driver through
 * a link status event.
 */
static void
ice_poll_for_media_avail(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	struct ice_port_info *pi = hw->port_info;

	/* E830 only: There's no interrupt for when the PHY FW has finished loading,
	 * so poll for the status in the media task here if it's previously
	 * been detected that it's still loading.
	 */
	if (ice_is_e830(hw) &&
	    ice_test_state(&sc->state, ICE_STATE_PHY_FW_INIT_PENDING)) {
		if (rd32(hw, GL_MNG_FWSM) & GL_MNG_FWSM_FW_LOADING_M)
			ice_clear_state(&sc->state, ICE_STATE_PHY_FW_INIT_PENDING);
		else
			return;
	}

	if (ice_test_state(&sc->state, ICE_STATE_NO_MEDIA)) {
		pi->phy.get_link_info = true;
		ice_get_link_status(pi, &sc->link_up);

		if (pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE) {
			int status;

			/* Re-enable link and re-apply user link settings */
			if (ice_test_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN) ||
			    (if_getflags(sc->ifp) & IFF_UP)) {
				ice_apply_saved_phy_cfg(sc, ICE_APPLY_LS_FEC_FC);

				/* Update the OS about changes in media capability */
				status = ice_add_media_types(sc, sc->media);
				if (status)
					device_printf(sc->dev,
					    "Error adding device media types: %s aq_err %s\n",
					    ice_status_str(status),
					    ice_aq_str(hw->adminq.sq_last_status));
			}

			ice_clear_state(&sc->state, ICE_STATE_NO_MEDIA);
		}
	}
}

/**
 * ice_if_timer - called by iflib periodically
 * @ctx: iflib ctx structure
 * @qid: the queue this timer was called for
 *
 * This callback is triggered by iflib periodically. We use it to update the
 * hw statistics.
 *
 * @remark this function is not protected by the iflib CTX lock.
 */
static void
ice_if_timer(if_ctx_t ctx, uint16_t qid)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	uint64_t prev_link_xoff_rx = sc->stats.cur.link_xoff_rx;

	if (qid != 0)
		return;

	/* Do not attempt to update stats when in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	/* Update device statistics */
	ice_update_pf_stats(sc);

	/*
	 * For proper watchdog management, the iflib stack needs to know if
	 * we've been paused during the last interval. Check if the
	 * link_xoff_rx stat changed, and set the isc_pause_frames, if so.
	 */
	if (sc->stats.cur.link_xoff_rx != prev_link_xoff_rx)
		sc->scctx->isc_pause_frames = 1;

	/* Update the primary VSI stats */
	ice_update_vsi_hw_stats(&sc->pf_vsi);

	/* Update mirror VSI stats */
	if (sc->mirr_if && sc->mirr_if->if_attached)
		ice_update_vsi_hw_stats(sc->mirr_if->vsi);
}

/**
 * ice_admin_timer - called periodically to trigger the admin task
 * @arg: callout(9) argument pointing to the device private softc structure
 *
 * Timer function used as part of a callout(9) timer that will periodically
 * trigger the admin task, even when the interface is down.
 *
 * @remark this function is not called by iflib and is not protected by the
 * iflib CTX lock.
 *
 * @remark because this is a callout function, it cannot sleep and should not
 * attempt taking the iflib CTX lock.
 */
static void
ice_admin_timer(void *arg)
{
	struct ice_softc *sc = (struct ice_softc *)arg;

	/*
	 * There is a point where callout routines are no longer
	 * cancelable.  So there exists a window of time where the
	 * driver enters detach() and tries to cancel the callout, but the
	 * callout routine has passed the cancellation point.  The detach()
	 * routine is unaware of this and tries to free resources that the
	 * callout routine needs.  So we check for the detach state flag to
	 * at least shrink the window of opportunity.
	 */
	if (ice_driver_is_detaching(sc))
		return;

	/* Fire off the admin task */
	iflib_admin_intr_deferred(sc->ctx);

	/* Reschedule the admin timer */
	callout_schedule(&sc->admin_timer, hz/2);
}

/**
 * ice_transition_recovery_mode - Transition to recovery mode
 * @sc: the device private softc
 *
 * Called when the driver detects that the firmware has entered recovery mode
 * at run time.
 */
static void
ice_transition_recovery_mode(struct ice_softc *sc)
{
	struct ice_vsi *vsi = &sc->pf_vsi;
	int i;

	device_printf(sc->dev, "Firmware recovery mode detected. Limiting functionality. Refer to Intel(R) Ethernet Adapters and Devices User Guide for details on firmware recovery mode.\n");

	/* Tell the stack that the link has gone down */
	iflib_link_state_change(sc->ctx, LINK_STATE_DOWN, 0);

	/* Request that the device be re-initialized */
	ice_request_stack_reinit(sc);

	ice_rdma_pf_detach(sc);
	ice_clear_bit(ICE_FEATURE_RDMA, sc->feat_cap);

	ice_clear_bit(ICE_FEATURE_SRIOV, sc->feat_en);
	ice_clear_bit(ICE_FEATURE_SRIOV, sc->feat_cap);

	ice_vsi_del_txqs_ctx(vsi);
	ice_vsi_del_rxqs_ctx(vsi);

	for (i = 0; i < sc->num_available_vsi; i++) {
		if (sc->all_vsi[i])
			ice_release_vsi(sc->all_vsi[i]);
	}
	sc->num_available_vsi = 0;

	if (sc->all_vsi) {
		free(sc->all_vsi, M_ICE);
		sc->all_vsi = NULL;
	}

	/* Destroy the interrupt manager */
	ice_resmgr_destroy(&sc->dev_imgr);
	/* Destroy the queue managers */
	ice_resmgr_destroy(&sc->tx_qmgr);
	ice_resmgr_destroy(&sc->rx_qmgr);

	ice_deinit_hw(&sc->hw);
}

/**
 * ice_transition_safe_mode - Transition to safe mode
 * @sc: the device private softc
 *
 * Called when the driver attempts to reload the DDP package during a device
 * reset, and the new download fails. If so, we must transition to safe mode
 * at run time.
 *
 * @remark although safe mode normally allocates only a single queue, we can't
 * change the number of queues dynamically when using iflib. Due to this, we
 * do not attempt to reduce the number of queues.
 */
static void
ice_transition_safe_mode(struct ice_softc *sc)
{
	/* Indicate that we are in Safe mode */
	ice_set_bit(ICE_FEATURE_SAFE_MODE, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_SAFE_MODE, sc->feat_en);

	ice_rdma_pf_detach(sc);
	ice_clear_bit(ICE_FEATURE_RDMA, sc->feat_cap);

	ice_clear_bit(ICE_FEATURE_SRIOV, sc->feat_en);
	ice_clear_bit(ICE_FEATURE_SRIOV, sc->feat_cap);

	ice_clear_bit(ICE_FEATURE_RSS, sc->feat_cap);
	ice_clear_bit(ICE_FEATURE_RSS, sc->feat_en);
}

/**
 * ice_if_update_admin_status - update admin status
 * @ctx: iflib ctx structure
 *
 * Called by iflib to update the admin status. For our purposes, this means
 * check the adminq, and update the link status. It's ultimately triggered by
 * our admin interrupt, or by the ice_if_timer periodically.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_update_admin_status(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	enum ice_fw_modes fw_mode;
	bool reschedule = false;
	u16 pending = 0;

	ASSERT_CTX_LOCKED(sc);

	/* Check if the firmware entered recovery mode at run time */
	fw_mode = ice_get_fw_mode(&sc->hw);
	if (fw_mode == ICE_FW_MODE_REC) {
		if (!ice_testandset_state(&sc->state, ICE_STATE_RECOVERY_MODE)) {
			/* If we just entered recovery mode, log a warning to
			 * the system administrator and deinit driver state
			 * that is no longer functional.
			 */
			ice_transition_recovery_mode(sc);
		}
	} else if (fw_mode == ICE_FW_MODE_ROLLBACK) {
		if (!ice_testandset_state(&sc->state, ICE_STATE_ROLLBACK_MODE)) {
			/* Rollback mode isn't fatal, but we don't want to
			 * repeatedly post a message about it.
			 */
			ice_print_rollback_msg(&sc->hw);
		}
	}

	/* Handle global reset events */
	ice_handle_reset_event(sc);

	/* Handle PF reset requests */
	ice_handle_pf_reset_request(sc);

	/* Handle MDD events */
	ice_handle_mdd_event(sc);

	if (ice_test_state(&sc->state, ICE_STATE_RESET_FAILED) ||
	    ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET) ||
	    ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE)) {
		/*
		 * If we know the control queues are disabled, skip processing
		 * the control queues entirely.
		 */
		;
	} else if (ice_testandclear_state(&sc->state, ICE_STATE_CONTROLQ_EVENT_PENDING)) {
		ice_process_ctrlq(sc, ICE_CTL_Q_ADMIN, &pending);
		if (pending > 0)
			reschedule = true;

		if (ice_is_generic_mac(&sc->hw)) {
			ice_process_ctrlq(sc, ICE_CTL_Q_SB, &pending);
			if (pending > 0)
				reschedule = true;
		}

		ice_process_ctrlq(sc, ICE_CTL_Q_MAILBOX, &pending);
		if (pending > 0)
			reschedule = true;
	}

	/* Poll for link up */
	ice_poll_for_media_avail(sc);

	/* Check and update link status */
	ice_update_link_status(sc, false);

	/*
	 * If there are still messages to process, we need to reschedule
	 * ourselves. Otherwise, we can just re-enable the interrupt. We'll be
	 * woken up at the next interrupt or timer event.
	 */
	if (reschedule) {
		ice_set_state(&sc->state, ICE_STATE_CONTROLQ_EVENT_PENDING);
		iflib_admin_intr_deferred(ctx);
	} else {
		ice_enable_intr(&sc->hw, sc->irqvs[0].me);
	}
}

/**
 * ice_prepare_for_reset - Prepare device for an impending reset
 * @sc: The device private softc
 *
 * Prepare the driver for an impending reset, shutting down VSIs, clearing the
 * scheduler setup, and shutting down controlqs. Uses the
 * ICE_STATE_PREPARED_FOR_RESET to indicate whether we've already prepared the
 * driver for reset or not.
 */
static void
ice_prepare_for_reset(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;

	/* If we're already prepared, there's nothing to do */
	if (ice_testandset_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET))
		return;

	log(LOG_INFO, "%s: preparing to reset device logic\n", if_name(sc->ifp));

	/* In recovery mode, hardware is not initialized */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	/* inform the RDMA client */
	ice_rdma_notify_reset(sc);
	/* stop the RDMA client */
	ice_rdma_pf_stop(sc);

	/* Release the main PF VSI queue mappings */
	ice_resmgr_release_map(&sc->tx_qmgr, sc->pf_vsi.tx_qmap,
				    sc->pf_vsi.num_tx_queues);
	ice_resmgr_release_map(&sc->rx_qmgr, sc->pf_vsi.rx_qmap,
				    sc->pf_vsi.num_rx_queues);
	if (sc->mirr_if) {
		ice_resmgr_release_map(&sc->tx_qmgr, sc->mirr_if->vsi->tx_qmap,
		    sc->mirr_if->num_irq_vectors);
		ice_resmgr_release_map(&sc->rx_qmgr, sc->mirr_if->vsi->rx_qmap,
		    sc->mirr_if->num_irq_vectors);
	}

	ice_clear_hw_tbls(hw);

	if (hw->port_info)
		ice_sched_cleanup_all(hw);

	ice_shutdown_all_ctrlq(hw, false);
}

/**
 * ice_rebuild_pf_vsi_qmap - Rebuild the main PF VSI queue mapping
 * @sc: the device softc pointer
 *
 * Loops over the Tx and Rx queues for the main PF VSI and reassigns the queue
 * mapping after a reset occurred.
 */
static int
ice_rebuild_pf_vsi_qmap(struct ice_softc *sc)
{
	struct ice_vsi *vsi = &sc->pf_vsi;
	struct ice_tx_queue *txq;
	struct ice_rx_queue *rxq;
	int err, i;

	/* Re-assign Tx queues from PF space to the main VSI */
	err = ice_resmgr_assign_contiguous(&sc->tx_qmgr, vsi->tx_qmap,
					    vsi->num_tx_queues);
	if (err) {
		device_printf(sc->dev, "Unable to re-assign PF Tx queues: %s\n",
			      ice_err_str(err));
		return (err);
	}

	/* Re-assign Rx queues from PF space to this VSI */
	err = ice_resmgr_assign_contiguous(&sc->rx_qmgr, vsi->rx_qmap,
					    vsi->num_rx_queues);
	if (err) {
		device_printf(sc->dev, "Unable to re-assign PF Rx queues: %s\n",
			      ice_err_str(err));
		goto err_release_tx_queues;
	}

	vsi->qmap_type = ICE_RESMGR_ALLOC_CONTIGUOUS;

	/* Re-assign Tx queue tail pointers */
	for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++)
		txq->tail = QTX_COMM_DBELL(vsi->tx_qmap[i]);

	/* Re-assign Rx queue tail pointers */
	for (i = 0, rxq = vsi->rx_queues; i < vsi->num_rx_queues; i++, rxq++)
		rxq->tail = QRX_TAIL(vsi->rx_qmap[i]);

	return (0);

err_release_tx_queues:
	ice_resmgr_release_map(&sc->tx_qmgr, sc->pf_vsi.tx_qmap,
				   sc->pf_vsi.num_tx_queues);

	return (err);
}

/* determine if the iflib context is active */
#define CTX_ACTIVE(ctx) ((if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING))

/**
 * ice_rebuild_recovery_mode - Rebuild driver state while in recovery mode
 * @sc: The device private softc
 *
 * Handle a driver rebuild while in recovery mode. This will only rebuild the
 * limited functionality supported while in recovery mode.
 */
static void
ice_rebuild_recovery_mode(struct ice_softc *sc)
{
	device_t dev = sc->dev;

	/* enable PCIe bus master */
	pci_enable_busmaster(dev);

	/* Configure interrupt causes for the administrative interrupt */
	ice_configure_misc_interrupts(sc);

	/* Enable ITR 0 right away, so that we can handle admin interrupts */
	ice_enable_intr(&sc->hw, sc->irqvs[0].me);

	/* Now that the rebuild is finished, we're no longer prepared to reset */
	ice_clear_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET);

	log(LOG_INFO, "%s: device rebuild successful\n", if_name(sc->ifp));

	/* In order to completely restore device functionality, the iflib core
	 * needs to be reset. We need to request an iflib reset. Additionally,
	 * because the state of IFC_DO_RESET is cached within task_fn_admin in
	 * the iflib core, we also want re-run the admin task so that iflib
	 * resets immediately instead of waiting for the next interrupt.
	 */
	ice_request_stack_reinit(sc);

	return;
}

/**
 * ice_rebuild - Rebuild driver state post reset
 * @sc: The device private softc
 *
 * Restore driver state after a reset occurred. Restart the controlqs, setup
 * the hardware port, and re-enable the VSIs.
 */
static void
ice_rebuild(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum ice_ddp_state pkg_state;
	int status;
	int err;

	sc->rebuild_ticks = ticks;

	/* If we're rebuilding, then a reset has succeeded. */
	ice_clear_state(&sc->state, ICE_STATE_RESET_FAILED);

	/*
	 * If the firmware is in recovery mode, only restore the limited
	 * functionality supported by recovery mode.
	 */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE)) {
		ice_rebuild_recovery_mode(sc);
		return;
	}

	/* enable PCIe bus master */
	pci_enable_busmaster(dev);

	status = ice_init_all_ctrlq(hw);
	if (status) {
		device_printf(dev, "failed to re-init controlqs, err %s\n",
			      ice_status_str(status));
		goto err_shutdown_ctrlq;
	}

	/* Query the allocated resources for Tx scheduler */
	status = ice_sched_query_res_alloc(hw);
	if (status) {
		device_printf(dev,
			      "Failed to query scheduler resources, err %s aq_err %s\n",
			      ice_status_str(status),
			      ice_aq_str(hw->adminq.sq_last_status));
		goto err_shutdown_ctrlq;
	}

	/* Re-enable FW logging. Keep going even if this fails */
	status = ICE_SUCCESS;
	if (hw->pf_id == 0)
		status = ice_fwlog_set(hw, &hw->fwlog_cfg);
	if (!status) {
		/*
		 * We should have the most updated cached copy of the
		 * configuration, regardless of whether we're rebuilding
		 * or not.  So we'll simply check to see if logging was
		 * enabled pre-rebuild.
		 */
		if (hw->fwlog_cfg.options & ICE_FWLOG_OPTION_IS_REGISTERED) {
			status = ice_fwlog_register(hw);
			if (status)
				device_printf(dev, "failed to re-register fw logging, err %s aq_err %s\n",
				   ice_status_str(status),
				   ice_aq_str(hw->adminq.sq_last_status));
		}
	} else
		device_printf(dev, "failed to rebuild fw logging configuration, err %s aq_err %s\n",
		   ice_status_str(status),
		   ice_aq_str(hw->adminq.sq_last_status));

	err = ice_send_version(sc);
	if (err)
		goto err_shutdown_ctrlq;

	err = ice_init_link_events(sc);
	if (err) {
		device_printf(dev, "ice_init_link_events failed: %s\n",
			      ice_err_str(err));
		goto err_shutdown_ctrlq;
	}

	status = ice_clear_pf_cfg(hw);
	if (status) {
		device_printf(dev, "failed to clear PF configuration, err %s\n",
			      ice_status_str(status));
		goto err_shutdown_ctrlq;
	}

	ice_clean_all_vsi_rss_cfg(sc);

	ice_clear_pxe_mode(hw);

	status = ice_get_caps(hw);
	if (status) {
		device_printf(dev, "failed to get capabilities, err %s\n",
			      ice_status_str(status));
		goto err_shutdown_ctrlq;
	}

	status = ice_sched_init_port(hw->port_info);
	if (status) {
		device_printf(dev, "failed to initialize port, err %s\n",
			      ice_status_str(status));
		goto err_sched_cleanup;
	}

	/* If we previously loaded the package, it needs to be reloaded now */
	if (!ice_is_bit_set(sc->feat_en, ICE_FEATURE_SAFE_MODE)) {
		pkg_state = ice_init_pkg(hw, hw->pkg_copy, hw->pkg_size);
		if (!ice_is_init_pkg_successful(pkg_state)) {
			ice_log_pkg_init(sc, pkg_state);
			ice_transition_safe_mode(sc);
		}
	}

	ice_reset_pf_stats(sc);

	err = ice_rebuild_pf_vsi_qmap(sc);
	if (err) {
		device_printf(sc->dev, "Unable to re-assign main VSI queues, err %s\n",
			      ice_err_str(err));
		goto err_sched_cleanup;
	}
	err = ice_initialize_vsi(&sc->pf_vsi);
	if (err) {
		device_printf(sc->dev, "Unable to re-initialize Main VSI, err %s\n",
			      ice_err_str(err));
		goto err_release_queue_allocations;
	}

	/* Replay all VSI configuration */
	err = ice_replay_all_vsi_cfg(sc);
	if (err)
		goto err_deinit_pf_vsi;

	/* Re-enable FW health event reporting */
	ice_init_health_events(sc);

	/* Reconfigure the main PF VSI for RSS */
	err = ice_config_rss(&sc->pf_vsi);
	if (err) {
		device_printf(sc->dev,
			      "Unable to reconfigure RSS for the main VSI, err %s\n",
			      ice_err_str(err));
		goto err_deinit_pf_vsi;
	}

	if (hw->port_info->qos_cfg.is_sw_lldp)
		ice_add_rx_lldp_filter(sc);

	/* Apply previous link settings and refresh link status, if PHY
	 * FW is ready.
	 */
	ice_clear_state(&sc->state, ICE_STATE_LINK_STATUS_REPORTED);
	ice_init_link(sc);

	/* RDMA interface will be restarted by the stack re-init */

	/* Configure interrupt causes for the administrative interrupt */
	ice_configure_misc_interrupts(sc);

	/* Enable ITR 0 right away, so that we can handle admin interrupts */
	ice_enable_intr(&sc->hw, sc->irqvs[0].me);

	/* Now that the rebuild is finished, we're no longer prepared to reset */
	ice_clear_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET);

	/* Reconfigure the subinterface */
	if (sc->mirr_if) {
		err = ice_subif_rebuild(sc);
		if (err)
			goto err_deinit_pf_vsi;
	}

	log(LOG_INFO, "%s: device rebuild successful\n", if_name(sc->ifp));

	/* In order to completely restore device functionality, the iflib core
	 * needs to be reset. We need to request an iflib reset. Additionally,
	 * because the state of IFC_DO_RESET is cached within task_fn_admin in
	 * the iflib core, we also want re-run the admin task so that iflib
	 * resets immediately instead of waiting for the next interrupt.
	 * If LLDP is enabled we need to reconfig DCB to properly reinit all TC
	 * queues, not only 0. It contains ice_request_stack_reinit as well.
	 */
	if (hw->port_info->qos_cfg.is_sw_lldp)
		ice_request_stack_reinit(sc);
	else
		ice_do_dcb_reconfig(sc, false);

	return;

err_deinit_pf_vsi:
	ice_deinit_vsi(&sc->pf_vsi);
err_release_queue_allocations:
	ice_resmgr_release_map(&sc->tx_qmgr, sc->pf_vsi.tx_qmap,
				    sc->pf_vsi.num_tx_queues);
	ice_resmgr_release_map(&sc->rx_qmgr, sc->pf_vsi.rx_qmap,
				    sc->pf_vsi.num_rx_queues);
err_sched_cleanup:
	ice_sched_cleanup_all(hw);
err_shutdown_ctrlq:
	ice_shutdown_all_ctrlq(hw, false);
	ice_clear_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET);
	ice_set_state(&sc->state, ICE_STATE_RESET_FAILED);
	device_printf(dev, "Driver rebuild failed, please reload the device driver\n");
}

/**
 * ice_handle_reset_event - Handle reset events triggered by OICR
 * @sc: The device private softc
 *
 * Handle reset events triggered by an OICR notification. This includes CORER,
 * GLOBR, and EMPR resets triggered by software on this or any other PF or by
 * firmware.
 *
 * @pre assumes the iflib context lock is held, and will unlock it while
 * waiting for the hardware to finish reset.
 */
static void
ice_handle_reset_event(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	int status;
	device_t dev = sc->dev;

	/* When a CORER, GLOBR, or EMPR is about to happen, the hardware will
	 * trigger an OICR interrupt. Our OICR handler will determine when
	 * this occurs and set the ICE_STATE_RESET_OICR_RECV bit as
	 * appropriate.
	 */
	if (!ice_testandclear_state(&sc->state, ICE_STATE_RESET_OICR_RECV))
		return;

	ice_prepare_for_reset(sc);

	/*
	 * Release the iflib context lock and wait for the device to finish
	 * resetting.
	 */
	IFLIB_CTX_UNLOCK(sc);

#define ICE_EMPR_ADDL_WAIT_MSEC_SLOW		20000
	if ((ice_is_e830(hw) || ice_is_e825c(hw)) &&
	    (((rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_RESET_TYPE_M) >>
	         GLGEN_RSTAT_RESET_TYPE_S) == ICE_RESET_EMPR))
			ice_msec_pause(ICE_EMPR_ADDL_WAIT_MSEC_SLOW);

	status = ice_check_reset(hw);
	IFLIB_CTX_LOCK(sc);
	if (status) {
		device_printf(dev, "Device never came out of reset, err %s\n",
			      ice_status_str(status));

		ice_set_state(&sc->state, ICE_STATE_RESET_FAILED);
		ice_clear_state(&sc->state, ICE_STATE_RESET_PFR_REQ);
		ice_clear_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET);
		device_printf(dev, "Reset failed; please reload the device driver\n");
		return;
	}

	/* We're done with the reset, so we can rebuild driver state */
	sc->hw.reset_ongoing = false;
	ice_rebuild(sc);

	/* In the unlikely event that a PF reset request occurs at the same
	 * time as a global reset, clear the request now. This avoids
	 * resetting a second time right after we reset due to a global event.
	 */
	if (ice_testandclear_state(&sc->state, ICE_STATE_RESET_PFR_REQ))
		device_printf(dev, "Ignoring PFR request that occurred while a reset was ongoing\n");
}

/**
 * ice_handle_pf_reset_request - Initiate PF reset requested by software
 * @sc: The device private softc
 *
 * Initiate a PF reset requested by software. We handle this in the admin task
 * so that only one thread actually handles driver preparation and cleanup,
 * rather than having multiple threads possibly attempt to run this code
 * simultaneously.
 *
 * @pre assumes the iflib context lock is held and will unlock it while
 * waiting for the PF reset to complete.
 */
static void
ice_handle_pf_reset_request(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	int status;

	/* Check for PF reset requests */
	if (!ice_testandclear_state(&sc->state, ICE_STATE_RESET_PFR_REQ))
		return;

	/* Make sure we're prepared for reset */
	ice_prepare_for_reset(sc);

	/*
	 * Release the iflib context lock and wait for the device to finish
	 * resetting.
	 */
	IFLIB_CTX_UNLOCK(sc);
	status = ice_reset(hw, ICE_RESET_PFR);
	IFLIB_CTX_LOCK(sc);
	if (status) {
		device_printf(sc->dev, "device PF reset failed, err %s\n",
			      ice_status_str(status));
		ice_set_state(&sc->state, ICE_STATE_RESET_FAILED);
		return;
	}

	sc->soft_stats.pfr_count++;
	ice_rebuild(sc);
}

/**
 * ice_init_device_features - Init device driver features
 * @sc: driver softc structure
 *
 * @pre assumes that the function capabilities bits have been set up by
 * ice_init_hw().
 */
static void
ice_init_device_features(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;

	/* Set capabilities that all devices support */
	ice_set_bit(ICE_FEATURE_SRIOV, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_RSS, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_RDMA, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_LENIENT_LINK_MODE, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_LINK_MGMT_VER_1, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_LINK_MGMT_VER_2, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_HEALTH_STATUS, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_FW_LOGGING, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_HAS_PBA, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_DCB, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_TX_BALANCE, sc->feat_cap);
	ice_set_bit(ICE_FEATURE_PHY_STATISTICS, sc->feat_cap);

	if (ice_is_e810(hw))
		ice_set_bit(ICE_FEATURE_PHY_STATISTICS, sc->feat_en);

	if (ice_is_e825c(hw))
		ice_set_bit(ICE_FEATURE_DUAL_NAC, sc->feat_cap);
	/* Disable features due to hardware limitations... */
	if (!hw->func_caps.common_cap.rss_table_size)
		ice_clear_bit(ICE_FEATURE_RSS, sc->feat_cap);
	if (!hw->func_caps.common_cap.iwarp || !ice_enable_irdma)
		ice_clear_bit(ICE_FEATURE_RDMA, sc->feat_cap);
	if (!hw->func_caps.common_cap.dcb)
		ice_clear_bit(ICE_FEATURE_DCB, sc->feat_cap);
	/* Disable features due to firmware limitations... */
	if (!ice_is_fw_health_report_supported(hw))
		ice_clear_bit(ICE_FEATURE_HEALTH_STATUS, sc->feat_cap);
	if (!ice_fwlog_supported(hw))
		ice_clear_bit(ICE_FEATURE_FW_LOGGING, sc->feat_cap);
	if (hw->fwlog_cfg.options & ICE_FWLOG_OPTION_IS_REGISTERED) {
		if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_FW_LOGGING))
			ice_set_bit(ICE_FEATURE_FW_LOGGING, sc->feat_en);
		else
			ice_fwlog_unregister(hw);
	}

	/* Disable capabilities not supported by the OS */
	ice_disable_unsupported_features(sc->feat_cap);

	/* RSS is always enabled for iflib */
	if (ice_is_bit_set(sc->feat_cap, ICE_FEATURE_RSS))
		ice_set_bit(ICE_FEATURE_RSS, sc->feat_en);

	/* Disable features based on sysctl settings */
	if (!ice_tx_balance_en)
		ice_clear_bit(ICE_FEATURE_TX_BALANCE, sc->feat_cap);

	if (hw->dev_caps.supported_sensors & ICE_SENSOR_SUPPORT_E810_INT_TEMP) {
		ice_set_bit(ICE_FEATURE_TEMP_SENSOR, sc->feat_cap);
		ice_set_bit(ICE_FEATURE_TEMP_SENSOR, sc->feat_en);
	}

	if (hw->func_caps.common_cap.next_cluster_id_support ||
	    hw->dev_caps.common_cap.next_cluster_id_support) {
		ice_set_bit(ICE_FEATURE_NEXT_CLUSTER_ID, sc->feat_cap);
		ice_set_bit(ICE_FEATURE_NEXT_CLUSTER_ID, sc->feat_en);
	}
}

/**
 * ice_if_multi_set - Callback to update Multicast filters in HW
 * @ctx: iflib ctx structure
 *
 * Called by iflib in response to SIOCDELMULTI and SIOCADDMULTI. Must search
 * the if_multiaddrs list and determine which filters have been added or
 * removed from the list, and update HW programming to reflect the new list.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_multi_set(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	int err;

	ASSERT_CTX_LOCKED(sc);

	/* Do not handle multicast configuration in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	err = ice_sync_multicast_filters(sc);
	if (err) {
		device_printf(sc->dev,
			      "Failed to synchronize multicast filter list: %s\n",
			      ice_err_str(err));
		return;
	}
}

/**
 * ice_if_vlan_register - Register a VLAN with the hardware
 * @ctx: iflib ctx pointer
 * @vtag: VLAN to add
 *
 * Programs the main PF VSI with a hardware filter for the given VLAN.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_vlan_register(if_ctx_t ctx, u16 vtag)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	int status;

	ASSERT_CTX_LOCKED(sc);

	/* Do not handle VLAN configuration in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	status = ice_add_vlan_hw_filter(&sc->pf_vsi, vtag);
	if (status) {
		device_printf(sc->dev,
			      "Failure adding VLAN %d to main VSI, err %s aq_err %s\n",
			      vtag, ice_status_str(status),
			      ice_aq_str(sc->hw.adminq.sq_last_status));
	}
}

/**
 * ice_if_vlan_unregister - Remove a VLAN filter from the hardware
 * @ctx: iflib ctx pointer
 * @vtag: VLAN to add
 *
 * Removes the previously programmed VLAN filter from the main PF VSI.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_vlan_unregister(if_ctx_t ctx, u16 vtag)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	int status;

	ASSERT_CTX_LOCKED(sc);

	/* Do not handle VLAN configuration in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	status = ice_remove_vlan_hw_filter(&sc->pf_vsi, vtag);
	if (status) {
		device_printf(sc->dev,
			      "Failure removing VLAN %d from main VSI, err %s aq_err %s\n",
			      vtag, ice_status_str(status),
			      ice_aq_str(sc->hw.adminq.sq_last_status));
	}
}

/**
 * ice_if_stop - Stop the device
 * @ctx: iflib context structure
 *
 * Called by iflib to stop the device and bring it down. (i.e. ifconfig ice0
 * down)
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_if_stop(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	ASSERT_CTX_LOCKED(sc);

	/*
	 * The iflib core may call IFDI_STOP prior to the first call to
	 * IFDI_INIT. This will cause us to attempt to remove MAC filters we
	 * don't have, and disable Tx queues which aren't yet configured.
	 * Although it is likely these extra operations are harmless, they do
	 * cause spurious warning messages to be displayed, which may confuse
	 * users.
	 *
	 * To avoid these messages, we use a state bit indicating if we've
	 * been initialized. It will be set when ice_if_init is called, and
	 * cleared here in ice_if_stop.
	 */
	if (!ice_testandclear_state(&sc->state, ICE_STATE_DRIVER_INITIALIZED))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RESET_FAILED)) {
		device_printf(sc->dev, "request to stop interface cannot be completed as the device failed to reset\n");
		return;
	}

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(sc->dev, "request to stop interface while device is prepared for impending reset\n");
		return;
	}

	ice_rdma_pf_stop(sc);

	/* Remove the MAC filters, stop Tx, and stop Rx. We don't check the
	 * return of these functions because there's nothing we can really do
	 * if they fail, and the functions already print error messages.
	 * Just try to shut down as much as we can.
	 */
	ice_rm_pf_default_mac_filters(sc);

	/* Dissociate the Tx and Rx queues from the interrupts */
	ice_flush_txq_interrupts(&sc->pf_vsi);
	ice_flush_rxq_interrupts(&sc->pf_vsi);

	/* Disable the Tx and Rx queues */
	ice_vsi_disable_tx(&sc->pf_vsi);
	ice_control_all_rx_queues(&sc->pf_vsi, false);

	if (!ice_test_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN) &&
		 !(if_getflags(sc->ifp) & IFF_UP) && sc->link_up)
		ice_set_link(sc, false);

	if (sc->mirr_if && ice_test_state(&mif->state, ICE_STATE_SUBIF_NEEDS_REINIT)) {
		ice_subif_if_stop(sc->mirr_if->subctx);
		device_printf(sc->dev, "The subinterface also comes down and up after reset\n");
	}
}

/**
 * ice_if_get_counter - Get current value of an ifnet statistic
 * @ctx: iflib context pointer
 * @counter: ifnet counter to read
 *
 * Reads the current value of an ifnet counter for the device.
 *
 * This function is not protected by the iflib CTX lock.
 */
static uint64_t
ice_if_get_counter(if_ctx_t ctx, ift_counter counter)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	/* Return the counter for the main PF VSI */
	return ice_get_ifnet_counter(&sc->pf_vsi, counter);
}

/**
 * ice_request_stack_reinit - Request that iflib re-initialize
 * @sc: the device private softc
 *
 * Request that the device be brought down and up, to re-initialize. For
 * example, this may be called when a device reset occurs, or when Tx and Rx
 * queues need to be re-initialized.
 *
 * This is required because the iflib state is outside the driver, and must be
 * re-initialized if we need to resart Tx and Rx queues.
 */
void
ice_request_stack_reinit(struct ice_softc *sc)
{
	if (CTX_ACTIVE(sc->ctx)) {
		iflib_request_reset(sc->ctx);
		iflib_admin_intr_deferred(sc->ctx);
	}
}

/**
 * ice_driver_is_detaching - Check if the driver is detaching/unloading
 * @sc: device private softc
 *
 * Returns true if the driver is detaching, false otherwise.
 *
 * @remark on newer kernels, take advantage of iflib_in_detach in order to
 * report detachment correctly as early as possible.
 *
 * @remark this function is used by various code paths that want to avoid
 * running if the driver is about to be removed. This includes sysctls and
 * other driver access points. Note that it does not fully resolve
 * detach-based race conditions as it is possible for a thread to race with
 * iflib_in_detach.
 */
bool
ice_driver_is_detaching(struct ice_softc *sc)
{
	return (ice_test_state(&sc->state, ICE_STATE_DETACHING) ||
		iflib_in_detach(sc->ctx));
}

/**
 * ice_if_priv_ioctl - Device private ioctl handler
 * @ctx: iflib context pointer
 * @command: The ioctl command issued
 * @data: ioctl specific data
 *
 * iflib callback for handling custom driver specific ioctls.
 *
 * @pre Assumes that the iflib context lock is held.
 */
static int
ice_if_priv_ioctl(if_ctx_t ctx, u_long command, caddr_t data)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);
	struct ifdrv *ifd;
	device_t dev = sc->dev;

	if (data == NULL)
		return (EINVAL);

	ASSERT_CTX_LOCKED(sc);

	/* Make sure the command type is valid */
	switch (command) {
	case SIOCSDRVSPEC:
	case SIOCGDRVSPEC:
		/* Accepted commands */
		break;
	case SIOCGPRIVATE_0:
		/*
		 * Although we do not support this ioctl command, it's
		 * expected that iflib will forward it to the IFDI_PRIV_IOCTL
		 * handler. Do not print a message in this case
		 */
		return (ENOTSUP);
	default:
		/*
		 * If we get a different command for this function, it's
		 * definitely unexpected, so log a message indicating what
		 * command we got for debugging purposes.
		 */
		device_printf(dev, "%s: unexpected ioctl command %08lx\n",
			      __func__, command);
		return (EINVAL);
	}

	ifd = (struct ifdrv *)data;

	switch (ifd->ifd_cmd) {
	case ICE_NVM_ACCESS:
		return ice_handle_nvm_access_ioctl(sc, ifd);
	case ICE_DEBUG_DUMP:
		return ice_handle_debug_dump_ioctl(sc, ifd);
	default:
		return EINVAL;
	}
}

/**
 * ice_if_i2c_req - I2C request handler for iflib
 * @ctx: iflib context pointer
 * @req: The I2C parameters to use
 *
 * Read from the port's I2C eeprom using the parameters from the ioctl.
 *
 * @remark The iflib-only part is pretty simple.
 */
static int
ice_if_i2c_req(if_ctx_t ctx, struct ifi2creq *req)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	return ice_handle_i2c_req(sc, req);
}

/**
 * ice_if_suspend - PCI device suspend handler for iflib
 * @ctx: iflib context pointer
 *
 * Deinitializes the driver and clears HW resources in preparation for
 * suspend or an FLR.
 *
 * @returns 0; this return value is ignored
 */
static int
ice_if_suspend(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	/* At least a PFR is always going to happen after this;
	 * either via FLR or during the D3->D0 transition.
	 */
	ice_clear_state(&sc->state, ICE_STATE_RESET_PFR_REQ);

	ice_prepare_for_reset(sc);

	return (0);
}

/**
 * ice_if_resume - PCI device resume handler for iflib
 * @ctx: iflib context pointer
 *
 * Reinitializes the driver and the HW after PCI resume or after
 * an FLR. An init is performed by iflib after this function is finished.
 *
 * @returns 0; this return value is ignored
 */
static int
ice_if_resume(if_ctx_t ctx)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	ice_rebuild(sc);

	return (0);
}

/**
 * ice_if_needs_restart - Tell iflib when the driver needs to be reinitialized
 * @ctx: iflib context pointer
 * @event: event code to check
 *
 * Defaults to returning true for unknown events.
 *
 * @returns true if iflib needs to reinit the interface
 */
static bool
ice_if_needs_restart(if_ctx_t ctx, enum iflib_restart_event event)
{
	struct ice_softc *sc = (struct ice_softc *)iflib_get_softc(ctx);

	switch (event) {
	case IFLIB_RESTART_VLAN_CONFIG:
		if (!ice_test_state(&sc->state, ICE_STATE_LINK_ACTIVE_ON_DOWN) &&
			 !(if_getflags(sc->ifp) & IFF_UP))
			return false;
	default:
		return true;
	}
}

/**
 * ice_init_link - Do link configuration and link status reporting
 * @sc: driver private structure
 *
 * Contains an extra check that skips link config when an E830 device
 * does not have the "FW_LOADING"/"PHYBUSY" bit set in GL_MNG_FWSM set.
 */
static void
ice_init_link(struct ice_softc *sc)
{
	struct ice_hw *hw = &sc->hw;
	device_t dev = sc->dev;

	/* Check if FW is ready before setting up link; defer setup to the
	 * admin task if it isn't.
	 */
	if (ice_is_e830(hw) &&
	    (rd32(hw, GL_MNG_FWSM) & GL_MNG_FWSM_FW_LOADING_M)) {
		ice_set_state(&sc->state, ICE_STATE_PHY_FW_INIT_PENDING);
		device_printf(dev,
		    "Link initialization is blocked by PHY FW initialization.\n");
		device_printf(dev,
		    "Link initialization will continue after PHY FW initialization completes.\n");
		/* Do not access PHY config while PHY FW is busy initializing */
	} else {
		ice_clear_state(&sc->state, ICE_STATE_PHY_FW_INIT_PENDING);
		ice_init_link_configuration(sc);
		ice_update_link_status(sc, true);
	}

}

extern struct if_txrx ice_subif_txrx;

/**
 * @var ice_subif_methods
 * @brief ice driver method entry points
 */
static device_method_t ice_subif_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, ice_subif_register),
	DEVMETHOD_END
};

/**
 * @var ice_subif_driver
 * @brief driver structure for the device API
 */
static driver_t ice_subif_driver = {
	.name = "ice_subif",
	.methods = ice_subif_methods,
	.size = sizeof(struct ice_mirr_if),
};

static device_method_t ice_iflib_subif_methods[] = {
	DEVMETHOD(ifdi_attach_pre, ice_subif_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, ice_subif_if_attach_post),
	DEVMETHOD(ifdi_tx_queues_alloc, ice_subif_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, ice_subif_if_rx_queues_alloc),
	DEVMETHOD(ifdi_msix_intr_assign, ice_subif_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, ice_subif_if_intr_enable),
	DEVMETHOD(ifdi_rx_queue_intr_enable, ice_subif_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, ice_subif_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_init, ice_subif_if_init),
	DEVMETHOD(ifdi_stop, ice_subif_if_stop),
	DEVMETHOD(ifdi_queues_free, ice_subif_if_queues_free),
	DEVMETHOD(ifdi_media_status, ice_subif_if_media_status),
	DEVMETHOD(ifdi_promisc_set, ice_subif_if_promisc_set),
};

/**
 * @var ice_iflib_subif_driver
 * @brief driver structure for the iflib stack
 *
 * driver_t definition used to setup the iflib device methods.
 */
static driver_t ice_iflib_subif_driver = {
	.name = "ice_subif",
	.methods = ice_iflib_subif_methods,
	.size = sizeof(struct ice_mirr_if),
};

/**
 * @var ice_subif_sctx
 * @brief ice driver shared context
 *
 * Similar to the existing ice_sctx, this structure has these differences:
 * - isc_admin_intrcnt is set to 0
 * - Uses subif iflib driver methods
 * - Flagged as a VF for iflib
 */
static struct if_shared_ctx ice_subif_sctx = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,

	.isc_tx_maxsize = ICE_MAX_FRAME_SIZE,
	.isc_tx_maxsegsize = ICE_MAX_FRAME_SIZE,
	.isc_tso_maxsize = ICE_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = ICE_MAX_DMA_SEG_SIZE,

	.isc_rx_maxsize = ICE_MAX_FRAME_SIZE,
	.isc_rx_nsegments = ICE_MAX_RX_SEGS,
	.isc_rx_maxsegsize = ICE_MAX_FRAME_SIZE,

	.isc_nfl = 1,
	.isc_ntxqs = 1,
	.isc_nrxqs = 1,

	.isc_admin_intrcnt = 0,
	.isc_vendor_info = ice_vendor_info_array,
	.isc_driver_version = __DECONST(char *, ice_driver_version),
	.isc_driver = &ice_iflib_subif_driver,

	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_TSO_INIT_IP |
		IFLIB_ADMIN_ALWAYS_RUN | IFLIB_SKIP_MSIX |
		IFLIB_IS_VF,

	.isc_nrxd_min = {ICE_MIN_DESC_COUNT},
	.isc_ntxd_min = {ICE_MIN_DESC_COUNT},
	.isc_nrxd_max = {ICE_IFLIB_MAX_DESC_COUNT},
	.isc_ntxd_max = {ICE_IFLIB_MAX_DESC_COUNT},
	.isc_nrxd_default = {ICE_DEFAULT_DESC_COUNT},
	.isc_ntxd_default = {ICE_DEFAULT_DESC_COUNT},
};

static void *
ice_subif_register(device_t dev __unused)
{
	return (&ice_subif_sctx);
}

static void
ice_subif_setup_scctx(struct ice_mirr_if *mif)
{
	if_softc_ctx_t scctx = mif->subscctx;

	scctx->isc_txrx = &ice_subif_txrx;

	scctx->isc_capenable = ICE_FULL_CAPS;
	scctx->isc_tx_csum_flags = ICE_CSUM_OFFLOAD;

	scctx->isc_ntxqsets = 4;
	scctx->isc_nrxqsets = 4;
	scctx->isc_vectors = scctx->isc_nrxqsets;

	scctx->isc_ntxqsets_max = 256;
	scctx->isc_nrxqsets_max = 256;

	scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0]
	    * sizeof(struct ice_tx_desc), DBA_ALIGN);
	scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0]
	    * sizeof(union ice_32b_rx_flex_desc), DBA_ALIGN);

	scctx->isc_tx_nsegments = ICE_MAX_TX_SEGS;
	scctx->isc_tx_tso_segments_max = ICE_MAX_TSO_SEGS;
	scctx->isc_tx_tso_size_max = ICE_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = ICE_MAX_DMA_SEG_SIZE;
}

static int
ice_subif_if_attach_pre(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	device_t dev = iflib_get_dev(ctx);

	mif->subctx = ctx;
	mif->subdev = dev;
	mif->subscctx = iflib_get_softc_ctx(ctx);

	/* Setup the iflib softc context structure */
	ice_subif_setup_scctx(mif);

	return (0);
}

static int
ice_subif_if_attach_post(if_ctx_t ctx __unused)
{
	return (0);
}

/**
 * ice_destroy_mirror_interface - destroy mirror interface
 * @sc: driver private data
 *
 * Destroys all resources associated with the mirroring interface.
 * Will not exit early on failure.
 *
 * @pre: Mirror interface already exists and is initialized.
 */
void
ice_destroy_mirror_interface(struct ice_softc *sc)
{
	struct ice_mirr_if *mif = sc->mirr_if;
	struct ice_vsi *vsi = mif->vsi;
	bool is_locked = false;
	int ret;

	is_locked = sx_xlocked(sc->iflib_ctx_lock);
	if (is_locked)
		IFLIB_CTX_UNLOCK(sc);

	if (mif->ifp) {
		ret = iflib_device_deregister(mif->subctx);
		if (ret) {
			device_printf(sc->dev,
			    "iflib_device_deregister for mirror interface failed: %d\n",
			    ret);
		}
	}

	bus_topo_lock();
	ret = device_delete_child(sc->dev, mif->subdev);
	bus_topo_unlock();
	if (ret) {
		device_printf(sc->dev,
		    "device_delete_child for mirror interface failed: %d\n",
		    ret);
	}

	if (is_locked)
		IFLIB_CTX_LOCK(sc);

	if (mif->if_imap) {
		free(mif->if_imap, M_ICE);
		mif->if_imap = NULL;
	}
	if (mif->os_imap) {
		free(mif->os_imap, M_ICE);
		mif->os_imap = NULL;
	}

	/* These are freed via ice_subif_queues_free_subif
	 * vsi:
	 * - rx_irqvs
	 * - tx_queues
	 * - rx_queues
	 */
	ice_release_vsi(vsi);

	free(mif, M_ICE);
	sc->mirr_if = NULL;

}

/**
 * ice_setup_mirror_vsi - Initialize mirror VSI
 * @mif: driver private data for mirror interface
 *
 * Allocates a VSI for a mirror interface, and sets that VSI up for use as a
 * mirror for the main PF VSI.
 *
 * Returns 0 on success, or a standard error code on failure.
 */
static int
ice_setup_mirror_vsi(struct ice_mirr_if *mif)
{
	struct ice_softc *sc = mif->back;
	device_t dev = sc->dev;
	struct ice_vsi *vsi;
	int ret = 0;

	/* vsi is for the new mirror vsi, not the PF's main VSI */
	vsi = ice_alloc_vsi(sc, ICE_VSI_VMDQ2);
	if (!vsi) {
		/* Already prints an error message */
		return (ENOMEM);
	}
	mif->vsi = vsi;

	/* Reserve VSI queue allocation from PF queues */
	ice_alloc_vsi_qmap(vsi, ICE_DEFAULT_VF_QUEUES, ICE_DEFAULT_VF_QUEUES);
	vsi->num_tx_queues = vsi->num_rx_queues = ICE_DEFAULT_VF_QUEUES;

	/* Assign Tx queues from PF space */
	ret = ice_resmgr_assign_scattered(&sc->tx_qmgr, vsi->tx_qmap,
	    vsi->num_tx_queues);
	if (ret) {
		device_printf(dev, "Unable to assign mirror VSI Tx queues: %s\n",
		    ice_err_str(ret));
		goto release_vsi;
	}
	/* Assign Rx queues from PF space */
	ret = ice_resmgr_assign_scattered(&sc->rx_qmgr, vsi->rx_qmap,
	    vsi->num_rx_queues);
	if (ret) {
		device_printf(dev, "Unable to assign mirror VSI Rx queues: %s\n",
		    ice_err_str(ret));
		goto release_vsi;
	}
	vsi->qmap_type = ICE_RESMGR_ALLOC_SCATTERED;
	vsi->max_frame_size = ICE_MAX_FRAME_SIZE;

	ret = ice_initialize_vsi(vsi);
	if (ret) {
		device_printf(dev, "%s: Error in ice_initialize_vsi for mirror VSI: %s\n",
		    __func__, ice_err_str(ret));
		goto release_vsi;
	}

	/* Setup this VSI for receiving traffic */
	ret = ice_config_rss(vsi);
	if (ret) {
		device_printf(dev,
		    "Unable to configure RSS for mirror VSI: %s\n",
		    ice_err_str(ret));
		goto release_vsi;
	}

	/* Set HW rules for mirroring traffic */
	vsi->mirror_src_vsi = sc->pf_vsi.idx;

	ice_debug(&sc->hw, ICE_DBG_INIT,
	    "Configuring mirroring from VSI %d to %d\n",
	    vsi->mirror_src_vsi, vsi->idx);
	ice_debug(&sc->hw, ICE_DBG_INIT, "(HW num: VSI %d to %d)\n",
	    ice_get_hw_vsi_num(&sc->hw, vsi->mirror_src_vsi),
	    ice_get_hw_vsi_num(&sc->hw, vsi->idx));

	ret = ice_setup_vsi_mirroring(vsi);
	if (ret) {
		device_printf(dev,
		    "Unable to configure mirroring for VSI: %s\n",
		    ice_err_str(ret));
		goto release_vsi;
	}

	return (0);

release_vsi:
	ice_release_vsi(vsi);
	mif->vsi = NULL;
	return (ret);
}

/**
 * ice_create_mirror_interface - Initialize mirror interface
 * @sc: driver private data
 *
 * Creates and sets up a mirror interface that will mirror traffic from
 * the main PF interface. Includes a call to iflib_device_register() in order
 * to setup necessary iflib structures for this new interface as well.
 *
 * If it returns successfully, a new interface will be created and will show
 * up in the ifconfig interface list.
 *
 * Returns 0 on success, or a standard error code on failure.
 */
int
ice_create_mirror_interface(struct ice_softc *sc)
{
	device_t dev = sc->dev;
	struct ice_mirr_if *mif;
	struct ifmedia *media;
	struct sbuf *sb;
	int ret = 0;

	mif = (struct ice_mirr_if *)malloc(sizeof(*mif), M_ICE, M_ZERO | M_NOWAIT);
	if (!mif) {
		device_printf(dev, "malloc() error allocating mirror interface\n");
		return (ENOMEM);
	}

	/* Set pointers */
	sc->mirr_if = mif;
	mif->back = sc;

	/* Do early setup because these will be called during iflib_device_register():
	 * - ice_subif_if_tx_queues_alloc
	 * - ice_subif_if_rx_queues_alloc
	 */
	ret = ice_setup_mirror_vsi(mif);
	if (ret)
		goto out;

	/* Determine name for new interface:
	 * (base interface name)(modifier name)(modifier unit number)
	 * e.g. for ice0 with a new mirror interface (modifier m)
	 * of index 0, this equals "ice0m0"
	 */
	sb = sbuf_new_auto();
	MPASS(sb != NULL);
	sbuf_printf(sb, "%sm", device_get_nameunit(dev));
	sbuf_finish(sb);

	bus_topo_lock();
	mif->subdev = device_add_child(dev, sbuf_data(sb), 0);
	bus_topo_unlock();

	if (!mif->subdev) {
		device_printf(dev, "device_add_child failed for %s0\n", sbuf_data(sb));
		sbuf_delete(sb);
		free(mif, M_ICE);
		sc->mirr_if = NULL;
		return (ENOMEM);
	}
	sbuf_delete(sb);

	device_set_driver(mif->subdev, &ice_subif_driver);

	/* Use iflib_device_register() directly because the driver already
	 * has an initialized softc to pass to iflib
	 */
	ret = iflib_device_register(mif->subdev, mif, &ice_subif_sctx, &mif->subctx);
	if (ret)
		goto out;

	/* Indicate that created interface will be just for monitoring */
	mif->ifp = iflib_get_ifp(mif->subctx);
	if_setflagbits(mif->ifp, IFF_MONITOR, 0);

	/* Use autoselect media by default */
	media = iflib_get_media(mif->subctx);
	ifmedia_add(media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(media, IFM_ETHER | IFM_AUTO);

	device_printf(dev, "Created dev %s and ifnet %s for mirroring\n",
	    device_get_nameunit(mif->subdev), if_name(mif->ifp));

	ice_add_vsi_sysctls(mif->vsi);

	ret = ice_wire_mirror_intrs(mif);
	if (ret)
		goto out;

	mif->if_attached = true;
	return (0);

out:
	ice_destroy_mirror_interface(sc);
	return (ret);
}

/**
 * ice_wire_mirror_intrs
 * @mif: driver private subinterface structure
 *
 * Helper function that sets up driver interrupt data and calls
 * into iflib in order to setup interrupts in its data structures as well.
 * 
 * Like ice_if_msix_intr_assign, currently requires that we get at least the same
 * number of vectors as we have queues, and that we always have the same number
 * of Tx and Rx queues. Unlike that function, this calls a special
 * iflib_irq_alloc_generic_subif() function for RX interrupts because the
 * driver needs to get MSI-X resources from the parent device.
 *
 * Tx queues use a softirq instead of using their own hardware interrupt so that
 * remains unchanged.
 *
 * Returns 0 on success or an error code from iflib_irq_alloc_generic_subctx()
 * on failure.
 */
static int
ice_wire_mirror_intrs(struct ice_mirr_if *mif)
{
	struct ice_softc *sc = mif->back;
	struct ice_hw *hw = &sc->hw;
	struct ice_vsi *vsi = mif->vsi;
	device_t dev = mif->subdev;
	int err, i, rid;

	if_ctx_t ctx = mif->subctx;

	ice_debug(hw, ICE_DBG_INIT, "%s: Last rid: %d\n", __func__, sc->last_rid);

	rid = sc->last_rid + 1;
	for (i = 0; i < vsi->num_rx_queues; i++, rid++) {
		struct ice_rx_queue *rxq = &vsi->rx_queues[i];
		struct ice_tx_queue *txq = &vsi->tx_queues[i];
		char irq_name[16];

		// TODO: Change to use dynamic interface number
		snprintf(irq_name, sizeof(irq_name), "m0rxq%d", i);
		/* First arg is parent device (physical port's) iflib ctx */
		err = iflib_irq_alloc_generic_subctx(sc->ctx, ctx,
		    &mif->rx_irqvs[i].irq, rid, IFLIB_INTR_RXTX, ice_msix_que, 
		    rxq, rxq->me, irq_name);
		if (err) {
			device_printf(dev,
			    "Failed to allocate q int %d err: %s\n",
			    i, ice_err_str(err));
			i--;
			goto fail;
		}
		MPASS(rid - 1 > 0);
		/* Set vector number used in interrupt enable/disable functions */
		mif->rx_irqvs[i].me = rid - 1;
		rxq->irqv = &mif->rx_irqvs[i];

		bzero(irq_name, sizeof(irq_name));
		snprintf(irq_name, sizeof(irq_name), "m0txq%d", i);
		iflib_softirq_alloc_generic(ctx, &mif->rx_irqvs[i].irq,
		    IFLIB_INTR_TX, txq, txq->me, irq_name);
		txq->irqv = &mif->rx_irqvs[i];
	}

	sc->last_rid = rid - 1;

	ice_debug(hw, ICE_DBG_INIT, "%s: New last rid: %d\n", __func__,
	    sc->last_rid);

	return (0);

fail:
	for (; i >= 0; i--)
		iflib_irq_free(ctx, &mif->rx_irqvs[i].irq);
	return (err);
}

/**
 * ice_subif_rebuild - Rebuild subinterface post reset
 * @sc: The device private softc
 *
 * Restore subinterface state after a reset occurred.
 * Restart the VSI and enable the mirroring.
 */
static int
ice_subif_rebuild(struct ice_softc *sc)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(sc->ctx);
	struct ice_vsi *vsi = sc->mirr_if->vsi;
	int err;

	err = ice_subif_rebuild_vsi_qmap(sc);
	if (err) {
		device_printf(sc->dev, "Unable to re-assign mirror VSI queues, err %s\n",
		      ice_err_str(err));
		return (err);
	}

	err = ice_initialize_vsi(vsi);
	if (err) {
		device_printf(sc->dev, "Unable to re-initialize mirror VSI, err %s\n",
		      ice_err_str(err));
		goto err_release_queue_allocations_subif;
	}

	err = ice_config_rss(vsi);
	if (err) {
		device_printf(sc->dev,
		      "Unable to reconfigure RSS for the mirror VSI, err %s\n",
		      ice_err_str(err));
		goto err_deinit_subif_vsi;
	}

	vsi->mirror_src_vsi = sc->pf_vsi.idx;

	err = ice_setup_vsi_mirroring(vsi);
	if (err) {
		device_printf(sc->dev,
		      "Unable to configure mirroring for VSI: %s\n",
		      ice_err_str(err));
		goto err_deinit_subif_vsi;
	}

	ice_set_state(&mif->state, ICE_STATE_SUBIF_NEEDS_REINIT);

	return (0);

err_deinit_subif_vsi:
	ice_deinit_vsi(vsi);
err_release_queue_allocations_subif:
	ice_resmgr_release_map(&sc->tx_qmgr, vsi->tx_qmap,
	    sc->mirr_if->num_irq_vectors);
	ice_resmgr_release_map(&sc->rx_qmgr, vsi->rx_qmap,
	    sc->mirr_if->num_irq_vectors);

	return (err);
}

/**
 * ice_subif_rebuild_vsi_qmap - Rebuild the mirror VSI queue mapping
 * @sc: the device softc pointer
 *
 * Loops over the Tx and Rx queues for the mirror VSI and reassigns the queue
 * mapping after a reset occurred.
 */
static int
ice_subif_rebuild_vsi_qmap(struct ice_softc *sc)
{
	struct ice_vsi *vsi = sc->mirr_if->vsi;
	struct ice_tx_queue *txq;
	struct ice_rx_queue *rxq;
	int err, i;

	err = ice_resmgr_assign_scattered(&sc->tx_qmgr, vsi->tx_qmap, sc->mirr_if->num_irq_vectors);
	if (err) {
		device_printf(sc->dev, "Unable to assign mirror VSI Tx queues: %s\n",
		      ice_err_str(err));
		return (err);
	}

	err = ice_resmgr_assign_scattered(&sc->rx_qmgr, vsi->rx_qmap, sc->mirr_if->num_irq_vectors);
	if (err) {
		device_printf(sc->dev, "Unable to assign mirror VSI Rx queues: %s\n",
		      ice_err_str(err));
		goto err_release_tx_queues;
	}

	vsi->qmap_type = ICE_RESMGR_ALLOC_SCATTERED;

	/* Re-assign Tx queue tail pointers */
	for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++)
		txq->tail = QTX_COMM_DBELL(vsi->tx_qmap[i]);

	/* Re-assign Rx queue tail pointers */
	for (i = 0, rxq = vsi->rx_queues; i < vsi->num_rx_queues; i++, rxq++)
		rxq->tail = QRX_TAIL(vsi->rx_qmap[i]);

	return (0);

err_release_tx_queues:
	ice_resmgr_release_map(&sc->tx_qmgr, vsi->tx_qmap, vsi->num_tx_queues);

	return (err);
}

/**
 * ice_subif_if_tx_queues_alloc - Allocate Tx queue memory for subinterfaces
 * @ctx: iflib context structure
 * @vaddrs: virtual addresses for the queue memory
 * @paddrs: physical addresses for the queue memory
 * @ntxqs: the number of Tx queues per set (should always be 1)
 * @ntxqsets: the number of Tx queue sets to allocate
 *
 * See ice_if_tx_queues_alloc() description. Similar to that function, but
 * for subinterfaces instead.
 */
static int
ice_subif_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
			     int __invariant_only ntxqs, int ntxqsets)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_tx_queue *txq;
	device_t dev = mif->subdev;
	struct ice_vsi *vsi;
	int err, i, j;

	MPASS(mif != NULL);
	MPASS(ntxqs == 1);
	MPASS(mif->subscctx->isc_ntxd[0] <= ICE_MAX_DESC_COUNT);

	vsi = mif->vsi;

	MPASS(vsi->num_tx_queues == ntxqsets);

	/* Allocate queue structure memory */
	if (!(vsi->tx_queues =
	      (struct ice_tx_queue *)malloc(sizeof(struct ice_tx_queue) * ntxqsets, M_ICE, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "%s: Unable to allocate Tx queue memory for subfunction\n",
		    __func__);
		return (ENOMEM);
	}

	/* Allocate report status arrays */
	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		if (!(txq->tx_rsq =
		      (uint16_t *)malloc(sizeof(uint16_t) * mif->subscctx->isc_ntxd[0], M_ICE, M_NOWAIT))) {
			device_printf(dev,
			    "%s: Unable to allocate tx_rsq memory for subfunction\n", __func__);
			err = ENOMEM;
			goto free_tx_queues;
		}
		/* Initialize report status array */
		for (j = 0; j < mif->subscctx->isc_ntxd[0]; j++)
			txq->tx_rsq[j] = QIDX_INVALID;
	}

	/* Add Tx queue sysctls context */
	ice_vsi_add_txqs_ctx(vsi);

	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		/* q_handle == me when only one TC */
		txq->me = txq->q_handle = i;
		txq->vsi = vsi;

		/* store the queue size for easier access */
		txq->desc_count = mif->subscctx->isc_ntxd[0];

		/* get the virtual and physical address of the hardware queues */
		txq->tail = QTX_COMM_DBELL(vsi->tx_qmap[i]);
		txq->tx_base = (struct ice_tx_desc *)vaddrs[i];
		txq->tx_paddr = paddrs[i];

		ice_add_txq_sysctls(txq);
	}

	return (0);

free_tx_queues:
	for (i = 0, txq = vsi->tx_queues; i < ntxqsets; i++, txq++) {
		if (txq->tx_rsq != NULL) {
			free(txq->tx_rsq, M_ICE);
			txq->tx_rsq = NULL;
		}
	}
	free(vsi->tx_queues, M_ICE);
	vsi->tx_queues = NULL;
	return (err);
}

/**
 * ice_subif_if_rx_queues_alloc - Allocate Rx queue memory for subinterfaces
 * @ctx: iflib context structure
 * @vaddrs: virtual addresses for the queue memory
 * @paddrs: physical addresses for the queue memory
 * @nrxqs: number of Rx queues per set (should always be 1)
 * @nrxqsets: number of Rx queue sets to allocate
 *
 * See ice_if_rx_queues_alloc() for general summary; this is similar to that
 * but implemented for subinterfaces.
 */
static int
ice_subif_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs,
    int __invariant_only nrxqs, int nrxqsets)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_rx_queue *rxq;
	device_t dev = mif->subdev;
	struct ice_vsi *vsi;
	int i;

	MPASS(mif != NULL);
	MPASS(nrxqs == 1);
	MPASS(mif->subscctx->isc_nrxd[0] <= ICE_MAX_DESC_COUNT);

	vsi = mif->vsi;

	MPASS(vsi->num_rx_queues == nrxqsets);

	/* Allocate queue structure memory */
	if (!(vsi->rx_queues =
	      (struct ice_rx_queue *) malloc(sizeof(struct ice_rx_queue) * nrxqsets, M_ICE, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "%s: Unable to allocate Rx queue memory for subfunction\n",
		    __func__);
		return (ENOMEM);
	}

	/* Add Rx queue sysctls context */
	ice_vsi_add_rxqs_ctx(vsi);

	for (i = 0, rxq = vsi->rx_queues; i < nrxqsets; i++, rxq++) {
		rxq->me = i;
		rxq->vsi = vsi;

		/* store the queue size for easier access */
		rxq->desc_count = mif->subscctx->isc_nrxd[0];

		/* get the virtual and physical address of the hardware queues */
		rxq->tail = QRX_TAIL(vsi->rx_qmap[i]);
		rxq->rx_base = (union ice_32b_rx_flex_desc *)vaddrs[i];
		rxq->rx_paddr = paddrs[i];

		ice_add_rxq_sysctls(rxq);
	}

	return (0);
}

/**
 * ice_subif_if_msix_intr_assign - Assign MSI-X interrupts to new sub interface
 * @ctx: the iflib context structure
 * @msix: the number of vectors we were assigned
 *
 * Allocates and assigns driver private resources for MSI-X interrupt tracking.
 *
 * @pre OS MSI-X resources have been pre-allocated by parent interface.
 */
static int
ice_subif_if_msix_intr_assign(if_ctx_t ctx, int msix)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;

	device_t dev = mif->subdev;
	int ret;

	if (vsi->num_rx_queues != vsi->num_tx_queues) {
		device_printf(dev,
			      "iflib requested %d Tx queues, and %d Rx queues, but the driver isn't able to support a differing number of Tx and Rx queues\n",
			      vsi->num_tx_queues, vsi->num_rx_queues);
		return (EOPNOTSUPP);
	}

	if (msix > sc->extra_vectors) {
		device_printf(dev,
		     "%s: Not enough spare (%d) msix vectors for new sub-interface requested (%d)\n",
		     __func__, sc->extra_vectors, msix);
		return (ENOSPC);
	}
	device_printf(dev, "%s: Using %d vectors for sub-interface\n", __func__,
	    msix);

	/* Allocate space to store the IRQ vector data */
	mif->num_irq_vectors = vsi->num_rx_queues;
	mif->rx_irqvs = (struct ice_irq_vector *)
	    malloc(sizeof(struct ice_irq_vector) * (mif->num_irq_vectors),
		   M_ICE, M_NOWAIT);
	if (!mif->rx_irqvs) {
		device_printf(dev,
			      "Unable to allocate RX irqv memory for mirror's %d vectors\n",
			      mif->num_irq_vectors);
		return (ENOMEM);
	}

	/* Assign mirror interface interrupts from PF device space */
	if (!(mif->if_imap =
	      (u16 *)malloc(sizeof(u16) * mif->num_irq_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate mirror intfc if_imap memory\n");
		ret = ENOMEM;
		goto free_irqvs;
	}
	ret = ice_resmgr_assign_contiguous(&sc->dev_imgr, mif->if_imap, mif->num_irq_vectors);
	if (ret) {
		device_printf(dev, "Unable to assign mirror intfc PF device interrupt mapping: %s\n",
			      ice_err_str(ret));
		goto free_if_imap;
	}
	/* Assign mirror interface interrupts from OS interrupt allocation space */
	if (!(mif->os_imap =
	      (u16 *)malloc(sizeof(u16) * mif->num_irq_vectors,
	      M_ICE, M_NOWAIT))) {
		device_printf(dev, "Unable to allocate mirror intfc os_imap memory\n");
		ret = ENOMEM;
		goto free_if_imap;
	}
	ret = ice_resmgr_assign_contiguous(&sc->os_imgr, mif->os_imap, mif->num_irq_vectors);
	if (ret) {
		device_printf(dev, "Unable to assign mirror intfc OS interrupt mapping: %s\n",
			      ice_err_str(ret));
		goto free_if_imap;
	}

	return (0);

free_if_imap:
	free(mif->if_imap, M_ICE);
	mif->if_imap = NULL;
free_irqvs:
	free(mif->rx_irqvs, M_ICE);
	mif->rx_irqvs = NULL;
	return (ret);
}

/**
 * ice_subif_if_intr_enable - Enable device interrupts for a subinterface
 * @ctx: iflib context structure
 *
 * Called by iflib to request enabling all interrupts that belong to a
 * subinterface.
 */
static void
ice_subif_if_intr_enable(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	struct ice_hw *hw = &sc->hw;

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	/* Enable all queue interrupts */
	for (int i = 0; i < vsi->num_rx_queues; i++)
		ice_enable_intr(hw, vsi->rx_queues[i].irqv->me);
}

/**
 * ice_subif_if_rx_queue_intr_enable - Enable a specific Rx queue interrupt
 * @ctx: iflib context structure
 * @rxqid: the Rx queue to enable
 *
 * Enable a specific Rx queue interrupt.
 *
 * This function is not protected by the iflib CTX lock.
 */
static int
ice_subif_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	struct ice_hw *hw = &sc->hw;

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	ice_enable_intr(hw, vsi->rx_queues[rxqid].irqv->me);
	return (0);
}

/**
 * ice_subif_if_tx_queue_intr_enable - Enable a specific Tx queue interrupt
 * @ctx: iflib context structure
 * @txqid: the Tx queue to enable
 *
 * Enable a specific Tx queue interrupt.
 *
 * This function is not protected by the iflib CTX lock.
 */
static int
ice_subif_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	struct ice_hw *hw = &sc->hw;

	/* Do not enable queue interrupts in recovery mode */
	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return (ENOSYS);

	ice_enable_intr(hw, vsi->tx_queues[txqid].irqv->me);
	return (0);
}

/**
 * ice_subif_if_init - Initialize the subinterface
 * @ctx: iflib ctx structure
 *
 * Called by iflib to bring the device up, i.e. ifconfig ice0m0 up.
 * Prepares the Tx and Rx engines and enables interrupts.
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_subif_if_init(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	device_t dev = mif->subdev;
	int err;

	if (ice_driver_is_detaching(sc))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RECOVERY_MODE))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RESET_FAILED)) {
		device_printf(dev,
		    "request to start interface cannot be completed as the parent device %s failed to reset\n",
		    device_get_nameunit(sc->dev));
		return;
	}

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(dev,
		    "request to start interface cannot be completed while parent device %s is prepared for impending reset\n",
		    device_get_nameunit(sc->dev));
		return;
	}

	/* XXX: Equiv to ice_update_rx_mbuf_sz */
	vsi->mbuf_sz = iflib_get_rx_mbuf_sz(ctx);

	/* Initialize software Tx tracking values */
	ice_init_tx_tracking(vsi);

	err = ice_cfg_vsi_for_tx(vsi);
	if (err) {
		device_printf(dev,
			      "Unable to configure subif VSI for Tx: %s\n",
			      ice_err_str(err));
		return;
	}

	err = ice_cfg_vsi_for_rx(vsi);
	if (err) {
		device_printf(dev,
			      "Unable to configure subif VSI for Rx: %s\n",
			      ice_err_str(err));
		goto err_cleanup_tx;
	}

	err = ice_control_all_rx_queues(vsi, true);
	if (err) {
		device_printf(dev,
			      "Unable to enable subif Rx rings for receive: %s\n",
			      ice_err_str(err));
		goto err_cleanup_tx;
	}

	ice_configure_all_rxq_interrupts(vsi);
	ice_configure_rx_itr(vsi);

	ice_set_state(&mif->state, ICE_STATE_DRIVER_INITIALIZED);
	return;

err_cleanup_tx:
	ice_vsi_disable_tx(vsi);
}

/**
 * ice_if_stop_subif - Stop the subinterface
 * @ctx: iflib context structure
 * @ifs: subinterface context structure
 *
 * Called by iflib to stop the subinterface and bring it down.
 * (e.g. ifconfig ice0m0 down)
 *
 * @pre assumes the caller holds the iflib CTX lock
 */
static void
ice_subif_if_stop(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	device_t dev = mif->subdev;

	if (!ice_testandclear_state(&mif->state, ICE_STATE_DRIVER_INITIALIZED))
		return;

	if (ice_test_state(&sc->state, ICE_STATE_RESET_FAILED)) {
		device_printf(dev,
		    "request to stop interface cannot be completed as the parent device %s failed to reset\n",
		    device_get_nameunit(sc->dev));
		return;
	}

	if (ice_test_state(&sc->state, ICE_STATE_PREPARED_FOR_RESET)) {
		device_printf(dev,
		    "request to stop interface cannot be completed while parent device %s is prepared for impending reset\n",
		    device_get_nameunit(sc->dev));
		return;
	}

	/* Dissociate the Tx and Rx queues from the interrupts */
	ice_flush_txq_interrupts(vsi);
	ice_flush_rxq_interrupts(vsi);

	/* Disable the Tx and Rx queues */
	ice_vsi_disable_tx(vsi);
	ice_control_all_rx_queues(vsi, false);
}

/**
 * ice_free_irqvs_subif - Free IRQ vector memory for subinterfaces
 * @mif: Mirror interface private structure
 *
 * Free IRQ vector memory allocated during ice_subif_if_msix_intr_assign.
 */
static void
ice_free_irqvs_subif(struct ice_mirr_if *mif)
{
	struct ice_softc *sc = mif->back;
	struct ice_vsi *vsi = mif->vsi;
	if_ctx_t ctx = sc->ctx;
	int i;

	/* If the irqvs array is NULL, then there are no vectors to free */
	if (mif->rx_irqvs == NULL)
		return;

	/* Free the IRQ vectors -- currently subinterfaces have number
	 * of vectors equal to number of RX queues
	 *
	 * XXX: ctx is parent device's ctx, not the subinterface ctx
	 */
	for (i = 0; i < vsi->num_rx_queues; i++)
		iflib_irq_free(ctx, &mif->rx_irqvs[i].irq);

	ice_resmgr_release_map(&sc->os_imgr, mif->os_imap,
	    mif->num_irq_vectors);
	ice_resmgr_release_map(&sc->dev_imgr, mif->if_imap,
	    mif->num_irq_vectors);

	sc->last_rid -= vsi->num_rx_queues;

	/* Clear the irqv pointers */
	for (i = 0; i < vsi->num_rx_queues; i++)
		vsi->rx_queues[i].irqv = NULL;

	for (i = 0; i < vsi->num_tx_queues; i++)
		vsi->tx_queues[i].irqv = NULL;

	/* Release the vector array memory */
	free(mif->rx_irqvs, M_ICE);
	mif->rx_irqvs = NULL;
}

/**
 * ice_subif_if_queues_free - Free queue memory for subinterfaces
 * @ctx: the iflib context structure
 *
 * Free queue memory allocated by ice_subif_tx_queues_alloc() and
 * ice_subif_if_rx_queues_alloc().
 */
static void
ice_subif_if_queues_free(if_ctx_t ctx)
{
	struct ice_mirr_if *mif = (struct ice_mirr_if *)iflib_get_softc(ctx);
	struct ice_vsi *vsi = mif->vsi;
	struct ice_tx_queue *txq;
	int i;

	/* Free the Tx and Rx sysctl contexts, and assign NULL to the node
	 * pointers.
	 */
	ice_vsi_del_txqs_ctx(vsi);
	ice_vsi_del_rxqs_ctx(vsi);

	/* Release MSI-X IRQ vectors */
	ice_free_irqvs_subif(mif);

	if (vsi->tx_queues != NULL) {
		/* free the tx_rsq arrays */
		for (i = 0, txq = vsi->tx_queues; i < vsi->num_tx_queues; i++, txq++) {
			if (txq->tx_rsq != NULL) {
				free(txq->tx_rsq, M_ICE);
				txq->tx_rsq = NULL;
			}
		}
		free(vsi->tx_queues, M_ICE);
		vsi->tx_queues = NULL;
	}
	if (vsi->rx_queues != NULL) {
		free(vsi->rx_queues, M_ICE);
		vsi->rx_queues = NULL;
	}
}

/**
 * ice_subif_if_media_status - Report subinterface media
 * @ctx: iflib context structure
 * @ifmr: ifmedia request structure to update
 *
 * Updates the provided ifmr with something, in order to prevent a
 * "no media types?" message from ifconfig.
 *
 * Mirror interfaces are always up.
 */
static void
ice_subif_if_media_status(if_ctx_t ctx __unused, struct ifmediareq *ifmr)
{
	ifmr->ifm_status = IFM_AVALID | IFM_ACTIVE;
	ifmr->ifm_active = IFM_ETHER | IFM_AUTO;
}

/**
 * ice_subif_if_promisc_set - Set subinterface promiscuous mode
 * @ctx: iflib context structure
 * @flags: promiscuous flags to configure
 *
 * Called by iflib to configure device promiscuous mode.
 *
 * @remark This does not need to be implemented for now.
 */
static int
ice_subif_if_promisc_set(if_ctx_t ctx __unused, int flags __unused)
{
	return (0);
}

