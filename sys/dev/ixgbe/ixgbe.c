/*******************************************************************************

Copyright (c) 2001-2007, Intel Corporation
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
/* $FreeBSD$ */

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
char ixgbe_driver_version[] = "1.2.6";

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
static void     ixgbe_start_locked(struct ifnet *);
static int      ixgbe_ioctl(struct ifnet *, u_long, caddr_t);
static void     ixgbe_watchdog(struct adapter *);
static void     ixgbe_init(void *);
static void     ixgbe_init_locked(struct adapter *);
static void     ixgbe_stop(void *);
static void     ixgbe_media_status(struct ifnet *, struct ifmediareq *);
static int      ixgbe_media_change(struct ifnet *);
static void     ixgbe_identify_hardware(struct adapter *);
static int      ixgbe_allocate_pci_resources(struct adapter *);
static void     ixgbe_free_pci_resources(struct adapter *);
static void     ixgbe_local_timer(void *);
static int      ixgbe_hardware_init(struct adapter *);
static void     ixgbe_setup_interface(device_t, struct adapter *);
static int	ixgbe_allocate_queues(struct adapter *);
static int	ixgbe_allocate_msix_resources(struct adapter *);
#if __FreeBSD_version >= 700000
static int	ixgbe_setup_msix(struct adapter *);
#endif
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
static int      ixgbe_rxeof(struct rx_ring *, int);
static void	ixgbe_rx_checksum(struct adapter *, uint32_t, struct mbuf *);
static void     ixgbe_set_promisc(struct adapter *);
static void     ixgbe_disable_promisc(struct adapter *);
static void     ixgbe_set_multi(struct adapter *);
static void     ixgbe_print_hw_stats(struct adapter *);
static void	ixgbe_print_debug_info(struct adapter *);
static void     ixgbe_update_link_status(struct adapter *);
static int	ixgbe_get_buf(struct rx_ring *, int);
static void     ixgbe_enable_vlans(struct adapter * adapter);
static int      ixgbe_encap(struct adapter *, struct mbuf **);
static int      ixgbe_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int	ixgbe_sysctl_debug(SYSCTL_HANDLER_ARGS);
static int	ixgbe_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	ixgbe_dma_malloc(struct adapter *, bus_size_t,
		    struct ixgbe_dma_alloc *, int);
static void     ixgbe_dma_free(struct adapter *, struct ixgbe_dma_alloc *);
static void	ixgbe_add_rx_process_limit(struct adapter *, const char *,
		    const char *, int *, int);
static boolean_t ixgbe_tx_csum_setup(struct tx_ring *, struct mbuf *);
static boolean_t ixgbe_tso_setup(struct tx_ring *, struct mbuf *, u32 *);
static void	ixgbe_set_ivar(struct adapter *, u16, u8);
static void	ixgbe_configure_ivars(struct adapter *);

/* Legacy Fast Interrupt routine and handlers */
#if __FreeBSD_version >= 700000
static int	ixgbe_fast_irq(void *);
/* The MSI/X Interrupt handlers */
static void	ixgbe_msix_tx(void *);
static void	ixgbe_msix_rx(void *);
static void	ixgbe_msix_link(void *);
#else
static void	ixgbe_fast_irq(void *);
#endif

static void	ixgbe_rxtx(void *context, int pending);
static void	ixgbe_link(void *context, int pending);

#ifndef NO_82598_A0_SUPPORT
static void	desc_flip(void *);
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

/* How many packets rxeof tries to clean at a time */
static int ixgbe_rx_process_limit = 100;
TUNABLE_INT("hw.ixgbe.rx_process_limit", &ixgbe_rx_process_limit);

/* Flow control setting, default to full */
static int ixgbe_flow_control = 3;
TUNABLE_INT("hw.ixgbe.flow_control", &ixgbe_flow_control);

/* Number of TX Queues, note multi tx is not working */
static int ixgbe_tx_queues = 1;
TUNABLE_INT("hw.ixgbe.tx_queues", &ixgbe_tx_queues);

/* Number of RX Queues */
static int ixgbe_rx_queues = 8;
TUNABLE_INT("hw.ixgbe.rx_queues", &ixgbe_rx_queues);

/* Number of Other Queues, this is used for link interrupts */
static int ixgbe_other_queues = 1;
TUNABLE_INT("hw.ixgbe.other_queues", &ixgbe_other_queues);

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

	u_int16_t       pci_vendor_id = 0;
	u_int16_t       pci_device_id = 0;
	u_int16_t       pci_subvendor_id = 0;
	u_int16_t       pci_subdevice_id = 0;
	char            adapter_name[60];

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
			switch (pci_device_id) {
				case IXGBE_DEV_ID_82598AF_DUAL_PORT :
					ixgbe_total_ports += 2;
					break;
				case IXGBE_DEV_ID_82598AF_SINGLE_PORT :
					ixgbe_total_ports += 1;
				default:
					break;
			}
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
	uint32_t	ctrl_ext;
	char		name_string[16];

	INIT_DEBUGOUT("ixgbe_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_get_softc(dev);
	adapter->dev = adapter->osdep.dev = dev;
	/* General Lock Init*/
	snprintf(name_string, sizeof(name_string), "%s:core",
	    device_get_nameunit(dev));
	mtx_init(&adapter->core_mtx, name_string, NULL, MTX_DEF);

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

	/* Set up the timer callout */
	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware revision */
	ixgbe_identify_hardware(adapter);

	/* Indicate to RX setup to use Jumbo Clusters */
	adapter->bigbufs = TRUE;

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

#if __FreeBSD_version >= 700000
	if (adapter->msix) {
		error = ixgbe_setup_msix(adapter); 
		if (error) 
			goto err_out;
	}
#endif

	/* Initialize the shared code */
	if (ixgbe_init_shared_code(&adapter->hw)) {
		device_printf(dev,"Unable to initialize the shared code\n");
		error = EIO;
		goto err_out;
	}

	/* Initialize the hardware */
	if (ixgbe_hardware_init(adapter)) {
		device_printf(dev,"Unable to initialize the hardware\n");
		error = EIO;
		goto err_out;
	}

	/* Setup OS specific network interface */
	ixgbe_setup_interface(dev, adapter);

	/* Sysctl for limiting the amount of work done in the taskqueue */
	ixgbe_add_rx_process_limit(adapter, "rx_processing_limit",
	    "max number of rx packets to process", &adapter->rx_process_limit,
	    ixgbe_rx_process_limit);

	/* Initialize statistics */
	ixgbe_update_stats_counters(adapter);

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

	INIT_DEBUGOUT("ixgbe_attach: end");
	return (0);

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

	mtx_lock(&adapter->core_mtx);
	ixgbe_stop(adapter);
	mtx_unlock(&adapter->core_mtx);

	if (adapter->tq != NULL) {
		taskqueue_drain(adapter->tq, &adapter->rxtx_task);
		taskqueue_drain(taskqueue_fast, &adapter->link_task);
		taskqueue_free(adapter->tq);
		adapter->tq = NULL;
	}

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&adapter->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_CTRL_EXT, ctrl_ext);

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);
	ixgbe_free_pci_resources(adapter);
	bus_generic_detach(dev);
	if_free(adapter->ifp);

	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);

	mtx_destroy(&adapter->core_mtx);
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
	mtx_lock(&adapter->core_mtx);
	ixgbe_stop(adapter);
	mtx_unlock(&adapter->core_mtx);
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
ixgbe_start_locked(struct ifnet * ifp)
{
	struct mbuf    *m_head;
	struct adapter *adapter = ifp->if_softc;

	mtx_assert(&adapter->tx_mtx, MA_OWNED);

	if ((ifp->if_drv_flags & (IFF_DRV_RUNNING|IFF_DRV_OACTIVE)) !=
	    IFF_DRV_RUNNING)
		return;
	if (!adapter->link_active)
		return;

	while (!IFQ_DRV_IS_EMPTY(&ifp->if_snd)) {

		IFQ_DRV_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (ixgbe_encap(adapter, &m_head)) {
			if (m_head == NULL)
				break;
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IFQ_DRV_PREPEND(&ifp->if_snd, m_head);
			break;
		}
		/* Send a copy of the frame to the BPF listener */
		ETHER_BPF_MTAP(ifp, m_head);

		/* Set timeout in case hardware has problems transmitting */
		adapter->watchdog_timer = IXGBE_TX_TIMEOUT;

	}
	return;
}

