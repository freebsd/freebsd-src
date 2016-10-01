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


#include "ixl_pf.h"

#ifdef PCI_IOV
#include "ixl_pf_iov.h"
#endif

#ifdef DEV_NETMAP
#include <net/netmap.h>
#include <sys/selinfo.h>
#include <dev/netmap/netmap_kern.h>
#endif /* DEV_NETMAP */

static int	ixl_setup_queue(struct ixl_queue *, struct ixl_pf *, int);

/* Sysctls */
static int	ixl_set_flowcntl(SYSCTL_HANDLER_ARGS);
static int	ixl_set_advertise(SYSCTL_HANDLER_ARGS);
static int	ixl_current_speed(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_show_fw(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_unallocated_queues(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_pf_tx_itr(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_pf_rx_itr(SYSCTL_HANDLER_ARGS);

/* Debug Sysctls */
static int 	ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hw_res_alloc(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_switch_config(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hkey(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hlut(SYSCTL_HANDLER_ARGS);

void
ixl_dbg(struct ixl_pf *pf, enum ixl_dbg_mask mask, char *fmt, ...)
{
	va_list args;

	if (!(mask & pf->dbg_mask))
		return;

	va_start(args, fmt);
	device_printf(pf->dev, fmt, args);
	va_end(args);
}

/*
** Put the FW, API, NVM, EEtrackID, and OEM version information into a string
*/
void
ixl_nvm_version_str(struct i40e_hw *hw, struct sbuf *buf)
{
	u8 oem_ver = (u8)(hw->nvm.oem_ver >> 24);
	u16 oem_build = (u16)((hw->nvm.oem_ver >> 16) & 0xFFFF);
	u8 oem_patch = (u8)(hw->nvm.oem_ver & 0xFF);

	sbuf_printf(buf,
	    "fw %d.%d.%05d api %d.%d nvm %x.%02x etid %08x oem %d.%d.%d",
	    hw->aq.fw_maj_ver, hw->aq.fw_min_ver, hw->aq.fw_build,
	    hw->aq.api_maj_ver, hw->aq.api_min_ver,
	    (hw->nvm.version & IXL_NVM_VERSION_HI_MASK) >>
	    IXL_NVM_VERSION_HI_SHIFT,
	    (hw->nvm.version & IXL_NVM_VERSION_LO_MASK) >>
	    IXL_NVM_VERSION_LO_SHIFT,
	    hw->nvm.eetrack,
	    oem_ver, oem_build, oem_patch);
}

void
ixl_print_nvm_version(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *sbuf;

	sbuf = sbuf_new_auto();
	ixl_nvm_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	device_printf(dev, "%s\n", sbuf_data(sbuf));
	sbuf_delete(sbuf);
}

static void
ixl_configure_tx_itr(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;

	vsi->tx_itr_setting = pf->tx_itr;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring	*txr = &que->txr;

		wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = IXL_AVE_LATENCY;
	}
}

static void
ixl_configure_rx_itr(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;

	vsi->rx_itr_setting = pf->rx_itr;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct rx_ring 	*rxr = &que->rxr;

		wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = IXL_AVE_LATENCY;
	}
}

/*
 * Write PF ITR values to queue ITR registers.
 */
void
ixl_configure_itr(struct ixl_pf *pf)
{
	ixl_configure_tx_itr(pf);
	ixl_configure_rx_itr(pf);
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
void
ixl_init_locked(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;
	device_t 	dev = pf->dev;
	struct i40e_filter_control_settings	filter;
	u8		tmpaddr[ETHER_ADDR_LEN];
	int		ret;

	mtx_assert(&pf->pf_mtx, MA_OWNED);
	INIT_DEBUGOUT("ixl_init_locked: begin");

	ixl_stop_locked(pf);

	/* Get the latest mac address... User might use a LAA */
	bcopy(IF_LLADDR(vsi->ifp), tmpaddr,
	      I40E_ETH_LENGTH_OF_ADDRESS);
	if (!cmp_etheraddr(hw->mac.addr, tmpaddr) &&
	    (i40e_validate_mac_addr(tmpaddr) == I40E_SUCCESS)) {
		ixl_del_filter(vsi, hw->mac.addr, IXL_VLAN_ANY);
		bcopy(tmpaddr, hw->mac.addr,
		    I40E_ETH_LENGTH_OF_ADDRESS);
		ret = i40e_aq_mac_address_write(hw,
		    I40E_AQC_WRITE_TYPE_LAA_ONLY,
		    hw->mac.addr, NULL);
		if (ret) {
			device_printf(dev, "LLA address"
			 "change failed!!\n");
			return;
		}
	}

	ixl_add_filter(vsi, hw->mac.addr, IXL_VLAN_ANY);

	/* Set the various hardware offload abilities */
	ifp->if_hwassist = 0;
	if (ifp->if_capenable & IFCAP_TSO)
		ifp->if_hwassist |= CSUM_TSO;
	if (ifp->if_capenable & IFCAP_TXCSUM)
		ifp->if_hwassist |= (CSUM_TCP | CSUM_UDP);
	if (ifp->if_capenable & IFCAP_TXCSUM_IPV6)
		ifp->if_hwassist |= (CSUM_TCP_IPV6 | CSUM_UDP_IPV6);

	/* Set up the device filtering */
	bzero(&filter, sizeof(filter));
	filter.enable_ethtype = TRUE;
	filter.enable_macvlan = TRUE;
	filter.enable_fdir = FALSE;
	filter.hash_lut_size = I40E_HASH_LUT_SIZE_512;
	if (i40e_set_filter_control(hw, &filter))
		device_printf(dev, "i40e_set_filter_control() failed\n");

	/* Prepare the VSI: rings, hmc contexts, etc... */
	if (ixl_initialize_vsi(vsi)) {
		device_printf(dev, "initialize vsi failed!!\n");
		return;
	}

	/* Set up RSS */
	ixl_config_rss(pf);

	/* Add protocol filters to list */
	ixl_init_filters(vsi);

	/* Setup vlan's if needed */
	ixl_setup_vlan_filters(vsi);

	/* Set up MSI/X routing and the ITR settings */
	if (pf->enable_msix) {
		ixl_configure_queue_intr_msix(pf);
		ixl_configure_itr(pf);
	} else
		ixl_configure_legacy(pf);

	ixl_enable_rings(vsi);

	i40e_aq_set_default_vsi(hw, vsi->seid, NULL);

	ixl_reconfigure_filters(vsi);

	/* And now turn on interrupts */
	ixl_enable_intr(vsi);

	/* Get link info */
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);
	ixl_update_link_status(pf);

	/* Set initial advertised speed sysctl value */
	ixl_get_initial_advertised_speeds(pf);

	/* Start the local timer */
	callout_reset(&pf->timer, hz, ixl_local_timer, pf);

	/* Now inform the stack we're ready */
	ifp->if_drv_flags |= IFF_DRV_RUNNING;
}


/*********************************************************************
 *
 *  Get the hardware capabilities
 *
 **********************************************************************/

int
ixl_get_hw_capabilities(struct ixl_pf *pf)
{
	struct i40e_aqc_list_capabilities_element_resp *buf;
	struct i40e_hw	*hw = &pf->hw;
	device_t 	dev = pf->dev;
	int             error, len;
	u16		needed;
	bool		again = TRUE;

	len = 40 * sizeof(struct i40e_aqc_list_capabilities_element_resp);
retry:
	if (!(buf = (struct i40e_aqc_list_capabilities_element_resp *)
	    malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate cap memory\n");
                return (ENOMEM);
	}

	/* This populates the hw struct */
        error = i40e_aq_discover_capabilities(hw, buf, len,
	    &needed, i40e_aqc_opc_list_func_capabilities, NULL);
	free(buf, M_DEVBUF);
	if ((pf->hw.aq.asq_last_status == I40E_AQ_RC_ENOMEM) &&
	    (again == TRUE)) {
		/* retry once with a larger buffer */
		again = FALSE;
		len = needed;
		goto retry;
	} else if (pf->hw.aq.asq_last_status != I40E_AQ_RC_OK) {
		device_printf(dev, "capability discovery failed: %d\n",
		    pf->hw.aq.asq_last_status);
		return (ENODEV);
	}

	/* Capture this PF's starting queue pair */
	pf->qbase = hw->func_caps.base_queue;

#ifdef IXL_DEBUG
	device_printf(dev, "pf_id=%d, num_vfs=%d, msix_pf=%d, "
	    "msix_vf=%d, fd_g=%d, fd_b=%d, tx_qp=%d rx_qp=%d qbase=%d\n",
	    hw->pf_id, hw->func_caps.num_vfs,
	    hw->func_caps.num_msix_vectors,
	    hw->func_caps.num_msix_vectors_vf,
	    hw->func_caps.fd_filters_guaranteed,
	    hw->func_caps.fd_filters_best_effort,
	    hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp,
	    hw->func_caps.base_queue);
#endif
	/* Print a subset of the capability information. */
	device_printf(dev, "PF-ID[%d]: VFs %d, MSIX %d, VF MSIX %d, QPs %d, %s\n",
	    hw->pf_id, hw->func_caps.num_vfs, hw->func_caps.num_msix_vectors,
	    hw->func_caps.num_msix_vectors_vf, hw->func_caps.num_tx_qp,
	    (hw->func_caps.mdio_port_mode == 2) ? "I2C" :
	    (hw->func_caps.mdio_port_mode == 1) ? "MDIO dedicated" :
	    "MDIO shared");

	return (error);
}

void
ixl_cap_txcsum_tso(struct ixl_vsi *vsi, struct ifnet *ifp, int mask)
{
	device_t 	dev = vsi->dev;

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
			device_printf(dev,
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
			device_printf(dev, 
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
			device_printf(dev,
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
			device_printf(dev,
			    "TSO6 requires txcsum6, disabling both...\n");
		} else if (mask & IFCAP_TSO6)
			ifp->if_capenable &= ~IFCAP_TSO6;
	}
}

/* For the set_advertise sysctl */
void
ixl_get_initial_advertised_speeds(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	enum i40e_status_code status;
	struct i40e_aq_get_phy_abilities_resp abilities;

	/* Set initial sysctl values */
	status = i40e_aq_get_phy_capabilities(hw, FALSE, false, &abilities,
					      NULL);
	if (status) {
		/* Non-fatal error */
		device_printf(dev, "%s: i40e_aq_get_phy_capabilities() error %d\n",
		     __func__, status);
		return;
	}

	if (abilities.link_speed & I40E_LINK_SPEED_40GB)
		pf->advertised_speed |= 0x10;
	if (abilities.link_speed & I40E_LINK_SPEED_20GB)
		pf->advertised_speed |= 0x8;
	if (abilities.link_speed & I40E_LINK_SPEED_10GB)
		pf->advertised_speed |= 0x4;
	if (abilities.link_speed & I40E_LINK_SPEED_1GB)
		pf->advertised_speed |= 0x2;
	if (abilities.link_speed & I40E_LINK_SPEED_100MB)
		pf->advertised_speed |= 0x1;
}

int
ixl_teardown_hw_structs(struct ixl_pf *pf)
{
	enum i40e_status_code status = 0;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;

	/* Shutdown LAN HMC */
	if (hw->hmc.hmc_obj) {
		status = i40e_shutdown_lan_hmc(hw);
		if (status) {
			device_printf(dev,
			    "init: LAN HMC shutdown failure; status %d\n", status);
			goto err_out;
		}
	}

	// XXX: This gets called when we know the adminq is inactive;
	// so we already know it's setup when we get here.

	/* Shutdown admin queue */
	status = i40e_shutdown_adminq(hw);
	if (status)
		device_printf(dev,
		    "init: Admin Queue shutdown failure; status %d\n", status);

err_out:
	return (status);
}

int
ixl_reset(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	u8 set_fc_err_mask;
	int error = 0;

	// XXX: clear_hw() actually writes to hw registers -- maybe this isn't necessary
	i40e_clear_hw(hw);
	error = i40e_pf_reset(hw);
	if (error) {
		device_printf(dev, "init: PF reset failure");
		error = EIO;
		goto err_out;
	}

	error = i40e_init_adminq(hw);
	if (error) {
		device_printf(dev, "init: Admin queue init failure;"
		    " status code %d", error);
		error = EIO;
		goto err_out;
	}

	i40e_clear_pxe_mode(hw);

	error = ixl_get_hw_capabilities(pf);
	if (error) {
		device_printf(dev, "init: Error retrieving HW capabilities;"
		    " status code %d\n", error);
		goto err_out;
	}

	error = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp, 0, 0);
	if (error) {
		device_printf(dev, "init: LAN HMC init failed; status code %d\n",
		    error);
		error = EIO;
		goto err_out;
	}

	error = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (error) {
		device_printf(dev, "init: LAN HMC config failed; status code %d\n",
		    error);
		error = EIO;
		goto err_out;
	}

	// XXX: possible fix for panic, but our failure recovery is still broken
	error = ixl_switch_config(pf);
	if (error) {
		device_printf(dev, "init: ixl_switch_config() failed: %d\n",
		     error);
		goto err_out;
	}

	error = i40e_aq_set_phy_int_mask(hw, IXL_DEFAULT_PHY_INT_MASK,
	    NULL);
        if (error) {
		device_printf(dev, "init: i40e_aq_set_phy_mask() failed: err %d,"
		    " aq_err %d\n", error, hw->aq.asq_last_status);
		error = EIO;
		goto err_out;
	}

	error = i40e_set_fc(hw, &set_fc_err_mask, true);
	if (error) {
		device_printf(dev, "init: setting link flow control failed; retcode %d,"
		    " fc_err_mask 0x%02x\n", error, set_fc_err_mask);
		goto err_out;
	}

	// XXX: (Rebuild VSIs?)

	/* Firmware delay workaround */
	if (((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver < 33)) ||
	    (hw->aq.fw_maj_ver < 4)) {
		i40e_msec_delay(75);
		error = i40e_aq_set_link_restart_an(hw, TRUE, NULL);
		if (error) {
			device_printf(dev, "init: link restart failed, aq_err %d\n",
			    hw->aq.asq_last_status);
			goto err_out;
		}
	}


err_out:
	return (error);
}

/*
** MSIX Interrupt Handlers and Tasklets
*/
void
ixl_handle_que(void *context, int pending)
{
	struct ixl_queue *que = context;
	struct ixl_vsi *vsi = que->vsi;
	struct i40e_hw  *hw = vsi->hw;
	struct tx_ring  *txr = &que->txr;
	struct ifnet    *ifp = vsi->ifp;
	bool		more;

	if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
		more = ixl_rxeof(que, IXL_RX_LIMIT);
		IXL_TX_LOCK(txr);
		ixl_txeof(que);
		if (!drbr_empty(ifp, txr->br))
			ixl_mq_start_locked(ifp, txr);
		IXL_TX_UNLOCK(txr);
		if (more) {
			taskqueue_enqueue(que->tq, &que->task);
			return;
		}
	}

	/* Reenable this interrupt - hmmm */
	ixl_enable_queue(hw, que->me);
	return;
}


/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/
void
ixl_intr(void *arg)
{
	struct ixl_pf		*pf = arg;
	struct i40e_hw		*hw =  &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	struct ifnet		*ifp = vsi->ifp;
	struct tx_ring		*txr = &que->txr;
        u32			reg, icr0, mask;
	bool			more_tx, more_rx;

	++que->irqs;

	/* Protect against spurious interrupts */
	if ((ifp->if_drv_flags & IFF_DRV_RUNNING) == 0)
		return;

	icr0 = rd32(hw, I40E_PFINT_ICR0);

	reg = rd32(hw, I40E_PFINT_DYN_CTL0);
	reg = reg | I40E_PFINT_DYN_CTL0_CLEARPBA_MASK;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);

        mask = rd32(hw, I40E_PFINT_ICR0_ENA);

#ifdef PCI_IOV
	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK)
		taskqueue_enqueue(pf->tq, &pf->vflr_task);
#endif

	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK) {
		taskqueue_enqueue(pf->tq, &pf->adminq);
		return;
	}

	more_rx = ixl_rxeof(que, IXL_RX_LIMIT);

	IXL_TX_LOCK(txr);
	more_tx = ixl_txeof(que);
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	IXL_TX_UNLOCK(txr);

	/* re-enable other interrupt causes */
	wr32(hw, I40E_PFINT_ICR0_ENA, mask);

	/* And now the queues */
	reg = rd32(hw, I40E_QINT_RQCTL(0));
	reg |= I40E_QINT_RQCTL_CAUSE_ENA_MASK;
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = rd32(hw, I40E_QINT_TQCTL(0));
	reg |= I40E_QINT_TQCTL_CAUSE_ENA_MASK;
	reg &= ~I40E_PFINT_ICR0_INTEVENT_MASK;
	wr32(hw, I40E_QINT_TQCTL(0), reg);

	ixl_enable_legacy(hw);

	return;
}


/*********************************************************************
 *
 *  MSIX VSI Interrupt Service routine
 *
 **********************************************************************/
void
ixl_msix_que(void *arg)
{
	struct ixl_queue	*que = arg;
	struct ixl_vsi	*vsi = que->vsi;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	bool		more_tx, more_rx;

	/* Protect against spurious interrupts */
	if (!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING))
		return;

	++que->irqs;

	more_rx = ixl_rxeof(que, IXL_RX_LIMIT);

	IXL_TX_LOCK(txr);
	more_tx = ixl_txeof(que);
	/*
	** Make certain that if the stack 
	** has anything queued the task gets
	** scheduled to handle it.
	*/
	if (!drbr_empty(vsi->ifp, txr->br))
		more_tx = 1;
	IXL_TX_UNLOCK(txr);

	ixl_set_queue_rx_itr(que);
	ixl_set_queue_tx_itr(que);

	if (more_tx || more_rx)
		taskqueue_enqueue(que->tq, &que->task);
	else
		ixl_enable_queue(hw, que->me);

	return;
}


/*********************************************************************
 *
 *  MSIX Admin Queue Interrupt Service routine
 *
 **********************************************************************/
void
ixl_msix_adminq(void *arg)
{
	struct ixl_pf	*pf = arg;
	struct i40e_hw	*hw = &pf->hw;
	device_t	dev = pf->dev;
	u32		reg, mask, rstat_reg;
	bool		do_task = FALSE;

	++pf->admin_irq;

	reg = rd32(hw, I40E_PFINT_ICR0);
	mask = rd32(hw, I40E_PFINT_ICR0_ENA);

	/* Check on the cause */
	if (reg & I40E_PFINT_ICR0_ADMINQ_MASK) {
		mask &= ~I40E_PFINT_ICR0_ADMINQ_MASK;
		do_task = TRUE;
	}

	if (reg & I40E_PFINT_ICR0_MAL_DETECT_MASK) {
		ixl_handle_mdd_event(pf);
		mask &= ~I40E_PFINT_ICR0_MAL_DETECT_MASK;
	}

	if (reg & I40E_PFINT_ICR0_GRST_MASK) {
		device_printf(dev, "Reset Requested!\n");
		rstat_reg = rd32(hw, I40E_GLGEN_RSTAT);
		rstat_reg = (rstat_reg & I40E_GLGEN_RSTAT_RESET_TYPE_MASK)
		    >> I40E_GLGEN_RSTAT_RESET_TYPE_SHIFT;
		device_printf(dev, "Reset type: ");
		switch (rstat_reg) {
		/* These others might be handled similarly to an EMPR reset */
		case I40E_RESET_CORER:
			printf("CORER\n");
			break;
		case I40E_RESET_GLOBR:
			printf("GLOBR\n");
			break;
		case I40E_RESET_EMPR:
			printf("EMPR\n");
			atomic_set_int(&pf->state, IXL_PF_STATE_EMPR_RESETTING);
			break;
		default:
			printf("POR\n");
			break;
		}
		/* overload admin queue task to check reset progress */
		do_task = TRUE;
	}

	if (reg & I40E_PFINT_ICR0_ECC_ERR_MASK) {
		device_printf(dev, "ECC Error detected!\n");
	}

	if (reg & I40E_PFINT_ICR0_HMC_ERR_MASK) {
		reg = rd32(hw, I40E_PFHMC_ERRORINFO);
		if (reg & I40E_PFHMC_ERRORINFO_ERROR_DETECTED_MASK) {
			device_printf(dev, "HMC Error detected!\n");
			device_printf(dev, "INFO 0x%08x\n", reg);
			reg = rd32(hw, I40E_PFHMC_ERRORDATA);
			device_printf(dev, "DATA 0x%08x\n", reg);
			wr32(hw, I40E_PFHMC_ERRORINFO, 0);
		}
	}

	if (reg & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK) {
		device_printf(dev, "PCI Exception detected!\n");
	}

#ifdef PCI_IOV
	if (reg & I40E_PFINT_ICR0_VFLR_MASK) {
		mask &= ~I40E_PFINT_ICR0_ENA_VFLR_MASK;
		taskqueue_enqueue(pf->tq, &pf->vflr_task);
	}
#endif

	if (do_task)
		taskqueue_enqueue(pf->tq, &pf->adminq);
	else
		ixl_enable_adminq(hw);
}

void
ixl_set_promisc(struct ixl_vsi *vsi)
{
	struct ifnet	*ifp = vsi->ifp;
	struct i40e_hw	*hw = vsi->hw;
	int		err, mcnt = 0;
	bool		uni = FALSE, multi = FALSE;

	if (ifp->if_flags & IFF_ALLMULTI)
                multi = TRUE;
	else { /* Need to count the multicast addresses */
		struct  ifmultiaddr *ifma;
		if_maddr_rlock(ifp);
		TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
                        if (ifma->ifma_addr->sa_family != AF_LINK)
                                continue;
                        if (mcnt == MAX_MULTICAST_ADDR)
                                break;
                        mcnt++;
		}
		if_maddr_runlock(ifp);
	}

	if (mcnt >= MAX_MULTICAST_ADDR)
                multi = TRUE;
        if (ifp->if_flags & IFF_PROMISC)
		uni = TRUE;

	err = i40e_aq_set_vsi_unicast_promiscuous(hw,
	    vsi->seid, uni, NULL, TRUE);
	err = i40e_aq_set_vsi_multicast_promiscuous(hw,
	    vsi->seid, multi, NULL);
	return;
}

