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
#include "ice_sched.h"
#include "ice_adminq_cmd.h"
#include "ice_flow.h"
#include "ice_switch.h"

#define ICE_PF_RESET_WAIT_COUNT	500

static const char * const ice_link_mode_str_low[] = {
	ice_arr_elem_idx(0, "100BASE_TX"),
	ice_arr_elem_idx(1, "100M_SGMII"),
	ice_arr_elem_idx(2, "1000BASE_T"),
	ice_arr_elem_idx(3, "1000BASE_SX"),
	ice_arr_elem_idx(4, "1000BASE_LX"),
	ice_arr_elem_idx(5, "1000BASE_KX"),
	ice_arr_elem_idx(6, "1G_SGMII"),
	ice_arr_elem_idx(7, "2500BASE_T"),
	ice_arr_elem_idx(8, "2500BASE_X"),
	ice_arr_elem_idx(9, "2500BASE_KX"),
	ice_arr_elem_idx(10, "5GBASE_T"),
	ice_arr_elem_idx(11, "5GBASE_KR"),
	ice_arr_elem_idx(12, "10GBASE_T"),
	ice_arr_elem_idx(13, "10G_SFI_DA"),
	ice_arr_elem_idx(14, "10GBASE_SR"),
	ice_arr_elem_idx(15, "10GBASE_LR"),
	ice_arr_elem_idx(16, "10GBASE_KR_CR1"),
	ice_arr_elem_idx(17, "10G_SFI_AOC_ACC"),
	ice_arr_elem_idx(18, "10G_SFI_C2C"),
	ice_arr_elem_idx(19, "25GBASE_T"),
	ice_arr_elem_idx(20, "25GBASE_CR"),
	ice_arr_elem_idx(21, "25GBASE_CR_S"),
	ice_arr_elem_idx(22, "25GBASE_CR1"),
	ice_arr_elem_idx(23, "25GBASE_SR"),
	ice_arr_elem_idx(24, "25GBASE_LR"),
	ice_arr_elem_idx(25, "25GBASE_KR"),
	ice_arr_elem_idx(26, "25GBASE_KR_S"),
	ice_arr_elem_idx(27, "25GBASE_KR1"),
	ice_arr_elem_idx(28, "25G_AUI_AOC_ACC"),
	ice_arr_elem_idx(29, "25G_AUI_C2C"),
	ice_arr_elem_idx(30, "40GBASE_CR4"),
	ice_arr_elem_idx(31, "40GBASE_SR4"),
	ice_arr_elem_idx(32, "40GBASE_LR4"),
	ice_arr_elem_idx(33, "40GBASE_KR4"),
	ice_arr_elem_idx(34, "40G_XLAUI_AOC_ACC"),
	ice_arr_elem_idx(35, "40G_XLAUI"),
	ice_arr_elem_idx(36, "50GBASE_CR2"),
	ice_arr_elem_idx(37, "50GBASE_SR2"),
	ice_arr_elem_idx(38, "50GBASE_LR2"),
	ice_arr_elem_idx(39, "50GBASE_KR2"),
	ice_arr_elem_idx(40, "50G_LAUI2_AOC_ACC"),
	ice_arr_elem_idx(41, "50G_LAUI2"),
	ice_arr_elem_idx(42, "50G_AUI2_AOC_ACC"),
	ice_arr_elem_idx(43, "50G_AUI2"),
	ice_arr_elem_idx(44, "50GBASE_CP"),
	ice_arr_elem_idx(45, "50GBASE_SR"),
	ice_arr_elem_idx(46, "50GBASE_FR"),
	ice_arr_elem_idx(47, "50GBASE_LR"),
	ice_arr_elem_idx(48, "50GBASE_KR_PAM4"),
	ice_arr_elem_idx(49, "50G_AUI1_AOC_ACC"),
	ice_arr_elem_idx(50, "50G_AUI1"),
	ice_arr_elem_idx(51, "100GBASE_CR4"),
	ice_arr_elem_idx(52, "100GBASE_SR4"),
	ice_arr_elem_idx(53, "100GBASE_LR4"),
	ice_arr_elem_idx(54, "100GBASE_KR4"),
	ice_arr_elem_idx(55, "100G_CAUI4_AOC_ACC"),
	ice_arr_elem_idx(56, "100G_CAUI4"),
	ice_arr_elem_idx(57, "100G_AUI4_AOC_ACC"),
	ice_arr_elem_idx(58, "100G_AUI4"),
	ice_arr_elem_idx(59, "100GBASE_CR_PAM4"),
	ice_arr_elem_idx(60, "100GBASE_KR_PAM4"),
	ice_arr_elem_idx(61, "100GBASE_CP2"),
	ice_arr_elem_idx(62, "100GBASE_SR2"),
	ice_arr_elem_idx(63, "100GBASE_DR"),
};

static const char * const ice_link_mode_str_high[] = {
	ice_arr_elem_idx(0, "100GBASE_KR2_PAM4"),
	ice_arr_elem_idx(1, "100G_CAUI2_AOC_ACC"),
	ice_arr_elem_idx(2, "100G_CAUI2"),
	ice_arr_elem_idx(3, "100G_AUI2_AOC_ACC"),
	ice_arr_elem_idx(4, "100G_AUI2"),
	ice_arr_elem_idx(5, "200G_CR4_PAM4"),
	ice_arr_elem_idx(6, "200G_SR4"),
	ice_arr_elem_idx(7, "200G_FR4"),
	ice_arr_elem_idx(8, "200G_LR4"),
	ice_arr_elem_idx(9, "200G_DR4"),
	ice_arr_elem_idx(10, "200G_KR4_PAM4"),
	ice_arr_elem_idx(11, "200G_AUI4_AOC_ACC"),
	ice_arr_elem_idx(12, "200G_AUI4"),
	ice_arr_elem_idx(13, "200G_AUI8_AOC_ACC"),
	ice_arr_elem_idx(14, "200G_AUI8"),
	ice_arr_elem_idx(15, "400GBASE_FR8"),
};

/**
 * ice_dump_phy_type - helper function to dump phy_type
 * @hw: pointer to the HW structure
 * @low: 64 bit value for phy_type_low
 * @high: 64 bit value for phy_type_high
 * @prefix: prefix string to differentiate multiple dumps
 */
static void
ice_dump_phy_type(struct ice_hw *hw, u64 low, u64 high, const char *prefix)
{
	u32 i;

	ice_debug(hw, ICE_DBG_PHY, "%s: phy_type_low: 0x%016llx\n", prefix,
		  (unsigned long long)low);

	for (i = 0; i < ARRAY_SIZE(ice_link_mode_str_low); i++) {
		if (low & BIT_ULL(i))
			ice_debug(hw, ICE_DBG_PHY, "%s:   bit(%d): %s\n",
				  prefix, i, ice_link_mode_str_low[i]);
	}

	ice_debug(hw, ICE_DBG_PHY, "%s: phy_type_high: 0x%016llx\n", prefix,
		  (unsigned long long)high);

	for (i = 0; i < ARRAY_SIZE(ice_link_mode_str_high); i++) {
		if (high & BIT_ULL(i))
			ice_debug(hw, ICE_DBG_PHY, "%s:   bit(%d): %s\n",
				  prefix, i, ice_link_mode_str_high[i]);
	}
}

/**
 * ice_set_mac_type - Sets MAC type
 * @hw: pointer to the HW structure
 *
 * This function sets the MAC type of the adapter based on the
 * vendor ID and device ID stored in the HW structure.
 */
int ice_set_mac_type(struct ice_hw *hw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (hw->vendor_id != ICE_INTEL_VENDOR_ID)
		return ICE_ERR_DEVICE_NOT_SUPPORTED;

	switch (hw->device_id) {
	case ICE_DEV_ID_E810C_BACKPLANE:
	case ICE_DEV_ID_E810C_QSFP:
	case ICE_DEV_ID_E810C_SFP:
	case ICE_DEV_ID_E810_XXV_BACKPLANE:
	case ICE_DEV_ID_E810_XXV_QSFP:
	case ICE_DEV_ID_E810_XXV_SFP:
		hw->mac_type = ICE_MAC_E810;
		break;
	case ICE_DEV_ID_E822C_10G_BASE_T:
	case ICE_DEV_ID_E822C_BACKPLANE:
	case ICE_DEV_ID_E822C_QSFP:
	case ICE_DEV_ID_E822C_SFP:
	case ICE_DEV_ID_E822C_SGMII:
	case ICE_DEV_ID_E822L_10G_BASE_T:
	case ICE_DEV_ID_E822L_BACKPLANE:
	case ICE_DEV_ID_E822L_SFP:
	case ICE_DEV_ID_E822L_SGMII:
	case ICE_DEV_ID_E823L_10G_BASE_T:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823L_BACKPLANE:
	case ICE_DEV_ID_E823L_QSFP:
	case ICE_DEV_ID_E823L_SFP:
	case ICE_DEV_ID_E823C_10G_BASE_T:
	case ICE_DEV_ID_E823C_BACKPLANE:
	case ICE_DEV_ID_E823C_QSFP:
	case ICE_DEV_ID_E823C_SFP:
	case ICE_DEV_ID_E823C_SGMII:
		hw->mac_type = ICE_MAC_GENERIC;
		break;
	case ICE_DEV_ID_E825C_BACKPLANE:
	case ICE_DEV_ID_E825C_QSFP:
	case ICE_DEV_ID_E825C_SFP:
	case ICE_DEV_ID_E825C_SGMII:
		hw->mac_type = ICE_MAC_GENERIC_3K_E825;
		break;
	case ICE_DEV_ID_E830_BACKPLANE:
	case ICE_DEV_ID_E830_QSFP56:
	case ICE_DEV_ID_E830_SFP:
	case ICE_DEV_ID_E830C_BACKPLANE:
	case ICE_DEV_ID_E830_L_BACKPLANE:
	case ICE_DEV_ID_E830C_QSFP:
	case ICE_DEV_ID_E830_L_QSFP:
	case ICE_DEV_ID_E830C_SFP:
	case ICE_DEV_ID_E830_L_SFP:
	case ICE_DEV_ID_E835CC_BACKPLANE:
	case ICE_DEV_ID_E835CC_QSFP56:
	case ICE_DEV_ID_E835CC_SFP:
	case ICE_DEV_ID_E835C_BACKPLANE:
	case ICE_DEV_ID_E835C_QSFP:
	case ICE_DEV_ID_E835C_SFP:
	case ICE_DEV_ID_E835_L_BACKPLANE:
	case ICE_DEV_ID_E835_L_QSFP:
	case ICE_DEV_ID_E835_L_SFP:
		hw->mac_type = ICE_MAC_E830;
		break;
	default:
		hw->mac_type = ICE_MAC_UNKNOWN;
		break;
	}

	ice_debug(hw, ICE_DBG_INIT, "mac_type: %d\n", hw->mac_type);
	return 0;
}

/**
 * ice_is_generic_mac
 * @hw: pointer to the hardware structure
 *
 * returns true if mac_type is ICE_MAC_GENERIC, false if not
 */
bool ice_is_generic_mac(struct ice_hw *hw)
{
	return (hw->mac_type == ICE_MAC_GENERIC ||
		hw->mac_type == ICE_MAC_GENERIC_3K ||
		hw->mac_type == ICE_MAC_GENERIC_3K_E825);
}

/**
 * ice_is_e810
 * @hw: pointer to the hardware structure
 *
 * returns true if the device is E810 based, false if not.
 */
bool ice_is_e810(struct ice_hw *hw)
{
	return hw->mac_type == ICE_MAC_E810;
}

/**
 * ice_is_e810t
 * @hw: pointer to the hardware structure
 *
 * returns true if the device is E810T based, false if not.
 */
bool ice_is_e810t(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E810C_SFP:
		switch (hw->subsystem_device_id) {
		case ICE_SUBDEV_ID_E810T:
		case ICE_SUBDEV_ID_E810T2:
		case ICE_SUBDEV_ID_E810T3:
		case ICE_SUBDEV_ID_E810T4:
		case ICE_SUBDEV_ID_E810T6:
		case ICE_SUBDEV_ID_E810T7:
			return true;
		}
		break;
	case ICE_DEV_ID_E810C_QSFP:
		switch (hw->subsystem_device_id) {
		case ICE_SUBDEV_ID_E810T2:
		case ICE_SUBDEV_ID_E810T3:
		case ICE_SUBDEV_ID_E810T5:
			return true;
		}
		break;
	default:
		break;
	}

	return false;
}

/**
 * ice_is_e830
 * @hw: pointer to the hardware structure
 *
 * returns true if the device is E830 based, false if not.
 */
bool ice_is_e830(struct ice_hw *hw)
{
	return hw->mac_type == ICE_MAC_E830;
}

/**
 * ice_is_e823
 * @hw: pointer to the hardware structure
 *
 * returns true if the device is E823-L or E823-C based, false if not.
 */
bool ice_is_e823(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E823L_BACKPLANE:
	case ICE_DEV_ID_E823L_SFP:
	case ICE_DEV_ID_E823L_10G_BASE_T:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823L_QSFP:
	case ICE_DEV_ID_E823C_BACKPLANE:
	case ICE_DEV_ID_E823C_QSFP:
	case ICE_DEV_ID_E823C_SFP:
	case ICE_DEV_ID_E823C_10G_BASE_T:
	case ICE_DEV_ID_E823C_SGMII:
		return true;
	default:
		return false;
	}
}

/**
 * ice_is_e825c
 * @hw: pointer to the hardware structure
 *
 * returns true if the device is E825-C based, false if not.
 */
bool ice_is_e825c(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E825C_BACKPLANE:
	case ICE_DEV_ID_E825C_QSFP:
	case ICE_DEV_ID_E825C_SFP:
	case ICE_DEV_ID_E825C_SGMII:
		return true;
	default:
		return false;
	}
}

/**
 * ice_clear_pf_cfg - Clear PF configuration
 * @hw: pointer to the hardware structure
 *
 * Clears any existing PF configuration (VSIs, VSI lists, switch rules, port
 * configuration, flow director filters, etc.).
 */
int ice_clear_pf_cfg(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_pf_cfg);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_aq_manage_mac_read - manage MAC address read command
 * @hw: pointer to the HW struct
 * @buf: a virtual buffer to hold the manage MAC read response
 * @buf_size: Size of the virtual buffer
 * @cd: pointer to command details structure or NULL
 *
 * This function is used to return per PF station MAC address (0x0107).
 * NOTE: Upon successful completion of this command, MAC address information
 * is returned in user specified buffer. Please interpret user specified
 * buffer as "manage_mac_read" response.
 * Response such as various MAC addresses are stored in HW struct (port.mac)
 * ice_discover_dev_caps is expected to be called before this function is
 * called.
 */
int
ice_aq_manage_mac_read(struct ice_hw *hw, void *buf, u16 buf_size,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_manage_mac_read_resp *resp;
	struct ice_aqc_manage_mac_read *cmd;
	struct ice_aq_desc desc;
	int status;
	u16 flags;
	u8 i;

	cmd = &desc.params.mac_read;

	if (buf_size < sizeof(*resp))
		return ICE_ERR_BUF_TOO_SHORT;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_manage_mac_read);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (status)
		return status;

	resp = (struct ice_aqc_manage_mac_read_resp *)buf;
	flags = LE16_TO_CPU(cmd->flags) & ICE_AQC_MAN_MAC_READ_M;

	if (!(flags & ICE_AQC_MAN_MAC_LAN_ADDR_VALID)) {
		ice_debug(hw, ICE_DBG_LAN, "got invalid MAC address\n");
		return ICE_ERR_CFG;
	}

	/* A single port can report up to two (LAN and WoL) addresses */
	for (i = 0; i < cmd->num_addr; i++)
		if (resp[i].addr_type == ICE_AQC_MAN_MAC_ADDR_TYPE_LAN) {
			ice_memcpy(hw->port_info->mac.lan_addr,
				   resp[i].mac_addr, ETH_ALEN,
				   ICE_NONDMA_TO_NONDMA);
			ice_memcpy(hw->port_info->mac.perm_addr,
				   resp[i].mac_addr,
				   ETH_ALEN, ICE_NONDMA_TO_NONDMA);
			break;
		}
	return 0;
}

/**
 * ice_phy_maps_to_media
 * @phy_type_low: PHY type low bits
 * @phy_type_high: PHY type high bits
 * @media_mask_low: media type PHY type low bitmask
 * @media_mask_high: media type PHY type high bitmask
 *
 * Return true if PHY type [low|high] bits are only of media type PHY types
 * [low|high] bitmask.
 */
static bool
ice_phy_maps_to_media(u64 phy_type_low, u64 phy_type_high,
		      u64 media_mask_low, u64 media_mask_high)
{
	/* check if a PHY type exist for media type */
	if (!(phy_type_low & media_mask_low ||
	      phy_type_high & media_mask_high))
		return false;

	/* check that PHY types are only of media type */
	if (!(phy_type_low & ~media_mask_low) &&
	    !(phy_type_high & ~media_mask_high))
		return true;

	return false;
}

/**
 * ice_set_media_type - Sets media type
 * @pi: port information structure
 *
 * Set ice_port_info PHY media type based on PHY type. This should be called
 * from Get PHY caps with media.
 */
static void ice_set_media_type(struct ice_port_info *pi)
{
	enum ice_media_type *media_type;
	u64 phy_type_high, phy_type_low;

	phy_type_high = pi->phy.phy_type_high;
	phy_type_low = pi->phy.phy_type_low;
	media_type = &pi->phy.media_type;

	/* if no media, then media type is NONE */
	if (!(pi->phy.link_info.link_info & ICE_AQ_MEDIA_AVAILABLE))
		*media_type = ICE_MEDIA_NONE;
	/* else if PHY types are only BASE-T, then media type is BASET */
	else if (ice_phy_maps_to_media(phy_type_low, phy_type_high,
				       ICE_MEDIA_BASET_PHY_TYPE_LOW_M, 0))
		*media_type = ICE_MEDIA_BASET;
	/* else if any PHY type is BACKPLANE, then media type is BACKPLANE */
	else if (phy_type_low & ICE_MEDIA_BP_PHY_TYPE_LOW_M ||
		 phy_type_high & ICE_MEDIA_BP_PHY_TYPE_HIGH_M)
		*media_type = ICE_MEDIA_BACKPLANE;
	/* else if PHY types are only optical, or optical and C2M, then media
	 * type is FIBER
	 */
	else if (ice_phy_maps_to_media(phy_type_low, phy_type_high,
				       ICE_MEDIA_OPT_PHY_TYPE_LOW_M,
				       ICE_MEDIA_OPT_PHY_TYPE_HIGH_M) ||
		 ((phy_type_low & ICE_MEDIA_OPT_PHY_TYPE_LOW_M ||
		   phy_type_high & ICE_MEDIA_OPT_PHY_TYPE_HIGH_M) &&
		  (phy_type_low & ICE_MEDIA_C2M_PHY_TYPE_LOW_M ||
		   phy_type_high & ICE_MEDIA_C2C_PHY_TYPE_HIGH_M)))
		*media_type = ICE_MEDIA_FIBER;
	/* else if PHY types are only DA, or DA and C2C, then media type DA */
	else if (ice_phy_maps_to_media(phy_type_low, phy_type_high,
				       ICE_MEDIA_DAC_PHY_TYPE_LOW_M,
				       ICE_MEDIA_DAC_PHY_TYPE_HIGH_M) ||
		 ((phy_type_low & ICE_MEDIA_DAC_PHY_TYPE_LOW_M ||
		   phy_type_high & ICE_MEDIA_DAC_PHY_TYPE_HIGH_M) &&
		  (phy_type_low & ICE_MEDIA_C2C_PHY_TYPE_LOW_M ||
		   phy_type_high & ICE_MEDIA_C2C_PHY_TYPE_HIGH_M)))
		*media_type = ICE_MEDIA_DA;
	/* else if PHY types are only C2M or only C2C, then media is AUI */
	else if (ice_phy_maps_to_media(phy_type_low, phy_type_high,
				       ICE_MEDIA_C2M_PHY_TYPE_LOW_M,
				       ICE_MEDIA_C2M_PHY_TYPE_HIGH_M) ||
		 ice_phy_maps_to_media(phy_type_low, phy_type_high,
				       ICE_MEDIA_C2C_PHY_TYPE_LOW_M,
				       ICE_MEDIA_C2C_PHY_TYPE_HIGH_M))
		*media_type = ICE_MEDIA_AUI;

	else
		*media_type = ICE_MEDIA_UNKNOWN;
}

/**
 * ice_aq_get_phy_caps - returns PHY capabilities
 * @pi: port information structure
 * @qual_mods: report qualified modules
 * @report_mode: report mode capabilities
 * @pcaps: structure for PHY capabilities to be filled
 * @cd: pointer to command details structure or NULL
 *
 * Returns the various PHY capabilities supported on the Port (0x0600)
 */
int
ice_aq_get_phy_caps(struct ice_port_info *pi, bool qual_mods, u8 report_mode,
		    struct ice_aqc_get_phy_caps_data *pcaps,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_get_phy_caps *cmd;
	u16 pcaps_size = sizeof(*pcaps);
	struct ice_aq_desc desc;
	const char *prefix;
	struct ice_hw *hw;
	int status;

	cmd = &desc.params.get_phy;

	if (!pcaps || (report_mode & ~ICE_AQC_REPORT_MODE_M) || !pi)
		return ICE_ERR_PARAM;
	hw = pi->hw;

	if (report_mode == ICE_AQC_REPORT_DFLT_CFG &&
	    !ice_fw_supports_report_dflt_cfg(hw))
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_phy_caps);

	if (qual_mods)
		cmd->param0 |= CPU_TO_LE16(ICE_AQC_GET_PHY_RQM);

	cmd->param0 |= CPU_TO_LE16(report_mode);

	status = ice_aq_send_cmd(hw, &desc, pcaps, pcaps_size, cd);

	ice_debug(hw, ICE_DBG_LINK, "get phy caps dump\n");

	switch (report_mode) {
	case ICE_AQC_REPORT_TOPO_CAP_MEDIA:
		prefix = "phy_caps_media";
		break;
	case ICE_AQC_REPORT_TOPO_CAP_NO_MEDIA:
		prefix = "phy_caps_no_media";
		break;
	case ICE_AQC_REPORT_ACTIVE_CFG:
		prefix = "phy_caps_active";
		break;
	case ICE_AQC_REPORT_DFLT_CFG:
		prefix = "phy_caps_default";
		break;
	default:
		prefix = "phy_caps_invalid";
	}

	ice_dump_phy_type(hw, LE64_TO_CPU(pcaps->phy_type_low),
			  LE64_TO_CPU(pcaps->phy_type_high), prefix);

	ice_debug(hw, ICE_DBG_LINK, "%s: report_mode = 0x%x\n",
		  prefix, report_mode);
	ice_debug(hw, ICE_DBG_LINK, "%s: caps = 0x%x\n", prefix, pcaps->caps);
	ice_debug(hw, ICE_DBG_LINK, "%s: low_power_ctrl_an = 0x%x\n", prefix,
		  pcaps->low_power_ctrl_an);
	ice_debug(hw, ICE_DBG_LINK, "%s: eee_cap = 0x%x\n", prefix,
		  pcaps->eee_cap);
	ice_debug(hw, ICE_DBG_LINK, "%s: eeer_value = 0x%x\n", prefix,
		  pcaps->eeer_value);
	ice_debug(hw, ICE_DBG_LINK, "%s: link_fec_options = 0x%x\n", prefix,
		  pcaps->link_fec_options);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_compliance_enforcement = 0x%x\n",
		  prefix, pcaps->module_compliance_enforcement);
	ice_debug(hw, ICE_DBG_LINK, "%s: extended_compliance_code = 0x%x\n",
		  prefix, pcaps->extended_compliance_code);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[0] = 0x%x\n", prefix,
		  pcaps->module_type[0]);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[1] = 0x%x\n", prefix,
		  pcaps->module_type[1]);
	ice_debug(hw, ICE_DBG_LINK, "%s: module_type[2] = 0x%x\n", prefix,
		  pcaps->module_type[2]);

	if (!status && report_mode == ICE_AQC_REPORT_TOPO_CAP_MEDIA) {
		pi->phy.phy_type_low = LE64_TO_CPU(pcaps->phy_type_low);
		pi->phy.phy_type_high = LE64_TO_CPU(pcaps->phy_type_high);
		ice_memcpy(pi->phy.link_info.module_type, &pcaps->module_type,
			   sizeof(pi->phy.link_info.module_type),
			   ICE_NONDMA_TO_NONDMA);
		ice_set_media_type(pi);
		ice_debug(hw, ICE_DBG_LINK, "%s: media_type = 0x%x\n", prefix,
			  pi->phy.media_type);
	}

	return status;
}

/**
 * ice_aq_get_phy_equalization - function to read serdes equalizer value from
 *                               firmware using admin queue command.
 * @hw: pointer to the HW struct
 * @data_in: represents the serdes equalization parameter requested
 * @op_code: represents the serdes number and flag to represent tx or rx
 * @serdes_num: represents the serdes number
 * @output: pointer to the caller-supplied buffer to return serdes equalizer
 *
 * Returns 0 on success,
 *	   non-zero status on error
 */
int ice_aq_get_phy_equalization(struct ice_hw *hw, u16 data_in, u16 op_code,
			    u8 serdes_num, int *output)
{
	struct ice_aqc_dnl_call_command *cmd;
	struct ice_aqc_dnl_call buf;
	struct ice_aq_desc desc;
	int err = 0;

	if (!hw || !output)
		return (ICE_ERR_PARAM);

	memset(&buf, 0, sizeof(buf));
	buf.sto.txrx_equa_reqs.data_in = CPU_TO_LE16(data_in);
	buf.sto.txrx_equa_reqs.op_code_serdes_sel =
		CPU_TO_LE16(op_code | (serdes_num & 0xF));

	cmd = &desc.params.dnl_call;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_dnl_call);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_BUF | ICE_AQ_FLAG_RD |
				  ICE_AQ_FLAG_SI);
	desc.datalen = CPU_TO_LE16(sizeof(struct ice_aqc_dnl_call));
	cmd->activity_id = CPU_TO_LE16(ICE_AQC_ACT_ID_DNL);
	cmd->ctx = 0;

	err = ice_aq_send_cmd(hw, &desc, &buf,
			      sizeof(struct ice_aqc_dnl_call), NULL);
	if (!err)
		*output = buf.sto.txrx_equa_resp.val;

	return err;
}

#define ice_get_link_status_data_ver(hw) ((hw)->mac_type == ICE_MAC_E830 ? \
		ICE_GET_LINK_STATUS_DATA_V2 : ICE_GET_LINK_STATUS_DATA_V1)

/**
 * ice_get_link_status_datalen
 * @hw: pointer to the HW struct
 *
 * return Get Link Status datalen
 */
static u16 ice_get_link_status_datalen(struct ice_hw *hw)
{
	return (ice_get_link_status_data_ver(hw) ==
		ICE_GET_LINK_STATUS_DATA_V1) ? ICE_GET_LINK_STATUS_DATALEN_V1 :
		ICE_GET_LINK_STATUS_DATALEN_V2;
}

/**
 * ice_aq_get_link_info
 * @pi: port information structure
 * @ena_lse: enable/disable LinkStatusEvent reporting
 * @link: pointer to link status structure - optional
 * @cd: pointer to command details structure or NULL
 *
 * Get Link Status (0x607). Returns the link status of the adapter.
 */
int
ice_aq_get_link_info(struct ice_port_info *pi, bool ena_lse,
		     struct ice_link_status *link, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_link_status_data link_data = { 0 };
	struct ice_aqc_get_link_status *resp;
	struct ice_link_status *li_old, *li;
	struct ice_fc_info *hw_fc_info;
	bool tx_pause, rx_pause;
	struct ice_aq_desc desc;
	struct ice_hw *hw;
	u16 cmd_flags;
	int status;

	if (!pi)
		return ICE_ERR_PARAM;
	hw = pi->hw;

	li_old = &pi->phy.link_info_old;
	li = &pi->phy.link_info;
	hw_fc_info = &pi->fc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_status);
	cmd_flags = (ena_lse) ? ICE_AQ_LSE_ENA : ICE_AQ_LSE_DIS;
	resp = &desc.params.get_link_status;
	resp->cmd_flags = CPU_TO_LE16(cmd_flags);
	resp->lport_num = pi->lport;

	status = ice_aq_send_cmd(hw, &desc, &link_data,
				 ice_get_link_status_datalen(hw), cd);
	if (status)
		return status;

	/* save off old link status information */
	*li_old = *li;

	/* update current link status information */
	li->link_speed = LE16_TO_CPU(link_data.link_speed);
	li->phy_type_low = LE64_TO_CPU(link_data.phy_type_low);
	li->phy_type_high = LE64_TO_CPU(link_data.phy_type_high);
	li->link_info = link_data.link_info;
	li->link_cfg_err = link_data.link_cfg_err;
	li->an_info = link_data.an_info;
	li->ext_info = link_data.ext_info;
	li->max_frame_size = LE16_TO_CPU(link_data.max_frame_size);
	li->fec_info = link_data.cfg & ICE_AQ_FEC_MASK;
	li->topo_media_conflict = link_data.topo_media_conflict;
	li->pacing = link_data.cfg & (ICE_AQ_CFG_PACING_M |
				      ICE_AQ_CFG_PACING_TYPE_M);

	/* update fc info */
	tx_pause = !!(link_data.an_info & ICE_AQ_LINK_PAUSE_TX);
	rx_pause = !!(link_data.an_info & ICE_AQ_LINK_PAUSE_RX);
	if (tx_pause && rx_pause)
		hw_fc_info->current_mode = ICE_FC_FULL;
	else if (tx_pause)
		hw_fc_info->current_mode = ICE_FC_TX_PAUSE;
	else if (rx_pause)
		hw_fc_info->current_mode = ICE_FC_RX_PAUSE;
	else
		hw_fc_info->current_mode = ICE_FC_NONE;

	li->lse_ena = !!(resp->cmd_flags & CPU_TO_LE16(ICE_AQ_LSE_IS_ENABLED));

	ice_debug(hw, ICE_DBG_LINK, "get link info\n");
	ice_debug(hw, ICE_DBG_LINK, "	link_speed = 0x%x\n", li->link_speed);
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_low = 0x%llx\n",
		  (unsigned long long)li->phy_type_low);
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_high = 0x%llx\n",
		  (unsigned long long)li->phy_type_high);
	ice_debug(hw, ICE_DBG_LINK, "	link_info = 0x%x\n", li->link_info);
	ice_debug(hw, ICE_DBG_LINK, "	link_cfg_err = 0x%x\n", li->link_cfg_err);
	ice_debug(hw, ICE_DBG_LINK, "	an_info = 0x%x\n", li->an_info);
	ice_debug(hw, ICE_DBG_LINK, "	ext_info = 0x%x\n", li->ext_info);
	ice_debug(hw, ICE_DBG_LINK, "	fec_info = 0x%x\n", li->fec_info);
	ice_debug(hw, ICE_DBG_LINK, "	lse_ena = 0x%x\n", li->lse_ena);
	ice_debug(hw, ICE_DBG_LINK, "	max_frame = 0x%x\n",
		  li->max_frame_size);
	ice_debug(hw, ICE_DBG_LINK, "	pacing = 0x%x\n", li->pacing);

	/* save link status information */
	if (link)
		*link = *li;

	/* flag cleared so calling functions don't call AQ again */
	pi->phy.get_link_info = false;

	return 0;
}

