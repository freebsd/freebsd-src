/******************************************************************************

  Copyright (c) 2013-2018, Intel Corporation
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

#ifdef IXL_IW
#include "ixl_iw.h"
#include "ixl_iw_int.h"
#endif

static u8	ixl_convert_sysctl_aq_link_speed(u8, bool);
static void	ixl_sbuf_print_bytes(struct sbuf *, u8 *, int, int, bool);
static const char * ixl_link_speed_string(enum i40e_aq_link_speed);
static u_int	ixl_add_maddr(void *, struct sockaddr_dl *, u_int);
static u_int	ixl_match_maddr(void *, struct sockaddr_dl *, u_int);
static char *	ixl_switch_element_string(struct sbuf *, u8, u16);
static enum ixl_fw_mode ixl_get_fw_mode(struct ixl_pf *);

/* Sysctls */
static int	ixl_sysctl_set_advertise(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_supported_speeds(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_current_speed(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_show_fw(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_unallocated_queues(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_pf_tx_itr(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_pf_rx_itr(SYSCTL_HANDLER_ARGS);

static int	ixl_sysctl_eee_enable(SYSCTL_HANDLER_ARGS);

/* Debug Sysctls */
static int 	ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_phy_abilities(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_sw_filter_list(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hw_res_alloc(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_switch_config(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hkey(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hena(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_hlut(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fw_link_management(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_read_i2c_byte(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_write_i2c_byte(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fec_fc_ability(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fec_rs_ability(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fec_fc_request(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fec_rs_request(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fec_auto_enable(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_dump_debug_data(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_fw_lldp(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_read_i2c_diag_data(SYSCTL_HANDLER_ARGS);

/* Debug Sysctls */
static int	ixl_sysctl_do_pf_reset(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_do_core_reset(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_do_global_reset(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_queue_interrupt_table(SYSCTL_HANDLER_ARGS);
#ifdef IXL_DEBUG
static int	ixl_sysctl_qtx_tail_handler(SYSCTL_HANDLER_ARGS);
static int	ixl_sysctl_qrx_tail_handler(SYSCTL_HANDLER_ARGS);
#endif

#ifdef IXL_IW
extern int ixl_enable_iwarp;
extern int ixl_limit_iwarp_msix;
#endif

static const char * const ixl_fc_string[6] = {
	"None",
	"Rx",
	"Tx",
	"Full",
	"Priority",
	"Default"
};

static char *ixl_fec_string[3] = {
       "CL108 RS-FEC",
       "CL74 FC-FEC/BASE-R",
       "None"
};

MALLOC_DEFINE(M_IXL, "ixl", "ixl driver allocations");

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

/**
 * ixl_get_fw_mode - Check the state of FW
 * @hw: device hardware structure
 *
 * Identify state of FW. It might be in a recovery mode
 * which limits functionality and requires special handling
 * from the driver.
 *
 * @returns FW mode (normal, recovery, unexpected EMP reset)
 */
static enum ixl_fw_mode
ixl_get_fw_mode(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	enum ixl_fw_mode fw_mode = IXL_FW_MODE_NORMAL;
	u32 fwsts;

#ifdef IXL_DEBUG
	if (pf->recovery_mode)
		return IXL_FW_MODE_RECOVERY;
#endif
	fwsts = rd32(hw, I40E_GL_FWSTS) & I40E_GL_FWSTS_FWS1B_MASK;

	/* Is set and has one of expected values */
	if ((fwsts >= I40E_XL710_GL_FWSTS_FWS1B_REC_MOD_CORER_MASK &&
	    fwsts <= I40E_XL710_GL_FWSTS_FWS1B_REC_MOD_NVM_MASK) ||
	    fwsts == I40E_X722_GL_FWSTS_FWS1B_REC_MOD_GLOBR_MASK ||
	    fwsts == I40E_X722_GL_FWSTS_FWS1B_REC_MOD_CORER_MASK)
		fw_mode = IXL_FW_MODE_RECOVERY;
	else {
		if (fwsts > I40E_GL_FWSTS_FWS1B_EMPR_0 &&
		    fwsts <= I40E_GL_FWSTS_FWS1B_EMPR_10)
			fw_mode = IXL_FW_MODE_UEMPR;
	}
	return (fw_mode);
}

/**
 * ixl_pf_reset - Reset the PF
 * @pf: PF structure
 *
 * Ensure that FW is in the right state and do the reset
 * if needed.
 *
 * @returns zero on success, or an error code on failure.
 */
int
ixl_pf_reset(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	enum i40e_status_code status;
	enum ixl_fw_mode fw_mode;

	fw_mode = ixl_get_fw_mode(pf);
	ixl_dbg_info(pf, "%s: before PF reset FW mode: 0x%08x\n", __func__, fw_mode);
	if (fw_mode == IXL_FW_MODE_RECOVERY) {
		atomic_set_32(&pf->state, IXL_PF_STATE_RECOVERY_MODE);
		/* Don't try to reset device if it's in recovery mode */
		return (0);
	}

	status = i40e_pf_reset(hw);
	if (status == I40E_SUCCESS)
		return (0);

	/* Check FW mode again in case it has changed while
	 * waiting for reset to complete */
	fw_mode = ixl_get_fw_mode(pf);
	ixl_dbg_info(pf, "%s: after PF reset FW mode: 0x%08x\n", __func__, fw_mode);
	if (fw_mode == IXL_FW_MODE_RECOVERY) {
		atomic_set_32(&pf->state, IXL_PF_STATE_RECOVERY_MODE);
		return (0);
	}

	if (fw_mode == IXL_FW_MODE_UEMPR)
		device_printf(pf->dev,
		    "Entering recovery mode due to repeated FW resets. This may take several minutes. Refer to the Intel(R) Ethernet Adapters and Devices User Guide.\n");
	else
		device_printf(pf->dev, "PF reset failure %s\n",
		    i40e_stat_str(hw, status));
	return (EIO);
}

/**
 * ixl_setup_hmc - Setup LAN Host Memory Cache
 * @pf: PF structure
 *
 * Init and configure LAN Host Memory Cache
 *
 * @returns 0 on success, EIO on error
 */
int
ixl_setup_hmc(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	enum i40e_status_code status;

	status = i40e_init_lan_hmc(hw, hw->func_caps.num_tx_qp,
	    hw->func_caps.num_rx_qp, 0, 0);
	if (status) {
		device_printf(pf->dev, "init_lan_hmc failed: %s\n",
		    i40e_stat_str(hw, status));
		return (EIO);
	}

	status = i40e_configure_lan_hmc(hw, I40E_HMC_MODEL_DIRECT_ONLY);
	if (status) {
		device_printf(pf->dev, "configure_lan_hmc failed: %s\n",
		    i40e_stat_str(hw, status));
		return (EIO);
	}

	return (0);
}

/**
 * ixl_shutdown_hmc - Shutdown LAN Host Memory Cache
 * @pf: PF structure
 *
 * Shutdown Host Memory Cache if configured.
 *
 */
void
ixl_shutdown_hmc(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	enum i40e_status_code status;

	/* HMC not configured, no need to shutdown */
	if (hw->hmc.hmc_obj == NULL)
		return;

	status = i40e_shutdown_lan_hmc(hw);
	if (status)
		device_printf(pf->dev,
		    "Shutdown LAN HMC failed with code %s\n",
		    i40e_stat_str(hw, status));
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
	enum i40e_status_code status;
	int len, i2c_intfc_num;
	bool again = TRUE;
	u16 needed;

	if (IXL_PF_IN_RECOVERY_MODE(pf)) {
		hw->func_caps.iwarp = 0;
		return (0);
	}

	len = 40 * sizeof(struct i40e_aqc_list_capabilities_element_resp);
retry:
	if (!(buf = (struct i40e_aqc_list_capabilities_element_resp *)
	    malloc(len, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		device_printf(dev, "Unable to allocate cap memory\n");
                return (ENOMEM);
	}

	/* This populates the hw struct */
        status = i40e_aq_discover_capabilities(hw, buf, len,
	    &needed, i40e_aqc_opc_list_func_capabilities, NULL);
	free(buf, M_DEVBUF);
	if ((pf->hw.aq.asq_last_status == I40E_AQ_RC_ENOMEM) &&
	    (again == TRUE)) {
		/* retry once with a larger buffer */
		again = FALSE;
		len = needed;
		goto retry;
	} else if (status != I40E_SUCCESS) {
		device_printf(dev, "capability discovery failed; status %s, error %s\n",
		    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
		return (ENODEV);
	}

	/*
	 * Some devices have both MDIO and I2C; since this isn't reported
	 * by the FW, check registers to see if an I2C interface exists.
	 */
	i2c_intfc_num = ixl_find_i2c_interface(pf);
	if (i2c_intfc_num != -1)
		pf->has_i2c = true;

	/* Determine functions to use for driver I2C accesses */
	switch (pf->i2c_access_method) {
	case IXL_I2C_ACCESS_METHOD_BEST_AVAILABLE: {
		if (hw->flags & I40E_HW_FLAG_AQ_PHY_ACCESS_CAPABLE) {
			pf->read_i2c_byte = ixl_read_i2c_byte_aq;
			pf->write_i2c_byte = ixl_write_i2c_byte_aq;
		} else {
			pf->read_i2c_byte = ixl_read_i2c_byte_reg;
			pf->write_i2c_byte = ixl_write_i2c_byte_reg;
		}
		break;
	}
	case IXL_I2C_ACCESS_METHOD_AQ:
		pf->read_i2c_byte = ixl_read_i2c_byte_aq;
		pf->write_i2c_byte = ixl_write_i2c_byte_aq;
		break;
	case IXL_I2C_ACCESS_METHOD_REGISTER_I2CCMD:
		pf->read_i2c_byte = ixl_read_i2c_byte_reg;
		pf->write_i2c_byte = ixl_write_i2c_byte_reg;
		break;
	case IXL_I2C_ACCESS_METHOD_BIT_BANG_I2CPARAMS:
		pf->read_i2c_byte = ixl_read_i2c_byte_bb;
		pf->write_i2c_byte = ixl_write_i2c_byte_bb;
		break;
	default:
		/* Should not happen */
		device_printf(dev, "Error setting I2C access functions\n");
		break;
	}

	/* Print a subset of the capability information. */
	device_printf(dev,
	    "PF-ID[%d]: VFs %d, MSI-X %d, VF MSI-X %d, QPs %d, %s\n",
	    hw->pf_id, hw->func_caps.num_vfs, hw->func_caps.num_msix_vectors,
	    hw->func_caps.num_msix_vectors_vf, hw->func_caps.num_tx_qp,
	    (hw->func_caps.mdio_port_mode == 2) ? "I2C" :
	    (hw->func_caps.mdio_port_mode == 1 && pf->has_i2c) ? "MDIO & I2C" :
	    (hw->func_caps.mdio_port_mode == 1) ? "MDIO dedicated" :
	    "MDIO shared");

	return (0);
}

/* For the set_advertise sysctl */
void
ixl_set_initial_advertised_speeds(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	int err;

	/* Make sure to initialize the device to the complete list of
	 * supported speeds on driver load, to ensure unloading and
	 * reloading the driver will restore this value.
	 */
	err = ixl_set_advertised_speeds(pf, pf->supported_speeds, true);
	if (err) {
		/* Non-fatal error */
		device_printf(dev, "%s: ixl_set_advertised_speeds() error %d\n",
			      __func__, err);
		return;
	}

	pf->advertised_speed =
	    ixl_convert_sysctl_aq_link_speed(pf->supported_speeds, false);
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
			    "init: LAN HMC shutdown failure; status %s\n",
			    i40e_stat_str(hw, status));
			goto err_out;
		}
	}

	/* Shutdown admin queue */
	ixl_disable_intr0(hw);
	status = i40e_shutdown_adminq(hw);
	if (status)
		device_printf(dev,
		    "init: Admin Queue shutdown failure; status %s\n",
		    i40e_stat_str(hw, status));

	ixl_pf_qmgr_release(&pf->qmgr, &pf->qtag);
err_out:
	return (status);
}

static u_int
ixl_add_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct ixl_vsi *vsi = arg;

	ixl_add_mc_filter(vsi, (u8*)LLADDR(sdl));

	return (1);
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
	struct ifnet		*ifp = vsi->ifp;
	struct i40e_hw		*hw = vsi->hw;
	int			mcnt = 0, flags;

	IOCTL_DEBUGOUT("ixl_add_multi: begin");

	/*
	** First just get a count, to decide if we
	** we simply use multicast promiscuous.
	*/
	mcnt = if_llmaddr_count(ifp);
	if (__predict_false(mcnt >= MAX_MULTICAST_ADDR)) {
		/* delete existing MC filters */
		ixl_del_hw_filters(vsi, mcnt);
		i40e_aq_set_vsi_multicast_promiscuous(hw,
		    vsi->seid, TRUE, NULL);
		return;
	}

	mcnt = if_foreach_llmaddr(ifp, ixl_add_maddr, vsi);
	if (mcnt > 0) {
		flags = (IXL_FILTER_ADD | IXL_FILTER_USED | IXL_FILTER_MC);
		ixl_add_hw_filters(vsi, flags, mcnt);
	}

	IOCTL_DEBUGOUT("ixl_add_multi: end");
}

static u_int
ixl_match_maddr(void *arg, struct sockaddr_dl *sdl, u_int cnt)
{
	struct ixl_mac_filter *f = arg;

	if (cmp_etheraddr(f->macaddr, (u8 *)LLADDR(sdl)))
		return (1);
	else
		return (0);
}

int
ixl_del_multi(struct ixl_vsi *vsi)
{
	struct ifnet		*ifp = vsi->ifp;
	struct ixl_mac_filter	*f;
	int			mcnt = 0;

	IOCTL_DEBUGOUT("ixl_del_multi: begin");

	/* Search for removed multicast addresses */
	SLIST_FOREACH(f, &vsi->ftl, next)
		if ((f->flags & IXL_FILTER_USED) &&
		    (f->flags & IXL_FILTER_MC) &&
		    (if_foreach_llmaddr(ifp, ixl_match_maddr, f) == 0)) {
			f->flags |= IXL_FILTER_DEL;
			mcnt++;
		}

	if (mcnt > 0)
		ixl_del_hw_filters(vsi, mcnt);

	return (mcnt);
}

void
ixl_link_up_msg(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	struct ifnet *ifp = pf->vsi.ifp;
	char *req_fec_string, *neg_fec_string;
	u8 fec_abilities;

	fec_abilities = hw->phy.link_info.req_fec_info;
	/* If both RS and KR are requested, only show RS */
	if (fec_abilities & I40E_AQ_REQUEST_FEC_RS)
		req_fec_string = ixl_fec_string[0];
	else if (fec_abilities & I40E_AQ_REQUEST_FEC_KR)
		req_fec_string = ixl_fec_string[1];
	else
		req_fec_string = ixl_fec_string[2];

	if (hw->phy.link_info.fec_info & I40E_AQ_CONFIG_FEC_RS_ENA)
		neg_fec_string = ixl_fec_string[0];
	else if (hw->phy.link_info.fec_info & I40E_AQ_CONFIG_FEC_KR_ENA)
		neg_fec_string = ixl_fec_string[1];
	else
		neg_fec_string = ixl_fec_string[2];

	log(LOG_NOTICE, "%s: Link is up, %s Full Duplex, Requested FEC: %s, Negotiated FEC: %s, Autoneg: %s, Flow Control: %s\n",
	    ifp->if_xname,
	    ixl_link_speed_string(hw->phy.link_info.link_speed),
	    req_fec_string, neg_fec_string,
	    (hw->phy.link_info.an_info & I40E_AQ_AN_COMPLETED) ? "True" : "False",
	    (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_TX &&
	        hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_RX) ?
		ixl_fc_string[3] : (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_TX) ?
		ixl_fc_string[2] : (hw->phy.link_info.an_info & I40E_AQ_LINK_PAUSE_RX) ?
		ixl_fc_string[1] : ixl_fc_string[0]);
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
	    I40E_PFINT_ICR0_ENA_PE_CRITERR_MASK |
	    I40E_PFINT_ICR0_ENA_PCI_EXCEPTION_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);

	/*
	 * 0x7FF is the end of the queue list.
	 * This means we won't use MSI-X vector 0 for a queue interrupt
	 * in MSI-X mode.
	 */
	wr32(hw, I40E_PFINT_LNKLST0, 0x7FF);
	/* Value is in 2 usec units, so 0x3E is 62*2 = 124 usecs. */
	wr32(hw, I40E_PFINT_ITR0(IXL_RX_ITR), 0x3E);

	wr32(hw, I40E_PFINT_DYN_CTL0,
	    I40E_PFINT_DYN_CTL0_SW_ITR_INDX_MASK |
	    I40E_PFINT_DYN_CTL0_INTENA_MSK_MASK);

	wr32(hw, I40E_PFINT_STAT_CTL0, 0);
}

void
ixl_add_ifmedia(struct ifmedia *media, u64 phy_types)
{
	/* Display supported media types */
	if (phy_types & (I40E_CAP_PHY_TYPE_100BASE_TX))
		ifmedia_add(media, IFM_ETHER | IFM_100_TX, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_1000BASE_T))
		ifmedia_add(media, IFM_ETHER | IFM_1000_T, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_1000BASE_SX))
		ifmedia_add(media, IFM_ETHER | IFM_1000_SX, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_1000BASE_LX))
		ifmedia_add(media, IFM_ETHER | IFM_1000_LX, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_2_5GBASE_T))
		ifmedia_add(media, IFM_ETHER | IFM_2500_T, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_5GBASE_T))
		ifmedia_add(media, IFM_ETHER | IFM_5000_T, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_XAUI) ||
	    phy_types & (I40E_CAP_PHY_TYPE_XFI) ||
	    phy_types & (I40E_CAP_PHY_TYPE_10GBASE_SFPP_CU))
		ifmedia_add(media, IFM_ETHER | IFM_10G_TWINAX, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_SR))
		ifmedia_add(media, IFM_ETHER | IFM_10G_SR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_LR))
		ifmedia_add(media, IFM_ETHER | IFM_10G_LR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_T))
		ifmedia_add(media, IFM_ETHER | IFM_10G_T, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_40GBASE_CR4) ||
	    phy_types & (I40E_CAP_PHY_TYPE_40GBASE_CR4_CU) ||
	    phy_types & (I40E_CAP_PHY_TYPE_40GBASE_AOC) ||
	    phy_types & (I40E_CAP_PHY_TYPE_XLAUI) ||
	    phy_types & (I40E_CAP_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(media, IFM_ETHER | IFM_40G_CR4, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_40GBASE_SR4))
		ifmedia_add(media, IFM_ETHER | IFM_40G_SR4, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_40GBASE_LR4))
		ifmedia_add(media, IFM_ETHER | IFM_40G_LR4, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_1000BASE_KX))
		ifmedia_add(media, IFM_ETHER | IFM_1000_KX, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_CR1_CU)
	    || phy_types & (I40E_CAP_PHY_TYPE_10GBASE_CR1))
		ifmedia_add(media, IFM_ETHER | IFM_10G_CR1, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_AOC))
		ifmedia_add(media, IFM_ETHER | IFM_10G_AOC, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_SFI))
		ifmedia_add(media, IFM_ETHER | IFM_10G_SFI, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_KX4))
		ifmedia_add(media, IFM_ETHER | IFM_10G_KX4, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_10GBASE_KR))
		ifmedia_add(media, IFM_ETHER | IFM_10G_KR, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_20GBASE_KR2))
		ifmedia_add(media, IFM_ETHER | IFM_20G_KR2, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_40GBASE_KR4))
		ifmedia_add(media, IFM_ETHER | IFM_40G_KR4, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_XLPPI))
		ifmedia_add(media, IFM_ETHER | IFM_40G_XLPPI, 0, NULL);

	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_KR))
		ifmedia_add(media, IFM_ETHER | IFM_25G_KR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_CR))
		ifmedia_add(media, IFM_ETHER | IFM_25G_CR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_SR))
		ifmedia_add(media, IFM_ETHER | IFM_25G_SR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_LR))
		ifmedia_add(media, IFM_ETHER | IFM_25G_LR, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_AOC))
		ifmedia_add(media, IFM_ETHER | IFM_25G_AOC, 0, NULL);
	if (phy_types & (I40E_CAP_PHY_TYPE_25GBASE_ACC))
		ifmedia_add(media, IFM_ETHER | IFM_25G_ACC, 0, NULL);
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
	device_t 	dev = iflib_get_dev(vsi->ctx);
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
		    LE16_TO_CPU(sw_config->header.num_reported),
		    LE16_TO_CPU(sw_config->header.num_total));
		for (int i = 0;
		    i < LE16_TO_CPU(sw_config->header.num_reported); i++) {
			device_printf(dev,
			    "-> %d: type=%d seid=%d uplink=%d downlink=%d\n", i,
			    sw_config->element[i].element_type,
			    LE16_TO_CPU(sw_config->element[i].seid),
			    LE16_TO_CPU(sw_config->element[i].uplink_seid),
			    LE16_TO_CPU(sw_config->element[i].downlink_seid));
		}
	}
	/* Simplified due to a single VSI */
	vsi->uplink_seid = LE16_TO_CPU(sw_config->element[0].uplink_seid);
	vsi->downlink_seid = LE16_TO_CPU(sw_config->element[0].downlink_seid);
	vsi->seid = LE16_TO_CPU(sw_config->element[0].seid);
	return (ret);
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

	vsi->num_hw_filters = 0;
}

