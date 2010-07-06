/******************************************************************************

  Copyright (c) 2001-2010, Intel Corporation 
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
#include "opt_device_polling.h"
#endif

#include "ixgbe.h"

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int             ixgbe_display_debug_stats = 0;

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixgbe_driver_version[] = "2.2.3";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixgbe_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixgbe_vendor_info_t ixgbe_vendor_info_array[] =
{
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AF_SINGLE_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT2, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_DA_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_CX4_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_XF_LR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_SFP_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_KX4_MEZZ, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_SFP, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_XAUI_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_CX4, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_T3_LOM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_COMBO_BACKPLANE, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *ixgbe_strings[] = {
	"Intel(R) PRO/10GbE PCI-Express Network Driver"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixgbe_probe(device_t);
static int      ixgbe_attach(device_t);
static int      ixgbe_detach(device_t);
static int      ixgbe_shutdown(device_t);
static void     ixgbe_start(struct ifnet *);
static void     ixgbe_start_locked(struct tx_ring *, struct ifnet *);
#if __FreeBSD_version >= 800000
static int	ixgbe_mq_start(struct ifnet *, struct mbuf *);
static int	ixgbe_mq_start_locked(struct ifnet *,
                    struct tx_ring *, struct mbuf *);
static void	ixgbe_qflush(struct ifnet *);
#endif
static int      ixgbe_ioctl(struct ifnet *, u_long, caddr_t);
static void	ixgbe_init(void *);
static void	ixgbe_init_locked(struct adapter *);
static void     ixgbe_stop(void *);
static void     ixgbe_media_status(struct ifnet *, struct ifmediareq *);
static int      ixgbe_media_change(struct ifnet *);
static void     ixgbe_identify_hardware(struct adapter *);
static int      ixgbe_allocate_pci_resources(struct adapter *);
static int      ixgbe_allocate_msix(struct adapter *);
static int      ixgbe_allocate_legacy(struct adapter *);
static int	ixgbe_allocate_queues(struct adapter *);
static int	ixgbe_setup_msix(struct adapter *);
static void	ixgbe_free_pci_resources(struct adapter *);
static void     ixgbe_local_timer(void *);
static void     ixgbe_setup_interface(device_t, struct adapter *);
static void     ixgbe_config_link(struct adapter *);

static int      ixgbe_allocate_transmit_buffers(struct tx_ring *);
static int	ixgbe_setup_transmit_structures(struct adapter *);
static void	ixgbe_setup_transmit_ring(struct tx_ring *);
static void     ixgbe_initialize_transmit_units(struct adapter *);
static void     ixgbe_free_transmit_structures(struct adapter *);
static void     ixgbe_free_transmit_buffers(struct tx_ring *);

static int      ixgbe_allocate_receive_buffers(struct rx_ring *);
static int      ixgbe_setup_receive_structures(struct adapter *);
static int	ixgbe_setup_receive_ring(struct rx_ring *);
static void     ixgbe_initialize_receive_units(struct adapter *);
static void     ixgbe_free_receive_structures(struct adapter *);
static void     ixgbe_free_receive_buffers(struct rx_ring *);
static void	ixgbe_setup_hw_rsc(struct rx_ring *);

static void     ixgbe_enable_intr(struct adapter *);
static void     ixgbe_disable_intr(struct adapter *);
static void     ixgbe_update_stats_counters(struct adapter *);
static bool	ixgbe_txeof(struct tx_ring *);
static bool	ixgbe_rxeof(struct ix_queue *, int);
static void	ixgbe_rx_checksum(u32, struct mbuf *, u32);
static void     ixgbe_set_promisc(struct adapter *);
static void     ixgbe_disable_promisc(struct adapter *);
static void     ixgbe_set_multi(struct adapter *);
static void     ixgbe_print_hw_stats(struct adapter *);
static void	ixgbe_print_debug_info(struct adapter *);
static void     ixgbe_update_link_status(struct adapter *);
static void	ixgbe_refresh_mbufs(struct rx_ring *, int);
static int      ixgbe_xmit(struct tx_ring *, struct mbuf **);
static int      ixgbe_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	ixgbe_sysctl_debug(SYSCTL_HANDLER_ARGS);
static int	ixgbe_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	ixgbe_set_advertise(SYSCTL_HANDLER_ARGS);
static int	ixgbe_dma_malloc(struct adapter *, bus_size_t,
		    struct ixgbe_dma_alloc *, int);
static void     ixgbe_dma_free(struct adapter *, struct ixgbe_dma_alloc *);
static void	ixgbe_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static bool	ixgbe_tx_ctx_setup(struct tx_ring *, struct mbuf *);
static bool	ixgbe_tso_setup(struct tx_ring *, struct mbuf *, u32 *);
static void	ixgbe_set_ivar(struct adapter *, u8, u8, s8);
static void	ixgbe_configure_ivars(struct adapter *);
static u8 *	ixgbe_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

static void	ixgbe_setup_vlan_hw_support(struct adapter *);
static void	ixgbe_register_vlan(void *, struct ifnet *, u16);
static void	ixgbe_unregister_vlan(void *, struct ifnet *, u16);

static __inline void ixgbe_rx_discard(struct rx_ring *, int);
static __inline void ixgbe_rx_input(struct rx_ring *, struct ifnet *,
		    struct mbuf *, u32);

/* Support for pluggable optic modules */
static bool	ixgbe_sfp_probe(struct adapter *);

/* Legacy (single vector interrupt handler */
static void	ixgbe_legacy_irq(void *);

/* The MSI/X Interrupt handlers */
static void	ixgbe_msix_que(void *);
static void	ixgbe_msix_link(void *);

/* Deferred interrupt tasklets */
static void	ixgbe_handle_que(void *, int);
static void	ixgbe_handle_link(void *, int);
static void	ixgbe_handle_msf(void *, int);
static void	ixgbe_handle_mod(void *, int);

#ifdef IXGBE_FDIR
static void	ixgbe_atr(struct tx_ring *, struct mbuf *);
static void	ixgbe_reinit_fdir(void *, int);
#endif

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ixgbe_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixgbe_probe),
	DEVMETHOD(device_attach, ixgbe_attach),
	DEVMETHOD(device_detach, ixgbe_detach),
	DEVMETHOD(device_shutdown, ixgbe_shutdown),
	{0, 0}
};

static driver_t ixgbe_driver = {
	"ix", ixgbe_methods, sizeof(struct adapter),
};

static devclass_t ixgbe_devclass;
DRIVER_MODULE(ixgbe, pci, ixgbe_driver, ixgbe_devclass, 0, 0);

MODULE_DEPEND(ixgbe, pci, 1, 1, 1);
MODULE_DEPEND(ixgbe, ether, 1, 1, 1);

/*
** TUNEABLE PARAMETERS:
*/

/*
** AIM: Adaptive Interrupt Moderation
** which means that the interrupt rate
** is varied over time based on the
** traffic for that interrupt vector
*/
static int ixgbe_enable_aim = TRUE;
TUNABLE_INT("hw.ixgbe.enable_aim", &ixgbe_enable_aim);

/* How many packets rxeof tries to clean at a time */
static int ixgbe_rx_process_limit = 128;
TUNABLE_INT("hw.ixgbe.rx_process_limit", &ixgbe_rx_process_limit);

/* Flow control setting, default to full */
static int ixgbe_flow_control = ixgbe_fc_full;
TUNABLE_INT("hw.ixgbe.flow_control", &ixgbe_flow_control);

/*
** Smart speed setting, default to on
** this only works as a compile option
** right now as its during attach, set
** this to 'ixgbe_smart_speed_off' to
** disable.
*/
static int ixgbe_smart_speed = ixgbe_smart_speed_on;

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int ixgbe_enable_msix = 1;
TUNABLE_INT("hw.ixgbe.enable_msix", &ixgbe_enable_msix);

/*
 * Header split: this causes the hardware to DMA
 * the header into a seperate mbuf from the payload,
 * it can be a performance win in some workloads, but
 * in others it actually hurts, its off by default. 
 */
static bool ixgbe_header_split = FALSE;
TUNABLE_INT("hw.ixgbe.hdr_split", &ixgbe_header_split);

/*
 * Number of Queues, can be set to 0,
 * it then autoconfigures based on the
 * number of cpus. Each queue is a pair
 * of RX and TX rings with a msix vector
 */
static int ixgbe_num_queues = 0;
TUNABLE_INT("hw.ixgbe.num_queues", &ixgbe_num_queues);

/*
** Number of TX descriptors per ring,
** setting higher than RX as this seems
** the better performing choice.
*/
static int ixgbe_txd = PERFORM_TXD;
TUNABLE_INT("hw.ixgbe.txd", &ixgbe_txd);

/* Number of RX descriptors per ring */
static int ixgbe_rxd = PERFORM_RXD;
TUNABLE_INT("hw.ixgbe.rxd", &ixgbe_rxd);

/* Keep running tab on them for sanity check */
static int ixgbe_total_ports;

/*
** Shadow VFTA table, this is needed because
** the real filter table gets cleared during
** a soft reset and we need to repopulate it.
*/
static u32 ixgbe_shadow_vfta[IXGBE_VFTA_SIZE];

/*
** The number of scatter-gather segments
** differs for 82598 and 82599, default to
** the former.
*/
static int ixgbe_num_segs = IXGBE_82598_SCATTER;

#ifdef IXGBE_FDIR
/*
** For Flow Director: this is the
** number of TX packets we sample
** for the filter pool, this means
** every 20th packet will be probed.
**
** This feature can be disabled by 
** setting this to 0.
*/
static int atr_sample_rate = 20;
/* 
** Flow Director actually 'steals'
** part of the packet buffer as its
** filter pool, this variable controls
** how much it uses:
**  0 = 64K, 1 = 128K, 2 = 256K
*/
static int fdir_pballoc = 1;
#endif

/*********************************************************************
 *  Device identification routine
 *
 *  ixgbe_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
ixgbe_probe(device_t dev)
{
	ixgbe_vendor_info_t *ent;

	u16	pci_vendor_id = 0;
	u16	pci_device_id = 0;
	u16	pci_subvendor_id = 0;
	u16	pci_subdevice_id = 0;
	char	adapter_name[256];

	INIT_DEBUGOUT("ixgbe_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != IXGBE_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixgbe_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(adapter_name, "%s, Version - %s",
				ixgbe_strings[ent->index],
				ixgbe_driver_version);
			device_set_desc_copy(dev, adapter_name);
			++ixgbe_total_ports;
			return (0);
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
ixgbe_attach(device_t dev)
{
	struct adapter *adapter;
	struct ixgbe_hw *hw;
	int             error = 0;
	u16		pci_device_id, csum;
	u32		ctrl_ext;

	INIT_DEBUGOUT("ixgbe_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	hw = &adapter->hw;

	/* Core Lock Init*/
	IXGBE_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* Keep track of optics */
	pci_device_id = pci_get_device(dev);
	switch (pci_device_id) {
		case IXGBE_DEV_ID_82598_CX4_DUAL_PORT:
		case IXGBE_DEV_ID_82598EB_CX4:
			adapter->optics = IFM_10G_CX4;
			break;
		case IXGBE_DEV_ID_82598:
		case IXGBE_DEV_ID_82598AF_DUAL_PORT:
		case IXGBE_DEV_ID_82598AF_SINGLE_PORT:
		case IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM:
		case IXGBE_DEV_ID_82598EB_SFP_LOM:
		case IXGBE_DEV_ID_82598AT:
			adapter->optics = IFM_10G_SR;
			break;
		case IXGBE_DEV_ID_82598AT2:
			adapter->optics = IFM_10G_T;
			break;
		case IXGBE_DEV_ID_82598EB_XF_LR:
			adapter->optics = IFM_10G_LR;
			break;
		case IXGBE_DEV_ID_82599_SFP:
			adapter->optics = IFM_10G_SR;
			ixgbe_num_segs = IXGBE_82599_SCATTER;
			break;
		case IXGBE_DEV_ID_82598_DA_DUAL_PORT :
			adapter->optics = IFM_10G_TWINAX;
			break;
		case IXGBE_DEV_ID_82599_KX4:
		case IXGBE_DEV_ID_82599_KX4_MEZZ:
		case IXGBE_DEV_ID_82599_CX4:
			adapter->optics = IFM_10G_CX4;
			ixgbe_num_segs = IXGBE_82599_SCATTER;
			break;
		case IXGBE_DEV_ID_82599_XAUI_LOM:
		case IXGBE_DEV_ID_82599_COMBO_BACKPLANE:
			ixgbe_num_segs = IXGBE_82599_SCATTER;
			break;
		case IXGBE_DEV_ID_82599_T3_LOM:
			ixgbe_num_segs = IXGBE_82599_SCATTER;
			adapter->optics = IFM_10G_T;
		default:
			break;
	}

	/* SYSCTL APIs */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "stats", CTLTYPE_INT | CTLFLAG_RW,
			adapter, 0, ixgbe_sysctl_stats, "I", "Statistics");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "debug", CTLTYPE_INT | CTLFLAG_RW,
			adapter, 0, ixgbe_sysctl_debug, "I", "Debug Info");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "flow_control", CTLTYPE_INT | CTLFLAG_RW,
			adapter, 0, ixgbe_set_flowcntl, "I", "Flow Control");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "advertise_gig", CTLTYPE_INT | CTLFLAG_RW,
			adapter, 0, ixgbe_set_advertise, "I", "1G Link");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "enable_aim", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_enable_aim, 1, "Interrupt Moderation");

	/* Set up the timer callout */
	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware revision */
	ixgbe_identify_hardware(adapter);

	/* Do base PCI setup - map BAR0 */
	if (ixgbe_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_out;
	}

	/* Do descriptor calc and sanity checks */
	if (((ixgbe_txd * sizeof(union ixgbe_adv_tx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_txd < MIN_TXD || ixgbe_txd > MAX_TXD) {
		device_printf(dev, "TXD config issue, using default!\n");
		adapter->num_tx_desc = DEFAULT_TXD;
	} else
		adapter->num_tx_desc = ixgbe_txd;

	/*
	** With many RX rings it is easy to exceed the
	** system mbuf allocation. Tuning nmbclusters
	** can alleviate this.
	*/
	if (nmbclusters > 0 ) {
		int s;
		s = (ixgbe_rxd * adapter->num_queues) * ixgbe_total_ports;
		if (s > nmbclusters) {
			device_printf(dev, "RX Descriptors exceed "
			    "system mbuf max, using default instead!\n");
			ixgbe_rxd = DEFAULT_RXD;
		}
	}

	if (((ixgbe_rxd * sizeof(union ixgbe_adv_rx_desc)) % DBA_ALIGN) != 0 ||
	    ixgbe_rxd < MIN_TXD || ixgbe_rxd > MAX_TXD) {
		device_printf(dev, "RXD config issue, using default!\n");
		adapter->num_rx_desc = DEFAULT_RXD;
	} else
		adapter->num_rx_desc = ixgbe_rxd;

	/* Allocate our TX/RX Queues */
	if (ixgbe_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_out;
	}

	/* Initialize the shared code */
	error = ixgbe_init_shared_code(hw);
	if (error == IXGBE_ERR_SFP_NOT_PRESENT) {
		/*
		** No optics in this port, set up
		** so the timer routine will probe 
		** for later insertion.
		*/
		adapter->sfp_probe = TRUE;
		error = 0;
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,"Unsupported SFP+ module detected!\n");
		error = EIO;
		goto err_late;
	} else if (error) {
		device_printf(dev,"Unable to initialize the shared code\n");
		error = EIO;
		goto err_late;
	}

	/* Make sure we have a good EEPROM before we read from it */
	if (ixgbe_validate_eeprom_checksum(&adapter->hw, &csum) < 0) {
		device_printf(dev,"The EEPROM Checksum Is Not Valid\n");
		error = EIO;
		goto err_late;
	}

	/* Pick up the smart speed setting */
	if (hw->mac.type == ixgbe_mac_82599EB)
		hw->phy.smart_speed = ixgbe_smart_speed;

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.pause_time = IXGBE_FC_PAUSE;
	hw->fc.low_water = IXGBE_FC_LO;
	hw->fc.high_water = IXGBE_FC_HI;
	hw->fc.send_xon = TRUE;

	error = ixgbe_init_hw(hw);
	if (error == IXGBE_ERR_EEPROM_VERSION) {
		device_printf(dev, "This device is a pre-production adapter/"
		    "LOM.  Please be aware there may be issues associated "
		    "with your hardware.\n If you are experiencing problems "
		    "please contact your Intel or hardware representative "
		    "who provided you with this hardware.\n");
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED)
		device_printf(dev,"Unsupported SFP+ Module\n");

	if (error) {
		error = EIO;
		device_printf(dev,"Hardware Initialization Failure\n");
		goto err_late;
	}

	if ((adapter->msix > 1) && (ixgbe_enable_msix))
		error = ixgbe_allocate_msix(adapter); 
	else
		error = ixgbe_allocate_legacy(adapter); 
	if (error) 
		goto err_late;

	/* Setup OS specific network interface */
	ixgbe_setup_interface(dev, adapter);

	/* Sysctl for limiting the amount of work done in the taskqueue */
	ixgbe_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    ixgbe_rx_process_limit);

	/* Initialize statistics */
	ixgbe_update_stats_counters(adapter);

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixgbe_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixgbe_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST);

        /* Print PCIE bus type/speed/width info */
	ixgbe_get_bus_info(hw);
	device_printf(dev,"PCI Express Bus: Speed %s %s\n",
	    ((hw->bus.speed == ixgbe_bus_speed_5000) ? "5.0Gb/s":
	    (hw->bus.speed == ixgbe_bus_speed_2500) ? "2.5Gb/s":"Unknown"),
	    (hw->bus.width == ixgbe_bus_width_pcie_x8) ? "Width x8" :
	    (hw->bus.width == ixgbe_bus_width_pcie_x4) ? "Width x4" :
	    (hw->bus.width == ixgbe_bus_width_pcie_x1) ? "Width x1" :
	    ("Unknown"));

	if ((hw->bus.width <= ixgbe_bus_width_pcie_x4) &&
	    (hw->bus.speed == ixgbe_bus_speed_2500)) {
		device_printf(dev, "PCI-Express bandwidth available"
		    " for this card\n     is not sufficient for"
		    " optimal performance.\n");
		device_printf(dev, "For optimal performance a x8 "
		    "PCIE, or x4 PCIE 2 slot is required.\n");
        }

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(hw, IXGBE_CTRL_EXT, ctrl_ext);

	INIT_DEBUGOUT("ixgbe_attach: end");
	return (0);
