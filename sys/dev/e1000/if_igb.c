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
#include "opt_inet.h"
#include "opt_altq.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
#if __FreeBSD_version >= 800000
#include <sys/buf_ring.h>
#endif
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/eventhandler.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <machine/smp.h>
#include <machine/bus.h>
#include <machine/resource.h>

#include <net/bpf.h>
#include <net/ethernet.h>
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <net/if_types.h>
#include <net/if_vlan_var.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/tcp.h>
#include <netinet/tcp_lro.h>
#include <netinet/udp.h>

#include <machine/in_cksum.h>
#include <dev/led/led.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "e1000_api.h"
#include "e1000_82575.h"
#include "if_igb.h"

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int	igb_display_debug_stats = 0;

/*********************************************************************
 *  Driver version:
 *********************************************************************/
char igb_driver_version[] = "version - 2.0.7";


/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static igb_vendor_info_t igb_vendor_info_array[] =
{
	{ 0x8086, E1000_DEV_ID_82575EB_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82575EB_FIBER_SERDES,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82575GB_QUAD_COPPER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_NS,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_NS_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_SERDES_QUAD,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_QUAD_COPPER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_QUAD_COPPER_ET2,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82576_VF,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_SGMII,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_COPPER_DUAL,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82580_QUAD_FIBER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_DH89XXCC_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_DH89XXCC_SGMII,	PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{ 0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings for all supported NICs.
 *********************************************************************/

static char *igb_strings[] = {
	"Intel(R) PRO/1000 Network Connection"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int	igb_probe(device_t);
static int	igb_attach(device_t);
static int	igb_detach(device_t);
static int	igb_shutdown(device_t);
static int	igb_suspend(device_t);
static int	igb_resume(device_t);
static void	igb_start(struct ifnet *);
static void	igb_start_locked(struct tx_ring *, struct ifnet *ifp);
#if __FreeBSD_version >= 800000
static int	igb_mq_start(struct ifnet *, struct mbuf *);
static int	igb_mq_start_locked(struct ifnet *,
		    struct tx_ring *, struct mbuf *);
static void	igb_qflush(struct ifnet *);
#endif
static int	igb_ioctl(struct ifnet *, u_long, caddr_t);
static void	igb_init(void *);
static void	igb_init_locked(struct adapter *);
static void	igb_stop(void *);
static void	igb_media_status(struct ifnet *, struct ifmediareq *);
static int	igb_media_change(struct ifnet *);
static void	igb_identify_hardware(struct adapter *);
static int	igb_allocate_pci_resources(struct adapter *);
static int	igb_allocate_msix(struct adapter *);
static int	igb_allocate_legacy(struct adapter *);
static int	igb_setup_msix(struct adapter *);
static void	igb_free_pci_resources(struct adapter *);
static void	igb_local_timer(void *);
static void	igb_reset(struct adapter *);
static int	igb_setup_interface(device_t, struct adapter *);
static int	igb_allocate_queues(struct adapter *);
static void	igb_configure_queues(struct adapter *);

static int	igb_allocate_transmit_buffers(struct tx_ring *);
static void	igb_setup_transmit_structures(struct adapter *);
static void	igb_setup_transmit_ring(struct tx_ring *);
static void	igb_initialize_transmit_units(struct adapter *);
static void	igb_free_transmit_structures(struct adapter *);
static void	igb_free_transmit_buffers(struct tx_ring *);

static int	igb_allocate_receive_buffers(struct rx_ring *);
static int	igb_setup_receive_structures(struct adapter *);
static int	igb_setup_receive_ring(struct rx_ring *);
static void	igb_initialize_receive_units(struct adapter *);
static void	igb_free_receive_structures(struct adapter *);
static void	igb_free_receive_buffers(struct rx_ring *);
static void	igb_free_receive_ring(struct rx_ring *);

static void	igb_enable_intr(struct adapter *);
static void	igb_disable_intr(struct adapter *);
static void	igb_update_stats_counters(struct adapter *);
static bool	igb_txeof(struct tx_ring *);

static __inline	void igb_rx_discard(struct rx_ring *, int);
static __inline void igb_rx_input(struct rx_ring *,
		    struct ifnet *, struct mbuf *, u32);

static bool	igb_rxeof(struct igb_queue *, int, int *);
static void	igb_rx_checksum(u32, struct mbuf *, u32);
static int	igb_tx_ctx_setup(struct tx_ring *, struct mbuf *);
static bool	igb_tso_setup(struct tx_ring *, struct mbuf *, u32 *);
static void	igb_set_promisc(struct adapter *);
static void	igb_disable_promisc(struct adapter *);
static void	igb_set_multi(struct adapter *);
static void	igb_update_link_status(struct adapter *);
static void	igb_refresh_mbufs(struct rx_ring *, int);

static void	igb_register_vlan(void *, struct ifnet *, u16);
static void	igb_unregister_vlan(void *, struct ifnet *, u16);
static void	igb_setup_vlan_hw_support(struct adapter *);

static int	igb_xmit(struct tx_ring *, struct mbuf **);
static int	igb_dma_malloc(struct adapter *, bus_size_t,
		    struct igb_dma_alloc *, int);
static void	igb_dma_free(struct adapter *, struct igb_dma_alloc *);
static int	igb_sysctl_nvm_info(SYSCTL_HANDLER_ARGS);
static void	igb_print_nvm_info(struct adapter *);
static int 	igb_is_valid_ether_addr(u8 *);
static void     igb_add_hw_stats(struct adapter *);

static void	igb_vf_init_stats(struct adapter *);
static void	igb_update_vf_stats_counters(struct adapter *);

/* Management and WOL Support */
static void	igb_init_manageability(struct adapter *);
static void	igb_release_manageability(struct adapter *);
static void     igb_get_hw_control(struct adapter *);
static void     igb_release_hw_control(struct adapter *);
static void     igb_enable_wakeup(device_t);
static void     igb_led_func(void *, int);

static int	igb_irq_fast(void *);
static void	igb_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static void	igb_handle_que(void *context, int pending);
static void	igb_handle_link(void *context, int pending);

/* These are MSIX only irq handlers */
static void	igb_msix_que(void *);
static void	igb_msix_link(void *);

#ifdef DEVICE_POLLING
static poll_handler_t igb_poll;
#endif /* POLLING */

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t igb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, igb_probe),
	DEVMETHOD(device_attach, igb_attach),
	DEVMETHOD(device_detach, igb_detach),
	DEVMETHOD(device_shutdown, igb_shutdown),
	DEVMETHOD(device_suspend, igb_suspend),
	DEVMETHOD(device_resume, igb_resume),
	{0, 0}
};

static driver_t igb_driver = {
	"igb", igb_methods, sizeof(struct adapter),
};

static devclass_t igb_devclass;
DRIVER_MODULE(igb, pci, igb_driver, igb_devclass, 0, 0);
MODULE_DEPEND(igb, pci, 1, 1, 1);
MODULE_DEPEND(igb, ether, 1, 1, 1);

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

/* Descriptor defaults */
static int igb_rxd = IGB_DEFAULT_RXD;
static int igb_txd = IGB_DEFAULT_TXD;
TUNABLE_INT("hw.igb.rxd", &igb_rxd);
TUNABLE_INT("hw.igb.txd", &igb_txd);

/*
** AIM: Adaptive Interrupt Moderation
** which means that the interrupt rate
** is varied over time based on the
** traffic for that interrupt vector
*/
static int igb_enable_aim = TRUE;
TUNABLE_INT("hw.igb.enable_aim", &igb_enable_aim);

/*
 * MSIX should be the default for best performance,
 * but this allows it to be forced off for testing.
 */         
static int igb_enable_msix = 1;
TUNABLE_INT("hw.igb.enable_msix", &igb_enable_msix);

/*
** Tuneable Interrupt rate
*/
static int igb_max_interrupt_rate = 8000;
TUNABLE_INT("hw.igb.max_interrupt_rate", &igb_max_interrupt_rate);

/*
** Header split causes the packet header to
** be dma'd to a seperate mbuf from the payload.
** this can have memory alignment benefits. But
** another plus is that small packets often fit
** into the header and thus use no cluster. Its
** a very workload dependent type feature.
*/
static bool igb_header_split = FALSE;
TUNABLE_INT("hw.igb.hdr_split", &igb_header_split);

/*
** This will autoconfigure based on
** the number of CPUs if left at 0.
*/
static int igb_num_queues = 0;
TUNABLE_INT("hw.igb.num_queues", &igb_num_queues);

/* How many packets rxeof tries to clean at a time */
static int igb_rx_process_limit = 100;
TUNABLE_INT("hw.igb.rx_process_limit", &igb_rx_process_limit);

/* Flow control setting - default to FULL */
static int igb_fc_setting = e1000_fc_full;
TUNABLE_INT("hw.igb.fc_setting", &igb_fc_setting);

/*********************************************************************
 *  Device identification routine
 *
 *  igb_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
igb_probe(device_t dev)
{
	char		adapter_name[60];
	uint16_t	pci_vendor_id = 0;
	uint16_t	pci_device_id = 0;
	uint16_t	pci_subvendor_id = 0;
	uint16_t	pci_subdevice_id = 0;
	igb_vendor_info_t *ent;

	INIT_DEBUGOUT("igb_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != IGB_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = igb_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		    (ent->subvendor_id == PCI_ANY_ID)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		    (ent->subdevice_id == PCI_ANY_ID))) {
			sprintf(adapter_name, "%s %s",
				igb_strings[ent->index],
				igb_driver_version);
			device_set_desc_copy(dev, adapter_name);
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
igb_attach(device_t dev)
{
	struct adapter	*adapter;
	int		error = 0;
	u16		eeprom_data;

	INIT_DEBUGOUT("igb_attach: begin");

	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	IGB_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* SYSCTL stuff */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "nvm", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    igb_sysctl_nvm_info, "I", "NVM Information");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, "flow_control", CTLTYPE_INT|CTLFLAG_RW,
	    &igb_fc_setting, 0, "Flow Control");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "enable_aim", CTLTYPE_INT|CTLFLAG_RW,
	    &igb_enable_aim, 1, "Interrupt Moderation");

	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware and mac info */
	igb_identify_hardware(adapter);

	/* Setup PCI resources */
	if (igb_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}

	/* Do Shared Code initialization */
	if (e1000_setup_init_funcs(&adapter->hw, TRUE)) {
		device_printf(dev, "Setup of Shared code failed\n");
		error = ENXIO;
		goto err_pci;
	}

	e1000_get_bus_info(&adapter->hw);

	/* Sysctls for limiting the amount of work done in the taskqueue */
	igb_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    igb_rx_process_limit);

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of E1000_DBA_ALIGN.
	 */
	if (((igb_txd * sizeof(struct e1000_tx_desc)) % IGB_DBA_ALIGN) != 0 ||
	    (igb_txd > IGB_MAX_TXD) || (igb_txd < IGB_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    IGB_DEFAULT_TXD, igb_txd);
		adapter->num_tx_desc = IGB_DEFAULT_TXD;
	} else
		adapter->num_tx_desc = igb_txd;
	if (((igb_rxd * sizeof(struct e1000_rx_desc)) % IGB_DBA_ALIGN) != 0 ||
	    (igb_rxd > IGB_MAX_RXD) || (igb_rxd < IGB_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    IGB_DEFAULT_RXD, igb_rxd);
		adapter->num_rx_desc = IGB_DEFAULT_RXD;
	} else
		adapter->num_rx_desc = igb_rxd;

	adapter->hw.mac.autoneg = DO_AUTO_NEG;
	adapter->hw.phy.autoneg_wait_to_complete = FALSE;
	adapter->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;

	/* Copper options */
	if (adapter->hw.phy.media_type == e1000_media_type_copper) {
		adapter->hw.phy.mdix = AUTO_ALL_MODES;
		adapter->hw.phy.disable_polarity_correction = FALSE;
		adapter->hw.phy.ms_type = IGB_MASTER_SLAVE;
	}

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	adapter->max_frame_size = ETHERMTU + ETHER_HDR_LEN + ETHERNET_FCS_SIZE;
	adapter->min_frame_size = ETH_ZLEN + ETHERNET_FCS_SIZE;

	/*
	** Allocate and Setup Queues
	*/
	if (igb_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_pci;
	}

	/* Allocate the appropriate stats memory */
	if (adapter->hw.mac.type == e1000_vfadapt) {
		adapter->stats =
		    (struct e1000_vf_stats *)malloc(sizeof \
		    (struct e1000_vf_stats), M_DEVBUF, M_NOWAIT | M_ZERO);
		igb_vf_init_stats(adapter);
	} else
		adapter->stats =
		    (struct e1000_hw_stats *)malloc(sizeof \
		    (struct e1000_hw_stats), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (adapter->stats == NULL) {
		device_printf(dev, "Can not allocate stats memory\n");
		error = ENOMEM;
		goto err_late;
	}

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(u8) * ETH_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (adapter->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_late;
	}

	/*
	** Start from a known state, this is
	** important in reading the nvm and
	** mac from that.
	*/
	e1000_reset_hw(&adapter->hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (e1000_validate_nvm_checksum(&adapter->hw) < 0) {
		/*
		** Some PCI-E parts fail the first check due to
		** the link being in sleep state, call it again,
		** if it fails a second time its a real issue.
		*/
		if (e1000_validate_nvm_checksum(&adapter->hw) < 0) {
			device_printf(dev,
			    "The EEPROM Checksum Is Not Valid\n");
			error = EIO;
			goto err_late;
		}
	}

	/*
	** Copy the permanent MAC address out of the EEPROM
	*/
	if (e1000_read_mac_addr(&adapter->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_late;
	}
	/* Check its sanity */
	if (!igb_is_valid_ether_addr(adapter->hw.mac.addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_late;
	}

	/* 
	** Configure Interrupts
	*/
	if ((adapter->msix > 1) && (igb_enable_msix))
		error = igb_allocate_msix(adapter);
	else /* MSI or Legacy */
		error = igb_allocate_legacy(adapter);
	if (error)
		goto err_late;

	/* Setup OS specific network interface */
	if (igb_setup_interface(dev, adapter) != 0)
		goto err_late;

	/* Now get a good starting state */
	igb_reset(adapter);

	/* Initialize statistics */
	igb_update_stats_counters(adapter);

	adapter->hw.mac.get_link_status = 1;
	igb_update_link_status(adapter);

	/* Indicate SOL/IDER usage */
	if (e1000_check_reset_block(&adapter->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Determine if we have to control management hardware */
	adapter->has_manage = e1000_enable_mng_pass_thru(&adapter->hw);

	/*
	 * Setup Wake-on-Lan
	 */
	/* APME bit in EEPROM is mapped to WUC.APME */
	eeprom_data = E1000_READ_REG(&adapter->hw, E1000_WUC) & E1000_WUC_APME;
	if (eeprom_data)
		adapter->wol = E1000_WUFC_MAG;

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	     igb_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	     igb_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST);

	igb_add_hw_stats(adapter);

	/* Tell the stack that the interface is not active */
	adapter->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	adapter->led_dev = led_create(igb_led_func, adapter,
	    device_get_nameunit(dev));

	INIT_DEBUGOUT("igb_attach: end");

	return (0);

err_late:
	igb_free_transmit_structures(adapter);
	igb_free_receive_structures(adapter);
	igb_release_hw_control(adapter);
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
err_pci:
	igb_free_pci_resources(adapter);
	free(adapter->mta, M_DEVBUF);
	IGB_CORE_LOCK_DESTROY(adapter);

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
igb_detach(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	struct ifnet	*ifp = adapter->ifp;

	INIT_DEBUGOUT("igb_detach: begin");

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

	if (adapter->led_dev != NULL)
		led_destroy(adapter->led_dev);

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	IGB_CORE_LOCK(adapter);
	adapter->in_detach = 1;
	igb_stop(adapter);
	IGB_CORE_UNLOCK(adapter);

	e1000_phy_hw_reset(&adapter->hw);

	/* Give control back to firmware */
	igb_release_manageability(adapter);
	igb_release_hw_control(adapter);

	if (adapter->wol) {
		E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
		E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
		igb_enable_wakeup(dev);
	}

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);

	ether_ifdetach(adapter->ifp);

	callout_drain(&adapter->timer);

	igb_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(ifp);

	igb_free_transmit_structures(adapter);
	igb_free_receive_structures(adapter);
	free(adapter->mta, M_DEVBUF);

	IGB_CORE_LOCK_DESTROY(adapter);

	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
igb_shutdown(device_t dev)
{
	return igb_suspend(dev);
}

/*
 * Suspend/resume device methods.
 */
static int
igb_suspend(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);

	IGB_CORE_LOCK(adapter);

	igb_stop(adapter);

        igb_release_manageability(adapter);
	igb_release_hw_control(adapter);

        if (adapter->wol) {
                E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
                E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
                igb_enable_wakeup(dev);
        }

	IGB_CORE_UNLOCK(adapter);

	return bus_generic_suspend(dev);
}

static int
igb_resume(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ifnet *ifp = adapter->ifp;

	IGB_CORE_LOCK(adapter);
	igb_init_locked(adapter);
	igb_init_manageability(adapter);

	if ((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING))
		igb_start(ifp);

	IGB_CORE_UNLOCK(adapter);

	return bus_generic_resume(dev);
}


/*********************************************************************
 *  Transmit entry point
 *
 *  igb_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

static void
igb_start_locked(struct tx_ring *txr, struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct mbuf	*m_head;

	IGB_TX_LOCK_ASSERT(txr);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	/* Call cleanup if number of TX descriptors low */
	if (txr->tx_avail <= IGB_TX_CLEANUP_THRESHOLD)
		igb_txeof(txr);

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		if (txr->tx_avail <= IGB_TX_OP_THRESHOLD) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 *  Encapsulation can modify our pointer, and or make it
		 *  NULL on failure.  In that event, we can't requeue.
		 */
		if (igb_xmit(txr, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set watchdog on */
		txr->watchdog_time = ticks;
		txr->queue_status = IGB_QUEUE_WORKING;
	}
}
 
/*
 * Legacy TX driver routine, called from the
 * stack, always uses tx[0], and spins for it.
 * Should not be used with multiqueue tx
 */
static void
igb_start(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		IGB_TX_LOCK(txr);
		igb_start_locked(txr, ifp);
		IGB_TX_UNLOCK(txr);
	}
	return;
}

#if __FreeBSD_version >= 800000
/*
** Multiqueue Transmit driver
**
*/
static int
igb_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter		*adapter = ifp->if_softc;
	struct igb_queue	*que;
	struct tx_ring		*txr;
	int 			i = 0, err = 0;

	/* Which queue to use */
	if ((m->m_flags & M_FLOWID) != 0)
		i = m->m_pkthdr.flowid % adapter->num_queues;

	txr = &adapter->tx_rings[i];
	que = &adapter->queues[i];

	if (IGB_TX_TRYLOCK(txr)) {
		err = igb_mq_start_locked(ifp, txr, m);
		IGB_TX_UNLOCK(txr);
	} else {
		err = drbr_enqueue(ifp, txr->br, m);
		taskqueue_enqueue(que->tq, &que->que_task);
	}

	return (err);
}

static int
igb_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr, struct mbuf *m)
{
	struct adapter  *adapter = txr->adapter;
        struct mbuf     *next;
        int             err = 0, enq;

	IGB_TX_LOCK_ASSERT(txr);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || adapter->link_active == 0) {
		if (m != NULL)
			err = drbr_enqueue(ifp, txr->br, m);
		return (err);
	}

	/* Call cleanup if number of TX descriptors low */
	if (txr->tx_avail <= IGB_TX_CLEANUP_THRESHOLD)
		igb_txeof(txr);

	enq = 0;
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
		if ((err = igb_xmit(txr, &next)) != 0) {
			if (next != NULL)
				err = drbr_enqueue(ifp, txr->br, next);
			break;
		}
		enq++;
		drbr_stats_update(ifp, next->m_pkthdr.len, next->m_flags);
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
		if (txr->tx_avail <= IGB_TX_OP_THRESHOLD) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		next = drbr_dequeue(ifp, txr->br);
	}
	if (enq > 0) {
		/* Set the watchdog */
		txr->queue_status = IGB_QUEUE_WORKING;
		txr->watchdog_time = ticks;
	}
	return (err);
}

