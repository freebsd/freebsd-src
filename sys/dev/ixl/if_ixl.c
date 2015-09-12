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

#ifndef IXL_STANDALONE_BUILD
#include "opt_inet.h"
#include "opt_inet6.h"
#include "opt_rss.h"
#endif

#include "ixl.h"
#include "ixl_pf.h"

#ifdef RSS
#include <net/rss_config.h>
#endif

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixl_driver_version[] = "1.4.3";

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
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_A, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_C, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_A, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_C, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T4, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_20G_KR2_A, 0, 0, 0},
#ifdef X722_SUPPORT
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_1G_BASE_T_X722, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_10G_BASE_T_X722, 0, 0, 0},
#endif
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *ixl_strings[] = {
	"Intel(R) Ethernet Connection XL710 Driver"
};


/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixl_probe(device_t);
static int      ixl_attach(device_t);
static int      ixl_detach(device_t);
static int      ixl_shutdown(device_t);
static int	ixl_get_hw_capabilities(struct ixl_pf *);
static void	ixl_cap_txcsum_tso(struct ixl_vsi *, struct ifnet *, int);
static int      ixl_ioctl(struct ifnet *, u_long, caddr_t);
static void	ixl_init(void *);
static void	ixl_init_locked(struct ixl_pf *);
static void     ixl_stop(struct ixl_pf *);
static void     ixl_media_status(struct ifnet *, struct ifmediareq *);
static int      ixl_media_change(struct ifnet *);
static void     ixl_update_link_status(struct ixl_pf *);
static int      ixl_allocate_pci_resources(struct ixl_pf *);
static u16	ixl_get_bus_info(struct i40e_hw *, device_t);
static int	ixl_setup_stations(struct ixl_pf *);
static int	ixl_switch_config(struct ixl_pf *);
static int	ixl_initialize_vsi(struct ixl_vsi *);
static int	ixl_assign_vsi_msix(struct ixl_pf *);
static int	ixl_assign_vsi_legacy(struct ixl_pf *);
static int	ixl_init_msix(struct ixl_pf *);
static void	ixl_configure_msix(struct ixl_pf *);
static void	ixl_configure_itr(struct ixl_pf *);
static void	ixl_configure_legacy(struct ixl_pf *);
static void	ixl_free_pci_resources(struct ixl_pf *);
static void	ixl_local_timer(void *);
static int	ixl_setup_interface(device_t, struct ixl_vsi *);
static void	ixl_link_event(struct ixl_pf *, struct i40e_arq_event_info *);
static void	ixl_config_rss(struct ixl_vsi *);
static void	ixl_set_queue_rx_itr(struct ixl_queue *);
static void	ixl_set_queue_tx_itr(struct ixl_queue *);
static int	ixl_set_advertised_speeds(struct ixl_pf *, int);

static int	ixl_enable_rings(struct ixl_vsi *);
static int	ixl_disable_rings(struct ixl_vsi *);
static void	ixl_enable_intr(struct ixl_vsi *);
static void	ixl_disable_intr(struct ixl_vsi *);
static void	ixl_disable_rings_intr(struct ixl_vsi *);

static void     ixl_enable_adminq(struct i40e_hw *);
static void     ixl_disable_adminq(struct i40e_hw *);
static void     ixl_enable_queue(struct i40e_hw *, int);
static void     ixl_disable_queue(struct i40e_hw *, int);
static void     ixl_enable_legacy(struct i40e_hw *);
static void     ixl_disable_legacy(struct i40e_hw *);

static void     ixl_set_promisc(struct ixl_vsi *);
static void     ixl_add_multi(struct ixl_vsi *);
static void     ixl_del_multi(struct ixl_vsi *);
static void	ixl_register_vlan(void *, struct ifnet *, u16);
static void	ixl_unregister_vlan(void *, struct ifnet *, u16);
static void	ixl_setup_vlan_filters(struct ixl_vsi *);

static void	ixl_init_filters(struct ixl_vsi *);
static void	ixl_reconfigure_filters(struct ixl_vsi *vsi);
static void	ixl_add_filter(struct ixl_vsi *, u8 *, s16 vlan);
static void	ixl_del_filter(struct ixl_vsi *, u8 *, s16 vlan);
static void	ixl_add_hw_filters(struct ixl_vsi *, int, int);
static void	ixl_del_hw_filters(struct ixl_vsi *, int);
static struct ixl_mac_filter *
		ixl_find_filter(struct ixl_vsi *, u8 *, s16);
static void	ixl_add_mc_filter(struct ixl_vsi *, u8 *);
static void	ixl_free_mac_filters(struct ixl_vsi *vsi);


/* Sysctl debug interface */
static int	ixl_debug_info(SYSCTL_HANDLER_ARGS);
static void	ixl_print_debug_info(struct ixl_pf *);

/* The MSI/X Interrupt handlers */
static void	ixl_intr(void *);
static void	ixl_msix_que(void *);
static void	ixl_msix_adminq(void *);
static void	ixl_handle_mdd_event(struct ixl_pf *);

/* Deferred interrupt tasklets */
static void	ixl_do_adminq(void *, int);

/* Sysctl handlers */
static int	ixl_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	ixl_set_advertise(SYSCTL_HANDLER_ARGS);
static int	ixl_current_speed(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_show_fw(SYSCTL_HANDLER_ARGS);

/* Statistics */
static void     ixl_add_hw_stats(struct ixl_pf *);
static void	ixl_add_sysctls_mac_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *, struct i40e_hw_port_stats *);
static void	ixl_add_sysctls_eth_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *,
		    struct i40e_eth_stats *);
static void	ixl_update_stats_counters(struct ixl_pf *);
static void	ixl_update_eth_stats(struct ixl_vsi *);
static void	ixl_update_vsi_stats(struct ixl_vsi *);
static void	ixl_pf_reset_stats(struct ixl_pf *);
static void	ixl_vsi_reset_stats(struct ixl_vsi *);
static void	ixl_stat_update48(struct i40e_hw *, u32, u32, bool,
		    u64 *, u64 *);
static void	ixl_stat_update32(struct i40e_hw *, u32, bool,
		    u64 *, u64 *);

#ifdef IXL_DEBUG_SYSCTL
static int 	ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hw_res_alloc(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_switch_config(SYSCTL_HANDLER_ARGS);
#endif

#ifdef PCI_IOV
static int	ixl_adminq_err_to_errno(enum i40e_admin_queue_err err);

static int	ixl_iov_init(device_t dev, uint16_t num_vfs, const nvlist_t*);
static void	ixl_iov_uninit(device_t dev);
static int	ixl_add_vf(device_t dev, uint16_t vfnum, const nvlist_t*);

static void	ixl_handle_vf_msg(struct ixl_pf *,
		    struct i40e_arq_event_info *);
static void	ixl_handle_vflr(void *arg, int pending);

static void	ixl_reset_vf(struct ixl_pf *pf, struct ixl_vf *vf);
static void	ixl_reinit_vf(struct ixl_pf *pf, struct ixl_vf *vf);
#endif

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
** Global reset mutex
*/
static struct mtx ixl_reset_mtx;

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
static int ixl_ringsz = DEFAULT_RING;
TUNABLE_INT("hw.ixl.ringsz", &ixl_ringsz);
SYSCTL_INT(_hw_ixl, OID_AUTO, ring_size, CTLFLAG_RDTUN,
    &ixl_ringsz, 0, "Descriptor Ring Size");

/* 
** This can be set manually, if left as 0 the
** number of queues will be calculated based
** on cpus and msix vectors available.
*/
int ixl_max_queues = 0;
TUNABLE_INT("hw.ixl.max_queues", &ixl_max_queues);
SYSCTL_INT(_hw_ixl, OID_AUTO, max_queues, CTLFLAG_RDTUN,
    &ixl_max_queues, 0, "Number of Queues");

/*
** Controls for Interrupt Throttling 
**	- true/false for dynamic adjustment
** 	- default values for static ITR
*/
int ixl_dynamic_rx_itr = 0;
TUNABLE_INT("hw.ixl.dynamic_rx_itr", &ixl_dynamic_rx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, dynamic_rx_itr, CTLFLAG_RDTUN,
    &ixl_dynamic_rx_itr, 0, "Dynamic RX Interrupt Rate");

int ixl_dynamic_tx_itr = 0;
TUNABLE_INT("hw.ixl.dynamic_tx_itr", &ixl_dynamic_tx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, dynamic_tx_itr, CTLFLAG_RDTUN,
    &ixl_dynamic_tx_itr, 0, "Dynamic TX Interrupt Rate");

int ixl_rx_itr = IXL_ITR_8K;
TUNABLE_INT("hw.ixl.rx_itr", &ixl_rx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, rx_itr, CTLFLAG_RDTUN,
    &ixl_rx_itr, 0, "RX Interrupt Rate");

int ixl_tx_itr = IXL_ITR_4K;
TUNABLE_INT("hw.ixl.tx_itr", &ixl_tx_itr);
SYSCTL_INT(_hw_ixl, OID_AUTO, tx_itr, CTLFLAG_RDTUN,
    &ixl_tx_itr, 0, "TX Interrupt Rate");

#ifdef IXL_FDIR
static int ixl_enable_fdir = 1;
TUNABLE_INT("hw.ixl.enable_fdir", &ixl_enable_fdir);
/* Rate at which we sample */
int ixl_atr_rate = 20;
TUNABLE_INT("hw.ixl.atr_rate", &ixl_atr_rate);
#endif

#ifdef DEV_NETMAP
#define NETMAP_IXL_MAIN /* only bring in one part of the netmap code */
#include <dev/netmap/if_ixl_netmap.h>
#endif /* DEV_NETMAP */

static char *ixl_fc_string[6] = {
	"None",
	"Rx",
	"Tx",
	"Full",
	"Priority",
	"Default"
};

static MALLOC_DEFINE(M_IXL, "ixl", "ixl driver allocations");

static uint8_t ixl_bcast_addr[ETHER_ADDR_LEN] =
    {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

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
	static bool lock_init = FALSE;

	INIT_DEBUGOUT("ixl_probe: begin");

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
			/* One shot mutex init */
			if (lock_init == FALSE) {
				lock_init = TRUE;
				mtx_init(&ixl_reset_mtx,
				    "ixl_reset",
				    "IXL RESET Lock", MTX_DEF);
			}
			return (BUS_PROBE_DEFAULT);
		}
		ent++;
	}
	return (ENXIO);
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
	struct ixl_vsi *vsi;
	u16		bus;
	int             error = 0;
#ifdef PCI_IOV
	nvlist_t	*pf_schema, *vf_schema;
	int		iov_error;
#endif

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

	/* Core Lock Init*/
	IXL_PF_LOCK_INIT(pf, device_get_nameunit(dev));

	/* Set up the timer callout */
	callout_init_mtx(&pf->timer, &pf->pf_mtx, 0);

	/* Set up sysctls */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_set_flowcntl, "I", "Flow Control");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "advertise_speed", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_set_advertise, "I", "Advertised Speed");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "current_speed", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_current_speed, "A", "Current Port Speed");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_show_fw, "A", "Firmware version");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rx_itr", CTLFLAG_RW,
	    &ixl_rx_itr, IXL_ITR_8K, "RX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dynamic_rx_itr", CTLFLAG_RW,
	    &ixl_dynamic_rx_itr, 0, "Dynamic RX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "tx_itr", CTLFLAG_RW,
	    &ixl_tx_itr, IXL_ITR_4K, "TX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dynamic_tx_itr", CTLFLAG_RW,
	    &ixl_dynamic_tx_itr, 0, "Dynamic TX ITR");

#ifdef IXL_DEBUG_SYSCTL
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug", CTLTYPE_INT|CTLFLAG_RW, pf, 0,
	    ixl_debug_info, "I", "Debug Information");

	/* Debug shared-code message level */
	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug_mask", CTLFLAG_RW,
	    &pf->hw.debug_mask, 0, "Debug Message Level");

	SYSCTL_ADD_UINT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "vc_debug_level", CTLFLAG_RW, &pf->vc_debug_lvl,
	    0, "PF/VF Virtual Channel debug level");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "link_status", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_link_status, "A", "Current Link Status");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "phy_abilities", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_phy_abilities, "A", "PHY Abilities");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "filter_list", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_sw_filter_list, "A", "SW Filter List");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "hw_res_alloc", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_hw_res_alloc, "A", "HW Resource Allocation");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "switch_config", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_switch_config, "A", "HW Switch Configuration");
#endif

	/* Save off the PCI information */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);

	pf->vc_debug_lvl = 1;

	/* Do PCI setup - map BAR0, etc */
	if (ixl_allocate_pci_resources(pf)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Establish a clean starting point */
	i40e_clear_hw(hw);
	error = i40e_pf_reset(hw);
	if (error) {
		device_printf(dev,"PF reset failure %x\n", error);
		error = EIO;
		goto err_out;
	}

	/* Set admin queue parameters */
	hw->aq.num_arq_entries = IXL_AQ_LEN;
	hw->aq.num_asq_entries = IXL_AQ_LEN;
	hw->aq.arq_buf_size = IXL_AQ_BUFSZ;
	hw->aq.asq_buf_size = IXL_AQ_BUFSZ;

	/* Initialize the shared code */
	error = i40e_init_shared_code(hw);
	if (error) {
		device_printf(dev,"Unable to initialize the shared code\n");
		error = EIO;
		goto err_out;
	}

	/* Set up the admin queue */
	error = i40e_init_adminq(hw);
	if (error) {
		device_printf(dev, "The driver for the device stopped "
		    "because the NVM image is newer than expected.\n"
		    "You must install the most recent version of "
		    " the network driver.\n");
		goto err_out;
	}
	device_printf(dev, "%s\n", ixl_fw_version_str(hw));

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
	error = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp, 0, 0);
	if (error) {
		device_printf(dev, "init_lan_hmc failed: %d\n", error);
		goto err_get_cap;
	}

	error = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (error) {
		device_printf(dev, "configure_lan_hmc failed: %d\n", error);
		goto err_mac_hmc;
	}

	/* Disable LLDP from the firmware */
	i40e_aq_stop_lldp(hw, TRUE, NULL);

	i40e_get_mac_addr(hw, hw->mac.addr);
	error = i40e_validate_mac_addr(hw->mac.addr);
	if (error) {
		device_printf(dev, "validate_mac_addr failed: %d\n", error);
		goto err_mac_hmc;
	}
	bcopy(hw->mac.addr, hw->mac.perm_addr, ETHER_ADDR_LEN);
	i40e_get_port_mac_addr(hw, hw->mac.port_addr);

	/* Set up VSI and queues */
	if (ixl_setup_stations(pf) != 0) { 
		device_printf(dev, "setup stations failed!\n");
		error = ENOMEM;
		goto err_mac_hmc;
	}

	/* Initialize mac filter list for VSI */
	SLIST_INIT(&vsi->ftl);

	/* Set up interrupt routing here */
	if (pf->msix > 1)
		error = ixl_assign_vsi_msix(pf);
	else
		error = ixl_assign_vsi_legacy(pf);
	if (error) 
		goto err_late;

	if (((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver < 33)) ||
	    (hw->aq.fw_maj_ver < 4)) {
		i40e_msec_delay(75);
		error = i40e_aq_set_link_restart_an(hw, TRUE, NULL);
		if (error)
			device_printf(dev, "link restart failed, aq_err=%d\n",
			    pf->hw.aq.asq_last_status);
	}

	/* Determine link state */
	i40e_aq_get_link_info(hw, TRUE, NULL, NULL);
	i40e_get_link_status(hw, &pf->link_up);

	/* Setup OS specific network interface */
	if (ixl_setup_interface(dev, vsi) != 0) {
		device_printf(dev, "interface setup failed!\n");
		error = EIO;
		goto err_late;
	}

	error = ixl_switch_config(pf);
	if (error) {
		device_printf(dev, "Initial switch config failed: %d\n", error);
		goto err_mac_hmc;
	}

	/* Limit phy interrupts to link and modules failure */
	error = i40e_aq_set_phy_int_mask(hw,
	    I40E_AQ_EVENT_LINK_UPDOWN | I40E_AQ_EVENT_MODULE_QUAL_FAIL, NULL);
        if (error)
		device_printf(dev, "set phy mask failed: %d\n", error);

	/* Get the bus configuration and set the shared code */
	bus = ixl_get_bus_info(hw, dev);
	i40e_set_pci_config_data(hw, bus);

	/* Initialize statistics */
	ixl_pf_reset_stats(pf);
	ixl_update_stats_counters(pf);
	ixl_add_hw_stats(pf);

	/* Register for VLAN events */
	vsi->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixl_register_vlan, vsi, EVENTHANDLER_PRI_FIRST);
	vsi->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixl_unregister_vlan, vsi, EVENTHANDLER_PRI_FIRST);

#ifdef PCI_IOV
	/* SR-IOV is only supported when MSI-X is in use. */
	if (pf->msix > 1) {
		pf_schema = pci_iov_schema_alloc_node();
		vf_schema = pci_iov_schema_alloc_node();
		pci_iov_schema_add_unicast_mac(vf_schema, "mac-addr", 0, NULL);
		pci_iov_schema_add_bool(vf_schema, "mac-anti-spoof",
		    IOV_SCHEMA_HASDEFAULT, TRUE);
		pci_iov_schema_add_bool(vf_schema, "allow-set-mac",
		    IOV_SCHEMA_HASDEFAULT, FALSE);
		pci_iov_schema_add_bool(vf_schema, "allow-promisc",
		    IOV_SCHEMA_HASDEFAULT, FALSE);

		iov_error = pci_iov_attach(dev, pf_schema, vf_schema);
		if (iov_error != 0)
			device_printf(dev,
			    "Failed to initialize SR-IOV (error=%d)\n",
			    iov_error);
	}
#endif

#ifdef DEV_NETMAP
	ixl_netmap_attach(vsi);
#endif /* DEV_NETMAP */
	INIT_DEBUGOUT("ixl_attach: end");
	return (0);

err_late:
	if (vsi->ifp != NULL)
		if_free(vsi->ifp);
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
	struct ixl_queue	*que = vsi->queues;
	i40e_status		status;
#ifdef PCI_IOV
	int			error;
#endif

	INIT_DEBUGOUT("ixl_detach: begin");

	/* Make sure VLANS are not using driver */
	if (vsi->ifp->if_vlantrunk != NULL) {
		device_printf(dev,"Vlan in use, detach first\n");
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
	if (vsi->ifp->if_drv_flags & IFF_DRV_RUNNING) {
		IXL_PF_LOCK(pf);
		ixl_stop(pf);
		IXL_PF_UNLOCK(pf);
	}

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		if (que->tq) {
			taskqueue_drain(que->tq, &que->task);
			taskqueue_drain(que->tq, &que->tx_task);
			taskqueue_free(que->tq);
		}
	}

	/* Shutdown LAN HMC */
	status = i40e_shutdown_lan_hmc(hw);
	if (status)
		device_printf(dev,
		    "Shutdown LAN HMC failed with code %d\n", status);

	/* Shutdown admin queue */
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
	IXL_PF_LOCK(pf);
	ixl_stop(pf);
	IXL_PF_UNLOCK(pf);
	return (0);
}


/*********************************************************************
 *
 *  Get the hardware capabilities
 *
 **********************************************************************/

static int
ixl_get_hw_capabilities(struct ixl_pf *pf)
{
	struct i40e_aqc_list_capabilities_element_resp *buf;
	struct i40e_hw	*hw = &pf->hw;
	device_t 	dev = pf->dev;
	int             error, len;
	u16		needed;
	bool		again = TRUE;

	len = 40 * sizeof(struct i40e_aqc_list_capabilities_element_resp);
retry:
	if (!(buf = (struct i40e_aqc_list_capabilities_element_resp *)
	    malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate cap memory\n");
                return (ENOMEM);
	}

	/* This populates the hw struct */
        error = i40e_aq_discover_capabilities(hw, buf, len,
	    &needed, i40e_aqc_opc_list_func_capabilities, NULL);
	free(buf, M_DEVBUF);
	if ((pf->hw.aq.asq_last_status == I40E_AQ_RC_ENOMEM) &&
	    (again == TRUE)) {
		/* retry once with a larger buffer */
		again = FALSE;
		len = needed;
		goto retry;
	} else if (pf->hw.aq.asq_last_status != I40E_AQ_RC_OK) {
		device_printf(dev, "capability discovery failed: %d\n",
		    pf->hw.aq.asq_last_status);
		return (ENODEV);
	}

	/* Capture this PF's starting queue pair */
	pf->qbase = hw->func_caps.base_queue;

#ifdef IXL_DEBUG
	device_printf(dev,"pf_id=%d, num_vfs=%d, msix_pf=%d, "
	    "msix_vf=%d, fd_g=%d, fd_b=%d, tx_qp=%d rx_qp=%d qbase=%d\n",
	    hw->pf_id, hw->func_caps.num_vfs,
	    hw->func_caps.num_msix_vectors,
	    hw->func_caps.num_msix_vectors_vf,
	    hw->func_caps.fd_filters_guaranteed,
	    hw->func_caps.fd_filters_best_effort,
	    hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp,
	    hw->func_caps.base_queue);
#endif
	return (error);
}

static void
ixl_cap_txcsum_tso(struct ixl_vsi *vsi, struct ifnet *ifp, int mask)
{
	device_t 	dev = vsi->dev;

	/* Enable/disable TXCSUM/TSO4 */
	if (!(ifp->if_capenable & IFCAP_TXCSUM)
	    && !(ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable |= IFCAP_TXCSUM;
			/* enable TXCSUM, restore TSO if previously enabled */
			if (vsi->flags & IXL_FLAGS_KEEP_TSO4) {
				vsi->flags &= ~IXL_FLAGS_KEEP_TSO4;
				ifp->if_capenable |= IFCAP_TSO4;
			}
		}
		else if (mask & IFCAP_TSO4) {
			ifp->if_capenable |= (IFCAP_TXCSUM | IFCAP_TSO4);
			vsi->flags &= ~IXL_FLAGS_KEEP_TSO4;
			device_printf(dev,
			    "TSO4 requires txcsum, enabling both...\n");
		}
	} else if((ifp->if_capenable & IFCAP_TXCSUM)
	    && !(ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM)
			ifp->if_capenable &= ~IFCAP_TXCSUM;
		else if (mask & IFCAP_TSO4)
			ifp->if_capenable |= IFCAP_TSO4;
	} else if((ifp->if_capenable & IFCAP_TXCSUM)
	    && (ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM) {
			vsi->flags |= IXL_FLAGS_KEEP_TSO4;
			ifp->if_capenable &= ~(IFCAP_TXCSUM | IFCAP_TSO4);
			device_printf(dev, 
			    "TSO4 requires txcsum, disabling both...\n");
		} else if (mask & IFCAP_TSO4)
			ifp->if_capenable &= ~IFCAP_TSO4;
	}

	/* Enable/disable TXCSUM_IPV6/TSO6 */
	if (!(ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && !(ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable |= IFCAP_TXCSUM_IPV6;
			if (vsi->flags & IXL_FLAGS_KEEP_TSO6) {
				vsi->flags &= ~IXL_FLAGS_KEEP_TSO6;
				ifp->if_capenable |= IFCAP_TSO6;
			}
		} else if (mask & IFCAP_TSO6) {
			ifp->if_capenable |= (IFCAP_TXCSUM_IPV6 | IFCAP_TSO6);
			vsi->flags &= ~IXL_FLAGS_KEEP_TSO6;
			device_printf(dev,
			    "TSO6 requires txcsum6, enabling both...\n");
		}
	} else if((ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && !(ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6)
			ifp->if_capenable &= ~IFCAP_TXCSUM_IPV6;
		else if (mask & IFCAP_TSO6)
			ifp->if_capenable |= IFCAP_TSO6;
	} else if ((ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && (ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6) {
			vsi->flags |= IXL_FLAGS_KEEP_TSO6;
			ifp->if_capenable &= ~(IFCAP_TXCSUM_IPV6 | IFCAP_TSO6);
			device_printf(dev,
			    "TSO6 requires txcsum6, disabling both...\n");
		} else if (mask & IFCAP_TSO6)
			ifp->if_capenable &= ~IFCAP_TSO6;
	}
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixl_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixl_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct ixl_pf	*pf = vsi->back;
	struct ifreq	*ifr = (struct ifreq *) data;
#if defined(INET) || defined(INET6)
	struct ifaddr *ifa = (struct ifaddr *)data;
	bool		avoid_reset = FALSE;
#endif
	int             error = 0;

	switch (command) {

        case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			avoid_reset = TRUE;
#endif
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6)
			avoid_reset = TRUE;
#endif
#if defined(INET) || defined(INET6)
		/*
		** Calling init results in link renegotiation,
		** so we avoid doing it when possible.
		*/
		if (avoid_reset) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				ixl_init(pf);
#ifdef INET
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			error = ether_ioctl(ifp, command, data);
		break;
#endif
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXL_MAX_FRAME -
		   ETHER_HDR_LEN - ETHER_CRC_LEN - ETHER_VLAN_ENCAP_LEN) {
			error = EINVAL;
		} else {
			IXL_PF_LOCK(pf);
			ifp->if_mtu = ifr->ifr_mtu;
			vsi->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
			    + ETHER_VLAN_ENCAP_LEN;
			ixl_init_locked(pf);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		IXL_PF_LOCK(pf);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ pf->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					ixl_set_promisc(vsi);
				}
			} else
				ixl_init_locked(pf);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixl_stop(pf);
		pf->if_flags = ifp->if_flags;
		IXL_PF_UNLOCK(pf);
		break;
	case SIOCADDMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCADDMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXL_PF_LOCK(pf);
			ixl_disable_intr(vsi);
			ixl_add_multi(vsi);
			ixl_enable_intr(vsi);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCDELMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXL_PF_LOCK(pf);
			ixl_disable_intr(vsi);
			ixl_del_multi(vsi);
			ixl_enable_intr(vsi);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
#ifdef IFM_ETH_XTYPE
	case SIOCGIFXMEDIA:
#endif
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &vsi->media, command);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");

		ixl_cap_txcsum_tso(vsi, ifp, mask);

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
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXL_PF_LOCK(pf);
			ixl_init_locked(pf);
			IXL_PF_UNLOCK(pf);
		}
		VLAN_CAPABILITIES(ifp);

		break;
	}

	default:
		IOCTL_DEBUGOUT("ioctl: UNKNOWN (0x%X)\n", (int)command);
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}