/*********************************************************************
 * 	Filter Routines
 *
 *	Routines for multicast and vlan filter management.
 *
 *********************************************************************/
void
ixl_add_multi(struct ixl_vsi *vsi)
{
	struct	ifmultiaddr	*ifma;
	struct ifnet		*ifp = vsi->ifp;
	struct i40e_hw		*hw = vsi->hw;
	int			mcnt = 0, flags;

	IOCTL_DEBUGOUT("ixl_add_multi: begin");

	if_maddr_rlock(ifp);
	/*
	** First just get a count, to decide if we
	** we simply use multicast promiscuous.
	*/
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		mcnt++;
	}
	if_maddr_runlock(ifp);

	if (__predict_false(mcnt >= MAX_MULTICAST_ADDR)) {
		/* delete existing MC filters */
		ixl_del_hw_filters(vsi, mcnt);
		i40e_aq_set_vsi_multicast_promiscuous(hw,
		    vsi->seid, TRUE, NULL);
		return;
	}

	mcnt = 0;
	if_maddr_rlock(ifp);
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;
		ixl_add_mc_filter(vsi,
		    (u8*)LLADDR((struct sockaddr_dl *) ifma->ifma_addr));
		mcnt++;
	}
	if_maddr_runlock(ifp);
	if (mcnt > 0) {
		flags = (IXL_FILTER_ADD | IXL_FILTER_USED | IXL_FILTER_MC);
		ixl_add_hw_filters(vsi, flags, mcnt);
	}

	IOCTL_DEBUGOUT("ixl_add_multi: end");
	return;
}

void
ixl_del_multi(struct ixl_vsi *vsi)
{
	struct ifnet		*ifp = vsi->ifp;
	struct ifmultiaddr	*ifma;
	struct ixl_mac_filter	*f;
	int			mcnt = 0;
	bool		match = FALSE;

	IOCTL_DEBUGOUT("ixl_del_multi: begin");

	/* Search for removed multicast addresses */
	if_maddr_rlock(ifp);
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if ((f->flags & IXL_FILTER_USED) && (f->flags & IXL_FILTER_MC)) {
			match = FALSE;
			TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
				if (ifma->ifma_addr->sa_family != AF_LINK)
					continue;
				u8 *mc_addr = (u8 *)LLADDR((struct sockaddr_dl *)ifma->ifma_addr);
				if (cmp_etheraddr(f->macaddr, mc_addr)) {
					match = TRUE;
					break;
				}
			}
			if (match == FALSE) {
				f->flags |= IXL_FILTER_DEL;
				mcnt++;
			}
		}
	}
	if_maddr_runlock(ifp);

	if (mcnt > 0)
		ixl_del_hw_filters(vsi, mcnt);
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog check.
 *
 *  Only runs when the driver is configured UP and RUNNING.
 *
 **********************************************************************/

void
ixl_local_timer(void *arg)
{
	struct ixl_pf		*pf = arg;
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = pf->dev;
	int			hung = 0;
	u32			mask;

	mtx_assert(&pf->pf_mtx, MA_OWNED);

	/* Fire off the adminq task */
	taskqueue_enqueue(pf->tq, &pf->adminq);

	/* Update stats */
	ixl_update_stats_counters(pf);

	/* Check status of the queues */
	mask = (I40E_PFINT_DYN_CTLN_INTENA_MASK |
		I40E_PFINT_DYN_CTLN_SWINT_TRIG_MASK);
 
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		/* Any queues with outstanding work get a sw irq */
		if (que->busy)
			wr32(hw, I40E_PFINT_DYN_CTLN(que->me), mask);
		/*
		** Each time txeof runs without cleaning, but there
		** are uncleaned descriptors it increments busy. If
		** we get to 5 we declare it hung.
		*/
		if (que->busy == IXL_QUEUE_HUNG) {
			++hung;
			continue;
		}
		if (que->busy >= IXL_MAX_TX_BUSY) {
#ifdef IXL_DEBUG
			device_printf(dev, "Warning queue %d "
			    "appears to be hung!\n", i);
#endif
			que->busy = IXL_QUEUE_HUNG;
			++hung;
		}
	}
	/* Only reinit if all queues show hung */
	if (hung == vsi->num_queues)
		goto hung;

	callout_reset(&pf->timer, hz, ixl_local_timer, pf);
	return;

hung:
	device_printf(dev, "Local Timer: HANG DETECT - Resetting!!\n");
	ixl_init_locked(pf);
}

/*
** Note: this routine updates the OS on the link state
**	the real check of the hardware only happens with
**	a link interrupt.
*/
void
ixl_update_link_status(struct ixl_pf *pf)
{
	struct ixl_vsi		*vsi = &pf->vsi;
	struct i40e_hw		*hw = &pf->hw;
	struct ifnet		*ifp = vsi->ifp;
	device_t		dev = pf->dev;

	if (pf->link_up) {
		if (vsi->link_active == FALSE) {
			pf->fc = hw->fc.current_mode;
			if (bootverbose) {
				device_printf(dev, "Link is up %d Gbps %s,"
				    " Flow Control: %s\n",
				    ((pf->link_speed ==
				    I40E_LINK_SPEED_40GB)? 40:10),
				    "Full Duplex", ixl_fc_string[pf->fc]);
			}
			vsi->link_active = TRUE;
			if_link_state_change(ifp, LINK_STATE_UP);
		}
	} else { /* Link down */
		if (vsi->link_active == TRUE) {
			if (bootverbose)
				device_printf(dev, "Link is Down\n");
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

void
ixl_stop_locked(struct ixl_pf *pf)
{
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ifnet	*ifp = vsi->ifp;

	INIT_DEBUGOUT("ixl_stop: begin\n");

	IXL_PF_LOCK_ASSERT(pf);

	/* Stop the local timer */
	callout_stop(&pf->timer);

	ixl_disable_rings_intr(vsi);
	ixl_disable_rings(vsi);

	/* Tell the stack that the interface is no longer active */
	ifp->if_drv_flags &= ~(IFF_DRV_RUNNING);
}

void
ixl_stop(struct ixl_pf *pf)
{
	IXL_PF_LOCK(pf);
	ixl_stop_locked(pf);
	IXL_PF_UNLOCK(pf);

	ixl_teardown_queue_msix(&pf->vsi);
	ixl_free_queue_tqs(&pf->vsi);
}

/*********************************************************************
 *
 *  Setup MSIX Interrupt resources and handlers for the VSI
 *
 **********************************************************************/
int
ixl_assign_vsi_legacy(struct ixl_pf *pf)
{
	device_t        dev = pf->dev;
	struct 		ixl_vsi *vsi = &pf->vsi;
	struct		ixl_queue *que = vsi->queues;
	int 		error, rid = 0;

	if (pf->msix == 1)
		rid = 1;
	pf->res = bus_alloc_resource_any(dev, SYS_RES_IRQ,
	    &rid, RF_SHAREABLE | RF_ACTIVE);
	if (pf->res == NULL) {
		device_printf(dev, "Unable to allocate"
		    " bus resource: vsi legacy/msi interrupt\n");
		return (ENXIO);
	}

	/* Set the handler function */
	error = bus_setup_intr(dev, pf->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixl_intr, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "Failed to register legacy/msi handler\n");
		return (error);
	}
	bus_describe_intr(dev, pf->res, pf->tag, "irq0");
	TASK_INIT(&que->tx_task, 0, ixl_deferred_mq_start, que);
	TASK_INIT(&que->task, 0, ixl_handle_que, que);
	que->tq = taskqueue_create_fast("ixl_que", M_NOWAIT,
	    taskqueue_thread_enqueue, &que->tq);
	taskqueue_start_threads(&que->tq, 1, PI_NET, "%s que",
	    device_get_nameunit(dev));
	TASK_INIT(&pf->adminq, 0, ixl_do_adminq, pf);

	pf->tq = taskqueue_create_fast("ixl_adm", M_NOWAIT,
	    taskqueue_thread_enqueue, &pf->tq);
	taskqueue_start_threads(&pf->tq, 1, PI_NET, "%s adminq",
	    device_get_nameunit(dev));

	return (0);
}

int
ixl_setup_adminq_tq(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	int error = 0;

	/* Tasklet for Admin Queue interrupts */
	TASK_INIT(&pf->adminq, 0, ixl_do_adminq, pf);
#ifdef PCI_IOV
	/* VFLR Tasklet */
	TASK_INIT(&pf->vflr_task, 0, ixl_handle_vflr, pf);
#endif
	/* Create and start Admin Queue taskqueue */
	pf->tq = taskqueue_create_fast("ixl_aq", M_NOWAIT,
	    taskqueue_thread_enqueue, &pf->tq);
	if (!pf->tq) {
		device_printf(dev, "taskqueue_create_fast (for AQ) returned NULL!\n");
		return (ENOMEM);
	}
	error = taskqueue_start_threads(&pf->tq, 1, PI_NET, "%s aq",
	    device_get_nameunit(dev));
	if (error) {
		device_printf(dev, "taskqueue_start_threads (for AQ) error: %d\n",
		    error);
		taskqueue_free(pf->tq);
		return (error);
	}
	return (0);
}

int
ixl_setup_queue_tqs(struct ixl_vsi *vsi)
{
	struct ixl_queue *que = vsi->queues;
	device_t dev = vsi->dev;
#ifdef  RSS
	int		cpu_id = 0;
        cpuset_t	cpu_mask;
#endif

	/* Create queue tasks and start queue taskqueues */
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		TASK_INIT(&que->tx_task, 0, ixl_deferred_mq_start, que);
		TASK_INIT(&que->task, 0, ixl_handle_que, que);
		que->tq = taskqueue_create_fast("ixl_que", M_NOWAIT,
		    taskqueue_thread_enqueue, &que->tq);
#ifdef RSS
		CPU_SETOF(cpu_id, &cpu_mask);
		taskqueue_start_threads_cpuset(&que->tq, 1, PI_NET,
		    &cpu_mask, "%s (bucket %d)",
		    device_get_nameunit(dev), cpu_id);
#else
		taskqueue_start_threads(&que->tq, 1, PI_NET,
		    "%s (que %d)", device_get_nameunit(dev), que->me);
#endif
	}

	return (0);
}

void
ixl_free_adminq_tq(struct ixl_pf *pf)
{
	if (pf->tq) {
		taskqueue_free(pf->tq);
		pf->tq = NULL;
	}
}

void
ixl_free_queue_tqs(struct ixl_vsi *vsi)
{
	struct ixl_queue *que = vsi->queues;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		if (que->tq) {
			taskqueue_free(que->tq);
			que->tq = NULL;
		}
	}
}

int
ixl_setup_adminq_msix(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	int rid, error = 0;

	/* Admin IRQ rid is 1, vector is 0 */
	rid = 1;
	/* Get interrupt resource from bus */
	pf->res = bus_alloc_resource_any(dev,
    	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (!pf->res) {
		device_printf(dev, "bus_alloc_resource_any() for Admin Queue"
		    " interrupt failed [rid=%d]\n", rid);
		return (ENXIO);
	}
	/* Then associate interrupt with handler */
	error = bus_setup_intr(dev, pf->res,
	    INTR_TYPE_NET | INTR_MPSAFE, NULL,
	    ixl_msix_adminq, pf, &pf->tag);
	if (error) {
		pf->res = NULL;
		device_printf(dev, "bus_setup_intr() for Admin Queue"
		    " interrupt handler failed, error %d\n", error);
		return (ENXIO);
	}
	error = bus_describe_intr(dev, pf->res, pf->tag, "aq");
	if (error) {
		/* Probably non-fatal? */
		device_printf(dev, "bus_describe_intr() for Admin Queue"
		    " interrupt name failed, error %d\n", error);
	}
	pf->admvec = 0;

	return (0);
}

/*
 * Allocate interrupt resources from bus and associate an interrupt handler
 * to those for the VSI's queues.
 */
int
ixl_setup_queue_msix(struct ixl_vsi *vsi)
{
	device_t	dev = vsi->dev;
	struct 		ixl_queue *que = vsi->queues;
	struct		tx_ring	 *txr;
	int 		error, rid, vector = 1;

	/* Queue interrupt vector numbers start at 1 (adminq intr is 0) */
	for (int i = 0; i < vsi->num_queues; i++, vector++, que++) {
		int cpu_id = i;
		rid = vector + 1;
		txr = &que->txr;
		que->res = bus_alloc_resource_any(dev, SYS_RES_IRQ, &rid,
		    RF_SHAREABLE | RF_ACTIVE);
		if (!que->res) {
			device_printf(dev, "bus_alloc_resource_any() for"
			    " Queue %d interrupt failed [rid=%d]\n",
			    que->me, rid);
			return (ENXIO);
		}
		/* Set the handler function */
		error = bus_setup_intr(dev, que->res,
		    INTR_TYPE_NET | INTR_MPSAFE, NULL,
		    ixl_msix_que, que, &que->tag);
		if (error) {
			device_printf(dev, "bus_setup_intr() for Queue %d"
			    " interrupt handler failed, error %d\n",
			    que->me, error);
			return (error);
		}
		error = bus_describe_intr(dev, que->res, que->tag, "q%d", i);
		if (error) {
			device_printf(dev, "bus_describe_intr() for Queue %d"
			    " interrupt name failed, error %d\n",
			    que->me, error);
		}
		/* Bind the vector to a CPU */
#ifdef RSS
		cpu_id = rss_getcpu(i % rss_getnumbuckets());
#endif
		error = bus_bind_intr(dev, que->res, cpu_id);
		if (error) {
			device_printf(dev, "bus_bind_intr() for Queue %d"
			    " to CPU %d failed, error %d\n",
			    que->me, cpu_id, error);
		}
		que->msix = vector;
	}

	return (0);
}

/*
 * When used in a virtualized environment PCI BUSMASTER capability may not be set
 * so explicity set it here and rewrite the ENABLE in the MSIX control register
 * at this point to cause the host to successfully initialize us.
 */
void
ixl_set_busmaster(device_t dev)
{
	u16 pci_cmd_word;
	int msix_ctrl, rid;

	pci_cmd_word = pci_read_config(dev, PCIR_COMMAND, 2);
	pci_cmd_word |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, pci_cmd_word, 2);

	pci_find_cap(dev, PCIY_MSIX, &rid);
	rid += PCIR_MSIX_CTRL;
	msix_ctrl = pci_read_config(dev, rid, 2);
	msix_ctrl |= PCIM_MSIXCTRL_MSIX_ENABLE;
	pci_write_config(dev, rid, msix_ctrl, 2);
}

/*
 * Allocate MSI/X vectors from the OS.
 * Returns 0 for legacy, 1 for MSI, >1 for MSIX.
 */
int
ixl_init_msix(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;
	int auto_max_queues;
	int rid, want, vectors, queues, available;

	/* Override by tuneable */
	if (!pf->enable_msix)
		goto no_msix;

	/* Ensure proper operation in virtualized environment */
	ixl_set_busmaster(dev);

	/* First try MSI/X */
	rid = PCIR_BAR(IXL_BAR);
	pf->msix_mem = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &rid, RF_ACTIVE);
       	if (!pf->msix_mem) {
		/* May not be enabled */
		device_printf(pf->dev,
		    "Unable to map MSIX table\n");
		goto no_msix;
	}

	available = pci_msix_count(dev); 
	if (available < 2) {
		/* system has msix disabled (0), or only one vector (1) */
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rid, pf->msix_mem);
		pf->msix_mem = NULL;
		goto no_msix;
	}

	/* Clamp max number of queues based on:
	 * - # of MSI-X vectors available
	 * - # of cpus available
	 * - # of queues that can be assigned to the LAN VSI
	 */
	auto_max_queues = min(mp_ncpus, available - 1);
	if (hw->mac.type == I40E_MAC_X722)
		auto_max_queues = min(auto_max_queues, 128);
	else
		auto_max_queues = min(auto_max_queues, 64);

	/* Override with tunable value if tunable is less than autoconfig count */
	if ((pf->max_queues != 0) && (pf->max_queues <= auto_max_queues))
		queues = pf->max_queues;
	/* Use autoconfig amount if that's lower */
	else if ((pf->max_queues != 0) && (pf->max_queues > auto_max_queues)) {
		device_printf(dev, "ixl_max_queues (%d) is too large, using "
		    "autoconfig amount (%d)...\n",
		    pf->max_queues, auto_max_queues);
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
               	device_printf(pf->dev,
		    "MSIX Configuration Problem, "
		    "%d vectors available but %d wanted!\n",
		    available, want);
		return (0); /* Will go to Legacy setup */
	}

	if (pci_alloc_msix(dev, &vectors) == 0) {
               	device_printf(pf->dev,
		    "Using MSIX interrupts with %d vectors\n", vectors);
		pf->msix = vectors;
		pf->vsi.num_queues = queues;
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
		return (vectors);
	}
no_msix:
	vectors = pci_msi_count(dev);
	pf->vsi.num_queues = 1;
	pf->max_queues = 1;
	pf->enable_msix = 0;
	if (vectors == 1 && pci_alloc_msi(dev, &vectors) == 0)
		device_printf(pf->dev, "Using an MSI interrupt\n");
	else {
		vectors = 0;
		device_printf(pf->dev, "Using a Legacy interrupt\n");
	}
	return (vectors);
}

