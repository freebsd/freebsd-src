/******************************************************************************

  Copyright (c) 2001-2015, Intel Corporation 
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

/*
 * Uncomment the following extensions for better performance in a VM,
 * especially if you have support in the hypervisor.
 * See http://info.iet.unipi.it/~luigi/netmap/
 */
// #define BATCH_DISPATCH
// #define NIC_SEND_COMBINING
// #define NIC_PARAVIRT	/* enable virtio-like synchronization */

#include "opt_inet.h"
#include "opt_inet6.h"

#ifdef HAVE_KERNEL_OPTION_HEADERS
#include "opt_device_polling.h"
#endif

#include <sys/param.h>
#include <sys/systm.h>
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
#include <netinet/udp.h>

#include <machine/in_cksum.h>
#include <dev/led/led.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#include "e1000_api.h"
#include "if_lem.h"

/*********************************************************************
 *  Legacy Em Driver version:
 *********************************************************************/
char lem_driver_version[] = "1.1.0";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static em_vendor_info_t lem_vendor_info_array[] =
{
	/* Intel(R) PRO/1000 Network Connection */
	{ 0x8086, E1000_DEV_ID_82540EM,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82540EM_LOM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82540EP,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82540EP_LOM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82540EP_LP,	PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82541EI,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541ER,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541ER_LOM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541EI_MOBILE,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541GI,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541GI_LF,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82541GI_MOBILE,	PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82542,		PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82543GC_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82543GC_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82544EI_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82544EI_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82544GC_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82544GC_LOM,	PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82545EM_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82545EM_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82545GM_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82545GM_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82545GM_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82546EB_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546EB_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546EB_QUAD_COPPER, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_PCIE,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_QUAD_COPPER, PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3,
						PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82547EI,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82547EI_MOBILE,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82547GI,		PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{ 0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings for all supported NICs.
 *********************************************************************/

static char *lem_strings[] = {
	"Intel(R) PRO/1000 Legacy Network Connection"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int	lem_probe(device_t);
static int	lem_attach(device_t);
static int	lem_detach(device_t);
static int	lem_shutdown(device_t);
static int	lem_suspend(device_t);
static int	lem_resume(device_t);
static void	lem_start(struct ifnet *);
static void	lem_start_locked(struct ifnet *ifp);
static int	lem_ioctl(struct ifnet *, u_long, caddr_t);
static void	lem_init(void *);
static void	lem_init_locked(struct adapter *);
static void	lem_stop(void *);
static void	lem_media_status(struct ifnet *, struct ifmediareq *);
static int	lem_media_change(struct ifnet *);
static void	lem_identify_hardware(struct adapter *);
static int	lem_allocate_pci_resources(struct adapter *);
static int	lem_allocate_irq(struct adapter *adapter);
static void	lem_free_pci_resources(struct adapter *);
static void	lem_local_timer(void *);
static int	lem_hardware_init(struct adapter *);
static int	lem_setup_interface(device_t, struct adapter *);
static void	lem_setup_transmit_structures(struct adapter *);
static void	lem_initialize_transmit_unit(struct adapter *);
static int	lem_setup_receive_structures(struct adapter *);
static void	lem_initialize_receive_unit(struct adapter *);
static void	lem_enable_intr(struct adapter *);
static void	lem_disable_intr(struct adapter *);
static void	lem_free_transmit_structures(struct adapter *);
static void	lem_free_receive_structures(struct adapter *);
static void	lem_update_stats_counters(struct adapter *);
static void	lem_add_hw_stats(struct adapter *adapter);
static void	lem_txeof(struct adapter *);
static void	lem_tx_purge(struct adapter *);
static int	lem_allocate_receive_structures(struct adapter *);
static int	lem_allocate_transmit_structures(struct adapter *);
static bool	lem_rxeof(struct adapter *, int, int *);
#ifndef __NO_STRICT_ALIGNMENT
static int	lem_fixup_rx(struct adapter *);
#endif
static void	lem_receive_checksum(struct adapter *, struct e1000_rx_desc *,
		    struct mbuf *);
static void	lem_transmit_checksum_setup(struct adapter *, struct mbuf *,
		    u32 *, u32 *);
static void	lem_set_promisc(struct adapter *);
static void	lem_disable_promisc(struct adapter *);
static void	lem_set_multi(struct adapter *);
static void	lem_update_link_status(struct adapter *);
static int	lem_get_buf(struct adapter *, int);
static void	lem_register_vlan(void *, struct ifnet *, u16);
static void	lem_unregister_vlan(void *, struct ifnet *, u16);
static void	lem_setup_vlan_hw_support(struct adapter *);
static int	lem_xmit(struct adapter *, struct mbuf **);
static void	lem_smartspeed(struct adapter *);
static int	lem_82547_fifo_workaround(struct adapter *, int);
static void	lem_82547_update_fifo_head(struct adapter *, int);
static int	lem_82547_tx_fifo_reset(struct adapter *);
static void	lem_82547_move_tail(void *);
static int	lem_dma_malloc(struct adapter *, bus_size_t,
		    struct em_dma_alloc *, int);
static void	lem_dma_free(struct adapter *, struct em_dma_alloc *);
static int	lem_sysctl_nvm_info(SYSCTL_HANDLER_ARGS);
static void	lem_print_nvm_info(struct adapter *);
static int 	lem_is_valid_ether_addr(u8 *);
static u32	lem_fill_descriptors (bus_addr_t address, u32 length,
		    PDESC_ARRAY desc_array);
static int	lem_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	lem_add_int_delay_sysctl(struct adapter *, const char *,
		    const char *, struct em_int_delay_info *, int, int);
static void	lem_set_flow_cntrl(struct adapter *, const char *,
		    const char *, int *, int);
/* Management and WOL Support */
static void	lem_init_manageability(struct adapter *);
static void	lem_release_manageability(struct adapter *);
static void     lem_get_hw_control(struct adapter *);
static void     lem_release_hw_control(struct adapter *);
static void	lem_get_wakeup(device_t);
static void     lem_enable_wakeup(device_t);
static int	lem_enable_phy_wakeup(struct adapter *);
static void	lem_led_func(void *, int);

static void	lem_intr(void *);
static int	lem_irq_fast(void *);
static void	lem_handle_rxtx(void *context, int pending);
static void	lem_handle_link(void *context, int pending);
static void	lem_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);

#ifdef DEVICE_POLLING
static poll_handler_t lem_poll;
#endif /* POLLING */

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t lem_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, lem_probe),
	DEVMETHOD(device_attach, lem_attach),
	DEVMETHOD(device_detach, lem_detach),
	DEVMETHOD(device_shutdown, lem_shutdown),
	DEVMETHOD(device_suspend, lem_suspend),
	DEVMETHOD(device_resume, lem_resume),
	DEVMETHOD_END
};

static driver_t lem_driver = {
	"em", lem_methods, sizeof(struct adapter),
};

extern devclass_t em_devclass;
DRIVER_MODULE(lem, pci, lem_driver, em_devclass, 0, 0);
MODULE_DEPEND(lem, pci, 1, 1, 1);
MODULE_DEPEND(lem, ether, 1, 1, 1);

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

#define EM_TICKS_TO_USECS(ticks)	((1024 * (ticks) + 500) / 1000)
#define EM_USECS_TO_TICKS(usecs)	((1000 * (usecs) + 512) / 1024)

#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR		(1000000000/(MAX_INTS_PER_SEC * 256))

static int lem_tx_int_delay_dflt = EM_TICKS_TO_USECS(EM_TIDV);
static int lem_rx_int_delay_dflt = EM_TICKS_TO_USECS(EM_RDTR);
static int lem_tx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_TADV);
static int lem_rx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_RADV);
/*
 * increase lem_rxd and lem_txd to at least 2048 in netmap mode
 * for better performance.
 */
static int lem_rxd = EM_DEFAULT_RXD;
static int lem_txd = EM_DEFAULT_TXD;
static int lem_smart_pwr_down = FALSE;

/* Controls whether promiscuous also shows bad packets */
static int lem_debug_sbp = FALSE;

TUNABLE_INT("hw.em.tx_int_delay", &lem_tx_int_delay_dflt);
TUNABLE_INT("hw.em.rx_int_delay", &lem_rx_int_delay_dflt);
TUNABLE_INT("hw.em.tx_abs_int_delay", &lem_tx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rx_abs_int_delay", &lem_rx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rxd", &lem_rxd);
TUNABLE_INT("hw.em.txd", &lem_txd);
TUNABLE_INT("hw.em.smart_pwr_down", &lem_smart_pwr_down);
TUNABLE_INT("hw.em.sbp", &lem_debug_sbp);

/* Interrupt style - default to fast */
static int lem_use_legacy_irq = 0;
TUNABLE_INT("hw.em.use_legacy_irq", &lem_use_legacy_irq);

/* How many packets rxeof tries to clean at a time */
static int lem_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &lem_rx_process_limit);

/* Flow control setting - default to FULL */
static int lem_fc_setting = e1000_fc_full;
TUNABLE_INT("hw.em.fc_setting", &lem_fc_setting);

/* Global used in WOL setup with multiport cards */
static int global_quad_port_a = 0;

#ifdef DEV_NETMAP	/* see ixgbe.c for details */
#include <dev/netmap/if_lem_netmap.h>
#endif /* DEV_NETMAP */

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
lem_probe(device_t dev)
{
	char		adapter_name[60];
	u16		pci_vendor_id = 0;
	u16		pci_device_id = 0;
	u16		pci_subvendor_id = 0;
	u16		pci_subdevice_id = 0;
	em_vendor_info_t *ent;

	INIT_DEBUGOUT("em_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != EM_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = lem_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		    (ent->subvendor_id == PCI_ANY_ID)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		    (ent->subdevice_id == PCI_ANY_ID))) {
			sprintf(adapter_name, "%s %s",
				lem_strings[ent->index],
				lem_driver_version);
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
lem_attach(device_t dev)
{
	struct adapter	*adapter;
	int		tsize, rsize;
	int		error = 0;

	INIT_DEBUGOUT("lem_attach: begin");

	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	EM_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));
	EM_TX_LOCK_INIT(adapter, device_get_nameunit(dev));
	EM_RX_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* SYSCTL stuff */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "nvm", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    lem_sysctl_nvm_info, "I", "NVM Information");

	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);
	callout_init_mtx(&adapter->tx_fifo_timer, &adapter->tx_mtx, 0);

	/* Determine hardware and mac info */
	lem_identify_hardware(adapter);

	/* Setup PCI resources */
	if (lem_allocate_pci_resources(adapter)) {
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

	/* Set up some sysctls for the tunable interrupt delays */
	lem_add_int_delay_sysctl(adapter, "rx_int_delay",
	    "receive interrupt delay in usecs", &adapter->rx_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_RDTR), lem_rx_int_delay_dflt);
	lem_add_int_delay_sysctl(adapter, "tx_int_delay",
	    "transmit interrupt delay in usecs", &adapter->tx_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_TIDV), lem_tx_int_delay_dflt);
	if (adapter->hw.mac.type >= e1000_82540) {
		lem_add_int_delay_sysctl(adapter, "rx_abs_int_delay",
		    "receive interrupt delay limit in usecs",
		    &adapter->rx_abs_int_delay,
		    E1000_REGISTER(&adapter->hw, E1000_RADV),
		    lem_rx_abs_int_delay_dflt);
		lem_add_int_delay_sysctl(adapter, "tx_abs_int_delay",
		    "transmit interrupt delay limit in usecs",
		    &adapter->tx_abs_int_delay,
		    E1000_REGISTER(&adapter->hw, E1000_TADV),
		    lem_tx_abs_int_delay_dflt);
		lem_add_int_delay_sysctl(adapter, "itr",
		    "interrupt delay limit in usecs/4",
		    &adapter->tx_itr,
		    E1000_REGISTER(&adapter->hw, E1000_ITR),
		    DEFAULT_ITR);
	}

	/* Sysctls for limiting the amount of work done in the taskqueue */
	lem_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    lem_rx_process_limit);

#ifdef NIC_SEND_COMBINING
	/* Sysctls to control mitigation */
	lem_add_rx_process_limit(adapter, "sc_enable",
	    "driver TDT mitigation", &adapter->sc_enable, 0);
#endif /* NIC_SEND_COMBINING */
#ifdef BATCH_DISPATCH
	lem_add_rx_process_limit(adapter, "batch_enable",
	    "driver rx batch", &adapter->batch_enable, 0);
#endif /* BATCH_DISPATCH */
#ifdef NIC_PARAVIRT
	lem_add_rx_process_limit(adapter, "rx_retries",
	    "driver rx retries", &adapter->rx_retries, 0);
