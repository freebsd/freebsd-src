/*-
 * Copyright (c) 2018, Mellanox Technologies, Ltd.
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

#ifndef __MLX5_DIAG_CNT_H__
#define	__MLX5_DIAG_CNT_H__

#include <dev/mlx5/driver.h>
#include <dev/mlx5/mlx5_core/mlx5_core.h>

#define	MLX5_DIAG_CNT_SUPPORTED(mdev) \
	(MLX5_CAP_GEN(mdev, debug) && \
	 MLX5_CAP_GEN(mdev, num_of_diagnostic_counters))

int	mlx5_diag_cnt_init(struct mlx5_core_dev *);
void	mlx5_diag_cnt_cleanup(struct mlx5_core_dev *);

int	mlx5_diag_query_params(struct mlx5_core_dev *);
int	mlx5_diag_set_params(struct mlx5_core_dev *);
int	mlx5_diag_query_counters(struct mlx5_core_dev *, u8 * *out_buffer);

#endif					/* __MLX5_DIAG_CNT_H__ */
