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
char em_driver_version[] = "7.0.5";


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
	{ 0x8086, E1000_DEV_ID_PCH_M_HV_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_M_HV_LC,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_D_HV_DM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_PCH_D_HV_DC,	PCI_ANY_ID, PCI_ANY_ID, 0},
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
static void	em_setup_interface(device_t, struct adapter *);

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
static bool	em_txeof(struct tx_ring *);
static int	em_rxeof(struct rx_ring *, int);
#ifndef __NO_STRICT_ALIGNMENT
static int	em_fixup_rx(struct rx_ring *);
#endif
static void	em_receive_checksum(struct e1000_rx_desc *, struct mbuf *);
static void	em_transmit_checksum_setup(struct tx_ring *, struct mbuf *,
		    u32 *, u32 *);
static bool	em_tso_setup(struct tx_ring *, struct mbuf *, u32 *, u32 *);
static void	em_set_promisc(struct adapter *);
static void	em_disable_promisc(struct adapter *);
static void	em_set_multi(struct adapter *);
static void	em_print_hw_stats(struct adapter *);
static void	em_update_link_status(struct adapter *);
static void	em_refresh_mbufs(struct rx_ring *, int);
static void	em_register_vlan(void *, struct ifnet *, u16);
static void	em_unregister_vlan(void *, struct ifnet *, u16);
static void	em_setup_vlan_hw_support(struct adapter *);
static int	em_xmit(struct tx_ring *, struct mbuf **);
static int	em_dma_malloc(struct adapter *, bus_size_t,
		    struct em_dma_alloc *, int);
static void	em_dma_free(struct adapter *, struct em_dma_alloc *);
static void	em_print_debug_info(struct adapter *);
static void	em_print_nvm_info(struct adapter *);
static int 	em_is_valid_ether_addr(u8 *);
static int	em_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
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

/* Local controls for MSI/MSIX */
#ifdef EM_MULTIQUEUE
static int em_enable_msix = TRUE;
static int em_msix_queues = 2; /* for 82574, can be 1 or 2 */
#else
static int em_enable_msix = FALSE;
static int em_msix_queues = 0; /* disable */
#endif
TUNABLE_INT("hw.em.enable_msix", &em_enable_msix);
TUNABLE_INT("hw.em.msix_queues", &em_msix_queues);

/* How many packets rxeof tries to clean at a time */
static int em_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &em_rx_process_limit);

/* Flow control setting - default to FULL */
static int em_fc_setting = e1000_fc_full;
TUNABLE_INT("hw.em.fc_setting", &em_fc_setting);

/*
** Shadow VFTA table, this is needed because
** the real vlan filter table gets cleared during
** a soft reset and the driver needs to be able
** to repopulate it.
*/
static u32 em_shadow_vfta[EM_VFTA_SIZE];

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
	    OID_AUTO, "debug", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "stats", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_stats, "I", "Statistics");

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
	    (adapter->hw.mac.type == e1000_pchlan) ||
	    (adapter->hw.mac.type == e1000_ich9lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan)) {
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

	/* Sysctls for limiting the amount of work done in the taskqueue */
	em_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    em_rx_process_limit);

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
	em_setup_interface(dev, adapter);

	em_reset(adapter);

	/* Initialize statistics */
	em_update_stats_counters(adapter);

	adapter->hw.mac.get_link_status = 1;
	em_update_link_status(adapter);

	/* Indicate SOL/IDER usage */
	if (e1000_check_reset_block(&adapter->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    em_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    em_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST); 

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
err_pci:
	em_free_pci_resources(adapter);
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

	if (adapter->led_dev != NULL)
		led_destroy(adapter->led_dev);

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
                txr->watchdog_check = TRUE;
		txr->watchdog_time = ticks;
	}
	return (err);
}

