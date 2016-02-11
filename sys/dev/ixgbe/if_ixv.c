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


#ifndef IXGBE_STANDALONE_BUILD
#include "opt_inet.h"
#include "opt_inet6.h"
#endif

#include "ixgbe.h"

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixv_driver_version[] = "1.4.6-k";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixv_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixgbe_vendor_info_t ixv_vendor_info_array[] =
{
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_82599_VF, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X540_VF, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550_VF, 0, 0, 0},
	{IXGBE_INTEL_VENDOR_ID, IXGBE_DEV_ID_X550EM_X_VF, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *ixv_strings[] = {
	"Intel(R) PRO/10GbE Virtual Function Network Driver"
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixv_probe(device_t);
static int      ixv_attach(device_t);
static int      ixv_detach(device_t);
static int      ixv_shutdown(device_t);
static int      ixv_ioctl(struct ifnet *, u_long, caddr_t);
static void	ixv_init(void *);
static void	ixv_init_locked(struct adapter *);
static void     ixv_stop(void *);
static void     ixv_media_status(struct ifnet *, struct ifmediareq *);
static int      ixv_media_change(struct ifnet *);
static void     ixv_identify_hardware(struct adapter *);
static int      ixv_allocate_pci_resources(struct adapter *);
static int      ixv_allocate_msix(struct adapter *);
static int	ixv_setup_msix(struct adapter *);
static void	ixv_free_pci_resources(struct adapter *);
static void     ixv_local_timer(void *);
static void     ixv_setup_interface(device_t, struct adapter *);
static void     ixv_config_link(struct adapter *);

static void     ixv_initialize_transmit_units(struct adapter *);
static void     ixv_initialize_receive_units(struct adapter *);

static void     ixv_enable_intr(struct adapter *);
static void     ixv_disable_intr(struct adapter *);
static void     ixv_set_multi(struct adapter *);
static void     ixv_update_link_status(struct adapter *);
static int	ixv_sysctl_debug(SYSCTL_HANDLER_ARGS);
static void	ixv_set_ivar(struct adapter *, u8, u8, s8);
static void	ixv_configure_ivars(struct adapter *);
static u8 *	ixv_mc_array_itr(struct ixgbe_hw *, u8 **, u32 *);

static void	ixv_setup_vlan_support(struct adapter *);
static void	ixv_register_vlan(void *, struct ifnet *, u16);
static void	ixv_unregister_vlan(void *, struct ifnet *, u16);

static void	ixv_save_stats(struct adapter *);
static void	ixv_init_stats(struct adapter *);
static void	ixv_update_stats(struct adapter *);
static void	ixv_add_stats_sysctls(struct adapter *);
static void	ixv_set_sysctl_value(struct adapter *, const char *,
		    const char *, int *, int);

/* The MSI/X Interrupt handlers */
static void	ixv_msix_que(void *);
static void	ixv_msix_mbx(void *);

/* Deferred interrupt tasklets */
static void	ixv_handle_que(void *, int);
static void	ixv_handle_mbx(void *, int);

#ifdef DEV_NETMAP
/*
 * This is defined in <dev/netmap/ixgbe_netmap.h>, which is included by
 * if_ix.c.
 */
extern void ixgbe_netmap_attach(struct adapter *adapter);

#include <net/netmap.h>
#include <sys/selinfo.h>
#include <dev/netmap/netmap_kern.h>
#endif /* DEV_NETMAP */

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ixv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixv_probe),
	DEVMETHOD(device_attach, ixv_attach),
	DEVMETHOD(device_detach, ixv_detach),
	DEVMETHOD(device_shutdown, ixv_shutdown),
	DEVMETHOD_END
};

static driver_t ixv_driver = {
	"ixv", ixv_methods, sizeof(struct adapter),
};

devclass_t ixv_devclass;
DRIVER_MODULE(ixv, pci, ixv_driver, ixv_devclass, 0, 0);
MODULE_DEPEND(ixv, pci, 1, 1, 1);
MODULE_DEPEND(ixv, ether, 1, 1, 1);
#ifdef DEV_NETMAP
MODULE_DEPEND(ix, netmap, 1, 1, 1);
#endif /* DEV_NETMAP */
/* XXX depend on 'ix' ? */

/*
** TUNEABLE PARAMETERS:
*/

/* Number of Queues - do not exceed MSIX vectors - 1 */
static int ixv_num_queues = 1;
TUNABLE_INT("hw.ixv.num_queues", &ixv_num_queues);

/*
** AIM: Adaptive Interrupt Moderation
** which means that the interrupt rate
** is varied over time based on the
** traffic for that interrupt vector
*/
static int ixv_enable_aim = FALSE;
TUNABLE_INT("hw.ixv.enable_aim", &ixv_enable_aim);

/* How many packets rxeof tries to clean at a time */
static int ixv_rx_process_limit = 256;
TUNABLE_INT("hw.ixv.rx_process_limit", &ixv_rx_process_limit);

/* How many packets txeof tries to clean at a time */
static int ixv_tx_process_limit = 256;
TUNABLE_INT("hw.ixv.tx_process_limit", &ixv_tx_process_limit);

/* Flow control setting, default to full */
static int ixv_flow_control = ixgbe_fc_full;
TUNABLE_INT("hw.ixv.flow_control", &ixv_flow_control);

/*
 * Header split: this causes the hardware to DMA
 * the header into a seperate mbuf from the payload,
 * it can be a performance win in some workloads, but
 * in others it actually hurts, its off by default.
 */
static int ixv_header_split = FALSE;
TUNABLE_INT("hw.ixv.hdr_split", &ixv_header_split);

/*
** Number of TX descriptors per ring,
** setting higher than RX as this seems
** the better performing choice.
*/
static int ixv_txd = DEFAULT_TXD;
TUNABLE_INT("hw.ixv.txd", &ixv_txd);

/* Number of RX descriptors per ring */
static int ixv_rxd = DEFAULT_RXD;
TUNABLE_INT("hw.ixv.rxd", &ixv_rxd);

/*
** Shadow VFTA table, this is needed because
** the real filter table gets cleared during
** a soft reset and we need to repopulate it.
*/
static u32 ixv_shadow_vfta[IXGBE_VFTA_SIZE];

/*********************************************************************
 *  Device identification routine
 *
 *  ixv_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
ixv_probe(device_t dev)
{
	ixgbe_vendor_info_t *ent;

	u16	pci_vendor_id = 0;
	u16	pci_device_id = 0;
	u16	pci_subvendor_id = 0;
	u16	pci_subdevice_id = 0;
	char	adapter_name[256];


	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != IXGBE_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixv_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(adapter_name, "%s, Version - %s",
				ixv_strings[ent->index],
				ixv_driver_version);
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
ixv_attach(device_t dev)
{
	struct adapter *adapter;
	struct ixgbe_hw *hw;
	int             error = 0;

	INIT_DEBUGOUT("ixv_attach: begin");

	/* Allocate, clear, and link in our adapter structure */
	adapter = device_get_softc(dev);
	adapter->dev = dev;
	hw = &adapter->hw;

