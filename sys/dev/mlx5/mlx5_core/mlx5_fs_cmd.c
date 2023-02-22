/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS `AS IS' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <linux/types.h>
#include <linux/module.h>
#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/device.h>
#include <dev/mlx5/fs.h>

#include <dev/mlx5/mlx5_core/fs_core.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>

int mlx5_cmd_update_root_ft(struct mlx5_core_dev *dev,
			    enum fs_ft_type type,
			    unsigned int id)
{
	u32 in[MLX5_ST_SZ_DW(set_flow_table_root_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(set_flow_table_root_out)] = {0};

	if (!dev)
		return -EINVAL;

	MLX5_SET(set_flow_table_root_in, in, opcode,
		 MLX5_CMD_OP_SET_FLOW_TABLE_ROOT);
	MLX5_SET(set_flow_table_root_in, in, table_type, type);
	MLX5_SET(set_flow_table_root_in, in, table_id, id);

	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

int mlx5_cmd_fs_create_ft(struct mlx5_core_dev *dev,
			  u16 vport, enum fs_ft_type type, unsigned int level,
			  unsigned int log_size, const char *name, unsigned int *table_id)
{
	u32 in[MLX5_ST_SZ_DW(create_flow_table_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(create_flow_table_out)] = {0};
	int err;

	if (!dev)
		return -EINVAL;

	MLX5_SET(create_flow_table_in, in, opcode,
		 MLX5_CMD_OP_CREATE_FLOW_TABLE);

	MLX5_SET(create_flow_table_in, in, table_type, type);
	MLX5_SET(create_flow_table_in, in, flow_table_context.level, level);
	MLX5_SET(create_flow_table_in, in, flow_table_context.log_size,
		 log_size);
	if (strstr(name, FS_REFORMAT_KEYWORD) != NULL)
		MLX5_SET(create_flow_table_in, in,
			 flow_table_context.reformat_en, 1);
	if (vport) {
		MLX5_SET(create_flow_table_in, in, vport_number, vport);
		MLX5_SET(create_flow_table_in, in, other_vport, 1);
	}

	err = mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
	if (!err)
		*table_id = MLX5_GET(create_flow_table_out, out, table_id);

	return err;
}

int mlx5_cmd_fs_destroy_ft(struct mlx5_core_dev *dev,
			   u16 vport,
			   enum fs_ft_type type, unsigned int table_id)
{
	u32 in[MLX5_ST_SZ_DW(destroy_flow_table_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(destroy_flow_table_out)] = {0};

	if (!dev)
		return -EINVAL;

	MLX5_SET(destroy_flow_table_in, in, opcode,
		 MLX5_CMD_OP_DESTROY_FLOW_TABLE);
	MLX5_SET(destroy_flow_table_in, in, table_type, type);
	MLX5_SET(destroy_flow_table_in, in, table_id, table_id);
	if (vport) {
		MLX5_SET(destroy_flow_table_in, in, vport_number, vport);
		MLX5_SET(destroy_flow_table_in, in, other_vport, 1);
	}

	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

int mlx5_cmd_fs_create_fg(struct mlx5_core_dev *dev,
			  u32 *in,
			  u16 vport,
			  enum fs_ft_type type, unsigned int table_id,
			  unsigned int *group_id)
{
	u32 out[MLX5_ST_SZ_DW(create_flow_group_out)] = {0};
	int err;
	int inlen = MLX5_ST_SZ_BYTES(create_flow_group_in);
	if (!dev)
		return -EINVAL;

	MLX5_SET(create_flow_group_in, in, opcode,
		 MLX5_CMD_OP_CREATE_FLOW_GROUP);
	MLX5_SET(create_flow_group_in, in, table_type, type);
	MLX5_SET(create_flow_group_in, in, table_id, table_id);
	if (vport) {
		MLX5_SET(create_flow_group_in, in, vport_number, vport);
		MLX5_SET(create_flow_group_in, in, other_vport, 1);
	}

	err = mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));
	if (!err)
		*group_id = MLX5_GET(create_flow_group_out, out, group_id);

	return err;
}

int mlx5_cmd_fs_destroy_fg(struct mlx5_core_dev *dev,
			   u16 vport,
			   enum fs_ft_type type, unsigned int table_id,
			   unsigned int group_id)
{
	u32 in[MLX5_ST_SZ_DW(destroy_flow_group_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(destroy_flow_group_out)] = {0};

	if (!dev)
		return -EINVAL;

	MLX5_SET(destroy_flow_group_in, in, opcode,
		 MLX5_CMD_OP_DESTROY_FLOW_GROUP);
	MLX5_SET(destroy_flow_group_in, in, table_type, type);
	MLX5_SET(destroy_flow_group_in, in, table_id,   table_id);
	MLX5_SET(destroy_flow_group_in, in, group_id, group_id);
	if (vport) {
		MLX5_SET(destroy_flow_group_in, in, vport_number, vport);
		MLX5_SET(destroy_flow_group_in, in, other_vport, 1);
	}

	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

int mlx5_cmd_fs_set_fte(struct mlx5_core_dev *dev,
			u16 vport,
			enum fs_fte_status *fte_status,
			u32 *match_val,
			enum fs_ft_type type, unsigned int table_id,
			unsigned int index, unsigned int group_id,
			struct mlx5_flow_act *flow_act,
			u32 sw_action, int dest_size,
			struct list_head *dests)  /* mlx5_flow_desination */
{
	u32 out[MLX5_ST_SZ_DW(set_fte_out)] = {0};
	u32 *in;
	unsigned int inlen;
	struct mlx5_flow_rule *dst;
	void *in_flow_context;
	void *in_match_value;
	void *in_dests;
	int err;
	int opmod = 0;
	int modify_mask = 0;
	int atomic_mod_cap;
	u32 prm_action = 0;
	int count_list = 0;

	if (sw_action != MLX5_FLOW_RULE_FWD_ACTION_DEST)
		dest_size = 0;

	if (sw_action & MLX5_FLOW_RULE_FWD_ACTION_ALLOW)
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_ALLOW;

	if (sw_action & MLX5_FLOW_RULE_FWD_ACTION_DROP)
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_DROP;

	if (sw_action & MLX5_FLOW_RULE_FWD_ACTION_DEST)
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_FWD_DEST;

	if (flow_act->actions & MLX5_FLOW_ACT_ACTIONS_COUNT) {
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_COUNT;
		count_list = 1;
	}

	inlen = MLX5_ST_SZ_BYTES(set_fte_in) +
		(dest_size + count_list) * MLX5_ST_SZ_BYTES(dest_format_struct);

	if (!dev)
		return -EINVAL;

	if (*fte_status & FS_FTE_STATUS_EXISTING) {
		atomic_mod_cap = MLX5_CAP_FLOWTABLE(dev,
						    flow_table_properties_nic_receive.
						    flow_modify_en);
		if (!atomic_mod_cap)
			return -ENOTSUPP;
		opmod = 1;
		modify_mask = 1 <<
			MLX5_SET_FTE_MODIFY_ENABLE_MASK_DESTINATION_LIST;
	}

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(dev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(set_fte_in, in, opcode, MLX5_CMD_OP_SET_FLOW_TABLE_ENTRY);
	MLX5_SET(set_fte_in, in, op_mod, opmod);
	MLX5_SET(set_fte_in, in, modify_enable_mask, modify_mask);
	MLX5_SET(set_fte_in, in, table_type, type);
	MLX5_SET(set_fte_in, in, table_id,   table_id);
	MLX5_SET(set_fte_in, in, flow_index, index);
	if (vport) {
		MLX5_SET(set_fte_in, in, vport_number, vport);
		MLX5_SET(set_fte_in, in, other_vport, 1);
	}

	in_flow_context = MLX5_ADDR_OF(set_fte_in, in, flow_context);
	MLX5_SET(flow_context, in_flow_context, group_id, group_id);
	if (flow_act->actions & MLX5_FLOW_ACT_ACTIONS_FLOW_TAG)
		MLX5_SET(flow_context, in_flow_context, flow_tag, flow_act->flow_tag);
	if (flow_act->actions & MLX5_FLOW_ACT_ACTIONS_MODIFY_HDR) {
		MLX5_SET(flow_context, in_flow_context, modify_header_id,
			 flow_act->modify_hdr->id);
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_MOD_HDR;
	}
	if (flow_act->actions & MLX5_FLOW_ACT_ACTIONS_PACKET_REFORMAT) {
		MLX5_SET(flow_context, in_flow_context, packet_reformat_id,
			 flow_act->pkt_reformat->id);
		prm_action |= MLX5_FLOW_CONTEXT_ACTION_PACKET_REFORMAT;
	}
	MLX5_SET(flow_context, in_flow_context, destination_list_size,
		 dest_size);
	in_match_value = MLX5_ADDR_OF(flow_context, in_flow_context,
				      match_value);
	memcpy(in_match_value, match_val, MLX5_ST_SZ_BYTES(fte_match_param));

	in_dests = MLX5_ADDR_OF(flow_context, in_flow_context, destination);

	if (dest_size) {
		list_for_each_entry(dst, dests, base.list) {
			unsigned int id;

			MLX5_SET(dest_format_struct, in_dests, destination_type,
				 dst->dest_attr.type);
			if (dst->dest_attr.type ==
				MLX5_FLOW_CONTEXT_DEST_TYPE_FLOW_TABLE)
				id = dst->dest_attr.ft->id;
			else
				id = dst->dest_attr.tir_num;
			MLX5_SET(dest_format_struct, in_dests, destination_id, id);
			in_dests += MLX5_ST_SZ_BYTES(dest_format_struct);
		}
	}

	if (flow_act->actions & MLX5_FLOW_ACT_ACTIONS_COUNT) {
		MLX5_SET(dest_format_struct, in_dests, destination_id,
			 mlx5_fc_id(flow_act->counter));
		in_dests += MLX5_ST_SZ_BYTES(dest_format_struct);
		MLX5_SET(flow_context, in_flow_context, flow_counter_list_size, 1);
	}

	MLX5_SET(flow_context, in_flow_context, action, prm_action);
	err = mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));
	if (!err)
		*fte_status |= FS_FTE_STATUS_EXISTING;

	kvfree(in);

	return err;
}

int mlx5_cmd_fs_delete_fte(struct mlx5_core_dev *dev,
			   u16 vport,
			   enum fs_fte_status *fte_status,
			   enum fs_ft_type type, unsigned int table_id,
			   unsigned int index)
{
	u32 in[MLX5_ST_SZ_DW(delete_fte_in)] = {0};
	u32 out[MLX5_ST_SZ_DW(delete_fte_out)] = {0};
	int err;

	if (!(*fte_status & FS_FTE_STATUS_EXISTING))
		return 0;

	if (!dev)
		return -EINVAL;

	MLX5_SET(delete_fte_in, in, opcode, MLX5_CMD_OP_DELETE_FLOW_TABLE_ENTRY);
	MLX5_SET(delete_fte_in, in, table_type, type);
	MLX5_SET(delete_fte_in, in, table_id, table_id);
	MLX5_SET(delete_fte_in, in, flow_index, index);
	if (vport) {
		MLX5_SET(delete_fte_in, in, vport_number,  vport);
		MLX5_SET(delete_fte_in, in, other_vport, 1);
	}

	err =  mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
	if (!err)
		*fte_status = 0;

	return err;
}

int mlx5_cmd_modify_header_alloc(struct mlx5_core_dev *dev,
				 enum mlx5_flow_namespace_type namespace,
				 u8 num_actions,
				 void *modify_actions,
				 struct mlx5_modify_hdr *modify_hdr)
{
        u32 out[MLX5_ST_SZ_DW(alloc_modify_header_context_out)] = {};
        int max_actions, actions_size, inlen, err;
        void *actions_in;
        u8 table_type;
        u32 *in;

        switch (namespace) {
        case MLX5_FLOW_NAMESPACE_FDB:
                max_actions = MLX5_CAP_ESW_FLOWTABLE_FDB(dev, max_modify_header_actions);
                table_type = FS_FT_FDB;
                break;
        case MLX5_FLOW_NAMESPACE_KERNEL:
        case MLX5_FLOW_NAMESPACE_BYPASS:
                max_actions = MLX5_CAP_FLOWTABLE_NIC_RX(dev, max_modify_header_actions);
                table_type = FS_FT_NIC_RX;
                break;
        case MLX5_FLOW_NAMESPACE_ESW_INGRESS:
                max_actions = MLX5_CAP_ESW_INGRESS_ACL(dev, max_modify_header_actions);
                table_type = FS_FT_ESW_INGRESS_ACL;
                break;
        default:
                return -EOPNOTSUPP;
        }

        if (num_actions > max_actions) {
                mlx5_core_warn(dev, "too many modify header actions %d, max supported %d\n",
                               num_actions, max_actions);
                return -EOPNOTSUPP;
        }

        actions_size = MLX5_UN_SZ_BYTES(set_add_copy_action_in_auto) * num_actions;
        inlen = MLX5_ST_SZ_BYTES(alloc_modify_header_context_in) + actions_size;

        in = kzalloc(inlen, GFP_KERNEL);
        if (!in)
                return -ENOMEM;

        MLX5_SET(alloc_modify_header_context_in, in, opcode,
                 MLX5_CMD_OP_ALLOC_MODIFY_HEADER_CONTEXT);
        MLX5_SET(alloc_modify_header_context_in, in, table_type, table_type);
        MLX5_SET(alloc_modify_header_context_in, in, num_of_actions, num_actions);

        actions_in = MLX5_ADDR_OF(alloc_modify_header_context_in, in, actions);
        memcpy(actions_in, modify_actions, actions_size);

        err = mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));

        modify_hdr->id = MLX5_GET(alloc_modify_header_context_out, out, modify_header_id);
        kfree(in);

        return err;
}

void mlx5_cmd_modify_header_dealloc(struct mlx5_core_dev *dev,
				    struct mlx5_modify_hdr *modify_hdr)
{
	u32 out[MLX5_ST_SZ_DW(dealloc_modify_header_context_out)] = {};
        u32 in[MLX5_ST_SZ_DW(dealloc_modify_header_context_in)] = {};

        MLX5_SET(dealloc_modify_header_context_in, in, opcode,
                 MLX5_CMD_OP_DEALLOC_MODIFY_HEADER_CONTEXT);
        MLX5_SET(dealloc_modify_header_context_in, in, modify_header_id,
                 modify_hdr->id);

        mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}

int mlx5_cmd_packet_reformat_alloc(struct mlx5_core_dev *dev,
				   struct mlx5_pkt_reformat_params *params,
				   enum mlx5_flow_namespace_type namespace,
				   struct mlx5_pkt_reformat *pkt_reformat)
{
        u32 out[MLX5_ST_SZ_DW(alloc_packet_reformat_context_out)] = {};
        void *packet_reformat_context_in;
        int max_encap_size;
        void *reformat;
        int inlen;
        int err;
        u32 *in;

        if (namespace == MLX5_FLOW_NAMESPACE_FDB)
                max_encap_size = MLX5_CAP_ESW(dev, max_encap_header_size);
        else
                max_encap_size = MLX5_CAP_FLOWTABLE(dev, max_encap_header_size);

        if (params->size > max_encap_size) {
                mlx5_core_warn(dev, "encap size %zd too big, max supported is %d\n",
                               params->size, max_encap_size);
                return -EINVAL;
        }

        in = kzalloc(MLX5_ST_SZ_BYTES(alloc_packet_reformat_context_in) +
                     params->size, GFP_KERNEL);
        if (!in)
                return -ENOMEM;

        packet_reformat_context_in = MLX5_ADDR_OF(alloc_packet_reformat_context_in,
                                                  in, packet_reformat_context);
        reformat = MLX5_ADDR_OF(packet_reformat_context_in,
                                packet_reformat_context_in,
                                reformat_data);
        inlen = reformat - (void *)in + params->size;

        MLX5_SET(alloc_packet_reformat_context_in, in, opcode,
                 MLX5_CMD_OP_ALLOC_PACKET_REFORMAT_CONTEXT);
        MLX5_SET(packet_reformat_context_in, packet_reformat_context_in,
                 reformat_data_size, params->size);
        MLX5_SET(packet_reformat_context_in, packet_reformat_context_in,
                 reformat_type, params->type);
        MLX5_SET(packet_reformat_context_in, packet_reformat_context_in,
                 reformat_param_0, params->param_0);
        MLX5_SET(packet_reformat_context_in, packet_reformat_context_in,
                 reformat_param_1, params->param_1);
        if (params->data && params->size)
                memcpy(reformat, params->data, params->size);

        err = mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));

        pkt_reformat->id = MLX5_GET(alloc_packet_reformat_context_out,
                                    out, packet_reformat_id);
        kfree(in);

        return err;
}

void mlx5_cmd_packet_reformat_dealloc(struct mlx5_core_dev *dev,
				      struct mlx5_pkt_reformat *pkt_reformat)
{
        u32 out[MLX5_ST_SZ_DW(dealloc_packet_reformat_context_out)] = {};
	u32 in[MLX5_ST_SZ_DW(dealloc_packet_reformat_context_in)] = {};

        MLX5_SET(dealloc_packet_reformat_context_in, in, opcode,
                 MLX5_CMD_OP_DEALLOC_PACKET_REFORMAT_CONTEXT);
        MLX5_SET(dealloc_packet_reformat_context_in, in, packet_reformat_id,
                 pkt_reformat->id);

        mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}
