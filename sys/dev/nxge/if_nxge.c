/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
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
 *
 * $FreeBSD$
 */

#include <dev/nxge/if_nxge.h>
#include <dev/nxge/xge-osdep.h>
#include <net/if_arp.h>
#include <sys/types.h>
#include <net/if.h>
#include <net/if_vlan_var.h>

int       copyright_print       = 0;
int       hal_driver_init_count = 0;
size_t    size                  = sizeof(int);

static void inline xge_flush_txds(xge_hal_channel_h);

/**
 * xge_probe
 * Probes for Xframe devices
 *
 * @dev Device handle
 *
 * Returns
 * BUS_PROBE_DEFAULT if device is supported
 * ENXIO if device is not supported
 */
int
xge_probe(device_t dev)
{
	int  devid    = pci_get_device(dev);
	int  vendorid = pci_get_vendor(dev);
	int  retValue = ENXIO;

	if(vendorid == XGE_PCI_VENDOR_ID) {
	    if((devid == XGE_PCI_DEVICE_ID_XENA_2) ||
	        (devid == XGE_PCI_DEVICE_ID_HERC_2)) {
	        if(!copyright_print) {
	            xge_os_printf(XGE_COPYRIGHT);
	            copyright_print = 1;
	        }
	        device_set_desc_copy(dev,
	            "Neterion Xframe 10 Gigabit Ethernet Adapter");
	        retValue = BUS_PROBE_DEFAULT;
	    }
	}

	return retValue;
}

/**
 * xge_init_params
 * Sets HAL parameter values (from kenv).
 *
 * @dconfig Device Configuration
 * @dev Device Handle
 */
void
xge_init_params(xge_hal_device_config_t *dconfig, device_t dev)
{
	int qindex, tindex, revision;
	device_t checkdev;
	xge_lldev_t *lldev = (xge_lldev_t *)device_get_softc(dev);

	dconfig->mtu                   = XGE_DEFAULT_INITIAL_MTU;
	dconfig->pci_freq_mherz        = XGE_DEFAULT_USER_HARDCODED;
	dconfig->device_poll_millis    = XGE_HAL_DEFAULT_DEVICE_POLL_MILLIS;
	dconfig->link_stability_period = XGE_HAL_DEFAULT_LINK_STABILITY_PERIOD;
	dconfig->mac.rmac_bcast_en     = XGE_DEFAULT_MAC_RMAC_BCAST_EN;
	dconfig->fifo.alignment_size   = XGE_DEFAULT_FIFO_ALIGNMENT_SIZE;

	XGE_GET_PARAM("hw.xge.enable_tso", (*lldev), enabled_tso,
	    XGE_DEFAULT_ENABLED_TSO);
	XGE_GET_PARAM("hw.xge.enable_lro", (*lldev), enabled_lro,
	    XGE_DEFAULT_ENABLED_LRO);
	XGE_GET_PARAM("hw.xge.enable_msi", (*lldev), enabled_msi,
	    XGE_DEFAULT_ENABLED_MSI);

	XGE_GET_PARAM("hw.xge.latency_timer", (*dconfig), latency_timer,
	    XGE_DEFAULT_LATENCY_TIMER);
	XGE_GET_PARAM("hw.xge.max_splits_trans", (*dconfig), max_splits_trans,
	    XGE_DEFAULT_MAX_SPLITS_TRANS);
	XGE_GET_PARAM("hw.xge.mmrb_count", (*dconfig), mmrb_count,
	    XGE_DEFAULT_MMRB_COUNT);
	XGE_GET_PARAM("hw.xge.shared_splits", (*dconfig), shared_splits,
	    XGE_DEFAULT_SHARED_SPLITS);
	XGE_GET_PARAM("hw.xge.isr_polling_cnt", (*dconfig), isr_polling_cnt,
	    XGE_DEFAULT_ISR_POLLING_CNT);
	XGE_GET_PARAM("hw.xge.stats_refresh_time_sec", (*dconfig),
	    stats_refresh_time_sec, XGE_DEFAULT_STATS_REFRESH_TIME_SEC);

	XGE_GET_PARAM_MAC("hw.xge.mac_tmac_util_period", tmac_util_period,
	    XGE_DEFAULT_MAC_TMAC_UTIL_PERIOD);
	XGE_GET_PARAM_MAC("hw.xge.mac_rmac_util_period", rmac_util_period,
	    XGE_DEFAULT_MAC_RMAC_UTIL_PERIOD);
	XGE_GET_PARAM_MAC("hw.xge.mac_rmac_pause_gen_en", rmac_pause_gen_en,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_GEN_EN);
	XGE_GET_PARAM_MAC("hw.xge.mac_rmac_pause_rcv_en", rmac_pause_rcv_en,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_RCV_EN);
	XGE_GET_PARAM_MAC("hw.xge.mac_rmac_pause_time", rmac_pause_time,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_TIME);
	XGE_GET_PARAM_MAC("hw.xge.mac_mc_pause_threshold_q0q3",
	    mc_pause_threshold_q0q3, XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q0Q3);
	XGE_GET_PARAM_MAC("hw.xge.mac_mc_pause_threshold_q4q7",
	    mc_pause_threshold_q4q7, XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q4Q7);

	XGE_GET_PARAM_FIFO("hw.xge.fifo_memblock_size", memblock_size,
	    XGE_DEFAULT_FIFO_MEMBLOCK_SIZE);
	XGE_GET_PARAM_FIFO("hw.xge.fifo_reserve_threshold", reserve_threshold,
	    XGE_DEFAULT_FIFO_RESERVE_THRESHOLD);
	XGE_GET_PARAM_FIFO("hw.xge.fifo_max_frags", max_frags,
	    XGE_DEFAULT_FIFO_MAX_FRAGS);

	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++) {
	    XGE_GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_intr", intr, qindex,
	        XGE_DEFAULT_FIFO_QUEUE_INTR);
	    XGE_GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_max", max, qindex,
	        XGE_DEFAULT_FIFO_QUEUE_MAX);
	    XGE_GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_initial", initial,
	        qindex, XGE_DEFAULT_FIFO_QUEUE_INITIAL);

	    for (tindex = 0; tindex < XGE_HAL_MAX_FIFO_TTI_NUM; tindex++) {
	        dconfig->fifo.queue[qindex].tti[tindex].enabled  = 1;
	        dconfig->fifo.queue[qindex].configured = 1;

	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_a",
	            urange_a, qindex, tindex,
	            XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_A);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_b",
	            urange_b, qindex, tindex,
	            XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_B);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_c",
	            urange_c, qindex, tindex,
	            XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_C);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_a",
	            ufc_a, qindex, tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_A);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_b",
	            ufc_b, qindex, tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_B);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_c",
	            ufc_c, qindex, tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_C);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_d",
	            ufc_d, qindex, tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_D);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI(
	            "hw.xge.fifo_queue_tti_timer_ci_en", timer_ci_en, qindex,
	            tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_CI_EN);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI(
	            "hw.xge.fifo_queue_tti_timer_ac_en", timer_ac_en, qindex,
	            tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_AC_EN);
	        XGE_GET_PARAM_FIFO_QUEUE_TTI(
	            "hw.xge.fifo_queue_tti_timer_val_us", timer_val_us, qindex,
	            tindex, XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_VAL_US);
	    }
	}

	XGE_GET_PARAM_RING("hw.xge.ring_memblock_size", memblock_size,
	    XGE_DEFAULT_RING_MEMBLOCK_SIZE);

	XGE_GET_PARAM_RING("hw.xge.ring_strip_vlan_tag", strip_vlan_tag,
	    XGE_DEFAULT_RING_STRIP_VLAN_TAG);

	XGE_GET_PARAM("hw.xge.buffer_mode", (*lldev), buffer_mode,
	    XGE_DEFAULT_BUFFER_MODE);
	if((lldev->buffer_mode < XGE_HAL_RING_QUEUE_BUFFER_MODE_1) ||
	    (lldev->buffer_mode > XGE_HAL_RING_QUEUE_BUFFER_MODE_2)) {
	    xge_trace(XGE_ERR, "Supported buffer modes are 1 and 2");
	    lldev->buffer_mode = XGE_HAL_RING_QUEUE_BUFFER_MODE_1;
	}

	for (qindex = 0; qindex < XGE_RING_COUNT; qindex++) {
	    dconfig->ring.queue[qindex].max_frm_len  = XGE_HAL_RING_USE_MTU;
	    dconfig->ring.queue[qindex].priority     = 0;
	    dconfig->ring.queue[qindex].configured   = 1;
	    dconfig->ring.queue[qindex].buffer_mode  =
	        (lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_2) ?
	        XGE_HAL_RING_QUEUE_BUFFER_MODE_3 : lldev->buffer_mode;

	    XGE_GET_PARAM_RING_QUEUE("hw.xge.ring_queue_max", max, qindex,
	        XGE_DEFAULT_RING_QUEUE_MAX);
	    XGE_GET_PARAM_RING_QUEUE("hw.xge.ring_queue_initial", initial,
	        qindex, XGE_DEFAULT_RING_QUEUE_INITIAL);
	    XGE_GET_PARAM_RING_QUEUE("hw.xge.ring_queue_dram_size_mb",
	        dram_size_mb, qindex, XGE_DEFAULT_RING_QUEUE_DRAM_SIZE_MB);
	    XGE_GET_PARAM_RING_QUEUE("hw.xge.ring_queue_indicate_max_pkts",
	        indicate_max_pkts, qindex,
	        XGE_DEFAULT_RING_QUEUE_INDICATE_MAX_PKTS);
	    XGE_GET_PARAM_RING_QUEUE("hw.xge.ring_queue_backoff_interval_us",
	        backoff_interval_us, qindex,
	        XGE_DEFAULT_RING_QUEUE_BACKOFF_INTERVAL_US);

	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_a", ufc_a,
	        qindex, XGE_DEFAULT_RING_QUEUE_RTI_UFC_A);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_b", ufc_b,
	        qindex, XGE_DEFAULT_RING_QUEUE_RTI_UFC_B);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_c", ufc_c,
	        qindex, XGE_DEFAULT_RING_QUEUE_RTI_UFC_C);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_d", ufc_d,
	        qindex, XGE_DEFAULT_RING_QUEUE_RTI_UFC_D);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_timer_ac_en",
	        timer_ac_en, qindex, XGE_DEFAULT_RING_QUEUE_RTI_TIMER_AC_EN);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_timer_val_us",
	        timer_val_us, qindex, XGE_DEFAULT_RING_QUEUE_RTI_TIMER_VAL_US);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_a",
	        urange_a, qindex, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_A);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_b",
	        urange_b, qindex, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_B);
	    XGE_GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_c",
	        urange_c, qindex, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_C);
	}

	if(dconfig->fifo.max_frags > (PAGE_SIZE/32)) {
	    xge_os_printf("fifo_max_frags = %d", dconfig->fifo.max_frags)
	    xge_os_printf("fifo_max_frags should be <= (PAGE_SIZE / 32) = %d",
	        (int)(PAGE_SIZE / 32))
	    xge_os_printf("Using fifo_max_frags = %d", (int)(PAGE_SIZE / 32))
	    dconfig->fifo.max_frags = (PAGE_SIZE / 32);
	}

	checkdev = pci_find_device(VENDOR_ID_AMD, DEVICE_ID_8131_PCI_BRIDGE);
	if(checkdev != NULL) {
	    /* Check Revision for 0x12 */
	    revision = pci_read_config(checkdev,
	        xge_offsetof(xge_hal_pci_config_t, revision), 1);
	    if(revision <= 0x12) {
	        /* Set mmrb_count to 1k and max splits = 2 */
	        dconfig->mmrb_count       = 1;
	        dconfig->max_splits_trans = XGE_HAL_THREE_SPLIT_TRANSACTION;
	    }
	}
}

/**
 * xge_buffer_sizes_set
 * Set buffer sizes based on Rx buffer mode
 *
 * @lldev Per-adapter Data
 * @buffer_mode Rx Buffer Mode
 */
void
xge_rx_buffer_sizes_set(xge_lldev_t *lldev, int buffer_mode, int mtu)
{
	int index = 0;
	int frame_header = XGE_HAL_MAC_HEADER_MAX_SIZE;
	int buffer_size = mtu + frame_header;

	xge_os_memzero(lldev->rxd_mbuf_len, sizeof(lldev->rxd_mbuf_len));

	if(buffer_mode != XGE_HAL_RING_QUEUE_BUFFER_MODE_5)
	    lldev->rxd_mbuf_len[buffer_mode - 1] = mtu;

	lldev->rxd_mbuf_len[0] = (buffer_mode == 1) ? buffer_size:frame_header;

	if(buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5)
	    lldev->rxd_mbuf_len[1] = XGE_HAL_TCPIP_HEADER_MAX_SIZE;

	if(buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	    index = 2;
	    buffer_size -= XGE_HAL_TCPIP_HEADER_MAX_SIZE;
	    while(buffer_size > MJUMPAGESIZE) {
	        lldev->rxd_mbuf_len[index++] = MJUMPAGESIZE;
	        buffer_size -= MJUMPAGESIZE;
	    }
	    XGE_ALIGN_TO(buffer_size, 128);
	    lldev->rxd_mbuf_len[index] = buffer_size;
	    lldev->rxd_mbuf_cnt = index + 1;
	}

	for(index = 0; index < buffer_mode; index++)
	    xge_trace(XGE_TRACE, "Buffer[%d] %d\n", index,
	        lldev->rxd_mbuf_len[index]);
}

/**
 * xge_buffer_mode_init
 * Init Rx buffer mode
 *
 * @lldev Per-adapter Data
 * @mtu Interface MTU
 */
void
xge_buffer_mode_init(xge_lldev_t *lldev, int mtu)
{
	int index = 0, buffer_size = 0;
	xge_hal_ring_config_t *ring_config = &((lldev->devh)->config.ring);

	buffer_size = mtu + XGE_HAL_MAC_HEADER_MAX_SIZE;

	if(lldev->enabled_lro)
	    (lldev->ifnetp)->if_capenable |= IFCAP_LRO;
	else
	    (lldev->ifnetp)->if_capenable &= ~IFCAP_LRO;

	lldev->rxd_mbuf_cnt = lldev->buffer_mode;
	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_2) {
	    XGE_SET_BUFFER_MODE_IN_RINGS(XGE_HAL_RING_QUEUE_BUFFER_MODE_3);
	    ring_config->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_B;
	}
	else {
	    XGE_SET_BUFFER_MODE_IN_RINGS(lldev->buffer_mode);
	    ring_config->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_A;
	}
	xge_rx_buffer_sizes_set(lldev, lldev->buffer_mode, mtu);

	xge_os_printf("%s: TSO %s", device_get_nameunit(lldev->device),
	    ((lldev->enabled_tso) ? "Enabled":"Disabled"));
	xge_os_printf("%s: LRO %s", device_get_nameunit(lldev->device),
	    ((lldev->ifnetp)->if_capenable & IFCAP_LRO) ? "Enabled":"Disabled");
	xge_os_printf("%s: Rx %d Buffer Mode Enabled",
	    device_get_nameunit(lldev->device), lldev->buffer_mode);
}

/**
 * xge_driver_initialize
 * Initializes HAL driver (common for all devices)
 *
 * Returns
 * XGE_HAL_OK if success
 * XGE_HAL_ERR_BAD_DRIVER_CONFIG if driver configuration parameters are invalid
 */
