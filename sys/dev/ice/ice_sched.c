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

#include "ice_sched.h"

/**
 * ice_sched_add_root_node - Insert the Tx scheduler root node in SW DB
 * @pi: port information structure
 * @info: Scheduler element information from firmware
 *
 * This function inserts the root node of the scheduling tree topology
 * to the SW DB.
 */
static int
ice_sched_add_root_node(struct ice_port_info *pi,
			struct ice_aqc_txsched_elem_data *info)
{
	struct ice_sched_node *root;
	struct ice_hw *hw;

	if (!pi)
		return ICE_ERR_PARAM;

	hw = pi->hw;

	root = (struct ice_sched_node *)ice_malloc(hw, sizeof(*root));
	if (!root)
		return ICE_ERR_NO_MEMORY;

	root->children = (struct ice_sched_node **)
		ice_calloc(hw, hw->max_children[0], sizeof(*root->children));
	if (!root->children) {
		ice_free(hw, root);
		return ICE_ERR_NO_MEMORY;
	}

	ice_memcpy(&root->info, info, sizeof(*info), ICE_NONDMA_TO_NONDMA);
	pi->root = root;
	return 0;
}

/**
 * ice_sched_find_node_by_teid - Find the Tx scheduler node in SW DB
 * @start_node: pointer to the starting ice_sched_node struct in a sub-tree
 * @teid: node TEID to search
 *
 * This function searches for a node matching the TEID in the scheduling tree
 * from the SW DB. The search is recursive and is restricted by the number of
 * layers it has searched through; stopping at the max supported layer.
 *
 * This function needs to be called when holding the port_info->sched_lock
 */
struct ice_sched_node *
ice_sched_find_node_by_teid(struct ice_sched_node *start_node, u32 teid)
{
	u16 i;

	if (!start_node)
		return NULL;

	/* The TEID is same as that of the start_node */
	if (ICE_TXSCHED_GET_NODE_TEID(start_node) == teid)
		return start_node;

	/* The node has no children or is at the max layer */
	if (!start_node->num_children ||
	    start_node->tx_sched_layer >= ICE_AQC_TOPO_MAX_LEVEL_NUM ||
	    start_node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF)
		return NULL;

	/* Check if TEID matches to any of the children nodes */
	for (i = 0; i < start_node->num_children; i++)
		if (ICE_TXSCHED_GET_NODE_TEID(start_node->children[i]) == teid)
			return start_node->children[i];

	/* Search within each child's sub-tree */
	for (i = 0; i < start_node->num_children; i++) {
		struct ice_sched_node *tmp;

		tmp = ice_sched_find_node_by_teid(start_node->children[i],
						  teid);
		if (tmp)
			return tmp;
	}

	return NULL;
}

/**
 * ice_aqc_send_sched_elem_cmd - send scheduling elements cmd
 * @hw: pointer to the HW struct
 * @cmd_opc: cmd opcode
 * @elems_req: number of elements to request
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @elems_resp: returns total number of elements response
 * @cd: pointer to command details structure or NULL
 *
 * This function sends a scheduling elements cmd (cmd_opc)
 */
static int
ice_aqc_send_sched_elem_cmd(struct ice_hw *hw, enum ice_adminq_opc cmd_opc,
			    u16 elems_req, void *buf, u16 buf_size,
			    u16 *elems_resp, struct ice_sq_cd *cd)
{
	struct ice_aqc_sched_elem_cmd *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.sched_elem_cmd;
	ice_fill_dflt_direct_cmd_desc(&desc, cmd_opc);
	cmd->num_elem_req = CPU_TO_LE16(elems_req);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && elems_resp)
		*elems_resp = LE16_TO_CPU(cmd->num_elem_resp);

	return status;
}

/**
 * ice_aq_query_sched_elems - query scheduler elements
 * @hw: pointer to the HW struct
 * @elems_req: number of elements to query
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @elems_ret: returns total number of elements returned
 * @cd: pointer to command details structure or NULL
 *
 * Query scheduling elements (0x0404)
 */
int
ice_aq_query_sched_elems(struct ice_hw *hw, u16 elems_req,
			 struct ice_aqc_txsched_elem_data *buf, u16 buf_size,
			 u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_get_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

/**
 * ice_sched_add_node - Insert the Tx scheduler node in SW DB
 * @pi: port information structure
 * @layer: Scheduler layer of the node
 * @info: Scheduler element information from firmware
 * @prealloc_node: preallocated ice_sched_node struct for SW DB
 *
 * This function inserts a scheduler node to the SW DB.
 */
int
ice_sched_add_node(struct ice_port_info *pi, u8 layer,
		   struct ice_aqc_txsched_elem_data *info,
		   struct ice_sched_node *prealloc_node)
{
	struct ice_aqc_txsched_elem_data elem;
	struct ice_sched_node *parent;
	struct ice_sched_node *node;
	struct ice_hw *hw;
	int status;

	if (!pi)
		return ICE_ERR_PARAM;

	hw = pi->hw;

	/* A valid parent node should be there */
	parent = ice_sched_find_node_by_teid(pi->root,
					     LE32_TO_CPU(info->parent_teid));
	if (!parent) {
		ice_debug(hw, ICE_DBG_SCHED, "Parent Node not found for parent_teid=0x%x\n",
			  LE32_TO_CPU(info->parent_teid));
		return ICE_ERR_PARAM;
	}

	/* query the current node information from FW before adding it
	 * to the SW DB
	 */
	status = ice_sched_query_elem(hw, LE32_TO_CPU(info->node_teid), &elem);
	if (status)
		return status;

	if (prealloc_node)
		node = prealloc_node;
	else
		node = (struct ice_sched_node *)ice_malloc(hw, sizeof(*node));
	if (!node)
		return ICE_ERR_NO_MEMORY;
	if (hw->max_children[layer]) {
		node->children = (struct ice_sched_node **)
			ice_calloc(hw, hw->max_children[layer],
				   sizeof(*node->children));
		if (!node->children) {
			ice_free(hw, node);
			return ICE_ERR_NO_MEMORY;
		}
	}

	node->in_use = true;
	node->parent = parent;
	node->tx_sched_layer = layer;
	parent->children[parent->num_children++] = node;
	node->info = elem;
	return 0;
}

/**
 * ice_aq_delete_sched_elems - delete scheduler elements
 * @hw: pointer to the HW struct
 * @grps_req: number of groups to delete
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @grps_del: returns total number of elements deleted
 * @cd: pointer to command details structure or NULL
 *
 * Delete scheduling elements (0x040F)
 */
static int
ice_aq_delete_sched_elems(struct ice_hw *hw, u16 grps_req,
			  struct ice_aqc_delete_elem *buf, u16 buf_size,
			  u16 *grps_del, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_delete_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_del, cd);
}

/**
 * ice_sched_remove_elems - remove nodes from HW
 * @hw: pointer to the HW struct
 * @parent: pointer to the parent node
 * @num_nodes: number of nodes
 * @node_teids: array of node teids to be deleted
 *
 * This function remove nodes from HW
 */
static int
ice_sched_remove_elems(struct ice_hw *hw, struct ice_sched_node *parent,
		       u16 num_nodes, u32 *node_teids)
{
	struct ice_aqc_delete_elem *buf;
	u16 i, num_groups_removed = 0;
	u16 buf_size;
	int status;

	buf_size = ice_struct_size(buf, teid, num_nodes);
	buf = (struct ice_aqc_delete_elem *)ice_malloc(hw, buf_size);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->hdr.parent_teid = parent->info.node_teid;
	buf->hdr.num_elems = CPU_TO_LE16(num_nodes);
	for (i = 0; i < num_nodes; i++)
		buf->teid[i] = CPU_TO_LE32(node_teids[i]);

	status = ice_aq_delete_sched_elems(hw, 1, buf, buf_size,
					   &num_groups_removed, NULL);
	if (status || num_groups_removed != 1)
		ice_debug(hw, ICE_DBG_SCHED, "remove node failed FW error %d\n",
			  hw->adminq.sq_last_status);

	ice_free(hw, buf);
	return status;
}

/**
 * ice_sched_get_first_node - get the first node of the given layer
 * @pi: port information structure
 * @parent: pointer the base node of the subtree
 * @layer: layer number
 *
 * This function retrieves the first node of the given layer from the subtree
 */
static struct ice_sched_node *
ice_sched_get_first_node(struct ice_port_info *pi,
			 struct ice_sched_node *parent, u8 layer)
{
	return pi->sib_head[parent->tc_num][layer];
}

/**
 * ice_sched_get_tc_node - get pointer to TC node
 * @pi: port information structure
 * @tc: TC number
 *
 * This function returns the TC node pointer
 */
struct ice_sched_node *ice_sched_get_tc_node(struct ice_port_info *pi, u8 tc)
{
	u8 i;

	if (!pi || !pi->root)
		return NULL;
	for (i = 0; i < pi->root->num_children; i++)
		if (pi->root->children[i]->tc_num == tc)
			return pi->root->children[i];
	return NULL;
}

/**
 * ice_free_sched_node - Free a Tx scheduler node from SW DB
 * @pi: port information structure
 * @node: pointer to the ice_sched_node struct
 *
 * This function frees up a node from SW DB as well as from HW
 *
 * This function needs to be called with the port_info->sched_lock held
 */
void ice_free_sched_node(struct ice_port_info *pi, struct ice_sched_node *node)
{
	struct ice_sched_node *parent;
	struct ice_hw *hw = pi->hw;
	u8 i, j;

	/* Free the children before freeing up the parent node
	 * The parent array is updated below and that shifts the nodes
	 * in the array. So always pick the first child if num children > 0
	 */
	while (node->num_children)
		ice_free_sched_node(pi, node->children[0]);

	/* Leaf, TC and root nodes can't be deleted by SW */
	if (node->tx_sched_layer >= hw->sw_entry_point_layer &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_TC &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_ROOT_PORT &&
	    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_LEAF) {
		u32 teid = LE32_TO_CPU(node->info.node_teid);

		ice_sched_remove_elems(hw, node->parent, 1, &teid);
	}
	parent = node->parent;
	/* root has no parent */
	if (parent) {
		struct ice_sched_node *p;

		/* update the parent */
		for (i = 0; i < parent->num_children; i++)
			if (parent->children[i] == node) {
				for (j = i + 1; j < parent->num_children; j++)
					parent->children[j - 1] =
						parent->children[j];
				parent->num_children--;
				break;
			}

		p = ice_sched_get_first_node(pi, node, node->tx_sched_layer);
		while (p) {
			if (p->sibling == node) {
				p->sibling = node->sibling;
				break;
			}
			p = p->sibling;
		}

		/* update the sibling head if head is getting removed */
		if (pi->sib_head[node->tc_num][node->tx_sched_layer] == node)
			pi->sib_head[node->tc_num][node->tx_sched_layer] =
				node->sibling;
	}

	/* leaf nodes have no children */
	if (node->children)
		ice_free(hw, node->children);
	ice_free(hw, node);
}

/**
 * ice_aq_get_dflt_topo - gets default scheduler topology
 * @hw: pointer to the HW struct
 * @lport: logical port number
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @num_branches: returns total number of queue to port branches
 * @cd: pointer to command details structure or NULL
 *
 * Get default scheduler topology (0x400)
 */
static int
ice_aq_get_dflt_topo(struct ice_hw *hw, u8 lport,
		     struct ice_aqc_get_topo_elem *buf, u16 buf_size,
		     u8 *num_branches, struct ice_sq_cd *cd)
{
	struct ice_aqc_get_topo *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.get_topo;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_get_dflt_topo);
	cmd->port_num = lport;
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && num_branches)
		*num_branches = cmd->num_branches;

	return status;
}

/**
 * ice_aq_add_sched_elems - adds scheduling element
 * @hw: pointer to the HW struct
 * @grps_req: the number of groups that are requested to be added
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @grps_added: returns total number of groups added
 * @cd: pointer to command details structure or NULL
 *
 * Add scheduling elements (0x0401)
 */
static int
ice_aq_add_sched_elems(struct ice_hw *hw, u16 grps_req,
		       struct ice_aqc_add_elem *buf, u16 buf_size,
		       u16 *grps_added, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_add_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_added, cd);
}

/**
 * ice_aq_cfg_sched_elems - configures scheduler elements
 * @hw: pointer to the HW struct
 * @elems_req: number of elements to configure
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @elems_cfgd: returns total number of elements configured
 * @cd: pointer to command details structure or NULL
 *
 * Configure scheduling elements (0x0403)
 */
static int
ice_aq_cfg_sched_elems(struct ice_hw *hw, u16 elems_req,
		       struct ice_aqc_txsched_elem_data *buf, u16 buf_size,
		       u16 *elems_cfgd, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_cfg_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_cfgd, cd);
}

/**
 * ice_aq_move_sched_elems - move scheduler elements
 * @hw: pointer to the HW struct
 * @grps_req: number of groups to move
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @grps_movd: returns total number of groups moved
 * @cd: pointer to command details structure or NULL
 *
 * Move scheduling elements (0x0408)
 */
int
ice_aq_move_sched_elems(struct ice_hw *hw, u16 grps_req,
			struct ice_aqc_move_elem *buf, u16 buf_size,
			u16 *grps_movd, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_move_sched_elems,
					   grps_req, (void *)buf, buf_size,
					   grps_movd, cd);
}

/**
 * ice_aq_suspend_sched_elems - suspend scheduler elements
 * @hw: pointer to the HW struct
 * @elems_req: number of elements to suspend
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @elems_ret: returns total number of elements suspended
 * @cd: pointer to command details structure or NULL
 *
 * Suspend scheduling elements (0x0409)
 */
static int
ice_aq_suspend_sched_elems(struct ice_hw *hw, u16 elems_req, __le32 *buf,
			   u16 buf_size, u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_suspend_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

/**
 * ice_aq_resume_sched_elems - resume scheduler elements
 * @hw: pointer to the HW struct
 * @elems_req: number of elements to resume
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @elems_ret: returns total number of elements resumed
 * @cd: pointer to command details structure or NULL
 *
 * resume scheduling elements (0x040A)
 */
static int
ice_aq_resume_sched_elems(struct ice_hw *hw, u16 elems_req, __le32 *buf,
			  u16 buf_size, u16 *elems_ret, struct ice_sq_cd *cd)
{
	return ice_aqc_send_sched_elem_cmd(hw, ice_aqc_opc_resume_sched_elems,
					   elems_req, (void *)buf, buf_size,
					   elems_ret, cd);
}

/**
 * ice_aq_query_sched_res - query scheduler resource
 * @hw: pointer to the HW struct
 * @buf_size: buffer size in bytes
 * @buf: pointer to buffer
 * @cd: pointer to command details structure or NULL
 *
 * Query scheduler resource allocation (0x0412)
 */
static int
ice_aq_query_sched_res(struct ice_hw *hw, u16 buf_size,
		       struct ice_aqc_query_txsched_res_resp *buf,
		       struct ice_sq_cd *cd)
{
	struct ice_aq_desc desc;

	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_sched_res);
	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_sched_suspend_resume_elems - suspend or resume HW nodes
 * @hw: pointer to the HW struct
 * @num_nodes: number of nodes
 * @node_teids: array of node teids to be suspended or resumed
 * @suspend: true means suspend / false means resume
 *
 * This function suspends or resumes HW nodes
 */
static int
ice_sched_suspend_resume_elems(struct ice_hw *hw, u8 num_nodes, u32 *node_teids,
			       bool suspend)
{
	u16 i, buf_size, num_elem_ret = 0;
	__le32 *buf;
	int status;

	buf_size = sizeof(*buf) * num_nodes;
	buf = (__le32 *)ice_malloc(hw, buf_size);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < num_nodes; i++)
		buf[i] = CPU_TO_LE32(node_teids[i]);

	if (suspend)
		status = ice_aq_suspend_sched_elems(hw, num_nodes, buf,
						    buf_size, &num_elem_ret,
						    NULL);
	else
		status = ice_aq_resume_sched_elems(hw, num_nodes, buf,
						   buf_size, &num_elem_ret,
						   NULL);
	if (status || num_elem_ret != num_nodes)
		ice_debug(hw, ICE_DBG_SCHED, "suspend/resume failed\n");

	ice_free(hw, buf);
	return status;
}

/**
 * ice_alloc_lan_q_ctx - allocate LAN queue contexts for the given VSI and TC
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 * @tc: TC number
 * @new_numqs: number of queues
 */
static int
ice_alloc_lan_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 new_numqs)
{
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_q_ctx *q_ctx;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	/* allocate LAN queue contexts */
	if (!vsi_ctx->lan_q_ctx[tc]) {
		vsi_ctx->lan_q_ctx[tc] = (struct ice_q_ctx *)
			ice_calloc(hw, new_numqs, sizeof(*q_ctx));
		if (!vsi_ctx->lan_q_ctx[tc])
			return ICE_ERR_NO_MEMORY;
		vsi_ctx->num_lan_q_entries[tc] = new_numqs;
		return 0;
	}
	/* num queues are increased, update the queue contexts */
	if (new_numqs > vsi_ctx->num_lan_q_entries[tc]) {
		u16 prev_num = vsi_ctx->num_lan_q_entries[tc];

		q_ctx = (struct ice_q_ctx *)
			ice_calloc(hw, new_numqs, sizeof(*q_ctx));
		if (!q_ctx)
			return ICE_ERR_NO_MEMORY;
		ice_memcpy(q_ctx, vsi_ctx->lan_q_ctx[tc],
			   prev_num * sizeof(*q_ctx), ICE_DMA_TO_NONDMA);
		ice_free(hw, vsi_ctx->lan_q_ctx[tc]);
		vsi_ctx->lan_q_ctx[tc] = q_ctx;
		vsi_ctx->num_lan_q_entries[tc] = new_numqs;
	}
	return 0;
}

/**
 * ice_alloc_rdma_q_ctx - allocate RDMA queue contexts for the given VSI and TC
 * @hw: pointer to the HW struct
 * @vsi_handle: VSI handle
 * @tc: TC number
 * @new_numqs: number of queues
 */
static int
ice_alloc_rdma_q_ctx(struct ice_hw *hw, u16 vsi_handle, u8 tc, u16 new_numqs)
{
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_q_ctx *q_ctx;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	/* allocate RDMA queue contexts */
	if (!vsi_ctx->rdma_q_ctx[tc]) {
		vsi_ctx->rdma_q_ctx[tc] = (struct ice_q_ctx *)
			ice_calloc(hw, new_numqs, sizeof(*q_ctx));
		if (!vsi_ctx->rdma_q_ctx[tc])
			return ICE_ERR_NO_MEMORY;
		vsi_ctx->num_rdma_q_entries[tc] = new_numqs;
		return 0;
	}
	/* num queues are increased, update the queue contexts */
	if (new_numqs > vsi_ctx->num_rdma_q_entries[tc]) {
		u16 prev_num = vsi_ctx->num_rdma_q_entries[tc];

		q_ctx = (struct ice_q_ctx *)
			ice_calloc(hw, new_numqs, sizeof(*q_ctx));
		if (!q_ctx)
			return ICE_ERR_NO_MEMORY;
		ice_memcpy(q_ctx, vsi_ctx->rdma_q_ctx[tc],
			   prev_num * sizeof(*q_ctx), ICE_DMA_TO_NONDMA);
		ice_free(hw, vsi_ctx->rdma_q_ctx[tc]);
		vsi_ctx->rdma_q_ctx[tc] = q_ctx;
		vsi_ctx->num_rdma_q_entries[tc] = new_numqs;
	}
	return 0;
}

/**
 * ice_aq_rl_profile - performs a rate limiting task
 * @hw: pointer to the HW struct
 * @opcode: opcode for add, query, or remove profile(s)
 * @num_profiles: the number of profiles
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @num_processed: number of processed add or remove profile(s) to return
 * @cd: pointer to command details structure
 *
 * RL profile function to add, query, or remove profile(s)
 */
static int
ice_aq_rl_profile(struct ice_hw *hw, enum ice_adminq_opc opcode,
		  u16 num_profiles, struct ice_aqc_rl_profile_elem *buf,
		  u16 buf_size, u16 *num_processed, struct ice_sq_cd *cd)
{
	struct ice_aqc_rl_profile *cmd;
	struct ice_aq_desc desc;
	int status;

	cmd = &desc.params.rl_profile;

	ice_fill_dflt_direct_cmd_desc(&desc, opcode);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);
	cmd->num_profiles = CPU_TO_LE16(num_profiles);
	status = ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
	if (!status && num_processed)
		*num_processed = LE16_TO_CPU(cmd->num_processed);
	return status;
}

/**
 * ice_aq_add_rl_profile - adds rate limiting profile(s)
 * @hw: pointer to the HW struct
 * @num_profiles: the number of profile(s) to be add
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @num_profiles_added: total number of profiles added to return
 * @cd: pointer to command details structure
 *
 * Add RL profile (0x0410)
 */
static int
ice_aq_add_rl_profile(struct ice_hw *hw, u16 num_profiles,
		      struct ice_aqc_rl_profile_elem *buf, u16 buf_size,
		      u16 *num_profiles_added, struct ice_sq_cd *cd)
{
	return ice_aq_rl_profile(hw, ice_aqc_opc_add_rl_profiles, num_profiles,
				 buf, buf_size, num_profiles_added, cd);
}

/**
 * ice_aq_query_rl_profile - query rate limiting profile(s)
 * @hw: pointer to the HW struct
 * @num_profiles: the number of profile(s) to query
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure
 *
 * Query RL profile (0x0411)
 */
int
ice_aq_query_rl_profile(struct ice_hw *hw, u16 num_profiles,
			struct ice_aqc_rl_profile_elem *buf, u16 buf_size,
			struct ice_sq_cd *cd)
{
	return ice_aq_rl_profile(hw, ice_aqc_opc_query_rl_profiles,
				 num_profiles, buf, buf_size, NULL, cd);
}

/**
 * ice_aq_remove_rl_profile - removes RL profile(s)
 * @hw: pointer to the HW struct
 * @num_profiles: the number of profile(s) to remove
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @num_profiles_removed: total number of profiles removed to return
 * @cd: pointer to command details structure or NULL
 *
 * Remove RL profile (0x0415)
 */
static int
ice_aq_remove_rl_profile(struct ice_hw *hw, u16 num_profiles,
			 struct ice_aqc_rl_profile_elem *buf, u16 buf_size,
			 u16 *num_profiles_removed, struct ice_sq_cd *cd)
{
	return ice_aq_rl_profile(hw, ice_aqc_opc_remove_rl_profiles,
				 num_profiles, buf, buf_size,
				 num_profiles_removed, cd);
}

