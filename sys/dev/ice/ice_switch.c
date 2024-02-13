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
#include "ice_switch.h"
#include "ice_flex_type.h"
#include "ice_flow.h"

#define ICE_ETH_DA_OFFSET		0
#define ICE_ETH_ETHTYPE_OFFSET		12
#define ICE_ETH_VLAN_TCI_OFFSET		14
#define ICE_MAX_VLAN_ID			0xFFF
#define ICE_IPV6_ETHER_ID		0x86DD
#define ICE_PPP_IPV6_PROTO_ID		0x0057
#define ICE_ETH_P_8021Q			0x8100

/* Dummy ethernet header needed in the ice_sw_rule_*
 * struct to configure any switch filter rules.
 * {DA (6 bytes), SA(6 bytes),
 * Ether type (2 bytes for header without VLAN tag) OR
 * VLAN tag (4 bytes for header with VLAN tag) }
 *
 * Word on Hardcoded values
 * byte 0 = 0x2: to identify it as locally administered DA MAC
 * byte 6 = 0x2: to identify it as locally administered SA MAC
 * byte 12 = 0x81 & byte 13 = 0x00:
 *	In case of VLAN filter first two bytes defines ether type (0x8100)
 *	and remaining two bytes are placeholder for programming a given VLAN ID
 *	In case of Ether type filter it is treated as header without VLAN tag
 *	and byte 12 and 13 is used to program a given Ether type instead
 */
static const u8 dummy_eth_header[DUMMY_ETH_HDR_LEN] = { 0x2, 0, 0, 0, 0, 0,
							0x2, 0, 0, 0, 0, 0,
							0x81, 0, 0, 0};

static bool
ice_vsi_uses_fltr(struct ice_fltr_mgmt_list_entry *fm_entry, u16 vsi_handle);

/**
 * ice_init_def_sw_recp - initialize the recipe book keeping tables
 * @hw: pointer to the HW struct
 * @recp_list: pointer to sw recipe list
 *
 * Allocate memory for the entire recipe table and initialize the structures/
 * entries corresponding to basic recipes.
 */
enum ice_status
ice_init_def_sw_recp(struct ice_hw *hw, struct ice_sw_recipe **recp_list)
{
	struct ice_sw_recipe *recps;
	u8 i;

	recps = (struct ice_sw_recipe *)
		ice_calloc(hw, ICE_MAX_NUM_RECIPES, sizeof(*recps));
	if (!recps)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		recps[i].root_rid = i;
		INIT_LIST_HEAD(&recps[i].filt_rules);
		INIT_LIST_HEAD(&recps[i].filt_replay_rules);
		INIT_LIST_HEAD(&recps[i].rg_list);
		ice_init_lock(&recps[i].filt_rule_lock);
	}

	*recp_list = recps;

	return ICE_SUCCESS;
}

/**
 * ice_aq_get_sw_cfg - get switch configuration
 * @hw: pointer to the hardware structure
 * @buf: pointer to the result buffer
 * @buf_size: length of the buffer available for response
 * @req_desc: pointer to requested descriptor
 * @num_elems: pointer to number of elements
 * @cd: pointer to command details structure or NULL
 *
 * Get switch configuration (0x0200) to be placed in buf.
 * This admin command returns information such as initial VSI/port number
 * and switch ID it belongs to.
 *
 * NOTE: *req_desc is both an input/output parameter.
 * The caller of this function first calls this function with *request_desc set
 * to 0. If the response from f/w has *req_desc set to 0, all the switch
 * configuration information has been returned; if non-zero (meaning not all
 * the information was returned), the caller should call this function again
 * with *req_desc set to the previous value returned by f/w to get the
 * next block of switch configuration information.
 *
 * *num_elems is output only parameter. This reflects the number of elements
 * in response buffer. The caller of this function to use *num_elems while
 * parsing the response buffer.
 */
static enum ice_status
ice_aq_get_sw_cfg(struct ice_hw *hw, struct ice_aqc_get_sw_cfg_resp_elem *buf,
		  u16 buf_size, u16 *req_desc, u16 *num_elems,
		  struct ice_sq_cd *cd)
{
	struct ice_aqc_get_sw_cfg *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_sw_cfg);
	cmd = &desc.params.get_sw_conf;
	cmd->element = CPU_TO_LE16(*req_desc);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status) {
		*req_desc = LE16_TO_CPU(cmd->element);
		*num_elems = LE16_TO_CPU(cmd->num_elems);
	}

	return status;
}

/**
 * ice_alloc_rss_global_lut - allocate a RSS global LUT
 * @hw: pointer to the HW struct
 * @shared_res: true to allocate as a shared resource and false to allocate as a dedicated resource
 * @global_lut_id: output parameter for the RSS global LUT's ID
 */
enum ice_status ice_alloc_rss_global_lut(struct ice_hw *hw, bool shared_res, u16 *global_lut_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	enum ice_status status;
	u16 buf_len;

	buf_len = ice_struct_size(sw_buf, elem, 1);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_GLOBAL_RSS_HASH |
				       (shared_res ? ICE_AQC_RES_TYPE_FLAG_SHARED :
				       ICE_AQC_RES_TYPE_FLAG_DEDICATED));

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len, ice_aqc_opc_alloc_res, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_RES, "Failed to allocate %s RSS global LUT, status %d\n",
			  shared_res ? "shared" : "dedicated", status);
		goto ice_alloc_global_lut_exit;
	}

	*global_lut_id = LE16_TO_CPU(sw_buf->elem[0].e.sw_resp);

ice_alloc_global_lut_exit:
	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_free_rss_global_lut - free a RSS global LUT
 * @hw: pointer to the HW struct
 * @global_lut_id: ID of the RSS global LUT to free
 */
enum ice_status ice_free_rss_global_lut(struct ice_hw *hw, u16 global_lut_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	u16 buf_len, num_elems = 1;
	enum ice_status status;

	buf_len = ice_struct_size(sw_buf, elem, num_elems);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	sw_buf->num_elems = CPU_TO_LE16(num_elems);
	sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_GLOBAL_RSS_HASH);
	sw_buf->elem[0].e.sw_resp = CPU_TO_LE16(global_lut_id);

	status = ice_aq_alloc_free_res(hw, num_elems, sw_buf, buf_len, ice_aqc_opc_free_res, NULL);
	if (status)
		ice_debug(hw, ICE_DBG_RES, "Failed to free RSS global LUT %d, status %d\n",
			  global_lut_id, status);

	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_alloc_sw - allocate resources specific to switch
 * @hw: pointer to the HW struct
 * @ena_stats: true to turn on VEB stats
 * @shared_res: true for shared resource, false for dedicated resource
 * @sw_id: switch ID returned
 * @counter_id: VEB counter ID returned
 *
 * allocates switch resources (SWID and VEB counter) (0x0208)
 */
enum ice_status
ice_alloc_sw(struct ice_hw *hw, bool ena_stats, bool shared_res, u16 *sw_id,
	     u16 *counter_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	struct ice_aqc_res_elem *sw_ele;
	enum ice_status status;
	u16 buf_len;

	buf_len = ice_struct_size(sw_buf, elem, 1);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer for switch ID.
	 * The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is allocated, and hence a single sw_id
	 * is requested.
	 */
	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type =
		CPU_TO_LE16(ICE_AQC_RES_TYPE_SWID |
			    (shared_res ? ICE_AQC_RES_TYPE_FLAG_SHARED :
			    ICE_AQC_RES_TYPE_FLAG_DEDICATED));

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);

	if (status)
		goto ice_alloc_sw_exit;

	sw_ele = &sw_buf->elem[0];
	*sw_id = LE16_TO_CPU(sw_ele->e.sw_resp);

	if (ena_stats) {
		/* Prepare buffer for VEB Counter */
		enum ice_adminq_opc opc = ice_aqc_opc_alloc_res;
		struct ice_aqc_alloc_free_res_elem *counter_buf;
		struct ice_aqc_res_elem *counter_ele;

		counter_buf = (struct ice_aqc_alloc_free_res_elem *)
				ice_malloc(hw, buf_len);
		if (!counter_buf) {
			status = ICE_ERR_NO_MEMORY;
			goto ice_alloc_sw_exit;
		}

		/* The number of resource entries in buffer is passed as 1 since
		 * only a single switch/VEB instance is allocated, and hence a
		 * single VEB counter is requested.
		 */
		counter_buf->num_elems = CPU_TO_LE16(1);
		counter_buf->res_type =
			CPU_TO_LE16(ICE_AQC_RES_TYPE_VEB_COUNTER |
				    ICE_AQC_RES_TYPE_FLAG_DEDICATED);
		status = ice_aq_alloc_free_res(hw, 1, counter_buf, buf_len,
					       opc, NULL);

		if (status) {
			ice_free(hw, counter_buf);
			goto ice_alloc_sw_exit;
		}
		counter_ele = &counter_buf->elem[0];
		*counter_id = LE16_TO_CPU(counter_ele->e.sw_resp);
		ice_free(hw, counter_buf);
	}

ice_alloc_sw_exit:
	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_free_sw - free resources specific to switch
 * @hw: pointer to the HW struct
 * @sw_id: switch ID returned
 * @counter_id: VEB counter ID returned
 *
 * free switch resources (SWID and VEB counter) (0x0209)
 *
 * NOTE: This function frees multiple resources. It continues
 * releasing other resources even after it encounters error.
 * The error code returned is the last error it encountered.
 */
enum ice_status ice_free_sw(struct ice_hw *hw, u16 sw_id, u16 counter_id)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf, *counter_buf;
	enum ice_status status, ret_status;
	u16 buf_len;

	buf_len = ice_struct_size(sw_buf, elem, 1);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	/* Prepare buffer to free for switch ID res.
	 * The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is freed, and hence a single sw_id
	 * is released.
	 */
	sw_buf->num_elems = CPU_TO_LE16(1);
	sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_SWID);
	sw_buf->elem[0].e.sw_resp = CPU_TO_LE16(sw_id);

	ret_status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
					   ice_aqc_opc_free_res, NULL);

	if (ret_status)
		ice_debug(hw, ICE_DBG_SW, "CQ CMD Buffer:\n");

	/* Prepare buffer to free for VEB Counter resource */
	counter_buf = (struct ice_aqc_alloc_free_res_elem *)
			ice_malloc(hw, buf_len);
	if (!counter_buf) {
		ice_free(hw, sw_buf);
		return ICE_ERR_NO_MEMORY;
	}

	/* The number of resource entries in buffer is passed as 1 since only a
	 * single switch/VEB instance is freed, and hence a single VEB counter
	 * is released
	 */
	counter_buf->num_elems = CPU_TO_LE16(1);
	counter_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_VEB_COUNTER);
	counter_buf->elem[0].e.sw_resp = CPU_TO_LE16(counter_id);

	status = ice_aq_alloc_free_res(hw, 1, counter_buf, buf_len,
				       ice_aqc_opc_free_res, NULL);
	if (status) {
		ice_debug(hw, ICE_DBG_SW, "VEB counter resource could not be freed\n");
		ret_status = status;
	}

	ice_free(hw, counter_buf);
	ice_free(hw, sw_buf);
	return ret_status;
}

/**
 * ice_aq_add_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Add a VSI context to the hardware (0x0210)
 */
enum ice_status
ice_aq_add_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *res;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	res = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_vsi);

	if (!vsi_ctx->alloc_from_pool)
		cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num |
					   ICE_AQ_VSI_IS_VALID);
	cmd->vf_id = vsi_ctx->vf_num;

	cmd->vsi_flags = CPU_TO_LE16(vsi_ctx->flags);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);

	if (!status) {
		vsi_ctx->vsi_num = LE16_TO_CPU(res->vsi_num) & ICE_AQ_VSI_NUM_M;
		vsi_ctx->vsis_allocd = LE16_TO_CPU(res->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(res->vsi_free);
	}

	return status;
}

/**
 * ice_aq_free_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @keep_vsi_alloc: keep VSI allocation as part of this PF's resources
 * @cd: pointer to command details structure or NULL
 *
 * Free VSI context info from hardware (0x0213)
 */
enum ice_status
ice_aq_free_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		bool keep_vsi_alloc, struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *resp;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_free_vsi);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);
	if (keep_vsi_alloc)
		cmd->cmd_flags = CPU_TO_LE16(ICE_AQ_VSI_KEEP_ALLOC);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
	if (!status) {
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_aq_update_vsi
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Update VSI context in the hardware (0x0211)
 */
enum ice_status
ice_aq_update_vsi(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		  struct ice_sq_cd *cd)
{
	struct ice_aqc_add_update_free_vsi_resp *resp;
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.add_update_free_vsi_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_update_vsi);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);

	if (!status) {
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_is_vsi_valid - check whether the VSI is valid or not
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * check whether the VSI is valid or not
 */
bool ice_is_vsi_valid(struct ice_hw *hw, u16 vsi_handle)
{
	return vsi_handle < ICE_MAX_VSI && hw->vsi_ctx[vsi_handle];
}

/**
 * ice_get_hw_vsi_num - return the HW VSI number
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * return the HW VSI number
 * Caution: call this function only if VSI is valid (ice_is_vsi_valid)
 */
u16 ice_get_hw_vsi_num(struct ice_hw *hw, u16 vsi_handle)
{
	return hw->vsi_ctx[vsi_handle]->vsi_num;
}

/**
 * ice_get_vsi_ctx - return the VSI context entry for a given VSI handle
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * return the VSI context entry for a given VSI handle
 */
struct ice_vsi_ctx *ice_get_vsi_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	return (vsi_handle >= ICE_MAX_VSI) ? NULL : hw->vsi_ctx[vsi_handle];
}

/**
 * ice_save_vsi_ctx - save the VSI context for a given VSI handle
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 * @vsi: VSI context pointer
 *
 * save the VSI context entry for a given VSI handle
 */
static void
ice_save_vsi_ctx(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi)
{
	hw->vsi_ctx[vsi_handle] = vsi;
}

/**
 * ice_clear_vsi_q_ctx - clear VSI queue contexts for all TCs
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 */
static void ice_clear_vsi_q_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_vsi_ctx *vsi;
	u8 i;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi)
		return;
	ice_for_each_traffic_class(i) {
		if (vsi->lan_q_ctx[i]) {
			ice_free(hw, vsi->lan_q_ctx[i]);
			vsi->lan_q_ctx[i] = NULL;
		}
		if (vsi->rdma_q_ctx[i]) {
			ice_free(hw, vsi->rdma_q_ctx[i]);
			vsi->rdma_q_ctx[i] = NULL;
		}
	}
}

/**
 * ice_clear_vsi_ctx - clear the VSI context entry
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 *
 * clear the VSI context entry
 */
static void ice_clear_vsi_ctx(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_vsi_ctx *vsi;

	vsi = ice_get_vsi_ctx(hw, vsi_handle);
	if (vsi) {
		ice_clear_vsi_q_ctx(hw, vsi_handle);
		ice_free(hw, vsi);
		hw->vsi_ctx[vsi_handle] = NULL;
	}
}

/**
 * ice_clear_all_vsi_ctx - clear all the VSI context entries
 * @hw: pointer to the HW struct
 */
void ice_clear_all_vsi_ctx(struct ice_hw *hw)
{
	u16 i;

	for (i = 0; i < ICE_MAX_VSI; i++)
		ice_clear_vsi_ctx(hw, i);
}

/**
 * ice_add_vsi - add VSI context to the hardware and VSI handle list
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle provided by drivers
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Add a VSI context to the hardware also add it into the VSI handle list.
 * If this function gets called after reset for existing VSIs then update
 * with the new HW VSI number in the corresponding VSI handle list entry.
 */
enum ice_status
ice_add_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	    struct ice_sq_cd *cd)
{
	struct ice_vsi_ctx *tmp_vsi_ctx;
	enum ice_status status;

	if (vsi_handle >= ICE_MAX_VSI)
		return ICE_ERR_PARAM;
	status = ice_aq_add_vsi(hw, vsi_ctx, cd);
	if (status)
		return status;
	tmp_vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!tmp_vsi_ctx) {
		/* Create a new VSI context */
		tmp_vsi_ctx = (struct ice_vsi_ctx *)
			ice_malloc(hw, sizeof(*tmp_vsi_ctx));
		if (!tmp_vsi_ctx) {
			ice_aq_free_vsi(hw, vsi_ctx, false, cd);
			return ICE_ERR_NO_MEMORY;
		}
		*tmp_vsi_ctx = *vsi_ctx;

		ice_save_vsi_ctx(hw, vsi_handle, tmp_vsi_ctx);
	} else {
		/* update with new HW VSI num */
		tmp_vsi_ctx->vsi_num = vsi_ctx->vsi_num;
	}

	return ICE_SUCCESS;
}

/**
 * ice_free_vsi- free VSI context from hardware and VSI handle list
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle
 * @vsi_ctx: pointer to a VSI context struct
 * @keep_vsi_alloc: keep VSI allocation as part of this PF's resources
 * @cd: pointer to command details structure or NULL
 *
 * Free VSI context info from hardware as well as from VSI handle list
 */
enum ice_status
ice_free_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	     bool keep_vsi_alloc, struct ice_sq_cd *cd)
{
	enum ice_status status;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx->vsi_num = ice_get_hw_vsi_num(hw, vsi_handle);
	status = ice_aq_free_vsi(hw, vsi_ctx, keep_vsi_alloc, cd);
	if (!status)
		ice_clear_vsi_ctx(hw, vsi_handle);
	return status;
}

/**
 * ice_update_vsi
 * @hw: pointer to the HW struct
 * @vsi_handle: unique VSI handle
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Update VSI context in the hardware
 */
enum ice_status
ice_update_vsi(struct ice_hw *hw, u16 vsi_handle, struct ice_vsi_ctx *vsi_ctx,
	       struct ice_sq_cd *cd)
{
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx->vsi_num = ice_get_hw_vsi_num(hw, vsi_handle);
	return ice_aq_update_vsi(hw, vsi_ctx, cd);
}

/**
 * ice_cfg_iwarp_fltr - enable/disable iWARP filtering on VSI
 * @hw: pointer to HW struct
 * @vsi_handle: VSI SW index
 * @enable: boolean for enable/disable
 */
