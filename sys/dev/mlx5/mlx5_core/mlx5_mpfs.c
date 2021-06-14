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

#include <linux/types.h>
#include <linux/etherdevice.h>

#include <dev/mlx5/mlx5_ifc.h>
#include <dev/mlx5/device.h>
#include <dev/mlx5/mpfs.h>
#include <dev/mlx5/driver.h>

#include "mlx5_core.h"

#define	MPFS_LOCK(dev) spin_lock(&(dev)->mpfs.spinlock)
#define	MPFS_UNLOCK(dev) spin_unlock(&(dev)->mpfs.spinlock)

int
mlx5_mpfs_add_mac(struct mlx5_core_dev *dev, u32 *p_index, const u8 *mac,
    u8 vlan_valid, u16 vlan)
{
	const u32 l2table_size = MIN(1U << MLX5_CAP_GEN(dev, log_max_l2_table),
	    MLX5_MPFS_TABLE_MAX);
	u32 in[MLX5_ST_SZ_DW(set_l2_table_entry_in)] = {};
	u32 out[MLX5_ST_SZ_DW(set_l2_table_entry_out)] = {};
	u8 *in_mac_addr;
	u32 index;
	int err;

	if (!MLX5_CAP_GEN(dev, eswitch_flow_table)) {
		*p_index = 0;
		return (0);
	}

	MPFS_LOCK(dev);
	index = find_first_zero_bit(dev->mpfs.bitmap, l2table_size);
	if (index < l2table_size)
		set_bit(index, dev->mpfs.bitmap);
	MPFS_UNLOCK(dev);

	if (index >= l2table_size)
		return (-ENOMEM);

	MLX5_SET(set_l2_table_entry_in, in, opcode, MLX5_CMD_OP_SET_L2_TABLE_ENTRY);
	MLX5_SET(set_l2_table_entry_in, in, table_index, index);
	MLX5_SET(set_l2_table_entry_in, in, vlan_valid, vlan_valid);
	MLX5_SET(set_l2_table_entry_in, in, vlan, vlan);

	in_mac_addr = MLX5_ADDR_OF(set_l2_table_entry_in, in, mac_address);
	ether_addr_copy(&in_mac_addr[2], mac);

	err = mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
	if (err != 0) {
		MPFS_LOCK(dev);
		clear_bit(index, dev->mpfs.bitmap);
		MPFS_UNLOCK(dev);
	} else {
		*p_index = index;
	}
	return (err);
}

int
mlx5_mpfs_del_mac(struct mlx5_core_dev *dev, u32 index)
{
	u32 in[MLX5_ST_SZ_DW(delete_l2_table_entry_in)] = {};
	u32 out[MLX5_ST_SZ_DW(delete_l2_table_entry_out)] = {};
	int err;

	if (!MLX5_CAP_GEN(dev, eswitch_flow_table)) {
		if (index != 0)
			return (-EINVAL);
		return (0);
	}

	MLX5_SET(delete_l2_table_entry_in, in, opcode, MLX5_CMD_OP_DELETE_L2_TABLE_ENTRY);
	MLX5_SET(delete_l2_table_entry_in, in, table_index, index);

	err = mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
	if (err == 0) {
		MPFS_LOCK(dev);
		clear_bit(index, dev->mpfs.bitmap);
		MPFS_UNLOCK(dev);
	}
	return (err);
}

int
mlx5_mpfs_init(struct mlx5_core_dev *dev)
{

	spin_lock_init(&dev->mpfs.spinlock);
	bitmap_zero(dev->mpfs.bitmap, MLX5_MPFS_TABLE_MAX);
	return (0);
}

void
mlx5_mpfs_destroy(struct mlx5_core_dev *dev)
{
	u32 num;

	num = bitmap_weight(dev->mpfs.bitmap, MLX5_MPFS_TABLE_MAX);
	if (num != 0)
		mlx5_core_err(dev, "Leaking %u MPFS MAC table entries\n", num);

	spin_lock_destroy(&dev->mpfs.spinlock);
}
