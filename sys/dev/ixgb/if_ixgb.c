/*******************************************************************************

Copyright (c) 2001-2004, Intel Corporation
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

#include <dev/ixgb/if_ixgb.h>

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int             ixgb_display_debug_stats = 0;

/*********************************************************************
 *  Linked list of board private structures for all NICs found
 *********************************************************************/

struct adapter *ixgb_adapter_list = NULL;



/*********************************************************************
 *  Driver version
 *********************************************************************/

char            ixgb_driver_version[] = "1.0.6";
char            ixgb_copyright[] = "Copyright (c) 2001-2004 Intel Corporation.";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixgb_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixgb_vendor_info_t ixgb_vendor_info_array[] =
{
	/* Intel(R) PRO/10000 Network Connection */
	{IXGB_VENDOR_ID, IXGB_DEVICE_ID_82597EX, PCI_ANY_ID, PCI_ANY_ID, 0},
	{IXGB_VENDOR_ID, IXGB_DEVICE_ID_82597EX_SR, PCI_ANY_ID, PCI_ANY_ID, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings for all supported NICs.
 *********************************************************************/

static char    *ixgb_strings[] = {
	"Intel(R) PRO/10GbE Network Driver"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixgb_probe(device_t);
static int      ixgb_attach(device_t);
static int      ixgb_detach(device_t);
static int      ixgb_shutdown(device_t);
static void     ixgb_intr(void *);
static void     ixgb_start(struct ifnet *);
static void     ixgb_start_locked(struct ifnet *);
static int      ixgb_ioctl(struct ifnet *, IOCTL_CMD_TYPE, caddr_t);
static void     ixgb_watchdog(struct adapter *);
static void     ixgb_init(void *);
static void     ixgb_init_locked(struct adapter *);
static void     ixgb_stop(void *);
static void     ixgb_media_status(struct ifnet *, struct ifmediareq *);
static int      ixgb_media_change(struct ifnet *);
static void     ixgb_identify_hardware(struct adapter *);
static int      ixgb_allocate_pci_resources(struct adapter *);
static void     ixgb_free_pci_resources(struct adapter *);
static void     ixgb_local_timer(void *);
static int      ixgb_hardware_init(struct adapter *);
static int      ixgb_setup_interface(device_t, struct adapter *);
static int      ixgb_setup_transmit_structures(struct adapter *);
static void     ixgb_initialize_transmit_unit(struct adapter *);
static int      ixgb_setup_receive_structures(struct adapter *);
static void     ixgb_initialize_receive_unit(struct adapter *);
static void     ixgb_enable_intr(struct adapter *);
static void     ixgb_disable_intr(struct adapter *);
static void     ixgb_free_transmit_structures(struct adapter *);
static void     ixgb_free_receive_structures(struct adapter *);
static void     ixgb_update_stats_counters(struct adapter *);
static void     ixgb_clean_transmit_interrupts(struct adapter *);
static int      ixgb_allocate_receive_structures(struct adapter *);
static int      ixgb_allocate_transmit_structures(struct adapter *);
static int      ixgb_process_receive_interrupts(struct adapter *, int);
static void 
ixgb_receive_checksum(struct adapter *,
		      struct ixgb_rx_desc * rx_desc,
		      struct mbuf *);
static void 
ixgb_transmit_checksum_setup(struct adapter *,
			     struct mbuf *,
			     u_int8_t *);
static void     ixgb_set_promisc(struct adapter *);
static void     ixgb_disable_promisc(struct adapter *);
static void     ixgb_set_multi(struct adapter *);
static void     ixgb_print_hw_stats(struct adapter *);
static void     ixgb_print_link_status(struct adapter *);
static int 
ixgb_get_buf(int i, struct adapter *,
	     struct mbuf *);
static void     ixgb_enable_vlans(struct adapter * adapter);
static int      ixgb_encap(struct adapter * adapter, struct mbuf * m_head);
static int      ixgb_sysctl_stats(SYSCTL_HANDLER_ARGS);
static int 
ixgb_dma_malloc(struct adapter *, bus_size_t,
		struct ixgb_dma_alloc *, int);
static void     ixgb_dma_free(struct adapter *, struct ixgb_dma_alloc *);
#ifdef DEVICE_POLLING
static poll_handler_t ixgb_poll;
#endif

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ixgb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixgb_probe),
	DEVMETHOD(device_attach, ixgb_attach),
	DEVMETHOD(device_detach, ixgb_detach),
	DEVMETHOD(device_shutdown, ixgb_shutdown),

	DEVMETHOD_END
};

static driver_t ixgb_driver = {
	"ixgb", ixgb_methods, sizeof(struct adapter),
};

static devclass_t ixgb_devclass;
DRIVER_MODULE(ixgb, pci, ixgb_driver, ixgb_devclass, 0, 0);

MODULE_DEPEND(ixgb, pci, 1, 1, 1);
MODULE_DEPEND(ixgb, ether, 1, 1, 1);

/* some defines for controlling descriptor fetches in h/w */
#define RXDCTL_PTHRESH_DEFAULT 128	/* chip considers prefech below this */
#define RXDCTL_HTHRESH_DEFAULT 16	/* chip will only prefetch if tail is
					 * pushed this many descriptors from
					 * head */
#define RXDCTL_WTHRESH_DEFAULT 0	/* chip writes back at this many or RXT0 */


/*********************************************************************
 *  Device identification routine
 *
 *  ixgb_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

static int
ixgb_probe(device_t dev)
{
	ixgb_vendor_info_t *ent;

	u_int16_t       pci_vendor_id = 0;
	u_int16_t       pci_device_id = 0;
	u_int16_t       pci_subvendor_id = 0;
	u_int16_t       pci_subdevice_id = 0;
	char            adapter_name[60];

	INIT_DEBUGOUT("ixgb_probe: begin");

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != IXGB_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixgb_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == PCI_ANY_ID)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == PCI_ANY_ID))) {
			sprintf(adapter_name, "%s, Version - %s",
				ixgb_strings[ent->index],
				ixgb_driver_version);
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
ixgb_attach(device_t dev)
{
	struct adapter *adapter;
	int             tsize, rsize;
	int             error = 0;

	device_printf(dev, "%s\n", ixgb_copyright);
	INIT_DEBUGOUT("ixgb_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	if (!(adapter = device_get_softc(dev))) {
		device_printf(dev, "adapter structure allocation failed\n");
		return (ENOMEM);
	}
	bzero(adapter, sizeof(struct adapter));
	adapter->dev = dev;
	adapter->osdep.dev = dev;
	IXGB_LOCK_INIT(adapter, device_get_nameunit(dev));

	if (ixgb_adapter_list != NULL)
		ixgb_adapter_list->prev = adapter;
	adapter->next = ixgb_adapter_list;
	ixgb_adapter_list = adapter;

	/* SYSCTL APIs */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "stats", CTLTYPE_INT | CTLFLAG_RW,
			(void *)adapter, 0,
			ixgb_sysctl_stats, "I", "Statistics");

	callout_init_mtx(&adapter->timer, &adapter->mtx, 0);

	/* Determine hardware revision */
	ixgb_identify_hardware(adapter);

	/* Parameters (to be read from user) */
	adapter->num_tx_desc = IXGB_MAX_TXD;
	adapter->num_rx_desc = IXGB_MAX_RXD;
	adapter->tx_int_delay = TIDV;
	adapter->rx_int_delay = RDTR;
	adapter->rx_buffer_len = IXGB_RXBUFFER_2048;

	adapter->hw.fc.high_water = FCRTH;
	adapter->hw.fc.low_water = FCRTL;
	adapter->hw.fc.pause_time = FCPAUSE;
	adapter->hw.fc.send_xon = TRUE;
	adapter->hw.fc.type = FLOW_CONTROL;


	/* Set the max frame size assuming standard ethernet sized frames */
	adapter->hw.max_frame_size =
		ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

	if (ixgb_allocate_pci_resources(adapter)) {
		device_printf(dev, "Allocation of PCI resources failed\n");
		error = ENXIO;
		goto err_pci;
	}
	tsize = IXGB_ROUNDUP(adapter->num_tx_desc *
			     sizeof(struct ixgb_tx_desc), 4096);

	/* Allocate Transmit Descriptor ring */
	if (ixgb_dma_malloc(adapter, tsize, &adapter->txdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate TxDescriptor memory\n");
		error = ENOMEM;
		goto err_tx_desc;
	}
	adapter->tx_desc_base = (struct ixgb_tx_desc *) adapter->txdma.dma_vaddr;

	rsize = IXGB_ROUNDUP(adapter->num_rx_desc *
			     sizeof(struct ixgb_rx_desc), 4096);

	/* Allocate Receive Descriptor ring */
	if (ixgb_dma_malloc(adapter, rsize, &adapter->rxdma, BUS_DMA_NOWAIT)) {
		device_printf(dev, "Unable to allocate rx_desc memory\n");
		error = ENOMEM;
		goto err_rx_desc;
	}
	adapter->rx_desc_base = (struct ixgb_rx_desc *) adapter->rxdma.dma_vaddr;

	/* Allocate multicast array memory. */
	adapter->mta = malloc(sizeof(u_int8_t) * IXGB_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (adapter->mta == NULL) {
		device_printf(dev, "Can not allocate multicast setup array\n");
		error = ENOMEM;
		goto err_hw_init;
	}

	/* Initialize the hardware */
	if (ixgb_hardware_init(adapter)) {
		device_printf(dev, "Unable to initialize the hardware\n");
		error = EIO;
		goto err_hw_init;
	}
	/* Setup OS specific network interface */
	if (ixgb_setup_interface(dev, adapter) != 0)
		goto err_hw_init;

	/* Initialize statistics */
	ixgb_clear_hw_cntrs(&adapter->hw);
	ixgb_update_stats_counters(adapter);

	INIT_DEBUGOUT("ixgb_attach: end");
	return (0);

err_hw_init:
	ixgb_dma_free(adapter, &adapter->rxdma);
err_rx_desc:
	ixgb_dma_free(adapter, &adapter->txdma);
err_tx_desc:
err_pci:
	if (adapter->ifp != NULL)
		if_free(adapter->ifp);
	ixgb_free_pci_resources(adapter);
	sysctl_ctx_free(&adapter->sysctl_ctx);
	free(adapter->mta, M_DEVBUF);
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
ixgb_detach(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ifnet   *ifp = adapter->ifp;

	INIT_DEBUGOUT("ixgb_detach: begin");

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING)
		ether_poll_deregister(ifp);
#endif

	IXGB_LOCK(adapter);
	adapter->in_detach = 1;

	ixgb_stop(adapter);
	IXGB_UNLOCK(adapter);

#if __FreeBSD_version < 500000
	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
#else
	ether_ifdetach(ifp);
#endif
	callout_drain(&adapter->timer);
	ixgb_free_pci_resources(adapter);
#if __FreeBSD_version >= 500000
	if_free(ifp);
#endif

	/* Free Transmit Descriptor ring */
	if (adapter->tx_desc_base) {
		ixgb_dma_free(adapter, &adapter->txdma);
		adapter->tx_desc_base = NULL;
	}
	/* Free Receive Descriptor ring */
	if (adapter->rx_desc_base) {
		ixgb_dma_free(adapter, &adapter->rxdma);
		adapter->rx_desc_base = NULL;
	}
	/* Remove from the adapter list */
	if (ixgb_adapter_list == adapter)
		ixgb_adapter_list = adapter->next;
	if (adapter->next != NULL)
		adapter->next->prev = adapter->prev;
	if (adapter->prev != NULL)
		adapter->prev->next = adapter->next;
	free(adapter->mta, M_DEVBUF);

	IXGB_LOCK_DESTROY(adapter);
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
ixgb_shutdown(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	IXGB_LOCK(adapter);
	ixgb_stop(adapter);
	IXGB_UNLOCK(adapter);
	return (0);
}


/*********************************************************************
 *  Transmit entry point
 *
 *  ixgb_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

static void
ixgb_start_locked(struct ifnet * ifp)
{
	struct mbuf    *m_head;
	struct adapter *adapter = ifp->if_softc;

	IXGB_LOCK_ASSERT(adapter);

	if (!adapter->link_active)
		return;

	while (ifp->if_snd.ifq_head != NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);

		if (m_head == NULL)
			break;

		if (ixgb_encap(adapter, m_head)) {
			ifp->if_drv_flags |= IFF_DRV_OACTIVE;
			IF_PREPEND(&ifp->if_snd, m_head);
			break;
		}
		/* Send a copy of the frame to the BPF listener */
#if __FreeBSD_version < 500000
		if (ifp->if_bpf)
			bpf_mtap(ifp, m_head);
#else
		ETHER_BPF_MTAP(ifp, m_head);
#endif
		/* Set timeout in case hardware has problems transmitting */
		adapter->tx_timer = IXGB_TX_TIMEOUT;

	}
	return;
}

