/******************************************************************************

  Copyright (c) 2013-2014, Intel Corporation 
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

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_inet.h"
#include "opt_inet6.h"
#endif

#include "i40e.h"
#include "i40e_pf.h"

/*********************************************************************
 *  Driver version
 *********************************************************************/
char i40e_driver_version[] = "1.0.0";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into i40e_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static i40e_vendor_info_t i40e_vendor_info_array[] =
{
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_SFP_XL710, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_A, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_KX_C, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_A, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_B, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_QSFP_C, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *i40e_strings[] = {
	"Intel(R) Ethernet Connection XL710 Driver"
};


/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      i40e_probe(device_t);
static int      i40e_attach(device_t);
static int      i40e_detach(device_t);
static int      i40e_shutdown(device_t);
static int	i40e_get_hw_capabilities(struct i40e_pf *);
static void	i40e_cap_txcsum_tso(struct i40e_vsi *, struct ifnet *, int);
static int      i40e_ioctl(struct ifnet *, u_long, caddr_t);
static void	i40e_init(void *);
static void	i40e_init_locked(struct i40e_pf *);
static void     i40e_stop(struct i40e_pf *);
static void     i40e_media_status(struct ifnet *, struct ifmediareq *);
static int      i40e_media_change(struct ifnet *);
static void     i40e_update_link_status(struct i40e_pf *);
static int      i40e_allocate_pci_resources(struct i40e_pf *);
static u16	i40e_get_bus_info(struct i40e_hw *, device_t);
static int	i40e_setup_stations(struct i40e_pf *);
static int	i40e_setup_vsi(struct i40e_vsi *);
static int	i40e_initialize_vsi(struct i40e_vsi *);
static int	i40e_assign_vsi_msix(struct i40e_pf *);
static int	i40e_assign_vsi_legacy(struct i40e_pf *);
static int	i40e_init_msix(struct i40e_pf *);
static void	i40e_configure_msix(struct i40e_pf *);
static void	i40e_configure_itr(struct i40e_pf *);
static void	i40e_configure_legacy(struct i40e_pf *);
static void	i40e_free_pci_resources(struct i40e_pf *);
static void	i40e_local_timer(void *);
static int	i40e_setup_interface(device_t, struct i40e_vsi *);
static bool	i40e_config_link(struct i40e_hw *);
static void	i40e_config_rss(struct i40e_vsi *);
static void	i40e_set_queue_rx_itr(struct i40e_queue *);
static void	i40e_set_queue_tx_itr(struct i40e_queue *);

static void	i40e_enable_rings(struct i40e_vsi *);
static void	i40e_disable_rings(struct i40e_vsi *);
static void     i40e_enable_intr(struct i40e_vsi *);
static void     i40e_disable_intr(struct i40e_vsi *);

static void     i40e_enable_adminq(struct i40e_hw *);
static void     i40e_disable_adminq(struct i40e_hw *);
static void     i40e_enable_queue(struct i40e_hw *, int);
static void     i40e_disable_queue(struct i40e_hw *, int);
static void     i40e_enable_legacy(struct i40e_hw *);
static void     i40e_disable_legacy(struct i40e_hw *);

static void     i40e_set_promisc(struct i40e_vsi *);
static void     i40e_add_multi(struct i40e_vsi *);
static void     i40e_del_multi(struct i40e_vsi *);
static void	i40e_register_vlan(void *, struct ifnet *, u16);
static void	i40e_unregister_vlan(void *, struct ifnet *, u16);
static void	i40e_setup_vlan_filters(struct i40e_vsi *);

static void	i40e_init_filters(struct i40e_vsi *);
static void	i40e_add_filter(struct i40e_vsi *, u8 *, s16 vlan);
static void	i40e_del_filter(struct i40e_vsi *, u8 *, s16 vlan);
static void	i40e_add_hw_filters(struct i40e_vsi *, int, int);
static void	i40e_del_hw_filters(struct i40e_vsi *, int);
static struct i40e_mac_filter *
		i40e_find_filter(struct i40e_vsi *, u8 *, s16);
static void	i40e_add_mc_filter(struct i40e_vsi *, u8 *);

/* Sysctl debug interface */
static int	i40e_debug_info(SYSCTL_HANDLER_ARGS);
static void	i40e_print_debug_info(struct i40e_pf *);

/* The MSI/X Interrupt handlers */
static void	i40e_intr(void *);
static void	i40e_msix_que(void *);
static void	i40e_msix_adminq(void *);
static void	i40e_handle_mdd_event(struct i40e_pf *);

/* Deferred interrupt tasklets */
static void	i40e_do_adminq(void *, int);

/* Sysctl handlers */
static int	i40e_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	i40e_set_advertise(SYSCTL_HANDLER_ARGS);

/* Statistics */
static void     i40e_add_hw_stats(struct i40e_pf *);
static void	i40e_add_sysctls_mac_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *, struct i40e_hw_port_stats *);
static void	i40e_add_sysctls_eth_stats(struct sysctl_ctx_list *,
		    struct sysctl_oid_list *,
		    struct i40e_eth_stats *);
static void	i40e_update_stats_counters(struct i40e_pf *);
static void	i40e_update_eth_stats(struct i40e_vsi *);
static void	i40e_pf_reset_stats(struct i40e_pf *);
static void	i40e_vsi_reset_stats(struct i40e_vsi *);
static void	i40e_stat_update48(struct i40e_hw *, u32, u32, bool,
		    u64 *, u64 *);
static void	i40e_stat_update32(struct i40e_hw *, u32, bool,
		    u64 *, u64 *);

#ifdef I40E_DEBUG
static int 	i40e_sysctl_link_status(SYSCTL_HANDLER_ARGS);
static int	i40e_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS);
static int	i40e_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS);
static int	i40e_sysctl_hw_res_info(SYSCTL_HANDLER_ARGS);
static int	i40e_sysctl_dump_txd(SYSCTL_HANDLER_ARGS);
#endif

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t i40e_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, i40e_probe),
	DEVMETHOD(device_attach, i40e_attach),
	DEVMETHOD(device_detach, i40e_detach),
	DEVMETHOD(device_shutdown, i40e_shutdown),
	{0, 0}
};

static driver_t i40e_driver = {
	"ixl", i40e_methods, sizeof(struct i40e_pf),
};

devclass_t i40e_devclass;
DRIVER_MODULE(i40e, pci, i40e_driver, i40e_devclass, 0, 0);

MODULE_DEPEND(i40e, pci, 1, 1, 1);
MODULE_DEPEND(i40e, ether, 1, 1, 1);

/*
** Global reset mutex
*/
static struct mtx i40e_reset_mtx;

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int i40e_enable_msix = 1;
TUNABLE_INT("hw.i40e.enable_msix", &i40e_enable_msix);

/*
** Number of descriptors per ring:
**   - TX and RX are the same size
*/
static int i40e_ringsz = DEFAULT_RING;
TUNABLE_INT("hw.i40e.ringsz", &i40e_ringsz);

/* 
** This can be set manually, if left as 0 the
** number of queues will be calculated based
** on cpus and msix vectors available.
*/
int i40e_max_queues = 0;
TUNABLE_INT("hw.i40e.max_queues", &i40e_max_queues);

/*
** Controls for Interrupt Throttling 
**	- true/false for dynamic adjustment
** 	- default values for static ITR
*/
int i40e_dynamic_rx_itr = 0;
TUNABLE_INT("hw.i40e.dynamic_rx_itr", &i40e_dynamic_rx_itr);
int i40e_dynamic_tx_itr = 0;
TUNABLE_INT("hw.i40e.dynamic_tx_itr", &i40e_dynamic_tx_itr);

int i40e_rx_itr = I40E_ITR_8K;
TUNABLE_INT("hw.i40e.rx_itr", &i40e_rx_itr);
int i40e_tx_itr = I40E_ITR_4K;
TUNABLE_INT("hw.i40e.tx_itr", &i40e_tx_itr);

#ifdef I40E_FDIR
static int i40e_enable_fdir = 1;
TUNABLE_INT("hw.i40e.enable_fdir", &i40e_enable_fdir);
/* Rate at which we sample */
int i40e_atr_rate = 20;
TUNABLE_INT("hw.i40e.atr_rate", &i40e_atr_rate);
#endif


static char *i40e_fc_string[6] = {
	"None",
	"Rx",
	"Tx",
	"Full",
	"Priority",
	"Default"
};


/*********************************************************************
 *  Device identification routine
 *
 *  i40e_probe determines if the driver should be loaded on
 *  the hardware based on PCI vendor/device id of the device.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
i40e_probe(device_t dev)
{
	i40e_vendor_info_t *ent;

	u16	pci_vendor_id, pci_device_id;
	u16	pci_subvendor_id, pci_subdevice_id;
	char	device_name[256];
	static bool lock_init = FALSE;

	INIT_DEBUGOUT("i40e_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != I40E_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = i40e_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(device_name, "%s, Version - %s",
				i40e_strings[ent->index],
				i40e_driver_version);
			device_set_desc_copy(dev, device_name);
			/* One shot mutex init */
			if (lock_init == FALSE) {
				lock_init = TRUE;
				mtx_init(&i40e_reset_mtx,
				    "i40e_reset",
				    "I40E RESET Lock", MTX_DEF);
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
i40e_attach(device_t dev)
{
	struct i40e_pf	*pf;
	struct i40e_hw	*hw;
	struct i40e_vsi *vsi;
	u16		bus;
	int             error = 0;

	INIT_DEBUGOUT("i40e_attach: begin");

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
	I40E_PF_LOCK_INIT(pf, device_get_nameunit(dev));

	/* Set up the timer callout */
	callout_init_mtx(&pf->timer, &pf->pf_mtx, 0);

	/* Set up sysctls */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, i40e_set_flowcntl, "I", "Flow Control");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "advertise_speed", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, i40e_set_advertise, "I", "Advertised Speed");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "rx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    &i40e_rx_itr, I40E_ITR_8K, "RX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dynamic_rx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    &i40e_dynamic_rx_itr, 0, "Dynamic RX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "tx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    &i40e_tx_itr, I40E_ITR_4K, "TX ITR");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dynamic_tx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    &i40e_dynamic_tx_itr, 0, "Dynamic TX ITR");

#ifdef I40E_DEBUG
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "link_status", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, i40e_sysctl_link_status, "A", "Current Link Status");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "phy_abilities", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, i40e_sysctl_phy_abilities, "A", "PHY Abilities");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "filter_list", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, i40e_sysctl_sw_filter_list, "A", "SW Filter List");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "hw_res_info", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, i40e_sysctl_hw_res_info, "A", "HW Resource Allocation");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "dump_desc", CTLTYPE_INT | CTLFLAG_WR,
	    pf, 0, i40e_sysctl_dump_txd, "I", "Desc dump");
