/*-
 * Copyright (c) 2013-2015, Mellanox Technologies, Ltd.  All rights reserved.
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
 *
 * $FreeBSD$
 */

#include <linux/etherdevice.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/vport.h>
#include "mlx5_core.h"

u8 mlx5_query_vport_state(struct mlx5_core_dev *mdev, u8 opmod)
{
	u32 in[MLX5_ST_SZ_DW(query_vport_state_in)];
	u32 out[MLX5_ST_SZ_DW(query_vport_state_out)];
	int err;

	memset(in, 0, sizeof(in));

	MLX5_SET(query_vport_state_in, in, opcode,
		 MLX5_CMD_OP_QUERY_VPORT_STATE);
	MLX5_SET(query_vport_state_in, in, op_mod, opmod);

	err = mlx5_cmd_exec_check_status(mdev, in, sizeof(in), out,
					 sizeof(out));
	if (err)
		mlx5_core_warn(mdev, "MLX5_CMD_OP_QUERY_VPORT_STATE failed\n");

	return MLX5_GET(query_vport_state_out, out, state);
}
EXPORT_SYMBOL_GPL(mlx5_query_vport_state);

static int mlx5_query_nic_vport_context(struct mlx5_core_dev *mdev, u32 vport,
					u32 *out, int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_nic_vport_context_in)];

	memset(in, 0, sizeof(in));

	MLX5_SET(query_nic_vport_context_in, in, opcode,
		 MLX5_CMD_OP_QUERY_NIC_VPORT_CONTEXT);

	MLX5_SET(query_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(query_nic_vport_context_in, in, other_vport, 1);

	return mlx5_cmd_exec_check_status(mdev, in, sizeof(in), out, outlen);
}

int mlx5_vport_alloc_q_counter(struct mlx5_core_dev *mdev, int *counter_set_id)
{
	u32 in[MLX5_ST_SZ_DW(alloc_q_counter_in)];
	u32 out[MLX5_ST_SZ_DW(alloc_q_counter_in)];
	int err;

	memset(in, 0, sizeof(in));
	memset(out, 0, sizeof(out));

	MLX5_SET(alloc_q_counter_in, in, opcode,
		 MLX5_CMD_OP_ALLOC_Q_COUNTER);

	err = mlx5_cmd_exec_check_status(mdev, in, sizeof(in),
					 out, sizeof(out));

	if (err)
		return err;

	*counter_set_id = MLX5_GET(alloc_q_counter_out, out,
				   counter_set_id);
	return err;
}

int mlx5_vport_dealloc_q_counter(struct mlx5_core_dev *mdev,
				 int counter_set_id)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_q_counter_in)];
	u32 out[MLX5_ST_SZ_DW(dealloc_q_counter_out)];

	memset(in, 0, sizeof(in));
	memset(out, 0, sizeof(out));

	MLX5_SET(dealloc_q_counter_in, in, opcode,
		 MLX5_CMD_OP_DEALLOC_Q_COUNTER);
	MLX5_SET(dealloc_q_counter_in, in, counter_set_id,
		 counter_set_id);

	return mlx5_cmd_exec_check_status(mdev, in, sizeof(in),
					  out, sizeof(out));
}

static int mlx5_vport_query_q_counter(struct mlx5_core_dev *mdev,
				      int counter_set_id,
				      int reset,
				      void *out,
				      int out_size)
{
	u32 in[MLX5_ST_SZ_DW(query_q_counter_in)];

	memset(in, 0, sizeof(in));

	MLX5_SET(query_q_counter_in, in, opcode, MLX5_CMD_OP_QUERY_Q_COUNTER);
	MLX5_SET(query_q_counter_in, in, clear, reset);
	MLX5_SET(query_q_counter_in, in, counter_set_id, counter_set_id);

	return mlx5_cmd_exec_check_status(mdev, in, sizeof(in),
					  out, out_size);
}

