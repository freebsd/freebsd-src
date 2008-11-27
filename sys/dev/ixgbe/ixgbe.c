/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
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
char ixgbe_driver_version[] = "1.6.2";

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
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_DA_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_CX4_DUAL_PORT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_XF_LR, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598AT, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598_SR_DUAL_PORT_EM, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82598EB_SFP_LOM, 0, 0, 0},
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
static int      ixgbe_ioctl(struct ifnet *, u_long, caddr_t);
static void     ixgbe_watchdog(struct adapter *);
static void     ixgbe_init(void *);
static void     ixgbe_init_locked(struct adapter *);
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
static int      ixgbe_hardware_init(struct adapter *);
static void     ixgbe_setup_interface(device_t, struct adapter *);

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

static void     ixgbe_enable_intr(struct adapter *);
static void     ixgbe_disable_intr(struct adapter *);
static void     ixgbe_update_stats_counters(struct adapter *);
static bool	ixgbe_txeof(struct tx_ring *);
static bool	ixgbe_rxeof(struct rx_ring *, int);
static void	ixgbe_rx_checksum(u32, struct mbuf *);
static void     ixgbe_set_promisc(struct adapter *);
static void     ixgbe_disable_promisc(struct adapter *);
static void     ixgbe_set_multi(struct adapter *);
static void     ixgbe_print_hw_stats(struct adapter *);
static void	ixgbe_print_debug_info(struct adapter *);
static void     ixgbe_update_link_status(struct adapter *);
static int	ixgbe_get_buf(struct rx_ring *, int, u8);
static int      ixgbe_xmit(struct tx_ring *, struct mbuf **);
static int      ixgbe_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	ixgbe_sysctl_debug(SYSCTL_HANDLER_ARGS);
static int	ixgbe_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	ixgbe_dma_malloc(struct adapter *, bus_size_t,
		    struct ixgbe_dma_alloc *, int);
static void     ixgbe_dma_free(struct adapter *, struct ixgbe_dma_alloc *);
static void	ixgbe_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static boolean_t ixgbe_tx_ctx_setup(struct tx_ring *, struct mbuf *);
static boolean_t ixgbe_tso_setup(struct tx_ring *, struct mbuf *, u32 *);
static void	ixgbe_set_ivar(struct adapter *, u16, u8, s8);
static void	ixgbe_configure_ivars(struct adapter *);
static u8 *	ixgbe_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

#ifdef IXGBE_HW_VLAN_SUPPORT
static void	ixgbe_register_vlan(void *, struct ifnet *, u16);
static void	ixgbe_unregister_vlan(void *, struct ifnet *, u16);
#endif

static void	ixgbe_update_aim(struct rx_ring *);

/* Support for pluggable optic modules */
static bool	ixgbe_sfp_probe(struct adapter *);

/* Legacy (single vector interrupt handler */
static void	ixgbe_legacy_irq(void *);

/* The MSI/X Interrupt handlers */
static void	ixgbe_msix_tx(void *);
static void	ixgbe_msix_rx(void *);
static void	ixgbe_msix_link(void *);

/* Legacy interrupts use deferred handlers */
static void	ixgbe_handle_tx(void *context, int pending);
static void	ixgbe_handle_rx(void *context, int pending);


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
** These  parameters are used in Adaptive 
** Interrupt Moderation. The value is set
** into EITR and controls the interrupt
** frequency. They can be modified but 
** be careful in tuning them.
*/
static int ixgbe_enable_aim = TRUE;
TUNABLE_INT("hw.ixgbe.enable_aim", &ixgbe_enable_aim);
static int ixgbe_low_latency = IXGBE_LOW_LATENCY;
TUNABLE_INT("hw.ixgbe.low_latency", &ixgbe_low_latency);
static int ixgbe_ave_latency = IXGBE_LOW_LATENCY;
TUNABLE_INT("hw.ixgbe.ave_latency", &ixgbe_low_latency);
static int ixgbe_bulk_latency = IXGBE_BULK_LATENCY;
TUNABLE_INT("hw.ixgbe.bulk_latency", &ixgbe_bulk_latency);

/* How many packets rxeof tries to clean at a time */
static int ixgbe_rx_process_limit = 100;
TUNABLE_INT("hw.ixgbe.rx_process_limit", &ixgbe_rx_process_limit);

/* Flow control setting, default to full */
static int ixgbe_flow_control = ixgbe_fc_none;
TUNABLE_INT("hw.ixgbe.flow_control", &ixgbe_flow_control);

/*
 * Should the driver do LRO on the RX end
 *  this can be toggled on the fly, but the
 *  interface must be reset (down/up) for it
 *  to take effect.  
 */
static int ixgbe_enable_lro = 1;
TUNABLE_INT("hw.ixgbe.enable_lro", &ixgbe_enable_lro);

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */
static int ixgbe_enable_msix = 1;
TUNABLE_INT("hw.ixgbe.enable_msix", &ixgbe_enable_msix);

/*
 * Enable RX Header Split
 */
static int ixgbe_rx_hdr_split = 1;
TUNABLE_INT("hw.ixgbe.rx_hdr_split", &ixgbe_rx_hdr_split);

/*
 * Number of TX/RX Queues, with 0 setting
 * it autoconfigures to the number of cpus.
 */
static int ixgbe_tx_queues = 1;
TUNABLE_INT("hw.ixgbe.tx_queues", &ixgbe_tx_queues);
static int ixgbe_rx_queues = 1;
TUNABLE_INT("hw.ixgbe.rx_queues", &ixgbe_rx_queues);

/* Number of TX descriptors per ring */
static int ixgbe_txd = DEFAULT_TXD;
TUNABLE_INT("hw.ixgbe.txd", &ixgbe_txd);

/* Number of RX descriptors per ring */
static int ixgbe_rxd = DEFAULT_RXD;
TUNABLE_INT("hw.ixgbe.rxd", &ixgbe_rxd);

/* Total number of Interfaces - need for config sanity check */
static int ixgbe_total_ports;

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
	int             error = 0;
	u16		pci_device_id;
	u32		ctrl_ext;

	INIT_DEBUGOUT("ixgbe_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;

	/* Core Lock Init*/
	IXGBE_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* Keep track of number of ports and optics */
	pci_device_id = pci_get_device(dev);
	switch (pci_device_id) {
		case IXGBE_DEV_ID_82598_CX4_DUAL_PORT :
			adapter->optics = IFM_10G_CX4;
			ixgbe_total_ports += 2;
			break;
		case IXGBE_DEV_ID_82598AF_DUAL_PORT :
			adapter->optics = IFM_10G_SR;
			ixgbe_total_ports += 2;
			break;
		case IXGBE_DEV_ID_82598AF_SINGLE_PORT :
			adapter->optics = IFM_10G_SR;
			ixgbe_total_ports += 1;
			break;
		case IXGBE_DEV_ID_82598EB_XF_LR :
			adapter->optics = IFM_10G_LR;
			ixgbe_total_ports += 1;
			break;
		case IXGBE_DEV_ID_82598EB_CX4 :
			adapter->optics = IFM_10G_CX4;
			ixgbe_total_ports += 1;
			break;
		case IXGBE_DEV_ID_82598AT :
			ixgbe_total_ports += 1;
		case IXGBE_DEV_ID_82598_DA_DUAL_PORT :
			ixgbe_total_ports += 2;
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

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "enable_lro", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_enable_lro, 1, "Large Receive Offload");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "enable_aim", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_enable_aim, 1, "Interrupt Moderation");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "low_latency", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_low_latency, 1, "Low Latency");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "ave_latency", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_ave_latency, 1, "Average Latency");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "bulk_latency", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_bulk_latency, 1, "Bulk Latency");

        SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "hdr_split", CTLTYPE_INT|CTLFLAG_RW,
			&ixgbe_rx_hdr_split, 1, "RX Header Split");

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
	if ((adapter->num_rx_queues > 1) && (nmbclusters > 0 )){
		int s;
		/* Calculate the total RX mbuf needs */
		s = (ixgbe_rxd * adapter->num_rx_queues) * ixgbe_total_ports;
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
	error = ixgbe_init_shared_code(&adapter->hw);
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

	/* Initialize the hardware */
	if (ixgbe_hardware_init(adapter)) {
		device_printf(dev,"Unable to initialize the hardware\n");
		error = EIO;
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

#ifdef IXGBE_HW_VLAN_SUPPORT
	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixgbe_register_vlan, 0, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixgbe_unregister_vlan, 0, EVENTHANDLER_PRI_FIRST);
#endif
                
	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

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
	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;
	u32	ctrl_ext;

	INIT_DEBUGOUT("ixgbe_detach: begin");

	/* Make sure VLANS are not using driver */
#if __FreeBSD_version >= 700000
	if (adapter->ifp->if_vlantrunk != NULL) {
#else
	if (adapter->ifp->if_nvlans != 0) {
#endif
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

	IXGBE_CORE_LOCK(adapter);
	ixgbe_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		if (txr->tq) {
			taskqueue_drain(txr->tq, &txr->tx_task);
			taskqueue_free(txr->tq);
		}
	}

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		if (rxr->tq) {
			taskqueue_drain(rxr->tq, &rxr->rx_task);
			taskqueue_free(rxr->tq);
		}
	}

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

#ifdef IXGBE_HW_VLAN_SUPPORT
	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);