/*
** Multiqueue capable stack interface, this is not
** yet truely multiqueue, but that is coming...
*/
static int
em_mq_start(struct ifnet *ifp, struct mbuf *m)
{
	struct adapter	*adapter = ifp->if_softc;
	struct tx_ring	*txr;
	int 		i, error = 0;

	/* Which queue to use */
	if ((m->m_flags & M_FLOWID) != 0)
                i = m->m_pkthdr.flowid % adapter->num_queues;
	else
		i = curcpu % adapter->num_queues;

	txr = &adapter->tx_rings[i];

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
		txr->watchdog_check = TRUE;
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
	case e1000_ich9lan:
	case e1000_ich10lan:
	case e1000_pchlan:
		pba = E1000_PBA_10K;
		break;
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
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

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		em_stop(adapter);
		return;
	}
	em_initialize_receive_unit(adapter);

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
			em_update_link_status(adapter);
			callout_reset(&adapter->timer, hz,
			    em_local_timer, adapter);
		}
	}
	EM_CORE_UNLOCK(adapter);

	rx_done = em_rxeof(rxr, count);

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
	bool		more_rx;


	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more_rx = em_rxeof(rxr, adapter->rx_process_limit);

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
		if (more_rx) {
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
	more = em_rxeof(rxr, adapter->rx_process_limit);
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
		taskqueue_enqueue(taskqueue_fast, &adapter->link_task);
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

	more = em_rxeof(rxr, adapter->rx_process_limit);
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

	if (!EM_TX_TRYLOCK(txr))
		return;

	em_txeof(txr);

#ifdef EM_MULTIQUEUE
	if (!drbr_empty(ifp, txr->br))
		em_mq_start_locked(ifp, txr, NULL);
#else
	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp, txr);
#endif
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

	/* As the speed/duplex settings my have changed we need to
	 * reset the PHY.
	 */
	adapter->hw.phy.reset_disable = FALSE;

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
	u32			txd_upper, txd_lower, txd_used, txd_saved;
	int			nsegs, i, j, first, last = 0;
	int			error, do_tso, tso_desc = 0;

	m_head = *m_headp;
	txd_upper = txd_lower = txd_used = txd_saved = 0;
	do_tso = ((m_head->m_pkthdr.csum_flags & CSUM_TSO) != 0);

	/*
	 * TSO workaround: 
	 *  If an mbuf is only header we need  
	 *     to pull 4 bytes of data into it. 
	 */
	if (do_tso && (m_head->m_len <= M_TSO_LEN)) {
		m_head = m_pullup(m_head, M_TSO_LEN + 4);
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
#if __FreeBSD_version >= 700000
	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		error = em_tso_setup(txr, m_head, &txd_upper, &txd_lower);
		if (error != TRUE)
			return (ENXIO); /* something foobar */
		/* we need to make a final sentinel transmit desc */
		tso_desc = TRUE;
	} else