enum ice_status
ice_cfg_iwarp_fltr(struct ice_hw *hw, u16 vsi_handle, bool enable)
{
	struct ice_vsi_ctx *ctx, *cached_ctx;
	enum ice_status status;

	cached_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!cached_ctx)
		return ICE_ERR_DOES_NOT_EXIST;

	ctx = (struct ice_vsi_ctx *)ice_calloc(hw, 1, sizeof(*ctx));
	if (!ctx)
		return ICE_ERR_NO_MEMORY;

	ctx->info.q_opt_rss = cached_ctx->info.q_opt_rss;
	ctx->info.q_opt_tc = cached_ctx->info.q_opt_tc;
	ctx->info.q_opt_flags = cached_ctx->info.q_opt_flags;

	ctx->info.valid_sections = CPU_TO_LE16(ICE_AQ_VSI_PROP_Q_OPT_VALID);

	if (enable)
		ctx->info.q_opt_flags |= ICE_AQ_VSI_Q_OPT_PE_FLTR_EN;
	else
		ctx->info.q_opt_flags &= ~ICE_AQ_VSI_Q_OPT_PE_FLTR_EN;

	status = ice_update_vsi(hw, vsi_handle, ctx, NULL);
	if (!status) {
		cached_ctx->info.q_opt_flags = ctx->info.q_opt_flags;
		cached_ctx->info.valid_sections |= ctx->info.valid_sections;
	}

	ice_free(hw, ctx);
	return status;
}

/**
 * ice_aq_get_vsi_params
 * @hw: pointer to the HW struct
 * @vsi_ctx: pointer to a VSI context struct
 * @cd: pointer to command details structure or NULL
 *
 * Get VSI context info from hardware (0x0212)
 */
enum ice_status
ice_aq_get_vsi_params(struct ice_hw *hw, struct ice_vsi_ctx *vsi_ctx,
		      struct ice_sq_cd *cd)
{
	struct ice_aqc_add_get_update_free_vsi *cmd;
	struct ice_aqc_get_vsi_resp *resp;
	struct ice_aq_desc desc;
	enum ice_status status;

	cmd = &desc.params.vsi_cmd;
	resp = &desc.params.get_vsi_resp;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_vsi_params);

	cmd->vsi_num = CPU_TO_LE16(vsi_ctx->vsi_num | ICE_AQ_VSI_IS_VALID);

	status = ice_aq_send_cmd(hw, &desc, &vsi_ctx->info,
				 sizeof(vsi_ctx->info), cd);
	if (!status) {
		vsi_ctx->vsi_num = LE16_TO_CPU(resp->vsi_num) &
					ICE_AQ_VSI_NUM_M;
		vsi_ctx->vf_num = resp->vf_id;
		vsi_ctx->vsis_allocd = LE16_TO_CPU(resp->vsi_used);
		vsi_ctx->vsis_unallocated = LE16_TO_CPU(resp->vsi_free);
	}

	return status;
}

/**
 * ice_aq_add_update_mir_rule - add/update a mirror rule
 * @hw: pointer to the HW struct
 * @rule_type: Rule Type
 * @dest_vsi: VSI number to which packets will be mirrored
 * @count: length of the list
 * @mr_buf: buffer for list of mirrored VSI numbers
 * @cd: pointer to command details structure or NULL
 * @rule_id: Rule ID
 *
 * Add/Update Mirror Rule (0x260).
 */
enum ice_status
ice_aq_add_update_mir_rule(struct ice_hw *hw, u16 rule_type, u16 dest_vsi,
			   u16 count, struct ice_mir_rule_buf *mr_buf,
			   struct ice_sq_cd *cd, u16 *rule_id)
{
	struct ice_aqc_add_update_mir_rule *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;
	__le16 *mr_list = NULL;
	u16 buf_size = 0;

	switch (rule_type) {
	case ICE_AQC_RULE_TYPE_VPORT_INGRESS:
	case ICE_AQC_RULE_TYPE_VPORT_EGRESS:
		/* Make sure count and mr_buf are set for these rule_types */
		if (!(count && mr_buf))
			return ICE_ERR_PARAM;

		buf_size = count * sizeof(__le16);
		mr_list = (_FORCE_ __le16 *)ice_malloc(hw, buf_size);
		if (!mr_list)
			return ICE_ERR_NO_MEMORY;
		break;
	case ICE_AQC_RULE_TYPE_PPORT_INGRESS:
	case ICE_AQC_RULE_TYPE_PPORT_EGRESS:
		/* Make sure count and mr_buf are not set for these
		 * rule_types
		 */
		if (count || mr_buf)
			return ICE_ERR_PARAM;
		break;
	default:
		ice_debug(hw, ICE_DBG_SW, "Error due to unsupported rule_type %u\n", rule_type);
		return ICE_ERR_OUT_OF_RANGE;
	}

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_add_update_mir_rule);

	/* Pre-process 'mr_buf' items for add/update of virtual port
	 * ingress/egress mirroring (but not physical port ingress/egress
	 * mirroring)
	 */
	if (mr_buf) {
		int i;

		for (i = 0; i < count; i++) {
			u16 id;

			id = mr_buf[i].vsi_idx & ICE_AQC_RULE_MIRRORED_VSI_M;

			/* Validate specified VSI number, make sure it is less
			 * than ICE_MAX_VSI, if not return with error.
			 */
			if (id >= ICE_MAX_VSI) {
				ice_debug(hw, ICE_DBG_SW, "Error VSI index (%u) out-of-range\n",
					  id);
				ice_free(hw, mr_list);
				return ICE_ERR_OUT_OF_RANGE;
			}

			/* add VSI to mirror rule */
			if (mr_buf[i].add)
				mr_list[i] =
					CPU_TO_LE16(id | ICE_AQC_RULE_ACT_M);
			else /* remove VSI from mirror rule */
				mr_list[i] = CPU_TO_LE16(id);
		}

		desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	}

	cmd = &desc.params.add_update_rule;
	if ((*rule_id) != ICE_INVAL_MIRROR_RULE_ID)
		cmd->rule_id = CPU_TO_LE16(((*rule_id) & ICE_AQC_RULE_ID_M) |
					   ICE_AQC_RULE_ID_VALID_M);
	cmd->rule_type = CPU_TO_LE16(rule_type & ICE_AQC_RULE_TYPE_M);
	cmd->num_entries = CPU_TO_LE16(count);
	cmd->dest = CPU_TO_LE16(dest_vsi);

	status = ice_aq_send_cmd(hw, &desc, mr_list, buf_size, cd);
	if (!status)
		*rule_id = LE16_TO_CPU(cmd->rule_id) & ICE_AQC_RULE_ID_M;

	ice_free(hw, mr_list);

	return status;
}

/**
 * ice_aq_delete_mir_rule - delete a mirror rule
 * @hw: pointer to the HW struct
 * @rule_id: Mirror rule ID (to be deleted)
 * @keep_allocd: if set, the VSI stays part of the PF allocated res,
 *		 otherwise it is returned to the shared pool
 * @cd: pointer to command details structure or NULL
 *
 * Delete Mirror Rule (0x261).
 */
enum ice_status
ice_aq_delete_mir_rule(struct ice_hw *hw, u16 rule_id, bool keep_allocd,
		       struct ice_sq_cd *cd)
{
	struct ice_aqc_delete_mir_rule *cmd;
	struct ice_aq_desc desc;

	/* rule_id should be in the range 0...63 */
	if (rule_id >= ICE_MAX_NUM_MIRROR_RULES)
		return ICE_ERR_OUT_OF_RANGE;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_del_mir_rule);

	cmd = &desc.params.del_rule;
	rule_id |= ICE_AQC_RULE_ID_VALID_M;
	cmd->rule_id = CPU_TO_LE16(rule_id);

	if (keep_allocd)
		cmd->flags = CPU_TO_LE16(ICE_AQC_FLAG_KEEP_ALLOCD_M);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, cd);
}

/**
 * ice_aq_alloc_free_vsi_list
 * @hw: pointer to the HW struct
 * @vsi_list_id: VSI list ID returned or used for lookup
 * @lkup_type: switch rule filter lookup type
 * @opc: switch rules population command type - pass in the command opcode
 *
 * allocates or free a VSI list resource
 */
static enum ice_status
ice_aq_alloc_free_vsi_list(struct ice_hw *hw, u16 *vsi_list_id,
			   enum ice_sw_lkup_type lkup_type,
			   enum ice_adminq_opc opc)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	struct ice_aqc_res_elem *vsi_ele;
	enum ice_status status;
	u16 buf_len;

	buf_len = ice_struct_size(sw_buf, elem, 1);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;
	sw_buf->num_elems = CPU_TO_LE16(1);

	if (lkup_type == ICE_SW_LKUP_MAC ||
	    lkup_type == ICE_SW_LKUP_MAC_VLAN ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
	    lkup_type == ICE_SW_LKUP_PROMISC ||
	    lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
	    lkup_type == ICE_SW_LKUP_DFLT ||
	    lkup_type == ICE_SW_LKUP_LAST) {
		sw_buf->res_type = CPU_TO_LE16(ICE_AQC_RES_TYPE_VSI_LIST_REP);
	} else if (lkup_type == ICE_SW_LKUP_VLAN) {
		sw_buf->res_type =
			CPU_TO_LE16(ICE_AQC_RES_TYPE_VSI_LIST_PRUNE);
	} else {
		status = ICE_ERR_PARAM;
		goto ice_aq_alloc_free_vsi_list_exit;
	}

	if (opc == ice_aqc_opc_free_res)
		sw_buf->elem[0].e.sw_resp = CPU_TO_LE16(*vsi_list_id);

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len, opc, NULL);
	if (status)
		goto ice_aq_alloc_free_vsi_list_exit;

	if (opc == ice_aqc_opc_alloc_res) {
		vsi_ele = &sw_buf->elem[0];
		*vsi_list_id = LE16_TO_CPU(vsi_ele->e.sw_resp);
	}

ice_aq_alloc_free_vsi_list_exit:
	ice_free(hw, sw_buf);
	return status;
}

/**
 * ice_aq_set_storm_ctrl - Sets storm control configuration
 * @hw: pointer to the HW struct
 * @bcast_thresh: represents the upper threshold for broadcast storm control
 * @mcast_thresh: represents the upper threshold for multicast storm control
 * @ctl_bitmask: storm control knobs
 *
 * Sets the storm control configuration (0x0280)
 */
enum ice_status
ice_aq_set_storm_ctrl(struct ice_hw *hw, u32 bcast_thresh, u32 mcast_thresh,
		      u32 ctl_bitmask)
{
	struct ice_aqc_storm_cfg *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.storm_conf;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_set_storm_cfg);

	cmd->bcast_thresh_size = CPU_TO_LE32(bcast_thresh & ICE_AQ_THRESHOLD_M);
	cmd->mcast_thresh_size = CPU_TO_LE32(mcast_thresh & ICE_AQ_THRESHOLD_M);
	cmd->storm_ctrl_ctrl = CPU_TO_LE32(ctl_bitmask);

	return ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
}

/**
 * ice_aq_get_storm_ctrl - gets storm control configuration
 * @hw: pointer to the HW struct
 * @bcast_thresh: represents the upper threshold for broadcast storm control
 * @mcast_thresh: represents the upper threshold for multicast storm control
 * @ctl_bitmask: storm control knobs
 *
 * Gets the storm control configuration (0x0281)
 */
enum ice_status
ice_aq_get_storm_ctrl(struct ice_hw *hw, u32 *bcast_thresh, u32 *mcast_thresh,
		      u32 *ctl_bitmask)
{
	enum ice_status status;
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_storm_cfg);

	status = ice_aq_send_cmd(hw, &desc, NULL, 0, NULL);
	if (!status) {
		struct ice_aqc_storm_cfg *resp = &desc.params.storm_conf;

		if (bcast_thresh)
			*bcast_thresh = LE32_TO_CPU(resp->bcast_thresh_size) &
				ICE_AQ_THRESHOLD_M;
		if (mcast_thresh)
			*mcast_thresh = LE32_TO_CPU(resp->mcast_thresh_size) &
				ICE_AQ_THRESHOLD_M;
		if (ctl_bitmask)
			*ctl_bitmask = LE32_TO_CPU(resp->storm_ctrl_ctrl);
	}

	return status;
}

/**
 * ice_aq_sw_rules - add/update/remove switch rules
 * @hw: pointer to the HW struct
 * @rule_list: pointer to switch rule population list
 * @rule_list_sz: total size of the rule list in bytes
 * @num_rules: number of switch rules in the rule_list
 * @opc: switch rules population command type - pass in the command opcode
 * @cd: pointer to command details structure or NULL
 *
 * Add(0x02a0)/Update(0x02a1)/Remove(0x02a2) switch rules commands to firmware
 */
enum ice_status
ice_aq_sw_rules(struct ice_hw *hw, void *rule_list, u16 rule_list_sz,
		u8 num_rules, enum ice_adminq_opc opc, struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (opc != ice_aqc_opc_add_sw_rules &&
	    opc != ice_aqc_opc_update_sw_rules &&
	    opc != ice_aqc_opc_remove_sw_rules)
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, opc);

	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	desc.params.sw_rules.num_rules_fltr_entry_index =
		CPU_TO_LE16(num_rules);
	status = ice_aq_send_cmd(hw, &desc, rule_list, rule_list_sz, cd);
	if (opc != ice_aqc_opc_add_sw_rules &&
	    hw->adminq.sq_last_status == ICE_AQ_RC_ENOENT)
		status = ICE_ERR_DOES_NOT_EXIST;

	return status;
}

/* ice_init_port_info - Initialize port_info with switch configuration data
 * @pi: pointer to port_info
 * @vsi_port_num: VSI number or port number
 * @type: Type of switch element (port or VSI)
 * @swid: switch ID of the switch the element is attached to
 * @pf_vf_num: PF or VF number
 * @is_vf: true if the element is a VF, false otherwise
 */
static void
ice_init_port_info(struct ice_port_info *pi, u16 vsi_port_num, u8 type,
		   u16 swid, u16 pf_vf_num, bool is_vf)
{
	switch (type) {
	case ICE_AQC_GET_SW_CONF_RESP_PHYS_PORT:
		pi->lport = (u8)(vsi_port_num & ICE_LPORT_MASK);
		pi->sw_id = swid;
		pi->pf_vf_num = pf_vf_num;
		pi->is_vf = is_vf;
		break;
	default:
		ice_debug(pi->hw, ICE_DBG_SW, "incorrect VSI/port type received\n");
		break;
	}
}

/* ice_get_initial_sw_cfg - Get initial port and default VSI data
 * @hw: pointer to the hardware structure
 */
enum ice_status ice_get_initial_sw_cfg(struct ice_hw *hw)
{
	struct ice_aqc_get_sw_cfg_resp_elem *rbuf;
	enum ice_status status;
	u8 num_total_ports;
	u16 req_desc = 0;
	u16 num_elems;
	u8 j = 0;
	u16 i;

	num_total_ports = 1;

	rbuf = (struct ice_aqc_get_sw_cfg_resp_elem *)
		ice_malloc(hw, ICE_SW_CFG_MAX_BUF_LEN);

	if (!rbuf)
		return ICE_ERR_NO_MEMORY;

	/* Multiple calls to ice_aq_get_sw_cfg may be required
	 * to get all the switch configuration information. The need
	 * for additional calls is indicated by ice_aq_get_sw_cfg
	 * writing a non-zero value in req_desc
	 */
	do {
		struct ice_aqc_get_sw_cfg_resp_elem *ele;

		status = ice_aq_get_sw_cfg(hw, rbuf, ICE_SW_CFG_MAX_BUF_LEN,
					   &req_desc, &num_elems, NULL);

		if (status)
			break;

		for (i = 0, ele = rbuf; i < num_elems; i++, ele++) {
			u16 pf_vf_num, swid, vsi_port_num;
			bool is_vf = false;
			u8 res_type;

			vsi_port_num = LE16_TO_CPU(ele->vsi_port_num) &
				ICE_AQC_GET_SW_CONF_RESP_VSI_PORT_NUM_M;

			pf_vf_num = LE16_TO_CPU(ele->pf_vf_num) &
				ICE_AQC_GET_SW_CONF_RESP_FUNC_NUM_M;

			swid = LE16_TO_CPU(ele->swid);

			if (LE16_TO_CPU(ele->pf_vf_num) &
			    ICE_AQC_GET_SW_CONF_RESP_IS_VF)
				is_vf = true;

			res_type = (u8)(LE16_TO_CPU(ele->vsi_port_num) >>
					ICE_AQC_GET_SW_CONF_RESP_TYPE_S);

			switch (res_type) {
			case ICE_AQC_GET_SW_CONF_RESP_PHYS_PORT:
			case ICE_AQC_GET_SW_CONF_RESP_VIRT_PORT:
				if (j == num_total_ports) {
					ice_debug(hw, ICE_DBG_SW, "more ports than expected\n");
					status = ICE_ERR_CFG;
					goto out;
				}
				ice_init_port_info(hw->port_info,
						   vsi_port_num, res_type, swid,
						   pf_vf_num, is_vf);
				j++;
				break;
			default:
				break;
			}
		}
	} while (req_desc && !status);

out:
	ice_free(hw, rbuf);
	return status;
}

/**
 * ice_fill_sw_info - Helper function to populate lb_en and lan_en
 * @hw: pointer to the hardware structure
 * @fi: filter info structure to fill/update
 *
 * This helper function populates the lb_en and lan_en elements of the provided
 * ice_fltr_info struct using the switch's type and characteristics of the
 * switch rule being configured.
 */
static void ice_fill_sw_info(struct ice_hw *hw, struct ice_fltr_info *fi)
{
	fi->lb_en = false;
	fi->lan_en = false;
	if ((fi->flag & ICE_FLTR_TX) &&
	    (fi->fltr_act == ICE_FWD_TO_VSI ||
	     fi->fltr_act == ICE_FWD_TO_VSI_LIST ||
	     fi->fltr_act == ICE_FWD_TO_Q ||
	     fi->fltr_act == ICE_FWD_TO_QGRP)) {
		/* Setting LB for prune actions will result in replicated
		 * packets to the internal switch that will be dropped.
		 */
		if (fi->lkup_type != ICE_SW_LKUP_VLAN)
			fi->lb_en = true;

		/* Set lan_en to TRUE if
		 * 1. The switch is a VEB AND
		 * 2
		 * 2.1 The lookup is a directional lookup like ethertype,
		 * promiscuous, ethertype-MAC, promiscuous-VLAN
		 * and default-port OR
		 * 2.2 The lookup is VLAN, OR
		 * 2.3 The lookup is MAC with mcast or bcast addr for MAC, OR
		 * 2.4 The lookup is MAC_VLAN with mcast or bcast addr for MAC.
		 *
		 * OR
		 *
		 * The switch is a VEPA.
		 *
		 * In all other cases, the LAN enable has to be set to false.
		 */
		if (hw->evb_veb) {
			if (fi->lkup_type == ICE_SW_LKUP_ETHERTYPE ||
			    fi->lkup_type == ICE_SW_LKUP_PROMISC ||
			    fi->lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
			    fi->lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
			    fi->lkup_type == ICE_SW_LKUP_DFLT ||
			    fi->lkup_type == ICE_SW_LKUP_VLAN ||
			    (fi->lkup_type == ICE_SW_LKUP_MAC &&
			     !IS_UNICAST_ETHER_ADDR(fi->l_data.mac.mac_addr)) ||
			    (fi->lkup_type == ICE_SW_LKUP_MAC_VLAN &&
			     !IS_UNICAST_ETHER_ADDR(fi->l_data.mac.mac_addr)))
				fi->lan_en = true;
		} else {
			fi->lan_en = true;
		}
	}
}