void
ixl_vsi_add_sysctls(struct ixl_vsi * vsi, const char * sysctl_name, bool queues_sysctls)
{
	struct sysctl_oid *tree;
	struct sysctl_oid_list *child;
	struct sysctl_oid_list *vsi_list;

	tree = device_get_sysctl_tree(vsi->dev);
	child = SYSCTL_CHILDREN(tree);
	vsi->vsi_node = SYSCTL_ADD_NODE(&vsi->sysctl_ctx, child, OID_AUTO, sysctl_name,
			CTLFLAG_RD, NULL, "VSI Number");

	vsi_list = SYSCTL_CHILDREN(vsi->vsi_node);
	ixl_add_sysctls_eth_stats(&vsi->sysctl_ctx, vsi_list, &vsi->eth_stats);

	if (queues_sysctls)
		ixl_vsi_add_queues_stats(vsi, &vsi->sysctl_ctx);
}

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
ixl_add_sysctls_mac_stats(struct sysctl_ctx_list *ctx,
	struct sysctl_oid_list *child,
	struct i40e_hw_port_stats *stats)
{
	struct sysctl_oid *stat_node = SYSCTL_ADD_NODE(ctx, child, OID_AUTO,
	    "mac", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL, "Mac Statistics");
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
	u32 rss_seed[IXL_RSS_KEY_SIZE_REG];
	enum i40e_status_code status;

#ifdef RSS
        /* Fetch the configured RSS key */
        rss_getkey((uint8_t *) &rss_seed);
#else
	ixl_get_default_rss_key(rss_seed);
#endif
	/* Fill out hash function seed */
	if (hw->mac.type == I40E_MAC_X722) {
		struct i40e_aqc_get_set_rss_key_data key_data;
		bcopy(rss_seed, &key_data, 52);
		status = i40e_aq_set_rss_key(hw, vsi->vsi_num, &key_data);
		if (status)
			device_printf(dev,
			    "i40e_aq_set_rss_key status %s, error %s\n",
			    i40e_stat_str(hw, status),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
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
	if (hw->mac.type == I40E_MAC_X722)
		set_hena = IXL_DEFAULT_RSS_HENA_X722;
	else
		set_hena = IXL_DEFAULT_RSS_HENA_XL710;
#endif
	hena = (u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0)) |
	    ((u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1)) << 32);
	hena |= set_hena;
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(0), (u32)hena);
	i40e_write_rx_ctl(hw, I40E_PFQF_HENA(1), (u32)(hena >> 32));

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
 * In some firmware versions there is default MAC/VLAN filter
 * configured which interferes with filters managed by driver.
 * Make sure it's removed.
 */
