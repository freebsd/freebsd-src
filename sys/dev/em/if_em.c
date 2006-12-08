/**************************************************************************

Copyright (c) 2001-2006, Intel Corporation
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

***************************************************************************/

/*$FreeBSD$*/

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/module.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>

#include <machine/bus.h>
#include <sys/rman.h>
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
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <machine/in_cksum.h>

#include <pci/pcivar.h>
#include <pci/pcireg.h>
#include <dev/em/if_em_hw.h>
#include <dev/em/if_em.h>

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int	em_display_debug_stats = 0;

/*********************************************************************
 *  Driver version
 *********************************************************************/
char em_driver_version[] = "Version - 6.2.9";


/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into em_strings
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
	{ 0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82571EB_QUAD_COPPER_LOWPROFILE,
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
static int	em_ioctl(struct ifnet *, u_long, caddr_t);
static void	em_watchdog(struct ifnet *);
static void	em_init(void *);
static void	em_stop(void *);
static void	em_media_status(struct ifnet *, struct ifmediareq *);
static int	em_media_change(struct ifnet *);
static void	em_identify_hardware(struct adapter *);
static int	em_allocate_pci_resources(struct adapter *);
static int	em_allocate_intr(struct adapter *);
static void	em_free_intr(struct adapter *);
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
static int	em_allocate_receive_structures(struct adapter *);
static int	em_allocate_transmit_structures(struct adapter *);
static void	em_rxeof(struct adapter *, int);
static void	em_receive_checksum(struct adapter *, struct em_rx_desc *,
		    struct mbuf *);
static void	em_transmit_checksum_setup(struct adapter *, struct mbuf *,
		    uint32_t *, uint32_t *);
static void	em_set_promisc(struct adapter *);
static void	em_disable_promisc(struct adapter *);
static void	em_set_multi(struct adapter *);
static void	em_print_hw_stats(struct adapter *);
static void	em_update_link_status(struct adapter *);
static int	em_get_buf(int i, struct adapter *, struct mbuf *);
static void	em_enable_vlans(struct adapter *);
static void	em_disable_vlans(struct adapter *);
static int	em_encap(struct adapter *, struct mbuf **);
static void	em_smartspeed(struct adapter *);
static int	em_82547_fifo_workaround(struct adapter *, int);
static void	em_82547_update_fifo_head(struct adapter *, int);
static int	em_82547_tx_fifo_reset(struct adapter *);
static void	em_82547_move_tail(void *);
static int	em_dma_malloc(struct adapter *, bus_size_t,
		struct em_dma_alloc *, int);
static void	em_dma_free(struct adapter *, struct em_dma_alloc *);
static void	em_print_debug_info(struct adapter *);
static int 	em_is_valid_ether_addr(uint8_t *);
static int	em_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static uint32_t	em_fill_descriptors (bus_addr_t address, uint32_t length,
		    PDESC_ARRAY desc_array);
static int	em_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	em_add_int_delay_sysctl(struct adapter *, const char *,
		const char *, struct em_int_delay_info *, int, int);
static void	em_add_rx_process_limit(struct adapter *, const char *,
		const char *, int *, int);
static void	em_intr(void *);
#ifdef DEVICE_POLLING
static poll_handler_t em_poll;
#endif /* DEVICE_POLLING */

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
DRIVER_MODULE(if_em, pci, em_driver, em_devclass, 0, 0);

/*********************************************************************
 *  Tunable default values.
 *********************************************************************/

#define E1000_TICKS_TO_USECS(ticks)	((1024 * (ticks) + 500) / 1000)
#define E1000_USECS_TO_TICKS(usecs)	((1000 * (usecs) + 512) / 1024)

static int em_tx_int_delay_dflt = E1000_TICKS_TO_USECS(EM_TIDV);
static int em_rx_int_delay_dflt = E1000_TICKS_TO_USECS(EM_RDTR);
static int em_tx_abs_int_delay_dflt = E1000_TICKS_TO_USECS(EM_TADV);
static int em_rx_abs_int_delay_dflt = E1000_TICKS_TO_USECS(EM_RADV);
static int em_rxd = EM_DEFAULT_RXD;
static int em_txd = EM_DEFAULT_TXD;
static int em_smart_pwr_down = FALSE;

TUNABLE_INT("hw.em.tx_int_delay", &em_tx_int_delay_dflt);
TUNABLE_INT("hw.em.rx_int_delay", &em_rx_int_delay_dflt);
TUNABLE_INT("hw.em.tx_abs_int_delay", &em_tx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rx_abs_int_delay", &em_rx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rxd", &em_rxd);
TUNABLE_INT("hw.em.txd", &em_txd);
TUNABLE_INT("hw.em.smart_pwr_down", &em_smart_pwr_down);

/* How many packets rxeof tries to clean at a time */
static int em_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &em_rx_process_limit);

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
em_probe(device_t dev)
{
	char		adapter_name[60];
	uint16_t	pci_vendor_id = 0;
	uint16_t	pci_device_id = 0;
	uint16_t	pci_subvendor_id = 0;
	uint16_t	pci_subdevice_id = 0;
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
em_attach(device_t dev)
{
	struct adapter	*adapter;
	int		tsize, rsize;
	int		error = 0;
	int		s;

	INIT_DEBUGOUT("em_attach: begin");
	s = splimp();

	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;

	/* SYSCTL stuff */
	sysctl_ctx_init(&adapter->sysctl_ctx);
	adapter->sysctl_tree = SYSCTL_ADD_NODE(&adapter->sysctl_ctx,
	    SYSCTL_STATIC_CHILDREN(_hw), OID_AUTO, device_get_nameunit(dev),
	    CTLFLAG_RD, 0, "");
	if (adapter->sysctl_tree == NULL) {
		error = EIO;
		goto err_sysctl;
	}

	SYSCTL_ADD_PROC(&adapter->sysctl_ctx,
	    SYSCTL_CHILDREN(adapter->sysctl_tree),
	    OID_AUTO, "debug_info", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(&adapter->sysctl_ctx,
	    SYSCTL_CHILDREN(adapter->sysctl_tree),
	    OID_AUTO, "stats", CTLTYPE_INT|CTLFLAG_RW, adapter, 0,
	    em_sysctl_stats, "I", "Statistics");

	callout_init(&adapter->timer);
	callout_init(&adapter->tx_fifo_timer);

	/* Determine hardware revision */
	em_identify_hardware(adapter);

	/* Set up some sysctls for the tunable interrupt delays */
	em_add_int_delay_sysctl(adapter, "rx_int_delay",
	    "receive interrupt delay in usecs", &adapter->rx_int_delay,
	    E1000_REG_OFFSET(&adapter->hw, RDTR), em_rx_int_delay_dflt);
	em_add_int_delay_sysctl(adapter, "tx_int_delay",
	    "transmit interrupt delay in usecs", &adapter->tx_int_delay,
	    E1000_REG_OFFSET(&adapter->hw, TIDV), em_tx_int_delay_dflt);
	if (adapter->hw.mac_type >= em_82540) {
		em_add_int_delay_sysctl(adapter, "rx_abs_int_delay",
		    "receive interrupt delay limit in usecs",
		    &adapter->rx_abs_int_delay,
		    E1000_REG_OFFSET(&adapter->hw, RADV),
		    em_rx_abs_int_delay_dflt);
		em_add_int_delay_sysctl(adapter, "tx_abs_int_delay",
		    "transmit interrupt delay limit in usecs",
		    &adapter->tx_abs_int_delay,
		    E1000_REG_OFFSET(&adapter->hw, TADV),
		    em_tx_abs_int_delay_dflt);
	}

	/* Sysctls for limiting the amount of work done in the taskqueue */
	em_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    em_rx_process_limit);

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of EM_DBA_ALIGN.
	 */
	if (((em_txd * sizeof(struct em_tx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac_type >= em_82544 && em_txd > EM_MAX_TXD) ||
	    (adapter->hw.mac_type < em_82544 && em_txd > EM_MAX_TXD_82543) ||
	    (em_txd < EM_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    EM_DEFAULT_TXD, em_txd);
		adapter->num_tx_desc = EM_DEFAULT_TXD;
	} else
		adapter->num_tx_desc = em_txd;
	if (((em_rxd * sizeof(struct em_rx_desc)) % EM_DBA_ALIGN) != 0 ||
	    (adapter->hw.mac_type >= em_82544 && em_rxd > EM_MAX_RXD) ||
	    (adapter->hw.mac_type < em_82544 && em_rxd > EM_MAX_RXD_82543) ||
	    (em_rxd < EM_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    EM_DEFAULT_RXD, em_rxd);
		adapter->num_rx_desc = EM_DEFAULT_RXD;
	} else
		adapter->num_rx_desc = em_rxd;

	adapter->hw.autoneg = DO_AUTO_NEG;
	adapter->hw.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
	adapter->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	adapter->hw.tbi_compatibility_en = TRUE;
	adapter->rx_buffer_len = EM_RXBUFFER_2048;

	adapter->hw.phy_init_script = 1;
	adapter->hw.phy_reset_disable = FALSE;

#ifndef EM_MASTER_SLAVE
	adapter->hw.master_slave = em_ms_hw_default;
#else
	adapter->hw.master_slave = EM_MASTER_SLAVE;
#endif
	/*
	 * Set the max frame size assuming standard ethernet
	 * sized frames.
	 */
	adapter->hw.max_frame_size =
	    ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

	adapter->hw.min_frame_size =
	    MINIMUM_ETHERNET_PACKET_SIZE + ETHER_CRC_LEN;

	/*
	 * This controls when hardware reports transmit completion
	 * status.
	 */
	adapter->hw.report_tx_early = 1;
	if (em_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}
	
	/* Initialize eeprom parameters */
	em_init_eeprom_params(&adapter->hw);

	tsize = roundup2(adapter->num_tx_desc * sizeof(struct em_tx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Transmit Descriptor ring */
	if (em_dma_malloc(adapter, tsize, &adapter->txdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate tx_desc memory\n");
		error = ENOMEM;
		goto err_tx_desc;
	}
	adapter->tx_desc_base = (struct em_tx_desc *)adapter->txdma.dma_vaddr;

	rsize = roundup2(adapter->num_rx_desc * sizeof(struct em_rx_desc),
	    EM_DBA_ALIGN);

	/* Allocate Receive Descriptor ring */
	if (em_dma_malloc(adapter, rsize, &adapter->rxdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate rx_desc memory\n");
		error = ENOMEM;
		goto err_rx_desc;
	}
	adapter->rx_desc_base = (struct em_rx_desc *)adapter->rxdma.dma_vaddr;

	/* Initialize the hardware */
	if (em_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (em_read_mac_addr(&adapter->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_hw_init;
	}

	if (!em_is_valid_ether_addr(adapter->hw.mac_addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_hw_init;
	}
	bcopy(adapter->hw.mac_addr, adapter->interface_data.ac_enaddr,
	      ETHER_ADDR_LEN);

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

	/* Setup OS specific network interface */
	em_setup_interface(dev, adapter);

	em_allocate_intr(adapter);

	/* Initialize statistics */
	em_clear_hw_cntrs(&adapter->hw);
	em_update_stats_counters(adapter);
	adapter->hw.get_link_status = 1;
	em_update_link_status(adapter);

	/* Indicate SOL/IDER usage */
	if (em_check_phy_reset_block(&adapter->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Identify 82544 on PCIX */
	em_get_bus_info(&adapter->hw);
	if (adapter->hw.bus_type == em_bus_type_pcix &&
	    adapter->hw.mac_type == em_82544)
		adapter->pcix_82544 = TRUE;
	else
		adapter->pcix_82544 = FALSE;

	splx(s);
	INIT_DEBUGOUT("em_attach: end");

	return (0);

err_rx_struct:
	em_free_transmit_structures(adapter);
err_hw_init:
err_tx_struct:
	em_dma_free(adapter, &adapter->rxdma);
err_rx_desc:
	em_dma_free(adapter, &adapter->txdma);
err_tx_desc:
err_pci:
	em_free_intr(adapter);
	em_free_pci_resources(adapter);
	sysctl_ctx_free(&adapter->sysctl_ctx);
err_sysctl:
	splx(s);

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
	struct ifnet	*ifp = &adapter->interface_data.ac_if;
	int		s;

	INIT_DEBUGOUT("em_detach: begin");
	s = splimp();

	em_free_intr(adapter);
	adapter->in_detach = 1;
	em_stop(adapter);
	em_phy_hw_reset(&adapter->hw);
	ether_ifdetach(&adapter->interface_data.ac_if, ETHER_BPF_SUPPORTED);

	em_free_pci_resources(adapter);
	bus_generic_detach(dev);

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

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	adapter->sysctl_tree = NULL;
	sysctl_ctx_free(&adapter->sysctl_ctx);

	splx(s);
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
	struct adapter *adapter = device_get_softc(dev);
	em_stop(adapter);
	return (0);
}

/*
 * Suspend/resume device methods.
 */
static int
em_suspend(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	int s;

	s = splimp();
	em_stop(adapter);
	splx(s);

	return bus_generic_suspend(dev);
}

static int
em_resume(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ifnet *ifp = &adapter->interface_data.ac_if;
	int s;

	s = splimp();
	em_init(adapter);
	if ((ifp->if_flags & IFF_UP) &&
	    (ifp->if_flags & IFF_RUNNING))
		em_start(ifp);
	splx(s);

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
em_start(struct ifnet *ifp)
{
	struct adapter	*adapter = ifp->if_softc;
	struct mbuf	*m_head;
	int		s;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) !=
	    IFF_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	s = splimp();
	while (ifp->if_snd.ifq_head != NULL) {

		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 * em_encap() can modify our pointer, and or make it NULL on
		 * failure.  In that event, we can't requeue.
		 */
		if (em_encap(adapter, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_flags |= IFF_OACTIVE;
			IF_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting. */
		ifp->if_timer = EM_TX_TIMEOUT;
	}
	splx(s);
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
	int error = 0;
	int s;

	s = splimp();

	if (adapter->in_detach)
		goto out;

	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFADDR (Get/Set Interface Addr)");
		ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;
		uint16_t eeprom_data = 0;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		switch (adapter->hw.mac_type) {
		case em_82573:
			/*
			 * 82573 only supports jumbo frames
			 * if ASPM is disabled.
			 */
			em_read_eeprom(&adapter->hw,
			    EEPROM_INIT_3GIO_3, 1, &eeprom_data);
			if (eeprom_data & EEPROM_WORD1A_ASPM_MASK) {
				max_frame_size = ETHER_MAX_LEN;
				break;
			}
			/* Allow Jumbo frames - fall thru */
		case em_82571:
		case em_82572:
		case em_80003es2lan:	/* Limit Jumbo Frame size */
			max_frame_size = 9234;
			break;
		case em_ich8lan:
			/* ICH8 does not support jumbo frames */
			max_frame_size = ETHER_MAX_LEN;
			break;
		default:
			max_frame_size = MAX_JUMBO_FRAME_SIZE;
		}
		if (ifr->ifr_mtu > max_frame_size - ETHER_HDR_LEN -
		    ETHER_CRC_LEN) {
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		adapter->hw.max_frame_size =
		ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
		em_init(adapter);
		break;
	    }
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd:\
		    SIOCSIFFLAGS (Set Interface Flags)");
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING)) {
				em_init(adapter);
			}

			em_disable_promisc(adapter);
			em_set_promisc(adapter);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				em_stop(adapter);
			}
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_flags & IFF_RUNNING) {
			em_disable_intr(adapter);
			em_set_multi(adapter);
			if (adapter->hw.mac_type == em_82542_rev2_0) {
				em_initialize_receive_unit(adapter);
			}
#ifdef DEVICE_POLLING
			if (!(ifp->if_ipending & IFF_POLLING))
#endif
				em_enable_intr(adapter);
		}
		break;
	case SIOCSIFMEDIA:
		/* Check SOL/IDER usage */
		if (em_check_phy_reset_block(&adapter->hw)) {
			device_printf(adapter->dev, "Media change is"
			    "blocked due to SOL/IDER session.\n");
			break;
		}
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
		if (mask & IFCAP_HWCSUM) {
			ifp->if_capenable ^= IFCAP_HWCSUM;
			reinit = 1;
		}
		if (reinit && (ifp->if_flags & IFF_RUNNING))
			em_init(adapter);
		break;
	    }
	default:
		IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%x)", (int)command);
		error = EINVAL;
	}

out:
	splx(s);
	return (error);
}

/*********************************************************************
 *  Watchdog entry point
 *
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

static void
em_watchdog(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	/* If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_TXOFF) {
		ifp->if_timer = EM_TX_TIMEOUT;
		return;
	}

	if (em_check_for_link(&adapter->hw) == 0)
		device_printf(adapter->dev, "watchdog timeout -- resetting\n");

	ifp->if_flags &= ~IFF_RUNNING;
	adapter->watchdog_events++;

	em_init(adapter);
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
em_init(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = &adapter->interface_data.ac_if;
	device_t	dev = adapter->dev;
	uint32_t	pba;
	int		s;

	INIT_DEBUGOUT("em_init: begin");

	s = splimp();

	em_stop(adapter);

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
	switch (adapter->hw.mac_type) {
	case em_82547:
	case em_82547_rev_2: /* 82547: Total Packet Buffer is 40K */
		if (adapter->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
		else
			pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		adapter->tx_fifo_head = 0;
		adapter->tx_head_addr = pba << EM_TX_HEAD_ADDR_SHIFT;
		adapter->tx_fifo_size =
		    (E1000_PBA_40K - pba) << EM_PBA_BYTES_SHIFT;
		break;
	/* Total Packet Buffer on these is 48K */
	case em_82571:
	case em_82572:
	case em_80003es2lan:
			pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		break;
	case em_82573: /* 82573: Total Packet Buffer is 32K */
			pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
		break;
	case em_ich8lan:
		pba = E1000_PBA_8K;
		break;
	default:
		/* Devices before 82547 had a Packet Buffer of 64K.   */
		if(adapter->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}

	INIT_DEBUGOUT1("em_init: pba=%dK",pba);
	E1000_WRITE_REG(&adapter->hw, PBA, pba);
	
	/* Get the latest mac address, User can use a LAA */
	bcopy(adapter->interface_data.ac_enaddr, adapter->hw.mac_addr,
              ETHER_ADDR_LEN);

	/* Initialize the hardware */
	if (em_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		splx(s);
		return;
	}
	em_update_link_status(adapter);

	em_enable_vlans(adapter);

	/* Set hardware offload abilities */
	ifp->if_hwassist = 0;
	if (adapter->hw.mac_type >= em_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist |= EM_CHECKSUM_FEATURES;
	}

	/* Prepare transmit descriptors and buffers */
	em_setup_transmit_structures(adapter);
	em_initialize_transmit_unit(adapter);

	/* Setup Multicast table */
	em_set_multi(adapter);

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(adapter)) {
		device_printf(dev, "Could not setup receive structures\n");
		em_stop(adapter);
		splx(s);
		return;
	}
	em_initialize_receive_unit(adapter);

	/* Don't lose promiscuous settings */
	em_set_promisc(adapter);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	callout_reset(&adapter->timer, hz, em_local_timer, adapter);
	em_clear_hw_cntrs(&adapter->hw);

#ifdef DEVICE_POLLING
	/*
	 * Only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_ipending & IFF_POLLING)
		em_disable_intr(adapter);
	else
#endif /* DEVICE_POLLING */
		em_enable_intr(adapter);

	/* Don't reset the phy next time init gets called */
	adapter->hw.phy_reset_disable = TRUE;

	splx(s);
}


#ifdef DEVICE_POLLING
/*********************************************************************
 *
 *  Polling routine
 *
 *********************************************************************/
static void
em_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	uint32_t reg_icr;

	if (cmd == POLL_DEREGISTER) {	/* final call, enable interrupts */
		em_enable_intr(adapter);
		return;
	}

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&adapter->hw, ICR);
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&adapter->timer);
			adapter->hw.get_link_status = 1;
			em_check_for_link(&adapter->hw);
			em_update_link_status(adapter);
			callout_reset(&adapter->timer, hz,
			    em_local_timer, adapter);
		}
	}
	if (!(ifp->if_flags & IFF_RUNNING))
		return;

	em_rxeof(adapter, count);
	em_txeof(adapter);

	if (ifp->if_snd.ifq_head != NULL)
		em_start(ifp);
}
#endif /* DEVICE_POLLING */

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 *********************************************************************/
#define EM_MAX_INTR 10

static void
em_intr(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp;
	uint32_t	reg_icr;
	int		i;

	ifp = &adapter->interface_data.ac_if;

#ifdef DEVICE_POLLING
	if (ifp->if_ipending & IFF_POLLING)
		return;

	if (ether_poll_register(em_poll, ifp)) {
		em_disable_intr(adapter);
		em_poll(ifp, 0, 1);
		return;
	}
#endif /* DEVICE_POLLING */

	reg_icr = E1000_READ_REG(&adapter->hw, ICR);

	if ((reg_icr == 0) || (adapter->hw.mac_type >= em_82571 &&
	    (reg_icr & E1000_ICR_INT_ASSERTED) == 0) ||
	/*
	 * XXX: some laptops trigger several spurious interrupts
	 * on em(4) when in the resume cycle. The ICR register
	 * reports all-ones value in this case. Processing such
	 * interrupts would lead to a freeze. I don't know why.
	 */
	    (reg_icr == 0xffffffff))
		goto leaving;

	for (i = 0;i < EM_MAX_INTR; ++i) {
		if (ifp->if_flags & IFF_RUNNING) {
			em_rxeof(adapter, adapter->rx_process_limit);
			em_txeof(adapter);
		}
		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&adapter->timer);
			adapter->hw.get_link_status = 1;
			em_check_for_link(&adapter->hw);
			em_update_link_status(adapter);
			callout_reset(&adapter->timer, hz,
			    em_local_timer, adapter);
		}

		if (reg_icr & E1000_ICR_RXO)
			adapter->rx_overruns++;
	}

