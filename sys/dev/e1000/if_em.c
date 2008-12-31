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
/*$FreeBSD: src/sys/dev/e1000/if_em.c,v 1.1.2.1.2.2 2008/11/30 04:41:25 jfv Exp $*/

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
#ifdef EM_TIMESYNC
#include <sys/ioccom.h>
#include <sys/time.h>
#endif
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
char em_driver_version[] = "6.9.6";


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
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_LF,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_R_BM_V,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_D_BM_LM,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_ICH10_D_BM_LF,	PCI_ANY_ID, PCI_ANY_ID, 0},
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
static void	em_start_locked(struct ifnet *ifp);
static int	em_ioctl(struct ifnet *, u_long, caddr_t);
static void	em_watchdog(struct adapter *);
static void	em_init(void *);
static void	em_init_locked(struct adapter *);
static void	em_stop(void *);
static void	em_media_status(struct ifnet *, struct ifmediareq *);
static int	em_media_change(struct ifnet *);
static void	em_identify_hardware(struct adapter *);
static int	em_allocate_pci_resources(struct adapter *);
static int	em_allocate_legacy(struct adapter *adapter);
static int	em_allocate_msix(struct adapter *adapter);
static int	em_setup_msix(struct adapter *);
static void	em_free_pci_resources(struct adapter *);
static void	em_local_timer(void *);
static int	em_hardware_init(struct adapter *);
static void	em_setup_interface(device_t, struct adapter *);
static void	em_setup_transmit_structures(struct adapter *);
static void	em_initialize_transmit_unit(struct adapter *);
static int	em_setup_receive_structures(struct adapter *);
static void	em_initialize_receive_unit(struct adapter *);
static void	em_enable_intr(struct adapter *);
static void	em_disable_intr(struct adapter *);
static void	em_free_transmit_structures(struct adapter *);
static void	em_free_receive_structures(struct adapter *);
static void	em_update_stats_counters(struct adapter *);
static void	em_txeof(struct adapter *);
static void	em_tx_purge(struct adapter *);
static int	em_allocate_receive_structures(struct adapter *);
static int	em_allocate_transmit_structures(struct adapter *);
static int	em_rxeof(struct adapter *, int);
#ifndef __NO_STRICT_ALIGNMENT
static int	em_fixup_rx(struct adapter *);
#endif
static void	em_receive_checksum(struct adapter *, struct e1000_rx_desc *,
		    struct mbuf *);
static void	em_transmit_checksum_setup(struct adapter *, struct mbuf *,
		    u32 *, u32 *);
#if __FreeBSD_version >= 700000
static bool	em_tso_setup(struct adapter *, struct mbuf *,
		    u32 *, u32 *);
#endif /* FreeBSD_version >= 700000 */
static void	em_set_promisc(struct adapter *);
static void	em_disable_promisc(struct adapter *);
static void	em_set_multi(struct adapter *);
static void	em_print_hw_stats(struct adapter *);
static void	em_update_link_status(struct adapter *);
static int	em_get_buf(struct adapter *, int);

#ifdef EM_HW_VLAN_SUPPORT
static void	em_register_vlan(void *, struct ifnet *, u16);
static void	em_unregister_vlan(void *, struct ifnet *, u16);
#endif

static int	em_xmit(struct adapter *, struct mbuf **);
static void	em_smartspeed(struct adapter *);
static int	em_82547_fifo_workaround(struct adapter *, int);
static void	em_82547_update_fifo_head(struct adapter *, int);
static int	em_82547_tx_fifo_reset(struct adapter *);
static void	em_82547_move_tail(void *);
static int	em_dma_malloc(struct adapter *, bus_size_t,
		    struct em_dma_alloc *, int);
static void	em_dma_free(struct adapter *, struct em_dma_alloc *);
static void	em_print_debug_info(struct adapter *);
static void	em_print_nvm_info(struct adapter *);
static int 	em_is_valid_ether_addr(u8 *);
static int	em_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static u32	em_fill_descriptors (bus_addr_t address, u32 length,
		    PDESC_ARRAY desc_array);
static int	em_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	em_add_int_delay_sysctl(struct adapter *, const char *,
		    const char *, struct em_int_delay_info *, int, int);
/* Management and WOL Support */
static void	em_init_manageability(struct adapter *);
static void	em_release_manageability(struct adapter *);
static void     em_get_hw_control(struct adapter *);
static void     em_release_hw_control(struct adapter *);
static void     em_enable_wakeup(device_t);

#ifdef EM_TIMESYNC
/* Precision Time sync support */
static int	em_tsync_init(struct adapter *);
static void	em_tsync_disable(struct adapter *);
#endif

#ifdef EM_LEGACY_IRQ
static void	em_intr(void *);
#else /* FAST IRQ */
#if __FreeBSD_version < 700000
static void	em_irq_fast(void *);
#else
static int	em_irq_fast(void *);
#endif
/* MSIX handlers */
static void	em_msix_tx(void *);
static void	em_msix_rx(void *);
static void	em_msix_link(void *);
static void	em_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static void	em_handle_rxtx(void *context, int pending);
static void	em_handle_rx(void *context, int pending);
static void	em_handle_tx(void *context, int pending);
static void	em_handle_link(void *context, int pending);
#endif /* EM_LEGACY_IRQ */

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

static devclass_t em_devclass;
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
static int em_tx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_TADV);
static int em_rx_abs_int_delay_dflt = EM_TICKS_TO_USECS(EM_RADV);
static int em_rxd = EM_DEFAULT_RXD;
static int em_txd = EM_DEFAULT_TXD;
static int em_smart_pwr_down = FALSE;
/* Controls whether promiscuous also shows bad packets */
static int em_debug_sbp = FALSE;
/* Local switch for MSI/MSIX */
static int em_enable_msi = TRUE;

TUNABLE_INT("hw.em.tx_int_delay", &em_tx_int_delay_dflt);
TUNABLE_INT("hw.em.rx_int_delay", &em_rx_int_delay_dflt);
TUNABLE_INT("hw.em.tx_abs_int_delay", &em_tx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rx_abs_int_delay", &em_rx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rxd", &em_rxd);
TUNABLE_INT("hw.em.txd", &em_txd);
TUNABLE_INT("hw.em.smart_pwr_down", &em_smart_pwr_down);
TUNABLE_INT("hw.em.sbp", &em_debug_sbp);
TUNABLE_INT("hw.em.enable_msi", &em_enable_msi);

#ifndef EM_LEGACY_IRQ
/* How many packets rxeof tries to clean at a time */
static int em_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &em_rx_process_limit);
#endif

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
	int		tsize, rsize;
	int		error = 0;
	u16		eeprom_data, device_id;

	INIT_DEBUGOUT("em_attach: begin");

	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	EM_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));
	EM_TX_LOCK_INIT(adapter, device_get_nameunit(dev));
	EM_RX_LOCK_INIT(adapter, device_get_nameunit(dev));

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
	callout_init_mtx(&adapter->tx_fifo_timer, &adapter->tx_mtx, 0);

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
	    (adapter->hw.mac.type == e1000_ich10lan) ||
	    (adapter->hw.mac.type == e1000_ich9lan)) {
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
	if (adapter->hw.mac.type >= e1000_82540) {
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
	}

#ifndef EM_LEGACY_IRQ
	/* Sysctls for limiting the amount of work done in the taskqueue */
	em_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    em_rx_process_limit);