#endif          

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

		/* Set timeout in case hardware has problems transmitting */
		txr->watchdog_timer = IXGBE_TX_TIMEOUT;

	}
	return;
}


static void
ixgbe_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	u32 queue = 0;

	/*
	** This is really just here for testing
	** TX multiqueue, ultimately what is
	** needed is the flow support in the stack
	** and appropriate logic here to deal with
	** it. -jfv
	*/
	if (adapter->num_tx_queues > 1)
		queue = (curcpu % adapter->num_tx_queues);

	txr = &adapter->tx_rings[queue];

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		IXGBE_TX_LOCK(txr);
		ixgbe_start_locked(txr, ifp);
		IXGBE_TX_UNLOCK(txr);
	}
	return;
}

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
	int             error = 0;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr   *ifa = (struct ifaddr *) data;
	struct adapter *adapter = ifp->if_softc;

	switch (command) {
	case SIOCSIFADDR:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFADDR (Get/Set Interface Addr)");
		if (ifa->ifa_addr->sa_family == AF_INET) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				IXGBE_CORE_LOCK(adapter);
				ixgbe_init_locked(adapter);
				IXGBE_CORE_UNLOCK(adapter);
			}
			arp_ifinit(ifp, ifa);
                } else
			ether_ioctl(ifp, command, data);
		break;
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
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING)
			ixgbe_init(adapter);
#if __FreeBSD_version >= 700000
		VLAN_CAPABILITIES(ifp);
#endif
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
 *  Watchdog entry point
 *
 *  This routine is called by the local timer
 *  to detect hardware hangs .
 *
 **********************************************************************/

static void
ixgbe_watchdog(struct adapter *adapter)
{
	device_t 	dev = adapter->dev;
	struct tx_ring *txr = adapter->tx_rings;
	struct ixgbe_hw *hw = &adapter->hw;
	bool		tx_hang = FALSE;

	IXGBE_CORE_LOCK_ASSERT(adapter);

        /*
         * The timer is set to 5 every time ixgbe_start() queues a packet.
         * Then ixgbe_txeof() keeps resetting to 5 as long as it cleans at
         * least one descriptor.
         * Finally, anytime all descriptors are clean the timer is
         * set to 0.
         */
	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		u32 head, tail;

		IXGBE_TX_LOCK(txr);
        	if (txr->watchdog_timer == 0 || --txr->watchdog_timer) {
			IXGBE_TX_UNLOCK(txr);
                	continue;
		} else {
			head = IXGBE_READ_REG(hw, IXGBE_TDH(i));
			tail = IXGBE_READ_REG(hw, IXGBE_TDT(i));
			if (head == tail) { /* last minute check */
				IXGBE_TX_UNLOCK(txr);
				continue;
			}
			/* Well, seems something is really hung */
			tx_hang = TRUE;
			IXGBE_TX_UNLOCK(txr);
			break;
		}
	}
	if (tx_hang == FALSE)
		return;

	/*
	 * If we are in this routine because of pause frames, then don't
	 * reset the hardware.
	 */
	if (IXGBE_READ_REG(hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF) {
		txr = adapter->tx_rings;	/* reset pointer */
		for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
			IXGBE_TX_LOCK(txr);
			txr->watchdog_timer = IXGBE_TX_TIMEOUT;
			IXGBE_TX_UNLOCK(txr);
		}
		return;
	}


	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", i,
		    IXGBE_READ_REG(hw, IXGBE_TDH(i)),
		    IXGBE_READ_REG(hw, IXGBE_TDT(i)));
		device_printf(dev,"TX(%d) desc avail = %d,"
		    "Next TX to Clean = %d\n",
		    i, txr->tx_avail, txr->next_tx_to_clean);
	}
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;

	ixgbe_init_locked(adapter);
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
	struct rx_ring *rxr = adapter->rx_rings;
	struct tx_ring *txr = adapter->tx_rings;
	struct ifnet   *ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	struct ixgbe_hw *hw;
	u32		k, txdctl, mhadd, gpie;
	u32		rxdctl, rxctrl;

	INIT_DEBUGOUT("ixgbe_init: begin");

	hw = &adapter->hw;
	mtx_assert(&adapter->core_mtx, MA_OWNED);

	ixgbe_stop(adapter);

	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(adapter->ifp), adapter->hw.mac.addr,
	      IXGBE_ETH_LENGTH_OF_ADDRESS);
	ixgbe_set_rar(&adapter->hw, 0, adapter->hw.mac.addr, 0, 1);
	adapter->hw.addr_ctrl.rar_used_count = 1;

	/* Initialize the hardware */
	if (ixgbe_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		return;
	}

#ifndef IXGBE_HW_VLAN_SUPPORT
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) {
		u32        ctrl;

		ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
		ctrl |= IXGBE_VLNCTRL_VME;
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
	}
#endif
	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev,"Could not setup transmit structures\n");
		ixgbe_stop(adapter);
		return;
	}

	ixgbe_initialize_transmit_units(adapter);

	/* TX irq moderation rate is fixed */
	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		IXGBE_WRITE_REG(&adapter->hw,
		    IXGBE_EITR(txr->msix), ixgbe_ave_latency);
		txr->watchdog_timer = FALSE;
	}

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

	/* RX moderation will be adapted over time, set default */
	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		IXGBE_WRITE_REG(&adapter->hw,
		    IXGBE_EITR(rxr->msix), ixgbe_low_latency);
	}

	/* Set Link moderation */
	IXGBE_WRITE_REG(&adapter->hw,
	    IXGBE_EITR(adapter->linkvec), IXGBE_LINK_ITR);

	gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);

	/* Enable Fan Failure Interrupt */
	if (adapter->hw.phy.media_type == ixgbe_media_type_copper)
		gpie |= IXGBE_SDP1_GPIEN;

	if (adapter->msix) {
		/* Enable Enhanced MSIX mode */
		gpie |= IXGBE_GPIE_MSIX_MODE;
		gpie |= IXGBE_GPIE_EIAME | IXGBE_GPIE_PBA_SUPPORT |
		    IXGBE_GPIE_OCD;
	}
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_GPIE, gpie);

	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;
	else if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist = (CSUM_TCP | CSUM_UDP);

	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		mhadd = IXGBE_READ_REG(&adapter->hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= adapter->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_MHADD, mhadd);
	}
	
	/* Now enable all the queues */

	for (int i = 0; i < adapter->num_tx_queues; i++) {
		txdctl = IXGBE_READ_REG(&adapter->hw, IXGBE_TXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		/* Set WTHRESH to 8, burst writeback */
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_TXDCTL(i), txdctl);
	}

	for (int i = 0; i < adapter->num_rx_queues; i++) {
		rxdctl = IXGBE_READ_REG(&adapter->hw, IXGBE_RXDCTL(i));
		/* PTHRESH set to 32 */
		rxdctl |= 0x0020;
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXDCTL(i), rxdctl);
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

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(hw, IXGBE_RXCTRL);
	if (adapter->hw.mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	IXGBE_WRITE_REG(hw, IXGBE_RXCTRL, rxctrl);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);

	/* Set up MSI/X routing */
	if (ixgbe_enable_msix)
		ixgbe_configure_ivars(adapter);

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
** MSIX Interrupt Handlers
*/

