/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2020, Intel Corporation
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
/*$FreeBSD$*/

#include "ice_common.h"
#include "ice_adminq_cmd.h"
#include "ice_sriov.h"

/**
 * ice_aq_send_msg_to_vf
 * @hw: pointer to the hardware structure
 * @vfid: VF ID to send msg
 * @v_opcode: opcodes for VF-PF communication
 * @v_retval: return error code
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 * @cd: pointer to command details
 *
 * Send message to VF driver (0x0802) using mailbox
 * queue and asynchronously sending message via
 * ice_sq_send_cmd() function
 */
enum ice_status
ice_aq_send_msg_to_vf(struct ice_hw *hw, u16 vfid, u32 v_opcode, u32 v_retval,
		      u8 *msg, u16 msglen, struct ice_sq_cd *cd)
{
	struct ice_aqc_pf_vf_msg *cmd;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_mbx_opc_send_msg_to_vf);

	cmd = &desc.params.virt;
	cmd->id = CPU_TO_LE32(vfid);

	desc.cookie_high = CPU_TO_LE32(v_opcode);
	desc.cookie_low = CPU_TO_LE32(v_retval);

	if (msglen)
		desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	return ice_sq_send_cmd(hw, &hw->mailboxq, &desc, msg, msglen, cd);
}

/**
 * ice_aq_send_msg_to_pf
 * @hw: pointer to the hardware structure
 * @v_opcode: opcodes for VF-PF communication
 * @v_retval: return error code
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 * @cd: pointer to command details
 *
 * Send message to PF driver using mailbox queue. By default, this
 * message is sent asynchronously, i.e. ice_sq_send_cmd()
 * does not wait for completion before returning.
 */
enum ice_status
ice_aq_send_msg_to_pf(struct ice_hw *hw, enum virtchnl_ops v_opcode,
		      enum ice_status v_retval, u8 *msg, u16 msglen,
		      struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_mbx_opc_send_msg_to_pf);
	desc.cookie_high = CPU_TO_LE32(v_opcode);
	desc.cookie_low = CPU_TO_LE32(v_retval);

	if (msglen)
		desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	return ice_sq_send_cmd(hw, &hw->mailboxq, &desc, msg, msglen, cd);
}

/**
 * ice_conv_link_speed_to_virtchnl
 * @adv_link_support: determines the format of the returned link speed
 * @link_speed: variable containing the link_speed to be converted
 *
 * Convert link speed supported by HW to link speed supported by virtchnl.
 * If adv_link_support is true, then return link speed in Mbps. Else return
 * link speed as a VIRTCHNL_LINK_SPEED_* casted to a u32. Note that the caller
 * needs to cast back to an enum virtchnl_link_speed in the case where
 * adv_link_support is false, but when adv_link_support is true the caller can
 * expect the speed in Mbps.
 */
u32 ice_conv_link_speed_to_virtchnl(bool adv_link_support, u16 link_speed)
{
	u32 speed;

	if (adv_link_support)
		switch (link_speed) {
		case ICE_AQ_LINK_SPEED_10MB:
			speed = ICE_LINK_SPEED_10MBPS;
			break;
		case ICE_AQ_LINK_SPEED_100MB:
			speed = ICE_LINK_SPEED_100MBPS;
			break;
		case ICE_AQ_LINK_SPEED_1000MB:
			speed = ICE_LINK_SPEED_1000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_2500MB:
			speed = ICE_LINK_SPEED_2500MBPS;
			break;
		case ICE_AQ_LINK_SPEED_5GB:
			speed = ICE_LINK_SPEED_5000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_10GB:
			speed = ICE_LINK_SPEED_10000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_20GB:
			speed = ICE_LINK_SPEED_20000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_25GB:
			speed = ICE_LINK_SPEED_25000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_40GB:
			speed = ICE_LINK_SPEED_40000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_50GB:
			speed = ICE_LINK_SPEED_50000MBPS;
			break;
		case ICE_AQ_LINK_SPEED_100GB:
			speed = ICE_LINK_SPEED_100000MBPS;
			break;
		default:
			speed = ICE_LINK_SPEED_UNKNOWN;
			break;
		}
	else
		/* Virtchnl speeds are not defined for every speed supported in
		 * the hardware. To maintain compatibility with older AVF
		 * drivers, while reporting the speed the new speed values are
		 * resolved to the closest known virtchnl speeds
		 */
		switch (link_speed) {
		case ICE_AQ_LINK_SPEED_10MB:
		case ICE_AQ_LINK_SPEED_100MB:
			speed = (u32)VIRTCHNL_LINK_SPEED_100MB;
			break;
		case ICE_AQ_LINK_SPEED_1000MB:
		case ICE_AQ_LINK_SPEED_2500MB:
		case ICE_AQ_LINK_SPEED_5GB:
			speed = (u32)VIRTCHNL_LINK_SPEED_1GB;
			break;
		case ICE_AQ_LINK_SPEED_10GB:
			speed = (u32)VIRTCHNL_LINK_SPEED_10GB;
			break;
		case ICE_AQ_LINK_SPEED_20GB:
			speed = (u32)VIRTCHNL_LINK_SPEED_20GB;
			break;
		case ICE_AQ_LINK_SPEED_25GB:
			speed = (u32)VIRTCHNL_LINK_SPEED_25GB;
			break;
		case ICE_AQ_LINK_SPEED_40GB:
		case ICE_AQ_LINK_SPEED_50GB:
		case ICE_AQ_LINK_SPEED_100GB:
			speed = (u32)VIRTCHNL_LINK_SPEED_40GB;
			break;
		default:
			speed = (u32)VIRTCHNL_LINK_SPEED_UNKNOWN;
			break;
		}

	return speed;
}