int mlx5_vport_query_out_of_rx_buffer(struct mlx5_core_dev *mdev,
				      int counter_set_id,
				      u32 *out_of_rx_buffer)
{
	u32 out[MLX5_ST_SZ_DW(query_q_counter_out)];
	int err;

	memset(out, 0, sizeof(out));

	err = mlx5_vport_query_q_counter(mdev, counter_set_id, 0, out,
					 sizeof(out));

	if (err)
		return err;

	*out_of_rx_buffer = MLX5_GET(query_q_counter_out, out,
				     out_of_buffer);
	return err;
}

int mlx5_query_nic_vport_mac_address(struct mlx5_core_dev *mdev,
				     u32 vport, u8 *addr)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_nic_vport_context_out);
	u8 *out_addr;
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	out_addr = MLX5_ADDR_OF(query_nic_vport_context_out, out,
				nic_vport_context.permanent_address);

	err = mlx5_query_nic_vport_context(mdev, vport, out, outlen);
	if (err)
		goto out;

	ether_addr_copy(addr, &out_addr[2]);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_nic_vport_mac_address);

int mlx5_query_nic_vport_system_image_guid(struct mlx5_core_dev *mdev,
					   u64 *system_image_guid)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_nic_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_nic_vport_context(mdev, 0, out, outlen);
	if (err)
		goto out;

	*system_image_guid = MLX5_GET64(query_nic_vport_context_out, out,
					nic_vport_context.system_image_guid);
out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_nic_vport_system_image_guid);

int mlx5_query_nic_vport_node_guid(struct mlx5_core_dev *mdev, u64 *node_guid)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_nic_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_nic_vport_context(mdev, 0, out, outlen);
	if (err)
		goto out;

	*node_guid = MLX5_GET64(query_nic_vport_context_out, out,
				nic_vport_context.node_guid);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_nic_vport_node_guid);

int mlx5_query_nic_vport_port_guid(struct mlx5_core_dev *mdev, u64 *port_guid)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_nic_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_nic_vport_context(mdev, 0, out, outlen);
	if (err)
		goto out;

	*port_guid = MLX5_GET64(query_nic_vport_context_out, out,
				nic_vport_context.port_guid);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_nic_vport_port_guid);

int mlx5_query_nic_vport_qkey_viol_cntr(struct mlx5_core_dev *mdev,
					u16 *qkey_viol_cntr)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_nic_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_nic_vport_context(mdev, 0, out, outlen);
	if (err)
		goto out;

	*qkey_viol_cntr = MLX5_GET(query_nic_vport_context_out, out,
				nic_vport_context.qkey_violation_counter);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_nic_vport_qkey_viol_cntr);

static int mlx5_modify_nic_vport_context(struct mlx5_core_dev *mdev, void *in,
					 int inlen)
{
	u32 out[MLX5_ST_SZ_DW(modify_nic_vport_context_out)];

	MLX5_SET(modify_nic_vport_context_in, in, opcode,
		 MLX5_CMD_OP_MODIFY_NIC_VPORT_CONTEXT);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(mdev, in, inlen, out, sizeof(out));
}

