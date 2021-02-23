/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
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

#include "ice_vlan_mode.h"
#include "ice_common.h"

/**
 * ice_set_svm - set single VLAN mode
 * @hw: pointer to the HW structure
 */
static enum ice_status ice_set_svm_dflt(struct ice_hw *hw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	return ice_aq_set_port_params(hw->port_info, 0, false, false, false, NULL);
}

/**
 * ice_init_vlan_mode_ops - initialize VLAN mode configuration ops
 * @hw: pointer to the HW structure
 */
void ice_init_vlan_mode_ops(struct ice_hw *hw)
{
	hw->vlan_mode_ops.set_dvm = NULL;
	hw->vlan_mode_ops.set_svm = ice_set_svm_dflt;
}

/**
 * ice_set_vlan_mode
 * @hw: pointer to the HW structure
 */
enum ice_status ice_set_vlan_mode(struct ice_hw *hw)
{
	enum ice_status status = ICE_ERR_NOT_IMPL;

	if (hw->vlan_mode_ops.set_dvm)
		status = hw->vlan_mode_ops.set_dvm(hw);

	if (status)
		return hw->vlan_mode_ops.set_svm(hw);

	return status;
}