int
xge_driver_initialize(void)
{
	xge_hal_uld_cbs_t       uld_callbacks;
	xge_hal_driver_config_t driver_config;
	xge_hal_status_e        status = XGE_HAL_OK;

	/* Initialize HAL driver */
	if(!hal_driver_init_count) {
	    xge_os_memzero(&uld_callbacks, sizeof(xge_hal_uld_cbs_t));
	    xge_os_memzero(&driver_config, sizeof(xge_hal_driver_config_t));

	    /*
	     * Initial and maximum size of the queue used to store the events
	     * like Link up/down (xge_hal_event_e)
	     */
	    driver_config.queue_size_initial = XGE_HAL_MIN_QUEUE_SIZE_INITIAL;
	    driver_config.queue_size_max     = XGE_HAL_MAX_QUEUE_SIZE_MAX;

	    uld_callbacks.link_up   = xge_callback_link_up;
	    uld_callbacks.link_down = xge_callback_link_down;
	    uld_callbacks.crit_err  = xge_callback_crit_err;
	    uld_callbacks.event     = xge_callback_event;

	    status = xge_hal_driver_initialize(&driver_config, &uld_callbacks);
	    if(status != XGE_HAL_OK) {
	        XGE_EXIT_ON_ERR("xgeX: Initialization of HAL driver failed",
	            xdi_out, status);
	    }
	}
	hal_driver_init_count = hal_driver_init_count + 1;

	xge_hal_driver_debug_module_mask_set(0xffffffff);
	xge_hal_driver_debug_level_set(XGE_TRACE);

xdi_out:
	return status;
}

/**
 * xge_media_init
 * Initializes, adds and sets media
 *
 * @devc Device Handle
 */
void
xge_media_init(device_t devc)
{
	xge_lldev_t *lldev = (xge_lldev_t *)device_get_softc(devc);

	/* Initialize Media */
	ifmedia_init(&lldev->media, IFM_IMASK, xge_ifmedia_change,
	    xge_ifmedia_status);

	/* Add supported media */
	ifmedia_add(&lldev->media, IFM_ETHER | IFM_1000_SX | IFM_FDX, 0, NULL);
	ifmedia_add(&lldev->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	ifmedia_add(&lldev->media, IFM_ETHER | IFM_AUTO,    0, NULL);
	ifmedia_add(&lldev->media, IFM_ETHER | IFM_10G_SR,  0, NULL);
	ifmedia_add(&lldev->media, IFM_ETHER | IFM_10G_LR,  0, NULL);

	/* Set media */
	ifmedia_set(&lldev->media, IFM_ETHER | IFM_AUTO);
}

/**
 * xge_pci_space_save
 * Save PCI configuration space
 *
 * @dev Device Handle
 */
void
xge_pci_space_save(device_t dev)
{
	struct pci_devinfo *dinfo = NULL;

	dinfo = device_get_ivars(dev);
	xge_trace(XGE_TRACE, "Saving PCI configuration space");
	pci_cfg_save(dev, dinfo, 0);
}

/**
 * xge_pci_space_restore
 * Restore saved PCI configuration space
 *
 * @dev Device Handle
 */
void
xge_pci_space_restore(device_t dev)
{
	struct pci_devinfo *dinfo = NULL;

	dinfo = device_get_ivars(dev);
	xge_trace(XGE_TRACE, "Restoring PCI configuration space");
	pci_cfg_restore(dev, dinfo);
}

/**
 * xge_msi_info_save
 * Save MSI info
 *
 * @lldev Per-adapter Data
 */
void
xge_msi_info_save(xge_lldev_t * lldev)
{
	xge_os_pci_read16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_control),
	    &lldev->msi_info.msi_control);
	xge_os_pci_read32(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_lower_address),
	    &lldev->msi_info.msi_lower_address);
	xge_os_pci_read32(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_higher_address),
	    &lldev->msi_info.msi_higher_address);
	xge_os_pci_read16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_data),
	    &lldev->msi_info.msi_data);
}

/**
 * xge_msi_info_restore
 * Restore saved MSI info
 *
 * @dev Device Handle
 */
void
xge_msi_info_restore(xge_lldev_t *lldev)
{
	/*
	 * If interface is made down and up, traffic fails. It was observed that
	 * MSI information were getting reset on down. Restoring them.
	 */
	xge_os_pci_write16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_control),
	    lldev->msi_info.msi_control);

	xge_os_pci_write32(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_lower_address),
	    lldev->msi_info.msi_lower_address);

	xge_os_pci_write32(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_higher_address),
	    lldev->msi_info.msi_higher_address);

	xge_os_pci_write16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_data),
	    lldev->msi_info.msi_data);
}

/**
 * xge_init_mutex
 * Initializes mutexes used in driver
 *
 * @lldev  Per-adapter Data
 */
void
xge_mutex_init(xge_lldev_t *lldev)
{
	int qindex;

	sprintf(lldev->mtx_name_drv, "%s_drv",
	    device_get_nameunit(lldev->device));
	mtx_init(&lldev->mtx_drv, lldev->mtx_name_drv, MTX_NETWORK_LOCK,
	    MTX_DEF);

	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++) {
	    sprintf(lldev->mtx_name_tx[qindex], "%s_tx_%d",
	        device_get_nameunit(lldev->device), qindex);
	    mtx_init(&lldev->mtx_tx[qindex], lldev->mtx_name_tx[qindex], NULL,
	        MTX_DEF);
	}
}

/**
 * xge_mutex_destroy
 * Destroys mutexes used in driver
 *
 * @lldev Per-adapter Data
 */
void
xge_mutex_destroy(xge_lldev_t *lldev)
{
	int qindex;

	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++)
	    mtx_destroy(&lldev->mtx_tx[qindex]);
	mtx_destroy(&lldev->mtx_drv);
}

/**
 * xge_print_info
 * Print device and driver information
 *
 * @lldev Per-adapter Data
 */
void
xge_print_info(xge_lldev_t *lldev)
{
	device_t dev = lldev->device;
	xge_hal_device_t *hldev = lldev->devh;
	xge_hal_status_e status = XGE_HAL_OK;
	u64 val64 = 0;
	const char *xge_pci_bus_speeds[17] = {
	    "PCI 33MHz Bus",
	    "PCI 66MHz Bus",
	    "PCIX(M1) 66MHz Bus",
	    "PCIX(M1) 100MHz Bus",
	    "PCIX(M1) 133MHz Bus",
	    "PCIX(M2) 133MHz Bus",
	    "PCIX(M2) 200MHz Bus",
	    "PCIX(M2) 266MHz Bus",
	    "PCIX(M1) Reserved",
	    "PCIX(M1) 66MHz Bus (Not Supported)",
	    "PCIX(M1) 100MHz Bus (Not Supported)",
	    "PCIX(M1) 133MHz Bus (Not Supported)",
	    "PCIX(M2) Reserved",
	    "PCIX 533 Reserved",
	    "PCI Basic Mode",
	    "PCIX Basic Mode",
	    "PCI Invalid Mode"
	};

	xge_os_printf("%s: Xframe%s %s Revision %d Driver v%s",
	    device_get_nameunit(dev),
	    ((hldev->device_id == XGE_PCI_DEVICE_ID_XENA_2) ? "I" : "II"),
	    hldev->vpd_data.product_name, hldev->revision, XGE_DRIVER_VERSION);
	xge_os_printf("%s: Serial Number %s",
	    device_get_nameunit(dev), hldev->vpd_data.serial_num);

	if(pci_get_device(dev) == XGE_PCI_DEVICE_ID_HERC_2) {
	    status = xge_hal_mgmt_reg_read(hldev, 0,
	        xge_offsetof(xge_hal_pci_bar0_t, pci_info), &val64);
	    if(status != XGE_HAL_OK)
	        xge_trace(XGE_ERR, "Error for getting bus speed");

	    xge_os_printf("%s: Adapter is on %s bit %s",
	        device_get_nameunit(dev), ((val64 & BIT(8)) ? "32":"64"),
	        (xge_pci_bus_speeds[((val64 & XGE_HAL_PCI_INFO) >> 60)]));
	}

	xge_os_printf("%s: Using %s Interrupts",
	    device_get_nameunit(dev),
	    (lldev->enabled_msi == XGE_HAL_INTR_MODE_MSI) ? "MSI":"Line");
}

/**
 * xge_create_dma_tags
 * Creates DMA tags for both Tx and Rx
 *
 * @dev Device Handle
 *
 * Returns XGE_HAL_OK or XGE_HAL_FAIL (if errors)
 */
xge_hal_status_e
xge_create_dma_tags(device_t dev)
{
	xge_lldev_t *lldev = (xge_lldev_t *)device_get_softc(dev);
	xge_hal_status_e status = XGE_HAL_FAIL;
	int mtu = (lldev->ifnetp)->if_mtu, maxsize;

	/* DMA tag for Tx */
	status = bus_dma_tag_create(
	    bus_get_dma_tag(dev),                /* Parent                    */
	    PAGE_SIZE,                           /* Alignment                 */
	    0,                                   /* Bounds                    */
	    BUS_SPACE_MAXADDR,                   /* Low Address               */
	    BUS_SPACE_MAXADDR,                   /* High Address              */
	    NULL,                                /* Filter Function           */
	    NULL,                                /* Filter Function Arguments */
	    MCLBYTES * XGE_MAX_SEGS,             /* Maximum Size              */
	    XGE_MAX_SEGS,                        /* Number of Segments        */
	    MCLBYTES,                            /* Maximum Segment Size      */
	    BUS_DMA_ALLOCNOW,                    /* Flags                     */
	    NULL,                                /* Lock Function             */
	    NULL,                                /* Lock Function Arguments   */
	    (&lldev->dma_tag_tx));               /* DMA Tag                   */
	if(status != 0)
	    goto _exit;

	maxsize = mtu + XGE_HAL_MAC_HEADER_MAX_SIZE;
	if(maxsize <= MCLBYTES) {
	    maxsize = MCLBYTES;
	}
	else {
	    if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5)
	        maxsize = MJUMPAGESIZE;
	    else
	        maxsize = (maxsize <= MJUMPAGESIZE) ? MJUMPAGESIZE : MJUM9BYTES;
	}
	
	/* DMA tag for Rx */
	status = bus_dma_tag_create(
	    bus_get_dma_tag(dev),                /* Parent                    */
	    PAGE_SIZE,                           /* Alignment                 */
	    0,                                   /* Bounds                    */
	    BUS_SPACE_MAXADDR,                   /* Low Address               */
	    BUS_SPACE_MAXADDR,                   /* High Address              */
	    NULL,                                /* Filter Function           */
	    NULL,                                /* Filter Function Arguments */
	    maxsize,                             /* Maximum Size              */
	    1,                                   /* Number of Segments        */
	    maxsize,                             /* Maximum Segment Size      */
	    BUS_DMA_ALLOCNOW,                    /* Flags                     */
	    NULL,                                /* Lock Function             */
	    NULL,                                /* Lock Function Arguments   */
	    (&lldev->dma_tag_rx));               /* DMA Tag                   */
	if(status != 0)
	    goto _exit1;

	status = bus_dmamap_create(lldev->dma_tag_rx, BUS_DMA_NOWAIT,
	    &lldev->extra_dma_map);
	if(status != 0)
	    goto _exit2;

	status = XGE_HAL_OK;
	goto _exit;

_exit2:
	status = bus_dma_tag_destroy(lldev->dma_tag_rx);
	if(status != 0)
	    xge_trace(XGE_ERR, "Rx DMA tag destroy failed");
_exit1:
	status = bus_dma_tag_destroy(lldev->dma_tag_tx);
	if(status != 0)
	    xge_trace(XGE_ERR, "Tx DMA tag destroy failed");
	status = XGE_HAL_FAIL;
_exit:
	return status;
}

/**
 * xge_confirm_changes
 * Disables and Enables interface to apply requested change
 *
 * @lldev Per-adapter Data
 * @mtu_set Is it called for changing MTU? (Yes: 1, No: 0)
 *
 * Returns 0 or Error Number
 */
void
xge_confirm_changes(xge_lldev_t *lldev, xge_option_e option)
{
	if(lldev->initialized == 0) goto _exit1;

	mtx_lock(&lldev->mtx_drv);
	if_down(lldev->ifnetp);
	xge_device_stop(lldev, XGE_HAL_CHANNEL_OC_NORMAL);

	if(option == XGE_SET_MTU)
	    (lldev->ifnetp)->if_mtu = lldev->mtu;
	else
	    xge_buffer_mode_init(lldev, lldev->mtu);

	xge_device_init(lldev, XGE_HAL_CHANNEL_OC_NORMAL);
	if_up(lldev->ifnetp);
	mtx_unlock(&lldev->mtx_drv);
	goto _exit;

_exit1:
	/* Request was to change MTU and device not initialized */
	if(option == XGE_SET_MTU) {
	    (lldev->ifnetp)->if_mtu = lldev->mtu;
	    xge_buffer_mode_init(lldev, lldev->mtu);
	}
_exit:
	return;
}

/**
 * xge_change_lro_status
 * Enable/Disable LRO feature
 *
 * @SYSCTL_HANDLER_ARGS sysctl_oid structure with arguments
 *
 * Returns 0 or error number.
 */
static int
xge_change_lro_status(SYSCTL_HANDLER_ARGS)
{
	xge_lldev_t *lldev = (xge_lldev_t *)arg1;
	int request = lldev->enabled_lro, status = XGE_HAL_OK;

	status = sysctl_handle_int(oidp, &request, arg2, req);
	if((status != XGE_HAL_OK) || (!req->newptr))
	    goto _exit;

	if((request < 0) || (request > 1)) {
	    status = EINVAL;
	    goto _exit;
	}

	/* Return if current and requested states are same */
	if(request == lldev->enabled_lro){
	    xge_trace(XGE_ERR, "LRO is already %s",
	        ((request) ? "enabled" : "disabled"));
	    goto _exit;
	}

	lldev->enabled_lro = request;
	xge_confirm_changes(lldev, XGE_CHANGE_LRO);
	arg2 = lldev->enabled_lro;

_exit:
	return status;
}

/**
 * xge_add_sysctl_handlers
 * Registers sysctl parameter value update handlers
 *
 * @lldev Per-adapter data
 */
void
xge_add_sysctl_handlers(xge_lldev_t *lldev)
{
	struct sysctl_ctx_list *context_list =
	    device_get_sysctl_ctx(lldev->device);
	struct sysctl_oid *oid = device_get_sysctl_tree(lldev->device);

	SYSCTL_ADD_PROC(context_list, SYSCTL_CHILDREN(oid), OID_AUTO,
	    "enable_lro", CTLTYPE_INT | CTLFLAG_RW, lldev, 0,
	    xge_change_lro_status, "I", "Enable or disable LRO feature");
}

/**
 * xge_attach
 * Connects driver to the system if probe was success
 *
 * @dev Device Handle
 */