/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static void
ixl_init_locked(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;
	device_t 	dev = pf->dev;
	struct i40e_filter_control_settings	filter;
	u8		tmpaddr[ETHER_ADDR_LEN];
	int		ret;

	mtx_assert(&pf->pf_mtx, MA_OWNED);
	INIT_DEBUGOUT("ixl_init: begin");
	ixl_stop(pf);

	/* Get the latest mac address... User might use a LAA */
	bcopy(IF_LLADDR(vsi->ifp), tmpaddr,
	      I40E_ETH_LENGTH_OF_ADDRESS);
	if (!cmp_etheraddr(hw->mac.addr, tmpaddr) && 
	    (i40e_validate_mac_addr(tmpaddr) == I40E_SUCCESS)) {
		ixl_del_filter(vsi, hw->mac.addr, IXL_VLAN_ANY);
		bcopy(tmpaddr, hw->mac.addr,
		    I40E_ETH_LENGTH_OF_ADDRESS);
		ret = i40e_aq_mac_address_write(hw,
		    I40E_AQC_WRITE_TYPE_LAA_ONLY,
		    hw->mac.addr, NULL);
		if (ret) {
			device_printf(dev, "LLA address"
			 "change failed!!\n");
			return;
		} else {
			ixl_add_filter(vsi, hw->mac.addr, IXL_VLAN_ANY);
		}
	}

	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= (CSUM_TCP_IPV6 | CSUM_UDP_IPV6);

	/* Set up the device filtering */
	bzero(&filter, sizeof(filter));
	filter.enable_ethtype = TRUE;
	filter.enable_macvlan = TRUE;
#ifdef IXL_FDIR
	filter.enable_fdir = TRUE;
#endif
	if (i40e_set_filter_control(hw, &filter))
		device_printf(dev, "set_filter_control() failed\n");

	/* Set up RSS */
	ixl_config_rss(vsi);

	/*
	** Prepare the VSI: rings, hmc contexts, etc...
	*/
	if (ixl_initialize_vsi(vsi)) {
		device_printf(dev, "initialize vsi failed!!\n");
		return;
	}

	/* Add protocol filters to list */
	ixl_init_filters(vsi);

	/* Setup vlan's if needed */
	ixl_setup_vlan_filters(vsi);

	/* Start the local timer */
	callout_reset(&pf->timer, hz, ixl_local_timer, pf);

	/* Set up MSI/X routing and the ITR settings */
	if (ixl_enable_msix) {
		ixl_configure_msix(pf);
		ixl_configure_itr(pf);
	} else
		ixl_configure_legacy(pf);

	ixl_enable_rings(vsi);

	i40e_aq_set_default_vsi(hw, vsi->seid, NULL);

	ixl_reconfigure_filters(vsi);

	/* Set MTU in hardware*/
	int aq_error = i40e_aq_set_mac_config(hw, vsi->max_frame_size,
	    TRUE, 0, NULL);
	if (aq_error)
		device_printf(vsi->dev,
			"aq_set_mac_config in init error, code %d\n",
		    aq_error);

	/* And now turn on interrupts */
	ixl_enable_intr(vsi);

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

static void
ixl_init(void *arg)
{
	struct ixl_pf *pf = arg;

	IXL_PF_LOCK(pf);
	ixl_init_locked(pf);
	IXL_PF_UNLOCK(pf);
	return;
}

/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/
static void
ixl_handle_que(void *context, int pending)
{
	struct ixl_queue *que = context;
	struct ixl_vsi *vsi = que->vsi;
	struct i40e_hw  *hw = vsi->hw;
	struct tx_ring  *txr = &que->txr;
	struct ifnet    *ifp = vsi->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixl_rxeof(que, IXL_RX_LIMIT);
		IXL_TX_LOCK(txr);
		ixl_txeof(que);
		if (!drbr_empty(ifp, txr->br))
			ixl_mq_start_locked(ifp, txr);
		IXL_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->task);
			return;
		}
	}

	/* Reenable this interrupt - hmmm */
	ixl_enable_queue(hw, que->me);
	return;
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/
void
ixl_intr(void *arg)
{
	struct ixl_pf		*pf = arg;
	struct i40e_hw		*hw =  &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	struct ifnet		*ifp = vsi->ifp;
	struct tx_ring		*txr = &que->txr;
        u32			reg, icr0, mask;
	bool			more_tx, more_rx;

	++que->irqs;

	/* Protect against spurious interrupts */
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	icr0 = rd32(hw, I40E_PFINT_ICR0);

	reg = rd32(hw, I40E_PFINT_DYN_CTL0);
	reg = reg | I40E_PFINT_DYN_CTL0_CLEARPBA_MASK;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

        mask = rd32(hw, I40E_PFINT_ICR0_ENA);

#ifdef PCI_IOV
	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK)
		taskqueue_enqueue(pf->tq, &pf->vflr_task);
#endif

	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		taskqueue_enqueue(pf->tq, &pf->adminq);
		return;
	}

	more_rx = ixl_rxeof(que, IXL_RX_LIMIT);

	IXL_TX_LOCK(txr);
	more_tx = ixl_txeof(que);
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	IXL_TX_UNLOCK(txr);

	/* re-enable other interrupt causes */
	wr32(hw, I40E_PFINT_ICR0_ENA, mask);

	/* And now the queues */
	reg = rd32(hw, I40E_QINT_RQCTL(0));
	reg |= I40E_QINT_RQCTL_CAUSE_ENA_MASK;
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = rd32(hw, I40E_QINT_TQCTL(0));
	reg |= I40E_QINT_TQCTL_CAUSE_ENA_MASK;
	reg &= ~I40E_PFINT_ICR0_INTEVENT_MASK;
	wr32(hw, I40E_QINT_TQCTL(0), reg);

	ixl_enable_legacy(hw);

	return;
}


/*********************************************************************
 *
 *  MSIX VSI Interrupt Service routine
 *
 **********************************************************************/
void
ixl_msix_que(void *arg)
{
	struct ixl_queue	*que = arg;
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	bool		more_tx, more_rx;

	/* Protect against spurious interrupts */
	if (!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	++que->irqs;

	more_rx = ixl_rxeof(que, IXL_RX_LIMIT);

	IXL_TX_LOCK(txr);
	more_tx = ixl_txeof(que);
	/*
	** Make certain that if the stack 
	** has anything queued the task gets
	** scheduled to handle it.
	*/
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	IXL_TX_UNLOCK(txr);

	ixl_set_queue_rx_itr(que);
	ixl_set_queue_tx_itr(que);

	if (more_tx || more_rx)
		taskqueue_enqueue(que->tq, &que->task);
	else
		ixl_enable_queue(hw, que->me);

	return;
}


/*********************************************************************
 *
 *  MSIX Admin Queue Interrupt Service routine
 *
 **********************************************************************/
static void
ixl_msix_adminq(void *arg)
{
	struct ixl_pf	*pf = arg;
	struct i40e_hw	*hw = &pf->hw;
	u32		reg, mask;

	++pf->admin_irq;

	reg = rd32(hw, I40E_PFINT_ICR0);
	mask = rd32(hw, I40E_PFINT_ICR0_ENA);

	/* Check on the cause */
	if (reg & I40E_PFINT_ICR0_ADMINQ_MASK)
		mask &= ~I40E_PFINT_ICR0_ENA_ADMINQ_MASK;

	if (reg & I40E_PFINT_ICR0_MAL_DETECT_MASK) {
		ixl_handle_mdd_event(pf);
		mask &= ~I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	}

#ifdef PCI_IOV
	if (reg & I40E_PFINT_ICR0_VFLR_MASK) {
		mask &= ~I40E_PFINT_ICR0_ENA_VFLR_MASK;
		taskqueue_enqueue(pf->tq, &pf->vflr_task);
	}
#endif

	reg = rd32(hw, I40E_PFINT_DYN_CTL0);
	reg = reg | I40E_PFINT_DYN_CTL0_CLEARPBA_MASK;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

	taskqueue_enqueue(pf->tq, &pf->adminq);
	return;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
static void
ixl_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct ixl_pf	*pf = vsi->back;
	struct i40e_hw  *hw = &pf->hw;

	INIT_DEBUGOUT("ixl_media_status: begin");
	IXL_PF_LOCK(pf);

	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);
	ixl_update_link_status(pf);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!pf->link_up) {
		IXL_PF_UNLOCK(pf);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	/* Hardware is always full-duplex */
	ifmr->ifm_active |= IFM_FDX;

	switch (hw->phy.link_info.phy_type) {
		/* 100 M */
		case I40E_PHY_TYPE_100BASE_TX:
			ifmr->ifm_active |= IFM_100_TX;
			break;
		/* 1 G */
		case I40E_PHY_TYPE_1000BASE_T:
			ifmr->ifm_active |= IFM_1000_T;
			break;
		case I40E_PHY_TYPE_1000BASE_SX:
			ifmr->ifm_active |= IFM_1000_SX;
			break;
		case I40E_PHY_TYPE_1000BASE_LX:
			ifmr->ifm_active |= IFM_1000_LX;
			break;
		/* 10 G */
		case I40E_PHY_TYPE_10GBASE_SFPP_CU:
			ifmr->ifm_active |= IFM_10G_TWINAX;
			break;
		case I40E_PHY_TYPE_10GBASE_SR:
			ifmr->ifm_active |= IFM_10G_SR;
			break;
		case I40E_PHY_TYPE_10GBASE_LR:
			ifmr->ifm_active |= IFM_10G_LR;
			break;
		case I40E_PHY_TYPE_10GBASE_T:
			ifmr->ifm_active |= IFM_10G_T;
			break;
		/* 40 G */
		case I40E_PHY_TYPE_40GBASE_CR4:
		case I40E_PHY_TYPE_40GBASE_CR4_CU:
			ifmr->ifm_active |= IFM_40G_CR4;
			break;
		case I40E_PHY_TYPE_40GBASE_SR4:
			ifmr->ifm_active |= IFM_40G_SR4;
			break;
		case I40E_PHY_TYPE_40GBASE_LR4:
			ifmr->ifm_active |= IFM_40G_LR4;
			break;
#ifndef IFM_ETH_XTYPE
		case I40E_PHY_TYPE_1000BASE_KX:
			ifmr->ifm_active |= IFM_1000_CX;
			break;
		case I40E_PHY_TYPE_10GBASE_CR1_CU:
		case I40E_PHY_TYPE_10GBASE_CR1:
			ifmr->ifm_active |= IFM_10G_TWINAX;
			break;
		case I40E_PHY_TYPE_10GBASE_KX4:
			ifmr->ifm_active |= IFM_10G_CX4;
			break;
		case I40E_PHY_TYPE_10GBASE_KR:
			ifmr->ifm_active |= IFM_10G_SR;
			break;
		case I40E_PHY_TYPE_40GBASE_KR4:
		case I40E_PHY_TYPE_XLPPI:
			ifmr->ifm_active |= IFM_40G_SR4;
			break;
#else
		case I40E_PHY_TYPE_1000BASE_KX:
			ifmr->ifm_active |= IFM_1000_KX;
			break;
		/* ERJ: What's the difference between these? */
		case I40E_PHY_TYPE_10GBASE_CR1_CU:
		case I40E_PHY_TYPE_10GBASE_CR1:
			ifmr->ifm_active |= IFM_10G_CR1;
			break;
		case I40E_PHY_TYPE_10GBASE_KX4:
			ifmr->ifm_active |= IFM_10G_KX4;
			break;
		case I40E_PHY_TYPE_10GBASE_KR:
			ifmr->ifm_active |= IFM_10G_KR;
			break;
		case I40E_PHY_TYPE_20GBASE_KR2:
			ifmr->ifm_active |= IFM_20G_KR2;
			break;
		case I40E_PHY_TYPE_40GBASE_KR4:
			ifmr->ifm_active |= IFM_40G_KR4;
			break;
		case I40E_PHY_TYPE_XLPPI:
			ifmr->ifm_active |= IFM_40G_XLPPI;
			break;
#endif
		default:
			ifmr->ifm_active |= IFM_UNKNOWN;
			break;
	}
	/* Report flow control status as well */
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_TX)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_RX)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;

	IXL_PF_UNLOCK(pf);

	return;
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
static int
ixl_media_change(struct ifnet * ifp)
{
	struct ixl_vsi *vsi = ifp->if_softc;
	struct ifmedia *ifm = &vsi->media;

	INIT_DEBUGOUT("ixl_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if_printf(ifp, "Media change is currently not supported.\n");

	return (ENODEV);
}


#ifdef IXL_FDIR
/*
** ATR: Application Targetted Receive - creates a filter
**	based on TX flow info that will keep the receive
**	portion of the flow on the same queue. Based on the
**	implementation this is only available for TCP connections
*/
void
ixl_atr(struct ixl_queue *que, struct tcphdr *th, int etype)
{
	struct ixl_vsi			*vsi = que->vsi;
	struct tx_ring			*txr = &que->txr;
	struct i40e_filter_program_desc	*FDIR;
	u32				ptype, dtype;
	int				idx;

	/* check if ATR is enabled and sample rate */
	if ((!ixl_enable_fdir) || (!txr->atr_rate))
		return;
	/*
	** We sample all TCP SYN/FIN packets,
	** or at the selected sample rate 
	*/
	txr->atr_count++;
	if (((th->th_flags & (TH_FIN | TH_SYN)) == 0) &&
	    (txr->atr_count < txr->atr_rate))
                return;
	txr->atr_count = 0;

	/* Get a descriptor to use */
	idx = txr->next_avail;
	FDIR = (struct i40e_filter_program_desc *) &txr->base[idx];
	if (++idx == que->num_desc)
		idx = 0;
	txr->avail--;
	txr->next_avail = idx;

	ptype = (que->me << I40E_TXD_FLTR_QW0_QINDEX_SHIFT) &
	    I40E_TXD_FLTR_QW0_QINDEX_MASK;

	ptype |= (etype == ETHERTYPE_IP) ?
	    (I40E_FILTER_PCTYPE_NONF_IPV4_TCP <<
	    I40E_TXD_FLTR_QW0_PCTYPE_SHIFT) :
	    (I40E_FILTER_PCTYPE_NONF_IPV6_TCP <<
	    I40E_TXD_FLTR_QW0_PCTYPE_SHIFT);

	ptype |= vsi->id << I40E_TXD_FLTR_QW0_DEST_VSI_SHIFT;

	dtype = I40E_TX_DESC_DTYPE_FILTER_PROG;

	/*
	** We use the TCP TH_FIN as a trigger to remove
	** the filter, otherwise its an update.
	*/
	dtype |= (th->th_flags & TH_FIN) ?
	    (I40E_FILTER_PROGRAM_DESC_PCMD_REMOVE <<
	    I40E_TXD_FLTR_QW1_PCMD_SHIFT) :
	    (I40E_FILTER_PROGRAM_DESC_PCMD_ADD_UPDATE <<
	    I40E_TXD_FLTR_QW1_PCMD_SHIFT);

	dtype |= I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_QINDEX <<
	    I40E_TXD_FLTR_QW1_DEST_SHIFT;

	dtype |= I40E_FILTER_PROGRAM_DESC_FD_STATUS_FD_ID <<
	    I40E_TXD_FLTR_QW1_FD_STATUS_SHIFT;

	FDIR->qindex_flex_ptype_vsi = htole32(ptype);
	FDIR->dtype_cmd_cntindex = htole32(dtype);
	return;
}
#endif


static void
ixl_set_promisc(struct ixl_vsi *vsi)
{
	struct ifnet	*ifp = vsi->ifp;
	struct i40e_hw	*hw = vsi->hw;
	int		err, mcnt = 0;
	bool		uni = FALSE, multi = FALSE;

	if (ifp->if_flags & IFF_ALLMULTI)
                multi = TRUE;
	else { /* Need to count the multicast addresses */
		struct  ifmultiaddr *ifma;
		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
                        if (ifma->ifma_addr->sa_family != AF_LINK)
                                continue;
                        if (mcnt == MAX_MULTICAST_ADDR)
                                break;
                        mcnt++;
		}
		if_maddr_runlock(ifp);
	}

	if (mcnt >= MAX_MULTICAST_ADDR)
                multi = TRUE;
        if (ifp->if_flags & IFF_PROMISC)
		uni = TRUE;

	err = i40e_aq_set_vsi_unicast_promiscuous(hw,
	    vsi->seid, uni, NULL);
	err = i40e_aq_set_vsi_multicast_promiscuous(hw,
	    vsi->seid, multi, NULL);
	return;
}

/*********************************************************************
 * 	Filter Routines
 *
 *	Routines for multicast and vlan filter management.
 *
 *********************************************************************/
static void
ixl_add_multi(struct ixl_vsi *vsi)
{
	struct	ifmultiaddr	*ifma;
	struct ifnet		*ifp = vsi->ifp;
	struct i40e_hw		*hw = vsi->hw;
	int			mcnt = 0, flags;

	IOCTL_DEBUGOUT("ixl_add_multi: begin");

	if_maddr_rlock(ifp);
	/*
	** First just get a count, to decide if we
	** we simply use multicast promiscuous.
	*/
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mcnt++;
	}
	if_maddr_runlock(ifp);

	if (__predict_false(mcnt >= MAX_MULTICAST_ADDR)) {
		/* delete existing MC filters */
		ixl_del_hw_filters(vsi, mcnt);
		i40e_aq_set_vsi_multicast_promiscuous(hw,
		    vsi->seid, TRUE, NULL);
		return;
	}

	mcnt = 0;
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		ixl_add_mc_filter(vsi,
		    (u8*)LLADDR((struct sockaddr_dl *) ifma->ifma_addr));
		mcnt++;
	}
	if_maddr_runlock(ifp);
	if (mcnt > 0) {
		flags = (IXL_FILTER_ADD | IXL_FILTER_USED | IXL_FILTER_MC);
		ixl_add_hw_filters(vsi, flags, mcnt);
	}

	IOCTL_DEBUGOUT("ixl_add_multi: end");
	return;
}

