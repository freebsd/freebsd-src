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
#include "e1000_82571.h"
#include "if_em.h"

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int	em_display_debug_stats = 0;

/*********************************************************************
 *  Driver version:
 *********************************************************************/
char em_driver_version[] = "7.1.9";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into e1000_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static em_vendor_info_t em_vendor_info_array[] =
{
	/* Intel(R) PRO/1000 Network Connection */
	{ 0x8086, E1000_DEV_ID_82571EB_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_SERDES_DUAL,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_SERDES_QUAD,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER_LP,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_QUAD_FIBER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571PT_QUAD_COPPER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI,		PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82573E,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82573E_IAMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82573L,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82583V,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_COPPER_SPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_SERDES_SPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_COPPER_DPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_SERDES_DPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IGP_M_AMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IGP_AMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IGP_C,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IFE,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IFE_GT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IFE_G,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_IGP_M,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH8_82567V_3,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IGP_M_AMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IGP_AMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IGP_C,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IGP_M,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IGP_M_V,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IFE,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IFE_GT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_IFE_G,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH9_BM,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82574L,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82574LA,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_LF,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_V,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_D_BM_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_D_BM_LF,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_D_BM_V,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_M_HV_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_M_HV_LC,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_D_HV_DM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_D_HV_DC,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH2_LV_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH2_LV_V,	PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{ 0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings for all supported NICs.
 *********************************************************************/

static char *em_strings[] = {
	"Intel(R) PRO/1000 Network Connection"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int	em_probe(device_t);
static int	em_attach(device_t);
static int	em_detach(device_t);
static int	em_shutdown(device_t);
static int	em_suspend(device_t);
static int	em_resume(device_t);
static void	em_start(struct ifnet *);
static void	em_start_locked(struct ifnet *, struct tx_ring *);
#ifdef EM_MULTIQUEUE
static int	em_mq_start(struct ifnet *, struct mbuf *);
static int	em_mq_start_locked(struct ifnet *,
		    struct tx_ring *, struct mbuf *);
static void	em_qflush(struct ifnet *);
#endif
static int	em_ioctl(struct ifnet *, u_long, caddr_t);
static void	em_init(void *);
static void	em_init_locked(struct adapter *);
static void	em_stop(void *);
static void	em_media_status(struct ifnet *, struct ifmediareq *);
static int	em_media_change(struct ifnet *);
static void	em_identify_hardware(struct adapter *);
static int	em_allocate_pci_resources(struct adapter *);
static int	em_allocate_legacy(struct adapter *);
static int	em_allocate_msix(struct adapter *);
static int	em_allocate_queues(struct adapter *);
static int	em_setup_msix(struct adapter *);
static void	em_free_pci_resources(struct adapter *);
static void	em_local_timer(void *);
static void	em_reset(struct adapter *);
static int	em_setup_interface(device_t, struct adapter *);

static void	em_setup_transmit_structures(struct adapter *);
static void	em_initialize_transmit_unit(struct adapter *);
static int	em_allocate_transmit_buffers(struct tx_ring *);
static void	em_free_transmit_structures(struct adapter *);
static void	em_free_transmit_buffers(struct tx_ring *);

static int	em_setup_receive_structures(struct adapter *);
static int	em_allocate_receive_buffers(struct rx_ring *);
static void	em_initialize_receive_unit(struct adapter *);
static void	em_free_receive_structures(struct adapter *);
static void	em_free_receive_buffers(struct rx_ring *);

static void	em_enable_intr(struct adapter *);
static void	em_disable_intr(struct adapter *);
static void	em_update_stats_counters(struct adapter *);
static void	em_add_hw_stats(struct adapter *adapter);
static bool	em_txeof(struct tx_ring *);
static bool	em_rxeof(struct rx_ring *, int, int *);
#ifndef __NO_STRICT_ALIGNMENT
static int	em_fixup_rx(struct rx_ring *);
#endif
static void	em_receive_checksum(struct e1000_rx_desc *, struct mbuf *);
static void	em_transmit_checksum_setup(struct tx_ring *, struct mbuf *, int,
		    struct ip *, u32 *, u32 *);
static void	em_tso_setup(struct tx_ring *, struct mbuf *, int, struct ip *,
		    struct tcphdr *, u32 *, u32 *);
static void	em_set_promisc(struct adapter *);
static void	em_disable_promisc(struct adapter *);
static void	em_set_multi(struct adapter *);
static void	em_update_link_status(struct adapter *);
static void	em_refresh_mbufs(struct rx_ring *, int);
static void	em_register_vlan(void *, struct ifnet *, u16);
static void	em_unregister_vlan(void *, struct ifnet *, u16);
static void	em_setup_vlan_hw_support(struct adapter *);
static int	em_xmit(struct tx_ring *, struct mbuf **);
static int	em_dma_malloc(struct adapter *, bus_size_t,
		    struct em_dma_alloc *, int);
static void	em_dma_free(struct adapter *, struct em_dma_alloc *);
static int	em_sysctl_nvm_info(SYSCTL_HANDLER_ARGS);
static void	em_print_nvm_info(struct adapter *);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static void	em_print_debug_info(struct adapter *);
static int 	em_is_valid_ether_addr(u8 *);
static int	em_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	em_add_int_delay_sysctl(struct adapter *, const char *,
		    const char *, struct em_int_delay_info *, int, int);
/* Management and WOL Support */
static void	em_init_manageability(struct adapter *);
static void	em_release_manageability(struct adapter *);
static void     em_get_hw_control(struct adapter *);
static void     em_release_hw_control(struct adapter *);
static void	em_get_wakeup(device_t);
static void     em_enable_wakeup(device_t);
static int	em_enable_phy_wakeup(struct adapter *);
static void	em_led_func(void *, int);
static void	em_disable_aspm(struct adapter *);

static int	em_irq_fast(void *);

/* MSIX handlers */
static void	em_msix_tx(void *);
static void	em_msix_rx(void *);
static void	em_msix_link(void *);
static void	em_handle_tx(void *context, int pending);
static void	em_handle_rx(void *context, int pending);
static void	em_handle_link(void *context, int pending);

static void	em_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static void	em_set_flow_cntrl(struct adapter *, const char *,
		    const char *, int *, int);

static __inline void em_rx_discard(struct rx_ring *, int);

#ifdef DEVICE_POLLING
static poll_handler_t em_poll;
#endif /* POLLING */

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t em_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, em_probe),
	DEVMETHOD(device_attach, em_attach),
	DEVMETHOD(device_detach, em_detach),
	DEVMETHOD(device_shutdown, em_shutdown),
	DEVMETHOD(device_suspend, em_suspend),
	DEVMETHOD(device_resume, em_resume),
	{0, 0}
};

static driver_t em_driver = {
	"em", em_methods, sizeof(struct adapter),
};

devclass_t em_devclass;
DRIVER_MODULE(em, pci, em_driver, em_devclass, 0, 0);
MODULE_DEPEND(em, pci, 1, 1, 1);
MODULE_DEPEND(em, ether, 1, 1, 1);

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

#define EM_TICKS_TO_USECS(ticks)	((1024 * (ticks) + 500) / 1000)
#define EM_USECS_TO_TICKS(usecs)	((1000 * (usecs) + 512) / 1024)
#define M_TSO_LEN			66

/* Allow common code without TSO */
#ifndef CSUM_TSO
#define CSUM_TSO	0
#endif

static int em_tx_int_delay_dflt = EM_TICKS_TO_USECS(EM_TIDV);
static int em_rx_int_delay_dflt = EM_TICKS_TO_USECS(EM_RDTR);
TUNABLE_INT("hw.em.tx_int_delay", &em_tx_int_delay_dflt);
TUNABLE_INT("hw.em.rx_int_delay", &em_rx_int_delay_dflt);

static int em_tx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_TADV);
static int em_rx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_RADV);
TUNABLE_INT("hw.em.tx_abs_int_delay", &em_tx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rx_abs_int_delay", &em_rx_abs_int_delay_dflt);

static int em_rxd = EM_DEFAULT_RXD;
static int em_txd = EM_DEFAULT_TXD;
TUNABLE_INT("hw.em.rxd", &em_rxd);
TUNABLE_INT("hw.em.txd", &em_txd);

static int em_smart_pwr_down = FALSE;
TUNABLE_INT("hw.em.smart_pwr_down", &em_smart_pwr_down);

/* Controls whether promiscuous also shows bad packets */
static int em_debug_sbp = FALSE;
TUNABLE_INT("hw.em.sbp", &em_debug_sbp);

static int em_enable_msix = TRUE;
TUNABLE_INT("hw.em.enable_msix", &em_enable_msix);

/* How many packets rxeof tries to clean at a time */
static int em_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &em_rx_process_limit);

/* Flow control setting - default to FULL */
static int em_fc_setting = e1000_fc_full;
TUNABLE_INT("hw.em.fc_setting", &em_fc_setting);