#endif /* NIC_PARAVIRT */

        /* Sysctl for setting the interface flow control */
	lem_set_flow_cntrl(adapter, "flow_control",
	    "flow control setting",
	    &adapter->fc_setting, lem_fc_setting);

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of E1000_DBA_ALIGN.
	 */
	if (((lem_txd * sizeof(struct e1000_tx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac.type >= e1000_82544 && lem_txd > EM_MAX_TXD) ||
	    (adapter->hw.mac.type < e1000_82544 && lem_txd > EM_MAX_TXD_82543) ||
	    (lem_txd < EM_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    EM_DEFAULT_TXD, lem_txd);
		adapter->num_tx_desc = EM_DEFAULT_TXD;
	} else
		adapter->num_tx_desc = lem_txd;
	if (((lem_rxd * sizeof(struct e1000_rx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac.type >= e1000_82544 && lem_rxd > EM_MAX_RXD) ||
	    (adapter->hw.mac.type < e1000_82544 && lem_rxd > EM_MAX_RXD_82543) ||
	    (lem_rxd < EM_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    EM_DEFAULT_RXD, lem_rxd);
		adapter->num_rx_desc = EM_DEFAULT_RXD;
	} else
		adapter->num_rx_desc = lem_rxd;

	adapter->hw.mac.autoneg = DO_AUTO_NEG;
	adapter->hw.phy.autoneg_wait_to_complete = FALSE;
	adapter->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	adapter->rx_buffer_len = 2048;

	e1000_init_script_state_82541(&adapter->hw, TRUE);
	e1000_set_tbi_compatibility_82543(&adapter->hw, TRUE);

	/* Copper options */
	if (adapter->hw.phy.media_type == e1000_media_type_copper) {
		adapter->hw.phy.mdix = AUTO_ALL_MODES;
		adapter->hw.phy.disable_polarity_correction = FALSE;
		adapter->hw.phy.ms_type = EM_MASTER_SLAVE;
	}

	/*
	 * Set the frame limits assuming
	 * standard ethernet sized frames.
	 */
	adapter->max_frame_size = ETHERMTU + ETHER_HDR_LEN + ETHERNET_FCS_SIZE;
	adapter->min_frame_size = ETH_ZLEN + ETHERNET_FCS_SIZE;

	/*
	 * This controls when hardware reports transmit completion
	 * status.
	 */
	adapter->hw.mac.report_tx_early = 1;

#ifdef NIC_PARAVIRT
	device_printf(dev, "driver supports paravirt, subdev 0x%x\n",
		adapter->hw.subsystem_device_id);
	if (adapter->hw.subsystem_device_id == E1000_PARA_SUBDEV) {
		uint64_t bus_addr;

		device_printf(dev, "paravirt support on dev %p\n", adapter);
		tsize = 4096; // XXX one page for the csb
		if (lem_dma_malloc(adapter, tsize, &adapter->csb_mem, BUS_DMA_NOWAIT)) {
			device_printf(dev, "Unable to allocate csb memory\n");
			error = ENOMEM;
			goto err_csb;
		}
		/* Setup the Base of the CSB */
		adapter->csb = (struct paravirt_csb *)adapter->csb_mem.dma_vaddr;
		/* force the first kick */
		adapter->csb->host_need_txkick = 1; /* txring empty */
		adapter->csb->guest_need_rxkick = 1; /* no rx packets */
		bus_addr = adapter->csb_mem.dma_paddr;
		lem_add_rx_process_limit(adapter, "csb_on",
		    "enable paravirt.", &adapter->csb->guest_csb_on, 0);
		lem_add_rx_process_limit(adapter, "txc_lim",
		    "txc_lim", &adapter->csb->host_txcycles_lim, 1);

		/* some stats */
#define PA_SC(name, var, val)		\
	lem_add_rx_process_limit(adapter, name, name, var, val)
		PA_SC("host_need_txkick",&adapter->csb->host_need_txkick, 1);
		PA_SC("host_rxkick_at",&adapter->csb->host_rxkick_at, ~0);
		PA_SC("guest_need_txkick",&adapter->csb->guest_need_txkick, 0);
		PA_SC("guest_need_rxkick",&adapter->csb->guest_need_rxkick, 1);
		PA_SC("tdt_reg_count",&adapter->tdt_reg_count, 0);
		PA_SC("tdt_csb_count",&adapter->tdt_csb_count, 0);
		PA_SC("tdt_int_count",&adapter->tdt_int_count, 0);
		PA_SC("guest_need_kick_count",&adapter->guest_need_kick_count, 0);
		/* tell the host where the block is */
		E1000_WRITE_REG(&adapter->hw, E1000_CSBAH,
			(u32)(bus_addr >> 32));
		E1000_WRITE_REG(&adapter->hw, E1000_CSBAL,
			(u32)bus_addr);
	}
#endif /* NIC_PARAVIRT */

	tsize = roundup2(adapter->num_tx_desc * sizeof(struct e1000_tx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Transmit Descriptor ring */
	if (lem_dma_malloc(adapter, tsize, &adapter->txdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate tx_desc memory\n");
		error = ENOMEM;
		goto err_tx_desc;
	}
	adapter->tx_desc_base = 
	    (struct e1000_tx_desc *)adapter->txdma.dma_vaddr;

	rsize = roundup2(adapter->num_rx_desc * sizeof(struct e1000_rx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Receive Descriptor ring */
	if (lem_dma_malloc(adapter, rsize, &adapter->rxdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate rx_desc memory\n");
		error = ENOMEM;
		goto err_rx_desc;
	}
	adapter->rx_desc_base =
	    (struct e1000_rx_desc *)adapter->rxdma.dma_vaddr;

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(u8) * ETH_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (adapter->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_hw_init;
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
			goto err_hw_init;
		}
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (e1000_read_mac_addr(&adapter->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_hw_init;
	}

	if (!lem_is_valid_ether_addr(adapter->hw.mac.addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Initialize the hardware */
	if (lem_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Allocate transmit descriptors and buffers */
	if (lem_allocate_transmit_structures(adapter)) {
		device_printf(dev, "Could not setup transmit structures\n");
		error = ENOMEM;
		goto err_tx_struct;
	}

	/* Allocate receive descriptors and buffers */
	if (lem_allocate_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		error = ENOMEM;
		goto err_rx_struct;
	}

	/*
	**  Do interrupt configuration
	*/
	error = lem_allocate_irq(adapter);
	if (error)
		goto err_rx_struct;

	/*
	 * Get Wake-on-Lan and Management info for later use
	 */
	lem_get_wakeup(dev);

	/* Setup OS specific network interface */
	if (lem_setup_interface(dev, adapter) != 0)
		goto err_rx_struct;

	/* Initialize statistics */
	lem_update_stats_counters(adapter);

	adapter->hw.mac.get_link_status = 1;
	lem_update_link_status(adapter);

	/* Indicate SOL/IDER usage */
	if (e1000_check_reset_block(&adapter->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Do we need workaround for 82544 PCI-X adapter? */
	if (adapter->hw.bus.type == e1000_bus_type_pcix &&
	    adapter->hw.mac.type == e1000_82544)
		adapter->pcix_82544 = TRUE;
	else
		adapter->pcix_82544 = FALSE;

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    lem_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    lem_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST); 

	lem_add_hw_stats(adapter);

	/* Non-AMT based hardware can now take control from firmware */
	if (adapter->has_manage && !adapter->has_amt)
		lem_get_hw_control(adapter);

	/* Tell the stack that the interface is not active */
	adapter->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	adapter->led_dev = led_create(lem_led_func, adapter,
	    device_get_nameunit(dev));

#ifdef DEV_NETMAP
	lem_netmap_attach(adapter);
#endif /* DEV_NETMAP */
	INIT_DEBUGOUT("lem_attach: end");

	return (0);

err_rx_struct:
	lem_free_transmit_structures(adapter);
err_tx_struct:
err_hw_init:
	lem_release_hw_control(adapter);
	lem_dma_free(adapter, &adapter->rxdma);
err_rx_desc:
	lem_dma_free(adapter, &adapter->txdma);
err_tx_desc:
#ifdef NIC_PARAVIRT
	lem_dma_free(adapter, &adapter->csb_mem);
err_csb:
#endif /* NIC_PARAVIRT */

err_pci:
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
	lem_free_pci_resources(adapter);
	free(adapter->mta, M_DEVBUF);
	EM_TX_LOCK_DESTROY(adapter);
	EM_RX_LOCK_DESTROY(adapter);
	EM_CORE_LOCK_DESTROY(adapter);

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
lem_detach(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	struct ifnet	*ifp = adapter->ifp;

	INIT_DEBUGOUT("em_detach: begin");

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	if (adapter->led_dev != NULL)
		led_destroy(adapter->led_dev);

	EM_CORE_LOCK(adapter);
	EM_TX_LOCK(adapter);
	adapter->in_detach = 1;
	lem_stop(adapter);
	e1000_phy_hw_reset(&adapter->hw);

	lem_release_manageability(adapter);

	EM_TX_UNLOCK(adapter);
	EM_CORE_UNLOCK(adapter);

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach); 

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);
	callout_drain(&adapter->tx_fifo_timer);

#ifdef DEV_NETMAP
	netmap_detach(ifp);
#endif /* DEV_NETMAP */
	lem_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(ifp);

	lem_free_transmit_structures(adapter);
	lem_free_receive_structures(adapter);

	/* Free Transmit Descriptor ring */
	if (adapter->tx_desc_base) {
		lem_dma_free(adapter, &adapter->txdma);
		adapter->tx_desc_base = NULL;
	}

	/* Free Receive Descriptor ring */
	if (adapter->rx_desc_base) {
		lem_dma_free(adapter, &adapter->rxdma);
		adapter->rx_desc_base = NULL;
	}

#ifdef NIC_PARAVIRT
	if (adapter->csb) {
		lem_dma_free(adapter, &adapter->csb_mem);
		adapter->csb = NULL;
	}
#endif /* NIC_PARAVIRT */
	lem_release_hw_control(adapter);
	free(adapter->mta, M_DEVBUF);
	EM_TX_LOCK_DESTROY(adapter);
	EM_RX_LOCK_DESTROY(adapter);
	EM_CORE_LOCK_DESTROY(adapter);

	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
lem_shutdown(device_t dev)
{
	return lem_suspend(dev);
}

/*
 * Suspend/resume device methods.
 */
static int
lem_suspend(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);

	EM_CORE_LOCK(adapter);

	lem_release_manageability(adapter);
	lem_release_hw_control(adapter);
	lem_enable_wakeup(dev);

	EM_CORE_UNLOCK(adapter);

	return bus_generic_suspend(dev);
}

static int
lem_resume(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ifnet *ifp = adapter->ifp;

	EM_CORE_LOCK(adapter);
	lem_init_locked(adapter);
	lem_init_manageability(adapter);
	EM_CORE_UNLOCK(adapter);
	lem_start(ifp);

	return bus_generic_resume(dev);
}


static void
lem_start_locked(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct mbuf	*m_head;

	EM_TX_LOCK_ASSERT(adapter);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!adapter->link_active)
		return;

        /*
         * Force a cleanup if number of TX descriptors
         * available hits the threshold
         */
	if (adapter->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
		lem_txeof(adapter);
		/* Now do we at least have a minimal? */
		if (adapter->num_tx_desc_avail <= EM_TX_OP_THRESHOLD) {
			adapter->no_tx_desc_avail1++;
			return;
		}
	}

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

                IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 *  Encapsulation can modify our pointer, and or make it
		 *  NULL on failure.  In that event, we can't requeue.
		 */
		if (lem_xmit(adapter, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting. */
		adapter->watchdog_check = TRUE;
		adapter->watchdog_time = ticks;
	}
	if (adapter->num_tx_desc_avail <= EM_TX_OP_THRESHOLD)
		ifp->if_drv_flags |= IFF_DRV_OACTIVE;
#ifdef NIC_PARAVIRT
	if ((ifp->if_drv_flags & IFF_DRV_OACTIVE) && adapter->csb &&
	    adapter->csb->guest_csb_on &&
	    !(adapter->csb->guest_need_txkick & 1))  {
		adapter->csb->guest_need_txkick = 1;
		adapter->guest_need_kick_count++;
		// XXX memory barrier
		lem_txeof(adapter); // XXX possibly clear IFF_DRV_OACTIVE
	}
#endif /* NIC_PARAVIRT */

	return;
}

static void
lem_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	EM_TX_LOCK(adapter);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		lem_start_locked(ifp);
	EM_TX_UNLOCK(adapter);
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  em_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
lem_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifreq	*ifr = (struct ifreq *)data;
#if defined(INET) || defined(INET6)
	struct ifaddr	*ifa = (struct ifaddr *)data;
#endif
	bool		avoid_reset = FALSE;
	int		error = 0;

	if (adapter->in_detach)
		return (error);

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
		/*
		** Calling init results in link renegotiation,
		** so we avoid doing it when possible.
		*/
		if (avoid_reset) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				lem_init(adapter);
#ifdef INET
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		EM_CORE_LOCK(adapter);
		switch (adapter->hw.mac.type) {
		case e1000_82542:
			max_frame_size = ETHER_MAX_LEN;
			break;
		default:
			max_frame_size = MAX_JUMBO_FRAME_SIZE;
		}
		if (ifr->ifr_mtu > max_frame_size - ETHER_HDR_LEN -
		    ETHER_CRC_LEN) {
			EM_CORE_UNLOCK(adapter);
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		adapter->max_frame_size =
		    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
		if ((if_getdrvflags(ifp) & IFF_DRV_RUNNING))
			lem_init_locked(adapter);
		EM_CORE_UNLOCK(adapter);
		break;
	    }
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd:\
		    SIOCSIFFLAGS (Set Interface Flags)");
		EM_CORE_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ adapter->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					lem_disable_promisc(adapter);
					lem_set_promisc(adapter);
				}
			} else
				lem_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				EM_TX_LOCK(adapter);
				lem_stop(adapter);
				EM_TX_UNLOCK(adapter);
			}
		adapter->if_flags = ifp->if_flags;
		EM_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			EM_CORE_LOCK(adapter);
			lem_disable_intr(adapter);
			lem_set_multi(adapter);
			if (adapter->hw.mac.type == e1000_82542 && 
	    		    adapter->hw.revision_id == E1000_REVISION_2) {
				lem_initialize_receive_unit(adapter);
			}
#ifdef DEVICE_POLLING
			if (!(ifp->if_capenable & IFCAP_POLLING))
#endif
				lem_enable_intr(adapter);
			EM_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
		/* Check SOL/IDER usage */
		EM_CORE_LOCK(adapter);
		if (e1000_check_reset_block(&adapter->hw)) {
			EM_CORE_UNLOCK(adapter);
			device_printf(adapter->dev, "Media change is"
			    " blocked due to SOL/IDER session.\n");
			break;
		}
		EM_CORE_UNLOCK(adapter);
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
				error = ether_poll_register(lem_poll, ifp);
				if (error)
					return (error);
				EM_CORE_LOCK(adapter);
				lem_disable_intr(adapter);
				ifp->if_capenable |= IFCAP_POLLING;
				EM_CORE_UNLOCK(adapter);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				EM_CORE_LOCK(adapter);
				lem_enable_intr(adapter);
				ifp->if_capenable &= ~IFCAP_POLLING;
				EM_CORE_UNLOCK(adapter);
			}
		}
#endif
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			reinit = 1;
		}
		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			reinit = 1;
		}
		if ((mask & IFCAP_WOL) &&
		    (ifp->if_capabilities & IFCAP_WOL) != 0) {
			if (mask & IFCAP_WOL_MCAST)
				ifp->if_capenable ^= IFCAP_WOL_MCAST;
			if (mask & IFCAP_WOL_MAGIC)
				ifp->if_capenable ^= IFCAP_WOL_MAGIC;
		}
		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING))
			lem_init(adapter);
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
lem_init_locked(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t	dev = adapter->dev;
	u32		pba;

	INIT_DEBUGOUT("lem_init: begin");

	EM_CORE_LOCK_ASSERT(adapter);

	EM_TX_LOCK(adapter);
	lem_stop(adapter);
	EM_TX_UNLOCK(adapter);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 *
	 * Devices before the 82547 had a Packet Buffer of 64K.
	 *   Default allocation: PBA=48K for Rx, leaving 16K for Tx.
	 * After the 82547 the buffer was reduced to 40K.
	 *   Default allocation: PBA=30K for Rx, leaving 10K for Tx.
	 *   Note: default does not leave enough room for Jumbo Frame >10k.
	 */
	switch (adapter->hw.mac.type) {
	case e1000_82547:
	case e1000_82547_rev_2: /* 82547: Total Packet Buffer is 40K */
		if (adapter->max_frame_size > 8192)
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
		else
			pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		adapter->tx_fifo_head = 0;
		adapter->tx_head_addr = pba << EM_TX_HEAD_ADDR_SHIFT;
		adapter->tx_fifo_size =
		    (E1000_PBA_40K - pba) << EM_PBA_BYTES_SHIFT;
		break;
	default:
		/* Devices before 82547 had a Packet Buffer of 64K.   */
		if (adapter->max_frame_size > 8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}

	INIT_DEBUGOUT1("lem_init: pba=%dK",pba);
	E1000_WRITE_REG(&adapter->hw, E1000_PBA, pba);
	
	/* Get the latest mac address, User can use a LAA */
        bcopy(IF_LLADDR(adapter->ifp), adapter->hw.mac.addr,
              ETHER_ADDR_LEN);

	/* Put the address into the Receive Address Array */
	e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	/* Initialize the hardware */
	if (lem_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		return;
	}
	lem_update_link_status(adapter);

	/* Setup VLAN support, basic and offload if available */
	E1000_WRITE_REG(&adapter->hw, E1000_VET, ETHERTYPE_VLAN);

	/* Set hardware offload abilities */
	ifp->if_hwassist = 0;
	if (adapter->hw.mac.type >= e1000_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
	}

	/* Configure for OS presence */
	lem_init_manageability(adapter);

	/* Prepare transmit descriptors and buffers */
	lem_setup_transmit_structures(adapter);
	lem_initialize_transmit_unit(adapter);

	/* Setup Multicast table */
	lem_set_multi(adapter);

	/* Prepare receive descriptors and buffers */
	if (lem_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		EM_TX_LOCK(adapter);
		lem_stop(adapter);
		EM_TX_UNLOCK(adapter);
		return;
	}
	lem_initialize_receive_unit(adapter);

	/* Use real VLAN Filter support? */
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) {
		if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
			/* Use real VLAN Filter support */
			lem_setup_vlan_hw_support(adapter);
		else {
			u32 ctrl;
			ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
			ctrl |= E1000_CTRL_VME;
			E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
                }
	}

	/* Don't lose promiscuous settings */
	lem_set_promisc(adapter);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&adapter->timer, hz, lem_local_timer, adapter);
	e1000_clear_hw_cntrs_base_generic(&adapter->hw);

#ifdef DEVICE_POLLING
	/*
	 * Only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		lem_disable_intr(adapter);
	else
#endif /* DEVICE_POLLING */
		lem_enable_intr(adapter);

	/* AMT based hardware can now take control from firmware */
	if (adapter->has_manage && adapter->has_amt)
		lem_get_hw_control(adapter);
}

static void
lem_init(void *arg)
{
	struct adapter *adapter = arg;

	EM_CORE_LOCK(adapter);
	lem_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}


#ifdef DEVICE_POLLING
/*********************************************************************
 *
 *  Legacy polling routine  
 *
 *********************************************************************/
static int
lem_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	u32		reg_icr, rx_done = 0;

	EM_CORE_LOCK(adapter);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		EM_CORE_UNLOCK(adapter);
		return (rx_done);
	}

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&adapter->timer);
			adapter->hw.mac.get_link_status = 1;
			lem_update_link_status(adapter);
			callout_reset(&adapter->timer, hz,
			    lem_local_timer, adapter);
		}
	}
	EM_CORE_UNLOCK(adapter);

	lem_rxeof(adapter, count, &rx_done);

	EM_TX_LOCK(adapter);
	lem_txeof(adapter);
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		lem_start_locked(ifp);
	EM_TX_UNLOCK(adapter);
	return (rx_done);
}
#endif /* DEVICE_POLLING */