void
ixl_del_default_hw_filters(struct ixl_vsi *vsi)
{
	struct i40e_aqc_remove_macvlan_element_data e;

	bzero(&e, sizeof(e));
	bcopy(vsi->hw->mac.perm_addr, e.mac_addr, ETHER_ADDR_LEN);
	e.vlan_tag = 0;
	e.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
	i40e_aq_remove_macvlan(vsi->hw, vsi->seid, &e, 1, NULL);

	bzero(&e, sizeof(e));
	bcopy(vsi->hw->mac.perm_addr, e.mac_addr, ETHER_ADDR_LEN);
	e.vlan_tag = 0;
	e.flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH |
		I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
	i40e_aq_remove_macvlan(vsi->hw, vsi->seid, &e, 1, NULL);
}

/*
** Initialize filter list and add filters that the hardware
** needs to know about.
**
** Requires VSI's seid to be set before calling.
*/
void
ixl_init_filters(struct ixl_vsi *vsi)
{
	struct ixl_pf *pf = (struct ixl_pf *)vsi->back;

	ixl_dbg_filter(pf, "%s: start\n", __func__);

	/* Initialize mac filter list for VSI */
	SLIST_INIT(&vsi->ftl);
	vsi->num_hw_filters = 0;

	/* Receive broadcast Ethernet frames */
	i40e_aq_set_vsi_broadcast(&pf->hw, vsi->seid, TRUE, NULL);

	if (IXL_VSI_IS_VF(vsi))
		return;

	ixl_del_default_hw_filters(vsi);

	ixl_add_filter(vsi, vsi->hw->mac.addr, IXL_VLAN_ANY);

	/*
	 * Prevent Tx flow control frames from being sent out by
	 * non-firmware transmitters.
	 * This affects every VSI in the PF.
	 */
#ifndef IXL_DEBUG_FC
	i40e_add_filter_to_drop_tx_flow_control_frames(vsi->hw, vsi->seid);
#else
	if (pf->enable_tx_fc_filter)
		i40e_add_filter_to_drop_tx_flow_control_frames(vsi->hw, vsi->seid);
#endif
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

	f = ixl_new_filter(vsi, macaddr, IXL_VLAN_ANY);
	if (f != NULL)
		f->flags |= IXL_FILTER_MC;
	else
		printf("WARNING: no filter available!!\n");
}

void
ixl_reconfigure_filters(struct ixl_vsi *vsi)
{
	ixl_add_hw_filters(vsi, IXL_FILTER_USED, vsi->num_macs);
}

/*
 * This routine adds a MAC/VLAN filter to the software filter
 * list, then adds that new filter to the HW if it doesn't already
 * exist in the SW filter list.
 */
void
ixl_add_filter(struct ixl_vsi *vsi, const u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f, *tmp;
	struct ixl_pf		*pf;
	device_t		dev;

	pf = vsi->back;
	dev = pf->dev;

	ixl_dbg_filter(pf, "ixl_add_filter: " MAC_FORMAT ", vlan %4d\n",
	    MAC_FORMAT_ARGS(macaddr), vlan);

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

	f = ixl_new_filter(vsi, macaddr, vlan);
	if (f == NULL) {
		device_printf(dev, "WARNING: no filter available!!\n");
		return;
	}
	if (f->vlan != IXL_VLAN_ANY)
		f->flags |= IXL_FILTER_VLAN;
	else
		vsi->num_macs++;

	f->flags |= IXL_FILTER_USED;
	ixl_add_hw_filters(vsi, f->flags, 1);
}