#ifdef DEV_NETMAP
	adapter->init_locked = ixv_init_locked;
	adapter->stop_locked = ixv_stop;
#endif

	/* Core Lock Init*/
	IXGBE_CORE_LOCK_INIT(adapter, device_get_nameunit(dev));

	/* SYSCTL APIs */
	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "debug", CTLTYPE_INT | CTLFLAG_RW,
			adapter, 0, ixv_sysctl_debug, "I", "Debug Info");

	SYSCTL_ADD_INT(device_get_sysctl_ctx(dev),
			SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			OID_AUTO, "enable_aim", CTLFLAG_RW,
			&ixv_enable_aim, 1, "Interrupt Moderation");

	/* Set up the timer callout */
	callout_init_mtx(&adapter->timer, &adapter->core_mtx, 0);

	/* Determine hardware revision */
	ixv_identify_hardware(adapter);

	/* Do base PCI setup - map BAR0 */
	if (ixv_allocate_pci_resources(adapter)) {
		device_printf(dev, "ixv_allocate_pci_resources() failed!\n");
		error = ENXIO;
		goto err_out;
	}

	/* Sysctls for limiting the amount of work done in the taskqueues */
	ixv_set_sysctl_value(adapter, "rx_processing_limit",
	    "max number of rx packets to process",
	    &adapter->rx_process_limit, ixv_rx_process_limit);

	ixv_set_sysctl_value(adapter, "tx_processing_limit",
	    "max number of tx packets to process",
	    &adapter->tx_process_limit, ixv_tx_process_limit);

	/* Sysctls for limiting the amount of work done in the taskqueues */
	ixv_set_sysctl_value(adapter, "rx_processing_limit",
	    "max number of rx packets to process",
	    &adapter->rx_process_limit, ixv_rx_process_limit);

	ixv_set_sysctl_value(adapter, "tx_processing_limit",
	    "max number of tx packets to process",
	    &adapter->tx_process_limit, ixv_tx_process_limit);

	/* Do descriptor calc and sanity checks */
	if (((ixv_txd * sizeof(union ixgbe_adv_tx_desc)) % DBA_ALIGN) != 0 ||
	    ixv_txd < MIN_TXD || ixv_txd > MAX_TXD) {
		device_printf(dev, "TXD config issue, using default!\n");
		adapter->num_tx_desc = DEFAULT_TXD;
	} else
		adapter->num_tx_desc = ixv_txd;

	if (((ixv_rxd * sizeof(union ixgbe_adv_rx_desc)) % DBA_ALIGN) != 0 ||
	    ixv_rxd < MIN_RXD || ixv_rxd > MAX_RXD) {
		device_printf(dev, "RXD config issue, using default!\n");
		adapter->num_rx_desc = DEFAULT_RXD;
	} else
		adapter->num_rx_desc = ixv_rxd;

	/* Allocate our TX/RX Queues */
	if (ixgbe_allocate_queues(adapter)) {
		device_printf(dev, "ixgbe_allocate_queues() failed!\n");
		error = ENOMEM;
		goto err_out;
	}

	/*
	** Initialize the shared code: its
	** at this point the mac type is set.
	*/
	error = ixgbe_init_shared_code(hw);
	if (error) {
		device_printf(dev, "ixgbe_init_shared_code() failed!\n");
		error = EIO;
		goto err_late;
	}

	/* Setup the mailbox */
	ixgbe_init_mbx_params_vf(hw);

	/* Reset mbox api to 1.0 */
	error = ixgbe_reset_hw(hw);
	if (error == IXGBE_ERR_RESET_FAILED)
		device_printf(dev, "ixgbe_reset_hw() failure: Reset Failed!\n");
	else if (error)
		device_printf(dev, "ixgbe_reset_hw() failed with error %d\n", error);
	if (error) {
		error = EIO;
		goto err_late;
	}

	/* Negotiate mailbox API version */
	error = ixgbevf_negotiate_api_version(hw, ixgbe_mbox_api_11);
	if (error) {
		device_printf(dev, "MBX API 1.1 negotiation failed! Error %d\n", error);
		error = EIO;
		goto err_late;
	}

	error = ixgbe_init_hw(hw);
	if (error) {
		device_printf(dev, "ixgbe_init_hw() failed!\n");
		error = EIO;
		goto err_late;
	}
	
	error = ixv_allocate_msix(adapter); 
	if (error) {
		device_printf(dev, "ixv_allocate_msix() failed!\n");
		goto err_late;
	}

	/* If no mac address was assigned, make a random one */
	if (!ixv_check_ether_addr(hw->mac.addr)) {
		u8 addr[ETHER_ADDR_LEN];
		arc4rand(&addr, sizeof(addr), 0);
		addr[0] &= 0xFE;
		addr[0] |= 0x02;
		bcopy(addr, hw->mac.addr, sizeof(addr));
	}

	/* Setup OS specific network interface */
	ixv_setup_interface(dev, adapter);

	/* Do the stats setup */
	ixv_save_stats(adapter);
	ixv_init_stats(adapter);
	ixv_add_stats_sysctls(adapter);

	/* Register for VLAN events */
	adapter->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixv_register_vlan, adapter, EVENTHANDLER_PRI_FIRST);
	adapter->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixv_unregister_vlan, adapter, EVENTHANDLER_PRI_FIRST);

#ifdef DEV_NETMAP
	ixgbe_netmap_attach(adapter);
#endif /* DEV_NETMAP */
	INIT_DEBUGOUT("ixv_attach: end");
	return (0);

err_late:
	ixgbe_free_transmit_structures(adapter);
	ixgbe_free_receive_structures(adapter);
err_out:
	ixv_free_pci_resources(adapter);
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
ixv_detach(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	struct ix_queue *que = adapter->queues;

	INIT_DEBUGOUT("ixv_detach: begin");

	/* Make sure VLANS are not using driver */
	if (adapter->ifp->if_vlantrunk != NULL) {
		device_printf(dev, "Vlan in use, detach first\n");
		return (EBUSY);
	}

	IXGBE_CORE_LOCK(adapter);
	ixv_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);

	for (int i = 0; i < adapter->num_queues; i++, que++) {
		if (que->tq) {
			struct tx_ring  *txr = que->txr;
			taskqueue_drain(que->tq, &txr->txq_task);
			taskqueue_drain(que->tq, &que->que_task);
			taskqueue_free(que->tq);
		}
	}

	/* Drain the Mailbox(link) queue */
	if (adapter->tq) {
		taskqueue_drain(adapter->tq, &adapter->link_task);
		taskqueue_free(adapter->tq);
	}

	/* Unregister VLAN events */
	if (adapter->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, adapter->vlan_attach);
	if (adapter->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, adapter->vlan_detach);

	ether_ifdetach(adapter->ifp);
	callout_drain(&adapter->timer);
