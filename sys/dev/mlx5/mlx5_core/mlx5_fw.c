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

#include <dev/mlx5/driver.h>
#include <linux/module.h>
#include "mlx5_core.h"

static int mlx5_cmd_query_adapter(struct mlx5_core_dev *dev, u32 *out,
				  int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_adapter_in)];
	int err;

	memset(in, 0, sizeof(in));

	MLX5_SET(query_adapter_in, in, opcode, MLX5_CMD_OP_QUERY_ADAPTER);

	err = mlx5_cmd_exec_check_status(dev, in, sizeof(in), out, outlen);
	return err;
}

int mlx5_query_board_id(struct mlx5_core_dev *dev)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_adapter_out);
	int err;

	out = kzalloc(outlen, GFP_KERNEL);

	err = mlx5_cmd_query_adapter(dev, out, outlen);
	if (err)
		goto out_out;

	memcpy(dev->board_id,
	       MLX5_ADDR_OF(query_adapter_out, out,
			    query_adapter_struct.vsd_contd_psid),
	       MLX5_FLD_SZ_BYTES(query_adapter_out,
				 query_adapter_struct.vsd_contd_psid));

out_out:
	kfree(out);

	return err;
}

int mlx5_core_query_vendor_id(struct mlx5_core_dev *mdev, u32 *vendor_id)
{
	u32 *out;
	int outlen = MLX5_ST_SZ_BYTES(query_adapter_out);
	int err;

	out = kzalloc(outlen, GFP_KERNEL);

	err = mlx5_cmd_query_adapter(mdev, out, outlen);
	if (err)
		goto out_out;

	*vendor_id = MLX5_GET(query_adapter_out, out,
			      query_adapter_struct.ieee_vendor_id);

out_out:
	kfree(out);

	return err;
}
EXPORT_SYMBOL(mlx5_core_query_vendor_id);

static int mlx5_core_query_special_contexts(struct mlx5_core_dev *dev)
{
	u32 in[MLX5_ST_SZ_DW(query_special_contexts_in)];
	u32 out[MLX5_ST_SZ_DW(query_special_contexts_out)];
	int err;

	memset(in, 0, sizeof(in));
	memset(out, 0, sizeof(out));

	MLX5_SET(query_special_contexts_in, in, opcode,
		 MLX5_CMD_OP_QUERY_SPECIAL_CONTEXTS);
	err = mlx5_cmd_exec_check_status(dev, in, sizeof(in), out,
					 sizeof(out));
	if (err)
		return err;

	dev->special_contexts.resd_lkey = MLX5_GET(query_special_contexts_out,
						   out, resd_lkey);

	return err;
}

int mlx5_query_hca_caps(struct mlx5_core_dev *dev)
{
	int err;

	err = mlx5_core_get_caps(dev, MLX5_CAP_GENERAL, HCA_CAP_OPMOD_GET_CUR);
	if (err)
		return err;

	err = mlx5_core_get_caps(dev, MLX5_CAP_GENERAL, HCA_CAP_OPMOD_GET_MAX);
	if (err)
		return err;

	if (MLX5_CAP_GEN(dev, eth_net_offloads)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ETHERNET_OFFLOADS,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ETHERNET_OFFLOADS,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, pg)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ODP,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ODP,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, atomic)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ATOMIC,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ATOMIC,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, roce)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ROCE,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ROCE,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, nic_flow_table)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_FLOW_TABLE,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_FLOW_TABLE,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (
	    MLX5_CAP_GEN(dev, eswitch_flow_table)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ESWITCH_FLOW_TABLE,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ESWITCH_FLOW_TABLE,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, vport_group_manager)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_ESWITCH,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_ESWITCH,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, snapshot)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_SNAPSHOT,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_SNAPSHOT,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, ipoib_enhanced_offloads)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_EOIB_OFFLOADS,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_EOIB_OFFLOADS,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, debug)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_DEBUG,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_DEBUG,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	if (MLX5_CAP_GEN(dev, qos)) {
		err = mlx5_core_get_caps(dev, MLX5_CAP_QOS,
					 HCA_CAP_OPMOD_GET_CUR);
		if (err)
			return err;
		err = mlx5_core_get_caps(dev, MLX5_CAP_QOS,
					 HCA_CAP_OPMOD_GET_MAX);
		if (err)
			return err;
	}

	err = mlx5_core_query_special_contexts(dev);
	if (err)
		return err;

	return 0;
}

int mlx5_cmd_init_hca(struct mlx5_core_dev *dev)
{
	u32 in[MLX5_ST_SZ_DW(init_hca_in)];
	u32 out[MLX5_ST_SZ_DW(init_hca_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(init_hca_in, in, opcode, MLX5_CMD_OP_INIT_HCA);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}

int mlx5_cmd_teardown_hca(struct mlx5_core_dev *dev)
{
	u32 in[MLX5_ST_SZ_DW(teardown_hca_in)];
	u32 out[MLX5_ST_SZ_DW(teardown_hca_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(teardown_hca_in, in, opcode, MLX5_CMD_OP_TEARDOWN_HCA);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}

int mlx5_core_set_dc_cnak_trace(struct mlx5_core_dev *dev, int enable,
				u64 addr)
{
	struct mlx5_cmd_set_dc_cnak_mbox_in *in;
	struct mlx5_cmd_set_dc_cnak_mbox_out out;
	int err;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_SET_DC_CNAK_TRACE);
	in->enable = !!enable << 7;
	in->pa = cpu_to_be64(addr);
	err = mlx5_cmd_exec(dev, in, sizeof(*in), &out, sizeof(out));
	if (err)
		goto out;

	if (out.hdr.status)
		err = mlx5_cmd_status_to_err(&out.hdr);

out:
	kfree(in);

	return err;
}
