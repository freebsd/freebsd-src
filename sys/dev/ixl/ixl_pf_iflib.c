/******************************************************************************

  Copyright (c) 2013-2020, Intel Corporation
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

void
ixl_configure_tx_itr(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_tx_queue	*que = vsi->tx_queues;

	vsi->tx_itr_setting = pf->tx_itr;

	for (int i = 0; i < vsi->num_tx_queues; i++, que++) {
		struct tx_ring	*txr = &que->txr;

		wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR, i),
		    vsi->tx_itr_setting);
		txr->itr = vsi->tx_itr_setting;
		txr->latency = IXL_AVE_LATENCY;
	}
}

void
ixl_configure_rx_itr(struct ixl_pf *pf)
{
	struct i40e_hw		*hw = &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_rx_queue	*que = vsi->rx_queues;

	vsi->rx_itr_setting = pf->rx_itr;

	for (int i = 0; i < vsi->num_rx_queues; i++, que++) {
		struct rx_ring 	*rxr = &que->rxr;

		wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR, i),
		    vsi->rx_itr_setting);
		rxr->itr = vsi->rx_itr_setting;
		rxr->latency = IXL_AVE_LATENCY;
	}
}

int
ixl_intr(void *arg)
{
	struct ixl_pf		*pf = arg;
	struct i40e_hw		*hw =  &pf->hw;
	struct ixl_vsi		*vsi = &pf->vsi;
	struct ixl_rx_queue	*que = vsi->rx_queues;
        u32			icr0;

	++que->irqs;

	/* Clear PBA at start of ISR if using legacy interrupts */
	if (vsi->shared->isc_intr == IFLIB_INTR_LEGACY)
		wr32(hw, I40E_PFINT_DYN_CTL0,
		    I40E_PFINT_DYN_CTLN_CLEARPBA_MASK |
		    (IXL_ITR_NONE << I40E_PFINT_DYN_CTLN_ITR_INDX_SHIFT));

	icr0 = rd32(hw, I40E_PFINT_ICR0);


#ifdef PCI_IOV
	if (icr0 & I40E_PFINT_ICR0_VFLR_MASK)
		iflib_iov_intr_deferred(vsi->ctx);
#endif

	if (icr0 & I40E_PFINT_ICR0_ADMINQ_MASK)
		iflib_admin_intr_deferred(vsi->ctx);

	ixl_enable_intr0(hw);

	if (icr0 & I40E_PFINT_ICR0_QUEUE_0_MASK)
		return (FILTER_SCHEDULE_THREAD);
	else
		return (FILTER_HANDLED);
}

/*********************************************************************
 *
 *  MSI-X VSI Interrupt Service routine
 *
 **********************************************************************/
int
ixl_msix_que(void *arg)
{
	struct ixl_rx_queue *rx_que = arg;

	++rx_que->irqs;

	ixl_set_queue_rx_itr(rx_que);

	return (FILTER_SCHEDULE_THREAD);
}

/*********************************************************************
 *
 *  MSI-X Admin Queue Interrupt Service routine
 *
 **********************************************************************/