int
xge_attach(device_t dev)
{
	xge_hal_device_config_t *device_config;
	xge_hal_device_attr_t   attr;
	xge_lldev_t             *lldev;
	xge_hal_device_t        *hldev;
	xge_pci_info_t          *pci_info;
	struct ifnet            *ifnetp;
	int                     rid, rid0, rid1, error;
	int                     msi_count = 0, status = XGE_HAL_OK;
	int                     enable_msi = XGE_HAL_INTR_MODE_IRQLINE;

	device_config = xge_os_malloc(NULL, sizeof(xge_hal_device_config_t));
	if(!device_config) {
	    XGE_EXIT_ON_ERR("Memory allocation for device configuration failed",
	        attach_out_config, ENOMEM);
	}

	lldev = (xge_lldev_t *) device_get_softc(dev);
	if(!lldev) {
	    XGE_EXIT_ON_ERR("Adapter softc is NULL", attach_out, ENOMEM);
	}
	lldev->device = dev;

	xge_mutex_init(lldev);

	error = xge_driver_initialize();
	if(error != XGE_HAL_OK) {
	    xge_resources_free(dev, xge_free_mutex);
	    XGE_EXIT_ON_ERR("Initializing driver failed", attach_out, ENXIO);
	}

	/* HAL device */
	hldev =
	    (xge_hal_device_t *)xge_os_malloc(NULL, sizeof(xge_hal_device_t));
	if(!hldev) {
	    xge_resources_free(dev, xge_free_terminate_hal_driver);
	    XGE_EXIT_ON_ERR("Memory allocation for HAL device failed",
	        attach_out, ENOMEM);
	}
	lldev->devh = hldev;

	/* Our private structure */
	pci_info =
	    (xge_pci_info_t*) xge_os_malloc(NULL, sizeof(xge_pci_info_t));
	if(!pci_info) {
	    xge_resources_free(dev, xge_free_hal_device);
	    XGE_EXIT_ON_ERR("Memory allocation for PCI info. failed",
	        attach_out, ENOMEM);
	}
	lldev->pdev      = pci_info;
	pci_info->device = dev;

	/* Set bus master */
	pci_enable_busmaster(dev);

	/* Get virtual address for BAR0 */
	rid0 = PCIR_BAR(0);
	pci_info->regmap0 = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid0,
	    RF_ACTIVE);
	if(pci_info->regmap0 == NULL) {
	    xge_resources_free(dev, xge_free_pci_info);
	    XGE_EXIT_ON_ERR("Bus resource allocation for BAR0 failed",
	        attach_out, ENOMEM);
	}
	attr.bar0 = (char *)pci_info->regmap0;

	pci_info->bar0resource = (xge_bus_resource_t*)
	    xge_os_malloc(NULL, sizeof(xge_bus_resource_t));
	if(pci_info->bar0resource == NULL) {
	    xge_resources_free(dev, xge_free_bar0);
	    XGE_EXIT_ON_ERR("Memory allocation for BAR0 Resources failed",
	        attach_out, ENOMEM);
	}
	((xge_bus_resource_t *)(pci_info->bar0resource))->bus_tag =
	    rman_get_bustag(pci_info->regmap0);
	((xge_bus_resource_t *)(pci_info->bar0resource))->bus_handle =
	    rman_get_bushandle(pci_info->regmap0);
	((xge_bus_resource_t *)(pci_info->bar0resource))->bar_start_addr =
	    pci_info->regmap0;

	/* Get virtual address for BAR1 */
	rid1 = PCIR_BAR(2);
	pci_info->regmap1 = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid1,
	    RF_ACTIVE);
	if(pci_info->regmap1 == NULL) {
	    xge_resources_free(dev, xge_free_bar0_resource);
	    XGE_EXIT_ON_ERR("Bus resource allocation for BAR1 failed",
	        attach_out, ENOMEM);
	}
	attr.bar1 = (char *)pci_info->regmap1;

	pci_info->bar1resource = (xge_bus_resource_t*)
	    xge_os_malloc(NULL, sizeof(xge_bus_resource_t));
	if(pci_info->bar1resource == NULL) {
	    xge_resources_free(dev, xge_free_bar1);
	    XGE_EXIT_ON_ERR("Memory allocation for BAR1 Resources failed",
	        attach_out, ENOMEM);
	}
	((xge_bus_resource_t *)(pci_info->bar1resource))->bus_tag =
	    rman_get_bustag(pci_info->regmap1);
	((xge_bus_resource_t *)(pci_info->bar1resource))->bus_handle =
	    rman_get_bushandle(pci_info->regmap1);
	((xge_bus_resource_t *)(pci_info->bar1resource))->bar_start_addr =
	    pci_info->regmap1;

	/* Save PCI config space */
	xge_pci_space_save(dev);

	attr.regh0 = (xge_bus_resource_t *) pci_info->bar0resource;
	attr.regh1 = (xge_bus_resource_t *) pci_info->bar1resource;
	attr.irqh  = lldev->irqhandle;
	attr.cfgh  = pci_info;
	attr.pdev  = pci_info;

	/* Initialize device configuration parameters */
	xge_init_params(device_config, dev);

	rid = 0;
	if(lldev->enabled_msi) {
	    /* Number of MSI messages supported by device */
	    msi_count = pci_msi_count(dev);
	    if(msi_count > 1) {
	        /* Device supports MSI */
	        if(bootverbose) {
	            xge_trace(XGE_ERR, "MSI count: %d", msi_count);
	            xge_trace(XGE_ERR, "Now, driver supporting 1 message");
	        }
	        msi_count = 1;
	        error = pci_alloc_msi(dev, &msi_count);
	        if(error == 0) {
	            if(bootverbose)
	                xge_trace(XGE_ERR, "Allocated messages: %d", msi_count);
	            enable_msi = XGE_HAL_INTR_MODE_MSI;
	            rid = 1;
	        }
	        else {
	            if(bootverbose)
	                xge_trace(XGE_ERR, "pci_alloc_msi failed, %d", error);
	        }
	    }
	}
	lldev->enabled_msi = enable_msi;

	/* Allocate resource for irq */
	lldev->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    (RF_SHAREABLE | RF_ACTIVE));
	if(lldev->irq == NULL) {
	    xge_trace(XGE_ERR, "Allocating irq resource for %s failed",
	        ((rid == 0) ? "line interrupt" : "MSI"));
	    if(rid == 1) {
	        error = pci_release_msi(dev);
	        if(error != 0) {
	            xge_trace(XGE_ERR, "Releasing MSI resources failed %d",
	                error);
	            xge_trace(XGE_ERR, "Requires reboot to use MSI again");
	        }
	        xge_trace(XGE_ERR, "Trying line interrupts");
	        rid = 0;
	        lldev->enabled_msi = XGE_HAL_INTR_MODE_IRQLINE;
	        lldev->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	            (RF_SHAREABLE | RF_ACTIVE));
	    }
	    if(lldev->irq == NULL) {
	        xge_trace(XGE_ERR, "Allocating irq resource failed");
	        xge_resources_free(dev, xge_free_bar1_resource);
	        status = ENOMEM;
	        goto attach_out;
	    }
	}

	device_config->intr_mode = lldev->enabled_msi;
	if(bootverbose) {
	    xge_trace(XGE_TRACE, "rid: %d, Mode: %d, MSI count: %d", rid,
	        lldev->enabled_msi, msi_count);
	}

	/* Initialize HAL device */
	error = xge_hal_device_initialize(hldev, &attr, device_config);
	if(error != XGE_HAL_OK) {
	    xge_resources_free(dev, xge_free_irq_resource);
	    XGE_EXIT_ON_ERR("Initializing HAL device failed", attach_out,
	        ENXIO);
	}

	xge_hal_device_private_set(hldev, lldev);

	error = xge_interface_setup(dev);
	if(error != 0) {
	    status = error;
	    goto attach_out;
	}

	ifnetp         = lldev->ifnetp;
	ifnetp->if_mtu = device_config->mtu;

	xge_media_init(dev);

	/* Associate interrupt handler with the device */
	if(lldev->enabled_msi == XGE_HAL_INTR_MODE_MSI) {
	    error = bus_setup_intr(dev, lldev->irq,
	        (INTR_TYPE_NET | INTR_MPSAFE),
#if __FreeBSD_version > 700030
	        NULL,
#endif
	        xge_isr_msi, lldev, &lldev->irqhandle);
	    xge_msi_info_save(lldev);
	}
	else {
	    error = bus_setup_intr(dev, lldev->irq,
	        (INTR_TYPE_NET | INTR_MPSAFE),
#if __FreeBSD_version > 700030
	        xge_isr_filter,
#endif
	        xge_isr_line, lldev, &lldev->irqhandle);
	}
	if(error != 0) {
	    xge_resources_free(dev, xge_free_media_interface);
	    XGE_EXIT_ON_ERR("Associating interrupt handler with device failed",
	        attach_out, ENXIO);
	}

	xge_print_info(lldev);

	xge_add_sysctl_handlers(lldev);

	xge_buffer_mode_init(lldev, device_config->mtu);

attach_out:
	xge_os_free(NULL, device_config, sizeof(xge_hal_device_config_t));
attach_out_config:
	return status;
}

/**
 * xge_resources_free
 * Undo what-all we did during load/attach
 *
 * @dev Device Handle
 * @error Identifies what-all to undo
 */
void
xge_resources_free(device_t dev, xge_lables_e error)
{
	xge_lldev_t *lldev;
	xge_pci_info_t *pci_info;
	xge_hal_device_t *hldev;
	int rid, status;

	/* LL Device */
	lldev = (xge_lldev_t *) device_get_softc(dev);
	pci_info = lldev->pdev;

	/* HAL Device */
	hldev = lldev->devh;

	switch(error) {
	    case xge_free_all:
	        /* Teardown interrupt handler - device association */
	        bus_teardown_intr(dev, lldev->irq, lldev->irqhandle);

	    case xge_free_media_interface:
	        /* Media */
	        ifmedia_removeall(&lldev->media);

	        /* Detach Ether */
	        ether_ifdetach(lldev->ifnetp);
	        if_free(lldev->ifnetp);

	        xge_hal_device_private_set(hldev, NULL);
	        xge_hal_device_disable(hldev);

	    case xge_free_terminate_hal_device:
	        /* HAL Device */
	        xge_hal_device_terminate(hldev);

	    case xge_free_irq_resource:
	        /* Release IRQ resource */
	        bus_release_resource(dev, SYS_RES_IRQ,
	            ((lldev->enabled_msi == XGE_HAL_INTR_MODE_IRQLINE) ? 0:1),
	            lldev->irq);

	        if(lldev->enabled_msi == XGE_HAL_INTR_MODE_MSI) {
	            status = pci_release_msi(dev);
	            if(status != 0) {
	                if(bootverbose) {
	                    xge_trace(XGE_ERR,
	                        "pci_release_msi returned %d", status);
	                }
	            }
	        }

	    case xge_free_bar1_resource:
	        /* Restore PCI configuration space */
	        xge_pci_space_restore(dev);

	        /* Free bar1resource */
	        xge_os_free(NULL, pci_info->bar1resource,
	            sizeof(xge_bus_resource_t));

	    case xge_free_bar1:
	        /* Release BAR1 */
	        rid = PCIR_BAR(2);
	        bus_release_resource(dev, SYS_RES_MEMORY, rid,
	            pci_info->regmap1);

	    case xge_free_bar0_resource:
	        /* Free bar0resource */
	        xge_os_free(NULL, pci_info->bar0resource,
	            sizeof(xge_bus_resource_t));

	    case xge_free_bar0:
	        /* Release BAR0 */
	        rid = PCIR_BAR(0);
	        bus_release_resource(dev, SYS_RES_MEMORY, rid,
	            pci_info->regmap0);

	    case xge_free_pci_info:
	        /* Disable Bus Master */
	        pci_disable_busmaster(dev);

	        /* Free pci_info_t */
	        lldev->pdev = NULL;
	        xge_os_free(NULL, pci_info, sizeof(xge_pci_info_t));

	    case xge_free_hal_device:
	        /* Free device configuration struct and HAL device */
	        xge_os_free(NULL, hldev, sizeof(xge_hal_device_t));

	    case xge_free_terminate_hal_driver:
	        /* Terminate HAL driver */
	        hal_driver_init_count = hal_driver_init_count - 1;
	        if(!hal_driver_init_count) {
	            xge_hal_driver_terminate();
	        }

	    case xge_free_mutex:
	        xge_mutex_destroy(lldev);
	}
}

/**
 * xge_detach
 * Detaches driver from the Kernel subsystem
 *
 * @dev Device Handle
 */
int
xge_detach(device_t dev)
{
	xge_lldev_t *lldev = (xge_lldev_t *)device_get_softc(dev);

	if(lldev->in_detach == 0) {
	    lldev->in_detach = 1;
	    xge_stop(lldev);
	    xge_resources_free(dev, xge_free_all);
	}

	return 0;
}

/**
 * xge_shutdown
 * To shutdown device before system shutdown
 *
 * @dev Device Handle
 */
int
xge_shutdown(device_t dev)
{
	xge_lldev_t *lldev = (xge_lldev_t *) device_get_softc(dev);
	xge_stop(lldev);

	return 0;
}

/**
 * xge_interface_setup
 * Setup interface
 *
 * @dev Device Handle
 *
 * Returns 0 on success, ENXIO/ENOMEM on failure
 */
int
xge_interface_setup(device_t dev)
{
	u8 mcaddr[ETHER_ADDR_LEN];
	xge_hal_status_e status;
	xge_lldev_t *lldev = (xge_lldev_t *)device_get_softc(dev);
	struct ifnet *ifnetp;
	xge_hal_device_t *hldev = lldev->devh;

	/* Get the MAC address of the device */
	status = xge_hal_device_macaddr_get(hldev, 0, &mcaddr);
	if(status != XGE_HAL_OK) {
	    xge_resources_free(dev, xge_free_terminate_hal_device);
	    XGE_EXIT_ON_ERR("Getting MAC address failed", ifsetup_out, ENXIO);
	}

	/* Get interface ifnet structure for this Ether device */
	ifnetp = lldev->ifnetp = if_alloc(IFT_ETHER);
	if(ifnetp == NULL) {
	    xge_resources_free(dev, xge_free_terminate_hal_device);
	    XGE_EXIT_ON_ERR("Allocation ifnet failed", ifsetup_out, ENOMEM);
	}

	/* Initialize interface ifnet structure */
	if_initname(ifnetp, device_get_name(dev), device_get_unit(dev));
	ifnetp->if_mtu      = XGE_HAL_DEFAULT_MTU;
	ifnetp->if_baudrate = XGE_BAUDRATE;
	ifnetp->if_init     = xge_init;
	ifnetp->if_softc    = lldev;
	ifnetp->if_flags    = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifnetp->if_ioctl    = xge_ioctl;
	ifnetp->if_start    = xge_send;

	/* TODO: Check and assign optimal value */
	ifnetp->if_snd.ifq_maxlen = ifqmaxlen;

	ifnetp->if_capabilities = IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU |
	    IFCAP_HWCSUM;
	if(lldev->enabled_tso)
	    ifnetp->if_capabilities |= IFCAP_TSO4;
	if(lldev->enabled_lro)
	    ifnetp->if_capabilities |= IFCAP_LRO;

	ifnetp->if_capenable = ifnetp->if_capabilities;

	/* Attach the interface */
	ether_ifattach(ifnetp, mcaddr);

ifsetup_out:
	return status;
}

/**
 * xge_callback_link_up
 * Callback for Link-up indication from HAL
 *
 * @userdata Per-adapter data
 */
void
xge_callback_link_up(void *userdata)
{
	xge_lldev_t  *lldev  = (xge_lldev_t *)userdata;
	struct ifnet *ifnetp = lldev->ifnetp;

	ifnetp->if_flags  &= ~IFF_DRV_OACTIVE;
	if_link_state_change(ifnetp, LINK_STATE_UP);
}

/**
 * xge_callback_link_down
 * Callback for Link-down indication from HAL
 *
 * @userdata Per-adapter data
 */
void
xge_callback_link_down(void *userdata)
{
	xge_lldev_t  *lldev  = (xge_lldev_t *)userdata;
	struct ifnet *ifnetp = lldev->ifnetp;

	ifnetp->if_flags  |= IFF_DRV_OACTIVE;
	if_link_state_change(ifnetp, LINK_STATE_DOWN);
}