/* Global used in WOL setup with multiport cards */
static int global_quad_port_a = 0;

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
em_probe(device_t dev)
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

	ent = em_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		    (ent->subvendor_id == PCI_ANY_ID)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		    (ent->subdevice_id == PCI_ANY_ID))) {
			sprintf(adapter_name, "%s %s",
				em_strings[ent->index],
				em_driver_version);
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
em_attach(device_t dev)
{
	struct adapter	*adapter;
	int		error = 0;

	INIT_DEBUGOUT("em_attach: begin");

	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	EM_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* SYSCTL stuff */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "nvm", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_nvm_info, "I", "NVM Information");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_debug_info, "I", "Debug Information");

	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware and mac info */
	em_identify_hardware(adapter);

	/* Setup PCI resources */
	if (em_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}

	/*
	** For ICH8 and family we need to
	** map the flash memory, and this
	** must happen after the MAC is 
	** identified
	*/
	if ((adapter->hw.mac.type == e1000_ich8lan) ||
	    (adapter->hw.mac.type == e1000_ich9lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan) ||
	    (adapter->hw.mac.type == e1000_pchlan) ||
	    (adapter->hw.mac.type == e1000_pch2lan)) {
		int rid = EM_BAR_TYPE_FLASH;
		adapter->flash = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
		if (adapter->flash == NULL) {
			device_printf(dev, "Mapping of Flash failed\n");
			error = ENXIO;
			goto err_pci;
		}
		/* This is used in the shared code */
		adapter->hw.flash_address = (u8 *)adapter->flash;
		adapter->osdep.flash_bus_space_tag =
		    rman_get_bustag(adapter->flash);
		adapter->osdep.flash_bus_space_handle =
		    rman_get_bushandle(adapter->flash);
	}

	/* Do Shared Code initialization */
	if (e1000_setup_init_funcs(&adapter->hw, TRUE)) {
		device_printf(dev, "Setup of Shared code failed\n");
		error = ENXIO;
		goto err_pci;
	}

	e1000_get_bus_info(&adapter->hw);

	/* Set up some sysctls for the tunable interrupt delays */
	em_add_int_delay_sysctl(adapter, "rx_int_delay",
	    "receive interrupt delay in usecs", &adapter->rx_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_RDTR), em_rx_int_delay_dflt);
	em_add_int_delay_sysctl(adapter, "tx_int_delay",
	    "transmit interrupt delay in usecs", &adapter->tx_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_TIDV), em_tx_int_delay_dflt);
	em_add_int_delay_sysctl(adapter, "rx_abs_int_delay",
	    "receive interrupt delay limit in usecs",
	    &adapter->rx_abs_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_RADV),
	    em_rx_abs_int_delay_dflt);
	em_add_int_delay_sysctl(adapter, "tx_abs_int_delay",
	    "transmit interrupt delay limit in usecs",
	    &adapter->tx_abs_int_delay,
	    E1000_REGISTER(&adapter->hw, E1000_TADV),
	    em_tx_abs_int_delay_dflt);

	/* Sysctl for limiting the amount of work done in the taskqueue */
	em_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    em_rx_process_limit);

	/* Sysctl for setting the interface flow control */
	em_set_flow_cntrl(adapter, "flow_control",
	    "configure flow control",
	    &adapter->fc_setting, em_fc_setting);

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of E1000_DBA_ALIGN.
	 */
	if (((em_txd * sizeof(struct e1000_tx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (em_txd > EM_MAX_TXD) || (em_txd < EM_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    EM_DEFAULT_TXD, em_txd);
		adapter->num_tx_desc = EM_DEFAULT_TXD;
	} else
		adapter->num_tx_desc = em_txd;

	if (((em_rxd * sizeof(struct e1000_rx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (em_rxd > EM_MAX_RXD) || (em_rxd < EM_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    EM_DEFAULT_RXD, em_rxd);
		adapter->num_rx_desc = EM_DEFAULT_RXD;
	} else
		adapter->num_rx_desc = em_rxd;

	adapter->hw.mac.autoneg = DO_AUTO_NEG;
	adapter->hw.phy.autoneg_wait_to_complete = FALSE;
	adapter->hw.phy.autoneg_advertised = AUTONEG_ADV_DEFAULT;

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

	/* 
	** Get queue/ring memory
	*/
	if (em_allocate_queues(adapter)) {
		error = ENOMEM;
		goto err_pci;
	}

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(u8) * ETH_ADDR_LEN *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (adapter->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_late;
	}

	/* Check SOL/IDER usage */
	if (e1000_check_reset_block(&adapter->hw))
		device_printf(dev, "PHY reset is blocked"
		    " due to SOL/IDER session.\n");

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

	/* Copy the permanent MAC address out of the EEPROM */
	if (e1000_read_mac_addr(&adapter->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_late;
	}

	if (!em_is_valid_ether_addr(adapter->hw.mac.addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_late;
	}

	/*
	**  Do interrupt configuration
	*/
	if (adapter->msix > 1) /* Do MSIX */
		error = em_allocate_msix(adapter);
	else  /* MSI or Legacy */
		error = em_allocate_legacy(adapter);
	if (error)
		goto err_late;

	/*
	 * Get Wake-on-Lan and Management info for later use
	 */
	em_get_wakeup(dev);

	/* Setup OS specific network interface */
	if (em_setup_interface(dev, adapter) != 0)
		goto err_late;

	em_reset(adapter);

	/* Initialize statistics */
	em_update_stats_counters(adapter);

	adapter->hw.mac.get_link_status = 1;
	em_update_link_status(adapter);

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    em_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    em_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST); 

	em_add_hw_stats(adapter);

	/* Non-AMT based hardware can now take control from firmware */
	if (adapter->has_manage && !adapter->has_amt)
		em_get_hw_control(adapter);

	/* Tell the stack that the interface is not active */
	adapter->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	adapter->led_dev = led_create(em_led_func, adapter,
	    device_get_nameunit(dev));

	INIT_DEBUGOUT("em_attach: end");

	return (0);

err_late:
	em_free_transmit_structures(adapter);
	em_free_receive_structures(adapter);
	em_release_hw_control(adapter);
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
err_pci:
	em_free_pci_resources(adapter);
	free(adapter->mta, M_DEVBUF);
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
em_detach(device_t dev)
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
	adapter->in_detach = 1;
	em_stop(adapter);
	EM_CORE_UNLOCK(adapter);
	EM_CORE_LOCK_DESTROY(adapter);

	e1000_phy_hw_reset(&adapter->hw);

	em_release_manageability(adapter);
	em_release_hw_control(adapter);

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach); 

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);

	em_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(ifp);

	em_free_transmit_structures(adapter);
	em_free_receive_structures(adapter);

	em_release_hw_control(adapter);
	free(adapter->mta, M_DEVBUF);

	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
em_shutdown(device_t dev)
{
	return em_suspend(dev);
}

/*
 * Suspend/resume device methods.
 */
static int
em_suspend(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);

	EM_CORE_LOCK(adapter);

        em_release_manageability(adapter);
	em_release_hw_control(adapter);
	em_enable_wakeup(dev);

	EM_CORE_UNLOCK(adapter);

	return bus_generic_suspend(dev);
}

static int
em_resume(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ifnet *ifp = adapter->ifp;

	EM_CORE_LOCK(adapter);
	em_init_locked(adapter);
	em_init_manageability(adapter);
	EM_CORE_UNLOCK(adapter);
	em_start(ifp);

	return bus_generic_resume(dev);
}


/*********************************************************************
 *  Transmit entry point
 *
 *  em_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

#ifdef EM_MULTIQUEUE
static int
em_mq_start_locked(struct ifnet *ifp, struct tx_ring *txr, struct mbuf *m)
{
	struct adapter  *adapter = txr->adapter;
        struct mbuf     *next;
        int             err = 0, enq = 0;

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING | IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING || adapter->link_active == 0) {
		if (m != NULL)
			err = drbr_enqueue(ifp, txr->br, m);
		return (err);
	}

        /* Call cleanup if number of TX descriptors low */
	if (txr->tx_avail <= EM_TX_CLEANUP_THRESHOLD)
		em_txeof(txr);

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
		if ((err = em_xmit(txr, &next)) != 0) {
                        if (next != NULL)
                                err = drbr_enqueue(ifp, txr->br, next);
                        break;
		}
		enq++;
		drbr_stats_update(ifp, next->m_pkthdr.len, next->m_flags);
		ETHER_BPF_MTAP(ifp, next);
		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
                        break;
		if (txr->tx_avail < EM_MAX_SCATTER) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			break;
		}
		next = drbr_dequeue(ifp, txr->br);
	}

	if (enq > 0) {
                /* Set the watchdog */
                txr->queue_status = EM_QUEUE_WORKING;
		txr->watchdog_time = ticks;
	}
	return (err);
}

/*
** Multiqueue capable stack interface
*/
static int
em_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	int 		error;

	if (EM_TX_TRYLOCK(txr)) {
		error = em_mq_start_locked(ifp, txr, m);
		EM_TX_UNLOCK(txr);
	} else 
		error = drbr_enqueue(ifp, txr->br, m);

	return (error);
}

/*
** Flush all ring buffers
*/
static void
em_qflush(struct ifnet *ifp)
{
	struct adapter  *adapter = ifp->if_softc;
	struct tx_ring  *txr = adapter->tx_rings;
	struct mbuf     *m;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		EM_TX_LOCK(txr);
		while ((m = buf_ring_dequeue_sc(txr->br)) != NULL)
			m_freem(m);
		EM_TX_UNLOCK(txr);
	}
	if_qflush(ifp);
}

#endif /* EM_MULTIQUEUE */

static void
em_start_locked(struct ifnet *ifp, struct tx_ring *txr)
{
	struct adapter	*adapter = ifp->if_softc;
	struct mbuf	*m_head;

	EM_TX_LOCK_ASSERT(txr);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;

	if (!adapter->link_active)
		return;

        /* Call cleanup if number of TX descriptors low */
	if (txr->tx_avail <= EM_TX_CLEANUP_THRESHOLD)
		em_txeof(txr);

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {
		if (txr->tx_avail < EM_MAX_SCATTER) {
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
		if (em_xmit(txr, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting. */
		txr->watchdog_time = ticks;
                txr->queue_status = EM_QUEUE_WORKING;
	}

	return;
}

static void
em_start(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		EM_TX_LOCK(txr);
		em_start_locked(ifp, txr);
		EM_TX_UNLOCK(txr);
	}
	return;
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
em_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
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
				EM_CORE_LOCK(adapter);
				em_init_locked(adapter);
				EM_CORE_UNLOCK(adapter);
			}
			arp_ifinit(ifp, ifa);
		} else
#endif
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		EM_CORE_LOCK(adapter);
		switch (adapter->hw.mac.type) {
		case e1000_82571:
		case e1000_82572:
		case e1000_ich9lan:
		case e1000_ich10lan:
		case e1000_pch2lan:
		case e1000_82574:
		case e1000_80003es2lan:	/* 9K Jumbo Frame size */
			max_frame_size = 9234;
			break;
		case e1000_pchlan:
			max_frame_size = 4096;
			break;
			/* Adapters that do not support jumbo frames */
		case e1000_82583:
		case e1000_ich8lan:
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
		em_init_locked(adapter);
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
					em_disable_promisc(adapter);
					em_set_promisc(adapter);
				}
			} else
				em_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				em_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		EM_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			EM_CORE_LOCK(adapter);
			em_disable_intr(adapter);
			em_set_multi(adapter);
#ifdef DEVICE_POLLING
			if (!(ifp->if_capenable & IFCAP_POLLING))
#endif
				em_enable_intr(adapter);
			EM_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
		/*
		** As the speed/duplex settings are being
		** changed, we need to reset the PHY.
		*/
		adapter->hw.phy.reset_disable = FALSE;
		/* Check SOL/IDER usage */
		EM_CORE_LOCK(adapter);
		if (e1000_check_reset_block(&adapter->hw)) {
			EM_CORE_UNLOCK(adapter);
			device_printf(adapter->dev, "Media change is"
			    " blocked due to SOL/IDER session.\n");
			break;
		}
		EM_CORE_UNLOCK(adapter);
		/* falls thru */
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
				error = ether_poll_register(em_poll, ifp);
				if (error)
					return (error);
				EM_CORE_LOCK(adapter);
				em_disable_intr(adapter);
				ifp->if_capenable |= IFCAP_POLLING;
				EM_CORE_UNLOCK(adapter);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				EM_CORE_LOCK(adapter);
				em_enable_intr(adapter);
				ifp->if_capenable &= ~IFCAP_POLLING;
				EM_CORE_UNLOCK(adapter);
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
		if ((mask & IFCAP_WOL) &&
		    (ifp->if_capabilities & IFCAP_WOL) != 0) {
			if (mask & IFCAP_WOL_MCAST)
				ifp->if_capenable ^= IFCAP_WOL_MCAST;
			if (mask & IFCAP_WOL_MAGIC)
				ifp->if_capenable ^= IFCAP_WOL_MAGIC;
		}
		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING))
			em_init(adapter);
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
em_init_locked(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t	dev = adapter->dev;
	u32		pba;

	INIT_DEBUGOUT("em_init: begin");

	EM_CORE_LOCK_ASSERT(adapter);

	em_disable_intr(adapter);
	callout_stop(&adapter->timer);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	switch (adapter->hw.mac.type) {
	/* Total Packet Buffer on these is 48K */
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
			pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		break;
	case e1000_82573: /* 82573: Total Packet Buffer is 32K */
			pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
		break;
	case e1000_82574:
	case e1000_82583:
			pba = E1000_PBA_20K; /* 20K for Rx, 20K for Tx */
		break;
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
		break;
	case e1000_ich9lan:
	case e1000_ich10lan:
		pba = E1000_PBA_10K;
		break;
	case e1000_pchlan:
	case e1000_pch2lan:
		pba = E1000_PBA_26K;
		break;
	default:
		if (adapter->max_frame_size > 8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}

	INIT_DEBUGOUT1("em_init: pba=%dK",pba);
	E1000_WRITE_REG(&adapter->hw, E1000_PBA, pba);
	
	/* Get the latest mac address, User can use a LAA */
        bcopy(IF_LLADDR(adapter->ifp), adapter->hw.mac.addr,
              ETHER_ADDR_LEN);

	/* Put the address into the Receive Address Array */
	e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	/*
	 * With the 82571 adapter, RAR[0] may be overwritten
	 * when the other port is reset, we make a duplicate
	 * in RAR[14] for that eventuality, this assures
	 * the interface continues to function.
	 */
	if (adapter->hw.mac.type == e1000_82571) {
		e1000_set_laa_state_82571(&adapter->hw, TRUE);
		e1000_rar_set(&adapter->hw, adapter->hw.mac.addr,
		    E1000_RAR_ENTRIES - 1);
	}

	/* Initialize the hardware */
	em_reset(adapter);
	em_update_link_status(adapter);

	/* Setup VLAN support, basic and offload if available */
	E1000_WRITE_REG(&adapter->hw, E1000_VET, ETHERTYPE_VLAN);

	/* Set hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;

	/* Configure for OS presence */
	em_init_manageability(adapter);

	/* Prepare transmit descriptors and buffers */
	em_setup_transmit_structures(adapter);
	em_initialize_transmit_unit(adapter);

	/* Setup Multicast table */
	em_set_multi(adapter);

	/*
	** Figure out the desired mbuf
	** pool for doing jumbos
	*/
	if (adapter->max_frame_size <= 2048)
		adapter->rx_mbuf_sz = MCLBYTES;
	else if (adapter->max_frame_size <= 4096)
		adapter->rx_mbuf_sz = MJUMPAGESIZE;
	else
		adapter->rx_mbuf_sz = MJUM9BYTES;

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		em_stop(adapter);
		return;
	}
	em_initialize_receive_unit(adapter);

	/* Use real VLAN Filter support? */
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) {
		if (ifp->if_capenable & IFCAP_VLAN_HWFILTER)
			/* Use real VLAN Filter support */
			em_setup_vlan_hw_support(adapter);
		else {
			u32 ctrl;
			ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
			ctrl |= E1000_CTRL_VME;
			E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
		}
	}

	/* Don't lose promiscuous settings */
	em_set_promisc(adapter);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	e1000_clear_hw_cntrs_base_generic(&adapter->hw);

	/* MSI/X configuration for 82574 */
	if (adapter->hw.mac.type == e1000_82574) {
		int tmp;
		tmp = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
		tmp |= E1000_CTRL_EXT_PBA_CLR;
		E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT, tmp);
		/* Set the IVAR - interrupt vector routing. */
		E1000_WRITE_REG(&adapter->hw, E1000_IVAR, adapter->ivars);
	}

#ifdef DEVICE_POLLING
	/*
	 * Only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		em_disable_intr(adapter);
	else
#endif /* DEVICE_POLLING */
		em_enable_intr(adapter);

	/* AMT based hardware can now take control from firmware */
	if (adapter->has_manage && adapter->has_amt)
		em_get_hw_control(adapter);

	/* Don't reset the phy next time init gets called */
	adapter->hw.phy.reset_disable = TRUE;
}

static void
em_init(void *arg)
{
	struct adapter *adapter = arg;

	EM_CORE_LOCK(adapter);
	em_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}


#ifdef DEVICE_POLLING
/*********************************************************************
 *
 *  Legacy polling routine: note this only works with single queue
 *
 *********************************************************************/
