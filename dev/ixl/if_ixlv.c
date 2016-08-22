/******************************************************************************

  Copyright (c) 2013-2015, Intel Corporation 
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

#include "ixl.h"
#include "ixlv.h"

/*********************************************************************
 *  Driver version
 *********************************************************************/
char ixlv_driver_version[] = "1.4.6-k";

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *  Last field stores an index into ixlv_strings
 *  Last entry must be all 0s
 *
 *  { Vendor ID, Device ID, SubVendor ID, SubDevice ID, String Index }
 *********************************************************************/

static ixl_vendor_info_t ixlv_vendor_info_array[] =
{
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_VF, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_VF_HV, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_X722_VF, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_X722_A0_VF, 0, 0, 0},
	{I40E_INTEL_VENDOR_ID, I40E_DEV_ID_X722_VF_HV, 0, 0, 0},
	/* required last entry */
	{0, 0, 0, 0, 0}
};

/*********************************************************************
 *  Table of branding strings
 *********************************************************************/

static char    *ixlv_strings[] = {
	"Intel(R) Ethernet Connection XL710/X722 VF Driver"
};


/*********************************************************************
 *  Function prototypes
 *********************************************************************/
static int      ixlv_probe(device_t);
static int      ixlv_attach(device_t);
static int      ixlv_detach(device_t);
static int      ixlv_shutdown(device_t);
static void	ixlv_init_locked(struct ixlv_sc *);
static int	ixlv_allocate_pci_resources(struct ixlv_sc *);
static void	ixlv_free_pci_resources(struct ixlv_sc *);
static int	ixlv_assign_msix(struct ixlv_sc *);
static int	ixlv_init_msix(struct ixlv_sc *);
static int	ixlv_init_taskqueue(struct ixlv_sc *);
static int	ixlv_setup_queues(struct ixlv_sc *);
static void	ixlv_config_rss(struct ixlv_sc *);
static void	ixlv_stop(struct ixlv_sc *);
static void	ixlv_add_multi(struct ixl_vsi *);
static void	ixlv_del_multi(struct ixl_vsi *);
static void	ixlv_free_queues(struct ixl_vsi *);
static int	ixlv_setup_interface(device_t, struct ixlv_sc *);

static int	ixlv_media_change(struct ifnet *);
static void	ixlv_media_status(struct ifnet *, struct ifmediareq *);

static void	ixlv_local_timer(void *);

static int	ixlv_add_mac_filter(struct ixlv_sc *, u8 *, u16);
static int	ixlv_del_mac_filter(struct ixlv_sc *sc, u8 *macaddr);
static void	ixlv_init_filters(struct ixlv_sc *);
static void	ixlv_free_filters(struct ixlv_sc *);

static void	ixlv_msix_que(void *);
static void	ixlv_msix_adminq(void *);
static void	ixlv_do_adminq(void *, int);
static void	ixlv_do_adminq_locked(struct ixlv_sc *sc);
static void	ixlv_handle_que(void *, int);
static int	ixlv_reset(struct ixlv_sc *);
static int	ixlv_reset_complete(struct i40e_hw *);
static void	ixlv_set_queue_rx_itr(struct ixl_queue *);
static void	ixlv_set_queue_tx_itr(struct ixl_queue *);
static void	ixl_init_cmd_complete(struct ixl_vc_cmd *, void *,
		    enum i40e_status_code);
static void	ixlv_configure_itr(struct ixlv_sc *);

static void	ixlv_enable_adminq_irq(struct i40e_hw *);
static void	ixlv_disable_adminq_irq(struct i40e_hw *);
static void	ixlv_enable_queue_irq(struct i40e_hw *, int);
static void	ixlv_disable_queue_irq(struct i40e_hw *, int);

static void	ixlv_setup_vlan_filters(struct ixlv_sc *);
static void	ixlv_register_vlan(void *, struct ifnet *, u16);
static void	ixlv_unregister_vlan(void *, struct ifnet *, u16);

static void	ixlv_init_hw(struct ixlv_sc *);
static int	ixlv_setup_vc(struct ixlv_sc *);
static int	ixlv_vf_config(struct ixlv_sc *);

static void	ixlv_cap_txcsum_tso(struct ixl_vsi *,
		    struct ifnet *, int);

static void	ixlv_add_sysctls(struct ixlv_sc *);
#ifdef IXL_DEBUG
static int 	ixlv_sysctl_qtx_tail_handler(SYSCTL_HANDLER_ARGS);
static int 	ixlv_sysctl_qrx_tail_handler(SYSCTL_HANDLER_ARGS);
#endif

/*********************************************************************
 *  FreeBSD Device Interface Entry Points
 *********************************************************************/

static device_method_t ixlv_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, ixlv_probe),
	DEVMETHOD(device_attach, ixlv_attach),
	DEVMETHOD(device_detach, ixlv_detach),
	DEVMETHOD(device_shutdown, ixlv_shutdown),
	{0, 0}
};

static driver_t ixlv_driver = {
	"ixlv", ixlv_methods, sizeof(struct ixlv_sc),
};

devclass_t ixlv_devclass;
DRIVER_MODULE(ixlv, pci, ixlv_driver, ixlv_devclass, 0, 0);

MODULE_DEPEND(ixlv, pci, 1, 1, 1);
MODULE_DEPEND(ixlv, ether, 1, 1, 1);

/*
** TUNEABLE PARAMETERS:
*/

static SYSCTL_NODE(_hw, OID_AUTO, ixlv, CTLFLAG_RD, 0,
                   "IXLV driver parameters");

/*
** Number of descriptors per ring:
**   - TX and RX are the same size
*/
static int ixlv_ringsz = DEFAULT_RING;
TUNABLE_INT("hw.ixlv.ringsz", &ixlv_ringsz);
SYSCTL_INT(_hw_ixlv, OID_AUTO, ring_size, CTLFLAG_RDTUN,
    &ixlv_ringsz, 0, "Descriptor Ring Size");

/* Set to zero to auto calculate  */
int ixlv_max_queues = 0;
TUNABLE_INT("hw.ixlv.max_queues", &ixlv_max_queues);
SYSCTL_INT(_hw_ixlv, OID_AUTO, max_queues, CTLFLAG_RDTUN,
    &ixlv_max_queues, 0, "Number of Queues");

/*
** Number of entries in Tx queue buf_ring.
** Increasing this will reduce the number of
** errors when transmitting fragmented UDP
** packets.
*/
static int ixlv_txbrsz = DEFAULT_TXBRSZ;
TUNABLE_INT("hw.ixlv.txbrsz", &ixlv_txbrsz);
SYSCTL_INT(_hw_ixlv, OID_AUTO, txbr_size, CTLFLAG_RDTUN,
    &ixlv_txbrsz, 0, "TX Buf Ring Size");

/*
** Controls for Interrupt Throttling
**      - true/false for dynamic adjustment
**      - default values for static ITR
*/
int ixlv_dynamic_rx_itr = 0;
TUNABLE_INT("hw.ixlv.dynamic_rx_itr", &ixlv_dynamic_rx_itr);
SYSCTL_INT(_hw_ixlv, OID_AUTO, dynamic_rx_itr, CTLFLAG_RDTUN,
    &ixlv_dynamic_rx_itr, 0, "Dynamic RX Interrupt Rate");

int ixlv_dynamic_tx_itr = 0;
TUNABLE_INT("hw.ixlv.dynamic_tx_itr", &ixlv_dynamic_tx_itr);
SYSCTL_INT(_hw_ixlv, OID_AUTO, dynamic_tx_itr, CTLFLAG_RDTUN,
    &ixlv_dynamic_tx_itr, 0, "Dynamic TX Interrupt Rate");

int ixlv_rx_itr = IXL_ITR_8K;
TUNABLE_INT("hw.ixlv.rx_itr", &ixlv_rx_itr);
SYSCTL_INT(_hw_ixlv, OID_AUTO, rx_itr, CTLFLAG_RDTUN,
    &ixlv_rx_itr, 0, "RX Interrupt Rate");

int ixlv_tx_itr = IXL_ITR_4K;
TUNABLE_INT("hw.ixlv.tx_itr", &ixlv_tx_itr);
SYSCTL_INT(_hw_ixlv, OID_AUTO, tx_itr, CTLFLAG_RDTUN,
    &ixlv_tx_itr, 0, "TX Interrupt Rate");

/* Fix when building as a standalone module when netmap is enabled */
#if defined(DEV_NETMAP) && !defined(NETMAP_IXL_MAIN)
int ixl_rx_miss, ixl_rx_miss_bufs, ixl_crcstrip;
#endif
        
/*********************************************************************
 *  Device identification routine
 *
 *  ixlv_probe determines if the driver should be loaded on
 *  the hardware based on PCI vendor/device id of the device.
 *
 *  return BUS_PROBE_DEFAULT on success, positive on failure
 *********************************************************************/

