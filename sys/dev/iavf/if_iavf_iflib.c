/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
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
 * @file if_iavf_iflib.c
 * @brief iflib driver implementation
 *
 * Contains the main entry point for the iflib driver implementation. It
 * implements the various ifdi driver methods, and sets up the module and
 * driver values to load an iflib driver.
 */

#include "iavf_iflib.h"
#include "iavf_vc_common.h"

#include "iavf_drv_info.h"
#include "iavf_sysctls_iflib.h"

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static void	 *iavf_register(device_t dev);
static int	 iavf_if_attach_pre(if_ctx_t ctx);
static int	 iavf_if_attach_post(if_ctx_t ctx);
static int	 iavf_if_detach(if_ctx_t ctx);
static int	 iavf_if_shutdown(if_ctx_t ctx);
static int	 iavf_if_suspend(if_ctx_t ctx);
static int	 iavf_if_resume(if_ctx_t ctx);
static int	 iavf_if_msix_intr_assign(if_ctx_t ctx, int msix);
static void	 iavf_if_enable_intr(if_ctx_t ctx);
static void	 iavf_if_disable_intr(if_ctx_t ctx);
static int	 iavf_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid);
static int	 iavf_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid);
static int	 iavf_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int ntxqs, int ntxqsets);
static int	 iavf_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int nqs, int nqsets);
static void	 iavf_if_queues_free(if_ctx_t ctx);
static void	 iavf_if_update_admin_status(if_ctx_t ctx);
static void	 iavf_if_multi_set(if_ctx_t ctx);
static int	 iavf_if_mtu_set(if_ctx_t ctx, uint32_t mtu);
static void	 iavf_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr);
static int	 iavf_if_media_change(if_ctx_t ctx);
static int	 iavf_if_promisc_set(if_ctx_t ctx, int flags);
static void	 iavf_if_timer(if_ctx_t ctx, uint16_t qid);
static void	 iavf_if_vlan_register(if_ctx_t ctx, u16 vtag);
static void	 iavf_if_vlan_unregister(if_ctx_t ctx, u16 vtag);
static uint64_t	 iavf_if_get_counter(if_ctx_t ctx, ift_counter cnt);
static void	 iavf_if_init(if_ctx_t ctx);
static void	 iavf_if_stop(if_ctx_t ctx);

static int	iavf_allocate_pci_resources(struct iavf_sc *);
static void	iavf_free_pci_resources(struct iavf_sc *);
static void	iavf_setup_interface(struct iavf_sc *);
static void	iavf_add_device_sysctls(struct iavf_sc *);
static void	iavf_enable_queue_irq(struct iavf_hw *, int);
static void	iavf_disable_queue_irq(struct iavf_hw *, int);
static void	iavf_stop(struct iavf_sc *);

static int	iavf_del_mac_filter(struct iavf_sc *sc, u8 *macaddr);
static int	iavf_msix_que(void *);
static int	iavf_msix_adminq(void *);
static void	iavf_configure_itr(struct iavf_sc *sc);

static int	iavf_sysctl_queue_interrupt_table(SYSCTL_HANDLER_ARGS);
#ifdef IAVF_DEBUG
static int	iavf_sysctl_vf_reset(SYSCTL_HANDLER_ARGS);
static int	iavf_sysctl_vflr_reset(SYSCTL_HANDLER_ARGS);
#endif

static enum iavf_status iavf_process_adminq(struct iavf_sc *, u16 *);
static void	iavf_vc_task(void *arg, int pending __unused);
static int	iavf_setup_vc_tq(struct iavf_sc *sc);
static int	iavf_vc_sleep_wait(struct iavf_sc *sc, u32 op);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

/**
 * @var iavf_methods
 * @brief device methods for the iavf driver
 *
 * Device method callbacks used to interact with the driver. For iflib this
 * primarily resolves to the default iflib implementations.
 */
static device_method_t iavf_methods[] = {
	/* Device interface */
	DEVMETHOD(device_register, iavf_register),
	DEVMETHOD(device_probe, iflib_device_probe),
	DEVMETHOD(device_attach, iflib_device_attach),
	DEVMETHOD(device_detach, iflib_device_detach),
	DEVMETHOD(device_shutdown, iflib_device_shutdown),
	DEVMETHOD_END
};

static driver_t iavf_driver = {
	"iavf", iavf_methods, sizeof(struct iavf_sc),
};

DRIVER_MODULE(iavf, pci, iavf_driver, 0, 0);
MODULE_VERSION(iavf, 1);

MODULE_DEPEND(iavf, pci, 1, 1, 1);
MODULE_DEPEND(iavf, ether, 1, 1, 1);
MODULE_DEPEND(iavf, iflib, 1, 1, 1);

IFLIB_PNP_INFO(pci, iavf, iavf_vendor_info_array);

/**
 * @var M_IAVF
 * @brief main iavf driver allocation type
 *
 * malloc(9) allocation type used by the majority of memory allocations in the
 * iavf iflib driver.
 */
MALLOC_DEFINE(M_IAVF, "iavf", "iavf driver allocations");

static device_method_t iavf_if_methods[] = {
	DEVMETHOD(ifdi_attach_pre, iavf_if_attach_pre),
	DEVMETHOD(ifdi_attach_post, iavf_if_attach_post),
	DEVMETHOD(ifdi_detach, iavf_if_detach),
	DEVMETHOD(ifdi_shutdown, iavf_if_shutdown),
	DEVMETHOD(ifdi_suspend, iavf_if_suspend),
	DEVMETHOD(ifdi_resume, iavf_if_resume),
	DEVMETHOD(ifdi_init, iavf_if_init),
	DEVMETHOD(ifdi_stop, iavf_if_stop),
	DEVMETHOD(ifdi_msix_intr_assign, iavf_if_msix_intr_assign),
	DEVMETHOD(ifdi_intr_enable, iavf_if_enable_intr),
	DEVMETHOD(ifdi_intr_disable, iavf_if_disable_intr),
	DEVMETHOD(ifdi_rx_queue_intr_enable, iavf_if_rx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queue_intr_enable, iavf_if_tx_queue_intr_enable),
	DEVMETHOD(ifdi_tx_queues_alloc, iavf_if_tx_queues_alloc),
	DEVMETHOD(ifdi_rx_queues_alloc, iavf_if_rx_queues_alloc),
	DEVMETHOD(ifdi_queues_free, iavf_if_queues_free),
	DEVMETHOD(ifdi_update_admin_status, iavf_if_update_admin_status),
	DEVMETHOD(ifdi_multi_set, iavf_if_multi_set),
	DEVMETHOD(ifdi_mtu_set, iavf_if_mtu_set),
	DEVMETHOD(ifdi_media_status, iavf_if_media_status),
	DEVMETHOD(ifdi_media_change, iavf_if_media_change),
	DEVMETHOD(ifdi_promisc_set, iavf_if_promisc_set),
	DEVMETHOD(ifdi_timer, iavf_if_timer),
	DEVMETHOD(ifdi_vlan_register, iavf_if_vlan_register),
	DEVMETHOD(ifdi_vlan_unregister, iavf_if_vlan_unregister),
	DEVMETHOD(ifdi_get_counter, iavf_if_get_counter),
	DEVMETHOD_END
};

static driver_t iavf_if_driver = {
	"iavf_if", iavf_if_methods, sizeof(struct iavf_sc)
};

extern struct if_txrx iavf_txrx_hwb;
extern struct if_txrx iavf_txrx_dwb;

static struct if_shared_ctx iavf_sctx = {
	.isc_magic = IFLIB_MAGIC,
	.isc_q_align = PAGE_SIZE,
	.isc_tx_maxsize = IAVF_MAX_FRAME,
	.isc_tx_maxsegsize = IAVF_MAX_FRAME,
	.isc_tso_maxsize = IAVF_TSO_SIZE + sizeof(struct ether_vlan_header),
	.isc_tso_maxsegsize = IAVF_MAX_DMA_SEG_SIZE,
	.isc_rx_maxsize = IAVF_MAX_FRAME,
	.isc_rx_nsegments = IAVF_MAX_RX_SEGS,
	.isc_rx_maxsegsize = IAVF_MAX_FRAME,
	.isc_nfl = 1,
	.isc_ntxqs = 1,
	.isc_nrxqs = 1,

	.isc_admin_intrcnt = 1,
	.isc_vendor_info = iavf_vendor_info_array,
	.isc_driver_version = __DECONST(char *, iavf_driver_version),
	.isc_driver = &iavf_if_driver,
	.isc_flags = IFLIB_NEED_SCRATCH | IFLIB_NEED_ZERO_CSUM | IFLIB_TSO_INIT_IP | IFLIB_IS_VF,