/*
 * Configure admin queue/misc interrupt cause registers in hardware.
 */
void
ixl_configure_intr0_msix(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u32 reg;

	/* First set up the adminq - vector 0 */
	wr32(hw, I40E_PFINT_ICR0_ENA, 0);  /* disable all */
	rd32(hw, I40E_PFINT_ICR0);         /* read to clear */

	reg = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK |
	    I40E_PFINT_ICR0_ENA_GRST_MASK |
	    I40E_PFINT_ICR0_ENA_HMC_ERR_MASK |
	    I40E_PFINT_ICR0_ENA_ADMINQ_MASK |
	    I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK |
	    I40E_PFINT_ICR0_ENA_VFLR_MASK |
	    I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	/*
	 * 0x7FF is the end of the queue list.
	 * This means we won't use MSI-X vector 0 for a queue interrupt
	 * in MSIX mode.
	 */
	wr32(hw, I40E_PFINT_LNKLST0, 0x7FF);
	/* Value is in 2 usec units, so 0x3E is 62*2 = 124 usecs. */
	wr32(hw, I40E_PFINT_ITR0(IXL_RX_ITR), 0x3E);

	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTL0_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTL0_INTENA_MSK_MASK);

	wr32(hw, I40E_PFINT_STAT_CTL0, 0);
}

/*
 * Configure queue interrupt cause registers in hardware.
 */
void
ixl_configure_queue_intr_msix(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	u32		reg;
	u16		vector = 1;

	for (int i = 0; i < vsi->num_queues; i++, vector++) {
		wr32(hw, I40E_PFINT_DYN_CTLN(i), 0);
		/* First queue type is RX / 0 */
		wr32(hw, I40E_PFINT_LNKLSTN(i), i);

		reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK |
		(IXL_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_RQCTL_MSIX_INDX_SHIFT) |
		(i << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_TX << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT);
		wr32(hw, I40E_QINT_RQCTL(i), reg);

		reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK |
		(IXL_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT) |
		(vector << I40E_QINT_TQCTL_MSIX_INDX_SHIFT) |
		(IXL_QUEUE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT) |
		(I40E_QUEUE_TYPE_RX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
		wr32(hw, I40E_QINT_TQCTL(i), reg);
	}
}

/*
 * Configure for MSI single vector operation 
 */
void
ixl_configure_legacy(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	u32		reg;

	wr32(hw, I40E_PFINT_ITR0(0), 0);
	wr32(hw, I40E_PFINT_ITR0(1), 0);

	/* Setup "other" causes */
	reg = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK
	    | I40E_PFINT_ICR0_ENA_GRST_MASK
	    | I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK
	    | I40E_PFINT_ICR0_ENA_GPIO_MASK
	    | I40E_PFINT_ICR0_ENA_LINK_STAT_CHANGE_MASK
	    | I40E_PFINT_ICR0_ENA_HMC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK
	    | I40E_PFINT_ICR0_ENA_VFLR_MASK
	    | I40E_PFINT_ICR0_ENA_ADMINQ_MASK
	    ;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	/* SW_ITR_IDX = 0, but don't change INTENA */
	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTLN_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTLN_INTENA_MSK_MASK);
	/* SW_ITR_IDX = 0, OTHER_ITR_IDX = 0 */
	wr32(hw, I40E_PFINT_STAT_CTL0, 0);

	/* FIRSTQ_INDX = 0, FIRSTQ_TYPE = 0 (rx) */
	wr32(hw, I40E_PFINT_LNKLST0, 0);

	/* Associate the queue pair to the vector and enable the q int */
	reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK
	    | (IXL_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT)
	    | (I40E_QUEUE_TYPE_TX << I40E_QINT_TQCTL_NEXTQ_TYPE_SHIFT);
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK
	    | (IXL_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT)
	    | (IXL_QUEUE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
	wr32(hw, I40E_QINT_TQCTL(0), reg);
}

int
ixl_allocate_pci_resources(struct ixl_pf *pf)
{
	int             rid;
	struct i40e_hw *hw = &pf->hw;
	device_t        dev = pf->dev;

	/* Map BAR0 */
	rid = PCIR_BAR(0);
	pf->pci_mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);

	if (!(pf->pci_mem)) {
		device_printf(dev, "Unable to allocate bus resource: PCI memory\n");
		return (ENXIO);
	}

	/* Save off the PCI information */
	hw->vendor_id = pci_get_vendor(dev);
	hw->device_id = pci_get_device(dev);
	hw->revision_id = pci_read_config(dev, PCIR_REVID, 1);
	hw->subsystem_vendor_id =
	    pci_read_config(dev, PCIR_SUBVEND_0, 2);
	hw->subsystem_device_id =
	    pci_read_config(dev, PCIR_SUBDEV_0, 2);

	hw->bus.device = pci_get_slot(dev);
	hw->bus.func = pci_get_function(dev);

	/* Save off register access information */
	pf->osdep.mem_bus_space_tag =
		rman_get_bustag(pf->pci_mem);
	pf->osdep.mem_bus_space_handle =
		rman_get_bushandle(pf->pci_mem);
	pf->osdep.mem_bus_space_size = rman_get_size(pf->pci_mem);
	pf->osdep.flush_reg = I40E_GLGEN_STAT;
	pf->hw.hw_addr = (u8 *) &pf->osdep.mem_bus_space_handle;

	pf->hw.back = &pf->osdep;

	return (0);
}

/*
 * Teardown and release the admin queue/misc vector
 * interrupt.
 */
int
ixl_teardown_adminq_msix(struct ixl_pf *pf)
{
	device_t		dev = pf->dev;
	int			rid;

	if (pf->admvec) /* we are doing MSIX */
		rid = pf->admvec + 1;
	else
		(pf->msix != 0) ? (rid = 1):(rid = 0);

	if (pf->tag != NULL) {
		bus_teardown_intr(dev, pf->res, pf->tag);
		pf->tag = NULL;
	}
	if (pf->res != NULL) {
		bus_release_resource(dev, SYS_RES_IRQ, rid, pf->res);
		pf->res = NULL;
	}

	return (0);
}

int
ixl_teardown_queue_msix(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = (struct ixl_pf *)vsi->back;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = vsi->dev;
	int			rid, error = 0;

	/* We may get here before stations are setup */
	if ((!pf->enable_msix) || (que == NULL))
		return (0);

	/* Release all MSIX queue resources */
	for (int i = 0; i < vsi->num_queues; i++, que++) {
		rid = que->msix + 1;
		if (que->tag != NULL) {
			error = bus_teardown_intr(dev, que->res, que->tag);
			if (error) {
				device_printf(dev, "bus_teardown_intr() for"
				    " Queue %d interrupt failed\n",
				    que->me);
				// return (ENXIO);
			}
			que->tag = NULL;
		}
		if (que->res != NULL) {
			error = bus_release_resource(dev, SYS_RES_IRQ, rid, que->res);
			if (error) {
				device_printf(dev, "bus_release_resource() for"
				    " Queue %d interrupt failed [rid=%d]\n",
				    que->me, rid);
				// return (ENXIO);
			}
			que->res = NULL;
		}
	}

	return (0);
}

void
ixl_free_pci_resources(struct ixl_pf *pf)
{
	device_t		dev = pf->dev;
	int			memrid;

	ixl_teardown_queue_msix(&pf->vsi);
	ixl_teardown_adminq_msix(pf);

	if (pf->msix)
		pci_release_msi(dev);
	
	memrid = PCIR_BAR(IXL_BAR);

	if (pf->msix_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    memrid, pf->msix_mem);

	if (pf->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    PCIR_BAR(0), pf->pci_mem);

	return;
}

void
ixl_add_ifmedia(struct ixl_vsi *vsi, u32 phy_type)
{
	/* Display supported media types */
	if (phy_type & (1 << I40E_PHY_TYPE_100BASE_TX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_100_TX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_T))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_SX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_LX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_LX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_XAUI) ||
	    phy_type & (1 << I40E_PHY_TYPE_XFI) ||
	    phy_type & (1 << I40E_PHY_TYPE_10GBASE_SFPP_CU))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_SR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SR, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_LR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_LR, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_T))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_T, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_CR4_CU) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_AOC) ||
	    phy_type & (1 << I40E_PHY_TYPE_XLAUI) ||
	    phy_type & (1 << I40E_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_CR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_SR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_LR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_LR4, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_1000BASE_KX))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_1000_KX, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1_CU)
	    || phy_type & (1 << I40E_PHY_TYPE_10GBASE_CR1))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_CR1, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_AOC))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_TWINAX_LONG, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_SFI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_SFI, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KX4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_KX4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_10GBASE_KR))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_10G_KR, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_20GBASE_KR2))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_20G_KR2, 0, NULL);

	if (phy_type & (1 << I40E_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_KR4, 0, NULL);
	if (phy_type & (1 << I40E_PHY_TYPE_XLPPI))
		ifmedia_add(&vsi->media, IFM_ETHER | IFM_40G_XLPPI, 0, NULL);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
int
ixl_setup_interface(device_t dev, struct ixl_vsi *vsi)
{
	struct ifnet		*ifp;
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;
	struct i40e_aq_get_phy_abilities_resp abilities;
	enum i40e_status_code aq_error = 0;

	INIT_DEBUGOUT("ixl_setup_interface: begin");

	ifp = vsi->ifp = if_alloc(IFT_ETHER);
	if (ifp == NULL) {
		device_printf(dev, "can not allocate ifnet structure\n");
		return (-1);
	}
	if_initname(ifp, device_get_name(dev), device_get_unit(dev));
	ifp->if_mtu = ETHERMTU;
	ifp->if_baudrate = IF_Gbps(40);
	ifp->if_init = ixl_init;
	ifp->if_softc = vsi;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixl_ioctl;

#if __FreeBSD_version >= 1100036
	if_setgetcounterfn(ifp, ixl_get_counter);
#endif

	ifp->if_transmit = ixl_mq_start;

	ifp->if_qflush = ixl_qflush;

	ifp->if_snd.ifq_maxlen = que->num_desc - 2;

	vsi->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	/* Set TSO limits */
	ifp->if_hw_tsomax = IP_MAXPACKET - (ETHER_HDR_LEN + ETHER_CRC_LEN);
	ifp->if_hw_tsomaxsegcount = IXL_MAX_TSO_SEGS;
	ifp->if_hw_tsomaxsegsize = PAGE_SIZE;

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_hdrlen = sizeof(struct ether_vlan_header);

	ifp->if_capabilities |= IFCAP_HWCSUM;
	ifp->if_capabilities |= IFCAP_HWCSUM_IPV6;
	ifp->if_capabilities |= IFCAP_TSO;
	ifp->if_capabilities |= IFCAP_JUMBO_MTU;
	ifp->if_capabilities |= IFCAP_LRO;

	/* VLAN capabilties */
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING
			     |  IFCAP_VLAN_HWTSO
			     |  IFCAP_VLAN_MTU
			     |  IFCAP_VLAN_HWCSUM;
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
	ifmedia_init(&vsi->media, IFM_IMASK, ixl_media_change,
		     ixl_media_status);

	aq_error = i40e_aq_get_phy_capabilities(hw,
	    FALSE, TRUE, &abilities, NULL);
	/* May need delay to detect fiber correctly */
	if (aq_error == I40E_ERR_UNKNOWN_PHY) {
		i40e_msec_delay(200);
		aq_error = i40e_aq_get_phy_capabilities(hw, FALSE,
		    TRUE, &abilities, NULL);
	}
	if (aq_error) {
		if (aq_error == I40E_ERR_UNKNOWN_PHY)
			device_printf(dev, "Unknown PHY type detected!\n");
		else
			device_printf(dev,
			    "Error getting supported media types, err %d,"
			    " AQ error %d\n", aq_error, hw->aq.asq_last_status);
		return (0);
	}

	ixl_add_ifmedia(vsi, abilities.phy_type);

	/* Use autoselect media by default */
	ifmedia_add(&vsi->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&vsi->media, IFM_ETHER | IFM_AUTO);

	ether_ifattach(ifp, hw->mac.addr);

	return (0);
}

/*
** Run when the Admin Queue gets a link state change interrupt.
*/
void
ixl_link_event(struct ixl_pf *pf, struct i40e_arq_event_info *e)
{
	struct i40e_hw	*hw = &pf->hw; 
	device_t dev = pf->dev;
	struct i40e_aqc_get_link_status *status =
	    (struct i40e_aqc_get_link_status *)&e->desc.params.raw;

	/* Request link status from adapter */
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);

	/* Print out message if an unqualified module is found */
	if ((status->link_info & I40E_AQ_MEDIA_AVAILABLE) &&
	    (!(status->an_info & I40E_AQ_QUALIFIED_MODULE)) &&
	    (!(status->link_info & I40E_AQ_LINK_UP)))
		device_printf(dev, "Link failed because "
		    "an unqualified module was detected!\n");

	/* Update OS link info */
	ixl_update_link_status(pf);
}

/*********************************************************************
 *
 *  Get Firmware Switch configuration
 *	- this will need to be more robust when more complex
 *	  switch configurations are enabled.
 *
 **********************************************************************/
int
ixl_switch_config(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw; 
	struct ixl_vsi	*vsi = &pf->vsi;
	device_t 	dev = vsi->dev;
	struct i40e_aqc_get_switch_config_resp *sw_config;
	u8	aq_buf[I40E_AQ_LARGE_BUF];
	int	ret;
	u16	next = 0;

	memset(&aq_buf, 0, sizeof(aq_buf));
	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;
	ret = i40e_aq_get_switch_config(hw, sw_config,
	    sizeof(aq_buf), &next, NULL);
	if (ret) {
		device_printf(dev, "aq_get_switch_config() failed, error %d,"
		    " aq_error %d\n", ret, pf->hw.aq.asq_last_status);
		return (ret);
	}
	if (pf->dbg_mask & IXL_DBG_SWITCH_INFO) {
		device_printf(dev,
		    "Switch config: header reported: %d in structure, %d total\n",
		    sw_config->header.num_reported, sw_config->header.num_total);
		for (int i = 0; i < sw_config->header.num_reported; i++) {
			device_printf(dev,
			    "%d: type=%d seid=%d uplink=%d downlink=%d\n", i,
			    sw_config->element[i].element_type,
			    sw_config->element[i].seid,
			    sw_config->element[i].uplink_seid,
			    sw_config->element[i].downlink_seid);
		}
	}
	/* Simplified due to a single VSI */
	vsi->uplink_seid = sw_config->element[0].uplink_seid;
	vsi->downlink_seid = sw_config->element[0].downlink_seid;
	vsi->seid = sw_config->element[0].seid;
	return (ret);
}

/*********************************************************************
 *
 *  Initialize the VSI:  this handles contexts, which means things
 *  			 like the number of descriptors, buffer size,
 *			 plus we init the rings thru this function.
 *
 **********************************************************************/
int
ixl_initialize_vsi(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = vsi->back;
	struct ixl_queue	*que = vsi->queues;
	device_t		dev = vsi->dev;
	struct i40e_hw		*hw = vsi->hw;
	struct i40e_vsi_context	ctxt;
	int 			tc_queues;
	int			err = 0;

	memset(&ctxt, 0, sizeof(ctxt));
	ctxt.seid = vsi->seid;
	if (pf->veb_seid != 0)
		ctxt.uplink_seid = pf->veb_seid;
	ctxt.pf_num = hw->pf_id;
	err = i40e_aq_get_vsi_params(hw, &ctxt, NULL);
	if (err) {
		device_printf(dev, "i40e_aq_get_vsi_params() failed, error %d"
		    " aq_error %d\n", err, hw->aq.asq_last_status);
		return (err);
	}
	ixl_dbg(pf, IXL_DBG_SWITCH_INFO,
	    "get_vsi_params: seid: %d, uplinkseid: %d, vsi_number: %d, "
	    "vsis_allocated: %d, vsis_unallocated: %d, flags: 0x%x, "
	    "pfnum: %d, vfnum: %d, stat idx: %d, enabled: %d\n", ctxt.seid,
	    ctxt.uplink_seid, ctxt.vsi_number,
	    ctxt.vsis_allocated, ctxt.vsis_unallocated,
	    ctxt.flags, ctxt.pf_num, ctxt.vf_num,
	    ctxt.info.stat_counter_idx, ctxt.info.up_enable_bits);
	/*
	** Set the queue and traffic class bits
	**  - when multiple traffic classes are supported
	**    this will need to be more robust.
	*/
	ctxt.info.valid_sections = I40E_AQ_VSI_PROP_QUEUE_MAP_VALID;
	ctxt.info.mapping_flags |= I40E_AQ_VSI_QUE_MAP_CONTIG;
	/* In contig mode, que_mapping[0] is first queue index used by this VSI */
	ctxt.info.queue_mapping[0] = 0;
	/*
	 * This VSI will only use traffic class 0; start traffic class 0's
	 * queue allocation at queue 0, and assign it 2^tc_queues queues (though
	 * the driver may not use all of them).
	 */
	tc_queues = bsrl(pf->qtag.num_allocated);
	ctxt.info.tc_mapping[0] = ((0 << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT)
	    & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) |
	    ((tc_queues << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT)
	    & I40E_AQ_VSI_TC_QUE_NUMBER_MASK);

	/* Set VLAN receive stripping mode */
	ctxt.info.valid_sections |= I40E_AQ_VSI_PROP_VLAN_VALID;
	ctxt.info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL;
	if (vsi->ifp->if_capenable & IFCAP_VLAN_HWTAGGING)
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	else
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

	/* Save VSI number and info for use later */
	vsi->vsi_num = ctxt.vsi_number;
	bcopy(&ctxt.info, &vsi->info, sizeof(vsi->info));

	/* Reset VSI statistics */
	ixl_vsi_reset_stats(vsi);
	vsi->hw_filters_add = 0;
	vsi->hw_filters_del = 0;

	ctxt.flags = htole16(I40E_AQ_VSI_TYPE_PF);

	err = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (err) {
		device_printf(dev, "i40e_aq_update_vsi_params() failed, error %d,"
		    " aq_error %d\n", err, hw->aq.asq_last_status);
		return (err);
	}

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring		*txr = &que->txr;
		struct rx_ring 		*rxr = &que->rxr;
		struct i40e_hmc_obj_txq tctx;
		struct i40e_hmc_obj_rxq rctx;
		u32			txctl;
		u16			size;

		/* Setup the HMC TX Context  */
		size = que->num_desc * sizeof(struct i40e_tx_desc);
		memset(&tctx, 0, sizeof(struct i40e_hmc_obj_txq));
		tctx.new_context = 1;
		tctx.base = (txr->dma.pa/IXL_TX_CTX_BASE_UNITS);
		tctx.qlen = que->num_desc;
		tctx.fc_ena = 0;
		tctx.rdylist = vsi->info.qs_handle[0]; /* index is TC */
		/* Enable HEAD writeback */
		tctx.head_wb_ena = 1;
		tctx.head_wb_addr = txr->dma.pa +
		    (que->num_desc * sizeof(struct i40e_tx_desc));
		tctx.rdylist_act = 0;
		err = i40e_clear_lan_tx_queue_context(hw, i);
		if (err) {
			device_printf(dev, "Unable to clear TX context\n");
			break;
		}
		err = i40e_set_lan_tx_queue_context(hw, i, &tctx);
		if (err) {
			device_printf(dev, "Unable to set TX context\n");
			break;
		}
		/* Associate the ring with this PF */
		txctl = I40E_QTX_CTL_PF_QUEUE;
		txctl |= ((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
		    I40E_QTX_CTL_PF_INDX_MASK);
		wr32(hw, I40E_QTX_CTL(i), txctl);
		ixl_flush(hw);

		/* Do ring (re)init */
		ixl_init_tx_ring(que);

		/* Next setup the HMC RX Context  */
		if (vsi->max_frame_size <= MCLBYTES)
			rxr->mbuf_sz = MCLBYTES;
		else
			rxr->mbuf_sz = MJUMPAGESIZE;

		u16 max_rxmax = rxr->mbuf_sz * hw->func_caps.rx_buf_chain_len;

		/* Set up an RX context for the HMC */
		memset(&rctx, 0, sizeof(struct i40e_hmc_obj_rxq));
		rctx.dbuff = rxr->mbuf_sz >> I40E_RXQ_CTX_DBUFF_SHIFT;
		/* ignore header split for now */
		rctx.hbuff = 0 >> I40E_RXQ_CTX_HBUFF_SHIFT;
		rctx.rxmax = (vsi->max_frame_size < max_rxmax) ?
		    vsi->max_frame_size : max_rxmax;
		rctx.dtype = 0;
		rctx.dsize = 1;	/* do 32byte descriptors */
		rctx.hsplit_0 = 0;  /* no HDR split initially */
		rctx.base = (rxr->dma.pa/IXL_RX_CTX_BASE_UNITS);
		rctx.qlen = que->num_desc;
		rctx.tphrdesc_ena = 1;
		rctx.tphwdesc_ena = 1;
		rctx.tphdata_ena = 0;
		rctx.tphhead_ena = 0;
		rctx.lrxqthresh = 2;
		rctx.crcstrip = 1;
		rctx.l2tsel = 1;
		rctx.showiv = 1;
		rctx.fc_ena = 0;
		rctx.prefena = 1;

		err = i40e_clear_lan_rx_queue_context(hw, i);
		if (err) {
			device_printf(dev,
			    "Unable to clear RX context %d\n", i);
			break;
		}
		err = i40e_set_lan_rx_queue_context(hw, i, &rctx);
		if (err) {
			device_printf(dev, "Unable to set RX context %d\n", i);
			break;
		}
		err = ixl_init_rx_ring(que);
		if (err) {
			device_printf(dev, "Fail in init_rx_ring %d\n", i);
			break;
		}
#ifdef DEV_NETMAP
		/* preserve queue */
		if (vsi->ifp->if_capenable & IFCAP_NETMAP) {
			struct netmap_adapter *na = NA(vsi->ifp);
			struct netmap_kring *kring = &na->rx_rings[i];
			int t = na->num_rx_desc - 1 - nm_kr_rxspace(kring);
			wr32(vsi->hw, I40E_QRX_TAIL(que->me), t);
		} else
#endif /* DEV_NETMAP */
		wr32(vsi->hw, I40E_QRX_TAIL(que->me), que->num_desc - 1);
	}
	return (err);
}