static int
ixlv_probe(device_t dev)
{
	ixl_vendor_info_t *ent;

	u16	pci_vendor_id, pci_device_id;
	u16	pci_subvendor_id, pci_subdevice_id;
	char	device_name[256];

#if 0
	INIT_DEBUGOUT("ixlv_probe: begin");
#endif

	pci_vendor_id = pci_get_vendor(dev);
	if (pci_vendor_id != I40E_INTEL_VENDOR_ID)
		return (ENXIO);

	pci_device_id = pci_get_device(dev);
	pci_subvendor_id = pci_get_subvendor(dev);
	pci_subdevice_id = pci_get_subdevice(dev);

	ent = ixlv_vendor_info_array;
	while (ent->vendor_id != 0) {
		if ((pci_vendor_id == ent->vendor_id) &&
		    (pci_device_id == ent->device_id) &&

		    ((pci_subvendor_id == ent->subvendor_id) ||
		     (ent->subvendor_id == 0)) &&

		    ((pci_subdevice_id == ent->subdevice_id) ||
		     (ent->subdevice_id == 0))) {
			sprintf(device_name, "%s, Version - %s",
				ixlv_strings[ent->index],
				ixlv_driver_version);
			device_set_desc_copy(dev, device_name);
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
ixlv_attach(device_t dev)
{
	struct ixlv_sc	*sc;
	struct i40e_hw	*hw;
	struct ixl_vsi 	*vsi;
	int            	error = 0;

	INIT_DBG_DEV(dev, "begin");

	/* Allocate, clear, and link in our primary soft structure */
	sc = device_get_softc(dev);
	sc->dev = sc->osdep.dev = dev;
	hw = &sc->hw;
	vsi = &sc->vsi;
	vsi->dev = dev;

	/* Initialize hw struct */
	ixlv_init_hw(sc);

	/* Allocate filter lists */
	ixlv_init_filters(sc);

	/* Core Lock Init */
	mtx_init(&sc->mtx, device_get_nameunit(dev),
	    "IXL SC Lock", MTX_DEF);

	/* Set up the timer callout */
	callout_init_mtx(&sc->timer, &sc->mtx, 0);

	/* Do PCI setup - map BAR0, etc */
	if (ixlv_allocate_pci_resources(sc)) {
		device_printf(dev, "%s: Allocation of PCI resources failed\n",
		    __func__);
		error = ENXIO;
		goto err_early;
	}

	INIT_DBG_DEV(dev, "Allocated PCI resources and MSIX vectors");

	error = i40e_set_mac_type(hw);
	if (error) {
		device_printf(dev, "%s: set_mac_type failed: %d\n",
		    __func__, error);
		goto err_pci_res;
	}

	error = ixlv_reset_complete(hw);
	if (error) {
		device_printf(dev, "%s: Device is still being reset\n",
		    __func__);
		goto err_pci_res;
	}

	INIT_DBG_DEV(dev, "VF Device is ready for configuration");

	error = ixlv_setup_vc(sc);
	if (error) {
		device_printf(dev, "%s: Error setting up PF comms, %d\n",
		    __func__, error);
		goto err_pci_res;
	}

	INIT_DBG_DEV(dev, "PF API version verified");

	/* Need API version before sending reset message */
	error = ixlv_reset(sc);
	if (error) {
		device_printf(dev, "VF reset failed; reload the driver\n");
		goto err_aq;
	}

	INIT_DBG_DEV(dev, "VF reset complete");

	/* Ask for VF config from PF */
	error = ixlv_vf_config(sc);
	if (error) {
		device_printf(dev, "Error getting configuration from PF: %d\n",
		    error);
		goto err_aq;
	}

	device_printf(dev, "VSIs %d, QPs %d, MSIX %d, RSS sizes: key %d lut %d\n",
	    sc->vf_res->num_vsis,
	    sc->vf_res->num_queue_pairs,
	    sc->vf_res->max_vectors,
	    sc->vf_res->rss_key_size,
	    sc->vf_res->rss_lut_size);
#ifdef IXL_DEBUG
	device_printf(dev, "Offload flags: 0x%b\n",
	    sc->vf_res->vf_offload_flags, IXLV_PRINTF_VF_OFFLOAD_FLAGS);
#endif

	/* got VF config message back from PF, now we can parse it */
	for (int i = 0; i < sc->vf_res->num_vsis; i++) {
		if (sc->vf_res->vsi_res[i].vsi_type == I40E_VSI_SRIOV)
			sc->vsi_res = &sc->vf_res->vsi_res[i];
	}
	if (!sc->vsi_res) {
		device_printf(dev, "%s: no LAN VSI found\n", __func__);
		error = EIO;
		goto err_res_buf;
	}

	INIT_DBG_DEV(dev, "Resource Acquisition complete");

	/* If no mac address was assigned just make a random one */
	if (!ixlv_check_ether_addr(hw->mac.addr)) {
		u8 addr[ETHER_ADDR_LEN];
		arc4rand(&addr, sizeof(addr), 0);
		addr[0] &= 0xFE;
		addr[0] |= 0x02;
		bcopy(addr, hw->mac.addr, sizeof(addr));
	}

	/* Now that the number of queues for this VF is known, set up interrupts */
	sc->msix = ixlv_init_msix(sc);
	/* We fail without MSIX support */
	if (sc->msix == 0) {
		error = ENXIO;
		goto err_res_buf;
	}

	vsi->id = sc->vsi_res->vsi_id;
	vsi->back = (void *)sc;
	sc->link_up = TRUE;

	/* This allocates the memory and early settings */
	if (ixlv_setup_queues(sc) != 0) {
		device_printf(dev, "%s: setup queues failed!\n",
		    __func__);
		error = EIO;
		goto out;
	}

	/* Setup the stack interface */
	if (ixlv_setup_interface(dev, sc) != 0) {
		device_printf(dev, "%s: setup interface failed!\n",
		    __func__);
		error = EIO;
		goto out;
	}

	INIT_DBG_DEV(dev, "Queue memory and interface setup");

	/* Do queue interrupt setup */
	if (ixlv_assign_msix(sc) != 0) {
		device_printf(dev, "%s: allocating queue interrupts failed!\n",
		    __func__);
		error = ENXIO;
		goto out;
	}

	/* Start AdminQ taskqueue */
	ixlv_init_taskqueue(sc);

	/* Initialize stats */
	bzero(&sc->vsi.eth_stats, sizeof(struct i40e_eth_stats));
	ixlv_add_sysctls(sc);

	/* Register for VLAN events */
	vsi->vlan_attach = EVENTHANDLER_REGISTER(vlan_config,
	    ixlv_register_vlan, vsi, EVENTHANDLER_PRI_FIRST);
	vsi->vlan_detach = EVENTHANDLER_REGISTER(vlan_unconfig,
	    ixlv_unregister_vlan, vsi, EVENTHANDLER_PRI_FIRST);

	/* We want AQ enabled early */
	ixlv_enable_adminq_irq(hw);

	/* Set things up to run init */
	sc->init_state = IXLV_INIT_READY;

	ixl_vc_init_mgr(sc, &sc->vc_mgr);

	INIT_DBG_DEV(dev, "end");
	return (error);

out:
	ixlv_free_queues(vsi);
err_res_buf:
	free(sc->vf_res, M_DEVBUF);
err_aq:
	i40e_shutdown_adminq(hw);
err_pci_res:
	ixlv_free_pci_resources(sc);
err_early:
	mtx_destroy(&sc->mtx);
	ixlv_free_filters(sc);
	INIT_DBG_DEV(dev, "end: error %d", error);
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
ixlv_detach(device_t dev)
{
	struct ixlv_sc	*sc = device_get_softc(dev);
	struct ixl_vsi 	*vsi = &sc->vsi;

	INIT_DBG_DEV(dev, "begin");

	/* Make sure VLANS are not using driver */
	if (vsi->ifp->if_vlantrunk != NULL) {
		if_printf(vsi->ifp, "Vlan in use, detach first\n");
		INIT_DBG_DEV(dev, "end");
		return (EBUSY);
	}

	/* Stop driver */
	ether_ifdetach(vsi->ifp);
	if (vsi->ifp->if_drv_flags & IFF_DRV_RUNNING) {
		mtx_lock(&sc->mtx);	
		ixlv_stop(sc);
		mtx_unlock(&sc->mtx);	
	}

	/* Unregister VLAN events */
	if (vsi->vlan_attach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_config, vsi->vlan_attach);
	if (vsi->vlan_detach != NULL)
		EVENTHANDLER_DEREGISTER(vlan_unconfig, vsi->vlan_detach);

	/* Drain VC mgr */
	callout_drain(&sc->vc_mgr.callout);

	i40e_shutdown_adminq(&sc->hw);
	taskqueue_free(sc->tq);
	if_free(vsi->ifp);
	free(sc->vf_res, M_DEVBUF);
	ixlv_free_pci_resources(sc);
	ixlv_free_queues(vsi);
	mtx_destroy(&sc->mtx);
	ixlv_free_filters(sc);

	bus_generic_detach(dev);
	INIT_DBG_DEV(dev, "end");
	return (0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

static int
ixlv_shutdown(device_t dev)
{
	struct ixlv_sc	*sc = device_get_softc(dev);

	INIT_DBG_DEV(dev, "begin");

	mtx_lock(&sc->mtx);	
	ixlv_stop(sc);
	mtx_unlock(&sc->mtx);	

	INIT_DBG_DEV(dev, "end");
	return (0);
}

/*
 * Configure TXCSUM(IPV6) and TSO(4/6)
 *	- the hardware handles these together so we
 *	  need to tweak them 
 */
static void
ixlv_cap_txcsum_tso(struct ixl_vsi *vsi, struct ifnet *ifp, int mask)
{
	/* Enable/disable TXCSUM/TSO4 */
	if (!(ifp->if_capenable & IFCAP_TXCSUM)
	    && !(ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM) {
			ifp->if_capenable |= IFCAP_TXCSUM;
			/* enable TXCSUM, restore TSO if previously enabled */
			if (vsi->flags & IXL_FLAGS_KEEP_TSO4) {
				vsi->flags &= ~IXL_FLAGS_KEEP_TSO4;
				ifp->if_capenable |= IFCAP_TSO4;
			}
		}
		else if (mask & IFCAP_TSO4) {
			ifp->if_capenable |= (IFCAP_TXCSUM | IFCAP_TSO4);
			vsi->flags &= ~IXL_FLAGS_KEEP_TSO4;
			if_printf(ifp,
			    "TSO4 requires txcsum, enabling both...\n");
		}
	} else if((ifp->if_capenable & IFCAP_TXCSUM)
	    && !(ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM)
			ifp->if_capenable &= ~IFCAP_TXCSUM;
		else if (mask & IFCAP_TSO4)
			ifp->if_capenable |= IFCAP_TSO4;
	} else if((ifp->if_capenable & IFCAP_TXCSUM)
	    && (ifp->if_capenable & IFCAP_TSO4)) {
		if (mask & IFCAP_TXCSUM) {
			vsi->flags |= IXL_FLAGS_KEEP_TSO4;
			ifp->if_capenable &= ~(IFCAP_TXCSUM | IFCAP_TSO4);
			if_printf(ifp, 
			    "TSO4 requires txcsum, disabling both...\n");
		} else if (mask & IFCAP_TSO4)
			ifp->if_capenable &= ~IFCAP_TSO4;
	}

	/* Enable/disable TXCSUM_IPV6/TSO6 */
	if (!(ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && !(ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6) {
			ifp->if_capenable |= IFCAP_TXCSUM_IPV6;
			if (vsi->flags & IXL_FLAGS_KEEP_TSO6) {
				vsi->flags &= ~IXL_FLAGS_KEEP_TSO6;
				ifp->if_capenable |= IFCAP_TSO6;
			}
		} else if (mask & IFCAP_TSO6) {
			ifp->if_capenable |= (IFCAP_TXCSUM_IPV6 | IFCAP_TSO6);
			vsi->flags &= ~IXL_FLAGS_KEEP_TSO6;
			if_printf(ifp,
			    "TSO6 requires txcsum6, enabling both...\n");
		}
	} else if((ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && !(ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6)
			ifp->if_capenable &= ~IFCAP_TXCSUM_IPV6;
		else if (mask & IFCAP_TSO6)
			ifp->if_capenable |= IFCAP_TSO6;
	} else if ((ifp->if_capenable & IFCAP_TXCSUM_IPV6)
	    && (ifp->if_capenable & IFCAP_TSO6)) {
		if (mask & IFCAP_TXCSUM_IPV6) {
			vsi->flags |= IXL_FLAGS_KEEP_TSO6;
			ifp->if_capenable &= ~(IFCAP_TXCSUM_IPV6 | IFCAP_TSO6);
			if_printf(ifp,
			    "TSO6 requires txcsum6, disabling both...\n");
		} else if (mask & IFCAP_TSO6)
			ifp->if_capenable &= ~IFCAP_TSO6;
	}
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixlv_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

static int
ixlv_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct ixl_vsi		*vsi = ifp->if_softc;
	struct ixlv_sc	*sc = vsi->back;
	struct ifreq		*ifr = (struct ifreq *)data;
#if defined(INET) || defined(INET6)
	struct ifaddr 		*ifa = (struct ifaddr *)data;
	bool			avoid_reset = FALSE;
#endif
	int             	error = 0;


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
				ixlv_init(vsi);
#ifdef INET
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			error = ether_ioctl(ifp, command, data);
		break;
#endif
	case SIOCSIFMTU:
		IOCTL_DBG_IF2(ifp, "SIOCSIFMTU (Set Interface MTU)");
		mtx_lock(&sc->mtx);
		if (ifr->ifr_mtu > IXL_MAX_FRAME -
		    ETHER_HDR_LEN - ETHER_CRC_LEN - ETHER_VLAN_ENCAP_LEN) {
			error = EINVAL;
			IOCTL_DBG_IF(ifp, "mtu too large");
		} else {
			IOCTL_DBG_IF2(ifp, "mtu: %lu -> %d", (u_long)ifp->if_mtu, ifr->ifr_mtu);
			// ERJ: Interestingly enough, these types don't match
			ifp->if_mtu = (u_long)ifr->ifr_mtu;
			vsi->max_frame_size =
			    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
			    + ETHER_VLAN_ENCAP_LEN;
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixlv_init_locked(sc);
		}
		mtx_unlock(&sc->mtx);
		break;
	case SIOCSIFFLAGS:
		IOCTL_DBG_IF2(ifp, "SIOCSIFFLAGS (Set Interface Flags)");
		mtx_lock(&sc->mtx);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
				ixlv_init_locked(sc);
		} else
			if (ifp->if_drv_flags & IFF_DRV_RUNNING)
				ixlv_stop(sc);
		sc->if_flags = ifp->if_flags;
		mtx_unlock(&sc->mtx);
		break;
	case SIOCADDMULTI:
		IOCTL_DBG_IF2(ifp, "SIOCADDMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			mtx_lock(&sc->mtx);
			ixlv_disable_intr(vsi);
			ixlv_add_multi(vsi);
			ixlv_enable_intr(vsi);
			mtx_unlock(&sc->mtx);
		}
		break;
	case SIOCDELMULTI:
		IOCTL_DBG_IF2(ifp, "SIOCDELMULTI");
		if (sc->init_state == IXLV_RUNNING) {
			mtx_lock(&sc->mtx);
			ixlv_disable_intr(vsi);
			ixlv_del_multi(vsi);
			ixlv_enable_intr(vsi);
			mtx_unlock(&sc->mtx);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DBG_IF2(ifp, "SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		IOCTL_DBG_IF2(ifp, "SIOCSIFCAP (Set Capabilities)");

		ixlv_cap_txcsum_tso(vsi, ifp, mask);

		if (mask & IFCAP_RXCSUM)
			ifp->if_capenable ^= IFCAP_RXCSUM;
		if (mask & IFCAP_RXCSUM_IPV6)
			ifp->if_capenable ^= IFCAP_RXCSUM_IPV6;
		if (mask & IFCAP_LRO)
			ifp->if_capenable ^= IFCAP_LRO;
		if (mask & IFCAP_VLAN_HWTAGGING)
			ifp->if_capenable ^= IFCAP_VLAN_HWTAGGING;
		if (mask & IFCAP_VLAN_HWFILTER)
			ifp->if_capenable ^= IFCAP_VLAN_HWFILTER;
		if (mask & IFCAP_VLAN_HWTSO)
			ifp->if_capenable ^= IFCAP_VLAN_HWTSO;
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			ixlv_init(vsi);
		}
		VLAN_CAPABILITIES(ifp);

		break;
	}

	default:
		IOCTL_DBG_IF2(ifp, "UNKNOWN (0x%X)", (int)command);
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

/*
** To do a reinit on the VF is unfortunately more complicated
** than a physical device, we must have the PF more or less
** completely recreate our memory, so many things that were
** done only once at attach in traditional drivers now must be
** redone at each reinitialization. This function does that
** 'prelude' so we can then call the normal locked init code.
*/
int
ixlv_reinit_locked(struct ixlv_sc *sc)
{
	struct i40e_hw		*hw = &sc->hw;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ifnet		*ifp = vsi->ifp;
	struct ixlv_mac_filter  *mf, *mf_temp;
	struct ixlv_vlan_filter	*vf;
	int			error = 0;

	INIT_DBG_IF(ifp, "begin");

	if (ifp->if_drv_flags & IFF_DRV_RUNNING)
		ixlv_stop(sc);

	error = ixlv_reset(sc);

	INIT_DBG_IF(ifp, "VF was reset");

	/* set the state in case we went thru RESET */
	sc->init_state = IXLV_RUNNING;

	/*
	** Resetting the VF drops all filters from hardware;
	** we need to mark them to be re-added in init.
	*/
	SLIST_FOREACH_SAFE(mf, sc->mac_filters, next, mf_temp) {
		if (mf->flags & IXL_FILTER_DEL) {
			SLIST_REMOVE(sc->mac_filters, mf,
			    ixlv_mac_filter, next);
			free(mf, M_DEVBUF);
		} else
			mf->flags |= IXL_FILTER_ADD;
	}
	if (vsi->num_vlans != 0)
		SLIST_FOREACH(vf, sc->vlan_filters, next)
			vf->flags = IXL_FILTER_ADD;
	else { /* clean any stale filters */
		while (!SLIST_EMPTY(sc->vlan_filters)) {
			vf = SLIST_FIRST(sc->vlan_filters);
			SLIST_REMOVE_HEAD(sc->vlan_filters, next);
			free(vf, M_DEVBUF);
		}
	}

	ixlv_enable_adminq_irq(hw);
	ixl_vc_flush(&sc->vc_mgr);

	INIT_DBG_IF(ifp, "end");
	return (error);
}

static void
ixl_init_cmd_complete(struct ixl_vc_cmd *cmd, void *arg,
	enum i40e_status_code code)
{
	struct ixlv_sc *sc;

	sc = arg;

	/*
	 * Ignore "Adapter Stopped" message as that happens if an ifconfig down
	 * happens while a command is in progress, so we don't print an error
	 * in that case.
	 */
	if (code != I40E_SUCCESS && code != I40E_ERR_ADAPTER_STOPPED) {
		if_printf(sc->vsi.ifp,
		    "Error %s waiting for PF to complete operation %d\n",
		    i40e_stat_str(&sc->hw, code), cmd->request);
	}
}

static void
ixlv_init_locked(struct ixlv_sc *sc)
{
	struct i40e_hw		*hw = &sc->hw;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;
	struct ifnet		*ifp = vsi->ifp;
	int			 error = 0;

	INIT_DBG_IF(ifp, "begin");

	IXLV_CORE_LOCK_ASSERT(sc);

	/* Do a reinit first if an init has already been done */
	if ((sc->init_state == IXLV_RUNNING) ||
	    (sc->init_state == IXLV_RESET_REQUIRED) ||
	    (sc->init_state == IXLV_RESET_PENDING))
		error = ixlv_reinit_locked(sc);
	/* Don't bother with init if we failed reinit */
	if (error)
		goto init_done;

	/* Remove existing MAC filter if new MAC addr is set */
	if (bcmp(IF_LLADDR(ifp), hw->mac.addr, ETHER_ADDR_LEN) != 0) {
		error = ixlv_del_mac_filter(sc, hw->mac.addr);
		if (error == 0)
			ixl_vc_enqueue(&sc->vc_mgr, &sc->del_mac_cmd, 
			    IXLV_FLAG_AQ_DEL_MAC_FILTER, ixl_init_cmd_complete,
			    sc);
	}

	/* Check for an LAA mac address... */
	bcopy(IF_LLADDR(ifp), hw->mac.addr, ETHER_ADDR_LEN);

	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_OFFLOAD_IPV4 & ~CSUM_IP);
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= CSUM_OFFLOAD_IPV6;

	/* Add mac filter for this VF to PF */
	if (i40e_validate_mac_addr(hw->mac.addr) == I40E_SUCCESS) {
		error = ixlv_add_mac_filter(sc, hw->mac.addr, 0);
		if (!error || error == EEXIST)
			ixl_vc_enqueue(&sc->vc_mgr, &sc->add_mac_cmd,
			    IXLV_FLAG_AQ_ADD_MAC_FILTER, ixl_init_cmd_complete,
			    sc);
	}

	/* Setup vlan's if needed */
	ixlv_setup_vlan_filters(sc);

	/* Prepare the queues for operation */
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct  rx_ring	*rxr = &que->rxr;

		ixl_init_tx_ring(que);

		if (vsi->max_frame_size <= MCLBYTES)
			rxr->mbuf_sz = MCLBYTES;
		else
			rxr->mbuf_sz = MJUMPAGESIZE;
		ixl_init_rx_ring(que);
	}

	/* Set initial ITR values */
	ixlv_configure_itr(sc);

	/* Configure queues */
	ixl_vc_enqueue(&sc->vc_mgr, &sc->config_queues_cmd,
	    IXLV_FLAG_AQ_CONFIGURE_QUEUES, ixl_init_cmd_complete, sc);

	/* Set up RSS */
	ixlv_config_rss(sc);

	/* Map vectors */
	ixl_vc_enqueue(&sc->vc_mgr, &sc->map_vectors_cmd, 
	    IXLV_FLAG_AQ_MAP_VECTORS, ixl_init_cmd_complete, sc);

	/* Enable queues */
	ixl_vc_enqueue(&sc->vc_mgr, &sc->enable_queues_cmd,
	    IXLV_FLAG_AQ_ENABLE_QUEUES, ixl_init_cmd_complete, sc);

	/* Start the local timer */
	callout_reset(&sc->timer, hz, ixlv_local_timer, sc);

	sc->init_state = IXLV_RUNNING;

init_done:
	INIT_DBG_IF(ifp, "end");
	return;
}

/*
**  Init entry point for the stack
*/
void
ixlv_init(void *arg)
{
	struct ixl_vsi *vsi = (struct ixl_vsi *)arg;
	struct ixlv_sc *sc = vsi->back;
	int retries = 0;

	/* Prevent init from running again while waiting for AQ calls
	 * made in init_locked() to complete. */
	mtx_lock(&sc->mtx);
	if (sc->init_in_progress) {
		mtx_unlock(&sc->mtx);
		return;
	} else
		sc->init_in_progress = true;

	ixlv_init_locked(sc);
	mtx_unlock(&sc->mtx);

	/* Wait for init_locked to finish */
	while (!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING)
	    && ++retries < IXLV_AQ_MAX_ERR) {
		i40e_msec_pause(25);
	}
	if (retries >= IXLV_AQ_MAX_ERR) {
		if_printf(vsi->ifp,
		    "Init failed to complete in allotted time!\n");
	}

	mtx_lock(&sc->mtx);
	sc->init_in_progress = false;
	mtx_unlock(&sc->mtx);
}

/*
 * ixlv_attach() helper function; gathers information about
 * the (virtual) hardware for use elsewhere in the driver.
 */
static void
ixlv_init_hw(struct ixlv_sc *sc)
{
	struct i40e_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	
	/* Save off the information about this board */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);
}

/*
 * ixlv_attach() helper function; initalizes the admin queue
 * and attempts to establish contact with the PF by
 * retrying the initial "API version" message several times
 * or until the PF responds.
 */
static int
ixlv_setup_vc(struct ixlv_sc *sc)
{
	struct i40e_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int error = 0, ret_error = 0, asq_retries = 0;
	bool send_api_ver_retried = 0;

	/* Need to set these AQ paramters before initializing AQ */
	hw->aq.num_arq_entries = IXL_AQ_LEN;
	hw->aq.num_asq_entries = IXL_AQ_LEN;
	hw->aq.arq_buf_size = IXL_AQ_BUF_SZ;
	hw->aq.asq_buf_size = IXL_AQ_BUF_SZ;

	for (int i = 0; i < IXLV_AQ_MAX_ERR; i++) {
		/* Initialize admin queue */
		error = i40e_init_adminq(hw);
		if (error) {
			device_printf(dev, "%s: init_adminq failed: %d\n",
			    __func__, error);
			ret_error = 1;
			continue;
		}

		INIT_DBG_DEV(dev, "Initialized Admin Queue; starting"
		    " send_api_ver attempt %d", i+1);

retry_send:
		/* Send VF's API version */
		error = ixlv_send_api_ver(sc);
		if (error) {
			i40e_shutdown_adminq(hw);
			ret_error = 2;
			device_printf(dev, "%s: unable to send api"
			    " version to PF on attempt %d, error %d\n",
			    __func__, i+1, error);
		}

		asq_retries = 0;
		while (!i40e_asq_done(hw)) {
			if (++asq_retries > IXLV_AQ_MAX_ERR) {
				i40e_shutdown_adminq(hw);
				device_printf(dev, "Admin Queue timeout "
				    "(waiting for send_api_ver), %d more tries...\n",
				    IXLV_AQ_MAX_ERR - (i + 1));
				ret_error = 3;
				break;
			} 
			i40e_msec_pause(10);
		}
		if (asq_retries > IXLV_AQ_MAX_ERR)
			continue;

		INIT_DBG_DEV(dev, "Sent API version message to PF");

		/* Verify that the VF accepts the PF's API version */
		error = ixlv_verify_api_ver(sc);
		if (error == ETIMEDOUT) {
			if (!send_api_ver_retried) {
				/* Resend message, one more time */
				send_api_ver_retried++;
				device_printf(dev,
				    "%s: Timeout while verifying API version on first"
				    " try!\n", __func__);
				goto retry_send;
			} else {
				device_printf(dev,
				    "%s: Timeout while verifying API version on second"
				    " try!\n", __func__);
				ret_error = 4;
				break;
			}
		}
		if (error) {
			device_printf(dev,
			    "%s: Unable to verify API version,"
			    " error %s\n", __func__, i40e_stat_str(hw, error));
			ret_error = 5;
		}
		break;
	}

	if (ret_error >= 4)
		i40e_shutdown_adminq(hw);
	return (ret_error);
}

/*
 * ixlv_attach() helper function; asks the PF for this VF's
 * configuration, and saves the information if it receives it.
 */
static int
ixlv_vf_config(struct ixlv_sc *sc)
{
	struct i40e_hw *hw = &sc->hw;
	device_t dev = sc->dev;
	int bufsz, error = 0, ret_error = 0;
	int asq_retries, retried = 0;

retry_config:
	error = ixlv_send_vf_config_msg(sc);
	if (error) {
		device_printf(dev,
		    "%s: Unable to send VF config request, attempt %d,"
		    " error %d\n", __func__, retried + 1, error);
		ret_error = 2;
	}

	asq_retries = 0;
	while (!i40e_asq_done(hw)) {
		if (++asq_retries > IXLV_AQ_MAX_ERR) {
			device_printf(dev, "%s: Admin Queue timeout "
			    "(waiting for send_vf_config_msg), attempt %d\n",
			    __func__, retried + 1);
			ret_error = 3;
			goto fail;
		}
		i40e_msec_pause(10);
	}

	INIT_DBG_DEV(dev, "Sent VF config message to PF, attempt %d",
	    retried + 1);

	if (!sc->vf_res) {
		bufsz = sizeof(struct i40e_virtchnl_vf_resource) +
		    (I40E_MAX_VF_VSI * sizeof(struct i40e_virtchnl_vsi_resource));
		sc->vf_res = malloc(bufsz, M_DEVBUF, M_NOWAIT);
		if (!sc->vf_res) {
			device_printf(dev,
			    "%s: Unable to allocate memory for VF configuration"
			    " message from PF on attempt %d\n", __func__, retried + 1);
			ret_error = 1;
			goto fail;
		}
	}

	/* Check for VF config response */
	error = ixlv_get_vf_config(sc);
	if (error == ETIMEDOUT) {
		/* The 1st time we timeout, send the configuration message again */
		if (!retried) {
			retried++;
			goto retry_config;
		}
		device_printf(dev,
		    "%s: ixlv_get_vf_config() timed out waiting for a response\n",
		    __func__);
	}
	if (error) {
		device_printf(dev,
		    "%s: Unable to get VF configuration from PF after %d tries!\n",
		    __func__, retried + 1);
		ret_error = 4;
	}
	goto done;

fail:
	free(sc->vf_res, M_DEVBUF);
done:
	return (ret_error);
}

/*
 * Allocate MSI/X vectors, setup the AQ vector early
 */
static int
ixlv_init_msix(struct ixlv_sc *sc)
{
	device_t dev = sc->dev;
	int rid, want, vectors, queues, available;
	int auto_max_queues;

	rid = PCIR_BAR(IXL_BAR);
	sc->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (!sc->msix_mem) {
		/* May not be enabled */
		device_printf(sc->dev,
		    "Unable to map MSIX table\n");
		goto fail;
	}

	available = pci_msix_count(dev); 
	if (available == 0) { /* system has msix disabled */
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, sc->msix_mem);
		sc->msix_mem = NULL;
		goto fail;
	}

	/* Clamp queues to number of CPUs and # of MSI-X vectors available */
	auto_max_queues = min(mp_ncpus, available - 1);
	/* Clamp queues to # assigned to VF by PF */
	auto_max_queues = min(auto_max_queues, sc->vf_res->num_queue_pairs);

	/* Override with tunable value if tunable is less than autoconfig count */
	if ((ixlv_max_queues != 0) && (ixlv_max_queues <= auto_max_queues))
		queues = ixlv_max_queues;
	/* Use autoconfig amount if that's lower */
	else if ((ixlv_max_queues != 0) && (ixlv_max_queues > auto_max_queues)) {
		device_printf(dev, "ixlv_max_queues (%d) is too large, using "
		    "autoconfig amount (%d)...\n",
		    ixlv_max_queues, auto_max_queues);
		queues = auto_max_queues;
	}
	/* Limit maximum auto-configured queues to 8 if no user value is set */
	else
		queues = min(auto_max_queues, 8);

#ifdef  RSS
	/* If we're doing RSS, clamp at the number of RSS buckets */
	if (queues > rss_getnumbuckets())
		queues = rss_getnumbuckets();
#endif

	/*
	** Want one vector (RX/TX pair) per queue
	** plus an additional for the admin queue.
	*/
	want = queues + 1;
	if (want <= available)	/* Have enough */
		vectors = want;
	else {
		device_printf(sc->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors available but %d wanted!\n",
		    available, want);
		goto fail;
	}

#ifdef RSS
	/*
	* If we're doing RSS, the number of queues needs to
	* match the number of RSS buckets that are configured.
	*
	* + If there's more queues than RSS buckets, we'll end
	*   up with queues that get no traffic.
	*
	* + If there's more RSS buckets than queues, we'll end
	*   up having multiple RSS buckets map to the same queue,
	*   so there'll be some contention.
	*/
	if (queues != rss_getnumbuckets()) {
		device_printf(dev,
		    "%s: queues (%d) != RSS buckets (%d)"
		    "; performance will be impacted.\n",
		     __func__, queues, rss_getnumbuckets());
	}
#endif

	if (pci_alloc_msix(dev, &vectors) == 0) {
		device_printf(sc->dev,
		    "Using MSIX interrupts with %d vectors\n", vectors);
		sc->msix = vectors;
		sc->vsi.num_queues = queues;
	}

	/* Next we need to setup the vector for the Admin Queue */
	rid = 1;	// zero vector + 1
	sc->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &rid, RF_SHAREABLE | RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev,"Unable to allocate"
		    " bus resource: AQ interrupt \n");
		goto fail;
	}
	if (bus_setup_intr(dev, sc->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixlv_msix_adminq, sc, &sc->tag)) {
		sc->res = NULL;
		device_printf(dev, "Failed to register AQ handler");
		goto fail;
	}
	bus_describe_intr(dev, sc->res, sc->tag, "adminq");

	return (vectors);

fail:
	/* The VF driver MUST use MSIX */
	return (0);
}

static int
ixlv_allocate_pci_resources(struct ixlv_sc *sc)
{
	int             rid;
	device_t        dev = sc->dev;

	rid = PCIR_BAR(0);
	sc->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(sc->pci_mem)) {
		device_printf(dev, "Unable to allocate bus resource: memory\n");
		return (ENXIO);
	}

	sc->osdep.mem_bus_space_tag =
		rman_get_bustag(sc->pci_mem);
	sc->osdep.mem_bus_space_handle =
		rman_get_bushandle(sc->pci_mem);
	sc->osdep.mem_bus_space_size = rman_get_size(sc->pci_mem);
	sc->osdep.flush_reg = I40E_VFGEN_RSTAT;
	sc->hw.hw_addr = (u8 *) &sc->osdep.mem_bus_space_handle;

	sc->hw.back = &sc->osdep;

	/*
	** Explicitly set the guest PCI BUSMASTER capability
	** and we must rewrite the ENABLE in the MSIX control
	** register again at this point to cause the host to
	** successfully initialize us.
	**
	** This must be set before accessing any registers.
	*/
	{
		u16 pci_cmd_word;
		int msix_ctrl;
		pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
		pci_cmd_word |= PCIM_CMD_BUSMASTEREN;
		pci_write_config(dev, PCIR_COMMAND, pci_cmd_word, 2);
		pci_find_cap(dev, PCIY_MSIX, &rid);
		rid += PCIR_MSIX_CTRL;
		msix_ctrl = pci_read_config(dev, rid, 2);
		msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
		pci_write_config(dev, rid, msix_ctrl, 2);
	}

	/* Disable adminq interrupts (just in case) */
	ixlv_disable_adminq_irq(&sc->hw);

	return (0);
}