#endif

	/* Save off the information about this board */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);

	/* Do PCI setup - map BAR0, etc */
	if (i40e_allocate_pci_resources(pf)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Create for initial debugging use */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug", CTLTYPE_INT|CTLFLAG_RW, pf, 0,
	    i40e_debug_info, "I", "Debug Information");


	/* Establish a clean starting point */
	i40e_clear_hw(hw);
	error = i40e_pf_reset(hw);
	if (error) {
		device_printf(dev,"PF reset failure %x\n", error);
		error = EIO;
		goto err_out;
	}

	/* For now always do an initial CORE reset on first device */
	{
		static int	i40e_dev_count;
		static int	i40e_dev_track[32];
		u32		my_dev;
		int		i, found = FALSE;
		u16		bus = pci_get_bus(dev);

		mtx_lock(&i40e_reset_mtx);
		my_dev = (bus << 8) | hw->bus.device;

		for (i = 0; i < i40e_dev_count; i++) {
			if (i40e_dev_track[i] == my_dev)
				found = TRUE;
		}

                if (!found) {
                        u32 reg;

                        i40e_dev_track[i40e_dev_count] = my_dev;
                        i40e_dev_count++;

                        device_printf(dev, "Initial CORE RESET\n");
                        wr32(hw, I40E_GLGEN_RTRIG, I40E_GLGEN_RTRIG_CORER_MASK);
                        i40e_flush(hw);
                        i = 50;
                        do {
				i40e_msec_delay(50);
                                reg = rd32(hw, I40E_GLGEN_RSTAT);
                                if (!(reg & I40E_GLGEN_RSTAT_DEVSTATE_MASK))
                                        break;
                        } while (i--);

                        /* paranoia */
                        wr32(hw, I40E_PF_ATQLEN, 0);
                        wr32(hw, I40E_PF_ATQBAL, 0);
                        wr32(hw, I40E_PF_ATQBAH, 0);
                        i40e_clear_pxe_mode(hw);
                }
                mtx_unlock(&i40e_reset_mtx);
	}

	/* Set admin queue parameters */
	hw->aq.num_arq_entries = I40E_AQ_LEN;
	hw->aq.num_asq_entries = I40E_AQ_LEN;
	hw->aq.arq_buf_size = I40E_AQ_BUFSZ;
	hw->aq.asq_buf_size = I40E_AQ_BUFSZ;

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
	device_printf(dev, "%s\n", i40e_fw_version_str(hw));

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
	error = i40e_get_hw_capabilities(pf);
	if (error) {
		device_printf(dev, "HW capabilities failure!\n");
		goto err_get_cap;
	}

	/* Set up host memory cache */
	error = i40e_init_lan_hmc(hw, vsi->num_queues, vsi->num_queues, 0, 0);
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

	if (i40e_setup_stations(pf) != 0) { 
		device_printf(dev, "setup stations failed!\n");
		error = ENOMEM;
		goto err_mac_hmc;
	}

	/* Initialize mac filter list for VSI */
	SLIST_INIT(&vsi->ftl);

	/* Set up interrupt routing here */
	if (pf->msix > 1)
		error = i40e_assign_vsi_msix(pf);
	else
		error = i40e_assign_vsi_legacy(pf);
	if (error) 
		goto err_late;

	/* Determine link state */
	vsi->link_up = i40e_config_link(hw);

	/* Report if Unqualified modules are found */
	if ((vsi->link_up == FALSE) &&
	    (pf->hw.phy.link_info.link_info &
	    I40E_AQ_MEDIA_AVAILABLE) &&
	    (!(pf->hw.phy.link_info.an_info &
	    I40E_AQ_QUALIFIED_MODULE)))
		device_printf(dev, "Link failed because "
		    "an unqualified module was detected\n");

	/* Setup OS specific network interface */
	if (i40e_setup_interface(dev, vsi) != 0)
		goto err_late;

	/* Get the bus configuration and set the shared code */
	bus = i40e_get_bus_info(hw, dev);
	i40e_set_pci_config_data(hw, bus);

	/* Initialize statistics */
	i40e_pf_reset_stats(pf);
	i40e_update_stats_counters(pf);
	i40e_add_hw_stats(pf);

	/* Register for VLAN events */
	vsi->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    i40e_register_vlan, vsi, EVENTHANDLER_PRI_FIRST);
	vsi->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    i40e_unregister_vlan, vsi, EVENTHANDLER_PRI_FIRST);


	INIT_DEBUGOUT("i40e_attach: end");
	return (0);

err_late:
	i40e_free_vsi(vsi);
err_mac_hmc:
	i40e_shutdown_lan_hmc(hw);
err_get_cap:
	i40e_shutdown_adminq(hw);
err_out:
	if (vsi->ifp != NULL)
		if_free(vsi->ifp);
	i40e_free_pci_resources(pf);
	I40E_PF_LOCK_DESTROY(pf);
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
i40e_detach(device_t dev)
{
	struct i40e_pf		*pf = device_get_softc(dev);
	struct i40e_hw		*hw = &pf->hw;
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;
	i40e_status		status;
	u32			reg;

	INIT_DEBUGOUT("i40e_detach: begin");

	/* Make sure VLANS are not using driver */
	if (vsi->ifp->if_vlantrunk != NULL) {
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

	I40E_PF_LOCK(pf);
	i40e_stop(pf);
	I40E_PF_UNLOCK(pf);

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		if (que->tq) {
			taskqueue_drain(que->tq, &que->task);
			taskqueue_drain(que->tq, &que->tx_task);
			taskqueue_free(que->tq);
		}
	}

	/* Drain other tasks here */

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

	/* Now force a pf reset */
	reg = rd32(hw, I40E_PFGEN_CTRL);
	reg |= I40E_PFGEN_CTRL_PFSWR_MASK;
	wr32(hw, I40E_PFGEN_CTRL, reg);
	//i40e_pf_reset(hw);
	i40e_flush(hw);

	/* Unregister VLAN events */
	if (vsi->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, vsi->vlan_attach);
	if (vsi->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, vsi->vlan_detach);

	ether_ifdetach(vsi->ifp);
	callout_drain(&pf->timer);


	i40e_free_pci_resources(pf);
	bus_generic_detach(dev);
	if_free(vsi->ifp);
	i40e_free_vsi(vsi);
	I40E_PF_LOCK_DESTROY(pf);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
i40e_shutdown(device_t dev)
{
	struct i40e_pf *pf = device_get_softc(dev);
	I40E_PF_LOCK(pf);
	i40e_stop(pf);
	I40E_PF_UNLOCK(pf);
	return (0);
}


/*********************************************************************
 *
 *  Get the hardware capabilities
 *
 **********************************************************************/

static int
i40e_get_hw_capabilities(struct i40e_pf *pf)
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

#ifdef I40E_DEBUG
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
i40e_cap_txcsum_tso(struct i40e_vsi *vsi, struct ifnet *ifp, int mask)
{
	device_t 	dev = vsi->dev;

	/* Enable/disable TXCSUM/TSO4 */
	if (!(ifp->if_capenable & IFCAP_TXCSUM)
	    && !(ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable |= IFCAP_TXCSUM;
			/* enable TXCSUM, restore TSO if previously enabled */
			if (vsi->flags & I40E_FLAGS_KEEP_TSO4) {
				vsi->flags &= ~I40E_FLAGS_KEEP_TSO4;
				ifp->if_capenable |= IFCAP_TSO4;
			}
		}
		else if (mask & IFCAP_TSO4) {
			ifp->if_capenable |= (IFCAP_TXCSUM | IFCAP_TSO4);
			vsi->flags &= ~I40E_FLAGS_KEEP_TSO4;
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
			vsi->flags |= I40E_FLAGS_KEEP_TSO4;
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
			if (vsi->flags & I40E_FLAGS_KEEP_TSO6) {
				vsi->flags &= ~I40E_FLAGS_KEEP_TSO6;
				ifp->if_capenable |= IFCAP_TSO6;
			}
		} else if (mask & IFCAP_TSO6) {
			ifp->if_capenable |= (IFCAP_TXCSUM_IPV6 | IFCAP_TSO6);
			vsi->flags &= ~I40E_FLAGS_KEEP_TSO6;
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
			vsi->flags |= I40E_FLAGS_KEEP_TSO6;
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
 *  i40e_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
i40e_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct i40e_vsi	*vsi = ifp->if_softc;
	struct i40e_pf	*pf = (struct i40e_pf *)vsi->back;
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
				i40e_init(pf);
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
		} else
			error = ether_ioctl(ifp, command, data);
		break;
#endif
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > I40E_MAX_FRAME -
		   ETHER_HDR_LEN - ETHER_CRC_LEN - ETHER_VLAN_ENCAP_LEN) {
			error = EINVAL;
		} else {
			I40E_PF_LOCK(pf);
			ifp->if_mtu = ifr->ifr_mtu;
			vsi->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
			    + ETHER_VLAN_ENCAP_LEN;
			i40e_init_locked(pf);
			I40E_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		I40E_PF_LOCK(pf);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ pf->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					i40e_set_promisc(vsi);
				}
			} else
				i40e_init_locked(pf);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				i40e_stop(pf);
		pf->if_flags = ifp->if_flags;
		I40E_PF_UNLOCK(pf);
		break;
	case SIOCADDMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCADDMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			I40E_PF_LOCK(pf);
			i40e_disable_intr(vsi);
			i40e_add_multi(vsi);
			i40e_enable_intr(vsi);
			I40E_PF_UNLOCK(pf);
		}
		break;
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCDELMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			I40E_PF_LOCK(pf);
			i40e_disable_intr(vsi);
			i40e_del_multi(vsi);
			i40e_enable_intr(vsi);
			I40E_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &vsi->media, command);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");

		i40e_cap_txcsum_tso(vsi, ifp, mask);

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
			I40E_PF_LOCK(pf);
			i40e_init_locked(pf);
			I40E_PF_UNLOCK(pf);
		}
		VLAN_CAPABILITIES(ifp);

		break;
	}

	default:
		IOCTL_DEBUGOUT1("ioctl: UNKNOWN (0x%X)\n", (int)command);
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
i40e_init_locked(struct i40e_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct i40e_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;
	device_t 	dev = pf->dev;
	struct i40e_filter_control_settings	filter;
	u8		tmpaddr[ETHER_ADDR_LEN];
	int		ret;

	mtx_assert(&pf->pf_mtx, MA_OWNED);
	INIT_DEBUGOUT("i40e_init: begin");
	i40e_stop(pf);

	/* Get the latest mac address... User might use a LAA */
	bcopy(IF_LLADDR(vsi->ifp), tmpaddr,
	      I40E_ETH_LENGTH_OF_ADDRESS);
	if (!cmp_etheraddr(hw->mac.addr, tmpaddr) && 
	    i40e_validate_mac_addr(tmpaddr)) {
		bcopy(tmpaddr, hw->mac.addr,
		    I40E_ETH_LENGTH_OF_ADDRESS);
		ret = i40e_aq_mac_address_write(hw,
		    I40E_AQC_WRITE_TYPE_LAA_ONLY,
		    hw->mac.addr, NULL);
		if (ret) {
			device_printf(dev, "LLA address"
			 "change failed!!\n");
			return;
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
#ifdef I40E_FDIR
	filter.enable_fdir = TRUE;
#endif
	if (i40e_set_filter_control(hw, &filter))
		device_printf(dev, "set_filter_control() failed\n");

	/* Set up RSS */
	i40e_config_rss(vsi);

	/* Setup the VSI */
	i40e_setup_vsi(vsi);

	/*
	** Prepare the rings, hmc contexts, etc...
	*/
	if (i40e_initialize_vsi(vsi)) {
		device_printf(dev,"initialize vsi failed!!\n");
		return;
	}

	/* Add protocol filters to list */
	i40e_init_filters(vsi);

	/* Setup vlan's if needed */
	i40e_setup_vlan_filters(vsi);

	/* Start the local timer */
	callout_reset(&pf->timer, hz, i40e_local_timer, pf);

	/* Set up MSI/X routing and the ITR settings */
	if (i40e_enable_msix) {
		i40e_configure_msix(pf);
		i40e_configure_itr(pf);
	} else
		i40e_configure_legacy(pf);

	i40e_enable_rings(vsi);

	i40e_aq_set_default_vsi(hw, vsi->seid, NULL);

	/* Flow control setup */
	/* NOTE: flow control currently doesn't work correctly */
	// i40e_set_fc_mode(pf, I40E_FC_FULL);

	/* Set MTU in hardware*/
	if (ifp->if_mtu > ETHERMTU) {
		int aq_error =
			i40e_aq_set_mac_config(hw, vsi->max_frame_size,
				TRUE, 0, NULL);
		if (aq_error)
			device_printf(vsi->dev,
				"aq_set_mac_config in init error, code %d\n",
			    aq_error);
	}

	/* And now turn on interrupts */
	i40e_enable_intr(vsi);

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

static void
i40e_init(void *arg)
{
	struct i40e_pf *pf = arg;

	I40E_PF_LOCK(pf);
	i40e_init_locked(pf);
	I40E_PF_UNLOCK(pf);
	return;
}

/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/
static void
i40e_handle_que(void *context, int pending)
{
	struct i40e_queue *que = context;
	struct i40e_vsi *vsi = que->vsi;
	struct i40e_hw  *hw = vsi->hw;
	struct tx_ring  *txr = &que->txr;
	struct ifnet    *ifp = vsi->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = i40e_rxeof(que, I40E_RX_LIMIT);
		I40E_TX_LOCK(txr);
		i40e_txeof(que);
		if (!drbr_empty(ifp, txr->br))
			i40e_mq_start_locked(ifp, txr);
		I40E_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->task);
			return;
		}
	}

	/* Reenable this interrupt - hmmm */
	i40e_enable_queue(hw, que->me);
	return;
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/
void
i40e_intr(void *arg)
{
	struct i40e_pf		*pf = arg;
	struct i40e_hw		*hw =  &pf->hw;
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;
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

	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		taskqueue_enqueue(pf->tq, &pf->adminq);
		return;
	}

	more_rx = i40e_rxeof(que, I40E_RX_LIMIT);

	I40E_TX_LOCK(txr);
	more_tx = i40e_txeof(que);
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	I40E_TX_UNLOCK(txr);

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

	i40e_enable_legacy(hw);

	return;
}