int
ixl_msix_adminq(void *arg)
{
	struct ixl_pf	*pf = arg;
	struct i40e_hw	*hw = &pf->hw;
	device_t	dev = pf->dev;
	u32		reg, mask, rstat_reg;
	bool		do_task = FALSE;

	DDPRINTF(dev, "begin");

	++pf->admin_irq;

	reg = rd32(hw, I40E_PFINT_ICR0);
	/*
	 * For masking off interrupt causes that need to be handled before
	 * they can be re-enabled
	 */
	mask = rd32(hw, I40E_PFINT_ICR0_ENA);

	/* Check on the cause */
	if (reg & I40E_PFINT_ICR0_ADMINQ_MASK) {
		mask &= ~I40E_PFINT_ICR0_ENA_ADMINQ_MASK;
		do_task = TRUE;
	}

	if (reg & I40E_PFINT_ICR0_MAL_DETECT_MASK) {
		mask &= ~I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
		atomic_set_32(&pf->state, IXL_PF_STATE_MDD_PENDING);
		do_task = TRUE;
	}

	if (reg & I40E_PFINT_ICR0_GRST_MASK) {
		const char *reset_type;
		mask &= ~I40E_PFINT_ICR0_ENA_GRST_MASK;
		rstat_reg = rd32(hw, I40E_GLGEN_RSTAT);
		rstat_reg = (rstat_reg & I40E_GLGEN_RSTAT_RESET_TYPE_MASK)
		    >> I40E_GLGEN_RSTAT_RESET_TYPE_SHIFT;
		switch (rstat_reg) {
		/* These others might be handled similarly to an EMPR reset */
		case I40E_RESET_CORER:
			reset_type = "CORER";
			break;
		case I40E_RESET_GLOBR:
			reset_type = "GLOBR";
			break;
		case I40E_RESET_EMPR:
			reset_type = "EMPR";
			break;
		default:
			reset_type = "POR";
			break;
		}
		device_printf(dev, "Reset Requested! (%s)\n", reset_type);
		/* overload admin queue task to check reset progress */
		atomic_set_int(&pf->state, IXL_PF_STATE_RESETTING);
		do_task = TRUE;
	}

	/*
	 * PE / PCI / ECC exceptions are all handled in the same way:
	 * mask out these three causes, then request a PF reset
	 */
	if (reg & I40E_PFINT_ICR0_ECC_ERR_MASK)
		device_printf(dev, "ECC Error detected!\n");
	if (reg & I40E_PFINT_ICR0_PCI_EXCEPTION_MASK)
		device_printf(dev, "PCI Exception detected!\n");
	if (reg & I40E_PFINT_ICR0_PE_CRITERR_MASK)
		device_printf(dev, "Critical Protocol Engine Error detected!\n");
	/* Checks against the conditions above */
	if (reg & IXL_ICR0_CRIT_ERR_MASK) {
		mask &= ~IXL_ICR0_CRIT_ERR_MASK;
		atomic_set_32(&pf->state,
		    IXL_PF_STATE_PF_RESET_REQ | IXL_PF_STATE_PF_CRIT_ERR);
		do_task = TRUE;
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

#ifdef PCI_IOV
	if (reg & I40E_PFINT_ICR0_VFLR_MASK) {
		mask &= ~I40E_PFINT_ICR0_ENA_VFLR_MASK;
		iflib_iov_intr_deferred(pf->vsi.ctx);
	}
#endif

	wr32(hw, I40E_PFINT_ICR0_ENA, mask);
	ixl_enable_intr0(hw);

	if (do_task)
		return (FILTER_SCHEDULE_THREAD);
	else
		return (FILTER_HANDLED);
}

/*
 * Configure queue interrupt cause registers in hardware.
 *
 * Linked list for each vector LNKLSTN(i) -> RQCTL(i) -> TQCTL(i) -> EOL
 */
void
ixl_configure_queue_intr_msix(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	u32		reg;
	u16		vector = 1;

	for (int i = 0; i < max(vsi->num_rx_queues, vsi->num_tx_queues); i++, vector++) {
		/* Make sure interrupt is disabled */
		wr32(hw, I40E_PFINT_DYN_CTLN(i), 0);
		/* Set linked list head to point to corresponding RX queue
		 * e.g. vector 1 (LNKLSTN register 0) points to queue pair 0's RX queue */
		reg = ((i << I40E_PFINT_LNKLSTN_FIRSTQ_INDX_SHIFT)
		        & I40E_PFINT_LNKLSTN_FIRSTQ_INDX_MASK) |
		    ((I40E_QUEUE_TYPE_RX << I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_SHIFT)
		        & I40E_PFINT_LNKLSTN_FIRSTQ_TYPE_MASK);
		wr32(hw, I40E_PFINT_LNKLSTN(i), reg);

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
 * Configure for single interrupt vector operation
 */
void
ixl_configure_legacy(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	u32 reg;

	vsi->rx_queues[0].rxr.itr = vsi->rx_itr_setting;

	/* Setup "other" causes */
	reg = I40E_PFINT_ICR0_ENA_ECC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK
	    | I40E_PFINT_ICR0_ENA_GRST_MASK
	    | I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK
	    | I40E_PFINT_ICR0_ENA_HMC_ERR_MASK
	    | I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK
	    | I40E_PFINT_ICR0_ENA_VFLR_MASK
	    | I40E_PFINT_ICR0_ENA_ADMINQ_MASK
	    ;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	/* No ITR for non-queue interrupts */
	wr32(hw, I40E_PFINT_STAT_CTL0,
	    IXL_ITR_NONE << I40E_PFINT_STAT_CTL0_OTHER_ITR_INDX_SHIFT);

	/* FIRSTQ_INDX = 0, FIRSTQ_TYPE = 0 (rx) */
	wr32(hw, I40E_PFINT_LNKLST0, 0);

	/* Associate the queue pair to the vector and enable the q int */
	reg = I40E_QINT_RQCTL_CAUSE_ENA_MASK
	    | (IXL_RX_ITR << I40E_QINT_RQCTL_ITR_INDX_SHIFT)
	    | (I40E_QUEUE_TYPE_TX << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT);
	wr32(hw, I40E_QINT_RQCTL(0), reg);

	reg = I40E_QINT_TQCTL_CAUSE_ENA_MASK
	    | (IXL_TX_ITR << I40E_QINT_TQCTL_ITR_INDX_SHIFT)
	    | (IXL_QUEUE_EOL << I40E_QINT_TQCTL_NEXTQ_INDX_SHIFT);
	wr32(hw, I40E_QINT_TQCTL(0), reg);
}

void
ixl_free_pci_resources(struct ixl_pf *pf)
{
	struct ixl_vsi		*vsi = &pf->vsi;
	device_t		dev = iflib_get_dev(vsi->ctx);
	struct ixl_rx_queue	*rx_que = vsi->rx_queues;

	/* We may get here before stations are set up */
	if (rx_que == NULL)
		goto early;

	/*
	**  Release all MSI-X VSI resources:
	*/
	iflib_irq_free(vsi->ctx, &vsi->irq);

	for (int i = 0; i < vsi->num_rx_queues; i++, rx_que++)
		iflib_irq_free(vsi->ctx, &rx_que->que_irq);
early:
	if (pf->pci_mem != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(pf->pci_mem), pf->pci_mem);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
int
ixl_setup_interface(device_t dev, struct ixl_pf *pf)
{
	struct ixl_vsi *vsi = &pf->vsi;
	if_ctx_t ctx = vsi->ctx;
	struct i40e_hw *hw = &pf->hw;
	struct ifnet *ifp = iflib_get_ifp(ctx);
	struct i40e_aq_get_phy_abilities_resp abilities;
	enum i40e_status_code aq_error = 0;

	INIT_DBG_DEV(dev, "begin");

	vsi->shared->isc_max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN
	    + ETHER_VLAN_ENCAP_LEN;

	if (IXL_PF_IN_RECOVERY_MODE(pf))
		goto only_auto;

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
	} else {
		pf->supported_speeds = abilities.link_speed;
		if_setbaudrate(ifp, ixl_max_aq_speed_to_value(pf->supported_speeds));

		ixl_add_ifmedia(vsi->media, hw->phy.phy_types);
	}

only_auto:
	/* Use autoselect media by default */
	ifmedia_add(vsi->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(vsi->media, IFM_ETHER | IFM_AUTO);

	return (0);
}

/*
** Run when the Admin Queue gets a link state change interrupt.
*/
void
ixl_link_event(struct ixl_pf *pf, struct i40e_arq_event_info *e)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = iflib_get_dev(pf->vsi.ctx);
	struct i40e_aqc_get_link_status *status =
	    (struct i40e_aqc_get_link_status *)&e->desc.params.raw;

	/* Request link status from adapter */
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);

	/* Print out message if an unqualified module is found */
	if ((status->link_info & I40E_AQ_MEDIA_AVAILABLE) &&
	    (pf->advertised_speed) &&
	    (!(status->an_info & I40E_AQ_QUALIFIED_MODULE)) &&
	    (!(status->link_info & I40E_AQ_LINK_UP)))
		device_printf(dev, "Link failed because "
		    "an unqualified module was detected!\n");

	/* OS link info is updated elsewhere */
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
	struct ixl_pf *pf = vsi->back;
	if_softc_ctx_t		scctx = iflib_get_softc_ctx(vsi->ctx);
	struct ixl_tx_queue	*tx_que = vsi->tx_queues;
	struct ixl_rx_queue	*rx_que = vsi->rx_queues;
	device_t		dev = iflib_get_dev(vsi->ctx);
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
	tc_queues = fls(pf->qtag.num_allocated) - 1;
	ctxt.info.tc_mapping[0] = ((pf->qtag.first_qidx << I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT)
	    & I40E_AQ_VSI_TC_QUE_OFFSET_MASK) |
	    ((tc_queues << I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT)
	    & I40E_AQ_VSI_TC_QUE_NUMBER_MASK);

	/* Set VLAN receive stripping mode */
	ctxt.info.valid_sections |= I40E_AQ_VSI_PROP_VLAN_VALID;
	ctxt.info.port_vlan_flags = I40E_AQ_VSI_PVLAN_MODE_ALL;
	if (if_getcapenable(vsi->ifp) & IFCAP_VLAN_HWTAGGING)
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_STR_BOTH;
	else
		ctxt.info.port_vlan_flags |= I40E_AQ_VSI_PVLAN_EMOD_NOTHING;

#ifdef IXL_IW
	/* Set TCP Enable for iWARP capable VSI */
	if (ixl_enable_iwarp && pf->iw_enabled) {
		ctxt.info.valid_sections |=
		    htole16(I40E_AQ_VSI_PROP_QUEUE_OPT_VALID);
		ctxt.info.queueing_opt_flags |= I40E_AQ_VSI_QUE_OPT_TCP_ENA;
	}
#endif
	/* Save VSI number and info for use later */
	vsi->vsi_num = ctxt.vsi_number;
	bcopy(&ctxt.info, &vsi->info, sizeof(vsi->info));

	ctxt.flags = htole16(I40E_AQ_VSI_TYPE_PF);

	err = i40e_aq_update_vsi_params(hw, &ctxt, NULL);
	if (err) {
		device_printf(dev, "i40e_aq_update_vsi_params() failed, error %d,"
		    " aq_error %d\n", err, hw->aq.asq_last_status);
		return (err);
	}

	for (int i = 0; i < vsi->num_tx_queues; i++, tx_que++) {
		struct tx_ring		*txr = &tx_que->txr;
		struct i40e_hmc_obj_txq tctx;
		u32			txctl;

		/* Setup the HMC TX Context  */
		bzero(&tctx, sizeof(tctx));
		tctx.new_context = 1;
		tctx.base = (txr->tx_paddr/IXL_TX_CTX_BASE_UNITS);
		tctx.qlen = scctx->isc_ntxd[0];
		tctx.fc_ena = 0;	/* Disable FCoE */
		/*
		 * This value needs to pulled from the VSI that this queue
		 * is assigned to. Index into array is traffic class.
		 */
		tctx.rdylist = vsi->info.qs_handle[0];
		/*
		 * Set these to enable Head Writeback
		 * - Address is last entry in TX ring (reserved for HWB index)
		 * Leave these as 0 for Descriptor Writeback
		 */
		if (vsi->enable_head_writeback) {
			tctx.head_wb_ena = 1;
			tctx.head_wb_addr = txr->tx_paddr +
			    (scctx->isc_ntxd[0] * sizeof(struct i40e_tx_desc));
		} else {
			tctx.head_wb_ena = 0;
			tctx.head_wb_addr = 0;
		}
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
		ixl_init_tx_ring(vsi, tx_que);
	}
	for (int i = 0; i < vsi->num_rx_queues; i++, rx_que++) {
		struct rx_ring 		*rxr = &rx_que->rxr;
		struct i40e_hmc_obj_rxq rctx;

		/* Next setup the HMC RX Context  */
		rxr->mbuf_sz = iflib_get_rx_mbuf_sz(vsi->ctx);

		u16 max_rxmax = rxr->mbuf_sz * hw->func_caps.rx_buf_chain_len;

		/* Set up an RX context for the HMC */
		memset(&rctx, 0, sizeof(struct i40e_hmc_obj_rxq));
		rctx.dbuff = rxr->mbuf_sz >> I40E_RXQ_CTX_DBUFF_SHIFT;
		/* ignore header split for now */
		rctx.hbuff = 0 >> I40E_RXQ_CTX_HBUFF_SHIFT;
		rctx.rxmax = (scctx->isc_max_frame_size < max_rxmax) ?
		    scctx->isc_max_frame_size : max_rxmax;
		rctx.dtype = 0;
		rctx.dsize = 1;		/* do 32byte descriptors */
		rctx.hsplit_0 = 0;	/* no header split */
		rctx.base = (rxr->rx_paddr/IXL_RX_CTX_BASE_UNITS);
		rctx.qlen = scctx->isc_nrxd[0];
		rctx.tphrdesc_ena = 1;
		rctx.tphwdesc_ena = 1;
		rctx.tphdata_ena = 0;	/* Header Split related */
		rctx.tphhead_ena = 0;	/* Header Split related */
		rctx.lrxqthresh = 1;	/* Interrupt at <64 desc avail */
		rctx.crcstrip = 1;
		rctx.l2tsel = 1;
		rctx.showiv = 1;	/* Strip inner VLAN header */
		rctx.fc_ena = 0;	/* Disable FCoE */
		rctx.prefena = 1;	/* Prefetch descriptors */

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
		wr32(vsi->hw, I40E_QRX_TAIL(i), 0);
	}
	return (err);
}


/*
** Provide a update to the queue RX
** interrupt moderation value.
*/
void
ixl_set_queue_rx_itr(struct ixl_rx_queue *que)
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
			rxr->itr = min(rx_itr, IXL_MAX_ITR);
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
			    rxr->me), rxr->itr);
		}
	} else { /* We may have have toggled to non-dynamic */
		if (vsi->rx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->rx_itr_setting = pf->rx_itr;
		/* Update the hardware if needed */
		if (rxr->itr != vsi->rx_itr_setting) {
			rxr->itr = vsi->rx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_RX_ITR,
			    rxr->me), rxr->itr);
		}
	}
	rxr->bytes = 0;
	rxr->packets = 0;
}