#endif
	if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD)
		em_transmit_checksum_setup(txr,  m_head,
		    &txd_upper, &txd_lower);

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

	if (adapter->hw.mac.type == e1000_82542 && 
	    adapter->hw.revision_id == E1000_REVISION_2) {
		reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		if (adapter->hw.bus.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			e1000_pci_clear_mwi(&adapter->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
		msec_delay(5);
	}

	/* Allocate temporary memory to setup array */
	mta = malloc(sizeof(u8) *
	    (ETH_ADDR_LEN * MAX_NUM_MULTICAST_ADDRESSES),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (mta == NULL)
		panic("em_set_multi memory failure\n");

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
	free(mta, M_DEVBUF);
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
	if (e1000_get_laa_state_82571(&adapter->hw) == TRUE)
		e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	if (em_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING)
		em_print_hw_stats(adapter);

	/*
	** Check for time since any descriptor was cleaned
	*/
	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		EM_TX_LOCK(txr);
		if (txr->watchdog_check == FALSE) {
			EM_TX_UNLOCK(txr);
			continue;
		}
		if ((ticks - txr->watchdog_time) > EM_WATCHDOG)
			goto hung;
		EM_TX_UNLOCK(txr);
	}

	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	return;
hung:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	EM_TX_UNLOCK(txr);
	em_init_locked(adapter);
}


static void
em_update_link_status(struct adapter *adapter)
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
		// JFV change later
		//adapter->watchdog_check = FALSE;
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
		txr->watchdog_check = FALSE;
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
	adapter->linkvec = vector;
	adapter->ivars |=  (8 | vector) << 16;
	adapter->ivars |= 0x80000000;
	TASK_INIT(&adapter->link_task, 0, em_handle_link, adapter);
	adapter->tq = taskqueue_create_fast("em_link", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s linkq",
	    device_get_nameunit(adapter->dev));

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


	/* Setup MSI/X for Hartwell */
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
		if (val != 5) {
			bus_release_resource(dev, SYS_RES_MEMORY,
			    PCIR_BAR(EM_MSIX_BAR), adapter->msix_mem);
			adapter->msix_mem = NULL;
               		device_printf(adapter->dev,
			    "MSIX vectors wrong, using MSI \n");
			goto msi;
		}
		if (em_msix_queues == 2) {
			val = 5;
			adapter->num_queues = 2;
		} else {
			val = 3;
			adapter->num_queues = 1;
		}
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
               	device_printf(adapter->dev,"Using MSI interrupt\n");
		return (val);
	} 
	/* Should only happen due to manual invention */
	device_printf(adapter->dev,"Setup MSIX failure\n");
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
        if ((em_fc_setting >= 0) || (em_fc_setting < 4))
		hw->fc.requested_mode = em_fc_setting;
	else
		hw->fc.requested_mode = e1000_fc_none;

	/* Override - workaround for PCHLAN issue */
	if (hw->mac.type == e1000_pchlan)
                hw->fc.requested_mode = e1000_fc_rx_pause;

	/* Issue a global reset */
	e1000_reset_hw(hw);
	E1000_WRITE_REG(hw, E1000_WUC, 0);

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
static void
em_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		panic("%s: can not if_alloc()", device_get_nameunit(dev));
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
	buf_ring_free(txr->br, M_DEVBUF);
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

		txr->watchdog_check = FALSE;
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
em_transmit_checksum_setup(struct tx_ring *txr, struct mbuf *mp,
    u32 *txd_upper, u32 *txd_lower)
{
	struct adapter			*adapter = txr->adapter;
	struct e1000_context_desc	*TXD = NULL;
	struct em_buffer *tx_buffer;
	struct ether_vlan_header *eh;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6;
	int cur, ehdrlen;
	u32 cmd, hdr_len, ip_hlen;
	u16 etype;
	u8 ipproto;


	cmd = hdr_len = ipproto = 0;
	cur = txr->next_avail_desc;

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
			    &txr->tx_base[cur];
			TXD->lower_setup.ip_fields.ipcss = ehdrlen;
			TXD->lower_setup.ip_fields.ipcse =
			    htole16(ehdrlen + ip_hlen);
			TXD->lower_setup.ip_fields.ipcso =
			    ehdrlen + offsetof(struct ip, ip_sum);
			cmd |= E1000_TXD_CMD_IP;
			*txd_upper |= E1000_TXD_POPTS_IXSM << 8;
		}

		if (mp->m_len < ehdrlen + ip_hlen)
			return;	/* failure */

		hdr_len = ehdrlen + ip_hlen;
		ipproto = ip->ip_p;

		break;
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		ip_hlen = sizeof(struct ip6_hdr); /* XXX: No header stacking. */

		if (mp->m_len < ehdrlen + ip_hlen)
			return;	/* failure */

		/* IPv6 doesn't have a header checksum. */

		hdr_len = ehdrlen + ip_hlen;
		ipproto = ip6->ip6_nxt;

		break;
	default:
		*txd_upper = 0;
		*txd_lower = 0;
		return;
	}

	switch (ipproto) {
	case IPPROTO_TCP:
		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
			/* no need for context if already set */
			if (txr->last_hw_offload == CSUM_TCP)
				return;
			txr->last_hw_offload = CSUM_TCP;
			/*
			 * Start offset for payload checksum calculation.
			 * End offset for payload checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD = (struct e1000_context_desc *)
			    &txr->tx_base[cur];
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
			if (txr->last_hw_offload == CSUM_UDP)
				return;
			txr->last_hw_offload = CSUM_UDP;
			/*
			 * Start offset for header checksum calculation.
			 * End offset for header checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD = (struct e1000_context_desc *)
			    &txr->tx_base[cur];
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
static bool
em_tso_setup(struct tx_ring *txr, struct mbuf *mp, u32 *txd_upper,
   u32 *txd_lower)
{
	struct adapter			*adapter = txr->adapter;
	struct e1000_context_desc	*TXD;
	struct em_buffer		*tx_buffer;
	struct ether_vlan_header	*eh;
	struct ip			*ip;
	struct ip6_hdr			*ip6;
	struct tcphdr			*th;
	int cur, ehdrlen, hdr_len, ip_hlen, isip6;
	u16 etype;

	/*
	 * This function could/should be extended to support IP/IPv6
	 * fragmentation as well.  But as they say, one step at a time.
	 */

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

	/* Ensure we have at least the IP+TCP header in the first mbuf. */
	if (mp->m_len < ehdrlen + sizeof(struct ip) + sizeof(struct tcphdr))
		return FALSE;	/* -1 */

	/*
	 * We only support TCP for IPv4 and IPv6 (notyet) for the moment.
	 * TODO: Support SCTP too when it hits the tree.
	 */
	switch (etype) {
	case ETHERTYPE_IP:
		isip6 = 0;
		ip = (struct ip *)(mp->m_data + ehdrlen);
		if (ip->ip_p != IPPROTO_TCP)
			return FALSE;	/* 0 */
		ip->ip_len = 0;
		ip->ip_sum = 0;
		ip_hlen = ip->ip_hl << 2;
		if (mp->m_len < ehdrlen + ip_hlen + sizeof(struct tcphdr))
			return FALSE;	/* -1 */
		th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
#if 1
		th->th_sum = in_pseudo(ip->ip_src.s_addr,
		    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
#else
		th->th_sum = mp->m_pkthdr.csum_data;
#endif
		break;
	case ETHERTYPE_IPV6:
		isip6 = 1;
		return FALSE;			/* Not supported yet. */
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return FALSE;	/* 0 */
		ip6->ip6_plen = 0;
		ip_hlen = sizeof(struct ip6_hdr); /* XXX: no header stacking. */
		if (mp->m_len < ehdrlen + ip_hlen + sizeof(struct tcphdr))
			return FALSE;	/* -1 */
		th = (struct tcphdr *)((caddr_t)ip6 + ip_hlen);
#if 0
		th->th_sum = in6_pseudo(ip6->ip6_src, ip->ip6_dst,
		    htons(IPPROTO_TCP));	/* XXX: function notyet. */
#else
		th->th_sum = mp->m_pkthdr.csum_data;
#endif
		break;
	default:
		return FALSE;
	}
	hdr_len = ehdrlen + ip_hlen + (th->th_off << 2);

	*txd_lower = (E1000_TXD_CMD_DEXT |	/* Extended descr type */
		      E1000_TXD_DTYP_D |	/* Data descr type */
		      E1000_TXD_CMD_TSE);	/* Do TSE on this packet */

	/* IP and/or TCP header checksum calculation and insertion. */
	*txd_upper = ((isip6 ? 0 : E1000_TXD_POPTS_IXSM) |
		      E1000_TXD_POPTS_TXSM) << 8;

	cur = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[cur];
	TXD = (struct e1000_context_desc *) &txr->tx_base[cur];

	/* IPv6 doesn't have a header checksum. */
	if (!isip6) {
		/*
		 * Start offset for header checksum calculation.
		 * End offset for header checksum calculation.
		 * Offset of place put the checksum.
		 */
		TXD->lower_setup.ip_fields.ipcss = ehdrlen;
		TXD->lower_setup.ip_fields.ipcse =
		    htole16(ehdrlen + ip_hlen - 1);
		TXD->lower_setup.ip_fields.ipcso =
		    ehdrlen + offsetof(struct ip, ip_sum);
	}
	/*
	 * Start offset for payload checksum calculation.
	 * End offset for payload checksum calculation.
	 * Offset of place to put the checksum.
	 */
	TXD->upper_setup.tcp_fields.tucss =
	    ehdrlen + ip_hlen;
	TXD->upper_setup.tcp_fields.tucse = 0;
	TXD->upper_setup.tcp_fields.tucso =
	    ehdrlen + ip_hlen + offsetof(struct tcphdr, th_sum);
	/*
	 * Payload size per packet w/o any headers.
	 * Length of all headers up to payload.
	 */
	TXD->tcp_seg_setup.fields.mss = htole16(mp->m_pkthdr.tso_segsz);
	TXD->tcp_seg_setup.fields.hdr_len = hdr_len;

	TXD->cmd_and_length = htole32(adapter->txd_cmd |
				E1000_TXD_CMD_DEXT |	/* Extended descr */
				E1000_TXD_CMD_TSE |	/* TSE context */
				(isip6 ? 0 : E1000_TXD_CMD_IP) | 
				E1000_TXD_CMD_TCP |	/* Do TCP checksum */
				(mp->m_pkthdr.len - (hdr_len))); /* Total len */

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++cur == adapter->num_tx_desc)
		cur = 0;

	txr->tx_avail--;
	txr->next_avail_desc = cur;
	txr->tx_tso = TRUE;

	return TRUE;
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
        int first, last, done, num_avail;
        struct em_buffer *tx_buffer;
        struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = adapter->ifp;

	EM_TX_LOCK_ASSERT(txr);

        if (txr->tx_avail == adapter->num_tx_desc)
                return (FALSE);

        num_avail = txr->tx_avail;
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
                	++num_avail;

			if (tx_buffer->m_head) {
				ifp->if_opackets++;
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
         * If we have enough room, clear IFF_DRV_OACTIVE to
         * tell the stack that it is OK to send packets.
         * If there are no pending descriptors, clear the watchdog.
         */
        if (num_avail > EM_TX_CLEANUP_THRESHOLD) {                
                ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
                if (num_avail == adapter->num_tx_desc) {
			txr->watchdog_check = FALSE;
        		txr->tx_avail = num_avail;
			return (FALSE);
		} 
        }

        txr->tx_avail = num_avail;
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
	bus_dmamap_t		map;
	struct em_buffer	*rxbuf;
	int			i, error, nsegs, cleaned;

	i = rxr->next_to_refresh;
	cleaned = -1;
	while (i != limit) {
		m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (m == NULL)
			goto update;
		m->m_len = m->m_pkthdr.len = MCLBYTES;

		if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
			m_adj(m, ETHER_ALIGN);

		/*
		 * Using memory from the mbuf cluster pool, invoke the
		 * bus_dma machinery to arrange the memory mapping.
		 */
		error = bus_dmamap_load_mbuf_sg(rxr->rxtag, rxr->rx_sparemap,
		    m, segs, &nsegs, BUS_DMA_NOWAIT);
		if (error != 0) {
			m_free(m);
			goto update;
		}

		/* If nsegs is wrong then the stack is corrupt. */
		KASSERT(nsegs == 1, ("Too many segments returned!"));
	
		rxbuf = &rxr->rx_buffers[i];
		if (rxbuf->m_head != NULL)
			bus_dmamap_unload(rxr->rxtag, rxbuf->map);
	
		map = rxbuf->map;
		rxbuf->map = rxr->rx_sparemap;
		rxr->rx_sparemap = map;
		bus_dmamap_sync(rxr->rxtag,
		    rxbuf->map, BUS_DMASYNC_PREREAD);
		rxbuf->m_head = m;
		rxr->rx_base[i].buffer_addr = htole64(segs[0].ds_addr);

		cleaned = i;
		/* Calculate next index */
		if (++i == adapter->num_rx_desc)
			i = 0;
		/* This is the work marker for refresh */
		rxr->next_to_refresh = i;
	}
update:
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
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
				MCLBYTES,		/* maxsize */
				1,			/* nsegments */
				MCLBYTES,		/* maxsegsize */
				0,			/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&rxr->rxtag);
	if (error) {
		device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
		    __func__, error);
		goto fail;
	}

	/* Create the spare map (used by getbuf) */
	error = bus_dmamap_create(rxr->rxtag, BUS_DMA_NOWAIT,
	     &rxr->rx_sparemap);
	if (error) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
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
		rxbuf->m_head = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (rxbuf->m_head == NULL)
			panic("RX ring hdr initialization failed!\n");
		rxbuf->m_head->m_len = MCLBYTES;
		rxbuf->m_head->m_flags &= ~M_HASFCS; /* we strip it */
		rxbuf->m_head->m_pkthdr.len = MCLBYTES;

		/* Get the memory mapping */
		error = bus_dmamap_load_mbuf_sg(rxr->rxtag,
		    rxbuf->map, rxbuf->m_head, seg,
		    &nsegs, BUS_DMA_NOWAIT);
		if (error != 0)
			panic("RX ring dma initialization failed!\n");
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

	if (rxr->rx_sparemap) {
		bus_dmamap_destroy(rxr->rxtag, rxr->rx_sparemap);
		rxr->rx_sparemap = NULL;
	}

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
	rctl |= E1000_RCTL_SZ_2048;
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
static int
em_rxeof(struct rx_ring *rxr, int count)
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
		count--;

		if ((cur->errors & E1000_RXD_ERR_FRAME_ERR_MASK) == 0) {

			/* Assign correct length to the current fragment */
			mp = rxr->rx_buffers[i].m_head;
			mp->m_len = len;

			if (rxr->fmp == NULL) {
				mp->m_pkthdr.len = len;
				rxr->fmp = mp; /* Store the first mbuf */
				rxr->lmp = mp;
			} else {
				/* Chain mbuf's together */
				mp->m_flags &= ~M_PKTHDR;
				rxr->lmp->m_next = mp;
				rxr->lmp = rxr->lmp->m_next;
				rxr->fmp->m_pkthdr.len += len;
			}

			if (eop) {
				rxr->fmp->m_pkthdr.rcvif = ifp;
				ifp->if_ipackets++;
				em_receive_checksum(cur, rxr->fmp);
#ifndef __NO_STRICT_ALIGNMENT
				if (adapter->max_frame_size >
				    (MCLBYTES - ETHER_ALIGN) &&
				    em_fixup_rx(rxr) != 0)
					goto skip;
#endif
				if (status & E1000_RXD_STAT_VP) {
					rxr->fmp->m_pkthdr.ether_vtag =
					    (le16toh(cur->special) &
					    E1000_RXD_SPC_VLAN_MASK);
					rxr->fmp->m_flags |= M_VLANTAG;
				}
#ifdef EM_MULTIQUEUE
				rxr->fmp->m_pkthdr.flowid = curcpu;
				rxr->fmp->m_flags |= M_FLOWID;
#endif
#ifndef __NO_STRICT_ALIGNMENT
skip:
#endif
				sendmp = rxr->fmp;
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
		} else {
			ifp->if_ierrors++;
			/* Reuse loaded DMA map and just update mbuf chain */
			mp = rxr->rx_buffers[i].m_head;
			mp->m_len = mp->m_pkthdr.len = MCLBYTES;
			mp->m_data = mp->m_ext.ext_buf;
			mp->m_next = NULL;
			if (adapter->max_frame_size <=
			    (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
			if (rxr->fmp != NULL) {
				m_freem(rxr->fmp);
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
			sendmp = NULL;
		}

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
	if (processed != 0) {
		em_refresh_mbufs(rxr, i);
		processed = 0;
	}

	rxr->next_to_check = i;
	EM_RX_UNLOCK(rxr);

#ifdef DEVICE_POLLING
	return (rxdone);
#else
	return ((status & E1000_RXD_STAT_DD) ? TRUE : FALSE);
#endif
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

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	em_shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	/* Re-init to load the changes */
	em_init(adapter);
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

	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	em_shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	em_init(adapter);
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
                if (em_shadow_vfta[i] != 0)
			E1000_WRITE_REG_ARRAY(hw, E1000_VFTA,
                            i, em_shadow_vfta[i]);

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

	if (adapter->hw.mac.type == e1000_pchlan) {
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

	adapter->stats.gorc += E1000_READ_REG(&adapter->hw, E1000_GORCH);
	adapter->stats.gotc += E1000_READ_REG(&adapter->hw, E1000_GOTCH);

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


/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
em_print_debug_info(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	u8 *hw_addr = adapter->hw.hw_addr;
	struct rx_ring *rxr = adapter->rx_rings;
	struct tx_ring *txr = adapter->tx_rings;

	device_printf(dev, "Adapter hardware address = %p \n", hw_addr);
	device_printf(dev, "CTRL = 0x%x RCTL = 0x%x \n",
	    E1000_READ_REG(&adapter->hw, E1000_CTRL),
	    E1000_READ_REG(&adapter->hw, E1000_RCTL));
	device_printf(dev, "Packet buffer = Tx=%dk Rx=%dk \n",
	    ((E1000_READ_REG(&adapter->hw, E1000_PBA) & 0xffff0000) >> 16),\
	    (E1000_READ_REG(&adapter->hw, E1000_PBA) & 0xffff) );
	device_printf(dev, "Flow control watermarks high = %d low = %d\n",
	    adapter->hw.fc.high_water,
	    adapter->hw.fc.low_water);
	device_printf(dev, "tx_int_delay = %d, tx_abs_int_delay = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_TIDV),
	    E1000_READ_REG(&adapter->hw, E1000_TADV));
	device_printf(dev, "rx_int_delay = %d, rx_abs_int_delay = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_RDTR),
	    E1000_READ_REG(&adapter->hw, E1000_RADV));

	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		device_printf(dev, "Queue(%d) tdh = %d, tdt = %d\n", i,
		    E1000_READ_REG(&adapter->hw, E1000_TDH(i)),
		    E1000_READ_REG(&adapter->hw, E1000_TDT(i)));
		device_printf(dev, "TX(%d) no descriptors avail event = %ld\n",
		    txr->me, txr->no_desc_avail);
		device_printf(dev, "TX(%d) MSIX IRQ Handled = %ld\n",
		    txr->me, txr->tx_irq);
		device_printf(dev, "Num Tx descriptors avail = %d\n",
		    txr->tx_avail);
		device_printf(dev, "Tx Descriptors not avail1 = %ld\n",
		    txr->no_desc_avail);
	}
	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		device_printf(dev, "RX(%d) MSIX IRQ Handled = %ld\n",
		    rxr->me, rxr->rx_irq);
		device_printf(dev, "hw rdh = %d, hw rdt = %d\n",
		    E1000_READ_REG(&adapter->hw, E1000_RDH(i)),
		    E1000_READ_REG(&adapter->hw, E1000_RDT(i)));
	}
	device_printf(dev, "Std mbuf failed = %ld\n",
	    adapter->mbuf_alloc_failed);
	device_printf(dev, "Std mbuf cluster failed = %ld\n",
	    adapter->mbuf_cluster_failed);
	device_printf(dev, "Driver dropped packets = %ld\n",
	    adapter->dropped_pkts);
}

static void
em_print_hw_stats(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	device_printf(dev, "Excessive collisions = %lld\n",
	    (long long)adapter->stats.ecol);
#if	(DEBUG_HW > 0)  /* Dont output these errors normally */
	device_printf(dev, "Symbol errors = %lld\n",
	    (long long)adapter->stats.symerrs);
#endif
	device_printf(dev, "Sequence errors = %lld\n",
	    (long long)adapter->stats.sec);
	device_printf(dev, "Defer count = %lld\n",
	    (long long)adapter->stats.dc);
	device_printf(dev, "Missed Packets = %lld\n",
	    (long long)adapter->stats.mpc);
	device_printf(dev, "Receive No Buffers = %lld\n",
	    (long long)adapter->stats.rnbc);
	/* RLEC is inaccurate on some hardware, calculate our own. */
	device_printf(dev, "Receive Length Errors = %lld\n",
	    ((long long)adapter->stats.roc + (long long)adapter->stats.ruc));
	device_printf(dev, "Receive errors = %lld\n",
	    (long long)adapter->stats.rxerrc);
	device_printf(dev, "Crc errors = %lld\n",
	    (long long)adapter->stats.crcerrs);
	device_printf(dev, "Alignment errors = %lld\n",
	    (long long)adapter->stats.algnerrc);
	device_printf(dev, "Collision/Carrier extension errors = %lld\n",
	    (long long)adapter->stats.cexterr);
	device_printf(dev, "watchdog timeouts = %ld\n",
	    adapter->watchdog_events);
	device_printf(dev, "XON Rcvd = %lld\n",
	    (long long)adapter->stats.xonrxc);
	device_printf(dev, "XON Xmtd = %lld\n",
	    (long long)adapter->stats.xontxc);
	device_printf(dev, "XOFF Rcvd = %lld\n",
	    (long long)adapter->stats.xoffrxc);
	device_printf(dev, "XOFF Xmtd = %lld\n",
	    (long long)adapter->stats.xofftxc);
	device_printf(dev, "Good Packets Rcvd = %lld\n",
	    (long long)adapter->stats.gprc);
	device_printf(dev, "Good Packets Xmtd = %lld\n",
	    (long long)adapter->stats.gptc);
	device_printf(dev, "TSO Contexts Xmtd = %lld\n",
	    (long long)adapter->stats.tsctc);
	device_printf(dev, "TSO Contexts Failed = %lld\n",
	    (long long)adapter->stats.tsctfc);
}

/**********************************************************************
 *
 *  This routine provides a way to dump out the adapter eeprom,
 *  often a useful debug/service tool. This only dumps the first
 *  32 words, stuff that matters is in that extent.
 *
 **********************************************************************/
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
	/*
	 * This value will cause a hex dump of the
	 * first 32 16-bit words of the EEPROM to
	 * the screen.
	 */
	if (result == 2) {
		adapter = (struct adapter *)arg1;
		em_print_nvm_info(adapter);
        }

	return (error);
}


static int
em_sysctl_stats(SYSCTL_HANDLER_ARGS)
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
		em_print_hw_stats(adapter);
	}

	return (error);
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