static void
ixlv_free_pci_resources(struct ixlv_sc *sc)
{
	struct ixl_vsi         *vsi = &sc->vsi;
	struct ixl_queue       *que = vsi->queues;
	device_t                dev = sc->dev;

	/* We may get here before stations are setup */
	if (que == NULL)
		goto early;

	/*
	**  Release all msix queue resources:
	*/
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		int rid = que->msix + 1;
		if (que->tag != NULL) {
			bus_teardown_intr(dev, que->res, que->tag);
			que->tag = NULL;
		}
		if (que->res != NULL) {
			bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
			que->res = NULL;
		}
	}
        
early:
	/* Clean the AdminQ interrupt */
	if (sc->tag != NULL) {
		bus_teardown_intr(dev, sc->res, sc->tag);
		sc->tag = NULL;
	}
	if (sc->res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, 1, sc->res);
		sc->res = NULL;
	}

	pci_release_msi(dev);

	if (sc->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(IXL_BAR), sc->msix_mem);

	if (sc->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), sc->pci_mem);
}

/*
 * Create taskqueue and tasklet for Admin Queue interrupts.
 */
static int
ixlv_init_taskqueue(struct ixlv_sc *sc)
{
	int error = 0;

	TASK_INIT(&sc->aq_irq, 0, ixlv_do_adminq, sc);

	sc->tq = taskqueue_create_fast("ixl_adm", M_NOWAIT,
	    taskqueue_thread_enqueue, &sc->tq);
	taskqueue_start_threads(&sc->tq, 1, PI_NET, "%s sc->tq",
	    device_get_nameunit(sc->dev));

	return (error);
}