/*
** Provide a update to the queue TX
** interrupt moderation value.
*/
void
ixl_set_queue_tx_itr(struct ixl_tx_queue *que)
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
			txr->itr = min(tx_itr, IXL_MAX_ITR);
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    txr->me), txr->itr);
		}

	} else { /* We may have have toggled to non-dynamic */
		if (vsi->tx_itr_setting & IXL_ITR_DYNAMIC)
			vsi->tx_itr_setting = pf->tx_itr;
		/* Update the hardware if needed */
		if (txr->itr != vsi->tx_itr_setting) {
			txr->itr = vsi->tx_itr_setting;
			wr32(hw, I40E_PFINT_ITRN(IXL_TX_ITR,
			    txr->me), txr->itr);
		}
	}
	txr->bytes = 0;
	txr->packets = 0;
	return;
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
	struct ixl_tx_queue *tx_que;
	int error;
	u32 val;

	tx_que = ((struct ixl_tx_queue *)oidp->oid_arg1);
	if (!tx_que) return 0;

	val = rd32(tx_que->vsi->hw, tx_que->txr.tail);
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
	struct ixl_rx_queue *rx_que;
	int error;
	u32 val;

	rx_que = ((struct ixl_rx_queue *)oidp->oid_arg1);
	if (!rx_que) return 0;

	val = rd32(rx_que->vsi->hw, rx_que->rxr.tail);
	error = sysctl_handle_int(oidp, &val, 0, req);
	if (error || !req->newptr)
		return error;
	return (0);
}
#endif