err_late:
	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
err_out:
	ixgbe_free_pci_resources(adapter);
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
ixgbe_detach(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ix_queue *que = adapter->queues;
	u32	ctrl_ext;

	INIT_DEBUGOUT("ixgbe_detach: begin");

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_queues; i++, que++) {
		if (que->tq) {
			taskqueue_drain(que->tq, &que->que_task);
			taskqueue_free(que->tq);
		}
	}

	/* Drain the Link queue */
	if (adapter->tq) {
		taskqueue_drain(adapter->tq, &adapter->link_task);
		taskqueue_drain(adapter->tq, &adapter->mod_task);
		taskqueue_drain(adapter->tq, &adapter->msf_task);
#ifdef IXGBE_FDIR
		taskqueue_drain(adapter->tq, &adapter->fdir_task);
#endif
		taskqueue_free(adapter->tq);
	}

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);
	ixgbe_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(adapter->ifp);

	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);

	IXGBE_CORE_LOCK_DESTROY(adapter);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
ixgbe_shutdown(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return (0);
}


/*********************************************************************
 *  Transmit entry point
 *
 *  ixgbe_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

static void
ixgbe_start_locked(struct tx_ring *txr, struct ifnet * ifp)
{
	struct mbuf    *m_head;
	struct adapter *adapter = txr->adapter;

	IXGBE_TX_LOCK_ASSERT(txr);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (ixgbe_xmit(txr, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set watchdog on */
		txr->watchdog_check = TRUE;
		txr->watchdog_time = ticks;

	}
	return;
}

/*
 * Legacy TX start - called by the stack, this
 * always uses the first tx ring, and should
 * not be used with multiqueue tx enabled.
 */
static void
ixgbe_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		IXGBE_TX_LOCK(txr);
		ixgbe_start_locked(txr, ifp);
		IXGBE_TX_UNLOCK(txr);
	}
	return;
}

#if __FreeBSD_version >= 800000
/*
** Multiqueue Transmit driver
**
*/
static int
ixgbe_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	int 		i = 0, err = 0;

	/* Which queue to use */
	if ((m->m_flags & M_FLOWID) != 0)
		i = m->m_pkthdr.flowid % adapter->num_queues;

	txr = &adapter->tx_rings[i];
	que = &adapter->queues[i];

	if (IXGBE_TX_TRYLOCK(txr)) {
		err = ixgbe_mq_start_locked(ifp, txr, m);
		IXGBE_TX_UNLOCK(txr);
	} else {
		err = drbr_enqueue(ifp, txr->br, m);
		taskqueue_enqueue(que->tq, &que->que_task);
	}

	return (err);
}

static int
ixgbe_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr, struct mbuf *m)
{
	struct adapter  *adapter = txr->adapter;
        struct mbuf     *next;
        int             enqueued, err = 0;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || adapter->link_active == 0) {
		if (m != NULL)
			err = drbr_enqueue(ifp, txr->br, m);
		return (err);
	}

	enqueued = 0;
	if (m == NULL) {
		next = drbr_dequeue(ifp, txr->br);
	} else if (drbr_needs_enqueue(ifp, txr->br)) {
		if ((err = drbr_enqueue(ifp, txr->br, m)) != 0)
			return (err);
		next = drbr_dequeue(ifp, txr->br);
	} else
		next = m;

	/* Process the queue */
	while (next != NULL) {
		if ((err = ixgbe_xmit(txr, &next)) != 0) {
			if (next != NULL)
				err = drbr_enqueue(ifp, txr->br, next);
			break;
		}
		enqueued++;
		drbr_stats_update(ifp, next->m_pkthdr.len, next->m_flags);
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
		if (txr->tx_avail <= IXGBE_TX_OP_THRESHOLD) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		next = drbr_dequeue(ifp, txr->br);
	}

	if (enqueued > 0) {
		/* Set watchdog on */
		txr->watchdog_check = TRUE;
		txr->watchdog_time = ticks;
	}

	return (err);
}

/*
** Flush all ring buffers
*/
static void
ixgbe_qflush(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	struct mbuf	*m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IXGBE_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}
#endif /* __FreeBSD_version >= 800000 */

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixgbe_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixgbe_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifreq	*ifr = (struct ifreq *) data;
	int             error = 0;

	switch (command) {

	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXGBE_MAX_FRAME_SIZE - ETHER_HDR_LEN) {
			error = EINVAL;
		} else {
			IXGBE_CORE_LOCK(adapter);
			ifp->if_mtu = ifr->ifr_mtu;
			adapter->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
			ixgbe_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		IXGBE_CORE_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ adapter->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					ixgbe_disable_promisc(adapter);
					ixgbe_set_promisc(adapter);
                                }
			} else
				ixgbe_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixgbe_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		IXGBE_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixgbe_disable_intr(adapter);
			ixgbe_set_multi(adapter);
			ixgbe_enable_intr(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");
		if (mask & IFCAP_HWCSUM)
			ifp->if_capenable ^= IFCAP_HWCSUM;
		if (mask & IFCAP_TSO4)
			ifp->if_capenable ^= IFCAP_TSO4;
		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixgbe_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
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
#define IXGBE_MHADD_MFS_SHIFT 16

static void
ixgbe_init_locked(struct adapter *adapter)
{
	struct ifnet   *ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		k, txdctl, mhadd, gpie;
	u32		rxdctl, rxctrl;

	mtx_assert(&adapter->core_mtx, MA_OWNED);
	INIT_DEBUGOUT("ixgbe_init: begin");
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
        callout_stop(&adapter->timer);

        /* reprogram the RAR[0] in case user changed it. */
        ixgbe_set_rar(hw, 0, adapter->hw.mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(adapter->ifp), hw->mac.addr,
	      IXGBE_ETH_LENGTH_OF_ADDRESS);
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, 1);
	hw->addr_ctrl.rar_used_count = 1;

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev,"Could not setup transmit structures\n");
		ixgbe_stop(adapter);
		return;
	}

	ixgbe_reset_hw(hw);
	ixgbe_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixgbe_set_multi(adapter);

	/*
	** Determine the correct mbuf pool
	** for doing jumbo/headersplit
	*/
	if (ifp->if_mtu > ETHERMTU)
		adapter->rx_mbuf_sz = MJUMPAGESIZE;
	else
		adapter->rx_mbuf_sz = MCLBYTES;

	/* Prepare receive descriptors and buffers */
	if (ixgbe_setup_receive_structures(adapter)) {
		device_printf(dev,"Could not setup receive structures\n");
		ixgbe_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixgbe_initialize_receive_units(adapter);

	gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);

	if (hw->mac.type == ixgbe_mac_82599EB) {
		gpie |= IXGBE_SDP1_GPIEN;
		gpie |= IXGBE_SDP2_GPIEN;
	}

	/* Enable Fan Failure Interrupt */
	if (hw->device_id == IXGBE_DEV_ID_82598AT)
		gpie |= IXGBE_SDP1_GPIEN;

	if (adapter->msix > 1) {
		/* Enable Enhanced MSIX mode */
		gpie |= IXGBE_GPIE_MSIX_MODE;
		gpie |= IXGBE_GPIE_EIAME | IXGBE_GPIE_PBA_SUPPORT |
		    IXGBE_GPIE_OCD;
	}
	IXGBE_WRITE_REG(hw, IXGBE_GPIE, gpie);

	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
#if __FreeBSD_version >= 800000
		if (hw->mac.type == ixgbe_mac_82599EB)
			ifp->if_hwassist |= CSUM_SCTP;
#endif
	}

	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		mhadd = IXGBE_READ_REG(hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= adapter->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(hw, IXGBE_MHADD, mhadd);
	}
	
	/* Now enable all the queues */

	for (int i = 0; i < adapter->num_queues; i++) {
		txdctl = IXGBE_READ_REG(hw, IXGBE_TXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		/* Set WTHRESH to 8, burst writeback */
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(hw, IXGBE_TXDCTL(i), txdctl);
	}

	for (int i = 0; i < adapter->num_queues; i++) {
		rxdctl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
		if (hw->mac.type == ixgbe_mac_82598EB) {
			/*
			** PTHRESH = 21
			** HTHRESH = 4
			** WTHRESH = 8
			*/
			rxdctl &= ~0x3FFFFF;
			rxdctl |= 0x080420;
		}
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), rxdctl);
		for (k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(hw, IXGBE_RXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		wmb();
		IXGBE_WRITE_REG(hw, IXGBE_RDT(i), adapter->num_rx_desc - 1);
	}

	/* Set up VLAN offloads and filter */
	ixgbe_setup_vlan_hw_support(adapter);

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (hw->mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	ixgbe_enable_rx_dma(hw, rxctrl);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);

	/* Set up MSI/X routing */
	if (ixgbe_enable_msix)  {
		ixgbe_configure_ivars(adapter);
		/* Set up auto-mask */
		if (hw->mac.type == ixgbe_mac_82598EB)
			IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
		else {
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(0), 0xFFFFFFFF);
			IXGBE_WRITE_REG(hw, IXGBE_EIAM_EX(1), 0xFFFFFFFF);
		}
	} else {  /* Simple settings for Legacy/MSI */
                ixgbe_set_ivar(adapter, 0, 0, 0);
                ixgbe_set_ivar(adapter, 0, 0, 1);
		IXGBE_WRITE_REG(hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}

#ifdef IXGBE_FDIR
	/* Init Flow director */
	if (hw->mac.type == ixgbe_mac_82599EB)
		ixgbe_init_fdir_signature_82599(&adapter->hw, fdir_pballoc);
#endif

	/*
	** Check on any SFP devices that
	** need to be kick-started
	*/
	if (hw->phy.type == ixgbe_phy_none) {
		int err = hw->phy.ops.identify(hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
                	device_printf(dev,
			    "Unsupported SFP+ module type was detected.\n");
			return;
        	}
	}

	/* Set moderation on the Link interrupt */
	IXGBE_WRITE_REG(hw, IXGBE_EITR(adapter->linkvec), IXGBE_LINK_ITR);

	/* Config/Enable Link */
	ixgbe_config_link(adapter);

	/* And now turn on interrupts */
	ixgbe_enable_intr(adapter);

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

static void
ixgbe_init(void *arg)
{
	struct adapter *adapter = arg;

	IXGBE_CORE_LOCK(adapter);
	ixgbe_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return;
}


/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/

static inline void
ixgbe_enable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1 << vector);
	u32	mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
                mask = (IXGBE_EIMS_RTX_QUEUE & queue);
                IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);
	} else {
                mask = (queue & 0xFFFFFFFF);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(0), mask);
                mask = (queue >> 32);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMS_EX(1), mask);
	}
}

static inline void
ixgbe_disable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1 << vector);
	u32	mask;

	if (hw->mac.type == ixgbe_mac_82598EB) {
                mask = (IXGBE_EIMS_RTX_QUEUE & queue);
                IXGBE_WRITE_REG(hw, IXGBE_EIMC, mask);
	} else {
                mask = (queue & 0xFFFFFFFF);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(0), mask);
                mask = (queue >> 32);
                if (mask)
                        IXGBE_WRITE_REG(hw, IXGBE_EIMC_EX(1), mask);
	}
}