static void
ixgbe_handle_rx(void *context, int pending)
{
	struct rx_ring  *rxr = context;
	struct adapter  *adapter = rxr->adapter;
	u32		loop = MAX_LOOP;
	bool		more;

	do {
		more = ixgbe_rxeof(rxr, -1);
	} while (loop-- && more);
        /* Reenable this interrupt */
        IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, rxr->eims);
}

static void
ixgbe_handle_tx(void *context, int pending)
{
	struct tx_ring  *txr = context;
	struct adapter  *adapter = txr->adapter;
	struct ifnet    *ifp = adapter->ifp;
	u32		loop = MAX_LOOP;
	bool		more;

	IXGBE_TX_LOCK(txr);
	do {
		more = ixgbe_txeof(txr);
	} while (loop-- && more);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		ixgbe_start_locked(txr, ifp);

	IXGBE_TX_UNLOCK(txr);

	/* Reenable this interrupt */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, txr->eims);
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/

static void
ixgbe_legacy_irq(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct 		tx_ring *txr = adapter->tx_rings;
	struct		rx_ring *rxr = adapter->rx_rings;
	u32       	reg_eicr;


	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICR);

	if (reg_eicr == 0) {
		ixgbe_enable_intr(adapter);
		return;
	}

	if (ixgbe_rxeof(rxr, adapter->rx_process_limit))
		taskqueue_enqueue(rxr->tq, &rxr->rx_task);
	if (ixgbe_txeof(txr))
		taskqueue_enqueue(txr->tq, &txr->tx_task);

	/* Check for fan failure */
	if ((hw->phy.media_type == ixgbe_media_type_copper) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_GPI_SDP1);
	}

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
        	ixgbe_update_link_status(adapter);

	ixgbe_enable_intr(adapter);
	return;
}


/*********************************************************************
 *
 *  MSI TX Interrupt Service routine
 *
 **********************************************************************/

void
ixgbe_msix_tx(void *arg)
{
	struct tx_ring	*txr = arg;
	struct adapter  *adapter = txr->adapter;
	bool		more;

	IXGBE_TX_LOCK(txr);
	++txr->tx_irq;
	more = ixgbe_txeof(txr);
	IXGBE_TX_UNLOCK(txr);
	if (more)
		taskqueue_enqueue(txr->tq, &txr->tx_task);
	else /* Reenable this interrupt */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, txr->eims);
	return;
}


/*********************************************************************
 *
 *  MSIX RX Interrupt Service routine
 *
 **********************************************************************/

static void
ixgbe_msix_rx(void *arg)
{
	struct rx_ring	*rxr = arg;
	struct adapter  *adapter = rxr->adapter;
	bool		more;

	++rxr->rx_irq;
	more = ixgbe_rxeof(rxr, -1);
	if (more)
		taskqueue_enqueue(rxr->tq, &rxr->rx_task);
	else
        	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, rxr->eims);
	/* Update interrupt rate */
	if (ixgbe_enable_aim == TRUE)
		ixgbe_update_aim(rxr);
	return;
}

/*
** Routine to do adjust the RX EITR value based on traffic,
** its a simple three state model, but seems to help.
**
** Note that the three EITR values are tuneable using
** sysctl in real time. The feature can be effectively
** nullified by setting them equal.
*/
#define BULK_THRESHOLD	10000
#define AVE_THRESHOLD	1600

static void
ixgbe_update_aim(struct rx_ring *rxr)
{
	struct adapter  *adapter = rxr->adapter;
	u32             olditr, newitr;

	/* Update interrupt moderation based on traffic */
	olditr = rxr->eitr_setting;
	newitr = olditr;

	/* Idle, don't change setting */
	if (rxr->bytes == 0)   
		return;
                
	if (olditr == ixgbe_low_latency) {
		if (rxr->bytes > AVE_THRESHOLD)
			newitr = ixgbe_ave_latency;
	} else if (olditr == ixgbe_ave_latency) {
		if (rxr->bytes < AVE_THRESHOLD)
			newitr = ixgbe_low_latency;
		else if (rxr->bytes > BULK_THRESHOLD)
			newitr = ixgbe_bulk_latency;
	} else if (olditr == ixgbe_bulk_latency) {
		if (rxr->bytes < BULK_THRESHOLD)
			newitr = ixgbe_ave_latency;
	}

	if (olditr != newitr) {
		/* Change interrupt rate */
		rxr->eitr_setting = newitr;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(rxr->me),
		    newitr | (newitr << 16));
	}

	rxr->bytes = 0;
	return;
}


static void
ixgbe_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		reg_eicr;

	++adapter->link_irq;

	reg_eicr = IXGBE_READ_REG(hw, IXGBE_EICR);

	if (reg_eicr & IXGBE_EICR_LSC)
        	ixgbe_update_link_status(adapter);

	/* Check for fan failure */
	if ((hw->phy.media_type == ixgbe_media_type_copper) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                device_printf(adapter->dev, "\nCRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n");
		IXGBE_WRITE_REG(hw, IXGBE_EIMS, IXGBE_EICR_GPI_SDP1);
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
                adapter->hw.mac.autoneg = TRUE;
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
 *  This routine maps the mbufs to tx descriptors.
 *    WARNING: while this code is using an MQ style infrastructure,
 *    it would NOT work as is with more than 1 queue.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixgbe_xmit(struct tx_ring *txr, struct mbuf **m_headp)
{
	struct adapter  *adapter = txr->adapter;
	u32		olinfo_status = 0, cmd_type_len = 0;
	u32		paylen = 0;
	int             i, j, error, nsegs;
	int		first, last = 0;
	struct mbuf	*m_head;
	bus_dma_segment_t segs[IXGBE_MAX_SCATTER];
	bus_dmamap_t	map;
	struct ixgbe_tx_buf *txbuf, *txbuf_mapped;
	union ixgbe_adv_tx_desc *txd = NULL;

	m_head = *m_headp;

	/* Basic descriptor defines */
        cmd_type_len |= IXGBE_ADVTXD_DTYP_DATA;
        cmd_type_len |= IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT;

	if (m_head->m_flags & M_VLANTAG)
        	cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;

	/*
	 * Force a cleanup if number of TX descriptors
	 * available is below the threshold. If it fails
	 * to get above, then abort transmit.
	 */
	if (txr->tx_avail <= IXGBE_TX_CLEANUP_THRESHOLD) {
		ixgbe_txeof(txr);
		/* Make sure things have improved */
		if (txr->tx_avail <= IXGBE_TX_OP_THRESHOLD) {
			txr->no_tx_desc_avail++;
			return (ENOBUFS);
		}
	}

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail_tx_desc;
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
		txr->no_tx_desc_avail++;
		error = ENOBUFS;
		goto xmit_fail;
	}
	m_head = *m_headp;

	/*
	** Set the appropriate offload context
	** this becomes the first descriptor of 
	** a packet.
	*/
	if (ixgbe_tso_setup(txr, m_head, &paylen)) {
		cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
		olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
		olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
		olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
		++adapter->tso_tx;
	} else if (ixgbe_tx_ctx_setup(txr, m_head))
			olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;

        /* Record payload length */
	if (paylen == 0)
        	olinfo_status |= m_head->m_pkthdr.len <<
		    IXGBE_ADVTXD_PAYLEN_SHIFT;

	i = txr->next_avail_tx_desc;
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
		last = i; /* Next descriptor that will get completed */

		if (++i == adapter->num_tx_desc)
			i = 0;

		txbuf->m_head = NULL;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= nsegs;
	txr->next_avail_tx_desc = i;

	txbuf->m_head = m_head;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	++txr->total_packets;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(txr->me), i);
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

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		bcopy(LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
	IF_ADDR_UNLOCK(ifp);

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
 *  and runs the watchdog timer.
 *
 **********************************************************************/

static void
ixgbe_local_timer(void *arg)
{
	struct adapter *adapter = arg;
	struct ifnet   *ifp = adapter->ifp;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	/* Check for pluggable optics */
	if (adapter->sfp_probe)
		if (!ixgbe_sfp_probe(adapter))
			goto out; /* Nothing to do */

	ixgbe_update_link_status(adapter);
	ixgbe_update_stats_counters(adapter);
	if (ixgbe_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ixgbe_print_hw_stats(adapter);
	}
	/*
	 * Each tick we check the watchdog
	 * to protect against hardware hangs.
	 */
	ixgbe_watchdog(adapter);

out:
	/* Trigger an RX interrupt on all queues */
        IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, adapter->rx_mask);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
}

