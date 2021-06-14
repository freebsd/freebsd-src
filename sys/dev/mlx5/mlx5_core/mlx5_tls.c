/*-
 * Copyright (c) 2019, Mellanox Technologies, Ltd.  All rights reserved.
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

#include "opt_rss.h"
#include "opt_ratelimit.h"

#include <linux/kernel.h>
#include <linux/module.h>
#include <dev/mlx5/driver.h>
#include <dev/mlx5/tls.h>

#include "mlx5_core.h"
#include "transobj.h"

int mlx5_encryption_key_create(struct mlx5_core_dev *mdev, u32 pdn,
    const void *p_key, u32 key_len, u32 *p_obj_id)
{
	u32 in[MLX5_ST_SZ_DW(create_encryption_key_in)] = {};
	u32 out[MLX5_ST_SZ_DW(create_encryption_key_out)] = {};
	u64 general_obj_types;
	int err;

	general_obj_types = MLX5_CAP_GEN_64(mdev, general_obj_types);
	if (!(general_obj_types & MLX5_HCA_CAP_GENERAL_OBJ_TYPES_ENCRYPTION_KEY))
		return -EINVAL;

	switch (key_len) {
	case 128 / 8:
		memcpy(MLX5_ADDR_OF(create_encryption_key_in, in,
		    encryption_key_object.key[4]), p_key, 128 / 8);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.pd, pdn);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.key_size,
			 MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_KEY_SIZE_128);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.key_type,
			 MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_DEK);
		break;
	case 256 / 8:
		memcpy(MLX5_ADDR_OF(create_encryption_key_in, in,
		    encryption_key_object.key[0]), p_key, 256 / 8);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.pd, pdn);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.key_size,
			 MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_KEY_SIZE_256);
		MLX5_SET(create_encryption_key_in, in, encryption_key_object.key_type,
			 MLX5_GENERAL_OBJECT_TYPE_ENCRYPTION_KEY_TYPE_DEK);
		break;
	default:
		return -EINVAL;
	}

	MLX5_SET(create_encryption_key_in, in, opcode, MLX5_CMD_OP_CREATE_GENERAL_OBJ);
	MLX5_SET(create_encryption_key_in, in, obj_type, MLX5_GENERAL_OBJECT_TYPES_ENCRYPTION_KEY);

	err = mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
	if (err == 0)
		*p_obj_id = MLX5_GET(create_encryption_key_out, out, obj_id);

	/* avoid leaking key on the stack */
	memset(in, 0, sizeof(in));

	return err;
}

int mlx5_encryption_key_destroy(struct mlx5_core_dev *mdev, u32 oid)
{
	u32 in[MLX5_ST_SZ_DW(destroy_encryption_key_in)] = {};
	u32 out[MLX5_ST_SZ_DW(destroy_encryption_key_out)] = {};

	MLX5_SET(destroy_encryption_key_in, in, opcode, MLX5_CMD_OP_DESTROY_GENERAL_OBJ);
	MLX5_SET(destroy_encryption_key_in, in, obj_type, MLX5_GENERAL_OBJECT_TYPES_ENCRYPTION_KEY);
	MLX5_SET(destroy_encryption_key_in, in, obj_id, oid);

	return mlx5_cmd_exec(mdev, in, sizeof(in), out, sizeof(out));
}

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

	mlx5_core_destroy_tis(mdev, tisn);
}