static int mlx5_nic_vport_enable_disable_roce(struct mlx5_core_dev *mdev,
					      int enable_disable)
{
	void *in;
	int inlen = MLX5_ST_SZ_BYTES(modify_nic_vport_context_in);
	int err;

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(mdev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(modify_nic_vport_context_in, in, field_select.roce_en, 1);
	MLX5_SET(modify_nic_vport_context_in, in, nic_vport_context.roce_en,
		 enable_disable);

	err = mlx5_modify_nic_vport_context(mdev, in, inlen);

	kvfree(in);

	return err;
}

int mlx5_set_nic_vport_current_mac(struct mlx5_core_dev *mdev, int vport,
				   bool other_vport, u8 *addr)
{
	void *in;
	int inlen = MLX5_ST_SZ_BYTES(modify_nic_vport_context_in)
		  + MLX5_ST_SZ_BYTES(mac_address_layout);
	u8  *mac_layout;
	u8  *mac_ptr;
	int err;

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(mdev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(modify_nic_vport_context_in, in,
		 opcode, MLX5_CMD_OP_MODIFY_NIC_VPORT_CONTEXT);
	MLX5_SET(modify_nic_vport_context_in, in,
		 vport_number, vport);
	MLX5_SET(modify_nic_vport_context_in, in,
		 other_vport, other_vport);
	MLX5_SET(modify_nic_vport_context_in, in,
		 field_select.addresses_list, 1);
	MLX5_SET(modify_nic_vport_context_in, in,
		 nic_vport_context.allowed_list_type,
		 MLX5_NIC_VPORT_LIST_TYPE_UC);
	MLX5_SET(modify_nic_vport_context_in, in,
		 nic_vport_context.allowed_list_size, 1);

	mac_layout = (u8 *)MLX5_ADDR_OF(modify_nic_vport_context_in, in,
		nic_vport_context.current_uc_mac_address);
	mac_ptr = (u8 *)MLX5_ADDR_OF(mac_address_layout, mac_layout,
		mac_addr_47_32);
	ether_addr_copy(mac_ptr, addr);

	err = mlx5_modify_nic_vport_context(mdev, in, inlen);

	kvfree(in);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_set_nic_vport_current_mac);

int mlx5_set_nic_vport_vlan_list(struct mlx5_core_dev *dev, u32 vport,
				 u16 *vlan_list, int list_len)
{
	void *in, *ctx;
	int i, err;
	int  inlen = MLX5_ST_SZ_BYTES(modify_nic_vport_context_in)
		+ MLX5_ST_SZ_BYTES(vlan_layout) * (int)list_len;

	int max_list_size = 1 << MLX5_CAP_GEN_MAX(dev, log_max_vlan_list);

	if (list_len > max_list_size) {
		mlx5_core_warn(dev, "Requested list size (%d) > (%d) max_list_size\n",
			       list_len, max_list_size);
		return -ENOSPC;
	}

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(dev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(modify_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_nic_vport_context_in, in,
			 other_vport, 1);
	MLX5_SET(modify_nic_vport_context_in, in,
		 field_select.addresses_list, 1);

	ctx = MLX5_ADDR_OF(modify_nic_vport_context_in, in, nic_vport_context);

	MLX5_SET(nic_vport_context, ctx, allowed_list_type,
		 MLX5_NIC_VPORT_LIST_TYPE_VLAN);
	MLX5_SET(nic_vport_context, ctx, allowed_list_size, list_len);

	for (i = 0; i < list_len; i++) {
		u8 *vlan_lout = MLX5_ADDR_OF(nic_vport_context, ctx,
					 current_uc_mac_address[i]);
		MLX5_SET(vlan_layout, vlan_lout, vlan, vlan_list[i]);
	}

	err = mlx5_modify_nic_vport_context(dev, in, inlen);

	kvfree(in);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_set_nic_vport_vlan_list);

int mlx5_set_nic_vport_mc_list(struct mlx5_core_dev *mdev, int vport,
			       u64 *addr_list, size_t addr_list_len)
{
	void *in, *ctx;
	int  inlen = MLX5_ST_SZ_BYTES(modify_nic_vport_context_in)
		  + MLX5_ST_SZ_BYTES(mac_address_layout) * (int)addr_list_len;
	int err;
	size_t i;
	int max_list_sz = 1 << MLX5_CAP_GEN_MAX(mdev, log_max_current_mc_list);

	if ((int)addr_list_len > max_list_sz) {
		mlx5_core_warn(mdev, "Requested list size (%d) > (%d) max_list_size\n",
			       (int)addr_list_len, max_list_sz);
		return -ENOSPC;
	}

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(mdev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(modify_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_nic_vport_context_in, in,
			 other_vport, 1);
	MLX5_SET(modify_nic_vport_context_in, in,
		 field_select.addresses_list, 1);

	ctx = MLX5_ADDR_OF(modify_nic_vport_context_in, in, nic_vport_context);

	MLX5_SET(nic_vport_context, ctx, allowed_list_type,
		 MLX5_NIC_VPORT_LIST_TYPE_MC);
	MLX5_SET(nic_vport_context, ctx, allowed_list_size, addr_list_len);

	for (i = 0; i < addr_list_len; i++) {
		u8 *mac_lout = (u8 *)MLX5_ADDR_OF(nic_vport_context, ctx,
						  current_uc_mac_address[i]);
		u8 *mac_ptr = (u8 *)MLX5_ADDR_OF(mac_address_layout, mac_lout,
						 mac_addr_47_32);
		ether_addr_copy(mac_ptr, (u8 *)&addr_list[i]);
	}

	err = mlx5_modify_nic_vport_context(mdev, in, inlen);

	kvfree(in);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_set_nic_vport_mc_list);

int mlx5_set_nic_vport_promisc(struct mlx5_core_dev *mdev, int vport,
			       bool promisc_mc, bool promisc_uc,
			       bool promisc_all)
{
	u8  in[MLX5_ST_SZ_BYTES(modify_nic_vport_context_in)];
	u8 *ctx = MLX5_ADDR_OF(modify_nic_vport_context_in, in,
			       nic_vport_context);

	memset(in, 0, MLX5_ST_SZ_BYTES(modify_nic_vport_context_in));

	MLX5_SET(modify_nic_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_nic_vport_context_in, in,
			 other_vport, 1);
	MLX5_SET(modify_nic_vport_context_in, in, field_select.promisc, 1);
	if (promisc_mc)
		MLX5_SET(nic_vport_context, ctx, promisc_mc, 1);
	if (promisc_uc)
		MLX5_SET(nic_vport_context, ctx, promisc_uc, 1);
	if (promisc_all)
		MLX5_SET(nic_vport_context, ctx, promisc_all, 1);

	return mlx5_modify_nic_vport_context(mdev, in, sizeof(in));
}
EXPORT_SYMBOL_GPL(mlx5_set_nic_vport_promisc);
int mlx5_set_nic_vport_permanent_mac(struct mlx5_core_dev *mdev, int vport,
				     u8 *addr)
{
	void *in;
	int inlen = MLX5_ST_SZ_BYTES(modify_nic_vport_context_in);
	u8  *mac_ptr;
	int err;

	in = mlx5_vzalloc(inlen);
	if (!in) {
		mlx5_core_warn(mdev, "failed to allocate inbox\n");
		return -ENOMEM;
	}

	MLX5_SET(modify_nic_vport_context_in, in,
		 opcode, MLX5_CMD_OP_MODIFY_NIC_VPORT_CONTEXT);
	MLX5_SET(modify_nic_vport_context_in, in, vport_number, vport);
	MLX5_SET(modify_nic_vport_context_in, in, other_vport, 1);
	MLX5_SET(modify_nic_vport_context_in, in,
		 field_select.permanent_address, 1);
	mac_ptr = (u8 *)MLX5_ADDR_OF(modify_nic_vport_context_in, in,
		nic_vport_context.permanent_address.mac_addr_47_32);
	ether_addr_copy(mac_ptr, addr);

	err = mlx5_modify_nic_vport_context(mdev, in, inlen);

	kvfree(in);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_set_nic_vport_permanent_mac);

int mlx5_nic_vport_enable_roce(struct mlx5_core_dev *mdev)
{
	return mlx5_nic_vport_enable_disable_roce(mdev, 1);
}
EXPORT_SYMBOL_GPL(mlx5_nic_vport_enable_roce);

int mlx5_nic_vport_disable_roce(struct mlx5_core_dev *mdev)
{
	return mlx5_nic_vport_enable_disable_roce(mdev, 0);
}
EXPORT_SYMBOL_GPL(mlx5_nic_vport_disable_roce);

int mlx5_query_hca_vport_context(struct mlx5_core_dev *mdev,
				 u8 port_num, u8 vport_num, u32 *out,
				 int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_hca_vport_context_in)];
	int is_group_manager;

	is_group_manager = MLX5_CAP_GEN(mdev, vport_group_manager);

	memset(in, 0, sizeof(in));

	MLX5_SET(query_hca_vport_context_in, in, opcode,
		 MLX5_CMD_OP_QUERY_HCA_VPORT_CONTEXT);

	if (vport_num) {
		if (is_group_manager) {
			MLX5_SET(query_hca_vport_context_in, in, other_vport,
				 1);
			MLX5_SET(query_hca_vport_context_in, in, vport_number,
				 vport_num);
		} else {
			return -EPERM;
		}
	}

	if (MLX5_CAP_GEN(mdev, num_ports) == 2)
		MLX5_SET(query_hca_vport_context_in, in, port_num, port_num);

	return mlx5_cmd_exec_check_status(mdev, in, sizeof(in), out, outlen);
}

int mlx5_query_hca_vport_system_image_guid(struct mlx5_core_dev *mdev,
					   u64 *system_image_guid)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_hca_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_hca_vport_context(mdev, 1, 0, out, outlen);
	if (err)
		goto out;

	*system_image_guid = MLX5_GET64(query_hca_vport_context_out, out,
					hca_vport_context.system_image_guid);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_hca_vport_system_image_guid);

int mlx5_query_hca_vport_node_guid(struct mlx5_core_dev *mdev, u64 *node_guid)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_hca_vport_context_out);
	int err;

	out = mlx5_vzalloc(outlen);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_hca_vport_context(mdev, 1, 0, out, outlen);
	if (err)
		goto out;

	*node_guid = MLX5_GET64(query_hca_vport_context_out, out,
				hca_vport_context.node_guid);