static void
ixgbe_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	mtx_lock(&adapter->tx_mtx);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ixgbe_start_locked(ifp);
	mtx_unlock(&adapter->tx_mtx);
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
				mtx_lock(&adapter->core_mtx);
				ixgbe_init_locked(adapter);
				mtx_unlock(&adapter->core_mtx);
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
			mtx_lock(&adapter->core_mtx);
			ifp->if_mtu = ifr->ifr_mtu;
			adapter->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
			ixgbe_init_locked(adapter);
			mtx_unlock(&adapter->core_mtx);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		mtx_lock(&adapter->core_mtx);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ adapter->if_flags) &
				    IFF_PROMISC) {
					ixgbe_disable_promisc(adapter);
					ixgbe_set_promisc(adapter);
                                }
			} else
				ixgbe_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixgbe_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		mtx_unlock(&adapter->core_mtx);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			mtx_lock(&adapter->core_mtx);
			ixgbe_disable_intr(adapter);
			ixgbe_set_multi(adapter);
			ixgbe_enable_intr(adapter);
			mtx_unlock(&adapter->core_mtx);
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
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

static void
ixgbe_watchdog(struct adapter *adapter)
{

	mtx_assert(&adapter->core_mtx, MA_OWNED);

        /*
         * The timer is set to 5 every time ixgbe_start() queues a packet.
         * Then ixgbe_txeof() keeps resetting to 5 as long as it cleans at
         * least one descriptor.
         * Finally, anytime all descriptors are clean the timer is
         * set to 0.
         */
        if (adapter->watchdog_timer == 0 || --adapter->watchdog_timer)
                return;

	/*
	 * If we are in this routine because of pause frames, then don't
	 * reset the hardware.
	 */
	if (IXGBE_READ_REG(&adapter->hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF) {
		adapter->watchdog_timer = IXGBE_TX_TIMEOUT;
		return;
	}


	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	ixgbe_print_debug_info(adapter);

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
	struct ifnet   *ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	u32		txdctl, rxdctl, mhadd;

	INIT_DEBUGOUT("ixgbe_init: begin");

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

	if (ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		ixgbe_enable_vlans(adapter);

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev,"Could not setup transmit structures\n");
		ixgbe_stop(adapter);
		return;
	}

	ixgbe_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixgbe_set_multi(adapter);

	/*
	** If we are resetting MTU smaller than 2K
	** drop to small RX buffers
	*/
	if (adapter->max_frame_size <= MCLBYTES)
		adapter->bigbufs = FALSE;

	/* Prepare receive descriptors and buffers */
	if (ixgbe_setup_receive_structures(adapter)) {
		device_printf(dev,"Could not setup receive structures\n");
		ixgbe_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixgbe_initialize_receive_units(adapter);

	/* Enable Enhanced MSIX mode */
	if (adapter->msix) {
		u32	gpie;
		gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);
		gpie |= IXGBE_GPIE_MSIX_MODE;
		gpie |= IXGBE_GPIE_EIAME | IXGBE_GPIE_PBA_SUPPORT |
		    IXGBE_GPIE_OCD;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_GPIE, gpie);
		gpie = IXGBE_READ_REG(&adapter->hw, IXGBE_GPIE);
	}

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
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_TXDCTL(i), txdctl);
	}

	for (int i = 0; i < adapter->num_rx_queues; i++) {
		rxdctl = IXGBE_READ_REG(&adapter->hw, IXGBE_RXDCTL(i));
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXDCTL(i), rxdctl);
	}

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);

	/* Set up MSI/X routing */
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

	mtx_lock(&adapter->core_mtx);
	ixgbe_init_locked(adapter);
	mtx_unlock(&adapter->core_mtx);
	return;
}


static void
ixgbe_link(void *context, int pending)
{
        struct adapter  *adapter = context;
        struct ifnet *ifp = adapter->ifp;

        mtx_lock(&adapter->core_mtx);
        if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
                mtx_unlock(&adapter->core_mtx);
                return;
        }

        callout_stop(&adapter->timer);
        ixgbe_update_link_status(adapter);
        callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
        mtx_unlock(&adapter->core_mtx);
}

/*
** MSI and Legacy Deferred Handler
**	- note this runs without the general lock
*/
static void
ixgbe_rxtx(void *context, int pending)
{
        struct adapter  *adapter = context;
        struct ifnet    *ifp = adapter->ifp;
	/* For legacy there is only one of each */
	struct rx_ring *rxr = adapter->rx_rings;
	struct tx_ring *txr = adapter->tx_rings;

        if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
                if (ixgbe_rxeof(rxr, adapter->rx_process_limit) != 0)
                        taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);
                mtx_lock(&adapter->tx_mtx);
                ixgbe_txeof(txr);

                if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
                        ixgbe_start_locked(ifp);
                mtx_unlock(&adapter->tx_mtx);
        }

        ixgbe_enable_intr(adapter);
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/

#if __FreeBSD_version >= 700000
static int
#else
static void
#endif
ixgbe_fast_irq(void *arg)
{
	u32       reg_eicr;
	struct adapter *adapter = arg;

	reg_eicr = IXGBE_READ_REG(&adapter->hw, IXGBE_EICR);
	if (reg_eicr == 0)
		return FILTER_STRAY;

        ixgbe_disable_intr(adapter);
        taskqueue_enqueue(adapter->tq, &adapter->rxtx_task);

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC)
        	taskqueue_enqueue(taskqueue_fast, &adapter->link_task);

	return FILTER_HANDLED;
}