/**
 * ice_sched_del_rl_profile - remove RL profile
 * @hw: pointer to the HW struct
 * @rl_info: rate limit profile information
 *
 * If the profile ID is not referenced anymore, it removes profile ID with
 * its associated parameters from HW DB,and locally. The caller needs to
 * hold scheduler lock.
 */
static int
ice_sched_del_rl_profile(struct ice_hw *hw,
			 struct ice_aqc_rl_profile_info *rl_info)
{
	struct ice_aqc_rl_profile_elem *buf;
	u16 num_profiles_removed;
	u16 num_profiles = 1;
	int status;

	if (rl_info->prof_id_ref != 0)
		return ICE_ERR_IN_USE;

	/* Safe to remove profile ID */
	buf = &rl_info->profile;
	status = ice_aq_remove_rl_profile(hw, num_profiles, buf, sizeof(*buf),
					  &num_profiles_removed, NULL);
	if (status || num_profiles_removed != num_profiles)
		return ICE_ERR_CFG;

	/* Delete stale entry now */
	LIST_DEL(&rl_info->list_entry);
	ice_free(hw, rl_info);
	return status;
}

/**
 * ice_sched_clear_rl_prof - clears RL prof entries
 * @pi: port information structure
 *
 * This function removes all RL profile from HW as well as from SW DB.
 */
static void ice_sched_clear_rl_prof(struct ice_port_info *pi)
{
	u16 ln;
	struct ice_hw *hw = pi->hw;

	for (ln = 0; ln < hw->num_tx_sched_layers; ln++) {
		struct ice_aqc_rl_profile_info *rl_prof_elem;
		struct ice_aqc_rl_profile_info *rl_prof_tmp;

		LIST_FOR_EACH_ENTRY_SAFE(rl_prof_elem, rl_prof_tmp,
					 &hw->rl_prof_list[ln],
					 ice_aqc_rl_profile_info, list_entry) {
			int status;

			rl_prof_elem->prof_id_ref = 0;
			status = ice_sched_del_rl_profile(hw, rl_prof_elem);
			if (status) {
				ice_debug(hw, ICE_DBG_SCHED, "Remove rl profile failed\n");
				/* On error, free mem required */
				LIST_DEL(&rl_prof_elem->list_entry);
				ice_free(hw, rl_prof_elem);
			}
		}
	}
}

/**
 * ice_sched_clear_agg - clears the aggregator related information
 * @hw: pointer to the hardware structure
 *
 * This function removes aggregator list and free up aggregator related memory
 * previously allocated.
 */
void ice_sched_clear_agg(struct ice_hw *hw)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_sched_agg_info *atmp;

	LIST_FOR_EACH_ENTRY_SAFE(agg_info, atmp, &hw->agg_list,
				 ice_sched_agg_info,
				 list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;
		struct ice_sched_agg_vsi_info *vtmp;

		LIST_FOR_EACH_ENTRY_SAFE(agg_vsi_info, vtmp,
					 &agg_info->agg_vsi_list,
					 ice_sched_agg_vsi_info, list_entry) {
			LIST_DEL(&agg_vsi_info->list_entry);
			ice_free(hw, agg_vsi_info);
		}
		LIST_DEL(&agg_info->list_entry);
		ice_free(hw, agg_info);
	}
}

/**
 * ice_sched_clear_tx_topo - clears the scheduler tree nodes
 * @pi: port information structure
 *
 * This function removes all the nodes from HW as well as from SW DB.
 */
static void ice_sched_clear_tx_topo(struct ice_port_info *pi)
{
	if (!pi)
		return;
	/* remove RL profiles related lists */
	ice_sched_clear_rl_prof(pi);
	if (pi->root) {
		ice_free_sched_node(pi, pi->root);
		pi->root = NULL;
	}
}

/**
 * ice_sched_clear_port - clear the scheduler elements from SW DB for a port
 * @pi: port information structure
 *
 * Cleanup scheduling elements from SW DB
 */
void ice_sched_clear_port(struct ice_port_info *pi)
{
	if (!pi || pi->port_state != ICE_SCHED_PORT_STATE_READY)
		return;

	pi->port_state = ICE_SCHED_PORT_STATE_INIT;
	ice_acquire_lock(&pi->sched_lock);
	ice_sched_clear_tx_topo(pi);
	ice_release_lock(&pi->sched_lock);
	ice_destroy_lock(&pi->sched_lock);
}

/**
 * ice_sched_cleanup_all - cleanup scheduler elements from SW DB for all ports
 * @hw: pointer to the HW struct
 *
 * Cleanup scheduling elements from SW DB for all the ports
 */
void ice_sched_cleanup_all(struct ice_hw *hw)
{
	if (!hw)
		return;

	if (hw->layer_info) {
		ice_free(hw, hw->layer_info);
		hw->layer_info = NULL;
	}

	ice_sched_clear_port(hw->port_info);

	hw->num_tx_sched_layers = 0;
	hw->num_tx_sched_phys_layers = 0;
	hw->flattened_layers = 0;
	hw->max_cgds = 0;
}

/**
 * ice_aq_cfg_node_attr - configure nodes' per-cone flattening attributes
 * @hw: pointer to the HW struct
 * @num_nodes: the number of nodes whose attributes to configure
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure or NULL
 *
 * Configure Node Attributes (0x0417)
 */
int
ice_aq_cfg_node_attr(struct ice_hw *hw, u16 num_nodes,
		     struct ice_aqc_node_attr_elem *buf, u16 buf_size,
		     struct ice_sq_cd *cd)
{
	struct ice_aqc_node_attr *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.node_attr;
	ice_fill_dflt_direct_cmd_desc(&desc,
				      ice_aqc_opc_cfg_node_attr);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->num_entries = CPU_TO_LE16(num_nodes);
	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_aq_cfg_l2_node_cgd - configures L2 node to CGD mapping
 * @hw: pointer to the HW struct
 * @num_l2_nodes: the number of L2 nodes whose CGDs to configure
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure or NULL
 *
 * Configure L2 Node CGD (0x0414)
 */
int
ice_aq_cfg_l2_node_cgd(struct ice_hw *hw, u16 num_l2_nodes,
		       struct ice_aqc_cfg_l2_node_cgd_elem *buf,
		       u16 buf_size, struct ice_sq_cd *cd)
{
	struct ice_aqc_cfg_l2_node_cgd *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.cfg_l2_node_cgd;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_cfg_l2_node_cgd);
	desc.flags |= CPU_TO_LE16(ICE_AQ_FLAG_RD);

	cmd->num_l2_nodes = CPU_TO_LE16(num_l2_nodes);
	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_sched_add_elems - add nodes to HW and SW DB
 * @pi: port information structure
 * @tc_node: pointer to the branch node
 * @parent: pointer to the parent node
 * @layer: layer number to add nodes
 * @num_nodes: number of nodes
 * @num_nodes_added: pointer to num nodes added
 * @first_node_teid: if new nodes are added then return the TEID of first node
 * @prealloc_nodes: preallocated nodes struct for software DB
 *
 * This function add nodes to HW as well as to SW DB for a given layer
 */
int
ice_sched_add_elems(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		    struct ice_sched_node *parent, u8 layer, u16 num_nodes,
		    u16 *num_nodes_added, u32 *first_node_teid,
		    struct ice_sched_node **prealloc_nodes)
{
	struct ice_sched_node *prev, *new_node;
	struct ice_aqc_add_elem *buf;
	u16 i, num_groups_added = 0;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u16 buf_size;
	u32 teid;

	buf_size = ice_struct_size(buf, generic, num_nodes);
	buf = (struct ice_aqc_add_elem *)ice_malloc(hw, buf_size);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	buf->hdr.parent_teid = parent->info.node_teid;
	buf->hdr.num_elems = CPU_TO_LE16(num_nodes);
	for (i = 0; i < num_nodes; i++) {
		buf->generic[i].parent_teid = parent->info.node_teid;
		buf->generic[i].data.elem_type = ICE_AQC_ELEM_TYPE_SE_GENERIC;
		buf->generic[i].data.valid_sections =
			ICE_AQC_ELEM_VALID_GENERIC | ICE_AQC_ELEM_VALID_CIR |
			ICE_AQC_ELEM_VALID_EIR;
		buf->generic[i].data.generic = 0;
		buf->generic[i].data.cir_bw.bw_profile_idx =
			CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->generic[i].data.cir_bw.bw_alloc =
			CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);
		buf->generic[i].data.eir_bw.bw_profile_idx =
			CPU_TO_LE16(ICE_SCHED_DFLT_RL_PROF_ID);
		buf->generic[i].data.eir_bw.bw_alloc =
			CPU_TO_LE16(ICE_SCHED_DFLT_BW_WT);
	}

	status = ice_aq_add_sched_elems(hw, 1, buf, buf_size,
					&num_groups_added, NULL);
	if (status || num_groups_added != 1) {
		ice_debug(hw, ICE_DBG_SCHED, "add node failed FW Error %d\n",
			  hw->adminq.sq_last_status);
		ice_free(hw, buf);
		return ICE_ERR_CFG;
	}

	*num_nodes_added = num_nodes;
	/* add nodes to the SW DB */
	for (i = 0; i < num_nodes; i++) {
		if (prealloc_nodes)
			status = ice_sched_add_node(pi, layer, &buf->generic[i], prealloc_nodes[i]);
		else
			status = ice_sched_add_node(pi, layer, &buf->generic[i], NULL);

		if (status) {
			ice_debug(hw, ICE_DBG_SCHED, "add nodes in SW DB failed status =%d\n",
				  status);
			break;
		}

		teid = LE32_TO_CPU(buf->generic[i].node_teid);
		new_node = ice_sched_find_node_by_teid(parent, teid);
		if (!new_node) {
			ice_debug(hw, ICE_DBG_SCHED, "Node is missing for teid =%d\n", teid);
			break;
		}

		new_node->sibling = NULL;
		new_node->tc_num = tc_node->tc_num;

		/* add it to previous node sibling pointer */
		/* Note: siblings are not linked across branches */
		prev = ice_sched_get_first_node(pi, tc_node, layer);
		if (prev && prev != new_node) {
			while (prev->sibling)
				prev = prev->sibling;
			prev->sibling = new_node;
		}

		/* initialize the sibling head */
		if (!pi->sib_head[tc_node->tc_num][layer])
			pi->sib_head[tc_node->tc_num][layer] = new_node;

		if (i == 0)
			*first_node_teid = teid;
	}

	ice_free(hw, buf);
	return status;
}

/**
 * ice_sched_add_nodes_to_hw_layer - Add nodes to hw layer
 * @pi: port information structure
 * @tc_node: pointer to TC node
 * @parent: pointer to parent node
 * @layer: layer number to add nodes
 * @num_nodes: number of nodes to be added
 * @first_node_teid: pointer to the first node TEID
 * @num_nodes_added: pointer to number of nodes added
 *
 * Add nodes into specific hw layer.
 */
static int
ice_sched_add_nodes_to_hw_layer(struct ice_port_info *pi,
				struct ice_sched_node *tc_node,
				struct ice_sched_node *parent, u8 layer,
				u16 num_nodes, u32 *first_node_teid,
				u16 *num_nodes_added)
{
	u16 max_child_nodes;

	*num_nodes_added = 0;

	if (!num_nodes)
		return 0;

	if (!parent || layer < pi->hw->sw_entry_point_layer)
		return ICE_ERR_PARAM;

	/* max children per node per layer */
	max_child_nodes = pi->hw->max_children[parent->tx_sched_layer];

	/* current number of children + required nodes exceed max children */
	if ((parent->num_children + num_nodes) > max_child_nodes) {
		/* Fail if the parent is a TC node */
		if (parent == tc_node)
			return ICE_ERR_CFG;
		return ICE_ERR_MAX_LIMIT;
	}

	return ice_sched_add_elems(pi, tc_node, parent, layer, num_nodes,
				   num_nodes_added, first_node_teid, NULL);
}

/**
 * ice_sched_add_nodes_to_layer - Add nodes to a given layer
 * @pi: port information structure
 * @tc_node: pointer to TC node
 * @parent: pointer to parent node
 * @layer: layer number to add nodes
 * @num_nodes: number of nodes to be added
 * @first_node_teid: pointer to the first node TEID
 * @num_nodes_added: pointer to number of nodes added
 *
 * This function add nodes to a given layer.
 */
static int
ice_sched_add_nodes_to_layer(struct ice_port_info *pi,
			     struct ice_sched_node *tc_node,
			     struct ice_sched_node *parent, u8 layer,
			     u16 num_nodes, u32 *first_node_teid,
			     u16 *num_nodes_added)
{
	u32 *first_teid_ptr = first_node_teid;
	u16 new_num_nodes = num_nodes;
	int status = 0;
	u32 temp;

	*num_nodes_added = 0;
	while (*num_nodes_added < num_nodes) {
		u16 max_child_nodes, num_added = 0;

		status = ice_sched_add_nodes_to_hw_layer(pi, tc_node, parent,
							 layer,	new_num_nodes,
							 first_teid_ptr,
							 &num_added);
		if (!status)
			*num_nodes_added += num_added;
		/* added more nodes than requested ? */
		if (*num_nodes_added > num_nodes) {
			ice_debug(pi->hw, ICE_DBG_SCHED, "added extra nodes %d %d\n", num_nodes,
				  *num_nodes_added);
			status = ICE_ERR_CFG;
			break;
		}
		/* break if all the nodes are added successfully */
		if (!status && (*num_nodes_added == num_nodes))
			break;
		/* break if the error is not max limit */
		if (status && status != ICE_ERR_MAX_LIMIT)
			break;
		/* Exceeded the max children */
		max_child_nodes = pi->hw->max_children[parent->tx_sched_layer];
		/* utilize all the spaces if the parent is not full */
		if (parent->num_children < max_child_nodes) {
			new_num_nodes = max_child_nodes - parent->num_children;
		} else {
			/* This parent is full, try the next sibling */
			parent = parent->sibling;
			/* Don't modify the first node TEID memory if the
			 * first node was added already in the above call.
			 * Instead send some temp memory for all other
			 * recursive calls.
			 */
			if (num_added)
				first_teid_ptr = &temp;

			new_num_nodes = num_nodes - *num_nodes_added;
		}
	}
	return status;
}

/**
 * ice_sched_get_qgrp_layer - get the current queue group layer number
 * @hw: pointer to the HW struct
 *
 * This function returns the current queue group layer number
 */
static u8 ice_sched_get_qgrp_layer(struct ice_hw *hw)
{
	/* It's always total layers - 1, the array is 0 relative so -2 */
	return hw->num_tx_sched_layers - ICE_QGRP_LAYER_OFFSET;
}

/**
 * ice_sched_get_vsi_layer - get the current VSI layer number
 * @hw: pointer to the HW struct
 *
 * This function returns the current VSI layer number
 */
static u8 ice_sched_get_vsi_layer(struct ice_hw *hw)
{
	/* Num Layers       VSI layer
	 *     9               6
	 *     7               4
	 *     5 or less       sw_entry_point_layer
	 */
	/* calculate the VSI layer based on number of layers. */
	if (hw->num_tx_sched_layers == ICE_SCHED_9_LAYERS)
		return hw->num_tx_sched_layers - ICE_VSI_LAYER_OFFSET;
	else if (hw->num_tx_sched_layers == ICE_SCHED_5_LAYERS)
		/* qgroup and VSI layers are same */
		return hw->num_tx_sched_layers - ICE_QGRP_LAYER_OFFSET;
	return hw->sw_entry_point_layer;
}

/**
 * ice_sched_get_agg_layer - get the current aggregator layer number
 * @hw: pointer to the HW struct
 *
 * This function returns the current aggregator layer number
 */
static u8 ice_sched_get_agg_layer(struct ice_hw *hw)
{
	/* Num Layers       aggregator layer
	 *     9               4
	 *     7 or less       sw_entry_point_layer
	 */
	/* calculate the aggregator layer based on number of layers. */
	if (hw->num_tx_sched_layers == ICE_SCHED_9_LAYERS)
		return hw->num_tx_sched_layers - ICE_AGG_LAYER_OFFSET;
	return hw->sw_entry_point_layer;
}

/**
 * ice_rm_dflt_leaf_node - remove the default leaf node in the tree
 * @pi: port information structure
 *
 * This function removes the leaf node that was created by the FW
 * during initialization
 */
static void ice_rm_dflt_leaf_node(struct ice_port_info *pi)
{
	struct ice_sched_node *node;

	node = pi->root;
	while (node) {
		if (!node->num_children)
			break;
		node = node->children[0];
	}
	if (node && node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF) {
		u32 teid = LE32_TO_CPU(node->info.node_teid);
		int status;

		/* remove the default leaf node */
		status = ice_sched_remove_elems(pi->hw, node->parent, 1, &teid);
		if (!status)
			ice_free_sched_node(pi, node);
	}
}

/**
 * ice_sched_rm_dflt_nodes - free the default nodes in the tree
 * @pi: port information structure
 *
 * This function frees all the nodes except root and TC that were created by
 * the FW during initialization
 */
static void ice_sched_rm_dflt_nodes(struct ice_port_info *pi)
{
	struct ice_sched_node *node;

	ice_rm_dflt_leaf_node(pi);

	/* remove the default nodes except TC and root nodes */
	node = pi->root;
	while (node) {
		if (node->tx_sched_layer >= pi->hw->sw_entry_point_layer &&
		    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_TC &&
		    node->info.data.elem_type != ICE_AQC_ELEM_TYPE_ROOT_PORT) {
			ice_free_sched_node(pi, node);
			break;
		}

		if (!node->num_children)
			break;
		node = node->children[0];
	}
}

/**
 * ice_sched_init_port - Initialize scheduler by querying information from FW
 * @pi: port info structure for the tree to cleanup
 *
 * This function is the initial call to find the total number of Tx scheduler
 * resources, default topology created by firmware and storing the information
 * in SW DB.
 */
int ice_sched_init_port(struct ice_port_info *pi)
{
	struct ice_aqc_get_topo_elem *buf;
	struct ice_hw *hw;
	u8 num_branches;
	u16 num_elems;
	int status;
	u8 i, j;

	if (!pi)
		return ICE_ERR_PARAM;
	hw = pi->hw;

	/* Query the Default Topology from FW */
	buf = (struct ice_aqc_get_topo_elem *)ice_malloc(hw,
							 ICE_AQ_MAX_BUF_LEN);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	/* Query default scheduling tree topology */
	status = ice_aq_get_dflt_topo(hw, pi->lport, buf, ICE_AQ_MAX_BUF_LEN,
				      &num_branches, NULL);
	if (status)
		goto err_init_port;

	/* num_branches should be between 1-8 */
	if (num_branches < 1 || num_branches > ICE_TXSCHED_MAX_BRANCHES) {
		ice_debug(hw, ICE_DBG_SCHED, "num_branches unexpected %d\n",
			  num_branches);
		status = ICE_ERR_PARAM;
		goto err_init_port;
	}

	/* get the number of elements on the default/first branch */
	num_elems = LE16_TO_CPU(buf[0].hdr.num_elems);

	/* num_elems should always be between 1-9 */
	if (num_elems < 1 || num_elems > ICE_AQC_TOPO_MAX_LEVEL_NUM) {
		ice_debug(hw, ICE_DBG_SCHED, "num_elems unexpected %d\n",
			  num_elems);
		status = ICE_ERR_PARAM;
		goto err_init_port;
	}

	/* If the last node is a leaf node then the index of the queue group
	 * layer is two less than the number of elements.
	 */
	if (num_elems > 2 && buf[0].generic[num_elems - 1].data.elem_type ==
	    ICE_AQC_ELEM_TYPE_LEAF)
		pi->last_node_teid =
			LE32_TO_CPU(buf[0].generic[num_elems - 2].node_teid);
	else
		pi->last_node_teid =
			LE32_TO_CPU(buf[0].generic[num_elems - 1].node_teid);

	/* Insert the Tx Sched root node */
	status = ice_sched_add_root_node(pi, &buf[0].generic[0]);
	if (status)
		goto err_init_port;

	/* Parse the default tree and cache the information */
	for (i = 0; i < num_branches; i++) {
		num_elems = LE16_TO_CPU(buf[i].hdr.num_elems);

		/* Skip root element as already inserted */
		for (j = 1; j < num_elems; j++) {
			/* update the sw entry point */
			if (buf[0].generic[j].data.elem_type ==
			    ICE_AQC_ELEM_TYPE_ENTRY_POINT)
				hw->sw_entry_point_layer = j;

			status = ice_sched_add_node(pi, j, &buf[i].generic[j], NULL);
			if (status)
				goto err_init_port;
		}
	}

	/* Remove the default nodes. */
	if (pi->root)
		ice_sched_rm_dflt_nodes(pi);

	/* initialize the port for handling the scheduler tree */
	pi->port_state = ICE_SCHED_PORT_STATE_READY;
	ice_init_lock(&pi->sched_lock);
	for (i = 0; i < ICE_AQC_TOPO_MAX_LEVEL_NUM; i++)
		INIT_LIST_HEAD(&hw->rl_prof_list[i]);

err_init_port:
	if (status && pi->root) {
		ice_free_sched_node(pi, pi->root);
		pi->root = NULL;
	}

	ice_free(hw, buf);
	return status;
}

/**
 * ice_sched_get_node - Get the struct ice_sched_node for given TEID
 * @pi: port information structure
 * @teid: Scheduler node TEID
 *
 * This function retrieves the ice_sched_node struct for given TEID from
 * the SW DB and returns it to the caller.
 */
struct ice_sched_node *ice_sched_get_node(struct ice_port_info *pi, u32 teid)
{
	struct ice_sched_node *node;

	if (!pi)
		return NULL;

	/* Find the node starting from root */
	ice_acquire_lock(&pi->sched_lock);
	node = ice_sched_find_node_by_teid(pi->root, teid);
	ice_release_lock(&pi->sched_lock);

	if (!node)
		ice_debug(pi->hw, ICE_DBG_SCHED, "Node not found for teid=0x%x\n", teid);

	return node;
}

/**
 * ice_sched_query_res_alloc - query the FW for num of logical sched layers
 * @hw: pointer to the HW struct
 *
 * query FW for allocated scheduler resources and store in HW struct
 */
