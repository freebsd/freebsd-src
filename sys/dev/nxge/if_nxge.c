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

/*
 *  if_nxge.c
 *
 *  FreeBSD specific initialization & routines
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

/******************************************
 * xge_probe
 * Parameters: Device structure
 * Return: BUS_PROBE_DEFAULT/ENXIO/ENOMEM
 * Description: Probes for Xframe device
 ******************************************/
int
xge_probe(device_t dev)
{
	int  devid    = pci_get_device(dev);
	int  vendorid = pci_get_vendor(dev);
	int  retValue = ENXIO;

	ENTER_FUNCTION

	if(vendorid == XGE_PCI_VENDOR_ID) {
	    if((devid == XGE_PCI_DEVICE_ID_XENA_2) ||
	        (devid == XGE_PCI_DEVICE_ID_HERC_2)) {
	        if(!copyright_print) {
	            PRINT_COPYRIGHT;
	            copyright_print = 1;
	        }
	        device_set_desc_copy(dev,
	            "Neterion Xframe 10 Gigabit Ethernet Adapter");
	        retValue = BUS_PROBE_DEFAULT;
	    }
	}

	LEAVE_FUNCTION
	return retValue;
}

/******************************************
 * xge_init_params
 * Parameters: HAL device configuration
 * structure, device pointer
 * Return: None
 * Description: Sets parameter values in
 * xge_hal_device_config_t structure
 ******************************************/
void
xge_init_params(xge_hal_device_config_t *dconfig, device_t dev)
{
	int index, revision;
	device_t checkdev;

	ENTER_FUNCTION

#define SAVE_PARAM(to, what, value) to.what = value;

#define GET_PARAM(str_kenv, to, param, hardcode) {                            \
	static int param##__LINE__;                                           \
	if(testenv(str_kenv) == 1) {                                          \
	    getenv_int(str_kenv, &param##__LINE__);                           \
	}                                                                     \
	else {                                                                \
	    param##__LINE__ = hardcode;                                       \
	}                                                                     \
	SAVE_PARAM(to, param, param##__LINE__);                               \
}

#define GET_PARAM_MAC(str_kenv, param, hardcode)                              \
	GET_PARAM(str_kenv, ((*dconfig).mac), param, hardcode);

#define GET_PARAM_FIFO(str_kenv, param, hardcode)                             \
	GET_PARAM(str_kenv, ((*dconfig).fifo), param, hardcode);

#define GET_PARAM_FIFO_QUEUE(str_kenv, param, qindex, hardcode)               \
	GET_PARAM(str_kenv, ((*dconfig).fifo.queue[qindex]), param, hardcode);

#define GET_PARAM_FIFO_QUEUE_TTI(str_kenv, param, qindex, tindex, hardcode)   \
	GET_PARAM(str_kenv, ((*dconfig).fifo.queue[qindex].tti[tindex]),      \
	    param, hardcode);

#define GET_PARAM_RING(str_kenv, param, hardcode)                             \
	GET_PARAM(str_kenv, ((*dconfig).ring), param, hardcode);

#define GET_PARAM_RING_QUEUE(str_kenv, param, qindex, hardcode)               \
	GET_PARAM(str_kenv, ((*dconfig).ring.queue[qindex]), param, hardcode);

#define GET_PARAM_RING_QUEUE_RTI(str_kenv, param, qindex, hardcode)           \
	GET_PARAM(str_kenv, ((*dconfig).ring.queue[qindex].rti), param,       \
	    hardcode);

	dconfig->mtu                   = XGE_DEFAULT_INITIAL_MTU;
	dconfig->pci_freq_mherz        = XGE_DEFAULT_USER_HARDCODED;
	dconfig->device_poll_millis    = XGE_HAL_DEFAULT_DEVICE_POLL_MILLIS;
	dconfig->link_stability_period = XGE_HAL_DEFAULT_LINK_STABILITY_PERIOD;
	dconfig->mac.rmac_bcast_en     = XGE_DEFAULT_MAC_RMAC_BCAST_EN;
	dconfig->fifo.alignment_size   = XGE_DEFAULT_FIFO_ALIGNMENT_SIZE;

	GET_PARAM("hw.xge.latency_timer", (*dconfig), latency_timer,
	    XGE_DEFAULT_LATENCY_TIMER);
	GET_PARAM("hw.xge.max_splits_trans", (*dconfig), max_splits_trans,
	    XGE_DEFAULT_MAX_SPLITS_TRANS);
	GET_PARAM("hw.xge.mmrb_count", (*dconfig), mmrb_count, 
	    XGE_DEFAULT_MMRB_COUNT);
	GET_PARAM("hw.xge.shared_splits", (*dconfig), shared_splits,
	    XGE_DEFAULT_SHARED_SPLITS);
	GET_PARAM("hw.xge.isr_polling_cnt", (*dconfig), isr_polling_cnt,
	    XGE_DEFAULT_ISR_POLLING_CNT);
	GET_PARAM("hw.xge.stats_refresh_time_sec", (*dconfig), 
	    stats_refresh_time_sec, XGE_DEFAULT_STATS_REFRESH_TIME_SEC);

	GET_PARAM_MAC("hw.xge.mac_tmac_util_period", tmac_util_period,
	    XGE_DEFAULT_MAC_TMAC_UTIL_PERIOD);
	GET_PARAM_MAC("hw.xge.mac_rmac_util_period", rmac_util_period,
	    XGE_DEFAULT_MAC_RMAC_UTIL_PERIOD);
	GET_PARAM_MAC("hw.xge.mac_rmac_pause_gen_en", rmac_pause_gen_en,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_GEN_EN);
	GET_PARAM_MAC("hw.xge.mac_rmac_pause_rcv_en", rmac_pause_rcv_en,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_RCV_EN);
	GET_PARAM_MAC("hw.xge.mac_rmac_pause_time", rmac_pause_time,
	    XGE_DEFAULT_MAC_RMAC_PAUSE_TIME);
	GET_PARAM_MAC("hw.xge.mac_mc_pause_threshold_q0q3", 
	    mc_pause_threshold_q0q3, XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q0Q3);
	GET_PARAM_MAC("hw.xge.mac_mc_pause_threshold_q4q7",
	    mc_pause_threshold_q4q7, XGE_DEFAULT_MAC_MC_PAUSE_THRESHOLD_Q4Q7);

	GET_PARAM_FIFO("hw.xge.fifo_memblock_size", memblock_size,
	    XGE_DEFAULT_FIFO_MEMBLOCK_SIZE);
	GET_PARAM_FIFO("hw.xge.fifo_reserve_threshold", reserve_threshold,
	    XGE_DEFAULT_FIFO_RESERVE_THRESHOLD);
	GET_PARAM_FIFO("hw.xge.fifo_max_frags", max_frags, 
	    XGE_DEFAULT_FIFO_MAX_FRAGS);

	GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_intr", intr, 0,
	    XGE_DEFAULT_FIFO_QUEUE_INTR);
	GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_max", max, 0,
	    XGE_DEFAULT_FIFO_QUEUE_MAX);
	GET_PARAM_FIFO_QUEUE("hw.xge.fifo_queue_initial", initial, 0,
	    XGE_DEFAULT_FIFO_QUEUE_INITIAL);

	for (index = 0; index < XGE_HAL_MAX_FIFO_TTI_NUM; index++) {
	    dconfig->fifo.queue[0].tti[index].enabled  = 1;
	    dconfig->fifo.queue[0].configured          = 1;

	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_a",
	        urange_a, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_A);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_b",
	        urange_b, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_B);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_urange_c",
	        urange_c, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_URANGE_C);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_a",
	        ufc_a, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_A);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_b",
	        ufc_b, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_B);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_c",
	        ufc_c, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_C);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_ufc_d",
	        ufc_d, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_UFC_D);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_timer_ci_en", 
	        timer_ci_en, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_CI_EN);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_timer_ac_en",
	        timer_ac_en, 0, index, XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_AC_EN);
	    GET_PARAM_FIFO_QUEUE_TTI("hw.xge.fifo_queue_tti_timer_val_us",
	        timer_val_us, 0, index,
	        XGE_DEFAULT_FIFO_QUEUE_TTI_TIMER_VAL_US);
	}

	GET_PARAM_RING("hw.xge.ring_memblock_size", memblock_size,
	    XGE_DEFAULT_RING_MEMBLOCK_SIZE);
	
	GET_PARAM_RING("hw.xge.ring_strip_vlan_tag", strip_vlan_tag,
	    XGE_DEFAULT_RING_STRIP_VLAN_TAG);
	
	for (index = 0; index < XGE_HAL_MIN_RING_NUM; index++) {
	    dconfig->ring.queue[index].max_frm_len  = XGE_HAL_RING_USE_MTU; 
	    dconfig->ring.queue[index].priority     = 0;
	    dconfig->ring.queue[index].configured   = 1;
	    dconfig->ring.queue[index].buffer_mode  =
	        XGE_HAL_RING_QUEUE_BUFFER_MODE_1;

	    GET_PARAM_RING_QUEUE("hw.xge.ring_queue_max", max, index,
	        XGE_DEFAULT_RING_QUEUE_MAX);
	    GET_PARAM_RING_QUEUE("hw.xge.ring_queue_initial", initial, index,
	        XGE_DEFAULT_RING_QUEUE_INITIAL);
	    GET_PARAM_RING_QUEUE("hw.xge.ring_queue_dram_size_mb", dram_size_mb,
	        index, XGE_DEFAULT_RING_QUEUE_DRAM_SIZE_MB);
	    GET_PARAM_RING_QUEUE("hw.xge.ring_queue_indicate_max_pkts", 
	        indicate_max_pkts, index,
	        XGE_DEFAULT_RING_QUEUE_INDICATE_MAX_PKTS);
	    GET_PARAM_RING_QUEUE("hw.xge.ring_queue_backoff_interval_us",
	        backoff_interval_us, index, 
	        XGE_DEFAULT_RING_QUEUE_BACKOFF_INTERVAL_US);

	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_a", ufc_a,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_UFC_A);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_b", ufc_b,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_UFC_B);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_c", ufc_c,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_UFC_C);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_ufc_d", ufc_d,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_UFC_D);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_timer_ac_en", 
	        timer_ac_en, index, XGE_DEFAULT_RING_QUEUE_RTI_TIMER_AC_EN);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_timer_val_us",
	        timer_val_us, index, XGE_DEFAULT_RING_QUEUE_RTI_TIMER_VAL_US);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_a", urange_a,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_A);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_b", urange_b,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_B);
	    GET_PARAM_RING_QUEUE_RTI("hw.xge.ring_queue_rti_urange_c", urange_c,
	        index, XGE_DEFAULT_RING_QUEUE_RTI_URANGE_C);
	}

	if(dconfig->fifo.max_frags > (PAGE_SIZE/32)) {
	    xge_os_printf("fifo_max_frags = %d", dconfig->fifo.max_frags);
	    xge_os_printf("fifo_max_frags should be <= (PAGE_SIZE / 32) = %d",
	        (PAGE_SIZE / 32));
	    xge_os_printf("Using fifo_max_frags = %d", (PAGE_SIZE / 32));
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

#ifdef XGE_FEATURE_LRO
	/* updating the LRO frame's sg size and frame len size. */
	dconfig->lro_sg_size = 20;
	dconfig->lro_frm_len = 65536;
#endif

	LEAVE_FUNCTION
}

/******************************************
 * xge_driver_initialize
 * Parameters: None
 * Return: 0/1
 * Description: Defines HAL-ULD callbacks
 * and initializes the HAL driver
 ******************************************/
int
xge_driver_initialize(void)
{
	xge_hal_uld_cbs_t       uld_callbacks;
	xge_hal_driver_config_t driver_config;
	xge_hal_status_e        status = XGE_HAL_OK;

	ENTER_FUNCTION

	/* Initialize HAL driver */
	if(!hal_driver_init_count) {
	    xge_os_memzero(&uld_callbacks, sizeof(xge_hal_uld_cbs_t));

	    /*
	     * Initial and maximum size of the queue used to store the events
	     * like Link up/down (xge_hal_event_e)
	     */
	    driver_config.queue_size_initial = 1;
	    driver_config.queue_size_max     = 4;

	    uld_callbacks.link_up   = xgell_callback_link_up;
	    uld_callbacks.link_down = xgell_callback_link_down;
	    uld_callbacks.crit_err  = xgell_callback_crit_err;
	    uld_callbacks.event     = xgell_callback_event;

	    status = xge_hal_driver_initialize(&driver_config, &uld_callbacks);
	    if(status != XGE_HAL_OK) {
	        xge_os_printf("xgeX: Initialization failed (Status: %d)",
	            status);
	        goto xdi_out;
	    }
	}
	hal_driver_init_count = hal_driver_init_count + 1;

	xge_hal_driver_debug_module_mask_set(0xffffffff);
	xge_hal_driver_debug_level_set(XGE_TRACE);

xdi_out:
	LEAVE_FUNCTION
	return status;
}

/******************************************
 * Function:    xge_media_init
 * Parameters:  Device pointer
 * Return:      None
 * Description: Initializes, adds and sets
 *              media
 ******************************************/