/*********************************************************************
 *
 *  Legacy Interrupt Service routine  
 *
 *********************************************************************/
static void
lem_intr(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;
	u32		reg_icr;


	if ((ifp->if_capenable & IFCAP_POLLING) ||
	    ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0))
		return;

	EM_CORE_LOCK(adapter);
	reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);
	if (reg_icr & E1000_ICR_RXO)
		adapter->rx_overruns++;

	if ((reg_icr == 0xffffffff) || (reg_icr == 0)) {
		EM_CORE_UNLOCK(adapter);
		return;
	}

	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
		callout_stop(&adapter->timer);
		adapter->hw.mac.get_link_status = 1;
		lem_update_link_status(adapter);
		/* Deal with TX cruft when link lost */
		lem_tx_purge(adapter);
		callout_reset(&adapter->timer, hz,
		    lem_local_timer, adapter);
		EM_CORE_UNLOCK(adapter);
		return;
	}

	EM_CORE_UNLOCK(adapter);
	lem_rxeof(adapter, -1, NULL);

	EM_TX_LOCK(adapter);
	lem_txeof(adapter);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		lem_start_locked(ifp);
	EM_TX_UNLOCK(adapter);
	return;
}


static void
lem_handle_link(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet *ifp = adapter->ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	EM_CORE_LOCK(adapter);
	callout_stop(&adapter->timer);
	lem_update_link_status(adapter);
	/* Deal with TX cruft when link lost */
	lem_tx_purge(adapter);
	callout_reset(&adapter->timer, hz, lem_local_timer, adapter);
	EM_CORE_UNLOCK(adapter);
}


/* Combined RX/TX handler, used by Legacy and MSI */
static void
lem_handle_rxtx(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;


	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		bool more = lem_rxeof(adapter, adapter->rx_process_limit, NULL);
		EM_TX_LOCK(adapter);
		lem_txeof(adapter);
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			lem_start_locked(ifp);
		EM_TX_UNLOCK(adapter);
		if (more) {
			taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);
			return;
		}
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		lem_enable_intr(adapter);
}

/*********************************************************************
 *
 *  Fast Legacy/MSI Combined Interrupt Service routine  
 *
 *********************************************************************/
static int
lem_irq_fast(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp;
	u32		reg_icr;

	ifp = adapter->ifp;

	reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);

	/* Hot eject?  */
	if (reg_icr == 0xffffffff)
		return FILTER_STRAY;

	/* Definitely not our interrupt.  */
	if (reg_icr == 0x0)
		return FILTER_STRAY;

	/*
	 * Mask interrupts until the taskqueue is finished running.  This is
	 * cheap, just assume that it is needed.  This also works around the
	 * MSI message reordering errata on certain systems.
	 */
	lem_disable_intr(adapter);
	taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
		adapter->hw.mac.get_link_status = 1;
		taskqueue_enqueue(taskqueue_fast, &adapter->link_task);
	}

	if (reg_icr & E1000_ICR_RXO)
		adapter->rx_overruns++;
	return FILTER_HANDLED;
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
lem_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct adapter *adapter = ifp->if_softc;
	u_char fiber_type = IFM_1000_SX;

	INIT_DEBUGOUT("lem_media_status: begin");

	EM_CORE_LOCK(adapter);
	lem_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		EM_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes)) {
		if (adapter->hw.mac.type == e1000_82545)
			fiber_type = IFM_1000_LX;
		ifmr->ifm_active |= fiber_type | IFM_FDX;
	} else {
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
	EM_CORE_UNLOCK(adapter);
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
lem_media_change(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia  *ifm = &adapter->media;

	INIT_DEBUGOUT("lem_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	EM_CORE_LOCK(adapter);
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

	lem_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
lem_xmit(struct adapter *adapter, struct mbuf **m_headp)
{
	bus_dma_segment_t	segs[EM_MAX_SCATTER];
	bus_dmamap_t		map;
	struct em_buffer	*tx_buffer, *tx_buffer_mapped;
	struct e1000_tx_desc	*ctxd = NULL;
	struct mbuf		*m_head;
	u32			txd_upper, txd_lower, txd_used, txd_saved;
	int			error, nsegs, i, j, first, last = 0;

	m_head = *m_headp;
	txd_upper = txd_lower = txd_used = txd_saved = 0;

	/*
	** When doing checksum offload, it is critical to
	** make sure the first mbuf has more than header,
	** because that routine expects data to be present.
	*/
	if ((m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD) &&
	    (m_head->m_len < ETHER_HDR_LEN + sizeof(struct ip))) {
		m_head = m_pullup(m_head, ETHER_HDR_LEN + sizeof(struct ip));
		*m_headp = m_head;
		if (m_head == NULL)
			return (ENOBUFS);
	}

	/*
	 * Map the packet for DMA
	 *
	 * Capture the first descriptor index,
	 * this descriptor will have the index
	 * of the EOP which is the only one that
	 * now gets a DONE bit writeback.
	 */
	first = adapter->next_avail_tx_desc;
	tx_buffer = &adapter->tx_buffer_area[first];
	tx_buffer_mapped = tx_buffer;
	map = tx_buffer->map;

	error = bus_dmamap_load_mbuf_sg(adapter->txtag, map,
	    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

	/*
	 * There are two types of errors we can (try) to handle:
	 * - EFBIG means the mbuf chain was too long and bus_dma ran
	 *   out of segments.  Defragment the mbuf chain and try again.
	 * - ENOMEM means bus_dma could not obtain enough bounce buffers
	 *   at this point in time.  Defer sending and try again later.
	 * All other errors, in particular EINVAL, are fatal and prevent the
	 * mbuf chain from ever going through.  Drop it and report error.
	 */
	if (error == EFBIG) {
		struct mbuf *m;

		m = m_collapse(*m_headp, M_NOWAIT, EM_MAX_SCATTER);
		if (m == NULL) {
			adapter->mbuf_defrag_failed++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (ENOBUFS);
		}
		*m_headp = m;

		/* Try it again */
		error = bus_dmamap_load_mbuf_sg(adapter->txtag, map,
		    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);

		if (error) {
			adapter->no_tx_dma_setup++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (error);
		}
	} else if (error != 0) {
		adapter->no_tx_dma_setup++;
		return (error);
	}

        if (adapter->num_tx_desc_avail < (nsegs + 2)) {
                adapter->no_tx_desc_avail2++;
		bus_dmamap_unload(adapter->txtag, map);
		return (ENOBUFS);
        }
	m_head = *m_headp;

	/* Do hardware assists */
	if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD)
		lem_transmit_checksum_setup(adapter,  m_head,
		    &txd_upper, &txd_lower);

	i = adapter->next_avail_tx_desc;
	if (adapter->pcix_82544) 
		txd_saved = i;

	/* Set up our transmit descriptors */
	for (j = 0; j < nsegs; j++) {
		bus_size_t seg_len;
		bus_addr_t seg_addr;
		/* If adapter is 82544 and on PCIX bus */
		if(adapter->pcix_82544) {
			DESC_ARRAY	desc_array;
			u32		array_elements, counter;
			/*
			 * Check the Address and Length combination and
			 * split the data accordingly
			 */
			array_elements = lem_fill_descriptors(segs[j].ds_addr,
			    segs[j].ds_len, &desc_array);
			for (counter = 0; counter < array_elements; counter++) {
				if (txd_used == adapter->num_tx_desc_avail) {
					adapter->next_avail_tx_desc = txd_saved;
					adapter->no_tx_desc_avail2++;
					bus_dmamap_unload(adapter->txtag, map);
					return (ENOBUFS);
				}
				tx_buffer = &adapter->tx_buffer_area[i];
				ctxd = &adapter->tx_desc_base[i];
				ctxd->buffer_addr = htole64(
				    desc_array.descriptor[counter].address);
				ctxd->lower.data = htole32(
				    (adapter->txd_cmd | txd_lower | (u16)
				    desc_array.descriptor[counter].length));
				ctxd->upper.data =
				    htole32((txd_upper));
				last = i;
				if (++i == adapter->num_tx_desc)
                                         i = 0;
				tx_buffer->m_head = NULL;
				tx_buffer->next_eop = -1;
				txd_used++;
                        }
		} else {
			tx_buffer = &adapter->tx_buffer_area[i];
			ctxd = &adapter->tx_desc_base[i];
			seg_addr = segs[j].ds_addr;
			seg_len  = segs[j].ds_len;
			ctxd->buffer_addr = htole64(seg_addr);
			ctxd->lower.data = htole32(
			adapter->txd_cmd | txd_lower | seg_len);
			ctxd->upper.data =
			    htole32(txd_upper);
			last = i;
			if (++i == adapter->num_tx_desc)
				i = 0;
			tx_buffer->m_head = NULL;
			tx_buffer->next_eop = -1;
		}
	}

	adapter->next_avail_tx_desc = i;

	if (adapter->pcix_82544)
		adapter->num_tx_desc_avail -= txd_used;
	else
		adapter->num_tx_desc_avail -= nsegs;

	if (m_head->m_flags & M_VLANTAG) {
		/* Set the vlan id. */
		ctxd->upper.fields.special =
		    htole16(m_head->m_pkthdr.ether_vtag);
                /* Tell hardware to add tag */
                ctxd->lower.data |= htole32(E1000_TXD_CMD_VLE);
        }

        tx_buffer->m_head = m_head;
	tx_buffer_mapped->map = tx_buffer->map;
	tx_buffer->map = map;
        bus_dmamap_sync(adapter->txtag, map, BUS_DMASYNC_PREWRITE);

        /*
         * Last Descriptor of Packet
	 * needs End Of Packet (EOP)
	 * and Report Status (RS)
         */
        ctxd->lower.data |=
	    htole32(E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
	/*
	 * Keep track in the first buffer which
	 * descriptor will be written back
	 */
	tx_buffer = &adapter->tx_buffer_area[first];
	tx_buffer->next_eop = last;
	adapter->watchdog_time = ticks;

	/*
	 * Advance the Transmit Descriptor Tail (TDT), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

#ifdef NIC_PARAVIRT
	if (adapter->csb) {
		adapter->csb->guest_tdt = i;
		/* XXX memory barrier ? */
 		if (adapter->csb->guest_csb_on &&
		    !(adapter->csb->host_need_txkick & 1)) {
			/* XXX maybe useless
			 * clean the ring. maybe do it before ?
			 * maybe a little bit of histeresys ?
			 */
			if (adapter->num_tx_desc_avail <= 64) {// XXX
				lem_txeof(adapter);
			}
			return (0);
		}
	}
#endif /* NIC_PARAVIRT */

#ifdef NIC_SEND_COMBINING
	if (adapter->sc_enable) {
		if (adapter->shadow_tdt & MIT_PENDING_INT) {
			/* signal intr and data pending */
			adapter->shadow_tdt = MIT_PENDING_TDT | (i & 0xffff);
			return (0);
		} else {
			adapter->shadow_tdt = MIT_PENDING_INT;
		}
	}
#endif /* NIC_SEND_COMBINING */

	if (adapter->hw.mac.type == e1000_82547 &&
	    adapter->link_duplex == HALF_DUPLEX)
		lem_82547_move_tail(adapter);
	else {
		E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), i);
		if (adapter->hw.mac.type == e1000_82547)
			lem_82547_update_fifo_head(adapter,
			    m_head->m_pkthdr.len);
	}

	return (0);
}

/*********************************************************************
 *
 * 82547 workaround to avoid controller hang in half-duplex environment.
 * The workaround is to avoid queuing a large packet that would span
 * the internal Tx FIFO ring boundary. We need to reset the FIFO pointers
 * in this case. We do that only when FIFO is quiescent.
 *
 **********************************************************************/
static void
lem_82547_move_tail(void *arg)
{
	struct adapter *adapter = arg;
	struct e1000_tx_desc *tx_desc;
	u16	hw_tdt, sw_tdt, length = 0;
	bool	eop = 0;

	EM_TX_LOCK_ASSERT(adapter);

	hw_tdt = E1000_READ_REG(&adapter->hw, E1000_TDT(0));
	sw_tdt = adapter->next_avail_tx_desc;
	
	while (hw_tdt != sw_tdt) {
		tx_desc = &adapter->tx_desc_base[hw_tdt];
		length += tx_desc->lower.flags.length;
		eop = tx_desc->lower.data & E1000_TXD_CMD_EOP;
		if (++hw_tdt == adapter->num_tx_desc)
			hw_tdt = 0;

		if (eop) {
			if (lem_82547_fifo_workaround(adapter, length)) {
				adapter->tx_fifo_wrk_cnt++;
				callout_reset(&adapter->tx_fifo_timer, 1,
					lem_82547_move_tail, adapter);
				break;
			}
			E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), hw_tdt);
			lem_82547_update_fifo_head(adapter, length);
			length = 0;
		}
	}	
}

static int
lem_82547_fifo_workaround(struct adapter *adapter, int len)
{	
	int fifo_space, fifo_pkt_len;

	fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);

	if (adapter->link_duplex == HALF_DUPLEX) {
		fifo_space = adapter->tx_fifo_size - adapter->tx_fifo_head;

		if (fifo_pkt_len >= (EM_82547_PKT_THRESH + fifo_space)) {
			if (lem_82547_tx_fifo_reset(adapter))
				return (0);
			else
				return (1);
		}
	}

	return (0);
}

static void
lem_82547_update_fifo_head(struct adapter *adapter, int len)
{
	int fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);
	
	/* tx_fifo_head is always 16 byte aligned */
	adapter->tx_fifo_head += fifo_pkt_len;
	if (adapter->tx_fifo_head >= adapter->tx_fifo_size) {
		adapter->tx_fifo_head -= adapter->tx_fifo_size;
	}
}