static int
em_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	struct tx_ring	*txr = adapter->tx_rings;
	struct rx_ring	*rxr = adapter->rx_rings;
	u32		reg_icr;
	int		rx_done;

	EM_CORE_LOCK(adapter);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		EM_CORE_UNLOCK(adapter);
		return (0);
	}

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&adapter->timer);
			adapter->hw.mac.get_link_status = 1;
			em_update_link_status(adapter);
			callout_reset(&adapter->timer, hz,
			    em_local_timer, adapter);
		}
	}
	EM_CORE_UNLOCK(adapter);

	em_rxeof(rxr, count, &rx_done);

	EM_TX_LOCK(txr);
	em_txeof(txr);
#ifdef EM_MULTIQUEUE
	if (!drbr_empty(ifp, txr->br))
		em_mq_start_locked(ifp, txr, NULL);
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp, txr);
#endif
	EM_TX_UNLOCK(txr);

	return (rx_done);
}
#endif /* DEVICE_POLLING */


/*********************************************************************
 *
 *  Fast Legacy/MSI Combined Interrupt Service routine  
 *
 *********************************************************************/
static int
em_irq_fast(void *arg)
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
	 * Starting with the 82571 chip, bit 31 should be used to
	 * determine whether the interrupt belongs to us.
	 */
	if (adapter->hw.mac.type >= e1000_82571 &&
	    (reg_icr & E1000_ICR_INT_ASSERTED) == 0)
		return FILTER_STRAY;

	em_disable_intr(adapter);
	taskqueue_enqueue(adapter->tq, &adapter->que_task);

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
		adapter->hw.mac.get_link_status = 1;
		taskqueue_enqueue(taskqueue_fast, &adapter->link_task);
	}

	if (reg_icr & E1000_ICR_RXO)
		adapter->rx_overruns++;
	return FILTER_HANDLED;
}

/* Combined RX/TX handler, used by Legacy and MSI */
static void
em_handle_que(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring	*txr = adapter->tx_rings;
	struct rx_ring	*rxr = adapter->rx_rings;
	bool		more;


	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = em_rxeof(rxr, adapter->rx_process_limit, NULL);

		EM_TX_LOCK(txr);
		em_txeof(txr);
#ifdef EM_MULTIQUEUE
		if (!drbr_empty(ifp, txr->br))
			em_mq_start_locked(ifp, txr, NULL);
#else
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			em_start_locked(ifp, txr);
#endif
		em_txeof(txr);
		EM_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(adapter->tq, &adapter->que_task);
			return;
		}
	}

	em_enable_intr(adapter);
	return;
}


/*********************************************************************
 *
 *  MSIX Interrupt Service Routines
 *
 **********************************************************************/
static void
em_msix_tx(void *arg)
{
	struct tx_ring *txr = arg;
	struct adapter *adapter = txr->adapter;
	bool		more;

	++txr->tx_irq;
	EM_TX_LOCK(txr);
	more = em_txeof(txr);
	EM_TX_UNLOCK(txr);
	if (more)
		taskqueue_enqueue(txr->tq, &txr->tx_task);
	else
		/* Reenable this interrupt */
		E1000_WRITE_REG(&adapter->hw, E1000_IMS, txr->ims);
	return;
}

/*********************************************************************
 *
 *  MSIX RX Interrupt Service routine
 *
 **********************************************************************/

static void
em_msix_rx(void *arg)
{
	struct rx_ring	*rxr = arg;
	struct adapter	*adapter = rxr->adapter;
	bool		more;

	++rxr->rx_irq;
	more = em_rxeof(rxr, adapter->rx_process_limit, NULL);
	if (more)
		taskqueue_enqueue(rxr->tq, &rxr->rx_task);
	else
		/* Reenable this interrupt */
		E1000_WRITE_REG(&adapter->hw, E1000_IMS, rxr->ims);
	return;
}

/*********************************************************************
 *
 *  MSIX Link Fast Interrupt Service routine
 *
 **********************************************************************/
static void
em_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	u32		reg_icr;

	++adapter->link_irq;
	reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);

	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
		adapter->hw.mac.get_link_status = 1;
		em_handle_link(adapter, 0);
	} else
		E1000_WRITE_REG(&adapter->hw, E1000_IMS,
		    EM_MSIX_LINK | E1000_IMS_LSC);
	return;
}

static void
em_handle_rx(void *context, int pending)
{
	struct rx_ring	*rxr = context;
	struct adapter	*adapter = rxr->adapter;
        bool            more;

	more = em_rxeof(rxr, adapter->rx_process_limit, NULL);
	if (more)
		taskqueue_enqueue(rxr->tq, &rxr->rx_task);
	else
		/* Reenable this interrupt */
		E1000_WRITE_REG(&adapter->hw, E1000_IMS, rxr->ims);
}

static void
em_handle_tx(void *context, int pending)
{
	struct tx_ring	*txr = context;
	struct adapter	*adapter = txr->adapter;
	struct ifnet	*ifp = adapter->ifp;

	EM_TX_LOCK(txr);
	em_txeof(txr);
#ifdef EM_MULTIQUEUE
	if (!drbr_empty(ifp, txr->br))
		em_mq_start_locked(ifp, txr, NULL);
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp, txr);
#endif
	em_txeof(txr);
	E1000_WRITE_REG(&adapter->hw, E1000_IMS, txr->ims);
	EM_TX_UNLOCK(txr);
}

static void
em_handle_link(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet *ifp = adapter->ifp;

	if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	EM_CORE_LOCK(adapter);
	callout_stop(&adapter->timer);
	em_update_link_status(adapter);
	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	E1000_WRITE_REG(&adapter->hw, E1000_IMS,
	    EM_MSIX_LINK | E1000_IMS_LSC);
	EM_CORE_UNLOCK(adapter);
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
em_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct adapter *adapter = ifp->if_softc;
	u_char fiber_type = IFM_1000_SX;

	INIT_DEBUGOUT("em_media_status: begin");

	EM_CORE_LOCK(adapter);
	em_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active) {
		EM_CORE_UNLOCK(adapter);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes)) {
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
em_media_change(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia  *ifm = &adapter->media;

	INIT_DEBUGOUT("em_media_change: begin");

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

	em_init_locked(adapter);
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
em_xmit(struct tx_ring *txr, struct mbuf **m_headp)
{
	struct adapter		*adapter = txr->adapter;
	bus_dma_segment_t	segs[EM_MAX_SCATTER];
	bus_dmamap_t		map;
	struct em_buffer	*tx_buffer, *tx_buffer_mapped;
	struct e1000_tx_desc	*ctxd = NULL;
	struct mbuf		*m_head;
	struct ether_header	*eh;
	struct ip		*ip = NULL;
	struct tcphdr		*tp = NULL;
	u32			txd_upper, txd_lower, txd_used, txd_saved;
	int			ip_off, poff;
	int			nsegs, i, j, first, last = 0;
	int			error, do_tso, tso_desc = 0;

	m_head = *m_headp;
	txd_upper = txd_lower = txd_used = txd_saved = 0;
	do_tso = ((m_head->m_pkthdr.csum_flags & CSUM_TSO) != 0);
	ip_off = poff = 0;

	/*
	 * Intel recommends entire IP/TCP header length reside in a single
	 * buffer. If multiple descriptors are used to describe the IP and
	 * TCP header, each descriptor should describe one or more
	 * complete headers; descriptors referencing only parts of headers
	 * are not supported. If all layer headers are not coalesced into
	 * a single buffer, each buffer should not cross a 4KB boundary,
	 * or be larger than the maximum read request size.
	 * Controller also requires modifing IP/TCP header to make TSO work
	 * so we firstly get a writable mbuf chain then coalesce ethernet/
	 * IP/TCP header into a single buffer to meet the requirement of
	 * controller. This also simplifies IP/TCP/UDP checksum offloading
	 * which also has similiar restrictions.
	 */
	if (do_tso || m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD) {
		if (do_tso || (m_head->m_next != NULL && 
		    m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD)) {
			if (M_WRITABLE(*m_headp) == 0) {
				m_head = m_dup(*m_headp, M_DONTWAIT);
				m_freem(*m_headp);
				if (m_head == NULL) {
					*m_headp = NULL;
					return (ENOBUFS);
				}
				*m_headp = m_head;
			}
		}
		/*
		 * XXX
		 * Assume IPv4, we don't have TSO/checksum offload support
		 * for IPv6 yet.
		 */
		ip_off = sizeof(struct ether_header);
		m_head = m_pullup(m_head, ip_off);
		if (m_head == NULL) {
			*m_headp = NULL;
			return (ENOBUFS);
		}
		eh = mtod(m_head, struct ether_header *);
		if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
			ip_off = sizeof(struct ether_vlan_header);
			m_head = m_pullup(m_head, ip_off);
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
		}
		m_head = m_pullup(m_head, ip_off + sizeof(struct ip));
		if (m_head == NULL) {
			*m_headp = NULL;
			return (ENOBUFS);
		}
		ip = (struct ip *)(mtod(m_head, char *) + ip_off);
		poff = ip_off + (ip->ip_hl << 2);
		if (do_tso) {
			m_head = m_pullup(m_head, poff + sizeof(struct tcphdr));
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
			tp = (struct tcphdr *)(mtod(m_head, char *) + poff);
			/*
			 * TSO workaround:
			 *   pull 4 more bytes of data into it.
			 */
			m_head = m_pullup(m_head, poff + (tp->th_off << 2) + 4);
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
			ip = (struct ip *)(mtod(m_head, char *) + ip_off);
			ip->ip_len = 0;
			ip->ip_sum = 0;
			/*
			 * The pseudo TCP checksum does not include TCP payload
			 * length so driver should recompute the checksum here
			 * what hardware expect to see. This is adherence of
			 * Microsoft's Large Send specification.
			 */
			tp = (struct tcphdr *)(mtod(m_head, char *) + poff);
			tp->th_sum = in_pseudo(ip->ip_src.s_addr,
			    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
		} else if (m_head->m_pkthdr.csum_flags & CSUM_TCP) {
			m_head = m_pullup(m_head, poff + sizeof(struct tcphdr));
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
			tp = (struct tcphdr *)(mtod(m_head, char *) + poff);
			m_head = m_pullup(m_head, poff + (tp->th_off << 2));
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
			ip = (struct ip *)(mtod(m_head, char *) + ip_off);
			tp = (struct tcphdr *)(mtod(m_head, char *) + poff);
		} else if (m_head->m_pkthdr.csum_flags & CSUM_UDP) {
			m_head = m_pullup(m_head, poff + sizeof(struct udphdr));
			if (m_head == NULL) {
				*m_headp = NULL;
				return (ENOBUFS);
			}
			ip = (struct ip *)(mtod(m_head, char *) + ip_off);
		}
		*m_headp = m_head;
	}

	/*
	 * Map the packet for DMA
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

		m = m_defrag(*m_headp, M_DONTWAIT);
		if (m == NULL) {
			adapter->mbuf_alloc_failed++;
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

	/*
	 * TSO Hardware workaround, if this packet is not
	 * TSO, and is only a single descriptor long, and
	 * it follows a TSO burst, then we need to add a
	 * sentinel descriptor to prevent premature writeback.
	 */
	if ((do_tso == 0) && (txr->tx_tso == TRUE)) {
		if (nsegs == 1)
			tso_desc = TRUE;
		txr->tx_tso = FALSE;
	}

        if (nsegs > (txr->tx_avail - 2)) {
                txr->no_desc_avail++;
		bus_dmamap_unload(txr->txtag, map);
		return (ENOBUFS);
        }
	m_head = *m_headp;

	/* Do hardware assists */
	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		em_tso_setup(txr, m_head, ip_off, ip, tp,
		    &txd_upper, &txd_lower);
		/* we need to make a final sentinel transmit desc */
		tso_desc = TRUE;
	} else if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD)
		em_transmit_checksum_setup(txr, m_head,
		    ip_off, ip, &txd_upper, &txd_lower);

	i = txr->next_avail_desc;

	/* Set up our transmit descriptors */
	for (j = 0; j < nsegs; j++) {
		bus_size_t seg_len;
		bus_addr_t seg_addr;

		tx_buffer = &txr->tx_buffers[i];
		ctxd = &txr->tx_base[i];
		seg_addr = segs[j].ds_addr;
		seg_len  = segs[j].ds_len;
		/*
		** TSO Workaround:
		** If this is the last descriptor, we want to
		** split it so we have a small final sentinel
		*/
		if (tso_desc && (j == (nsegs -1)) && (seg_len > 8)) {
			seg_len -= 4;
			ctxd->buffer_addr = htole64(seg_addr);
			ctxd->lower.data = htole32(
			adapter->txd_cmd | txd_lower | seg_len);
			ctxd->upper.data =
			    htole32(txd_upper);
			if (++i == adapter->num_tx_desc)
				i = 0;
			/* Now make the sentinel */	
			++txd_used; /* using an extra txd */
			ctxd = &txr->tx_base[i];
			tx_buffer = &txr->tx_buffers[i];
			ctxd->buffer_addr =
			    htole64(seg_addr + seg_len);
			ctxd->lower.data = htole32(
			adapter->txd_cmd | txd_lower | 4);
			ctxd->upper.data =
			    htole32(txd_upper);
			last = i;
			if (++i == adapter->num_tx_desc)
				i = 0;
		} else {
			ctxd->buffer_addr = htole64(seg_addr);
			ctxd->lower.data = htole32(
			adapter->txd_cmd | txd_lower | seg_len);
			ctxd->upper.data =
			    htole32(txd_upper);
			last = i;
			if (++i == adapter->num_tx_desc)
				i = 0;
		}
		tx_buffer->m_head = NULL;
		tx_buffer->next_eop = -1;
	}

	txr->next_avail_desc = i;
	txr->tx_avail -= nsegs;
	if (tso_desc) /* TSO used an extra for sentinel */
		txr->tx_avail -= txd_used;

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
        bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

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
	tx_buffer = &txr->tx_buffers[first];
	tx_buffer->next_eop = last;
	/* Update the watchdog time early and often */
	txr->watchdog_time = ticks;

	/*
	 * Advance the Transmit Descriptor Tail (TDT), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	E1000_WRITE_REG(&adapter->hw, E1000_TDT(txr->me), i);

	return (0);
}

static void
em_set_promisc(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	u32		reg_rctl;

	reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		/* Turn this on if you want to see bad packets */
		if (em_debug_sbp)
			reg_rctl |= E1000_RCTL_SBP;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	}
}