	.isc_nrxd_min = {IAVF_MIN_RING},
	.isc_ntxd_min = {IAVF_MIN_RING},
	.isc_nrxd_max = {IAVF_MAX_RING},
	.isc_ntxd_max = {IAVF_MAX_RING},
	.isc_nrxd_default = {IAVF_DEFAULT_RING},
	.isc_ntxd_default = {IAVF_DEFAULT_RING},
};

/*** Functions ***/

/**
 * iavf_register - iflib callback to obtain the shared context pointer
 * @dev: the device being registered
 *
 * Called when the driver is first being attached to the driver. This function
 * is used by iflib to obtain a pointer to the shared context structure which
 * describes the device features.
 *
 * @returns a pointer to the iavf shared context structure.
 */
static void *
iavf_register(device_t dev __unused)
{
	return (&iavf_sctx);
}

/**
 * iavf_allocate_pci_resources - Allocate PCI resources
 * @sc: the device private softc
 *
 * Allocate PCI resources used by the iflib driver.
 *
 * @returns zero or a non-zero error code on failure
 */
static int
iavf_allocate_pci_resources(struct iavf_sc *sc)
{
	return iavf_allocate_pci_resources_common(sc);
}

/**
 * iavf_if_attach_pre - Begin attaching the device to the driver
 * @ctx: the iflib context pointer
 *
 * Called by iflib to begin the attach process. Allocates resources and
 * initializes the hardware for operation.
 *
 * @returns zero or a non-zero error code on failure.
 */
static int
iavf_if_attach_pre(if_ctx_t ctx)
{
	device_t dev;
	struct iavf_sc *sc;
	struct iavf_hw *hw;
	struct iavf_vsi *vsi;
	if_softc_ctx_t scctx;
	int error = 0;

	/* Setup pointers */
	dev = iflib_get_dev(ctx);
	sc = iavf_sc_from_ctx(ctx);

	vsi = &sc->vsi;
	vsi->back = sc;
	sc->dev = sc->osdep.dev = dev;
	hw = &sc->hw;

	vsi->dev = dev;
	vsi->hw = &sc->hw;
	vsi->num_vlans = 0;
	vsi->ctx = ctx;
	sc->media = iflib_get_media(ctx);
	vsi->ifp = iflib_get_ifp(ctx);
	vsi->shared = scctx = iflib_get_softc_ctx(ctx);

	iavf_save_tunables(sc);

	/* Setup VC mutex */
	snprintf(sc->vc_mtx_name, sizeof(sc->vc_mtx_name),
		 "%s:vc", device_get_nameunit(dev));
	mtx_init(&sc->vc_mtx, sc->vc_mtx_name, NULL, MTX_DEF);

	/* Do PCI setup - map BAR0, etc */
	error = iavf_allocate_pci_resources(sc);
	if (error) {
		device_printf(dev, "%s: Allocation of PCI resources failed\n",
		    __func__);
		goto err_early;
	}

	iavf_dbg_init(sc, "Allocated PCI resources and MSI-X vectors\n");

	error = iavf_set_mac_type(hw);
	if (error) {
		device_printf(dev, "%s: set_mac_type failed: %d\n",
		    __func__, error);
		goto err_pci_res;
	}

	error = iavf_reset_complete(hw);
	if (error) {
		device_printf(dev, "%s: Device is still being reset\n",
		    __func__);
		goto err_pci_res;
	}

	iavf_dbg_init(sc, "VF Device is ready for configuration\n");

	/* Sets up Admin Queue */
	error = iavf_setup_vc(sc);
	if (error) {
		device_printf(dev, "%s: Error setting up PF comms, %d\n",
		    __func__, error);
		goto err_pci_res;
	}

	iavf_dbg_init(sc, "PF API version verified\n");

	/* Need API version before sending reset message */
	error = iavf_reset(sc);
	if (error) {
		device_printf(dev, "VF reset failed; reload the driver\n");
		goto err_aq;
	}

	iavf_dbg_init(sc, "VF reset complete\n");

	/* Ask for VF config from PF */
	error = iavf_vf_config(sc);
	if (error) {
		device_printf(dev, "Error getting configuration from PF: %d\n",
		    error);
		goto err_aq;
	}

	iavf_print_device_info(sc);

	error = iavf_get_vsi_res_from_vf_res(sc);
	if (error)
		goto err_res_buf;

	iavf_dbg_init(sc, "Resource Acquisition complete\n");

	/* Setup taskqueue to service VC messages */
	error = iavf_setup_vc_tq(sc);
	if (error)
		goto err_vc_tq;

	iavf_set_mac_addresses(sc);
	iflib_set_mac(ctx, hw->mac.addr);

	/* Allocate filter lists */
	iavf_init_filters(sc);

	/* Fill out more iflib parameters */
	scctx->isc_ntxqsets_max = scctx->isc_nrxqsets_max =
	    sc->vsi_res->num_queue_pairs;
	if (vsi->enable_head_writeback) {
		scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0]
		    * sizeof(struct iavf_tx_desc) + sizeof(u32), DBA_ALIGN);
		scctx->isc_txrx = &iavf_txrx_hwb;
	} else {
		scctx->isc_txqsizes[0] = roundup2(scctx->isc_ntxd[0]
		    * sizeof(struct iavf_tx_desc), DBA_ALIGN);
		scctx->isc_txrx = &iavf_txrx_dwb;
	}
	scctx->isc_rxqsizes[0] = roundup2(scctx->isc_nrxd[0]
	    * sizeof(union iavf_32byte_rx_desc), DBA_ALIGN);
	scctx->isc_msix_bar = PCIR_BAR(IAVF_MSIX_BAR);
	scctx->isc_tx_nsegments = IAVF_MAX_TX_SEGS;
	scctx->isc_tx_tso_segments_max = IAVF_MAX_TSO_SEGS;
	scctx->isc_tx_tso_size_max = IAVF_TSO_SIZE;
	scctx->isc_tx_tso_segsize_max = IAVF_MAX_DMA_SEG_SIZE;
	scctx->isc_rss_table_size = IAVF_RSS_VSI_LUT_SIZE;
	scctx->isc_capabilities = scctx->isc_capenable = IAVF_CAPS;
	scctx->isc_tx_csum_flags = CSUM_OFFLOAD;

	/* Update OS cache of MSIX control register values */
	iavf_update_msix_devinfo(dev);

	return (0);

err_vc_tq:
	taskqueue_free(sc->vc_tq);
err_res_buf:
	free(sc->vf_res, M_IAVF);
err_aq:
	iavf_shutdown_adminq(hw);
err_pci_res:
	iavf_free_pci_resources(sc);
err_early:
	IAVF_VC_LOCK_DESTROY(sc);
	return (error);
}

/**
 * iavf_vc_task - task used to process VC messages
 * @arg: device softc
 * @pending: unused
 *
 * Processes the admin queue, in order to process the virtual
 * channel messages received from the PF.
 */
static void
iavf_vc_task(void *arg, int pending __unused)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg;
	u16 var;

	iavf_process_adminq(sc, &var);
}

/**
 * iavf_setup_vc_tq - Setup task queues
 * @sc: device softc
 *
 * Create taskqueue and tasklet for processing virtual channel messages. This
 * is done in a separate non-iflib taskqueue so that the iflib context lock
 * does not need to be held for VC messages to be processed.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_setup_vc_tq(struct iavf_sc *sc)
{
	device_t dev = sc->dev;
	int error = 0;

	TASK_INIT(&sc->vc_task, 0, iavf_vc_task, sc);

	sc->vc_tq = taskqueue_create_fast("iavf_vc", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->vc_tq);
	if (!sc->vc_tq) {
		device_printf(dev, "taskqueue_create_fast (for VC task) returned NULL!\n");
		return (ENOMEM);
	}
	error = taskqueue_start_threads(&sc->vc_tq, 1, PI_NET, "%s vc",
	    device_get_nameunit(dev));
	if (error) {
		device_printf(dev, "taskqueue_start_threads (for VC task) error: %d\n",
		    error);
		taskqueue_free(sc->vc_tq);
		return (error);
	}

	return (error);
}

/**
 * iavf_if_attach_post - Finish attaching the device to the driver
 * @ctx: the iflib context pointer
 *
 * Called by iflib after it has setup queues and interrupts. Used to finish up
 * the attach process for a device. Attach logic which must occur after Tx and
 * Rx queues are setup belongs here.
 *
 * @returns zero or a non-zero error code on failure
 */