static void
ixgb_start(struct ifnet *ifp)
{
	struct adapter *adapter = ifp->if_softc;

	IXGB_LOCK(adapter);
	ixgb_start_locked(ifp);
	IXGB_UNLOCK(adapter);
	return;
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixgb_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixgb_ioctl(struct ifnet * ifp, IOCTL_CMD_TYPE command, caddr_t data)
{
	int             mask, error = 0;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct adapter *adapter = ifp->if_softc;

	if (adapter->in_detach)
		goto out;

	switch (command) {
	case SIOCSIFADDR:
	case SIOCGIFADDR:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFADDR (Get/Set Interface Addr)");
		ether_ioctl(ifp, command, data);
		break;
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXGB_MAX_JUMBO_FRAME_SIZE - ETHER_HDR_LEN) {
			error = EINVAL;
		} else {
			IXGB_LOCK(adapter);
			ifp->if_mtu = ifr->ifr_mtu;
			adapter->hw.max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

			ixgb_init_locked(adapter);
			IXGB_UNLOCK(adapter);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
		IXGB_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				ixgb_init_locked(adapter);
			}
			ixgb_disable_promisc(adapter);
			ixgb_set_promisc(adapter);
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				ixgb_stop(adapter);
			}
		}
		IXGB_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGB_LOCK(adapter);
			ixgb_disable_intr(adapter);
			ixgb_set_multi(adapter);
			ixgb_enable_intr(adapter);
			IXGB_UNLOCK(adapter);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &adapter->media, command);
		break;
	case SIOCSIFCAP:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFCAP (Set Capabilities)");
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
#ifdef DEVICE_POLLING
		if (mask & IFCAP_POLLING) {
			if (ifr->ifr_reqcap & IFCAP_POLLING) {
				error = ether_poll_register(ixgb_poll, ifp);
				if (error)
					return(error);
				IXGB_LOCK(adapter);
				ixgb_disable_intr(adapter);
				ifp->if_capenable |= IFCAP_POLLING;
				IXGB_UNLOCK(adapter);
			} else {
				error = ether_poll_deregister(ifp);
				/* Enable interrupt even in error case */
				IXGB_LOCK(adapter);
				ixgb_enable_intr(adapter);
				ifp->if_capenable &= ~IFCAP_POLLING;
				IXGB_UNLOCK(adapter);
			}
		}
#endif /* DEVICE_POLLING */
		if (mask & IFCAP_HWCSUM) {
			if (IFCAP_HWCSUM & ifp->if_capenable)
				ifp->if_capenable &= ~IFCAP_HWCSUM;
			else
				ifp->if_capenable |= IFCAP_HWCSUM;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixgb_init(adapter);
		}
		break;
	default:
		IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%X)\n", (int)command);
		error = EINVAL;
	}

out:
	return (error);
}

/*********************************************************************
 *  Watchdog entry point
 *
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

static void
ixgb_watchdog(struct adapter *adapter)
{
	struct ifnet *ifp;

	ifp = adapter->ifp;

	/*
	 * If we are in this routine because of pause frames, then don't
	 * reset the hardware.
	 */
	if (IXGB_READ_REG(&adapter->hw, STATUS) & IXGB_STATUS_TXOFF) {
		adapter->tx_timer = IXGB_TX_TIMEOUT;
		return;
	}
	if_printf(ifp, "watchdog timeout -- resetting\n");

	ixgb_stop(adapter);
	ixgb_init_locked(adapter);


	ifp->if_oerrors++;

	return;
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
ixgb_init_locked(struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("ixgb_init: begin");

	IXGB_LOCK_ASSERT(adapter);

	ixgb_stop(adapter);
	ifp = adapter->ifp;

	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(ifp), adapter->hw.curr_mac_addr,
	    IXGB_ETH_LENGTH_OF_ADDRESS);

	/* Initialize the hardware */
	if (ixgb_hardware_init(adapter)) {
		if_printf(ifp, "Unable to initialize the hardware\n");
		return;
	}
	ixgb_enable_vlans(adapter);

	/* Prepare transmit descriptors and buffers */
	if (ixgb_setup_transmit_structures(adapter)) {
		if_printf(ifp, "Could not setup transmit structures\n");
		ixgb_stop(adapter);
		return;
	}
	ixgb_initialize_transmit_unit(adapter);

	/* Setup Multicast table */
	ixgb_set_multi(adapter);

	/* Prepare receive descriptors and buffers */
	if (ixgb_setup_receive_structures(adapter)) {
		if_printf(ifp, "Could not setup receive structures\n");
		ixgb_stop(adapter);
		return;
	}
	ixgb_initialize_receive_unit(adapter);

	/* Don't lose promiscuous settings */
	ixgb_set_promisc(adapter);

	ifp = adapter->ifp;
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;


	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist = IXGB_CHECKSUM_FEATURES;
	else
		ifp->if_hwassist = 0;


	/* Enable jumbo frames */
	if (ifp->if_mtu > ETHERMTU) {
		uint32_t        temp_reg;
		IXGB_WRITE_REG(&adapter->hw, MFS,
			       adapter->hw.max_frame_size << IXGB_MFS_SHIFT);
		temp_reg = IXGB_READ_REG(&adapter->hw, CTRL0);
		temp_reg |= IXGB_CTRL0_JFE;
		IXGB_WRITE_REG(&adapter->hw, CTRL0, temp_reg);
	}
	callout_reset(&adapter->timer, hz, ixgb_local_timer, adapter);
	ixgb_clear_hw_cntrs(&adapter->hw);