static int
lem_82547_tx_fifo_reset(struct adapter *adapter)
{
	u32 tctl;

	if ((E1000_READ_REG(&adapter->hw, E1000_TDT(0)) ==
	    E1000_READ_REG(&adapter->hw, E1000_TDH(0))) &&
	    (E1000_READ_REG(&adapter->hw, E1000_TDFT) == 
	    E1000_READ_REG(&adapter->hw, E1000_TDFH)) &&
	    (E1000_READ_REG(&adapter->hw, E1000_TDFTS) ==
	    E1000_READ_REG(&adapter->hw, E1000_TDFHS)) &&
	    (E1000_READ_REG(&adapter->hw, E1000_TDFPC) == 0)) {
		/* Disable TX unit */
		tctl = E1000_READ_REG(&adapter->hw, E1000_TCTL);
		E1000_WRITE_REG(&adapter->hw, E1000_TCTL,
		    tctl & ~E1000_TCTL_EN);

		/* Reset FIFO pointers */
		E1000_WRITE_REG(&adapter->hw, E1000_TDFT,
		    adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, E1000_TDFH,
		    adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, E1000_TDFTS,
		    adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, E1000_TDFHS,
		    adapter->tx_head_addr);

		/* Re-enable TX unit */
		E1000_WRITE_REG(&adapter->hw, E1000_TCTL, tctl);
		E1000_WRITE_FLUSH(&adapter->hw);

		adapter->tx_fifo_head = 0;
		adapter->tx_fifo_reset_cnt++;

		return (TRUE);
	}
	else {
		return (FALSE);
	}
}

static void
lem_set_promisc(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	u32		reg_rctl;

	reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		/* Turn this on if you want to see bad packets */
		if (lem_debug_sbp)
			reg_rctl |= E1000_RCTL_SBP;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	}
}

static void
lem_disable_promisc(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	u32		reg_rctl;
	int		mcnt = 0;

	reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	reg_rctl &=  (~E1000_RCTL_UPE);
	if (ifp->if_flags & IFF_ALLMULTI)
		mcnt = MAX_NUM_MULTICAST_ADDRESSES;
	else {
		struct  ifmultiaddr *ifma;
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
			mcnt++;
		}
#if __FreeBSD_version < 800000
		IF_ADDR_UNLOCK(ifp);
#else
		if_maddr_runlock(ifp);
#endif
	}
	/* Don't disable if in MAX groups */
	if (mcnt < MAX_NUM_MULTICAST_ADDRESSES)
		reg_rctl &=  (~E1000_RCTL_MPE);
	reg_rctl &=  (~E1000_RCTL_SBP);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