out:
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_hca_vport_node_guid);

int mlx5_query_hca_vport_gid(struct mlx5_core_dev *dev, u8 port_num,
			     u16 vport_num, u16 gid_index, union ib_gid *gid)
{
	int in_sz = MLX5_ST_SZ_BYTES(query_hca_vport_gid_in);
	int out_sz = MLX5_ST_SZ_BYTES(query_hca_vport_gid_out);
	int is_group_manager;
	void *out = NULL;
	void *in = NULL;
	union ib_gid *tmp;
	int tbsz;
	int nout;
	int err;

	is_group_manager = MLX5_CAP_GEN(dev, vport_group_manager);
	tbsz = mlx5_get_gid_table_len(MLX5_CAP_GEN(dev, gid_table_size));

	if (gid_index > tbsz && gid_index != 0xffff)
		return -EINVAL;

	if (gid_index == 0xffff)
		nout = tbsz;
	else
		nout = 1;

	out_sz += nout * sizeof(*gid);

	in = mlx5_vzalloc(in_sz);
	out = mlx5_vzalloc(out_sz);
	if (!in || !out) {
		err = -ENOMEM;
		goto out;
	}

	MLX5_SET(query_hca_vport_gid_in, in, opcode,
		 MLX5_CMD_OP_QUERY_HCA_VPORT_GID);
	if (vport_num) {
		if (is_group_manager) {
			MLX5_SET(query_hca_vport_gid_in, in, vport_number,
				 vport_num);
			MLX5_SET(query_hca_vport_gid_in, in, other_vport, 1);
		} else {
			err = -EPERM;
			goto out;
		}
	}

	MLX5_SET(query_hca_vport_gid_in, in, gid_index, gid_index);

	if (MLX5_CAP_GEN(dev, num_ports) == 2)
		MLX5_SET(query_hca_vport_gid_in, in, port_num, port_num);

	err = mlx5_cmd_exec(dev, in, in_sz, out, out_sz);
	if (err)
		goto out;

	err = mlx5_cmd_status_to_err_v2(out);
	if (err)
		goto out;

	tmp = (union ib_gid *)MLX5_ADDR_OF(query_hca_vport_gid_out, out, gid);
	gid->global.subnet_prefix = tmp->global.subnet_prefix;
	gid->global.interface_id = tmp->global.interface_id;

out:
	kvfree(in);
	kvfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_hca_vport_gid);