/*
** Flush all ring buffers
*/
static void
igb_qflush(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	struct mbuf	*m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IGB_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		IGB_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}
#endif /* __FreeBSD_version >= 800000 */

/*********************************************************************
 *  Ioctl entry point
 *
 *  igb_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
igb_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
#ifdef INET
	struct ifaddr *ifa = (struct ifaddr *)data;
#endif
	int error = 0;

	if (adapter->in_detach)
		return (error);

	switch (command) {
	case SIOCSIFADDR:
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET) {
			/*
			 * XXX
			 * Since resetting hardware takes a very long time
			 * and results in link renegotiation we only
			 * initialize the hardware only when it is absolutely
			 * required.
			 */
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				IGB_CORE_LOCK(adapter);
				igb_init_locked(adapter);
				IGB_CORE_UNLOCK(adapter);
			}
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
		} else
#endif
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		IGB_CORE_LOCK(adapter);
		max_frame_size = 9234;
		if (ifr->ifr_mtu > max_frame_size - ETHER_HDR_LEN -
		    ETHER_CRC_LEN) {
			IGB_CORE_UNLOCK(adapter);
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		adapter->max_frame_size =
		    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
		igb_init_locked(adapter);
		IGB_CORE_UNLOCK(adapter);
		break;
	    }
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd:\
		    SIOCSIFFLAGS (Set Interface Flags)");
		IGB_CORE_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ adapter->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					igb_disable_promisc(adapter);
					igb_set_promisc(adapter);
				}
			} else
				igb_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				igb_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		IGB_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IGB_CORE_LOCK(adapter);
			igb_disable_intr(adapter);
			igb_set_multi(adapter);
#ifdef DEVICE_POLLING
			if (!(ifp->if_capenable & IFCAP_POLLING))
#endif
				igb_enable_intr(adapter);
			IGB_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
		/*
		** As the speed/duplex settings are being
		** changed, we need toreset the PHY.
		*/
		adapter->hw.phy.reset_disable = FALSE;
		/* Check SOL/IDER usage */
		IGB_CORE_LOCK(adapter);
		if (e1000_check_reset_block(&adapter->hw)) {
			IGB_CORE_UNLOCK(adapter);
			device_printf(adapter->dev, "Media change is"
			    " blocked due to SOL/IDER session.\n");
			break;
		}
		IGB_CORE_UNLOCK(adapter);
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: \
		    SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
		break;
	case SIOCSIFCAP:
	    {
		int mask, reinit;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFCAP (Set Capabilities)");
		reinit = 0;
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(igb_poll, ifp);
				if (error)
					return (error);
				IGB_CORE_LOCK(adapter);
				igb_disable_intr(adapter);
				ifp->if_capenable |= IFCAP_POLLING;
				IGB_CORE_UNLOCK(adapter);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				IGB_CORE_LOCK(adapter);
				igb_enable_intr(adapter);
				ifp->if_capenable &= ~IFCAP_POLLING;
				IGB_CORE_UNLOCK(adapter);
			}
		}
#endif
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			reinit = 1;
		}
		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			reinit = 1;
		}
		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			reinit = 1;
		}
		if (mask & IFCAP_VLAN_HWFILTER) {
			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
			reinit = 1;
		}
		if (mask & IFCAP_LRO) {
			ifp->if_capenable ^= IFCAP_LRO;
			reinit = 1;
		}
		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING))
			igb_init(adapter);
		VLAN_CAPABILITIES(ifp);
		break;
	    }

	default:
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
igb_init_locked(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t	dev = adapter->dev;

	INIT_DEBUGOUT("igb_init: begin");

	IGB_CORE_LOCK_ASSERT(adapter);

	igb_disable_intr(adapter);
	callout_stop(&adapter->timer);

	/* Get the latest mac address, User can use a LAA */
        bcopy(IF_LLADDR(adapter->ifp), adapter->hw.mac.addr,
              ETHER_ADDR_LEN);

	/* Put the address into the Receive Address Array */
	e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	igb_reset(adapter);
	igb_update_link_status(adapter);

	E1000_WRITE_REG(&adapter->hw, E1000_VET, ETHERTYPE_VLAN);

	/* Set hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
#if __FreeBSD_version >= 800000
		if (adapter->hw.mac.type == e1000_82576)
			ifp->if_hwassist |= CSUM_SCTP;
#endif
	}

	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;

	/* Configure for OS presence */
	igb_init_manageability(adapter);

	/* Prepare transmit descriptors and buffers */
	igb_setup_transmit_structures(adapter);
	igb_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	igb_set_multi(adapter);

	/*
	** Figure out the desired mbuf pool
	** for doing jumbo/packetsplit
	*/
	if (adapter->max_frame_size <= 2048)
		adapter->rx_mbuf_sz = MCLBYTES;
	else if (adapter->max_frame_size <= 4096)
		adapter->rx_mbuf_sz = MJUMPAGESIZE;
	else
		adapter->rx_mbuf_sz = MJUM9BYTES;

	/* Prepare receive descriptors and buffers */
	if (igb_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		return;
	}
	igb_initialize_receive_units(adapter);

        /* Use real VLAN Filter support? */
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) {
		if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
			/* Use real VLAN Filter support */
			igb_setup_vlan_hw_support(adapter);
		else {
			u32 ctrl;
			ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
			ctrl |= E1000_CTRL_VME;
			E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
		}
	}
                                
	/* Don't lose promiscuous settings */
	igb_set_promisc(adapter);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&adapter->timer, hz, igb_local_timer, adapter);
	e1000_clear_hw_cntrs_base_generic(&adapter->hw);

	if (adapter->msix > 1) /* Set up queue routing */
		igb_configure_queues(adapter);

	/* this clears any pending interrupts */
	E1000_READ_REG(&adapter->hw, E1000_ICR);
#ifdef DEVICE_POLLING
	/*
	 * Only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		igb_disable_intr(adapter);
	else
#endif /* DEVICE_POLLING */
	{
	igb_enable_intr(adapter);
	E1000_WRITE_REG(&adapter->hw, E1000_ICS, E1000_ICS_LSC);
	}

	/* Don't reset the phy next time init gets called */
	adapter->hw.phy.reset_disable = TRUE;
}

static void
igb_init(void *arg)
{
	struct adapter *adapter = arg;

	IGB_CORE_LOCK(adapter);
	igb_init_locked(adapter);
	IGB_CORE_UNLOCK(adapter);
}


static void
igb_handle_que(void *context, int pending)
{
	struct igb_queue *que = context;
	struct adapter *adapter = que->adapter;
	struct tx_ring *txr = que->txr;
	struct ifnet	*ifp = adapter->ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		bool	more;

		more = igb_rxeof(que, -1, NULL);

		IGB_TX_LOCK(txr);
		if (igb_txeof(txr))
			more = TRUE;
#if __FreeBSD_version >= 800000
		if (!drbr_empty(ifp, txr->br))
			igb_mq_start_locked(ifp, txr, NULL);
#else
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			igb_start_locked(txr, ifp);
#endif
		IGB_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->que_task);
			return;
		}
	}

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		return;
#endif
	/* Reenable this interrupt */
	if (que->eims)
		E1000_WRITE_REG(&adapter->hw, E1000_EIMS, que->eims);
	else
		igb_enable_intr(adapter);
}

/* Deal with link in a sleepable context */
static void
igb_handle_link(void *context, int pending)
{
	struct adapter *adapter = context;

	adapter->hw.mac.get_link_status = 1;
	igb_update_link_status(adapter);
}

/*********************************************************************
 *
 *  MSI/Legacy Deferred
 *  Interrupt Service routine  
 *
 *********************************************************************/
static int
igb_irq_fast(void *arg)
{
	struct adapter		*adapter = arg;
	struct igb_queue	*que = adapter->queues;
	u32			reg_icr;


	reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);

	/* Hot eject?  */
	if (reg_icr == 0xffffffff)
		return FILTER_STRAY;

	/* Definitely not our interrupt.  */
	if (reg_icr == 0x0)
		return FILTER_STRAY;

	if ((reg_icr & E1000_ICR_INT_ASSERTED) == 0)
		return FILTER_STRAY;

	/*
	 * Mask interrupts until the taskqueue is finished running.  This is
	 * cheap, just assume that it is needed.  This also works around the
	 * MSI message reordering errata on certain systems.
	 */
	igb_disable_intr(adapter);
	taskqueue_enqueue(que->tq, &que->que_task);

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC))
		taskqueue_enqueue(que->tq, &adapter->link_task);

	if (reg_icr & E1000_ICR_RXO)
		adapter->rx_overruns++;
	return FILTER_HANDLED;
}

#ifdef DEVICE_POLLING
/*********************************************************************
 *
 *  Legacy polling routine : if using this code you MUST be sure that
 *  multiqueue is not defined, ie, set igb_num_queues to 1.
 *
 *********************************************************************/
#if __FreeBSD_version >= 800000
#define POLL_RETURN_COUNT(a) (a)
static int
#else
#define POLL_RETURN_COUNT(a)
static void
#endif
igb_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct adapter		*adapter = ifp->if_softc;
	struct igb_queue	*que = adapter->queues;
	struct tx_ring		*txr = adapter->tx_rings;
	u32			reg_icr, rx_done = 0;
	u32			loop = IGB_MAX_LOOP;
	bool			more;

	IGB_CORE_LOCK(adapter);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		IGB_CORE_UNLOCK(adapter);
		return POLL_RETURN_COUNT(rx_done);
	}

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);
		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC))
			igb_handle_link(adapter, 0);

		if (reg_icr & E1000_ICR_RXO)
			adapter->rx_overruns++;
	}
	IGB_CORE_UNLOCK(adapter);

	igb_rxeof(que, count, &rx_done);

	IGB_TX_LOCK(txr);
	do {
		more = igb_txeof(txr);
	} while (loop-- && more);
#if __FreeBSD_version >= 800000
	if (!drbr_empty(ifp, txr->br))
		igb_mq_start_locked(ifp, txr, NULL);
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		igb_start_locked(txr, ifp);
#endif
	IGB_TX_UNLOCK(txr);
	return POLL_RETURN_COUNT(rx_done);
}
#endif /* DEVICE_POLLING */

/*********************************************************************
 *
 *  MSIX TX Interrupt Service routine
 *
 **********************************************************************/
static void
igb_msix_que(void *arg)
{
	struct igb_queue *que = arg;
	struct adapter *adapter = que->adapter;
	struct tx_ring *txr = que->txr;
	struct rx_ring *rxr = que->rxr;
	u32		newitr = 0;
	bool		more_tx, more_rx;

	E1000_WRITE_REG(&adapter->hw, E1000_EIMC, que->eims);
	++que->irqs;

	IGB_TX_LOCK(txr);
	more_tx = igb_txeof(txr);
	IGB_TX_UNLOCK(txr);

	more_rx = igb_rxeof(que, adapter->rx_process_limit, NULL);

	if (igb_enable_aim == FALSE)
		goto no_calc;
	/*
	** Do Adaptive Interrupt Moderation:
        **  - Write out last calculated setting
	**  - Calculate based on average size over
	**    the last interval.
	*/
        if (que->eitr_setting)
                E1000_WRITE_REG(&adapter->hw,
                    E1000_EITR(que->msix), que->eitr_setting);
 
        que->eitr_setting = 0;

        /* Idle, do nothing */
        if ((txr->bytes == 0) && (rxr->bytes == 0))
                goto no_calc;
                                
        /* Used half Default if sub-gig */
        if (adapter->link_speed != 1000)
                newitr = IGB_DEFAULT_ITR / 2;
        else {
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
        }
        newitr &= 0x7FFC;  /* Mask invalid bits */
        if (adapter->hw.mac.type == e1000_82575)
                newitr |= newitr << 16;
        else
                newitr |= E1000_EITR_CNT_IGNR;
                 
        /* save for next interrupt */
        que->eitr_setting = newitr;

        /* Reset state */
        txr->bytes = 0;
        txr->packets = 0;
        rxr->bytes = 0;
        rxr->packets = 0;

no_calc:
	/* Schedule a clean task if needed*/
	if (more_tx || more_rx) 
		taskqueue_enqueue(que->tq, &que->que_task);
	else
		/* Reenable this interrupt */
		E1000_WRITE_REG(&adapter->hw, E1000_EIMS, que->eims);
	return;
}