/*********************************************************************
 *
 *  Free all VSI structs.
 *
 **********************************************************************/
void
ixl_free_vsi(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = (struct ixl_pf *)vsi->back;
	struct ixl_queue	*que = vsi->queues;

	/* Free station queues */
	if (!vsi->queues)
		goto free_filters;

	for (int i = 0; i < vsi->num_queues; i++, que++) {
		struct tx_ring *txr = &que->txr;
		struct rx_ring *rxr = &que->rxr;
	
		if (!mtx_initialized(&txr->mtx)) /* uninitialized */
			continue;
		IXL_TX_LOCK(txr);
		ixl_free_que_tx(que);
		if (txr->base)
			i40e_free_dma_mem(&pf->hw, &txr->dma);
		IXL_TX_UNLOCK(txr);
		IXL_TX_LOCK_DESTROY(txr);

		if (!mtx_initialized(&rxr->mtx)) /* uninitialized */
			continue;
		IXL_RX_LOCK(rxr);
		ixl_free_que_rx(que);
		if (rxr->base)
			i40e_free_dma_mem(&pf->hw, &rxr->dma);
		IXL_RX_UNLOCK(rxr);
		IXL_RX_LOCK_DESTROY(rxr);
	}
	free(vsi->queues, M_DEVBUF);

free_filters:
	/* Free VSI filter list */
	ixl_free_mac_filters(vsi);
}

void
ixl_free_mac_filters(struct ixl_vsi *vsi)
{
	struct ixl_mac_filter *f;

	while (!SLIST_EMPTY(&vsi->ftl)) {
		f = SLIST_FIRST(&vsi->ftl);
		SLIST_REMOVE_HEAD(&vsi->ftl, next);
		free(f, M_DEVBUF);
	}
}

/*
 * Fill out fields in queue struct and setup tx/rx memory and structs
 */
static int
ixl_setup_queue(struct ixl_queue *que, struct ixl_pf *pf, int index)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	struct tx_ring *txr = &que->txr;
	struct rx_ring *rxr = &que->rxr;
	int error = 0;
	int rsize, tsize;

	/* ERJ: A lot of references to external objects... */
	que->num_desc = pf->ringsz;
	que->me = index;
	que->vsi = vsi;

	txr->que = que;
	txr->tail = I40E_QTX_TAIL(que->me);

	/* Initialize the TX lock */
	snprintf(txr->mtx_name, sizeof(txr->mtx_name), "%s:tx(%d)",
	    device_get_nameunit(dev), que->me);
	mtx_init(&txr->mtx, txr->mtx_name, NULL, MTX_DEF);
	/* Create the TX descriptor ring */
	tsize = roundup2((que->num_desc *
	    sizeof(struct i40e_tx_desc)) +
	    sizeof(u32), DBA_ALIGN);
	if (i40e_allocate_dma_mem(hw,
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
	txr->br = buf_ring_alloc(DEFAULT_TXBRSZ, M_DEVBUF,
	    M_NOWAIT, &txr->mtx);
	if (txr->br == NULL) {
		device_printf(dev,
		    "Critical Failure setting up TX buf ring\n");
		error = ENOMEM;
		goto fail;
	}

	rsize = roundup2(que->num_desc *
	    sizeof(union i40e_rx_desc), DBA_ALIGN);
	rxr->que = que;
	rxr->tail = I40E_QRX_TAIL(que->me);

	/* Initialize the RX side lock */
	snprintf(rxr->mtx_name, sizeof(rxr->mtx_name), "%s:rx(%d)",
	    device_get_nameunit(dev), que->me);
	mtx_init(&rxr->mtx, rxr->mtx_name, NULL, MTX_DEF);

	if (i40e_allocate_dma_mem(hw,
	    &rxr->dma, i40e_mem_reserved, rsize, 4096)) {
		device_printf(dev,
		    "Unable to allocate RX Descriptor memory\n");
		error = ENOMEM;
		goto fail;
	}
	rxr->base = (union i40e_rx_desc *)rxr->dma.va;
	bzero((void *)rxr->base, rsize);
	/* Allocate receive soft structs for the ring*/
	if (ixl_allocate_rx_data(que)) {
		device_printf(dev,
		    "Critical Failure setting up receive structs\n");
		error = ENOMEM;
		goto fail;
	}

	return (0);
fail:
	if (rxr->base)
		i40e_free_dma_mem(&pf->hw, &rxr->dma);
	if (mtx_initialized(&rxr->mtx))
		mtx_destroy(&rxr->mtx);
	if (txr->br) {
		buf_ring_free(txr->br, M_DEVBUF);
		txr->br = NULL;
	}
	if (txr->base)
		i40e_free_dma_mem(&pf->hw, &txr->dma);
	if (mtx_initialized(&txr->mtx))
		mtx_destroy(&txr->mtx);

	return (error);
}

/*********************************************************************
 *
 *  Allocate memory for the VSI (virtual station interface) and their
 *  associated queues, rings and the descriptors associated with each,
 *  called only once at attach.
 *
 **********************************************************************/
int
ixl_setup_stations(struct ixl_pf *pf)
{
	device_t		dev = pf->dev;
	struct ixl_vsi		*vsi;
	struct ixl_queue	*que;
	int			error = 0;

	vsi = &pf->vsi;
	vsi->back = (void *)pf;
	vsi->hw = &pf->hw;
	vsi->id = 0;
	vsi->num_vlans = 0;
	vsi->back = pf;

	/* Get memory for the station queues */
        if (!(vsi->queues =
            (struct ixl_queue *) malloc(sizeof(struct ixl_queue) *
            vsi->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
                device_printf(dev, "Unable to allocate queue memory\n");
                error = ENOMEM;
                return (error);
        }

	for (int i = 0; i < vsi->num_queues; i++) {
		que = &vsi->queues[i];
		error = ixl_setup_queue(que, pf, i);
		if (error)
			return (error);
	}

	return (0);
}

/*
** Provide a update to the queue RX
** interrupt moderation value.
*/
void
ixl_set_queue_rx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw	*hw = vsi->hw;
	struct rx_ring	*rxr = &que->rxr;
	u16		rx_itr;
	u16		rx_latency = 0;
	int		rx_bytes;

	/* Idle, do nothing */
	if (rxr->bytes == 0)
		return;

	if (pf->dynamic_rx_itr) {
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
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
			    que->me), rxr->itr);
		}
	} else { /* We may have have toggled to non-dynamic */
		if (vsi->rx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->rx_itr_setting = pf->rx_itr;
		/* Update the hardware if needed */
		if (rxr->itr != vsi->rx_itr_setting) {
			rxr->itr = vsi->rx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
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
void
ixl_set_queue_tx_itr(struct ixl_queue *que)
{
	struct ixl_vsi	*vsi = que->vsi;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw	*hw = vsi->hw;
	struct tx_ring	*txr = &que->txr;
	u16		tx_itr;
	u16		tx_latency = 0;
	int		tx_bytes;


	/* Idle, do nothing */
	if (txr->bytes == 0)
		return;

	if (pf->dynamic_tx_itr) {
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
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    que->me), txr->itr);
		}

	} else { /* We may have have toggled to non-dynamic */
		if (vsi->tx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->tx_itr_setting = pf->tx_itr;
		/* Update the hardware if needed */
		if (txr->itr != vsi->tx_itr_setting) {
			txr->itr = vsi->tx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    que->me), txr->itr);
		}
	}
	txr->bytes = 0;
	txr->packets = 0;
	return;
}

void
ixl_add_vsi_sysctls(struct ixl_pf *pf, struct ixl_vsi *vsi,
    struct sysctl_ctx_list *ctx, const char *sysctl_name)
{
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	struct sysctl_oid_list *vsi_list;

	tree = device_get_sysctl_tree(pf->dev);
	child = SYSCTL_CHILDREN(tree);
	vsi->vsi_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, sysctl_name,
				   CTLFLAG_RD, NULL, "VSI Number");
	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);

	ixl_add_sysctls_eth_stats(ctx, vsi_list, &vsi->eth_stats);
}

#ifdef IXL_DEBUG
/**
 * ixl_sysctl_qtx_tail_handler
 * Retrieves I40E_QTX_TAIL value from hardware
 * for a sysctl.
 */
int
ixl_sysctl_qtx_tail_handler(SYSCTL_HANDLER_ARGS)
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
 * ixl_sysctl_qrx_tail_handler
 * Retrieves I40E_QRX_TAIL value from hardware
 * for a sysctl.
 */
int
ixl_sysctl_qrx_tail_handler(SYSCTL_HANDLER_ARGS)
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

/*
 * Used to set the Tx ITR value for all of the PF LAN VSI's queues.
 * Writes to the ITR registers immediately.
 */
static int
ixl_sysctl_pf_tx_itr(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	int error = 0;
	int requested_tx_itr;

	requested_tx_itr = pf->tx_itr;
	error = sysctl_handle_int(oidp, &requested_tx_itr, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (pf->dynamic_tx_itr) {
		device_printf(dev,
		    "Cannot set TX itr value while dynamic TX itr is enabled\n");
		    return (EINVAL);
	}
	if (requested_tx_itr < 0 || requested_tx_itr > IXL_MAX_ITR) {
		device_printf(dev,
		    "Invalid TX itr value; value must be between 0 and %d\n",
		        IXL_MAX_ITR);
		return (EINVAL);
	}

	pf->tx_itr = requested_tx_itr;
	ixl_configure_tx_itr(pf);

	return (error);
}

/*
 * Used to set the Rx ITR value for all of the PF LAN VSI's queues.
 * Writes to the ITR registers immediately.
 */
static int
ixl_sysctl_pf_rx_itr(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	int error = 0;
	int requested_rx_itr;

	requested_rx_itr = pf->rx_itr;
	error = sysctl_handle_int(oidp, &requested_rx_itr, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (pf->dynamic_rx_itr) {
		device_printf(dev,
		    "Cannot set RX itr value while dynamic RX itr is enabled\n");
		    return (EINVAL);
	}
	if (requested_rx_itr < 0 || requested_rx_itr > IXL_MAX_ITR) {
		device_printf(dev,
		    "Invalid RX itr value; value must be between 0 and %d\n",
		        IXL_MAX_ITR);
		return (EINVAL);
	}

	pf->rx_itr = requested_rx_itr;
	ixl_configure_rx_itr(pf);

	return (error);
}

void
ixl_add_hw_stats(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	struct ixl_vsi *vsi = &pf->vsi;
	struct ixl_queue *queues = vsi->queues;
	struct i40e_hw_port_stats *pf_stats = &pf->stats;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);
	struct sysctl_oid_list *vsi_list;

	struct sysctl_oid *queue_node;
	struct sysctl_oid_list *queue_list;

	struct tx_ring *txr;
	struct rx_ring *rxr;
	char queue_namebuf[QUEUE_NAME_LEN];

	/* Driver statistics */
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "watchdog_events",
			CTLFLAG_RD, &pf->watchdog_events,
			"Watchdog timeouts");
	SYSCTL_ADD_ULONG(ctx, child, OID_AUTO, "admin_irq",
			CTLFLAG_RD, &pf->admin_irq,
			"Admin Queue IRQ Handled");

	ixl_add_vsi_sysctls(pf, &pf->vsi, ctx, "pf");
	vsi_list = SYSCTL_CHILDREN(pf->vsi.vsi_node);

	/* Queue statistics */
	for (int q = 0; q < vsi->num_queues; q++) {
		snprintf(queue_namebuf, QUEUE_NAME_LEN, "que%d", q);
		queue_node = SYSCTL_ADD_NODE(ctx, vsi_list,
		    OID_AUTO, queue_namebuf, CTLFLAG_RD, NULL, "Queue #");
		queue_list = SYSCTL_CHILDREN(queue_node);

		txr = &(queues[q].txr);
		rxr = &(queues[q].rxr);

		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "mbuf_defrag_failed",
				CTLFLAG_RD, &(queues[q].mbuf_defrag_failed),
				"m_defrag() failed");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "irqs",
				CTLFLAG_RD, &(queues[q].irqs),
				"irqs on this queue");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tso_tx",
				CTLFLAG_RD, &(queues[q].tso),
				"TSO");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_dmamap_failed",
				CTLFLAG_RD, &(queues[q].tx_dmamap_failed),
				"Driver tx dma failure in xmit");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "no_desc_avail",
				CTLFLAG_RD, &(txr->no_desc),
				"Queue No Descriptor Available");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_packets",
				CTLFLAG_RD, &(txr->total_packets),
				"Queue Packets Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "tx_bytes",
				CTLFLAG_RD, &(txr->tx_bytes),
				"Queue Bytes Transmitted");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_packets",
				CTLFLAG_RD, &(rxr->rx_packets),
				"Queue Packets Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_bytes",
				CTLFLAG_RD, &(rxr->rx_bytes),
				"Queue Bytes Received");
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_desc_err",
				CTLFLAG_RD, &(rxr->desc_errs),
				"Queue Rx Descriptor Errors");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "rx_itr",
				CTLFLAG_RD, &(rxr->itr), 0,
				"Queue Rx ITR Interval");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "tx_itr",
				CTLFLAG_RD, &(txr->itr), 0,
				"Queue Tx ITR Interval");
#ifdef IXL_DEBUG
		SYSCTL_ADD_UQUAD(ctx, queue_list, OID_AUTO, "rx_not_done",
				CTLFLAG_RD, &(rxr->not_done),
				"Queue Rx Descriptors not Done");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "rx_next_refresh",
				CTLFLAG_RD, &(rxr->next_refresh), 0,
				"Queue Rx Descriptors not Done");
		SYSCTL_ADD_UINT(ctx, queue_list, OID_AUTO, "rx_next_check",
				CTLFLAG_RD, &(rxr->next_check), 0,
				"Queue Rx Descriptors not Done");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "qtx_tail", 
				CTLTYPE_UINT | CTLFLAG_RD, &queues[q],
				sizeof(struct ixl_queue),
				ixl_sysctl_qtx_tail_handler, "IU",
				"Queue Transmit Descriptor Tail");
		SYSCTL_ADD_PROC(ctx, queue_list, OID_AUTO, "qrx_tail", 
				CTLTYPE_UINT | CTLFLAG_RD, &queues[q],
				sizeof(struct ixl_queue),
				ixl_sysctl_qrx_tail_handler, "IU",
				"Queue Receive Descriptor Tail");
#endif
	}

	/* MAC stats */
	ixl_add_sysctls_mac_stats(ctx, child, pf_stats);
}

void
ixl_add_sysctls_eth_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_eth_stats *eth_stats)
{
	struct ixl_sysctl_info ctls[] =
	{
		{&eth_stats->rx_bytes, "good_octets_rcvd", "Good Octets Received"},
		{&eth_stats->rx_unicast, "ucast_pkts_rcvd",
			"Unicast Packets Received"},
		{&eth_stats->rx_multicast, "mcast_pkts_rcvd",
			"Multicast Packets Received"},
		{&eth_stats->rx_broadcast, "bcast_pkts_rcvd",
			"Broadcast Packets Received"},
		{&eth_stats->rx_discards, "rx_discards", "Discarded RX packets"},
		{&eth_stats->tx_bytes, "good_octets_txd", "Good Octets Transmitted"},
		{&eth_stats->tx_unicast, "ucast_pkts_txd", "Unicast Packets Transmitted"},
		{&eth_stats->tx_multicast, "mcast_pkts_txd",
			"Multicast Packets Transmitted"},
		{&eth_stats->tx_broadcast, "bcast_pkts_txd",
			"Broadcast Packets Transmitted"},
		// end
		{0,0,0}
	};

	struct ixl_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

void
ixl_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_hw_port_stats *stats)
{
	struct sysctl_oid *stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO, "mac",
				    CTLFLAG_RD, NULL, "Mac Statistics");
	struct sysctl_oid_list *stat_list = SYSCTL_CHILDREN(stat_node);

	struct i40e_eth_stats *eth_stats = &stats->eth;
	ixl_add_sysctls_eth_stats(ctx, stat_list, eth_stats);

	struct ixl_sysctl_info ctls[] = 
	{
		{&stats->crc_errors, "crc_errors", "CRC Errors"},
		{&stats->illegal_bytes, "illegal_bytes", "Illegal Byte Errors"},
		{&stats->mac_local_faults, "local_faults", "MAC Local Faults"},
		{&stats->mac_remote_faults, "remote_faults", "MAC Remote Faults"},
		{&stats->rx_length_errors, "rx_length_errors", "Receive Length Errors"},
		/* Packet Reception Stats */
		{&stats->rx_size_64, "rx_frames_64", "64 byte frames received"},
		{&stats->rx_size_127, "rx_frames_65_127", "65-127 byte frames received"},
		{&stats->rx_size_255, "rx_frames_128_255", "128-255 byte frames received"},
		{&stats->rx_size_511, "rx_frames_256_511", "256-511 byte frames received"},
		{&stats->rx_size_1023, "rx_frames_512_1023", "512-1023 byte frames received"},
		{&stats->rx_size_1522, "rx_frames_1024_1522", "1024-1522 byte frames received"},
		{&stats->rx_size_big, "rx_frames_big", "1523-9522 byte frames received"},
		{&stats->rx_undersize, "rx_undersize", "Undersized packets received"},
		{&stats->rx_fragments, "rx_fragmented", "Fragmented packets received"},
		{&stats->rx_oversize, "rx_oversized", "Oversized packets received"},
		{&stats->rx_jabber, "rx_jabber", "Received Jabber"},
		{&stats->checksum_error, "checksum_errors", "Checksum Errors"},
		/* Packet Transmission Stats */
		{&stats->tx_size_64, "tx_frames_64", "64 byte frames transmitted"},
		{&stats->tx_size_127, "tx_frames_65_127", "65-127 byte frames transmitted"},
		{&stats->tx_size_255, "tx_frames_128_255", "128-255 byte frames transmitted"},
		{&stats->tx_size_511, "tx_frames_256_511", "256-511 byte frames transmitted"},
		{&stats->tx_size_1023, "tx_frames_512_1023", "512-1023 byte frames transmitted"},
		{&stats->tx_size_1522, "tx_frames_1024_1522", "1024-1522 byte frames transmitted"},
		{&stats->tx_size_big, "tx_frames_big", "1523-9522 byte frames transmitted"},
		/* Flow control */
		{&stats->link_xon_tx, "xon_txd", "Link XON transmitted"},
		{&stats->link_xon_rx, "xon_recvd", "Link XON received"},
		{&stats->link_xoff_tx, "xoff_txd", "Link XOFF transmitted"},
		{&stats->link_xoff_rx, "xoff_recvd", "Link XOFF received"},
		/* End */
		{0,0,0}
	};

	struct ixl_sysctl_info *entry = ctls;
	while (entry->stat != 0)
	{
		SYSCTL_ADD_UQUAD(ctx, stat_list, OID_AUTO, entry->name,
				CTLFLAG_RD, entry->stat,
				entry->description);
		entry++;
	}
}