/**
 * ice_fill_tx_timer_and_fc_thresh
 * @hw: pointer to the HW struct
 * @cmd: pointer to MAC cfg structure
 *
 * Add Tx timer and FC refresh threshold info to Set MAC Config AQ command
 * descriptor
 */
static void
ice_fill_tx_timer_and_fc_thresh(struct ice_hw *hw,
				struct ice_aqc_set_mac_cfg *cmd)
{
	u16 fc_thres_val, tx_timer_val;
	u32 val;

	/* We read back the transmit timer and fc threshold value of
	 * LFC. Thus, we will use index =
	 * PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA_MAX_INDEX.
	 *
	 * Also, because we are operating on transmit timer and fc
	 * threshold of LFC, we don't turn on any bit in tx_tmr_priority
	 */
#define E800_IDX_OF_LFC E800_PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA_MAX_INDEX

	if ((hw)->mac_type == ICE_MAC_E830) {
		/* Retrieve the transmit timer */
		val = rd32(hw, E830_PRTMAC_CL01_PAUSE_QUANTA);
		tx_timer_val = val & E830_PRTMAC_CL01_PAUSE_QUANTA_CL0_PAUSE_QUANTA_M;
		cmd->tx_tmr_value = CPU_TO_LE16(tx_timer_val);

		/* Retrieve the fc threshold */
		val = rd32(hw, E830_PRTMAC_CL01_QUANTA_THRESH);
		fc_thres_val = val & E830_PRTMAC_CL01_QUANTA_THRESH_CL0_QUANTA_THRESH_M;
	} else {
		/* Retrieve the transmit timer */
		val = rd32(hw, E800_PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA(E800_IDX_OF_LFC));
		tx_timer_val = val &
			E800_PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA_HSEC_CTL_TX_PAUSE_QUANTA_M;
		cmd->tx_tmr_value = CPU_TO_LE16(tx_timer_val);

		/* Retrieve the fc threshold */
		val = rd32(hw, E800_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(E800_IDX_OF_LFC));
		fc_thres_val = val & E800_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_M;
	}

	cmd->fc_refresh_threshold = CPU_TO_LE16(fc_thres_val);
}

/**
 * ice_aq_set_mac_cfg
 * @hw: pointer to the HW struct
 * @max_frame_size: Maximum Frame Size to be supported
 * @auto_drop: Tell HW to drop packets if TC queue is blocked
 * @cd: pointer to command details structure or NULL
 *
 * Set MAC configuration (0x0603)
 */
int
ice_aq_set_mac_cfg(struct ice_hw *hw, u16 max_frame_size, bool auto_drop,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_set_mac_cfg *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_mac_cfg;

	if (max_frame_size == 0)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_mac_cfg);

	cmd->max_frame_size = CPU_TO_LE16(max_frame_size);

	if (ice_is_fw_auto_drop_supported(hw) && auto_drop)
		cmd->drop_opts |= ICE_AQ_SET_MAC_AUTO_DROP_BLOCKING_PKTS;
	ice_fill_tx_timer_and_fc_thresh(hw, cmd);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_init_fltr_mgmt_struct - initializes filter management list and locks
 * @hw: pointer to the HW struct
 */
int ice_init_fltr_mgmt_struct(struct ice_hw *hw)
{
	struct ice_switch_info *sw;
	int status;

	hw->switch_info = (struct ice_switch_info *)
			  ice_malloc(hw, sizeof(*hw->switch_info));

	sw = hw->switch_info;

	if (!sw)
		return ICE_ERR_NO_MEMORY;

	INIT_LIST_HEAD(&sw->vsi_list_map_head);
	sw->prof_res_bm_init = 0;

	status = ice_init_def_sw_recp(hw, &hw->switch_info->recp_list);
	if (status) {
		ice_free(hw, hw->switch_info);
		return status;
	}
	return 0;
}

/**
 * ice_cleanup_fltr_mgmt_single - clears single filter mngt struct
 * @hw: pointer to the HW struct
 * @sw: pointer to switch info struct for which function clears filters
 */
static void
ice_cleanup_fltr_mgmt_single(struct ice_hw *hw, struct ice_switch_info *sw)
{
	struct ice_vsi_list_map_info *v_pos_map;
	struct ice_vsi_list_map_info *v_tmp_map;
	struct ice_sw_recipe *recps;
	u8 i;

	if (!sw)
		return;

	LIST_FOR_EACH_ENTRY_SAFE(v_pos_map, v_tmp_map, &sw->vsi_list_map_head,
				 ice_vsi_list_map_info, list_entry) {
		LIST_DEL(&v_pos_map->list_entry);
		ice_free(hw, v_pos_map);
	}
	recps = sw->recp_list;
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct ice_recp_grp_entry *rg_entry, *tmprg_entry;

		recps[i].root_rid = i;
		LIST_FOR_EACH_ENTRY_SAFE(rg_entry, tmprg_entry,
					 &recps[i].rg_list, ice_recp_grp_entry,
					 l_entry) {
			LIST_DEL(&rg_entry->l_entry);
			ice_free(hw, rg_entry);
		}

		if (recps[i].adv_rule) {
			struct ice_adv_fltr_mgmt_list_entry *tmp_entry;
			struct ice_adv_fltr_mgmt_list_entry *lst_itr;

			ice_destroy_lock(&recps[i].filt_rule_lock);
			LIST_FOR_EACH_ENTRY_SAFE(lst_itr, tmp_entry,
						 &recps[i].filt_rules,
						 ice_adv_fltr_mgmt_list_entry,
						 list_entry) {
				LIST_DEL(&lst_itr->list_entry);
				ice_free(hw, lst_itr->lkups);
				ice_free(hw, lst_itr);
			}
		} else {
			struct ice_fltr_mgmt_list_entry *lst_itr, *tmp_entry;

			ice_destroy_lock(&recps[i].filt_rule_lock);
			LIST_FOR_EACH_ENTRY_SAFE(lst_itr, tmp_entry,
						 &recps[i].filt_rules,
						 ice_fltr_mgmt_list_entry,
						 list_entry) {
				LIST_DEL(&lst_itr->list_entry);
				ice_free(hw, lst_itr);
			}
		}
		if (recps[i].root_buf)
			ice_free(hw, recps[i].root_buf);
	}
	ice_rm_sw_replay_rule_info(hw, sw);
	ice_free(hw, sw->recp_list);
	ice_free(hw, sw);
}

/**
 * ice_cleanup_fltr_mgmt_struct - cleanup filter management list and locks
 * @hw: pointer to the HW struct
 */
void ice_cleanup_fltr_mgmt_struct(struct ice_hw *hw)
{
	ice_cleanup_fltr_mgmt_single(hw, hw->switch_info);
}

/**
 * ice_get_itr_intrl_gran
 * @hw: pointer to the HW struct
 *
 * Determines the ITR/INTRL granularities based on the maximum aggregate
 * bandwidth according to the device's configuration during power-on.
 */
static void ice_get_itr_intrl_gran(struct ice_hw *hw)
{
	u8 max_agg_bw = (rd32(hw, GL_PWR_MODE_CTL) &
			 GL_PWR_MODE_CTL_CAR_MAX_BW_M) >>
			GL_PWR_MODE_CTL_CAR_MAX_BW_S;

	switch (max_agg_bw) {
	case ICE_MAX_AGG_BW_200G:
	case ICE_MAX_AGG_BW_100G:
	case ICE_MAX_AGG_BW_50G:
		hw->itr_gran = ICE_ITR_GRAN_ABOVE_25;
		hw->intrl_gran = ICE_INTRL_GRAN_ABOVE_25;
		break;
	case ICE_MAX_AGG_BW_25G:
		hw->itr_gran = ICE_ITR_GRAN_MAX_25;
		hw->intrl_gran = ICE_INTRL_GRAN_MAX_25;
		break;
	}
}

/**
 * ice_print_rollback_msg - print FW rollback message
 * @hw: pointer to the hardware structure
 */
void ice_print_rollback_msg(struct ice_hw *hw)
{
	char nvm_str[ICE_NVM_VER_LEN] = { 0 };
	struct ice_orom_info *orom;
	struct ice_nvm_info *nvm;

	orom = &hw->flash.orom;
	nvm = &hw->flash.nvm;

	(void)SNPRINTF(nvm_str, sizeof(nvm_str), "%x.%02x 0x%x %d.%d.%d",
		 nvm->major, nvm->minor, nvm->eetrack, orom->major,
		 orom->build, orom->patch);
	ice_warn(hw,
		 "Firmware rollback mode detected. Current version is NVM: %s, FW: %d.%d. Device may exhibit limited functionality. Refer to the Intel(R) Ethernet Adapters and Devices User Guide for details on firmware rollback mode\n",
		 nvm_str, hw->fw_maj_ver, hw->fw_min_ver);
}

/**
 * ice_set_umac_shared
 * @hw: pointer to the hw struct
 *
 * Set boolean flag to allow unicast MAC sharing
 */
void ice_set_umac_shared(struct ice_hw *hw)
{
	hw->umac_shared = true;
}

/**
 * ice_init_hw - main hardware initialization routine
 * @hw: pointer to the hardware structure
 */
int ice_init_hw(struct ice_hw *hw)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	u16 mac_buf_len;
	void *mac_buf;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	/* Set MAC type based on DeviceID */
	status = ice_set_mac_type(hw);
	if (status)
		return status;

	hw->pf_id = (u8)(rd32(hw, PF_FUNC_RID) &
			 PF_FUNC_RID_FUNCTION_NUMBER_M) >>
		PF_FUNC_RID_FUNCTION_NUMBER_S;

	status = ice_reset(hw, ICE_RESET_PFR);
	if (status)
		return status;
	ice_get_itr_intrl_gran(hw);

	hw->fw_vsi_num = ICE_DFLT_VSI_INVAL;

	status = ice_create_all_ctrlq(hw);
	if (status)
		goto err_unroll_cqinit;

	ice_fwlog_set_support_ena(hw);
	status = ice_fwlog_set(hw, &hw->fwlog_cfg);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to enable FW logging, status %d.\n",
			  status);
	} else {
		if (hw->fwlog_cfg.options & ICE_FWLOG_OPTION_REGISTER_ON_INIT) {
			status = ice_fwlog_register(hw);
			if (status)
				ice_debug(hw, ICE_DBG_INIT, "Failed to register for FW logging events, status %d.\n",
					  status);
		} else {
			status = ice_fwlog_unregister(hw);
			if (status)
				ice_debug(hw, ICE_DBG_INIT, "Failed to unregister for FW logging events, status %d.\n",
					  status);
		}
	}

	status = ice_init_nvm(hw);
	if (status)
		goto err_unroll_cqinit;

	if (ice_get_fw_mode(hw) == ICE_FW_MODE_ROLLBACK)
		ice_print_rollback_msg(hw);

	if (!hw->skip_clear_pf) {
		status = ice_clear_pf_cfg(hw);
		if (status)
			goto err_unroll_cqinit;
	}

	ice_clear_pxe_mode(hw);

	status = ice_get_caps(hw);
	if (status)
		goto err_unroll_cqinit;

	if (!hw->port_info)
		hw->port_info = (struct ice_port_info *)
			ice_malloc(hw, sizeof(*hw->port_info));
	if (!hw->port_info) {
		status = ICE_ERR_NO_MEMORY;
		goto err_unroll_cqinit;
	}

	hw->port_info->loopback_mode = ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_NORMAL;

	/* set the back pointer to HW */
	hw->port_info->hw = hw;

	/* Initialize port_info struct with switch configuration data */
	status = ice_get_initial_sw_cfg(hw);
	if (status)
		goto err_unroll_alloc;

	hw->evb_veb = true;
	/* Query the allocated resources for Tx scheduler */
	status = ice_sched_query_res_alloc(hw);
	if (status) {
		ice_debug(hw, ICE_DBG_SCHED, "Failed to get scheduler allocated resources\n");
		goto err_unroll_alloc;
	}
	ice_sched_get_psm_clk_freq(hw);

	/* Initialize port_info struct with scheduler data */
	status = ice_sched_init_port(hw->port_info);
	if (status)
		goto err_unroll_sched;
	pcaps = (struct ice_aqc_get_phy_caps_data *)
		ice_malloc(hw, sizeof(*pcaps));
	if (!pcaps) {
		status = ICE_ERR_NO_MEMORY;
		goto err_unroll_sched;
	}

	/* Initialize port_info struct with PHY capabilities */
	status = ice_aq_get_phy_caps(hw->port_info, false,
				     ICE_AQC_REPORT_TOPO_CAP_MEDIA, pcaps, NULL);
	ice_free(hw, pcaps);
	if (status)
		ice_warn(hw, "Get PHY capabilities failed status = %d, continuing anyway\n",
			 status);

	/* Initialize port_info struct with link information */
	status = ice_aq_get_link_info(hw->port_info, false, NULL, NULL);
	if (status)
		goto err_unroll_sched;
	/* need a valid SW entry point to build a Tx tree */
	if (!hw->sw_entry_point_layer) {
		ice_debug(hw, ICE_DBG_SCHED, "invalid sw entry point\n");
		status = ICE_ERR_CFG;
		goto err_unroll_sched;
	}
	INIT_LIST_HEAD(&hw->agg_list);
	/* Initialize max burst size */
	if (!hw->max_burst_size)
		ice_cfg_rl_burst_size(hw, ICE_SCHED_DFLT_BURST_SIZE);
	status = ice_init_fltr_mgmt_struct(hw);
	if (status)
		goto err_unroll_sched;

	/* Get MAC information */

	/* A single port can report up to two (LAN and WoL) addresses */
	mac_buf = ice_calloc(hw, 2,
			     sizeof(struct ice_aqc_manage_mac_read_resp));
	mac_buf_len = 2 * sizeof(struct ice_aqc_manage_mac_read_resp);

	if (!mac_buf) {
		status = ICE_ERR_NO_MEMORY;
		goto err_unroll_fltr_mgmt_struct;
	}

	status = ice_aq_manage_mac_read(hw, mac_buf, mac_buf_len, NULL);
	ice_free(hw, mac_buf);

	if (status)
		goto err_unroll_fltr_mgmt_struct;

	/* enable jumbo frame support at MAC level */
	status = ice_aq_set_mac_cfg(hw, ICE_AQ_SET_MAC_FRAME_SIZE_MAX, false,
				    NULL);
	if (status)
		goto err_unroll_fltr_mgmt_struct;

	status = ice_init_hw_tbls(hw);
	if (status)
		goto err_unroll_fltr_mgmt_struct;
	ice_init_lock(&hw->tnl_lock);

	return 0;

err_unroll_fltr_mgmt_struct:
	ice_cleanup_fltr_mgmt_struct(hw);
err_unroll_sched:
	ice_sched_cleanup_all(hw);
err_unroll_alloc:
	ice_free(hw, hw->port_info);
	hw->port_info = NULL;
err_unroll_cqinit:
	ice_destroy_all_ctrlq(hw);
	return status;
}

/**
 * ice_deinit_hw - unroll initialization operations done by ice_init_hw
 * @hw: pointer to the hardware structure
 *
 * This should be called only during nominal operation, not as a result of
 * ice_init_hw() failing since ice_init_hw() will take care of unrolling
 * applicable initializations if it fails for any reason.
 */
void ice_deinit_hw(struct ice_hw *hw)
{
	ice_cleanup_fltr_mgmt_struct(hw);

	ice_sched_cleanup_all(hw);
	ice_sched_clear_agg(hw);
	ice_free_seg(hw);
	ice_free_hw_tbls(hw);
	ice_destroy_lock(&hw->tnl_lock);

	if (hw->port_info) {
		ice_free(hw, hw->port_info);
		hw->port_info = NULL;
	}

	ice_destroy_all_ctrlq(hw);

	/* Clear VSI contexts if not already cleared */
	ice_clear_all_vsi_ctx(hw);
}

/**
 * ice_check_reset - Check to see if a global reset is complete
 * @hw: pointer to the hardware structure
 */
int ice_check_reset(struct ice_hw *hw)
{
	u32 cnt, reg = 0, grst_timeout, uld_mask, reset_wait_cnt;

	/* Poll for Device Active state in case a recent CORER, GLOBR,
	 * or EMPR has occurred. The grst delay value is in 100ms units.
	 * Add 1sec for outstanding AQ commands that can take a long time.
	 */
	grst_timeout = ((rd32(hw, GLGEN_RSTCTL) & GLGEN_RSTCTL_GRSTDEL_M) >>
			GLGEN_RSTCTL_GRSTDEL_S) + 10;

	for (cnt = 0; cnt < grst_timeout; cnt++) {
		ice_msec_delay(100, true);
		reg = rd32(hw, GLGEN_RSTAT);
		if (!(reg & GLGEN_RSTAT_DEVSTATE_M))
			break;
	}

	if (cnt == grst_timeout) {
		ice_debug(hw, ICE_DBG_INIT, "Global reset polling failed to complete.\n");
		return ICE_ERR_RESET_FAILED;
	}

#define ICE_RESET_DONE_MASK	(GLNVM_ULD_PCIER_DONE_M |\
				 GLNVM_ULD_PCIER_DONE_1_M |\
				 GLNVM_ULD_CORER_DONE_M |\
				 GLNVM_ULD_GLOBR_DONE_M |\
				 GLNVM_ULD_POR_DONE_M |\
				 GLNVM_ULD_POR_DONE_1_M |\
				 GLNVM_ULD_PCIER_DONE_2_M)

	uld_mask = ICE_RESET_DONE_MASK | (hw->func_caps.common_cap.iwarp ?
					  GLNVM_ULD_PE_DONE_M : 0);

	reset_wait_cnt = ICE_PF_RESET_WAIT_COUNT;

	/* Device is Active; check Global Reset processes are done */
	for (cnt = 0; cnt < reset_wait_cnt; cnt++) {
		reg = rd32(hw, GLNVM_ULD) & uld_mask;
		if (reg == uld_mask) {
			ice_debug(hw, ICE_DBG_INIT, "Global reset processes done. %d\n", cnt);
			break;
		}
		ice_msec_delay(10, true);
	}

	if (cnt == reset_wait_cnt) {
		ice_debug(hw, ICE_DBG_INIT, "Wait for Reset Done timed out. GLNVM_ULD = 0x%x\n",
			  reg);
		return ICE_ERR_RESET_FAILED;
	}

	return 0;
}

/**
 * ice_pf_reset - Reset the PF
 * @hw: pointer to the hardware structure
 *
 * If a global reset has been triggered, this function checks
 * for its completion and then issues the PF reset
 */
static int ice_pf_reset(struct ice_hw *hw)
{
	u32 cnt, reg, reset_wait_cnt, cfg_lock_timeout;

	/* If at function entry a global reset was already in progress, i.e.
	 * state is not 'device active' or any of the reset done bits are not
	 * set in GLNVM_ULD, there is no need for a PF Reset; poll until the
	 * global reset is done.
	 */
	if ((rd32(hw, GLGEN_RSTAT) & GLGEN_RSTAT_DEVSTATE_M) ||
	    (rd32(hw, GLNVM_ULD) & ICE_RESET_DONE_MASK) ^ ICE_RESET_DONE_MASK) {
		/* poll on global reset currently in progress until done */
		if (ice_check_reset(hw))
			return ICE_ERR_RESET_FAILED;

		return 0;
	}

	/* Reset the PF */
	reg = rd32(hw, PFGEN_CTRL);

	wr32(hw, PFGEN_CTRL, (reg | PFGEN_CTRL_PFSWR_M));

	/* Wait for the PFR to complete. The wait time is the global config lock
	 * timeout plus the PFR timeout which will account for a possible reset
	 * that is occurring during a download package operation.
	 */
	reset_wait_cnt = ICE_PF_RESET_WAIT_COUNT;
	cfg_lock_timeout = ICE_GLOBAL_CFG_LOCK_TIMEOUT;

	for (cnt = 0; cnt < cfg_lock_timeout + reset_wait_cnt; cnt++) {
		reg = rd32(hw, PFGEN_CTRL);
		if (!(reg & PFGEN_CTRL_PFSWR_M))
			break;

		ice_msec_delay(1, true);
	}

	if (cnt == cfg_lock_timeout + reset_wait_cnt) {
		ice_debug(hw, ICE_DBG_INIT, "PF reset polling failed to complete.\n");
		return ICE_ERR_RESET_FAILED;
	}

	return 0;
}

/**
 * ice_reset - Perform different types of reset
 * @hw: pointer to the hardware structure
 * @req: reset request
 *
 * This function triggers a reset as specified by the req parameter.
 *
 * Note:
 * If anything other than a PF reset is triggered, PXE mode is restored.
 * This has to be cleared using ice_clear_pxe_mode again, once the AQ
 * interface has been restored in the rebuild flow.
 */
int ice_reset(struct ice_hw *hw, enum ice_reset_req req)
{
	u32 val = 0;

	switch (req) {
	case ICE_RESET_PFR:
		return ice_pf_reset(hw);
	case ICE_RESET_CORER:
		ice_debug(hw, ICE_DBG_INIT, "CoreR requested\n");
		val = GLGEN_RTRIG_CORER_M;
		break;
	case ICE_RESET_GLOBR:
		ice_debug(hw, ICE_DBG_INIT, "GlobalR requested\n");
		val = GLGEN_RTRIG_GLOBR_M;
		break;
	default:
		return ICE_ERR_PARAM;
	}

	val |= rd32(hw, GLGEN_RTRIG);
	wr32(hw, GLGEN_RTRIG, val);
	ice_flush(hw);

	/* wait for the FW to be ready */
	return ice_check_reset(hw);
}

/**
 * ice_copy_rxq_ctx_to_hw
 * @hw: pointer to the hardware structure
 * @ice_rxq_ctx: pointer to the rxq context
 * @rxq_index: the index of the Rx queue
 *
 * Copies rxq context from dense structure to HW register space
 */
static int
ice_copy_rxq_ctx_to_hw(struct ice_hw *hw, u8 *ice_rxq_ctx, u32 rxq_index)
{
	u8 i;

	if (!ice_rxq_ctx)
		return ICE_ERR_BAD_PTR;

	if (rxq_index > QRX_CTRL_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Copy each dword separately to HW */
	for (i = 0; i < ICE_RXQ_CTX_SIZE_DWORDS; i++) {
		wr32(hw, QRX_CONTEXT(i, rxq_index),
		     *((u32 *)(ice_rxq_ctx + (i * sizeof(u32)))));

		ice_debug(hw, ICE_DBG_QCTX, "qrxdata[%d]: %08X\n", i,
			  *((u32 *)(ice_rxq_ctx + (i * sizeof(u32)))));
	}

	return 0;
}

/**
 * ice_copy_rxq_ctx_from_hw - Copy rxq context register from HW
 * @hw: pointer to the hardware structure
 * @ice_rxq_ctx: pointer to the rxq context
 * @rxq_index: the index of the Rx queue
 *
 * Copies rxq context from HW register space to dense structure
 */
static int
ice_copy_rxq_ctx_from_hw(struct ice_hw *hw, u8 *ice_rxq_ctx, u32 rxq_index)
{
	u8 i;

	if (!ice_rxq_ctx)
		return ICE_ERR_BAD_PTR;

	if (rxq_index > QRX_CTRL_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Copy each dword separately from HW */
	for (i = 0; i < ICE_RXQ_CTX_SIZE_DWORDS; i++) {
		u32 *ctx = (u32 *)(ice_rxq_ctx + (i * sizeof(u32)));

		*ctx = rd32(hw, QRX_CONTEXT(i, rxq_index));

		ice_debug(hw, ICE_DBG_QCTX, "qrxdata[%d]: %08X\n", i, *ctx);
	}

	return 0;
}

/* LAN Rx Queue Context */
static const struct ice_ctx_ele ice_rlan_ctx_info[] = {
	/* Field		Width	LSB */
	ICE_CTX_STORE(ice_rlan_ctx, head,		13,	0),
	ICE_CTX_STORE(ice_rlan_ctx, cpuid,		8,	13),
	ICE_CTX_STORE(ice_rlan_ctx, base,		57,	32),
	ICE_CTX_STORE(ice_rlan_ctx, qlen,		13,	89),
	ICE_CTX_STORE(ice_rlan_ctx, dbuf,		7,	102),
	ICE_CTX_STORE(ice_rlan_ctx, hbuf,		5,	109),
	ICE_CTX_STORE(ice_rlan_ctx, dtype,		2,	114),
	ICE_CTX_STORE(ice_rlan_ctx, dsize,		1,	116),
	ICE_CTX_STORE(ice_rlan_ctx, crcstrip,		1,	117),
	ICE_CTX_STORE(ice_rlan_ctx, l2tsel,		1,	119),
	ICE_CTX_STORE(ice_rlan_ctx, hsplit_0,		4,	120),
	ICE_CTX_STORE(ice_rlan_ctx, hsplit_1,		2,	124),
	ICE_CTX_STORE(ice_rlan_ctx, showiv,		1,	127),
	ICE_CTX_STORE(ice_rlan_ctx, rxmax,		14,	174),
	ICE_CTX_STORE(ice_rlan_ctx, tphrdesc_ena,	1,	193),
	ICE_CTX_STORE(ice_rlan_ctx, tphwdesc_ena,	1,	194),
	ICE_CTX_STORE(ice_rlan_ctx, tphdata_ena,	1,	195),
	ICE_CTX_STORE(ice_rlan_ctx, tphhead_ena,	1,	196),
	ICE_CTX_STORE(ice_rlan_ctx, lrxqthresh,		3,	198),
	ICE_CTX_STORE(ice_rlan_ctx, prefena,		1,	201),
	{ 0 }
};

/**
 * ice_write_rxq_ctx
 * @hw: pointer to the hardware structure
 * @rlan_ctx: pointer to the rxq context
 * @rxq_index: the index of the Rx queue
 *
 * Converts rxq context from sparse to dense structure and then writes
 * it to HW register space and enables the hardware to prefetch descriptors
 * instead of only fetching them on demand
 */
int
ice_write_rxq_ctx(struct ice_hw *hw, struct ice_rlan_ctx *rlan_ctx,
		  u32 rxq_index)
{
	u8 ctx_buf[ICE_RXQ_CTX_SZ] = { 0 };

	if (!rlan_ctx)
		return ICE_ERR_BAD_PTR;

	rlan_ctx->prefena = 1;

	ice_set_ctx(hw, (u8 *)rlan_ctx, ctx_buf, ice_rlan_ctx_info);
	return ice_copy_rxq_ctx_to_hw(hw, ctx_buf, rxq_index);
}

/**
 * ice_read_rxq_ctx - Read rxq context from HW
 * @hw: pointer to the hardware structure
 * @rlan_ctx: pointer to the rxq context
 * @rxq_index: the index of the Rx queue
 *
 * Read rxq context from HW register space and then converts it from dense
 * structure to sparse
 */
int
ice_read_rxq_ctx(struct ice_hw *hw, struct ice_rlan_ctx *rlan_ctx,
		 u32 rxq_index)
{
	u8 ctx_buf[ICE_RXQ_CTX_SZ] = { 0 };
	int status;

	if (!rlan_ctx)
		return ICE_ERR_BAD_PTR;

	status = ice_copy_rxq_ctx_from_hw(hw, ctx_buf, rxq_index);
	if (status)
		return status;

	return ice_get_ctx(ctx_buf, (u8 *)rlan_ctx, ice_rlan_ctx_info);
}

/**
 * ice_clear_rxq_ctx
 * @hw: pointer to the hardware structure
 * @rxq_index: the index of the Rx queue to clear
 *
 * Clears rxq context in HW register space
 */
int ice_clear_rxq_ctx(struct ice_hw *hw, u32 rxq_index)
{
	u8 i;

	if (rxq_index > QRX_CTRL_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Clear each dword register separately */
	for (i = 0; i < ICE_RXQ_CTX_SIZE_DWORDS; i++)
		wr32(hw, QRX_CONTEXT(i, rxq_index), 0);

	return 0;
}

/* LAN Tx Queue Context used for set Tx config by ice_aqc_opc_add_txqs,
 * Bit[0-175] is valid
 */
const struct ice_ctx_ele ice_tlan_ctx_info[] = {
				    /* Field			Width	LSB */
	ICE_CTX_STORE(ice_tlan_ctx, base,			57,	0),
	ICE_CTX_STORE(ice_tlan_ctx, port_num,			3,	57),
	ICE_CTX_STORE(ice_tlan_ctx, cgd_num,			5,	60),
	ICE_CTX_STORE(ice_tlan_ctx, pf_num,			3,	65),
	ICE_CTX_STORE(ice_tlan_ctx, vmvf_num,			10,	68),
	ICE_CTX_STORE(ice_tlan_ctx, vmvf_type,			2,	78),
	ICE_CTX_STORE(ice_tlan_ctx, src_vsi,			10,	80),
	ICE_CTX_STORE(ice_tlan_ctx, tsyn_ena,			1,	90),
	ICE_CTX_STORE(ice_tlan_ctx, internal_usage_flag,	1,	91),
	ICE_CTX_STORE(ice_tlan_ctx, alt_vlan,			1,	92),
	ICE_CTX_STORE(ice_tlan_ctx, cpuid,			8,	93),
	ICE_CTX_STORE(ice_tlan_ctx, wb_mode,			1,	101),
	ICE_CTX_STORE(ice_tlan_ctx, tphrd_desc,			1,	102),
	ICE_CTX_STORE(ice_tlan_ctx, tphrd,			1,	103),
	ICE_CTX_STORE(ice_tlan_ctx, tphwr_desc,			1,	104),
	ICE_CTX_STORE(ice_tlan_ctx, cmpq_id,			9,	105),
	ICE_CTX_STORE(ice_tlan_ctx, qnum_in_func,		14,	114),
	ICE_CTX_STORE(ice_tlan_ctx, itr_notification_mode,	1,	128),
	ICE_CTX_STORE(ice_tlan_ctx, adjust_prof_id,		6,	129),
	ICE_CTX_STORE(ice_tlan_ctx, qlen,			13,	135),
	ICE_CTX_STORE(ice_tlan_ctx, quanta_prof_idx,		4,	148),
	ICE_CTX_STORE(ice_tlan_ctx, tso_ena,			1,	152),
	ICE_CTX_STORE(ice_tlan_ctx, tso_qnum,			11,	153),
	ICE_CTX_STORE(ice_tlan_ctx, legacy_int,			1,	164),
	ICE_CTX_STORE(ice_tlan_ctx, drop_ena,			1,	165),
	ICE_CTX_STORE(ice_tlan_ctx, cache_prof_idx,		2,	166),
	ICE_CTX_STORE(ice_tlan_ctx, pkt_shaper_prof_idx,	3,	168),
	ICE_CTX_STORE(ice_tlan_ctx, int_q_state,		122,	171),
	{ 0 }
};

/**
 * ice_copy_tx_cmpltnq_ctx_to_hw
 * @hw: pointer to the hardware structure
 * @ice_tx_cmpltnq_ctx: pointer to the Tx completion queue context
 * @tx_cmpltnq_index: the index of the completion queue
 *
 * Copies Tx completion queue context from dense structure to HW register space
 */
static int
ice_copy_tx_cmpltnq_ctx_to_hw(struct ice_hw *hw, u8 *ice_tx_cmpltnq_ctx,
			      u32 tx_cmpltnq_index)
{
	u8 i;

	if (!ice_tx_cmpltnq_ctx)
		return ICE_ERR_BAD_PTR;

	if (tx_cmpltnq_index > GLTCLAN_CQ_CNTX0_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Copy each dword separately to HW */
	for (i = 0; i < ICE_TX_CMPLTNQ_CTX_SIZE_DWORDS; i++) {
		wr32(hw, GLTCLAN_CQ_CNTX(i, tx_cmpltnq_index),
		     *((u32 *)(ice_tx_cmpltnq_ctx + (i * sizeof(u32)))));

		ice_debug(hw, ICE_DBG_QCTX, "cmpltnqdata[%d]: %08X\n", i,
			  *((u32 *)(ice_tx_cmpltnq_ctx + (i * sizeof(u32)))));
	}

	return 0;
}

/* LAN Tx Completion Queue Context */
static const struct ice_ctx_ele ice_tx_cmpltnq_ctx_info[] = {
				       /* Field			Width   LSB */
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, base,			57,	0),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, q_len,		18,	64),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, generation,		1,	96),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, wrt_ptr,		22,	97),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, pf_num,		3,	128),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, vmvf_num,		10,	131),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, vmvf_type,		2,	141),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, tph_desc_wr,		1,	160),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, cpuid,		8,	161),
	ICE_CTX_STORE(ice_tx_cmpltnq_ctx, cmpltn_cache,		512,	192),
	{ 0 }
};