/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers for the VSI queues
 *
 **********************************************************************/
static int
ixlv_assign_msix(struct ixlv_sc *sc)
{
	device_t	dev = sc->dev;
	struct 		ixl_vsi *vsi = &sc->vsi;
	struct 		ixl_queue *que = vsi->queues;
	struct		tx_ring	 *txr;
	int 		error, rid, vector = 1;
#ifdef	RSS
	cpuset_t	cpu_mask;
#endif

	for (int i = 0; i < vsi->num_queues; i++, vector++, que++) {
		int cpu_id = i;
		rid = vector + 1;
		txr = &que->txr;
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
		    ixlv_msix_que, que, &que->tag);
		if (error) {
			que->res = NULL;
			device_printf(dev, "Failed to register que handler");
			return (error);
		}
		bus_describe_intr(dev, que->res, que->tag, "que %d", i);
		/* Bind the vector to a CPU */
#ifdef RSS
		cpu_id = rss_getcpu(i % rss_getnumbuckets());
#endif
		bus_bind_intr(dev, que->res, cpu_id);
		que->msix = vector;
		TASK_INIT(&que->tx_task, 0, ixl_deferred_mq_start, que);
		TASK_INIT(&que->task, 0, ixlv_handle_que, que);
		que->tq = taskqueue_create_fast("ixlv_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
#ifdef RSS
		CPU_SETOF(cpu_id, &cpu_mask);
		taskqueue_start_threads_cpuset(&que->tq, 1, PI_NET,
		    &cpu_mask, "%s (bucket %d)",
		    device_get_nameunit(dev), cpu_id);
#else
                taskqueue_start_threads(&que->tq, 1, PI_NET,
                    "%s que", device_get_nameunit(dev));
#endif

	}

	return (0);
}