static int
iavf_if_attach_post(if_ctx_t ctx)
{
#ifdef IXL_DEBUG
	device_t dev = iflib_get_dev(ctx);
#endif
	struct iavf_sc	*sc;
	struct iavf_hw	*hw;
	struct iavf_vsi *vsi;
	int error = 0;

	INIT_DBG_DEV(dev, "begin");

	sc = iavf_sc_from_ctx(ctx);
	vsi = &sc->vsi;
	hw = &sc->hw;

	/* Save off determined number of queues for interface */
	vsi->num_rx_queues = vsi->shared->isc_nrxqsets;
	vsi->num_tx_queues = vsi->shared->isc_ntxqsets;

	/* Setup the stack interface */
	iavf_setup_interface(sc);

	iavf_dbg_init(sc, "Interface setup complete\n");

	/* Initialize statistics & add sysctls */
	bzero(&sc->vsi.eth_stats, sizeof(struct iavf_eth_stats));
	iavf_add_device_sysctls(sc);

	atomic_store_rel_32(&sc->queues_enabled, 0);
	iavf_set_state(&sc->state, IAVF_STATE_INITIALIZED);

	/* We want AQ enabled early for init */
	iavf_enable_adminq_irq(hw);

	INIT_DBG_DEV(dev, "end");

	return (error);
}

/**
 * iavf_if_detach - Detach a device from the driver
 * @ctx: the iflib context of the device to detach
 *
 * Called by iflib to detach a given device from the driver. Clean up any
 * resources associated with the driver and shut the device down.
 *
 * @remark iflib always ignores the return value of IFDI_DETACH, so this
 * function is effectively not allowed to fail. Instead, it should clean up
 * and release as much as possible even if something goes wrong.
 *
 * @returns zero
 */
static int
iavf_if_detach(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	enum iavf_status status;

	INIT_DBG_DEV(dev, "begin");

	iavf_clear_state(&sc->state, IAVF_STATE_INITIALIZED);

	/* Drain admin queue taskqueue */
	taskqueue_free(sc->vc_tq);
	IAVF_VC_LOCK_DESTROY(sc);

	/* Remove all the media and link information */
	ifmedia_removeall(sc->media);

	iavf_disable_adminq_irq(hw);
	status = iavf_shutdown_adminq(&sc->hw);
	if (status != IAVF_SUCCESS) {
		device_printf(dev,
		    "iavf_shutdown_adminq() failed with status %s\n",
		    iavf_stat_str(hw, status));
	}

	free(sc->vf_res, M_IAVF);
	sc->vf_res = NULL;
	iavf_free_pci_resources(sc);
	iavf_free_filters(sc);

	INIT_DBG_DEV(dev, "end");
	return (0);
}

/**
 * iavf_if_shutdown - called by iflib to handle shutdown
 * @ctx: the iflib context pointer
 *
 * Callback for the IFDI_SHUTDOWN iflib function.
 *
 * @returns zero or an error code on failure
 */
static int
iavf_if_shutdown(if_ctx_t ctx __unused)
{
	return (0);
}

/**
 * iavf_if_suspend - called by iflib to handle suspend
 * @ctx: the iflib context pointer
 *
 * Callback for the IFDI_SUSPEND iflib function.
 *
 * @returns zero or an error code on failure
 */
static int
iavf_if_suspend(if_ctx_t ctx __unused)
{
	return (0);
}

/**
 * iavf_if_resume - called by iflib to handle resume
 * @ctx: the iflib context pointer
 *
 * Callback for the IFDI_RESUME iflib function.
 *
 * @returns zero or an error code on failure
 */
static int
iavf_if_resume(if_ctx_t ctx __unused)
{
	return (0);
}

/**
 * iavf_vc_sleep_wait - Sleep for a response from a VC message
 * @sc: device softc
 * @op: the op code to sleep on
 *
 * Sleep until a response from the PF for the VC message sent by the
 * given op.
 *
 * @returns zero on success, or EWOULDBLOCK if the sleep times out.
 */
static int
iavf_vc_sleep_wait(struct iavf_sc *sc, u32 op)
{
	int error = 0;

	IAVF_VC_LOCK_ASSERT(sc);

	iavf_dbg_vc(sc, "Sleeping for op %b\n", op, IAVF_FLAGS);

	error = mtx_sleep(iavf_vc_get_op_chan(sc, op),
	    &sc->vc_mtx, PRI_MAX, "iavf_vc", IAVF_AQ_TIMEOUT);

	return (error);
}

/**
 * iavf_send_vc_msg_sleep - Send a virtchnl message and wait for a response
 * @sc: device softc
 * @op: the op code to send
 *
 * Send a virtchnl message to the PF, and sleep or busy wait for a response
 * from the PF, depending on iflib context lock type.
 *
 * @remark this function does not wait if the device is detaching, on kernels
 * that support indicating to the driver that the device is detaching
 *
 * @returns zero or an error code on failure.
 */
int
iavf_send_vc_msg_sleep(struct iavf_sc *sc, u32 op)
{
	if_ctx_t ctx = sc->vsi.ctx;
	int error = 0;

	IAVF_VC_LOCK(sc);
	error = iavf_vc_send_cmd(sc, op);
	if (error != 0) {
		iavf_dbg_vc(sc, "Error sending %b: %d\n", op, IAVF_FLAGS, error);
		goto release_lock;
	}

	/* Don't wait for a response if the device is being detached. */
	if (!iflib_in_detach(ctx)) {
		error = iavf_vc_sleep_wait(sc, op);
		IAVF_VC_LOCK_ASSERT(sc);

		if (error == EWOULDBLOCK)
			device_printf(sc->dev, "%b timed out\n", op, IAVF_FLAGS);
	}
release_lock:
	IAVF_VC_UNLOCK(sc);
	return (error);
}

/**
 * iavf_send_vc_msg - Send a virtchnl message to the PF
 * @sc: device softc
 * @op: the op code to send
 *
 * Send a virtchnl message to the PF and do not wait for a response.
 *
 * @returns zero on success, or an error code on failure.
 */
int
iavf_send_vc_msg(struct iavf_sc *sc, u32 op)
{
	int error = 0;

	error = iavf_vc_send_cmd(sc, op);
	if (error != 0)
		iavf_dbg_vc(sc, "Error sending %b: %d\n", op, IAVF_FLAGS, error);

	return (error);
}

/**
 * iavf_init_queues - initialize Tx and Rx queues
 * @vsi: the VSI to initialize
 *
 * Refresh the Tx and Rx ring contents and update the tail pointers for each
 * queue.
 */
static void
iavf_init_queues(struct iavf_vsi *vsi)
{
	struct iavf_tx_queue *tx_que = vsi->tx_queues;
	struct iavf_rx_queue *rx_que = vsi->rx_queues;
	struct rx_ring *rxr;
	uint32_t mbuf_sz;

	mbuf_sz = iflib_get_rx_mbuf_sz(vsi->ctx);
	MPASS(mbuf_sz <= UINT16_MAX);

	for (int i = 0; i < vsi->num_tx_queues; i++, tx_que++)
		iavf_init_tx_ring(vsi, tx_que);

	for (int i = 0; i < vsi->num_rx_queues; i++, rx_que++) {
		rxr = &rx_que->rxr;

		rxr->mbuf_sz = mbuf_sz;
		wr32(vsi->hw, rxr->tail, 0);
	}
}

/**
 * iavf_if_init - Initialize device for operation
 * @ctx: the iflib context pointer
 *
 * Initializes a device for operation. Called by iflib in response to an
 * interface up event from the stack.
 *
 * @remark this function does not return a value and thus cannot indicate
 * failure to initialize.
 */
