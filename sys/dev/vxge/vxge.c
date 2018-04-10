/*-
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#include <dev/vxge/vxge.h>

static int vxge_pci_bd_no = -1;
static u32 vxge_drv_copyright = 0;
static u32 vxge_dev_ref_count = 0;
static u32 vxge_dev_req_reboot = 0;

static int vpath_selector[VXGE_HAL_MAX_VIRTUAL_PATHS] = \
{0, 1, 3, 3, 7, 7, 7, 7, 15, 15, 15, 15, 15, 15, 15, 15, 31};

/*
 * vxge_probe
 * Probes for x3100 devices
 */
int
vxge_probe(device_t ndev)
{
	int err = ENXIO;

	u16 pci_bd_no = 0;
	u16 pci_vendor_id = 0;
	u16 pci_device_id = 0;

	char adapter_name[64];

	pci_vendor_id = pci_get_vendor(ndev);
	if (pci_vendor_id != VXGE_PCI_VENDOR_ID)
		goto _exit0;

	pci_device_id = pci_get_device(ndev);

	if (pci_device_id == VXGE_PCI_DEVICE_ID_TITAN_1) {

		pci_bd_no = (pci_get_bus(ndev) | pci_get_slot(ndev));

		snprintf(adapter_name, sizeof(adapter_name),
		    VXGE_ADAPTER_NAME, pci_get_revid(ndev));
		device_set_desc_copy(ndev, adapter_name);

		if (!vxge_drv_copyright) {
			device_printf(ndev, VXGE_COPYRIGHT);
			vxge_drv_copyright = 1;
		}

		if (vxge_dev_req_reboot == 0) {
			vxge_pci_bd_no = pci_bd_no;
			err = BUS_PROBE_DEFAULT;
		} else {
			if (pci_bd_no != vxge_pci_bd_no) {
				vxge_pci_bd_no = pci_bd_no;
				err = BUS_PROBE_DEFAULT;
			}
		}
	}

_exit0:
	return (err);
}

/*
 * vxge_attach
 * Connects driver to the system if probe was success @ndev handle
 */
int
vxge_attach(device_t ndev)
{
	int err = 0;
	vxge_dev_t *vdev;
	vxge_hal_device_t *hldev = NULL;
	vxge_hal_device_attr_t device_attr;
	vxge_free_resources_e error_level = VXGE_FREE_NONE;

	vxge_hal_status_e status = VXGE_HAL_OK;

	/* Get per-ndev buffer */
	vdev = (vxge_dev_t *) device_get_softc(ndev);
	if (!vdev)
		goto _exit0;

	bzero(vdev, sizeof(vxge_dev_t));

	vdev->ndev = ndev;
	strlcpy(vdev->ndev_name, "vxge", sizeof(vdev->ndev_name));

	err = vxge_driver_config(vdev);
	if (err != 0)
		goto _exit0;

	/* Initialize HAL driver */
	status = vxge_driver_init(vdev);
	if (status != VXGE_HAL_OK) {
		device_printf(vdev->ndev, "Failed to initialize driver\n");
		goto _exit0;
	}
	/* Enable PCI bus-master */
	pci_enable_busmaster(ndev);

	/* Allocate resources */
	err = vxge_alloc_resources(vdev);
	if (err != 0) {
		device_printf(vdev->ndev, "resource allocation failed\n");
		goto _exit0;
	}

	err = vxge_device_hw_info_get(vdev);
	if (err != 0) {
		error_level = VXGE_FREE_BAR2;
		goto _exit0;
	}

	/* Get firmware default values for Device Configuration */
	vxge_hal_device_config_default_get(vdev->device_config);

	/* Customize Device Configuration based on User request */
	vxge_vpath_config(vdev);

	/* Allocate ISR resources */
	err = vxge_alloc_isr_resources(vdev);
	if (err != 0) {
		error_level = VXGE_FREE_ISR_RESOURCE;
		device_printf(vdev->ndev, "isr resource allocation failed\n");
		goto _exit0;
	}

	/* HAL attributes */
	device_attr.bar0 = (u8 *) vdev->pdev->bar_info[0];
	device_attr.bar1 = (u8 *) vdev->pdev->bar_info[1];
	device_attr.bar2 = (u8 *) vdev->pdev->bar_info[2];
	device_attr.regh0 = (vxge_bus_res_t *) vdev->pdev->reg_map[0];
	device_attr.regh1 = (vxge_bus_res_t *) vdev->pdev->reg_map[1];
	device_attr.regh2 = (vxge_bus_res_t *) vdev->pdev->reg_map[2];
	device_attr.irqh = (pci_irq_h) vdev->config.isr_info[0].irq_handle;
	device_attr.cfgh = vdev->pdev;
	device_attr.pdev = vdev->pdev;

	/* Initialize HAL Device */
	status = vxge_hal_device_initialize((vxge_hal_device_h *) &hldev,
	    &device_attr, vdev->device_config);
	if (status != VXGE_HAL_OK) {
		error_level = VXGE_FREE_ISR_RESOURCE;
		device_printf(vdev->ndev, "hal device initialization failed\n");
		goto _exit0;
	}

	vdev->devh = hldev;
	vxge_hal_device_private_set(hldev, vdev);

	if (vdev->is_privilaged) {
		err = vxge_firmware_verify(vdev);
		if (err != 0) {
			vxge_dev_req_reboot = 1;
			error_level = VXGE_FREE_TERMINATE_DEVICE;
			goto _exit0;
		}
	}

	/* Allocate memory for vpath */
	vdev->vpaths = (vxge_vpath_t *)
	    vxge_mem_alloc(vdev->no_of_vpath * sizeof(vxge_vpath_t));

	if (vdev->vpaths == NULL) {
		error_level = VXGE_FREE_TERMINATE_DEVICE;
		device_printf(vdev->ndev, "vpath memory allocation failed\n");
		goto _exit0;
	}

	vdev->no_of_func = 1;
	if (vdev->is_privilaged) {

		vxge_hal_func_mode_count(vdev->devh,
		    vdev->config.hw_info.function_mode, &vdev->no_of_func);

		vxge_bw_priority_config(vdev);
	}

	/* Initialize mutexes */
	vxge_mutex_init(vdev);

	/* Initialize Media */
	vxge_media_init(vdev);

	err = vxge_ifp_setup(ndev);
	if (err != 0) {
		error_level = VXGE_FREE_MEDIA;
		device_printf(vdev->ndev, "setting up interface failed\n");
		goto _exit0;
	}

	err = vxge_isr_setup(vdev);
	if (err != 0) {
		error_level = VXGE_FREE_INTERFACE;
		device_printf(vdev->ndev,
		    "failed to associate interrupt handler with device\n");
		goto _exit0;
	}
	vxge_device_hw_info_print(vdev);
	vdev->is_active = TRUE;

_exit0:
	if (error_level) {
		vxge_free_resources(ndev, error_level);
		err = ENXIO;
	}

	return (err);
}

/*
 * vxge_detach
 * Detaches driver from the Kernel subsystem
 */
int
vxge_detach(device_t ndev)
{
	vxge_dev_t *vdev;

	vdev = (vxge_dev_t *) device_get_softc(ndev);
	if (vdev->is_active) {
		vdev->is_active = FALSE;
		vxge_stop(vdev);
		vxge_free_resources(ndev, VXGE_FREE_ALL);
	}

	return (0);
}

/*
 * vxge_shutdown
 * To shutdown device before system shutdown
 */
int
vxge_shutdown(device_t ndev)
{
	vxge_dev_t *vdev = (vxge_dev_t *) device_get_softc(ndev);
	vxge_stop(vdev);
	return (0);
}

/*
 * vxge_init
 * Initialize the interface
 */
void
vxge_init(void *vdev_ptr)
{
	vxge_dev_t *vdev = (vxge_dev_t *) vdev_ptr;

	VXGE_DRV_LOCK(vdev);
	vxge_init_locked(vdev);
	VXGE_DRV_UNLOCK(vdev);
}

/*
 * vxge_init_locked
 * Initialize the interface
 */
void
vxge_init_locked(vxge_dev_t *vdev)
{
	int i, err = EINVAL;
	vxge_hal_device_t *hldev = vdev->devh;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_h vpath_handle;

	ifnet_t ifp = vdev->ifp;

	/* If device is in running state, initializing is not required */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		goto _exit0;

	VXGE_DRV_LOCK_ASSERT(vdev);

	/* Opening vpaths */
	err = vxge_vpath_open(vdev);
	if (err != 0)
		goto _exit1;

	if (vdev->config.rth_enable) {
		status = vxge_rth_config(vdev);
		if (status != VXGE_HAL_OK)
			goto _exit1;
	}

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		/* check initial mtu before enabling the device */
		status = vxge_hal_device_mtu_check(vpath_handle, ifp->if_mtu);
		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "invalid mtu size %u specified\n", ifp->if_mtu);
			goto _exit1;
		}

		status = vxge_hal_vpath_mtu_set(vpath_handle, ifp->if_mtu);
		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "setting mtu in device failed\n");
			goto _exit1;
		}
	}

	/* Enable HAL device */
	status = vxge_hal_device_enable(hldev);
	if (status != VXGE_HAL_OK) {
		device_printf(vdev->ndev, "failed to enable device\n");
		goto _exit1;
	}

	if (vdev->config.intr_mode == VXGE_HAL_INTR_MODE_MSIX)
		vxge_msix_enable(vdev);

	/* Checksum capability */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);

	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		/* Enabling mcast for all vpath */
		vxge_hal_vpath_mcast_enable(vpath_handle);

		/* Enabling bcast for all vpath */
		status = vxge_hal_vpath_bcast_enable(vpath_handle);
		if (status != VXGE_HAL_OK)
			device_printf(vdev->ndev,
			    "can't enable bcast on vpath (%d)\n", i);
	}

	/* Enable interrupts */
	vxge_hal_device_intr_enable(vdev->devh);

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		bzero(&(vdev->vpaths[i].driver_stats),
		    sizeof(vxge_drv_stats_t));
		status = vxge_hal_vpath_enable(vpath_handle);
		if (status != VXGE_HAL_OK)
			goto _exit2;
	}

	vxge_os_mdelay(1000);

	/* Device is initialized */
	vdev->is_initialized = TRUE;

	/* Now inform the stack we're ready */
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;

	goto _exit0;

_exit2:
	vxge_hal_device_intr_disable(vdev->devh);
	vxge_hal_device_disable(hldev);

_exit1:
	vxge_vpath_close(vdev);

_exit0:
	return;
}

/*
 * vxge_driver_init
 * Initializes HAL driver
 */
vxge_hal_status_e
vxge_driver_init(vxge_dev_t *vdev)
{
	vxge_hal_uld_cbs_t uld_callbacks;
	vxge_hal_driver_config_t driver_config;
	vxge_hal_status_e status = VXGE_HAL_OK;

	/* Initialize HAL driver */
	if (!vxge_dev_ref_count) {
		bzero(&uld_callbacks, sizeof(vxge_hal_uld_cbs_t));
		bzero(&driver_config, sizeof(vxge_hal_driver_config_t));

		uld_callbacks.link_up = vxge_link_up;
		uld_callbacks.link_down = vxge_link_down;
		uld_callbacks.crit_err = vxge_crit_error;
		uld_callbacks.sched_timer = NULL;
		uld_callbacks.xpak_alarm_log = NULL;

		status = vxge_hal_driver_initialize(&driver_config,
		    &uld_callbacks);
		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "failed to initialize driver\n");
			goto _exit0;
		}
	}
	vxge_hal_driver_debug_set(VXGE_TRACE);
	vxge_dev_ref_count++;

_exit0:
	return (status);
}

/*
 * vxge_driver_config
 */
int
vxge_driver_config(vxge_dev_t *vdev)
{
	int i, err = 0;
	char temp_buffer[30];

	vxge_bw_info_t bw_info;

	VXGE_GET_PARAM("hint.vxge.0.no_of_vpath", vdev->config,
	    no_of_vpath, VXGE_DEFAULT_USER_HARDCODED);

	if (vdev->config.no_of_vpath == VXGE_DEFAULT_USER_HARDCODED)
		vdev->config.no_of_vpath = mp_ncpus;

	if (vdev->config.no_of_vpath <= 0) {
		err = EINVAL;
		device_printf(vdev->ndev,
		    "Failed to load driver, \
		    invalid config : \'no_of_vpath\'\n");
		goto _exit0;
	}

	VXGE_GET_PARAM("hint.vxge.0.intr_coalesce", vdev->config,
	    intr_coalesce, VXGE_DEFAULT_CONFIG_DISABLE);

	VXGE_GET_PARAM("hint.vxge.0.rth_enable", vdev->config,
	    rth_enable, VXGE_DEFAULT_CONFIG_ENABLE);

	VXGE_GET_PARAM("hint.vxge.0.rth_bkt_sz", vdev->config,
	    rth_bkt_sz, VXGE_DEFAULT_RTH_BUCKET_SIZE);

	VXGE_GET_PARAM("hint.vxge.0.lro_enable", vdev->config,
	    lro_enable, VXGE_DEFAULT_CONFIG_ENABLE);

	VXGE_GET_PARAM("hint.vxge.0.tso_enable", vdev->config,
	    tso_enable, VXGE_DEFAULT_CONFIG_ENABLE);

	VXGE_GET_PARAM("hint.vxge.0.tx_steering", vdev->config,
	    tx_steering, VXGE_DEFAULT_CONFIG_DISABLE);

	VXGE_GET_PARAM("hint.vxge.0.msix_enable", vdev->config,
	    intr_mode, VXGE_HAL_INTR_MODE_MSIX);

	VXGE_GET_PARAM("hint.vxge.0.ifqmaxlen", vdev->config,
	    ifq_maxlen, VXGE_DEFAULT_CONFIG_IFQ_MAXLEN);

	VXGE_GET_PARAM("hint.vxge.0.port_mode", vdev->config,
	    port_mode, VXGE_DEFAULT_CONFIG_VALUE);

	if (vdev->config.port_mode == VXGE_DEFAULT_USER_HARDCODED)
		vdev->config.port_mode = VXGE_DEFAULT_CONFIG_VALUE;

	VXGE_GET_PARAM("hint.vxge.0.l2_switch", vdev->config,
	    l2_switch, VXGE_DEFAULT_CONFIG_VALUE);

	if (vdev->config.l2_switch == VXGE_DEFAULT_USER_HARDCODED)
		vdev->config.l2_switch = VXGE_DEFAULT_CONFIG_VALUE;

	VXGE_GET_PARAM("hint.vxge.0.fw_upgrade", vdev->config,
	    fw_option, VXGE_FW_UPGRADE_ALL);

	VXGE_GET_PARAM("hint.vxge.0.low_latency", vdev->config,
	    low_latency, VXGE_DEFAULT_CONFIG_DISABLE);

	VXGE_GET_PARAM("hint.vxge.0.func_mode", vdev->config,
	    function_mode, VXGE_DEFAULT_CONFIG_VALUE);

	if (vdev->config.function_mode == VXGE_DEFAULT_USER_HARDCODED)
		vdev->config.function_mode = VXGE_DEFAULT_CONFIG_VALUE;

	if (!(is_multi_func(vdev->config.function_mode) ||
	    is_single_func(vdev->config.function_mode)))
		vdev->config.function_mode = VXGE_DEFAULT_CONFIG_VALUE;

	for (i = 0; i < VXGE_HAL_MAX_FUNCTIONS; i++) {

		bw_info.func_id = i;

		sprintf(temp_buffer, "hint.vxge.0.bandwidth_%d", i);
		VXGE_GET_PARAM(temp_buffer, bw_info,
		    bandwidth, VXGE_DEFAULT_USER_HARDCODED);

		if (bw_info.bandwidth == VXGE_DEFAULT_USER_HARDCODED)
			bw_info.bandwidth = VXGE_HAL_VPATH_BW_LIMIT_DEFAULT;

		sprintf(temp_buffer, "hint.vxge.0.priority_%d", i);
		VXGE_GET_PARAM(temp_buffer, bw_info,
		    priority, VXGE_DEFAULT_USER_HARDCODED);

		if (bw_info.priority == VXGE_DEFAULT_USER_HARDCODED)
			bw_info.priority = VXGE_HAL_VPATH_PRIORITY_DEFAULT;

		vxge_os_memcpy(&vdev->config.bw_info[i], &bw_info,
		    sizeof(vxge_bw_info_t));
	}

_exit0:
	return (err);
}

/*
 * vxge_stop
 */
void
vxge_stop(vxge_dev_t *vdev)
{
	VXGE_DRV_LOCK(vdev);
	vxge_stop_locked(vdev);
	VXGE_DRV_UNLOCK(vdev);
}

/*
 * vxge_stop_locked
 * Common code for both stop and part of reset.
 * disables device, interrupts and closes vpaths handle
 */