/**
 * ice_write_tx_cmpltnq_ctx
 * @hw: pointer to the hardware structure
 * @tx_cmpltnq_ctx: pointer to the completion queue context
 * @tx_cmpltnq_index: the index of the completion queue
 *
 * Converts completion queue context from sparse to dense structure and then
 * writes it to HW register space
 */
int
ice_write_tx_cmpltnq_ctx(struct ice_hw *hw,
			 struct ice_tx_cmpltnq_ctx *tx_cmpltnq_ctx,
			 u32 tx_cmpltnq_index)
{
	u8 ctx_buf[ICE_TX_CMPLTNQ_CTX_SIZE_DWORDS * sizeof(u32)] = { 0 };

	ice_set_ctx(hw, (u8 *)tx_cmpltnq_ctx, ctx_buf, ice_tx_cmpltnq_ctx_info);
	return ice_copy_tx_cmpltnq_ctx_to_hw(hw, ctx_buf, tx_cmpltnq_index);
}

/**
 * ice_clear_tx_cmpltnq_ctx
 * @hw: pointer to the hardware structure
 * @tx_cmpltnq_index: the index of the completion queue to clear
 *
 * Clears Tx completion queue context in HW register space
 */
int
ice_clear_tx_cmpltnq_ctx(struct ice_hw *hw, u32 tx_cmpltnq_index)
{
	u8 i;

	if (tx_cmpltnq_index > GLTCLAN_CQ_CNTX0_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Clear each dword register separately */
	for (i = 0; i < ICE_TX_CMPLTNQ_CTX_SIZE_DWORDS; i++)
		wr32(hw, GLTCLAN_CQ_CNTX(i, tx_cmpltnq_index), 0);

	return 0;
}

/**
 * ice_copy_tx_drbell_q_ctx_to_hw
 * @hw: pointer to the hardware structure
 * @ice_tx_drbell_q_ctx: pointer to the doorbell queue context
 * @tx_drbell_q_index: the index of the doorbell queue
 *
 * Copies doorbell queue context from dense structure to HW register space
 */
static int
ice_copy_tx_drbell_q_ctx_to_hw(struct ice_hw *hw, u8 *ice_tx_drbell_q_ctx,
			       u32 tx_drbell_q_index)
{
	u8 i;

	if (!ice_tx_drbell_q_ctx)
		return ICE_ERR_BAD_PTR;

	if (tx_drbell_q_index > QTX_COMM_DBLQ_DBELL_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Copy each dword separately to HW */
	for (i = 0; i < ICE_TX_DRBELL_Q_CTX_SIZE_DWORDS; i++) {
		wr32(hw, QTX_COMM_DBLQ_CNTX(i, tx_drbell_q_index),
		     *((u32 *)(ice_tx_drbell_q_ctx + (i * sizeof(u32)))));

		ice_debug(hw, ICE_DBG_QCTX, "tx_drbell_qdata[%d]: %08X\n", i,
			  *((u32 *)(ice_tx_drbell_q_ctx + (i * sizeof(u32)))));
	}

	return 0;
}

/* LAN Tx Doorbell Queue Context info */
static const struct ice_ctx_ele ice_tx_drbell_q_ctx_info[] = {
					/* Field		Width   LSB */
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, base,		57,	0),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, ring_len,		13,	64),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, pf_num,		3,	80),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, vf_num,		8,	84),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, vmvf_type,		2,	94),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, cpuid,		8,	96),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, tph_desc_rd,		1,	104),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, tph_desc_wr,		1,	108),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, db_q_en,		1,	112),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, rd_head,		13,	128),
	ICE_CTX_STORE(ice_tx_drbell_q_ctx, rd_tail,		13,	144),
	{ 0 }
};

/**
 * ice_write_tx_drbell_q_ctx
 * @hw: pointer to the hardware structure
 * @tx_drbell_q_ctx: pointer to the doorbell queue context
 * @tx_drbell_q_index: the index of the doorbell queue
 *
 * Converts doorbell queue context from sparse to dense structure and then
 * writes it to HW register space
 */
int
ice_write_tx_drbell_q_ctx(struct ice_hw *hw,
			  struct ice_tx_drbell_q_ctx *tx_drbell_q_ctx,
			  u32 tx_drbell_q_index)
{
	u8 ctx_buf[ICE_TX_DRBELL_Q_CTX_SIZE_DWORDS * sizeof(u32)] = { 0 };

	ice_set_ctx(hw, (u8 *)tx_drbell_q_ctx, ctx_buf,
		    ice_tx_drbell_q_ctx_info);
	return ice_copy_tx_drbell_q_ctx_to_hw(hw, ctx_buf, tx_drbell_q_index);
}

/**
 * ice_clear_tx_drbell_q_ctx
 * @hw: pointer to the hardware structure
 * @tx_drbell_q_index: the index of the doorbell queue to clear
 *
 * Clears doorbell queue context in HW register space
 */
int
ice_clear_tx_drbell_q_ctx(struct ice_hw *hw, u32 tx_drbell_q_index)
{
	u8 i;

	if (tx_drbell_q_index > QTX_COMM_DBLQ_DBELL_MAX_INDEX)
		return ICE_ERR_PARAM;

	/* Clear each dword register separately */
	for (i = 0; i < ICE_TX_DRBELL_Q_CTX_SIZE_DWORDS; i++)
		wr32(hw, QTX_COMM_DBLQ_CNTX(i, tx_drbell_q_index), 0);

	return 0;
}

/* Sideband Queue command wrappers */

/**
 * ice_get_sbq - returns the right control queue to use for sideband
 * @hw: pointer to the hardware structure
 */
static struct ice_ctl_q_info *ice_get_sbq(struct ice_hw *hw)
{
	if (!ice_is_generic_mac(hw))
		return &hw->adminq;
	return &hw->sbq;
}

/**
 * ice_sbq_send_cmd - send Sideband Queue command to Sideband Queue
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 * @cd: pointer to command details structure
 */
static int
ice_sbq_send_cmd(struct ice_hw *hw, struct ice_sbq_cmd_desc *desc,
		 void *buf, u16 buf_size, struct ice_sq_cd *cd)
{
	return ice_sq_send_cmd(hw, ice_get_sbq(hw), (struct ice_aq_desc *)desc,
			       buf, buf_size, cd);
}

/**
 * ice_sbq_send_cmd_nolock - send Sideband Queue command to Sideband Queue
 *                           but do not lock sq_lock
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 * @cd: pointer to command details structure
 */
static int
ice_sbq_send_cmd_nolock(struct ice_hw *hw, struct ice_sbq_cmd_desc *desc,
			void *buf, u16 buf_size, struct ice_sq_cd *cd)
{
	return ice_sq_send_cmd_nolock(hw, ice_get_sbq(hw),
				      (struct ice_aq_desc *)desc, buf,
				      buf_size, cd);
}

/**
 * ice_sbq_rw_reg_lp - Fill Sideband Queue command, with lock parameter
 * @hw: pointer to the HW struct
 * @in: message info to be filled in descriptor
 * @flag: flag to fill desc structure
 * @lock: true to lock the sq_lock (the usual case); false if the sq_lock has
 *        already been locked at a higher level
 */
int ice_sbq_rw_reg_lp(struct ice_hw *hw, struct ice_sbq_msg_input *in,
		      u16 flag, bool lock)
{
	struct ice_sbq_cmd_desc desc = {0};
	struct ice_sbq_msg_req msg = {0};
	u16 msg_len;
	int status;

	msg_len = sizeof(msg);

	msg.dest_dev = in->dest_dev;
	msg.opcode = in->opcode;
	msg.flags = ICE_SBQ_MSG_FLAGS;
	msg.sbe_fbe = ICE_SBQ_MSG_SBE_FBE;
	msg.msg_addr_low = CPU_TO_LE16(in->msg_addr_low);
	msg.msg_addr_high = CPU_TO_LE32(in->msg_addr_high);

	if (in->opcode)
		msg.data = CPU_TO_LE32(in->data);
	else
		/* data read comes back in completion, so shorten the struct by
		 * sizeof(msg.data)
		 */
		msg_len -= sizeof(msg.data);

	desc.flags = CPU_TO_LE16(flag);
	desc.opcode = CPU_TO_LE16(ice_sbq_opc_neigh_dev_req);
	desc.param0.cmd_len = CPU_TO_LE16(msg_len);
	if (lock)
		status = ice_sbq_send_cmd(hw, &desc, &msg, msg_len, NULL);
	else
		status = ice_sbq_send_cmd_nolock(hw, &desc, &msg, msg_len,
						 NULL);
	if (!status && !in->opcode)
		in->data = LE32_TO_CPU
			(((struct ice_sbq_msg_cmpl *)&msg)->data);
	return status;
}

/**
 * ice_sbq_rw_reg - Fill Sideband Queue command
 * @hw: pointer to the HW struct
 * @in: message info to be filled in descriptor
 * @flag: flag to fill desc structure
 */
int ice_sbq_rw_reg(struct ice_hw *hw, struct ice_sbq_msg_input *in, u16 flag)
{
	return ice_sbq_rw_reg_lp(hw, in, flag, true);
}

/**
 * ice_sbq_lock - Lock the sideband queue's sq_lock
 * @hw: pointer to the HW struct
 */
void ice_sbq_lock(struct ice_hw *hw)
{
	ice_acquire_lock(&ice_get_sbq(hw)->sq_lock);
}

/**
 * ice_sbq_unlock - Unlock the sideband queue's sq_lock
 * @hw: pointer to the HW struct
 */
void ice_sbq_unlock(struct ice_hw *hw)
{
	ice_release_lock(&ice_get_sbq(hw)->sq_lock);
}

/* FW Admin Queue command wrappers */

/**
 * ice_should_retry_sq_send_cmd
 * @opcode: AQ opcode
 *
 * Decide if we should retry the send command routine for the ATQ, depending
 * on the opcode.
 */
static bool ice_should_retry_sq_send_cmd(u16 opcode)
{
	switch (opcode) {
	case ice_aqc_opc_dnl_get_status:
	case ice_aqc_opc_dnl_run:
	case ice_aqc_opc_dnl_call:
	case ice_aqc_opc_dnl_read_sto:
	case ice_aqc_opc_dnl_write_sto:
	case ice_aqc_opc_dnl_set_breakpoints:
	case ice_aqc_opc_dnl_read_log:
	case ice_aqc_opc_get_link_topo:
	case ice_aqc_opc_done_alt_write:
	case ice_aqc_opc_lldp_stop:
	case ice_aqc_opc_lldp_start:
	case ice_aqc_opc_lldp_filter_ctrl:
		return true;
	}

	return false;
}

/**
 * ice_sq_send_cmd_retry - send command to Control Queue (ATQ)
 * @hw: pointer to the HW struct
 * @cq: pointer to the specific Control queue
 * @desc: prefilled descriptor describing the command
 * @buf: buffer to use for indirect commands (or NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (or 0 for direct commands)
 * @cd: pointer to command details structure
 *
 * Retry sending the FW Admin Queue command, multiple times, to the FW Admin
 * Queue if the EBUSY AQ error is returned.
 */
static int
ice_sq_send_cmd_retry(struct ice_hw *hw, struct ice_ctl_q_info *cq,
		      struct ice_aq_desc *desc, void *buf, u16 buf_size,
		      struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc_cpy;
	bool is_cmd_for_retry;
	u8 *buf_cpy = NULL;
	u8 idx = 0;
	u16 opcode;
	int status;

	opcode = LE16_TO_CPU(desc->opcode);
	is_cmd_for_retry = ice_should_retry_sq_send_cmd(opcode);
	ice_memset(&desc_cpy, 0, sizeof(desc_cpy), ICE_NONDMA_MEM);

	if (is_cmd_for_retry) {
		if (buf) {
			buf_cpy = (u8 *)ice_malloc(hw, buf_size);
			if (!buf_cpy)
				return ICE_ERR_NO_MEMORY;
		}

		ice_memcpy(&desc_cpy, desc, sizeof(desc_cpy),
			   ICE_NONDMA_TO_NONDMA);
	}

	do {
		status = ice_sq_send_cmd(hw, cq, desc, buf, buf_size, cd);

		if (!is_cmd_for_retry || !status ||
		    hw->adminq.sq_last_status != ICE_AQ_RC_EBUSY)
			break;

		if (buf_cpy)
			ice_memcpy(buf, buf_cpy, buf_size,
				   ICE_NONDMA_TO_NONDMA);

		ice_memcpy(desc, &desc_cpy, sizeof(desc_cpy),
			   ICE_NONDMA_TO_NONDMA);

		ice_msec_delay(ICE_SQ_SEND_DELAY_TIME_MS, false);

	} while (++idx < ICE_SQ_SEND_MAX_EXECUTE);

	if (buf_cpy)
		ice_free(hw, buf_cpy);

	return status;
}

/**
 * ice_aq_send_cmd - send FW Admin Queue command to FW Admin Queue
 * @hw: pointer to the HW struct
 * @desc: descriptor describing the command
 * @buf: buffer to use for indirect commands (NULL for direct commands)
 * @buf_size: size of buffer for indirect commands (0 for direct commands)
 * @cd: pointer to command details structure
 *
 * Helper function to send FW Admin Queue commands to the FW Admin Queue.
 */
int
ice_aq_send_cmd(struct ice_hw *hw, struct ice_aq_desc *desc, void *buf,
		u16 buf_size, struct ice_sq_cd *cd)
{
	return ice_sq_send_cmd_retry(hw, &hw->adminq, desc, buf, buf_size, cd);
}

/**
 * ice_aq_get_fw_ver
 * @hw: pointer to the HW struct
 * @cd: pointer to command details structure or NULL
 *
 * Get the firmware version (0x0001) from the admin queue commands
 */
int ice_aq_get_fw_ver(struct ice_hw *hw, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_ver *resp;
	struct ice_aq_desc desc;
	int status;

	resp = &desc.params.get_ver;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_ver);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	if (!status) {
		hw->fw_branch = resp->fw_branch;
		hw->fw_maj_ver = resp->fw_major;
		hw->fw_min_ver = resp->fw_minor;
		hw->fw_patch = resp->fw_patch;
		hw->fw_build = LE32_TO_CPU(resp->fw_build);
		hw->api_branch = resp->api_branch;
		hw->api_maj_ver = resp->api_major;
		hw->api_min_ver = resp->api_minor;
		hw->api_patch = resp->api_patch;
	}

	return status;
}

/**
 * ice_aq_send_driver_ver
 * @hw: pointer to the HW struct
 * @dv: driver's major, minor version
 * @cd: pointer to command details structure or NULL
 *
 * Send the driver version (0x0002) to the firmware
 */
int
ice_aq_send_driver_ver(struct ice_hw *hw, struct ice_driver_ver *dv,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_driver_ver *cmd;
	struct ice_aq_desc desc;
	u16 len;

	cmd = &desc.params.driver_ver;

	if (!dv)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_driver_ver);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	cmd->major_ver = dv->major_ver;
	cmd->minor_ver = dv->minor_ver;
	cmd->build_ver = dv->build_ver;
	cmd->subbuild_ver = dv->subbuild_ver;

	len = 0;
	while (len < sizeof(dv->driver_string) &&
	       IS_ASCII(dv->driver_string[len]) && dv->driver_string[len])
		len++;

	return ice_aq_send_cmd(hw, &desc, dv->driver_string, len, cd);
}

/**
 * ice_aq_q_shutdown
 * @hw: pointer to the HW struct
 * @unloading: is the driver unloading itself
 *
 * Tell the Firmware that we're shutting down the AdminQ and whether
 * or not the driver is unloading as well (0x0003).
 */
int ice_aq_q_shutdown(struct ice_hw *hw, bool unloading)
{
	struct ice_aqc_q_shutdown *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.q_shutdown;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_q_shutdown);

	if (unloading)
		cmd->driver_unloading = ICE_AQC_DRIVER_UNLOADING;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_aq_req_res
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @access: access type
 * @sdp_number: resource number
 * @timeout: the maximum time in ms that the driver may hold the resource
 * @cd: pointer to command details structure or NULL
 *
 * Requests common resource using the admin queue commands (0x0008).
 * When attempting to acquire the Global Config Lock, the driver can
 * learn of three states:
 *  1) 0 - acquired lock, and can perform download package
 *  2) ICE_ERR_AQ_ERROR - did not get lock, driver should fail to load
 *  3) ICE_ERR_AQ_NO_WORK - did not get lock, but another driver has
 *                          successfully downloaded the package; the driver does
 *                          not have to download the package and can continue
 *                          loading
 *
 * Note that if the caller is in an acquire lock, perform action, release lock
 * phase of operation, it is possible that the FW may detect a timeout and issue
 * a CORER. In this case, the driver will receive a CORER interrupt and will
 * have to determine its cause. The calling thread that is handling this flow
 * will likely get an error propagated back to it indicating the Download
 * Package, Update Package or the Release Resource AQ commands timed out.
 */
static int
ice_aq_req_res(struct ice_hw *hw, enum ice_aq_res_ids res,
	       enum ice_aq_res_access_type access, u8 sdp_number, u32 *timeout,
	       struct ice_sq_cd *cd)
{
	struct ice_aqc_req_res *cmd_resp;
	struct ice_aq_desc desc;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd_resp = &desc.params.res_owner;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_req_res);

	cmd_resp->res_id = CPU_TO_LE16(res);
	cmd_resp->access_type = CPU_TO_LE16(access);
	cmd_resp->res_number = CPU_TO_LE32(sdp_number);
	cmd_resp->timeout = CPU_TO_LE32(*timeout);
	*timeout = 0;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	/* The completion specifies the maximum time in ms that the driver
	 * may hold the resource in the Timeout field.
	 */

	/* Global config lock response utilizes an additional status field.
	 *
	 * If the Global config lock resource is held by some other driver, the
	 * command completes with ICE_AQ_RES_GLBL_IN_PROG in the status field
	 * and the timeout field indicates the maximum time the current owner
	 * of the resource has to free it.
	 */
	if (res == ICE_GLOBAL_CFG_LOCK_RES_ID) {
		if (LE16_TO_CPU(cmd_resp->status) == ICE_AQ_RES_GLBL_SUCCESS) {
			*timeout = LE32_TO_CPU(cmd_resp->timeout);
			return 0;
		} else if (LE16_TO_CPU(cmd_resp->status) ==
			   ICE_AQ_RES_GLBL_IN_PROG) {
			*timeout = LE32_TO_CPU(cmd_resp->timeout);
			return ICE_ERR_AQ_ERROR;
		} else if (LE16_TO_CPU(cmd_resp->status) ==
			   ICE_AQ_RES_GLBL_DONE) {
			return ICE_ERR_AQ_NO_WORK;
		}

		/* invalid FW response, force a timeout immediately */
		*timeout = 0;
		return ICE_ERR_AQ_ERROR;
	}

	/* If the resource is held by some other driver, the command completes
	 * with a busy return value and the timeout field indicates the maximum
	 * time the current owner of the resource has to free it.
	 */
	if (!status || hw->adminq.sq_last_status == ICE_AQ_RC_EBUSY)
		*timeout = LE32_TO_CPU(cmd_resp->timeout);

	return status;
}

/**
 * ice_aq_release_res
 * @hw: pointer to the HW struct
 * @res: resource ID
 * @sdp_number: resource number
 * @cd: pointer to command details structure or NULL
 *
 * release common resource using the admin queue commands (0x0009)
 */
static int
ice_aq_release_res(struct ice_hw *hw, enum ice_aq_res_ids res, u8 sdp_number,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_req_res *cmd;
	struct ice_aq_desc desc;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.res_owner;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_release_res);

	cmd->res_id = CPU_TO_LE16(res);
	cmd->res_number = CPU_TO_LE32(sdp_number);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_acquire_res
 * @hw: pointer to the HW structure
 * @res: resource ID
 * @access: access type (read or write)
 * @timeout: timeout in milliseconds
 *
 * This function will attempt to acquire the ownership of a resource.
 */
int
ice_acquire_res(struct ice_hw *hw, enum ice_aq_res_ids res,
		enum ice_aq_res_access_type access, u32 timeout)
{
#define ICE_RES_POLLING_DELAY_MS	10
	u32 delay = ICE_RES_POLLING_DELAY_MS;
	u32 time_left = timeout;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_aq_req_res(hw, res, access, 0, &time_left, NULL);

	/* A return code of ICE_ERR_AQ_NO_WORK means that another driver has
	 * previously acquired the resource and performed any necessary updates;
	 * in this case the caller does not obtain the resource and has no
	 * further work to do.
	 */
	if (status == ICE_ERR_AQ_NO_WORK)
		goto ice_acquire_res_exit;

	if (status)
		ice_debug(hw, ICE_DBG_RES, "resource %d acquire type %d failed.\n", res, access);

	/* If necessary, poll until the current lock owner timeouts */
	timeout = time_left;
	while (status && timeout && time_left) {
		ice_msec_delay(delay, true);
		timeout = (timeout > delay) ? timeout - delay : 0;
		status = ice_aq_req_res(hw, res, access, 0, &time_left, NULL);

		if (status == ICE_ERR_AQ_NO_WORK)
			/* lock free, but no work to do */
			break;

		if (!status)
			/* lock acquired */
			break;
	}
	if (status && status != ICE_ERR_AQ_NO_WORK)
		ice_debug(hw, ICE_DBG_RES, "resource acquire timed out.\n");

ice_acquire_res_exit:
	if (status == ICE_ERR_AQ_NO_WORK) {
		if (access == ICE_RES_WRITE)
			ice_debug(hw, ICE_DBG_RES, "resource indicates no work to do.\n");
		else
			ice_debug(hw, ICE_DBG_RES, "Warning: ICE_ERR_AQ_NO_WORK not expected\n");
	}
	return status;
}

/**
 * ice_release_res
 * @hw: pointer to the HW structure
 * @res: resource ID
 *
 * This function will release a resource using the proper Admin Command.
 */
void ice_release_res(struct ice_hw *hw, enum ice_aq_res_ids res)
{
	u32 total_delay = 0;
	int status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	status = ice_aq_release_res(hw, res, 0, NULL);

	/* there are some rare cases when trying to release the resource
	 * results in an admin queue timeout, so handle them correctly
	 */
	while ((status == ICE_ERR_AQ_TIMEOUT) &&
	       (total_delay < hw->adminq.sq_cmd_timeout)) {
		ice_msec_delay(1, true);
		status = ice_aq_release_res(hw, res, 0, NULL);
		total_delay++;
	}
}

/**
 * ice_aq_alloc_free_res - command to allocate/free resources
 * @hw: pointer to the HW struct
 * @num_entries: number of resource entries in buffer
 * @buf: Indirect buffer to hold data parameters and response
 * @buf_size: size of buffer for indirect commands
 * @opc: pass in the command opcode
 * @cd: pointer to command details structure or NULL
 *
 * Helper function to allocate/free resources using the admin queue commands
 */
int
ice_aq_alloc_free_res(struct ice_hw *hw, u16 num_entries,
		      struct ice_aqc_alloc_free_res_elem *buf, u16 buf_size,
		      enum ice_adminq_opc opc, struct ice_sq_cd *cd)
{
	struct ice_aqc_alloc_free_res_cmd *cmd;
	struct ice_aq_desc desc;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.sw_res_ctrl;

	if (!buf)
		return ICE_ERR_PARAM;

	if (buf_size < FLEX_ARRAY_SIZE(buf, elem, num_entries))
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->num_entries = CPU_TO_LE16(num_entries);

	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_alloc_hw_res - allocate resource
 * @hw: pointer to the HW struct
 * @type: type of resource
 * @num: number of resources to allocate
 * @btm: allocate from bottom
 * @res: pointer to array that will receive the resources
 */
int
ice_alloc_hw_res(struct ice_hw *hw, u16 type, u16 num, bool btm, u16 *res)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	u16 buf_len;
	int status;

	buf_len = ice_struct_size(buf, elem, num);
	buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer to allocate resource. */
	buf->num_elems = CPU_TO_LE16(num);
	buf->res_type = CPU_TO_LE16(type | ICE_AQC_RES_TYPE_FLAG_DEDICATED |
				    ICE_AQC_RES_TYPE_FLAG_IGNORE_INDEX);
	if (btm)
		buf->res_type |= CPU_TO_LE16(ICE_AQC_RES_TYPE_FLAG_SCAN_BOTTOM);

	status = ice_aq_alloc_free_res(hw, 1, buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (status)
		goto ice_alloc_res_exit;

	ice_memcpy(res, buf->elem, sizeof(*buf->elem) * num,
		   ICE_NONDMA_TO_NONDMA);

ice_alloc_res_exit:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_free_hw_res - free allocated HW resource
 * @hw: pointer to the HW struct
 * @type: type of resource to free
 * @num: number of resources
 * @res: pointer to array that contains the resources to free
 */
int ice_free_hw_res(struct ice_hw *hw, u16 type, u16 num, u16 *res)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	u16 buf_len;
	int status;

	buf_len = ice_struct_size(buf, elem, num);
	buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer to free resource. */
	buf->num_elems = CPU_TO_LE16(num);
	buf->res_type = CPU_TO_LE16(type);
	ice_memcpy(buf->elem, res, sizeof(*buf->elem) * num,
		   ICE_NONDMA_TO_NONDMA);

	status = ice_aq_alloc_free_res(hw, num, buf, buf_len,
				       ice_aqc_opc_free_res, NULL);
	if (status)
		ice_debug(hw, ICE_DBG_SW, "CQ CMD Buffer:\n");

	ice_free(hw, buf);
	return status;
}

/**
 * ice_get_num_per_func - determine number of resources per PF
 * @hw: pointer to the HW structure
 * @max: value to be evenly split between each PF
 *
 * Determine the number of valid functions by going through the bitmap returned
 * from parsing capabilities and use this to calculate the number of resources
 * per PF based on the max value passed in.
 */
static u32 ice_get_num_per_func(struct ice_hw *hw, u32 max)
{
	u8 funcs;

#define ICE_CAPS_VALID_FUNCS_M	0xFF
	funcs = ice_hweight8(hw->dev_caps.common_cap.valid_functions &
			     ICE_CAPS_VALID_FUNCS_M);

	if (!funcs)
		return 0;

	return max / funcs;
}

/**
 * ice_print_led_caps - print LED capabilities
 * @hw: pointer to the ice_hw instance
 * @caps: pointer to common caps instance
 * @prefix: string to prefix when printing
 * @dbg: set to indicate debug print
 */
static void
ice_print_led_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps,
		   char const *prefix, bool dbg)
{
	u8 i;

	if (dbg)
		ice_debug(hw, ICE_DBG_INIT, "%s: led_pin_num = %u\n", prefix,
			  caps->led_pin_num);
	else
		ice_info(hw, "%s: led_pin_num = %u\n", prefix,
			 caps->led_pin_num);

	for (i = 0; i < ICE_MAX_SUPPORTED_GPIO_LED; i++) {
		if (!caps->led[i])
			continue;

		if (dbg)
			ice_debug(hw, ICE_DBG_INIT, "%s: led[%u] = %u\n",
				  prefix, i, caps->led[i]);
		else
			ice_info(hw, "%s: led[%u] = %u\n", prefix, i,
				 caps->led[i]);
	}
}