static void
em_disable_promisc(struct adapter *adapter)
{
	u32	reg_rctl;

	reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);

	reg_rctl &=  (~E1000_RCTL_UPE);
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
em_set_multi(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	struct ifmultiaddr *ifma;
	u32 reg_rctl = 0;
	u8  *mta; /* Multicast array memory */
	int mcnt = 0;

	IOCTL_DEBUGOUT("em_set_multi: begin");

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
em_local_timer(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring	*txr = adapter->tx_rings;

	EM_CORE_LOCK_ASSERT(adapter);

	em_update_link_status(adapter);
	em_update_stats_counters(adapter);

	/* Reset LAA into RAR[0] on 82571 */
	if ((adapter->hw.mac.type == e1000_82571) &&
	    e1000_get_laa_state_82571(&adapter->hw))
		e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	/* 
	** Don't do TX watchdog check if we've been paused
	*/
	if (adapter->pause_frames) {
		adapter->pause_frames = 0;
		goto out;
	}
	/*
	** Check on the state of the TX queue(s), this 
	** can be done without the lock because its RO
	** and the HUNG state will be static if set.
	*/
	for (int i = 0; i < adapter->num_queues; i++, txr++)
		if (txr->queue_status == EM_QUEUE_HUNG)
			goto hung;
out:
	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	return;
hung:
	/* Looks like we're hung */
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	device_printf(adapter->dev,
	    "Queue(%d) tdh = %d, hw tdt = %d\n", txr->me,
	    E1000_READ_REG(&adapter->hw, E1000_TDH(txr->me)),
	    E1000_READ_REG(&adapter->hw, E1000_TDT(txr->me)));
	device_printf(adapter->dev,"TX(%d) desc avail = %d,"
	    "Next TX to Clean = %d\n",
	    txr->me, txr->tx_avail, txr->next_to_clean);
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	em_init_locked(adapter);
}


static void
em_update_link_status(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	struct ifnet *ifp = adapter->ifp;
	device_t dev = adapter->dev;
	struct tx_ring *txr = adapter->tx_rings;
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
		/* Check if we must disable SPEED_MODE bit on PCI-E */
		if ((adapter->link_speed != SPEED_1000) &&
		    ((hw->mac.type == e1000_82571) ||
		    (hw->mac.type == e1000_82572))) {
			int tarc0;
			tarc0 = E1000_READ_REG(hw, E1000_TARC(0));
			tarc0 &= ~SPEED_MODE_BIT;
			E1000_WRITE_REG(hw, E1000_TARC(0), tarc0);
		}
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
		for (int i = 0; i < adapter->num_queues; i++, txr++)
			txr->queue_status = EM_QUEUE_IDLE;
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
em_stop(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;
	struct tx_ring	*txr = adapter->tx_rings;

	EM_CORE_LOCK_ASSERT(adapter);

	INIT_DEBUGOUT("em_stop: begin");

	em_disable_intr(adapter);
	callout_stop(&adapter->timer);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

        /* Unarm watchdog timer. */
	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		EM_TX_LOCK(txr);
		txr->queue_status = EM_QUEUE_IDLE;
		EM_TX_UNLOCK(txr);
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
em_identify_hardware(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	/* Make sure our PCI config space has the necessary stuff set */
	adapter->hw.bus.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
	if (!((adapter->hw.bus.pci_cmd_word & PCIM_CMD_BUSMASTEREN) &&
	    (adapter->hw.bus.pci_cmd_word & PCIM_CMD_MEMEN))) {
		device_printf(dev, "Memory Access and/or Bus Master bits "
		    "were not set!\n");
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

	/* Do Shared Code Init and Setup */
	if (e1000_set_mac_type(&adapter->hw)) {
		device_printf(dev, "Setup init failure\n");
		return;
	}
}

static int
em_allocate_pci_resources(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	int		rid;

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

	/* Default to a single queue */
	adapter->num_queues = 1;

	/*
	 * Setup MSI/X or MSI if PCI Express
	 */
	adapter->msix = em_setup_msix(adapter);

	adapter->hw.back = &adapter->osdep;

	return (0);
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
int
em_allocate_legacy(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int error, rid = 0;

	/* Manually turn off all interrupts */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	if (adapter->msix == 1) /* using MSI */
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
	 * Allocate a fast interrupt and the associated
	 * deferred processing contexts.
	 */
	TASK_INIT(&adapter->que_task, 0, em_handle_que, adapter);
	TASK_INIT(&adapter->link_task, 0, em_handle_link, adapter);
	adapter->tq = taskqueue_create_fast("em_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(adapter->dev));
	if ((error = bus_setup_intr(dev, adapter->res, INTR_TYPE_NET,
	    em_irq_fast, NULL, adapter, &adapter->tag)) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
			    "handler: %d\n", error);
		taskqueue_free(adapter->tq);
		adapter->tq = NULL;
		return (error);
	}
	
	return (0);
}

/*********************************************************************
 *
 *  Setup the MSIX Interrupt handlers
 *   This is not really Multiqueue, rather
 *   its just multiple interrupt vectors.
 *
 **********************************************************************/
int
em_allocate_msix(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct		tx_ring *txr = adapter->tx_rings;
	struct		rx_ring *rxr = adapter->rx_rings;
	int		error, rid, vector = 0;


	/* Make sure all interrupts are disabled */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	/* First set up ring resources */
	for (int i = 0; i < adapter->num_queues; i++, txr++, rxr++) {

		/* RX ring */
		rid = vector + 1;

		rxr->res = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &rid, RF_ACTIVE);
		if (rxr->res == NULL) {
			device_printf(dev,
			    "Unable to allocate bus resource: "
			    "RX MSIX Interrupt %d\n", i);
			return (ENXIO);
		}
		if ((error = bus_setup_intr(dev, rxr->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_msix_rx,
		    rxr, &rxr->tag)) != 0) {
			device_printf(dev, "Failed to register RX handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, rxr->res, rxr->tag, "rx %d", i);
#endif
		rxr->msix = vector++; /* NOTE increment vector for TX */
		TASK_INIT(&rxr->rx_task, 0, em_handle_rx, rxr);
		rxr->tq = taskqueue_create_fast("em_rxq", M_NOWAIT,
		    taskqueue_thread_enqueue, &rxr->tq);
		taskqueue_start_threads(&rxr->tq, 1, PI_NET, "%s rxq",
		    device_get_nameunit(adapter->dev));
		/*
		** Set the bit to enable interrupt
		** in E1000_IMS -- bits 20 and 21
		** are for RX0 and RX1, note this has
		** NOTHING to do with the MSIX vector
		*/
		rxr->ims = 1 << (20 + i);
		adapter->ivars |= (8 | rxr->msix) << (i * 4);

		/* TX ring */
		rid = vector + 1;
		txr->res = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &rid, RF_ACTIVE);
		if (txr->res == NULL) {
			device_printf(dev,
			    "Unable to allocate bus resource: "
			    "TX MSIX Interrupt %d\n", i);
			return (ENXIO);
		}
		if ((error = bus_setup_intr(dev, txr->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_msix_tx,
		    txr, &txr->tag)) != 0) {
			device_printf(dev, "Failed to register TX handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, txr->res, txr->tag, "tx %d", i);
#endif
		txr->msix = vector++; /* Increment vector for next pass */
		TASK_INIT(&txr->tx_task, 0, em_handle_tx, txr);
		txr->tq = taskqueue_create_fast("em_txq", M_NOWAIT,
		    taskqueue_thread_enqueue, &txr->tq);
		taskqueue_start_threads(&txr->tq, 1, PI_NET, "%s txq",
		    device_get_nameunit(adapter->dev));
		/*
		** Set the bit to enable interrupt
		** in E1000_IMS -- bits 22 and 23
		** are for TX0 and TX1, note this has
		** NOTHING to do with the MSIX vector
		*/
		txr->ims = 1 << (22 + i);
		adapter->ivars |= (8 | txr->msix) << (8 + (i * 4));
	}

	/* Link interrupt */
	++rid;
	adapter->res = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (!adapter->res) {
		device_printf(dev,"Unable to allocate "
		    "bus resource: Link interrupt [%d]\n", rid);
		return (ENXIO);
        }
	/* Set the link handler function */
	error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    em_msix_link, adapter, &adapter->tag);
	if (error) {
		adapter->res = NULL;
		device_printf(dev, "Failed to register LINK handler");
		return (error);
	}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, adapter->res, adapter->tag, "link");
#endif
	adapter->linkvec = vector;
	adapter->ivars |=  (8 | vector) << 16;
	adapter->ivars |= 0x80000000;

	return (0);
}


static void
em_free_pci_resources(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct tx_ring	*txr;
	struct rx_ring	*rxr;
	int		rid;


	/*
	** Release all the queue interrupt resources:
	*/
	for (int i = 0; i < adapter->num_queues; i++) {
		txr = &adapter->tx_rings[i];
		rxr = &adapter->rx_rings[i];
		/* an early abort? */
		if ((txr == NULL) || (rxr == NULL))
			break;
		rid = txr->msix +1;
		if (txr->tag != NULL) {
			bus_teardown_intr(dev, txr->res, txr->tag);
			txr->tag = NULL;
		}
		if (txr->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ,
			    rid, txr->res);
		rid = rxr->msix +1;
		if (rxr->tag != NULL) {
			bus_teardown_intr(dev, rxr->res, rxr->tag);
			rxr->tag = NULL;
		}
		if (rxr->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ,
			    rid, rxr->res);
	}

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


	if (adapter->msix)
		pci_release_msi(dev);

	if (adapter->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(EM_MSIX_BAR), adapter->msix_mem);

	if (adapter->memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->memory);

	if (adapter->flash != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    EM_FLASH, adapter->flash);
}

/*
 * Setup MSI or MSI/X
 */
static int
em_setup_msix(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int val = 0;


	/*
	** Setup MSI/X for Hartwell: tests have shown
	** use of two queues to be unstable, and to
	** provide no great gain anyway, so we simply
	** seperate the interrupts and use a single queue.
	*/
	if ((adapter->hw.mac.type == e1000_82574) &&
	    (em_enable_msix == TRUE)) {
		/* Map the MSIX BAR */
		int rid = PCIR_BAR(EM_MSIX_BAR);
		adapter->msix_mem = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       		if (!adapter->msix_mem) {
			/* May not be enabled */
               		device_printf(adapter->dev,
			    "Unable to map MSIX table \n");
			goto msi;
       		}
		val = pci_msix_count(dev); 
		if (val < 3) {
			bus_release_resource(dev, SYS_RES_MEMORY,
			    PCIR_BAR(EM_MSIX_BAR), adapter->msix_mem);
			adapter->msix_mem = NULL;
               		device_printf(adapter->dev,
			    "MSIX: insufficient vectors, using MSI\n");
			goto msi;
		}
		val = 3;
		adapter->num_queues = 1;
		if (pci_alloc_msix(dev, &val) == 0) {
			device_printf(adapter->dev,
			    "Using MSIX interrupts "
			    "with %d vectors\n", val);
		}

		return (val);
	}
msi:
       	val = pci_msi_count(dev);
       	if (val == 1 && pci_alloc_msi(dev, &val) == 0) {
               	adapter->msix = 1;
               	device_printf(adapter->dev,"Using an MSI interrupt\n");
		return (val);
	} 
	/* Should only happen due to manual configuration */
	device_printf(adapter->dev,"No MSI/MSIX using a Legacy IRQ\n");
	return (0);
}


/*********************************************************************
 *
 *  Initialize the hardware to a configuration
 *  as specified by the adapter structure.
 *
 **********************************************************************/
static void
em_reset(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct ifnet	*ifp = adapter->ifp;
	struct e1000_hw	*hw = &adapter->hw;
	u16		rx_buffer_size;

	INIT_DEBUGOUT("em_reset: begin");

	/* Set up smart power down as default off on newer adapters. */
	if (!em_smart_pwr_down && (hw->mac.type == e1000_82571 ||
	    hw->mac.type == e1000_82572)) {
		u16 phy_tmp = 0;

		/* Speed up time to link by disabling smart power down. */
		e1000_read_phy_reg(hw, IGP02E1000_PHY_POWER_MGMT, &phy_tmp);
		phy_tmp &= ~IGP02E1000_PM_SPD;
		e1000_write_phy_reg(hw, IGP02E1000_PHY_POWER_MGMT, phy_tmp);
	}

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
	rx_buffer_size = ((E1000_READ_REG(hw, E1000_PBA) & 0xffff) << 10 );

	hw->fc.high_water = rx_buffer_size -
	    roundup2(adapter->max_frame_size, 1024);
	hw->fc.low_water = hw->fc.high_water - 1500;

	if (hw->mac.type == e1000_80003es2lan)
		hw->fc.pause_time = 0xFFFF;
	else
		hw->fc.pause_time = EM_FC_PAUSE_TIME;

	hw->fc.send_xon = TRUE;

        /* Set Flow control, use the tunable location if sane */
	hw->fc.requested_mode = adapter->fc_setting;

	/* Workaround: no TX flow ctrl for PCH */
	if (hw->mac.type == e1000_pchlan)
                hw->fc.requested_mode = e1000_fc_rx_pause;

	/* Override - settings for PCH2LAN, ya its magic :) */
	if (hw->mac.type == e1000_pch2lan) {
		hw->fc.high_water = 0x5C20;
		hw->fc.low_water = 0x5048;
		hw->fc.pause_time = 0x0650;
		hw->fc.refresh_time = 0x0400;
		/* Jumbos need adjusted PBA */
		if (ifp->if_mtu > ETHERMTU)
			E1000_WRITE_REG(hw, E1000_PBA, 12);
		else
			E1000_WRITE_REG(hw, E1000_PBA, 26);
	}

	/* Issue a global reset */
	e1000_reset_hw(hw);
	E1000_WRITE_REG(hw, E1000_WUC, 0);
	em_disable_aspm(adapter);

	if (e1000_init_hw(hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed\n");
		return;
	}

	E1000_WRITE_REG(hw, E1000_VET, ETHERTYPE_VLAN);
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
em_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_init =  em_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, adapter->num_tx_desc - 1);
	ifp->if_snd.ifq_drv_maxlen = adapter->num_tx_desc - 1;
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, adapter->hw.mac.addr);

	ifp->if_capabilities = ifp->if_capenable = 0;

#ifdef EM_MULTIQUEUE
	/* Multiqueue tx functions */
	ifp->if_transmit = em_mq_start;
	ifp->if_qflush = em_qflush;
#endif	

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
	ifp->if_capenable |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;

	/* Enable TSO by default, can disable with ifconfig */
	ifp->if_capabilities |= IFCAP_TSO4;
	ifp->if_capenable |= IFCAP_TSO4;

	/*
	 * Tell the upper layer(s) we
	 * support full VLAN capability
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
	    em_media_change, em_media_status);
	if ((adapter->hw.phy.media_type == e1000_media_type_fiber) ||
	    (adapter->hw.phy.media_type == e1000_media_type_internal_serdes)) {
		u_char fiber_type = IFM_1000_SX;	/* default type */

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


/*
 * Manage DMA'able memory.
 */
static void
em_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs[0].ds_addr;
}

static int
em_dma_malloc(struct adapter *adapter, bus_size_t size,
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
	    size, em_dmamap_cb, &dma->dma_paddr, mapflags | BUS_DMA_NOWAIT);
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
em_dma_free(struct adapter *adapter, struct em_dma_alloc *dma)
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
em_allocate_queues(struct adapter *adapter)
{
	device_t		dev = adapter->dev;
	struct tx_ring		*txr = NULL;
	struct rx_ring		*rxr = NULL;
	int rsize, tsize, error = E1000_SUCCESS;
	int txconf = 0, rxconf = 0;


	/* Allocate the TX ring struct memory */
	if (!(adapter->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    adapter->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate TX ring memory\n");
		error = ENOMEM;
		goto fail;
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
	    sizeof(struct e1000_tx_desc), EM_DBA_ALIGN);
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

		if (em_dma_malloc(adapter, tsize,
			&txr->txdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto err_tx_desc;
		}
		txr->tx_base = (struct e1000_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);

        	if (em_allocate_transmit_buffers(txr)) {
			device_printf(dev,
			    "Critical Failure setting up transmit buffers\n");
			error = ENOMEM;
			goto err_tx_desc;
        	}
#if __FreeBSD_version >= 800000
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(4096, M_DEVBUF,
		    M_WAITOK, &txr->tx_mtx);
#endif
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(struct e1000_rx_desc), EM_DBA_ALIGN);
	for (int i = 0; i < adapter->num_queues; i++, rxconf++) {
		rxr = &adapter->rx_rings[i];
		rxr->adapter = adapter;
		rxr->me = i;

		/* Initialize the RX lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), txr->me);
		mtx_init(&rxr->rx_mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (em_dma_malloc(adapter, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			device_printf(dev,
			    "Unable to allocate RxDescriptor memory\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
		rxr->rx_base = (struct e1000_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);

        	/* Allocate receive buffers for the ring*/
		if (em_allocate_receive_buffers(rxr)) {
			device_printf(dev,
			    "Critical Failure setting up receive buffers\n");
			error = ENOMEM;
			goto err_rx_desc;
		}
	}

	return (0);

err_rx_desc:
	for (rxr = adapter->rx_rings; rxconf > 0; rxr++, rxconf--)
		em_dma_free(adapter, &rxr->rxdma);
err_tx_desc:
	for (txr = adapter->tx_rings; txconf > 0; txr++, txconf--)
		em_dma_free(adapter, &txr->txdma);
	free(adapter->rx_rings, M_DEVBUF);
rx_fail:
#if __FreeBSD_version >= 800000
	buf_ring_free(txr->br, M_DEVBUF);
#endif
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
em_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	device_t dev = adapter->dev;
	struct em_buffer *txbuf;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev),
			       1, 0,			/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       EM_TSO_SIZE,		/* maxsize */
			       EM_MAX_SCATTER,		/* nsegments */
			       PAGE_SIZE,		/* maxsegsize */
			       0,			/* flags */
			       NULL,			/* lockfunc */
			       NULL,			/* lockfuncarg */
			       &txr->txtag))) {
		device_printf(dev,"Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if (!(txr->tx_buffers =
	    (struct em_buffer *) malloc(sizeof(struct em_buffer) *
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
	em_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
static void
em_setup_transmit_ring(struct tx_ring *txr)
{
	struct adapter *adapter = txr->adapter;
	struct em_buffer *txbuf;
	int i;

	/* Clear the old descriptor contents */
	EM_TX_LOCK(txr);
	bzero((void *)txr->tx_base,
	      (sizeof(struct e1000_tx_desc)) * adapter->num_tx_desc);
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
	txr->queue_status = EM_QUEUE_IDLE;

	/* Clear checksum offload context. */
	txr->last_hw_offload = 0;
	txr->last_hw_ipcss = 0;
	txr->last_hw_ipcso = 0;
	txr->last_hw_tucss = 0;
	txr->last_hw_tucso = 0;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	EM_TX_UNLOCK(txr);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
static void
em_setup_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++)
		em_setup_transmit_ring(txr);

	return;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct e1000_hw	*hw = &adapter->hw;
	u32	tctl, tarc, tipg = 0;

	 INIT_DEBUGOUT("em_initialize_transmit_unit: begin");

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64 bus_addr = txr->txdma.dma_paddr;
		/* Base and Len of TX Ring */
		E1000_WRITE_REG(hw, E1000_TDLEN(i),
	    	    adapter->num_tx_desc * sizeof(struct e1000_tx_desc));
		E1000_WRITE_REG(hw, E1000_TDBAH(i),
	    	    (u32)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_TDBAL(i),
	    	    (u32)bus_addr);
		/* Init the HEAD/TAIL indices */
		E1000_WRITE_REG(hw, E1000_TDT(i), 0);
		E1000_WRITE_REG(hw, E1000_TDH(i), 0);

		HW_DEBUGOUT2("Base = %x, Length = %x\n",
		    E1000_READ_REG(&adapter->hw, E1000_TDBAL(i)),
		    E1000_READ_REG(&adapter->hw, E1000_TDLEN(i)));

		txr->queue_status = EM_QUEUE_IDLE;
	}

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (adapter->hw.mac.type) {
	case e1000_82542:
		tipg = DEFAULT_82542_TIPG_IPGT;
		tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	case e1000_80003es2lan:
		tipg = DEFAULT_82543_TIPG_IPGR1;
		tipg |= DEFAULT_80003ES2LAN_TIPG_IPGR2 <<
		    E1000_TIPG_IPGR2_SHIFT;
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

	if ((adapter->hw.mac.type == e1000_82571) ||
	    (adapter->hw.mac.type == e1000_82572)) {
		tarc = E1000_READ_REG(&adapter->hw, E1000_TARC(0));
		tarc |= SPEED_MODE_BIT;
		E1000_WRITE_REG(&adapter->hw, E1000_TARC(0), tarc);
	} else if (adapter->hw.mac.type == e1000_80003es2lan) {
		tarc = E1000_READ_REG(&adapter->hw, E1000_TARC(0));
		tarc |= 1;
		E1000_WRITE_REG(&adapter->hw, E1000_TARC(0), tarc);
		tarc = E1000_READ_REG(&adapter->hw, E1000_TARC(1));
		tarc |= 1;
		E1000_WRITE_REG(&adapter->hw, E1000_TARC(1), tarc);
	}

	adapter->txd_cmd = E1000_TXD_CMD_IFCS;
	if (adapter->tx_int_delay.value > 0)
		adapter->txd_cmd |= E1000_TXD_CMD_IDE;

	/* Program the Transmit Control Register */
	tctl = E1000_READ_REG(&adapter->hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	if (adapter->hw.mac.type >= e1000_82571)
		tctl |= E1000_TCTL_MULR;

	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(&adapter->hw, E1000_TCTL, tctl);

}


/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
static void
em_free_transmit_structures(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		EM_TX_LOCK(txr);
		em_free_transmit_buffers(txr);
		em_dma_free(adapter, &txr->txdma);
		EM_TX_UNLOCK(txr);
		EM_TX_LOCK_DESTROY(txr);
	}

	free(adapter->tx_rings, M_DEVBUF);
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
static void
em_free_transmit_buffers(struct tx_ring *txr)
{
	struct adapter		*adapter = txr->adapter;
	struct em_buffer	*txbuf;

	INIT_DEBUGOUT("free_transmit_ring: begin");

	if (txr->tx_buffers == NULL)
		return;

	for (int i = 0; i < adapter->num_tx_desc; i++) {
		txbuf = &txr->tx_buffers[i];
		if (txbuf->m_head != NULL) {
			bus_dmamap_sync(txr->txtag, txbuf->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txtag,
			    txbuf->map);
			m_freem(txbuf->m_head);
			txbuf->m_head = NULL;
			if (txbuf->map != NULL) {
				bus_dmamap_destroy(txr->txtag,
				    txbuf->map);
				txbuf->map = NULL;
			}
		} else if (txbuf->map != NULL) {
			bus_dmamap_unload(txr->txtag,
			    txbuf->map);
			bus_dmamap_destroy(txr->txtag,
			    txbuf->map);
			txbuf->map = NULL;
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
 *  The offload context is protocol specific (TCP/UDP) and thus
 *  only needs to be set when the protocol changes. The occasion
 *  of a context change can be a performance detriment, and
 *  might be better just disabled. The reason arises in the way
 *  in which the controller supports pipelined requests from the
 *  Tx data DMA. Up to four requests can be pipelined, and they may
 *  belong to the same packet or to multiple packets. However all
 *  requests for one packet are issued before a request is issued
 *  for a subsequent packet and if a request for the next packet
 *  requires a context change, that request will be stalled
 *  until the previous request completes. This means setting up
 *  a new context effectively disables pipelined Tx data DMA which
 *  in turn greatly slow down performance to send small sized
 *  frames. 
 **********************************************************************/
static void
em_transmit_checksum_setup(struct tx_ring *txr, struct mbuf *mp, int ip_off,
    struct ip *ip, u32 *txd_upper, u32 *txd_lower)
{
	struct adapter			*adapter = txr->adapter;
	struct e1000_context_desc	*TXD = NULL;
	struct em_buffer		*tx_buffer;
	int				cur, hdr_len;
	u32				cmd = 0;
	u16				offload = 0;
	u8				ipcso, ipcss, tucso, tucss;

	ipcss = ipcso = tucss = tucso = 0;
	hdr_len = ip_off + (ip->ip_hl << 2);
	cur = txr->next_avail_desc;

	/* Setup of IP header checksum. */
	if (mp->m_pkthdr.csum_flags & CSUM_IP) {
		*txd_upper |= E1000_TXD_POPTS_IXSM << 8;
		offload |= CSUM_IP;
		ipcss = ip_off;
		ipcso = ip_off + offsetof(struct ip, ip_sum);
		/*
		 * Start offset for header checksum calculation.
		 * End offset for header checksum calculation.
		 * Offset of place to put the checksum.
		 */
		TXD = (struct e1000_context_desc *)&txr->tx_base[cur];
		TXD->lower_setup.ip_fields.ipcss = ipcss;
		TXD->lower_setup.ip_fields.ipcse = htole16(hdr_len);
		TXD->lower_setup.ip_fields.ipcso = ipcso;
		cmd |= E1000_TXD_CMD_IP;
	}

	if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
 		*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
 		*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
 		offload |= CSUM_TCP;
 		tucss = hdr_len;
 		tucso = hdr_len + offsetof(struct tcphdr, th_sum);
 		/*
 		 * Setting up new checksum offload context for every frames
 		 * takes a lot of processing time for hardware. This also
 		 * reduces performance a lot for small sized frames so avoid
 		 * it if driver can use previously configured checksum
 		 * offload context.
 		 */
 		if (txr->last_hw_offload == offload) {
 			if (offload & CSUM_IP) {
 				if (txr->last_hw_ipcss == ipcss &&
 				    txr->last_hw_ipcso == ipcso &&
 				    txr->last_hw_tucss == tucss &&
 				    txr->last_hw_tucso == tucso)
 					return;
 			} else {
 				if (txr->last_hw_tucss == tucss &&
 				    txr->last_hw_tucso == tucso)
 					return;
 			}
  		}
 		txr->last_hw_offload = offload;
 		txr->last_hw_tucss = tucss;
 		txr->last_hw_tucso = tucso;
 		/*
 		 * Start offset for payload checksum calculation.
 		 * End offset for payload checksum calculation.
 		 * Offset of place to put the checksum.
 		 */
		TXD = (struct e1000_context_desc *)&txr->tx_base[cur];
 		TXD->upper_setup.tcp_fields.tucss = hdr_len;
 		TXD->upper_setup.tcp_fields.tucse = htole16(0);
 		TXD->upper_setup.tcp_fields.tucso = tucso;
 		cmd |= E1000_TXD_CMD_TCP;
 	} else if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
 		*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
 		*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
 		tucss = hdr_len;
 		tucso = hdr_len + offsetof(struct udphdr, uh_sum);
 		/*
 		 * Setting up new checksum offload context for every frames
 		 * takes a lot of processing time for hardware. This also
 		 * reduces performance a lot for small sized frames so avoid
 		 * it if driver can use previously configured checksum
 		 * offload context.
 		 */
 		if (txr->last_hw_offload == offload) {
 			if (offload & CSUM_IP) {
 				if (txr->last_hw_ipcss == ipcss &&
 				    txr->last_hw_ipcso == ipcso &&
 				    txr->last_hw_tucss == tucss &&
 				    txr->last_hw_tucso == tucso)
 					return;
 			} else {
 				if (txr->last_hw_tucss == tucss &&
 				    txr->last_hw_tucso == tucso)
 					return;
 			}
 		}
 		txr->last_hw_offload = offload;
 		txr->last_hw_tucss = tucss;
 		txr->last_hw_tucso = tucso;
 		/*
 		 * Start offset for header checksum calculation.
 		 * End offset for header checksum calculation.
 		 * Offset of place to put the checksum.
 		 */
		TXD = (struct e1000_context_desc *)&txr->tx_base[cur];
 		TXD->upper_setup.tcp_fields.tucss = tucss;
 		TXD->upper_setup.tcp_fields.tucse = htole16(0);
 		TXD->upper_setup.tcp_fields.tucso = tucso;
  	}
  
 	if (offload & CSUM_IP) {
 		txr->last_hw_ipcss = ipcss;
 		txr->last_hw_ipcso = ipcso;
  	}

	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length =
	    htole32(adapter->txd_cmd | E1000_TXD_CMD_DEXT | cmd);
	tx_buffer = &txr->tx_buffers[cur];
	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++cur == adapter->num_tx_desc)
		cur = 0;

	txr->tx_avail--;
	txr->next_avail_desc = cur;
}


/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO)
 *
 **********************************************************************/
static void
em_tso_setup(struct tx_ring *txr, struct mbuf *mp, int ip_off,
    struct ip *ip, struct tcphdr *tp, u32 *txd_upper, u32 *txd_lower)
{
	struct adapter			*adapter = txr->adapter;
	struct e1000_context_desc	*TXD;
	struct em_buffer		*tx_buffer;
	int cur, hdr_len;

	/*
	 * In theory we can use the same TSO context if and only if
	 * frame is the same type(IP/TCP) and the same MSS. However
	 * checking whether a frame has the same IP/TCP structure is
	 * hard thing so just ignore that and always restablish a
	 * new TSO context.
	 */
	hdr_len = ip_off + (ip->ip_hl << 2) + (tp->th_off << 2);
	*txd_lower = (E1000_TXD_CMD_DEXT |	/* Extended descr type */
		      E1000_TXD_DTYP_D |	/* Data descr type */
		      E1000_TXD_CMD_TSE);	/* Do TSE on this packet */

	/* IP and/or TCP header checksum calculation and insertion. */
	*txd_upper = (E1000_TXD_POPTS_IXSM | E1000_TXD_POPTS_TXSM) << 8;

	cur = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[cur];
	TXD = (struct e1000_context_desc *) &txr->tx_base[cur];

	/*
	 * Start offset for header checksum calculation.
	 * End offset for header checksum calculation.
	 * Offset of place put the checksum.
	 */
	TXD->lower_setup.ip_fields.ipcss = ip_off;
	TXD->lower_setup.ip_fields.ipcse =
	    htole16(ip_off + (ip->ip_hl << 2) - 1);
	TXD->lower_setup.ip_fields.ipcso = ip_off + offsetof(struct ip, ip_sum);
	/*
	 * Start offset for payload checksum calculation.
	 * End offset for payload checksum calculation.
	 * Offset of place to put the checksum.
	 */
	TXD->upper_setup.tcp_fields.tucss = ip_off + (ip->ip_hl << 2);
	TXD->upper_setup.tcp_fields.tucse = 0;
	TXD->upper_setup.tcp_fields.tucso =
	    ip_off + (ip->ip_hl << 2) + offsetof(struct tcphdr, th_sum);
	/*
	 * Payload size per packet w/o any headers.
	 * Length of all headers up to payload.
	 */
	TXD->tcp_seg_setup.fields.mss = htole16(mp->m_pkthdr.tso_segsz);
	TXD->tcp_seg_setup.fields.hdr_len = hdr_len;

	TXD->cmd_and_length = htole32(adapter->txd_cmd |
				E1000_TXD_CMD_DEXT |	/* Extended descr */
				E1000_TXD_CMD_TSE |	/* TSE context */
				E1000_TXD_CMD_IP |	/* Do IP csum */
				E1000_TXD_CMD_TCP |	/* Do TCP checksum */
				(mp->m_pkthdr.len - (hdr_len))); /* Total len */

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++cur == adapter->num_tx_desc)
		cur = 0;

	txr->tx_avail--;
	txr->next_avail_desc = cur;
	txr->tx_tso = TRUE;
}


/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static bool
em_txeof(struct tx_ring *txr)
{
	struct adapter	*adapter = txr->adapter;
        int first, last, done, processed;
        struct em_buffer *tx_buffer;
        struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = adapter->ifp;

	EM_TX_LOCK_ASSERT(txr);

	/* No work, make sure watchdog is off */
        if (txr->tx_avail == adapter->num_tx_desc) {
		txr->queue_status = EM_QUEUE_IDLE;
                return (FALSE);
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
            BUS_DMASYNC_POSTREAD);

        while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
                	tx_desc->upper.data = 0;
                	tx_desc->lower.data = 0;
                	tx_desc->buffer_addr = 0;
                	++txr->tx_avail;
			++processed;

			if (tx_buffer->m_head) {
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
	** for too long indicates a hang. local timer
	** will examine this and do a reset if needed.
	*/
	if ((!processed) && ((ticks - txr->watchdog_time) > EM_WATCHDOG))
		txr->queue_status = EM_QUEUE_HUNG;

        /*
         * If we have enough room, clear IFF_DRV_OACTIVE
         * to tell the stack that it is OK to send packets.
         */
        if (txr->tx_avail > EM_TX_CLEANUP_THRESHOLD) {                
                ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		/* Disable watchdog if all clean */
                if (txr->tx_avail == adapter->num_tx_desc) {
			txr->queue_status = EM_QUEUE_IDLE;
			return (FALSE);
		} 
        }

	return (TRUE);
}


/*********************************************************************
 *
 *  Refresh RX descriptor mbufs from system mbuf buffer pool.
 *
 **********************************************************************/
static void
em_refresh_mbufs(struct rx_ring *rxr, int limit)
{
	struct adapter		*adapter = rxr->adapter;
	struct mbuf		*m;
	bus_dma_segment_t	segs[1];
	struct em_buffer	*rxbuf;
	int			i, error, nsegs, cleaned;

	i = rxr->next_to_refresh;
	cleaned = -1;
	while (i != limit) {
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head == NULL) {
			m = m_getjcl(M_DONTWAIT, MT_DATA,
			    M_PKTHDR, adapter->rx_mbuf_sz);
			/*
			** If we have a temporary resource shortage
			** that causes a failure, just abort refresh
			** for now, we will return to this point when
			** reinvoked from em_rxeof.
			*/
			if (m == NULL)
				goto update;
		} else
			m = rxbuf->m_head;

		m->m_len = m->m_pkthdr.len = adapter->rx_mbuf_sz;
		m->m_flags |= M_PKTHDR;
		m->m_data = m->m_ext.ext_buf;

		/* Use bus_dma machinery to setup the memory mapping  */
		error = bus_dmamap_load_mbuf_sg(rxr->rxtag, rxbuf->map,
		    m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("Refresh mbufs: hdr dmamap load"
			    " failure - %d\n", error);
			m_free(m);
			rxbuf->m_head = NULL;
			goto update;
		}
		rxbuf->m_head = m;
		bus_dmamap_sync(rxr->rxtag,
		    rxbuf->map, BUS_DMASYNC_PREREAD);
		rxr->rx_base[i].buffer_addr = htole64(segs[0].ds_addr);

		cleaned = i;
		/* Calculate next index */
		if (++i == adapter->num_rx_desc)
			i = 0;
		rxr->next_to_refresh = i;
	}
update:
	/*
	** Update the tail pointer only if,
	** and as far as we have refreshed.
	*/
	if (cleaned != -1) /* Update tail index */
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
em_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	device_t		dev = adapter->dev;
	struct em_buffer	*rxbuf;
	int			error;

	rxr->rx_buffers = malloc(sizeof(struct em_buffer) *
	    adapter->num_rx_desc, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (rxr->rx_buffers == NULL) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		return (ENOMEM);
	}

	error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				MJUM9BYTES,		/* maxsize */
				1,			/* nsegments */
				MJUM9BYTES,		/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&rxr->rxtag);
	if (error) {
		device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
		    __func__, error);
		goto fail;
	}

	rxbuf = rxr->rx_buffers;
	for (int i = 0; i < adapter->num_rx_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->rxtag, BUS_DMA_NOWAIT,
		    &rxbuf->map);
		if (error) {
			device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
			    __func__, error);
			goto fail;
		}
	}

	return (0);

