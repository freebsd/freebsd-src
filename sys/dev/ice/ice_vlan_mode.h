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

#ifndef _ICE_VLAN_MODE_H_
#define _ICE_VLAN_MODE_H_

struct ice_hw;

enum ice_status ice_set_vlan_mode(struct ice_hw *hw);
void ice_init_vlan_mode_ops(struct ice_hw *hw);

/* This structure defines the VLAN mode configuration interface. It is used to set the VLAN mode.
 *
 * Note: These operations will be called while the global configuration lock is held.
 *
 * enum ice_status (*set_svm)(struct ice_hw *hw);
 *	This function is called when the DDP and/or Firmware don't support double VLAN mode (DVM) or
 *	if the set_dvm op is not implemented and/or returns failure. It will set the device in
 *	single VLAN mode (SVM).
 *
 * enum ice_status (*set_dvm)(struct ice_hw *hw);
 *	This function is called when the DDP and Firmware support double VLAN mode (DVM). It should
 *	be implemented to set double VLAN mode. If it fails or remains unimplemented, set_svm will
 *	be called as a fallback plan.
 */
struct ice_vlan_mode_ops {
	enum ice_status (*set_svm)(struct ice_hw *hw);
	enum ice_status (*set_dvm)(struct ice_hw *hw);
};

#endif /* _ICE_VLAN_MODE_H */