/*
** Requests a VF reset from the PF.
**
** Requires the VF's Admin Queue to be initialized.
*/
static int
ixlv_reset(struct ixlv_sc *sc)
{
	struct i40e_hw	*hw = &sc->hw;
	device_t	dev = sc->dev;
	int		error = 0;

	/* Ask the PF to reset us if we are initiating */
	if (sc->init_state != IXLV_RESET_PENDING)
		ixlv_request_reset(sc);

	i40e_msec_pause(100);
	error = ixlv_reset_complete(hw);
	if (error) {
		device_printf(dev, "%s: VF reset failed\n",
		    __func__);
		return (error);
	}

	error = i40e_shutdown_adminq(hw);
	if (error) {
		device_printf(dev, "%s: shutdown_adminq failed: %d\n",
		    __func__, error);
		return (error);
	}

	error = i40e_init_adminq(hw);
	if (error) {
		device_printf(dev, "%s: init_adminq failed: %d\n",
		    __func__, error);
		return(error);
	}

	return (0);
}

static int
ixlv_reset_complete(struct i40e_hw *hw)
{
	u32 reg;

	/* Wait up to ~10 seconds */
	for (int i = 0; i < 100; i++) {
		reg = rd32(hw, I40E_VFGEN_RSTAT) &
		    I40E_VFGEN_RSTAT_VFR_STATE_MASK;

                if ((reg == I40E_VFR_VFACTIVE) ||
		    (reg == I40E_VFR_COMPLETED))
			return (0);
		i40e_msec_pause(100);
	}

	return (EBUSY);
}


/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
static int
ixlv_setup_interface(device_t dev, struct ixlv_sc *sc)
{
	struct ifnet		*ifp;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;

	INIT_DBG_DEV(dev, "begin");

	ifp = vsi->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "%s: could not allocate ifnet"
		    " structure!\n", __func__);
		return (-1);
	}

	if_initname(ifp, device_get_name(dev), device_get_unit(dev));

	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = IF_Gbps(40);
	ifp->if_init = ixlv_init;
	ifp->if_softc = vsi;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixlv_ioctl;

#if __FreeBSD_version >= 1100000
	if_setgetcounterfn(ifp, ixl_get_counter);
#endif

	ifp->if_transmit = ixl_mq_start;

	ifp->if_qflush = ixl_qflush;
	ifp->if_snd.ifq_maxlen = que->num_desc - 2;

	ether_ifattach(ifp, sc->hw.mac.addr);

	vsi->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM;
	ifp->if_capabilities |= IFCAP_HWCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_TSO;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;

	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING
			     |  IFCAP_VLAN_HWTSO
			     |  IFCAP_VLAN_MTU
			     |  IFCAP_VLAN_HWCSUM
			     |  IFCAP_LRO;
	ifp->if_capenable = ifp->if_capabilities;

	/*
	** Don't turn this on by default, if vlans are
	** created on another pseudo device (eg. lagg)
	** then vlan events are not passed thru, breaking
	** operation, but with HW FILTER off it works. If
	** using vlans directly on the ixl driver you can
	** enable this and get full hardware tag filtering.
	*/
	ifp->if_capabilities |= IFCAP_VLAN_HWFILTER;

	/*
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, ixlv_media_change,
		     ixlv_media_status);

	// JFV Add media types later?

	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	INIT_DBG_DEV(dev, "end");
	return (0);
}

/*
** Allocate and setup the interface queues
*/
static int
ixlv_setup_queues(struct ixlv_sc *sc)
{
	device_t		dev = sc->dev;
	struct ixl_vsi		*vsi;
	struct ixl_queue	*que;
	struct tx_ring		*txr;
	struct rx_ring		*rxr;
	int 			rsize, tsize;
	int			error = I40E_SUCCESS;

	vsi = &sc->vsi;
	vsi->back = (void *)sc;
	vsi->hw = &sc->hw;
	vsi->num_vlans = 0;

	/* Get memory for the station queues */
	if (!(vsi->queues =
		(struct ixl_queue *) malloc(sizeof(struct ixl_queue) *
		vsi->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
			device_printf(dev, "Unable to allocate queue memory\n");
			error = ENOMEM;
			goto early;
	}

	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		que->num_desc = ixlv_ringsz;
		que->me = i;
		que->vsi = vsi;
		/* mark the queue as active */
		vsi->active_queues |= (u64)1 << que->me;

		txr = &que->txr;
		txr->que = que;
		txr->tail = I40E_QTX_TAIL1(que->me);
		/* Initialize the TX lock */
		snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
		    device_get_nameunit(dev), que->me);
		mtx_init(&txr->mtx, txr->mtx_name, NULL, MTX_DEF);
		/*
		** Create the TX descriptor ring, the extra int is
		** added as the location for HEAD WB.
		*/
		tsize = roundup2((que->num_desc *
		    sizeof(struct i40e_tx_desc)) +
		    sizeof(u32), DBA_ALIGN);
		if (i40e_allocate_dma_mem(&sc->hw,
		    &txr->dma, i40e_mem_reserved, tsize, DBA_ALIGN)) {
			device_printf(dev,
			    "Unable to allocate TX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		txr->base = (struct i40e_tx_desc *)txr->dma.va;
		bzero((void *)txr->base, tsize);
		/* Now allocate transmit soft structs for the ring */
		if (ixl_allocate_tx_data(que)) {
			device_printf(dev,
			    "Critical Failure setting up TX structures\n");
			error = ENOMEM;
			goto fail;
		}
		/* Allocate a buf ring */
		txr->br = buf_ring_alloc(ixlv_txbrsz, M_DEVBUF,
		    M_WAITOK, &txr->mtx);
		if (txr->br == NULL) {
			device_printf(dev,
			    "Critical Failure setting up TX buf ring\n");
			error = ENOMEM;
			goto fail;
		}

		/*
		 * Next the RX queues...
		 */ 
		rsize = roundup2(que->num_desc *
		    sizeof(union i40e_rx_desc), DBA_ALIGN);
		rxr = &que->rxr;
		rxr->que = que;
		rxr->tail = I40E_QRX_TAIL1(que->me);

		/* Initialize the RX side lock */
		snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
		    device_get_nameunit(dev), que->me);
		mtx_init(&rxr->mtx, rxr->mtx_name, NULL, MTX_DEF);

		if (i40e_allocate_dma_mem(&sc->hw,
		    &rxr->dma, i40e_mem_reserved, rsize, 4096)) { //JFV - should this be DBA?
			device_printf(dev,
			    "Unable to allocate RX Descriptor memory\n");
			error = ENOMEM;
			goto fail;
		}
		rxr->base = (union i40e_rx_desc *)rxr->dma.va;
		bzero((void *)rxr->base, rsize);

		/* Allocate receive soft structs for the ring */
		if (ixl_allocate_rx_data(que)) {
			device_printf(dev,
			    "Critical Failure setting up receive structs\n");
			error = ENOMEM;
			goto fail;
		}
	}

	return (0);

fail:
	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		rxr = &que->rxr;
		txr = &que->txr;
		if (rxr->base)
			i40e_free_dma_mem(&sc->hw, &rxr->dma);
		if (txr->base)
			i40e_free_dma_mem(&sc->hw, &txr->dma);
	}
	free(vsi->queues, M_DEVBUF);

early:
	return (error);
}