#if __FreeBSD_version >= 700000
/*********************************************************************
 *
 *  MSI TX Interrupt Service routine
 *
 **********************************************************************/

void
ixgbe_msix_tx(void *arg)
{
	struct tx_ring *txr = arg;
	struct adapter *adapter = txr->adapter;
	struct ifnet   *ifp = adapter->ifp;
	uint32_t       loop_cnt = MAX_INTR;

	mtx_lock(&adapter->tx_mtx);

	while (loop_cnt > 0) {
		if (__predict_false(!ixgbe_txeof(txr)))
			break;
		loop_cnt--;
	}

	if (ifp->if_drv_flags & IFF_DRV_RUNNING &&
	    ifp->if_snd.ifq_head != NULL)
		ixgbe_start_locked(ifp);
	ixgbe_enable_intr(adapter);
	mtx_unlock(&adapter->tx_mtx);
	return;
}

/*********************************************************************
 *
 *  MSI RX Interrupt Service routine
 *
 **********************************************************************/

static void
ixgbe_msix_rx(void *arg)
{
	struct rx_ring	*rxr = arg;
	struct adapter	*adapter = rxr->adapter;
	struct ifnet	*ifp = adapter->ifp;
	uint32_t	loop = MAX_INTR;


	while ((loop-- > 0) && (ifp->if_drv_flags & IFF_DRV_RUNNING))
		ixgbe_rxeof(rxr, adapter->rx_process_limit);

	ixgbe_enable_intr(adapter);
}

static void
ixgbe_msix_link(void *arg)
{
	struct adapter	*adapter = arg;
	uint32_t       reg_eicr;

	mtx_lock(&adapter->core_mtx);

	reg_eicr = IXGBE_READ_REG(&adapter->hw, IXGBE_EICR);

	if (reg_eicr & IXGBE_EICR_LSC) {
		callout_stop(&adapter->timer);
		ixgbe_update_link_status(adapter);
		callout_reset(&adapter->timer, hz,
		    ixgbe_local_timer, adapter);
	}

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, IXGBE_EIMS_OTHER);
	ixgbe_enable_intr(adapter);
	mtx_unlock(&adapter->core_mtx);
}
#endif /*  __FreeBSD_version >= 700000 */

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
	ixgbe_update_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->link_active)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_10G_SR | IFM_FDX;

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
ixgbe_encap(struct adapter *adapter, struct mbuf **m_headp)
{
	u32		olinfo_status = 0, cmd_type_len = 0;
	u32		paylen;
	int             i, j, error, nsegs;
	int		first, last = 0;
	struct mbuf	*m_head;
	bus_dma_segment_t segs[IXGBE_MAX_SCATTER];
	bus_dmamap_t	map;
	struct tx_ring  *txr = adapter->tx_rings;
	struct ixgbe_tx_buf *txbuf, *txbuf_mapped;
	union ixgbe_adv_tx_desc *txd = NULL;

	m_head = *m_headp;
	paylen = 0;

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
			adapter->no_tx_desc_avail1++;
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

	/* Make certain there are enough descriptors */
	if (nsegs > txr->tx_avail - 2) {
		adapter->no_tx_desc_avail2++;
		error = ENOBUFS;
		goto encap_fail;
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
	} else if (m_head->m_pkthdr.csum_flags & CSUM_OFFLOAD) {
		if (ixgbe_tx_csum_setup(txr, m_head))
			olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
	}

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
		txbuf->next_eop = -1;
		/*
		** we have to do this inside the loop right now
		** because of the hardware workaround.
		*/
		if (j == (nsegs -1)) /* Last descriptor gets EOP and RS */
			txd->read.cmd_type_len |=
			    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
#ifndef NO_82598_A0_SUPPORT
		if (adapter->hw.revision_id == 0)
			desc_flip(txd);
#endif
	}

	txr->tx_avail -= nsegs;
	txr->next_avail_tx_desc = i;

	txbuf->m_head = m_head;
	txbuf->map = map;
	bus_dmamap_sync(txr->txtag, map, BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
        txbuf->next_eop = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(txr->me), i);
	return (0);

encap_fail:
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
	uint32_t fctrl;
	uint8_t mta[MAX_NUM_MULTICAST_ADDRESSES * IXGBE_ETH_LENGTH_OF_ADDRESS];
	struct ifmultiaddr *ifma;
	int             mcnt = 0;
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

	ixgbe_update_mc_addr_list(&adapter->hw, mta, mcnt, 0);

	return;
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

	ixgbe_update_link_status(adapter);
	ixgbe_update_stats_counters(adapter);
	if (ixgbe_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ixgbe_print_hw_stats(adapter);
	}
	/*
	 * Each second we check the watchdog
	 * to protect against hardware hangs.
	 */
	ixgbe_watchdog(adapter);

	callout_reset(&adapter->timer, hz, ixgbe_local_timer, adapter);
}

