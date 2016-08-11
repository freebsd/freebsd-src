/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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

******************************************************************************/
/*$FreeBSD$*/

#include "ixl.h"
#include "ixl_pf.h"

#ifdef PCI_IOV
#include "ixl_pf_iov.h"
#endif

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixl_driver_version[] = "1.6.6-k";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixl_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixl_vendor_info_t ixl_vendor_info_array[] =
{
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_XL710, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_C, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_A, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_C, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T4, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_1G_BASE_T_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_I_X722, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *ixl_strings[] = {
	"Intel(R) Ethernet Connection XL710/X722 Driver"
};


/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixl_probe(device_t);
static int      ixl_attach(device_t);
static int      ixl_detach(device_t);
static int      ixl_shutdown(device_t);

static int	ixl_save_pf_tunables(struct ixl_pf *);
static int	ixl_attach_get_link_status(struct ixl_pf *);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ixl_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixl_probe),
	DEVMETHOD(device_attach, ixl_attach),
	DEVMETHOD(device_detach, ixl_detach),
	DEVMETHOD(device_shutdown, ixl_shutdown),
#ifdef PCI_IOV
	DEVMETHOD(pci_iov_init, ixl_iov_init),
	DEVMETHOD(pci_iov_uninit, ixl_iov_uninit),
	DEVMETHOD(pci_iov_add_vf, ixl_add_vf),
#endif
	{0, 0}
};

static driver_t ixl_driver = {
	"ixl", ixl_methods, sizeof(struct ixl_pf),
};

devclass_t ixl_devclass;
DRIVER_MODULE(ixl, pci, ixl_driver, ixl_devclass, 0, 0);

MODULE_DEPEND(ixl, pci, 1, 1, 1);
MODULE_DEPEND(ixl, ether, 1, 1, 1);
#ifdef DEV_NETMAP
MODULE_DEPEND(ixl, netmap, 1, 1, 1);
#endif /* DEV_NETMAP */

/*
** TUNEABLE PARAMETERS:
*/

static SYSCTL_NODE(_hw, OID_AUTO, ixl, CTLFLAG_RD, 0,
                   "IXL driver parameters");

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int ixl_enable_msix = 1;
TUNABLE_INT("hw.ixl.enable_msix", &ixl_enable_msix);
SYSCTL_INT(_hw_ixl, OID_AUTO, enable_msix, CTLFLAG_RDTUN, &ixl_enable_msix, 0,
    "Enable MSI-X interrupts");

/*
** Number of descriptors per ring:
**   - TX and RX are the same size
*/
static int ixl_ring_size = DEFAULT_RING;
TUNABLE_INT("hw.ixl.ring_size", &ixl_ring_size);
SYSCTL_INT(_hw_ixl, OID_AUTO, ring_size, CTLFLAG_RDTUN,
    &ixl_ring_size, 0, "Descriptor Ring Size");

/* 
** This can be set manually, if left as 0 the
** number of queues will be calculated based
** on cpus and msix vectors available.
*/
static int ixl_max_queues = 0;
TUNABLE_INT("hw.ixl.max_queues", &ixl_max_queues);
SYSCTL_INT(_hw_ixl, OID_AUTO, max_queues, CTLFLAG_RDTUN,
    &ixl_max_queues, 0, "Number of Queues");

static int ixl_enable_tx_fc_filter = 1;
TUNABLE_INT("hw.ixl.enable_tx_fc_filter",
    &ixl_enable_tx_fc_filter);
SYSCTL_INT(_hw_ixl, OID_AUTO, enable_tx_fc_filter, CTLFLAG_RDTUN,
    &ixl_enable_tx_fc_filter, 0,
    "Filter out packets with Ethertype 0x8808 from being sent out by non-HW sources");

static int ixl_core_debug_mask = 0;
TUNABLE_INT("hw.ixl.core_debug_mask",
    &ixl_core_debug_mask);