int ice_sched_query_res_alloc(struct ice_hw *hw)
{
	struct ice_aqc_query_txsched_res_resp *buf;
	__le16 max_sibl;
	int status = 0;
	u16 i;

	if (hw->layer_info)
		return status;

	buf = (struct ice_aqc_query_txsched_res_resp *)
		ice_malloc(hw, sizeof(*buf));
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	status = ice_aq_query_sched_res(hw, sizeof(*buf), buf, NULL);
	if (status)
		goto sched_query_out;

	hw->num_tx_sched_layers =
		(u8)LE16_TO_CPU(buf->sched_props.logical_levels);
	hw->num_tx_sched_phys_layers =
		(u8)LE16_TO_CPU(buf->sched_props.phys_levels);
	hw->flattened_layers = buf->sched_props.flattening_bitmap;
	hw->max_cgds = buf->sched_props.max_pf_cgds;

	/* max sibling group size of current layer refers to the max children
	 * of the below layer node.
	 * layer 1 node max children will be layer 2 max sibling group size
	 * layer 2 node max children will be layer 3 max sibling group size
	 * and so on. This array will be populated from root (index 0) to
	 * qgroup layer 7. Leaf node has no children.
	 */
	for (i = 0; i < hw->num_tx_sched_layers - 1; i++) {
		max_sibl = buf->layer_props[i + 1].max_sibl_grp_sz;
		hw->max_children[i] = LE16_TO_CPU(max_sibl);
	}

	hw->layer_info = (struct ice_aqc_layer_props *)
			 ice_memdup(hw, buf->layer_props,
				    (hw->num_tx_sched_layers *
				     sizeof(*hw->layer_info)),
				    ICE_NONDMA_TO_NONDMA);
	if (!hw->layer_info) {
		status = ICE_ERR_NO_MEMORY;
		goto sched_query_out;
	}

sched_query_out:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_sched_get_psm_clk_freq - determine the PSM clock frequency
 * @hw: pointer to the HW struct
 *
 * Determine the PSM clock frequency and store in HW struct
 */
void ice_sched_get_psm_clk_freq(struct ice_hw *hw)
{
	u32 val, clk_src;

	val = rd32(hw, GLGEN_CLKSTAT_SRC);
	clk_src = (val & GLGEN_CLKSTAT_SRC_PSM_CLK_SRC_M) >>
		GLGEN_CLKSTAT_SRC_PSM_CLK_SRC_S;

	switch (clk_src) {
	case PSM_CLK_SRC_367_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_367MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_416_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_416MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_446_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_446MHZ_IN_HZ;
		break;
	case PSM_CLK_SRC_390_MHZ:
		hw->psm_clk_freq = ICE_PSM_CLK_390MHZ_IN_HZ;
		break;

	/* default condition is not required as clk_src is restricted
	 * to a 2-bit value from GLGEN_CLKSTAT_SRC_PSM_CLK_SRC_M mask.
	 * The above switch statements cover the possible values of
	 * this variable.
	 */
	}
}

/**
 * ice_sched_find_node_in_subtree - Find node in part of base node subtree
 * @hw: pointer to the HW struct
 * @base: pointer to the base node
 * @node: pointer to the node to search
 *
 * This function checks whether a given node is part of the base node
 * subtree or not
 */
bool
ice_sched_find_node_in_subtree(struct ice_hw *hw, struct ice_sched_node *base,
			       struct ice_sched_node *node)
{
	u8 i;

	for (i = 0; i < base->num_children; i++) {
		struct ice_sched_node *child = base->children[i];

		if (node == child)
			return true;

		if (child->tx_sched_layer > node->tx_sched_layer)
			return false;

		/* this recursion is intentional, and wouldn't
		 * go more than 8 calls
		 */
		if (ice_sched_find_node_in_subtree(hw, child, node))
			return true;
	}
	return false;
}

/**
 * ice_sched_get_free_qgrp - Scan all queue group siblings and find a free node
 * @pi: port information structure
 * @vsi_node: software VSI handle
 * @qgrp_node: first queue group node identified for scanning
 * @owner: LAN or RDMA
 *
 * This function retrieves a free LAN or RDMA queue group node by scanning
 * qgrp_node and its siblings for the queue group with the fewest number
 * of queues currently assigned.
 */
static struct ice_sched_node *
ice_sched_get_free_qgrp(struct ice_port_info *pi,
			struct ice_sched_node *vsi_node,
			struct ice_sched_node *qgrp_node, u8 owner)
{
	struct ice_sched_node *min_qgrp;
	u8 min_children;

	if (!qgrp_node)
		return qgrp_node;
	min_children = qgrp_node->num_children;
	if (!min_children)
		return qgrp_node;
	min_qgrp = qgrp_node;
	/* scan all queue groups until find a node which has less than the
	 * minimum number of children. This way all queue group nodes get
	 * equal number of shares and active. The bandwidth will be equally
	 * distributed across all queues.
	 */
	while (qgrp_node) {
		/* make sure the qgroup node is part of the VSI subtree */
		if (ice_sched_find_node_in_subtree(pi->hw, vsi_node, qgrp_node))
			if (qgrp_node->num_children < min_children &&
			    qgrp_node->owner == owner) {
				/* replace the new min queue group node */
				min_qgrp = qgrp_node;
				min_children = min_qgrp->num_children;
				/* break if it has no children, */
				if (!min_children)
					break;
			}
		qgrp_node = qgrp_node->sibling;
	}
	return min_qgrp;
}

/**
 * ice_sched_get_free_qparent - Get a free LAN or RDMA queue group node
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: branch number
 * @owner: LAN or RDMA
 *
 * This function retrieves a free LAN or RDMA queue group node
 */
struct ice_sched_node *
ice_sched_get_free_qparent(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			   u8 owner)
{
	struct ice_sched_node *vsi_node, *qgrp_node;
	struct ice_vsi_ctx *vsi_ctx;
	u8 qgrp_layer, vsi_layer;
	u16 max_children;

	qgrp_layer = ice_sched_get_qgrp_layer(pi->hw);
	vsi_layer = ice_sched_get_vsi_layer(pi->hw);
	max_children = pi->hw->max_children[qgrp_layer];

	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return NULL;
	vsi_node = vsi_ctx->sched.vsi_node[tc];
	/* validate invalid VSI ID */
	if (!vsi_node)
		return NULL;

	/* If the queue group and vsi layer are same then queues
	 * are all attached directly to VSI
	 */
	if (qgrp_layer == vsi_layer)
		return vsi_node;

	/* get the first queue group node from VSI sub-tree */
	qgrp_node = ice_sched_get_first_node(pi, vsi_node, qgrp_layer);
	while (qgrp_node) {
		/* make sure the qgroup node is part of the VSI subtree */
		if (ice_sched_find_node_in_subtree(pi->hw, vsi_node, qgrp_node))
			if (qgrp_node->num_children < max_children &&
			    qgrp_node->owner == owner)
				break;
		qgrp_node = qgrp_node->sibling;
	}

	/* Select the best queue group */
	return ice_sched_get_free_qgrp(pi, vsi_node, qgrp_node, owner);
}

/**
 * ice_sched_get_vsi_node - Get a VSI node based on VSI ID
 * @pi: pointer to the port information structure
 * @tc_node: pointer to the TC node
 * @vsi_handle: software VSI handle
 *
 * This function retrieves a VSI node for a given VSI ID from a given
 * TC branch
 */
struct ice_sched_node *
ice_sched_get_vsi_node(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		       u16 vsi_handle)
{
	struct ice_sched_node *node;
	u8 vsi_layer;

	vsi_layer = ice_sched_get_vsi_layer(pi->hw);
	node = ice_sched_get_first_node(pi, tc_node, vsi_layer);

	/* Check whether it already exists */
	while (node) {
		if (node->vsi_handle == vsi_handle)
			return node;
		node = node->sibling;
	}

	return node;
}

/**
 * ice_sched_get_agg_node - Get an aggregator node based on aggregator ID
 * @pi: pointer to the port information structure
 * @tc_node: pointer to the TC node
 * @agg_id: aggregator ID
 *
 * This function retrieves an aggregator node for a given aggregator ID from
 * a given TC branch
 */
static struct ice_sched_node *
ice_sched_get_agg_node(struct ice_port_info *pi, struct ice_sched_node *tc_node,
		       u32 agg_id)
{
	struct ice_sched_node *node;
	struct ice_hw *hw = pi->hw;
	u8 agg_layer;

	if (!hw)
		return NULL;
	agg_layer = ice_sched_get_agg_layer(hw);
	node = ice_sched_get_first_node(pi, tc_node, agg_layer);

	/* Check whether it already exists */
	while (node) {
		if (node->agg_id == agg_id)
			return node;
		node = node->sibling;
	}

	return node;
}

/**
 * ice_sched_check_node - Compare node parameters between SW DB and HW DB
 * @hw: pointer to the HW struct
 * @node: pointer to the ice_sched_node struct
 *
 * This function queries and compares the HW element with SW DB node parameters
 */
static bool ice_sched_check_node(struct ice_hw *hw, struct ice_sched_node *node)
{
	struct ice_aqc_txsched_elem_data buf;
	u32 node_teid;
	int status;

	node_teid = LE32_TO_CPU(node->info.node_teid);
	status = ice_sched_query_elem(hw, node_teid, &buf);
	if (status)
		return false;

	if (memcmp(&buf, &node->info, sizeof(buf))) {
		ice_debug(hw, ICE_DBG_SCHED, "Node mismatch for teid=0x%x\n",
			  node_teid);
		return false;
	}

	return true;
}

/**
 * ice_sched_calc_vsi_child_nodes - calculate number of VSI child nodes
 * @hw: pointer to the HW struct
 * @num_qs: number of queues
 * @num_nodes: num nodes array
 *
 * This function calculates the number of VSI child nodes based on the
 * number of queues.
 */
static void
ice_sched_calc_vsi_child_nodes(struct ice_hw *hw, u16 num_qs, u16 *num_nodes)
{
	u16 num = num_qs;
	u8 i, qgl, vsil;

	qgl = ice_sched_get_qgrp_layer(hw);
	vsil = ice_sched_get_vsi_layer(hw);

	/* calculate num nodes from queue group to VSI layer */
	for (i = qgl; i > vsil; i--) {
		/* round to the next integer if there is a remainder */
		num = DIVIDE_AND_ROUND_UP(num, hw->max_children[i]);

		/* need at least one node */
		num_nodes[i] = num ? num : 1;
	}
}

/**
 * ice_sched_add_vsi_child_nodes - add VSI child nodes to tree
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc_node: pointer to the TC node
 * @num_nodes: pointer to the num nodes that needs to be added per layer
 * @owner: node owner (LAN or RDMA)
 *
 * This function adds the VSI child nodes to tree. It gets called for
 * LAN and RDMA separately.
 */
static int
ice_sched_add_vsi_child_nodes(struct ice_port_info *pi, u16 vsi_handle,
			      struct ice_sched_node *tc_node, u16 *num_nodes,
			      u8 owner)
{
	struct ice_sched_node *parent, *node;
	struct ice_hw *hw = pi->hw;
	u32 first_node_teid;
	u16 num_added = 0;
	u8 i, qgl, vsil;

	qgl = ice_sched_get_qgrp_layer(hw);
	vsil = ice_sched_get_vsi_layer(hw);
	parent = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	for (i = vsil + 1; i <= qgl; i++) {
		int status;

		if (!parent)
			return ICE_ERR_CFG;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_added);
		if (status || num_nodes[i] != num_added)
			return ICE_ERR_CFG;

		/* The newly added node can be a new parent for the next
		 * layer nodes
		 */
		if (num_added) {
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
			node = parent;
			while (node) {
				node->owner = owner;
				node = node->sibling;
			}
		} else {
			parent = parent->children[0];
		}
	}

	return 0;
}

/**
 * ice_sched_calc_vsi_support_nodes - calculate number of VSI support nodes
 * @pi: pointer to the port info structure
 * @tc_node: pointer to TC node
 * @num_nodes: pointer to num nodes array
 *
 * This function calculates the number of supported nodes needed to add this
 * VSI into Tx tree including the VSI, parent and intermediate nodes in below
 * layers
 */
static void
ice_sched_calc_vsi_support_nodes(struct ice_port_info *pi,
				 struct ice_sched_node *tc_node, u16 *num_nodes)
{
	struct ice_sched_node *node;
	u8 vsil;
	int i;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	for (i = vsil; i >= pi->hw->sw_entry_point_layer; i--)
		/* Add intermediate nodes if TC has no children and
		 * need at least one node for VSI
		 */
		if (!tc_node->num_children || i == vsil) {
			num_nodes[i]++;
		} else {
			/* If intermediate nodes are reached max children
			 * then add a new one.
			 */
			node = ice_sched_get_first_node(pi, tc_node, (u8)i);
			/* scan all the siblings */
			while (node) {
				if (node->num_children <
				    pi->hw->max_children[i])
					break;
				node = node->sibling;
			}

			/* tree has one intermediate node to add this new VSI.
			 * So no need to calculate supported nodes for below
			 * layers.
			 */
			if (node)
				break;
			/* all the nodes are full, allocate a new one */
			num_nodes[i]++;
		}
}

/**
 * ice_sched_add_vsi_support_nodes - add VSI supported nodes into Tx tree
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc_node: pointer to TC node
 * @num_nodes: pointer to num nodes array
 *
 * This function adds the VSI supported nodes into Tx tree including the
 * VSI, its parent and intermediate nodes in below layers
 */
static int
ice_sched_add_vsi_support_nodes(struct ice_port_info *pi, u16 vsi_handle,
				struct ice_sched_node *tc_node, u16 *num_nodes)
{
	struct ice_sched_node *parent = tc_node;
	u32 first_node_teid;
	u16 num_added = 0;
	u8 i, vsil;

	if (!pi)
		return ICE_ERR_PARAM;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	for (i = pi->hw->sw_entry_point_layer; i <= vsil; i++) {
		int status;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent,
						      i, num_nodes[i],
						      &first_node_teid,
						      &num_added);
		if (status || num_nodes[i] != num_added)
			return ICE_ERR_CFG;

		/* The newly added node can be a new parent for the next
		 * layer nodes
		 */
		if (num_added)
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
		else
			parent = parent->children[0];

		if (!parent)
			return ICE_ERR_CFG;

		if (i == vsil)
			parent->vsi_handle = vsi_handle;
	}

	return 0;
}

/**
 * ice_sched_add_vsi_to_topo - add a new VSI into tree
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 *
 * This function adds a new VSI into scheduler tree
 */
static int
ice_sched_add_vsi_to_topo(struct ice_port_info *pi, u16 vsi_handle, u8 tc)
{
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_sched_node *tc_node;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_PARAM;

	/* calculate number of supported nodes needed for this VSI */
	ice_sched_calc_vsi_support_nodes(pi, tc_node, num_nodes);

	/* add VSI supported nodes to TC subtree */
	return ice_sched_add_vsi_support_nodes(pi, vsi_handle, tc_node,
					       num_nodes);
}

/**
 * ice_sched_update_vsi_child_nodes - update VSI child nodes
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @new_numqs: new number of max queues
 * @owner: owner of this subtree
 *
 * This function updates the VSI child nodes based on the number of queues
 */
static int
ice_sched_update_vsi_child_nodes(struct ice_port_info *pi, u16 vsi_handle,
				 u8 tc, u16 new_numqs, u8 owner)
{
	u16 new_num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_sched_node *vsi_node;
	struct ice_sched_node *tc_node;
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u16 prev_numqs;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	if (!vsi_node)
		return ICE_ERR_CFG;

	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;

	if (owner == ICE_SCHED_NODE_OWNER_LAN)
		prev_numqs = vsi_ctx->sched.max_lanq[tc];
	else
		prev_numqs = vsi_ctx->sched.max_rdmaq[tc];
	/* num queues are not changed or less than the previous number */
	if (new_numqs <= prev_numqs)
		return status;
	if (owner == ICE_SCHED_NODE_OWNER_LAN) {
		status = ice_alloc_lan_q_ctx(hw, vsi_handle, tc, new_numqs);
		if (status)
			return status;
	} else {
		status = ice_alloc_rdma_q_ctx(hw, vsi_handle, tc, new_numqs);
		if (status)
			return status;
	}

	if (new_numqs)
		ice_sched_calc_vsi_child_nodes(hw, new_numqs, new_num_nodes);
	/* Keep the max number of queue configuration all the time. Update the
	 * tree only if number of queues > previous number of queues. This may
	 * leave some extra nodes in the tree if number of queues < previous
	 * number but that wouldn't harm anything. Removing those extra nodes
	 * may complicate the code if those nodes are part of SRL or
	 * individually rate limited.
	 */
	status = ice_sched_add_vsi_child_nodes(pi, vsi_handle, tc_node,
					       new_num_nodes, owner);
	if (status)
		return status;
	if (owner == ICE_SCHED_NODE_OWNER_LAN)
		vsi_ctx->sched.max_lanq[tc] = new_numqs;
	else
		vsi_ctx->sched.max_rdmaq[tc] = new_numqs;

	return 0;
}

/**
 * ice_sched_cfg_vsi - configure the new/existing VSI
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: TC number
 * @maxqs: max number of queues
 * @owner: LAN or RDMA
 * @enable: TC enabled or disabled
 *
 * This function adds/updates VSI nodes based on the number of queues. If TC is
 * enabled and VSI is in suspended state then resume the VSI back. If TC is
 * disabled then suspend the VSI if it is not already.
 */
int
ice_sched_cfg_vsi(struct ice_port_info *pi, u16 vsi_handle, u8 tc, u16 maxqs,
		  u8 owner, bool enable)
{
	struct ice_sched_node *vsi_node, *tc_node;
	struct ice_vsi_ctx *vsi_ctx;
	struct ice_hw *hw = pi->hw;
	int status = 0;

	ice_debug(pi->hw, ICE_DBG_SCHED, "add/config VSI %d\n", vsi_handle);
	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_PARAM;
	vsi_ctx = ice_get_vsi_ctx(hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);

	/* suspend the VSI if TC is not enabled */
	if (!enable) {
		if (vsi_node && vsi_node->in_use) {
			u32 teid = LE32_TO_CPU(vsi_node->info.node_teid);

			status = ice_sched_suspend_resume_elems(hw, 1, &teid,
								true);
			if (!status)
				vsi_node->in_use = false;
		}
		return status;
	}

	/* TC is enabled, if it is a new VSI then add it to the tree */
	if (!vsi_node) {
		status = ice_sched_add_vsi_to_topo(pi, vsi_handle, tc);
		if (status)
			return status;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			return ICE_ERR_CFG;

		vsi_ctx->sched.vsi_node[tc] = vsi_node;
		vsi_node->in_use = true;
		/* invalidate the max queues whenever VSI gets added first time
		 * into the scheduler tree (boot or after reset). We need to
		 * recreate the child nodes all the time in these cases.
		 */
		vsi_ctx->sched.max_lanq[tc] = 0;
		vsi_ctx->sched.max_rdmaq[tc] = 0;
	}

	/* update the VSI child nodes */
	status = ice_sched_update_vsi_child_nodes(pi, vsi_handle, tc, maxqs,
						  owner);
	if (status)
		return status;

	/* TC is enabled, resume the VSI if it is in the suspend state */
	if (!vsi_node->in_use) {
		u32 teid = LE32_TO_CPU(vsi_node->info.node_teid);

		status = ice_sched_suspend_resume_elems(hw, 1, &teid, false);
		if (!status)
			vsi_node->in_use = true;
	}

	return status;
}

/**
 * ice_sched_rm_agg_vsi_info - remove aggregator related VSI info entry
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function removes single aggregator VSI info entry from
 * aggregator list.
 */
static void ice_sched_rm_agg_vsi_info(struct ice_port_info *pi, u16 vsi_handle)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_sched_agg_info *atmp;

	LIST_FOR_EACH_ENTRY_SAFE(agg_info, atmp, &pi->hw->agg_list,
				 ice_sched_agg_info,
				 list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;
		struct ice_sched_agg_vsi_info *vtmp;

		LIST_FOR_EACH_ENTRY_SAFE(agg_vsi_info, vtmp,
					 &agg_info->agg_vsi_list,
					 ice_sched_agg_vsi_info, list_entry)
			if (agg_vsi_info->vsi_handle == vsi_handle) {
				LIST_DEL(&agg_vsi_info->list_entry);
				ice_free(pi->hw, agg_vsi_info);
				return;
			}
	}
}

/**
 * ice_sched_is_leaf_node_present - check for a leaf node in the sub-tree
 * @node: pointer to the sub-tree node
 *
 * This function checks for a leaf node presence in a given sub-tree node.
 */
static bool ice_sched_is_leaf_node_present(struct ice_sched_node *node)
{
	u8 i;

	for (i = 0; i < node->num_children; i++)
		if (ice_sched_is_leaf_node_present(node->children[i]))
			return true;
	/* check for a leaf node */
	return (node->info.data.elem_type == ICE_AQC_ELEM_TYPE_LEAF);
}

/**
 * ice_sched_rm_vsi_cfg - remove the VSI and its children nodes
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @owner: LAN or RDMA
 *
 * This function removes the VSI and its LAN or RDMA children nodes from the
 * scheduler tree.
 */
