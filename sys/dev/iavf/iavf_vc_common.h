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

/**
 * @file iavf_vc_common.h
 * @brief header for the virtchnl interface
 *
 * Contains function declarations for the virtchnl PF to VF communication
 * interface.
 */
#ifndef _IAVF_VC_COMMON_H_
#define _IAVF_VC_COMMON_H_

#include "iavf_iflib.h"

int iavf_send_pf_msg(struct iavf_sc *sc,
	enum virtchnl_ops op, u8 *msg, u16 len);
int iavf_verify_api_ver(struct iavf_sc *);
int iavf_send_api_ver(struct iavf_sc *sc);
int iavf_enable_queues(struct iavf_sc *sc);
int iavf_disable_queues(struct iavf_sc *sc);
int iavf_add_vlans(struct iavf_sc *sc);
int iavf_send_vf_config_msg(struct iavf_sc *sc);
int iavf_get_vf_config(struct iavf_sc *sc);
int iavf_del_vlans(struct iavf_sc *sc);
int iavf_add_ether_filters(struct iavf_sc *sc);
int iavf_del_ether_filters(struct iavf_sc *sc);
int iavf_request_reset(struct iavf_sc *sc);
int iavf_request_stats(struct iavf_sc *sc);
void iavf_update_stats_counters(struct iavf_sc *sc, struct iavf_eth_stats *es);
int iavf_config_rss_key(struct iavf_sc *sc);
int iavf_set_rss_hena(struct iavf_sc *sc);
int iavf_config_rss_lut(struct iavf_sc *sc);
int iavf_config_promisc_mode(struct iavf_sc *sc);
void	*iavf_vc_get_op_chan(struct iavf_sc *sc, uint32_t request);
int	iavf_vc_send_cmd(struct iavf_sc *sc, uint32_t request);
const char * iavf_vc_stat_str(struct iavf_hw *hw,
    enum virtchnl_status_code stat_err);
const char *
    iavf_vc_speed_to_string(enum virtchnl_link_speed link_speed);
const char * iavf_vc_opcode_str(uint16_t op);
void
iavf_vc_completion(struct iavf_sc *sc,
    enum virtchnl_ops v_opcode,
    enum virtchnl_status_code v_retval, u8 *msg, u16 msglen);
enum iavf_ext_link_speed iavf_adv_speed_to_ext_speed(u32 adv_link_speed);
u32 iavf_ext_speed_to_ifmedia(enum iavf_ext_link_speed link_speed);
enum iavf_ext_link_speed iavf_vc_speed_to_ext_speed(enum virtchnl_link_speed link_speed);
const char * iavf_ext_speed_to_str(enum iavf_ext_link_speed link_speed);

int iavf_configure_queues(struct iavf_sc *sc);
int iavf_map_queues(struct iavf_sc *sc);

#endif /* _IAVF_VC_COMMON_H_ */