void
xge_media_init(device_t devc)
{
	xgelldev_t *lldev = (xgelldev_t *)device_get_softc(devc);

	ENTER_FUNCTION

	/* Initialize Media */
	ifmedia_init(&lldev->xge_media, IFM_IMASK, xge_ifmedia_change,
	    xge_ifmedia_status);

	/* Add supported media */
	ifmedia_add(&lldev->xge_media, IFM_ETHER | IFM_1000_SX | IFM_FDX,
	    0, NULL);
	ifmedia_add(&lldev->xge_media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	ifmedia_add(&lldev->xge_media, IFM_ETHER | IFM_AUTO,    0, NULL);
	ifmedia_add(&lldev->xge_media, IFM_ETHER | IFM_10G_SR,  0, NULL);
	ifmedia_add(&lldev->xge_media, IFM_ETHER | IFM_10G_LR,  0, NULL);

	/* Set media */
	ifmedia_set(&lldev->xge_media, IFM_ETHER | IFM_AUTO);

	LEAVE_FUNCTION
}

/*
 * xge_pci_space_save
 * Save PCI configuration space
 * @dev Device structure
 */
void
xge_pci_space_save(device_t dev)
{
	ENTER_FUNCTION

	struct pci_devinfo *dinfo = NULL;

	dinfo = device_get_ivars(dev);
	xge_trace(XGE_TRACE, "Saving PCI configuration space");
	pci_cfg_save(dev, dinfo, 0);

	LEAVE_FUNCTION
}

/*
 * xge_pci_space_restore
 * Restore saved PCI configuration space
 * @dev Device structure
 */
void
xge_pci_space_restore(device_t dev)
{
	ENTER_FUNCTION

	struct pci_devinfo *dinfo = NULL;

	dinfo = device_get_ivars(dev);
	xge_trace(XGE_TRACE, "Restoring PCI configuration space");
	pci_cfg_restore(dev, dinfo);

	LEAVE_FUNCTION
}

/******************************************
 * xge_attach
 * Parameters: Per adapter xgelldev_t
 * structure pointer
 * Return: None
 * Description: Connects the driver to the
 * system if the probe routine returned success
 ******************************************/
int
xge_attach(device_t dev)
{
	xge_hal_device_config_t *device_config;
	xge_hal_ring_config_t   *pRingConfig;
	xge_hal_device_attr_t   attr;
	xgelldev_t              *lldev;
	xge_hal_device_t        *hldev;
	pci_info_t              *pci_info;
	struct ifnet            *ifnetp;
	char                    *mesg;
	char		        *desc;
	int                     rid;
	int                     rid0;
	int                     rid1;
	int                     error;
	u64                     val64    = 0;
	int                     retValue = 0;
	int                     mode     = 0;
	int                     buffer_index, buffer_length, index;

	ENTER_FUNCTION

	device_config = xge_malloc(sizeof(xge_hal_device_config_t));
	if(!device_config) {
	    xge_ctrace(XGE_ERR, "Malloc of device config failed");
	    retValue = ENOMEM;
	    goto attach_out_config;
	}

	lldev = (xgelldev_t *) device_get_softc(dev);
	if(!lldev) {
	    xge_ctrace(XGE_ERR, "Adapter softc structure allocation failed");
	    retValue = ENOMEM;
	    goto attach_out;
	}
	lldev->device = dev;

	/* Initialize mutex */
	if(mtx_initialized(&lldev->xge_lock) == 0) {
	    mtx_init((&lldev->xge_lock), "xge", MTX_NETWORK_LOCK, MTX_DEF);
	}

	error = xge_driver_initialize();
	if(error != XGE_HAL_OK) {
	    xge_ctrace(XGE_ERR, "Initializing driver failed");
	    freeResources(dev, 1);
	    retValue = ENXIO;
	    goto attach_out;
	}

	/* HAL device */
	hldev = (xge_hal_device_t *)xge_malloc(sizeof(xge_hal_device_t));
	if(!hldev) {
	    xge_trace(XGE_ERR, "Allocating memory for xge_hal_device_t failed");
	    freeResources(dev, 2);
	    retValue = ENOMEM;
	    goto attach_out;
	}
	lldev->devh = hldev;

	/* Our private structure */
	pci_info = (pci_info_t*) xge_malloc(sizeof(pci_info_t));
	if(!pci_info) {
	    xge_trace(XGE_ERR, "Allocating memory for pci_info_t failed");
	    freeResources(dev, 3);
	    retValue = ENOMEM;
	    goto attach_out;
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
	    xge_trace(XGE_ERR, "NULL handler for BAR0");
	    freeResources(dev, 4);
	    retValue = ENOMEM;
	    goto attach_out;
	}
	attr.bar0 = (char *)pci_info->regmap0;

	pci_info->bar0resource =
	    (busresource_t*) xge_malloc(sizeof(busresource_t));
	if(pci_info->bar0resource == NULL) {
	    xge_trace(XGE_ERR, "Allocating memory for bar0resources failed");
	    freeResources(dev, 5);
	    retValue = ENOMEM;
	    goto attach_out;
	}
	((struct busresources *)(pci_info->bar0resource))->bus_tag =
	    rman_get_bustag(pci_info->regmap0);
	((struct busresources *)(pci_info->bar0resource))->bus_handle =
	    rman_get_bushandle(pci_info->regmap0);
	((struct busresources *)(pci_info->bar0resource))->bar_start_addr =
	    pci_info->regmap0;

	/* Get virtual address for BAR1 */
	rid1 = PCIR_BAR(2);
	pci_info->regmap1 = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid1,
	    RF_ACTIVE);
	if(pci_info->regmap1 == NULL) {
	    xge_trace(XGE_ERR, "NULL handler for BAR1");
	    freeResources(dev, 6);
	    retValue = ENOMEM;
	    goto attach_out;
	}
	attr.bar1 = (char *)pci_info->regmap1;

	pci_info->bar1resource =
	    (busresource_t*) xge_malloc(sizeof(busresource_t));
	if(pci_info->bar1resource == NULL) {
	    xge_trace(XGE_ERR, "Allocating memory for bar0resources failed");
	    freeResources(dev, 7);
	    retValue = ENOMEM;
	    goto attach_out;
	}
	((struct busresources *)(pci_info->bar1resource))->bus_tag =
	    rman_get_bustag(pci_info->regmap1);
	((struct busresources *)(pci_info->bar1resource))->bus_handle =
	    rman_get_bushandle(pci_info->regmap1);
	((struct busresources *)(pci_info->bar1resource))->bar_start_addr =
	    pci_info->regmap1;

	/* Save PCI config space */
	xge_pci_space_save(dev);

	attr.regh0 = (busresource_t *) pci_info->bar0resource;
	attr.regh1 = (busresource_t *) pci_info->bar1resource;
	attr.irqh  = lldev->irqhandle;
	attr.cfgh  = pci_info;
	attr.pdev  = pci_info;

	/* Initialize device configuration parameters */
	xge_init_params(device_config, dev);

	/* Initialize HAL device */
	error = xge_hal_device_initialize(hldev, &attr, device_config);
	if(error != XGE_HAL_OK) {
	    switch(error) {
	        case XGE_HAL_ERR_DRIVER_NOT_INITIALIZED:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_DRIVER_NOT_INITIALIZED");
	            break;

	        case XGE_HAL_ERR_OUT_OF_MEMORY:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_OUT_OF_MEMORY");
	            break;

	        case XGE_HAL_ERR_BAD_SUBSYSTEM_ID:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_BAD_SUBSYSTEM_ID");
	            break;

	        case XGE_HAL_ERR_INVALID_MAC_ADDRESS:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_INVALID_MAC_ADDRESS");
	            break;

	        case XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING:
	            xge_trace(XGE_ERR, "XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING");
	            break;

	        case XGE_HAL_ERR_SWAPPER_CTRL:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_SWAPPER_CTRL");
	            break;

	        case XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT:
	            xge_trace(XGE_ERR, "XGE_HAL_ERR_DEVICE_IS_NOT_QUIESCENT");
	            break;
	    }
	    xge_trace(XGE_ERR, "Initializing HAL device failed (error: %d)\n",
	        error);
	    freeResources(dev, 8);
	    retValue = ENXIO;
	    goto attach_out;
	}

	desc = (char *) malloc(100, M_DEVBUF, M_NOWAIT);
	if(desc == NULL) {
	    retValue = ENOMEM;
	}
	else {
	    sprintf(desc, "%s (Rev %d) Driver v%s \n%s: Serial Number: %s ",
	        hldev->vpd_data.product_name, hldev->revision, DRIVER_VERSION,
	        device_get_nameunit(dev), hldev->vpd_data.serial_num);
	    printf("%s: Xframe%s %s\n", device_get_nameunit(dev),
	        ((hldev->device_id == XGE_PCI_DEVICE_ID_XENA_2) ? "I": "II"), 
	        desc);
	    free(desc, M_DEVBUF);

	}
	
	if(pci_get_device(dev) == XGE_PCI_DEVICE_ID_HERC_2) {
	    error = xge_hal_mgmt_reg_read(hldev, 0, 
	        xge_offsetof(xge_hal_pci_bar0_t, pci_info), &val64);
	    if(error != XGE_HAL_OK) {
	        xge_trace(XGE_ERR, "Error for getting bus speed");
	    }
	    mesg = (char *) xge_malloc(20);
	    if(mesg == NULL) {
	        freeResources(dev, 8);
	        retValue = ENOMEM;
	        goto attach_out;
	    }

	    sprintf(mesg, "%s: Device is on %s bit", device_get_nameunit(dev),
	        (val64 & BIT(8)) ? "32":"64");

	    mode = (u8)((val64 & vBIT(0xF, 0, 4)) >> 60);
	    switch(mode) {
	        case 0x00: xge_os_printf("%s PCI 33MHz bus",       mesg); break;
	        case 0x01: xge_os_printf("%s PCI 66MHz bus",       mesg); break;
	        case 0x02: xge_os_printf("%s PCIX(M1) 66MHz bus",  mesg); break;
	        case 0x03: xge_os_printf("%s PCIX(M1) 100MHz bus", mesg); break;
	        case 0x04: xge_os_printf("%s PCIX(M1) 133MHz bus", mesg); break;
	        case 0x05: xge_os_printf("%s PCIX(M2) 133MHz bus", mesg); break;
	        case 0x06: xge_os_printf("%s PCIX(M2) 200MHz bus", mesg); break;
	        case 0x07: xge_os_printf("%s PCIX(M2) 266MHz bus", mesg); break;
	    }
	    free(mesg, M_DEVBUF);
	}

	xge_hal_device_private_set(hldev, lldev);

	error = xge_interface_setup(dev);
	if(error != 0) {
	    retValue = error;
	    goto attach_out;
	}

	ifnetp         = lldev->ifnetp;
	ifnetp->if_mtu = device_config->mtu;

	xge_media_init(dev);

	/* Interrupt */
	rid = 0;
	lldev->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if(lldev->irq == NULL) {
	    xge_trace(XGE_ERR, "NULL handler for IRQ");
	    freeResources(dev, 10);
	    retValue = ENOMEM;
	    goto attach_out;
	}

	/* Associate interrupt handler with the device */
	error = bus_setup_intr(dev, lldev->irq, INTR_TYPE_NET | INTR_MPSAFE,
#if __FreeBSD_version > 700030
	    xge_intr_filter,
#endif
	    (void *)xge_intr, lldev, &lldev->irqhandle);
	if(error != 0) {
	    xge_trace(XGE_ERR,
	        "Associating interrupt handler with device failed");
	    freeResources(dev, 11);
	    retValue = ENXIO;
	    goto attach_out;
	}

	/* Create DMA tags */
	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),  /* Parent                    */
	    PAGE_SIZE,             /* Alignment                 */
	    0,                     /* Bounds                    */
	    BUS_SPACE_MAXADDR,     /* Low Address               */
	    BUS_SPACE_MAXADDR,     /* High Address              */
	    NULL,                  /* Filter Function           */
	    NULL,                  /* Filter Function Arguments */
	    MCLBYTES * MAX_SEGS,   /* Maximum Size              */
	    MAX_SEGS,              /* Number of Segments        */
	    MCLBYTES,              /* Maximum Segment Size      */
	    BUS_DMA_ALLOCNOW,      /* Flags                     */
	    NULL,                  /* Lock Function             */
	    NULL,                  /* Lock Function Arguments   */
	    (&lldev->dma_tag_tx)); /* DMA Tag                   */
	if(error != 0) {
	    xge_trace(XGE_ERR, "Tx DMA tag creation failed");
	    freeResources(dev, 12);
	    retValue = ENOMEM;
	    goto attach_out;
	}

	error = bus_dma_tag_create(
	    bus_get_dma_tag(dev),   /* Parent                    */
	    PAGE_SIZE,              /* Alignment                 */
	    0,                      /* Bounds                    */
	    BUS_SPACE_MAXADDR,      /* Low Address               */
	    BUS_SPACE_MAXADDR,      /* High Address              */
	    NULL,                   /* Filter Function           */
	    NULL,                   /* Filter Function Arguments */
	    MJUMPAGESIZE,           /* Maximum Size              */
	    1,                      /* Number of Segments        */
	    MJUMPAGESIZE,           /* Maximum Segment Size      */
	    BUS_DMA_ALLOCNOW,       /* Flags                     */
	    NULL,                   /* Lock Function             */
	    NULL,                   /* Lock Function Arguments   */
	    (&lldev->dma_tag_rx));  /* DMA Tag                   */
   
	if(error != 0) {
	    xge_trace(XGE_ERR, "Rx DMA tag creation failed");
	    freeResources(dev, 13);
	    retValue = ENOMEM;
	    goto attach_out;
	}

	/*Updating lldev->buffer_mode parameter*/
	pRingConfig = &(hldev->config.ring);

	if((device_config->mtu + XGE_HAL_MAC_HEADER_MAX_SIZE) <= PAGE_SIZE) {
#if defined(XGE_FEATURE_BUFFER_MODE_3)
	    xge_os_printf("%s: 3 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    }
	    pRingConfig->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_A;
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    lldev->rxd_mbuf_len[0]    = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1]    = XGE_HAL_TCPIP_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[2]    = device_config->mtu;
	    lldev->rxd_mbuf_cnt       = 3;
#else
#if defined(XGE_FEATURE_BUFFER_MODE_2)
	    xge_os_printf("%s: 2 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    }
	    pRingConfig->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_B;
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_2;
	    lldev->rxd_mbuf_len[0]    = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1]    = device_config->mtu;
	    lldev->rxd_mbuf_cnt       = 2;
#else
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_1;
	    lldev->rxd_mbuf_len[0]    = device_config->mtu;
	    lldev->rxd_mbuf_cnt       = 1;
#endif
#endif
	}
	else {
	    xge_os_printf("%s: 5 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    xge_os_memzero(lldev->rxd_mbuf_len, sizeof(lldev->rxd_mbuf_len));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_5;
	    }
	    lldev->buffer_mode     = XGE_HAL_RING_QUEUE_BUFFER_MODE_5;
	    buffer_length          = device_config->mtu;
	    buffer_index           = 2;
	    lldev->rxd_mbuf_len[0] = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1] = XGE_HAL_TCPIP_HEADER_MAX_SIZE;

	    while(buffer_length > PAGE_SIZE) {
	        buffer_length -= PAGE_SIZE;
	        lldev->rxd_mbuf_len[buffer_index] = PAGE_SIZE;
	        buffer_index++;
	    }

	    BUFALIGN(buffer_length);

	    lldev->rxd_mbuf_len[buffer_index] = buffer_length;
	    lldev->rxd_mbuf_cnt = buffer_index;
	}