static int
ice_sched_rm_vsi_cfg(struct ice_port_info *pi, u16 vsi_handle, u8 owner)
{
	struct ice_vsi_ctx *vsi_ctx;
	int status = ICE_ERR_PARAM;
	u8 i;

	ice_debug(pi->hw, ICE_DBG_SCHED, "removing VSI %d\n", vsi_handle);
	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return status;
	ice_acquire_lock(&pi->sched_lock);
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		goto exit_sched_rm_vsi_cfg;

	ice_for_each_traffic_class(i) {
		struct ice_sched_node *vsi_node, *tc_node;
		u8 j = 0;

		tc_node = ice_sched_get_tc_node(pi, i);
		if (!tc_node)
			continue;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;

		if (ice_sched_is_leaf_node_present(vsi_node)) {
			ice_debug(pi->hw, ICE_DBG_SCHED, "VSI has leaf nodes in TC %d\n", i);
			status = ICE_ERR_IN_USE;
			goto exit_sched_rm_vsi_cfg;
		}
		while (j < vsi_node->num_children) {
			if (vsi_node->children[j]->owner == owner) {
				ice_free_sched_node(pi, vsi_node->children[j]);

				/* reset the counter again since the num
				 * children will be updated after node removal
				 */
				j = 0;
			} else {
				j++;
			}
		}
		/* remove the VSI if it has no children */
		if (!vsi_node->num_children) {
			ice_free_sched_node(pi, vsi_node);
			vsi_ctx->sched.vsi_node[i] = NULL;

			/* clean up aggregator related VSI info if any */
			ice_sched_rm_agg_vsi_info(pi, vsi_handle);
		}
		if (owner == ICE_SCHED_NODE_OWNER_LAN)
			vsi_ctx->sched.max_lanq[i] = 0;
		else
			vsi_ctx->sched.max_rdmaq[i] = 0;
	}
	status = 0;

exit_sched_rm_vsi_cfg:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_rm_vsi_lan_cfg - remove VSI and its LAN children nodes
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function clears the VSI and its LAN children nodes from scheduler tree
 * for all TCs.
 */
int ice_rm_vsi_lan_cfg(struct ice_port_info *pi, u16 vsi_handle)
{
	return ice_sched_rm_vsi_cfg(pi, vsi_handle, ICE_SCHED_NODE_OWNER_LAN);
}

/**
 * ice_rm_vsi_rdma_cfg - remove VSI and its RDMA children nodes
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function clears the VSI and its RDMA children nodes from scheduler tree
 * for all TCs.
 */
int ice_rm_vsi_rdma_cfg(struct ice_port_info *pi, u16 vsi_handle)
{
	return ice_sched_rm_vsi_cfg(pi, vsi_handle, ICE_SCHED_NODE_OWNER_RDMA);
}

/**
 * ice_sched_is_tree_balanced - Check tree nodes are identical or not
 * @hw: pointer to the HW struct
 * @node: pointer to the ice_sched_node struct
 *
 * This function compares all the nodes for a given tree against HW DB nodes
 * This function needs to be called with the port_info->sched_lock held
 */
bool ice_sched_is_tree_balanced(struct ice_hw *hw, struct ice_sched_node *node)
{
	u8 i;

	/* start from the leaf node */
	for (i = 0; i < node->num_children; i++)
		/* Fail if node doesn't match with the SW DB
		 * this recursion is intentional, and wouldn't
		 * go more than 9 calls
		 */
		if (!ice_sched_is_tree_balanced(hw, node->children[i]))
			return false;

	return ice_sched_check_node(hw, node);
}

/**
 * ice_aq_query_node_to_root - retrieve the tree topology for a given node TEID
 * @hw: pointer to the HW struct
 * @node_teid: node TEID
 * @buf: pointer to buffer
 * @buf_size: buffer size in bytes
 * @cd: pointer to command details structure or NULL
 *
 * This function retrieves the tree topology from the firmware for a given
 * node TEID to the root node.
 */
int
ice_aq_query_node_to_root(struct ice_hw *hw, u32 node_teid,
			  struct ice_aqc_txsched_elem_data *buf, u16 buf_size,
			  struct ice_sq_cd *cd)
{
	struct ice_aqc_query_node_to_root *cmd;
	struct ice_aq_desc desc;

	cmd = &desc.params.query_node_to_root;
	ice_fill_dflt_direct_cmd_desc(&desc, ice_aqc_opc_query_node_to_root);
	cmd->teid = CPU_TO_LE32(node_teid);
	return ice_aq_send_cmd(hw, &desc, buf, buf_size, cd);
}

/**
 * ice_get_agg_info - get the aggregator ID
 * @hw: pointer to the hardware structure
 * @agg_id: aggregator ID
 *
 * This function validates aggregator ID. The function returns info if
 * aggregator ID is present in list otherwise it returns null.
 */
static struct ice_sched_agg_info *
ice_get_agg_info(struct ice_hw *hw, u32 agg_id)
{
	struct ice_sched_agg_info *agg_info;

	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry)
		if (agg_info->agg_id == agg_id)
			return agg_info;

	return NULL;
}

/**
 * ice_sched_get_free_vsi_parent - Find a free parent node in aggregator subtree
 * @hw: pointer to the HW struct
 * @node: pointer to a child node
 * @num_nodes: num nodes count array
 *
 * This function walks through the aggregator subtree to find a free parent
 * node
 */
static struct ice_sched_node *
ice_sched_get_free_vsi_parent(struct ice_hw *hw, struct ice_sched_node *node,
			      u16 *num_nodes)
{
	u8 l = node->tx_sched_layer;
	u8 vsil, i;

	vsil = ice_sched_get_vsi_layer(hw);

	/* Is it VSI parent layer ? */
	if (l == vsil - 1)
		return (node->num_children < hw->max_children[l]) ? node : NULL;

	/* We have intermediate nodes. Let's walk through the subtree. If the
	 * intermediate node has space to add a new node then clear the count
	 */
	if (node->num_children < hw->max_children[l])
		num_nodes[l] = 0;
	/* The below recursive call is intentional and wouldn't go more than
	 * 2 or 3 iterations.
	 */

	for (i = 0; i < node->num_children; i++) {
		struct ice_sched_node *parent;

		parent = ice_sched_get_free_vsi_parent(hw, node->children[i],
						       num_nodes);
		if (parent)
			return parent;
	}

	return NULL;
}

/**
 * ice_sched_update_parent - update the new parent in SW DB
 * @new_parent: pointer to a new parent node
 * @node: pointer to a child node
 *
 * This function removes the child from the old parent and adds it to a new
 * parent
 */
void
ice_sched_update_parent(struct ice_sched_node *new_parent,
			struct ice_sched_node *node)
{
	struct ice_sched_node *old_parent;
	u8 i, j;

	old_parent = node->parent;

	/* update the old parent children */
	for (i = 0; i < old_parent->num_children; i++)
		if (old_parent->children[i] == node) {
			for (j = i + 1; j < old_parent->num_children; j++)
				old_parent->children[j - 1] =
					old_parent->children[j];
			old_parent->num_children--;
			break;
		}

	/* now move the node to a new parent */
	new_parent->children[new_parent->num_children++] = node;
	node->parent = new_parent;
	node->info.parent_teid = new_parent->info.node_teid;
}

/**
 * ice_sched_move_nodes - move child nodes to a given parent
 * @pi: port information structure
 * @parent: pointer to parent node
 * @num_items: number of child nodes to be moved
 * @list: pointer to child node teids
 *
 * This function move the child nodes to a given parent.
 */
int
ice_sched_move_nodes(struct ice_port_info *pi, struct ice_sched_node *parent,
		     u16 num_items, u32 *list)
{
	struct ice_aqc_move_elem *buf;
	struct ice_sched_node *node;
	u16 i, grps_movd = 0;
	struct ice_hw *hw;
	int status = 0;
	u16 buf_len;

	hw = pi->hw;

	if (!parent || !num_items)
		return ICE_ERR_PARAM;

	/* Does parent have enough space */
	if (parent->num_children + num_items >
	    hw->max_children[parent->tx_sched_layer])
		return ICE_ERR_AQ_FULL;

	buf_len = ice_struct_size(buf, teid, 1);
	buf = (struct ice_aqc_move_elem *)ice_malloc(hw, buf_len);
	if (!buf)
		return ICE_ERR_NO_MEMORY;

	for (i = 0; i < num_items; i++) {
		node = ice_sched_find_node_by_teid(pi->root, list[i]);
		if (!node) {
			status = ICE_ERR_PARAM;
			goto move_err_exit;
		}

		buf->hdr.src_parent_teid = node->info.parent_teid;
		buf->hdr.dest_parent_teid = parent->info.node_teid;
		buf->teid[0] = node->info.node_teid;
		buf->hdr.num_elems = CPU_TO_LE16(1);
		status = ice_aq_move_sched_elems(hw, 1, buf, buf_len,
						 &grps_movd, NULL);
		if (status && grps_movd != 1) {
			status = ICE_ERR_CFG;
			goto move_err_exit;
		}

		/* update the SW DB */
		ice_sched_update_parent(parent, node);
	}

move_err_exit:
	ice_free(hw, buf);
	return status;
}

/**
 * ice_sched_move_vsi_to_agg - move VSI to aggregator node
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @agg_id: aggregator ID
 * @tc: TC number
 *
 * This function moves a VSI to an aggregator node or its subtree.
 * Intermediate nodes may be created if required.
 */
static int
ice_sched_move_vsi_to_agg(struct ice_port_info *pi, u16 vsi_handle, u32 agg_id,
			  u8 tc)
{
	struct ice_sched_node *vsi_node, *agg_node, *tc_node, *parent;
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	u32 first_node_teid, vsi_teid;
	u16 num_nodes_added;
	u8 aggl, vsil, i;
	int status;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		return ICE_ERR_DOES_NOT_EXIST;

	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	if (!vsi_node)
		return ICE_ERR_DOES_NOT_EXIST;

	/* Is this VSI already part of given aggregator? */
	if (ice_sched_find_node_in_subtree(pi->hw, agg_node, vsi_node))
		return 0;

	aggl = ice_sched_get_agg_layer(pi->hw);
	vsil = ice_sched_get_vsi_layer(pi->hw);

	/* set intermediate node count to 1 between aggregator and VSI layers */
	for (i = aggl + 1; i < vsil; i++)
		num_nodes[i] = 1;

	/* Check if the aggregator subtree has any free node to add the VSI */
	for (i = 0; i < agg_node->num_children; i++) {
		parent = ice_sched_get_free_vsi_parent(pi->hw,
						       agg_node->children[i],
						       num_nodes);
		if (parent)
			goto move_nodes;
	}

	/* add new nodes */
	parent = agg_node;
	for (i = aggl + 1; i < vsil; i++) {
		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_nodes_added);
		if (status || num_nodes[i] != num_nodes_added)
			return ICE_ERR_CFG;

		/* The newly added node can be a new parent for the next
		 * layer nodes
		 */
		if (num_nodes_added)
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
		else
			parent = parent->children[0];

		if (!parent)
			return ICE_ERR_CFG;
	}

move_nodes:
	vsi_teid = LE32_TO_CPU(vsi_node->info.node_teid);
	return ice_sched_move_nodes(pi, parent, 1, &vsi_teid);
}

/**
 * ice_move_all_vsi_to_dflt_agg - move all VSI(s) to default aggregator
 * @pi: port information structure
 * @agg_info: aggregator info
 * @tc: traffic class number
 * @rm_vsi_info: true or false
 *
 * This function move all the VSI(s) to the default aggregator and delete
 * aggregator VSI info based on passed in boolean parameter rm_vsi_info. The
 * caller holds the scheduler lock.
 */
static int
ice_move_all_vsi_to_dflt_agg(struct ice_port_info *pi,
			     struct ice_sched_agg_info *agg_info, u8 tc,
			     bool rm_vsi_info)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_sched_agg_vsi_info *tmp;
	int status = 0;

	LIST_FOR_EACH_ENTRY_SAFE(agg_vsi_info, tmp, &agg_info->agg_vsi_list,
				 ice_sched_agg_vsi_info, list_entry) {
		u16 vsi_handle = agg_vsi_info->vsi_handle;

		/* Move VSI to default aggregator */
		if (!ice_is_tc_ena(agg_vsi_info->tc_bitmap[0], tc))
			continue;

		status = ice_sched_move_vsi_to_agg(pi, vsi_handle,
						   ICE_DFLT_AGG_ID, tc);
		if (status)
			break;

		ice_clear_bit(tc, agg_vsi_info->tc_bitmap);
		if (rm_vsi_info && !agg_vsi_info->tc_bitmap[0]) {
			LIST_DEL(&agg_vsi_info->list_entry);
			ice_free(pi->hw, agg_vsi_info);
		}
	}

	return status;
}

/**
 * ice_sched_is_agg_inuse - check whether the aggregator is in use or not
 * @pi: port information structure
 * @node: node pointer
 *
 * This function checks whether the aggregator is attached with any VSI or not.
 */
static bool
ice_sched_is_agg_inuse(struct ice_port_info *pi, struct ice_sched_node *node)
{
	u8 vsil, i;

	vsil = ice_sched_get_vsi_layer(pi->hw);
	if (node->tx_sched_layer < vsil - 1) {
		for (i = 0; i < node->num_children; i++)
			if (ice_sched_is_agg_inuse(pi, node->children[i]))
				return true;
		return false;
	} else {
		return node->num_children ? true : false;
	}
}

/**
 * ice_sched_rm_agg_cfg - remove the aggregator node
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: TC number
 *
 * This function removes the aggregator node and intermediate nodes if any
 * from the given TC
 */
static int
ice_sched_rm_agg_cfg(struct ice_port_info *pi, u32 agg_id, u8 tc)
{
	struct ice_sched_node *tc_node, *agg_node;
	struct ice_hw *hw = pi->hw;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		return ICE_ERR_DOES_NOT_EXIST;

	/* Can't remove the aggregator node if it has children */
	if (ice_sched_is_agg_inuse(pi, agg_node))
		return ICE_ERR_IN_USE;

	/* need to remove the whole subtree if aggregator node is the
	 * only child.
	 */
	while (agg_node->tx_sched_layer > hw->sw_entry_point_layer) {
		struct ice_sched_node *parent = agg_node->parent;

		if (!parent)
			return ICE_ERR_CFG;

		if (parent->num_children > 1)
			break;

		agg_node = parent;
	}

	ice_free_sched_node(pi, agg_node);
	return 0;
}

/**
 * ice_rm_agg_cfg_tc - remove aggregator configuration for TC
 * @pi: port information structure
 * @agg_info: aggregator ID
 * @tc: TC number
 * @rm_vsi_info: bool value true or false
 *
 * This function removes aggregator reference to VSI of given TC. It removes
 * the aggregator configuration completely for requested TC. The caller needs
 * to hold the scheduler lock.
 */
static int
ice_rm_agg_cfg_tc(struct ice_port_info *pi, struct ice_sched_agg_info *agg_info,
		  u8 tc, bool rm_vsi_info)
{
	int status = 0;

	/* If nothing to remove - return success */
	if (!ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
		goto exit_rm_agg_cfg_tc;

	status = ice_move_all_vsi_to_dflt_agg(pi, agg_info, tc, rm_vsi_info);
	if (status)
		goto exit_rm_agg_cfg_tc;

	/* Delete aggregator node(s) */
	status = ice_sched_rm_agg_cfg(pi, agg_info->agg_id, tc);
	if (status)
		goto exit_rm_agg_cfg_tc;

	ice_clear_bit(tc, agg_info->tc_bitmap);
exit_rm_agg_cfg_tc:
	return status;
}

/**
 * ice_save_agg_tc_bitmap - save aggregator TC bitmap
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc_bitmap: 8 bits TC bitmap
 *
 * Save aggregator TC bitmap. This function needs to be called with scheduler
 * lock held.
 */
static int
ice_save_agg_tc_bitmap(struct ice_port_info *pi, u32 agg_id,
		       ice_bitmap_t *tc_bitmap)
{
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return ICE_ERR_PARAM;
	ice_cp_bitmap(agg_info->replay_tc_bitmap, tc_bitmap,
		      ICE_MAX_TRAFFIC_CLASS);
	return 0;
}

/**
 * ice_sched_add_agg_cfg - create an aggregator node
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: TC number
 *
 * This function creates an aggregator node and intermediate nodes if required
 * for the given TC
 */
static int
ice_sched_add_agg_cfg(struct ice_port_info *pi, u32 agg_id, u8 tc)
{
	struct ice_sched_node *parent, *agg_node, *tc_node;
	u16 num_nodes[ICE_AQC_TOPO_MAX_LEVEL_NUM] = { 0 };
	struct ice_hw *hw = pi->hw;
	u32 first_node_teid;
	u16 num_nodes_added;
	int status = 0;
	u8 i, aggl;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	/* Does Agg node already exist ? */
	if (agg_node)
		return status;

	aggl = ice_sched_get_agg_layer(hw);

	/* need one node in Agg layer */
	num_nodes[aggl] = 1;

	/* Check whether the intermediate nodes have space to add the
	 * new aggregator. If they are full, then SW needs to allocate a new
	 * intermediate node on those layers
	 */
	for (i = hw->sw_entry_point_layer; i < aggl; i++) {
		parent = ice_sched_get_first_node(pi, tc_node, i);

		/* scan all the siblings */
		while (parent) {
			if (parent->num_children < hw->max_children[i])
				break;
			parent = parent->sibling;
		}

		/* all the nodes are full, reserve one for this layer */
		if (!parent)
			num_nodes[i]++;
	}

	/* add the aggregator node */
	parent = tc_node;
	for (i = hw->sw_entry_point_layer; i <= aggl; i++) {
		if (!parent)
			return ICE_ERR_CFG;

		status = ice_sched_add_nodes_to_layer(pi, tc_node, parent, i,
						      num_nodes[i],
						      &first_node_teid,
						      &num_nodes_added);
		if (status || num_nodes[i] != num_nodes_added)
			return ICE_ERR_CFG;

		/* The newly added node can be a new parent for the next
		 * layer nodes
		 */
		if (num_nodes_added) {
			parent = ice_sched_find_node_by_teid(tc_node,
							     first_node_teid);
			/* register aggregator ID with the aggregator node */
			if (parent && i == aggl)
				parent->agg_id = agg_id;
		} else {
			parent = parent->children[0];
		}
	}

	return 0;
}

/**
 * ice_sched_cfg_agg - configure aggregator node
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @agg_type: aggregator type queue, VSI, or aggregator group
 * @tc_bitmap: bits TC bitmap
 *
 * It registers a unique aggregator node into scheduler services. It
 * allows a user to register with a unique ID to track it's resources.
 * The aggregator type determines if this is a queue group, VSI group
 * or aggregator group. It then creates the aggregator node(s) for requested
 * TC(s) or removes an existing aggregator node including its configuration
 * if indicated via tc_bitmap. Call ice_rm_agg_cfg to release aggregator
 * resources and remove aggregator ID.
 * This function needs to be called with scheduler lock held.
 */
static int
ice_sched_cfg_agg(struct ice_port_info *pi, u32 agg_id,
		  enum ice_agg_type agg_type, ice_bitmap_t *tc_bitmap)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u8 tc;

	agg_info = ice_get_agg_info(hw, agg_id);
	if (!agg_info) {
		/* Create new entry for new aggregator ID */
		agg_info = (struct ice_sched_agg_info *)
			ice_malloc(hw, sizeof(*agg_info));
		if (!agg_info)
			return ICE_ERR_NO_MEMORY;

		agg_info->agg_id = agg_id;
		agg_info->agg_type = agg_type;
		agg_info->tc_bitmap[0] = 0;

		/* Initialize the aggregator VSI list head */
		INIT_LIST_HEAD(&agg_info->agg_vsi_list);

		/* Add new entry in aggregator list */
		LIST_ADD(&agg_info->list_entry, &hw->agg_list);
	}
	/* Create aggregator node(s) for requested TC(s) */
	ice_for_each_traffic_class(tc) {
		if (!ice_is_tc_ena(*tc_bitmap, tc)) {
			/* Delete aggregator cfg TC if it exists previously */
			status = ice_rm_agg_cfg_tc(pi, agg_info, tc, false);
			if (status)
				break;
			continue;
		}

		/* Check if aggregator node for TC already exists */
		if (ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
			continue;

		/* Create new aggregator node for TC */
		status = ice_sched_add_agg_cfg(pi, agg_id, tc);
		if (status)
			break;

		/* Save aggregator node's TC information */
		ice_set_bit(tc, agg_info->tc_bitmap);
	}

	return status;
}

/**
 * ice_cfg_agg - config aggregator node
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @agg_type: aggregator type queue, VSI, or aggregator group
 * @tc_bitmap: bits TC bitmap
 *
 * This function configures aggregator node(s).
 */
int
ice_cfg_agg(struct ice_port_info *pi, u32 agg_id, enum ice_agg_type agg_type,
	    u8 tc_bitmap)
{
	ice_bitmap_t bitmap = tc_bitmap;
	int status;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_cfg_agg(pi, agg_id, agg_type,
				   (ice_bitmap_t *)&bitmap);
	if (!status)
		status = ice_save_agg_tc_bitmap(pi, agg_id,
						(ice_bitmap_t *)&bitmap);
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_get_agg_vsi_info - get the aggregator ID
 * @agg_info: aggregator info
 * @vsi_handle: software VSI handle
 *
 * The function returns aggregator VSI info based on VSI handle. This function
 * needs to be called with scheduler lock held.
 */
static struct ice_sched_agg_vsi_info *
ice_get_agg_vsi_info(struct ice_sched_agg_info *agg_info, u16 vsi_handle)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;

	LIST_FOR_EACH_ENTRY(agg_vsi_info, &agg_info->agg_vsi_list,
			    ice_sched_agg_vsi_info, list_entry)
		if (agg_vsi_info->vsi_handle == vsi_handle)
			return agg_vsi_info;

	return NULL;
}

/**
 * ice_get_vsi_agg_info - get the aggregator info of VSI
 * @hw: pointer to the hardware structure
 * @vsi_handle: Sw VSI handle
 *
 * The function returns aggregator info of VSI represented via vsi_handle. The
 * VSI has in this case a different aggregator than the default one. This
 * function needs to be called with scheduler lock held.
 */
static struct ice_sched_agg_info *
ice_get_vsi_agg_info(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_sched_agg_info *agg_info;

	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;

		agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
		if (agg_vsi_info)
			return agg_info;
	}
	return NULL;
}

/**
 * ice_save_agg_vsi_tc_bitmap - save aggregator VSI TC bitmap
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap of enabled TC(s)
 *
 * Save VSI to aggregator TC bitmap. This function needs to call with scheduler
 * lock held.
 */
static int
ice_save_agg_vsi_tc_bitmap(struct ice_port_info *pi, u32 agg_id, u16 vsi_handle,
			   ice_bitmap_t *tc_bitmap)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return ICE_ERR_PARAM;
	/* check if entry already exist */
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info)
		return ICE_ERR_PARAM;
	ice_cp_bitmap(agg_vsi_info->replay_tc_bitmap, tc_bitmap,
		      ICE_MAX_TRAFFIC_CLASS);
	return 0;
}

/**
 * ice_sched_assoc_vsi_to_agg - associate/move VSI to new/default aggregator
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap of enabled TC(s)
 *
 * This function moves VSI to a new or default aggregator node. If VSI is
 * already associated to the aggregator node then no operation is performed on
 * the tree. This function needs to be called with scheduler lock held.
 */
static int
ice_sched_assoc_vsi_to_agg(struct ice_port_info *pi, u32 agg_id,
			   u16 vsi_handle, ice_bitmap_t *tc_bitmap)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info, *old_agg_vsi_info = NULL;
	struct ice_sched_agg_info *agg_info, *old_agg_info;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u8 tc;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	agg_info = ice_get_agg_info(hw, agg_id);
	if (!agg_info)
		return ICE_ERR_PARAM;
	/* If the vsi is already part of another aggregator then update
	 * its vsi info list
	 */
	old_agg_info = ice_get_vsi_agg_info(hw, vsi_handle);
	if (old_agg_info && old_agg_info != agg_info) {
		struct ice_sched_agg_vsi_info *vtmp;

		LIST_FOR_EACH_ENTRY_SAFE(old_agg_vsi_info, vtmp,
					 &old_agg_info->agg_vsi_list,
					 ice_sched_agg_vsi_info, list_entry)
			if (old_agg_vsi_info->vsi_handle == vsi_handle)
				break;
	}

	/* check if entry already exist */
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info) {
		/* Create new entry for VSI under aggregator list */
		agg_vsi_info = (struct ice_sched_agg_vsi_info *)
			ice_malloc(hw, sizeof(*agg_vsi_info));
		if (!agg_vsi_info)
			return ICE_ERR_PARAM;

		/* add VSI ID into the aggregator list */
		agg_vsi_info->vsi_handle = vsi_handle;
		LIST_ADD(&agg_vsi_info->list_entry, &agg_info->agg_vsi_list);
	}
	/* Move VSI node to new aggregator node for requested TC(s) */
	ice_for_each_traffic_class(tc) {
		if (!ice_is_tc_ena(*tc_bitmap, tc))
			continue;

		/* Move VSI to new aggregator */
		status = ice_sched_move_vsi_to_agg(pi, vsi_handle, agg_id, tc);
		if (status)
			break;

		ice_set_bit(tc, agg_vsi_info->tc_bitmap);
		if (old_agg_vsi_info)
			ice_clear_bit(tc, old_agg_vsi_info->tc_bitmap);
	}
	if (old_agg_vsi_info && !old_agg_vsi_info->tc_bitmap[0]) {
		LIST_DEL(&old_agg_vsi_info->list_entry);
		ice_free(pi->hw, old_agg_vsi_info);
	}
	return status;
}

