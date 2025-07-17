/*-
 * Copyright (c) 2019-2021, Mellanox Technologies, Ltd.  All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/tls.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>
#include <dev/mlx5/mlx5_core/transobj.h>

int mlx5_tls_open_tis(struct mlx5_core_dev *mdev, int tc, int tdn, int pdn, u32 *p_tisn)
{
	u32 in[MLX5_ST_SZ_DW(create_tis_in)] = {};
	void *tisc = MLX5_ADDR_OF(create_tis_in, in, ctx);
	int err;

	MLX5_SET(tisc, tisc, prio, tc);
	MLX5_SET(tisc, tisc, transport_domain, tdn);
	MLX5_SET(tisc, tisc, tls_en, 1);
	MLX5_SET(tisc, tisc, pd, pdn);

	err = mlx5_core_create_tis(mdev, in, sizeof(in), p_tisn);
	if (err)
		return (err);
	else if (*p_tisn == 0)
		return (-EINVAL);
	else
		return (0);	/* success */
}

void mlx5_tls_close_tis(struct mlx5_core_dev *mdev, u32 tisn)
{

	mlx5_core_destroy_tis(mdev, tisn, 0);
}

int mlx5_tls_open_tir(struct mlx5_core_dev *mdev, int tdn, int rqtn, u32 *p_tirn)
{
	u32 in[MLX5_ST_SZ_DW(create_tir_in)] = {};
	void *tirc = MLX5_ADDR_OF(create_tir_in, in, tir_context);
	int err;

        MLX5_SET(tirc, tirc, transport_domain, tdn);
        MLX5_SET(tirc, tirc, disp_type, MLX5_TIRC_DISP_TYPE_INDIRECT);
        MLX5_SET(tirc, tirc, rx_hash_fn, MLX5_TIRC_RX_HASH_FN_HASH_INVERTED_XOR8);
        MLX5_SET(tirc, tirc, indirect_table, rqtn);
        MLX5_SET(tirc, tirc, tls_en, 1);
        MLX5_SET(tirc, tirc, self_lb_en,
                 MLX5_TIRC_SELF_LB_EN_ENABLE_UNICAST |
                 MLX5_TIRC_SELF_LB_EN_ENABLE_MULTICAST);

	err = mlx5_core_create_tir(mdev, in, sizeof(in), p_tirn);
	if (err)
		return (err);
	else if (*p_tirn == 0)
		return (-EINVAL);
	else
		return (0);	/* success */
}

void mlx5_tls_close_tir(struct mlx5_core_dev *mdev, u32 tirn)
{
	mlx5_core_destroy_tir(mdev, tirn, 0);
}