#ifdef XGE_FEATURE_LRO
	xge_os_printf("%s: LRO (Large Receive Offload) Enabled",
	    device_get_nameunit(dev));
#endif

#ifdef XGE_FEATURE_TSO
	    xge_os_printf("%s: TSO (TCP Segmentation Offload) enabled",
	        device_get_nameunit(dev));
#endif

attach_out:
	free(device_config, M_DEVBUF);
attach_out_config:
	LEAVE_FUNCTION
	return retValue;
}

/******************************************
 * freeResources
 * Parameters: Device structure, error (used
 * to branch freeing)
 * Return: None
 * Description: Frees allocated resources
 ******************************************/
void
freeResources(device_t dev, int error)
{
	xgelldev_t *lldev;
	pci_info_t *pci_info;
	xge_hal_device_t *hldev;
	int rid, status;

	ENTER_FUNCTION

	/* LL Device */
	lldev = (xgelldev_t *) device_get_softc(dev);
	pci_info = lldev->pdev;

	/* HAL Device */
	hldev = lldev->devh;

	switch(error) {
	    case 0:
	        status = bus_dma_tag_destroy(lldev->dma_tag_rx);
	        if(status) {
	            xge_trace(XGE_ERR, "Rx DMA tag destroy failed");
	        }

	    case 13:
	        status = bus_dma_tag_destroy(lldev->dma_tag_tx);
	        if(status) {
	            xge_trace(XGE_ERR, "Tx DMA tag destroy failed");
	        }

	    case 12:
	        /* Teardown interrupt handler - device association */
	        bus_teardown_intr(dev, lldev->irq, lldev->irqhandle);

	    case 11:
	        /* Release IRQ */
	        bus_release_resource(dev, SYS_RES_IRQ, 0, lldev->irq);

	    case 10:
	        /* Media */
	        ifmedia_removeall(&lldev->xge_media);

	        /* Detach Ether */
	        ether_ifdetach(lldev->ifnetp);
	        if_free(lldev->ifnetp);

	        xge_hal_device_private_set(hldev, NULL);
	        xge_hal_device_disable(hldev);

	    case 9:
	        /* HAL Device */
	        xge_hal_device_terminate(hldev);

	    case 8:
	        /* Restore PCI configuration space */
	        xge_pci_space_restore(dev);

	        /* Free bar1resource */
	        free(pci_info->bar1resource, M_DEVBUF);

	    case 7:
	        /* Release BAR1 */
	        rid = PCIR_BAR(2);
	        bus_release_resource(dev, SYS_RES_MEMORY, rid,
	            pci_info->regmap1);

	    case 6:
	        /* Free bar0resource */
	        free(pci_info->bar0resource, M_DEVBUF);

	    case 5:
	        /* Release BAR0 */
	        rid = PCIR_BAR(0);
	        bus_release_resource(dev, SYS_RES_MEMORY, rid,
	            pci_info->regmap0);

	    case 4:
	        /* Disable Bus Master */
	        pci_disable_busmaster(dev);

	        /* Free pci_info_t */
	        lldev->pdev = NULL;
	        free(pci_info, M_DEVBUF);

	    case 3:
	        /* Free device configuration struct and HAL device */
	        free(hldev, M_DEVBUF);

	    case 2:
	        /* Terminate HAL driver */
	        hal_driver_init_count = hal_driver_init_count - 1;
	        if(!hal_driver_init_count) {
	            xge_hal_driver_terminate();
	        }

	    case 1:
	        if(mtx_initialized(&lldev->xge_lock) != 0) {
	            mtx_destroy(&lldev->xge_lock);
	        }
	}

	LEAVE_FUNCTION
}

/******************************************
 * xge_detach
 * Parameters: Device structure
 * Return: 0
 * Description: Detaches the driver from the
 * kernel subsystem.
 ******************************************/
int
xge_detach(device_t dev)
{
	xgelldev_t *lldev = (xgelldev_t *)device_get_softc(dev);

	ENTER_FUNCTION

	mtx_lock(&lldev->xge_lock);
	lldev->in_detach = 1;
	xge_stop(lldev);
	mtx_unlock(&lldev->xge_lock);

	freeResources(dev, 0);

	LEAVE_FUNCTION

	return 0;
}

/******************************************
 * xge_shutdown
 * Parameters: Per adapter xgelldev_t
 * structure pointer
 * Return: None
 * Description: Gets called when the system
 * is about to be shutdown.
 ******************************************/
int
xge_shutdown(device_t dev)
{
	xgelldev_t *lldev = (xgelldev_t *) device_get_softc(dev);

	ENTER_FUNCTION
	mtx_lock(&lldev->xge_lock);
	xge_stop(lldev);
	mtx_unlock(&lldev->xge_lock);
	LEAVE_FUNCTION
	return 0;
}

/******************************************
 * Function:    xge_interface_setup
 * Parameters:  Device pointer
 * Return:      0/ENXIO/ENOMEM
 * Description: Sets up the interface
 *              through ifnet pointer
 ******************************************/
int
xge_interface_setup(device_t dev)
{
	u8 mcaddr[ETHER_ADDR_LEN];
	xge_hal_status_e status_code;
	xgelldev_t *lldev = (xgelldev_t *)device_get_softc(dev);
	struct ifnet *ifnetp;
	xge_hal_device_t *hldev = lldev->devh;
	int retValue = 0;

	ENTER_FUNCTION

	/* Get the MAC address of the device */
	status_code = xge_hal_device_macaddr_get(hldev, 0, &mcaddr);
	if(status_code != XGE_HAL_OK) {
	    switch(status_code) {
	        case XGE_HAL_INF_MEM_STROBE_CMD_EXECUTING:
	            xge_trace(XGE_ERR,
	                "Failed to retrieve MAC address (timeout)");
	            break;

	        case XGE_HAL_ERR_OUT_OF_MAC_ADDRESSES:
	            xge_trace(XGE_ERR, "Invalid MAC address index");
	            break;

	        default:
	            xge_trace(XGE_TRACE, "Default Case");
	            break;
	    }
	    freeResources(dev, 9);
	    retValue = ENXIO;
	    goto ifsetup_out;
	}

	/* Get interface ifnet structure for this Ether device */
	ifnetp = lldev->ifnetp = if_alloc(IFT_ETHER);
	if(ifnetp == NULL) {
	    xge_trace(XGE_ERR, "Allocating/getting ifnet structure failed");
	    freeResources(dev, 9);
	    retValue = ENOMEM;
	    goto ifsetup_out;
	}

	/* Initialize interface ifnet structure */
	if_initname(ifnetp, device_get_name(dev), device_get_unit(dev));
	ifnetp->if_mtu = XGE_HAL_DEFAULT_MTU;

	/*
	 * TODO: Can't set more than 2Gbps. -- Higher value results in overflow.
	 * But there is no effect in performance even if you set this to 10 Mbps
	 */
	ifnetp->if_baudrate = IF_Gbps(2);
	ifnetp->if_init     = xge_init;
	ifnetp->if_softc    = lldev;
	ifnetp->if_flags    = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifnetp->if_ioctl    = xge_ioctl;
	ifnetp->if_start    = xge_send;

	/* TODO: Check and assign optimal value */
	ifnetp->if_snd.ifq_maxlen = IFQ_MAXLEN;

	ifnetp->if_capabilities = IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU | 
	    IFCAP_HWCSUM;

	ifnetp->if_capenable = ifnetp->if_capabilities;

#ifdef XGE_FEATURE_TSO
	ifnetp->if_capabilities |= IFCAP_TSO4;
	ifnetp->if_capenable |= IFCAP_TSO4;
#endif

	/* Attach the interface */
	ether_ifattach(ifnetp, mcaddr);

ifsetup_out:
	LEAVE_FUNCTION

	return retValue;
}

/******************************************
 * xgell_callback_link_up
 * Parameters: Per adapter xgelldev_t
 * structure pointer as void *
 * Return: None
 * Description: Called by HAL to notify
 * hardware link up state change
 ******************************************/
void
xgell_callback_link_up(void *userdata)
{
	xgelldev_t *lldev = (xgelldev_t *)userdata;
	struct ifnet *ifnetp = lldev->ifnetp;

	ENTER_FUNCTION

	ifnetp->if_flags  &= ~IFF_DRV_OACTIVE;
	if_link_state_change(ifnetp, LINK_STATE_UP);

	LEAVE_FUNCTION
}

/******************************************
 * xgell_callback_link_down
 * Parameters: Per adapter xgelldev_t
 * structure pointer as void *
 * Return: None
 * Description: Called by HAL to notify
 * hardware link up state change
 ******************************************/
void
xgell_callback_link_down(void *userdata)
{
	xgelldev_t *lldev = (xgelldev_t *)userdata;
	struct ifnet *ifnetp = lldev->ifnetp;

	ENTER_FUNCTION

	ifnetp->if_flags  |= IFF_DRV_OACTIVE;
	if_link_state_change(ifnetp, LINK_STATE_DOWN);

	LEAVE_FUNCTION
}

/******************************************
 * xgell_callback_crit_err
 * Parameters: Per adapter xgelldev_t
 * structure pointer as void *, event,
 * serr_data ->
 * Return: None
 * Description: Called by HAL on serious
 * error event
 ******************************************/
void
xgell_callback_crit_err(void *userdata, xge_hal_event_e type, u64 serr_data)
{
	ENTER_FUNCTION

	xge_trace(XGE_ERR, "Critical Error");
	xgell_reset(userdata);

	LEAVE_FUNCTION
}

/******************************************
 * xgell_callback_event
 * Parameters: Queue item
 * Return: None
 * Description: Called by HAL in case of
 * some unknown to HAL events.
 ******************************************/