/**
 * ice_fill_sw_rule - Helper function to fill switch rule structure
 * @hw: pointer to the hardware structure
 * @f_info: entry containing packet forwarding information
 * @s_rule: switch rule structure to be filled in based on mac_entry
 * @opc: switch rules population command type - pass in the command opcode
 */
static void
ice_fill_sw_rule(struct ice_hw *hw, struct ice_fltr_info *f_info,
		 struct ice_sw_rule_lkup_rx_tx *s_rule,
		 enum ice_adminq_opc opc)
{
	u16 vlan_id = ICE_MAX_VLAN_ID + 1;
	u16 vlan_tpid = ICE_ETH_P_8021Q;
	void *daddr = NULL;
	u16 eth_hdr_sz;
	u8 *eth_hdr;
	u32 act = 0;
	__be16 *off;
	u8 q_rgn;

	if (opc == ice_aqc_opc_remove_sw_rules) {
		s_rule->act = 0;
		s_rule->index = CPU_TO_LE16(f_info->fltr_rule_id);
		s_rule->hdr_len = 0;
		return;
	}

	eth_hdr_sz = sizeof(dummy_eth_header);
	eth_hdr = s_rule->hdr_data;

	/* initialize the ether header with a dummy header */
	ice_memcpy(eth_hdr, dummy_eth_header, eth_hdr_sz, ICE_NONDMA_TO_NONDMA);
	ice_fill_sw_info(hw, f_info);

	switch (f_info->fltr_act) {
	case ICE_FWD_TO_VSI:
		act |= (f_info->fwd_id.hw_vsi_id << ICE_SINGLE_ACT_VSI_ID_S) &
			ICE_SINGLE_ACT_VSI_ID_M;
		if (f_info->lkup_type != ICE_SW_LKUP_VLAN)
			act |= ICE_SINGLE_ACT_VSI_FORWARDING |
				ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_VSI_LIST:
		act |= ICE_SINGLE_ACT_VSI_LIST;
		act |= (f_info->fwd_id.vsi_list_id <<
			ICE_SINGLE_ACT_VSI_LIST_ID_S) &
			ICE_SINGLE_ACT_VSI_LIST_ID_M;
		if (f_info->lkup_type != ICE_SW_LKUP_VLAN)
			act |= ICE_SINGLE_ACT_VSI_FORWARDING |
				ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_Q:
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (f_info->fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
			ICE_SINGLE_ACT_Q_INDEX_M;
		break;
	case ICE_DROP_PACKET:
		act |= ICE_SINGLE_ACT_VSI_FORWARDING | ICE_SINGLE_ACT_DROP |
			ICE_SINGLE_ACT_VALID_BIT;
		break;
	case ICE_FWD_TO_QGRP:
		q_rgn = f_info->qgrp_size > 0 ?
			(u8)ice_ilog2(f_info->qgrp_size) : 0;
		act |= ICE_SINGLE_ACT_TO_Q;
		act |= (f_info->fwd_id.q_id << ICE_SINGLE_ACT_Q_INDEX_S) &
			ICE_SINGLE_ACT_Q_INDEX_M;
		act |= (q_rgn << ICE_SINGLE_ACT_Q_REGION_S) &
			ICE_SINGLE_ACT_Q_REGION_M;
		break;
	default:
		return;
	}

	if (f_info->lb_en)
		act |= ICE_SINGLE_ACT_LB_ENABLE;
	if (f_info->lan_en)
		act |= ICE_SINGLE_ACT_LAN_ENABLE;

	switch (f_info->lkup_type) {
	case ICE_SW_LKUP_MAC:
		daddr = f_info->l_data.mac.mac_addr;
		break;
	case ICE_SW_LKUP_VLAN:
		vlan_id = f_info->l_data.vlan.vlan_id;
		if (f_info->l_data.vlan.tpid_valid)
			vlan_tpid = f_info->l_data.vlan.tpid;
		if (f_info->fltr_act == ICE_FWD_TO_VSI ||
		    f_info->fltr_act == ICE_FWD_TO_VSI_LIST) {
			act |= ICE_SINGLE_ACT_PRUNE;
			act |= ICE_SINGLE_ACT_EGRESS | ICE_SINGLE_ACT_INGRESS;
		}
		break;
	case ICE_SW_LKUP_ETHERTYPE_MAC:
		daddr = f_info->l_data.ethertype_mac.mac_addr;
		/* fall-through */
	case ICE_SW_LKUP_ETHERTYPE:
		off = (_FORCE_ __be16 *)(eth_hdr + ICE_ETH_ETHTYPE_OFFSET);
		*off = CPU_TO_BE16(f_info->l_data.ethertype_mac.ethertype);
		break;
	case ICE_SW_LKUP_MAC_VLAN:
		daddr = f_info->l_data.mac_vlan.mac_addr;
		vlan_id = f_info->l_data.mac_vlan.vlan_id;
		break;
	case ICE_SW_LKUP_PROMISC_VLAN:
		vlan_id = f_info->l_data.mac_vlan.vlan_id;
		/* fall-through */
	case ICE_SW_LKUP_PROMISC:
		daddr = f_info->l_data.mac_vlan.mac_addr;
		break;
	default:
		break;
	}

	s_rule->hdr.type = (f_info->flag & ICE_FLTR_RX) ?
		CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_RX) :
		CPU_TO_LE16(ICE_AQC_SW_RULES_T_LKUP_TX);

	/* Recipe set depending on lookup type */
	s_rule->recipe_id = CPU_TO_LE16(f_info->lkup_type);
	s_rule->src = CPU_TO_LE16(f_info->src);
	s_rule->act = CPU_TO_LE32(act);

	if (daddr)
		ice_memcpy(eth_hdr + ICE_ETH_DA_OFFSET, daddr, ETH_ALEN,
			   ICE_NONDMA_TO_NONDMA);

	if (!(vlan_id > ICE_MAX_VLAN_ID)) {
		off = (_FORCE_ __be16 *)(eth_hdr + ICE_ETH_VLAN_TCI_OFFSET);
		*off = CPU_TO_BE16(vlan_id);
		off = (_FORCE_ __be16 *)(eth_hdr + ICE_ETH_ETHTYPE_OFFSET);
		*off = CPU_TO_BE16(vlan_tpid);
	}

	/* Create the switch rule with the final dummy Ethernet header */
	if (opc != ice_aqc_opc_update_sw_rules)
		s_rule->hdr_len = CPU_TO_LE16(eth_hdr_sz);
}

/**
 * ice_add_marker_act
 * @hw: pointer to the hardware structure
 * @m_ent: the management entry for which sw marker needs to be added
 * @sw_marker: sw marker to tag the Rx descriptor with
 * @l_id: large action resource ID
 *
 * Create a large action to hold software marker and update the switch rule
 * entry pointed by m_ent with newly created large action
 */
static enum ice_status
ice_add_marker_act(struct ice_hw *hw, struct ice_fltr_mgmt_list_entry *m_ent,
		   u16 sw_marker, u16 l_id)
{
	struct ice_sw_rule_lkup_rx_tx *rx_tx;
	struct ice_sw_rule_lg_act *lg_act;
	/* For software marker we need 3 large actions
	 * 1. FWD action: FWD TO VSI or VSI LIST
	 * 2. GENERIC VALUE action to hold the profile ID
	 * 3. GENERIC VALUE action to hold the software marker ID
	 */
	const u16 num_lg_acts = 3;
	enum ice_status status;
	u16 lg_act_size;
	u16 rules_size;
	u32 act;
	u16 id;

	if (m_ent->fltr_info.lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	/* Create two back-to-back switch rules and submit them to the HW using
	 * one memory buffer:
	 *    1. Large Action
	 *    2. Look up Tx Rx
	 */
	lg_act_size = (u16)ice_struct_size(lg_act, act, num_lg_acts);
	rules_size = lg_act_size +
		     ice_struct_size(rx_tx, hdr_data, DUMMY_ETH_HDR_LEN);
	lg_act = (struct ice_sw_rule_lg_act *)ice_malloc(hw, rules_size);
	if (!lg_act)
		return ICE_ERR_NO_MEMORY;

	rx_tx = (struct ice_sw_rule_lkup_rx_tx *)((u8 *)lg_act + lg_act_size);

	/* Fill in the first switch rule i.e. large action */
	lg_act->hdr.type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LG_ACT);
	lg_act->index = CPU_TO_LE16(l_id);
	lg_act->size = CPU_TO_LE16(num_lg_acts);

	/* First action VSI forwarding or VSI list forwarding depending on how
	 * many VSIs
	 */
	id = (m_ent->vsi_count > 1) ? m_ent->fltr_info.fwd_id.vsi_list_id :
		m_ent->fltr_info.fwd_id.hw_vsi_id;

	act = ICE_LG_ACT_VSI_FORWARDING | ICE_LG_ACT_VALID_BIT;
	act |= (id << ICE_LG_ACT_VSI_LIST_ID_S) & ICE_LG_ACT_VSI_LIST_ID_M;
	if (m_ent->vsi_count > 1)
		act |= ICE_LG_ACT_VSI_LIST;
	lg_act->act[0] = CPU_TO_LE32(act);

	/* Second action descriptor type */
	act = ICE_LG_ACT_GENERIC;

	act |= (1 << ICE_LG_ACT_GENERIC_VALUE_S) & ICE_LG_ACT_GENERIC_VALUE_M;
	lg_act->act[1] = CPU_TO_LE32(act);

	act = (ICE_LG_ACT_GENERIC_OFF_RX_DESC_PROF_IDX <<
	       ICE_LG_ACT_GENERIC_OFFSET_S) & ICE_LG_ACT_GENERIC_OFFSET_M;

	/* Third action Marker value */
	act |= ICE_LG_ACT_GENERIC;
	act |= (sw_marker << ICE_LG_ACT_GENERIC_VALUE_S) &
		ICE_LG_ACT_GENERIC_VALUE_M;

	lg_act->act[2] = CPU_TO_LE32(act);

	/* call the fill switch rule to fill the lookup Tx Rx structure */
	ice_fill_sw_rule(hw, &m_ent->fltr_info, rx_tx,
			 ice_aqc_opc_update_sw_rules);

	/* Update the action to point to the large action ID */
	rx_tx->act = CPU_TO_LE32(ICE_SINGLE_ACT_PTR |
				 ((l_id << ICE_SINGLE_ACT_PTR_VAL_S) &
				  ICE_SINGLE_ACT_PTR_VAL_M));

	/* Use the filter rule ID of the previously created rule with single
	 * act. Once the update happens, hardware will treat this as large
	 * action
	 */
	rx_tx->index = CPU_TO_LE16(m_ent->fltr_info.fltr_rule_id);

	status = ice_aq_sw_rules(hw, lg_act, rules_size, 2,
				 ice_aqc_opc_update_sw_rules, NULL);
	if (!status) {
		m_ent->lg_act_idx = l_id;
		m_ent->sw_marker_id = sw_marker;
	}

	ice_free(hw, lg_act);
	return status;
}

/**
 * ice_add_counter_act - add/update filter rule with counter action
 * @hw: pointer to the hardware structure
 * @m_ent: the management entry for which counter needs to be added
 * @counter_id: VLAN counter ID returned as part of allocate resource
 * @l_id: large action resource ID
 */