#ifdef DEV_NETMAP
	netmap_detach(adapter->ifp);
#endif /* DEV_NETMAP */
	ixv_free_pci_resources(adapter);
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
ixv_shutdown(device_t dev)
{
	struct adapter *adapter = device_get_softc(dev);
	IXGBE_CORE_LOCK(adapter);
	ixv_stop(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return (0);
}


/*********************************************************************
 *  Ioctl entry point
 *
 *  ixv_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixv_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct adapter	*adapter = ifp->if_softc;
	struct ifreq	*ifr = (struct ifreq *) data;
#if defined(INET) || defined(INET6)
	struct ifaddr	*ifa = (struct ifaddr *) data;
	bool		avoid_reset = FALSE;
#endif
	int             error = 0;

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
#if defined(INET) || defined(INET6)
		/*
		** Calling init results in link renegotiation,
		** so we avoid doing it when possible.
		*/
		if (avoid_reset) {
			ifp->if_flags |= IFF_UP;
			if (!(ifp->if_drv_flags & IFF_DRV_RUNNING))
				ixv_init(adapter);
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
		} else
			error = ether_ioctl(ifp, command, data);
		break;
#endif
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXGBE_MAX_FRAME_SIZE - IXGBE_MTU_HDR) {
			error = EINVAL;
		} else {
			IXGBE_CORE_LOCK(adapter);
			ifp->if_mtu = ifr->ifr_mtu;
			adapter->max_frame_size =
				ifp->if_mtu + IXGBE_MTU_HDR;
			ixv_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		IXGBE_CORE_LOCK(adapter);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				ixv_init_locked(adapter);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixv_stop(adapter);
		adapter->if_flags = ifp->if_flags;
		IXGBE_CORE_UNLOCK(adapter);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOC(ADD|DEL)MULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixv_disable_intr(adapter);
			ixv_set_multi(adapter);
			ixv_enable_intr(adapter);
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
		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXGBE_CORE_LOCK(adapter);
			ixv_init_locked(adapter);
			IXGBE_CORE_UNLOCK(adapter);
		}
		VLAN_CAPABILITIES(ifp);
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
ixv_init_locked(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t 	dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;
	int error = 0;

	INIT_DEBUGOUT("ixv_init_locked: begin");
	mtx_assert(&adapter->core_mtx, MA_OWNED);
	hw->adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
        callout_stop(&adapter->timer);

        /* reprogram the RAR[0] in case user changed it. */
        ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	bcopy(IF_LLADDR(adapter->ifp), hw->mac.addr,
	     IXGBE_ETH_LENGTH_OF_ADDRESS);
        ixgbe_set_rar(hw, 0, hw->mac.addr, 0, 1);
	hw->addr_ctrl.rar_used_count = 1;

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(adapter)) {
		device_printf(dev, "Could not setup transmit structures\n");
		ixv_stop(adapter);
		return;
	}

	/* Reset VF and renegotiate mailbox API version */
	ixgbe_reset_hw(hw);
	error = ixgbevf_negotiate_api_version(hw, ixgbe_mbox_api_11);
	if (error)
		device_printf(dev, "MBX API 1.1 negotiation failed! Error %d\n", error);

	ixv_initialize_transmit_units(adapter);

	/* Setup Multicast table */
	ixv_set_multi(adapter);

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
		device_printf(dev, "Could not setup receive structures\n");
		ixv_stop(adapter);
		return;
	}

	/* Configure RX settings */
	ixv_initialize_receive_units(adapter);

	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO4)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM) {
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
#if __FreeBSD_version >= 800000
		ifp->if_hwassist |= CSUM_SCTP;
#endif
	}
	
	/* Set up VLAN offload and filter */
	ixv_setup_vlan_support(adapter);

	/* Set up MSI/X routing */
	ixv_configure_ivars(adapter);

	/* Set up auto-mask */
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAM, IXGBE_EICS_RTX_QUEUE);

        /* Set moderation on the Link interrupt */
        IXGBE_WRITE_REG(hw, IXGBE_VTEITR(adapter->vector), IXGBE_LINK_ITR);

	/* Stats init */
	ixv_init_stats(adapter);

	/* Config/Enable Link */
	ixv_config_link(adapter);

	/* Start watchdog */
	callout_reset(&adapter->timer, hz, ixv_local_timer, adapter);

	/* And now turn on interrupts */
	ixv_enable_intr(adapter);

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
	ifp->if_drv_flags &= ~IFF_DRV_OACTIVE;

	return;
}

static void
ixv_init(void *arg)
{
	struct adapter *adapter = arg;

	IXGBE_CORE_LOCK(adapter);
	ixv_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);
	return;
}


/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/

static inline void
ixv_enable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	queue = 1 << vector;
	u32	mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);
}

static inline void
ixv_disable_queue(struct adapter *adapter, u32 vector)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u64	queue = (u64)(1 << vector);
	u32	mask;

	mask = (IXGBE_EIMS_RTX_QUEUE & queue);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIMC, mask);
}

static inline void
ixv_rearm_queues(struct adapter *adapter, u64 queues)
{
	u32 mask = (IXGBE_EIMS_RTX_QUEUE & queues);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEICS, mask);
}


static void
ixv_handle_que(void *context, int pending)
{
	struct ix_queue *que = context;
	struct adapter  *adapter = que->adapter;
	struct tx_ring	*txr = que->txr;
	struct ifnet    *ifp = adapter->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixgbe_rxeof(que);
		IXGBE_TX_LOCK(txr);
		ixgbe_txeof(txr);
#if __FreeBSD_version >= 800000
		if (!drbr_empty(ifp, txr->br))
			ixgbe_mq_start_locked(ifp, txr);
#else
		if (!IFQ_DRV_IS_EMPTY(&ifp->if_snd))
			ixgbe_start_locked(txr, ifp);
#endif
		IXGBE_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->que_task);
			return;
		}
	}

	/* Reenable this interrupt */
	ixv_enable_queue(adapter, que->msix);
	return;
}

/*********************************************************************
 *
 *  MSI Queue Interrupt Service routine
 *
 **********************************************************************/