/**
 * ice_sched_rm_unused_rl_prof - remove unused RL profile
 * @hw: pointer to the hardware structure
 *
 * This function removes unused rate limit profiles from the HW and
 * SW DB. The caller needs to hold scheduler lock.
 */
static void ice_sched_rm_unused_rl_prof(struct ice_hw *hw)
{
	u16 ln;

	for (ln = 0; ln < hw->num_tx_sched_layers; ln++) {
		struct ice_aqc_rl_profile_info *rl_prof_elem;
		struct ice_aqc_rl_profile_info *rl_prof_tmp;

		LIST_FOR_EACH_ENTRY_SAFE(rl_prof_elem, rl_prof_tmp,
					 &hw->rl_prof_list[ln],
					 ice_aqc_rl_profile_info, list_entry) {
			if (!ice_sched_del_rl_profile(hw, rl_prof_elem))
				ice_debug(hw, ICE_DBG_SCHED, "Removed rl profile\n");
		}
	}
}

/**
 * ice_sched_update_elem - update element
 * @hw: pointer to the HW struct
 * @node: pointer to node
 * @info: node info to update
 *
 * Update the HW DB, and local SW DB of node. Update the scheduling
 * parameters of node from argument info data buffer (Info->data buf) and
 * returns success or error on config sched element failure. The caller
 * needs to hold scheduler lock.
 */
static int
ice_sched_update_elem(struct ice_hw *hw, struct ice_sched_node *node,
		      struct ice_aqc_txsched_elem_data *info)
{
	struct ice_aqc_txsched_elem_data buf;
	u16 elem_cfgd = 0;
	u16 num_elems = 1;
	int status;

	buf = *info;
	/* For TC nodes, CIR config is not supported */
	if (node->info.data.elem_type == ICE_AQC_ELEM_TYPE_TC)
		buf.data.valid_sections &= ~ICE_AQC_ELEM_VALID_CIR;
	/* Parent TEID is reserved field in this aq call */
	buf.parent_teid = 0;
	/* Element type is reserved field in this aq call */
	buf.data.elem_type = 0;
	/* Flags is reserved field in this aq call */
	buf.data.flags = 0;

	/* Update HW DB */
	/* Configure element node */
	status = ice_aq_cfg_sched_elems(hw, num_elems, &buf, sizeof(buf),
					&elem_cfgd, NULL);
	if (status || elem_cfgd != num_elems) {
		ice_debug(hw, ICE_DBG_SCHED, "Config sched elem error\n");
		return ICE_ERR_CFG;
	}

	/* Config success case */
	/* Now update local SW DB */
	/* Only copy the data portion of info buffer */
	node->info.data = info->data;
	return status;
}

/**
 * ice_sched_cfg_node_bw_alloc - configure node BW weight/alloc params
 * @hw: pointer to the HW struct
 * @node: sched node to configure
 * @rl_type: rate limit type CIR, EIR, or shared
 * @bw_alloc: BW weight/allocation
 *
 * This function configures node element's BW allocation.
 */
static int
ice_sched_cfg_node_bw_alloc(struct ice_hw *hw, struct ice_sched_node *node,
			    enum ice_rl_type rl_type, u16 bw_alloc)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;
	int status;

	buf = node->info;
	data = &buf.data;
	if (rl_type == ICE_MIN_BW) {
		data->valid_sections |= ICE_AQC_ELEM_VALID_CIR;
		data->cir_bw.bw_alloc = CPU_TO_LE16(bw_alloc);
	} else if (rl_type == ICE_MAX_BW) {
		data->valid_sections |= ICE_AQC_ELEM_VALID_EIR;
		data->eir_bw.bw_alloc = CPU_TO_LE16(bw_alloc);
	} else {
		return ICE_ERR_PARAM;
	}

	/* Configure element */
	status = ice_sched_update_elem(hw, node, &buf);
	return status;
}

/**
 * ice_move_vsi_to_agg - moves VSI to new or default aggregator
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @vsi_handle: software VSI handle
 * @tc_bitmap: TC bitmap of enabled TC(s)
 *
 * Move or associate VSI to a new or default aggregator node.
 */
int
ice_move_vsi_to_agg(struct ice_port_info *pi, u32 agg_id, u16 vsi_handle,
		    u8 tc_bitmap)
{
	ice_bitmap_t bitmap = tc_bitmap;
	int status;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_assoc_vsi_to_agg(pi, agg_id, vsi_handle,
					    (ice_bitmap_t *)&bitmap);
	if (!status)
		status = ice_save_agg_vsi_tc_bitmap(pi, agg_id, vsi_handle,
						    (ice_bitmap_t *)&bitmap);
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_rm_agg_cfg - remove aggregator configuration
 * @pi: port information structure
 * @agg_id: aggregator ID
 *
 * This function removes aggregator reference to VSI and delete aggregator ID
 * info. It removes the aggregator configuration completely.
 */
int ice_rm_agg_cfg(struct ice_port_info *pi, u32 agg_id)
{
	struct ice_sched_agg_info *agg_info;
	int status = 0;
	u8 tc;

	ice_acquire_lock(&pi->sched_lock);
	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info) {
		status = ICE_ERR_DOES_NOT_EXIST;
		goto exit_ice_rm_agg_cfg;
	}

	ice_for_each_traffic_class(tc) {
		status = ice_rm_agg_cfg_tc(pi, agg_info, tc, true);
		if (status)
			goto exit_ice_rm_agg_cfg;
	}

	if (ice_is_any_bit_set(agg_info->tc_bitmap, ICE_MAX_TRAFFIC_CLASS)) {
		status = ICE_ERR_IN_USE;
		goto exit_ice_rm_agg_cfg;
	}

	/* Safe to delete entry now */
	LIST_DEL(&agg_info->list_entry);
	ice_free(pi->hw, agg_info);

	/* Remove unused RL profile IDs from HW and SW DB */
	ice_sched_rm_unused_rl_prof(pi->hw);

exit_ice_rm_agg_cfg:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_set_clear_cir_bw_alloc - set or clear CIR BW alloc information
 * @bw_t_info: bandwidth type information structure
 * @bw_alloc: Bandwidth allocation information
 *
 * Save or clear CIR BW alloc information (bw_alloc) in the passed param
 * bw_t_info.
 */
static void
ice_set_clear_cir_bw_alloc(struct ice_bw_type_info *bw_t_info, u16 bw_alloc)
{
	bw_t_info->cir_bw.bw_alloc = bw_alloc;
	if (bw_t_info->cir_bw.bw_alloc)
		ice_set_bit(ICE_BW_TYPE_CIR_WT, bw_t_info->bw_t_bitmap);
	else
		ice_clear_bit(ICE_BW_TYPE_CIR_WT, bw_t_info->bw_t_bitmap);
}

/**
 * ice_set_clear_eir_bw_alloc - set or clear EIR BW alloc information
 * @bw_t_info: bandwidth type information structure
 * @bw_alloc: Bandwidth allocation information
 *
 * Save or clear EIR BW alloc information (bw_alloc) in the passed param
 * bw_t_info.
 */
static void
ice_set_clear_eir_bw_alloc(struct ice_bw_type_info *bw_t_info, u16 bw_alloc)
{
	bw_t_info->eir_bw.bw_alloc = bw_alloc;
	if (bw_t_info->eir_bw.bw_alloc)
		ice_set_bit(ICE_BW_TYPE_EIR_WT, bw_t_info->bw_t_bitmap);
	else
		ice_clear_bit(ICE_BW_TYPE_EIR_WT, bw_t_info->bw_t_bitmap);
}

/**
 * ice_sched_save_vsi_bw_alloc - save VSI node's BW alloc information
 * @pi: port information structure
 * @vsi_handle: sw VSI handle
 * @tc: traffic class
 * @rl_type: rate limit type min or max
 * @bw_alloc: Bandwidth allocation information
 *
 * Save BW alloc information of VSI type node for post replay use.
 */
static int
ice_sched_save_vsi_bw_alloc(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			    enum ice_rl_type rl_type, u16 bw_alloc)
{
	struct ice_vsi_ctx *vsi_ctx;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw_alloc(&vsi_ctx->sched.bw_t_info[tc],
					   bw_alloc);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw_alloc(&vsi_ctx->sched.bw_t_info[tc],
					   bw_alloc);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_set_clear_cir_bw - set or clear CIR BW
 * @bw_t_info: bandwidth type information structure
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save or clear CIR bandwidth (BW) in the passed param bw_t_info.
 */
static void ice_set_clear_cir_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		ice_clear_bit(ICE_BW_TYPE_CIR, bw_t_info->bw_t_bitmap);
		bw_t_info->cir_bw.bw = 0;
	} else {
		/* Save type of BW information */
		ice_set_bit(ICE_BW_TYPE_CIR, bw_t_info->bw_t_bitmap);
		bw_t_info->cir_bw.bw = bw;
	}
}

/**
 * ice_set_clear_eir_bw - set or clear EIR BW
 * @bw_t_info: bandwidth type information structure
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save or clear EIR bandwidth (BW) in the passed param bw_t_info.
 */
static void ice_set_clear_eir_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		ice_clear_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap);
		bw_t_info->eir_bw.bw = 0;
	} else {
		/* save EIR BW information */
		ice_set_bit(ICE_BW_TYPE_EIR, bw_t_info->bw_t_bitmap);
		bw_t_info->eir_bw.bw = bw;
	}
}

/**
 * ice_set_clear_shared_bw - set or clear shared BW
 * @bw_t_info: bandwidth type information structure
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save or clear shared bandwidth (BW) in the passed param bw_t_info.
 */
static void ice_set_clear_shared_bw(struct ice_bw_type_info *bw_t_info, u32 bw)
{
	if (bw == ICE_SCHED_DFLT_BW) {
		ice_clear_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap);
		bw_t_info->shared_bw = 0;
	} else {
		/* save shared BW information */
		ice_set_bit(ICE_BW_TYPE_SHARED, bw_t_info->bw_t_bitmap);
		bw_t_info->shared_bw = bw;
	}
}

/**
 * ice_sched_save_vsi_bw - save VSI node's BW information
 * @pi: port information structure
 * @vsi_handle: sw VSI handle
 * @tc: traffic class
 * @rl_type: rate limit type min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save BW information of VSI type node for post replay use.
 */
static int
ice_sched_save_vsi_bw(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      enum ice_rl_type rl_type, u32 bw)
{
	struct ice_vsi_ctx *vsi_ctx;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&vsi_ctx->sched.bw_t_info[tc], bw);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_set_clear_prio - set or clear priority information
 * @bw_t_info: bandwidth type information structure
 * @prio: priority to save
 *
 * Save or clear priority (prio) in the passed param bw_t_info.
 */
static void ice_set_clear_prio(struct ice_bw_type_info *bw_t_info, u8 prio)
{
	bw_t_info->generic = prio;
	if (bw_t_info->generic)
		ice_set_bit(ICE_BW_TYPE_PRIO, bw_t_info->bw_t_bitmap);
	else
		ice_clear_bit(ICE_BW_TYPE_PRIO, bw_t_info->bw_t_bitmap);
}

/**
 * ice_sched_save_vsi_prio - save VSI node's priority information
 * @pi: port information structure
 * @vsi_handle: Software VSI handle
 * @tc: traffic class
 * @prio: priority to save
 *
 * Save priority information of VSI type node for post replay use.
 */
static int
ice_sched_save_vsi_prio(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			u8 prio)
{
	struct ice_vsi_ctx *vsi_ctx;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	if (tc >= ICE_MAX_TRAFFIC_CLASS)
		return ICE_ERR_PARAM;
	ice_set_clear_prio(&vsi_ctx->sched.bw_t_info[tc], prio);
	return 0;
}

/**
 * ice_sched_save_agg_bw_alloc - save aggregator node's BW alloc information
 * @pi: port information structure
 * @agg_id: node aggregator ID
 * @tc: traffic class
 * @rl_type: rate limit type min or max
 * @bw_alloc: bandwidth alloc information
 *
 * Save BW alloc information of AGG type node for post replay use.
 */
static int
ice_sched_save_agg_bw_alloc(struct ice_port_info *pi, u32 agg_id, u8 tc,
			    enum ice_rl_type rl_type, u16 bw_alloc)
{
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return ICE_ERR_PARAM;
	if (!ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw_alloc(&agg_info->bw_t_info[tc], bw_alloc);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw_alloc(&agg_info->bw_t_info[tc], bw_alloc);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_sched_save_agg_bw - save aggregator node's BW information
 * @pi: port information structure
 * @agg_id: node aggregator ID
 * @tc: traffic class
 * @rl_type: rate limit type min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save BW information of AGG type node for post replay use.
 */
static int
ice_sched_save_agg_bw(struct ice_port_info *pi, u32 agg_id, u8 tc,
		      enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_agg_info *agg_info;

	agg_info = ice_get_agg_info(pi->hw, agg_id);
	if (!agg_info)
		return ICE_ERR_PARAM;
	if (!ice_is_tc_ena(agg_info->tc_bitmap[0], tc))
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&agg_info->bw_t_info[tc], bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&agg_info->bw_t_info[tc], bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&agg_info->bw_t_info[tc], bw);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_cfg_vsi_bw_lmt_per_tc - configure VSI BW limit per TC
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: traffic class
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function configures BW limit of VSI scheduling node based on TC
 * information.
 */
int
ice_cfg_vsi_bw_lmt_per_tc(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			  enum ice_rl_type rl_type, u32 bw)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, vsi_handle,
						  ICE_AGG_TYPE_VSI,
						  tc, rl_type, bw);
	if (!status) {
		ice_acquire_lock(&pi->sched_lock);
		status = ice_sched_save_vsi_bw(pi, vsi_handle, tc, rl_type, bw);
		ice_release_lock(&pi->sched_lock);
	}
	return status;
}

/**
 * ice_cfg_vsi_bw_dflt_lmt_per_tc - configure default VSI BW limit per TC
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: traffic class
 * @rl_type: min or max
 *
 * This function configures default BW limit of VSI scheduling node based on TC
 * information.
 */
int
ice_cfg_vsi_bw_dflt_lmt_per_tc(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
			       enum ice_rl_type rl_type)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, vsi_handle,
						  ICE_AGG_TYPE_VSI,
						  tc, rl_type,
						  ICE_SCHED_DFLT_BW);
	if (!status) {
		ice_acquire_lock(&pi->sched_lock);
		status = ice_sched_save_vsi_bw(pi, vsi_handle, tc, rl_type,
					       ICE_SCHED_DFLT_BW);
		ice_release_lock(&pi->sched_lock);
	}
	return status;
}

/**
 * ice_cfg_agg_bw_lmt_per_tc - configure aggregator BW limit per TC
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function applies BW limit to aggregator scheduling node based on TC
 * information.
 */
int
ice_cfg_agg_bw_lmt_per_tc(struct ice_port_info *pi, u32 agg_id, u8 tc,
			  enum ice_rl_type rl_type, u32 bw)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, agg_id, ICE_AGG_TYPE_AGG,
						  tc, rl_type, bw);
	if (!status) {
		ice_acquire_lock(&pi->sched_lock);
		status = ice_sched_save_agg_bw(pi, agg_id, tc, rl_type, bw);
		ice_release_lock(&pi->sched_lock);
	}
	return status;
}

/**
 * ice_cfg_agg_bw_dflt_lmt_per_tc - configure aggregator BW default limit per TC
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @rl_type: min or max
 *
 * This function applies default BW limit to aggregator scheduling node based
 * on TC information.
 */
int
ice_cfg_agg_bw_dflt_lmt_per_tc(struct ice_port_info *pi, u32 agg_id, u8 tc,
			       enum ice_rl_type rl_type)
{
	int status;

	status = ice_sched_set_node_bw_lmt_per_tc(pi, agg_id, ICE_AGG_TYPE_AGG,
						  tc, rl_type,
						  ICE_SCHED_DFLT_BW);
	if (!status) {
		ice_acquire_lock(&pi->sched_lock);
		status = ice_sched_save_agg_bw(pi, agg_id, tc, rl_type,
					       ICE_SCHED_DFLT_BW);
		ice_release_lock(&pi->sched_lock);
	}
	return status;
}

/**
 * ice_cfg_vsi_bw_shared_lmt - configure VSI BW shared limit
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * Configure shared rate limiter(SRL) of all VSI type nodes across all traffic
 * classes for VSI matching handle.
 */
int
ice_cfg_vsi_bw_shared_lmt(struct ice_port_info *pi, u16 vsi_handle, u32 min_bw,
			  u32 max_bw, u32 shared_bw)
{
	return ice_sched_set_vsi_bw_shared_lmt(pi, vsi_handle, min_bw, max_bw,
					       shared_bw);
}

/**
 * ice_cfg_vsi_bw_no_shared_lmt - configure VSI BW for no shared limiter
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function removes the shared rate limiter(SRL) of all VSI type nodes
 * across all traffic classes for VSI matching handle.
 */
int
ice_cfg_vsi_bw_no_shared_lmt(struct ice_port_info *pi, u16 vsi_handle)
{
	return ice_sched_set_vsi_bw_shared_lmt(pi, vsi_handle,
					       ICE_SCHED_DFLT_BW,
					       ICE_SCHED_DFLT_BW,
					       ICE_SCHED_DFLT_BW);
}

/**
 * ice_cfg_agg_bw_shared_lmt - configure aggregator BW shared limit
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * This function configures the shared rate limiter(SRL) of all aggregator type
 * nodes across all traffic classes for aggregator matching agg_id.
 */
int
ice_cfg_agg_bw_shared_lmt(struct ice_port_info *pi, u32 agg_id, u32 min_bw,
			  u32 max_bw, u32 shared_bw)
{
	return ice_sched_set_agg_bw_shared_lmt(pi, agg_id, min_bw, max_bw,
					       shared_bw);
}

/**
 * ice_cfg_agg_bw_no_shared_lmt - configure aggregator BW for no shared limiter
 * @pi: port information structure
 * @agg_id: aggregator ID
 *
 * This function removes the shared rate limiter(SRL) of all aggregator type
 * nodes across all traffic classes for aggregator matching agg_id.
 */
int
ice_cfg_agg_bw_no_shared_lmt(struct ice_port_info *pi, u32 agg_id)
{
	return ice_sched_set_agg_bw_shared_lmt(pi, agg_id, ICE_SCHED_DFLT_BW,
					       ICE_SCHED_DFLT_BW,
					       ICE_SCHED_DFLT_BW);
}

/**
 * ice_cfg_agg_bw_shared_lmt_per_tc - config aggregator BW shared limit per tc
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * This function configures the shared rate limiter(SRL) of all aggregator type
 * nodes across all traffic classes for aggregator matching agg_id.
 */
int
ice_cfg_agg_bw_shared_lmt_per_tc(struct ice_port_info *pi, u32 agg_id, u8 tc,
				 u32 min_bw, u32 max_bw, u32 shared_bw)
{
	return ice_sched_set_agg_bw_shared_lmt_per_tc(pi, agg_id, tc, min_bw,
						      max_bw, shared_bw);
}

/**
 * ice_cfg_agg_bw_no_shared_lmt_per_tc - cfg aggregator BW shared limit per tc
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 *
 * This function configures the shared rate limiter(SRL) of all aggregator type
 * nodes across all traffic classes for aggregator matching agg_id.
 */
int
ice_cfg_agg_bw_no_shared_lmt_per_tc(struct ice_port_info *pi, u32 agg_id, u8 tc)
{
	return ice_sched_set_agg_bw_shared_lmt_per_tc(pi, agg_id, tc,
						      ICE_SCHED_DFLT_BW,
						      ICE_SCHED_DFLT_BW,
						      ICE_SCHED_DFLT_BW);
}

/**
 * ice_cfg_vsi_q_priority - config VSI queue priority of node
 * @pi: port information structure
 * @num_qs: number of VSI queues
 * @q_ids: queue IDs array
 * @q_prio: queue priority array
 *
 * This function configures the queue node priority (Sibling Priority) of the
 * passed in VSI's queue(s) for a given traffic class (TC).
 */