lem_set_multi(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct ifmultiaddr *ifma;
	u32 reg_rctl = 0;
	u8  *mta; /* Multicast array memory */
	int mcnt = 0;

	IOCTL_DEBUGOUT("lem_set_multi: begin");

	mta = adapter->mta;
	bzero(mta, sizeof(u8) * ETH_ADDR_LEN * MAX_NUM_MULTICAST_ADDRESSES);

	if (adapter->hw.mac.type == e1000_82542 && 
	    adapter->hw.revision_id == E1000_REVISION_2) {
		reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		if (adapter->hw.bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_clear_mwi(&adapter->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
		msec_delay(5);
	}

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

	if (adapter->hw.mac.type == e1000_82542 && 
	    adapter->hw.revision_id == E1000_REVISION_2) {
		reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
		msec_delay(5);
		if (adapter->hw.bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_set_mwi(&adapter->hw);
	}
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status and updates statistics.
 *
 **********************************************************************/

static void
lem_local_timer(void *arg)
{
	struct adapter	*adapter = arg;

	EM_CORE_LOCK_ASSERT(adapter);

	lem_update_link_status(adapter);
	lem_update_stats_counters(adapter);

	lem_smartspeed(adapter);

#ifdef NIC_PARAVIRT
	/* recover space if needed */
	if (adapter->csb && adapter->csb->guest_csb_on &&
	    (adapter->watchdog_check == TRUE) &&
	    (ticks - adapter->watchdog_time > EM_WATCHDOG) &&
	    (adapter->num_tx_desc_avail != adapter->num_tx_desc) ) {
		lem_txeof(adapter);
		/*
		 * lem_txeof() normally (except when space in the queue
		 * runs low XXX) cleans watchdog_check so that
		 * we do not hung.
		 */
	}
#endif /* NIC_PARAVIRT */
	/*
	 * We check the watchdog: the time since
	 * the last TX descriptor was cleaned.
	 * This implies a functional TX engine.
	 */
	if ((adapter->watchdog_check == TRUE) &&
	    (ticks - adapter->watchdog_time > EM_WATCHDOG))
		goto hung;

	callout_reset(&adapter->timer, hz, lem_local_timer, adapter);
	return;
hung:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	lem_init_locked(adapter);
}

static void
lem_update_link_status(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct ifnet *ifp = adapter->ifp;
	device_t dev = adapter->dev;
	u32 link_check = 0;

	/* Get the cached link value or read phy for real */
	switch (hw->phy.media_type) {
	case e1000_media_type_copper:
		if (hw->mac.get_link_status) {
			/* Do the work to read phy */
			e1000_check_for_link(hw);
			link_check = !hw->mac.get_link_status;
			if (link_check) /* ESB2 fix */
				e1000_cfg_on_link_up(hw);
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
	default:
	case e1000_media_type_unknown:
		break;
	}

	/* Now check for a transition */
	if (link_check && (adapter->link_active == 0)) {
		e1000_get_speed_and_duplex(hw, &adapter->link_speed,
		    &adapter->link_duplex);
		if (bootverbose)
			device_printf(dev, "Link is up %d Mbps %s\n",
			    adapter->link_speed,
			    ((adapter->link_duplex == FULL_DUPLEX) ?
			    "Full Duplex" : "Half Duplex"));
		adapter->link_active = 1;
		adapter->smartspeed = 0;
		ifp->if_baudrate = adapter->link_speed * 1000000;
		if_link_state_change(ifp, LINK_STATE_UP);
	} else if (!link_check && (adapter->link_active == 1)) {
		ifp->if_baudrate = adapter->link_speed = 0;
		adapter->link_duplex = 0;
		if (bootverbose)
			device_printf(dev, "Link is Down\n");
		adapter->link_active = 0;
		/* Link down, disable watchdog */
		adapter->watchdog_check = FALSE;
		if_link_state_change(ifp, LINK_STATE_DOWN);
	}
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 *  This routine should always be called with BOTH the CORE
 *  and TX locks.
 **********************************************************************/

static void
lem_stop(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;

	EM_CORE_LOCK_ASSERT(adapter);
	EM_TX_LOCK_ASSERT(adapter);

	INIT_DEBUGOUT("lem_stop: begin");

	lem_disable_intr(adapter);
	callout_stop(&adapter->timer);
	callout_stop(&adapter->tx_fifo_timer);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	e1000_reset_hw(&adapter->hw);
	if (adapter->hw.mac.type >= e1000_82544)
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
lem_identify_hardware(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	/* Make sure our PCI config space has the necessary stuff set */
	pci_enable_busmaster(dev);
	adapter->hw.bus.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);

	/* Save off the information about this board */
	adapter->hw.vendor_id = pci_get_vendor(dev);
	adapter->hw.device_id = pci_get_device(dev);
	adapter->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	adapter->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	adapter->hw.subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Do Shared Code Init and Setup */
	if (e1000_set_mac_type(&adapter->hw)) {
		device_printf(dev, "Setup init failure\n");
		return;
	}
}

static int
lem_allocate_pci_resources(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	int		val, rid, error = E1000_SUCCESS;

	rid = PCIR_BAR(0);
	adapter->memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (adapter->memory == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	adapter->osdep.mem_bus_space_tag =
	    rman_get_bustag(adapter->memory);
	adapter->osdep.mem_bus_space_handle =
	    rman_get_bushandle(adapter->memory);
	adapter->hw.hw_addr = (u8 *)&adapter->osdep.mem_bus_space_handle;

	/* Only older adapters use IO mapping */
	if (adapter->hw.mac.type > e1000_82543) {
		/* Figure our where our IO BAR is ? */
		for (rid = PCIR_BAR(0); rid < PCIR_CIS;) {
			val = pci_read_config(dev, rid, 4);
			if (EM_BAR_TYPE(val) == EM_BAR_TYPE_IO) {
				adapter->io_rid = rid;
				break;
			}
			rid += 4;
			/* check for 64bit BAR */
			if (EM_BAR_MEM_TYPE(val) == EM_BAR_MEM_TYPE_64BIT)
				rid += 4;
		}
		if (rid >= PCIR_CIS) {
			device_printf(dev, "Unable to locate IO BAR\n");
			return (ENXIO);
		}
		adapter->ioport = bus_alloc_resource_any(dev,
		    SYS_RES_IOPORT, &adapter->io_rid, RF_ACTIVE);
		if (adapter->ioport == NULL) {
			device_printf(dev, "Unable to allocate bus resource: "
			    "ioport\n");
			return (ENXIO);
		}
		adapter->hw.io_base = 0;
		adapter->osdep.io_bus_space_tag =
		    rman_get_bustag(adapter->ioport);
		adapter->osdep.io_bus_space_handle =
		    rman_get_bushandle(adapter->ioport);
	}

	adapter->hw.back = &adapter->osdep;

	return (error);
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
int
lem_allocate_irq(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int error, rid = 0;

	/* Manually turn off all interrupts */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	/* We allocate a single interrupt resource */
	adapter->res[0] = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res[0] == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

	/* Do Legacy setup? */
	if (lem_use_legacy_irq) {
		if ((error = bus_setup_intr(dev, adapter->res[0],
	    	    INTR_TYPE_NET | INTR_MPSAFE, NULL, lem_intr, adapter,
	    	    &adapter->tag[0])) != 0) {
			device_printf(dev,
			    "Failed to register interrupt handler");
			return (error);
		}
		return (0);
	}

	/*
	 * Use a Fast interrupt and the associated
	 * deferred processing contexts.
	 */
	TASK_INIT(&adapter->rxtx_task, 0, lem_handle_rxtx, adapter);
	TASK_INIT(&adapter->link_task, 0, lem_handle_link, adapter);
	adapter->tq = taskqueue_create_fast("lem_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(adapter->dev));
	if ((error = bus_setup_intr(dev, adapter->res[0],
	    INTR_TYPE_NET, lem_irq_fast, NULL, adapter,
	    &adapter->tag[0])) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
			    "handler: %d\n", error);
		taskqueue_free(adapter->tq);
		adapter->tq = NULL;
		return (error);
	}
	
	return (0);
}


static void
lem_free_pci_resources(struct adapter *adapter)
{
	device_t dev = adapter->dev;


	if (adapter->tag[0] != NULL) {
		bus_teardown_intr(dev, adapter->res[0],
		    adapter->tag[0]);
		adapter->tag[0] = NULL;
	}

	if (adapter->res[0] != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ,
		    0, adapter->res[0]);
	}

	if (adapter->memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->memory);

	if (adapter->ioport != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    adapter->io_rid, adapter->ioport);
}


/*********************************************************************
 *
 *  Initialize the hardware to a configuration
 *  as specified by the adapter structure.
 *
 **********************************************************************/
static int
lem_hardware_init(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	u16 	rx_buffer_size;

	INIT_DEBUGOUT("lem_hardware_init: begin");

	/* Issue a global reset */
	e1000_reset_hw(&adapter->hw);

	/* When hardware is reset, fifo_head is also reset */
	adapter->tx_fifo_head = 0;

	/*
	 * These parameters control the automatic generation (Tx) and
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two frames to be
	 *   received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buffer_size = ((E1000_READ_REG(&adapter->hw, E1000_PBA) &
	    0xffff) << 10 );

	adapter->hw.fc.high_water = rx_buffer_size -
	    roundup2(adapter->max_frame_size, 1024);
	adapter->hw.fc.low_water = adapter->hw.fc.high_water - 1500;

	adapter->hw.fc.pause_time = EM_FC_PAUSE_TIME;
	adapter->hw.fc.send_xon = TRUE;

        /* Set Flow control, use the tunable location if sane */
        if ((lem_fc_setting >= 0) && (lem_fc_setting < 4))
                adapter->hw.fc.requested_mode = lem_fc_setting;
        else
                adapter->hw.fc.requested_mode = e1000_fc_none;

	if (e1000_init_hw(&adapter->hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed\n");
		return (EIO);
	}

	e1000_check_for_link(&adapter->hw);

	return (0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
lem_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("lem_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_init =  lem_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = lem_ioctl;
	ifp->if_start = lem_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, adapter->num_tx_desc - 1);
	ifp->if_snd.ifq_drv_maxlen = adapter->num_tx_desc - 1;
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, adapter->hw.mac.addr);

	ifp->if_capabilities = ifp->if_capenable = 0;

	if (adapter->hw.mac.type >= e1000_82543) {
		ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
		ifp->if_capenable |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
	}

	/*
	 * Tell the upper layer(s) we support long frames.
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

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/* Enable only WOL MAGIC by default */
	if (adapter->wol) {
		ifp->if_capabilities |= IFCAP_WOL;
		ifp->if_capenable |= IFCAP_WOL_MAGIC;
	}
		
	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK,
	    lem_media_change, lem_media_status);
	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes)) {
		u_char fiber_type = IFM_1000_SX;	/* default type */

		if (adapter->hw.mac.type == e1000_82545)
			fiber_type = IFM_1000_LX;
		ifmedia_add(&adapter->media, IFM_ETHER | fiber_type | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&adapter->media, IFM_ETHER | fiber_type, 0, NULL);
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


/*********************************************************************
 *
 *  Workaround for SmartSpeed on 82541 and 82547 controllers
 *
 **********************************************************************/
static void
lem_smartspeed(struct adapter *adapter)
{
	u16 phy_tmp;

	if (adapter->link_active || (adapter->hw.phy.type != e1000_phy_igp) ||
	    adapter->hw.mac.autoneg == 0 ||
	    (adapter->hw.phy.autoneg_advertised & ADVERTISE_1000_FULL) == 0)
		return;

	if (adapter->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		e1000_read_phy_reg(&adapter->hw, PHY_1000T_STATUS, &phy_tmp);
		if (!(phy_tmp & SR_1000T_MS_CONFIG_FAULT))
			return;
		e1000_read_phy_reg(&adapter->hw, PHY_1000T_STATUS, &phy_tmp);
		if (phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			e1000_read_phy_reg(&adapter->hw,
			    PHY_1000T_CTRL, &phy_tmp);
			if(phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				e1000_write_phy_reg(&adapter->hw,
				    PHY_1000T_CTRL, phy_tmp);
				adapter->smartspeed++;
				if(adapter->hw.mac.autoneg &&
				   !e1000_copper_link_autoneg(&adapter->hw) &&
				   !e1000_read_phy_reg(&adapter->hw,
				    PHY_CONTROL, &phy_tmp)) {
					phy_tmp |= (MII_CR_AUTO_NEG_EN |
						    MII_CR_RESTART_AUTO_NEG);
					e1000_write_phy_reg(&adapter->hw,
					    PHY_CONTROL, phy_tmp);
				}
			}
		}
		return;
	} else if(adapter->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		e1000_read_phy_reg(&adapter->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		e1000_write_phy_reg(&adapter->hw, PHY_1000T_CTRL, phy_tmp);
		if(adapter->hw.mac.autoneg &&
		   !e1000_copper_link_autoneg(&adapter->hw) &&
		   !e1000_read_phy_reg(&adapter->hw, PHY_CONTROL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			e1000_write_phy_reg(&adapter->hw, PHY_CONTROL, phy_tmp);
		}
	}
	/* Restart process after EM_SMARTSPEED_MAX iterations */
	if(adapter->smartspeed++ == EM_SMARTSPEED_MAX)
		adapter->smartspeed = 0;
}


/*
 * Manage DMA'able memory.
 */
static void
lem_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs[0].ds_addr;
}

static int
lem_dma_malloc(struct adapter *adapter, bus_size_t size,
        struct em_dma_alloc *dma, int mapflags)
{
	int error;

	error = bus_dma_tag_create(bus_get_dma_tag(adapter->dev), /* parent */
				EM_DBA_ALIGN, 0,	/* alignment, bounds */
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
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT, &dma->dma_map);
	if (error) {
		device_printf(adapter->dev,
		    "%s: bus_dmamem_alloc(%ju) failed: %d\n",
		    __func__, (uintmax_t)size, error);
		goto fail_2;
	}

	dma->dma_paddr = 0;
	error = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
	    size, lem_dmamap_cb, &dma->dma_paddr, mapflags | BUS_DMA_NOWAIT);
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
	dma->dma_tag = NULL;

	return (error);
}

static void
lem_dma_free(struct adapter *adapter, struct em_dma_alloc *dma)
{
	if (dma->dma_tag == NULL)
		return;
	if (dma->dma_paddr != 0) {
		bus_dmamap_sync(dma->dma_tag, dma->dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		dma->dma_paddr = 0;
	}
	if (dma->dma_vaddr != NULL) {
		bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
		dma->dma_vaddr = NULL;
	}
	bus_dma_tag_destroy(dma->dma_tag);
	dma->dma_tag = NULL;
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire.
 *
 **********************************************************************/
static int
lem_allocate_transmit_structures(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct em_buffer *tx_buffer;
	int error;

	/*
	 * Create DMA tags for tx descriptors
	 */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				MCLBYTES * EM_MAX_SCATTER,	/* maxsize */
				EM_MAX_SCATTER,		/* nsegments */
				MCLBYTES,		/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&adapter->txtag)) != 0) {
		device_printf(dev, "Unable to allocate TX DMA tag\n");
		goto fail;
	}

	adapter->tx_buffer_area = malloc(sizeof(struct em_buffer) *
	    adapter->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (adapter->tx_buffer_area == NULL) {
		device_printf(dev, "Unable to allocate tx_buffer memory\n");
		error = ENOMEM;
		goto fail;
	}

	/* Create the descriptor buffer dma maps */
	for (int i = 0; i < adapter->num_tx_desc; i++) {
		tx_buffer = &adapter->tx_buffer_area[i];
		error = bus_dmamap_create(adapter->txtag, 0, &tx_buffer->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
		tx_buffer->next_eop = -1;
	}

	return (0);
fail:
	lem_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  (Re)Initialize transmit structures.
 *
 **********************************************************************/
static void
lem_setup_transmit_structures(struct adapter *adapter)
{
	struct em_buffer *tx_buffer;
#ifdef DEV_NETMAP
	/* we are already locked */
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_TX, 0, 0);
#endif /* DEV_NETMAP */

	/* Clear the old ring contents */
	bzero(adapter->tx_desc_base,
	    (sizeof(struct e1000_tx_desc)) * adapter->num_tx_desc);

	/* Free any existing TX buffers */
	for (int i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
		tx_buffer = &adapter->tx_buffer_area[i];
		bus_dmamap_sync(adapter->txtag, tx_buffer->map,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(adapter->txtag, tx_buffer->map);
		m_freem(tx_buffer->m_head);
		tx_buffer->m_head = NULL;
#ifdef DEV_NETMAP
		if (slot) {
			/* the i-th NIC entry goes to slot si */
			int si = netmap_idx_n2k(&na->tx_rings[0], i);
			uint64_t paddr;
			void *addr;

			addr = PNMB(na, slot + si, &paddr);
			adapter->tx_desc_base[i].buffer_addr = htole64(paddr);
			/* reload the map for netmap mode */
			netmap_load_map(na, adapter->txtag, tx_buffer->map, addr);
		}
#endif /* DEV_NETMAP */
		tx_buffer->next_eop = -1;
	}

	/* Reset state */
	adapter->last_hw_offload = 0;
	adapter->next_avail_tx_desc = 0;
	adapter->next_tx_to_clean = 0;
	adapter->num_tx_desc_avail = adapter->num_tx_desc;

	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
lem_initialize_transmit_unit(struct adapter *adapter)
{
	u32	tctl, tipg = 0;
	u64	bus_addr;

	 INIT_DEBUGOUT("lem_initialize_transmit_unit: begin");
	/* Setup the Base and Length of the Tx Descriptor Ring */
	bus_addr = adapter->txdma.dma_paddr;
	E1000_WRITE_REG(&adapter->hw, E1000_TDLEN(0),
	    adapter->num_tx_desc * sizeof(struct e1000_tx_desc));
	E1000_WRITE_REG(&adapter->hw, E1000_TDBAH(0),
	    (u32)(bus_addr >> 32));
	E1000_WRITE_REG(&adapter->hw, E1000_TDBAL(0),
	    (u32)bus_addr);
	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), 0);
	E1000_WRITE_REG(&adapter->hw, E1000_TDH(0), 0);

	HW_DEBUGOUT2("Base = %x, Length = %x\n",
	    E1000_READ_REG(&adapter->hw, E1000_TDBAL(0)),
	    E1000_READ_REG(&adapter->hw, E1000_TDLEN(0)));

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (adapter->hw.mac.type) {
	case e1000_82542:
		tipg = DEFAULT_82542_TIPG_IPGT;
		tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
		    (adapter->hw.phy.media_type ==
		    e1000_media_type_internal_serdes))
			tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}

	E1000_WRITE_REG(&adapter->hw, E1000_TIPG, tipg);
	E1000_WRITE_REG(&adapter->hw, E1000_TIDV, adapter->tx_int_delay.value);
	if(adapter->hw.mac.type >= e1000_82540)
		E1000_WRITE_REG(&adapter->hw, E1000_TADV,
		    adapter->tx_abs_int_delay.value);

	/* Program the Transmit Control Register */
	tctl = E1000_READ_REG(&adapter->hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(&adapter->hw, E1000_TCTL, tctl);

	/* Setup Transmit Descriptor Base Settings */   
	adapter->txd_cmd = E1000_TXD_CMD_IFCS;

	if (adapter->tx_int_delay.value > 0)
		adapter->txd_cmd |= E1000_TXD_CMD_IDE;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
static void
lem_free_transmit_structures(struct adapter *adapter)
{
	struct em_buffer *tx_buffer;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (adapter->tx_buffer_area != NULL) {
		for (int i = 0; i < adapter->num_tx_desc; i++) {
			tx_buffer = &adapter->tx_buffer_area[i];
			if (tx_buffer->m_head != NULL) {
				bus_dmamap_sync(adapter->txtag, tx_buffer->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(adapter->txtag,
				    tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			} else if (tx_buffer->map != NULL)
				bus_dmamap_unload(adapter->txtag,
				    tx_buffer->map);
			if (tx_buffer->map != NULL) {
				bus_dmamap_destroy(adapter->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		}
	}
	if (adapter->tx_buffer_area != NULL) {
		free(adapter->tx_buffer_area, M_DEVBUF);
		adapter->tx_buffer_area = NULL;
	}
	if (adapter->txtag != NULL) {
		bus_dma_tag_destroy(adapter->txtag);
		adapter->txtag = NULL;
	}
}

/*********************************************************************
 *
 *  The offload context needs to be set when we transfer the first
 *  packet of a particular protocol (TCP/UDP). This routine has been
 *  enhanced to deal with inserted VLAN headers, and IPV6 (not complete)
 *
 *  Added back the old method of keeping the current context type
 *  and not setting if unnecessary, as this is reported to be a
 *  big performance win.  -jfv
 **********************************************************************/
static void
lem_transmit_checksum_setup(struct adapter *adapter, struct mbuf *mp,
    u32 *txd_upper, u32 *txd_lower)
{
	struct e1000_context_desc *TXD = NULL;
	struct em_buffer *tx_buffer;
	struct ether_vlan_header *eh;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6;
	int curr_txd, ehdrlen;
	u32 cmd, hdr_len, ip_hlen;
	u16 etype;
	u8 ipproto;


	cmd = hdr_len = ipproto = 0;
	*txd_upper = *txd_lower = 0;
	curr_txd = adapter->next_avail_tx_desc;

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

	/*
	 * We only support TCP/UDP for IPv4 and IPv6 for the moment.
	 * TODO: Support SCTP too when it hits the tree.
	 */
	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(mp->m_data + ehdrlen);
		ip_hlen = ip->ip_hl << 2;

		/* Setup of IP header checksum. */
		if (mp->m_pkthdr.csum_flags & CSUM_IP) {
			/*
			 * Start offset for header checksum calculation.
			 * End offset for header checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD = (struct e1000_context_desc *)
			    &adapter->tx_desc_base[curr_txd];
			TXD->lower_setup.ip_fields.ipcss = ehdrlen;
			TXD->lower_setup.ip_fields.ipcse =
			    htole16(ehdrlen + ip_hlen);
			TXD->lower_setup.ip_fields.ipcso =
			    ehdrlen + offsetof(struct ip, ip_sum);
			cmd |= E1000_TXD_CMD_IP;
			*txd_upper |= E1000_TXD_POPTS_IXSM << 8;
		}

		hdr_len = ehdrlen + ip_hlen;
		ipproto = ip->ip_p;

		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		ip_hlen = sizeof(struct ip6_hdr); /* XXX: No header stacking. */

		/* IPv6 doesn't have a header checksum. */

		hdr_len = ehdrlen + ip_hlen;
		ipproto = ip6->ip6_nxt;
		break;

	default:
		return;
	}

	switch (ipproto) {
	case IPPROTO_TCP:
		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
			/* no need for context if already set */
			if (adapter->last_hw_offload == CSUM_TCP)
				return;
			adapter->last_hw_offload = CSUM_TCP;
			/*
			 * Start offset for payload checksum calculation.
			 * End offset for payload checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD = (struct e1000_context_desc *)
			    &adapter->tx_desc_base[curr_txd];
			TXD->upper_setup.tcp_fields.tucss = hdr_len;
			TXD->upper_setup.tcp_fields.tucse = htole16(0);
			TXD->upper_setup.tcp_fields.tucso =
			    hdr_len + offsetof(struct tcphdr, th_sum);
			cmd |= E1000_TXD_CMD_TCP;
		}
		break;
	case IPPROTO_UDP:
	{
		if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
			/* no need for context if already set */
			if (adapter->last_hw_offload == CSUM_UDP)
				return;
			adapter->last_hw_offload = CSUM_UDP;
			/*
			 * Start offset for header checksum calculation.
			 * End offset for header checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD = (struct e1000_context_desc *)
			    &adapter->tx_desc_base[curr_txd];
			TXD->upper_setup.tcp_fields.tucss = hdr_len;
			TXD->upper_setup.tcp_fields.tucse = htole16(0);
			TXD->upper_setup.tcp_fields.tucso =
			    hdr_len + offsetof(struct udphdr, uh_sum);
		}
		/* Fall Thru */
	}
	default:
		break;
	}

	if (TXD == NULL)
		return;
	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length =
	    htole32(adapter->txd_cmd | E1000_TXD_CMD_DEXT | cmd);
	tx_buffer = &adapter->tx_buffer_area[curr_txd];
	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++curr_txd == adapter->num_tx_desc)
		curr_txd = 0;

	adapter->num_tx_desc_avail--;
	adapter->next_avail_tx_desc = curr_txd;
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static void
lem_txeof(struct adapter *adapter)
{
        int first, last, done, num_avail;
        struct em_buffer *tx_buffer;
        struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = adapter->ifp;

	EM_TX_LOCK_ASSERT(adapter);

#ifdef DEV_NETMAP
	if (netmap_tx_irq(ifp, 0))
		return;
#endif /* DEV_NETMAP */
        if (adapter->num_tx_desc_avail == adapter->num_tx_desc)
                return;

        num_avail = adapter->num_tx_desc_avail;
        first = adapter->next_tx_to_clean;
        tx_desc = &adapter->tx_desc_base[first];
        tx_buffer = &adapter->tx_buffer_area[first];
	last = tx_buffer->next_eop;
        eop_desc = &adapter->tx_desc_base[last];

	/*
	 * What this does is get the index of the
	 * first descriptor AFTER the EOP of the 
	 * first packet, that way we can do the
	 * simple comparison on the inner while loop.
	 */
	if (++last == adapter->num_tx_desc)
 		last = 0;
	done = last;

        bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);

        while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
                	tx_desc->upper.data = 0;
                	tx_desc->lower.data = 0;
                	tx_desc->buffer_addr = 0;
                	++num_avail;

			if (tx_buffer->m_head) {
				ifp->if_opackets++;
				bus_dmamap_sync(adapter->txtag,
				    tx_buffer->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(adapter->txtag,
				    tx_buffer->map);

                        	m_freem(tx_buffer->m_head);
                        	tx_buffer->m_head = NULL;
                	}
			tx_buffer->next_eop = -1;
			adapter->watchdog_time = ticks;

	                if (++first == adapter->num_tx_desc)
				first = 0;

	                tx_buffer = &adapter->tx_buffer_area[first];
			tx_desc = &adapter->tx_desc_base[first];
		}
		/* See if we can continue to the next packet */
		last = tx_buffer->next_eop;
		if (last != -1) {
        		eop_desc = &adapter->tx_desc_base[last];
			/* Get new done point */
			if (++last == adapter->num_tx_desc) last = 0;
			done = last;
		} else
			break;
        }
        bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

        adapter->next_tx_to_clean = first;
        adapter->num_tx_desc_avail = num_avail;

#ifdef NIC_SEND_COMBINING
	if ((adapter->shadow_tdt & MIT_PENDING_TDT) == MIT_PENDING_TDT) {
		/* a tdt write is pending, do it */
		E1000_WRITE_REG(&adapter->hw, E1000_TDT(0),
			0xffff & adapter->shadow_tdt);
		adapter->shadow_tdt = MIT_PENDING_INT;
	} else {
		adapter->shadow_tdt = 0; // disable
	}
#endif /* NIC_SEND_COMBINING */
        /*
         * If we have enough room, clear IFF_DRV_OACTIVE to
         * tell the stack that it is OK to send packets.
         * If there are no pending descriptors, clear the watchdog.
         */
        if (adapter->num_tx_desc_avail > EM_TX_CLEANUP_THRESHOLD) {                
                ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
#ifdef NIC_PARAVIRT
		if (adapter->csb) { // XXX also csb_on ?
			adapter->csb->guest_need_txkick = 2; /* acked */
			// XXX memory barrier
		}
#endif /* NIC_PARAVIRT */
                if (adapter->num_tx_desc_avail == adapter->num_tx_desc) {
			adapter->watchdog_check = FALSE;
			return;
		} 
        }
}

/*********************************************************************
 *
 *  When Link is lost sometimes there is work still in the TX ring
 *  which may result in a watchdog, rather than allow that we do an
 *  attempted cleanup and then reinit here. Note that this has been
 *  seens mostly with fiber adapters.
 *
 **********************************************************************/
static void
lem_tx_purge(struct adapter *adapter)
{
	if ((!adapter->link_active) && (adapter->watchdog_check)) {
		EM_TX_LOCK(adapter);
		lem_txeof(adapter);
		EM_TX_UNLOCK(adapter);
		if (adapter->watchdog_check) /* Still outstanding? */
			lem_init_locked(adapter);
	}
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
lem_get_buf(struct adapter *adapter, int i)
{
	struct mbuf		*m;
	bus_dma_segment_t	segs[1];
	bus_dmamap_t		map;
	struct em_buffer	*rx_buffer;
	int			error, nsegs;

	m = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m == NULL) {
		adapter->mbuf_cluster_failed++;
		return (ENOBUFS);
	}
	m->m_len = m->m_pkthdr.len = MCLBYTES;

	if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
		m_adj(m, ETHER_ALIGN);

	/*
	 * Using memory from the mbuf cluster pool, invoke the
	 * bus_dma machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load_mbuf_sg(adapter->rxtag,
	    adapter->rx_sparemap, m, segs, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		m_free(m);
		return (error);
	}

	/* If nsegs is wrong then the stack is corrupt. */
	KASSERT(nsegs == 1, ("Too many segments returned!"));

	rx_buffer = &adapter->rx_buffer_area[i];
	if (rx_buffer->m_head != NULL)
		bus_dmamap_unload(adapter->rxtag, rx_buffer->map);

	map = rx_buffer->map;
	rx_buffer->map = adapter->rx_sparemap;
	adapter->rx_sparemap = map;
	bus_dmamap_sync(adapter->rxtag, rx_buffer->map, BUS_DMASYNC_PREREAD);
	rx_buffer->m_head = m;

	adapter->rx_desc_base[i].buffer_addr = htole64(segs[0].ds_addr);
	return (0);
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
lem_allocate_receive_structures(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct em_buffer *rx_buffer;
	int i, error;

	adapter->rx_buffer_area = malloc(sizeof(struct em_buffer) *
	    adapter->num_rx_desc, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (adapter->rx_buffer_area == NULL) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		return (ENOMEM);
	}

	error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				MCLBYTES,		/* maxsize */
				1,			/* nsegments */
				MCLBYTES,		/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&adapter->rxtag);
	if (error) {
		device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
		    __func__, error);
		goto fail;
	}

	/* Create the spare map (used by getbuf) */
	error = bus_dmamap_create(adapter->rxtag, 0, &adapter->rx_sparemap);
	if (error) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
		    __func__, error);
		goto fail;
	}

	rx_buffer = adapter->rx_buffer_area;
	for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(adapter->rxtag, 0, &rx_buffer->map);
		if (error) {
			device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
			    __func__, error);
			goto fail;
		}
	}

	return (0);

fail:
	lem_free_receive_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  (Re)initialize receive structures.
 *
 **********************************************************************/
static int
lem_setup_receive_structures(struct adapter *adapter)
{
	struct em_buffer *rx_buffer;
	int i, error;
#ifdef DEV_NETMAP
	/* we are already under lock */
	struct netmap_adapter *na = NA(adapter->ifp);
	struct netmap_slot *slot = netmap_reset(na, NR_RX, 0, 0);
#endif

	/* Reset descriptor ring */
	bzero(adapter->rx_desc_base,
	    (sizeof(struct e1000_rx_desc)) * adapter->num_rx_desc);

	/* Free current RX buffers. */
	rx_buffer = adapter->rx_buffer_area;
	for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
		if (rx_buffer->m_head != NULL) {
			bus_dmamap_sync(adapter->rxtag, rx_buffer->map,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(adapter->rxtag, rx_buffer->map);
			m_freem(rx_buffer->m_head);
			rx_buffer->m_head = NULL;
		}
        }

	/* Allocate new ones. */
	for (i = 0; i < adapter->num_rx_desc; i++) {
#ifdef DEV_NETMAP
		if (slot) {
			/* the i-th NIC entry goes to slot si */
			int si = netmap_idx_n2k(&na->rx_rings[0], i);
			uint64_t paddr;
			void *addr;

			addr = PNMB(na, slot + si, &paddr);
			netmap_load_map(na, adapter->rxtag, rx_buffer->map, addr);
			/* Update descriptor */
			adapter->rx_desc_base[i].buffer_addr = htole64(paddr);
			continue;
		}
#endif /* DEV_NETMAP */
		error = lem_get_buf(adapter, i);
		if (error)
                        return (error);
	}

	/* Setup our descriptor pointers */
	adapter->next_rx_desc_to_check = 0;
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/

static void
lem_initialize_receive_unit(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	u64	bus_addr;
	u32	rctl, rxcsum;

	INIT_DEBUGOUT("lem_initialize_receive_unit: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	if (adapter->hw.mac.type >= e1000_82540) {
		E1000_WRITE_REG(&adapter->hw, E1000_RADV,
		    adapter->rx_abs_int_delay.value);
		/*
		 * Set the interrupt throttling rate. Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns)
		 */
		E1000_WRITE_REG(&adapter->hw, E1000_ITR, DEFAULT_ITR);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */
	bus_addr = adapter->rxdma.dma_paddr;
	E1000_WRITE_REG(&adapter->hw, E1000_RDLEN(0),
	    adapter->num_rx_desc * sizeof(struct e1000_rx_desc));
	E1000_WRITE_REG(&adapter->hw, E1000_RDBAH(0),
	    (u32)(bus_addr >> 32));
	E1000_WRITE_REG(&adapter->hw, E1000_RDBAL(0),
	    (u32)bus_addr);

	/* Setup the Receive Control Register */
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		   E1000_RCTL_RDMTS_HALF |
		   (adapter->hw.mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

	/* Make sure VLAN Filters are off */
	rctl &= ~E1000_RCTL_VFE;

	if (e1000_tbi_sbp_enabled_82543(&adapter->hw))
		rctl |= E1000_RCTL_SBP;
	else
		rctl &= ~E1000_RCTL_SBP;

	switch (adapter->rx_buffer_len) {
	default:
	case 2048:
		rctl |= E1000_RCTL_SZ_2048;
		break;
	case 4096:
		rctl |= E1000_RCTL_SZ_4096 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case 8192:
		rctl |= E1000_RCTL_SZ_8192 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case 16384:
		rctl |= E1000_RCTL_SZ_16384 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	}

	if (ifp->if_mtu > ETHERMTU)
		rctl |= E1000_RCTL_LPE;
	else
		rctl &= ~E1000_RCTL_LPE;

	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if ((adapter->hw.mac.type >= e1000_82543) &&
	    (ifp->if_capenable & IFCAP_RXCSUM)) {
		rxcsum = E1000_READ_REG(&adapter->hw, E1000_RXCSUM);
		rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&adapter->hw, E1000_RXCSUM, rxcsum);
	}

	/* Enable Receives */
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl);

	/*
	 * Setup the HW Rx Head and
	 * Tail Descriptor Pointers
	 */
	E1000_WRITE_REG(&adapter->hw, E1000_RDH(0), 0);
	rctl = adapter->num_rx_desc - 1; /* default RDT value */
#ifdef DEV_NETMAP
	/* preserve buffers already made available to clients */
	if (ifp->if_capenable & IFCAP_NETMAP)
		rctl -= nm_kr_rxspace(&NA(adapter->ifp)->rx_rings[0]);
#endif /* DEV_NETMAP */
	E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), rctl);

	return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
lem_free_receive_structures(struct adapter *adapter)
{
	struct em_buffer *rx_buffer;
	int i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (adapter->rx_sparemap) {
		bus_dmamap_destroy(adapter->rxtag, adapter->rx_sparemap);
		adapter->rx_sparemap = NULL;
	}

	/* Cleanup any existing buffers */
	if (adapter->rx_buffer_area != NULL) {
		rx_buffer = adapter->rx_buffer_area;
		for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->m_head != NULL) {
				bus_dmamap_sync(adapter->rxtag, rx_buffer->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(adapter->rxtag,
				    rx_buffer->map);
				m_freem(rx_buffer->m_head);
				rx_buffer->m_head = NULL;
			} else if (rx_buffer->map != NULL)
				bus_dmamap_unload(adapter->rxtag,
				    rx_buffer->map);
			if (rx_buffer->map != NULL) {
				bus_dmamap_destroy(adapter->rxtag,
				    rx_buffer->map);
				rx_buffer->map = NULL;
			}
		}
	}

	if (adapter->rx_buffer_area != NULL) {
		free(adapter->rx_buffer_area, M_DEVBUF);
		adapter->rx_buffer_area = NULL;
	}

	if (adapter->rxtag != NULL) {
		bus_dma_tag_destroy(adapter->rxtag);
		adapter->rxtag = NULL;
	}
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
 *  For polling we also now return the number of cleaned packets
 *********************************************************************/
static bool
lem_rxeof(struct adapter *adapter, int count, int *done)
{
	struct ifnet	*ifp = adapter->ifp;
	struct mbuf	*mp;
	u8		status = 0, accept_frame = 0, eop = 0;
	u16 		len, desc_len, prev_len_adj;
	int		i, rx_sent = 0;
	struct e1000_rx_desc   *current_desc;

#ifdef BATCH_DISPATCH
	struct mbuf *mh = NULL, *mt = NULL;
#endif /* BATCH_DISPATCH */
#ifdef NIC_PARAVIRT
	int retries = 0;
	struct paravirt_csb* csb = adapter->csb;
	int csb_mode = csb && csb->guest_csb_on;

	//ND("clear guest_rxkick at %d", adapter->next_rx_desc_to_check);
	if (csb_mode && csb->guest_need_rxkick)
		csb->guest_need_rxkick = 0;
#endif /* NIC_PARAVIRT */
	EM_RX_LOCK(adapter);

#ifdef BATCH_DISPATCH
    batch_again:
#endif /* BATCH_DISPATCH */
	i = adapter->next_rx_desc_to_check;
	current_desc = &adapter->rx_desc_base[i];
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

#ifdef DEV_NETMAP
	if (netmap_rx_irq(ifp, 0, &rx_sent)) {
		EM_RX_UNLOCK(adapter);
		return (FALSE);
	}
#endif /* DEV_NETMAP */

#if 1 // XXX optimization ?
	if (!((current_desc->status) & E1000_RXD_STAT_DD)) {
		if (done != NULL)
			*done = rx_sent;
		EM_RX_UNLOCK(adapter);
		return (FALSE);
	}
#endif /* 0 */

	while (count != 0 && ifp->if_drv_flags & IFF_DRV_RUNNING) {
		struct mbuf *m = NULL;

		status = current_desc->status;
		if ((status & E1000_RXD_STAT_DD) == 0) {
#ifdef NIC_PARAVIRT
		    if (csb_mode) {
			/* buffer not ready yet. Retry a few times before giving up */
			if (++retries <= adapter->rx_retries) {
				continue;
			}
			if (csb->guest_need_rxkick == 0) {
				// ND("set guest_rxkick at %d", adapter->next_rx_desc_to_check);
				csb->guest_need_rxkick = 1;
				// XXX memory barrier, status volatile ?
				continue; /* double check */
			}
		    }
		    /* no buffer ready, give up */
#endif /* NIC_PARAVIRT */
			break;
		}
#ifdef NIC_PARAVIRT
		if (csb_mode) {
			if (csb->guest_need_rxkick)
				// ND("clear again guest_rxkick at %d", adapter->next_rx_desc_to_check);
			csb->guest_need_rxkick = 0;
			retries = 0;
		}
#endif /* NIC_PARAVIRT */

		mp = adapter->rx_buffer_area[i].m_head;
		/*
		 * Can't defer bus_dmamap_sync(9) because TBI_ACCEPT
		 * needs to access the last received byte in the mbuf.
		 */
		bus_dmamap_sync(adapter->rxtag, adapter->rx_buffer_area[i].map,
		    BUS_DMASYNC_POSTREAD);

		accept_frame = 1;
		prev_len_adj = 0;
		desc_len = le16toh(current_desc->length);
		if (status & E1000_RXD_STAT_EOP) {
			count--;
			eop = 1;
			if (desc_len < ETHER_CRC_LEN) {
				len = 0;
				prev_len_adj = ETHER_CRC_LEN - desc_len;
			} else
				len = desc_len - ETHER_CRC_LEN;
		} else {
			eop = 0;
			len = desc_len;
		}

		if (current_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
			u8	last_byte;
			u32	pkt_len = desc_len;

			if (adapter->fmp != NULL)
				pkt_len += adapter->fmp->m_pkthdr.len;

			last_byte = *(mtod(mp, caddr_t) + desc_len - 1);			
			if (TBI_ACCEPT(&adapter->hw, status,
			    current_desc->errors, pkt_len, last_byte,
			    adapter->min_frame_size, adapter->max_frame_size)) {
				e1000_tbi_adjust_stats_82543(&adapter->hw,
				    &adapter->stats, pkt_len,
				    adapter->hw.mac.addr,
				    adapter->max_frame_size);
				if (len > 0)
					len--;
			} else
				accept_frame = 0;
		}

		if (accept_frame) {
			if (lem_get_buf(adapter, i) != 0) {
				ifp->if_iqdrops++;
				goto discard;
			}

			/* Assign correct length to the current fragment */
			mp->m_len = len;

			if (adapter->fmp == NULL) {
				mp->m_pkthdr.len = len;
				adapter->fmp = mp; /* Store the first mbuf */
				adapter->lmp = mp;
			} else {
				/* Chain mbuf's together */
				mp->m_flags &= ~M_PKTHDR;
				/*
				 * Adjust length of previous mbuf in chain if
				 * we received less than 4 bytes in the last
				 * descriptor.
				 */
				if (prev_len_adj > 0) {
					adapter->lmp->m_len -= prev_len_adj;
					adapter->fmp->m_pkthdr.len -=
					    prev_len_adj;
				}
				adapter->lmp->m_next = mp;
				adapter->lmp = adapter->lmp->m_next;
				adapter->fmp->m_pkthdr.len += len;
			}

			if (eop) {
				adapter->fmp->m_pkthdr.rcvif = ifp;
				ifp->if_ipackets++;
				lem_receive_checksum(adapter, current_desc,
				    adapter->fmp);
#ifndef __NO_STRICT_ALIGNMENT
				if (adapter->max_frame_size >
				    (MCLBYTES - ETHER_ALIGN) &&
				    lem_fixup_rx(adapter) != 0)
					goto skip;
#endif
				if (status & E1000_RXD_STAT_VP) {
					adapter->fmp->m_pkthdr.ether_vtag =
					    le16toh(current_desc->special);
					adapter->fmp->m_flags |= M_VLANTAG;
				}
#ifndef __NO_STRICT_ALIGNMENT
skip:
#endif
				m = adapter->fmp;
				adapter->fmp = NULL;
				adapter->lmp = NULL;
			}
		} else {
			adapter->dropped_pkts++;
discard:
			/* Reuse loaded DMA map and just update mbuf chain */
			mp = adapter->rx_buffer_area[i].m_head;
			mp->m_len = mp->m_pkthdr.len = MCLBYTES;
			mp->m_data = mp->m_ext.ext_buf;
			mp->m_next = NULL;
			if (adapter->max_frame_size <=
			    (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
			if (adapter->fmp != NULL) {
				m_freem(adapter->fmp);
				adapter->fmp = NULL;
				adapter->lmp = NULL;
			}
			m = NULL;
		}

		/* Zero out the receive descriptors status. */
		current_desc->status = 0;
		bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

#ifdef NIC_PARAVIRT
		if (csb_mode) {
			/* the buffer at i has been already replaced by lem_get_buf()
			 * so it is safe to set guest_rdt = i and possibly send a kick.
			 * XXX see if we can optimize it later.
			 */
			csb->guest_rdt = i;
			// XXX memory barrier
			if (i == csb->host_rxkick_at)
				E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), i);
		}
#endif /* NIC_PARAVIRT */
		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;
		/* Call into the stack */
		if (m != NULL) {
#ifdef BATCH_DISPATCH
		    if (adapter->batch_enable) {
			if (mh == NULL)
				mh = mt = m;
			else
				mt->m_nextpkt = m;
			mt = m;
			m->m_nextpkt = NULL;
			rx_sent++;
			current_desc = &adapter->rx_desc_base[i];
			continue;
		    }
#endif /* BATCH_DISPATCH */
			adapter->next_rx_desc_to_check = i;
			EM_RX_UNLOCK(adapter);
			(*ifp->if_input)(ifp, m);
			EM_RX_LOCK(adapter);
			rx_sent++;
			i = adapter->next_rx_desc_to_check;
		}
		current_desc = &adapter->rx_desc_base[i];
	}
	adapter->next_rx_desc_to_check = i;
#ifdef BATCH_DISPATCH
	if (mh) {
		EM_RX_UNLOCK(adapter);
		while ( (mt = mh) != NULL) {
			mh = mh->m_nextpkt;
			mt->m_nextpkt = NULL;
			if_input(ifp, mt);
		}
		EM_RX_LOCK(adapter);
		i = adapter->next_rx_desc_to_check; /* in case of interrupts */
		if (count > 0)
			goto batch_again;
	}
#endif /* BATCH_DISPATCH */

	/* Advance the E1000's Receive Queue #0  "Tail Pointer". */
	if (--i < 0)
		i = adapter->num_rx_desc - 1;
#ifdef NIC_PARAVIRT
	if (!csb_mode) /* filter out writes */
#endif /* NIC_PARAVIRT */
	E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), i);
	if (done != NULL)
		*done = rx_sent;
	EM_RX_UNLOCK(adapter);
	return ((status & E1000_RXD_STAT_DD) ? TRUE : FALSE);
}

#ifndef __NO_STRICT_ALIGNMENT
/*
 * When jumbo frames are enabled we should realign entire payload on
 * architecures with strict alignment. This is serious design mistake of 8254x
 * as it nullifies DMA operations. 8254x just allows RX buffer size to be
 * 2048/4096/8192/16384. What we really want is 2048 - ETHER_ALIGN to align its
 * payload. On architecures without strict alignment restrictions 8254x still
 * performs unaligned memory access which would reduce the performance too.
 * To avoid copying over an entire frame to align, we allocate a new mbuf and
 * copy ethernet header to the new mbuf. The new mbuf is prepended into the
 * existing mbuf chain.
 *
 * Be aware, best performance of the 8254x is achived only when jumbo frame is
 * not used at all on architectures with strict alignment.
 */
static int
lem_fixup_rx(struct adapter *adapter)
{
	struct mbuf *m, *n;
	int error;

	error = 0;
	m = adapter->fmp;
	if (m->m_len <= (MCLBYTES - ETHER_HDR_LEN)) {
		bcopy(m->m_data, m->m_data + ETHER_HDR_LEN, m->m_len);
		m->m_data += ETHER_HDR_LEN;
	} else {
		MGETHDR(n, M_NOWAIT, MT_DATA);
		if (n != NULL) {
			bcopy(m->m_data, n->m_data, ETHER_HDR_LEN);
			m->m_data += ETHER_HDR_LEN;
			m->m_len -= ETHER_HDR_LEN;
			n->m_len = ETHER_HDR_LEN;
			M_MOVE_PKTHDR(n, m);
			n->m_next = m;
			adapter->fmp = n;
		} else {
			adapter->dropped_pkts++;
			m_freem(adapter->fmp);
			adapter->fmp = NULL;
			error = ENOMEM;
		}
	}

	return (error);
}
#endif

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
lem_receive_checksum(struct adapter *adapter,
	    struct e1000_rx_desc *rx_desc, struct mbuf *mp)
{
	/* 82543 or newer only */
	if ((adapter->hw.mac.type < e1000_82543) ||
	    /* Ignore Checksum bit is set */
	    (rx_desc->status & E1000_RXD_STAT_IXSM)) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}

	if (rx_desc->status & E1000_RXD_STAT_IPCS) {
		/* Did it pass? */
		if (!(rx_desc->errors & E1000_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else {
			mp->m_pkthdr.csum_flags = 0;
		}
	}

	if (rx_desc->status & E1000_RXD_STAT_TCPCS) {
		/* Did it pass? */
		if (!(rx_desc->errors & E1000_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |=
			(CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			mp->m_pkthdr.csum_data = htons(0xffff);
		}
	}
}

/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
lem_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, bit;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))       /* Invalid ID */
                return;

	EM_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	/* Re-init to load the changes */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
		lem_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
lem_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))       /* Invalid */
                return;

	EM_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	adapter->shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
		lem_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}

static void
lem_setup_vlan_hw_support(struct adapter *adapter)
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
	for (int i = 0; i < EM_VFTA_SIZE; i++)
                if (adapter->shadow_vfta[i] != 0)
			E1000_WRITE_REG_ARRAY(hw, E1000_VFTA,
                            i, adapter->shadow_vfta[i]);

	reg = E1000_READ_REG(hw, E1000_CTRL);
	reg |= E1000_CTRL_VME;
	E1000_WRITE_REG(hw, E1000_CTRL, reg);

	/* Enable the Filter Table */
	reg = E1000_READ_REG(hw, E1000_RCTL);
	reg &= ~E1000_RCTL_CFIEN;
	reg |= E1000_RCTL_VFE;
	E1000_WRITE_REG(hw, E1000_RCTL, reg);
}

static void
lem_enable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ims_mask = IMS_ENABLE_MASK;

	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
}