static void
iavf_if_init(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	struct iavf_hw *hw = &sc->hw;
	if_t ifp = iflib_get_ifp(ctx);
	u8 tmpaddr[ETHER_ADDR_LEN];
	enum iavf_status status;
	device_t dev = sc->dev;
	int error = 0;

	INIT_DBG_IF(ifp, "begin");

	IFLIB_CTX_ASSERT(ctx);

	error = iavf_reset_complete(hw);
	if (error) {
		device_printf(sc->dev, "%s: VF reset failed\n",
		    __func__);
	}

	if (!iavf_check_asq_alive(hw)) {
		iavf_dbg_info(sc, "ASQ is not alive, re-initializing AQ\n");
		pci_enable_busmaster(dev);

		status = iavf_shutdown_adminq(hw);
		if (status != IAVF_SUCCESS) {
			device_printf(dev,
			    "%s: iavf_shutdown_adminq failed: %s\n",
			    __func__, iavf_stat_str(hw, status));
			return;
		}

		status = iavf_init_adminq(hw);
		if (status != IAVF_SUCCESS) {
			device_printf(dev,
			"%s: iavf_init_adminq failed: %s\n",
			    __func__, iavf_stat_str(hw, status));
			return;
		}
	}

	/* Make sure queues are disabled */
	iavf_disable_queues_with_retries(sc);

	bcopy(if_getlladdr(ifp), tmpaddr, ETHER_ADDR_LEN);
	if (!cmp_etheraddr(hw->mac.addr, tmpaddr) &&
	    (iavf_validate_mac_addr(tmpaddr) == IAVF_SUCCESS)) {
		error = iavf_del_mac_filter(sc, hw->mac.addr);
		if (error == 0)
			iavf_send_vc_msg(sc, IAVF_FLAG_AQ_DEL_MAC_FILTER);

		bcopy(tmpaddr, hw->mac.addr, ETH_ALEN);
	}

	error = iavf_add_mac_filter(sc, hw->mac.addr, 0);
	if (!error || error == EEXIST)
		iavf_send_vc_msg(sc, IAVF_FLAG_AQ_ADD_MAC_FILTER);
	iflib_set_mac(ctx, hw->mac.addr);

	/* Prepare the queues for operation */
	iavf_init_queues(vsi);

	/* Set initial ITR values */
	iavf_configure_itr(sc);

	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_CONFIGURE_QUEUES);

	/* Set up RSS */
	iavf_config_rss(sc);

	/* Map vectors */
	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_MAP_VECTORS);

	/* Init SW TX ring indices */
	if (vsi->enable_head_writeback)
		iavf_init_tx_cidx(vsi);
	else
		iavf_init_tx_rsqs(vsi);

	/* Configure promiscuous mode */
	iavf_config_promisc(sc, if_getflags(ifp));

	/* Enable queues */
	iavf_send_vc_msg_sleep(sc, IAVF_FLAG_AQ_ENABLE_QUEUES);

	iavf_set_state(&sc->state, IAVF_STATE_RUNNING);
}

/**
 * iavf_if_msix_intr_assign - Assign MSI-X interrupts
 * @ctx: the iflib context pointer
 * @msix: the number of MSI-X vectors available
 *
 * Called by iflib to assign MSI-X interrupt vectors to queues. Assigns and
 * sets up vectors for each Tx and Rx queue, as well as the administrative
 * control interrupt.
 *
 * @returns zero or an error code on failure
 */
static int
iavf_if_msix_intr_assign(if_ctx_t ctx, int msix __unused)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	struct iavf_rx_queue *rx_que = vsi->rx_queues;
	struct iavf_tx_queue *tx_que = vsi->tx_queues;
	int err, i, rid, vector = 0;
	char buf[16];

	MPASS(vsi->shared->isc_nrxqsets > 0);
	MPASS(vsi->shared->isc_ntxqsets > 0);

	/* Admin Que is vector 0*/
	rid = vector + 1;
	err = iflib_irq_alloc_generic(ctx, &vsi->irq, rid, IFLIB_INTR_ADMIN,
	    iavf_msix_adminq, sc, 0, "aq");
	if (err) {
		iflib_irq_free(ctx, &vsi->irq);
		device_printf(iflib_get_dev(ctx),
		    "Failed to register Admin Que handler");
		return (err);
	}

	/* Now set up the stations */
	for (i = 0, vector = 1; i < vsi->shared->isc_nrxqsets; i++, vector++, rx_que++) {
		rid = vector + 1;

		snprintf(buf, sizeof(buf), "rxq%d", i);
		err = iflib_irq_alloc_generic(ctx, &rx_que->que_irq, rid,
		    IFLIB_INTR_RXTX, iavf_msix_que, rx_que, rx_que->rxr.me, buf);
		if (err) {
			device_printf(iflib_get_dev(ctx),
			    "Failed to allocate queue RX int vector %d, err: %d\n", i, err);
			vsi->num_rx_queues = i + 1;
			goto fail;
		}
		rx_que->msix = vector;
	}

	bzero(buf, sizeof(buf));

	for (i = 0; i < vsi->shared->isc_ntxqsets; i++, tx_que++) {
		snprintf(buf, sizeof(buf), "txq%d", i);
		iflib_softirq_alloc_generic(ctx,
		    &vsi->rx_queues[i % vsi->shared->isc_nrxqsets].que_irq,
		    IFLIB_INTR_TX, tx_que, tx_que->txr.me, buf);

		tx_que->msix = (i % vsi->shared->isc_nrxqsets) + 1;
	}

	return (0);
fail:
	iflib_irq_free(ctx, &vsi->irq);
	rx_que = vsi->rx_queues;
	for (int i = 0; i < vsi->num_rx_queues; i++, rx_que++)
		iflib_irq_free(ctx, &rx_que->que_irq);
	return (err);
}

/**
 * iavf_if_enable_intr - Enable all interrupts for a device
 * @ctx: the iflib context pointer
 *
 * Called by iflib to request enabling all interrupts.
 */
static void
iavf_if_enable_intr(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;

	iavf_enable_intr(vsi);
}

/**
 * iavf_if_disable_intr - Disable all interrupts for a device
 * @ctx: the iflib context pointer
 *
 * Called by iflib to request disabling all interrupts.
 */
static void
iavf_if_disable_intr(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;

	iavf_disable_intr(vsi);
}

/**
 * iavf_if_rx_queue_intr_enable - Enable one Rx queue interrupt
 * @ctx: the iflib context pointer
 * @rxqid: Rx queue index
 *
 * Enables the interrupt associated with a specified Rx queue.
 *
 * @returns zero
 */
static int
iavf_if_rx_queue_intr_enable(if_ctx_t ctx, uint16_t rxqid)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	struct iavf_hw *hw = vsi->hw;
	struct iavf_rx_queue *rx_que = &vsi->rx_queues[rxqid];

	iavf_enable_queue_irq(hw, rx_que->msix - 1);
	return (0);
}

/**
 * iavf_if_tx_queue_intr_enable - Enable one Tx queue interrupt
 * @ctx: the iflib context pointer
 * @txqid: Tx queue index
 *
 * Enables the interrupt associated with a specified Tx queue.
 *
 * @returns zero
 */
static int
iavf_if_tx_queue_intr_enable(if_ctx_t ctx, uint16_t txqid)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	struct iavf_hw *hw = vsi->hw;
	struct iavf_tx_queue *tx_que = &vsi->tx_queues[txqid];

	iavf_enable_queue_irq(hw, tx_que->msix - 1);
	return (0);
}

/**
 * iavf_if_tx_queues_alloc - Allocate Tx queue memory
 * @ctx: the iflib context pointer
 * @vaddrs: Array of virtual addresses
 * @paddrs: Array of physical addresses
 * @ntxqs: the number of Tx queues per group (should always be 1)
 * @ntxqsets: the number of Tx queues
 *
 * Allocates memory for the specified number of Tx queues. This includes
 * memory for the queue structures and the report status array for the queues.
 * The virtual and physical addresses are saved for later use during
 * initialization.
 *
 * @returns zero or a non-zero error code on failure
 */