void
vxge_stop_locked(vxge_dev_t *vdev)
{
	u64 adapter_status = 0;
	vxge_hal_status_e status;
	vxge_hal_device_t *hldev = vdev->devh;
	ifnet_t ifp = vdev->ifp;

	VXGE_DRV_LOCK_ASSERT(vdev);

	/* If device is not in "Running" state, return */
	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	/* Set appropriate flags */
	vdev->is_initialized = FALSE;
	hldev->link_state = VXGE_HAL_LINK_NONE;
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	if_link_state_change(ifp, LINK_STATE_DOWN);

	/* Disable interrupts */
	vxge_hal_device_intr_disable(hldev);

	/* Disable HAL device */
	status = vxge_hal_device_disable(hldev);
	if (status != VXGE_HAL_OK) {
		vxge_hal_device_status(hldev, &adapter_status);
		device_printf(vdev->ndev,
		    "adapter status: 0x%llx\n", adapter_status);
	}

	/* reset vpaths */
	vxge_vpath_reset(vdev);

	vxge_os_mdelay(1000);

	/* Close Vpaths */
	vxge_vpath_close(vdev);
}

void
vxge_send(ifnet_t ifp)
{
	vxge_vpath_t *vpath;
	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;

	vpath = &(vdev->vpaths[0]);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		if (VXGE_TX_TRYLOCK(vpath)) {
			vxge_send_locked(ifp, vpath);
			VXGE_TX_UNLOCK(vpath);
		}
	}
}

static inline void
vxge_send_locked(ifnet_t ifp, vxge_vpath_t *vpath)
{
	mbuf_t m_head = NULL;
	vxge_dev_t *vdev = vpath->vdev;

	VXGE_TX_LOCK_ASSERT(vpath);

	if ((!vdev->is_initialized) ||
	    ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING))
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (vxge_xmit(ifp, vpath, &m_head)) {
			if (m_head == NULL)
				break;

			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			VXGE_DRV_STATS(vpath, tx_again);
			break;
		}
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);
	}
}

#if __FreeBSD_version >= 800000

int
vxge_mq_send(ifnet_t ifp, mbuf_t m_head)
{
	int i = 0, err = 0;

	vxge_vpath_t *vpath;
	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;

	if (vdev->config.tx_steering) {
		i = vxge_vpath_get(vdev, m_head);
	} else if (M_HASHTYPE_GET(m_head) != M_HASHTYPE_NONE) {
		i = m_head->m_pkthdr.flowid % vdev->no_of_vpath;
	}

	vpath = &(vdev->vpaths[i]);
	if (VXGE_TX_TRYLOCK(vpath)) {
		err = vxge_mq_send_locked(ifp, vpath, m_head);
		VXGE_TX_UNLOCK(vpath);
	} else
		err = drbr_enqueue(ifp, vpath->br, m_head);

	return (err);
}

static inline int
vxge_mq_send_locked(ifnet_t ifp, vxge_vpath_t *vpath, mbuf_t m_head)
{
	int err = 0;
	mbuf_t next = NULL;
	vxge_dev_t *vdev = vpath->vdev;

	VXGE_TX_LOCK_ASSERT(vpath);

	if ((!vdev->is_initialized) ||
	    ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)) {
		err = drbr_enqueue(ifp, vpath->br, m_head);
		goto _exit0;
	}
	if (m_head == NULL) {
		next = drbr_dequeue(ifp, vpath->br);
	} else if (drbr_needs_enqueue(ifp, vpath->br)) {
		if ((err = drbr_enqueue(ifp, vpath->br, m_head)) != 0)
			goto _exit0;
		next = drbr_dequeue(ifp, vpath->br);
	} else
		next = m_head;

	/* Process the queue */
	while (next != NULL) {
		if ((err = vxge_xmit(ifp, vpath, &next)) != 0) {
			if (next == NULL)
				break;

			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			err = drbr_enqueue(ifp, vpath->br, next);
			VXGE_DRV_STATS(vpath, tx_again);
			break;
		}
		if_inc_counter(ifp, IFCOUNTER_OBYTES, next->m_pkthdr.len);
		if (next->m_flags & M_MCAST)
			if_inc_counter(ifp, IFCOUNTER_OMCASTS, 1);

		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		next = drbr_dequeue(ifp, vpath->br);
	}

_exit0:
	return (err);
}

void
vxge_mq_qflush(ifnet_t ifp)
{
	int i;
	mbuf_t m_head;
	vxge_vpath_t *vpath;

	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath = &(vdev->vpaths[i]);
		if (!vpath->handle)
			continue;

		VXGE_TX_LOCK(vpath);
		while ((m_head = buf_ring_dequeue_sc(vpath->br)) != NULL)
			vxge_free_packet(m_head);

		VXGE_TX_UNLOCK(vpath);
	}
	if_qflush(ifp);
}
#endif

static inline int
vxge_xmit(ifnet_t ifp, vxge_vpath_t *vpath, mbuf_t *m_headp)
{
	int err, num_segs = 0;
	u32 txdl_avail, dma_index, tagged = 0;

	dma_addr_t dma_addr;
	bus_size_t dma_sizes;

	void *dtr_priv;
	vxge_txdl_priv_t *txdl_priv;
	vxge_hal_txdl_h txdlh;
	vxge_hal_status_e status;
	vxge_dev_t *vdev = vpath->vdev;

	VXGE_DRV_STATS(vpath, tx_xmit);

	txdl_avail = vxge_hal_fifo_free_txdl_count_get(vpath->handle);
	if (txdl_avail < VXGE_TX_LOW_THRESHOLD) {

		VXGE_DRV_STATS(vpath, tx_low_dtr_cnt);
		err = ENOBUFS;
		goto _exit0;
	}

	/* Reserve descriptors */
	status = vxge_hal_fifo_txdl_reserve(vpath->handle, &txdlh, &dtr_priv);
	if (status != VXGE_HAL_OK) {
		VXGE_DRV_STATS(vpath, tx_reserve_failed);
		err = ENOBUFS;
		goto _exit0;
	}

	/* Update Tx private structure for this descriptor */
	txdl_priv = (vxge_txdl_priv_t *) dtr_priv;

	/*
	 * Map the packet for DMA.
	 * Returns number of segments through num_segs.
	 */
	err = vxge_dma_mbuf_coalesce(vpath->dma_tag_tx, txdl_priv->dma_map,
	    m_headp, txdl_priv->dma_buffers, &num_segs);

	if (vpath->driver_stats.tx_max_frags < num_segs)
		vpath->driver_stats.tx_max_frags = num_segs;

	if (err == ENOMEM) {
		VXGE_DRV_STATS(vpath, tx_no_dma_setup);
		vxge_hal_fifo_txdl_free(vpath->handle, txdlh);
		goto _exit0;
	} else if (err != 0) {
		vxge_free_packet(*m_headp);
		VXGE_DRV_STATS(vpath, tx_no_dma_setup);
		vxge_hal_fifo_txdl_free(vpath->handle, txdlh);
		goto _exit0;
	}

	txdl_priv->mbuf_pkt = *m_headp;

	/* Set VLAN tag in descriptor only if this packet has it */
	if ((*m_headp)->m_flags & M_VLANTAG)
		vxge_hal_fifo_txdl_vlan_set(txdlh,
		    (*m_headp)->m_pkthdr.ether_vtag);

	/* Set descriptor buffer for header and each fragment/segment */
	for (dma_index = 0; dma_index < num_segs; dma_index++) {

		dma_sizes = txdl_priv->dma_buffers[dma_index].ds_len;
		dma_addr = htole64(txdl_priv->dma_buffers[dma_index].ds_addr);

		vxge_hal_fifo_txdl_buffer_set(vpath->handle, txdlh, dma_index,
		    dma_addr, dma_sizes);
	}

	/* Pre-write Sync of mapping */
	bus_dmamap_sync(vpath->dma_tag_tx, txdl_priv->dma_map,
	    BUS_DMASYNC_PREWRITE);

	if ((*m_headp)->m_pkthdr.csum_flags & CSUM_TSO) {
		if ((*m_headp)->m_pkthdr.tso_segsz) {
			VXGE_DRV_STATS(vpath, tx_tso);
			vxge_hal_fifo_txdl_lso_set(txdlh,
			    VXGE_HAL_FIFO_LSO_FRM_ENCAP_AUTO,
			    (*m_headp)->m_pkthdr.tso_segsz);
		}
	}

	/* Checksum */
	if (ifp->if_hwassist > 0) {
		vxge_hal_fifo_txdl_cksum_set_bits(txdlh,
		    VXGE_HAL_FIFO_TXD_TX_CKO_IPV4_EN |
		    VXGE_HAL_FIFO_TXD_TX_CKO_TCP_EN |
		    VXGE_HAL_FIFO_TXD_TX_CKO_UDP_EN);
	}

	if ((vxge_hal_device_check_id(vdev->devh) == VXGE_HAL_CARD_TITAN_1A) &&
	    (vdev->hw_fw_version >= VXGE_FW_VERSION(1, 8, 0)))
		tagged = 1;

	vxge_hal_fifo_txdl_post(vpath->handle, txdlh, tagged);
	VXGE_DRV_STATS(vpath, tx_posted);

_exit0:
	return (err);
}

/*
 * vxge_tx_replenish
 * Allocate buffers and set them into descriptors for later use
 */
/* ARGSUSED */
vxge_hal_status_e
vxge_tx_replenish(vxge_hal_vpath_h vpath_handle, vxge_hal_txdl_h txdlh,
    void *dtr_priv, u32 dtr_index, void *userdata, vxge_hal_reopen_e reopen)
{
	int err = 0;

	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_txdl_priv_t *txdl_priv = (vxge_txdl_priv_t *) dtr_priv;

	err = bus_dmamap_create(vpath->dma_tag_tx, BUS_DMA_NOWAIT,
	    &txdl_priv->dma_map);

	return ((err == 0) ? VXGE_HAL_OK : VXGE_HAL_FAIL);
}

/*
 * vxge_tx_compl
 * If the interrupt is due to Tx completion, free the sent buffer
 */
vxge_hal_status_e
vxge_tx_compl(vxge_hal_vpath_h vpath_handle, vxge_hal_txdl_h txdlh,
    void *dtr_priv, vxge_hal_fifo_tcode_e t_code, void *userdata)
{
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_txdl_priv_t *txdl_priv;
	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_dev_t *vdev = vpath->vdev;

	ifnet_t ifp = vdev->ifp;

	VXGE_TX_LOCK(vpath);

	/*
	 * For each completed descriptor
	 * Get private structure, free buffer, do unmapping, and free descriptor
	 */

	do {
		VXGE_DRV_STATS(vpath, tx_compl);
		if (t_code != VXGE_HAL_FIFO_T_CODE_OK) {
			device_printf(vdev->ndev, "tx transfer code %d\n",
			    t_code);

			if_inc_counter(ifp, IFCOUNTER_OERRORS, 1);
			VXGE_DRV_STATS(vpath, tx_tcode);
			vxge_hal_fifo_handle_tcode(vpath_handle, txdlh, t_code);
		}
		if_inc_counter(ifp, IFCOUNTER_OPACKETS, 1);
		txdl_priv = (vxge_txdl_priv_t *) dtr_priv;

		bus_dmamap_unload(vpath->dma_tag_tx, txdl_priv->dma_map);

		vxge_free_packet(txdl_priv->mbuf_pkt);
		vxge_hal_fifo_txdl_free(vpath->handle, txdlh);

	} while (vxge_hal_fifo_txdl_next_completed(vpath_handle, &txdlh,
	    &dtr_priv, &t_code) == VXGE_HAL_OK);


	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	VXGE_TX_UNLOCK(vpath);

	return (status);
}

/* ARGSUSED */
void
vxge_tx_term(vxge_hal_vpath_h vpath_handle, vxge_hal_txdl_h txdlh,
    void *dtr_priv, vxge_hal_txdl_state_e state,
    void *userdata, vxge_hal_reopen_e reopen)
{
	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_txdl_priv_t *txdl_priv = (vxge_txdl_priv_t *) dtr_priv;

	if (state != VXGE_HAL_TXDL_STATE_POSTED)
		return;

	if (txdl_priv != NULL) {
		bus_dmamap_sync(vpath->dma_tag_tx, txdl_priv->dma_map,
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(vpath->dma_tag_tx, txdl_priv->dma_map);
		bus_dmamap_destroy(vpath->dma_tag_tx, txdl_priv->dma_map);
		vxge_free_packet(txdl_priv->mbuf_pkt);
	}

	/* Free the descriptor */
	vxge_hal_fifo_txdl_free(vpath->handle, txdlh);
}

/*
 * vxge_rx_replenish
 * Allocate buffers and set them into descriptors for later use
 */
/* ARGSUSED */
vxge_hal_status_e
vxge_rx_replenish(vxge_hal_vpath_h vpath_handle, vxge_hal_rxd_h rxdh,
    void *dtr_priv, u32 dtr_index, void *userdata, vxge_hal_reopen_e reopen)
{
	int err = 0;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_rxd_priv_t *rxd_priv = (vxge_rxd_priv_t *) dtr_priv;

	/* Create DMA map for these descriptors */
	err = bus_dmamap_create(vpath->dma_tag_rx, BUS_DMA_NOWAIT,
	    &rxd_priv->dma_map);
	if (err == 0) {
		if (vxge_rx_rxd_1b_set(vpath, rxdh, dtr_priv)) {
			bus_dmamap_destroy(vpath->dma_tag_rx,
			    rxd_priv->dma_map);
			status = VXGE_HAL_FAIL;
		}
	}

	return (status);
}

/*
 * vxge_rx_compl
 */
vxge_hal_status_e
vxge_rx_compl(vxge_hal_vpath_h vpath_handle, vxge_hal_rxd_h rxdh,
    void *dtr_priv, u8 t_code, void *userdata)
{
	mbuf_t mbuf_up;

	vxge_rxd_priv_t *rxd_priv;
	vxge_hal_ring_rxd_info_t ext_info;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_dev_t *vdev = vpath->vdev;

	struct lro_ctrl *lro = &vpath->lro;

	/* get the interface pointer */
	ifnet_t ifp = vdev->ifp;

	do {
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
			vxge_hal_ring_rxd_post(vpath_handle, rxdh);
			status = VXGE_HAL_FAIL;
			break;
		}

		VXGE_DRV_STATS(vpath, rx_compl);
		rxd_priv = (vxge_rxd_priv_t *) dtr_priv;

		/* Gets details of mbuf i.e., packet length */
		vxge_rx_rxd_1b_get(vpath, rxdh, dtr_priv);

		/*
		 * Prepare one buffer to send it to upper layer Since upper
		 * layer frees the buffer do not use rxd_priv->mbuf_pkt.
		 * Meanwhile prepare a new buffer, do mapping, use with the
		 * current descriptor and post descriptor back to ring vpath
		 */
		mbuf_up = rxd_priv->mbuf_pkt;
		if (t_code != VXGE_HAL_RING_RXD_T_CODE_OK) {

			if_inc_counter(ifp, IFCOUNTER_IERRORS, 1);
			VXGE_DRV_STATS(vpath, rx_tcode);
			status = vxge_hal_ring_handle_tcode(vpath_handle,
			    rxdh, t_code);

			/*
			 * If transfer code is not for unknown protocols and
			 * vxge_hal_device_handle_tcode is NOT returned
			 * VXGE_HAL_OK
			 * drop this packet and increment rx_tcode stats
			 */
			if ((status != VXGE_HAL_OK) &&
			    (t_code != VXGE_HAL_RING_T_CODE_L3_PKT_ERR)) {

				vxge_free_packet(mbuf_up);
				vxge_hal_ring_rxd_post(vpath_handle, rxdh);
				continue;
			}
		}

		if (vxge_rx_rxd_1b_set(vpath, rxdh, dtr_priv)) {
			/*
			 * If unable to allocate buffer, post descriptor back
			 * to vpath for future processing of same packet.
			 */
			vxge_hal_ring_rxd_post(vpath_handle, rxdh);
			continue;
		}

		/* Get the extended information */
		vxge_hal_ring_rxd_1b_info_get(vpath_handle, rxdh, &ext_info);

		/* post descriptor with newly allocated mbuf back to vpath */
		vxge_hal_ring_rxd_post(vpath_handle, rxdh);
		vpath->rxd_posted++;

		if (vpath->rxd_posted % VXGE_RXD_REPLENISH_COUNT == 0)
			vxge_hal_ring_rxd_post_post_db(vpath_handle);

		/*
		 * Set successfully computed checksums in the mbuf.
		 * Leave the rest to the stack to be reverified.
		 */
		vxge_rx_checksum(ext_info, mbuf_up);

#if __FreeBSD_version >= 800000
		M_HASHTYPE_SET(mbuf_up, M_HASHTYPE_OPAQUE);
		mbuf_up->m_pkthdr.flowid = vpath->vp_index;
#endif
		/* Post-Read sync for buffers */
		bus_dmamap_sync(vpath->dma_tag_rx, rxd_priv->dma_map,
		    BUS_DMASYNC_POSTREAD);

		vxge_rx_input(ifp, mbuf_up, vpath);

	} while (vxge_hal_ring_rxd_next_completed(vpath_handle, &rxdh,
	    &dtr_priv, &t_code) == VXGE_HAL_OK);

	/* Flush any outstanding LRO work */
	if (vpath->lro_enable && vpath->lro.lro_cnt)
		tcp_lro_flush_all(lro);

	return (status);
}