/*********************************************************************
 *
 *  MSIX Link Interrupt Service routine
 *
 **********************************************************************/

static void
igb_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	u32       	icr;

	++adapter->link_irq;
	icr = E1000_READ_REG(&adapter->hw, E1000_ICR);
	if (!(icr & E1000_ICR_LSC))
		goto spurious;
	igb_handle_link(adapter, 0);

spurious:
	/* Rearm */
	E1000_WRITE_REG(&adapter->hw, E1000_IMS, E1000_IMS_LSC);
	E1000_WRITE_REG(&adapter->hw, E1000_EIMS, adapter->link_mask);
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
igb_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct adapter *adapter = ifp->if_softc;
	u_char fiber_type = IFM_1000_SX;

	INIT_DEBUGOUT("igb_media_status: begin");

	IGB_CORE_LOCK(adapter);
	igb_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		IGB_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes))
		ifmr->ifm_active |= fiber_type | IFM_FDX;
	else {
		switch (adapter->link_speed) {
		case 10:
			ifmr->ifm_active |= IFM_10_T;
			break;
		case 100:
			ifmr->ifm_active |= IFM_100_TX;
			break;
		case 1000:
			ifmr->ifm_active |= IFM_1000_T;
			break;
		}
		if (adapter->link_duplex == FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	}
	IGB_CORE_UNLOCK(adapter);
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
igb_media_change(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia  *ifm = &adapter->media;

	INIT_DEBUGOUT("igb_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	IGB_CORE_LOCK(adapter);
	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		adapter->hw.mac.autoneg = DO_AUTO_NEG;
		adapter->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_LX:
	case IFM_1000_SX:
	case IFM_1000_T:
		adapter->hw.mac.autoneg = DO_AUTO_NEG;
		adapter->hw.phy.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		adapter->hw.mac.autoneg = FALSE;
		adapter->hw.phy.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			adapter->hw.mac.forced_speed_duplex = ADVERTISE_100_FULL;
		else
			adapter->hw.mac.forced_speed_duplex = ADVERTISE_100_HALF;
		break;
	case IFM_10_T:
		adapter->hw.mac.autoneg = FALSE;
		adapter->hw.phy.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			adapter->hw.mac.forced_speed_duplex = ADVERTISE_10_FULL;
		else
			adapter->hw.mac.forced_speed_duplex = ADVERTISE_10_HALF;
		break;
	default:
		device_printf(adapter->dev, "Unsupported media type\n");
	}

	igb_init_locked(adapter);
	IGB_CORE_UNLOCK(adapter);

	return (0);
}


/*********************************************************************
 *
 *  This routine maps the mbufs to Advanced TX descriptors.
 *  used by the 82575 adapter.
 *  
 **********************************************************************/

static int
igb_xmit(struct tx_ring *txr, struct mbuf **m_headp)
{
	struct adapter		*adapter = txr->adapter;
	bus_dma_segment_t	segs[IGB_MAX_SCATTER];
	bus_dmamap_t		map;
	struct igb_tx_buffer	*tx_buffer, *tx_buffer_mapped;
	union e1000_adv_tx_desc	*txd = NULL;
	struct mbuf		*m_head;
	u32			olinfo_status = 0, cmd_type_len = 0;
	int			nsegs, i, j, error, first, last = 0;
	u32			hdrlen = 0;

	m_head = *m_headp;


	/* Set basic descriptor constants */
	cmd_type_len |= E1000_ADVTXD_DTYP_DATA;
	cmd_type_len |= E1000_ADVTXD_DCMD_IFCS | E1000_ADVTXD_DCMD_DEXT;
	if (m_head->m_flags & M_VLANTAG)
		cmd_type_len |= E1000_ADVTXD_DCMD_VLE;

        /*
         * Force a cleanup if number of TX descriptors
         * available hits the threshold
         */
	if (txr->tx_avail <= IGB_TX_CLEANUP_THRESHOLD) {
		igb_txeof(txr);
		/* Now do we at least have a minimal? */
		if (txr->tx_avail <= IGB_TX_OP_THRESHOLD) {
			txr->no_desc_avail++;
			return (ENOBUFS);
		}
	}

	/*
         * Map the packet for DMA.
	 *
	 * Capture the first descriptor index,
	 * this descriptor will have the index
	 * of the EOP which is the only one that
	 * now gets a DONE bit writeback.
	 */
	first = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[first];
	tx_buffer_mapped = tx_buffer;
	map = tx_buffer->map;

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

	/* Check again to be sure we have enough descriptors */
        if (nsegs > (txr->tx_avail - 2)) {
                txr->no_desc_avail++;
		bus_dmamap_unload(txr->txtag, map);
		return (ENOBUFS);
        }
	m_head = *m_headp;

        /*
         * Set up the context descriptor:
         * used when any hardware offload is done.
	 * This includes CSUM, VLAN, and TSO. It
	 * will use the first descriptor.
         */
        if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		if (igb_tso_setup(txr, m_head, &hdrlen)) {
			cmd_type_len |= E1000_ADVTXD_DCMD_TSE;
			olinfo_status |= E1000_TXD_POPTS_IXSM << 8;
			olinfo_status |= E1000_TXD_POPTS_TXSM << 8;
		} else
			return (ENXIO); 
	} else if (igb_tx_ctx_setup(txr, m_head))
		olinfo_status |= E1000_TXD_POPTS_TXSM << 8;

	/* Calculate payload length */
	olinfo_status |= ((m_head->m_pkthdr.len - hdrlen)
	    << E1000_ADVTXD_PAYLEN_SHIFT);

	/* 82575 needs the queue index added */
	if (adapter->hw.mac.type == e1000_82575)
		olinfo_status |= txr->me << 4;

	/* Set up our transmit descriptors */
	i = txr->next_avail_desc;
	for (j = 0; j < nsegs; j++) {
		bus_size_t seg_len;
		bus_addr_t seg_addr;

		tx_buffer = &txr->tx_buffers[i];
		txd = (union e1000_adv_tx_desc *)&txr->tx_base[i];
		seg_addr = segs[j].ds_addr;
		seg_len  = segs[j].ds_len;

		txd->read.buffer_addr = htole64(seg_addr);
		txd->read.cmd_type_len = htole32(cmd_type_len | seg_len);
		txd->read.olinfo_status = htole32(olinfo_status);
		last = i;
		if (++i == adapter->num_tx_desc)
			i = 0;
		tx_buffer->m_head = NULL;
		tx_buffer->next_eop = -1;
	}

	txr->next_avail_desc = i;
	txr->tx_avail -= nsegs;

        tx_buffer->m_head = m_head;
	tx_buffer_mapped->map = tx_buffer->map;
	tx_buffer->map = map;
        bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

        /*
         * Last Descriptor of Packet
	 * needs End Of Packet (EOP)
	 * and Report Status (RS)
         */
        txd->read.cmd_type_len |=
	    htole32(E1000_ADVTXD_DCMD_EOP | E1000_ADVTXD_DCMD_RS);
	/*
	 * Keep track in the first buffer which
	 * descriptor will be written back
	 */
	tx_buffer = &txr->tx_buffers[first];
	tx_buffer->next_eop = last;
	txr->watchdog_time = ticks;

	/*
	 * Advance the Transmit Descriptor Tail (TDT), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), i);
	++txr->tx_packets;

	return (0);

}

static void
igb_set_promisc(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct e1000_hw *hw = &adapter->hw;
	u32		reg;

	if (hw->mac.type == e1000_vfadapt) {
		e1000_promisc_set_vf(hw, e1000_promisc_enabled);
		return;
	}

	reg = E1000_READ_REG(hw, E1000_RCTL);
	if (ifp->if_flags & IFF_PROMISC) {
		reg |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		E1000_WRITE_REG(hw, E1000_RCTL, reg);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg |= E1000_RCTL_MPE;
		reg &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(hw, E1000_RCTL, reg);
	}
}

static void
igb_disable_promisc(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32		reg;

	if (hw->mac.type == e1000_vfadapt) {
		e1000_promisc_set_vf(hw, e1000_promisc_disabled);
		return;
	}
	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &=  (~E1000_RCTL_UPE);
	reg &=  (~E1000_RCTL_MPE);
	E1000_WRITE_REG(hw, E1000_RCTL, reg);
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
igb_set_multi(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct ifmultiaddr *ifma;
	u32 reg_rctl = 0;
	u8  *mta;

	int mcnt = 0;

	IOCTL_DEBUGOUT("igb_set_multi: begin");

	mta = adapter->mta;
	bzero(mta, sizeof(uint8_t) * ETH_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES);

#if __FreeBSD_version < 800000
	IF_ADDR_LOCK(ifp);
#else
	if_maddr_rlock(ifp);
#endif
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;

		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    &mta[mcnt * ETH_ADDR_LEN], ETH_ADDR_LEN);
		mcnt++;
	}
#if __FreeBSD_version < 800000
	IF_ADDR_UNLOCK(ifp);
#else
	if_maddr_runlock(ifp);
#endif

	if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		reg_rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	} else
		e1000_update_mc_addr_list(&adapter->hw, mta, mcnt);
}


/*********************************************************************
 *  Timer routine:
 *  	This routine checks for link status,
 *	updates statistics, and does the watchdog.
 *
 **********************************************************************/

static void
igb_local_timer(void *arg)
{
	struct adapter		*adapter = arg;
	device_t		dev = adapter->dev;
	struct tx_ring		*txr = adapter->tx_rings;


	IGB_CORE_LOCK_ASSERT(adapter);

	igb_update_link_status(adapter);
	igb_update_stats_counters(adapter);

	/* 
	** If flow control has paused us since last checking
	** it invalidates the watchdog timing, so dont run it.
	*/
	if (adapter->pause_frames) {
		adapter->pause_frames = 0;
		goto out;
	}

        /*
        ** Watchdog: check for time since any descriptor was cleaned
        */
	for (int i = 0; i < adapter->num_queues; i++, txr++)
		if (txr->queue_status == IGB_QUEUE_HUNG) 
			goto timeout;
out:
	callout_reset(&adapter->timer, hz, igb_local_timer, adapter);
	return;

timeout:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	device_printf(dev,"Queue(%d) tdh = %d, hw tdt = %d\n", txr->me,
            E1000_READ_REG(&adapter->hw, E1000_TDH(txr->me)),
            E1000_READ_REG(&adapter->hw, E1000_TDT(txr->me)));
	device_printf(dev,"TX(%d) desc avail = %d,"
            "Next TX to Clean = %d\n",
            txr->me, txr->tx_avail, txr->next_to_clean);
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	igb_init_locked(adapter);
}

static void
igb_update_link_status(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct ifnet *ifp = adapter->ifp;
	device_t dev = adapter->dev;
	struct tx_ring *txr = adapter->tx_rings;
	u32 link_check = 0;

	/* Get the cached link value or read for real */
        switch (hw->phy.media_type) {
        case e1000_media_type_copper:
                if (hw->mac.get_link_status) {
			/* Do the work to read phy */
                        e1000_check_for_link(hw);
                        link_check = !hw->mac.get_link_status;
                } else
                        link_check = TRUE;
                break;
        case e1000_media_type_fiber:
                e1000_check_for_link(hw);
                link_check = (E1000_READ_REG(hw, E1000_STATUS) &
                                 E1000_STATUS_LU);
                break;
        case e1000_media_type_internal_serdes:
                e1000_check_for_link(hw);
                link_check = adapter->hw.mac.serdes_has_link;
                break;
	/* VF device is type_unknown */
        case e1000_media_type_unknown:
                e1000_check_for_link(hw);
		link_check = !hw->mac.get_link_status;
		/* Fall thru */
        default:
                break;
        }

	/* Now we check if a transition has happened */
	if (link_check && (adapter->link_active == 0)) {
		e1000_get_speed_and_duplex(&adapter->hw, 
		    &adapter->link_speed, &adapter->link_duplex);
		if (bootverbose)
			device_printf(dev, "Link is up %d Mbps %s\n",
			    adapter->link_speed,
			    ((adapter->link_duplex == FULL_DUPLEX) ?
			    "Full Duplex" : "Half Duplex"));
		adapter->link_active = 1;
		ifp->if_baudrate = adapter->link_speed * 1000000;
		/* This can sleep */
		if_link_state_change(ifp, LINK_STATE_UP);
	} else if (!link_check && (adapter->link_active == 1)) {
		ifp->if_baudrate = adapter->link_speed = 0;
		adapter->link_duplex = 0;
		if (bootverbose)
			device_printf(dev, "Link is Down\n");
		adapter->link_active = 0;
		/* This can sleep */
		if_link_state_change(ifp, LINK_STATE_DOWN);
		/* Turn off watchdogs */
		for (int i = 0; i < adapter->num_queues; i++, txr++)
			txr->queue_status = IGB_QUEUE_IDLE;
	}
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

static void
igb_stop(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring *txr = adapter->tx_rings;

	IGB_CORE_LOCK_ASSERT(adapter);

	INIT_DEBUGOUT("igb_stop: begin");

	igb_disable_intr(adapter);

	callout_stop(&adapter->timer);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	/* Unarm watchdog timer. */
	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IGB_TX_LOCK(txr);
		txr->queue_status = IGB_QUEUE_IDLE;
		IGB_TX_UNLOCK(txr);
	}

	e1000_reset_hw(&adapter->hw);
	E1000_WRITE_REG(&adapter->hw, E1000_WUC, 0);

	e1000_led_off(&adapter->hw);
	e1000_cleanup_led(&adapter->hw);
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
igb_identify_hardware(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	/* Make sure our PCI config space has the necessary stuff set */
	adapter->hw.bus.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
	if (!((adapter->hw.bus.pci_cmd_word & PCIM_CMD_BUSMASTEREN) &&
	    (adapter->hw.bus.pci_cmd_word & PCIM_CMD_MEMEN))) {
		INIT_DEBUGOUT("Memory Access and/or Bus Master "
		    "bits were not set!\n");
		adapter->hw.bus.pci_cmd_word |=
		(PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
		pci_write_config(dev, PCIR_COMMAND,
		    adapter->hw.bus.pci_cmd_word, 2);
	}

	/* Save off the information about this board */
	adapter->hw.vendor_id = pci_get_vendor(dev);
	adapter->hw.device_id = pci_get_device(dev);
	adapter->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	adapter->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	adapter->hw.subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Set MAC type early for PCI setup */
	e1000_set_mac_type(&adapter->hw);
}

static int
igb_allocate_pci_resources(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	int		rid;

	rid = PCIR_BAR(0);
	adapter->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (adapter->pci_mem == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	adapter->osdep.mem_bus_space_tag =
	    rman_get_bustag(adapter->pci_mem);
	adapter->osdep.mem_bus_space_handle =
	    rman_get_bushandle(adapter->pci_mem);
	adapter->hw.hw_addr = (u8 *)&adapter->osdep.mem_bus_space_handle;

	adapter->num_queues = 1; /* Defaults for Legacy or MSI */

	/* This will setup either MSI/X or MSI */
	adapter->msix = igb_setup_msix(adapter);
	adapter->hw.back = &adapter->osdep;

	return (0);
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
static int
igb_allocate_legacy(struct adapter *adapter)
{
	device_t		dev = adapter->dev;
	struct igb_queue	*que = adapter->queues;
	int			error, rid = 0;

	/* Turn off all interrupts */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	/* MSI RID is 1 */
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
	TASK_INIT(&que->que_task, 0, igb_handle_que, que);
	/* Make tasklet for deferred link handling */
	TASK_INIT(&adapter->link_task, 0, igb_handle_link, adapter);
	que->tq = taskqueue_create_fast("igb_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(adapter->dev));
	if ((error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, igb_irq_fast, NULL,
	    adapter, &adapter->tag)) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
			    "handler: %d\n", error);
		taskqueue_free(que->tq);
		que->tq = NULL;
		return (error);
	}

	return (0);
}


/*********************************************************************
 *
 *  Setup the MSIX Queue Interrupt handlers: 
 *
 **********************************************************************/
static int
igb_allocate_msix(struct adapter *adapter)
{
	device_t		dev = adapter->dev;
	struct igb_queue	*que = adapter->queues;
	int			error, rid, vector = 0;


	for (int i = 0; i < adapter->num_queues; i++, vector++, que++) {
		rid = vector +1;
		que->res = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
		if (que->res == NULL) {
			device_printf(dev,
			    "Unable to allocate bus resource: "
			    "MSIX Queue Interrupt\n");
			return (ENXIO);
		}
		error = bus_setup_intr(dev, que->res,
	    	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
		    igb_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register Queue handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, que->res, que->tag, "que %d", i);
#endif
		que->msix = vector;
		if (adapter->hw.mac.type == e1000_82575)
			que->eims = E1000_EICR_TX_QUEUE0 << i;
		else
			que->eims = 1 << vector;
		/*
		** Bind the msix vector, and thus the
		** rings to the corresponding cpu.
		*/
		if (adapter->num_queues > 1)
			bus_bind_intr(dev, que->res, i);
		/* Make tasklet for deferred handling */
		TASK_INIT(&que->que_task, 0, igb_handle_que, que);
		que->tq = taskqueue_create_fast("igb_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
		taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
		    device_get_nameunit(adapter->dev));
	}

	/* And Link */
	rid = vector + 1;
	adapter->res = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res == NULL) {
		device_printf(dev,
		    "Unable to allocate bus resource: "
		    "MSIX Link Interrupt\n");
		return (ENXIO);
	}
	if ((error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    igb_msix_link, adapter, &adapter->tag)) != 0) {
		device_printf(dev, "Failed to register Link handler");
		return (error);
	}
#if __FreeBSD_version >= 800504
	bus_describe_intr(dev, adapter->res, adapter->tag, "link");
#endif
	adapter->linkvec = vector;

	return (0);
}


static void
igb_configure_queues(struct adapter *adapter)
{
	struct	e1000_hw	*hw = &adapter->hw;
	struct	igb_queue	*que;
	u32			tmp, ivar = 0, newitr = 0;

	/* First turn on RSS capability */
	if (adapter->hw.mac.type > e1000_82575)
		E1000_WRITE_REG(hw, E1000_GPIE,
		    E1000_GPIE_MSIX_MODE | E1000_GPIE_EIAME |
		    E1000_GPIE_PBA | E1000_GPIE_NSICR);

	/* Turn on MSIX */
	switch (adapter->hw.mac.type) {
	case e1000_82580:
	case e1000_vfadapt:
		/* RX entries */
		for (int i = 0; i < adapter->num_queues; i++) {
			u32 index = i >> 1;
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			que = &adapter->queues[i];
			if (i & 1) {
				ivar &= 0xFF00FFFF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 16;
			} else {
				ivar &= 0xFFFFFF00;
				ivar |= que->msix | E1000_IVAR_VALID;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
		}
		/* TX entries */
		for (int i = 0; i < adapter->num_queues; i++) {
			u32 index = i >> 1;
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			que = &adapter->queues[i];
			if (i & 1) {
				ivar &= 0x00FFFFFF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 24;
			} else {
				ivar &= 0xFFFF00FF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 8;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			adapter->eims_mask |= que->eims;
		}

		/* And for the link interrupt */
		ivar = (adapter->linkvec | E1000_IVAR_VALID) << 8;
		adapter->link_mask = 1 << adapter->linkvec;
		adapter->eims_mask |= adapter->link_mask;
		E1000_WRITE_REG(hw, E1000_IVAR_MISC, ivar);
		break;
	case e1000_82576:
		/* RX entries */
		for (int i = 0; i < adapter->num_queues; i++) {
			u32 index = i & 0x7; /* Each IVAR has two entries */
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			que = &adapter->queues[i];
			if (i < 8) {
				ivar &= 0xFFFFFF00;
				ivar |= que->msix | E1000_IVAR_VALID;
			} else {
				ivar &= 0xFF00FFFF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 16;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			adapter->eims_mask |= que->eims;
		}
		/* TX entries */
		for (int i = 0; i < adapter->num_queues; i++) {
			u32 index = i & 0x7; /* Each IVAR has two entries */
			ivar = E1000_READ_REG_ARRAY(hw, E1000_IVAR0, index);
			que = &adapter->queues[i];
			if (i < 8) {
				ivar &= 0xFFFF00FF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 8;
			} else {
				ivar &= 0x00FFFFFF;
				ivar |= (que->msix | E1000_IVAR_VALID) << 24;
			}
			E1000_WRITE_REG_ARRAY(hw, E1000_IVAR0, index, ivar);
			adapter->eims_mask |= que->eims;
		}

		/* And for the link interrupt */
		ivar = (adapter->linkvec | E1000_IVAR_VALID) << 8;
		adapter->link_mask = 1 << adapter->linkvec;
		adapter->eims_mask |= adapter->link_mask;
		E1000_WRITE_REG(hw, E1000_IVAR_MISC, ivar);
		break;

	case e1000_82575:
                /* enable MSI-X support*/
		tmp = E1000_READ_REG(hw, E1000_CTRL_EXT);
                tmp |= E1000_CTRL_EXT_PBA_CLR;
                /* Auto-Mask interrupts upon ICR read. */
                tmp |= E1000_CTRL_EXT_EIAME;
                tmp |= E1000_CTRL_EXT_IRCA;
                E1000_WRITE_REG(hw, E1000_CTRL_EXT, tmp);

		/* Queues */
		for (int i = 0; i < adapter->num_queues; i++) {
			que = &adapter->queues[i];
			tmp = E1000_EICR_RX_QUEUE0 << i;
			tmp |= E1000_EICR_TX_QUEUE0 << i;
			que->eims = tmp;
			E1000_WRITE_REG_ARRAY(hw, E1000_MSIXBM(0),
			    i, que->eims);
			adapter->eims_mask |= que->eims;
		}

		/* Link */
		E1000_WRITE_REG(hw, E1000_MSIXBM(adapter->linkvec),
		    E1000_EIMS_OTHER);
		adapter->link_mask |= E1000_EIMS_OTHER;
		adapter->eims_mask |= adapter->link_mask;
	default:
		break;
	}

	/* Set the starting interrupt rate */
	if (igb_max_interrupt_rate > 0)
		newitr = (4000000 / igb_max_interrupt_rate) & 0x7FFC;

        if (hw->mac.type == e1000_82575)
                newitr |= newitr << 16;
        else
                newitr |= E1000_EITR_CNT_IGNR;

	for (int i = 0; i < adapter->num_queues; i++) {
		que = &adapter->queues[i];
		E1000_WRITE_REG(hw, E1000_EITR(que->msix), newitr);
	}

	return;
}


static void
igb_free_pci_resources(struct adapter *adapter)
{
	struct		igb_queue *que = adapter->queues;
	device_t	dev = adapter->dev;
	int		rid;

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
	 * First release all the interrupt resources:
	 */
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL)
			bus_release_resource(dev,
			    SYS_RES_IRQ, rid, que->res);
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
		    PCIR_BAR(IGB_MSIX_BAR), adapter->msix_mem);

	if (adapter->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->pci_mem);

}

/*
 * Setup Either MSI/X or MSI
 */
static int
igb_setup_msix(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int rid, want, queues, msgs;

	/* tuneable override */
	if (igb_enable_msix == 0)
		goto msi;

	/* First try MSI/X */
	rid = PCIR_BAR(IGB_MSIX_BAR);
	adapter->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (!adapter->msix_mem) {
		/* May not be enabled */
		device_printf(adapter->dev,
		    "Unable to map MSIX table \n");
		goto msi;
	}

	msgs = pci_msix_count(dev); 
	if (msgs == 0) { /* system has msix disabled */
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(IGB_MSIX_BAR), adapter->msix_mem);
		adapter->msix_mem = NULL;
		goto msi;
	}

	/* Figure out a reasonable auto config value */
	queues = (mp_ncpus > (msgs-1)) ? (msgs-1) : mp_ncpus;

	/* Manual override */
	if (igb_num_queues != 0)
		queues = igb_num_queues;
	if (queues > 8)  /* max queues */
		queues = 8;

	/* Can have max of 4 queues on 82575 */
	if ((adapter->hw.mac.type == e1000_82575) && (queues > 4))
		queues = 4;

	/* Limit the VF adapter to one queue */
	if (adapter->hw.mac.type == e1000_vfadapt)
		queues = 1;

	/*
	** One vector (RX/TX pair) per queue
	** plus an additional for Link interrupt
	*/
	want = queues + 1;
	if (msgs >= want)
		msgs = want;
	else {
               	device_printf(adapter->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors configured, but %d queues wanted!\n",
		    msgs, want);
		return (ENXIO);
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

/*********************************************************************
 *
 *  Set up an fresh starting state
 *
 **********************************************************************/
static void
igb_reset(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_fc_info *fc = &hw->fc;
	struct ifnet	*ifp = adapter->ifp;
	u32		pba = 0;
	u16		hwm;

	INIT_DEBUGOUT("igb_reset: begin");

	/* Let the firmware know the OS is in control */
	igb_get_hw_control(adapter);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	switch (hw->mac.type) {
	case e1000_82575:
		pba = E1000_PBA_32K;
		break;
	case e1000_82576:
	case e1000_vfadapt:
		pba = E1000_PBA_64K;
		break;
	case e1000_82580:
		pba = E1000_PBA_35K;
	default:
		break;
	}

	/* Special needs in case of Jumbo frames */
	if ((hw->mac.type == e1000_82575) && (ifp->if_mtu > ETHERMTU)) {
		u32 tx_space, min_tx, min_rx;
		pba = E1000_READ_REG(hw, E1000_PBA);
		tx_space = pba >> 16;
		pba &= 0xffff;
		min_tx = (adapter->max_frame_size +
		    sizeof(struct e1000_tx_desc) - ETHERNET_FCS_SIZE) * 2;
		min_tx = roundup2(min_tx, 1024);
		min_tx >>= 10;
                min_rx = adapter->max_frame_size;
                min_rx = roundup2(min_rx, 1024);
                min_rx >>= 10;
		if (tx_space < min_tx &&
		    ((min_tx - tx_space) < pba)) {
			pba = pba - (min_tx - tx_space);
			/*
                         * if short on rx space, rx wins
                         * and must trump tx adjustment
			 */
                        if (pba < min_rx)
                                pba = min_rx;
		}
		E1000_WRITE_REG(hw, E1000_PBA, pba);
	}

	INIT_DEBUGOUT1("igb_init: pba=%dK",pba);

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two frames to be
	 *   received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit.
	 */
	hwm = min(((pba << 10) * 9 / 10),
	    ((pba << 10) - 2 * adapter->max_frame_size));

	if (hw->mac.type < e1000_82576) {
		fc->high_water = hwm & 0xFFF8;  /* 8-byte granularity */
		fc->low_water = fc->high_water - 8;
	} else {
		fc->high_water = hwm & 0xFFF0;  /* 16-byte granularity */
		fc->low_water = fc->high_water - 16;
	}

	fc->pause_time = IGB_FC_PAUSE_TIME;
	fc->send_xon = TRUE;

	/* Set Flow control, use the tunable location if sane */
	if ((igb_fc_setting >= 0) && (igb_fc_setting < 4))
		fc->requested_mode = igb_fc_setting;
	else
		fc->requested_mode = e1000_fc_none;

	fc->current_mode = fc->requested_mode;

	/* Issue a global reset */
	e1000_reset_hw(hw);
	E1000_WRITE_REG(hw, E1000_WUC, 0);

	if (e1000_init_hw(hw) < 0)
		device_printf(dev, "Hardware Initialization Failed\n");

	if (hw->mac.type == e1000_82580) {
		u32 reg;

		hwm = (pba << 10) - (2 * adapter->max_frame_size);
		/*
		 * 0x80000000 - enable DMA COAL
		 * 0x10000000 - use L0s as low power
		 * 0x20000000 - use L1 as low power
		 * X << 16 - exit dma coal when rx data exceeds X kB
		 * Y - upper limit to stay in dma coal in units of 32usecs
		 */
		E1000_WRITE_REG(hw, E1000_DMACR,
		    0xA0000006 | ((hwm << 6) & 0x00FF0000));

		/* set hwm to PBA -  2 * max frame size */
		E1000_WRITE_REG(hw, E1000_FCRTC, hwm);
		/*
		 * This sets the time to wait before requesting transition to
		 * low power state to number of usecs needed to receive 1 512
		 * byte frame at gigabit line rate
		 */
		E1000_WRITE_REG(hw, E1000_DMCTLX, 4);

		/* free space in tx packet buffer to wake from DMA coal */
		E1000_WRITE_REG(hw, E1000_DMCTXTH,
		    (20480 - (2 * adapter->max_frame_size)) >> 6);

		/* make low power state decision controlled by DMA coal */
		reg = E1000_READ_REG(hw, E1000_PCIEMISC);
		E1000_WRITE_REG(hw, E1000_PCIEMISC,
		    reg | E1000_PCIEMISC_LX_DECISION);
	}

	E1000_WRITE_REG(&adapter->hw, E1000_VET, ETHERTYPE_VLAN);
	e1000_get_phy_info(hw);
	e1000_check_for_link(hw);
	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
igb_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("igb_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_init =  igb_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = igb_ioctl;
	ifp->if_start = igb_start;
#if __FreeBSD_version >= 800000
	ifp->if_transmit = igb_mq_start;
	ifp->if_qflush = igb_qflush;
#endif
	IFQ_SET_MAXLEN(&ifp->if_snd, adapter->num_tx_desc - 1);
	ifp->if_snd.ifq_drv_maxlen = adapter->num_tx_desc - 1;
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, adapter->hw.mac.addr);

	ifp->if_capabilities = ifp->if_capenable = 0;

	ifp->if_capabilities = IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
	ifp->if_capabilities |= IFCAP_TSO4;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	ifp->if_capenable = ifp->if_capabilities;

	/* Don't enable LRO by default */
	ifp->if_capabilities |= IFCAP_LRO;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * Tell the upper layer(s) we
	 * support full VLAN capability.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;

	/*
	** Dont turn this on by default, if vlans are
	** created on another pseudo device (eg. lagg)
	** then vlan events are not passed thru, breaking
	** operation, but with HW FILTER off it works. If
	** using vlans directly on the em driver you can
	** enable this and get full hardware tag filtering.
	*/
	ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK,
	    igb_media_change, igb_media_status);
	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes)) {
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	} else {
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_10_T | IFM_FDX,
			    0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_100_TX,
			    0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | IFM_100_TX | IFM_FDX,
			    0, NULL);
		if (adapter->hw.phy.type != e1000_phy_ife) {
			ifmedia_add(&adapter->media,
				IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
			ifmedia_add(&adapter->media,
				IFM_ETHER | IFM_1000_T, 0, NULL);
		}
	}
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);
	return (0);
}