/**
 * ice_print_sdp_caps - print SDP capabilities
 * @hw: pointer to the ice_hw instance
 * @caps: pointer to common caps instance
 * @prefix: string to prefix when printing
 * @dbg: set to indicate debug print
 */
static void
ice_print_sdp_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps,
		   char const *prefix, bool dbg)
{
	u8 i;

	if (dbg)
		ice_debug(hw, ICE_DBG_INIT, "%s: sdp_pin_num = %u\n", prefix,
			  caps->sdp_pin_num);
	else
		ice_info(hw, "%s: sdp_pin_num = %u\n", prefix,
			 caps->sdp_pin_num);

	for (i = 0; i < ICE_MAX_SUPPORTED_GPIO_SDP; i++) {
		if (!caps->sdp[i])
			continue;

		if (dbg)
			ice_debug(hw, ICE_DBG_INIT, "%s: sdp[%u] = %u\n",
				  prefix, i, caps->sdp[i]);
		else
			ice_info(hw, "%s: sdp[%u] = %u\n", prefix,
				 i, caps->sdp[i]);
	}
}

/**
 * ice_parse_common_caps - parse common device/function capabilities
 * @hw: pointer to the HW struct
 * @caps: pointer to common capabilities structure
 * @elem: the capability element to parse
 * @prefix: message prefix for tracing capabilities
 *
 * Given a capability element, extract relevant details into the common
 * capability structure.
 *
 * Returns: true if the capability matches one of the common capability ids,
 * false otherwise.
 */
static bool
ice_parse_common_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps,
		      struct ice_aqc_list_caps_elem *elem, const char *prefix)
{
	u32 logical_id = LE32_TO_CPU(elem->logical_id);
	u32 phys_id = LE32_TO_CPU(elem->phys_id);
	u32 number = LE32_TO_CPU(elem->number);
	u16 cap = LE16_TO_CPU(elem->cap);
	bool found = true;

	switch (cap) {
	case ICE_AQC_CAPS_SWITCHING_MODE:
		caps->switching_mode = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: switching_mode = %u\n", prefix,
			  caps->switching_mode);
		break;
	case ICE_AQC_CAPS_MANAGEABILITY_MODE:
		caps->mgmt_mode = number;
		caps->mgmt_protocols_mctp = logical_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: mgmt_mode = %u\n", prefix,
			  caps->mgmt_mode);
		ice_debug(hw, ICE_DBG_INIT, "%s: mgmt_protocols_mctp = %u\n", prefix,
			  caps->mgmt_protocols_mctp);
		break;
	case ICE_AQC_CAPS_OS2BMC:
		caps->os2bmc = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: os2bmc = %u\n", prefix, caps->os2bmc);
		break;
	case ICE_AQC_CAPS_VALID_FUNCTIONS:
		caps->valid_functions = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: valid_functions (bitmap) = 0x%x\n", prefix,
			  caps->valid_functions);
		break;
	case ICE_AQC_CAPS_SRIOV:
		caps->sr_iov_1_1 = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: sr_iov_1_1 = %u\n", prefix,
			  caps->sr_iov_1_1);
		break;
	case ICE_AQC_CAPS_VMDQ:
		caps->vmdq = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: vmdq = %u\n", prefix, caps->vmdq);
		break;
	case ICE_AQC_CAPS_802_1QBG:
		caps->evb_802_1_qbg = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: evb_802_1_qbg = %u\n", prefix, number);
		break;
	case ICE_AQC_CAPS_802_1BR:
		caps->evb_802_1_qbh = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: evb_802_1_qbh = %u\n", prefix, number);
		break;
	case ICE_AQC_CAPS_DCB:
		caps->dcb = (number == 1);
		caps->active_tc_bitmap = logical_id;
		caps->maxtc = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: dcb = %u\n", prefix, caps->dcb);
		ice_debug(hw, ICE_DBG_INIT, "%s: active_tc_bitmap = 0x%x\n", prefix,
			  caps->active_tc_bitmap);
		ice_debug(hw, ICE_DBG_INIT, "%s: maxtc = %u\n", prefix, caps->maxtc);
		break;
	case ICE_AQC_CAPS_ISCSI:
		caps->iscsi = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: iscsi = %u\n", prefix, caps->iscsi);
		break;
	case ICE_AQC_CAPS_RSS:
		caps->rss_table_size = number;
		caps->rss_table_entry_width = logical_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: rss_table_size = %u\n", prefix,
			  caps->rss_table_size);
		ice_debug(hw, ICE_DBG_INIT, "%s: rss_table_entry_width = %u\n", prefix,
			  caps->rss_table_entry_width);
		break;
	case ICE_AQC_CAPS_RXQS:
		caps->num_rxq = number;
		caps->rxq_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_rxq = %u\n", prefix,
			  caps->num_rxq);
		ice_debug(hw, ICE_DBG_INIT, "%s: rxq_first_id = %u\n", prefix,
			  caps->rxq_first_id);
		break;
	case ICE_AQC_CAPS_TXQS:
		caps->num_txq = number;
		caps->txq_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_txq = %u\n", prefix,
			  caps->num_txq);
		ice_debug(hw, ICE_DBG_INIT, "%s: txq_first_id = %u\n", prefix,
			  caps->txq_first_id);
		break;
	case ICE_AQC_CAPS_MSIX:
		caps->num_msix_vectors = number;
		caps->msix_vector_first_id = phys_id;
		ice_debug(hw, ICE_DBG_INIT, "%s: num_msix_vectors = %u\n", prefix,
			  caps->num_msix_vectors);
		ice_debug(hw, ICE_DBG_INIT, "%s: msix_vector_first_id = %u\n", prefix,
			  caps->msix_vector_first_id);
		break;
	case ICE_AQC_CAPS_NVM_MGMT:
		caps->sec_rev_disabled =
			(number & ICE_NVM_MGMT_SEC_REV_DISABLED) ?
			true : false;
		ice_debug(hw, ICE_DBG_INIT, "%s: sec_rev_disabled = %d\n", prefix,
			  caps->sec_rev_disabled);
		caps->update_disabled =
			(number & ICE_NVM_MGMT_UPDATE_DISABLED) ?
			true : false;
		ice_debug(hw, ICE_DBG_INIT, "%s: update_disabled = %d\n", prefix,
			  caps->update_disabled);
		caps->nvm_unified_update =
			(number & ICE_NVM_MGMT_UNIFIED_UPD_SUPPORT) ?
			true : false;
		ice_debug(hw, ICE_DBG_INIT, "%s: nvm_unified_update = %d\n", prefix,
			  caps->nvm_unified_update);
		caps->netlist_auth =
			(number & ICE_NVM_MGMT_NETLIST_AUTH_SUPPORT) ?
			true : false;
		ice_debug(hw, ICE_DBG_INIT, "%s: netlist_auth = %d\n", prefix,
			  caps->netlist_auth);
		break;
	case ICE_AQC_CAPS_CEM:
		caps->mgmt_cem = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: mgmt_cem = %u\n", prefix,
			  caps->mgmt_cem);
		break;
	case ICE_AQC_CAPS_IWARP:
		caps->iwarp = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: iwarp = %u\n", prefix, caps->iwarp);
		break;
	case ICE_AQC_CAPS_ROCEV2_LAG:
		caps->roce_lag = !!(number & ICE_AQC_BIT_ROCEV2_LAG);
		ice_debug(hw, ICE_DBG_INIT, "%s: roce_lag = %u\n",
			  prefix, caps->roce_lag);
		break;
	case ICE_AQC_CAPS_LED:
		if (phys_id < ICE_MAX_SUPPORTED_GPIO_LED) {
			caps->led[phys_id] = true;
			caps->led_pin_num++;
			ice_debug(hw, ICE_DBG_INIT, "%s: led[%u] = 1\n", prefix, phys_id);
		}
		break;
	case ICE_AQC_CAPS_SDP:
		if (phys_id < ICE_MAX_SUPPORTED_GPIO_SDP) {
			caps->sdp[phys_id] = true;
			caps->sdp_pin_num++;
			ice_debug(hw, ICE_DBG_INIT, "%s: sdp[%u] = 1\n", prefix, phys_id);
		}
		break;
	case ICE_AQC_CAPS_WR_CSR_PROT:
		caps->wr_csr_prot = number;
		caps->wr_csr_prot |= (u64)logical_id << 32;
		ice_debug(hw, ICE_DBG_INIT, "%s: wr_csr_prot = 0x%llX\n", prefix,
			  (unsigned long long)caps->wr_csr_prot);
		break;
	case ICE_AQC_CAPS_WOL_PROXY:
		caps->num_wol_proxy_fltr = number;
		caps->wol_proxy_vsi_seid = logical_id;
		caps->apm_wol_support = !!(phys_id & ICE_WOL_SUPPORT_M);
		caps->acpi_prog_mthd = !!(phys_id &
					  ICE_ACPI_PROG_MTHD_M);
		caps->proxy_support = !!(phys_id & ICE_PROXY_SUPPORT_M);
		ice_debug(hw, ICE_DBG_INIT, "%s: num_wol_proxy_fltr = %u\n", prefix,
			  caps->num_wol_proxy_fltr);
		ice_debug(hw, ICE_DBG_INIT, "%s: wol_proxy_vsi_seid = %u\n", prefix,
			  caps->wol_proxy_vsi_seid);
		ice_debug(hw, ICE_DBG_INIT, "%s: apm_wol_support = %u\n",
			  prefix, caps->apm_wol_support);
		break;
	case ICE_AQC_CAPS_MAX_MTU:
		caps->max_mtu = number;
		ice_debug(hw, ICE_DBG_INIT, "%s: max_mtu = %u\n",
			  prefix, caps->max_mtu);
		break;
	case ICE_AQC_CAPS_PCIE_RESET_AVOIDANCE:
		caps->pcie_reset_avoidance = (number > 0);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: pcie_reset_avoidance = %d\n", prefix,
			  caps->pcie_reset_avoidance);
		break;
	case ICE_AQC_CAPS_POST_UPDATE_RESET_RESTRICT:
		caps->reset_restrict_support = (number == 1);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: reset_restrict_support = %d\n", prefix,
			  caps->reset_restrict_support);
		break;
	case ICE_AQC_CAPS_EXT_TOPO_DEV_IMG0:
	case ICE_AQC_CAPS_EXT_TOPO_DEV_IMG1:
	case ICE_AQC_CAPS_EXT_TOPO_DEV_IMG2:
	case ICE_AQC_CAPS_EXT_TOPO_DEV_IMG3:
	{
		u8 index = (u8)(cap - ICE_AQC_CAPS_EXT_TOPO_DEV_IMG0);

		caps->ext_topo_dev_img_ver_high[index] = number;
		caps->ext_topo_dev_img_ver_low[index] = logical_id;
		caps->ext_topo_dev_img_part_num[index] =
			(phys_id & ICE_EXT_TOPO_DEV_IMG_PART_NUM_M) >>
			ICE_EXT_TOPO_DEV_IMG_PART_NUM_S;
		caps->ext_topo_dev_img_load_en[index] =
			(phys_id & ICE_EXT_TOPO_DEV_IMG_LOAD_EN) != 0;
		caps->ext_topo_dev_img_prog_en[index] =
			(phys_id & ICE_EXT_TOPO_DEV_IMG_PROG_EN) != 0;
		caps->ext_topo_dev_img_ver_schema[index] =
			(phys_id & ICE_EXT_TOPO_DEV_IMG_VER_SCHEMA) != 0;
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_ver_high[%d] = %u\n",
			  prefix, index,
			  caps->ext_topo_dev_img_ver_high[index]);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_ver_low[%d] = %u\n",
			  prefix, index,
			  caps->ext_topo_dev_img_ver_low[index]);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_part_num[%d] = %u\n",
			  prefix, index,
			  caps->ext_topo_dev_img_part_num[index]);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_load_en[%d] = %d\n",
			  prefix, index,
			  caps->ext_topo_dev_img_load_en[index]);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_prog_en[%d] = %d\n",
			  prefix, index,
			  caps->ext_topo_dev_img_prog_en[index]);
		ice_debug(hw, ICE_DBG_INIT,
			  "%s: ext_topo_dev_img_ver_schema[%d] = %d\n",
			  prefix, index,
			  caps->ext_topo_dev_img_ver_schema[index]);
		break;
	}
	case ICE_AQC_CAPS_TX_SCHED_TOPO_COMP_MODE:
		caps->tx_sched_topo_comp_mode_en = (number == 1);
		break;
	case ICE_AQC_CAPS_DYN_FLATTENING:
		caps->dyn_flattening_en = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: dyn_flattening_en = %d\n",
			  prefix, caps->dyn_flattening_en);
		break;
	case ICE_AQC_CAPS_OROM_RECOVERY_UPDATE:
		caps->orom_recovery_update = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: orom_recovery_update = %d\n",
			  prefix, caps->orom_recovery_update);
		break;
	case ICE_AQC_CAPS_NEXT_CLUSTER_ID:
		caps->next_cluster_id_support = (number == 1);
		ice_debug(hw, ICE_DBG_INIT, "%s: next_cluster_id_support = %d\n",
			  prefix, caps->next_cluster_id_support);
		break;
	default:
		/* Not one of the recognized common capabilities */
		found = false;
	}

	return found;
}

/**
 * ice_recalc_port_limited_caps - Recalculate port limited capabilities
 * @hw: pointer to the HW structure
 * @caps: pointer to capabilities structure to fix
 *
 * Re-calculate the capabilities that are dependent on the number of physical
 * ports; i.e. some features are not supported or function differently on
 * devices with more than 4 ports.
 */
static void
ice_recalc_port_limited_caps(struct ice_hw *hw, struct ice_hw_common_caps *caps)
{
	/* This assumes device capabilities are always scanned before function
	 * capabilities during the initialization flow.
	 */
	if (hw->dev_caps.num_funcs > 4) {
		/* Max 4 TCs per port */
		caps->maxtc = 4;
		ice_debug(hw, ICE_DBG_INIT, "reducing maxtc to %u (based on #ports)\n",
			  caps->maxtc);
		if (caps->iwarp) {
			ice_debug(hw, ICE_DBG_INIT, "forcing RDMA off\n");
			caps->iwarp = 0;
		}

		/* print message only when processing device capabilities
		 * during initialization.
		 */
		if (caps == &hw->dev_caps.common_cap)
			ice_info(hw, "RDMA functionality is not available with the current device configuration.\n");
	}
}

/**
 * ice_parse_vf_func_caps - Parse ICE_AQC_CAPS_VF function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for ICE_AQC_CAPS_VF.
 */
static void
ice_parse_vf_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
		       struct ice_aqc_list_caps_elem *cap)
{
	u32 number = LE32_TO_CPU(cap->number);
	u32 logical_id = LE32_TO_CPU(cap->logical_id);

	func_p->num_allocd_vfs = number;
	func_p->vf_base_id = logical_id;
	ice_debug(hw, ICE_DBG_INIT, "func caps: num_allocd_vfs = %u\n",
		  func_p->num_allocd_vfs);
	ice_debug(hw, ICE_DBG_INIT, "func caps: vf_base_id = %u\n",
		  func_p->vf_base_id);
}

/**
 * ice_parse_vsi_func_caps - Parse ICE_AQC_CAPS_VSI function caps
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @cap: pointer to the capability element to parse
 *
 * Extract function capabilities for ICE_AQC_CAPS_VSI.
 */
static void
ice_parse_vsi_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
			struct ice_aqc_list_caps_elem *cap)
{
	func_p->guar_num_vsi = ice_get_num_per_func(hw, ICE_MAX_VSI);
	ice_debug(hw, ICE_DBG_INIT, "func caps: guar_num_vsi (fw) = %u\n",
		  LE32_TO_CPU(cap->number));
	ice_debug(hw, ICE_DBG_INIT, "func caps: guar_num_vsi = %u\n",
		  func_p->guar_num_vsi);
}

/**
 * ice_parse_func_caps - Parse function capabilities
 * @hw: pointer to the HW struct
 * @func_p: pointer to function capabilities structure
 * @buf: buffer containing the function capability records
 * @cap_count: the number of capabilities
 *
 * Helper function to parse function (0x000A) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ice_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the function capabilities structured.
 */
static void
ice_parse_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_p,
		    void *buf, u32 cap_count)
{
	struct ice_aqc_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ice_aqc_list_caps_elem *)buf;

	ice_memset(func_p, 0, sizeof(*func_p), ICE_NONDMA_MEM);

	for (i = 0; i < cap_count; i++) {
		u16 cap = LE16_TO_CPU(cap_resp[i].cap);
		bool found;

		found = ice_parse_common_caps(hw, &func_p->common_cap,
					      &cap_resp[i], "func caps");

		switch (cap) {
		case ICE_AQC_CAPS_VF:
			ice_parse_vf_func_caps(hw, func_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VSI:
			ice_parse_vsi_func_caps(hw, func_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			if (!found)
				ice_debug(hw, ICE_DBG_INIT, "func caps: unknown capability[%d]: 0x%x\n",
					  i, cap);
			break;
		}
	}

	ice_print_led_caps(hw, &func_p->common_cap, "func caps", true);
	ice_print_sdp_caps(hw, &func_p->common_cap, "func caps", true);

	ice_recalc_port_limited_caps(hw, &func_p->common_cap);
}

/**
 * ice_parse_valid_functions_cap - Parse ICE_AQC_CAPS_VALID_FUNCTIONS caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse ICE_AQC_CAPS_VALID_FUNCTIONS for device capabilities.
 */
static void
ice_parse_valid_functions_cap(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			      struct ice_aqc_list_caps_elem *cap)
{
	u32 number = LE32_TO_CPU(cap->number);

	dev_p->num_funcs = ice_hweight32(number);
	ice_debug(hw, ICE_DBG_INIT, "dev caps: num_funcs = %u\n",
		  dev_p->num_funcs);

}

/**
 * ice_parse_vf_dev_caps - Parse ICE_AQC_CAPS_VF device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse ICE_AQC_CAPS_VF for device capabilities.
 */
static void
ice_parse_vf_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		      struct ice_aqc_list_caps_elem *cap)
{
	u32 number = LE32_TO_CPU(cap->number);

	dev_p->num_vfs_exposed = number;
	ice_debug(hw, ICE_DBG_INIT, "dev_caps: num_vfs_exposed = %u\n",
		  dev_p->num_vfs_exposed);
}

/**
 * ice_parse_vsi_dev_caps - Parse ICE_AQC_CAPS_VSI device caps
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse ICE_AQC_CAPS_VSI for device capabilities.
 */
static void
ice_parse_vsi_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		       struct ice_aqc_list_caps_elem *cap)
{
	u32 number = LE32_TO_CPU(cap->number);

	dev_p->num_vsi_allocd_to_host = number;
	ice_debug(hw, ICE_DBG_INIT, "dev caps: num_vsi_allocd_to_host = %u\n",
		  dev_p->num_vsi_allocd_to_host);
}

/**
 * ice_parse_nac_topo_dev_caps - Parse ICE_AQC_CAPS_NAC_TOPOLOGY cap
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse ICE_AQC_CAPS_NAC_TOPOLOGY for device capabilities.
 */
static void
ice_parse_nac_topo_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			    struct ice_aqc_list_caps_elem *cap)
{
	dev_p->nac_topo.mode = LE32_TO_CPU(cap->number);
	dev_p->nac_topo.id = LE32_TO_CPU(cap->phys_id) & ICE_NAC_TOPO_ID_M;

	ice_info(hw, "PF is configured in %s mode with IP instance ID %u\n",
		 (dev_p->nac_topo.mode & ICE_NAC_TOPO_PRIMARY_M) ?
		 "primary" : "secondary", dev_p->nac_topo.id);

	ice_debug(hw, ICE_DBG_INIT, "dev caps: nac topology is_primary = %d\n",
		  !!(dev_p->nac_topo.mode & ICE_NAC_TOPO_PRIMARY_M));
	ice_debug(hw, ICE_DBG_INIT, "dev caps: nac topology is_dual = %d\n",
		  !!(dev_p->nac_topo.mode & ICE_NAC_TOPO_DUAL_M));
	ice_debug(hw, ICE_DBG_INIT, "dev caps: nac topology id = %u\n",
		  dev_p->nac_topo.id);
}

/**
 * ice_parse_sensor_reading_cap - Parse ICE_AQC_CAPS_SENSOR_READING cap
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @cap: capability element to parse
 *
 * Parse ICE_AQC_CAPS_SENSOR_READING for device capability for reading
 * enabled sensors.
 */
static void
ice_parse_sensor_reading_cap(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
			     struct ice_aqc_list_caps_elem *cap)
{
	dev_p->supported_sensors = LE32_TO_CPU(cap->number);

	ice_debug(hw, ICE_DBG_INIT,
		  "dev caps: supported sensors (bitmap) = 0x%x\n",
		  dev_p->supported_sensors);
}

/**
 * ice_parse_dev_caps - Parse device capabilities
 * @hw: pointer to the HW struct
 * @dev_p: pointer to device capabilities structure
 * @buf: buffer containing the device capability records
 * @cap_count: the number of capabilities
 *
 * Helper device to parse device (0x000B) capabilities list. For
 * capabilities shared between device and function, this relies on
 * ice_parse_common_caps.
 *
 * Loop through the list of provided capabilities and extract the relevant
 * data into the device capabilities structured.
 */
static void
ice_parse_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_p,
		   void *buf, u32 cap_count)
{
	struct ice_aqc_list_caps_elem *cap_resp;
	u32 i;

	cap_resp = (struct ice_aqc_list_caps_elem *)buf;

	ice_memset(dev_p, 0, sizeof(*dev_p), ICE_NONDMA_MEM);

	for (i = 0; i < cap_count; i++) {
		u16 cap = LE16_TO_CPU(cap_resp[i].cap);
		bool found;

		found = ice_parse_common_caps(hw, &dev_p->common_cap,
					      &cap_resp[i], "dev caps");

		switch (cap) {
		case ICE_AQC_CAPS_VALID_FUNCTIONS:
			ice_parse_valid_functions_cap(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VF:
			ice_parse_vf_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_VSI:
			ice_parse_vsi_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_NAC_TOPOLOGY:
			ice_parse_nac_topo_dev_caps(hw, dev_p, &cap_resp[i]);
			break;
		case ICE_AQC_CAPS_SENSOR_READING:
			ice_parse_sensor_reading_cap(hw, dev_p, &cap_resp[i]);
			break;
		default:
			/* Don't list common capabilities as unknown */
			if (!found)
				ice_debug(hw, ICE_DBG_INIT, "dev caps: unknown capability[%u]: 0x%x\n",
					  i, cap);
			break;
		}
	}

	ice_print_led_caps(hw, &dev_p->common_cap, "dev caps", true);
	ice_print_sdp_caps(hw, &dev_p->common_cap, "dev caps", true);

	ice_recalc_port_limited_caps(hw, &dev_p->common_cap);
}

/**
 * ice_aq_get_netlist_node
 * @hw: pointer to the hw struct
 * @cmd: get_link_topo AQ structure
 * @node_part_number: output node part number if node found
 * @node_handle: output node handle parameter if node found
 */
int
ice_aq_get_netlist_node(struct ice_hw *hw, struct ice_aqc_get_link_topo *cmd,
			u8 *node_part_number, u16 *node_handle)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_link_topo);
	desc.params.get_link_topo = *cmd;

	if (ice_aq_send_cmd(hw, &desc, NULL, 0, NULL))
		return ICE_ERR_NOT_SUPPORTED;

	if (node_handle)
		*node_handle =
			LE16_TO_CPU(desc.params.get_link_topo.addr.handle);
	if (node_part_number)
		*node_part_number = desc.params.get_link_topo.node_part_num;

	return 0;
}

#define MAX_NETLIST_SIZE 10
/**
 * ice_find_netlist_node
 * @hw: pointer to the hw struct
 * @node_type_ctx: type of netlist node to look for
 * @node_part_number: node part number to look for
 * @node_handle: output parameter if node found - optional
 *
 * Scan the netlist for a node handle of the given node type and part number.
 *
 * If node_handle is non-NULL it will be modified on function exit. It is only
 * valid if the function returns zero, and should be ignored on any non-zero
 * return value.
 *
 * Returns: 0 if the node is found, ICE_ERR_DOES_NOT_EXIST if no handle was
 * found, and an error code on failure to access the AQ.
 */
int
ice_find_netlist_node(struct ice_hw *hw, u8 node_type_ctx, u8 node_part_number,
		      u16 *node_handle)
{
	u8 idx;

	for (idx = 0; idx < MAX_NETLIST_SIZE; idx++) {
		struct ice_aqc_get_link_topo cmd;
		u8 rec_node_part_number;
		int status;

		memset(&cmd, 0, sizeof(cmd));

		cmd.addr.topo_params.node_type_ctx =
			(node_type_ctx << ICE_AQC_LINK_TOPO_NODE_TYPE_S);
		cmd.addr.topo_params.index = idx;

		status = ice_aq_get_netlist_node(hw, &cmd,
						 &rec_node_part_number,
						 node_handle);
		if (status)
			return status;

		if (rec_node_part_number == node_part_number)
			return 0;
	}

	return ICE_ERR_DOES_NOT_EXIST;
}

/**
 * ice_aq_list_caps - query function/device capabilities
 * @hw: pointer to the HW struct
 * @buf: a buffer to hold the capabilities
 * @buf_size: size of the buffer
 * @cap_count: if not NULL, set to the number of capabilities reported
 * @opc: capabilities type to discover, device or function
 * @cd: pointer to command details structure or NULL
 *
 * Get the function (0x000A) or device (0x000B) capabilities description from
 * firmware and store it in the buffer.
 *
 * If the cap_count pointer is not NULL, then it is set to the number of
 * capabilities firmware will report. Note that if the buffer size is too
 * small, it is possible the command will return ICE_AQ_ERR_ENOMEM. The
 * cap_count will still be updated in this case. It is recommended that the
 * buffer size be set to ICE_AQ_MAX_BUF_LEN (the largest possible buffer that
 * firmware could return) to avoid this.
 */
static int
ice_aq_list_caps(struct ice_hw *hw, void *buf, u16 buf_size, u32 *cap_count,
		 enum ice_adminq_opc opc, struct ice_sq_cd *cd)
{
	struct ice_aqc_list_caps *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.get_cap;

	if (opc != ice_aqc_opc_list_func_caps &&
	    opc != ice_aqc_opc_list_dev_caps)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (cap_count)
		*cap_count = LE32_TO_CPU(cmd->count);

	return status;
}

/**
 * ice_discover_dev_caps - Read and extract device capabilities
 * @hw: pointer to the hardware structure
 * @dev_caps: pointer to device capabilities structure
 *
 * Read the device capabilities and extract them into the dev_caps structure
 * for later use.
 */
static int
ice_discover_dev_caps(struct ice_hw *hw, struct ice_hw_dev_caps *dev_caps)
{
	u32 cap_count = 0;
	void *cbuf;
	int status;

	cbuf = ice_malloc(hw, ICE_AQ_MAX_BUF_LEN);
	if (!cbuf)
		return ICE_ERR_NO_MEMORY;

	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = ICE_AQ_MAX_BUF_LEN / sizeof(struct ice_aqc_list_caps_elem);

	status = ice_aq_list_caps(hw, cbuf, ICE_AQ_MAX_BUF_LEN, &cap_count,
				  ice_aqc_opc_list_dev_caps, NULL);
	if (!status)
		ice_parse_dev_caps(hw, dev_caps, cbuf, cap_count);
	ice_free(hw, cbuf);

	return status;
}

/**
 * ice_discover_func_caps - Read and extract function capabilities
 * @hw: pointer to the hardware structure
 * @func_caps: pointer to function capabilities structure
 *
 * Read the function capabilities and extract them into the func_caps structure
 * for later use.
 */
static int
ice_discover_func_caps(struct ice_hw *hw, struct ice_hw_func_caps *func_caps)
{
	u32 cap_count = 0;
	void *cbuf;
	int status;

	cbuf = ice_malloc(hw, ICE_AQ_MAX_BUF_LEN);
	if (!cbuf)
		return ICE_ERR_NO_MEMORY;

	/* Although the driver doesn't know the number of capabilities the
	 * device will return, we can simply send a 4KB buffer, the maximum
	 * possible size that firmware can return.
	 */
	cap_count = ICE_AQ_MAX_BUF_LEN / sizeof(struct ice_aqc_list_caps_elem);

	status = ice_aq_list_caps(hw, cbuf, ICE_AQ_MAX_BUF_LEN, &cap_count,
				  ice_aqc_opc_list_func_caps, NULL);
	if (!status)
		ice_parse_func_caps(hw, func_caps, cbuf, cap_count);
	ice_free(hw, cbuf);

	return status;
}

/**
 * ice_set_safe_mode_caps - Override dev/func capabilities when in safe mode
 * @hw: pointer to the hardware structure
 */