/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
static void
ixlv_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi		*vsi = arg;
	struct ixlv_sc		*sc = vsi->back;
	struct ixlv_vlan_filter	*v;


	if (ifp->if_softc != arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	/* Sanity check - make sure it doesn't already exist */
	SLIST_FOREACH(v, sc->vlan_filters, next) {
		if (v->vlan == vtag)
			return;
	}

	mtx_lock(&sc->mtx);
	++vsi->num_vlans;
	v = malloc(sizeof(struct ixlv_vlan_filter), M_DEVBUF, M_NOWAIT | M_ZERO);
	SLIST_INSERT_HEAD(sc->vlan_filters, v, next);
	v->vlan = vtag;
	v->flags = IXL_FILTER_ADD;
	ixl_vc_enqueue(&sc->vc_mgr, &sc->add_vlan_cmd,
	    IXLV_FLAG_AQ_ADD_VLAN_FILTER, ixl_init_cmd_complete, sc);
	mtx_unlock(&sc->mtx);
	return;
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
static void
ixlv_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi		*vsi = arg;
	struct ixlv_sc		*sc = vsi->back;
	struct ixlv_vlan_filter	*v;
	int			i = 0;
	
	if (ifp->if_softc != arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	mtx_lock(&sc->mtx);
	SLIST_FOREACH(v, sc->vlan_filters, next) {
		if (v->vlan == vtag) {
			v->flags = IXL_FILTER_DEL;
			++i;
			--vsi->num_vlans;
		}
	}
	if (i)
		ixl_vc_enqueue(&sc->vc_mgr, &sc->del_vlan_cmd,
		    IXLV_FLAG_AQ_DEL_VLAN_FILTER, ixl_init_cmd_complete, sc);
	mtx_unlock(&sc->mtx);
	return;
}

/*
** Get a new filter and add it to the mac filter list.
*/
static struct ixlv_mac_filter *
ixlv_get_mac_filter(struct ixlv_sc *sc)
{
	struct ixlv_mac_filter	*f;

	f = malloc(sizeof(struct ixlv_mac_filter),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (f)
		SLIST_INSERT_HEAD(sc->mac_filters, f, next);

	return (f);
}

/*
** Find the filter with matching MAC address
*/
static struct ixlv_mac_filter *
ixlv_find_mac_filter(struct ixlv_sc *sc, u8 *macaddr)
{
	struct ixlv_mac_filter	*f;
	bool				match = FALSE;

	SLIST_FOREACH(f, sc->mac_filters, next) {
		if (cmp_etheraddr(f->macaddr, macaddr)) {
			match = TRUE;
			break;
		}
	}	

	if (!match)
		f = NULL;
	return (f);
}

/*
** Admin Queue interrupt handler
*/
static void
ixlv_msix_adminq(void *arg)
{
	struct ixlv_sc	*sc = arg;
	struct i40e_hw	*hw = &sc->hw;
	u32		reg, mask;

        reg = rd32(hw, I40E_VFINT_ICR01);
        mask = rd32(hw, I40E_VFINT_ICR0_ENA1);

        reg = rd32(hw, I40E_VFINT_DYN_CTL01);
        reg |= I40E_VFINT_DYN_CTL01_CLEARPBA_MASK;
        wr32(hw, I40E_VFINT_DYN_CTL01, reg);

	/* schedule task */
	taskqueue_enqueue(sc->tq, &sc->aq_irq);
	return;
}

void
ixlv_enable_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;

	ixlv_enable_adminq_irq(hw);
	for (int i = 0; i < vsi->num_queues; i++, que++)
		ixlv_enable_queue_irq(hw, que->me);
}

void
ixlv_disable_intr(struct ixl_vsi *vsi)
{
        struct i40e_hw          *hw = vsi->hw;
        struct ixl_queue       *que = vsi->queues;

	ixlv_disable_adminq_irq(hw);
	for (int i = 0; i < vsi->num_queues; i++, que++)
		ixlv_disable_queue_irq(hw, que->me);
}


static void
ixlv_disable_adminq_irq(struct i40e_hw *hw)
{
	wr32(hw, I40E_VFINT_DYN_CTL01, 0);
	wr32(hw, I40E_VFINT_ICR0_ENA1, 0);
	/* flush */
	rd32(hw, I40E_VFGEN_RSTAT);
	return;
}

static void
ixlv_enable_adminq_irq(struct i40e_hw *hw)
{
	wr32(hw, I40E_VFINT_DYN_CTL01,
	    I40E_VFINT_DYN_CTL01_INTENA_MASK |
	    I40E_VFINT_DYN_CTL01_ITR_INDX_MASK);
	wr32(hw, I40E_VFINT_ICR0_ENA1, I40E_VFINT_ICR0_ENA1_ADMINQ_MASK);
	/* flush */
	rd32(hw, I40E_VFGEN_RSTAT);
	return;
}

static void
ixlv_enable_queue_irq(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = I40E_VFINT_DYN_CTLN1_INTENA_MASK |
	    I40E_VFINT_DYN_CTLN1_CLEARPBA_MASK |
	    I40E_VFINT_DYN_CTLN1_ITR_INDX_MASK;
	wr32(hw, I40E_VFINT_DYN_CTLN1(id), reg);
}

static void
ixlv_disable_queue_irq(struct i40e_hw *hw, int id)
{
	wr32(hw, I40E_VFINT_DYN_CTLN1(id),
	    I40E_VFINT_DYN_CTLN1_ITR_INDX_MASK);
	rd32(hw, I40E_VFGEN_RSTAT);
	return;
}

/*
 * Get initial ITR values from tunable values.
 */
static void
ixlv_configure_itr(struct ixlv_sc *sc)
{
	struct i40e_hw		*hw = &sc->hw;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;

	vsi->rx_itr_setting = ixlv_rx_itr;
	vsi->tx_itr_setting = ixlv_tx_itr;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring	*txr = &que->txr;
		struct rx_ring 	*rxr = &que->rxr;

		wr32(hw, I40E_VFINT_ITRN1(IXL_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = IXL_AVE_LATENCY;

		wr32(hw, I40E_VFINT_ITRN1(IXL_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = IXL_AVE_LATENCY;
	}
}

/*
** Provide a update to the queue RX
** interrupt moderation value.
*/
static void
ixlv_set_queue_rx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct rx_ring	*rxr = &que->rxr;
	u16		rx_itr;
	u16		rx_latency = 0;
	int		rx_bytes;


	/* Idle, do nothing */
	if (rxr->bytes == 0)
		return;

	if (ixlv_dynamic_rx_itr) {
		rx_bytes = rxr->bytes/rxr->itr;
		rx_itr = rxr->itr;

		/* Adjust latency range */
		switch (rxr->latency) {
		case IXL_LOW_LATENCY:
			if (rx_bytes > 10) {
				rx_latency = IXL_AVE_LATENCY;
				rx_itr = IXL_ITR_20K;
			}
			break;
		case IXL_AVE_LATENCY:
			if (rx_bytes > 20) {
				rx_latency = IXL_BULK_LATENCY;
				rx_itr = IXL_ITR_8K;
			} else if (rx_bytes <= 10) {
				rx_latency = IXL_LOW_LATENCY;
				rx_itr = IXL_ITR_100K;
			}
			break;
		case IXL_BULK_LATENCY:
			if (rx_bytes <= 20) {
				rx_latency = IXL_AVE_LATENCY;
				rx_itr = IXL_ITR_20K;
			}
			break;
       		 }

		rxr->latency = rx_latency;

		if (rx_itr != rxr->itr) {
			/* do an exponential smoothing */
			rx_itr = (10 * rx_itr * rxr->itr) /
			    ((9 * rx_itr) + rxr->itr);
			rxr->itr = rx_itr & IXL_MAX_ITR;
			wr32(hw, I40E_VFINT_ITRN1(IXL_RX_ITR,
			    que->me), rxr->itr);
		}
	} else { /* We may have have toggled to non-dynamic */
		if (vsi->rx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->rx_itr_setting = ixlv_rx_itr;
		/* Update the hardware if needed */
		if (rxr->itr != vsi->rx_itr_setting) {
			rxr->itr = vsi->rx_itr_setting;
			wr32(hw, I40E_VFINT_ITRN1(IXL_RX_ITR,
			    que->me), rxr->itr);
		}
	}
	rxr->bytes = 0;
	rxr->packets = 0;
	return;
}


/*
** Provide a update to the queue TX
** interrupt moderation value.
*/
static void
ixlv_set_queue_tx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	u16		tx_itr;
	u16		tx_latency = 0;
	int		tx_bytes;


	/* Idle, do nothing */
	if (txr->bytes == 0)
		return;

	if (ixlv_dynamic_tx_itr) {
		tx_bytes = txr->bytes/txr->itr;
		tx_itr = txr->itr;

		switch (txr->latency) {
		case IXL_LOW_LATENCY:
			if (tx_bytes > 10) {
				tx_latency = IXL_AVE_LATENCY;
				tx_itr = IXL_ITR_20K;
			}
			break;
		case IXL_AVE_LATENCY:
			if (tx_bytes > 20) {
				tx_latency = IXL_BULK_LATENCY;
				tx_itr = IXL_ITR_8K;
			} else if (tx_bytes <= 10) {
				tx_latency = IXL_LOW_LATENCY;
				tx_itr = IXL_ITR_100K;
			}
			break;
		case IXL_BULK_LATENCY:
			if (tx_bytes <= 20) {
				tx_latency = IXL_AVE_LATENCY;
				tx_itr = IXL_ITR_20K;
			}
			break;
		}

		txr->latency = tx_latency;

		if (tx_itr != txr->itr) {
       	         /* do an exponential smoothing */
			tx_itr = (10 * tx_itr * txr->itr) /
			    ((9 * tx_itr) + txr->itr);
			txr->itr = tx_itr & IXL_MAX_ITR;
			wr32(hw, I40E_VFINT_ITRN1(IXL_TX_ITR,
			    que->me), txr->itr);
		}

	} else { /* We may have have toggled to non-dynamic */
		if (vsi->tx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->tx_itr_setting = ixlv_tx_itr;
		/* Update the hardware if needed */
		if (txr->itr != vsi->tx_itr_setting) {
			txr->itr = vsi->tx_itr_setting;
			wr32(hw, I40E_VFINT_ITRN1(IXL_TX_ITR,
			    que->me), txr->itr);
		}
	}
	txr->bytes = 0;
	txr->packets = 0;
	return;
}


/*
**
** MSIX Interrupt Handlers and Tasklets
**
*/
static void
ixlv_handle_que(void *context, int pending)
{
	struct ixl_queue *que = context;
	struct ixl_vsi *vsi = que->vsi;
	struct i40e_hw  *hw = vsi->hw;
	struct tx_ring  *txr = &que->txr;
	struct ifnet    *ifp = vsi->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixl_rxeof(que, IXL_RX_LIMIT);
		mtx_lock(&txr->mtx);
		ixl_txeof(que);
		if (!drbr_empty(ifp, txr->br))
			ixl_mq_start_locked(ifp, txr);
		mtx_unlock(&txr->mtx);
		if (more) {
			taskqueue_enqueue(que->tq, &que->task);
			return;
		}
	}

	/* Reenable this interrupt - hmmm */
	ixlv_enable_queue_irq(hw, que->me);
	return;
}


/*********************************************************************
 *
 *  MSIX Queue Interrupt Service routine
 *
 **********************************************************************/
static void
ixlv_msix_que(void *arg)
{
	struct ixl_queue	*que = arg;
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	bool		more_tx, more_rx;

	/* Spurious interrupts are ignored */
	if (!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	++que->irqs;

	more_rx = ixl_rxeof(que, IXL_RX_LIMIT);

	mtx_lock(&txr->mtx);
	more_tx = ixl_txeof(que);
	/*
	** Make certain that if the stack 
	** has anything queued the task gets
	** scheduled to handle it.
	*/
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	mtx_unlock(&txr->mtx);

	ixlv_set_queue_rx_itr(que);
	ixlv_set_queue_tx_itr(que);

	if (more_tx || more_rx)
		taskqueue_enqueue(que->tq, &que->task);
	else
		ixlv_enable_queue_irq(hw, que->me);

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
ixlv_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct ixl_vsi		*vsi = ifp->if_softc;
	struct ixlv_sc	*sc = vsi->back;

	INIT_DBG_IF(ifp, "begin");

	mtx_lock(&sc->mtx);

	ixlv_update_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_up) {
		mtx_unlock(&sc->mtx);
		INIT_DBG_IF(ifp, "end: link not up");
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;
	/* Hardware is always full-duplex */
	ifmr->ifm_active |= IFM_FDX;
	mtx_unlock(&sc->mtx);
	INIT_DBG_IF(ifp, "end");
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
ixlv_media_change(struct ifnet * ifp)
{
	struct ixl_vsi *vsi = ifp->if_softc;
	struct ifmedia *ifm = &vsi->media;

	INIT_DBG_IF(ifp, "begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	INIT_DBG_IF(ifp, "end");
	return (0);
}


/*********************************************************************
 *  Multicast Initialization
 *
 *  This routine is called by init to reset a fresh state.
 *
 **********************************************************************/

static void
ixlv_init_multi(struct ixl_vsi *vsi)
{
	struct ixlv_mac_filter *f;
	struct ixlv_sc	*sc = vsi->back;
	int			mcnt = 0;

	IOCTL_DBG_IF(vsi->ifp, "begin");

	/* First clear any multicast filters */
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if ((f->flags & IXL_FILTER_USED)
		    && (f->flags & IXL_FILTER_MC)) {
			f->flags |= IXL_FILTER_DEL;
			mcnt++;
		}
	}
	if (mcnt > 0)
		ixl_vc_enqueue(&sc->vc_mgr, &sc->del_multi_cmd,
		    IXLV_FLAG_AQ_DEL_MAC_FILTER, ixl_init_cmd_complete,
		    sc);

	IOCTL_DBG_IF(vsi->ifp, "end");
}

static void
ixlv_add_multi(struct ixl_vsi *vsi)
{
	struct ifmultiaddr	*ifma;
	struct ifnet		*ifp = vsi->ifp;
	struct ixlv_sc	*sc = vsi->back;
	int			mcnt = 0;

	IOCTL_DBG_IF(ifp, "begin");

	if_maddr_rlock(ifp);
	/*
	** Get a count, to decide if we
	** simply use multicast promiscuous.
	*/
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mcnt++;
	}
	if_maddr_runlock(ifp);

	/* TODO: Remove -- cannot set promiscuous mode in a VF */
	if (__predict_false(mcnt >= MAX_MULTICAST_ADDR)) {
		/* delete all multicast filters */
		ixlv_init_multi(vsi);
		sc->promiscuous_flags |= I40E_FLAG_VF_MULTICAST_PROMISC;
		ixl_vc_enqueue(&sc->vc_mgr, &sc->add_multi_cmd,
		    IXLV_FLAG_AQ_CONFIGURE_PROMISC, ixl_init_cmd_complete,
		    sc);
		IOCTL_DEBUGOUT("%s: end: too many filters", __func__);
		return;
	}

	mcnt = 0;
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		if (!ixlv_add_mac_filter(sc,
		    (u8*)LLADDR((struct sockaddr_dl *) ifma->ifma_addr),
		    IXL_FILTER_MC))
			mcnt++;
	}
	if_maddr_runlock(ifp);
	/*
	** Notify AQ task that sw filters need to be
	** added to hw list
	*/
	if (mcnt > 0)
		ixl_vc_enqueue(&sc->vc_mgr, &sc->add_multi_cmd,
		    IXLV_FLAG_AQ_ADD_MAC_FILTER, ixl_init_cmd_complete,
		    sc);

	IOCTL_DBG_IF(ifp, "end");
}

static void
ixlv_del_multi(struct ixl_vsi *vsi)
{
	struct ixlv_mac_filter *f;
	struct ifmultiaddr	*ifma;
	struct ifnet		*ifp = vsi->ifp;
	struct ixlv_sc	*sc = vsi->back;
	int			mcnt = 0;
	bool		match = FALSE;

	IOCTL_DBG_IF(ifp, "begin");

	/* Search for removed multicast addresses */
	if_maddr_rlock(ifp);
	SLIST_FOREACH(f, sc->mac_filters, next) {
		if ((f->flags & IXL_FILTER_USED)
		    && (f->flags & IXL_FILTER_MC)) {
			/* check if mac address in filter is in sc's list */
			match = FALSE;
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				u8 *mc_addr =
				    (u8 *)LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
				if (cmp_etheraddr(f->macaddr, mc_addr)) {
					match = TRUE;
					break;
				}
			}
			/* if this filter is not in the sc's list, remove it */
			if (match == FALSE && !(f->flags & IXL_FILTER_DEL)) {
				f->flags |= IXL_FILTER_DEL;
				mcnt++;
				IOCTL_DBG_IF(ifp, "marked: " MAC_FORMAT,
				    MAC_FORMAT_ARGS(f->macaddr));
			}
			else if (match == FALSE)
				IOCTL_DBG_IF(ifp, "exists: " MAC_FORMAT,
				    MAC_FORMAT_ARGS(f->macaddr));
		}
	}
	if_maddr_runlock(ifp);

	if (mcnt > 0)
		ixl_vc_enqueue(&sc->vc_mgr, &sc->del_multi_cmd,
		    IXLV_FLAG_AQ_DEL_MAC_FILTER, ixl_init_cmd_complete,
		    sc);

	IOCTL_DBG_IF(ifp, "end");
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 **********************************************************************/

static void
ixlv_local_timer(void *arg)
{
	struct ixlv_sc	*sc = arg;
	struct i40e_hw		*hw = &sc->hw;
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = sc->dev;
	int			hung = 0;
	u32			mask, val;

	IXLV_CORE_LOCK_ASSERT(sc);

	/* If Reset is in progress just bail */
	if (sc->init_state == IXLV_RESET_PENDING)
		return;

	/* Check for when PF triggers a VF reset */
	val = rd32(hw, I40E_VFGEN_RSTAT) &
	    I40E_VFGEN_RSTAT_VFR_STATE_MASK;

	if (val != I40E_VFR_VFACTIVE
	    && val != I40E_VFR_COMPLETED) {
		DDPRINTF(dev, "reset in progress! (%d)", val);
		return;
	}

	ixlv_request_stats(sc);

	/* clean and process any events */
	taskqueue_enqueue(sc->tq, &sc->aq_irq);

	/*
	** Check status on the queues for a hang
	*/
	mask = (I40E_VFINT_DYN_CTLN1_INTENA_MASK |
	    I40E_VFINT_DYN_CTLN1_SWINT_TRIG_MASK |
	    I40E_VFINT_DYN_CTLN1_ITR_INDX_MASK);

	for (int i = 0; i < vsi->num_queues; i++,que++) {
		/* Any queues with outstanding work get a sw irq */
		if (que->busy)
			wr32(hw, I40E_VFINT_DYN_CTLN1(que->me), mask);
		/*
		** Each time txeof runs without cleaning, but there
		** are uncleaned descriptors it increments busy. If
		** we get to 5 we declare it hung.
		*/
		if (que->busy == IXL_QUEUE_HUNG) {
			++hung;
			/* Mark the queue as inactive */
			vsi->active_queues &= ~((u64)1 << que->me);
			continue;
		} else {
			/* Check if we've come back from hung */
			if ((vsi->active_queues & ((u64)1 << que->me)) == 0)
				vsi->active_queues |= ((u64)1 << que->me);
		}
		if (que->busy >= IXL_MAX_TX_BUSY) {
			device_printf(dev,"Warning queue %d "
			    "appears to be hung!\n", i);
			que->busy = IXL_QUEUE_HUNG;
			++hung;
		}
	}
	/* Only reset when all queues show hung */
	if (hung == vsi->num_queues)
		goto hung;
	callout_reset(&sc->timer, hz, ixlv_local_timer, sc);
	return;

hung:
	device_printf(dev, "Local Timer: TX HANG DETECTED - Resetting!!\n");
	sc->init_state = IXLV_RESET_REQUIRED;
	ixlv_init_locked(sc);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
void
ixlv_update_link_status(struct ixlv_sc *sc)
{
	struct ixl_vsi		*vsi = &sc->vsi;
	struct ifnet		*ifp = vsi->ifp;

	if (sc->link_up){ 
		if (vsi->link_active == FALSE) {
			if (bootverbose)
				if_printf(ifp,"Link is Up, %d Gbps\n",
				    (sc->link_speed == I40E_LINK_SPEED_40GB) ? 40:10);
			vsi->link_active = TRUE;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (vsi->link_active == TRUE) {
			if (bootverbose)
				if_printf(ifp,"Link is Down\n");
			if_link_state_change(ifp, LINK_STATE_DOWN);
			vsi->link_active = FALSE;
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
ixlv_stop(struct ixlv_sc *sc)
{
	struct ifnet *ifp;
	int start;

	ifp = sc->vsi.ifp;
	INIT_DBG_IF(ifp, "begin");

	IXLV_CORE_LOCK_ASSERT(sc);

	ixl_vc_flush(&sc->vc_mgr);
	ixlv_disable_queues(sc);

	start = ticks;
	while ((ifp->if_drv_flags & IFF_DRV_RUNNING) &&
	    ((ticks - start) < hz/10))
		ixlv_do_adminq_locked(sc);

	/* Stop the local timer */
	callout_stop(&sc->timer);

	INIT_DBG_IF(ifp, "end");
}


/*********************************************************************
 *
 *  Free all station queue structs.
 *
 **********************************************************************/
static void
ixlv_free_queues(struct ixl_vsi *vsi)
{
	struct ixlv_sc	*sc = (struct ixlv_sc *)vsi->back;
	struct ixl_queue	*que = vsi->queues;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		struct rx_ring *rxr = &que->rxr;
	
		if (!mtx_initialized(&txr->mtx)) /* uninitialized */
			continue;
		IXL_TX_LOCK(txr);
		ixl_free_que_tx(que);
		if (txr->base)
			i40e_free_dma_mem(&sc->hw, &txr->dma);
		IXL_TX_UNLOCK(txr);
		IXL_TX_LOCK_DESTROY(txr);

		if (!mtx_initialized(&rxr->mtx)) /* uninitialized */
			continue;
		IXL_RX_LOCK(rxr);
		ixl_free_que_rx(que);
		if (rxr->base)
			i40e_free_dma_mem(&sc->hw, &rxr->dma);
		IXL_RX_UNLOCK(rxr);
		IXL_RX_LOCK_DESTROY(rxr);
		
	}
	free(vsi->queues, M_DEVBUF);
}

static void
ixlv_config_rss_reg(struct ixlv_sc *sc)
{
	struct i40e_hw	*hw = &sc->hw;
	struct ixl_vsi	*vsi = &sc->vsi;
	u32		lut = 0;
	u64		set_hena = 0, hena;
	int		i, j, que_id;
	u32		rss_seed[IXL_RSS_KEY_SIZE_REG];
#ifdef RSS
	u32		rss_hash_config;
#endif
        
	/* Don't set up RSS if using a single queue */
	if (vsi->num_queues == 1) {
		wr32(hw, I40E_VFQF_HENA(0), 0);
		wr32(hw, I40E_VFQF_HENA(1), 0);
		ixl_flush(hw);
		return;
	}

#ifdef RSS
	/* Fetch the configured RSS key */
	rss_getkey((uint8_t *) &rss_seed);
#else
	ixl_get_default_rss_key(rss_seed);
#endif

	/* Fill out hash function seed */
	for (i = 0; i < IXL_RSS_KEY_SIZE_REG; i++)
                wr32(hw, I40E_VFQF_HKEY(i), rss_seed[i]);

	/* Enable PCTYPES for RSS: */
#ifdef RSS
	rss_hash_config = rss_gethashconfig();
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_TCP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV4)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV4_UDP);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_OTHER);
	if (rss_hash_config & RSS_HASHTYPE_RSS_IPV6_EX)
		set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_FRAG_IPV6);
	if (rss_hash_config & RSS_HASHTYPE_RSS_TCP_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_TCP);
        if (rss_hash_config & RSS_HASHTYPE_RSS_UDP_IPV6)
                set_hena |= ((u64)1 << I40E_FILTER_PCTYPE_NONF_IPV6_UDP);
#else
	set_hena = IXL_DEFAULT_RSS_HENA;
#endif
	hena = (u64)rd32(hw, I40E_VFQF_HENA(0)) |
	    ((u64)rd32(hw, I40E_VFQF_HENA(1)) << 32);
	hena |= set_hena;
	wr32(hw, I40E_VFQF_HENA(0), (u32)hena);
	wr32(hw, I40E_VFQF_HENA(1), (u32)(hena >> 32));

	/* Populate the LUT with max no. of queues in round robin fashion */
	for (i = 0, j = 0; i < IXL_RSS_VSI_LUT_SIZE; i++, j++) {
                if (j == vsi->num_queues)
                        j = 0;
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % vsi->num_queues;
#else
		que_id = j;
#endif
                /* lut = 4-byte sliding window of 4 lut entries */
                lut = (lut << 8) | (que_id & IXL_RSS_VF_LUT_ENTRY_MASK);
                /* On i = 3, we have 4 entries in lut; write to the register */
                if ((i & 3) == 3) {
                        wr32(hw, I40E_VFQF_HLUT(i >> 2), lut);
			DDPRINTF(sc->dev, "HLUT(%2d): %#010x", i, lut);
		}
        }
	ixl_flush(hw);
}

static void
ixlv_config_rss_pf(struct ixlv_sc *sc)
{
	ixl_vc_enqueue(&sc->vc_mgr, &sc->config_rss_key_cmd,
	    IXLV_FLAG_AQ_CONFIG_RSS_KEY, ixl_init_cmd_complete, sc);

	ixl_vc_enqueue(&sc->vc_mgr, &sc->set_rss_hena_cmd,
	    IXLV_FLAG_AQ_SET_RSS_HENA, ixl_init_cmd_complete, sc);

	ixl_vc_enqueue(&sc->vc_mgr, &sc->config_rss_lut_cmd,
	    IXLV_FLAG_AQ_CONFIG_RSS_LUT, ixl_init_cmd_complete, sc);
}

/*
** ixlv_config_rss - setup RSS 
**
** RSS keys and table are cleared on VF reset.
*/
static void
ixlv_config_rss(struct ixlv_sc *sc)
{
	if (sc->vf_res->vf_offload_flags & I40E_VIRTCHNL_VF_OFFLOAD_RSS_REG) {
		DDPRINTF(sc->dev, "Setting up RSS using VF registers...");
		ixlv_config_rss_reg(sc);
	} else if (sc->vf_res->vf_offload_flags & I40E_VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		DDPRINTF(sc->dev, "Setting up RSS using messages to PF...");
		ixlv_config_rss_pf(sc);
	} else
		device_printf(sc->dev, "VF does not support RSS capability sent by PF.\n");
}

/*
** This routine refreshes vlan filters, called by init
** it scans the filter table and then updates the AQ
*/
static void
ixlv_setup_vlan_filters(struct ixlv_sc *sc)
{
	struct ixl_vsi			*vsi = &sc->vsi;
	struct ixlv_vlan_filter	*f;
	int				cnt = 0;

	if (vsi->num_vlans == 0)
		return;
	/*
	** Scan the filter table for vlan entries,
	** and if found call for the AQ update.
	*/
	SLIST_FOREACH(f, sc->vlan_filters, next)
                if (f->flags & IXL_FILTER_ADD)
			cnt++;
	if (cnt > 0)
		ixl_vc_enqueue(&sc->vc_mgr, &sc->add_vlan_cmd,
		    IXLV_FLAG_AQ_ADD_VLAN_FILTER, ixl_init_cmd_complete, sc);
}


/*
** This routine adds new MAC filters to the sc's list;
** these are later added in hardware by sending a virtual
** channel message.
*/
static int
ixlv_add_mac_filter(struct ixlv_sc *sc, u8 *macaddr, u16 flags)
{
	struct ixlv_mac_filter	*f;

	/* Does one already exist? */
	f = ixlv_find_mac_filter(sc, macaddr);
	if (f != NULL) {
		IDPRINTF(sc->vsi.ifp, "exists: " MAC_FORMAT,
		    MAC_FORMAT_ARGS(macaddr));
		return (EEXIST);
	}

	/* If not, get a new empty filter */
	f = ixlv_get_mac_filter(sc);
	if (f == NULL) {
		if_printf(sc->vsi.ifp, "%s: no filters available!!\n",
		    __func__);
		return (ENOMEM);
	}

	IDPRINTF(sc->vsi.ifp, "marked: " MAC_FORMAT,
	    MAC_FORMAT_ARGS(macaddr));

	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->flags |= (IXL_FILTER_ADD | IXL_FILTER_USED);
	f->flags |= flags;
	return (0);
}

/*
** Marks a MAC filter for deletion.
*/
static int
ixlv_del_mac_filter(struct ixlv_sc *sc, u8 *macaddr)
{
	struct ixlv_mac_filter	*f;

	f = ixlv_find_mac_filter(sc, macaddr);
	if (f == NULL)
		return (ENOENT);

	f->flags |= IXL_FILTER_DEL;
	return (0);
}

/*
** Tasklet handler for MSIX Adminq interrupts
**  - done outside interrupt context since it might sleep
*/
static void
ixlv_do_adminq(void *context, int pending)
{
	struct ixlv_sc		*sc = context;

	mtx_lock(&sc->mtx);
	ixlv_do_adminq_locked(sc);
	mtx_unlock(&sc->mtx);
	return;
}

static void
ixlv_do_adminq_locked(struct ixlv_sc *sc)
{
	struct i40e_hw			*hw = &sc->hw;
	struct i40e_arq_event_info	event;
	struct i40e_virtchnl_msg	*v_msg;
	device_t			dev = sc->dev;
	u16				result = 0;
	u32				reg, oldreg;
	i40e_status			ret;

	IXLV_CORE_LOCK_ASSERT(sc);

	event.buf_len = IXL_AQ_BUF_SZ;
        event.msg_buf = sc->aq_buffer;
	v_msg = (struct i40e_virtchnl_msg *)&event.desc;

	do {
		ret = i40e_clean_arq_element(hw, &event, &result);
		if (ret)
			break;
		ixlv_vc_completion(sc, v_msg->v_opcode,
		    v_msg->v_retval, event.msg_buf, event.msg_len);
		if (result != 0)
			bzero(event.msg_buf, IXL_AQ_BUF_SZ);
	} while (result);

	/* check for Admin queue errors */
	oldreg = reg = rd32(hw, hw->aq.arq.len);
	if (reg & I40E_VF_ARQLEN1_ARQVFE_MASK) {
		device_printf(dev, "ARQ VF Error detected\n");
		reg &= ~I40E_VF_ARQLEN1_ARQVFE_MASK;
	}
	if (reg & I40E_VF_ARQLEN1_ARQOVFL_MASK) {
		device_printf(dev, "ARQ Overflow Error detected\n");
		reg &= ~I40E_VF_ARQLEN1_ARQOVFL_MASK;
	}
	if (reg & I40E_VF_ARQLEN1_ARQCRIT_MASK) {
		device_printf(dev, "ARQ Critical Error detected\n");
		reg &= ~I40E_VF_ARQLEN1_ARQCRIT_MASK;
	}
	if (oldreg != reg)
		wr32(hw, hw->aq.arq.len, reg);

	oldreg = reg = rd32(hw, hw->aq.asq.len);
	if (reg & I40E_VF_ATQLEN1_ATQVFE_MASK) {
		device_printf(dev, "ASQ VF Error detected\n");
		reg &= ~I40E_VF_ATQLEN1_ATQVFE_MASK;
	}
	if (reg & I40E_VF_ATQLEN1_ATQOVFL_MASK) {
		device_printf(dev, "ASQ Overflow Error detected\n");
		reg &= ~I40E_VF_ATQLEN1_ATQOVFL_MASK;
	}
	if (reg & I40E_VF_ATQLEN1_ATQCRIT_MASK) {
		device_printf(dev, "ASQ Critical Error detected\n");
		reg &= ~I40E_VF_ATQLEN1_ATQCRIT_MASK;
	}
	if (oldreg != reg)
		wr32(hw, hw->aq.asq.len, reg);

	ixlv_enable_adminq_irq(hw);
}

static void
ixlv_add_sysctls(struct ixlv_sc *sc)
{
	device_t dev = sc->dev;
	struct ixl_vsi *vsi = &sc->vsi;
	struct i40e_eth_stats *es = &vsi->eth_stats;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	struct sysctl_oid *vsi_node, *queue_node;
	struct sysctl_oid_list *vsi_list, *queue_list;

#define QUEUE_NAME_LEN 32
	char queue_namebuf[QUEUE_NAME_LEN];

	struct ixl_queue *queues = vsi->queues;
	struct tx_ring *txr;
	struct rx_ring *rxr;

	/* Driver statistics sysctls */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
			CTLFLAG_RD, &sc->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "admin_irq",
			CTLFLAG_RD, &sc->admin_irq,
			"Admin Queue IRQ Handled");

	/* VSI statistics sysctls */
	vsi_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "vsi",
				   CTLFLAG_RD, NULL, "VSI-specific statistics");
	vsi_list = SYSCTL_CHILDREN(vsi_node);

	struct ixl_sysctl_info ctls[] =
	{
		{&es->rx_bytes, "good_octets_rcvd", "Good Octets Received"},
		{&es->rx_unicast, "ucast_pkts_rcvd",
			"Unicast Packets Received"},
		{&es->rx_multicast, "mcast_pkts_rcvd",
			"Multicast Packets Received"},
		{&es->rx_broadcast, "bcast_pkts_rcvd",
			"Broadcast Packets Received"},
		{&es->rx_discards, "rx_discards", "Discarded RX packets"},
		{&es->rx_unknown_protocol, "rx_unknown_proto", "RX unknown protocol packets"},
		{&es->tx_bytes, "good_octets_txd", "Good Octets Transmitted"},
		{&es->tx_unicast, "ucast_pkts_txd", "Unicast Packets Transmitted"},
		{&es->tx_multicast, "mcast_pkts_txd",
			"Multicast Packets Transmitted"},
		{&es->tx_broadcast, "bcast_pkts_txd",
			"Broadcast Packets Transmitted"},
		{&es->tx_errors, "tx_errors", "TX packet errors"},
		// end
		{0,0,0}
	};
	struct ixl_sysctl_info *entry = ctls;
	while (entry->stat != NULL)
	{
		SYSCTL_ADD_QUAD(ctx, child, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}

	/* Queue sysctls */
	for (int q = 0; q < vsi->num_queues; q++) {
		snprintf(queue_namebuf, QUEUE_NAME_LEN, "que%d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list, OID_AUTO, queue_namebuf,
					     CTLFLAG_RD, NULL, "Queue Name");
		queue_list = SYSCTL_CHILDREN(queue_node);

		txr = &(queues[q].txr);
		rxr = &(queues[q].rxr);

		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "mbuf_defrag_failed",
				CTLFLAG_RD, &(queues[q].mbuf_defrag_failed),
				"m_defrag() failed");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "dropped",
				CTLFLAG_RD, &(queues[q].dropped_pkts),
				"Driver dropped packets");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "irqs",
				CTLFLAG_RD, &(queues[q].irqs),
				"irqs on this queue");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "tso_tx",
				CTLFLAG_RD, &(queues[q].tso),
				"TSO");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "tx_dmamap_failed",
				CTLFLAG_RD, &(queues[q].tx_dmamap_failed),
				"Driver tx dma failure in xmit");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "no_desc_avail",
				CTLFLAG_RD, &(txr->no_desc),
				"Queue No Descriptor Available");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "tx_packets",
				CTLFLAG_RD, &(txr->total_packets),
				"Queue Packets Transmitted");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "tx_bytes",
				CTLFLAG_RD, &(txr->tx_bytes),
				"Queue Bytes Transmitted");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "rx_packets",
				CTLFLAG_RD, &(rxr->rx_packets),
				"Queue Packets Received");
		SYSCTL_ADD_QUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
				CTLFLAG_RD, &(rxr->rx_bytes),
				"Queue Bytes Received");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "rx_itr",
				CTLFLAG_RD, &(rxr->itr), 0,
				"Queue Rx ITR Interval");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "tx_itr",
				CTLFLAG_RD, &(txr->itr), 0,
				"Queue Tx ITR Interval");