void
ixl_del_filter(struct ixl_vsi *vsi, const u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter *f;

	ixl_dbg_filter((struct ixl_pf *)vsi->back,
	    "ixl_del_filter: " MAC_FORMAT ", vlan %4d\n",
	    MAC_FORMAT_ARGS(macaddr), vlan);

	f = ixl_find_filter(vsi, macaddr, vlan);
	if (f == NULL)
		return;

	f->flags |= IXL_FILTER_DEL;
	ixl_del_hw_filters(vsi, 1);
	if (f->vlan == IXL_VLAN_ANY && (f->flags & IXL_FILTER_VLAN) != 0)
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
ixl_find_filter(struct ixl_vsi *vsi, const u8 *macaddr, s16 vlan)
{
	struct ixl_mac_filter	*f;

	SLIST_FOREACH(f, &vsi->ftl, next) {
		if ((cmp_etheraddr(f->macaddr, macaddr) != 0)
		    && (f->vlan == vlan)) {
			return (f);
		}
	}

	return (NULL);
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
	enum i40e_status_code	status;
	int			j = 0;

	pf = vsi->back;
	dev = vsi->dev;
	hw = &pf->hw;

	ixl_dbg_filter(pf,
	    "ixl_add_hw_filters: flags: %d cnt: %d\n", flags, cnt);

	if (cnt < 1) {
		ixl_dbg_info(pf, "ixl_add_hw_filters: cnt == 0\n");
		return;
	}

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
		if ((f->flags & flags) == flags) {
			b = &a[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, b->mac_addr, ETHER_ADDR_LEN);
			if (f->vlan == IXL_VLAN_ANY) {
				b->vlan_tag = 0;
				b->flags = CPU_TO_LE16(
				    I40E_AQC_MACVLAN_ADD_IGNORE_VLAN);
			} else {
				b->vlan_tag = CPU_TO_LE16(f->vlan);
				b->flags = 0;
			}
			b->flags |= CPU_TO_LE16(
			    I40E_AQC_MACVLAN_ADD_PERFECT_MATCH);
			f->flags &= ~IXL_FILTER_ADD;
			j++;

			ixl_dbg_filter(pf, "ADD: " MAC_FORMAT "\n",
			    MAC_FORMAT_ARGS(f->macaddr));
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		status = i40e_aq_add_macvlan(hw, vsi->seid, a, j, NULL);
		if (status)
			device_printf(dev, "i40e_aq_add_macvlan status %s, "
			    "error %s\n", i40e_stat_str(hw, status),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
		else
			vsi->num_hw_filters += j;
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
	enum i40e_status_code	status;
	int			j = 0;

	pf = vsi->back;
	hw = &pf->hw;
	dev = vsi->dev;

	ixl_dbg_filter(pf, "%s: start, cnt: %d\n", __func__, cnt);

	d = malloc(sizeof(struct i40e_aqc_remove_macvlan_element_data) * cnt,
	    M_DEVBUF, M_NOWAIT | M_ZERO);
	if (d == NULL) {
		device_printf(dev, "%s: failed to get memory\n", __func__);
		return;
	}

	SLIST_FOREACH_SAFE(f, &vsi->ftl, next, f_temp) {
		if (f->flags & IXL_FILTER_DEL) {
			e = &d[j]; // a pox on fvl long names :)
			bcopy(f->macaddr, e->mac_addr, ETHER_ADDR_LEN);
			e->flags = I40E_AQC_MACVLAN_DEL_PERFECT_MATCH;
			if (f->vlan == IXL_VLAN_ANY) {
				e->vlan_tag = 0;
				e->flags |= I40E_AQC_MACVLAN_DEL_IGNORE_VLAN;
			} else {
				e->vlan_tag = f->vlan;
			}

			ixl_dbg_filter(pf, "DEL: " MAC_FORMAT "\n",
			    MAC_FORMAT_ARGS(f->macaddr));

			/* delete entry from vsi list */
			SLIST_REMOVE(&vsi->ftl, f, ixl_mac_filter, next);
			free(f, M_DEVBUF);
			j++;
		}
		if (j == cnt)
			break;
	}
	if (j > 0) {
		status = i40e_aq_remove_macvlan(hw, vsi->seid, d, j, NULL);
		if (status) {
			int sc = 0;
			for (int i = 0; i < j; i++)
				sc += (!d[i].error_code);
			vsi->num_hw_filters -= sc;
			device_printf(dev,
			    "Failed to remove %d/%d filters, error %s\n",
			    j - sc, j, i40e_aq_str(hw, hw->aq.asq_last_status));
		} else
			vsi->num_hw_filters -= j;
	}
	free(d, M_DEVBUF);

	ixl_dbg_filter(pf, "%s: end\n", __func__);
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
		i40e_usec_delay(10);
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
		i40e_usec_delay(10);
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

/*
 * Returns error on first ring that is detected hung.
 */
int
ixl_disable_tx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	ixl_dbg(pf, IXL_DBG_EN_DIS,
	    "Disabling PF TX ring %4d / VSI TX ring %4d...\n",
	    pf_qidx, vsi_qidx);

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

/*
 * Returns error on first ring that is detected hung.
 */
int
ixl_disable_rx_ring(struct ixl_pf *pf, struct ixl_pf_qtag *qtag, u16 vsi_qidx)
{
	struct i40e_hw	*hw = &pf->hw;
	int		error = 0;
	u32		reg;
	u16		pf_qidx;

	pf_qidx = ixl_pf_qidx_from_vsi_qidx(qtag, vsi_qidx);

	ixl_dbg(pf, IXL_DBG_EN_DIS,
	    "Disabling PF RX ring %4d / VSI RX ring %4d...\n",
	    pf_qidx, vsi_qidx);

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

static void
ixl_handle_tx_mdd_event(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct ixl_vf *vf;
	bool mdd_detected = false;
	bool pf_mdd_detected = false;
	bool vf_mdd_detected = false;
	u16 vf_num, queue;
	u8 pf_num, event;
	u8 pf_mdet_num, vp_mdet_num;
	u32 reg;

	/* find what triggered the MDD event */
	reg = rd32(hw, I40E_GL_MDET_TX);
	if (reg & I40E_GL_MDET_TX_VALID_MASK) {
		pf_num = (reg & I40E_GL_MDET_TX_PF_NUM_MASK) >>
		    I40E_GL_MDET_TX_PF_NUM_SHIFT;
		vf_num = (reg & I40E_GL_MDET_TX_VF_NUM_MASK) >>
		    I40E_GL_MDET_TX_VF_NUM_SHIFT;
		event = (reg & I40E_GL_MDET_TX_EVENT_MASK) >>
		    I40E_GL_MDET_TX_EVENT_SHIFT;
		queue = (reg & I40E_GL_MDET_TX_QUEUE_MASK) >>
		    I40E_GL_MDET_TX_QUEUE_SHIFT;
		wr32(hw, I40E_GL_MDET_TX, 0xffffffff);
		mdd_detected = true;
	}

	if (!mdd_detected)
		return;

	reg = rd32(hw, I40E_PF_MDET_TX);
	if (reg & I40E_PF_MDET_TX_VALID_MASK) {
		wr32(hw, I40E_PF_MDET_TX, 0xFFFF);
		pf_mdet_num = hw->pf_id;
		pf_mdd_detected = true;
	}

	/* Check if MDD was caused by a VF */
	for (int i = 0; i < pf->num_vfs; i++) {
		vf = &(pf->vfs[i]);
		reg = rd32(hw, I40E_VP_MDET_TX(i));
		if (reg & I40E_VP_MDET_TX_VALID_MASK) {
			wr32(hw, I40E_VP_MDET_TX(i), 0xFFFF);
			vp_mdet_num = i;
			vf->num_mdd_events++;
			vf_mdd_detected = true;
		}
	}

	/* Print out an error message */
	if (vf_mdd_detected && pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on TX queue %d, pf number %d (PF-%d), vf number %d (VF-%d)\n",
		    event, queue, pf_num, pf_mdet_num, vf_num, vp_mdet_num);
	else if (vf_mdd_detected && !pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on TX queue %d, pf number %d, vf number %d (VF-%d)\n",
		    event, queue, pf_num, vf_num, vp_mdet_num);
	else if (!vf_mdd_detected && pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on TX queue %d, pf number %d (PF-%d)\n",
		    event, queue, pf_num, pf_mdet_num);
	/* Theoretically shouldn't happen */
	else
		device_printf(dev,
		    "TX Malicious Driver Detection event (unknown)\n");
}

static void
ixl_handle_rx_mdd_event(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct ixl_vf *vf;
	bool mdd_detected = false;
	bool pf_mdd_detected = false;
	bool vf_mdd_detected = false;
	u16 queue;
	u8 pf_num, event;
	u8 pf_mdet_num, vp_mdet_num;
	u32 reg;

	/*
	 * GL_MDET_RX doesn't contain VF number information, unlike
	 * GL_MDET_TX.
	 */
	reg = rd32(hw, I40E_GL_MDET_RX);
	if (reg & I40E_GL_MDET_RX_VALID_MASK) {
		pf_num = (reg & I40E_GL_MDET_RX_FUNCTION_MASK) >>
		    I40E_GL_MDET_RX_FUNCTION_SHIFT;
		event = (reg & I40E_GL_MDET_RX_EVENT_MASK) >>
		    I40E_GL_MDET_RX_EVENT_SHIFT;
		queue = (reg & I40E_GL_MDET_RX_QUEUE_MASK) >>
		    I40E_GL_MDET_RX_QUEUE_SHIFT;
		wr32(hw, I40E_GL_MDET_RX, 0xffffffff);
		mdd_detected = true;
	}

	if (!mdd_detected)
		return;

	reg = rd32(hw, I40E_PF_MDET_RX);
	if (reg & I40E_PF_MDET_RX_VALID_MASK) {
		wr32(hw, I40E_PF_MDET_RX, 0xFFFF);
		pf_mdet_num = hw->pf_id;
		pf_mdd_detected = true;
	}

	/* Check if MDD was caused by a VF */
	for (int i = 0; i < pf->num_vfs; i++) {
		vf = &(pf->vfs[i]);
		reg = rd32(hw, I40E_VP_MDET_RX(i));
		if (reg & I40E_VP_MDET_RX_VALID_MASK) {
			wr32(hw, I40E_VP_MDET_RX(i), 0xFFFF);
			vp_mdet_num = i;
			vf->num_mdd_events++;
			vf_mdd_detected = true;
		}
	}

	/* Print out an error message */
	if (vf_mdd_detected && pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on RX queue %d, pf number %d (PF-%d), (VF-%d)\n",
		    event, queue, pf_num, pf_mdet_num, vp_mdet_num);
	else if (vf_mdd_detected && !pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on RX queue %d, pf number %d, (VF-%d)\n",
		    event, queue, pf_num, vp_mdet_num);
	else if (!vf_mdd_detected && pf_mdd_detected)
		device_printf(dev,
		    "Malicious Driver Detection event %d"
		    " on RX queue %d, pf number %d (PF-%d)\n",
		    event, queue, pf_num, pf_mdet_num);
	/* Theoretically shouldn't happen */
	else
		device_printf(dev,
		    "RX Malicious Driver Detection event (unknown)\n");
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
	u32 reg;

	/*
	 * Handle both TX/RX because it's possible they could
	 * both trigger in the same interrupt.
	 */
	ixl_handle_tx_mdd_event(pf);
	ixl_handle_rx_mdd_event(pf);

	atomic_clear_32(&pf->state, IXL_PF_STATE_MDD_PENDING);

	/* re-enable mdd interrupt cause */
	reg = rd32(hw, I40E_PFINT_ICR0_ENA);
	reg |= I40E_PFINT_ICR0_ENA_MAL_DETECT_MASK;
	wr32(hw, I40E_PFINT_ICR0_ENA, reg);
	ixl_flush(hw);
}

void
ixl_enable_intr0(struct i40e_hw *hw)
{
	u32		reg;

	/* Use IXL_ITR_NONE so ITR isn't updated here */
	reg = I40E_PFINT_DYN_CTL0_INTENA_MASK |
	    I40E_PFINT_DYN_CTL0_CLEARPBA_MASK |
	    (IXL_ITR_NONE << I40E_PFINT_DYN_CTL0_ITR_INDX_SHIFT);
	wr32(hw, I40E_PFINT_DYN_CTL0, reg);
}

void
ixl_disable_intr0(struct i40e_hw *hw)
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
ixl_handle_empr_reset(struct ixl_pf *pf)
{
	struct ixl_vsi	*vsi = &pf->vsi;
	bool is_up = !!(vsi->ifp->if_drv_flags & IFF_DRV_RUNNING);

	ixl_prepare_for_reset(pf, is_up);
	/*
	 * i40e_pf_reset checks the type of reset and acts
	 * accordingly. If EMP or Core reset was performed
	 * doing PF reset is not necessary and it sometimes
	 * fails.
	 */
	ixl_pf_reset(pf);

	if (!IXL_PF_IN_RECOVERY_MODE(pf) &&
	    ixl_get_fw_mode(pf) == IXL_FW_MODE_RECOVERY) {
		atomic_set_32(&pf->state, IXL_PF_STATE_RECOVERY_MODE);
		device_printf(pf->dev,
		    "Firmware recovery mode detected. Limiting functionality. Refer to Intel(R) Ethernet Adapters and Devices User Guide for details on firmware recovery mode.\n");
		pf->link_up = FALSE;
		ixl_update_link_status(pf);
	}

	ixl_rebuild_hw_structs_after_reset(pf, is_up);

	atomic_clear_32(&pf->state, IXL_PF_STATE_ADAPTER_RESETTING);
}

void
ixl_update_stats_counters(struct ixl_pf *pf)
{
	struct i40e_hw	*hw = &pf->hw;
	struct ixl_vsi	*vsi = &pf->vsi;
	struct ixl_vf	*vf;
	u64 prev_link_xoff_rx = pf->stats.link_xoff_rx;

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

	/*
	 * For watchdog management we need to know if we have been paused
	 * during the last interval, so capture that here.
	 */
	if (pf->stats.link_xoff_rx != prev_link_xoff_rx)
		vsi->shared->isc_pause_frames = 1;

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
	/* EEE */
	i40e_get_phy_lpi_status(hw, nsd);

	i40e_lpi_stat_update(hw, pf->stat_offsets_loaded,
			  &osd->tx_lpi_count, &nsd->tx_lpi_count,
			  &osd->rx_lpi_count, &nsd->rx_lpi_count);

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
	u16 stat_idx = vsi->info.stat_counter_idx;

	es = &vsi->eth_stats;
	oes = &vsi->eth_stats_offsets;

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

/**
 * Add subset of device sysctls safe to use in recovery mode
 */
void
ixl_add_sysctls_recovery_mode(struct ixl_pf *pf)
{
	device_t dev = pf->dev;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	struct sysctl_oid *debug_node;
	struct sysctl_oid_list *debug_list;

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_show_fw, "A", "Firmware version");

	/* Add sysctls meant to print debug information, but don't list them
	 * in "sysctl -a" output. */
	debug_node = SYSCTL_ADD_NODE(ctx, ctx_list,
	    OID_AUTO, "debug", CTLFLAG_RD | CTLFLAG_SKIP | CTLFLAG_MPSAFE, NULL,
	    "Debug Sysctls");
	debug_list = SYSCTL_CHILDREN(debug_node);

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "shared_debug_mask", CTLFLAG_RW,
	    &pf->hw.debug_mask, 0, "Shared code debug message level");

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "core_debug_mask", CTLFLAG_RW,
	    &pf->dbg_mask, 0, "Non-shared code debug message level");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "dump_debug_data",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_dump_debug_data, "A", "Dump Debug Data from FW");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_pf_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_pf_reset, "I", "Tell HW to initiate a PF reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_core_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_core_reset, "I", "Tell HW to initiate a CORE reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_global_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_global_reset, "I", "Tell HW to initiate a GLOBAL reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "queue_interrupt_table",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_queue_interrupt_table, "A", "View MSI-X indices for TX/RX queues");
}

void
ixl_add_device_sysctls(struct ixl_pf *pf)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;

	struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
	struct sysctl_oid_list *ctx_list =
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev));

	struct sysctl_oid *debug_node;
	struct sysctl_oid_list *debug_list;

	struct sysctl_oid *fec_node;
	struct sysctl_oid_list *fec_list;
	struct sysctl_oid *eee_node;
	struct sysctl_oid_list *eee_list;

	/* Set up sysctls */
	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fc", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_set_flowcntl, "I", IXL_SYSCTL_HELP_FC);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "advertise_speed",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_set_advertise, "I", IXL_SYSCTL_HELP_SET_ADVERTISE);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "supported_speeds",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_supported_speeds, "I", IXL_SYSCTL_HELP_SUPPORTED_SPEED);

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "current_speed",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_current_speed, "A", "Current Port Speed");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_version",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_show_fw, "A", "Firmware version");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "unallocated_queues",
	    CTLTYPE_INT | CTLFLAG_RD | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_unallocated_queues, "I",
	    "Queues not allocated to a PF or VF");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "tx_itr",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_pf_tx_itr, "I",
	    "Immediately set TX ITR value for all queues");

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "rx_itr",
	    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
	    ixl_sysctl_pf_rx_itr, "I",
	    "Immediately set RX ITR value for all queues");

	SYSCTL_ADD_INT(ctx, ctx_list,
	    OID_AUTO, "dynamic_rx_itr", CTLFLAG_RW,
	    &pf->dynamic_rx_itr, 0, "Enable dynamic RX ITR");

	SYSCTL_ADD_INT(ctx, ctx_list,
	    OID_AUTO, "dynamic_tx_itr", CTLFLAG_RW,
	    &pf->dynamic_tx_itr, 0, "Enable dynamic TX ITR");

	/* Add FEC sysctls for 25G adapters */
	if (i40e_is_25G_device(hw->device_id)) {
		fec_node = SYSCTL_ADD_NODE(ctx, ctx_list,
		    OID_AUTO, "fec", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
		    "FEC Sysctls");
		fec_list = SYSCTL_CHILDREN(fec_node);

		SYSCTL_ADD_PROC(ctx, fec_list,
		    OID_AUTO, "fc_ability",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
		    ixl_sysctl_fec_fc_ability, "I", "FC FEC ability enabled");

		SYSCTL_ADD_PROC(ctx, fec_list,
		    OID_AUTO, "rs_ability",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
		    ixl_sysctl_fec_rs_ability, "I", "RS FEC ability enabled");

		SYSCTL_ADD_PROC(ctx, fec_list,
		    OID_AUTO, "fc_requested",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
		    ixl_sysctl_fec_fc_request, "I",
		    "FC FEC mode requested on link");

		SYSCTL_ADD_PROC(ctx, fec_list,
		    OID_AUTO, "rs_requested",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
		    ixl_sysctl_fec_rs_request, "I",
		    "RS FEC mode requested on link");

		SYSCTL_ADD_PROC(ctx, fec_list,
		    OID_AUTO, "auto_fec_enabled",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT, pf, 0,
		    ixl_sysctl_fec_auto_enable, "I",
		    "Let FW decide FEC ability/request modes");
	}

	SYSCTL_ADD_PROC(ctx, ctx_list,
	    OID_AUTO, "fw_lldp", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_fw_lldp, "I", IXL_SYSCTL_HELP_FW_LLDP);

	eee_node = SYSCTL_ADD_NODE(ctx, ctx_list,
	    OID_AUTO, "eee", CTLFLAG_RD | CTLFLAG_MPSAFE, NULL,
	    "Energy Efficient Ethernet (EEE) Sysctls");
	eee_list = SYSCTL_CHILDREN(eee_node);

	SYSCTL_ADD_PROC(ctx, eee_list,
	    OID_AUTO, "enable", CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_MPSAFE,
	    pf, 0, ixl_sysctl_eee_enable, "I",
	    "Enable Energy Efficient Ethernet (EEE)");

	SYSCTL_ADD_UINT(ctx, eee_list, OID_AUTO, "tx_lpi_status",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &pf->stats.tx_lpi_status, 0,
	    "TX LPI status");

	SYSCTL_ADD_UINT(ctx, eee_list, OID_AUTO, "rx_lpi_status",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &pf->stats.rx_lpi_status, 0,
	    "RX LPI status");

	SYSCTL_ADD_UQUAD(ctx, eee_list, OID_AUTO, "tx_lpi_count",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &pf->stats.tx_lpi_count,
	    "TX LPI count");

	SYSCTL_ADD_UQUAD(ctx, eee_list, OID_AUTO, "rx_lpi_count",
	    CTLFLAG_RD | CTLFLAG_MPSAFE, &pf->stats.rx_lpi_count,
	    "RX LPI count");

	/* Add sysctls meant to print debug information, but don't list them
	 * in "sysctl -a" output. */
	debug_node = SYSCTL_ADD_NODE(ctx, ctx_list,
	    OID_AUTO, "debug", CTLFLAG_RD | CTLFLAG_SKIP | CTLFLAG_MPSAFE, NULL,
	    "Debug Sysctls");
	debug_list = SYSCTL_CHILDREN(debug_node);

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "shared_debug_mask", CTLFLAG_RW,
	    &pf->hw.debug_mask, 0, "Shared code debug message level");

	SYSCTL_ADD_UINT(ctx, debug_list,
	    OID_AUTO, "core_debug_mask", CTLFLAG_RW,
	    &pf->dbg_mask, 0, "Non-shared code debug message level");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "link_status",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_link_status, "A", IXL_SYSCTL_HELP_LINK_STATUS);

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "phy_abilities",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_phy_abilities, "A", "PHY Abilities");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "filter_list",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_sw_filter_list, "A", "SW Filter List");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "hw_res_alloc",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_hw_res_alloc, "A", "HW Resource Allocation");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "switch_config",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_switch_config, "A", "HW Switch Configuration");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "rss_key",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_hkey, "A", "View RSS key");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "rss_lut",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_hlut, "A", "View RSS lookup table");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "rss_hena",
	    CTLTYPE_ULONG | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_hena, "LU", "View enabled packet types for RSS");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "disable_fw_link_management",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_fw_link_management, "I", "Disable FW Link Management");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "dump_debug_data",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_dump_debug_data, "A", "Dump Debug Data from FW");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_pf_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_pf_reset, "I", "Tell HW to initiate a PF reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_core_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_core_reset, "I", "Tell HW to initiate a CORE reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "do_global_reset",
	    CTLTYPE_INT | CTLFLAG_WR | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_do_global_reset, "I", "Tell HW to initiate a GLOBAL reset");

	SYSCTL_ADD_PROC(ctx, debug_list,
	    OID_AUTO, "queue_interrupt_table",
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
	    pf, 0, ixl_sysctl_queue_interrupt_table, "A", "View MSI-X indices for TX/RX queues");

	if (pf->has_i2c) {
		SYSCTL_ADD_PROC(ctx, debug_list,
		    OID_AUTO, "read_i2c_byte",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
		    pf, 0, ixl_sysctl_read_i2c_byte, "I", IXL_SYSCTL_HELP_READ_I2C);

		SYSCTL_ADD_PROC(ctx, debug_list,
		    OID_AUTO, "write_i2c_byte",
		    CTLTYPE_INT | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
		    pf, 0, ixl_sysctl_write_i2c_byte, "I", IXL_SYSCTL_HELP_WRITE_I2C);

		SYSCTL_ADD_PROC(ctx, debug_list,
		    OID_AUTO, "read_i2c_diag_data",
		    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_NEEDGIANT,
		    pf, 0, ixl_sysctl_read_i2c_diag_data, "A", "Dump selected diagnostic data from FW");
	}
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

	queues = (int)ixl_pf_qmgr_get_num_free(&pf->qmgr);

	return sysctl_handle_int(oidp, NULL, queues, req);
}

