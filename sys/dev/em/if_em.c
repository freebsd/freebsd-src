/**************************************************************************

Copyright (c) 2001-2005, Intel Corporation
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
#include <netinet/tcp.h>
#include <netinet/udp.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/em/if_em_hw.h>
#include <dev/em/if_em.h>

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int	em_display_debug_stats = 0;

/*********************************************************************
 *  Driver version
 *********************************************************************/

char em_driver_version[] = "Version - 5.1.5";


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

	{ 0x8086, E1000_DEV_ID_82572EI_COPPER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI_FIBER,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI_SERDES,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82572EI,		PCI_ANY_ID, PCI_ANY_ID, 0},

	{ 0x8086, E1000_DEV_ID_82573E,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82573E_IAMT,	PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_82573L,		PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_COPPER_DPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},
	{ 0x8086, E1000_DEV_ID_80003ES2LAN_SERDES_DPT,
						PCI_ANY_ID, PCI_ANY_ID, 0},

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
static void	em_watchdog(struct ifnet *);
static void	em_init(void *);
static void	em_init_locked(struct em_softc *);
static void	em_stop(void *);
static void	em_media_status(struct ifnet *, struct ifmediareq *);
static int	em_media_change(struct ifnet *);
static void	em_identify_hardware(struct em_softc *);
static int	em_allocate_pci_resources(struct em_softc *);
static int	em_allocate_intr(struct em_softc *);
static void	em_free_intr(struct em_softc *);
static void	em_free_pci_resources(struct em_softc *);
static void	em_local_timer(void *);
static int	em_hardware_init(struct em_softc *);
static void	em_setup_interface(device_t, struct em_softc *);
static int	em_setup_transmit_structures(struct em_softc *);
static void	em_initialize_transmit_unit(struct em_softc *);
static int	em_setup_receive_structures(struct em_softc *);
static void	em_initialize_receive_unit(struct em_softc *);
static void	em_enable_intr(struct em_softc *);
static void	em_disable_intr(struct em_softc *);
static void	em_free_transmit_structures(struct em_softc *);
static void	em_free_receive_structures(struct em_softc *);
static void	em_update_stats_counters(struct em_softc *);
static void	em_txeof(struct em_softc *);
static int	em_allocate_receive_structures(struct em_softc *);
static int	em_allocate_transmit_structures(struct em_softc *);
static int	em_rxeof(struct em_softc *, int);
#ifndef __NO_STRICT_ALIGNMENT
static int	em_fixup_rx(struct em_softc *);
#endif
static void	em_receive_checksum(struct em_softc *, struct em_rx_desc *,
		    struct mbuf *);
static void	em_transmit_checksum_setup(struct em_softc *, struct mbuf *,
		    uint32_t *, uint32_t *);
static void	em_set_promisc(struct em_softc *);
static void	em_disable_promisc(struct em_softc *);
static void	em_set_multi(struct em_softc *);
static void	em_print_hw_stats(struct em_softc *);
static void	em_update_link_status(struct em_softc *);
static int	em_get_buf(int i, struct em_softc *, struct mbuf *);
static void	em_enable_vlans(struct em_softc *);
static void	em_disable_vlans(struct em_softc *);
static int	em_encap(struct em_softc *, struct mbuf **);
static void	em_smartspeed(struct em_softc *);
static int	em_82547_fifo_workaround(struct em_softc *, int);
static void	em_82547_update_fifo_head(struct em_softc *, int);
static int	em_82547_tx_fifo_reset(struct em_softc *);
static void	em_82547_move_tail(void *arg);
static void	em_82547_move_tail_locked(struct em_softc *);
static int	em_dma_malloc(struct em_softc *, bus_size_t,
		struct em_dma_alloc *, int);
static void	em_dma_free(struct em_softc *, struct em_dma_alloc *);
static void	em_print_debug_info(struct em_softc *);
static int 	em_is_valid_ether_addr(uint8_t *);
static int	em_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	em_sysctl_debug_info(SYSCTL_HANDLER_ARGS);
static uint32_t	em_fill_descriptors (bus_addr_t address, uint32_t length,
		    PDESC_ARRAY desc_array);
static int	em_sysctl_int_delay(SYSCTL_HANDLER_ARGS);
static void	em_add_int_delay_sysctl(struct em_softc *, const char *,
		const char *, struct em_int_delay_info *, int, int);

/*
 * Fast interrupt handler and legacy ithread/polling modes are
 * mutually exclusive.
 */
#ifdef DEVICE_POLLING
static poll_handler_t em_poll;
static void	em_intr(void *);
#else
static void	em_intr_fast(void *);
static void	em_add_int_process_limit(struct em_softc *, const char *,
		const char *, int *, int);
static void	em_handle_rxtx(void *context, int pending);
static void	em_handle_link(void *context, int pending);
#endif

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
	"em", em_methods, sizeof(struct em_softc),
};

static devclass_t em_devclass;
DRIVER_MODULE(em, pci, em_driver, em_devclass, 0, 0);
MODULE_DEPEND(em, pci, 1, 1, 1);
MODULE_DEPEND(em, ether, 1, 1, 1);

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

TUNABLE_INT("hw.em.tx_int_delay", &em_tx_int_delay_dflt);
TUNABLE_INT("hw.em.rx_int_delay", &em_rx_int_delay_dflt);
TUNABLE_INT("hw.em.tx_abs_int_delay", &em_tx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rx_abs_int_delay", &em_rx_abs_int_delay_dflt);
TUNABLE_INT("hw.em.rxd", &em_rxd);
TUNABLE_INT("hw.em.txd", &em_txd);
#ifndef DEVICE_POLLING
static int em_rx_process_limit = 100;
TUNABLE_INT("hw.em.rx_process_limit", &em_rx_process_limit);
#endif

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
	struct em_softc	*sc;
	int		tsize, rsize;
	int		error = 0;

	INIT_DEBUGOUT("em_attach: begin");

	sc = device_get_softc(dev);
	sc->dev = sc->osdep.dev = dev;
	EM_LOCK_INIT(sc, device_get_nameunit(dev));

	/* SYSCTL stuff */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "debug_info", CTLTYPE_INT|CTLFLAG_RW, sc, 0,
	    em_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
	    OID_AUTO, "stats", CTLTYPE_INT|CTLFLAG_RW, sc, 0,
	    em_sysctl_stats, "I", "Statistics");

	callout_init(&sc->timer, CALLOUT_MPSAFE);
	callout_init(&sc->tx_fifo_timer, CALLOUT_MPSAFE);

	/* Determine hardware revision */
	em_identify_hardware(sc);

	/* Set up some sysctls for the tunable interrupt delays */
	em_add_int_delay_sysctl(sc, "rx_int_delay",
	    "receive interrupt delay in usecs", &sc->rx_int_delay,
	    E1000_REG_OFFSET(&sc->hw, RDTR), em_rx_int_delay_dflt);
	em_add_int_delay_sysctl(sc, "tx_int_delay",
	    "transmit interrupt delay in usecs", &sc->tx_int_delay,
	    E1000_REG_OFFSET(&sc->hw, TIDV), em_tx_int_delay_dflt);
	if (sc->hw.mac_type >= em_82540) {
		em_add_int_delay_sysctl(sc, "rx_abs_int_delay",
		    "receive interrupt delay limit in usecs",
		    &sc->rx_abs_int_delay,
		    E1000_REG_OFFSET(&sc->hw, RADV),
		    em_rx_abs_int_delay_dflt);
		em_add_int_delay_sysctl(sc, "tx_abs_int_delay",
		    "transmit interrupt delay limit in usecs",
		    &sc->tx_abs_int_delay,
		    E1000_REG_OFFSET(&sc->hw, TADV),
		    em_tx_abs_int_delay_dflt);
	}

#ifndef DEVICE_POLLING
	/* Sysctls for limiting the amount of work done in the taskqueue */
	em_add_int_process_limit(sc, "rx_processing_limit",
	    "max number of rx packets to process", &sc->rx_process_limit,
	    em_rx_process_limit);
#endif

	/*
	 * Validate number of transmit and receive descriptors. It
	 * must not exceed hardware maximum, and must be multiple
	 * of E1000_DBA_ALIGN.
	 */
	if (((em_txd * sizeof(struct em_tx_desc)) % E1000_DBA_ALIGN) != 0 ||
	    (sc->hw.mac_type >= em_82544 && em_txd > EM_MAX_TXD) ||
	    (sc->hw.mac_type < em_82544 && em_txd > EM_MAX_TXD_82543) ||
	    (em_txd < EM_MIN_TXD)) {
		device_printf(dev, "Using %d TX descriptors instead of %d!\n",
		    EM_DEFAULT_TXD, em_txd);
		sc->num_tx_desc = EM_DEFAULT_TXD;
	} else
		sc->num_tx_desc = em_txd;
	if (((em_rxd * sizeof(struct em_rx_desc)) % E1000_DBA_ALIGN) != 0 ||
	    (sc->hw.mac_type >= em_82544 && em_rxd > EM_MAX_RXD) ||
	    (sc->hw.mac_type < em_82544 && em_rxd > EM_MAX_RXD_82543) ||
	    (em_rxd < EM_MIN_RXD)) {
		device_printf(dev, "Using %d RX descriptors instead of %d!\n",
		    EM_DEFAULT_RXD, em_rxd);
		sc->num_rx_desc = EM_DEFAULT_RXD;
	} else
		sc->num_rx_desc = em_rxd;

	sc->hw.autoneg = DO_AUTO_NEG;
	sc->hw.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
	sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	sc->hw.tbi_compatibility_en = TRUE;
	sc->rx_buffer_len = EM_RXBUFFER_2048;

	sc->hw.phy_init_script = 1;
	sc->hw.phy_reset_disable = FALSE;

#ifndef EM_MASTER_SLAVE
	sc->hw.master_slave = em_ms_hw_default;
#else
	sc->hw.master_slave = EM_MASTER_SLAVE;