void ice_set_safe_mode_caps(struct ice_hw *hw)
{
	struct ice_hw_func_caps *func_caps = &hw->func_caps;
	struct ice_hw_dev_caps *dev_caps = &hw->dev_caps;
	struct ice_hw_common_caps cached_caps;
	u32 num_funcs;

	/* cache some func_caps values that should be restored after memset */
	cached_caps = func_caps->common_cap;

	/* unset func capabilities */
	memset(func_caps, 0, sizeof(*func_caps));

#define ICE_RESTORE_FUNC_CAP(name) \
	func_caps->common_cap.name = cached_caps.name

	/* restore cached values */
	ICE_RESTORE_FUNC_CAP(valid_functions);
	ICE_RESTORE_FUNC_CAP(txq_first_id);
	ICE_RESTORE_FUNC_CAP(rxq_first_id);
	ICE_RESTORE_FUNC_CAP(msix_vector_first_id);
	ICE_RESTORE_FUNC_CAP(max_mtu);
	ICE_RESTORE_FUNC_CAP(nvm_unified_update);

	/* one Tx and one Rx queue in safe mode */
	func_caps->common_cap.num_rxq = 1;
	func_caps->common_cap.num_txq = 1;

	/* two MSIX vectors, one for traffic and one for misc causes */
	func_caps->common_cap.num_msix_vectors = 2;
	func_caps->guar_num_vsi = 1;

	/* cache some dev_caps values that should be restored after memset */
	cached_caps = dev_caps->common_cap;
	num_funcs = dev_caps->num_funcs;

	/* unset dev capabilities */
	memset(dev_caps, 0, sizeof(*dev_caps));

#define ICE_RESTORE_DEV_CAP(name) \
	dev_caps->common_cap.name = cached_caps.name

	/* restore cached values */
	ICE_RESTORE_DEV_CAP(valid_functions);
	ICE_RESTORE_DEV_CAP(txq_first_id);
	ICE_RESTORE_DEV_CAP(rxq_first_id);
	ICE_RESTORE_DEV_CAP(msix_vector_first_id);
	ICE_RESTORE_DEV_CAP(max_mtu);
	ICE_RESTORE_DEV_CAP(nvm_unified_update);
	dev_caps->num_funcs = num_funcs;

	/* one Tx and one Rx queue per function in safe mode */
	dev_caps->common_cap.num_rxq = num_funcs;
	dev_caps->common_cap.num_txq = num_funcs;

	/* two MSIX vectors per function */
	dev_caps->common_cap.num_msix_vectors = 2 * num_funcs;
}

/**
 * ice_get_caps - get info about the HW
 * @hw: pointer to the hardware structure
 */
int ice_get_caps(struct ice_hw *hw)
{
	int status;

	status = ice_discover_dev_caps(hw, &hw->dev_caps);
	if (status)
		return status;

	return ice_discover_func_caps(hw, &hw->func_caps);
}

/**
 * ice_aq_manage_mac_write - manage MAC address write command
 * @hw: pointer to the HW struct
 * @mac_addr: MAC address to be written as LAA/LAA+WoL/Port address
 * @flags: flags to control write behavior
 * @cd: pointer to command details structure or NULL
 *
 * This function is used to write MAC address to the NVM (0x0108).
 */
int
ice_aq_manage_mac_write(struct ice_hw *hw, const u8 *mac_addr, u8 flags,
			struct ice_sq_cd *cd)
{
	struct ice_aqc_manage_mac_write *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.mac_write;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_manage_mac_write);

	cmd->flags = flags;
	ice_memcpy(cmd->mac_addr, mac_addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_clear_pxe_mode
 * @hw: pointer to the HW struct
 *
 * Tell the firmware that the driver is taking over from PXE (0x0110).
 */
static int ice_aq_clear_pxe_mode(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_pxe_mode);
	desc.params.clear_pxe.rx_cnt = ICE_AQC_CLEAR_PXE_RX_CNT;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_clear_pxe_mode - clear pxe operations mode
 * @hw: pointer to the HW struct
 *
 * Make sure all PXE mode settings are cleared, including things
 * like descriptor fetch/write-back mode.
 */
void ice_clear_pxe_mode(struct ice_hw *hw)
{
	if (ice_check_sq_alive(hw, &hw->adminq))
		ice_aq_clear_pxe_mode(hw);
}

/**
 * ice_aq_set_port_params - set physical port parameters
 * @pi: pointer to the port info struct
 * @bad_frame_vsi: defines the VSI to which bad frames are forwarded
 * @save_bad_pac: if set packets with errors are forwarded to the bad frames VSI
 * @pad_short_pac: if set transmit packets smaller than 60 bytes are padded
 * @double_vlan: if set double VLAN is enabled
 * @cd: pointer to command details structure or NULL
 *
 * Set Physical port parameters (0x0203)
 */
int
ice_aq_set_port_params(struct ice_port_info *pi, u16 bad_frame_vsi,
		       bool save_bad_pac, bool pad_short_pac, bool double_vlan,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_set_port_params *cmd;
	struct ice_hw *hw = pi->hw;
	struct ice_aq_desc desc;
	u16 cmd_flags = 0;

	cmd = &desc.params.set_port_params;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_params);
	cmd->lb_mode = pi->loopback_mode |
	               ICE_AQC_SET_P_PARAMS_LOOPBACK_MODE_VALID;
	cmd->bad_frame_vsi = CPU_TO_LE16(bad_frame_vsi);
	if (save_bad_pac)
		cmd_flags |= ICE_AQC_SET_P_PARAMS_SAVE_BAD_PACKETS;
	if (pad_short_pac)
		cmd_flags |= ICE_AQC_SET_P_PARAMS_PAD_SHORT_PACKETS;
	if (double_vlan)
		cmd_flags |= ICE_AQC_SET_P_PARAMS_DOUBLE_VLAN_ENA;
	cmd->cmd_flags = CPU_TO_LE16(cmd_flags);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_is_100m_speed_supported
 * @hw: pointer to the HW struct
 *
 * returns true if 100M speeds are supported by the device,
 * false otherwise.
 */
bool ice_is_100m_speed_supported(struct ice_hw *hw)
{
	switch (hw->device_id) {
	case ICE_DEV_ID_E822C_SGMII:
	case ICE_DEV_ID_E822L_SGMII:
	case ICE_DEV_ID_E823L_1GBE:
	case ICE_DEV_ID_E823C_SGMII:
		return true;
	default:
		return false;
	}
}

/**
 * ice_get_link_speed_based_on_phy_type - returns link speed
 * @phy_type_low: lower part of phy_type
 * @phy_type_high: higher part of phy_type
 *
 * This helper function will convert an entry in PHY type structure
 * [phy_type_low, phy_type_high] to its corresponding link speed.
 * Note: In the structure of [phy_type_low, phy_type_high], there should
 * be one bit set, as this function will convert one PHY type to its
 * speed.
 * If no bit gets set, ICE_AQ_LINK_SPEED_UNKNOWN will be returned
 * If more than one bit gets set, ICE_AQ_LINK_SPEED_UNKNOWN will be returned
 */
static u16
ice_get_link_speed_based_on_phy_type(u64 phy_type_low, u64 phy_type_high)
{
	u16 speed_phy_type_high = ICE_AQ_LINK_SPEED_UNKNOWN;
	u16 speed_phy_type_low = ICE_AQ_LINK_SPEED_UNKNOWN;

	switch (phy_type_low) {
	case ICE_PHY_TYPE_LOW_100BASE_TX:
	case ICE_PHY_TYPE_LOW_100M_SGMII:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_100MB;
		break;
	case ICE_PHY_TYPE_LOW_1000BASE_T:
	case ICE_PHY_TYPE_LOW_1000BASE_SX:
	case ICE_PHY_TYPE_LOW_1000BASE_LX:
	case ICE_PHY_TYPE_LOW_1000BASE_KX:
	case ICE_PHY_TYPE_LOW_1G_SGMII:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_1000MB;
		break;
	case ICE_PHY_TYPE_LOW_2500BASE_T:
	case ICE_PHY_TYPE_LOW_2500BASE_X:
	case ICE_PHY_TYPE_LOW_2500BASE_KX:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_2500MB;
		break;
	case ICE_PHY_TYPE_LOW_5GBASE_T:
	case ICE_PHY_TYPE_LOW_5GBASE_KR:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_5GB;
		break;
	case ICE_PHY_TYPE_LOW_10GBASE_T:
	case ICE_PHY_TYPE_LOW_10G_SFI_DA:
	case ICE_PHY_TYPE_LOW_10GBASE_SR:
	case ICE_PHY_TYPE_LOW_10GBASE_LR:
	case ICE_PHY_TYPE_LOW_10GBASE_KR_CR1:
	case ICE_PHY_TYPE_LOW_10G_SFI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_10G_SFI_C2C:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_10GB;
		break;
	case ICE_PHY_TYPE_LOW_25GBASE_T:
	case ICE_PHY_TYPE_LOW_25GBASE_CR:
	case ICE_PHY_TYPE_LOW_25GBASE_CR_S:
	case ICE_PHY_TYPE_LOW_25GBASE_CR1:
	case ICE_PHY_TYPE_LOW_25GBASE_SR:
	case ICE_PHY_TYPE_LOW_25GBASE_LR:
	case ICE_PHY_TYPE_LOW_25GBASE_KR:
	case ICE_PHY_TYPE_LOW_25GBASE_KR_S:
	case ICE_PHY_TYPE_LOW_25GBASE_KR1:
	case ICE_PHY_TYPE_LOW_25G_AUI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_25G_AUI_C2C:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_25GB;
		break;
	case ICE_PHY_TYPE_LOW_40GBASE_CR4:
	case ICE_PHY_TYPE_LOW_40GBASE_SR4:
	case ICE_PHY_TYPE_LOW_40GBASE_LR4:
	case ICE_PHY_TYPE_LOW_40GBASE_KR4:
	case ICE_PHY_TYPE_LOW_40G_XLAUI_AOC_ACC:
	case ICE_PHY_TYPE_LOW_40G_XLAUI:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_40GB;
		break;
	case ICE_PHY_TYPE_LOW_50GBASE_CR2:
	case ICE_PHY_TYPE_LOW_50GBASE_SR2:
	case ICE_PHY_TYPE_LOW_50GBASE_LR2:
	case ICE_PHY_TYPE_LOW_50GBASE_KR2:
	case ICE_PHY_TYPE_LOW_50G_LAUI2_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_LAUI2:
	case ICE_PHY_TYPE_LOW_50G_AUI2_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_AUI2:
	case ICE_PHY_TYPE_LOW_50GBASE_CP:
	case ICE_PHY_TYPE_LOW_50GBASE_SR:
	case ICE_PHY_TYPE_LOW_50GBASE_FR:
	case ICE_PHY_TYPE_LOW_50GBASE_LR:
	case ICE_PHY_TYPE_LOW_50GBASE_KR_PAM4:
	case ICE_PHY_TYPE_LOW_50G_AUI1_AOC_ACC:
	case ICE_PHY_TYPE_LOW_50G_AUI1:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_50GB;
		break;
	case ICE_PHY_TYPE_LOW_100GBASE_CR4:
	case ICE_PHY_TYPE_LOW_100GBASE_SR4:
	case ICE_PHY_TYPE_LOW_100GBASE_LR4:
	case ICE_PHY_TYPE_LOW_100GBASE_KR4:
	case ICE_PHY_TYPE_LOW_100G_CAUI4_AOC_ACC:
	case ICE_PHY_TYPE_LOW_100G_CAUI4:
	case ICE_PHY_TYPE_LOW_100G_AUI4_AOC_ACC:
	case ICE_PHY_TYPE_LOW_100G_AUI4:
	case ICE_PHY_TYPE_LOW_100GBASE_CR_PAM4:
	case ICE_PHY_TYPE_LOW_100GBASE_KR_PAM4:
	case ICE_PHY_TYPE_LOW_100GBASE_CP2:
	case ICE_PHY_TYPE_LOW_100GBASE_SR2:
	case ICE_PHY_TYPE_LOW_100GBASE_DR:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_100GB;
		break;
	default:
		speed_phy_type_low = ICE_AQ_LINK_SPEED_UNKNOWN;
		break;
	}

	switch (phy_type_high) {
	case ICE_PHY_TYPE_HIGH_100GBASE_KR2_PAM4:
	case ICE_PHY_TYPE_HIGH_100G_CAUI2_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_100G_CAUI2:
	case ICE_PHY_TYPE_HIGH_100G_AUI2_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_100G_AUI2:
		speed_phy_type_high = ICE_AQ_LINK_SPEED_100GB;
		break;
	case ICE_PHY_TYPE_HIGH_200G_CR4_PAM4:
	case ICE_PHY_TYPE_HIGH_200G_SR4:
	case ICE_PHY_TYPE_HIGH_200G_FR4:
	case ICE_PHY_TYPE_HIGH_200G_LR4:
	case ICE_PHY_TYPE_HIGH_200G_DR4:
	case ICE_PHY_TYPE_HIGH_200G_KR4_PAM4:
	case ICE_PHY_TYPE_HIGH_200G_AUI4_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_200G_AUI4:
	case ICE_PHY_TYPE_HIGH_200G_AUI8_AOC_ACC:
	case ICE_PHY_TYPE_HIGH_200G_AUI8:
		speed_phy_type_high = ICE_AQ_LINK_SPEED_200GB;
		break;
	default:
		speed_phy_type_high = ICE_AQ_LINK_SPEED_UNKNOWN;
		break;
	}

	if (speed_phy_type_low == ICE_AQ_LINK_SPEED_UNKNOWN &&
	    speed_phy_type_high == ICE_AQ_LINK_SPEED_UNKNOWN)
		return ICE_AQ_LINK_SPEED_UNKNOWN;
	else if (speed_phy_type_low != ICE_AQ_LINK_SPEED_UNKNOWN &&
		 speed_phy_type_high != ICE_AQ_LINK_SPEED_UNKNOWN)
		return ICE_AQ_LINK_SPEED_UNKNOWN;
	else if (speed_phy_type_low != ICE_AQ_LINK_SPEED_UNKNOWN &&
		 speed_phy_type_high == ICE_AQ_LINK_SPEED_UNKNOWN)
		return speed_phy_type_low;
	else
		return speed_phy_type_high;
}

/**
 * ice_update_phy_type
 * @phy_type_low: pointer to the lower part of phy_type
 * @phy_type_high: pointer to the higher part of phy_type
 * @link_speeds_bitmap: targeted link speeds bitmap
 *
 * Note: For the link_speeds_bitmap structure, you can check it at
 * [ice_aqc_get_link_status->link_speed]. Caller can pass in
 * link_speeds_bitmap include multiple speeds.
 *
 * Each entry in this [phy_type_low, phy_type_high] structure will
 * present a certain link speed. This helper function will turn on bits
 * in [phy_type_low, phy_type_high] structure based on the value of
 * link_speeds_bitmap input parameter.
 */
void
ice_update_phy_type(u64 *phy_type_low, u64 *phy_type_high,
		    u16 link_speeds_bitmap)
{
	u64 pt_high;
	u64 pt_low;
	int index;
	u16 speed;

	/* We first check with low part of phy_type */
	for (index = 0; index <= ICE_PHY_TYPE_LOW_MAX_INDEX; index++) {
		pt_low = BIT_ULL(index);
		speed = ice_get_link_speed_based_on_phy_type(pt_low, 0);

		if (link_speeds_bitmap & speed)
			*phy_type_low |= BIT_ULL(index);
	}

	/* We then check with high part of phy_type */
	for (index = 0; index <= ICE_PHY_TYPE_HIGH_MAX_INDEX; index++) {
		pt_high = BIT_ULL(index);
		speed = ice_get_link_speed_based_on_phy_type(0, pt_high);

		if (link_speeds_bitmap & speed)
			*phy_type_high |= BIT_ULL(index);
	}
}

/**
 * ice_aq_set_phy_cfg
 * @hw: pointer to the HW struct
 * @pi: port info structure of the interested logical port
 * @cfg: structure with PHY configuration data to be set
 * @cd: pointer to command details structure or NULL
 *
 * Set the various PHY configuration parameters supported on the Port.
 * One or more of the Set PHY config parameters may be ignored in an MFP
 * mode as the PF may not have the privilege to set some of the PHY Config
 * parameters. This status will be indicated by the command response (0x0601).
 */
int
ice_aq_set_phy_cfg(struct ice_hw *hw, struct ice_port_info *pi,
		   struct ice_aqc_set_phy_cfg_data *cfg, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	int status;

	if (!cfg)
		return ICE_ERR_PARAM;

	/* Ensure that only valid bits of cfg->caps can be turned on. */
	if (cfg->caps & ~ICE_AQ_PHY_ENA_VALID_MASK) {
		ice_debug(hw, ICE_DBG_PHY, "Invalid bit is set in ice_aqc_set_phy_cfg_data->caps : 0x%x\n",
			  cfg->caps);

		cfg->caps &= ICE_AQ_PHY_ENA_VALID_MASK;
	}

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_phy_cfg);
	desc.params.set_phy.lport_num = pi->lport;
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	ice_debug(hw, ICE_DBG_LINK, "set phy cfg\n");
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_low = 0x%llx\n",
		  (unsigned long long)LE64_TO_CPU(cfg->phy_type_low));
	ice_debug(hw, ICE_DBG_LINK, "	phy_type_high = 0x%llx\n",
		  (unsigned long long)LE64_TO_CPU(cfg->phy_type_high));
	ice_debug(hw, ICE_DBG_LINK, "	caps = 0x%x\n", cfg->caps);
	ice_debug(hw, ICE_DBG_LINK, "	low_power_ctrl_an = 0x%x\n",
		  cfg->low_power_ctrl_an);
	ice_debug(hw, ICE_DBG_LINK, "	eee_cap = 0x%x\n", cfg->eee_cap);
	ice_debug(hw, ICE_DBG_LINK, "	eeer_value = 0x%x\n", cfg->eeer_value);
	ice_debug(hw, ICE_DBG_LINK, "	link_fec_opt = 0x%x\n",
		  cfg->link_fec_opt);

	status = ice_aq_send_cmd(hw, &desc, cfg, sizeof(*cfg), cd);

	if (hw->adminq.sq_last_status == ICE_AQ_RC_EMODE)
		status = 0;

	if (!status)
		pi->phy.curr_user_phy_cfg = *cfg;

	return status;
}

/**
 * ice_update_link_info - update status of the HW network link
 * @pi: port info structure of the interested logical port
 */
int ice_update_link_info(struct ice_port_info *pi)
{
	struct ice_link_status *li;
	int status;

	if (!pi)
		return ICE_ERR_PARAM;

	li = &pi->phy.link_info;

	status = ice_aq_get_link_info(pi, true, NULL, NULL);
	if (status)
		return status;

	if (li->link_info & ICE_AQ_MEDIA_AVAILABLE) {
		struct ice_aqc_get_phy_caps_data *pcaps;
		struct ice_hw *hw;

		hw = pi->hw;
		pcaps = (struct ice_aqc_get_phy_caps_data *)
			ice_malloc(hw, sizeof(*pcaps));
		if (!pcaps)
			return ICE_ERR_NO_MEMORY;

		status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_MEDIA,
					     pcaps, NULL);

		if (!status)
			ice_memcpy(li->module_type, &pcaps->module_type,
				   sizeof(li->module_type),
				   ICE_NONDMA_TO_NONDMA);

		ice_free(hw, pcaps);
	}

	return status;
}

/**
 * ice_cache_phy_user_req
 * @pi: port information structure
 * @cache_data: PHY logging data
 * @cache_mode: PHY logging mode
 *
 * Log the user request on (FC, FEC, SPEED) for later user.
 */
static void
ice_cache_phy_user_req(struct ice_port_info *pi,
		       struct ice_phy_cache_mode_data cache_data,
		       enum ice_phy_cache_mode cache_mode)
{
	if (!pi)
		return;

	switch (cache_mode) {
	case ICE_FC_MODE:
		pi->phy.curr_user_fc_req = cache_data.data.curr_user_fc_req;
		break;
	case ICE_SPEED_MODE:
		pi->phy.curr_user_speed_req =
			cache_data.data.curr_user_speed_req;
		break;
	case ICE_FEC_MODE:
		pi->phy.curr_user_fec_req = cache_data.data.curr_user_fec_req;
		break;
	default:
		break;
	}
}

/**
 * ice_caps_to_fc_mode
 * @caps: PHY capabilities
 *
 * Convert PHY FC capabilities to ice FC mode
 */
enum ice_fc_mode ice_caps_to_fc_mode(u8 caps)
{
	if (caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE &&
	    caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE)
		return ICE_FC_FULL;

	if (caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE)
		return ICE_FC_TX_PAUSE;

	if (caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE)
		return ICE_FC_RX_PAUSE;

	return ICE_FC_NONE;
}

/**
 * ice_caps_to_fec_mode
 * @caps: PHY capabilities
 * @fec_options: Link FEC options
 *
 * Convert PHY FEC capabilities to ice FEC mode
 */
enum ice_fec_mode ice_caps_to_fec_mode(u8 caps, u8 fec_options)
{
	if (caps & ICE_AQC_PHY_EN_AUTO_FEC) {
		if (fec_options & ICE_AQC_PHY_FEC_DIS)
			return ICE_FEC_DIS_AUTO;
		else
			return ICE_FEC_AUTO;
	}

	if (fec_options & (ICE_AQC_PHY_FEC_10G_KR_40G_KR4_EN |
			   ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ |
			   ICE_AQC_PHY_FEC_25G_KR_CLAUSE74_EN |
			   ICE_AQC_PHY_FEC_25G_KR_REQ))
		return ICE_FEC_BASER;

	if (fec_options & (ICE_AQC_PHY_FEC_25G_RS_528_REQ |
			   ICE_AQC_PHY_FEC_25G_RS_544_REQ |
			   ICE_AQC_PHY_FEC_25G_RS_CLAUSE91_EN))
		return ICE_FEC_RS;

	return ICE_FEC_NONE;
}

/**
 * ice_cfg_phy_fc - Configure PHY FC data based on FC mode
 * @pi: port information structure
 * @cfg: PHY configuration data to set FC mode
 * @req_mode: FC mode to configure
 */
static int
ice_cfg_phy_fc(struct ice_port_info *pi, struct ice_aqc_set_phy_cfg_data *cfg,
	       enum ice_fc_mode req_mode)
{
	struct ice_phy_cache_mode_data cache_data;
	u8 pause_mask = 0x0;

	if (!pi || !cfg)
		return ICE_ERR_BAD_PTR;
	switch (req_mode) {
	case ICE_FC_AUTO:
	{
		struct ice_aqc_get_phy_caps_data *pcaps;
		int status;

		pcaps = (struct ice_aqc_get_phy_caps_data *)
			ice_malloc(pi->hw, sizeof(*pcaps));
		if (!pcaps)
			return ICE_ERR_NO_MEMORY;
		/* Query the value of FC that both the NIC and attached media
		 * can do.
		 */
		status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_TOPO_CAP_MEDIA,
					     pcaps, NULL);
		if (status) {
			ice_free(pi->hw, pcaps);
			return status;
		}

		pause_mask |= pcaps->caps & ICE_AQC_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= pcaps->caps & ICE_AQC_PHY_EN_RX_LINK_PAUSE;

		ice_free(pi->hw, pcaps);
		break;
	}
	case ICE_FC_FULL:
		pause_mask |= ICE_AQC_PHY_EN_TX_LINK_PAUSE;
		pause_mask |= ICE_AQC_PHY_EN_RX_LINK_PAUSE;
		break;
	case ICE_FC_RX_PAUSE:
		pause_mask |= ICE_AQC_PHY_EN_RX_LINK_PAUSE;
		break;
	case ICE_FC_TX_PAUSE:
		pause_mask |= ICE_AQC_PHY_EN_TX_LINK_PAUSE;
		break;
	default:
		break;
	}

	/* clear the old pause settings */
	cfg->caps &= ~(ICE_AQC_PHY_EN_TX_LINK_PAUSE |
		ICE_AQC_PHY_EN_RX_LINK_PAUSE);

	/* set the new capabilities */
	cfg->caps |= pause_mask;

	/* Cache user FC request */
	cache_data.data.curr_user_fc_req = req_mode;
	ice_cache_phy_user_req(pi, cache_data, ICE_FC_MODE);

	return 0;
}

/**
 * ice_set_fc
 * @pi: port information structure
 * @aq_failures: pointer to status code, specific to ice_set_fc routine
 * @ena_auto_link_update: enable automatic link update
 *
 * Set the requested flow control mode.
 */
int
ice_set_fc(struct ice_port_info *pi, u8 *aq_failures, bool ena_auto_link_update)
{
	struct ice_aqc_set_phy_cfg_data  cfg = { 0 };
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_hw *hw;
	int status;

	if (!pi || !aq_failures)
		return ICE_ERR_BAD_PTR;

	*aq_failures = 0;
	hw = pi->hw;

	pcaps = (struct ice_aqc_get_phy_caps_data *)
		ice_malloc(hw, sizeof(*pcaps));
	if (!pcaps)
		return ICE_ERR_NO_MEMORY;

	/* Get the current PHY config */
	status = ice_aq_get_phy_caps(pi, false, ICE_AQC_REPORT_ACTIVE_CFG,
				     pcaps, NULL);

	if (status) {
		*aq_failures = ICE_SET_FC_AQ_FAIL_GET;
		goto out;
	}

	ice_copy_phy_caps_to_cfg(pi, pcaps, &cfg);

	/* Configure the set PHY data */
	status = ice_cfg_phy_fc(pi, &cfg, pi->fc.req_mode);
	if (status) {
		if (status != ICE_ERR_BAD_PTR)
			*aq_failures = ICE_SET_FC_AQ_FAIL_GET;

		goto out;
	}

	/* If the capabilities have changed, then set the new config */
	if (cfg.caps != pcaps->caps) {
		int retry_count, retry_max = 10;

		/* Auto restart link so settings take effect */
		if (ena_auto_link_update)
			cfg.caps |= ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;

		status = ice_aq_set_phy_cfg(hw, pi, &cfg, NULL);
		if (status) {
			*aq_failures = ICE_SET_FC_AQ_FAIL_SET;
			goto out;
		}

		/* Update the link info
		 * It sometimes takes a really long time for link to
		 * come back from the atomic reset. Thus, we wait a
		 * little bit.
		 */
		for (retry_count = 0; retry_count < retry_max; retry_count++) {
			status = ice_update_link_info(pi);

			if (!status)
				break;

			ice_msec_delay(100, true);
		}

		if (status)
			*aq_failures = ICE_SET_FC_AQ_FAIL_UPDATE;
	}

out:
	ice_free(hw, pcaps);
	return status;
}

/**
 * ice_phy_caps_equals_cfg
 * @phy_caps: PHY capabilities
 * @phy_cfg: PHY configuration
 *
 * Helper function to determine if PHY capabilities matches PHY
 * configuration
 */
bool
ice_phy_caps_equals_cfg(struct ice_aqc_get_phy_caps_data *phy_caps,
			struct ice_aqc_set_phy_cfg_data *phy_cfg)
{
	u8 caps_mask, cfg_mask;

	if (!phy_caps || !phy_cfg)
		return false;

	/* These bits are not common between capabilities and configuration.
	 * Do not use them to determine equality.
	 */
	caps_mask = ICE_AQC_PHY_CAPS_MASK & ~(ICE_AQC_PHY_AN_MODE |
					      ICE_AQC_PHY_EN_MOD_QUAL);
	cfg_mask = ICE_AQ_PHY_ENA_VALID_MASK & ~ICE_AQ_PHY_ENA_AUTO_LINK_UPDT;

	if (phy_caps->phy_type_low != phy_cfg->phy_type_low ||
	    phy_caps->phy_type_high != phy_cfg->phy_type_high ||
	    ((phy_caps->caps & caps_mask) != (phy_cfg->caps & cfg_mask)) ||
	    phy_caps->low_power_ctrl_an != phy_cfg->low_power_ctrl_an ||
	    phy_caps->eee_cap != phy_cfg->eee_cap ||
	    phy_caps->eeer_value != phy_cfg->eeer_value ||
	    phy_caps->link_fec_options != phy_cfg->link_fec_opt)
		return false;

	return true;
}

/**
 * ice_copy_phy_caps_to_cfg - Copy PHY ability data to configuration data
 * @pi: port information structure
 * @caps: PHY ability structure to copy data from
 * @cfg: PHY configuration structure to copy data to
 *
 * Helper function to copy AQC PHY get ability data to PHY set configuration
 * data structure
 */
void
ice_copy_phy_caps_to_cfg(struct ice_port_info *pi,
			 struct ice_aqc_get_phy_caps_data *caps,
			 struct ice_aqc_set_phy_cfg_data *cfg)
{
	if (!pi || !caps || !cfg)
		return;

	ice_memset(cfg, 0, sizeof(*cfg), ICE_NONDMA_MEM);
	cfg->phy_type_low = caps->phy_type_low;
	cfg->phy_type_high = caps->phy_type_high;
	cfg->caps = caps->caps;
	cfg->low_power_ctrl_an = caps->low_power_ctrl_an;
	cfg->eee_cap = caps->eee_cap;
	cfg->eeer_value = caps->eeer_value;
	cfg->link_fec_opt = caps->link_fec_options;
	cfg->module_compliance_enforcement =
		caps->module_compliance_enforcement;
}

/**
 * ice_cfg_phy_fec - Configure PHY FEC data based on FEC mode
 * @pi: port information structure
 * @cfg: PHY configuration data to set FEC mode
 * @fec: FEC mode to configure
 */
int
ice_cfg_phy_fec(struct ice_port_info *pi, struct ice_aqc_set_phy_cfg_data *cfg,
		enum ice_fec_mode fec)
{
	struct ice_aqc_get_phy_caps_data *pcaps;
	struct ice_hw *hw;
	int status = 0;

	if (!pi || !cfg)
		return ICE_ERR_BAD_PTR;

	hw = pi->hw;

	pcaps = (struct ice_aqc_get_phy_caps_data *)
		ice_malloc(hw, sizeof(*pcaps));
	if (!pcaps)
		return ICE_ERR_NO_MEMORY;

	status = ice_aq_get_phy_caps(pi, false,
				     (ice_fw_supports_report_dflt_cfg(hw) ?
				      ICE_AQC_REPORT_DFLT_CFG :
				      ICE_AQC_REPORT_TOPO_CAP_MEDIA), pcaps, NULL);

	if (status)
		goto out;

	cfg->caps |= (pcaps->caps & ICE_AQC_PHY_EN_AUTO_FEC);
	cfg->link_fec_opt = pcaps->link_fec_options;