void
xgell_callback_event(xge_queue_item_t *item)
{
	xgelldev_t       *lldev  = NULL;
	xge_hal_device_t *hldev  = NULL;
	struct ifnet     *ifnetp = NULL;

	ENTER_FUNCTION

	hldev  = item->context;
	lldev  = xge_hal_device_private(hldev);
	ifnetp = lldev->ifnetp;

	if(item->event_type == XGE_LL_EVENT_TRY_XMIT_AGAIN) {
	    if(lldev->initialized) {
	        if(xge_hal_channel_dtr_count(lldev->fifo_channel_0) > 0) {
	            ifnetp->if_flags  &= ~IFF_DRV_OACTIVE;
	        }
	        else {
	            /* try next time */
	            xge_queue_produce_context(
	                xge_hal_device_queue(lldev->devh),
	                XGE_LL_EVENT_TRY_XMIT_AGAIN, lldev->devh);
	        }
	    }
	}
	else if(item->event_type == XGE_LL_EVENT_DEVICE_RESETTING) {
	    xgell_reset(item->context);
	}

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xge_ifmedia_change
 * Parameters:  Pointer to ifnet structure
 * Return:      0 for success, EINVAL if media
 *              type is not IFM_ETHER.
 * Description: Media change driver callback
 ******************************************/
int
xge_ifmedia_change(struct ifnet *ifnetp)
{
	xgelldev_t     *lldev    = ifnetp->if_softc;
	struct ifmedia *ifmediap = &lldev->xge_media;

	ENTER_FUNCTION
	LEAVE_FUNCTION

	return (IFM_TYPE(ifmediap->ifm_media) != IFM_ETHER) ?  EINVAL:0;
}

/******************************************
 * Function:    xge_ifmedia_status
 * Parameters:  Pointer to ifnet structure
 *              ifmediareq structure pointer
 *              through which status of media
 *              will be returned.
 * Return:      None
 * Description: Media status driver callback
 ******************************************/
void
xge_ifmedia_status(struct ifnet *ifnetp, struct ifmediareq *ifmr)
{
	xge_hal_status_e status;
	u64              regvalue;
	xgelldev_t       *lldev = ifnetp->if_softc;
	xge_hal_device_t *hldev = lldev->devh;

	ENTER_FUNCTION

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	status = xge_hal_mgmt_reg_read(hldev, 0,
	    xge_offsetof(xge_hal_pci_bar0_t, adapter_status), &regvalue);
	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Getting adapter status failed");
	    return;
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

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xge_ioctl
 * Parameters:  Pointer to ifnet structure,
 *              command -> indicates requests,
 *              data -> passed values (if any)
 * Return:
 * Description: IOCTL entry point. Called
 *              when the user wants to
 *              configure the interface
 ******************************************/
int
xge_ioctl(struct ifnet *ifnetp, unsigned long command, caddr_t data)
{
	struct ifmedia              *ifmediap;
	xge_hal_stats_hw_info_t     *hw_stats;
	xge_hal_pci_config_t        *pci_conf;
	xge_hal_device_config_t     *device_conf;
	xge_hal_stats_sw_err_t      *tcode;
	xge_hal_stats_device_info_t *intr;
	bar0reg_t                   *reg;
	xge_hal_status_e             status_code;
	xge_hal_device_t            *hldev;
	void                        *regInfo;
	u64                          value;
	u64                          offset;
	char                        *pAccess;
	char                        *version;
	int                          retValue = 0, index = 0, buffer_mode = 0;
	struct ifreq                *ifreqp   = (struct ifreq *) data;
	xgelldev_t                  *lldev    = ifnetp->if_softc;

	ifmediap = &lldev->xge_media;
	hldev    = lldev->devh;

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
	        retValue = changeMtu(lldev, ifreqp->ifr_mtu);
	        break;

	    /* Set ifnet flags */
	    case SIOCSIFFLAGS:
	        mtx_lock(&lldev->xge_lock);
	        if(ifnetp->if_flags & IFF_UP) {
	            /* Link status is UP */
	            if(!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)) {
	                xge_init_locked(lldev);
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
	        mtx_unlock(&lldev->xge_lock);
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
	        mtx_lock(&lldev->xge_lock);
	        int mask = 0;
	        mask = ifreqp->ifr_reqcap ^ ifnetp->if_capenable;
#if defined(__FreeBSD_version) && (__FreeBSD_version >= 700026)
	        if(mask & IFCAP_TSO4) {
	            if(ifnetp->if_capenable & IFCAP_TSO4) {
	                ifnetp->if_capenable &= ~IFCAP_TSO4;
	                ifnetp->if_hwassist  &= ~CSUM_TSO;
	            }

	            /*enable tso only if txcsum is enabled*/
	            if(ifnetp->if_capenable & IFCAP_TXCSUM) {
	                ifnetp->if_capenable |= IFCAP_TSO4;
	                ifnetp->if_hwassist  |= CSUM_TSO;
	            }
	        }
#endif
	        mtx_unlock(&lldev->xge_lock);
	        break;

	    /* Custom IOCTL 0 :
	     * Used to get Statistics & PCI configuration through application */
	    case SIOCGPRIVATE_0:
	        pAccess = (char*) ifreqp->ifr_data;
	        if(*pAccess == XGE_QUERY_STATS) {
	            mtx_lock(&lldev->xge_lock);
	            status_code = xge_hal_stats_hw(hldev, &hw_stats);
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR,
	                    "Getting statistics failed (Status: %d)",
	                    status_code);
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	            }
	            copyout(hw_stats, ifreqp->ifr_data,
	                sizeof(xge_hal_stats_hw_info_t));
	            mtx_unlock(&lldev->xge_lock);
	        }
	        else if(*pAccess == XGE_QUERY_PCICONF) {
	            pci_conf = xge_malloc(sizeof(xge_hal_pci_config_t));
	            if(pci_conf == NULL) {
	                return(ENOMEM);
	            }
	            mtx_lock(&lldev->xge_lock);
	            status_code = xge_hal_mgmt_pci_config(hldev, pci_conf,
	                sizeof(xge_hal_pci_config_t));
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR,
	                    "Getting PCIconfiguration failed (Status: %d)",
	                    status_code);
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	            }
	            copyout(pci_conf, ifreqp->ifr_data,
	                sizeof(xge_hal_pci_config_t));
	            mtx_unlock(&lldev->xge_lock);
	            free(pci_conf, M_DEVBUF);
	        }
	        else if(*pAccess ==XGE_QUERY_INTRSTATS) {
	            intr = xge_malloc(sizeof(xge_hal_stats_device_info_t));
	            if(intr == NULL) {
	                return(ENOMEM);
	            }
	            mtx_lock(&lldev->xge_lock);
	            status_code =xge_hal_mgmt_device_stats(hldev, intr,
	                sizeof(xge_hal_stats_device_info_t));
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR,
	                    "Getting intr statistics failed (Status: %d)",
	                    status_code);
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	            }
	            copyout(intr, ifreqp->ifr_data,
	                sizeof(xge_hal_stats_device_info_t));
	            mtx_unlock(&lldev->xge_lock);
	            free(intr, M_DEVBUF);
	        }
	        else if(*pAccess == XGE_QUERY_TCODE) {
	            tcode = xge_malloc(sizeof(xge_hal_stats_sw_err_t));
	            if(tcode == NULL) {
	                return(ENOMEM);
	            }
	            mtx_lock(&lldev->xge_lock);
	            status_code =xge_hal_mgmt_sw_stats(hldev, tcode,
	                sizeof(xge_hal_stats_sw_err_t));
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR,
	                    "Getting tcode statistics failed (Status: %d)",
	                    status_code);
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	            }
	            copyout(tcode, ifreqp->ifr_data,
	                sizeof(xge_hal_stats_sw_err_t));
	            mtx_unlock(&lldev->xge_lock);
	            free(tcode, M_DEVBUF);
	        }
	        else if(*pAccess ==XGE_READ_VERSION) {
	            version = xge_malloc(BUFFER_SIZE);
	            if(version == NULL) {
	                return(ENOMEM);
	            }
	            mtx_lock(&lldev->xge_lock);
	            strcpy(version,DRIVER_VERSION);
	            copyout(version, ifreqp->ifr_data, BUFFER_SIZE);
	            mtx_unlock(&lldev->xge_lock);
	            free(version, M_DEVBUF);
	        }
	        else if(*pAccess == XGE_QUERY_DEVCONF) {
	            device_conf = xge_malloc(sizeof(xge_hal_device_config_t));
	            if(device_conf == NULL) {
	                return(ENOMEM);
	            }
	            mtx_lock(&lldev->xge_lock);
	            status_code = xge_hal_mgmt_device_config(hldev, device_conf,
	                sizeof(xge_hal_device_config_t));
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR,
	                    "Getting devconfig failed (Status: %d)",
	                    status_code);
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	            }
	            if(copyout(device_conf, ifreqp->ifr_data,
	                sizeof(xge_hal_device_config_t)) != 0) {
	                xge_trace(XGE_ERR, "Device configuration copyout erro");
	            }
	            mtx_unlock(&lldev->xge_lock);
	            free(device_conf, M_DEVBUF);
	        }
	        else if(*pAccess == XGE_QUERY_BUFFER_MODE) {
	            buffer_mode = lldev->buffer_mode;
	            if(copyout(&buffer_mode, ifreqp->ifr_data,
	                sizeof(int)) != 0) {
	                xge_trace(XGE_ERR, "Error with copyout of buffermode");
	                retValue = EINVAL;
	            }
	        }
	        else if((*pAccess == XGE_SET_BUFFER_MODE_1) ||
	                (*pAccess == XGE_SET_BUFFER_MODE_2) ||
	                (*pAccess == XGE_SET_BUFFER_MODE_3) ||
	                (*pAccess == XGE_SET_BUFFER_MODE_5)) {
	            switch(*pAccess) {
	                case XGE_SET_BUFFER_MODE_1: *pAccess = 'Y'; break;
	                case XGE_SET_BUFFER_MODE_2:
	                case XGE_SET_BUFFER_MODE_3:
	                case XGE_SET_BUFFER_MODE_5: *pAccess = 'N'; break;
	            }
	            if(copyout(pAccess, ifreqp->ifr_data,
	                sizeof(pAccess)) != 0) {
	                xge_trace(XGE_ERR,
	                    "Copyout of chgbufmode result failed");
	            }
	        }
	        else {
	            xge_trace(XGE_TRACE, "Nothing is matching");
	        }
	        break;

	    /*
	     * Custom IOCTL 1 :
	     * Used to get BAR0 register values through application program
	     */
	    case SIOCGPRIVATE_1:
	        reg = (bar0reg_t *) ifreqp->ifr_data;
	        if(strcmp(reg->option,"-r") == 0) {
	            offset  = reg->offset;
	            value   = 0x0000;
	            mtx_lock(&lldev->xge_lock);
	            status_code = xge_hal_mgmt_reg_read(hldev, 0, offset,
	                &value );
	            if(status_code == XGE_HAL_OK) {
	                reg->value = value;
	            }
	            else {
	                xge_trace(XGE_ERR, "Getting register value failed");
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	                break;
	            }
	            copyout(reg, ifreqp->ifr_data, sizeof(bar0reg_t));
	            mtx_unlock(&lldev->xge_lock);
	        }
	        else if(strcmp(reg->option,"-w") == 0) {
	            offset  = reg->offset;
	            value   = reg->value;
	            mtx_lock(&lldev->xge_lock);
	            status_code = xge_hal_mgmt_reg_write(hldev, 0, offset,
	                value );
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR, "Getting register value failed");
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	                break;
	            }
	            value = 0x0000;
	            status_code = xge_hal_mgmt_reg_read(hldev, 0, offset,
	                &value);
	            if(status_code != XGE_HAL_OK) {
	                xge_trace(XGE_ERR, "Getting register value failed");
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	                break;
	            }
	            if(reg->value != value) {
	                mtx_unlock(&lldev->xge_lock);
	                retValue = EINVAL;
	                break;
	            }
	            mtx_unlock(&lldev->xge_lock);
	        }
	        else
	        {
	            offset  = 0x0000;
	            value   = 0x0000;
	            regInfo = (void *)ifreqp->ifr_data;

	            mtx_lock(&lldev->xge_lock);
	            for(index = 0, offset = 0; offset <= XGE_OFFSET_OF_LAST_REG;
	                index++, offset += 0x0008) {
	                status_code = xge_hal_mgmt_reg_read(hldev, 0, offset,
	                    &value);
	                if(status_code == XGE_HAL_OK) {
	                    *( ( u64 *)( ( u64 * )regInfo + index ) ) = value;
	                }
	                else {
	                    xge_trace(XGE_ERR, "Getting register value failed");
	                    mtx_unlock(&lldev->xge_lock);
	                    retValue = EINVAL;
	                    break;
	                }
	            }

	            copyout(regInfo, ifreqp->ifr_data,
	                sizeof(xge_hal_pci_bar0_t));
	            mtx_unlock(&lldev->xge_lock);
	        }
	        break;

	    default:
	        retValue = EINVAL;
	        break;
	}
	return retValue;
}

/******************************************
 * Function:    xge_init
 * Parameters:  Pointer to per-device
 *              xgelldev_t structure as void*.
 * Return:      None
 * Description: Init entry point.
 ******************************************/
void
xge_init(void *plldev)
{
	ENTER_FUNCTION

	xgelldev_t *lldev = (xgelldev_t *)plldev;

	mtx_lock(&lldev->xge_lock);
	xge_init_locked(lldev);
	mtx_unlock(&lldev->xge_lock);

	LEAVE_FUNCTION
}

void
xge_init_locked(void *pdevin)
{
	ENTER_FUNCTION

	xgelldev_t         *lldev  = (xgelldev_t *)pdevin;
	struct ifnet       *ifnetp = lldev->ifnetp;
	device_t           dev     = lldev->device;

	mtx_assert((&lldev->xge_lock), MA_OWNED);

	/* If device is in running state, initializing is not required */
	if(ifnetp->if_drv_flags & IFF_DRV_RUNNING) {
	    return;
	}

	/* Initializing timer */
	callout_init(&lldev->timer, CALLOUT_MPSAFE);

	xge_initialize(dev, XGE_HAL_CHANNEL_OC_NORMAL);

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xge_timer
 * Parameters:  Pointer to per-device
 *              xgelldev_t structure as void*.
 * Return:      None
 * Description: Polls the changes.
 ******************************************/
void
xge_timer(void *devp)
{
	xgelldev_t       *lldev = (xgelldev_t *)devp;
	xge_hal_device_t *hldev = lldev->devh;

	/* Poll for changes */
	xge_hal_device_poll(hldev);

	/* Reset timer */
	callout_reset(&lldev->timer, hz, xge_timer, lldev);

	return;
}

/******************************************
 * Function:    xge_stop
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer
 * Return:      None
 * Description: Deactivates the interface
 *              (Called on "ifconfig down"
 ******************************************/
void
xge_stop(xgelldev_t *lldev)
{
	struct ifnet     *ifnetp = lldev->ifnetp;
	device_t         dev     = lldev->device;

	ENTER_FUNCTION

	mtx_assert((&lldev->xge_lock), MA_OWNED);

	/* If device is not in "Running" state, return */
	if (!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)) {
	    goto xfstop_out;
	}

	xge_terminate(dev, XGE_HAL_CHANNEL_OC_NORMAL);

xfstop_out:
	LEAVE_FUNCTION
	return;
}

/*
 * xge_intr_filter
 * 
 * ISR filter function
 * @handle softc/lldev per device structure
 */