static int
iavf_if_tx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int ntxqs, int ntxqsets)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	if_softc_ctx_t scctx = vsi->shared;
	struct iavf_tx_queue *que;
	int i, j, error = 0;

	MPASS(scctx->isc_ntxqsets > 0);
	MPASS(ntxqs == 1);
	MPASS(scctx->isc_ntxqsets == ntxqsets);

	/* Allocate queue structure memory */
	if (!(vsi->tx_queues =
	    (struct iavf_tx_queue *)malloc(sizeof(struct iavf_tx_queue) *ntxqsets, M_IAVF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate TX ring memory\n");
		return (ENOMEM);
	}

	for (i = 0, que = vsi->tx_queues; i < ntxqsets; i++, que++) {
		struct tx_ring *txr = &que->txr;

		txr->me = i;
		que->vsi = vsi;

		if (!vsi->enable_head_writeback) {
			/* Allocate report status array */
			if (!(txr->tx_rsq = (qidx_t *)malloc(sizeof(qidx_t) * scctx->isc_ntxd[0], M_IAVF, M_NOWAIT))) {
				device_printf(iflib_get_dev(ctx), "failed to allocate tx_rsq memory\n");
				error = ENOMEM;
				goto fail;
			}
			/* Init report status array */
			for (j = 0; j < scctx->isc_ntxd[0]; j++)
				txr->tx_rsq[j] = QIDX_INVALID;
		}
		/* get the virtual and physical address of the hardware queues */
		txr->tail = IAVF_QTX_TAIL1(txr->me);
		txr->tx_base = (struct iavf_tx_desc *)vaddrs[i * ntxqs];
		txr->tx_paddr = paddrs[i * ntxqs];
		txr->que = que;
	}

	return (0);
fail:
	iavf_if_queues_free(ctx);
	return (error);
}

/**
 * iavf_if_rx_queues_alloc - Allocate Rx queue memory
 * @ctx: the iflib context pointer
 * @vaddrs: Array of virtual addresses
 * @paddrs: Array of physical addresses
 * @nrxqs: number of Rx queues per group (should always be 1)
 * @nrxqsets: the number of Rx queues to allocate
 *
 * Called by iflib to allocate driver memory for a number of Rx queues.
 * Allocates memory for the drivers private Rx queue data structure, and saves
 * the physical and virtual addresses for later use.
 *
 * @returns zero or a non-zero error code on failure
 */
static int
iavf_if_rx_queues_alloc(if_ctx_t ctx, caddr_t *vaddrs, uint64_t *paddrs, int nrxqs, int nrxqsets)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	struct iavf_rx_queue *que;
	int i, error = 0;

#ifdef INVARIANTS
	if_softc_ctx_t scctx = vsi->shared;
	MPASS(scctx->isc_nrxqsets > 0);
	MPASS(nrxqs == 1);
	MPASS(scctx->isc_nrxqsets == nrxqsets);
#endif

	/* Allocate queue structure memory */
	if (!(vsi->rx_queues =
	    (struct iavf_rx_queue *) malloc(sizeof(struct iavf_rx_queue) *
	    nrxqsets, M_IAVF, M_NOWAIT | M_ZERO))) {
		device_printf(iflib_get_dev(ctx), "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto fail;
	}

	for (i = 0, que = vsi->rx_queues; i < nrxqsets; i++, que++) {
		struct rx_ring *rxr = &que->rxr;

		rxr->me = i;
		que->vsi = vsi;

		/* get the virtual and physical address of the hardware queues */
		rxr->tail = IAVF_QRX_TAIL1(rxr->me);
		rxr->rx_base = (union iavf_rx_desc *)vaddrs[i * nrxqs];
		rxr->rx_paddr = paddrs[i * nrxqs];
		rxr->que = que;
	}

	return (0);
fail:
	iavf_if_queues_free(ctx);
	return (error);
}

/**
 * iavf_if_queues_free - Free driver queue memory
 * @ctx: the iflib context pointer
 *
 * Called by iflib to release memory allocated by the driver when setting up
 * Tx and Rx queues.
 *
 * @remark The ordering of this function and iavf_if_detach is not guaranteed.
 * It is possible for this function to be called either before or after the
 * iavf_if_detach. Thus, care must be taken to ensure that either ordering of
 * iavf_if_detach and iavf_if_queues_free is safe.
 */
static void
iavf_if_queues_free(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;

	if (!vsi->enable_head_writeback) {
		struct iavf_tx_queue *que;
		int i = 0;

		for (i = 0, que = vsi->tx_queues; i < vsi->shared->isc_ntxqsets; i++, que++) {
			struct tx_ring *txr = &que->txr;
			if (txr->tx_rsq != NULL) {
				free(txr->tx_rsq, M_IAVF);
				txr->tx_rsq = NULL;
			}
		}
	}

	if (vsi->tx_queues != NULL) {
		free(vsi->tx_queues, M_IAVF);
		vsi->tx_queues = NULL;
	}
	if (vsi->rx_queues != NULL) {
		free(vsi->rx_queues, M_IAVF);
		vsi->rx_queues = NULL;
	}
}

/**
 * iavf_check_aq_errors - Check for AdminQ errors
 * @sc: device softc
 *
 * Check the AdminQ registers for errors, and determine whether or not a reset
 * may be required to resolve them.
 *
 * @post if there are errors, the VF device will be stopped and a reset will
 * be requested.
 *
 * @returns zero if there are no issues, EBUSY if the device is resetting,
 * or EIO if there are any AQ errors.
 */
static int
iavf_check_aq_errors(struct iavf_sc *sc)
{
	struct iavf_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	u32 reg, oldreg;
	u8 aq_error = false;

	oldreg = reg = rd32(hw, hw->aq.arq.len);

	/* Check if device is in reset */
	if (reg == 0xdeadbeef || reg == 0xffffffff) {
		device_printf(dev, "VF in reset\n");
		return (EBUSY);
	}

	/* Check for Admin queue errors */
	if (reg & IAVF_VF_ARQLEN1_ARQVFE_MASK) {
		device_printf(dev, "ARQ VF Error detected\n");
		reg &= ~IAVF_VF_ARQLEN1_ARQVFE_MASK;
		aq_error = true;
	}
	if (reg & IAVF_VF_ARQLEN1_ARQOVFL_MASK) {
		device_printf(dev, "ARQ Overflow Error detected\n");
		reg &= ~IAVF_VF_ARQLEN1_ARQOVFL_MASK;
		aq_error = true;
	}
	if (reg & IAVF_VF_ARQLEN1_ARQCRIT_MASK) {
		device_printf(dev, "ARQ Critical Error detected\n");
		reg &= ~IAVF_VF_ARQLEN1_ARQCRIT_MASK;
		aq_error = true;
	}
	if (oldreg != reg)
		wr32(hw, hw->aq.arq.len, reg);

	oldreg = reg = rd32(hw, hw->aq.asq.len);
	if (reg & IAVF_VF_ATQLEN1_ATQVFE_MASK) {
		device_printf(dev, "ASQ VF Error detected\n");
		reg &= ~IAVF_VF_ATQLEN1_ATQVFE_MASK;
		aq_error = true;
	}
	if (reg & IAVF_VF_ATQLEN1_ATQOVFL_MASK) {
		device_printf(dev, "ASQ Overflow Error detected\n");
		reg &= ~IAVF_VF_ATQLEN1_ATQOVFL_MASK;
		aq_error = true;
	}
	if (reg & IAVF_VF_ATQLEN1_ATQCRIT_MASK) {
		device_printf(dev, "ASQ Critical Error detected\n");
		reg &= ~IAVF_VF_ATQLEN1_ATQCRIT_MASK;
		aq_error = true;
	}
	if (oldreg != reg)
		wr32(hw, hw->aq.asq.len, reg);

	return (aq_error ? EIO : 0);
}

/**
 * iavf_process_adminq - Process adminq responses from the PF
 * @sc: device softc
 * @pending: output parameter indicating how many messages remain
 *
 * Process the adminq to handle replies from the PF over the virtchnl
 * connection.
 *
 * @returns zero or an iavf_status code on failure
 */
static enum iavf_status
iavf_process_adminq(struct iavf_sc *sc, u16 *pending)
{
	enum iavf_status status = IAVF_SUCCESS;
	struct iavf_arq_event_info event;
	struct iavf_hw *hw = &sc->hw;
	struct virtchnl_msg *v_msg;
	int error = 0, loop = 0;
	u32 reg;

	if (iavf_test_state(&sc->state, IAVF_STATE_RESET_PENDING)) {
		status = IAVF_ERR_ADMIN_QUEUE_ERROR;
		goto reenable_interrupt;
	}

	error = iavf_check_aq_errors(sc);
	if (error) {
		status = IAVF_ERR_ADMIN_QUEUE_CRITICAL_ERROR;
		goto reenable_interrupt;
	}

	event.buf_len = IAVF_AQ_BUF_SZ;
        event.msg_buf = sc->aq_buffer;
	bzero(event.msg_buf, IAVF_AQ_BUF_SZ);
	v_msg = (struct virtchnl_msg *)&event.desc;

	IAVF_VC_LOCK(sc);
	/* clean and process any events */
	do {
		status = iavf_clean_arq_element(hw, &event, pending);
		/*
		 * Also covers normal case when iavf_clean_arq_element()
		 * returns "IAVF_ERR_ADMIN_QUEUE_NO_WORK"
		 */
		if (status)
			break;
		iavf_vc_completion(sc, v_msg->v_opcode,
		    v_msg->v_retval, event.msg_buf, event.msg_len);
		bzero(event.msg_buf, IAVF_AQ_BUF_SZ);
	} while (*pending && (loop++ < IAVF_ADM_LIMIT));
	IAVF_VC_UNLOCK(sc);

reenable_interrupt:
	/* Re-enable admin queue interrupt cause */
	reg = rd32(hw, IAVF_VFINT_ICR0_ENA1);
	reg |= IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK;
	wr32(hw, IAVF_VFINT_ICR0_ENA1, reg);

	return (status);
}

/**
 * iavf_if_update_admin_status - Administrative status task
 * @ctx: iflib context
 *
 * Called by iflib to handle administrative status events. The iavf driver
 * uses this to process the adminq virtchnl messages outside of interrupt
 * context.
 */
static void
iavf_if_update_admin_status(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_hw *hw = &sc->hw;
	u16 pending = 0;

	iavf_process_adminq(sc, &pending);
	iavf_update_link_status(sc);

	/*
	 * If there are still messages to process, reschedule.
	 * Otherwise, re-enable the Admin Queue interrupt.
	 */
	if (pending > 0)
		iflib_admin_intr_deferred(ctx);
	else
		iavf_enable_adminq_irq(hw);
}

/**
 * iavf_if_multi_set - Set multicast address filters
 * @ctx: iflib context
 *
 * Called by iflib to update the current list of multicast filters for the
 * device.
 */
static void
iavf_if_multi_set(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);

	iavf_multi_set(sc);
}