	switch (fec) {
	case ICE_FEC_BASER:
		/* Clear RS bits, and AND BASE-R ability
		 * bits and OR request bits.
		 */
		cfg->link_fec_opt &= ICE_AQC_PHY_FEC_10G_KR_40G_KR4_EN |
			ICE_AQC_PHY_FEC_25G_KR_CLAUSE74_EN;
		cfg->link_fec_opt |= ICE_AQC_PHY_FEC_10G_KR_40G_KR4_REQ |
			ICE_AQC_PHY_FEC_25G_KR_REQ;
		break;
	case ICE_FEC_RS:
		/* Clear BASE-R bits, and AND RS ability
		 * bits and OR request bits.
		 */
		cfg->link_fec_opt &= ICE_AQC_PHY_FEC_25G_RS_CLAUSE91_EN;
		cfg->link_fec_opt |= ICE_AQC_PHY_FEC_25G_RS_528_REQ |
			ICE_AQC_PHY_FEC_25G_RS_544_REQ;
		break;
	case ICE_FEC_NONE:
		/* Clear all FEC option bits. */
		cfg->link_fec_opt &= ~ICE_AQC_PHY_FEC_MASK;
		break;
	case ICE_FEC_DIS_AUTO:
		/* Set No FEC and auto FEC */
		if (!ice_fw_supports_fec_dis_auto(hw)) {
			status = ICE_ERR_NOT_SUPPORTED;
			goto out;
		}
		cfg->link_fec_opt |= ICE_AQC_PHY_FEC_DIS;
		/* fall-through */
	case ICE_FEC_AUTO:
		/* AND auto FEC bit, and all caps bits. */
		cfg->caps &= ICE_AQC_PHY_CAPS_MASK;
		cfg->link_fec_opt |= pcaps->link_fec_options;
		break;
	default:
		status = ICE_ERR_PARAM;
		break;
	}

	if (fec == ICE_FEC_AUTO && ice_fw_supports_link_override(pi->hw) &&
	    !ice_fw_supports_report_dflt_cfg(pi->hw)) {
		struct ice_link_default_override_tlv tlv;

		if (ice_get_link_default_override(&tlv, pi))
			goto out;

		if (!(tlv.options & ICE_LINK_OVERRIDE_STRICT_MODE) &&
		    (tlv.options & ICE_LINK_OVERRIDE_EN))
			cfg->link_fec_opt = tlv.fec_options;
	}

out:
	ice_free(hw, pcaps);

	return status;
}

/**
 * ice_get_link_status - get status of the HW network link
 * @pi: port information structure
 * @link_up: pointer to bool (true/false = linkup/linkdown)
 *
 * Variable link_up is true if link is up, false if link is down.
 * The variable link_up is invalid if status is non zero. As a
 * result of this call, link status reporting becomes enabled
 */
int ice_get_link_status(struct ice_port_info *pi, bool *link_up)
{
	struct ice_phy_info *phy_info;
	int status = 0;

	if (!pi || !link_up)
		return ICE_ERR_PARAM;

	phy_info = &pi->phy;

	if (phy_info->get_link_info) {
		status = ice_update_link_info(pi);

		if (status)
			ice_debug(pi->hw, ICE_DBG_LINK, "get link status error, status = %d\n",
				  status);
	}

	*link_up = phy_info->link_info.link_info & ICE_AQ_LINK_UP;

	return status;
}

/**
 * ice_aq_set_link_restart_an
 * @pi: pointer to the port information structure
 * @ena_link: if true: enable link, if false: disable link
 * @cd: pointer to command details structure or NULL
 *
 * Sets up the link and restarts the Auto-Negotiation over the link.
 */
int
ice_aq_set_link_restart_an(struct ice_port_info *pi, bool ena_link,
			   struct ice_sq_cd *cd)
{
	int status = ICE_ERR_AQ_ERROR;
	struct ice_aqc_restart_an *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.restart_an;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_restart_an);

	cmd->cmd_flags = ICE_AQC_RESTART_AN_LINK_RESTART;
	cmd->lport_num = pi->lport;
	if (ena_link)
		cmd->cmd_flags |= ICE_AQC_RESTART_AN_LINK_ENABLE;
	else
		cmd->cmd_flags &= ~ICE_AQC_RESTART_AN_LINK_ENABLE;

	status = ice_aq_send_cmd(pi->hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	if (ena_link)
		pi->phy.curr_user_phy_cfg.caps |= ICE_AQC_PHY_EN_LINK;
	else
		pi->phy.curr_user_phy_cfg.caps &= ~ICE_AQC_PHY_EN_LINK;

	return 0;
}

/**
 * ice_aq_set_event_mask
 * @hw: pointer to the HW struct
 * @port_num: port number of the physical function
 * @mask: event mask to be set
 * @cd: pointer to command details structure or NULL
 *
 * Set event mask (0x0613)
 */
int
ice_aq_set_event_mask(struct ice_hw *hw, u8 port_num, u16 mask,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_set_event_mask *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_event_mask;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_event_mask);

	cmd->lport_num = port_num;

	cmd->event_mask = CPU_TO_LE16(mask);
	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_set_mac_loopback
 * @hw: pointer to the HW struct
 * @ena_lpbk: Enable or Disable loopback
 * @cd: pointer to command details structure or NULL
 *
 * Enable/disable loopback on a given port
 */
int
ice_aq_set_mac_loopback(struct ice_hw *hw, bool ena_lpbk, struct ice_sq_cd *cd)
{
	struct ice_aqc_set_mac_lb *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_mac_lb;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_mac_lb);
	if (ena_lpbk)
		cmd->lb_mode = ICE_AQ_MAC_LB_EN;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_set_port_id_led
 * @pi: pointer to the port information
 * @is_orig_mode: is this LED set to original mode (by the net-list)
 * @cd: pointer to command details structure or NULL
 *
 * Set LED value for the given port (0x06e9)
 */
int
ice_aq_set_port_id_led(struct ice_port_info *pi, bool is_orig_mode,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_set_port_id_led *cmd;
	struct ice_hw *hw = pi->hw;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_port_id_led;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_id_led);

	if (is_orig_mode)
		cmd->ident_mode = ICE_AQC_PORT_IDENT_LED_ORIG;
	else
		cmd->ident_mode = ICE_AQC_PORT_IDENT_LED_BLINK;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_sff_eeprom
 * @hw: pointer to the HW struct
 * @lport: bits [7:0] = logical port, bit [8] = logical port valid
 * @bus_addr: I2C bus address of the eeprom (typically 0xA0, 0=topo default)
 * @mem_addr: I2C offset. lower 8 bits for address, 8 upper bits zero padding.
 * @page: QSFP page
 * @set_page: set or ignore the page
 * @data: pointer to data buffer to be read/written to the I2C device.
 * @length: 1-16 for read, 1 for write.
 * @write: 0 read, 1 for write.
 * @cd: pointer to command details structure or NULL
 *
 * Read/Write SFF EEPROM (0x06EE)
 */
int
ice_aq_sff_eeprom(struct ice_hw *hw, u16 lport, u8 bus_addr,
		  u16 mem_addr, u8 page, u8 set_page, u8 *data, u8 length,
		  bool write, struct ice_sq_cd *cd)
{
	struct ice_aqc_sff_eeprom *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!data || (mem_addr & 0xff00))
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_sff_eeprom);
	cmd = &desc.params.read_write_sff_param;
	desc.flags = CPU_TO_LE16(ICE_AQ_FLAG_RD);
	cmd->lport_num = (u8)(lport & 0xff);
	cmd->lport_num_valid = (u8)((lport >> 8) & 0x01);
	cmd->i2c_bus_addr = CPU_TO_LE16(((bus_addr >> 1) &
					 ICE_AQC_SFF_I2CBUS_7BIT_M) |
					((set_page <<
					  ICE_AQC_SFF_SET_EEPROM_PAGE_S) &
					 ICE_AQC_SFF_SET_EEPROM_PAGE_M));
	cmd->i2c_mem_addr = CPU_TO_LE16(mem_addr & 0xff);
	cmd->eeprom_page = CPU_TO_LE16((u16)page << ICE_AQC_SFF_EEPROM_PAGE_S);
	if (write)
		cmd->i2c_bus_addr |= CPU_TO_LE16(ICE_AQC_SFF_IS_WRITE);

	status = ice_aq_send_cmd(hw, &desc, data, length, cd);
	return status;
}

/**
 * ice_aq_prog_topo_dev_nvm
 * @hw: pointer to the hardware structure
 * @topo_params: pointer to structure storing topology parameters for a device
 * @cd: pointer to command details structure or NULL
 *
 * Program Topology Device NVM (0x06F2)
 *
 */
int
ice_aq_prog_topo_dev_nvm(struct ice_hw *hw,
			 struct ice_aqc_link_topo_params *topo_params,
			 struct ice_sq_cd *cd)
{
	struct ice_aqc_prog_topo_dev_nvm *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.prog_topo_dev_nvm;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_prog_topo_dev_nvm);

	ice_memcpy(&cmd->topo_params, topo_params, sizeof(*topo_params),
		   ICE_NONDMA_TO_NONDMA);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_read_topo_dev_nvm
 * @hw: pointer to the hardware structure
 * @topo_params: pointer to structure storing topology parameters for a device
 * @start_address: byte offset in the topology device NVM
 * @data: pointer to data buffer
 * @data_size: number of bytes to be read from the topology device NVM
 * @cd: pointer to command details structure or NULL
 * Read Topology Device NVM (0x06F3)
 *
 */
int
ice_aq_read_topo_dev_nvm(struct ice_hw *hw,
			 struct ice_aqc_link_topo_params *topo_params,
			 u32 start_address, u8 *data, u8 data_size,
			 struct ice_sq_cd *cd)
{
	struct ice_aqc_read_topo_dev_nvm *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!data || data_size == 0 ||
	    data_size > ICE_AQC_READ_TOPO_DEV_NVM_DATA_READ_SIZE)
		return ICE_ERR_PARAM;

	cmd = &desc.params.read_topo_dev_nvm;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_read_topo_dev_nvm);

	desc.datalen = CPU_TO_LE16(data_size);
	ice_memcpy(&cmd->topo_params, topo_params, sizeof(*topo_params),
		   ICE_NONDMA_TO_NONDMA);
	cmd->start_address = CPU_TO_LE32(start_address);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	ice_memcpy(data, cmd->data_read, data_size, ICE_NONDMA_TO_NONDMA);

	return 0;
}

static u16 ice_lut_type_to_size(u16 lut_type)
{
	switch (lut_type) {
	case ICE_LUT_VSI:
		return ICE_LUT_VSI_SIZE;
	case ICE_LUT_GLOBAL:
		return ICE_LUT_GLOBAL_SIZE;
	case ICE_LUT_PF:
		return ICE_LUT_PF_SIZE;
	case ICE_LUT_PF_SMALL:
		return ICE_LUT_PF_SMALL_SIZE;
	default:
		return 0;
	}
}

static u16 ice_lut_size_to_flag(u16 lut_size)
{
	u16 f = 0;

	switch (lut_size) {
	case ICE_LUT_GLOBAL_SIZE:
		f = ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_512_FLAG;
		break;
	case ICE_LUT_PF_SIZE:
		f = ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_2K_FLAG;
		break;
	default:
		break;
	}
	return f << ICE_AQC_GSET_RSS_LUT_TABLE_SIZE_S;
}

int ice_lut_size_to_type(int lut_size)
{
	switch (lut_size) {
	case ICE_LUT_VSI_SIZE:
		return ICE_LUT_VSI;
	case ICE_LUT_GLOBAL_SIZE:
		return ICE_LUT_GLOBAL;
	case ICE_LUT_PF_SIZE:
		return ICE_LUT_PF;
	case ICE_LUT_PF_SMALL_SIZE:
		return ICE_LUT_PF_SMALL;
	default:
		return -1;
	}
}

/**
 * __ice_aq_get_set_rss_lut
 * @hw: pointer to the hardware structure
 * @params: RSS LUT parameters
 * @set: set true to set the table, false to get the table
 *
 * Internal function to get (0x0B05) or set (0x0B03) RSS look up table
 */
static int
__ice_aq_get_set_rss_lut(struct ice_hw *hw, struct ice_aq_get_set_rss_lut_params *params, bool set)
{
	u16 flags, vsi_id, lut_type, lut_size, glob_lut_idx = 0, vsi_handle;
	struct ice_aqc_get_set_rss_lut *cmd_resp;
	struct ice_aq_desc desc;
	int status;
	u8 *lut;

	if (!params)
		return ICE_ERR_PARAM;

	vsi_handle = params->vsi_handle;
	lut = params->lut;
	lut_size = ice_lut_type_to_size(params->lut_type);
	lut_type = params->lut_type & ICE_LUT_TYPE_MASK;
	cmd_resp = &desc.params.get_set_rss_lut;
	if (lut_type == ICE_LUT_GLOBAL)
		glob_lut_idx = params->global_lut_id;

	if (!lut || !lut_size || !ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	if (lut_size > params->lut_size)
		return ICE_ERR_INVAL_SIZE;

	if (set && lut_size != params->lut_size)
		return ICE_ERR_PARAM;

	vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	if (set) {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_rss_lut);
		desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	} else {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_rss_lut);
	}

	cmd_resp->vsi_id = CPU_TO_LE16(((vsi_id <<
					 ICE_AQC_GSET_RSS_LUT_VSI_ID_S) &
					ICE_AQC_GSET_RSS_LUT_VSI_ID_M) |
				       ICE_AQC_GSET_RSS_LUT_VSI_VALID);

	flags = ice_lut_size_to_flag(lut_size) |
		 ((lut_type << ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_S) &
		  ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_M) |
		 ((glob_lut_idx << ICE_AQC_GSET_RSS_LUT_GLOBAL_IDX_S) &
		  ICE_AQC_GSET_RSS_LUT_GLOBAL_IDX_M);

	cmd_resp->flags = CPU_TO_LE16(flags);
	status = ice_aq_send_cmd(hw, &desc, lut, lut_size, NULL);
	params->lut_size = LE16_TO_CPU(desc.datalen);
	return status;
}

/**
 * ice_aq_get_rss_lut
 * @hw: pointer to the hardware structure
 * @get_params: RSS LUT parameters used to specify which RSS LUT to get
 *
 * get the RSS lookup table, PF or VSI type
 */
int
ice_aq_get_rss_lut(struct ice_hw *hw, struct ice_aq_get_set_rss_lut_params *get_params)
{
	return __ice_aq_get_set_rss_lut(hw, get_params, false);
}

/**
 * ice_aq_set_rss_lut
 * @hw: pointer to the hardware structure
 * @set_params: RSS LUT parameters used to specify how to set the RSS LUT
 *
 * set the RSS lookup table, PF or VSI type
 */
int
ice_aq_set_rss_lut(struct ice_hw *hw, struct ice_aq_get_set_rss_lut_params *set_params)
{
	return __ice_aq_get_set_rss_lut(hw, set_params, true);
}

/**
 * __ice_aq_get_set_rss_key
 * @hw: pointer to the HW struct
 * @vsi_id: VSI FW index
 * @key: pointer to key info struct
 * @set: set true to set the key, false to get the key
 *
 * get (0x0B04) or set (0x0B02) the RSS key per VSI
 */
static int __ice_aq_get_set_rss_key(struct ice_hw *hw, u16 vsi_id,
				    struct ice_aqc_get_set_rss_keys *key,
				    bool set)
{
	struct ice_aqc_get_set_rss_key *cmd_resp;
	u16 key_size = sizeof(*key);
	struct ice_aq_desc desc;

	cmd_resp = &desc.params.get_set_rss_key;

	if (set) {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_rss_key);
		desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	} else {
		ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_rss_key);
	}

	cmd_resp->vsi_id = CPU_TO_LE16(((vsi_id <<
					 ICE_AQC_GSET_RSS_KEY_VSI_ID_S) &
					ICE_AQC_GSET_RSS_KEY_VSI_ID_M) |
				       ICE_AQC_GSET_RSS_KEY_VSI_VALID);

	return ice_aq_send_cmd(hw, &desc, key, key_size, NULL);
}

/**
 * ice_aq_get_rss_key
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 * @key: pointer to key info struct
 *
 * get the RSS key per VSI
 */
int
ice_aq_get_rss_key(struct ice_hw *hw, u16 vsi_handle,
		   struct ice_aqc_get_set_rss_keys *key)
{
	if (!ice_is_vsi_valid(hw, vsi_handle) || !key)
		return ICE_ERR_PARAM;

	return __ice_aq_get_set_rss_key(hw, ice_get_hw_vsi_num(hw, vsi_handle),
					key, false);
}

/**
 * ice_aq_set_rss_key
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 * @keys: pointer to key info struct
 *
 * set the RSS key per VSI
 */
int
ice_aq_set_rss_key(struct ice_hw *hw, u16 vsi_handle,
		   struct ice_aqc_get_set_rss_keys *keys)
{
	if (!ice_is_vsi_valid(hw, vsi_handle) || !keys)
		return ICE_ERR_PARAM;

	return __ice_aq_get_set_rss_key(hw, ice_get_hw_vsi_num(hw, vsi_handle),
					keys, true);
}

/**
 * ice_aq_add_lan_txq
 * @hw: pointer to the hardware structure
 * @num_qgrps: Number of added queue groups
 * @qg_list: list of queue groups to be added
 * @buf_size: size of buffer for indirect command
 * @cd: pointer to command details structure or NULL
 *
 * Add Tx LAN queue (0x0C30)
 *
 * NOTE:
 * Prior to calling add Tx LAN queue:
 * Initialize the following as part of the Tx queue context:
 * Completion queue ID if the queue uses Completion queue, Quanta profile,
 * Cache profile and Packet shaper profile.
 *
 * After add Tx LAN queue AQ command is completed:
 * Interrupts should be associated with specific queues,
 * Association of Tx queue to Doorbell queue is not part of Add LAN Tx queue
 * flow.
 */
int
ice_aq_add_lan_txq(struct ice_hw *hw, u8 num_qgrps,
		   struct ice_aqc_add_tx_qgrp *qg_list, u16 buf_size,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_add_tx_qgrp *list;
	struct ice_aqc_add_txqs *cmd;
	struct ice_aq_desc desc;
	u16 i, sum_size = 0;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.add_txqs;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_txqs);

	if (!qg_list)
		return ICE_ERR_PARAM;

	if (num_qgrps > ICE_LAN_TXQ_MAX_QGRPS)
		return ICE_ERR_PARAM;

	for (i = 0, list = qg_list; i < num_qgrps; i++) {
		sum_size += ice_struct_size(list, txqs, list->num_txqs);
		list = (struct ice_aqc_add_tx_qgrp *)(list->txqs +
						      list->num_txqs);
	}

	if (buf_size != sum_size)
		return ICE_ERR_PARAM;

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->num_qgrps = num_qgrps;

	return ice_aq_send_cmd(hw, &desc, qg_list, buf_size, cd);
}

/**
 * ice_aq_dis_lan_txq
 * @hw: pointer to the hardware structure
 * @num_qgrps: number of groups in the list
 * @qg_list: the list of groups to disable
 * @buf_size: the total size of the qg_list buffer in bytes
 * @rst_src: if called due to reset, specifies the reset source
 * @vmvf_num: the relative VM or VF number that is undergoing the reset
 * @cd: pointer to command details structure or NULL
 *
 * Disable LAN Tx queue (0x0C31)
 */
static int
ice_aq_dis_lan_txq(struct ice_hw *hw, u8 num_qgrps,
		   struct ice_aqc_dis_txq_item *qg_list, u16 buf_size,
		   enum ice_disq_rst_src rst_src, u16 vmvf_num,
		   struct ice_sq_cd *cd)
{
	struct ice_aqc_dis_txq_item *item;
	struct ice_aqc_dis_txqs *cmd;
	struct ice_aq_desc desc;
	int status;
	u16 i, sz = 0;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);
	cmd = &desc.params.dis_txqs;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_dis_txqs);

	/* qg_list can be NULL only in VM/VF reset flow */
	if (!qg_list && !rst_src)
		return ICE_ERR_PARAM;

	if (num_qgrps > ICE_LAN_TXQ_MAX_QGRPS)
		return ICE_ERR_PARAM;

	cmd->num_entries = num_qgrps;

	cmd->vmvf_and_timeout = CPU_TO_LE16((5 << ICE_AQC_Q_DIS_TIMEOUT_S) &
					    ICE_AQC_Q_DIS_TIMEOUT_M);

	switch (rst_src) {
	case ICE_VM_RESET:
		cmd->cmd_type = ICE_AQC_Q_DIS_CMD_VM_RESET;
		cmd->vmvf_and_timeout |=
			CPU_TO_LE16(vmvf_num & ICE_AQC_Q_DIS_VMVF_NUM_M);
		break;
	case ICE_VF_RESET:
		cmd->cmd_type = ICE_AQC_Q_DIS_CMD_VF_RESET;
		/* In this case, FW expects vmvf_num to be absolute VF ID */
		cmd->vmvf_and_timeout |=
			CPU_TO_LE16((vmvf_num + hw->func_caps.vf_base_id) &
				    ICE_AQC_Q_DIS_VMVF_NUM_M);
		break;
	case ICE_NO_RESET:
	default:
		break;
	}

	/* flush pipe on time out */
	cmd->cmd_type |= ICE_AQC_Q_DIS_CMD_FLUSH_PIPE;
	/* If no queue group info, we are in a reset flow. Issue the AQ */
	if (!qg_list)
		goto do_aq;

	/* set RD bit to indicate that command buffer is provided by the driver
	 * and it needs to be read by the firmware
	 */
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	for (i = 0, item = qg_list; i < num_qgrps; i++) {
		u16 item_size = ice_struct_size(item, q_id, item->num_qs);

		/* If the num of queues is even, add 2 bytes of padding */
		if ((item->num_qs % 2) == 0)
			item_size += 2;

		sz += item_size;

		item = (struct ice_aqc_dis_txq_item *)((u8 *)item + item_size);
	}

	if (buf_size != sz)
		return ICE_ERR_PARAM;

do_aq:
	status = ice_aq_send_cmd(hw, &desc, qg_list, buf_size, cd);
	if (status) {
		if (!qg_list)
			ice_debug(hw, ICE_DBG_SCHED, "VM%d disable failed %d\n",
				  vmvf_num, hw->adminq.sq_last_status);
		else
			ice_debug(hw, ICE_DBG_SCHED, "disable queue %d failed %d\n",
				  LE16_TO_CPU(qg_list[0].q_id[0]),
				  hw->adminq.sq_last_status);
	}
	return status;
}

/**
 * ice_aq_move_recfg_lan_txq
 * @hw: pointer to the hardware structure
 * @num_qs: number of queues to move/reconfigure
 * @is_move: true if this operation involves node movement
 * @is_tc_change: true if this operation involves a TC change
 * @subseq_call: true if this operation is a subsequent call
 * @flush_pipe: on timeout, true to flush pipe, false to return EAGAIN
 * @timeout: timeout in units of 100 usec (valid values 0-50)
 * @blocked_cgds: out param, bitmap of CGDs that timed out if returning EAGAIN
 * @buf: struct containing src/dest TEID and per-queue info
 * @buf_size: size of buffer for indirect command
 * @txqs_moved: out param, number of queues successfully moved
 * @cd: pointer to command details structure or NULL
 *
 * Move / Reconfigure Tx LAN queues (0x0C32)
 */
int
ice_aq_move_recfg_lan_txq(struct ice_hw *hw, u8 num_qs, bool is_move,
			  bool is_tc_change, bool subseq_call, bool flush_pipe,
			  u8 timeout, u32 *blocked_cgds,
			  struct ice_aqc_move_txqs_data *buf, u16 buf_size,
			  u8 *txqs_moved, struct ice_sq_cd *cd)
{
	struct ice_aqc_move_txqs *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.move_txqs;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_move_recfg_txqs);

#define ICE_LAN_TXQ_MOVE_TIMEOUT_MAX 50
	if (timeout > ICE_LAN_TXQ_MOVE_TIMEOUT_MAX)
		return ICE_ERR_PARAM;

	if (is_tc_change && !flush_pipe && !blocked_cgds)
		return ICE_ERR_PARAM;

	if (!is_move && !is_tc_change)
		return ICE_ERR_PARAM;

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	if (is_move)
		cmd->cmd_type |= ICE_AQC_Q_CMD_TYPE_MOVE;

	if (is_tc_change)
		cmd->cmd_type |= ICE_AQC_Q_CMD_TYPE_TC_CHANGE;

	if (subseq_call)
		cmd->cmd_type |= ICE_AQC_Q_CMD_SUBSEQ_CALL;

	if (flush_pipe)
		cmd->cmd_type |= ICE_AQC_Q_CMD_FLUSH_PIPE;

	cmd->num_qs = num_qs;
	cmd->timeout = ((timeout << ICE_AQC_Q_CMD_TIMEOUT_S) &
			ICE_AQC_Q_CMD_TIMEOUT_M);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (!status && txqs_moved)
		*txqs_moved = cmd->num_qs;

	if (hw->adminq.sq_last_status == ICE_AQ_RC_EAGAIN &&
	    is_tc_change && !flush_pipe)
		*blocked_cgds = LE32_TO_CPU(cmd->blocked_cgds);

	return status;
}

/**
 * ice_aq_add_rdma_qsets
 * @hw: pointer to the hardware structure
 * @num_qset_grps: Number of RDMA Qset groups
 * @qset_list: list of qset groups to be added
 * @buf_size: size of buffer for indirect command
 * @cd: pointer to command details structure or NULL
 *
 * Add Tx RDMA Qsets (0x0C33)
 */
int
ice_aq_add_rdma_qsets(struct ice_hw *hw, u8 num_qset_grps,
		      struct ice_aqc_add_rdma_qset_data *qset_list,
		      u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_rdma_qset_data *list;
	struct ice_aqc_add_rdma_qset *cmd;
	struct ice_aq_desc desc;
	u16 i, sum_size = 0;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.add_rdma_qset;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_rdma_qset);

	if (!qset_list)
		return ICE_ERR_PARAM;

	if (num_qset_grps > ICE_LAN_TXQ_MAX_QGRPS)
		return ICE_ERR_PARAM;

	for (i = 0, list = qset_list; i < num_qset_grps; i++) {
		u16 num_qsets = LE16_TO_CPU(list->num_qsets);

		sum_size += ice_struct_size(list, rdma_qsets, num_qsets);
		list = (struct ice_aqc_add_rdma_qset_data *)(list->rdma_qsets +
							     num_qsets);
	}

	if (buf_size != sum_size)
		return ICE_ERR_PARAM;

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->num_qset_grps = num_qset_grps;

	return ice_aq_send_cmd(hw, &desc, qset_list, buf_size, cd);
}

/* End of FW Admin Queue command wrappers */