static void
ixgbe_update_link_status(struct adapter *adapter)
{
	boolean_t link_up = FALSE;
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring *txr = adapter->tx_rings;
	device_t dev = adapter->dev;

	ixgbe_check_link(&adapter->hw, &adapter->link_speed, &link_up, 0);

	if (link_up){ 
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
			for (int i = 0; i < adapter->num_tx_queues;
			    i++, txr++)
				txr->watchdog_timer = FALSE;
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
	struct 		tx_ring *txr = adapter->tx_rings;
	struct		rx_ring *rxr = adapter->rx_rings;
	int error;

	/* Legacy RID at 0 */
	if (adapter->msix == 0)
		adapter->rid[0] = 0;

	/* We allocate a single interrupt resource */
	adapter->res[0] = bus_alloc_resource_any(dev,
            SYS_RES_IRQ, &adapter->rid[0], RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res[0] == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
	TASK_INIT(&txr->tx_task, 0, ixgbe_handle_tx, txr);
	TASK_INIT(&rxr->rx_task, 0, ixgbe_handle_rx, rxr);
	txr->tq = taskqueue_create_fast("ixgbe_txq", M_NOWAIT,
            taskqueue_thread_enqueue, &txr->tq);
	rxr->tq = taskqueue_create_fast("ixgbe_rxq", M_NOWAIT,
            taskqueue_thread_enqueue, &rxr->tq);
	taskqueue_start_threads(&txr->tq, 1, PI_NET, "%s txq",
            device_get_nameunit(adapter->dev));
	taskqueue_start_threads(&rxr->tq, 1, PI_NET, "%s rxq",
            device_get_nameunit(adapter->dev));
	if ((error = bus_setup_intr(dev, adapter->res[0],
            INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_legacy_irq,
            adapter, &adapter->tag[0])) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
		    "handler: %d\n", error);
		taskqueue_free(txr->tq);
		taskqueue_free(rxr->tq);
		txr->tq = NULL;
		rxr->tq = NULL;
		return (error);
	}

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
	struct 		tx_ring *txr = adapter->tx_rings;
	struct		rx_ring *rxr = adapter->rx_rings;
	int 		error, vector = 0;

	/* TX setup: the code is here for multi tx,
	   there are other parts of the driver not ready for it */
	for (int i = 0; i < adapter->num_tx_queues; i++, vector++, txr++) {
		adapter->res[vector] = bus_alloc_resource_any(dev,
	    	    SYS_RES_IRQ, &adapter->rid[vector],
		    RF_SHAREABLE | RF_ACTIVE);
		if (!adapter->res[vector]) {
			device_printf(dev,"Unable to allocate"
		    	    " bus resource: tx interrupt [%d]\n", vector);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, adapter->res[vector],
		    INTR_TYPE_NET | INTR_MPSAFE, NULL,
		    ixgbe_msix_tx, txr, &adapter->tag[vector]);
		if (error) {
			adapter->res[vector] = NULL;
			device_printf(dev, "Failed to register TX handler");
			return (error);
		}
		txr->msix = vector;
		txr->eims = IXGBE_IVAR_TX_QUEUE(vector);
		TASK_INIT(&txr->tx_task, 0, ixgbe_handle_tx, txr);
		txr->tq = taskqueue_create_fast("ixgbe_txq", M_NOWAIT,
		    taskqueue_thread_enqueue, &txr->tq);
		taskqueue_start_threads(&txr->tq, 1, PI_NET, "%s txq",
		    device_get_nameunit(adapter->dev));
	}

	/* RX setup */
	for (int i = 0; i < adapter->num_rx_queues; i++, vector++, rxr++) {
		adapter->res[vector] = bus_alloc_resource_any(dev,
	    	    SYS_RES_IRQ, &adapter->rid[vector],
		    RF_SHAREABLE | RF_ACTIVE);
		if (!adapter->res[vector]) {
			device_printf(dev,"Unable to allocate"
		    	    " bus resource: rx interrupt [%d],"
			    "rid = %d\n", i, adapter->rid[vector]);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, adapter->res[vector],
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_msix_rx,
		    rxr, &adapter->tag[vector]);
		if (error) {
			adapter->res[vector] = NULL;
			device_printf(dev, "Failed to register RX handler");
			return (error);
		}
		rxr->msix = vector;
		rxr->eims = IXGBE_IVAR_RX_QUEUE(vector);
		/* used in local timer */
		adapter->rx_mask |= rxr->eims;
		TASK_INIT(&rxr->rx_task, 0, ixgbe_handle_rx, rxr);
		rxr->tq = taskqueue_create_fast("ixgbe_rxq", M_NOWAIT,
		    taskqueue_thread_enqueue, &rxr->tq);
		taskqueue_start_threads(&rxr->tq, 1, PI_NET, "%s rxq",
		    device_get_nameunit(adapter->dev));
	}

	/* Now for Link changes */
	adapter->res[vector] = bus_alloc_resource_any(dev,
    	    SYS_RES_IRQ, &adapter->rid[vector], RF_SHAREABLE | RF_ACTIVE);
	if (!adapter->res[vector]) {
		device_printf(dev,"Unable to allocate"
    	    " bus resource: Link interrupt [%d]\n", adapter->rid[vector]);
		return (ENXIO);
	}
	/* Set the link handler function */
	error = bus_setup_intr(dev, adapter->res[vector],
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, ixgbe_msix_link,
	    adapter, &adapter->tag[vector]);
	if (error) {
		adapter->res[vector] = NULL;
		device_printf(dev, "Failed to register LINK handler");
		return (error);
	}
	adapter->linkvec = vector;

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
	queues = (mp_ncpus > ((msgs-1)/2)) ? (msgs-1)/2 : mp_ncpus;

	if (ixgbe_tx_queues == 0)
		ixgbe_tx_queues = queues;
	if (ixgbe_rx_queues == 0)
		ixgbe_rx_queues = queues;
	want = ixgbe_tx_queues + ixgbe_rx_queues + 1;
	if (msgs >= want)
		msgs = want;
	else {
               	device_printf(adapter->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors but %d queues wanted!\n",
		    msgs, want);
		return (ENXIO);
	}
	if ((msgs) && pci_alloc_msix(dev, &msgs) == 0) {
               	device_printf(adapter->dev,
		    "Using MSIX interrupts with %d vectors\n", msgs);
		adapter->num_tx_queues = ixgbe_tx_queues;
		adapter->num_rx_queues = ixgbe_rx_queues;
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

	/*
	 * Init the resource arrays
	 */
	for (int i = 0; i < IXGBE_MSGS; i++) {
		adapter->rid[i] = i + 1; /* MSI/X RID starts at 1 */   
		adapter->tag[i] = NULL;
		adapter->res[i] = NULL;
	}

	/* Legacy defaults */
	adapter->num_tx_queues = 1;
	adapter->num_rx_queues = 1;

	/* Now setup MSI or MSI/X */
	adapter->msix = ixgbe_setup_msix(adapter);

	adapter->hw.back = &adapter->osdep;
	return (0);
}

static void
ixgbe_free_pci_resources(struct adapter * adapter)
{
	device_t	dev = adapter->dev;
	int		rid;

	/*
	 * Legacy has this set to 0, but we need
	 * to run this once, so reset it.
	 */
	if (adapter->msix == 0)
		adapter->msix = 1;

	rid = PCIR_BAR(MSIX_82598_BAR);

	/*
	 * First release all the interrupt resources:
	 * 	notice that since these are just kept
	 *	in an array we can do the same logic
	 * 	whether its MSIX or just legacy.
	 */
	for (int i = 0; i < adapter->msix; i++) {
		if (adapter->tag[i] != NULL) {
			bus_teardown_intr(dev, adapter->res[i],
			    adapter->tag[i]);
			adapter->tag[i] = NULL;
		}
		if (adapter->res[i] != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ,
			    adapter->rid[i], adapter->res[i]);
		}
	}

	if (adapter->msix)
		pci_release_msi(dev);

	if (adapter->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, adapter->msix_mem);

	if (adapter->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->pci_mem);

	return;
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  adapter structure. The controller is reset, the EEPROM is
 *  verified, the MAC address is set, then the shared initialization
 *  routines are called.
 *
 **********************************************************************/
static int
ixgbe_hardware_init(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	u16 csum;

	csum = 0;
	/* Issue a global reset */
	adapter->hw.adapter_stopped = FALSE;
	ixgbe_stop_adapter(&adapter->hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (ixgbe_validate_eeprom_checksum(&adapter->hw, &csum) < 0) {
		device_printf(dev,"The EEPROM Checksum Is Not Valid\n");
		return (EIO);
	}

	/* Get Hardware Flow Control setting */
	adapter->hw.fc.requested_mode = ixgbe_fc_full;
	adapter->hw.fc.pause_time = IXGBE_FC_PAUSE;
	adapter->hw.fc.low_water = IXGBE_FC_LO;
	adapter->hw.fc.high_water = IXGBE_FC_HI;
	adapter->hw.fc.send_xon = TRUE;

	if (ixgbe_init_hw(&adapter->hw)) {
		device_printf(dev,"Hardware Initialization Failed");
		return (EIO);
	}

	return (0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
ixgbe_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;
	struct ixgbe_hw *hw = &adapter->hw;
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
	ifp->if_timer = 0;
	ifp->if_watchdog = NULL;
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 1;

	ether_ifattach(ifp, adapter->hw.mac.addr);

	adapter->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= (IFCAP_HWCSUM | IFCAP_TSO4);
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;

	ifp->if_capenable = ifp->if_capabilities;

	if (hw->device_id == IXGBE_DEV_ID_82598AT)
		ixgbe_setup_link_speed(hw, (IXGBE_LINK_SPEED_10GB_FULL |
		    IXGBE_LINK_SPEED_1GB_FULL), TRUE, TRUE);
	else
		ixgbe_setup_link_speed(hw, IXGBE_LINK_SPEED_10GB_FULL,
		    TRUE, FALSE);

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

	r = bus_dma_tag_create(NULL,	/* parent */
			       PAGE_SIZE, 0,	/* alignment, bounds */
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
	device_t dev = adapter->dev;
	struct tx_ring *txr;
	struct rx_ring *rxr;
	int rsize, tsize, error = IXGBE_SUCCESS;
	int txconf = 0, rxconf = 0;

	/* First allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_tx_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto fail;
	}
	txr = adapter->tx_rings;

	/* Next allocate the RX */
	if (!(adapter->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    adapter->num_rx_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto rx_fail;
	}
	rxr = adapter->rx_rings;

	/* For the ring itself */
	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), 4096);

	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (int i = 0; i < adapter->num_tx_queues; i++, txconf++) {
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

	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), 4096);
	for (int i = 0; i < adapter->num_rx_queues; i++, rxconf++) {
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
			       PAGE_SIZE, 0,		/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IXGBE_TSO_SIZE,		/* maxsize */
			       IXGBE_MAX_SCATTER,	/* nsegments */
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
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * adapter->num_tx_desc);
	/* Reset indices */
	txr->next_avail_tx_desc = 0;
	txr->next_tx_to_clean = 0;

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
        }

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

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

	for (int i = 0; i < adapter->num_tx_queues; i++, txr++)
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

	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		u64	txhwb = 0, tdba = txr->txdma.dma_paddr;
		u32	txctrl;

		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(i),
		    adapter->num_tx_desc * sizeof(struct ixgbe_legacy_tx_desc));

		/* Setup for Head WriteBack */
		txhwb = (u64)vtophys(&txr->tx_hwb);
		txhwb |= IXGBE_TDWBAL_HEAD_WB_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_TDWBAL(i),
		    (txhwb & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDWBAH(i),
		    (txhwb >> 32));
		txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
		txctrl &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), txctrl);

		/* Setup the HW Tx Head and Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(i), 0);

		/* Setup Transmit Descriptor Cmd Settings */
		txr->txd_cmd = IXGBE_TXD_CMD_IFCS;

		txr->watchdog_timer = 0;
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

	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
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
	int ctxd = txr->next_avail_tx_desc;