static inline void
ixgbe_rearm_queues(struct adapter *adapter, u64 queues)
{
	u32 mask;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queues);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, mask);
	} else {
		mask = (queues & 0xFFFFFFFF);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(0), mask);
		mask = (queues >> 32);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS_EX(1), mask);
	}
}


static void
ixgbe_handle_que(void *context, int pending)
{
	struct ix_queue *que = context;
	struct adapter  *adapter = que->adapter;
	struct tx_ring  *txr = que->txr;
	struct ifnet    *ifp = adapter->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixgbe_rxeof(que, adapter->rx_process_limit);
		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
#if __FreeBSD_version >= 800000
		if (!drbr_empty(ifp, txr->br))
			ixgbe_mq_start_locked(ifp, txr, NULL);
#else
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			ixgbe_start_locked(txr, ifp);
#endif
		IXGBE_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->que_task);
			return;
		}
	}

	/* Reenable this interrupt */
	ixgbe_enable_queue(adapter, que->msix);
	return;
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/

static void
ixgbe_legacy_irq(void *arg)
{
	struct ix_queue *que = arg;
	struct adapter	*adapter = que->adapter;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct 		tx_ring *txr = adapter->tx_rings;
	bool		more_tx, more_rx;
	u32       	reg_eicr, loop = MAX_LOOP;


	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICR);

	++que->irqs;
	if (reg_eicr == 0) {
		ixgbe_enable_intr(adapter);
		return;
	}

	more_rx = ixgbe_rxeof(que, adapter->rx_process_limit);

	IXGBE_TX_LOCK(txr);
	do {
		more_tx = ixgbe_txeof(txr);
	} while (loop-- && more_tx);
	IXGBE_TX_UNLOCK(txr);

	if (more_rx || more_tx)
		taskqueue_enqueue(que->tq, &que->que_task);

	/* Check for fan failure */
	if ((hw->phy.media_type == ixgbe_media_type_copper) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_GPI_SDP1);
	}

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
		taskqueue_enqueue(adapter->tq, &adapter->link_task);

	ixgbe_enable_intr(adapter);
	return;
}


/*********************************************************************
 *
 *  MSI Queue Interrupt Service routine
 *
 **********************************************************************/
void
ixgbe_msix_que(void *arg)
{
	struct ix_queue	*que = arg;
	struct adapter  *adapter = que->adapter;
	struct tx_ring	*txr = que->txr;
	struct rx_ring	*rxr = que->rxr;
	bool		more_tx, more_rx;
	u32		newitr = 0;

	++que->irqs;

	more_rx = ixgbe_rxeof(que, adapter->rx_process_limit);

	IXGBE_TX_LOCK(txr);
	more_tx = ixgbe_txeof(txr);
	IXGBE_TX_UNLOCK(txr);

	more_rx = ixgbe_rxeof(que, adapter->rx_process_limit);

	/* Do AIM now? */

	if (ixgbe_enable_aim == FALSE)
		goto no_calc;
	/*
	** Do Adaptive Interrupt Moderation:
        **  - Write out last calculated setting
	**  - Calculate based on average size over
	**    the last interval.
	*/
        if (que->eitr_setting)
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_EITR(que->msix), que->eitr_setting);
 
        que->eitr_setting = 0;

        /* Idle, do nothing */
        if ((txr->bytes == 0) && (rxr->bytes == 0))
                goto no_calc;
                                
	if ((txr->bytes) && (txr->packets))
               	newitr = txr->bytes/txr->packets;
	if ((rxr->bytes) && (rxr->packets))
		newitr = max(newitr,
		    (rxr->bytes / rxr->packets));
	newitr += 24; /* account for hardware frame, crc */

	/* set an upper boundary */
	newitr = min(newitr, 3000);

	/* Be nice to the mid range */
	if ((newitr > 300) && (newitr < 1200))
		newitr = (newitr / 3);
	else
		newitr = (newitr / 2);

        if (adapter->hw.mac.type == ixgbe_mac_82598EB)
                newitr |= newitr << 16;
        else
                newitr |= IXGBE_EITR_CNT_WDIS;
                 
        /* save for next interrupt */
        que->eitr_setting = newitr;

        /* Reset state */
        txr->bytes = 0;
        txr->packets = 0;
        rxr->bytes = 0;
        rxr->packets = 0;

no_calc:
	if (more_tx || more_rx)
		taskqueue_enqueue(que->tq, &que->que_task);
	else /* Reenable this interrupt */
		ixgbe_enable_queue(adapter, que->msix);
	return;
}


static void
ixgbe_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		reg_eicr;

	++adapter->link_irq;

	/* First get the cause */
	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICS);
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_EICR, reg_eicr);

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
		taskqueue_enqueue(adapter->tq, &adapter->link_task);

	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
#ifdef IXGBE_FDIR
		if (reg_eicr & IXGBE_EICR_FLOW_DIR) {
			/* This is probably overkill :) */
			if (!atomic_cmpset_int(&adapter->fdir_reinit, 0, 1))
				return;
                	/* Clear the interrupt */
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_FLOW_DIR);
			/* Turn off the interface */
			adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
			taskqueue_enqueue(adapter->tq, &adapter->fdir_task);
		} else