static void
ixgbe_update_link_status(struct adapter *adapter)
{
	uint32_t  link_speed;
	boolean_t link_up = FALSE;
	struct ifnet	*ifp = adapter->ifp;
	device_t dev = adapter->dev;

	ixgbe_check_link(&adapter->hw, &link_speed, &link_up);

	if (link_up){ 
		if (adapter->link_active == FALSE) {
			if (bootverbose)
				device_printf(dev,"Link is up %d Mbps %s \n",
				    10000, "Full Duplex");
			adapter->link_active = TRUE;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (adapter->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			adapter->link_active = FALSE;
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

#if __FreeBSD_version >= 700000
/*********************************************************************
 *
 *  Setup MSIX: this is a prereq for doing Multiqueue/RSS.
 *
 **********************************************************************/
static int
ixgbe_setup_msix(struct adapter *adapter)
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
		adapter->msix++;
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
		adapter->msix++;
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
	adapter->msix++;

	return (0);
}
#endif

static int
ixgbe_allocate_pci_resources(struct adapter *adapter)
{
	int             error, rid;
	device_t        dev = adapter->dev;

	rid = PCIR_BAR(0);
	adapter->res_memory = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(adapter->res_memory)) {
		device_printf(dev,"Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	adapter->osdep.mem_bus_space_tag =
		rman_get_bustag(adapter->res_memory);
	adapter->osdep.mem_bus_space_handle =
		rman_get_bushandle(adapter->res_memory);
	adapter->hw.hw_addr = (uint8_t *) &adapter->osdep.mem_bus_space_handle;

	/*
	 * First try to setup MSI/X interrupts,
	 * if that fails fall back to Legacy.
	 */
	if (ixgbe_allocate_msix_resources(adapter)) {
		int val;

		adapter->num_tx_queues = 1;
		adapter->num_rx_queues = 1;
		val = 0;

#if __FreeBSD_version >= 700000
		/* Attempt to use MSI */
		val = pci_msi_count(dev);
		if ((val) && pci_alloc_msi(dev, &val) == 0) {
			adapter->rid[0] = 1;
			device_printf(dev, "MSI Interrupts enabled\n");
		} else
#endif
		{
			adapter->rid[0] = 0;
			device_printf(dev, "Legacy Interrupts enabled\n");
		}
		adapter->res[0] = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &adapter->rid[0], RF_SHAREABLE | RF_ACTIVE);
		if (adapter->res[0] == NULL) {
			device_printf(dev, "Unable to allocate bus "
			    "resource: interrupt\n");
			return (ENXIO);
		}
		/* Set the handler contexts */
		TASK_INIT(&adapter->rxtx_task, 0, ixgbe_rxtx, adapter);
		TASK_INIT(&adapter->link_task, 0, ixgbe_link, adapter);
		adapter->tq = taskqueue_create_fast("ix_taskq", M_NOWAIT,
		    taskqueue_thread_enqueue, &adapter->tq);
		taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s taskq",
		    device_get_nameunit(adapter->dev));
#if __FreeBSD_version < 700000
		error = bus_setup_intr(dev, adapter->res[0],
		    INTR_TYPE_NET | INTR_FAST, ixgbe_fast_irq,
#else
		error = bus_setup_intr(dev, adapter->res[0],
		    INTR_TYPE_NET, ixgbe_fast_irq, NULL,
#endif
		    adapter, &adapter->tag[0]);
		if (error) {
			adapter->res[0] = NULL;
			device_printf(dev, "Failed to register"
			    " Fast Legacy handler");
			return (error);
		}
	}

	adapter->hw.back = &adapter->osdep;
	return (0);
}

#if __FreeBSD_version >= 700000
/*
 * Attempt to configure MSI/X, the prefered
 * interrupt option.
 */
static int
ixgbe_allocate_msix_resources(struct adapter *adapter)
{
	int             error, val, want, rid;
	device_t        dev = adapter->dev;
	int		vector = 1;


	/* First map the MSIX table */
	rid = PCIR_BAR(3);
	adapter->res_msix = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (!adapter->res_msix) {
		device_printf(dev,"Unable to map MSIX table \n");
			return (ENXIO);
      	}

	/* Now figure out now many vectors we need to use */
	val = pci_msix_count(dev); 

	/* check configured values */
	want = ixgbe_tx_queues + ixgbe_rx_queues + ixgbe_other_queues;
	/*
	 *  We arent going to do anything fancy for now,
	 *  we either can meet desired config or we fail.
	 */
	if (val >= want) 
		val = want;
	else 
		return (ENXIO);

	/* Initialize the resource arrays */
	for (int i = 0; i < IXGBE_MSGS; i++, vector++) {
		adapter->rid[i] = vector;
		adapter->tag[i] = NULL;
		adapter->res[i] = NULL;
	}

	adapter->num_tx_queues = ixgbe_tx_queues;
	adapter->num_rx_queues = ixgbe_rx_queues;

	/* Now allocate the vectors */	
	if ((error = pci_alloc_msix(dev, &val)) == 0) {
		adapter->msix = 1;
		device_printf(dev,
		    "MSI/X enabled with %d vectors\n", val);
	} else {
		device_printf(dev,
		    "FAIL pci_alloc_msix() %d\n", error);
		return (error);
	}
	return (0);
}
#else	/* FreeBSD 6.2 */
static int
ixgbe_allocate_msix_resources(struct adapter *adapter)
{
	return (1); /* Force Legacy behavior for 6.2 */
}
#endif

static void
ixgbe_free_pci_resources(struct adapter * adapter)
{
	device_t dev = adapter->dev;
	int		i, loop;

	/*
	 * Legacy has this set to 0, but we need
	 * to run this once, so reset it.
	 */
	if (adapter->msix)
		loop = adapter->msix;
	else
		loop = 1;
	/*
	 * First release all the interrupt resources:
	 * 	notice that since these are just kept
	 *	in an array we can do the same logic
	 * 	whether its MSIX or just legacy.
	 */
	for (i = 0; i < loop; i++) {
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

#if __FreeBSD_version >= 700000
	pci_release_msi(dev);
#endif
	if (adapter->res_memory != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    IXGBE_MMBA, adapter->res_memory);

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
	uint16_t csum;

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
	adapter->hw.fc.original_type = ixgbe_fc_full;
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

	if (adapter->msix) /* RSS and HWCSUM not compatible */
		ifp->if_capabilities |= IFCAP_TSO4;
	else
		ifp->if_capabilities |= (IFCAP_HWCSUM | IFCAP_TSO4);
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;

	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixgbe_media_change,
		     ixgbe_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_10G_SR |
		    IFM_FDX, 0, NULL);
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

	tsize = roundup2(adapter->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), 4096);
	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (int i = 0; i < adapter->num_tx_queues; i++, txconf++) {
		char	name_string[16];
		/* Set up some basics */
		txr = &adapter->tx_rings[i];
		txr->adapter = adapter;
		txr->me = i;
		/*
		 * Initialize the TX side lock
		 *  -this has to change for multi tx
		 */
		snprintf(name_string, sizeof(name_string), "%s:tx",
		    device_get_nameunit(dev));
		mtx_init(&adapter->tx_mtx, name_string, NULL, MTX_DEF);

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
		/* clear the watch index */
		txbuf->next_eop = -1;
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
 *	NOTE: this will need to be changed if there are more than
 *	one transmit queues.
 **********************************************************************/
static void
ixgbe_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring *txr = adapter->tx_rings;
	uint64_t       tdba = txr->txdma.dma_paddr;

	/* Setup the Base and Length of the Tx Descriptor Ring */

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDBAL(0),
		       (tdba & 0x00000000ffffffffULL));
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDBAH(0), (tdba >> 32));
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDLEN(0),
		       adapter->num_tx_desc *
		       sizeof(struct ixgbe_legacy_tx_desc));

	/* Setup the HW Tx Head and Tail descriptor pointers */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDH(0), 0);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TDT(0), 0);

	IXGBE_WRITE_REG(&adapter->hw, IXGBE_TIPG, IXGBE_TIPG_FIBER_DEFAULT);

	/* Setup Transmit Descriptor Cmd Settings */
	txr->txd_cmd = IXGBE_TXD_CMD_IFCS;

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
	mtx_lock(&adapter->tx_mtx);
	for (int i = 0; i < adapter->num_tx_queues; i++, txr++) {
		ixgbe_free_transmit_buffers(txr);
		ixgbe_dma_free(adapter, &txr->txdma);
	}
	mtx_unlock(&adapter->tx_mtx);
	mtx_destroy(&adapter->tx_mtx);
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
ixgbe_tx_csum_setup(struct tx_ring *txr, struct mbuf *mp)
{
	struct adapter *adapter = txr->adapter;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixgbe_tx_buf        *tx_buffer;
	uint32_t vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	struct ether_vlan_header *eh;
	struct ip *ip;
	struct ip6_hdr *ip6;
	int  ehdrlen, ip_hlen;
	u16	etype;
	u8	ipproto;
	int ctxd = txr->next_avail_tx_desc;
#if __FreeBSD_version < 700000
	struct m_tag	*mtag;
#else
	u16 vtag = 0;
#endif


	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	** In advanced descriptors the vlan tag must 
	** be placed into the descriptor itself.
	*/
#if __FreeBSD_version < 700000
	mtag = VLAN_OUTPUT_TAG(ifp, mp);
	if (mtag != NULL)
		vlan_macip_lens |=
		    htole16(VLAN_TAG_VALUE(mtag)) << IXGBE_ADVTXD_VLAN_SHIFT;
#else
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}
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
			return FALSE;
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
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