int
ice_cfg_vsi_q_priority(struct ice_port_info *pi, u16 num_qs, u32 *q_ids,
		       u8 *q_prio)
{
	int status = ICE_ERR_PARAM;
	u16 i;

	ice_acquire_lock(&pi->sched_lock);

	for (i = 0; i < num_qs; i++) {
		struct ice_sched_node *node;

		node = ice_sched_find_node_by_teid(pi->root, q_ids[i]);
		if (!node || node->info.data.elem_type !=
		    ICE_AQC_ELEM_TYPE_LEAF) {
			status = ICE_ERR_PARAM;
			break;
		}
		/* Configure Priority */
		status = ice_sched_cfg_sibl_node_prio(pi, node, q_prio[i]);
		if (status)
			break;
	}

	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_agg_vsi_priority_per_tc - config aggregator's VSI priority per TC
 * @pi: port information structure
 * @agg_id: Aggregator ID
 * @num_vsis: number of VSI(s)
 * @vsi_handle_arr: array of software VSI handles
 * @node_prio: pointer to node priority
 * @tc: traffic class
 *
 * This function configures the node priority (Sibling Priority) of the
 * passed in VSI's for a given traffic class (TC) of an Aggregator ID.
 */
int
ice_cfg_agg_vsi_priority_per_tc(struct ice_port_info *pi, u32 agg_id,
				u16 num_vsis, u16 *vsi_handle_arr,
				u8 *node_prio, u8 tc)
{
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_sched_node *tc_node, *agg_node;
	struct ice_sched_agg_info *agg_info;
	bool agg_id_present = false;
	struct ice_hw *hw = pi->hw;
	int status = ICE_ERR_PARAM;
	u16 i;

	ice_acquire_lock(&pi->sched_lock);
	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry)
		if (agg_info->agg_id == agg_id) {
			agg_id_present = true;
			break;
		}
	if (!agg_id_present)
		goto exit_agg_priority_per_tc;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		goto exit_agg_priority_per_tc;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		goto exit_agg_priority_per_tc;

	if (num_vsis > hw->max_children[agg_node->tx_sched_layer])
		goto exit_agg_priority_per_tc;

	for (i = 0; i < num_vsis; i++) {
		struct ice_sched_node *vsi_node;
		bool vsi_handle_valid = false;
		u16 vsi_handle;

		status = ICE_ERR_PARAM;
		vsi_handle = vsi_handle_arr[i];
		if (!ice_is_vsi_valid(hw, vsi_handle))
			goto exit_agg_priority_per_tc;
		/* Verify child nodes before applying settings */
		LIST_FOR_EACH_ENTRY(agg_vsi_info, &agg_info->agg_vsi_list,
				    ice_sched_agg_vsi_info, list_entry)
			if (agg_vsi_info->vsi_handle == vsi_handle) {
				vsi_handle_valid = true;
				break;
			}

		if (!vsi_handle_valid)
			goto exit_agg_priority_per_tc;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			goto exit_agg_priority_per_tc;

		if (ice_sched_find_node_in_subtree(hw, agg_node, vsi_node)) {
			/* Configure Priority */
			status = ice_sched_cfg_sibl_node_prio(pi, vsi_node,
							      node_prio[i]);
			if (status)
				break;
			status = ice_sched_save_vsi_prio(pi, vsi_handle, tc,
							 node_prio[i]);
			if (status)
				break;
		}
	}

exit_agg_priority_per_tc:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_vsi_bw_alloc - config VSI BW alloc per TC
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @ena_tcmap: enabled TC map
 * @rl_type: Rate limit type CIR/EIR
 * @bw_alloc: Array of BW alloc
 *
 * This function configures the BW allocation of the passed in VSI's
 * node(s) for enabled traffic class.
 */
int
ice_cfg_vsi_bw_alloc(struct ice_port_info *pi, u16 vsi_handle, u8 ena_tcmap,
		     enum ice_rl_type rl_type, u8 *bw_alloc)
{
	int status = 0;
	u8 tc;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;

	ice_acquire_lock(&pi->sched_lock);

	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *vsi_node;

		if (!ice_is_tc_ena(ena_tcmap, tc))
			continue;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;

		status = ice_sched_cfg_node_bw_alloc(pi->hw, vsi_node, rl_type,
						     bw_alloc[tc]);
		if (status)
			break;
		status = ice_sched_save_vsi_bw_alloc(pi, vsi_handle, tc,
						     rl_type, bw_alloc[tc]);
		if (status)
			break;
	}

	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_agg_bw_alloc - config aggregator BW alloc
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @ena_tcmap: enabled TC map
 * @rl_type: rate limit type CIR/EIR
 * @bw_alloc: array of BW alloc
 *
 * This function configures the BW allocation of passed in aggregator for
 * enabled traffic class(s).
 */
int
ice_cfg_agg_bw_alloc(struct ice_port_info *pi, u32 agg_id, u8 ena_tcmap,
		     enum ice_rl_type rl_type, u8 *bw_alloc)
{
	struct ice_sched_agg_info *agg_info;
	bool agg_id_present = false;
	struct ice_hw *hw = pi->hw;
	int status = 0;
	u8 tc;

	ice_acquire_lock(&pi->sched_lock);
	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry)
		if (agg_info->agg_id == agg_id) {
			agg_id_present = true;
			break;
		}
	if (!agg_id_present) {
		status = ICE_ERR_PARAM;
		goto exit_cfg_agg_bw_alloc;
	}

	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *agg_node;

		if (!ice_is_tc_ena(ena_tcmap, tc))
			continue;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
		if (!agg_node)
			continue;

		status = ice_sched_cfg_node_bw_alloc(hw, agg_node, rl_type,
						     bw_alloc[tc]);
		if (status)
			break;
		status = ice_sched_save_agg_bw_alloc(pi, agg_id, tc, rl_type,
						     bw_alloc[tc]);
		if (status)
			break;
	}

exit_cfg_agg_bw_alloc:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_calc_wakeup - calculate RL profile wakeup parameter
 * @hw: pointer to the HW struct
 * @bw: bandwidth in Kbps
 *
 * This function calculates the wakeup parameter of RL profile.
 */
static u16 ice_sched_calc_wakeup(struct ice_hw *hw, s32 bw)
{
	s64 bytes_per_sec, wakeup_int, wakeup_a, wakeup_b, wakeup_f;
	s32 wakeup_f_int;
	u16 wakeup = 0;

	/* Get the wakeup integer value */
	bytes_per_sec = DIV_S64((s64)bw * 1000, BITS_PER_BYTE);
	wakeup_int = DIV_S64(hw->psm_clk_freq, bytes_per_sec);
	if (wakeup_int > 63) {
		wakeup = (u16)((1 << 15) | wakeup_int);
	} else {
		/* Calculate fraction value up to 4 decimals
		 * Convert Integer value to a constant multiplier
		 */
		wakeup_b = (s64)ICE_RL_PROF_MULTIPLIER * wakeup_int;
		wakeup_a = DIV_S64((s64)ICE_RL_PROF_MULTIPLIER *
				   hw->psm_clk_freq, bytes_per_sec);

		/* Get Fraction value */
		wakeup_f = wakeup_a - wakeup_b;

		/* Round up the Fractional value via Ceil(Fractional value) */
		if (wakeup_f > DIV_S64(ICE_RL_PROF_MULTIPLIER, 2))
			wakeup_f += 1;

		wakeup_f_int = (s32)DIV_S64(wakeup_f * ICE_RL_PROF_FRACTION,
					    ICE_RL_PROF_MULTIPLIER);
		wakeup |= (u16)(wakeup_int << 9);
		wakeup |= (u16)(0x1ff & wakeup_f_int);
	}

	return wakeup;
}

/**
 * ice_sched_bw_to_rl_profile - convert BW to profile parameters
 * @hw: pointer to the HW struct
 * @bw: bandwidth in Kbps
 * @profile: profile parameters to return
 *
 * This function converts the BW to profile structure format.
 */
static int
ice_sched_bw_to_rl_profile(struct ice_hw *hw, u32 bw,
			   struct ice_aqc_rl_profile_elem *profile)
{
	s64 bytes_per_sec, ts_rate, mv_tmp;
	int status = ICE_ERR_PARAM;
	bool found = false;
	s32 encode = 0;
	s64 mv = 0;
	s32 i;

	/* Bw settings range is from 0.5Mb/sec to 100Gb/sec */
	if (bw < ICE_SCHED_MIN_BW || bw > ICE_SCHED_MAX_BW)
		return status;

	/* Bytes per second from Kbps */
	bytes_per_sec = DIV_S64((s64)bw * 1000, BITS_PER_BYTE);

	/* encode is 6 bits but really useful are 5 bits */
	for (i = 0; i < 64; i++) {
		u64 pow_result = BIT_ULL(i);

		ts_rate = DIV_S64((s64)hw->psm_clk_freq,
				  pow_result * ICE_RL_PROF_TS_MULTIPLIER);
		if (ts_rate <= 0)
			continue;

		/* Multiplier value */
		mv_tmp = DIV_S64(bytes_per_sec * ICE_RL_PROF_MULTIPLIER,
				 ts_rate);

		/* Round to the nearest ICE_RL_PROF_MULTIPLIER */
		mv = round_up_64bit(mv_tmp, ICE_RL_PROF_MULTIPLIER);

		/* First multiplier value greater than the given
		 * accuracy bytes
		 */
		if (mv > ICE_RL_PROF_ACCURACY_BYTES) {
			encode = i;
			found = true;
			break;
		}
	}
	if (found) {
		u16 wm;

		wm = ice_sched_calc_wakeup(hw, bw);
		profile->rl_multiply = CPU_TO_LE16(mv);
		profile->wake_up_calc = CPU_TO_LE16(wm);
		profile->rl_encode = CPU_TO_LE16(encode);
		status = 0;
	} else {
		status = ICE_ERR_DOES_NOT_EXIST;
	}

	return status;
}

/**
 * ice_sched_add_rl_profile - add RL profile
 * @hw: pointer to the hardware structure
 * @rl_type: type of rate limit BW - min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 * @layer_num: specifies in which layer to create profile
 *
 * This function first checks the existing list for corresponding BW
 * parameter. If it exists, it returns the associated profile otherwise
 * it creates a new rate limit profile for requested BW, and adds it to
 * the HW DB and local list. It returns the new profile or null on error.
 * The caller needs to hold the scheduler lock.
 */
static struct ice_aqc_rl_profile_info *
ice_sched_add_rl_profile(struct ice_hw *hw, enum ice_rl_type rl_type,
			 u32 bw, u8 layer_num)
{
	struct ice_aqc_rl_profile_info *rl_prof_elem;
	u16 profiles_added = 0, num_profiles = 1;
	struct ice_aqc_rl_profile_elem *buf;
	u8 profile_type;
	int status;

	if (!hw || layer_num >= hw->num_tx_sched_layers)
		return NULL;
	switch (rl_type) {
	case ICE_MIN_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_CIR;
		break;
	case ICE_MAX_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_EIR;
		break;
	case ICE_SHARED_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_SRL;
		break;
	default:
		return NULL;
	}

	LIST_FOR_EACH_ENTRY(rl_prof_elem, &hw->rl_prof_list[layer_num],
			    ice_aqc_rl_profile_info, list_entry)
		if ((rl_prof_elem->profile.flags & ICE_AQC_RL_PROFILE_TYPE_M) ==
		    profile_type && rl_prof_elem->bw == bw)
			/* Return existing profile ID info */
			return rl_prof_elem;

	/* Create new profile ID */
	rl_prof_elem = (struct ice_aqc_rl_profile_info *)
		ice_malloc(hw, sizeof(*rl_prof_elem));

	if (!rl_prof_elem)
		return NULL;

	status = ice_sched_bw_to_rl_profile(hw, bw, &rl_prof_elem->profile);
	if (status)
		goto exit_add_rl_prof;

	rl_prof_elem->bw = bw;
	/* layer_num is zero relative, and fw expects level from 1 to 9 */
	rl_prof_elem->profile.level = layer_num + 1;
	rl_prof_elem->profile.flags = profile_type;
	rl_prof_elem->profile.max_burst_size = CPU_TO_LE16(hw->max_burst_size);

	/* Create new entry in HW DB */
	buf = &rl_prof_elem->profile;
	status = ice_aq_add_rl_profile(hw, num_profiles, buf, sizeof(*buf),
				       &profiles_added, NULL);
	if (status || profiles_added != num_profiles)
		goto exit_add_rl_prof;

	/* Good entry - add in the list */
	rl_prof_elem->prof_id_ref = 0;
	LIST_ADD(&rl_prof_elem->list_entry, &hw->rl_prof_list[layer_num]);
	return rl_prof_elem;

exit_add_rl_prof:
	ice_free(hw, rl_prof_elem);
	return NULL;
}

/**
 * ice_sched_cfg_node_bw_lmt - configure node sched params
 * @hw: pointer to the HW struct
 * @node: sched node to configure
 * @rl_type: rate limit type CIR, EIR, or shared
 * @rl_prof_id: rate limit profile ID
 *
 * This function configures node element's BW limit.
 */
static int
ice_sched_cfg_node_bw_lmt(struct ice_hw *hw, struct ice_sched_node *node,
			  enum ice_rl_type rl_type, u16 rl_prof_id)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;
	switch (rl_type) {
	case ICE_MIN_BW:
		data->valid_sections |= ICE_AQC_ELEM_VALID_CIR;
		data->cir_bw.bw_profile_idx = CPU_TO_LE16(rl_prof_id);
		break;
	case ICE_MAX_BW:
		data->valid_sections |= ICE_AQC_ELEM_VALID_EIR;
		data->eir_bw.bw_profile_idx = CPU_TO_LE16(rl_prof_id);
		break;
	case ICE_SHARED_BW:
		data->valid_sections |= ICE_AQC_ELEM_VALID_SHARED;
		data->srl_id = CPU_TO_LE16(rl_prof_id);
		break;
	default:
		/* Unknown rate limit type */
		return ICE_ERR_PARAM;
	}

	/* Configure element */
	return ice_sched_update_elem(hw, node, &buf);
}

/**
 * ice_sched_get_node_rl_prof_id - get node's rate limit profile ID
 * @node: sched node
 * @rl_type: rate limit type
 *
 * If existing profile matches, it returns the corresponding rate
 * limit profile ID, otherwise it returns an invalid ID as error.
 */
static u16
ice_sched_get_node_rl_prof_id(struct ice_sched_node *node,
			      enum ice_rl_type rl_type)
{
	u16 rl_prof_id = ICE_SCHED_INVAL_PROF_ID;
	struct ice_aqc_txsched_elem *data;

	data = &node->info.data;
	switch (rl_type) {
	case ICE_MIN_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_CIR)
			rl_prof_id = LE16_TO_CPU(data->cir_bw.bw_profile_idx);
		break;
	case ICE_MAX_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_EIR)
			rl_prof_id = LE16_TO_CPU(data->eir_bw.bw_profile_idx);
		break;
	case ICE_SHARED_BW:
		if (data->valid_sections & ICE_AQC_ELEM_VALID_SHARED)
			rl_prof_id = LE16_TO_CPU(data->srl_id);
		break;
	default:
		break;
	}

	return rl_prof_id;
}

/**
 * ice_sched_get_rl_prof_layer - selects rate limit profile creation layer
 * @pi: port information structure
 * @rl_type: type of rate limit BW - min, max, or shared
 * @layer_index: layer index
 *
 * This function returns requested profile creation layer.
 */
static u8
ice_sched_get_rl_prof_layer(struct ice_port_info *pi, enum ice_rl_type rl_type,
			    u8 layer_index)
{
	struct ice_hw *hw = pi->hw;

	if (layer_index >= hw->num_tx_sched_layers)
		return ICE_SCHED_INVAL_LAYER_NUM;
	switch (rl_type) {
	case ICE_MIN_BW:
		if (hw->layer_info[layer_index].max_cir_rl_profiles)
			return layer_index;
		break;
	case ICE_MAX_BW:
		if (hw->layer_info[layer_index].max_eir_rl_profiles)
			return layer_index;
		break;
	case ICE_SHARED_BW:
		/* if current layer doesn't support SRL profile creation
		 * then try a layer up or down.
		 */
		if (hw->layer_info[layer_index].max_srl_profiles)
			return layer_index;
		else if (layer_index < hw->num_tx_sched_layers - 1 &&
			 hw->layer_info[layer_index + 1].max_srl_profiles)
			return layer_index + 1;
		else if (layer_index > 0 &&
			 hw->layer_info[layer_index - 1].max_srl_profiles)
			return layer_index - 1;
		break;
	default:
		break;
	}
	return ICE_SCHED_INVAL_LAYER_NUM;
}

/**
 * ice_sched_get_srl_node - get shared rate limit node
 * @node: tree node
 * @srl_layer: shared rate limit layer
 *
 * This function returns SRL node to be used for shared rate limit purpose.
 * The caller needs to hold scheduler lock.
 */
static struct ice_sched_node *
ice_sched_get_srl_node(struct ice_sched_node *node, u8 srl_layer)
{
	if (srl_layer > node->tx_sched_layer)
		return node->children[0];
	else if (srl_layer < node->tx_sched_layer)
		/* Node can't be created without a parent. It will always
		 * have a valid parent except root node.
		 */
		return node->parent;
	else
		return node;
}

/**
 * ice_sched_rm_rl_profile - remove RL profile ID
 * @hw: pointer to the hardware structure
 * @layer_num: layer number where profiles are saved
 * @profile_type: profile type like EIR, CIR, or SRL
 * @profile_id: profile ID to remove
 *
 * This function removes rate limit profile from layer 'layer_num' of type
 * 'profile_type' and profile ID as 'profile_id'. The caller needs to hold
 * scheduler lock.
 */
static int
ice_sched_rm_rl_profile(struct ice_hw *hw, u8 layer_num, u8 profile_type,
			u16 profile_id)
{
	struct ice_aqc_rl_profile_info *rl_prof_elem;
	int status = 0;

	if (!hw || layer_num >= hw->num_tx_sched_layers)
		return ICE_ERR_PARAM;
	/* Check the existing list for RL profile */
	LIST_FOR_EACH_ENTRY(rl_prof_elem, &hw->rl_prof_list[layer_num],
			    ice_aqc_rl_profile_info, list_entry)
		if ((rl_prof_elem->profile.flags & ICE_AQC_RL_PROFILE_TYPE_M) ==
		    profile_type &&
		    LE16_TO_CPU(rl_prof_elem->profile.profile_id) ==
		    profile_id) {
			if (rl_prof_elem->prof_id_ref)
				rl_prof_elem->prof_id_ref--;

			/* Remove old profile ID from database */
			status = ice_sched_del_rl_profile(hw, rl_prof_elem);
			if (status && status != ICE_ERR_IN_USE)
				ice_debug(hw, ICE_DBG_SCHED, "Remove rl profile failed\n");
			break;
		}
	if (status == ICE_ERR_IN_USE)
		status = 0;
	return status;
}

/**
 * ice_sched_set_node_bw_dflt - set node's bandwidth limit to default
 * @pi: port information structure
 * @node: pointer to node structure
 * @rl_type: rate limit type min, max, or shared
 * @layer_num: layer number where RL profiles are saved
 *
 * This function configures node element's BW rate limit profile ID of
 * type CIR, EIR, or SRL to default. This function needs to be called
 * with the scheduler lock held.
 */
static int
ice_sched_set_node_bw_dflt(struct ice_port_info *pi,
			   struct ice_sched_node *node,
			   enum ice_rl_type rl_type, u8 layer_num)
{
	struct ice_hw *hw;
	u8 profile_type;
	u16 rl_prof_id;
	int status;
	u16 old_id;

	hw = pi->hw;
	switch (rl_type) {
	case ICE_MIN_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_CIR;
		rl_prof_id = ICE_SCHED_DFLT_RL_PROF_ID;
		break;
	case ICE_MAX_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_EIR;
		rl_prof_id = ICE_SCHED_DFLT_RL_PROF_ID;
		break;
	case ICE_SHARED_BW:
		profile_type = ICE_AQC_RL_PROFILE_TYPE_SRL;
		/* No SRL is configured for default case */
		rl_prof_id = ICE_SCHED_NO_SHARED_RL_PROF_ID;
		break;
	default:
		return ICE_ERR_PARAM;
	}
	/* Save existing RL prof ID for later clean up */
	old_id = ice_sched_get_node_rl_prof_id(node, rl_type);
	/* Configure BW scheduling parameters */
	status = ice_sched_cfg_node_bw_lmt(hw, node, rl_type, rl_prof_id);
	if (status)
		return status;

	/* Remove stale RL profile ID */
	if (old_id == ICE_SCHED_DFLT_RL_PROF_ID ||
	    old_id == ICE_SCHED_INVAL_PROF_ID)
		return 0;

	return ice_sched_rm_rl_profile(hw, layer_num, profile_type, old_id);
}

/**
 * ice_sched_set_node_bw - set node's bandwidth
 * @pi: port information structure
 * @node: tree node
 * @rl_type: rate limit type min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 * @layer_num: layer number
 *
 * This function adds new profile corresponding to requested BW, configures
 * node's RL profile ID of type CIR, EIR, or SRL, and removes old profile
 * ID from local database. The caller needs to hold scheduler lock.
 */
int
ice_sched_set_node_bw(struct ice_port_info *pi, struct ice_sched_node *node,
		      enum ice_rl_type rl_type, u32 bw, u8 layer_num)
{
	struct ice_aqc_rl_profile_info *rl_prof_info;
	struct ice_hw *hw = pi->hw;
	u16 old_id, rl_prof_id;
	int status = ICE_ERR_PARAM;

	rl_prof_info = ice_sched_add_rl_profile(hw, rl_type, bw, layer_num);
	if (!rl_prof_info)
		return status;

	rl_prof_id = LE16_TO_CPU(rl_prof_info->profile.profile_id);

	/* Save existing RL prof ID for later clean up */
	old_id = ice_sched_get_node_rl_prof_id(node, rl_type);
	/* Configure BW scheduling parameters */
	status = ice_sched_cfg_node_bw_lmt(hw, node, rl_type, rl_prof_id);
	if (status)
		return status;

	/* New changes has been applied */
	/* Increment the profile ID reference count */
	rl_prof_info->prof_id_ref++;

	/* Check for old ID removal */
	if ((old_id == ICE_SCHED_DFLT_RL_PROF_ID && rl_type != ICE_SHARED_BW) ||
	    old_id == ICE_SCHED_INVAL_PROF_ID || old_id == rl_prof_id)
		return 0;

	return ice_sched_rm_rl_profile(hw, layer_num,
				       rl_prof_info->profile.flags &
				       ICE_AQC_RL_PROFILE_TYPE_M, old_id);
}

/**
 * ice_sched_set_node_priority - set node's priority
 * @pi: port information structure
 * @node: tree node
 * @priority: number 0-7 representing priority among siblings
 *
 * This function sets priority of a node among it's siblings.
 */
int
ice_sched_set_node_priority(struct ice_port_info *pi, struct ice_sched_node *node,
			    u16 priority)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;

	data->valid_sections |= ICE_AQC_ELEM_VALID_GENERIC;
	data->generic |= ICE_AQC_ELEM_GENERIC_PRIO_M &
			 (priority << ICE_AQC_ELEM_GENERIC_PRIO_S);

	return ice_sched_update_elem(pi->hw, node, &buf);
}

/**
 * ice_sched_set_node_weight - set node's weight
 * @pi: port information structure
 * @node: tree node
 * @weight: number 1-200 representing weight for WFQ
 *
 * This function sets weight of the node for WFQ algorithm.
 */
int
ice_sched_set_node_weight(struct ice_port_info *pi, struct ice_sched_node *node, u16 weight)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;

	buf = node->info;
	data = &buf.data;

	data->valid_sections = ICE_AQC_ELEM_VALID_CIR | ICE_AQC_ELEM_VALID_EIR |
			       ICE_AQC_ELEM_VALID_GENERIC;
	data->cir_bw.bw_alloc = CPU_TO_LE16(weight);
	data->eir_bw.bw_alloc = CPU_TO_LE16(weight);
	data->generic |= ICE_AQC_ELEM_GENERIC_SP_M &
			 (0x0 << ICE_AQC_ELEM_GENERIC_SP_S);

	return ice_sched_update_elem(pi->hw, node, &buf);
}

