/*-
 * Copyright (c) 2013-2017, Mellanox Technologies, Ltd.  All rights reserved.
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
#include <linux/hardirq.h>
#include <dev/mlx5/driver.h>
#include <rdma/ib_verbs.h>
#include <dev/mlx5/cq.h>
#include "mlx5_core.h"

#include <sys/epoch.h>

static void
mlx5_cq_table_write_lock(struct mlx5_cq_table *table)
{

	atomic_inc(&table->writercount);
	/* make sure all see the updated writercount */
	NET_EPOCH_WAIT();
	spin_lock(&table->writerlock);
}

static void
mlx5_cq_table_write_unlock(struct mlx5_cq_table *table)
{

	spin_unlock(&table->writerlock);
	atomic_dec(&table->writercount);
	/* drain all pending CQ callers */
	NET_EPOCH_WAIT();
}

void mlx5_cq_completion(struct mlx5_core_dev *dev, struct mlx5_eqe *eqe)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	struct mlx5_core_cq *cq;
	struct epoch_tracker et;
	u32 cqn;
	bool do_lock;

	cqn = be32_to_cpu(eqe->data.comp.cqn) & 0xffffff;

	NET_EPOCH_ENTER(et);

	do_lock = atomic_read(&table->writercount) != 0;
	if (unlikely(do_lock))
		spin_lock(&table->writerlock);

	if (likely(cqn < MLX5_CQ_LINEAR_ARRAY_SIZE))
		cq = table->linear_array[cqn].cq;
	else
		cq = radix_tree_lookup(&table->tree, cqn);

	if (unlikely(do_lock))
		spin_unlock(&table->writerlock);

	if (likely(cq != NULL)) {
		++cq->arm_sn;
		cq->comp(cq, eqe);
	} else {
		mlx5_core_warn(dev,
		    "Completion event for bogus CQ 0x%x\n", cqn);
	}

	NET_EPOCH_EXIT(et);
}

void mlx5_cq_event(struct mlx5_core_dev *dev, u32 cqn, int event_type)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	struct mlx5_core_cq *cq;
	struct epoch_tracker et;
	bool do_lock;

	NET_EPOCH_ENTER(et);

	do_lock = atomic_read(&table->writercount) != 0;
	if (unlikely(do_lock))
		spin_lock(&table->writerlock);

	if (likely(cqn < MLX5_CQ_LINEAR_ARRAY_SIZE))
		cq = table->linear_array[cqn].cq;
	else
		cq = radix_tree_lookup(&table->tree, cqn);

	if (unlikely(do_lock))
		spin_unlock(&table->writerlock);

	if (likely(cq != NULL)) {
		cq->event(cq, event_type);
	} else {
		mlx5_core_warn(dev,
		    "Asynchronous event for bogus CQ 0x%x\n", cqn);
	}

	NET_EPOCH_EXIT(et);
}

int mlx5_core_create_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
			u32 *in, int inlen, u32 *out, int outlen)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	u32 din[MLX5_ST_SZ_DW(destroy_cq_in)] = {0};
	u32 dout[MLX5_ST_SZ_DW(destroy_cq_out)] = {0};
	int err;

	memset(out, 0, outlen);
	MLX5_SET(create_cq_in, in, opcode, MLX5_CMD_OP_CREATE_CQ);
	err = mlx5_cmd_exec(dev, in, inlen, out, outlen);
	if (err)
		return err;

	cq->cqn = MLX5_GET(create_cq_out, out, cqn);
	cq->cons_index = 0;
	cq->arm_sn     = 0;

	mlx5_cq_table_write_lock(table);
	err = radix_tree_insert(&table->tree, cq->cqn, cq);
	if (likely(err == 0 && cq->cqn < MLX5_CQ_LINEAR_ARRAY_SIZE))
		table->linear_array[cq->cqn].cq = cq;
	mlx5_cq_table_write_unlock(table);

	if (err)
		goto err_cmd;

	cq->pid = curthread->td_proc->p_pid;
	cq->uar = dev->priv.uar;

	return 0;

err_cmd:
	MLX5_SET(destroy_cq_in, din, opcode, MLX5_CMD_OP_DESTROY_CQ);
	MLX5_SET(destroy_cq_in, din, cqn, cq->cqn);
	mlx5_cmd_exec(dev, din, sizeof(din), dout, sizeof(dout));
	return err;
}
EXPORT_SYMBOL(mlx5_core_create_cq);