#if __FreeBSD_version < 700000
	struct m_tag	*mtag;
#else
	u16 vtag = 0;
#endif


	if ((mp->m_pkthdr.csum_flags & CSUM_OFFLOAD) == 0)
		offload = FALSE;

	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the descriptor itself.
	*/
#if __FreeBSD_version < 700000
	mtag = VLAN_OUTPUT_TAG(ifp, mp);
	if (mtag != NULL) {
		vlan_macip_lens |=
		    htole16(VLAN_TAG_VALUE(mtag)) << IXGBE_ADVTXD_VLAN_SHIFT;
	} else if (offload == FALSE)
		return FALSE;	/* No need for CTX */
#else
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else if (offload == FALSE)
		return FALSE;
#endif
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
				return FALSE; /* failure */
			ipproto = ip->ip_p;
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
			break;
		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
			ip_hlen = sizeof(struct ip6_hdr);
			if (mp->m_len < ehdrlen + ip_hlen)
				return FALSE; /* failure */
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

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_tx_desc = ctxd;
	--txr->tx_avail;

        return (offload);
}

#if __FreeBSD_version >= 700000
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

	if (((mp->m_pkthdr.csum_flags & CSUM_TSO) == 0) ||
	    (mp->m_pkthdr.len <= IXGBE_TX_BUFFER_SIZE))
	        return FALSE;

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

	ctxd = txr->next_avail_tx_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	ip = (struct ip *)(mp->m_data + ehdrlen);
	if (ip->ip_p != IPPROTO_TCP)
		return FALSE;   /* 0 */
	ip->ip_len = 0;
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

	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_tx_desc = ctxd;
	return TRUE;
}