void
ixv_msix_que(void *arg)
{
	struct ix_queue	*que = arg;
	struct adapter  *adapter = que->adapter;
	struct ifnet    *ifp = adapter->ifp;
	struct tx_ring	*txr = que->txr;
	struct rx_ring	*rxr = que->rxr;
	bool		more;
	u32		newitr = 0;

	ixv_disable_queue(adapter, que->msix);
	++que->irqs;

	more = ixgbe_rxeof(que);

	IXGBE_TX_LOCK(txr);
	ixgbe_txeof(txr);
	/*
	** Make certain that if the stack
	** has anything queued the task gets
	** scheduled to handle it.
	*/
#ifdef IXGBE_LEGACY_TX
	if (!IFQ_DRV_IS_EMPTY(&adapter->ifp->if_snd))
		ixgbe_start_locked(txr, ifp);
#else
	if (!drbr_empty(adapter->ifp, txr->br))
		ixgbe_mq_start_locked(ifp, txr);
#endif
	IXGBE_TX_UNLOCK(txr);

	/* Do AIM now? */

	if (ixv_enable_aim == FALSE)
		goto no_calc;
	/*
	** Do Adaptive Interrupt Moderation:
        **  - Write out last calculated setting
	**  - Calculate based on average size over
	**    the last interval.
	*/
        if (que->eitr_setting)
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_VTEITR(que->msix),
		    que->eitr_setting);
 
        que->eitr_setting = 0;

        /* Idle, do nothing */
        if ((txr->bytes == 0) && (rxr->bytes == 0))
                goto no_calc;
                                
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

	newitr |= newitr << 16;
                 
        /* save for next interrupt */
        que->eitr_setting = newitr;

        /* Reset state */
        txr->bytes = 0;
        txr->packets = 0;
        rxr->bytes = 0;
        rxr->packets = 0;

no_calc:
	if (more)
		taskqueue_enqueue(que->tq, &que->que_task);
	else /* Reenable this interrupt */
		ixv_enable_queue(adapter, que->msix);
	return;
}