/**
 * iavf_if_mtu_set - Set the device MTU
 * @ctx: iflib context
 * @mtu: MTU value to set
 *
 * Called by iflib to set the device MTU.
 *
 * @returns zero on success, or EINVAL if the MTU is invalid.
 */
static int
iavf_if_mtu_set(if_ctx_t ctx, uint32_t mtu)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;

	IOCTL_DEBUGOUT("ioctl: SiOCSIFMTU (Set Interface MTU)");
	if (mtu < IAVF_MIN_MTU || mtu > IAVF_MAX_MTU) {
		device_printf(sc->dev, "mtu %d is not in valid range [%d-%d]\n",
		    mtu, IAVF_MIN_MTU, IAVF_MAX_MTU);
		return (EINVAL);
	}

	vsi->shared->isc_max_frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN +
		ETHER_VLAN_ENCAP_LEN;

	return (0);
}

/**
 * iavf_if_media_status - Report current media status
 * @ctx: iflib context
 * @ifmr: ifmedia request structure
 *
 * Called by iflib to report the current media status in the ifmr.
 */
static void
iavf_if_media_status(if_ctx_t ctx, struct ifmediareq *ifmr)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);

	iavf_media_status_common(sc, ifmr);
}

/**
 * iavf_if_media_change - Change the current media settings
 * @ctx: iflib context
 *
 * Called by iflib to change the current media settings.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_if_media_change(if_ctx_t ctx)
{
	return iavf_media_change_common(iflib_get_ifp(ctx));
}

/**
 * iavf_if_promisc_set - Set device promiscuous mode
 * @ctx: iflib context
 * @flags: promiscuous configuration
 *
 * Called by iflib to request that the device enter promiscuous mode.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_if_promisc_set(if_ctx_t ctx, int flags)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);

	return iavf_config_promisc(sc, flags);
}

/**
 * iavf_if_timer - Periodic timer called by iflib
 * @ctx: iflib context
 * @qid: The queue being triggered
 *
 * Called by iflib periodically as a timer task, so that the driver can handle
 * periodic work.
 *
 * @remark this timer is only called while the interface is up, even if
 * IFLIB_ADMIN_ALWAYS_RUN is set.
 */
static void
iavf_if_timer(if_ctx_t ctx, uint16_t qid)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_hw *hw = &sc->hw;
	u32 val;

	if (qid != 0)
		return;

	/* Check for when PF triggers a VF reset */
	val = rd32(hw, IAVF_VFGEN_RSTAT) &
	    IAVF_VFGEN_RSTAT_VFR_STATE_MASK;
	if (val != VIRTCHNL_VFR_VFACTIVE
	    && val != VIRTCHNL_VFR_COMPLETED) {
		iavf_dbg_info(sc, "reset in progress! (%d)\n", val);
		return;
	}

	/* Fire off the adminq task */
	iflib_admin_intr_deferred(ctx);

	/* Update stats */
	iavf_request_stats(sc);
}

/**
 * iavf_if_vlan_register - Register a VLAN
 * @ctx: iflib context
 * @vtag: the VLAN to register
 *
 * Register a VLAN filter for a given vtag.
 */
static void
iavf_if_vlan_register(if_ctx_t ctx, u16 vtag)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	/* Add VLAN 0 to list, for untagged traffic */
	if (vsi->num_vlans == 0)
		iavf_add_vlan_filter(sc, 0);

	iavf_add_vlan_filter(sc, vtag);

	++vsi->num_vlans;

	iavf_send_vc_msg(sc, IAVF_FLAG_AQ_ADD_VLAN_FILTER);
}

/**
 * iavf_if_vlan_unregister - Unregister a VLAN
 * @ctx: iflib context
 * @vtag: the VLAN to remove
 *
 * Unregister (remove) a VLAN filter for the given vtag.
 */
static void
iavf_if_vlan_unregister(if_ctx_t ctx, u16 vtag)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	int i = 0;

	if ((vtag == 0) || (vtag > 4095) || (vsi->num_vlans == 0))	/* Invalid */
		return;

	i = iavf_mark_del_vlan_filter(sc, vtag);
	vsi->num_vlans -= i;

	/* Remove VLAN filter 0 if the last VLAN is being removed */
	if (vsi->num_vlans == 0)
		i += iavf_mark_del_vlan_filter(sc, 0);

	if (i > 0)
		iavf_send_vc_msg(sc, IAVF_FLAG_AQ_DEL_VLAN_FILTER);
}

/**
 * iavf_if_get_counter - Get network statistic counters
 * @ctx: iflib context
 * @cnt: The counter to obtain
 *
 * Called by iflib to obtain the value of the specified counter.
 *
 * @returns the uint64_t counter value.
 */
static uint64_t
iavf_if_get_counter(if_ctx_t ctx, ift_counter cnt)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);
	struct iavf_vsi *vsi = &sc->vsi;
	if_t ifp = iflib_get_ifp(ctx);

	switch (cnt) {
	case IFCOUNTER_IPACKETS:
		return (vsi->ipackets);
	case IFCOUNTER_IERRORS:
		return (vsi->ierrors);
	case IFCOUNTER_OPACKETS:
		return (vsi->opackets);
	case IFCOUNTER_OERRORS:
		return (vsi->oerrors);
	case IFCOUNTER_COLLISIONS:
		/* Collisions are by standard impossible in 40G/10G Ethernet */
		return (0);
	case IFCOUNTER_IBYTES:
		return (vsi->ibytes);
	case IFCOUNTER_OBYTES:
		return (vsi->obytes);
	case IFCOUNTER_IMCASTS:
		return (vsi->imcasts);
	case IFCOUNTER_OMCASTS:
		return (vsi->omcasts);
	case IFCOUNTER_IQDROPS:
		return (vsi->iqdrops);
	case IFCOUNTER_OQDROPS:
		return (vsi->oqdrops);
	case IFCOUNTER_NOPROTO:
		return (vsi->noproto);
	default:
		return (if_get_counter_default(ifp, cnt));
	}
}

/**
 * iavf_free_pci_resources - Free PCI resources
 * @sc: device softc
 *
 * Called to release the PCI resources allocated during attach. May be called
 * in the error flow of attach_pre, or during detach as part of cleanup.
 */
static void
iavf_free_pci_resources(struct iavf_sc *sc)
{
	struct iavf_vsi		*vsi = &sc->vsi;
	struct iavf_rx_queue	*rx_que = vsi->rx_queues;
	device_t                dev = sc->dev;

	/* We may get here before stations are set up */
	if (rx_que == NULL)
		goto early;

	/* Release all interrupts */
	iflib_irq_free(vsi->ctx, &vsi->irq);

	for (int i = 0; i < vsi->num_rx_queues; i++, rx_que++)
		iflib_irq_free(vsi->ctx, &rx_que->que_irq);

early:
	if (sc->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->pci_mem), sc->pci_mem);
}

/**
 * iavf_setup_interface - Setup the device interface
 * @sc: device softc
 *
 * Called to setup some device interface settings, such as the ifmedia
 * structure.
 */