#endif
	/*
	 * Set the max frame size assuming standard ethernet
	 * sized frames.
	 */
	sc->hw.max_frame_size = ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

	sc->hw.min_frame_size = MINIMUM_ETHERNET_PACKET_SIZE + ETHER_CRC_LEN;

	/*
	 * This controls when hardware reports transmit completion
	 * status.
	 */
	sc->hw.report_tx_early = 1;
	if (em_allocate_pci_resources(sc)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}
	
	/* Initialize eeprom parameters */
	em_init_eeprom_params(&sc->hw);

	tsize = roundup2(sc->num_tx_desc * sizeof(struct em_tx_desc),
	    E1000_DBA_ALIGN);

	/* Allocate Transmit Descriptor ring */
	if (em_dma_malloc(sc, tsize, &sc->txdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate tx_desc memory\n");
		error = ENOMEM;
		goto err_tx_desc;
	}
	sc->tx_desc_base = (struct em_tx_desc *)sc->txdma.dma_vaddr;

	rsize = roundup2(sc->num_rx_desc * sizeof(struct em_rx_desc),
	    E1000_DBA_ALIGN);

	/* Allocate Receive Descriptor ring */
	if (em_dma_malloc(sc, rsize, &sc->rxdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate rx_desc memory\n");
		error = ENOMEM;
		goto err_rx_desc;
	}
	sc->rx_desc_base = (struct em_rx_desc *)sc->rxdma.dma_vaddr;

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (em_read_mac_addr(&sc->hw) < 0) {
		device_printf(dev, "EEPROM read error while reading MAC"
		    " address\n");
		error = EIO;
		goto err_hw_init;
	}

	if (!em_is_valid_ether_addr(sc->hw.mac_addr)) {
		device_printf(dev, "Invalid MAC address\n");
		error = EIO;
		goto err_hw_init;
	}

	/* Setup OS specific network interface */
	em_setup_interface(dev, sc);

	em_allocate_intr(sc);

	/* Initialize statistics */
	em_clear_hw_cntrs(&sc->hw);
	em_update_stats_counters(sc);
	sc->hw.get_link_status = 1;
	em_update_link_status(sc);

	/* Indicate SOL/IDER usage */
	if (em_check_phy_reset_block(&sc->hw))
		device_printf(dev,
		    "PHY reset is blocked due to SOL/IDER session.\n");

	/* Identify 82544 on PCIX */
	em_get_bus_info(&sc->hw);
	if(sc->hw.bus_type == em_bus_type_pcix && sc->hw.mac_type == em_82544)
		sc->pcix_82544 = TRUE;
	else
		sc->pcix_82544 = FALSE;

	INIT_DEBUGOUT("em_attach: end");

	return (0);

err_hw_init:
	em_dma_free(sc, &sc->rxdma);
err_rx_desc:
	em_dma_free(sc, &sc->txdma);
err_tx_desc:
err_pci:
	em_free_intr(sc);
	em_free_pci_resources(sc);
	EM_LOCK_DESTROY(sc);

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
	struct em_softc	*sc = device_get_softc(dev);
	struct ifnet	*ifp = sc->ifp;

	INIT_DEBUGOUT("em_detach: begin");

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	em_free_intr(sc);
	EM_LOCK(sc);
	sc->in_detach = 1;
	em_stop(sc);
	em_phy_hw_reset(&sc->hw);
	EM_UNLOCK(sc);
	ether_ifdetach(sc->ifp);

	em_free_pci_resources(sc);
	bus_generic_detach(dev);
	if_free(ifp);

	/* Free Transmit Descriptor ring */
	if (sc->tx_desc_base) {
		em_dma_free(sc, &sc->txdma);
		sc->tx_desc_base = NULL;
	}

	/* Free Receive Descriptor ring */
	if (sc->rx_desc_base) {
		em_dma_free(sc, &sc->rxdma);
		sc->rx_desc_base = NULL;
	}

	EM_LOCK_DESTROY(sc);

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
	struct em_softc *sc = device_get_softc(dev);
	EM_LOCK(sc);
	em_stop(sc);
	EM_UNLOCK(sc);
	return (0);
}

/*
 * Suspend/resume device methods.
 */
static int
em_suspend(device_t dev)
{
	struct em_softc *sc = device_get_softc(dev);

	EM_LOCK(sc);
	em_stop(sc);
	EM_UNLOCK(sc);

	return bus_generic_suspend(dev);
}

static int
em_resume(device_t dev)
{
	struct em_softc *sc = device_get_softc(dev);
	struct ifnet *ifp = sc->ifp;

	EM_LOCK(sc);
	em_init_locked(sc);
	if ((ifp->if_flags & IFF_UP) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING))
		em_start_locked(ifp);
	EM_UNLOCK(sc);

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
	struct em_softc	*sc = ifp->if_softc;
	struct mbuf	*m_head;

	EM_LOCK_ASSERT(sc);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!sc->link_active)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;
		/*
		 * em_encap() can modify our pointer, and or make it NULL on
		 * failure.  In that event, we can't requeue.
		 */
		if (em_encap(sc, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}

		/* Send a copy of the frame to the BPF listener */
		BPF_MTAP(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting. */
		ifp->if_timer = EM_TX_TIMEOUT;
	}
}

static void
em_start(struct ifnet *ifp)
{
	struct em_softc *sc = ifp->if_softc;

	EM_LOCK(sc);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		em_start_locked(ifp);
	EM_UNLOCK(sc);
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
	struct em_softc	*sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int error = 0;

	if (sc->in_detach)
		return (error);

	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
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
				EM_LOCK(sc);
				em_init_locked(sc);
				EM_UNLOCK(sc);
			}
			arp_ifinit(ifp, ifa);
		} else
			error = ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
	    {
		int max_frame_size;
		uint16_t eeprom_data = 0;

		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");

		EM_LOCK(sc);
		switch (sc->hw.mac_type) {
		case em_82573:
			/*
			 * 82573 only supports jumbo frames
			 * if ASPM is disabled.
			 */
			em_read_eeprom(&sc->hw, EEPROM_INIT_3GIO_3, 1,
			    &eeprom_data);
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
		default:
			max_frame_size = MAX_JUMBO_FRAME_SIZE;
		}
		if (ifr->ifr_mtu > max_frame_size - ETHER_HDR_LEN -
		    ETHER_CRC_LEN) {
			EM_UNLOCK(sc);
			error = EINVAL;
			break;
		}

		ifp->if_mtu = ifr->ifr_mtu;
		sc->hw.max_frame_size =
		ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
		em_init_locked(sc);
		EM_UNLOCK(sc);
		break;
	    }
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
		EM_LOCK(sc);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ sc->if_flags) &
				    IFF_PROMISC) {
					em_disable_promisc(sc);
					em_set_promisc(sc);
				}
			} else
				em_init_locked(sc);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				em_stop(sc);
			}
		}
		sc->if_flags = ifp->if_flags;
		EM_UNLOCK(sc);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			EM_LOCK(sc);
			em_disable_intr(sc);
			em_set_multi(sc);
			if (sc->hw.mac_type == em_82542_rev2_0) {
				em_initialize_receive_unit(sc);
			}
#ifdef DEVICE_POLLING
			if (!(ifp->if_capenable & IFCAP_POLLING))
#endif
				em_enable_intr(sc);
			EM_UNLOCK(sc);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
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
				EM_LOCK(sc);
				em_disable_intr(sc);
				ifp->if_capenable |= IFCAP_POLLING;
				EM_UNLOCK(sc);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				EM_LOCK(sc);
				em_enable_intr(sc);
				ifp->if_capenable &= ~IFCAP_POLLING;
				EM_UNLOCK(sc);
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
		if (reinit && (ifp->if_drv_flags & IFF_DRV_RUNNING))
			em_init(sc);
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
 *  Watchdog entry point
 *
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

static void
em_watchdog(struct ifnet *ifp)
{
	struct em_softc *sc = ifp->if_softc;

	EM_LOCK(sc);
	/* If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_TXOFF) {
		ifp->if_timer = EM_TX_TIMEOUT;
		EM_UNLOCK(sc);
		return;
	}

	if (em_check_for_link(&sc->hw) == 0)
		device_printf(sc->dev, "watchdog timeout -- resetting\n");

	ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	sc->watchdog_events++;

	em_init_locked(sc);
	EM_UNLOCK(sc);
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
em_init_locked(struct em_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	device_t	dev = sc->dev;
	uint32_t	pba;

	INIT_DEBUGOUT("em_init: begin");

	EM_LOCK_ASSERT(sc);

	em_stop(sc);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 */
	switch (sc->hw.mac_type) {
	case em_82547:
	case em_82547_rev_2: /* 82547: Total Packet Buffer is 40K */
		if (sc->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
		else
			pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		sc->tx_fifo_head = 0;
		sc->tx_head_addr = pba << EM_TX_HEAD_ADDR_SHIFT;
		sc->tx_fifo_size = (E1000_PBA_40K - pba) << EM_PBA_BYTES_SHIFT;
		break;
	case em_80003es2lan: /* 80003es2lan: Total Packet Buffer is 48K */
	case em_82571: /* 82571: Total Packet Buffer is 48K */
	case em_82572: /* 82572: Total Packet Buffer is 48K */
			pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		break;
	case em_82573: /* 82573: Total Packet Buffer is 32K */
		/* Jumbo frames not supported */
			pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
		break;
	default:
		/* Devices before 82547 had a Packet Buffer of 64K.   */
		if(sc->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}

	INIT_DEBUGOUT1("em_init: pba=%dK",pba);
	E1000_WRITE_REG(&sc->hw, PBA, pba);
	
	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(sc->ifp), sc->hw.mac_addr, ETHER_ADDR_LEN);

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		return;
	}
	em_update_link_status(sc);

	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		em_enable_vlans(sc);

	/* Prepare transmit descriptors and buffers */
	if (em_setup_transmit_structures(sc)) {
		device_printf(dev, "Could not setup transmit structures\n");
		em_stop(sc);
		return;
	}
	em_initialize_transmit_unit(sc);

	/* Setup Multicast table */
	em_set_multi(sc);

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(sc)) {
		device_printf(dev, "Could not setup receive structures\n");
		em_stop(sc);
		return;
	}
	em_initialize_receive_unit(sc);

	/* Don't loose promiscuous settings */
	em_set_promisc(sc);

	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	if (sc->hw.mac_type >= em_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = EM_CHECKSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
	}

	callout_reset(&sc->timer, hz, em_local_timer, sc);
	em_clear_hw_cntrs(&sc->hw);
#ifdef DEVICE_POLLING
	/*
	 * Only enable interrupts if we are not polling, make sure
	 * they are off otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		em_disable_intr(sc);
	else
#endif /* DEVICE_POLLING */
		em_enable_intr(sc);

	/* Don't reset the phy next time init gets called */
	sc->hw.phy_reset_disable = TRUE;
}

static void
em_init(void *arg)
{
	struct em_softc *sc = arg;

	EM_LOCK(sc);
	em_init_locked(sc);
	EM_UNLOCK(sc);
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
	struct em_softc *sc = ifp->if_softc;
	uint32_t reg_icr;

	EM_LOCK(sc);
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0) {
		EM_UNLOCK(sc);
		return;
	}

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&sc->hw, ICR);
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&sc->timer);
			sc->hw.get_link_status = 1;
			em_check_for_link(&sc->hw);
			em_update_link_status(sc);
			callout_reset(&sc->timer, hz, em_local_timer, sc);
		}
	}
	em_rxeof(sc, count);
	em_txeof(sc);

	if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp);
	EM_UNLOCK(sc);
}

/*********************************************************************
 *
 *  Legacy Interrupt Service routine  
 *
 *********************************************************************/
