/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2025, Intel Corporation
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

#ifndef _ICE_VF_MBX_H_
#define _ICE_VF_MBX_H_

#include "ice_type.h"
#include "ice_controlq.h"

/* Defining the mailbox message threshold as 63 asynchronous
 * pending messages. Normal VF functionality does not require
 * sending more than 63 asynchronous pending message.
 */

 /* Threshold value should be used to initialize
  * MBX_VF_IN_FLIGHT_MSGS_AT_PF_CNT register.
  */
#define ICE_ASYNC_VF_MSG_THRESHOLD	63

int
ice_aq_send_msg_to_pf(struct ice_hw *hw, enum virtchnl_ops v_opcode,
		      int v_retval, u8 *msg, u16 msglen,
		      struct ice_sq_cd *cd);
int
ice_aq_send_msg_to_vf(struct ice_hw *hw, u16 vfid, u32 v_opcode, u32 v_retval,
		      u8 *msg, u16 msglen, struct ice_sq_cd *cd);

u32 ice_conv_link_speed_to_virtchnl(bool adv_link_support, u16 link_speed);

void ice_e830_mbx_vf_dec_trig(struct ice_hw *hw,
			      struct ice_rq_event_info *event);
void ice_mbx_vf_clear_cnt_e830(struct ice_hw *hw, u16 vf_id);
int
ice_mbx_vf_state_handler(struct ice_hw *hw, struct ice_mbx_data *mbx_data,
			 struct ice_mbx_vf_info *vf_info, bool *report_malvf);
void ice_mbx_clear_malvf(struct ice_mbx_vf_info *vf_info);
void ice_mbx_init_vf_info(struct ice_hw *hw, struct ice_mbx_vf_info *vf_info);
void ice_mbx_init_snapshot(struct ice_hw *hw);
#endif /* _ICE_VF_MBX_H_ */