/*
 * Manage DMA'able memory.
 */
static void
igb_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs[0].ds_addr;
}

static int
igb_dma_malloc(struct adapter *adapter, bus_size_t size,
        struct igb_dma_alloc *dma, int mapflags)
{
	int error;

	error = bus_dma_tag_create(bus_get_dma_tag(adapter->dev), /* parent */
				IGB_DBA_ALIGN, 0,	/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				size,			/* maxsize */
				1,			/* nsegments */
				size,			/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&dma->dma_tag);
	if (error) {
		device_printf(adapter->dev,
		    "%s: bus_dma_tag_create failed: %d\n",
		    __func__, error);
		goto fail_0;
	}

	error = bus_dmamem_alloc(dma->dma_tag, (void**) &dma->dma_vaddr,
	    BUS_DMA_NOWAIT, &dma->dma_map);
	if (error) {
		device_printf(adapter->dev,
		    "%s: bus_dmamem_alloc(%ju) failed: %d\n",
		    __func__, (uintmax_t)size, error);
		goto fail_2;
	}

	dma->dma_paddr = 0;
	error = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
	    size, igb_dmamap_cb, &dma->dma_paddr, mapflags | BUS_DMA_NOWAIT);
	if (error || dma->dma_paddr == 0) {
		device_printf(adapter->dev,
		    "%s: bus_dmamap_load failed: %d\n",
		    __func__, error);
		goto fail_3;
	}

	return (0);