static void
ixl_del_multi(struct ixl_vsi *vsi)
{
	struct ifnet		*ifp = vsi->ifp;
	struct ifmultiaddr	*ifma;
	struct ixl_mac_filter	*f;
	int			mcnt = 0;
	bool		match = FALSE;

	IOCTL_DEBUGOUT("ixl_del_multi: begin");

	/* Search for removed multicast addresses */
	if_maddr_rlock(ifp);
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if ((f->flags & IXL_FILTER_USED) && (f->flags & IXL_FILTER_MC)) {
			match = FALSE;
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				u8 *mc_addr = (u8 *)LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
				if (cmp_etheraddr(f->macaddr, mc_addr)) {
					match = TRUE;
					break;
				}
			}
			if (match == FALSE) {
				f->flags |= IXL_FILTER_DEL;
				mcnt++;
			}
		}
	}
	if_maddr_runlock(ifp);

	if (mcnt > 0)
		ixl_del_hw_filters(vsi, mcnt);
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
ixl_local_timer(void *arg)
{
	struct ixl_pf		*pf = arg;
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = pf->dev;
	int			hung = 0;
	u32			mask;

	mtx_assert(&pf->pf_mtx, MA_OWNED);

	/* Fire off the adminq task */
	taskqueue_enqueue(pf->tq, &pf->adminq);

	/* Update stats */
	ixl_update_stats_counters(pf);

	/*
	** Check status of the queues
	*/
	mask = (I40E_PFINT_DYN_CTLN_INTENA_MASK |
		I40E_PFINT_DYN_CTLN_SWINT_TRIG_MASK);
 
	for (int i = 0; i < vsi->num_queues; i++,que++) {
		/* Any queues with outstanding work get a sw irq */
		if (que->busy)
			wr32(hw, I40E_PFINT_DYN_CTLN(que->me), mask);
		/*
		** Each time txeof runs without cleaning, but there
		** are uncleaned descriptors it increments busy. If
		** we get to 5 we declare it hung.
		*/
		if (que->busy == IXL_QUEUE_HUNG) {
			++hung;
			/* Mark the queue as inactive */
			vsi->active_queues &= ~((u64)1 << que->me);
			continue;
		} else {
			/* Check if we've come back from hung */
			if ((vsi->active_queues & ((u64)1 << que->me)) == 0)
				vsi->active_queues |= ((u64)1 << que->me);
		}
		if (que->busy >= IXL_MAX_TX_BUSY) {
#ifdef IXL_DEBUG
			device_printf(dev,"Warning queue %d "
			    "appears to be hung!\n", i);
#endif
			que->busy = IXL_QUEUE_HUNG;
			++hung;
		}
	}
	/* Only reinit if all queues show hung */
	if (hung == vsi->num_queues)
		goto hung;

	callout_reset(&pf->timer, hz, ixl_local_timer, pf);
	return;

hung:
	device_printf(dev, "Local Timer: HANG DETECT - Resetting!!\n");
	ixl_init_locked(pf);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
ixl_update_link_status(struct ixl_pf *pf)
{
	struct ixl_vsi		*vsi = &pf->vsi;
	struct i40e_hw		*hw = &pf->hw;
	struct ifnet		*ifp = vsi->ifp;
	device_t		dev = pf->dev;

	if (pf->link_up){ 
		if (vsi->link_active == FALSE) {
			pf->fc = hw->fc.current_mode;
			if (bootverbose) {
				device_printf(dev,"Link is up %d Gbps %s,"
				    " Flow Control: %s\n",
				    ((pf->link_speed ==
				    I40E_LINK_SPEED_40GB)? 40:10),
				    "Full Duplex", ixl_fc_string[pf->fc]);
			}
			vsi->link_active = TRUE;
			/*
			** Warn user if link speed on NPAR enabled
			** partition is not at least 10GB
			*/
			if (hw->func_caps.npar_enable &&
			   (hw->phy.link_info.link_speed ==
			   I40E_LINK_SPEED_1GB ||
			   hw->phy.link_info.link_speed ==
			   I40E_LINK_SPEED_100MB))
				device_printf(dev, "The partition detected"
				    "link speed that is less than 10Gbps\n");
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (vsi->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			vsi->link_active = FALSE;
		}
	}

	return;
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

static void
ixl_stop(struct ixl_pf *pf)
{
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;

	mtx_assert(&pf->pf_mtx, MA_OWNED);

	INIT_DEBUGOUT("ixl_stop: begin\n");
	if (pf->num_vfs == 0)
		ixl_disable_intr(vsi);
	else
		ixl_disable_rings_intr(vsi);
	ixl_disable_rings(vsi);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* Stop the local timer */
	callout_stop(&pf->timer);

	return;
}


/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers for the VSI
 *
 **********************************************************************/
static int
ixl_assign_vsi_legacy(struct ixl_pf *pf)
{
	device_t        dev = pf->dev;
	struct 		ixl_vsi *vsi = &pf->vsi;
	struct		ixl_queue *que = vsi->queues;
	int 		error, rid = 0;

	if (pf->msix == 1)
		rid = 1;
	pf->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &rid, RF_SHAREABLE | RF_ACTIVE);
	if (pf->res == NULL) {
		device_printf(dev,"Unable to allocate"
		    " bus resource: vsi legacy/msi interrupt\n");
		return (ENXIO);
	}

	/* Set the handler function */
	error = bus_setup_intr(dev, pf->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixl_intr, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "Failed to register legacy/msi handler");
		return (error);
	}
	bus_describe_intr(dev, pf->res, pf->tag, "irq0");
	TASK_INIT(&que->tx_task, 0, ixl_deferred_mq_start, que);
	TASK_INIT(&que->task, 0, ixl_handle_que, que);
	que->tq = taskqueue_create_fast("ixl_que", M_NOWAIT,
	    taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
	    device_get_nameunit(dev));
	TASK_INIT(&pf->adminq, 0, ixl_do_adminq, pf);

#ifdef PCI_IOV
	TASK_INIT(&pf->vflr_task, 0, ixl_handle_vflr, pf);
#endif

	pf->tq = taskqueue_create_fast("ixl_adm", M_NOWAIT,
	    taskqueue_thread_enqueue, &pf->tq);
	taskqueue_start_threads(&pf->tq, 1, PI_NET, "%s adminq",
	    device_get_nameunit(dev));

	return (0);
}


/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers for the VSI
 *
 **********************************************************************/
static int
ixl_assign_vsi_msix(struct ixl_pf *pf)
{
	device_t	dev = pf->dev;
	struct 		ixl_vsi *vsi = &pf->vsi;
	struct 		ixl_queue *que = vsi->queues;
	struct		tx_ring	 *txr;
	int 		error, rid, vector = 0;
#ifdef	RSS
	cpuset_t cpu_mask;
#endif

	/* Admin Que is vector 0*/
	rid = vector + 1;
	pf->res = bus_alloc_resource_any(dev,
    	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (!pf->res) {
		device_printf(dev,"Unable to allocate"
    	    " bus resource: Adminq interrupt [%d]\n", rid);
		return (ENXIO);
	}
	/* Set the adminq vector and handler */
	error = bus_setup_intr(dev, pf->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixl_msix_adminq, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "Failed to register Admin que handler");
		return (error);
	}
	bus_describe_intr(dev, pf->res, pf->tag, "aq");
	pf->admvec = vector;
	/* Tasklet for Admin Queue */
	TASK_INIT(&pf->adminq, 0, ixl_do_adminq, pf);

#ifdef PCI_IOV
	TASK_INIT(&pf->vflr_task, 0, ixl_handle_vflr, pf);
#endif

	pf->tq = taskqueue_create_fast("ixl_adm", M_NOWAIT,
	    taskqueue_thread_enqueue, &pf->tq);
	taskqueue_start_threads(&pf->tq, 1, PI_NET, "%s adminq",
	    device_get_nameunit(pf->dev));
	++vector;

	/* Now set up the stations */
	for (int i = 0; i < vsi->num_queues; i++, vector++, que++) {
		int cpu_id = i;
		rid = vector + 1;
		txr = &que->txr;
		que->res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (que->res == NULL) {
			device_printf(dev,"Unable to allocate"
		    	    " bus resource: que interrupt [%d]\n", vector);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, que->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL,
		    ixl_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register que handler");
			return (error);
		}
		bus_describe_intr(dev, que->res, que->tag, "q%d", i);
		/* Bind the vector to a CPU */
#ifdef RSS
		cpu_id = rss_getcpu(i % rss_getnumbuckets());
#endif
		bus_bind_intr(dev, que->res, cpu_id);
		que->msix = vector;
		TASK_INIT(&que->tx_task, 0, ixl_deferred_mq_start, que);
		TASK_INIT(&que->task, 0, ixl_handle_que, que);
		que->tq = taskqueue_create_fast("ixl_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
#ifdef RSS
		CPU_SETOF(cpu_id, &cpu_mask);
		taskqueue_start_threads_cpuset(&que->tq, 1, PI_NET,
		    &cpu_mask, "%s (bucket %d)",
		    device_get_nameunit(dev), cpu_id);
#else
		taskqueue_start_threads(&que->tq, 1, PI_NET,
		    "%s que", device_get_nameunit(dev));
#endif
	}

	return (0);
}


/*
 * Allocate MSI/X vectors
 */
static int
ixl_init_msix(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	int rid, want, vectors, queues, available;

	/* Override by tuneable */
	if (ixl_enable_msix == 0)
		goto msi;

	/*
	** When used in a virtualized environment 
	** PCI BUSMASTER capability may not be set
	** so explicity set it here and rewrite
	** the ENABLE in the MSIX control register
	** at this point to cause the host to
	** successfully initialize us.
	*/
	{
		u16 pci_cmd_word;
		int msix_ctrl;
		pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
		pci_cmd_word |= PCIM_CMD_BUSMASTEREN;
		pci_write_config(dev, PCIR_COMMAND, pci_cmd_word, 2);
		pci_find_cap(dev, PCIY_MSIX, &rid);
		rid += PCIR_MSIX_CTRL;
		msix_ctrl = pci_read_config(dev, rid, 2);
		msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
		pci_write_config(dev, rid, msix_ctrl, 2);
	}

	/* First try MSI/X */
	rid = PCIR_BAR(IXL_BAR);
	pf->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (!pf->msix_mem) {
		/* May not be enabled */
		device_printf(pf->dev,
		    "Unable to map MSIX table \n");
		goto msi;
	}

	available = pci_msix_count(dev); 
	if (available == 0) { /* system has msix disabled */
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, pf->msix_mem);
		pf->msix_mem = NULL;
		goto msi;
	}

	/* Figure out a reasonable auto config value */
	queues = (mp_ncpus > (available - 1)) ? (available - 1) : mp_ncpus;

	/* Override with hardcoded value if sane */
	if ((ixl_max_queues != 0) && (ixl_max_queues <= queues)) 
		queues = ixl_max_queues;

#ifdef  RSS
	/* If we're doing RSS, clamp at the number of RSS buckets */
	if (queues > rss_getnumbuckets())
		queues = rss_getnumbuckets();
#endif

	/*
	** Want one vector (RX/TX pair) per queue
	** plus an additional for the admin queue.
	*/
	want = queues + 1;
	if (want <= available)	/* Have enough */
		vectors = want;
	else {
               	device_printf(pf->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors available but %d wanted!\n",
		    available, want);
		return (0); /* Will go to Legacy setup */
	}

	if (pci_alloc_msix(dev, &vectors) == 0) {
               	device_printf(pf->dev,
		    "Using MSIX interrupts with %d vectors\n", vectors);
		pf->msix = vectors;
		pf->vsi.num_queues = queues;
#ifdef RSS
		/*
		 * If we're doing RSS, the number of queues needs to
		 * match the number of RSS buckets that are configured.
		 *
		 * + If there's more queues than RSS buckets, we'll end
		 *   up with queues that get no traffic.
		 *
		 * + If there's more RSS buckets than queues, we'll end
		 *   up having multiple RSS buckets map to the same queue,
		 *   so there'll be some contention.
		 */
		if (queues != rss_getnumbuckets()) {
			device_printf(dev,
			    "%s: queues (%d) != RSS buckets (%d)"
			    "; performance will be impacted.\n",
			    __func__, queues, rss_getnumbuckets());
		}
#endif
		return (vectors);
	}
msi:
       	vectors = pci_msi_count(dev);
	pf->vsi.num_queues = 1;
	pf->msix = 1;
	ixl_max_queues = 1;
	ixl_enable_msix = 0;
       	if (vectors == 1 && pci_alloc_msi(dev, &vectors) == 0)
               	device_printf(pf->dev,"Using an MSI interrupt\n");
	else {
		pf->msix = 0;
               	device_printf(pf->dev,"Using a Legacy interrupt\n");
	}
	return (vectors);
}


/*
 * Plumb MSI/X vectors
 */
static void
ixl_configure_msix(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	u32		reg;
	u16		vector = 1;

	/* First set up the adminq - vector 0 */
	wr32(hw, I40E_PFINT_ICR0_ENA, 0);  /* disable all */
	rd32(hw, I40E_PFINT_ICR0);         /* read to clear */

	reg = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK |
	    I40E_PFINT_ICR0_ENA_GRST_MASK |
	    I40E_PFINT_ICR0_HMC_ERR_MASK |
	    I40E_PFINT_ICR0_ENA_ADMINQ_MASK |
	    I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK |
	    I40E_PFINT_ICR0_ENA_VFLR_MASK |
	    I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	wr32(hw, I40E_PFINT_LNKLST0, 0x7FF);
	wr32(hw, I40E_PFINT_ITR0(IXL_RX_ITR), 0x003E);

	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTL0_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTL0_INTENA_MSK_MASK);

	wr32(hw, I40E_PFINT_STAT_CTL0, 0);

	/* Next configure the queues */
	for (int i = 0; i < vsi->num_queues; i++, vector++) {
		wr32(hw, I40E_PFINT_DYN_CTLN(i), i);
		wr32(hw, I40E_PFINT_LNKLSTN(i), i);

		reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK |
		(IXL_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
		(i << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_TX << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT);
		wr32(hw, I40E_QINT_RQCTL(i), reg);

		reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
		(IXL_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
		((i+1) << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_RX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
		if (i == (vsi->num_queues - 1))
			reg |= (IXL_QUEUE_EOL
			    << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
		wr32(hw, I40E_QINT_TQCTL(i), reg);
	}
}

/*
 * Configure for MSI single vector operation 
 */
static void
ixl_configure_legacy(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	u32		reg;


	wr32(hw, I40E_PFINT_ITR0(0), 0);
	wr32(hw, I40E_PFINT_ITR0(1), 0);


	/* Setup "other" causes */
	reg = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK
	    | I40E_PFINT_ICR0_ENA_GRST_MASK
	    | I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK
	    | I40E_PFINT_ICR0_ENA_GPIO_MASK
	    | I40E_PFINT_ICR0_ENA_LINK_STAT_CHANGE_MASK
	    | I40E_PFINT_ICR0_ENA_HMC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK
	    | I40E_PFINT_ICR0_ENA_VFLR_MASK
	    | I40E_PFINT_ICR0_ENA_ADMINQ_MASK
	    ;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	/* SW_ITR_IDX = 0, but don't change INTENA */
	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTLN_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTLN_INTENA_MSK_MASK);
	/* SW_ITR_IDX = 0, OTHER_ITR_IDX = 0 */
	wr32(hw, I40E_PFINT_STAT_CTL0, 0);

	/* FIRSTQ_INDX = 0, FIRSTQ_TYPE = 0 (rx) */
	wr32(hw, I40E_PFINT_LNKLST0, 0);

	/* Associate the queue pair to the vector and enable the q int */
	reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK
	    | (IXL_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT)
	    | (I40E_QUEUE_TYPE_TX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK
	    | (IXL_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT)
	    | (IXL_QUEUE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
	wr32(hw, I40E_QINT_TQCTL(0), reg);

	/* Next enable the queue pair */
	reg = rd32(hw, I40E_QTX_ENA(0));
	reg |= I40E_QTX_ENA_QENA_REQ_MASK;
	wr32(hw, I40E_QTX_ENA(0), reg);

	reg = rd32(hw, I40E_QRX_ENA(0));
	reg |= I40E_QRX_ENA_QENA_REQ_MASK;
	wr32(hw, I40E_QRX_ENA(0), reg);
}


/*
 * Set the Initial ITR state
 */
static void
ixl_configure_itr(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;

	vsi->rx_itr_setting = ixl_rx_itr;
	if (ixl_dynamic_rx_itr)
		vsi->rx_itr_setting |= IXL_ITR_DYNAMIC;
	vsi->tx_itr_setting = ixl_tx_itr;
	if (ixl_dynamic_tx_itr)
		vsi->tx_itr_setting |= IXL_ITR_DYNAMIC;
	
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring	*txr = &que->txr;
		struct rx_ring 	*rxr = &que->rxr;

		wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = IXL_AVE_LATENCY;
		wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = IXL_AVE_LATENCY;
	}
}


static int
ixl_allocate_pci_resources(struct ixl_pf *pf)
{
	int             rid;
	device_t        dev = pf->dev;

	rid = PCIR_BAR(0);
	pf->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(pf->pci_mem)) {
		device_printf(dev,"Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	pf->osdep.mem_bus_space_tag =
		rman_get_bustag(pf->pci_mem);
	pf->osdep.mem_bus_space_handle =
		rman_get_bushandle(pf->pci_mem);
	pf->osdep.mem_bus_space_size = rman_get_size(pf->pci_mem);
	pf->osdep.flush_reg = I40E_GLGEN_STAT;
	pf->hw.hw_addr = (u8 *) &pf->osdep.mem_bus_space_handle;

	pf->hw.back = &pf->osdep;

	/*
	** Now setup MSI or MSI/X, should
	** return us the number of supported
	** vectors. (Will be 1 for MSI)
	*/
	pf->msix = ixl_init_msix(pf);
	return (0);
}

static void
ixl_free_pci_resources(struct ixl_pf * pf)
{
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = pf->dev;
	int			rid, memrid;

	memrid = PCIR_BAR(IXL_BAR);

	/* We may get here before stations are setup */
	if ((!ixl_enable_msix) || (que == NULL))
		goto early;

	/*
	**  Release all msix VSI resources:
	*/
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
	}

early:
	/* Clean the AdminQ interrupt last */
	if (pf->admvec) /* we are doing MSIX */
		rid = pf->admvec + 1;
	else
		(pf->msix != 0) ? (rid = 1):(rid = 0);

	if (pf->tag != NULL) {
		bus_teardown_intr(dev, pf->res, pf->tag);
		pf->tag = NULL;
	}
	if (pf->res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, rid, pf->res);

	if (pf->msix)
		pci_release_msi(dev);

	if (pf->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    memrid, pf->msix_mem);

	if (pf->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), pf->pci_mem);

	return;
}

static void
ixl_add_ifmedia(struct ixl_vsi *vsi, u32 phy_type)
{
	/* Display supported media types */
	if (phy_type & (1 << I40E_PHY_TYPE_100BASE_TX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_100_TX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_T))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_SX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_LX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_LX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_XAUI) ||
	    phy_type & (1 << I40E_PHY_TYPE_XFI) ||
	    phy_type & (1 << I40E_PHY_TYPE_10GBASE_SFPP_CU))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_SR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SR, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_LR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_LR, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_T))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_T, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4_CU) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_AOC) ||
	    phy_type & (1 << I40E_PHY_TYPE_XLAUI) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_CR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_SR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_LR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_LR4, 0, NULL);

#ifndef IFM_ETH_XTYPE
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_KX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_CX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1_CU) ||
	    phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1) ||
	    phy_type & (1 << I40E_PHY_TYPE_10GBASE_AOC) ||
	    phy_type & (1 << I40E_PHY_TYPE_SFI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KX4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_CX4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SR, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_XLPPI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_CR4, 0, NULL);
#else
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_KX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_KX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1_CU)
	    || phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_CR1, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_AOC))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX_LONG, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_SFI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SFI, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KX4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_KX4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_KR, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_20GBASE_KR2))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_20G_KR2, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_KR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_XLPPI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_XLPPI, 0, NULL);
#endif
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
ixl_setup_interface(device_t dev, struct ixl_vsi *vsi)
{
	struct ifnet		*ifp;
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;
	struct i40e_aq_get_phy_abilities_resp abilities;
	enum i40e_status_code aq_error = 0;

	INIT_DEBUGOUT("ixl_setup_interface: begin");

	ifp = vsi->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = IF_Gbps(40);
	ifp->if_init = ixl_init;
	ifp->if_softc = vsi;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixl_ioctl;

#if __FreeBSD_version >= 1100036
	if_setgetcounterfn(ifp, ixl_get_counter);
#endif

	ifp->if_transmit = ixl_mq_start;

	ifp->if_qflush = ixl_qflush;

	ifp->if_snd.ifq_maxlen = que->num_desc - 2;

	vsi->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM;
	ifp->if_capabilities |= IFCAP_HWCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_TSO;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_LRO;

	/* VLAN capabilties */
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING
			     |  IFCAP_VLAN_HWTSO
			     |  IFCAP_VLAN_MTU
			     |  IFCAP_VLAN_HWCSUM;
	ifp->if_capenable = ifp->if_capabilities;

	/*
	** Don't turn this on by default, if vlans are
	** created on another pseudo device (eg. lagg)
	** then vlan events are not passed thru, breaking
	** operation, but with HW FILTER off it works. If
	** using vlans directly on the ixl driver you can
	** enable this and get full hardware tag filtering.
	*/
	ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&vsi->media, IFM_IMASK, ixl_media_change,
		     ixl_media_status);

	aq_error = i40e_aq_get_phy_capabilities(hw,
	    FALSE, TRUE, &abilities, NULL);
	/* May need delay to detect fiber correctly */
	if (aq_error == I40E_ERR_UNKNOWN_PHY) {
		i40e_msec_delay(200);
		aq_error = i40e_aq_get_phy_capabilities(hw, FALSE,
		    TRUE, &abilities, NULL);
	}
	if (aq_error) {
		if (aq_error == I40E_ERR_UNKNOWN_PHY)
			device_printf(dev, "Unknown PHY type detected!\n");
		else
			device_printf(dev,
			    "Error getting supported media types, err %d,"
			    " AQ error %d\n", aq_error, hw->aq.asq_last_status);
		return (0);
	}

	ixl_add_ifmedia(vsi, abilities.phy_type);

	/* Use autoselect media by default */
	ifmedia_add(&vsi->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&vsi->media, IFM_ETHER | IFM_AUTO);

	ether_ifattach(ifp, hw->mac.addr);

	return (0);
}

/*
** Run when the Admin Queue gets a
** link transition interrupt.
*/
static void
ixl_link_event(struct ixl_pf *pf, struct i40e_arq_event_info *e)
{
	struct i40e_hw	*hw = &pf->hw; 
	struct i40e_aqc_get_link_status *status =
	    (struct i40e_aqc_get_link_status *)&e->desc.params.raw;
	bool check;

	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &check);
	pf->link_up = check;
#ifdef IXL_DEBUG
	printf("Link is %s\n", check ? "up":"down");
#endif
	/* Report if Unqualified modules are found */
	if ((status->link_info & I40E_AQ_MEDIA_AVAILABLE) &&
	    (!(status->an_info & I40E_AQ_QUALIFIED_MODULE)) &&
	    (!(status->link_info & I40E_AQ_LINK_UP)))
		device_printf(pf->dev, "Link failed because "
		    "an unqualified module was detected\n");

	return;
}

/*********************************************************************
 *
 *  Get Firmware Switch configuration
 *	- this will need to be more robust when more complex
 *	  switch configurations are enabled.
 *
 **********************************************************************/
static int
ixl_switch_config(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw; 
	struct ixl_vsi	*vsi = &pf->vsi;
	device_t 	dev = vsi->dev;
	struct i40e_aqc_get_switch_config_resp *sw_config;
	u8	aq_buf[I40E_AQ_LARGE_BUF];
	int	ret;
	u16	next = 0;

	memset(&aq_buf, 0, sizeof(aq_buf));
	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;
	ret = i40e_aq_get_switch_config(hw, sw_config,
	    sizeof(aq_buf), &next, NULL);
	if (ret) {
		device_printf(dev,"aq_get_switch_config failed (ret=%d)!!\n",
		    ret);
		return (ret);
	}
#ifdef IXL_DEBUG
	device_printf(dev,
	    "Switch config: header reported: %d in structure, %d total\n",
    	    sw_config->header.num_reported, sw_config->header.num_total);
	for (int i = 0; i < sw_config->header.num_reported; i++) {
		device_printf(dev,
		    "%d: type=%d seid=%d uplink=%d downlink=%d\n", i,
		    sw_config->element[i].element_type,
		    sw_config->element[i].seid,
		    sw_config->element[i].uplink_seid,
		    sw_config->element[i].downlink_seid);
	}
#endif
	/* Simplified due to a single VSI at the moment */
	vsi->uplink_seid = sw_config->element[0].uplink_seid;
	vsi->downlink_seid = sw_config->element[0].downlink_seid;
	vsi->seid = sw_config->element[0].seid;
	return (ret);
}

/*********************************************************************
 *
 *  Initialize the VSI:  this handles contexts, which means things
 *  			 like the number of descriptors, buffer size,
 *			 plus we init the rings thru this function.
 *
 **********************************************************************/