static inline void
vxge_rx_input(ifnet_t ifp, mbuf_t mbuf_up, vxge_vpath_t *vpath)
{
	if (vpath->lro_enable && vpath->lro.lro_cnt) {
		if (tcp_lro_rx(&vpath->lro, mbuf_up, 0) == 0)
			return;
	}
	(*ifp->if_input) (ifp, mbuf_up);
}

static inline void
vxge_rx_checksum(vxge_hal_ring_rxd_info_t ext_info, mbuf_t mbuf_up)
{

	if (!(ext_info.proto & VXGE_HAL_FRAME_PROTO_IP_FRAG) &&
	    (ext_info.proto & VXGE_HAL_FRAME_PROTO_TCP_OR_UDP) &&
	    ext_info.l3_cksum_valid && ext_info.l4_cksum_valid) {

		mbuf_up->m_pkthdr.csum_data = htons(0xffff);

		mbuf_up->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
		mbuf_up->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		mbuf_up->m_pkthdr.csum_flags |=
		    (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);

	} else {

		if (ext_info.vlan) {
			mbuf_up->m_pkthdr.ether_vtag = ext_info.vlan;
			mbuf_up->m_flags |= M_VLANTAG;
		}
	}
}

/*
 * vxge_rx_term During unload terminate and free all descriptors
 * @vpath_handle Rx vpath Handle @rxdh Rx Descriptor Handle @state Descriptor
 * State @userdata Per-adapter Data @reopen vpath open/reopen option
 */
/* ARGSUSED */
void
vxge_rx_term(vxge_hal_vpath_h vpath_handle, vxge_hal_rxd_h rxdh,
    void *dtr_priv, vxge_hal_rxd_state_e state, void *userdata,
    vxge_hal_reopen_e reopen)
{
	vxge_vpath_t *vpath = (vxge_vpath_t *) userdata;
	vxge_rxd_priv_t *rxd_priv = (vxge_rxd_priv_t *) dtr_priv;

	if (state != VXGE_HAL_RXD_STATE_POSTED)
		return;

	if (rxd_priv != NULL) {
		bus_dmamap_sync(vpath->dma_tag_rx, rxd_priv->dma_map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(vpath->dma_tag_rx, rxd_priv->dma_map);
		bus_dmamap_destroy(vpath->dma_tag_rx, rxd_priv->dma_map);

		vxge_free_packet(rxd_priv->mbuf_pkt);
	}
	/* Free the descriptor */
	vxge_hal_ring_rxd_free(vpath_handle, rxdh);
}

/*
 * vxge_rx_rxd_1b_get
 * Get descriptors of packet to send up
 */
void
vxge_rx_rxd_1b_get(vxge_vpath_t *vpath, vxge_hal_rxd_h rxdh, void *dtr_priv)
{
	vxge_rxd_priv_t *rxd_priv = (vxge_rxd_priv_t *) dtr_priv;
	mbuf_t mbuf_up = rxd_priv->mbuf_pkt;

	/* Retrieve data from completed descriptor */
	vxge_hal_ring_rxd_1b_get(vpath->handle, rxdh, &rxd_priv->dma_addr[0],
	    (u32 *) &rxd_priv->dma_sizes[0]);

	/* Update newly created buffer to be sent up with packet length */
	mbuf_up->m_len = rxd_priv->dma_sizes[0];
	mbuf_up->m_pkthdr.len = rxd_priv->dma_sizes[0];
	mbuf_up->m_next = NULL;
}

/*
 * vxge_rx_rxd_1b_set
 * Allocates new mbufs to be placed into descriptors
 */
int
vxge_rx_rxd_1b_set(vxge_vpath_t *vpath, vxge_hal_rxd_h rxdh, void *dtr_priv)
{
	int num_segs, err = 0;

	mbuf_t mbuf_pkt;
	bus_dmamap_t dma_map;
	bus_dma_segment_t dma_buffers[1];
	vxge_rxd_priv_t *rxd_priv = (vxge_rxd_priv_t *) dtr_priv;

	vxge_dev_t *vdev = vpath->vdev;

	mbuf_pkt = m_getjcl(M_NOWAIT, MT_DATA, M_PKTHDR, vdev->rx_mbuf_sz);
	if (!mbuf_pkt) {
		err = ENOBUFS;
		VXGE_DRV_STATS(vpath, rx_no_buf);
		device_printf(vdev->ndev, "out of memory to allocate mbuf\n");
		goto _exit0;
	}

	/* Update mbuf's length, packet length and receive interface */
	mbuf_pkt->m_len = vdev->rx_mbuf_sz;
	mbuf_pkt->m_pkthdr.len = vdev->rx_mbuf_sz;
	mbuf_pkt->m_pkthdr.rcvif = vdev->ifp;

	/* Load DMA map */
	err = vxge_dma_mbuf_coalesce(vpath->dma_tag_rx, vpath->extra_dma_map,
	    &mbuf_pkt, dma_buffers, &num_segs);
	if (err != 0) {
		VXGE_DRV_STATS(vpath, rx_map_fail);
		vxge_free_packet(mbuf_pkt);
		goto _exit0;
	}

	/* Unload DMA map of mbuf in current descriptor */
	bus_dmamap_sync(vpath->dma_tag_rx, rxd_priv->dma_map,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(vpath->dma_tag_rx, rxd_priv->dma_map);

	/* Update descriptor private data */
	dma_map = rxd_priv->dma_map;
	rxd_priv->mbuf_pkt = mbuf_pkt;
	rxd_priv->dma_addr[0] = htole64(dma_buffers->ds_addr);
	rxd_priv->dma_map = vpath->extra_dma_map;
	vpath->extra_dma_map = dma_map;

	/* Pre-Read/Write sync */
	bus_dmamap_sync(vpath->dma_tag_rx, rxd_priv->dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Set descriptor buffer */
	vxge_hal_ring_rxd_1b_set(rxdh, rxd_priv->dma_addr[0], vdev->rx_mbuf_sz);

_exit0:
	return (err);
}

/*
 * vxge_link_up
 * Callback for Link-up indication from HAL
 */
/* ARGSUSED */
void
vxge_link_up(vxge_hal_device_h devh, void *userdata)
{
	int i;
	vxge_vpath_t *vpath;
	vxge_hal_device_hw_info_t *hw_info;

	vxge_dev_t *vdev = (vxge_dev_t *) userdata;
	hw_info = &vdev->config.hw_info;

	ifnet_t ifp = vdev->ifp;

	if (vdev->config.intr_mode == VXGE_HAL_INTR_MODE_MSIX) {
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vpath = &(vdev->vpaths[i]);
			vxge_hal_vpath_tti_ci_set(vpath->handle);
			vxge_hal_vpath_rti_ci_set(vpath->handle);
		}
	}

	if (vdev->is_privilaged && (hw_info->ports > 1)) {
		vxge_active_port_update(vdev);
		device_printf(vdev->ndev,
		    "Active Port : %lld\n", vdev->active_port);
	}

	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
	if_link_state_change(ifp, LINK_STATE_UP);
}

/*
 * vxge_link_down
 * Callback for Link-down indication from HAL
 */
/* ARGSUSED */
void
vxge_link_down(vxge_hal_device_h devh, void *userdata)
{
	int i;
	vxge_vpath_t *vpath;
	vxge_dev_t *vdev = (vxge_dev_t *) userdata;

	ifnet_t ifp = vdev->ifp;

	if (vdev->config.intr_mode == VXGE_HAL_INTR_MODE_MSIX) {
		for (i = 0; i < vdev->no_of_vpath; i++) {
			vpath = &(vdev->vpaths[i]);
			vxge_hal_vpath_tti_ci_reset(vpath->handle);
			vxge_hal_vpath_rti_ci_reset(vpath->handle);
		}
	}

	ifp->if_drv_flags |= IFF_DRV_OACTIVE;
	if_link_state_change(ifp, LINK_STATE_DOWN);
}

/*
 * vxge_reset
 */
void
vxge_reset(vxge_dev_t *vdev)
{
	if (!vdev->is_initialized)
		return;

	VXGE_DRV_LOCK(vdev);
	vxge_stop_locked(vdev);
	vxge_init_locked(vdev);
	VXGE_DRV_UNLOCK(vdev);
}

/*
 * vxge_crit_error
 * Callback for Critical error indication from HAL
 */
/* ARGSUSED */
void
vxge_crit_error(vxge_hal_device_h devh, void *userdata,
    vxge_hal_event_e type, u64 serr_data)
{
	vxge_dev_t *vdev = (vxge_dev_t *) userdata;
	ifnet_t ifp = vdev->ifp;

	switch (type) {
	case VXGE_HAL_EVENT_SERR:
	case VXGE_HAL_EVENT_KDFCCTL:
	case VXGE_HAL_EVENT_CRITICAL:
		vxge_hal_device_intr_disable(vdev->devh);
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
		if_link_state_change(ifp, LINK_STATE_DOWN);
		break;
	default:
		break;
	}
}

/*
 * vxge_ifp_setup
 */
int
vxge_ifp_setup(device_t ndev)
{
	ifnet_t ifp;
	int i, j, err = 0;

	vxge_dev_t *vdev = (vxge_dev_t *) device_get_softc(ndev);

	for (i = 0, j = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if (!bVAL1(vdev->config.hw_info.vpath_mask, i))
			continue;

		if (j >= vdev->no_of_vpath)
			break;

		vdev->vpaths[j].vp_id = i;
		vdev->vpaths[j].vp_index = j;
		vdev->vpaths[j].vdev = vdev;
		vdev->vpaths[j].is_configured = TRUE;

		vxge_os_memcpy((u8 *) vdev->vpaths[j].mac_addr,
		    (u8 *) (vdev->config.hw_info.mac_addrs[i]),
		    (size_t) ETHER_ADDR_LEN);
		j++;
	}

	/* Get interface ifnet structure for this Ether device */
	ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(vdev->ndev,
		    "memory allocation for ifnet failed\n");
		err = ENXIO;
		goto _exit0;
	}
	vdev->ifp = ifp;

	/* Initialize interface ifnet structure */
	if_initname(ifp, device_get_name(ndev), device_get_unit(ndev));

	ifp->if_baudrate = VXGE_BAUDRATE;
	ifp->if_init = vxge_init;
	ifp->if_softc = vdev;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vxge_ioctl;
	ifp->if_start = vxge_send;

#if __FreeBSD_version >= 800000
	ifp->if_transmit = vxge_mq_send;
	ifp->if_qflush = vxge_mq_qflush;
#endif
	ifp->if_snd.ifq_drv_maxlen = max(vdev->config.ifq_maxlen, ifqmaxlen);
	IFQ_SET_MAXLEN(&ifp->if_snd, ifp->if_snd.ifq_drv_maxlen);
	/* IFQ_SET_READY(&ifp->if_snd); */

	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;

	if (vdev->config.tso_enable)
		vxge_tso_config(vdev);

	if (vdev->config.lro_enable)
		ifp->if_capabilities |= IFCAP_LRO;

	ifp->if_capenable = ifp->if_capabilities;

	strlcpy(vdev->ndev_name, device_get_nameunit(ndev),
	    sizeof(vdev->ndev_name));

	/* Attach the interface */
	ether_ifattach(ifp, vdev->vpaths[0].mac_addr);

_exit0:
	return (err);
}

/*
 * vxge_isr_setup
 * Register isr functions
 */
int
vxge_isr_setup(vxge_dev_t *vdev)
{
	int i, irq_rid, err = 0;
	vxge_vpath_t *vpath;

	void *isr_func_arg;
	void (*isr_func_ptr) (void *);

	switch (vdev->config.intr_mode) {
	case VXGE_HAL_INTR_MODE_IRQLINE:
		err = bus_setup_intr(vdev->ndev,
		    vdev->config.isr_info[0].irq_res,
		    (INTR_TYPE_NET | INTR_MPSAFE),
		    vxge_isr_filter, vxge_isr_line, vdev,
		    &vdev->config.isr_info[0].irq_handle);
		break;

	case VXGE_HAL_INTR_MODE_MSIX:
		for (i = 0; i < vdev->intr_count; i++) {

			irq_rid = vdev->config.isr_info[i].irq_rid;
			vpath = &vdev->vpaths[irq_rid / 4];

			if ((irq_rid % 4) == 2) {
				isr_func_ptr = vxge_isr_msix;
				isr_func_arg = (void *) vpath;
			} else if ((irq_rid % 4) == 3) {
				isr_func_ptr = vxge_isr_msix_alarm;
				isr_func_arg = (void *) vpath;
			} else
				break;

			err = bus_setup_intr(vdev->ndev,
			    vdev->config.isr_info[i].irq_res,
			    (INTR_TYPE_NET | INTR_MPSAFE), NULL,
			    (void *) isr_func_ptr, (void *) isr_func_arg,
			    &vdev->config.isr_info[i].irq_handle);
			if (err != 0)
				break;
		}

		if (err != 0) {
			/* Teardown interrupt handler */
			while (--i > 0)
				bus_teardown_intr(vdev->ndev,
				    vdev->config.isr_info[i].irq_res,
				    vdev->config.isr_info[i].irq_handle);
		}
		break;
	}

	return (err);
}

/*
 * vxge_isr_filter
 * ISR filter function - filter interrupts from other shared devices
 */
int
vxge_isr_filter(void *handle)
{
	u64 val64 = 0;
	vxge_dev_t *vdev = (vxge_dev_t *) handle;
	__hal_device_t *hldev = (__hal_device_t *) vdev->devh;

	vxge_hal_common_reg_t *common_reg =
	(vxge_hal_common_reg_t *) (hldev->common_reg);

	val64 = vxge_os_pio_mem_read64(vdev->pdev, (vdev->devh)->regh0,
	    &common_reg->titan_general_int_status);

	return ((val64) ? FILTER_SCHEDULE_THREAD : FILTER_STRAY);
}

/*
 * vxge_isr_line
 * Interrupt service routine for Line interrupts
 */
void
vxge_isr_line(void *vdev_ptr)
{
	vxge_dev_t *vdev = (vxge_dev_t *) vdev_ptr;

	vxge_hal_device_handle_irq(vdev->devh, 0);
}

void
vxge_isr_msix(void *vpath_ptr)
{
	u32 got_rx = 0;
	u32 got_tx = 0;

	__hal_virtualpath_t *hal_vpath;
	vxge_vpath_t *vpath = (vxge_vpath_t *) vpath_ptr;
	vxge_dev_t *vdev = vpath->vdev;
	hal_vpath = ((__hal_vpath_handle_t *) vpath->handle)->vpath;

	VXGE_DRV_STATS(vpath, isr_msix);
	VXGE_HAL_DEVICE_STATS_SW_INFO_TRAFFIC_INTR(vdev->devh);

	vxge_hal_vpath_mf_msix_mask(vpath->handle, vpath->msix_vec);

	/* processing rx */
	vxge_hal_vpath_poll_rx(vpath->handle, &got_rx);

	/* processing tx */
	if (hal_vpath->vp_config->fifo.enable) {
		vxge_intr_coalesce_tx(vpath);
		vxge_hal_vpath_poll_tx(vpath->handle, &got_tx);
	}

	vxge_hal_vpath_mf_msix_unmask(vpath->handle, vpath->msix_vec);
}

void
vxge_isr_msix_alarm(void *vpath_ptr)
{
	int i;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_vpath_t *vpath = (vxge_vpath_t *) vpath_ptr;
	vxge_dev_t *vdev = vpath->vdev;

	VXGE_HAL_DEVICE_STATS_SW_INFO_NOT_TRAFFIC_INTR(vdev->devh);

	/* Process alarms in each vpath */
	for (i = 0; i < vdev->no_of_vpath; i++) {

		vpath = &(vdev->vpaths[i]);
		vxge_hal_vpath_mf_msix_mask(vpath->handle,
		    vpath->msix_vec_alarm);
		status = vxge_hal_vpath_alarm_process(vpath->handle, 0);
		if ((status == VXGE_HAL_ERR_EVENT_SLOT_FREEZE) ||
		    (status == VXGE_HAL_ERR_EVENT_SERR)) {
			device_printf(vdev->ndev,
			    "processing alarms urecoverable error %x\n",
			    status);

			/* Stop the driver */
			vdev->is_initialized = FALSE;
			break;
		}
		vxge_hal_vpath_mf_msix_unmask(vpath->handle,
		    vpath->msix_vec_alarm);
	}
}

/*
 * vxge_msix_enable
 */
vxge_hal_status_e
vxge_msix_enable(vxge_dev_t *vdev)
{
	int i, first_vp_id, msix_id;

	vxge_vpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	/*
	 * Unmasking and Setting MSIX vectors before enabling interrupts
	 * tim[] : 0 - Tx ## 1 - Rx ## 2 - UMQ-DMQ ## 0 - BITMAP
	 */
	int tim[4] = {0, 1, 0, 0};

	for (i = 0; i < vdev->no_of_vpath; i++) {

		vpath = vdev->vpaths + i;
		first_vp_id = vdev->vpaths[0].vp_id;

		msix_id = vpath->vp_id * VXGE_HAL_VPATH_MSIX_ACTIVE;
		tim[1] = vpath->msix_vec = msix_id + 1;

		vpath->msix_vec_alarm = first_vp_id *
		    VXGE_HAL_VPATH_MSIX_ACTIVE + VXGE_HAL_VPATH_MSIX_ALARM_ID;

		status = vxge_hal_vpath_mf_msix_set(vpath->handle,
		    tim, VXGE_HAL_VPATH_MSIX_ALARM_ID);

		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "failed to set msix vectors to vpath\n");
			break;
		}

		vxge_hal_vpath_mf_msix_unmask(vpath->handle, vpath->msix_vec);
		vxge_hal_vpath_mf_msix_unmask(vpath->handle,
		    vpath->msix_vec_alarm);
	}

	return (status);
}

