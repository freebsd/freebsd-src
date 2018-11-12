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
#include <linux/hardirq.h>
#include <dev/mlx5/driver.h>
#include <rdma/ib_verbs.h>
#include <dev/mlx5/cq.h>
#include "mlx5_core.h"

void mlx5_cq_completion(struct mlx5_core_dev *dev, u32 cqn)
{
	struct mlx5_core_cq *cq;
	struct mlx5_cq_table *table = &dev->priv.cq_table;

	if (cqn < MLX5_CQ_LINEAR_ARRAY_SIZE) {
		struct mlx5_cq_linear_array_entry *entry;

		entry = &table->linear_array[cqn];
		spin_lock(&entry->lock);
		cq = entry->cq;
		if (cq == NULL) {
			mlx5_core_warn(dev,
			    "Completion event for bogus CQ 0x%x\n", cqn);
		} else {
			++cq->arm_sn;
			cq->comp(cq);
		}
		spin_unlock(&entry->lock);
		return;
	}

	spin_lock(&table->lock);
	cq = radix_tree_lookup(&table->tree, cqn);
	if (likely(cq))
		atomic_inc(&cq->refcount);
	spin_unlock(&table->lock);

	if (!cq) {
		mlx5_core_warn(dev, "Completion event for bogus CQ 0x%x\n", cqn);
		return;
	}

	++cq->arm_sn;

	cq->comp(cq);

	if (atomic_dec_and_test(&cq->refcount))
		complete(&cq->free);
}

void mlx5_cq_event(struct mlx5_core_dev *dev, u32 cqn, int event_type)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	struct mlx5_core_cq *cq;

	spin_lock(&table->lock);

	cq = radix_tree_lookup(&table->tree, cqn);
	if (cq)
		atomic_inc(&cq->refcount);

	spin_unlock(&table->lock);

	if (!cq) {
		mlx5_core_warn(dev, "Async event for bogus CQ 0x%x\n", cqn);
		return;
	}

	cq->event(cq, event_type);

	if (atomic_dec_and_test(&cq->refcount))
		complete(&cq->free);
}


int mlx5_core_create_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
			struct mlx5_create_cq_mbox_in *in, int inlen)
{
	int err;
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	struct mlx5_create_cq_mbox_out out;
	struct mlx5_destroy_cq_mbox_in din;
	struct mlx5_destroy_cq_mbox_out dout;

	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_CQ);
	memset(&out, 0, sizeof(out));
	err = mlx5_cmd_exec(dev, in, inlen, &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	cq->cqn = be32_to_cpu(out.cqn) & 0xffffff;
	cq->cons_index = 0;
	cq->arm_sn     = 0;
	atomic_set(&cq->refcount, 1);
	init_completion(&cq->free);

	spin_lock_irq(&table->lock);
	err = radix_tree_insert(&table->tree, cq->cqn, cq);
	spin_unlock_irq(&table->lock);
	if (err)
		goto err_cmd;

	if (cq->cqn < MLX5_CQ_LINEAR_ARRAY_SIZE) {
		struct mlx5_cq_linear_array_entry *entry;

		entry = &table->linear_array[cq->cqn];
		spin_lock_irq(&entry->lock);
		entry->cq = cq;
		spin_unlock_irq(&entry->lock);
	}

	cq->pid = curthread->td_proc->p_pid;

	return 0;

err_cmd:
	memset(&din, 0, sizeof(din));
	memset(&dout, 0, sizeof(dout));
	din.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_CQ);
	din.cqn = cpu_to_be32(cq->cqn);
	mlx5_cmd_exec(dev, &din, sizeof(din), &dout, sizeof(dout));
	return err;
}
EXPORT_SYMBOL(mlx5_core_create_cq);

int mlx5_core_destroy_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	struct mlx5_destroy_cq_mbox_in in;
	struct mlx5_destroy_cq_mbox_out out;
	struct mlx5_core_cq *tmp;
	int err;

	if (cq->cqn < MLX5_CQ_LINEAR_ARRAY_SIZE) {
		struct mlx5_cq_linear_array_entry *entry;

		entry = &table->linear_array[cq->cqn];
		spin_lock_irq(&entry->lock);
		entry->cq = NULL;
		spin_unlock_irq(&entry->lock);
	}

	spin_lock_irq(&table->lock);
	tmp = radix_tree_delete(&table->tree, cq->cqn);
	spin_unlock_irq(&table->lock);
	if (!tmp) {
		mlx5_core_warn(dev, "cq 0x%x not found in tree\n", cq->cqn);
		return -EINVAL;
	}
	if (tmp != cq) {
		mlx5_core_warn(dev, "corruption on srqn 0x%x\n", cq->cqn);
		return -EINVAL;
	}

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_CQ);
	in.cqn = cpu_to_be32(cq->cqn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		goto out;

	if (out.hdr.status) {
		err = mlx5_cmd_status_to_err(&out.hdr);
		goto out;
	}

	synchronize_irq(cq->irqn);

	if (atomic_dec_and_test(&cq->refcount))
		complete(&cq->free);
	wait_for_completion(&cq->free);

out:

	return err;
}
EXPORT_SYMBOL(mlx5_core_destroy_cq);

int mlx5_core_query_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
		       struct mlx5_query_cq_mbox_out *out)
{
	struct mlx5_query_cq_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(out, 0, sizeof(*out));

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_CQ);
	in.cqn = cpu_to_be32(cq->cqn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, sizeof(*out));
	if (err)
		return err;

	if (out->hdr.status)
		return mlx5_cmd_status_to_err(&out->hdr);

	return err;
}
EXPORT_SYMBOL(mlx5_core_query_cq);


int mlx5_core_modify_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
			struct mlx5_modify_cq_mbox_in *in, int in_sz)
{
	struct mlx5_modify_cq_mbox_out out;
	int err;

	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_MODIFY_CQ);
	err = mlx5_cmd_exec(dev, in, in_sz, &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return 0;
}
EXPORT_SYMBOL(mlx5_core_modify_cq);

int mlx5_core_modify_cq_moderation(struct mlx5_core_dev *dev,
				   struct mlx5_core_cq *cq,
				   u16 cq_period,
				   u16 cq_max_count)
{
	struct mlx5_modify_cq_mbox_in in;

	memset(&in, 0, sizeof(in));

	in.cqn              = cpu_to_be32(cq->cqn);
	in.ctx.cq_period    = cpu_to_be16(cq_period);
	in.ctx.cq_max_count = cpu_to_be16(cq_max_count);
	in.field_select     = cpu_to_be32(MLX5_CQ_MODIFY_PERIOD |
					  MLX5_CQ_MODIFY_COUNT);

	return mlx5_core_modify_cq(dev, cq, &in, sizeof(in));
}

int mlx5_init_cq_table(struct mlx5_core_dev *dev)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	int err;
	int x;

	spin_lock_init(&table->lock);
	for (x = 0; x != MLX5_CQ_LINEAR_ARRAY_SIZE; x++)
		spin_lock_init(&table->linear_array[x].lock);
	INIT_RADIX_TREE(&table->tree, GFP_ATOMIC);
	err = 0;

	return err;
}

void mlx5_cleanup_cq_table(struct mlx5_core_dev *dev)
{
}