static int
ixl_initialize_vsi(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = vsi->back;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = vsi->dev;
	struct i40e_hw		*hw = vsi->hw;
	struct i40e_vsi_context	ctxt;
	int			err = 0;

	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = vsi->seid;
	if (pf->veb_seid != 0)
		ctxt.uplink_seid = pf->veb_seid;
	ctxt.pf_num = hw->pf_id;
	err = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
	if (err) {
		device_printf(dev,"get vsi params failed %x!!\n", err);
		return (err);
	}
#ifdef IXL_DEBUG
	printf("get_vsi_params: seid: %d, uplinkseid: %d, vsi_number: %d, "
	    "vsis_allocated: %d, vsis_unallocated: %d, flags: 0x%x, "
	    "pfnum: %d, vfnum: %d, stat idx: %d, enabled: %d\n", ctxt.seid,
	    ctxt.uplink_seid, ctxt.vsi_number,
	    ctxt.vsis_allocated, ctxt.vsis_unallocated,
	    ctxt.flags, ctxt.pf_num, ctxt.vf_num,
	    ctxt.info.stat_counter_idx, ctxt.info.up_enable_bits);
#endif
	/*
	** Set the queue and traffic class bits
	**  - when multiple traffic classes are supported
	**    this will need to be more robust.
	*/
	ctxt.info.valid_sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	ctxt.info.mapping_flags |= I40E_AQ_VSI_QUE_MAP_CONTIG;
	ctxt.info.queue_mapping[0] = 0; 
	ctxt.info.tc_mapping[0] = 0x0800; 

	/* Set VLAN receive stripping mode */
	ctxt.info.valid_sections |= I40E_AQ_VSI_PROP_VLAN_VALID;
	ctxt.info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL;
	if (vsi->ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
	    ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	else
	    ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	/* Keep copy of VSI info in VSI for statistic counters */
	memcpy(&vsi->info, &ctxt.info, sizeof(ctxt.info));

	/* Reset VSI statistics */
	ixl_vsi_reset_stats(vsi);
	vsi->hw_filters_add = 0;
	vsi->hw_filters_del = 0;

	ctxt.flags = htole16(I40E_AQ_VSI_TYPE_PF);

	err = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (err) {
		device_printf(dev,"update vsi params failed %x!!\n",
		   hw->aq.asq_last_status);
		return (err);
	}

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring		*txr = &que->txr;
		struct rx_ring 		*rxr = &que->rxr;
		struct i40e_hmc_obj_txq tctx;
		struct i40e_hmc_obj_rxq rctx;
		u32			txctl;
		u16			size;


		/* Setup the HMC TX Context  */
		size = que->num_desc * sizeof(struct i40e_tx_desc);
		memset(&tctx, 0, sizeof(struct i40e_hmc_obj_txq));
		tctx.new_context = 1;
		tctx.base = (txr->dma.pa/IXL_TX_CTX_BASE_UNITS);
		tctx.qlen = que->num_desc;
		tctx.fc_ena = 0;
		tctx.rdylist = vsi->info.qs_handle[0]; /* index is TC */
		/* Enable HEAD writeback */
		tctx.head_wb_ena = 1;
		tctx.head_wb_addr = txr->dma.pa +
		    (que->num_desc * sizeof(struct i40e_tx_desc));
		tctx.rdylist_act = 0;
		err = i40e_clear_lan_tx_queue_context(hw, i);
		if (err) {
			device_printf(dev, "Unable to clear TX context\n");
			break;
		}
		err = i40e_set_lan_tx_queue_context(hw, i, &tctx);
		if (err) {
			device_printf(dev, "Unable to set TX context\n");
			break;
		}
		/* Associate the ring with this PF */
		txctl = I40E_QTX_CTL_PF_QUEUE;
		txctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
		    I40E_QTX_CTL_PF_INDX_MASK);
		wr32(hw, I40E_QTX_CTL(i), txctl);
		ixl_flush(hw);

		/* Do ring (re)init */
		ixl_init_tx_ring(que);

		/* Next setup the HMC RX Context  */
		if (vsi->max_frame_size <= MCLBYTES)
			rxr->mbuf_sz = MCLBYTES;
		else
			rxr->mbuf_sz = MJUMPAGESIZE;

		u16 max_rxmax = rxr->mbuf_sz * hw->func_caps.rx_buf_chain_len;

		/* Set up an RX context for the HMC */
		memset(&rctx, 0, sizeof(struct i40e_hmc_obj_rxq));
		rctx.dbuff = rxr->mbuf_sz >> I40E_RXQ_CTX_DBUFF_SHIFT;
		/* ignore header split for now */
		rctx.hbuff = 0 >> I40E_RXQ_CTX_HBUFF_SHIFT;
		rctx.rxmax = (vsi->max_frame_size < max_rxmax) ?
		    vsi->max_frame_size : max_rxmax;
		rctx.dtype = 0;
		rctx.dsize = 1;	/* do 32byte descriptors */
		rctx.hsplit_0 = 0;  /* no HDR split initially */
		rctx.base = (rxr->dma.pa/IXL_RX_CTX_BASE_UNITS);
		rctx.qlen = que->num_desc;
		rctx.tphrdesc_ena = 1;
		rctx.tphwdesc_ena = 1;
		rctx.tphdata_ena = 0;
		rctx.tphhead_ena = 0;
		rctx.lrxqthresh = 2;
		rctx.crcstrip = 1;
		rctx.l2tsel = 1;
		rctx.showiv = 1;
		rctx.fc_ena = 0;
		rctx.prefena = 1;

		err = i40e_clear_lan_rx_queue_context(hw, i);
		if (err) {
			device_printf(dev,
			    "Unable to clear RX context %d\n", i);
			break;
		}
		err = i40e_set_lan_rx_queue_context(hw, i, &rctx);
		if (err) {
			device_printf(dev, "Unable to set RX context %d\n", i);
			break;
		}
		err = ixl_init_rx_ring(que);
		if (err) {
			device_printf(dev, "Fail in init_rx_ring %d\n", i);
			break;
		}
		wr32(vsi->hw, I40E_QRX_TAIL(que->me), 0);
#ifdef DEV_NETMAP
		/* preserve queue */
		if (vsi->ifp->if_capenable & IFCAP_NETMAP) {
			struct netmap_adapter *na = NA(vsi->ifp);
			struct netmap_kring *kring = &na->rx_rings[i];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);
			wr32(vsi->hw, I40E_QRX_TAIL(que->me), t);
		} else
#endif /* DEV_NETMAP */
		wr32(vsi->hw, I40E_QRX_TAIL(que->me), que->num_desc - 1);
	}
	return (err);
}


/*********************************************************************
 *
 *  Free all VSI structs.
 *
 **********************************************************************/
void
ixl_free_vsi(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = (struct ixl_pf *)vsi->back;
	struct ixl_queue	*que = vsi->queues;

	/* Free station queues */
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		struct rx_ring *rxr = &que->rxr;
	
		if (!mtx_initialized(&txr->mtx)) /* uninitialized */
			continue;
		IXL_TX_LOCK(txr);
		ixl_free_que_tx(que);
		if (txr->base)
			i40e_free_dma_mem(&pf->hw, &txr->dma);
		IXL_TX_UNLOCK(txr);
		IXL_TX_LOCK_DESTROY(txr);

		if (!mtx_initialized(&rxr->mtx)) /* uninitialized */
			continue;
		IXL_RX_LOCK(rxr);
		ixl_free_que_rx(que);
		if (rxr->base)
			i40e_free_dma_mem(&pf->hw, &rxr->dma);
		IXL_RX_UNLOCK(rxr);
		IXL_RX_LOCK_DESTROY(rxr);
		
	}
	free(vsi->queues, M_DEVBUF);

	/* Free VSI filter list */
	ixl_free_mac_filters(vsi);
}

static void
ixl_free_mac_filters(struct ixl_vsi *vsi)
{
	struct ixl_mac_filter *f;

	while (!SLIST_EMPTY(&vsi->ftl)) {
		f = SLIST_FIRST(&vsi->ftl);
		SLIST_REMOVE_HEAD(&vsi->ftl, next);
		free(f, M_DEVBUF);
	}
}


/*********************************************************************
 *
 *  Allocate memory for the VSI (virtual station interface) and their
 *  associated queues, rings and the descriptors associated with each,
 *  called only once at attach.
 *
 **********************************************************************/
static int
ixl_setup_stations(struct ixl_pf *pf)
{
	device_t		dev = pf->dev;
	struct ixl_vsi		*vsi;
	struct ixl_queue	*que;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	int 			rsize, tsize;
	int			error = I40E_SUCCESS;

	vsi = &pf->vsi;
	vsi->back = (void *)pf;
	vsi->hw = &pf->hw;
	vsi->id = 0;
	vsi->num_vlans = 0;
	vsi->back = pf;

	/* Get memory for the station queues */
        if (!(vsi->queues =
            (struct ixl_queue *) malloc(sizeof(struct ixl_queue) *
            vsi->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                device_printf(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                goto early;
        }

	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		que->num_desc = ixl_ringsz;
		que->me = i;
		que->vsi = vsi;
		/* mark the queue as active */
		vsi->active_queues |= (u64)1 << que->me;
		txr = &que->txr;
		txr->que = que;
		txr->tail = I40E_QTX_TAIL(que->me);

		/* Initialize the TX lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(dev), que->me);
		mtx_init(&txr->mtx, txr->mtx_name, NULL, MTX_DEF);
		/* Create the TX descriptor ring */
		tsize = roundup2((que->num_desc *
		    sizeof(struct i40e_tx_desc)) +
		    sizeof(u32), DBA_ALIGN);
		if (i40e_allocate_dma_mem(&pf->hw,
		    &txr->dma, i40e_mem_reserved, tsize, DBA_ALIGN)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		txr->base = (struct i40e_tx_desc *)txr->dma.va;
		bzero((void *)txr->base, tsize);
       		/* Now allocate transmit soft structs for the ring */
       		if (ixl_allocate_tx_data(que)) {
			device_printf(dev,
			    "Critical Failure setting up TX structures\n");
			error = ENOMEM;
			goto fail;
       		}
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(4096, M_DEVBUF,
		    M_WAITOK, &txr->mtx);
		if (txr->br == NULL) {
			device_printf(dev,
			    "Critical Failure setting up TX buf ring\n");
			error = ENOMEM;
			goto fail;
       		}

		/*
		 * Next the RX queues...
		 */ 
		rsize = roundup2(que->num_desc *
		    sizeof(union i40e_rx_desc), DBA_ALIGN);
		rxr = &que->rxr;
		rxr->que = que;
		rxr->tail = I40E_QRX_TAIL(que->me);

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), que->me);
		mtx_init(&rxr->mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (i40e_allocate_dma_mem(&pf->hw,
		    &rxr->dma, i40e_mem_reserved, rsize, 4096)) {
			device_printf(dev,
			    "Unable to allocate RX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		rxr->base = (union i40e_rx_desc *)rxr->dma.va;
		bzero((void *)rxr->base, rsize);

        	/* Allocate receive soft structs for the ring*/
		if (ixl_allocate_rx_data(que)) {
			device_printf(dev,
			    "Critical Failure setting up receive structs\n");
			error = ENOMEM;
			goto fail;
		}
	}

	return (0);

fail:
	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		rxr = &que->rxr;
		txr = &que->txr;
		if (rxr->base)
			i40e_free_dma_mem(&pf->hw, &rxr->dma);
		if (txr->base)
			i40e_free_dma_mem(&pf->hw, &txr->dma);
	}

early:
	return (error);
}

/*
** Provide a update to the queue RX
** interrupt moderation value.
*/
static void
ixl_set_queue_rx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct rx_ring	*rxr = &que->rxr;
	u16		rx_itr;
	u16		rx_latency = 0;
	int		rx_bytes;


	/* Idle, do nothing */
	if (rxr->bytes == 0)
		return;

	if (ixl_dynamic_rx_itr) {
		rx_bytes = rxr->bytes/rxr->itr;
		rx_itr = rxr->itr;

		/* Adjust latency range */
		switch (rxr->latency) {
		case IXL_LOW_LATENCY:
			if (rx_bytes > 10) {
				rx_latency = IXL_AVE_LATENCY;
				rx_itr = IXL_ITR_20K;
			}
			break;
		case IXL_AVE_LATENCY:
			if (rx_bytes > 20) {
				rx_latency = IXL_BULK_LATENCY;
				rx_itr = IXL_ITR_8K;
			} else if (rx_bytes <= 10) {
				rx_latency = IXL_LOW_LATENCY;
				rx_itr = IXL_ITR_100K;
			}
			break;
		case IXL_BULK_LATENCY:
			if (rx_bytes <= 20) {
				rx_latency = IXL_AVE_LATENCY;
				rx_itr = IXL_ITR_20K;
			}
			break;
       		 }

		rxr->latency = rx_latency;

		if (rx_itr != rxr->itr) {
			/* do an exponential smoothing */
			rx_itr = (10 * rx_itr * rxr->itr) /
			    ((9 * rx_itr) + rxr->itr);
			rxr->itr = rx_itr & IXL_MAX_ITR;
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
			    que->me), rxr->itr);
		}
	} else { /* We may have have toggled to non-dynamic */
		if (vsi->rx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->rx_itr_setting = ixl_rx_itr;
		/* Update the hardware if needed */
		if (rxr->itr != vsi->rx_itr_setting) {
			rxr->itr = vsi->rx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
			    que->me), rxr->itr);
		}
	}
	rxr->bytes = 0;
	rxr->packets = 0;
	return;
}


/*
** Provide a update to the queue TX
** interrupt moderation value.
*/
static void
ixl_set_queue_tx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	u16		tx_itr;
	u16		tx_latency = 0;
	int		tx_bytes;


	/* Idle, do nothing */
	if (txr->bytes == 0)
		return;

	if (ixl_dynamic_tx_itr) {
		tx_bytes = txr->bytes/txr->itr;
		tx_itr = txr->itr;

		switch (txr->latency) {
		case IXL_LOW_LATENCY:
			if (tx_bytes > 10) {
				tx_latency = IXL_AVE_LATENCY;
				tx_itr = IXL_ITR_20K;
			}
			break;
		case IXL_AVE_LATENCY:
			if (tx_bytes > 20) {
				tx_latency = IXL_BULK_LATENCY;
				tx_itr = IXL_ITR_8K;
			} else if (tx_bytes <= 10) {
				tx_latency = IXL_LOW_LATENCY;
				tx_itr = IXL_ITR_100K;
			}
			break;
		case IXL_BULK_LATENCY:
			if (tx_bytes <= 20) {
				tx_latency = IXL_AVE_LATENCY;
				tx_itr = IXL_ITR_20K;
			}
			break;
		}

		txr->latency = tx_latency;

		if (tx_itr != txr->itr) {
       	         /* do an exponential smoothing */
			tx_itr = (10 * tx_itr * txr->itr) /
			    ((9 * tx_itr) + txr->itr);
			txr->itr = tx_itr & IXL_MAX_ITR;
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    que->me), txr->itr);
		}

	} else { /* We may have have toggled to non-dynamic */
		if (vsi->tx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->tx_itr_setting = ixl_tx_itr;
		/* Update the hardware if needed */
		if (txr->itr != vsi->tx_itr_setting) {
			txr->itr = vsi->tx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    que->me), txr->itr);
		}
	}
	txr->bytes = 0;
	txr->packets = 0;
	return;
}

#define QUEUE_NAME_LEN 32

static void
ixl_add_vsi_sysctls(struct ixl_pf *pf, struct ixl_vsi *vsi,
    struct sysctl_ctx_list *ctx, const char *sysctl_name)
{
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	struct sysctl_oid_list *vsi_list;

	tree = device_get_sysctl_tree(pf->dev);
	child = SYSCTL_CHILDREN(tree);
	vsi->vsi_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, sysctl_name,
				   CTLFLAG_RD, NULL, "VSI Number");
	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	ixl_add_sysctls_eth_stats(ctx, vsi_list, &vsi->eth_stats);
}

static void
ixl_add_hw_stats(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	struct ixl_vsi *vsi = &pf->vsi;
	struct ixl_queue *queues = vsi->queues;
	struct i40e_hw_port_stats *pf_stats = &pf->stats;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct sysctl_oid_list *vsi_list;

	struct sysctl_oid *queue_node;
	struct sysctl_oid_list *queue_list;

	struct tx_ring *txr;
	struct rx_ring *rxr;
	char queue_namebuf[QUEUE_NAME_LEN];

	/* Driver statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
			CTLFLAG_RD, &pf->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "admin_irq",
			CTLFLAG_RD, &pf->admin_irq,
			"Admin Queue IRQ Handled");

	ixl_add_vsi_sysctls(pf, &pf->vsi, ctx, "pf");
	vsi_list = SYSCTL_CHILDREN(pf->vsi.vsi_node);

	/* Queue statistics */
	for (int q = 0; q < vsi->num_queues; q++) {
		snprintf(queue_namebuf, QUEUE_NAME_LEN, "que%d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list,
		    OID_AUTO, queue_namebuf, CTLFLAG_RD, NULL, "Queue #");
		queue_list = SYSCTL_CHILDREN(queue_node);

		txr = &(queues[q].txr);
		rxr = &(queues[q].rxr);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "mbuf_defrag_failed",
				CTLFLAG_RD, &(queues[q].mbuf_defrag_failed),
				"m_defrag() failed");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "dropped",
				CTLFLAG_RD, &(queues[q].dropped_pkts),
				"Driver dropped packets");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
				CTLFLAG_RD, &(queues[q].irqs),
				"irqs on this queue");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tso_tx",
				CTLFLAG_RD, &(queues[q].tso),
				"TSO");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_dma_setup",
				CTLFLAG_RD, &(queues[q].tx_dma_setup),
				"Driver tx dma failure in xmit");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "no_desc_avail",
				CTLFLAG_RD, &(txr->no_desc),
				"Queue No Descriptor Available");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_packets",
				CTLFLAG_RD, &(txr->total_packets),
				"Queue Packets Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_bytes",
				CTLFLAG_RD, &(txr->tx_bytes),
				"Queue Bytes Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
				CTLFLAG_RD, &(rxr->rx_packets),
				"Queue Packets Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
				CTLFLAG_RD, &(rxr->rx_bytes),
				"Queue Bytes Received");
	}

	/* MAC stats */
	ixl_add_sysctls_mac_stats(ctx, child, pf_stats);
}

static void
ixl_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_eth_stats *eth_stats)
{
	struct ixl_sysctl_info ctls[] =
	{
		{&eth_stats->rx_bytes, "good_octets_rcvd", "Good Octets Received"},
		{&eth_stats->rx_unicast, "ucast_pkts_rcvd",
			"Unicast Packets Received"},
		{&eth_stats->rx_multicast, "mcast_pkts_rcvd",
			"Multicast Packets Received"},
		{&eth_stats->rx_broadcast, "bcast_pkts_rcvd",
			"Broadcast Packets Received"},
		{&eth_stats->rx_discards, "rx_discards", "Discarded RX packets"},
		{&eth_stats->tx_bytes, "good_octets_txd", "Good Octets Transmitted"},
		{&eth_stats->tx_unicast, "ucast_pkts_txd", "Unicast Packets Transmitted"},
		{&eth_stats->tx_multicast, "mcast_pkts_txd",
			"Multicast Packets Transmitted"},
		{&eth_stats->tx_broadcast, "bcast_pkts_txd",
			"Broadcast Packets Transmitted"},
		// end
		{0,0,0}
	};

	struct ixl_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

static void
ixl_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_hw_port_stats *stats)
{
	struct sysctl_oid *stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
				    CTLFLAG_RD, NULL, "Mac Statistics");
	struct sysctl_oid_list *stat_list = SYSCTL_CHILDREN(stat_node);

	struct i40e_eth_stats *eth_stats = &stats->eth;
	ixl_add_sysctls_eth_stats(ctx, stat_list, eth_stats);

	struct ixl_sysctl_info ctls[] = 
	{
		{&stats->crc_errors, "crc_errors", "CRC Errors"},
		{&stats->illegal_bytes, "illegal_bytes", "Illegal Byte Errors"},
		{&stats->mac_local_faults, "local_faults", "MAC Local Faults"},
		{&stats->mac_remote_faults, "remote_faults", "MAC Remote Faults"},
		{&stats->rx_length_errors, "rx_length_errors", "Receive Length Errors"},
		/* Packet Reception Stats */
		{&stats->rx_size_64, "rx_frames_64", "64 byte frames received"},
		{&stats->rx_size_127, "rx_frames_65_127", "65-127 byte frames received"},
		{&stats->rx_size_255, "rx_frames_128_255", "128-255 byte frames received"},
		{&stats->rx_size_511, "rx_frames_256_511", "256-511 byte frames received"},
		{&stats->rx_size_1023, "rx_frames_512_1023", "512-1023 byte frames received"},
		{&stats->rx_size_1522, "rx_frames_1024_1522", "1024-1522 byte frames received"},
		{&stats->rx_size_big, "rx_frames_big", "1523-9522 byte frames received"},
		{&stats->rx_undersize, "rx_undersize", "Undersized packets received"},
		{&stats->rx_fragments, "rx_fragmented", "Fragmented packets received"},
		{&stats->rx_oversize, "rx_oversized", "Oversized packets received"},
		{&stats->rx_jabber, "rx_jabber", "Received Jabber"},
		{&stats->checksum_error, "checksum_errors", "Checksum Errors"},
		/* Packet Transmission Stats */
		{&stats->tx_size_64, "tx_frames_64", "64 byte frames transmitted"},
		{&stats->tx_size_127, "tx_frames_65_127", "65-127 byte frames transmitted"},
		{&stats->tx_size_255, "tx_frames_128_255", "128-255 byte frames transmitted"},
		{&stats->tx_size_511, "tx_frames_256_511", "256-511 byte frames transmitted"},
		{&stats->tx_size_1023, "tx_frames_512_1023", "512-1023 byte frames transmitted"},
		{&stats->tx_size_1522, "tx_frames_1024_1522", "1024-1522 byte frames transmitted"},
		{&stats->tx_size_big, "tx_frames_big", "1523-9522 byte frames transmitted"},
		/* Flow control */
		{&stats->link_xon_tx, "xon_txd", "Link XON transmitted"},
		{&stats->link_xon_rx, "xon_recvd", "Link XON received"},
		{&stats->link_xoff_tx, "xoff_txd", "Link XOFF transmitted"},
		{&stats->link_xoff_rx, "xoff_recvd", "Link XOFF received"},
		/* End */
		{0,0,0}
	};

	struct ixl_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}


/*
** ixl_config_rss - setup RSS 
**  - note this is done for the single vsi
*/
static void ixl_config_rss(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw	*hw = vsi->hw;
	u32		lut = 0;
	u64		set_hena = 0, hena;
	int		i, j, que_id;
#ifdef RSS
	u32		rss_hash_config;
	u32		rss_seed[IXL_KEYSZ];
#else
	u32             rss_seed[IXL_KEYSZ] = {0x41b01687,
			    0x183cfd8c, 0xce880440, 0x580cbc3c,
			    0x35897377, 0x328b25e1, 0x4fa98922,
			    0xb7d90c14, 0xd5bad70d, 0xcd15a2c1};
#endif

#ifdef RSS
        /* Fetch the configured RSS key */
        rss_getkey((uint8_t *) &rss_seed);
#endif

	/* Fill out hash function seed */
	for (i = 0; i < IXL_KEYSZ; i++)
                wr32(hw, I40E_PFQF_HKEY(i), rss_seed[i]);

	/* Enable PCTYPES for RSS: */
#ifdef RSS
	rss_hash_config = rss_gethashconfig();
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_UDP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_FRAG_IPV6);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_TCP);
        if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_UDP);
