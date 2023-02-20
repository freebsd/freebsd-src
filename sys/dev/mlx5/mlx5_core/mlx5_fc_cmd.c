/*-
 * Copyright (c) 2022 NVIDIA corporation & affiliates.
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
#include <dev/mlx5/device.h>
#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/mlx5_core/mlx5_fc_cmd.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>

int mlx5_cmd_fc_bulk_alloc(struct mlx5_core_dev *dev,
			   enum mlx5_fc_bulk_alloc_bitmask alloc_bitmask,
			   u32 *id)
{
	u32 out[MLX5_ST_SZ_DW(alloc_flow_counter_out)] = {};
	u32 in[MLX5_ST_SZ_DW(alloc_flow_counter_in)] = {};
	int err;

	MLX5_SET(alloc_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_ALLOC_FLOW_COUNTER);
	MLX5_SET(alloc_flow_counter_in, in, flow_counter_bulk, alloc_bitmask);

	err = mlx5_cmd_exec_inout(dev, alloc_flow_counter, in, out);
	if (!err)
		*id = MLX5_GET(alloc_flow_counter_out, out, flow_counter_id);
	return err;
}

int mlx5_cmd_fc_alloc(struct mlx5_core_dev *dev, u32 *id)
{
	return mlx5_cmd_fc_bulk_alloc(dev, 0, id);
}

int mlx5_cmd_fc_free(struct mlx5_core_dev *dev, u32 id)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_flow_counter_in)] = {};

	MLX5_SET(dealloc_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_DEALLOC_FLOW_COUNTER);
	MLX5_SET(dealloc_flow_counter_in, in, flow_counter_id, id);
	return mlx5_cmd_exec_in(dev, dealloc_flow_counter, in);
}

int mlx5_cmd_fc_query(struct mlx5_core_dev *dev, u32 id,
		      u64 *packets, u64 *bytes)
{
	u32 out[MLX5_ST_SZ_BYTES(query_flow_counter_out) +
		MLX5_ST_SZ_BYTES(traffic_counter)] = {};
	u32 in[MLX5_ST_SZ_DW(query_flow_counter_in)] = {};
	void *stats;
	int err = 0;

	MLX5_SET(query_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_FLOW_COUNTER);
	MLX5_SET(query_flow_counter_in, in, op_mod, 0);
	MLX5_SET(query_flow_counter_in, in, flow_counter_id, id);
	err = mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
	if (err)
		return err;

	stats = MLX5_ADDR_OF(query_flow_counter_out, out, flow_statistics);
	*packets = MLX5_GET64(traffic_counter, stats, packets);
	*bytes = MLX5_GET64(traffic_counter, stats, octets);
	return 0;
}

int mlx5_cmd_fc_bulk_query(struct mlx5_core_dev *dev, u32 base_id, int bulk_len,
			   u32 *out)
{
	int outlen = mlx5_cmd_fc_get_bulk_query_out_len(bulk_len);
	u32 in[MLX5_ST_SZ_DW(query_flow_counter_in)] = {};

	MLX5_SET(query_flow_counter_in, in, opcode,
		 MLX5_CMD_OP_QUERY_FLOW_COUNTER);
	MLX5_SET(query_flow_counter_in, in, flow_counter_id, base_id);
	MLX5_SET(query_flow_counter_in, in, num_of_counters, bulk_len);
	return mlx5_cmd_exec(dev, in, sizeof(in), out, outlen);
}