int mlx5_core_destroy_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;
	u32 out[MLX5_ST_SZ_DW(destroy_cq_out)] = {0};
	u32 in[MLX5_ST_SZ_DW(destroy_cq_in)] = {0};
	struct mlx5_core_cq *tmp;

	mlx5_cq_table_write_lock(table);
	if (likely(cq->cqn < MLX5_CQ_LINEAR_ARRAY_SIZE))
		table->linear_array[cq->cqn].cq = NULL;
	tmp = radix_tree_delete(&table->tree, cq->cqn);
	mlx5_cq_table_write_unlock(table);

	if (unlikely(tmp == NULL)) {
		mlx5_core_warn(dev, "cq 0x%x not found in tree\n", cq->cqn);
		return -EINVAL;
	} else if (unlikely(tmp != cq)) {
		mlx5_core_warn(dev, "corrupted cqn 0x%x\n", cq->cqn);
		return -EINVAL;
	}

	MLX5_SET(destroy_cq_in, in, opcode, MLX5_CMD_OP_DESTROY_CQ);
	MLX5_SET(destroy_cq_in, in, cqn, cq->cqn);
	return mlx5_cmd_exec(dev, in, sizeof(in), out, sizeof(out));
}
EXPORT_SYMBOL(mlx5_core_destroy_cq);

int mlx5_core_query_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
		       u32 *out, int outlen)
{
	u32 in[MLX5_ST_SZ_DW(query_cq_in)] = {0};

	MLX5_SET(query_cq_in, in, opcode, MLX5_CMD_OP_QUERY_CQ);
	MLX5_SET(query_cq_in, in, cqn, cq->cqn);

	return mlx5_cmd_exec(dev, in, sizeof(in), out, outlen);
}
EXPORT_SYMBOL(mlx5_core_query_cq);


int mlx5_core_modify_cq(struct mlx5_core_dev *dev, struct mlx5_core_cq *cq,
			u32 *in, int inlen)
{
	u32 out[MLX5_ST_SZ_DW(modify_cq_out)] = {0};

	MLX5_SET(modify_cq_in, in, opcode, MLX5_CMD_OP_MODIFY_CQ);
	return mlx5_cmd_exec(dev, in, inlen, out, sizeof(out));
}
EXPORT_SYMBOL(mlx5_core_modify_cq);

int mlx5_core_modify_cq_moderation(struct mlx5_core_dev *dev,
				   struct mlx5_core_cq *cq,
				   u16 cq_period,
				   u16 cq_max_count)
{
	u32 in[MLX5_ST_SZ_DW(modify_cq_in)] = {0};
	void *cqc;

	MLX5_SET(modify_cq_in, in, cqn, cq->cqn);
	cqc = MLX5_ADDR_OF(modify_cq_in, in, cq_context);
	MLX5_SET(cqc, cqc, cq_period, cq_period);
	MLX5_SET(cqc, cqc, cq_max_count, cq_max_count);
	MLX5_SET(modify_cq_in, in,
		 modify_field_select_resize_field_select.modify_field_select.modify_field_select,
		 MLX5_CQ_MODIFY_PERIOD | MLX5_CQ_MODIFY_COUNT);

	return mlx5_core_modify_cq(dev, cq, in, sizeof(in));
}

int mlx5_core_modify_cq_moderation_mode(struct mlx5_core_dev *dev,
					struct mlx5_core_cq *cq,
					u16 cq_period,
					u16 cq_max_count,
					u8 cq_mode)
{
	u32 in[MLX5_ST_SZ_DW(modify_cq_in)] = {0};
	void *cqc;

	MLX5_SET(modify_cq_in, in, cqn, cq->cqn);
	cqc = MLX5_ADDR_OF(modify_cq_in, in, cq_context);
	MLX5_SET(cqc, cqc, cq_period, cq_period);
	MLX5_SET(cqc, cqc, cq_max_count, cq_max_count);
	MLX5_SET(cqc, cqc, cq_period_mode, cq_mode);
	MLX5_SET(modify_cq_in, in,
		 modify_field_select_resize_field_select.modify_field_select.modify_field_select,
		 MLX5_CQ_MODIFY_PERIOD | MLX5_CQ_MODIFY_COUNT | MLX5_CQ_MODIFY_PERIOD_MODE);

	return mlx5_core_modify_cq(dev, cq, in, sizeof(in));
}

int mlx5_init_cq_table(struct mlx5_core_dev *dev)
{
	struct mlx5_cq_table *table = &dev->priv.cq_table;

	memset(table, 0, sizeof(*table));
	spin_lock_init(&table->writerlock);
	INIT_RADIX_TREE(&table->tree, GFP_ATOMIC);

	return 0;
}

void mlx5_cleanup_cq_table(struct mlx5_core_dev *dev)
{
}