#ifndef NO_82598_A0_SUPPORT
	if (adapter->hw.revision_id == 0)
		desc_flip(TXD);
#endif

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == adapter->num_tx_desc)
		ctxd = 0;
	txr->next_avail_tx_desc = ctxd;
	--txr->tx_avail;

        return TRUE;
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
	tx_buffer->next_eop = -1;

#ifndef NO_82598_A0_SUPPORT
	if (adapter->hw.revision_id == 0)
		desc_flip(TXD);
#endif

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
	int	first, last, done, num_avail;
	struct ixgbe_tx_buf *tx_buffer;
	struct ixgbe_legacy_tx_desc *tx_desc, *eop_desc;

	mtx_assert(&adapter->tx_mtx, MA_OWNED);

	if (txr->tx_avail == adapter->num_tx_desc)
		return FALSE;

	num_avail = txr->tx_avail;
	first = txr->next_tx_to_clean;

	tx_buffer = &txr->tx_buffers[first];
	/* For cleanup we just use legacy struct */
	tx_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
	last = tx_buffer->next_eop;
        if (last == -1)
		return FALSE;

	eop_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];

        /*
         * What this does is get the index of the
         * first descriptor AFTER the EOP of the
         * first packet, that way we can do the
         * simple comparison on the inner while loop
	 * below.
         */
        if (++last == adapter->num_tx_desc) last = 0;
        done = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
            BUS_DMASYNC_POSTREAD);

	while (eop_desc->upper.fields.status & IXGBE_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			tx_desc->buffer_addr = 0;
			num_avail++;

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
			tx_buffer->next_eop = -1;

			if (++first == adapter->num_tx_desc)
				first = 0;

			tx_buffer = &txr->tx_buffers[first];
			tx_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
		}
		/* See if we can continue to the next packet */
		last = tx_buffer->next_eop;
		if (last != -1) {
			eop_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];
			/* Get new done point */
			if (++last == adapter->num_tx_desc) last = 0;
			done = last;
		} else
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
		if (num_avail == adapter->num_tx_desc)
			adapter->watchdog_timer = 0;
		/* Some were cleaned, so reset timer */
		else if (num_avail == txr->tx_avail)
			adapter->watchdog_timer = IXGBE_TX_TIMEOUT;
	}

	txr->tx_avail = num_avail;
	return TRUE;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