/**
 * xge_callback_crit_err
 * Callback for Critical error indication from HAL
 *
 * @userdata Per-adapter data
 * @type Event type (Enumerated hardware error)
 * @serr_data Hardware status
 */
void
xge_callback_crit_err(void *userdata, xge_hal_event_e type, u64 serr_data)
{
	xge_trace(XGE_ERR, "Critical Error");
	xge_reset(userdata);
}

/**
 * xge_callback_event
 * Callback from HAL indicating that some event has been queued
 *
 * @item Queued event item
 */
void
xge_callback_event(xge_queue_item_t *item)
{
	xge_lldev_t      *lldev  = NULL;
	xge_hal_device_t *hldev  = NULL;
	struct ifnet     *ifnetp = NULL;

	hldev  = item->context;
	lldev  = xge_hal_device_private(hldev);
	ifnetp = lldev->ifnetp;

	switch((int)item->event_type) {
	    case XGE_LL_EVENT_TRY_XMIT_AGAIN:
	        if(lldev->initialized) {
	            if(xge_hal_channel_dtr_count(lldev->fifo_channel[0]) > 0) {
	                ifnetp->if_flags  &= ~IFF_DRV_OACTIVE;
	            }
	            else {
	                xge_queue_produce_context(
	                    xge_hal_device_queue(lldev->devh),
	                    XGE_LL_EVENT_TRY_XMIT_AGAIN, lldev->devh);
	            }
	        }
	        break;

	    case XGE_LL_EVENT_DEVICE_RESETTING:
	        xge_reset(item->context);
	        break;

	    default:
	        break;
	}
}

/**
 * xge_ifmedia_change
 * Media change driver callback
 *
 * @ifnetp Interface Handle
 *
 * Returns 0 if media is Ether else EINVAL
 */
int
xge_ifmedia_change(struct ifnet *ifnetp)
{
	xge_lldev_t    *lldev    = ifnetp->if_softc;
	struct ifmedia *ifmediap = &lldev->media;

	return (IFM_TYPE(ifmediap->ifm_media) != IFM_ETHER) ?  EINVAL:0;
}

/**
 * xge_ifmedia_status
 * Media status driver callback
 *
 * @ifnetp Interface Handle
 * @ifmr Interface Media Settings
 */
void
xge_ifmedia_status(struct ifnet *ifnetp, struct ifmediareq *ifmr)
{
	xge_hal_status_e status;
	u64              regvalue;
	xge_lldev_t      *lldev = ifnetp->if_softc;
	xge_hal_device_t *hldev = lldev->devh;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	status = xge_hal_mgmt_reg_read(hldev, 0,
	    xge_offsetof(xge_hal_pci_bar0_t, adapter_status), &regvalue);
	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_TRACE, "Getting adapter status failed");
	    goto _exit;
	}

	if((regvalue & (XGE_HAL_ADAPTER_STATUS_RMAC_REMOTE_FAULT |
	    XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)) == 0) {
	    ifmr->ifm_status |= IFM_ACTIVE;
	    ifmr->ifm_active |= IFM_10G_SR | IFM_FDX;
	    if_link_state_change(ifnetp, LINK_STATE_UP);
	}
	else {
	    if_link_state_change(ifnetp, LINK_STATE_DOWN);
	}
_exit:
	return;
}

/**
 * xge_ioctl_stats
 * IOCTL to get statistics
 *
 * @lldev Per-adapter data
 * @ifreqp Interface request
 */
int
xge_ioctl_stats(xge_lldev_t *lldev, struct ifreq *ifreqp)
{
	xge_hal_status_e status = XGE_HAL_OK;
	char *data = (char *)ifreqp->ifr_data;
	void *info = NULL;
	int retValue = EINVAL;

	switch(*data) {
	    case XGE_QUERY_STATS:
	        mtx_lock(&lldev->mtx_drv);
	        status = xge_hal_stats_hw(lldev->devh,
	            (xge_hal_stats_hw_info_t **)&info);
	        mtx_unlock(&lldev->mtx_drv);
	        if(status == XGE_HAL_OK) {
	            if(copyout(info, ifreqp->ifr_data,
	                sizeof(xge_hal_stats_hw_info_t)) == 0)
	                retValue = 0;
	        }
	        else {
	            xge_trace(XGE_ERR, "Getting statistics failed (Status: %d)",
	                status);
	        }
	        break;

	    case XGE_QUERY_PCICONF:
	        info = xge_os_malloc(NULL, sizeof(xge_hal_pci_config_t));
	        if(info != NULL) {
	            mtx_lock(&lldev->mtx_drv);
	            status = xge_hal_mgmt_pci_config(lldev->devh, info,
	                sizeof(xge_hal_pci_config_t));
	            mtx_unlock(&lldev->mtx_drv);
	            if(status == XGE_HAL_OK) {
	                if(copyout(info, ifreqp->ifr_data,
	                    sizeof(xge_hal_pci_config_t)) == 0)
	                    retValue = 0;
	            }
	            else {
	                xge_trace(XGE_ERR,
	                    "Getting PCI configuration failed (%d)", status);
	            }
	            xge_os_free(NULL, info, sizeof(xge_hal_pci_config_t));
	        }
	        break;

	    case XGE_QUERY_DEVSTATS:
	        info = xge_os_malloc(NULL, sizeof(xge_hal_stats_device_info_t));
	        if(info != NULL) {
	            mtx_lock(&lldev->mtx_drv);
	            status =xge_hal_mgmt_device_stats(lldev->devh, info,
	                sizeof(xge_hal_stats_device_info_t));
	            mtx_unlock(&lldev->mtx_drv);
	            if(status == XGE_HAL_OK) {
	                if(copyout(info, ifreqp->ifr_data,
	                    sizeof(xge_hal_stats_device_info_t)) == 0)
	                    retValue = 0;
	            }
	            else {
	                xge_trace(XGE_ERR, "Getting device info failed (%d)",
	                    status);
	            }
	            xge_os_free(NULL, info,
	                sizeof(xge_hal_stats_device_info_t));
	        }
	        break;

	    case XGE_QUERY_SWSTATS:
	        info = xge_os_malloc(NULL, sizeof(xge_hal_stats_sw_err_t));
	        if(info != NULL) {
	            mtx_lock(&lldev->mtx_drv);
	            status =xge_hal_mgmt_sw_stats(lldev->devh, info,
	                sizeof(xge_hal_stats_sw_err_t));
	            mtx_unlock(&lldev->mtx_drv);
	            if(status == XGE_HAL_OK) {
	                if(copyout(info, ifreqp->ifr_data,
	                    sizeof(xge_hal_stats_sw_err_t)) == 0)
	                    retValue = 0;
	            }
	            else {
	                xge_trace(XGE_ERR,
	                    "Getting tcode statistics failed (%d)", status);
	            }
	            xge_os_free(NULL, info, sizeof(xge_hal_stats_sw_err_t));
	        }
	        break;

	    case XGE_QUERY_DRIVERSTATS:
		if(copyout(&lldev->driver_stats, ifreqp->ifr_data,
	            sizeof(xge_driver_stats_t)) == 0) {
	            retValue = 0;
	        }
	        else {
	            xge_trace(XGE_ERR,
	                "Copyout of driver statistics failed (%d)", status);
	        }
	        break;

	    case XGE_READ_VERSION:
	        info = xge_os_malloc(NULL, XGE_BUFFER_SIZE);
	        if(version != NULL) {
	            strcpy(info, XGE_DRIVER_VERSION);
	            if(copyout(info, ifreqp->ifr_data, XGE_BUFFER_SIZE) == 0)
	                retValue = 0;
	            xge_os_free(NULL, info, XGE_BUFFER_SIZE);
	        }
	        break;

	    case XGE_QUERY_DEVCONF:
	        info = xge_os_malloc(NULL, sizeof(xge_hal_device_config_t));
	        if(info != NULL) {
	            mtx_lock(&lldev->mtx_drv);
	            status = xge_hal_mgmt_device_config(lldev->devh, info,
	                sizeof(xge_hal_device_config_t));
	            mtx_unlock(&lldev->mtx_drv);
	            if(status == XGE_HAL_OK) {
	                if(copyout(info, ifreqp->ifr_data,
	                    sizeof(xge_hal_device_config_t)) == 0)
	                    retValue = 0;
	            }
	            else {
	                xge_trace(XGE_ERR, "Getting devconfig failed (%d)",
	                    status);
	            }
	            xge_os_free(NULL, info, sizeof(xge_hal_device_config_t));
	        }
	        break;

	    case XGE_QUERY_BUFFER_MODE:
	        if(copyout(&lldev->buffer_mode, ifreqp->ifr_data,
	            sizeof(int)) == 0)
	            retValue = 0;
	        break;

	    case XGE_SET_BUFFER_MODE_1:
	    case XGE_SET_BUFFER_MODE_2:
	    case XGE_SET_BUFFER_MODE_5:
	        *data = (*data == XGE_SET_BUFFER_MODE_1) ? 'Y':'N';
	        if(copyout(data, ifreqp->ifr_data, sizeof(data)) == 0)
	            retValue = 0;
	        break;
	    default:
	        xge_trace(XGE_TRACE, "Nothing is matching");
	        retValue = ENOTTY;
	        break;
	}
	return retValue;
}

/**
 * xge_ioctl_registers
 * IOCTL to get registers
 *
 * @lldev Per-adapter data
 * @ifreqp Interface request
 */
int
xge_ioctl_registers(xge_lldev_t *lldev, struct ifreq *ifreqp)
{
	xge_register_t *data = (xge_register_t *)ifreqp->ifr_data;
	xge_hal_status_e status = XGE_HAL_OK;
	int retValue = EINVAL, offset = 0, index = 0;
	u64 val64 = 0;

	/* Reading a register */
	if(strcmp(data->option, "-r") == 0) {
	    data->value = 0x0000;
	    mtx_lock(&lldev->mtx_drv);
	    status = xge_hal_mgmt_reg_read(lldev->devh, 0, data->offset,
	        &data->value);
	    mtx_unlock(&lldev->mtx_drv);
	    if(status == XGE_HAL_OK) {
	        if(copyout(data, ifreqp->ifr_data, sizeof(xge_register_t)) == 0)
	            retValue = 0;
	    }
	}
	/* Writing to a register */
	else if(strcmp(data->option, "-w") == 0) {
	    mtx_lock(&lldev->mtx_drv);
	    status = xge_hal_mgmt_reg_write(lldev->devh, 0, data->offset,
	        data->value);
	    if(status == XGE_HAL_OK) {
	        val64 = 0x0000;
	        status = xge_hal_mgmt_reg_read(lldev->devh, 0, data->offset,
	            &val64);
	        if(status != XGE_HAL_OK) {
	            xge_trace(XGE_ERR, "Reading back updated register failed");
	        }
	        else {
	            if(val64 != data->value) {
	                xge_trace(XGE_ERR,
	                    "Read and written register values mismatched");
	            }
	            else retValue = 0;
	        }
	    }
	    else {
	        xge_trace(XGE_ERR, "Getting register value failed");
	    }
	    mtx_unlock(&lldev->mtx_drv);
	}
	else {
	    mtx_lock(&lldev->mtx_drv);
	    for(index = 0, offset = 0; offset <= XGE_OFFSET_OF_LAST_REG;
	        index++, offset += 0x0008) {
	        val64 = 0;
	        status = xge_hal_mgmt_reg_read(lldev->devh, 0, offset, &val64);
	        if(status != XGE_HAL_OK) {
	            xge_trace(XGE_ERR, "Getting register value failed");
	            break;
	        }
	        *((u64 *)((u64 *)data + index)) = val64;
	        retValue = 0;
	    }
	    mtx_unlock(&lldev->mtx_drv);

	    if(retValue == 0) {
	        if(copyout(data, ifreqp->ifr_data,
	            sizeof(xge_hal_pci_bar0_t)) != 0) {
	            xge_trace(XGE_ERR, "Copyout of register values failed");
	            retValue = EINVAL;
	        }
	    }
	    else {
	        xge_trace(XGE_ERR, "Getting register values failed");
	    }
	}
	return retValue;
}

/**
 * xge_ioctl
 * Callback to control the device - Interface configuration
 *
 * @ifnetp Interface Handle
 * @command Device control command
 * @data Parameters associated with command (if any)
 */
int
xge_ioctl(struct ifnet *ifnetp, unsigned long command, caddr_t data)
{
	struct ifreq   *ifreqp   = (struct ifreq *)data;
	xge_lldev_t    *lldev    = ifnetp->if_softc;
	struct ifmedia *ifmediap = &lldev->media;
	int             retValue = 0, mask = 0;

	if(lldev->in_detach) {
	    return retValue;
	}

	switch(command) {
	    /* Set/Get ifnet address */
	    case SIOCSIFADDR:
	    case SIOCGIFADDR:
	        ether_ioctl(ifnetp, command, data);
	        break;

	    /* Set ifnet MTU */
	    case SIOCSIFMTU:
	        retValue = xge_change_mtu(lldev, ifreqp->ifr_mtu);
	        break;

	    /* Set ifnet flags */
	    case SIOCSIFFLAGS:
	        if(ifnetp->if_flags & IFF_UP) {
	            /* Link status is UP */
	            if(!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)) {
	                xge_init(lldev);
	            }
	            xge_disable_promisc(lldev);
	            xge_enable_promisc(lldev);
	        }
	        else {
	            /* Link status is DOWN */
	            /* If device is in running, make it down */
	            if(ifnetp->if_drv_flags & IFF_DRV_RUNNING) {
	                xge_stop(lldev);
	            }
	        }
	        break;

	    /* Add/delete multicast address */
	    case SIOCADDMULTI:
	    case SIOCDELMULTI:
	        if(ifnetp->if_drv_flags & IFF_DRV_RUNNING) {
	            xge_setmulti(lldev);
	        }
	        break;

	    /* Set/Get net media */
	    case SIOCSIFMEDIA:
	    case SIOCGIFMEDIA:
	        retValue = ifmedia_ioctl(ifnetp, ifreqp, ifmediap, command);
	        break;

	    /* Set capabilities */
	    case SIOCSIFCAP:
	        mtx_lock(&lldev->mtx_drv);
	        mask = ifreqp->ifr_reqcap ^ ifnetp->if_capenable;
	        if(mask & IFCAP_TXCSUM) {
	            if(ifnetp->if_capenable & IFCAP_TXCSUM) {
	                ifnetp->if_capenable &= ~(IFCAP_TSO4 | IFCAP_TXCSUM);
	                ifnetp->if_hwassist &=
	                    ~(CSUM_TCP | CSUM_UDP | CSUM_TSO);
	            }
	            else {
	                ifnetp->if_capenable |= IFCAP_TXCSUM;
	                ifnetp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
	            }
	        }
	        if(mask & IFCAP_TSO4) {
	            if(ifnetp->if_capenable & IFCAP_TSO4) {
	                ifnetp->if_capenable &= ~IFCAP_TSO4;
	                ifnetp->if_hwassist  &= ~CSUM_TSO;

	                xge_os_printf("%s: TSO Disabled",
	                    device_get_nameunit(lldev->device));
	            }
	            else if(ifnetp->if_capenable & IFCAP_TXCSUM) {
	                ifnetp->if_capenable |= IFCAP_TSO4;
	                ifnetp->if_hwassist  |= CSUM_TSO;

	                xge_os_printf("%s: TSO Enabled",
	                    device_get_nameunit(lldev->device));
	            }
	        }

	        mtx_unlock(&lldev->mtx_drv);
	        break;

	    /* Custom IOCTL 0 */
	    case SIOCGPRIVATE_0:
	        retValue = xge_ioctl_stats(lldev, ifreqp);
	        break;

	    /* Custom IOCTL 1 */
	    case SIOCGPRIVATE_1:
	        retValue = xge_ioctl_registers(lldev, ifreqp);
	        break;

	    default:
	        retValue = EINVAL;
	        break;
	}
	return retValue;
}