/*
 * vxge_media_init
 * Initializes, adds and sets media
 */
void
vxge_media_init(vxge_dev_t *vdev)
{
	ifmedia_init(&vdev->media,
	    IFM_IMASK, vxge_media_change, vxge_media_status);

	/* Add supported media */
	ifmedia_add(&vdev->media,
	    IFM_ETHER | vdev->ifm_optics | IFM_FDX,
	    0, NULL);

	/* Set media */
	ifmedia_add(&vdev->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&vdev->media, IFM_ETHER | IFM_AUTO);
}

/*
 * vxge_media_status
 * Callback  for interface media settings
 */
void
vxge_media_status(ifnet_t ifp, struct ifmediareq *ifmr)
{
	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;
	vxge_hal_device_t *hldev = vdev->devh;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	/* set link state */
	if (vxge_hal_device_link_state_get(hldev) == VXGE_HAL_LINK_UP) {
		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= vdev->ifm_optics | IFM_FDX;
		if_link_state_change(ifp, LINK_STATE_UP);
	}
}

/*
 * vxge_media_change
 * Media change driver callback
 */
int
vxge_media_change(ifnet_t ifp)
{
	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;
	struct ifmedia *ifmediap = &vdev->media;

	return (IFM_TYPE(ifmediap->ifm_media) != IFM_ETHER ? EINVAL : 0);
}

/*
 * Allocate PCI resources
 */
int
vxge_alloc_resources(vxge_dev_t *vdev)
{
	int err = 0;
	vxge_pci_info_t *pci_info = NULL;
	vxge_free_resources_e error_level = VXGE_FREE_NONE;

	device_t ndev = vdev->ndev;

	/* Allocate Buffer for HAL Device Configuration */
	vdev->device_config = (vxge_hal_device_config_t *)
	    vxge_mem_alloc(sizeof(vxge_hal_device_config_t));

	if (!vdev->device_config) {
		err = ENOMEM;
		error_level = VXGE_DISABLE_PCI_BUSMASTER;
		device_printf(vdev->ndev,
		    "failed to allocate memory for device config\n");
		goto _exit0;
	}


	pci_info = (vxge_pci_info_t *) vxge_mem_alloc(sizeof(vxge_pci_info_t));
	if (!pci_info) {
		error_level = VXGE_FREE_DEVICE_CONFIG;
		err = ENOMEM;
		device_printf(vdev->ndev,
		    "failed to allocate memory for pci info\n");
		goto _exit0;
	}
	pci_info->ndev = ndev;
	vdev->pdev = pci_info;

	err = vxge_alloc_bar_resources(vdev, 0);
	if (err != 0) {
		error_level = VXGE_FREE_BAR0;
		goto _exit0;
	}

	err = vxge_alloc_bar_resources(vdev, 1);
	if (err != 0) {
		error_level = VXGE_FREE_BAR1;
		goto _exit0;
	}

	err = vxge_alloc_bar_resources(vdev, 2);
	if (err != 0)
		error_level = VXGE_FREE_BAR2;

_exit0:
	if (error_level)
		vxge_free_resources(ndev, error_level);

	return (err);
}

/*
 * vxge_alloc_bar_resources
 * Allocates BAR resources
 */
int
vxge_alloc_bar_resources(vxge_dev_t *vdev, int i)
{
	int err = 0;
	int res_id = 0;
	vxge_pci_info_t *pci_info = vdev->pdev;

	res_id = PCIR_BAR((i == 0) ? 0 : (i * 2));

	pci_info->bar_info[i] =
	    bus_alloc_resource_any(vdev->ndev,
	    SYS_RES_MEMORY, &res_id, RF_ACTIVE);

	if (pci_info->bar_info[i] == NULL) {
		device_printf(vdev->ndev,
		    "failed to allocate memory for bus resources\n");
		err = ENOMEM;
		goto _exit0;
	}

	pci_info->reg_map[i] =
	    (vxge_bus_res_t *) vxge_mem_alloc(sizeof(vxge_bus_res_t));

	if (pci_info->reg_map[i] == NULL) {
		device_printf(vdev->ndev,
		    "failed to allocate memory bar resources\n");
		err = ENOMEM;
		goto _exit0;
	}

	((vxge_bus_res_t *) (pci_info->reg_map[i]))->bus_space_tag =
	    rman_get_bustag(pci_info->bar_info[i]);

	((vxge_bus_res_t *) (pci_info->reg_map[i]))->bus_space_handle =
	    rman_get_bushandle(pci_info->bar_info[i]);

	((vxge_bus_res_t *) (pci_info->reg_map[i]))->bar_start_addr =
	    pci_info->bar_info[i];

	((vxge_bus_res_t *) (pci_info->reg_map[i]))->bus_res_len =
	    rman_get_size(pci_info->bar_info[i]);

_exit0:
	return (err);
}

/*
 * vxge_alloc_isr_resources
 */
int
vxge_alloc_isr_resources(vxge_dev_t *vdev)
{
	int i, err = 0, irq_rid;
	int msix_vec_reqd, intr_count, msix_count;

	int intr_mode = VXGE_HAL_INTR_MODE_IRQLINE;

	if (vdev->config.intr_mode == VXGE_HAL_INTR_MODE_MSIX) {
		/* MSI-X messages supported by device */
		intr_count = pci_msix_count(vdev->ndev);
		if (intr_count) {

			msix_vec_reqd = 4 * vdev->no_of_vpath;
			if (intr_count >= msix_vec_reqd) {
				intr_count = msix_vec_reqd;

				err = pci_alloc_msix(vdev->ndev, &intr_count);
				if (err == 0)
					intr_mode = VXGE_HAL_INTR_MODE_MSIX;
			}

			if ((err != 0) || (intr_count < msix_vec_reqd)) {
				device_printf(vdev->ndev, "Unable to allocate "
				    "msi/x vectors switching to INTA mode\n");
			}
		}
	}

	err = 0;
	vdev->intr_count = 0;
	vdev->config.intr_mode = intr_mode;

	switch (vdev->config.intr_mode) {
	case VXGE_HAL_INTR_MODE_IRQLINE:
		vdev->config.isr_info[0].irq_rid = 0;
		vdev->config.isr_info[0].irq_res =
		    bus_alloc_resource_any(vdev->ndev, SYS_RES_IRQ,
		    &vdev->config.isr_info[0].irq_rid,
		    (RF_SHAREABLE | RF_ACTIVE));

		if (vdev->config.isr_info[0].irq_res == NULL) {
			device_printf(vdev->ndev,
			    "failed to allocate line interrupt resource\n");
			err = ENOMEM;
			goto _exit0;
		}
		vdev->intr_count++;
		break;

	case VXGE_HAL_INTR_MODE_MSIX:
		msix_count = 0;
		for (i = 0; i < vdev->no_of_vpath; i++) {
			irq_rid = i * 4;

			vdev->config.isr_info[msix_count].irq_rid = irq_rid + 2;
			vdev->config.isr_info[msix_count].irq_res =
			    bus_alloc_resource_any(vdev->ndev, SYS_RES_IRQ,
			    &vdev->config.isr_info[msix_count].irq_rid,
			    (RF_SHAREABLE | RF_ACTIVE));

			if (vdev->config.isr_info[msix_count].irq_res == NULL) {
				device_printf(vdev->ndev,
				    "allocating bus resource (rid %d) failed\n",
				    vdev->config.isr_info[msix_count].irq_rid);
				err = ENOMEM;
				goto _exit0;
			}

			vdev->intr_count++;
			err = bus_bind_intr(vdev->ndev,
			    vdev->config.isr_info[msix_count].irq_res,
			    (i % mp_ncpus));
			if (err != 0)
				break;

			msix_count++;
		}

		vdev->config.isr_info[msix_count].irq_rid = 3;
		vdev->config.isr_info[msix_count].irq_res =
		    bus_alloc_resource_any(vdev->ndev, SYS_RES_IRQ,
		    &vdev->config.isr_info[msix_count].irq_rid,
		    (RF_SHAREABLE | RF_ACTIVE));

		if (vdev->config.isr_info[msix_count].irq_res == NULL) {
			device_printf(vdev->ndev,
			    "allocating bus resource (rid %d) failed\n",
			    vdev->config.isr_info[msix_count].irq_rid);
			err = ENOMEM;
			goto _exit0;
		}

		vdev->intr_count++;
		err = bus_bind_intr(vdev->ndev,
		    vdev->config.isr_info[msix_count].irq_res, (i % mp_ncpus));

		break;
	}

	vdev->device_config->intr_mode = vdev->config.intr_mode;

_exit0:
	return (err);
}

/*
 * vxge_free_resources
 * Undo what-all we did during load/attach
 */
void
vxge_free_resources(device_t ndev, vxge_free_resources_e vxge_free_resource)
{
	int i;
	vxge_dev_t *vdev;

	vdev = (vxge_dev_t *) device_get_softc(ndev);

	switch (vxge_free_resource) {
	case VXGE_FREE_ALL:
		for (i = 0; i < vdev->intr_count; i++) {
			bus_teardown_intr(ndev,
			    vdev->config.isr_info[i].irq_res,
			    vdev->config.isr_info[i].irq_handle);
		}
		/* FALLTHROUGH */

	case VXGE_FREE_INTERFACE:
		ether_ifdetach(vdev->ifp);
		bus_generic_detach(ndev);
		if_free(vdev->ifp);
		/* FALLTHROUGH */

	case VXGE_FREE_MEDIA:
		ifmedia_removeall(&vdev->media);
		/* FALLTHROUGH */

	case VXGE_FREE_MUTEX:
		vxge_mutex_destroy(vdev);
		/* FALLTHROUGH */

	case VXGE_FREE_VPATH:
		vxge_mem_free(vdev->vpaths,
		    vdev->no_of_vpath * sizeof(vxge_vpath_t));
		/* FALLTHROUGH */

	case VXGE_FREE_TERMINATE_DEVICE:
		if (vdev->devh != NULL) {
			vxge_hal_device_private_set(vdev->devh, 0);
			vxge_hal_device_terminate(vdev->devh);
		}
		/* FALLTHROUGH */

	case VXGE_FREE_ISR_RESOURCE:
		vxge_free_isr_resources(vdev);
		/* FALLTHROUGH */

	case VXGE_FREE_BAR2:
		vxge_free_bar_resources(vdev, 2);
		/* FALLTHROUGH */

	case VXGE_FREE_BAR1:
		vxge_free_bar_resources(vdev, 1);
		/* FALLTHROUGH */

	case VXGE_FREE_BAR0:
		vxge_free_bar_resources(vdev, 0);
		/* FALLTHROUGH */

	case VXGE_FREE_PCI_INFO:
		vxge_mem_free(vdev->pdev, sizeof(vxge_pci_info_t));
		/* FALLTHROUGH */

	case VXGE_FREE_DEVICE_CONFIG:
		vxge_mem_free(vdev->device_config,
		    sizeof(vxge_hal_device_config_t));
		/* FALLTHROUGH */

	case VXGE_DISABLE_PCI_BUSMASTER:
		pci_disable_busmaster(ndev);
		/* FALLTHROUGH */

	case VXGE_FREE_TERMINATE_DRIVER:
		if (vxge_dev_ref_count) {
			--vxge_dev_ref_count;
			if (0 == vxge_dev_ref_count)
				vxge_hal_driver_terminate();
		}
		/* FALLTHROUGH */

	default:
	case VXGE_FREE_NONE:
		break;
		/* NOTREACHED */
	}
}

void
vxge_free_isr_resources(vxge_dev_t *vdev)
{
	int i;

	switch (vdev->config.intr_mode) {
	case VXGE_HAL_INTR_MODE_IRQLINE:
		if (vdev->config.isr_info[0].irq_res) {
			bus_release_resource(vdev->ndev, SYS_RES_IRQ,
			    vdev->config.isr_info[0].irq_rid,
			    vdev->config.isr_info[0].irq_res);

			vdev->config.isr_info[0].irq_res = NULL;
		}
		break;

	case VXGE_HAL_INTR_MODE_MSIX:
		for (i = 0; i < vdev->intr_count; i++) {
			if (vdev->config.isr_info[i].irq_res) {
				bus_release_resource(vdev->ndev, SYS_RES_IRQ,
				    vdev->config.isr_info[i].irq_rid,
				    vdev->config.isr_info[i].irq_res);

				vdev->config.isr_info[i].irq_res = NULL;
			}
		}

		if (vdev->intr_count)
			pci_release_msi(vdev->ndev);

		break;
	}
}

void
vxge_free_bar_resources(vxge_dev_t *vdev, int i)
{
	int res_id = 0;
	vxge_pci_info_t *pci_info = vdev->pdev;

	res_id = PCIR_BAR((i == 0) ? 0 : (i * 2));

	if (pci_info->bar_info[i])
		bus_release_resource(vdev->ndev, SYS_RES_MEMORY,
		    res_id, pci_info->bar_info[i]);

	vxge_mem_free(pci_info->reg_map[i], sizeof(vxge_bus_res_t));
}

/*
 * vxge_init_mutex
 * Initializes mutexes used in driver
 */
void
vxge_mutex_init(vxge_dev_t *vdev)
{
	int i;

	snprintf(vdev->mtx_drv_name, sizeof(vdev->mtx_drv_name),
	    "%s_drv", vdev->ndev_name);

	mtx_init(&vdev->mtx_drv, vdev->mtx_drv_name,
	    MTX_NETWORK_LOCK, MTX_DEF);

	for (i = 0; i < vdev->no_of_vpath; i++) {
		snprintf(vdev->vpaths[i].mtx_tx_name,
		    sizeof(vdev->vpaths[i].mtx_tx_name), "%s_tx_%d",
		    vdev->ndev_name, i);

		mtx_init(&vdev->vpaths[i].mtx_tx,
		    vdev->vpaths[i].mtx_tx_name, NULL, MTX_DEF);
	}
}

/*
 * vxge_mutex_destroy
 * Destroys mutexes used in driver
 */
void
vxge_mutex_destroy(vxge_dev_t *vdev)
{
	int i;

	for (i = 0; i < vdev->no_of_vpath; i++)
		VXGE_TX_LOCK_DESTROY(&(vdev->vpaths[i]));

	VXGE_DRV_LOCK_DESTROY(vdev);
}

/*
 * vxge_rth_config
 */
vxge_hal_status_e
vxge_rth_config(vxge_dev_t *vdev)
{
	int i;
	vxge_hal_vpath_h vpath_handle;
	vxge_hal_rth_hash_types_t hash_types;
	vxge_hal_status_e status = VXGE_HAL_OK;
	u8 mtable[256] = {0};

	/* Filling matable with bucket-to-vpath mapping */
	vdev->config.rth_bkt_sz = VXGE_DEFAULT_RTH_BUCKET_SIZE;

	for (i = 0; i < (1 << vdev->config.rth_bkt_sz); i++)
		mtable[i] = i % vdev->no_of_vpath;

	/* Fill RTH hash types */
	hash_types.hash_type_tcpipv4_en = VXGE_HAL_RING_HASH_TYPE_TCP_IPV4;
	hash_types.hash_type_tcpipv6_en = VXGE_HAL_RING_HASH_TYPE_TCP_IPV6;
	hash_types.hash_type_tcpipv6ex_en = VXGE_HAL_RING_HASH_TYPE_TCP_IPV6_EX;
	hash_types.hash_type_ipv4_en = VXGE_HAL_RING_HASH_TYPE_IPV4;
	hash_types.hash_type_ipv6_en = VXGE_HAL_RING_HASH_TYPE_IPV6;
	hash_types.hash_type_ipv6ex_en = VXGE_HAL_RING_HASH_TYPE_IPV6_EX;

	/* set indirection table, bucket-to-vpath mapping */
	status = vxge_hal_vpath_rts_rth_itable_set(vdev->vpath_handles,
	    vdev->no_of_vpath, mtable,
	    ((u32) (1 << vdev->config.rth_bkt_sz)));

	if (status != VXGE_HAL_OK) {
		device_printf(vdev->ndev, "rth configuration failed\n");
		goto _exit0;
	}
	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		status = vxge_hal_vpath_rts_rth_set(vpath_handle,
		    RTH_ALG_JENKINS,
		    &hash_types, vdev->config.rth_bkt_sz, TRUE);
		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "rth configuration failed for vpath (%d)\n",
			    vdev->vpaths[i].vp_id);
			break;
		}
	}