void
ixl_add_hw_stats(struct ixl_pf *pf)
{
	struct ixl_vsi *vsi = &pf->vsi;
	device_t dev = iflib_get_dev(vsi->ctx);
	struct i40e_hw_port_stats *pf_stats = &pf->stats;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(dev);
	struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

	/* Driver statistics */
	SYSCTL_ADD_UQUAD(ctx, child, OID_AUTO, "admin_irq",
			CTLFLAG_RD, &pf->admin_irq,
			"Admin Queue IRQs received");

	sysctl_ctx_init(&vsi->sysctl_ctx);
	ixl_vsi_add_sysctls(vsi, "pf", true);

	ixl_add_sysctls_mac_stats(ctx, child, pf_stats);
}

void
ixl_set_rss_hlut(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t	dev = iflib_get_dev(vsi->ctx);
	int		i, que_id;
	int		lut_entry_width;
	u32		lut = 0;
	enum i40e_status_code status;

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
		que_id = que_id % vsi->num_rx_queues;
#else
		que_id = i % vsi->num_rx_queues;
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

/* For PF VSI only */
int
ixl_enable_rings(struct ixl_vsi *vsi)
{
	struct ixl_pf	*pf = vsi->back;
	int		error = 0;

	for (int i = 0; i < vsi->num_tx_queues; i++)
		error = ixl_enable_tx_ring(pf, &pf->qtag, i);

	for (int i = 0; i < vsi->num_rx_queues; i++)
		error = ixl_enable_rx_ring(pf, &pf->qtag, i);

	return (error);
}

int
ixl_disable_rings(struct ixl_pf *pf, struct ixl_vsi *vsi, struct ixl_pf_qtag *qtag)
{
	int error = 0;

	for (int i = 0; i < vsi->num_tx_queues; i++)
		error = ixl_disable_tx_ring(pf, qtag, i);

	for (int i = 0; i < vsi->num_rx_queues; i++)
		error = ixl_disable_rx_ring(pf, qtag, i);

	return (error);
}

void
ixl_enable_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_rx_queue	*que = vsi->rx_queues;

	if (vsi->shared->isc_intr == IFLIB_INTR_MSIX) {
		for (int i = 0; i < vsi->num_rx_queues; i++, que++)
			ixl_enable_queue(hw, que->rxr.me);
	} else
		ixl_enable_intr0(hw);
}