void
ixl_set_rss_key(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t	dev = pf->dev;
	enum i40e_status_code status;
#ifdef RSS
	u32		rss_seed[IXL_RSS_KEY_SIZE_REG];
#else
	u32             rss_seed[IXL_RSS_KEY_SIZE_REG] = {0x41b01687,
			    0x183cfd8c, 0xce880440, 0x580cbc3c,
			    0x35897377, 0x328b25e1, 0x4fa98922,
			    0xb7d90c14, 0xd5bad70d, 0xcd15a2c1,
			    0x0, 0x0, 0x0};
#endif

#ifdef RSS
        /* Fetch the configured RSS key */
        rss_getkey((uint8_t *) &rss_seed);
#endif
	/* Fill out hash function seed */
	if (hw->mac.type == I40E_MAC_X722) {
		struct i40e_aqc_get_set_rss_key_data key_data;
		bcopy(rss_seed, key_data.standard_rss_key, 40);
		status = i40e_aq_set_rss_key(hw, vsi->vsi_num, &key_data);
		if (status)
			device_printf(dev, "i40e_aq_set_rss_key status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
	} else {
		for (int i = 0; i < IXL_RSS_KEY_SIZE_REG; i++)
			i40e_write_rx_ctl(hw, I40E_PFQF_HKEY(i), rss_seed[i]);
	}
}

/*
 * Configure enabled PCTYPES for RSS.
 */
void
ixl_set_rss_pctypes(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	u64		set_hena = 0, hena;

#ifdef RSS
	u32		rss_hash_config;

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
	hena = (u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0)) |
	    ((u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1)) << 32);
	hena |= set_hena;
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (u32)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

}

void
ixl_set_rss_hlut(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	device_t	dev = pf->dev;
	struct ixl_vsi *vsi = &pf->vsi;
	int		i, que_id;
	int		lut_entry_width;
	u32		lut = 0;
	enum i40e_status_code status;

	if (hw->mac.type == I40E_MAC_X722)
		lut_entry_width = 7;
	else
		lut_entry_width = pf->hw.func_caps.rss_table_entry_width;

	/* Populate the LUT with max no. of queues in round robin fashion */
	u8 hlut_buf[512];
	for (i = 0; i < pf->hw.func_caps.rss_table_size; i++) {
#ifdef RSS
		/*
		 * Fetch the RSS bucket id for the given indirection entry.
		 * Cap it at the number of configured buckets (which is
		 * num_queues.)
		 */
		que_id = rss_get_indirection_to_bucket(i);
		que_id = que_id % vsi->num_queues;
#else
		que_id = i % vsi->num_queues;
#endif
		lut = (que_id & ((0x1 << lut_entry_width) - 1));
		hlut_buf[i] = lut;
	}

	if (hw->mac.type == I40E_MAC_X722) {
		status = i40e_aq_set_rss_lut(hw, vsi->vsi_num, TRUE, hlut_buf, sizeof(hlut_buf));
		if (status)
			device_printf(dev, "i40e_aq_set_rss_lut status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
	} else {
		for (i = 0; i < pf->hw.func_caps.rss_table_size >> 2; i++)
			wr32(hw, I40E_PFQF_HLUT(i), ((u32 *)hlut_buf)[i]);
		ixl_flush(hw);
	}
}

/*
** Setup the PF's RSS parameters.
*/
void
ixl_config_rss(struct ixl_pf *pf)
{
	ixl_set_rss_key(pf);
	ixl_set_rss_pctypes(pf);
	ixl_set_rss_hlut(pf);
}

/*
** This routine is run via an vlan config EVENT,
** it enables us to use the HW Filter table since
** we can get the vlan id. This just creates the
** entry in the soft version of the VFTA, init will
** repopulate the real table.
*/
void
ixl_register_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;

	if (ifp->if_softc !=  arg)   /* Not our event */
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXL_PF_LOCK(pf);
	++vsi->num_vlans;
	ixl_add_filter(vsi, hw->mac.addr, vtag);
	IXL_PF_UNLOCK(pf);
}

/*
** This routine is run via an vlan
** unconfig EVENT, remove our entry
** in the soft vfta.
*/
void
ixl_unregister_vlan(void *arg, struct ifnet *ifp, u16 vtag)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct i40e_hw	*hw = vsi->hw;
	struct ixl_pf	*pf = (struct ixl_pf *)vsi->back;

	if (ifp->if_softc !=  arg)
		return;

	if ((vtag == 0) || (vtag > 4095))	/* Invalid */
		return;

	IXL_PF_LOCK(pf);
	--vsi->num_vlans;
	ixl_del_filter(vsi, hw->mac.addr, vtag);
	IXL_PF_UNLOCK(pf);
}

/*
** This routine updates vlan filters, called by init
** it scans the filter table and then updates the hw
** after a soft reset.
*/
void
ixl_setup_vlan_filters(struct ixl_vsi *vsi)
{
	struct ixl_mac_filter	*f;
	int			cnt = 0, flags;

	if (vsi->num_vlans == 0)
		return;
	/*
	** Scan the filter list for vlan entries,
	** mark them for addition and then call
	** for the AQ update.
	*/
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (f->flags & IXL_FILTER_VLAN) {
			f->flags |=
			    (IXL_FILTER_ADD |
			    IXL_FILTER_USED);
			cnt++;
		}
	}
	if (cnt == 0) {
		printf("setup vlan: no filters found!\n");
		return;
	}
	flags = IXL_FILTER_VLAN;
	flags |= (IXL_FILTER_ADD | IXL_FILTER_USED);
	ixl_add_hw_filters(vsi, flags, cnt);
	return;
}

/*
** Initialize filter list and add filters that the hardware
** needs to know about.
**
** Requires VSI's filter list & seid to be set before calling.
*/
void
ixl_init_filters(struct ixl_vsi *vsi)
{
	struct ixl_pf *pf = (struct ixl_pf *)vsi->back;

	/* Add broadcast address */
	ixl_add_filter(vsi, ixl_bcast_addr, IXL_VLAN_ANY);

	/*
	 * Prevent Tx flow control frames from being sent out by
	 * non-firmware transmitters.
	 * This affects every VSI in the PF.
	 */
	if (pf->enable_tx_fc_filter)
		i40e_add_filter_to_drop_tx_flow_control_frames(vsi->hw, vsi->seid);
}

/*
** This routine adds mulicast filters
*/
void
ixl_add_mc_filter(struct ixl_vsi *vsi, u8 *macaddr)
{
	struct ixl_mac_filter *f;

	/* Does one already exist */
	f = ixl_find_filter(vsi, macaddr, IXL_VLAN_ANY);
	if (f != NULL)
		return;

	f = ixl_get_filter(vsi);
	if (f == NULL) {
		printf("WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = IXL_VLAN_ANY;
	f->flags |= (IXL_FILTER_ADD | IXL_FILTER_USED
	    | IXL_FILTER_MC);

	return;
}

void
ixl_reconfigure_filters(struct ixl_vsi *vsi)
{
	ixl_add_hw_filters(vsi, IXL_FILTER_USED, vsi->num_macs);
}

/*
** This routine adds macvlan filters
*/
void
ixl_add_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f, *tmp;
	struct ixl_pf		*pf;
	device_t		dev;

	DEBUGOUT("ixl_add_filter: begin");

	pf = vsi->back;
	dev = pf->dev;

	/* Does one already exist */
	f = ixl_find_filter(vsi, macaddr, vlan);
	if (f != NULL)
		return;
	/*
	** Is this the first vlan being registered, if so we
	** need to remove the ANY filter that indicates we are
	** not in a vlan, and replace that with a 0 filter.
	*/
	if ((vlan != IXL_VLAN_ANY) && (vsi->num_vlans == 1)) {
		tmp = ixl_find_filter(vsi, macaddr, IXL_VLAN_ANY);
		if (tmp != NULL) {
			ixl_del_filter(vsi, macaddr, IXL_VLAN_ANY);
			ixl_add_filter(vsi, macaddr, 0);
		}
	}

	f = ixl_get_filter(vsi);
	if (f == NULL) {
		device_printf(dev, "WARNING: no filter available!!\n");
		return;
	}
	bcopy(macaddr, f->macaddr, ETHER_ADDR_LEN);
	f->vlan = vlan;
	f->flags |= (IXL_FILTER_ADD | IXL_FILTER_USED);
	if (f->vlan != IXL_VLAN_ANY)
		f->flags |= IXL_FILTER_VLAN;
	else
		vsi->num_macs++;

	ixl_add_hw_filters(vsi, f->flags, 1);
	return;
}

void
ixl_del_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter *f;

	f = ixl_find_filter(vsi, macaddr, vlan);
	if (f == NULL)
		return;

	f->flags |= IXL_FILTER_DEL;
	ixl_del_hw_filters(vsi, 1);
	vsi->num_macs--;

	/* Check if this is the last vlan removal */
	if (vlan != IXL_VLAN_ANY && vsi->num_vlans == 0) {
		/* Switch back to a non-vlan filter */
		ixl_del_filter(vsi, macaddr, 0);
		ixl_add_filter(vsi, macaddr, IXL_VLAN_ANY);
	}
	return;
}

/*
** Find the filter with both matching mac addr and vlan id
*/
struct ixl_mac_filter *
ixl_find_filter(struct ixl_vsi *vsi, u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f;
	bool			match = FALSE;

	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (!cmp_etheraddr(f->macaddr, macaddr))
			continue;
		if (f->vlan == vlan) {
			match = TRUE;
			break;
		}
	}	

	if (!match)
		f = NULL;
	return (f);
}

/*
** This routine takes additions to the vsi filter
** table and creates an Admin Queue call to create
** the filters in the hardware.
*/
void
ixl_add_hw_filters(struct ixl_vsi *vsi, int flags, int cnt)
{
	struct i40e_aqc_add_macvlan_element_data *a, *b;
	struct ixl_mac_filter	*f;
	struct ixl_pf		*pf;
	struct i40e_hw		*hw;
	device_t		dev;
	int			err, j = 0;

	pf = vsi->back;
	dev = pf->dev;
	hw = &pf->hw;
	IXL_PF_LOCK_ASSERT(pf);

	a = malloc(sizeof(struct i40e_aqc_add_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (a == NULL) {
		device_printf(dev, "add_hw_filters failed to get memory\n");
		return;
	}

	/*
	** Scan the filter list, each time we find one
	** we add it to the admin queue array and turn off
	** the add bit.
	*/
	SLIST_FOREACH(f, &vsi->ftl, next) {
		if (f->flags == flags) {
			b = &a[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, b->mac_addr, ETHER_ADDR_LEN);
			if (f->vlan == IXL_VLAN_ANY) {
				b->vlan_tag = 0;
				b->flags = I40E_AQC_MACVLAN_ADD_IGNORE_VLAN;
			} else {
				b->vlan_tag = f->vlan;
				b->flags = 0;
			}
			b->flags |= I40E_AQC_MACVLAN_ADD_PERFECT_MATCH;
			f->flags &= ~IXL_FILTER_ADD;
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		err = i40e_aq_add_macvlan(hw, vsi->seid, a, j, NULL);
		if (err) 
			device_printf(dev, "aq_add_macvlan err %d, "
			    "aq_error %d\n", err, hw->aq.asq_last_status);
		else
			vsi->hw_filters_add += j;
	}
	free(a, M_DEVBUF);
	return;
}

/*
** This routine takes removals in the vsi filter
** table and creates an Admin Queue call to delete
** the filters in the hardware.
*/
void
ixl_del_hw_filters(struct ixl_vsi *vsi, int cnt)
{
	struct i40e_aqc_remove_macvlan_element_data *d, *e;
	struct ixl_pf		*pf;
	struct i40e_hw		*hw;
	device_t		dev;
	struct ixl_mac_filter	*f, *f_temp;
	int			err, j = 0;

	DEBUGOUT("ixl_del_hw_filters: begin\n");

	pf = vsi->back;
	hw = &pf->hw;
	dev = pf->dev;

	d = malloc(sizeof(struct i40e_aqc_remove_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		printf("del hw filter failed to get memory\n");
		return;
	}

	SLIST_FOREACH_SAFE(f, &vsi->ftl, next, f_temp) {
		if (f->flags & IXL_FILTER_DEL) {
			e = &d[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, e->mac_addr, ETHER_ADDR_LEN);
			e->vlan_tag = (f->vlan == IXL_VLAN_ANY ? 0 : f->vlan);
			e->flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
			/* delete entry from vsi list */
			SLIST_REMOVE(&vsi->ftl, f, ixl_mac_filter, next);
			free(f, M_DEVBUF);
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		err = i40e_aq_remove_macvlan(hw, vsi->seid, d, j, NULL);
		if (err && hw->aq.asq_last_status != I40E_AQ_RC_ENOENT) {
			int sc = 0;
			for (int i = 0; i < j; i++)
				sc += (!d[i].error_code);
			vsi->hw_filters_del += sc;
			device_printf(dev,
			    "Failed to remove %d/%d filters, aq error %d\n",
			    j - sc, j, hw->aq.asq_last_status);
		} else
			vsi->hw_filters_del += j;
	}
	free(d, M_DEVBUF);

	DEBUGOUT("ixl_del_hw_filters: end\n");
	return;
}

int
ixl_enable_tx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	ixl_dbg(pf, IXL_DBG_EN_DIS,
	    "Enabling PF TX ring %4d / VSI TX ring %4d...\n",
	    pf_qidx, vsi_qidx);

	i40e_pre_tx_queue_cfg(hw, pf_qidx, TRUE);

	reg = rd32(hw, I40E_QTX_ENA(pf_qidx));
	reg |= I40E_QTX_ENA_QENA_REQ_MASK |
	    I40E_QTX_ENA_QENA_STAT_MASK;
	wr32(hw, I40E_QTX_ENA(pf_qidx), reg);
	/* Verify the enable took */
	for (int j = 0; j < 10; j++) {
		reg = rd32(hw, I40E_QTX_ENA(pf_qidx));
		if (reg & I40E_QTX_ENA_QENA_STAT_MASK)
			break;
		i40e_msec_delay(10);
	}
	if ((reg & I40E_QTX_ENA_QENA_STAT_MASK) == 0) {
		device_printf(pf->dev, "TX queue %d still disabled!\n",
		    pf_qidx);
		error = ETIMEDOUT;
	}

	return (error);
}

int
ixl_enable_rx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	ixl_dbg(pf, IXL_DBG_EN_DIS,
	    "Enabling PF RX ring %4d / VSI RX ring %4d...\n",
	    pf_qidx, vsi_qidx);

	reg = rd32(hw, I40E_QRX_ENA(pf_qidx));
	reg |= I40E_QRX_ENA_QENA_REQ_MASK |
	    I40E_QRX_ENA_QENA_STAT_MASK;
	wr32(hw, I40E_QRX_ENA(pf_qidx), reg);
	/* Verify the enable took */
	for (int j = 0; j < 10; j++) {
		reg = rd32(hw, I40E_QRX_ENA(pf_qidx));
		if (reg & I40E_QRX_ENA_QENA_STAT_MASK)
			break;
		i40e_msec_delay(10);
	}
	if ((reg & I40E_QRX_ENA_QENA_STAT_MASK) == 0) {
		device_printf(pf->dev, "RX queue %d still disabled!\n",
		    pf_qidx);
		error = ETIMEDOUT;
	}

	return (error);
}

int
ixl_enable_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	int error = 0;

	error = ixl_enable_tx_ring(pf, qtag, vsi_qidx);
	/* Called function already prints error message */
	if (error)
		return (error);
	error = ixl_enable_rx_ring(pf, qtag, vsi_qidx);
	return (error);
}

/* For PF VSI only */
int
ixl_enable_rings(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = vsi->back;
	int		error = 0;

	for (int i = 0; i < vsi->num_queues; i++) {
		error = ixl_enable_ring(pf, &pf->qtag, i);
		if (error)
			return (error);
	}

	return (error);
}

int
ixl_disable_tx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	i40e_pre_tx_queue_cfg(hw, pf_qidx, FALSE);
	i40e_usec_delay(500);

	reg = rd32(hw, I40E_QTX_ENA(pf_qidx));
	reg &= ~I40E_QTX_ENA_QENA_REQ_MASK;
	wr32(hw, I40E_QTX_ENA(pf_qidx), reg);
	/* Verify the disable took */
	for (int j = 0; j < 10; j++) {
		reg = rd32(hw, I40E_QTX_ENA(pf_qidx));
		if (!(reg & I40E_QTX_ENA_QENA_STAT_MASK))
			break;
		i40e_msec_delay(10);
	}
	if (reg & I40E_QTX_ENA_QENA_STAT_MASK) {
		device_printf(pf->dev, "TX queue %d still enabled!\n",
		    pf_qidx);
		error = ETIMEDOUT;
	}

	return (error);
}

int
ixl_disable_rx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	reg = rd32(hw, I40E_QRX_ENA(pf_qidx));
	reg &= ~I40E_QRX_ENA_QENA_REQ_MASK;
	wr32(hw, I40E_QRX_ENA(pf_qidx), reg);
	/* Verify the disable took */
	for (int j = 0; j < 10; j++) {
		reg = rd32(hw, I40E_QRX_ENA(pf_qidx));
		if (!(reg & I40E_QRX_ENA_QENA_STAT_MASK))
			break;
		i40e_msec_delay(10);
	}
	if (reg & I40E_QRX_ENA_QENA_STAT_MASK) {
		device_printf(pf->dev, "RX queue %d still enabled!\n",
		    pf_qidx);
		error = ETIMEDOUT;
	}

	return (error);
}

