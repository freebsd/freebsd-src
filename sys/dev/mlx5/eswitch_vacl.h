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

#ifndef MLX5_ESWITCH_VACL_TABLE_H
#define MLX5_ESWITCH_VACL_TABLE_H

#include <dev/mlx5/driver.h>

void *mlx5_vacl_table_create(struct mlx5_core_dev *dev,
			     u16 vport, bool is_egress);
void mlx5_vacl_table_cleanup(void *acl_t);
int mlx5_vacl_table_add_vlan(void *acl_t, u16 vlan);
void mlx5_vacl_table_del_vlan(void *acl_t, u16 vlan);
int mlx5_vacl_table_enable_vlan_filter(void *acl_t);
void mlx5_vacl_table_disable_vlan_filter(void *acl_t);
int mlx5_vacl_table_drop_untagged(void *acl_t);
int mlx5_vacl_table_allow_untagged(void *acl_t);
int mlx5_vacl_table_drop_unknown_vlan(void *acl_t);
int mlx5_vacl_table_allow_unknown_vlan(void *acl_t);
int mlx5_vacl_table_set_spoofchk(void *acl_t, bool spoofchk, u8 *vport_mac);

#endif /* MLX5_ESWITCH_VACL_TABLE_H */