#else
	set_hena =
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_UDP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_TCP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER) |
		((u64)1 << I40E_FILTER_PCTYPE_FRAG_IPV4) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_UDP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_TCP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) |
		((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER) |
		((u64)1 << I40E_FILTER_PCTYPE_FRAG_IPV6) |
		((u64)1 << I40E_FILTER_PCTYPE_L2_PAYLOAD);
#endif
	hena = (u64)rd32(hw, I40E_PFQF_HENA(0)) |
	    ((u64)rd32(hw, I40E_PFQF_HENA(1)) << 32);
	hena |= set_hena;
	wr32(hw, I40E_PFQF_HENA(0), (u32)hena);
	wr32(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = j = 0; i < pf->hw.func_caps.rss_table_size; i++, j++) {
		if (j == vsi->num_queues)
			j = 0;
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % vsi->num_queues;
#else
		que_id = j;
#endif
		/* lut = 4-byte sliding window of 4 lut entries */
		lut = (lut << 8) | (que_id &
		    ((0x1 << pf->hw.func_caps.rss_table_entry_width) - 1));
		/* On i = 3, we have 4 entries in lut; write to the register */
		if ((i & 3) == 3)
			wr32(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}
	ixl_flush(hw);
}


/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixl_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXL_PF_LOCK(pf);
	++vsi->num_vlans;
	ixl_add_filter(vsi, hw->mac.addr, vtag);
	IXL_PF_UNLOCK(pf);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixl_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXL_PF_LOCK(pf);
	--vsi->num_vlans;
	ixl_del_filter(vsi, hw->mac.addr, vtag);
	IXL_PF_UNLOCK(pf);
}

/*
** This routine updates vlan filters, called by init
** it scans the filter table and then updates the hw
** after a soft reset.
*/
static void
ixl_setup_vlan_filters(struct ixl_vsi *vsi)
{
	struct ixl_mac_filter	*f;
	int			cnt = 0, flags;

	if (vsi->num_vlans == 0)
		return;
	/*
	** Scan the filter list for vlan entries,
	** mark them for addition and then call
	** for the AQ update.
	*/
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (f->flags & IXL_FILTER_VLAN) {
			f->flags |=
			    (IXL_FILTER_ADD |
			    IXL_FILTER_USED);
			cnt++;
		}
	}
	if (cnt == 0) {
		printf("setup vlan: no filters found!\n");
		return;
	}
	flags = IXL_FILTER_VLAN;
	flags |= (IXL_FILTER_ADD | IXL_FILTER_USED);
	ixl_add_hw_filters(vsi, flags, cnt);
	return;
}

/*
** Initialize filter list and add filters that the hardware
** needs to know about.
*/
static void
ixl_init_filters(struct ixl_vsi *vsi)
{
	/* Add broadcast address */
	ixl_add_filter(vsi, ixl_bcast_addr, IXL_VLAN_ANY);
}

/*
** This routine adds mulicast filters
*/
static void
ixl_add_mc_filter(struct ixl_vsi *vsi, u8 *macaddr)
{
	struct ixl_mac_filter *f;

	/* Does one already exist */
	f = ixl_find_filter(vsi, macaddr, IXL_VLAN_ANY);
	if (f != NULL)
		return;

	f = ixl_get_filter(vsi);
	if (f == NULL) {
		printf("WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = IXL_VLAN_ANY;
	f->flags |= (IXL_FILTER_ADD | IXL_FILTER_USED
	    | IXL_FILTER_MC);

	return;
}

static void
ixl_reconfigure_filters(struct ixl_vsi *vsi)
{

	ixl_add_hw_filters(vsi, IXL_FILTER_USED, vsi->num_macs);
}

/*
** This routine adds macvlan filters
*/
static void
ixl_add_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f, *tmp;
	struct ixl_pf		*pf;
	device_t		dev;

	DEBUGOUT("ixl_add_filter: begin");

	pf = vsi->back;
	dev = pf->dev;

	/* Does one already exist */
	f = ixl_find_filter(vsi, macaddr, vlan);
	if (f != NULL)
		return;
	/*
	** Is this the first vlan being registered, if so we
	** need to remove the ANY filter that indicates we are
	** not in a vlan, and replace that with a 0 filter.
	*/
	if ((vlan != IXL_VLAN_ANY) && (vsi->num_vlans == 1)) {
		tmp = ixl_find_filter(vsi, macaddr, IXL_VLAN_ANY);
		if (tmp != NULL) {
			ixl_del_filter(vsi, macaddr, IXL_VLAN_ANY);
			ixl_add_filter(vsi, macaddr, 0);
		}
	}

	f = ixl_get_filter(vsi);
	if (f == NULL) {
		device_printf(dev, "WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = vlan;
	f->flags |= (IXL_FILTER_ADD | IXL_FILTER_USED);
	if (f->vlan != IXL_VLAN_ANY)
		f->flags |= IXL_FILTER_VLAN;
	else
		vsi->num_macs++;

	ixl_add_hw_filters(vsi, f->flags, 1);
	return;
}

static void
ixl_del_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter *f;

	f = ixl_find_filter(vsi, macaddr, vlan);
	if (f == NULL)
		return;

	f->flags |= IXL_FILTER_DEL;
	ixl_del_hw_filters(vsi, 1);
	vsi->num_macs--;

	/* Check if this is the last vlan removal */
	if (vlan != IXL_VLAN_ANY && vsi->num_vlans == 0) {
		/* Switch back to a non-vlan filter */
		ixl_del_filter(vsi, macaddr, 0);
		ixl_add_filter(vsi, macaddr, IXL_VLAN_ANY);
	}
	return;
}

/*
** Find the filter with both matching mac addr and vlan id
*/
static struct ixl_mac_filter *
ixl_find_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f;
	bool			match = FALSE;

	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (!cmp_etheraddr(f->macaddr, macaddr))
			continue;
		if (f->vlan == vlan) {
			match = TRUE;
			break;
		}
	}	

	if (!match)
		f = NULL;
	return (f);
}

/*
** This routine takes additions to the vsi filter
** table and creates an Admin Queue call to create
** the filters in the hardware.
*/
static void
ixl_add_hw_filters(struct ixl_vsi *vsi, int flags, int cnt)
{
	struct i40e_aqc_add_macvlan_element_data *a, *b;
	struct ixl_mac_filter	*f;
	struct ixl_pf		*pf;
	struct i40e_hw		*hw;
	device_t		dev;
	int			err, j = 0;

	pf = vsi->back;
	dev = pf->dev;
	hw = &pf->hw;
	IXL_PF_LOCK_ASSERT(pf);

	a = malloc(sizeof(struct i40e_aqc_add_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "add_hw_filters failed to get memory\n");
		return;
	}

	/*
	** Scan the filter list, each time we find one
	** we add it to the admin queue array and turn off
	** the add bit.
	*/
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (f->flags == flags) {
			b = &a[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, b->mac_addr, ETHER_ADDR_LEN);
			if (f->vlan == IXL_VLAN_ANY) {
				b->vlan_tag = 0;
				b->flags = I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
			} else {
				b->vlan_tag = f->vlan;
				b->flags = 0;
			}
			b->flags |= I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
			f->flags &= ~IXL_FILTER_ADD;
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		err = i40e_aq_add_macvlan(hw, vsi->seid, a, j, NULL);
		if (err) 
			device_printf(dev, "aq_add_macvlan err %d, "
			    "aq_error %d\n", err, hw->aq.asq_last_status);
		else
			vsi->hw_filters_add += j;
	}
	free(a, M_DEVBUF);
	return;
}

/*
** This routine takes removals in the vsi filter
** table and creates an Admin Queue call to delete
** the filters in the hardware.
*/
static void
ixl_del_hw_filters(struct ixl_vsi *vsi, int cnt)
{
	struct i40e_aqc_remove_macvlan_element_data *d, *e;
	struct ixl_pf		*pf;
	struct i40e_hw		*hw;
	device_t		dev;
	struct ixl_mac_filter	*f, *f_temp;
	int			err, j = 0;

	DEBUGOUT("ixl_del_hw_filters: begin\n");

	pf = vsi->back;
	hw = &pf->hw;
	dev = pf->dev;

	d = malloc(sizeof(struct i40e_aqc_remove_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		printf("del hw filter failed to get memory\n");
		return;
	}

	SLIST_FOREACH_SAFE(f, &vsi->ftl, next, f_temp) {
		if (f->flags & IXL_FILTER_DEL) {
			e = &d[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, e->mac_addr, ETHER_ADDR_LEN);
			e->vlan_tag = (f->vlan == IXL_VLAN_ANY ? 0 : f->vlan);
			e->flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
			/* delete entry from vsi list */
			SLIST_REMOVE(&vsi->ftl, f, ixl_mac_filter, next);
			free(f, M_DEVBUF);
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		err = i40e_aq_remove_macvlan(hw, vsi->seid, d, j, NULL);
		/* NOTE: returns ENOENT every time but seems to work fine,
		   so we'll ignore that specific error. */
		// TODO: Does this still occur on current firmwares?
		if (err && hw->aq.asq_last_status != I40E_AQ_RC_ENOENT) {
			int sc = 0;
			for (int i = 0; i < j; i++)
				sc += (!d[i].error_code);
			vsi->hw_filters_del += sc;
			device_printf(dev,
			    "Failed to remove %d/%d filters, aq error %d\n",
			    j - sc, j, hw->aq.asq_last_status);
		} else
			vsi->hw_filters_del += j;
	}
	free(d, M_DEVBUF);

	DEBUGOUT("ixl_del_hw_filters: end\n");
	return;
}

static int
ixl_enable_rings(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = vsi->back;
	struct i40e_hw	*hw = &pf->hw;
	int		index, error;
	u32		reg;

	error = 0;
	for (int i = 0; i < vsi->num_queues; i++) {
		index = vsi->first_queue + i;
		i40e_pre_tx_queue_cfg(hw, index, TRUE);

		reg = rd32(hw, I40E_QTX_ENA(index));
		reg |= I40E_QTX_ENA_QENA_REQ_MASK |
		    I40E_QTX_ENA_QENA_STAT_MASK;
		wr32(hw, I40E_QTX_ENA(index), reg);
		/* Verify the enable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QTX_ENA(index));
			if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
				break;
			i40e_msec_delay(10);
		}
		if ((reg & I40E_QTX_ENA_QENA_STAT_MASK) == 0) {
			device_printf(pf->dev, "TX queue %d disabled!\n",
			    index);
			error = ETIMEDOUT;
		}

		reg = rd32(hw, I40E_QRX_ENA(index));
		reg |= I40E_QRX_ENA_QENA_REQ_MASK |
		    I40E_QRX_ENA_QENA_STAT_MASK;
		wr32(hw, I40E_QRX_ENA(index), reg);
		/* Verify the enable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QRX_ENA(index));
			if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
				break;
			i40e_msec_delay(10);
		}
		if ((reg & I40E_QRX_ENA_QENA_STAT_MASK) == 0) {
			device_printf(pf->dev, "RX queue %d disabled!\n",
			    index);
			error = ETIMEDOUT;
		}
	}

	return (error);
}

static int
ixl_disable_rings(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = vsi->back;
	struct i40e_hw	*hw = &pf->hw;
	int		index, error;
	u32		reg;

	error = 0;
	for (int i = 0; i < vsi->num_queues; i++) {
		index = vsi->first_queue + i;

		i40e_pre_tx_queue_cfg(hw, index, FALSE);
		i40e_usec_delay(500);

		reg = rd32(hw, I40E_QTX_ENA(index));
		reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QTX_ENA(index), reg);
		/* Verify the disable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QTX_ENA(index));
			if (!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
			i40e_msec_delay(10);
		}
		if (reg & I40E_QTX_ENA_QENA_STAT_MASK) {
			device_printf(pf->dev, "TX queue %d still enabled!\n",
			    index);
			error = ETIMEDOUT;
		}

		reg = rd32(hw, I40E_QRX_ENA(index));
		reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QRX_ENA(index), reg);
		/* Verify the disable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QRX_ENA(index));
			if (!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
			i40e_msec_delay(10);
		}
		if (reg & I40E_QRX_ENA_QENA_STAT_MASK) {
			device_printf(pf->dev, "RX queue %d still enabled!\n",
			    index);
			error = ETIMEDOUT;
		}
	}

	return (error);
}

/**
 * ixl_handle_mdd_event
 *
 * Called from interrupt handler to identify possibly malicious vfs
 * (But also detects events from the PF, as well)
 **/
static void ixl_handle_mdd_event(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	bool mdd_detected = false;
	bool pf_mdd_detected = false;
	u32 reg;

	/* find what triggered the MDD event */
	reg = rd32(hw, I40E_GL_MDET_TX);
	if (reg & I40E_GL_MDET_TX_VALID_MASK) {
		u8 pf_num = (reg & I40E_GL_MDET_TX_PF_NUM_MASK) >>
				I40E_GL_MDET_TX_PF_NUM_SHIFT;
		u8 event = (reg & I40E_GL_MDET_TX_EVENT_MASK) >>
				I40E_GL_MDET_TX_EVENT_SHIFT;
		u8 queue = (reg & I40E_GL_MDET_TX_QUEUE_MASK) >>
				I40E_GL_MDET_TX_QUEUE_SHIFT;
		device_printf(dev,
			 "Malicious Driver Detection event 0x%02x"
			 " on TX queue %d pf number 0x%02x\n",
			 event, queue, pf_num);
		wr32(hw, I40E_GL_MDET_TX, 0xffffffff);
		mdd_detected = true;
	}
	reg = rd32(hw, I40E_GL_MDET_RX);
	if (reg & I40E_GL_MDET_RX_VALID_MASK) {
		u8 func = (reg & I40E_GL_MDET_RX_FUNCTION_MASK) >>
				I40E_GL_MDET_RX_FUNCTION_SHIFT;
		u8 event = (reg & I40E_GL_MDET_RX_EVENT_MASK) >>
				I40E_GL_MDET_RX_EVENT_SHIFT;
		u8 queue = (reg & I40E_GL_MDET_RX_QUEUE_MASK) >>
				I40E_GL_MDET_RX_QUEUE_SHIFT;
		device_printf(dev,
			 "Malicious Driver Detection event 0x%02x"
			 " on RX queue %d of function 0x%02x\n",
			 event, queue, func);
		wr32(hw, I40E_GL_MDET_RX, 0xffffffff);
		mdd_detected = true;
	}

	if (mdd_detected) {
		reg = rd32(hw, I40E_PF_MDET_TX);
		if (reg & I40E_PF_MDET_TX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_TX, 0xFFFF);
			device_printf(dev,
				 "MDD TX event is for this function 0x%08x",
				 reg);
			pf_mdd_detected = true;
		}
		reg = rd32(hw, I40E_PF_MDET_RX);
		if (reg & I40E_PF_MDET_RX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_RX, 0xFFFF);
			device_printf(dev,
				 "MDD RX event is for this function 0x%08x",
				 reg);
			pf_mdd_detected = true;
		}
	}

	/* re-enable mdd interrupt cause */
	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	ixl_flush(hw);
}

static void
ixl_enable_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;

	if (ixl_enable_msix) {
		ixl_enable_adminq(hw);
		for (int i = 0; i < vsi->num_queues; i++, que++)
			ixl_enable_queue(hw, que->me);
	} else
		ixl_enable_legacy(hw);
}

static void
ixl_disable_rings_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;

	for (int i = 0; i < vsi->num_queues; i++, que++)
		ixl_disable_queue(hw, que->me);
}

static void
ixl_disable_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;

	if (ixl_enable_msix)
		ixl_disable_adminq(hw);
	else
		ixl_disable_legacy(hw);
}

static void
ixl_enable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
	ixl_flush(hw);
	return;
}

static void
ixl_disable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

	return;
}

static void
ixl_enable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTLN_INTENA_MASK |
	    I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);
}

static void
ixl_disable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);

	return;
}

static void
ixl_enable_legacy(struct i40e_hw *hw)
{
	u32		reg;
	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
}

static void
ixl_disable_legacy(struct i40e_hw *hw)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

	return;
}

static void
ixl_update_stats_counters(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ixl_vf	*vf;

	struct i40e_hw_port_stats *nsd = &pf->stats;
	struct i40e_hw_port_stats *osd = &pf->stats_offsets;

	/* Update hw stats */
	ixl_stat_update32(hw, I40E_GLPRT_CRCERRS(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->crc_errors, &nsd->crc_errors);
	ixl_stat_update32(hw, I40E_GLPRT_ILLERRC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->illegal_bytes, &nsd->illegal_bytes);
	ixl_stat_update48(hw, I40E_GLPRT_GORCH(hw->port),
			   I40E_GLPRT_GORCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_bytes, &nsd->eth.rx_bytes);
	ixl_stat_update48(hw, I40E_GLPRT_GOTCH(hw->port),
			   I40E_GLPRT_GOTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_bytes, &nsd->eth.tx_bytes);
	ixl_stat_update32(hw, I40E_GLPRT_RDPC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_discards,
			   &nsd->eth.rx_discards);
	ixl_stat_update48(hw, I40E_GLPRT_UPRCH(hw->port),
			   I40E_GLPRT_UPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_unicast,
			   &nsd->eth.rx_unicast);
	ixl_stat_update48(hw, I40E_GLPRT_UPTCH(hw->port),
			   I40E_GLPRT_UPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_unicast,
			   &nsd->eth.tx_unicast);
	ixl_stat_update48(hw, I40E_GLPRT_MPRCH(hw->port),
			   I40E_GLPRT_MPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_multicast,
			   &nsd->eth.rx_multicast);
	ixl_stat_update48(hw, I40E_GLPRT_MPTCH(hw->port),
			   I40E_GLPRT_MPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_multicast,
			   &nsd->eth.tx_multicast);
	ixl_stat_update48(hw, I40E_GLPRT_BPRCH(hw->port),
			   I40E_GLPRT_BPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_broadcast,
			   &nsd->eth.rx_broadcast);
	ixl_stat_update48(hw, I40E_GLPRT_BPTCH(hw->port),
			   I40E_GLPRT_BPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_broadcast,
			   &nsd->eth.tx_broadcast);

	ixl_stat_update32(hw, I40E_GLPRT_TDOLD(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_dropped_link_down,
			   &nsd->tx_dropped_link_down);
	ixl_stat_update32(hw, I40E_GLPRT_MLFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_local_faults,
			   &nsd->mac_local_faults);
	ixl_stat_update32(hw, I40E_GLPRT_MRFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_remote_faults,
			   &nsd->mac_remote_faults);
	ixl_stat_update32(hw, I40E_GLPRT_RLEC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_length_errors,
			   &nsd->rx_length_errors);

	/* Flow control (LFC) stats */
	ixl_stat_update32(hw, I40E_GLPRT_LXONRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_rx, &nsd->link_xon_rx);
	ixl_stat_update32(hw, I40E_GLPRT_LXONTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_tx, &nsd->link_xon_tx);
	ixl_stat_update32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_rx, &nsd->link_xoff_rx);
	ixl_stat_update32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_tx, &nsd->link_xoff_tx);

	/* Packet size stats rx */
	ixl_stat_update48(hw, I40E_GLPRT_PRC64H(hw->port),
			   I40E_GLPRT_PRC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_64, &nsd->rx_size_64);
	ixl_stat_update48(hw, I40E_GLPRT_PRC127H(hw->port),
			   I40E_GLPRT_PRC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_127, &nsd->rx_size_127);
	ixl_stat_update48(hw, I40E_GLPRT_PRC255H(hw->port),
			   I40E_GLPRT_PRC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_255, &nsd->rx_size_255);
	ixl_stat_update48(hw, I40E_GLPRT_PRC511H(hw->port),
			   I40E_GLPRT_PRC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_511, &nsd->rx_size_511);
	ixl_stat_update48(hw, I40E_GLPRT_PRC1023H(hw->port),
			   I40E_GLPRT_PRC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1023, &nsd->rx_size_1023);
	ixl_stat_update48(hw, I40E_GLPRT_PRC1522H(hw->port),
			   I40E_GLPRT_PRC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1522, &nsd->rx_size_1522);
	ixl_stat_update48(hw, I40E_GLPRT_PRC9522H(hw->port),
			   I40E_GLPRT_PRC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_big, &nsd->rx_size_big);

	/* Packet size stats tx */
	ixl_stat_update48(hw, I40E_GLPRT_PTC64H(hw->port),
			   I40E_GLPRT_PTC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_64, &nsd->tx_size_64);
	ixl_stat_update48(hw, I40E_GLPRT_PTC127H(hw->port),
			   I40E_GLPRT_PTC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_127, &nsd->tx_size_127);
	ixl_stat_update48(hw, I40E_GLPRT_PTC255H(hw->port),
			   I40E_GLPRT_PTC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_255, &nsd->tx_size_255);
	ixl_stat_update48(hw, I40E_GLPRT_PTC511H(hw->port),
			   I40E_GLPRT_PTC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_511, &nsd->tx_size_511);
	ixl_stat_update48(hw, I40E_GLPRT_PTC1023H(hw->port),
			   I40E_GLPRT_PTC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1023, &nsd->tx_size_1023);
	ixl_stat_update48(hw, I40E_GLPRT_PTC1522H(hw->port),
			   I40E_GLPRT_PTC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1522, &nsd->tx_size_1522);
	ixl_stat_update48(hw, I40E_GLPRT_PTC9522H(hw->port),
			   I40E_GLPRT_PTC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_big, &nsd->tx_size_big);

	ixl_stat_update32(hw, I40E_GLPRT_RUC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_undersize, &nsd->rx_undersize);
	ixl_stat_update32(hw, I40E_GLPRT_RFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_fragments, &nsd->rx_fragments);
	ixl_stat_update32(hw, I40E_GLPRT_ROC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_oversize, &nsd->rx_oversize);
	ixl_stat_update32(hw, I40E_GLPRT_RJC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_jabber, &nsd->rx_jabber);
	pf->stat_offsets_loaded = true;
	/* End hw stats */

	/* Update vsi stats */
	ixl_update_vsi_stats(vsi);

	for (int i = 0; i < pf->num_vfs; i++) {
		vf = &pf->vfs[i];
		if (vf->vf_flags & VF_FLAG_ENABLED)
			ixl_update_eth_stats(&pf->vfs[i].vsi);
	}
}

/*
** Tasklet handler for MSIX Adminq interrupts
**  - do outside interrupt since it might sleep
*/
static void
ixl_do_adminq(void *context, int pending)
{
	struct ixl_pf			*pf = context;
	struct i40e_hw			*hw = &pf->hw;
	struct ixl_vsi			*vsi = &pf->vsi;
	struct i40e_arq_event_info	event;
	i40e_status			ret;
	u32				reg, loop = 0;
	u16				opcode, result;

	event.buf_len = IXL_AQ_BUF_SZ;
	event.msg_buf = malloc(event.buf_len,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!event.msg_buf) {
		printf("Unable to allocate adminq memory\n");
		return;
	}

	IXL_PF_LOCK(pf);
	/* clean and process any events */
	do {
		ret = i40e_clean_arq_element(hw, &event, &result);
		if (ret)
			break;
		opcode = LE16_TO_CPU(event.desc.opcode);
		switch (opcode) {
		case i40e_aqc_opc_get_link_status:
			ixl_link_event(pf, &event);
			ixl_update_link_status(pf);
			break;
		case i40e_aqc_opc_send_msg_to_pf:
#ifdef PCI_IOV
			ixl_handle_vf_msg(pf, &event);
#endif
			break;
		case i40e_aqc_opc_event_lan_overflow:
			break;
		default:
#ifdef IXL_DEBUG
			printf("AdminQ unknown event %x\n", opcode);
#endif
			break;
		}

	} while (result && (loop++ < IXL_ADM_LIMIT));

	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	free(event.msg_buf, M_DEVBUF);

	/*
	 * If there are still messages to process, reschedule ourselves.
	 * Otherwise, re-enable our interrupt and go to sleep.
	 */
	if (result > 0)
		taskqueue_enqueue(pf->tq, &pf->adminq);
	else
		ixl_enable_intr(vsi);

	IXL_PF_UNLOCK(pf);
}