static void
ixv_msix_mbx(void *arg)
{
	struct adapter	*adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	u32		reg;

	++adapter->link_irq;

	/* First get the cause */
	reg = IXGBE_READ_REG(hw, IXGBE_VTEICS);
	/* Clear interrupt with write */
	IXGBE_WRITE_REG(hw, IXGBE_VTEICR, reg);

	/* Link status change */
	if (reg & IXGBE_EICR_LSC)
		taskqueue_enqueue(adapter->tq, &adapter->link_task);

	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, IXGBE_EIMS_OTHER);
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
ixv_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct adapter *adapter = ifp->if_softc;

	INIT_DEBUGOUT("ixv_media_status: begin");
	IXGBE_CORE_LOCK(adapter);
	ixv_update_link_status(adapter);

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
			ifmr->ifm_active |= IFM_FDX;
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
ixv_media_change(struct ifnet * ifp)
{
	struct adapter *adapter = ifp->if_softc;
	struct ifmedia *ifm = &adapter->media;

	INIT_DEBUGOUT("ixv_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

        switch (IFM_SUBTYPE(ifm->ifm_media)) {
        case IFM_AUTO:
                break;
        default:
                device_printf(adapter->dev, "Only auto media type\n");
		return (EINVAL);
        }

	return (0);
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
#define IXGBE_RAR_ENTRIES 16

static void
ixv_set_multi(struct adapter *adapter)
{
	u8	mta[MAX_NUM_MULTICAST_ADDRESSES * IXGBE_ETH_LENGTH_OF_ADDRESS];
	u8	*update_ptr;
	struct	ifmultiaddr *ifma;
	int	mcnt = 0;
	struct ifnet   *ifp = adapter->ifp;

	IOCTL_DEBUGOUT("ixv_set_multi: begin");

#if __FreeBSD_version < 800000
	IF_ADDR_LOCK(ifp);
#else
	if_maddr_rlock(ifp);
#endif
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		bcopy(LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
#if __FreeBSD_version < 800000
	IF_ADDR_UNLOCK(ifp);
#else
	if_maddr_runlock(ifp);
#endif

	update_ptr = mta;

	ixgbe_update_mc_addr_list(&adapter->hw,
	    update_ptr, mcnt, ixv_mc_array_itr, TRUE);

	return;
}

/*
 * This is an iterator function now needed by the multicast
 * shared code. It simply feeds the shared code routine the
 * addresses in the array of ixv_set_multi() one by one.
 */
static u8 *
ixv_mc_array_itr(struct ixgbe_hw *hw, u8 **update_ptr, u32 *vmdq)
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
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
ixv_local_timer(void *arg)
{
	struct adapter	*adapter = arg;
	device_t	dev = adapter->dev;
	struct ix_queue	*que = adapter->queues;
	u64		queues = 0;
	int		hung = 0;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	ixv_update_link_status(adapter);

	/* Stats Update */
	ixv_update_stats(adapter);

	/*
	** Check the TX queues status
	**      - mark hung queues so we don't schedule on them
	**      - watchdog only if all queues show hung
	*/
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* Keep track of queues with work for soft irq */
		if (que->txr->busy)
			queues |= ((u64)1 << que->me);
		/*
		** Each time txeof runs without cleaning, but there
		** are uncleaned descriptors it increments busy. If
		** we get to the MAX we declare it hung.
		*/
		if (que->busy == IXGBE_QUEUE_HUNG) {
			++hung;
			/* Mark the queue as inactive */
			adapter->active_queues &= ~((u64)1 << que->me);
			continue;
		} else {
			/* Check if we've come back from hung */
			if ((adapter->active_queues & ((u64)1 << que->me)) == 0)
                                adapter->active_queues |= ((u64)1 << que->me);
		}
		if (que->busy >= IXGBE_MAX_TX_BUSY) {
			device_printf(dev,"Warning queue %d "
			    "appears to be hung!\n", i);
			que->txr->busy = IXGBE_QUEUE_HUNG;
			++hung;
		}

	}

	/* Only truely watchdog if all queues show hung */
	if (hung == adapter->num_queues)
		goto watchdog;
	else if (queues != 0) { /* Force an IRQ on queues with work */
		ixv_rearm_queues(adapter, queues);
	}

	callout_reset(&adapter->timer, hz, ixv_local_timer, adapter);
	return;

watchdog:
	device_printf(adapter->dev, "Watchdog timeout -- resetting\n");
	adapter->ifp->if_drv_flags &= ~IFF_DRV_RUNNING;
	adapter->watchdog_events++;
	ixv_init_locked(adapter);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
static void
ixv_update_link_status(struct adapter *adapter)
{
	struct ifnet	*ifp = adapter->ifp;
	device_t dev = adapter->dev;

	if (adapter->link_up){ 
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
ixv_stop(void *arg)
{
	struct ifnet   *ifp;
	struct adapter *adapter = arg;
	struct ixgbe_hw *hw = &adapter->hw;
	ifp = adapter->ifp;

	mtx_assert(&adapter->core_mtx, MA_OWNED);

	INIT_DEBUGOUT("ixv_stop: begin\n");
	ixv_disable_intr(adapter);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING | IFF_DRV_OACTIVE);

	ixgbe_reset_hw(hw);
	adapter->hw.adapter_stopped = FALSE;
	ixgbe_stop_adapter(hw);
	callout_stop(&adapter->timer);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_set_rar(hw, 0, hw->mac.addr, 0, IXGBE_RAH_AV);

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
static void
ixv_identify_hardware(struct adapter *adapter)
{
	device_t        dev = adapter->dev;
	struct ixgbe_hw *hw = &adapter->hw;

	/*
	** Make sure BUSMASTER is set, on a VM under
	** KVM it may not be and will break things.
	*/
	pci_enable_busmaster(dev);

	/* Save off the information about this board */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	/* We need this to determine device-specific things */
	ixgbe_set_mac_type(hw);

	/* Set the right number of segments */
	adapter->num_segs = IXGBE_82599_SCATTER;

	return;
}

/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers 
 *
 **********************************************************************/
static int
ixv_allocate_msix(struct adapter *adapter)
{
	device_t	dev = adapter->dev;
	struct 		ix_queue *que = adapter->queues;
	struct		tx_ring *txr = adapter->tx_rings;
	int 		error, rid, vector = 0;

	for (int i = 0; i < adapter->num_queues; i++, vector++, que++, txr++) {
		rid = vector + 1;
		que->res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (que->res == NULL) {
			device_printf(dev,"Unable to allocate"
		    	    " bus resource: que interrupt [%d]\n", vector);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, que->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL,
		    ixv_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register QUE handler");
			return (error);
		}
#if __FreeBSD_version >= 800504
		bus_describe_intr(dev, que->res, que->tag, "que %d", i);
#endif
		que->msix = vector;
        	adapter->active_queues |= (u64)(1 << que->msix);
		/*
		** Bind the msix vector, and thus the
		** ring to the corresponding cpu.
		*/
		if (adapter->num_queues > 1)
			bus_bind_intr(dev, que->res, i);
		TASK_INIT(&txr->txq_task, 0, ixgbe_deferred_mq_start, txr);
		TASK_INIT(&que->que_task, 0, ixv_handle_que, que);
		que->tq = taskqueue_create_fast("ixv_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
		taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
		    device_get_nameunit(adapter->dev));
	}

	/* and Mailbox */
	rid = vector + 1;
	adapter->res = bus_alloc_resource_any(dev,
    	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (!adapter->res) {
		device_printf(dev,"Unable to allocate"
    	    " bus resource: MBX interrupt [%d]\n", rid);
		return (ENXIO);
	}
	/* Set the mbx handler function */
	error = bus_setup_intr(dev, adapter->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixv_msix_mbx, adapter, &adapter->tag);
	if (error) {
		adapter->res = NULL;
		device_printf(dev, "Failed to register LINK handler");
		return (error);
	}
#if __FreeBSD_version >= 800504
	bus_describe_intr(dev, adapter->res, adapter->tag, "mbx");
#endif
	adapter->vector = vector;
	/* Tasklets for Mailbox */
	TASK_INIT(&adapter->link_task, 0, ixv_handle_mbx, adapter);
	adapter->tq = taskqueue_create_fast("ixv_mbx", M_NOWAIT,
	    taskqueue_thread_enqueue, &adapter->tq);
	taskqueue_start_threads(&adapter->tq, 1, PI_NET, "%s mbxq",
	    device_get_nameunit(adapter->dev));
	/*
	** Due to a broken design QEMU will fail to properly
	** enable the guest for MSIX unless the vectors in
	** the table are all set up, so we must rewrite the
	** ENABLE in the MSIX control register again at this
	** point to cause it to successfully initialize us.
	*/
	if (adapter->hw.mac.type == ixgbe_mac_82599_vf) {
		int msix_ctrl;
		pci_find_cap(dev, PCIY_MSIX, &rid);
		rid += PCIR_MSIX_CTRL;
		msix_ctrl = pci_read_config(dev, rid, 2);
		msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
		pci_write_config(dev, rid, msix_ctrl, 2);
	}

	return (0);
}

/*
 * Setup MSIX resources, note that the VF
 * device MUST use MSIX, there is no fallback.
 */
static int
ixv_setup_msix(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	int rid, want, msgs;


	/* Must have at least 2 MSIX vectors */
	msgs = pci_msix_count(dev);
	if (msgs < 2)
		goto out;
	rid = PCIR_BAR(3);
	adapter->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (adapter->msix_mem == NULL) {
		device_printf(adapter->dev,
		    "Unable to map MSIX table \n");
		goto out;
	}

	/*
	** Want vectors for the queues,
	** plus an additional for mailbox.
	*/
	want = adapter->num_queues + 1;
	if (want > msgs) {
		want = msgs;
		adapter->num_queues = msgs - 1;
	} else
		msgs = want;
	if ((pci_alloc_msix(dev, &msgs) == 0) && (msgs == want)) {
               	device_printf(adapter->dev,
		    "Using MSIX interrupts with %d vectors\n", want);
		return (want);
	}
	/* Release in case alloc was insufficient */
	pci_release_msi(dev);
out:
       	if (adapter->msix_mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, adapter->msix_mem);
		adapter->msix_mem = NULL;
	}
	device_printf(adapter->dev,"MSIX config error\n");
	return (ENXIO);
}


static int
ixv_allocate_pci_resources(struct adapter *adapter)
{
	int             rid;
	device_t        dev = adapter->dev;

	rid = PCIR_BAR(0);
	adapter->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(adapter->pci_mem)) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	adapter->osdep.mem_bus_space_tag =
		rman_get_bustag(adapter->pci_mem);
	adapter->osdep.mem_bus_space_handle =
		rman_get_bushandle(adapter->pci_mem);
	adapter->hw.hw_addr = (u8 *)&adapter->osdep.mem_bus_space_handle;

	/* Pick up the tuneable queues */
	adapter->num_queues = ixv_num_queues;
	adapter->hw.back = adapter;

	/*
	** Now setup MSI/X, should
	** return us the number of
	** configured vectors.
	*/
	adapter->msix = ixv_setup_msix(adapter);
	if (adapter->msix == ENXIO)
		return (ENXIO);
	else
		return (0);
}

static void
ixv_free_pci_resources(struct adapter * adapter)
{
	struct 		ix_queue *que = adapter->queues;
	device_t	dev = adapter->dev;
	int		rid, memrid;

	memrid = PCIR_BAR(MSIX_82598_BAR);

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
	**  Release all msix queue resources:
	*/
	for (int i = 0; i < adapter->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
	}


	/* Clean the Legacy or Link interrupt last */
	if (adapter->vector) /* we are doing MSIX */
		rid = adapter->vector + 1;
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
		    memrid, adapter->msix_mem);

	if (adapter->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), adapter->pci_mem);

	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static void
ixv_setup_interface(device_t dev, struct adapter *adapter)
{
	struct ifnet   *ifp;

	INIT_DEBUGOUT("ixv_setup_interface: begin");

	ifp = adapter->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL)
		panic("%s: can not if_alloc()\n", device_get_nameunit(dev));
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_baudrate = 1000000000;
	ifp->if_init = ixv_init;
	ifp->if_softc = adapter;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixv_ioctl;
#if __FreeBSD_version >= 800000
	ifp->if_transmit = ixgbe_mq_start;
	ifp->if_qflush = ixgbe_qflush;
#else
	ifp->if_start = ixgbe_start;
#endif
	ifp->if_snd.ifq_maxlen = adapter->num_tx_desc - 2;

	ether_ifattach(ifp, adapter->hw.mac.addr);

	adapter->max_frame_size =
	    ifp->if_mtu + IXGBE_MTU_HDR_VLAN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM | IFCAP_TSO4 | IFCAP_VLAN_HWCSUM;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING
			     |  IFCAP_VLAN_HWTSO
			     |  IFCAP_VLAN_MTU;
	ifp->if_capabilities |= IFCAP_LRO;
	ifp->if_capenable = ifp->if_capabilities;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&adapter->media, IFM_IMASK, ixv_media_change,
		     ixv_media_status);
	ifmedia_add(&adapter->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&adapter->media, IFM_ETHER | IFM_AUTO);

	return;
}
	
static void
ixv_config_link(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32	autoneg;

	if (hw->mac.ops.check_link)
		hw->mac.ops.check_link(hw, &autoneg,
		    &adapter->link_up, FALSE);
}


/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
static void
ixv_initialize_transmit_units(struct adapter *adapter)
{
	struct tx_ring	*txr = adapter->tx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;


	for (int i = 0; i < adapter->num_queues; i++, txr++) {
		u64	tdba = txr->txdma.dma_paddr;
		u32	txctrl, txdctl;

		/* Set WTHRESH to 8, burst writeback */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);

		/* Set the HW Tx Head and Tail indices */
	    	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VFTDH(i), 0);
	    	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VFTDT(i), 0);

		/* Set Tx Tail register */
		txr->tail = IXGBE_VFTDT(i);

		/* Set Ring parameters */
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFTDLEN(i),
		    adapter->num_tx_desc *
		    sizeof(struct ixgbe_legacy_tx_desc));
		txctrl = IXGBE_READ_REG(hw, IXGBE_VFDCA_TXCTRL(i));
		txctrl &= ~IXGBE_DCA_TXCTRL_DESC_WRO_EN;
		IXGBE_WRITE_REG(hw, IXGBE_VFDCA_TXCTRL(i), txctrl);

		/* Now enable */
		txdctl = IXGBE_READ_REG(hw, IXGBE_VFTXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFTXDCTL(i), txdctl);
	}

	return;
}