int mlx5_query_hca_vport_pkey(struct mlx5_core_dev *dev, u8 other_vport,
			      u8 port_num, u16 vf_num, u16 pkey_index,
			      u16 *pkey)
{
	int in_sz = MLX5_ST_SZ_BYTES(query_hca_vport_pkey_in);
	int out_sz = MLX5_ST_SZ_BYTES(query_hca_vport_pkey_out);
	int is_group_manager;
	void *out = NULL;
	void *in = NULL;
	void *pkarr;
	int nout;
	int tbsz;
	int err;
	int i;

	is_group_manager = MLX5_CAP_GEN(dev, vport_group_manager);

	tbsz = mlx5_to_sw_pkey_sz(MLX5_CAP_GEN(dev, pkey_table_size));
	if (pkey_index > tbsz && pkey_index != 0xffff)
		return -EINVAL;

	if (pkey_index == 0xffff)
		nout = tbsz;
	else
		nout = 1;

	out_sz += nout * MLX5_ST_SZ_BYTES(pkey);

	in = kzalloc(in_sz, GFP_KERNEL);
	out = kzalloc(out_sz, GFP_KERNEL);

	MLX5_SET(query_hca_vport_pkey_in, in, opcode,
		 MLX5_CMD_OP_QUERY_HCA_VPORT_PKEY);
	if (other_vport) {
		if (is_group_manager) {
			MLX5_SET(query_hca_vport_pkey_in, in, vport_number,
				 vf_num);
			MLX5_SET(query_hca_vport_pkey_in, in, other_vport, 1);
		} else {
			err = -EPERM;
			goto out;
		}
	}
	MLX5_SET(query_hca_vport_pkey_in, in, pkey_index, pkey_index);

	if (MLX5_CAP_GEN(dev, num_ports) == 2)
		MLX5_SET(query_hca_vport_pkey_in, in, port_num, port_num);

	err = mlx5_cmd_exec(dev, in, in_sz, out, out_sz);
	if (err)
		goto out;

	err = mlx5_cmd_status_to_err_v2(out);
	if (err)
		goto out;

	pkarr = MLX5_ADDR_OF(query_hca_vport_pkey_out, out, pkey);
	for (i = 0; i < nout; i++, pkey++,
	     pkarr += MLX5_ST_SZ_BYTES(pkey))
		*pkey = MLX5_GET_PR(pkey, pkarr, pkey);

out:
	kfree(in);
	kfree(out);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_hca_vport_pkey);