static enum ice_status
ice_add_counter_act(struct ice_hw *hw, struct ice_fltr_mgmt_list_entry *m_ent,
		    u16 counter_id, u16 l_id)
{
	struct ice_sw_rule_lkup_rx_tx *rx_tx;
	struct ice_sw_rule_lg_act *lg_act;
	enum ice_status status;
	/* 2 actions will be added while adding a large action counter */
	const int num_acts = 2;
	u16 lg_act_size;
	u16 rules_size;
	u16 f_rule_id;
	u32 act;
	u16 id;

	if (m_ent->fltr_info.lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	/* Create two back-to-back switch rules and submit them to the HW using
	 * one memory buffer:
	 * 1. Large Action
	 * 2. Look up Tx Rx
	 */
	lg_act_size = (u16)ice_struct_size(lg_act, act, num_acts);
	rules_size = lg_act_size +
		     ice_struct_size(rx_tx, hdr_data, DUMMY_ETH_HDR_LEN);
	lg_act = (struct ice_sw_rule_lg_act *)ice_malloc(hw, rules_size);
	if (!lg_act)
		return ICE_ERR_NO_MEMORY;

	rx_tx = (struct ice_sw_rule_lkup_rx_tx *)((u8 *)lg_act +
						      lg_act_size);

	/* Fill in the first switch rule i.e. large action */
	lg_act->hdr.type = CPU_TO_LE16(ICE_AQC_SW_RULES_T_LG_ACT);
	lg_act->index = CPU_TO_LE16(l_id);
	lg_act->size = CPU_TO_LE16(num_acts);

	/* First action VSI forwarding or VSI list forwarding depending on how
	 * many VSIs
	 */
	id = (m_ent->vsi_count > 1) ?  m_ent->fltr_info.fwd_id.vsi_list_id :
		m_ent->fltr_info.fwd_id.hw_vsi_id;

	act = ICE_LG_ACT_VSI_FORWARDING | ICE_LG_ACT_VALID_BIT;
	act |= (id << ICE_LG_ACT_VSI_LIST_ID_S) &
		ICE_LG_ACT_VSI_LIST_ID_M;
	if (m_ent->vsi_count > 1)
		act |= ICE_LG_ACT_VSI_LIST;
	lg_act->act[0] = CPU_TO_LE32(act);

	/* Second action counter ID */
	act = ICE_LG_ACT_STAT_COUNT;
	act |= (counter_id << ICE_LG_ACT_STAT_COUNT_S) &
		ICE_LG_ACT_STAT_COUNT_M;
	lg_act->act[1] = CPU_TO_LE32(act);

	/* call the fill switch rule to fill the lookup Tx Rx structure */
	ice_fill_sw_rule(hw, &m_ent->fltr_info, rx_tx,
			 ice_aqc_opc_update_sw_rules);

	act = ICE_SINGLE_ACT_PTR;
	act |= (l_id << ICE_SINGLE_ACT_PTR_VAL_S) & ICE_SINGLE_ACT_PTR_VAL_M;
	rx_tx->act = CPU_TO_LE32(act);

	/* Use the filter rule ID of the previously created rule with single
	 * act. Once the update happens, hardware will treat this as large
	 * action
	 */
	f_rule_id = m_ent->fltr_info.fltr_rule_id;
	rx_tx->index = CPU_TO_LE16(f_rule_id);

	status = ice_aq_sw_rules(hw, lg_act, rules_size, 2,
				 ice_aqc_opc_update_sw_rules, NULL);
	if (!status) {
		m_ent->lg_act_idx = l_id;
		m_ent->counter_index = (u8)counter_id;
	}

	ice_free(hw, lg_act);
	return status;
}

/**
 * ice_create_vsi_list_map
 * @hw: pointer to the hardware structure
 * @vsi_handle_arr: array of VSI handles to set in the VSI mapping
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 *
 * Helper function to create a new entry of VSI list ID to VSI mapping
 * using the given VSI list ID
 */
static struct ice_vsi_list_map_info *
ice_create_vsi_list_map(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			u16 vsi_list_id)
{
	struct ice_switch_info *sw = hw->switch_info;
	struct ice_vsi_list_map_info *v_map;
	int i;

	v_map = (struct ice_vsi_list_map_info *)ice_malloc(hw, sizeof(*v_map));
	if (!v_map)
		return NULL;

	v_map->vsi_list_id = vsi_list_id;
	v_map->ref_cnt = 1;
	for (i = 0; i < num_vsi; i++)
		ice_set_bit(vsi_handle_arr[i], v_map->vsi_map);

	LIST_ADD(&v_map->list_entry, &sw->vsi_list_map_head);
	return v_map;
}

/**
 * ice_update_vsi_list_rule
 * @hw: pointer to the hardware structure
 * @vsi_handle_arr: array of VSI handles to form a VSI list
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 * @remove: Boolean value to indicate if this is a remove action
 * @opc: switch rules population command type - pass in the command opcode
 * @lkup_type: lookup type of the filter
 *
 * Call AQ command to add a new switch rule or update existing switch rule
 * using the given VSI list ID
 */
static enum ice_status
ice_update_vsi_list_rule(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			 u16 vsi_list_id, bool remove, enum ice_adminq_opc opc,
			 enum ice_sw_lkup_type lkup_type)
{
	struct ice_sw_rule_vsi_list *s_rule;
	enum ice_status status;
	u16 s_rule_size;
	u16 rule_type;
	int i;

	if (!num_vsi)
		return ICE_ERR_PARAM;

	if (lkup_type == ICE_SW_LKUP_MAC ||
	    lkup_type == ICE_SW_LKUP_MAC_VLAN ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE ||
	    lkup_type == ICE_SW_LKUP_ETHERTYPE_MAC ||
	    lkup_type == ICE_SW_LKUP_PROMISC ||
	    lkup_type == ICE_SW_LKUP_PROMISC_VLAN ||
	    lkup_type == ICE_SW_LKUP_DFLT ||
	    lkup_type == ICE_SW_LKUP_LAST)
		rule_type = remove ? ICE_AQC_SW_RULES_T_VSI_LIST_CLEAR :
			ICE_AQC_SW_RULES_T_VSI_LIST_SET;
	else if (lkup_type == ICE_SW_LKUP_VLAN)
		rule_type = remove ? ICE_AQC_SW_RULES_T_PRUNE_LIST_CLEAR :
			ICE_AQC_SW_RULES_T_PRUNE_LIST_SET;
	else
		return ICE_ERR_PARAM;

	s_rule_size = (u16)ice_struct_size(s_rule, vsi, num_vsi);
	s_rule = (struct ice_sw_rule_vsi_list *)ice_malloc(hw, s_rule_size);
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;
	for (i = 0; i < num_vsi; i++) {
		if (!ice_is_vsi_valid(hw, vsi_handle_arr[i])) {
			status = ICE_ERR_PARAM;
			goto exit;
		}
		/* AQ call requires hw_vsi_id(s) */
		s_rule->vsi[i] =
			CPU_TO_LE16(ice_get_hw_vsi_num(hw, vsi_handle_arr[i]));
	}

	s_rule->hdr.type = CPU_TO_LE16(rule_type);
	s_rule->number_vsi = CPU_TO_LE16(num_vsi);
	s_rule->index = CPU_TO_LE16(vsi_list_id);

	status = ice_aq_sw_rules(hw, s_rule, s_rule_size, 1, opc, NULL);

exit:
	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_create_vsi_list_rule - Creates and populates a VSI list rule
 * @hw: pointer to the HW struct
 * @vsi_handle_arr: array of VSI handles to form a VSI list
 * @num_vsi: number of VSI handles in the array
 * @vsi_list_id: stores the ID of the VSI list to be created
 * @lkup_type: switch rule filter's lookup type
 */
static enum ice_status
ice_create_vsi_list_rule(struct ice_hw *hw, u16 *vsi_handle_arr, u16 num_vsi,
			 u16 *vsi_list_id, enum ice_sw_lkup_type lkup_type)
{
	enum ice_status status;

	status = ice_aq_alloc_free_vsi_list(hw, vsi_list_id, lkup_type,
					    ice_aqc_opc_alloc_res);
	if (status)
		return status;

	/* Update the newly created VSI list to include the specified VSIs */
	return ice_update_vsi_list_rule(hw, vsi_handle_arr, num_vsi,
					*vsi_list_id, false,
					ice_aqc_opc_add_sw_rules, lkup_type);
}

/**
 * ice_create_pkt_fwd_rule
 * @hw: pointer to the hardware structure
 * @recp_list: corresponding filter management list
 * @f_entry: entry containing packet forwarding information
 *
 * Create switch rule with given filter information and add an entry
 * to the corresponding filter management list to track this switch rule
 * and VSI mapping
 */
static enum ice_status
ice_create_pkt_fwd_rule(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
			struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct ice_sw_rule_lkup_rx_tx *s_rule;
	enum ice_status status;

	s_rule = (struct ice_sw_rule_lkup_rx_tx *)
		ice_malloc(hw, ice_struct_size(s_rule, hdr_data,
					       DUMMY_ETH_HDR_LEN));
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;
	fm_entry = (struct ice_fltr_mgmt_list_entry *)
		   ice_malloc(hw, sizeof(*fm_entry));
	if (!fm_entry) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_create_pkt_fwd_rule_exit;
	}

	fm_entry->fltr_info = f_entry->fltr_info;

	/* Initialize all the fields for the management entry */
	fm_entry->vsi_count = 1;
	fm_entry->lg_act_idx = ICE_INVAL_LG_ACT_INDEX;
	fm_entry->sw_marker_id = ICE_INVAL_SW_MARKER_ID;
	fm_entry->counter_index = ICE_INVAL_COUNTER_ID;

	ice_fill_sw_rule(hw, &fm_entry->fltr_info, s_rule,
			 ice_aqc_opc_add_sw_rules);

	status = ice_aq_sw_rules(hw, s_rule,
				 ice_struct_size(s_rule, hdr_data,
						 DUMMY_ETH_HDR_LEN),
				 1, ice_aqc_opc_add_sw_rules, NULL);
	if (status) {
		ice_free(hw, fm_entry);
		goto ice_create_pkt_fwd_rule_exit;
	}

	f_entry->fltr_info.fltr_rule_id = LE16_TO_CPU(s_rule->index);
	fm_entry->fltr_info.fltr_rule_id = LE16_TO_CPU(s_rule->index);

	/* The book keeping entries will get removed when base driver
	 * calls remove filter AQ command
	 */
	LIST_ADD(&fm_entry->list_entry, &recp_list->filt_rules);

ice_create_pkt_fwd_rule_exit:
	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_update_pkt_fwd_rule
 * @hw: pointer to the hardware structure
 * @f_info: filter information for switch rule
 *
 * Call AQ command to update a previously created switch rule with a
 * VSI list ID
 */
static enum ice_status
ice_update_pkt_fwd_rule(struct ice_hw *hw, struct ice_fltr_info *f_info)
{
	struct ice_sw_rule_lkup_rx_tx *s_rule;
	enum ice_status status;

	s_rule = (struct ice_sw_rule_lkup_rx_tx *)
		ice_malloc(hw, ice_struct_size(s_rule, hdr_data,
					       DUMMY_ETH_HDR_LEN));
	if (!s_rule)
		return ICE_ERR_NO_MEMORY;

	ice_fill_sw_rule(hw, f_info, s_rule, ice_aqc_opc_update_sw_rules);

	s_rule->index = CPU_TO_LE16(f_info->fltr_rule_id);

	/* Update switch rule with new rule set to forward VSI list */
	status = ice_aq_sw_rules(hw, s_rule,
				 ice_struct_size(s_rule, hdr_data,
						 DUMMY_ETH_HDR_LEN),
				 1, ice_aqc_opc_update_sw_rules, NULL);

	ice_free(hw, s_rule);
	return status;
}

/**
 * ice_update_sw_rule_bridge_mode
 * @hw: pointer to the HW struct
 *
 * Updates unicast switch filter rules based on VEB/VEPA mode
 */
enum ice_status ice_update_sw_rule_bridge_mode(struct ice_hw *hw)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	enum ice_status status = ICE_SUCCESS;
	struct ice_switch_info *sw = NULL;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	sw = hw->switch_info;

	rule_lock = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rule_lock;
	rule_head = &sw->recp_list[ICE_SW_LKUP_MAC].filt_rules;

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(fm_entry, rule_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_info *fi = &fm_entry->fltr_info;
		u8 *addr = fi->l_data.mac.mac_addr;

		/* Update unicast Tx rules to reflect the selected
		 * VEB/VEPA mode
		 */
		if ((fi->flag & ICE_FLTR_TX) && IS_UNICAST_ETHER_ADDR(addr) &&
		    (fi->fltr_act == ICE_FWD_TO_VSI ||
		     fi->fltr_act == ICE_FWD_TO_VSI_LIST ||
		     fi->fltr_act == ICE_FWD_TO_Q ||
		     fi->fltr_act == ICE_FWD_TO_QGRP)) {
			status = ice_update_pkt_fwd_rule(hw, fi);
			if (status)
				break;
		}
	}

	ice_release_lock(rule_lock);

	return status;
}

/**
 * ice_add_update_vsi_list
 * @hw: pointer to the hardware structure
 * @m_entry: pointer to current filter management list entry
 * @cur_fltr: filter information from the book keeping entry
 * @new_fltr: filter information with the new VSI to be added
 *
 * Call AQ command to add or update previously created VSI list with new VSI.
 *
 * Helper function to do book keeping associated with adding filter information
 * The algorithm to do the book keeping is described below :
 * When a VSI needs to subscribe to a given filter (MAC/VLAN/Ethtype etc.)
 *	if only one VSI has been added till now
 *		Allocate a new VSI list and add two VSIs
 *		to this list using switch rule command
 *		Update the previously created switch rule with the
 *		newly created VSI list ID
 *	if a VSI list was previously created
 *		Add the new VSI to the previously created VSI list set
 *		using the update switch rule command
 */
static enum ice_status
ice_add_update_vsi_list(struct ice_hw *hw,
			struct ice_fltr_mgmt_list_entry *m_entry,
			struct ice_fltr_info *cur_fltr,
			struct ice_fltr_info *new_fltr)
{
	enum ice_status status = ICE_SUCCESS;
	u16 vsi_list_id = 0;
	if ((cur_fltr->fltr_act == ICE_FWD_TO_Q ||
	     cur_fltr->fltr_act == ICE_FWD_TO_QGRP))
		return ICE_ERR_NOT_IMPL;

	if ((new_fltr->fltr_act == ICE_FWD_TO_Q ||
	     new_fltr->fltr_act == ICE_FWD_TO_QGRP) &&
	    (cur_fltr->fltr_act == ICE_FWD_TO_VSI ||
	     cur_fltr->fltr_act == ICE_FWD_TO_VSI_LIST))
		return ICE_ERR_NOT_IMPL;

	if (m_entry->vsi_count < 2 && !m_entry->vsi_list_info) {
		/* Only one entry existed in the mapping and it was not already
		 * a part of a VSI list. So, create a VSI list with the old and
		 * new VSIs.
		 */
		struct ice_fltr_info tmp_fltr;
		u16 vsi_handle_arr[2];

		/* A rule already exists with the new VSI being added */
		if (cur_fltr->fwd_id.hw_vsi_id == new_fltr->fwd_id.hw_vsi_id)
			return ICE_ERR_ALREADY_EXISTS;

		vsi_handle_arr[0] = cur_fltr->vsi_handle;
		vsi_handle_arr[1] = new_fltr->vsi_handle;
		status = ice_create_vsi_list_rule(hw, &vsi_handle_arr[0], 2,
						  &vsi_list_id,
						  new_fltr->lkup_type);
		if (status)
			return status;

		tmp_fltr = *new_fltr;
		tmp_fltr.fltr_rule_id = cur_fltr->fltr_rule_id;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI_LIST;
		tmp_fltr.fwd_id.vsi_list_id = vsi_list_id;
		/* Update the previous switch rule of "MAC forward to VSI" to
		 * "MAC fwd to VSI list"
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status)
			return status;

		cur_fltr->fwd_id.vsi_list_id = vsi_list_id;
		cur_fltr->fltr_act = ICE_FWD_TO_VSI_LIST;
		m_entry->vsi_list_info =
			ice_create_vsi_list_map(hw, &vsi_handle_arr[0], 2,
						vsi_list_id);

		if (!m_entry->vsi_list_info)
			return ICE_ERR_NO_MEMORY;

		/* If this entry was large action then the large action needs
		 * to be updated to point to FWD to VSI list
		 */
		if (m_entry->sw_marker_id != ICE_INVAL_SW_MARKER_ID)
			status =
			    ice_add_marker_act(hw, m_entry,
					       m_entry->sw_marker_id,
					       m_entry->lg_act_idx);
	} else {
		u16 vsi_handle = new_fltr->vsi_handle;
		enum ice_adminq_opc opcode;

		if (!m_entry->vsi_list_info)
			return ICE_ERR_CFG;

		/* A rule already exists with the new VSI being added */
		if (ice_is_bit_set(m_entry->vsi_list_info->vsi_map, vsi_handle))
			return ICE_SUCCESS;

		/* Update the previously created VSI list set with
		 * the new VSI ID passed in
		 */
		vsi_list_id = cur_fltr->fwd_id.vsi_list_id;
		opcode = ice_aqc_opc_update_sw_rules;

		status = ice_update_vsi_list_rule(hw, &vsi_handle, 1,
						  vsi_list_id, false, opcode,
						  new_fltr->lkup_type);
		/* update VSI list mapping info with new VSI ID */
		if (!status)
			ice_set_bit(vsi_handle,
				    m_entry->vsi_list_info->vsi_map);
	}
	if (!status)
		m_entry->vsi_count++;
	return status;
}

/**
 * ice_find_rule_entry - Search a rule entry
 * @list_head: head of rule list
 * @f_info: rule information
 *
 * Helper function to search for a given rule entry
 * Returns pointer to entry storing the rule if found
 */
static struct ice_fltr_mgmt_list_entry *
ice_find_rule_entry(struct LIST_HEAD_TYPE *list_head,
		    struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *list_itr, *ret = NULL;

	LIST_FOR_EACH_ENTRY(list_itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		if (!memcmp(&f_info->l_data, &list_itr->fltr_info.l_data,
			    sizeof(f_info->l_data)) &&
		    f_info->flag == list_itr->fltr_info.flag) {
			ret = list_itr;
			break;
		}
	}
	return ret;
}

/**
 * ice_find_vsi_list_entry - Search VSI list map with VSI count 1
 * @recp_list: VSI lists needs to be searched
 * @vsi_handle: VSI handle to be found in VSI list
 * @vsi_list_id: VSI list ID found containing vsi_handle
 *
 * Helper function to search a VSI list with single entry containing given VSI
 * handle element. This can be extended further to search VSI list with more
 * than 1 vsi_count. Returns pointer to VSI list entry if found.
 */
struct ice_vsi_list_map_info *
ice_find_vsi_list_entry(struct ice_sw_recipe *recp_list, u16 vsi_handle,
			u16 *vsi_list_id)
{
	struct ice_vsi_list_map_info *map_info = NULL;
	struct LIST_HEAD_TYPE *list_head;

	list_head = &recp_list->filt_rules;
	if (recp_list->adv_rule) {
		struct ice_adv_fltr_mgmt_list_entry *list_itr;

		LIST_FOR_EACH_ENTRY(list_itr, list_head,
				    ice_adv_fltr_mgmt_list_entry,
				    list_entry) {
			if (list_itr->vsi_list_info) {
				map_info = list_itr->vsi_list_info;
				if (ice_is_bit_set(map_info->vsi_map,
						   vsi_handle)) {
					*vsi_list_id = map_info->vsi_list_id;
					return map_info;
				}
			}
		}
	} else {
		struct ice_fltr_mgmt_list_entry *list_itr;

		LIST_FOR_EACH_ENTRY(list_itr, list_head,
				    ice_fltr_mgmt_list_entry,
				    list_entry) {
			if (list_itr->vsi_count == 1 &&
			    list_itr->vsi_list_info) {
				map_info = list_itr->vsi_list_info;
				if (ice_is_bit_set(map_info->vsi_map,
						   vsi_handle)) {
					*vsi_list_id = map_info->vsi_list_id;
					return map_info;
				}
			}
		}
	}
	return NULL;
}

/**
 * ice_add_rule_internal - add rule for a given lookup type
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which rule has to be added
 * @lport: logic port number on which function add rule
 * @f_entry: structure containing MAC forwarding information
 *
 * Adds or updates the rule lists for a given recipe
 */
static enum ice_status
ice_add_rule_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
		      u8 lport, struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_info *new_fltr, *cur_fltr;
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;

	/* Load the hw_vsi_id only if the fwd action is fwd to VSI */
	if (f_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI)
		f_entry->fltr_info.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	rule_lock = &recp_list->filt_rule_lock;

	ice_acquire_lock(rule_lock);
	new_fltr = &f_entry->fltr_info;
	if (new_fltr->flag & ICE_FLTR_RX)
		new_fltr->src = lport;
	else if (new_fltr->flag & ICE_FLTR_TX)
		new_fltr->src =
			ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	m_entry = ice_find_rule_entry(&recp_list->filt_rules, new_fltr);
	if (!m_entry) {
		status = ice_create_pkt_fwd_rule(hw, recp_list, f_entry);
		goto exit_add_rule_internal;
	}

	cur_fltr = &m_entry->fltr_info;
	status = ice_add_update_vsi_list(hw, m_entry, cur_fltr, new_fltr);

exit_add_rule_internal:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_remove_vsi_list_rule
 * @hw: pointer to the hardware structure
 * @vsi_list_id: VSI list ID generated as part of allocate resource
 * @lkup_type: switch rule filter lookup type
 *
 * The VSI list should be emptied before this function is called to remove the
 * VSI list.
 */
static enum ice_status
ice_remove_vsi_list_rule(struct ice_hw *hw, u16 vsi_list_id,
			 enum ice_sw_lkup_type lkup_type)
{
	/* Free the vsi_list resource that we allocated. It is assumed that the
	 * list is empty at this point.
	 */
	return ice_aq_alloc_free_vsi_list(hw, &vsi_list_id, lkup_type,
					    ice_aqc_opc_free_res);
}

/**
 * ice_rem_update_vsi_list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle of the VSI to remove
 * @fm_list: filter management entry for which the VSI list management needs to
 *           be done
 */
static enum ice_status
ice_rem_update_vsi_list(struct ice_hw *hw, u16 vsi_handle,
			struct ice_fltr_mgmt_list_entry *fm_list)
{
	enum ice_sw_lkup_type lkup_type;
	enum ice_status status = ICE_SUCCESS;
	u16 vsi_list_id;

	if (fm_list->fltr_info.fltr_act != ICE_FWD_TO_VSI_LIST ||
	    fm_list->vsi_count == 0)
		return ICE_ERR_PARAM;

	/* A rule with the VSI being removed does not exist */
	if (!ice_is_bit_set(fm_list->vsi_list_info->vsi_map, vsi_handle))
		return ICE_ERR_DOES_NOT_EXIST;

	lkup_type = fm_list->fltr_info.lkup_type;
	vsi_list_id = fm_list->fltr_info.fwd_id.vsi_list_id;
	status = ice_update_vsi_list_rule(hw, &vsi_handle, 1, vsi_list_id, true,
					  ice_aqc_opc_update_sw_rules,
					  lkup_type);
	if (status)
		return status;

	fm_list->vsi_count--;
	ice_clear_bit(vsi_handle, fm_list->vsi_list_info->vsi_map);

	if (fm_list->vsi_count == 1 && lkup_type != ICE_SW_LKUP_VLAN) {
		struct ice_fltr_info tmp_fltr_info = fm_list->fltr_info;
		struct ice_vsi_list_map_info *vsi_list_info =
			fm_list->vsi_list_info;
		u16 rem_vsi_handle;

		rem_vsi_handle = ice_find_first_bit(vsi_list_info->vsi_map,
						    ICE_MAX_VSI);
		if (!ice_is_vsi_valid(hw, rem_vsi_handle))
			return ICE_ERR_OUT_OF_RANGE;

		/* Make sure VSI list is empty before removing it below */
		status = ice_update_vsi_list_rule(hw, &rem_vsi_handle, 1,
						  vsi_list_id, true,
						  ice_aqc_opc_update_sw_rules,
						  lkup_type);
		if (status)
			return status;

		tmp_fltr_info.fltr_act = ICE_FWD_TO_VSI;
		tmp_fltr_info.fwd_id.hw_vsi_id =
			ice_get_hw_vsi_num(hw, rem_vsi_handle);
		tmp_fltr_info.vsi_handle = rem_vsi_handle;
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr_info);
		if (status) {
			ice_debug(hw, ICE_DBG_SW, "Failed to update pkt fwd rule to FWD_TO_VSI on HW VSI %d, error %d\n",
				  tmp_fltr_info.fwd_id.hw_vsi_id, status);
			return status;
		}

		fm_list->fltr_info = tmp_fltr_info;
	}

	if ((fm_list->vsi_count == 1 && lkup_type != ICE_SW_LKUP_VLAN) ||
	    (fm_list->vsi_count == 0 && lkup_type == ICE_SW_LKUP_VLAN)) {
		struct ice_vsi_list_map_info *vsi_list_info =
			fm_list->vsi_list_info;

		/* Remove the VSI list since it is no longer used */
		status = ice_remove_vsi_list_rule(hw, vsi_list_id, lkup_type);
		if (status) {
			ice_debug(hw, ICE_DBG_SW, "Failed to remove VSI list %d, error %d\n",
				  vsi_list_id, status);
			return status;
		}

		LIST_DEL(&vsi_list_info->list_entry);
		ice_free(hw, vsi_list_info);
		fm_list->vsi_list_info = NULL;
	}

	return status;
}