SYSCTL_INT(_hw_ixl, OID_AUTO, core_debug_mask, CTLFLAG_RDTUN,
    &ixl_core_debug_mask, 0,
    "Display debug statements that are printed in non-shared code");

static int ixl_shared_debug_mask = 0;
TUNABLE_INT("hw.ixl.shared_debug_mask",
    &ixl_shared_debug_mask);
SYSCTL_INT(_hw_ixl, OID_AUTO, shared_debug_mask, CTLFLAG_RDTUN,
    &ixl_shared_debug_mask, 0,
    "Display debug statements that are printed in shared code");

/*
** Controls for Interrupt Throttling 
**	- true/false for dynamic adjustment
** 	- default values for static ITR
*/
static int ixl_dynamic_rx_itr = 1;
TUNABLE_INT("hw.ixl.dynamic_rx_itr", &ixl_dynamic_rx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, dynamic_rx_itr, CTLFLAG_RDTUN,
    &ixl_dynamic_rx_itr, 0, "Dynamic RX Interrupt Rate");

static int ixl_dynamic_tx_itr = 1;
TUNABLE_INT("hw.ixl.dynamic_tx_itr", &ixl_dynamic_tx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, dynamic_tx_itr, CTLFLAG_RDTUN,
    &ixl_dynamic_tx_itr, 0, "Dynamic TX Interrupt Rate");

static int ixl_rx_itr = IXL_ITR_8K;
TUNABLE_INT("hw.ixl.rx_itr", &ixl_rx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, rx_itr, CTLFLAG_RDTUN,
    &ixl_rx_itr, 0, "RX Interrupt Rate");

static int ixl_tx_itr = IXL_ITR_4K;
TUNABLE_INT("hw.ixl.tx_itr", &ixl_tx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, tx_itr, CTLFLAG_RDTUN,
    &ixl_tx_itr, 0, "TX Interrupt Rate");

#ifdef DEV_NETMAP
#define NETMAP_IXL_MAIN /* only bring in one part of the netmap code */
#include <dev/netmap/if_ixl_netmap.h>
#endif /* DEV_NETMAP */

/*********************************************************************
 *  Device identification routine
 *
 *  ixl_probe determines if the driver should be loaded on
 *  the hardware based on PCI vendor/device id of the device.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
ixl_probe(device_t dev)
{
	ixl_vendor_info_t *ent;

	u16	pci_vendor_id, pci_device_id;
	u16	pci_subvendor_id, pci_subdevice_id;
	char	device_name[256];

#if 0
	INIT_DEBUGOUT("ixl_probe: begin");
#endif
	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != I40E_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixl_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(device_name, "%s, Version - %s",
				ixl_strings[ent->index],
				ixl_driver_version);
			device_set_desc_copy(dev, device_name);
			return (BUS_PROBE_DEFAULT);
		}
		ent++;
	}
	return (ENXIO);
}

static int
ixl_attach_get_link_status(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int error = 0;

	if (((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver < 33)) ||
	    (hw->aq.fw_maj_ver < 4)) {
		i40e_msec_delay(75);
		error = i40e_aq_set_link_restart_an(hw, TRUE, NULL);
		if (error) {
			device_printf(dev, "link restart failed, aq_err=%d\n",
			    pf->hw.aq.asq_last_status);
			return error;
		}
	}

	/* Determine link state */
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);
	return (0);
}

/*
 * Sanity check and save off tunable values.
 */
