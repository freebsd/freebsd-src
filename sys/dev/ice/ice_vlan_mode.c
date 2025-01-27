/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2024, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#include "ice_common.h"

#include "ice_ddp_common.h"
/**
 * ice_pkg_get_supported_vlan_mode - chk if DDP supports Double VLAN mode (DVM)
 * @hw: pointer to the HW struct
 * @dvm: output variable to determine if DDP supports DVM(true) or SVM(false)
 */
static int
ice_pkg_get_supported_vlan_mode(struct ice_hw *hw, bool *dvm)
{
	u16 meta_init_size = sizeof(struct ice_meta_init_section);
	struct ice_meta_init_section *sect;
	struct ice_buf_build *bld;
	int status;

	/* if anything fails, we assume there is no DVM support */
	*dvm = false;

	bld = ice_pkg_buf_alloc_single_section(hw,
					       ICE_SID_RXPARSER_METADATA_INIT,
					       meta_init_size, (void **)&sect);
	if (!bld)
		return ICE_ERR_NO_MEMORY;

	/* only need to read a single section */
	sect->count = CPU_TO_LE16(1);
	sect->offset = CPU_TO_LE16(ICE_META_VLAN_MODE_ENTRY);

	status = ice_aq_upload_section(hw,
				       (struct ice_buf_hdr *)ice_pkg_buf(bld),
				       ICE_PKG_BUF_SIZE, NULL);
	if (!status) {
		ice_declare_bitmap(entry, ICE_META_INIT_BITS);
		u32 arr[ICE_META_INIT_DW_CNT];
		u16 i;

		/* convert to host bitmap format */
		for (i = 0; i < ICE_META_INIT_DW_CNT; i++)
			arr[i] = LE32_TO_CPU(sect->entry[0].bm[i]);

		ice_bitmap_from_array32(entry, arr, (u16)ICE_META_INIT_BITS);

		/* check if DVM is supported */
		*dvm = ice_is_bit_set(entry, ICE_META_VLAN_MODE_BIT);
	}

	ice_pkg_buf_free(hw, bld);

	return status;
}

/**
 * ice_aq_get_vlan_mode - get the VLAN mode of the device
 * @hw: pointer to the HW structure
 * @get_params: structure FW fills in based on the current VLAN mode config
 *
 * Get VLAN Mode Parameters (0x020D)
 */
static int
ice_aq_get_vlan_mode(struct ice_hw *hw,
		     struct ice_aqc_get_vlan_mode *get_params)
{
	struct ice_aq_desc desc;

	if (!get_params)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_get_vlan_mode_parameters);

	return ice_aq_send_cmd(hw, &desc, get_params, sizeof(*get_params),
			       NULL);
}

/**
 * ice_aq_is_dvm_ena - query FW to check if double VLAN mode is enabled
 * @hw: pointer to the HW structure
 *
 * Returns true if the hardware/firmware is configured in double VLAN mode,
 * else return false signaling that the hardware/firmware is configured in
 * single VLAN mode.
 *
 * Also, return false if this call fails for any reason (i.e. firmware doesn't
 * support this AQ call).
 */
static bool ice_aq_is_dvm_ena(struct ice_hw *hw)
{
	struct ice_aqc_get_vlan_mode get_params = { 0 };
	int status;

	status = ice_aq_get_vlan_mode(hw, &get_params);
	if (status) {
		ice_debug(hw, ICE_DBG_AQ, "Failed to get VLAN mode, status %d\n",
			  status);
		return false;
	}

	return (get_params.vlan_mode & ICE_AQ_VLAN_MODE_DVM_ENA);
}

/**
 * ice_is_dvm_ena - check if double VLAN mode is enabled
 * @hw: pointer to the HW structure
 *
 * The device is configured in single or double VLAN mode on initialization and
 * this cannot be dynamically changed during runtime. Based on this there is no
 * need to make an AQ call every time the driver needs to know the VLAN mode.
 * Instead, use the cached VLAN mode.
 */
bool ice_is_dvm_ena(struct ice_hw *hw)
{
	return hw->dvm_ena;
}

/**
 * ice_cache_vlan_mode - cache VLAN mode after DDP is downloaded
 * @hw: pointer to the HW structure
 *
 * This is only called after downloading the DDP and after the global
 * configuration lock has been released because all ports on a device need to
 * cache the VLAN mode.
 */
static void ice_cache_vlan_mode(struct ice_hw *hw)
{
	hw->dvm_ena = ice_aq_is_dvm_ena(hw) ? true : false;
}

/**
 * ice_pkg_supports_dvm - find out if DDP supports DVM
 * @hw: pointer to the HW structure
 */
static bool ice_pkg_supports_dvm(struct ice_hw *hw)
{
	int status;
	bool pkg_supports_dvm;

	status = ice_pkg_get_supported_vlan_mode(hw, &pkg_supports_dvm);
	if (status) {
		ice_debug(hw, ICE_DBG_PKG, "Failed to get supported VLAN mode, status %d\n",
			  status);
		return false;
	}

	return pkg_supports_dvm;
}