int
ixl_disable_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	int error = 0;

	error = ixl_disable_tx_ring(pf, qtag, vsi_qidx);
	/* Called function already prints error message */
	if (error)
		return (error);
	error = ixl_disable_rx_ring(pf, qtag, vsi_qidx);
	return (error);
}

/* For PF VSI only */
int
ixl_disable_rings(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = vsi->back;
	int		error = 0;

	for (int i = 0; i < vsi->num_queues; i++) {
		error = ixl_disable_ring(pf, &pf->qtag, i);
		if (error)
			return (error);
	}

	return (error);
}

/**
 * ixl_handle_mdd_event
 *
 * Called from interrupt handler to identify possibly malicious vfs
 * (But also detects events from the PF, as well)
 **/
void
ixl_handle_mdd_event(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	bool mdd_detected = false;
	bool pf_mdd_detected = false;
	u32 reg;

	/* find what triggered the MDD event */
	reg = rd32(hw, I40E_GL_MDET_TX);
	if (reg & I40E_GL_MDET_TX_VALID_MASK) {
		u8 pf_num = (reg & I40E_GL_MDET_TX_PF_NUM_MASK) >>
				I40E_GL_MDET_TX_PF_NUM_SHIFT;
		u8 event = (reg & I40E_GL_MDET_TX_EVENT_MASK) >>
				I40E_GL_MDET_TX_EVENT_SHIFT;
		u16 queue = (reg & I40E_GL_MDET_TX_QUEUE_MASK) >>
				I40E_GL_MDET_TX_QUEUE_SHIFT;
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on TX queue %d, pf number %d\n",
		    event, queue, pf_num);
		wr32(hw, I40E_GL_MDET_TX, 0xffffffff);
		mdd_detected = true;
	}
	reg = rd32(hw, I40E_GL_MDET_RX);
	if (reg & I40E_GL_MDET_RX_VALID_MASK) {
		u8 pf_num = (reg & I40E_GL_MDET_RX_FUNCTION_MASK) >>
				I40E_GL_MDET_RX_FUNCTION_SHIFT;
		u8 event = (reg & I40E_GL_MDET_RX_EVENT_MASK) >>
				I40E_GL_MDET_RX_EVENT_SHIFT;
		u16 queue = (reg & I40E_GL_MDET_RX_QUEUE_MASK) >>
				I40E_GL_MDET_RX_QUEUE_SHIFT;
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on RX queue %d, pf number %d\n",
		    event, queue, pf_num);
		wr32(hw, I40E_GL_MDET_RX, 0xffffffff);
		mdd_detected = true;
	}

	if (mdd_detected) {
		reg = rd32(hw, I40E_PF_MDET_TX);
		if (reg & I40E_PF_MDET_TX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_TX, 0xFFFF);
			device_printf(dev,
			    "MDD TX event is for this function!");
			pf_mdd_detected = true;
		}
		reg = rd32(hw, I40E_PF_MDET_RX);
		if (reg & I40E_PF_MDET_RX_VALID_MASK) {
			wr32(hw, I40E_PF_MDET_RX, 0xFFFF);
			device_printf(dev,
			    "MDD RX event is for this function!");
			pf_mdd_detected = true;
		}
	}

	/* re-enable mdd interrupt cause */
	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	ixl_flush(hw);
}

void
ixl_enable_intr(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;

	if (pf->enable_msix) {
		for (int i = 0; i < vsi->num_queues; i++, que++)
			ixl_enable_queue(hw, que->me);
	} else
		ixl_enable_legacy(hw);
}

void
ixl_disable_rings_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_queue	*que = vsi->queues;

	for (int i = 0; i < vsi->num_queues; i++, que++)
		ixl_disable_queue(hw, que->me);
}

void
ixl_disable_intr(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw		*hw = vsi->hw;

	if (pf->enable_msix)
		ixl_disable_adminq(hw);
	else
		ixl_disable_legacy(hw);
}

void
ixl_enable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
	ixl_flush(hw);
}

void
ixl_disable_adminq(struct i40e_hw *hw)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
	ixl_flush(hw);
}

void
ixl_enable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = I40E_PFINT_DYN_CTLN_INTENA_MASK |
	    I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);
}

void
ixl_disable_queue(struct i40e_hw *hw, int id)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTLN(id), reg);
}

void
ixl_enable_legacy(struct i40e_hw *hw)
{
	u32		reg;
	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
}

void
ixl_disable_legacy(struct i40e_hw *hw)
{
	u32		reg;

	reg = IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT;
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
}

void
ixl_update_stats_counters(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ixl_vf	*vf;

	struct i40e_hw_port_stats *nsd = &pf->stats;
	struct i40e_hw_port_stats *osd = &pf->stats_offsets;

	/* Update hw stats */
	ixl_stat_update32(hw, I40E_GLPRT_CRCERRS(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->crc_errors, &nsd->crc_errors);
	ixl_stat_update32(hw, I40E_GLPRT_ILLERRC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->illegal_bytes, &nsd->illegal_bytes);
	ixl_stat_update48(hw, I40E_GLPRT_GORCH(hw->port),
			   I40E_GLPRT_GORCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_bytes, &nsd->eth.rx_bytes);
	ixl_stat_update48(hw, I40E_GLPRT_GOTCH(hw->port),
			   I40E_GLPRT_GOTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_bytes, &nsd->eth.tx_bytes);
	ixl_stat_update32(hw, I40E_GLPRT_RDPC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_discards,
			   &nsd->eth.rx_discards);
	ixl_stat_update48(hw, I40E_GLPRT_UPRCH(hw->port),
			   I40E_GLPRT_UPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_unicast,
			   &nsd->eth.rx_unicast);
	ixl_stat_update48(hw, I40E_GLPRT_UPTCH(hw->port),
			   I40E_GLPRT_UPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_unicast,
			   &nsd->eth.tx_unicast);
	ixl_stat_update48(hw, I40E_GLPRT_MPRCH(hw->port),
			   I40E_GLPRT_MPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_multicast,
			   &nsd->eth.rx_multicast);
	ixl_stat_update48(hw, I40E_GLPRT_MPTCH(hw->port),
			   I40E_GLPRT_MPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_multicast,
			   &nsd->eth.tx_multicast);
	ixl_stat_update48(hw, I40E_GLPRT_BPRCH(hw->port),
			   I40E_GLPRT_BPRCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.rx_broadcast,
			   &nsd->eth.rx_broadcast);
	ixl_stat_update48(hw, I40E_GLPRT_BPTCH(hw->port),
			   I40E_GLPRT_BPTCL(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->eth.tx_broadcast,
			   &nsd->eth.tx_broadcast);

	ixl_stat_update32(hw, I40E_GLPRT_TDOLD(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_dropped_link_down,
			   &nsd->tx_dropped_link_down);
	ixl_stat_update32(hw, I40E_GLPRT_MLFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_local_faults,
			   &nsd->mac_local_faults);
	ixl_stat_update32(hw, I40E_GLPRT_MRFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->mac_remote_faults,
			   &nsd->mac_remote_faults);
	ixl_stat_update32(hw, I40E_GLPRT_RLEC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_length_errors,
			   &nsd->rx_length_errors);

	/* Flow control (LFC) stats */
	ixl_stat_update32(hw, I40E_GLPRT_LXONRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_rx, &nsd->link_xon_rx);
	ixl_stat_update32(hw, I40E_GLPRT_LXONTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xon_tx, &nsd->link_xon_tx);
	ixl_stat_update32(hw, I40E_GLPRT_LXOFFRXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_rx, &nsd->link_xoff_rx);
	ixl_stat_update32(hw, I40E_GLPRT_LXOFFTXC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->link_xoff_tx, &nsd->link_xoff_tx);

	/* Packet size stats rx */
	ixl_stat_update48(hw, I40E_GLPRT_PRC64H(hw->port),
			   I40E_GLPRT_PRC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_64, &nsd->rx_size_64);
	ixl_stat_update48(hw, I40E_GLPRT_PRC127H(hw->port),
			   I40E_GLPRT_PRC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_127, &nsd->rx_size_127);
	ixl_stat_update48(hw, I40E_GLPRT_PRC255H(hw->port),
			   I40E_GLPRT_PRC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_255, &nsd->rx_size_255);
	ixl_stat_update48(hw, I40E_GLPRT_PRC511H(hw->port),
			   I40E_GLPRT_PRC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_511, &nsd->rx_size_511);
	ixl_stat_update48(hw, I40E_GLPRT_PRC1023H(hw->port),
			   I40E_GLPRT_PRC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1023, &nsd->rx_size_1023);
	ixl_stat_update48(hw, I40E_GLPRT_PRC1522H(hw->port),
			   I40E_GLPRT_PRC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_1522, &nsd->rx_size_1522);
	ixl_stat_update48(hw, I40E_GLPRT_PRC9522H(hw->port),
			   I40E_GLPRT_PRC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_size_big, &nsd->rx_size_big);

	/* Packet size stats tx */
	ixl_stat_update48(hw, I40E_GLPRT_PTC64H(hw->port),
			   I40E_GLPRT_PTC64L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_64, &nsd->tx_size_64);
	ixl_stat_update48(hw, I40E_GLPRT_PTC127H(hw->port),
			   I40E_GLPRT_PTC127L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_127, &nsd->tx_size_127);
	ixl_stat_update48(hw, I40E_GLPRT_PTC255H(hw->port),
			   I40E_GLPRT_PTC255L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_255, &nsd->tx_size_255);
	ixl_stat_update48(hw, I40E_GLPRT_PTC511H(hw->port),
			   I40E_GLPRT_PTC511L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_511, &nsd->tx_size_511);
	ixl_stat_update48(hw, I40E_GLPRT_PTC1023H(hw->port),
			   I40E_GLPRT_PTC1023L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1023, &nsd->tx_size_1023);
	ixl_stat_update48(hw, I40E_GLPRT_PTC1522H(hw->port),
			   I40E_GLPRT_PTC1522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_1522, &nsd->tx_size_1522);
	ixl_stat_update48(hw, I40E_GLPRT_PTC9522H(hw->port),
			   I40E_GLPRT_PTC9522L(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->tx_size_big, &nsd->tx_size_big);

	ixl_stat_update32(hw, I40E_GLPRT_RUC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_undersize, &nsd->rx_undersize);
	ixl_stat_update32(hw, I40E_GLPRT_RFC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_fragments, &nsd->rx_fragments);
	ixl_stat_update32(hw, I40E_GLPRT_ROC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_oversize, &nsd->rx_oversize);
	ixl_stat_update32(hw, I40E_GLPRT_RJC(hw->port),
			   pf->stat_offsets_loaded,
			   &osd->rx_jabber, &nsd->rx_jabber);
	pf->stat_offsets_loaded = true;
	/* End hw stats */

	/* Update vsi stats */
	ixl_update_vsi_stats(vsi);

	for (int i = 0; i < pf->num_vfs; i++) {
		vf = &pf->vfs[i];
		if (vf->vf_flags & VF_FLAG_ENABLED)
			ixl_update_eth_stats(&pf->vfs[i].vsi);
	}
}

int
ixl_rebuild_hw_structs_after_reset(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t dev = pf->dev;
	bool is_up = false;
	int error = 0;

	is_up = !!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING);

	/* Teardown */
	if (is_up)
		ixl_stop(pf);
	error = i40e_shutdown_lan_hmc(hw);
	if (error)
		device_printf(dev,
		    "Shutdown LAN HMC failed with code %d\n", error);
	ixl_disable_adminq(hw);
	ixl_teardown_adminq_msix(pf);
	error = i40e_shutdown_adminq(hw);
	if (error)
		device_printf(dev,
		    "Shutdown Admin queue failed with code %d\n", error);

	/* Setup */
	error = i40e_init_adminq(hw);
	if (error != 0 && error != I40E_ERR_FIRMWARE_API_VERSION) {
		device_printf(dev, "Unable to initialize Admin Queue, error %d\n",
		    error);
	}
	error = ixl_setup_adminq_msix(pf);
	if (error) {
		device_printf(dev, "ixl_setup_adminq_msix error: %d\n",
		    error);
	}
	ixl_configure_intr0_msix(pf);
	ixl_enable_adminq(hw);
	error = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp, 0, 0);
	if (error) {
		device_printf(dev, "init_lan_hmc failed: %d\n", error);
	}
	error = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (error) {
		device_printf(dev, "configure_lan_hmc failed: %d\n", error);
	}
	if (is_up)
		ixl_init(pf);

	return (0);
}

void
ixl_handle_empr_reset(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int count = 0;
	u32 reg;

	/* Typically finishes within 3-4 seconds */
	while (count++ < 100) {
		reg = rd32(hw, I40E_GLGEN_RSTAT)
		    & I40E_GLGEN_RSTAT_DEVSTATE_MASK;
		if (reg)
			i40e_msec_delay(100);
		else
			break;
	}
	ixl_dbg(pf, IXL_DBG_INFO,
	    "EMPR reset wait count: %d\n", count);

	device_printf(dev, "Rebuilding driver state...\n");
	ixl_rebuild_hw_structs_after_reset(pf);
	device_printf(dev, "Rebuilding driver state done.\n");

	atomic_clear_int(&pf->state, IXL_PF_STATE_EMPR_RESETTING);
}

/*
** Tasklet handler for MSIX Adminq interrupts
**  - do outside interrupt since it might sleep
*/
void
ixl_do_adminq(void *context, int pending)
{
	struct ixl_pf			*pf = context;
	struct i40e_hw			*hw = &pf->hw;
	struct i40e_arq_event_info	event;
	i40e_status			ret;
	device_t			dev = pf->dev;
	u32				loop = 0;
	u16				opcode, result;

	if (pf->state & IXL_PF_STATE_EMPR_RESETTING) {
		/* Flag cleared at end of this function */
		ixl_handle_empr_reset(pf);
		return;
	}

	/* Admin Queue handling */
	event.buf_len = IXL_AQ_BUF_SZ;
	event.msg_buf = malloc(event.buf_len,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!event.msg_buf) {
		device_printf(dev, "%s: Unable to allocate memory for Admin"
		    " Queue event!\n", __func__);
		return;
	}

	IXL_PF_LOCK(pf);
	/* clean and process any events */
	do {
		ret = i40e_clean_arq_element(hw, &event, &result);
		if (ret)
			break;
		opcode = LE16_TO_CPU(event.desc.opcode);
		ixl_dbg(pf, IXL_DBG_AQ,
		    "%s: Admin Queue event: %#06x\n", __func__, opcode);
		switch (opcode) {
		case i40e_aqc_opc_get_link_status:
			ixl_link_event(pf, &event);
			break;
		case i40e_aqc_opc_send_msg_to_pf:
#ifdef PCI_IOV
			ixl_handle_vf_msg(pf, &event);
#endif
			break;
		case i40e_aqc_opc_event_lan_overflow:
		default:
			break;
		}

	} while (result && (loop++ < IXL_ADM_LIMIT));

	free(event.msg_buf, M_DEVBUF);

	/*
	 * If there are still messages to process, reschedule ourselves.
	 * Otherwise, re-enable our interrupt.
	 */
	if (result > 0)
		taskqueue_enqueue(pf->tq, &pf->adminq);
	else
		ixl_enable_adminq(hw);

	IXL_PF_UNLOCK(pf);
}

/**
 * Update VSI-specific ethernet statistics counters.
 **/
void
ixl_update_eth_stats(struct ixl_vsi *vsi)
{
	struct ixl_pf *pf = (struct ixl_pf *)vsi->back;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_eth_stats *es;
	struct i40e_eth_stats *oes;
	struct i40e_hw_port_stats *nsd;
	u16 stat_idx = vsi->info.stat_counter_idx;

	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;
	nsd = &pf->stats;

	/* Gather up the stats that the hw collects */
	ixl_stat_update32(hw, I40E_GLV_TEPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_errors, &es->tx_errors);
	ixl_stat_update32(hw, I40E_GLV_RDPC(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_discards, &es->rx_discards);

	ixl_stat_update48(hw, I40E_GLV_GORCH(stat_idx),
			   I40E_GLV_GORCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_bytes, &es->rx_bytes);
	ixl_stat_update48(hw, I40E_GLV_UPRCH(stat_idx),
			   I40E_GLV_UPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_unicast, &es->rx_unicast);
	ixl_stat_update48(hw, I40E_GLV_MPRCH(stat_idx),
			   I40E_GLV_MPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_multicast, &es->rx_multicast);
	ixl_stat_update48(hw, I40E_GLV_BPRCH(stat_idx),
			   I40E_GLV_BPRCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->rx_broadcast, &es->rx_broadcast);

	ixl_stat_update48(hw, I40E_GLV_GOTCH(stat_idx),
			   I40E_GLV_GOTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_bytes, &es->tx_bytes);
	ixl_stat_update48(hw, I40E_GLV_UPTCH(stat_idx),
			   I40E_GLV_UPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_unicast, &es->tx_unicast);
	ixl_stat_update48(hw, I40E_GLV_MPTCH(stat_idx),
			   I40E_GLV_MPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_multicast, &es->tx_multicast);
	ixl_stat_update48(hw, I40E_GLV_BPTCH(stat_idx),
			   I40E_GLV_BPTCL(stat_idx),
			   vsi->stat_offsets_loaded,
			   &oes->tx_broadcast, &es->tx_broadcast);
	vsi->stat_offsets_loaded = true;
}

void
ixl_update_vsi_stats(struct ixl_vsi *vsi)
{
	struct ixl_pf		*pf;
	struct ifnet		*ifp;
	struct i40e_eth_stats	*es;
	u64			tx_discards;

	struct i40e_hw_port_stats *nsd;

	pf = vsi->back;
	ifp = vsi->ifp;
	es = &vsi->eth_stats;
	nsd = &pf->stats;

	ixl_update_eth_stats(vsi);

	tx_discards = es->tx_discards + nsd->tx_dropped_link_down;
	for (int i = 0; i < vsi->num_queues; i++)
		tx_discards += vsi->queues[i].txr.br->br_drops;

	/* Update ifnet stats */
	IXL_SET_IPACKETS(vsi, es->rx_unicast +
	                   es->rx_multicast +
			   es->rx_broadcast);
	IXL_SET_OPACKETS(vsi, es->tx_unicast +
	                   es->tx_multicast +
			   es->tx_broadcast);
	IXL_SET_IBYTES(vsi, es->rx_bytes);
	IXL_SET_OBYTES(vsi, es->tx_bytes);
	IXL_SET_IMCASTS(vsi, es->rx_multicast);
	IXL_SET_OMCASTS(vsi, es->tx_multicast);

	IXL_SET_IERRORS(vsi, nsd->crc_errors + nsd->illegal_bytes +
	    nsd->rx_undersize + nsd->rx_oversize + nsd->rx_fragments +
	    nsd->rx_jabber);
	IXL_SET_OERRORS(vsi, es->tx_errors);
	IXL_SET_IQDROPS(vsi, es->rx_discards + nsd->eth.rx_discards);
	IXL_SET_OQDROPS(vsi, tx_discards);
	IXL_SET_NOPROTO(vsi, es->rx_unknown_protocol);
	IXL_SET_COLLISIONS(vsi, 0);
}

/**
 * Reset all of the stats for the given pf
 **/
void
ixl_pf_reset_stats(struct ixl_pf *pf)
{
	bzero(&pf->stats, sizeof(struct i40e_hw_port_stats));
	bzero(&pf->stats_offsets, sizeof(struct i40e_hw_port_stats));
	pf->stat_offsets_loaded = false;
}

/**
 * Resets all stats of the given vsi
 **/
void
ixl_vsi_reset_stats(struct ixl_vsi *vsi)
{
	bzero(&vsi->eth_stats, sizeof(struct i40e_eth_stats));
	bzero(&vsi->eth_stats_offsets, sizeof(struct i40e_eth_stats));
	vsi->stat_offsets_loaded = false;
}

/**
 * Read and update a 48 bit stat from the hw
 *
 * Since the device stats are not reset at PFReset, they likely will not
 * be zeroed when the driver starts.  We'll save the first values read
 * and use them as offsets to be subtracted from the raw values in order
 * to report stats that count from zero.
 **/
void
ixl_stat_update48(struct i40e_hw *hw, u32 hireg, u32 loreg,
	bool offset_loaded, u64 *offset, u64 *stat)
{
	u64 new_data;

#if defined(__FreeBSD__) && (__FreeBSD_version >= 1000000) && defined(__amd64__)
	new_data = rd64(hw, loreg);
#else
	/*
	 * Use two rd32's instead of one rd64; FreeBSD versions before
	 * 10 don't support 64-bit bus reads/writes.
	 */
	new_data = rd32(hw, loreg);
	new_data |= ((u64)(rd32(hw, hireg) & 0xFFFF)) << 32;
#endif

	if (!offset_loaded)
		*offset = new_data;
	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (new_data + ((u64)1 << 48)) - *offset;
	*stat &= 0xFFFFFFFFFFFFULL;
}

/**
 * Read and update a 32 bit stat from the hw
 **/
void
ixl_stat_update32(struct i40e_hw *hw, u32 reg,
	bool offset_loaded, u64 *offset, u64 *stat)
{
	u32 new_data;

	new_data = rd32(hw, reg);
	if (!offset_loaded)
		*offset = new_data;
	if (new_data >= *offset)
		*stat = (u32)(new_data - *offset);
	else
		*stat = (u32)((new_data + ((u64)1 << 32)) - *offset);
}

void
ixl_add_device_sysctls(struct ixl_pf *pf)
{
	device_t dev = pf->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	struct sysctl_oid *debug_node;
	struct sysctl_oid_list *debug_list;

	/* Set up sysctls */
	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_set_flowcntl, "I", IXL_SYSCTL_HELP_FC);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "advertise_speed", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_set_advertise, "I", IXL_SYSCTL_HELP_SET_ADVERTISE);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "current_speed", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_current_speed, "A", "Current Port Speed");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_version", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_show_fw, "A", "Firmware version");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "unallocated_queues", CTLTYPE_INT | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_unallocated_queues, "I",
	    "Queues not allocated to a PF or VF");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "tx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_sysctl_pf_tx_itr, "I",
	    "Immediately set TX ITR value for all queues");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "rx_itr", CTLTYPE_INT | CTLFLAG_RW,
	    pf, 0, ixl_sysctl_pf_rx_itr, "I",
	    "Immediately set RX ITR value for all queues");

	SYSCTL_ADD_INT(ctx, ctx_list,
	    OID_AUTO, "dynamic_rx_itr", CTLFLAG_RW,
	    &pf->dynamic_rx_itr, 0, "Enable dynamic RX ITR");

	SYSCTL_ADD_INT(ctx, ctx_list,
	    OID_AUTO, "dynamic_tx_itr", CTLFLAG_RW,
	    &pf->dynamic_tx_itr, 0, "Enable dynamic TX ITR");

	/* Add sysctls meant to print debug information, but don't list them
	 * in "sysctl -a" output. */
	debug_node = SYSCTL_ADD_NODE(ctx, ctx_list,
	    OID_AUTO, "debug", CTLFLAG_RD | CTLFLAG_SKIP, NULL, "Debug Sysctls");
	debug_list = SYSCTL_CHILDREN(debug_node);

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "shared_debug_mask", CTLFLAG_RW,
	    &pf->hw.debug_mask, 0, "Shared code debug message level");

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "core_debug_mask", CTLFLAG_RW,
	    &pf->dbg_mask, 0, "Non-hared code debug message level");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "link_status", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_link_status, "A", IXL_SYSCTL_HELP_LINK_STATUS);

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "phy_abilities", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_phy_abilities, "A", "PHY Abilities");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "filter_list", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_sw_filter_list, "A", "SW Filter List");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "hw_res_alloc", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_hw_res_alloc, "A", "HW Resource Allocation");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "switch_config", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_switch_config, "A", "HW Switch Configuration");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "rss_key", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_hkey, "A", "View RSS key");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "rss_lut", CTLTYPE_STRING | CTLFLAG_RD,
	    pf, 0, ixl_sysctl_hlut, "A", "View RSS lookup table");