static int
ixl_save_pf_tunables(struct ixl_pf *pf)
{
	device_t dev = pf->dev;

	/* Save tunable information */
	pf->enable_msix = ixl_enable_msix;
	pf->max_queues = ixl_max_queues;
	pf->ringsz = ixl_ring_size;
	pf->enable_tx_fc_filter = ixl_enable_tx_fc_filter;
	pf->dynamic_rx_itr = ixl_dynamic_rx_itr;
	pf->dynamic_tx_itr = ixl_dynamic_tx_itr;
	pf->tx_itr = ixl_tx_itr;
	pf->rx_itr = ixl_rx_itr;
	pf->dbg_mask = ixl_core_debug_mask;
	pf->hw.debug_mask = ixl_shared_debug_mask;

	if (ixl_ring_size < IXL_MIN_RING
	     || ixl_ring_size > IXL_MAX_RING
	     || ixl_ring_size % IXL_RING_INCREMENT != 0) {
		device_printf(dev, "Invalid ring_size value of %d set!\n",
		    ixl_ring_size);
		device_printf(dev, "ring_size must be between %d and %d, "
		    "inclusive, and must be a multiple of %d\n",
		    IXL_MIN_RING, IXL_MAX_RING, IXL_RING_INCREMENT);
		return (EINVAL);
	}

	return (0);
}

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
ixl_attach(device_t dev)
{
	struct ixl_pf	*pf;
	struct i40e_hw	*hw;
	struct ixl_vsi  *vsi;
	enum i40e_status_code status;
	int             error = 0;

	INIT_DEBUGOUT("ixl_attach: begin");

	/* Allocate, clear, and link in our primary soft structure */
	pf = device_get_softc(dev);
	pf->dev = pf->osdep.dev = dev;
	hw = &pf->hw;

	/*
	** Note this assumes we have a single embedded VSI,
	** this could be enhanced later to allocate multiple
	*/
	vsi = &pf->vsi;
	vsi->dev = pf->dev;

	/* Save tunable values */
	error = ixl_save_pf_tunables(pf);
	if (error)
		return (error);

	/* Core Lock Init*/
	IXL_PF_LOCK_INIT(pf, device_get_nameunit(dev));

	/* Set up the timer callout */
	callout_init_mtx(&pf->timer, &pf->pf_mtx, 0);

	/* Do PCI setup - map BAR0, etc */
	if (ixl_allocate_pci_resources(pf)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Establish a clean starting point */
	i40e_clear_hw(hw);
	status = i40e_pf_reset(hw);
	if (status) {
		device_printf(dev, "PF reset failure %s\n",
		    i40e_stat_str(hw, status));
		error = EIO;
		goto err_out;
	}

	/* Initialize the shared code */
	status = i40e_init_shared_code(hw);
	if (status) {
		device_printf(dev, "Unable to initialize shared code, error %s\n",
		    i40e_stat_str(hw, status));
		error = EIO;
		goto err_out;
	}

	/*
	 * Allocate interrupts and figure out number of queues to use
	 * for PF interface
	 */
	pf->msix = ixl_init_msix(pf);

	/* Set up the admin queue */
	hw->aq.num_arq_entries = IXL_AQ_LEN;
	hw->aq.num_asq_entries = IXL_AQ_LEN;
	hw->aq.arq_buf_size = IXL_AQ_BUF_SZ;
	hw->aq.asq_buf_size = IXL_AQ_BUF_SZ;

	status = i40e_init_adminq(hw);
	if (status != 0 && status != I40E_ERR_FIRMWARE_API_VERSION) {
		device_printf(dev, "Unable to initialize Admin Queue, error %s\n",
		    i40e_stat_str(hw, status));
		error = EIO;
		goto err_out;
	}
	ixl_print_nvm_version(pf);

	if (status == I40E_ERR_FIRMWARE_API_VERSION) {
		device_printf(dev, "The driver for the device stopped "
		    "because the NVM image is newer than expected.\n"
		    "You must install the most recent version of "
		    "the network driver.\n");
		error = EIO;
		goto err_out;
	}

        if (hw->aq.api_maj_ver == I40E_FW_API_VERSION_MAJOR &&
	    hw->aq.api_min_ver > I40E_FW_API_VERSION_MINOR)
		device_printf(dev, "The driver for the device detected "
		    "a newer version of the NVM image than expected.\n"
		    "Please install the most recent version of the network driver.\n");
	else if (hw->aq.api_maj_ver < I40E_FW_API_VERSION_MAJOR ||
	    hw->aq.api_min_ver < (I40E_FW_API_VERSION_MINOR - 1))
		device_printf(dev, "The driver for the device detected "
		    "an older version of the NVM image than expected.\n"
		    "Please update the NVM image.\n");

	/* Clear PXE mode */
	i40e_clear_pxe_mode(hw);

	/* Get capabilities from the device */
	error = ixl_get_hw_capabilities(pf);
	if (error) {
		device_printf(dev, "HW capabilities failure!\n");
		goto err_get_cap;
	}

	/* Set up host memory cache */
	status = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp, 0, 0);
	if (status) {
		device_printf(dev, "init_lan_hmc failed: %s\n",
		    i40e_stat_str(hw, status));
		goto err_get_cap;
	}

	status = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (status) {
		device_printf(dev, "configure_lan_hmc failed: %s\n",
		    i40e_stat_str(hw, status));
		goto err_mac_hmc;
	}

	/* Init queue allocation manager */
	error = ixl_pf_qmgr_init(&pf->qmgr, hw->func_caps.num_tx_qp);
	if (error) {
		device_printf(dev, "Failed to init queue manager for PF queues, error %d\n",
		    error);
		goto err_mac_hmc;
	}
	/* reserve a contiguous allocation for the PF's VSI */
	error = ixl_pf_qmgr_alloc_contiguous(&pf->qmgr, vsi->num_queues, &pf->qtag);
	if (error) {
		device_printf(dev, "Failed to reserve queues for PF LAN VSI, error %d\n",
		    error);
		goto err_mac_hmc;
	}
	device_printf(dev, "Allocating %d queues for PF LAN VSI; %d queues active\n",
	    pf->qtag.num_allocated, pf->qtag.num_active);

	/* Disable LLDP from the firmware for certain NVM versions */
	if (((pf->hw.aq.fw_maj_ver == 4) && (pf->hw.aq.fw_min_ver < 3)) ||
	    (pf->hw.aq.fw_maj_ver < 4))
		i40e_aq_stop_lldp(hw, TRUE, NULL);

	/* Get MAC addresses from hardware */
	i40e_get_mac_addr(hw, hw->mac.addr);
	error = i40e_validate_mac_addr(hw->mac.addr);
	if (error) {
		device_printf(dev, "validate_mac_addr failed: %d\n", error);
		goto err_mac_hmc;
	}
	bcopy(hw->mac.addr, hw->mac.perm_addr, ETHER_ADDR_LEN);
	i40e_get_port_mac_addr(hw, hw->mac.port_addr);

	/* Initialize mac filter list for VSI */
	SLIST_INIT(&vsi->ftl);

	/* Set up SW VSI and allocate queue memory and rings */
	if (ixl_setup_stations(pf)) { 
		device_printf(dev, "setup stations failed!\n");
		error = ENOMEM;
		goto err_mac_hmc;
	}

	/* Setup OS network interface / ifnet */
	if (ixl_setup_interface(dev, vsi)) {
		device_printf(dev, "interface setup failed!\n");
		error = EIO;
		goto err_late;
	}

	/* Determine link state */
	if (ixl_attach_get_link_status(pf)) {
		error = EINVAL;
		goto err_late;
	}

	error = ixl_switch_config(pf);
	if (error) {
		device_printf(dev, "Initial ixl_switch_config() failed: %d\n",
		     error);
		goto err_late;
	}

	/* Limit PHY interrupts to link, autoneg, and modules failure */
	status = i40e_aq_set_phy_int_mask(hw, IXL_DEFAULT_PHY_INT_MASK,
	    NULL);
        if (status) {
		device_printf(dev, "i40e_aq_set_phy_mask() failed: err %s,"
		    " aq_err %s\n", i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		goto err_late;
	}

	/* Get the bus configuration and set the shared code's config */
	ixl_get_bus_info(hw, dev);

	/*
	 * In MSI-X mode, initialize the Admin Queue interrupt,
	 * so userland tools can communicate with the adapter regardless of
	 * the ifnet interface's status.
	 */
	if (pf->msix > 1) {
		error = ixl_setup_adminq_msix(pf);
		if (error) {
			device_printf(dev, "ixl_setup_adminq_msix error: %d\n",
			    error);
			goto err_late;
		}
		error = ixl_setup_adminq_tq(pf);
		if (error) {
			device_printf(dev, "ixl_setup_adminq_tq error: %d\n",
			    error);
			goto err_late;
		}
		ixl_configure_intr0_msix(pf);
		ixl_enable_adminq(hw);
	}

	/* Initialize statistics & add sysctls */
	ixl_add_device_sysctls(pf);

	ixl_pf_reset_stats(pf);
	ixl_update_stats_counters(pf);
	ixl_add_hw_stats(pf);

	/* Register for VLAN events */
	vsi->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixl_register_vlan, vsi, EVENTHANDLER_PRI_FIRST);
	vsi->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixl_unregister_vlan, vsi, EVENTHANDLER_PRI_FIRST);