static void
lem_disable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	E1000_WRITE_REG(hw, E1000_IMC, 0xffffffff);
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features 
 */
static void
lem_init_manageability(struct adapter *adapter)
{
	/* A shared code workaround */
	if (adapter->has_manage) {
		int manc = E1000_READ_REG(&adapter->hw, E1000_MANC);
		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);
		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * Give control back to hardware management
 * controller if there is one.
 */
static void
lem_release_manageability(struct adapter *adapter)
{
	if (adapter->has_manage) {
		int manc = E1000_READ_REG(&adapter->hw, E1000_MANC);

		/* re-enable hardware interception of ARP */
		manc |= E1000_MANC_ARP_EN;
		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * lem_get_hw_control sets the {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded. For AMT version type f/w
 * this means that the network i/f is open.
 */
static void
lem_get_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext;

	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
	return;
}

/*
 * lem_release_hw_control resets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is no longer loaded. For AMT versions of the
 * f/w this means that the network i/f is closed.
 */
static void
lem_release_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext;

	if (!adapter->has_manage)
		return;

	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	return;
}

static int
lem_is_valid_ether_addr(u8 *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (FALSE);
	}

	return (TRUE);
}

/*
** Parse the interface capabilities with regard
** to both system management and wake-on-lan for
** later use.
*/
static void
lem_get_wakeup(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	u16		eeprom_data = 0, device_id, apme_mask;

	adapter->has_manage = e1000_enable_mng_pass_thru(&adapter->hw);
	apme_mask = EM_EEPROM_APME;

	switch (adapter->hw.mac.type) {
	case e1000_82542:
	case e1000_82543:
		break;
	case e1000_82544:
		e1000_read_nvm(&adapter->hw,
		    NVM_INIT_CONTROL2_REG, 1, &eeprom_data);
		apme_mask = EM_82544_APME;
		break;
	case e1000_82546:
	case e1000_82546_rev_3:
		if (adapter->hw.bus.func == 1) {
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);
			break;
		} else
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	default:
		e1000_read_nvm(&adapter->hw,
		    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	}
	if (eeprom_data & apme_mask)
		adapter->wol = (E1000_WUFC_MAG | E1000_WUFC_MC);
	/*
         * We have the eeprom settings, now apply the special cases
         * where the eeprom may be wrong or the board won't support
         * wake on lan on a particular port
	 */
	device_id = pci_get_device(dev);
        switch (device_id) {
	case E1000_DEV_ID_82546GB_PCIE:
		adapter->wol = 0;
		break;
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546GB_FIBER:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (E1000_READ_REG(&adapter->hw, E1000_STATUS) &
		    E1000_STATUS_FUNC_1)
			adapter->wol = 0;
		break;
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
                /* if quad port adapter, disable WoL on all but port A */
		if (global_quad_port_a != 0)
			adapter->wol = 0;
		/* Reset for multiple quad port adapters */
		if (++global_quad_port_a == 4)
			global_quad_port_a = 0;
                break;
	}
	return;
}


/*
 * Enable PCI Wake On Lan capability
 */
static void
lem_enable_wakeup(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	struct ifnet	*ifp = adapter->ifp;
	u32		pmc, ctrl, ctrl_ext, rctl;
	u16     	status;

	if ((pci_find_cap(dev, PCIY_PMG, &pmc) != 0))
		return;

	/* Advertise the wakeup capability */
	ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
	ctrl |= (E1000_CTRL_SWDPIN2 | E1000_CTRL_SWDPIN3);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
	E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);

	/* Keep the laser running on Fiber adapters */
	if (adapter->hw.phy.media_type == e1000_media_type_fiber ||
	    adapter->hw.phy.media_type == e1000_media_type_internal_serdes) {
		ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
		ctrl_ext |= E1000_CTRL_EXT_SDP3_DATA;
		E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT, ctrl_ext);
	}

	/*
	** Determine type of Wakeup: note that wol
	** is set with all bits on by default.
	*/
	if ((ifp->if_capenable & IFCAP_WOL_MAGIC) == 0)
		adapter->wol &= ~E1000_WUFC_MAG;

	if ((ifp->if_capenable & IFCAP_WOL_MCAST) == 0)
		adapter->wol &= ~E1000_WUFC_MC;
	else {
		rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl);
	}

	if (adapter->hw.mac.type == e1000_pchlan) {
		if (lem_enable_phy_wakeup(adapter))
			return;
	} else {
		E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
		E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
	}


        /* Request PME */
        status = pci_read_config(dev, pmc + PCIR_POWER_STATUS, 2);
	status &= ~(PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE);
	if (ifp->if_capenable & IFCAP_WOL)
		status |= PCIM_PSTAT_PME | PCIM_PSTAT_PMEENABLE;
        pci_write_config(dev, pmc + PCIR_POWER_STATUS, status, 2);

	return;
}