/*********************************************************************
 *
 *  Setup receive registers and features.
 *
 **********************************************************************/
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

static void
ixv_initialize_receive_units(struct adapter *adapter)
{
	struct	rx_ring	*rxr = adapter->rx_rings;
	struct ixgbe_hw	*hw = &adapter->hw;
	struct ifnet	*ifp = adapter->ifp;
	u32		bufsz, rxcsum, psrtype;

	if (ifp->if_mtu > ETHERMTU)
		bufsz = 4096 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;
	else
		bufsz = 2048 >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	psrtype = IXGBE_PSRTYPE_TCPHDR | IXGBE_PSRTYPE_UDPHDR |
	    IXGBE_PSRTYPE_IPV4HDR | IXGBE_PSRTYPE_IPV6HDR |
	    IXGBE_PSRTYPE_L2HDR;

	IXGBE_WRITE_REG(hw, IXGBE_VFPSRTYPE, psrtype);

	/* Tell PF our max_frame size */
	ixgbevf_rlpml_set_vf(hw, adapter->max_frame_size);

	for (int i = 0; i < adapter->num_queues; i++, rxr++) {
		u64 rdba = rxr->rxdma.dma_paddr;
		u32 reg, rxdctl;

		/* Disable the queue */
		rxdctl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		rxdctl &= ~IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), rxdctl);
		for (int j = 0; j < 10; j++) {
			if (IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				msec_delay(1);
			else
				break;
		}
		wmb();
		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAL(i),
		    (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDBAH(i),
		    (rdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_VFRDLEN(i),
		    adapter->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Reset the ring indices */
		IXGBE_WRITE_REG(hw, IXGBE_VFRDH(rxr->me), 0);
		IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me), 0);

		/* Set up the SRRCTL register */
		reg = IXGBE_READ_REG(hw, IXGBE_VFSRRCTL(i));
		reg &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		reg &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		reg |= bufsz;
		reg |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
		IXGBE_WRITE_REG(hw, IXGBE_VFSRRCTL(i), reg);

		/* Capture  Rx Tail register */
		rxr->tail = IXGBE_VFRDT(rxr->me);

		/* Do the queue enabling last */
		rxdctl |= IXGBE_RXDCTL_ENABLE | IXGBE_RXDCTL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), rxdctl);
		for (int k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		wmb();

		/* Set the Tail Pointer */
#ifdef DEV_NETMAP
		/*
		 * In netmap mode, we must preserve the buffers made
		 * available to userspace before the if_init()
		 * (this is true by default on the TX side, because
		 * init makes all buffers available to userspace).
		 *
		 * netmap_reset() and the device specific routines
		 * (e.g. ixgbe_setup_receive_rings()) map these
		 * buffers at the end of the NIC ring, so here we
		 * must set the RDT (tail) register to make sure
		 * they are not overwritten.
		 *
		 * In this driver the NIC ring starts at RDH = 0,
		 * RDT points to the last slot available for reception (?),
		 * so RDT = num_rx_desc - 1 means the whole ring is available.
		 */
		if (ifp->if_capenable & IFCAP_NETMAP) {
			struct netmap_adapter *na = NA(adapter->ifp);
			struct netmap_kring *kring = &na->rx_rings[i];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);

			IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me), t);
		} else
#endif /* DEV_NETMAP */
			IXGBE_WRITE_REG(hw, IXGBE_VFRDT(rxr->me),
			    adapter->num_rx_desc - 1);
	}

	rxcsum = IXGBE_READ_REG(hw, IXGBE_RXCSUM);

	if (ifp->if_capenable & IFCAP_RXCSUM)
		rxcsum |= IXGBE_RXCSUM_PCSD;

	if (!(rxcsum & IXGBE_RXCSUM_PCSD))
		rxcsum |= IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(hw, IXGBE_RXCSUM, rxcsum);

	return;
}

