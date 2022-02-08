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
 *
 * $FreeBSD$
 */

#ifndef __MLX5_TLS_H__
#define	__MLX5_TLS_H__

struct mlx5_core_dev;

int mlx5_encryption_key_create(struct mlx5_core_dev *mdev, u32 pdn,
    const void *p_key, u32 key_len, u32 * p_obj_id);
int mlx5_encryption_key_destroy(struct mlx5_core_dev *mdev, u32 oid);
int mlx5_tls_open_tis(struct mlx5_core_dev *mdev, int tc, int tdn, int pdn, u32 *p_tisn);
void mlx5_tls_close_tis(struct mlx5_core_dev *mdev, u32 tisn);
int mlx5_tls_open_tir(struct mlx5_core_dev *mdev, int tdn, int rqtn, u32 *p_tirn);
void mlx5_tls_close_tir(struct mlx5_core_dev *mdev, u32 tirn);

#endif					/* __MLX5_TLS_H__ */
