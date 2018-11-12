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


#include <linux/gfp.h>
#include <dev/mlx5/qp.h>
#include <dev/mlx5/driver.h>

#include "mlx5_core.h"

#include "transobj.h"

static struct mlx5_core_rsc_common *mlx5_get_rsc(struct mlx5_core_dev *dev,
						 u32 rsn)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	struct mlx5_core_rsc_common *common;

	spin_lock(&table->lock);

	common = radix_tree_lookup(&table->tree, rsn);
	if (common)
		atomic_inc(&common->refcount);

	spin_unlock(&table->lock);

	if (!common) {
		mlx5_core_warn(dev, "Async event for bogus resource 0x%x\n",
			       rsn);
		return NULL;
	}
	return common;
}

void mlx5_core_put_rsc(struct mlx5_core_rsc_common *common)
{
	if (atomic_dec_and_test(&common->refcount))
		complete(&common->free);
}

void mlx5_rsc_event(struct mlx5_core_dev *dev, u32 rsn, int event_type)
{
	struct mlx5_core_rsc_common *common = mlx5_get_rsc(dev, rsn);
	struct mlx5_core_qp *qp;

	if (!common)
		return;

	switch (common->res) {
	case MLX5_RES_QP:
		qp = (struct mlx5_core_qp *)common;
		qp->event(qp, event_type);
		break;

	default:
		mlx5_core_warn(dev, "invalid resource type for 0x%x\n", rsn);
	}

	mlx5_core_put_rsc(common);
}

static int create_qprqsq_common(struct mlx5_core_dev *dev,
				struct mlx5_core_qp *qp, int rsc_type)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	int err;

	qp->common.res = rsc_type;

	spin_lock_irq(&table->lock);
	err = radix_tree_insert(&table->tree, qp->qpn | (rsc_type << 24), qp);
	spin_unlock_irq(&table->lock);
	if (err)
		return err;

	atomic_set(&qp->common.refcount, 1);
	init_completion(&qp->common.free);
	qp->pid = curthread->td_proc->p_pid;

	return 0;
}

static void destroy_qprqsq_common(struct mlx5_core_dev *dev,
				  struct mlx5_core_qp *qp, int rsc_type)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	unsigned long flags;

	spin_lock_irqsave(&table->lock, flags);
	radix_tree_delete(&table->tree, qp->qpn | (rsc_type << 24));
	spin_unlock_irqrestore(&table->lock, flags);

	mlx5_core_put_rsc((struct mlx5_core_rsc_common *)qp);
	wait_for_completion(&qp->common.free);
}

int mlx5_core_create_qp(struct mlx5_core_dev *dev,
			struct mlx5_core_qp *qp,
			struct mlx5_create_qp_mbox_in *in,
			int inlen)
{
	struct mlx5_create_qp_mbox_out out;
	struct mlx5_destroy_qp_mbox_in din;
	struct mlx5_destroy_qp_mbox_out dout;
	int err;

	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_QP);

	err = mlx5_cmd_exec(dev, in, inlen, &out, sizeof(out));
	if (err) {
		mlx5_core_warn(dev, "ret %d\n", err);
		return err;
	}

	if (out.hdr.status) {
		mlx5_core_warn(dev, "current num of QPs 0x%x\n",
			       atomic_read(&dev->num_qps));
		return mlx5_cmd_status_to_err(&out.hdr);
	}

	qp->qpn = be32_to_cpu(out.qpn) & 0xffffff;
	mlx5_core_dbg(dev, "qpn = 0x%x\n", qp->qpn);

	err = create_qprqsq_common(dev, qp, MLX5_RES_QP);
	if (err)
		goto err_cmd;

	atomic_inc(&dev->num_qps);

	return 0;

err_cmd:
	memset(&din, 0, sizeof(din));
	memset(&dout, 0, sizeof(dout));
	din.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_QP);
	din.qpn = cpu_to_be32(qp->qpn);
	mlx5_cmd_exec(dev, &din, sizeof(din), &out, sizeof(dout));

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_create_qp);

int mlx5_core_destroy_qp(struct mlx5_core_dev *dev,
			 struct mlx5_core_qp *qp)
{
	struct mlx5_destroy_qp_mbox_in in;
	struct mlx5_destroy_qp_mbox_out out;
	int err;


	destroy_qprqsq_common(dev, qp, MLX5_RES_QP);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_QP);
	in.qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	atomic_dec(&dev->num_qps);
	return 0;
}
EXPORT_SYMBOL_GPL(mlx5_core_destroy_qp);