/*********************************************************************
 *
 *  MSIX VSI Interrupt Service routine
 *
 **********************************************************************/
void
i40e_msix_que(void *arg)
{
	struct i40e_queue	*que = arg;
	struct i40e_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	bool		more_tx, more_rx;

	/* Protect against spurious interrupts */
	if (!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	++que->irqs;

	more_rx = i40e_rxeof(que, I40E_RX_LIMIT);

	I40E_TX_LOCK(txr);
	more_tx = i40e_txeof(que);
	/*
	** Make certain that if the stack 
	** has anything queued the task gets
	** scheduled to handle it.
	*/
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	I40E_TX_UNLOCK(txr);

	i40e_set_queue_rx_itr(que);
	i40e_set_queue_tx_itr(que);

	if (more_tx || more_rx)
		taskqueue_enqueue(que->tq, &que->task);
	else
		i40e_enable_queue(hw, que->me);

	return;
}


/*********************************************************************
 *
 *  MSIX Admin Queue Interrupt Service routine
 *
 **********************************************************************/
static void
i40e_msix_adminq(void *arg)
{
	struct i40e_pf	*pf = arg;
	struct i40e_hw	*hw = &pf->hw;
	u32		reg, mask;

	++pf->admin_irq;

	reg = rd32(hw, I40E_PFINT_ICR0);
	mask = rd32(hw, I40E_PFINT_ICR0_ENA);

	/* Check on the cause */
	if (reg & I40E_PFINT_ICR0_ADMINQ_MASK)
		mask &= ~I40E_PFINT_ICR0_ENA_ADMINQ_MASK;

	if (reg & I40E_PFINT_ICR0_MAL_DETECT_MASK) {
		i40e_handle_mdd_event(pf);
		mask &= ~I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	}

	if (reg & I40E_PFINT_ICR0_VFLR_MASK)
		mask &= ~I40E_PFINT_ICR0_ENA_VFLR_MASK;

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
i40e_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct i40e_vsi	*vsi = ifp->if_softc;
	struct i40e_pf	*pf = (struct i40e_pf *)vsi->back;
	struct i40e_hw  *hw = &pf->hw;

	INIT_DEBUGOUT("i40e_media_status: begin");
	I40E_PF_LOCK(pf);

	i40e_update_link_status(pf);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!vsi->link_up) {
		I40E_PF_UNLOCK(pf);
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
		case I40E_PHY_TYPE_10GBASE_CR1_CU:
		case I40E_PHY_TYPE_10GBASE_SFPP_CU:
			ifmr->ifm_active |= IFM_10G_TWINAX;
			break;
		case I40E_PHY_TYPE_10GBASE_SR:
			ifmr->ifm_active |= IFM_10G_SR;
			break;
		case I40E_PHY_TYPE_10GBASE_LR:
			ifmr->ifm_active |= IFM_10G_LR;
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
		default:
			ifmr->ifm_active |= IFM_UNKNOWN;
			break;
	}
	/* Report flow control status as well */
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_TX)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_RX)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;

	I40E_PF_UNLOCK(pf);

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
i40e_media_change(struct ifnet * ifp)
{
	struct i40e_vsi *vsi = ifp->if_softc;
	struct ifmedia *ifm = &vsi->media;

	INIT_DEBUGOUT("i40e_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if_printf(ifp, "Media change is currently not supported.\n");

	return (ENODEV);
}


#ifdef I40E_FDIR
/*
** ATR: Application Targetted Receive - creates a filter
**	based on TX flow info that will keep the receive
**	portion of the flow on the same queue. Based on the
**	implementation this is only available for TCP connections
*/
void
i40e_atr(struct i40e_queue *que, struct tcphdr *th, int etype)
{
	struct i40e_vsi			*vsi = que->vsi;
	struct tx_ring			*txr = &que->txr;
	struct i40e_filter_program_desc	*FDIR;
	u32				ptype, dtype;
	int				idx;

	/* check if ATR is enabled and sample rate */
	if ((!i40e_enable_fdir) || (!txr->atr_rate))
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
i40e_set_promisc(struct i40e_vsi *vsi)
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
i40e_add_multi(struct i40e_vsi *vsi)
{
	struct	ifmultiaddr	*ifma;
	struct ifnet		*ifp = vsi->ifp;
	struct i40e_hw		*hw = vsi->hw;
	int			mcnt = 0, flags;

	IOCTL_DEBUGOUT("i40e_add_multi: begin");

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
		i40e_del_hw_filters(vsi, mcnt);
		i40e_aq_set_vsi_multicast_promiscuous(hw,
		    vsi->seid, TRUE, NULL);
		return;
	}

	mcnt = 0;
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		i40e_add_mc_filter(vsi,
		    (u8*)LLADDR((struct sockaddr_dl *) ifma->ifma_addr));
		mcnt++;
	}
	if_maddr_runlock(ifp);
	if (mcnt > 0) {
		flags = (I40E_FILTER_ADD | I40E_FILTER_USED | I40E_FILTER_MC);
		i40e_add_hw_filters(vsi, flags, mcnt);
	}

	IOCTL_DEBUGOUT("i40e_add_multi: end");
	return;
}

static void
i40e_del_multi(struct i40e_vsi *vsi)
{
	struct ifnet		*ifp = vsi->ifp;
	struct ifmultiaddr	*ifma;
	struct i40e_mac_filter	*f;
	int			mcnt = 0;
	bool		match = FALSE;

	IOCTL_DEBUGOUT("i40e_del_multi: begin");

	/* Search for removed multicast addresses */
	if_maddr_rlock(ifp);
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if ((f->flags & I40E_FILTER_USED) && (f->flags & I40E_FILTER_MC)) {
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
				f->flags |= I40E_FILTER_DEL;
				mcnt++;
			}
		}
	}
	if_maddr_runlock(ifp);

	if (mcnt > 0)
		i40e_del_hw_filters(vsi, mcnt);
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
i40e_local_timer(void *arg)
{
	struct i40e_pf		*pf = arg;
	struct i40e_hw		*hw = &pf->hw;
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;
	device_t		dev = pf->dev;
	int			hung = 0;
	u32			mask;

	mtx_assert(&pf->pf_mtx, MA_OWNED);

	/* Fire off the adminq task */
	taskqueue_enqueue(pf->tq, &pf->adminq);

	/* Update stats */
	i40e_update_stats_counters(pf);

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
		if (que->busy == I40E_QUEUE_HUNG) {
			++hung;
			/* Mark the queue as inactive */
			vsi->active_queues &= ~((u64)1 << que->me);
			continue;
		} else {
			/* Check if we've come back from hung */
			if ((vsi->active_queues & ((u64)1 << que->me)) == 0)
				vsi->active_queues |= ((u64)1 << que->me);
		}
		if (que->busy >= I40E_MAX_TX_BUSY) {
			device_printf(dev,"Warning queue %d "
			    "appears to be hung!\n", i);
			que->busy = I40E_QUEUE_HUNG;
			++hung;
		}
	}
	/* Only reinit if all queues show hung */
	if (hung == vsi->num_queues)
		goto hung;

	callout_reset(&pf->timer, hz, i40e_local_timer, pf);
	return;

hung:
	device_printf(dev, "Local Timer: HANG DETECT - Resetting!!\n");
	i40e_init_locked(pf);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