fail:
	em_free_receive_structures(adapter);
	return (error);
}


/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
static int
em_setup_receive_ring(struct rx_ring *rxr)
{
	struct	adapter 	*adapter = rxr->adapter;
	struct em_buffer	*rxbuf;
	bus_dma_segment_t	seg[1];
	int			rsize, nsegs, error;


	/* Clear the ring contents */
	EM_RX_LOCK(rxr);
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(struct e1000_rx_desc), EM_DBA_ALIGN);
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
			m_freem(rxbuf->m_head);
		}
	}

	/* Now replenish the mbufs */
	for (int j = 0; j != adapter->num_rx_desc; ++j) {

		rxbuf = &rxr->rx_buffers[j];
		rxbuf->m_head = m_getjcl(M_DONTWAIT, MT_DATA,
		    M_PKTHDR, adapter->rx_mbuf_sz);
		if (rxbuf->m_head == NULL)
			return (ENOBUFS);
		rxbuf->m_head->m_len = adapter->rx_mbuf_sz;
		rxbuf->m_head->m_flags &= ~M_HASFCS; /* we strip it */
		rxbuf->m_head->m_pkthdr.len = adapter->rx_mbuf_sz;

		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->rxtag,
		    rxbuf->map, rxbuf->m_head, seg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_freem(rxbuf->m_head);
			rxbuf->m_head = NULL;
			return (error);
		}
		bus_dmamap_sync(rxr->rxtag,
		    rxbuf->map, BUS_DMASYNC_PREREAD);

		/* Update descriptor */
		rxr->rx_base[j].buffer_addr = htole64(seg[0].ds_addr);
	}


	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->next_to_refresh = 0;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	EM_RX_UNLOCK(rxr);
	return (0);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