static int mlx5_modify_eswitch_vport_context(struct mlx5_core_dev *mdev,
					     u16 vport, void *in, int inlen)
{
	u32 out[MLX5_ST_SZ_DW(modify_esw_vport_context_out)];
	int err;

	memset(out, 0, sizeof(out));

	MLX5_SET(modify_esw_vport_context_in, in, vport_number, vport);
	if (vport)
		MLX5_SET(modify_esw_vport_context_in, in, other_vport, 1);

	MLX5_SET(modify_esw_vport_context_in, in, opcode,
		 MLX5_CMD_OP_MODIFY_ESW_VPORT_CONTEXT);

	err = mlx5_cmd_exec_check_status(mdev, in, inlen,
					 out, sizeof(out));
	if (err)
		mlx5_core_warn(mdev, "MLX5_CMD_OP_MODIFY_ESW_VPORT_CONTEXT failed\n");

	return err;
}

int mlx5_set_eswitch_cvlan_info(struct mlx5_core_dev *mdev, u8 vport,
				u8 insert_mode, u8 strip_mode,
				u16 vlan, u8 cfi, u8 pcp)
{
	u32 in[MLX5_ST_SZ_DW(modify_esw_vport_context_in)];

	memset(in, 0, sizeof(in));

	if (insert_mode != MLX5_MODIFY_ESW_VPORT_CONTEXT_CVLAN_INSERT_NONE) {
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.cvlan_cfi, cfi);
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.cvlan_pcp, pcp);
		MLX5_SET(modify_esw_vport_context_in, in,
			 esw_vport_context.cvlan_id, vlan);
	}

	MLX5_SET(modify_esw_vport_context_in, in,
		 esw_vport_context.vport_cvlan_insert, insert_mode);

	MLX5_SET(modify_esw_vport_context_in, in,
		 esw_vport_context.vport_cvlan_strip, strip_mode);

	MLX5_SET(modify_esw_vport_context_in, in, field_select,
		 MLX5_MODIFY_ESW_VPORT_CONTEXT_FIELD_SELECT_CVLAN_STRIP |
		 MLX5_MODIFY_ESW_VPORT_CONTEXT_FIELD_SELECT_CVLAN_INSERT);

	return mlx5_modify_eswitch_vport_context(mdev, vport, in, sizeof(in));
}
EXPORT_SYMBOL_GPL(mlx5_set_eswitch_cvlan_info);