i40e_update_link_status(struct i40e_pf *pf)
{
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_hw		*hw = &pf->hw;
	struct ifnet		*ifp = vsi->ifp;
	device_t		dev = pf->dev;
	enum i40e_fc_mode 	fc;


	if (vsi->link_up){ 
		if (vsi->link_active == FALSE) {
			i40e_aq_get_link_info(hw, TRUE, NULL, NULL);
			if (bootverbose) {
				fc = hw->fc.current_mode;
				device_printf(dev,"Link is up %d Gbps %s,"
				    " Flow Control: %s\n",
				    ((vsi->link_speed == I40E_LINK_SPEED_40GB)? 40:10),
				    "Full Duplex", i40e_fc_string[fc]);
			}
			vsi->link_active = TRUE;
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
i40e_stop(struct i40e_pf *pf)
{
	struct i40e_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;

	mtx_assert(&pf->pf_mtx, MA_OWNED);

	INIT_DEBUGOUT("i40e_stop: begin\n");
	i40e_disable_intr(vsi);
	i40e_disable_rings(vsi);

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
i40e_assign_vsi_legacy(struct i40e_pf *pf)
{
	device_t        dev = pf->dev;
	struct 		i40e_vsi *vsi = &pf->vsi;
	struct		i40e_queue *que = vsi->queues;
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
	    i40e_intr, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "Failed to register legacy/msi handler");
		return (error);
	}
	bus_describe_intr(dev, pf->res, pf->tag, "irq0");
	TASK_INIT(&que->tx_task, 0, i40e_deferred_mq_start, que);
	TASK_INIT(&que->task, 0, i40e_handle_que, que);
	que->tq = taskqueue_create_fast("i40e_que", M_NOWAIT,
	    taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
	    device_get_nameunit(dev));
	TASK_INIT(&pf->adminq, 0, i40e_do_adminq, pf);
	pf->tq = taskqueue_create_fast("i40e_adm", M_NOWAIT,
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
i40e_assign_vsi_msix(struct i40e_pf *pf)
{
	device_t	dev = pf->dev;
	struct 		i40e_vsi *vsi = &pf->vsi;
	struct 		i40e_queue *que = vsi->queues;
	struct		tx_ring	 *txr;
	int 		error, rid, vector = 0;

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
	    i40e_msix_adminq, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "Failed to register Admin que handler");
		return (error);
	}
	bus_describe_intr(dev, pf->res, pf->tag, "aq");
	pf->admvec = vector;
	/* Tasklet for Admin Queue */
	TASK_INIT(&pf->adminq, 0, i40e_do_adminq, pf);
	pf->tq = taskqueue_create_fast("i40e_adm", M_NOWAIT,
	    taskqueue_thread_enqueue, &pf->tq);
	taskqueue_start_threads(&pf->tq, 1, PI_NET, "%s adminq",
	    device_get_nameunit(pf->dev));
	++vector;

	/* Now set up the stations */
	for (int i = 0; i < vsi->num_queues; i++, vector++, que++) {
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
		    i40e_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register que handler");
			return (error);
		}
		bus_describe_intr(dev, que->res, que->tag, "q%d", i);
		/* Bind the vector to a CPU */
		bus_bind_intr(dev, que->res, i);
		que->msix = vector;
		TASK_INIT(&que->tx_task, 0, i40e_deferred_mq_start, que);
		TASK_INIT(&que->task, 0, i40e_handle_que, que);
		que->tq = taskqueue_create_fast("i40e_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
		taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
		    device_get_nameunit(pf->dev));
	}

	return (0);
}


/*
 * Allocate MSI/X vectors
 */
static int
i40e_init_msix(struct i40e_pf *pf)
{
	device_t dev = pf->dev;
	int rid, want, vectors, queues, available;

	/* Override by tuneable */
	if (i40e_enable_msix == 0)
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
	rid = PCIR_BAR(I40E_BAR);
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
	if ((i40e_max_queues != 0) && (i40e_max_queues <= queues)) 
		queues = i40e_max_queues;

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
		return (vectors);
	}
msi:
       	vectors = pci_msi_count(dev);
	pf->vsi.num_queues = 1;
	pf->msix = 1;
	i40e_max_queues = 1;
	i40e_enable_msix = 0;
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
i40e_configure_msix(struct i40e_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct i40e_vsi *vsi = &pf->vsi;
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
	wr32(hw, I40E_PFINT_ITR0(I40E_RX_ITR), 0x003E);

	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTL0_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTL0_INTENA_MSK_MASK);

	wr32(hw, I40E_PFINT_STAT_CTL0, 0);

	/* Next configure the queues */
	for (int i = 0; i < vsi->num_queues; i++, vector++) {
		wr32(hw, I40E_PFINT_DYN_CTLN(i), i);
		wr32(hw, I40E_PFINT_LNKLSTN(i), i);

		reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK |
		(I40E_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
		(i << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_TX << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT);
		wr32(hw, I40E_QINT_RQCTL(i), reg);

		reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
		(I40E_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
		((i+1) << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_RX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
		if (i == (vsi->num_queues - 1))
			reg |= (I40E_QUEUE_EOL
			    << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
		wr32(hw, I40E_QINT_TQCTL(i), reg);
	}
}

/*
 * Configure for MSI single vector operation 
 */
static void
i40e_configure_legacy(struct i40e_pf *pf)
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
	    | (I40E_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT)
	    | (I40E_QUEUE_TYPE_TX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK
	    | (I40E_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT)
	    | (I40E_QUEUE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
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
i40e_configure_itr(struct i40e_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;

	vsi->rx_itr_setting = i40e_rx_itr;
	if (i40e_dynamic_rx_itr)
		vsi->rx_itr_setting |= I40E_ITR_DYNAMIC;
	vsi->tx_itr_setting = i40e_tx_itr;
	if (i40e_dynamic_tx_itr)
		vsi->tx_itr_setting |= I40E_ITR_DYNAMIC;
	
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring	*txr = &que->txr;
		struct rx_ring 	*rxr = &que->rxr;

		wr32(hw, I40E_PFINT_ITRN(I40E_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = I40E_AVE_LATENCY;
		wr32(hw, I40E_PFINT_ITRN(I40E_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = I40E_AVE_LATENCY;
	}
}


static int
i40e_allocate_pci_resources(struct i40e_pf *pf)
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
	pf->hw.hw_addr = (u8 *) &pf->osdep.mem_bus_space_handle;

	pf->hw.back = &pf->osdep;

	/*
	** Now setup MSI or MSI/X, should
	** return us the number of supported
	** vectors. (Will be 1 for MSI)
	*/
	pf->msix = i40e_init_msix(pf);
	return (0);
}

static void
i40e_free_pci_resources(struct i40e_pf * pf)
{
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;
	device_t		dev = pf->dev;
	int			rid, memrid;

	memrid = PCIR_BAR(I40E_BAR);

	/* We may get here before stations are setup */
	if ((!i40e_enable_msix) || (que == NULL))
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


/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
i40e_setup_interface(device_t dev, struct i40e_vsi *vsi)
{
	struct ifnet		*ifp;
	struct i40e_hw		*hw = vsi->hw;
	struct i40e_queue	*que = vsi->queues;
	struct i40e_aq_get_phy_abilities_resp abilities_resp;
	enum i40e_status_code aq_error = 0;

	INIT_DEBUGOUT("i40e_setup_interface: begin");

	ifp = vsi->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = 4000000000;  // ??
	ifp->if_init = i40e_init;
	ifp->if_softc = vsi;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = i40e_ioctl;

	ifp->if_transmit = i40e_mq_start;

	ifp->if_qflush = i40e_qflush;

	ifp->if_snd.ifq_maxlen = que->num_desc - 2;

	ether_ifattach(ifp, hw->mac.addr);

	vsi->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

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
	** using vlans directly on the i40e driver you can
	** enable this and get full hardware tag filtering.
	*/
	ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&vsi->media, IFM_IMASK, i40e_media_change,
		     i40e_media_status);

	aq_error = i40e_aq_get_phy_capabilities(hw, FALSE, TRUE, &abilities_resp, NULL);
	if (aq_error) {
		printf("Error getting supported media types, AQ error %d\n", aq_error);
		return (EPERM);
	}

	/* Display supported media types */
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_100BASE_TX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_100_TX, 0, NULL);

	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_1000BASE_T))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_T, 0, NULL);

	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1_CU) ||
	    abilities_resp.phy_type & (1 << I40E_PHY_TYPE_10GBASE_SFPP_CU))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX, 0, NULL);
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_10GBASE_SR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SR, 0, NULL);
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_10GBASE_LR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_LR, 0, NULL);
		
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4_CU) ||
	    abilities_resp.phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_CR4, 0, NULL);
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_40GBASE_SR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	if (abilities_resp.phy_type & (1 << I40E_PHY_TYPE_40GBASE_LR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_LR4, 0, NULL);

	/* Use autoselect media by default */
	ifmedia_add(&vsi->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&vsi->media, IFM_ETHER | IFM_AUTO);

	return (0);
}

static bool
i40e_config_link(struct i40e_hw *hw)
{
	bool check;

	i40e_aq_get_link_info(hw, TRUE, NULL, NULL);
	check = i40e_get_link_status(hw);
#ifdef I40E_DEBUG
	printf("Link is %s\n", check ? "up":"down");
#endif
	return (check);
}

/*********************************************************************
 *
 *  Initialize this VSI 
 *
 **********************************************************************/