static int
em_setup_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;
	int j;

	for (j = 0; j < adapter->num_queues; j++, rxr++)
		if (em_setup_receive_ring(rxr))
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
		for (int n = 0; n < adapter->num_rx_desc; n++) {
			struct em_buffer *rxbuf;
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
 *  Free all receive rings.
 *
 **********************************************************************/
static void
em_free_receive_structures(struct adapter *adapter)
{
	struct rx_ring *rxr = adapter->rx_rings;

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		em_free_receive_buffers(rxr);
		/* Free the ring memory as well */
		em_dma_free(adapter, &rxr->rxdma);
		EM_RX_LOCK_DESTROY(rxr);
	}

	free(adapter->rx_rings, M_DEVBUF);
}


/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
static void
em_free_receive_buffers(struct rx_ring *rxr)
{
	struct adapter		*adapter = rxr->adapter;
	struct em_buffer	*rxbuf = NULL;

	INIT_DEBUGOUT("free_receive_buffers: begin");

	if (rxr->rx_buffers != NULL) {
		for (int i = 0; i < adapter->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->map != NULL) {
				bus_dmamap_sync(rxr->rxtag, rxbuf->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxtag, rxbuf->map);
				bus_dmamap_destroy(rxr->rxtag, rxbuf->map);
			}
			if (rxbuf->m_head != NULL) {
				m_freem(rxbuf->m_head);
				rxbuf->m_head = NULL;
			}
		}
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
 *  Enable receive unit.
 *
 **********************************************************************/
#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR	     1000000000/(MAX_INTS_PER_SEC * 256)

static void
em_initialize_receive_unit(struct adapter *adapter)
{
	struct rx_ring	*rxr = adapter->rx_rings;
	struct ifnet	*ifp = adapter->ifp;
	struct e1000_hw	*hw = &adapter->hw;
	u64	bus_addr;
	u32	rctl, rxcsum;

	INIT_DEBUGOUT("em_initialize_receive_units: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	rctl = E1000_READ_REG(hw, E1000_RCTL);
	E1000_WRITE_REG(hw, E1000_RCTL, rctl & ~E1000_RCTL_EN);

	E1000_WRITE_REG(&adapter->hw, E1000_RADV,
	    adapter->rx_abs_int_delay.value);
	/*
	 * Set the interrupt throttling rate. Value is calculated
	 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns)
	 */
	E1000_WRITE_REG(hw, E1000_ITR, DEFAULT_ITR);

	/*
	** When using MSIX interrupts we need to throttle
	** using the EITR register (82574 only)
	*/
	if (hw->mac.type == e1000_82574)
		for (int i = 0; i < 4; i++)
			E1000_WRITE_REG(hw, E1000_EITR_82574(i),
			    DEFAULT_ITR);

	/* Disable accelerated ackknowledge */
	if (adapter->hw.mac.type == e1000_82574)
		E1000_WRITE_REG(hw, E1000_RFCTL, E1000_RFCTL_ACK_DIS);

	if (ifp->if_capenable & IFCAP_RXCSUM) {
		rxcsum = E1000_READ_REG(hw, E1000_RXCSUM);
		rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(hw, E1000_RXCSUM, rxcsum);
	}

	/*
	** XXX TEMPORARY WORKAROUND: on some systems with 82573
	** long latencies are observed, like Lenovo X60. This
	** change eliminates the problem, but since having positive
	** values in RDTR is a known source of problems on other
	** platforms another solution is being sought.
	*/
	if (hw->mac.type == e1000_82573)
		E1000_WRITE_REG(hw, E1000_RDTR, 0x20);

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		/* Setup the Base and Length of the Rx Descriptor Ring */
		bus_addr = rxr->rxdma.dma_paddr;
		E1000_WRITE_REG(hw, E1000_RDLEN(i),
		    adapter->num_rx_desc * sizeof(struct e1000_rx_desc));
		E1000_WRITE_REG(hw, E1000_RDBAH(i), (u32)(bus_addr >> 32));
		E1000_WRITE_REG(hw, E1000_RDBAL(i), (u32)bus_addr);
		/* Setup the Head and Tail Descriptor Pointers */
		E1000_WRITE_REG(hw, E1000_RDH(i), 0);
		E1000_WRITE_REG(hw, E1000_RDT(i), adapter->num_rx_desc - 1);
	}

	/* Set early receive threshold on appropriate hw */
	if (((adapter->hw.mac.type == e1000_ich9lan) ||
	    (adapter->hw.mac.type == e1000_pch2lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan)) &&
	    (ifp->if_mtu > ETHERMTU)) {
		u32 rxdctl = E1000_READ_REG(hw, E1000_RXDCTL(0));
		E1000_WRITE_REG(hw, E1000_RXDCTL(0), rxdctl | 3);
		E1000_WRITE_REG(hw, E1000_ERT, 0x100 | (1 << 13));
	}
		
	if (adapter->hw.mac.type == e1000_pch2lan) {
		if (ifp->if_mtu > ETHERMTU)
			e1000_lv_jumbo_workaround_ich8lan(hw, TRUE);
		else
			e1000_lv_jumbo_workaround_ich8lan(hw, FALSE);
	}

	/* Setup the Receive Control Register */
	rctl &= ~(3 << E1000_RCTL_MO_SHIFT);
	rctl |= E1000_RCTL_EN | E1000_RCTL_BAM |
	    E1000_RCTL_LBM_NO | E1000_RCTL_RDMTS_HALF |
	    (hw->mac.mc_filter_type << E1000_RCTL_MO_SHIFT);

        /* Strip the CRC */
        rctl |= E1000_RCTL_SECRC;

        /* Make sure VLAN Filters are off */
        rctl &= ~E1000_RCTL_VFE;
	rctl &= ~E1000_RCTL_SBP;

	if (adapter->rx_mbuf_sz == MCLBYTES)
		rctl |= E1000_RCTL_SZ_2048;
	else if (adapter->rx_mbuf_sz == MJUMPAGESIZE)
		rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX;
	else if (adapter->rx_mbuf_sz > MJUMPAGESIZE)
		rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX;

	if (ifp->if_mtu > ETHERMTU)
		rctl |= E1000_RCTL_LPE;
	else
		rctl &= ~E1000_RCTL_LPE;

	/* Write out the settings */
	E1000_WRITE_REG(hw, E1000_RCTL, rctl);

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
 *  For polling we also now return the number of cleaned packets
 *********************************************************************/
static bool
em_rxeof(struct rx_ring *rxr, int count, int *done)
{
	struct adapter		*adapter = rxr->adapter;
	struct ifnet		*ifp = adapter->ifp;
	struct mbuf		*mp, *sendmp;
	u8			status = 0;
	u16 			len;
	int			i, processed, rxdone = 0;
	bool			eop;
	struct e1000_rx_desc	*cur;

	EM_RX_LOCK(rxr);

	for (i = rxr->next_to_check, processed = 0; count != 0;) {

		if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
			break;

		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur = &rxr->rx_base[i];
		status = cur->status;
		mp = sendmp = NULL;

		if ((status & E1000_RXD_STAT_DD) == 0)
			break;

		len = le16toh(cur->length);
		eop = (status & E1000_RXD_STAT_EOP) != 0;

		if ((cur->errors & E1000_RXD_ERR_FRAME_ERR_MASK) ||
		    (rxr->discard == TRUE)) {
			ifp->if_ierrors++;
			++rxr->rx_discarded;
			if (!eop) /* Catch subsequent segs */
				rxr->discard = TRUE;
			else
				rxr->discard = FALSE;
			em_rx_discard(rxr, i);
			goto next_desc;
		}

		/* Assign correct length to the current fragment */
		mp = rxr->rx_buffers[i].m_head;
		mp->m_len = len;

		/* Trigger for refresh */
		rxr->rx_buffers[i].m_head = NULL;

		/* First segment? */
		if (rxr->fmp == NULL) {
			mp->m_pkthdr.len = len;
			rxr->fmp = rxr->lmp = mp;
		} else {
			/* Chain mbuf's together */
			mp->m_flags &= ~M_PKTHDR;
			rxr->lmp->m_next = mp;
			rxr->lmp = mp;
			rxr->fmp->m_pkthdr.len += len;
		}

		if (eop) {
			--count;
			sendmp = rxr->fmp;
			sendmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			em_receive_checksum(cur, sendmp);
#ifndef __NO_STRICT_ALIGNMENT
			if (adapter->max_frame_size >
			    (MCLBYTES - ETHER_ALIGN) &&
			    em_fixup_rx(rxr) != 0)
				goto skip;
#endif
			if (status & E1000_RXD_STAT_VP) {
				sendmp->m_pkthdr.ether_vtag =
				    (le16toh(cur->special) &
				    E1000_RXD_SPC_VLAN_MASK);
				sendmp->m_flags |= M_VLANTAG;
			}
#ifdef EM_MULTIQUEUE
			sendmp->m_pkthdr.flowid = rxr->msix;
			sendmp->m_flags |= M_FLOWID;
#endif
#ifndef __NO_STRICT_ALIGNMENT
skip:
#endif
			rxr->fmp = rxr->lmp = NULL;
		}
next_desc:
		/* Zero out the receive descriptors status. */
		cur->status = 0;
		++rxdone;	/* cumulative for POLL */
		++processed;

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;

		/* Send to the stack */
		if (sendmp != NULL) {
			rxr->next_to_check = i;
			EM_RX_UNLOCK(rxr);
			(*ifp->if_input)(ifp, sendmp);
			EM_RX_LOCK(rxr);
			i = rxr->next_to_check;
		}

		/* Only refresh mbufs every 8 descriptors */
		if (processed == 8) {
			em_refresh_mbufs(rxr, i);
			processed = 0;
		}
	}

	/* Catch any remaining refresh work */
	em_refresh_mbufs(rxr, i);

	rxr->next_to_check = i;
	if (done != NULL)
		*done = rxdone;
	EM_RX_UNLOCK(rxr);

	return ((status & E1000_RXD_STAT_DD) ? TRUE : FALSE);
}

static __inline void
em_rx_discard(struct rx_ring *rxr, int i)
{
	struct em_buffer	*rbuf;

	rbuf = &rxr->rx_buffers[i];
	/* Free any previous pieces */
	if (rxr->fmp != NULL) {
		rxr->fmp->m_flags |= M_PKTHDR;
		m_freem(rxr->fmp);
		rxr->fmp = NULL;
		rxr->lmp = NULL;
	}
	/*
	** Free buffer and allow em_refresh_mbufs()
	** to clean up and recharge buffer.
	*/
	if (rbuf->m_head) {
		m_free(rbuf->m_head);
		rbuf->m_head = NULL;
	}
	return;
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
em_fixup_rx(struct rx_ring *rxr)
{
	struct adapter *adapter = rxr->adapter;
	struct mbuf *m, *n;
	int error;

	error = 0;
	m = rxr->fmp;
	if (m->m_len <= (MCLBYTES - ETHER_HDR_LEN)) {
		bcopy(m->m_data, m->m_data + ETHER_HDR_LEN, m->m_len);
		m->m_data += ETHER_HDR_LEN;
	} else {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n != NULL) {
			bcopy(m->m_data, n->m_data, ETHER_HDR_LEN);
			m->m_data += ETHER_HDR_LEN;
			m->m_len -= ETHER_HDR_LEN;
			n->m_len = ETHER_HDR_LEN;
			M_MOVE_PKTHDR(n, m);
			n->m_next = m;
			rxr->fmp = n;
		} else {
			adapter->dropped_pkts++;
			m_freem(rxr->fmp);
			rxr->fmp = NULL;
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
em_receive_checksum(struct e1000_rx_desc *rx_desc, struct mbuf *mp)
{
	/* Ignore Checksum bit is set */
	if (rx_desc->status & E1000_RXD_STAT_IXSM) {
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
em_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
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
		em_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
em_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
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
		em_init_locked(adapter);
	EM_CORE_UNLOCK(adapter);
}

static void
em_setup_vlan_hw_support(struct adapter *adapter)
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
em_enable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ims_mask = IMS_ENABLE_MASK;

	if (hw->mac.type == e1000_82574) {
		E1000_WRITE_REG(hw, EM_EIAC, EM_MSIX_MASK);
		ims_mask |= EM_MSIX_MASK;
	} 
	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
}

static void
em_disable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (hw->mac.type == e1000_82574)
		E1000_WRITE_REG(hw, EM_EIAC, 0);
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);
}

/*
 * Bit of a misnomer, what this really means is
 * to enable OS management of the system... aka
 * to disable special hardware management features 
 */
static void
em_init_manageability(struct adapter *adapter)
{
	/* A shared code workaround */
#define E1000_82542_MANC2H E1000_MANC2H
	if (adapter->has_manage) {
		int manc2h = E1000_READ_REG(&adapter->hw, E1000_MANC2H);
		int manc = E1000_READ_REG(&adapter->hw, E1000_MANC);

		/* disable hardware interception of ARP */
		manc &= ~(E1000_MANC_ARP_EN);

                /* enable receiving management packets to the host */
		manc |= E1000_MANC_EN_MNG2HOST;
#define E1000_MNG2HOST_PORT_623 (1 << 5)
#define E1000_MNG2HOST_PORT_664 (1 << 6)
		manc2h |= E1000_MNG2HOST_PORT_623;
		manc2h |= E1000_MNG2HOST_PORT_664;
		E1000_WRITE_REG(&adapter->hw, E1000_MANC2H, manc2h);
		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * Give control back to hardware management
 * controller if there is one.
 */
static void
em_release_manageability(struct adapter *adapter)
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
 * em_get_hw_control sets the {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded. For AMT version type f/w
 * this means that the network i/f is open.
 */
static void
em_get_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext, swsm;

	if (adapter->hw.mac.type == e1000_82573) {
		swsm = E1000_READ_REG(&adapter->hw, E1000_SWSM);
		E1000_WRITE_REG(&adapter->hw, E1000_SWSM,
		    swsm | E1000_SWSM_DRV_LOAD);
		return;
	}
	/* else */
	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
	return;
}

/*
 * em_release_hw_control resets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is no longer loaded. For AMT versions of the
 * f/w this means that the network i/f is closed.
 */
static void
em_release_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext, swsm;

	if (!adapter->has_manage)
		return;

	if (adapter->hw.mac.type == e1000_82573) {
		swsm = E1000_READ_REG(&adapter->hw, E1000_SWSM);
		E1000_WRITE_REG(&adapter->hw, E1000_SWSM,
		    swsm & ~E1000_SWSM_DRV_LOAD);
		return;
	}
	/* else */
	ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
	    ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
	return;
}

static int
em_is_valid_ether_addr(u8 *addr)
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
em_get_wakeup(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	u16		eeprom_data = 0, device_id, apme_mask;

	adapter->has_manage = e1000_enable_mng_pass_thru(&adapter->hw);
	apme_mask = EM_EEPROM_APME;

	switch (adapter->hw.mac.type) {
	case e1000_82573:
	case e1000_82583:
		adapter->has_amt = TRUE;
		/* Falls thru */
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
		if (adapter->hw.bus.func == 1) {
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);
			break;
		} else
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		break;
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
	case e1000_pchlan:
	case e1000_pch2lan:
		apme_mask = E1000_WUC_APME;
		adapter->has_amt = TRUE;
		eeprom_data = E1000_READ_REG(&adapter->hw, E1000_WUC);
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
	case E1000_DEV_ID_82571EB_FIBER:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (E1000_READ_REG(&adapter->hw, E1000_STATUS) &
		    E1000_STATUS_FUNC_1)
			adapter->wol = 0;
		break;
	case E1000_DEV_ID_82571EB_QUAD_COPPER:
	case E1000_DEV_ID_82571EB_QUAD_FIBER:
	case E1000_DEV_ID_82571EB_QUAD_COPPER_LP:
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
em_enable_wakeup(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	struct ifnet	*ifp = adapter->ifp;
	u32		pmc, ctrl, ctrl_ext, rctl;
	u16     	status;

	if ((pci_find_extcap(dev, PCIY_PMG, &pmc) != 0))
		return;

	/* Advertise the wakeup capability */
	ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
	ctrl |= (E1000_CTRL_SWDPIN2 | E1000_CTRL_SWDPIN3);
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
	E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);

	if ((adapter->hw.mac.type == e1000_ich8lan) ||
	    (adapter->hw.mac.type == e1000_pchlan) ||
	    (adapter->hw.mac.type == e1000_ich9lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan)) {
		e1000_disable_gig_wol_ich8lan(&adapter->hw);
		e1000_hv_phy_powerdown_workaround_ich8lan(&adapter->hw);
	}

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

	if ((adapter->hw.mac.type == e1000_pchlan) ||
	    (adapter->hw.mac.type == e1000_pch2lan)) {
		if (em_enable_phy_wakeup(adapter))
			return;
	} else {
		E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
		E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
	}

	if (adapter->hw.phy.type == e1000_phy_igp_3)
		e1000_igp3_phy_powerdown_workaround_ich8lan(&adapter->hw);

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
em_enable_phy_wakeup(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 mreg, ret = 0;
	u16 preg;

	/* copy MAC RARs to PHY RARs */
	e1000_copy_rx_addrs_to_phy_ich8lan(hw);

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
em_led_func(void *arg, int onoff)
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

/*
** Disable the L0S and L1 LINK states
*/
static void
em_disable_aspm(struct adapter *adapter)
{
	int		base, reg;
	u16		link_cap,link_ctrl;
	device_t	dev = adapter->dev;

	switch (adapter->hw.mac.type) {
		case e1000_82573:
		case e1000_82574:
		case e1000_82583:
			break;
		default:
			return;
	}
	if (pci_find_extcap(dev, PCIY_EXPRESS, &base) != 0)
		return;
	reg = base + PCIR_EXPRESS_LINK_CAP;
	link_cap = pci_read_config(dev, reg, 2);
	if ((link_cap & PCIM_LINK_CAP_ASPM) == 0)
		return;
	reg = base + PCIR_EXPRESS_LINK_CTL;
	link_ctrl = pci_read_config(dev, reg, 2);
	link_ctrl &= 0xFFFC; /* turn off bit 1 and 2 */
	pci_write_config(dev, reg, link_ctrl, 2);
	return;
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
em_update_stats_counters(struct adapter *adapter)
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
	/*
	** For watchdog management we need to know if we have been
	** paused during the last interval, so capture that here.
	*/
	adapter->pause_frames = E1000_READ_REG(&adapter->hw, E1000_XOFFRXC);
	adapter->stats.xoffrxc += adapter->pause_frames;
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

	/* Interrupt Counts */

	adapter->stats.iac += E1000_READ_REG(&adapter->hw, E1000_IAC);
	adapter->stats.icrxptc += E1000_READ_REG(&adapter->hw, E1000_ICRXPTC);
	adapter->stats.icrxatc += E1000_READ_REG(&adapter->hw, E1000_ICRXATC);
	adapter->stats.ictxptc += E1000_READ_REG(&adapter->hw, E1000_ICTXPTC);
	adapter->stats.ictxatc += E1000_READ_REG(&adapter->hw, E1000_ICTXATC);
	adapter->stats.ictxqec += E1000_READ_REG(&adapter->hw, E1000_ICTXQEC);
	adapter->stats.ictxqmtc += E1000_READ_REG(&adapter->hw, E1000_ICTXQMTC);
	adapter->stats.icrxdmtc += E1000_READ_REG(&adapter->hw, E1000_ICRXDMTC);
	adapter->stats.icrxoc += E1000_READ_REG(&adapter->hw, E1000_ICRXOC);

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
em_sysctl_reg_handler(SYSCTL_HANDLER_ARGS)
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
em_add_hw_stats(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct e1000_hw_stats *stats = &adapter->stats;

	struct sysctl_oid *stat_node, *queue_node, *int_node;
	struct sysctl_oid_list *stat_list, *queue_list, *int_list;

#define QUEUE_NAME_LEN 32
	char namebuf[QUEUE_NAME_LEN];
	
	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "link_irq",
			CTLFLAG_RD, &adapter->link_irq,
			"Link MSIX IRQ Handled");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_alloc_fail", 
			 CTLFLAG_RD, &adapter->mbuf_alloc_failed,
			 "Std mbuf failed");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "cluster_alloc_fail", 
			 CTLFLAG_RD, &adapter->mbuf_cluster_failed,
			 "Std mbuf cluster failed");
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
	
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "device_control",
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_CTRL,
			em_sysctl_reg_handler, "IU",
			"Device Control Register");
	SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "rx_control",
			CTLTYPE_UINT | CTLFLAG_RD, adapter, E1000_RCTL,
			em_sysctl_reg_handler, "IU",
			"Receiver Control Register");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_high_water",
			CTLFLAG_RD, &adapter->hw.fc.high_water, 0,
			"Flow Control High Watermark");
	SYSCTL_ADD_UINT(ctx, child, OID_AUTO, "fc_low_water", 
			CTLFLAG_RD, &adapter->hw.fc.low_water, 0,
			"Flow Control Low Watermark");

	for (int i = 0; i < adapter->num_queues; i++, rxr++, txr++) {
		snprintf(namebuf, QUEUE_NAME_LEN, "queue%d", i);
		queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, namebuf,
					    CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_head", 
				CTLTYPE_UINT | CTLFLAG_RD, adapter,
				E1000_TDH(txr->me),
				em_sysctl_reg_handler, "IU",
 				"Transmit Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "txd_tail", 
				CTLTYPE_UINT | CTLFLAG_RD, adapter,
				E1000_TDT(txr->me),
				em_sysctl_reg_handler, "IU",
 				"Transmit Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "tx_irq",
				CTLFLAG_RD, &txr->tx_irq,
				"Queue MSI-X Transmit Interrupts");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "no_desc_avail", 
				CTLFLAG_RD, &txr->no_desc_avail,
				"Queue No Descriptor Available");
		
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_head", 
				CTLTYPE_UINT | CTLFLAG_RD, adapter,
				E1000_RDH(rxr->me),
				em_sysctl_reg_handler, "IU",
				"Receive Descriptor Head");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "rxd_tail", 
				CTLTYPE_UINT | CTLFLAG_RD, adapter,
				E1000_RDT(rxr->me),
				em_sysctl_reg_handler, "IU",
				"Receive Descriptor Tail");
		SYSCTL_ADD_ULONG(ctx, queue_list, OID_AUTO, "rx_irq",
				CTLFLAG_RD, &rxr->rx_irq,
				"Queue MSI-X Receive Interrupts");
	}

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
	/* On 82575 these are collision counts */
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


	/* Interrupt Stats */

	int_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "interrupts", 
				    CTLFLAG_RD, NULL, "Interrupt Statistics");
	int_list = SYSCTL_CHILDREN(int_node);

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "asserts",
			CTLFLAG_RD, &adapter->stats.iac,
			"Interrupt Assertion Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_pkt_timer",
			CTLFLAG_RD, &adapter->stats.icrxptc,
			"Interrupt Cause Rx Pkt Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_abs_timer",
			CTLFLAG_RD, &adapter->stats.icrxatc,
			"Interrupt Cause Rx Abs Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_pkt_timer",
			CTLFLAG_RD, &adapter->stats.ictxptc,
			"Interrupt Cause Tx Pkt Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_abs_timer",
			CTLFLAG_RD, &adapter->stats.ictxatc,
			"Interrupt Cause Tx Abs Timer Expire Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_queue_empty",
			CTLFLAG_RD, &adapter->stats.ictxqec,
			"Interrupt Cause Tx Queue Empty Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "tx_queue_min_thresh",
			CTLFLAG_RD, &adapter->stats.ictxqmtc,
			"Interrupt Cause Tx Queue Min Thresh Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_desc_min_thresh",
			CTLFLAG_RD, &adapter->stats.icrxdmtc,
			"Interrupt Cause Rx Desc Min Thresh Count");

	SYSCTL_ADD_UQUAD(ctx, int_list, OID_AUTO, "rx_overrun",
			CTLFLAG_RD, &adapter->stats.icrxoc,
			"Interrupt Cause Receiver Overrun Count");
}