static void
ixv_setup_vlan_support(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32		ctrl, vid, vfta, retry;
	struct rx_ring	*rxr;

	/*
	** We get here thru init_locked, meaning
	** a soft reset, this has already cleared
	** the VFTA and other state, so if there
	** have been no vlan's registered do nothing.
	*/
	if (adapter->num_vlans == 0)
		return;

	/* Enable the queues */
	for (int i = 0; i < adapter->num_queues; i++) {
		ctrl = IXGBE_READ_REG(hw, IXGBE_VFRXDCTL(i));
		ctrl |= IXGBE_RXDCTL_VME;
		IXGBE_WRITE_REG(hw, IXGBE_VFRXDCTL(i), ctrl);
		/*
		 * Let Rx path know that it needs to store VLAN tag
		 * as part of extra mbuf info.
		 */
		rxr = &adapter->rx_rings[i];
		rxr->vtag_strip = TRUE;
	}

	/*
	** A soft reset zero's out the VFTA, so
	** we need to repopulate it now.
	*/
	for (int i = 0; i < IXGBE_VFTA_SIZE; i++) {
		if (ixv_shadow_vfta[i] == 0)
			continue;
		vfta = ixv_shadow_vfta[i];
		/*
		** Reconstruct the vlan id's
		** based on the bits set in each
		** of the array ints.
		*/
		for (int j = 0; j < 32; j++) {
			retry = 0;
			if ((vfta & (1 << j)) == 0)
				continue;
			vid = (i * 32) + j;
			/* Call the shared code mailbox routine */
			while (ixgbe_set_vfta(hw, vid, 0, TRUE)) {
				if (++retry > 5)
					break;
			}
		}
	}
}

/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixv_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc != arg) /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095)) /* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixv_shadow_vfta[index] |= (1 << bit);
	++adapter->num_vlans;
	/* Re-init to load the changes */
	ixv_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixv_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct adapter	*adapter = ifp->if_softc;
	u16		index, bit;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXGBE_CORE_LOCK(adapter);
	index = (vtag >> 5) & 0x7F;
	bit = vtag & 0x1F;
	ixv_shadow_vfta[index] &= ~(1 << bit);
	--adapter->num_vlans;
	/* Re-init to load the changes */
	ixv_init_locked(adapter);
	IXGBE_CORE_UNLOCK(adapter);
}

static void
ixv_enable_intr(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
	struct ix_queue *que = adapter->queues;
	u32 mask = (IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE);


	IXGBE_WRITE_REG(hw, IXGBE_VTEIMS, mask);

	mask = IXGBE_EIMS_ENABLE_MASK;
	mask &= ~(IXGBE_EIMS_OTHER | IXGBE_EIMS_LSC);
	IXGBE_WRITE_REG(hw, IXGBE_VTEIAC, mask);

        for (int i = 0; i < adapter->num_queues; i++, que++)
		ixv_enable_queue(adapter, que->msix);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

static void
ixv_disable_intr(struct adapter *adapter)
{
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEIAC, 0);
	IXGBE_WRITE_REG(&adapter->hw, IXGBE_VTEIMC, ~0);
	IXGBE_WRITE_FLUSH(&adapter->hw);
	return;
}

/*
** Setup the correct IVAR register for a particular MSIX interrupt
**  - entry is the register array entry
**  - vector is the MSIX vector for this queue
**  - type is RX/TX/MISC
*/
static void
ixv_set_ivar(struct adapter *adapter, u8 entry, u8 vector, s8 type)
{
	struct ixgbe_hw *hw = &adapter->hw;
	u32 ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	if (type == -1) { /* MISC IVAR */
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR_MISC);
		ivar &= ~0xFF;
		ivar |= vector;
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR_MISC, ivar);
	} else {	/* RX/TX IVARS */
		index = (16 * (entry & 1)) + (8 * type);
		ivar = IXGBE_READ_REG(hw, IXGBE_VTIVAR(entry >> 1));
		ivar &= ~(0xFF << index);
		ivar |= (vector << index);
		IXGBE_WRITE_REG(hw, IXGBE_VTIVAR(entry >> 1), ivar);
	}
}

static void
ixv_configure_ivars(struct adapter *adapter)
{
	struct  ix_queue *que = adapter->queues;

        for (int i = 0; i < adapter->num_queues; i++, que++) {
		/* First the RX queue entry */
                ixv_set_ivar(adapter, i, que->msix, 0);
		/* ... and the TX */
		ixv_set_ivar(adapter, i, que->msix, 1);
		/* Set an initial value in EITR */
                IXGBE_WRITE_REG(&adapter->hw,
                    IXGBE_VTEITR(que->msix), IXV_EITR_DEFAULT);
	}

	/* For the mailbox interrupt */
        ixv_set_ivar(adapter, 1, adapter->vector, -1);
}


/*
** Tasklet handler for MSIX MBX interrupts
**  - do outside interrupt since it might sleep
*/
static void
ixv_handle_mbx(void *context, int pending)
{
	struct adapter  *adapter = context;

	ixgbe_check_link(&adapter->hw,
	    &adapter->link_speed, &adapter->link_up, 0);
	ixv_update_link_status(adapter);
}

/*
** The VF stats registers never have a truely virgin
** starting point, so this routine tries to make an
** artificial one, marking ground zero on attach as
** it were.
*/
static void
ixv_save_stats(struct adapter *adapter)
{
	if (adapter->stats.vf.vfgprc || adapter->stats.vf.vfgptc) {
		adapter->stats.vf.saved_reset_vfgprc +=
		    adapter->stats.vf.vfgprc - adapter->stats.vf.base_vfgprc;
		adapter->stats.vf.saved_reset_vfgptc +=
		    adapter->stats.vf.vfgptc - adapter->stats.vf.base_vfgptc;
		adapter->stats.vf.saved_reset_vfgorc +=
		    adapter->stats.vf.vfgorc - adapter->stats.vf.base_vfgorc;
		adapter->stats.vf.saved_reset_vfgotc +=
		    adapter->stats.vf.vfgotc - adapter->stats.vf.base_vfgotc;
		adapter->stats.vf.saved_reset_vfmprc +=
		    adapter->stats.vf.vfmprc - adapter->stats.vf.base_vfmprc;
	}
}
 
static void
ixv_init_stats(struct adapter *adapter)
{
	struct ixgbe_hw *hw = &adapter->hw;
 
	adapter->stats.vf.last_vfgprc = IXGBE_READ_REG(hw, IXGBE_VFGPRC);
	adapter->stats.vf.last_vfgorc = IXGBE_READ_REG(hw, IXGBE_VFGORC_LSB);
	adapter->stats.vf.last_vfgorc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGORC_MSB))) << 32);

	adapter->stats.vf.last_vfgptc = IXGBE_READ_REG(hw, IXGBE_VFGPTC);
	adapter->stats.vf.last_vfgotc = IXGBE_READ_REG(hw, IXGBE_VFGOTC_LSB);
	adapter->stats.vf.last_vfgotc |=
	    (((u64)(IXGBE_READ_REG(hw, IXGBE_VFGOTC_MSB))) << 32);

	adapter->stats.vf.last_vfmprc = IXGBE_READ_REG(hw, IXGBE_VFMPRC);

	adapter->stats.vf.base_vfgprc = adapter->stats.vf.last_vfgprc;
	adapter->stats.vf.base_vfgorc = adapter->stats.vf.last_vfgorc;
	adapter->stats.vf.base_vfgptc = adapter->stats.vf.last_vfgptc;
	adapter->stats.vf.base_vfgotc = adapter->stats.vf.last_vfgotc;
	adapter->stats.vf.base_vfmprc = adapter->stats.vf.last_vfmprc;
}