/**
 * xge_init
 * Initialize the interface
 *
 * @plldev Per-adapter Data
 */
void
xge_init(void *plldev)
{
	xge_lldev_t *lldev = (xge_lldev_t *)plldev;

	mtx_lock(&lldev->mtx_drv);
	xge_os_memzero(&lldev->driver_stats, sizeof(xge_driver_stats_t));
	xge_device_init(lldev, XGE_HAL_CHANNEL_OC_NORMAL);
	mtx_unlock(&lldev->mtx_drv);
}

/**
 * xge_device_init
 * Initialize the interface (called by holding lock)
 *
 * @pdevin Per-adapter Data
 */
void
xge_device_init(xge_lldev_t *lldev, xge_hal_channel_reopen_e option)
{
	struct ifnet     *ifnetp = lldev->ifnetp;
	xge_hal_device_t *hldev  = lldev->devh;
	struct ifaddr      *ifaddrp;
	unsigned char      *macaddr;
	struct sockaddr_dl *sockaddrp;
	int                 status   = XGE_HAL_OK;

	mtx_assert((&lldev->mtx_drv), MA_OWNED);

	/* If device is in running state, initializing is not required */
	if(ifnetp->if_drv_flags & IFF_DRV_RUNNING)
	    return;

	/* Initializing timer */
	callout_init(&lldev->timer, 1);

	xge_trace(XGE_TRACE, "Set MTU size");
	status = xge_hal_device_mtu_set(hldev, ifnetp->if_mtu);
	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Setting MTU in HAL device failed");
	    goto _exit;
	}

	/* Enable HAL device */
	xge_hal_device_enable(hldev);

	/* Get MAC address and update in HAL */
	ifaddrp             = ifnetp->if_addr;
	sockaddrp           = (struct sockaddr_dl *)ifaddrp->ifa_addr;
	sockaddrp->sdl_type = IFT_ETHER;
	sockaddrp->sdl_alen = ifnetp->if_addrlen;
	macaddr             = LLADDR(sockaddrp);
	xge_trace(XGE_TRACE,
	    "Setting MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
	    *macaddr, *(macaddr + 1), *(macaddr + 2), *(macaddr + 3),
	    *(macaddr + 4), *(macaddr + 5));
	status = xge_hal_device_macaddr_set(hldev, 0, macaddr);
	if(status != XGE_HAL_OK)
	    xge_trace(XGE_ERR, "Setting MAC address failed (%d)", status);

	/* Opening channels */
	mtx_unlock(&lldev->mtx_drv);
	status = xge_channel_open(lldev, option);
	mtx_lock(&lldev->mtx_drv);
	if(status != XGE_HAL_OK)
	    goto _exit;

	/* Set appropriate flags */
	ifnetp->if_drv_flags  |=  IFF_DRV_RUNNING;
	ifnetp->if_flags &= ~IFF_DRV_OACTIVE;

	/* Checksum capability */
	ifnetp->if_hwassist = (ifnetp->if_capenable & IFCAP_TXCSUM) ?
	    (CSUM_TCP | CSUM_UDP) : 0;

	if((lldev->enabled_tso) && (ifnetp->if_capenable & IFCAP_TSO4))
	    ifnetp->if_hwassist |= CSUM_TSO;

	/* Enable interrupts */
	xge_hal_device_intr_enable(hldev);

	callout_reset(&lldev->timer, 10*hz, xge_timer, lldev);

	/* Disable promiscuous mode */
	xge_trace(XGE_TRACE, "If opted, enable promiscuous mode");
	xge_enable_promisc(lldev);

	/* Device is initialized */
	lldev->initialized = 1;
	xge_os_mdelay(1000);

_exit:
	return;
}

/**
 * xge_timer
 * Timer timeout function to handle link status
 *
 * @devp Per-adapter Data
 */
void
xge_timer(void *devp)
{
	xge_lldev_t      *lldev = (xge_lldev_t *)devp;
	xge_hal_device_t *hldev = lldev->devh;

	/* Poll for changes */
	xge_hal_device_poll(hldev);

	/* Reset timer */
	callout_reset(&lldev->timer, hz, xge_timer, lldev);

	return;
}

/**
 * xge_stop
 * De-activate the interface
 *
 * @lldev Per-adater Data
 */
void
xge_stop(xge_lldev_t *lldev)
{
	mtx_lock(&lldev->mtx_drv);
	xge_device_stop(lldev, XGE_HAL_CHANNEL_OC_NORMAL);
	mtx_unlock(&lldev->mtx_drv);
}

/**
 * xge_isr_filter
 * ISR filter function - to filter interrupts from other devices (shared)
 *
 * @handle Per-adapter Data
 *
 * Returns
 * FILTER_STRAY if interrupt is from other device
 * FILTER_SCHEDULE_THREAD if interrupt is from Xframe device
 */
int
xge_isr_filter(void *handle)
{
	xge_lldev_t *lldev       = (xge_lldev_t *)handle;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)((lldev->devh)->bar0);
	u16 retValue = FILTER_STRAY;
	u64 val64    = 0;

	XGE_DRV_STATS(isr_filter);

	val64 = xge_os_pio_mem_read64(lldev->pdev, (lldev->devh)->regh0,
	    &bar0->general_int_status);
	retValue = (!val64) ? FILTER_STRAY : FILTER_SCHEDULE_THREAD;

	return retValue;
}

/**
 * xge_isr_line
 * Interrupt service routine for Line interrupts
 *
 * @plldev Per-adapter Data
 */
void
xge_isr_line(void *plldev)
{
	xge_hal_status_e status;
	xge_lldev_t      *lldev   = (xge_lldev_t *)plldev;
	xge_hal_device_t *hldev   = (xge_hal_device_t *)lldev->devh;
	struct ifnet     *ifnetp  = lldev->ifnetp;

	XGE_DRV_STATS(isr_line);

	if(ifnetp->if_drv_flags & IFF_DRV_RUNNING) {
	    status = xge_hal_device_handle_irq(hldev);
	    if(!(IFQ_DRV_IS_EMPTY(&ifnetp->if_snd)))
	        xge_send(ifnetp);
	}
}

/*
 * xge_isr_msi
 * ISR for Message signaled interrupts
 */
void
xge_isr_msi(void *plldev)
{
	xge_lldev_t *lldev = (xge_lldev_t *)plldev;
	XGE_DRV_STATS(isr_msi);
	xge_hal_device_continue_irq(lldev->devh);
}

/**
 * xge_rx_open
 * Initiate and open all Rx channels
 *
 * @qid Ring Index
 * @lldev Per-adapter Data
 * @rflag Channel open/close/reopen flag
 *
 * Returns 0 or Error Number
 */
int
xge_rx_open(int qid, xge_lldev_t *lldev, xge_hal_channel_reopen_e rflag)
{
	u64 adapter_status = 0x0;
	xge_hal_status_e status = XGE_HAL_FAIL;

	xge_hal_channel_attr_t attr = {
	    .post_qid      = qid,
	    .compl_qid     = 0,
	    .callback      = xge_rx_compl,
	    .per_dtr_space = sizeof(xge_rx_priv_t),
	    .flags         = 0,
	    .type          = XGE_HAL_CHANNEL_TYPE_RING,
	    .userdata      = lldev,
	    .dtr_init      = xge_rx_initial_replenish,
	    .dtr_term      = xge_rx_term
	};

	/* If device is not ready, return */
	status = xge_hal_device_status(lldev->devh, &adapter_status);
	if(status != XGE_HAL_OK) {
	    xge_os_printf("Adapter Status: 0x%llx", (long long) adapter_status);
	    XGE_EXIT_ON_ERR("Device is not ready", _exit, XGE_HAL_FAIL);
	}
	else {
	    status = xge_hal_channel_open(lldev->devh, &attr,
	        &lldev->ring_channel[qid], rflag);
	}

_exit:
	return status;
}

/**
 * xge_tx_open
 * Initialize and open all Tx channels
 *
 * @lldev Per-adapter Data
 * @tflag Channel open/close/reopen flag
 *
 * Returns 0 or Error Number
 */
int
xge_tx_open(xge_lldev_t *lldev, xge_hal_channel_reopen_e tflag)
{
	xge_hal_status_e status = XGE_HAL_FAIL;
	u64 adapter_status = 0x0;
	int qindex, index;

	xge_hal_channel_attr_t attr = {
	    .compl_qid     = 0,
	    .callback      = xge_tx_compl,
	    .per_dtr_space = sizeof(xge_tx_priv_t),
	    .flags         = 0,
	    .type          = XGE_HAL_CHANNEL_TYPE_FIFO,
	    .userdata      = lldev,
	    .dtr_init      = xge_tx_initial_replenish,
	    .dtr_term      = xge_tx_term
	};

	/* If device is not ready, return */
	status = xge_hal_device_status(lldev->devh, &adapter_status);
	if(status != XGE_HAL_OK) {
	    xge_os_printf("Adapter Status: 0x%llx", (long long) adapter_status);
	    XGE_EXIT_ON_ERR("Device is not ready", _exit, XGE_HAL_FAIL);
	}

	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++) {
	    attr.post_qid = qindex,
	    status = xge_hal_channel_open(lldev->devh, &attr,
	        &lldev->fifo_channel[qindex], tflag);
	    if(status != XGE_HAL_OK) {
	        for(index = 0; index < qindex; index++)
	            xge_hal_channel_close(lldev->fifo_channel[index], tflag);
	    }
	}

_exit:
	return status;
}

/**
 * xge_enable_msi
 * Enables MSI
 *
 * @lldev Per-adapter Data
 */
void
xge_enable_msi(xge_lldev_t *lldev)
{
	xge_list_t        *item    = NULL;
	xge_hal_device_t  *hldev   = lldev->devh;
	xge_hal_channel_t *channel = NULL;
	u16 offset = 0, val16 = 0;

	xge_os_pci_read16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_control), &val16);

	/* Update msi_data */
	offset = (val16 & 0x80) ? 0x4c : 0x48;
	xge_os_pci_read16(lldev->pdev, NULL, offset, &val16);
	if(val16 & 0x1)
	    val16 &= 0xfffe;
	else
	    val16 |= 0x1;
	xge_os_pci_write16(lldev->pdev, NULL, offset, val16);

	/* Update msi_control */
	xge_os_pci_read16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_control), &val16);
	val16 |= 0x10;
	xge_os_pci_write16(lldev->pdev, NULL,
	    xge_offsetof(xge_hal_pci_config_le_t, msi_control), val16);

	/* Set TxMAT and RxMAT registers with MSI */
	xge_list_for_each(item, &hldev->free_channels) {
	    channel = xge_container_of(item, xge_hal_channel_t, item);
	    xge_hal_channel_msi_set(channel, 1, (u32)val16);
	}
}

/**
 * xge_channel_open
 * Open both Tx and Rx channels
 *
 * @lldev Per-adapter Data
 * @option Channel reopen option
 */
int
xge_channel_open(xge_lldev_t *lldev, xge_hal_channel_reopen_e option)
{
	xge_lro_entry_t *lro_session = NULL;
	xge_hal_status_e status   = XGE_HAL_OK;
	int index = 0, index2 = 0;

	if(lldev->enabled_msi == XGE_HAL_INTR_MODE_MSI) {
	    xge_msi_info_restore(lldev);
	    xge_enable_msi(lldev);
	}

_exit2:
	status = xge_create_dma_tags(lldev->device);
	if(status != XGE_HAL_OK)
	    XGE_EXIT_ON_ERR("DMA tag creation failed", _exit, status);

	/* Open ring (Rx) channel */
	for(index = 0; index < XGE_RING_COUNT; index++) {
	    status = xge_rx_open(index, lldev, option);
	    if(status != XGE_HAL_OK) {
	        /*
	         * DMA mapping fails in the unpatched Kernel which can't
	         * allocate contiguous memory for Jumbo frames.
	         * Try using 5 buffer mode.
	         */
	        if((lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) &&
	            (((lldev->ifnetp)->if_mtu + XGE_HAL_MAC_HEADER_MAX_SIZE) >
	            MJUMPAGESIZE)) {
	            /* Close so far opened channels */
	            for(index2 = 0; index2 < index; index2++) {
	                xge_hal_channel_close(lldev->ring_channel[index2],
	                    option);
	            }

	            /* Destroy DMA tags intended to use for 1 buffer mode */
	            if(bus_dmamap_destroy(lldev->dma_tag_rx,
	                lldev->extra_dma_map)) {
	                xge_trace(XGE_ERR, "Rx extra DMA map destroy failed");
	            }
	            if(bus_dma_tag_destroy(lldev->dma_tag_rx))
	                xge_trace(XGE_ERR, "Rx DMA tag destroy failed");
	            if(bus_dma_tag_destroy(lldev->dma_tag_tx))
	                xge_trace(XGE_ERR, "Tx DMA tag destroy failed");

	            /* Switch to 5 buffer mode */
	            lldev->buffer_mode = XGE_HAL_RING_QUEUE_BUFFER_MODE_5;
	            xge_buffer_mode_init(lldev, (lldev->ifnetp)->if_mtu);

	            /* Restart init */
	            goto _exit2;
	        }
	        else {
	            XGE_EXIT_ON_ERR("Opening Rx channel failed", _exit1,
	                status);
	        }
	    }
	}

	if(lldev->enabled_lro) {
	    SLIST_INIT(&lldev->lro_free);
	    SLIST_INIT(&lldev->lro_active);
	    lldev->lro_num = XGE_LRO_DEFAULT_ENTRIES;

	    for(index = 0; index < lldev->lro_num; index++) {
	        lro_session = (xge_lro_entry_t *)
	            xge_os_malloc(NULL, sizeof(xge_lro_entry_t));
	        if(lro_session == NULL) {
	            lldev->lro_num = index;
	            break;
	        }
	        SLIST_INSERT_HEAD(&lldev->lro_free, lro_session, next);
	    }
	}

	/* Open FIFO (Tx) channel */
	status = xge_tx_open(lldev, option);
	if(status != XGE_HAL_OK)
	    XGE_EXIT_ON_ERR("Opening Tx channel failed", _exit1, status);

	goto _exit;

_exit1:
	/*
	 * Opening Rx channel(s) failed (index is <last ring index - 1>) or
	 * Initialization of LRO failed (index is XGE_RING_COUNT)
	 * Opening Tx channel failed    (index is XGE_RING_COUNT)
	 */
	for(index2 = 0; index2 < index; index2++)
	    xge_hal_channel_close(lldev->ring_channel[index2], option);

_exit:
	return status;
}

/**
 * xge_channel_close
 * Close both Tx and Rx channels
 *
 * @lldev Per-adapter Data
 * @option Channel reopen option
 *
 */
void
xge_channel_close(xge_lldev_t *lldev, xge_hal_channel_reopen_e option)
{
	int qindex = 0;

	DELAY(1000 * 1000);

	/* Close FIFO (Tx) channel */
	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++)
	    xge_hal_channel_close(lldev->fifo_channel[qindex], option);

	/* Close Ring (Rx) channels */
	for(qindex = 0; qindex < XGE_RING_COUNT; qindex++)
	    xge_hal_channel_close(lldev->ring_channel[qindex], option);

	if(bus_dmamap_destroy(lldev->dma_tag_rx, lldev->extra_dma_map))
	    xge_trace(XGE_ERR, "Rx extra map destroy failed");
	if(bus_dma_tag_destroy(lldev->dma_tag_rx))
	    xge_trace(XGE_ERR, "Rx DMA tag destroy failed");
	if(bus_dma_tag_destroy(lldev->dma_tag_tx))
	    xge_trace(XGE_ERR, "Tx DMA tag destroy failed");
}