/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/
static int
em_sysctl_nvm_info(SYSCTL_HANDLER_ARGS)
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
		em_print_nvm_info(adapter);
        }

	return (error);
}

static void
em_print_nvm_info(struct adapter *adapter)
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
em_sysctl_int_delay(SYSCTL_HANDLER_ARGS)
{
	struct em_int_delay_info *info;
	struct adapter *adapter;
	u32 regval;
	int error, usecs, ticks;

	info = (struct em_int_delay_info *)arg1;
	usecs = info->value;
	error = sysctl_handle_int(oidp, &usecs, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (usecs < 0 || usecs > EM_TICKS_TO_USECS(65535))
		return (EINVAL);
	info->value = usecs;
	ticks = EM_USECS_TO_TICKS(usecs);

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
em_add_int_delay_sysctl(struct adapter *adapter, const char *name,
	const char *description, struct em_int_delay_info *info,
	int offset, int value)
{
	info->adapter = adapter;
	info->offset = offset;
	info->value = value;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW,
	    info, 0, em_sysctl_int_delay, "I", description);
}

static void
em_add_rx_process_limit(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}

static void
em_set_flow_cntrl(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}

static int
em_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct adapter *adapter;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		adapter = (struct adapter *)arg1;
		em_print_debug_info(adapter);
        }

	return (error);
}