#ifdef PCI_IOV
	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "vc_debug_level", CTLFLAG_RW, &pf->vc_debug_lvl,
	    0, "PF/VF Virtual Channel debug level");
#endif
}

/*
 * Primarily for finding out how many queues can be assigned to VFs,
 * at runtime.
 */
static int
ixl_sysctl_unallocated_queues(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int queues;

	IXL_PF_LOCK(pf);
	queues = (int)ixl_pf_qmgr_get_num_free(&pf->qmgr);
	IXL_PF_UNLOCK(pf);

	return sysctl_handle_int(oidp, NULL, queues, req);
}

/*
** Set flow control using sysctl:
** 	0 - off
**	1 - rx pause
**	2 - tx pause
**	3 - full
*/
int
ixl_set_flowcntl(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int requested_fc, error = 0;
	enum i40e_status_code aq_error = 0;
	u8 fc_aq_err = 0;

	/* Get request */
	requested_fc = pf->fc;
	error = sysctl_handle_int(oidp, &requested_fc, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (requested_fc < 0 || requested_fc > 3) {
		device_printf(dev,
		    "Invalid fc mode; valid modes are 0 through 3\n");
		return (EINVAL);
	}

	/* Set fc ability for port */
	hw->fc.requested_mode = requested_fc;
	aq_error = i40e_set_fc(hw, &fc_aq_err, TRUE);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new fc mode %d; fc_err %#x\n",
		    __func__, aq_error, fc_aq_err);
		return (EIO);
	}
	pf->fc = requested_fc;

	/* Get new link state */
	i40e_msec_delay(250);
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);

	return (0);
}

int
ixl_current_speed(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	int error = 0, index = 0;

	char *speeds[] = {
		"Unknown",
		"100M",
		"1G",
		"10G",
		"40G",
		"20G"
	};

	ixl_update_link_status(pf);

	switch (hw->phy.link_info.link_speed) {
	case I40E_LINK_SPEED_100MB:
		index = 1;
		break;
	case I40E_LINK_SPEED_1GB:
		index = 2;
		break;
	case I40E_LINK_SPEED_10GB:
		index = 3;
		break;
	case I40E_LINK_SPEED_40GB:
		index = 4;
		break;
	case I40E_LINK_SPEED_20GB:
		index = 5;
		break;
	case I40E_LINK_SPEED_UNKNOWN:
	default:
		index = 0;
		break;
	}

	error = sysctl_handle_string(oidp, speeds[index],
	    strlen(speeds[index]), req);
	return (error);
}

int
ixl_set_advertised_speeds(struct ixl_pf *pf, int speeds)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct i40e_aq_get_phy_abilities_resp abilities;
	struct i40e_aq_set_phy_config config;
	enum i40e_status_code aq_error = 0;

	/* Get current capability information */
	aq_error = i40e_aq_get_phy_capabilities(hw,
	    FALSE, FALSE, &abilities, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error getting phy capabilities %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EIO);
	}

	/* Prepare new config */
	bzero(&config, sizeof(config));
	config.phy_type = abilities.phy_type;
	config.abilities = abilities.abilities
	    | I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	config.eee_capability = abilities.eee_capability;
	config.eeer = abilities.eeer_val;
	config.low_power_ctrl = abilities.d3_lpan;
	/* Translate into aq cmd link_speed */
	if (speeds & 0x10)
		config.link_speed |= I40E_LINK_SPEED_40GB;
	if (speeds & 0x8)
		config.link_speed |= I40E_LINK_SPEED_20GB;
	if (speeds & 0x4)
		config.link_speed |= I40E_LINK_SPEED_10GB;
	if (speeds & 0x2)
		config.link_speed |= I40E_LINK_SPEED_1GB;
	if (speeds & 0x1)
		config.link_speed |= I40E_LINK_SPEED_100MB;

	/* Do aq command & restart link */
	aq_error = i40e_aq_set_phy_config(hw, &config, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new phy config %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EAGAIN);
	}

	/*
	** This seems a bit heavy handed, but we
	** need to get a reinit on some devices
	*/
	IXL_PF_LOCK(pf);
	ixl_stop_locked(pf);
	ixl_init_locked(pf);
	IXL_PF_UNLOCK(pf);

	return (0);
}

/*
** Control link advertise speed:
**	Flags:
**	 0x1 - advertise 100 Mb
**	 0x2 - advertise 1G
**	 0x4 - advertise 10G
**	 0x8 - advertise 20G
**	0x10 - advertise 40G
**
**	Set to 0 to disable link
*/
int
ixl_set_advertise(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int requested_ls = 0;
	int error = 0;

	/* Read in new mode */
	requested_ls = pf->advertised_speed;
	error = sysctl_handle_int(oidp, &requested_ls, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	/* Check for sane value */
	if (requested_ls > 0x10) {
		device_printf(dev, "Invalid advertised speed; "
		    "valid modes are 0x1 through 0x10\n");
		return (EINVAL);
	}
	/* Then check for validity based on adapter type */
	switch (hw->device_id) {
	case I40E_DEV_ID_1G_BASE_T_X722:
		/* 1G BaseT */
		if (requested_ls & ~(0x2)) {
			device_printf(dev,
				"Only 1G speeds supported on this device.\n");
			return (EINVAL);
		}
		break;
	case I40E_DEV_ID_10G_BASE_T:
	case I40E_DEV_ID_10G_BASE_T4:
		/* 10G BaseT */
		if (requested_ls & ~(0x7)) {
			device_printf(dev,
			    "Only 100M/1G/10G speeds supported on this device.\n");
			return (EINVAL);
		}
		break;
	case I40E_DEV_ID_20G_KR2:
	case I40E_DEV_ID_20G_KR2_A:
		/* 20G */
		if (requested_ls & ~(0xE)) {
			device_printf(dev,
			    "Only 1G/10G/20G speeds supported on this device.\n");
			return (EINVAL);
		}
		break;
	case I40E_DEV_ID_KX_B:
	case I40E_DEV_ID_QSFP_A:
	case I40E_DEV_ID_QSFP_B:
		/* 40G */
		if (requested_ls & ~(0x10)) {
			device_printf(dev,
			    "Only 40G speeds supported on this device.\n");
			return (EINVAL);
		}
		break;
	default:
		/* 10G (1G) */
		if (requested_ls & ~(0x6)) {
			device_printf(dev,
			    "Only 1/10G speeds supported on this device.\n");
			return (EINVAL);
		}
		break;
	}

	/* Exit if no change */
	if (pf->advertised_speed == requested_ls)
		return (0);

	error = ixl_set_advertised_speeds(pf, requested_ls);
	if (error)
		return (error);

	pf->advertised_speed = requested_ls;
	ixl_update_link_status(pf);
	return (0);
}

/*
** Get the width and transaction speed of
** the bus this adapter is plugged into.
*/
void
ixl_get_bus_info(struct i40e_hw *hw, device_t dev)
{
        u16                     link;
        u32                     offset;

	/* Some devices don't use PCIE */
	if (hw->mac.type == I40E_MAC_X722)
		return;

        /* Read PCI Express Capabilities Link Status Register */
        pci_find_cap(dev, PCIY_EXPRESS, &offset);
        link = pci_read_config(dev, offset + PCIER_LINK_STA, 2);

	/* Fill out hw struct with PCIE info */
	i40e_set_pci_config_data(hw, link);

	/* Use info to print out bandwidth messages */
        device_printf(dev,"PCI Express Bus: Speed %s %s\n",
            ((hw->bus.speed == i40e_bus_speed_8000) ? "8.0GT/s":
            (hw->bus.speed == i40e_bus_speed_5000) ? "5.0GT/s":
            (hw->bus.speed == i40e_bus_speed_2500) ? "2.5GT/s":"Unknown"),
            (hw->bus.width == i40e_bus_width_pcie_x8) ? "Width x8" :
            (hw->bus.width == i40e_bus_width_pcie_x4) ? "Width x4" :
            (hw->bus.width == i40e_bus_width_pcie_x1) ? "Width x1" :
            ("Unknown"));

        if ((hw->bus.width <= i40e_bus_width_pcie_x8) &&
            (hw->bus.speed < i40e_bus_speed_8000)) {
                device_printf(dev, "PCI-Express bandwidth available"
                    " for this device may be insufficient for"
                    " optimal performance.\n");
                device_printf(dev, "For optimal performance, a x8 "
                    "PCIE Gen3 slot is required.\n");
        }
}

static int
ixl_sysctl_show_fw(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf	*pf = (struct ixl_pf *)arg1;
	struct i40e_hw	*hw = &pf->hw;
	struct sbuf	*sbuf;

	sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	ixl_nvm_version_str(hw, sbuf);
	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return 0;
}

void
ixl_print_nvm_cmd(device_t dev, struct i40e_nvm_access *nvma)
{
	if ((nvma->command == I40E_NVM_READ) &&
	    ((nvma->config & 0xFF) == 0xF) &&
	    (((nvma->config & 0xF00) >> 8) == 0xF) &&
	    (nvma->offset == 0) &&
	    (nvma->data_size == 1)) {
		// device_printf(dev, "- Get Driver Status Command\n");
	}
	else if (nvma->command == I40E_NVM_READ) {
	
	}
	else {
		switch (nvma->command) {
		case 0xB:
			device_printf(dev, "- command: I40E_NVM_READ\n");
			break;
		case 0xC:
			device_printf(dev, "- command: I40E_NVM_WRITE\n");
			break;
		default:
			device_printf(dev, "- command: unknown 0x%08x\n", nvma->command);
			break;
		}

		device_printf(dev, "- config (ptr)  : 0x%02x\n", nvma->config & 0xFF);
		device_printf(dev, "- config (flags): 0x%01x\n", (nvma->config & 0xF00) >> 8);
		device_printf(dev, "- offset : 0x%08x\n", nvma->offset);
		device_printf(dev, "- data_s : 0x%08x\n", nvma->data_size);
	}
}

int
ixl_handle_nvmupd_cmd(struct ixl_pf *pf, struct ifdrv *ifd)
{
	struct i40e_hw *hw = &pf->hw;
	struct i40e_nvm_access *nvma;
	device_t dev = pf->dev;
	enum i40e_status_code status = 0;
	int perrno;

	DEBUGFUNC("ixl_handle_nvmupd_cmd");

	/* Sanity checks */
	if (ifd->ifd_len < sizeof(struct i40e_nvm_access) ||
	    ifd->ifd_data == NULL) {
		device_printf(dev, "%s: incorrect ifdrv length or data pointer\n",
		    __func__);
		device_printf(dev, "%s: ifdrv length: %lu, sizeof(struct i40e_nvm_access): %lu\n",
		    __func__, ifd->ifd_len, sizeof(struct i40e_nvm_access));
		device_printf(dev, "%s: data pointer: %p\n", __func__,
		    ifd->ifd_data);
		return (EINVAL);
	}

	nvma = (struct i40e_nvm_access *)ifd->ifd_data;

	if (pf->dbg_mask & IXL_DBG_NVMUPD)
		ixl_print_nvm_cmd(dev, nvma);

	if (pf->state & IXL_PF_STATE_EMPR_RESETTING) {
		int count = 0;
		while (count++ < 100) {
			i40e_msec_delay(100);
			if (!(pf->state & IXL_PF_STATE_EMPR_RESETTING))
				break;
		}
	}

	if (!(pf->state & IXL_PF_STATE_EMPR_RESETTING)) {
		IXL_PF_LOCK(pf);
		status = i40e_nvmupd_command(hw, nvma, nvma->data, &perrno);
		IXL_PF_UNLOCK(pf);
	} else {
		perrno = -EBUSY;
	}

	if (status)
		device_printf(dev, "i40e_nvmupd_command status %d, perrno %d\n",
		    status, perrno);

	/*
	 * -EPERM is actually ERESTART, which the kernel interprets as it needing
	 * to run this ioctl again. So use -EACCES for -EPERM instead.
	 */
	if (perrno == -EPERM)
		return (-EACCES);
	else
		return (perrno);
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
void
ixl_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct ixl_pf	*pf = vsi->back;
	struct i40e_hw  *hw = &pf->hw;

	INIT_DEBUGOUT("ixl_media_status: begin");
	IXL_PF_LOCK(pf);

	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);
	ixl_update_link_status(pf);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!pf->link_up) {
		IXL_PF_UNLOCK(pf);
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	/* Hardware always does full-duplex */
	ifmr->ifm_active |= IFM_FDX;

	switch (hw->phy.link_info.phy_type) {
		/* 100 M */
		case I40E_PHY_TYPE_100BASE_TX:
			ifmr->ifm_active |= IFM_100_TX;
			break;
		/* 1 G */
		case I40E_PHY_TYPE_1000BASE_T:
			ifmr->ifm_active |= IFM_1000_T;
			break;
		case I40E_PHY_TYPE_1000BASE_SX:
			ifmr->ifm_active |= IFM_1000_SX;
			break;
		case I40E_PHY_TYPE_1000BASE_LX:
			ifmr->ifm_active |= IFM_1000_LX;
			break;
		case I40E_PHY_TYPE_1000BASE_T_OPTICAL:
			ifmr->ifm_active |= IFM_OTHER;
			break;
		/* 10 G */
		case I40E_PHY_TYPE_10GBASE_SFPP_CU:
			ifmr->ifm_active |= IFM_10G_TWINAX;
			break;
		case I40E_PHY_TYPE_10GBASE_SR:
			ifmr->ifm_active |= IFM_10G_SR;
			break;
		case I40E_PHY_TYPE_10GBASE_LR:
			ifmr->ifm_active |= IFM_10G_LR;
			break;
		case I40E_PHY_TYPE_10GBASE_T:
			ifmr->ifm_active |= IFM_10G_T;
			break;
		case I40E_PHY_TYPE_XAUI:
		case I40E_PHY_TYPE_XFI:
		case I40E_PHY_TYPE_10GBASE_AOC:
			ifmr->ifm_active |= IFM_OTHER;
			break;
		/* 40 G */
		case I40E_PHY_TYPE_40GBASE_CR4:
		case I40E_PHY_TYPE_40GBASE_CR4_CU:
			ifmr->ifm_active |= IFM_40G_CR4;
			break;
		case I40E_PHY_TYPE_40GBASE_SR4:
			ifmr->ifm_active |= IFM_40G_SR4;
			break;
		case I40E_PHY_TYPE_40GBASE_LR4:
			ifmr->ifm_active |= IFM_40G_LR4;
			break;
		case I40E_PHY_TYPE_XLAUI:
			ifmr->ifm_active |= IFM_OTHER;
			break;
		case I40E_PHY_TYPE_1000BASE_KX:
			ifmr->ifm_active |= IFM_1000_KX;
			break;
		case I40E_PHY_TYPE_SGMII:
			ifmr->ifm_active |= IFM_1000_SGMII;
			break;
		/* ERJ: What's the difference between these? */
		case I40E_PHY_TYPE_10GBASE_CR1_CU:
		case I40E_PHY_TYPE_10GBASE_CR1:
			ifmr->ifm_active |= IFM_10G_CR1;
			break;
		case I40E_PHY_TYPE_10GBASE_KX4:
			ifmr->ifm_active |= IFM_10G_KX4;
			break;
		case I40E_PHY_TYPE_10GBASE_KR:
			ifmr->ifm_active |= IFM_10G_KR;
			break;
		case I40E_PHY_TYPE_SFI:
			ifmr->ifm_active |= IFM_10G_SFI;
			break;
		/* Our single 20G media type */
		case I40E_PHY_TYPE_20GBASE_KR2:
			ifmr->ifm_active |= IFM_20G_KR2;
			break;
		case I40E_PHY_TYPE_40GBASE_KR4:
			ifmr->ifm_active |= IFM_40G_KR4;
			break;
		case I40E_PHY_TYPE_XLPPI:
		case I40E_PHY_TYPE_40GBASE_AOC:
			ifmr->ifm_active |= IFM_40G_XLPPI;
			break;
		/* Unknown to driver */
		default:
			ifmr->ifm_active |= IFM_UNKNOWN;
			break;
	}
	/* Report flow control status as well */
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_TX)
		ifmr->ifm_active |= IFM_ETH_TXPAUSE;
	if (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_RX)
		ifmr->ifm_active |= IFM_ETH_RXPAUSE;

	IXL_PF_UNLOCK(pf);
}