static void
em_intr(void *arg)
{
	struct em_softc	*sc = arg;
	struct ifnet	*ifp;
	uint32_t	reg_icr;

	EM_LOCK(sc);

	ifp = sc->ifp;

	if (ifp->if_capenable & IFCAP_POLLING) {
		EM_UNLOCK(sc);
		return;
	}

	for (;;) {
		reg_icr = E1000_READ_REG(&sc->hw, ICR);
		if (sc->hw.mac_type >= em_82571 &&
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

		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			em_rxeof(sc, -1);
			em_txeof(sc);
		}

		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			callout_stop(&sc->timer);
			sc->hw.get_link_status = 1;
			em_check_for_link(&sc->hw);
			em_update_link_status(sc);
			callout_reset(&sc->timer, hz, em_local_timer, sc);
		}

		if (reg_icr & E1000_ICR_RXO)
			sc->rx_overruns++;
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    !IFQ_DRV_IS_EMPTY(&ifp->if_snd))
		em_start_locked(ifp);

	EM_UNLOCK(sc);
}

#else  /* if not DEVICE_POLLING, then fast interrupt routines only */

static void
em_handle_link(void *context, int pending)
{
	struct em_softc	*sc = context;
	struct ifnet *ifp;

	ifp = sc->ifp;

	EM_LOCK(sc);

	callout_stop(&sc->timer);
	sc->hw.get_link_status = 1;
	em_check_for_link(&sc->hw);
	em_update_link_status(sc);
	callout_reset(&sc->timer, hz, em_local_timer, sc);
	EM_UNLOCK(sc);
}

static void
em_handle_rxtx(void *context, int pending)
{
	struct em_softc	*sc = context;
	struct ifnet	*ifp;

	NET_LOCK_GIANT();
	ifp = sc->ifp;

	/*
	 * TODO:
	 * It should be possible to run the tx clean loop without the lock.
	 */
	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		if (em_rxeof(sc, sc->rx_process_limit) != 0)
			taskqueue_enqueue(sc->tq, &sc->rxtx_task);
		EM_LOCK(sc);
		em_txeof(sc);

		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			em_start_locked(ifp);
		EM_UNLOCK(sc);
	}

	em_enable_intr(sc);
	NET_UNLOCK_GIANT();
}

/*********************************************************************
 *
 *  Fast Interrupt Service routine  
 *
 *********************************************************************/