static int
ixl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf	*pf;
	int		error, input = 0;

	error = sysctl_handle_int(oidp, &input, 0, req);

	if (error || !req->newptr)
		return (error);

	if (input == 1) {
		pf = (struct ixl_pf *)arg1;
		ixl_print_debug_info(pf);
	}

	return (error);
}

static void
ixl_print_debug_info(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	struct rx_ring		*rxr = &que->rxr;
	struct tx_ring		*txr = &que->txr;
	u32			reg;	


	printf("Queue irqs = %jx\n", (uintmax_t)que->irqs);
	printf("AdminQ irqs = %jx\n", (uintmax_t)pf->admin_irq);
	printf("RX next check = %x\n", rxr->next_check);
	printf("RX not ready = %jx\n", (uintmax_t)rxr->not_done);
	printf("RX packets = %jx\n", (uintmax_t)rxr->rx_packets);
	printf("TX desc avail = %x\n", txr->avail);

	reg = rd32(hw, I40E_GLV_GORCL(0xc));
	 printf("RX Bytes = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_GORCL(hw->port));
	 printf("Port RX Bytes = %x\n", reg);
	reg = rd32(hw, I40E_GLV_RDPC(0xc));
	 printf("RX discard = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_RDPC(hw->port));
	 printf("Port RX discard = %x\n", reg);

	reg = rd32(hw, I40E_GLV_TEPC(0xc));
	 printf("TX errors = %x\n", reg);
	reg = rd32(hw, I40E_GLV_GOTCL(0xc));
	 printf("TX Bytes = %x\n", reg);

	reg = rd32(hw, I40E_GLPRT_RUC(hw->port));
	 printf("RX undersize = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_RFC(hw->port));
	 printf("RX fragments = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_ROC(hw->port));
	 printf("RX oversize = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_RLEC(hw->port));
	 printf("RX length error = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_MRFC(hw->port));
	 printf("mac remote fault = %x\n", reg);
	reg = rd32(hw, I40E_GLPRT_MLFC(hw->port));
	 printf("mac local fault = %x\n", reg);
}

/**
 * Update VSI-specific ethernet statistics counters.
 **/
void ixl_update_eth_stats(struct ixl_vsi *vsi)
{
	struct ixl_pf *pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_eth_stats *es;
	struct i40e_eth_stats *oes;
	struct i40e_hw_port_stats *nsd;
	u16 stat_idx = vsi->info.stat_counter_idx;

	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;
	nsd = &pf->stats;

	/* Gather up the stats that the hw collects */
	ixl_stat_update32(hw, I40E_GLV_TEPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_errors, &es->tx_errors);
	ixl_stat_update32(hw, I40E_GLV_RDPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_discards, &es->rx_discards);

	ixl_stat_update48(hw, I40E_GLV_GORCH(stat_idx),
			   I40E_GLV_GORCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_bytes, &es->rx_bytes);
	ixl_stat_update48(hw, I40E_GLV_UPRCH(stat_idx),
			   I40E_GLV_UPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_unicast, &es->rx_unicast);
	ixl_stat_update48(hw, I40E_GLV_MPRCH(stat_idx),
			   I40E_GLV_MPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_multicast, &es->rx_multicast);
	ixl_stat_update48(hw, I40E_GLV_BPRCH(stat_idx),
			   I40E_GLV_BPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_broadcast, &es->rx_broadcast);

	ixl_stat_update48(hw, I40E_GLV_GOTCH(stat_idx),
			   I40E_GLV_GOTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_bytes, &es->tx_bytes);
	ixl_stat_update48(hw, I40E_GLV_UPTCH(stat_idx),
			   I40E_GLV_UPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_unicast, &es->tx_unicast);
	ixl_stat_update48(hw, I40E_GLV_MPTCH(stat_idx),
			   I40E_GLV_MPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_multicast, &es->tx_multicast);
	ixl_stat_update48(hw, I40E_GLV_BPTCH(stat_idx),
			   I40E_GLV_BPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_broadcast, &es->tx_broadcast);
	vsi->stat_offsets_loaded = true;
}

static void
ixl_update_vsi_stats(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf;
	struct ifnet		*ifp;
	struct i40e_eth_stats	*es;
	u64			tx_discards;

	struct i40e_hw_port_stats *nsd;

	pf = vsi->back;
	ifp = vsi->ifp;
	es = &vsi->eth_stats;
	nsd = &pf->stats;

	ixl_update_eth_stats(vsi);

	tx_discards = es->tx_discards + nsd->tx_dropped_link_down;
	for (int i = 0; i < vsi->num_queues; i++)
		tx_discards += vsi->queues[i].txr.br->br_drops;

	/* Update ifnet stats */
	IXL_SET_IPACKETS(vsi, es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast);
	IXL_SET_OPACKETS(vsi, es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast);
	IXL_SET_IBYTES(vsi, es->rx_bytes);
	IXL_SET_OBYTES(vsi, es->tx_bytes);
	IXL_SET_IMCASTS(vsi, es->rx_multicast);
	IXL_SET_OMCASTS(vsi, es->tx_multicast);

	IXL_SET_IERRORS(vsi, nsd->crc_errors + nsd->illegal_bytes +
	    nsd->rx_undersize + nsd->rx_oversize + nsd->rx_fragments +
	    nsd->rx_jabber);
	IXL_SET_OERRORS(vsi, es->tx_errors);
	IXL_SET_IQDROPS(vsi, es->rx_discards + nsd->eth.rx_discards);
	IXL_SET_OQDROPS(vsi, tx_discards);
	IXL_SET_NOPROTO(vsi, es->rx_unknown_protocol);
	IXL_SET_COLLISIONS(vsi, 0);
}

/**
 * Reset all of the stats for the given pf
 **/
void ixl_pf_reset_stats(struct ixl_pf *pf)
{
	bzero(&pf->stats, sizeof(struct i40e_hw_port_stats));
	bzero(&pf->stats_offsets, sizeof(struct i40e_hw_port_stats));
	pf->stat_offsets_loaded = false;
}

/**
 * Resets all stats of the given vsi
 **/
void ixl_vsi_reset_stats(struct ixl_vsi *vsi)
{
	bzero(&vsi->eth_stats, sizeof(struct i40e_eth_stats));
	bzero(&vsi->eth_stats_offsets, sizeof(struct i40e_eth_stats));
	vsi->stat_offsets_loaded = false;
}

/**
 * Read and update a 48 bit stat from the hw
 *
 * Since the device stats are not reset at PFReset, they likely will not
 * be zeroed when the driver starts.  We'll save the first values read
 * and use them as offsets to be subtracted from the raw values in order
 * to report stats that count from zero.
 **/
static void
ixl_stat_update48(struct i40e_hw *hw, u32 hireg, u32 loreg,
	bool offset_loaded, u64 *offset, u64 *stat)
{
	u64 new_data;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 1000000) && defined(__amd64__)
	new_data = rd64(hw, loreg);
#else
	/*
	 * Use two rd32's instead of one rd64; FreeBSD versions before
	 * 10 don't support 8 byte bus reads/writes.
	 */
	new_data = rd32(hw, loreg);
	new_data |= ((u64)(rd32(hw, hireg) & 0xFFFF)) << 32;
#endif

	if (!offset_loaded)
		*offset = new_data;
	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (new_data + ((u64)1 << 48)) - *offset;
	*stat &= 0xFFFFFFFFFFFFULL;
}

/**
 * Read and update a 32 bit stat from the hw
 **/
static void
ixl_stat_update32(struct i40e_hw *hw, u32 reg,
	bool offset_loaded, u64 *offset, u64 *stat)
{
	u32 new_data;

	new_data = rd32(hw, reg);
	if (!offset_loaded)
		*offset = new_data;
	if (new_data >= *offset)
		*stat = (u32)(new_data - *offset);
	else
		*stat = (u32)((new_data + ((u64)1 << 32)) - *offset);
}

/*
** Set flow control using sysctl:
** 	0 - off
**	1 - rx pause
**	2 - tx pause
**	3 - full
*/
static int
ixl_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	/*
	 * TODO: ensure flow control is disabled if
	 * priority flow control is enabled
	 *
	 * TODO: ensure tx CRC by hardware should be enabled
	 * if tx flow control is enabled.
	 */
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int error = 0;
	enum i40e_status_code aq_error = 0;
	u8 fc_aq_err = 0;

	/* Get request */
	error = sysctl_handle_int(oidp, &pf->fc, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (pf->fc < 0 || pf->fc > 3) {
		device_printf(dev,
		    "Invalid fc mode; valid modes are 0 through 3\n");
		return (EINVAL);
	}

	/*
	** Changing flow control mode currently does not work on
	** 40GBASE-CR4 PHYs
	*/
	if (hw->phy.link_info.phy_type == I40E_PHY_TYPE_40GBASE_CR4
	    || hw->phy.link_info.phy_type == I40E_PHY_TYPE_40GBASE_CR4_CU) {
		device_printf(dev, "Changing flow control mode unsupported"
		    " on 40GBase-CR4 media.\n");
		return (ENODEV);
	}

	/* Set fc ability for port */
	hw->fc.requested_mode = pf->fc;
	aq_error = i40e_set_fc(hw, &fc_aq_err, TRUE);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new fc mode %d; fc_err %#x\n",
		    __func__, aq_error, fc_aq_err);
		return (EAGAIN);
	}

	return (0);
}

static int
ixl_current_speed(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	int error = 0, index = 0;

	char *speeds[] = {
		"Unknown",
		"100M",
		"1G",
		"10G",
		"40G",
		"20G"
	};

	ixl_update_link_status(pf);

	switch (hw->phy.link_info.link_speed) {
	case I40E_LINK_SPEED_100MB:
		index = 1;
		break;
	case I40E_LINK_SPEED_1GB:
		index = 2;
		break;
	case I40E_LINK_SPEED_10GB:
		index = 3;
		break;
	case I40E_LINK_SPEED_40GB:
		index = 4;
		break;
	case I40E_LINK_SPEED_20GB:
		index = 5;
		break;
	case I40E_LINK_SPEED_UNKNOWN:
	default:
		index = 0;
		break;
	}

	error = sysctl_handle_string(oidp, speeds[index],
	    strlen(speeds[index]), req);
	return (error);
}

static int
ixl_set_advertised_speeds(struct ixl_pf *pf, int speeds)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct i40e_aq_get_phy_abilities_resp abilities;
	struct i40e_aq_set_phy_config config;
	enum i40e_status_code aq_error = 0;

	/* Get current capability information */
	aq_error = i40e_aq_get_phy_capabilities(hw,
	    FALSE, FALSE, &abilities, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error getting phy capabilities %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EAGAIN);
	}

	/* Prepare new config */
	bzero(&config, sizeof(config));
	config.phy_type = abilities.phy_type;
	config.abilities = abilities.abilities
	    | I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	config.eee_capability = abilities.eee_capability;
	config.eeer = abilities.eeer_val;
	config.low_power_ctrl = abilities.d3_lpan;
	/* Translate into aq cmd link_speed */
	if (speeds & 0x8)
		config.link_speed |= I40E_LINK_SPEED_20GB;
	if (speeds & 0x4)
		config.link_speed |= I40E_LINK_SPEED_10GB;
	if (speeds & 0x2)
		config.link_speed |= I40E_LINK_SPEED_1GB;
	if (speeds & 0x1)
		config.link_speed |= I40E_LINK_SPEED_100MB;

	/* Do aq command & restart link */
	aq_error = i40e_aq_set_phy_config(hw, &config, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new phy config %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EAGAIN);
	}

	/*
	** This seems a bit heavy handed, but we
	** need to get a reinit on some devices
	*/
	IXL_PF_LOCK(pf);
	ixl_stop(pf);
	ixl_init_locked(pf);
	IXL_PF_UNLOCK(pf);

	return (0);
}

/*
** Control link advertise speed:
**	Flags:
**	0x1 - advertise 100 Mb
**	0x2 - advertise 1G
**	0x4 - advertise 10G
**	0x8 - advertise 20G
**
** Does not work on 40G devices.
*/
static int
ixl_set_advertise(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int requested_ls = 0;
	int error = 0;

	/*
	** FW doesn't support changing advertised speed
	** for 40G devices; speed is always 40G.
	*/
	if (i40e_is_40G_device(hw->device_id))
		return (ENODEV);

	/* Read in new mode */
	requested_ls = pf->advertised_speed;
	error = sysctl_handle_int(oidp, &requested_ls, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	/* Check for sane value */
	if (requested_ls < 0x1 || requested_ls > 0xE) {
		device_printf(dev, "Invalid advertised speed; "
		    "valid modes are 0x1 through 0xE\n");
		return (EINVAL);
	}
	/* Then check for validity based on adapter type */
	switch (hw->device_id) {
	case I40E_DEV_ID_10G_BASE_T:
		if (requested_ls & 0x8) {
			device_printf(dev,
			    "20Gbs speed not supported on this device.\n");
			return (EINVAL);
		}
		break;
	case I40E_DEV_ID_20G_KR2:
		if (requested_ls & 0x1) {
			device_printf(dev,
			    "100Mbs speed not supported on this device.\n");
			return (EINVAL);
		}
		break;
	default:
		if (requested_ls & ~0x6) {
			device_printf(dev,
			    "Only 1/10Gbs speeds are supported on this device.\n");
			return (EINVAL);
		}
		break;
	}

	/* Exit if no change */
	if (pf->advertised_speed == requested_ls)
		return (0);

	error = ixl_set_advertised_speeds(pf, requested_ls);
	if (error)
		return (error);

	pf->advertised_speed = requested_ls;
	ixl_update_link_status(pf);
	return (0);
}

/*
** Get the width and transaction speed of
** the bus this adapter is plugged into.
*/
static u16
ixl_get_bus_info(struct i40e_hw *hw, device_t dev)
{
        u16                     link;
        u32                     offset;
                
                
        /* Get the PCI Express Capabilities offset */
        pci_find_cap(dev, PCIY_EXPRESS, &offset);

        /* ...and read the Link Status Register */
        link = pci_read_config(dev, offset + PCIER_LINK_STA, 2);

        switch (link & I40E_PCI_LINK_WIDTH) {
        case I40E_PCI_LINK_WIDTH_1:
                hw->bus.width = i40e_bus_width_pcie_x1;
                break;
        case I40E_PCI_LINK_WIDTH_2:
                hw->bus.width = i40e_bus_width_pcie_x2;
                break;
        case I40E_PCI_LINK_WIDTH_4:
                hw->bus.width = i40e_bus_width_pcie_x4;
                break;
        case I40E_PCI_LINK_WIDTH_8:
                hw->bus.width = i40e_bus_width_pcie_x8;
                break;
        default:
                hw->bus.width = i40e_bus_width_unknown;
                break;
        }

        switch (link & I40E_PCI_LINK_SPEED) {
        case I40E_PCI_LINK_SPEED_2500:
                hw->bus.speed = i40e_bus_speed_2500;
                break;
        case I40E_PCI_LINK_SPEED_5000:
                hw->bus.speed = i40e_bus_speed_5000;
                break;
        case I40E_PCI_LINK_SPEED_8000:
                hw->bus.speed = i40e_bus_speed_8000;
                break;
        default:
                hw->bus.speed = i40e_bus_speed_unknown;
                break;
        }


        device_printf(dev,"PCI Express Bus: Speed %s %s\n",
            ((hw->bus.speed == i40e_bus_speed_8000) ? "8.0GT/s":
            (hw->bus.speed == i40e_bus_speed_5000) ? "5.0GT/s":
            (hw->bus.speed == i40e_bus_speed_2500) ? "2.5GT/s":"Unknown"),
            (hw->bus.width == i40e_bus_width_pcie_x8) ? "Width x8" :
            (hw->bus.width == i40e_bus_width_pcie_x4) ? "Width x4" :
            (hw->bus.width == i40e_bus_width_pcie_x1) ? "Width x1" :
            ("Unknown"));

        if ((hw->bus.width <= i40e_bus_width_pcie_x8) &&
            (hw->bus.speed < i40e_bus_speed_8000)) {
                device_printf(dev, "PCI-Express bandwidth available"
                    " for this device\n     may be insufficient for"
                    " optimal performance.\n");
                device_printf(dev, "For expected performance a x8 "
                    "PCIE Gen3 slot is required.\n");
        }

        return (link);
}

static int
ixl_sysctl_show_fw(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf	*pf = (struct ixl_pf *)arg1;
	struct i40e_hw	*hw = &pf->hw;
	char		buf[32];

	snprintf(buf, sizeof(buf),
	    "f%d.%d a%d.%d n%02x.%02x e%08x",
	    hw->aq.fw_maj_ver, hw->aq.fw_min_ver,
	    hw->aq.api_maj_ver, hw->aq.api_min_ver,
	    (hw->nvm.version & IXL_NVM_VERSION_HI_MASK) >>
	    IXL_NVM_VERSION_HI_SHIFT,
	    (hw->nvm.version & IXL_NVM_VERSION_LO_MASK) >>
	    IXL_NVM_VERSION_LO_SHIFT,
	    hw->nvm.eetrack);
	return (sysctl_handle_string(oidp, buf, strlen(buf), req));
}


#ifdef IXL_DEBUG_SYSCTL
static int
ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_link_status link_status;
	char buf[512];

	enum i40e_status_code aq_error = 0;

	aq_error = i40e_aq_get_link_info(hw, TRUE, &link_status, NULL);
	if (aq_error) {
		printf("i40e_aq_get_link_info() error %d\n", aq_error);
		return (EPERM);
	}

	sprintf(buf, "\n"
	    "PHY Type : %#04x\n"
	    "Speed    : %#04x\n" 
	    "Link info: %#04x\n" 
	    "AN info  : %#04x\n" 
	    "Ext info : %#04x", 
	    link_status.phy_type, link_status.link_speed, 
	    link_status.link_info, link_status.an_info,
	    link_status.ext_info);

	return (sysctl_handle_string(oidp, buf, strlen(buf), req));
}

static int
ixl_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf		*pf = (struct ixl_pf *)arg1;
	struct i40e_hw		*hw = &pf->hw;
	char			buf[512];
	enum i40e_status_code	aq_error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;

	aq_error = i40e_aq_get_phy_capabilities(hw,
	    TRUE, FALSE, &abilities, NULL);
	if (aq_error) {
		printf("i40e_aq_get_phy_capabilities() error %d\n", aq_error);
		return (EPERM);
	}

	sprintf(buf, "\n"
	    "PHY Type : %#010x\n"
	    "Speed    : %#04x\n" 
	    "Abilities: %#04x\n" 
	    "EEE cap  : %#06x\n" 
	    "EEER reg : %#010x\n" 
	    "D3 Lpan  : %#04x",
	    abilities.phy_type, abilities.link_speed, 
	    abilities.abilities, abilities.eee_capability,
	    abilities.eeer_val, abilities.d3_lpan);

	return (sysctl_handle_string(oidp, buf, strlen(buf), req));
}

static int
ixl_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct ixl_vsi *vsi = &pf->vsi;
	struct ixl_mac_filter *f;
	char *buf, *buf_i;

	int error = 0;
	int ftl_len = 0;
	int ftl_counter = 0;
	int buf_len = 0;
	int entry_len = 42;

	SLIST_FOREACH(f, &vsi->ftl, next) {
		ftl_len++;
	}

	if (ftl_len < 1) {
		sysctl_handle_string(oidp, "(none)", 6, req);
		return (0);
	}

	buf_len = sizeof(char) * (entry_len + 1) * ftl_len + 2;
	buf = buf_i = malloc(buf_len, M_DEVBUF, M_NOWAIT);

	sprintf(buf_i++, "\n");
	SLIST_FOREACH(f, &vsi->ftl, next) {
		sprintf(buf_i,
		    MAC_FORMAT ", vlan %4d, flags %#06x",
		    MAC_FORMAT_ARGS(f->macaddr), f->vlan, f->flags);
		buf_i += entry_len;
		/* don't print '\n' for last entry */
		if (++ftl_counter != ftl_len) {
			sprintf(buf_i, "\n");
			buf_i++;
		}
	}

	error = sysctl_handle_string(oidp, buf, strlen(buf), req);
	if (error)
		printf("sysctl error: %d\n", error);
	free(buf, M_DEVBUF);
	return error;
}

#define IXL_SW_RES_SIZE 0x14
static int
ixl_res_alloc_cmp(const void *a, const void *b)
{
	const struct i40e_aqc_switch_resource_alloc_element_resp *one, *two;
	one = (const struct i40e_aqc_switch_resource_alloc_element_resp *)a;
	two = (const struct i40e_aqc_switch_resource_alloc_element_resp *)b;

	return ((int)one->resource_type - (int)two->resource_type);
}