leaving:
	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		em_start(ifp);
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

	em_check_for_link(&adapter->hw);
	em_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if ((adapter->hw.media_type == em_media_type_fiber) ||
	    (adapter->hw.media_type == em_media_type_internal_serdes)) {
		if (adapter->hw.mac_type == em_82545)
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
			ifmr->ifm_active |= IFM_1000_TX;
			break;
		}
		if (adapter->link_duplex == FULL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	}
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

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		adapter->hw.autoneg = DO_AUTO_NEG;
		adapter->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_LX:
	case IFM_1000_SX:
	case IFM_1000_TX:
		adapter->hw.autoneg = DO_AUTO_NEG;
		adapter->hw.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		adapter->hw.autoneg = FALSE;
		adapter->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			adapter->hw.forced_speed_duplex = em_100_full;
		else
			adapter->hw.forced_speed_duplex = em_100_half;
		break;
	case IFM_10_T:
		adapter->hw.autoneg = FALSE;
		adapter->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			adapter->hw.forced_speed_duplex = em_10_full;
		else
			adapter->hw.forced_speed_duplex = em_10_half;
		break;
	default:
		device_printf(adapter->dev, "Unsupported media type\n");
	}

	/* As the speed/duplex settings my have changed we need to
	 * reset the PHY.
	 */
	adapter->hw.phy_reset_disable = FALSE;

	em_init(adapter);

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/
static int
em_encap(struct adapter *adapter, struct mbuf **m_headp)
{
	struct ifnet		*ifp = &adapter->interface_data.ac_if;
	bus_dma_segment_t	segs[EM_MAX_SCATTER];
	bus_dmamap_t		map;
	struct em_buffer	*tx_buffer, *tx_buffer_mapped;
	struct em_tx_desc	*current_tx_desc;
	struct mbuf		*m_head;
	struct ifvlan		*ifv = NULL;
	uint32_t		txd_upper, txd_lower, txd_used, txd_saved;
	int			nsegs, i, j, first, last = 0;
	int			error;

	m_head = *m_headp;
	current_tx_desc = NULL;
	txd_upper = txd_lower = txd_used = txd_saved = 0;

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

	/* Find out if we are in vlan mode. */
	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL &&
	    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
		ifv = m_head->m_pkthdr.rcvif->if_softc;

	/*
	 * When operating in promiscuous mode, hardware encapsulation for
	 * packets is disabled.  This means we have to add the vlan
	 * encapsulation in the driver, since it will have come down from the
	 * VLAN layer with a tag instead of a VLAN header.
	 */
	if (ifv != NULL && adapter->em_insert_vlan_header) {
		struct ether_vlan_header *evl;
		struct ether_header eh;

		m_head = m_pullup(m_head, sizeof(eh));
		if (m_head == NULL) {
			*m_headp = NULL;
			return (ENOBUFS);
		}
		eh = *mtod(m_head, struct ether_header *);

		/*
		 * If it's already tagged don't add a new one.  In 6.x and
		 * later this case is handled by removing the VLAN mtag
		 * once the vlan header is prepended.
		 */
		if (eh.ether_type == htons(ETHERTYPE_VLAN))
			goto tagged;
		M_PREPEND(m_head, sizeof(*evl), M_DONTWAIT);
		if (m_head == NULL) {
			*m_headp = NULL;
			return (ENOBUFS);
		}
		m_head = m_pullup(m_head, sizeof(*evl));
		if (m_head == NULL) {
			*m_headp = NULL;
			return (ENOBUFS);
		}
		evl = mtod(m_head, struct ether_vlan_header *);
		bcopy(&eh, evl, sizeof(*evl));
		evl->evl_proto = evl->evl_encap_proto;
		evl->evl_encap_proto = htons(ETHERTYPE_VLAN);
		evl->evl_tag = htons(ifv->ifv_tag);
tagged:
		ifv = NULL;
		*m_headp = m_head;
	}


	/*
	 * Capture the first descriptor index,
	 * this descriptor will have the index
	 * of the EOP which is the only one that
	 * now gets a DONE bit writeback.
	 */
	first = adapter->next_avail_tx_desc;

        /*
         * Map the packet for DMA.
         */
	tx_buffer = &adapter->tx_buffer_area[first];
	tx_buffer_mapped = tx_buffer;
	map = tx_buffer->map;
	error = bus_dmamap_load_mbuf_sg(adapter->txtag, tx_buffer->map,
	    *m_headp, segs, &nsegs, BUS_DMA_NOWAIT);
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
		m_head = *m_headp;

		/* Try it again */
		error = bus_dmamap_load_mbuf_sg(adapter->txtag, tx_buffer->map,
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

        if (nsegs > (adapter->num_tx_desc_avail - 2)) {
                adapter->no_tx_desc_avail2++;
                error = ENOBUFS;
		goto encap_fail;
        }

	/* Do hardware assists */
	if (ifp->if_hwassist > 0) {
			em_transmit_checksum_setup(adapter,  m_head,
			    &txd_upper, &txd_lower);
	}

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
			uint32_t	array_elements, counter;
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
					error = ENOBUFS;
					goto encap_fail;
				}
				tx_buffer = &adapter->tx_buffer_area[i];
				current_tx_desc = &adapter->tx_desc_base[i];
				current_tx_desc->buffer_addr = htole64(
				    desc_array.descriptor[counter].address);
				current_tx_desc->lower.data = htole32(
				    (adapter->txd_cmd | txd_lower | (uint16_t) 
				    desc_array.descriptor[counter].length));
				current_tx_desc->upper.data =
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
			current_tx_desc = &adapter->tx_desc_base[i];
			seg_addr = htole64(segs[j].ds_addr);
			seg_len  = segs[j].ds_len;
				current_tx_desc->buffer_addr = seg_addr;
				current_tx_desc->lower.data = htole32(
				adapter->txd_cmd | txd_lower | seg_len);
				current_tx_desc->upper.data =
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

	if (ifv != NULL) {
		/* Set the vlan id. */
		current_tx_desc->upper.fields.special =
		    htole16(ifv->ifv_tag);
                /* Tell hardware to add tag */
                current_tx_desc->lower.data |=
		    htole32(E1000_TXD_CMD_VLE);
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
        current_tx_desc->lower.data |=
	    htole32(E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);
	/*
	 * Keep track in the first buffer which
	 * descriptor will be written back
	 */
	tx_buffer = &adapter->tx_buffer_area[first];
	tx_buffer->next_eop = last;

	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	if (adapter->hw.mac_type == em_82547 &&
	    adapter->link_duplex == HALF_DUPLEX)
		em_82547_move_tail(adapter);
	else {
		E1000_WRITE_REG(&adapter->hw, TDT, i);
		if (adapter->hw.mac_type == em_82547)
			em_82547_update_fifo_head(adapter,
			    m_head->m_pkthdr.len);
	}

	return (0);

encap_fail:
	bus_dmamap_unload(adapter->txtag, tx_buffer->map);
	return (error);
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
	uint16_t hw_tdt;
	uint16_t sw_tdt;
	struct em_tx_desc *tx_desc;
	uint16_t length = 0;
	boolean_t eop = 0;
	int s;

	s = splimp();

	hw_tdt = E1000_READ_REG(&adapter->hw, TDT);
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
			E1000_WRITE_REG(&adapter->hw, TDT, hw_tdt);
			em_82547_update_fifo_head(adapter, length);
			length = 0;
		}
	}

	splx(s);
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
	uint32_t tctl;

	if ((E1000_READ_REG(&adapter->hw, TDT) ==
	    E1000_READ_REG(&adapter->hw, TDH)) &&
	    (E1000_READ_REG(&adapter->hw, TDFT) == 
	    E1000_READ_REG(&adapter->hw, TDFH)) &&
	    (E1000_READ_REG(&adapter->hw, TDFTS) ==
	    E1000_READ_REG(&adapter->hw, TDFHS)) &&
	    (E1000_READ_REG(&adapter->hw, TDFPC) == 0)) {
		/* Disable TX unit */
		tctl = E1000_READ_REG(&adapter->hw, TCTL);
		E1000_WRITE_REG(&adapter->hw, TCTL, tctl & ~E1000_TCTL_EN);

		/* Reset FIFO pointers */
		E1000_WRITE_REG(&adapter->hw, TDFT,  adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, TDFH,  adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, TDFTS, adapter->tx_head_addr);
		E1000_WRITE_REG(&adapter->hw, TDFHS, adapter->tx_head_addr);

		/* Re-enable TX unit */
		E1000_WRITE_REG(&adapter->hw, TCTL, tctl);
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
	struct ifnet	*ifp = &adapter->interface_data.ac_if;
	uint32_t	reg_rctl;

	reg_rctl = E1000_READ_REG(&adapter->hw, RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
		/* Disable VLAN stripping in promiscous mode
		 * This enables bridging of vlan tagged frames to occur
		 * and also allows vlan tags to be seen in tcpdump
		 */
		em_disable_vlans(adapter);
		adapter->em_insert_vlan_header = 1;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
		adapter->em_insert_vlan_header = 0;
	} else
		adapter->em_insert_vlan_header = 0;
}