_exit0:
	return (status);
}

/*
 * vxge_vpath_config
 * Sets HAL parameter values from kenv
 */
void
vxge_vpath_config(vxge_dev_t *vdev)
{
	int i;
	u32 no_of_vpath = 0;
	vxge_hal_vp_config_t *vp_config;
	vxge_hal_device_config_t *device_config = vdev->device_config;

	device_config->debug_level = VXGE_TRACE;
	device_config->debug_mask = VXGE_COMPONENT_ALL;
	device_config->device_poll_millis = VXGE_DEFAULT_DEVICE_POLL_MILLIS;

	vdev->config.no_of_vpath =
	    min(vdev->config.no_of_vpath, vdev->max_supported_vpath);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		vp_config = &(device_config->vp_config[i]);
		vp_config->fifo.enable = VXGE_HAL_FIFO_DISABLE;
		vp_config->ring.enable = VXGE_HAL_RING_DISABLE;
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if (no_of_vpath >= vdev->config.no_of_vpath)
			break;

		if (!bVAL1(vdev->config.hw_info.vpath_mask, i))
			continue;

		no_of_vpath++;
		vp_config = &(device_config->vp_config[i]);
		vp_config->mtu = VXGE_HAL_DEFAULT_MTU;
		vp_config->ring.enable = VXGE_HAL_RING_ENABLE;
		vp_config->ring.post_mode = VXGE_HAL_RING_POST_MODE_DOORBELL;
		vp_config->ring.buffer_mode = VXGE_HAL_RING_RXD_BUFFER_MODE_1;
		vp_config->ring.ring_length =
		    vxge_ring_length_get(VXGE_HAL_RING_RXD_BUFFER_MODE_1);
		vp_config->ring.scatter_mode = VXGE_HAL_RING_SCATTER_MODE_A;
		vp_config->rpa_all_vid_en = VXGE_DEFAULT_ALL_VID_ENABLE;
		vp_config->rpa_strip_vlan_tag = VXGE_DEFAULT_STRIP_VLAN_TAG;
		vp_config->rpa_ucast_all_addr_en =
		    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE;

		vp_config->rti.intr_enable = VXGE_HAL_TIM_INTR_ENABLE;
		vp_config->rti.txfrm_cnt_en = VXGE_HAL_TXFRM_CNT_EN_ENABLE;
		vp_config->rti.util_sel =
		    VXGE_HAL_TIM_UTIL_SEL_LEGACY_RX_NET_UTIL;

		vp_config->rti.uec_a = VXGE_DEFAULT_RTI_RX_UFC_A;
		vp_config->rti.uec_b = VXGE_DEFAULT_RTI_RX_UFC_B;
		vp_config->rti.uec_c = VXGE_DEFAULT_RTI_RX_UFC_C;
		vp_config->rti.uec_d = VXGE_DEFAULT_RTI_RX_UFC_D;

		vp_config->rti.urange_a = VXGE_DEFAULT_RTI_RX_URANGE_A;
		vp_config->rti.urange_b = VXGE_DEFAULT_RTI_RX_URANGE_B;
		vp_config->rti.urange_c = VXGE_DEFAULT_RTI_RX_URANGE_C;

		vp_config->rti.timer_ac_en = VXGE_HAL_TIM_TIMER_AC_ENABLE;
		vp_config->rti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_ENABLE;

		vp_config->rti.btimer_val =
		    (VXGE_DEFAULT_RTI_BTIMER_VAL * 1000) / 272;
		vp_config->rti.rtimer_val =
		    (VXGE_DEFAULT_RTI_RTIMER_VAL * 1000) / 272;
		vp_config->rti.ltimer_val =
		    (VXGE_DEFAULT_RTI_LTIMER_VAL * 1000) / 272;

		if ((no_of_vpath > 1) && (VXGE_DEFAULT_CONFIG_MQ_ENABLE == 0))
			continue;

		vp_config->fifo.enable = VXGE_HAL_FIFO_ENABLE;
		vp_config->fifo.max_aligned_frags =
		    VXGE_DEFAULT_FIFO_ALIGNED_FRAGS;

		vp_config->tti.intr_enable = VXGE_HAL_TIM_INTR_ENABLE;
		vp_config->tti.txfrm_cnt_en = VXGE_HAL_TXFRM_CNT_EN_ENABLE;
		vp_config->tti.util_sel =
		    VXGE_HAL_TIM_UTIL_SEL_LEGACY_TX_NET_UTIL;

		vp_config->tti.uec_a = VXGE_DEFAULT_TTI_TX_UFC_A;
		vp_config->tti.uec_b = VXGE_DEFAULT_TTI_TX_UFC_B;
		vp_config->tti.uec_c = VXGE_DEFAULT_TTI_TX_UFC_C;
		vp_config->tti.uec_d = VXGE_DEFAULT_TTI_TX_UFC_D;

		vp_config->tti.urange_a = VXGE_DEFAULT_TTI_TX_URANGE_A;
		vp_config->tti.urange_b = VXGE_DEFAULT_TTI_TX_URANGE_B;
		vp_config->tti.urange_c = VXGE_DEFAULT_TTI_TX_URANGE_C;

		vp_config->tti.timer_ac_en = VXGE_HAL_TIM_TIMER_AC_ENABLE;
		vp_config->tti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_ENABLE;

		vp_config->tti.btimer_val =
		    (VXGE_DEFAULT_TTI_BTIMER_VAL * 1000) / 272;
		vp_config->tti.rtimer_val =
		    (VXGE_DEFAULT_TTI_RTIMER_VAL * 1000) / 272;
		vp_config->tti.ltimer_val =
		    (VXGE_DEFAULT_TTI_LTIMER_VAL * 1000) / 272;
	}

	vdev->no_of_vpath = no_of_vpath;

	if (vdev->no_of_vpath == 1)
		vdev->config.tx_steering = 0;

	if (vdev->config.rth_enable && (vdev->no_of_vpath > 1)) {
		device_config->rth_en = VXGE_HAL_RTH_ENABLE;
		device_config->rth_it_type = VXGE_HAL_RTH_IT_TYPE_MULTI_IT;
	}

	vdev->config.rth_enable = device_config->rth_en;
}

/*
 * vxge_vpath_cb_fn
 * Virtual path Callback function
 */
/* ARGSUSED */
static vxge_hal_status_e
vxge_vpath_cb_fn(vxge_hal_client_h client_handle, vxge_hal_up_msg_h msgh,
    vxge_hal_message_type_e msg_type, vxge_hal_obj_id_t obj_id,
    vxge_hal_result_e result, vxge_hal_opaque_handle_t *opaque_handle)
{
	return (VXGE_HAL_OK);
}

/*
 * vxge_vpath_open
 */
int
vxge_vpath_open(vxge_dev_t *vdev)
{
	int i, err = EINVAL;
	u64 func_id;

	vxge_vpath_t *vpath;
	vxge_hal_vpath_attr_t vpath_attr;
	vxge_hal_status_e status = VXGE_HAL_OK;
	struct lro_ctrl *lro = NULL;

	bzero(&vpath_attr, sizeof(vxge_hal_vpath_attr_t));

	for (i = 0; i < vdev->no_of_vpath; i++) {

		vpath = &(vdev->vpaths[i]);
		lro = &vpath->lro;

		/* Vpath vpath_attr: FIFO */
		vpath_attr.vp_id = vpath->vp_id;
		vpath_attr.fifo_attr.callback = vxge_tx_compl;
		vpath_attr.fifo_attr.txdl_init = vxge_tx_replenish;
		vpath_attr.fifo_attr.txdl_term = vxge_tx_term;
		vpath_attr.fifo_attr.userdata = vpath;
		vpath_attr.fifo_attr.per_txdl_space = sizeof(vxge_txdl_priv_t);

		/* Vpath vpath_attr: Ring */
		vpath_attr.ring_attr.callback = vxge_rx_compl;
		vpath_attr.ring_attr.rxd_init = vxge_rx_replenish;
		vpath_attr.ring_attr.rxd_term = vxge_rx_term;
		vpath_attr.ring_attr.userdata = vpath;
		vpath_attr.ring_attr.per_rxd_space = sizeof(vxge_rxd_priv_t);

		err = vxge_dma_tags_create(vpath);
		if (err != 0) {
			device_printf(vdev->ndev,
			    "failed to create dma tags\n");
			break;
		}
#if __FreeBSD_version >= 800000
		vpath->br = buf_ring_alloc(VXGE_DEFAULT_BR_SIZE, M_DEVBUF,
		    M_WAITOK, &vpath->mtx_tx);
		if (vpath->br == NULL) {
			err = ENOMEM;
			break;
		}
#endif
		status = vxge_hal_vpath_open(vdev->devh, &vpath_attr,
		    (vxge_hal_vpath_callback_f) vxge_vpath_cb_fn,
		    NULL, &vpath->handle);
		if (status != VXGE_HAL_OK) {
			device_printf(vdev->ndev,
			    "failed to open vpath (%d)\n", vpath->vp_id);
			err = EPERM;
			break;
		}
		vpath->is_open = TRUE;
		vdev->vpath_handles[i] = vpath->handle;

		vpath->tx_ticks = ticks;
		vpath->rx_ticks = ticks;

		vpath->tti_rtimer_val = VXGE_DEFAULT_TTI_RTIMER_VAL;
		vpath->rti_rtimer_val = VXGE_DEFAULT_RTI_RTIMER_VAL;

		vpath->tx_intr_coalesce = vdev->config.intr_coalesce;
		vpath->rx_intr_coalesce = vdev->config.intr_coalesce;

		func_id = vdev->config.hw_info.func_id;

		if (vdev->config.low_latency &&
		    (vdev->config.bw_info[func_id].priority ==
			VXGE_DEFAULT_VPATH_PRIORITY_HIGH)) {
			vpath->tx_intr_coalesce = 0;
		}

		if (vdev->ifp->if_capenable & IFCAP_LRO) {
			err = tcp_lro_init(lro);
			if (err != 0) {
				device_printf(vdev->ndev,
				    "LRO Initialization failed!\n");
				break;
			}
			vpath->lro_enable = TRUE;
			lro->ifp = vdev->ifp;
		}
	}

	return (err);
}

void
vxge_tso_config(vxge_dev_t *vdev)
{
	u32 func_id, priority;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vdev->ifp->if_capabilities |= IFCAP_TSO4;

	status = vxge_bw_priority_get(vdev, NULL);
	if (status == VXGE_HAL_OK) {

		func_id = vdev->config.hw_info.func_id;
		priority = vdev->config.bw_info[func_id].priority;

		if (priority != VXGE_DEFAULT_VPATH_PRIORITY_HIGH)
			vdev->ifp->if_capabilities &= ~IFCAP_TSO4;
	}

#if __FreeBSD_version >= 800000
	if (vdev->ifp->if_capabilities & IFCAP_TSO4)
		vdev->ifp->if_capabilities |= IFCAP_VLAN_HWTSO;
#endif

}

vxge_hal_status_e
vxge_bw_priority_get(vxge_dev_t *vdev, vxge_bw_info_t *bw_info)
{
	u32 priority, bandwidth;
	u32 vpath_count;

	u64 func_id, func_mode, vpath_list[VXGE_HAL_MAX_VIRTUAL_PATHS];
	vxge_hal_status_e status = VXGE_HAL_OK;

	func_id = vdev->config.hw_info.func_id;
	if (bw_info) {
		func_id = bw_info->func_id;
		func_mode = vdev->config.hw_info.function_mode;
		if ((is_single_func(func_mode)) && (func_id > 0))
			return (VXGE_HAL_FAIL);
	}

	if (vdev->hw_fw_version >= VXGE_FW_VERSION(1, 8, 0)) {

		status = vxge_hal_vf_rx_bw_get(vdev->devh,
		    func_id, &bandwidth, &priority);

	} else {

		status = vxge_hal_get_vpath_list(vdev->devh,
		    func_id, vpath_list, &vpath_count);

		if (status == VXGE_HAL_OK) {
			status = vxge_hal_bw_priority_get(vdev->devh,
			    vpath_list[0], &bandwidth, &priority);
		}
	}

	if (status == VXGE_HAL_OK) {
		if (bw_info) {
			bw_info->priority = priority;
			bw_info->bandwidth = bandwidth;
		} else {
			vdev->config.bw_info[func_id].priority = priority;
			vdev->config.bw_info[func_id].bandwidth = bandwidth;
		}
	}

	return (status);
}

/*
 * close vpaths
 */
void
vxge_vpath_close(vxge_dev_t *vdev)
{
	int i;
	vxge_vpath_t *vpath;

	for (i = 0; i < vdev->no_of_vpath; i++) {

		vpath = &(vdev->vpaths[i]);
		if (vpath->handle)
			vxge_hal_vpath_close(vpath->handle);

#if __FreeBSD_version >= 800000
		if (vpath->br != NULL)
			buf_ring_free(vpath->br, M_DEVBUF);
#endif
		/* Free LRO memory */
		if (vpath->lro_enable)
			tcp_lro_free(&vpath->lro);

		if (vpath->dma_tag_rx) {
			bus_dmamap_destroy(vpath->dma_tag_rx,
			    vpath->extra_dma_map);
			bus_dma_tag_destroy(vpath->dma_tag_rx);
		}

		if (vpath->dma_tag_tx)
			bus_dma_tag_destroy(vpath->dma_tag_tx);

		vpath->handle = NULL;
		vpath->is_open = FALSE;
	}
}

/*
 * reset vpaths
 */
void
vxge_vpath_reset(vxge_dev_t *vdev)
{
	int i;
	vxge_hal_vpath_h vpath_handle;
	vxge_hal_status_e status = VXGE_HAL_OK;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		status = vxge_hal_vpath_reset(vpath_handle);
		if (status != VXGE_HAL_OK)
			device_printf(vdev->ndev,
			    "failed to reset vpath :%d\n", i);
	}
}

static inline int
vxge_vpath_get(vxge_dev_t *vdev, mbuf_t mhead)
{
	struct tcphdr *th = NULL;
	struct udphdr *uh = NULL;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6 = NULL;
	struct ether_vlan_header *eth = NULL;
	void *ulp = NULL;

	int ehdrlen, iphlen = 0;
	u8 ipproto = 0;
	u16 etype, src_port, dst_port;
	u16 queue_len, counter = 0;

	src_port = dst_port = 0;
	queue_len = vdev->no_of_vpath;

	eth = mtod(mhead, struct ether_vlan_header *);
	if (eth->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eth->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eth->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *) (mhead->m_data + ehdrlen);
		iphlen = ip->ip_hl << 2;
		ipproto = ip->ip_p;
		th = (struct tcphdr *) ((caddr_t)ip + iphlen);
		uh = (struct udphdr *) ((caddr_t)ip + iphlen);
		break;

	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *) (mhead->m_data + ehdrlen);
		iphlen = sizeof(struct ip6_hdr);
		ipproto = ip6->ip6_nxt;

		ulp = mtod(mhead, char *) + iphlen;
		th = ((struct tcphdr *) (ulp));
		uh = ((struct udphdr *) (ulp));
		break;

	default:
		break;
	}

	switch (ipproto) {
	case IPPROTO_TCP:
		src_port = th->th_sport;
		dst_port = th->th_dport;
		break;

	case IPPROTO_UDP:
		src_port = uh->uh_sport;
		dst_port = uh->uh_dport;
		break;

	default:
		break;
	}

	counter = (ntohs(src_port) + ntohs(dst_port)) &
	    vpath_selector[queue_len - 1];

	if (counter >= queue_len)
		counter = queue_len - 1;

	return (counter);
}

static inline vxge_hal_vpath_h
vxge_vpath_handle_get(vxge_dev_t *vdev, int i)
{
	return (vdev->vpaths[i].is_open ? vdev->vpaths[i].handle : NULL);
}