/**
 * ice_remove_rule_internal - Remove a filter rule of a given type
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which the rule needs to removed
 * @f_entry: rule entry containing filter information
 */
static enum ice_status
ice_remove_rule_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
			 struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *list_elem;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	bool remove_rule = false;
	u16 vsi_handle;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;
	f_entry->fltr_info.fwd_id.hw_vsi_id =
		ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);

	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	list_elem = ice_find_rule_entry(&recp_list->filt_rules,
					&f_entry->fltr_info);
	if (!list_elem) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto exit;
	}

	if (list_elem->fltr_info.fltr_act != ICE_FWD_TO_VSI_LIST) {
		remove_rule = true;
	} else if (!list_elem->vsi_list_info) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto exit;
	} else if (list_elem->vsi_list_info->ref_cnt > 1) {
		/* a ref_cnt > 1 indicates that the vsi_list is being
		 * shared by multiple rules. Decrement the ref_cnt and
		 * remove this rule, but do not modify the list, as it
		 * is in-use by other rules.
		 */
		list_elem->vsi_list_info->ref_cnt--;
		remove_rule = true;
	} else {
		/* a ref_cnt of 1 indicates the vsi_list is only used
		 * by one rule. However, the original removal request is only
		 * for a single VSI. Update the vsi_list first, and only
		 * remove the rule if there are no further VSIs in this list.
		 */
		vsi_handle = f_entry->fltr_info.vsi_handle;
		status = ice_rem_update_vsi_list(hw, vsi_handle, list_elem);
		if (status)
			goto exit;
		/* if VSI count goes to zero after updating the VSI list */
		if (list_elem->vsi_count == 0)
			remove_rule = true;
	}

	if (remove_rule) {
		/* Remove the lookup rule */
		struct ice_sw_rule_lkup_rx_tx *s_rule;

		s_rule = (struct ice_sw_rule_lkup_rx_tx *)
			ice_malloc(hw, ice_struct_size(s_rule, hdr_data, 0));
		if (!s_rule) {
			status = ICE_ERR_NO_MEMORY;
			goto exit;
		}

		ice_fill_sw_rule(hw, &list_elem->fltr_info, s_rule,
				 ice_aqc_opc_remove_sw_rules);

		status = ice_aq_sw_rules(hw, s_rule,
					 ice_struct_size(s_rule, hdr_data, 0),
					 1, ice_aqc_opc_remove_sw_rules, NULL);

		/* Remove a book keeping from the list */
		ice_free(hw, s_rule);

		if (status)
			goto exit;

		LIST_DEL(&list_elem->list_entry);
		ice_free(hw, list_elem);
	}
exit:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_aq_get_res_alloc - get allocated resources
 * @hw: pointer to the HW struct
 * @num_entries: pointer to u16 to store the number of resource entries returned
 * @buf: pointer to buffer
 * @buf_size: size of buf
 * @cd: pointer to command details structure or NULL
 *
 * The caller-supplied buffer must be large enough to store the resource
 * information for all resource types. Each resource type is an
 * ice_aqc_get_res_resp_elem structure.
 */
enum ice_status
ice_aq_get_res_alloc(struct ice_hw *hw, u16 *num_entries,
		     struct ice_aqc_get_res_resp_elem *buf, u16 buf_size,
		     struct ice_sq_cd *cd)
{
	struct ice_aqc_get_res_alloc *resp;
	enum ice_status status;
	struct ice_aq_desc desc;

	if (!buf)
		return ICE_ERR_BAD_PTR;

	if (buf_size < ICE_AQ_GET_RES_ALLOC_BUF_LEN)
		return ICE_ERR_INVAL_SIZE;

	resp = &desc.params.get_res;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_res_alloc);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);

	if (!status && num_entries)
		*num_entries = LE16_TO_CPU(resp->resp_elem_num);

	return status;
}

/**
 * ice_aq_get_res_descs - get allocated resource descriptors
 * @hw: pointer to the hardware structure
 * @num_entries: number of resource entries in buffer
 * @buf: structure to hold response data buffer
 * @buf_size: size of buffer
 * @res_type: resource type
 * @res_shared: is resource shared
 * @desc_id: input - first desc ID to start; output - next desc ID
 * @cd: pointer to command details structure or NULL
 */
enum ice_status
ice_aq_get_res_descs(struct ice_hw *hw, u16 num_entries,
		     struct ice_aqc_res_elem *buf, u16 buf_size, u16 res_type,
		     bool res_shared, u16 *desc_id, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_allocd_res_desc *cmd;
	struct ice_aq_desc desc;
	enum ice_status status;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	cmd = &desc.params.get_res_desc;

	if (!buf)
		return ICE_ERR_PARAM;

	if (buf_size != (num_entries * sizeof(*buf)))
		return ICE_ERR_PARAM;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_allocd_res_desc);

	cmd->ops.cmd.res = CPU_TO_LE16(((res_type << ICE_AQC_RES_TYPE_S) &
					 ICE_AQC_RES_TYPE_M) | (res_shared ?
					ICE_AQC_RES_TYPE_FLAG_SHARED : 0));
	cmd->ops.cmd.first_desc = CPU_TO_LE16(*desc_id);

	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status)
		*desc_id = LE16_TO_CPU(cmd->ops.resp.next_desc);

	return status;
}

/**
 * ice_add_mac_rule - Add a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 * @sw: pointer to switch info struct for which function add rule
 * @lport: logic port number on which function add rule
 *
 * IMPORTANT: When the umac_shared flag is set to false and m_list has
 * multiple unicast addresses, the function assumes that all the
 * addresses are unique in a given add_mac call. It doesn't
 * check for duplicates in this case, removing duplicates from a given
 * list should be taken care of in the caller of this function.
 */
static enum ice_status
ice_add_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list,
		 struct ice_switch_info *sw, u8 lport)
{
	struct ice_sw_recipe *recp_list = &sw->recp_list[ICE_SW_LKUP_MAC];
	struct ice_sw_rule_lkup_rx_tx *s_rule, *r_iter;
	struct ice_fltr_list_entry *m_list_itr;
	struct LIST_HEAD_TYPE *rule_head;
	u16 total_elem_left, s_rule_size;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	u16 num_unicast = 0;
	u8 elem_sent;

	s_rule = NULL;
	rule_lock = &recp_list->filt_rule_lock;
	rule_head = &recp_list->filt_rules;

	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		u8 *add = &m_list_itr->fltr_info.l_data.mac.mac_addr[0];
		u16 vsi_handle;
		u16 hw_vsi_id;

		m_list_itr->fltr_info.flag = ICE_FLTR_TX;
		vsi_handle = m_list_itr->fltr_info.vsi_handle;
		if (!ice_is_vsi_valid(hw, vsi_handle))
			return ICE_ERR_PARAM;
		hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);
		if (m_list_itr->fltr_info.fltr_act == ICE_FWD_TO_VSI)
			m_list_itr->fltr_info.fwd_id.hw_vsi_id = hw_vsi_id;
		/* update the src in case it is VSI num */
		if (m_list_itr->fltr_info.src_id != ICE_SRC_ID_VSI)
			return ICE_ERR_PARAM;
		m_list_itr->fltr_info.src = hw_vsi_id;
		if (m_list_itr->fltr_info.lkup_type != ICE_SW_LKUP_MAC ||
		    IS_ZERO_ETHER_ADDR(add))
			return ICE_ERR_PARAM;
		if (IS_UNICAST_ETHER_ADDR(add) && !hw->umac_shared) {
			/* Don't overwrite the unicast address */
			ice_acquire_lock(rule_lock);
			if (ice_find_rule_entry(rule_head,
						&m_list_itr->fltr_info)) {
				ice_release_lock(rule_lock);
				continue;
			}
			ice_release_lock(rule_lock);
			num_unicast++;
		} else if (IS_MULTICAST_ETHER_ADDR(add) ||
			   (IS_UNICAST_ETHER_ADDR(add) && hw->umac_shared)) {
			m_list_itr->status =
				ice_add_rule_internal(hw, recp_list, lport,
						      m_list_itr);
			if (m_list_itr->status)
				return m_list_itr->status;
		}
	}

	ice_acquire_lock(rule_lock);
	/* Exit if no suitable entries were found for adding bulk switch rule */
	if (!num_unicast) {
		status = ICE_SUCCESS;
		goto ice_add_mac_exit;
	}

	/* Allocate switch rule buffer for the bulk update for unicast */
	s_rule_size = ice_struct_size(s_rule, hdr_data, DUMMY_ETH_HDR_LEN);
	s_rule = (struct ice_sw_rule_lkup_rx_tx *)
		ice_calloc(hw, num_unicast, s_rule_size);
	if (!s_rule) {
		status = ICE_ERR_NO_MEMORY;
		goto ice_add_mac_exit;
	}

	r_iter = s_rule;
	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_fltr_info *f_info = &m_list_itr->fltr_info;
		u8 *mac_addr = &f_info->l_data.mac.mac_addr[0];

		if (IS_UNICAST_ETHER_ADDR(mac_addr)) {
			ice_fill_sw_rule(hw, &m_list_itr->fltr_info, r_iter,
					 ice_aqc_opc_add_sw_rules);
			r_iter = (struct ice_sw_rule_lkup_rx_tx *)
				((u8 *)r_iter + s_rule_size);
		}
	}

	/* Call AQ bulk switch rule update for all unicast addresses */
	r_iter = s_rule;
	/* Call AQ switch rule in AQ_MAX chunk */
	for (total_elem_left = num_unicast; total_elem_left > 0;
	     total_elem_left -= elem_sent) {
		struct ice_sw_rule_lkup_rx_tx *entry = r_iter;

		elem_sent = MIN_T(u8, total_elem_left,
				  (ICE_AQ_MAX_BUF_LEN / s_rule_size));
		status = ice_aq_sw_rules(hw, entry, elem_sent * s_rule_size,
					 elem_sent, ice_aqc_opc_add_sw_rules,
					 NULL);
		if (status)
			goto ice_add_mac_exit;
		r_iter = (struct ice_sw_rule_lkup_rx_tx *)
			((u8 *)r_iter + (elem_sent * s_rule_size));
	}

	/* Fill up rule ID based on the value returned from FW */
	r_iter = s_rule;
	LIST_FOR_EACH_ENTRY(m_list_itr, m_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_fltr_info *f_info = &m_list_itr->fltr_info;
		u8 *mac_addr = &f_info->l_data.mac.mac_addr[0];
		struct ice_fltr_mgmt_list_entry *fm_entry;

		if (IS_UNICAST_ETHER_ADDR(mac_addr)) {
			f_info->fltr_rule_id =
				LE16_TO_CPU(r_iter->index);
			f_info->fltr_act = ICE_FWD_TO_VSI;
			/* Create an entry to track this MAC address */
			fm_entry = (struct ice_fltr_mgmt_list_entry *)
				ice_malloc(hw, sizeof(*fm_entry));
			if (!fm_entry) {
				status = ICE_ERR_NO_MEMORY;
				goto ice_add_mac_exit;
			}
			fm_entry->fltr_info = *f_info;
			fm_entry->vsi_count = 1;
			/* The book keeping entries will get removed when
			 * base driver calls remove filter AQ command
			 */

			LIST_ADD(&fm_entry->list_entry, rule_head);
			r_iter = (struct ice_sw_rule_lkup_rx_tx *)
				((u8 *)r_iter + s_rule_size);
		}
	}

ice_add_mac_exit:
	ice_release_lock(rule_lock);
	if (s_rule)
		ice_free(hw, s_rule);
	return status;
}

/**
 * ice_add_mac - Add a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 *
 * Function add MAC rule for logical port from HW struct
 */
enum ice_status ice_add_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list)
{
	if (!m_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_mac_rule(hw, m_list, hw->switch_info,
				hw->port_info->lport);
}

/**
 * ice_add_vlan_internal - Add one VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @recp_list: recipe list for which rule has to be added
 * @f_entry: filter entry containing one VLAN information
 */
static enum ice_status
ice_add_vlan_internal(struct ice_hw *hw, struct ice_sw_recipe *recp_list,
		      struct ice_fltr_list_entry *f_entry)
{
	struct ice_fltr_mgmt_list_entry *v_list_itr;
	struct ice_fltr_info *new_fltr, *cur_fltr;
	enum ice_sw_lkup_type lkup_type;
	u16 vsi_list_id = 0, vsi_handle;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;

	if (!ice_is_vsi_valid(hw, f_entry->fltr_info.vsi_handle))
		return ICE_ERR_PARAM;

	f_entry->fltr_info.fwd_id.hw_vsi_id =
		ice_get_hw_vsi_num(hw, f_entry->fltr_info.vsi_handle);
	new_fltr = &f_entry->fltr_info;

	/* VLAN ID should only be 12 bits */
	if (new_fltr->l_data.vlan.vlan_id > ICE_MAX_VLAN_ID)
		return ICE_ERR_PARAM;

	if (new_fltr->src_id != ICE_SRC_ID_VSI)
		return ICE_ERR_PARAM;

	new_fltr->src = new_fltr->fwd_id.hw_vsi_id;
	lkup_type = new_fltr->lkup_type;
	vsi_handle = new_fltr->vsi_handle;
	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	v_list_itr = ice_find_rule_entry(&recp_list->filt_rules, new_fltr);
	if (!v_list_itr) {
		struct ice_vsi_list_map_info *map_info = NULL;

		if (new_fltr->fltr_act == ICE_FWD_TO_VSI) {
			/* All VLAN pruning rules use a VSI list. Check if
			 * there is already a VSI list containing VSI that we
			 * want to add. If found, use the same vsi_list_id for
			 * this new VLAN rule or else create a new list.
			 */
			map_info = ice_find_vsi_list_entry(recp_list,
							   vsi_handle,
							   &vsi_list_id);
			if (!map_info) {
				status = ice_create_vsi_list_rule(hw,
								  &vsi_handle,
								  1,
								  &vsi_list_id,
								  lkup_type);
				if (status)
					goto exit;
			}
			/* Convert the action to forwarding to a VSI list. */
			new_fltr->fltr_act = ICE_FWD_TO_VSI_LIST;
			new_fltr->fwd_id.vsi_list_id = vsi_list_id;
		}

		status = ice_create_pkt_fwd_rule(hw, recp_list, f_entry);
		if (!status) {
			v_list_itr = ice_find_rule_entry(&recp_list->filt_rules,
							 new_fltr);
			if (!v_list_itr) {
				status = ICE_ERR_DOES_NOT_EXIST;
				goto exit;
			}
			/* reuse VSI list for new rule and increment ref_cnt */
			if (map_info) {
				v_list_itr->vsi_list_info = map_info;
				map_info->ref_cnt++;
			} else {
				v_list_itr->vsi_list_info =
					ice_create_vsi_list_map(hw, &vsi_handle,
								1, vsi_list_id);
			}
		}
	} else if (v_list_itr->vsi_list_info->ref_cnt == 1) {
		/* Update existing VSI list to add new VSI ID only if it used
		 * by one VLAN rule.
		 */
		cur_fltr = &v_list_itr->fltr_info;
		status = ice_add_update_vsi_list(hw, v_list_itr, cur_fltr,
						 new_fltr);
	} else {
		/* If VLAN rule exists and VSI list being used by this rule is
		 * referenced by more than 1 VLAN rule. Then create a new VSI
		 * list appending previous VSI with new VSI and update existing
		 * VLAN rule to point to new VSI list ID
		 */
		struct ice_fltr_info tmp_fltr;
		u16 vsi_handle_arr[2];
		u16 cur_handle;

		/* Current implementation only supports reusing VSI list with
		 * one VSI count. We should never hit below condition
		 */
		if (v_list_itr->vsi_count > 1 &&
		    v_list_itr->vsi_list_info->ref_cnt > 1) {
			ice_debug(hw, ICE_DBG_SW, "Invalid configuration: Optimization to reuse VSI list with more than one VSI is not being done yet\n");
			status = ICE_ERR_CFG;
			goto exit;
		}

		cur_handle =
			ice_find_first_bit(v_list_itr->vsi_list_info->vsi_map,
					   ICE_MAX_VSI);

		/* A rule already exists with the new VSI being added */
		if (cur_handle == vsi_handle) {
			status = ICE_ERR_ALREADY_EXISTS;
			goto exit;
		}

		vsi_handle_arr[0] = cur_handle;
		vsi_handle_arr[1] = vsi_handle;
		status = ice_create_vsi_list_rule(hw, &vsi_handle_arr[0], 2,
						  &vsi_list_id, lkup_type);
		if (status)
			goto exit;

		tmp_fltr = v_list_itr->fltr_info;
		tmp_fltr.fltr_rule_id = v_list_itr->fltr_info.fltr_rule_id;
		tmp_fltr.fwd_id.vsi_list_id = vsi_list_id;
		tmp_fltr.fltr_act = ICE_FWD_TO_VSI_LIST;
		/* Update the previous switch rule to a new VSI list which
		 * includes current VSI that is requested
		 */
		status = ice_update_pkt_fwd_rule(hw, &tmp_fltr);
		if (status)
			goto exit;

		/* before overriding VSI list map info. decrement ref_cnt of
		 * previous VSI list
		 */
		v_list_itr->vsi_list_info->ref_cnt--;

		/* now update to newly created list */
		v_list_itr->fltr_info.fwd_id.vsi_list_id = vsi_list_id;
		v_list_itr->vsi_list_info =
			ice_create_vsi_list_map(hw, &vsi_handle_arr[0], 2,
						vsi_list_id);
		v_list_itr->vsi_count++;
	}

exit:
	ice_release_lock(rule_lock);
	return status;
}

/**
 * ice_add_vlan_rule - Add VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN entries and forwarding information
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
ice_add_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list,
		  struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *v_list_itr;
	struct ice_sw_recipe *recp_list;

	recp_list = &sw->recp_list[ICE_SW_LKUP_VLAN];
	LIST_FOR_EACH_ENTRY(v_list_itr, v_list, ice_fltr_list_entry,
			    list_entry) {
		if (v_list_itr->fltr_info.lkup_type != ICE_SW_LKUP_VLAN)
			return ICE_ERR_PARAM;
		v_list_itr->fltr_info.flag = ICE_FLTR_TX;
		v_list_itr->status = ice_add_vlan_internal(hw, recp_list,
							   v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_add_vlan - Add a VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN and forwarding information
 *
 * Function add VLAN rule for logical port from HW struct
 */
enum ice_status ice_add_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list)
{
	if (!v_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_vlan_rule(hw, v_list, hw->switch_info);
}

/**
 * ice_add_eth_mac_rule - Add ethertype and MAC based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ether type MAC filter, MAC is optional
 * @sw: pointer to switch info struct for which function add rule
 * @lport: logic port number on which function add rule
 *
 * This function requires the caller to populate the entries in
 * the filter list with the necessary fields (including flags to
 * indicate Tx or Rx rules).
 */
static enum ice_status
ice_add_eth_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list,
		     struct ice_switch_info *sw, u8 lport)
{
	struct ice_fltr_list_entry *em_list_itr;