/*
** This routine is meant to be fluid, add whatever is
** needed for debugging a problem.  -jfv
*/
static void
em_print_debug_info(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct tx_ring *txr = adapter->tx_rings;
	struct rx_ring *rxr = adapter->rx_rings;

	if (adapter->ifp->if_drv_flags & IFF_DRV_RUNNING)
		printf("Interface is RUNNING ");
	else
		printf("Interface is NOT RUNNING\n");
	if (adapter->ifp->if_drv_flags & IFF_DRV_OACTIVE)
		printf("and ACTIVE\n");
	else
		printf("and INACTIVE\n");

	device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_TDH(0)),
	    E1000_READ_REG(&adapter->hw, E1000_TDT(0)));
	device_printf(dev, "hw rdh = %d, hw rdt = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_RDH(0)),
	    E1000_READ_REG(&adapter->hw, E1000_RDT(0)));
	device_printf(dev, "Tx Queue Status = %d\n", txr->queue_status);
	device_printf(dev, "TX descriptors avail = %d\n",
	    txr->tx_avail);
	device_printf(dev, "Tx Descriptors avail failure = %ld\n",
	    txr->no_desc_avail);
	device_printf(dev, "RX discarded packets = %ld\n",
	    rxr->rx_discarded);
	device_printf(dev, "RX Next to Check = %d\n", rxr->next_to_check);
	device_printf(dev, "RX Next to Refresh = %d\n", rxr->next_to_refresh);
}