/**
 * ice_write_byte - write a byte to a packed context structure
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_write_byte(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u8 src_byte, dest_byte, mask;
	u8 *from, *dest;
	u16 shift_width;

	/* copy from the next struct field */
	from = src_ctx + ce_info->offset;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;
	mask = (u8)(BIT(ce_info->width) - 1);

	src_byte = *from;
	src_byte &= mask;

	/* shift to correct alignment */
	mask <<= shift_width;
	src_byte <<= shift_width;

	/* get the current bits from the target bit string */
	dest = dest_ctx + (ce_info->lsb / 8);

	ice_memcpy(&dest_byte, dest, sizeof(dest_byte), ICE_NONDMA_TO_NONDMA);

	dest_byte &= ~mask;	/* get the bits not changing */
	dest_byte |= src_byte;	/* add in the new bits */

	/* put it all back */
	ice_memcpy(dest, &dest_byte, sizeof(dest_byte), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_write_word - write a word to a packed context structure
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_write_word(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u16 src_word, mask;
	__le16 dest_word;
	u8 *from, *dest;
	u16 shift_width;

	/* copy from the next struct field */
	from = src_ctx + ce_info->offset;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;
	mask = BIT(ce_info->width) - 1;

	/* don't swizzle the bits until after the mask because the mask bits
	 * will be in a different bit position on big endian machines
	 */
	src_word = *(u16 *)from;
	src_word &= mask;

	/* shift to correct alignment */
	mask <<= shift_width;
	src_word <<= shift_width;

	/* get the current bits from the target bit string */
	dest = dest_ctx + (ce_info->lsb / 8);

	ice_memcpy(&dest_word, dest, sizeof(dest_word), ICE_NONDMA_TO_NONDMA);

	dest_word &= ~(CPU_TO_LE16(mask));	/* get the bits not changing */
	dest_word |= CPU_TO_LE16(src_word);	/* add in the new bits */

	/* put it all back */
	ice_memcpy(dest, &dest_word, sizeof(dest_word), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_write_dword - write a dword to a packed context structure
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_write_dword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u32 src_dword, mask;
	__le32 dest_dword;
	u8 *from, *dest;
	u16 shift_width;

	/* copy from the next struct field */
	from = src_ctx + ce_info->offset;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* if the field width is exactly 32 on an x86 machine, then the shift
	 * operation will not work because the SHL instructions count is masked
	 * to 5 bits so the shift will do nothing
	 */
	if (ce_info->width < 32)
		mask = BIT(ce_info->width) - 1;
	else
		mask = (u32)~0;

	/* don't swizzle the bits until after the mask because the mask bits
	 * will be in a different bit position on big endian machines
	 */
	src_dword = *(u32 *)from;
	src_dword &= mask;

	/* shift to correct alignment */
	mask <<= shift_width;
	src_dword <<= shift_width;

	/* get the current bits from the target bit string */
	dest = dest_ctx + (ce_info->lsb / 8);

	ice_memcpy(&dest_dword, dest, sizeof(dest_dword), ICE_NONDMA_TO_NONDMA);

	dest_dword &= ~(CPU_TO_LE32(mask));	/* get the bits not changing */
	dest_dword |= CPU_TO_LE32(src_dword);	/* add in the new bits */

	/* put it all back */
	ice_memcpy(dest, &dest_dword, sizeof(dest_dword), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_write_qword - write a qword to a packed context structure
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_write_qword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u64 src_qword, mask;
	__le64 dest_qword;
	u8 *from, *dest;
	u16 shift_width;

	/* copy from the next struct field */
	from = src_ctx + ce_info->offset;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* if the field width is exactly 64 on an x86 machine, then the shift
	 * operation will not work because the SHL instructions count is masked
	 * to 6 bits so the shift will do nothing
	 */
	if (ce_info->width < 64)
		mask = BIT_ULL(ce_info->width) - 1;
	else
		mask = (u64)~0;

	/* don't swizzle the bits until after the mask because the mask bits
	 * will be in a different bit position on big endian machines
	 */
	src_qword = *(u64 *)from;
	src_qword &= mask;

	/* shift to correct alignment */
	mask <<= shift_width;
	src_qword <<= shift_width;

	/* get the current bits from the target bit string */
	dest = dest_ctx + (ce_info->lsb / 8);

	ice_memcpy(&dest_qword, dest, sizeof(dest_qword), ICE_NONDMA_TO_NONDMA);

	dest_qword &= ~(CPU_TO_LE64(mask));	/* get the bits not changing */
	dest_qword |= CPU_TO_LE64(src_qword);	/* add in the new bits */

	/* put it all back */
	ice_memcpy(dest, &dest_qword, sizeof(dest_qword), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_set_ctx - set context bits in packed structure
 * @hw: pointer to the hardware structure
 * @src_ctx:  pointer to a generic non-packed context structure
 * @dest_ctx: pointer to memory for the packed structure
 * @ce_info:  a description of the structure to be transformed
 */
int
ice_set_ctx(struct ice_hw *hw, u8 *src_ctx, u8 *dest_ctx,
	    const struct ice_ctx_ele *ce_info)
{
	int f;

	for (f = 0; ce_info[f].width; f++) {
		/* We have to deal with each element of the FW response
		 * using the correct size so that we are correct regardless
		 * of the endianness of the machine.
		 */
		if (ce_info[f].width > (ce_info[f].size_of * BITS_PER_BYTE)) {
			ice_debug(hw, ICE_DBG_QCTX, "Field %d width of %d bits larger than size of %d byte(s) ... skipping write\n",
				  f, ce_info[f].width, ce_info[f].size_of);
			continue;
		}
		switch (ce_info[f].size_of) {
		case sizeof(u8):
			ice_write_byte(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u16):
			ice_write_word(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u32):
			ice_write_dword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case sizeof(u64):
			ice_write_qword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		default:
			return ICE_ERR_INVAL_SIZE;
		}
	}

	return 0;
}

/**
 * ice_aq_get_internal_data
 * @hw: pointer to the hardware structure
 * @cluster_id: specific cluster to dump
 * @table_id: table ID within cluster
 * @start: index of line in the block to read
 * @buf: dump buffer
 * @buf_size: dump buffer size
 * @ret_buf_size: return buffer size (returned by FW)
 * @ret_next_cluster: next cluster to read (returned by FW)
 * @ret_next_table: next block to read (returned by FW)
 * @ret_next_index: next index to read (returned by FW)
 * @cd: pointer to command details structure
 *
 * Get internal FW/HW data (0xFF08) for debug purposes.
 */
int
ice_aq_get_internal_data(struct ice_hw *hw, u16 cluster_id, u16 table_id,
			 u32 start, void *buf, u16 buf_size, u16 *ret_buf_size,
			 u16 *ret_next_cluster, u16 *ret_next_table,
			 u32 *ret_next_index, struct ice_sq_cd *cd)
{
	struct ice_aqc_debug_dump_internals *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.debug_dump;

	if (buf_size == 0 || !buf)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_debug_dump_internals);

	cmd->cluster_id = CPU_TO_LE16(cluster_id);
	cmd->table_id = CPU_TO_LE16(table_id);
	cmd->idx = CPU_TO_LE32(start);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (!status) {
		if (ret_buf_size)
			*ret_buf_size = LE16_TO_CPU(desc.datalen);
		if (ret_next_cluster)
			*ret_next_cluster = LE16_TO_CPU(cmd->cluster_id);
		if (ret_next_table)
			*ret_next_table = LE16_TO_CPU(cmd->table_id);
		if (ret_next_index)
			*ret_next_index = LE32_TO_CPU(cmd->idx);
	}

	return status;
}

/**
 * ice_read_byte - read context byte into struct
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_read_byte(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u8 dest_byte, mask;
	u8 *src, *target;
	u16 shift_width;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;
	mask = (u8)(BIT(ce_info->width) - 1);

	/* shift to correct alignment */
	mask <<= shift_width;

	/* get the current bits from the src bit string */
	src = src_ctx + (ce_info->lsb / 8);

	ice_memcpy(&dest_byte, src, sizeof(dest_byte), ICE_NONDMA_TO_NONDMA);

	dest_byte &= mask;

	dest_byte >>= shift_width;

	/* get the address from the struct field */
	target = dest_ctx + ce_info->offset;

	/* put it back in the struct */
	ice_memcpy(target, &dest_byte, sizeof(dest_byte), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_read_word - read context word into struct
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_read_word(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u16 dest_word, mask;
	u8 *src, *target;
	__le16 src_word;
	u16 shift_width;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;
	mask = BIT(ce_info->width) - 1;

	/* shift to correct alignment */
	mask <<= shift_width;

	/* get the current bits from the src bit string */
	src = src_ctx + (ce_info->lsb / 8);

	ice_memcpy(&src_word, src, sizeof(src_word), ICE_NONDMA_TO_NONDMA);

	/* the data in the memory is stored as little endian so mask it
	 * correctly
	 */
	src_word &= CPU_TO_LE16(mask);

	/* get the data back into host order before shifting */
	dest_word = LE16_TO_CPU(src_word);

	dest_word >>= shift_width;

	/* get the address from the struct field */
	target = dest_ctx + ce_info->offset;

	/* put it back in the struct */
	ice_memcpy(target, &dest_word, sizeof(dest_word), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_read_dword - read context dword into struct
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_read_dword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u32 dest_dword, mask;
	__le32 src_dword;
	u8 *src, *target;
	u16 shift_width;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* if the field width is exactly 32 on an x86 machine, then the shift
	 * operation will not work because the SHL instructions count is masked
	 * to 5 bits so the shift will do nothing
	 */
	if (ce_info->width < 32)
		mask = BIT(ce_info->width) - 1;
	else
		mask = (u32)~0;

	/* shift to correct alignment */
	mask <<= shift_width;

	/* get the current bits from the src bit string */
	src = src_ctx + (ce_info->lsb / 8);

	ice_memcpy(&src_dword, src, sizeof(src_dword), ICE_NONDMA_TO_NONDMA);

	/* the data in the memory is stored as little endian so mask it
	 * correctly
	 */
	src_dword &= CPU_TO_LE32(mask);

	/* get the data back into host order before shifting */
	dest_dword = LE32_TO_CPU(src_dword);

	dest_dword >>= shift_width;

	/* get the address from the struct field */
	target = dest_ctx + ce_info->offset;

	/* put it back in the struct */
	ice_memcpy(target, &dest_dword, sizeof(dest_dword), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_read_qword - read context qword into struct
 * @src_ctx:  the context structure to read from
 * @dest_ctx: the context to be written to
 * @ce_info:  a description of the struct to be filled
 */
static void
ice_read_qword(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	u64 dest_qword, mask;
	__le64 src_qword;
	u8 *src, *target;
	u16 shift_width;

	/* prepare the bits and mask */
	shift_width = ce_info->lsb % 8;

	/* if the field width is exactly 64 on an x86 machine, then the shift
	 * operation will not work because the SHL instructions count is masked
	 * to 6 bits so the shift will do nothing
	 */
	if (ce_info->width < 64)
		mask = BIT_ULL(ce_info->width) - 1;
	else
		mask = (u64)~0;

	/* shift to correct alignment */
	mask <<= shift_width;

	/* get the current bits from the src bit string */
	src = src_ctx + (ce_info->lsb / 8);

	ice_memcpy(&src_qword, src, sizeof(src_qword), ICE_NONDMA_TO_NONDMA);

	/* the data in the memory is stored as little endian so mask it
	 * correctly
	 */
	src_qword &= CPU_TO_LE64(mask);

	/* get the data back into host order before shifting */
	dest_qword = LE64_TO_CPU(src_qword);

	dest_qword >>= shift_width;

	/* get the address from the struct field */
	target = dest_ctx + ce_info->offset;

	/* put it back in the struct */
	ice_memcpy(target, &dest_qword, sizeof(dest_qword), ICE_NONDMA_TO_NONDMA);
}

/**
 * ice_get_ctx - extract context bits from a packed structure
 * @src_ctx:  pointer to a generic packed context structure
 * @dest_ctx: pointer to a generic non-packed context structure
 * @ce_info:  a description of the structure to be read from
 */
int
ice_get_ctx(u8 *src_ctx, u8 *dest_ctx, const struct ice_ctx_ele *ce_info)
{
	int f;

	for (f = 0; ce_info[f].width; f++) {
		switch (ce_info[f].size_of) {
		case 1:
			ice_read_byte(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case 2:
			ice_read_word(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case 4:
			ice_read_dword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		case 8:
			ice_read_qword(src_ctx, dest_ctx, &ce_info[f]);
			break;
		default:
			/* nothing to do, just keep going */
			break;
		}
	}

	return 0;
}

/**
 * ice_get_lan_q_ctx - get the LAN queue context for the given VSI and TC
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @q_handle: software queue handle
 */
struct ice_q_ctx *
ice_get_lan_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 q_handle)
{
	struct ice_vsi_ctx *vsi;
	struct ice_q_ctx *q_ctx;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi)
		return NULL;
	if (q_handle >= vsi->num_lan_q_entries[tc])
		return NULL;
	if (!vsi->lan_q_ctx[tc])
		return NULL;
	q_ctx = vsi->lan_q_ctx[tc];
	return &q_ctx[q_handle];
}

/**
 * ice_ena_vsi_txq
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @q_handle: software queue handle
 * @num_qgrps: Number of added queue groups
 * @buf: list of queue groups to be added
 * @buf_size: size of buffer for indirect command
 * @cd: pointer to command details structure or NULL
 *
 * This function adds one LAN queue
 */
int
ice_ena_vsi_txq(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u16 q_handle,
		u8 num_qgrps, struct ice_aqc_add_tx_qgrp *buf, u16 buf_size,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_txsched_elem_data node = { 0 };
	struct ice_sched_node *parent;
	struct ice_q_ctx *q_ctx;
	struct ice_hw *hw;
	int status;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return ICE_ERR_CFG;

	if (num_qgrps > 1 || buf->num_txqs > 1)
		return ICE_ERR_MAX_LIMIT;

	hw = pi->hw;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	ice_acquire_lock(&pi->sched_lock);

	q_ctx = ice_get_lan_q_ctx(hw, vsi_handle, tc, q_handle);
	if (!q_ctx) {
		ice_debug(hw, ICE_DBG_SCHED, "Enaq: invalid queue handle %d\n",
			  q_handle);
		status = ICE_ERR_PARAM;
		goto ena_txq_exit;
	}

	/* find a parent node */
	parent = ice_sched_get_free_qparent(pi, vsi_handle, tc,
					    ICE_SCHED_NODE_OWNER_LAN);
	if (!parent) {
		status = ICE_ERR_PARAM;
		goto ena_txq_exit;
	}

	buf->parent_teid = parent->info.node_teid;
	node.parent_teid = parent->info.node_teid;
	/* Mark that the values in the "generic" section as valid. The default
	 * value in the "generic" section is zero. This means that :
	 * - Scheduling mode is Bytes Per Second (BPS), indicated by Bit 0.
	 * - 0 priority among siblings, indicated by Bit 1-3.
	 * - WFQ, indicated by Bit 4.
	 * - 0 Adjustment value is used in PSM credit update flow, indicated by
	 * Bit 5-6.
	 * - Bit 7 is reserved.
	 * Without setting the generic section as valid in valid_sections, the
	 * Admin queue command will fail with error code ICE_AQ_RC_EINVAL.
	 */
	buf->txqs[0].info.valid_sections =
		ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
		ICE_AQC_ELEM_VALID_EIR;
	buf->txqs[0].info.generic = 0;
	buf->txqs[0].info.cir_bw.bw_profile_idx =
		CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
	buf->txqs[0].info.cir_bw.bw_alloc =
		CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);
	buf->txqs[0].info.eir_bw.bw_profile_idx =
		CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
	buf->txqs[0].info.eir_bw.bw_alloc =
		CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);

	/* add the LAN queue */
	status = ice_aq_add_lan_txq(hw, num_qgrps, buf, buf_size, cd);
	if (status) {
		ice_debug(hw, ICE_DBG_SCHED, "enable queue %d failed %d\n",
			  LE16_TO_CPU(buf->txqs[0].txq_id),
			  hw->adminq.sq_last_status);
		goto ena_txq_exit;
	}

	node.node_teid = buf->txqs[0].q_teid;
	node.data.elem_type = ICE_AQC_ELEM_TYPE_LEAF;
	q_ctx->q_handle = q_handle;
	q_ctx->q_teid = LE32_TO_CPU(node.node_teid);

	/* add a leaf node into scheduler tree queue layer */
	status = ice_sched_add_node(pi, hw->num_tx_sched_layers - 1, &node, NULL);
	if (!status)
		status = ice_sched_replay_q_bw(pi, q_ctx);

ena_txq_exit:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_dis_vsi_txq
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @num_queues: number of queues
 * @q_handles: pointer to software queue handle array
 * @q_ids: pointer to the q_id array
 * @q_teids: pointer to queue node teids
 * @rst_src: if called due to reset, specifies the reset source
 * @vmvf_num: the relative VM or VF number that is undergoing the reset
 * @cd: pointer to command details structure or NULL
 *
 * This function removes queues and their corresponding nodes in SW DB
 */
int
ice_dis_vsi_txq(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u8 num_queues,
		u16 *q_handles, u16 *q_ids, u32 *q_teids,
		enum ice_disq_rst_src rst_src, u16 vmvf_num,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_dis_txq_item *qg_list;
	struct ice_q_ctx *q_ctx;
	int status = ICE_ERR_DOES_NOT_EXIST;
	struct ice_hw *hw;
	u16 i, buf_size;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return ICE_ERR_CFG;

	hw = pi->hw;

	if (!num_queues) {
		/* if queue is disabled already yet the disable queue command
		 * has to be sent to complete the VF reset, then call
		 * ice_aq_dis_lan_txq without any queue information
		 */
		if (rst_src)
			return ice_aq_dis_lan_txq(hw, 0, NULL, 0, rst_src,
						  vmvf_num, NULL);
		return ICE_ERR_CFG;
	}

	buf_size = ice_struct_size(qg_list, q_id, 1);
	qg_list = (struct ice_aqc_dis_txq_item *)ice_malloc(hw, buf_size);
	if (!qg_list)
		return ICE_ERR_NO_MEMORY;

	ice_acquire_lock(&pi->sched_lock);

	for (i = 0; i < num_queues; i++) {
		struct ice_sched_node *node;

		node = ice_sched_find_node_by_teid(pi->root, q_teids[i]);
		if (!node)
			continue;
		q_ctx = ice_get_lan_q_ctx(hw, vsi_handle, tc, q_handles[i]);
		if (!q_ctx) {
			ice_debug(hw, ICE_DBG_SCHED, "invalid queue handle%d\n",
				  q_handles[i]);
			continue;
		}
		if (q_ctx->q_handle != q_handles[i]) {
			ice_debug(hw, ICE_DBG_SCHED, "Err:handles %d %d\n",
				  q_ctx->q_handle, q_handles[i]);
			continue;
		}
		qg_list->parent_teid = node->info.parent_teid;
		qg_list->num_qs = 1;
		qg_list->q_id[0] = CPU_TO_LE16(q_ids[i]);
		status = ice_aq_dis_lan_txq(hw, 1, qg_list, buf_size, rst_src,
					    vmvf_num, cd);

		if (status)
			break;
		ice_free_sched_node(pi, node);
		q_ctx->q_handle = ICE_INVAL_Q_HANDLE;
	}
	ice_release_lock(&pi->sched_lock);
	ice_free(hw, qg_list);
	return status;
}

/**
 * ice_cfg_vsi_qs - configure the new/existing VSI queues
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap
 * @maxqs: max queues array per TC
 * @owner: LAN or RDMA
 *
 * This function adds/updates the VSI queues per TC.
 */
static int
ice_cfg_vsi_qs(struct ice_port_info *pi, u16 vsi_handle, u16 tc_bitmap,
	       u16 *maxqs, u8 owner)
{
	int status = 0;
	u8 i;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return ICE_ERR_CFG;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;

	ice_acquire_lock(&pi->sched_lock);

	ice_for_each_traffic_class(i) {
		/* configuration is possible only if TC node is present */
		if (!ice_sched_get_tc_node(pi, i))
			continue;

		status = ice_sched_cfg_vsi(pi, vsi_handle, i, maxqs[i], owner,
					   ice_is_tc_ena(tc_bitmap, i));
		if (status)
			break;
	}

	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_vsi_lan - configure VSI LAN queues
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap
 * @max_lanqs: max LAN queues array per TC
 *
 * This function adds/updates the VSI LAN queues per TC.
 */
int
ice_cfg_vsi_lan(struct ice_port_info *pi, u16 vsi_handle, u16 tc_bitmap,
		u16 *max_lanqs)
{
	return ice_cfg_vsi_qs(pi, vsi_handle, tc_bitmap, max_lanqs,
			      ICE_SCHED_NODE_OWNER_LAN);
}

/**
 * ice_cfg_vsi_rdma - configure the VSI RDMA queues
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap
 * @max_rdmaqs: max RDMA queues array per TC
 *
 * This function adds/updates the VSI RDMA queues per TC.
 */
int
ice_cfg_vsi_rdma(struct ice_port_info *pi, u16 vsi_handle, u16 tc_bitmap,
		 u16 *max_rdmaqs)
{
	return ice_cfg_vsi_qs(pi, vsi_handle, tc_bitmap, max_rdmaqs,
			      ICE_SCHED_NODE_OWNER_RDMA);
}

/**
 * ice_ena_vsi_rdma_qset
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @rdma_qset: pointer to RDMA qset
 * @num_qsets: number of RDMA qsets
 * @qset_teid: pointer to qset node teids
 *
 * This function adds RDMA qset
 */
int
ice_ena_vsi_rdma_qset(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      u16 *rdma_qset, u16 num_qsets, u32 *qset_teid)
{
	struct ice_aqc_txsched_elem_data node = { 0 };
	struct ice_aqc_add_rdma_qset_data *buf;
	struct ice_sched_node *parent;
	struct ice_hw *hw;
	u16 i, buf_size;
	int status;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return ICE_ERR_CFG;
	hw = pi->hw;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	buf_size = ice_struct_size(buf, rdma_qsets, num_qsets);
	buf = (struct ice_aqc_add_rdma_qset_data *)ice_malloc(hw, buf_size);
	if (!buf)
		return ICE_ERR_NO_MEMORY;
	ice_acquire_lock(&pi->sched_lock);

	parent = ice_sched_get_free_qparent(pi, vsi_handle, tc,
					    ICE_SCHED_NODE_OWNER_RDMA);
	if (!parent) {
		status = ICE_ERR_PARAM;
		goto rdma_error_exit;
	}
	buf->parent_teid = parent->info.node_teid;
	node.parent_teid = parent->info.node_teid;

	buf->num_qsets = CPU_TO_LE16(num_qsets);
	for (i = 0; i < num_qsets; i++) {
		buf->rdma_qsets[i].tx_qset_id = CPU_TO_LE16(rdma_qset[i]);
		buf->rdma_qsets[i].info.valid_sections =
			ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
			ICE_AQC_ELEM_VALID_EIR;
		buf->rdma_qsets[i].info.generic = 0;
		buf->rdma_qsets[i].info.cir_bw.bw_profile_idx =
			CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->rdma_qsets[i].info.cir_bw.bw_alloc =
			CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);
		buf->rdma_qsets[i].info.eir_bw.bw_profile_idx =
			CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->rdma_qsets[i].info.eir_bw.bw_alloc =
			CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);
	}
	status = ice_aq_add_rdma_qsets(hw, 1, buf, buf_size, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_RDMA, "add RDMA qset failed\n");
		goto rdma_error_exit;
	}
	node.data.elem_type = ICE_AQC_ELEM_TYPE_LEAF;
	for (i = 0; i < num_qsets; i++) {
		node.node_teid = buf->rdma_qsets[i].qset_teid;
		status = ice_sched_add_node(pi, hw->num_tx_sched_layers - 1,
					    &node, NULL);
		if (status)
			break;
		qset_teid[i] = LE32_TO_CPU(node.node_teid);
	}
rdma_error_exit:
	ice_release_lock(&pi->sched_lock);
	ice_free(hw, buf);
	return status;
}

/**
 * ice_dis_vsi_rdma_qset - free RDMA resources
 * @pi: port_info struct
 * @count: number of RDMA qsets to free
 * @qset_teid: TEID of qset node
 * @q_id: list of queue IDs being disabled
 */
int
ice_dis_vsi_rdma_qset(struct ice_port_info *pi, u16 count, u32 *qset_teid,
		      u16 *q_id)
{
	struct ice_aqc_dis_txq_item *qg_list;
	struct ice_hw *hw;
	int status = 0;
	u16 qg_size;
	int i;

	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return ICE_ERR_CFG;

	hw = pi->hw;

	qg_size = ice_struct_size(qg_list, q_id, 1);
	qg_list = (struct ice_aqc_dis_txq_item *)ice_malloc(hw, qg_size);
	if (!qg_list)
		return ICE_ERR_NO_MEMORY;

	ice_acquire_lock(&pi->sched_lock);

	for (i = 0; i < count; i++) {
		struct ice_sched_node *node;

		node = ice_sched_find_node_by_teid(pi->root, qset_teid[i]);
		if (!node)
			continue;

		qg_list->parent_teid = node->info.parent_teid;
		qg_list->num_qs = 1;
		qg_list->q_id[0] =
			CPU_TO_LE16(q_id[i] |
				    ICE_AQC_Q_DIS_BUF_ELEM_TYPE_RDMA_QSET);

		status = ice_aq_dis_lan_txq(hw, 1, qg_list, qg_size,
					    ICE_NO_RESET, 0, NULL);
		if (status)
			break;

		ice_free_sched_node(pi, node);
	}

	ice_release_lock(&pi->sched_lock);
	ice_free(hw, qg_list);
	return status;
}

/**
 * ice_aq_get_sensor_reading
 * @hw: pointer to the HW struct
 * @sensor: sensor type
 * @format: requested response format
 * @data: pointer to data to be read from the sensor
 * @cd: pointer to command details structure or NULL
 *
 * Get sensor reading (0x0632)
 */
int
ice_aq_get_sensor_reading(struct ice_hw *hw, u8 sensor, u8 format,
			  struct ice_aqc_get_sensor_reading_resp *data,
			  struct ice_sq_cd *cd)
{
	struct ice_aqc_get_sensor_reading *cmd;
	struct ice_aq_desc desc;
	int status;

	if (!data)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_sensor_reading);
	cmd = &desc.params.get_sensor_reading;
	cmd->sensor = sensor;
	cmd->format = format;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);

	if (!status)
		ice_memcpy(data, &desc.params.get_sensor_reading_resp,
			   sizeof(*data), ICE_NONDMA_TO_NONDMA);

	return status;
}

/**
 * ice_is_main_vsi - checks whether the VSI is main VSI
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * Checks whether the VSI is the main VSI (the first PF VSI created on
 * given PF).
 */
static bool ice_is_main_vsi(struct ice_hw *hw, u16 vsi_handle)
{
	return vsi_handle == ICE_MAIN_VSI_HANDLE && hw->vsi_ctx[vsi_handle];
}

/**
 * ice_replay_pre_init - replay pre initialization
 * @hw: pointer to the HW struct
 * @sw: pointer to switch info struct for which function initializes filters
 *
 * Initializes required config data for VSI, FD, ACL, and RSS before replay.
 */
int
ice_replay_pre_init(struct ice_hw *hw, struct ice_switch_info *sw)
{
	int status;
	u8 i;

	/* Delete old entries from replay filter list head if there is any */
	ice_rm_sw_replay_rule_info(hw, sw);
	/* In start of replay, move entries into replay_rules list, it
	 * will allow adding rules entries back to filt_rules list,
	 * which is operational list.
	 */
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++)
		LIST_REPLACE_INIT(&sw->recp_list[i].filt_rules,
				  &sw->recp_list[i].filt_replay_rules);
	ice_sched_replay_agg_vsi_preinit(hw);

	status = ice_sched_replay_root_node_bw(hw->port_info);
	if (status)
		return status;

	return ice_sched_replay_tc_node_bw(hw->port_info);
}

/**
 * ice_replay_vsi - replay VSI configuration
 * @hw: pointer to the HW struct
 * @vsi_handle: driver VSI handle
 *
 * Restore all VSI configuration after reset. It is required to call this
 * function with main VSI first.
 */
int ice_replay_vsi(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_port_info *pi = hw->port_info;
	int status;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	/* Replay pre-initialization if there is any */
	if (ice_is_main_vsi(hw, vsi_handle)) {
		status = ice_replay_pre_init(hw, sw);
		if (status)
			return status;
	}
	/* Replay per VSI all RSS configurations */
	status = ice_replay_rss_cfg(hw, vsi_handle);
	if (status)
		return status;
	/* Replay per VSI all filters */
	status = ice_replay_vsi_all_fltr(hw, pi, vsi_handle);
	if (!status)
		status = ice_replay_vsi_agg(hw, vsi_handle);
	return status;
}

/**
 * ice_replay_post - post replay configuration cleanup
 * @hw: pointer to the HW struct
 *
 * Post replay cleanup.
 */
void ice_replay_post(struct ice_hw *hw)
{
	/* Delete old entries from replay filter list head */
	ice_rm_all_sw_replay_rule_info(hw);
	ice_sched_replay_agg(hw);
}

/**
 * ice_stat_update40 - read 40 bit stat from the chip and update stat values
 * @hw: ptr to the hardware info
 * @reg: offset of 64 bit HW register to read from
 * @prev_stat_loaded: bool to specify if previous stats are loaded
 * @prev_stat: ptr to previous loaded stat value
 * @cur_stat: ptr to current stat value
 */
void
ice_stat_update40(struct ice_hw *hw, u32 reg, bool prev_stat_loaded,
		  u64 *prev_stat, u64 *cur_stat)
{
	u64 new_data = rd64(hw, reg) & (BIT_ULL(40) - 1);

	/* device stats are not reset at PFR, they likely will not be zeroed
	 * when the driver starts. Thus, save the value from the first read
	 * without adding to the statistic value so that we report stats which
	 * count up from zero.
	 */
	if (!prev_stat_loaded) {
		*prev_stat = new_data;
		return;
	}

	/* Calculate the difference between the new and old values, and then
	 * add it to the software stat value.
	 */
	if (new_data >= *prev_stat)
		*cur_stat += new_data - *prev_stat;
	else
		/* to manage the potential roll-over */
		*cur_stat += (new_data + BIT_ULL(40)) - *prev_stat;

	/* Update the previously stored value to prepare for next read */
	*prev_stat = new_data;
}

/**
 * ice_stat_update32 - read 32 bit stat from the chip and update stat values
 * @hw: ptr to the hardware info
 * @reg: offset of HW register to read from
 * @prev_stat_loaded: bool to specify if previous stats are loaded
 * @prev_stat: ptr to previous loaded stat value
 * @cur_stat: ptr to current stat value
 */
void
ice_stat_update32(struct ice_hw *hw, u32 reg, bool prev_stat_loaded,
		  u64 *prev_stat, u64 *cur_stat)
{
	u32 new_data;

	new_data = rd32(hw, reg);

	/* device stats are not reset at PFR, they likely will not be zeroed
	 * when the driver starts. Thus, save the value from the first read
	 * without adding to the statistic value so that we report stats which
	 * count up from zero.
	 */
	if (!prev_stat_loaded) {
		*prev_stat = new_data;
		return;
	}

	/* Calculate the difference between the new and old values, and then
	 * add it to the software stat value.
	 */
	if (new_data >= *prev_stat)
		*cur_stat += new_data - *prev_stat;
	else
		/* to manage the potential roll-over */
		*cur_stat += (new_data + BIT_ULL(32)) - *prev_stat;

	/* Update the previously stored value to prepare for next read */
	*prev_stat = new_data;
}

/**
 * ice_stat_update_repc - read GLV_REPC stats from chip and update stat values
 * @hw: ptr to the hardware info
 * @vsi_handle: VSI handle
 * @prev_stat_loaded: bool to specify if the previous stat values are loaded
 * @cur_stats: ptr to current stats structure
 *
 * The GLV_REPC statistic register actually tracks two 16bit statistics, and
 * thus cannot be read using the normal ice_stat_update32 function.
 *
 * Read the GLV_REPC register associated with the given VSI, and update the
 * rx_no_desc and rx_error values in the ice_eth_stats structure.
 *
 * Because the statistics in GLV_REPC stick at 0xFFFF, the register must be
 * cleared each time it's read.
 *
 * Note that the GLV_RDPC register also counts the causes that would trigger
 * GLV_REPC. However, it does not give the finer grained detail about why the
 * packets are being dropped. The GLV_REPC values can be used to distinguish
 * whether Rx packets are dropped due to errors or due to no available
 * descriptors.
 */
void
ice_stat_update_repc(struct ice_hw *hw, u16 vsi_handle, bool prev_stat_loaded,
		     struct ice_eth_stats *cur_stats)
{
	u16 vsi_num, no_desc, error_cnt;
	u32 repc;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return;

	vsi_num = ice_get_hw_vsi_num(hw, vsi_handle);

	/* If we haven't loaded stats yet, just clear the current value */
	if (!prev_stat_loaded) {
		wr32(hw, GLV_REPC(vsi_num), 0);
		return;
	}

	repc = rd32(hw, GLV_REPC(vsi_num));
	no_desc = (repc & GLV_REPC_NO_DESC_CNT_M) >> GLV_REPC_NO_DESC_CNT_S;
	error_cnt = (repc & GLV_REPC_ERROR_CNT_M) >> GLV_REPC_ERROR_CNT_S;

	/* Clear the count by writing to the stats register */
	wr32(hw, GLV_REPC(vsi_num), 0);

	cur_stats->rx_no_desc += no_desc;
	cur_stats->rx_errors += error_cnt;
}

/**
 * ice_aq_alternate_write
 * @hw: pointer to the hardware structure
 * @reg_addr0: address of first dword to be written
 * @reg_val0: value to be written under 'reg_addr0'
 * @reg_addr1: address of second dword to be written
 * @reg_val1: value to be written under 'reg_addr1'
 *
 * Write one or two dwords to alternate structure. Fields are indicated
 * by 'reg_addr0' and 'reg_addr1' register numbers.
 */
int
ice_aq_alternate_write(struct ice_hw *hw, u32 reg_addr0, u32 reg_val0,
		       u32 reg_addr1, u32 reg_val1)
{
	struct ice_aqc_read_write_alt_direct *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.read_write_alt_direct;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_write_alt_direct);
	cmd->dword0_addr = CPU_TO_LE32(reg_addr0);
	cmd->dword1_addr = CPU_TO_LE32(reg_addr1);
	cmd->dword0_value = CPU_TO_LE32(reg_val0);
	cmd->dword1_value = CPU_TO_LE32(reg_val1);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);

	return status;
}