#define UPDATE_STAT_32(reg, last, count)		\
{							\
	u32 current = IXGBE_READ_REG(hw, reg);		\
	if (current < last)				\
		count += 0x100000000LL;			\
	last = current;					\
	count &= 0xFFFFFFFF00000000LL;			\
	count |= current;				\
}

#define UPDATE_STAT_36(lsb, msb, last, count) 		\
{							\
	u64 cur_lsb = IXGBE_READ_REG(hw, lsb);		\
	u64 cur_msb = IXGBE_READ_REG(hw, msb);		\
	u64 current = ((cur_msb << 32) | cur_lsb);	\
	if (current < last)				\
		count += 0x1000000000LL;		\
	last = current;					\
	count &= 0xFFFFFFF000000000LL;			\
	count |= current;				\
}

/*
** ixv_update_stats - Update the board statistics counters.
*/
void
ixv_update_stats(struct adapter *adapter)
{
        struct ixgbe_hw *hw = &adapter->hw;

        UPDATE_STAT_32(IXGBE_VFGPRC, adapter->stats.vf.last_vfgprc,
	    adapter->stats.vf.vfgprc);
        UPDATE_STAT_32(IXGBE_VFGPTC, adapter->stats.vf.last_vfgptc,
	    adapter->stats.vf.vfgptc);
        UPDATE_STAT_36(IXGBE_VFGORC_LSB, IXGBE_VFGORC_MSB,
	    adapter->stats.vf.last_vfgorc, adapter->stats.vf.vfgorc);
        UPDATE_STAT_36(IXGBE_VFGOTC_LSB, IXGBE_VFGOTC_MSB,
	    adapter->stats.vf.last_vfgotc, adapter->stats.vf.vfgotc);
        UPDATE_STAT_32(IXGBE_VFMPRC, adapter->stats.vf.last_vfmprc,
	    adapter->stats.vf.vfmprc);
}

/*
 * Add statistic sysctls for the VF.
 */
static void
ixv_add_stats_sysctls(struct adapter *adapter)
{
	device_t dev = adapter->dev;
	struct ix_queue *que = &adapter->queues[0];
	struct tx_ring *txr = que->txr;
	struct rx_ring *rxr = que->rxr;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct ixgbevf_hw_stats *stats = &adapter->stats.vf;

	struct sysctl_oid *stat_node, *queue_node;
	struct sysctl_oid_list *stat_list, *queue_list;

	/* Driver Statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "dropped",
			CTLFLAG_RD, &adapter->dropped_pkts,
			"Driver dropped packets");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "mbuf_defrag_failed",
			CTLFLAG_RD, &adapter->mbuf_defrag_failed,
			"m_defrag() failed");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
			CTLFLAG_RD, &adapter->watchdog_events,
			"Watchdog timeouts");

	stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
				    CTLFLAG_RD, NULL,
				    "VF Statistics (read from HW registers)");
	stat_list = SYSCTL_CHILDREN(stat_node);

	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_rcvd",
			CTLFLAG_RD, &stats->vfgprc,
			"Good Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_rcvd",
			CTLFLAG_RD, &stats->vfgorc, 
			"Good Octets Received"); 
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "mcast_pkts_rcvd",
			CTLFLAG_RD, &stats->vfmprc,
			"Multicast Packets Received");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_pkts_txd",
			CTLFLAG_RD, &stats->vfgptc,
			"Good Packets Transmitted");
	SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, "good_octets_txd",
			CTLFLAG_RD, &stats->vfgotc, 
			"Good Octets Transmitted"); 

	queue_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "que",
				    CTLFLAG_RD, NULL,
				    "Queue Statistics (collected by SW)");
	queue_list = SYSCTL_CHILDREN(queue_node);

	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
			CTLFLAG_RD, &(que->irqs),
			"IRQs on queue");
	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_irqs",
			CTLFLAG_RD, &(rxr->rx_irq),
			"RX irqs on queue");
	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
			CTLFLAG_RD, &(rxr->rx_packets),
			"RX packets");
	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
			CTLFLAG_RD, &(rxr->rx_bytes),
			"RX bytes");
	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_discarded",
			CTLFLAG_RD, &(rxr->rx_discarded),
			"Discarded RX packets");

	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_packets",
			CTLFLAG_RD, &(txr->total_packets),
			"TX Packets");
	SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "tx_bytes",
			CTLFLAG_RD, &(txr->bytes), 0,
			"TX Bytes");
	SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_no_desc",
			CTLFLAG_RD, &(txr->no_desc_avail),
			"# of times not enough descriptors were available during TX");
}

static void
ixv_set_sysctl_value(struct adapter *adapter, const char *name,
	const char *description, int *limit, int value)
{
	*limit = value;
	SYSCTL_ADD_INT(device_get_sysctl_ctx(adapter->dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(adapter->dev)),
	    OID_AUTO, name, CTLFLAG_RW, limit, value, description);
}

/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
static void
ixv_print_debug_info(struct adapter *adapter)
{
        device_t dev = adapter->dev;
        struct ixgbe_hw         *hw = &adapter->hw;
        struct ix_queue         *que = adapter->queues;
        struct rx_ring          *rxr;
        struct tx_ring          *txr;
        struct lro_ctrl         *lro;

        device_printf(dev,"Error Byte Count = %u \n",
            IXGBE_READ_REG(hw, IXGBE_ERRBC));

        for (int i = 0; i < adapter->num_queues; i++, que++) {
                txr = que->txr;
                rxr = que->rxr;
                lro = &rxr->lro;
                device_printf(dev,"QUE(%d) IRQs Handled: %lu\n",
                    que->msix, (long)que->irqs);
                device_printf(dev,"RX(%d) Packets Received: %lld\n",
                    rxr->me, (long long)rxr->rx_packets);
                device_printf(dev,"RX(%d) Bytes Received: %lu\n",
                    rxr->me, (long)rxr->rx_bytes);
                device_printf(dev,"RX(%d) LRO Queued= %d\n",
                    rxr->me, lro->lro_queued);
                device_printf(dev,"RX(%d) LRO Flushed= %d\n",
                    rxr->me, lro->lro_flushed);
                device_printf(dev,"TX(%d) Packets Sent: %lu\n",
                    txr->me, (long)txr->total_packets);
                device_printf(dev,"TX(%d) NO Desc Avail: %lu\n",
                    txr->me, (long)txr->no_desc_avail);
        }

        device_printf(dev,"MBX IRQ Handled: %lu\n",
            (long)adapter->link_irq);
        return;
}

static int
ixv_sysctl_debug(SYSCTL_HANDLER_ARGS)
{
	int error, result;
	struct adapter *adapter;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		adapter = (struct adapter *) arg1;
		ixv_print_debug_info(adapter);
	}
	return error;
}