static void
iavf_setup_interface(struct iavf_sc *sc)
{
	struct iavf_vsi *vsi = &sc->vsi;
	if_ctx_t ctx = vsi->ctx;
	if_t ifp = iflib_get_ifp(ctx);

	iavf_dbg_init(sc, "begin\n");

	vsi->shared->isc_max_frame_size =
	    if_getmtu(ifp) + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	iavf_set_initial_baudrate(ifp);

	ifmedia_add(sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(sc->media, IFM_ETHER | IFM_AUTO);
}

/**
 * iavf_msix_adminq - Admin Queue interrupt handler
 * @arg: void pointer to the device softc
 *
 * Interrupt handler for the non-queue interrupt causes. Primarily this will
 * be the adminq interrupt, but also includes other miscellaneous causes.
 *
 * @returns FILTER_SCHEDULE_THREAD if the admin task needs to be run, otherwise
 * returns FITLER_HANDLED.
 */
static int
iavf_msix_adminq(void *arg)
{
	struct iavf_sc	*sc = (struct iavf_sc *)arg;
	struct iavf_hw	*hw = &sc->hw;
	u32		reg, mask;

	++sc->admin_irq;

	if (!iavf_test_state(&sc->state, IAVF_STATE_INITIALIZED))
		return (FILTER_HANDLED);

        reg = rd32(hw, IAVF_VFINT_ICR01);
	/*
	 * For masking off interrupt causes that need to be handled before
	 * they can be re-enabled
	 */
        mask = rd32(hw, IAVF_VFINT_ICR0_ENA1);

	/* Check on the cause */
	if (reg & IAVF_VFINT_ICR01_ADMINQ_MASK) {
		mask &= ~IAVF_VFINT_ICR0_ENA1_ADMINQ_MASK;

		/* Process messages outside of the iflib context lock */
		taskqueue_enqueue(sc->vc_tq, &sc->vc_task);
	}

	wr32(hw, IAVF_VFINT_ICR0_ENA1, mask);
	iavf_enable_adminq_irq(hw);

	return (FILTER_HANDLED);
}

/**
 * iavf_enable_intr - Enable device interrupts
 * @vsi: the main VSI
 *
 * Called to enable all queue interrupts.
 */
void
iavf_enable_intr(struct iavf_vsi *vsi)
{
	struct iavf_hw *hw = vsi->hw;
	struct iavf_rx_queue *que = vsi->rx_queues;

	iavf_enable_adminq_irq(hw);
	for (int i = 0; i < vsi->num_rx_queues; i++, que++)
		iavf_enable_queue_irq(hw, que->rxr.me);
}

/**
 * iavf_disable_intr - Disable device interrupts
 * @vsi: the main VSI
 *
 * Called to disable all interrupts
 *
 * @remark we never disable the admin status interrupt.
 */
void
iavf_disable_intr(struct iavf_vsi *vsi)
{
        struct iavf_hw *hw = vsi->hw;
        struct iavf_rx_queue *que = vsi->rx_queues;

	for (int i = 0; i < vsi->num_rx_queues; i++, que++)
		iavf_disable_queue_irq(hw, que->rxr.me);
}

/**
 * iavf_enable_queue_irq - Enable IRQ register for a queue interrupt
 * @hw: hardware structure
 * @id: IRQ vector to enable
 *
 * Writes the IAVF_VFINT_DYN_CTLN1 register to enable a given IRQ interrupt.
 */
static void
iavf_enable_queue_irq(struct iavf_hw *hw, int id)
{
	u32		reg;

	reg = IAVF_VFINT_DYN_CTLN1_INTENA_MASK |
	    IAVF_VFINT_DYN_CTLN1_CLEARPBA_MASK |
	    IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK;
	wr32(hw, IAVF_VFINT_DYN_CTLN1(id), reg);
}

/**
 * iavf_disable_queue_irq - Disable IRQ register for a queue interrupt
 * @hw: hardware structure
 * @id: IRQ vector to disable
 *
 * Writes the IAVF_VFINT_DYN_CTLN1 register to disable a given IRQ interrupt.
 */
static void
iavf_disable_queue_irq(struct iavf_hw *hw, int id)
{
	wr32(hw, IAVF_VFINT_DYN_CTLN1(id),
	    IAVF_VFINT_DYN_CTLN1_ITR_INDX_MASK);
	rd32(hw, IAVF_VFGEN_RSTAT);
}

/**
 * iavf_configure_itr - Get initial ITR values from tunable values.
 * @sc: device softc
 *
 * Load the initial tunable values for the ITR configuration.
 */
static void
iavf_configure_itr(struct iavf_sc *sc)
{
	iavf_configure_tx_itr(sc);
	iavf_configure_rx_itr(sc);
}

/**
 * iavf_set_queue_rx_itr - Update Rx ITR value
 * @que: Rx queue to update
 *
 * Provide a update to the queue RX interrupt moderation value.
 */
static void
iavf_set_queue_rx_itr(struct iavf_rx_queue *que)
{
	struct iavf_vsi	*vsi = que->vsi;
	struct iavf_hw	*hw = vsi->hw;
	struct rx_ring	*rxr = &que->rxr;

	/* Idle, do nothing */
	if (rxr->bytes == 0)
		return;

	/* Update the hardware if needed */
	if (rxr->itr != vsi->rx_itr_setting) {
		rxr->itr = vsi->rx_itr_setting;
		wr32(hw, IAVF_VFINT_ITRN1(IAVF_RX_ITR,
		    que->rxr.me), rxr->itr);
	}
}

/**
 * iavf_msix_que - Main Rx queue interrupt handler
 * @arg: void pointer to the Rx queue
 *
 * Main MSI-X interrupt handler for Rx queue interrupts
 *
 * @returns FILTER_SCHEDULE_THREAD if the main thread for Rx needs to run,
 * otherwise returns FILTER_HANDLED.
 */
static int
iavf_msix_que(void *arg)
{
	struct iavf_rx_queue *rx_que = (struct iavf_rx_queue *)arg;
	struct iavf_sc *sc = rx_que->vsi->back;

	++rx_que->irqs;

	if (!iavf_test_state(&sc->state, IAVF_STATE_RUNNING))
		return (FILTER_HANDLED);

	iavf_set_queue_rx_itr(rx_que);

	return (FILTER_SCHEDULE_THREAD);
}

/**
 * iavf_update_link_status - Update iflib Link status
 * @sc: device softc
 *
 * Notify the iflib stack of changes in link status. Called after the device
 * receives a virtchnl message indicating a change in link status.
 */
void
iavf_update_link_status(struct iavf_sc *sc)
{
	struct iavf_vsi *vsi = &sc->vsi;
	u64 baudrate;

	if (sc->link_up){
		if (vsi->link_active == FALSE) {
			vsi->link_active = TRUE;
			baudrate = iavf_baudrate_from_link_speed(sc);
			iavf_dbg_info(sc, "baudrate: %llu\n", (unsigned long long)baudrate);
			iflib_link_state_change(vsi->ctx, LINK_STATE_UP, baudrate);
		}
	} else { /* Link down */
		if (vsi->link_active == TRUE) {
			vsi->link_active = FALSE;
			iflib_link_state_change(vsi->ctx, LINK_STATE_DOWN, 0);
		}
	}
}

/**
 * iavf_stop - Stop the interface
 * @sc: device softc
 *
 * This routine disables all traffic on the adapter by disabling interrupts
 * and sending a message to the PF to tell it to stop the hardware
 * Tx/Rx LAN queues.
 */
static void
iavf_stop(struct iavf_sc *sc)
{
	iavf_clear_state(&sc->state, IAVF_STATE_RUNNING);

	iavf_disable_intr(&sc->vsi);

	iavf_disable_queues_with_retries(sc);
}

/**
 * iavf_if_stop - iflib stop handler
 * @ctx: iflib context
 *
 * Call iavf_stop to stop the interface.
 */
static void
iavf_if_stop(if_ctx_t ctx)
{
	struct iavf_sc *sc = iavf_sc_from_ctx(ctx);

	iavf_stop(sc);
}

/**
 * iavf_del_mac_filter - Delete a MAC filter
 * @sc: device softc
 * @macaddr: MAC address to remove
 *
 * Marks a MAC filter for deletion.
 *
 * @returns zero if the filter existed, or ENOENT if it did not.
 */
static int
iavf_del_mac_filter(struct iavf_sc *sc, u8 *macaddr)
{
	struct iavf_mac_filter	*f;

	f = iavf_find_mac_filter(sc, macaddr);
	if (f == NULL)
		return (ENOENT);

	f->flags |= IAVF_FILTER_DEL;
	return (0);
}

/**
 * iavf_init_tx_rsqs - Initialize Report Status array
 * @vsi: the main VSI
 *
 * Set the Report Status queue fields to zero in order to initialize the
 * queues for transmit.
 */
void
iavf_init_tx_rsqs(struct iavf_vsi *vsi)
{
	if_softc_ctx_t scctx = vsi->shared;
	struct iavf_tx_queue *tx_que;
	int i, j;

	for (i = 0, tx_que = vsi->tx_queues; i < vsi->num_tx_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;

		txr->tx_rs_cidx = txr->tx_rs_pidx;

		/* Initialize the last processed descriptor to be the end of
		 * the ring, rather than the start, so that we avoid an
		 * off-by-one error when calculating how many descriptors are
		 * done in the credits_update function.
		 */
		txr->tx_cidx_processed = scctx->isc_ntxd[0] - 1;

		for (j = 0; j < scctx->isc_ntxd[0]; j++)
			txr->tx_rsq[j] = QIDX_INVALID;
	}
}

/**
 * iavf_init_tx_cidx - Initialize Tx cidx values
 * @vsi: the main VSI
 *
 * Initialize the tx_cidx_processed values for Tx queues in order to
 * initialize the Tx queues for transmit.
 */
void
iavf_init_tx_cidx(struct iavf_vsi *vsi)
{
	if_softc_ctx_t scctx = vsi->shared;
	struct iavf_tx_queue *tx_que;
	int i;

	for (i = 0, tx_que = vsi->tx_queues; i < vsi->num_tx_queues; i++, tx_que++) {
		struct tx_ring *txr = &tx_que->txr;

		txr->tx_cidx_processed = scctx->isc_ntxd[0] - 1;
	}
}

/**
 * iavf_add_device_sysctls - Add device sysctls for configuration
 * @sc: device softc
 *
 * Add the main sysctl nodes and sysctls for device configuration.
 */
static void
iavf_add_device_sysctls(struct iavf_sc *sc)
{
	struct iavf_vsi *vsi = &sc->vsi;
	device_t dev = sc->dev;
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *debug_list;

	iavf_add_device_sysctls_common(sc);

	debug_list = iavf_create_debug_sysctl_tree(sc);

	iavf_add_debug_sysctls_common(sc, debug_list);

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "queue_interrupt_table", CTLTYPE_STRING | CTLFLAG_RD,
	    sc, 0, iavf_sysctl_queue_interrupt_table, "A", "View MSI-X indices for TX/RX queues");

#ifdef IAVF_DEBUG
	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_vf_reset", CTLTYPE_INT | CTLFLAG_WR,
	    sc, 0, iavf_sysctl_vf_reset, "A", "Request a VF reset from PF");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_vflr_reset", CTLTYPE_INT | CTLFLAG_WR,
	    sc, 0, iavf_sysctl_vflr_reset, "A", "Request a VFLR reset from HW");
