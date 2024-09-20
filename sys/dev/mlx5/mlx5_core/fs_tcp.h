/*-
 * Copyright (c) 2020-2021, Mellanox Technologies, Ltd.
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

#ifndef __MLX5E_ACCEL_FS_TCP_H__
#define	__MLX5E_ACCEL_FS_TCP_H__

struct inpcb;
struct mlx5_flow_handle;
struct mlx5e_priv;

int	mlx5e_accel_fs_tcp_create(struct mlx5e_priv *);
void	mlx5e_accel_fs_tcp_destroy(struct mlx5e_priv *);
struct mlx5_flow_handle *
mlx5e_accel_fs_add_inpcb(struct mlx5e_priv *,
    struct inpcb *, uint32_t tirn, uint32_t flow_tag, uint16_t vlan_id);
#define	MLX5E_ACCEL_FS_ADD_INPCB_NO_VLAN 0xFFFF
void	mlx5e_accel_fs_del_inpcb(struct mlx5_flow_handle *);

#endif					/* __MLX5E_ACCEL_FS_TCP_H__ */