#else	/* For 6.2 RELEASE */
/* This makes it easy to keep the code common */
static boolean_t
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp, u32 *paylen)
{
	return (FALSE);
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
	struct adapter * adapter = txr->adapter;
	struct ifnet	*ifp = adapter->ifp;
	u32	first, last, done, num_avail;
	u32	cleaned = 0;
	struct ixgbe_tx_buf *tx_buffer;
	struct ixgbe_legacy_tx_desc *tx_desc;

	mtx_assert(&txr->mtx, MA_OWNED);

	if (txr->tx_avail == adapter->num_tx_desc)
		return FALSE;

	num_avail = txr->tx_avail;
	first = txr->next_tx_to_clean;

	tx_buffer = &txr->tx_buffers[first];
	/* For cleanup we just use legacy struct */
	tx_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];

	/* Get the HWB */
	rmb();
        done = txr->tx_hwb;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);

	while (TRUE) {
		/* We clean the range til last head write back */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			tx_desc->buffer_addr = 0;
			num_avail++; cleaned++;

			if (tx_buffer->m_head) {
				ifp->if_opackets++;
				bus_dmamap_sync(txr->txtag,
				    tx_buffer->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(txr->txtag,
				    tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
				tx_buffer->map = NULL;
			}

			if (++first == adapter->num_tx_desc)
				first = 0;

			tx_buffer = &txr->tx_buffers[first];
			tx_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
		}
		/* See if there is more work now */
		last = done;
		rmb();
        	done = txr->tx_hwb;
		if (last == done)
			break;
	}
	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	txr->next_tx_to_clean = first;

	/*
	 * If we have enough room, clear IFF_DRV_OACTIVE to tell the stack that
	 * it is OK to send packets. If there are no pending descriptors,
	 * clear the timeout. Otherwise, if some descriptors have been freed,
	 * restart the timeout.
	 */
	if (num_avail > IXGBE_TX_CLEANUP_THRESHOLD) {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		/* If all are clean turn off the timer */
		if (num_avail == adapter->num_tx_desc) {
			txr->watchdog_timer = 0;
			txr->tx_avail = num_avail;
			return FALSE;
		}
	}

	/* Some were cleaned, so reset timer */
	if (cleaned)
		txr->watchdog_timer = IXGBE_TX_TIMEOUT;
	txr->tx_avail = num_avail;
	return TRUE;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
ixgbe_get_buf(struct rx_ring *rxr, int i, u8 clean)
{
	struct adapter		*adapter = rxr->adapter;
	bus_dma_segment_t	seg[2];
	struct ixgbe_rx_buf	*rxbuf;
	struct mbuf		*mh, *mp;
	bus_dmamap_t		map;
	int			nsegs, error;
	int			merr = 0;


	rxbuf = &rxr->rx_buffers[i];

	/* First get our header and payload mbuf */
	if (clean & IXGBE_CLEAN_HDR) {
		mh = m_gethdr(M_DONTWAIT, MT_DATA);
		if (mh == NULL)
			goto remap;
	} else  /* reuse */
		mh = rxr->rx_buffers[i].m_head;

	mh->m_len = MHLEN;
	mh->m_flags |= M_PKTHDR;

	if (clean & IXGBE_CLEAN_PKT) {
		mp = m_getjcl(M_DONTWAIT, MT_DATA,
		    M_PKTHDR, adapter->rx_mbuf_sz);
		if (mp == NULL)
			goto remap;
		mp->m_len = adapter->rx_mbuf_sz;
		mp->m_flags &= ~M_PKTHDR;
	} else {	/* reusing */
		mp = rxr->rx_buffers[i].m_pack;
		mp->m_len = adapter->rx_mbuf_sz;
		mp->m_flags &= ~M_PKTHDR;
	}
	/*
	** Need to create a chain for the following
	** dmamap call at this point.
	*/
	mh->m_next = mp;
	mh->m_pkthdr.len = mh->m_len + mp->m_len;

	/* Get the memory mapping */
	error = bus_dmamap_load_mbuf_sg(rxr->rxtag,
	    rxr->spare_map, mh, seg, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("GET BUF: dmamap load failure - %d\n", error);
		m_free(mh);
		return (error);
	}

	/* Unload old mapping and update buffer struct */
	if (rxbuf->m_head != NULL)
		bus_dmamap_unload(rxr->rxtag, rxbuf->map);
	map = rxbuf->map;
	rxbuf->map = rxr->spare_map;
	rxr->spare_map = map;
	rxbuf->m_head = mh;
	rxbuf->m_pack = mp;
	bus_dmamap_sync(rxr->rxtag,
	    rxbuf->map, BUS_DMASYNC_PREREAD);

	/* Update descriptor */
	rxr->rx_base[i].read.hdr_addr = htole64(seg[0].ds_addr);
	rxr->rx_base[i].read.pkt_addr = htole64(seg[1].ds_addr);

	return (0);

	/*
	** If we get here, we have an mbuf resource
	** issue, so we discard the incoming packet
	** and attempt to reuse existing mbufs next
	** pass thru the ring, but to do so we must
	** fix up the descriptor which had the address
	** clobbered with writeback info.
	*/
remap:
	adapter->mbuf_header_failed++;
	merr = ENOBUFS;
	/* Is there a reusable buffer? */
	mh = rxr->rx_buffers[i].m_head;
	if (mh == NULL) /* Nope, init error */
		return (merr);
	mp = rxr->rx_buffers[i].m_pack;
	if (mp == NULL) /* Nope, init error */
		return (merr);
	/* Get our old mapping */
	rxbuf = &rxr->rx_buffers[i];
	error = bus_dmamap_load_mbuf_sg(rxr->rxtag,
	    rxbuf->map, mh, seg, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		/* We really have a problem */
		m_free(mh);
		return (error);
	}
	/* Now fix the descriptor as needed */
	rxr->rx_base[i].read.hdr_addr = htole64(seg[0].ds_addr);
	rxr->rx_base[i].read.pkt_addr = htole64(seg[1].ds_addr);

	return (merr);
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

	/*
	** The tag is made to accomodate the largest buffer size
	** with packet split (hence the two segments, even though
	** it may not always use this.
	*/
	if ((error = bus_dma_tag_create(NULL,		/* parent */
				   PAGE_SIZE, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MJUM16BYTES,		/* maxsize */
				   2,			/* nsegments */
				   MJUMPAGESIZE,	/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->rxtag))) {
		device_printf(dev, "Unable to create RX DMA tag\n");
		goto fail;
	}

	/* Create the spare map (used by getbuf) */
        error = bus_dmamap_create(rxr->rxtag, BUS_DMA_NOWAIT,
	     &rxr->spare_map);
	if (error) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
		    __func__, error);
		goto fail;
	}

	for (i = 0; i < adapter->num_rx_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->rxtag,
		    BUS_DMA_NOWAIT, &rxbuf->map);
		if (error) {
			device_printf(dev, "Unable to create RX DMA map\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	ixgbe_free_receive_structures(adapter);
	return (error);
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
	device_t		dev;
	struct ixgbe_rx_buf	*rxbuf;
	struct lro_ctrl		*lro = &rxr->lro;
	int			j, rsize;

	adapter = rxr->adapter;
	dev = adapter->dev;

	/* Clear the ring contents */
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);

	/*
	** Free current RX buffer structs and their mbufs
	*/
	for (int i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head != NULL) {
			bus_dmamap_sync(rxr->rxtag, rxbuf->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->rxtag, rxbuf->map);
			if (rxbuf->m_head) {
				rxbuf->m_head->m_next = rxbuf->m_pack;
				m_freem(rxbuf->m_head);
			}
			rxbuf->m_head = NULL;
			rxbuf->m_pack = NULL;
		}
	}

	/* Now refresh the mbufs */
	for (j = 0; j < adapter->num_rx_desc; j++) {
		if (ixgbe_get_buf(rxr, j, IXGBE_CLEAN_ALL) == ENOBUFS) {
			rxr->rx_buffers[j].m_head = NULL;
			rxr->rx_buffers[j].m_pack = NULL;
			rxr->rx_base[j].read.hdr_addr = 0;
			rxr->rx_base[j].read.pkt_addr = 0;
			goto fail;
		}
	}

	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->last_cleaned = 0;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/* Now set up the LRO interface */
	if (ixgbe_enable_lro) {
		int err = tcp_lro_init(lro);
		if (err) {
			INIT_DEBUGOUT("LRO Initialization failed!\n");
			goto fail;
		}
		INIT_DEBUGOUT("RX LRO Initialized\n");
		lro->ifp = adapter->ifp;
	}

	return (0);