static int
ixl_sysctl_hw_res_alloc(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;

	u8 num_entries;
	struct i40e_aqc_switch_resource_alloc_element_resp resp[IXL_SW_RES_SIZE];

	buf = sbuf_new_for_sysctl(NULL, NULL, 0, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	bzero(resp, sizeof(resp));
	error = i40e_aq_get_switch_resource_alloc(hw, &num_entries,
				resp,
				IXL_SW_RES_SIZE,
				NULL);
	if (error) {
		device_printf(dev,
		    "%s: get_switch_resource_alloc() error %d, aq error %d\n",
		    __func__, error, hw->aq.asq_last_status);
		sbuf_delete(buf);
		return error;
	}

	/* Sort entries by type for display */
	qsort(resp, num_entries,
	    sizeof(struct i40e_aqc_switch_resource_alloc_element_resp),
	    &ixl_res_alloc_cmp);

	sbuf_cat(buf, "\n");
	sbuf_printf(buf, "# of entries: %d\n", num_entries);
	sbuf_printf(buf,
	    "Type | Guaranteed | Total | Used   | Un-allocated\n"
	    "     | (this)     | (all) | (this) | (all)       \n");
	for (int i = 0; i < num_entries; i++) {
		sbuf_printf(buf,
		    "%#4x | %10d   %5d   %6d   %12d",
		    resp[i].resource_type,
		    resp[i].guaranteed,
		    resp[i].total,
		    resp[i].used,
		    resp[i].total_unalloced);
		if (i < num_entries - 1)
			sbuf_cat(buf, "\n");
	}

	error = sbuf_finish(buf);
	if (error) {
		device_printf(dev, "Error finishing sbuf: %d\n", error);
		sbuf_delete(buf);
		return error;
	}

	error = sysctl_handle_string(oidp, sbuf_data(buf), sbuf_len(buf), req);
	if (error)
		device_printf(dev, "sysctl error: %d\n", error);
	sbuf_delete(buf);
	return error;
}

/*
** Caller must init and delete sbuf; this function will clear and
** finish it for caller.
*/
static char *
ixl_switch_element_string(struct sbuf *s, u16 seid, bool uplink)
{
	sbuf_clear(s);

	if (seid == 0 && uplink)
		sbuf_cat(s, "Network");
	else if (seid == 0)
		sbuf_cat(s, "Host");
	else if (seid == 1)
		sbuf_cat(s, "EMP");
	else if (seid <= 5)
		sbuf_printf(s, "MAC %d", seid - 2);
	else if (seid <= 15)
		sbuf_cat(s, "Reserved");
	else if (seid <= 31)
		sbuf_printf(s, "PF %d", seid - 16);
	else if (seid <= 159)
		sbuf_printf(s, "VF %d", seid - 32);
	else if (seid <= 287)
		sbuf_cat(s, "Reserved");
	else if (seid <= 511)
		sbuf_cat(s, "Other"); // for other structures
	else if (seid <= 895)
		sbuf_printf(s, "VSI %d", seid - 512);
	else if (seid <= 1023)
		sbuf_printf(s, "Reserved");
	else
		sbuf_cat(s, "Invalid");

	sbuf_finish(s);
	return sbuf_data(s);
}

static int
ixl_sysctl_switch_config(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	struct sbuf *nmbuf;
	int error = 0;
	u8 aq_buf[I40E_AQ_LARGE_BUF];

	u16 next = 0;
	struct i40e_aqc_get_switch_config_resp *sw_config;
	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;

	buf = sbuf_new_for_sysctl(NULL, NULL, 0, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	error = i40e_aq_get_switch_config(hw, sw_config,
	    sizeof(aq_buf), &next, NULL);
	if (error) {
		device_printf(dev,
		    "%s: aq_get_switch_config() error %d, aq error %d\n",
		    __func__, error, hw->aq.asq_last_status);
		sbuf_delete(buf);
		return error;
	}

	nmbuf = sbuf_new_auto();
	if (!nmbuf) {
		device_printf(dev, "Could not allocate sbuf for name output.\n");
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	// Assuming <= 255 elements in switch
	sbuf_printf(buf, "# of elements: %d\n", sw_config->header.num_reported);
	/* Exclude:
	** Revision -- all elements are revision 1 for now
	*/
	sbuf_printf(buf,
	    "SEID (  Name  ) |  Uplink  | Downlink | Conn Type\n"
	    "                |          |          | (uplink)\n");
	for (int i = 0; i < sw_config->header.num_reported; i++) {
		// "%4d (%8s) | %8s   %8s   %#8x",
		sbuf_printf(buf, "%4d", sw_config->element[i].seid);
		sbuf_cat(buf, " ");
		sbuf_printf(buf, "(%8s)", ixl_switch_element_string(nmbuf,
		    sw_config->element[i].seid, false));
		sbuf_cat(buf, " | ");
		sbuf_printf(buf, "%8s", ixl_switch_element_string(nmbuf,
		    sw_config->element[i].uplink_seid, true));
		sbuf_cat(buf, "   ");
		sbuf_printf(buf, "%8s", ixl_switch_element_string(nmbuf,
		    sw_config->element[i].downlink_seid, false));
		sbuf_cat(buf, "   ");
		sbuf_printf(buf, "%#8x", sw_config->element[i].connection_type);
		if (i < sw_config->header.num_reported - 1)
			sbuf_cat(buf, "\n");
	}
	sbuf_delete(nmbuf);

	error = sbuf_finish(buf);
	if (error) {
		device_printf(dev, "Error finishing sbuf: %d\n", error);
		sbuf_delete(buf);
		return error;
	}

	error = sysctl_handle_string(oidp, sbuf_data(buf), sbuf_len(buf), req);
	if (error)
		device_printf(dev, "sysctl error: %d\n", error);
	sbuf_delete(buf);

	return (error);
}
#endif /* IXL_DEBUG_SYSCTL */


#ifdef PCI_IOV
static int
ixl_vf_alloc_vsi(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	struct ixl_vsi *vsi;
	struct i40e_vsi_context vsi_ctx;
	int i;
	uint16_t first_queue;
	enum i40e_status_code code;

	hw = &pf->hw;
	vsi = &pf->vsi;

	vsi_ctx.pf_num = hw->pf_id;
	vsi_ctx.uplink_seid = pf->veb_seid;
	vsi_ctx.connection_type = IXL_VSI_DATA_PORT;
	vsi_ctx.vf_num = hw->func_caps.vf_base_id + vf->vf_num;
	vsi_ctx.flags = I40E_AQ_VSI_TYPE_VF;

	bzero(&vsi_ctx.info, sizeof(vsi_ctx.info));

	vsi_ctx.info.valid_sections = htole16(I40E_AQ_VSI_PROP_SWITCH_VALID);
	vsi_ctx.info.switch_id = htole16(0);

	vsi_ctx.info.valid_sections |= htole16(I40E_AQ_VSI_PROP_SECURITY_VALID);
	vsi_ctx.info.sec_flags = 0;
	if (vf->vf_flags & VF_FLAG_MAC_ANTI_SPOOF)
		vsi_ctx.info.sec_flags |= I40E_AQ_VSI_SEC_FLAG_ENABLE_MAC_CHK;

	vsi_ctx.info.valid_sections |= htole16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi_ctx.info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
	    I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	vsi_ctx.info.valid_sections |=
	    htole16(I40E_AQ_VSI_PROP_QUEUE_MAP_VALID);
	vsi_ctx.info.mapping_flags = htole16(I40E_AQ_VSI_QUE_MAP_NONCONTIG);
	first_queue = vsi->num_queues + vf->vf_num * IXLV_MAX_QUEUES;
	for (i = 0; i < IXLV_MAX_QUEUES; i++)
		vsi_ctx.info.queue_mapping[i] = htole16(first_queue + i);
	for (; i < nitems(vsi_ctx.info.queue_mapping); i++)
		vsi_ctx.info.queue_mapping[i] = htole16(I40E_AQ_VSI_QUEUE_MASK);

	vsi_ctx.info.tc_mapping[0] = htole16(
	    (0 << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT) |
	    (1 << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT));

	code = i40e_aq_add_vsi(hw, &vsi_ctx, NULL);
	if (code != I40E_SUCCESS)
		return (ixl_adminq_err_to_errno(hw->aq.asq_last_status));
	vf->vsi.seid = vsi_ctx.seid;
	vf->vsi.vsi_num = vsi_ctx.vsi_number;
	vf->vsi.first_queue = first_queue;
	vf->vsi.num_queues = IXLV_MAX_QUEUES;

	code = i40e_aq_get_vsi_params(hw, &vsi_ctx, NULL);
	if (code != I40E_SUCCESS)
		return (ixl_adminq_err_to_errno(hw->aq.asq_last_status));

	code = i40e_aq_config_vsi_bw_limit(hw, vf->vsi.seid, 0, 0, NULL);
	if (code != I40E_SUCCESS) {
		device_printf(pf->dev, "Failed to disable BW limit: %d\n",
		    ixl_adminq_err_to_errno(hw->aq.asq_last_status));
		return (ixl_adminq_err_to_errno(hw->aq.asq_last_status));
	}

	memcpy(&vf->vsi.info, &vsi_ctx.info, sizeof(vf->vsi.info));
	return (0);
}

static int
ixl_vf_setup_vsi(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	int error;

	hw = &pf->hw;

	error = ixl_vf_alloc_vsi(pf, vf);
	if (error != 0)
		return (error);

	vf->vsi.hw_filters_add = 0;
	vf->vsi.hw_filters_del = 0;
	ixl_add_filter(&vf->vsi, ixl_bcast_addr, IXL_VLAN_ANY);
	ixl_reconfigure_filters(&vf->vsi);

	return (0);
}

static void
ixl_vf_map_vsi_queue(struct i40e_hw *hw, struct ixl_vf *vf, int qnum,
    uint32_t val)
{
	uint32_t qtable;
	int index, shift;

	/*
	 * Two queues are mapped in a single register, so we have to do some
	 * gymnastics to convert the queue number into a register index and
	 * shift.
	 */
	index = qnum / 2;
	shift = (qnum % 2) * I40E_VSILAN_QTABLE_QINDEX_1_SHIFT;

	qtable = rd32(hw, I40E_VSILAN_QTABLE(index, vf->vsi.vsi_num));
	qtable &= ~(I40E_VSILAN_QTABLE_QINDEX_0_MASK << shift);
	qtable |= val << shift;
	wr32(hw, I40E_VSILAN_QTABLE(index, vf->vsi.vsi_num), qtable);
}

static void
ixl_vf_map_queues(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	uint32_t qtable;
	int i;

	hw = &pf->hw;

	/*
	 * Contiguous mappings aren't actually supported by the hardware,
	 * so we have to use non-contiguous mappings.
	 */
	wr32(hw, I40E_VSILAN_QBASE(vf->vsi.vsi_num),
	     I40E_VSILAN_QBASE_VSIQTABLE_ENA_MASK);

	wr32(hw, I40E_VPLAN_MAPENA(vf->vf_num),
	    I40E_VPLAN_MAPENA_TXRX_ENA_MASK);

	for (i = 0; i < vf->vsi.num_queues; i++) {
		qtable = (vf->vsi.first_queue + i) <<
		    I40E_VPLAN_QTABLE_QINDEX_SHIFT;

		wr32(hw, I40E_VPLAN_QTABLE(i, vf->vf_num), qtable);
	}

	/* Map queues allocated to VF to its VSI. */
	for (i = 0; i < vf->vsi.num_queues; i++)
		ixl_vf_map_vsi_queue(hw, vf, i, vf->vsi.first_queue + i);

	/* Set rest of VSI queues as unused. */
	for (; i < IXL_MAX_VSI_QUEUES; i++)
		ixl_vf_map_vsi_queue(hw, vf, i,
		    I40E_VSILAN_QTABLE_QINDEX_0_MASK);

	ixl_flush(hw);
}

static void
ixl_vf_vsi_release(struct ixl_pf *pf, struct ixl_vsi *vsi)
{
	struct i40e_hw *hw;

	hw = &pf->hw;

	if (vsi->seid == 0)
		return;

	i40e_aq_delete_element(hw, vsi->seid, NULL);
}

static void
ixl_vf_disable_queue_intr(struct i40e_hw *hw, uint32_t vfint_reg)
{

	wr32(hw, vfint_reg, I40E_VFINT_DYN_CTLN_CLEARPBA_MASK);
	ixl_flush(hw);
}

static void
ixl_vf_unregister_intr(struct i40e_hw *hw, uint32_t vpint_reg)
{

	wr32(hw, vpint_reg, I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_MASK |
	    I40E_VPINT_LNKLSTN_FIRSTQ_INDX_MASK);
	ixl_flush(hw);
}

static void
ixl_vf_release_resources(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	uint32_t vfint_reg, vpint_reg;
	int i;

	hw = &pf->hw;

	ixl_vf_vsi_release(pf, &vf->vsi);

	/* Index 0 has a special register. */
	ixl_vf_disable_queue_intr(hw, I40E_VFINT_DYN_CTL0(vf->vf_num));

	for (i = 1; i < hw->func_caps.num_msix_vectors_vf; i++) {
		vfint_reg = IXL_VFINT_DYN_CTLN_REG(hw, i , vf->vf_num);
		ixl_vf_disable_queue_intr(hw, vfint_reg);
	}

	/* Index 0 has a special register. */
	ixl_vf_unregister_intr(hw, I40E_VPINT_LNKLST0(vf->vf_num));

	for (i = 1; i < hw->func_caps.num_msix_vectors_vf; i++) {
		vpint_reg = IXL_VPINT_LNKLSTN_REG(hw, i, vf->vf_num);
		ixl_vf_unregister_intr(hw, vpint_reg);
	}

	vf->vsi.num_queues = 0;
}

static int
ixl_flush_pcie(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	int i;
	uint16_t global_vf_num;
	uint32_t ciad;

	hw = &pf->hw;
	global_vf_num = hw->func_caps.vf_base_id + vf->vf_num;

	wr32(hw, I40E_PF_PCI_CIAA, IXL_PF_PCI_CIAA_VF_DEVICE_STATUS |
	     (global_vf_num << I40E_PF_PCI_CIAA_VF_NUM_SHIFT));
	for (i = 0; i < IXL_VF_RESET_TIMEOUT; i++) {
		ciad = rd32(hw, I40E_PF_PCI_CIAD);
		if ((ciad & IXL_PF_PCI_CIAD_VF_TRANS_PENDING_MASK) == 0)
			return (0);
		DELAY(1);
	}

	return (ETIMEDOUT);
}

static void
ixl_reset_vf(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	uint32_t vfrtrig;

	hw = &pf->hw;

	vfrtrig = rd32(hw, I40E_VPGEN_VFRTRIG(vf->vf_num));
	vfrtrig |= I40E_VPGEN_VFRTRIG_VFSWR_MASK;
	wr32(hw, I40E_VPGEN_VFRTRIG(vf->vf_num), vfrtrig);
	ixl_flush(hw);

	ixl_reinit_vf(pf, vf);
}

static void
ixl_reinit_vf(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_hw *hw;
	uint32_t vfrstat, vfrtrig;
	int i, error;

	hw = &pf->hw;

	error = ixl_flush_pcie(pf, vf);
	if (error != 0)
		device_printf(pf->dev,
		    "Timed out waiting for PCIe activity to stop on VF-%d\n",
		    vf->vf_num);

	for (i = 0; i < IXL_VF_RESET_TIMEOUT; i++) {
		DELAY(10);

		vfrstat = rd32(hw, I40E_VPGEN_VFRSTAT(vf->vf_num));
		if (vfrstat & I40E_VPGEN_VFRSTAT_VFRD_MASK)
			break;
	}

	if (i == IXL_VF_RESET_TIMEOUT)
		device_printf(pf->dev, "VF %d failed to reset\n", vf->vf_num);

	wr32(hw, I40E_VFGEN_RSTAT1(vf->vf_num), I40E_VFR_COMPLETED);

	vfrtrig = rd32(hw, I40E_VPGEN_VFRTRIG(vf->vf_num));
	vfrtrig &= ~I40E_VPGEN_VFRTRIG_VFSWR_MASK;
	wr32(hw, I40E_VPGEN_VFRTRIG(vf->vf_num), vfrtrig);

	if (vf->vsi.seid != 0)
		ixl_disable_rings(&vf->vsi);

	ixl_vf_release_resources(pf, vf);
	ixl_vf_setup_vsi(pf, vf);
	ixl_vf_map_queues(pf, vf);

	wr32(hw, I40E_VFGEN_RSTAT1(vf->vf_num), I40E_VFR_VFACTIVE);
	ixl_flush(hw);
}

static const char *
ixl_vc_opcode_str(uint16_t op)
{

	switch (op) {
	case I40E_VIRTCHNL_OP_VERSION:
		return ("VERSION");
	case I40E_VIRTCHNL_OP_RESET_VF:
		return ("RESET_VF");
	case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
		return ("GET_VF_RESOURCES");
	case I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE:
		return ("CONFIG_TX_QUEUE");
	case I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE:
		return ("CONFIG_RX_QUEUE");
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		return ("CONFIG_VSI_QUEUES");
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		return ("CONFIG_IRQ_MAP");
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
		return ("ENABLE_QUEUES");
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		return ("DISABLE_QUEUES");
	case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
		return ("ADD_ETHER_ADDRESS");
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		return ("DEL_ETHER_ADDRESS");
	case I40E_VIRTCHNL_OP_ADD_VLAN:
		return ("ADD_VLAN");
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		return ("DEL_VLAN");
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		return ("CONFIG_PROMISCUOUS_MODE");
	case I40E_VIRTCHNL_OP_GET_STATS:
		return ("GET_STATS");
	case I40E_VIRTCHNL_OP_FCOE:
		return ("FCOE");
	case I40E_VIRTCHNL_OP_EVENT:
		return ("EVENT");
	default:
		return ("UNKNOWN");
	}
}

static int
ixl_vc_opcode_level(uint16_t opcode)
{

	switch (opcode) {
	case I40E_VIRTCHNL_OP_GET_STATS:
		return (10);
	default:
		return (5);
	}
}

static void
ixl_send_vf_msg(struct ixl_pf *pf, struct ixl_vf *vf, uint16_t op,
    enum i40e_status_code status, void *msg, uint16_t len)
{
	struct i40e_hw *hw;
	int global_vf_id;

	hw = &pf->hw;
	global_vf_id = hw->func_caps.vf_base_id + vf->vf_num;

	I40E_VC_DEBUG(pf, ixl_vc_opcode_level(op),
	    "Sending msg (op=%s[%d], status=%d) to VF-%d\n",
	    ixl_vc_opcode_str(op), op, status, vf->vf_num);

	i40e_aq_send_msg_to_vf(hw, global_vf_id, op, status, msg, len, NULL);
}

static void
ixl_send_vf_ack(struct ixl_pf *pf, struct ixl_vf *vf, uint16_t op)
{

	ixl_send_vf_msg(pf, vf, op, I40E_SUCCESS, NULL, 0);
}

static void
ixl_send_vf_nack_msg(struct ixl_pf *pf, struct ixl_vf *vf, uint16_t op,
    enum i40e_status_code status, const char *file, int line)
{

	I40E_VC_DEBUG(pf, 1,
	    "Sending NACK (op=%s[%d], err=%d) to VF-%d from %s:%d\n",
	    ixl_vc_opcode_str(op), op, status, vf->vf_num, file, line);
	ixl_send_vf_msg(pf, vf, op, status, NULL, 0);
}

static void
ixl_vf_version_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_version_info reply;

	if (msg_size != sizeof(struct i40e_virtchnl_version_info)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_VERSION,
		    I40E_ERR_PARAM);
		return;
	}

	reply.major = I40E_VIRTCHNL_VERSION_MAJOR;
	reply.minor = I40E_VIRTCHNL_VERSION_MINOR;
	ixl_send_vf_msg(pf, vf, I40E_VIRTCHNL_OP_VERSION, I40E_SUCCESS, &reply,
	    sizeof(reply));
}

static void
ixl_vf_reset_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{

	if (msg_size != 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_RESET_VF,
		    I40E_ERR_PARAM);
		return;
	}

	ixl_reset_vf(pf, vf);

	/* No response to a reset message. */
}

static void
ixl_vf_get_resources_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_vf_resource reply;

	if (msg_size != 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
		    I40E_ERR_PARAM);
		return;
	}

	bzero(&reply, sizeof(reply));

	reply.vf_offload_flags = I40E_VIRTCHNL_VF_OFFLOAD_L2;

	reply.num_vsis = 1;
	reply.num_queue_pairs = vf->vsi.num_queues;
	reply.max_vectors = pf->hw.func_caps.num_msix_vectors_vf;
	reply.vsi_res[0].vsi_id = vf->vsi.vsi_num;
	reply.vsi_res[0].vsi_type = I40E_VSI_SRIOV;
	reply.vsi_res[0].num_queue_pairs = vf->vsi.num_queues;
	memcpy(reply.vsi_res[0].default_mac_addr, vf->mac, ETHER_ADDR_LEN);

	ixl_send_vf_msg(pf, vf, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
	    I40E_SUCCESS, &reply, sizeof(reply));
}

static int
ixl_vf_config_tx_queue(struct ixl_pf *pf, struct ixl_vf *vf,
    struct i40e_virtchnl_txq_info *info)
{
	struct i40e_hw *hw;
	struct i40e_hmc_obj_txq txq;
	uint16_t global_queue_num, global_vf_num;
	enum i40e_status_code status;
	uint32_t qtx_ctl;

	hw = &pf->hw;
	global_queue_num = vf->vsi.first_queue + info->queue_id;
	global_vf_num = hw->func_caps.vf_base_id + vf->vf_num;
	bzero(&txq, sizeof(txq));

	status = i40e_clear_lan_tx_queue_context(hw, global_queue_num);
	if (status != I40E_SUCCESS)
		return (EINVAL);

	txq.base = info->dma_ring_addr / IXL_TX_CTX_BASE_UNITS;

	txq.head_wb_ena = info->headwb_enabled;
	txq.head_wb_addr = info->dma_headwb_addr;
	txq.qlen = info->ring_len;
	txq.rdylist = le16_to_cpu(vf->vsi.info.qs_handle[0]);
	txq.rdylist_act = 0;

	status = i40e_set_lan_tx_queue_context(hw, global_queue_num, &txq);
	if (status != I40E_SUCCESS)
		return (EINVAL);

	qtx_ctl = I40E_QTX_CTL_VF_QUEUE |
	    (hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) |
	    (global_vf_num << I40E_QTX_CTL_VFVM_INDX_SHIFT);
	wr32(hw, I40E_QTX_CTL(global_queue_num), qtx_ctl);
	ixl_flush(hw);

	return (0);
}

static int
ixl_vf_config_rx_queue(struct ixl_pf *pf, struct ixl_vf *vf,
    struct i40e_virtchnl_rxq_info *info)
{
	struct i40e_hw *hw;
	struct i40e_hmc_obj_rxq rxq;
	uint16_t global_queue_num;
	enum i40e_status_code status;

	hw = &pf->hw;
	global_queue_num = vf->vsi.first_queue + info->queue_id;
	bzero(&rxq, sizeof(rxq));

	if (info->databuffer_size > IXL_VF_MAX_BUFFER)
		return (EINVAL);

	if (info->max_pkt_size > IXL_VF_MAX_FRAME ||
	    info->max_pkt_size < ETHER_MIN_LEN)
		return (EINVAL);

	if (info->splithdr_enabled) {
		if (info->hdr_size > IXL_VF_MAX_HDR_BUFFER)
			return (EINVAL);

		rxq.hsplit_0 = info->rx_split_pos &
		    (I40E_HMC_OBJ_RX_HSPLIT_0_SPLIT_L2 |
		     I40E_HMC_OBJ_RX_HSPLIT_0_SPLIT_IP |
		     I40E_HMC_OBJ_RX_HSPLIT_0_SPLIT_TCP_UDP |
		     I40E_HMC_OBJ_RX_HSPLIT_0_SPLIT_SCTP);
		rxq.hbuff = info->hdr_size >> I40E_RXQ_CTX_HBUFF_SHIFT;

		rxq.dtype = 2;
	}

	status = i40e_clear_lan_rx_queue_context(hw, global_queue_num);
	if (status != I40E_SUCCESS)
		return (EINVAL);

	rxq.base = info->dma_ring_addr / IXL_RX_CTX_BASE_UNITS;
	rxq.qlen = info->ring_len;

	rxq.dbuff = info->databuffer_size >> I40E_RXQ_CTX_DBUFF_SHIFT;

	rxq.dsize = 1;
	rxq.crcstrip = 1;
	rxq.l2tsel = 1;

	rxq.rxmax = info->max_pkt_size;
	rxq.tphrdesc_ena = 1;
	rxq.tphwdesc_ena = 1;
	rxq.tphdata_ena = 1;
	rxq.tphhead_ena = 1;
	rxq.lrxqthresh = 2;
	rxq.prefena = 1;

	status = i40e_set_lan_rx_queue_context(hw, global_queue_num, &rxq);
	if (status != I40E_SUCCESS)
		return (EINVAL);

	return (0);
}

static void
ixl_vf_config_vsi_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_vsi_queue_config_info *info;
	struct i40e_virtchnl_queue_pair_info *pair;
	int i;

	if (msg_size < sizeof(*info)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	info = msg;
	if (info->num_queue_pairs == 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	if (msg_size != sizeof(*info) + info->num_queue_pairs * sizeof(*pair)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	if (info->vsi_id != vf->vsi.vsi_num) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < info->num_queue_pairs; i++) {
		pair = &info->qpair[i];

		if (pair->txq.vsi_id != vf->vsi.vsi_num ||
		    pair->rxq.vsi_id != vf->vsi.vsi_num ||
		    pair->txq.queue_id != pair->rxq.queue_id ||
		    pair->txq.queue_id >= vf->vsi.num_queues) {

			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES, I40E_ERR_PARAM);
			return;
		}

		if (ixl_vf_config_tx_queue(pf, vf, &pair->txq) != 0) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES, I40E_ERR_PARAM);
			return;
		}

		if (ixl_vf_config_rx_queue(pf, vf, &pair->rxq) != 0) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES, I40E_ERR_PARAM);
			return;
		}
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES);
}