fail_3:
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
fail_2:
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
	bus_dma_tag_destroy(dma->dma_tag);
fail_0:
	dma->dma_map = NULL;
	dma->dma_tag = NULL;

	return (error);
}

static void
igb_dma_free(struct adapter *adapter, struct igb_dma_alloc *dma)
{
	if (dma->dma_tag == NULL)
		return;
	if (dma->dma_map != NULL) {
		bus_dmamap_sync(dma->dma_tag, dma->dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
		dma->dma_map = NULL;
	}
	bus_dma_tag_destroy(dma->dma_tag);
	dma->dma_tag = NULL;
}


/*********************************************************************
 *
 *  Allocate memory for the transmit and receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
static int
igb_allocate_queues(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct igb_queue	*que = NULL;
	struct tx_ring		*txr = NULL;
	struct rx_ring		*rxr = NULL;
	int rsize, tsize, error = E1000_SUCCESS;
	int txconf = 0, rxconf = 0;

	/* First allocate the top level queue structs */
	if (!(adapter->queues =
	    (struct igb_queue *) malloc(sizeof(struct igb_queue) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate queue memory\n");
		error = ENOMEM;
		goto fail;
	}

	/* Next allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto tx_fail;
	}

	/* Now allocate the RX */
	if (!(adapter->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate RX ring memory\n");
		error = ENOMEM;
		goto rx_fail;
	}

	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union e1000_adv_tx_desc), IGB_DBA_ALIGN);
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

		/* Initialize the TX lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(dev), txr->me);
		mtx_init(&txr->tx_mtx, txr->mtx_name, NULL, MTX_DEF);

		if (igb_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (struct e1000_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	/* Now allocate transmit buffers for the ring */
        	if (igb_allocate_transmit_buffers(txr)) {
			device_printf(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#if __FreeBSD_version >= 800000
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(IGB_BR_SIZE, M_DEVBUF,
		    M_WAITOK, &txr->tx_mtx);
#endif
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union e1000_adv_rx_desc), IGB_DBA_ALIGN);
	for (int i = 0; i < adapter->num_queues; i++, rxconf++) {
		rxr = &adapter->rx_rings[i];
		rxr->adapter = adapter;
		rxr->me = i;

		/* Initialize the RX lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), txr->me);
		mtx_init(&rxr->rx_mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (igb_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (union e1000_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (igb_allocate_receive_buffers(rxr)) {
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
		igb_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		igb_dma_free(adapter, &txr->txdma);
	free(adapter->rx_rings, M_DEVBUF);
rx_fail:
#if __FreeBSD_version >= 800000
	buf_ring_free(txr->br, M_DEVBUF);
#endif
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
igb_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct igb_tx_buffer *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       IGB_TSO_SIZE,		/* maxsize */
			       IGB_MAX_SCATTER,		/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->txtag))) {
		device_printf(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct igb_tx_buffer *) malloc(sizeof(struct igb_tx_buffer) *
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
	igb_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
igb_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct igb_tx_buffer *txbuf;
	int i;

	/* Clear the old descriptor contents */
	IGB_TX_LOCK(txr);
	bzero((void *)txr->tx_base,
	      (sizeof(union e1000_adv_tx_desc)) * adapter->num_tx_desc);
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
		/* clear the watch index */
		txbuf->next_eop = -1;
        }

	/* Set number of descriptors available */
	txr->tx_avail = adapter->num_tx_desc;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	IGB_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static void
igb_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		igb_setup_transmit_ring(txr);

	return;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
igb_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct e1000_hw *hw = &adapter->hw;
	u32		tctl, txdctl;

	INIT_DEBUGOUT("igb_initialize_transmit_units: begin");
	tctl = txdctl = 0;

	/* Setup the Tx Descriptor Rings */
	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64 bus_addr = txr->txdma.dma_paddr;

		E1000_WRITE_REG(hw, E1000_TDLEN(i),
		    adapter->num_tx_desc * sizeof(struct e1000_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
		    (uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i),
		    (uint32_t)bus_addr);

		/* Setup the HW Tx Head and Tail descriptor pointers */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		HW_DEBUGOUT2("Base = %x, Length = %x\n",
		    E1000_READ_REG(hw, E1000_TDBAL(i)),
		    E1000_READ_REG(hw, E1000_TDLEN(i)));

		txr->queue_status = IGB_QUEUE_IDLE;

		txdctl |= IGB_TX_PTHRESH;
		txdctl |= IGB_TX_HTHRESH << 8;
		txdctl |= IGB_TX_WTHRESH << 16;
		txdctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(hw, E1000_TXDCTL(i), txdctl);
	}

	if (adapter->hw.mac.type == e1000_vfadapt)
		return;

	e1000_config_collision_dist(hw);

	/* Program the Transmit Control Register */
	tctl = E1000_READ_REG(hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(hw, E1000_TCTL, tctl);
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
igb_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		IGB_TX_LOCK(txr);
		igb_free_transmit_buffers(txr);
		igb_dma_free(adapter, &txr->txdma);
		IGB_TX_UNLOCK(txr);
		IGB_TX_LOCK_DESTROY(txr);
	}
	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
igb_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct igb_tx_buffer *tx_buffer;
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

/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO)
 *
 **********************************************************************/
static boolean_t
igb_tso_setup(struct tx_ring *txr, struct mbuf *mp, u32 *hdrlen)
{
	struct adapter *adapter = txr->adapter;
	struct e1000_adv_tx_context_desc *TXD;
	struct igb_tx_buffer        *tx_buffer;
	u32 vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	u32 mss_l4len_idx = 0;
	u16 vtag = 0;
	int ctxd, ehdrlen, ip_hlen, tcp_hlen;
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

	/* Only supports IPV4 for now */
	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct e1000_adv_tx_context_desc *) &txr->tx_base[ctxd];

	ip = (struct ip *)(mp->m_data + ehdrlen);
	if (ip->ip_p != IPPROTO_TCP)
                return FALSE;   /* 0 */
	ip->ip_sum = 0;
	ip_hlen = ip->ip_hl << 2;
	th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
	th->th_sum = in_pseudo(ip->ip_src.s_addr,
	    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
	tcp_hlen = th->th_off << 2;
	/*
	 * Calculate header length, this is used
	 * in the transmit desc in igb_xmit
	 */
	*hdrlen = ehdrlen + ip_hlen + tcp_hlen;

	/* VLAN MACLEN IPLEN */
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << E1000_ADVTXD_VLAN_SHIFT);
	}

	vlan_macip_lens |= (ehdrlen << E1000_ADVTXD_MACLEN_SHIFT);
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= E1000_ADVTXD_DCMD_DEXT | E1000_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_TCP;
	type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_IPV4;
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);

	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.tso_segsz << E1000_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << E1000_ADVTXD_L4LEN_SHIFT);
	/* 82575 needs the queue index added */
	if (adapter->hw.mac.type == e1000_82575)
		mss_l4len_idx |= txr->me << 4;
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);
	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	return TRUE;
}


/*********************************************************************
 *
 *  Context Descriptor setup for VLAN or CSUM
 *
 **********************************************************************/

static bool
igb_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter *adapter = txr->adapter;
	struct e1000_adv_tx_context_desc *TXD;
	struct igb_tx_buffer        *tx_buffer;
	u32 vlan_macip_lens, type_tucmd_mlhl, mss_l4len_idx;
	struct ether_vlan_header *eh;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6;
	int  ehdrlen, ctxd, ip_hlen = 0;
	u16	etype, vtag = 0;
	u8	ipproto = 0;
	bool	offload = TRUE;

	if ((mp->m_pkthdr.csum_flags & CSUM_OFFLOAD) == 0)
		offload = FALSE;

	vlan_macip_lens = type_tucmd_mlhl = mss_l4len_idx = 0;
	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct e1000_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the context descriptor, thus
	** we need to be here just for that setup.
	*/
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << E1000_ADVTXD_VLAN_SHIFT);
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
	vlan_macip_lens |= ehdrlen << E1000_ADVTXD_MACLEN_SHIFT;

	switch (etype) {
		case ETHERTYPE_IP:
			ip = (struct ip *)(mp->m_data + ehdrlen);
			ip_hlen = ip->ip_hl << 2;
			if (mp->m_len < ehdrlen + ip_hlen) {
				offload = FALSE;
				break;
			}
			ipproto = ip->ip_p;
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_IPV4;
			break;
		case ETHERTYPE_IPV6:
			ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
			ip_hlen = sizeof(struct ip6_hdr);
			ipproto = ip6->ip6_nxt;
			type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_IPV6;
			break;
		default:
			offload = FALSE;
			break;
	}

	vlan_macip_lens |= ip_hlen;
	type_tucmd_mlhl |= E1000_ADVTXD_DCMD_DEXT | E1000_ADVTXD_DTYP_CTXT;

	switch (ipproto) {
		case IPPROTO_TCP:
			if (mp->m_pkthdr.csum_flags & CSUM_TCP)
				type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_TCP;
			break;
		case IPPROTO_UDP:
			if (mp->m_pkthdr.csum_flags & CSUM_UDP)
				type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_UDP;
			break;
#if __FreeBSD_version >= 800000
		case IPPROTO_SCTP:
			if (mp->m_pkthdr.csum_flags & CSUM_SCTP)
				type_tucmd_mlhl |= E1000_ADVTXD_TUCMD_L4T_SCTP;
			break;
#endif
		default:
			offload = FALSE;
			break;
	}

	/* 82575 needs the queue index added */
	if (adapter->hw.mac.type == e1000_82575)
		mss_l4len_idx = txr->me << 4;

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return (offload);
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 *  TRUE return means there's work in the ring to clean, FALSE its empty.
 **********************************************************************/
static bool
igb_txeof(struct tx_ring *txr)
{
	struct adapter	*adapter = txr->adapter;
        int first, last, done, processed;
        struct igb_tx_buffer *tx_buffer;
        struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = adapter->ifp;

	IGB_TX_LOCK_ASSERT(txr);

        if (txr->tx_avail == adapter->num_tx_desc) {
		txr->queue_status = IGB_QUEUE_IDLE;
                return FALSE;
	}

	processed = 0;
        first = txr->next_to_clean;
        tx_desc = &txr->tx_base[first];
        tx_buffer = &txr->tx_buffers[first];
	last = tx_buffer->next_eop;
        eop_desc = &txr->tx_base[last];

	/*
	 * What this does is get the index of the
	 * first descriptor AFTER the EOP of the 
	 * first packet, that way we can do the
	 * simple comparison on the inner while loop.
	 */
	if (++last == adapter->num_tx_desc)
 		last = 0;
	done = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

        while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
                	tx_desc->upper.data = 0;
                	tx_desc->lower.data = 0;
                	tx_desc->buffer_addr = 0;
                	++txr->tx_avail;
			++processed;

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
                	}
			tx_buffer->next_eop = -1;
			txr->watchdog_time = ticks;

	                if (++first == adapter->num_tx_desc)
				first = 0;

	                tx_buffer = &txr->tx_buffers[first];
			tx_desc = &txr->tx_base[first];
		}
		++txr->packets;
		++ifp->if_opackets;
		/* See if we can continue to the next packet */
		last = tx_buffer->next_eop;
		if (last != -1) {
        		eop_desc = &txr->tx_base[last];
			/* Get new done point */
			if (++last == adapter->num_tx_desc) last = 0;
			done = last;
		} else
			break;
        }
        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

        txr->next_to_clean = first;

	/*
	** Watchdog calculation, we know there's
	** work outstanding or the first return
	** would have been taken, so none processed
	** for too long indicates a hang.
	*/
	if ((!processed) && ((ticks - txr->watchdog_time) > IGB_WATCHDOG))
		txr->queue_status = IGB_QUEUE_HUNG;

        /*
         * If we have enough room, clear IFF_DRV_OACTIVE
         * to tell the stack that it is OK to send packets.
         */
        if (txr->tx_avail > IGB_TX_CLEANUP_THRESHOLD) {                
                ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		/* All clean, turn off the watchdog */
                if (txr->tx_avail == adapter->num_tx_desc) {
			txr->queue_status = IGB_QUEUE_IDLE;
			return (FALSE);
		}
        }

	return (TRUE);
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
igb_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	bus_dma_segment_t	hseg[1];
	bus_dma_segment_t	pseg[1];
	struct igb_rx_buf	*rxbuf;
	struct mbuf		*mh, *mp;
	int			i, nsegs, error, cleaned;

	i = rxr->next_to_refresh;
	cleaned = -1; /* Signify no completions */
	while (i != limit) {
		rxbuf = &rxr->rx_buffers[i];
		/* No hdr mbuf used with header split off */
		if (rxr->hdr_split == FALSE)
			goto no_split;
		if (rxbuf->m_head == NULL) {
			mh = m_gethdr(M_DONTWAIT, MT_DATA);
			if (mh == NULL)
				goto update;
		} else
			mh = rxbuf->m_head;

		mh->m_pkthdr.len = mh->m_len = MHLEN;
		mh->m_len = MHLEN;
		mh->m_flags |= M_PKTHDR;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->htag,
		    rxbuf->hmap, mh, hseg, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("Refresh mbufs: hdr dmamap load"
			    " failure - %d\n", error);
			m_free(mh);
			rxbuf->m_head = NULL;
			goto update;
		}
		rxbuf->m_head = mh;
		bus_dmamap_sync(rxr->htag, rxbuf->hmap,
		    BUS_DMASYNC_PREREAD);
		rxr->rx_base[i].read.hdr_addr =
		    htole64(hseg[0].ds_addr);
no_split:
		if (rxbuf->m_pack == NULL) {
			mp = m_getjcl(M_DONTWAIT, MT_DATA,
			    M_PKTHDR, adapter->rx_mbuf_sz);
			if (mp == NULL)
				goto update;
		} else
			mp = rxbuf->m_pack;

		mp->m_pkthdr.len = mp->m_len = adapter->rx_mbuf_sz;
		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->ptag,
		    rxbuf->pmap, mp, pseg, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("Refresh mbufs: payload dmamap load"
			    " failure - %d\n", error);
			m_free(mp);
			rxbuf->m_pack = NULL;
			goto update;
		}
		rxbuf->m_pack = mp;
		bus_dmamap_sync(rxr->ptag, rxbuf->pmap,
		    BUS_DMASYNC_PREREAD);
		rxr->rx_base[i].read.pkt_addr =
		    htole64(pseg[0].ds_addr);

		cleaned = i;
		/* Calculate next index */
		if (++i == adapter->num_rx_desc)
			i = 0;
		/* This is the work marker for refresh */
		rxr->next_to_refresh = i;
	}
update:
	if (cleaned != -1) /* If we refreshed some, bump tail */
		E1000_WRITE_REG(&adapter->hw,
		    E1000_RDT(rxr->me), cleaned);
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
igb_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	device_t 		dev = adapter->dev;
	struct igb_rx_buf	*rxbuf;
	int             	i, bsize, error;

	bsize = sizeof(struct igb_rx_buf) * adapter->num_rx_desc;
	if (!(rxr->rx_buffers =
	    (struct igb_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),
				   1, 0,		/* alignment, bounds */
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

	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),
				   1, 0,		/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MJUM9BYTES,		/* maxsize */
				   1,			/* nsegments */
				   MJUM9BYTES,		/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->ptag))) {
		device_printf(dev, "Unable to create RX payload DMA tag\n");
		goto fail;
	}

	for (i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->htag,
		    BUS_DMA_NOWAIT, &rxbuf->hmap);
		if (error) {
			device_printf(dev,
			    "Unable to create RX head DMA maps\n");
			goto fail;
		}
		error = bus_dmamap_create(rxr->ptag,
		    BUS_DMA_NOWAIT, &rxbuf->pmap);
		if (error) {
			device_printf(dev,
			    "Unable to create RX packet DMA maps\n");
			goto fail;
		}
	}

	return (0);