#ifdef PCI_IOV
	ixl_initialize_sriov(pf);
#endif

#ifdef DEV_NETMAP
	ixl_netmap_attach(vsi);
#endif /* DEV_NETMAP */
	INIT_DEBUGOUT("ixl_attach: end");
	return (0);

err_late:
	if (vsi->ifp != NULL) {
		ether_ifdetach(vsi->ifp);
		if_free(vsi->ifp);
	}
err_mac_hmc:
	i40e_shutdown_lan_hmc(hw);
err_get_cap:
	i40e_shutdown_adminq(hw);
err_out:
	ixl_free_pci_resources(pf);
	ixl_free_vsi(vsi);
	IXL_PF_LOCK_DESTROY(pf);
	return (error);
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
ixl_detach(device_t dev)
{
	struct ixl_pf		*pf = device_get_softc(dev);
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	enum i40e_status_code	status;
#ifdef PCI_IOV
	int			error;
#endif

	INIT_DEBUGOUT("ixl_detach: begin");

	/* Make sure VLANS are not using driver */
	if (vsi->ifp->if_vlantrunk != NULL) {
		device_printf(dev, "Vlan in use, detach first\n");
		return (EBUSY);
	}

#ifdef PCI_IOV
	error = pci_iov_detach(dev);
	if (error != 0) {
		device_printf(dev, "SR-IOV in use; detach first.\n");
		return (error);
	}
#endif

	ether_ifdetach(vsi->ifp);
	if (vsi->ifp->if_drv_flags & IFF_DRV_RUNNING)
		ixl_stop(pf);

	ixl_free_queue_tqs(vsi);

	/* Shutdown LAN HMC */
	status = i40e_shutdown_lan_hmc(hw);
	if (status)
		device_printf(dev,
		    "Shutdown LAN HMC failed with code %d\n", status);

	/* Shutdown admin queue */
	ixl_disable_adminq(hw);
	ixl_free_adminq_tq(pf);
	ixl_teardown_adminq_msix(pf);
	status = i40e_shutdown_adminq(hw);
	if (status)
		device_printf(dev,
		    "Shutdown Admin queue failed with code %d\n", status);

	/* Unregister VLAN events */
	if (vsi->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, vsi->vlan_attach);
	if (vsi->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, vsi->vlan_detach);

	callout_drain(&pf->timer);
#ifdef DEV_NETMAP
	netmap_detach(vsi->ifp);
#endif /* DEV_NETMAP */
	ixl_pf_qmgr_destroy(&pf->qmgr);
	ixl_free_pci_resources(pf);
	bus_generic_detach(dev);
	if_free(vsi->ifp);
	ixl_free_vsi(vsi);
	IXL_PF_LOCK_DESTROY(pf);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
ixl_shutdown(device_t dev)
{
	struct ixl_pf *pf = device_get_softc(dev);
	ixl_stop(pf);
	return (0);
}