static void
ixl_vf_set_qctl(struct ixl_pf *pf,
    const struct i40e_virtchnl_vector_map *vector,
    enum i40e_queue_type cur_type, uint16_t cur_queue,
    enum i40e_queue_type *last_type, uint16_t *last_queue)
{
	uint32_t offset, qctl;
	uint16_t itr_indx;

	if (cur_type == I40E_QUEUE_TYPE_RX) {
		offset = I40E_QINT_RQCTL(cur_queue);
		itr_indx = vector->rxitr_idx;
	} else {
		offset = I40E_QINT_TQCTL(cur_queue);
		itr_indx = vector->txitr_idx;
	}

	qctl = htole32((vector->vector_id << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
	    (*last_type << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
	    (*last_queue << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
	    I40E_QINT_RQCTL_CAUSE_ENA_MASK |
	    (itr_indx << I40E_QINT_RQCTL_ITR_INDX_SHIFT));

	wr32(&pf->hw, offset, qctl);

	*last_type = cur_type;
	*last_queue = cur_queue;
}

static void
ixl_vf_config_vector(struct ixl_pf *pf, struct ixl_vf *vf,
    const struct i40e_virtchnl_vector_map *vector)
{
	struct i40e_hw *hw;
	u_int qindex;
	enum i40e_queue_type type, last_type;
	uint32_t lnklst_reg;
	uint16_t rxq_map, txq_map, cur_queue, last_queue;

	hw = &pf->hw;

	rxq_map = vector->rxq_map;
	txq_map = vector->txq_map;

	last_queue = IXL_END_OF_INTR_LNKLST;
	last_type = I40E_QUEUE_TYPE_RX;

	/*
	 * The datasheet says to optimize performance, RX queues and TX queues
	 * should be interleaved in the interrupt linked list, so we process
	 * both at once here.
	 */
	while ((rxq_map != 0) || (txq_map != 0)) {
		if (txq_map != 0) {
			qindex = ffs(txq_map) - 1;
			type = I40E_QUEUE_TYPE_TX;
			cur_queue = vf->vsi.first_queue + qindex;
			ixl_vf_set_qctl(pf, vector, type, cur_queue,
			    &last_type, &last_queue);
			txq_map &= ~(1 << qindex);
		}

		if (rxq_map != 0) {
			qindex = ffs(rxq_map) - 1;
			type = I40E_QUEUE_TYPE_RX;
			cur_queue = vf->vsi.first_queue + qindex;
			ixl_vf_set_qctl(pf, vector, type, cur_queue,
			    &last_type, &last_queue);
			rxq_map &= ~(1 << qindex);
		}
	}

	if (vector->vector_id == 0)
		lnklst_reg = I40E_VPINT_LNKLST0(vf->vf_num);
	else
		lnklst_reg = IXL_VPINT_LNKLSTN_REG(hw, vector->vector_id,
		    vf->vf_num);
	wr32(hw, lnklst_reg,
	    (last_queue << I40E_VPINT_LNKLST0_FIRSTQ_INDX_SHIFT) |
	    (last_type << I40E_VPINT_LNKLST0_FIRSTQ_TYPE_SHIFT));

	ixl_flush(hw);
}

static void
ixl_vf_config_irq_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_irq_map_info *map;
	struct i40e_virtchnl_vector_map *vector;
	struct i40e_hw *hw;
	int i, largest_txq, largest_rxq;

	hw = &pf->hw;

	if (msg_size < sizeof(*map)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
		    I40E_ERR_PARAM);
		return;
	}

	map = msg;
	if (map->num_vectors == 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
		    I40E_ERR_PARAM);
		return;
	}

	if (msg_size != sizeof(*map) + map->num_vectors * sizeof(*vector)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < map->num_vectors; i++) {
		vector = &map->vecmap[i];

		if ((vector->vector_id >= hw->func_caps.num_msix_vectors_vf) ||
		    vector->vsi_id != vf->vsi.vsi_num) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP, I40E_ERR_PARAM);
			return;
		}

		if (vector->rxq_map != 0) {
			largest_rxq = fls(vector->rxq_map) - 1;
			if (largest_rxq >= vf->vsi.num_queues) {
				i40e_send_vf_nack(pf, vf,
				    I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
				    I40E_ERR_PARAM);
				return;
			}
		}

		if (vector->txq_map != 0) {
			largest_txq = fls(vector->txq_map) - 1;
			if (largest_txq >= vf->vsi.num_queues) {
				i40e_send_vf_nack(pf, vf,
				    I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
				    I40E_ERR_PARAM);
				return;
			}
		}

		if (vector->rxitr_idx > IXL_MAX_ITR_IDX ||
		    vector->txitr_idx > IXL_MAX_ITR_IDX) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
			    I40E_ERR_PARAM);
			return;
		}

		ixl_vf_config_vector(pf, vf, vector);
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP);
}

static void
ixl_vf_enable_queues_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_queue_select *select;
	int error;

	if (msg_size != sizeof(*select)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	select = msg;
	if (select->vsi_id != vf->vsi.vsi_num ||
	    select->rx_queues == 0 || select->tx_queues == 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	error = ixl_enable_rings(&vf->vsi);
	if (error) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
		    I40E_ERR_TIMEOUT);
		return;
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_ENABLE_QUEUES);
}

static void
ixl_vf_disable_queues_msg(struct ixl_pf *pf, struct ixl_vf *vf,
    void *msg, uint16_t msg_size)
{
	struct i40e_virtchnl_queue_select *select;
	int error;

	if (msg_size != sizeof(*select)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	select = msg;
	if (select->vsi_id != vf->vsi.vsi_num ||
	    select->rx_queues == 0 || select->tx_queues == 0) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
		    I40E_ERR_PARAM);
		return;
	}

	error = ixl_disable_rings(&vf->vsi);
	if (error) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
		    I40E_ERR_TIMEOUT);
		return;
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_DISABLE_QUEUES);
}

static boolean_t
ixl_zero_mac(const uint8_t *addr)
{
	uint8_t zero[ETHER_ADDR_LEN] = {0, 0, 0, 0, 0, 0};

	return (cmp_etheraddr(addr, zero));
}

static boolean_t
ixl_bcast_mac(const uint8_t *addr)
{

	return (cmp_etheraddr(addr, ixl_bcast_addr));
}

static int
ixl_vf_mac_valid(struct ixl_vf *vf, const uint8_t *addr)
{

	if (ixl_zero_mac(addr) || ixl_bcast_mac(addr))
		return (EINVAL);

	/*
	 * If the VF is not allowed to change its MAC address, don't let it
	 * set a MAC filter for an address that is not a multicast address and
	 * is not its assigned MAC.
	 */
	if (!(vf->vf_flags & VF_FLAG_SET_MAC_CAP) &&
	    !(ETHER_IS_MULTICAST(addr) || cmp_etheraddr(addr, vf->mac)))
		return (EPERM);

	return (0);
}

static void
ixl_vf_add_mac_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_ether_addr_list *addr_list;
	struct i40e_virtchnl_ether_addr *addr;
	struct ixl_vsi *vsi;
	int i;
	size_t expected_size;

	vsi = &vf->vsi;

	if (msg_size < sizeof(*addr_list)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS,
		    I40E_ERR_PARAM);
		return;
	}

	addr_list = msg;
	expected_size = sizeof(*addr_list) +
	    addr_list->num_elements * sizeof(*addr);

	if (addr_list->num_elements == 0 ||
	    addr_list->vsi_id != vsi->vsi_num ||
	    msg_size != expected_size) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		if (ixl_vf_mac_valid(vf, addr_list->list[i].addr) != 0) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS, I40E_ERR_PARAM);
			return;
		}
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		addr = &addr_list->list[i];
		ixl_add_filter(vsi, addr->addr, IXL_VLAN_ANY);
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS);
}

static void
ixl_vf_del_mac_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_ether_addr_list *addr_list;
	struct i40e_virtchnl_ether_addr *addr;
	size_t expected_size;
	int i;

	if (msg_size < sizeof(*addr_list)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS,
		    I40E_ERR_PARAM);
		return;
	}

	addr_list = msg;
	expected_size = sizeof(*addr_list) +
	    addr_list->num_elements * sizeof(*addr);

	if (addr_list->num_elements == 0 ||
	    addr_list->vsi_id != vf->vsi.vsi_num ||
	    msg_size != expected_size) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		addr = &addr_list->list[i];
		if (ixl_zero_mac(addr->addr) || ixl_bcast_mac(addr->addr)) {
			i40e_send_vf_nack(pf, vf,
			    I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS, I40E_ERR_PARAM);
			return;
		}
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		addr = &addr_list->list[i];
		ixl_del_filter(&vf->vsi, addr->addr, IXL_VLAN_ANY);
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS);
}

static enum i40e_status_code
ixl_vf_enable_vlan_strip(struct ixl_pf *pf, struct ixl_vf *vf)
{
	struct i40e_vsi_context vsi_ctx;

	vsi_ctx.seid = vf->vsi.seid;

	bzero(&vsi_ctx.info, sizeof(vsi_ctx.info));
	vsi_ctx.info.valid_sections = htole16(I40E_AQ_VSI_PROP_VLAN_VALID);
	vsi_ctx.info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL |
	    I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	return (i40e_aq_update_vsi_params(&pf->hw, &vsi_ctx, NULL));
}

static void
ixl_vf_add_vlan_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_vlan_filter_list *filter_list;
	enum i40e_status_code code;
	size_t expected_size;
	int i;

	if (msg_size < sizeof(*filter_list)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	filter_list = msg;
	expected_size = sizeof(*filter_list) +
	    filter_list->num_elements * sizeof(uint16_t);
	if (filter_list->num_elements == 0 ||
	    filter_list->vsi_id != vf->vsi.vsi_num ||
	    msg_size != expected_size) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	if (!(vf->vf_flags & VF_FLAG_VLAN_CAP)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < filter_list->num_elements; i++) {
		if (filter_list->vlan_id[i] > EVL_VLID_MASK) {
			i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
			    I40E_ERR_PARAM);
			return;
		}
	}

	code = ixl_vf_enable_vlan_strip(pf, vf);
	if (code != I40E_SUCCESS) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
		    I40E_ERR_PARAM);
	}

	for (i = 0; i < filter_list->num_elements; i++)
		ixl_add_filter(&vf->vsi, vf->mac, filter_list->vlan_id[i]);

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN);
}

static void
ixl_vf_del_vlan_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_vlan_filter_list *filter_list;
	int i;
	size_t expected_size;

	if (msg_size < sizeof(*filter_list)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_DEL_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	filter_list = msg;
	expected_size = sizeof(*filter_list) +
	    filter_list->num_elements * sizeof(uint16_t);
	if (filter_list->num_elements == 0 ||
	    filter_list->vsi_id != vf->vsi.vsi_num ||
	    msg_size != expected_size) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_DEL_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < filter_list->num_elements; i++) {
		if (filter_list->vlan_id[i] > EVL_VLID_MASK) {
			i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
			    I40E_ERR_PARAM);
			return;
		}
	}

	if (!(vf->vf_flags & VF_FLAG_VLAN_CAP)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_ADD_VLAN,
		    I40E_ERR_PARAM);
		return;
	}

	for (i = 0; i < filter_list->num_elements; i++)
		ixl_del_filter(&vf->vsi, vf->mac, filter_list->vlan_id[i]);

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_DEL_VLAN);
}

static void
ixl_vf_config_promisc_msg(struct ixl_pf *pf, struct ixl_vf *vf,
    void *msg, uint16_t msg_size)
{
	struct i40e_virtchnl_promisc_info *info;
	enum i40e_status_code code;

	if (msg_size != sizeof(*info)) {
		i40e_send_vf_nack(pf, vf,
		    I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, I40E_ERR_PARAM);
		return;
	}

	if (!vf->vf_flags & VF_FLAG_PROMISC_CAP) {
		i40e_send_vf_nack(pf, vf,
		    I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, I40E_ERR_PARAM);
		return;
	}

	info = msg;
	if (info->vsi_id != vf->vsi.vsi_num) {
		i40e_send_vf_nack(pf, vf,
		    I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, I40E_ERR_PARAM);
		return;
	}

	code = i40e_aq_set_vsi_unicast_promiscuous(&pf->hw, info->vsi_id,
	    info->flags & I40E_FLAG_VF_UNICAST_PROMISC, NULL);
	if (code != I40E_SUCCESS) {
		i40e_send_vf_nack(pf, vf,
		    I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, code);
		return;
	}

	code = i40e_aq_set_vsi_multicast_promiscuous(&pf->hw, info->vsi_id,
	    info->flags & I40E_FLAG_VF_MULTICAST_PROMISC, NULL);
	if (code != I40E_SUCCESS) {
		i40e_send_vf_nack(pf, vf,
		    I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, code);
		return;
	}

	ixl_send_vf_ack(pf, vf, I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE);
}

static void
ixl_vf_get_stats_msg(struct ixl_pf *pf, struct ixl_vf *vf, void *msg,
    uint16_t msg_size)
{
	struct i40e_virtchnl_queue_select *queue;

	if (msg_size != sizeof(*queue)) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_GET_STATS,
		    I40E_ERR_PARAM);
		return;
	}

	queue = msg;
	if (queue->vsi_id != vf->vsi.vsi_num) {
		i40e_send_vf_nack(pf, vf, I40E_VIRTCHNL_OP_GET_STATS,
		    I40E_ERR_PARAM);
		return;
	}

	ixl_update_eth_stats(&vf->vsi);

	ixl_send_vf_msg(pf, vf, I40E_VIRTCHNL_OP_GET_STATS,
	    I40E_SUCCESS, &vf->vsi.eth_stats, sizeof(vf->vsi.eth_stats));
}

static void
ixl_handle_vf_msg(struct ixl_pf *pf, struct i40e_arq_event_info *event)
{
	struct ixl_vf *vf;
	void *msg;
	uint16_t vf_num, msg_size;
	uint32_t opcode;

	vf_num = le16toh(event->desc.retval) - pf->hw.func_caps.vf_base_id;
	opcode = le32toh(event->desc.cookie_high);

	if (vf_num >= pf->num_vfs) {
		device_printf(pf->dev, "Got msg from illegal VF: %d\n", vf_num);
		return;
	}

	vf = &pf->vfs[vf_num];
	msg = event->msg_buf;
	msg_size = event->msg_len;

	I40E_VC_DEBUG(pf, ixl_vc_opcode_level(opcode),
	    "Got msg %s(%d) from VF-%d of size %d\n",
	    ixl_vc_opcode_str(opcode), opcode, vf_num, msg_size);

	switch (opcode) {
	case I40E_VIRTCHNL_OP_VERSION:
		ixl_vf_version_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_RESET_VF:
		ixl_vf_reset_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
		ixl_vf_get_resources_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		ixl_vf_config_vsi_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		ixl_vf_config_irq_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
		ixl_vf_enable_queues_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		ixl_vf_disable_queues_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
		ixl_vf_add_mac_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		ixl_vf_del_mac_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_ADD_VLAN:
		ixl_vf_add_vlan_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		ixl_vf_del_vlan_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		ixl_vf_config_promisc_msg(pf, vf, msg, msg_size);
		break;
	case I40E_VIRTCHNL_OP_GET_STATS:
		ixl_vf_get_stats_msg(pf, vf, msg, msg_size);
		break;

	/* These two opcodes have been superseded by CONFIG_VSI_QUEUES. */
	case I40E_VIRTCHNL_OP_CONFIG_TX_QUEUE:
	case I40E_VIRTCHNL_OP_CONFIG_RX_QUEUE:
	default:
		i40e_send_vf_nack(pf, vf, opcode, I40E_ERR_NOT_IMPLEMENTED);
		break;
	}
}

/* Handle any VFs that have reset themselves via a Function Level Reset(FLR). */
static void
ixl_handle_vflr(void *arg, int pending)
{
	struct ixl_pf *pf;
	struct i40e_hw *hw;
	uint16_t global_vf_num;
	uint32_t vflrstat_index, vflrstat_mask, vflrstat, icr0;
	int i;

	pf = arg;
	hw = &pf->hw;

	IXL_PF_LOCK(pf);
	for (i = 0; i < pf->num_vfs; i++) {
		global_vf_num = hw->func_caps.vf_base_id + i;

		vflrstat_index = IXL_GLGEN_VFLRSTAT_INDEX(global_vf_num);
		vflrstat_mask = IXL_GLGEN_VFLRSTAT_MASK(global_vf_num);
		vflrstat = rd32(hw, I40E_GLGEN_VFLRSTAT(vflrstat_index));
		if (vflrstat & vflrstat_mask) {
			wr32(hw, I40E_GLGEN_VFLRSTAT(vflrstat_index),
			    vflrstat_mask);

			ixl_reinit_vf(pf, &pf->vfs[i]);
		}
	}

	icr0 = rd32(hw, I40E_PFINT_ICR0_ENA);
	icr0 |= I40E_PFINT_ICR0_ENA_VFLR_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, icr0);
	ixl_flush(hw);

	IXL_PF_UNLOCK(pf);
}

static int
ixl_adminq_err_to_errno(enum i40e_admin_queue_err err)
{

	switch (err) {
	case I40E_AQ_RC_EPERM:
		return (EPERM);
	case I40E_AQ_RC_ENOENT:
		return (ENOENT);
	case I40E_AQ_RC_ESRCH:
		return (ESRCH);
	case I40E_AQ_RC_EINTR:
		return (EINTR);
	case I40E_AQ_RC_EIO:
		return (EIO);
	case I40E_AQ_RC_ENXIO:
		return (ENXIO);
	case I40E_AQ_RC_E2BIG:
		return (E2BIG);
	case I40E_AQ_RC_EAGAIN:
		return (EAGAIN);
	case I40E_AQ_RC_ENOMEM:
		return (ENOMEM);
	case I40E_AQ_RC_EACCES:
		return (EACCES);
	case I40E_AQ_RC_EFAULT:
		return (EFAULT);
	case I40E_AQ_RC_EBUSY:
		return (EBUSY);
	case I40E_AQ_RC_EEXIST:
		return (EEXIST);
	case I40E_AQ_RC_EINVAL:
		return (EINVAL);
	case I40E_AQ_RC_ENOTTY:
		return (ENOTTY);
	case I40E_AQ_RC_ENOSPC:
		return (ENOSPC);
	case I40E_AQ_RC_ENOSYS:
		return (ENOSYS);
	case I40E_AQ_RC_ERANGE:
		return (ERANGE);
	case I40E_AQ_RC_EFLUSHED:
		return (EINVAL);	/* No exact equivalent in errno.h */
	case I40E_AQ_RC_BAD_ADDR:
		return (EFAULT);
	case I40E_AQ_RC_EMODE:
		return (EPERM);
	case I40E_AQ_RC_EFBIG:
		return (EFBIG);
	default:
		return (EINVAL);
	}
}

static int
ixl_iov_init(device_t dev, uint16_t num_vfs, const nvlist_t *params)
{
	struct ixl_pf *pf;
	struct i40e_hw *hw;
	struct ixl_vsi *pf_vsi;
	enum i40e_status_code ret;
	int i, error;

	pf = device_get_softc(dev);
	hw = &pf->hw;
	pf_vsi = &pf->vsi;

	IXL_PF_LOCK(pf);
	pf->vfs = malloc(sizeof(struct ixl_vf) * num_vfs, M_IXL, M_NOWAIT |
	    M_ZERO);

	if (pf->vfs == NULL) {
		error = ENOMEM;
		goto fail;
	}

	for (i = 0; i < num_vfs; i++)
		sysctl_ctx_init(&pf->vfs[i].ctx);

	ret = i40e_aq_add_veb(hw, pf_vsi->uplink_seid, pf_vsi->seid,
	    1, FALSE, FALSE, &pf->veb_seid, NULL);
	if (ret != I40E_SUCCESS) {
		error = ixl_adminq_err_to_errno(hw->aq.asq_last_status);
		device_printf(dev, "add_veb failed; code=%d error=%d", ret,
		    error);
		goto fail;
	}

	ixl_configure_msix(pf);
	ixl_enable_adminq(hw);

	pf->num_vfs = num_vfs;
	IXL_PF_UNLOCK(pf);
	return (0);

fail:
	free(pf->vfs, M_IXL);
	pf->vfs = NULL;
	IXL_PF_UNLOCK(pf);
	return (error);
}

static void
ixl_iov_uninit(device_t dev)
{
	struct ixl_pf *pf;
	struct i40e_hw *hw;
	struct ixl_vsi *vsi;
	struct ifnet *ifp;
	struct ixl_vf *vfs;
	int i, num_vfs;

	pf = device_get_softc(dev);
	hw = &pf->hw;
	vsi = &pf->vsi;
	ifp = vsi->ifp;

	IXL_PF_LOCK(pf);
	for (i = 0; i < pf->num_vfs; i++) {
		if (pf->vfs[i].vsi.seid != 0)
			i40e_aq_delete_element(hw, pf->vfs[i].vsi.seid, NULL);
	}

	if (pf->veb_seid != 0) {
		i40e_aq_delete_element(hw, pf->veb_seid, NULL);
		pf->veb_seid = 0;
	}

	if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING) == 0)
		ixl_disable_intr(vsi);

	vfs = pf->vfs;
	num_vfs = pf->num_vfs;

	pf->vfs = NULL;
	pf->num_vfs = 0;
	IXL_PF_UNLOCK(pf);

	/* Do this after the unlock as sysctl_ctx_free might sleep. */
	for (i = 0; i < num_vfs; i++)
		sysctl_ctx_free(&vfs[i].ctx);
	free(vfs, M_IXL);
}

static int
ixl_add_vf(device_t dev, uint16_t vfnum, const nvlist_t *params)
{
	char sysctl_name[QUEUE_NAME_LEN];
	struct ixl_pf *pf;
	struct ixl_vf *vf;
	const void *mac;
	size_t size;
	int error;

	pf = device_get_softc(dev);
	vf = &pf->vfs[vfnum];

	IXL_PF_LOCK(pf);
	vf->vf_num = vfnum;

	vf->vsi.back = pf;
	vf->vf_flags = VF_FLAG_ENABLED;
	SLIST_INIT(&vf->vsi.ftl);

	error = ixl_vf_setup_vsi(pf, vf);
	if (error != 0)
		goto out;

	if (nvlist_exists_binary(params, "mac-addr")) {
		mac = nvlist_get_binary(params, "mac-addr", &size);
		bcopy(mac, vf->mac, ETHER_ADDR_LEN);

		if (nvlist_get_bool(params, "allow-set-mac"))
			vf->vf_flags |= VF_FLAG_SET_MAC_CAP;
	} else
		/*
		 * If the administrator has not specified a MAC address then
		 * we must allow the VF to choose one.
		 */
		vf->vf_flags |= VF_FLAG_SET_MAC_CAP;

	if (nvlist_get_bool(params, "mac-anti-spoof"))
		vf->vf_flags |= VF_FLAG_MAC_ANTI_SPOOF;

	if (nvlist_get_bool(params, "allow-promisc"))
		vf->vf_flags |= VF_FLAG_PROMISC_CAP;

	vf->vf_flags |= VF_FLAG_VLAN_CAP;

	ixl_reset_vf(pf, vf);
out:
	IXL_PF_UNLOCK(pf);
	if (error == 0) {
		snprintf(sysctl_name, sizeof(sysctl_name), "vf%d", vfnum);
		ixl_add_vsi_sysctls(pf, &vf->vsi, &vf->ctx, sysctl_name);
	}

	return (error);
}
#endif /* PCI_IOV */