static int
i40e_setup_vsi(struct i40e_vsi *vsi)
{
	struct i40e_hw	*hw = vsi->hw;
	device_t 	dev = vsi->dev;
	struct i40e_aqc_get_switch_config_resp *sw_config;
	struct i40e_vsi_context	ctxt;
	u8	aq_buf[I40E_AQ_LARGE_BUF];
	int	ret = I40E_SUCCESS;
	u16	next = 0;

	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;
	ret = i40e_aq_get_switch_config(hw, sw_config,
	    sizeof(aq_buf), &next, NULL);
	if (ret) {
		device_printf(dev,"aq_get_switch_config failed!!\n");
		return (ret);
	}
#ifdef I40E_DEBUG
	printf("Switch config: header reported: %d in structure, %d total\n",
    	    sw_config->header.num_reported, sw_config->header.num_total);
	printf("type=%d seid=%d uplink=%d downlink=%d\n",
	    sw_config->element[0].element_type,
	    sw_config->element[0].seid,
	    sw_config->element[0].uplink_seid,
	    sw_config->element[0].downlink_seid);
#endif
	/* Save off this important value */
	vsi->seid = sw_config->element[0].seid;

	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = vsi->seid;
	ctxt.pf_num = hw->pf_id;
	ret = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
	if (ret) {
		device_printf(dev,"get vsi params failed %x!!\n", ret);
		return (ret);
	}
#ifdef I40E_DEBUG
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
	i40e_vsi_reset_stats(vsi);
	vsi->hw_filters_add = 0;
	vsi->hw_filters_del = 0;

	ret = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (ret)
		device_printf(dev,"update vsi params failed %x!!\n",
		   hw->aq.asq_last_status);
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
i40e_initialize_vsi(struct i40e_vsi *vsi)
{
	struct i40e_queue	*que = vsi->queues;
	device_t		dev = vsi->dev;
	struct i40e_hw		*hw = vsi->hw;
	int			err = 0;


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
		tctx.base = (txr->dma.pa/128);
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
		i40e_flush(hw);

		/* Do ring (re)init */
		i40e_init_tx_ring(que);

		/* Next setup the HMC RX Context  */
		if (vsi->max_frame_size <= 2048)
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
		rctx.base = (rxr->dma.pa/128);
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
		err = i40e_init_rx_ring(que);
		if (err) {
			device_printf(dev, "Fail in init_rx_ring %d\n", i);
			break;
		}
		wr32(vsi->hw, I40E_QRX_TAIL(que->me), 0);
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
i40e_free_vsi(struct i40e_vsi *vsi)
{
	struct i40e_pf		*pf = (struct i40e_pf *)vsi->back;
	struct i40e_queue	*que = vsi->queues;
	struct i40e_mac_filter *f;

	/* Free station queues */
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		struct rx_ring *rxr = &que->rxr;
	
		if (!mtx_initialized(&txr->mtx)) /* uninitialized */
			continue;
		I40E_TX_LOCK(txr);
		i40e_free_que_tx(que);
		if (txr->base)
			i40e_free_dma(&pf->hw, &txr->dma);
		I40E_TX_UNLOCK(txr);
		I40E_TX_LOCK_DESTROY(txr);

		if (!mtx_initialized(&rxr->mtx)) /* uninitialized */
			continue;
		I40E_RX_LOCK(rxr);
		i40e_free_que_rx(que);
		if (rxr->base)
			i40e_free_dma(&pf->hw, &rxr->dma);
		I40E_RX_UNLOCK(rxr);
		I40E_RX_LOCK_DESTROY(rxr);
		
	}
	free(vsi->queues, M_DEVBUF);

	/* Free VSI filter list */
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
i40e_setup_stations(struct i40e_pf *pf)
{
	device_t		dev = pf->dev;
	struct i40e_vsi		*vsi;
	struct i40e_queue	*que;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	int 			rsize, tsize;
	int			error = I40E_SUCCESS;

	vsi = &pf->vsi;
	vsi->back = (void *)pf;
	vsi->hw = &pf->hw;
	vsi->id = 0;
	vsi->num_vlans = 0;

	/* Get memory for the station queues */
        if (!(vsi->queues =
            (struct i40e_queue *) malloc(sizeof(struct i40e_queue) *
            vsi->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                device_printf(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                goto early;
        }

	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		que->num_desc = i40e_ringsz;
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
		if (i40e_allocate_dma(&pf->hw,
		    &txr->dma, tsize, DBA_ALIGN)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		txr->base = (struct i40e_tx_desc *)txr->dma.va;
		bzero((void *)txr->base, tsize);
       		/* Now allocate transmit soft structs for the ring */
       		if (i40e_allocate_tx_data(que)) {
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

		if (i40e_allocate_dma(&pf->hw,
		    &rxr->dma, rsize, 4096)) {
			device_printf(dev,
			    "Unable to allocate RX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		rxr->base = (union i40e_rx_desc *)rxr->dma.va;
		bzero((void *)rxr->base, rsize);

        	/* Allocate receive soft structs for the ring*/
		if (i40e_allocate_rx_data(que)) {
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
			i40e_free_dma(&pf->hw, &rxr->dma);
		if (txr->base)
			i40e_free_dma(&pf->hw, &txr->dma);
	}

early:
	return (error);
}

/*
** Provide a update to the queue RX
** interrupt moderation value.
*/
static void
i40e_set_queue_rx_itr(struct i40e_queue *que)
{
	struct i40e_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct rx_ring	*rxr = &que->rxr;
	u16		rx_itr;
	u16		rx_latency = 0;
	int		rx_bytes;


	/* Idle, do nothing */
	if (rxr->bytes == 0)
		return;

	if (i40e_dynamic_rx_itr) {
		rx_bytes = rxr->bytes/rxr->itr;
		rx_itr = rxr->itr;

		/* Adjust latency range */
		switch (rxr->latency) {
		case I40E_LOW_LATENCY:
			if (rx_bytes > 10) {
				rx_latency = I40E_AVE_LATENCY;
				rx_itr = I40E_ITR_20K;
			}
			break;
		case I40E_AVE_LATENCY:
			if (rx_bytes > 20) {
				rx_latency = I40E_BULK_LATENCY;
				rx_itr = I40E_ITR_8K;
			} else if (rx_bytes <= 10) {
				rx_latency = I40E_LOW_LATENCY;
				rx_itr = I40E_ITR_100K;
			}
			break;
		case I40E_BULK_LATENCY:
			if (rx_bytes <= 20) {
				rx_latency = I40E_AVE_LATENCY;
				rx_itr = I40E_ITR_20K;
			}
			break;
       		 }

		rxr->latency = rx_latency;

		if (rx_itr != rxr->itr) {
			/* do an exponential smoothing */
			rx_itr = (10 * rx_itr * rxr->itr) /
			    ((9 * rx_itr) + rxr->itr);
			rxr->itr = rx_itr & I40E_MAX_ITR;
			wr32(hw, I40E_PFINT_ITRN(I40E_RX_ITR,
			    que->me), rxr->itr);
		}
	} else { /* We may have have toggled to non-dynamic */
		if (vsi->rx_itr_setting & I40E_ITR_DYNAMIC)
			vsi->rx_itr_setting = i40e_rx_itr;
		/* Update the hardware if needed */
		if (rxr->itr != vsi->rx_itr_setting) {
			rxr->itr = vsi->rx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(I40E_RX_ITR,
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
i40e_set_queue_tx_itr(struct i40e_queue *que)
{
	struct i40e_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	u16		tx_itr;
	u16		tx_latency = 0;
	int		tx_bytes;


	/* Idle, do nothing */
	if (txr->bytes == 0)
		return;

	if (i40e_dynamic_tx_itr) {
		tx_bytes = txr->bytes/txr->itr;
		tx_itr = txr->itr;

		switch (txr->latency) {
		case I40E_LOW_LATENCY:
			if (tx_bytes > 10) {
				tx_latency = I40E_AVE_LATENCY;
				tx_itr = I40E_ITR_20K;
			}
			break;
		case I40E_AVE_LATENCY:
			if (tx_bytes > 20) {
				tx_latency = I40E_BULK_LATENCY;
				tx_itr = I40E_ITR_8K;
			} else if (tx_bytes <= 10) {
				tx_latency = I40E_LOW_LATENCY;
				tx_itr = I40E_ITR_100K;
			}
			break;
		case I40E_BULK_LATENCY:
			if (tx_bytes <= 20) {
				tx_latency = I40E_AVE_LATENCY;
				tx_itr = I40E_ITR_20K;
			}
			break;
		}

		txr->latency = tx_latency;

		if (tx_itr != txr->itr) {
       	         /* do an exponential smoothing */
			tx_itr = (10 * tx_itr * txr->itr) /
			    ((9 * tx_itr) + txr->itr);
			txr->itr = tx_itr & I40E_MAX_ITR;
			wr32(hw, I40E_PFINT_ITRN(I40E_TX_ITR,
			    que->me), txr->itr);
		}

	} else { /* We may have have toggled to non-dynamic */
		if (vsi->tx_itr_setting & I40E_ITR_DYNAMIC)
			vsi->tx_itr_setting = i40e_tx_itr;
		/* Update the hardware if needed */
		if (txr->itr != vsi->tx_itr_setting) {
			txr->itr = vsi->tx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(I40E_TX_ITR,
			    que->me), txr->itr);
		}
	}
	txr->bytes = 0;
	txr->packets = 0;
	return;
}


static void
i40e_add_hw_stats(struct i40e_pf *pf)
{
	device_t dev = pf->dev;
	struct i40e_vsi *vsi = &pf->vsi;
	struct i40e_queue *queues = vsi->queues;
	struct i40e_eth_stats *vsi_stats = &vsi->eth_stats;
	struct i40e_hw_port_stats *pf_stats = &pf->stats;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	struct sysctl_oid *vsi_node, *queue_node;
	struct sysctl_oid_list *vsi_list, *queue_list;

	struct tx_ring *txr;
	struct rx_ring *rxr;

	/* Driver statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
			CTLFLAG_RD, &pf->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "admin_irq",
			CTLFLAG_RD, &pf->admin_irq,
			"Admin Queue IRQ Handled");

	/* VSI statistics */
#define QUEUE_NAME_LEN 32
	char queue_namebuf[QUEUE_NAME_LEN];
	
	// ERJ: Only one vsi now, re-do when >1 VSI enabled
	// snprintf(vsi_namebuf, QUEUE_NAME_LEN, "vsi%d", vsi->info.stat_counter_idx);
	vsi_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "vsi",
				   CTLFLAG_RD, NULL, "VSI-specific stats");
	vsi_list = SYSCTL_CHILDREN(vsi_node);

	i40e_add_sysctls_eth_stats(ctx, vsi_list, vsi_stats);

	/* Queue statistics */
	for (int q = 0; q < vsi->num_queues; q++) {
		snprintf(queue_namebuf, QUEUE_NAME_LEN, "que%d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list, OID_AUTO, queue_namebuf,
					     CTLFLAG_RD, NULL, "Queue #");
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
				CTLFLAG_RD, &(txr->bytes),
				"Queue Bytes Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
				CTLFLAG_RD, &(rxr->rx_packets),
				"Queue Packets Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
				CTLFLAG_RD, &(rxr->rx_bytes),
				"Queue Bytes Received");
	}

	/* MAC stats */
	i40e_add_sysctls_mac_stats(ctx, child, pf_stats);
}

static void
i40e_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_eth_stats *eth_stats)
{
	struct i40e_sysctl_info ctls[] =
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
		{&eth_stats->tx_discards, "tx_discards", "Discarded TX packets"},
		// end
		{0,0,0}
	};

	struct i40e_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

static void
i40e_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_hw_port_stats *stats)
{
	struct sysctl_oid *stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
				    CTLFLAG_RD, NULL, "Mac Statistics");
	struct sysctl_oid_list *stat_list = SYSCTL_CHILDREN(stat_node);

	struct i40e_eth_stats *eth_stats = &stats->eth;
	i40e_add_sysctls_eth_stats(ctx, stat_list, eth_stats);

	struct i40e_sysctl_info ctls[] = 
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

	struct i40e_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

/*
** i40e_config_rss - setup RSS 
**  - note this is done for the single vsi
*/
static void i40e_config_rss(struct i40e_vsi *vsi)
{
	struct i40e_pf	*pf = (struct i40e_pf *)vsi->back;
	struct i40e_hw	*hw = vsi->hw;
	u32		lut = 0;
	u64		set_hena, hena;
	int		i, j;

	static const u32 seed[I40E_PFQF_HKEY_MAX_INDEX + 1] = {0x41b01687,
	    0x183cfd8c, 0xce880440, 0x580cbc3c, 0x35897377,
	    0x328b25e1, 0x4fa98922, 0xb7d90c14, 0xd5bad70d,
	    0xcd15a2c1, 0xe8580225, 0x4a1e9d11, 0xfe5731be};

	/* Fill out hash function seed */
	for (i = 0; i <= I40E_PFQF_HKEY_MAX_INDEX; i++)
                wr32(hw, I40E_PFQF_HKEY(i), seed[i]);

	/* Enable PCTYPES for RSS: */
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

	hena = (u64)rd32(hw, I40E_PFQF_HENA(0)) |
	    ((u64)rd32(hw, I40E_PFQF_HENA(1)) << 32);
	hena |= set_hena;
	wr32(hw, I40E_PFQF_HENA(0), (u32)hena);
	wr32(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = j = 0; i < pf->hw.func_caps.rss_table_size; i++, j++) {
		if (j == vsi->num_queues)
			j = 0;
		/* lut = 4-byte sliding window of 4 lut entries */
		lut = (lut << 8) | (j &
		    ((0x1 << pf->hw.func_caps.rss_table_entry_width) - 1));
		/* On i = 3, we have 4 entries in lut; write to the register */
		if ((i & 3) == 3)
			wr32(hw, I40E_PFQF_HLUT(i >> 2), lut);
	}
	i40e_flush(hw);
}


/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
i40e_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct i40e_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct i40e_pf	*pf = (struct i40e_pf *)vsi->back;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	I40E_PF_LOCK(pf);
	++vsi->num_vlans;
	i40e_add_filter(vsi, hw->mac.addr, vtag);
	I40E_PF_UNLOCK(pf);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
i40e_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct i40e_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct i40e_pf	*pf = (struct i40e_pf *)vsi->back;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	I40E_PF_LOCK(pf);
	--vsi->num_vlans;
	i40e_del_filter(vsi, hw->mac.addr, vtag);
	I40E_PF_UNLOCK(pf);
}

/*
** This routine updates vlan filters, called by init
** it scans the filter table and then updates the hw
** after a soft reset.
*/
static void
i40e_setup_vlan_filters(struct i40e_vsi *vsi)
{
	struct i40e_mac_filter	*f;
	int			cnt = 0, flags;

	if (vsi->num_vlans == 0)
		return;
	/*
	** Scan the filter list for vlan entries,
	** mark them for addition and then call
	** for the AQ update.
	*/
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (f->flags & I40E_FILTER_VLAN) {
			f->flags |=
			    (I40E_FILTER_ADD |
			    I40E_FILTER_USED);
			cnt++;
		}
	}
	if (cnt == 0) {
		printf("setup vlan: no filters found!\n");
		return;
	}
	flags = I40E_FILTER_VLAN;
	flags |= (I40E_FILTER_ADD | I40E_FILTER_USED);
	i40e_add_hw_filters(vsi, flags, cnt);
	return;
}

/*
** Initialize filter list and add filters that the hardware
** needs to know about.
*/
static void
i40e_init_filters(struct i40e_vsi *vsi)
{
	/* Add broadcast address */
	u8 bc[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
	i40e_add_filter(vsi, bc, I40E_VLAN_ANY);
}

/*
** This routine adds mulicast filters
*/
static void
i40e_add_mc_filter(struct i40e_vsi *vsi, u8 *macaddr)
{
	struct i40e_mac_filter *f;

	/* Does one already exist */
	f = i40e_find_filter(vsi, macaddr, I40E_VLAN_ANY);
	if (f != NULL)
		return;

	f = i40e_get_filter(vsi);
	if (f == NULL) {
		printf("WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = I40E_VLAN_ANY;
	f->flags |= (I40E_FILTER_ADD | I40E_FILTER_USED
	    | I40E_FILTER_MC);

	return;
}

/*
** This routine adds macvlan filters
*/
static void
i40e_add_filter(struct i40e_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter	*f, *tmp;
	device_t		dev = vsi->dev;

	DEBUGOUT("i40e_add_filter: begin");

	/* Does one already exist */
	f = i40e_find_filter(vsi, macaddr, vlan);
	if (f != NULL)
		return;
	/*
	** Is this the first vlan being registered, if so we
	** need to remove the ANY filter that indicates we are
	** not in a vlan, and replace that with a 0 filter.
	*/
	if ((vlan != I40E_VLAN_ANY) && (vsi->num_vlans == 1)) {
		tmp = i40e_find_filter(vsi, macaddr, I40E_VLAN_ANY);
		if (tmp != NULL) {
			i40e_del_filter(vsi, macaddr, I40E_VLAN_ANY);
			i40e_add_filter(vsi, macaddr, 0);
		}
	}

	f = i40e_get_filter(vsi);
	if (f == NULL) {
		device_printf(dev, "WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = vlan;
	f->flags |= (I40E_FILTER_ADD | I40E_FILTER_USED);
	if (f->vlan != I40E_VLAN_ANY)
		f->flags |= I40E_FILTER_VLAN;

	i40e_add_hw_filters(vsi, f->flags, 1);
	return;
}

static void
i40e_del_filter(struct i40e_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter *f;

	f = i40e_find_filter(vsi, macaddr, vlan);
	if (f == NULL)
		return;

	f->flags |= I40E_FILTER_DEL;
	i40e_del_hw_filters(vsi, 1);

	/* Check if this is the last vlan removal */
	if (vlan != I40E_VLAN_ANY && vsi->num_vlans == 0) {
		/* Switch back to a non-vlan filter */
		i40e_del_filter(vsi, macaddr, 0);
		i40e_add_filter(vsi, macaddr, I40E_VLAN_ANY);
	}
	return;
}

/*
** Find the filter with both matching mac addr and vlan id
*/
static struct i40e_mac_filter *
i40e_find_filter(struct i40e_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct i40e_mac_filter	*f;
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
i40e_add_hw_filters(struct i40e_vsi *vsi, int flags, int cnt)
{
	struct i40e_aqc_add_macvlan_element_data *a, *b;
	struct i40e_mac_filter	*f;
	struct i40e_hw	*hw = vsi->hw;
	device_t	dev = vsi->dev;
	int		err, j = 0;

	a = malloc(sizeof(struct i40e_aqc_add_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "add hw filter failed to get memory\n");
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
			b->vlan_tag =
			    (f->vlan == I40E_VLAN_ANY ? 0 : f->vlan);
			b->flags = I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
			f->flags &= ~I40E_FILTER_ADD;
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		err = i40e_aq_add_macvlan(hw, vsi->seid, a, j, NULL);
		if (err) 
			device_printf(dev, "aq_add_macvlan failure %d\n",
			    hw->aq.asq_last_status);
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
i40e_del_hw_filters(struct i40e_vsi *vsi, int cnt)
{
	struct i40e_aqc_remove_macvlan_element_data *d, *e;
	struct i40e_hw		*hw = vsi->hw;
	device_t		dev = vsi->dev;
	struct i40e_mac_filter	*f, *f_temp;
	int			err, j = 0;

	DEBUGOUT("i40e_del_hw_filters: begin\n");

	d = malloc(sizeof(struct i40e_aqc_remove_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		printf("del hw filter failed to get memory\n");
		return;
	}

	SLIST_FOREACH_SAFE(f, &vsi->ftl, next, f_temp) {
		if (f->flags & I40E_FILTER_DEL) {
			e = &d[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, e->mac_addr, ETHER_ADDR_LEN);
			e->vlan_tag = (f->vlan == I40E_VLAN_ANY ? 0 : f->vlan);
			e->flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
			/* delete entry from vsi list */
			SLIST_REMOVE(&vsi->ftl, f, i40e_mac_filter, next);
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

	DEBUGOUT("i40e_del_hw_filters: end\n");
	return;
}


static void
i40e_enable_rings(struct i40e_vsi *vsi)
{
	struct i40e_hw	*hw = vsi->hw;
	u32		reg;

	for (int i = 0; i < vsi->num_queues; i++) {
		i40e_pre_tx_queue_cfg(hw, i, TRUE);

		reg = rd32(hw, I40E_QTX_ENA(i));
		reg |= I40E_QTX_ENA_QENA_REQ_MASK |
		    I40E_QTX_ENA_QENA_STAT_MASK;
		wr32(hw, I40E_QTX_ENA(i), reg);
		/* Verify the enable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QTX_ENA(i));
			if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
				break;
			i40e_msec_delay(10);
		}
		if ((reg & I40E_QTX_ENA_QENA_STAT_MASK) == 0)
			printf("TX queue %d disabled!\n", i);

		reg = rd32(hw, I40E_QRX_ENA(i));
		reg |= I40E_QRX_ENA_QENA_REQ_MASK |
		    I40E_QRX_ENA_QENA_STAT_MASK;
		wr32(hw, I40E_QRX_ENA(i), reg);
		/* Verify the enable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QRX_ENA(i));
			if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
				break;
			i40e_msec_delay(10);
		}
		if ((reg & I40E_QRX_ENA_QENA_STAT_MASK) == 0)
			printf("RX queue %d disabled!\n", i);
	}
}

static void
i40e_disable_rings(struct i40e_vsi *vsi)
{
	struct i40e_hw	*hw = vsi->hw;
	u32		reg;

	for (int i = 0; i < vsi->num_queues; i++) {
		i40e_pre_tx_queue_cfg(hw, i, FALSE);
		i40e_usec_delay(500);

		reg = rd32(hw, I40E_QTX_ENA(i));
		reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QTX_ENA(i), reg);
		/* Verify the disable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QTX_ENA(i));
			if (!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
				break;
			i40e_msec_delay(10);
		}
		if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
			printf("TX queue %d still enabled!\n", i);

		reg = rd32(hw, I40E_QRX_ENA(i));
		reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
		wr32(hw, I40E_QRX_ENA(i), reg);
		/* Verify the disable took */
		for (int j = 0; j < 10; j++) {
			reg = rd32(hw, I40E_QRX_ENA(i));
			if (!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
				break;
			i40e_msec_delay(10);
		}
		if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
			printf("RX queue %d still enabled!\n", i);
	}
}

/**
 * i40e_handle_mdd_event
 *
 * Called from interrupt handler to identify possibly malicious vfs
 * (But also detects events from the PF, as well)
 **/
static void i40e_handle_mdd_event(struct i40e_pf *pf)
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
	i40e_flush(hw);
}

static void
i40e_enable_intr(struct i40e_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct i40e_queue	*que = vsi->queues;

	if (i40e_enable_msix) {
		i40e_enable_adminq(hw);
		for (int i = 0; i < vsi->num_queues; i++, que++)
			i40e_enable_queue(hw, que->me);
	} else
		i40e_enable_legacy(hw);
}

static void
i40e_disable_intr(struct i40e_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct i40e_queue	*que = vsi->queues;

	if (i40e_enable_msix) {
		i40e_disable_adminq(hw);
		for (int i = 0; i < vsi->num_queues; i++, que++)
			i40e_disable_queue(hw, que->me);
	} else
		i40e_disable_legacy(hw);
}

static void
i40e_enable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
	i40e_flush(hw);
	return;
}

static void
i40e_disable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

	return;
}

static void
i40e_enable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTLN_INTENA_MASK |
	    I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
	    (I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);
}

static void
i40e_disable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = I40E_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);

	return;
}

static void
i40e_enable_legacy(struct i40e_hw *hw)
{
	u32		reg;
	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
}

static void
i40e_disable_legacy(struct i40e_hw *hw)
{
	u32		reg;

	reg = I40E_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

	return;
}

static void
i40e_update_stats_counters(struct i40e_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct i40e_vsi *vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;

	struct i40e_hw_port_stats *nsd = &pf->stats;
	struct i40e_hw_port_stats *osd = &pf->stats_offsets;

	/* Update hw stats */
	i40e_stat_update32(hw, I40E_GLPRT_CRCERRS(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->crc_errors, &nsd->crc_errors);
	i40e_stat_update32(hw, I40E_GLPRT_ILLERRC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->illegal_bytes, &nsd->illegal_bytes);
	i40e_stat_update48(hw, I40E_GLPRT_GORCH(hw->port),
			   I40E_GLPRT_GORCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_bytes, &nsd->eth.rx_bytes);
	i40e_stat_update48(hw, I40E_GLPRT_GOTCH(hw->port),
			   I40E_GLPRT_GOTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_bytes, &nsd->eth.tx_bytes);
	i40e_stat_update32(hw, I40E_GLPRT_RDPC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_discards,
			   &nsd->eth.rx_discards);
	i40e_stat_update32(hw, I40E_GLPRT_TDPC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_discards,
			   &nsd->eth.tx_discards);
	i40e_stat_update48(hw, I40E_GLPRT_UPRCH(hw->port),
			   I40E_GLPRT_UPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_unicast,
			   &nsd->eth.rx_unicast);
	i40e_stat_update48(hw, I40E_GLPRT_UPTCH(hw->port),
			   I40E_GLPRT_UPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_unicast,
			   &nsd->eth.tx_unicast);
	i40e_stat_update48(hw, I40E_GLPRT_MPRCH(hw->port),
			   I40E_GLPRT_MPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_multicast,
			   &nsd->eth.rx_multicast);
	i40e_stat_update48(hw, I40E_GLPRT_MPTCH(hw->port),
			   I40E_GLPRT_MPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_multicast,
			   &nsd->eth.tx_multicast);
	i40e_stat_update48(hw, I40E_GLPRT_BPRCH(hw->port),
			   I40E_GLPRT_BPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_broadcast,
			   &nsd->eth.rx_broadcast);
	i40e_stat_update48(hw, I40E_GLPRT_BPTCH(hw->port),
			   I40E_GLPRT_BPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_broadcast,
			   &nsd->eth.tx_broadcast);

	i40e_stat_update32(hw, I40E_GLPRT_TDOLD(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_dropped_link_down,
			   &nsd->tx_dropped_link_down);
	i40e_stat_update32(hw, I40E_GLPRT_MLFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_local_faults,
			   &nsd->mac_local_faults);
	i40e_stat_update32(hw, I40E_GLPRT_MRFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_remote_faults,
			   &nsd->mac_remote_faults);
	i40e_stat_update32(hw, I40E_GLPRT_RLEC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_length_errors,
			   &nsd->rx_length_errors);

	/* Flow control (LFC) stats */
	i40e_stat_update32(hw, I40E_GLPRT_LXONRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_rx, &nsd->link_xon_rx);
	i40e_stat_update32(hw, I40E_GLPRT_LXONTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_tx, &nsd->link_xon_tx);
	i40e_stat_update32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_rx, &nsd->link_xoff_rx);
	i40e_stat_update32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_tx, &nsd->link_xoff_tx);

	/* Priority flow control stats */
#if 0
	for (int i = 0; i < 8; i++) {
		i40e_stat_update32(hw, I40E_GLPRT_PXONRXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_rx[i],
				   &nsd->priority_xon_rx[i]);
		i40e_stat_update32(hw, I40E_GLPRT_PXONTXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_tx[i],
				   &nsd->priority_xon_tx[i]);
		i40e_stat_update32(hw, I40E_GLPRT_PXOFFTXC(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xoff_tx[i],
				   &nsd->priority_xoff_tx[i]);
		i40e_stat_update32(hw,
				   I40E_GLPRT_RXON2OFFCNT(hw->port, i),
				   pf->stat_offsets_loaded,
				   &osd->priority_xon_2_xoff[i],
				   &nsd->priority_xon_2_xoff[i]);
	}
#endif

	/* Packet size stats rx */
	i40e_stat_update48(hw, I40E_GLPRT_PRC64H(hw->port),
			   I40E_GLPRT_PRC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_64, &nsd->rx_size_64);
	i40e_stat_update48(hw, I40E_GLPRT_PRC127H(hw->port),
			   I40E_GLPRT_PRC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_127, &nsd->rx_size_127);
	i40e_stat_update48(hw, I40E_GLPRT_PRC255H(hw->port),
			   I40E_GLPRT_PRC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_255, &nsd->rx_size_255);
	i40e_stat_update48(hw, I40E_GLPRT_PRC511H(hw->port),
			   I40E_GLPRT_PRC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_511, &nsd->rx_size_511);
	i40e_stat_update48(hw, I40E_GLPRT_PRC1023H(hw->port),
			   I40E_GLPRT_PRC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1023, &nsd->rx_size_1023);
	i40e_stat_update48(hw, I40E_GLPRT_PRC1522H(hw->port),
			   I40E_GLPRT_PRC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1522, &nsd->rx_size_1522);
	i40e_stat_update48(hw, I40E_GLPRT_PRC9522H(hw->port),
			   I40E_GLPRT_PRC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_big, &nsd->rx_size_big);

	/* Packet size stats tx */
	i40e_stat_update48(hw, I40E_GLPRT_PTC64H(hw->port),
			   I40E_GLPRT_PTC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_64, &nsd->tx_size_64);
	i40e_stat_update48(hw, I40E_GLPRT_PTC127H(hw->port),
			   I40E_GLPRT_PTC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_127, &nsd->tx_size_127);
	i40e_stat_update48(hw, I40E_GLPRT_PTC255H(hw->port),
			   I40E_GLPRT_PTC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_255, &nsd->tx_size_255);
	i40e_stat_update48(hw, I40E_GLPRT_PTC511H(hw->port),
			   I40E_GLPRT_PTC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_511, &nsd->tx_size_511);
	i40e_stat_update48(hw, I40E_GLPRT_PTC1023H(hw->port),
			   I40E_GLPRT_PTC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1023, &nsd->tx_size_1023);
	i40e_stat_update48(hw, I40E_GLPRT_PTC1522H(hw->port),
			   I40E_GLPRT_PTC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1522, &nsd->tx_size_1522);
	i40e_stat_update48(hw, I40E_GLPRT_PTC9522H(hw->port),
			   I40E_GLPRT_PTC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_big, &nsd->tx_size_big);

	i40e_stat_update32(hw, I40E_GLPRT_RUC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_undersize, &nsd->rx_undersize);
	i40e_stat_update32(hw, I40E_GLPRT_RFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_fragments, &nsd->rx_fragments);
	i40e_stat_update32(hw, I40E_GLPRT_ROC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_oversize, &nsd->rx_oversize);
	i40e_stat_update32(hw, I40E_GLPRT_RJC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_jabber, &nsd->rx_jabber);
	pf->stat_offsets_loaded = true;
	/* End hw stats */

	/* Update vsi stats */
	i40e_update_eth_stats(vsi);

	/* OS statistics */
	// ERJ - these are per-port, update all vsis?
	ifp->if_ierrors = nsd->crc_errors + nsd->illegal_bytes;
}

/*
** Tasklet handler for MSIX Adminq interrupts
**  - do outside interrupt since it might sleep
*/
static void
i40e_do_adminq(void *context, int pending)
{
	struct i40e_pf			*pf = context;
	struct i40e_hw			*hw = &pf->hw;
	struct i40e_vsi			*vsi = &pf->vsi;
	struct i40e_arq_event_info	event;
	i40e_status			ret;
	u32				reg, loop = 0;
	u16				opcode, result;

	event.msg_size = I40E_AQ_BUF_SZ;
	event.msg_buf = malloc(event.msg_size,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!event.msg_buf) {
		printf("Unable to allocate adminq memory\n");
		return;
	}

	/* clean and process any events */
	do {
		ret = i40e_clean_arq_element(hw, &event, &result);
		if (ret)
			break;
		opcode = LE16_TO_CPU(event.desc.opcode);
		switch (opcode) {
		case i40e_aqc_opc_get_link_status:
			vsi->link_up = i40e_config_link(hw);
			i40e_update_link_status(pf);
			break;
		case i40e_aqc_opc_send_msg_to_pf:
			/* process pf/vf communication here */
			break;
		case i40e_aqc_opc_event_lan_overflow:
			break;
		default:
#ifdef I40E_DEBUG
			printf("AdminQ unknown event %x\n", opcode);
#endif
			break;
		}

	} while (result && (loop++ < I40E_ADM_LIMIT));

	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	free(event.msg_buf, M_DEVBUF);

	if (pf->msix > 1)
		i40e_enable_adminq(&pf->hw);
	else
		i40e_enable_intr(vsi);
}

static int
i40e_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf	*pf;
	int		error, input = 0;

	error = sysctl_handle_int(oidp, &input, 0, req);

	if (error || !req->newptr)
		return (error);

	if (input == 1) {
		pf = (struct i40e_pf *)arg1;
		i40e_print_debug_info(pf);
	}

	return (error);
}

static void
i40e_print_debug_info(struct i40e_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct i40e_vsi		*vsi = &pf->vsi;
	struct i40e_queue	*que = vsi->queues;
	struct rx_ring		*rxr = &que->rxr;
	struct tx_ring		*txr = &que->txr;
	u32			reg;	


	printf("Queue irqs = %lx\n", que->irqs);
	printf("AdminQ irqs = %lx\n", pf->admin_irq);
	printf("RX next check = %x\n", rxr->next_check);
	printf("RX not ready = %lx\n", rxr->not_done);
	printf("RX packets = %lx\n", rxr->rx_packets);
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
void i40e_update_eth_stats(struct i40e_vsi *vsi)
{
	struct i40e_pf *pf = (struct i40e_pf *)vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct ifnet *ifp = vsi->ifp;
	struct i40e_eth_stats *es;
	struct i40e_eth_stats *oes;
	u16 stat_idx = vsi->info.stat_counter_idx;

	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;

	/* Gather up the stats that the hw collects */
	i40e_stat_update32(hw, I40E_GLV_TEPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_errors, &es->tx_errors);
	i40e_stat_update32(hw, I40E_GLV_RDPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_discards, &es->rx_discards);

	i40e_stat_update48(hw, I40E_GLV_GORCH(stat_idx),
			   I40E_GLV_GORCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_bytes, &es->rx_bytes);
	i40e_stat_update48(hw, I40E_GLV_UPRCH(stat_idx),
			   I40E_GLV_UPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_unicast, &es->rx_unicast);
	i40e_stat_update48(hw, I40E_GLV_MPRCH(stat_idx),
			   I40E_GLV_MPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_multicast, &es->rx_multicast);
	i40e_stat_update48(hw, I40E_GLV_BPRCH(stat_idx),
			   I40E_GLV_BPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_broadcast, &es->rx_broadcast);

	i40e_stat_update48(hw, I40E_GLV_GOTCH(stat_idx),
			   I40E_GLV_GOTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_bytes, &es->tx_bytes);
	i40e_stat_update48(hw, I40E_GLV_UPTCH(stat_idx),
			   I40E_GLV_UPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_unicast, &es->tx_unicast);
	i40e_stat_update48(hw, I40E_GLV_MPTCH(stat_idx),
			   I40E_GLV_MPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_multicast, &es->tx_multicast);
	i40e_stat_update48(hw, I40E_GLV_BPTCH(stat_idx),
			   I40E_GLV_BPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_broadcast, &es->tx_broadcast);
	vsi->stat_offsets_loaded = true;

	/* Update ifnet stats */
	ifp->if_ipackets = es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast;
	ifp->if_opackets = es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast;
	ifp->if_ibytes = es->rx_bytes;
	ifp->if_obytes = es->tx_bytes;
	ifp->if_imcasts = es->rx_multicast;
	ifp->if_omcasts = es->tx_multicast;

	ifp->if_oerrors = es->tx_errors;
	ifp->if_iqdrops = es->rx_discards;
	ifp->if_noproto = es->rx_unknown_protocol;
	ifp->if_collisions = 0;
}

/**
 * Reset all of the stats for the given pf
 **/
void i40e_pf_reset_stats(struct i40e_pf *pf)
{
	bzero(&pf->stats, sizeof(struct i40e_hw_port_stats));
	bzero(&pf->stats_offsets, sizeof(struct i40e_hw_port_stats));
	pf->stat_offsets_loaded = false;
}

/**
 * Resets all stats of the given vsi
 **/
void i40e_vsi_reset_stats(struct i40e_vsi *vsi)
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
i40e_stat_update48(struct i40e_hw *hw, u32 hireg, u32 loreg,
	bool offset_loaded, u64 *offset, u64 *stat)
{
	u64 new_data;

#if __FreeBSD__ >= 10 && __amd64__
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
i40e_stat_update32(struct i40e_hw *hw, u32 reg,
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
i40e_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	/*
	 * TODO: ensure flow control is disabled if
	 * priority flow control is enabled
	 *
	 * TODO: ensure tx CRC by hardware should be enabled
	 * if tx flow control is enabled.
	 */
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int requested_fc = 0, error = 0;
	enum i40e_status_code aq_error = 0;
	u8 fc_aq_err = 0;

	aq_error = i40e_aq_get_link_info(hw, TRUE, NULL, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error retrieving link info from aq, %d\n",
		    __func__, aq_error);
		return (EAGAIN);
	}

	/* Read in new mode */
	requested_fc = hw->fc.current_mode;
	error = sysctl_handle_int(oidp, &requested_fc, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (requested_fc < 0 || requested_fc > 3) {
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
	hw->fc.requested_mode = requested_fc;
	aq_error = i40e_set_fc(hw, &fc_aq_err, TRUE);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new fc mode %d; fc_err %#x\n",
		    __func__, aq_error, fc_aq_err);
		return (EAGAIN);
	}

	if (hw->fc.current_mode != hw->fc.requested_mode) {
		device_printf(dev, "%s: FC set failure:\n", __func__);
		device_printf(dev, "%s: Current: %s / Requested: %s\n",
		    __func__,
		    i40e_fc_string[hw->fc.current_mode],
		    i40e_fc_string[hw->fc.requested_mode]);
	}

	return (0);
}

/*
** Control link advertise speed:
**	1 - advertise 1G only
**	2 - advertise 10G only
**	3 - advertise 1 and 10G
**
** Does not work on 40G devices.
*/
static int
i40e_set_advertise(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct i40e_aq_get_phy_abilities_resp abilities;
	struct i40e_aq_set_phy_config config;
	int current_ls = 0, requested_ls = 0;
	enum i40e_status_code aq_error = 0;
	int error = 0;

	/*
	** FW doesn't support changing advertised speed
	** for 40G devices; speed is always 40G.
	*/
	if (i40e_is_40G_device(hw->device_id))
		return (ENODEV);

	/* Get current capability information */
	aq_error = i40e_aq_get_phy_capabilities(hw, FALSE, FALSE, &abilities, NULL);
	if (aq_error) {
		device_printf(dev, "%s: Error getting phy capabilities %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EAGAIN);
	}

	/* Figure out current mode */
	else if (abilities.link_speed & I40E_LINK_SPEED_10GB
	    && abilities.link_speed & I40E_LINK_SPEED_1GB)
		current_ls = 3;
	else if (abilities.link_speed & I40E_LINK_SPEED_10GB)
		current_ls = 2;
	else if (abilities.link_speed & I40E_LINK_SPEED_1GB)
		current_ls = 1;
	else
		current_ls = 0;

	/* Read in new mode */
	requested_ls = current_ls;
	error = sysctl_handle_int(oidp, &requested_ls, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (requested_ls < 1 || requested_ls > 3) {
		device_printf(dev,
		    "Invalid advertised speed; valid modes are 1 through 3\n");
		return (EINVAL);
	}

	/* Exit if no change */
	if (current_ls == requested_ls)
		return (0);

	/* Prepare new config */
	bzero(&config, sizeof(config));
	config.phy_type = abilities.phy_type;
	config.abilities = abilities.abilities
	    | I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	config.eee_capability = abilities.eee_capability;
	config.eeer = abilities.eeer_val;
	config.low_power_ctrl = abilities.d3_lpan;
	/* Translate into aq cmd link_speed */
	switch (requested_ls) {
	case 3:
		config.link_speed = I40E_LINK_SPEED_10GB
		    | I40E_LINK_SPEED_1GB;
	case 2:
		config.link_speed = I40E_LINK_SPEED_10GB;
	case 1:
		config.link_speed = I40E_LINK_SPEED_1GB;
	default:
		// nothing should get here
		break;
	}

	/* Do aq command & restart link */
	aq_error = i40e_aq_set_phy_config(hw, &config, NULL);
	if (aq_error) {
		device_printf(dev, "%s: Error setting new phy config %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EAGAIN);
	}

	i40e_update_link_status(pf);
	return (0);
}

/*
** Get the width and transaction speed of
** the bus this adapter is plugged into.
*/
static u16
i40e_get_bus_info(struct i40e_hw *hw, device_t dev)
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
                    " for this device\n     is not sufficient for"
                    " normal operation.\n");
                device_printf(dev, "For expected performance a x8 "
                    "PCIE Gen3 slot is required.\n");
        }

        return (link);
}

#ifdef I40E_DEBUG
static int
i40e_sysctl_link_status(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
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
i40e_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_aq_get_phy_abilities_resp abilities_resp;
	char buf[512];

	enum i40e_status_code aq_error = 0;

	// TODO: Print out list of qualified modules as well?
	aq_error = i40e_aq_get_phy_capabilities(hw, TRUE, FALSE, &abilities_resp, NULL);
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
	    abilities_resp.phy_type, abilities_resp.link_speed, 
	    abilities_resp.abilities, abilities_resp.eee_capability,
	    abilities_resp.eeer_val, abilities_resp.d3_lpan);

	return (sysctl_handle_string(oidp, buf, strlen(buf), req));
}

static int
i40e_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	struct i40e_vsi *vsi = &pf->vsi;
	struct i40e_mac_filter *f;
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

#define I40E_SW_RES_SIZE 0x14
static int
i40e_sysctl_hw_res_info(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;

	u8 num_entries;
	struct i40e_aqc_switch_resource_alloc_element_resp resp[I40E_SW_RES_SIZE];

	buf = sbuf_new_for_sysctl(NULL, NULL, 0, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	error = i40e_aq_get_switch_resource_alloc(hw, &num_entries,
				resp,
				I40E_SW_RES_SIZE,
				NULL);
	if (error) {
		device_printf(dev, "%s: get_switch_resource_alloc() error %d, aq error %d\n",
		    __func__, error, hw->aq.asq_last_status);
		sbuf_delete(buf);
		return error;
	}
	device_printf(dev, "Num_entries: %d\n", num_entries);

	sbuf_cat(buf, "\n");
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
** Dump TX desc given index.
** Doesn't work; don't use.
** TODO: Also needs a queue index input!
**/
static int
i40e_sysctl_dump_txd(SYSCTL_HANDLER_ARGS)
{
	struct i40e_pf *pf = (struct i40e_pf *)arg1;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;

	u16 desc_idx = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 0, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	/* Read in index */
	error = sysctl_handle_int(oidp, &desc_idx, 0, req);
	if (error)
		return (error);
	if (req->newptr == NULL)
		return (EIO); // fix
	if (desc_idx > 1024) { // fix
		device_printf(dev,
		    "Invalid descriptor index, needs to be < 1024\n"); // fix
		return (EINVAL);
	}

	// Don't use this sysctl yet
	if (TRUE)
		return (ENODEV);

	sbuf_cat(buf, "\n");

	// set to queue 1?
	struct i40e_queue *que = pf->vsi.queues;
	struct tx_ring *txr = &(que[1].txr);
	struct i40e_tx_desc *txd = &txr->base[desc_idx];

	sbuf_printf(buf, "Que: %d, Desc: %d\n", que->me, desc_idx);
	sbuf_printf(buf, "Addr: %#18lx\n", txd->buffer_addr);
	sbuf_printf(buf, "Opts: %#18lx\n", txd->cmd_type_offset_bsz);

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
#endif