/**
 * ice_fw_supports_dvm - find out if FW supports DVM
 * @hw: pointer to the HW structure
 */
static bool ice_fw_supports_dvm(struct ice_hw *hw)
{
	struct ice_aqc_get_vlan_mode get_vlan_mode = { 0 };
	int status;

	/* If firmware returns success, then it supports DVM, else it only
	 * supports SVM
	 */
	status = ice_aq_get_vlan_mode(hw, &get_vlan_mode);
	if (status) {
		ice_debug(hw, ICE_DBG_NVM, "Failed to get VLAN mode, status %d\n",
			  status);
		return false;
	}

	return true;
}

/**
 * ice_is_dvm_supported - check if Double VLAN Mode is supported
 * @hw: pointer to the hardware structure
 *
 * Returns true if Double VLAN Mode (DVM) is supported and false if only Single
 * VLAN Mode (SVM) is supported. In order for DVM to be supported the DDP and
 * firmware must support it, otherwise only SVM is supported. This function
 * should only be called while the global config lock is held and after the
 * package has been successfully downloaded.
 */
static bool ice_is_dvm_supported(struct ice_hw *hw)
{
	if (!ice_pkg_supports_dvm(hw)) {
		ice_debug(hw, ICE_DBG_PKG, "DDP doesn't support DVM\n");
		return false;
	}

	if (!ice_fw_supports_dvm(hw)) {
		ice_debug(hw, ICE_DBG_PKG, "FW doesn't support DVM\n");
		return false;
	}

	return true;
}

/**
 * ice_aq_set_vlan_mode - set the VLAN mode of the device
 * @hw: pointer to the HW structure
 * @set_params: requested VLAN mode configuration
 *
 * Set VLAN Mode Parameters (0x020C)
 */
static int
ice_aq_set_vlan_mode(struct ice_hw *hw,
		     struct ice_aqc_set_vlan_mode *set_params)
{
	u8 rdma_packet, mng_vlan_prot_id;
	struct ice_aq_desc desc;

	if (!set_params)
		return ICE_ERR_PARAM;

	if (set_params->l2tag_prio_tagging > ICE_AQ_VLAN_PRIO_TAG_MAX)
		return ICE_ERR_PARAM;

	rdma_packet = set_params->rdma_packet;
	if (rdma_packet != ICE_AQ_SVM_VLAN_RDMA_PKT_FLAG_SETTING &&
	    rdma_packet != ICE_AQ_DVM_VLAN_RDMA_PKT_FLAG_SETTING)
		return ICE_ERR_PARAM;

	mng_vlan_prot_id = set_params->mng_vlan_prot_id;
	if (mng_vlan_prot_id != ICE_AQ_VLAN_MNG_PROTOCOL_ID_OUTER &&
	    mng_vlan_prot_id != ICE_AQ_VLAN_MNG_PROTOCOL_ID_INNER)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_set_vlan_mode_parameters);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	return ice_aq_send_cmd(hw, &desc, set_params, sizeof(*set_params),
			       NULL);
}

/**
 * ice_set_svm - set single VLAN mode
 * @hw: pointer to the HW structure
 */
static int ice_set_svm(struct ice_hw *hw)
{
	struct ice_aqc_set_vlan_mode *set_params;
	int status;

	status = ice_aq_set_port_params(hw->port_info, 0, false, false, false, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to set port parameters for single VLAN mode\n");
		return status;
	}

	set_params = (struct ice_aqc_set_vlan_mode *)
		ice_malloc(hw, sizeof(*set_params));
	if (!set_params)
		return ICE_ERR_NO_MEMORY;

	/* default configuration for SVM configurations */
	set_params->l2tag_prio_tagging = ICE_AQ_VLAN_PRIO_TAG_INNER_CTAG;
	set_params->rdma_packet = ICE_AQ_SVM_VLAN_RDMA_PKT_FLAG_SETTING;
	set_params->mng_vlan_prot_id = ICE_AQ_VLAN_MNG_PROTOCOL_ID_INNER;

	status = ice_aq_set_vlan_mode(hw, set_params);
	if (status)
		ice_debug(hw, ICE_DBG_INIT, "Failed to configure port in single VLAN mode\n");

	ice_free(hw, set_params);
	return status;
}

/**
 * ice_set_vlan_mode
 * @hw: pointer to the HW structure
 */
int ice_set_vlan_mode(struct ice_hw *hw)
{
	if (!ice_is_dvm_supported(hw))
		return 0;

	return ice_set_svm(hw);
}

/**
 * ice_post_pkg_dwnld_vlan_mode_cfg - configure VLAN mode after DDP download
 * @hw: pointer to the HW structure
 *
 * This function is meant to configure any VLAN mode specific functionality
 * after the global configuration lock has been released and the DDP has been
 * downloaded.
 *
 * Since only one PF downloads the DDP and configures the VLAN mode there needs
 * to be a way to configure the other PFs after the DDP has been downloaded and
 * the global configuration lock has been released. All such code should go in
 * this function.
 */
void ice_post_pkg_dwnld_vlan_mode_cfg(struct ice_hw *hw)
{
	ice_cache_vlan_mode(hw);
}