#ifdef DEVICE_POLLING
	/*
	 * Only disable interrupts if we are polling, make sure they are on
	 * otherwise.
	 */
	if (ifp->if_capenable & IFCAP_POLLING)
		ixgb_disable_intr(adapter);
	else
#endif
		ixgb_enable_intr(adapter);

	return;
}

static void
ixgb_init(void *arg)
{
	struct adapter *adapter = arg;

	IXGB_LOCK(adapter);
	ixgb_init_locked(adapter);
	IXGB_UNLOCK(adapter);
	return;
}

#ifdef DEVICE_POLLING
static int
ixgb_poll_locked(struct ifnet * ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	u_int32_t       reg_icr;
	int		rx_npkts;

	IXGB_LOCK_ASSERT(adapter);

	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = IXGB_READ_REG(&adapter->hw, ICR);
		if (reg_icr & (IXGB_INT_RXSEQ | IXGB_INT_LSC)) {
			ixgb_check_for_link(&adapter->hw);
			ixgb_print_link_status(adapter);
		}
	}
	rx_npkts = ixgb_process_receive_interrupts(adapter, count);
	ixgb_clean_transmit_interrupts(adapter);

	if (ifp->if_snd.ifq_head != NULL)
		ixgb_start_locked(ifp);
	return (rx_npkts);
}

static int
ixgb_poll(struct ifnet * ifp, enum poll_cmd cmd, int count)
{
	struct adapter *adapter = ifp->if_softc;
	int rx_npkts = 0;

	IXGB_LOCK(adapter);
	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		rx_npkts = ixgb_poll_locked(ifp, cmd, count);
	IXGB_UNLOCK(adapter);
	return (rx_npkts);
}
#endif /* DEVICE_POLLING */

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 **********************************************************************/