#endif
		if (reg_eicr & IXGBE_EICR_ECC) {
                	device_printf(adapter->dev, "\nCRITICAL: ECC ERROR!! "
			    "Please Reboot!!\n");
			IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_ECC);
		} else

		if (reg_eicr & IXGBE_EICR_GPI_SDP1) {
                	/* Clear the interrupt */
                	IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
			taskqueue_enqueue(adapter->tq, &adapter->msf_task);
        	} else if (reg_eicr & IXGBE_EICR_GPI_SDP2) {
                	/* Clear the interrupt */
                	IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP2);
			taskqueue_enqueue(adapter->tq, &adapter->mod_task);
		}
        } 

	/* Check for fan failure */
	if ((hw->device_id == IXGBE_DEV_ID_82598AT) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EICR, IXGBE_EICR_GPI_SDP1);
	}

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, IXGBE_EIMS_OTHER);
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
ixgbe_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct adapter *adapter = ifp->if_softc;

	INIT_DEBUGOUT("ixgbe_media_status: begin");
	IXGBE_CORE_LOCK(adapter);
	ixgbe_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		IXGBE_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	switch (adapter->link_speed) {
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= adapter->optics | IFM_FDX;
			break;
	}

	IXGBE_CORE_UNLOCK(adapter);

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
ixgbe_media_change(struct ifnet * ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia *ifm = &adapter->media;

	INIT_DEBUGOUT("ixgbe_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

        switch (IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_AUTO:
                adapter->hw.phy.autoneg_advertised =
		    IXGBE_LINK_SPEED_1GB_FULL | IXGBE_LINK_SPEED_10GB_FULL;
                break;
        default:
                device_printf(adapter->dev, "Only auto media type\n");
		return (EINVAL);
        }

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors, allowing the
 *  TX engine to transmit the packets. 
 *  	- return 0 on success, positive on failure
 *
 **********************************************************************/

static int
ixgbe_xmit(struct tx_ring *txr, struct mbuf **m_headp)
{
	struct adapter  *adapter = txr->adapter;
	u32		olinfo_status = 0, cmd_type_len;
	u32		paylen = 0;
	int             i, j, error, nsegs;
	int		first, last = 0;
	struct mbuf	*m_head;
	bus_dma_segment_t segs[ixgbe_num_segs];
	bus_dmamap_t	map;
	struct ixgbe_tx_buf *txbuf, *txbuf_mapped;
	union ixgbe_adv_tx_desc *txd = NULL;

	m_head = *m_headp;

	/* Basic descriptor defines */
        cmd_type_len = (IXGBE_ADVTXD_DTYP_DATA |
	    IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT);

	if (m_head->m_flags & M_VLANTAG)
        	cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail_desc;
	txbuf = &txr->tx_buffers[first];
	txbuf_mapped = txbuf;
	map = txbuf->map;

	/*
	 * Map the packet for DMA.
	 */
	error = bus_dmamap_load_mbuf_sg(txr->txtag, map,
	    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

	if (error == EFBIG) {
		struct mbuf *m;

		m = m_defrag(*m_headp, M_DONTWAIT);
		if (m == NULL) {
			adapter->mbuf_defrag_failed++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (ENOBUFS);
		}
		*m_headp = m;

		/* Try it again */
		error = bus_dmamap_load_mbuf_sg(txr->txtag, map,
		    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

		if (error == ENOMEM) {
			adapter->no_tx_dma_setup++;
			return (error);
		} else if (error != 0) {
			adapter->no_tx_dma_setup++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (error);
		}
	} else if (error == ENOMEM) {
		adapter->no_tx_dma_setup++;
		return (error);
	} else if (error != 0) {
		adapter->no_tx_dma_setup++;
		m_freem(*m_headp);
		*m_headp = NULL;
		return (error);
	}

	/* Make certain there are enough descriptors */
	if (nsegs > txr->tx_avail - 2) {
		txr->no_desc_avail++;
		error = ENOBUFS;
		goto xmit_fail;
	}
	m_head = *m_headp;

	/*
	** Set up the appropriate offload context
	** this becomes the first descriptor of 
	** a packet.
	*/
	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		if (ixgbe_tso_setup(txr, m_head, &paylen)) {
			cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
			olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
			olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
			olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
			++adapter->tso_tx;
		} else
			return (ENXIO);
	} else if (ixgbe_tx_ctx_setup(txr, m_head))
		olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;

#ifdef IXGBE_IEEE1588
        /* This is changing soon to an mtag detection */
        if (we detect this mbuf has a TSTAMP mtag)
                cmd_type_len |= IXGBE_ADVTXD_MAC_TSTAMP;
#endif

#ifdef IXGBE_FDIR
	/* Do the flow director magic */
	if ((txr->atr_sample) && (!adapter->fdir_reinit)) {
		++txr->atr_count;
		if (txr->atr_count >= atr_sample_rate) {
			ixgbe_atr(txr, m_head);
			txr->atr_count = 0;
		}
	}
#endif
        /* Record payload length */
	if (paylen == 0)
        	olinfo_status |= m_head->m_pkthdr.len <<
		    IXGBE_ADVTXD_PAYLEN_SHIFT;

	i = txr->next_avail_desc;
	for (j = 0; j < nsegs; j++) {
		bus_size_t seglen;
		bus_addr_t segaddr;

		txbuf = &txr->tx_buffers[i];
		txd = &txr->tx_base[i];
		seglen = segs[j].ds_len;
		segaddr = htole64(segs[j].ds_addr);

		txd->read.buffer_addr = segaddr;
		txd->read.cmd_type_len = htole32(txr->txd_cmd |
		    cmd_type_len |seglen);
		txd->read.olinfo_status = htole32(olinfo_status);
		last = i; /* descriptor that will get completion IRQ */

		if (++i == adapter->num_tx_desc)
			i = 0;

		txbuf->m_head = NULL;
		txbuf->eop_index = -1;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= nsegs;
	txr->next_avail_desc = i;

	txbuf->m_head = m_head;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
	txbuf->eop_index = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(txr->me), i);

	/* Do a clean if descriptors are low */
	if (txr->tx_avail <= IXGBE_TX_CLEANUP_THRESHOLD)
		ixgbe_txeof(txr);

	return (0);

xmit_fail:
	bus_dmamap_unload(txr->txtag, txbuf->map);
	return (error);

}

static void
ixgbe_set_promisc(struct adapter *adapter)
{

	u_int32_t       reg_rctl;
	struct ifnet   *ifp = adapter->ifp;

	reg_rctl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= IXGBE_FCTRL_MPE;
		reg_rctl &= ~IXGBE_FCTRL_UPE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);
	}
	return;
}

static void
ixgbe_disable_promisc(struct adapter * adapter)
{
	u_int32_t       reg_rctl;

	reg_rctl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);

	reg_rctl &= (~IXGBE_FCTRL_UPE);
	reg_rctl &= (~IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, reg_rctl);

	return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
#define IXGBE_RAR_ENTRIES 16

static void
ixgbe_set_multi(struct adapter *adapter)
{
	u32	fctrl;
	u8	mta[MAX_NUM_MULTICAST_ADDRESSES * IXGBE_ETH_LENGTH_OF_ADDRESS];
	u8	*update_ptr;
	struct	ifmultiaddr *ifma;
	int	mcnt = 0;
	struct ifnet   *ifp = adapter->ifp;

	IOCTL_DEBUGOUT("ixgbe_set_multi: begin");

	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	if (ifp->if_flags & IFF_PROMISC)
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	else if (ifp->if_flags & IFF_ALLMULTI) {
		fctrl |= IXGBE_FCTRL_MPE;
		fctrl &= ~IXGBE_FCTRL_UPE;
	} else
		fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

#if __FreeBSD_version < 800000
	IF_ADDR_LOCK(ifp);
#else
	if_maddr_rlock(ifp);
#endif
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		bcopy(LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
#if __FreeBSD_version < 800000
	IF_ADDR_UNLOCK(ifp);
#else
	if_maddr_runlock(ifp);
#endif

	update_ptr = mta;
	ixgbe_update_mc_addr_list(&adapter->hw,
	    update_ptr, mcnt, ixgbe_mc_array_itr);

	return;
}

/*
 * This is an iterator function now needed by the multicast
 * shared code. It simply feeds the shared code routine the
 * addresses in the array of ixgbe_set_multi() one by one.
 */
static u8 *
ixgbe_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
{
	u8 *addr = *update_ptr;
	u8 *newptr;
	*vmdq = 0;

	newptr = addr + IXGBE_ETH_LENGTH_OF_ADDRESS;
	*update_ptr = newptr;
	return addr;
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
ixgbe_local_timer(void *arg)
{
	struct adapter *adapter = arg;
	struct ifnet   *ifp = adapter->ifp;
	device_t	dev = adapter->dev;
	struct tx_ring *txr = adapter->tx_rings;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	/* Check for pluggable optics */
	if (adapter->sfp_probe)
		if (!ixgbe_sfp_probe(adapter))
			goto out; /* Nothing to do */

	ixgbe_update_link_status(adapter);
	ixgbe_update_stats_counters(adapter);

	/* Debug display */
	if (ixgbe_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING)
		ixgbe_print_hw_stats(adapter);

	/*
	 * If the interface has been paused
	 * then don't do the watchdog check
	 */
	if (IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF)
		goto out;
	/*
	** Check for time since any descriptor was cleaned
	*/
        for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		if (txr->watchdog_check == FALSE) {
			IXGBE_TX_UNLOCK(txr);
			continue;
		}
		if ((ticks - txr->watchdog_time) > IXGBE_WATCHDOG)
			goto hung;
		IXGBE_TX_UNLOCK(txr);
	}
out:
       	ixgbe_rearm_queues(adapter, adapter->que_mask);
	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
	return;

hung:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", txr->me,
	    IXGBE_READ_REG(&adapter->hw, IXGBE_TDH(txr->me)),
	    IXGBE_READ_REG(&adapter->hw, IXGBE_TDT(txr->me)));
	device_printf(dev,"TX(%d) desc avail = %d,"
	    "Next TX to Clean = %d\n",
	    txr->me, txr->tx_avail, txr->next_to_clean);
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	IXGBE_TX_UNLOCK(txr);
	ixgbe_init_locked(adapter);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
ixgbe_update_link_status(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring *txr = adapter->tx_rings;
	device_t dev = adapter->dev;


	if (adapter->link_up){ 
		if (adapter->link_active == FALSE) {
			if (bootverbose)
				device_printf(dev,"Link is up %d Gbps %s \n",
				    ((adapter->link_speed == 128)? 10:1),
				    "Full Duplex");
			adapter->link_active = TRUE;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (adapter->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			adapter->link_active = FALSE;
			for (int i = 0; i < adapter->num_queues;
			    i++, txr++)
				txr->watchdog_check = FALSE;
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
ixgbe_stop(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	ifp = adapter->ifp;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	INIT_DEBUGOUT("ixgbe_stop: begin\n");
	ixgbe_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	ixgbe_reset_hw(&adapter->hw);
	adapter->hw.adapter_stopped = FALSE;
	ixgbe_stop_adapter(&adapter->hw);
	callout_stop(&adapter->timer);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(&adapter->hw, 0, adapter->hw.mac.addr, 0, IXGBE_RAH_AV);

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
ixgbe_identify_hardware(struct adapter *adapter)
{
	device_t        dev = adapter->dev;

	/* Save off the information about this board */
	adapter->hw.vendor_id = pci_get_vendor(dev);
	adapter->hw.device_id = pci_get_device(dev);
	adapter->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	adapter->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	adapter->hw.subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	return;
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
static int
ixgbe_allocate_legacy(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct		ix_queue *que = adapter->queues;
	int error, rid = 0;

	/* MSI RID at 1 */
	if (adapter->msix == 1)
		rid = 1;

	/* We allocate a single interrupt resource */
	adapter->res = bus_alloc_resource_any(dev,
            SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
	TASK_INIT(&que->que_task, 0, ixgbe_handle_que, que);
	que->tq = taskqueue_create_fast("ixgbe_que", M_NOWAIT,
            taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s ixq",
            device_get_nameunit(adapter->dev));

	/* Tasklets for Link, SFP and Multispeed Fiber */
	TASK_INIT(&adapter->link_task, 0, ixgbe_handle_link, adapter);
	TASK_INIT(&adapter->mod_task, 0, ixgbe_handle_mod, adapter);
	TASK_INIT(&adapter->msf_task, 0, ixgbe_handle_msf, adapter);
#ifdef IXGBE_FDIR
	TASK_INIT(&adapter->fdir_task, 0, ixgbe_reinit_fdir, adapter);
#endif
	adapter->tq = taskqueue_create_fast("ixgbe_link", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s linkq",
	    device_get_nameunit(adapter->dev));

	if ((error = bus_setup_intr(dev, adapter->res,
            INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_legacy_irq,
            que, &adapter->tag)) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
		    "handler: %d\n", error);
		taskqueue_free(que->tq);
		taskqueue_free(adapter->tq);
		que->tq = NULL;
		adapter->tq = NULL;
		return (error);
	}
	/* For simplicity in the handlers */
	adapter->que_mask = IXGBE_EIMS_ENABLE_MASK;

	return (0);
}


/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers 
 *
 **********************************************************************/
static int
ixgbe_allocate_msix(struct adapter *adapter)
{
	device_t        dev = adapter->dev;
	struct 		ix_queue *que = adapter->queues;
	int 		error, rid, vector = 0;

	for (int i = 0; i < adapter->num_queues; i++, vector++, que++) {
		rid = vector + 1;
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
		    ixgbe_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register QUE handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, que->res, que->tag, "que %d", i);
#endif
		que->msix = vector;
        	adapter->que_mask |= (u64)(1 << que->msix);
		/*
		** Bind the msix vector, and thus the
		** ring to the corresponding cpu.
		*/
		if (adapter->num_queues > 1)
			bus_bind_intr(dev, que->res, i);

		TASK_INIT(&que->que_task, 0, ixgbe_handle_que, que);
		que->tq = taskqueue_create_fast("ixgbe_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
		taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
		    device_get_nameunit(adapter->dev));
	}

	/* and Link */
	rid = vector + 1;
	adapter->res = bus_alloc_resource_any(dev,
    	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (!adapter->res) {
		device_printf(dev,"Unable to allocate"
    	    " bus resource: Link interrupt [%d]\n", rid);
		return (ENXIO);
	}
	/* Set the link handler function */
	error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixgbe_msix_link, adapter, &adapter->tag);
	if (error) {
		adapter->res = NULL;
		device_printf(dev, "Failed to register LINK handler");
		return (error);
	}
#if __FreeBSD_version >= 800504
	bus_describe_intr(dev, adapter->res, adapter->tag, "link");
#endif
	adapter->linkvec = vector;
	/* Tasklets for Link, SFP and Multispeed Fiber */
	TASK_INIT(&adapter->link_task, 0, ixgbe_handle_link, adapter);
	TASK_INIT(&adapter->mod_task, 0, ixgbe_handle_mod, adapter);
	TASK_INIT(&adapter->msf_task, 0, ixgbe_handle_msf, adapter);
#ifdef IXGBE_FDIR
	TASK_INIT(&adapter->fdir_task, 0, ixgbe_reinit_fdir, adapter);
#endif
	adapter->tq = taskqueue_create_fast("ixgbe_link", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s linkq",
	    device_get_nameunit(adapter->dev));

	return (0);
}

/*
 * Setup Either MSI/X or MSI
 */
static int
ixgbe_setup_msix(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int rid, want, queues, msgs;

	/* Override by tuneable */
	if (ixgbe_enable_msix == 0)
		goto msi;

	/* First try MSI/X */
	rid = PCIR_BAR(MSIX_82598_BAR);
	adapter->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (!adapter->msix_mem) {
		rid += 4;	/* 82599 maps in higher BAR */
		adapter->msix_mem = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
	}
       	if (!adapter->msix_mem) {
		/* May not be enabled */
		device_printf(adapter->dev,
		    "Unable to map MSIX table \n");
		goto msi;
	}

	msgs = pci_msix_count(dev); 
	if (msgs == 0) { /* system has msix disabled */
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, adapter->msix_mem);
		adapter->msix_mem = NULL;
		goto msi;
	}

	/* Figure out a reasonable auto config value */
	queues = (mp_ncpus > (msgs-1)) ? (msgs-1) : mp_ncpus;

	if (ixgbe_num_queues != 0)
		queues = ixgbe_num_queues;

	/*
	** Want one vector (RX/TX pair) per queue
	** plus an additional for Link.
	*/
	want = queues + 1;
	if (msgs >= want)
		msgs = want;
	else {
               	device_printf(adapter->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors but %d queues wanted!\n",
		    msgs, want);
		return (0); /* Will go to Legacy setup */
	}
	if ((msgs) && pci_alloc_msix(dev, &msgs) == 0) {
               	device_printf(adapter->dev,
		    "Using MSIX interrupts with %d vectors\n", msgs);
		adapter->num_queues = queues;
		return (msgs);
	}
msi:
       	msgs = pci_msi_count(dev);
       	if (msgs == 1 && pci_alloc_msi(dev, &msgs) == 0)
               	device_printf(adapter->dev,"Using MSI interrupt\n");
	return (msgs);
}


static int
ixgbe_allocate_pci_resources(struct adapter *adapter)
{
	int             rid;
	device_t        dev = adapter->dev;

	rid = PCIR_BAR(0);
	adapter->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(adapter->pci_mem)) {
		device_printf(dev,"Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	adapter->osdep.mem_bus_space_tag =
		rman_get_bustag(adapter->pci_mem);
	adapter->osdep.mem_bus_space_handle =
		rman_get_bushandle(adapter->pci_mem);
	adapter->hw.hw_addr = (u8 *) &adapter->osdep.mem_bus_space_handle;

	/* Legacy defaults */
	adapter->num_queues = 1;
	adapter->hw.back = &adapter->osdep;

	/*
	** Now setup MSI or MSI/X, should
	** return us the number of supported
	** vectors. (Will be 1 for MSI)
	*/
	adapter->msix = ixgbe_setup_msix(adapter);
	return (0);
}

static void
ixgbe_free_pci_resources(struct adapter * adapter)
{
	struct 		ix_queue *que = adapter->queues;
	device_t	dev = adapter->dev;
	int		rid, memrid;

	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		memrid = PCIR_BAR(MSIX_82598_BAR);
	else
		memrid = PCIR_BAR(MSIX_82599_BAR);

	/*
	** There is a slight possibility of a failure mode
	** in attach that will result in entering this function
	** before interrupt resources have been initialized, and
	** in that case we do not want to execute the loops below
	** We can detect this reliably by the state of the adapter
	** res pointer.
	*/
	if (adapter->res == NULL)
		goto mem;

	/*
	**  Release all msix queue resources:
	*/
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
	}


	/* Clean the Legacy or Link interrupt last */
	if (adapter->linkvec) /* we are doing MSIX */
		rid = adapter->linkvec + 1;
	else
		(adapter->msix != 0) ? (rid = 1):(rid = 0);

	if (adapter->tag != NULL) {
		bus_teardown_intr(dev, adapter->res, adapter->tag);
		adapter->tag = NULL;
	}
	if (adapter->res != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, rid, adapter->res);

mem:
	if (adapter->msix)
		pci_release_msi(dev);

	if (adapter->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    memrid, adapter->msix_mem);

	if (adapter->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->pci_mem);

	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
ixgbe_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ifnet   *ifp;

	INIT_DEBUGOUT("ixgbe_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		panic("%s: can not if_alloc()\n", device_get_nameunit(dev));
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = 1000000000;
	ifp->if_init = ixgbe_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixgbe_ioctl;
	ifp->if_start = ixgbe_start;
#if __FreeBSD_version >= 800000
	ifp->if_transmit = ixgbe_mq_start;
	ifp->if_qflush = ixgbe_qflush;
#endif
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 2;

	ether_ifattach(ifp, adapter->hw.mac.addr);

	adapter->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_TSO4 | IFCAP_VLAN_HWCSUM;
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU | IFCAP_LRO;

	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixgbe_media_change,
		     ixgbe_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | adapter->optics |
	    IFM_FDX, 0, NULL);
	if (hw->device_id == IXGBE_DEV_ID_82598AT) {
		ifmedia_add(&adapter->media,
		    IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
		ifmedia_add(&adapter->media,
		    IFM_ETHER | IFM_1000_T, 0, NULL);
	}
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return;
}

static void
ixgbe_config_link(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	autoneg, err = 0;
	bool	sfp, negotiate;

	sfp = ixgbe_is_sfp(hw);

	if (sfp) { 
		if (hw->phy.multispeed_fiber) {
			hw->mac.ops.setup_sfp(hw);
			taskqueue_enqueue(adapter->tq, &adapter->msf_task);
		} else
			taskqueue_enqueue(adapter->tq, &adapter->mod_task);
	} else {
		if (hw->mac.ops.check_link)
			err = ixgbe_check_link(hw, &autoneg,
			    &adapter->link_up, FALSE);
		if (err)
			goto out;
		autoneg = hw->phy.autoneg_advertised;
		if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
                	err  = hw->mac.ops.get_link_capabilities(hw,
			    &autoneg, &negotiate);
		if (err)
			goto out;
		if (hw->mac.ops.setup_link)
                	err = hw->mac.ops.setup_link(hw, autoneg,
			    negotiate, adapter->link_up);
	}
out:
	return;
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/
static void
ixgbe_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

static int
ixgbe_dma_malloc(struct adapter *adapter, bus_size_t size,
		struct ixgbe_dma_alloc *dma, int mapflags)
{
	device_t dev = adapter->dev;
	int             r;

	r = bus_dma_tag_create(bus_get_dma_tag(adapter->dev),	/* parent */
			       DBA_ALIGN, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
			       &dma->dma_tag);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dma_tag_create failed; "
		       "error %u\n", r);
		goto fail_0;
	}
	r = bus_dmamem_alloc(dma->dma_tag, (void **)&dma->dma_vaddr,
			     BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dmamem_alloc failed; "
		       "error %u\n", r);
		goto fail_1;
	}
	r = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
			    size,
			    ixgbe_dmamap_cb,
			    &dma->dma_paddr,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		device_printf(dev,"ixgbe_dma_malloc: bus_dmamap_load failed; "
		       "error %u\n", r);
		goto fail_2;
	}
	dma->dma_size = size;
	return (0);
fail_2:
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
fail_1:
	bus_dma_tag_destroy(dma->dma_tag);
fail_0:
	dma->dma_map = NULL;
	dma->dma_tag = NULL;
	return (r);
}

static void
ixgbe_dma_free(struct adapter *adapter, struct ixgbe_dma_alloc *dma)
{
	bus_dmamap_sync(dma->dma_tag, dma->dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
	bus_dma_tag_destroy(dma->dma_tag);
}


/*********************************************************************
 *
 *  Allocate memory for the transmit and receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int
ixgbe_allocate_queues(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct ix_queue	*que;
	struct tx_ring	*txr;
	struct rx_ring	*rxr;
	int rsize, tsize, error = IXGBE_SUCCESS;
	int txconf = 0, rxconf = 0;

        /* First allocate the top level queue structs */
        if (!(adapter->queues =
            (struct ix_queue *) malloc(sizeof(struct ix_queue) *
            adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                device_printf(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                goto fail;
        }

	/* First allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto tx_fail;
	}

	/* Next allocate the RX */
	if (!(adapter->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto rx_fail;
	}

	/* For the ring itself */
	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), DBA_ALIGN);

	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (int i = 0; i < adapter->num_queues; i++, txconf++) {
		/* Set up some basics */
		txr = &adapter->tx_rings[i];
		txr->adapter = adapter;
		txr->me = i;

		/* Initialize the TX side lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(dev), txr->me);
		mtx_init(&txr->tx_mtx, txr->mtx_name, NULL, MTX_DEF);

		if (ixgbe_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (union ixgbe_adv_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	/* Now allocate transmit buffers for the ring */
        	if (ixgbe_allocate_transmit_buffers(txr)) {
			device_printf(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#if __FreeBSD_version >= 800000
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(IXGBE_BR_SIZE, M_DEVBUF,
		    M_WAITOK, &txr->tx_mtx);
		if (txr->br == NULL) {
			device_printf(dev,
			    "Critical Failure setting up buf ring\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#endif
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	for (int i = 0; i < adapter->num_queues; i++, rxconf++) {
		rxr = &adapter->rx_rings[i];
		/* Set up some basics */
		rxr->adapter = adapter;
		rxr->me = i;

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), rxr->me);
		mtx_init(&rxr->rx_mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (ixgbe_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (union ixgbe_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (ixgbe_allocate_receive_buffers(rxr)) {
			device_printf(dev,
			    "Critical Failure setting up receive buffers\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
	}

	/*
	** Finally set up the queue holding structs
	*/
	for (int i = 0; i < adapter->num_queues; i++) {
		que = &adapter->queues[i];
		que->adapter = adapter;
		que->txr = &adapter->tx_rings[i];
		que->rxr = &adapter->rx_rings[i];
	}

	return (0);

err_rx_desc:
	for (rxr = adapter->rx_rings; rxconf > 0; rxr++, rxconf--)
		ixgbe_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		ixgbe_dma_free(adapter, &txr->txdma);
	free(adapter->rx_rings, M_DEVBUF);
rx_fail:
	free(adapter->tx_rings, M_DEVBUF);
tx_fail:
	free(adapter->queues, M_DEVBUF);
fail:
	return (error);
}

/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/
static int
ixgbe_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct ixgbe_tx_buf *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
			       1, 0,		/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IXGBE_TSO_SIZE,		/* maxsize */
			       ixgbe_num_segs,		/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->txtag))) {
		device_printf(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct ixgbe_tx_buf *) malloc(sizeof(struct ixgbe_tx_buf) *
	    adapter->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

        /* Create the descriptor buffer dma maps */
	txbuf = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, txbuf++) {
		error = bus_dmamap_create(txr->txtag, 0, &txbuf->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
	}

	return 0;
fail:
	/* We free all, it handles case where we are in the middle */
	ixgbe_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
ixgbe_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *txbuf;
	int i;

	/* Clear the old ring contents */
	IXGBE_TX_LOCK(txr);
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * adapter->num_tx_desc);
	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Free any existing tx buffers. */
        txbuf = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, txbuf++) {
		if (txbuf->m_head != NULL) {
			bus_dmamap_sync(txr->txtag, txbuf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag, txbuf->map);
			m_freem(txbuf->m_head);
			txbuf->m_head = NULL;
		}
		/* Clear the EOP index */
		txbuf->eop_index = -1;
        }

#ifdef IXGBE_FDIR
	/* Set the rate at which we sample packets */
	if (adapter->hw.mac.type == ixgbe_mac_82599EB)
		txr->atr_sample = atr_sample_rate;
#endif

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IXGBE_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static int
ixgbe_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		ixgbe_setup_transmit_ring(txr);

	return (0);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
ixgbe_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;

	/* Setup the Base and Length of the Tx Descriptor Ring */

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64	tdba = txr->txdma.dma_paddr;
		u32	txctrl;

		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(i),
		    adapter->num_tx_desc * sizeof(struct ixgbe_legacy_tx_desc));

		/* Setup the HW Tx Head and Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(i), 0);

		/* Setup Transmit Descriptor Cmd Settings */
		txr->txd_cmd = IXGBE_TXD_CMD_IFCS;
		txr->watchdog_check = FALSE;

		/* Disable Head Writeback */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
			break;
		case ixgbe_mac_82599EB:
		default:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(i));
			break;
                }
		txctrl &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), txctrl);
			break;
		case ixgbe_mac_82599EB:
		default:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(i), txctrl);
			break;
		}

	}

	if (hw->mac.type == ixgbe_mac_82599EB) {
		u32 dmatxctl, rttdcs;
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
		/* Disable arbiter to set MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
		IXGBE_WRITE_REG(hw, IXGBE_MTQC, IXGBE_MTQC_64Q_1PB);
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}

	return;
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
ixgbe_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IXGBE_TX_LOCK(txr);
		ixgbe_free_transmit_buffers(txr);
		ixgbe_dma_free(adapter, &txr->txdma);
		IXGBE_TX_UNLOCK(txr);
		IXGBE_TX_LOCK_DESTROY(txr);
	}
	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
ixgbe_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_tx_buf *tx_buffer;
	int             i;

	INIT_DEBUGOUT("free_transmit_ring: begin");

	if (txr->tx_buffers == NULL)
		return;

	tx_buffer = txr->tx_buffers;
	for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
		if (tx_buffer->m_head != NULL) {
			bus_dmamap_sync(txr->txtag, tx_buffer->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag,
			    tx_buffer->map);
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
			if (tx_buffer->map != NULL) {
				bus_dmamap_destroy(txr->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		} else if (tx_buffer->map != NULL) {
			bus_dmamap_unload(txr->txtag,
			    tx_buffer->map);
			bus_dmamap_destroy(txr->txtag,
			    tx_buffer->map);
			tx_buffer->map = NULL;
		}
	}
#if __FreeBSD_version >= 800000
	if (txr->br != NULL)
		buf_ring_free(txr->br, M_DEVBUF);
#endif
	if (txr->tx_buffers != NULL) {
		free(txr->tx_buffers, M_DEVBUF);
		txr->tx_buffers = NULL;
	}
	if (txr->txtag != NULL) {
		bus_dma_tag_destroy(txr->txtag);
		txr->txtag = NULL;
	}
	return;
}

/*********************************************************************
 *
 *  Advanced Context Descriptor setup for VLAN or CSUM
 *
 **********************************************************************/

static boolean_t
ixgbe_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixgbe_tx_buf        *tx_buffer;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	int  ehdrlen, ip_hlen = 0;
	u16	etype;
	u8	ipproto = 0;
	bool	offload = TRUE;
	int ctxd = txr->next_avail_desc;
	u16 vtag = 0;


	if ((mp->m_pkthdr.csum_flags & CSUM_OFFLOAD) == 0)
		offload = FALSE;

	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the descriptor itself.
	*/
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else if (offload == FALSE)
		return FALSE;

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	/* Set the ether header length */
	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;

	switch (etype) {
		case ETHERTYPE_IP:
			ip = (struct ip *)(mp->m_data + ehdrlen);
			ip_hlen = ip->ip_hl << 2;
			if (mp->m_len < ehdrlen + ip_hlen)
				return (FALSE);
			ipproto = ip->ip_p;
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
			break;
		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
			ip_hlen = sizeof(struct ip6_hdr);
			if (mp->m_len < ehdrlen + ip_hlen)
				return (FALSE);
			ipproto = ip6->ip6_nxt;
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
			break;
		default:
			offload = FALSE;
			break;
	}

	vlan_macip_lens |= ip_hlen;
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	switch (ipproto) {
		case IPPROTO_TCP:
			if (mp->m_pkthdr.csum_flags & CSUM_TCP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
			break;

		case IPPROTO_UDP:
			if (mp->m_pkthdr.csum_flags & CSUM_UDP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP;
			break;

#if __FreeBSD_version >= 800000
		case IPPROTO_SCTP:
			if (mp->m_pkthdr.csum_flags & CSUM_SCTP)
				type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_SCTP;
			break;
#endif
		default:
			offload = FALSE;
			break;
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

	tx_buffer->m_head = NULL;
	tx_buffer->eop_index = -1;

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return (offload);
}

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  adapters using advanced tx descriptors
 *
 **********************************************************************/
static boolean_t
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp, u32 *paylen)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixgbe_tx_buf        *tx_buffer;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	u32 mss_l4len_idx = 0;
	u16 vtag = 0;
	int ctxd, ehdrlen,  hdrlen, ip_hlen, tcp_hlen;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct tcphdr *th;


	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) 
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	else
		ehdrlen = ETHER_HDR_LEN;

        /* Ensure we have at least the IP+TCP header in the first mbuf. */
        if (mp->m_len < ehdrlen + sizeof(struct ip) + sizeof(struct tcphdr))
		return FALSE;

	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	ip = (struct ip *)(mp->m_data + ehdrlen);
	if (ip->ip_p != IPPROTO_TCP)
		return FALSE;   /* 0 */
	ip->ip_sum = 0;
	ip_hlen = ip->ip_hl << 2;
	th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
	th->th_sum = in_pseudo(ip->ip_src.s_addr,
	    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
	tcp_hlen = th->th_off << 2;
	hdrlen = ehdrlen + ip_hlen + tcp_hlen;

	/* This is used in the transmit desc in encap */
	*paylen = mp->m_pkthdr.len - hdrlen;

	/* VLAN MACLEN IPLEN */
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
                vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);


	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.tso_segsz << IXGBE_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << IXGBE_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);
	tx_buffer->m_head = NULL;
	tx_buffer->eop_index = -1;

	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	return TRUE;
}

#ifdef IXGBE_FDIR
/*
** This routine parses packet headers so that Flow
** Director can make a hashed filter table entry 
** allowing traffic flows to be identified and kept
** on the same cpu.  This would be a performance
** hit, but we only do it at IXGBE_FDIR_RATE of
** packets.
*/
static void
ixgbe_atr(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter			*adapter = txr->adapter;
	struct ix_queue			*que;
	struct ixgbe_atr_input		atr_input;
	struct ip			*ip;
	struct tcphdr			*th;
	struct udphdr			*uh;
	struct ether_vlan_header	*eh;
	int  				ehdrlen, ip_hlen;
	u16 etype, vlan_id, src_port, dst_port, flex_bytes;
	u32 src_ipv4_addr, dst_ipv4_addr;
	u8 l4type = 0, ipproto = 0;

	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) 
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	else
		ehdrlen = ETHER_HDR_LEN;
	etype = ntohs(eh->evl_proto);

	/* Only handling IPv4 */
	if (etype != ETHERTYPE_IP)
		return;

	ip = (struct ip *)(mp->m_data + ehdrlen);
	ipproto = ip->ip_p;
	ip_hlen = ip->ip_hl << 2;
	src_port = dst_port = 0;

	/* check if we're UDP or TCP */
	switch (ipproto) {
	case IPPROTO_TCP:
		th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
		src_port = th->th_sport;
		dst_port = th->th_dport;
		l4type |= IXGBE_ATR_L4TYPE_TCP;
		break;
	case IPPROTO_UDP:
		uh = (struct udphdr *)((caddr_t)ip + ip_hlen);
		src_port = uh->uh_sport;
		dst_port = uh->uh_dport;
		l4type |= IXGBE_ATR_L4TYPE_UDP;
		break;
	default:
		return;
	}

	memset(&atr_input, 0, sizeof(struct ixgbe_atr_input));

	vlan_id = htole16(mp->m_pkthdr.ether_vtag);
	src_ipv4_addr = ip->ip_src.s_addr;
	dst_ipv4_addr = ip->ip_dst.s_addr;
	flex_bytes = etype;
	que = &adapter->queues[txr->me];

	ixgbe_atr_set_vlan_id_82599(&atr_input, vlan_id);
	ixgbe_atr_set_src_port_82599(&atr_input, dst_port);
	ixgbe_atr_set_dst_port_82599(&atr_input, src_port);
	ixgbe_atr_set_flex_byte_82599(&atr_input, flex_bytes);
	ixgbe_atr_set_l4type_82599(&atr_input, l4type);
	/* src and dst are inverted, think how the receiver sees them */
	ixgbe_atr_set_src_ipv4_82599(&atr_input, dst_ipv4_addr);
	ixgbe_atr_set_dst_ipv4_82599(&atr_input, src_ipv4_addr);

	/* This assumes the Rx queue and Tx queue are bound to the same CPU */
	ixgbe_fdir_add_signature_filter_82599(&adapter->hw,
	    &atr_input, que->msix);
}
#endif

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static boolean_t
ixgbe_txeof(struct tx_ring *txr)
{
	struct adapter	*adapter = txr->adapter;
	struct ifnet	*ifp = adapter->ifp;
	u32	first, last, done;
	struct ixgbe_tx_buf *tx_buffer;
	struct ixgbe_legacy_tx_desc *tx_desc, *eop_desc;

	mtx_assert(&txr->tx_mtx, MA_OWNED);

	if (txr->tx_avail == adapter->num_tx_desc)
		return FALSE;

	first = txr->next_to_clean;
	tx_buffer = &txr->tx_buffers[first];
	/* For cleanup we just use legacy struct */
	tx_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
	last = tx_buffer->eop_index;
	if (last == -1)
		return FALSE;
	eop_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];

	/*
	** Get the index of the first descriptor
	** BEYOND the EOP and call that 'done'.
	** I do this so the comparison in the
	** inner while loop below can be simple
	*/
	if (++last == adapter->num_tx_desc) last = 0;
	done = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);
	/*
	** Only the EOP descriptor of a packet now has the DD
	** bit set, this is what we look for...
	*/
	while (eop_desc->upper.fields.status & IXGBE_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			tx_desc->buffer_addr = 0;
			++txr->tx_avail;

			if (tx_buffer->m_head) {
				txr->bytes +=
				    tx_buffer->m_head->m_pkthdr.len;
				bus_dmamap_sync(txr->txtag,
				    tx_buffer->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(txr->txtag,
				    tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
				tx_buffer->map = NULL;
			}
			tx_buffer->eop_index = -1;
			txr->watchdog_time = ticks;

			if (++first == adapter->num_tx_desc)
				first = 0;

			tx_buffer = &txr->tx_buffers[first];
			tx_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
		}
		++txr->packets;
		++ifp->if_opackets;
		/* See if there is more work now */
		last = tx_buffer->eop_index;
		if (last != -1) {
			eop_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];
			/* Get next done point */
			if (++last == adapter->num_tx_desc) last = 0;
			done = last;
		} else
			break;
	}
	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	txr->next_to_clean = first;

	/*
	 * If we have enough room, clear IFF_DRV_OACTIVE to tell the stack that
	 * it is OK to send packets. If there are no pending descriptors,
	 * clear the timeout. Otherwise, if some descriptors have been freed,
	 * restart the timeout.
	 */
	if (txr->tx_avail > IXGBE_TX_CLEANUP_THRESHOLD) {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if (txr->tx_avail == adapter->num_tx_desc) {
			txr->watchdog_check = FALSE;
			return FALSE;
		}
	}

	return TRUE;
}