int mlx5_core_qp_modify(struct mlx5_core_dev *dev, u16 operation,
			struct mlx5_modify_qp_mbox_in *in, int sqd_event,
			struct mlx5_core_qp *qp)
{
	struct mlx5_modify_qp_mbox_out out;
	int err = 0;

	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(operation);
	in->qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, in, sizeof(*in), &out, sizeof(out));
	if (err)
		return err;

	return mlx5_cmd_status_to_err(&out.hdr);
}
EXPORT_SYMBOL_GPL(mlx5_core_qp_modify);

void mlx5_init_qp_table(struct mlx5_core_dev *dev)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;

	spin_lock_init(&table->lock);
	INIT_RADIX_TREE(&table->tree, GFP_ATOMIC);
}

void mlx5_cleanup_qp_table(struct mlx5_core_dev *dev)
{
}

int mlx5_core_qp_query(struct mlx5_core_dev *dev, struct mlx5_core_qp *qp,
		       struct mlx5_query_qp_mbox_out *out, int outlen)
{
	struct mlx5_query_qp_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(out, 0, outlen);
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_QP);
	in.qpn = cpu_to_be32(qp->qpn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, outlen);
	if (err)
		return err;

	if (out->hdr.status)
		return mlx5_cmd_status_to_err(&out->hdr);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_qp_query);

int mlx5_core_xrcd_alloc(struct mlx5_core_dev *dev, u32 *xrcdn)
{
	u32 in[MLX5_ST_SZ_DW(alloc_xrcd_in)];
	u32 out[MLX5_ST_SZ_DW(alloc_xrcd_out)];
	int err;

	memset(in, 0, sizeof(in));

	MLX5_SET(alloc_xrcd_in, in, opcode, MLX5_CMD_OP_ALLOC_XRCD);

	memset(out, 0, sizeof(out));
	err = mlx5_cmd_exec_check_status(dev, in, sizeof(in), out, sizeof(out));
	if (err)
		return err;

	*xrcdn = MLX5_GET(alloc_xrcd_out, out, xrcd);
	return 0;
}
EXPORT_SYMBOL_GPL(mlx5_core_xrcd_alloc);

int mlx5_core_xrcd_dealloc(struct mlx5_core_dev *dev, u32 xrcdn)
{
	u32 in[MLX5_ST_SZ_DW(dealloc_xrcd_in)];
	u32 out[MLX5_ST_SZ_DW(dealloc_xrcd_out)];

	memset(in, 0, sizeof(in));

	MLX5_SET(dealloc_xrcd_in, in, opcode, MLX5_CMD_OP_DEALLOC_XRCD);
	MLX5_SET(dealloc_xrcd_in, in, xrcd, xrcdn);

	memset(out, 0, sizeof(out));
	return mlx5_cmd_exec_check_status(dev, in,  sizeof(in),
					       out, sizeof(out));
}
EXPORT_SYMBOL_GPL(mlx5_core_xrcd_dealloc);

int mlx5_core_create_dct(struct mlx5_core_dev *dev,
			 struct mlx5_core_dct *dct,
			 struct mlx5_create_dct_mbox_in *in)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	struct mlx5_create_dct_mbox_out out;
	struct mlx5_destroy_dct_mbox_in din;
	struct mlx5_destroy_dct_mbox_out dout;
	int err;

	init_completion(&dct->drained);
	memset(&out, 0, sizeof(out));
	in->hdr.opcode = cpu_to_be16(MLX5_CMD_OP_CREATE_DCT);

	err = mlx5_cmd_exec(dev, in, sizeof(*in), &out, sizeof(out));
	if (err) {
		mlx5_core_warn(dev, "create DCT failed, ret %d", err);
		return err;
	}

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	dct->dctn = be32_to_cpu(out.dctn) & 0xffffff;

	dct->common.res = MLX5_RES_DCT;
	spin_lock_irq(&table->lock);
	err = radix_tree_insert(&table->tree, dct->dctn, dct);
	spin_unlock_irq(&table->lock);
	if (err) {
		mlx5_core_warn(dev, "err %d", err);
		goto err_cmd;
	}

	dct->pid = curthread->td_proc->p_pid;
	atomic_set(&dct->common.refcount, 1);
	init_completion(&dct->common.free);

	return 0;

err_cmd:
	memset(&din, 0, sizeof(din));
	memset(&dout, 0, sizeof(dout));
	din.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_DCT);
	din.dctn = cpu_to_be32(dct->dctn);
	mlx5_cmd_exec(dev, &din, sizeof(din), &out, sizeof(dout));

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_create_dct);