/**
 * ice_sched_set_node_bw_lmt - set node's BW limit
 * @pi: port information structure
 * @node: tree node
 * @rl_type: rate limit type min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * It updates node's BW limit parameters like BW RL profile ID of type CIR,
 * EIR, or SRL. The caller needs to hold scheduler lock.
 *
 * NOTE: Caller provides the correct SRL node in case of shared profile
 * settings.
 */
int
ice_sched_set_node_bw_lmt(struct ice_port_info *pi, struct ice_sched_node *node,
			  enum ice_rl_type rl_type, u32 bw)
{
	struct ice_hw *hw;
	u8 layer_num;

	if (!pi)
		return ICE_ERR_PARAM;
	hw = pi->hw;
	/* Remove unused RL profile IDs from HW and SW DB */
	ice_sched_rm_unused_rl_prof(hw);

	layer_num = ice_sched_get_rl_prof_layer(pi, rl_type,
						node->tx_sched_layer);
	if (layer_num >= hw->num_tx_sched_layers)
		return ICE_ERR_PARAM;

	if (bw == ICE_SCHED_DFLT_BW)
		return ice_sched_set_node_bw_dflt(pi, node, rl_type, layer_num);
	return ice_sched_set_node_bw(pi, node, rl_type, bw, layer_num);
}

/**
 * ice_sched_set_node_bw_dflt_lmt - set node's BW limit to default
 * @pi: port information structure
 * @node: pointer to node structure
 * @rl_type: rate limit type min, max, or shared
 *
 * This function configures node element's BW rate limit profile ID of
 * type CIR, EIR, or SRL to default. This function needs to be called
 * with the scheduler lock held.
 */
static int
ice_sched_set_node_bw_dflt_lmt(struct ice_port_info *pi,
			       struct ice_sched_node *node,
			       enum ice_rl_type rl_type)
{
	return ice_sched_set_node_bw_lmt(pi, node, rl_type,
					 ICE_SCHED_DFLT_BW);
}

/**
 * ice_sched_validate_srl_node - Check node for SRL applicability
 * @node: sched node to configure
 * @sel_layer: selected SRL layer
 *
 * This function checks if the SRL can be applied to a selceted layer node on
 * behalf of the requested node (first argument). This function needs to be
 * called with scheduler lock held.
 */
static int
ice_sched_validate_srl_node(struct ice_sched_node *node, u8 sel_layer)
{
	/* SRL profiles are not available on all layers. Check if the
	 * SRL profile can be applied to a node above or below the
	 * requested node. SRL configuration is possible only if the
	 * selected layer's node has single child.
	 */
	if (sel_layer == node->tx_sched_layer ||
	    ((sel_layer == node->tx_sched_layer + 1) &&
	    node->num_children == 1) ||
	    ((sel_layer == node->tx_sched_layer - 1) &&
	    (node->parent && node->parent->num_children == 1)))
		return 0;

	return ICE_ERR_CFG;
}

/**
 * ice_sched_save_q_bw - save queue node's BW information
 * @q_ctx: queue context structure
 * @rl_type: rate limit type min, max, or shared
 * @bw: bandwidth in Kbps - Kilo bits per sec
 *
 * Save BW information of queue type node for post replay use.
 */
static int
ice_sched_save_q_bw(struct ice_q_ctx *q_ctx, enum ice_rl_type rl_type, u32 bw)
{
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&q_ctx->bw_t_info, bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&q_ctx->bw_t_info, bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&q_ctx->bw_t_info, bw);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_sched_set_q_bw_lmt - sets queue BW limit
 * @pi: port information structure
 * @vsi_handle: sw VSI handle
 * @tc: traffic class
 * @q_handle: software queue handle
 * @rl_type: min, max, or shared
 * @bw: bandwidth in Kbps
 *
 * This function sets BW limit of queue scheduling node.
 */
static int
ice_sched_set_q_bw_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		       u16 q_handle, enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_node *node;
	struct ice_q_ctx *q_ctx;
	int status = ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	ice_acquire_lock(&pi->sched_lock);
	q_ctx = ice_get_lan_q_ctx(pi->hw, vsi_handle, tc, q_handle);
	if (!q_ctx)
		goto exit_q_bw_lmt;
	node = ice_sched_find_node_by_teid(pi->root, q_ctx->q_teid);
	if (!node) {
		ice_debug(pi->hw, ICE_DBG_SCHED, "Wrong q_teid\n");
		goto exit_q_bw_lmt;
	}

	/* Return error if it is not a leaf node */
	if (node->info.data.elem_type != ICE_AQC_ELEM_TYPE_LEAF)
		goto exit_q_bw_lmt;

	/* SRL bandwidth layer selection */
	if (rl_type == ICE_SHARED_BW) {
		u8 sel_layer; /* selected layer */

		sel_layer = ice_sched_get_rl_prof_layer(pi, rl_type,
							node->tx_sched_layer);
		if (sel_layer >= pi->hw->num_tx_sched_layers) {
			status = ICE_ERR_PARAM;
			goto exit_q_bw_lmt;
		}
		status = ice_sched_validate_srl_node(node, sel_layer);
		if (status)
			goto exit_q_bw_lmt;
	}

	if (bw == ICE_SCHED_DFLT_BW)
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, rl_type);
	else
		status = ice_sched_set_node_bw_lmt(pi, node, rl_type, bw);

	if (!status)
		status = ice_sched_save_q_bw(q_ctx, rl_type, bw);

exit_q_bw_lmt:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_q_bw_lmt - configure queue BW limit
 * @pi: port information structure
 * @vsi_handle: sw VSI handle
 * @tc: traffic class
 * @q_handle: software queue handle
 * @rl_type: min, max, or shared
 * @bw: bandwidth in Kbps
 *
 * This function configures BW limit of queue scheduling node.
 */
int
ice_cfg_q_bw_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		 u16 q_handle, enum ice_rl_type rl_type, u32 bw)
{
	return ice_sched_set_q_bw_lmt(pi, vsi_handle, tc, q_handle, rl_type,
				      bw);
}

/**
 * ice_cfg_q_bw_dflt_lmt - configure queue BW default limit
 * @pi: port information structure
 * @vsi_handle: sw VSI handle
 * @tc: traffic class
 * @q_handle: software queue handle
 * @rl_type: min, max, or shared
 *
 * This function configures BW default limit of queue scheduling node.
 */
int
ice_cfg_q_bw_dflt_lmt(struct ice_port_info *pi, u16 vsi_handle, u8 tc,
		      u16 q_handle, enum ice_rl_type rl_type)
{
	return ice_sched_set_q_bw_lmt(pi, vsi_handle, tc, q_handle, rl_type,
				      ICE_SCHED_DFLT_BW);
}

/**
 * ice_sched_save_tc_node_bw - save TC node BW limit
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function saves the modified values of bandwidth settings for later
 * replay purpose (restore) after reset.
 */
static int
ice_sched_save_tc_node_bw(struct ice_port_info *pi, u8 tc,
			  enum ice_rl_type rl_type, u32 bw)
{
	if (tc >= ICE_MAX_TRAFFIC_CLASS)
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw(&pi->tc_node_bw_t_info[tc], bw);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw(&pi->tc_node_bw_t_info[tc], bw);
		break;
	case ICE_SHARED_BW:
		ice_set_clear_shared_bw(&pi->tc_node_bw_t_info[tc], bw);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

#define ICE_SCHED_GENERIC_STRICT_MODE		BIT(4)
#define ICE_SCHED_GENERIC_PRIO_S		1

/**
 * ice_sched_set_tc_node_bw_lmt - sets TC node BW limit
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function configures bandwidth limit of TC node.
 */
static int
ice_sched_set_tc_node_bw_lmt(struct ice_port_info *pi, u8 tc,
			     enum ice_rl_type rl_type, u32 bw)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;
	struct ice_sched_node *tc_node;
	int status = ICE_ERR_PARAM;

	if (tc >= ICE_MAX_TRAFFIC_CLASS)
		return status;
	ice_acquire_lock(&pi->sched_lock);
	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		goto exit_set_tc_node_bw;

	/* update node's generic field */
	buf = tc_node->info;
	data = &buf.data;
	data->valid_sections = ICE_AQC_ELEM_VALID_GENERIC;
	data->generic = (tc << ICE_SCHED_GENERIC_PRIO_S) |
		ICE_SCHED_GENERIC_STRICT_MODE;
	status = ice_sched_update_elem(pi->hw, tc_node, &buf);
	if (status)
		goto exit_set_tc_node_bw;

	if (bw == ICE_SCHED_DFLT_BW)
		status = ice_sched_set_node_bw_dflt_lmt(pi, tc_node, rl_type);
	else
		status = ice_sched_set_node_bw_lmt(pi, tc_node, rl_type, bw);
	if (!status)
		status = ice_sched_save_tc_node_bw(pi, tc, rl_type, bw);

exit_set_tc_node_bw:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_tc_node_bw_lmt - configure TC node BW limit
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function configures BW limit of TC node.
 * Note: The minimum guaranteed reservation is done via DCBX.
 */
int
ice_cfg_tc_node_bw_lmt(struct ice_port_info *pi, u8 tc,
		       enum ice_rl_type rl_type, u32 bw)
{
	return ice_sched_set_tc_node_bw_lmt(pi, tc, rl_type, bw);
}

/**
 * ice_cfg_tc_node_bw_dflt_lmt - configure TC node BW default limit
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 *
 * This function configures BW default limit of TC node.
 */
int
ice_cfg_tc_node_bw_dflt_lmt(struct ice_port_info *pi, u8 tc,
			    enum ice_rl_type rl_type)
{
	return ice_sched_set_tc_node_bw_lmt(pi, tc, rl_type, ICE_SCHED_DFLT_BW);
}

/**
 * ice_sched_save_tc_node_bw_alloc - save TC node's BW alloc information
 * @pi: port information structure
 * @tc: traffic class
 * @rl_type: rate limit type min or max
 * @bw_alloc: Bandwidth allocation information
 *
 * Save BW alloc information of VSI type node for post replay use.
 */
static int
ice_sched_save_tc_node_bw_alloc(struct ice_port_info *pi, u8 tc,
				enum ice_rl_type rl_type, u16 bw_alloc)
{
	if (tc >= ICE_MAX_TRAFFIC_CLASS)
		return ICE_ERR_PARAM;
	switch (rl_type) {
	case ICE_MIN_BW:
		ice_set_clear_cir_bw_alloc(&pi->tc_node_bw_t_info[tc],
					   bw_alloc);
		break;
	case ICE_MAX_BW:
		ice_set_clear_eir_bw_alloc(&pi->tc_node_bw_t_info[tc],
					   bw_alloc);
		break;
	default:
		return ICE_ERR_PARAM;
	}
	return 0;
}

/**
 * ice_sched_set_tc_node_bw_alloc - set TC node BW alloc
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 * @bw_alloc: bandwidth alloc
 *
 * This function configures bandwidth alloc of TC node, also saves the
 * changed settings for replay purpose, and return success if it succeeds
 * in modifying bandwidth alloc setting.
 */
static int
ice_sched_set_tc_node_bw_alloc(struct ice_port_info *pi, u8 tc,
			       enum ice_rl_type rl_type, u8 bw_alloc)
{
	struct ice_sched_node *tc_node;
	int status = ICE_ERR_PARAM;

	if (tc >= ICE_MAX_TRAFFIC_CLASS)
		return status;
	ice_acquire_lock(&pi->sched_lock);
	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		goto exit_set_tc_node_bw_alloc;
	status = ice_sched_cfg_node_bw_alloc(pi->hw, tc_node, rl_type,
					     bw_alloc);
	if (status)
		goto exit_set_tc_node_bw_alloc;
	status = ice_sched_save_tc_node_bw_alloc(pi, tc, rl_type, bw_alloc);

exit_set_tc_node_bw_alloc:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_cfg_tc_node_bw_alloc - configure TC node BW alloc
 * @pi: port information structure
 * @tc: TC number
 * @rl_type: min or max
 * @bw_alloc: bandwidth alloc
 *
 * This function configures BW limit of TC node.
 * Note: The minimum guaranteed reservation is done via DCBX.
 */
int
ice_cfg_tc_node_bw_alloc(struct ice_port_info *pi, u8 tc,
			 enum ice_rl_type rl_type, u8 bw_alloc)
{
	return ice_sched_set_tc_node_bw_alloc(pi, tc, rl_type, bw_alloc);
}

/**
 * ice_sched_set_agg_bw_dflt_lmt - set aggregator node's BW limit to default
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function retrieves the aggregator ID based on VSI ID and TC,
 * and sets node's BW limit to default. This function needs to be
 * called with the scheduler lock held.
 */
int
ice_sched_set_agg_bw_dflt_lmt(struct ice_port_info *pi, u16 vsi_handle)
{
	struct ice_vsi_ctx *vsi_ctx;
	int status = 0;
	u8 tc;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;
	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;

	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *node;

		node = vsi_ctx->sched.ag_node[tc];
		if (!node)
			continue;

		/* Set min profile to default */
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, ICE_MIN_BW);
		if (status)
			break;

		/* Set max profile to default */
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, ICE_MAX_BW);
		if (status)
			break;

		/* Remove shared profile, if there is one */
		status = ice_sched_set_node_bw_dflt_lmt(pi, node,
							ICE_SHARED_BW);
		if (status)
			break;
	}

	return status;
}

/**
 * ice_sched_get_node_by_id_type - get node from ID type
 * @pi: port information structure
 * @id: identifier
 * @agg_type: type of aggregator
 * @tc: traffic class
 *
 * This function returns node identified by ID of type aggregator, and
 * based on traffic class (TC). This function needs to be called with
 * the scheduler lock held.
 */
static struct ice_sched_node *
ice_sched_get_node_by_id_type(struct ice_port_info *pi, u32 id,
			      enum ice_agg_type agg_type, u8 tc)
{
	struct ice_sched_node *node = NULL;

	switch (agg_type) {
	case ICE_AGG_TYPE_VSI: {
		struct ice_vsi_ctx *vsi_ctx;
		u16 vsi_handle = (u16)id;

		if (!ice_is_vsi_valid(pi->hw, vsi_handle))
			break;
		/* Get sched_vsi_info */
		vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
		if (!vsi_ctx)
			break;
		node = vsi_ctx->sched.vsi_node[tc];
		break;
	}

	case ICE_AGG_TYPE_AGG: {
		struct ice_sched_node *tc_node;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (tc_node)
			node = ice_sched_get_agg_node(pi, tc_node, id);
		break;
	}

	case ICE_AGG_TYPE_Q:
		/* The current implementation allows single queue to modify */
		node = ice_sched_find_node_by_teid(pi->root, id);
		break;

	case ICE_AGG_TYPE_QG: {
		struct ice_sched_node *child_node;

		/* The current implementation allows single qg to modify */
		child_node = ice_sched_find_node_by_teid(pi->root, id);
		if (!child_node)
			break;
		node = child_node->parent;
		break;
	}

	default:
		break;
	}

	return node;
}

/**
 * ice_sched_set_node_bw_lmt_per_tc - set node BW limit per TC
 * @pi: port information structure
 * @id: ID (software VSI handle or AGG ID)
 * @agg_type: aggregator type (VSI or AGG type node)
 * @tc: traffic class
 * @rl_type: min or max
 * @bw: bandwidth in Kbps
 *
 * This function sets BW limit of VSI or Aggregator scheduling node
 * based on TC information from passed in argument BW.
 */
int
ice_sched_set_node_bw_lmt_per_tc(struct ice_port_info *pi, u32 id,
				 enum ice_agg_type agg_type, u8 tc,
				 enum ice_rl_type rl_type, u32 bw)
{
	struct ice_sched_node *node;
	int status = ICE_ERR_PARAM;

	if (!pi)
		return status;

	if (rl_type == ICE_UNKNOWN_BW)
		return status;

	ice_acquire_lock(&pi->sched_lock);
	node = ice_sched_get_node_by_id_type(pi, id, agg_type, tc);
	if (!node) {
		ice_debug(pi->hw, ICE_DBG_SCHED, "Wrong id, agg type, or tc\n");
		goto exit_set_node_bw_lmt_per_tc;
	}
	if (bw == ICE_SCHED_DFLT_BW)
		status = ice_sched_set_node_bw_dflt_lmt(pi, node, rl_type);
	else
		status = ice_sched_set_node_bw_lmt(pi, node, rl_type, bw);

exit_set_node_bw_lmt_per_tc:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_validate_vsi_srl_node - validate VSI SRL node
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 *
 * This function validates SRL node of the VSI node if available SRL layer is
 * different than the VSI node layer on all TC(s).This function needs to be
 * called with scheduler lock held.
 */
static int
ice_sched_validate_vsi_srl_node(struct ice_port_info *pi, u16 vsi_handle)
{
	u8 sel_layer = ICE_SCHED_INVAL_LAYER_NUM;
	u8 tc;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;

	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *vsi_node;
		enum ice_rl_type rl_type = ICE_SHARED_BW;
		int status;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;

		/* SRL bandwidth layer selection */
		if (sel_layer == ICE_SCHED_INVAL_LAYER_NUM) {
			u8 node_layer = vsi_node->tx_sched_layer;
			u8 layer_num;

			layer_num = ice_sched_get_rl_prof_layer(pi, rl_type,
								node_layer);
			if (layer_num >= pi->hw->num_tx_sched_layers)
				return ICE_ERR_PARAM;
			sel_layer = layer_num;
		}

		status = ice_sched_validate_srl_node(vsi_node, sel_layer);
		if (status)
			return status;
	}
	return 0;
}

/**
 * ice_sched_set_save_vsi_srl_node_bw - set VSI shared limit values
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: traffic class
 * @srl_node: sched node to configure
 * @rl_type: rate limit type minimum, maximum, or shared
 * @bw: minimum, maximum, or shared bandwidth in Kbps
 *
 * Configure shared rate limiter(SRL) of VSI type nodes across given traffic
 * class, and saves those value for later use for replaying purposes. The
 * caller holds the scheduler lock.
 */
static int
ice_sched_set_save_vsi_srl_node_bw(struct ice_port_info *pi, u16 vsi_handle,
				   u8 tc, struct ice_sched_node *srl_node,
				   enum ice_rl_type rl_type, u32 bw)
{
	int status;

	if (bw == ICE_SCHED_DFLT_BW) {
		status = ice_sched_set_node_bw_dflt_lmt(pi, srl_node, rl_type);
	} else {
		status = ice_sched_set_node_bw_lmt(pi, srl_node, rl_type, bw);
		if (status)
			return status;
		status = ice_sched_save_vsi_bw(pi, vsi_handle, tc, rl_type, bw);
	}
	return status;
}

/**
 * ice_sched_set_vsi_node_srl_per_tc - set VSI node BW shared limit for tc
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @tc: traffic class
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * Configure shared rate limiter(SRL) of  VSI type nodes across requested
 * traffic class for VSI matching handle. When BW value of ICE_SCHED_DFLT_BW
 * is passed, it removes the corresponding bw from the node. The caller
 * holds scheduler lock.
 */
static int
ice_sched_set_vsi_node_srl_per_tc(struct ice_port_info *pi, u16 vsi_handle,
				  u8 tc, u32 min_bw, u32 max_bw, u32 shared_bw)
{
	struct ice_sched_node *tc_node, *vsi_node, *cfg_node;
	u8 layer_num;
	int status;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
	if (!vsi_node)
		return ICE_ERR_CFG;

	layer_num = ice_sched_get_rl_prof_layer(pi, ICE_SHARED_BW,
						vsi_node->tx_sched_layer);
	if (layer_num >= pi->hw->num_tx_sched_layers)
		return ICE_ERR_PARAM;

	/* SRL node may be different */
	cfg_node = ice_sched_get_srl_node(vsi_node, layer_num);
	if (!cfg_node)
		return ICE_ERR_CFG;

	status = ice_sched_set_save_vsi_srl_node_bw(pi, vsi_handle, tc,
						    cfg_node, ICE_MIN_BW,
						    min_bw);
	if (status)
		return status;

	status = ice_sched_set_save_vsi_srl_node_bw(pi, vsi_handle, tc,
						    cfg_node, ICE_MAX_BW,
						    max_bw);
	if (status)
		return status;

	return ice_sched_set_save_vsi_srl_node_bw(pi, vsi_handle, tc, cfg_node,
						  ICE_SHARED_BW, shared_bw);
}

/**
 * ice_sched_set_vsi_bw_shared_lmt - set VSI BW shared limit
 * @pi: port information structure
 * @vsi_handle: software VSI handle
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * Configure shared rate limiter(SRL) of all VSI type nodes across all traffic
 * classes for VSI matching handle. When BW value of ICE_SCHED_DFLT_BW is
 * passed, it removes those value(s) from the node.
 */
int
ice_sched_set_vsi_bw_shared_lmt(struct ice_port_info *pi, u16 vsi_handle,
				u32 min_bw, u32 max_bw, u32 shared_bw)
{
	int status = 0;
	u8 tc;

	if (!pi)
		return ICE_ERR_PARAM;

	if (!ice_is_vsi_valid(pi->hw, vsi_handle))
		return ICE_ERR_PARAM;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_validate_vsi_srl_node(pi, vsi_handle);
	if (status)
		goto exit_set_vsi_bw_shared_lmt;
	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *vsi_node;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;

		status = ice_sched_set_vsi_node_srl_per_tc(pi, vsi_handle, tc,
							   min_bw, max_bw,
							   shared_bw);
		if (status)
			break;
	}

exit_set_vsi_bw_shared_lmt:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_validate_agg_srl_node - validate AGG SRL node
 * @pi: port information structure
 * @agg_id: aggregator ID
 *
 * This function validates SRL node of the AGG node if available SRL layer is
 * different than the AGG node layer on all TC(s).This function needs to be
 * called with scheduler lock held.
 */
static int
ice_sched_validate_agg_srl_node(struct ice_port_info *pi, u32 agg_id)
{
	u8 sel_layer = ICE_SCHED_INVAL_LAYER_NUM;
	struct ice_sched_agg_info *agg_info;
	bool agg_id_present = false;
	int status = 0;
	u8 tc;

	LIST_FOR_EACH_ENTRY(agg_info, &pi->hw->agg_list, ice_sched_agg_info,
			    list_entry)
		if (agg_info->agg_id == agg_id) {
			agg_id_present = true;
			break;
		}
	if (!agg_id_present)
		return ICE_ERR_PARAM;
	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *agg_node;
		enum ice_rl_type rl_type = ICE_SHARED_BW;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
		if (!agg_node)
			continue;
		/* SRL bandwidth layer selection */
		if (sel_layer == ICE_SCHED_INVAL_LAYER_NUM) {
			u8 node_layer = agg_node->tx_sched_layer;
			u8 layer_num;

			layer_num = ice_sched_get_rl_prof_layer(pi, rl_type,
								node_layer);
			if (layer_num >= pi->hw->num_tx_sched_layers)
				return ICE_ERR_PARAM;
			sel_layer = layer_num;
		}

		status = ice_sched_validate_srl_node(agg_node, sel_layer);
		if (status)
			break;
	}
	return status;
}