/*********************************************************************
 *
 *  Refresh mbuf buffers for RX descriptor rings
 *   - now keeps its own state so discards due to resource
 *     exhaustion are unnecessary, if an mbuf cannot be obtained
 *     it just returns, keeping its placeholder, thus it can simply
 *     be recalled to try again.
 *
 **********************************************************************/
static void
ixgbe_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	bus_dma_segment_t	hseg[1];
	bus_dma_segment_t	pseg[1];
	struct ixgbe_rx_buf	*rxbuf;
	struct mbuf		*mh, *mp;
	int			i, nsegs, error, cleaned;

	i = rxr->next_to_refresh;
	cleaned = -1; /* Signify no completions */
	while (i != limit) {
		rxbuf = &rxr->rx_buffers[i];
		if ((rxbuf->m_head == NULL) && (rxr->hdr_split)) {
			mh = m_gethdr(M_DONTWAIT, MT_DATA);
			if (mh == NULL)
				goto update;
			mh->m_pkthdr.len = mh->m_len = MHLEN;
			mh->m_len = MHLEN;
			mh->m_flags |= M_PKTHDR;
			m_adj(mh, ETHER_ALIGN);
			/* Get the memory mapping */
			error = bus_dmamap_load_mbuf_sg(rxr->htag,
			    rxbuf->hmap, mh, hseg, &nsegs, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("GET BUF: dmamap load"
				    " failure - %d\n", error);
				m_free(mh);
				goto update;
			}
			rxbuf->m_head = mh;
			bus_dmamap_sync(rxr->htag, rxbuf->hmap,
			    BUS_DMASYNC_PREREAD);
			rxr->rx_base[i].read.hdr_addr =
			    htole64(hseg[0].ds_addr);
		}

		if (rxbuf->m_pack == NULL) {
			mp = m_getjcl(M_DONTWAIT, MT_DATA,
			    M_PKTHDR, adapter->rx_mbuf_sz);
			if (mp == NULL)
				goto update;
			mp->m_pkthdr.len = mp->m_len = adapter->rx_mbuf_sz;
			/* Get the memory mapping */
			error = bus_dmamap_load_mbuf_sg(rxr->ptag,
			    rxbuf->pmap, mp, pseg, &nsegs, BUS_DMA_NOWAIT);
			if (error != 0) {
				printf("GET BUF: dmamap load"
				    " failure - %d\n", error);
				m_free(mp);
				goto update;
			}
			rxbuf->m_pack = mp;
			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
			    BUS_DMASYNC_PREREAD);
			rxr->rx_base[i].read.pkt_addr =
			    htole64(pseg[0].ds_addr);
		}

		cleaned = i;
		/* Calculate next index */
		if (++i == adapter->num_rx_desc)
			i = 0;
		/* This is the work marker for refresh */
		rxr->next_to_refresh = i;
	}
