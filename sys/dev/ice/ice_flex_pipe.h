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

#ifndef _ICE_FLEX_PIPE_H_
#define _ICE_FLEX_PIPE_H_

#include "ice_type.h"

int
ice_find_prot_off(struct ice_hw *hw, enum ice_block blk, u8 prof, u16 fv_idx,
		  u8 *prot, u16 *off);
int
ice_find_label_value(struct ice_seg *ice_seg, char const *name, u32 type,
		     u16 *value);
void
ice_get_sw_fv_bitmap(struct ice_hw *hw, enum ice_prof_type type,
		     ice_bitmap_t *bm);
void
ice_init_prof_result_bm(struct ice_hw *hw);
int
ice_aq_upload_section(struct ice_hw *hw, struct ice_buf_hdr *pkg_buf,
		      u16 buf_size, struct ice_sq_cd *cd);
bool
ice_get_open_tunnel_port(struct ice_hw *hw, enum ice_tunnel_type type,
			 u16 *port);
int
ice_create_tunnel(struct ice_hw *hw, enum ice_tunnel_type type, u16 port);
int ice_destroy_tunnel(struct ice_hw *hw, u16 port, bool all);
bool ice_tunnel_port_in_use(struct ice_hw *hw, u16 port, u16 *index);
bool
ice_tunnel_get_type(struct ice_hw *hw, u16 port, enum ice_tunnel_type *type);
int ice_replay_tunnels(struct ice_hw *hw);

/* XLT1/PType group functions */
int ice_ptg_update_xlt1(struct ice_hw *hw, enum ice_block blk);
void ice_ptg_free(struct ice_hw *hw, enum ice_block blk, u8 ptg);

/* XLT2/VSI group functions */
int ice_vsig_update_xlt2(struct ice_hw *hw, enum ice_block blk);
int
ice_vsig_find_vsi(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 *vsig);
int
ice_add_prof(struct ice_hw *hw, enum ice_block blk, u64 id,
	     ice_bitmap_t *ptypes, struct ice_fv_word *es);
struct ice_prof_map *
ice_search_prof_id(struct ice_hw *hw, enum ice_block blk, u64 id);
int
ice_add_vsi_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u16 vsig);
int
ice_add_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
int
ice_rem_prof_id_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi, u64 hdl);
int
ice_set_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 cntxt);
int
ice_get_prof_context(struct ice_hw *hw, enum ice_block blk, u64 id, u64 *cntxt);
int ice_init_hw_tbls(struct ice_hw *hw);
void ice_fill_blk_tbls(struct ice_hw *hw);
void ice_clear_hw_tbls(struct ice_hw *hw);
void ice_free_hw_tbls(struct ice_hw *hw);
int
ice_add_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id);
int
ice_rem_flow(struct ice_hw *hw, enum ice_block blk, u16 vsi[], u8 count,
	     u64 id);
int
ice_rem_prof(struct ice_hw *hw, enum ice_block blk, u64 id);

void ice_fill_blk_tbls(struct ice_hw *hw);

/* To support tunneling entries by PF, the package will append the PF number to
 * the label; for example TNL_VXLAN_PF0, TNL_VXLAN_PF1, TNL_VXLAN_PF2, etc.
 */
#define ICE_TNL_PRE	"TNL_"

void ice_add_tunnel_hint(struct ice_hw *hw, char *label_name, u16 val);

#endif /* _ICE_FLEX_PIPE_H_ */