int
vxge_firmware_verify(vxge_dev_t *vdev)
{
	int err = 0;
	u64 active_config;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	if (vdev->fw_upgrade) {
		status = vxge_firmware_upgrade(vdev);
		if (status == VXGE_HAL_OK) {
			err = ENXIO;
			goto _exit0;
		}
	}

	if ((vdev->config.function_mode != VXGE_DEFAULT_CONFIG_VALUE) &&
	    (vdev->config.hw_info.function_mode !=
	    (u64) vdev->config.function_mode)) {

		status = vxge_func_mode_set(vdev);
		if (status == VXGE_HAL_OK)
			err = ENXIO;
	}

	/* l2_switch configuration */
	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_L2SwitchEnabled,
	    &active_config);

	if (status == VXGE_HAL_OK) {
		vdev->l2_switch = active_config;
		if (vdev->config.l2_switch != VXGE_DEFAULT_CONFIG_VALUE) {
			if (vdev->config.l2_switch != active_config) {
				status = vxge_l2switch_mode_set(vdev);
				if (status == VXGE_HAL_OK)
					err = ENXIO;
			}
		}
	}

	if (vdev->config.hw_info.ports == VXGE_DUAL_PORT_MODE) {
		if (vxge_port_mode_update(vdev) == ENXIO)
			err = ENXIO;
	}

_exit0:
	if (err == ENXIO)
		device_printf(vdev->ndev, "PLEASE POWER CYCLE THE SYSTEM\n");

	return (err);
}

vxge_hal_status_e
vxge_firmware_upgrade(vxge_dev_t *vdev)
{
	u8 *fw_buffer;
	u32 fw_size;
	vxge_hal_device_hw_info_t *hw_info;
	vxge_hal_status_e status = VXGE_HAL_OK;

	hw_info = &vdev->config.hw_info;

	fw_size = sizeof(VXGE_FW_ARRAY_NAME);
	fw_buffer = (u8 *) VXGE_FW_ARRAY_NAME;

	device_printf(vdev->ndev, "Current firmware version : %s (%s)\n",
	    hw_info->fw_version.version, hw_info->fw_date.date);

	device_printf(vdev->ndev, "Upgrading firmware to %d.%d.%d\n",
	    VXGE_MIN_FW_MAJOR_VERSION, VXGE_MIN_FW_MINOR_VERSION,
	    VXGE_MIN_FW_BUILD_NUMBER);

	/* Call HAL API to upgrade firmware */
	status = vxge_hal_mrpcim_fw_upgrade(vdev->pdev,
	    (pci_reg_h) vdev->pdev->reg_map[0],
	    (u8 *) vdev->pdev->bar_info[0],
	    fw_buffer, fw_size);

	device_printf(vdev->ndev, "firmware upgrade %s\n",
	    (status == VXGE_HAL_OK) ? "successful" : "failed");

	return (status);
}

vxge_hal_status_e
vxge_func_mode_set(vxge_dev_t *vdev)
{
	u64 active_config;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	status = vxge_hal_mrpcim_pcie_func_mode_set(vdev->devh,
	    vdev->config.function_mode);
	device_printf(vdev->ndev,
	    "function mode change %s\n",
	    (status == VXGE_HAL_OK) ? "successful" : "failed");

	if (status == VXGE_HAL_OK) {
		vxge_hal_set_fw_api(vdev->devh, 0ULL,
		    VXGE_HAL_API_FUNC_MODE_COMMIT,
		    0, 0ULL, 0ULL);

		vxge_hal_get_active_config(vdev->devh,
		    VXGE_HAL_XMAC_NWIF_ActConfig_NWPortMode,
		    &active_config);

		/*
		 * If in MF + DP mode
		 * if user changes to SF, change port_mode to single port mode
		 */
		if (((is_multi_func(vdev->config.hw_info.function_mode)) &&
		    is_single_func(vdev->config.function_mode)) &&
		    (active_config == VXGE_HAL_DP_NP_MODE_DUAL_PORT)) {
			vdev->config.port_mode =
			    VXGE_HAL_DP_NP_MODE_SINGLE_PORT;

			status = vxge_port_mode_set(vdev);
		}
	}
	return (status);
}

vxge_hal_status_e
vxge_port_mode_set(vxge_dev_t *vdev)
{
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	status = vxge_hal_set_port_mode(vdev->devh, vdev->config.port_mode);
	device_printf(vdev->ndev,
	    "port mode change %s\n",
	    (status == VXGE_HAL_OK) ? "successful" : "failed");

	if (status == VXGE_HAL_OK) {
		vxge_hal_set_fw_api(vdev->devh, 0ULL,
		    VXGE_HAL_API_FUNC_MODE_COMMIT,
		    0, 0ULL, 0ULL);

		/* Configure vpath_mapping for active-active mode only */
		if (vdev->config.port_mode == VXGE_HAL_DP_NP_MODE_DUAL_PORT) {

			status = vxge_hal_config_vpath_map(vdev->devh,
			    VXGE_DUAL_PORT_MAP);

			device_printf(vdev->ndev, "dual port map change %s\n",
			    (status == VXGE_HAL_OK) ? "successful" : "failed");
		}
	}
	return (status);
}

int
vxge_port_mode_update(vxge_dev_t *vdev)
{
	int err = 0;
	u64 active_config;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	if ((vdev->config.port_mode == VXGE_HAL_DP_NP_MODE_DUAL_PORT) &&
	    is_single_func(vdev->config.hw_info.function_mode)) {

		device_printf(vdev->ndev,
		    "Adapter in SF mode, dual port mode is not allowed\n");
		err = EPERM;
		goto _exit0;
	}

	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_NWPortMode,
	    &active_config);
	if (status != VXGE_HAL_OK) {
		err = EINVAL;
		goto _exit0;
	}

	vdev->port_mode = active_config;
	if (vdev->config.port_mode != VXGE_DEFAULT_CONFIG_VALUE) {
		if (vdev->config.port_mode != vdev->port_mode) {
			status = vxge_port_mode_set(vdev);
			if (status != VXGE_HAL_OK) {
				err = EINVAL;
				goto _exit0;
			}
			err = ENXIO;
			vdev->port_mode  = vdev->config.port_mode;
		}
	}

	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_BehaviourOnFail,
	    &active_config);
	if (status != VXGE_HAL_OK) {
		err = EINVAL;
		goto _exit0;
	}

	vdev->port_failure = active_config;

	/*
	 * active/active mode : set to NoMove
	 * active/passive mode: set to Failover-Failback
	 */
	if (vdev->port_mode == VXGE_HAL_DP_NP_MODE_DUAL_PORT)
		vdev->config.port_failure =
		    VXGE_HAL_XMAC_NWIF_OnFailure_NoMove;

	else if (vdev->port_mode == VXGE_HAL_DP_NP_MODE_ACTIVE_PASSIVE)
		vdev->config.port_failure =
		    VXGE_HAL_XMAC_NWIF_OnFailure_OtherPortBackOnRestore;

	if ((vdev->port_mode != VXGE_HAL_DP_NP_MODE_SINGLE_PORT) &&
	    (vdev->config.port_failure != vdev->port_failure)) {
		status = vxge_port_behavior_on_failure_set(vdev);
		if (status == VXGE_HAL_OK)
			err = ENXIO;
	}

_exit0:
	return (err);
}

vxge_hal_status_e
vxge_port_mode_get(vxge_dev_t *vdev, vxge_port_info_t *port_info)
{
	int err = 0;
	u64 active_config;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_NWPortMode,
	    &active_config);

	if (status != VXGE_HAL_OK) {
		err = ENXIO;
		goto _exit0;
	}

	port_info->port_mode = active_config;

	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_BehaviourOnFail,
	    &active_config);
	if (status != VXGE_HAL_OK) {
		err = ENXIO;
		goto _exit0;
	}

	port_info->port_failure = active_config;

_exit0:
	return (err);
}

vxge_hal_status_e
vxge_port_behavior_on_failure_set(vxge_dev_t *vdev)
{
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	status = vxge_hal_set_behavior_on_failure(vdev->devh,
	    vdev->config.port_failure);

	device_printf(vdev->ndev,
	    "port behaviour on failure change %s\n",
	    (status == VXGE_HAL_OK) ? "successful" : "failed");

	if (status == VXGE_HAL_OK)
		vxge_hal_set_fw_api(vdev->devh, 0ULL,
		    VXGE_HAL_API_FUNC_MODE_COMMIT,
		    0, 0ULL, 0ULL);

	return (status);
}

void
vxge_active_port_update(vxge_dev_t *vdev)
{
	u64 active_config;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	active_config = VXGE_DEFAULT_CONFIG_VALUE;
	status = vxge_hal_get_active_config(vdev->devh,
	    VXGE_HAL_XMAC_NWIF_ActConfig_ActivePort,
	    &active_config);

	if (status == VXGE_HAL_OK)
		vdev->active_port = active_config;
}

vxge_hal_status_e
vxge_l2switch_mode_set(vxge_dev_t *vdev)
{
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	status = vxge_hal_set_l2switch_mode(vdev->devh,
	    vdev->config.l2_switch);

	device_printf(vdev->ndev, "L2 switch %s\n",
	    (status == VXGE_HAL_OK) ?
	    (vdev->config.l2_switch) ? "enable" : "disable" :
	    "change failed");

	if (status == VXGE_HAL_OK)
		vxge_hal_set_fw_api(vdev->devh, 0ULL,
		    VXGE_HAL_API_FUNC_MODE_COMMIT,
		    0, 0ULL, 0ULL);

	return (status);
}

/*
 * vxge_promisc_set
 * Enable Promiscuous Mode
 */
void
vxge_promisc_set(vxge_dev_t *vdev)
{
	int i;
	ifnet_t ifp;
	vxge_hal_vpath_h vpath_handle;

	if (!vdev->is_initialized)
		return;

	ifp = vdev->ifp;

	for (i = 0; i < vdev->no_of_vpath; i++) {
		vpath_handle = vxge_vpath_handle_get(vdev, i);
		if (!vpath_handle)
			continue;

		if (ifp->if_flags & IFF_PROMISC)
			vxge_hal_vpath_promisc_enable(vpath_handle);
		else
			vxge_hal_vpath_promisc_disable(vpath_handle);
	}
}

/*
 * vxge_change_mtu
 * Change interface MTU to a requested valid size
 */
int
vxge_change_mtu(vxge_dev_t *vdev, unsigned long new_mtu)
{
	int err = EINVAL;

	if ((new_mtu < VXGE_HAL_MIN_MTU) || (new_mtu > VXGE_HAL_MAX_MTU))
		goto _exit0;

	(vdev->ifp)->if_mtu = new_mtu;
	device_printf(vdev->ndev, "MTU changed to %u\n", (vdev->ifp)->if_mtu);

	if (vdev->is_initialized) {
		if_down(vdev->ifp);
		vxge_reset(vdev);
		if_up(vdev->ifp);
	}
	err = 0;

_exit0:
	return (err);
}

/*
 * Creates DMA tags for both Tx and Rx
 */
int
vxge_dma_tags_create(vxge_vpath_t *vpath)
{
	int err = 0;
	bus_size_t max_size, boundary;
	vxge_dev_t *vdev = vpath->vdev;
	ifnet_t ifp = vdev->ifp;

	max_size = ifp->if_mtu +
	    VXGE_HAL_MAC_HEADER_MAX_SIZE +
	    VXGE_HAL_HEADER_ETHERNET_II_802_3_ALIGN;

	VXGE_BUFFER_ALIGN(max_size, 128)
	if (max_size <= MCLBYTES)
		vdev->rx_mbuf_sz = MCLBYTES;
	else
		vdev->rx_mbuf_sz =
		    (max_size > MJUMPAGESIZE) ? MJUM9BYTES : MJUMPAGESIZE;

	boundary = (max_size > PAGE_SIZE) ? 0 : PAGE_SIZE;

	/* DMA tag for Tx */
	err = bus_dma_tag_create(
	    bus_get_dma_tag(vdev->ndev),
	    1,
	    PAGE_SIZE,
	    BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR,
	    NULL,
	    NULL,
	    VXGE_TSO_SIZE,
	    VXGE_MAX_SEGS,
	    PAGE_SIZE,
	    BUS_DMA_ALLOCNOW,
	    NULL,
	    NULL,
	    &(vpath->dma_tag_tx));
	if (err != 0)
		goto _exit0;

	/* DMA tag for Rx */
	err = bus_dma_tag_create(
	    bus_get_dma_tag(vdev->ndev),
	    1,
	    boundary,
	    BUS_SPACE_MAXADDR,
	    BUS_SPACE_MAXADDR,
	    NULL,
	    NULL,
	    vdev->rx_mbuf_sz,
	    1,
	    vdev->rx_mbuf_sz,
	    BUS_DMA_ALLOCNOW,
	    NULL,
	    NULL,
	    &(vpath->dma_tag_rx));
	if (err != 0)
		goto _exit1;

	/* Create DMA map for this descriptor */
	err = bus_dmamap_create(vpath->dma_tag_rx, BUS_DMA_NOWAIT,
	    &vpath->extra_dma_map);
	if (err == 0)
		goto _exit0;

	bus_dma_tag_destroy(vpath->dma_tag_rx);

_exit1:
	bus_dma_tag_destroy(vpath->dma_tag_tx);

_exit0:
	return (err);
}

static inline int
vxge_dma_mbuf_coalesce(bus_dma_tag_t dma_tag_tx, bus_dmamap_t dma_map,
    mbuf_t * m_headp, bus_dma_segment_t * dma_buffers,
    int *num_segs)
{
	int err = 0;
	mbuf_t mbuf_pkt = NULL;

retry:
	err = bus_dmamap_load_mbuf_sg(dma_tag_tx, dma_map, *m_headp,
	    dma_buffers, num_segs, BUS_DMA_NOWAIT);
	if (err == EFBIG) {
		/* try to defrag, too many segments */
		mbuf_pkt = m_defrag(*m_headp, M_NOWAIT);
		if (mbuf_pkt == NULL) {
			err = ENOBUFS;
			goto _exit0;
		}
		*m_headp = mbuf_pkt;
		goto retry;
	}

_exit0:
	return (err);
}

int
vxge_device_hw_info_get(vxge_dev_t *vdev)
{
	int i, err = ENXIO;
	u64 vpath_mask = 0;
	u32 max_supported_vpath = 0;
	u32 fw_ver_maj_min;
	vxge_firmware_upgrade_e fw_option;

	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_device_hw_info_t *hw_info;

	status = vxge_hal_device_hw_info_get(vdev->pdev,
	    (pci_reg_h) vdev->pdev->reg_map[0],
	    (u8 *) vdev->pdev->bar_info[0],
	    &vdev->config.hw_info);

	if (status != VXGE_HAL_OK)
		goto _exit0;

	hw_info = &vdev->config.hw_info;

	vpath_mask = hw_info->vpath_mask;
	if (vpath_mask == 0) {
		device_printf(vdev->ndev, "No vpaths available in device\n");
		goto _exit0;
	}

	fw_option = vdev->config.fw_option;

	/* Check how many vpaths are available */
	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if (!((vpath_mask) & mBIT(i)))
			continue;
		max_supported_vpath++;
	}

	vdev->max_supported_vpath = max_supported_vpath;
	status = vxge_hal_device_is_privileged(hw_info->host_type,
	    hw_info->func_id);
	vdev->is_privilaged = (status == VXGE_HAL_OK) ? TRUE : FALSE;

	vdev->hw_fw_version = VXGE_FW_VERSION(
	    hw_info->fw_version.major,
	    hw_info->fw_version.minor,
	    hw_info->fw_version.build);

	fw_ver_maj_min =
	    VXGE_FW_MAJ_MIN_VERSION(hw_info->fw_version.major,
	    hw_info->fw_version.minor);

	if ((fw_option >= VXGE_FW_UPGRADE_FORCE) ||
	    (vdev->hw_fw_version != VXGE_DRV_FW_VERSION)) {

		/* For fw_ver 1.8.1 and above ignore build number. */
		if ((fw_option == VXGE_FW_UPGRADE_ALL) &&
		    ((vdev->hw_fw_version >= VXGE_FW_VERSION(1, 8, 1)) &&
		    (fw_ver_maj_min == VXGE_DRV_FW_MAJ_MIN_VERSION))) {
			goto _exit1;
		}

		if (vdev->hw_fw_version < VXGE_BASE_FW_VERSION) {
			device_printf(vdev->ndev,
			    "Upgrade driver through vxge_update, "
			    "Unable to load the driver.\n");
			goto _exit0;
		}
		vdev->fw_upgrade = TRUE;
	}

_exit1:
	err = 0;

_exit0:
	return (err);
}

/*
 * vxge_device_hw_info_print
 * Print device and driver information
 */