update:
	if (cleaned != -1) /* If we refreshed some, bump tail */
		IXGBE_WRITE_REG(&adapter->hw,
		    IXGBE_RDT(rxr->me), cleaned);
	return;
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one
 *  rx_buffer per received packet, the maximum number of rx_buffer's
 *  that we'll need is equal to the number of receive descriptors
 *  that we've allocated.
 *
 **********************************************************************/
static int
ixgbe_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	device_t 		dev = adapter->dev;
	struct ixgbe_rx_buf 	*rxbuf;
	int             	i, bsize, error;

	bsize = sizeof(struct ixgbe_rx_buf) * adapter->num_rx_desc;
	if (!(rxr->rx_buffers =
	    (struct ixgbe_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
				   1, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MSIZE,		/* maxsize */
				   1,			/* nsegments */
				   MSIZE,		/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->htag))) {
		device_printf(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
				   1, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MJUMPAGESIZE,	/* maxsize */
				   1,			/* nsegments */
				   MJUMPAGESIZE,	/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->ptag))) {
		device_printf(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	for (i = 0; i < adapter->num_rx_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->htag,
		    BUS_DMA_NOWAIT, &rxbuf->hmap);
		if (error) {
			device_printf(dev, "Unable to create RX head map\n");
			goto fail;
		}
		error = bus_dmamap_create(rxr->ptag,
		    BUS_DMA_NOWAIT, &rxbuf->pmap);
		if (error) {
			device_printf(dev, "Unable to create RX pkt map\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	ixgbe_free_receive_structures(adapter);
	return (error);
}

/*
** Used to detect a descriptor that has
** been merged by Hardware RSC.
*/
static inline u32
ixgbe_rsc_count(union ixgbe_adv_rx_desc *rx)
{
	return (le32toh(rx->wb.lower.lo_dword.data) &
	    IXGBE_RXDADV_RSCCNT_MASK) >> IXGBE_RXDADV_RSCCNT_SHIFT;
}

/*********************************************************************
 *
 *  Initialize Hardware RSC (LRO) feature on 82599
 *  for an RX ring, this is toggled by the LRO capability
 *  even though it is transparent to the stack.
 *
 **********************************************************************/
static void
ixgbe_setup_hw_rsc(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	struct	ixgbe_hw	*hw = &adapter->hw;
	u32			rscctrl, rdrxctl;

	rdrxctl = IXGBE_READ_REG(hw, IXGBE_RDRXCTL);
	rdrxctl &= ~IXGBE_RDRXCTL_RSCFRSTSIZE;
	rdrxctl |= IXGBE_RDRXCTL_CRCSTRIP;
	rdrxctl |= IXGBE_RDRXCTL_RSCACKC;
	IXGBE_WRITE_REG(hw, IXGBE_RDRXCTL, rdrxctl);

	rscctrl = IXGBE_READ_REG(hw, IXGBE_RSCCTL(rxr->me));
	rscctrl |= IXGBE_RSCCTL_RSCEN;
	/*
	** Limit the total number of descriptors that
	** can be combined, so it does not exceed 64K
	*/
	if (adapter->rx_mbuf_sz == MCLBYTES)
		rscctrl |= IXGBE_RSCCTL_MAXDESC_16;
	else  /* using 4K clusters */
		rscctrl |= IXGBE_RSCCTL_MAXDESC_8;
	IXGBE_WRITE_REG(hw, IXGBE_RSCCTL(rxr->me), rscctrl);

	/* Enable TCP header recognition */
	IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0),
	    (IXGBE_READ_REG(hw, IXGBE_PSRTYPE(0)) |
	    IXGBE_PSRTYPE_TCPHDR));

	/* Disable RSC for ACK packets */
	IXGBE_WRITE_REG(hw, IXGBE_RSCDBU,
	    (IXGBE_RSCDBU_RSCACKDIS | IXGBE_READ_REG(hw, IXGBE_RSCDBU)));

	rxr->hw_rsc = TRUE;
}


static void     
ixgbe_free_receive_ring(struct rx_ring *rxr)
{ 
	struct  adapter         *adapter;
	struct ixgbe_rx_buf       *rxbuf;
	int i;

	adapter = rxr->adapter;
	for (i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head != NULL) {
			bus_dmamap_sync(rxr->htag, rxbuf->hmap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->htag, rxbuf->hmap);
			rxbuf->m_head->m_flags |= M_PKTHDR;
			m_freem(rxbuf->m_head);
		}
		if (rxbuf->m_pack != NULL) {
			bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->ptag, rxbuf->pmap);
			rxbuf->m_pack->m_flags |= M_PKTHDR;
			m_freem(rxbuf->m_pack);
		}
		rxbuf->m_head = NULL;
		rxbuf->m_pack = NULL;
	}
}


/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
ixgbe_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter 	*adapter;
	struct ifnet		*ifp;
	device_t		dev;
	struct ixgbe_rx_buf	*rxbuf;
	bus_dma_segment_t	pseg[1], hseg[1];
	struct lro_ctrl		*lro = &rxr->lro;
	int			rsize, nsegs, error = 0;

	adapter = rxr->adapter;
	ifp = adapter->ifp;
	dev = adapter->dev;

	/* Clear the ring contents */
	IXGBE_RX_LOCK(rxr);
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);

	/* Free current RX buffer structs and their mbufs */
	ixgbe_free_receive_ring(rxr);

	/* Configure header split? */
	if (ixgbe_header_split)
		rxr->hdr_split = TRUE;

	/* Now replenish the mbufs */
	for (int j = 0; j != adapter->num_rx_desc; ++j) {
		struct mbuf	*mh, *mp;

		rxbuf = &rxr->rx_buffers[j];
		/*
		** Dont allocate mbufs if not
		** doing header split, its wasteful
		*/ 
		if (rxr->hdr_split == FALSE)
			goto skip_head;

		/* First the header */
		rxbuf->m_head = m_gethdr(M_NOWAIT, MT_DATA);
		if (rxbuf->m_head == NULL) {
			error = ENOBUFS;
			goto fail;
		}
		m_adj(rxbuf->m_head, ETHER_ALIGN);
		mh = rxbuf->m_head;
		mh->m_len = mh->m_pkthdr.len = MHLEN;
		mh->m_flags |= M_PKTHDR;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->htag,
		    rxbuf->hmap, rxbuf->m_head, hseg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) /* Nothing elegant to do here */
			goto fail;
		bus_dmamap_sync(rxr->htag,
		    rxbuf->hmap, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->rx_base[j].read.hdr_addr = htole64(hseg[0].ds_addr);

skip_head:
		/* Now the payload cluster */
		rxbuf->m_pack = m_getjcl(M_NOWAIT, MT_DATA,
		    M_PKTHDR, adapter->rx_mbuf_sz);
		if (rxbuf->m_pack == NULL) {
			error = ENOBUFS;
                        goto fail;
		}
		mp = rxbuf->m_pack;
		mp->m_pkthdr.len = mp->m_len = adapter->rx_mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->ptag,
		    rxbuf->pmap, mp, pseg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0)
                        goto fail;
		bus_dmamap_sync(rxr->ptag,
		    rxbuf->pmap, BUS_DMASYNC_PREREAD);
		/* Update descriptor */
		rxr->rx_base[j].read.pkt_addr = htole64(pseg[0].ds_addr);
	}


	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;
	rxr->lro_enabled = FALSE;
	rxr->rx_split_packets = 0;
	rxr->rx_bytes = 0;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	** Now set up the LRO interface:
	** 82598 uses software LRO, the
	** 82599 uses a hardware assist.
	*/
	if ((adapter->hw.mac.type == ixgbe_mac_82599EB) &&
	    (ifp->if_capenable & IFCAP_RXCSUM) &&
	    (ifp->if_capenable & IFCAP_LRO))
		ixgbe_setup_hw_rsc(rxr);
	else if (ifp->if_capenable & IFCAP_LRO) {
		int err = tcp_lro_init(lro);
		if (err) {
			device_printf(dev, "LRO Initialization failed!\n");
			goto fail;
		}
		INIT_DEBUGOUT("RX Soft LRO Initialized\n");
		rxr->lro_enabled = TRUE;
		lro->ifp = adapter->ifp;
	}

	IXGBE_RX_UNLOCK(rxr);
	return (0);

fail:
	ixgbe_free_receive_ring(rxr);
	IXGBE_RX_UNLOCK(rxr);
	return (error);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
ixgbe_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int j;

	for (j = 0; j < adapter->num_queues; j++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'j' failed, so its the terminus.
	 */
	for (int i = 0; i < j; ++i) {
		rxr = &adapter->rx_rings[i];
		ixgbe_free_receive_ring(rxr);
	}

	return (ENOBUFS);
}

/*********************************************************************
 *
 *  Setup receive registers and features.
 *
 **********************************************************************/
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

static void
ixgbe_initialize_receive_units(struct adapter *adapter)
{
	struct	rx_ring	*rxr = adapter->rx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ifnet   *ifp = adapter->ifp;
	u32		bufsz, rxctrl, fctrl, srrctl, rxcsum;
	u32		reta, mrqc = 0, hlreg, random[10];


	/*
	 * Make sure receives are disabled while
	 * setting up the descriptor ring
	 */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL,
	    rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF;
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);

	/* Set for Jumbo Frames? */
	hlreg = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU) {
		hlreg |= IXGBE_HLREG0_JUMBOEN;
		bufsz = 4096 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	} else {
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
		bufsz = 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	}
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg);

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;

		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(i),
			       (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(i), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Set up the SRRCTL register */
		srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(i));
		srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		srrctl |= bufsz;
		if (rxr->hdr_split) {
			/* Use a standard mbuf for the header */
			srrctl |= ((IXGBE_RX_HDR <<
			    IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT)
			    & IXGBE_SRRCTL_BSIZEHDR_MASK);
			srrctl |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
		} else
			srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
		IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(i), srrctl);

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(hw, IXGBE_RDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(i), 0);
	}

	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		/* PSRTYPE must be initialized in 82599 */
		u32 psrtype = IXGBE_PSRTYPE_TCPHDR |
			      IXGBE_PSRTYPE_UDPHDR |
			      IXGBE_PSRTYPE_IPV4HDR |
			      IXGBE_PSRTYPE_IPV6HDR;
		IXGBE_WRITE_REG(hw, IXGBE_PSRTYPE(0), psrtype);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	/* Setup RSS */
	if (adapter->num_queues > 1) {
		int i, j;
		reta = 0;

		/* set up random bits */
		arc4rand(&random, sizeof(random), 0);

		/* Set up the redirection table */
		for (i = 0, j = 0; i < 128; i++, j++) {
			if (j == adapter->num_queues) j = 0;
			reta = (reta << 8) | (j * 0x11);
			if ((i & 3) == 3)
				IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
		}

		/* Now fill our hash function seeds */
		for (int i = 0; i < 10; i++)
			IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), random[i]);

		/* Perform hash on these packet types */
		mrqc = IXGBE_MRQC_RSSEN
		     | IXGBE_MRQC_RSS_FIELD_IPV4
		     | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		     | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		     | IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP
		     | IXGBE_MRQC_RSS_FIELD_IPV6_EX
		     | IXGBE_MRQC_RSS_FIELD_IPV6
		     | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
		     | IXGBE_MRQC_RSS_FIELD_IPV6_UDP
		     | IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
		IXGBE_WRITE_REG(hw, IXGBE_MRQC, mrqc);

		/* RSS and RX IPP Checksum are mutually exclusive */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}

	if (ifp->if_capenable & IFCAP_RXCSUM)
		rxcsum |= IXGBE_RXCSUM_PCSD;

	if (!(rxcsum & IXGBE_RXCSUM_PCSD))
		rxcsum |= IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	return;
}

/*********************************************************************
 *
 *  Free all receive rings.
 *
 **********************************************************************/
static void
ixgbe_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		struct lro_ctrl		*lro = &rxr->lro;
		ixgbe_free_receive_buffers(rxr);
		/* Free LRO memory */
		tcp_lro_free(lro);
		/* Free the ring memory as well */
		ixgbe_dma_free(adapter, &rxr->rxdma);
	}

	free(adapter->rx_rings, M_DEVBUF);
}