static void
em_disable_promisc(struct adapter *adapter)
{
	uint32_t	reg_rctl;

	reg_rctl = E1000_READ_REG(&adapter->hw, RCTL);

	reg_rctl &=  (~E1000_RCTL_UPE);
	reg_rctl &=  (~E1000_RCTL_MPE);
	E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);

	em_enable_vlans(adapter);
	adapter->em_insert_vlan_header = 0;
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
	struct ifnet	*ifp = &adapter->interface_data.ac_if;
	struct ifmultiaddr *ifma;
	uint32_t reg_rctl = 0;
	uint8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
	int mcnt = 0;

	IOCTL_DEBUGOUT("em_set_multi: begin");

	if (adapter->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&adapter->hw, RCTL);
		if (adapter->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_clear_mwi(&adapter->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
		msec_delay(5);
	}

	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;

		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    &mta[mcnt*ETH_LENGTH_OF_ADDRESS], ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}

	if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = E1000_READ_REG(&adapter->hw, RCTL);
		reg_rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
	} else
		em_mc_addr_list_update(&adapter->hw, mta, mcnt, 0, 1);

	if (adapter->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&adapter->hw, RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
		msec_delay(5);
		if (adapter->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_set_mwi(&adapter->hw);
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
	struct ifnet	*ifp = &adapter->interface_data.ac_if;
	int		s;

	s = splimp();

	em_check_for_link(&adapter->hw);
	em_update_link_status(adapter);
	em_update_stats_counters(adapter);
	if (em_display_debug_stats && ifp->if_flags & IFF_RUNNING)
		em_print_hw_stats(adapter);
	em_smartspeed(adapter);

	callout_reset(&adapter->timer, hz, em_local_timer, adapter);

	splx(s);
}

static void
em_update_link_status(struct adapter *adapter)
{
	struct ifnet *ifp = &adapter->interface_data.ac_if;
	device_t dev = adapter->dev;

	if (E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_LU) {
		if (adapter->link_active == 0) {
			em_get_speed_and_duplex(&adapter->hw, 
			    &adapter->link_speed, &adapter->link_duplex);
			/* Check if we may set SPEED_MODE bit on PCI-E */
			if ((adapter->link_speed == SPEED_1000) &&
			    ((adapter->hw.mac_type == em_82571) ||
			    (adapter->hw.mac_type == em_82572))) {
				int tarc0;

				tarc0 = E1000_READ_REG(&adapter->hw, TARC0);
				tarc0 |= SPEED_MODE_BIT;
				E1000_WRITE_REG(&adapter->hw, TARC0, tarc0);
			}
			if (bootverbose)
				device_printf(dev, "Link is up %d Mbps %s\n",
				    adapter->link_speed,
				    ((adapter->link_duplex == FULL_DUPLEX) ?
				    "Full Duplex" : "Half Duplex"));
			adapter->link_active = 1;
			adapter->smartspeed = 0;
			ifp->if_baudrate = adapter->link_speed * 1000000;
		}
	} else {
		if (adapter->link_active == 1) {
			ifp->if_baudrate = adapter->link_speed = 0;
			adapter->link_duplex = 0;
			if (bootverbose)
				device_printf(dev, "Link is Down\n");
			adapter->link_active = 0;
		}
	}
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

static void
em_stop(void *arg)
{
	struct adapter	*adapter = arg;
	struct ifnet	*ifp = &adapter->interface_data.ac_if;

	INIT_DEBUGOUT("em_stop: begin");

	em_disable_intr(adapter);
	callout_stop(&adapter->timer);
	callout_stop(&adapter->tx_fifo_timer);

	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	em_reset_hw(&adapter->hw);
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
	adapter->hw.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
	if (!((adapter->hw.pci_cmd_word & PCIM_CMD_BUSMASTEREN) &&
	      (adapter->hw.pci_cmd_word & PCIM_CMD_MEMEN))) {
		device_printf(dev, "Memory Access and/or Bus Master bits "
		    "were not set!\n");
		adapter->hw.pci_cmd_word |=
		(PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
		pci_write_config(dev, PCIR_COMMAND,
		    adapter->hw.pci_cmd_word, 2);
	}

	/* Save off the information about this board */
	adapter->hw.vendor_id = pci_get_vendor(dev);
	adapter->hw.device_id = pci_get_device(dev);
	adapter->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	adapter->hw.subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	adapter->hw.subsystem_id = pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Identify the MAC */
	if (em_set_mac_type(&adapter->hw))
		device_printf(dev, "Unknown MAC Type\n");
	
	if (adapter->hw.mac_type == em_82541 ||
	    adapter->hw.mac_type == em_82541_rev_2 ||
	    adapter->hw.mac_type == em_82547 ||
	    adapter->hw.mac_type == em_82547_rev_2)
		adapter->hw.phy_init_script = TRUE;
}

static int
em_allocate_pci_resources(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	int		val, rid;

	rid = PCIR_BAR(0);
	adapter->res_memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (adapter->res_memory == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	adapter->osdep.mem_bus_space_tag =
	    rman_get_bustag(adapter->res_memory);
	adapter->osdep.mem_bus_space_handle =
	    rman_get_bushandle(adapter->res_memory);
	adapter->hw.hw_addr = (uint8_t *)&adapter->osdep.mem_bus_space_handle;

	if (adapter->hw.mac_type > em_82543) {
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
		adapter->res_ioport = bus_alloc_resource_any(dev,
		    SYS_RES_IOPORT, &adapter->io_rid, RF_ACTIVE);
		if (adapter->res_ioport == NULL) {
			device_printf(dev, "Unable to allocate bus resource: "
			    "ioport\n");
			return (ENXIO);
		}
		adapter->hw.io_base = 0;
		adapter->osdep.io_bus_space_tag =
		    rman_get_bustag(adapter->res_ioport);
		adapter->osdep.io_bus_space_handle =
		    rman_get_bushandle(adapter->res_ioport);
	}

	/* For ICH8 we need to find the flash memory. */
	if (adapter->hw.mac_type == em_ich8lan) {
		rid = EM_FLASH;
		adapter->flash_mem = bus_alloc_resource_any(dev,
		    SYS_RES_MEMORY, &rid, RF_ACTIVE);
		adapter->osdep.flash_bus_space_tag =
		    rman_get_bustag(adapter->flash_mem);
		adapter->osdep.flash_bus_space_handle =
		    rman_get_bushandle(adapter->flash_mem);
	}

	rid = 0x0;
	adapter->res_interrupt = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (adapter->res_interrupt == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

	adapter->hw.back = &adapter->osdep;

	return (0);
}

/*********************************************************************
 *
 *  Setup the appropriate Interrupt handlers.
 *
 **********************************************************************/
int
em_allocate_intr(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int error;

	/* Manually turn off all interrupts */
	E1000_WRITE_REG(&adapter->hw, IMC, 0xffffffff);

	if (adapter->int_handler_tag == NULL &&
	    (error = bus_setup_intr(dev, adapter->res_interrupt,
	    INTR_TYPE_NET, em_intr, adapter,
	    &adapter->int_handler_tag)) != 0) {
		device_printf(dev, "Failed to register interrupt handler");
		return (error);
	}

	em_enable_intr(adapter);
	return (0);
}

static void
em_free_intr(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	if (adapter->int_handler_tag != NULL) {
		bus_teardown_intr(dev, adapter->res_interrupt,
			adapter->int_handler_tag);
		adapter->int_handler_tag = NULL;
	}
}

static void
em_free_pci_resources(struct adapter *adapter)
{
	device_t dev = adapter->dev;

	if (adapter->res_interrupt != NULL)
		bus_release_resource(dev, SYS_RES_IRQ,
		    0, adapter->res_interrupt);

	if (adapter->res_memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->res_memory);

	if (adapter->flash_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    EM_FLASH, adapter->flash_mem);

	if (adapter->res_ioport != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT,
		    adapter->io_rid, adapter->res_ioport);
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
em_hardware_init(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	uint16_t rx_buffer_size;

	INIT_DEBUGOUT("em_hardware_init: begin");
	/* Issue a global reset */
	em_reset_hw(&adapter->hw);

	/* When hardware is reset, fifo_head is also reset */
	adapter->tx_fifo_head = 0;

	/* Make sure we have a good EEPROM before we read from it */
	if (em_validate_eeprom_checksum(&adapter->hw) < 0) {
		device_printf(dev, "The EEPROM Checksum Is Not Valid\n");
		return (EIO);
	}

	if (em_read_part_num(&adapter->hw, &(adapter->part_num)) < 0) {
		device_printf(dev, "EEPROM read error "
		    "reading part number\n");
		return (EIO);
	}

	/* Set up smart power down as default off on newer adapters. */
	if (!em_smart_pwr_down && (adapter->hw.mac_type == em_82571 ||
	    adapter->hw.mac_type == em_82572)) {
		uint16_t phy_tmp = 0;

		/* Speed up time to link by disabling smart power down. */
		em_read_phy_reg(&adapter->hw,
		    IGP02E1000_PHY_POWER_MGMT, &phy_tmp);
		phy_tmp &= ~IGP02E1000_PM_SPD;
		em_write_phy_reg(&adapter->hw,
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
	rx_buffer_size = ((E1000_READ_REG(&adapter->hw, PBA) & 0xffff) << 10 );

	adapter->hw.fc_high_water = rx_buffer_size -
	    roundup2(adapter->hw.max_frame_size, 1024);
	adapter->hw.fc_low_water = adapter->hw.fc_high_water - 1500;
	if (adapter->hw.mac_type == em_80003es2lan)
		adapter->hw.fc_pause_time = 0xFFFF;
	else
		adapter->hw.fc_pause_time = 0x1000;
	adapter->hw.fc_send_xon = TRUE;
	adapter->hw.fc = E1000_FC_FULL;

	if (em_init_hw(&adapter->hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed\n");
		return (EIO);
	}

	em_check_for_link(&adapter->hw);

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
	u_char fiber_type = IFM_1000_SX;	/* default type */

	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = &adapter->interface_data.ac_if;
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "em";
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_init =  em_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	ifp->if_watchdog = em_watchdog;
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 1;

	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);

	ifp->if_capabilities = ifp->if_capenable = 0;

	if (adapter->hw.mac_type >= em_82543) {
		ifp->if_capabilities |= IFCAP_HWCSUM;
		ifp->if_capenable |= IFCAP_HWCSUM;
	}

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK,
	    em_media_change, em_media_status);
	if ((adapter->hw.media_type == em_media_type_fiber) ||
	    (adapter->hw.media_type == em_media_type_internal_serdes)) {
		if (adapter->hw.mac_type == em_82545)
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
		if (adapter->hw.phy_type != em_phy_ife) {
			ifmedia_add(&adapter->media,
				IFM_ETHER | IFM_1000_TX | IFM_FDX, 0, NULL);
			ifmedia_add(&adapter->media,
				IFM_ETHER | IFM_1000_TX, 0, NULL);
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
	uint16_t phy_tmp;

	if (adapter->link_active || (adapter->hw.phy_type != em_phy_igp) ||
	    adapter->hw.autoneg == 0 ||
	    (adapter->hw.autoneg_advertised & ADVERTISE_1000_FULL) == 0)
		return;

	if (adapter->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		em_read_phy_reg(&adapter->hw, PHY_1000T_STATUS, &phy_tmp);
		if (!(phy_tmp & SR_1000T_MS_CONFIG_FAULT))
			return;
		em_read_phy_reg(&adapter->hw, PHY_1000T_STATUS, &phy_tmp);
		if (phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			em_read_phy_reg(&adapter->hw, PHY_1000T_CTRL, &phy_tmp);
			if(phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				em_write_phy_reg(&adapter->hw, PHY_1000T_CTRL,
				    phy_tmp);
				adapter->smartspeed++;
				if(adapter->hw.autoneg &&
				   !em_phy_setup_autoneg(&adapter->hw) &&
				   !em_read_phy_reg(&adapter->hw, PHY_CTRL,
				    &phy_tmp)) {
					phy_tmp |= (MII_CR_AUTO_NEG_EN |
						    MII_CR_RESTART_AUTO_NEG);
					em_write_phy_reg(&adapter->hw, PHY_CTRL,
					    phy_tmp);
				}
			}
		}
		return;
	} else if(adapter->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		em_read_phy_reg(&adapter->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		em_write_phy_reg(&adapter->hw, PHY_1000T_CTRL, phy_tmp);
		if(adapter->hw.autoneg &&
		   !em_phy_setup_autoneg(&adapter->hw) &&
		   !em_read_phy_reg(&adapter->hw, PHY_CTRL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			em_write_phy_reg(&adapter->hw, PHY_CTRL, phy_tmp);
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

	error = bus_dma_tag_create(NULL,		/* parent */
				EM_DBA_ALIGN, 0,	/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				size,			/* maxsize */
				1,			/* nsegments */
				size,			/* maxsegsize */
				0,			/* flags */
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
		    "%s: bus_dmamem_alloc(%llu) failed: %d\n",
		    __func__, (unsigned long long)size, error);
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
	int error, i;

	/*
	 * Create DMA tags for tx descriptors
	 */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				EM_TSO_SIZE,		/* maxsize */
				EM_MAX_SCATTER,		/* nsegments */
				PAGE_SIZE,		/* maxsegsize */
				0,			/* flags */
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
	tx_buffer = adapter->tx_buffer_area;
	for (i = 0; i < adapter->num_tx_desc; i++) {
		error = bus_dmamap_create(adapter->txtag, 0, &tx_buffer->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
		tx_buffer++;
	}

	return (0);

fail:
	em_free_transmit_structures(adapter);
	return (error);
}

/*********************************************************************
 *
 *  Initialize transmit structures.
 *
 **********************************************************************/
static void
em_setup_transmit_structures(struct adapter *adapter)
{
	struct em_buffer *tx_buffer;
	int i;

	/* Clear the old ring contents */
	bzero(adapter->tx_desc_base,
	    (sizeof(struct em_tx_desc)) * adapter->num_tx_desc);

	adapter->next_avail_tx_desc = 0;
	adapter->next_tx_to_clean = 0;

	/* Free any existing tx buffers. */
	tx_buffer = adapter->tx_buffer_area;
	for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
		if (tx_buffer->m_head != NULL) {
			bus_dmamap_sync(adapter->txtag, tx_buffer->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->txtag, tx_buffer->map);
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}
	}

	/* Set number of descriptors available */
	adapter->num_tx_desc_avail = adapter->num_tx_desc;

	/* Set checksum context */
	adapter->active_checksum_context = OFFLOAD_NONE;
	bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(struct adapter *adapter)
{
	uint32_t	reg_tctl, reg_tipg = 0;
	uint64_t	bus_addr;

	 INIT_DEBUGOUT("em_initialize_transmit_unit: begin");
	/* Setup the Base and Length of the Tx Descriptor Ring */
	bus_addr = adapter->txdma.dma_paddr;
	E1000_WRITE_REG(&adapter->hw, TDLEN,
	    adapter->num_tx_desc * sizeof(struct em_tx_desc));
	E1000_WRITE_REG(&adapter->hw, TDBAH, (uint32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&adapter->hw, TDBAL, (uint32_t)bus_addr);

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG(&adapter->hw, TDT, 0);
	E1000_WRITE_REG(&adapter->hw, TDH, 0);

	HW_DEBUGOUT2("Base = %x, Length = %x\n",
	    E1000_READ_REG(&adapter->hw, TDBAL),
	    E1000_READ_REG(&adapter->hw, TDLEN));

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (adapter->hw.mac_type) {
	case em_82542_rev2_0:
	case em_82542_rev2_1:
		reg_tipg = DEFAULT_82542_TIPG_IPGT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	case em_80003es2lan:
		reg_tipg = DEFAULT_82543_TIPG_IPGR1;
		reg_tipg |= DEFAULT_80003ES2LAN_TIPG_IPGR2 <<
		    E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		if ((adapter->hw.media_type == em_media_type_fiber) ||
		    (adapter->hw.media_type == em_media_type_internal_serdes))
			reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}

	E1000_WRITE_REG(&adapter->hw, TIPG, reg_tipg);
	E1000_WRITE_REG(&adapter->hw, TIDV, adapter->tx_int_delay.value);
	if(adapter->hw.mac_type >= em_82540)
		E1000_WRITE_REG(&adapter->hw, TADV,
		    adapter->tx_abs_int_delay.value);

	/* Program the Transmit Control Register */
	reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
	if (adapter->hw.mac_type >= em_82571)
		reg_tctl |= E1000_TCTL_MULR;
	if (adapter->link_duplex == FULL_DUPLEX) {
		reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	} else {
		reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	}
	/* This write will effectively turn on the transmit unit. */
	E1000_WRITE_REG(&adapter->hw, TCTL, reg_tctl);

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
	int i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (adapter->tx_buffer_area != NULL) {
		tx_buffer = adapter->tx_buffer_area;
		for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
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
 *  packet of a particular protocol (TCP/UDP). We change the
 *  context only if the protocol type changes.
 *
 **********************************************************************/
static void
em_transmit_checksum_setup(struct adapter *adapter, struct mbuf *mp,
    uint32_t *txd_upper, uint32_t *txd_lower)
{
	struct em_context_desc *TXD;
	struct em_buffer *tx_buffer;
	int curr_txd;

	if (mp->m_pkthdr.csum_flags) {

		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (adapter->active_checksum_context == OFFLOAD_TCP_IP)
				return;
			else
				adapter->active_checksum_context =
				    OFFLOAD_TCP_IP;
		} else if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (adapter->active_checksum_context == OFFLOAD_UDP_IP)
				return;
			else
				adapter->active_checksum_context =
				    OFFLOAD_UDP_IP;
		} else {
			*txd_upper = 0;
			*txd_lower = 0;
			return;
		}
	} else {
		*txd_upper = 0;
		*txd_lower = 0;
		return;
	}

	/* If we reach this point, the checksum offload context
	 * needs to be reset.
	 */
	curr_txd = adapter->next_avail_tx_desc;
	tx_buffer = &adapter->tx_buffer_area[curr_txd];
	TXD = (struct em_context_desc *) &adapter->tx_desc_base[curr_txd];

	TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
	TXD->lower_setup.ip_fields.ipcso =
		ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
	TXD->lower_setup.ip_fields.ipcse =
		htole16(ETHER_HDR_LEN + sizeof(struct ip) - 1);

	TXD->upper_setup.tcp_fields.tucss =
		ETHER_HDR_LEN + sizeof(struct ip);
	TXD->upper_setup.tcp_fields.tucse = htole16(0);

	if (adapter->active_checksum_context == OFFLOAD_TCP_IP) {
		TXD->upper_setup.tcp_fields.tucso =
			ETHER_HDR_LEN + sizeof(struct ip) +
			offsetof(struct tcphdr, th_sum);
	} else if (adapter->active_checksum_context == OFFLOAD_UDP_IP) {
		TXD->upper_setup.tcp_fields.tucso =
			ETHER_HDR_LEN + sizeof(struct ip) +
			offsetof(struct udphdr, uh_sum);
	}

	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length = htole32(adapter->txd_cmd | E1000_TXD_CMD_DEXT);

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
em_txeof(struct adapter *adapter)
{
	int s;
        int first, last, done, num_avail;
        struct em_buffer *tx_buffer;
        struct em_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = &adapter->interface_data.ac_if;

	s = splimp();

        if (adapter->num_tx_desc_avail == adapter->num_tx_desc)
                goto out;

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
	if (++last == adapter->num_tx_desc) last = 0;
	done = last;

        bus_dmamap_sync(adapter->txdma.dma_tag, adapter->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);

        while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
                	tx_desc->upper.data = 0;
                	tx_desc->lower.data = 0;
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
         * If we have enough room, clear IFF_OACTIVE to tell the stack
         * that it is OK to send packets.
         * If there are no pending descriptors, clear the timeout. Otherwise,
         * if some descriptors have been freed, restart the timeout.
         */
        if (num_avail > EM_TX_CLEANUP_THRESHOLD) {                
                ifp->if_flags &= ~IFF_OACTIVE;
		/* All clean, turn off the timer */
                if (num_avail == adapter->num_tx_desc)
			ifp->if_timer = 0;
		/* Some cleaned, reset the timer */
                else if (num_avail != adapter->num_tx_desc_avail)
			ifp->if_timer = EM_TX_TIMEOUT;
        }
        adapter->num_tx_desc_avail = num_avail;
out:
	splx(s);
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_buf(int i, struct adapter *adapter, struct mbuf *mp)
{
	struct ifnet		*ifp = &adapter->interface_data.ac_if;
	bus_dma_segment_t	segs[1];
	struct em_buffer	*rx_buffer;
	int			error, nsegs;

	if (mp == NULL) {
		mp = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (mp == NULL) {
			adapter->mbuf_cluster_failed++;
			return (ENOBUFS);
		}
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
	} else {
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
		mp->m_data = mp->m_ext.ext_buf;
		mp->m_next = NULL;
	}

	if (ifp->if_mtu <= ETHERMTU)
		m_adj(mp, ETHER_ALIGN);

	rx_buffer = &adapter->rx_buffer_area[i];

	/*
	 * Using memory from the mbuf cluster pool, invoke the
	 * bus_dma machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load_mbuf_sg(adapter->rxtag, rx_buffer->map,
	    mp, segs, &nsegs, 0);
	if (error != 0) {
		m_free(mp);
		return (error);
	}
	/* If nsegs is wrong then the stack is corrupt. */
	KASSERT(nsegs == 1, ("Too many segments returned!"));
	rx_buffer->m_head = mp;
	adapter->rx_desc_base[i].buffer_addr = htole64(segs[0].ds_addr);
	bus_dmamap_sync(adapter->rxtag, rx_buffer->map, BUS_DMASYNC_PREREAD);

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
	    adapter->num_rx_desc, M_DEVBUF, M_NOWAIT);
	if (adapter->rx_buffer_area == NULL) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		return (ENOMEM);
	}

	bzero(adapter->rx_buffer_area,
	    sizeof(struct em_buffer) * adapter->num_rx_desc);

	error = bus_dma_tag_create(NULL,		/* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				MCLBYTES,		/* maxsize */
				1,			/* nsegments */
				MCLBYTES,		/* maxsegsize */
				0,			/* flags */
				&adapter->rxtag);
	if (error) {
		device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
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
 *  Allocate and initialize receive structures.
 *
 **********************************************************************/
static int
em_setup_receive_structures(struct adapter *adapter)
{
	struct em_buffer *rx_buffer;
	int i, error;

	bzero(adapter->rx_desc_base,
	    (sizeof(struct em_rx_desc)) * adapter->num_rx_desc);

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
		error = em_get_buf(i, adapter, NULL);
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
em_initialize_receive_unit(struct adapter *adapter)
{
	struct ifnet    *ifp = &adapter->interface_data.ac_if;
	uint64_t	bus_addr;
	uint32_t	reg_rctl;
	uint32_t	reg_rxcsum;

	INIT_DEBUGOUT("em_initialize_receive_unit: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	E1000_WRITE_REG(&adapter->hw, RCTL, 0);

	if(adapter->hw.mac_type >= em_82540) {
		E1000_WRITE_REG(&adapter->hw, RADV,
		    adapter->rx_abs_int_delay.value);
		/*
		 * Set the interrupt throttling rate. Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns)
		 */
#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR	     1000000000/(MAX_INTS_PER_SEC * 256)
		E1000_WRITE_REG(&adapter->hw, ITR, DEFAULT_ITR);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */
	bus_addr = adapter->rxdma.dma_paddr;
	E1000_WRITE_REG(&adapter->hw, RDLEN, adapter->num_rx_desc *
			sizeof(struct em_rx_desc));
	E1000_WRITE_REG(&adapter->hw, RDBAH, (uint32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&adapter->hw, RDBAL, (uint32_t)bus_addr);

	/* Setup the Receive Control Register */
	reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		   E1000_RCTL_RDMTS_HALF |
		   (adapter->hw.mc_filter_type << E1000_RCTL_MO_SHIFT);

	if (adapter->hw.tbi_compatibility_on == TRUE)
		reg_rctl |= E1000_RCTL_SBP;


	switch (adapter->rx_buffer_len) {
	default:
	case EM_RXBUFFER_2048:
		reg_rctl |= E1000_RCTL_SZ_2048;
		break;
	case EM_RXBUFFER_4096:
		reg_rctl |= E1000_RCTL_SZ_4096 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case EM_RXBUFFER_8192:
		reg_rctl |= E1000_RCTL_SZ_8192 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case EM_RXBUFFER_16384:
		reg_rctl |= E1000_RCTL_SZ_16384 |
		    E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	}

	if (ifp->if_mtu > ETHERMTU)
		reg_rctl |= E1000_RCTL_LPE;

	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if ((adapter->hw.mac_type >= em_82543) &&
	    (ifp->if_capenable & IFCAP_RXCSUM)) {
		reg_rxcsum = E1000_READ_REG(&adapter->hw, RXCSUM);
		reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&adapter->hw, RXCSUM, reg_rxcsum);
	}

	/* Enable Receives */
	E1000_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
	/*
	 * Setup the HW Rx Head and
	 * Tail Descriptor Pointers
	 */
	E1000_WRITE_REG(&adapter->hw, RDH, 0);
	E1000_WRITE_REG(&adapter->hw, RDT, adapter->num_rx_desc - 1);

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
static void
em_rxeof(struct adapter *adapter, int count)
{
	struct ifnet	*ifp;
	struct mbuf	*mp;
	struct ether_header *eh;
	uint8_t		accept_frame = 0;
	uint8_t		eop = 0;
	uint16_t 	len, desc_len, prev_len_adj;
	int		i;

	/* Pointer to the receive descriptor being examined. */
	struct em_rx_desc   *current_desc;
	uint8_t		status;

	ifp = &adapter->interface_data.ac_if;
	i = adapter->next_rx_desc_to_check;
	current_desc = &adapter->rx_desc_base[i];
	bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	if (!((current_desc->status) & E1000_RXD_STAT_DD))
		return;

	while ((current_desc->status & E1000_RXD_STAT_DD) &&
	    (count != 0) &&
	    (ifp->if_flags & IFF_RUNNING)) {

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
			uint8_t		last_byte;
			uint32_t	pkt_len = desc_len;

			if (adapter->fmp != NULL)
				pkt_len += adapter->fmp->m_pkthdr.len;

			last_byte = *(mtod(mp, caddr_t) + desc_len - 1);			
			if (TBI_ACCEPT(&adapter->hw, status,
			    current_desc->errors, pkt_len, last_byte)) {
				em_tbi_adjust_stats(&adapter->hw,
				    &adapter->stats, pkt_len,
				    adapter->hw.mac_addr);
				if (len > 0)
					len--;
			} else
				accept_frame = 0;
		}

		if (accept_frame) {
			if (em_get_buf(i, adapter, NULL) == ENOBUFS) {
				adapter->dropped_pkts++;
				em_get_buf(i, adapter, mp);
				if (adapter->fmp != NULL)
					m_freem(adapter->fmp);
				adapter->fmp = NULL;
				adapter->lmp = NULL;
				break;
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

				eh = mtod(adapter->fmp, struct ether_header *);
				/* Remove ethernet header from mbuf */
				m_adj(adapter->fmp, sizeof(struct ether_header));
				em_receive_checksum(adapter, current_desc,
				    adapter->fmp);
				if (status & E1000_RXD_STAT_VP)
					VLAN_INPUT_TAG(eh, adapter->fmp,
					    (le16toh(current_desc->special) &
					    E1000_RXD_SPC_VLAN_MASK));
				else
					ether_input(ifp, eh, adapter->fmp);

				adapter->fmp = NULL;
				adapter->lmp = NULL;
			}
		} else {
			adapter->dropped_pkts++;
			em_get_buf(i, adapter, mp);
			if (adapter->fmp != NULL)
				m_freem(adapter->fmp);
			adapter->fmp = NULL;
			adapter->lmp = NULL;
		}

		/* Zero out the receive descriptors status. */
		current_desc->status = 0;
		bus_dmamap_sync(adapter->rxdma.dma_tag, adapter->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == adapter->num_rx_desc)
			i = 0;
		current_desc = &adapter->rx_desc_base[i];
	}
	adapter->next_rx_desc_to_check = i;

	/* Advance the E1000's Receive Queue #0  "Tail Pointer". */
	if (--i < 0)
		i = adapter->num_rx_desc - 1;
	E1000_WRITE_REG(&adapter->hw, RDT, i);
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
em_receive_checksum(struct adapter *adapter,
	    struct em_rx_desc *rx_desc, struct mbuf *mp)
{
	/* 82543 or newer only */
	if ((adapter->hw.mac_type < em_82543) ||
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


static void
em_enable_vlans(struct adapter *adapter)
{
	uint32_t ctrl;

	E1000_WRITE_REG(&adapter->hw, VET, ETHERTYPE_VLAN);

	ctrl = E1000_READ_REG(&adapter->hw, CTRL);
	ctrl |= E1000_CTRL_VME;
	E1000_WRITE_REG(&adapter->hw, CTRL, ctrl);
}

static void
em_disable_vlans(struct adapter *adapter)
{
	uint32_t ctrl;

	ctrl = E1000_READ_REG(&adapter->hw, CTRL);
	ctrl &= ~E1000_CTRL_VME;
	E1000_WRITE_REG(&adapter->hw, CTRL, ctrl);
}

static void
em_enable_intr(struct adapter *adapter)
{
	E1000_WRITE_REG(&adapter->hw, IMS, (IMS_ENABLE_MASK));
}

static void
em_disable_intr(struct adapter *adapter)
{
	/*
	 * The first version of 82542 had an errata where when link was forced
	 * it would stay up even up even if the cable was disconnected.
	 * Sequence errors were used to detect the disconnect and then the
	 * driver would unforce the link. This code in the in the ISR. For this
	 * to work correctly the Sequence error interrupt had to be enabled
	 * all the time.
	 */

	if (adapter->hw.mac_type == em_82542_rev2_0)
	    E1000_WRITE_REG(&adapter->hw, IMC,
		(0xffffffff & ~E1000_IMC_RXSEQ));
	else
	    E1000_WRITE_REG(&adapter->hw, IMC, 0xffffffff);
}

static int
em_is_valid_ether_addr(uint8_t *addr)
{
	char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (FALSE);
	}

	return (TRUE);
}

void
em_write_pci_cfg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	pci_write_config(((struct em_osdep *)hw->back)->dev, reg, *value, 2);
}

void
em_read_pci_cfg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	*value = pci_read_config(((struct em_osdep *)hw->back)->dev, reg, 2);
}

void
em_pci_set_mwi(struct em_hw *hw)
{
	pci_write_config(((struct em_osdep *)hw->back)->dev, PCIR_COMMAND,
	    (hw->pci_cmd_word | CMD_MEM_WRT_INVALIDATE), 2);
}

void
em_pci_clear_mwi(struct em_hw *hw)
{
	pci_write_config(((struct em_osdep *)hw->back)->dev, PCIR_COMMAND,
	    (hw->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE), 2);
}

uint32_t 
em_io_read(struct em_hw *hw, unsigned long port)
{
	struct em_osdep *io = hw->back;
	uint32_t ret;

	ret = bus_space_read_4(io->io_bus_space_tag,
            io->io_bus_space_handle, port);
	return (ret);
}

void 
em_io_write(struct em_hw *hw, unsigned long port, uint32_t value)
{
	struct em_osdep *io = hw->back;

	bus_space_write_4(io->io_bus_space_tag,
            io->io_bus_space_handle, port, value);
	return;
}

/*
 * We may eventually really do this, but its unnecessary 
 * for now so we just return unsupported.
 */
int32_t
em_read_pcie_cap_reg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	return (0);
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
static uint32_t
em_fill_descriptors (bus_addr_t address, uint32_t length,
		PDESC_ARRAY desc_array)
{
	/* Since issue is sensitive to length and address.*/
	/* Let us first check the address...*/
	uint32_t safe_terminator;
	if (length <= 4) {
		desc_array->descriptor[0].address = address;
		desc_array->descriptor[0].length = length;
		desc_array->elements = 1;
		return (desc_array->elements);
	}
	safe_terminator = (uint32_t)((((uint32_t)address & 0x7) +
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

	if(adapter->hw.media_type == em_media_type_copper ||
	   (E1000_READ_REG(&adapter->hw, STATUS) & E1000_STATUS_LU)) {
		adapter->stats.symerrs += E1000_READ_REG(&adapter->hw, SYMERRS);
		adapter->stats.sec += E1000_READ_REG(&adapter->hw, SEC);
	}
	adapter->stats.crcerrs += E1000_READ_REG(&adapter->hw, CRCERRS);
	adapter->stats.mpc += E1000_READ_REG(&adapter->hw, MPC);
	adapter->stats.scc += E1000_READ_REG(&adapter->hw, SCC);
	adapter->stats.ecol += E1000_READ_REG(&adapter->hw, ECOL);

	adapter->stats.mcc += E1000_READ_REG(&adapter->hw, MCC);
	adapter->stats.latecol += E1000_READ_REG(&adapter->hw, LATECOL);
	adapter->stats.colc += E1000_READ_REG(&adapter->hw, COLC);
	adapter->stats.dc += E1000_READ_REG(&adapter->hw, DC);
	adapter->stats.rlec += E1000_READ_REG(&adapter->hw, RLEC);
	adapter->stats.xonrxc += E1000_READ_REG(&adapter->hw, XONRXC);
	adapter->stats.xontxc += E1000_READ_REG(&adapter->hw, XONTXC);
	adapter->stats.xoffrxc += E1000_READ_REG(&adapter->hw, XOFFRXC);
	adapter->stats.xofftxc += E1000_READ_REG(&adapter->hw, XOFFTXC);
	adapter->stats.fcruc += E1000_READ_REG(&adapter->hw, FCRUC);
	adapter->stats.prc64 += E1000_READ_REG(&adapter->hw, PRC64);
	adapter->stats.prc127 += E1000_READ_REG(&adapter->hw, PRC127);
	adapter->stats.prc255 += E1000_READ_REG(&adapter->hw, PRC255);
	adapter->stats.prc511 += E1000_READ_REG(&adapter->hw, PRC511);
	adapter->stats.prc1023 += E1000_READ_REG(&adapter->hw, PRC1023);
	adapter->stats.prc1522 += E1000_READ_REG(&adapter->hw, PRC1522);
	adapter->stats.gprc += E1000_READ_REG(&adapter->hw, GPRC);
	adapter->stats.bprc += E1000_READ_REG(&adapter->hw, BPRC);
	adapter->stats.mprc += E1000_READ_REG(&adapter->hw, MPRC);
	adapter->stats.gptc += E1000_READ_REG(&adapter->hw, GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	adapter->stats.gorcl += E1000_READ_REG(&adapter->hw, GORCL);
	adapter->stats.gorch += E1000_READ_REG(&adapter->hw, GORCH);
	adapter->stats.gotcl += E1000_READ_REG(&adapter->hw, GOTCL);
	adapter->stats.gotch += E1000_READ_REG(&adapter->hw, GOTCH);

	adapter->stats.rnbc += E1000_READ_REG(&adapter->hw, RNBC);
	adapter->stats.ruc += E1000_READ_REG(&adapter->hw, RUC);
	adapter->stats.rfc += E1000_READ_REG(&adapter->hw, RFC);
	adapter->stats.roc += E1000_READ_REG(&adapter->hw, ROC);
	adapter->stats.rjc += E1000_READ_REG(&adapter->hw, RJC);

	adapter->stats.torl += E1000_READ_REG(&adapter->hw, TORL);
	adapter->stats.torh += E1000_READ_REG(&adapter->hw, TORH);
	adapter->stats.totl += E1000_READ_REG(&adapter->hw, TOTL);
	adapter->stats.toth += E1000_READ_REG(&adapter->hw, TOTH);

	adapter->stats.tpr += E1000_READ_REG(&adapter->hw, TPR);
	adapter->stats.tpt += E1000_READ_REG(&adapter->hw, TPT);
	adapter->stats.ptc64 += E1000_READ_REG(&adapter->hw, PTC64);
	adapter->stats.ptc127 += E1000_READ_REG(&adapter->hw, PTC127);
	adapter->stats.ptc255 += E1000_READ_REG(&adapter->hw, PTC255);
	adapter->stats.ptc511 += E1000_READ_REG(&adapter->hw, PTC511);
	adapter->stats.ptc1023 += E1000_READ_REG(&adapter->hw, PTC1023);
	adapter->stats.ptc1522 += E1000_READ_REG(&adapter->hw, PTC1522);
	adapter->stats.mptc += E1000_READ_REG(&adapter->hw, MPTC);
	adapter->stats.bptc += E1000_READ_REG(&adapter->hw, BPTC);

	if (adapter->hw.mac_type >= em_82543) {
		adapter->stats.algnerrc += 
		E1000_READ_REG(&adapter->hw, ALGNERRC);
		adapter->stats.rxerrc += 
		E1000_READ_REG(&adapter->hw, RXERRC);
		adapter->stats.tncrs += 
		E1000_READ_REG(&adapter->hw, TNCRS);
		adapter->stats.cexterr += 
		E1000_READ_REG(&adapter->hw, CEXTERR);
		adapter->stats.tsctc += 
		E1000_READ_REG(&adapter->hw, TSCTC);
		adapter->stats.tsctfc += 
		E1000_READ_REG(&adapter->hw, TSCTFC);
	}
	ifp = &adapter->interface_data.ac_if;

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
	uint8_t *hw_addr = adapter->hw.hw_addr;

	device_printf(dev, "Adapter hardware address = %p \n", hw_addr);
	device_printf(dev, "CTRL = 0x%x RCTL = 0x%x \n",
	    E1000_READ_REG(&adapter->hw, CTRL),
	    E1000_READ_REG(&adapter->hw, RCTL));
	device_printf(dev, "Packet buffer = Tx=%dk Rx=%dk \n",
	    ((E1000_READ_REG(&adapter->hw, PBA) & 0xffff0000) >> 16),\
	    (E1000_READ_REG(&adapter->hw, PBA) & 0xffff) );
	device_printf(dev, "Flow control watermarks high = %d low = %d\n",
	    adapter->hw.fc_high_water,
	    adapter->hw.fc_low_water);
	device_printf(dev, "tx_int_delay = %d, tx_abs_int_delay = %d\n",
	    E1000_READ_REG(&adapter->hw, TIDV),
	    E1000_READ_REG(&adapter->hw, TADV));
	device_printf(dev, "rx_int_delay = %d, rx_abs_int_delay = %d\n",
	    E1000_READ_REG(&adapter->hw, RDTR),
	    E1000_READ_REG(&adapter->hw, RADV));
	device_printf(dev, "fifo workaround = %lld, fifo_reset_count = %lld\n",
	    (long long)adapter->tx_fifo_wrk_cnt,
	    (long long)adapter->tx_fifo_reset_cnt);
	device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
	    E1000_READ_REG(&adapter->hw, TDH),
	    E1000_READ_REG(&adapter->hw, TDT));
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
	device_printf(dev, "Carrier extension errors = %lld\n",
	    (long long)adapter->stats.cexterr);
	device_printf(dev, "RX overruns = %ld\n", adapter->rx_overruns);
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
	uint32_t regval;
	int error;
	int usecs;
	int ticks;
	int s;

	info = (struct em_int_delay_info *)arg1;
	usecs = info->value;
	error = sysctl_handle_int(oidp, &usecs, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (usecs < 0 || usecs > E1000_TICKS_TO_USECS(65535))
		return (EINVAL);
	info->value = usecs;
	ticks = E1000_USECS_TO_TICKS(usecs);

	adapter = info->adapter;
	
	s = splimp();
	regval = E1000_READ_OFFSET(&adapter->hw, info->offset);
	regval = (regval & ~0xffff) | (ticks & 0xffff);
	/* Handle a few special cases. */
	switch (info->offset) {
	case E1000_RDTR:
	case E1000_82542_RDTR:
		regval |= E1000_RDT_FPDB;
		break;
	case E1000_TIDV:
	case E1000_82542_TIDV:
		if (ticks == 0) {
			adapter->txd_cmd &= ~E1000_TXD_CMD_IDE;
			/* Don't write 0 into the TIDV register. */
			regval++;
		} else
			adapter->txd_cmd |= E1000_TXD_CMD_IDE;
		break;
	}
	E1000_WRITE_OFFSET(&adapter->hw, info->offset, regval);
	splx(s);
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
	SYSCTL_ADD_PROC(&adapter->sysctl_ctx,
	    SYSCTL_CHILDREN(adapter->sysctl_tree),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW,
	    info, 0, em_sysctl_int_delay, "I", description);
}

static void
em_add_rx_process_limit(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(&adapter->sysctl_ctx,
	    SYSCTL_CHILDREN(adapter->sysctl_tree),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}
