/* SPDX-License-Identifier: GPL-2.0 OR Linux-OpenIB */
/* Copyright (c) 2021 Mellanox Technologies. */

#ifndef __MLX5_FS_FT_POOL_H__
#define __MLX5_FS_FT_POOL_H__

#include <linux/module.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/mlx5_core/fs_core.h>
#include <linux/compiler.h>

#define POOL_NEXT_SIZE BIT(30)

int mlx5_ft_pool_init(struct mlx5_core_dev *dev);
void mlx5_ft_pool_destroy(struct mlx5_core_dev *dev);

int
mlx5_ft_pool_get_avail_sz(struct mlx5_core_dev *dev, enum fs_flow_table_type table_type,
			  int desired_size);
void
mlx5_ft_pool_put_sz(struct mlx5_core_dev *dev, int sz);

#endif /* __MLX5_FS_FT_POOL_H__ */