#endif

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of E1000_DBA_ALIGN.
	 */
	if (((em_txd * sizeof(struct e1000_tx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac.type >= e1000_82544 && em_txd > EM_MAX_TXD) ||
	    (adapter->hw.mac.type < e1000_82544 && em_txd > EM_MAX_TXD_82543) ||
	    (em_txd < EM_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    EM_DEFAULT_TXD, em_txd);
		adapter->num_tx_desc = EM_DEFAULT_TXD;
	} else
		adapter->num_tx_desc = em_txd;
	if (((em_rxd * sizeof(struct e1000_rx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac.type >= e1000_82544 && em_rxd > EM_MAX_RXD) ||
	    (adapter->hw.mac.type < e1000_82544 && em_rxd > EM_MAX_RXD_82543) ||
	    (em_rxd < EM_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    EM_DEFAULT_RXD, em_rxd);
		adapter->num_rx_desc = EM_DEFAULT_RXD;
	} else
		adapter->num_rx_desc = em_rxd;

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

	tsize = roundup2(adapter->num_tx_desc * sizeof(struct e1000_tx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Transmit Descriptor ring */
	if (em_dma_malloc(adapter, tsize, &adapter->txdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate tx_desc memory\n");
		error = ENOMEM;
		goto err_tx_desc;
	}
	adapter->tx_desc_base = 
	    (struct e1000_tx_desc *)adapter->txdma.dma_vaddr;

	rsize = roundup2(adapter->num_rx_desc * sizeof(struct e1000_rx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Receive Descriptor ring */
	if (em_dma_malloc(adapter, rsize, &adapter->rxdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate rx_desc memory\n");
		error = ENOMEM;
		goto err_rx_desc;
	}
	adapter->rx_desc_base =
	    (struct e1000_rx_desc *)adapter->rxdma.dma_vaddr;

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

	/* Initialize the hardware */
	if (em_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (e1000_read_mac_addr(&adapter->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_hw_init;
	}

	if (!em_is_valid_ether_addr(adapter->hw.mac.addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Allocate transmit descriptors and buffers */
	if (em_allocate_transmit_structures(adapter)) {
		device_printf(dev, "Could not setup transmit structures\n");
		error = ENOMEM;
		goto err_tx_struct;
	}

	/* Allocate receive descriptors and buffers */
	if (em_allocate_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		error = ENOMEM;
		goto err_rx_struct;
	}

	/*
	**  Do interrupt configuration
	*/
	if (adapter->msi > 1) /* Do MSI/X */
		error = em_allocate_msix(adapter);
	else  /* MSI or Legacy */
		error = em_allocate_legacy(adapter);
	if (error)
		goto err_rx_struct;

	/* Setup OS specific network interface */
	em_setup_interface(dev, adapter);

	/* Initialize statistics */
	em_update_stats_counters(adapter);

	adapter->hw.mac.get_link_status = 1;
	em_update_link_status(adapter);

	/* Indicate SOL/IDER usage */
	if (e1000_check_reset_block(&adapter->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Determine if we have to control management hardware */
	adapter->has_manage = e1000_enable_mng_pass_thru(&adapter->hw);

	/*
	 * Setup Wake-on-Lan
	 */
	switch (adapter->hw.mac.type) {

	case e1000_82542:
	case e1000_82543:
		break;
	case e1000_82546:
	case e1000_82546_rev_3:
	case e1000_82571:
	case e1000_80003es2lan:
		if (adapter->hw.bus.func == 1)
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_B, 1, &eeprom_data);
		else
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_CONTROL3_PORT_A, 1, &eeprom_data);
		eeprom_data &= EM_EEPROM_APME;
		break;
	default:
		/* APME bit in EEPROM is mapped to WUC.APME */
		eeprom_data = E1000_READ_REG(&adapter->hw, E1000_WUC) &
		    E1000_WUC_APME;
		break;
	}
	if (eeprom_data)
		adapter->wol = E1000_WUFC_MAG;
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
	case E1000_DEV_ID_82571EB_FIBER:
		/* Wake events only supported on port A for dual fiber
		 * regardless of eeprom setting */
		if (E1000_READ_REG(&adapter->hw, E1000_STATUS) &
		    E1000_STATUS_FUNC_1)
			adapter->wol = 0;
		break;
	case E1000_DEV_ID_82546GB_QUAD_COPPER_KSP3:
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

	/* Do we need workaround for 82544 PCI-X adapter? */
	if (adapter->hw.bus.type == e1000_bus_type_pcix &&
	    adapter->hw.mac.type == e1000_82544)
		adapter->pcix_82544 = TRUE;
	else
		adapter->pcix_82544 = FALSE;

#ifdef EM_HW_VLAN_SUPPORT
	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    em_register_vlan, 0, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    em_unregister_vlan, 0, EVENTHANDLER_PRI_FIRST); 
#endif

	/* Tell the stack that the interface is not active */
	adapter->ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	INIT_DEBUGOUT("em_attach: end");

	return (0);

err_rx_struct:
	em_free_transmit_structures(adapter);
err_tx_struct:
err_hw_init:
	em_release_hw_control(adapter);
	em_dma_free(adapter, &adapter->rxdma);
err_rx_desc:
	em_dma_free(adapter, &adapter->txdma);
err_tx_desc:
err_pci:
	em_free_pci_resources(adapter);
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
em_detach(device_t dev)
{
	struct adapter	*adapter = device_get_softc(dev);
	struct ifnet	*ifp = adapter->ifp;

	INIT_DEBUGOUT("em_detach: begin");

	/* Make sure VLANS are not using driver */
#if __FreeBSD_version >= 700000
	if (adapter->ifp->if_vlantrunk != NULL) {
#else
	if (adapter->ifp->if_nvlans != 0) {
#endif   
		device_printf(dev,"Vlan in use, detach first\n");
		return (EBUSY);
	}

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	EM_CORE_LOCK(adapter);
	EM_TX_LOCK(adapter);
	adapter->in_detach = 1;
	em_stop(adapter);
	e1000_phy_hw_reset(&adapter->hw);

	em_release_manageability(adapter);

	if (((adapter->hw.mac.type == e1000_82573) ||
	    (adapter->hw.mac.type == e1000_ich8lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan) ||
	    (adapter->hw.mac.type == e1000_ich9lan)) &&
	    e1000_check_mng_mode(&adapter->hw))
		em_release_hw_control(adapter);

	if (adapter->wol) {
		E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
		E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
		em_enable_wakeup(dev);
	}

	EM_TX_UNLOCK(adapter);
	EM_CORE_UNLOCK(adapter);

#ifdef EM_HW_VLAN_SUPPORT
	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach); 
#endif

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);
	callout_drain(&adapter->tx_fifo_timer);

	em_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(ifp);

	em_free_transmit_structures(adapter);
	em_free_receive_structures(adapter);

	/* Free Transmit Descriptor ring */
	if (adapter->tx_desc_base) {
		em_dma_free(adapter, &adapter->txdma);
		adapter->tx_desc_base = NULL;
	}

	/* Free Receive Descriptor ring */
	if (adapter->rx_desc_base) {
		em_dma_free(adapter, &adapter->rxdma);
		adapter->rx_desc_base = NULL;
	}

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

	EM_TX_LOCK(adapter);
	em_stop(adapter);
	EM_TX_UNLOCK(adapter);

        em_release_manageability(adapter);

        if (((adapter->hw.mac.type == e1000_82573) ||
            (adapter->hw.mac.type == e1000_ich8lan) ||
            (adapter->hw.mac.type == e1000_ich10lan) ||
            (adapter->hw.mac.type == e1000_ich9lan)) &&
            e1000_check_mng_mode(&adapter->hw))
                em_release_hw_control(adapter);

        if (adapter->wol) {
                E1000_WRITE_REG(&adapter->hw, E1000_WUC, E1000_WUC_PME_EN);
                E1000_WRITE_REG(&adapter->hw, E1000_WUFC, adapter->wol);
                em_enable_wakeup(dev);
        }

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

static void
em_start_locked(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct mbuf	*m_head;

	EM_TX_LOCK_ASSERT(adapter);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 *  Encapsulation can modify our pointer, and or make it
		 *  NULL on failure.  In that event, we can't requeue.
		 */
		if (em_xmit(adapter, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting. */
		adapter->watchdog_timer = EM_TX_TIMEOUT;
	}
}

static void
em_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	EM_TX_LOCK(adapter);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		em_start_locked(ifp);
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
em_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int error = 0;

	if (adapter->in_detach)
		return (error);

	switch (command) {
	case SIOCSIFADDR:
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
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;
		u16 eeprom_data = 0;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		EM_CORE_LOCK(adapter);
		switch (adapter->hw.mac.type) {
		case e1000_82573:
			/*
			 * 82573 only supports jumbo frames
			 * if ASPM is disabled.
			 */
			e1000_read_nvm(&adapter->hw,
			    NVM_INIT_3GIO_3, 1, &eeprom_data);
			if (eeprom_data & NVM_WORD1A_ASPM_MASK) {
				max_frame_size = ETHER_MAX_LEN;
				break;
			}
			/* Allow Jumbo frames - fall thru */
		case e1000_82571:
		case e1000_82572:
		case e1000_ich9lan:
		case e1000_ich10lan:
		case e1000_82574:
		case e1000_80003es2lan:	/* Limit Jumbo Frame size */
			max_frame_size = 9234;
			break;
			/* Adapters that do not support jumbo frames */
		case e1000_82542:
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
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				EM_TX_LOCK(adapter);
				em_stop(adapter);
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
			em_disable_intr(adapter);
			em_set_multi(adapter);
			if (adapter->hw.mac.type == e1000_82542 && 
	    		    adapter->hw.revision_id == E1000_REVISION_2) {
				em_initialize_receive_unit(adapter);
			}
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
#if __FreeBSD_version >= 700000
		if (mask & IFCAP_TSO4) {
			ifp->if_capenable ^= IFCAP_TSO4;
			reinit = 1;
		}
#endif

		if (mask & IFCAP_VLAN_HWTAGGING) {
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
			reinit = 1;
		}
		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING))
			em_init(adapter);
#if __FreeBSD_version >= 700000
		VLAN_CAPABILITIES(ifp);
#endif
		break;
	    }

#ifdef EM_TIMESYNC
	/*
	** IOCTL support for Precision Time (IEEE 1588) Support
	*/
	case EM_TIMESYNC_READTS:
	    {
		u32 rx_ctl, tx_ctl;
		struct em_tsync_read *tdata;

		tdata = (struct em_tsync_read *) ifr->ifr_data;

		IOCTL_DEBUGOUT("Reading Timestamp\n");

		if (tdata->read_current_time) {
			getnanotime(&tdata->system_time);
			tdata->network_time = E1000_READ_REG(&adapter->hw, E1000_SYSTIML);
			tdata->network_time |=
			    (u64)E1000_READ_REG(&adapter->hw, E1000_SYSTIMH ) << 32;
		}
 
		rx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCRXCTL);
		tx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCTXCTL);

		IOCTL_DEBUGOUT1("RX_CTL value = %u\n", rx_ctl);
		IOCTL_DEBUGOUT1("TX_CTL value = %u\n", tx_ctl);

		if (rx_ctl & 0x1) {
			IOCTL_DEBUGOUT("RX timestamp is valid\n");
			u32 tmp;
			unsigned char *tmp_cp;

			tdata->rx_valid = 1;
			tdata->rx_stamp = E1000_READ_REG(&adapter->hw, E1000_RXSTMPL);
			tdata->rx_stamp |= (u64)E1000_READ_REG(&adapter->hw,
			    E1000_RXSTMPH) << 32;

			tmp = E1000_READ_REG(&adapter->hw, E1000_RXSATRL);
			tmp_cp = (unsigned char *) &tmp;
			tdata->srcid[0] = tmp_cp[0];
			tdata->srcid[1] = tmp_cp[1];
			tdata->srcid[2] = tmp_cp[2];
			tdata->srcid[3] = tmp_cp[3];
			tmp = E1000_READ_REG(&adapter->hw, E1000_RXSATRH);
			tmp_cp = (unsigned char *) &tmp;
			tdata->srcid[4] = tmp_cp[0];
			tdata->srcid[5] = tmp_cp[1];
			tdata->seqid = tmp >> 16;
			tdata->seqid = htons(tdata->seqid);
		} else
			tdata->rx_valid = 0;

		if (tx_ctl & 0x1) {
			IOCTL_DEBUGOUT("TX timestamp is valid\n");
			tdata->tx_valid = 1;
			tdata->tx_stamp = E1000_READ_REG(&adapter->hw, E1000_TXSTMPL);
			tdata->tx_stamp |= (u64) E1000_READ_REG(&adapter->hw,
			    E1000_TXSTMPH) << 32;
		} else
			tdata->tx_valid = 0;

		return (0);
	    }
#endif	/* EM_TIMESYNC */

	default:
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

/*********************************************************************
 *  Watchdog timer:
 *
 *  This routine is called from the local timer every second.
 *  As long as transmit descriptors are being cleaned the value
 *  is non-zero and we do nothing. Reaching 0 indicates a tx hang
 *  and we then reset the device.
 *
 **********************************************************************/

static void
em_watchdog(struct adapter *adapter)
{

	EM_CORE_LOCK_ASSERT(adapter);

	/*
	** The timer is set to 5 every time start queues a packet.
	** Then txeof keeps resetting it as long as it cleans at
	** least one descriptor.
	** Finally, anytime all descriptors are clean the timer is
	** set to 0.
	*/
	EM_TX_LOCK(adapter);
	if ((adapter->watchdog_timer == 0) || (--adapter->watchdog_timer)) {
		EM_TX_UNLOCK(adapter);
		return;
	}

	/* If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (E1000_READ_REG(&adapter->hw, E1000_STATUS) &
	    E1000_STATUS_TXOFF) {
		adapter->watchdog_timer = EM_TX_TIMEOUT;
		EM_TX_UNLOCK(adapter);
		return;
	}

	if (e1000_check_for_link(&adapter->hw) == 0)
		device_printf(adapter->dev, "watchdog timeout -- resetting\n");
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	EM_TX_UNLOCK(adapter);

	em_init_locked(adapter);
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

	EM_TX_LOCK(adapter);
	em_stop(adapter);
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
			pba = E1000_PBA_20K; /* 20K for Rx, 20K for Tx */
		break;
	case e1000_ich9lan:
	case e1000_ich10lan:
#define E1000_PBA_10K	0x000A
		pba = E1000_PBA_10K;
		break;
	case e1000_ich8lan:
		pba = E1000_PBA_8K;
		break;
	default:
		/* Devices before 82547 had a Packet Buffer of 64K.   */
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
	if (em_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		return;
	}
	em_update_link_status(adapter);

	/* Setup VLAN support, basic and offload if available */
	E1000_WRITE_REG(&adapter->hw, E1000_VET, ETHERTYPE_VLAN);

#ifndef EM_HW_VLAN_SUPPORT
	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING) {
		u32 ctrl;
		ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
		ctrl |= E1000_CTRL_VME;
		E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);
	}
#endif
	/* Set hardware offload abilities */
	ifp->if_hwassist = 0;
	if (adapter->hw.mac.type >= e1000_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
#if __FreeBSD_version >= 700000
		if (ifp->if_capenable & IFCAP_TSO4)
			ifp->if_hwassist |= CSUM_TSO;
#endif
	}

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
		EM_TX_LOCK(adapter);
		em_stop(adapter);
		EM_TX_UNLOCK(adapter);
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
		/*
		** Set the IVAR - interrupt vector routing.
		** Each nibble represents a vector, high bit
		** is enable, other 3 bits are the MSIX table
		** entry, we map RXQ0 to 0, TXQ0 to 1, and
		** Link (other) to 2, hence the magic number.
		*/
		E1000_WRITE_REG(&adapter->hw, E1000_IVAR, 0x800A0908);
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

#ifdef EM_TIMESYNC
	/* Initializae IEEE 1588 Precision Time hardware */
	if ((adapter->hw.mac.type == e1000_82574) ||
	    (adapter->hw.mac.type == e1000_ich10lan))
		em_tsync_init(adapter);
#endif

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
 *  Legacy polling routine  
 *
 *********************************************************************/
static void
em_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	u32		reg_icr;

	EM_CORE_LOCK(adapter);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		EM_CORE_UNLOCK(adapter);
		return;
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

	em_rxeof(adapter, count);

	EM_TX_LOCK(adapter);
	em_txeof(adapter);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp);
	EM_TX_UNLOCK(adapter);
}
#endif /* DEVICE_POLLING */

#ifdef EM_LEGACY_IRQ 
/*********************************************************************
 *
 *  Legacy Interrupt Service routine  
 *
 *********************************************************************/

static void
em_intr(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = adapter->ifp;
	u32		reg_icr;


	if (ifp->if_capenable & IFCAP_POLLING)
		return;

	EM_CORE_LOCK(adapter);
	for (;;) {
		reg_icr = E1000_READ_REG(&adapter->hw, E1000_ICR);

		if (adapter->hw.mac.type >= e1000_82571 &&
	    	    (reg_icr & E1000_ICR_INT_ASSERTED) == 0)
			break;
		else if (reg_icr == 0)
			break;

		/*
		 * XXX: some laptops trigger several spurious interrupts
		 * on em(4) when in the resume cycle. The ICR register
		 * reports all-ones value in this case. Processing such
		 * interrupts would lead to a freeze. I don't know why.
		 */
		if (reg_icr == 0xffffffff)
			break;

		EM_CORE_UNLOCK(adapter);
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			em_rxeof(adapter, -1);
			EM_TX_LOCK(adapter);
			em_txeof(adapter);
			EM_TX_UNLOCK(adapter);
		}
		EM_CORE_LOCK(adapter);

		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&adapter->timer);
			adapter->hw.mac.get_link_status = 1;
			em_update_link_status(adapter);
			/* Deal with TX cruft when link lost */
			em_tx_purge(adapter);
			callout_reset(&adapter->timer, hz,
			    em_local_timer, adapter);
		}

		if (reg_icr & E1000_ICR_RXO)
			adapter->rx_overruns++;
	}
	EM_CORE_UNLOCK(adapter);

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start(ifp);
}

#else /* EM_FAST_IRQ, then fast interrupt routines only */

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
	/* Deal with TX cruft when link lost */
	em_tx_purge(adapter);
	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	EM_CORE_UNLOCK(adapter);
}


/* Combined RX/TX handler, used by Legacy and MSI */
static void
em_handle_rxtx(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;


	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		if (em_rxeof(adapter, adapter->rx_process_limit) != 0)
			taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);
		EM_TX_LOCK(adapter);
		em_txeof(adapter);

		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			em_start_locked(ifp);
		EM_TX_UNLOCK(adapter);
	}

	em_enable_intr(adapter);
}

static void
em_handle_rx(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;

	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
	    (em_rxeof(adapter, adapter->rx_process_limit) != 0))
		taskqueue_enqueue(adapter->tq, &adapter->rx_task);

}

static void
em_handle_tx(void *context, int pending)
{
	struct adapter	*adapter = context;
	struct ifnet	*ifp = adapter->ifp;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		EM_TX_LOCK(adapter);
		em_txeof(adapter);
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			em_start_locked(ifp);
		EM_TX_UNLOCK(adapter);
	}
}