static void
em_intr_fast(void *arg)
{
	struct em_softc	*sc = arg;
	struct ifnet	*ifp;
	uint32_t	reg_icr;

	ifp = sc->ifp;

	reg_icr = E1000_READ_REG(&sc->hw, ICR);

	/* Hot eject?  */
	if (reg_icr == 0xffffffff)
		return;

	/* Definitely not our interrupt.  */
	if (reg_icr == 0x0)
		return;

	/*
	 * Starting with the 82571 chip, bit 31 should be used to
	 * determine whether the interrupt belongs to us.
	 */
	if (sc->hw.mac_type >= em_82571 &&
	    (reg_icr & E1000_ICR_INT_ASSERTED) == 0)
		return;

	/*
	 * Mask interrupts until the taskqueue is finished running.  This is
	 * cheap, just assume that it is needed.  This also works around the
	 * MSI message reordering errata on certain systems.
	 */
	em_disable_intr(sc);
	taskqueue_enqueue(sc->tq, &sc->rxtx_task);

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC))
		taskqueue_enqueue(taskqueue_fast, &sc->link_task);

	if (reg_icr & E1000_ICR_RXO)
		sc->rx_overruns++;
}
#endif /* ! DEVICE_POLLING */

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
	struct em_softc *sc = ifp->if_softc;

	INIT_DEBUGOUT("em_media_status: begin");

	em_check_for_link(&sc->hw);
	em_update_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->hw.media_type == em_media_type_fiber) {
		ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
	} else {
		switch (sc->link_speed) {
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
		if (sc->link_duplex == FULL_DUPLEX)
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
	struct em_softc *sc = ifp->if_softc;
	struct ifmedia  *ifm = &sc->media;

	INIT_DEBUGOUT("em_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_SX:
	case IFM_1000_T:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		sc->hw.autoneg = FALSE;
		sc->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.forced_speed_duplex = em_100_full;
		else
			sc->hw.forced_speed_duplex = em_100_half;
		break;
	case IFM_10_T:
		sc->hw.autoneg = FALSE;
		sc->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.forced_speed_duplex = em_10_full;
		else
			sc->hw.forced_speed_duplex = em_10_half;
		break;
	default:
		device_printf(sc->dev, "Unsupported media type\n");
	}

	/* As the speed/duplex settings my have changed we need to
	 * reset the PHY.
	 */
	sc->hw.phy_reset_disable = FALSE;

	em_init(sc);

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/
static int
em_encap(struct em_softc *sc, struct mbuf **m_headp)
{
	struct ifnet		*ifp = sc->ifp;
	bus_dma_segment_t	segs[EM_MAX_SCATTER];
	bus_dmamap_t		map;
	struct em_buffer	*tx_buffer, *tx_buffer_last;
	struct em_tx_desc	*current_tx_desc;
	struct mbuf		*m_head;
	struct m_tag		*mtag;
	uint32_t		txd_upper, txd_lower, txd_used, txd_saved;
	int			nsegs, i, j;
	int			error;

	m_head = *m_headp;
	current_tx_desc = NULL;
	txd_used = txd_saved = 0;

	/*
	 * Force a cleanup if number of TX descriptors
	 * available hits the threshold.
	 */
	if (sc->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
		em_txeof(sc);
		if (sc->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
			sc->no_tx_desc_avail1++;
			return (ENOBUFS);
		}
	}

	/*
	 * Map the packet for DMA.
	 */
	tx_buffer = &sc->tx_buffer_area[sc->next_avail_tx_desc];
	tx_buffer_last = tx_buffer;
	map = tx_buffer->map;
	error = bus_dmamap_load_mbuf_sg(sc->txtag, map, m_head, segs, &nsegs,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		sc->no_tx_dma_setup++;
		return (error);
	}
	KASSERT(nsegs != 0, ("em_encap: empty packet"));

	if (nsegs > sc->num_tx_desc_avail) {
		sc->no_tx_desc_avail2++;
		error = ENOBUFS;
		goto encap_fail;
	}

	if (ifp->if_hwassist > 0)
		em_transmit_checksum_setup(sc,  m_head, &txd_upper, &txd_lower);
	else
		txd_upper = txd_lower = 0;

	/* Find out if we are in vlan mode. */
	mtag = VLAN_OUTPUT_TAG(ifp, m_head);

	/*
	 * When operating in promiscuous mode, hardware encapsulation for
	 * packets is disabled.  This means we have to add the vlan
	 * encapsulation in the driver, since it will have come down from the
	 * VLAN layer with a tag instead of a VLAN header.
	 */
	if (mtag != NULL && sc->em_insert_vlan_header) {
		struct ether_vlan_header *evl;
		struct ether_header eh;

		m_head = m_pullup(m_head, sizeof(eh));
		if (m_head == NULL) {
			*m_headp = NULL;
			error = ENOBUFS;
			goto encap_fail;
		}
		eh = *mtod(m_head, struct ether_header *);
		M_PREPEND(m_head, sizeof(*evl), M_DONTWAIT);
		if (m_head == NULL) {
			*m_headp = NULL;
			error = ENOBUFS;
			goto encap_fail;
		}
		m_head = m_pullup(m_head, sizeof(*evl));
		if (m_head == NULL) {
			*m_headp = NULL;
			error = ENOBUFS;
			goto encap_fail;
		}
		evl = mtod(m_head, struct ether_vlan_header *);
		bcopy(&eh, evl, sizeof(*evl));
		evl->evl_proto = evl->evl_encap_proto;
		evl->evl_encap_proto = htons(ETHERTYPE_VLAN);
		evl->evl_tag = htons(VLAN_TAG_VALUE(mtag));
		m_tag_delete(m_head, mtag);
		mtag = NULL;
		*m_headp = m_head;
	}

	i = sc->next_avail_tx_desc;
	if (sc->pcix_82544) {
		txd_saved = i;
		txd_used = 0;
	}
	for (j = 0; j < nsegs; j++) {
		/* If adapter is 82544 and on PCIX bus. */
		if(sc->pcix_82544) {
			DESC_ARRAY	desc_array;
			uint32_t	array_elements, counter;

			/*
			 * Check the Address and Length combination and
			 * split the data accordingly
			 */
			array_elements = em_fill_descriptors(segs[j].ds_addr,
			    segs[j].ds_len, &desc_array);
			for (counter = 0; counter < array_elements; counter++) {
				if (txd_used == sc->num_tx_desc_avail) {
					sc->next_avail_tx_desc = txd_saved;
					sc->no_tx_desc_avail2++;
					error = ENOBUFS;
					goto encap_fail;
				}
				tx_buffer = &sc->tx_buffer_area[i];
				current_tx_desc = &sc->tx_desc_base[i];
				current_tx_desc->buffer_addr = htole64(
					desc_array.descriptor[counter].address);
				current_tx_desc->lower.data = htole32(
					(sc->txd_cmd | txd_lower |
					(uint16_t)desc_array.descriptor[counter].length));
				current_tx_desc->upper.data = htole32((txd_upper));
				if (++i == sc->num_tx_desc)
					i = 0;

				tx_buffer->m_head = NULL;
				txd_used++;
			}
		} else {
			tx_buffer = &sc->tx_buffer_area[i];
			current_tx_desc = &sc->tx_desc_base[i];

			current_tx_desc->buffer_addr = htole64(segs[j].ds_addr);
			current_tx_desc->lower.data = htole32(
				sc->txd_cmd | txd_lower | segs[j].ds_len);
			current_tx_desc->upper.data = htole32(txd_upper);

			if (++i == sc->num_tx_desc)
				i = 0;

			tx_buffer->m_head = NULL;
		}
	}

	sc->next_avail_tx_desc = i;
	if (sc->pcix_82544)
		sc->num_tx_desc_avail -= txd_used;
	else
		sc->num_tx_desc_avail -= nsegs;

	if (mtag != NULL) {
		/* Set the vlan id. */
		current_tx_desc->upper.fields.special =
		    htole16(VLAN_TAG_VALUE(mtag));

		/* Tell hardware to add tag. */
		current_tx_desc->lower.data |= htole32(E1000_TXD_CMD_VLE);
	}

	tx_buffer->m_head = m_head;
	tx_buffer_last->map = tx_buffer->map;
	tx_buffer->map = map;
	bus_dmamap_sync(sc->txtag, map, BUS_DMASYNC_PREWRITE);

	/*
	 * Last Descriptor of Packet needs End Of Packet (EOP).
	 */
	current_tx_desc->lower.data |= htole32(E1000_TXD_CMD_EOP);

	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
	 * that this frame is available to transmit.
	 */
	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	if (sc->hw.mac_type == em_82547 && sc->link_duplex == HALF_DUPLEX)
		em_82547_move_tail_locked(sc);
	else {
		E1000_WRITE_REG(&sc->hw, TDT, i);
		if (sc->hw.mac_type == em_82547)
			em_82547_update_fifo_head(sc, m_head->m_pkthdr.len);
	}

	return (0);

encap_fail:
	bus_dmamap_unload(sc->txtag, map);
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
em_82547_move_tail_locked(struct em_softc *sc)
{
	uint16_t hw_tdt;
	uint16_t sw_tdt;
	struct em_tx_desc *tx_desc;
	uint16_t length = 0;
	boolean_t eop = 0;

	EM_LOCK_ASSERT(sc);

	hw_tdt = E1000_READ_REG(&sc->hw, TDT);
	sw_tdt = sc->next_avail_tx_desc;
	
	while (hw_tdt != sw_tdt) {
		tx_desc = &sc->tx_desc_base[hw_tdt];
		length += tx_desc->lower.flags.length;
		eop = tx_desc->lower.data & E1000_TXD_CMD_EOP;
		if(++hw_tdt == sc->num_tx_desc)
			hw_tdt = 0;

		if (eop) {
			if (em_82547_fifo_workaround(sc, length)) {
				sc->tx_fifo_wrk_cnt++;
				callout_reset(&sc->tx_fifo_timer, 1,
					em_82547_move_tail, sc);
				break;
			}
			E1000_WRITE_REG(&sc->hw, TDT, hw_tdt);
			em_82547_update_fifo_head(sc, length);
			length = 0;
		}
	}	
}

static void
em_82547_move_tail(void *arg)
{
	struct em_softc *sc = arg;

	EM_LOCK(sc);
	em_82547_move_tail_locked(sc);
	EM_UNLOCK(sc);
}

static int
em_82547_fifo_workaround(struct em_softc *sc, int len)
{	
	int fifo_space, fifo_pkt_len;

	fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);

	if (sc->link_duplex == HALF_DUPLEX) {
		fifo_space = sc->tx_fifo_size - sc->tx_fifo_head;

		if (fifo_pkt_len >= (EM_82547_PKT_THRESH + fifo_space)) {
			if (em_82547_tx_fifo_reset(sc))
				return (0);
			else
				return (1);
		}
	}

	return (0);
}

static void
em_82547_update_fifo_head(struct em_softc *sc, int len)
{
	int fifo_pkt_len = roundup2(len + EM_FIFO_HDR, EM_FIFO_HDR);
	
	/* tx_fifo_head is always 16 byte aligned */
	sc->tx_fifo_head += fifo_pkt_len;
	if (sc->tx_fifo_head >= sc->tx_fifo_size) {
		sc->tx_fifo_head -= sc->tx_fifo_size;
	}
}


static int
em_82547_tx_fifo_reset(struct em_softc *sc)
{	
	uint32_t tctl;

	if ((E1000_READ_REG(&sc->hw, TDT) == E1000_READ_REG(&sc->hw, TDH)) &&
	    (E1000_READ_REG(&sc->hw, TDFT) == E1000_READ_REG(&sc->hw, TDFH)) &&
	    (E1000_READ_REG(&sc->hw, TDFTS) == E1000_READ_REG(&sc->hw, TDFHS))&&
	    (E1000_READ_REG(&sc->hw, TDFPC) == 0)) {

		/* Disable TX unit */
		tctl = E1000_READ_REG(&sc->hw, TCTL);
		E1000_WRITE_REG(&sc->hw, TCTL, tctl & ~E1000_TCTL_EN);

		/* Reset FIFO pointers */
		E1000_WRITE_REG(&sc->hw, TDFT,  sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFH,  sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFTS, sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFHS, sc->tx_head_addr);

		/* Re-enable TX unit */
		E1000_WRITE_REG(&sc->hw, TCTL, tctl);
		E1000_WRITE_FLUSH(&sc->hw);

		sc->tx_fifo_head = 0;
		sc->tx_fifo_reset_cnt++;

		return (TRUE);
	}
	else {
		return (FALSE);
	}
}

static void
em_set_promisc(struct em_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	uint32_t	reg_rctl;

	reg_rctl = E1000_READ_REG(&sc->hw, RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		/* Disable VLAN stripping in promiscous mode
		 * This enables bridging of vlan tagged frames to occur
		 * and also allows vlan tags to be seen in tcpdump
		 */
		if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
			em_disable_vlans(sc);
		sc->em_insert_vlan_header = 1;
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		sc->em_insert_vlan_header = 0;
	} else
		sc->em_insert_vlan_header = 0;
}

static void
em_disable_promisc(struct em_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	uint32_t	reg_rctl;

	reg_rctl = E1000_READ_REG(&sc->hw, RCTL);

	reg_rctl &=  (~E1000_RCTL_UPE);
	reg_rctl &=  (~E1000_RCTL_MPE);
	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);

	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		em_enable_vlans(sc);
	sc->em_insert_vlan_header = 0;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
em_set_multi(struct em_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	struct ifmultiaddr *ifma;
	uint32_t reg_rctl = 0;
	uint8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
	int mcnt = 0;

	IOCTL_DEBUGOUT("em_set_multi: begin");

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_clear_mwi(&sc->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
	}

	IF_ADDR_LOCK(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;

		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		    &mta[mcnt*ETH_LENGTH_OF_ADDRESS], ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
	IF_ADDR_UNLOCK(ifp);

	if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		reg_rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
	} else
		em_mc_addr_list_update(&sc->hw, mta, mcnt, 0, 1);

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_set_mwi(&sc->hw);
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
	struct em_softc	*sc = arg;
	struct ifnet	*ifp = sc->ifp;

	EM_LOCK(sc);

	em_check_for_link(&sc->hw);
	em_update_link_status(sc);
	em_update_stats_counters(sc);
	if (em_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING)
		em_print_hw_stats(sc);
	em_smartspeed(sc);

	callout_reset(&sc->timer, hz, em_local_timer, sc);

	EM_UNLOCK(sc);
}

static void
em_update_link_status(struct em_softc *sc)
{
	struct ifnet *ifp = sc->ifp;
	device_t dev = sc->dev;

	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU) {
		if (sc->link_active == 0) {
			em_get_speed_and_duplex(&sc->hw, &sc->link_speed,
			    &sc->link_duplex);
			if (bootverbose)
				device_printf(dev, "Link is up %d Mbps %s\n",
				    sc->link_speed,
				    ((sc->link_duplex == FULL_DUPLEX) ?
				    "Full Duplex" : "Half Duplex"));
			sc->link_active = 1;
			sc->smartspeed = 0;
			ifp->if_baudrate = sc->link_speed * 1000000;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else {
		if (sc->link_active == 1) {
			ifp->if_baudrate = sc->link_speed = 0;
			sc->link_duplex = 0;
			if (bootverbose)
				device_printf(dev, "Link is Down\n");
			sc->link_active = 0;
			if_link_state_change(ifp, LINK_STATE_DOWN);
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
	struct em_softc	*sc = arg;
	struct ifnet	*ifp = sc->ifp;

	EM_LOCK_ASSERT(sc);

	INIT_DEBUGOUT("em_stop: begin");

	em_disable_intr(sc);
	em_reset_hw(&sc->hw);
	callout_stop(&sc->timer);
	callout_stop(&sc->tx_fifo_timer);
	em_free_transmit_structures(sc);
	em_free_receive_structures(sc);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
em_identify_hardware(struct em_softc *sc)
{
	device_t dev = sc->dev;

	/* Make sure our PCI config space has the necessary stuff set */
	sc->hw.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((sc->hw.pci_cmd_word & PCIM_CMD_BUSMASTEREN) == 0 &&
	    (sc->hw.pci_cmd_word & PCIM_CMD_MEMEN)) {
		device_printf(dev, "Memory Access and/or Bus Master bits "
		    "were not set!\n");
		sc->hw.pci_cmd_word |=
		(PCIM_CMD_BUSMASTEREN | PCIM_CMD_MEMEN);
		pci_write_config(dev, PCIR_COMMAND, sc->hw.pci_cmd_word, 2);
	}

	/* Save off the information about this board */
	sc->hw.vendor_id = pci_get_vendor(dev);
	sc->hw.device_id = pci_get_device(dev);
	sc->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	sc->hw.subsystem_vendor_id = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	sc->hw.subsystem_id = pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Identify the MAC */
	if (em_set_mac_type(&sc->hw))
		device_printf(dev, "Unknown MAC Type\n");
	
	if(sc->hw.mac_type == em_82541 || sc->hw.mac_type == em_82541_rev_2 ||
	   sc->hw.mac_type == em_82547 || sc->hw.mac_type == em_82547_rev_2)
		sc->hw.phy_init_script = TRUE;
}

static int
em_allocate_pci_resources(struct em_softc *sc)
{
	device_t	dev = sc->dev;
	int		val, rid;

	rid = PCIR_BAR(0);
	sc->res_memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->res_memory == NULL) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	sc->osdep.mem_bus_space_tag =
	rman_get_bustag(sc->res_memory);
	sc->osdep.mem_bus_space_handle = rman_get_bushandle(sc->res_memory);
	sc->hw.hw_addr = (uint8_t *)&sc->osdep.mem_bus_space_handle;

	if (sc->hw.mac_type > em_82543) {
		/* Figure our where our IO BAR is ? */
		for (rid = PCIR_BAR(0); rid < PCIR_CIS;) {
			val = pci_read_config(dev, rid, 4);
			if (E1000_BAR_TYPE(val) == E1000_BAR_TYPE_IO) {
				sc->io_rid = rid;
				break;
			}
			rid += 4;
			/* check for 64bit BAR */
			if (E1000_BAR_MEM_TYPE(val) == E1000_BAR_MEM_TYPE_64BIT)
				rid += 4;
		}
		if (rid >= PCIR_CIS) {
			device_printf(dev, "Unable to locate IO BAR\n");
			return (ENXIO);
		}
		sc->res_ioport = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &sc->io_rid, RF_ACTIVE);
		if (sc->res_ioport == NULL) {
			device_printf(dev, "Unable to allocate bus resource: "
			    "ioport\n");
			return (ENXIO);
		}
		sc->hw.io_base = 0;
		sc->osdep.io_bus_space_tag = rman_get_bustag(sc->res_ioport);
		sc->osdep.io_bus_space_handle =
		    rman_get_bushandle(sc->res_ioport);
	}

	rid = 0x0;
	sc->res_interrupt = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
	    RF_SHAREABLE | RF_ACTIVE);
	if (sc->res_interrupt == NULL) {
		device_printf(dev, "Unable to allocate bus resource: "
		    "interrupt\n");
		return (ENXIO);
	}

	sc->hw.back = &sc->osdep;

	return (0);
}

int
em_allocate_intr(struct em_softc *sc)
{
	device_t dev = sc->dev;
	int error;

	/* Manually turn off all interrupts */
	E1000_WRITE_REG(&sc->hw, IMC, 0xffffffff);

#ifdef DEVICE_POLLING
	if (sc->int_handler_tag == NULL && (error = bus_setup_intr(dev,
	    sc->res_interrupt, INTR_TYPE_NET | INTR_MPSAFE, em_intr, sc,
	    &sc->int_handler_tag)) != 0) {
		device_printf(dev, "Failed to register interrupt handler");
		return (error);
	}
#else
	/*
	 * Try allocating a fast interrupt and the associated deferred
	 * processing contexts.
	 */
	TASK_INIT(&sc->rxtx_task, 0, em_handle_rxtx, sc);
	TASK_INIT(&sc->link_task, 0, em_handle_link, sc);
	sc->tq = taskqueue_create_fast("em_taskq", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->tq);
	taskqueue_start_threads(&sc->tq, 1, PI_NET, "%s taskq",
	    device_get_nameunit(sc->dev));
	if ((error = bus_setup_intr(dev, sc->res_interrupt,
	    INTR_TYPE_NET | INTR_FAST, em_intr_fast, sc,
	    &sc->int_handler_tag)) != 0) {
		device_printf(dev, "Failed to register fast interrupt "
			    "handler: %d\n", error);
		taskqueue_free(sc->tq);
		sc->tq = NULL;
		return (error);
	}
#endif

	em_enable_intr(sc);
	return (0);
}

static void
em_free_intr(struct em_softc *sc)
{
	device_t dev = sc->dev;

	if (sc->res_interrupt != NULL) {
		bus_teardown_intr(dev, sc->res_interrupt, sc->int_handler_tag);
		sc->int_handler_tag = NULL;
	}
	if (sc->tq != NULL) {
		taskqueue_drain(sc->tq, &sc->rxtx_task);
		taskqueue_drain(taskqueue_fast, &sc->link_task);
		taskqueue_free(sc->tq);
		sc->tq = NULL;
	}
}

static void
em_free_pci_resources(struct em_softc *sc)
{
	device_t dev = sc->dev;

	if (sc->res_interrupt != NULL)
		bus_release_resource(dev, SYS_RES_IRQ, 0, sc->res_interrupt);

	if (sc->res_memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, PCIR_BAR(0),
		    sc->res_memory);

	if (sc->res_ioport != NULL)
		bus_release_resource(dev, SYS_RES_IOPORT, sc->io_rid,
		    sc->res_ioport);
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
em_hardware_init(struct em_softc *sc)
{
	device_t dev = sc->dev;
	uint16_t rx_buffer_size;

	INIT_DEBUGOUT("em_hardware_init: begin");
	/* Issue a global reset */
	em_reset_hw(&sc->hw);

	/* When hardware is reset, fifo_head is also reset */
	sc->tx_fifo_head = 0;

	/* Make sure we have a good EEPROM before we read from it */
	if (em_validate_eeprom_checksum(&sc->hw) < 0) {
		device_printf(dev, "The EEPROM Checksum Is Not Valid\n");
		return (EIO);
	}

	if (em_read_part_num(&sc->hw, &(sc->part_num)) < 0) {
		device_printf(dev, "EEPROM read error while reading part "
		    "number\n");
		return (EIO);
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
	rx_buffer_size = ((E1000_READ_REG(&sc->hw, PBA) & 0xffff) << 10 );

	sc->hw.fc_high_water = rx_buffer_size -
	    roundup2(sc->hw.max_frame_size, 1024);
	sc->hw.fc_low_water = sc->hw.fc_high_water - 1500;
	if (sc->hw.mac_type == em_80003es2lan)
		sc->hw.fc_pause_time = 0xFFFF;
	else
		sc->hw.fc_pause_time = 0x1000;
	sc->hw.fc_send_xon = TRUE;
	sc->hw.fc = em_fc_full;

	if (em_init_hw(&sc->hw) < 0) {
		device_printf(dev, "Hardware Initialization Failed");
		return (EIO);
	}

	em_check_for_link(&sc->hw);

	return (0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
em_setup_interface(device_t dev, struct em_softc *sc)
{
	struct ifnet   *ifp;
	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = sc->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		panic("%s: can not if_alloc()", device_get_nameunit(dev));
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_init =  em_init;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	ifp->if_watchdog = em_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->num_tx_desc - 1);
	ifp->if_snd.ifq_drv_maxlen = sc->num_tx_desc - 1;
	IFQ_SET_READY(&ifp->if_snd);

	ether_ifattach(ifp, sc->hw.mac_addr);

	ifp->if_capabilities = ifp->if_capenable = 0;

	if (sc->hw.mac_type >= em_82543) {
		ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
		ifp->if_capenable |= IFCAP_HWCSUM | IFCAP_VLAN_HWCSUM;
	}

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capenable |= IFCAP_VLAN_MTU;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, em_media_change, em_media_status);
	if (sc->hw.media_type == em_media_type_fiber) {
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_SX | IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_SX,
			    0, NULL);
	} else {
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T | IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX,
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);
}


/*********************************************************************
 *
 *  Workaround for SmartSpeed on 82541 and 82547 controllers
 *
 **********************************************************************/
static void
em_smartspeed(struct em_softc *sc)
{
	uint16_t phy_tmp;

	if (sc->link_active || (sc->hw.phy_type != em_phy_igp) ||
	    sc->hw.autoneg == 0 ||
	    (sc->hw.autoneg_advertised & ADVERTISE_1000_FULL) == 0)
		return;

	if (sc->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (!(phy_tmp & SR_1000T_MS_CONFIG_FAULT))
			return;
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			em_read_phy_reg(&sc->hw, PHY_1000T_CTRL, &phy_tmp);
			if(phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				em_write_phy_reg(&sc->hw, PHY_1000T_CTRL,
				    phy_tmp);
				sc->smartspeed++;
				if(sc->hw.autoneg &&
				   !em_phy_setup_autoneg(&sc->hw) &&
				   !em_read_phy_reg(&sc->hw, PHY_CTRL,
				    &phy_tmp)) {
					phy_tmp |= (MII_CR_AUTO_NEG_EN |
						    MII_CR_RESTART_AUTO_NEG);
					em_write_phy_reg(&sc->hw, PHY_CTRL,
					    phy_tmp);
				}
			}
		}
		return;
	} else if(sc->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		em_read_phy_reg(&sc->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		em_write_phy_reg(&sc->hw, PHY_1000T_CTRL, phy_tmp);
		if(sc->hw.autoneg &&
		   !em_phy_setup_autoneg(&sc->hw) &&
		   !em_read_phy_reg(&sc->hw, PHY_CTRL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			em_write_phy_reg(&sc->hw, PHY_CTRL, phy_tmp);
		}
	}
	/* Restart process after EM_SMARTSPEED_MAX iterations */
	if(sc->smartspeed++ == EM_SMARTSPEED_MAX)
		sc->smartspeed = 0;
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
em_dma_malloc(struct em_softc *sc, bus_size_t size, struct em_dma_alloc *dma,
	int mapflags)
{
	int error;

	error = bus_dma_tag_create(NULL,		/* parent */
				E1000_DBA_ALIGN, 0,	/* alignment, bounds */
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
		device_printf(sc->dev, "%s: bus_dma_tag_create failed: %d\n",
		    __func__, error);
		goto fail_0;
	}

	error = bus_dmamem_alloc(dma->dma_tag, (void**) &dma->dma_vaddr,
	    BUS_DMA_NOWAIT, &dma->dma_map);
	if (error) {
		device_printf(sc->dev, "%s: bus_dmamem_alloc(%ju) failed: %d\n",
		    __func__, (uintmax_t)size, error);
		goto fail_2;
	}

	dma->dma_paddr = 0;
	error = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
	    size, em_dmamap_cb, &dma->dma_paddr, mapflags | BUS_DMA_NOWAIT);
	if (error || dma->dma_paddr == 0) {
		device_printf(sc->dev, "%s: bus_dmamap_load failed: %d\n",
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
em_dma_free(struct em_softc *sc, struct em_dma_alloc *dma)
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
em_allocate_transmit_structures(struct em_softc *sc)
{
	sc->tx_buffer_area =  malloc(sizeof(struct em_buffer) *
	    sc->num_tx_desc, M_DEVBUF, M_NOWAIT);
	if (sc->tx_buffer_area == NULL) {
		device_printf(sc->dev, "Unable to allocate tx_buffer memory\n");
		return (ENOMEM);
	}

	bzero(sc->tx_buffer_area, sizeof(struct em_buffer) * sc->num_tx_desc);

	return (0);
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures.
 *
 **********************************************************************/
static int
em_setup_transmit_structures(struct em_softc *sc)
{
	device_t dev = sc->dev;
	struct em_buffer *tx_buffer;
	bus_size_t size;
	int error, i;

	/*
	 * Setup DMA descriptor areas.
	 */
	size = roundup2(sc->hw.max_frame_size, MCLBYTES);
	if ((error = bus_dma_tag_create(NULL,		/* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				size,			/* maxsize */
				EM_MAX_SCATTER,		/* nsegments */
				size,			/* maxsegsize */
				0,			/* flags */
				NULL,		/* lockfunc */
				NULL,		/* lockarg */
				&sc->txtag)) != 0) {
		device_printf(dev, "Unable to allocate TX DMA tag\n");
		goto fail;
	}

	if ((error = em_allocate_transmit_structures(sc)) != 0)
		goto fail;

	bzero(sc->tx_desc_base, (sizeof(struct em_tx_desc)) * sc->num_tx_desc);
	tx_buffer = sc->tx_buffer_area;
	for (i = 0; i < sc->num_tx_desc; i++) {
		error = bus_dmamap_create(sc->txtag, 0, &tx_buffer->map);
		if (error != 0) {
			device_printf(dev, "Unable to create TX DMA map\n");
			goto fail;
		}
		tx_buffer++;
	}

	sc->next_avail_tx_desc = 0;
	sc->oldest_used_tx_desc = 0;

	/* Set number of descriptors available */
	sc->num_tx_desc_avail = sc->num_tx_desc;

	/* Set checksum context */
	sc->active_checksum_context = OFFLOAD_NONE;
	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);

fail:
	em_free_transmit_structures(sc);
	return (error);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
em_initialize_transmit_unit(struct em_softc *sc)
{
	uint32_t	reg_tctl, tarc;
	uint32_t	reg_tipg = 0;
	uint64_t	bus_addr;

	 INIT_DEBUGOUT("em_initialize_transmit_unit: begin");
	/* Setup the Base and Length of the Tx Descriptor Ring */
	bus_addr = sc->txdma.dma_paddr;
	E1000_WRITE_REG(&sc->hw, TDBAL, (uint32_t)bus_addr);
	E1000_WRITE_REG(&sc->hw, TDBAH, (uint32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, TDLEN,
	    sc->num_tx_desc * sizeof(struct em_tx_desc));

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG(&sc->hw, TDH, 0);
	E1000_WRITE_REG(&sc->hw, TDT, 0);


	HW_DEBUGOUT2("Base = %x, Length = %x\n", E1000_READ_REG(&sc->hw, TDBAL),
	    E1000_READ_REG(&sc->hw, TDLEN));

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (sc->hw.mac_type) {
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
		if (sc->hw.media_type == em_media_type_fiber)
			reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}

	E1000_WRITE_REG(&sc->hw, TIPG, reg_tipg);
	E1000_WRITE_REG(&sc->hw, TIDV, sc->tx_int_delay.value);
	if(sc->hw.mac_type >= em_82540)
		E1000_WRITE_REG(&sc->hw, TADV, sc->tx_abs_int_delay.value);

	/* Program the Transmit Control Register */
	reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
	if (sc->hw.mac_type >= em_82571)
		reg_tctl |= E1000_TCTL_MULR;
	if (sc->link_duplex == 1) {
		reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	} else {
		reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	}
	E1000_WRITE_REG(&sc->hw, TCTL, reg_tctl);

	if (sc->hw.mac_type == em_82571 || sc->hw.mac_type == em_82572) {
		tarc = E1000_READ_REG(&sc->hw, TARC0);
		tarc |= ((1 << 25) | (1 << 21));
		E1000_WRITE_REG(&sc->hw, TARC0, tarc);
		tarc = E1000_READ_REG(&sc->hw, TARC1);
		tarc |= (1 << 25);
		if (reg_tctl & E1000_TCTL_MULR)
			tarc &= ~(1 << 28);
		else
			tarc |= (1 << 28);
		E1000_WRITE_REG(&sc->hw, TARC1, tarc);
	} else if (sc->hw.mac_type == em_80003es2lan) {
		tarc = E1000_READ_REG(&sc->hw, TARC0);
		tarc |= 1;
		if (sc->hw.media_type == em_media_type_internal_serdes)
			tarc |= (1 << 20);
		E1000_WRITE_REG(&sc->hw, TARC0, tarc);
		tarc = E1000_READ_REG(&sc->hw, TARC1);
		tarc |= 1;
		E1000_WRITE_REG(&sc->hw, TARC1, tarc);
	}

	/* Setup Transmit Descriptor Settings for this adapter */
	sc->txd_cmd = E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;

	if (sc->tx_int_delay.value > 0)
		sc->txd_cmd |= E1000_TXD_CMD_IDE;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
static void
em_free_transmit_structures(struct em_softc *sc)
{
	struct em_buffer *tx_buffer;
	int i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (sc->tx_buffer_area != NULL) {
		tx_buffer = sc->tx_buffer_area;
		for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
			if (tx_buffer->m_head != NULL) {
				bus_dmamap_sync(sc->txtag, tx_buffer->map,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->txtag,
				    tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			} else if (tx_buffer->map != NULL)
				bus_dmamap_unload(sc->txtag,
				    tx_buffer->map);
			if (tx_buffer->map != NULL) {
				bus_dmamap_destroy(sc->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		}
	}
	if (sc->tx_buffer_area != NULL) {
		free(sc->tx_buffer_area, M_DEVBUF);
		sc->tx_buffer_area = NULL;
	}
	if (sc->txtag != NULL) {
		bus_dma_tag_destroy(sc->txtag);
		sc->txtag = NULL;
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
em_transmit_checksum_setup(struct em_softc *sc, struct mbuf *mp,
    uint32_t *txd_upper, uint32_t *txd_lower)
{
	struct em_context_desc *TXD;
	struct em_buffer *tx_buffer;
	int curr_txd;

	if (mp->m_pkthdr.csum_flags) {

		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (sc->active_checksum_context == OFFLOAD_TCP_IP)
				return;
			else
				sc->active_checksum_context = OFFLOAD_TCP_IP;

		} else if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (sc->active_checksum_context == OFFLOAD_UDP_IP)
				return;
			else
				sc->active_checksum_context = OFFLOAD_UDP_IP;
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
	curr_txd = sc->next_avail_tx_desc;
	tx_buffer = &sc->tx_buffer_area[curr_txd];
	TXD = (struct em_context_desc *) &sc->tx_desc_base[curr_txd];

	TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
	TXD->lower_setup.ip_fields.ipcso =
		ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
	TXD->lower_setup.ip_fields.ipcse =
		htole16(ETHER_HDR_LEN + sizeof(struct ip) - 1);

	TXD->upper_setup.tcp_fields.tucss =
		ETHER_HDR_LEN + sizeof(struct ip);
	TXD->upper_setup.tcp_fields.tucse = htole16(0);

	if (sc->active_checksum_context == OFFLOAD_TCP_IP) {
		TXD->upper_setup.tcp_fields.tucso =
			ETHER_HDR_LEN + sizeof(struct ip) +
			offsetof(struct tcphdr, th_sum);
	} else if (sc->active_checksum_context == OFFLOAD_UDP_IP) {
		TXD->upper_setup.tcp_fields.tucso =
			ETHER_HDR_LEN + sizeof(struct ip) +
			offsetof(struct udphdr, uh_sum);
	}

	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length = htole32(sc->txd_cmd | E1000_TXD_CMD_DEXT);

	tx_buffer->m_head = NULL;

	if (++curr_txd == sc->num_tx_desc)
		curr_txd = 0;

	sc->num_tx_desc_avail--;
	sc->next_avail_tx_desc = curr_txd;
}

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static void
em_txeof(struct em_softc *sc)
{
	int i, num_avail;
	struct em_buffer *tx_buffer;
	struct em_tx_desc   *tx_desc;
	struct ifnet   *ifp = sc->ifp;

	EM_LOCK_ASSERT(sc);

	if (sc->num_tx_desc_avail == sc->num_tx_desc)
		return;

	num_avail = sc->num_tx_desc_avail;
	i = sc->oldest_used_tx_desc;

	tx_buffer = &sc->tx_buffer_area[i];
	tx_desc = &sc->tx_desc_base[i];

	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map,
	    BUS_DMASYNC_POSTREAD);
	while (tx_desc->upper.fields.status & E1000_TXD_STAT_DD) {

		tx_desc->upper.data = 0;
		num_avail++;

		if (tx_buffer->m_head) {
			ifp->if_opackets++;
			bus_dmamap_sync(sc->txtag, tx_buffer->map,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->txtag, tx_buffer->map);

			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}

		if (++i == sc->num_tx_desc)
			i = 0;

		tx_buffer = &sc->tx_buffer_area[i];
		tx_desc = &sc->tx_desc_base[i];
	}
	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->oldest_used_tx_desc = i;

	/*
	 * If we have enough room, clear IFF_DRV_OACTIVE to tell the stack
	 * that it is OK to send packets.
	 * If there are no pending descriptors, clear the timeout. Otherwise,
	 * if some descriptors have been freed, restart the timeout.
	 */
	if (num_avail > EM_TX_CLEANUP_THRESHOLD) {
		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if (num_avail == sc->num_tx_desc)
			ifp->if_timer = 0;
		else if (num_avail != sc->num_tx_desc_avail)
			ifp->if_timer = EM_TX_TIMEOUT;
	}
	sc->num_tx_desc_avail = num_avail;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
em_get_buf(int i, struct em_softc *sc, struct mbuf *mp)
{
	struct ifnet		*ifp = sc->ifp;
	bus_dma_segment_t	segs[1];
	struct em_buffer	*rx_buffer;
	int			error, nsegs;

	if (mp == NULL) {
		mp = m_getcl(M_DONTWAIT, MT_DATA, M_PKTHDR);
		if (mp == NULL) {
			sc->mbuf_cluster_failed++;
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

	rx_buffer = &sc->rx_buffer_area[i];

	/*
	 * Using memory from the mbuf cluster pool, invoke the
	 * bus_dma machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load_mbuf_sg(sc->rxtag, rx_buffer->map,
	    mp, segs, &nsegs, 0);
	if (error != 0) {
		m_free(mp);
		return (error);
	}
	/* If nsegs is wrong then the stack is corrupt. */
	KASSERT(nsegs == 1, ("Too many segments returned!"));
	rx_buffer->m_head = mp;
	sc->rx_desc_base[i].buffer_addr = htole64(segs[0].ds_addr);
	bus_dmamap_sync(sc->rxtag, rx_buffer->map, BUS_DMASYNC_PREREAD);

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
em_allocate_receive_structures(struct em_softc *sc)
{
	device_t dev = sc->dev;
	struct em_buffer *rx_buffer;
	int i, error;

	sc->rx_buffer_area = malloc(sizeof(struct em_buffer) * sc->num_rx_desc,
	    M_DEVBUF, M_NOWAIT);
	if (sc->rx_buffer_area == NULL) {
		device_printf(dev, "Unable to allocate rx_buffer memory\n");
		return (ENOMEM);
	}

	bzero(sc->rx_buffer_area, sizeof(struct em_buffer) * sc->num_rx_desc);

	error = bus_dma_tag_create(NULL,		/* parent */
				1, 0,			/* alignment, bounds */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				MCLBYTES,		/* maxsize */
				1,			/* nsegments */
				MCLBYTES,		/* maxsegsize */
				BUS_DMA_ALLOCNOW,	/* flags */
				NULL,			/* lockfunc */
				NULL,			/* lockarg */
				&sc->rxtag);
	if (error) {
		device_printf(dev, "%s: bus_dma_tag_create failed %d\n",
		    __func__, error);
		goto fail;
	}

	rx_buffer = sc->rx_buffer_area;
	for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(sc->rxtag, BUS_DMA_NOWAIT,
		    &rx_buffer->map);
		if (error) {
			device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
			    __func__, error);
			goto fail;
		}
	}

	for (i = 0; i < sc->num_rx_desc; i++) {
		error = em_get_buf(i, sc, NULL);
		if (error)
			goto fail;
	}
	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);

fail:
	em_free_receive_structures(sc);
	return (error);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *
 **********************************************************************/
static int
em_setup_receive_structures(struct em_softc *sc)
{
	int error;

	bzero(sc->rx_desc_base, (sizeof(struct em_rx_desc)) * sc->num_rx_desc);

	if ((error = em_allocate_receive_structures(sc)) != 0)
		return (error);

	/* Setup our descriptor pointers */
	sc->next_rx_desc_to_check = 0;

	return (0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
static void
em_initialize_receive_unit(struct em_softc *sc)
{
	struct ifnet	*ifp = sc->ifp;
	uint64_t	bus_addr;
	uint32_t	reg_rctl;
	uint32_t	reg_rxcsum;

	INIT_DEBUGOUT("em_initialize_receive_unit: begin");

	/*
	 * Make sure receives are disabled while setting
	 * up the descriptor ring
	 */
	E1000_WRITE_REG(&sc->hw, RCTL, 0);

	/* Set the Receive Delay Timer Register */
	E1000_WRITE_REG(&sc->hw, RDTR, sc->rx_int_delay.value | E1000_RDT_FPDB);

	if(sc->hw.mac_type >= em_82540) {
		E1000_WRITE_REG(&sc->hw, RADV, sc->rx_abs_int_delay.value);

		/*
		 * Set the interrupt throttling rate. Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns)
		 */
#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR	     1000000000/(MAX_INTS_PER_SEC * 256)
		E1000_WRITE_REG(&sc->hw, ITR, DEFAULT_ITR);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */
	bus_addr = sc->rxdma.dma_paddr;
	E1000_WRITE_REG(&sc->hw, RDBAL, (uint32_t)bus_addr);
	E1000_WRITE_REG(&sc->hw, RDBAH, (uint32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, RDLEN, sc->num_rx_desc *
			sizeof(struct em_rx_desc));

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG(&sc->hw, RDH, 0);
	E1000_WRITE_REG(&sc->hw, RDT, sc->num_rx_desc - 1);

	/* Setup the Receive Control Register */
	reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
		   E1000_RCTL_RDMTS_HALF |
		   (sc->hw.mc_filter_type << E1000_RCTL_MO_SHIFT);

	if (sc->hw.tbi_compatibility_on == TRUE)
		reg_rctl |= E1000_RCTL_SBP;


	switch (sc->rx_buffer_len) {
	default:
	case EM_RXBUFFER_2048:
		reg_rctl |= E1000_RCTL_SZ_2048;
		break;
	case EM_RXBUFFER_4096:
		reg_rctl |= E1000_RCTL_SZ_4096 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case EM_RXBUFFER_8192:
		reg_rctl |= E1000_RCTL_SZ_8192 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	case EM_RXBUFFER_16384:
		reg_rctl |= E1000_RCTL_SZ_16384 | E1000_RCTL_BSEX | E1000_RCTL_LPE;
		break;
	}

	if (ifp->if_mtu > ETHERMTU)
		reg_rctl |= E1000_RCTL_LPE;

	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if ((sc->hw.mac_type >= em_82543) &&
	    (ifp->if_capenable & IFCAP_RXCSUM)) {
		reg_rxcsum = E1000_READ_REG(&sc->hw, RXCSUM);
		reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&sc->hw, RXCSUM, reg_rxcsum);
	}

	/* Enable Receives */
	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
em_free_receive_structures(struct em_softc *sc)
{
	struct em_buffer *rx_buffer;
	int i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (sc->rx_buffer_area != NULL) {
		rx_buffer = sc->rx_buffer_area;
		for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->m_head != NULL) {
				bus_dmamap_sync(sc->rxtag, rx_buffer->map,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->rxtag,
				    rx_buffer->map);
				m_freem(rx_buffer->m_head);
				rx_buffer->m_head = NULL;
			} else if (rx_buffer->map != NULL)
				bus_dmamap_unload(sc->rxtag,
				    rx_buffer->map);
			if (rx_buffer->map != NULL) {
				bus_dmamap_destroy(sc->rxtag,
				    rx_buffer->map);
				rx_buffer->map = NULL;
			}
		}
	}
	if (sc->rx_buffer_area != NULL) {
		free(sc->rx_buffer_area, M_DEVBUF);
		sc->rx_buffer_area = NULL;
	}
	if (sc->rxtag != NULL) {
		bus_dma_tag_destroy(sc->rxtag);
		sc->rxtag = NULL;
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
em_rxeof(struct em_softc *sc, int count)
{
	struct ifnet	*ifp;
	struct mbuf	*mp;
	uint8_t		accept_frame = 0;
	uint8_t		eop = 0;
	uint16_t 	len, desc_len, prev_len_adj;
	int		i;

	/* Pointer to the receive descriptor being examined. */
	struct em_rx_desc   *current_desc;

	ifp = sc->ifp;
	i = sc->next_rx_desc_to_check;
	current_desc = &sc->rx_desc_base[i];
	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
	    BUS_DMASYNC_POSTREAD);

	if (!((current_desc->status) & E1000_RXD_STAT_DD))
		return (0);

	while ((current_desc->status & E1000_RXD_STAT_DD) &&
	    (count != 0) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		struct mbuf *m = NULL;

		mp = sc->rx_buffer_area[i].m_head;
		bus_dmamap_sync(sc->rxtag, sc->rx_buffer_area[i].map,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxtag,
		    sc->rx_buffer_area[i].map);

		accept_frame = 1;
		prev_len_adj = 0;
		desc_len = le16toh(current_desc->length);
		if (current_desc->status & E1000_RXD_STAT_EOP) {
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

			if (sc->fmp != NULL)
				pkt_len += sc->fmp->m_pkthdr.len;

			last_byte = *(mtod(mp, caddr_t) + desc_len - 1);			
			if (TBI_ACCEPT(&sc->hw, current_desc->status,
			    current_desc->errors,
			    pkt_len, last_byte)) {
				em_tbi_adjust_stats(&sc->hw,
				    &sc->stats, pkt_len,
				    sc->hw.mac_addr);
				if (len > 0)
					len--;
			} else
				accept_frame = 0;
		}

		if (accept_frame) {
			if (em_get_buf(i, sc, NULL) == ENOBUFS) {
				sc->dropped_pkts++;
				em_get_buf(i, sc, mp);
				if (sc->fmp != NULL)
					m_freem(sc->fmp);
				sc->fmp = NULL;
				sc->lmp = NULL;
				break;
			}

			/* Assign correct length to the current fragment */
			mp->m_len = len;

			if (sc->fmp == NULL) {
				mp->m_pkthdr.len = len;
				sc->fmp = mp; /* Store the first mbuf */
				sc->lmp = mp;
			} else {
				/* Chain mbuf's together */
				mp->m_flags &= ~M_PKTHDR;
				/*
				 * Adjust length of previous mbuf in chain if
				 * we received less than 4 bytes in the last
				 * descriptor.
				 */
				if (prev_len_adj > 0) {
					sc->lmp->m_len -= prev_len_adj;
					sc->fmp->m_pkthdr.len -=
					    prev_len_adj;
				}
				sc->lmp->m_next = mp;
				sc->lmp = sc->lmp->m_next;
				sc->fmp->m_pkthdr.len += len;
			}

			if (eop) {
				sc->fmp->m_pkthdr.rcvif = ifp;
				ifp->if_ipackets++;
				em_receive_checksum(sc, current_desc,
				    sc->fmp);
#ifndef __NO_STRICT_ALIGNMENT
				if (ifp->if_mtu > ETHERMTU &&
				    em_fixup_rx(sc) != 0)
					goto skip;
#endif
				if (current_desc->status & E1000_RXD_STAT_VP)
					VLAN_INPUT_TAG(ifp, sc->fmp,
					    (le16toh(current_desc->special) &
					    E1000_RXD_SPC_VLAN_MASK));
#ifndef __NO_STRICT_ALIGNMENT
skip:
#endif
				m = sc->fmp;
				sc->fmp = NULL;
				sc->lmp = NULL;
			}
		} else {
			sc->dropped_pkts++;
			em_get_buf(i, sc, mp);
			if (sc->fmp != NULL)
				m_freem(sc->fmp);
			sc->fmp = NULL;
			sc->lmp = NULL;
		}

		/* Zero out the receive descriptors status. */
		current_desc->status = 0;
		bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Advance our pointers to the next descriptor. */
		if (++i == sc->num_rx_desc)
			i = 0;
		if (m != NULL) {
			sc->next_rx_desc_to_check = i;
#ifdef DEVICE_POLLING
			EM_UNLOCK(sc);
			(*ifp->if_input)(ifp, m);
			EM_LOCK(sc);
#else
			(*ifp->if_input)(ifp, m);
#endif
			i = sc->next_rx_desc_to_check;
		}
		current_desc = &sc->rx_desc_base[i];
	}
	sc->next_rx_desc_to_check = i;

	/* Advance the E1000's Receive Queue #0  "Tail Pointer". */
	if (--i < 0)
		i = sc->num_rx_desc - 1;
	E1000_WRITE_REG(&sc->hw, RDT, i);
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
em_fixup_rx(struct em_softc *sc)
{
	struct mbuf *m, *n;
	int error;

	error = 0;
	m = sc->fmp;
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
			sc->fmp = n;
		} else {
			sc->dropped_pkts++;
			m_freem(sc->fmp);
			sc->fmp = NULL;
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
em_receive_checksum(struct em_softc *sc, struct em_rx_desc *rx_desc,
		    struct mbuf *mp)
{
	/* 82543 or newer only */
	if ((sc->hw.mac_type < em_82543) ||
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
em_enable_vlans(struct em_softc *sc)
{
	uint32_t ctrl;

	E1000_WRITE_REG(&sc->hw, VET, ETHERTYPE_VLAN);

	ctrl = E1000_READ_REG(&sc->hw, CTRL);
	ctrl |= E1000_CTRL_VME;
	E1000_WRITE_REG(&sc->hw, CTRL, ctrl);
}

static void
em_disable_vlans(struct em_softc *sc)
{
	uint32_t ctrl;

	ctrl = E1000_READ_REG(&sc->hw, CTRL);
	ctrl &= ~E1000_CTRL_VME;
	E1000_WRITE_REG(&sc->hw, CTRL, ctrl);
}

static void
em_enable_intr(struct em_softc *sc)
{
	E1000_WRITE_REG(&sc->hw, IMS, (IMS_ENABLE_MASK));
}

static void
em_disable_intr(struct em_softc *sc)
{
	/*
	 * The first version of 82542 had an errata where when link was forced
	 * it would stay up even up even if the cable was disconnected.
	 * Sequence errors were used to detect the disconnect and then the
	 * driver would unforce the link. This code in the in the ISR. For this
	 * to work correctly the Sequence error interrupt had to be enabled
	 * all the time.
	 */

	if (sc->hw.mac_type == em_82542_rev2_0)
	    E1000_WRITE_REG(&sc->hw, IMC,
		(0xffffffff & ~E1000_IMC_RXSEQ));
	else
	    E1000_WRITE_REG(&sc->hw, IMC,
		0xffffffff);
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
*	  Make sure we do not have ending address as 1,2,3,4(Hang) or 9,a,b,c (DAC)
*
*** *********************************************************************/
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
	safe_terminator = (uint32_t)((((uint32_t)address & 0x7) + (length & 0xF)) & 0xF);
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
em_update_stats_counters(struct em_softc *sc)
{
	struct ifnet   *ifp;

	if(sc->hw.media_type == em_media_type_copper ||
	   (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU)) {
		sc->stats.symerrs += E1000_READ_REG(&sc->hw, SYMERRS);
		sc->stats.sec += E1000_READ_REG(&sc->hw, SEC);
	}
	sc->stats.crcerrs += E1000_READ_REG(&sc->hw, CRCERRS);
	sc->stats.mpc += E1000_READ_REG(&sc->hw, MPC);
	sc->stats.scc += E1000_READ_REG(&sc->hw, SCC);
	sc->stats.ecol += E1000_READ_REG(&sc->hw, ECOL);

	sc->stats.mcc += E1000_READ_REG(&sc->hw, MCC);
	sc->stats.latecol += E1000_READ_REG(&sc->hw, LATECOL);
	sc->stats.colc += E1000_READ_REG(&sc->hw, COLC);
	sc->stats.dc += E1000_READ_REG(&sc->hw, DC);
	sc->stats.rlec += E1000_READ_REG(&sc->hw, RLEC);
	sc->stats.xonrxc += E1000_READ_REG(&sc->hw, XONRXC);
	sc->stats.xontxc += E1000_READ_REG(&sc->hw, XONTXC);
	sc->stats.xoffrxc += E1000_READ_REG(&sc->hw, XOFFRXC);
	sc->stats.xofftxc += E1000_READ_REG(&sc->hw, XOFFTXC);
	sc->stats.fcruc += E1000_READ_REG(&sc->hw, FCRUC);
	sc->stats.prc64 += E1000_READ_REG(&sc->hw, PRC64);
	sc->stats.prc127 += E1000_READ_REG(&sc->hw, PRC127);
	sc->stats.prc255 += E1000_READ_REG(&sc->hw, PRC255);
	sc->stats.prc511 += E1000_READ_REG(&sc->hw, PRC511);
	sc->stats.prc1023 += E1000_READ_REG(&sc->hw, PRC1023);
	sc->stats.prc1522 += E1000_READ_REG(&sc->hw, PRC1522);
	sc->stats.gprc += E1000_READ_REG(&sc->hw, GPRC);
	sc->stats.bprc += E1000_READ_REG(&sc->hw, BPRC);
	sc->stats.mprc += E1000_READ_REG(&sc->hw, MPRC);
	sc->stats.gptc += E1000_READ_REG(&sc->hw, GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	sc->stats.gorcl += E1000_READ_REG(&sc->hw, GORCL);
	sc->stats.gorch += E1000_READ_REG(&sc->hw, GORCH);
	sc->stats.gotcl += E1000_READ_REG(&sc->hw, GOTCL);
	sc->stats.gotch += E1000_READ_REG(&sc->hw, GOTCH);

	sc->stats.rnbc += E1000_READ_REG(&sc->hw, RNBC);
	sc->stats.ruc += E1000_READ_REG(&sc->hw, RUC);
	sc->stats.rfc += E1000_READ_REG(&sc->hw, RFC);
	sc->stats.roc += E1000_READ_REG(&sc->hw, ROC);
	sc->stats.rjc += E1000_READ_REG(&sc->hw, RJC);

	sc->stats.torl += E1000_READ_REG(&sc->hw, TORL);
	sc->stats.torh += E1000_READ_REG(&sc->hw, TORH);
	sc->stats.totl += E1000_READ_REG(&sc->hw, TOTL);
	sc->stats.toth += E1000_READ_REG(&sc->hw, TOTH);

	sc->stats.tpr += E1000_READ_REG(&sc->hw, TPR);
	sc->stats.tpt += E1000_READ_REG(&sc->hw, TPT);
	sc->stats.ptc64 += E1000_READ_REG(&sc->hw, PTC64);
	sc->stats.ptc127 += E1000_READ_REG(&sc->hw, PTC127);
	sc->stats.ptc255 += E1000_READ_REG(&sc->hw, PTC255);
	sc->stats.ptc511 += E1000_READ_REG(&sc->hw, PTC511);
	sc->stats.ptc1023 += E1000_READ_REG(&sc->hw, PTC1023);
	sc->stats.ptc1522 += E1000_READ_REG(&sc->hw, PTC1522);
	sc->stats.mptc += E1000_READ_REG(&sc->hw, MPTC);
	sc->stats.bptc += E1000_READ_REG(&sc->hw, BPTC);

	if (sc->hw.mac_type >= em_82543) {
		sc->stats.algnerrc += E1000_READ_REG(&sc->hw, ALGNERRC);
		sc->stats.rxerrc += E1000_READ_REG(&sc->hw, RXERRC);
		sc->stats.tncrs += E1000_READ_REG(&sc->hw, TNCRS);
		sc->stats.cexterr += E1000_READ_REG(&sc->hw, CEXTERR);
		sc->stats.tsctc += E1000_READ_REG(&sc->hw, TSCTC);
		sc->stats.tsctfc += E1000_READ_REG(&sc->hw, TSCTFC);
	}
	ifp = sc->ifp;

	ifp->if_collisions = sc->stats.colc;

	/* Rx Errors */
	ifp->if_ierrors = sc->dropped_pkts + sc->stats.rxerrc +
	    sc->stats.crcerrs + sc->stats.algnerrc + sc->stats.rlec +
	    sc->stats.mpc + sc->stats.cexterr;

	/* Tx Errors */
	ifp->if_oerrors = sc->stats.ecol + sc->stats.latecol +
	    sc->watchdog_events;
}


/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
em_print_debug_info(struct em_softc *sc)
{
	device_t dev = sc->dev;
	uint8_t *hw_addr = sc->hw.hw_addr;

	device_printf(dev, "Adapter hardware address = %p \n", hw_addr);
	device_printf(dev, "CTRL = 0x%x RCTL = 0x%x \n",
	    E1000_READ_REG(&sc->hw, CTRL),
	    E1000_READ_REG(&sc->hw, RCTL));
	device_printf(dev, "Packet buffer = Tx=%dk Rx=%dk \n",
	    ((E1000_READ_REG(&sc->hw, PBA) & 0xffff0000) >> 16),\
	    (E1000_READ_REG(&sc->hw, PBA) & 0xffff) );
	device_printf(dev, "Flow control watermarks high = %d low = %d\n",
	    sc->hw.fc_high_water,
	    sc->hw.fc_low_water);
	device_printf(dev, "tx_int_delay = %d, tx_abs_int_delay = %d\n",
	    E1000_READ_REG(&sc->hw, TIDV),
	    E1000_READ_REG(&sc->hw, TADV));
	device_printf(dev, "rx_int_delay = %d, rx_abs_int_delay = %d\n",
	    E1000_READ_REG(&sc->hw, RDTR),
	    E1000_READ_REG(&sc->hw, RADV));
	device_printf(dev, "fifo workaround = %lld, fifo_reset_count = %lld\n",
	    (long long)sc->tx_fifo_wrk_cnt,
	    (long long)sc->tx_fifo_reset_cnt);
	device_printf(dev, "hw tdh = %d, hw tdt = %d\n",
	    E1000_READ_REG(&sc->hw, TDH),
	    E1000_READ_REG(&sc->hw, TDT));
	device_printf(dev, "Num Tx descriptors avail = %d\n",
	    sc->num_tx_desc_avail);
	device_printf(dev, "Tx Descriptors not avail1 = %ld\n",
	    sc->no_tx_desc_avail1);
	device_printf(dev, "Tx Descriptors not avail2 = %ld\n",
	    sc->no_tx_desc_avail2);
	device_printf(dev, "Std mbuf failed = %ld\n",
	    sc->mbuf_alloc_failed);
	device_printf(dev, "Std mbuf cluster failed = %ld\n",
	    sc->mbuf_cluster_failed);
	device_printf(dev, "Driver dropped packets = %ld\n",
	    sc->dropped_pkts);
}

static void
em_print_hw_stats(struct em_softc *sc)
{
	device_t dev = sc->dev;

	device_printf(dev, "Excessive collisions = %lld\n",
	    (long long)sc->stats.ecol);
	device_printf(dev, "Symbol errors = %lld\n",
	    (long long)sc->stats.symerrs);
	device_printf(dev, "Sequence errors = %lld\n",
	    (long long)sc->stats.sec);
	device_printf(dev, "Defer count = %lld\n", (long long)sc->stats.dc);

	device_printf(dev, "Missed Packets = %lld\n", (long long)sc->stats.mpc);
	device_printf(dev, "Receive No Buffers = %lld\n",
	    (long long)sc->stats.rnbc);
	device_printf(dev, "Receive length errors = %lld\n",
	    (long long)sc->stats.rlec);
	device_printf(dev, "Receive errors = %lld\n",
	    (long long)sc->stats.rxerrc);
	device_printf(dev, "Crc errors = %lld\n", (long long)sc->stats.crcerrs);
	device_printf(dev, "Alignment errors = %lld\n",
	    (long long)sc->stats.algnerrc);
	device_printf(dev, "Carrier extension errors = %lld\n",
	    (long long)sc->stats.cexterr);
	device_printf(dev, "RX overruns = %ld\n", sc->rx_overruns);
	device_printf(dev, "watchdog timeouts = %ld\n", sc->watchdog_events);

	device_printf(dev, "XON Rcvd = %lld\n", (long long)sc->stats.xonrxc);
	device_printf(dev, "XON Xmtd = %lld\n", (long long)sc->stats.xontxc);
	device_printf(dev, "XOFF Rcvd = %lld\n", (long long)sc->stats.xoffrxc);
	device_printf(dev, "XOFF Xmtd = %lld\n", (long long)sc->stats.xofftxc);

	device_printf(dev, "Good Packets Rcvd = %lld\n",
	    (long long)sc->stats.gprc);
	device_printf(dev, "Good Packets Xmtd = %lld\n",
	    (long long)sc->stats.gptc);
}

static int
em_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	struct em_softc *sc;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct em_softc *)arg1;
		em_print_debug_info(sc);
	}

	return (error);
}


static int
em_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	struct em_softc *sc;
	int error;
	int result;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct em_softc *)arg1;
		em_print_hw_stats(sc);
	}

	return (error);
}

static int
em_sysctl_int_delay(SYSCTL_HANDLER_ARGS)
{
	struct em_int_delay_info *info;
	struct em_softc *sc;
	uint32_t regval;
	int error;
	int usecs;
	int ticks;

	info = (struct em_int_delay_info *)arg1;
	usecs = info->value;
	error = sysctl_handle_int(oidp, &usecs, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	if (usecs < 0 || usecs > E1000_TICKS_TO_USECS(65535))
		return (EINVAL);
	info->value = usecs;
	ticks = E1000_USECS_TO_TICKS(usecs);

	sc = info->sc;
	
	EM_LOCK(sc);
	regval = E1000_READ_OFFSET(&sc->hw, info->offset);
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
			sc->txd_cmd &= ~E1000_TXD_CMD_IDE;
			/* Don't write 0 into the TIDV register. */
			regval++;
		} else
			sc->txd_cmd |= E1000_TXD_CMD_IDE;
		break;
	}
	E1000_WRITE_OFFSET(&sc->hw, info->offset, regval);
	EM_UNLOCK(sc);
	return (0);
}

static void
em_add_int_delay_sysctl(struct em_softc *sc, const char *name,
	const char *description, struct em_int_delay_info *info,
	int offset, int value)
{
	info->sc = sc;
	info->offset = offset;
	info->value = value;
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW,
	    info, 0, em_sysctl_int_delay, "I", description);
}

#ifndef DEVICE_POLLING
static void
em_add_int_process_limit(struct em_softc *sc, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(sc->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(sc->dev)),
	    OID_AUTO, name, CTLTYPE_INT|CTLFLAG_RW, limit, value, description);
}
#endif