void
vxge_device_hw_info_print(vxge_dev_t *vdev)
{
	u32 i;
	device_t ndev;
	struct sysctl_ctx_list *ctx;
	struct sysctl_oid_list *children;
	char pmd_type[2][VXGE_PMD_INFO_LEN];

	vxge_hal_device_t *hldev;
	vxge_hal_device_hw_info_t *hw_info;
	vxge_hal_device_pmd_info_t *pmd_port;

	hldev = vdev->devh;
	ndev = vdev->ndev;

	ctx = device_get_sysctl_ctx(ndev);
	children = SYSCTL_CHILDREN(device_get_sysctl_tree(ndev));

	hw_info = &(vdev->config.hw_info);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_DRV_VERSION],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_DRV_VERSION]),
	    "%d.%d.%d.%d", XGELL_VERSION_MAJOR, XGELL_VERSION_MINOR,
	    XGELL_VERSION_FIX, XGELL_VERSION_BUILD);

	/* Print PCI-e bus type/speed/width info */
	snprintf(vdev->config.nic_attr[VXGE_PRINT_PCIE_INFO],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_PCIE_INFO]),
	    "x%d", hldev->link_width);

	if (hldev->link_width <= VXGE_HAL_PCI_E_LINK_WIDTH_X4)
		device_printf(ndev, "For optimal performance a x8 "
		    "PCI-Express slot is required.\n");

	vxge_null_terminate((char *) hw_info->serial_number,
	    sizeof(hw_info->serial_number));

	vxge_null_terminate((char *) hw_info->part_number,
	    sizeof(hw_info->part_number));

	snprintf(vdev->config.nic_attr[VXGE_PRINT_SERIAL_NO],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_SERIAL_NO]),
	    "%s", hw_info->serial_number);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_PART_NO],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_PART_NO]),
	    "%s", hw_info->part_number);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_FW_VERSION],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_FW_VERSION]),
	    "%s", hw_info->fw_version.version);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_FW_DATE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_FW_DATE]),
	    "%s", hw_info->fw_date.date);

	pmd_port = &(hw_info->pmd_port0);
	for (i = 0; i < hw_info->ports; i++) {

		vxge_pmd_port_type_get(vdev, pmd_port->type,
		    pmd_type[i], sizeof(pmd_type[i]));

		strncpy(vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0 + i],
		    "vendor=??, sn=??, pn=??, type=??",
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0 + i]));

		vxge_null_terminate(pmd_port->vendor, sizeof(pmd_port->vendor));
		if (strlen(pmd_port->vendor) == 0) {
			pmd_port = &(hw_info->pmd_port1);
			continue;
		}

		vxge_null_terminate(pmd_port->ser_num,
		    sizeof(pmd_port->ser_num));

		vxge_null_terminate(pmd_port->part_num,
		    sizeof(pmd_port->part_num));

		snprintf(vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0 + i],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0 + i]),
		    "vendor=%s, sn=%s, pn=%s, type=%s",
		    pmd_port->vendor, pmd_port->ser_num,
		    pmd_port->part_num, pmd_type[i]);

		pmd_port = &(hw_info->pmd_port1);
	}

	switch (hw_info->function_mode) {
	case VXGE_HAL_PCIE_FUNC_MODE_SF1_VP17:
		snprintf(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]),
		    "%s %d %s", "Single Function - 1 function(s)",
		    vdev->max_supported_vpath, "VPath(s)/function");
		break;

	case VXGE_HAL_PCIE_FUNC_MODE_MF2_VP8:
		snprintf(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]),
		    "%s %d %s", "Multi Function - 2 function(s)",
		    vdev->max_supported_vpath, "VPath(s)/function");
		break;

	case VXGE_HAL_PCIE_FUNC_MODE_MF4_VP4:
		snprintf(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]),
		    "%s %d %s", "Multi Function - 4 function(s)",
		    vdev->max_supported_vpath, "VPath(s)/function");
		break;

	case VXGE_HAL_PCIE_FUNC_MODE_MF8_VP2:
		snprintf(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]),
		    "%s %d %s", "Multi Function - 8 function(s)",
		    vdev->max_supported_vpath, "VPath(s)/function");
		break;

	case VXGE_HAL_PCIE_FUNC_MODE_MF8P_VP2:
		snprintf(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    sizeof(vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]),
		    "%s %d %s", "Multi Function (DirectIO) - 8 function(s)",
		    vdev->max_supported_vpath, "VPath(s)/function");
		break;
	}

	snprintf(vdev->config.nic_attr[VXGE_PRINT_INTR_MODE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_INTR_MODE]),
	    "%s", ((vdev->config.intr_mode == VXGE_HAL_INTR_MODE_MSIX) ?
	    "MSI-X" : "INTA"));

	snprintf(vdev->config.nic_attr[VXGE_PRINT_VPATH_COUNT],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_VPATH_COUNT]),
	    "%d", vdev->no_of_vpath);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_MTU_SIZE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_MTU_SIZE]),
	    "%u", vdev->ifp->if_mtu);

	snprintf(vdev->config.nic_attr[VXGE_PRINT_LRO_MODE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_LRO_MODE]),
	    "%s", ((vdev->config.lro_enable) ? "Enabled" : "Disabled"));

	snprintf(vdev->config.nic_attr[VXGE_PRINT_RTH_MODE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_RTH_MODE]),
	    "%s", ((vdev->config.rth_enable) ? "Enabled" : "Disabled"));

	snprintf(vdev->config.nic_attr[VXGE_PRINT_TSO_MODE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_TSO_MODE]),
	    "%s", ((vdev->ifp->if_capenable & IFCAP_TSO4) ?
	    "Enabled" : "Disabled"));

	snprintf(vdev->config.nic_attr[VXGE_PRINT_ADAPTER_TYPE],
	    sizeof(vdev->config.nic_attr[VXGE_PRINT_ADAPTER_TYPE]),
	    "%s", ((hw_info->ports == 1) ? "Single Port" : "Dual Port"));

	if (vdev->is_privilaged) {

		if (hw_info->ports > 1) {

			snprintf(vdev->config.nic_attr[VXGE_PRINT_PORT_MODE],
			    sizeof(vdev->config.nic_attr[VXGE_PRINT_PORT_MODE]),
			    "%s", vxge_port_mode[vdev->port_mode]);

			if (vdev->port_mode != VXGE_HAL_DP_NP_MODE_SINGLE_PORT)
				snprintf(vdev->config.nic_attr[VXGE_PRINT_PORT_FAILURE],
				    sizeof(vdev->config.nic_attr[VXGE_PRINT_PORT_FAILURE]),
				    "%s", vxge_port_failure[vdev->port_failure]);

			vxge_active_port_update(vdev);
			snprintf(vdev->config.nic_attr[VXGE_PRINT_ACTIVE_PORT],
			    sizeof(vdev->config.nic_attr[VXGE_PRINT_ACTIVE_PORT]),
			    "%lld", vdev->active_port);
		}

		if (!is_single_func(hw_info->function_mode)) {
			snprintf(vdev->config.nic_attr[VXGE_PRINT_L2SWITCH_MODE],
			    sizeof(vdev->config.nic_attr[VXGE_PRINT_L2SWITCH_MODE]),
			    "%s", ((vdev->l2_switch) ? "Enabled" : "Disabled"));
		}
	}

	device_printf(ndev, "Driver version\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_DRV_VERSION]);

	device_printf(ndev, "Serial number\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_SERIAL_NO]);

	device_printf(ndev, "Part number\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_PART_NO]);

	device_printf(ndev, "Firmware version\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_FW_VERSION]);

	device_printf(ndev, "Firmware date\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_FW_DATE]);

	device_printf(ndev, "Link width\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_PCIE_INFO]);

	if (vdev->is_privilaged) {
		device_printf(ndev, "Function mode\t: %s\n",
		    vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE]);
	}

	device_printf(ndev, "Interrupt type\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_INTR_MODE]);

	device_printf(ndev, "VPath(s) opened\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_VPATH_COUNT]);

	device_printf(ndev, "Adapter Type\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_ADAPTER_TYPE]);

	device_printf(ndev, "PMD Port 0\t: %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0]);

	if (hw_info->ports > 1) {
		device_printf(ndev, "PMD Port 1\t: %s\n",
		    vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_1]);

		if (vdev->is_privilaged) {
			device_printf(ndev, "Port Mode\t: %s\n",
			    vdev->config.nic_attr[VXGE_PRINT_PORT_MODE]);

			if (vdev->port_mode != VXGE_HAL_DP_NP_MODE_SINGLE_PORT)
				device_printf(ndev, "Port Failure\t: %s\n",
				    vdev->config.nic_attr[VXGE_PRINT_PORT_FAILURE]);

			device_printf(vdev->ndev, "Active Port\t: %s\n",
			    vdev->config.nic_attr[VXGE_PRINT_ACTIVE_PORT]);
		}
	}

	if (vdev->is_privilaged && !is_single_func(hw_info->function_mode)) {
		device_printf(vdev->ndev, "L2 Switch\t: %s\n",
		    vdev->config.nic_attr[VXGE_PRINT_L2SWITCH_MODE]);
	}

	device_printf(ndev, "MTU is %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_MTU_SIZE]);

	device_printf(ndev, "LRO %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_LRO_MODE]);

	device_printf(ndev, "RTH %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_RTH_MODE]);

	device_printf(ndev, "TSO %s\n",
	    vdev->config.nic_attr[VXGE_PRINT_TSO_MODE]);

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Driver version", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_DRV_VERSION],
	    0, "Driver version");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Serial number", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_SERIAL_NO],
	    0, "Serial number");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Part number", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_PART_NO],
	    0, "Part number");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Firmware version", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_FW_VERSION],
	    0, "Firmware version");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Firmware date", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_FW_DATE],
	    0, "Firmware date");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Link width", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_PCIE_INFO],
	    0, "Link width");

	if (vdev->is_privilaged) {
		SYSCTL_ADD_STRING(ctx, children,
		    OID_AUTO, "Function mode", CTLFLAG_RD,
		    vdev->config.nic_attr[VXGE_PRINT_FUNC_MODE],
		    0, "Function mode");
	}

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Interrupt type", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_INTR_MODE],
	    0, "Interrupt type");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "VPath(s) opened", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_VPATH_COUNT],
	    0, "VPath(s) opened");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "Adapter Type", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_ADAPTER_TYPE],
	    0, "Adapter Type");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "pmd port 0", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_0],
	    0, "pmd port");

	if (hw_info->ports > 1) {

		SYSCTL_ADD_STRING(ctx, children,
		    OID_AUTO, "pmd port 1", CTLFLAG_RD,
		    vdev->config.nic_attr[VXGE_PRINT_PMD_PORTS_1],
		    0, "pmd port");

		if (vdev->is_privilaged) {
			SYSCTL_ADD_STRING(ctx, children,
			    OID_AUTO, "Port Mode", CTLFLAG_RD,
			    vdev->config.nic_attr[VXGE_PRINT_PORT_MODE],
			    0, "Port Mode");

			if (vdev->port_mode != VXGE_HAL_DP_NP_MODE_SINGLE_PORT)
				SYSCTL_ADD_STRING(ctx, children,
				    OID_AUTO, "Port Failure", CTLFLAG_RD,
				    vdev->config.nic_attr[VXGE_PRINT_PORT_FAILURE],
				    0, "Port Failure");

			SYSCTL_ADD_STRING(ctx, children,
			    OID_AUTO, "L2 Switch", CTLFLAG_RD,
			    vdev->config.nic_attr[VXGE_PRINT_L2SWITCH_MODE],
			    0, "L2 Switch");
		}
	}

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "LRO mode", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_LRO_MODE],
	    0, "LRO mode");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "RTH mode", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_RTH_MODE],
	    0, "RTH mode");

	SYSCTL_ADD_STRING(ctx, children,
	    OID_AUTO, "TSO mode", CTLFLAG_RD,
	    vdev->config.nic_attr[VXGE_PRINT_TSO_MODE],
	    0, "TSO mode");
}