void
ixl_disable_rings_intr(struct ixl_vsi *vsi)
{
	struct i40e_hw		*hw = vsi->hw;
	struct ixl_rx_queue	*que = vsi->rx_queues;

	for (int i = 0; i < vsi->num_rx_queues; i++, que++)
		ixl_disable_queue(hw, que->rxr.me);
}

int
ixl_prepare_for_reset(struct ixl_pf *pf, bool is_up)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int error = 0;

	if (is_up)
		ixl_if_stop(pf->vsi.ctx);

	ixl_shutdown_hmc(pf);

	ixl_disable_intr0(hw);

	error = i40e_shutdown_adminq(hw);
	if (error)
		device_printf(dev,
		    "Shutdown Admin queue failed with code %d\n", error);

	ixl_pf_qmgr_release(&pf->qmgr, &pf->qtag);
	return (error);
}

int
ixl_rebuild_hw_structs_after_reset(struct ixl_pf *pf, bool is_up)
{
	struct i40e_hw *hw = &pf->hw;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t dev = pf->dev;
	enum i40e_get_fw_lldp_status_resp lldp_status;
	int error = 0;

	device_printf(dev, "Rebuilding driver state...\n");

	/* Setup */
	error = i40e_init_adminq(hw);
	if (error != 0 && error != I40E_ERR_FIRMWARE_API_VERSION) {
		device_printf(dev, "Unable to initialize Admin Queue, error %d\n",
		    error);
		goto ixl_rebuild_hw_structs_after_reset_err;
	}

	if (IXL_PF_IN_RECOVERY_MODE(pf)) {
		/* Keep admin queue interrupts active while driver is loaded */
		if (vsi->shared->isc_intr == IFLIB_INTR_MSIX) {
			ixl_configure_intr0_msix(pf);
			ixl_enable_intr0(hw);
		}

		return (0);
	}

	i40e_clear_pxe_mode(hw);

	error = ixl_get_hw_capabilities(pf);
	if (error) {
		device_printf(dev, "ixl_get_hw_capabilities failed: %d\n", error);
		goto ixl_rebuild_hw_structs_after_reset_err;
	}

	error = ixl_setup_hmc(pf);
	if (error)
		goto ixl_rebuild_hw_structs_after_reset_err;

	/* reserve a contiguous allocation for the PF's VSI */
	error = ixl_pf_qmgr_alloc_contiguous(&pf->qmgr, vsi->num_tx_queues, &pf->qtag);
	if (error) {
		device_printf(dev, "Failed to reserve queues for PF LAN VSI, error %d\n",
		    error);
	}

	error = ixl_switch_config(pf);
	if (error) {
		device_printf(dev, "ixl_rebuild_hw_structs_after_reset: ixl_switch_config() failed: %d\n",
		     error);
		error = EIO;
		goto ixl_rebuild_hw_structs_after_reset_err;
	}

	error = i40e_aq_set_phy_int_mask(hw, IXL_DEFAULT_PHY_INT_MASK,
	    NULL);
	if (error) {
		device_printf(dev, "init: i40e_aq_set_phy_mask() failed: err %d,"
		    " aq_err %d\n", error, hw->aq.asq_last_status);
		error = EIO;
		goto ixl_rebuild_hw_structs_after_reset_err;
	}

	u8 set_fc_err_mask;
	error = i40e_set_fc(hw, &set_fc_err_mask, true);
	if (error) {
		device_printf(dev, "init: setting link flow control failed; retcode %d,"
		    " fc_err_mask 0x%02x\n", error, set_fc_err_mask);
		error = EIO;
		goto ixl_rebuild_hw_structs_after_reset_err;
	}

	/* Remove default filters reinstalled by FW on reset */
	ixl_del_default_hw_filters(vsi);

	/* Receive broadcast Ethernet frames */
	i40e_aq_set_vsi_broadcast(&pf->hw, vsi->seid, TRUE, NULL);

	/* Determine link state */
	if (ixl_attach_get_link_status(pf)) {
		error = EINVAL;
	}

	i40e_aq_set_dcb_parameters(hw, TRUE, NULL);

	/* Query device FW LLDP status */
	if (i40e_get_fw_lldp_status(hw, &lldp_status) == I40E_SUCCESS) {
		if (lldp_status == I40E_GET_FW_LLDP_STATUS_DISABLED) {
			atomic_set_32(&pf->state,
			    IXL_PF_STATE_FW_LLDP_DISABLED);
		} else {
			atomic_clear_32(&pf->state,
			    IXL_PF_STATE_FW_LLDP_DISABLED);
		}
	}

	/* Keep admin queue interrupts active while driver is loaded */
	if (vsi->shared->isc_intr == IFLIB_INTR_MSIX) {
		ixl_configure_intr0_msix(pf);
		ixl_enable_intr0(hw);
	}

	if (is_up) {
		iflib_request_reset(vsi->ctx);
		iflib_admin_intr_deferred(vsi->ctx);
	}

	device_printf(dev, "Rebuilding driver state done.\n");
	return (0);

ixl_rebuild_hw_structs_after_reset_err:
	device_printf(dev, "Reload the driver to recover\n");
	return (error);
}

/*
** Set flow control using sysctl:
** 	0 - off
**	1 - rx pause
**	2 - tx pause
**	3 - full
*/
int
ixl_sysctl_set_flowcntl(SYSCTL_HANDLER_ARGS)
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

	return (0);
}