int
xge_intr_filter(void *handle)
{
	xgelldev_t *lldev        = NULL;
	xge_hal_device_t *hldev  = NULL;
	xge_hal_pci_bar0_t *bar0 = NULL;
	device_t dev             = NULL;
	u16 retValue = FILTER_STRAY;
	u64 val64    = 0;

	lldev = (xgelldev_t *)handle;
	hldev = lldev->devh;
	dev   = lldev->device;
	bar0  = (xge_hal_pci_bar0_t *)hldev->bar0;

	val64 = xge_os_pio_mem_read64(lldev->pdev, hldev->regh0,
	    &bar0->general_int_status);
	retValue = (!val64) ? FILTER_STRAY : FILTER_SCHEDULE_THREAD;

	return retValue;
}

/******************************************
 * xge_intr
 * Parameters: Per adapter xgelldev_t
 * structure pointer
 * Return: None
 * Description: Interrupt service routine
 ******************************************/
void
xge_intr(void *plldev)
{
	xge_hal_status_e status;
	xgelldev_t       *lldev   = (xgelldev_t *)plldev;
	xge_hal_device_t *hldev   = (xge_hal_device_t *)lldev->devh;
	struct ifnet     *ifnetp  = lldev->ifnetp;

	mtx_lock(&lldev->xge_lock);
	if(ifnetp->if_drv_flags & IFF_DRV_RUNNING) {
	    status = xge_hal_device_handle_irq(hldev);

	    if(!(IFQ_DRV_IS_EMPTY(&ifnetp->if_snd))) {
	        xge_send_locked(ifnetp);
	    }
	}
	mtx_unlock(&lldev->xge_lock);
	return;
}

/********************************************
 * Function  :    xgell_rx_open
 * Parameters:  Queue index, channel
 *              open/close/reopen flag
 * Return:      0 or ENODEV
 * Description: Initialize and open all Rx
 *              channels.
 ******************************************/
int
xgell_rx_open(int qid, xgelldev_t *lldev, xge_hal_channel_reopen_e rflag)
{
	u64 adapter_status = 0x0;
	int retValue       = 0;
	xge_hal_status_e status_code;

	ENTER_FUNCTION

	xge_hal_channel_attr_t attr = {
	    .post_qid      = qid,
	    .compl_qid     = 0,
	    .callback      = xgell_rx_compl,
	    .per_dtr_space = sizeof(xgell_rx_priv_t),
	    .flags         = 0,
	    .type          = XGE_HAL_CHANNEL_TYPE_RING,
	    .userdata      = lldev,
	    .dtr_init      = xgell_rx_initial_replenish,
	    .dtr_term      = xgell_rx_term
	};

	/* If device is not ready, return */
	if(xge_hal_device_status(lldev->devh, &adapter_status)) {
	    xge_trace(XGE_ERR, "Device is not ready. Adapter status: 0x%llx",
	        (unsigned long long) adapter_status);
	    retValue = -ENODEV;
	    goto rxopen_out;
	}

	/* Open ring channel */
	status_code = xge_hal_channel_open(lldev->devh, &attr,
	    &lldev->ring_channel[qid], rflag);
	if(status_code != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Can not open Rx RING channel, Status: %d\n",
	        status_code);
	    retValue = -ENODEV;
	    goto rxopen_out;
	}

rxopen_out:
	LEAVE_FUNCTION

	return retValue;
}

/******************************************
 * Function:    xgell_tx_open
 * Parameters:  Channel
 *              open/close/reopen flag
 * Return:      0 or ENODEV
 * Description: Initialize and open all Tx
 *              channels.
 ******************************************/
int
xgell_tx_open(xgelldev_t *lldev, xge_hal_channel_reopen_e tflag)
{
	xge_hal_status_e status_code;
	u64 adapter_status = 0x0;
	int retValue       = 0;

	ENTER_FUNCTION

	xge_hal_channel_attr_t attr = {
	    .post_qid      = 0,
	    .compl_qid     = 0,
	    .callback      = xgell_tx_compl,
	    .per_dtr_space = sizeof(xgell_tx_priv_t),
	    .flags         = 0,
	    .type          = XGE_HAL_CHANNEL_TYPE_FIFO,
	    .userdata      = lldev,
	    .dtr_init      = xgell_tx_initial_replenish,
	    .dtr_term      = xgell_tx_term
	};

	/* If device is not ready, return */
	if(xge_hal_device_status(lldev->devh, &adapter_status)) {
	    xge_trace(XGE_ERR, "Device is not ready. Adapter status: 0x%llx\n",
	        (unsigned long long) adapter_status);
	    retValue = -ENODEV;
	    goto txopen_out;
	}

	/* Open FIFO channel */
	status_code = xge_hal_channel_open(lldev->devh, &attr,
	    &lldev->fifo_channel_0, tflag);
	if(status_code != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Can not open Tx FIFO channel, Status: %d\n",
	        status_code);
	    retValue = -ENODEV;
	    goto txopen_out;
	}

txopen_out:
	LEAVE_FUNCTION

	return retValue;
}

/******************************************
 * Function:    xgell_channel_open
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer
 * Return:      None
 * Description: Opens both Rx and Tx channels.
 ******************************************/
int
xgell_channel_open(xgelldev_t *lldev, xge_hal_channel_reopen_e option)
{
	int status   = XGE_HAL_OK;
	int index    = 0;
	int index2   = 0;

	ENTER_FUNCTION

	/* Open ring (Rx) channel */
	for(index = 0; index < XGE_RING_COUNT; index++) {
	    if((status = xgell_rx_open(index, lldev, option))) {
	        xge_trace(XGE_ERR, "Opening Rx channel failed (Status: %d)\n",
	            status);
	        for(index2 = 0; index2 < index; index2++) {
	            xge_hal_channel_close(lldev->ring_channel[index2], option);
	        }
	        return status;
	    }
	}
#ifdef XGE_FEATURE_LRO
	status = xge_hal_lro_init(1, lldev->devh);
	if (status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "cannot init Rx LRO got status code %d", status);
	    return -ENODEV;
	}
#endif

	/* Open FIFO (Tx) channel */
	if((status = xgell_tx_open(lldev, option))) {
	    xge_trace(XGE_ERR, "Opening Tx channel failed (Status: %d)\n",
	        status);
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        xge_hal_channel_close(lldev->ring_channel[index], option);
	    }
	}

	LEAVE_FUNCTION
	return status;
}

/******************************************
 * Function:    xgell_channel_close
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer
 * Return:      0 for success, non-zero for
 *              failure
 * Description: Closes both Tx and Rx channels
 ******************************************/
int
xgell_channel_close(xgelldev_t *lldev, xge_hal_channel_reopen_e option)
{
	int index;

	ENTER_FUNCTION

	DELAY(1000 * 1000);

	/* Close FIFO (Tx) channel */
	xge_hal_channel_close(lldev->fifo_channel_0, option);

	/* Close Ring (Rx) channel */
	for(index = 0; index < XGE_RING_COUNT; index++) {
	    xge_hal_channel_close(lldev->ring_channel[index], option);
	}

	LEAVE_FUNCTION

	return 0;
}


/******************************************
 * Function:    dmamap_cb
 * Parameters:  Parameter passed from dmamap
 *              function, Segment, Number of
 *              segments, error (if any)
 * Return:      None
 * Description: Callback function used for
 *              DMA mapping
 ******************************************/
void
dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if(!error) {
	    *(bus_addr_t *) arg = segs->ds_addr;
	}
}

/******************************************
 * Function:    xgell_reset
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer
 * Return:      HAL status code/EPERM
 * Description: Resets the device
 ******************************************/
void
xgell_reset(xgelldev_t *lldev)
{
	device_t dev = lldev->device;

	ENTER_FUNCTION

	xge_trace(XGE_TRACE, "Reseting the chip");

	mtx_lock(&lldev->xge_lock);

	/* If the device is not initialized, return */
	if(!lldev->initialized) {
	    goto xreset_out;
	}

	xge_terminate(dev, XGE_HAL_CHANNEL_OC_NORMAL);

	xge_initialize(dev, XGE_HAL_CHANNEL_OC_NORMAL);

xreset_out:
	LEAVE_FUNCTION
	mtx_unlock(&lldev->xge_lock);

	return;
}

/******************************************
 * Function:    xge_setmulti
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer
 * Return:      None
 * Description: Set an address as a multicast
 *              address
 ******************************************/
void
xge_setmulti(xgelldev_t *lldev)
{
	ENTER_FUNCTION
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
	    printf("Failed to %s multicast (status: %d)\n",
	        (ifnetp->if_flags & IFF_ALLMULTI ? "enable" : "disable"),
	        status);
	}

	/* Updating address list */
	IF_ADDR_LOCK(ifnetp);
	index = 0;
	TAILQ_FOREACH(ifma, &ifnetp->if_multiaddrs, ifma_link) {
	    if(ifma->ifma_addr->sa_family != AF_LINK) {
	        continue;
	    }
	    lladdr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	    index += 1;
	}
	IF_ADDR_UNLOCK(ifnetp);

	if((!lldev->all_multicast) && (index)) {
	    lldev->macaddr_count = (index + 1);
	    if(lldev->macaddr_count > table_size) {
	        return;
	    }

	    /* Clear old addresses */
	    for(index = 0; index < 48; index++) {
	        xge_hal_device_macaddr_set(hldev, (offset + index),
	            initial_addr);
	    }
	}

	/* Add new addresses */
	IF_ADDR_LOCK(ifnetp);
	index = 0;
	TAILQ_FOREACH(ifma, &ifnetp->if_multiaddrs, ifma_link) {
	    if(ifma->ifma_addr->sa_family != AF_LINK) {
	        continue;
	    }
	    lladdr = LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
	    xge_hal_device_macaddr_set(hldev, (offset + index), lladdr);
	    index += 1;
	}
	IF_ADDR_UNLOCK(ifnetp);

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xge_enable_promisc
 * Parameters:  Adapter structure
 * Return:      None
 * Description: Enables promiscuous mode
 ******************************************/
void
xge_enable_promisc(xgelldev_t *lldev)
{
	struct ifnet *ifnetp = lldev->ifnetp;
	xge_hal_device_t *hldev = lldev->devh;
	xge_hal_pci_bar0_t *bar0 = NULL;
	u64 val64 = 0;

	ENTER_FUNCTION
	
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

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xge_disable_promisc
 * Parameters:  Adapter structure
 * Return:      None
 * Description: Disables promiscuous mode
 ******************************************/
void
xge_disable_promisc(xgelldev_t *lldev)
{
	xge_hal_device_t *hldev = lldev->devh;
	xge_hal_pci_bar0_t *bar0 = NULL;
	u64 val64 = 0;

	ENTER_FUNCTION

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

	LEAVE_FUNCTION
}

/******************************************
 * Function:    changeMtu
 * Parameters:  Pointer to per-device
 *              xgelldev_t structure, New
 *              MTU size.
 * Return:      None
 * Description: Changes MTU size to requested
 ******************************************/
int
changeMtu(xgelldev_t *lldev, int NewMtu)
{
	struct ifnet *ifnetp = lldev->ifnetp;
	xge_hal_device_t *hldev = lldev->devh;
	int retValue = 0;

	ENTER_FUNCTION

	do {
	    /* Check requested MTU size for boundary */
	    if(xge_hal_device_mtu_check(hldev, NewMtu) != XGE_HAL_OK) {
	        xge_trace(XGE_ERR, "Invalid MTU");
	        retValue = EINVAL;
	        break;
	    }

	    if(lldev->initialized != 0) {
	        mtx_lock(&lldev->xge_lock);
	        if_down(ifnetp);
	        xge_stop(lldev);
	        ifnetp->if_mtu = NewMtu;
	        changeBufmode(lldev, NewMtu);
	        xge_init_locked((void *)lldev);
	        if_up(ifnetp);
	        mtx_unlock(&lldev->xge_lock);
	    }
	    else {
	        ifnetp->if_mtu = NewMtu;
	        changeBufmode(lldev, NewMtu);
	    }
	} while(FALSE);

	LEAVE_FUNCTION
	return retValue;
}

/******************************************
 * Function:    changeBufmode
 * Parameters:  Pointer to per-device
 *              xgelldev_t structure, New
 *              MTU size.
 * Return:      None
 * Description: Updates RingConfiguration structure
 *              depending the NewMtu size.
 ******************************************/
int
changeBufmode (xgelldev_t *lldev, int NewMtu)
{
	xge_hal_ring_config_t  * pRingConfig;
	xge_hal_device_t *hldev = lldev->devh;
	device_t dev = lldev->device;
	int buffer_length = 0, buffer_index = 0, index;
	
	pRingConfig = &(hldev->config.ring);
	xge_os_memzero(lldev->rxd_mbuf_len, sizeof(lldev->rxd_mbuf_len));

	if((NewMtu + XGE_HAL_MAC_HEADER_MAX_SIZE) <= MJUMPAGESIZE) {
#if defined(XGE_FEATURE_BUFFER_MODE_3)
	    xge_os_printf("%s: 3 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    }
	    pRingConfig->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_A;
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    lldev->rxd_mbuf_len[0]    = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1]    = XGE_HAL_TCPIP_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[2]    = NewMtu;
	    lldev->rxd_mbuf_cnt       = 3;
#else
#if defined(XGE_FEATURE_BUFFER_MODE_2)
	    xge_os_printf("%s: 2 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_3;
	    }
	    pRingConfig->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_B;
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_2;
	    lldev->rxd_mbuf_len[0]    = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1]    = NewMtu;
	    lldev->rxd_mbuf_cnt       = 2;
#else
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_1;
	    }
	    pRingConfig->scatter_mode = XGE_HAL_RING_QUEUE_SCATTER_MODE_A;
	    lldev->buffer_mode        = XGE_HAL_RING_QUEUE_BUFFER_MODE_1;
	    lldev->rxd_mbuf_len[0]    = NewMtu;
	    lldev->rxd_mbuf_cnt       = 1;
#endif
#endif
	}
	else {
#if defined(XGE_FEATURE_BUFFER_MODE_3) || defined (XGE_FEATURE_BUFFER_MODE_2)
	    xge_os_printf("2 or 3 Buffer mode is not supported for given MTU");
	    xge_os_printf("So changing buffer mode to 5 buffer mode\n");
#endif     
	    xge_os_printf("%s: 5 Buffer Mode Enabled",
	        device_get_nameunit(dev));
	    for(index = 0; index < XGE_RING_COUNT; index++) {
	        pRingConfig->queue[index].buffer_mode =
	            XGE_HAL_RING_QUEUE_BUFFER_MODE_5;
	    }
	    lldev->buffer_mode     = XGE_HAL_RING_QUEUE_BUFFER_MODE_5;
	    buffer_length          = NewMtu;
	    buffer_index           = 2;
	    lldev->rxd_mbuf_len[0] = XGE_HAL_MAC_HEADER_MAX_SIZE;
	    lldev->rxd_mbuf_len[1] = XGE_HAL_TCPIP_HEADER_MAX_SIZE;

	    while(buffer_length > MJUMPAGESIZE) {
	        buffer_length -= MJUMPAGESIZE;
	        lldev->rxd_mbuf_len[buffer_index] = MJUMPAGESIZE;
	        buffer_index++;
	    }

	    BUFALIGN(buffer_length);

	    lldev->rxd_mbuf_len[buffer_index] = buffer_length;
	    lldev->rxd_mbuf_cnt = buffer_index+1;
	}

	return XGE_HAL_OK;
}