/**
 * ice_aq_alternate_read
 * @hw: pointer to the hardware structure
 * @reg_addr0: address of first dword to be read
 * @reg_val0: pointer for data read from 'reg_addr0'
 * @reg_addr1: address of second dword to be read
 * @reg_val1: pointer for data read from 'reg_addr1'
 *
 * Read one or two dwords from alternate structure. Fields are indicated
 * by 'reg_addr0' and 'reg_addr1' register numbers. If 'reg_val1' pointer
 * is not passed then only register at 'reg_addr0' is read.
 */
int
ice_aq_alternate_read(struct ice_hw *hw, u32 reg_addr0, u32 *reg_val0,
		      u32 reg_addr1, u32 *reg_val1)
{
	struct ice_aqc_read_write_alt_direct *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.read_write_alt_direct;

	if (!reg_val0)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_read_alt_direct);
	cmd->dword0_addr = CPU_TO_LE32(reg_addr0);
	cmd->dword1_addr = CPU_TO_LE32(reg_addr1);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);

	if (!status) {
		*reg_val0 = LE32_TO_CPU(cmd->dword0_value);

		if (reg_val1)
			*reg_val1 = LE32_TO_CPU(cmd->dword1_value);
	}

	return status;
}

/**
 *  ice_aq_alternate_write_done
 *  @hw: pointer to the HW structure.
 *  @bios_mode: indicates whether the command is executed by UEFI or legacy BIOS
 *  @reset_needed: indicates the SW should trigger GLOBAL reset
 *
 *  Indicates to the FW that alternate structures have been changed.
 */
int
ice_aq_alternate_write_done(struct ice_hw *hw, u8 bios_mode, bool *reset_needed)
{
	struct ice_aqc_done_alt_write *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.done_alt_write;

	if (!reset_needed)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_done_alt_write);
	cmd->flags = bios_mode;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (!status)
		*reset_needed = (LE16_TO_CPU(cmd->flags) &
				 ICE_AQC_RESP_RESET_NEEDED) != 0;

	return status;
}

/**
 *  ice_aq_alternate_clear
 *  @hw: pointer to the HW structure.
 *
 *  Clear the alternate structures of the port from which the function
 *  is called.
 */
int ice_aq_alternate_clear(struct ice_hw *hw)
{
	struct ice_aq_desc desc;
	int status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_clear_port_alt_write);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);

	return status;
}

/**
 * ice_sched_query_elem - query element information from HW
 * @hw: pointer to the HW struct
 * @node_teid: node TEID to be queried
 * @buf: buffer to element information
 *
 * This function queries HW element information
 */
int
ice_sched_query_elem(struct ice_hw *hw, u32 node_teid,
		     struct ice_aqc_txsched_elem_data *buf)
{
	u16 buf_size, num_elem_ret = 0;
	int status;

	buf_size = sizeof(*buf);
	ice_memset(buf, 0, buf_size, ICE_NONDMA_MEM);
	buf->node_teid = CPU_TO_LE32(node_teid);
	status = ice_aq_query_sched_elems(hw, 1, buf, buf_size, &num_elem_ret,
					  NULL);
	if (status || num_elem_ret != 1)
		ice_debug(hw, ICE_DBG_SCHED, "query element failed\n");
	return status;
}

/**
 * ice_get_fw_mode - returns FW mode
 * @hw: pointer to the HW struct
 */
enum ice_fw_modes ice_get_fw_mode(struct ice_hw *hw)
{
#define ICE_FW_MODE_DBG_M BIT(0)
#define ICE_FW_MODE_REC_M BIT(1)
#define ICE_FW_MODE_ROLLBACK_M BIT(2)
	u32 fw_mode;

	/* check the current FW mode */
	fw_mode = rd32(hw, GL_MNG_FWSM) & E800_GL_MNG_FWSM_FW_MODES_M;
	if (fw_mode & ICE_FW_MODE_DBG_M)
		return ICE_FW_MODE_DBG;
	else if (fw_mode & ICE_FW_MODE_REC_M)
		return ICE_FW_MODE_REC;
	else if (fw_mode & ICE_FW_MODE_ROLLBACK_M)
		return ICE_FW_MODE_ROLLBACK;
	else
		return ICE_FW_MODE_NORMAL;
}

/**
 * ice_get_cur_lldp_persist_status
 * @hw: pointer to the HW struct
 * @lldp_status: return value of LLDP persistent status
 *
 * Get the current status of LLDP persistent
 */
int
ice_get_cur_lldp_persist_status(struct ice_hw *hw, u32 *lldp_status)
{
	struct ice_port_info *pi = hw->port_info;
	__le32 raw_data;
	u32 data, mask;
	int ret;

	if (!lldp_status)
		return ICE_ERR_BAD_PTR;

	ret = ice_acquire_nvm(hw, ICE_RES_READ);
	if (ret)
		return ret;

	ret = ice_aq_read_nvm(hw, ICE_AQC_NVM_LLDP_PRESERVED_MOD_ID,
			      ICE_AQC_NVM_CUR_LLDP_PERSIST_RD_OFFSET,
			      ICE_AQC_NVM_LLDP_STATUS_RD_LEN, &raw_data,
			      false, true, NULL);
	if (!ret) {
		data = LE32_TO_CPU(raw_data);
		mask = ICE_AQC_NVM_LLDP_STATUS_M <<
			(ICE_AQC_NVM_LLDP_STATUS_M_LEN * pi->lport);
		data = data & mask;
		*lldp_status = data >>
			(ICE_AQC_NVM_LLDP_STATUS_M_LEN * pi->lport);
	}

	ice_release_nvm(hw);

	return ret;
}

/**
 * ice_get_dflt_lldp_persist_status
 * @hw: pointer to the HW struct
 * @lldp_status: return value of LLDP persistent status
 *
 * Get the default status of LLDP persistent
 */
int
ice_get_dflt_lldp_persist_status(struct ice_hw *hw, u32 *lldp_status)
{
	struct ice_port_info *pi = hw->port_info;
	u32 data, mask, loc_data, loc_data_tmp;
	__le16 loc_raw_data;
	__le32 raw_data;
	int ret;

	if (!lldp_status)
		return ICE_ERR_BAD_PTR;

	ret = ice_acquire_nvm(hw, ICE_RES_READ);
	if (ret)
		return ret;

	/* Read the offset of EMP_SR_PTR */
	ret = ice_aq_read_nvm(hw, ICE_AQC_NVM_START_POINT,
			      ICE_AQC_NVM_EMP_SR_PTR_OFFSET,
			      ICE_AQC_NVM_EMP_SR_PTR_RD_LEN,
			      &loc_raw_data, false, true, NULL);
	if (ret)
		goto exit;

	loc_data = LE16_TO_CPU(loc_raw_data);
	if (loc_data & ICE_AQC_NVM_EMP_SR_PTR_TYPE_M) {
		loc_data &= ICE_AQC_NVM_EMP_SR_PTR_M;
		loc_data *= ICE_AQC_NVM_SECTOR_UNIT;
	} else {
		loc_data *= ICE_AQC_NVM_WORD_UNIT;
	}

	/* Read the offset of LLDP configuration pointer */
	loc_data += ICE_AQC_NVM_LLDP_CFG_PTR_OFFSET;
	ret = ice_aq_read_nvm(hw, ICE_AQC_NVM_START_POINT, loc_data,
			      ICE_AQC_NVM_LLDP_CFG_PTR_RD_LEN, &loc_raw_data,
			      false, true, NULL);
	if (ret)
		goto exit;

	loc_data_tmp = LE16_TO_CPU(loc_raw_data);
	loc_data_tmp *= ICE_AQC_NVM_WORD_UNIT;
	loc_data += loc_data_tmp;

	/* We need to skip LLDP configuration section length (2 bytes) */
	loc_data += ICE_AQC_NVM_LLDP_CFG_HEADER_LEN;

	/* Read the LLDP Default Configure */
	ret = ice_aq_read_nvm(hw, ICE_AQC_NVM_START_POINT, loc_data,
			      ICE_AQC_NVM_LLDP_STATUS_RD_LEN, &raw_data, false,
			      true, NULL);
	if (!ret) {
		data = LE32_TO_CPU(raw_data);
		mask = ICE_AQC_NVM_LLDP_STATUS_M <<
			(ICE_AQC_NVM_LLDP_STATUS_M_LEN * pi->lport);
		data = data & mask;
		*lldp_status = data >>
			(ICE_AQC_NVM_LLDP_STATUS_M_LEN * pi->lport);
	}

exit:
	ice_release_nvm(hw);

	return ret;
}

/**
 * ice_aq_read_i2c
 * @hw: pointer to the hw struct
 * @topo_addr: topology address for a device to communicate with
 * @bus_addr: 7-bit I2C bus address
 * @addr: I2C memory address (I2C offset) with up to 16 bits
 * @params: I2C parameters: bit [7] - Repeated start, bits [6:5] data offset size,
 *			    bit [4] - I2C address type, bits [3:0] - data size to read (0-16 bytes)
 * @data: pointer to data (0 to 16 bytes) to be read from the I2C device
 * @cd: pointer to command details structure or NULL
 *
 * Read I2C (0x06E2)
 */
int
ice_aq_read_i2c(struct ice_hw *hw, struct ice_aqc_link_topo_addr topo_addr,
		u16 bus_addr, __le16 addr, u8 params, u8 *data,
		struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc = { 0 };
	struct ice_aqc_i2c *cmd;
	u8 data_size;
	int status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_read_i2c);
	cmd = &desc.params.read_write_i2c;

	if (!data)
		return ICE_ERR_PARAM;

	data_size = (params & ICE_AQC_I2C_DATA_SIZE_M) >> ICE_AQC_I2C_DATA_SIZE_S;

	cmd->i2c_bus_addr = CPU_TO_LE16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (!status) {
		struct ice_aqc_read_i2c_resp *resp;
		u8 i;

		resp = &desc.params.read_i2c_resp;
		for (i = 0; i < data_size; i++) {
			*data = resp->i2c_data[i];
			data++;
		}
	}

	return status;
}

/**
 * ice_aq_write_i2c
 * @hw: pointer to the hw struct
 * @topo_addr: topology address for a device to communicate with
 * @bus_addr: 7-bit I2C bus address
 * @addr: I2C memory address (I2C offset) with up to 16 bits
 * @params: I2C parameters: bit [4] - I2C address type, bits [3:0] - data size to write (0-7 bytes)
 * @data: pointer to data (0 to 4 bytes) to be written to the I2C device
 * @cd: pointer to command details structure or NULL
 *
 * Write I2C (0x06E3)
 */
int
ice_aq_write_i2c(struct ice_hw *hw, struct ice_aqc_link_topo_addr topo_addr,
		 u16 bus_addr, __le16 addr, u8 params, const u8 *data,
		 struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc = { 0 };
	struct ice_aqc_i2c *cmd;
	u8 i, data_size;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_write_i2c);
	cmd = &desc.params.read_write_i2c;

	data_size = (params & ICE_AQC_I2C_DATA_SIZE_M) >> ICE_AQC_I2C_DATA_SIZE_S;

	/* data_size limited to 4 */
	if (data_size > 4)
		return ICE_ERR_PARAM;

	cmd->i2c_bus_addr = CPU_TO_LE16(bus_addr);
	cmd->topo_addr = topo_addr;
	cmd->i2c_params = params;
	cmd->i2c_addr = addr;

	for (i = 0; i < data_size; i++) {
		cmd->i2c_data[i] = *data;
		data++;
	}

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_set_gpio
 * @hw: pointer to the hw struct
 * @gpio_ctrl_handle: GPIO controller node handle
 * @pin_idx: IO Number of the GPIO that needs to be set
 * @value: SW provide IO value to set in the LSB
 * @cd: pointer to command details structure or NULL
 *
 * Sends 0x06EC AQ command to set the GPIO pin state that's part of the topology
 */
int
ice_aq_set_gpio(struct ice_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx, bool value,
		struct ice_sq_cd *cd)
{
	struct ice_aqc_gpio *cmd;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = CPU_TO_LE16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;
	cmd->gpio_val = value ? 1 : 0;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_get_gpio
 * @hw: pointer to the hw struct
 * @gpio_ctrl_handle: GPIO controller node handle
 * @pin_idx: IO Number of the GPIO that needs to be set
 * @value: IO value read
 * @cd: pointer to command details structure or NULL
 *
 * Sends 0x06ED AQ command to get the value of a GPIO signal which is part of
 * the topology
 */
int
ice_aq_get_gpio(struct ice_hw *hw, u16 gpio_ctrl_handle, u8 pin_idx,
		bool *value, struct ice_sq_cd *cd)
{
	struct ice_aqc_gpio *cmd;
	struct ice_aq_desc desc;
	int status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_gpio);
	cmd = &desc.params.read_write_gpio;
	cmd->gpio_ctrl_handle = CPU_TO_LE16(gpio_ctrl_handle);
	cmd->gpio_num = pin_idx;

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (status)
		return status;

	*value = !!cmd->gpio_val;
	return 0;
}

/**
 * ice_is_fw_api_min_ver
 * @hw: pointer to the hardware structure
 * @maj: major version
 * @min: minor version
 * @patch: patch version
 *
 * Checks if the firmware is minimum version
 */
static bool ice_is_fw_api_min_ver(struct ice_hw *hw, u8 maj, u8 min, u8 patch)
{
	if (hw->api_maj_ver == maj) {
		if (hw->api_min_ver > min)
			return true;
		if (hw->api_min_ver == min && hw->api_patch >= patch)
			return true;
	} else if (hw->api_maj_ver > maj) {
		return true;
	}

	return false;
}

/**
 * ice_is_fw_min_ver
 * @hw: pointer to the hardware structure
 * @branch: branch version
 * @maj: major version
 * @min: minor version
 * @patch: patch version
 *
 * Checks if the firmware is minimum version
 */
static bool ice_is_fw_min_ver(struct ice_hw *hw, u8 branch, u8 maj, u8 min,
			      u8 patch)
{
	if (hw->fw_branch == branch) {
		if (hw->fw_maj_ver > maj)
			return true;
		if (hw->fw_maj_ver == maj) {
			if (hw->fw_min_ver > min)
				return true;
			if (hw->fw_min_ver == min && hw->fw_patch >= patch)
				return true;
		}
	}

	return false;
}

/**
 * ice_fw_supports_link_override
 * @hw: pointer to the hardware structure
 *
 * Checks if the firmware supports link override
 */
bool ice_fw_supports_link_override(struct ice_hw *hw)
{
	return ice_is_fw_api_min_ver(hw, ICE_FW_API_LINK_OVERRIDE_MAJ,
				     ICE_FW_API_LINK_OVERRIDE_MIN,
				     ICE_FW_API_LINK_OVERRIDE_PATCH);
}

/**
 * ice_get_link_default_override
 * @ldo: pointer to the link default override struct
 * @pi: pointer to the port info struct
 *
 * Gets the link default override for a port
 */
int
ice_get_link_default_override(struct ice_link_default_override_tlv *ldo,
			      struct ice_port_info *pi)
{
	u16 i, tlv, tlv_len, tlv_start, buf, offset;
	struct ice_hw *hw = pi->hw;
	int status;

	status = ice_get_pfa_module_tlv(hw, &tlv, &tlv_len,
					ICE_SR_LINK_DEFAULT_OVERRIDE_PTR);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read link override TLV.\n");
		return status;
	}

	/* Each port has its own config; calculate for our port */
	tlv_start = tlv + pi->lport * ICE_SR_PFA_LINK_OVERRIDE_WORDS +
		ICE_SR_PFA_LINK_OVERRIDE_OFFSET;

	/* link options first */
	status = ice_read_sr_word(hw, tlv_start, &buf);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
		return status;
	}
	ldo->options = buf & ICE_LINK_OVERRIDE_OPT_M;
	ldo->phy_config = (buf & ICE_LINK_OVERRIDE_PHY_CFG_M) >>
		ICE_LINK_OVERRIDE_PHY_CFG_S;

	/* link PHY config */
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_FEC_OFFSET;
	status = ice_read_sr_word(hw, offset, &buf);
	if (status) {
		ice_debug(hw, ICE_DBG_INIT, "Failed to read override phy config.\n");
		return status;
	}
	ldo->fec_options = buf & ICE_LINK_OVERRIDE_FEC_OPT_M;

	/* PHY types low */
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_PHY_OFFSET;
	for (i = 0; i < ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS; i++) {
		status = ice_read_sr_word(hw, (offset + i), &buf);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
			return status;
		}
		/* shift 16 bits at a time to fill 64 bits */
		ldo->phy_type_low |= ((u64)buf << (i * 16));
	}

	/* PHY types high */
	offset = tlv_start + ICE_SR_PFA_LINK_OVERRIDE_PHY_OFFSET +
		ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS;
	for (i = 0; i < ICE_SR_PFA_LINK_OVERRIDE_PHY_WORDS; i++) {
		status = ice_read_sr_word(hw, (offset + i), &buf);
		if (status) {
			ice_debug(hw, ICE_DBG_INIT, "Failed to read override link options.\n");
			return status;
		}
		/* shift 16 bits at a time to fill 64 bits */
		ldo->phy_type_high |= ((u64)buf << (i * 16));
	}

	return status;
}

/**
 * ice_is_phy_caps_an_enabled - check if PHY capabilities autoneg is enabled
 * @caps: get PHY capability data
 */
bool ice_is_phy_caps_an_enabled(struct ice_aqc_get_phy_caps_data *caps)
{
	if (caps->caps & ICE_AQC_PHY_AN_MODE ||
	    caps->low_power_ctrl_an & (ICE_AQC_PHY_AN_EN_CLAUSE28 |
				       ICE_AQC_PHY_AN_EN_CLAUSE73 |
				       ICE_AQC_PHY_AN_EN_CLAUSE37))
		return true;

	return false;
}

/**
 * ice_is_fw_health_report_supported
 * @hw: pointer to the hardware structure
 *
 * Return true if firmware supports health status reports,
 * false otherwise
 */
bool ice_is_fw_health_report_supported(struct ice_hw *hw)
{
	if (hw->api_maj_ver > ICE_FW_API_HEALTH_REPORT_MAJ)
		return true;

	if (hw->api_maj_ver == ICE_FW_API_HEALTH_REPORT_MAJ) {
		if (hw->api_min_ver > ICE_FW_API_HEALTH_REPORT_MIN)
			return true;
		if (hw->api_min_ver == ICE_FW_API_HEALTH_REPORT_MIN &&
		    hw->api_patch >= ICE_FW_API_HEALTH_REPORT_PATCH)
			return true;
	}

	return false;
}

/**
 * ice_aq_set_health_status_config - Configure FW health events
 * @hw: pointer to the HW struct
 * @event_source: type of diagnostic events to enable
 * @cd: pointer to command details structure or NULL
 *
 * Configure the health status event types that the firmware will send to this
 * PF. The supported event types are: PF-specific, all PFs, and global
 */
int
ice_aq_set_health_status_config(struct ice_hw *hw, u8 event_source,
				struct ice_sq_cd *cd)
{
	struct ice_aqc_set_health_status_config *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.set_health_status_config;

	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_set_health_status_config);

	cmd->event_source = event_source;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_get_port_options
 * @hw: pointer to the hw struct
 * @options: buffer for the resultant port options
 * @option_count: input - size of the buffer in port options structures,
 *                output - number of returned port options
 * @lport: logical port to call the command with (optional)
 * @lport_valid: when false, FW uses port owned by the PF instead of lport,
 *               when PF owns more than 1 port it must be true
 * @active_option_idx: index of active port option in returned buffer
 * @active_option_valid: active option in returned buffer is valid
 * @pending_option_idx: index of pending port option in returned buffer
 * @pending_option_valid: pending option in returned buffer is valid
 *
 * Calls Get Port Options AQC (0x06ea) and verifies result.
 */
int
ice_aq_get_port_options(struct ice_hw *hw,
			struct ice_aqc_get_port_options_elem *options,
			u8 *option_count, u8 lport, bool lport_valid,
			u8 *active_option_idx, bool *active_option_valid,
			u8 *pending_option_idx, bool *pending_option_valid)
{
	struct ice_aqc_get_port_options *cmd;
	struct ice_aq_desc desc;
	int status;
	u8 i;

	/* options buffer shall be able to hold max returned options */
	if (*option_count < ICE_AQC_PORT_OPT_COUNT_M)
		return ICE_ERR_PARAM;

	cmd = &desc.params.get_port_options;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_port_options);

	cmd->lport_num = lport;
	cmd->lport_num_valid = lport_valid;

	status = ice_aq_send_cmd(hw, &desc, options,
				 *option_count * sizeof(*options), NULL);
	if (status)
		return status;

	/* verify direct FW response & set output parameters */
	*option_count = cmd->port_options_count & ICE_AQC_PORT_OPT_COUNT_M;
	ice_debug(hw, ICE_DBG_PHY, "options: %x\n", *option_count);
	*active_option_valid = cmd->port_options & ICE_AQC_PORT_OPT_VALID;
	if (*active_option_valid) {
		*active_option_idx = cmd->port_options &
				     ICE_AQC_PORT_OPT_ACTIVE_M;
		if (*active_option_idx > (*option_count - 1))
			return ICE_ERR_OUT_OF_RANGE;
		ice_debug(hw, ICE_DBG_PHY, "active idx: %x\n",
			  *active_option_idx);
	}

	*pending_option_valid = cmd->pending_port_option_status &
				ICE_AQC_PENDING_PORT_OPT_VALID;
	if (*pending_option_valid) {
		*pending_option_idx = cmd->pending_port_option_status &
				      ICE_AQC_PENDING_PORT_OPT_IDX_M;
		if (*pending_option_idx > (*option_count - 1))
			return ICE_ERR_OUT_OF_RANGE;
		ice_debug(hw, ICE_DBG_PHY, "pending idx: %x\n",
			  *pending_option_idx);
	}

	/* mask output options fields */
	for (i = 0; i < *option_count; i++) {
		options[i].pmd &= ICE_AQC_PORT_OPT_PMD_COUNT_M;
		options[i].max_lane_speed &= ICE_AQC_PORT_OPT_MAX_LANE_M;
		ice_debug(hw, ICE_DBG_PHY, "pmds: %x max speed: %x\n",
			  options[i].pmd, options[i].max_lane_speed);
	}

	return 0;
}

/**
 * ice_aq_set_port_option
 * @hw: pointer to the hw struct
 * @lport: logical port to call the command with
 * @lport_valid: when false, FW uses port owned by the PF instead of lport,
 *               when PF owns more than 1 port it must be true
 * @new_option: new port option to be written
 *
 * Calls Set Port Options AQC (0x06eb).
 */
int
ice_aq_set_port_option(struct ice_hw *hw, u8 lport, u8 lport_valid,
		       u8 new_option)
{
	struct ice_aqc_set_port_option *cmd;
	struct ice_aq_desc desc;

	if (new_option >= ICE_AQC_PORT_OPT_COUNT_M)
		return ICE_ERR_PARAM;

	cmd = &desc.params.set_port_option;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_port_option);

	cmd->lport_num = lport;

	cmd->lport_num_valid = lport_valid;
	cmd->selected_port_option = new_option;

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_aq_set_lldp_mib - Set the LLDP MIB
 * @hw: pointer to the HW struct
 * @mib_type: Local, Remote or both Local and Remote MIBs
 * @buf: pointer to the caller-supplied buffer to store the MIB block
 * @buf_size: size of the buffer (in bytes)
 * @cd: pointer to command details structure or NULL
 *
 * Set the LLDP MIB. (0x0A08)
 */
int
ice_aq_set_lldp_mib(struct ice_hw *hw, u8 mib_type, void *buf, u16 buf_size,
		    struct ice_sq_cd *cd)
{
	struct ice_aqc_lldp_set_local_mib *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_set_mib;

	if (buf_size == 0 || !buf)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_set_local_mib);

	desc.flags |= CPU_TO_LE16((u16)ICE_AQ_FLAG_RD);
	desc.datalen = CPU_TO_LE16(buf_size);

	cmd->type = mib_type;
	cmd->length = CPU_TO_LE16(buf_size);

	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_fw_supports_lldp_fltr_ctrl - check NVM version supports lldp_fltr_ctrl
 * @hw: pointer to HW struct
 */
bool ice_fw_supports_lldp_fltr_ctrl(struct ice_hw *hw)
{
	if (hw->mac_type != ICE_MAC_E810 && hw->mac_type != ICE_MAC_GENERIC)
		return false;

	return ice_is_fw_api_min_ver(hw, ICE_FW_API_LLDP_FLTR_MAJ,
				     ICE_FW_API_LLDP_FLTR_MIN,
				     ICE_FW_API_LLDP_FLTR_PATCH);
}

/**
 * ice_lldp_fltr_add_remove - add or remove a LLDP Rx switch filter
 * @hw: pointer to HW struct
 * @vsi_num: absolute HW index for VSI
 * @add: boolean for if adding or removing a filter
 */
int
ice_lldp_fltr_add_remove(struct ice_hw *hw, u16 vsi_num, bool add)
{
	struct ice_aqc_lldp_filter_ctrl *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.lldp_filter_ctrl;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_lldp_filter_ctrl);

	if (add)
		cmd->cmd_flags = ICE_AQC_LLDP_FILTER_ACTION_ADD;
	else
		cmd->cmd_flags = ICE_AQC_LLDP_FILTER_ACTION_DELETE;

	cmd->vsi_num = CPU_TO_LE16(vsi_num);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_lldp_execute_pending_mib - execute LLDP pending MIB request
 * @hw: pointer to HW struct
 */
int ice_lldp_execute_pending_mib(struct ice_hw *hw)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_execute_pending_lldp_mib);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_fw_supports_report_dflt_cfg
 * @hw: pointer to the hardware structure
 *
 * Checks if the firmware supports report default configuration
 */
bool ice_fw_supports_report_dflt_cfg(struct ice_hw *hw)
{
	return ice_is_fw_api_min_ver(hw, ICE_FW_API_REPORT_DFLT_CFG_MAJ,
				     ICE_FW_API_REPORT_DFLT_CFG_MIN,
				     ICE_FW_API_REPORT_DFLT_CFG_PATCH);
}

/* each of the indexes into the following array match the speed of a return
 * value from the list of AQ returned speeds like the range:
 * ICE_AQ_LINK_SPEED_10MB .. ICE_AQ_LINK_SPEED_100GB excluding
 * ICE_AQ_LINK_SPEED_UNKNOWN which is BIT(15) The array is defined as 15
 * elements long because the link_speed returned by the firmware is a 16 bit
 * value, but is indexed by [fls(speed) - 1]
 */
static const u32 ice_aq_to_link_speed[] = {
	ICE_LINK_SPEED_10MBPS,	/* BIT(0) */
	ICE_LINK_SPEED_100MBPS,
	ICE_LINK_SPEED_1000MBPS,
	ICE_LINK_SPEED_2500MBPS,
	ICE_LINK_SPEED_5000MBPS,
	ICE_LINK_SPEED_10000MBPS,
	ICE_LINK_SPEED_20000MBPS,
	ICE_LINK_SPEED_25000MBPS,
	ICE_LINK_SPEED_40000MBPS,
	ICE_LINK_SPEED_50000MBPS,
	ICE_LINK_SPEED_100000MBPS,	/* BIT(10) */
	ICE_LINK_SPEED_200000MBPS,
};

/**
 * ice_get_link_speed - get integer speed from table
 * @index: array index from fls(aq speed) - 1
 *
 * Returns: u32 value containing integer speed
 */
u32 ice_get_link_speed(u16 index)
{
	if (index >= ARRAY_SIZE(ice_aq_to_link_speed))
		return ICE_LINK_SPEED_UNKNOWN;

	return ice_aq_to_link_speed[index];
}

/**
 * ice_fw_supports_fec_dis_auto
 * @hw: pointer to the hardware structure
 *
 * Checks if the firmware supports FEC disable in Auto FEC mode
 */
bool ice_fw_supports_fec_dis_auto(struct ice_hw *hw)
{
	if (ice_is_e830(hw))
		return true;
	return ice_is_fw_min_ver(hw, ICE_FW_VER_BRANCH_E810,
				 ICE_FW_FEC_DIS_AUTO_MAJ,
				 ICE_FW_FEC_DIS_AUTO_MIN,
				 ICE_FW_FEC_DIS_AUTO_PATCH) ||
	       ice_is_fw_min_ver(hw, ICE_FW_VER_BRANCH_E82X,
				 ICE_FW_FEC_DIS_AUTO_MAJ_E82X,
				 ICE_FW_FEC_DIS_AUTO_MIN_E82X,
				 ICE_FW_FEC_DIS_AUTO_PATCH_E82X);
}

/**
 * ice_is_fw_auto_drop_supported
 * @hw: pointer to the hardware structure
 *
 * Checks if the firmware supports auto drop feature
 */
bool ice_is_fw_auto_drop_supported(struct ice_hw *hw)
{
	if (hw->api_maj_ver >= ICE_FW_API_AUTO_DROP_MAJ &&
	    hw->api_min_ver >= ICE_FW_API_AUTO_DROP_MIN)
		return true;
	return false;
}