/**
 * ice_sched_validate_agg_id - Validate aggregator id
 * @pi: port information structure
 * @agg_id: aggregator ID
 *
 * This function validates aggregator id. Caller holds the scheduler lock.
 */
static int
ice_sched_validate_agg_id(struct ice_port_info *pi, u32 agg_id)
{
	struct ice_sched_agg_info *agg_info;
	struct ice_sched_agg_info *tmp;
	bool agg_id_present = false;
	int status;

	status = ice_sched_validate_agg_srl_node(pi, agg_id);
	if (status)
		return status;

	LIST_FOR_EACH_ENTRY_SAFE(agg_info, tmp, &pi->hw->agg_list,
				 ice_sched_agg_info, list_entry)
		if (agg_info->agg_id == agg_id) {
			agg_id_present = true;
			break;
		}

	if (!agg_id_present)
		return ICE_ERR_PARAM;

	return 0;
}

/**
 * ice_sched_set_save_agg_srl_node_bw - set aggregator shared limit values
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @srl_node: sched node to configure
 * @rl_type: rate limit type minimum, maximum, or shared
 * @bw: minimum, maximum, or shared bandwidth in Kbps
 *
 * Configure shared rate limiter(SRL) of aggregator type nodes across
 * requested traffic class, and saves those value for later use for
 * replaying purposes. The caller holds the scheduler lock.
 */
static int
ice_sched_set_save_agg_srl_node_bw(struct ice_port_info *pi, u32 agg_id, u8 tc,
				   struct ice_sched_node *srl_node,
				   enum ice_rl_type rl_type, u32 bw)
{
	int status;

	if (bw == ICE_SCHED_DFLT_BW) {
		status = ice_sched_set_node_bw_dflt_lmt(pi, srl_node, rl_type);
	} else {
		status = ice_sched_set_node_bw_lmt(pi, srl_node, rl_type, bw);
		if (status)
			return status;
		status = ice_sched_save_agg_bw(pi, agg_id, tc, rl_type, bw);
	}
	return status;
}

/**
 * ice_sched_set_agg_node_srl_per_tc - set aggregator SRL per tc
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * This function configures the shared rate limiter(SRL) of aggregator type
 * node for a given traffic class for aggregator matching agg_id. When BW
 * value of ICE_SCHED_DFLT_BW is passed, it removes SRL from the node. Caller
 * holds the scheduler lock.
 */
static int
ice_sched_set_agg_node_srl_per_tc(struct ice_port_info *pi, u32 agg_id,
				  u8 tc, u32 min_bw, u32 max_bw, u32 shared_bw)
{
	struct ice_sched_node *tc_node, *agg_node, *cfg_node;
	enum ice_rl_type rl_type = ICE_SHARED_BW;
	int status = ICE_ERR_CFG;
	u8 layer_num;

	tc_node = ice_sched_get_tc_node(pi, tc);
	if (!tc_node)
		return ICE_ERR_CFG;

	agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
	if (!agg_node)
		return ICE_ERR_CFG;

	layer_num = ice_sched_get_rl_prof_layer(pi, rl_type,
						agg_node->tx_sched_layer);
	if (layer_num >= pi->hw->num_tx_sched_layers)
		return ICE_ERR_PARAM;

	/* SRL node may be different */
	cfg_node = ice_sched_get_srl_node(agg_node, layer_num);
	if (!cfg_node)
		return ICE_ERR_CFG;

	status = ice_sched_set_save_agg_srl_node_bw(pi, agg_id, tc, cfg_node,
						    ICE_MIN_BW, min_bw);
	if (status)
		return status;

	status = ice_sched_set_save_agg_srl_node_bw(pi, agg_id, tc, cfg_node,
						    ICE_MAX_BW, max_bw);
	if (status)
		return status;

	status = ice_sched_set_save_agg_srl_node_bw(pi, agg_id, tc, cfg_node,
						    ICE_SHARED_BW, shared_bw);
	return status;
}

/**
 * ice_sched_set_agg_bw_shared_lmt - set aggregator BW shared limit
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * This function configures the shared rate limiter(SRL) of all aggregator type
 * nodes across all traffic classes for aggregator matching agg_id. When
 * BW value of ICE_SCHED_DFLT_BW is passed, it removes SRL from the
 * node(s).
 */
int
ice_sched_set_agg_bw_shared_lmt(struct ice_port_info *pi, u32 agg_id,
				u32 min_bw, u32 max_bw, u32 shared_bw)
{
	int status;
	u8 tc;

	if (!pi)
		return ICE_ERR_PARAM;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_validate_agg_id(pi, agg_id);
	if (status)
		goto exit_agg_bw_shared_lmt;

	/* Return success if no nodes are present across TC */
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node, *agg_node;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;

		agg_node = ice_sched_get_agg_node(pi, tc_node, agg_id);
		if (!agg_node)
			continue;

		status = ice_sched_set_agg_node_srl_per_tc(pi, agg_id, tc,
							   min_bw, max_bw,
							   shared_bw);
		if (status)
			break;
	}

exit_agg_bw_shared_lmt:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_set_agg_bw_shared_lmt_per_tc - set aggregator BW shared lmt per tc
 * @pi: port information structure
 * @agg_id: aggregator ID
 * @tc: traffic class
 * @min_bw: minimum bandwidth in Kbps
 * @max_bw: maximum bandwidth in Kbps
 * @shared_bw: shared bandwidth in Kbps
 *
 * This function configures the shared rate limiter(SRL) of aggregator type
 * node for a given traffic class for aggregator matching agg_id. When BW
 * value of ICE_SCHED_DFLT_BW is passed, it removes SRL from the node.
 */
int
ice_sched_set_agg_bw_shared_lmt_per_tc(struct ice_port_info *pi, u32 agg_id,
				       u8 tc, u32 min_bw, u32 max_bw,
				       u32 shared_bw)
{
	int status;

	if (!pi)
		return ICE_ERR_PARAM;
	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_validate_agg_id(pi, agg_id);
	if (status)
		goto exit_agg_bw_shared_lmt_per_tc;

	status = ice_sched_set_agg_node_srl_per_tc(pi, agg_id, tc, min_bw,
						   max_bw, shared_bw);

exit_agg_bw_shared_lmt_per_tc:
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_cfg_sibl_node_prio - configure node sibling priority
 * @pi: port information structure
 * @node: sched node to configure
 * @priority: sibling priority
 *
 * This function configures node element's sibling priority only. This
 * function needs to be called with scheduler lock held.
 */
int
ice_sched_cfg_sibl_node_prio(struct ice_port_info *pi,
			     struct ice_sched_node *node, u8 priority)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;
	struct ice_hw *hw = pi->hw;
	int status;

	if (!hw)
		return ICE_ERR_PARAM;
	buf = node->info;
	data = &buf.data;
	data->valid_sections |= ICE_AQC_ELEM_VALID_GENERIC;
	priority = (priority << ICE_AQC_ELEM_GENERIC_PRIO_S) &
		   ICE_AQC_ELEM_GENERIC_PRIO_M;
	data->generic &= ~ICE_AQC_ELEM_GENERIC_PRIO_M;
	data->generic |= priority;

	/* Configure element */
	status = ice_sched_update_elem(hw, node, &buf);
	return status;
}

/**
 * ice_cfg_rl_burst_size - Set burst size value
 * @hw: pointer to the HW struct
 * @bytes: burst size in bytes
 *
 * This function configures/set the burst size to requested new value. The new
 * burst size value is used for future rate limit calls. It doesn't change the
 * existing or previously created RL profiles.
 */
int ice_cfg_rl_burst_size(struct ice_hw *hw, u32 bytes)
{
	u16 burst_size_to_prog;

	if (bytes < ICE_MIN_BURST_SIZE_ALLOWED ||
	    bytes > ICE_MAX_BURST_SIZE_ALLOWED)
		return ICE_ERR_PARAM;
	if (ice_round_to_num(bytes, 64) <=
	    ICE_MAX_BURST_SIZE_64_BYTE_GRANULARITY) {
		/* 64 byte granularity case */
		/* Disable MSB granularity bit */
		burst_size_to_prog = ICE_64_BYTE_GRANULARITY;
		/* round number to nearest 64 byte granularity */
		bytes = ice_round_to_num(bytes, 64);
		/* The value is in 64 byte chunks */
		burst_size_to_prog |= (u16)(bytes / 64);
	} else {
		/* k bytes granularity case */
		/* Enable MSB granularity bit */
		burst_size_to_prog = ICE_KBYTE_GRANULARITY;
		/* round number to nearest 1024 granularity */
		bytes = ice_round_to_num(bytes, 1024);
		/* check rounding doesn't go beyond allowed */
		if (bytes > ICE_MAX_BURST_SIZE_KBYTE_GRANULARITY)
			bytes = ICE_MAX_BURST_SIZE_KBYTE_GRANULARITY;
		/* The value is in k bytes */
		burst_size_to_prog |= (u16)(bytes / 1024);
	}
	hw->max_burst_size = burst_size_to_prog;
	return 0;
}

/**
 * ice_sched_replay_node_prio - re-configure node priority
 * @hw: pointer to the HW struct
 * @node: sched node to configure
 * @priority: priority value
 *
 * This function configures node element's priority value. It
 * needs to be called with scheduler lock held.
 */
static int
ice_sched_replay_node_prio(struct ice_hw *hw, struct ice_sched_node *node,
			   u8 priority)
{
	struct ice_aqc_txsched_elem_data buf;
	struct ice_aqc_txsched_elem *data;
	int status;

	buf = node->info;
	data = &buf.data;
	data->valid_sections |= ICE_AQC_ELEM_VALID_GENERIC;
	data->generic = priority;

	/* Configure element */
	status = ice_sched_update_elem(hw, node, &buf);
	return status;
}

/**
 * ice_sched_replay_node_bw - replay node(s) BW
 * @hw: pointer to the HW struct
 * @node: sched node to configure
 * @bw_t_info: BW type information
 *
 * This function restores node's BW from bw_t_info. The caller needs
 * to hold the scheduler lock.
 */
static int
ice_sched_replay_node_bw(struct ice_hw *hw, struct ice_sched_node *node,
			 struct ice_bw_type_info *bw_t_info)
{
	struct ice_port_info *pi = hw->port_info;
	int status = ICE_ERR_PARAM;
	u16 bw_alloc;

	if (!node)
		return status;
	if (!ice_is_any_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_CNT))
		return 0;
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_PRIO)) {
		status = ice_sched_replay_node_prio(hw, node,
						    bw_t_info->generic);
		if (status)
			return status;
	}
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_CIR)) {
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_MIN_BW,
						   bw_t_info->cir_bw.bw);
		if (status)
			return status;
	}
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_CIR_WT)) {
		bw_alloc = bw_t_info->cir_bw.bw_alloc;
		status = ice_sched_cfg_node_bw_alloc(hw, node, ICE_MIN_BW,
						     bw_alloc);
		if (status)
			return status;
	}
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_EIR)) {
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_MAX_BW,
						   bw_t_info->eir_bw.bw);
		if (status)
			return status;
	}
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_EIR_WT)) {
		bw_alloc = bw_t_info->eir_bw.bw_alloc;
		status = ice_sched_cfg_node_bw_alloc(hw, node, ICE_MAX_BW,
						     bw_alloc);
		if (status)
			return status;
	}
	if (ice_is_bit_set(bw_t_info->bw_t_bitmap, ICE_BW_TYPE_SHARED))
		status = ice_sched_set_node_bw_lmt(pi, node, ICE_SHARED_BW,
						   bw_t_info->shared_bw);
	return status;
}

/**
 * ice_sched_replay_agg_bw - replay aggregator node(s) BW
 * @hw: pointer to the HW struct
 * @agg_info: aggregator data structure
 *
 * This function re-creates aggregator type nodes. The caller needs to hold
 * the scheduler lock.
 */
static int
ice_sched_replay_agg_bw(struct ice_hw *hw, struct ice_sched_agg_info *agg_info)
{
	struct ice_sched_node *tc_node, *agg_node;
	int status = 0;
	u8 tc;

	if (!agg_info)
		return ICE_ERR_PARAM;
	ice_for_each_traffic_class(tc) {
		if (!ice_is_any_bit_set(agg_info->bw_t_info[tc].bw_t_bitmap,
					ICE_BW_TYPE_CNT))
			continue;
		tc_node = ice_sched_get_tc_node(hw->port_info, tc);
		if (!tc_node) {
			status = ICE_ERR_PARAM;
			break;
		}
		agg_node = ice_sched_get_agg_node(hw->port_info, tc_node,
						  agg_info->agg_id);
		if (!agg_node) {
			status = ICE_ERR_PARAM;
			break;
		}
		status = ice_sched_replay_node_bw(hw, agg_node,
						  &agg_info->bw_t_info[tc]);
		if (status)
			break;
	}
	return status;
}

/**
 * ice_sched_get_ena_tc_bitmap - get enabled TC bitmap
 * @pi: port info struct
 * @tc_bitmap: 8 bits TC bitmap to check
 * @ena_tc_bitmap: 8 bits enabled TC bitmap to return
 *
 * This function returns enabled TC bitmap in variable ena_tc_bitmap. Some TCs
 * may be missing, it returns enabled TCs. This function needs to be called with
 * scheduler lock held.
 */
static void
ice_sched_get_ena_tc_bitmap(struct ice_port_info *pi, ice_bitmap_t *tc_bitmap,
			    ice_bitmap_t *ena_tc_bitmap)
{
	u8 tc;

	/* Some TC(s) may be missing after reset, adjust for replay */
	ice_for_each_traffic_class(tc)
		if (ice_is_tc_ena(*tc_bitmap, tc) &&
		    (ice_sched_get_tc_node(pi, tc)))
			ice_set_bit(tc, ena_tc_bitmap);
}

/**
 * ice_sched_replay_agg - recreate aggregator node(s)
 * @hw: pointer to the HW struct
 *
 * This function recreate aggregator type nodes which are not replayed earlier.
 * It also replay aggregator BW information. These aggregator nodes are not
 * associated with VSI type node yet.
 */
void ice_sched_replay_agg(struct ice_hw *hw)
{
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;

	ice_acquire_lock(&pi->sched_lock);
	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry)
		/* replay aggregator (re-create aggregator node) */
		if (!ice_cmp_bitmap(agg_info->tc_bitmap,
				    agg_info->replay_tc_bitmap,
				    ICE_MAX_TRAFFIC_CLASS)) {
			ice_declare_bitmap(replay_bitmap,
					   ICE_MAX_TRAFFIC_CLASS);
			int status;

			ice_zero_bitmap(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
			ice_sched_get_ena_tc_bitmap(pi,
						    agg_info->replay_tc_bitmap,
						    replay_bitmap);
			status = ice_sched_cfg_agg(hw->port_info,
						   agg_info->agg_id,
						   ICE_AGG_TYPE_AGG,
						   replay_bitmap);
			if (status) {
				ice_info(hw, "Replay agg id[%d] failed\n",
					 agg_info->agg_id);
				/* Move on to next one */
				continue;
			}
			/* Replay aggregator node BW (restore aggregator BW) */
			status = ice_sched_replay_agg_bw(hw, agg_info);
			if (status)
				ice_info(hw, "Replay agg bw [id=%d] failed\n",
					 agg_info->agg_id);
		}
	ice_release_lock(&pi->sched_lock);
}

/**
 * ice_sched_replay_agg_vsi_preinit - Agg/VSI replay pre initialization
 * @hw: pointer to the HW struct
 *
 * This function initialize aggregator(s) TC bitmap to zero. A required
 * preinit step for replaying aggregators.
 */
void ice_sched_replay_agg_vsi_preinit(struct ice_hw *hw)
{
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;

	ice_acquire_lock(&pi->sched_lock);
	LIST_FOR_EACH_ENTRY(agg_info, &hw->agg_list, ice_sched_agg_info,
			    list_entry) {
		struct ice_sched_agg_vsi_info *agg_vsi_info;

		agg_info->tc_bitmap[0] = 0;
		LIST_FOR_EACH_ENTRY(agg_vsi_info, &agg_info->agg_vsi_list,
				    ice_sched_agg_vsi_info, list_entry)
			agg_vsi_info->tc_bitmap[0] = 0;
	}
	ice_release_lock(&pi->sched_lock);
}

/**
 * ice_sched_replay_root_node_bw - replay root node BW
 * @pi: port information structure
 *
 * Replay root node BW settings.
 */
int ice_sched_replay_root_node_bw(struct ice_port_info *pi)
{
	int status = 0;

	if (!pi->hw)
		return ICE_ERR_PARAM;
	ice_acquire_lock(&pi->sched_lock);

	status = ice_sched_replay_node_bw(pi->hw, pi->root,
					  &pi->root_node_bw_t_info);
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_replay_tc_node_bw - replay TC node(s) BW
 * @pi: port information structure
 *
 * This function replay TC nodes.
 */
int ice_sched_replay_tc_node_bw(struct ice_port_info *pi)
{
	int status = 0;
	u8 tc;

	if (!pi->hw)
		return ICE_ERR_PARAM;
	ice_acquire_lock(&pi->sched_lock);
	ice_for_each_traffic_class(tc) {
		struct ice_sched_node *tc_node;

		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue; /* TC not present */
		status = ice_sched_replay_node_bw(pi->hw, tc_node,
						  &pi->tc_node_bw_t_info[tc]);
		if (status)
			break;
	}
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_replay_vsi_bw - replay VSI type node(s) BW
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 * @tc_bitmap: 8 bits TC bitmap
 *
 * This function replays VSI type nodes bandwidth. This function needs to be
 * called with scheduler lock held.
 */
static int
ice_sched_replay_vsi_bw(struct ice_hw *hw, u16 vsi_handle,
			ice_bitmap_t *tc_bitmap)
{
	struct ice_sched_node *vsi_node, *tc_node;
	struct ice_port_info *pi = hw->port_info;
	struct ice_bw_type_info *bw_t_info;
	struct ice_vsi_ctx *vsi_ctx;
	int status = 0;
	u8 tc;

	vsi_ctx = ice_get_vsi_ctx(pi->hw, vsi_handle);
	if (!vsi_ctx)
		return ICE_ERR_PARAM;
	ice_for_each_traffic_class(tc) {
		if (!ice_is_tc_ena(*tc_bitmap, tc))
			continue;
		tc_node = ice_sched_get_tc_node(pi, tc);
		if (!tc_node)
			continue;
		vsi_node = ice_sched_get_vsi_node(pi, tc_node, vsi_handle);
		if (!vsi_node)
			continue;
		bw_t_info = &vsi_ctx->sched.bw_t_info[tc];
		status = ice_sched_replay_node_bw(hw, vsi_node, bw_t_info);
		if (status)
			break;
	}
	return status;
}

/**
 * ice_sched_replay_vsi_agg - replay aggregator & VSI to aggregator node(s)
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 *
 * This function replays aggregator node, VSI to aggregator type nodes, and
 * their node bandwidth information. This function needs to be called with
 * scheduler lock held.
 */
static int
ice_sched_replay_vsi_agg(struct ice_hw *hw, u16 vsi_handle)
{
	ice_declare_bitmap(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	struct ice_sched_agg_vsi_info *agg_vsi_info;
	struct ice_port_info *pi = hw->port_info;
	struct ice_sched_agg_info *agg_info;
	int status;

	ice_zero_bitmap(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	if (!ice_is_vsi_valid(hw, vsi_handle))
		return ICE_ERR_PARAM;
	agg_info = ice_get_vsi_agg_info(hw, vsi_handle);
	if (!agg_info)
		return 0; /* Not present in list - default Agg case */
	agg_vsi_info = ice_get_agg_vsi_info(agg_info, vsi_handle);
	if (!agg_vsi_info)
		return 0; /* Not present in list - default Agg case */
	ice_sched_get_ena_tc_bitmap(pi, agg_info->replay_tc_bitmap,
				    replay_bitmap);
	/* Replay aggregator node associated to vsi_handle */
	status = ice_sched_cfg_agg(hw->port_info, agg_info->agg_id,
				   ICE_AGG_TYPE_AGG, replay_bitmap);
	if (status)
		return status;
	/* Replay aggregator node BW (restore aggregator BW) */
	status = ice_sched_replay_agg_bw(hw, agg_info);
	if (status)
		return status;

	ice_zero_bitmap(replay_bitmap, ICE_MAX_TRAFFIC_CLASS);
	ice_sched_get_ena_tc_bitmap(pi, agg_vsi_info->replay_tc_bitmap,
				    replay_bitmap);
	/* Move this VSI (vsi_handle) to above aggregator */
	status = ice_sched_assoc_vsi_to_agg(pi, agg_info->agg_id, vsi_handle,
					    replay_bitmap);
	if (status)
		return status;
	/* Replay VSI BW (restore VSI BW) */
	return ice_sched_replay_vsi_bw(hw, vsi_handle,
				       agg_vsi_info->tc_bitmap);
}

/**
 * ice_replay_vsi_agg - replay VSI to aggregator node
 * @hw: pointer to the HW struct
 * @vsi_handle: software VSI handle
 *
 * This function replays association of VSI to aggregator type nodes, and
 * node bandwidth information.
 */
int ice_replay_vsi_agg(struct ice_hw *hw, u16 vsi_handle)
{
	struct ice_port_info *pi = hw->port_info;
	int status;

	ice_acquire_lock(&pi->sched_lock);
	status = ice_sched_replay_vsi_agg(hw, vsi_handle);
	ice_release_lock(&pi->sched_lock);
	return status;
}

/**
 * ice_sched_replay_q_bw - replay queue type node BW
 * @pi: port information structure
 * @q_ctx: queue context structure
 *
 * This function replays queue type node bandwidth. This function needs to be
 * called with scheduler lock held.
 */
int
ice_sched_replay_q_bw(struct ice_port_info *pi, struct ice_q_ctx *q_ctx)
{
	struct ice_sched_node *q_node;

	/* Following also checks the presence of node in tree */
	q_node = ice_sched_find_node_by_teid(pi->root, q_ctx->q_teid);
	if (!q_node)
		return ICE_ERR_PARAM;
	return ice_sched_replay_node_bw(pi->hw, q_node, &q_ctx->bw_t_info);
}