/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
static void
ixgbe_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixgbe_rx_buf	*rxbuf;

	INIT_DEBUGOUT("free_receive_structures: begin");

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->m_head != NULL) {
				bus_dmamap_sync(rxr->htag, rxbuf->hmap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->htag, rxbuf->hmap);
				rxbuf->m_head->m_flags |= M_PKTHDR;
				m_freem(rxbuf->m_head);
			}
			if (rxbuf->m_pack != NULL) {
				bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->ptag, rxbuf->pmap);
				rxbuf->m_pack->m_flags |= M_PKTHDR;
				m_freem(rxbuf->m_pack);
			}
			rxbuf->m_head = NULL;
			rxbuf->m_pack = NULL;
			if (rxbuf->hmap != NULL) {
				bus_dmamap_destroy(rxr->htag, rxbuf->hmap);
				rxbuf->hmap = NULL;
			}
			if (rxbuf->pmap != NULL) {
				bus_dmamap_destroy(rxr->ptag, rxbuf->pmap);
				rxbuf->pmap = NULL;
			}
		}
		if (rxr->rx_buffers != NULL) {
			free(rxr->rx_buffers, M_DEVBUF);
			rxr->rx_buffers = NULL;
		}
	}

	if (rxr->htag != NULL) {
		bus_dma_tag_destroy(rxr->htag);
		rxr->htag = NULL;
	}
	if (rxr->ptag != NULL) {
		bus_dma_tag_destroy(rxr->ptag);
		rxr->ptag = NULL;
	}

	return;
}

static __inline void
ixgbe_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u32 ptype)
{
                 
        /*
         * ATM LRO is only for IPv4/TCP packets and TCP checksum of the packet
         * should be computed by hardware. Also it should not have VLAN tag in
         * ethernet header.
         */
        if (rxr->lro_enabled &&
            (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0 &&
            (ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
            (ptype & (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP)) ==
            (IXGBE_RXDADV_PKTTYPE_IPV4 | IXGBE_RXDADV_PKTTYPE_TCP) &&
            (m->m_pkthdr.csum_flags & (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) ==
            (CSUM_DATA_VALID | CSUM_PSEUDO_HDR)) {
                /*
                 * Send to the stack if:
                 **  - LRO not enabled, or
                 **  - no LRO resources, or
                 **  - lro enqueue fails
                 */
                if (rxr->lro.lro_cnt != 0)
                        if (tcp_lro_rx(&rxr->lro, m, 0) == 0)
                                return;
        }
        (*ifp->if_input)(ifp, m);
}

static __inline void
ixgbe_rx_discard(struct rx_ring *rxr, int i)
{
	struct adapter		*adapter = rxr->adapter;
	struct ixgbe_rx_buf	*rbuf;
	struct mbuf		*mh, *mp;

	rbuf = &rxr->rx_buffers[i];
        if (rbuf->fmp != NULL) /* Partial chain ? */
                m_freem(rbuf->fmp);

	mh = rbuf->m_head;
	mp = rbuf->m_pack;

	/* Reuse loaded DMA map and just update mbuf chain */
	mh->m_len = MHLEN;
	mh->m_flags |= M_PKTHDR;
	mh->m_next = NULL;

	mp->m_len = mp->m_pkthdr.len = adapter->rx_mbuf_sz;
	mp->m_data = mp->m_ext.ext_buf;
	mp->m_next = NULL;
	return;
}


/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *  We loop at most count times if count is > 0, or until done if
 *  count < 0.
 *
 *  Return TRUE for more work, FALSE for all clean.
 *********************************************************************/
static bool
ixgbe_rxeof(struct ix_queue *que, int count)
{
	struct adapter		*adapter = que->adapter;
	struct rx_ring		*rxr = que->rxr;
	struct ifnet		*ifp = adapter->ifp;
	struct lro_ctrl		*lro = &rxr->lro;
	struct lro_entry	*queued;
	int			i, nextp, processed = 0;
	u32			staterr = 0;
	union ixgbe_adv_rx_desc	*cur;
	struct ixgbe_rx_buf	*rbuf, *nbuf;

	IXGBE_RX_LOCK(rxr);

	for (i = rxr->next_to_check; count != 0;) {
		struct mbuf	*sendmp, *mh, *mp;
		u32		rsc, ptype;
		u16		hlen, plen, hdr, vtag;
		bool		eop;
 
		/* Sync the ring. */
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->rx_base[i];
		staterr = le32toh(cur->wb.upper.status_error);

		if ((staterr & IXGBE_RXD_STAT_DD) == 0)
			break;
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		count--;
		sendmp = NULL;
		nbuf = NULL;
		rsc = 0;
		cur->wb.upper.status_error = 0;
		rbuf = &rxr->rx_buffers[i];
		mh = rbuf->m_head;
		mp = rbuf->m_pack;

		plen = le16toh(cur->wb.upper.length);
		ptype = le32toh(cur->wb.lower.lo_dword.data) &
		    IXGBE_RXDADV_PKTTYPE_MASK;
		hdr = le16toh(cur->wb.lower.lo_dword.hs_rss.hdr_info);
		vtag = le16toh(cur->wb.upper.vlan);
		eop = ((staterr & IXGBE_RXD_STAT_EOP) != 0);

		/* Make sure all parts of a bad packet are discarded */
		if (((staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) != 0) ||
		    (rxr->discard)) {
			ifp->if_ierrors++;
			rxr->rx_discarded++;
			if (!eop)
				rxr->discard = TRUE;
			else
				rxr->discard = FALSE;
			ixgbe_rx_discard(rxr, i);
			goto next_desc;
		}

		/*
		** On 82599 which supports a hardware
		** LRO (called HW RSC), packets need
		** not be fragmented across sequential
		** descriptors, rather the next descriptor
		** is indicated in bits of the descriptor.
		** This also means that we might proceses
		** more than one packet at a time, something
		** that has never been true before, it
		** required eliminating global chain pointers
		** in favor of what we are doing here.  -jfv
		*/
		if (!eop) {
			/*
			** Figure out the next descriptor
			** of this frame.
			*/
			if (rxr->hw_rsc == TRUE) {
				rsc = ixgbe_rsc_count(cur);
				rxr->rsc_num += (rsc - 1);
			}
			if (rsc) { /* Get hardware index */
				nextp = ((staterr &
				    IXGBE_RXDADV_NEXTP_MASK) >>
				    IXGBE_RXDADV_NEXTP_SHIFT);
			} else { /* Just sequential */
				nextp = i + 1;
				if (nextp == adapter->num_rx_desc)
					nextp = 0;
			}
			nbuf = &rxr->rx_buffers[nextp];
			prefetch(nbuf);
		}
		/*
		** The header mbuf is ONLY used when header 
		** split is enabled, otherwise we get normal 
		** behavior, ie, both header and payload
		** are DMA'd into the payload buffer.
		**
		** Rather than using the fmp/lmp global pointers
		** we now keep the head of a packet chain in the
		** buffer struct and pass this along from one
		** descriptor to the next, until we get EOP.
		*/
		if (rxr->hdr_split && (rbuf->fmp == NULL)) {
			/* This must be an initial descriptor */
			hlen = (hdr & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			    IXGBE_RXDADV_HDRBUFLEN_SHIFT;
			if (hlen > IXGBE_RX_HDR)
				hlen = IXGBE_RX_HDR;
			mh->m_len = hlen;
			mh->m_flags |= M_PKTHDR;
			mh->m_next = NULL;
			mh->m_pkthdr.len = mh->m_len;
			/* Null buf pointer so it is refreshed */
			rbuf->m_head = NULL;
			/*
			** Check the payload length, this
			** could be zero if its a small
			** packet.
			*/
			if (plen > 0) {
				mp->m_len = plen;
				mp->m_next = NULL;
				mp->m_flags &= ~M_PKTHDR;
				mh->m_next = mp;
				mh->m_pkthdr.len += mp->m_len;
				/* Null buf pointer so it is refreshed */
				rbuf->m_pack = NULL;
				rxr->rx_split_packets++;
			}
			/*
			** Now create the forward
			** chain so when complete 
			** we wont have to.
			*/
                        if (eop == 0) {
				/* stash the chain head */
                                nbuf->fmp = mh;
				/* Make forward chain */
                                if (plen)
                                        mp->m_next = nbuf->m_pack;
                                else
                                        mh->m_next = nbuf->m_pack;
                        } else {
				/* Singlet, prepare to send */
                                sendmp = mh;
                                if (staterr & IXGBE_RXD_STAT_VP) {
                                        sendmp->m_pkthdr.ether_vtag = vtag;
                                        sendmp->m_flags |= M_VLANTAG;
                                }
                        }
		} else {
			/*
			** Either no header split, or a
			** secondary piece of a fragmented
			** split packet.
			*/
			mp->m_len = plen;
			/*
			** See if there is a stored head
			** that determines what we are
			*/
			sendmp = rbuf->fmp;
			rbuf->m_pack = rbuf->fmp = NULL;

			if (sendmp != NULL) /* secondary frag */
				sendmp->m_pkthdr.len += mp->m_len;
			else {
				/* first desc of a non-ps chain */
				sendmp = mp;
				sendmp->m_flags |= M_PKTHDR;
				sendmp->m_pkthdr.len = mp->m_len;
				if (staterr & IXGBE_RXD_STAT_VP) {
					sendmp->m_pkthdr.ether_vtag = vtag;
					sendmp->m_flags |= M_VLANTAG;
				}
                        }
			/* Pass the head pointer on */
			if (eop == 0) {
				nbuf->fmp = sendmp;
				sendmp = NULL;
				mp->m_next = nbuf->m_pack;
			}
		}
		++processed;
		/* Sending this frame? */
		if (eop) {
			sendmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			rxr->rx_packets++;
			/* capture data for AIM */
			rxr->bytes += sendmp->m_pkthdr.len;
			rxr->rx_bytes += sendmp->m_pkthdr.len;
			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
				ixgbe_rx_checksum(staterr, sendmp, ptype);
#if __FreeBSD_version >= 800000
			sendmp->m_pkthdr.flowid = que->msix;
			sendmp->m_flags |= M_FLOWID;
#endif
		}
next_desc:
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;

		/* Now send to the stack or do LRO */
		if (sendmp != NULL)
			ixgbe_rx_input(rxr, ifp, sendmp, ptype);

               /* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
			ixgbe_refresh_mbufs(rxr, i);
			processed = 0;
		}
	}

	/* Refresh any remaining buf structs */
	if (processed != 0) {
		ixgbe_refresh_mbufs(rxr, i);
		processed = 0;
	}

	rxr->next_to_check = i;

	/*
	 * Flush any outstanding LRO work
	 */
	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}

	IXGBE_RX_UNLOCK(rxr);

	/*
	** We still have cleaning to do?
	** Schedule another interrupt if so.
	*/
	if ((staterr & IXGBE_RXD_STAT_DD) != 0) {
		ixgbe_rearm_queues(adapter, (u64)(1 << que->msix));
		return (TRUE);
	}

	return (FALSE);
}


/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixgbe_rx_checksum(u32 staterr, struct mbuf * mp, u32 ptype)
{
	u16	status = (u16) staterr;
	u8	errors = (u8) (staterr >> 24);
	bool	sctp = FALSE;

	if ((ptype & IXGBE_RXDADV_PKTTYPE_ETQF) == 0 &&
	    (ptype & IXGBE_RXDADV_PKTTYPE_SCTP) != 0)
		sctp = TRUE;

	if (status & IXGBE_RXD_STAT_IPCS) {
		if (!(errors & IXGBE_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else
			mp->m_pkthdr.csum_flags = 0;
	}
	if (status & IXGBE_RXD_STAT_L4CS) {
		u16 type = (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
#if __FreeBSD_version >= 800000
		if (sctp)
			type = CSUM_SCTP_VALID;
#endif
		if (!(errors & IXGBE_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |= type;
			if (!sctp)
				mp->m_pkthdr.csum_data = htons(0xffff);
		} 
	}
	return;
}


/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixgbe_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixgbe_shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	/* Re-init to load the changes */
	ixgbe_init(adapter);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixgbe_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixgbe_shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	ixgbe_init(adapter);
}

static void
ixgbe_setup_vlan_hw_support(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32		ctrl;


	/*
	** We get here thru init_locked, meaning
	** a soft reset, this has already cleared
	** the VFTA and other state, so if there
	** have been no vlan's registered do nothing.
	*/
	if (adapter->num_vlans == 0)
		return;

	/*
	** A soft reset zero's out the VFTA, so
	** we need to repopulate it now.
	*/
	for (int i = 0; i < IXGBE_VFTA_SIZE; i++)
		if (ixgbe_shadow_vfta[i] != 0)
			IXGBE_WRITE_REG(hw, IXGBE_VFTA(i),
			    ixgbe_shadow_vfta[i]);

	/* Enable the Filter Table */
	ctrl = IXGBE_READ_REG(hw, IXGBE_VLNCTRL);
	ctrl &= ~IXGBE_VLNCTRL_CFIEN;
	ctrl |= IXGBE_VLNCTRL_VFE;
	if (hw->mac.type == ixgbe_mac_82598EB)
		ctrl |= IXGBE_VLNCTRL_VME;
	IXGBE_WRITE_REG(hw, IXGBE_VLNCTRL, ctrl);

	/* On 82599 the VLAN enable is per/queue in RXDCTL */
	if (hw->mac.type == ixgbe_mac_82599EB)
		for (int i = 0; i < adapter->num_queues; i++) {
			ctrl = IXGBE_READ_REG(hw, IXGBE_RXDCTL(i));
				ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(hw, IXGBE_RXDCTL(i), ctrl);
		}
}

static void
ixgbe_enable_intr(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ix_queue *que = adapter->queues;
	u32 mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);


	/* Enable Fan Failure detection */
	if (hw->device_id == IXGBE_DEV_ID_82598AT)
		    mask |= IXGBE_EIMS_GPI_SDP1;

	/* 82599 specific interrupts */
	if (adapter->hw.mac.type == ixgbe_mac_82599EB) {
		    mask |= IXGBE_EIMS_ECC;
		    mask |= IXGBE_EIMS_GPI_SDP1;
		    mask |= IXGBE_EIMS_GPI_SDP2;
#ifdef IXGBE_FDIR
		    mask |= IXGBE_EIMS_FLOW_DIR;
#endif
	}

	IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);

	/* With RSS we use auto clear */
	if (adapter->msix_mem) {
		mask = IXGBE_EIMS_ENABLE_MASK;
		/* Dont autoclear Link */
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		IXGBE_WRITE_REG(hw, IXGBE_EIAC, mask);
	}

	/*
	** Now enable all queues, this is done seperately to
	** allow for handling the extended (beyond 32) MSIX
	** vectors that can be used by 82599
	*/
        for (int i = 0; i < adapter->num_queues; i++, que++)
                ixgbe_enable_queue(adapter, que->msix);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static void
ixgbe_disable_intr(struct adapter *adapter)
{
	if (adapter->msix_mem)
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, 0);
	if (adapter->hw.mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(&adapter->hw);
	return;
}

u16
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, u32 reg)
{
	u16 value;

	value = pci_read_config(((struct ixgbe_osdep *)hw->back)->dev,
	    reg, 2);

	return (value);
}

void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, u32 reg, u16 value)
{
	pci_write_config(((struct ixgbe_osdep *)hw->back)->dev,
	    reg, value, 2);

	return;
}

/*
** Setup the correct IVAR register for a particular MSIX interrupt
**   (yes this is all very magic and confusing :)
**  - entry is the register array entry
**  - vector is the MSIX vector for this queue
**  - type is RX/TX/MISC
*/
static void
ixgbe_set_ivar(struct adapter *adapter, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	switch (hw->mac.type) {

	case ixgbe_mac_82598EB:
		if (type == -1)
			entry = IXGBE_IVAR_OTHER_CAUSES_INDEX;
		else
			entry += (type * 64);
		index = (entry >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (entry & 0x3)));
		ivar |= (vector << (8 * (entry & 0x3)));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR(index), ivar);
		break;

	case ixgbe_mac_82599EB:
		if (type == -1) { /* MISC IVAR */
			index = (entry & 1) * 8;
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR_MISC, ivar);
		} else {	/* RX/TX IVARS */
			index = (16 * (entry & 1)) + (8 * type);
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(entry >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(entry >> 1), ivar);
		}

	default:
		break;
	}
}

static void
ixgbe_configure_ivars(struct adapter *adapter)
{
	struct  ix_queue *que = adapter->queues;

        for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* First the RX queue entry */
                ixgbe_set_ivar(adapter, i, que->msix, 0);
		/* ... and the TX */
		ixgbe_set_ivar(adapter, i, que->msix, 1);
		/* Set an Initial EITR value */
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_EITR(que->msix), IXGBE_LOW_LATENCY);
	}

	/* For the Link interrupt */
        ixgbe_set_ivar(adapter, 1, adapter->linkvec, -1);
}