#ifdef IXL_DEBUG
		/* Examine queue state */
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "qtx_head", 
				CTLTYPE_UINT | CTLFLAG_RD, &queues[q],
				sizeof(struct ixl_queue),
				ixlv_sysctl_qtx_tail_handler, "IU",
				"Queue Transmit Descriptor Tail");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "qrx_head", 
				CTLTYPE_UINT | CTLFLAG_RD, &queues[q],
				sizeof(struct ixl_queue),
				ixlv_sysctl_qrx_tail_handler, "IU",
				"Queue Receive Descriptor Tail");
#endif
	}
}

static void
ixlv_init_filters(struct ixlv_sc *sc)
{
	sc->mac_filters = malloc(sizeof(struct ixlv_mac_filter),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	SLIST_INIT(sc->mac_filters);
	sc->vlan_filters = malloc(sizeof(struct ixlv_vlan_filter),
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	SLIST_INIT(sc->vlan_filters);
	return;
}

static void
ixlv_free_filters(struct ixlv_sc *sc)
{
	struct ixlv_mac_filter *f;
	struct ixlv_vlan_filter *v;

	while (!SLIST_EMPTY(sc->mac_filters)) {
		f = SLIST_FIRST(sc->mac_filters);
		SLIST_REMOVE_HEAD(sc->mac_filters, next);
		free(f, M_DEVBUF);
	}
	while (!SLIST_EMPTY(sc->vlan_filters)) {
		v = SLIST_FIRST(sc->vlan_filters);
		SLIST_REMOVE_HEAD(sc->vlan_filters, next);
		free(v, M_DEVBUF);
	}
	return;
}

#ifdef IXL_DEBUG
/**
 * ixlv_sysctl_qtx_tail_handler
 * Retrieves I40E_QTX_TAIL1 value from hardware
 * for a sysctl.
 */
static int 
ixlv_sysctl_qtx_tail_handler(SYSCTL_HANDLER_ARGS)
{
	struct ixl_queue *que;
	int error;
	u32 val;

	que = ((struct ixl_queue *)oidp->oid_arg1);
	if (!que) return 0;

	val = rd32(que->vsi->hw, que->txr.tail);
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return (0);
}

/**
 * ixlv_sysctl_qrx_tail_handler
 * Retrieves I40E_QRX_TAIL1 value from hardware
 * for a sysctl.
 */
static int 
ixlv_sysctl_qrx_tail_handler(SYSCTL_HANDLER_ARGS)
{
	struct ixl_queue *que;
	int error;
	u32 val;

	que = ((struct ixl_queue *)oidp->oid_arg1);
	if (!que) return 0;

	val = rd32(que->vsi->hw, que->rxr.tail);
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return (0);
}
#endif

