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

#include <linux/kernel.h>
#include <linux/module.h>
#include <dev/mlx5/driver.h>
#include "mlx5_core.h"

void mlx5_init_mr_table(struct mlx5_core_dev *dev)
{
	struct mlx5_mr_table *table = &dev->priv.mr_table;

	spin_lock_init(&table->lock);
	INIT_RADIX_TREE(&table->tree, GFP_ATOMIC);
}

void mlx5_cleanup_mr_table(struct mlx5_core_dev *dev)
{
}

int mlx5_core_create_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			  struct mlx5_create_mkey_mbox_in *in, int inlen,
			  mlx5_cmd_cbk_t callback, void *context,
			  struct mlx5_create_mkey_mbox_out *out)
{
	struct mlx5_mr_table *table = &dev->priv.mr_table;
	struct mlx5_create_mkey_mbox_out lout;
	unsigned long flags;
	int err;
	u8 key;

	memset(&lout, 0, sizeof(lout));
	spin_lock_irq(&dev->priv.mkey_lock);
	key = dev->priv.mkey_key++;
	spin_unlock_irq(&dev->priv.mkey_lock);
	in->seg.qpn_mkey7_0 |= cpu_to_be32(key);
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_MKEY);
	if (callback) {
		err = mlx5_cmd_exec_cb(dev, in, inlen, out, sizeof(*out),
				       callback, context);
		return err;
	} else {
		err = mlx5_cmd_exec(dev, in, inlen, &lout, sizeof(lout));
	}

	if (err) {
		mlx5_core_dbg(dev, "cmd exec failed %d\n", err);
		return err;
	}

	if (lout.hdr.status) {
		mlx5_core_dbg(dev, "status %d\n", lout.hdr.status);
		return mlx5_cmd_status_to_err(&lout.hdr);
	}

	mr->iova = be64_to_cpu(in->seg.start_addr);
	mr->size = be64_to_cpu(in->seg.len);
	mr->key = mlx5_idx_to_mkey(be32_to_cpu(lout.mkey) & 0xffffff) | key;
	mr->pd = be32_to_cpu(in->seg.flags_pd) & 0xffffff;

	mlx5_core_dbg(dev, "out 0x%x, key 0x%x, mkey 0x%x\n",
		      be32_to_cpu(lout.mkey), key, mr->key);

	/* connect to MR tree */
	spin_lock_irqsave(&table->lock, flags);
	err = radix_tree_insert(&table->tree, mlx5_mkey_to_idx(mr->key), mr);
	spin_unlock_irqrestore(&table->lock, flags);
	if (err) {
		mlx5_core_warn(dev, "failed radix tree insert of mr 0x%x, %d\n",
			       mr->key, err);
		mlx5_core_destroy_mkey(dev, mr);
	}

	return err;
}
EXPORT_SYMBOL(mlx5_core_create_mkey);

int mlx5_core_destroy_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr)
{
	struct mlx5_mr_table *table = &dev->priv.mr_table;
	u32 in[MLX5_ST_SZ_DW(destroy_mkey_in)];
	u32 out[MLX5_ST_SZ_DW(destroy_mkey_out)];
	struct mlx5_core_mr *deleted_mr;
	unsigned long flags;

	memset(in, 0, sizeof(in));

	spin_lock_irqsave(&table->lock, flags);
	deleted_mr = radix_tree_delete(&table->tree, mlx5_mkey_to_idx(mr->key));
	spin_unlock_irqrestore(&table->lock, flags);
	if (!deleted_mr) {
		mlx5_core_warn(dev, "failed radix tree delete of mr 0x%x\n", mr->key);
		return -ENOENT;
	}

	MLX5_SET(destroy_mkey_in, in, opcode, MLX5_CMD_OP_DESTROY_MKEY);
	MLX5_SET(destroy_mkey_in, in, mkey_index, mlx5_mkey_to_idx(mr->key));

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}
EXPORT_SYMBOL(mlx5_core_destroy_mkey);

int mlx5_core_query_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			 struct mlx5_query_mkey_mbox_out *out, int outlen)
{
	struct mlx5_query_mkey_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(out, 0, outlen);

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_MKEY);
	in.mkey = cpu_to_be32(mlx5_mkey_to_idx(mr->key));
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, outlen);
	if (err)
		return err;

	if (out->hdr.status)
		return mlx5_cmd_status_to_err(&out->hdr);

	return err;
}
EXPORT_SYMBOL(mlx5_core_query_mkey);

int mlx5_core_dump_fill_mkey(struct mlx5_core_dev *dev, struct mlx5_core_mr *mr,
			     u32 *mkey)
{
	struct mlx5_query_special_ctxs_mbox_in in;
	struct mlx5_query_special_ctxs_mbox_out out;
	int err;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_SPECIAL_CONTEXTS);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	*mkey = be32_to_cpu(out.dump_fill_mkey);

	return err;
}
EXPORT_SYMBOL(mlx5_core_dump_fill_mkey);

int mlx5_core_create_psv(struct mlx5_core_dev *dev, u32 pdn,
			 int npsvs, u32 *sig_index)
{
	struct mlx5_allocate_psv_in in;
	struct mlx5_allocate_psv_out out;
	int i, err;

	if (npsvs > MLX5_MAX_PSVS)
		return -EINVAL;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_PSV);
	in.npsv_pd = cpu_to_be32((npsvs << 28) | pdn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err) {
		mlx5_core_err(dev, "cmd exec failed %d\n", err);
		return err;
	}

	if (out.hdr.status) {
		mlx5_core_err(dev, "create_psv bad status %d\n",
			      out.hdr.status);
		return mlx5_cmd_status_to_err(&out.hdr);
	}

	for (i = 0; i < npsvs; i++)
		sig_index[i] = be32_to_cpu(out.psv_idx[i]) & 0xffffff;

	return err;
}
EXPORT_SYMBOL(mlx5_core_create_psv);

int mlx5_core_destroy_psv(struct mlx5_core_dev *dev, int psv_num)
{
	struct mlx5_destroy_psv_in in;
	struct mlx5_destroy_psv_out out;
	int err;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.psv_number = cpu_to_be32(psv_num);
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_PSV);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err) {
		mlx5_core_err(dev, "destroy_psv cmd exec failed %d\n", err);
		goto out;
	}

	if (out.hdr.status) {
		mlx5_core_err(dev, "destroy_psv bad status %d\n",
			      out.hdr.status);
		err = mlx5_cmd_status_to_err(&out.hdr);
		goto out;
	}

out:
	return err;
}
EXPORT_SYMBOL(mlx5_core_destroy_psv);