fail:
	/* Frees all, but can handle partial completion */
	igb_free_receive_structures(adapter);
	return (error);
}


static void
igb_free_receive_ring(struct rx_ring *rxr)
{
	struct	adapter		*adapter;
	struct igb_rx_buf	*rxbuf;
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
igb_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter		*adapter;
	struct  ifnet		*ifp;
	device_t		dev;
	struct igb_rx_buf	*rxbuf;
	bus_dma_segment_t	pseg[1], hseg[1];
	struct lro_ctrl		*lro = &rxr->lro;
	int			rsize, nsegs, error = 0;

	adapter = rxr->adapter;
	dev = adapter->dev;
	ifp = adapter->ifp;

	/* Clear the ring contents */
	IGB_RX_LOCK(rxr);
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union e1000_adv_rx_desc), IGB_DBA_ALIGN);
	bzero((void *)rxr->rx_base, rsize);

	/*
	** Free current RX buffer structures and their mbufs
	*/
	igb_free_receive_ring(rxr);

	/* Configure for header split? */
	if (igb_header_split)
		rxr->hdr_split = TRUE;

        /* Now replenish the ring mbufs */
	for (int j = 0; j < adapter->num_rx_desc; ++j) {
		struct mbuf	*mh, *mp;

		rxbuf = &rxr->rx_buffers[j];
		if (rxr->hdr_split == FALSE)
			goto skip_head;

		/* First the header */
		rxbuf->m_head = m_gethdr(M_DONTWAIT, MT_DATA);
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
		rxbuf->m_pack = m_getjcl(M_DONTWAIT, MT_DATA,
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

	rxr->fmp = NULL;
	rxr->lmp = NULL;
	rxr->discard = FALSE;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	** Now set up the LRO interface, we
	** also only do head split when LRO
	** is enabled, since so often they
	** are undesireable in similar setups.
	*/
	if (ifp->if_capenable & IFCAP_LRO) {
		error = tcp_lro_init(lro);
		if (error) {
			device_printf(dev, "LRO Initialization failed!\n");
			goto fail;
		}
		INIT_DEBUGOUT("RX LRO Initialized\n");
		rxr->lro_enabled = TRUE;
		lro->ifp = adapter->ifp;
	}

	IGB_RX_UNLOCK(rxr);
	return (0);

fail:
	igb_free_receive_ring(rxr);
	IGB_RX_UNLOCK(rxr);
	return (error);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
igb_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int i;

	for (i = 0; i < adapter->num_queues; i++, rxr++)
		if (igb_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. 'i' is the endpoint.
	 */
	for (int j = 0; j > i; ++j) {
		rxr = &adapter->rx_rings[i];
		IGB_RX_LOCK(rxr);
		igb_free_receive_ring(rxr);
		IGB_RX_UNLOCK(rxr);
	}

	return (ENOBUFS);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
static void
igb_initialize_receive_units(struct adapter *adapter)
{
	struct rx_ring	*rxr = adapter->rx_rings;
	struct ifnet	*ifp = adapter->ifp;
	struct e1000_hw *hw = &adapter->hw;
	u32		rctl, rxcsum, psize, srrctl = 0;

	INIT_DEBUGOUT("igb_initialize_receive_unit: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	/*
	** Set up for header split
	*/
	if (rxr->hdr_split) {
		/* Use a standard mbuf for the header */
		srrctl |= IGB_HDR_BUF << E1000_SRRCTL_BSIZEHDRSIZE_SHIFT;
		srrctl |= E1000_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
	} else
		srrctl |= E1000_SRRCTL_DESCTYPE_ADV_ONEBUF;

	/*
	** Set up for jumbo frames
	*/
	if (ifp->if_mtu > ETHERMTU) {
		rctl |= E1000_RCTL_LPE;
		if (adapter->rx_mbuf_sz == MJUMPAGESIZE) {
			srrctl |= 4096 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
			rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX;
		} else if (adapter->rx_mbuf_sz > MJUMPAGESIZE) {
			srrctl |= 8192 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
			rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX;
		}
		/* Set maximum packet len */
		psize = adapter->max_frame_size;
		/* are we on a vlan? */
		if (adapter->ifp->if_vlantrunk != NULL)
			psize += VLAN_TAG_SIZE;
		E1000_WRITE_REG(&adapter->hw, E1000_RLPML, psize);
	} else {
		rctl &= ~E1000_RCTL_LPE;
		srrctl |= 2048 >> E1000_SRRCTL_BSIZEPKT_SHIFT;
		rctl |= E1000_RCTL_SZ_2048;
	}

	/* Setup the Base and Length of the Rx Descriptor Rings */
	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 bus_addr = rxr->rxdma.dma_paddr;
		u32 rxdctl;

		E1000_WRITE_REG(hw, E1000_RDLEN(i),
		    adapter->num_rx_desc * sizeof(struct e1000_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i),
		    (uint32_t)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i),
		    (uint32_t)bus_addr);
		E1000_WRITE_REG(hw, E1000_SRRCTL(i), srrctl);
		/* Enable this Queue */
		rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(i));
		rxdctl |= E1000_RXDCTL_QUEUE_ENABLE;
		rxdctl &= 0xFFF00000;
		rxdctl |= IGB_RX_PTHRESH;
		rxdctl |= IGB_RX_HTHRESH << 8;
		rxdctl |= IGB_RX_WTHRESH << 16;
		E1000_WRITE_REG(hw, E1000_RXDCTL(i), rxdctl);
	}

	/*
	** Setup for RX MultiQueue
	*/
	rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
	if (adapter->num_queues >1) {
		u32 random[10], mrqc, shift = 0;
		union igb_reta {
			u32 dword;
			u8  bytes[4];
		} reta;

		arc4rand(&random, sizeof(random), 0);
		if (adapter->hw.mac.type == e1000_82575)
			shift = 6;
		/* Warning FM follows */
		for (int i = 0; i < 128; i++) {
			reta.bytes[i & 3] =
			    (i % adapter->num_queues) << shift;
			if ((i & 3) == 3)
				E1000_WRITE_REG(hw,
				    E1000_RETA(i >> 2), reta.dword);
		}
		/* Now fill in hash table */
		mrqc = E1000_MRQC_ENABLE_RSS_4Q;
		for (int i = 0; i < 10; i++)
			E1000_WRITE_REG_ARRAY(hw,
			    E1000_RSSRK(0), i, random[i]);

		mrqc |= (E1000_MRQC_RSS_FIELD_IPV4 |
		    E1000_MRQC_RSS_FIELD_IPV4_TCP);
		mrqc |= (E1000_MRQC_RSS_FIELD_IPV6 |
		    E1000_MRQC_RSS_FIELD_IPV6_TCP);
		mrqc |=( E1000_MRQC_RSS_FIELD_IPV4_UDP |
		    E1000_MRQC_RSS_FIELD_IPV6_UDP);
		mrqc |=( E1000_MRQC_RSS_FIELD_IPV6_UDP_EX |
		    E1000_MRQC_RSS_FIELD_IPV6_TCP_EX);

		E1000_WRITE_REG(hw, E1000_MRQC, mrqc);

		/*
		** NOTE: Receive Full-Packet Checksum Offload 
		** is mutually exclusive with Multiqueue. However
		** this is not the same as TCP/IP checksums which
		** still work.
		*/
		rxcsum |= E1000_RXCSUM_PCSD;
#if __FreeBSD_version >= 800000
		/* For SCTP Offload */
		if ((hw->mac.type == e1000_82576)
		    && (ifp->if_capenable & IFCAP_RXCSUM))
			rxcsum |= E1000_RXCSUM_CRCOFL;
#endif
	} else {
		/* Non RSS setup */
		if (ifp->if_capenable & IFCAP_RXCSUM) {
			rxcsum |= E1000_RXCSUM_IPPCSE;
#if __FreeBSD_version >= 800000
			if (adapter->hw.mac.type == e1000_82576)
				rxcsum |= E1000_RXCSUM_CRCOFL;
#endif
		} else
			rxcsum &= ~E1000_RXCSUM_TUOFL;
	}
	E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);

	/* Setup the Receive Control Register */
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		   E1000_RCTL_RDMTS_HALF |
		   (hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);
	/* Strip CRC bytes. */
	rctl |= E1000_RCTL_SECRC;
	/* Make sure VLAN Filters are off */
	rctl &= ~E1000_RCTL_VFE;
	/* Don't store bad packets */
	rctl &= ~E1000_RCTL_SBP;

	/* Enable Receives */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

	/*
	 * Setup the HW Rx Head and Tail Descriptor Pointers
	 *   - needs to be after enable
	 */
	for (int i = 0; i < adapter->num_queues; i++) {
		E1000_WRITE_REG(hw, E1000_RDH(i), 0);
		E1000_WRITE_REG(hw, E1000_RDT(i),
		     adapter->num_rx_desc - 1);
	}
	return;
}

/*********************************************************************
 *
 *  Free receive rings.
 *
 **********************************************************************/