	LIST_FOR_EACH_ENTRY(em_list_itr, em_list, ice_fltr_list_entry,
			    list_entry) {
		struct ice_sw_recipe *recp_list;
		enum ice_sw_lkup_type l_type;

		l_type = em_list_itr->fltr_info.lkup_type;
		recp_list = &sw->recp_list[l_type];

		if (l_type != ICE_SW_LKUP_ETHERTYPE_MAC &&
		    l_type != ICE_SW_LKUP_ETHERTYPE)
			return ICE_ERR_PARAM;

		em_list_itr->status = ice_add_rule_internal(hw, recp_list,
							    lport,
							    em_list_itr);
		if (em_list_itr->status)
			return em_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_add_eth_mac - Add a ethertype based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype and forwarding information
 *
 * Function add ethertype rule for logical port from HW struct
 */
enum ice_status
ice_add_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list)
{
	if (!em_list || !hw)
		return ICE_ERR_PARAM;

	return ice_add_eth_mac_rule(hw, em_list, hw->switch_info,
				    hw->port_info->lport);
}

/**
 * ice_remove_eth_mac_rule - Remove an ethertype (or MAC) based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype or ethertype MAC entries
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
ice_remove_eth_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list,
			struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *em_list_itr, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(em_list_itr, tmp, em_list, ice_fltr_list_entry,
				 list_entry) {
		struct ice_sw_recipe *recp_list;
		enum ice_sw_lkup_type l_type;

		l_type = em_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_ETHERTYPE_MAC &&
		    l_type != ICE_SW_LKUP_ETHERTYPE)
			return ICE_ERR_PARAM;

		recp_list = &sw->recp_list[l_type];
		em_list_itr->status = ice_remove_rule_internal(hw, recp_list,
							       em_list_itr);
		if (em_list_itr->status)
			return em_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_eth_mac - remove a ethertype based filter rule
 * @hw: pointer to the hardware structure
 * @em_list: list of ethertype and forwarding information
 *
 */
enum ice_status
ice_remove_eth_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *em_list)
{
	if (!em_list || !hw)
		return ICE_ERR_PARAM;

	return ice_remove_eth_mac_rule(hw, em_list, hw->switch_info);
}

/**
 * ice_get_lg_act_aqc_res_type - get resource type for a large action
 * @res_type: resource type to be filled in case of function success
 * @num_acts: number of actions to hold with a large action entry
 *
 * Get resource type for a large action depending on the number
 * of single actions that it contains.
 */
static enum ice_status
ice_get_lg_act_aqc_res_type(u16 *res_type, int num_acts)
{
	if (!res_type)
		return ICE_ERR_BAD_PTR;

	/* If num_acts is 1, use ICE_AQC_RES_TYPE_WIDE_TABLE_1.
	 * If num_acts is 2, use ICE_AQC_RES_TYPE_WIDE_TABLE_3.
	 * If num_acts is greater than 2, then use
	 * ICE_AQC_RES_TYPE_WIDE_TABLE_4.
	 * The num_acts cannot be equal to 0 or greater than 4.
	 */
	switch (num_acts) {
	case 1:
		*res_type = ICE_AQC_RES_TYPE_WIDE_TABLE_1;
		break;
	case 2:
		*res_type = ICE_AQC_RES_TYPE_WIDE_TABLE_2;
		break;
	case 3:
	case 4:
		*res_type = ICE_AQC_RES_TYPE_WIDE_TABLE_4;
		break;
	default:
		return ICE_ERR_PARAM;
	}

	return ICE_SUCCESS;
}

/**
 * ice_alloc_res_lg_act - add large action resource
 * @hw: pointer to the hardware structure
 * @l_id: large action ID to fill it in
 * @num_acts: number of actions to hold with a large action entry
 */
static enum ice_status
ice_alloc_res_lg_act(struct ice_hw *hw, u16 *l_id, u16 num_acts)
{
	struct ice_aqc_alloc_free_res_elem *sw_buf;
	enum ice_status status;
	u16 buf_len, res_type;

	if (!l_id)
		return ICE_ERR_BAD_PTR;

	status = ice_get_lg_act_aqc_res_type(&res_type, num_acts);
	if (status)
		return status;

	/* Allocate resource for large action */
	buf_len = ice_struct_size(sw_buf, elem, 1);
	sw_buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!sw_buf)
		return ICE_ERR_NO_MEMORY;

	sw_buf->res_type = CPU_TO_LE16(res_type);
	sw_buf->num_elems = CPU_TO_LE16(1);

	status = ice_aq_alloc_free_res(hw, 1, sw_buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (!status)
		*l_id = LE16_TO_CPU(sw_buf->elem[0].e.sw_resp);

	ice_free(hw, sw_buf);

	return status;
}

/**
 * ice_rem_sw_rule_info
 * @hw: pointer to the hardware structure
 * @rule_head: pointer to the switch list structure that we want to delete
 */
static void
ice_rem_sw_rule_info(struct ice_hw *hw, struct LIST_HEAD_TYPE *rule_head)
{
	if (!LIST_EMPTY(rule_head)) {
		struct ice_fltr_mgmt_list_entry *entry;
		struct ice_fltr_mgmt_list_entry *tmp;

		LIST_FOR_EACH_ENTRY_SAFE(entry, tmp, rule_head,
					 ice_fltr_mgmt_list_entry, list_entry) {
			LIST_DEL(&entry->list_entry);
			ice_free(hw, entry);
		}
	}
}

/**
 * ice_rem_all_sw_rules_info
 * @hw: pointer to the hardware structure
 */
void ice_rem_all_sw_rules_info(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	u8 i;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *rule_head;

		rule_head = &sw->recp_list[i].filt_rules;
		if (!sw->recp_list[i].adv_rule)
			ice_rem_sw_rule_info(hw, rule_head);
	}
}

/**
 * ice_cfg_dflt_vsi - change state of VSI to set/clear default
 * @pi: pointer to the port_info structure
 * @vsi_handle: VSI handle to set as default
 * @set: true to add the above mentioned switch rule, false to remove it
 * @direction: ICE_FLTR_RX or ICE_FLTR_TX
 *
 * add filter rule to set/unset given VSI as default VSI for the switch
 * (represented by swid)
 */
enum ice_status
ice_cfg_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle, bool set,
		 u8 direction)
{
	struct ice_fltr_list_entry f_list_entry;
	struct ice_sw_recipe *recp_list = NULL;
	struct ice_fltr_info f_info;
	struct ice_hw *hw = pi->hw;
	enum ice_status status;
	u8 lport = pi->lport;
	u16 hw_vsi_id;
	recp_list = &pi->hw->switch_info->recp_list[ICE_SW_LKUP_DFLT];

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	ice_memset(&f_info, 0, sizeof(f_info), ICE_NONDMA_MEM);

	f_info.lkup_type = ICE_SW_LKUP_DFLT;
	f_info.flag = direction;
	f_info.fltr_act = ICE_FWD_TO_VSI;
	f_info.fwd_id.hw_vsi_id = hw_vsi_id;
	f_info.vsi_handle = vsi_handle;

	if (f_info.flag & ICE_FLTR_RX) {
		f_info.src = pi->lport;
		f_info.src_id = ICE_SRC_ID_LPORT;
	} else if (f_info.flag & ICE_FLTR_TX) {
		f_info.src_id = ICE_SRC_ID_VSI;
		f_info.src = hw_vsi_id;
	}
	f_list_entry.fltr_info = f_info;

	if (set)
		status = ice_add_rule_internal(hw, recp_list, lport,
					       &f_list_entry);
	else
		status = ice_remove_rule_internal(hw, recp_list,
						  &f_list_entry);

	return status;
}

/**
 * ice_check_if_dflt_vsi - check if VSI is default VSI
 * @pi: pointer to the port_info structure
 * @vsi_handle: vsi handle to check for in filter list
 * @rule_exists: indicates if there are any VSI's in the rule list
 *
 * checks if the VSI is in a default VSI list, and also indicates
 * if the default VSI list is empty
 */
bool ice_check_if_dflt_vsi(struct ice_port_info *pi, u16 vsi_handle,
			   bool *rule_exists)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_sw_recipe *recp_list;
	struct ice_lock *rule_lock;
	bool ret = false;
	recp_list = &pi->hw->switch_info->recp_list[ICE_SW_LKUP_DFLT];
	rule_lock = &recp_list->filt_rule_lock;
	rule_head = &recp_list->filt_rules;

	ice_acquire_lock(rule_lock);

	if (rule_exists && !LIST_EMPTY(rule_head))
		*rule_exists = true;

	LIST_FOR_EACH_ENTRY(fm_entry, rule_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		if (ice_vsi_uses_fltr(fm_entry, vsi_handle)) {
			ret = true;
			break;
		}
	}

	ice_release_lock(rule_lock);
	return ret;
}

/**
 * ice_find_ucast_rule_entry - Search for a unicast MAC filter rule entry
 * @list_head: head of rule list
 * @f_info: rule information
 *
 * Helper function to search for a unicast rule entry - this is to be used
 * to remove unicast MAC filter that is not shared with other VSIs on the
 * PF switch.
 *
 * Returns pointer to entry storing the rule if found
 */
static struct ice_fltr_mgmt_list_entry *
ice_find_ucast_rule_entry(struct LIST_HEAD_TYPE *list_head,
			  struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *list_itr;

	LIST_FOR_EACH_ENTRY(list_itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		if (!memcmp(&f_info->l_data, &list_itr->fltr_info.l_data,
			    sizeof(f_info->l_data)) &&
		    f_info->fwd_id.hw_vsi_id ==
		    list_itr->fltr_info.fwd_id.hw_vsi_id &&
		    f_info->flag == list_itr->fltr_info.flag)
			return list_itr;
	}
	return NULL;
}

/**
 * ice_remove_mac_rule - remove a MAC based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 * @recp_list: list from which function remove MAC address
 *
 * This function removes either a MAC filter rule or a specific VSI from a
 * VSI list for a multicast MAC address.
 *
 * Returns ICE_ERR_DOES_NOT_EXIST if a given entry was not added by
 * ice_add_mac. Caller should be aware that this call will only work if all
 * the entries passed into m_list were added previously. It will not attempt to
 * do a partial remove of entries that were found.
 */
static enum ice_status
ice_remove_mac_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list,
		    struct ice_sw_recipe *recp_list)
{
	struct ice_fltr_list_entry *list_itr, *tmp;
	struct ice_lock *rule_lock; /* Lock to protect filter rule list */

	if (!m_list)
		return ICE_ERR_PARAM;

	rule_lock = &recp_list->filt_rule_lock;
	LIST_FOR_EACH_ENTRY_SAFE(list_itr, tmp, m_list, ice_fltr_list_entry,
				 list_entry) {
		enum ice_sw_lkup_type l_type = list_itr->fltr_info.lkup_type;
		u8 *add = &list_itr->fltr_info.l_data.mac.mac_addr[0];
		u16 vsi_handle;

		if (l_type != ICE_SW_LKUP_MAC)
			return ICE_ERR_PARAM;

		vsi_handle = list_itr->fltr_info.vsi_handle;
		if (!ice_is_vsi_valid(hw, vsi_handle))
			return ICE_ERR_PARAM;

		list_itr->fltr_info.fwd_id.hw_vsi_id =
					ice_get_hw_vsi_num(hw, vsi_handle);
		if (IS_UNICAST_ETHER_ADDR(add) && !hw->umac_shared) {
			/* Don't remove the unicast address that belongs to
			 * another VSI on the switch, since it is not being
			 * shared...
			 */
			ice_acquire_lock(rule_lock);
			if (!ice_find_ucast_rule_entry(&recp_list->filt_rules,
						       &list_itr->fltr_info)) {
				ice_release_lock(rule_lock);
				return ICE_ERR_DOES_NOT_EXIST;
			}
			ice_release_lock(rule_lock);
		}
		list_itr->status = ice_remove_rule_internal(hw, recp_list,
							    list_itr);
		if (list_itr->status)
			return list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_mac - remove a MAC address based filter rule
 * @hw: pointer to the hardware structure
 * @m_list: list of MAC addresses and forwarding information
 *
 */
enum ice_status ice_remove_mac(struct ice_hw *hw, struct LIST_HEAD_TYPE *m_list)
{
	struct ice_sw_recipe *recp_list;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];
	return ice_remove_mac_rule(hw, m_list, recp_list);
}

/**
 * ice_remove_vlan_rule - Remove VLAN based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN entries and forwarding information
 * @recp_list: list from which function remove VLAN
 */
static enum ice_status
ice_remove_vlan_rule(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list,
		     struct ice_sw_recipe *recp_list)
{
	struct ice_fltr_list_entry *v_list_itr, *tmp;

	LIST_FOR_EACH_ENTRY_SAFE(v_list_itr, tmp, v_list, ice_fltr_list_entry,
				 list_entry) {
		enum ice_sw_lkup_type l_type = v_list_itr->fltr_info.lkup_type;

		if (l_type != ICE_SW_LKUP_VLAN)
			return ICE_ERR_PARAM;
		v_list_itr->status = ice_remove_rule_internal(hw, recp_list,
							      v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * ice_remove_vlan - remove a VLAN address based filter rule
 * @hw: pointer to the hardware structure
 * @v_list: list of VLAN and forwarding information
 *
 */
enum ice_status
ice_remove_vlan(struct ice_hw *hw, struct LIST_HEAD_TYPE *v_list)
{
	struct ice_sw_recipe *recp_list;

	if (!v_list || !hw)
		return ICE_ERR_PARAM;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_VLAN];
	return ice_remove_vlan_rule(hw, v_list, recp_list);
}

/**
 * ice_vsi_uses_fltr - Determine if given VSI uses specified filter
 * @fm_entry: filter entry to inspect
 * @vsi_handle: VSI handle to compare with filter info
 */
static bool
ice_vsi_uses_fltr(struct ice_fltr_mgmt_list_entry *fm_entry, u16 vsi_handle)
{
	return ((fm_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI &&
		 fm_entry->fltr_info.vsi_handle == vsi_handle) ||
		(fm_entry->fltr_info.fltr_act == ICE_FWD_TO_VSI_LIST &&
		 fm_entry->vsi_list_info &&
		 (ice_is_bit_set(fm_entry->vsi_list_info->vsi_map,
				 vsi_handle))));
}

/**
 * ice_add_entry_to_vsi_fltr_list - Add copy of fltr_list_entry to remove list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @vsi_list_head: pointer to the list to add entry to
 * @fi: pointer to fltr_info of filter entry to copy & add
 *
 * Helper function, used when creating a list of filters to remove from
 * a specific VSI. The entry added to vsi_list_head is a COPY of the
 * original filter entry, with the exception of fltr_info.fltr_act and
 * fltr_info.fwd_id fields. These are set such that later logic can
 * extract which VSI to remove the fltr from, and pass on that information.
 */
static enum ice_status
ice_add_entry_to_vsi_fltr_list(struct ice_hw *hw, u16 vsi_handle,
			       struct LIST_HEAD_TYPE *vsi_list_head,
			       struct ice_fltr_info *fi)
{
	struct ice_fltr_list_entry *tmp;

	/* this memory is freed up in the caller function
	 * once filters for this VSI are removed
	 */
	tmp = (struct ice_fltr_list_entry *)ice_malloc(hw, sizeof(*tmp));
	if (!tmp)
		return ICE_ERR_NO_MEMORY;

	tmp->fltr_info = *fi;

	/* Overwrite these fields to indicate which VSI to remove filter from,
	 * so find and remove logic can extract the information from the
	 * list entries. Note that original entries will still have proper
	 * values.
	 */
	tmp->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	tmp->fltr_info.vsi_handle = vsi_handle;
	tmp->fltr_info.fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	LIST_ADD(&tmp->list_entry, vsi_list_head);

	return ICE_SUCCESS;
}

/**
 * ice_add_to_vsi_fltr_list - Add VSI filters to the list
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @lkup_list_head: pointer to the list that has certain lookup type filters
 * @vsi_list_head: pointer to the list pertaining to VSI with vsi_handle
 *
 * Locates all filters in lkup_list_head that are used by the given VSI,
 * and adds COPIES of those entries to vsi_list_head (intended to be used
 * to remove the listed filters).
 * Note that this means all entries in vsi_list_head must be explicitly
 * deallocated by the caller when done with list.
 */
static enum ice_status
ice_add_to_vsi_fltr_list(struct ice_hw *hw, u16 vsi_handle,
			 struct LIST_HEAD_TYPE *lkup_list_head,
			 struct LIST_HEAD_TYPE *vsi_list_head)
{
	struct ice_fltr_mgmt_list_entry *fm_entry;
	enum ice_status status = ICE_SUCCESS;

	/* check to make sure VSI ID is valid and within boundary */
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	LIST_FOR_EACH_ENTRY(fm_entry, lkup_list_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		if (!ice_vsi_uses_fltr(fm_entry, vsi_handle))
			continue;

		status = ice_add_entry_to_vsi_fltr_list(hw, vsi_handle,
							vsi_list_head,
							&fm_entry->fltr_info);
		if (status)
			return status;
	}
	return status;
}

/**
 * ice_determine_promisc_mask
 * @fi: filter info to parse
 * @promisc_mask: pointer to mask to be filled in
 *
 * Helper function to determine which ICE_PROMISC_ mask corresponds
 * to given filter into.
 */
static void ice_determine_promisc_mask(struct ice_fltr_info *fi,
				       ice_bitmap_t *promisc_mask)
{
	u16 vid = fi->l_data.mac_vlan.vlan_id;
	u8 *macaddr = fi->l_data.mac.mac_addr;
	bool is_tx_fltr = false;

	ice_zero_bitmap(promisc_mask, ICE_PROMISC_MAX);

	if (fi->flag == ICE_FLTR_TX)
		is_tx_fltr = true;

	if (IS_BROADCAST_ETHER_ADDR(macaddr)) {
		ice_set_bit(is_tx_fltr ? ICE_PROMISC_BCAST_TX
				       : ICE_PROMISC_BCAST_RX, promisc_mask);
	} else if (IS_MULTICAST_ETHER_ADDR(macaddr)) {
		ice_set_bit(is_tx_fltr ? ICE_PROMISC_MCAST_TX
				       : ICE_PROMISC_MCAST_RX, promisc_mask);
	} else if (IS_UNICAST_ETHER_ADDR(macaddr)) {
		ice_set_bit(is_tx_fltr ? ICE_PROMISC_UCAST_TX
				       : ICE_PROMISC_UCAST_RX, promisc_mask);
	}

	if (vid) {
		ice_set_bit(is_tx_fltr ? ICE_PROMISC_VLAN_TX
				       : ICE_PROMISC_VLAN_RX, promisc_mask);
	}
}

/**
 * _ice_get_vsi_promisc - get promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 * @sw: pointer to switch info struct for which function add rule
 * @lkup: switch rule filter lookup type
 */
static enum ice_status
_ice_get_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		     ice_bitmap_t *promisc_mask, u16 *vid,
		     struct ice_switch_info *sw, enum ice_sw_lkup_type lkup)
{
	ice_declare_bitmap(fltr_promisc_mask, ICE_PROMISC_MAX);
	struct ice_fltr_mgmt_list_entry *itr;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */

	if (!ice_is_vsi_valid(hw, vsi_handle) ||
	    (lkup != ICE_SW_LKUP_PROMISC && lkup != ICE_SW_LKUP_PROMISC_VLAN))
		return ICE_ERR_PARAM;

	*vid = 0;
	rule_head = &sw->recp_list[lkup].filt_rules;
	rule_lock = &sw->recp_list[lkup].filt_rule_lock;

	ice_zero_bitmap(promisc_mask, ICE_PROMISC_MAX);

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(itr, rule_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		/* Continue if this filter doesn't apply to this VSI or the
		 * VSI ID is not in the VSI map for this filter
		 */
		if (!ice_vsi_uses_fltr(itr, vsi_handle))
			continue;

		ice_determine_promisc_mask(&itr->fltr_info, fltr_promisc_mask);
		ice_or_bitmap(promisc_mask, promisc_mask, fltr_promisc_mask,
			      ICE_PROMISC_MAX);

	}
	ice_release_lock(rule_lock);

	return ICE_SUCCESS;
}

/**
 * ice_get_vsi_promisc - get promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 */
enum ice_status
ice_get_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		    ice_bitmap_t *promisc_mask, u16 *vid)
{
	if (!vid || !promisc_mask || !hw)
		return ICE_ERR_PARAM;

	return _ice_get_vsi_promisc(hw, vsi_handle, promisc_mask,
				    vid, hw->switch_info, ICE_SW_LKUP_PROMISC);
}

/**
 * ice_get_vsi_vlan_promisc - get VLAN promiscuous mode of given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to retrieve info from
 * @promisc_mask: pointer to mask to be filled in
 * @vid: VLAN ID of promisc VLAN VSI
 */
enum ice_status
ice_get_vsi_vlan_promisc(struct ice_hw *hw, u16 vsi_handle,
			 ice_bitmap_t *promisc_mask, u16 *vid)
{
	if (!hw || !promisc_mask || !vid)
		return ICE_ERR_PARAM;

	return _ice_get_vsi_promisc(hw, vsi_handle, promisc_mask,
				    vid, hw->switch_info,
				    ICE_SW_LKUP_PROMISC_VLAN);
}

/**
 * ice_remove_promisc - Remove promisc based filter rules
 * @hw: pointer to the hardware structure
 * @recp_id: recipe ID for which the rule needs to removed
 * @v_list: list of promisc entries
 */
static enum ice_status
ice_remove_promisc(struct ice_hw *hw, u8 recp_id,
		   struct LIST_HEAD_TYPE *v_list)
{
	struct ice_fltr_list_entry *v_list_itr, *tmp;
	struct ice_sw_recipe *recp_list;

	recp_list = &hw->switch_info->recp_list[recp_id];
	LIST_FOR_EACH_ENTRY_SAFE(v_list_itr, tmp, v_list, ice_fltr_list_entry,
				 list_entry) {
		v_list_itr->status =
			ice_remove_rule_internal(hw, recp_list, v_list_itr);
		if (v_list_itr->status)
			return v_list_itr->status;
	}
	return ICE_SUCCESS;
}

/**
 * _ice_clear_vsi_promisc - clear specified promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to clear mode
 * @promisc_mask: pointer to mask of promiscuous config bits to clear
 * @vid: VLAN ID to clear VLAN promiscuous
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		       ice_bitmap_t *promisc_mask, u16 vid,
		       struct ice_switch_info *sw)
{
	ice_declare_bitmap(compl_promisc_mask, ICE_PROMISC_MAX);
	ice_declare_bitmap(fltr_promisc_mask, ICE_PROMISC_MAX);
	struct ice_fltr_list_entry *fm_entry, *tmp;
	struct LIST_HEAD_TYPE remove_list_head;
	struct ice_fltr_mgmt_list_entry *itr;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status status = ICE_SUCCESS;
	u8 recipe_id;

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;

	if (ice_is_bit_set(promisc_mask, ICE_PROMISC_VLAN_RX) &&
	    ice_is_bit_set(promisc_mask, ICE_PROMISC_VLAN_TX))
		recipe_id = ICE_SW_LKUP_PROMISC_VLAN;
	else
		recipe_id = ICE_SW_LKUP_PROMISC;

	rule_head = &sw->recp_list[recipe_id].filt_rules;
	rule_lock = &sw->recp_list[recipe_id].filt_rule_lock;

	INIT_LIST_HEAD(&remove_list_head);

	ice_acquire_lock(rule_lock);
	LIST_FOR_EACH_ENTRY(itr, rule_head,
			    ice_fltr_mgmt_list_entry, list_entry) {
		struct ice_fltr_info *fltr_info;
		ice_zero_bitmap(compl_promisc_mask, ICE_PROMISC_MAX);

		if (!ice_vsi_uses_fltr(itr, vsi_handle))
			continue;
		fltr_info = &itr->fltr_info;

		if (recipe_id == ICE_SW_LKUP_PROMISC_VLAN &&
		    vid != fltr_info->l_data.mac_vlan.vlan_id)
			continue;

		ice_determine_promisc_mask(fltr_info, fltr_promisc_mask);
		ice_andnot_bitmap(compl_promisc_mask, fltr_promisc_mask,
				  promisc_mask, ICE_PROMISC_MAX);

		/* Skip if filter is not completely specified by given mask */
		if (ice_is_any_bit_set(compl_promisc_mask, ICE_PROMISC_MAX))
			continue;

		status = ice_add_entry_to_vsi_fltr_list(hw, vsi_handle,
							&remove_list_head,
							fltr_info);
		if (status) {
			ice_release_lock(rule_lock);
			goto free_fltr_list;
		}
	}
	ice_release_lock(rule_lock);

	status = ice_remove_promisc(hw, recipe_id, &remove_list_head);

free_fltr_list:
	LIST_FOR_EACH_ENTRY_SAFE(fm_entry, tmp, &remove_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&fm_entry->list_entry);
		ice_free(hw, fm_entry);
	}

	return status;
}

/**
 * ice_clear_vsi_promisc - clear specified promiscuous mode(s) for given VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to clear mode
 * @promisc_mask: pointer to mask of promiscuous config bits to clear
 * @vid: VLAN ID to clear VLAN promiscuous
 */
enum ice_status
ice_clear_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		      ice_bitmap_t *promisc_mask, u16 vid)
{
	if (!hw || !promisc_mask)
		return ICE_ERR_PARAM;

	return _ice_clear_vsi_promisc(hw, vsi_handle, promisc_mask,
				      vid, hw->switch_info);
}

/**
 * _ice_set_vsi_promisc - set given VSI to given promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: pointer to mask of promiscuous config bits
 * @vid: VLAN ID to set VLAN promiscuous
 * @lport: logical port number to configure promisc mode
 * @sw: pointer to switch info struct for which function add rule
 */
static enum ice_status
_ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		     ice_bitmap_t *promisc_mask, u16 vid, u8 lport,
		     struct ice_switch_info *sw)
{
	enum { UCAST_FLTR = 1, MCAST_FLTR, BCAST_FLTR };
	ice_declare_bitmap(p_mask, ICE_PROMISC_MAX);
	struct ice_fltr_list_entry f_list_entry;
	struct ice_fltr_info new_fltr;
	enum ice_status status = ICE_SUCCESS;
	bool is_tx_fltr;
	u16 hw_vsi_id;
	int pkt_type;
	u8 recipe_id;

	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	ice_memset(&new_fltr, 0, sizeof(new_fltr), ICE_NONDMA_MEM);

	/* Do not modify original bitmap */
	ice_cp_bitmap(p_mask, promisc_mask, ICE_PROMISC_MAX);

	if (ice_is_bit_set(p_mask, ICE_PROMISC_VLAN_RX) &&
	    ice_is_bit_set(p_mask, ICE_PROMISC_VLAN_TX)) {
		new_fltr.lkup_type = ICE_SW_LKUP_PROMISC_VLAN;
		new_fltr.l_data.mac_vlan.vlan_id = vid;
		recipe_id = ICE_SW_LKUP_PROMISC_VLAN;
	} else {
		new_fltr.lkup_type = ICE_SW_LKUP_PROMISC;
		recipe_id = ICE_SW_LKUP_PROMISC;
	}

	/* Separate filters must be set for each direction/packet type
	 * combination, so we will loop over the mask value, store the
	 * individual type, and clear it out in the input mask as it
	 * is found.
	 */
	while (ice_is_any_bit_set(p_mask, ICE_PROMISC_MAX)) {
		struct ice_sw_recipe *recp_list;
		u8 *mac_addr;

		pkt_type = 0;
		is_tx_fltr = false;

		if (ice_test_and_clear_bit(ICE_PROMISC_UCAST_RX,
					   p_mask)) {
			pkt_type = UCAST_FLTR;
		} else if (ice_test_and_clear_bit(ICE_PROMISC_UCAST_TX,
						  p_mask)) {
			pkt_type = UCAST_FLTR;
			is_tx_fltr = true;
		} else if (ice_test_and_clear_bit(ICE_PROMISC_MCAST_RX,
						  p_mask)) {
			pkt_type = MCAST_FLTR;
		} else if (ice_test_and_clear_bit(ICE_PROMISC_MCAST_TX,
						  p_mask)) {
			pkt_type = MCAST_FLTR;
			is_tx_fltr = true;
		} else if (ice_test_and_clear_bit(ICE_PROMISC_BCAST_RX,
						  p_mask)) {
			pkt_type = BCAST_FLTR;
		} else if (ice_test_and_clear_bit(ICE_PROMISC_BCAST_TX,
						  p_mask)) {
			pkt_type = BCAST_FLTR;
			is_tx_fltr = true;
		}

		/* Check for VLAN promiscuous flag */
		if (ice_is_bit_set(p_mask, ICE_PROMISC_VLAN_RX)) {
			ice_clear_bit(ICE_PROMISC_VLAN_RX, p_mask);
		} else if (ice_test_and_clear_bit(ICE_PROMISC_VLAN_TX,
						  p_mask)) {
			is_tx_fltr = true;
		}
		/* Set filter DA based on packet type */
		mac_addr = new_fltr.l_data.mac.mac_addr;
		if (pkt_type == BCAST_FLTR) {
			ice_memset(mac_addr, 0xff, ETH_ALEN, ICE_NONDMA_MEM);
		} else if (pkt_type == MCAST_FLTR ||
			   pkt_type == UCAST_FLTR) {
			/* Use the dummy ether header DA */
			ice_memcpy(mac_addr, dummy_eth_header, ETH_ALEN,
				   ICE_NONDMA_TO_NONDMA);
			if (pkt_type == MCAST_FLTR)
				mac_addr[0] |= 0x1;	/* Set multicast bit */
		}

		/* Need to reset this to zero for all iterations */
		new_fltr.flag = 0;
		if (is_tx_fltr) {
			new_fltr.flag |= ICE_FLTR_TX;
			new_fltr.src = hw_vsi_id;
		} else {
			new_fltr.flag |= ICE_FLTR_RX;
			new_fltr.src = lport;
		}

		new_fltr.fltr_act = ICE_FWD_TO_VSI;
		new_fltr.vsi_handle = vsi_handle;
		new_fltr.fwd_id.hw_vsi_id = hw_vsi_id;
		f_list_entry.fltr_info = new_fltr;
		recp_list = &sw->recp_list[recipe_id];

		status = ice_add_rule_internal(hw, recp_list, lport,
					       &f_list_entry);
		if (status != ICE_SUCCESS)
			goto set_promisc_exit;
	}

set_promisc_exit:
	return status;
}

/**
 * ice_set_vsi_promisc - set given VSI to given promiscuous mode(s)
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: pointer to mask of promiscuous config bits
 * @vid: VLAN ID to set VLAN promiscuous
 */
enum ice_status
ice_set_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
		    ice_bitmap_t *promisc_mask, u16 vid)
{
	if (!hw || !promisc_mask)
		return ICE_ERR_PARAM;

	return _ice_set_vsi_promisc(hw, vsi_handle, promisc_mask, vid,
				    hw->port_info->lport,
				    hw->switch_info);
}

/**
 * _ice_set_vlan_vsi_promisc
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: pointer to mask of promiscuous config bits
 * @rm_vlan_promisc: Clear VLANs VSI promisc mode
 * @lport: logical port number to configure promisc mode
 * @sw: pointer to switch info struct for which function add rule
 *
 * Configure VSI with all associated VLANs to given promiscuous mode(s)
 */
static enum ice_status
_ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
			  ice_bitmap_t *promisc_mask, bool rm_vlan_promisc,
			  u8 lport, struct ice_switch_info *sw)
{
	struct ice_fltr_list_entry *list_itr, *tmp;
	struct LIST_HEAD_TYPE vsi_list_head;
	struct LIST_HEAD_TYPE *vlan_head;
	struct ice_lock *vlan_lock; /* Lock to protect filter rule list */
	enum ice_status status;
	u16 vlan_id;

	INIT_LIST_HEAD(&vsi_list_head);
	vlan_lock = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rule_lock;
	vlan_head = &sw->recp_list[ICE_SW_LKUP_VLAN].filt_rules;
	ice_acquire_lock(vlan_lock);
	status = ice_add_to_vsi_fltr_list(hw, vsi_handle, vlan_head,
					  &vsi_list_head);
	ice_release_lock(vlan_lock);
	if (status)
		goto free_fltr_list;

	LIST_FOR_EACH_ENTRY(list_itr, &vsi_list_head, ice_fltr_list_entry,
			    list_entry) {
		/* Avoid enabling or disabling vlan zero twice when in double
		 * vlan mode
		 */
		if (ice_is_dvm_ena(hw) &&
		    list_itr->fltr_info.l_data.vlan.tpid == 0)
			continue;

		vlan_id = list_itr->fltr_info.l_data.vlan.vlan_id;
		if (rm_vlan_promisc)
			status =  _ice_clear_vsi_promisc(hw, vsi_handle,
							 promisc_mask,
							 vlan_id, sw);
		else
			status =  _ice_set_vsi_promisc(hw, vsi_handle,
						       promisc_mask, vlan_id,
						       lport, sw);
		if (status && status != ICE_ERR_ALREADY_EXISTS)
			break;
	}

free_fltr_list:
	LIST_FOR_EACH_ENTRY_SAFE(list_itr, tmp, &vsi_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&list_itr->list_entry);
		ice_free(hw, list_itr);
	}
	return status;
}

/**
 * ice_set_vlan_vsi_promisc
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to configure
 * @promisc_mask: mask of promiscuous config bits
 * @rm_vlan_promisc: Clear VLANs VSI promisc mode
 *
 * Configure VSI with all associated VLANs to given promiscuous mode(s)
 */
enum ice_status
ice_set_vlan_vsi_promisc(struct ice_hw *hw, u16 vsi_handle,
			 ice_bitmap_t *promisc_mask, bool rm_vlan_promisc)
{
	if (!hw || !promisc_mask)
		return ICE_ERR_PARAM;

	return _ice_set_vlan_vsi_promisc(hw, vsi_handle, promisc_mask,
					 rm_vlan_promisc, hw->port_info->lport,
					 hw->switch_info);
}

/**
 * ice_remove_vsi_lkup_fltr - Remove lookup type filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @recp_list: recipe list from which function remove fltr
 * @lkup: switch rule filter lookup type
 */
static void
ice_remove_vsi_lkup_fltr(struct ice_hw *hw, u16 vsi_handle,
			 struct ice_sw_recipe *recp_list,
			 enum ice_sw_lkup_type lkup)
{
	struct ice_fltr_list_entry *fm_entry;
	struct LIST_HEAD_TYPE remove_list_head;
	struct LIST_HEAD_TYPE *rule_head;
	struct ice_fltr_list_entry *tmp;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status status;

	INIT_LIST_HEAD(&remove_list_head);
	rule_lock = &recp_list[lkup].filt_rule_lock;
	rule_head = &recp_list[lkup].filt_rules;
	ice_acquire_lock(rule_lock);
	status = ice_add_to_vsi_fltr_list(hw, vsi_handle, rule_head,
					  &remove_list_head);
	ice_release_lock(rule_lock);
	if (status)
		goto free_fltr_list;

	switch (lkup) {
	case ICE_SW_LKUP_MAC:
		ice_remove_mac_rule(hw, &remove_list_head, &recp_list[lkup]);
		break;
	case ICE_SW_LKUP_VLAN:
		ice_remove_vlan_rule(hw, &remove_list_head, &recp_list[lkup]);
		break;
	case ICE_SW_LKUP_PROMISC:
	case ICE_SW_LKUP_PROMISC_VLAN:
		ice_remove_promisc(hw, (u8)lkup, &remove_list_head);
		break;
	case ICE_SW_LKUP_MAC_VLAN:
		ice_debug(hw, ICE_DBG_SW, "MAC VLAN look up is not supported yet\n");
		break;
	case ICE_SW_LKUP_ETHERTYPE:
	case ICE_SW_LKUP_ETHERTYPE_MAC:
		ice_remove_eth_mac(hw, &remove_list_head);
		break;
	case ICE_SW_LKUP_DFLT:
		ice_debug(hw, ICE_DBG_SW, "Remove filters for this lookup type hasn't been implemented yet\n");
		break;
	case ICE_SW_LKUP_LAST:
		ice_debug(hw, ICE_DBG_SW, "Unsupported lookup type\n");
		break;
	}

free_fltr_list:
	LIST_FOR_EACH_ENTRY_SAFE(fm_entry, tmp, &remove_list_head,
				 ice_fltr_list_entry, list_entry) {
		LIST_DEL(&fm_entry->list_entry);
		ice_free(hw, fm_entry);
	}
}