static const char *
ixl_link_speed_string(enum i40e_aq_link_speed link_speed)
{
	const char * link_speed_str[] = {
		"Unknown",
		"100 Mbps",
		"1 Gbps",
		"10 Gbps",
		"40 Gbps",
		"20 Gbps",
		"25 Gbps",
		"2.5 Gbps",
		"5 Gbps"
	};
	int index;

	switch (link_speed) {
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
	case I40E_LINK_SPEED_25GB:
		index = 6;
		break;
	case I40E_LINK_SPEED_2_5GB:
		index = 7;
		break;
	case I40E_LINK_SPEED_5GB:
		index = 8;
		break;
	case I40E_LINK_SPEED_UNKNOWN:
	default:
		index = 0;
		break;
	}

	return (link_speed_str[index]);
}

int
ixl_sysctl_current_speed(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	int error = 0;

	ixl_update_link_status(pf);

	error = sysctl_handle_string(oidp,
	    __DECONST(void *,
		ixl_link_speed_string(hw->phy.link_info.link_speed)),
	    8, req);

	return (error);
}

/*
 * Converts 8-bit speeds value to and from sysctl flags and
 * Admin Queue flags.
 */
static u8
ixl_convert_sysctl_aq_link_speed(u8 speeds, bool to_aq)
{
#define SPEED_MAP_SIZE 8
	static u16 speedmap[SPEED_MAP_SIZE] = {
		(I40E_LINK_SPEED_100MB | (0x1 << 8)),
		(I40E_LINK_SPEED_1GB   | (0x2 << 8)),
		(I40E_LINK_SPEED_10GB  | (0x4 << 8)),
		(I40E_LINK_SPEED_20GB  | (0x8 << 8)),
		(I40E_LINK_SPEED_25GB  | (0x10 << 8)),
		(I40E_LINK_SPEED_40GB  | (0x20 << 8)),
		(I40E_LINK_SPEED_2_5GB | (0x40 << 8)),
		(I40E_LINK_SPEED_5GB   | (0x80 << 8)),
	};
	u8 retval = 0;

	for (int i = 0; i < SPEED_MAP_SIZE; i++) {
		if (to_aq)
			retval |= (speeds & (speedmap[i] >> 8)) ? (speedmap[i] & 0xff) : 0;
		else
			retval |= (speeds & speedmap[i]) ? (speedmap[i] >> 8) : 0;
	}

	return (retval);
}

int
ixl_set_advertised_speeds(struct ixl_pf *pf, int speeds, bool from_aq)
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
	if (from_aq)
		config.link_speed = speeds;
	else
		config.link_speed = ixl_convert_sysctl_aq_link_speed(speeds, true);
	config.phy_type = abilities.phy_type;
	config.phy_type_ext = abilities.phy_type_ext;
	config.abilities = abilities.abilities
	    | I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
	config.eee_capability = abilities.eee_capability;
	config.eeer = abilities.eeer_val;
	config.low_power_ctrl = abilities.d3_lpan;
	config.fec_config = abilities.fec_cfg_curr_mod_ext_info
	    & I40E_AQ_PHY_FEC_CONFIG_MASK;

	/* Do aq command & restart link */
	aq_error = i40e_aq_set_phy_config(hw, &config, NULL);
	if (aq_error) {
		device_printf(dev,
		    "%s: Error setting new phy config %d,"
		    " aq error: %d\n", __func__, aq_error,
		    hw->aq.asq_last_status);
		return (EIO);
	}

	return (0);
}

/*
** Supported link speeds
**	Flags:
**	 0x1 - 100 Mb
**	 0x2 - 1G
**	 0x4 - 10G
**	 0x8 - 20G
**	0x10 - 25G
**	0x20 - 40G
**	0x40 - 2.5G
**	0x80 - 5G
*/
static int
ixl_sysctl_supported_speeds(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int supported = ixl_convert_sysctl_aq_link_speed(pf->supported_speeds, false);

	return sysctl_handle_int(oidp, NULL, supported, req);
}

/*
** Control link advertise speed:
**	Flags:
**	 0x1 - advertise 100 Mb
**	 0x2 - advertise 1G
**	 0x4 - advertise 10G
**	 0x8 - advertise 20G
**	0x10 - advertise 25G
**	0x20 - advertise 40G
**	0x40 - advertise 2.5G
**	0x80 - advertise 5G
**
**	Set to 0 to disable link
*/
int
ixl_sysctl_set_advertise(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	u8 converted_speeds;
	int requested_ls = 0;
	int error = 0;

	/* Read in new mode */
	requested_ls = pf->advertised_speed;
	error = sysctl_handle_int(oidp, &requested_ls, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	if (IXL_PF_IN_RECOVERY_MODE(pf)) {
		device_printf(dev, "Interface is currently in FW recovery mode. "
				"Setting advertise speed not supported\n");
		return (EINVAL);
	}

	/* Error out if bits outside of possible flag range are set */
	if ((requested_ls & ~((u8)0xFF)) != 0) {
		device_printf(dev, "Input advertised speed out of range; "
		    "valid flags are: 0x%02x\n",
		    ixl_convert_sysctl_aq_link_speed(pf->supported_speeds, false));
		return (EINVAL);
	}

	/* Check if adapter supports input value */
	converted_speeds = ixl_convert_sysctl_aq_link_speed((u8)requested_ls, true);
	if ((converted_speeds | pf->supported_speeds) != pf->supported_speeds) {
		device_printf(dev, "Invalid advertised speed; "
		    "valid flags are: 0x%02x\n",
		    ixl_convert_sysctl_aq_link_speed(pf->supported_speeds, false));
		return (EINVAL);
	}

	error = ixl_set_advertised_speeds(pf, requested_ls, false);
	if (error)
		return (error);

	pf->advertised_speed = requested_ls;
	ixl_update_link_status(pf);
	return (0);
}

/*
 * Input: bitmap of enum i40e_aq_link_speed
 */
u64
ixl_max_aq_speed_to_value(u8 link_speeds)
{
	if (link_speeds & I40E_LINK_SPEED_40GB)
		return IF_Gbps(40);
	if (link_speeds & I40E_LINK_SPEED_25GB)
		return IF_Gbps(25);
	if (link_speeds & I40E_LINK_SPEED_20GB)
		return IF_Gbps(20);
	if (link_speeds & I40E_LINK_SPEED_10GB)
		return IF_Gbps(10);
	if (link_speeds & I40E_LINK_SPEED_5GB)
		return IF_Gbps(5);
	if (link_speeds & I40E_LINK_SPEED_2_5GB)
		return IF_Mbps(2500);
	if (link_speeds & I40E_LINK_SPEED_1GB)
		return IF_Gbps(1);
	if (link_speeds & I40E_LINK_SPEED_100MB)
		return IF_Mbps(100);
	else
		/* Minimum supported link speed */
		return IF_Mbps(100);
}

/*
** Get the width and transaction speed of
** the bus this adapter is plugged into.
*/
void
ixl_get_bus_info(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
        u16 link;
        u32 offset, num_ports;
	u64 max_speed;

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
            (hw->bus.width == i40e_bus_width_pcie_x2) ? "Width x2" :
            (hw->bus.width == i40e_bus_width_pcie_x1) ? "Width x1" :
            ("Unknown"));

	/*
	 * If adapter is in slot with maximum supported speed,
	 * no warning message needs to be printed out.
	 */
	if (hw->bus.speed >= i40e_bus_speed_8000
	    && hw->bus.width >= i40e_bus_width_pcie_x8)
		return;

	num_ports = bitcount32(hw->func_caps.valid_functions);
	max_speed = ixl_max_aq_speed_to_value(pf->supported_speeds) / 1000000;

	if ((num_ports * max_speed) > hw->bus.speed * hw->bus.width) {
                device_printf(dev, "PCI-Express bandwidth available"
                    " for this device may be insufficient for"
                    " optimal performance.\n");
                device_printf(dev, "Please move the device to a different"
		    " PCI-e link with more lanes and/or higher"
		    " transfer rate.\n");
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

	return (0);
}

void
ixl_print_nvm_cmd(device_t dev, struct i40e_nvm_access *nvma)
{
	u8 nvma_ptr = nvma->config & 0xFF;
	u8 nvma_flags = (nvma->config & 0xF00) >> 8;
	const char * cmd_str;

	switch (nvma->command) {
	case I40E_NVM_READ:
		if (nvma_ptr == 0xF && nvma_flags == 0xF &&
		    nvma->offset == 0 && nvma->data_size == 1) {
			device_printf(dev, "NVMUPD: Get Driver Status Command\n");
			return;
		}
		cmd_str = "READ ";
		break;
	case I40E_NVM_WRITE:
		cmd_str = "WRITE";
		break;
	default:
		device_printf(dev, "NVMUPD: unknown command: 0x%08x\n", nvma->command);
		return;
	}
	device_printf(dev,
	    "NVMUPD: cmd: %s ptr: 0x%02x flags: 0x%01x offset: 0x%08x data_s: 0x%08x\n",
	    cmd_str, nvma_ptr, nvma_flags, nvma->offset, nvma->data_size);
}

int
ixl_handle_nvmupd_cmd(struct ixl_pf *pf, struct ifdrv *ifd)
{
	struct i40e_hw *hw = &pf->hw;
	struct i40e_nvm_access *nvma;
	device_t dev = pf->dev;
	enum i40e_status_code status = 0;
	size_t nvma_size, ifd_len, exp_len;
	int err, perrno;

	DEBUGFUNC("ixl_handle_nvmupd_cmd");

	/* Sanity checks */
	nvma_size = sizeof(struct i40e_nvm_access);
	ifd_len = ifd->ifd_len;

	if (ifd_len < nvma_size ||
	    ifd->ifd_data == NULL) {
		device_printf(dev, "%s: incorrect ifdrv length or data pointer\n",
		    __func__);
		device_printf(dev, "%s: ifdrv length: %zu, sizeof(struct i40e_nvm_access): %zu\n",
		    __func__, ifd_len, nvma_size);
		device_printf(dev, "%s: data pointer: %p\n", __func__,
		    ifd->ifd_data);
		return (EINVAL);
	}

	nvma = malloc(ifd_len, M_IXL, M_WAITOK);
	err = copyin(ifd->ifd_data, nvma, ifd_len);
	if (err) {
		device_printf(dev, "%s: Cannot get request from user space\n",
		    __func__);
		free(nvma, M_IXL);
		return (err);
	}

	if (pf->dbg_mask & IXL_DBG_NVMUPD)
		ixl_print_nvm_cmd(dev, nvma);

	if (pf->state & IXL_PF_STATE_ADAPTER_RESETTING) {
		int count = 0;
		while (count++ < 100) {
			i40e_msec_delay(100);
			if (!(pf->state & IXL_PF_STATE_ADAPTER_RESETTING))
				break;
		}
	}

	if (pf->state & IXL_PF_STATE_ADAPTER_RESETTING) {
		device_printf(dev,
		    "%s: timeout waiting for EMP reset to finish\n",
		    __func__);
		free(nvma, M_IXL);
		return (-EBUSY);
	}

	if (nvma->data_size < 1 || nvma->data_size > 4096) {
		device_printf(dev,
		    "%s: invalid request, data size not in supported range\n",
		    __func__);
		free(nvma, M_IXL);
		return (EINVAL);
	}

	/*
	 * Older versions of the NVM update tool don't set ifd_len to the size
	 * of the entire buffer passed to the ioctl. Check the data_size field
	 * in the contained i40e_nvm_access struct and ensure everything is
	 * copied in from userspace.
	 */
	exp_len = nvma_size + nvma->data_size - 1; /* One byte is kept in struct */

	if (ifd_len < exp_len) {
		ifd_len = exp_len;
		nvma = realloc(nvma, ifd_len, M_IXL, M_WAITOK);
		err = copyin(ifd->ifd_data, nvma, ifd_len);
		if (err) {
			device_printf(dev, "%s: Cannot get request from user space\n",
					__func__);
			free(nvma, M_IXL);
			return (err);
		}
	}

	// TODO: Might need a different lock here
	// IXL_PF_LOCK(pf);
	status = i40e_nvmupd_command(hw, nvma, nvma->data, &perrno);
	// IXL_PF_UNLOCK(pf);

	err = copyout(nvma, ifd->ifd_data, ifd_len);
	free(nvma, M_IXL);
	if (err) {
		device_printf(dev, "%s: Cannot return data to user space\n",
				__func__);
		return (err);
	}

	/* Let the nvmupdate report errors, show them only when debug is enabled */
	if (status != 0 && (pf->dbg_mask & IXL_DBG_NVMUPD) != 0)
		device_printf(dev, "i40e_nvmupd_command status %s, perrno %d\n",
		    i40e_stat_str(hw, status), perrno);

	/*
	 * -EPERM is actually ERESTART, which the kernel interprets as it needing
	 * to run this ioctl again. So use -EACCES for -EPERM instead.
	 */
	if (perrno == -EPERM)
		return (-EACCES);
	else
		return (perrno);
}

int
ixl_find_i2c_interface(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	bool i2c_en, port_matched;
	u32 reg;

	for (int i = 0; i < 4; i++) {
		reg = rd32(hw, I40E_GLGEN_MDIO_I2C_SEL(i));
		i2c_en = (reg & I40E_GLGEN_MDIO_I2C_SEL_MDIO_I2C_SEL_MASK);
		port_matched = ((reg & I40E_GLGEN_MDIO_I2C_SEL_PHY_PORT_NUM_MASK)
		    >> I40E_GLGEN_MDIO_I2C_SEL_PHY_PORT_NUM_SHIFT)
		    & BIT(hw->port);
		if (i2c_en && port_matched)
			return (i);
	}

	return (-1);
}

static char *
ixl_phy_type_string(u32 bit_pos, bool ext)
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
		"SFP+ Active DA",
		"QSFP+ Active DA",
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
	static char * ext_phy_types_str[8] = {
		"25GBASE-KR",
		"25GBASE-CR",
		"25GBASE-SR",
		"25GBASE-LR",
		"25GBASE-AOC",
		"25GBASE-ACC",
		"2.5GBASE-T",
		"5GBASE-T"
	};

	if (ext && bit_pos > 7) return "Invalid_Ext";
	if (bit_pos > 31) return "Invalid";

	return (ext) ? ext_phy_types_str[bit_pos] : phy_types_str[bit_pos];
}