/*************************************************************
 * xge_initialize
 *
 * @dev: Device structure
 * @option: Normal/Reset option for channels
 *
 * Called by both init and reset functions to enable device, interrupts, and to
 * open channels.
 *
 **************************************************************/
void xge_initialize(device_t dev, xge_hal_channel_reopen_e option)
{
	ENTER_FUNCTION

	struct ifaddr      *ifaddrp;
	struct sockaddr_dl *sockaddrp;
	unsigned char      *macaddr;
	xgelldev_t         *lldev    = (xgelldev_t *) device_get_softc(dev);
	xge_hal_device_t   *hldev    = lldev->devh;
	struct ifnet       *ifnetp   = lldev->ifnetp;
	int                 status   = XGE_HAL_OK;

	xge_trace(XGE_TRACE, "Set MTU size");
	status = xge_hal_device_mtu_set(hldev, ifnetp->if_mtu);
	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Setting HAL device MTU failed (Status: %d)",
	        status);
	    goto init_sub_out;
	}


	/* Enable HAL device */
	xge_hal_device_enable(hldev);

	/* Get MAC address and update in HAL */
	ifaddrp             = ifaddr_byindex(ifnetp->if_index);
	sockaddrp           = (struct sockaddr_dl *)ifaddrp->ifa_addr;
	sockaddrp->sdl_type = IFT_ETHER;
	sockaddrp->sdl_alen = ifnetp->if_addrlen;
	macaddr             = LLADDR(sockaddrp);
	xge_trace(XGE_TRACE,
	    "Setting MAC address: %02x:%02x:%02x:%02x:%02x:%02x\n",
	    *macaddr, *(macaddr + 1), *(macaddr + 2), *(macaddr + 3),
	    *(macaddr + 4), *(macaddr + 5));
	status = xge_hal_device_macaddr_set(hldev, 0, macaddr);
	if(status != XGE_HAL_OK) {
	    xge_trace(XGE_ERR,
	        "Setting MAC address failed (Status: %d)\n", status);
	}

	/* Opening channels */
	mtx_unlock(&lldev->xge_lock);
	status = xgell_channel_open(lldev, option);
	mtx_lock(&lldev->xge_lock);
	if(status != 0) {
	    goto init_sub_out;
	}

	/* Set appropriate flags */
	ifnetp->if_drv_flags  |=  IFF_DRV_RUNNING;
	ifnetp->if_flags &= ~IFF_DRV_OACTIVE;

	/* Checksum capability */
	ifnetp->if_hwassist = (ifnetp->if_capenable & IFCAP_TXCSUM) ?
	    (CSUM_TCP | CSUM_UDP) : 0;

#ifdef XGE_FEATURE_TSO
	if(ifnetp->if_capenable & IFCAP_TSO4)
	    ifnetp->if_hwassist |= CSUM_TSO;
#endif

	/* Enable interrupts */
	xge_hal_device_intr_enable(hldev);

	callout_reset(&lldev->timer, 10*hz, xge_timer, lldev);

	/* Disable promiscuous mode */
	xge_trace(XGE_TRACE, "If opted, enable promiscuous mode");
	xge_enable_promisc(lldev);

	/* Device is initialized */
	lldev->initialized = 1;
	xge_os_mdelay(1000);

init_sub_out:
	LEAVE_FUNCTION
	return;
}

/*******************************************************
 * xge_terminate
 *
 * @dev: Device structure
 * @option: Normal/Reset option for channels
 *
 * Called by both stop and reset functions to disable device, interrupts, and to
 * close channels.
 ******************************************************/
void xge_terminate(device_t dev, xge_hal_channel_reopen_e option)
{
	ENTER_FUNCTION

	xgelldev_t       *lldev  = (xgelldev_t *)device_get_softc(dev);
	xge_hal_device_t *hldev  = lldev->devh;
	struct ifnet     *ifnetp = lldev->ifnetp;

	/* Set appropriate flags */
	ifnetp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* Stop timer */
	callout_stop(&lldev->timer);

	/* Disable interrupts */
	xge_hal_device_intr_disable(hldev);

	mtx_unlock(&lldev->xge_lock);
	xge_queue_flush(xge_hal_device_queue(lldev->devh));
	mtx_lock(&lldev->xge_lock);

	/* Disable HAL device */
	if(xge_hal_device_disable(hldev) != XGE_HAL_OK) {
	    xge_trace(XGE_ERR, "Disabling HAL device failed");
	}

	/* Close Tx and Rx channels */
	xgell_channel_close(lldev, option);

	/* Reset HAL device */
	xge_hal_device_reset(hldev);

	xge_os_mdelay(1000);
	lldev->initialized = 0;

	if_link_state_change(ifnetp, LINK_STATE_DOWN);

	LEAVE_FUNCTION
}

/******************************************
 * Function:    xgell_set_mbuf_cflags
 * Parameters:  mbuf structure pointer
 * Return:      None
 * Description: This fuction will set the csum_flag of the mbuf
 ******************************************/
void xgell_set_mbuf_cflags(mbuf_t pkt)
{
	pkt->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
	pkt->m_pkthdr.csum_flags |= CSUM_IP_VALID;
	pkt->m_pkthdr.csum_flags |= (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
	pkt->m_pkthdr.csum_data = htons(0xffff);
}

#ifdef XGE_FEATURE_LRO
/******************************************
 * Function:    xgell_lro_flush_sessions
 * Parameters:  Per adapter xgelldev_t
 * Return:      None
 * Description: This function will flush the LRO session and send the
 *              accumulated LRO packet to Upper layer.
 ******************************************/
void xgell_lro_flush_sessions(xgelldev_t *lldev)
{
	lro_t *lro;
	struct ifnet *ifnetp = lldev->ifnetp;
	xge_hal_device_t *hldev = (xge_hal_device_t *)lldev->devh;

	while (NULL != (lro = xge_hal_lro_get_next_session(hldev))) {
	    xgell_set_mbuf_cflags(lro->os_buf);

	    /* Send it up */
	    mtx_unlock(&lldev->xge_lock);
	    (*ifnetp->if_input)(ifnetp, lro->os_buf);
	    mtx_lock(&lldev->xge_lock);

	    xge_hal_lro_close_session(lro);
	}
}

/******************************************
 * Function:    xgell_accumulate_large_rx
 * Parameters:  Descriptor info structure, current mbuf structure,
 *              packet length, Per adapter structure, Rx Desc private structure
 * Return:      None
 * Description: This function will accumulate packets to form the LRO
 *              packets based on various condition.
 ******************************************/
void xgell_accumulate_large_rx(xge_hal_dtr_info_t *ext_info,mbuf_t pkt,
	int pkt_length, xgelldev_t *lldev, xgell_rx_priv_t *rxd_priv)
{
	tcplro_t *tcp;
	lro_t *lro, *lro_end3;
	xge_hal_status_e status;
	unsigned char * temp;
	struct ifnet *ifnetp = lldev->ifnetp;

	status = xge_hal_accumulate_large_rx(pkt->m_data, &tcp, &pkt_length,
	    &lro, ext_info, lldev->devh, &lro_end3);
	pkt->m_next = NULL;
	temp = (unsigned char *)tcp;

	if(status == XGE_HAL_INF_LRO_BEGIN) {
	    pkt->m_flags |= M_PKTHDR;
	    pkt->m_pkthdr.rcvif = ifnetp;
	    lro->os_buf = lro->os_buf_end = pkt;
	}
	else if(status == XGE_HAL_INF_LRO_CONT) {
	    /*
	     * Current mbuf will be combine to form LRO frame,
	     * So mask the pkthdr of the flag variable for current mbuf
	     */
	    pkt->m_flags = pkt->m_flags & 0xFFFD; //Mask pkthdr
	    pkt->m_data = (u8 *)tcp;
	    pkt->m_len = pkt_length;

	    /*
	     * Combine the current mbuf to the LRO frame and update
	     * the LRO's pkthdr len accordingly
	     */
	    lro->os_buf_end->m_next = pkt;
	    lro->os_buf_end = pkt;
	    lro->os_buf->m_pkthdr.len += pkt_length;
	}
	else if(status == XGE_HAL_INF_LRO_END_2) {
	    lro->os_buf->m_flags |= M_EOR;

	    /* Update the Checksum flags of the LRO frames */
	    xgell_set_mbuf_cflags(lro->os_buf);

	    /* Post-Read sync */
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);

	    /*
	     * Current packet can not be combined with LRO frame.
	     * Flush the previous LRO frames and send the current packet
	     * seperately
	     */
	    mtx_unlock(&lldev->xge_lock);
	    (*ifnetp->if_input)(ifnetp, lro->os_buf);
	    (*ifnetp->if_input)(ifnetp, pkt);
	    mtx_lock(&lldev->xge_lock);
	    xge_hal_lro_close_session(lro);
	}
	else if(status == XGE_HAL_INF_LRO_END_1) {
	    pkt->m_flags = pkt->m_flags & 0xFFFD;
	    pkt->m_data = (u8 *)tcp;
	    pkt->m_len = pkt_length;
	    lro->os_buf_end->m_next = pkt;
	    lro->os_buf->m_pkthdr.len += pkt_length;
	    xgell_set_mbuf_cflags(lro->os_buf);
	    lro->os_buf->m_flags |= M_EOR;

	    /* Post-Read sync */
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);

	    /* Send it up */
	    mtx_unlock(&lldev->xge_lock);
	    (*ifnetp->if_input)(ifnetp, lro->os_buf);
	    mtx_lock(&lldev->xge_lock);

	    xge_hal_lro_close_session(lro);
	}
	else if(status == XGE_HAL_INF_LRO_END_3) {
	    pkt->m_flags |= M_PKTHDR;
	    pkt->m_len = pkt_length;
	    pkt->m_pkthdr.len = pkt_length;
	    lro_end3->os_buf = lro_end3->os_buf_end = pkt;
	    lro->os_buf->m_flags |= M_EOR;
	    xgell_set_mbuf_cflags(lro->os_buf);

	    /* Post-Read sync */
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);

	    /* Send it up */
	    mtx_unlock(&lldev->xge_lock);
	    (*ifnetp->if_input)(ifnetp, lro->os_buf);
	    mtx_lock(&lldev->xge_lock);
	    xge_hal_lro_close_session(lro);
	}
	else if((status == XGE_HAL_INF_LRO_UNCAPABLE) ||
	    (status == XGE_HAL_INF_LRO_SESSIONS_XCDED)) {
	    pkt->m_flags |= M_PKTHDR;
	    pkt->m_len = pkt_length;
	    pkt->m_pkthdr.len = pkt_length;

	    /* Post-Read sync */
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);

	    /* Send it up */
	    mtx_unlock(&lldev->xge_lock);
	    (*ifnetp->if_input)(ifnetp, pkt);
	    mtx_lock(&lldev->xge_lock);
	}
}
#endif

/******************************************
 * Function:    xgell_rx_compl
 * Parameters:  Channel handle, descriptor,
 *              transfer code, userdata
 *              (not used)
 * Return:      HAL status code
 * Description: If the interrupt is because
 *              of a received frame or if
 *              the receive ring contains
 *              fresh as yet un-processed
 *              frames, this function is
 *              called.
 ******************************************/