/*********************************************************************
 *
 *  Fast Legacy/MSI Combined Interrupt Service routine  
 *
 *********************************************************************/
#if __FreeBSD_version < 700000
#define FILTER_STRAY
#define FILTER_HANDLED
static void
#else
static int
#endif
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

	/*
	 * Mask interrupts until the taskqueue is finished running.  This is
	 * cheap, just assume that it is needed.  This also works around the
	 * MSI message reordering errata on certain systems.
	 */
	em_disable_intr(adapter);
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
 *  MSIX Interrupt Service Routines
 *
 **********************************************************************/
#define EM_MSIX_TX	0x00040000
#define EM_MSIX_RX	0x00010000
#define EM_MSIX_LINK	0x00100000

static void
em_msix_tx(void *arg)
{
	struct adapter *adapter = arg;
	struct ifnet	*ifp = adapter->ifp;

	++adapter->tx_irq;
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		EM_TX_LOCK(adapter);
		em_txeof(adapter);
		EM_TX_UNLOCK(adapter);
		taskqueue_enqueue(adapter->tq, &adapter->tx_task);
	}
	/* Reenable this interrupt */
	E1000_WRITE_REG(&adapter->hw, E1000_IMS, EM_MSIX_TX);
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
	struct adapter *adapter = arg;
	struct ifnet	*ifp = adapter->ifp;

	++adapter->rx_irq;
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
	    (em_rxeof(adapter, adapter->rx_process_limit) != 0))
		taskqueue_enqueue(adapter->tq, &adapter->rx_task);
	/* Reenable this interrupt */
	E1000_WRITE_REG(&adapter->hw, E1000_IMS, EM_MSIX_RX);
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
	}
	E1000_WRITE_REG(&adapter->hw, E1000_IMS,
	    EM_MSIX_LINK | E1000_IMS_LSC);
	return;
}
#endif /* EM_FAST_IRQ */

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
em_xmit(struct adapter *adapter, struct mbuf **m_headp)
{
	bus_dma_segment_t	segs[EM_MAX_SCATTER];
	bus_dmamap_t		map;
	struct em_buffer	*tx_buffer, *tx_buffer_mapped;
	struct e1000_tx_desc	*ctxd = NULL;
	struct mbuf		*m_head;
	u32			txd_upper, txd_lower, txd_used, txd_saved;
	int			nsegs, i, j, first, last = 0;
	int			error, do_tso, tso_desc = 0;
#if __FreeBSD_version < 700000
	struct m_tag		*mtag;
#endif
	m_head = *m_headp;
	txd_upper = txd_lower = txd_used = txd_saved = 0;

#if __FreeBSD_version >= 700000
	do_tso = ((m_head->m_pkthdr.csum_flags & CSUM_TSO) != 0);
#else
	do_tso = 0;
#endif

        /*
         * Force a cleanup if number of TX descriptors
         * available hits the threshold
         */
	if (adapter->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
		em_txeof(adapter);
		/* Now do we at least have a minimal? */
		if (adapter->num_tx_desc_avail <= EM_TX_OP_THRESHOLD) {
			adapter->no_tx_desc_avail1++;
			return (ENOBUFS);
		}
	}


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

		m = m_defrag(*m_headp, M_DONTWAIT);
		if (m == NULL) {
			adapter->mbuf_alloc_failed++;
			m_freem(*m_headp);
			*m_headp = NULL;
			return (ENOBUFS);
		}
		*m_headp = m;

		/* Try it again */
		error = bus_dmamap_load_mbuf_sg(adapter->txtag, map,
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
	if ((do_tso == 0) && (adapter->tx_tso == TRUE)) {
		if (nsegs == 1)
			tso_desc = TRUE;
		adapter->tx_tso = FALSE;
	}

        if (nsegs > (adapter->num_tx_desc_avail - 2)) {
                adapter->no_tx_desc_avail2++;
		bus_dmamap_unload(adapter->txtag, map);
		return (ENOBUFS);
        }
	m_head = *m_headp;

	/* Do hardware assists */
#if __FreeBSD_version >= 700000
	if (m_head->m_pkthdr.csum_flags & CSUM_TSO) {
		error = em_tso_setup(adapter, m_head, &txd_upper, &txd_lower);
		if (error != TRUE)
			return (ENXIO); /* something foobar */
		/* we need to make a final sentinel transmit desc */
		tso_desc = TRUE;
	} else
#endif
#ifndef EM_TIMESYNC
	/*
	** Timesync needs to check the packet header 
	** so call checksum code to do so, but don't
	** penalize the code if not defined.
	*/
	if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD)
#endif
		em_transmit_checksum_setup(adapter,  m_head,
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
			array_elements = em_fill_descriptors(segs[j].ds_addr,
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
				ctxd = &adapter->tx_desc_base[i];
				tx_buffer = &adapter->tx_buffer_area[i];
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
	}

	adapter->next_avail_tx_desc = i;
	if (adapter->pcix_82544)
		adapter->num_tx_desc_avail -= txd_used;
	else {
		adapter->num_tx_desc_avail -= nsegs;
		if (tso_desc) /* TSO used an extra for sentinel */
			adapter->num_tx_desc_avail -= txd_used;
	}

        /*
	** Handle VLAN tag, this is the
	** biggest difference between 
	** 6.x and 7
	*/
#if __FreeBSD_version < 700000
        /* Find out if we are in vlan mode. */
        mtag = VLAN_OUTPUT_TAG(ifp, m_head);
        if (mtag != NULL) {
                ctxd->upper.fields.special =
                    htole16(VLAN_TAG_VALUE(mtag));
#else /* FreeBSD 7 */
	if (m_head->m_flags & M_VLANTAG) {
		/* Set the vlan id. */
		ctxd->upper.fields.special =
		    htole16(m_head->m_pkthdr.ether_vtag);
#endif
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

	/*
	 * Advance the Transmit Descriptor Tail (TDT), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	if (adapter->hw.mac.type == e1000_82547 &&
	    adapter->link_duplex == HALF_DUPLEX)
		em_82547_move_tail(adapter);
	else {
		E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), i);
		if (adapter->hw.mac.type == e1000_82547)
			em_82547_update_fifo_head(adapter,
			    m_head->m_pkthdr.len);
	}

#ifdef EM_TIMESYNC
	if (ctxd->upper.data & E1000_TXD_EXTCMD_TSTAMP) {
		HW_DEBUGOUT( "@@@ Timestamp bit is set in transmit descriptor\n" );
	} 
#endif
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
em_82547_move_tail(void *arg)
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
			if (em_82547_fifo_workaround(adapter, length)) {
				adapter->tx_fifo_wrk_cnt++;
				callout_reset(&adapter->tx_fifo_timer, 1,
					em_82547_move_tail, adapter);
				break;
			}
			E1000_WRITE_REG(&adapter->hw, E1000_TDT(0), hw_tdt);
			em_82547_update_fifo_head(adapter, length);
			length = 0;
		}
	}	
}

static int
em_82547_fifo_workaround(struct adapter *adapter, int len)
{	
	int fifo_space, fifo_pkt_len;

	fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);

	if (adapter->link_duplex == HALF_DUPLEX) {
		fifo_space = adapter->tx_fifo_size - adapter->tx_fifo_head;

		if (fifo_pkt_len >= (EM_82547_PKT_THRESH + fifo_space)) {
			if (em_82547_tx_fifo_reset(adapter))
				return (0);
			else
				return (1);
		}
	}

	return (0);
}

static void
em_82547_update_fifo_head(struct adapter *adapter, int len)
{
	int fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);
	
	/* tx_fifo_head is always 16 byte aligned */
	adapter->tx_fifo_head += fifo_pkt_len;
	if (adapter->tx_fifo_head >= adapter->tx_fifo_size) {
		adapter->tx_fifo_head -= adapter->tx_fifo_size;
	}
}


static int
em_82547_tx_fifo_reset(struct adapter *adapter)
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
	u8  mta[512]; /* Largest MTS is 4096 bits */
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

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;

		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    &mta[mcnt * ETH_ADDR_LEN], ETH_ADDR_LEN);
		mcnt++;
	}
	IF_ADDR_UNLOCK(ifp);

	if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		reg_rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, reg_rctl);
	} else
		e1000_update_mc_addr_list(&adapter->hw, mta,
		    mcnt, 1, adapter->hw.mac.rar_entry_count);

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

	EM_CORE_LOCK_ASSERT(adapter);

	em_update_link_status(adapter);
	em_update_stats_counters(adapter);

	/* Reset LAA into RAR[0] on 82571 */
	if (e1000_get_laa_state_82571(&adapter->hw) == TRUE)
		e1000_rar_set(&adapter->hw, adapter->hw.mac.addr, 0);

	if (em_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING)
		em_print_hw_stats(adapter);

	em_smartspeed(adapter);

	/*
	 * Each second we check the watchdog to 
	 * protect against hardware hangs.
	 */
	em_watchdog(adapter);

	callout_reset(&adapter->timer, hz, em_local_timer, adapter);

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
		adapter->watchdog_timer = FALSE;
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

	EM_CORE_LOCK_ASSERT(adapter);
	EM_TX_LOCK_ASSERT(adapter);

	INIT_DEBUGOUT("em_stop: begin");

	em_disable_intr(adapter);
	callout_stop(&adapter->timer);
	callout_stop(&adapter->tx_fifo_timer);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