/* TODO: ERJ: I don't this is necessary anymore. */
int
ixl_aq_get_link_status(struct ixl_pf *pf, struct i40e_aqc_get_link_status *link_status)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_aq_desc desc;
	enum i40e_status_code status;

	struct i40e_aqc_get_link_status *aq_link_status =
		(struct i40e_aqc_get_link_status *)&desc.params.raw;

	i40e_fill_default_direct_cmd_desc(&desc, i40e_aqc_opc_get_link_status);
	link_status->command_flags = CPU_TO_LE16(I40E_AQ_LSE_ENABLE);
	status = i40e_asq_send_command(hw, &desc, NULL, 0, NULL);
	if (status) {
		device_printf(dev,
		    "%s: i40e_aqc_opc_get_link_status status %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		return (EIO);
	}

	bcopy(aq_link_status, link_status, sizeof(struct i40e_aqc_get_link_status));
	return (0);
}

static char *
ixl_phy_type_string_ls(u8 val)
{
	if (val >= 0x1F)
		return ixl_phy_type_string(val - 0x1F, true);
	else
		return ixl_phy_type_string(val, false);
}

static int
ixl_sysctl_link_status(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	struct i40e_aqc_get_link_status link_status;
	error = ixl_aq_get_link_status(pf, &link_status);
	if (error) {
		sbuf_delete(buf);
		return (error);
	}

	sbuf_printf(buf, "\n"
	    "PHY Type : 0x%02x<%s>\n"
	    "Speed    : 0x%02x\n"
	    "Link info: 0x%02x\n"
	    "AN info  : 0x%02x\n"
	    "Ext info : 0x%02x\n"
	    "Loopback : 0x%02x\n"
	    "Max Frame: %d\n"
	    "Config   : 0x%02x\n"
	    "Power    : 0x%02x",
	    link_status.phy_type,
	    ixl_phy_type_string_ls(link_status.phy_type),
	    link_status.link_speed,
	    link_status.link_info,
	    link_status.an_info,
	    link_status.ext_info,
	    link_status.loopback,
	    link_status.max_frame_size,
	    link_status.config,
	    link_status.power_desc);

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
	    FALSE, FALSE, &abilities, NULL);
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
				sbuf_printf(buf, "%s,", ixl_phy_type_string(i, false));
		sbuf_printf(buf, ">");
	}

	sbuf_printf(buf, "\nPHY Ext  : %02x",
	    abilities.phy_type_ext);

	if (abilities.phy_type_ext != 0) {
		sbuf_printf(buf, "<");
		for (int i = 0; i < 4; i++)
			if ((1 << i) & abilities.phy_type_ext)
				sbuf_printf(buf, "%s,",
				    ixl_phy_type_string(i, true));
		sbuf_printf(buf, ">");
	}

	sbuf_printf(buf, "\nSpeed    : %02x", abilities.link_speed);
	if (abilities.link_speed != 0) {
		u8 link_speed;
		sbuf_printf(buf, " <");
		for (int i = 0; i < 8; i++) {
			link_speed = (1 << i) & abilities.link_speed;
			if (link_speed)
				sbuf_printf(buf, "%s, ",
				    ixl_link_speed_string(link_speed));
		}
		sbuf_printf(buf, ">");
	}

	sbuf_printf(buf, "\n"
	    "Abilities: %02x\n"
	    "EEE cap  : %04x\n"
	    "EEER reg : %08x\n"
	    "D3 Lpan  : %02x\n"
	    "ID       : %02x %02x %02x %02x\n"
	    "ModType  : %02x %02x %02x\n"
	    "ModType E: %01x\n"
	    "FEC Cfg  : %02x\n"
	    "Ext CC   : %02x",
	    abilities.abilities, abilities.eee_capability,
	    abilities.eeer_val, abilities.d3_lpan,
	    abilities.phy_id[0], abilities.phy_id[1],
	    abilities.phy_id[2], abilities.phy_id[3],
	    abilities.module_type[0], abilities.module_type[1],
	    abilities.module_type[2], (abilities.fec_cfg_curr_mod_ext_info & 0xe0) >> 5,
	    abilities.fec_cfg_curr_mod_ext_info & 0x1F,
	    abilities.ext_comp_code);

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
	device_t dev = pf->dev;
	int error = 0, ftl_len = 0, ftl_counter = 0;

	struct sbuf *buf;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for sysctl output.\n");
		return (ENOMEM);
	}

	sbuf_printf(buf, "\n");

	/* Print MAC filters */
	sbuf_printf(buf, "PF Filters:\n");
	SLIST_FOREACH(f, &vsi->ftl, next)
		ftl_len++;

	if (ftl_len < 1)
		sbuf_printf(buf, "(none)\n");
	else {
		SLIST_FOREACH(f, &vsi->ftl, next) {
			sbuf_printf(buf,
			    MAC_FORMAT ", vlan %4d, flags %#06x",
			    MAC_FORMAT_ARGS(f->macaddr), f->vlan, f->flags);
			/* don't print '\n' for last entry */
			if (++ftl_counter != ftl_len)
				sbuf_printf(buf, "\n");
		}
	}

#ifdef PCI_IOV
	/* TODO: Give each VF its own filter list sysctl */
	struct ixl_vf *vf;
	if (pf->num_vfs > 0) {
		sbuf_printf(buf, "\n\n");
		for (int i = 0; i < pf->num_vfs; i++) {
			vf = &pf->vfs[i];
			if (!(vf->vf_flags & VF_FLAG_ENABLED))
				continue;

			vsi = &vf->vsi;
			ftl_len = 0, ftl_counter = 0;
			sbuf_printf(buf, "VF-%d Filters:\n", vf->vf_num);
			SLIST_FOREACH(f, &vsi->ftl, next)
				ftl_len++;

			if (ftl_len < 1)
				sbuf_printf(buf, "(none)\n");
			else {
				SLIST_FOREACH(f, &vsi->ftl, next) {
					sbuf_printf(buf,
					    MAC_FORMAT ", vlan %4d, flags %#06x\n",
					    MAC_FORMAT_ARGS(f->macaddr), f->vlan, f->flags);
				}
			}
		}
	}