int mlx5_query_vport_counter(struct mlx5_core_dev *dev,
			     u8 port_num, u16 vport_num,
			     void *out, int out_size)
{
	int in_sz = MLX5_ST_SZ_BYTES(query_vport_counter_in);
	int is_group_manager;
	void *in;
	int err;

	is_group_manager = MLX5_CAP_GEN(dev, vport_group_manager);

	in = mlx5_vzalloc(in_sz);
	if (!in)
		return -ENOMEM;

	MLX5_SET(query_vport_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_VPORT_COUNTER);
	if (vport_num) {
		if (is_group_manager) {
			MLX5_SET(query_vport_counter_in, in, other_vport, 1);
			MLX5_SET(query_vport_counter_in, in, vport_number,
				 vport_num);
		} else {
			err = -EPERM;
			goto ex;
		}
	}
	if (MLX5_CAP_GEN(dev, num_ports) == 2)
		MLX5_SET(query_vport_counter_in, in, port_num, port_num);

	err = mlx5_cmd_exec(dev, in, in_sz, out,  out_size);
	if (err)
		goto ex;
	err = mlx5_cmd_status_to_err_v2(out);
	if (err)
		goto ex;

ex:
	kvfree(in);
	return err;
}
EXPORT_SYMBOL_GPL(mlx5_query_vport_counter);

int mlx5_get_vport_counters(struct mlx5_core_dev *dev, u8 port_num,
			    struct mlx5_vport_counters *vc)
{
	int out_sz = MLX5_ST_SZ_BYTES(query_vport_counter_out);
	void *out;
	int err;

	out = mlx5_vzalloc(out_sz);
	if (!out)
		return -ENOMEM;

	err = mlx5_query_vport_counter(dev, port_num, 0, out, out_sz);
	if (err)
		goto ex;

	vc->received_errors.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_errors.packets);
	vc->received_errors.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_errors.octets);
	vc->transmit_errors.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmit_errors.packets);
	vc->transmit_errors.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmit_errors.octets);
	vc->received_ib_unicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_ib_unicast.packets);
	vc->received_ib_unicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_ib_unicast.octets);
	vc->transmitted_ib_unicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_ib_unicast.packets);
	vc->transmitted_ib_unicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_ib_unicast.octets);
	vc->received_ib_multicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_ib_multicast.packets);
	vc->received_ib_multicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_ib_multicast.octets);
	vc->transmitted_ib_multicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_ib_multicast.packets);
	vc->transmitted_ib_multicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_ib_multicast.octets);
	vc->received_eth_broadcast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_broadcast.packets);
	vc->received_eth_broadcast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_broadcast.octets);
	vc->transmitted_eth_broadcast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_broadcast.packets);
	vc->transmitted_eth_broadcast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_broadcast.octets);
	vc->received_eth_unicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_unicast.octets);
	vc->received_eth_unicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_unicast.packets);
	vc->transmitted_eth_unicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_unicast.octets);
	vc->transmitted_eth_unicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_unicast.packets);
	vc->received_eth_multicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_multicast.octets);
	vc->received_eth_multicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, received_eth_multicast.packets);
	vc->transmitted_eth_multicast.octets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_multicast.octets);
	vc->transmitted_eth_multicast.packets =
		MLX5_GET64(query_vport_counter_out,
			   out, transmitted_eth_multicast.packets);

ex:
	kvfree(out);
	return err;
}