ixgbe_get_buf(struct rx_ring *rxr, int i)
{
	struct adapter	*adapter = rxr->adapter;
	struct mbuf	*mp;
	bus_dmamap_t	map;
	int		nsegs, error, old, s = 0;
	int		size = MCLBYTES;


	bus_dma_segment_t	segs[1];
	struct ixgbe_rx_buf	*rxbuf;

	/* Are we going to Jumbo clusters? */
	if (adapter->bigbufs) {
		size = MJUMPAGESIZE;
		s = 1;
	};
	
	mp = m_getjcl(M_DONTWAIT, MT_DATA, M_PKTHDR, size);
	if (mp == NULL) {
		adapter->mbuf_alloc_failed++;
		return (ENOBUFS);
	}

	mp->m_len = mp->m_pkthdr.len = size;

	if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
		m_adj(mp, ETHER_ALIGN);

	/*
	 * Using memory from the mbuf cluster pool, invoke the bus_dma
	 * machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load_mbuf_sg(rxr->rxtag[s], rxr->spare_map[s],
	    mp, segs, &nsegs, BUS_DMA_NOWAIT);
	if (error) {
		m_free(mp);
		return (error);
	}

	/* Now check our target buffer for existing mapping */
	rxbuf = &rxr->rx_buffers[i];
	old = rxbuf->bigbuf;
	if (rxbuf->m_head != NULL)
		bus_dmamap_unload(rxr->rxtag[old], rxbuf->map[old]);

        map = rxbuf->map[old];
        rxbuf->map[s] = rxr->spare_map[s];
        rxr->spare_map[old] = map;
        bus_dmamap_sync(rxr->rxtag[s], rxbuf->map[s], BUS_DMASYNC_PREREAD);
        rxbuf->m_head = mp;
        rxbuf->bigbuf = s;

        rxr->rx_base[i].read.pkt_addr = htole64(segs[0].ds_addr);

#ifndef NO_82598_A0_SUPPORT
        /* A0 needs to One's Compliment descriptors */
	if (adapter->hw.revision_id == 0) {
        	struct dhack {u32 a1; u32 a2; u32 b1; u32 b2;};
        	struct dhack *d;   

        	d = (struct dhack *)&rxr->rx_base[i];
        	d->a1 = ~(d->a1);
        	d->a2 = ~(d->a2);
	}
#endif

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

	/* First make the small (2K) tag/map */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
				   PAGE_SIZE, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MCLBYTES,		/* maxsize */
				   1,			/* nsegments */
				   MCLBYTES,		/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->rxtag[0]))) {
		device_printf(dev, "Unable to create RX Small DMA tag\n");
		goto fail;
	}

	/* Next make the large (4K) tag/map */
	if ((error = bus_dma_tag_create(NULL,		/* parent */
				   PAGE_SIZE, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,		/* filter, filterarg */
				   MJUMPAGESIZE,	/* maxsize */
				   1,			/* nsegments */
				   MJUMPAGESIZE,	/* maxsegsize */
				   0,			/* flags */
				   NULL,		/* lockfunc */
				   NULL,		/* lockfuncarg */
				   &rxr->rxtag[1]))) {
		device_printf(dev, "Unable to create RX Large DMA tag\n");
		goto fail;
	}

	/* Create the spare maps (used by getbuf) */
        error = bus_dmamap_create(rxr->rxtag[0], BUS_DMA_NOWAIT,
	     &rxr->spare_map[0]);
        error = bus_dmamap_create(rxr->rxtag[1], BUS_DMA_NOWAIT,
	     &rxr->spare_map[1]);
	if (error) {
		device_printf(dev, "%s: bus_dmamap_create failed: %d\n",
		    __func__, error);
		goto fail;
	}

	for (i = 0; i < adapter->num_rx_desc; i++, rxbuf++) {
		rxbuf = &rxr->rx_buffers[i];
		error = bus_dmamap_create(rxr->rxtag[0],
		    BUS_DMA_NOWAIT, &rxbuf->map[0]);
		if (error) {
			device_printf(dev, "Unable to create Small RX DMA map\n");
			goto fail;
		}
		error = bus_dmamap_create(rxr->rxtag[1],
		    BUS_DMA_NOWAIT, &rxbuf->map[1]);
		if (error) {
			device_printf(dev, "Unable to create Large RX DMA map\n");
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
	struct ixgbe_rx_buf *rxbuf;
	int j, rsize, s;

	adapter = rxr->adapter;
	rsize = roundup2(adapter->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), 4096);
	/* Clear the ring contents */
	bzero((void *)rxr->rx_base, rsize);

	/*
	** Free current RX buffers: the size buffer
	** that is loaded is indicated by the buffer
	** bigbuf value.
	*/
	for (int i = 0; i < adapter->num_rx_desc; i++) {
		rxbuf = &rxr->rx_buffers[i];
		s = rxbuf->bigbuf;
		if (rxbuf->m_head != NULL) {
			bus_dmamap_sync(rxr->rxtag[s], rxbuf->map[s],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->rxtag[s], rxbuf->map[s]);
			m_freem(rxbuf->m_head);
			rxbuf->m_head = NULL;
		}
	}

	for (j = 0; j < adapter->num_rx_desc; j++) {
		if (ixgbe_get_buf(rxr, j) == ENOBUFS) {
			rxr->rx_buffers[j].m_head = NULL;
			rxr->rx_base[j].read.pkt_addr = 0;
			/* If we fail some may have change size */
			s = adapter->bigbufs;
			goto fail;
		}
	}

	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->last_cleaned = 0;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
fail:
	/*
	 * We need to clean up any buffers allocated so far
	 * 'j' is the failing index, decrement it to get the
	 * last success.
	 */
	for (--j; j < 0; j--) {
		rxbuf = &rxr->rx_buffers[j];
		if (rxbuf->m_head != NULL) {
			bus_dmamap_sync(rxr->rxtag[s], rxbuf->map[s],
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(rxr->rxtag[s], rxbuf->map[s]);
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
	int i, j, s;

	for (i = 0; i < adapter->num_rx_queues; i++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
			goto fail;

	return (0);
fail:
	/*
	 * Free RX buffers allocated so far, we will only handle
	 * the rings that completed, the failing case will have
	 * cleaned up for itself. The value of 'i' will be the
	 * failed ring so we must pre-decrement it.
	 */
	rxr = adapter->rx_rings;
	for (--i; i > 0; i--, rxr++) {
		for (j = 0; j < adapter->num_rx_desc; j++) {
			struct ixgbe_rx_buf *rxbuf;
			rxbuf = &rxr->rx_buffers[j];
			s = rxbuf->bigbuf;
			if (rxbuf->m_head != NULL) {
				bus_dmamap_sync(rxr->rxtag[s], rxbuf->map[s],
			  	  BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxtag[s], rxbuf->map[s]);
				m_freem(rxbuf->m_head);
				rxbuf->m_head = NULL;
			}
		}
	}

	return (ENOBUFS);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
static void
ixgbe_initialize_receive_units(struct adapter *adapter)
{
	struct	rx_ring	*rxr = adapter->rx_rings;
	struct ifnet   *ifp = adapter->ifp;
	u32		rxctrl, fctrl, srrctl, rxcsum;
	u32		reta, mrqc, hlreg, linkvec;
	u32		random[10];


	/*
	 * Make sure receives are disabled while
	 * setting up the descriptor ring
	 */
	rxctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXCTRL,
	    rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_FCTRL, fctrl);

	hlreg = IXGBE_READ_REG(&adapter->hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU)
		hlreg |= IXGBE_HLREG0_JUMBOEN;
	else
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_HLREG0, hlreg);

	srrctl = IXGBE_READ_REG(&adapter->hw, IXGBE_SRRCTL(0));
	srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
	srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
	if (adapter->bigbufs)
		srrctl |= 4096 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	else
		srrctl |= 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_SRRCTL(0), srrctl);

	/* Set Queue moderation rate */
	for (int i = 0; i < IXGBE_MSGS; i++)
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(i), DEFAULT_ITR);

	/* Set Link moderation lower */
	linkvec = adapter->num_tx_queues + adapter->num_rx_queues;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EITR(linkvec), LINK_ITR);

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;
		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDBAL(i),
			       (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDBAH(i), (rdba >> 32));
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDH(i), 0);
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDT(i),
		    adapter->num_rx_desc - 1);
	}

	if (adapter->num_rx_queues > 1) {
		/* set up random bits */
		arc4rand(&random, sizeof(random), 0);
		switch (adapter->num_rx_queues) {
			case 8:
			case 4:
				reta = 0x00010203;
				break;
			case 2:
				reta = 0x00010001;
				break;
			default:
				reta = 0x00000000;
		}

		/* Set up the redirection table */
		for (int i = 0; i < 32; i++) {
			IXGBE_WRITE_REG(&adapter->hw, IXGBE_RETA(i), reta);
			if (adapter->num_rx_queues > 4) {
				++i;
				IXGBE_WRITE_REG(&adapter->hw,
				    IXGBE_RETA(i), 0x04050607);
			}
		}

		/* Now fill our hash function seeds */
		for (int i = 0; i < 10; i++)
			IXGBE_WRITE_REG_ARRAY(&adapter->hw,
			    IXGBE_RSSRK(0), i, random[i]);

		mrqc = IXGBE_MRQC_RSSEN
		    /* Perform hash on these packet types */
		    | IXGBE_MRQC_RSS_FIELD_IPV4
		    | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX
		    | IXGBE_MRQC_RSS_FIELD_IPV6
		    | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_UDP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_MRQC, mrqc);

		/* RSS and RX IPP Checksum are mutually exclusive */
		rxcsum = IXGBE_READ_REG(&adapter->hw, IXGBE_RXCSUM);
		rxcsum |= IXGBE_RXCSUM_PCSD;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXCSUM, rxcsum);
	} else {
		rxcsum = IXGBE_READ_REG(&adapter->hw, IXGBE_RXCSUM);
		if (ifp->if_capenable & IFCAP_RXCSUM)
			rxcsum |= IXGBE_RXCSUM_IPPCSE;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXCSUM, rxcsum);
	}

	/* Enable Receive engine */
	rxctrl |= (IXGBE_RXCTRL_RXEN | IXGBE_RXCTRL_DMBYPS);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_RXCTRL, rxctrl);

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
		ixgbe_free_receive_buffers(rxr);
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
			int s = rxbuf->bigbuf;
			if (rxbuf->map != NULL) {
				bus_dmamap_unload(rxr->rxtag[s], rxbuf->map[s]);
				bus_dmamap_destroy(rxr->rxtag[s], rxbuf->map[s]);
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
	for (int s = 0; s < 2; s++) {
		if (rxr->rxtag[s] != NULL) {
			bus_dma_tag_destroy(rxr->rxtag[s]);
			rxr->rxtag[s] = NULL;
		}
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
 *********************************************************************/
static int
ixgbe_rxeof(struct rx_ring *rxr, int count)
{
	struct adapter 		*adapter = rxr->adapter;
	struct ifnet   		*ifp = adapter->ifp;
	struct mbuf    		*mp;
	int             	len, i, eop = 0;
	uint8_t			accept_frame = 0;
	uint32_t		staterr;
	union ixgbe_adv_rx_desc	*cur;


	i = rxr->next_to_check;
	cur = &rxr->rx_base[i];
	staterr = cur->wb.upper.status_error;

	if (!(staterr & IXGBE_RXD_STAT_DD))
		return (0);

	while ((staterr & IXGBE_RXD_STAT_DD) && (count != 0) &&
	    (ifp->if_drv_flags & IFF_DRV_RUNNING)) {
		struct mbuf *m = NULL;
		int s;

		mp = rxr->rx_buffers[i].m_head;
		s = rxr->rx_buffers[i].bigbuf;
		bus_dmamap_sync(rxr->rxtag[s], rxr->rx_buffers[i].map[s],
				BUS_DMASYNC_POSTREAD);
		accept_frame = 1;
		if (staterr & IXGBE_RXD_STAT_EOP) {
			count--;
			eop = 1;
		} else {
			eop = 0;
		}
		len = cur->wb.upper.length;

		if (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK)
			accept_frame = 0;

		if (accept_frame) {
			/* Get a fresh buffer first */
			if (ixgbe_get_buf(rxr, i) != 0) {
				ifp->if_iqdrops++;
				goto discard;
			}

			/* Assign correct length to the current fragment */
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
				rxr->packet_count++;
				rxr->byte_count += rxr->fmp->m_pkthdr.len;

				ixgbe_rx_checksum(adapter,
				    staterr, rxr->fmp);

				if (staterr & IXGBE_RXD_STAT_VP) {
#if __FreeBSD_version < 700000
					VLAN_INPUT_TAG_NEW(ifp, rxr->fmp,
					    (le16toh(cur->wb.upper.vlan) &
					    IXGBE_RX_DESC_SPECIAL_VLAN_MASK));
#else
					rxr->fmp->m_pkthdr.ether_vtag =
                                            le16toh(cur->wb.upper.vlan);
                                        rxr->fmp->m_flags |= M_VLANTAG;
#endif
				}
				m = rxr->fmp;
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
		} else {
			ifp->if_ierrors++;
discard:
			/* Reuse loaded DMA map and just update mbuf chain */
			mp = rxr->rx_buffers[i].m_head;
			mp->m_len = mp->m_pkthdr.len =
			    (rxr->rx_buffers[i].bigbuf ? MJUMPAGESIZE:MCLBYTES);
			mp->m_data = mp->m_ext.ext_buf;
			mp->m_next = NULL;
			if (adapter->max_frame_size <= (MCLBYTES - ETHER_ALIGN))
				m_adj(mp, ETHER_ALIGN);
			if (rxr->fmp != NULL) {
				m_freem(rxr->fmp);
				rxr->fmp = NULL;
				rxr->lmp = NULL;
			}
			m = NULL;
		}

		/* Zero out the receive descriptors status  */
		cur->wb.upper.status_error = 0;
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		rxr->last_cleaned = i; /* for updating tail */

		if (++i == adapter->num_rx_desc)
			i = 0;

		/* Now send up to the stack */
                if (m != NULL) {
                        rxr->next_to_check = i;
                        (*ifp->if_input)(ifp, m);
                        i = rxr->next_to_check;
                }
		/* Get next descriptor */
		cur = &rxr->rx_base[i];
		staterr = cur->wb.upper.status_error;
	}
	rxr->next_to_check = i;

	/* Advance the IXGB's Receive Queue "Tail Pointer" */
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_RDT(rxr->me), rxr->last_cleaned);

	if (!(staterr & IXGBE_RXD_STAT_DD))
		return (0);

	return (1);
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixgbe_rx_checksum(struct adapter *adapter,
    uint32_t staterr, struct mbuf * mp)
{
	uint16_t status = (uint16_t) staterr;
	uint8_t  errors = (uint8_t) (staterr >> 24);

	/* Not offloaded */
	if (status & IXGBE_RXD_STAT_IXSM) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}

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


static void
ixgbe_enable_vlans(struct adapter *adapter)
{
	uint32_t        ctrl;

	ixgbe_disable_intr(adapter);
	ctrl = IXGBE_READ_REG(&adapter->hw, IXGBE_VLNCTRL);
	ctrl |= IXGBE_VLNCTRL_VME;
	ctrl &= ~IXGBE_VLNCTRL_CFIEN;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VLNCTRL, ctrl);
	ixgbe_enable_intr(adapter);

	return;
}


static void
ixgbe_enable_intr(struct adapter *adapter)
{
	u32 mask;

	/* With RSS set up what to auto clear */
	if (adapter->msix) {
		mask = IXGBE_EIMS_ENABLE_MASK;
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIAC, mask);
	}

	mask = IXGBE_EIMS_ENABLE_MASK;
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMS, IXGBE_EIMS_ENABLE_MASK);
	IXGBE_WRITE_FLUSH(&adapter->hw);

	return;
}