/**
 * ice_remove_vsi_fltr_rule - Remove all filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 * @sw: pointer to switch info struct
 */
static void
ice_remove_vsi_fltr_rule(struct ice_hw *hw, u16 vsi_handle,
			 struct ice_switch_info *sw)
{
	ice_debug(hw, ICE_DBG_TRACE, "%s\n", __func__);

	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_MAC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_MAC_VLAN);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_PROMISC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_VLAN);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_DFLT);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_ETHERTYPE);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_ETHERTYPE_MAC);
	ice_remove_vsi_lkup_fltr(hw, vsi_handle,
				 sw->recp_list, ICE_SW_LKUP_PROMISC_VLAN);
}

/**
 * ice_remove_vsi_fltr - Remove all filters for a VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: VSI handle to remove filters from
 */
void ice_remove_vsi_fltr(struct ice_hw *hw, u16 vsi_handle)
{
	ice_remove_vsi_fltr_rule(hw, vsi_handle, hw->switch_info);
}

/**
 * ice_alloc_res_cntr - allocating resource counter
 * @hw: pointer to the hardware structure
 * @type: type of resource
 * @alloc_shared: if set it is shared else dedicated
 * @num_items: number of entries requested for FD resource type
 * @counter_id: counter index returned by AQ call
 */
static enum ice_status
ice_alloc_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		   u16 *counter_id)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	enum ice_status status;
	u16 buf_len;

	/* Allocate resource */
	buf_len = ice_struct_size(buf, elem, 1);
	buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->num_elems = CPU_TO_LE16(num_items);
	buf->res_type = CPU_TO_LE16(((type << ICE_AQC_RES_TYPE_S) &
				      ICE_AQC_RES_TYPE_M) | alloc_shared);

	status = ice_aq_alloc_free_res(hw, 1, buf, buf_len,
				       ice_aqc_opc_alloc_res, NULL);
	if (status)
		goto exit;

	*counter_id = LE16_TO_CPU(buf->elem[0].e.sw_resp);

exit:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_free_res_cntr - free resource counter
 * @hw: pointer to the hardware structure
 * @type: type of resource
 * @alloc_shared: if set it is shared else dedicated
 * @num_items: number of entries to be freed for FD resource type
 * @counter_id: counter ID resource which needs to be freed
 */
static enum ice_status
ice_free_res_cntr(struct ice_hw *hw, u8 type, u8 alloc_shared, u16 num_items,
		  u16 counter_id)
{
	struct ice_aqc_alloc_free_res_elem *buf;
	enum ice_status status;
	u16 buf_len;

	/* Free resource */
	buf_len = ice_struct_size(buf, elem, 1);
	buf = (struct ice_aqc_alloc_free_res_elem *)ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->num_elems = CPU_TO_LE16(num_items);
	buf->res_type = CPU_TO_LE16(((type << ICE_AQC_RES_TYPE_S) &
				      ICE_AQC_RES_TYPE_M) | alloc_shared);
	buf->elem[0].e.sw_resp = CPU_TO_LE16(counter_id);

	status = ice_aq_alloc_free_res(hw, 1, buf, buf_len,
				       ice_aqc_opc_free_res, NULL);
	if (status)
		ice_debug(hw, ICE_DBG_SW, "counter resource could not be freed\n");

	ice_free(hw, buf);
	return status;
}

/**
 * ice_alloc_vlan_res_counter - obtain counter resource for VLAN type
 * @hw: pointer to the hardware structure
 * @counter_id: returns counter index
 */
enum ice_status ice_alloc_vlan_res_counter(struct ice_hw *hw, u16 *counter_id)
{
	return ice_alloc_res_cntr(hw, ICE_AQC_RES_TYPE_VLAN_COUNTER,
				  ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1,
				  counter_id);
}

/**
 * ice_free_vlan_res_counter - Free counter resource for VLAN type
 * @hw: pointer to the hardware structure
 * @counter_id: counter index to be freed
 */
enum ice_status ice_free_vlan_res_counter(struct ice_hw *hw, u16 counter_id)
{
	return ice_free_res_cntr(hw, ICE_AQC_RES_TYPE_VLAN_COUNTER,
				 ICE_AQC_RES_TYPE_FLAG_DEDICATED, 1,
				 counter_id);
}

/**
 * ice_add_mac_with_sw_marker - add filter with sw marker
 * @hw: pointer to the hardware structure
 * @f_info: filter info structure containing the MAC filter information
 * @sw_marker: sw marker to tag the Rx descriptor with
 */
enum ice_status
ice_add_mac_with_sw_marker(struct ice_hw *hw, struct ice_fltr_info *f_info,
			   u16 sw_marker)
{
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_fltr_list_entry fl_info;
	struct ice_sw_recipe *recp_list;
	struct LIST_HEAD_TYPE l_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status ret;
	bool entry_exists;
	u16 lg_act_id;

	if (f_info->fltr_act != ICE_FWD_TO_VSI)
		return ICE_ERR_PARAM;

	if (f_info->lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	if (sw_marker == ICE_INVAL_SW_MARKER_ID)
		return ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(hw, f_info->vsi_handle))
		return ICE_ERR_PARAM;
	f_info->fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, f_info->vsi_handle);

	/* Add filter if it doesn't exist so then the adding of large
	 * action always results in update
	 */

	INIT_LIST_HEAD(&l_head);
	fl_info.fltr_info = *f_info;
	LIST_ADD(&fl_info.list_entry, &l_head);

	entry_exists = false;
	ret = ice_add_mac_rule(hw, &l_head, hw->switch_info,
			       hw->port_info->lport);
	if (ret == ICE_ERR_ALREADY_EXISTS)
		entry_exists = true;
	else if (ret)
		return ret;

	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];
	rule_lock = &recp_list->filt_rule_lock;
	ice_acquire_lock(rule_lock);
	/* Get the book keeping entry for the filter */
	m_entry = ice_find_rule_entry(&recp_list->filt_rules, f_info);
	if (!m_entry)
		goto exit_error;

	/* If counter action was enabled for this rule then don't enable
	 * sw marker large action
	 */
	if (m_entry->counter_index != ICE_INVAL_COUNTER_ID) {
		ret = ICE_ERR_PARAM;
		goto exit_error;
	}

	/* if same marker was added before */
	if (m_entry->sw_marker_id == sw_marker) {
		ret = ICE_ERR_ALREADY_EXISTS;
		goto exit_error;
	}

	/* Allocate a hardware table entry to hold large act. Three actions
	 * for marker based large action
	 */
	ret = ice_alloc_res_lg_act(hw, &lg_act_id, 3);
	if (ret)
		goto exit_error;

	if (lg_act_id == ICE_INVAL_LG_ACT_INDEX)
		goto exit_error;

	/* Update the switch rule to add the marker action */
	ret = ice_add_marker_act(hw, m_entry, sw_marker, lg_act_id);
	if (!ret) {
		ice_release_lock(rule_lock);
		return ret;
	}

exit_error:
	ice_release_lock(rule_lock);
	/* only remove entry if it did not exist previously */
	if (!entry_exists)
		ret = ice_remove_mac(hw, &l_head);

	return ret;
}

/**
 * ice_add_mac_with_counter - add filter with counter enabled
 * @hw: pointer to the hardware structure
 * @f_info: pointer to filter info structure containing the MAC filter
 *          information
 */
enum ice_status
ice_add_mac_with_counter(struct ice_hw *hw, struct ice_fltr_info *f_info)
{
	struct ice_fltr_mgmt_list_entry *m_entry;
	struct ice_fltr_list_entry fl_info;
	struct ice_sw_recipe *recp_list;
	struct LIST_HEAD_TYPE l_head;
	struct ice_lock *rule_lock;	/* Lock to protect filter rule list */
	enum ice_status ret;
	bool entry_exist;
	u16 counter_id;
	u16 lg_act_id;

	if (f_info->fltr_act != ICE_FWD_TO_VSI)
		return ICE_ERR_PARAM;

	if (f_info->lkup_type != ICE_SW_LKUP_MAC)
		return ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(hw, f_info->vsi_handle))
		return ICE_ERR_PARAM;
	f_info->fwd_id.hw_vsi_id = ice_get_hw_vsi_num(hw, f_info->vsi_handle);
	recp_list = &hw->switch_info->recp_list[ICE_SW_LKUP_MAC];

	entry_exist = false;

	rule_lock = &recp_list->filt_rule_lock;

	/* Add filter if it doesn't exist so then the adding of large
	 * action always results in update
	 */
	INIT_LIST_HEAD(&l_head);

	fl_info.fltr_info = *f_info;
	LIST_ADD(&fl_info.list_entry, &l_head);

	ret = ice_add_mac_rule(hw, &l_head, hw->switch_info,
			       hw->port_info->lport);
	if (ret == ICE_ERR_ALREADY_EXISTS)
		entry_exist = true;
	else if (ret)
		return ret;

	ice_acquire_lock(rule_lock);
	m_entry = ice_find_rule_entry(&recp_list->filt_rules, f_info);
	if (!m_entry) {
		ret = ICE_ERR_BAD_PTR;
		goto exit_error;
	}

	/* Don't enable counter for a filter for which sw marker was enabled */
	if (m_entry->sw_marker_id != ICE_INVAL_SW_MARKER_ID) {
		ret = ICE_ERR_PARAM;
		goto exit_error;
	}

	/* If a counter was already enabled then don't need to add again */
	if (m_entry->counter_index != ICE_INVAL_COUNTER_ID) {
		ret = ICE_ERR_ALREADY_EXISTS;
		goto exit_error;
	}

	/* Allocate a hardware table entry to VLAN counter */
	ret = ice_alloc_vlan_res_counter(hw, &counter_id);
	if (ret)
		goto exit_error;

	/* Allocate a hardware table entry to hold large act. Two actions for
	 * counter based large action
	 */
	ret = ice_alloc_res_lg_act(hw, &lg_act_id, 2);
	if (ret)
		goto exit_error;

	if (lg_act_id == ICE_INVAL_LG_ACT_INDEX)
		goto exit_error;

	/* Update the switch rule to add the counter action */
	ret = ice_add_counter_act(hw, m_entry, counter_id, lg_act_id);
	if (!ret) {
		ice_release_lock(rule_lock);
		return ret;
	}

exit_error:
	ice_release_lock(rule_lock);
	/* only remove entry if it did not exist previously */
	if (!entry_exist)
		ret = ice_remove_mac(hw, &l_head);

	return ret;
}

/**
 * ice_replay_fltr - Replay all the filters stored by a specific list head
 * @hw: pointer to the hardware structure
 * @list_head: list for which filters needs to be replayed
 * @recp_id: Recipe ID for which rules need to be replayed
 */
static enum ice_status
ice_replay_fltr(struct ice_hw *hw, u8 recp_id, struct LIST_HEAD_TYPE *list_head)
{
	struct ice_fltr_mgmt_list_entry *itr;
	enum ice_status status = ICE_SUCCESS;
	struct ice_sw_recipe *recp_list;
	u8 lport = hw->port_info->lport;
	struct LIST_HEAD_TYPE l_head;

	if (LIST_EMPTY(list_head))
		return status;

	recp_list = &hw->switch_info->recp_list[recp_id];
	/* Move entries from the given list_head to a temporary l_head so that
	 * they can be replayed. Otherwise when trying to re-add the same
	 * filter, the function will return already exists
	 */
	LIST_REPLACE_INIT(list_head, &l_head);

	/* Mark the given list_head empty by reinitializing it so filters
	 * could be added again by *handler
	 */
	LIST_FOR_EACH_ENTRY(itr, &l_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_list_entry f_entry;
		u16 vsi_handle;

		f_entry.fltr_info = itr->fltr_info;
		if (itr->vsi_count < 2 && recp_id != ICE_SW_LKUP_VLAN) {
			status = ice_add_rule_internal(hw, recp_list, lport,
						       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
			continue;
		}

		/* Add a filter per VSI separately */
		ice_for_each_set_bit(vsi_handle, itr->vsi_list_info->vsi_map,
				     ICE_MAX_VSI) {
			if (!ice_is_vsi_valid(hw, vsi_handle))
				break;

			ice_clear_bit(vsi_handle, itr->vsi_list_info->vsi_map);
			f_entry.fltr_info.vsi_handle = vsi_handle;
			f_entry.fltr_info.fwd_id.hw_vsi_id =
				ice_get_hw_vsi_num(hw, vsi_handle);
			f_entry.fltr_info.fltr_act = ICE_FWD_TO_VSI;
			if (recp_id == ICE_SW_LKUP_VLAN)
				status = ice_add_vlan_internal(hw, recp_list,
							       &f_entry);
			else
				status = ice_add_rule_internal(hw, recp_list,
							       lport,
							       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
		}
	}
end:
	/* Clear the filter management list */
	ice_rem_sw_rule_info(hw, &l_head);
	return status;
}

/**
 * ice_replay_all_fltr - replay all filters stored in bookkeeping lists
 * @hw: pointer to the hardware structure
 *
 * NOTE: This function does not clean up partially added filters on error.
 * It is up to caller of the function to issue a reset or fail early.
 */
enum ice_status ice_replay_all_fltr(struct ice_hw *hw)
{
	struct ice_switch_info *sw = hw->switch_info;
	enum ice_status status = ICE_SUCCESS;
	u8 i;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *head = &sw->recp_list[i].filt_rules;

		status = ice_replay_fltr(hw, i, head);
		if (status != ICE_SUCCESS)
			return status;
	}
	return status;
}

/**
 * ice_replay_vsi_fltr - Replay filters for requested VSI
 * @hw: pointer to the hardware structure
 * @pi: pointer to port information structure
 * @sw: pointer to switch info struct for which function replays filters
 * @vsi_handle: driver VSI handle
 * @recp_id: Recipe ID for which rules need to be replayed
 * @list_head: list for which filters need to be replayed
 *
 * Replays the filter of recipe recp_id for a VSI represented via vsi_handle.
 * It is required to pass valid VSI handle.
 */
static enum ice_status
ice_replay_vsi_fltr(struct ice_hw *hw, struct ice_port_info *pi,
		    struct ice_switch_info *sw, u16 vsi_handle, u8 recp_id,
		    struct LIST_HEAD_TYPE *list_head)
{
	struct ice_fltr_mgmt_list_entry *itr;
	enum ice_status status = ICE_SUCCESS;
	struct ice_sw_recipe *recp_list;
	u16 hw_vsi_id;

	if (LIST_EMPTY(list_head))
		return status;
	recp_list = &sw->recp_list[recp_id];
	hw_vsi_id = ice_get_hw_vsi_num(hw, vsi_handle);

	LIST_FOR_EACH_ENTRY(itr, list_head, ice_fltr_mgmt_list_entry,
			    list_entry) {
		struct ice_fltr_list_entry f_entry;

		f_entry.fltr_info = itr->fltr_info;
		if (itr->vsi_count < 2 && recp_id != ICE_SW_LKUP_VLAN &&
		    itr->fltr_info.vsi_handle == vsi_handle) {
			/* update the src in case it is VSI num */
			if (f_entry.fltr_info.src_id == ICE_SRC_ID_VSI)
				f_entry.fltr_info.src = hw_vsi_id;
			status = ice_add_rule_internal(hw, recp_list,
						       pi->lport,
						       &f_entry);
			if (status != ICE_SUCCESS)
				goto end;
			continue;
		}
		if (!itr->vsi_list_info ||
		    !ice_is_bit_set(itr->vsi_list_info->vsi_map, vsi_handle))
			continue;
		/* Clearing it so that the logic can add it back */
		ice_clear_bit(vsi_handle, itr->vsi_list_info->vsi_map);
		f_entry.fltr_info.vsi_handle = vsi_handle;
		f_entry.fltr_info.fltr_act = ICE_FWD_TO_VSI;
		/* update the src in case it is VSI num */
		if (f_entry.fltr_info.src_id == ICE_SRC_ID_VSI)
			f_entry.fltr_info.src = hw_vsi_id;
		if (recp_id == ICE_SW_LKUP_VLAN)
			status = ice_add_vlan_internal(hw, recp_list, &f_entry);
		else
			status = ice_add_rule_internal(hw, recp_list,
						       pi->lport,
						       &f_entry);
		if (status != ICE_SUCCESS)
			goto end;
	}
end:
	return status;
}

/**
 * ice_replay_vsi_all_fltr - replay all filters stored in bookkeeping lists
 * @hw: pointer to the hardware structure
 * @pi: pointer to port information structure
 * @vsi_handle: driver VSI handle
 *
 * Replays filters for requested VSI via vsi_handle.
 */
enum ice_status
ice_replay_vsi_all_fltr(struct ice_hw *hw, struct ice_port_info *pi,
			u16 vsi_handle)
{
	struct ice_switch_info *sw = NULL;
	enum ice_status status = ICE_SUCCESS;
	u8 i;

	sw = hw->switch_info;

	/* Update the recipes that were created */
	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		struct LIST_HEAD_TYPE *head;

		head = &sw->recp_list[i].filt_replay_rules;
		if (!sw->recp_list[i].adv_rule)
			status = ice_replay_vsi_fltr(hw, pi, sw, vsi_handle, i,
						     head);
		if (status != ICE_SUCCESS)
			return status;
	}

	return ICE_SUCCESS;
}

/**
 * ice_rm_sw_replay_rule_info - helper function to delete filter replay rules
 * @hw: pointer to the HW struct
 * @sw: pointer to switch info struct for which function removes filters
 *
 * Deletes the filter replay rules for given switch
 */
void ice_rm_sw_replay_rule_info(struct ice_hw *hw, struct ice_switch_info *sw)
{
	u8 i;

	if (!sw)
		return;

	for (i = 0; i < ICE_MAX_NUM_RECIPES; i++) {
		if (!LIST_EMPTY(&sw->recp_list[i].filt_replay_rules)) {
			struct LIST_HEAD_TYPE *l_head;

			l_head = &sw->recp_list[i].filt_replay_rules;
			if (!sw->recp_list[i].adv_rule)
				ice_rem_sw_rule_info(hw, l_head);
		}
	}
}

/**
 * ice_rm_all_sw_replay_rule_info - deletes filter replay rules
 * @hw: pointer to the HW struct
 *
 * Deletes the filter replay rules.
 */
void ice_rm_all_sw_replay_rule_info(struct ice_hw *hw)
{
	ice_rm_sw_replay_rule_info(hw, hw->switch_info);
}