/*
** WOL in the newer chipset interfaces (pchlan)
** require thing to be copied into the phy
*/
static int
lem_enable_phy_wakeup(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 mreg, ret = 0;
	u16 preg;

	/* copy MAC RARs to PHY RARs */
	for (int i = 0; i < adapter->hw.mac.rar_entry_count; i++) {
		mreg = E1000_READ_REG(hw, E1000_RAL(i));
		e1000_write_phy_reg(hw, BM_RAR_L(i), (u16)(mreg & 0xFFFF));
		e1000_write_phy_reg(hw, BM_RAR_M(i),
		    (u16)((mreg >> 16) & 0xFFFF));
		mreg = E1000_READ_REG(hw, E1000_RAH(i));
		e1000_write_phy_reg(hw, BM_RAR_H(i), (u16)(mreg & 0xFFFF));
		e1000_write_phy_reg(hw, BM_RAR_CTRL(i),
		    (u16)((mreg >> 16) & 0xFFFF));
	}

	/* copy MAC MTA to PHY MTA */
	for (int i = 0; i < adapter->hw.mac.mta_reg_count; i++) {
		mreg = E1000_READ_REG_ARRAY(hw, E1000_MTA, i);
		e1000_write_phy_reg(hw, BM_MTA(i), (u16)(mreg & 0xFFFF));
		e1000_write_phy_reg(hw, BM_MTA(i) + 1,
		    (u16)((mreg >> 16) & 0xFFFF));
	}

	/* configure PHY Rx Control register */
	e1000_read_phy_reg(&adapter->hw, BM_RCTL, &preg);
	mreg = E1000_READ_REG(hw, E1000_RCTL);
	if (mreg & E1000_RCTL_UPE)
		preg |= BM_RCTL_UPE;
	if (mreg & E1000_RCTL_MPE)
		preg |= BM_RCTL_MPE;
	preg &= ~(BM_RCTL_MO_MASK);
	if (mreg & E1000_RCTL_MO_3)
		preg |= (((mreg & E1000_RCTL_MO_3) >> E1000_RCTL_MO_SHIFT)
				<< BM_RCTL_MO_SHIFT);
	if (mreg & E1000_RCTL_BAM)
		preg |= BM_RCTL_BAM;
	if (mreg & E1000_RCTL_PMCF)
		preg |= BM_RCTL_PMCF;
	mreg = E1000_READ_REG(hw, E1000_CTRL);
	if (mreg & E1000_CTRL_RFCE)
		preg |= BM_RCTL_RFCE;
	e1000_write_phy_reg(&adapter->hw, BM_RCTL, preg);

	/* enable PHY wakeup in MAC register */
	E1000_WRITE_REG(hw, E1000_WUC,
	    E1000_WUC_PHY_WAKE | E1000_WUC_PME_EN);
	E1000_WRITE_REG(hw, E1000_WUFC, adapter->wol);

	/* configure and enable PHY wakeup in PHY registers */
	e1000_write_phy_reg(&adapter->hw, BM_WUFC, adapter->wol);
	e1000_write_phy_reg(&adapter->hw, BM_WUC, E1000_WUC_PME_EN);

	/* activate PHY wakeup */
	ret = hw->phy.ops.acquire(hw);
	if (ret) {
		printf("Could not acquire PHY\n");
		return ret;
	}
	e1000_write_phy_reg_mdic(hw, IGP01E1000_PHY_PAGE_SELECT,
	                         (BM_WUC_ENABLE_PAGE << IGP_PAGE_SHIFT));
	ret = e1000_read_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, &preg);
	if (ret) {
		printf("Could not read PHY page 769\n");
		goto out;
	}
	preg |= BM_WUC_ENABLE_BIT | BM_WUC_HOST_WU_BIT;
	ret = e1000_write_phy_reg_mdic(hw, BM_WUC_ENABLE_REG, preg);
	if (ret)
		printf("Could not set PHY Host Wakeup bit\n");
out:
	hw->phy.ops.release(hw);

	return ret;
}

static void
lem_led_func(void *arg, int onoff)
{
	struct adapter	*adapter = arg;

	EM_CORE_LOCK(adapter);
	if (onoff) {
		e1000_setup_led(&adapter->hw);
		e1000_led_on(&adapter->hw);
	} else {
		e1000_led_off(&adapter->hw);
		e1000_cleanup_led(&adapter->hw);
	}
	EM_CORE_UNLOCK(adapter);
}