void
ixl_init(void *arg)
{
	struct ixl_pf *pf = arg;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t dev = pf->dev;
	int error = 0;

	/*
	 * If the aq is dead here, it probably means something outside of the driver
	 * did something to the adapter, like a PF reset.
	 * So rebuild the driver's state here if that occurs.
	 */
	if (!i40e_check_asq_alive(&pf->hw)) {
		device_printf(dev, "Admin Queue is down; resetting...\n");
		IXL_PF_LOCK(pf);
		ixl_teardown_hw_structs(pf);
		ixl_reset(pf);
		IXL_PF_UNLOCK(pf);
	}

	/*
	 * Set up LAN queue interrupts here.
	 * Kernel interrupt setup functions cannot be called while holding a lock,
	 * so this is done outside of init_locked().
	 */
	if (pf->msix > 1) {
		/* Teardown existing interrupts, if they exist */
		ixl_teardown_queue_msix(vsi);
		ixl_free_queue_tqs(vsi);
		/* Then set them up again */
		error = ixl_setup_queue_msix(vsi);
		if (error)
			device_printf(dev, "ixl_setup_queue_msix() error: %d\n",
			    error);
		error = ixl_setup_queue_tqs(vsi);
		if (error)
			device_printf(dev, "ixl_setup_queue_tqs() error: %d\n",
			    error);
	} else
		// possibly broken
		error = ixl_assign_vsi_legacy(pf);
	if (error) {
		device_printf(pf->dev, "assign_vsi_msix/legacy error: %d\n", error);
		return;
	}

	IXL_PF_LOCK(pf);
	ixl_init_locked(pf);
	IXL_PF_UNLOCK(pf);
}

/*
 * NOTE: Fortville does not support forcing media speeds. Instead,
 * use the set_advertise sysctl to set the speeds Fortville
 * will advertise or be allowed to operate at.
 */
int
ixl_media_change(struct ifnet * ifp)
{
	struct ixl_vsi *vsi = ifp->if_softc;
	struct ifmedia *ifm = &vsi->media;

	INIT_DEBUGOUT("ixl_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	if_printf(ifp, "Use 'advertise_speed' sysctl to change advertised speeds\n");

	return (ENODEV);
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixl_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

int
ixl_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct ixl_vsi	*vsi = ifp->if_softc;
	struct ixl_pf	*pf = vsi->back;
	struct ifreq	*ifr = (struct ifreq *)data;
	struct ifdrv	*ifd = (struct ifdrv *)data;
#if defined(INET) || defined(INET6)
	struct ifaddr *ifa = (struct ifaddr *)data;
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
				ixl_init(pf);
#ifdef INET
			if (!(ifp->if_flags & IFF_NOARP))
				arp_ifinit(ifp, ifa);
#endif
		} else
			error = ether_ioctl(ifp, command, data);
		break;
#endif
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > IXL_MAX_FRAME -
		   ETHER_HDR_LEN - ETHER_CRC_LEN - ETHER_VLAN_ENCAP_LEN) {
			error = EINVAL;
		} else {
			IXL_PF_LOCK(pf);
			ifp->if_mtu = ifr->ifr_mtu;
			vsi->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
			    + ETHER_VLAN_ENCAP_LEN;
			ixl_init_locked(pf);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		IXL_PF_LOCK(pf);
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_drv_flags & IFF_DRV_RUNNING)) {
				if ((ifp->if_flags ^ pf->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					ixl_set_promisc(vsi);
				}
			} else {
				IXL_PF_UNLOCK(pf);
				ixl_init(pf);
				IXL_PF_LOCK(pf);
			}
		} else {
			if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
				IXL_PF_UNLOCK(pf);
				ixl_stop(pf);
				IXL_PF_LOCK(pf);
			}
		}
		pf->if_flags = ifp->if_flags;
		IXL_PF_UNLOCK(pf);
		break;
	case SIOCSDRVSPEC:
	case SIOCGDRVSPEC:
		IOCTL_DEBUGOUT("ioctl: SIOCxDRVSPEC (Get/Set Driver-specific "
		    "Info)\n");

		/* NVM update command */
		if (ifd->ifd_cmd == I40E_NVM_ACCESS)
			error = ixl_handle_nvmupd_cmd(pf, ifd);
		else
			error = EINVAL;
		break;
	case SIOCADDMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCADDMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXL_PF_LOCK(pf);
			ixl_disable_intr(vsi);
			ixl_add_multi(vsi);
			ixl_enable_intr(vsi);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl: SIOCDELMULTI");
		if (ifp->if_drv_flags & IFF_DRV_RUNNING) {
			IXL_PF_LOCK(pf);
			ixl_disable_intr(vsi);
			ixl_del_multi(vsi);
			ixl_enable_intr(vsi);
			IXL_PF_UNLOCK(pf);
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
	case SIOCGIFXMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &vsi->media, command);
		break;
	case SIOCSIFCAP:
	{
		int mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		IOCTL_DEBUGOUT("ioctl: SIOCSIFCAP (Set Capabilities)");

		ixl_cap_txcsum_tso(vsi, ifp, mask);

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
			IXL_PF_LOCK(pf);
			ixl_init_locked(pf);
			IXL_PF_UNLOCK(pf);
		}
		VLAN_CAPABILITIES(ifp);

		break;
	}

	default:
		IOCTL_DEBUGOUT("ioctl: UNKNOWN (0x%X)\n", (int)command);
		error = ether_ioctl(ifp, command, data);
		break;
	}

	return (error);
}

static char *
ixl_phy_type_string(u32 bit_pos)
{
	static char * phy_types_str[32] = {
		"SGMII",
		"1000BASE-KX",
		"10GBASE-KX4",
		"10GBASE-KR",
		"40GBASE-KR4",
		"XAUI",
		"XFI",
		"SFI",
		"XLAUI",
		"XLPPI",
		"40GBASE-CR4",
		"10GBASE-CR1",
		"Reserved (12)",
		"Reserved (13)",
		"Reserved (14)",
		"Reserved (15)",
		"Reserved (16)",
		"100BASE-TX",
		"1000BASE-T",
		"10GBASE-T",
		"10GBASE-SR",
		"10GBASE-LR",
		"10GBASE-SFP+Cu",
		"10GBASE-CR1",
		"40GBASE-CR4",
		"40GBASE-SR4",
		"40GBASE-LR4",
		"1000BASE-SX",
		"1000BASE-LX",
		"1000BASE-T Optical",
		"20GBASE-KR2",
		"Reserved (31)"
	};

	if (bit_pos > 31) return "Invalid";
	return phy_types_str[bit_pos];
}


static int
ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct i40e_link_status link_status;
	enum i40e_status_code status;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	status = i40e_aq_get_link_info(hw, true, &link_status, NULL);
	if (status) {
		device_printf(dev,
		    "%s: i40e_aq_get_link_info() status %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_delete(buf);
		return (EIO);
	}

	sbuf_printf(buf, "\n"
	    "PHY Type : 0x%02x<%s>\n"
	    "Speed    : 0x%02x\n"
	    "Link info: 0x%02x\n"
	    "AN info  : 0x%02x\n"
	    "Ext info : 0x%02x\n"
	    "Max Frame: %d\n"
	    "Pacing   : 0x%02x\n"
	    "CRC En?  : %s\n",
	    link_status.phy_type, ixl_phy_type_string(link_status.phy_type),
	    link_status.link_speed, 
	    link_status.link_info, link_status.an_info,
	    link_status.ext_info, link_status.max_frame_size,
	    link_status.pacing,
	    (link_status.crc_enable) ? "Yes" : "No");

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);
	return (error);
}

static int
ixl_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	enum i40e_status_code status;
	struct i40e_aq_get_phy_abilities_resp abilities;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	status = i40e_aq_get_phy_capabilities(hw,
	    TRUE, FALSE, &abilities, NULL);
	if (status) {
		device_printf(dev,
		    "%s: i40e_aq_get_phy_capabilities() status %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_delete(buf);
		return (EIO);
	}

	sbuf_printf(buf, "\n"
	    "PHY Type : %08x",
	    abilities.phy_type);

	if (abilities.phy_type != 0) {
		sbuf_printf(buf, "<");
		for (int i = 0; i < 32; i++)
			if ((1 << i) & abilities.phy_type)
				sbuf_printf(buf, "%s,", ixl_phy_type_string(i));
		sbuf_printf(buf, ">\n");
	}

	sbuf_printf(buf,
	    "Speed    : %02x\n"
	    "Abilities: %02x\n"
	    "EEE cap  : %04x\n"
	    "EEER reg : %08x\n"
	    "D3 Lpan  : %02x\n"
	    "ID       : %02x %02x %02x %02x\n"
	    "ModType  : %02x %02x %02x",
	    abilities.link_speed, 
	    abilities.abilities, abilities.eee_capability,
	    abilities.eeer_val, abilities.d3_lpan,
	    abilities.phy_id[0], abilities.phy_id[1],
	    abilities.phy_id[2], abilities.phy_id[3],
	    abilities.module_type[0], abilities.module_type[1],
	    abilities.module_type[2]);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);
	return (error);
}

static int
ixl_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct ixl_vsi *vsi = &pf->vsi;
	struct ixl_mac_filter *f;
	char *buf, *buf_i;

	int error = 0;
	int ftl_len = 0;
	int ftl_counter = 0;
	int buf_len = 0;
	int entry_len = 42;

	SLIST_FOREACH(f, &vsi->ftl, next) {
		ftl_len++;
	}

	if (ftl_len < 1) {
		sysctl_handle_string(oidp, "(none)", 6, req);
		return (0);
	}

	buf_len = sizeof(char) * (entry_len + 1) * ftl_len + 2;
	buf = buf_i = malloc(buf_len, M_DEVBUF, M_NOWAIT);

	sprintf(buf_i++, "\n");
	SLIST_FOREACH(f, &vsi->ftl, next) {
		sprintf(buf_i,
		    MAC_FORMAT ", vlan %4d, flags %#06x",
		    MAC_FORMAT_ARGS(f->macaddr), f->vlan, f->flags);
		buf_i += entry_len;
		/* don't print '\n' for last entry */
		if (++ftl_counter != ftl_len) {
			sprintf(buf_i, "\n");
			buf_i++;
		}
	}

	error = sysctl_handle_string(oidp, buf, strlen(buf), req);
	if (error)
		printf("sysctl error: %d\n", error);
	free(buf, M_DEVBUF);
	return error;
}

#define IXL_SW_RES_SIZE 0x14
int
ixl_res_alloc_cmp(const void *a, const void *b)
{
	const struct i40e_aqc_switch_resource_alloc_element_resp *one, *two;
	one = (const struct i40e_aqc_switch_resource_alloc_element_resp *)a;
	two = (const struct i40e_aqc_switch_resource_alloc_element_resp *)b;

	return ((int)one->resource_type - (int)two->resource_type);
}

/*
 * Longest string length: 25 
 */
char *
ixl_switch_res_type_string(u8 type)
{
	char * ixl_switch_res_type_strings[0x14] = {
		"VEB",
		"VSI",
		"Perfect Match MAC address",
		"S-tag",
		"(Reserved)",
		"Multicast hash entry",
		"Unicast hash entry",
		"VLAN",
		"VSI List entry",
		"(Reserved)",
		"VLAN Statistic Pool",
		"Mirror Rule",
		"Queue Set",
		"Inner VLAN Forward filter",
		"(Reserved)",
		"Inner MAC",
		"IP",
		"GRE/VN1 Key",
		"VN2 Key",
		"Tunneling Port"
	};

	if (type < 0x14)
		return ixl_switch_res_type_strings[type];
	else
		return "(Reserved)";
}

static int
ixl_sysctl_hw_res_alloc(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	enum i40e_status_code status;
	int error = 0;

	u8 num_entries;
	struct i40e_aqc_switch_resource_alloc_element_resp resp[IXL_SW_RES_SIZE];

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	bzero(resp, sizeof(resp));
	status = i40e_aq_get_switch_resource_alloc(hw, &num_entries,
				resp,
				IXL_SW_RES_SIZE,
				NULL);
	if (status) {
		device_printf(dev,
		    "%s: get_switch_resource_alloc() error %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_delete(buf);
		return (error);
	}

	/* Sort entries by type for display */
	qsort(resp, num_entries,
	    sizeof(struct i40e_aqc_switch_resource_alloc_element_resp),
	    &ixl_res_alloc_cmp);

	sbuf_cat(buf, "\n");
	sbuf_printf(buf, "# of entries: %d\n", num_entries);
	sbuf_printf(buf,
	    "                     Type | Guaranteed | Total | Used   | Un-allocated\n"
	    "                          | (this)     | (all) | (this) | (all)       \n");
	for (int i = 0; i < num_entries; i++) {
		sbuf_printf(buf,
		    "%25s | %10d   %5d   %6d   %12d",
		    ixl_switch_res_type_string(resp[i].resource_type),
		    resp[i].guaranteed,
		    resp[i].total,
		    resp[i].used,
		    resp[i].total_unalloced);
		if (i < num_entries - 1)
			sbuf_cat(buf, "\n");
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);
	return (error);
}

/*
** Caller must init and delete sbuf; this function will clear and
** finish it for caller.
**
** XXX: Cannot use the SEID for this, since there is no longer a 
** fixed mapping between SEID and element type.
*/
char *
ixl_switch_element_string(struct sbuf *s,
    struct i40e_aqc_switch_config_element_resp *element)
{
	sbuf_clear(s);

	switch (element->element_type) {
	case I40E_AQ_SW_ELEM_TYPE_MAC:
		sbuf_printf(s, "MAC %3d", element->element_info);
		break;
	case I40E_AQ_SW_ELEM_TYPE_PF:
		sbuf_printf(s, "PF  %3d", element->element_info);
		break;
	case I40E_AQ_SW_ELEM_TYPE_VF:
		sbuf_printf(s, "VF  %3d", element->element_info);
		break;
	case I40E_AQ_SW_ELEM_TYPE_EMP:
		sbuf_cat(s, "EMP");
		break;
	case I40E_AQ_SW_ELEM_TYPE_BMC:
		sbuf_cat(s, "BMC");
		break;
	case I40E_AQ_SW_ELEM_TYPE_PV:
		sbuf_cat(s, "PV");
		break;
	case I40E_AQ_SW_ELEM_TYPE_VEB:
		sbuf_cat(s, "VEB");
		break;
	case I40E_AQ_SW_ELEM_TYPE_PA:
		sbuf_cat(s, "PA");
		break;
	case I40E_AQ_SW_ELEM_TYPE_VSI:
		sbuf_printf(s, "VSI %3d", element->element_info);
		break;
	default:
		sbuf_cat(s, "?");
		break;
	}

	sbuf_finish(s);
	return sbuf_data(s);
}

static int
ixl_sysctl_switch_config(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	struct sbuf *nmbuf;
	enum i40e_status_code status;
	int error = 0;
	u16 next = 0;
	u8 aq_buf[I40E_AQ_LARGE_BUF];

	struct i40e_aqc_get_switch_config_resp *sw_config;
	sw_config = (struct i40e_aqc_get_switch_config_resp *)aq_buf;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	status = i40e_aq_get_switch_config(hw, sw_config,
	    sizeof(aq_buf), &next, NULL);
	if (status) {
		device_printf(dev,
		    "%s: aq_get_switch_config() error %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_delete(buf);
		return error;
	}
	if (next)
		device_printf(dev, "%s: TODO: get more config with SEID %d\n",
		    __func__, next);

	nmbuf = sbuf_new_auto();
	if (!nmbuf) {
		device_printf(dev, "Could not allocate sbuf for name output.\n");
		sbuf_delete(buf);
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	/* Assuming <= 255 elements in switch */
	sbuf_printf(buf, "# of reported elements: %d\n", sw_config->header.num_reported);
	sbuf_printf(buf, "total # of elements: %d\n", sw_config->header.num_total);
	/* Exclude:
	** Revision -- all elements are revision 1 for now
	*/
	sbuf_printf(buf,
	    "SEID (  Name  ) |  Uplink  | Downlink | Conn Type\n"
	    "                |          |          | (uplink)\n");
	for (int i = 0; i < sw_config->header.num_reported; i++) {
		// "%4d (%8s) | %8s   %8s   %#8x",
		sbuf_printf(buf, "%4d", sw_config->element[i].seid);
		sbuf_cat(buf, " ");
		sbuf_printf(buf, "(%8s)", ixl_switch_element_string(nmbuf,
		    &sw_config->element[i]));
		sbuf_cat(buf, " | ");
		sbuf_printf(buf, "%8d", sw_config->element[i].uplink_seid);
		sbuf_cat(buf, "   ");
		sbuf_printf(buf, "%8d", sw_config->element[i].downlink_seid);
		sbuf_cat(buf, "   ");
		sbuf_printf(buf, "%#8x", sw_config->element[i].connection_type);
		if (i < sw_config->header.num_reported - 1)
			sbuf_cat(buf, "\n");
	}
	sbuf_delete(nmbuf);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);

	sbuf_delete(buf);

	return (error);
}

static int
ixl_sysctl_hkey(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;
	enum i40e_status_code status;
	u32 reg;

	struct i40e_aqc_get_set_rss_key_data key_data;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	if (hw->mac.type == I40E_MAC_X722) {
		bzero(key_data.standard_rss_key, sizeof(key_data.standard_rss_key));
		status = i40e_aq_get_rss_key(hw, pf->vsi.vsi_num, &key_data);
		if (status)
			device_printf(dev, "i40e_aq_get_rss_key status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_printf(buf, "%40D", (u_char *)key_data.standard_rss_key, "");
	} else {
		for (int i = 0; i < IXL_RSS_KEY_SIZE_REG; i++) {
			reg = i40e_read_rx_ctl(hw, I40E_PFQF_HKEY(i));
			sbuf_printf(buf, "%4D", (u_char *)&reg, "");
		}
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

static int
ixl_sysctl_hlut(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;
	enum i40e_status_code status;
	u8 hlut[512];
	u32 reg;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	if (hw->mac.type == I40E_MAC_X722) {
		bzero(hlut, sizeof(hlut));
		status = i40e_aq_get_rss_lut(hw, pf->vsi.vsi_num, TRUE, hlut, sizeof(hlut));
		if (status)
			device_printf(dev, "i40e_aq_get_rss_lut status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
		sbuf_printf(buf, "%512D", (u_char *)hlut, "");
	} else {
		for (int i = 0; i < hw->func_caps.rss_table_size >> 2; i++) {
			reg = rd32(hw, I40E_PFQF_HLUT(i));
			sbuf_printf(buf, "%4D", (u_char *)&reg, "");
		}
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