/**
 * dmamap_cb
 * DMA map callback
 *
 * @arg Parameter passed from dmamap
 * @segs Segments
 * @nseg Number of segments
 * @error Error
 */
void
dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if(!error) {
	    *(bus_addr_t *) arg = segs->ds_addr;
	}
}

/**
 * xge_reset
 * Device Reset
 *
 * @lldev Per-adapter Data
 */
void
xge_reset(xge_lldev_t *lldev)
{
	xge_trace(XGE_TRACE, "Reseting the chip");

	/* If the device is not initialized, return */
	if(lldev->initialized) {
	    mtx_lock(&lldev->mtx_drv);
	    xge_device_stop(lldev, XGE_HAL_CHANNEL_OC_NORMAL);
	    xge_device_init(lldev, XGE_HAL_CHANNEL_OC_NORMAL);
	    mtx_unlock(&lldev->mtx_drv);
	}

	return;
}

/**
 * xge_setmulti
 * Set an address as a multicast address
 *
 * @lldev Per-adapter Data
 */
void
xge_setmulti(xge_lldev_t *lldev)
{
	struct ifmultiaddr *ifma;
	u8                 *lladdr;
	xge_hal_device_t   *hldev        = (xge_hal_device_t *)lldev->devh;
	struct ifnet       *ifnetp       = lldev->ifnetp;
	int                index         = 0;
	int                offset        = 1;
	int                table_size    = 47;
	xge_hal_status_e   status        = XGE_HAL_OK;
	u8                 initial_addr[]= {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

	if((ifnetp->if_flags & IFF_MULTICAST) && (!lldev->all_multicast)) {
	    status = xge_hal_device_mcast_enable(hldev);
	    lldev->all_multicast = 1;
	}
	else if((ifnetp->if_flags & IFF_MULTICAST) && (lldev->all_multicast)) {
	    status = xge_hal_device_mcast_disable(hldev);
	    lldev->all_multicast = 0;
	}

	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Enabling/disabling multicast failed");
	    goto _exit;
	}

	/* Updating address list */
	if_maddr_rlock(ifnetp);
	index = 0;
	TAILQ_FOREACH(ifma, &ifnetp->if_multiaddrs, ifma_link) {
	    if(ifma->ifma_addr->sa_family != AF_LINK) {
	        continue;
	    }
	    lladdr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	    index += 1;
	}
	if_maddr_runlock(ifnetp);

	if((!lldev->all_multicast) && (index)) {
	    lldev->macaddr_count = (index + 1);
	    if(lldev->macaddr_count > table_size) {
	        goto _exit;
	    }

	    /* Clear old addresses */
	    for(index = 0; index < 48; index++) {
	        xge_hal_device_macaddr_set(hldev, (offset + index),
	            initial_addr);
	    }
	}

	/* Add new addresses */
	if_maddr_rlock(ifnetp);
	index = 0;
	TAILQ_FOREACH(ifma, &ifnetp->if_multiaddrs, ifma_link) {
	    if(ifma->ifma_addr->sa_family != AF_LINK) {
	        continue;
	    }
	    lladdr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	    xge_hal_device_macaddr_set(hldev, (offset + index), lladdr);
	    index += 1;
	}
	if_maddr_runlock(ifnetp);

_exit:
	return;
}

/**
 * xge_enable_promisc
 * Enable Promiscuous Mode
 *
 * @lldev Per-adapter Data
 */
void
xge_enable_promisc(xge_lldev_t *lldev)
{
	struct ifnet *ifnetp = lldev->ifnetp;
	xge_hal_device_t *hldev = lldev->devh;
	xge_hal_pci_bar0_t *bar0 = NULL;
	u64 val64 = 0;

	bar0 = (xge_hal_pci_bar0_t *) hldev->bar0;

	if(ifnetp->if_flags & IFF_PROMISC) {
	    xge_hal_device_promisc_enable(lldev->devh);

	    /*
	     * When operating in promiscuous mode, don't strip the VLAN tag
	     */
	    val64 = xge_os_pio_mem_read64(lldev->pdev, hldev->regh0,
	        &bar0->rx_pa_cfg);
	    val64 &= ~XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(1);
	    val64 |= XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(0);
	    xge_os_pio_mem_write64(lldev->pdev, hldev->regh0, val64,
	        &bar0->rx_pa_cfg);

	    xge_trace(XGE_TRACE, "Promiscuous mode ON");
	}
}

/**
 * xge_disable_promisc
 * Disable Promiscuous Mode
 *
 * @lldev Per-adapter Data
 */
void
xge_disable_promisc(xge_lldev_t *lldev)
{
	xge_hal_device_t *hldev = lldev->devh;
	xge_hal_pci_bar0_t *bar0 = NULL;
	u64 val64 = 0;

	bar0 = (xge_hal_pci_bar0_t *) hldev->bar0;

	xge_hal_device_promisc_disable(lldev->devh);

	/*
	 * Strip VLAN tag when operating in non-promiscuous mode
	 */
	val64 = xge_os_pio_mem_read64(lldev->pdev, hldev->regh0,
	    &bar0->rx_pa_cfg);
	val64 &= ~XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(1);
	val64 |= XGE_HAL_RX_PA_CFG_STRIP_VLAN_TAG_MODE(1);
	xge_os_pio_mem_write64(lldev->pdev, hldev->regh0, val64,
	    &bar0->rx_pa_cfg);

	xge_trace(XGE_TRACE, "Promiscuous mode OFF");
}

/**
 * xge_change_mtu
 * Change interface MTU to a requested valid size
 *
 * @lldev Per-adapter Data
 * @NewMtu Requested MTU
 *
 * Returns 0 or Error Number
 */
int
xge_change_mtu(xge_lldev_t *lldev, int new_mtu)
{
	int status = XGE_HAL_OK;

	/* Check requested MTU size for boundary */
	if(xge_hal_device_mtu_check(lldev->devh, new_mtu) != XGE_HAL_OK) {
	    XGE_EXIT_ON_ERR("Invalid MTU", _exit, EINVAL);
	}

	lldev->mtu = new_mtu;
	xge_confirm_changes(lldev, XGE_SET_MTU);

_exit:
	return status;
}

/**
 * xge_device_stop
 *
 * Common code for both stop and part of reset. Disables device, interrupts and
 * closes channels
 *
 * @dev Device Handle
 * @option Channel normal/reset option
 */
void
xge_device_stop(xge_lldev_t *lldev, xge_hal_channel_reopen_e option)
{
	xge_hal_device_t *hldev  = lldev->devh;
	struct ifnet     *ifnetp = lldev->ifnetp;
	u64               val64  = 0;

	mtx_assert((&lldev->mtx_drv), MA_OWNED);

	/* If device is not in "Running" state, return */
	if (!(ifnetp->if_drv_flags & IFF_DRV_RUNNING))
	    goto _exit;

	/* Set appropriate flags */
	ifnetp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* Stop timer */
	callout_stop(&lldev->timer);

	/* Disable interrupts */
	xge_hal_device_intr_disable(hldev);

	mtx_unlock(&lldev->mtx_drv);
	xge_queue_flush(xge_hal_device_queue(lldev->devh));
	mtx_lock(&lldev->mtx_drv);

	/* Disable HAL device */
	if(xge_hal_device_disable(hldev) != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Disabling HAL device failed");
	    xge_hal_device_status(hldev, &val64);
	    xge_trace(XGE_ERR, "Adapter Status: 0x%llx", (long long)val64);
	}

	/* Close Tx and Rx channels */
	xge_channel_close(lldev, option);

	/* Reset HAL device */
	xge_hal_device_reset(hldev);

	xge_os_mdelay(1000);
	lldev->initialized = 0;

	if_link_state_change(ifnetp, LINK_STATE_DOWN);

_exit:
	return;
}

/**
 * xge_set_mbuf_cflags
 * set checksum flag for the mbuf
 *
 * @pkt Packet
 */
void
xge_set_mbuf_cflags(mbuf_t pkt)
{
	pkt->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
	pkt->m_pkthdr.csum_flags |= CSUM_IP_VALID;
	pkt->m_pkthdr.csum_flags |= (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
	pkt->m_pkthdr.csum_data = htons(0xffff);
}

/**
 * xge_lro_flush_sessions
 * Flush LRO session and send accumulated LRO packet to upper layer
 *
 * @lldev Per-adapter Data
 */
void
xge_lro_flush_sessions(xge_lldev_t *lldev)
{
	xge_lro_entry_t *lro_session = NULL;

	while(!SLIST_EMPTY(&lldev->lro_active)) {
	    lro_session = SLIST_FIRST(&lldev->lro_active);
	    SLIST_REMOVE_HEAD(&lldev->lro_active, next);
	    xge_lro_flush(lldev, lro_session);
	}
}

/**
 * xge_lro_flush
 * Flush LRO session. Send accumulated LRO packet to upper layer
 *
 * @lldev Per-adapter Data
 * @lro LRO session to be flushed
 */
static void
xge_lro_flush(xge_lldev_t *lldev, xge_lro_entry_t *lro_session)
{
	struct ip *header_ip;
	struct tcphdr *header_tcp;
	u32 *ptr;

	if(lro_session->append_cnt) {
	    header_ip = lro_session->lro_header_ip;
	    header_ip->ip_len = htons(lro_session->len - ETHER_HDR_LEN);
	    lro_session->m_head->m_pkthdr.len = lro_session->len;
	    header_tcp = (struct tcphdr *)(header_ip + 1);
	    header_tcp->th_ack = lro_session->ack_seq;
	    header_tcp->th_win = lro_session->window;
	    if(lro_session->timestamp) {
	        ptr = (u32 *)(header_tcp + 1);
	        ptr[1] = htonl(lro_session->tsval);
	        ptr[2] = lro_session->tsecr;
	    }
	}

	(*lldev->ifnetp->if_input)(lldev->ifnetp, lro_session->m_head);
	lro_session->m_head = NULL;
	lro_session->timestamp = 0;
	lro_session->append_cnt = 0;
	SLIST_INSERT_HEAD(&lldev->lro_free, lro_session, next);
}

/**
 * xge_lro_accumulate
 * Accumulate packets to form a large LRO packet based on various conditions
 *
 * @lldev Per-adapter Data
 * @m_head Current Packet
 *
 * Returns XGE_HAL_OK or XGE_HAL_FAIL (failure)
 */
static int
xge_lro_accumulate(xge_lldev_t *lldev, struct mbuf *m_head)
{
	struct ether_header *header_ethernet;
	struct ip *header_ip;
	struct tcphdr *header_tcp;
	u32 seq, *ptr;
	struct mbuf *buffer_next, *buffer_tail;
	xge_lro_entry_t *lro_session;
	xge_hal_status_e status = XGE_HAL_FAIL;
	int hlen, ip_len, tcp_hdr_len, tcp_data_len, tot_len, tcp_options;
	int trim;

	/* Get Ethernet header */
	header_ethernet = mtod(m_head, struct ether_header *);

	/* Return if it is not IP packet */
	if(header_ethernet->ether_type != htons(ETHERTYPE_IP))
	    goto _exit;

	/* Get IP header */
	header_ip = lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1 ?
	    (struct ip *)(header_ethernet + 1) :
	    mtod(m_head->m_next, struct ip *);

	/* Return if it is not TCP packet */
	if(header_ip->ip_p != IPPROTO_TCP)
	    goto _exit;

	/* Return if packet has options */
	if((header_ip->ip_hl << 2) != sizeof(*header_ip))
	    goto _exit;

	/* Return if packet is fragmented */
	if(header_ip->ip_off & htons(IP_MF | IP_OFFMASK))
	    goto _exit;

	/* Get TCP header */
	header_tcp = (struct tcphdr *)(header_ip + 1);

	/* Return if not ACK or PUSH */
	if((header_tcp->th_flags & ~(TH_ACK | TH_PUSH)) != 0)
	    goto _exit;

	/* Only timestamp option is handled */
	tcp_options = (header_tcp->th_off << 2) - sizeof(*header_tcp);
	tcp_hdr_len = sizeof(*header_tcp) + tcp_options;
	ptr = (u32 *)(header_tcp + 1);
	if(tcp_options != 0) {
	    if(__predict_false(tcp_options != TCPOLEN_TSTAMP_APPA) ||
	        (*ptr != ntohl(TCPOPT_NOP << 24 | TCPOPT_NOP << 16 |
	        TCPOPT_TIMESTAMP << 8 | TCPOLEN_TIMESTAMP))) {
	        goto _exit;
	    }
	}

	/* Total length of packet (IP) */
	ip_len = ntohs(header_ip->ip_len);

	/* TCP data size */
	tcp_data_len = ip_len - (header_tcp->th_off << 2) - sizeof(*header_ip);

	/* If the frame is padded, trim it */
	tot_len = m_head->m_pkthdr.len;
	trim = tot_len - (ip_len + ETHER_HDR_LEN);
	if(trim != 0) {
	    if(trim < 0)
	        goto _exit;
	    m_adj(m_head, -trim);
	    tot_len = m_head->m_pkthdr.len;
	}

	buffer_next = m_head;
	buffer_tail = NULL;
	while(buffer_next != NULL) {
	    buffer_tail = buffer_next;
	    buffer_next = buffer_tail->m_next;
	}

	/* Total size of only headers */
	hlen = ip_len + ETHER_HDR_LEN - tcp_data_len;

	/* Get sequence number */
	seq = ntohl(header_tcp->th_seq);

	SLIST_FOREACH(lro_session, &lldev->lro_active, next) {
	    if(lro_session->source_port == header_tcp->th_sport &&
	        lro_session->dest_port == header_tcp->th_dport &&
	        lro_session->source_ip == header_ip->ip_src.s_addr &&
	        lro_session->dest_ip == header_ip->ip_dst.s_addr) {

	        /* Unmatched sequence number, flush LRO session */
	        if(__predict_false(seq != lro_session->next_seq)) {
	            SLIST_REMOVE(&lldev->lro_active, lro_session,
	                xge_lro_entry_t, next);
	            xge_lro_flush(lldev, lro_session);
	            goto _exit;
	        }

	        /* Handle timestamp option */
	        if(tcp_options) {
	            u32 tsval = ntohl(*(ptr + 1));
	            if(__predict_false(lro_session->tsval > tsval ||
	                *(ptr + 2) == 0)) {
	                goto _exit;
	            }
	            lro_session->tsval = tsval;
	            lro_session->tsecr = *(ptr + 2);
	        }

	        lro_session->next_seq += tcp_data_len;
	        lro_session->ack_seq = header_tcp->th_ack;
	        lro_session->window = header_tcp->th_win;

	        /* If TCP data/payload is of 0 size, free mbuf */
	        if(tcp_data_len == 0) {
	            m_freem(m_head);
	            status = XGE_HAL_OK;
	            goto _exit;
	        }

	        lro_session->append_cnt++;
	        lro_session->len += tcp_data_len;

	        /* Adjust mbuf so that m_data points to payload than headers */
	        m_adj(m_head, hlen);

	        /* Append this packet to LRO accumulated packet */
	        lro_session->m_tail->m_next = m_head;
	        lro_session->m_tail = buffer_tail;

	        /* Flush if LRO packet is exceeding maximum size */
	        if(lro_session->len >
	            (XGE_HAL_LRO_DEFAULT_FRM_LEN - lldev->ifnetp->if_mtu)) {
	            SLIST_REMOVE(&lldev->lro_active, lro_session,
	                xge_lro_entry_t, next);
	            xge_lro_flush(lldev, lro_session);
	        }
	        status = XGE_HAL_OK;
	        goto _exit;
	    }
	}

	if(SLIST_EMPTY(&lldev->lro_free))
	    goto _exit;

	/* Start a new LRO session */
	lro_session = SLIST_FIRST(&lldev->lro_free);
	SLIST_REMOVE_HEAD(&lldev->lro_free, next);
	SLIST_INSERT_HEAD(&lldev->lro_active, lro_session, next);
	lro_session->source_port = header_tcp->th_sport;
	lro_session->dest_port = header_tcp->th_dport;
	lro_session->source_ip = header_ip->ip_src.s_addr;
	lro_session->dest_ip = header_ip->ip_dst.s_addr;
	lro_session->next_seq = seq + tcp_data_len;
	lro_session->mss = tcp_data_len;
	lro_session->ack_seq = header_tcp->th_ack;
	lro_session->window = header_tcp->th_win;

	lro_session->lro_header_ip = header_ip;

	/* Handle timestamp option */
	if(tcp_options) {
	    lro_session->timestamp = 1;
	    lro_session->tsval = ntohl(*(ptr + 1));
	    lro_session->tsecr = *(ptr + 2);
	}

	lro_session->len = tot_len;
	lro_session->m_head = m_head;
	lro_session->m_tail = buffer_tail;
	status = XGE_HAL_OK;

_exit:
	return status;
}

/**
 * xge_accumulate_large_rx
 * Accumulate packets to form a large LRO packet based on various conditions
 *
 * @lldev Per-adapter Data
 * @pkt Current packet
 * @pkt_length Packet Length
 * @rxd_priv Rx Descriptor Private Data
 */
void
xge_accumulate_large_rx(xge_lldev_t *lldev, struct mbuf *pkt, int pkt_length,
	xge_rx_priv_t *rxd_priv)
{
	if(xge_lro_accumulate(lldev, pkt) != XGE_HAL_OK) {
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);
	    (*lldev->ifnetp->if_input)(lldev->ifnetp, pkt);
	}
}