#ifdef EM_TIMESYNC
	/* Disable IEEE 1588 Time hardware */
	if ((adapter->hw.mac.type == e1000_82574) ||
	    (adapter->hw.mac.type == e1000_ich10lan))
		em_tsync_disable(adapter);
#endif

	e1000_reset_hw(&adapter->hw);
	if (adapter->hw.mac.type >= e1000_82544)
		E1000_WRITE_REG(&adapter->hw, E1000_WUC, 0);
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
	if ((adapter->hw.mac.type > e1000_82543) &&
	    (adapter->hw.mac.type < e1000_82571)) {
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

	/*
	** Init the resource arrays
	**  used by MSIX setup 
	*/
	for (int i = 0; i < 3; i++) {
		adapter->rid[i] = i + 1; /* MSI/X RID starts at 1 */
		adapter->tag[i] = NULL;
		adapter->res[i] = NULL;
	}

	/*
	 * Setup MSI/X or MSI if PCI Express
	 */
	if (em_enable_msi)
		adapter->msi = em_setup_msix(adapter);

	adapter->hw.back = &adapter->osdep;

	return (error);
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
	int error;

	/* Manually turn off all interrupts */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	/* Legacy RID is 0 */
	if (adapter->msi == 0)
		adapter->rid[0] = 0;

	/* We allocate a single interrupt resource */
	adapter->res[0] = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &adapter->rid[0], RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res[0] == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

#ifdef EM_LEGACY_IRQ
	/* We do Legacy setup */
	if ((error = bus_setup_intr(dev, adapter->res[0],
#if __FreeBSD_version > 700000
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_intr, adapter,
#else /* 6.X */
	    INTR_TYPE_NET | INTR_MPSAFE, em_intr, adapter,
#endif
	    &adapter->tag[0])) != 0) {
		device_printf(dev, "Failed to register interrupt handler");
		return (error);
	}

#else /* FAST_IRQ */
	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
	TASK_INIT(&adapter->rxtx_task, 0, em_handle_rxtx, adapter);
	TASK_INIT(&adapter->link_task, 0, em_handle_link, adapter);
	adapter->tq = taskqueue_create_fast("em_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(adapter->dev));
#if __FreeBSD_version < 700000
	if ((error = bus_setup_intr(dev, adapter->res[0],
	    INTR_TYPE_NET | INTR_FAST, em_irq_fast, adapter,
#else
	if ((error = bus_setup_intr(dev, adapter->res[0],
	    INTR_TYPE_NET, em_irq_fast, NULL, adapter,
#endif
	    &adapter->tag[0])) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
			    "handler: %d\n", error);
		taskqueue_free(adapter->tq);
		adapter->tq = NULL;
		return (error);
	}
#endif  /* EM_LEGACY_IRQ */
	
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
	device_t dev = adapter->dev;
	int error;

	/* Make sure all interrupts are disabled */
	E1000_WRITE_REG(&adapter->hw, E1000_IMC, 0xffffffff);

	/* First get the resources */
	for (int i = 0; i < adapter->msi; i++) {
		adapter->res[i] = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &adapter->rid[i], RF_ACTIVE);
		if (adapter->res[i] == NULL) {
			device_printf(dev,
			    "Unable to allocate bus resource: "
			    "MSIX Interrupt\n");
			return (ENXIO);
		}
	}

	/*
	 * Now allocate deferred processing contexts.
	 */
	TASK_INIT(&adapter->rx_task, 0, em_handle_rx, adapter);
	TASK_INIT(&adapter->tx_task, 0, em_handle_tx, adapter);
	TASK_INIT(&adapter->link_task, 0, em_handle_link, adapter);
	adapter->tq = taskqueue_create_fast("em_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(adapter->dev));

	/*
	 * And setup the interrupt handlers
	 */

	/* First slot to RX */
	if ((error = bus_setup_intr(dev, adapter->res[0],
#if __FreeBSD_version > 700000
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_msix_rx, adapter,
#else /* 6.X */
	    INTR_TYPE_NET | INTR_MPSAFE, em_msix_rx, adapter,
#endif
	    &adapter->tag[0])) != 0) {
		device_printf(dev, "Failed to register RX handler");
		return (error);
	}

	/* Next TX */
	if ((error = bus_setup_intr(dev, adapter->res[1],
#if __FreeBSD_version > 700000
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_msix_tx, adapter,
#else /* 6.X */
	    INTR_TYPE_NET | INTR_MPSAFE, em_msix_tx, adapter,
#endif
	    &adapter->tag[1])) != 0) {
		device_printf(dev, "Failed to register TX handler");
		return (error);
	}

	/* And Link */
	if ((error = bus_setup_intr(dev, adapter->res[2],
#if __FreeBSD_version > 700000
	    INTR_TYPE_NET | INTR_MPSAFE, NULL, em_msix_link, adapter,
#else /* 6.X */
	    INTR_TYPE_NET | INTR_MPSAFE, em_msix_link, adapter,
#endif
	    &adapter->tag[2])) != 0) {
		device_printf(dev, "Failed to register TX handler");
		return (error);
	}

	return (0);
}

static void
em_free_pci_resources(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	/* Make sure the for loop below runs once */
	if (adapter->msi == 0)
		adapter->msi = 1;

	/*
	 * First release all the interrupt resources:
	 *      notice that since these are just kept
	 *      in an array we can do the same logic
	 *      whether its MSIX or just legacy.
	 */
	for (int i = 0; i < adapter->msi; i++) {
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

	if (adapter->msi)
		pci_release_msi(dev);

	if (adapter->msix != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(EM_MSIX_BAR), adapter->msix);

	if (adapter->memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->memory);

	if (adapter->flash != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    EM_FLASH, adapter->flash);

	if (adapter->ioport != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    adapter->io_rid, adapter->ioport);
}

/*
 * Setup MSI/X
 */
static int
em_setup_msix(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int val = 0;

	if (adapter->hw.mac.type < e1000_82571)
		return (0);

	/* Setup MSI/X for Hartwell */
	if (adapter->hw.mac.type == e1000_82574) {
		/* Map the MSIX BAR */
		int rid = PCIR_BAR(EM_MSIX_BAR);
		adapter->msix = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       		if (!adapter->msix) {
			/* May not be enabled */
               		device_printf(adapter->dev,
			    "Unable to map MSIX table \n");
			goto msi;
       		}
		val = pci_msix_count(dev); 
		/*
		** 82574 can be configured for 5 but
		** we limit use to 3.
		*/
		if (val > 3) val = 3;
		if ((val) && pci_alloc_msix(dev, &val) == 0) {
               		device_printf(adapter->dev,"Using MSIX interrupts\n");
			return (val);
		}
	}
msi:
       	val = pci_msi_count(dev);
       	if (val == 1 && pci_alloc_msi(dev, &val) == 0) {
               	adapter->msi = 1;
               	device_printf(adapter->dev,"Using MSI interrupt\n");
		return (val);
	} 
	return (0);
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration
 *  as specified by the adapter structure.
 *
 **********************************************************************/
static int
em_hardware_init(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	u16 	rx_buffer_size;

	INIT_DEBUGOUT("em_hardware_init: begin");

	/* Issue a global reset */
	e1000_reset_hw(&adapter->hw);

	/* Get control from any management/hw control */
	if (((adapter->hw.mac.type == e1000_82573) ||
	    (adapter->hw.mac.type == e1000_ich8lan) ||
	    (adapter->hw.mac.type == e1000_ich10lan) ||
	    (adapter->hw.mac.type == e1000_ich9lan)) &&
	    e1000_check_mng_mode(&adapter->hw))
		em_get_hw_control(adapter);

	/* When hardware is reset, fifo_head is also reset */
	adapter->tx_fifo_head = 0;

	/* Set up smart power down as default off on newer adapters. */
	if (!em_smart_pwr_down && (adapter->hw.mac.type == e1000_82571 ||
	    adapter->hw.mac.type == e1000_82572)) {
		u16 phy_tmp = 0;

		/* Speed up time to link by disabling smart power down. */
		e1000_read_phy_reg(&adapter->hw,
		    IGP02E1000_PHY_POWER_MGMT, &phy_tmp);
		phy_tmp &= ~IGP02E1000_PM_SPD;
		e1000_write_phy_reg(&adapter->hw,
		    IGP02E1000_PHY_POWER_MGMT, phy_tmp);
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
	rx_buffer_size = ((E1000_READ_REG(&adapter->hw, E1000_PBA) &
	    0xffff) << 10 );

	adapter->hw.fc.high_water = rx_buffer_size -
	    roundup2(adapter->max_frame_size, 1024);
	adapter->hw.fc.low_water = adapter->hw.fc.high_water - 1500;

	if (adapter->hw.mac.type == e1000_80003es2lan)
		adapter->hw.fc.pause_time = 0xFFFF;
	else
		adapter->hw.fc.pause_time = EM_FC_PAUSE_TIME;
	adapter->hw.fc.send_xon = TRUE;
	adapter->hw.fc.requested_mode = e1000_fc_full;

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

	if (adapter->hw.mac.type >= e1000_82543) {
		int version_cap;
#if __FreeBSD_version < 700000
		version_cap = IFCAP_HWCSUM;
#else
		version_cap = IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
#endif
		ifp->if_capabilities |= version_cap;
		ifp->if_capenable |= version_cap;
	}

#if __FreeBSD_version >= 700000
	/* Identify TSO capable adapters */
	if ((adapter->hw.mac.type > e1000_82544) &&
	    (adapter->hw.mac.type != e1000_82547))
		ifp->if_capabilities |= IFCAP_TSO4;
	/*
	 * By default only enable on PCI-E, this
	 * can be overriden by ifconfig.
	 */
	if (adapter->hw.mac.type >= e1000_82571)
		ifp->if_capenable |= IFCAP_TSO4;
#endif

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK,
	    em_media_change, em_media_status);
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
}


/*********************************************************************
 *
 *  Workaround for SmartSpeed on 82541 and 82547 controllers
 *
 **********************************************************************/
static void
em_smartspeed(struct adapter *adapter)
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
				   !e1000_phy_setup_autoneg(&adapter->hw) &&
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
		   !e1000_phy_setup_autoneg(&adapter->hw) &&
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

#if __FreeBSD_version >= 700000
	error = bus_dma_tag_create(bus_get_dma_tag(adapter->dev), /* parent */
#else
	error = bus_dma_tag_create(NULL,		 /* parent */
#endif
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
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire.
 *
 **********************************************************************/
static int
em_allocate_transmit_structures(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct em_buffer *tx_buffer;
	int error;

	/*
	 * Create DMA tags for tx descriptors
	 */
#if __FreeBSD_version >= 700000
	if ((error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
#else
	if ((error = bus_dma_tag_create(NULL,		 /* parent */
#endif
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				EM_TSO_SIZE,		/* maxsize */
				EM_MAX_SCATTER,		/* nsegments */
				EM_TSO_SEG_SIZE,	/* maxsegsize */
				0,			/* flags */
				NULL,		/* lockfunc */
				NULL,		/* lockarg */
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
	em_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  (Re)Initialize transmit structures.
 *
 **********************************************************************/
static void
em_setup_transmit_structures(struct adapter *adapter)
{
	struct em_buffer *tx_buffer;

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
		tx_buffer->next_eop = -1;
	}

	/* Reset state */
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
em_initialize_transmit_unit(struct adapter *adapter)
{
	u32	tctl, tarc, tipg = 0;
	u64	bus_addr;

	 INIT_DEBUGOUT("em_initialize_transmit_unit: begin");
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

	/* Program the Transmit Control Register */
	tctl = E1000_READ_REG(&adapter->hw, E1000_TCTL);
	tctl &= ~E1000_TCTL_CT;
	tctl |= (E1000_TCTL_PSP | E1000_TCTL_RTLC | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT));

	if (adapter->hw.mac.type >= e1000_82571)
		tctl |= E1000_TCTL_MULR;

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
em_free_transmit_structures(struct adapter *adapter)
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
 **********************************************************************/
static void
em_transmit_checksum_setup(struct adapter *adapter, struct mbuf *mp,
    u32 *txd_upper, u32 *txd_lower)
{
	struct e1000_context_desc *TXD;
	struct em_buffer *tx_buffer;
	struct ether_vlan_header *eh;
	struct ip *ip = NULL;
	struct ip6_hdr *ip6;
	struct tcp_hdr *th;
	int curr_txd, ehdrlen;
	u32 cmd, hdr_len, ip_hlen;
	u16 etype;
	u8 ipproto;

	cmd = hdr_len = ipproto = 0;
	/* Setup checksum offload context. */
	curr_txd = adapter->next_avail_tx_desc;
	tx_buffer = &adapter->tx_buffer_area[curr_txd];
	TXD = (struct e1000_context_desc *) &adapter->tx_desc_base[curr_txd];

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
#ifdef EM_TIMESYNC
	case ETHERTYPE_IEEE1588:
		*txd_upper |= E1000_TXD_EXTCMD_TSTAMP;
		break;
#endif
	default:
		*txd_upper = 0;
		*txd_lower = 0;
		return;
	}

	switch (ipproto) {
	case IPPROTO_TCP:
		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			/*
			 * Start offset for payload checksum calculation.
			 * End offset for payload checksum calculation.
			 * Offset of place to put the checksum.
			 */
			th = (struct tcp_hdr *)(mp->m_data + hdr_len);
			TXD->upper_setup.tcp_fields.tucss = hdr_len;
			TXD->upper_setup.tcp_fields.tucse = htole16(0);
			TXD->upper_setup.tcp_fields.tucso =
			    hdr_len + offsetof(struct tcphdr, th_sum);
			cmd |= E1000_TXD_CMD_TCP;
			*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
		}
		break;
	case IPPROTO_UDP:
	{
#ifdef EM_TIMESYNC
		void *hdr = (caddr_t) ip + ip_hlen;
		struct udphdr *uh = (struct udphdr *)hdr;

		if (uh->uh_dport == htons(TSYNC_PORT)) {
			*txd_upper |= E1000_TXD_EXTCMD_TSTAMP;
			IOCTL_DEBUGOUT("@@@ Sending Event Packet\n");
		}
#endif
		if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
			/*
			 * Start offset for header checksum calculation.
			 * End offset for header checksum calculation.
			 * Offset of place to put the checksum.
			 */
			TXD->upper_setup.tcp_fields.tucss = hdr_len;
			TXD->upper_setup.tcp_fields.tucse = htole16(0);
			TXD->upper_setup.tcp_fields.tucso =
			    hdr_len + offsetof(struct udphdr, uh_sum);
			*txd_upper |= E1000_TXD_POPTS_TXSM << 8;
		}
		/* Fall Thru */
	}
	default:
		break;
	}

#ifdef EM_TIMESYNC
	/*
	** We might be here just for TIMESYNC
	** which means we don't need the context
	** descriptor.
	*/
	if (!mp->m_pkthdr.csum_flags & CSUM_OFFLOAD)
		return;
#endif
	*txd_lower = E1000_TXD_CMD_DEXT |	/* Extended descr type */
		     E1000_TXD_DTYP_D;		/* Data descr */
	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length =
	    htole32(adapter->txd_cmd | E1000_TXD_CMD_DEXT | cmd);
	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++curr_txd == adapter->num_tx_desc)
		curr_txd = 0;

	adapter->num_tx_desc_avail--;
	adapter->next_avail_tx_desc = curr_txd;
}


#if __FreeBSD_version >= 700000
/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO)
 *
 **********************************************************************/
static bool
em_tso_setup(struct adapter *adapter, struct mbuf *mp, u32 *txd_upper,
   u32 *txd_lower)
{
	struct e1000_context_desc *TXD;
	struct em_buffer *tx_buffer;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	struct tcphdr *th;
	int curr_txd, ehdrlen, hdr_len, ip_hlen, isip6;
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

	curr_txd = adapter->next_avail_tx_desc;
	tx_buffer = &adapter->tx_buffer_area[curr_txd];
	TXD = (struct e1000_context_desc *) &adapter->tx_desc_base[curr_txd];

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
				(isip6 ? 0 : E1000_TXD_CMD_IP) | /* Do IP csum */
				E1000_TXD_CMD_TCP |	/* Do TCP checksum */
				(mp->m_pkthdr.len - (hdr_len))); /* Total len */

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++curr_txd == adapter->num_tx_desc)
		curr_txd = 0;

	adapter->num_tx_desc_avail--;
	adapter->next_avail_tx_desc = curr_txd;
	adapter->tx_tso = TRUE;

	return TRUE;
}

#endif /* __FreeBSD_version >= 700000 */

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static void
em_txeof(struct adapter *adapter)
{
        int first, last, done, num_avail;
        struct em_buffer *tx_buffer;
        struct e1000_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = adapter->ifp;

	EM_TX_LOCK_ASSERT(adapter);

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
                	num_avail++;

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

        /*
         * If we have enough room, clear IFF_DRV_OACTIVE to tell the stack
         * that it is OK to send packets.
         * If there are no pending descriptors, clear the timeout. Otherwise,
         * if some descriptors have been freed, restart the timeout.
         */
        if (num_avail > EM_TX_CLEANUP_THRESHOLD) {                
                ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		/* All clean, turn off the timer */
                if (num_avail == adapter->num_tx_desc) {
			adapter->watchdog_timer = 0;
		} else
		/* Some cleaned, reset the timer */
                if (num_avail != adapter->num_tx_desc_avail)
			adapter->watchdog_timer = EM_TX_TIMEOUT;
        }
        adapter->num_tx_desc_avail = num_avail;
	return;
}

/*********************************************************************
 *
 *  When Link is lost sometimes there is work still in the TX ring
 *  which will result in a watchdog, rather than allow that do an
 *  attempted cleanup and then reinit here. Note that this has been
 *  seens mostly with fiber adapters.
 *
 **********************************************************************/
static void
em_tx_purge(struct adapter *adapter)
{
	if ((!adapter->link_active) && (adapter->watchdog_timer)) {
		EM_TX_LOCK(adapter);
		em_txeof(adapter);
		EM_TX_UNLOCK(adapter);
		if (adapter->watchdog_timer) { /* Still not clean? */
			adapter->watchdog_timer = 0;
			em_init_locked(adapter);
		}
	}
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_buf(struct adapter *adapter, int i)
{
	struct mbuf		*m;
	bus_dma_segment_t	segs[1];
	bus_dmamap_t		map;
	struct em_buffer	*rx_buffer;
	int			error, nsegs;

	m = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
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
em_allocate_receive_structures(struct adapter *adapter)
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

#if __FreeBSD_version >= 700000
	error = bus_dma_tag_create(bus_get_dma_tag(dev), /* parent */
#else
	error = bus_dma_tag_create(NULL,		 /* parent */
#endif
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
	error = bus_dmamap_create(adapter->rxtag, BUS_DMA_NOWAIT,
	     &adapter->rx_sparemap);
	if (error) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
		    __func__, error);
		goto fail;
	}

	rx_buffer = adapter->rx_buffer_area;
	for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(adapter->rxtag, BUS_DMA_NOWAIT,
		    &rx_buffer->map);
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
 *  (Re)initialize receive structures.
 *
 **********************************************************************/
static int
em_setup_receive_structures(struct adapter *adapter)
{
	struct em_buffer *rx_buffer;
	int i, error;

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
		error = em_get_buf(adapter, i);
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
#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR	     1000000000/(MAX_INTS_PER_SEC * 256)

static void
em_initialize_receive_unit(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	u64	bus_addr;
	u32	rctl, rxcsum;

	INIT_DEBUGOUT("em_initialize_receive_unit: begin");

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

	/*
	** When using MSIX interrupts we need to throttle
	** using the EITR register (82574 only)
	*/
	if (adapter->msix)
		for (int i = 0; i < 4; i++)
			E1000_WRITE_REG(&adapter->hw,
			    E1000_EITR_82574(i), DEFAULT_ITR);

	/* Disable accelerated ackknowledge */
	if (adapter->hw.mac.type == e1000_82574)
		E1000_WRITE_REG(&adapter->hw,
		    E1000_RFCTL, E1000_RFCTL_ACK_DIS);

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

	/*
	** XXX TEMPORARY WORKAROUND: on some systems with 82573
	** long latencies are observed, like Lenovo X60. This
	** change eliminates the problem, but since having positive
	** values in RDTR is a known source of problems on other
	** platforms another solution is being sought.
	*/
	if (adapter->hw.mac.type == e1000_82573)
		E1000_WRITE_REG(&adapter->hw, E1000_RDTR, 0x20);

	/* Enable Receives */
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl);

	/*
	 * Setup the HW Rx Head and
	 * Tail Descriptor Pointers
	 */
	E1000_WRITE_REG(&adapter->hw, E1000_RDH(0), 0);
	E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), adapter->num_rx_desc - 1);

	return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
em_free_receive_structures(struct adapter *adapter)
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
 *********************************************************************/
static int
em_rxeof(struct adapter *adapter, int count)
{
	struct ifnet	*ifp = adapter->ifp;;
	struct mbuf	*mp;
	u8		status, accept_frame = 0, eop = 0;
	u16 		len, desc_len, prev_len_adj;
	int		i;
	struct e1000_rx_desc   *current_desc;

	EM_RX_LOCK(adapter);
	i = adapter->next_rx_desc_to_check;
	current_desc = &adapter->rx_desc_base[i];
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	if (!((current_desc->status) & E1000_RXD_STAT_DD)) {
		EM_RX_UNLOCK(adapter);
		return (0);
	}

	while ((current_desc->status & E1000_RXD_STAT_DD) &&
	    (count != 0) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		struct mbuf *m = NULL;

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
		status = current_desc->status;
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
			if (em_get_buf(adapter, i) != 0) {
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
				em_receive_checksum(adapter, current_desc,
				    adapter->fmp);
#ifndef __NO_STRICT_ALIGNMENT
				if (adapter->max_frame_size >
				    (MCLBYTES - ETHER_ALIGN) &&
				    em_fixup_rx(adapter) != 0)
					goto skip;
#endif
				if (status & E1000_RXD_STAT_VP) {
#if __FreeBSD_version < 700000
					VLAN_INPUT_TAG_NEW(ifp, adapter->fmp,
					    (le16toh(current_desc->special) &
					    E1000_RXD_SPC_VLAN_MASK));
#else
					adapter->fmp->m_pkthdr.ether_vtag =
					    (le16toh(current_desc->special) &
					    E1000_RXD_SPC_VLAN_MASK);
					adapter->fmp->m_flags |= M_VLANTAG;
#endif
				}
#ifndef __NO_STRICT_ALIGNMENT
skip:
#endif
				m = adapter->fmp;
				adapter->fmp = NULL;
				adapter->lmp = NULL;
			}
		} else {
			ifp->if_ierrors++;
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

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;
		if (m != NULL) {
			adapter->next_rx_desc_to_check = i;
			/* Unlock for call into stack */
			EM_RX_UNLOCK(adapter);
			(*ifp->if_input)(ifp, m);
			EM_RX_LOCK(adapter);
			i = adapter->next_rx_desc_to_check;
		}
		current_desc = &adapter->rx_desc_base[i];
	}
	adapter->next_rx_desc_to_check = i;

	/* Advance the E1000's Receive Queue #0  "Tail Pointer". */
	if (--i < 0)
		i = adapter->num_rx_desc - 1;
	E1000_WRITE_REG(&adapter->hw, E1000_RDT(0), i);
	EM_RX_UNLOCK(adapter);
	if (!((current_desc->status) & E1000_RXD_STAT_DD))
		return (0);

	return (1);
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
em_fixup_rx(struct adapter *adapter)
{
	struct mbuf *m, *n;
	int error;

	error = 0;
	m = adapter->fmp;
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
em_receive_checksum(struct adapter *adapter,
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


#ifdef EM_HW_VLAN_SUPPORT
/*
 * This routine is run via an vlan
 * config EVENT
 */
static void
em_register_vlan(void *unused, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		ctrl, rctl, index, vfta;

	ctrl = E1000_READ_REG(&adapter->hw, E1000_CTRL);
	ctrl |= E1000_CTRL_VME;
	E1000_WRITE_REG(&adapter->hw, E1000_CTRL, ctrl);

	/* Setup for Hardware Filter */
	rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
	rctl |= E1000_RCTL_VFE;
	rctl &= ~E1000_RCTL_CFIEN;
	E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl);

	/* Make entry in the hardware filter table */
	index = ((vtag >> 5) & 0x7F);
	vfta = E1000_READ_REG_ARRAY(&adapter->hw, E1000_VFTA, index);
	vfta |= (1 << (vtag & 0x1F));
	E1000_WRITE_REG_ARRAY(&adapter->hw, E1000_VFTA, index, vfta);

	/* Update the frame size */
	E1000_WRITE_REG(&adapter->hw, E1000_RLPML,
	    adapter->max_frame_size + VLAN_TAG_SIZE);

}

/*
 * This routine is run via an vlan
 * unconfig EVENT
 */
static void
em_unregister_vlan(void *unused, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u32		index, vfta;

	/* Remove entry in the hardware filter table */
	index = ((vtag >> 5) & 0x7F);
	vfta = E1000_READ_REG_ARRAY(&adapter->hw, E1000_VFTA, index);
	vfta &= ~(1 << (vtag & 0x1F));
	E1000_WRITE_REG_ARRAY(&adapter->hw, E1000_VFTA, index, vfta);
	/* Have all vlans unregistered? */
	if (adapter->ifp->if_vlantrunk == NULL) {
		u32 rctl;
		/* Turn off the filter table */
		rctl = E1000_READ_REG(&adapter->hw, E1000_RCTL);
		rctl &= ~E1000_RCTL_VFE;
		rctl |= E1000_RCTL_CFIEN;
		E1000_WRITE_REG(&adapter->hw, E1000_RCTL, rctl);
		/* Reset the frame size */
		E1000_WRITE_REG(&adapter->hw, E1000_RLPML,
		    adapter->max_frame_size);
	}
}
#endif /* EM_HW_VLAN_SUPPORT */

static void
em_enable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;
	u32 ims_mask = IMS_ENABLE_MASK;

	if (adapter->msix) {
		E1000_WRITE_REG(hw, EM_EIAC, EM_MSIX_MASK);
		ims_mask |= EM_MSIX_MASK;
	} 
	E1000_WRITE_REG(hw, E1000_IMS, ims_mask);
}

static void
em_disable_intr(struct adapter *adapter)
{
	struct e1000_hw *hw = &adapter->hw;

	if (adapter->msix)
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
                if (adapter->hw.mac.type >= e1000_82571) {
			manc |= E1000_MANC_EN_MNG2HOST;
#define E1000_MNG2HOST_PORT_623 (1 << 5)
#define E1000_MNG2HOST_PORT_664 (1 << 6)
			manc2h |= E1000_MNG2HOST_PORT_623;
			manc2h |= E1000_MNG2HOST_PORT_664;
			E1000_WRITE_REG(&adapter->hw, E1000_MANC2H, manc2h);
		}

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

		if (adapter->hw.mac.type >= e1000_82571)
			manc &= ~E1000_MANC_EN_MNG2HOST;

		E1000_WRITE_REG(&adapter->hw, E1000_MANC, manc);
	}
}

/*
 * em_get_hw_control sets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that
 * the driver is loaded. For AMT version (only with 82573)
 * of the f/w this means that the network i/f is open.
 *
 */
static void
em_get_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext, swsm;

	/* Let firmware know the driver has taken over */
	switch (adapter->hw.mac.type) {
	case e1000_82573:
		swsm = E1000_READ_REG(&adapter->hw, E1000_SWSM);
		E1000_WRITE_REG(&adapter->hw, E1000_SWSM,
		    swsm | E1000_SWSM_DRV_LOAD);
		break;
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
		ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
		E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
		    ctrl_ext | E1000_CTRL_EXT_DRV_LOAD);
		break;
	default:
		break;
	}
}

/*
 * em_release_hw_control resets {CTRL_EXT|FWSM}:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded. For AMT version (only with 82573) i
 * of the f/w this means that the network i/f is closed.
 *
 */
static void
em_release_hw_control(struct adapter *adapter)
{
	u32 ctrl_ext, swsm;

	/* Let firmware taken over control of h/w */
	switch (adapter->hw.mac.type) {
	case e1000_82573:
		swsm = E1000_READ_REG(&adapter->hw, E1000_SWSM);
		E1000_WRITE_REG(&adapter->hw, E1000_SWSM,
		    swsm & ~E1000_SWSM_DRV_LOAD);
		break;
	case e1000_82571:
	case e1000_82572:
	case e1000_80003es2lan:
	case e1000_ich8lan:
	case e1000_ich9lan:
	case e1000_ich10lan:
		ctrl_ext = E1000_READ_REG(&adapter->hw, E1000_CTRL_EXT);
		E1000_WRITE_REG(&adapter->hw, E1000_CTRL_EXT,
		    ctrl_ext & ~E1000_CTRL_EXT_DRV_LOAD);
		break;
	default:
		break;

	}
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
 * Enable PCI Wake On Lan capability
 */
void
em_enable_wakeup(device_t dev)
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
em_fill_descriptors (bus_addr_t address, u32 length,
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
	device_printf(dev, "fifo workaround = %lld, fifo_reset_count = %lld\n",
	    (long long)adapter->tx_fifo_wrk_cnt,
	    (long long)adapter->tx_fifo_reset_cnt);
	device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_TDH(0)),
	    E1000_READ_REG(&adapter->hw, E1000_TDT(0)));
	device_printf(dev, "hw rdh = %d, hw rdt = %d\n",
	    E1000_READ_REG(&adapter->hw, E1000_RDH(0)),
	    E1000_READ_REG(&adapter->hw, E1000_RDT(0)));
	device_printf(dev, "Num Tx descriptors avail = %d\n",
	    adapter->num_tx_desc_avail);
	device_printf(dev, "Tx Descriptors not avail1 = %ld\n",
	    adapter->no_tx_desc_avail1);
	device_printf(dev, "Tx Descriptors not avail2 = %ld\n",
	    adapter->no_tx_desc_avail2);
	device_printf(dev, "Std mbuf failed = %ld\n",
	    adapter->mbuf_alloc_failed);
	device_printf(dev, "Std mbuf cluster failed = %ld\n",
	    adapter->mbuf_cluster_failed);
	device_printf(dev, "Driver dropped packets = %ld\n",
	    adapter->dropped_pkts);
	device_printf(dev, "Driver tx dma failure in encap = %ld\n",
		adapter->no_tx_dma_setup);
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
	device_printf(dev, "RX overruns = %ld\n", adapter->rx_overruns);
	device_printf(dev, "watchdog timeouts = %ld\n",
	    adapter->watchdog_events);
	device_printf(dev, "RX MSIX IRQ = %ld TX MSIX IRQ = %ld"
	    " LINK MSIX IRQ = %ld\n", adapter->rx_irq,
	    adapter->tx_irq , adapter->link_irq);
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

#ifndef EM_LEGACY_IRQ
static void
em_add_rx_process_limit(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}
#endif

#ifdef EM_TIMESYNC
/*
 * Initialize the Time Sync Feature
 */
static int
em_tsync_init(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	u32		tx_ctl, rx_ctl;


	E1000_WRITE_REG(&adapter->hw, E1000_TIMINCA, (1<<24) |
	    20833/PICOSECS_PER_TICK);

	adapter->last_stamp =  E1000_READ_REG(&adapter->hw, E1000_SYSTIML);
	adapter->last_stamp |= (u64)E1000_READ_REG(&adapter->hw,
	    E1000_SYSTIMH) << 32ULL;

	/* Enable the TX side */
	tx_ctl =  E1000_READ_REG(&adapter->hw, E1000_TSYNCTXCTL);
	tx_ctl |= 0x10;
	E1000_WRITE_REG(&adapter->hw, E1000_TSYNCTXCTL, tx_ctl);
	E1000_WRITE_FLUSH(&adapter->hw);

	tx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCTXCTL);
	if ((tx_ctl & 0x10) == 0) {
     		device_printf(dev, "Failed to enable TX timestamping\n");
		return (ENXIO);
	} 

	/* Enable RX */
	rx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCRXCTL);
	rx_ctl |= 0x10; /* Enable the feature */
	rx_ctl |= 0x0a; /* This value turns on Ver 1 and 2 */
	E1000_WRITE_REG(&adapter->hw, E1000_TSYNCRXCTL, rx_ctl);

	/*
	 * Ethertype Stamping (Ethertype = 0x88F7)
	 */
	E1000_WRITE_REG(&adapter->hw, E1000_RXMTRL, htonl(0x440088f7));

	/*
	 * Source Port Queue Filter Setup:
	 *  this is for UDP port filtering 
	 */
	E1000_WRITE_REG(&adapter->hw, E1000_RXUDP, htons(TSYNC_PORT));
	/* Protocol = UDP, enable Timestamp, and filter on source/protocol */

	E1000_WRITE_FLUSH(&adapter->hw);

	rx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCRXCTL);
	if ((rx_ctl & 0x10) == 0) {
     		device_printf(dev, "Failed to enable RX timestamping\n");
		return (ENXIO);
	} 

	device_printf(dev, "IEEE 1588 Precision Time Protocol enabled\n");

	return (0);
}

/*
 * Disable the Time Sync Feature
 */
static void
em_tsync_disable(struct adapter *adapter)
{
	u32		tx_ctl, rx_ctl;
 
	tx_ctl =  E1000_READ_REG(&adapter->hw, E1000_TSYNCTXCTL);
	tx_ctl &= ~0x10;
	E1000_WRITE_REG(&adapter->hw, E1000_TSYNCTXCTL, tx_ctl);
	E1000_WRITE_FLUSH(&adapter->hw);
   
	/* Invalidate TX Timestamp */
	E1000_READ_REG(&adapter->hw, E1000_TXSTMPH);
 
	tx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCTXCTL);
	if (tx_ctl & 0x10)
     		HW_DEBUGOUT("Failed to disable TX timestamping\n");
   
	rx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCRXCTL);
	rx_ctl &= ~0x10;
   
	E1000_WRITE_REG(&adapter->hw, E1000_TSYNCRXCTL, rx_ctl);
	E1000_WRITE_FLUSH(&adapter->hw);
   
	/* Invalidate RX Timestamp */
	E1000_READ_REG(&adapter->hw, E1000_RXSATRH);
 
	rx_ctl = E1000_READ_REG(&adapter->hw, E1000_TSYNCRXCTL);
	if (rx_ctl & 0x10)
		HW_DEBUGOUT("Failed to disable RX timestamping\n");
 
	return;
}
#endif /* EM_TIMESYNC */