static int mlx5_core_drain_dct(struct mlx5_core_dev *dev,
			       struct mlx5_core_dct *dct)
{
	struct mlx5_drain_dct_mbox_out out;
	struct mlx5_drain_dct_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DRAIN_DCT);
	in.dctn = cpu_to_be32(dct->dctn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return 0;
}

int mlx5_core_destroy_dct(struct mlx5_core_dev *dev,
			  struct mlx5_core_dct *dct)
{
	struct mlx5_qp_table *table = &dev->priv.qp_table;
	struct mlx5_destroy_dct_mbox_out out;
	struct mlx5_destroy_dct_mbox_in in;
	unsigned long flags;
	int err;

	err = mlx5_core_drain_dct(dev, dct);
	if (err) {
		mlx5_core_warn(dev, "failed drain DCT 0x%x\n", dct->dctn);
		return err;
	}

	wait_for_completion(&dct->drained);

	spin_lock_irqsave(&table->lock, flags);
	if (radix_tree_delete(&table->tree, dct->dctn) != dct)
		mlx5_core_warn(dev, "dct delete differs\n");
	spin_unlock_irqrestore(&table->lock, flags);

	if (atomic_dec_and_test(&dct->common.refcount))
		complete(&dct->common.free);
	wait_for_completion(&dct->common.free);

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_DESTROY_DCT);
	in.dctn = cpu_to_be32(dct->dctn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return 0;
}
EXPORT_SYMBOL_GPL(mlx5_core_destroy_dct);

int mlx5_core_dct_query(struct mlx5_core_dev *dev, struct mlx5_core_dct *dct,
			struct mlx5_query_dct_mbox_out *out)
{
	struct mlx5_query_dct_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(out, 0, sizeof(*out));
	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_QUERY_DCT);
	in.dctn = cpu_to_be32(dct->dctn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), out, sizeof(*out));
	if (err)
		return err;

	if (out->hdr.status)
		return mlx5_cmd_status_to_err(&out->hdr);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_dct_query);

int mlx5_core_arm_dct(struct mlx5_core_dev *dev, struct mlx5_core_dct *dct)
{
	struct mlx5_arm_dct_mbox_out out;
	struct mlx5_arm_dct_mbox_in in;
	int err;

	memset(&in, 0, sizeof(in));
	memset(&out, 0, sizeof(out));

	in.hdr.opcode = cpu_to_be16(MLX5_CMD_OP_ARM_DCT_FOR_KEY_VIOLATION);
	in.dctn = cpu_to_be32(dct->dctn);
	err = mlx5_cmd_exec(dev, &in, sizeof(in), &out, sizeof(out));
	if (err)
		return err;

	if (out.hdr.status)
		return mlx5_cmd_status_to_err(&out.hdr);

	return err;
}
EXPORT_SYMBOL_GPL(mlx5_core_arm_dct);

int mlx5_core_create_rq_tracked(struct mlx5_core_dev *dev, u32 *in, int inlen,
				struct mlx5_core_qp *rq)
{
	int err;

	err = mlx5_core_create_rq(dev, in, inlen, &rq->qpn);
	if (err)
		return err;

	err = create_qprqsq_common(dev, rq, MLX5_RES_RQ);
	if (err)
		mlx5_core_destroy_rq(dev, rq->qpn);

	return err;
}
EXPORT_SYMBOL(mlx5_core_create_rq_tracked);

void mlx5_core_destroy_rq_tracked(struct mlx5_core_dev *dev,
				  struct mlx5_core_qp *rq)
{
	destroy_qprqsq_common(dev, rq, MLX5_RES_RQ);
	mlx5_core_destroy_rq(dev, rq->qpn);
}
EXPORT_SYMBOL(mlx5_core_destroy_rq_tracked);

int mlx5_core_create_sq_tracked(struct mlx5_core_dev *dev, u32 *in, int inlen,
				struct mlx5_core_qp *sq)
{
	int err;

	err = mlx5_core_create_sq(dev, in, inlen, &sq->qpn);
	if (err)
		return err;

	err = create_qprqsq_common(dev, sq, MLX5_RES_SQ);
	if (err)
		mlx5_core_destroy_sq(dev, sq->qpn);

	return err;
}
EXPORT_SYMBOL(mlx5_core_create_sq_tracked);

void mlx5_core_destroy_sq_tracked(struct mlx5_core_dev *dev,
				  struct mlx5_core_qp *sq)
{
	destroy_qprqsq_common(dev, sq, MLX5_RES_SQ);
	mlx5_core_destroy_sq(dev, sq->qpn);
}
EXPORT_SYMBOL(mlx5_core_destroy_sq_tracked);