/*
** ixgbe_sfp_probe - called in the local timer to
** determine if a port had optics inserted.
*/  
static bool ixgbe_sfp_probe(struct adapter *adapter)
{
	struct ixgbe_hw	*hw = &adapter->hw;
	device_t	dev = adapter->dev;
	bool		result = FALSE;

	if ((hw->phy.type == ixgbe_phy_nl) &&
	    (hw->phy.sfp_type == ixgbe_sfp_type_not_present)) {
		s32 ret = hw->phy.ops.identify_sfp(hw);
		if (ret)
                        goto out;
		ret = hw->phy.ops.reset(hw);
		if (ret == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			device_printf(dev,"Unsupported SFP+ module detected!");
			printf(" Reload driver with supported module.\n");
			adapter->sfp_probe = FALSE;
                        goto out;
		} else
			device_printf(dev,"SFP+ module detected!\n");
		/* We now have supported optics */
		adapter->sfp_probe = FALSE;
		result = TRUE;
	}
out:
	return (result);
}

/*
** Tasklet handler for MSIX Link interrupts
**  - do outside interrupt since it might sleep
*/
static void
ixgbe_handle_link(void *context, int pending)
{
	struct adapter  *adapter = context;

	ixgbe_check_link(&adapter->hw,
	    &adapter->link_speed, &adapter->link_up, 0);
       	ixgbe_update_link_status(adapter);
}

/*
** Tasklet for handling SFP module interrupts
*/
static void
ixgbe_handle_mod(void *context, int pending)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	device_t	dev = adapter->dev;
	u32 err;

	err = hw->phy.ops.identify_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Unsupported SFP+ module type was detected.\n");
		return;
	}
	err = hw->mac.ops.setup_sfp(hw);
	if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		device_printf(dev,
		    "Setup failure - unsupported SFP+ module type.\n");
		return;
	}
	taskqueue_enqueue(adapter->tq, &adapter->msf_task);
	return;
}


/*
** Tasklet for handling MSF (multispeed fiber) interrupts
*/
static void
ixgbe_handle_msf(void *context, int pending)
{
	struct adapter  *adapter = context;
	struct ixgbe_hw *hw = &adapter->hw;
	u32 autoneg;
	bool negotiate;

	autoneg = hw->phy.autoneg_advertised;
	if ((!autoneg) && (hw->mac.ops.get_link_capabilities))
		hw->mac.ops.get_link_capabilities(hw, &autoneg, &negotiate);
	if (hw->mac.ops.setup_link)
		hw->mac.ops.setup_link(hw, autoneg, negotiate, TRUE);
#if 0
	ixgbe_check_link(&adapter->hw, &speed, &adapter->link_up, 0);
       	ixgbe_update_link_status(adapter);
#endif
	return;
}

#ifdef IXGBE_FDIR
/*
** Tasklet for reinitializing the Flow Director filter table
*/
static void
ixgbe_reinit_fdir(void *context, int pending)
{
	struct adapter  *adapter = context;
	struct ifnet   *ifp = adapter->ifp;

	if (adapter->fdir_reinit != 1) /* Shouldn't happen */
		return;
	ixgbe_reinit_fdir_tables_82599(&adapter->hw);
	adapter->fdir_reinit = 0;
	/* Restart the interface */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	return;
}
#endif

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
ixgbe_update_stats_counters(struct adapter *adapter)
{
	struct ifnet   *ifp = adapter->ifp;
	struct ixgbe_hw *hw = &adapter->hw;
	u32  missed_rx = 0, bprc, lxon, lxoff, total;
	u64  total_missed_rx = 0;

	adapter->stats.crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);

	for (int i = 0; i < 8; i++) {
		u32 mp;
		mp = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		/* missed_rx tallies misses for the gprc workaround */
		missed_rx += mp;
		/* global total per queue */
        	adapter->stats.mpc[i] += mp;
		/* Running comprehensive total for stats display */
		total_missed_rx += adapter->stats.mpc[i];
		if (hw->mac.type == ixgbe_mac_82598EB)
			adapter->stats.rnbc[i] += IXGBE_READ_REG(hw, IXGBE_RNBC(i));
	}

	/* Hardware workaround, gprc counts missed packets */
	adapter->stats.gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	adapter->stats.gprc -= missed_rx;

	if (hw->mac.type == ixgbe_mac_82599EB) {
		adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCL);
		IXGBE_READ_REG(hw, IXGBE_GORCH); /* clears register */
		adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCL);
		IXGBE_READ_REG(hw, IXGBE_GOTCH); /* clears register */
		adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORL);
		IXGBE_READ_REG(hw, IXGBE_TORH); /* clears register */
		adapter->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXCNT);
		adapter->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXCNT);
	} else {
		adapter->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
		adapter->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
		/* 82598 only has a counter in the high register */
		adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
		adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
		adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORH);
	}

	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	adapter->stats.bprc += bprc;
	adapter->stats.mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	adapter->stats.mprc -= bprc;

	adapter->stats.roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	adapter->stats.prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	adapter->stats.prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	adapter->stats.prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	adapter->stats.prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	adapter->stats.prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	adapter->stats.prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);
	adapter->stats.rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);

	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	adapter->stats.lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	adapter->stats.lxofftxc += lxoff;
	total = lxon + lxoff;

	adapter->stats.gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	adapter->stats.mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	adapter->stats.ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	adapter->stats.gptc -= total;
	adapter->stats.mptc -= total;
	adapter->stats.ptc64 -= total;
	adapter->stats.gotc -= total * ETHER_MIN_LEN;

	adapter->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	adapter->stats.rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	adapter->stats.tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	adapter->stats.ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	adapter->stats.ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	adapter->stats.ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	adapter->stats.ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	adapter->stats.ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	adapter->stats.bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);


	/* Fill out the OS statistics structure */
	ifp->if_ipackets = adapter->stats.gprc;
	ifp->if_opackets = adapter->stats.gptc;
	ifp->if_ibytes = adapter->stats.gorc;
	ifp->if_obytes = adapter->stats.gotc;
	ifp->if_imcasts = adapter->stats.mprc;
	ifp->if_collisions = 0;

	/* Rx Errors */
	ifp->if_ierrors = total_missed_rx + adapter->stats.crcerrs +
		adapter->stats.rlec;
}


/**********************************************************************
 *
 *  This routine is called only when ixgbe_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixgbe_print_hw_stats(struct adapter * adapter)
{
	device_t dev = adapter->dev;


	device_printf(dev,"Std Mbuf Failed = %lu\n",
	       adapter->mbuf_defrag_failed);
	device_printf(dev,"Missed Packets = %llu\n",
	       (long long)adapter->stats.mpc[0]);
	device_printf(dev,"Receive length errors = %llu\n",
	       ((long long)adapter->stats.roc +
	       (long long)adapter->stats.ruc));
	device_printf(dev,"Crc errors = %llu\n",
	       (long long)adapter->stats.crcerrs);
	device_printf(dev,"Driver dropped packets = %lu\n",
	       adapter->dropped_pkts);
	device_printf(dev, "watchdog timeouts = %ld\n",
	       adapter->watchdog_events);

	device_printf(dev,"XON Rcvd = %llu\n",
	       (long long)adapter->stats.lxonrxc);
	device_printf(dev,"XON Xmtd = %llu\n",
	       (long long)adapter->stats.lxontxc);
	device_printf(dev,"XOFF Rcvd = %llu\n",
	       (long long)adapter->stats.lxoffrxc);
	device_printf(dev,"XOFF Xmtd = %llu\n",
	       (long long)adapter->stats.lxofftxc);

	device_printf(dev,"Total Packets Rcvd = %llu\n",
	       (long long)adapter->stats.tpr);
	device_printf(dev,"Good Packets Rcvd = %llu\n",
	       (long long)adapter->stats.gprc);
	device_printf(dev,"Good Packets Xmtd = %llu\n",
	       (long long)adapter->stats.gptc);
	device_printf(dev,"TSO Transmissions = %lu\n",
	       adapter->tso_tx);

	return;
}

/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixgbe_print_debug_info(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct ixgbe_hw		*hw = &adapter->hw;
	struct ix_queue		*que = adapter->queues;
	struct rx_ring		*rxr;
	struct tx_ring		*txr;
	struct lro_ctrl		*lro;
 
	device_printf(dev,"Error Byte Count = %u \n",
	    IXGBE_READ_REG(hw, IXGBE_ERRBC));

	for (int i = 0; i < adapter->num_queues; i++, que++) {
		txr = que->txr;
		rxr = que->rxr;
		lro = &rxr->lro;
		device_printf(dev,"QUE(%d) IRQs Handled: %lu\n",
		    que->msix, (long)que->irqs);
		device_printf(dev,"RX[%d]: rdh = %d, hw rdt = %d\n",
	    	    i, IXGBE_READ_REG(hw, IXGBE_RDH(i)),
	    	    IXGBE_READ_REG(hw, IXGBE_RDT(i)));
		device_printf(dev,"TX[%d] tdh = %d, hw tdt = %d\n", i,
		    IXGBE_READ_REG(hw, IXGBE_TDH(i)),
		    IXGBE_READ_REG(hw, IXGBE_TDT(i)));
		device_printf(dev,"RX(%d) Packets Received: %lld\n",
	    	    rxr->me, (long long)rxr->rx_packets);
		device_printf(dev,"RX(%d) Split RX Packets: %lld\n",
	    	    rxr->me, (long long)rxr->rx_split_packets);
		device_printf(dev,"RX(%d) Bytes Received: %lu\n",
	    	    rxr->me, (long)rxr->rx_bytes);
		device_printf(dev,"RX(%d) LRO Queued= %d\n",
		    rxr->me, lro->lro_queued);
		device_printf(dev,"RX(%d) LRO Flushed= %d\n",
		    rxr->me, lro->lro_flushed);
		device_printf(dev,"RX(%d) HW LRO Merges= %lu\n",
		    rxr->me, (long)rxr->rsc_num);
		device_printf(dev,"TX(%d) Packets Sent: %lu\n",
		    txr->me, (long)txr->total_packets);
		device_printf(dev,"TX(%d) NO Desc Avail: %lu\n",
		    txr->me, (long)txr->no_desc_avail);
	}

	device_printf(dev,"Link IRQ Handled: %lu\n",
    	    (long)adapter->link_irq);
	return;
}

static int
ixgbe_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	int             error;
	int             result;
	struct adapter *adapter;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		adapter = (struct adapter *) arg1;
		ixgbe_print_hw_stats(adapter);
	}
	return error;
}

static int
ixgbe_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	int error, result;
	struct adapter *adapter;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		adapter = (struct adapter *) arg1;
		ixgbe_print_debug_info(adapter);
	}
	return error;
}

/*
** Set flow control using sysctl:
** Flow control values:
** 	0 - off
**	1 - rx pause
**	2 - tx pause
**	3 - full
*/
static int
ixgbe_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	int error;
	struct adapter *adapter;

	error = sysctl_handle_int(oidp, &ixgbe_flow_control, 0, req);

	if (error)
		return (error);

	adapter = (struct adapter *) arg1;
	switch (ixgbe_flow_control) {
		case ixgbe_fc_rx_pause:
		case ixgbe_fc_tx_pause:
		case ixgbe_fc_full:
			adapter->hw.fc.requested_mode = ixgbe_flow_control;
			break;
		case ixgbe_fc_none:
		default:
			adapter->hw.fc.requested_mode = ixgbe_fc_none;
	}

	ixgbe_fc_enable(&adapter->hw, 0);
	return error;
}

static void
ixgbe_add_rx_process_limit(struct adapter *adapter, const char *name,
        const char *description, int *limit, int value)
{
        *limit = value;
        SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
            SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
            OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}

/*
** Control link advertise speed:
** 	0 - normal
**	1 - advertise only 1G
*/
static int
ixgbe_set_advertise(SYSCTL_HANDLER_ARGS)
{
	int			error;
	struct adapter		*adapter;
	struct ixgbe_hw		*hw;
	ixgbe_link_speed	speed, last;

	adapter = (struct adapter *) arg1;
	hw = &adapter->hw;
	last = hw->phy.autoneg_advertised;

	error = sysctl_handle_int(oidp, &adapter->advertise, 0, req);

	if ((error) || (adapter->advertise == -1))
		return (error);

	if (!((hw->phy.media_type == ixgbe_media_type_copper) ||
            (hw->phy.multispeed_fiber)))
		return (error);

	if (adapter->advertise == 1)
                speed = IXGBE_LINK_SPEED_1GB_FULL;
	else
                speed = IXGBE_LINK_SPEED_1GB_FULL |
			IXGBE_LINK_SPEED_10GB_FULL;

	if (speed == last) /* no change */
		return (error);

	hw->mac.autotry_restart = TRUE;
	hw->mac.ops.setup_link(hw, speed, TRUE, TRUE);

	return (error);
}
