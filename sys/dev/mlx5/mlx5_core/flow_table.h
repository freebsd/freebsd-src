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

#ifndef MLX5_FLOW_TABLE_H
#define MLX5_FLOW_TABLE_H

#include <dev/mlx5/driver.h>

#define MLX5_SET_FLOW_TABLE_ROOT_OPMOD_SET      0x0
#define MLX5_SET_FLOW_TABLE_ROOT_OPMOD_RESET    0x1

struct mlx5_flow_table_group {
	u8	log_sz;
	u8	match_criteria_enable;
	u32	match_criteria[MLX5_ST_SZ_DW(fte_match_param)];
};

void *mlx5_create_flow_table(struct mlx5_core_dev *dev, u8 level, u8 table_type,
			     u16 vport,
			     u16 num_groups,
			     struct mlx5_flow_table_group *group);
void mlx5_destroy_flow_table(void *flow_table);
int mlx5_add_flow_table_entry(void *flow_table, u8 match_criteria_enable,
			      void *match_criteria, void *flow_context,
			      u32 *flow_index);
int mlx5_del_flow_table_entry(void *flow_table, u32 flow_index);
u32 mlx5_get_flow_table_id(void *flow_table);
int mlx5_set_flow_table_root(struct mlx5_core_dev *mdev, u16 op_mod,
			     u8 vport_num, u8 table_type, u32 table_id,
			     u32 underlay_qpn);
void *mlx5_get_flow_table_properties(void *flow_table);
u32 mlx5_set_flow_table_miss_id(void *flow_table, u32 miss_ft_id);

int mlx5_create_flow_counter(struct mlx5_core_dev *dev, u16 *cnt_id);
void mlx5_destroy_flow_counter(struct mlx5_core_dev *dev, u16 cnt_id);
int mlx5_query_flow_counters(struct mlx5_core_dev *dev,
			     u32 num_counters, u16 *cnt_ids,
			     struct mlx5_traffic_counter *cnt_data);
int mlx5_reset_flow_counter(struct mlx5_core_dev *dev, u16 cnt_id);

#endif /* MLX5_FLOW_TABLE_H */