#endif

	/* Add stats sysctls */
	iavf_add_vsi_sysctls(dev, vsi, ctx, "vsi");

	iavf_add_queues_sysctls(dev, vsi);
}

/**
 * iavf_add_queues_sysctls - Add per-queue sysctls
 * @dev: device pointer
 * @vsi: the main VSI
 *
 * Add sysctls for each Tx and Rx queue.
 */
void
iavf_add_queues_sysctls(device_t dev, struct iavf_vsi *vsi)
{
	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *vsi_list, *queue_list;
	struct sysctl_oid *queue_node;
	char queue_namebuf[32];

	struct iavf_rx_queue *rx_que;
	struct iavf_tx_queue *tx_que;
	struct tx_ring *txr;
	struct rx_ring *rxr;

	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	/* Queue statistics */
	for (int q = 0; q < vsi->num_rx_queues; q++) {
		bzero(queue_namebuf, sizeof(queue_namebuf));
		snprintf(queue_namebuf, IAVF_QUEUE_NAME_LEN, "rxq%02d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list,
		    OID_AUTO, queue_namebuf, CTLFLAG_RD, NULL, "RX Queue #");
		queue_list = SYSCTL_CHILDREN(queue_node);

		rx_que = &(vsi->rx_queues[q]);
		rxr = &(rx_que->rxr);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
				CTLFLAG_RD, &(rx_que->irqs),
				"irqs on this queue (both Tx and Rx)");

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "packets",
				CTLFLAG_RD, &(rxr->rx_packets),
				"Queue Packets Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "bytes",
				CTLFLAG_RD, &(rxr->rx_bytes),
				"Queue Bytes Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "desc_err",
				CTLFLAG_RD, &(rxr->desc_errs),
				"Queue Rx Descriptor Errors");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "itr",
				CTLFLAG_RD, &(rxr->itr), 0,
				"Queue Rx ITR Interval");
	}
	for (int q = 0; q < vsi->num_tx_queues; q++) {
		bzero(queue_namebuf, sizeof(queue_namebuf));
		snprintf(queue_namebuf, IAVF_QUEUE_NAME_LEN, "txq%02d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list,
		    OID_AUTO, queue_namebuf, CTLFLAG_RD, NULL, "TX Queue #");
		queue_list = SYSCTL_CHILDREN(queue_node);

		tx_que = &(vsi->tx_queues[q]);
		txr = &(tx_que->txr);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tso",
				CTLFLAG_RD, &(tx_que->tso),
				"TSO");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "mss_too_small",
				CTLFLAG_RD, &(txr->mss_too_small),
				"TSO sends with an MSS less than 64");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "packets",
				CTLFLAG_RD, &(txr->tx_packets),
				"Queue Packets Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "bytes",
				CTLFLAG_RD, &(txr->tx_bytes),
				"Queue Bytes Transmitted");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "itr",
				CTLFLAG_RD, &(txr->itr), 0,
				"Queue Tx ITR Interval");
	}
}

/**
 * iavf_driver_is_detaching - Check if the driver is detaching/unloading
 * @sc: device private softc
 *
 * @returns true if the driver is detaching, false otherwise.
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
iavf_driver_is_detaching(struct iavf_sc *sc)
{
	return (!iavf_test_state(&sc->state, IAVF_STATE_INITIALIZED) ||
		iflib_in_detach(sc->vsi.ctx));
}

/**
 * iavf_sysctl_queue_interrupt_table - Sysctl for displaying Tx queue mapping
 * @oidp: sysctl oid structure
 * @arg1: void pointer to device softc
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Print out mapping of TX queue indexes and Rx queue indexes to MSI-X vectors.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_sysctl_queue_interrupt_table(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	struct iavf_vsi *vsi = &sc->vsi;
	device_t dev = sc->dev;
	struct sbuf *buf;
	int error = 0;

	struct iavf_rx_queue *rx_que;
	struct iavf_tx_queue *tx_que;

	UNREFERENCED_2PARAMETER(arg2, oidp);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	for (int i = 0; i < vsi->num_rx_queues; i++) {
		rx_que = &vsi->rx_queues[i];
		sbuf_printf(buf, "(rxq %3d): %d\n", i, rx_que->msix);
	}
	for (int i = 0; i < vsi->num_tx_queues; i++) {
		tx_que = &vsi->tx_queues[i];
		sbuf_printf(buf, "(txq %3d): %d\n", i, tx_que->msix);
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

#ifdef IAVF_DEBUG
#define CTX_ACTIVE(ctx) ((if_getdrvflags(iflib_get_ifp(ctx)) & IFF_DRV_RUNNING))

/**
 * iavf_sysctl_vf_reset - Request a VF reset
 * @oidp: sysctl oid pointer
 * @arg1: void pointer to device softc
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Request a VF reset for the device.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_sysctl_vf_reset(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	int do_reset = 0, error = 0;

	UNREFERENCED_PARAMETER(arg2);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	error = sysctl_handle_int(oidp, &do_reset, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (do_reset == 1) {
		iavf_reset(sc);
		if (CTX_ACTIVE(sc->vsi.ctx))
			iflib_request_reset(sc->vsi.ctx);
	}

	return (error);
}

/**
 * iavf_sysctl_vflr_reset - Trigger a PCIe FLR for the device
 * @oidp: sysctl oid pointer
 * @arg1: void pointer to device softc
 * @arg2: unused
 * @req: sysctl request pointer
 *
 * Sysctl callback to trigger a PCIe FLR.
 *
 * @returns zero on success, or an error code on failure.
 */
static int
iavf_sysctl_vflr_reset(SYSCTL_HANDLER_ARGS)
{
	struct iavf_sc *sc = (struct iavf_sc *)arg1;
	device_t dev = sc->dev;
	int do_reset = 0, error = 0;

	UNREFERENCED_PARAMETER(arg2);

	if (iavf_driver_is_detaching(sc))
		return (ESHUTDOWN);

	error = sysctl_handle_int(oidp, &do_reset, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	if (do_reset == 1) {
		if (!pcie_flr(dev, max(pcie_get_max_completion_timeout(dev) / 1000, 10), true)) {
			device_printf(dev, "PCIE FLR failed\n");
			error = EIO;
		}
		else if (CTX_ACTIVE(sc->vsi.ctx))
			iflib_request_reset(sc->vsi.ctx);
	}

	return (error);
}
#undef CTX_ACTIVE
#endif
