/*-
 * Copyright (c) 2017, Mellanox Technologies, Ltd.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef __MLX5_FPGA_H__
#define __MLX5_FPGA_H__

#include <linux/in6.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/mlx5io.h>

enum mlx5_fpga_qpc_field_select {
	MLX5_FPGA_QPC_STATE = BIT(0),
};

struct mlx5_fpga_qp_counters {
	u64 rx_ack_packets;
	u64 rx_send_packets;
	u64 tx_ack_packets;
	u64 tx_send_packets;
	u64 rx_total_drop;
};

struct mlx5_fpga_shell_counters {
	u64 ddr_read_requests;
	u64 ddr_write_requests;
	u64 ddr_read_bytes;
	u64 ddr_write_bytes;
};

int mlx5_fpga_caps(struct mlx5_core_dev *dev);
int mlx5_fpga_query(struct mlx5_core_dev *dev, struct mlx5_fpga_query *query);
int mlx5_fpga_query_mtmp(struct mlx5_core_dev *dev,
			 struct mlx5_fpga_temperature *temp);
int mlx5_fpga_ctrl_op(struct mlx5_core_dev *dev, u8 op);
int mlx5_fpga_access_reg(struct mlx5_core_dev *dev, u8 size, u64 addr,
			 void *buf, bool write);
int mlx5_fpga_sbu_caps(struct mlx5_core_dev *dev, void *caps, int size);
int mlx5_fpga_load(struct mlx5_core_dev *dev, enum mlx5_fpga_image image);
int mlx5_fpga_image_select(struct mlx5_core_dev *dev,
			   enum mlx5_fpga_image image);
int mlx5_fpga_ctrl_connect(struct mlx5_core_dev *dev,
			   enum mlx5_fpga_connect *connect);
int mlx5_fpga_shell_counters(struct mlx5_core_dev *dev, bool clear,
			     struct mlx5_fpga_shell_counters *data);

int mlx5_fpga_create_qp(struct mlx5_core_dev *dev, void *fpga_qpc,
			u32 *fpga_qpn);
int mlx5_fpga_modify_qp(struct mlx5_core_dev *dev, u32 fpga_qpn,
			enum mlx5_fpga_qpc_field_select fields, void *fpga_qpc);
int mlx5_fpga_query_qp(struct mlx5_core_dev *dev, u32 fpga_qpn, void *fpga_qpc);
int mlx5_fpga_query_qp_counters(struct mlx5_core_dev *dev, u32 fpga_qpn,
				bool clear, struct mlx5_fpga_qp_counters *data);
int mlx5_fpga_destroy_qp(struct mlx5_core_dev *dev, u32 fpga_qpn);

#endif /* __MLX5_FPGA_H__ */