static void
ixgb_intr(void *arg)
{
	u_int32_t       loop_cnt = IXGB_MAX_INTR;
	u_int32_t       reg_icr;
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	boolean_t       rxdmt0 = FALSE;

	IXGB_LOCK(adapter);

	ifp = adapter->ifp;

#ifdef DEVICE_POLLING
	if (ifp->if_capenable & IFCAP_POLLING) {
		IXGB_UNLOCK(adapter);
		return;
	}
#endif

	reg_icr = IXGB_READ_REG(&adapter->hw, ICR);
	if (reg_icr == 0) {
		IXGB_UNLOCK(adapter);
		return;
	}

	if (reg_icr & IXGB_INT_RXDMT0)
		rxdmt0 = TRUE;

#ifdef _SV_
	if (reg_icr & IXGB_INT_RXDMT0)
		adapter->sv_stats.icr_rxdmt0++;
	if (reg_icr & IXGB_INT_RXO)
		adapter->sv_stats.icr_rxo++;
	if (reg_icr & IXGB_INT_RXT0)
		adapter->sv_stats.icr_rxt0++;
	if (reg_icr & IXGB_INT_TXDW)
		adapter->sv_stats.icr_TXDW++;
#endif				/* _SV_ */

	/* Link status change */
	if (reg_icr & (IXGB_INT_RXSEQ | IXGB_INT_LSC)) {
		ixgb_check_for_link(&adapter->hw);
		ixgb_print_link_status(adapter);
	}
	while (loop_cnt > 0) {
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ixgb_process_receive_interrupts(adapter, -1);
			ixgb_clean_transmit_interrupts(adapter);
		}
		loop_cnt--;
	}

	if (rxdmt0 && adapter->raidc) {
		IXGB_WRITE_REG(&adapter->hw, IMC, IXGB_INT_RXDMT0);
		IXGB_WRITE_REG(&adapter->hw, IMS, IXGB_INT_RXDMT0);
	}
	if (ifp->if_drv_flags & IFF_DRV_RUNNING && ifp->if_snd.ifq_head != NULL)
		ixgb_start_locked(ifp);

	IXGB_UNLOCK(adapter);
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
ixgb_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct adapter *adapter = ifp->if_softc;

	INIT_DEBUGOUT("ixgb_media_status: begin");

	ixgb_check_for_link(&adapter->hw);
	ixgb_print_link_status(adapter);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!adapter->hw.link_up)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;
	ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;

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
ixgb_media_change(struct ifnet * ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia *ifm = &adapter->media;

	INIT_DEBUGOUT("ixgb_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixgb_encap(struct adapter * adapter, struct mbuf * m_head)
{
	u_int8_t        txd_popts;
	int             i, j, error, nsegs;

#if __FreeBSD_version < 500000
	struct ifvlan  *ifv = NULL;
#endif
	bus_dma_segment_t segs[IXGB_MAX_SCATTER];
	bus_dmamap_t	map;
	struct ixgb_buffer *tx_buffer = NULL;
	struct ixgb_tx_desc *current_tx_desc = NULL;
	struct ifnet   *ifp = adapter->ifp;

	/*
	 * Force a cleanup if number of TX descriptors available hits the
	 * threshold
	 */
	if (adapter->num_tx_desc_avail <= IXGB_TX_CLEANUP_THRESHOLD) {
		ixgb_clean_transmit_interrupts(adapter);
	}
	if (adapter->num_tx_desc_avail <= IXGB_TX_CLEANUP_THRESHOLD) {
		adapter->no_tx_desc_avail1++;
		return (ENOBUFS);
	}
	/*
	 * Map the packet for DMA.
	 */
	if (bus_dmamap_create(adapter->txtag, BUS_DMA_NOWAIT, &map)) {
		adapter->no_tx_map_avail++;
		return (ENOMEM);
	}
	error = bus_dmamap_load_mbuf_sg(adapter->txtag, map, m_head, segs,
					&nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		adapter->no_tx_dma_setup++;
		if_printf(ifp, "ixgb_encap: bus_dmamap_load_mbuf failed; "
		       "error %u\n", error);
		bus_dmamap_destroy(adapter->txtag, map);
		return (error);
	}
	KASSERT(nsegs != 0, ("ixgb_encap: empty packet"));

	if (nsegs > adapter->num_tx_desc_avail) {
		adapter->no_tx_desc_avail2++;
		bus_dmamap_destroy(adapter->txtag, map);
		return (ENOBUFS);
	}
	if (ifp->if_hwassist > 0) {
		ixgb_transmit_checksum_setup(adapter, m_head,
					     &txd_popts);
	} else
		txd_popts = 0;

	/* Find out if we are in vlan mode */
#if __FreeBSD_version < 500000
	if ((m_head->m_flags & (M_PROTO1 | M_PKTHDR)) == (M_PROTO1 | M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL &&
	    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#elseif __FreeBSD_version < 700000
	mtag = VLAN_OUTPUT_TAG(ifp, m_head);
#endif
	i = adapter->next_avail_tx_desc;
	for (j = 0; j < nsegs; j++) {
		tx_buffer = &adapter->tx_buffer_area[i];
		current_tx_desc = &adapter->tx_desc_base[i];

		current_tx_desc->buff_addr = htole64(segs[j].ds_addr);
		current_tx_desc->cmd_type_len = (adapter->txd_cmd | segs[j].ds_len);
		current_tx_desc->popts = txd_popts;
		if (++i == adapter->num_tx_desc)
			i = 0;

		tx_buffer->m_head = NULL;
	}

	adapter->num_tx_desc_avail -= nsegs;
	adapter->next_avail_tx_desc = i;

#if __FreeBSD_version < 500000
	if (ifv != NULL) {
		/* Set the vlan id */
		current_tx_desc->vlan = ifv->ifv_tag;
#elseif __FreeBSD_version < 700000
	if (mtag != NULL) {
		/* Set the vlan id */
		current_tx_desc->vlan = VLAN_TAG_VALUE(mtag);
#else
	if (m_head->m_flags & M_VLANTAG) {
		current_tx_desc->vlan = m_head->m_pkthdr.ether_vtag;
#endif

		/* Tell hardware to add tag */
		current_tx_desc->cmd_type_len |= IXGB_TX_DESC_CMD_VLE;
	}
	tx_buffer->m_head = m_head;
	tx_buffer->map = map;
	bus_dmamap_sync(adapter->txtag, map, BUS_DMASYNC_PREWRITE);

	/*
	 * Last Descriptor of Packet needs End Of Packet (EOP)
	 */
	current_tx_desc->cmd_type_len |= (IXGB_TX_DESC_CMD_EOP);

	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
	 * that this frame is available to transmit.
	 */
	IXGB_WRITE_REG(&adapter->hw, TDT, i);

	return (0);
}

static void
ixgb_set_promisc(struct adapter * adapter)
{

	u_int32_t       reg_rctl;
	struct ifnet   *ifp = adapter->ifp;

	reg_rctl = IXGB_READ_REG(&adapter->hw, RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (IXGB_RCTL_UPE | IXGB_RCTL_MPE);
		IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= IXGB_RCTL_MPE;
		reg_rctl &= ~IXGB_RCTL_UPE;
		IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
	}
	return;
}

static void
ixgb_disable_promisc(struct adapter * adapter)
{
	u_int32_t       reg_rctl;

	reg_rctl = IXGB_READ_REG(&adapter->hw, RCTL);

	reg_rctl &= (~IXGB_RCTL_UPE);
	reg_rctl &= (~IXGB_RCTL_MPE);
	IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl);

	return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

static void
ixgb_set_multi(struct adapter * adapter)
{
	u_int32_t       reg_rctl = 0;
	u_int8_t        *mta;
	struct ifmultiaddr *ifma;
	int             mcnt = 0;
	struct ifnet   *ifp = adapter->ifp;

	IOCTL_DEBUGOUT("ixgb_set_multi: begin");

	mta = adapter->mta;
	bzero(mta, sizeof(u_int8_t) * IXGB_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES);

	if_maddr_rlock(ifp);
#if __FreeBSD_version < 500000
	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		bcopy(LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		      &mta[mcnt * IXGB_ETH_LENGTH_OF_ADDRESS], IXGB_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
	if_maddr_runlock(ifp);

	if (mcnt > MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = IXGB_READ_REG(&adapter->hw, RCTL);
		reg_rctl |= IXGB_RCTL_MPE;
		IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl);
	} else
		ixgb_mc_addr_list_update(&adapter->hw, mta, mcnt, 0);

	return;
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status and updates statistics.
 *
 **********************************************************************/

static void
ixgb_local_timer(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	ifp = adapter->ifp;

	IXGB_LOCK_ASSERT(adapter);

	ixgb_check_for_link(&adapter->hw);
	ixgb_print_link_status(adapter);
	ixgb_update_stats_counters(adapter);
	if (ixgb_display_debug_stats && ifp->if_drv_flags & IFF_DRV_RUNNING) {
		ixgb_print_hw_stats(adapter);
	}
	if (adapter->tx_timer != 0 && --adapter->tx_timer == 0)
		ixgb_watchdog(adapter);
	callout_reset(&adapter->timer, hz, ixgb_local_timer, adapter);
}

static void
ixgb_print_link_status(struct adapter * adapter)
{
	if (adapter->hw.link_up) {
		if (!adapter->link_active) {
			if_printf(adapter->ifp, "Link is up %d Mbps %s \n",
			       10000,
			       "Full Duplex");
			adapter->link_active = 1;
		}
	} else {
		if (adapter->link_active) {
			if_printf(adapter->ifp, "Link is Down \n");
			adapter->link_active = 0;
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
ixgb_stop(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	ifp = adapter->ifp;

	IXGB_LOCK_ASSERT(adapter);

	INIT_DEBUGOUT("ixgb_stop: begin\n");
	ixgb_disable_intr(adapter);
	adapter->hw.adapter_stopped = FALSE;
	ixgb_adapter_stop(&adapter->hw);
	callout_stop(&adapter->timer);
	ixgb_free_transmit_structures(adapter);
	ixgb_free_receive_structures(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);
	adapter->tx_timer = 0;

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
ixgb_identify_hardware(struct adapter * adapter)
{
	device_t        dev = adapter->dev;

	/* Make sure our PCI config space has the necessary stuff set */
	pci_enable_busmaster(dev);
	adapter->hw.pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);

	/* Save off the information about this board */
	adapter->hw.vendor_id = pci_get_vendor(dev);
	adapter->hw.device_id = pci_get_device(dev);
	adapter->hw.revision_id = pci_read_config(dev, PCIR_REVID, 1);
	adapter->hw.subsystem_vendor_id = pci_read_config(dev, PCIR_SUBVEND_0, 2);
	adapter->hw.subsystem_id = pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* Set MacType, etc. based on this PCI info */
	switch (adapter->hw.device_id) {
	case IXGB_DEVICE_ID_82597EX:
	case IXGB_DEVICE_ID_82597EX_SR:
		adapter->hw.mac_type = ixgb_82597;
		break;
	default:
		INIT_DEBUGOUT1("Unknown device if 0x%x", adapter->hw.device_id);
		device_printf(dev, "unsupported device id 0x%x\n",
		    adapter->hw.device_id);
	}

	return;
}

static int
ixgb_allocate_pci_resources(struct adapter * adapter)
{
	int             rid;
	device_t        dev = adapter->dev;

	rid = IXGB_MMBA;
	adapter->res_memory = bus_alloc_resource(dev, SYS_RES_MEMORY,
						 &rid, 0, ~0, 1,
						 RF_ACTIVE);
	if (!(adapter->res_memory)) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}
	adapter->osdep.mem_bus_space_tag =
		rman_get_bustag(adapter->res_memory);
	adapter->osdep.mem_bus_space_handle =
		rman_get_bushandle(adapter->res_memory);
	adapter->hw.hw_addr = (uint8_t *) & adapter->osdep.mem_bus_space_handle;

	rid = 0x0;
	adapter->res_interrupt = bus_alloc_resource(dev, SYS_RES_IRQ,
						    &rid, 0, ~0, 1,
						  RF_SHAREABLE | RF_ACTIVE);
	if (!(adapter->res_interrupt)) {
		device_printf(dev,
		    "Unable to allocate bus resource: interrupt\n");
		return (ENXIO);
	}
	if (bus_setup_intr(dev, adapter->res_interrupt,
			   INTR_TYPE_NET | INTR_MPSAFE,
			   NULL, (void (*) (void *))ixgb_intr, adapter,
			   &adapter->int_handler_tag)) {
		device_printf(dev, "Error registering interrupt handler!\n");
		return (ENXIO);
	}
	adapter->hw.back = &adapter->osdep;

	return (0);
}

static void
ixgb_free_pci_resources(struct adapter * adapter)
{
	device_t        dev = adapter->dev;

	if (adapter->res_interrupt != NULL) {
		bus_teardown_intr(dev, adapter->res_interrupt,
				  adapter->int_handler_tag);
		bus_release_resource(dev, SYS_RES_IRQ, 0,
				     adapter->res_interrupt);
	}
	if (adapter->res_memory != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, IXGB_MMBA,
				     adapter->res_memory);
	}
	if (adapter->res_ioport != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, adapter->io_rid,
				     adapter->res_ioport);
	}
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
ixgb_hardware_init(struct adapter * adapter)
{
	/* Issue a global reset */
	adapter->hw.adapter_stopped = FALSE;
	ixgb_adapter_stop(&adapter->hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (!ixgb_validate_eeprom_checksum(&adapter->hw)) {
		device_printf(adapter->dev,
		    "The EEPROM Checksum Is Not Valid\n");
		return (EIO);
	}
	if (!ixgb_init_hw(&adapter->hw)) {
		device_printf(adapter->dev, "Hardware Initialization Failed");
		return (EIO);
	}

	return (0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
ixgb_setup_interface(device_t dev, struct adapter * adapter)
{
	struct ifnet   *ifp;
	INIT_DEBUGOUT("ixgb_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
#if __FreeBSD_version >= 502000
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
#else
	ifp->if_unit = device_get_unit(dev);
	ifp->if_name = "ixgb";
#endif
	ifp->if_baudrate = 1000000000;
	ifp->if_init = ixgb_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixgb_ioctl;
	ifp->if_start = ixgb_start;
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 1;

#if __FreeBSD_version < 500000
	ether_ifattach(ifp, ETHER_BPF_SUPPORTED);
#else
	ether_ifattach(ifp, adapter->hw.curr_mac_addr);
#endif

	ifp->if_capabilities = IFCAP_HWCSUM;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ether_vlan_header);

#if __FreeBSD_version >= 500000
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
#endif

	ifp->if_capenable = ifp->if_capabilities;

#ifdef DEVICE_POLLING
	ifp->if_capabilities |= IFCAP_POLLING;
#endif

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixgb_media_change,
		     ixgb_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX | IFM_FDX,
		    0, NULL);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_1000_SX,
		    0, NULL);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return (0);
}

/********************************************************************
 * Manage DMA'able memory.
 *******************************************************************/
static void
ixgb_dmamap_cb(void *arg, bus_dma_segment_t * segs, int nseg, int error)
{
	if (error)
		return;
	*(bus_addr_t *) arg = segs->ds_addr;
	return;
}

static int
ixgb_dma_malloc(struct adapter * adapter, bus_size_t size,
		struct ixgb_dma_alloc * dma, int mapflags)
{
	device_t dev;
	int             r;

	dev = adapter->dev;
	r = bus_dma_tag_create(bus_get_dma_tag(dev),	/* parent */
			       PAGE_SIZE, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       size,	/* maxsize */
			       1,	/* nsegments */
			       size,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
#if __FreeBSD_version >= 502000
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
#endif
			       &dma->dma_tag);
	if (r != 0) {
		device_printf(dev, "ixgb_dma_malloc: bus_dma_tag_create failed; "
		       "error %u\n", r);
		goto fail_0;
	}
	r = bus_dmamem_alloc(dma->dma_tag, (void **)&dma->dma_vaddr,
			     BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		device_printf(dev, "ixgb_dma_malloc: bus_dmamem_alloc failed; "
		       "error %u\n", r);
		goto fail_1;
	}
	r = bus_dmamap_load(dma->dma_tag, dma->dma_map, dma->dma_vaddr,
			    size,
			    ixgb_dmamap_cb,
			    &dma->dma_paddr,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		device_printf(dev, "ixgb_dma_malloc: bus_dmamap_load failed; "
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
	dma->dma_tag = NULL;
	return (r);
}



static void
ixgb_dma_free(struct adapter * adapter, struct ixgb_dma_alloc * dma)
{
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_free(dma->dma_tag, dma->dma_vaddr, dma->dma_map);
	bus_dma_tag_destroy(dma->dma_tag);
}

/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire.
 *
 **********************************************************************/
static int
ixgb_allocate_transmit_structures(struct adapter * adapter)
{
	if (!(adapter->tx_buffer_area =
	      (struct ixgb_buffer *) malloc(sizeof(struct ixgb_buffer) *
					    adapter->num_tx_desc, M_DEVBUF,
					    M_NOWAIT | M_ZERO))) {
		device_printf(adapter->dev,
		    "Unable to allocate tx_buffer memory\n");
		return ENOMEM;
	}
	bzero(adapter->tx_buffer_area,
	      sizeof(struct ixgb_buffer) * adapter->num_tx_desc);

	return 0;
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures.
 *
 **********************************************************************/
static int
ixgb_setup_transmit_structures(struct adapter * adapter)
{
	/*
	 * Setup DMA descriptor areas.
	 */
	if (bus_dma_tag_create(bus_get_dma_tag(adapter->dev),	/* parent */
			       PAGE_SIZE, 0,	/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,	/* filter, filterarg */
			       MCLBYTES * IXGB_MAX_SCATTER,	/* maxsize */
			       IXGB_MAX_SCATTER,	/* nsegments */
			       MCLBYTES,	/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
#if __FreeBSD_version >= 502000
			       NULL,	/* lockfunc */
			       NULL,	/* lockfuncarg */
#endif
			       &adapter->txtag)) {
		device_printf(adapter->dev, "Unable to allocate TX DMA tag\n");
		return (ENOMEM);
	}
	if (ixgb_allocate_transmit_structures(adapter))
		return ENOMEM;

	bzero((void *)adapter->tx_desc_base,
	      (sizeof(struct ixgb_tx_desc)) * adapter->num_tx_desc);

	adapter->next_avail_tx_desc = 0;
	adapter->oldest_used_tx_desc = 0;

	/* Set number of descriptors available */
	adapter->num_tx_desc_avail = adapter->num_tx_desc;

	/* Set checksum context */
	adapter->active_checksum_context = OFFLOAD_NONE;

	return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
ixgb_initialize_transmit_unit(struct adapter * adapter)
{
	u_int32_t       reg_tctl;
	u_int64_t       tdba = adapter->txdma.dma_paddr;

	/* Setup the Base and Length of the Tx Descriptor Ring */
	IXGB_WRITE_REG(&adapter->hw, TDBAL,
		       (tdba & 0x00000000ffffffffULL));
	IXGB_WRITE_REG(&adapter->hw, TDBAH, (tdba >> 32));
	IXGB_WRITE_REG(&adapter->hw, TDLEN,
		       adapter->num_tx_desc *
		       sizeof(struct ixgb_tx_desc));

	/* Setup the HW Tx Head and Tail descriptor pointers */
	IXGB_WRITE_REG(&adapter->hw, TDH, 0);
	IXGB_WRITE_REG(&adapter->hw, TDT, 0);


	HW_DEBUGOUT2("Base = %x, Length = %x\n",
		     IXGB_READ_REG(&adapter->hw, TDBAL),
		     IXGB_READ_REG(&adapter->hw, TDLEN));

	IXGB_WRITE_REG(&adapter->hw, TIDV, adapter->tx_int_delay);


	/* Program the Transmit Control Register */
	reg_tctl = IXGB_READ_REG(&adapter->hw, TCTL);
	reg_tctl = IXGB_TCTL_TCE | IXGB_TCTL_TXEN | IXGB_TCTL_TPDE;
	IXGB_WRITE_REG(&adapter->hw, TCTL, reg_tctl);

	/* Setup Transmit Descriptor Settings for this adapter */
	adapter->txd_cmd = IXGB_TX_DESC_TYPE | IXGB_TX_DESC_CMD_RS;

	if (adapter->tx_int_delay > 0)
		adapter->txd_cmd |= IXGB_TX_DESC_CMD_IDE;
	return;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
static void
ixgb_free_transmit_structures(struct adapter * adapter)
{
	struct ixgb_buffer *tx_buffer;
	int             i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (adapter->tx_buffer_area != NULL) {
		tx_buffer = adapter->tx_buffer_area;
		for (i = 0; i < adapter->num_tx_desc; i++, tx_buffer++) {
			if (tx_buffer->m_head != NULL) {
				bus_dmamap_unload(adapter->txtag, tx_buffer->map);
				bus_dmamap_destroy(adapter->txtag, tx_buffer->map);
				m_freem(tx_buffer->m_head);
			}
			tx_buffer->m_head = NULL;
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
	return;
}

/*********************************************************************
 *
 *  The offload context needs to be set when we transfer the first
 *  packet of a particular protocol (TCP/UDP). We change the
 *  context only if the protocol type changes.
 *
 **********************************************************************/
static void
ixgb_transmit_checksum_setup(struct adapter * adapter,
			     struct mbuf * mp,
			     u_int8_t * txd_popts)
{
	struct ixgb_context_desc *TXD;
	struct ixgb_buffer *tx_buffer;
	int             curr_txd;

	if (mp->m_pkthdr.csum_flags) {

		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_popts = IXGB_TX_DESC_POPTS_TXSM;
			if (adapter->active_checksum_context == OFFLOAD_TCP_IP)
				return;
			else
				adapter->active_checksum_context = OFFLOAD_TCP_IP;
		} else if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
			*txd_popts = IXGB_TX_DESC_POPTS_TXSM;
			if (adapter->active_checksum_context == OFFLOAD_UDP_IP)
				return;
			else
				adapter->active_checksum_context = OFFLOAD_UDP_IP;
		} else {
			*txd_popts = 0;
			return;
		}
	} else {
		*txd_popts = 0;
		return;
	}

	/*
	 * If we reach this point, the checksum offload context needs to be
	 * reset.
	 */
	curr_txd = adapter->next_avail_tx_desc;
	tx_buffer = &adapter->tx_buffer_area[curr_txd];
	TXD = (struct ixgb_context_desc *) & adapter->tx_desc_base[curr_txd];


	TXD->tucss = ENET_HEADER_SIZE + sizeof(struct ip);
	TXD->tucse = 0;

	TXD->mss = 0;

	if (adapter->active_checksum_context == OFFLOAD_TCP_IP) {
		TXD->tucso =
			ENET_HEADER_SIZE + sizeof(struct ip) +
			offsetof(struct tcphdr, th_sum);
	} else if (adapter->active_checksum_context == OFFLOAD_UDP_IP) {
		TXD->tucso =
			ENET_HEADER_SIZE + sizeof(struct ip) +
			offsetof(struct udphdr, uh_sum);
	}
	TXD->cmd_type_len = IXGB_CONTEXT_DESC_CMD_TCP | IXGB_TX_DESC_CMD_RS | IXGB_CONTEXT_DESC_CMD_IDE;

	tx_buffer->m_head = NULL;

	if (++curr_txd == adapter->num_tx_desc)
		curr_txd = 0;

	adapter->num_tx_desc_avail--;
	adapter->next_avail_tx_desc = curr_txd;
	return;
}

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
static void
ixgb_clean_transmit_interrupts(struct adapter * adapter)
{
	int             i, num_avail;
	struct ixgb_buffer *tx_buffer;
	struct ixgb_tx_desc *tx_desc;

	IXGB_LOCK_ASSERT(adapter);

	if (adapter->num_tx_desc_avail == adapter->num_tx_desc)
		return;

#ifdef _SV_
	adapter->clean_tx_interrupts++;
#endif
	num_avail = adapter->num_tx_desc_avail;
	i = adapter->oldest_used_tx_desc;

	tx_buffer = &adapter->tx_buffer_area[i];
	tx_desc = &adapter->tx_desc_base[i];

	while (tx_desc->status & IXGB_TX_DESC_STATUS_DD) {

		tx_desc->status = 0;
		num_avail++;

		if (tx_buffer->m_head) {
			bus_dmamap_sync(adapter->txtag, tx_buffer->map,
					BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(adapter->txtag, tx_buffer->map);
			bus_dmamap_destroy(adapter->txtag, tx_buffer->map);
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}
		if (++i == adapter->num_tx_desc)
			i = 0;

		tx_buffer = &adapter->tx_buffer_area[i];
		tx_desc = &adapter->tx_desc_base[i];
	}

	adapter->oldest_used_tx_desc = i;

	/*
	 * If we have enough room, clear IFF_DRV_OACTIVE to tell the stack that
	 * it is OK to send packets. If there are no pending descriptors,
	 * clear the timeout. Otherwise, if some descriptors have been freed,
	 * restart the timeout.
	 */
	if (num_avail > IXGB_TX_CLEANUP_THRESHOLD) {
		struct ifnet   *ifp = adapter->ifp;

		ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;
		if (num_avail == adapter->num_tx_desc)
			adapter->tx_timer = 0;
		else if (num_avail == adapter->num_tx_desc_avail)
			adapter->tx_timer = IXGB_TX_TIMEOUT;
	}
	adapter->num_tx_desc_avail = num_avail;
	return;
}


/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
static int
ixgb_get_buf(int i, struct adapter * adapter,
	     struct mbuf * nmp)
{
	register struct mbuf *mp = nmp;
	struct ixgb_buffer *rx_buffer;
	struct ifnet   *ifp;
	bus_addr_t      paddr;
	int             error;

	ifp = adapter->ifp;

	if (mp == NULL) {

		mp = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);

		if (mp == NULL) {
			adapter->mbuf_alloc_failed++;
			return (ENOBUFS);
		}
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
	} else {
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
		mp->m_data = mp->m_ext.ext_buf;
		mp->m_next = NULL;
	}

	if (ifp->if_mtu <= ETHERMTU) {
		m_adj(mp, ETHER_ALIGN);
	}
	rx_buffer = &adapter->rx_buffer_area[i];

	/*
	 * Using memory from the mbuf cluster pool, invoke the bus_dma
	 * machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load(adapter->rxtag, rx_buffer->map,
				mtod(mp, void *), mp->m_len,
				ixgb_dmamap_cb, &paddr, 0);
	if (error) {
		m_free(mp);
		return (error);
	}
	rx_buffer->m_head = mp;
	adapter->rx_desc_base[i].buff_addr = htole64(paddr);
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
ixgb_allocate_receive_structures(struct adapter * adapter)
{
	int             i, error;
	struct ixgb_buffer *rx_buffer;

	if (!(adapter->rx_buffer_area =
	      (struct ixgb_buffer *) malloc(sizeof(struct ixgb_buffer) *
					    adapter->num_rx_desc, M_DEVBUF,
					    M_NOWAIT | M_ZERO))) {
		device_printf(adapter->dev,
		    "Unable to allocate rx_buffer memory\n");
		return (ENOMEM);
	}
	bzero(adapter->rx_buffer_area,
	      sizeof(struct ixgb_buffer) * adapter->num_rx_desc);

	error = bus_dma_tag_create(bus_get_dma_tag(adapter->dev),/* parent */
				   PAGE_SIZE, 0,	/* alignment, bounds */
				   BUS_SPACE_MAXADDR,	/* lowaddr */
				   BUS_SPACE_MAXADDR,	/* highaddr */
				   NULL, NULL,	/* filter, filterarg */
				   MCLBYTES,	/* maxsize */
				   1,	/* nsegments */
				   MCLBYTES,	/* maxsegsize */
				   BUS_DMA_ALLOCNOW,	/* flags */
#if __FreeBSD_version >= 502000
				   NULL,	/* lockfunc */
				   NULL,	/* lockfuncarg */
#endif
				   &adapter->rxtag);
	if (error != 0) {
		device_printf(adapter->dev, "ixgb_allocate_receive_structures: "
		       "bus_dma_tag_create failed; error %u\n",
		       error);
		goto fail_0;
	}
	rx_buffer = adapter->rx_buffer_area;
	for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(adapter->rxtag, BUS_DMA_NOWAIT,
					  &rx_buffer->map);
		if (error != 0) {
			device_printf(adapter->dev,
			       "ixgb_allocate_receive_structures: "
			       "bus_dmamap_create failed; error %u\n",
			       error);
			goto fail_1;
		}
	}

	for (i = 0; i < adapter->num_rx_desc; i++) {
		if (ixgb_get_buf(i, adapter, NULL) == ENOBUFS) {
			adapter->rx_buffer_area[i].m_head = NULL;
			adapter->rx_desc_base[i].buff_addr = 0;
			return (ENOBUFS);
		}
	}

	return (0);
fail_1:
	bus_dma_tag_destroy(adapter->rxtag);
fail_0:
	adapter->rxtag = NULL;
	free(adapter->rx_buffer_area, M_DEVBUF);
	adapter->rx_buffer_area = NULL;
	return (error);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *
 **********************************************************************/
static int
ixgb_setup_receive_structures(struct adapter * adapter)
{
	bzero((void *)adapter->rx_desc_base,
	      (sizeof(struct ixgb_rx_desc)) * adapter->num_rx_desc);

	if (ixgb_allocate_receive_structures(adapter))
		return ENOMEM;

	/* Setup our descriptor pointers */
	adapter->next_rx_desc_to_check = 0;
	adapter->next_rx_desc_to_use = 0;
	return (0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
static void
ixgb_initialize_receive_unit(struct adapter * adapter)
{
	u_int32_t       reg_rctl;
	u_int32_t       reg_rxcsum;
	u_int32_t       reg_rxdctl;
	struct ifnet   *ifp;
	u_int64_t       rdba = adapter->rxdma.dma_paddr;

	ifp = adapter->ifp;

	/*
	 * Make sure receives are disabled while setting up the descriptor
	 * ring
	 */
	reg_rctl = IXGB_READ_REG(&adapter->hw, RCTL);
	IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl & ~IXGB_RCTL_RXEN);

	/* Set the Receive Delay Timer Register */
	IXGB_WRITE_REG(&adapter->hw, RDTR,
		       adapter->rx_int_delay);


	/* Setup the Base and Length of the Rx Descriptor Ring */
	IXGB_WRITE_REG(&adapter->hw, RDBAL,
		       (rdba & 0x00000000ffffffffULL));
	IXGB_WRITE_REG(&adapter->hw, RDBAH, (rdba >> 32));
	IXGB_WRITE_REG(&adapter->hw, RDLEN, adapter->num_rx_desc *
		       sizeof(struct ixgb_rx_desc));

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	IXGB_WRITE_REG(&adapter->hw, RDH, 0);

	IXGB_WRITE_REG(&adapter->hw, RDT, adapter->num_rx_desc - 1);



	reg_rxdctl = RXDCTL_WTHRESH_DEFAULT << IXGB_RXDCTL_WTHRESH_SHIFT
		| RXDCTL_HTHRESH_DEFAULT << IXGB_RXDCTL_HTHRESH_SHIFT
		| RXDCTL_PTHRESH_DEFAULT << IXGB_RXDCTL_PTHRESH_SHIFT;
	IXGB_WRITE_REG(&adapter->hw, RXDCTL, reg_rxdctl);


	adapter->raidc = 1;
	if (adapter->raidc) {
		uint32_t        raidc;
		uint8_t         poll_threshold;
#define IXGB_RAIDC_POLL_DEFAULT 120

		poll_threshold = ((adapter->num_rx_desc - 1) >> 3);
		poll_threshold >>= 1;
		poll_threshold &= 0x3F;
		raidc = IXGB_RAIDC_EN | IXGB_RAIDC_RXT_GATE |
			(IXGB_RAIDC_POLL_DEFAULT << IXGB_RAIDC_POLL_SHIFT) |
			(adapter->rx_int_delay << IXGB_RAIDC_DELAY_SHIFT) |
			poll_threshold;
		IXGB_WRITE_REG(&adapter->hw, RAIDC, raidc);
	}
	/* Enable Receive Checksum Offload for TCP and UDP ? */
	if (ifp->if_capenable & IFCAP_RXCSUM) {
		reg_rxcsum = IXGB_READ_REG(&adapter->hw, RXCSUM);
		reg_rxcsum |= IXGB_RXCSUM_TUOFL;
		IXGB_WRITE_REG(&adapter->hw, RXCSUM, reg_rxcsum);
	}
	/* Setup the Receive Control Register */
	reg_rctl = IXGB_READ_REG(&adapter->hw, RCTL);
	reg_rctl &= ~(3 << IXGB_RCTL_MO_SHIFT);
	reg_rctl |= IXGB_RCTL_BAM | IXGB_RCTL_RDMTS_1_2 | IXGB_RCTL_SECRC |
		IXGB_RCTL_CFF |
		(adapter->hw.mc_filter_type << IXGB_RCTL_MO_SHIFT);

	switch (adapter->rx_buffer_len) {
	default:
	case IXGB_RXBUFFER_2048:
		reg_rctl |= IXGB_RCTL_BSIZE_2048;
		break;
	case IXGB_RXBUFFER_4096:
		reg_rctl |= IXGB_RCTL_BSIZE_4096;
		break;
	case IXGB_RXBUFFER_8192:
		reg_rctl |= IXGB_RCTL_BSIZE_8192;
		break;
	case IXGB_RXBUFFER_16384:
		reg_rctl |= IXGB_RCTL_BSIZE_16384;
		break;
	}

	reg_rctl |= IXGB_RCTL_RXEN;


	/* Enable Receives */
	IXGB_WRITE_REG(&adapter->hw, RCTL, reg_rctl);

	return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
static void
ixgb_free_receive_structures(struct adapter * adapter)
{
	struct ixgb_buffer *rx_buffer;
	int             i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (adapter->rx_buffer_area != NULL) {
		rx_buffer = adapter->rx_buffer_area;
		for (i = 0; i < adapter->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->map != NULL) {
				bus_dmamap_unload(adapter->rxtag, rx_buffer->map);
				bus_dmamap_destroy(adapter->rxtag, rx_buffer->map);
			}
			if (rx_buffer->m_head != NULL)
				m_freem(rx_buffer->m_head);
			rx_buffer->m_head = NULL;
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
ixgb_process_receive_interrupts(struct adapter * adapter, int count)
{
	struct ifnet   *ifp;
	struct mbuf    *mp;
#if __FreeBSD_version < 500000
	struct ether_header *eh;
#endif
	int             eop = 0;
	int             len;
	u_int8_t        accept_frame = 0;
	int             i;
	int             next_to_use = 0;
	int             eop_desc;
	int		rx_npkts = 0;
	/* Pointer to the receive descriptor being examined. */
	struct ixgb_rx_desc *current_desc;

	IXGB_LOCK_ASSERT(adapter);

	ifp = adapter->ifp;
	i = adapter->next_rx_desc_to_check;
	next_to_use = adapter->next_rx_desc_to_use;
	eop_desc = adapter->next_rx_desc_to_check;
	current_desc = &adapter->rx_desc_base[i];

	if (!((current_desc->status) & IXGB_RX_DESC_STATUS_DD)) {
#ifdef _SV_
		adapter->no_pkts_avail++;
#endif
		return (rx_npkts);
	}
	while ((current_desc->status & IXGB_RX_DESC_STATUS_DD) && (count != 0)) {

		mp = adapter->rx_buffer_area[i].m_head;
		bus_dmamap_sync(adapter->rxtag, adapter->rx_buffer_area[i].map,
				BUS_DMASYNC_POSTREAD);
		accept_frame = 1;
		if (current_desc->status & IXGB_RX_DESC_STATUS_EOP) {
			count--;
			eop = 1;
		} else {
			eop = 0;
		}
		len = current_desc->length;

		if (current_desc->errors & (IXGB_RX_DESC_ERRORS_CE |
			    IXGB_RX_DESC_ERRORS_SE | IXGB_RX_DESC_ERRORS_P |
					    IXGB_RX_DESC_ERRORS_RXE)) {
			accept_frame = 0;
		}
		if (accept_frame) {

			/* Assign correct length to the current fragment */
			mp->m_len = len;

			if (adapter->fmp == NULL) {
				mp->m_pkthdr.len = len;
				adapter->fmp = mp;	/* Store the first mbuf */
				adapter->lmp = mp;
			} else {
				/* Chain mbuf's together */
				mp->m_flags &= ~M_PKTHDR;
				adapter->lmp->m_next = mp;
				adapter->lmp = adapter->lmp->m_next;
				adapter->fmp->m_pkthdr.len += len;
			}

			if (eop) {
				eop_desc = i;
				adapter->fmp->m_pkthdr.rcvif = ifp;

#if __FreeBSD_version < 500000
				eh = mtod(adapter->fmp, struct ether_header *);

				/* Remove ethernet header from mbuf */
				m_adj(adapter->fmp, sizeof(struct ether_header));
				ixgb_receive_checksum(adapter, current_desc,
						      adapter->fmp);

				if (current_desc->status & IXGB_RX_DESC_STATUS_VP)
					VLAN_INPUT_TAG(eh, adapter->fmp,
						     current_desc->special);
				else
					ether_input(ifp, eh, adapter->fmp);
#else
				ixgb_receive_checksum(adapter, current_desc,
						      adapter->fmp);
#if __FreeBSD_version < 700000
				if (current_desc->status & IXGB_RX_DESC_STATUS_VP)
					VLAN_INPUT_TAG(ifp, adapter->fmp,
						       current_desc->special);
#else
				if (current_desc->status & IXGB_RX_DESC_STATUS_VP) {
					adapter->fmp->m_pkthdr.ether_vtag =
					    current_desc->special;
					adapter->fmp->m_flags |= M_VLANTAG;
				}
#endif

				if (adapter->fmp != NULL) {
					IXGB_UNLOCK(adapter);
					(*ifp->if_input) (ifp, adapter->fmp);
					IXGB_LOCK(adapter);
					rx_npkts++;
				}
#endif
				adapter->fmp = NULL;
				adapter->lmp = NULL;
			}
			adapter->rx_buffer_area[i].m_head = NULL;
		} else {
			adapter->dropped_pkts++;
			if (adapter->fmp != NULL)
				m_freem(adapter->fmp);
			adapter->fmp = NULL;
			adapter->lmp = NULL;
		}

		/* Zero out the receive descriptors status  */
		current_desc->status = 0;

		/* Advance our pointers to the next descriptor */
		if (++i == adapter->num_rx_desc) {
			i = 0;
			current_desc = adapter->rx_desc_base;
		} else
			current_desc++;
	}
	adapter->next_rx_desc_to_check = i;

	if (--i < 0)
		i = (adapter->num_rx_desc - 1);

	/*
	 * 82597EX: Workaround for redundent write back in receive descriptor ring (causes
 	 * memory corruption). Avoid using and re-submitting the most recently received RX
	 * descriptor back to hardware.
	 *
	 * if(Last written back descriptor == EOP bit set descriptor)
	 * 	then avoid re-submitting the most recently received RX descriptor 
	 *	back to hardware.
	 * if(Last written back descriptor != EOP bit set descriptor)
	 *	then avoid re-submitting the most recently received RX descriptors
	 * 	till last EOP bit set descriptor. 
	 */
	if (eop_desc != i) {
		if (++eop_desc == adapter->num_rx_desc)
			eop_desc = 0;
		i = eop_desc;
	}
	/* Replenish the descriptors with new mbufs till last EOP bit set descriptor */
	while (next_to_use != i) {
		current_desc = &adapter->rx_desc_base[next_to_use];
		if ((current_desc->errors & (IXGB_RX_DESC_ERRORS_CE |
			    IXGB_RX_DESC_ERRORS_SE | IXGB_RX_DESC_ERRORS_P |
					     IXGB_RX_DESC_ERRORS_RXE))) {
			mp = adapter->rx_buffer_area[next_to_use].m_head;
			ixgb_get_buf(next_to_use, adapter, mp);
		} else {
			if (ixgb_get_buf(next_to_use, adapter, NULL) == ENOBUFS)
				break;
		}
		/* Advance our pointers to the next descriptor */
		if (++next_to_use == adapter->num_rx_desc) {
			next_to_use = 0;
			current_desc = adapter->rx_desc_base;
		} else
			current_desc++;
	}
	adapter->next_rx_desc_to_use = next_to_use;
	if (--next_to_use < 0)
		next_to_use = (adapter->num_rx_desc - 1);
	/* Advance the IXGB's Receive Queue #0  "Tail Pointer" */
	IXGB_WRITE_REG(&adapter->hw, RDT, next_to_use);

	return (rx_npkts);
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
static void
ixgb_receive_checksum(struct adapter * adapter,
		      struct ixgb_rx_desc * rx_desc,
		      struct mbuf * mp)
{
	if (rx_desc->status & IXGB_RX_DESC_STATUS_IXSM) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}
	if (rx_desc->status & IXGB_RX_DESC_STATUS_IPCS) {
		/* Did it pass? */
		if (!(rx_desc->errors & IXGB_RX_DESC_ERRORS_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else {
			mp->m_pkthdr.csum_flags = 0;
		}
	}
	if (rx_desc->status & IXGB_RX_DESC_STATUS_TCPCS) {
		/* Did it pass? */
		if (!(rx_desc->errors & IXGB_RX_DESC_ERRORS_TCPE)) {
			mp->m_pkthdr.csum_flags |=
				(CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			mp->m_pkthdr.csum_data = htons(0xffff);
		}
	}
	return;
}


static void
ixgb_enable_vlans(struct adapter * adapter)
{
	uint32_t        ctrl;

	ctrl = IXGB_READ_REG(&adapter->hw, CTRL0);
	ctrl |= IXGB_CTRL0_VME;
	IXGB_WRITE_REG(&adapter->hw, CTRL0, ctrl);

	return;
}


static void
ixgb_enable_intr(struct adapter * adapter)
{
	IXGB_WRITE_REG(&adapter->hw, IMS, (IXGB_INT_RXT0 | IXGB_INT_TXDW |
			    IXGB_INT_RXDMT0 | IXGB_INT_LSC | IXGB_INT_RXO));
	return;
}

static void
ixgb_disable_intr(struct adapter * adapter)
{
	IXGB_WRITE_REG(&adapter->hw, IMC, ~0);
	return;
}

void
ixgb_write_pci_cfg(struct ixgb_hw * hw,
		   uint32_t reg,
		   uint16_t * value)
{
	pci_write_config(((struct ixgb_osdep *) hw->back)->dev, reg,
			 *value, 2);
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
static void
ixgb_update_stats_counters(struct adapter * adapter)
{
	struct ifnet   *ifp;

	adapter->stats.crcerrs += IXGB_READ_REG(&adapter->hw, CRCERRS);
	adapter->stats.gprcl += IXGB_READ_REG(&adapter->hw, GPRCL);
	adapter->stats.gprch += IXGB_READ_REG(&adapter->hw, GPRCH);
	adapter->stats.gorcl += IXGB_READ_REG(&adapter->hw, GORCL);
	adapter->stats.gorch += IXGB_READ_REG(&adapter->hw, GORCH);
	adapter->stats.bprcl += IXGB_READ_REG(&adapter->hw, BPRCL);
	adapter->stats.bprch += IXGB_READ_REG(&adapter->hw, BPRCH);
	adapter->stats.mprcl += IXGB_READ_REG(&adapter->hw, MPRCL);
	adapter->stats.mprch += IXGB_READ_REG(&adapter->hw, MPRCH);
	adapter->stats.roc += IXGB_READ_REG(&adapter->hw, ROC);

	adapter->stats.mpc += IXGB_READ_REG(&adapter->hw, MPC);
	adapter->stats.dc += IXGB_READ_REG(&adapter->hw, DC);
	adapter->stats.rlec += IXGB_READ_REG(&adapter->hw, RLEC);
	adapter->stats.xonrxc += IXGB_READ_REG(&adapter->hw, XONRXC);
	adapter->stats.xontxc += IXGB_READ_REG(&adapter->hw, XONTXC);
	adapter->stats.xoffrxc += IXGB_READ_REG(&adapter->hw, XOFFRXC);
	adapter->stats.xofftxc += IXGB_READ_REG(&adapter->hw, XOFFTXC);
	adapter->stats.gptcl += IXGB_READ_REG(&adapter->hw, GPTCL);
	adapter->stats.gptch += IXGB_READ_REG(&adapter->hw, GPTCH);
	adapter->stats.gotcl += IXGB_READ_REG(&adapter->hw, GOTCL);
	adapter->stats.gotch += IXGB_READ_REG(&adapter->hw, GOTCH);
	adapter->stats.ruc += IXGB_READ_REG(&adapter->hw, RUC);
	adapter->stats.rfc += IXGB_READ_REG(&adapter->hw, RFC);
	adapter->stats.rjc += IXGB_READ_REG(&adapter->hw, RJC);
	adapter->stats.torl += IXGB_READ_REG(&adapter->hw, TORL);
	adapter->stats.torh += IXGB_READ_REG(&adapter->hw, TORH);
	adapter->stats.totl += IXGB_READ_REG(&adapter->hw, TOTL);
	adapter->stats.toth += IXGB_READ_REG(&adapter->hw, TOTH);
	adapter->stats.tprl += IXGB_READ_REG(&adapter->hw, TPRL);
	adapter->stats.tprh += IXGB_READ_REG(&adapter->hw, TPRH);
	adapter->stats.tptl += IXGB_READ_REG(&adapter->hw, TPTL);
	adapter->stats.tpth += IXGB_READ_REG(&adapter->hw, TPTH);
	adapter->stats.plt64c += IXGB_READ_REG(&adapter->hw, PLT64C);
	adapter->stats.mptcl += IXGB_READ_REG(&adapter->hw, MPTCL);
	adapter->stats.mptch += IXGB_READ_REG(&adapter->hw, MPTCH);
	adapter->stats.bptcl += IXGB_READ_REG(&adapter->hw, BPTCL);
	adapter->stats.bptch += IXGB_READ_REG(&adapter->hw, BPTCH);

	adapter->stats.uprcl += IXGB_READ_REG(&adapter->hw, UPRCL);
	adapter->stats.uprch += IXGB_READ_REG(&adapter->hw, UPRCH);
	adapter->stats.vprcl += IXGB_READ_REG(&adapter->hw, VPRCL);
	adapter->stats.vprch += IXGB_READ_REG(&adapter->hw, VPRCH);
	adapter->stats.jprcl += IXGB_READ_REG(&adapter->hw, JPRCL);
	adapter->stats.jprch += IXGB_READ_REG(&adapter->hw, JPRCH);
	adapter->stats.rnbc += IXGB_READ_REG(&adapter->hw, RNBC);
	adapter->stats.icbc += IXGB_READ_REG(&adapter->hw, ICBC);
	adapter->stats.ecbc += IXGB_READ_REG(&adapter->hw, ECBC);
	adapter->stats.uptcl += IXGB_READ_REG(&adapter->hw, UPTCL);
	adapter->stats.uptch += IXGB_READ_REG(&adapter->hw, UPTCH);
	adapter->stats.vptcl += IXGB_READ_REG(&adapter->hw, VPTCL);
	adapter->stats.vptch += IXGB_READ_REG(&adapter->hw, VPTCH);
	adapter->stats.jptcl += IXGB_READ_REG(&adapter->hw, JPTCL);
	adapter->stats.jptch += IXGB_READ_REG(&adapter->hw, JPTCH);
	adapter->stats.tsctc += IXGB_READ_REG(&adapter->hw, TSCTC);
	adapter->stats.tsctfc += IXGB_READ_REG(&adapter->hw, TSCTFC);
	adapter->stats.ibic += IXGB_READ_REG(&adapter->hw, IBIC);
	adapter->stats.lfc += IXGB_READ_REG(&adapter->hw, LFC);
	adapter->stats.pfrc += IXGB_READ_REG(&adapter->hw, PFRC);
	adapter->stats.pftc += IXGB_READ_REG(&adapter->hw, PFTC);
	adapter->stats.mcfrc += IXGB_READ_REG(&adapter->hw, MCFRC);

	ifp = adapter->ifp;

	/* Fill out the OS statistics structure */
	ifp->if_ipackets = adapter->stats.gprcl;
	ifp->if_opackets = adapter->stats.gptcl;
	ifp->if_ibytes = adapter->stats.gorcl;
	ifp->if_obytes = adapter->stats.gotcl;
	ifp->if_imcasts = adapter->stats.mprcl;
	ifp->if_collisions = 0;

	/* Rx Errors */
	ifp->if_ierrors =
		adapter->dropped_pkts +
		adapter->stats.crcerrs +
		adapter->stats.rnbc +
		adapter->stats.mpc +
		adapter->stats.rlec;


}


/**********************************************************************
 *
 *  This routine is called only when ixgb_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixgb_print_hw_stats(struct adapter * adapter)
{
	char            buf_speed[100], buf_type[100];
	ixgb_bus_speed  bus_speed;
	ixgb_bus_type   bus_type;
	device_t dev;

	dev = adapter->dev;
#ifdef _SV_
	device_printf(dev, "Packets not Avail = %ld\n",
	       adapter->no_pkts_avail);
	device_printf(dev, "CleanTxInterrupts = %ld\n",
	       adapter->clean_tx_interrupts);
	device_printf(dev, "ICR RXDMT0 = %lld\n",
	       (long long)adapter->sv_stats.icr_rxdmt0);
	device_printf(dev, "ICR RXO = %lld\n",
	       (long long)adapter->sv_stats.icr_rxo);
	device_printf(dev, "ICR RXT0 = %lld\n",
	       (long long)adapter->sv_stats.icr_rxt0);
	device_printf(dev, "ICR TXDW = %lld\n",
	       (long long)adapter->sv_stats.icr_TXDW);
#endif				/* _SV_ */

	bus_speed = adapter->hw.bus.speed;
	bus_type = adapter->hw.bus.type;
	sprintf(buf_speed,
		bus_speed == ixgb_bus_speed_33 ? "33MHz" :
		bus_speed == ixgb_bus_speed_66 ? "66MHz" :
		bus_speed == ixgb_bus_speed_100 ? "100MHz" :
		bus_speed == ixgb_bus_speed_133 ? "133MHz" :
		"UNKNOWN");
	device_printf(dev, "PCI_Bus_Speed = %s\n",
	       buf_speed);

	sprintf(buf_type,
		bus_type == ixgb_bus_type_pci ? "PCI" :
		bus_type == ixgb_bus_type_pcix ? "PCI-X" :
		"UNKNOWN");
	device_printf(dev, "PCI_Bus_Type = %s\n",
	       buf_type);

	device_printf(dev, "Tx Descriptors not Avail1 = %ld\n",
	       adapter->no_tx_desc_avail1);
	device_printf(dev, "Tx Descriptors not Avail2 = %ld\n",
	       adapter->no_tx_desc_avail2);
	device_printf(dev, "Std Mbuf Failed = %ld\n",
	       adapter->mbuf_alloc_failed);
	device_printf(dev, "Std Cluster Failed = %ld\n",
	       adapter->mbuf_cluster_failed);

	device_printf(dev, "Defer count = %lld\n",
	       (long long)adapter->stats.dc);
	device_printf(dev, "Missed Packets = %lld\n",
	       (long long)adapter->stats.mpc);
	device_printf(dev, "Receive No Buffers = %lld\n",
	       (long long)adapter->stats.rnbc);
	device_printf(dev, "Receive length errors = %lld\n",
	       (long long)adapter->stats.rlec);
	device_printf(dev, "Crc errors = %lld\n",
	       (long long)adapter->stats.crcerrs);
	device_printf(dev, "Driver dropped packets = %ld\n",
	       adapter->dropped_pkts);

	device_printf(dev, "XON Rcvd = %lld\n",
	       (long long)adapter->stats.xonrxc);
	device_printf(dev, "XON Xmtd = %lld\n",
	       (long long)adapter->stats.xontxc);
	device_printf(dev, "XOFF Rcvd = %lld\n",
	       (long long)adapter->stats.xoffrxc);
	device_printf(dev, "XOFF Xmtd = %lld\n",
	       (long long)adapter->stats.xofftxc);

	device_printf(dev, "Good Packets Rcvd = %lld\n",
	       (long long)adapter->stats.gprcl);
	device_printf(dev, "Good Packets Xmtd = %lld\n",
	       (long long)adapter->stats.gptcl);

	device_printf(dev, "Jumbo frames recvd = %lld\n",
	       (long long)adapter->stats.jprcl);
	device_printf(dev, "Jumbo frames Xmtd = %lld\n",
	       (long long)adapter->stats.jptcl);

	return;

}

static int
ixgb_sysctl_stats(SYSCTL_HANDLER_ARGS)
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
		ixgb_print_hw_stats(adapter);
	}
	return error;
}