#endif

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
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
const char *
ixl_switch_res_type_string(u8 type)
{
	static const char * ixl_switch_res_type_strings[IXL_SW_RES_SIZE] = {
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

	if (type < IXL_SW_RES_SIZE)
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

enum ixl_sw_seid_offset {
	IXL_SW_SEID_EMP = 1,
	IXL_SW_SEID_MAC_START = 2,
	IXL_SW_SEID_MAC_END = 5,
	IXL_SW_SEID_PF_START = 16,
	IXL_SW_SEID_PF_END = 31,
	IXL_SW_SEID_VF_START = 32,
	IXL_SW_SEID_VF_END = 159,
};

/*
 * Caller must init and delete sbuf; this function will clear and
 * finish it for caller.
 *
 * Note: The SEID argument only applies for elements defined by FW at
 * power-on; these include the EMP, Ports, PFs and VFs.
 */
static char *
ixl_switch_element_string(struct sbuf *s, u8 element_type, u16 seid)
{
	sbuf_clear(s);

	/* If SEID is in certain ranges, then we can infer the
	 * mapping of SEID to switch element.
	 */
	if (seid == IXL_SW_SEID_EMP) {
		sbuf_cat(s, "EMP");
		goto out;
	} else if (seid >= IXL_SW_SEID_MAC_START &&
	    seid <= IXL_SW_SEID_MAC_END) {
		sbuf_printf(s, "MAC  %2d",
		    seid - IXL_SW_SEID_MAC_START);
		goto out;
	} else if (seid >= IXL_SW_SEID_PF_START &&
	    seid <= IXL_SW_SEID_PF_END) {
		sbuf_printf(s, "PF  %3d",
		    seid - IXL_SW_SEID_PF_START);
		goto out;
	} else if (seid >= IXL_SW_SEID_VF_START &&
	    seid <= IXL_SW_SEID_VF_END) {
		sbuf_printf(s, "VF  %3d",
		    seid - IXL_SW_SEID_VF_START);
		goto out;
	}

	switch (element_type) {
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
		sbuf_printf(s, "VSI");
		break;
	default:
		sbuf_cat(s, "?");
		break;
	}

out:
	sbuf_finish(s);
	return sbuf_data(s);
}

static int
ixl_sw_cfg_elem_seid_cmp(const void *a, const void *b)
{
	const struct i40e_aqc_switch_config_element_resp *one, *two;
	one = (const struct i40e_aqc_switch_config_element_resp *)a;
	two = (const struct i40e_aqc_switch_config_element_resp *)b;

	return ((int)one->seid - (int)two->seid);
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

	struct i40e_aqc_switch_config_element_resp *elem;
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

	/* Sort entries by SEID for display */
	qsort(sw_config->element, sw_config->header.num_reported,
	    sizeof(struct i40e_aqc_switch_config_element_resp),
	    &ixl_sw_cfg_elem_seid_cmp);

	sbuf_cat(buf, "\n");
	/* Assuming <= 255 elements in switch */
	sbuf_printf(buf, "# of reported elements: %d\n", sw_config->header.num_reported);
	sbuf_printf(buf, "total # of elements: %d\n", sw_config->header.num_total);
	/* Exclude:
	 * Revision -- all elements are revision 1 for now
	 */
	sbuf_printf(buf,
	    "SEID (  Name  ) |  Up  (  Name  ) | Down (  Name  ) | Conn Type\n"
	    "                |                 |                 | (uplink)\n");
	for (int i = 0; i < sw_config->header.num_reported; i++) {
		elem = &sw_config->element[i];

		// "%4d (%8s) | %8s   %8s   %#8x",
		sbuf_printf(buf, "%4d", elem->seid);
		sbuf_cat(buf, " ");
		sbuf_printf(buf, "(%8s)", ixl_switch_element_string(nmbuf,
		    elem->element_type, elem->seid));
		sbuf_cat(buf, " | ");
		sbuf_printf(buf, "%4d", elem->uplink_seid);
		sbuf_cat(buf, " ");
		sbuf_printf(buf, "(%8s)", ixl_switch_element_string(nmbuf,
		    0, elem->uplink_seid));
		sbuf_cat(buf, " | ");
		sbuf_printf(buf, "%4d", elem->downlink_seid);
		sbuf_cat(buf, " ");
		sbuf_printf(buf, "(%8s)", ixl_switch_element_string(nmbuf,
		    0, elem->downlink_seid));
		sbuf_cat(buf, " | ");
		sbuf_printf(buf, "%8d", elem->connection_type);
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

	bzero(&key_data, sizeof(key_data));

	sbuf_cat(buf, "\n");
	if (hw->mac.type == I40E_MAC_X722) {
		status = i40e_aq_get_rss_key(hw, pf->vsi.vsi_num, &key_data);
		if (status)
			device_printf(dev, "i40e_aq_get_rss_key status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
	} else {
		for (int i = 0; i < IXL_RSS_KEY_SIZE_REG; i++) {
			reg = i40e_read_rx_ctl(hw, I40E_PFQF_HKEY(i));
			bcopy(&reg, ((caddr_t)&key_data) + (i << 2), 4);
		}
	}

	ixl_sbuf_print_bytes(buf, (u8 *)&key_data, sizeof(key_data), 0, true);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

static void
ixl_sbuf_print_bytes(struct sbuf *sb, u8 *buf, int length, int label_offset, bool text)
{
	int i, j, k, width;
	char c;

	if (length < 1 || buf == NULL) return;

	int byte_stride = 16;
	int lines = length / byte_stride;
	int rem = length % byte_stride;
	if (rem > 0)
		lines++;

	for (i = 0; i < lines; i++) {
		width = (rem > 0 && i == lines - 1)
		    ? rem : byte_stride;

		sbuf_printf(sb, "%4d | ", label_offset + i * byte_stride);

		for (j = 0; j < width; j++)
			sbuf_printf(sb, "%02x ", buf[i * byte_stride + j]);

		if (width < byte_stride) {
			for (k = 0; k < (byte_stride - width); k++)
				sbuf_printf(sb, "   ");
		}

		if (!text) {
			sbuf_printf(sb, "\n");
			continue;
		}

		for (j = 0; j < width; j++) {
			c = (char)buf[i * byte_stride + j];
			if (c < 32 || c > 126)
				sbuf_printf(sb, ".");
			else
				sbuf_printf(sb, "%c", c);

			if (j == width - 1)
				sbuf_printf(sb, "\n");
		}
	}
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

	bzero(hlut, sizeof(hlut));
	sbuf_cat(buf, "\n");
	if (hw->mac.type == I40E_MAC_X722) {
		status = i40e_aq_get_rss_lut(hw, pf->vsi.vsi_num, TRUE, hlut, sizeof(hlut));
		if (status)
			device_printf(dev, "i40e_aq_get_rss_lut status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
	} else {
		for (int i = 0; i < hw->func_caps.rss_table_size >> 2; i++) {
			reg = rd32(hw, I40E_PFQF_HLUT(i));
			bcopy(&reg, &hlut[i << 2], 4);
		}
	}
	ixl_sbuf_print_bytes(buf, hlut, 512, 0, false);

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

static int
ixl_sysctl_hena(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	u64 hena;

	hena = (u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(0)) |
	    ((u64)i40e_read_rx_ctl(hw, I40E_PFQF_HENA(1)) << 32);

	return sysctl_handle_long(oidp, NULL, hena, req);
}

/*
 * Sysctl to disable firmware's link management
 *
 * 1 - Disable link management on this port
 * 0 - Re-enable link management
 *
 * On normal NVMs, firmware manages link by default.
 */
static int
ixl_sysctl_fw_link_management(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int requested_mode = -1;
	enum i40e_status_code status = 0;
	int error = 0;

	/* Read in new mode */
	error = sysctl_handle_int(oidp, &requested_mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	/* Check for sane value */
	if (requested_mode < 0 || requested_mode > 1) {
		device_printf(dev, "Valid modes are 0 or 1\n");
		return (EINVAL);
	}

	/* Set new mode */
	status = i40e_aq_set_phy_debug(hw, !!(requested_mode) << 4, NULL);
	if (status) {
		device_printf(dev,
		    "%s: Error setting new phy debug mode %s,"
		    " aq error: %s\n", __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		return (EIO);
	}

	return (0);
}

/*
 * Read some diagnostic data from a (Q)SFP+ module
 *
 *             SFP A2   QSFP Lower Page
 * Temperature 96-97	22-23
 * Vcc         98-99    26-27
 * TX power    102-103  34-35..40-41
 * RX power    104-105  50-51..56-57
 */
static int
ixl_sysctl_read_i2c_diag_data(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	struct sbuf *sbuf;
	int error = 0;
	u8 output;

	if (req->oldptr == NULL) {
		error = SYSCTL_OUT(req, 0, 128);
		return (0);
	}

	error = pf->read_i2c_byte(pf, 0, 0xA0, &output);
	if (error) {
		device_printf(dev, "Error reading from i2c\n");
		return (error);
	}

	/* 0x3 for SFP; 0xD/0x11 for QSFP+/QSFP28 */
	if (output == 0x3) {
		/*
		 * Check for:
		 * - Internally calibrated data
		 * - Diagnostic monitoring is implemented
		 */
		pf->read_i2c_byte(pf, 92, 0xA0, &output);
		if (!(output & 0x60)) {
			device_printf(dev, "Module doesn't support diagnostics: %02X\n", output);
			return (0);
		}

		sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

		for (u8 offset = 96; offset < 100; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA2, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
		for (u8 offset = 102; offset < 106; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA2, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
	} else if (output == 0xD || output == 0x11) {
		/*
		 * QSFP+ modules are always internally calibrated, and must indicate
		 * what types of diagnostic monitoring are implemented
		 */
		sbuf = sbuf_new_for_sysctl(NULL, NULL, 128, req);

		for (u8 offset = 22; offset < 24; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA0, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
		for (u8 offset = 26; offset < 28; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA0, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
		/* Read the data from the first lane */
		for (u8 offset = 34; offset < 36; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA0, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
		for (u8 offset = 50; offset < 52; offset++) {
			pf->read_i2c_byte(pf, offset, 0xA0, &output);
			sbuf_printf(sbuf, "%02X ", output);
		}
	} else {
		device_printf(dev, "Module is not SFP/SFP+/SFP28/QSFP+ (%02X)\n", output);
		return (0);
	}

	sbuf_finish(sbuf);
	sbuf_delete(sbuf);

	return (0);
}

/*
 * Sysctl to read a byte from I2C bus.
 *
 * Input: 32-bit value:
 * 	bits 0-7:   device address (0xA0 or 0xA2)
 * 	bits 8-15:  offset (0-255)
 *	bits 16-31: unused
 * Output: 8-bit value read
 */
static int
ixl_sysctl_read_i2c_byte(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	int input = -1, error = 0;
	u8 dev_addr, offset, output;

	/* Read in I2C read parameters */
	error = sysctl_handle_int(oidp, &input, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	/* Validate device address */
	dev_addr = input & 0xFF;
	if (dev_addr != 0xA0 && dev_addr != 0xA2) {
		return (EINVAL);
	}
	offset = (input >> 8) & 0xFF;

	error = pf->read_i2c_byte(pf, offset, dev_addr, &output);
	if (error)
		return (error);

	device_printf(dev, "%02X\n", output);
	return (0);
}

/*
 * Sysctl to write a byte to the I2C bus.
 *
 * Input: 32-bit value:
 * 	bits 0-7:   device address (0xA0 or 0xA2)
 * 	bits 8-15:  offset (0-255)
 *	bits 16-23: value to write
 *	bits 24-31: unused
 * Output: 8-bit value written
 */
static int
ixl_sysctl_write_i2c_byte(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	device_t dev = pf->dev;
	int input = -1, error = 0;
	u8 dev_addr, offset, value;

	/* Read in I2C write parameters */
	error = sysctl_handle_int(oidp, &input, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);
	/* Validate device address */
	dev_addr = input & 0xFF;
	if (dev_addr != 0xA0 && dev_addr != 0xA2) {
		return (EINVAL);
	}
	offset = (input >> 8) & 0xFF;
	value = (input >> 16) & 0xFF;

	error = pf->write_i2c_byte(pf, offset, dev_addr, value);
	if (error)
		return (error);

	device_printf(dev, "%02X written\n", value);
	return (0);
}

static int
ixl_get_fec_config(struct ixl_pf *pf, struct i40e_aq_get_phy_abilities_resp *abilities,
    u8 bit_pos, int *is_set)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;
	enum i40e_status_code status;

	if (IXL_PF_IN_RECOVERY_MODE(pf))
		return (EIO);

	status = i40e_aq_get_phy_capabilities(hw,
	    FALSE, FALSE, abilities, NULL);
	if (status) {
		device_printf(dev,
		    "%s: i40e_aq_get_phy_capabilities() status %s, aq error %s\n",
		    __func__, i40e_stat_str(hw, status),
		    i40e_aq_str(hw, hw->aq.asq_last_status));
		return (EIO);
	}

	*is_set = !!(abilities->fec_cfg_curr_mod_ext_info & bit_pos);
	return (0);
}

static int
ixl_set_fec_config(struct ixl_pf *pf, struct i40e_aq_get_phy_abilities_resp *abilities,
    u8 bit_pos, int set)
{
	device_t dev = pf->dev;
	struct i40e_hw *hw = &pf->hw;
	struct i40e_aq_set_phy_config config;
	enum i40e_status_code status;

	/* Set new PHY config */
	memset(&config, 0, sizeof(config));
	config.fec_config = abilities->fec_cfg_curr_mod_ext_info & ~(bit_pos);
	if (set)
		config.fec_config |= bit_pos;
	if (config.fec_config != abilities->fec_cfg_curr_mod_ext_info) {
		config.abilities |= I40E_AQ_PHY_ENABLE_ATOMIC_LINK;
		config.phy_type = abilities->phy_type;
		config.phy_type_ext = abilities->phy_type_ext;
		config.link_speed = abilities->link_speed;
		config.eee_capability = abilities->eee_capability;
		config.eeer = abilities->eeer_val;
		config.low_power_ctrl = abilities->d3_lpan;
		status = i40e_aq_set_phy_config(hw, &config, NULL);

		if (status) {
			device_printf(dev,
			    "%s: i40e_aq_set_phy_config() status %s, aq error %s\n",
			    __func__, i40e_stat_str(hw, status),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
			return (EIO);
		}
	}

	return (0);
}

static int
ixl_sysctl_fec_fc_ability(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int mode, error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;
	error = ixl_get_fec_config(pf, &abilities, I40E_AQ_ENABLE_FEC_KR, &mode);
	if (error)
		return (error);
	/* Read in new mode */
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixl_set_fec_config(pf, &abilities, I40E_AQ_SET_FEC_ABILITY_KR, !!(mode));
}

static int
ixl_sysctl_fec_rs_ability(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int mode, error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;
	error = ixl_get_fec_config(pf, &abilities, I40E_AQ_ENABLE_FEC_RS, &mode);
	if (error)
		return (error);
	/* Read in new mode */
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixl_set_fec_config(pf, &abilities, I40E_AQ_SET_FEC_ABILITY_RS, !!(mode));
}

static int
ixl_sysctl_fec_fc_request(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int mode, error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;
	error = ixl_get_fec_config(pf, &abilities, I40E_AQ_REQUEST_FEC_KR, &mode);
	if (error)
		return (error);
	/* Read in new mode */
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixl_set_fec_config(pf, &abilities, I40E_AQ_SET_FEC_REQUEST_KR, !!(mode));
}

static int
ixl_sysctl_fec_rs_request(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int mode, error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;
	error = ixl_get_fec_config(pf, &abilities, I40E_AQ_REQUEST_FEC_RS, &mode);
	if (error)
		return (error);
	/* Read in new mode */
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixl_set_fec_config(pf, &abilities, I40E_AQ_SET_FEC_REQUEST_RS, !!(mode));
}

static int
ixl_sysctl_fec_auto_enable(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int mode, error = 0;

	struct i40e_aq_get_phy_abilities_resp abilities;
	error = ixl_get_fec_config(pf, &abilities, I40E_AQ_ENABLE_FEC_AUTO, &mode);
	if (error)
		return (error);
	/* Read in new mode */
	error = sysctl_handle_int(oidp, &mode, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	return ixl_set_fec_config(pf, &abilities, I40E_AQ_SET_FEC_AUTO, !!(mode));
}

static int
ixl_sysctl_dump_debug_data(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;
	enum i40e_status_code status;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	u8 *final_buff;
	/* This amount is only necessary if reading the entire cluster into memory */
#define IXL_FINAL_BUFF_SIZE	(1280 * 1024)
	final_buff = malloc(IXL_FINAL_BUFF_SIZE, M_DEVBUF, M_NOWAIT);
	if (final_buff == NULL) {
		device_printf(dev, "Could not allocate memory for output.\n");
		goto out;
	}
	int final_buff_len = 0;

	u8 cluster_id = 1;
	bool more = true;

	u8 dump_buf[4096];
	u16 curr_buff_size = 4096;
	u8 curr_next_table = 0;
	u32 curr_next_index = 0;

	u16 ret_buff_size;
	u8 ret_next_table;
	u32 ret_next_index;

	sbuf_cat(buf, "\n");

	while (more) {
		status = i40e_aq_debug_dump(hw, cluster_id, curr_next_table, curr_next_index, curr_buff_size,
		    dump_buf, &ret_buff_size, &ret_next_table, &ret_next_index, NULL);
		if (status) {
			device_printf(dev, "i40e_aq_debug_dump status %s, error %s\n",
			    i40e_stat_str(hw, status), i40e_aq_str(hw, hw->aq.asq_last_status));
			goto free_out;
		}

		/* copy info out of temp buffer */
		bcopy(dump_buf, (caddr_t)final_buff + final_buff_len, ret_buff_size);
		final_buff_len += ret_buff_size;

		if (ret_next_table != curr_next_table) {
			/* We're done with the current table; we can dump out read data. */
			sbuf_printf(buf, "%d:", curr_next_table);
			int bytes_printed = 0;
			while (bytes_printed <= final_buff_len) {
				sbuf_printf(buf, "%16D", ((caddr_t)final_buff + bytes_printed), "");
				bytes_printed += 16;
			}
				sbuf_cat(buf, "\n");

			/* The entire cluster has been read; we're finished */
			if (ret_next_table == 0xFF)
				break;

			/* Otherwise clear the output buffer and continue reading */
			bzero(final_buff, IXL_FINAL_BUFF_SIZE);
			final_buff_len = 0;
		}

		if (ret_next_index == 0xFFFFFFFF)
			ret_next_index = 0;

		bzero(dump_buf, sizeof(dump_buf));
		curr_next_table = ret_next_table;
		curr_next_index = ret_next_index;
	}

free_out:
	free(final_buff, M_DEVBUF);
out:
	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}

static int
ixl_start_fw_lldp(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	enum i40e_status_code status;

	status = i40e_aq_start_lldp(hw, false, NULL);
	if (status != I40E_SUCCESS) {
		switch (hw->aq.asq_last_status) {
		case I40E_AQ_RC_EEXIST:
			device_printf(pf->dev,
			    "FW LLDP agent is already running\n");
			break;
		case I40E_AQ_RC_EPERM:
			device_printf(pf->dev,
			    "Device configuration forbids SW from starting "
			    "the LLDP agent. Set the \"LLDP Agent\" UEFI HII "
			    "attribute to \"Enabled\" to use this sysctl\n");
			return (EINVAL);
		default:
			device_printf(pf->dev,
			    "Starting FW LLDP agent failed: error: %s, %s\n",
			    i40e_stat_str(hw, status),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
			return (EINVAL);
		}
	}

	atomic_clear_32(&pf->state, IXL_PF_STATE_FW_LLDP_DISABLED);
	return (0);
}

static int
ixl_stop_fw_lldp(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	enum i40e_status_code status;

	if (hw->func_caps.npar_enable != 0) {
		device_printf(dev,
		    "Disabling FW LLDP agent is not supported on this device\n");
		return (EINVAL);
	}

	if ((hw->flags & I40E_HW_FLAG_FW_LLDP_STOPPABLE) == 0) {
		device_printf(dev,
		    "Disabling FW LLDP agent is not supported in this FW version. Please update FW to enable this feature.\n");
		return (EINVAL);
	}

	status = i40e_aq_stop_lldp(hw, true, false, NULL);
	if (status != I40E_SUCCESS) {
		if (hw->aq.asq_last_status != I40E_AQ_RC_EPERM) {
			device_printf(dev,
			    "Disabling FW LLDP agent failed: error: %s, %s\n",
			    i40e_stat_str(hw, status),
			    i40e_aq_str(hw, hw->aq.asq_last_status));
			return (EINVAL);
		}

		device_printf(dev, "FW LLDP agent is already stopped\n");
	}

#ifndef EXTERNAL_RELEASE
	/* Let the FW set default DCB configuration on link UP as described in DCR 307.1 */
#endif
	i40e_aq_set_dcb_parameters(hw, true, NULL);
	atomic_set_32(&pf->state, IXL_PF_STATE_FW_LLDP_DISABLED);
	return (0);
}

static int
ixl_sysctl_fw_lldp(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int state, new_state, error = 0;

	state = new_state = ((pf->state & IXL_PF_STATE_FW_LLDP_DISABLED) == 0);

	/* Read in new mode */
	error = sysctl_handle_int(oidp, &new_state, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	/* Already in requested state */
	if (new_state == state)
		return (error);

	if (new_state == 0)
		return ixl_stop_fw_lldp(pf);

	return ixl_start_fw_lldp(pf);
}

static int
ixl_sysctl_eee_enable(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf         *pf = (struct ixl_pf *)arg1;
	int                   state, new_state;
	int                   sysctl_handle_status = 0;
	enum i40e_status_code cmd_status;

	/* Init states' values */
	state = new_state = (!!(pf->state & IXL_PF_STATE_EEE_ENABLED));

	/* Get requested mode */
	sysctl_handle_status = sysctl_handle_int(oidp, &new_state, 0, req);
	if ((sysctl_handle_status) || (req->newptr == NULL))
		return (sysctl_handle_status);

	/* Check if state has changed */
	if (new_state == state)
		return (0);

	/* Set new state */
	cmd_status = i40e_enable_eee(&pf->hw, (bool)(!!new_state));

	/* Save new state or report error */
	if (!cmd_status) {
		if (new_state == 0)
			atomic_clear_32(&pf->state, IXL_PF_STATE_EEE_ENABLED);
		else
			atomic_set_32(&pf->state, IXL_PF_STATE_EEE_ENABLED);
	} else if (cmd_status == I40E_ERR_CONFIG)
		return (EPERM);
	else
		return (EIO);

	return (0);
}

int
ixl_attach_get_link_status(struct ixl_pf *pf)
{
	struct i40e_hw *hw = &pf->hw;
	device_t dev = pf->dev;
	int error = 0;

	if (((hw->aq.fw_maj_ver == 4) && (hw->aq.fw_min_ver < 33)) ||
	    (hw->aq.fw_maj_ver < 4)) {
		i40e_msec_delay(75);
		error = i40e_aq_set_link_restart_an(hw, TRUE, NULL);
		if (error) {
			device_printf(dev, "link restart failed, aq_err=%d\n",
			    pf->hw.aq.asq_last_status);
			return error;
		}
	}

	/* Determine link state */
	hw->phy.get_link_info = TRUE;
	i40e_get_link_status(hw, &pf->link_up);
	return (0);
}

static int
ixl_sysctl_do_pf_reset(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	int requested = 0, error = 0;

	/* Read in new mode */
	error = sysctl_handle_int(oidp, &requested, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	/* Initiate the PF reset later in the admin task */
	atomic_set_32(&pf->state, IXL_PF_STATE_PF_RESET_REQ);

	return (error);
}

static int
ixl_sysctl_do_core_reset(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	int requested = 0, error = 0;

	/* Read in new mode */
	error = sysctl_handle_int(oidp, &requested, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	wr32(hw, I40E_GLGEN_RTRIG, I40E_GLGEN_RTRIG_CORER_MASK);

	return (error);
}

static int
ixl_sysctl_do_global_reset(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct i40e_hw *hw = &pf->hw;
	int requested = 0, error = 0;

	/* Read in new mode */
	error = sysctl_handle_int(oidp, &requested, 0, req);
	if ((error) || (req->newptr == NULL))
		return (error);

	wr32(hw, I40E_GLGEN_RTRIG, I40E_GLGEN_RTRIG_GLOBR_MASK);

	return (error);
}

/*
 * Print out mapping of TX queue indexes and Rx queue indexes
 * to MSI-X vectors.
 */
static int
ixl_sysctl_queue_interrupt_table(SYSCTL_HANDLER_ARGS)
{
	struct ixl_pf *pf = (struct ixl_pf *)arg1;
	struct ixl_vsi *vsi = &pf->vsi;
	device_t dev = pf->dev;
	struct sbuf *buf;
	int error = 0;

	struct ixl_rx_queue *rx_que = vsi->rx_queues;
	struct ixl_tx_queue *tx_que = vsi->tx_queues;

	buf = sbuf_new_for_sysctl(NULL, NULL, 128, req);
	if (!buf) {
		device_printf(dev, "Could not allocate sbuf for output.\n");
		return (ENOMEM);
	}

	sbuf_cat(buf, "\n");
	for (int i = 0; i < vsi->num_rx_queues; i++) {
		rx_que = &vsi->rx_queues[i];
		sbuf_printf(buf, "(rxq %3d): %d\n", i, rx_que->msix);
	}
	for (int i = 0; i < vsi->num_tx_queues; i++) {
		tx_que = &vsi->tx_queues[i];
		sbuf_printf(buf, "(txq %3d): %d\n", i, tx_que->msix);
	}

	error = sbuf_finish(buf);
	if (error)
		device_printf(dev, "Error finishing sbuf: %d\n", error);
	sbuf_delete(buf);

	return (error);
}