/**
 * xge_rx_compl
 * If the interrupt is due to received frame (Rx completion), send it up
 *
 * @channelh Ring Channel Handle
 * @dtr Current Descriptor
 * @t_code Transfer Code indicating success or error
 * @userdata Per-adapter Data
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
xge_hal_status_e
xge_rx_compl(xge_hal_channel_h channelh, xge_hal_dtr_h dtr, u8 t_code,
	void *userdata)
{
	struct ifnet       *ifnetp;
	xge_rx_priv_t      *rxd_priv = NULL;
	mbuf_t              mbuf_up  = NULL;
	xge_hal_status_e    status   = XGE_HAL_OK;
	xge_hal_dtr_info_t  ext_info;
	int                 index;
	u16                 vlan_tag;

	/*get the user data portion*/
	xge_lldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(!lldev) {
	    XGE_EXIT_ON_ERR("Failed to get user data", _exit, XGE_HAL_FAIL);
	}

	XGE_DRV_STATS(rx_completions);

	/* get the interface pointer */
	ifnetp = lldev->ifnetp;

	do {
	    XGE_DRV_STATS(rx_desc_compl);

	    if(!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)) {
	        status = XGE_HAL_FAIL;
	        goto _exit;
	    }

	    if(t_code) {
	        xge_trace(XGE_TRACE, "Packet dropped because of %d", t_code);
	        XGE_DRV_STATS(rx_tcode);
	        xge_hal_device_handle_tcode(channelh, dtr, t_code);
	        xge_hal_ring_dtr_post(channelh,dtr);
	        continue;
	    }

	    /* Get the private data for this descriptor*/
	    rxd_priv = (xge_rx_priv_t *) xge_hal_ring_dtr_private(channelh,
	        dtr);
	    if(!rxd_priv) {
	        XGE_EXIT_ON_ERR("Failed to get descriptor private data", _exit,
	            XGE_HAL_FAIL);
	    }

	    /*
	     * Prepare one buffer to send it to upper layer -- since the upper
	     * layer frees the buffer do not use rxd_priv->buffer. Meanwhile
	     * prepare a new buffer, do mapping, use it in the current
	     * descriptor and post descriptor back to ring channel
	     */
	    mbuf_up = rxd_priv->bufferArray[0];

	    /* Gets details of mbuf i.e., packet length */
	    xge_ring_dtr_get(mbuf_up, channelh, dtr, lldev, rxd_priv);

	    status =
	        (lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) ?
	        xge_get_buf(dtr, rxd_priv, lldev, 0) :
	        xge_get_buf_3b_5b(dtr, rxd_priv, lldev);

	    if(status != XGE_HAL_OK) {
	        xge_trace(XGE_ERR, "No memory");
	        XGE_DRV_STATS(rx_no_buf);

	        /*
	         * Unable to allocate buffer. Instead of discarding, post
	         * descriptor back to channel for future processing of same
	         * packet.
	         */
	        xge_hal_ring_dtr_post(channelh, dtr);
	        continue;
	    }

	    /* Get the extended information */
	    xge_hal_ring_dtr_info_get(channelh, dtr, &ext_info);

	    /*
	     * As we have allocated a new mbuf for this descriptor, post this
	     * descriptor with new mbuf back to ring channel
	     */
	    vlan_tag = ext_info.vlan;
	    xge_hal_ring_dtr_post(channelh, dtr);
	    if ((!(ext_info.proto & XGE_HAL_FRAME_PROTO_IP_FRAGMENTED) &&
	        (ext_info.proto & XGE_HAL_FRAME_PROTO_TCP_OR_UDP) &&
	        (ext_info.l3_cksum == XGE_HAL_L3_CKSUM_OK) &&
	        (ext_info.l4_cksum == XGE_HAL_L4_CKSUM_OK))) {

	        /* set Checksum Flag */
	        xge_set_mbuf_cflags(mbuf_up);

	        if(lldev->enabled_lro) {
	            xge_accumulate_large_rx(lldev, mbuf_up, mbuf_up->m_len,
	                rxd_priv);
	        }
	        else {
	            /* Post-Read sync for buffers*/
	            for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	                bus_dmamap_sync(lldev->dma_tag_rx,
	                    rxd_priv->dmainfo[0].dma_map, BUS_DMASYNC_POSTREAD);
	            }
	            (*ifnetp->if_input)(ifnetp, mbuf_up);
	        }
	    }
	    else {
	        /*
	         * Packet with erroneous checksum , let the upper layer deal
	         * with it
	         */

	        /* Post-Read sync for buffers*/
	        for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	            bus_dmamap_sync(lldev->dma_tag_rx,
	                 rxd_priv->dmainfo[0].dma_map, BUS_DMASYNC_POSTREAD);
	        }

	        if(vlan_tag) {
	            mbuf_up->m_pkthdr.ether_vtag = vlan_tag;
	            mbuf_up->m_flags |= M_VLANTAG;
	        }

	        if(lldev->enabled_lro)
	            xge_lro_flush_sessions(lldev);

	        (*ifnetp->if_input)(ifnetp, mbuf_up);
	    }
	} while(xge_hal_ring_dtr_next_completed(channelh, &dtr, &t_code)
	    == XGE_HAL_OK);

	if(lldev->enabled_lro)
	    xge_lro_flush_sessions(lldev);

_exit:
	return status;
}

/**
 * xge_ring_dtr_get
 * Get descriptors
 *
 * @mbuf_up Packet to send up
 * @channelh Ring Channel Handle
 * @dtr Descriptor
 * @lldev Per-adapter Data
 * @rxd_priv Rx Descriptor Private Data
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
int
xge_ring_dtr_get(mbuf_t mbuf_up, xge_hal_channel_h channelh, xge_hal_dtr_h dtr,
	xge_lldev_t *lldev, xge_rx_priv_t *rxd_priv)
{
	mbuf_t           m;
	int              pkt_length[5]={0,0}, pkt_len=0;
	dma_addr_t       dma_data[5];
	int              index;

	m = mbuf_up;
	pkt_len = 0;

	if(lldev->buffer_mode != XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    xge_os_memzero(pkt_length, sizeof(pkt_length));

	    /*
	     * Retrieve data of interest from the completed descriptor -- This
	     * returns the packet length
	     */
	    if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	        xge_hal_ring_dtr_5b_get(channelh, dtr, dma_data, pkt_length);
	    }
	    else {
	        xge_hal_ring_dtr_3b_get(channelh, dtr, dma_data, pkt_length);
	    }

	    for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	        m->m_len  = pkt_length[index];

	        if(index < (lldev->rxd_mbuf_cnt-1)) {
	            m->m_next = rxd_priv->bufferArray[index + 1];
	            m = m->m_next;
	        }
	        else {
	            m->m_next = NULL;
	        }
	        pkt_len+=pkt_length[index];
	    }

	    /*
	     * Since 2 buffer mode is an exceptional case where data is in 3rd
	     * buffer but not in 2nd buffer
	     */
	    if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_2) {
	        m->m_len = pkt_length[2];
	        pkt_len+=pkt_length[2];
	    }

	    /*
	     * Update length of newly created buffer to be sent up with packet
	     * length
	     */
	    mbuf_up->m_pkthdr.len = pkt_len;
	}
	else {
	    /*
	     * Retrieve data of interest from the completed descriptor -- This
	     * returns the packet length
	     */
	    xge_hal_ring_dtr_1b_get(channelh, dtr,&dma_data[0], &pkt_length[0]);

	    /*
	     * Update length of newly created buffer to be sent up with packet
	     * length
	     */
	    mbuf_up->m_len =  mbuf_up->m_pkthdr.len = pkt_length[0];
	}

	return XGE_HAL_OK;
}

/**
 * xge_flush_txds
 * Flush Tx descriptors
 *
 * @channelh Channel handle
 */
static void inline
xge_flush_txds(xge_hal_channel_h channelh)
{
	xge_lldev_t *lldev = xge_hal_channel_userdata(channelh);
	xge_hal_dtr_h tx_dtr;
	xge_tx_priv_t *tx_priv;
	u8 t_code;

	while(xge_hal_fifo_dtr_next_completed(channelh, &tx_dtr, &t_code)
	    == XGE_HAL_OK) {
	    XGE_DRV_STATS(tx_desc_compl);
	    if(t_code) {
	        xge_trace(XGE_TRACE, "Tx descriptor with t_code %d", t_code);
	        XGE_DRV_STATS(tx_tcode);
	        xge_hal_device_handle_tcode(channelh, tx_dtr, t_code);
	    }

	    tx_priv = xge_hal_fifo_dtr_private(tx_dtr);
	    bus_dmamap_unload(lldev->dma_tag_tx, tx_priv->dma_map);
	    m_freem(tx_priv->buffer);
	    tx_priv->buffer = NULL;
	    xge_hal_fifo_dtr_free(channelh, tx_dtr);
	}
}

/**
 * xge_send
 * Transmit function
 *
 * @ifnetp Interface Handle
 */
void
xge_send(struct ifnet *ifnetp)
{
	int qindex = 0;
	xge_lldev_t *lldev = ifnetp->if_softc;

	for(qindex = 0; qindex < XGE_FIFO_COUNT; qindex++) {
	    if(mtx_trylock(&lldev->mtx_tx[qindex]) == 0) {
	        XGE_DRV_STATS(tx_lock_fail);
	        break;
	    }
	    xge_send_locked(ifnetp, qindex);
	    mtx_unlock(&lldev->mtx_tx[qindex]);
	}
}

static void inline
xge_send_locked(struct ifnet *ifnetp, int qindex)
{
	xge_hal_dtr_h            dtr;
	static bus_dma_segment_t segs[XGE_MAX_SEGS];
	xge_hal_status_e         status;
	unsigned int             max_fragments;
	xge_lldev_t              *lldev          = ifnetp->if_softc;
	xge_hal_channel_h        channelh        = lldev->fifo_channel[qindex];
	mbuf_t                   m_head          = NULL;
	mbuf_t                   m_buf           = NULL;
	xge_tx_priv_t            *ll_tx_priv     = NULL;
	register unsigned int    count           = 0;
	unsigned int             nsegs           = 0;
	u16                      vlan_tag;

	max_fragments = ((xge_hal_fifo_t *)channelh)->config->max_frags;

	/* If device is not initialized, return */
	if((!lldev->initialized) || (!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)))
	    return;

	XGE_DRV_STATS(tx_calls);

	/*
	 * This loop will be executed for each packet in the kernel maintained
	 * queue -- each packet can be with fragments as an mbuf chain
	 */
	for(;;) {
	    IF_DEQUEUE(&ifnetp->if_snd, m_head);
	    if (m_head == NULL) {
		ifnetp->if_drv_flags &= ~(IFF_DRV_OACTIVE);
		return;
	    }

	    for(m_buf = m_head; m_buf != NULL; m_buf = m_buf->m_next) {
	        if(m_buf->m_len) count += 1;
	    }

	    if(count >= max_fragments) {
	        m_buf = m_defrag(m_head, M_NOWAIT);
	        if(m_buf != NULL) m_head = m_buf;
	        XGE_DRV_STATS(tx_defrag);
	    }

	    /* Reserve descriptors */
	    status = xge_hal_fifo_dtr_reserve(channelh, &dtr);
	    if(status != XGE_HAL_OK) {
	        XGE_DRV_STATS(tx_no_txd);
	        xge_flush_txds(channelh);
		break;
	    }

	    vlan_tag =
	        (m_head->m_flags & M_VLANTAG) ? m_head->m_pkthdr.ether_vtag : 0;
	    xge_hal_fifo_dtr_vlan_set(dtr, vlan_tag);

	    /* Update Tx private structure for this descriptor */
	    ll_tx_priv         = xge_hal_fifo_dtr_private(dtr);
	    ll_tx_priv->buffer = m_head;

	    /*
	     * Do mapping -- Required DMA tag has been created in xge_init
	     * function and DMA maps have already been created in the
	     * xgell_tx_replenish function.
	     * Returns number of segments through nsegs
	     */
	    if(bus_dmamap_load_mbuf_sg(lldev->dma_tag_tx,
	        ll_tx_priv->dma_map, m_head, segs, &nsegs, BUS_DMA_NOWAIT)) {
	        xge_trace(XGE_TRACE, "DMA map load failed");
	        XGE_DRV_STATS(tx_map_fail);
		break;
	    }

	    if(lldev->driver_stats.tx_max_frags < nsegs)
	        lldev->driver_stats.tx_max_frags = nsegs;

	    /* Set descriptor buffer for header and each fragment/segment */
	    count = 0;
	    do {
	        xge_hal_fifo_dtr_buffer_set(channelh, dtr, count,
	            (dma_addr_t)htole64(segs[count].ds_addr),
	            segs[count].ds_len);
	        count++;
	    } while(count < nsegs);

	    /* Pre-write Sync of mapping */
	    bus_dmamap_sync(lldev->dma_tag_tx, ll_tx_priv->dma_map,
	        BUS_DMASYNC_PREWRITE);

	    if((lldev->enabled_tso) &&
	        (m_head->m_pkthdr.csum_flags & CSUM_TSO)) {
	        XGE_DRV_STATS(tx_tso);
	        xge_hal_fifo_dtr_mss_set(dtr, m_head->m_pkthdr.tso_segsz);
	    }

	    /* Checksum */
	    if(ifnetp->if_hwassist > 0) {
	        xge_hal_fifo_dtr_cksum_set_bits(dtr, XGE_HAL_TXD_TX_CKO_IPV4_EN
	            | XGE_HAL_TXD_TX_CKO_TCP_EN | XGE_HAL_TXD_TX_CKO_UDP_EN);
	    }

	    /* Post descriptor to FIFO channel */
	    xge_hal_fifo_dtr_post(channelh, dtr);
	    XGE_DRV_STATS(tx_posted);

	    /* Send the same copy of mbuf packet to BPF (Berkely Packet Filter)
	     * listener so that we can use tools like tcpdump */
	    ETHER_BPF_MTAP(ifnetp, m_head);
	}

	/* Prepend the packet back to queue */
	IF_PREPEND(&ifnetp->if_snd, m_head);
	ifnetp->if_drv_flags |= IFF_DRV_OACTIVE;

	xge_queue_produce_context(xge_hal_device_queue(lldev->devh),
	    XGE_LL_EVENT_TRY_XMIT_AGAIN, lldev->devh);
	XGE_DRV_STATS(tx_again);
}