void
vxge_pmd_port_type_get(vxge_dev_t *vdev, u32 port_type,
    char *ifm_name, u8 ifm_len)
{

	vdev->ifm_optics = IFM_UNKNOWN;

	switch (port_type) {
	case VXGE_HAL_DEVICE_PMD_TYPE_10G_SR:
		vdev->ifm_optics = IFM_10G_SR;
		strlcpy(ifm_name, "10GbE SR", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_LR:
		vdev->ifm_optics = IFM_10G_LR;
		strlcpy(ifm_name, "10GbE LR", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_LRM:
		vdev->ifm_optics = IFM_10G_LRM;
		strlcpy(ifm_name, "10GbE LRM", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_DIRECT:
		vdev->ifm_optics = IFM_10G_TWINAX;
		strlcpy(ifm_name, "10GbE DA (Direct Attached)", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_CX4:
		vdev->ifm_optics = IFM_10G_CX4;
		strlcpy(ifm_name, "10GbE CX4", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_BASE_T:
#if __FreeBSD_version >= 800000
		vdev->ifm_optics = IFM_10G_T;
#endif
		strlcpy(ifm_name, "10GbE baseT", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_10G_OTHER:
		strlcpy(ifm_name, "10GbE Other", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_SX:
		vdev->ifm_optics = IFM_1000_SX;
		strlcpy(ifm_name, "1GbE SX", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_LX:
		vdev->ifm_optics = IFM_1000_LX;
		strlcpy(ifm_name, "1GbE LX", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_CX:
		vdev->ifm_optics = IFM_1000_CX;
		strlcpy(ifm_name, "1GbE CX", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_BASE_T:
		vdev->ifm_optics = IFM_1000_T;
		strlcpy(ifm_name, "1GbE baseT", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_DIRECT:
		strlcpy(ifm_name, "1GbE DA (Direct Attached)",
		    ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_CX4:
		strlcpy(ifm_name, "1GbE CX4", ifm_len);
		break;

	case VXGE_HAL_DEVICE_PMD_TYPE_1G_OTHER:
		strlcpy(ifm_name, "1GbE Other", ifm_len);
		break;

	default:
	case VXGE_HAL_DEVICE_PMD_TYPE_UNKNOWN:
		strlcpy(ifm_name, "UNSUP", ifm_len);
		break;
	}
}

u32
vxge_ring_length_get(u32 buffer_mode)
{
	return (VXGE_DEFAULT_RING_BLOCK *
	    vxge_hal_ring_rxds_per_block_get(buffer_mode));
}

/*
 * Removes trailing spaces padded
 * and NULL terminates strings
 */
static inline void
vxge_null_terminate(char *str, size_t len)
{
	len--;
	while (*str && (*str != ' ') && (len != 0))
		++str;

	--len;
	if (*str)
		*str = '\0';
}

/*
 * vxge_ioctl
 * Callback to control the device
 */
int
vxge_ioctl(ifnet_t ifp, u_long command, caddr_t data)
{
	int mask, err = 0;
	vxge_dev_t *vdev = (vxge_dev_t *) ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;

	if (!vdev->is_active)
		return (EBUSY);

	switch (command) {
		/* Set Interface MTU */
	case SIOCSIFMTU:
		err = vxge_change_mtu(vdev, (unsigned long)ifr->ifr_mtu);
		break;

		/* Set Interface Flags */
	case SIOCSIFFLAGS:
		VXGE_DRV_LOCK(vdev);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ vdev->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI))
					vxge_promisc_set(vdev);
			} else {
				vxge_init_locked(vdev);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				vxge_stop_locked(vdev);
		}
		vdev->if_flags = ifp->if_flags;
		VXGE_DRV_UNLOCK(vdev);
		break;

		/* Add/delete multicast address */
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

		/* Get/Set Interface Media */
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		err = ifmedia_ioctl(ifp, ifr, &vdev->media, command);
		break;

		/* Set Capabilities */
	case SIOCSIFCAP:
		VXGE_DRV_LOCK(vdev);
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;

		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable ^= IFCAP_TXCSUM;
			ifp->if_hwassist ^= (CSUM_TCP | CSUM_UDP | CSUM_IP);

			if ((ifp->if_capenable & IFCAP_TSO) &&
			    !(ifp->if_capenable & IFCAP_TXCSUM)) {

				ifp->if_capenable &= ~IFCAP_TSO;
				ifp->if_hwassist &= ~CSUM_TSO;
				if_printf(ifp, "TSO Disabled\n");
			}
		}
		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;

		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;

			if (ifp->if_capenable & IFCAP_TSO) {
				if (ifp->if_capenable & IFCAP_TXCSUM) {
					ifp->if_hwassist |= CSUM_TSO;
					if_printf(ifp, "TSO Enabled\n");
				} else {
					ifp->if_capenable &= ~IFCAP_TSO;
					ifp->if_hwassist &= ~CSUM_TSO;
					if_printf(ifp,
					    "Enable tx checksum offload \
					     first.\n");
					err = EAGAIN;
				}
			} else {
				ifp->if_hwassist &= ~CSUM_TSO;
				if_printf(ifp, "TSO Disabled\n");
			}
		}
		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;

		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;

		if (mask & IFCAP_VLAN_MTU)
			ifp->if_capenable ^= IFCAP_VLAN_MTU;

		if (mask & IFCAP_VLAN_HWCSUM)
			ifp->if_capenable ^= IFCAP_VLAN_HWCSUM;

#if __FreeBSD_version >= 800000
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
#endif

#if defined(VLAN_CAPABILITIES)
		VLAN_CAPABILITIES(ifp);
#endif

		VXGE_DRV_UNLOCK(vdev);
		break;

	case SIOCGPRIVATE_0:
		VXGE_DRV_LOCK(vdev);
		err = vxge_ioctl_stats(vdev, ifr);
		VXGE_DRV_UNLOCK(vdev);
		break;

	case SIOCGPRIVATE_1:
		VXGE_DRV_LOCK(vdev);
		err = vxge_ioctl_regs(vdev, ifr);
		VXGE_DRV_UNLOCK(vdev);
		break;

	default:
		err = ether_ioctl(ifp, command, data);
		break;
	}

	return (err);
}

/*
 * vxge_ioctl_regs
 * IOCTL to get registers
 */
int
vxge_ioctl_regs(vxge_dev_t *vdev, struct ifreq *ifr)
{
	u64 value = 0x0;
	u32 vp_id = 0;
	u32 offset, reqd_size = 0;
	int i, err = EINVAL;

	char *command = ifr_data_get_ptr(ifr);
	void *reg_info = ifr_data_get_ptr(ifr);

	vxge_vpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_mgmt_reg_type_e regs_type;

	switch (*command) {
	case vxge_hal_mgmt_reg_type_pcicfgmgmt:
		if (vdev->is_privilaged) {
			reqd_size = sizeof(vxge_hal_pcicfgmgmt_reg_t);
			regs_type = vxge_hal_mgmt_reg_type_pcicfgmgmt;
		}
		break;

	case vxge_hal_mgmt_reg_type_mrpcim:
		if (vdev->is_privilaged) {
			reqd_size = sizeof(vxge_hal_mrpcim_reg_t);
			regs_type = vxge_hal_mgmt_reg_type_mrpcim;
		}
		break;

	case vxge_hal_mgmt_reg_type_srpcim:
		if (vdev->is_privilaged) {
			reqd_size = sizeof(vxge_hal_srpcim_reg_t);
			regs_type = vxge_hal_mgmt_reg_type_srpcim;
		}
		break;

	case vxge_hal_mgmt_reg_type_memrepair:
		if (vdev->is_privilaged) {
			/* reqd_size = sizeof(vxge_hal_memrepair_reg_t); */
			regs_type = vxge_hal_mgmt_reg_type_memrepair;
		}
		break;

	case vxge_hal_mgmt_reg_type_legacy:
		reqd_size = sizeof(vxge_hal_legacy_reg_t);
		regs_type = vxge_hal_mgmt_reg_type_legacy;
		break;

	case vxge_hal_mgmt_reg_type_toc:
		reqd_size = sizeof(vxge_hal_toc_reg_t);
		regs_type = vxge_hal_mgmt_reg_type_toc;
		break;

	case vxge_hal_mgmt_reg_type_common:
		reqd_size = sizeof(vxge_hal_common_reg_t);
		regs_type = vxge_hal_mgmt_reg_type_common;
		break;

	case vxge_hal_mgmt_reg_type_vpmgmt:
		reqd_size = sizeof(vxge_hal_vpmgmt_reg_t);
		regs_type = vxge_hal_mgmt_reg_type_vpmgmt;
		vpath = &(vdev->vpaths[*((u32 *) reg_info + 1)]);
		vp_id = vpath->vp_id;
		break;

	case vxge_hal_mgmt_reg_type_vpath:
		reqd_size = sizeof(vxge_hal_vpath_reg_t);
		regs_type = vxge_hal_mgmt_reg_type_vpath;
		vpath = &(vdev->vpaths[*((u32 *) reg_info + 1)]);
		vp_id = vpath->vp_id;
		break;

	case VXGE_GET_VPATH_COUNT:
		*((u32 *) reg_info) = vdev->no_of_vpath;
		err = 0;
		break;

	default:
		reqd_size = 0;
		break;
	}

	if (reqd_size) {
		for (i = 0, offset = 0; offset < reqd_size;
		    i++, offset += 0x0008) {
			value = 0x0;
			status = vxge_hal_mgmt_reg_read(vdev->devh, regs_type,
			    vp_id, offset, &value);

			err = (status != VXGE_HAL_OK) ? EINVAL : 0;
			if (err == EINVAL)
				break;

			*((u64 *) ((u64 *) reg_info + i)) = value;
		}
	}
	return (err);
}

/*
 * vxge_ioctl_stats
 * IOCTL to get statistics
 */
int
vxge_ioctl_stats(vxge_dev_t *vdev, struct ifreq *ifr)
{
	int i, retsize, err = EINVAL;
	u32 bufsize;

	vxge_vpath_t *vpath;
	vxge_bw_info_t *bw_info;
	vxge_port_info_t *port_info;
	vxge_drv_stats_t *drv_stat;

	char *buffer = NULL;
	char *command = ifr_data_get_ptr(ifr);
	vxge_hal_status_e status = VXGE_HAL_OK;

	switch (*command) {
	case VXGE_GET_PCI_CONF:
		bufsize = VXGE_STATS_BUFFER_SIZE;
		buffer = (char *) vxge_mem_alloc(bufsize);
		if (buffer != NULL) {
			status = vxge_hal_aux_pci_config_read(vdev->devh,
			    bufsize, buffer, &retsize);
			if (status == VXGE_HAL_OK)
				err = copyout(buffer, ifr_data_get_ptr(ifr),
				    retsize);
			else
				device_printf(vdev->ndev,
				    "failed pciconfig statistics query\n");

			vxge_mem_free(buffer, bufsize);
		}
		break;

	case VXGE_GET_MRPCIM_STATS:
		if (!vdev->is_privilaged)
			break;

		bufsize = VXGE_STATS_BUFFER_SIZE;
		buffer = (char *) vxge_mem_alloc(bufsize);
		if (buffer != NULL) {
			status = vxge_hal_aux_stats_mrpcim_read(vdev->devh,
			    bufsize, buffer, &retsize);
			if (status == VXGE_HAL_OK)
				err = copyout(buffer, ifr_data_get_ptr(ifr),
				    retsize);
			else
				device_printf(vdev->ndev,
				    "failed mrpcim statistics query\n");

			vxge_mem_free(buffer, bufsize);
		}
		break;

	case VXGE_GET_DEVICE_STATS:
		bufsize = VXGE_STATS_BUFFER_SIZE;
		buffer = (char *) vxge_mem_alloc(bufsize);
		if (buffer != NULL) {
			status = vxge_hal_aux_stats_device_read(vdev->devh,
			    bufsize, buffer, &retsize);
			if (status == VXGE_HAL_OK)
				err = copyout(buffer, ifr_data_get_ptr(ifr),
				    retsize);
			else
				device_printf(vdev->ndev,
				    "failed device statistics query\n");

			vxge_mem_free(buffer, bufsize);
		}
		break;

	case VXGE_GET_DEVICE_HWINFO:
		bufsize = sizeof(vxge_device_hw_info_t);
		buffer = (char *) vxge_mem_alloc(bufsize);
		if (buffer != NULL) {
			vxge_os_memcpy(
			    &(((vxge_device_hw_info_t *) buffer)->hw_info),
			    &vdev->config.hw_info,
			    sizeof(vxge_hal_device_hw_info_t));

			((vxge_device_hw_info_t *) buffer)->port_mode =
			    vdev->port_mode;

			((vxge_device_hw_info_t *) buffer)->port_failure =
			    vdev->port_failure;

			err = copyout(buffer, ifr_data_get_ptr(ifr), bufsize);
			if (err != 0)
				device_printf(vdev->ndev,
				    "failed device hardware info query\n");

			vxge_mem_free(buffer, bufsize);
		}
		break;

	case VXGE_GET_DRIVER_STATS:
		bufsize = sizeof(vxge_drv_stats_t) * vdev->no_of_vpath;
		drv_stat = (vxge_drv_stats_t *) vxge_mem_alloc(bufsize);
		if (drv_stat != NULL) {
			for (i = 0; i < vdev->no_of_vpath; i++) {
				vpath = &(vdev->vpaths[i]);

				vpath->driver_stats.rx_lro_queued +=
				    vpath->lro.lro_queued;

				vpath->driver_stats.rx_lro_flushed +=
				    vpath->lro.lro_flushed;

				vxge_os_memcpy(&drv_stat[i],
				    &(vpath->driver_stats),
				    sizeof(vxge_drv_stats_t));
			}

			err = copyout(drv_stat, ifr_data_get_ptr(ifr), bufsize);
			if (err != 0)
				device_printf(vdev->ndev,
				    "failed driver statistics query\n");

			vxge_mem_free(drv_stat, bufsize);
		}
		break;

	case VXGE_GET_BANDWIDTH:
		bw_info = ifr_data_get_ptr(ifr);

		if ((vdev->config.hw_info.func_id != 0) &&
		    (vdev->hw_fw_version < VXGE_FW_VERSION(1, 8, 0)))
			break;

		if (vdev->config.hw_info.func_id != 0)
			bw_info->func_id = vdev->config.hw_info.func_id;

		status = vxge_bw_priority_get(vdev, bw_info);
		if (status != VXGE_HAL_OK)
			break;

		err = copyout(bw_info, ifr_data_get_ptr(ifr),
		    sizeof(vxge_bw_info_t));
		break;

	case VXGE_SET_BANDWIDTH:
		if (vdev->is_privilaged)
			err = vxge_bw_priority_set(vdev, ifr);
		break;

	case VXGE_SET_PORT_MODE:
		if (vdev->is_privilaged) {
			if (vdev->config.hw_info.ports == VXGE_DUAL_PORT_MODE) {
				port_info = ifr_data_get_ptr(ifr);
				vdev->config.port_mode = port_info->port_mode;
				err = vxge_port_mode_update(vdev);
				if (err != ENXIO)
					err = VXGE_HAL_FAIL;
				else {
					err = VXGE_HAL_OK;
					device_printf(vdev->ndev,
					    "PLEASE POWER CYCLE THE SYSTEM\n");
				}
			}
		}
		break;

	case VXGE_GET_PORT_MODE:
		if (vdev->is_privilaged) {
			if (vdev->config.hw_info.ports == VXGE_DUAL_PORT_MODE) {
				port_info = ifr_data_get_ptr(ifr);
				err = vxge_port_mode_get(vdev, port_info);
				if (err == VXGE_HAL_OK) {
					err = copyout(port_info,
					    ifr_data_get_ptr(ifr),
					    sizeof(vxge_port_info_t));
				}
			}
		}
		break;

	default:
		break;
	}

	return (err);
}

int
vxge_bw_priority_config(vxge_dev_t *vdev)
{
	u32 i;
	int err = EINVAL;

	for (i = 0; i < vdev->no_of_func; i++) {
		err = vxge_bw_priority_update(vdev, i, TRUE);
		if (err != 0)
			break;
	}

	return (err);
}

int
vxge_bw_priority_set(vxge_dev_t *vdev, struct ifreq *ifr)
{
	int err;
	u32 func_id;
	vxge_bw_info_t *bw_info;

	bw_info = ifr_data_get_ptr(ifr);
	func_id = bw_info->func_id;

	vdev->config.bw_info[func_id].priority = bw_info->priority;
	vdev->config.bw_info[func_id].bandwidth = bw_info->bandwidth;

	err = vxge_bw_priority_update(vdev, func_id, FALSE);

	return (err);
}

int
vxge_bw_priority_update(vxge_dev_t *vdev, u32 func_id, bool binit)
{
	u32 i, set = 0;
	u32 bandwidth, priority, vpath_count;
	u64 vpath_list[VXGE_HAL_MAX_VIRTUAL_PATHS];

	vxge_hal_device_t *hldev;
	vxge_hal_vp_config_t *vp_config;
	vxge_hal_status_e status = VXGE_HAL_OK;

	hldev = vdev->devh;

	status = vxge_hal_get_vpath_list(vdev->devh, func_id,
	    vpath_list, &vpath_count);

	if (status != VXGE_HAL_OK)
		return (status);

	for (i = 0; i < vpath_count; i++) {
		vp_config = &(hldev->config.vp_config[vpath_list[i]]);

		/* Configure Bandwidth */
		if (vdev->config.bw_info[func_id].bandwidth !=
		    VXGE_HAL_VPATH_BW_LIMIT_DEFAULT) {

			set = 1;
			bandwidth = vdev->config.bw_info[func_id].bandwidth;
			if (bandwidth < VXGE_HAL_VPATH_BW_LIMIT_MIN ||
			    bandwidth > VXGE_HAL_VPATH_BW_LIMIT_MAX) {

				bandwidth = VXGE_HAL_VPATH_BW_LIMIT_DEFAULT;
			}
			vp_config->bandwidth = bandwidth;
		}

		/*
		 * If b/w limiting is enabled on any of the
		 * VFs, then for remaining VFs set the priority to 3
		 * and b/w limiting to max i.e 10 Gb)
		 */
		if (vp_config->bandwidth == VXGE_HAL_VPATH_BW_LIMIT_DEFAULT)
			vp_config->bandwidth = VXGE_HAL_VPATH_BW_LIMIT_MAX;

		if (binit && vdev->config.low_latency) {
			if (func_id == 0)
				vdev->config.bw_info[func_id].priority =
				    VXGE_DEFAULT_VPATH_PRIORITY_HIGH;
		}

		/* Configure Priority */
		if (vdev->config.bw_info[func_id].priority !=
		    VXGE_HAL_VPATH_PRIORITY_DEFAULT) {

			set = 1;
			priority = vdev->config.bw_info[func_id].priority;
			if (priority < VXGE_HAL_VPATH_PRIORITY_MIN ||
			    priority > VXGE_HAL_VPATH_PRIORITY_MAX) {

				priority = VXGE_HAL_VPATH_PRIORITY_DEFAULT;
			}
			vp_config->priority = priority;

		} else if (vdev->config.low_latency) {
			set = 1;
			vp_config->priority = VXGE_DEFAULT_VPATH_PRIORITY_LOW;
		}

		if (set == 1) {
			status = vxge_hal_rx_bw_priority_set(vdev->devh,
			    vpath_list[i]);
			if (status != VXGE_HAL_OK)
				break;

			if (vpath_list[i] < VXGE_HAL_TX_BW_VPATH_LIMIT) {
				status = vxge_hal_tx_bw_priority_set(
				    vdev->devh, vpath_list[i]);
				if (status != VXGE_HAL_OK)
					break;
			}
		}
	}

	return ((status  == VXGE_HAL_OK) ? 0 : EINVAL);
}

/*
 * vxge_intr_coalesce_tx
 * Changes interrupt coalescing if the interrupts are not within a range
 * Return Value: Nothing
 */
void
vxge_intr_coalesce_tx(vxge_vpath_t *vpath)
{
	u32 timer;

	if (!vpath->tx_intr_coalesce)
		return;

	vpath->tx_interrupts++;
	if (ticks > vpath->tx_ticks + hz/100) {

		vpath->tx_ticks = ticks;
		timer = vpath->tti_rtimer_val;
		if (vpath->tx_interrupts > VXGE_MAX_TX_INTERRUPT_COUNT) {
			if (timer != VXGE_TTI_RTIMER_ADAPT_VAL) {
				vpath->tti_rtimer_val =
				    VXGE_TTI_RTIMER_ADAPT_VAL;

				vxge_hal_vpath_dynamic_tti_rtimer_set(
				    vpath->handle, vpath->tti_rtimer_val);
			}
		} else {
			if (timer != 0) {
				vpath->tti_rtimer_val = 0;
				vxge_hal_vpath_dynamic_tti_rtimer_set(
				    vpath->handle, vpath->tti_rtimer_val);
			}
		}
		vpath->tx_interrupts = 0;
	}
}

/*
 * vxge_intr_coalesce_rx
 * Changes interrupt coalescing if the interrupts are not within a range
 * Return Value: Nothing
 */
void
vxge_intr_coalesce_rx(vxge_vpath_t *vpath)
{
	u32 timer;

	if (!vpath->rx_intr_coalesce)
		return;

	vpath->rx_interrupts++;
	if (ticks > vpath->rx_ticks + hz/100) {

		vpath->rx_ticks = ticks;
		timer = vpath->rti_rtimer_val;

		if (vpath->rx_interrupts > VXGE_MAX_RX_INTERRUPT_COUNT) {
			if (timer != VXGE_RTI_RTIMER_ADAPT_VAL) {
				vpath->rti_rtimer_val =
				    VXGE_RTI_RTIMER_ADAPT_VAL;

				vxge_hal_vpath_dynamic_rti_rtimer_set(
				    vpath->handle, vpath->rti_rtimer_val);
			}
		} else {
			if (timer != 0) {
				vpath->rti_rtimer_val = 0;
				vxge_hal_vpath_dynamic_rti_rtimer_set(
				    vpath->handle, vpath->rti_rtimer_val);
			}
		}
		vpath->rx_interrupts = 0;
	}
}

/*
 * vxge_methods FreeBSD device interface entry points
 */
static device_method_t vxge_methods[] = {
	DEVMETHOD(device_probe, vxge_probe),
	DEVMETHOD(device_attach, vxge_attach),
	DEVMETHOD(device_detach, vxge_detach),
	DEVMETHOD(device_shutdown, vxge_shutdown),

	DEVMETHOD_END
};

static driver_t vxge_driver = {
	"vxge", vxge_methods, sizeof(vxge_dev_t),
};

static devclass_t vxge_devclass;

DRIVER_MODULE(vxge, pci, vxge_driver, vxge_devclass, 0, 0);