xge_hal_status_e
xgell_rx_compl(xge_hal_channel_h channelh, xge_hal_dtr_h dtr, u8 t_code,
	void *userdata)
{
	xge_hal_dtr_info_t ext_info;
	xge_hal_status_e   status_code;
	struct ifnet       *ifnetp;
	device_t           dev;
	int                index;
	mbuf_t             mbuf_up        = NULL;
	xgell_rx_priv_t    *rxd_priv      = NULL, old_rxd_priv;
	u16                vlan_tag;

//    ENTER_FUNCTION


	/*get the user data portion*/
	xgelldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(!lldev) {
	    xge_ctrace(XGE_TRACE, "xgeX: %s: Failed to get user data",
	        __FUNCTION__);
	    return XGE_HAL_FAIL;
	}
	dev = lldev->device;

	mtx_assert((&lldev->xge_lock), MA_OWNED);

	/* get the interface pointer */
	ifnetp = lldev->ifnetp;

	do {
	    if(!(ifnetp->if_drv_flags & IFF_DRV_RUNNING)) {
	        return XGE_HAL_FAIL;
	    }

	    if(t_code) {
	        xge_trace(XGE_TRACE, "Packet dropped because of %d", t_code);
	        xge_hal_device_handle_tcode(channelh, dtr, t_code);
	        xge_hal_ring_dtr_post(channelh,dtr);
	        continue;
	    }

	    /* Get the private data for this descriptor*/
	    rxd_priv = (xgell_rx_priv_t *) xge_hal_ring_dtr_private(channelh,
	        dtr);
	    if(!rxd_priv) {
	        xge_trace(XGE_ERR, "Failed to get descriptor private data");
	        return XGE_HAL_FAIL;
	    }

	    /* Taking backup of rxd_priv structure details of current packet */
	    xge_os_memcpy(&old_rxd_priv, rxd_priv, sizeof(xgell_rx_priv_t));

	    /* Prepare one buffer to send it to upper layer -- since the upper
	     * layer frees the buffer do not use rxd_priv->buffer
	     * Meanwhile prepare a new buffer, do mapping, use it in the
	     * current descriptor and post descriptor back to ring channel */
	    mbuf_up = rxd_priv->bufferArray[0];

	    /* Gets details of mbuf i.e., packet length */
	    xge_ring_dtr_get(mbuf_up, channelh, dtr, lldev, rxd_priv);

	    status_code =
	        (lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) ?
	        xgell_get_buf(dtr, rxd_priv, lldev, 0) :
	        xgell_get_buf_3b_5b(dtr, rxd_priv, lldev);

	    if(status_code != XGE_HAL_OK) {
	        xge_trace(XGE_ERR, "No memory");

	        /*
	         * Do not deliver the received buffer to the stack. Instead,
	         * Re-post the descriptor with the same buffer
	         */

	        /* Get back previous rxd_priv structure before posting */
	        xge_os_memcpy(rxd_priv, &old_rxd_priv, sizeof(xgell_rx_priv_t));

	        xge_hal_ring_dtr_post(channelh, dtr);
	        continue;
	    }

	    /* Get the extended information */
	    xge_hal_ring_dtr_info_get(channelh, dtr, &ext_info);

	    if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	        /*
	         * As we have allocated a new mbuf for this descriptor, post
	         * this descriptor with new mbuf back to ring channel
	         */
	        vlan_tag = ext_info.vlan;
	        xge_hal_ring_dtr_post(channelh, dtr);
	        if ((!(ext_info.proto & XGE_HAL_FRAME_PROTO_IP_FRAGMENTED) &&
	            (ext_info.proto & XGE_HAL_FRAME_PROTO_TCP_OR_UDP) &&
	            (ext_info.l3_cksum == XGE_HAL_L3_CKSUM_OK) &&
	            (ext_info.l4_cksum == XGE_HAL_L4_CKSUM_OK))) {
	            /* set Checksum Flag */
	            xgell_set_mbuf_cflags(mbuf_up);
#ifdef XGE_FEATURE_LRO
	            if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	                xgell_accumulate_large_rx(&ext_info, mbuf_up,
	                    mbuf_up->m_len, lldev, rxd_priv);
	            }
#else
	            /* Post-Read sync for buffers*/
	            bus_dmamap_sync(lldev->dma_tag_rx,
	                rxd_priv->dmainfo[0].dma_map, BUS_DMASYNC_POSTREAD);

	            /* Send it up */
	            mtx_unlock(&lldev->xge_lock);
	            (*ifnetp->if_input)(ifnetp, mbuf_up);
	            mtx_lock(&lldev->xge_lock);
#endif
	        }
	        else {
	            /*
	             * Packet with erroneous checksum , let the upper layer
	             * deal with it
	             */

	             /* Post-Read sync for buffers*/
	             bus_dmamap_sync(lldev->dma_tag_rx,
	                 rxd_priv->dmainfo[0].dma_map, BUS_DMASYNC_POSTREAD);

#ifdef XGE_FEATURE_LRO
	            xgell_lro_flush_sessions(lldev);
#endif

	            if (vlan_tag) {
	                mbuf_up->m_pkthdr.ether_vtag = vlan_tag;
	                    mbuf_up->m_flags |= M_VLANTAG;
	            }
	             /* Send it up */
	             mtx_unlock(&lldev->xge_lock);
	             (*ifnetp->if_input)(ifnetp, mbuf_up);
	             mtx_lock(&lldev->xge_lock);
	        }
	    }
	    else {
	        /*
	         * As we have allocated a new mbuf for this descriptor, post
	         * this descriptor with new mbuf back to ring channel
	         */
	        xge_hal_ring_dtr_post(channelh, dtr);
	        if ((!(ext_info.proto & XGE_HAL_FRAME_PROTO_IP_FRAGMENTED) &&
	            (ext_info.proto & XGE_HAL_FRAME_PROTO_TCP_OR_UDP) &&
	            (ext_info.l3_cksum == XGE_HAL_L3_CKSUM_OK) &&
	            (ext_info.l4_cksum == XGE_HAL_L4_CKSUM_OK))) {
	            /* set Checksum Flag */
	            xgell_set_mbuf_cflags(mbuf_up);
#ifdef XGE_FEATURE_LRO
	            if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	                xgell_accumulate_large_rx(&ext_info, mbuf_up,
	                    mbuf_up->m_len, lldev, rxd_priv);
	            }
#else
	            /* Post-Read sync for buffers*/
	            for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	                /* Post-Read sync */
	                bus_dmamap_sync(lldev->dma_tag_rx,
	                    rxd_priv->dmainfo[index].dma_map,
	                    BUS_DMASYNC_POSTREAD);
	            }

	            /* Send it up */
	            mtx_unlock(&lldev->xge_lock);
	            (*ifnetp->if_input)(ifnetp, mbuf_up);
	            mtx_lock(&lldev->xge_lock);
#endif
	        }
	        else {
	            /*
	             * Packet with erroneous checksum , let the upper layer
	             * deal with it
	             */
	            for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	                /* Post-Read sync */
	                bus_dmamap_sync(lldev->dma_tag_rx,
	                    rxd_priv->dmainfo[index].dma_map,
	                    BUS_DMASYNC_POSTREAD);
	            }

#ifdef XGE_FEATURE_LRO
	            xgell_lro_flush_sessions(lldev);
#endif
	            /* Send it up */
	            mtx_unlock(&lldev->xge_lock);
	            (*ifnetp->if_input)(ifnetp, mbuf_up);
	            mtx_lock(&lldev->xge_lock);
	        }
	    }
	} while(xge_hal_ring_dtr_next_completed(channelh, &dtr, &t_code)
	    == XGE_HAL_OK);
#ifdef XGE_FEATURE_LRO
	xgell_lro_flush_sessions(lldev);
#endif

//    LEAVE_FUNCTION

	return XGE_HAL_OK;
}

/******************************************
 * Function:    xge_ring_dtr_get
 * Parameters:  mbuf pointer, channel handler
 *              descriptot, Per adapter xgelldev_t
 *              structure pointer,
 *              Rx private structure
 * Return:      HAL status code
 * Description: Updates the mbuf lengths
 *              depending on packet lengths.
 ******************************************/
int
xge_ring_dtr_get(mbuf_t mbuf_up, xge_hal_channel_h channelh, xge_hal_dtr_h dtr,
	xgelldev_t *lldev, xgell_rx_priv_t *rxd_priv)
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


/******************************************
 * Function:    xge_send
 * Parameters:  Pointer to ifnet structure
 * Return:      None
 * Description: Transmit entry point
 ******************************************/
void
xge_send(struct ifnet *ifnetp)
{
	xgelldev_t *lldev = ifnetp->if_softc;

	mtx_lock(&lldev->xge_lock);
	xge_send_locked(ifnetp);
	mtx_unlock(&lldev->xge_lock);
}

void
xge_send_locked(struct ifnet *ifnetp)
{
	xge_hal_dtr_h            dtr;
	static bus_dma_segment_t segs[MAX_SEGS];
	xge_hal_status_e         status_code;
	unsigned int             max_fragments;
	xgelldev_t               *lldev          = ifnetp->if_softc;
	xge_hal_channel_h        channelh        = lldev->fifo_channel_0;
	mbuf_t                   m_head          = NULL;
	mbuf_t                   m_buf           = NULL;
	xgell_tx_priv_t          *ll_tx_priv     = NULL;
	register unsigned int    count           = 0;
	unsigned int             nsegs           = 0;
	u16                      vlan_tag;

	max_fragments = ((xge_hal_fifo_t *)channelh)->config->max_frags;

	mtx_assert((&lldev->xge_lock), MA_OWNED);

	/* If device is not initialized, return */
	if((!lldev->initialized) ||
	    (!(ifnetp->if_drv_flags & IFF_DRV_RUNNING))) {
	    xge_trace(XGE_ERR, "Device is not initialized");
	    return;
	}

	/*
	 * Get the number of free descriptors in the FIFO channel and return if
	 * the count is less than the XGELL_TX_LEVEL_LOW -- the low threshold
	 */
	count = xge_hal_channel_dtr_count(channelh);
	if(count <= XGELL_TX_LEVEL_LOW) {
	    ifnetp->if_drv_flags |= IFF_DRV_OACTIVE;
	    xge_trace(XGE_TRACE, "Free descriptor count %d/%d at low threshold",
	        count, XGELL_TX_LEVEL_LOW);

	    /* Serialized -- through queue */
	    xge_queue_produce_context(xge_hal_device_queue(lldev->devh),
	        XGE_LL_EVENT_TRY_XMIT_AGAIN, lldev);
	    return;
	}

	/* This loop will be executed for each packet in the kernel maintained
	 * queue -- each packet can be with fragments as an mbuf chain */
	while((ifnetp->if_snd.ifq_head) &&
	    (xge_hal_channel_dtr_count(channelh) > XGELL_TX_LEVEL_LOW)) {
	    IF_DEQUEUE(&ifnetp->if_snd, m_head);

	    for(count = 0, m_buf = m_head; m_buf != NULL;
	        m_buf = m_buf->m_next) {
	        if(m_buf->m_len) {
	            count += 1;
	        }
	    }

	    if(count >= max_fragments) {
	        m_buf = m_defrag(m_head, M_DONTWAIT);
	        if(m_buf != NULL) {
	            m_head = m_buf;
	        }
	    }

	    /* Reserve descriptors */
	    status_code = xge_hal_fifo_dtr_reserve(channelh, &dtr);
	    if(status_code) {
	        switch(status_code) {
	            case XGE_HAL_INF_CHANNEL_IS_NOT_READY:
	                xge_trace(XGE_ERR, "Channel is not ready");
	                break;

	            case XGE_HAL_INF_OUT_OF_DESCRIPTORS:
	                xge_trace(XGE_ERR, "Out of descriptors");
	                break;

	            default:
	                xge_trace(XGE_ERR,
	                    "Reserving (Tx) descriptors failed. Status %d",
	                    status_code);
	        }
	        goto out2;
	        break;
	    }

	    vlan_tag = (m_head->m_flags & M_VLANTAG) ? m_head->m_pkthdr.ether_vtag : 0;
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
	        xge_trace(XGE_ERR, "DMA map load with segments failed");
	        goto out2;
	    }

	    /* Set descriptor buffer for header and each fragment/segment */
	    count = 0;
	    do {
	        xge_hal_fifo_dtr_buffer_set(channelh, dtr, count,
	            (dma_addr_t)htole64(segs[count].ds_addr),
	            segs[count].ds_len);
	        count = count + 1;
	    } while(count < nsegs);

	    /* Pre-write Sync of mapping */
	    bus_dmamap_sync(lldev->dma_tag_tx, ll_tx_priv->dma_map,
	        BUS_DMASYNC_PREWRITE);

#ifdef XGE_FEATURE_TSO
	   if((m_head->m_pkthdr.csum_flags & CSUM_TSO) != 0) {
	       xge_hal_fifo_dtr_mss_set(dtr, m_head->m_pkthdr.tso_segsz);
	   }
#endif
	    /* Checksum */
	    if(ifnetp->if_hwassist > 0) {
	        xge_hal_fifo_dtr_cksum_set_bits(dtr, XGE_HAL_TXD_TX_CKO_IPV4_EN
	            | XGE_HAL_TXD_TX_CKO_TCP_EN | XGE_HAL_TXD_TX_CKO_UDP_EN);
	    }

	    /* Post descriptor to FIFO channel */
	    xge_hal_fifo_dtr_post(channelh, dtr);

	    /* Send the same copy of mbuf packet to BPF (Berkely Packet Filter)
	     * listener so that we can use tools like tcpdump */
	    ETHER_BPF_MTAP(ifnetp, m_head);
	}
	goto out1;
out2:
	/* Prepend the packet back to queue */
	IF_PREPEND(&ifnetp->if_snd, m_head);
out1:
	ifnetp->if_timer = 15;
}