static void
ixgbe_disable_intr(struct adapter *adapter)
{
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_EIMC, ~0);
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

static void
ixgbe_set_ivar(struct adapter *adapter, u16 entry, u8 vector)
{
	u32 ivar, index;
	vector |= IXGBE_IVAR_ALLOC_VAL;
	index = (entry >> 2) & 0x1F;
	ivar = IXGBE_READ_REG(&adapter->hw, IXGBE_IVAR(index));
	ivar |= (vector << (8 * (entry & 0x3)));
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_IVAR(index), ivar);
}

static void
ixgbe_configure_ivars(struct adapter *adapter)
{
	int	i, vec;

        for (i = 0, vec = 1; i < adapter->num_rx_queues; i++, vec++)
                ixgbe_set_ivar(adapter, IXGBE_IVAR_RX_QUEUE(i), vec);

        for (i = 0, vec = 8; i < adapter->num_tx_queues; i++, vec++)
		ixgbe_set_ivar(adapter, IXGBE_IVAR_TX_QUEUE(i), vec);

	/* For the Link interrupt */
        ixgbe_set_ivar(adapter, IXGBE_IVAR_OTHER_CAUSES_INDEX, 0);
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
ixgbe_update_stats_counters(struct adapter *adapter)
{
	struct ifnet   *ifp;
	struct ixgbe_hw *hw = &adapter->hw;
	u64  good_rx, missed_rx;

	adapter->stats.crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);

	good_rx = IXGBE_READ_REG(hw, IXGBE_GPRC);
	missed_rx  = IXGBE_READ_REG(hw, IXGBE_MPC(0));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(1));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(2));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(3));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(4));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(5));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(6));
	missed_rx += IXGBE_READ_REG(hw, IXGBE_MPC(7));

	adapter->stats.gprc += (good_rx - missed_rx);

        adapter->stats.mpc[0] += missed_rx;
	adapter->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
	adapter->stats.bprc += IXGBE_READ_REG(hw, IXGBE_BPRC);
	adapter->stats.mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	adapter->stats.mprc -= adapter->stats.bprc;
	adapter->stats.roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	adapter->stats.prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	adapter->stats.prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	adapter->stats.prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	adapter->stats.prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	adapter->stats.prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	adapter->stats.prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);

	adapter->stats.rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);
	adapter->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
	adapter->stats.lxontxc += IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	adapter->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);
	adapter->stats.lxofftxc += IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	adapter->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
	adapter->stats.rnbc[0] += IXGBE_READ_REG(hw, IXGBE_RNBC(0));
	adapter->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	adapter->stats.rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	adapter->stats.rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	adapter->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORH);
	adapter->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
	adapter->stats.tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	adapter->stats.ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	adapter->stats.ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	adapter->stats.ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	adapter->stats.ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	adapter->stats.ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	adapter->stats.ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	adapter->stats.mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	adapter->stats.bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);

	ifp = adapter->ifp;

	/* Fill out the OS statistics structure */
	ifp->if_ipackets = adapter->stats.gprc;
	ifp->if_opackets = adapter->stats.gptc;
	ifp->if_ibytes = adapter->stats.gorc;
	ifp->if_obytes = adapter->stats.gotc;
	ifp->if_imcasts = adapter->stats.mprc;
	ifp->if_collisions = 0;

	/* Rx Errors */
	ifp->if_ierrors =
		adapter->stats.mpc[0] +
		adapter->stats.crcerrs +
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


	device_printf(dev,"Tx Descriptors not Avail1 = %lu\n",
	       adapter->no_tx_desc_avail1);
	device_printf(dev,"Tx Descriptors not Avail2 = %lu\n",
	       adapter->no_tx_desc_avail2);
	device_printf(dev,"Std Mbuf Failed = %lu\n",
	       adapter->mbuf_alloc_failed);
	device_printf(dev,"Std Cluster Failed = %lu\n",
	       adapter->mbuf_cluster_failed);

	device_printf(dev,"Missed Packets = %llu\n",
	       (long long)adapter->stats.mpc[0]);
	device_printf(dev,"Receive length errors = %llu\n",
	       ((long long)adapter->stats.roc +
	       (long long)adapter->stats.ruc));
	device_printf(dev,"Crc errors = %llu\n",
	       (long long)adapter->stats.crcerrs);
	device_printf(dev,"Driver dropped packets = %lu\n",
	       adapter->dropped_pkts);

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
	struct ixgbe_hw *hw = &adapter->hw;
	uint8_t *hw_addr = adapter->hw.hw_addr;
 
	device_printf(dev,"Adapter hardware address = %p \n", hw_addr);
	device_printf(dev,"CTRL = 0x%x RXCTRL = 0x%x \n",
	    IXGBE_READ_REG(hw, IXGBE_TXDCTL(0)),
	    IXGBE_READ_REG(hw, IXGBE_RXCTRL)); 
	device_printf(dev,"RXDCTL(0) = 0x%x RXDCTL(1) = 0x%x"
	    " RXCTRL(2) = 0x%x \n",
	    IXGBE_READ_REG(hw, IXGBE_RXDCTL(0)),
	    IXGBE_READ_REG(hw, IXGBE_RXDCTL(1)),
	    IXGBE_READ_REG(hw, IXGBE_RXDCTL(2)));
	device_printf(dev,"SRRCTL(0) = 0x%x SRRCTL(1) = 0x%x"
	    " SRRCTL(2) = 0x%x \n",
	    IXGBE_READ_REG(hw, IXGBE_SRRCTL(0)),
	    IXGBE_READ_REG(hw, IXGBE_SRRCTL(1)),
	    IXGBE_READ_REG(hw, IXGBE_SRRCTL(2)));
	device_printf(dev,"EIMC = 0x%x EIMS = 0x%x\n",
	    IXGBE_READ_REG(hw, IXGBE_EIMC),
	    IXGBE_READ_REG(hw, IXGBE_EIMS));
	device_printf(dev,"Queue(0) tdh = %d, hw tdt = %d\n",
	    IXGBE_READ_REG(hw, IXGBE_TDH(0)),
	    IXGBE_READ_REG(hw, IXGBE_TDT(0)));
	device_printf(dev,"Error Byte Count = %u \n",
	    IXGBE_READ_REG(hw, IXGBE_ERRBC));

	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		device_printf(dev,"Queue %d Packets Received: %lu\n",
	    	    rxr->me, (long)rxr->packet_count);
	}

	rxr = adapter->rx_rings; // Reset
	for (int i = 0; i < adapter->num_rx_queues; i++, rxr++) {
		device_printf(dev,"Queue %d Bytes Received: %lu\n",
	    	    rxr->me, (long)rxr->byte_count);
	}

	for (int i = 0; i < adapter->num_rx_queues; i++) {
		device_printf(dev,"Queue[%d]: rdh = %d, hw rdt = %d\n",
	    	    i, IXGBE_READ_REG(hw, IXGBE_RDH(i)),
	    	    IXGBE_READ_REG(hw, IXGBE_RDT(i)));
	}

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
			adapter->hw.fc.original_type = ixgbe_flow_control;
			break;
		case ixgbe_fc_none:
		default:
			adapter->hw.fc.original_type = ixgbe_fc_none;
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

#ifndef NO_82598_A0_SUPPORT
/*
 * A0 Workaround: invert descriptor for hardware
 */
void
desc_flip(void *desc)
{
        struct dhack {u32 a1; u32 a2; u32 b1; u32 b2;};
        struct dhack *d;

        d = (struct dhack *)desc;
        d->a1 = ~(d->a1);
        d->a2 = ~(d->a2);
        d->b1 = ~(d->b1);
        d->b2 = ~(d->b2);
        d->b2 &= 0xFFFFFFF0;
        d->b1 &= ~IXGBE_ADVTXD_DCMD_RS;
}
#endif