/**
 * xge_get_buf
 * Allocates new mbufs to be placed into descriptors
 *
 * @dtrh Descriptor Handle
 * @rxd_priv Rx Descriptor Private Data
 * @lldev Per-adapter Data
 * @index Buffer Index (if multi-buffer mode)
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
int
xge_get_buf(xge_hal_dtr_h dtrh, xge_rx_priv_t *rxd_priv,
	xge_lldev_t *lldev, int index)
{
	register mbuf_t mp            = NULL;
	struct          ifnet *ifnetp = lldev->ifnetp;
	int             status        = XGE_HAL_OK;
	int             buffer_size = 0, cluster_size = 0, count;
	bus_dmamap_t    map = rxd_priv->dmainfo[index].dma_map;
	bus_dma_segment_t segs[3];

	buffer_size = (lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) ?
	    ifnetp->if_mtu + XGE_HAL_MAC_HEADER_MAX_SIZE :
	    lldev->rxd_mbuf_len[index];

	if(buffer_size <= MCLBYTES) {
	    cluster_size = MCLBYTES;
	    mp = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	}
	else {
	    cluster_size = MJUMPAGESIZE;
	    if((lldev->buffer_mode != XGE_HAL_RING_QUEUE_BUFFER_MODE_5) &&
	        (buffer_size > MJUMPAGESIZE)) {
	        cluster_size = MJUM9BYTES;
	    }
	    mp = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, cluster_size);
	}
	if(!mp) {
	    xge_trace(XGE_ERR, "Out of memory to allocate mbuf");
	    status = XGE_HAL_FAIL;
	    goto getbuf_out;
	}

	/* Update mbuf's length, packet length and receive interface */
	mp->m_len = mp->m_pkthdr.len = buffer_size;
	mp->m_pkthdr.rcvif = ifnetp;

	/* Load DMA map */
	if(bus_dmamap_load_mbuf_sg(lldev->dma_tag_rx, lldev->extra_dma_map,
	    mp, segs, &count, BUS_DMA_NOWAIT)) {
	    XGE_DRV_STATS(rx_map_fail);
	    m_freem(mp);
	    XGE_EXIT_ON_ERR("DMA map load failed", getbuf_out, XGE_HAL_FAIL);
	}

	/* Update descriptor private data */
	rxd_priv->bufferArray[index]         = mp;
	rxd_priv->dmainfo[index].dma_phyaddr = htole64(segs->ds_addr);
	rxd_priv->dmainfo[index].dma_map     = lldev->extra_dma_map;
	lldev->extra_dma_map = map;

	/* Pre-Read/Write sync */
	bus_dmamap_sync(lldev->dma_tag_rx, map, BUS_DMASYNC_POSTREAD);

	/* Unload DMA map of mbuf in current descriptor */
	bus_dmamap_unload(lldev->dma_tag_rx, map);

	/* Set descriptor buffer */
	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    xge_hal_ring_dtr_1b_set(dtrh, rxd_priv->dmainfo[0].dma_phyaddr,
	        cluster_size);
	}

getbuf_out:
	return status;
}

/**
 * xge_get_buf_3b_5b
 * Allocates new mbufs to be placed into descriptors (in multi-buffer modes)
 *
 * @dtrh Descriptor Handle
 * @rxd_priv Rx Descriptor Private Data
 * @lldev Per-adapter Data
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
int
xge_get_buf_3b_5b(xge_hal_dtr_h dtrh, xge_rx_priv_t *rxd_priv,
	xge_lldev_t *lldev)
{
	bus_addr_t  dma_pointers[5];
	int         dma_sizes[5];
	int         status = XGE_HAL_OK, index;
	int         newindex = 0;

	for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	    status = xge_get_buf(dtrh, rxd_priv, lldev, index);
	    if(status != XGE_HAL_OK) {
	        for(newindex = 0; newindex < index; newindex++) {
	            m_freem(rxd_priv->bufferArray[newindex]);
	        }
	        XGE_EXIT_ON_ERR("mbuf allocation failed", _exit, status);
	    }
	}

	for(index = 0; index < lldev->buffer_mode; index++) {
	    if(lldev->rxd_mbuf_len[index] != 0) {
	        dma_pointers[index] = rxd_priv->dmainfo[index].dma_phyaddr;
	        dma_sizes[index]    = lldev->rxd_mbuf_len[index];
	    }
	    else {
	        dma_pointers[index] = rxd_priv->dmainfo[index-1].dma_phyaddr;
	        dma_sizes[index]    = 1;
	    }
	}

	/* Assigning second buffer to third pointer in 2 buffer mode */
	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_2) {
	    dma_pointers[2] = dma_pointers[1];
	    dma_sizes[2]    = dma_sizes[1];
	    dma_sizes[1]    = 1;
	}

	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_5) {
	    xge_hal_ring_dtr_5b_set(dtrh, dma_pointers, dma_sizes);
	}
	else {
	    xge_hal_ring_dtr_3b_set(dtrh, dma_pointers, dma_sizes);
	}

_exit:
	return status;
}

/**
 * xge_tx_compl
 * If the interrupt is due to Tx completion, free the sent buffer
 *
 * @channelh Channel Handle
 * @dtr Descriptor
 * @t_code Transfer Code indicating success or error
 * @userdata Per-adapter Data
 *
 * Returns XGE_HAL_OK or HAL error enum
 */
xge_hal_status_e
xge_tx_compl(xge_hal_channel_h channelh,
	xge_hal_dtr_h dtr, u8 t_code, void *userdata)
{
	xge_tx_priv_t *ll_tx_priv = NULL;
	xge_lldev_t   *lldev  = (xge_lldev_t *)userdata;
	struct ifnet  *ifnetp = lldev->ifnetp;
	mbuf_t         m_buffer = NULL;
	int            qindex   = xge_hal_channel_id(channelh);

	mtx_lock(&lldev->mtx_tx[qindex]);

	XGE_DRV_STATS(tx_completions);

	/*
	 * For each completed descriptor: Get private structure, free buffer,
	 * do unmapping, and free descriptor
	 */
	do {
	    XGE_DRV_STATS(tx_desc_compl);

	    if(t_code) {
	        XGE_DRV_STATS(tx_tcode);
	        xge_trace(XGE_TRACE, "t_code %d", t_code);
	        xge_hal_device_handle_tcode(channelh, dtr, t_code);
	    }

	    ll_tx_priv = xge_hal_fifo_dtr_private(dtr);
	    m_buffer   = ll_tx_priv->buffer;
	    bus_dmamap_unload(lldev->dma_tag_tx, ll_tx_priv->dma_map);
	    m_freem(m_buffer);
	    ll_tx_priv->buffer = NULL;
	    xge_hal_fifo_dtr_free(channelh, dtr);
	} while(xge_hal_fifo_dtr_next_completed(channelh, &dtr, &t_code)
	    == XGE_HAL_OK);
	xge_send_locked(ifnetp, qindex);
	ifnetp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	mtx_unlock(&lldev->mtx_tx[qindex]);

	return XGE_HAL_OK;
}

/**
 * xge_tx_initial_replenish
 * Initially allocate buffers and set them into descriptors for later use
 *
 * @channelh Tx Channel Handle
 * @dtrh Descriptor Handle
 * @index
 * @userdata Per-adapter Data
 * @reopen Channel open/reopen option
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
xge_hal_status_e
xge_tx_initial_replenish(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	int index, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xge_tx_priv_t *txd_priv = NULL;
	int            status   = XGE_HAL_OK;

	/* Get the user data portion from channel handle */
	xge_lldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(lldev == NULL) {
	    XGE_EXIT_ON_ERR("Failed to get user data from channel", txinit_out,
	        XGE_HAL_FAIL);
	}

	/* Get the private data */
	txd_priv = (xge_tx_priv_t *) xge_hal_fifo_dtr_private(dtrh);
	if(txd_priv == NULL) {
	    XGE_EXIT_ON_ERR("Failed to get descriptor private data", txinit_out,
	        XGE_HAL_FAIL);
	}

	/* Create DMA map for this descriptor */
	if(bus_dmamap_create(lldev->dma_tag_tx, BUS_DMA_NOWAIT,
	    &txd_priv->dma_map)) {
	    XGE_EXIT_ON_ERR("DMA map creation for Tx descriptor failed",
	        txinit_out, XGE_HAL_FAIL);
	}

txinit_out:
	return status;
}

/**
 * xge_rx_initial_replenish
 * Initially allocate buffers and set them into descriptors for later use
 *
 * @channelh Tx Channel Handle
 * @dtrh Descriptor Handle
 * @index Ring Index
 * @userdata Per-adapter Data
 * @reopen Channel open/reopen option
 *
 * Returns XGE_HAL_OK or HAL error enums
 */
xge_hal_status_e
xge_rx_initial_replenish(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	int index, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xge_rx_priv_t  *rxd_priv = NULL;
	int             status   = XGE_HAL_OK;
	int             index1 = 0, index2 = 0;

	/* Get the user data portion from channel handle */
	xge_lldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(lldev == NULL) {
	    XGE_EXIT_ON_ERR("Failed to get user data from channel", rxinit_out,
	        XGE_HAL_FAIL);
	}

	/* Get the private data */
	rxd_priv = (xge_rx_priv_t *) xge_hal_ring_dtr_private(channelh, dtrh);
	if(rxd_priv == NULL) {
	    XGE_EXIT_ON_ERR("Failed to get descriptor private data", rxinit_out,
	        XGE_HAL_FAIL);
	}

	rxd_priv->bufferArray = xge_os_malloc(NULL,
	        (sizeof(rxd_priv->bufferArray) * lldev->rxd_mbuf_cnt));

	if(rxd_priv->bufferArray == NULL) {
	    XGE_EXIT_ON_ERR("Failed to allocate Rxd private", rxinit_out,
	        XGE_HAL_FAIL);
	}

	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    /* Create DMA map for these descriptors*/
	    if(bus_dmamap_create(lldev->dma_tag_rx , BUS_DMA_NOWAIT,
	        &rxd_priv->dmainfo[0].dma_map)) {
	        XGE_EXIT_ON_ERR("DMA map creation for Rx descriptor failed",
	            rxinit_err_out, XGE_HAL_FAIL);
	    }
	    /* Get a buffer, attach it to this descriptor */
	    status = xge_get_buf(dtrh, rxd_priv, lldev, 0);
	}
	else {
	    for(index1 = 0; index1 < lldev->rxd_mbuf_cnt; index1++) {
	        /* Create DMA map for this descriptor */
	        if(bus_dmamap_create(lldev->dma_tag_rx , BUS_DMA_NOWAIT ,
	            &rxd_priv->dmainfo[index1].dma_map)) {
	            for(index2 = index1 - 1; index2 >= 0; index2--) {
	                bus_dmamap_destroy(lldev->dma_tag_rx,
	                    rxd_priv->dmainfo[index2].dma_map);
	            }
	            XGE_EXIT_ON_ERR(
	                "Jumbo DMA map creation for Rx descriptor failed",
	                rxinit_err_out, XGE_HAL_FAIL);
	        }
	    }
	    status = xge_get_buf_3b_5b(dtrh, rxd_priv, lldev);
	}

	if(status != XGE_HAL_OK) {
	    for(index1 = 0; index1 < lldev->rxd_mbuf_cnt; index1++) {
	        bus_dmamap_destroy(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index1].dma_map);
	    }
	    goto rxinit_err_out;
	}
	else {
	    goto rxinit_out;
	}

rxinit_err_out:
	xge_os_free(NULL, rxd_priv->bufferArray,
	    (sizeof(rxd_priv->bufferArray) * lldev->rxd_mbuf_cnt));
rxinit_out:
	return status;
}

/**
 * xge_rx_term
 * During unload terminate and free all descriptors
 *
 * @channelh Rx Channel Handle
 * @dtrh Rx Descriptor Handle
 * @state Descriptor State
 * @userdata Per-adapter Data
 * @reopen Channel open/reopen option
 */
void
xge_rx_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	xge_hal_dtr_state_e state, void *userdata,
	xge_hal_channel_reopen_e reopen)
{
	xge_rx_priv_t *rxd_priv = NULL;
	xge_lldev_t   *lldev    = NULL;
	int            index = 0;

	/* Descriptor state is not "Posted" */
	if(state != XGE_HAL_DTR_STATE_POSTED) goto rxterm_out;

	/* Get the user data portion */
	lldev = xge_hal_channel_userdata(channelh);

	/* Get the private data */
	rxd_priv = (xge_rx_priv_t *) xge_hal_ring_dtr_private(channelh, dtrh);

	for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	    if(rxd_priv->dmainfo[index].dma_map != NULL) {
	        bus_dmamap_sync(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map, BUS_DMASYNC_POSTREAD);
	        bus_dmamap_unload(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map);
	        if(rxd_priv->bufferArray[index] != NULL)
	            m_free(rxd_priv->bufferArray[index]);
	        bus_dmamap_destroy(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map);
	    }
	}
	xge_os_free(NULL, rxd_priv->bufferArray,
	    (sizeof(rxd_priv->bufferArray) * lldev->rxd_mbuf_cnt));

	/* Free the descriptor */
	xge_hal_ring_dtr_free(channelh, dtrh);

rxterm_out:
	return;
}

/**
 * xge_tx_term
 * During unload terminate and free all descriptors
 *
 * @channelh Rx Channel Handle
 * @dtrh Rx Descriptor Handle
 * @state Descriptor State
 * @userdata Per-adapter Data
 * @reopen Channel open/reopen option
 */
void
xge_tx_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtr,
	xge_hal_dtr_state_e state, void *userdata,
	xge_hal_channel_reopen_e reopen)
{
	xge_tx_priv_t *ll_tx_priv = xge_hal_fifo_dtr_private(dtr);
	xge_lldev_t   *lldev      = (xge_lldev_t *)userdata;

	/* Destroy DMA map */
	bus_dmamap_destroy(lldev->dma_tag_tx, ll_tx_priv->dma_map);
}

/**
 * xge_methods
 *
 * FreeBSD device interface entry points
 */
static device_method_t xge_methods[] = {
	DEVMETHOD(device_probe,     xge_probe),
	DEVMETHOD(device_attach,    xge_attach),
	DEVMETHOD(device_detach,    xge_detach),
	DEVMETHOD(device_shutdown,  xge_shutdown),

	DEVMETHOD_END
};

static driver_t xge_driver = {
	"nxge",
	xge_methods,
	sizeof(xge_lldev_t),
};
static devclass_t xge_devclass;
DRIVER_MODULE(nxge, pci, xge_driver, xge_devclass, 0, 0);