/*********************************************************************
* 82544 Coexistence issue workaround.
*    There are 2 issues.
*       1. Transmit Hang issue.
*    To detect this issue, following equation can be used...
*	  SIZE[3:0] + ADDR[2:0] = SUM[3:0].
*	  If SUM[3:0] is in between 1 to 4, we will have this issue.
*
*       2. DAC issue.
*    To detect this issue, following equation can be used...
*	  SIZE[3:0] + ADDR[2:0] = SUM[3:0].
*	  If SUM[3:0] is in between 9 to c, we will have this issue.
*
*
*    WORKAROUND:
*	  Make sure we do not have ending address
*	  as 1,2,3,4(Hang) or 9,a,b,c (DAC)
*
*************************************************************************/
static u32
lem_fill_descriptors (bus_addr_t address, u32 length,
		PDESC_ARRAY desc_array)
{
	u32 safe_terminator;

	/* Since issue is sensitive to length and address.*/
	/* Let us first check the address...*/
	if (length <= 4) {
		desc_array->descriptor[0].address = address;
		desc_array->descriptor[0].length = length;
		desc_array->elements = 1;
		return (desc_array->elements);
	}
	safe_terminator = (u32)((((u32)address & 0x7) +
	    (length & 0xF)) & 0xF);
	/* if it does not fall between 0x1 to 0x4 and 0x9 to 0xC then return */
	if (safe_terminator == 0   ||
	(safe_terminator > 4   &&
	safe_terminator < 9)   ||
	(safe_terminator > 0xC &&
	safe_terminator <= 0xF)) {
		desc_array->descriptor[0].address = address;
		desc_array->descriptor[0].length = length;
		desc_array->elements = 1;
		return (desc_array->elements);
	}

	desc_array->descriptor[0].address = address;
	desc_array->descriptor[0].length = length - 4;
	desc_array->descriptor[1].address = address + (length - 4);
	desc_array->descriptor[1].length = 4;
	desc_array->elements = 2;
	return (desc_array->elements);
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
lem_update_stats_counters(struct adapter *adapter)
{
	struct ifnet   *ifp;

	if(adapter->hw.phy.media_type == e1000_media_type_copper ||
	   (E1000_READ_REG(&adapter->hw, E1000_STATUS) & E1000_STATUS_LU)) {
		adapter->stats.symerrs += E1000_READ_REG(&adapter->hw, E1000_SYMERRS);
		adapter->stats.sec += E1000_READ_REG(&adapter->hw, E1000_SEC);
	}
	adapter->stats.crcerrs += E1000_READ_REG(&adapter->hw, E1000_CRCERRS);
	adapter->stats.mpc += E1000_READ_REG(&adapter->hw, E1000_MPC);
	adapter->stats.scc += E1000_READ_REG(&adapter->hw, E1000_SCC);
	adapter->stats.ecol += E1000_READ_REG(&adapter->hw, E1000_ECOL);

	adapter->stats.mcc += E1000_READ_REG(&adapter->hw, E1000_MCC);
	adapter->stats.latecol += E1000_READ_REG(&adapter->hw, E1000_LATECOL);
	adapter->stats.colc += E1000_READ_REG(&adapter->hw, E1000_COLC);
	adapter->stats.dc += E1000_READ_REG(&adapter->hw, E1000_DC);
	adapter->stats.rlec += E1000_READ_REG(&adapter->hw, E1000_RLEC);
	adapter->stats.xonrxc += E1000_READ_REG(&adapter->hw, E1000_XONRXC);
	adapter->stats.xontxc += E1000_READ_REG(&adapter->hw, E1000_XONTXC);
	adapter->stats.xoffrxc += E1000_READ_REG(&adapter->hw, E1000_XOFFRXC);
	adapter->stats.xofftxc += E1000_READ_REG(&adapter->hw, E1000_XOFFTXC);
	adapter->stats.fcruc += E1000_READ_REG(&adapter->hw, E1000_FCRUC);
	adapter->stats.prc64 += E1000_READ_REG(&adapter->hw, E1000_PRC64);
	adapter->stats.prc127 += E1000_READ_REG(&adapter->hw, E1000_PRC127);
	adapter->stats.prc255 += E1000_READ_REG(&adapter->hw, E1000_PRC255);
	adapter->stats.prc511 += E1000_READ_REG(&adapter->hw, E1000_PRC511);
	adapter->stats.prc1023 += E1000_READ_REG(&adapter->hw, E1000_PRC1023);
	adapter->stats.prc1522 += E1000_READ_REG(&adapter->hw, E1000_PRC1522);
	adapter->stats.gprc += E1000_READ_REG(&adapter->hw, E1000_GPRC);
	adapter->stats.bprc += E1000_READ_REG(&adapter->hw, E1000_BPRC);
	adapter->stats.mprc += E1000_READ_REG(&adapter->hw, E1000_MPRC);
	adapter->stats.gptc += E1000_READ_REG(&adapter->hw, E1000_GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	adapter->stats.gorc += E1000_READ_REG(&adapter->hw, E1000_GORCL) +
	    ((u64)E1000_READ_REG(&adapter->hw, E1000_GORCH) << 32);
	adapter->stats.gotc += E1000_READ_REG(&adapter->hw, E1000_GOTCL) +
	    ((u64)E1000_READ_REG(&adapter->hw, E1000_GOTCH) << 32);

	adapter->stats.rnbc += E1000_READ_REG(&adapter->hw, E1000_RNBC);
	adapter->stats.ruc += E1000_READ_REG(&adapter->hw, E1000_RUC);
	adapter->stats.rfc += E1000_READ_REG(&adapter->hw, E1000_RFC);
	adapter->stats.roc += E1000_READ_REG(&adapter->hw, E1000_ROC);
	adapter->stats.rjc += E1000_READ_REG(&adapter->hw, E1000_RJC);

	adapter->stats.tor += E1000_READ_REG(&adapter->hw, E1000_TORH);
	adapter->stats.tot += E1000_READ_REG(&adapter->hw, E1000_TOTH);

	adapter->stats.tpr += E1000_READ_REG(&adapter->hw, E1000_TPR);
	adapter->stats.tpt += E1000_READ_REG(&adapter->hw, E1000_TPT);
	adapter->stats.ptc64 += E1000_READ_REG(&adapter->hw, E1000_PTC64);
	adapter->stats.ptc127 += E1000_READ_REG(&adapter->hw, E1000_PTC127);
	adapter->stats.ptc255 += E1000_READ_REG(&adapter->hw, E1000_PTC255);
	adapter->stats.ptc511 += E1000_READ_REG(&adapter->hw, E1000_PTC511);
	adapter->stats.ptc1023 += E1000_READ_REG(&adapter->hw, E1000_PTC1023);
	adapter->stats.ptc1522 += E1000_READ_REG(&adapter->hw, E1000_PTC1522);
	adapter->stats.mptc += E1000_READ_REG(&adapter->hw, E1000_MPTC);
	adapter->stats.bptc += E1000_READ_REG(&adapter->hw, E1000_BPTC);

	if (adapter->hw.mac.type >= e1000_82543) {
		adapter->stats.algnerrc += 
		E1000_READ_REG(&adapter->hw, E1000_ALGNERRC);
		adapter->stats.rxerrc += 
		E1000_READ_REG(&adapter->hw, E1000_RXERRC);
		adapter->stats.tncrs += 
		E1000_READ_REG(&adapter->hw, E1000_TNCRS);
		adapter->stats.cexterr += 
		E1000_READ_REG(&adapter->hw, E1000_CEXTERR);
		adapter->stats.tsctc += 
		E1000_READ_REG(&adapter->hw, E1000_TSCTC);
		adapter->stats.tsctfc += 
		E1000_READ_REG(&adapter->hw, E1000_TSCTFC);
	}
	ifp = adapter->ifp;

	ifp->if_collisions = adapter->stats.colc;

	/* Rx Errors */
	ifp->if_ierrors = adapter->dropped_pkts + adapter->stats.rxerrc +
	    adapter->stats.crcerrs + adapter->stats.algnerrc +
	    adapter->stats.ruc + adapter->stats.roc +
	    adapter->stats.mpc + adapter->stats.cexterr;

	/* Tx Errors */
	ifp->if_oerrors = adapter->stats.ecol +
	    adapter->stats.latecol + adapter->watchdog_events;
}

/* Export a single 32-bit register via a read-only sysctl. */
static int
lem_sysctl_reg_handler(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	u_int val;

	adapter = oidp->oid_arg1;
	val = E1000_READ_REG(&adapter->hw, oidp->oid_arg2);
	return (sysctl_handle_int(oidp, &val, 0, req));
}

/*
 * Add sysctl variables, one per statistic, to the system.
 */
static void
lem_add_hw_stats(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct e1000_hw_stats *stats = &adapter->stats;

	struct sysctl_oid *stat_node;
	struct sysctl_oid_list *stat_list;

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "cluster_alloc_fail", 
			 CTLFLAG_RD, &adapter->mbuf_cluster_failed,
			 "Std mbuf cluster failed");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_defrag_fail", 
			 CTLFLAG_RD, &adapter->mbuf_defrag_failed,
			 "Defragmenting mbuf chain failed");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped", 
			CTLFLAG_RD, &adapter->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "tx_dma_fail", 
			CTLFLAG_RD, &adapter->no_tx_dma_setup,
			"Driver tx dma failure in xmit");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "tx_desc_fail1",
			CTLFLAG_RD, &adapter->no_tx_desc_avail1,
			"Not enough tx descriptors failure in xmit");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "tx_desc_fail2",
			CTLFLAG_RD, &adapter->no_tx_desc_avail2,
			"Not enough tx descriptors failure in xmit");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "rx_overruns",
			CTLFLAG_RD, &adapter->rx_overruns,
			"RX overruns");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_timeouts",
			CTLFLAG_RD, &adapter->watchdog_events,
			"Watchdog timeouts");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "device_control",
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_CTRL,
			lem_sysctl_reg_handler, "IU",
			"Device Control Register");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_control",
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_RCTL,
			lem_sysctl_reg_handler, "IU",
			"Receiver Control Register");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_high_water",
			CTLFLAG_RD, &adapter->hw.fc.high_water, 0,
			"Flow Control High Watermark");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_low_water", 
			CTLFLAG_RD, &adapter->hw.fc.low_water, 0,
			"Flow Control Low Watermark");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "fifo_workaround",
			CTLFLAG_RD, &adapter->tx_fifo_wrk_cnt,
			"TX FIFO workaround events");
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "fifo_reset",
			CTLFLAG_RD, &adapter->tx_fifo_reset_cnt,
			"TX FIFO resets");

	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "txd_head", 
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_TDH(0),
			lem_sysctl_reg_handler, "IU",
 			"Transmit Descriptor Head");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "txd_tail", 
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_TDT(0),
			lem_sysctl_reg_handler, "IU",
 			"Transmit Descriptor Tail");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rxd_head", 
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_RDH(0),
			lem_sysctl_reg_handler, "IU",
			"Receive Descriptor Head");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rxd_tail", 
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_RDT(0),
			lem_sysctl_reg_handler, "IU",
			"Receive Descriptor Tail");
	

	/* MAC stats get their own sub node */

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac_stats", 
				    CTLFLAG_RD, NULL, "Statistics");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "excess_coll",
			CTLFLAG_RD, &stats->ecol,
			"Excessive collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "single_coll",
			CTLFLAG_RD, &stats->scc,
			"Single collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "multiple_coll",
			CTLFLAG_RD, &stats->mcc,
			"Multiple collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "late_coll",
			CTLFLAG_RD, &stats->latecol,
			"Late collisions");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "collision_count",
			CTLFLAG_RD, &stats->colc,
			"Collision Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "symbol_errors",
			CTLFLAG_RD, &adapter->stats.symerrs,
			"Symbol Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "sequence_errors",
			CTLFLAG_RD, &adapter->stats.sec,
			"Sequence Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "defer_count",
			CTLFLAG_RD, &adapter->stats.dc,
			"Defer Count");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "missed_packets",
			CTLFLAG_RD, &adapter->stats.mpc,
			"Missed Packets");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_no_buff",
			CTLFLAG_RD, &adapter->stats.rnbc,
			"Receive No Buffers");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_undersize",
			CTLFLAG_RD, &adapter->stats.ruc,
			"Receive Undersize");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_fragmented",
			CTLFLAG_RD, &adapter->stats.rfc,
			"Fragmented Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_oversize",
			CTLFLAG_RD, &adapter->stats.roc,
			"Oversized Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_jabber",
			CTLFLAG_RD, &adapter->stats.rjc,
			"Recevied Jabber");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "recv_errs",
			CTLFLAG_RD, &adapter->stats.rxerrc,
			"Receive Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "crc_errs",
			CTLFLAG_RD, &adapter->stats.crcerrs,
			"CRC errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "alignment_errs",
			CTLFLAG_RD, &adapter->stats.algnerrc,
			"Alignment Errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "coll_ext_errs",
			CTLFLAG_RD, &adapter->stats.cexterr,
			"Collision/Carrier extension errors");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_recvd",
			CTLFLAG_RD, &adapter->stats.xonrxc,
			"XON Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xon_txd",
			CTLFLAG_RD, &adapter->stats.xontxc,
			"XON Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_recvd",
			CTLFLAG_RD, &adapter->stats.xoffrxc,
			"XOFF Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "xoff_txd",
			CTLFLAG_RD, &adapter->stats.xofftxc,
			"XOFF Transmitted");

	/* Packet Reception Stats */
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_recvd",
			CTLFLAG_RD, &adapter->stats.tpr,
			"Total Packets Received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_recvd",
			CTLFLAG_RD, &adapter->stats.gprc,
			"Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_recvd",
			CTLFLAG_RD, &adapter->stats.bprc,
			"Broadcast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_recvd",
			CTLFLAG_RD, &adapter->stats.mprc,
			"Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_64",
			CTLFLAG_RD, &adapter->stats.prc64,
			"64 byte frames received ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_65_127",
			CTLFLAG_RD, &adapter->stats.prc127,
			"65-127 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_128_255",
			CTLFLAG_RD, &adapter->stats.prc255,
			"128-255 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_256_511",
			CTLFLAG_RD, &adapter->stats.prc511,
			"256-511 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_512_1023",
			CTLFLAG_RD, &adapter->stats.prc1023,
			"512-1023 byte frames received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "rx_frames_1024_1522",
			CTLFLAG_RD, &adapter->stats.prc1522,
			"1023-1522 byte frames received");
 	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_recvd",
 			CTLFLAG_RD, &adapter->stats.gorc, 
 			"Good Octets Received");

	/* Packet Transmission Stats */
 	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
 			CTLFLAG_RD, &adapter->stats.gotc, 
 			"Good Octets Transmitted"); 
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "total_pkts_txd",
			CTLFLAG_RD, &adapter->stats.tpt,
			"Total Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &adapter->stats.gptc,
			"Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "bcast_pkts_txd",
			CTLFLAG_RD, &adapter->stats.bptc,
			"Broadcast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_txd",
			CTLFLAG_RD, &adapter->stats.mptc,
			"Multicast Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_64",
			CTLFLAG_RD, &adapter->stats.ptc64,
			"64 byte frames transmitted ");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_65_127",
			CTLFLAG_RD, &adapter->stats.ptc127,
			"65-127 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_128_255",
			CTLFLAG_RD, &adapter->stats.ptc255,
			"128-255 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_256_511",
			CTLFLAG_RD, &adapter->stats.ptc511,
			"256-511 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_512_1023",
			CTLFLAG_RD, &adapter->stats.ptc1023,
			"512-1023 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tx_frames_1024_1522",
			CTLFLAG_RD, &adapter->stats.ptc1522,
			"1024-1522 byte frames transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tso_txd",
			CTLFLAG_RD, &adapter->stats.tsctc,
			"TSO Contexts Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "tso_ctx_fail",
			CTLFLAG_RD, &adapter->stats.tsctfc,
			"TSO Contexts Failed");
}

/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/

static int
lem_sysctl_nvm_info(SYSCTL_HANDLER_ARGS)
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
		lem_print_nvm_info(adapter);
        }

	return (error);
}

static void
lem_print_nvm_info(struct adapter *adapter)
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

static int
lem_sysctl_int_delay(SYSCTL_HANDLER_ARGS)
{
	struct em_int_delay_info *info;
	struct adapter *adapter;
	u32 regval;
	int error;
	int usecs;
	int ticks;

	info = (struct em_int_delay_info *)arg1;
	usecs = info->value;
	error = sysctl_handle_int(oidp, &usecs, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (usecs < 0 || usecs > EM_TICKS_TO_USECS(65535))
		return (EINVAL);
	info->value = usecs;
	ticks = EM_USECS_TO_TICKS(usecs);
	if (info->offset == E1000_ITR)	/* units are 256ns here */
		ticks *= 4;

	adapter = info->adapter;
	
	EM_CORE_LOCK(adapter);
	regval = E1000_READ_OFFSET(&adapter->hw, info->offset);
	regval = (regval & ~0xffff) | (ticks & 0xffff);
	/* Handle a few special cases. */
	switch (info->offset) {
	case E1000_RDTR:
		break;
	case E1000_TIDV:
		if (ticks == 0) {
			adapter->txd_cmd &= ~E1000_TXD_CMD_IDE;
			/* Don't write 0 into the TIDV register. */
			regval++;
		} else
			adapter->txd_cmd |= E1000_TXD_CMD_IDE;
		break;
	}
	E1000_WRITE_OFFSET(&adapter->hw, info->offset, regval);
	EM_CORE_UNLOCK(adapter);
	return (0);
}

static void
lem_add_int_delay_sysctl(struct adapter *adapter, const char *name,
	const char *description, struct em_int_delay_info *info,
	int offset, int value)
{
	info->adapter = adapter;
	info->offset = offset;
	info->value = value;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW,
	    info, 0, lem_sysctl_int_delay, "I", description);
}

static void
lem_set_flow_cntrl(struct adapter *adapter, const char *name,
        const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLFLAG_RW, limit, value, description);
}

static void
lem_add_rx_process_limit(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLFLAG_RW, limit, value, description);
}