/******************************************
 * Function:    xgell_get_buf
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer, descriptor,
 *              Rx private structure, rxd_priv buffer
 *              buffer index for mapping
 * Return:      HAL status code
 * Description: Gets buffer from system mbuf
 *              buffer pool.
 ******************************************/
int
xgell_get_buf(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
	xgelldev_t *lldev, int index)
{
	register mbuf_t mp            = NULL;
	struct          ifnet *ifnetp = lldev->ifnetp;
	int             retValue      = XGE_HAL_OK;
	bus_addr_t      paddr;
	int             BUFLEN = 0, CLUSTLEN = 0;

	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    CLUSTLEN = MJUMPAGESIZE;
	    BUFLEN   = MJUMPAGESIZE;
	}
	else {
	    BUFLEN = lldev->rxd_mbuf_len[index];
	    if(BUFLEN < MCLBYTES) {
	        CLUSTLEN = MCLBYTES;
	    }
	    else {
	        CLUSTLEN = MJUMPAGESIZE;
	    }
	}

	/* Get mbuf with attached cluster */
	mp = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, CLUSTLEN);
	if(!mp) {
	    xge_trace(XGE_ERR, "Out of memory to allocate mbuf");
	    retValue = XGE_HAL_FAIL;
	    goto getbuf_out;
	}

	/* Update mbuf's length, packet length and receive interface */
	mp->m_len = mp->m_pkthdr.len = BUFLEN;
	mp->m_pkthdr.rcvif = ifnetp;

	/* Unload DMA map of mbuf in current descriptor */
	bus_dmamap_unload(lldev->dma_tag_rx, rxd_priv->dmainfo[index].dma_map);

	/* Load DMA map */
	if(bus_dmamap_load(lldev->dma_tag_rx , rxd_priv->dmainfo[index].dma_map,
	    mtod(mp, void*), mp->m_len, dmamap_cb , &paddr , 0)) {
	    xge_trace(XGE_ERR, "Loading DMA map failed");
	    m_freem(mp);
	    retValue = XGE_HAL_FAIL;
	    goto getbuf_out;
	}

	/* Update descriptor private data */
	rxd_priv->bufferArray[index]         = mp;
	rxd_priv->dmainfo[index].dma_phyaddr = htole64(paddr);

	/* Pre-Read/Write sync */
	bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[index].dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Set descriptor buffer */
	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    xge_hal_ring_dtr_1b_set(dtrh, rxd_priv->dmainfo[0].dma_phyaddr,
	        MJUMPAGESIZE);
	}

getbuf_out:
	return retValue;
}

/******************************************
 * Function:    xgell_get_buf_3b_5b
 * Parameters:  Per adapter xgelldev_t
 *              structure pointer, descriptor,
 *              Rx private structure
 * Return:      HAL status code
 * Description: Gets buffers from system mbuf
 *              buffer pool.
 ******************************************/
int
xgell_get_buf_3b_5b(xge_hal_dtr_h dtrh, xgell_rx_priv_t *rxd_priv,
	xgelldev_t *lldev)
{
	bus_addr_t  dma_pointers[5];
	int         dma_sizes[5];
	int         retValue = XGE_HAL_OK, index;
	int         newindex = 0;

	for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	    retValue = xgell_get_buf(dtrh, rxd_priv, lldev, index);
	    if(retValue != XGE_HAL_OK) {
	        for(newindex = 0; newindex < index; newindex++) {
	            m_freem(rxd_priv->bufferArray[newindex]);
	        }
	        return retValue;
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

	return retValue;
}

/******************************************
 * Function:    xgell_tx_compl
 * Parameters:  Channel handle, descriptor,
 *              transfer code,
 *              userdata -> per adapter
 *              xgelldev_t structure as void *
 * Return:      HAL status code
 * Description: If an interrupt was raised
 *              to indicate DMA complete of
 *              the Tx packet, this function
 *              is called. It identifies the
 *              last TxD whose buffer was
 *              freed and frees all skbs
 *              whose data have already DMA'ed
 *              into the NICs internal memory.
 ******************************************/
xge_hal_status_e
xgell_tx_compl(xge_hal_channel_h channelh,
	xge_hal_dtr_h dtr, u8 t_code, void *userdata)
{
	xgell_tx_priv_t *ll_tx_priv;
	mbuf_t          m_buffer;
	xgelldev_t      *lldev  = (xgelldev_t *)userdata;
	struct ifnet    *ifnetp = lldev->ifnetp;

	ifnetp->if_timer = 0;

	/* For each completed descriptor: Get private structure, free buffer,
	 * do unmapping, and free descriptor */
	do {
	    if(t_code) {
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
	ifnetp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return XGE_HAL_OK;
}

/******************************************
 * Function:    xgell_tx_initial_replenish
 * Parameters:  Channel handle, descriptor,
 *              index (not used), userdata
 *              (not used), channel
 *              open/close/reopen option.
 * Return:      HAL status code
 * Description: Creates DMA maps to be used
 *              for Tx
 ******************************************/
xge_hal_status_e
xgell_tx_initial_replenish(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	int index, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xgell_tx_priv_t *txd_priv = NULL;
	int             retValue  = XGE_HAL_OK;
	device_t        dev = NULL;

	/* Get the user data portion from channel handle */
	xgelldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(lldev == NULL) {
	    xge_trace(XGE_ERR, "Failed to get user data");
	    retValue = XGE_HAL_FAIL;
	    goto txinit_out;
	}
	dev = lldev->device;

	/* Get the private data */
	txd_priv = (xgell_tx_priv_t *) xge_hal_fifo_dtr_private(dtrh);
	if(txd_priv == NULL) {
	    xge_trace(XGE_ERR, "Failed to get descriptor private data");
	    retValue = XGE_HAL_FAIL;
	    goto txinit_out;
	}

	/* Create DMA map for this descriptor */
	if(bus_dmamap_create(lldev->dma_tag_tx, BUS_DMA_NOWAIT,
	    &txd_priv->dma_map)) {
	    xge_trace(XGE_ERR, "DMA map creation for Tx descriptor failed");
	    retValue = XGE_HAL_FAIL;
	    goto txinit_out;
	}

txinit_out:
	return retValue;
}

/******************************************
 * Function:    xgell_rx_initial_replenish
 * Parameters:  Channel handle, descriptor,
 *              ring index, userdata
 *              (not used), channel
 *              open/close/reopen option.
 * Return:      HAL status code
 * Description: Replenish descriptor with
 *              rx_buffer in Rx buffer pool.
 ******************************************/
xge_hal_status_e
xgell_rx_initial_replenish(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	int index, void *userdata, xge_hal_channel_reopen_e reopen)
{
	xgell_rx_priv_t  *rxd_priv = NULL;
	int              retValue  = XGE_HAL_OK;
	struct ifnet     *ifnetp;
	device_t         dev;
	int              index1, index2;

	/* Get the user data portion from channel handle */
	xgelldev_t *lldev = xge_hal_channel_userdata(channelh);
	if(lldev == NULL) {
	    xge_ctrace(XGE_ERR, "xgeX: %s: Failed to get user data",
	        __FUNCTION__);
	    retValue = XGE_HAL_FAIL;
	    goto rxinit_out;
	}
	dev = lldev->device;

	/* Get the private data */
	rxd_priv = (xgell_rx_priv_t *) xge_hal_ring_dtr_private(channelh, dtrh);
	if(rxd_priv == NULL) {
	    xge_trace(XGE_ERR, "Failed to get descriptor private data");
	    retValue = XGE_HAL_FAIL;
	    goto rxinit_out;
	}

	rxd_priv->bufferArray =
	    malloc(((sizeof(rxd_priv->bufferArray)) * (lldev->rxd_mbuf_cnt)),
	    M_DEVBUF, M_NOWAIT);

	if(rxd_priv->bufferArray == NULL) {
	    xge_trace(XGE_ERR,
	        "Failed to allocate buffers for Rxd private structure");
	    retValue = XGE_HAL_FAIL;
	    goto rxinit_out;
	}

	ifnetp = lldev->ifnetp;

	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    /* Create DMA map for these descriptors*/
	    if(bus_dmamap_create(lldev->dma_tag_rx , BUS_DMA_NOWAIT,
	        &rxd_priv->dmainfo[0].dma_map)) {
	        xge_trace(XGE_ERR,
	            "DMA map creation for Rx descriptor failed");
	        retValue = XGE_HAL_FAIL;
	        goto rxinit_err_out;
	    }
	    /* Get a buffer, attach it to this descriptor */
	    retValue = xgell_get_buf(dtrh, rxd_priv, lldev, 0);
	}
	else {
	    for(index1 = 0; index1 < lldev->rxd_mbuf_cnt; index1++) {
	        /* Create DMA map for this descriptor */
	        if(bus_dmamap_create(lldev->dma_tag_rx , BUS_DMA_NOWAIT ,
	            &rxd_priv->dmainfo[index1].dma_map)) {
	            xge_trace(XGE_ERR,
	                "Jumbo DMA map creation for Rx descriptor failed");
	            for(index2 = index1 - 1; index2 >= 0; index2--) {
	                bus_dmamap_destroy(lldev->dma_tag_rx,
	                    rxd_priv->dmainfo[index2].dma_map);
	            }
	            retValue = XGE_HAL_FAIL;
	            goto rxinit_err_out;
	        }
	    }
	    retValue = xgell_get_buf_3b_5b(dtrh, rxd_priv, lldev);
	}

	if(retValue != XGE_HAL_OK) {
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
	free(rxd_priv->bufferArray,M_DEVBUF);
rxinit_out:
	return retValue;
}

/******************************************
 * Function:    xgell_rx_term
 * Parameters:  Channel handle, descriptor,
 *              descriptor state, userdata
 *              (not used), channel
 *              open/close/reopen option.
 * Return:      None
 * Description: Called by HAL to terminate
 *              all DTRs for ring channels.
 ******************************************/
void
xgell_rx_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtrh,
	xge_hal_dtr_state_e state, void *userdata,
	xge_hal_channel_reopen_e reopen)
{
	xgell_rx_priv_t *rxd_priv;
	xgelldev_t      *lldev;
	struct ifnet    *ifnetp;
	device_t        dev;
	int             index;

//    ENTER_FUNCTION

	/* Descriptor state is not "Posted" */
	if(state != XGE_HAL_DTR_STATE_POSTED) {
	    xge_ctrace(XGE_ERR, "xgeX: %s: Descriptor not posted\n",
	        __FUNCTION__);
	    goto rxterm_out;
	}

	/* Get the user data portion */
	lldev = xge_hal_channel_userdata(channelh);

	dev    = lldev->device;
	ifnetp = lldev->ifnetp;

	/* Get the private data */
	rxd_priv = (xgell_rx_priv_t *) xge_hal_ring_dtr_private(channelh, dtrh);

	if(lldev->buffer_mode == XGE_HAL_RING_QUEUE_BUFFER_MODE_1) {
	    /* Post-Read sync */
	    bus_dmamap_sync(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map,
	        BUS_DMASYNC_POSTREAD);

	    /* Do unmapping and destory DMA map */
	    bus_dmamap_unload(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map);
	    m_freem(rxd_priv->bufferArray[0]);
	    bus_dmamap_destroy(lldev->dma_tag_rx, rxd_priv->dmainfo[0].dma_map);
	}
	else {
	    for(index = 0; index < lldev->rxd_mbuf_cnt; index++) {
	        /* Post-Read sync */
	        bus_dmamap_sync(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map, BUS_DMASYNC_POSTREAD);

	        /* Do unmapping and destory DMA map */
	        bus_dmamap_unload(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map);

	        bus_dmamap_destroy(lldev->dma_tag_rx,
	            rxd_priv->dmainfo[index].dma_map);

	        /* Free the buffer */
	        m_free(rxd_priv->bufferArray[index]);
	    }
	}
	free(rxd_priv->bufferArray,M_DEVBUF);

	/* Free the descriptor */
	xge_hal_ring_dtr_free(channelh, dtrh);

rxterm_out:
//    LEAVE_FUNCTION
	return;
}


/******************************************
 * Function:    xgell_tx_term
 * Parameters:  Channel handle, descriptor,
 *              descriptor state, userdata
 *              (not used), channel
 *              open/close/reopen option.
 * Return:      None
 * Description: Called by HAL to terminate
 *              all DTRs for fifo channels.
 ******************************************/
void
xgell_tx_term(xge_hal_channel_h channelh, xge_hal_dtr_h dtr,
	xge_hal_dtr_state_e state, void *userdata,
	xge_hal_channel_reopen_e reopen)
{
	xgell_tx_priv_t *ll_tx_priv = xge_hal_fifo_dtr_private(dtr);
	xgelldev_t      *lldev      = (xgelldev_t *)userdata;

//    ENTER_FUNCTION

	/* Destroy DMA map */
	bus_dmamap_destroy(lldev->dma_tag_tx, ll_tx_priv->dma_map);

//    LEAVE_FUNCTION
}

/******************************************
 * xge_methods
 *
 * FreeBSD device interface entry points
 ******************************************/
static device_method_t xge_methods[] = {
	DEVMETHOD(device_probe,     xge_probe),
	DEVMETHOD(device_attach,    xge_attach),
	DEVMETHOD(device_detach,    xge_detach),
	DEVMETHOD(device_shutdown,  xge_shutdown),
	{0, 0}
};

static driver_t xge_driver = {
	"nxge",
	xge_methods,
	sizeof(xgelldev_t),
};
static devclass_t xge_devclass;
DRIVER_MODULE(nxge, pci, xge_driver, xge_devclass, 0, 0);