fail:
	/*
	 * We need to clean up any buffers allocated
	 * so far, 'j' is the failing index.
	 */
	for (int i = 0; i < j; i++) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head != NULL) {
			bus_dmamap_sync(rxr->rxtag, rxbuf->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->rxtag, rxbuf->map);
			m_freem(rxbuf->m_head);
			rxbuf->m_head = NULL;
		}
	}
	return (ENOBUFS);
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

	for (j = 0; j < adapter->num_rx_queues; j++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'j' failed, so its the terminus.
	 */
	for (int i = 0; i < j; i++) {
		rxr = &adapter->rx_rings[i];
		for (int n = 0; n < adapter->num_rx_desc; n++) {
			struct ixgbe_rx_buf *rxbuf;
			rxbuf = &rxr->rx_buffers[n];
			if (rxbuf->m_head != NULL) {
				bus_dmamap_sync(rxr->rxtag, rxbuf->map,
			  	  BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxtag, rxbuf->map);
				m_freem(rxbuf->m_head);
				rxbuf->m_head = NULL;
			}
		}
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
	u32		rxctrl, fctrl, srrctl, rxcsum;
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

	srrctl = IXGBE_READ_REG(hw, IXGBE_SRRCTL(0));
	srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
	srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;

	hlreg = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	/* Set for Jumbo Frames? */
	if (ifp->if_mtu > ETHERMTU) {
		hlreg |= IXGBE_HLREG0_JUMBOEN;
		srrctl |= 4096 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	} else {
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
		srrctl |= 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	}
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, hlreg);

	if (ixgbe_rx_hdr_split) {
		/* Use a standard mbuf for the header */
		srrctl |= ((IXGBE_RX_HDR << IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT)
		    & IXGBE_SRRCTL_BSIZEHDR_MASK);
		srrctl |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
	} else
		srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;

	IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(0), srrctl);

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;
		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_RDBAL(i),
			       (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_RDBAH(i), (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_RDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(hw, IXGBE_RDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_RDT(i), 0);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	/* Setup RSS */
	if (adapter->num_rx_queues > 1) {
		int i, j;
		reta = 0;

		/* set up random bits */
		arc4rand(&random, sizeof(random), 0);

		/* Set up the redirection table */
		for (i = 0, j = 0; i < 128; i++, j++) {
			if (j == adapter->num_rx_queues) j = 0;
			reta = (reta << 8) | (j * 0x11);
			if ((i & 3) == 3)
				IXGBE_WRITE_REG(hw, IXGBE_RETA(i >> 2), reta);
		}

		/* Now fill our hash function seeds */
		for (int i = 0; i < 10; i++)
			IXGBE_WRITE_REG(hw, IXGBE_RSSRK(i), random[i]);

		/* Perform hash on these packet types */
		mrqc |= IXGBE_MRQC_RSSEN
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

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
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
void
ixgbe_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = NULL;
	struct ixgbe_rx_buf	*rxbuf = NULL;

	INIT_DEBUGOUT("free_receive_buffers: begin");
	adapter = rxr->adapter;
	if (rxr->rx_buffers != NULL) {
		rxbuf = &rxr->rx_buffers[0];
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			if (rxbuf->map != NULL) {
				bus_dmamap_sync(rxr->rxtag, rxbuf->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxtag, rxbuf->map);
				bus_dmamap_destroy(rxr->rxtag, rxbuf->map);
			}
			if (rxbuf->m_head != NULL) {
				m_freem(rxbuf->m_head);
			}
			rxbuf->m_head = NULL;
			++rxbuf;
		}
	}
	if (rxr->rx_buffers != NULL) {
		free(rxr->rx_buffers, M_DEVBUF);
		rxr->rx_buffers = NULL;
	}
	if (rxr->rxtag != NULL) {
		bus_dma_tag_destroy(rxr->rxtag);
		rxr->rxtag = NULL;
	}
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
ixgbe_rxeof(struct rx_ring *rxr, int count)
{
	struct adapter 		*adapter = rxr->adapter;
	struct ifnet   		*ifp = adapter->ifp;
	struct lro_ctrl		*lro = &rxr->lro;
	struct lro_entry	*queued;
	int             	i;
	u32			staterr;
	union ixgbe_adv_rx_desc	*cur;


	IXGBE_RX_LOCK(rxr);
	i = rxr->next_to_check;
	cur = &rxr->rx_base[i];
	staterr = cur->wb.upper.status_error;

	if (!(staterr & IXGBE_RXD_STAT_DD)) {
		IXGBE_RX_UNLOCK(rxr);
		return FALSE;
	}

	/* Sync the ring */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	while ((staterr & IXGBE_RXD_STAT_DD) && (count != 0) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		struct mbuf	*sendmp, *mh, *mp;
		u16		hlen, plen, hdr;	
		u8		dopayload, accept_frame, eop;


		accept_frame = 1;
		hlen = plen = 0;
		sendmp = mh = mp = NULL;

		/* Sync the buffers */
		bus_dmamap_sync(rxr->rxtag, rxr->rx_buffers[i].map,
			    BUS_DMASYNC_POSTREAD);

		/*
		** The way the hardware is configured to
		** split, it will ONLY use the header buffer
		** when header split is enabled, otherwise we
		** get normal behavior, ie, both header and
		** payload are DMA'd into the payload buffer.
		**
		** The fmp test is to catch the case where a
		** packet spans multiple descriptors, in that
		** case only the first header is valid.
		*/
		if ((ixgbe_rx_hdr_split) && (rxr->fmp == NULL)){
			hdr = le16toh(cur->
			    wb.lower.lo_dword.hs_rss.hdr_info);
			hlen = (hdr & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			    IXGBE_RXDADV_HDRBUFLEN_SHIFT;
			if (hlen > IXGBE_RX_HDR)
				hlen = IXGBE_RX_HDR;
			plen = le16toh(cur->wb.upper.length);
			/* Handle the header mbuf */
			mh = rxr->rx_buffers[i].m_head;
			mh->m_len = hlen;
			dopayload = IXGBE_CLEAN_HDR;
			/*
			** Get the payload length, this
			** could be zero if its a small
			** packet.
			*/
			if (plen) {
				mp = rxr->rx_buffers[i].m_pack;
				mp->m_len = plen;
				mp->m_next = NULL;
				mp->m_flags &= ~M_PKTHDR;
				mh->m_next = mp;
				mh->m_flags |= M_PKTHDR;
				dopayload = IXGBE_CLEAN_ALL;
				rxr->rx_split_packets++;
			} else {  /* small packets */
				mh->m_flags &= ~M_PKTHDR;
				mh->m_next = NULL;
			}
		} else {
			/*
			** Either no header split, or a
			** secondary piece of a fragmented
			** split packet.
			*/
			mh = rxr->rx_buffers[i].m_pack;
			mh->m_flags |= M_PKTHDR;
			mh->m_len = le16toh(cur->wb.upper.length);
			dopayload = IXGBE_CLEAN_PKT;
		}

		if (staterr & IXGBE_RXD_STAT_EOP) {
			count--;
			eop = 1;
		} else 
			eop = 0;

		if (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK)
			accept_frame = 0;

		if (accept_frame) {
			if (ixgbe_get_buf(rxr, i, dopayload) != 0) {
				ifp->if_iqdrops++;
				goto discard;
			}
			/* Initial frame - setup */
			if (rxr->fmp == NULL) {
				mh->m_flags |= M_PKTHDR;
				mh->m_pkthdr.len = mh->m_len;
				rxr->fmp = mh; /* Store the first mbuf */
				rxr->lmp = mh;
				if (mp) { /* Add payload if split */
					mh->m_pkthdr.len += mp->m_len;
					rxr->lmp = mh->m_next;
				}
			} else {
				/* Chain mbuf's together */
				mh->m_flags &= ~M_PKTHDR;
				rxr->lmp->m_next = mh;
				rxr->lmp = rxr->lmp->m_next;
				rxr->fmp->m_pkthdr.len += mh->m_len;
			}

			if (eop) {
				rxr->fmp->m_pkthdr.rcvif = ifp;
				ifp->if_ipackets++;
				rxr->rx_packets++;
				/* capture data for AIM */
				rxr->bytes += rxr->fmp->m_pkthdr.len;
				rxr->rx_bytes += rxr->bytes;
				if (ifp->if_capenable & IFCAP_RXCSUM)
					ixgbe_rx_checksum(staterr, rxr->fmp);
				else
					rxr->fmp->m_pkthdr.csum_flags = 0;
				if (staterr & IXGBE_RXD_STAT_VP) {
					rxr->fmp->m_pkthdr.ether_vtag =
					    le16toh(cur->wb.upper.vlan);
					rxr->fmp->m_flags |= M_VLANTAG;
				}
				sendmp = rxr->fmp;
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
		} else {
			ifp->if_ierrors++;
discard:
			/* Reuse loaded DMA map and just update mbuf chain */
			if (hlen) {
				mh = rxr->rx_buffers[i].m_head;
				mh->m_len = MHLEN;
				mh->m_next = NULL;
			}
			mp = rxr->rx_buffers[i].m_pack;
			mp->m_len = mp->m_pkthdr.len = adapter->rx_mbuf_sz;
			mp->m_data = mp->m_ext.ext_buf;
			mp->m_next = NULL;
			if (adapter->max_frame_size <=
			    (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
			if (rxr->fmp != NULL) {
				/* handles the whole chain */
				m_freem(rxr->fmp);
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
			sendmp = NULL;
		}
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		rxr->last_cleaned = i; /* for updating tail */

		if (++i == adapter->num_rx_desc)
			i = 0;

		/*
		** Now send up to the stack,
		** note the the value of next_to_check
		** is safe because we keep the RX lock
		** thru this call.
		*/
                if (sendmp != NULL) {
			/* Use LRO if possible */
			if ((!lro->lro_cnt) || (tcp_lro_rx(lro, sendmp, 0)))
	                        (*ifp->if_input)(ifp, sendmp);
                }

		/* Get next descriptor */
		cur = &rxr->rx_base[i];
		staterr = cur->wb.upper.status_error;
	}
	rxr->next_to_check = i;

	/* Advance the IXGB's Receive Queue "Tail Pointer" */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDT(rxr->me), rxr->last_cleaned);

	/*
	 * Flush any outstanding LRO work
	 */
	while (!SLIST_EMPTY(&lro->lro_active)) {
		queued = SLIST_FIRST(&lro->lro_active);
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}

	IXGBE_RX_UNLOCK(rxr);

	/*
	** Leaving with more to clean?
	** then schedule another interrupt.
	*/
	if (staterr & IXGBE_RXD_STAT_DD) {
        	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EICS, rxr->eims);
		return TRUE;
	}

	return FALSE;
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixgbe_rx_checksum(u32 staterr, struct mbuf * mp)
{
	u16 status = (u16) staterr;
	u8  errors = (u8) (staterr >> 24);

	if (status & IXGBE_RXD_STAT_IPCS) {
		/* Did it pass? */
		if (!(errors & IXGBE_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else
			mp->m_pkthdr.csum_flags = 0;
	}
	if (status & IXGBE_RXD_STAT_L4CS) {
		/* Did it pass? */
		if (!(errors & IXGBE_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |=
				(CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			mp->m_pkthdr.csum_data = htons(0xffff);
		} 
	}
	return;
}


#ifdef IXGBE_HW_VLAN_SUPPORT
/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
ixgbe_register_vlan(void *unused, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		ctrl, rctl, index, vfta;

	ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
	ctrl |= IXGBE_VLNCTRL_VME | IXGBE_VLNCTRL_VFE;
	ctrl &= ~IXGBE_VLNCTRL_CFIEN;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);

	/* Make entry in the hardware filter table */
	ixgbe_set_vfta(&adapter->hw, vtag, 0, TRUE);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
ixgbe_unregister_vlan(void *unused, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, vfta;

	/* Remove entry in the hardware filter table */
	ixgbe_set_vfta(&adapter->hw, vtag, 0, FALSE);

	/* Have all vlans unregistered? */
	if (adapter->ifp->if_vlantrunk == NULL) {
		u32 ctrl;
		/* Turn off the filter table */
		ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
		ctrl &= ~IXGBE_VLNCTRL_VME;
		ctrl &=  ~IXGBE_VLNCTRL_VFE;
		ctrl |= IXGBE_VLNCTRL_CFIEN;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
	}
}
#endif

static void
ixgbe_enable_intr(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 mask = IXGBE_EIMS_ENABLE_MASK;

	/* Enable Fan Failure detection */
	if (hw->phy.media_type == ixgbe_media_type_copper)
		    mask |= IXGBE_EIMS_GPI_SDP1;

	IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);

	/* With RSS we use auto clear */
	if (adapter->msix_mem) {
		/* Dont autoclear Link */
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		IXGBE_WRITE_REG(hw, IXGBE_EIAC, mask);
	}

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static void
ixgbe_disable_intr(struct adapter *adapter)
{
	if (adapter->msix_mem)
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, 0);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
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

/*
** Setup the correct IVAR register for a particular MSIX interrupt
**   (yes this is all very magic and confusing :)
**  - entry is the register array entry
**  - vector is the MSIX vector for this queue
**  - type is RX/TX/MISC
*/
static void
ixgbe_set_ivar(struct adapter *adapter, u16 entry, u8 vector, s8 type)
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

	default:
		break;
	}
}

static void
ixgbe_configure_ivars(struct adapter *adapter)
{
	struct  tx_ring *txr = adapter->tx_rings;
	struct  rx_ring *rxr = adapter->rx_rings;

        for (int i = 0; i < adapter->num_rx_queues; i++, rxr++)
                ixgbe_set_ivar(adapter, i, rxr->msix, 0);

        for (int i = 0; i < adapter->num_tx_queues; i++, txr++)
		ixgbe_set_ivar(adapter, i, txr->msix, 1);

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


/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
ixgbe_update_stats_counters(struct adapter *adapter)
{
	struct ifnet   *ifp = adapter->ifp;;
	struct ixgbe_hw *hw = &adapter->hw;
	u32  missed_rx = 0, bprc, lxon, lxoff, total;

	adapter->stats.crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);

	for (int i = 0; i < 8; i++) {
		int mp;
		mp = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		missed_rx += mp;
        	adapter->stats.mpc[i] += mp;
		adapter->stats.rnbc[i] += IXGBE_READ_REG(hw, IXGBE_RNBC(i));
	}

	/* Hardware workaround, gprc counts missed packets */
	adapter->stats.gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	adapter->stats.gprc -= missed_rx;

	adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
	adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
	adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORH);

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
	ifp->if_ierrors = missed_rx + adapter->stats.crcerrs +
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
	struct rx_ring *rxr = adapter->rx_rings;
	struct tx_ring *txr = adapter->tx_rings;
	struct ixgbe_hw *hw = &adapter->hw;
 
	device_printf(dev,"Error Byte Count = %u \n",
	    IXGBE_READ_REG(hw, IXGBE_ERRBC));

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		struct lro_ctrl		*lro = &rxr->lro;
		device_printf(dev,"Queue[%d]: rdh = %d, hw rdt = %d\n",
	    	    i, IXGBE_READ_REG(hw, IXGBE_RDH(i)),
	    	    IXGBE_READ_REG(hw, IXGBE_RDT(i)));
		device_printf(dev,"RX(%d) Packets Received: %lld\n",
	    	    rxr->me, (long long)rxr->rx_packets);
		device_printf(dev,"RX(%d) Split RX Packets: %lld\n",
	    	    rxr->me, (long long)rxr->rx_split_packets);
		device_printf(dev,"RX(%d) Bytes Received: %lu\n",
	    	    rxr->me, (long)rxr->rx_bytes);
		device_printf(dev,"RX(%d) IRQ Handled: %lu\n",
	    	    rxr->me, (long)rxr->rx_irq);
		device_printf(dev,"RX(%d) LRO Queued= %d\n",
		    rxr->me, lro->lro_queued);
		device_printf(dev,"RX(%d) LRO Flushed= %d\n",
		    rxr->me, lro->lro_flushed);
	}

	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", i,
		    IXGBE_READ_REG(hw, IXGBE_TDH(i)),
		    IXGBE_READ_REG(hw, IXGBE_TDT(i)));
		device_printf(dev,"TX(%d) Packets Sent: %lu\n",
		    txr->me, (long)txr->total_packets);
		device_printf(dev,"TX(%d) IRQ Handled: %lu\n",
		    txr->me, (long)txr->tx_irq);
		device_printf(dev,"TX(%d) NO Desc Avail: %lu\n",
		    txr->me, (long)txr->no_tx_desc_avail);
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

	ixgbe_setup_fc(&adapter->hw, 0);
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