static void
igb_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		struct lro_ctrl	*lro = &rxr->lro;
		igb_free_receive_buffers(rxr);
		tcp_lro_free(lro);
		igb_dma_free(adapter, &rxr->rxdma);
	}

	free(adapter->rx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free receive ring data structures.
 *
 **********************************************************************/
static void
igb_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct igb_rx_buf	*rxbuf;
	int i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	/* Cleanup any existing buffers */
	if (rxr->rx_buffers != NULL) {
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
}

static __inline void
igb_rx_discard(struct rx_ring *rxr, int i)
{
	struct igb_rx_buf	*rbuf;

	rbuf = &rxr->rx_buffers[i];

	/* Partially received? Free the chain */
	if (rxr->fmp != NULL) {
		rxr->fmp->m_flags |= M_PKTHDR;
		m_freem(rxr->fmp);
		rxr->fmp = NULL;
		rxr->lmp = NULL;
	}

	/*
	** With advanced descriptors the writeback
	** clobbers the buffer addrs, so its easier
	** to just free the existing mbufs and take
	** the normal refresh path to get new buffers
	** and mapping.
	*/
	if (rbuf->m_head) {
		m_free(rbuf->m_head);
		rbuf->m_head = NULL;
	}

	if (rbuf->m_pack) {
		m_free(rbuf->m_pack);
		rbuf->m_pack = NULL;
	}

	return;
}

static __inline void
igb_rx_input(struct rx_ring *rxr, struct ifnet *ifp, struct mbuf *m, u32 ptype)
{

	/*
	 * ATM LRO is only for IPv4/TCP packets and TCP checksum of the packet
	 * should be computed by hardware. Also it should not have VLAN tag in
	 * ethernet header.
	 */
	if (rxr->lro_enabled &&
	    (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0 &&
	    (ptype & E1000_RXDADV_PKTTYPE_ETQF) == 0 &&
	    (ptype & (E1000_RXDADV_PKTTYPE_IPV4 | E1000_RXDADV_PKTTYPE_TCP)) ==
	    (E1000_RXDADV_PKTTYPE_IPV4 | E1000_RXDADV_PKTTYPE_TCP) &&
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
	IGB_RX_UNLOCK(rxr);
	(*ifp->if_input)(ifp, m);
	IGB_RX_LOCK(rxr);
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
 *  Return TRUE if more to clean, FALSE otherwise
 *********************************************************************/
static bool
igb_rxeof(struct igb_queue *que, int count, int *done)
{
	struct adapter		*adapter = que->adapter;
	struct rx_ring		*rxr = que->rxr;
	struct ifnet		*ifp = adapter->ifp;
	struct lro_ctrl		*lro = &rxr->lro;
	struct lro_entry	*queued;
	int			i, processed = 0, rxdone = 0;
	u32			ptype, staterr = 0;
	union e1000_adv_rx_desc	*cur;

	IGB_RX_LOCK(rxr);
	/* Sync the ring. */
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	/* Main clean loop */
	for (i = rxr->next_to_check; count != 0;) {
		struct mbuf		*sendmp, *mh, *mp;
		struct igb_rx_buf	*rxbuf;
		u16			hlen, plen, hdr, vtag;
		bool			eop = FALSE;
 
		cur = &rxr->rx_base[i];
		staterr = le32toh(cur->wb.upper.status_error);
		if ((staterr & E1000_RXD_STAT_DD) == 0)
			break;
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;
		count--;
		sendmp = mh = mp = NULL;
		cur->wb.upper.status_error = 0;
		rxbuf = &rxr->rx_buffers[i];
		plen = le16toh(cur->wb.upper.length);
		ptype = le32toh(cur->wb.lower.lo_dword.data) & IGB_PKTTYPE_MASK;
		vtag = le16toh(cur->wb.upper.vlan);
		hdr = le16toh(cur->wb.lower.lo_dword.hs_rss.hdr_info);
		eop = ((staterr & E1000_RXD_STAT_EOP) == E1000_RXD_STAT_EOP);

		/* Make sure all segments of a bad packet are discarded */
		if (((staterr & E1000_RXDEXT_ERR_FRAME_ERR_MASK) != 0) ||
		    (rxr->discard)) {
			ifp->if_ierrors++;
			++rxr->rx_discarded;
			if (!eop) /* Catch subsequent segs */
				rxr->discard = TRUE;
			else
				rxr->discard = FALSE;
			igb_rx_discard(rxr, i);
			goto next_desc;
		}

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
		if (rxr->hdr_split && rxr->fmp == NULL) {
			hlen = (hdr & E1000_RXDADV_HDRBUFLEN_MASK) >>
			    E1000_RXDADV_HDRBUFLEN_SHIFT;
			if (hlen > IGB_HDR_BUF)
				hlen = IGB_HDR_BUF;
			mh = rxr->rx_buffers[i].m_head;
			mh->m_len = hlen;
			/* clear buf pointer for refresh */
			rxbuf->m_head = NULL;
			/*
			** Get the payload length, this
			** could be zero if its a small
			** packet.
			*/
			if (plen > 0) {
				mp = rxr->rx_buffers[i].m_pack;
				mp->m_len = plen;
				mh->m_next = mp;
				/* clear buf pointer */
				rxbuf->m_pack = NULL;
				rxr->rx_split_packets++;
			}
		} else {
			/*
			** Either no header split, or a
			** secondary piece of a fragmented
			** split packet.
			*/
			mh = rxr->rx_buffers[i].m_pack;
			mh->m_len = plen;
			/* clear buf info for refresh */
			rxbuf->m_pack = NULL;
		}

		++processed; /* So we know when to refresh */

		/* Initial frame - setup */
		if (rxr->fmp == NULL) {
			mh->m_pkthdr.len = mh->m_len;
			/* Save the head of the chain */
			rxr->fmp = mh;
			rxr->lmp = mh;
			if (mp != NULL) {
				/* Add payload if split */
				mh->m_pkthdr.len += mp->m_len;
				rxr->lmp = mh->m_next;
			}
		} else {
			/* Chain mbuf's together */
			rxr->lmp->m_next = mh;
			rxr->lmp = rxr->lmp->m_next;
			rxr->fmp->m_pkthdr.len += mh->m_len;
		}

		if (eop) {
			rxr->fmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			rxr->rx_packets++;
			/* capture data for AIM */
			rxr->packets++;
			rxr->bytes += rxr->fmp->m_pkthdr.len;
			rxr->rx_bytes += rxr->fmp->m_pkthdr.len;

			if ((ifp->if_capenable & IFCAP_RXCSUM) != 0)
				igb_rx_checksum(staterr, rxr->fmp, ptype);

			if ((ifp->if_capenable & IFCAP_VLAN_HWTAGGING) != 0 &&
			    (staterr & E1000_RXD_STAT_VP) != 0) {
				rxr->fmp->m_pkthdr.ether_vtag = vtag;
				rxr->fmp->m_flags |= M_VLANTAG;
			}
#if __FreeBSD_version >= 800000
			rxr->fmp->m_pkthdr.flowid = que->msix;
			rxr->fmp->m_flags |= M_FLOWID;
#endif
			sendmp = rxr->fmp;
			/* Make sure to set M_PKTHDR. */
			sendmp->m_flags |= M_PKTHDR;
			rxr->fmp = NULL;
			rxr->lmp = NULL;
		}

next_desc:
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;
		/*
		** Send to the stack or LRO
		*/
		if (sendmp != NULL) {
			rxr->next_to_check = i;
			igb_rx_input(rxr, ifp, sendmp, ptype);
			i = rxr->next_to_check;
			rxdone++;
		}

		/* Every 8 descriptors we go to refresh mbufs */
		if (processed == 8) {
                        igb_refresh_mbufs(rxr, i);
                        processed = 0;
		}
	}

	/* Catch any remainders */
	if (processed != 0) {
		igb_refresh_mbufs(rxr, i);
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

	IGB_RX_UNLOCK(rxr);

	if (done != NULL)
		*done = rxdone;

	/*
	** We still have cleaning to do?
	** Schedule another interrupt if so.
	*/
	if ((staterr & E1000_RXD_STAT_DD) != 0)
		return (TRUE);

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
igb_rx_checksum(u32 staterr, struct mbuf *mp, u32 ptype)
{
	u16 status = (u16)staterr;
	u8  errors = (u8) (staterr >> 24);
	int sctp;

	/* Ignore Checksum bit is set */
	if (status & E1000_RXD_STAT_IXSM) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}

	if ((ptype & E1000_RXDADV_PKTTYPE_ETQF) == 0 &&
	    (ptype & E1000_RXDADV_PKTTYPE_SCTP) != 0)
		sctp = 1;
	else
		sctp = 0;
	if (status & E1000_RXD_STAT_IPCS) {
		/* Did it pass? */
		if (!(errors & E1000_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;
		} else
			mp->m_pkthdr.csum_flags = 0;
	}

	if (status & (E1000_RXD_STAT_TCPCS | E1000_RXD_STAT_UDPCS)) {
		u16 type = (CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
#if __FreeBSD_version >= 800000
		if (sctp) /* reassign */
			type = CSUM_SCTP_VALID;
#endif
		/* Did it pass? */
		if (!(errors & E1000_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |= type;
			if (sctp == 0)
				mp->m_pkthdr.csum_data = htons(0xffff);
		}
	}
	return;
}

/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
igb_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, bit;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))       /* Invalid */
                return;

	IGB_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	/* Re-init to load the changes */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
		igb_init_locked(adapter);
	IGB_CORE_UNLOCK(adapter);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
igb_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))       /* Invalid */
                return;

	IGB_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
		igb_init_locked(adapter);
	IGB_CORE_UNLOCK(adapter);
}

static void
igb_setup_vlan_hw_support(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32             reg;

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
	for (int i = 0; i < IGB_VFTA_SIZE; i++)
                if (adapter->shadow_vfta[i] != 0) {
			if (hw->mac.type == e1000_vfadapt)
				e1000_vfta_set_vf(hw,
				    adapter->shadow_vfta[i], TRUE);
			else
				E1000_WRITE_REG_ARRAY(hw, E1000_VFTA,
                           	 i, adapter->shadow_vfta[i]);
		}

	if (hw->mac.type == e1000_vfadapt)
		e1000_rlpml_set_vf(hw,
		    adapter->max_frame_size + VLAN_TAG_SIZE);
	else {
		reg = E1000_READ_REG(hw, E1000_CTRL);
		reg |= E1000_CTRL_VME;
		E1000_WRITE_REG(hw, E1000_CTRL, reg);

		/* Enable the Filter Table */
		reg = E1000_READ_REG(hw, E1000_RCTL);
		reg &= ~E1000_RCTL_CFIEN;
		reg |= E1000_RCTL_VFE;
		E1000_WRITE_REG(hw, E1000_RCTL, reg);

		/* Update the frame size */
		E1000_WRITE_REG(&adapter->hw, E1000_RLPML,
		    adapter->max_frame_size + VLAN_TAG_SIZE);
	}
}

static void
igb_enable_intr(struct adapter *adapter)
{
	/* With RSS set up what to auto clear */
	if (adapter->msix_mem) {
		E1000_WRITE_REG(&adapter->hw, E1000_EIAC,
		    adapter->eims_mask);
		E1000_WRITE_REG(&adapter->hw, E1000_EIAM,
		    adapter->eims_mask);
		E1000_WRITE_REG(&adapter->hw, E1000_EIMS,
		    adapter->eims_mask);
		E1000_WRITE_REG(&adapter->hw, E1000_IMS,
		    E1000_IMS_LSC);
	} else {
		E1000_WRITE_REG(&adapter->hw, E1000_IMS,
		    IMS_ENABLE_MASK);
	}
	E1000_WRITE_FLUSH(&adapter->hw);

	return;
}

static void
igb_disable_intr(struct adapter *adapter)
{
	if (adapter->msix_mem) {
		E1000_WRITE_REG(&adapter->hw, E1000_EIMC, ~0);
		E1000_WRITE_REG(&adapter->hw, E1000_EIAC, 0);
	} 
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, ~0);
	E1000_WRITE_FLUSH(&adapter->hw);
	return;
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features 
 */
static void
igb_init_manageability(struct adapter *adapter)
{
	if (adapter->has_manage) {
		int manc2h = E1000_READ_REG(&adapter->hw, E1000_MANC2H);
		int manc = E1000_READ_REG(&adapter->hw, E1000_MANC);

		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);

                /* enable receiving management packets to the host */
		manc |= E1000_MANC_EN_MNG2HOST;
		manc2h |= 1 << 5;  /* Mng Port 623 */
		manc2h |= 1 << 6;  /* Mng Port 664 */
		E1000_WRITE_REG(&adapter->hw, E1000_MANC2H, manc2h);
		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * Give control back to hardware management
 * controller if there is one.
 */
static void
igb_release_manageability(struct adapter *adapter)
{
	if (adapter->has_manage) {
		int manc = E1000_READ_REG(&adapter->hw, E1000_MANC);

		/* re-enable hardware interception of ARP */
		manc |= E1000_MANC_ARP_EN;
		manc &= ~E1000_MANC_EN_MNG2HOST;

		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * igb_get_hw_control sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is loaded. 
 *
 */
static void
igb_get_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext;

	if (adapter->hw.mac.type == e1000_vfadapt)
		return;

	/* Let firmware know the driver has taken over */
	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
}

/*
 * igb_release_hw_control resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 *
 */
static void
igb_release_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext;

	if (adapter->hw.mac.type == e1000_vfadapt)
		return;

	/* Let firmware taken over control of h/w */
	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
}

static int
igb_is_valid_ether_addr(uint8_t *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (FALSE);
	}

	return (TRUE);
}


/*
 * Enable PCI Wake On Lan capability
 */
static void
igb_enable_wakeup(device_t dev)
{
	u16     cap, status;
	u8      id;

	/* First find the capabilities pointer*/
	cap = pci_read_config(dev, PCIR_CAP_PTR, 2);
	/* Read the PM Capabilities */
	id = pci_read_config(dev, cap, 1);
	if (id != PCIY_PMG)     /* Something wrong */
		return;
	/* OK, we have the power capabilities, so
	   now get the status register */
	cap += PCIR_POWER_STATUS;
	status = pci_read_config(dev, cap, 2);
	status |= PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE;
	pci_write_config(dev, cap, status, 2);
	return;
}

static void
igb_led_func(void *arg, int onoff)
{
	struct adapter	*adapter = arg;

	IGB_CORE_LOCK(adapter);
	if (onoff) {
		e1000_setup_led(&adapter->hw);
		e1000_led_on(&adapter->hw);
	} else {
		e1000_led_off(&adapter->hw);
		e1000_cleanup_led(&adapter->hw);
	}
	IGB_CORE_UNLOCK(adapter);
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
igb_update_stats_counters(struct adapter *adapter)
{
	struct ifnet		*ifp;
        struct e1000_hw		*hw = &adapter->hw;
	struct e1000_hw_stats	*stats;

	/* 
	** The virtual function adapter has only a
	** small controlled set of stats, do only 
	** those and return.
	*/
	if (adapter->hw.mac.type == e1000_vfadapt) {
		igb_update_vf_stats_counters(adapter);
		return;
	}

	stats = (struct e1000_hw_stats	*)adapter->stats;

	if(adapter->hw.phy.media_type == e1000_media_type_copper ||
	   (E1000_READ_REG(hw, E1000_STATUS) & E1000_STATUS_LU)) {
		stats->symerrs +=
		    E1000_READ_REG(hw,E1000_SYMERRS);
		stats->sec += E1000_READ_REG(hw, E1000_SEC);
	}

	stats->crcerrs += E1000_READ_REG(hw, E1000_CRCERRS);
	stats->mpc += E1000_READ_REG(hw, E1000_MPC);
	stats->scc += E1000_READ_REG(hw, E1000_SCC);
	stats->ecol += E1000_READ_REG(hw, E1000_ECOL);

	stats->mcc += E1000_READ_REG(hw, E1000_MCC);
	stats->latecol += E1000_READ_REG(hw, E1000_LATECOL);
	stats->colc += E1000_READ_REG(hw, E1000_COLC);
	stats->dc += E1000_READ_REG(hw, E1000_DC);
	stats->rlec += E1000_READ_REG(hw, E1000_RLEC);
	stats->xonrxc += E1000_READ_REG(hw, E1000_XONRXC);
	stats->xontxc += E1000_READ_REG(hw, E1000_XONTXC);
	/*
	** For watchdog management we need to know if we have been
	** paused during the last interval, so capture that here.
	*/ 
        adapter->pause_frames = E1000_READ_REG(&adapter->hw, E1000_XOFFRXC);
        stats->xoffrxc += adapter->pause_frames;
	stats->xofftxc += E1000_READ_REG(hw, E1000_XOFFTXC);
	stats->fcruc += E1000_READ_REG(hw, E1000_FCRUC);
	stats->prc64 += E1000_READ_REG(hw, E1000_PRC64);
	stats->prc127 += E1000_READ_REG(hw, E1000_PRC127);
	stats->prc255 += E1000_READ_REG(hw, E1000_PRC255);
	stats->prc511 += E1000_READ_REG(hw, E1000_PRC511);
	stats->prc1023 += E1000_READ_REG(hw, E1000_PRC1023);
	stats->prc1522 += E1000_READ_REG(hw, E1000_PRC1522);
	stats->gprc += E1000_READ_REG(hw, E1000_GPRC);
	stats->bprc += E1000_READ_REG(hw, E1000_BPRC);
	stats->mprc += E1000_READ_REG(hw, E1000_MPRC);
	stats->gptc += E1000_READ_REG(hw, E1000_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	stats->gorc += E1000_READ_REG(hw, E1000_GORCL) +
	    ((u64)E1000_READ_REG(hw, E1000_GORCH) << 32);
	stats->gotc += E1000_READ_REG(hw, E1000_GOTCL) +
	    ((u64)E1000_READ_REG(hw, E1000_GOTCH) << 32);

	stats->rnbc += E1000_READ_REG(hw, E1000_RNBC);
	stats->ruc += E1000_READ_REG(hw, E1000_RUC);
	stats->rfc += E1000_READ_REG(hw, E1000_RFC);
	stats->roc += E1000_READ_REG(hw, E1000_ROC);
	stats->rjc += E1000_READ_REG(hw, E1000_RJC);

	stats->tor += E1000_READ_REG(hw, E1000_TORH);
	stats->tot += E1000_READ_REG(hw, E1000_TOTH);

	stats->tpr += E1000_READ_REG(hw, E1000_TPR);
	stats->tpt += E1000_READ_REG(hw, E1000_TPT);
	stats->ptc64 += E1000_READ_REG(hw, E1000_PTC64);
	stats->ptc127 += E1000_READ_REG(hw, E1000_PTC127);
	stats->ptc255 += E1000_READ_REG(hw, E1000_PTC255);
	stats->ptc511 += E1000_READ_REG(hw, E1000_PTC511);
	stats->ptc1023 += E1000_READ_REG(hw, E1000_PTC1023);
	stats->ptc1522 += E1000_READ_REG(hw, E1000_PTC1522);
	stats->mptc += E1000_READ_REG(hw, E1000_MPTC);
	stats->bptc += E1000_READ_REG(hw, E1000_BPTC);

	/* Interrupt Counts */

	stats->iac += E1000_READ_REG(hw, E1000_IAC);
	stats->icrxptc += E1000_READ_REG(hw, E1000_ICRXPTC);
	stats->icrxatc += E1000_READ_REG(hw, E1000_ICRXATC);
	stats->ictxptc += E1000_READ_REG(hw, E1000_ICTXPTC);
	stats->ictxatc += E1000_READ_REG(hw, E1000_ICTXATC);
	stats->ictxqec += E1000_READ_REG(hw, E1000_ICTXQEC);
	stats->ictxqmtc += E1000_READ_REG(hw, E1000_ICTXQMTC);
	stats->icrxdmtc += E1000_READ_REG(hw, E1000_ICRXDMTC);
	stats->icrxoc += E1000_READ_REG(hw, E1000_ICRXOC);

	/* Host to Card Statistics */

	stats->cbtmpc += E1000_READ_REG(hw, E1000_CBTMPC);
	stats->htdpmc += E1000_READ_REG(hw, E1000_HTDPMC);
	stats->cbrdpc += E1000_READ_REG(hw, E1000_CBRDPC);
	stats->cbrmpc += E1000_READ_REG(hw, E1000_CBRMPC);
	stats->rpthc += E1000_READ_REG(hw, E1000_RPTHC);
	stats->hgptc += E1000_READ_REG(hw, E1000_HGPTC);
	stats->htcbdpc += E1000_READ_REG(hw, E1000_HTCBDPC);
	stats->hgorc += (E1000_READ_REG(hw, E1000_HGORCL) +
	    ((u64)E1000_READ_REG(hw, E1000_HGORCH) << 32));
	stats->hgotc += (E1000_READ_REG(hw, E1000_HGOTCL) +
	    ((u64)E1000_READ_REG(hw, E1000_HGOTCH) << 32));
	stats->lenerrs += E1000_READ_REG(hw, E1000_LENERRS);
	stats->scvpc += E1000_READ_REG(hw, E1000_SCVPC);
	stats->hrmpc += E1000_READ_REG(hw, E1000_HRMPC);

	stats->algnerrc += E1000_READ_REG(hw, E1000_ALGNERRC);
	stats->rxerrc += E1000_READ_REG(hw, E1000_RXERRC);
	stats->tncrs += E1000_READ_REG(hw, E1000_TNCRS);
	stats->cexterr += E1000_READ_REG(hw, E1000_CEXTERR);
	stats->tsctc += E1000_READ_REG(hw, E1000_TSCTC);
	stats->tsctfc += E1000_READ_REG(hw, E1000_TSCTFC);

	ifp = adapter->ifp;
	ifp->if_collisions = stats->colc;

	/* Rx Errors */
	ifp->if_ierrors = adapter->dropped_pkts + stats->rxerrc +
	    stats->crcerrs + stats->algnerrc +
	    stats->ruc + stats->roc + stats->mpc + stats->cexterr;

	/* Tx Errors */
	ifp->if_oerrors = stats->ecol +
	    stats->latecol + adapter->watchdog_events;

	/* Driver specific counters */
	adapter->device_control = E1000_READ_REG(hw, E1000_CTRL);
	adapter->rx_control = E1000_READ_REG(hw, E1000_RCTL);
	adapter->int_mask = E1000_READ_REG(hw, E1000_IMS);
	adapter->eint_mask = E1000_READ_REG(hw, E1000_EIMS);
	adapter->packet_buf_alloc_tx =
	    ((E1000_READ_REG(hw, E1000_PBA) & 0xffff0000) >> 16);
	adapter->packet_buf_alloc_rx =
	    (E1000_READ_REG(hw, E1000_PBA) & 0xffff);
}


/**********************************************************************
 *
 *  Initialize the VF board statistics counters.
 *
 **********************************************************************/
static void
igb_vf_init_stats(struct adapter *adapter)
{
        struct e1000_hw *hw = &adapter->hw;
	struct e1000_vf_stats	*stats;

	stats = (struct e1000_vf_stats	*)adapter->stats;
	if (stats == NULL)
		return;
        stats->last_gprc = E1000_READ_REG(hw, E1000_VFGPRC);
        stats->last_gorc = E1000_READ_REG(hw, E1000_VFGORC);
        stats->last_gptc = E1000_READ_REG(hw, E1000_VFGPTC);
        stats->last_gotc = E1000_READ_REG(hw, E1000_VFGOTC);
        stats->last_mprc = E1000_READ_REG(hw, E1000_VFMPRC);
}
 
/**********************************************************************
 *
 *  Update the VF board statistics counters.
 *
 **********************************************************************/
static void
igb_update_vf_stats_counters(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct e1000_vf_stats	*stats;

	if (adapter->link_speed == 0)
		return;

	stats = (struct e1000_vf_stats	*)adapter->stats;

	UPDATE_VF_REG(E1000_VFGPRC,
	    stats->last_gprc, stats->gprc);
	UPDATE_VF_REG(E1000_VFGORC,
	    stats->last_gorc, stats->gorc);
	UPDATE_VF_REG(E1000_VFGPTC,
	    stats->last_gptc, stats->gptc);
	UPDATE_VF_REG(E1000_VFGOTC,
	    stats->last_gotc, stats->gotc);
	UPDATE_VF_REG(E1000_VFMPRC,
	    stats->last_mprc, stats->mprc);
}

/* Export a single 32-bit register via a read-only sysctl. */
static int
igb_sysctl_reg_handler(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	u_int val;

	adapter = oidp->oid_arg1;
	val = E1000_READ_REG(&adapter->hw, oidp->oid_arg2);
	return (sysctl_handle_int(oidp, &val, 0, req));
}

/*
**  Tuneable interrupt rate handler
*/
static int
igb_sysctl_interrupt_rate_handler(SYSCTL_HANDLER_ARGS)
{
	struct igb_queue	*que = ((struct igb_queue *)oidp->oid_arg1);
	int			error;
	u32			reg, usec, rate;
                        
	reg = E1000_READ_REG(&que->adapter->hw, E1000_EITR(que->msix));
	usec = ((reg & 0x7FFC) >> 2);
	if (usec > 0)
		rate = 1000000 / usec;
	else
		rate = 0;
	error = sysctl_handle_int(oidp, &rate, 0, req);
	if (error || !req->newptr)
		return error;
	return 0;
}

/*
 * Add sysctl variables, one per statistic, to the system.
 */
static void
igb_add_hw_stats(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct e1000_hw_stats *stats = adapter->stats;

	struct sysctl_oid *stat_node, *queue_node, *int_node, *host_node;
	struct sysctl_oid_list *stat_list, *queue_list, *int_list, *host_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];

	/* Driver Statistics */
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "link_irq", 
			CTLFLAG_RD, &adapter->link_irq, 0,
			"Link MSIX IRQ Handled");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped", 
			CTLFLAG_RD, &adapter->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "tx_dma_fail", 
			CTLFLAG_RD, &adapter->no_tx_dma_setup,
			"Driver tx dma failure in xmit");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_overruns",
			CTLFLAG_RD, &adapter->rx_overruns,
			"RX overruns");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_timeouts",
			CTLFLAG_RD, &adapter->watchdog_events,
			"Watchdog timeouts");

	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "device_control", 
			CTLFLAG_RD, &adapter->device_control,
			"Device Control Register");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_control", 
			CTLFLAG_RD, &adapter->rx_control,
			"Receiver Control Register");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "interrupt_mask", 
			CTLFLAG_RD, &adapter->int_mask,
			"Interrupt Mask");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "extended_int_mask", 
			CTLFLAG_RD, &adapter->eint_mask,
			"Extended Interrupt Mask");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "tx_buf_alloc", 
			CTLFLAG_RD, &adapter->packet_buf_alloc_tx,
			"Transmit Buffer Packet Allocation");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_buf_alloc", 
			CTLFLAG_RD, &adapter->packet_buf_alloc_rx,
			"Receive Buffer Packet Allocation");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_high_water",
			CTLFLAG_RD, &adapter->hw.fc.high_water, 0,
			"Flow Control High Watermark");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_low_water", 
			CTLFLAG_RD, &adapter->hw.fc.low_water, 0,
			"Flow Control Low Watermark");

	for (int i = 0; i < adapter->num_queues; i++, rxr++, txr++) {
		struct lro_ctrl *lro = &rxr->lro;

		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
					    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "interrupt_rate", 
				CTLFLAG_RD, &adapter->queues[i],
				sizeof(&adapter->queues[i]),
				igb_sysctl_interrupt_rate_handler,
				"IU", "Interrupt Rate");

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_head", 
				CTLFLAG_RD, adapter, E1000_TDH(txr->me),
				igb_sysctl_reg_handler, "IU",
 				"Transmit Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_tail", 
				CTLFLAG_RD, adapter, E1000_TDT(txr->me),
				igb_sysctl_reg_handler, "IU",
 				"Transmit Descriptor Tail");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "no_desc_avail", 
				CTLFLAG_RD, &txr->no_desc_avail,
				"Queue No Descriptor Available");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "tx_packets",
				CTLFLAG_RD, &txr->tx_packets,
				"Queue Packets Transmitted");

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_head", 
				CTLFLAG_RD, adapter, E1000_RDH(rxr->me),
				igb_sysctl_reg_handler, "IU",
				"Receive Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_tail", 
				CTLFLAG_RD, adapter, E1000_RDT(rxr->me),
				igb_sysctl_reg_handler, "IU",
				"Receive Descriptor Tail");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "rx_packets",
				CTLFLAG_RD, &rxr->rx_packets,
				"Queue Packets Received");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
				CTLFLAG_RD, &rxr->rx_bytes,
				"Queue Bytes Received");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "lro_queued",
				CTLFLAG_RD, &lro->lro_queued, 0,
				"LRO Queued");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "lro_flushed",
				CTLFLAG_RD, &lro->lro_flushed, 0,
				"LRO Flushed");
	}

	/* MAC stats get their own sub node */

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats", 
				    CTLFLAG_RD, NULL, "MAC Statistics");
	stat_list = SYSCTL_CHILDREN(stat_node);

	/*
	** VF adapter has a very limited set of stats
	** since its not managing the metal, so to speak.
	*/
	if (adapter->hw.mac.type == e1000_vfadapt) {
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_pkts_recvd",
			CTLFLAG_RD, &stats->gprc,
			"Good Packets Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &stats->gptc,
			"Good Packets Transmitted");
 	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_octets_recvd", 
 			CTLFLAG_RD, &stats->gorc, 
 			"Good Octets Received"); 
 	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_octets_txd", 
 			CTLFLAG_RD, &stats->gotc, 
 			"Good Octets Transmitted"); 
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_recvd",
			CTLFLAG_RD, &stats->mprc,
			"Multicast Packets Received");
		return;
	}

	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "excess_coll", 
			CTLFLAG_RD, &stats->ecol,
			"Excessive collisions");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "single_coll", 
			CTLFLAG_RD, &stats->scc,
			"Single collisions");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "multiple_coll", 
			CTLFLAG_RD, &stats->mcc,
			"Multiple collisions");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "late_coll", 
			CTLFLAG_RD, &stats->latecol,
			"Late collisions");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "collision_count", 
			CTLFLAG_RD, &stats->colc,
			"Collision Count");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "symbol_errors",
			CTLFLAG_RD, &stats->symerrs,
			"Symbol Errors");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "sequence_errors",
			CTLFLAG_RD, &stats->sec,
			"Sequence Errors");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "defer_count",
			CTLFLAG_RD, &stats->dc,
			"Defer Count");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "missed_packets",
			CTLFLAG_RD, &stats->mpc,
			"Missed Packets");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_no_buff",
			CTLFLAG_RD, &stats->rnbc,
			"Receive No Buffers");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_undersize",
			CTLFLAG_RD, &stats->ruc,
			"Receive Undersize");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_fragmented",
			CTLFLAG_RD, &stats->rfc,
			"Fragmented Packets Received ");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_oversize",
			CTLFLAG_RD, &stats->roc,
			"Oversized Packets Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_jabber",
			CTLFLAG_RD, &stats->rjc,
			"Recevied Jabber");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "recv_errs",
			CTLFLAG_RD, &stats->rxerrc,
			"Receive Errors");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "crc_errs",
			CTLFLAG_RD, &stats->crcerrs,
			"CRC errors");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "alignment_errs",
			CTLFLAG_RD, &stats->algnerrc,
			"Alignment Errors");
	/* On 82575 these are collision counts */
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "coll_ext_errs",
			CTLFLAG_RD, &stats->cexterr,
			"Collision/Carrier extension errors");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "xon_recvd",
			CTLFLAG_RD, &stats->xonrxc,
			"XON Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "xon_txd",
			CTLFLAG_RD, &stats->xontxc,
			"XON Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "xoff_recvd",
			CTLFLAG_RD, &stats->xoffrxc,
			"XOFF Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "xoff_txd",
			CTLFLAG_RD, &stats->xofftxc,
			"XOFF Transmitted");
	/* Packet Reception Stats */
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "total_pkts_recvd",
			CTLFLAG_RD, &stats->tpr,
			"Total Packets Received ");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_pkts_recvd",
			CTLFLAG_RD, &stats->gprc,
			"Good Packets Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_recvd",
			CTLFLAG_RD, &stats->bprc,
			"Broadcast Packets Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_recvd",
			CTLFLAG_RD, &stats->mprc,
			"Multicast Packets Received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_64",
			CTLFLAG_RD, &stats->prc64,
			"64 byte frames received ");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_65_127",
			CTLFLAG_RD, &stats->prc127,
			"65-127 byte frames received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_128_255",
			CTLFLAG_RD, &stats->prc255,
			"128-255 byte frames received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_256_511",
			CTLFLAG_RD, &stats->prc511,
			"256-511 byte frames received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_512_1023",
			CTLFLAG_RD, &stats->prc1023,
			"512-1023 byte frames received");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "rx_frames_1024_1522",
			CTLFLAG_RD, &stats->prc1522,
			"1023-1522 byte frames received");
 	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_octets_recvd", 
 			CTLFLAG_RD, &stats->gorc, 
 			"Good Octets Received"); 

	/* Packet Transmission Stats */
 	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_octets_txd", 
 			CTLFLAG_RD, &stats->gotc, 
 			"Good Octets Transmitted"); 
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "total_pkts_txd",
			CTLFLAG_RD, &stats->tpt,
			"Total Packets Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &stats->gptc,
			"Good Packets Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
			CTLFLAG_RD, &stats->bptc,
			"Broadcast Packets Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
			CTLFLAG_RD, &stats->mptc,
			"Multicast Packets Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_64",
			CTLFLAG_RD, &stats->ptc64,
			"64 byte frames transmitted ");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_65_127",
			CTLFLAG_RD, &stats->ptc127,
			"65-127 byte frames transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_128_255",
			CTLFLAG_RD, &stats->ptc255,
			"128-255 byte frames transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_256_511",
			CTLFLAG_RD, &stats->ptc511,
			"256-511 byte frames transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_512_1023",
			CTLFLAG_RD, &stats->ptc1023,
			"512-1023 byte frames transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tx_frames_1024_1522",
			CTLFLAG_RD, &stats->ptc1522,
			"1024-1522 byte frames transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tso_txd",
			CTLFLAG_RD, &stats->tsctc,
			"TSO Contexts Transmitted");
	SYSCTL_ADD_QUAD(ctx, stat_list, OID_AUTO, "tso_ctx_fail",
			CTLFLAG_RD, &stats->tsctfc,
			"TSO Contexts Failed");


	/* Interrupt Stats */

	int_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "interrupts", 
				    CTLFLAG_RD, NULL, "Interrupt Statistics");
	int_list = SYSCTL_CHILDREN(int_node);

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "asserts",
			CTLFLAG_RD, &stats->iac,
			"Interrupt Assertion Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "rx_pkt_timer",
			CTLFLAG_RD, &stats->icrxptc,
			"Interrupt Cause Rx Pkt Timer Expire Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "rx_abs_timer",
			CTLFLAG_RD, &stats->icrxatc,
			"Interrupt Cause Rx Abs Timer Expire Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "tx_pkt_timer",
			CTLFLAG_RD, &stats->ictxptc,
			"Interrupt Cause Tx Pkt Timer Expire Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "tx_abs_timer",
			CTLFLAG_RD, &stats->ictxatc,
			"Interrupt Cause Tx Abs Timer Expire Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "tx_queue_empty",
			CTLFLAG_RD, &stats->ictxqec,
			"Interrupt Cause Tx Queue Empty Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "tx_queue_min_thresh",
			CTLFLAG_RD, &stats->ictxqmtc,
			"Interrupt Cause Tx Queue Min Thresh Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "rx_desc_min_thresh",
			CTLFLAG_RD, &stats->icrxdmtc,
			"Interrupt Cause Rx Desc Min Thresh Count");

	SYSCTL_ADD_QUAD(ctx, int_list, OID_AUTO, "rx_overrun",
			CTLFLAG_RD, &stats->icrxoc,
			"Interrupt Cause Receiver Overrun Count");

	/* Host to Card Stats */

	host_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "host", 
				    CTLFLAG_RD, NULL, 
				    "Host to Card Statistics");

	host_list = SYSCTL_CHILDREN(host_node);

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "breaker_tx_pkt",
			CTLFLAG_RD, &stats->cbtmpc,
			"Circuit Breaker Tx Packet Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "host_tx_pkt_discard",
			CTLFLAG_RD, &stats->htdpmc,
			"Host Transmit Discarded Packets");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "rx_pkt",
			CTLFLAG_RD, &stats->rpthc,
			"Rx Packets To Host");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "breaker_rx_pkts",
			CTLFLAG_RD, &stats->cbrmpc,
			"Circuit Breaker Rx Packet Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "breaker_rx_pkt_drop",
			CTLFLAG_RD, &stats->cbrdpc,
			"Circuit Breaker Rx Dropped Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "tx_good_pkt",
			CTLFLAG_RD, &stats->hgptc,
			"Host Good Packets Tx Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "breaker_tx_pkt_drop",
			CTLFLAG_RD, &stats->htcbdpc,
			"Host Tx Circuit Breaker Dropped Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "rx_good_bytes",
			CTLFLAG_RD, &stats->hgorc,
			"Host Good Octets Received Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "tx_good_bytes",
			CTLFLAG_RD, &stats->hgotc,
			"Host Good Octets Transmit Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "length_errors",
			CTLFLAG_RD, &stats->lenerrs,
			"Length Errors");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "serdes_violation_pkt",
			CTLFLAG_RD, &stats->scvpc,
			"SerDes/SGMII Code Violation Pkt Count");

	SYSCTL_ADD_QUAD(ctx, host_list, OID_AUTO, "header_redir_missed",
			CTLFLAG_RD, &stats->hrmpc,
			"Header Redirection Missed Packet Count");
}


/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/
static int
igb_sysctl_nvm_info(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	/*
	 * This value will cause a hex dump of the
	 * first 32 16-bit words of the EEPROM to
	 * the screen.
	 */
	if (result == 1) {
		adapter = (struct adapter *)arg1;
		igb_print_nvm_info(adapter);
        }

	return (error);
}

static void
igb_print_nvm_info(struct adapter *adapter)
{
	u16	eeprom_data;
	int	i, j, row = 0;

	/* Its a bit crude, but it gets the job done */
	printf("\nInterface EEPROM Dump:\n");
	printf("Offset\n0x0000  ");
	for (i = 0, j = 0; i < 32; i++, j++) {
		if (j == 8) { /* Make the offset block */
			j = 0; ++row;
			printf("\n0x00%x0  ",row);
		}
		e1000_read_nvm(&adapter->hw, i, 1, &eeprom_data);
		printf("%04x ", eeprom_data);
	}
	printf("\n");
}

static void
igb_add_rx_process_limit(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}
