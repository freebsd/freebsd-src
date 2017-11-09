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

#include <linux/module.h>
#include <rdma/ib_cache.h>
#include <rdma/ib_umem.h>
#include "mlx5_ib.h"
#include "user.h"
#include <dev/mlx5/mlx5_core/transobj.h>
#include <sys/priv.h>

#define	IPV6_DEFAULT_HOPLIMIT 64


static int __mlx5_ib_modify_qp(struct ib_qp *ibqp,
			       const struct ib_qp_attr *attr, int attr_mask,
			       enum ib_qp_state cur_state, enum ib_qp_state new_state);

/* not supported currently */
static int workqueue_signature;

enum {
	MLX5_IB_ACK_REQ_FREQ	= 8,
};

enum {
	MLX5_IB_DEFAULT_SCHED_QUEUE	= 0x83,
	MLX5_IB_DEFAULT_QP0_SCHED_QUEUE	= 0x3f,
	MLX5_IB_LINK_TYPE_IB		= 0,
	MLX5_IB_LINK_TYPE_ETH		= 1
};

enum {
	MLX5_IB_SQ_STRIDE	= 6,
	MLX5_IB_CACHE_LINE_SIZE	= 64,
};

enum {
	MLX5_RQ_NUM_STATE	= MLX5_RQC_STATE_ERR + 1,
	MLX5_SQ_NUM_STATE	= MLX5_SQC_STATE_ERR + 1,
	MLX5_QP_STATE		= MLX5_QP_NUM_STATE + 1,
	MLX5_QP_STATE_BAD	= MLX5_QP_STATE + 1,
};

static const u32 mlx5_ib_opcode[] = {
	[IB_WR_SEND]				= MLX5_OPCODE_SEND,
	[IB_WR_SEND_WITH_IMM]			= MLX5_OPCODE_SEND_IMM,
	[IB_WR_RDMA_WRITE]			= MLX5_OPCODE_RDMA_WRITE,
	[IB_WR_RDMA_WRITE_WITH_IMM]		= MLX5_OPCODE_RDMA_WRITE_IMM,
	[IB_WR_RDMA_READ]			= MLX5_OPCODE_RDMA_READ,
	[IB_WR_ATOMIC_CMP_AND_SWP]		= MLX5_OPCODE_ATOMIC_CS,
	[IB_WR_ATOMIC_FETCH_AND_ADD]		= MLX5_OPCODE_ATOMIC_FA,
	[IB_WR_SEND_WITH_INV]			= MLX5_OPCODE_SEND_INVAL,
	[IB_WR_LOCAL_INV]			= MLX5_OPCODE_UMR,
	[IB_WR_FAST_REG_MR]			= MLX5_OPCODE_UMR,
	[IB_WR_MASKED_ATOMIC_CMP_AND_SWP]	= MLX5_OPCODE_ATOMIC_MASKED_CS,
	[IB_WR_MASKED_ATOMIC_FETCH_AND_ADD]	= MLX5_OPCODE_ATOMIC_MASKED_FA,
};

struct umr_wr {
	u64				virt_addr;
	struct ib_pd		       *pd;
	unsigned int			page_shift;
	unsigned int			npages;
	u32				length;
	int				access_flags;
	u32				mkey;
};

static int is_qp0(enum ib_qp_type qp_type)
{
	return qp_type == IB_QPT_SMI;
}

static int is_qp1(enum ib_qp_type qp_type)
{
	return qp_type == IB_QPT_GSI;
}

static int is_sqp(enum ib_qp_type qp_type)
{
	return is_qp0(qp_type) || is_qp1(qp_type);
}

static void *get_wqe(struct mlx5_ib_qp *qp, int offset)
{
	return mlx5_buf_offset(&qp->buf, offset);
}

static void *get_recv_wqe(struct mlx5_ib_qp *qp, int n)
{
	return get_wqe(qp, qp->rq.offset + (n << qp->rq.wqe_shift));
}

void *mlx5_get_send_wqe(struct mlx5_ib_qp *qp, int n)
{
	return get_wqe(qp, qp->sq.offset + (n << MLX5_IB_SQ_STRIDE));
}


static int
query_wqe_idx(struct mlx5_ib_qp *qp)
{
	struct mlx5_ib_dev *dev = to_mdev(qp->ibqp.device);
	struct mlx5_query_qp_mbox_out *outb;
	struct mlx5_qp_context *context;
	int ret;

	outb = kzalloc(sizeof(*outb), GFP_KERNEL);
	if (!outb)
		return -ENOMEM;

	context = &outb->ctx;

	mutex_lock(&qp->mutex);
	ret = mlx5_core_qp_query(dev->mdev, &qp->mqp, outb, sizeof(*outb));
	if (ret)
		goto out_free;

	ret = be16_to_cpu(context->hw_sq_wqe_counter) & (qp->sq.wqe_cnt - 1);

out_free:
	mutex_unlock(&qp->mutex);
	kfree(outb);

	return ret;
}

static int mlx5_handle_sig_pipelining(struct mlx5_ib_qp *qp)
{
	int wqe_idx;

	wqe_idx = query_wqe_idx(qp);
	if (wqe_idx < 0) {
		printf("mlx5_ib: ERR: ""Failed to query QP 0x%x wqe index\n", qp->mqp.qpn);
		return wqe_idx;
	}

	if (qp->sq.swr_ctx[wqe_idx].sig_piped) {
		struct mlx5_ib_dev *dev = to_mdev(qp->ibqp.device);
		struct mlx5_wqe_ctrl_seg *cwqe;

		cwqe = mlx5_get_send_wqe(qp, wqe_idx);
		cwqe->opmod_idx_opcode = cpu_to_be32(be32_to_cpu(cwqe->opmod_idx_opcode) & 0xffffff00);
		qp->sq.swr_ctx[wqe_idx].w_list.opcode |= MLX5_OPCODE_SIGNATURE_CANCELED;
		mlx5_ib_dbg(dev, "Cancel QP 0x%x wqe_index 0x%x\n",
			    qp->mqp.qpn, wqe_idx);
	}

	return 0;
}

static void mlx5_ib_sqd_work(struct work_struct *work)
{
	struct mlx5_ib_sqd *sqd;
	struct mlx5_ib_qp *qp;
	struct ib_qp_attr qp_attr;

	sqd = container_of(work, struct mlx5_ib_sqd, work);
	qp = sqd->qp;

	if (mlx5_handle_sig_pipelining(qp))
		goto out;

	mutex_lock(&qp->mutex);
	if (__mlx5_ib_modify_qp(&qp->ibqp, &qp_attr, 0, IB_QPS_SQD, IB_QPS_RTS))
		printf("mlx5_ib: ERR: ""Failed to resume QP 0x%x\n", qp->mqp.qpn);
	mutex_unlock(&qp->mutex);
out:
	kfree(sqd);
}

static void mlx5_ib_sigerr_sqd_event(struct mlx5_ib_qp *qp)
{
	struct mlx5_ib_sqd *sqd;

	sqd = kzalloc(sizeof(*sqd), GFP_ATOMIC);
	if (!sqd)
		return;

	sqd->qp = qp;
	INIT_WORK(&sqd->work, mlx5_ib_sqd_work);
	queue_work(mlx5_ib_wq, &sqd->work);
}

static void mlx5_ib_qp_event(struct mlx5_core_qp *qp, int type)
{
	struct ib_qp *ibqp = &to_mibqp(qp)->ibqp;
	struct ib_event event;

	if (type == MLX5_EVENT_TYPE_SQ_DRAINED &&
	    to_mibqp(qp)->state != IB_QPS_SQD) {
		mlx5_ib_sigerr_sqd_event(to_mibqp(qp));
		return;
	}

	if (type == MLX5_EVENT_TYPE_PATH_MIG)
		to_mibqp(qp)->port = to_mibqp(qp)->alt_port;

	if (ibqp->event_handler) {
		event.device     = ibqp->device;
		event.element.qp = ibqp;
		switch (type) {
		case MLX5_EVENT_TYPE_PATH_MIG:
			event.event = IB_EVENT_PATH_MIG;
			break;
		case MLX5_EVENT_TYPE_COMM_EST:
			event.event = IB_EVENT_COMM_EST;
			break;
		case MLX5_EVENT_TYPE_SQ_DRAINED:
			event.event = IB_EVENT_SQ_DRAINED;
			break;
		case MLX5_EVENT_TYPE_SRQ_LAST_WQE:
			event.event = IB_EVENT_QP_LAST_WQE_REACHED;
			break;
		case MLX5_EVENT_TYPE_WQ_CATAS_ERROR:
			event.event = IB_EVENT_QP_FATAL;
			break;
		case MLX5_EVENT_TYPE_PATH_MIG_FAILED:
			event.event = IB_EVENT_PATH_MIG_ERR;
			break;
		case MLX5_EVENT_TYPE_WQ_INVAL_REQ_ERROR:
			event.event = IB_EVENT_QP_REQ_ERR;
			break;
		case MLX5_EVENT_TYPE_WQ_ACCESS_ERROR:
			event.event = IB_EVENT_QP_ACCESS_ERR;
			break;
		default:
			printf("mlx5_ib: WARN: ""mlx5_ib: Unexpected event type %d on QP %06x\n", type, qp->qpn);
			return;
		}

		ibqp->event_handler(&event, ibqp->qp_context);
	}
}

static int set_rq_size(struct mlx5_ib_dev *dev, struct ib_qp_cap *cap,
		       int has_rq, struct mlx5_ib_qp *qp, struct mlx5_ib_create_qp *ucmd)
{
	int wqe_size;
	int wq_size;

	/* Sanity check RQ size before proceeding */
	if (cap->max_recv_wr > (1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz)))
		return -EINVAL;

	if (!has_rq) {
		qp->rq.max_gs = 0;
		qp->rq.wqe_cnt = 0;
		qp->rq.wqe_shift = 0;
		cap->max_recv_wr = 0;
		cap->max_recv_sge = 0;
	} else {
		if (ucmd) {
			qp->rq.wqe_cnt = ucmd->rq_wqe_count;
			qp->rq.wqe_shift = ucmd->rq_wqe_shift;
			qp->rq.max_gs = (1 << qp->rq.wqe_shift) / sizeof(struct mlx5_wqe_data_seg) - qp->wq_sig;
			qp->rq.max_post = qp->rq.wqe_cnt;
		} else {
			wqe_size = qp->wq_sig ? sizeof(struct mlx5_wqe_signature_seg) : 0;
			wqe_size += cap->max_recv_sge * sizeof(struct mlx5_wqe_data_seg);
			wqe_size = roundup_pow_of_two(wqe_size);
			wq_size = roundup_pow_of_two(cap->max_recv_wr) * wqe_size;
			wq_size = max_t(int, wq_size, MLX5_SEND_WQE_BB);
			qp->rq.wqe_cnt = wq_size / wqe_size;
			if (wqe_size > MLX5_CAP_GEN(dev->mdev, max_wqe_sz_rq)) {
				mlx5_ib_dbg(dev, "wqe_size %d, max %d\n",
					    wqe_size,
					    MLX5_CAP_GEN(dev->mdev,
							 max_wqe_sz_rq));
				return -EINVAL;
			}
			qp->rq.wqe_shift = ilog2(wqe_size);
			qp->rq.max_gs = (1 << qp->rq.wqe_shift) / sizeof(struct mlx5_wqe_data_seg) - qp->wq_sig;
			qp->rq.max_post = qp->rq.wqe_cnt;
		}
	}

	return 0;
}

static int sq_overhead(enum ib_qp_type qp_type)
{
	int size = 0;

	switch (qp_type) {
	case IB_QPT_XRC_INI:
		size += sizeof(struct mlx5_wqe_xrc_seg);
		/* fall through */
	case IB_QPT_RC:
		size += sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_atomic_seg) +
			sizeof(struct mlx5_wqe_raddr_seg) +
			sizeof(struct mlx5_wqe_umr_ctrl_seg) +
			sizeof(struct mlx5_mkey_seg);
		break;

	case IB_QPT_XRC_TGT:
		return 0;

	case IB_QPT_UC:
		size += sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_raddr_seg) +
			sizeof(struct mlx5_wqe_umr_ctrl_seg) +
			sizeof(struct mlx5_mkey_seg);
		break;

	case IB_QPT_UD:
	case IB_QPT_SMI:
	case IB_QPT_GSI:
		size += sizeof(struct mlx5_wqe_ctrl_seg) +
			sizeof(struct mlx5_wqe_datagram_seg);
		break;

	default:
		return -EINVAL;
	}

	return size;
}

static int calc_send_wqe(struct ib_qp_init_attr *attr)
{
	int inl_size = 0;
	int size;

	size = sq_overhead(attr->qp_type);
	if (size < 0)
		return size;

	if (attr->cap.max_inline_data) {
		inl_size = size + sizeof(struct mlx5_wqe_inline_seg) +
			attr->cap.max_inline_data;
	}

	size += attr->cap.max_send_sge * sizeof(struct mlx5_wqe_data_seg);
	return ALIGN(max_t(int, inl_size, size), MLX5_SEND_WQE_BB);
}

static int get_send_sge(struct ib_qp_init_attr *attr, int wqe_size)
{
	int max_sge;

	if (attr->qp_type == IB_QPT_RC)
		max_sge = (min_t(int, wqe_size, 512) -
			   sizeof(struct mlx5_wqe_ctrl_seg) -
			   sizeof(struct mlx5_wqe_raddr_seg)) /
			sizeof(struct mlx5_wqe_data_seg);
	else if (attr->qp_type == IB_QPT_XRC_INI)
		max_sge = (min_t(int, wqe_size, 512) -
			   sizeof(struct mlx5_wqe_ctrl_seg) -
			   sizeof(struct mlx5_wqe_xrc_seg) -
			   sizeof(struct mlx5_wqe_raddr_seg)) /
			sizeof(struct mlx5_wqe_data_seg);
	else
		max_sge = (wqe_size - sq_overhead(attr->qp_type)) /
			sizeof(struct mlx5_wqe_data_seg);

	return min_t(int, max_sge, wqe_size - sq_overhead(attr->qp_type) /
		     sizeof(struct mlx5_wqe_data_seg));
}

static int calc_sq_size(struct mlx5_ib_dev *dev, struct ib_qp_init_attr *attr,
			struct mlx5_ib_qp *qp)
{
	int wqe_size;
	int wq_size;

	if (!attr->cap.max_send_wr)
		return 0;

	wqe_size = calc_send_wqe(attr);
	mlx5_ib_dbg(dev, "wqe_size %d\n", wqe_size);
	if (wqe_size < 0)
		return wqe_size;

	if (wqe_size > MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq)) {
		mlx5_ib_warn(dev, "wqe_size(%d) > max_sq_desc_sz(%d)\n",
			     wqe_size, MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq));
		return -EINVAL;
	}

	qp->max_inline_data = wqe_size - sq_overhead(attr->qp_type) -
		sizeof(struct mlx5_wqe_inline_seg);
	attr->cap.max_inline_data = qp->max_inline_data;

	wq_size = roundup_pow_of_two(attr->cap.max_send_wr * (u64)wqe_size);
	qp->sq.wqe_cnt = wq_size / MLX5_SEND_WQE_BB;
	if (qp->sq.wqe_cnt > (1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz))) {
		mlx5_ib_warn(dev, "wqe count(%d) exceeds limits(%d)\n",
			     qp->sq.wqe_cnt,
			     1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz));
		return -ENOMEM;
	}
	qp->sq.wqe_shift = ilog2(MLX5_SEND_WQE_BB);
	qp->sq.max_gs = get_send_sge(attr, wqe_size);
	if (qp->sq.max_gs < attr->cap.max_send_sge) {
		mlx5_ib_warn(dev, "max sge(%d) exceeds limits(%d)\n",
			     qp->sq.max_gs, attr->cap.max_send_sge);
		return -ENOMEM;
	}

	attr->cap.max_send_sge = qp->sq.max_gs;
	qp->sq.max_post = wq_size / wqe_size;
	attr->cap.max_send_wr = qp->sq.max_post;

	return wq_size;
}

static int set_user_buf_size(struct mlx5_ib_dev *dev,
			    struct mlx5_ib_qp *qp,
			    struct mlx5_ib_create_qp *ucmd,
			    struct ib_qp_init_attr *attr)
{
	int desc_sz = 1 << qp->sq.wqe_shift;

	if (desc_sz > MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq)) {
		mlx5_ib_warn(dev, "desc_sz %d, max_sq_desc_sz %d\n",
			     desc_sz, MLX5_CAP_GEN(dev->mdev, max_wqe_sz_sq));
		return -EINVAL;
	}

	if (ucmd->sq_wqe_count && ((1 << ilog2(ucmd->sq_wqe_count)) != ucmd->sq_wqe_count)) {
		mlx5_ib_warn(dev, "sq_wqe_count %d, sq_wqe_count %d\n",
			     ucmd->sq_wqe_count, ucmd->sq_wqe_count);
		return -EINVAL;
	}

	qp->sq.wqe_cnt = ucmd->sq_wqe_count;

	if (qp->sq.wqe_cnt > (1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz))) {
		mlx5_ib_warn(dev, "wqe_cnt %d, max_wqes %d\n",
			     qp->sq.wqe_cnt,
			     1 << MLX5_CAP_GEN(dev->mdev, log_max_qp_sz));
		return -EINVAL;
	}


	if (attr->qp_type == IB_QPT_RAW_PACKET) {
		qp->buf_size = qp->rq.wqe_cnt << qp->rq.wqe_shift;
		qp->sq_buf_size = qp->sq.wqe_cnt << 6;
	} else {
		qp->buf_size = (qp->rq.wqe_cnt << qp->rq.wqe_shift) +
			(qp->sq.wqe_cnt << 6);
		qp->sq_buf_size = 0;
	}

	return 0;
}

static int qp_has_rq(struct ib_qp_init_attr *attr)
{
	if (attr->qp_type == IB_QPT_XRC_INI ||
	    attr->qp_type == IB_QPT_XRC_TGT || attr->srq ||
	    !attr->cap.max_recv_wr)
		return 0;

	return 1;
}

static int first_med_uuar(void)
{
	return 1;
}

static int next_uuar(int n)
{
	n++;

	while (((n % 4) & 2))
		n++;

	return n;
}

static int num_med_uuar(struct mlx5_uuar_info *uuari)
{
	int n;

	n = uuari->num_uars * MLX5_NON_FP_BF_REGS_PER_PAGE -
		uuari->num_low_latency_uuars - 1;

	return n >= 0 ? n : 0;
}

static int max_uuari(struct mlx5_uuar_info *uuari)
{
	return uuari->num_uars * 4;
}

static int first_hi_uuar(struct mlx5_uuar_info *uuari)
{
	int med;
	int i;
	int t;

	med = num_med_uuar(uuari);
	for (t = 0, i = first_med_uuar();; i = next_uuar(i)) {
		t++;
		if (t == med)
			return next_uuar(i);
	}

	return 0;
}

static int alloc_high_class_uuar(struct mlx5_uuar_info *uuari)
{
	int i;

	for (i = first_hi_uuar(uuari); i < max_uuari(uuari); i = next_uuar(i)) {
		if (!test_bit(i, uuari->bitmap)) {
			set_bit(i, uuari->bitmap);
			uuari->count[i]++;
			return i;
		}
	}

	return -ENOMEM;
}

static int alloc_med_class_uuar(struct mlx5_uuar_info *uuari)
{
	int minidx = first_med_uuar();
	int i;

	for (i = first_med_uuar(); i < first_hi_uuar(uuari); i = next_uuar(i)) {
		if (uuari->count[i] < uuari->count[minidx])
			minidx = i;
	}

	uuari->count[minidx]++;

	return minidx;
}

static int alloc_uuar(struct mlx5_uuar_info *uuari,
		      enum mlx5_ib_latency_class lat)
{
	int uuarn = -EINVAL;

	mutex_lock(&uuari->lock);
	switch (lat) {
	case MLX5_IB_LATENCY_CLASS_LOW:
		uuarn = 0;
		uuari->count[uuarn]++;
		break;

	case MLX5_IB_LATENCY_CLASS_MEDIUM:
		if (uuari->ver < 2)
			uuarn = -ENOMEM;
		else
			uuarn = alloc_med_class_uuar(uuari);
		break;

	case MLX5_IB_LATENCY_CLASS_HIGH:
		if (uuari->ver < 2)
			uuarn = -ENOMEM;
		else
			uuarn = alloc_high_class_uuar(uuari);
		break;

	case MLX5_IB_LATENCY_CLASS_FAST_PATH:
		uuarn = 2;
		break;
	}
	mutex_unlock(&uuari->lock);

	return uuarn;
}

static void free_med_class_uuar(struct mlx5_uuar_info *uuari, int uuarn)
{
	clear_bit(uuarn, uuari->bitmap);
	--uuari->count[uuarn];
}

static void free_high_class_uuar(struct mlx5_uuar_info *uuari, int uuarn)
{
	clear_bit(uuarn, uuari->bitmap);
	--uuari->count[uuarn];
}

static void free_uuar(struct mlx5_uuar_info *uuari, int uuarn)
{
	int nuuars = uuari->num_uars * MLX5_BF_REGS_PER_PAGE;
	int high_uuar = nuuars - uuari->num_low_latency_uuars;

	mutex_lock(&uuari->lock);
	if (uuarn == 0) {
		--uuari->count[uuarn];
		goto out;
	}

	if (uuarn < high_uuar) {
		free_med_class_uuar(uuari, uuarn);
		goto out;
	}

	free_high_class_uuar(uuari, uuarn);

out:
	mutex_unlock(&uuari->lock);
}

static enum mlx5_qp_state to_mlx5_state(enum ib_qp_state state)
{
	switch (state) {
	case IB_QPS_RESET:	return MLX5_QP_STATE_RST;
	case IB_QPS_INIT:	return MLX5_QP_STATE_INIT;
	case IB_QPS_RTR:	return MLX5_QP_STATE_RTR;
	case IB_QPS_RTS:	return MLX5_QP_STATE_RTS;
	case IB_QPS_SQD:	return MLX5_QP_STATE_SQD;
	case IB_QPS_SQE:	return MLX5_QP_STATE_SQER;
	case IB_QPS_ERR:	return MLX5_QP_STATE_ERR;
	default:		return -1;
	}
}

static int to_mlx5_st(enum ib_qp_type type)
{
	switch (type) {
	case IB_QPT_RC:			return MLX5_QP_ST_RC;
	case IB_QPT_UC:			return MLX5_QP_ST_UC;
	case IB_QPT_UD:			return MLX5_QP_ST_UD;
	case IB_QPT_XRC_INI:
	case IB_QPT_XRC_TGT:		return MLX5_QP_ST_XRC;
	case IB_QPT_SMI:		return MLX5_QP_ST_QP0;
	case IB_QPT_GSI:		return MLX5_QP_ST_QP1;
	case IB_QPT_RAW_IPV6:		return MLX5_QP_ST_RAW_IPV6;
	case IB_QPT_RAW_PACKET:
	case IB_QPT_RAW_ETHERTYPE:	return MLX5_QP_ST_RAW_ETHERTYPE;
	case IB_QPT_MAX:
	default:		return -EINVAL;
	}
}

static void mlx5_ib_lock_cqs(struct mlx5_ib_cq *send_cq,
			     struct mlx5_ib_cq *recv_cq);
static void mlx5_ib_unlock_cqs(struct mlx5_ib_cq *send_cq,
			       struct mlx5_ib_cq *recv_cq);

static int uuarn_to_uar_index(struct mlx5_uuar_info *uuari, int uuarn)
{
	return uuari->uars[uuarn / MLX5_BF_REGS_PER_PAGE].index;
}

static int create_user_qp(struct mlx5_ib_dev *dev, struct ib_pd *pd,
			  struct mlx5_ib_qp *qp, struct ib_udata *udata,
			  struct ib_qp_init_attr *attr,
			  struct mlx5_create_qp_mbox_in **in,
			  int *inlen,
			  struct mlx5_exp_ib_create_qp *ucmd)
{
	struct mlx5_exp_ib_create_qp_resp resp;
	struct mlx5_ib_ucontext *context;
	int page_shift = 0;
	int uar_index;
	int npages;
	u32 offset = 0;
	int uuarn;
	int ncont = 0;
	int err;

	context = to_mucontext(pd->uobject->context);
	memset(&resp, 0, sizeof(resp));
	resp.size_of_prefix = offsetof(struct mlx5_exp_ib_create_qp_resp, prefix_reserved);
	/*
	 * TBD: should come from the verbs when we have the API
	 */
	if (ucmd->exp.comp_mask & MLX5_EXP_CREATE_QP_MASK_WC_UAR_IDX) {
		if (ucmd->exp.wc_uar_index == MLX5_EXP_CREATE_QP_DB_ONLY_UUAR) {
			/* Assign LATENCY_CLASS_LOW (DB only UUAR) to this QP */
			uuarn = alloc_uuar(&context->uuari, MLX5_IB_LATENCY_CLASS_LOW);
			if (uuarn < 0) {
				mlx5_ib_warn(dev, "DB only uuar allocation failed\n");
				return uuarn;
			}
			uar_index = uuarn_to_uar_index(&context->uuari, uuarn);
		} else if (ucmd->exp.wc_uar_index >= MLX5_IB_MAX_CTX_DYNAMIC_UARS ||
			   context->dynamic_wc_uar_index[ucmd->exp.wc_uar_index] ==
			   MLX5_IB_INVALID_UAR_INDEX) {
			mlx5_ib_warn(dev, "dynamic uuar allocation failed\n");
			return -EINVAL;
		} else {
			uar_index = context->dynamic_wc_uar_index[ucmd->exp.wc_uar_index];
			uuarn = MLX5_EXP_INVALID_UUAR;
		}
	} else {
		uuarn = alloc_uuar(&context->uuari, MLX5_IB_LATENCY_CLASS_HIGH);
		if (uuarn < 0) {
			mlx5_ib_dbg(dev, "failed to allocate low latency UUAR\n");
			mlx5_ib_dbg(dev, "reverting to medium latency\n");
			uuarn = alloc_uuar(&context->uuari, MLX5_IB_LATENCY_CLASS_MEDIUM);
			if (uuarn < 0) {
				mlx5_ib_dbg(dev, "failed to allocate medium latency UUAR\n");
				mlx5_ib_dbg(dev, "reverting to high latency\n");
				uuarn = alloc_uuar(&context->uuari, MLX5_IB_LATENCY_CLASS_LOW);
				if (uuarn < 0) {
					mlx5_ib_warn(dev, "uuar allocation failed\n");
					return uuarn;
				}
			}
		}
		uar_index = uuarn_to_uar_index(&context->uuari, uuarn);
	}
	mlx5_ib_dbg(dev, "uuarn 0x%x, uar_index 0x%x\n", uuarn, uar_index);

	qp->rq.offset = 0;
	qp->sq.wqe_shift = ilog2(MLX5_SEND_WQE_BB);
	qp->sq.offset = qp->rq.wqe_cnt << qp->rq.wqe_shift;

	err = set_user_buf_size(dev, qp, (struct mlx5_ib_create_qp *)ucmd, attr);
	if (err)
		goto err_uuar;

	if (ucmd->buf_addr && qp->buf_size) {
		qp->umem = ib_umem_get(pd->uobject->context, ucmd->buf_addr,
				       qp->buf_size, 0, 0);
		if (IS_ERR(qp->umem)) {
			mlx5_ib_warn(dev, "umem_get failed\n");
			err = PTR_ERR(qp->umem);
			goto err_uuar;
		}
	} else {
		qp->umem = NULL;
	}

	if (qp->umem) {
		mlx5_ib_cont_pages(qp->umem, ucmd->buf_addr, &npages, &page_shift,
				   &ncont, NULL);
		err = mlx5_ib_get_buf_offset(ucmd->buf_addr, page_shift, &offset);
		if (err) {
			mlx5_ib_warn(dev, "bad offset\n");
			goto err_umem;
		}
		mlx5_ib_dbg(dev, "addr 0x%llx, size %d, npages %d, page_shift %d, ncont %d, offset %d\n",
			    (unsigned long long)ucmd->buf_addr, qp->buf_size,
			    npages, page_shift, ncont, offset);
	}

	*inlen = sizeof(**in) + sizeof(*(*in)->pas) * ncont;
	*in = mlx5_vzalloc(*inlen);
	if (!*in) {
		err = -ENOMEM;
		goto err_umem;
	}
	if (qp->umem)
		mlx5_ib_populate_pas(dev, qp->umem, page_shift, (*in)->pas, 0);
	(*in)->ctx.log_pg_sz_remote_qpn =
		cpu_to_be32((page_shift - MLX5_ADAPTER_PAGE_SHIFT) << 24);
	(*in)->ctx.params2 = cpu_to_be32(offset << 6);

	(*in)->ctx.qp_counter_set_usr_page = cpu_to_be32(uar_index);
	resp.uuar_index = uuarn;
	qp->uuarn = uuarn;

	err = mlx5_ib_db_map_user(context, ucmd->db_addr, &qp->db);
	if (err) {
		mlx5_ib_warn(dev, "map failed\n");
		goto err_free;
	}

	err = ib_copy_to_udata(udata, &resp, sizeof(struct mlx5_ib_create_qp_resp));
	if (err) {
		mlx5_ib_err(dev, "copy failed\n");
		goto err_unmap;
	}
	qp->create_type = MLX5_QP_USER;

	return 0;

err_unmap:
	mlx5_ib_db_unmap_user(context, &qp->db);

err_free:
	kvfree(*in);

err_umem:
	if (qp->umem)
		ib_umem_release(qp->umem);

err_uuar:
	free_uuar(&context->uuari, uuarn);
	return err;
}

static void destroy_qp_user(struct ib_pd *pd, struct mlx5_ib_qp *qp)
{
	struct mlx5_ib_ucontext *context;

	context = to_mucontext(pd->uobject->context);
	mlx5_ib_db_unmap_user(context, &qp->db);
	if (qp->umem)
		ib_umem_release(qp->umem);
	if (qp->sq_umem)
		ib_umem_release(qp->sq_umem);
	/*
	 * Free only the UUARs handled by the kernel.
	 * UUARs of UARs allocated dynamically are handled by user.
	 */
	if (qp->uuarn != MLX5_EXP_INVALID_UUAR)
		free_uuar(&context->uuari, qp->uuarn);
}

static int create_kernel_qp(struct mlx5_ib_dev *dev,
			    struct ib_qp_init_attr *init_attr,
			    struct mlx5_ib_qp *qp,
			    struct mlx5_create_qp_mbox_in **in, int *inlen)
{
	enum mlx5_ib_latency_class lc = MLX5_IB_LATENCY_CLASS_LOW;
	struct mlx5_uuar_info *uuari;
	int uar_index;
	int uuarn;
	int err;

	uuari = &dev->mdev->priv.uuari;
	if (init_attr->create_flags & ~(IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK))
		return -EINVAL;

	uuarn = alloc_uuar(uuari, lc);
	if (uuarn < 0) {
		mlx5_ib_warn(dev, "\n");
		return -ENOMEM;
	}

	qp->bf = &uuari->bfs[uuarn];
	uar_index = qp->bf->uar->index;

	err = calc_sq_size(dev, init_attr, qp);
	if (err < 0) {
		mlx5_ib_warn(dev, "err %d\n", err);
		goto err_uuar;
	}

	qp->rq.offset = 0;
	qp->sq.offset = qp->rq.wqe_cnt << qp->rq.wqe_shift;
	qp->buf_size = err + (qp->rq.wqe_cnt << qp->rq.wqe_shift);

	err = mlx5_buf_alloc(dev->mdev, qp->buf_size, PAGE_SIZE * 2, &qp->buf);
	if (err) {
		mlx5_ib_warn(dev, "err %d\n", err);
		goto err_uuar;
	}

	qp->sq.qend = mlx5_get_send_wqe(qp, qp->sq.wqe_cnt);
	*inlen = sizeof(**in) + sizeof(*(*in)->pas) * qp->buf.npages;
	*in = mlx5_vzalloc(*inlen);
	if (!*in) {
		err = -ENOMEM;
		goto err_buf;
	}
	(*in)->ctx.qp_counter_set_usr_page = cpu_to_be32(uar_index);
	(*in)->ctx.log_pg_sz_remote_qpn =
		cpu_to_be32((qp->buf.page_shift - MLX5_ADAPTER_PAGE_SHIFT) << 24);
	/* Set "fast registration enabled" for all kernel QPs */
	(*in)->ctx.params1 |= cpu_to_be32(1 << 11);
	(*in)->ctx.sq_crq_size |= cpu_to_be16(1 << 4);

	mlx5_fill_page_array(&qp->buf, (*in)->pas);

	err = mlx5_db_alloc(dev->mdev, &qp->db);
	if (err) {
		mlx5_ib_warn(dev, "err %d\n", err);
		goto err_free;
	}

	qp->sq.swr_ctx = kcalloc(qp->sq.wqe_cnt, sizeof(*qp->sq.swr_ctx),
				 GFP_KERNEL);
	qp->rq.rwr_ctx = kcalloc(qp->rq.wqe_cnt, sizeof(*qp->rq.rwr_ctx),
				 GFP_KERNEL);
	if (!qp->sq.swr_ctx || !qp->rq.rwr_ctx) {
		err = -ENOMEM;
		goto err_wrid;
	}
	qp->create_type = MLX5_QP_KERNEL;

	return 0;

err_wrid:
	mlx5_db_free(dev->mdev, &qp->db);
	kfree(qp->sq.swr_ctx);
	kfree(qp->rq.rwr_ctx);

err_free:
	kvfree(*in);

err_buf:
	mlx5_buf_free(dev->mdev, &qp->buf);

err_uuar:
	free_uuar(&dev->mdev->priv.uuari, uuarn);
	return err;
}

static void destroy_qp_kernel(struct mlx5_ib_dev *dev, struct mlx5_ib_qp *qp)
{
	mlx5_db_free(dev->mdev, &qp->db);
	kfree(qp->sq.swr_ctx);
	kfree(qp->rq.rwr_ctx);
	mlx5_buf_free(dev->mdev, &qp->buf);
	free_uuar(&dev->mdev->priv.uuari, qp->bf->uuarn);
}

static __be32 get_rx_type(struct mlx5_ib_qp *qp, struct ib_qp_init_attr *attr)
{
	enum ib_qp_type qt = attr->qp_type;

	if (attr->srq || (qt == IB_QPT_XRC_TGT) || (qt == IB_QPT_XRC_INI))
		return cpu_to_be32(MLX5_SRQ_RQ);
	else if (!qp->has_rq)
		return cpu_to_be32(MLX5_ZERO_LEN_RQ);
	else
		return cpu_to_be32(MLX5_NON_ZERO_RQ);
}

static int is_connected(enum ib_qp_type qp_type)
{
	if (qp_type == IB_QPT_RC || qp_type == IB_QPT_UC)
		return 1;

	return 0;
}

static void get_cqs(enum ib_qp_type qp_type,
		    struct ib_cq *ib_send_cq, struct ib_cq *ib_recv_cq,
		    struct mlx5_ib_cq **send_cq, struct mlx5_ib_cq **recv_cq)
{
	switch (qp_type) {
	case IB_QPT_XRC_TGT:
		*send_cq = NULL;
		*recv_cq = NULL;
		break;
	case IB_QPT_XRC_INI:
		*send_cq = ib_send_cq ? to_mcq(ib_send_cq) : NULL;
		*recv_cq = NULL;
		break;

	case IB_QPT_SMI:
	case IB_QPT_GSI:
	case IB_QPT_RC:
	case IB_QPT_UC:
	case IB_QPT_UD:
	case IB_QPT_RAW_IPV6:
	case IB_QPT_RAW_ETHERTYPE:
	case IB_QPT_RAW_PACKET:
		*send_cq = ib_send_cq ? to_mcq(ib_send_cq) : NULL;
		*recv_cq = ib_recv_cq ? to_mcq(ib_recv_cq) : NULL;
		break;

	case IB_QPT_MAX:
	default:
		*send_cq = NULL;
		*recv_cq = NULL;
		break;
	}
}

enum {
	MLX5_QP_END_PAD_MODE_ALIGN	= MLX5_WQ_END_PAD_MODE_ALIGN,
	MLX5_QP_END_PAD_MODE_NONE	= MLX5_WQ_END_PAD_MODE_NONE,
};

static int create_qp_common(struct mlx5_ib_dev *dev, struct ib_pd *pd,
			    struct ib_qp_init_attr *init_attr,
			    struct ib_udata *udata, struct mlx5_ib_qp *qp)
{
	struct mlx5_ib_resources *devr = &dev->devr;
	struct mlx5_core_dev *mdev = dev->mdev;
	struct mlx5_create_qp_mbox_in *in = NULL;
	struct mlx5_exp_ib_create_qp ucmd;
	struct mlx5_ib_create_qp *pucmd = NULL;
	struct mlx5_ib_cq *send_cq;
	struct mlx5_ib_cq *recv_cq;
	unsigned long flags;
	int inlen = sizeof(*in);
	size_t ucmd_size;
	int err;
	int st;
	u32 uidx;
	void *qpc;

	mutex_init(&qp->mutex);
	spin_lock_init(&qp->sq.lock);
	spin_lock_init(&qp->rq.lock);

	if (init_attr->create_flags & IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK) {
		if (!MLX5_CAP_GEN(mdev, block_lb_mc)) {
			mlx5_ib_warn(dev, "block multicast loopback isn't supported\n");
			return -EINVAL;
		} else {
			qp->flags |= MLX5_IB_QP_BLOCK_MULTICAST_LOOPBACK;
		}
	}

	if (init_attr->sq_sig_type == IB_SIGNAL_ALL_WR)
		qp->sq_signal_bits = MLX5_WQE_CTRL_CQ_UPDATE;

	if (pd && pd->uobject) {
		memset(&ucmd, 0, sizeof(ucmd));
		ucmd_size = sizeof(struct mlx5_ib_create_qp);
		if (ucmd_size > offsetof(struct mlx5_exp_ib_create_qp, size_of_prefix)) {
			mlx5_ib_warn(dev, "mlx5_ib_create_qp is too big to fit as prefix of mlx5_exp_ib_create_qp\n");
				return -EINVAL;
		}
		err = ib_copy_from_udata(&ucmd, udata, min(udata->inlen, ucmd_size));
		if (err) {
			mlx5_ib_err(dev, "copy failed\n");
			return err;
		}
		pucmd = (struct mlx5_ib_create_qp *)&ucmd;
		if (ucmd.exp.comp_mask & MLX5_EXP_CREATE_QP_MASK_UIDX)
			uidx = ucmd.exp.uidx;
		else
			uidx = 0xffffff;

		qp->wq_sig = !!(ucmd.flags & MLX5_QP_FLAG_SIGNATURE);
	} else {
		qp->wq_sig = !!workqueue_signature;
		uidx = 0xffffff;
	}

	qp->has_rq = qp_has_rq(init_attr);
	err = set_rq_size(dev, &init_attr->cap, qp->has_rq,
			  qp, (pd && pd->uobject) ? pucmd : NULL);
	if (err) {
		mlx5_ib_warn(dev, "err %d\n", err);
		return err;
	}

	if (pd) {
		if (pd->uobject) {
			__u32 max_wqes =
				1 << MLX5_CAP_GEN(mdev, log_max_qp_sz);
			mlx5_ib_dbg(dev, "requested sq_wqe_count (%d)\n", ucmd.sq_wqe_count);
			if (ucmd.rq_wqe_shift != qp->rq.wqe_shift ||
			    ucmd.rq_wqe_count != qp->rq.wqe_cnt) {
				mlx5_ib_warn(dev, "invalid rq params\n");
				return -EINVAL;
			}
			if (ucmd.sq_wqe_count > max_wqes) {
				mlx5_ib_warn(dev, "requested sq_wqe_count (%d) > max allowed (%d)\n",
					     ucmd.sq_wqe_count, max_wqes);
				return -EINVAL;
			}
			err = create_user_qp(dev, pd, qp, udata, init_attr, &in,
					     &inlen, &ucmd);
			if (err)
				mlx5_ib_warn(dev, "err %d\n", err);
		} else {
			if (init_attr->qp_type == IB_QPT_RAW_PACKET) {
				mlx5_ib_warn(dev, "Raw Eth QP is disabled for Kernel consumers\n");
				return -EINVAL;
			}
			err = create_kernel_qp(dev, init_attr, qp, &in, &inlen);
			if (err)
				mlx5_ib_warn(dev, "err %d\n", err);
			else
				qp->pa_lkey = to_mpd(pd)->pa_lkey;
		}

		if (err)
			return err;
	} else {
		in = mlx5_vzalloc(sizeof(*in));
		if (!in)
			return -ENOMEM;

		qp->create_type = MLX5_QP_EMPTY;
	}

	if (is_sqp(init_attr->qp_type))
		qp->port = init_attr->port_num;

	st = to_mlx5_st(init_attr->qp_type);
	if (st < 0) {
		mlx5_ib_warn(dev, "invalid service type\n");
		err = st;
		goto err_create;
	}
	in->ctx.flags |= cpu_to_be32(st << 16 | MLX5_QP_PM_MIGRATED << 11);

	in->ctx.flags_pd = cpu_to_be32(to_mpd(pd ? pd : devr->p0)->pdn);

	if (qp->wq_sig)
		in->ctx.flags_pd |= cpu_to_be32(MLX5_QP_ENABLE_SIG);

	if (qp->flags & MLX5_IB_QP_BLOCK_MULTICAST_LOOPBACK)
		in->ctx.flags_pd |= cpu_to_be32(MLX5_QP_BLOCK_MCAST);

	if (qp->flags &  MLX5_IB_QP_CAP_RX_END_PADDING)
		in->ctx.flags |= cpu_to_be32(MLX5_QP_END_PAD_MODE_ALIGN << 2);
	else
		in->ctx.flags |= cpu_to_be32(MLX5_QP_END_PAD_MODE_NONE << 2);

	if (qp->scat_cqe && is_connected(init_attr->qp_type)) {
		int rcqe_sz;
		int scqe_sz;

		rcqe_sz = mlx5_ib_get_cqe_size(dev, init_attr->recv_cq);
		scqe_sz = mlx5_ib_get_cqe_size(dev, init_attr->send_cq);

		if (rcqe_sz == 128) {
			in->ctx.cs_res = MLX5_RES_SCAT_DATA64_CQE;
		} else {
			in->ctx.cs_res = MLX5_RES_SCAT_DATA32_CQE;
		}

		if (init_attr->sq_sig_type != IB_SIGNAL_ALL_WR) {
			in->ctx.cs_req = 0;
		} else {
			if (scqe_sz == 128)
				in->ctx.cs_req = MLX5_REQ_SCAT_DATA64_CQE;
			else
				in->ctx.cs_req = MLX5_REQ_SCAT_DATA32_CQE;
		}
	}

	if (qp->rq.wqe_cnt) {
		in->ctx.rq_size_stride = (qp->rq.wqe_shift - 4);
		in->ctx.rq_size_stride |= ilog2(qp->rq.wqe_cnt) << 3;
	}

	in->ctx.rq_type_srqn = get_rx_type(qp, init_attr);

	if (qp->sq.wqe_cnt)
		in->ctx.sq_crq_size |= cpu_to_be16(ilog2(qp->sq.wqe_cnt) << 11);
	else
		in->ctx.sq_crq_size |= cpu_to_be16(0x8000);

	/* Set default resources */
	switch (init_attr->qp_type) {
	case IB_QPT_XRC_TGT:
		in->ctx.cqn_recv = cpu_to_be32(to_mcq(devr->c0)->mcq.cqn);
		in->ctx.cqn_send = cpu_to_be32(to_mcq(devr->c0)->mcq.cqn);
		in->ctx.rq_type_srqn |= cpu_to_be32(to_msrq(devr->s0)->msrq.srqn);
		in->ctx.xrcd = cpu_to_be32(to_mxrcd(init_attr->xrcd)->xrcdn);
		break;
	case IB_QPT_XRC_INI:
		in->ctx.cqn_recv = cpu_to_be32(to_mcq(devr->c0)->mcq.cqn);
		in->ctx.xrcd = cpu_to_be32(to_mxrcd(devr->x1)->xrcdn);
		in->ctx.rq_type_srqn |= cpu_to_be32(to_msrq(devr->s0)->msrq.srqn);
		break;
	default:
		if (init_attr->srq) {
			in->ctx.xrcd = cpu_to_be32(to_mxrcd(devr->x0)->xrcdn);
			in->ctx.rq_type_srqn |= cpu_to_be32(to_msrq(init_attr->srq)->msrq.srqn);
		} else {
			in->ctx.xrcd = cpu_to_be32(to_mxrcd(devr->x1)->xrcdn);
			in->ctx.rq_type_srqn |= cpu_to_be32(to_msrq(devr->s1)->msrq.srqn);
		}
	}

	if (init_attr->send_cq)
		in->ctx.cqn_send = cpu_to_be32(to_mcq(init_attr->send_cq)->mcq.cqn);

	if (init_attr->recv_cq)
		in->ctx.cqn_recv = cpu_to_be32(to_mcq(init_attr->recv_cq)->mcq.cqn);

	in->ctx.db_rec_addr = cpu_to_be64(qp->db.dma);

	if (MLX5_CAP_GEN(mdev, cqe_version)) {
		qpc = MLX5_ADDR_OF(create_qp_in, in, qpc);
		/* 0xffffff means we ask to work with cqe version 0 */
		MLX5_SET(qpc, qpc, user_index, uidx);
	}

	if (init_attr->qp_type == IB_QPT_RAW_PACKET) {
		if (MLX5_CAP_GEN(dev->mdev, port_type) != MLX5_CAP_PORT_TYPE_ETH) {
			mlx5_ib_warn(dev, "Raw Ethernet QP is allowed only for Ethernet link layer\n");
			return -ENOSYS;
		}
		if (ucmd.exp.comp_mask & MLX5_EXP_CREATE_QP_MASK_SQ_BUFF_ADD) {
			qp->sq_buf_addr = ucmd.exp.sq_buf_addr;
		} else {
			mlx5_ib_warn(dev, "Raw Ethernet QP needs SQ buff address\n");
			return -EINVAL;
		}
		err = -EOPNOTSUPP;
	} else {
		err = mlx5_core_create_qp(dev->mdev, &qp->mqp, in, inlen);
		qp->mqp.event = mlx5_ib_qp_event;
	}

	if (err) {
		mlx5_ib_warn(dev, "create qp failed\n");
		goto err_create;
	}

	kvfree(in);
	/* Hardware wants QPN written in big-endian order (after
	 * shifting) for send doorbell.  Precompute this value to save
	 * a little bit when posting sends.
	 */
	qp->doorbell_qpn = swab32(qp->mqp.qpn << 8);

	get_cqs(init_attr->qp_type, init_attr->send_cq, init_attr->recv_cq,
		&send_cq, &recv_cq);
	spin_lock_irqsave(&dev->reset_flow_resource_lock, flags);
	mlx5_ib_lock_cqs(send_cq, recv_cq);
	/* Maintain device to QPs access, needed for further handling via reset
	 * flow
	 */
	list_add_tail(&qp->qps_list, &dev->qp_list);
	/* Maintain CQ to QPs access, needed for further handling via reset flow
	 */
	if (send_cq)
		list_add_tail(&qp->cq_send_list, &send_cq->list_send_qp);
	if (recv_cq)
		list_add_tail(&qp->cq_recv_list, &recv_cq->list_recv_qp);
	mlx5_ib_unlock_cqs(send_cq, recv_cq);
	spin_unlock_irqrestore(&dev->reset_flow_resource_lock, flags);

	return 0;

err_create:
	if (qp->create_type == MLX5_QP_USER)
		destroy_qp_user(pd, qp);
	else if (qp->create_type == MLX5_QP_KERNEL)
		destroy_qp_kernel(dev, qp);

	kvfree(in);
	return err;
}

static void mlx5_ib_lock_cqs(struct mlx5_ib_cq *send_cq, struct mlx5_ib_cq *recv_cq)
	__acquires(&send_cq->lock) __acquires(&recv_cq->lock)
{
	if (send_cq) {
		if (recv_cq) {
			if (send_cq->mcq.cqn < recv_cq->mcq.cqn)  {
				spin_lock(&send_cq->lock);
				spin_lock_nested(&recv_cq->lock,
						 SINGLE_DEPTH_NESTING);
			} else if (send_cq->mcq.cqn == recv_cq->mcq.cqn) {
				spin_lock(&send_cq->lock);
				__acquire(&recv_cq->lock);
			} else {
				spin_lock(&recv_cq->lock);
				spin_lock_nested(&send_cq->lock,
						 SINGLE_DEPTH_NESTING);
			}
		} else {
			spin_lock(&send_cq->lock);
			__acquire(&recv_cq->lock);
		}
	} else if (recv_cq) {
		spin_lock(&recv_cq->lock);
		__acquire(&send_cq->lock);
	} else {
		__acquire(&send_cq->lock);
		__acquire(&recv_cq->lock);
	}
}

static void mlx5_ib_unlock_cqs(struct mlx5_ib_cq *send_cq, struct mlx5_ib_cq *recv_cq)
	__releases(&send_cq->lock) __releases(&recv_cq->lock)
{
	if (send_cq) {
		if (recv_cq) {
			if (send_cq->mcq.cqn < recv_cq->mcq.cqn)  {
				spin_unlock(&recv_cq->lock);
				spin_unlock(&send_cq->lock);
			} else if (send_cq->mcq.cqn == recv_cq->mcq.cqn) {
				__release(&recv_cq->lock);
				spin_unlock(&send_cq->lock);
			} else {
				spin_unlock(&send_cq->lock);
				spin_unlock(&recv_cq->lock);
			}
		} else {
			__release(&recv_cq->lock);
			spin_unlock(&send_cq->lock);
		}
	} else if (recv_cq) {
		__release(&send_cq->lock);
		spin_unlock(&recv_cq->lock);
	} else {
		__release(&recv_cq->lock);
		__release(&send_cq->lock);
	}
}

static struct mlx5_ib_pd *get_pd(struct mlx5_ib_qp *qp)
{
	return to_mpd(qp->ibqp.pd);
}

static void destroy_qp_common(struct mlx5_ib_dev *dev, struct mlx5_ib_qp *qp)
{
	struct mlx5_ib_cq *send_cq, *recv_cq;
	struct mlx5_modify_qp_mbox_in *in;
	unsigned long flags;
	int err;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return;

	if (qp->state != IB_QPS_RESET) {
		if (qp->ibqp.qp_type != IB_QPT_RAW_PACKET) {
			if (mlx5_core_qp_modify(dev->mdev, MLX5_CMD_OP_2RST_QP, in, 0,
						&qp->mqp))
			mlx5_ib_warn(dev, "mlx5_ib: modify QP %06x to RESET failed\n",
				     qp->mqp.qpn);
		}
	}

	get_cqs(qp->ibqp.qp_type, qp->ibqp.send_cq, qp->ibqp.recv_cq,
		&send_cq, &recv_cq);

	spin_lock_irqsave(&dev->reset_flow_resource_lock, flags);
	mlx5_ib_lock_cqs(send_cq, recv_cq);
	/* del from lists under both locks above to protect reset flow paths */
	list_del(&qp->qps_list);
	if (send_cq)
		list_del(&qp->cq_send_list);

	if (recv_cq)
		list_del(&qp->cq_recv_list);

	if (qp->create_type == MLX5_QP_KERNEL) {
		__mlx5_ib_cq_clean(recv_cq, qp->mqp.qpn,
				   qp->ibqp.srq ? to_msrq(qp->ibqp.srq) : NULL);
		if (send_cq != recv_cq)
			__mlx5_ib_cq_clean(send_cq, qp->mqp.qpn, NULL);
	}
	mlx5_ib_unlock_cqs(send_cq, recv_cq);
	spin_unlock_irqrestore(&dev->reset_flow_resource_lock, flags);

	if (qp->ibqp.qp_type == IB_QPT_RAW_PACKET) {
	} else {
		err = mlx5_core_destroy_qp(dev->mdev, &qp->mqp);
		if (err)
			mlx5_ib_warn(dev, "failed to destroy QP 0x%x\n",
				     qp->mqp.qpn);
	}

	kfree(in);

	if (qp->create_type == MLX5_QP_KERNEL)
		destroy_qp_kernel(dev, qp);
	else if (qp->create_type == MLX5_QP_USER)
		destroy_qp_user(&get_pd(qp)->ibpd, qp);
}

static const char *ib_qp_type_str(enum ib_qp_type type)
{
	switch (type) {
	case IB_QPT_SMI:
		return "IB_QPT_SMI";
	case IB_QPT_GSI:
		return "IB_QPT_GSI";
	case IB_QPT_RC:
		return "IB_QPT_RC";
	case IB_QPT_UC:
		return "IB_QPT_UC";
	case IB_QPT_UD:
		return "IB_QPT_UD";
	case IB_QPT_RAW_IPV6:
		return "IB_QPT_RAW_IPV6";
	case IB_QPT_RAW_ETHERTYPE:
		return "IB_QPT_RAW_ETHERTYPE";
	case IB_QPT_XRC_INI:
		return "IB_QPT_XRC_INI";
	case IB_QPT_XRC_TGT:
		return "IB_QPT_XRC_TGT";
	case IB_QPT_RAW_PACKET:
		return "IB_QPT_RAW_PACKET";
	case IB_QPT_MAX:
	default:
		return "Invalid QP type";
	}
}

struct ib_qp *mlx5_ib_create_qp(struct ib_pd *pd,
				struct ib_qp_init_attr *init_attr,
				struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev;
	struct mlx5_ib_qp *qp;
	u16 xrcdn = 0;
	int err;
	u32 rcqn;
	u32 scqn;

	init_attr->qpg_type = IB_QPG_NONE;

	if (pd) {
		dev = to_mdev(pd->device);
	} else {
		/* being cautious here */
		if (init_attr->qp_type != IB_QPT_XRC_TGT) {
			printf("mlx5_ib: WARN: ""%s: no PD for transport %s\n", __func__, ib_qp_type_str(init_attr->qp_type));
			return ERR_PTR(-EINVAL);
		}
		dev = to_mdev(to_mxrcd(init_attr->xrcd)->ibxrcd.device);
	}

	switch (init_attr->qp_type) {
	case IB_QPT_XRC_TGT:
	case IB_QPT_XRC_INI:
		if (!MLX5_CAP_GEN(dev->mdev, xrc)) {
			mlx5_ib_warn(dev, "XRC not supported\n");
			return ERR_PTR(-ENOSYS);
		}
		init_attr->recv_cq = NULL;
		if (init_attr->qp_type == IB_QPT_XRC_TGT) {
			xrcdn = to_mxrcd(init_attr->xrcd)->xrcdn;
			init_attr->send_cq = NULL;
		}

		/* fall through */
	case IB_QPT_RC:
	case IB_QPT_UC:
	case IB_QPT_UD:
	case IB_QPT_SMI:
	case IB_QPT_GSI:
	case IB_QPT_RAW_ETHERTYPE:
	case IB_QPT_RAW_PACKET:
		qp = kzalloc(sizeof(*qp), GFP_KERNEL);
		if (!qp)
			return ERR_PTR(-ENOMEM);

		err = create_qp_common(dev, pd, init_attr, udata, qp);
		if (err) {
			mlx5_ib_warn(dev, "create_qp_common failed\n");
			kfree(qp);
			return ERR_PTR(err);
		}

		if (is_qp0(init_attr->qp_type))
			qp->ibqp.qp_num = 0;
		else if (is_qp1(init_attr->qp_type))
			qp->ibqp.qp_num = 1;
		else
			qp->ibqp.qp_num = qp->mqp.qpn;

		rcqn = init_attr->recv_cq ? to_mcq(init_attr->recv_cq)->mcq.cqn : -1;
		scqn = init_attr->send_cq ? to_mcq(init_attr->send_cq)->mcq.cqn : -1;
		mlx5_ib_dbg(dev, "ib qpnum 0x%x, mlx qpn 0x%x, rcqn 0x%x, scqn 0x%x\n",
			    qp->ibqp.qp_num, qp->mqp.qpn, rcqn, scqn);

		qp->xrcdn = xrcdn;

		break;

	case IB_QPT_RAW_IPV6:
	case IB_QPT_MAX:
	default:
		mlx5_ib_warn(dev, "unsupported qp type %d\n",
			     init_attr->qp_type);
		/* Don't support raw QPs */
		return ERR_PTR(-EINVAL);
	}

	return &qp->ibqp;
}

int mlx5_ib_destroy_qp(struct ib_qp *qp)
{
	struct mlx5_ib_dev *dev = to_mdev(qp->device);
	struct mlx5_ib_qp *mqp = to_mqp(qp);

	destroy_qp_common(dev, mqp);

	kfree(mqp);

	return 0;
}

static u32 atomic_mode_qp(struct mlx5_ib_dev *dev)
{
	unsigned long mask;
	unsigned long tmp;

	mask = MLX5_CAP_ATOMIC(dev->mdev, atomic_size_qp) &
		MLX5_CAP_ATOMIC(dev->mdev, atomic_size_dc);

	tmp = find_last_bit(&mask, BITS_PER_LONG);
	if (tmp < 2 || tmp >= BITS_PER_LONG)
		return MLX5_ATOMIC_MODE_NONE;

	if (tmp == 2)
		return MLX5_ATOMIC_MODE_CX;

	return tmp << MLX5_ATOMIC_MODE_OFF;
}

static __be32 to_mlx5_access_flags(struct mlx5_ib_qp *qp, const struct ib_qp_attr *attr,
				   int attr_mask)
{
	struct mlx5_ib_dev *dev = to_mdev(qp->ibqp.device);
	u32 hw_access_flags = 0;
	u8 dest_rd_atomic;
	u32 access_flags;

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		dest_rd_atomic = attr->max_dest_rd_atomic;
	else
		dest_rd_atomic = qp->resp_depth;

	if (attr_mask & IB_QP_ACCESS_FLAGS)
		access_flags = attr->qp_access_flags;
	else
		access_flags = qp->atomic_rd_en;

	if (!dest_rd_atomic)
		access_flags &= IB_ACCESS_REMOTE_WRITE;

	if (access_flags & IB_ACCESS_REMOTE_READ)
		hw_access_flags |= MLX5_QP_BIT_RRE;
	if (access_flags & IB_ACCESS_REMOTE_ATOMIC)
		hw_access_flags |= (MLX5_QP_BIT_RAE |
				    atomic_mode_qp(dev));
	if (access_flags & IB_ACCESS_REMOTE_WRITE)
		hw_access_flags |= MLX5_QP_BIT_RWE;

	return cpu_to_be32(hw_access_flags);
}

enum {
	MLX5_PATH_FLAG_FL	= 1 << 0,
	MLX5_PATH_FLAG_FREE_AR	= 1 << 1,
	MLX5_PATH_FLAG_COUNTER	= 1 << 2,
};

static int ib_rate_to_mlx5(struct mlx5_ib_dev *dev, u8 rate)
{
	if (rate == IB_RATE_PORT_CURRENT) {
		return 0;
	} else if (rate < IB_RATE_2_5_GBPS || rate > IB_RATE_300_GBPS) {
		return -EINVAL;
	} else {
		while (rate != IB_RATE_2_5_GBPS &&
		       !(1 << (rate + MLX5_STAT_RATE_OFFSET) &
			 MLX5_CAP_GEN(dev->mdev, stat_rate_support)))
			--rate;
	}

	return rate + MLX5_STAT_RATE_OFFSET;
}

static int mlx5_set_path(struct mlx5_ib_dev *dev, const struct ib_ah_attr *ah,
			 struct mlx5_qp_path *path, u8 port, int attr_mask,
			 u32 path_flags, const struct ib_qp_attr *attr,
			 int alt)
{
	enum rdma_link_layer ll = dev->ib_dev.get_link_layer(&dev->ib_dev,
							     port);
	int err;
	int gid_type;

	if ((ll == IB_LINK_LAYER_ETHERNET) || (ah->ah_flags & IB_AH_GRH)) {
		int len = dev->mdev->port_caps[port - 1].gid_table_len;
		if (ah->grh.sgid_index >= len) {
			printf("mlx5_ib: ERR: ""sgid_index (%u) too large. max is %d\n", ah->grh.sgid_index, len - 1);
			return -EINVAL;
		}
	}

	if (ll == IB_LINK_LAYER_ETHERNET) {
		if (!(ah->ah_flags & IB_AH_GRH))
			return -EINVAL;

		err = mlx5_get_roce_gid_type(dev, port, ah->grh.sgid_index,
					     &gid_type);
		if (err)
			return err;
		err = mlx5_ib_resolve_grh(ah, path->rmac, NULL);
		if (err)
			return err;
		path->udp_sport = mlx5_get_roce_udp_sport(dev, port,
							  ah->grh.sgid_index,
							  0);
		path->dci_cfi_prio_sl = (ah->sl & 0xf) << 4;
	} else {
		path->fl_free_ar = (path_flags & MLX5_PATH_FLAG_FL) ? 0x80 : 0;
		path->grh_mlid	= ah->src_path_bits & 0x7f;
		path->rlid	= cpu_to_be16(ah->dlid);
		if (ah->ah_flags & IB_AH_GRH)
			path->grh_mlid	|= 1 << 7;
		if (attr_mask & IB_QP_PKEY_INDEX)
			path->pkey_index = cpu_to_be16(alt ?
						       attr->alt_pkey_index :
						       attr->pkey_index);

		path->dci_cfi_prio_sl = ah->sl & 0xf;
	}

	path->fl_free_ar |= (path_flags & MLX5_PATH_FLAG_FREE_AR) ? 0x40 : 0;

	if (ah->ah_flags & IB_AH_GRH) {
		path->mgid_index = ah->grh.sgid_index;
		path->hop_limit  = ah->grh.hop_limit;
		path->tclass_flowlabel =
			cpu_to_be32((ah->grh.traffic_class << 20) |
				    (ah->grh.flow_label));
		memcpy(path->rgid, ah->grh.dgid.raw, 16);
	}

	err = ib_rate_to_mlx5(dev, ah->static_rate);
	if (err < 0)
		return err;
	path->static_rate = err;
	path->port = port;

	if (attr_mask & IB_QP_TIMEOUT)
		path->ackto_lt = alt ? attr->alt_timeout << 3 : attr->timeout << 3;

	return 0;
}

static enum mlx5_qp_optpar opt_mask[MLX5_QP_NUM_STATE][MLX5_QP_NUM_STATE][MLX5_QP_ST_MAX] = {
	[MLX5_QP_STATE_INIT] = {
		[MLX5_QP_STATE_INIT] = {
			[MLX5_QP_ST_RC] = MLX5_QP_OPTPAR_RRE		|
					  MLX5_QP_OPTPAR_RAE		|
					  MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_PKEY_INDEX	|
					  MLX5_QP_OPTPAR_PRI_PORT,
			[MLX5_QP_ST_UC] = MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_PKEY_INDEX	|
					  MLX5_QP_OPTPAR_PRI_PORT,
			[MLX5_QP_ST_UD] = MLX5_QP_OPTPAR_PKEY_INDEX	|
					  MLX5_QP_OPTPAR_Q_KEY		|
					  MLX5_QP_OPTPAR_PRI_PORT,
			[MLX5_QP_ST_DCI] = MLX5_QP_OPTPAR_PRI_PORT	|
					  MLX5_QP_OPTPAR_DC_KEY		|
					  MLX5_QP_OPTPAR_PKEY_INDEX	|
					  MLX5_QP_OPTPAR_RAE,
		},
		[MLX5_QP_STATE_RTR] = {
			[MLX5_QP_ST_RC] = MLX5_QP_OPTPAR_ALT_ADDR_PATH  |
					  MLX5_QP_OPTPAR_RRE            |
					  MLX5_QP_OPTPAR_RAE            |
					  MLX5_QP_OPTPAR_RWE            |
					  MLX5_QP_OPTPAR_PKEY_INDEX,
			[MLX5_QP_ST_UC] = MLX5_QP_OPTPAR_ALT_ADDR_PATH  |
					  MLX5_QP_OPTPAR_RWE            |
					  MLX5_QP_OPTPAR_PKEY_INDEX,
			[MLX5_QP_ST_UD] = MLX5_QP_OPTPAR_PKEY_INDEX     |
					  MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_MLX] = MLX5_QP_OPTPAR_PKEY_INDEX	|
					   MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_XRC] = MLX5_QP_OPTPAR_ALT_ADDR_PATH |
					  MLX5_QP_OPTPAR_RRE            |
					  MLX5_QP_OPTPAR_RAE            |
					  MLX5_QP_OPTPAR_RWE            |
					  MLX5_QP_OPTPAR_PKEY_INDEX,
			[MLX5_QP_ST_DCI] = MLX5_QP_OPTPAR_PKEY_INDEX	|
					  MLX5_QP_OPTPAR_RAE		|
					  MLX5_QP_OPTPAR_DC_KEY,
		},
	},
	[MLX5_QP_STATE_RTR] = {
		[MLX5_QP_STATE_RTS] = {
			[MLX5_QP_ST_RC] = MLX5_QP_OPTPAR_ALT_ADDR_PATH	|
					  MLX5_QP_OPTPAR_RRE		|
					  MLX5_QP_OPTPAR_RAE		|
					  MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_PM_STATE	|
					  MLX5_QP_OPTPAR_RNR_TIMEOUT,
			[MLX5_QP_ST_UC] = MLX5_QP_OPTPAR_ALT_ADDR_PATH	|
					  MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_PM_STATE,
			[MLX5_QP_ST_UD] = MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_DCI] = MLX5_QP_OPTPAR_DC_KEY		|
					  MLX5_QP_OPTPAR_PM_STATE	|
					  MLX5_QP_OPTPAR_RAE,
		},
	},
	[MLX5_QP_STATE_RTS] = {
		[MLX5_QP_STATE_RTS] = {
			[MLX5_QP_ST_RC] = MLX5_QP_OPTPAR_RRE		|
					  MLX5_QP_OPTPAR_RAE		|
					  MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_RNR_TIMEOUT	|
					  MLX5_QP_OPTPAR_PM_STATE	|
					  MLX5_QP_OPTPAR_ALT_ADDR_PATH,
			[MLX5_QP_ST_UC] = MLX5_QP_OPTPAR_RWE		|
					  MLX5_QP_OPTPAR_PM_STATE	|
					  MLX5_QP_OPTPAR_ALT_ADDR_PATH,
			[MLX5_QP_ST_UD] = MLX5_QP_OPTPAR_Q_KEY		|
					  MLX5_QP_OPTPAR_SRQN		|
					  MLX5_QP_OPTPAR_CQN_RCV,
			[MLX5_QP_ST_DCI] = MLX5_QP_OPTPAR_DC_KEY		|
					  MLX5_QP_OPTPAR_PM_STATE	|
					  MLX5_QP_OPTPAR_RAE,
		},
	},
	[MLX5_QP_STATE_SQER] = {
		[MLX5_QP_STATE_RTS] = {
			[MLX5_QP_ST_UD]	 = MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_MLX] = MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_UC]	 = MLX5_QP_OPTPAR_RWE,
			[MLX5_QP_ST_RC]	 = MLX5_QP_OPTPAR_RNR_TIMEOUT	|
					   MLX5_QP_OPTPAR_RWE		|
					   MLX5_QP_OPTPAR_RAE		|
					   MLX5_QP_OPTPAR_RRE,
			[MLX5_QP_ST_DCI]  = MLX5_QP_OPTPAR_DC_KEY	|
					   MLX5_QP_OPTPAR_RAE,

		},
	},
	[MLX5_QP_STATE_SQD] = {
		[MLX5_QP_STATE_RTS] = {
			[MLX5_QP_ST_UD]	 = MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_MLX] = MLX5_QP_OPTPAR_Q_KEY,
			[MLX5_QP_ST_UC]	 = MLX5_QP_OPTPAR_RWE,
			[MLX5_QP_ST_RC]	 = MLX5_QP_OPTPAR_RNR_TIMEOUT	|
					   MLX5_QP_OPTPAR_RWE		|
					   MLX5_QP_OPTPAR_RAE		|
					   MLX5_QP_OPTPAR_RRE,
		},
	},
};

static int ib_nr_to_mlx5_nr(int ib_mask)
{
	switch (ib_mask) {
	case IB_QP_STATE:
		return 0;
	case IB_QP_CUR_STATE:
		return 0;
	case IB_QP_EN_SQD_ASYNC_NOTIFY:
		return 0;
	case IB_QP_ACCESS_FLAGS:
		return MLX5_QP_OPTPAR_RWE | MLX5_QP_OPTPAR_RRE |
			MLX5_QP_OPTPAR_RAE;
	case IB_QP_PKEY_INDEX:
		return MLX5_QP_OPTPAR_PKEY_INDEX;
	case IB_QP_PORT:
		return MLX5_QP_OPTPAR_PRI_PORT;
	case IB_QP_QKEY:
		return MLX5_QP_OPTPAR_Q_KEY;
	case IB_QP_AV:
		return MLX5_QP_OPTPAR_PRIMARY_ADDR_PATH |
			MLX5_QP_OPTPAR_PRI_PORT;
	case IB_QP_PATH_MTU:
		return 0;
	case IB_QP_TIMEOUT:
		return MLX5_QP_OPTPAR_ACK_TIMEOUT;
	case IB_QP_RETRY_CNT:
		return MLX5_QP_OPTPAR_RETRY_COUNT;
	case IB_QP_RNR_RETRY:
		return MLX5_QP_OPTPAR_RNR_RETRY;
	case IB_QP_RQ_PSN:
		return 0;
	case IB_QP_MAX_QP_RD_ATOMIC:
		return MLX5_QP_OPTPAR_SRA_MAX;
	case IB_QP_ALT_PATH:
		return MLX5_QP_OPTPAR_ALT_ADDR_PATH;
	case IB_QP_MIN_RNR_TIMER:
		return MLX5_QP_OPTPAR_RNR_TIMEOUT;
	case IB_QP_SQ_PSN:
		return 0;
	case IB_QP_MAX_DEST_RD_ATOMIC:
		return MLX5_QP_OPTPAR_RRA_MAX | MLX5_QP_OPTPAR_RWE |
			MLX5_QP_OPTPAR_RRE | MLX5_QP_OPTPAR_RAE;
	case IB_QP_PATH_MIG_STATE:
		return MLX5_QP_OPTPAR_PM_STATE;
	case IB_QP_CAP:
		return 0;
	case IB_QP_DEST_QPN:
		return 0;
	}
	return 0;
}

static int ib_mask_to_mlx5_opt(int ib_mask)
{
	int result = 0;
	int i;

	for (i = 0; i < 8 * sizeof(int); i++) {
		if ((1 << i) & ib_mask)
			result |= ib_nr_to_mlx5_nr(1 << i);
	}

	return result;
}

static int __mlx5_ib_modify_qp(struct ib_qp *ibqp,
			       const struct ib_qp_attr *attr, int attr_mask,
			       enum ib_qp_state cur_state, enum ib_qp_state new_state)
{
	static const u16 optab[MLX5_QP_NUM_STATE][MLX5_QP_NUM_STATE] = {
		[MLX5_QP_STATE_RST] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_INIT]	= MLX5_CMD_OP_RST2INIT_QP,
		},
		[MLX5_QP_STATE_INIT]  = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_INIT]	= MLX5_CMD_OP_INIT2INIT_QP,
			[MLX5_QP_STATE_RTR]	= MLX5_CMD_OP_INIT2RTR_QP,
		},
		[MLX5_QP_STATE_RTR]   = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_RTR2RTS_QP,
		},
		[MLX5_QP_STATE_RTS]   = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_RTS2RTS_QP,
		},
		[MLX5_QP_STATE_SQD] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_SQD_RTS_QP,
		},
		[MLX5_QP_STATE_SQER] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
			[MLX5_QP_STATE_RTS]	= MLX5_CMD_OP_SQERR2RTS_QP,
		},
		[MLX5_QP_STATE_ERR] = {
			[MLX5_QP_STATE_RST]	= MLX5_CMD_OP_2RST_QP,
			[MLX5_QP_STATE_ERR]	= MLX5_CMD_OP_2ERR_QP,
		}
	};

	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx5_ib_qp *qp = to_mqp(ibqp);
	struct mlx5_ib_cq *send_cq, *recv_cq;
	struct mlx5_qp_context *context;
	struct mlx5_modify_qp_mbox_in *in;
	struct mlx5_ib_pd *pd;
	enum mlx5_qp_state mlx5_cur, mlx5_new;
	enum mlx5_qp_optpar optpar;
	int sqd_event;
	int mlx5_st;
	int err;
	u16 op;

	in = kzalloc(sizeof(*in), GFP_KERNEL);
	if (!in)
		return -ENOMEM;

	context = &in->ctx;
	err = to_mlx5_st(ibqp->qp_type);
	if (err < 0)
		goto out;

	context->flags = cpu_to_be32(err << 16);

	if (!(attr_mask & IB_QP_PATH_MIG_STATE)) {
		context->flags |= cpu_to_be32(MLX5_QP_PM_MIGRATED << 11);
	} else {
		switch (attr->path_mig_state) {
		case IB_MIG_MIGRATED:
			context->flags |= cpu_to_be32(MLX5_QP_PM_MIGRATED << 11);
			break;
		case IB_MIG_REARM:
			context->flags |= cpu_to_be32(MLX5_QP_PM_REARM << 11);
			break;
		case IB_MIG_ARMED:
			context->flags |= cpu_to_be32(MLX5_QP_PM_ARMED << 11);
			break;
		}
	}

	if (ibqp->qp_type == IB_QPT_GSI || ibqp->qp_type == IB_QPT_SMI) {
		context->mtu_msgmax = (IB_MTU_256 << 5) | 8;
	} else if (ibqp->qp_type == IB_QPT_UD) {
		context->mtu_msgmax = (IB_MTU_4096 << 5) | 12;
	} else if (attr_mask & IB_QP_PATH_MTU) {
		if (attr->path_mtu < IB_MTU_256 ||
		    attr->path_mtu > IB_MTU_4096) {
			mlx5_ib_warn(dev, "invalid mtu %d\n", attr->path_mtu);
			err = -EINVAL;
			goto out;
		}
		context->mtu_msgmax = (attr->path_mtu << 5) |
				      (u8)MLX5_CAP_GEN(dev->mdev, log_max_msg);
	}

	if (attr_mask & IB_QP_DEST_QPN)
		context->log_pg_sz_remote_qpn = cpu_to_be32(attr->dest_qp_num);

	if (attr_mask & IB_QP_PKEY_INDEX)
		context->pri_path.pkey_index = cpu_to_be16(attr->pkey_index);

	/* todo implement counter_index functionality */

	if (is_sqp(ibqp->qp_type))
		context->pri_path.port = qp->port;

	if (attr_mask & IB_QP_PORT)
		context->pri_path.port = attr->port_num;

	if (attr_mask & IB_QP_AV) {
		err = mlx5_set_path(dev, &attr->ah_attr, &context->pri_path,
				    attr_mask & IB_QP_PORT ? attr->port_num : qp->port,
				    attr_mask, 0, attr, 0);
		if (err)
			goto out;
	}

	if (attr_mask & IB_QP_TIMEOUT)
		context->pri_path.ackto_lt |= attr->timeout << 3;

	if (attr_mask & IB_QP_ALT_PATH) {
		err = mlx5_set_path(dev, &attr->alt_ah_attr, &context->alt_path,
				    attr->alt_port_num,
				    attr_mask  | IB_QP_PKEY_INDEX | IB_QP_TIMEOUT,
				    0, attr, 1);
		if (err)
			goto out;
	}

	pd = get_pd(qp);
	get_cqs(qp->ibqp.qp_type, qp->ibqp.send_cq, qp->ibqp.recv_cq,
		&send_cq, &recv_cq);

	context->flags_pd = cpu_to_be32(pd ? pd->pdn : to_mpd(dev->devr.p0)->pdn);
	context->cqn_send = send_cq ? cpu_to_be32(send_cq->mcq.cqn) : 0;
	context->cqn_recv = recv_cq ? cpu_to_be32(recv_cq->mcq.cqn) : 0;
	context->params1  = cpu_to_be32(MLX5_IB_ACK_REQ_FREQ << 28);

	if (attr_mask & IB_QP_RNR_RETRY)
		context->params1 |= cpu_to_be32(attr->rnr_retry << 13);

	if (attr_mask & IB_QP_RETRY_CNT)
		context->params1 |= cpu_to_be32(attr->retry_cnt << 16);

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC) {
		if (attr->max_rd_atomic)
			context->params1 |=
				cpu_to_be32(fls(attr->max_rd_atomic - 1) << 21);
	}

	if (attr_mask & IB_QP_SQ_PSN)
		context->next_send_psn = cpu_to_be32(attr->sq_psn & 0xffffff);

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC) {
		if (attr->max_dest_rd_atomic)
			context->params2 |=
				cpu_to_be32(fls(attr->max_dest_rd_atomic - 1) << 21);
	}

	if ((attr_mask & IB_QP_ACCESS_FLAGS) &&
	    (attr->qp_access_flags & IB_ACCESS_REMOTE_ATOMIC) &&
	    !dev->enable_atomic_resp) {
		mlx5_ib_warn(dev, "atomic responder is not supported\n");
		err = -EINVAL;
		goto out;
	}

	if (attr_mask & (IB_QP_ACCESS_FLAGS | IB_QP_MAX_DEST_RD_ATOMIC))
		context->params2 |= to_mlx5_access_flags(qp, attr, attr_mask);

	if (attr_mask & IB_QP_MIN_RNR_TIMER)
		context->rnr_nextrecvpsn |= cpu_to_be32(attr->min_rnr_timer << 24);

	if (attr_mask & IB_QP_RQ_PSN)
		context->rnr_nextrecvpsn |= cpu_to_be32(attr->rq_psn & 0xffffff);

	if (attr_mask & IB_QP_QKEY)
		context->qkey = cpu_to_be32(attr->qkey);

	if (qp->rq.wqe_cnt && cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT)
		context->db_rec_addr = cpu_to_be64(qp->db.dma);

	if (cur_state == IB_QPS_RTS && new_state == IB_QPS_SQD	&&
	    attr_mask & IB_QP_EN_SQD_ASYNC_NOTIFY && attr->en_sqd_async_notify)
		sqd_event = 1;
	else
		sqd_event = 0;

	if (!ibqp->uobject && cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT)
		context->sq_crq_size |= cpu_to_be16(1 << 4);

	if (cur_state == IB_QPS_RESET && new_state == IB_QPS_INIT) {
		u8 port_num = (attr_mask & IB_QP_PORT ? attr->port_num :
			       qp->port) - 1;
		struct mlx5_ib_port *mibport = &dev->port[port_num];

		context->qp_counter_set_usr_page |=
			cpu_to_be32(mibport->q_cnt_id << 24);
	}

	mlx5_cur = to_mlx5_state(cur_state);
	mlx5_new = to_mlx5_state(new_state);
	mlx5_st = to_mlx5_st(ibqp->qp_type);
	if (mlx5_st < 0)
		goto out;

	if (mlx5_cur >= MLX5_QP_NUM_STATE || mlx5_new >= MLX5_QP_NUM_STATE ||
	    !optab[mlx5_cur][mlx5_new])
		return -EINVAL;

	op = optab[mlx5_cur][mlx5_new];
	optpar = ib_mask_to_mlx5_opt(attr_mask);
	optpar &= opt_mask[mlx5_cur][mlx5_new][mlx5_st];
	in->optparam = cpu_to_be32(optpar);

	if (qp->ibqp.qp_type == IB_QPT_RAW_PACKET)
		err = -EOPNOTSUPP;
	else
		err = mlx5_core_qp_modify(dev->mdev, op, in, sqd_event,
				  &qp->mqp);
	if (err)
		goto out;

	qp->state = new_state;

	if (attr_mask & IB_QP_ACCESS_FLAGS)
		qp->atomic_rd_en = attr->qp_access_flags;
	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC)
		qp->resp_depth = attr->max_dest_rd_atomic;
	if (attr_mask & IB_QP_PORT)
		qp->port = attr->port_num;
	if (attr_mask & IB_QP_ALT_PATH)
		qp->alt_port = attr->alt_port_num;

	/*
	 * If we moved a kernel QP to RESET, clean up all old CQ
	 * entries and reinitialize the QP.
	 */
	if (new_state == IB_QPS_RESET && !ibqp->uobject) {
		mlx5_ib_cq_clean(recv_cq, qp->mqp.qpn,
				 ibqp->srq ? to_msrq(ibqp->srq) : NULL);
		if (send_cq != recv_cq)
			mlx5_ib_cq_clean(send_cq, qp->mqp.qpn, NULL);

		qp->rq.head = 0;
		qp->rq.tail = 0;
		qp->sq.head = 0;
		qp->sq.tail = 0;
		qp->sq.cur_post = 0;
		qp->sq.last_poll = 0;
		if (qp->db.db) {
			qp->db.db[MLX5_RCV_DBR] = 0;
			qp->db.db[MLX5_SND_DBR] = 0;
		}
	}

out:
	kfree(in);
	return err;
}

static int ignored_ts_check(enum ib_qp_type qp_type)
{
	return 0;
}

int mlx5_ib_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		      int attr_mask, struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx5_ib_qp *qp = to_mqp(ibqp);
	enum ib_qp_state cur_state, new_state;
	int err = -EINVAL;
	int port;
 
	mutex_lock(&qp->mutex);

	cur_state = attr_mask & IB_QP_CUR_STATE ? attr->cur_qp_state : qp->state;
	new_state = attr_mask & IB_QP_STATE ? attr->qp_state : cur_state;
 
	if (!ignored_ts_check(ibqp->qp_type) &&
	    !ib_modify_qp_is_ok(cur_state, new_state, ibqp->qp_type, attr_mask))
		goto out;

	if ((attr_mask & IB_QP_PORT) &&
	    (attr->port_num == 0 ||
	     attr->port_num > MLX5_CAP_GEN(dev->mdev, num_ports)))
		goto out;

	if (attr_mask & IB_QP_PKEY_INDEX) {
		port = attr_mask & IB_QP_PORT ? attr->port_num : qp->port;
		if (attr->pkey_index >=
		    dev->mdev->port_caps[port - 1].pkey_table_len)
			goto out;
	}

	if (attr_mask & IB_QP_MAX_QP_RD_ATOMIC &&
	    attr->max_rd_atomic >
	    (1 << MLX5_CAP_GEN(dev->mdev, log_max_ra_res_qp)))
		goto out;

	if (attr_mask & IB_QP_MAX_DEST_RD_ATOMIC &&
	    attr->max_dest_rd_atomic >
	    (1 << MLX5_CAP_GEN(dev->mdev, log_max_ra_req_qp)))
		goto out;

	if (cur_state == new_state && cur_state == IB_QPS_RESET) {
		err = 0;
		goto out;
	}

	err = __mlx5_ib_modify_qp(ibqp, attr, attr_mask, cur_state, new_state);

out:
	mutex_unlock(&qp->mutex);
	return err;
}

static int mlx5_wq_overflow(struct mlx5_ib_wq *wq, int nreq, struct ib_cq *ib_cq)
{
	struct mlx5_ib_cq *cq;
	unsigned cur;

	cur = wq->head - wq->tail;
	if (likely(cur + nreq < wq->max_post))
		return 0;

	cq = to_mcq(ib_cq);
	spin_lock(&cq->lock);
	cur = wq->head - wq->tail;
	spin_unlock(&cq->lock);

	return cur + nreq >= wq->max_post;
}

static __always_inline void set_raddr_seg(struct mlx5_wqe_raddr_seg *rseg,
					  u64 remote_addr, u32 rkey)
{
	rseg->raddr    = cpu_to_be64(remote_addr);
	rseg->rkey     = cpu_to_be32(rkey);
	rseg->reserved = 0;
}

static void set_datagram_seg(struct mlx5_wqe_datagram_seg *dseg,
			     struct ib_send_wr *wr)
{
	memcpy(&dseg->av, &to_mah(wr->wr.ud.ah)->av, sizeof(struct mlx5_av));
	dseg->av.dqp_dct = cpu_to_be32(wr->wr.ud.remote_qpn | MLX5_EXTENDED_UD_AV);
	dseg->av.key.qkey.qkey = cpu_to_be32(wr->wr.ud.remote_qkey);
}

static void set_data_ptr_seg(struct mlx5_wqe_data_seg *dseg, struct ib_sge *sg)
{
	dseg->byte_count = cpu_to_be32(sg->length);
	dseg->lkey       = cpu_to_be32(sg->lkey);
	dseg->addr       = cpu_to_be64(sg->addr);
}

static __be16 get_klm_octo(int npages)
{
	return cpu_to_be16(ALIGN(npages, 8) / 2);
}

static __be64 frwr_mkey_mask(void)
{
	u64 result;

	result = MLX5_MKEY_MASK_LEN		|
		MLX5_MKEY_MASK_PAGE_SIZE	|
		MLX5_MKEY_MASK_START_ADDR	|
		MLX5_MKEY_MASK_EN_RINVAL	|
		MLX5_MKEY_MASK_KEY		|
		MLX5_MKEY_MASK_LR		|
		MLX5_MKEY_MASK_LW		|
		MLX5_MKEY_MASK_RR		|
		MLX5_MKEY_MASK_RW		|
		MLX5_MKEY_MASK_A		|
		MLX5_MKEY_MASK_SMALL_FENCE	|
		MLX5_MKEY_MASK_FREE;

	return cpu_to_be64(result);
}

static void set_frwr_umr_segment(struct mlx5_wqe_umr_ctrl_seg *umr,
				 struct ib_send_wr *wr, int li)
{
	memset(umr, 0, sizeof(*umr));

	if (li) {
		umr->mkey_mask = cpu_to_be64(MLX5_MKEY_MASK_FREE);
		umr->flags = 1 << 7;
		return;
	}

	umr->flags = (1 << 5); /* fail if not free */
	umr->klm_octowords = get_klm_octo(wr->wr.fast_reg.page_list_len);
	umr->mkey_mask = frwr_mkey_mask();
}

static u8 get_umr_flags(int acc)
{
	return (acc & IB_ACCESS_REMOTE_ATOMIC ? MLX5_PERM_ATOMIC       : 0) |
	       (acc & IB_ACCESS_REMOTE_WRITE  ? MLX5_PERM_REMOTE_WRITE : 0) |
	       (acc & IB_ACCESS_REMOTE_READ   ? MLX5_PERM_REMOTE_READ  : 0) |
	       (acc & IB_ACCESS_LOCAL_WRITE   ? MLX5_PERM_LOCAL_WRITE  : 0) |
		MLX5_PERM_LOCAL_READ | MLX5_PERM_UMR_EN;
}

static void set_mkey_segment(struct mlx5_mkey_seg *seg, struct ib_send_wr *wr,
			     int li, int *writ)
{
	memset(seg, 0, sizeof(*seg));
	if (li) {
		seg->status = MLX5_MKEY_STATUS_FREE;
		return;
	}

	seg->flags = get_umr_flags(wr->wr.fast_reg.access_flags) |
		     MLX5_ACCESS_MODE_MTT;
	*writ = seg->flags & (MLX5_PERM_LOCAL_WRITE | IB_ACCESS_REMOTE_WRITE);
	seg->qpn_mkey7_0 = cpu_to_be32((wr->wr.fast_reg.rkey & 0xff) | 0xffffff00);
	seg->flags_pd = cpu_to_be32(MLX5_MKEY_REMOTE_INVAL);
	seg->start_addr = cpu_to_be64(wr->wr.fast_reg.iova_start);
	seg->len = cpu_to_be64(wr->wr.fast_reg.length);
	seg->xlt_oct_size = cpu_to_be32((wr->wr.fast_reg.page_list_len + 1) / 2);
	seg->log2_page_size = wr->wr.fast_reg.page_shift;
}

static void set_frwr_pages(struct mlx5_wqe_data_seg *dseg,
			   struct ib_send_wr *wr,
			   struct mlx5_core_dev *mdev,
			   struct mlx5_ib_pd *pd,
			   int writ)
{
	struct mlx5_ib_fast_reg_page_list *mfrpl = to_mfrpl(wr->wr.fast_reg.page_list);
	u64 *page_list = wr->wr.fast_reg.page_list->page_list;
	u64 perm = MLX5_EN_RD | (writ ? MLX5_EN_WR : 0);
	int i;

	for (i = 0; i < wr->wr.fast_reg.page_list_len; i++)
		mfrpl->mapped_page_list[i] = cpu_to_be64(page_list[i] | perm);
	dseg->addr = cpu_to_be64(mfrpl->map);
	dseg->byte_count = cpu_to_be32(ALIGN(sizeof(u64) * wr->wr.fast_reg.page_list_len, 64));
	dseg->lkey = cpu_to_be32(pd->pa_lkey);
}

static __be32 send_ieth(struct ib_send_wr *wr)
{
	switch (wr->opcode) {
	case IB_WR_SEND_WITH_IMM:
	case IB_WR_RDMA_WRITE_WITH_IMM:
		return wr->ex.imm_data;

	case IB_WR_SEND_WITH_INV:
		return cpu_to_be32(wr->ex.invalidate_rkey);

	default:
		return 0;
	}
}

static u8 calc_sig(void *wqe, int size)
{
	u8 *p = wqe;
	u8 res = 0;
	int i;

	for (i = 0; i < size; i++)
		res ^= p[i];

	return ~res;
}

static u8 calc_wq_sig(void *wqe)
{
	return calc_sig(wqe, (*((u8 *)wqe + 8) & 0x3f) << 4);
}

static int set_data_inl_seg(struct mlx5_ib_qp *qp, struct ib_send_wr *wr,
			    void *wqe, int *sz)
{
	struct mlx5_wqe_inline_seg *seg;
	void *qend = qp->sq.qend;
	void *addr;
	int inl = 0;
	int copy;
	int len;
	int i;

	seg = wqe;
	wqe += sizeof(*seg);
	for (i = 0; i < wr->num_sge; i++) {
		addr = (void *)(uintptr_t)(wr->sg_list[i].addr);
		len  = wr->sg_list[i].length;
		inl += len;

		if (unlikely(inl > qp->max_inline_data))
			return -ENOMEM;

		if (unlikely(wqe + len > qend)) {
			copy = (int)(qend - wqe);
			memcpy(wqe, addr, copy);
			addr += copy;
			len -= copy;
			wqe = mlx5_get_send_wqe(qp, 0);
		}
		memcpy(wqe, addr, len);
		wqe += len;
	}

	seg->byte_count = cpu_to_be32(inl | MLX5_INLINE_SEG);

	*sz = ALIGN(inl + sizeof(seg->byte_count), 16) / 16;

	return 0;
}

static int set_frwr_li_wr(void **seg, struct ib_send_wr *wr, int *size,
			  struct mlx5_core_dev *mdev, struct mlx5_ib_pd *pd, struct mlx5_ib_qp *qp)
{
	int writ = 0;
	int li;

	li = wr->opcode == IB_WR_LOCAL_INV ? 1 : 0;
	if (unlikely(wr->send_flags & IB_SEND_INLINE))
		return -EINVAL;

	set_frwr_umr_segment(*seg, wr, li);
	*seg += sizeof(struct mlx5_wqe_umr_ctrl_seg);
	*size += sizeof(struct mlx5_wqe_umr_ctrl_seg) / 16;
	if (unlikely((*seg == qp->sq.qend)))
		*seg = mlx5_get_send_wqe(qp, 0);
	set_mkey_segment(*seg, wr, li, &writ);
	*seg += sizeof(struct mlx5_mkey_seg);
	*size += sizeof(struct mlx5_mkey_seg) / 16;
	if (unlikely((*seg == qp->sq.qend)))
		*seg = mlx5_get_send_wqe(qp, 0);
	if (!li) {
		if (unlikely(wr->wr.fast_reg.page_list_len >
			     wr->wr.fast_reg.page_list->max_page_list_len))
			return	-ENOMEM;

		set_frwr_pages(*seg, wr, mdev, pd, writ);
		*seg += sizeof(struct mlx5_wqe_data_seg);
		*size += (sizeof(struct mlx5_wqe_data_seg) / 16);
	}
	return 0;
}

static void dump_wqe(struct mlx5_ib_qp *qp, int idx, int size_16)
{
	__be32 *p = NULL;
	int tidx = idx;
	int i, j;

	pr_debug("dump wqe at %p\n", mlx5_get_send_wqe(qp, tidx));
	for (i = 0, j = 0; i < size_16 * 4; i += 4, j += 4) {
		if ((i & 0xf) == 0) {
			void *buf = mlx5_get_send_wqe(qp, tidx);
			tidx = (tidx + 1) & (qp->sq.wqe_cnt - 1);
			p = buf;
			j = 0;
		}
		pr_debug("%08x %08x %08x %08x\n", be32_to_cpu(p[j]),
			 be32_to_cpu(p[j + 1]), be32_to_cpu(p[j + 2]),
			 be32_to_cpu(p[j + 3]));
	}
}

static void mlx5_bf_copy(u64 __iomem *dst, u64 *src,
			 unsigned bytecnt, struct mlx5_ib_qp *qp)
{
	while (bytecnt > 0) {
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		__iowrite64_copy(dst++, src++, 8);
		bytecnt -= 64;
		if (unlikely(src == qp->sq.qend))
			src = mlx5_get_send_wqe(qp, 0);
	}
}

static u8 get_fence(u8 fence, struct ib_send_wr *wr)
{
	if (unlikely(wr->opcode == IB_WR_LOCAL_INV &&
		     wr->send_flags & IB_SEND_FENCE))
		return MLX5_FENCE_MODE_STRONG_ORDERING;

	if (unlikely(fence)) {
		if (wr->send_flags & IB_SEND_FENCE)
			return MLX5_FENCE_MODE_SMALL_AND_FENCE;
		else
			return fence;

	} else {
		return 0;
	}
}

static int begin_wqe(struct mlx5_ib_qp *qp, void **seg,
		     struct mlx5_wqe_ctrl_seg **ctrl,
		     struct ib_send_wr *wr, unsigned *idx,
		     int *size, int nreq)
{
	int err = 0;

	if (unlikely(mlx5_wq_overflow(&qp->sq, nreq, qp->ibqp.send_cq))) {
		mlx5_ib_warn(to_mdev(qp->ibqp.device), "work queue overflow\n");
		err = -ENOMEM;
		return err;
	}

	*idx = qp->sq.cur_post & (qp->sq.wqe_cnt - 1);
	*seg = mlx5_get_send_wqe(qp, *idx);
	*ctrl = *seg;
	*(u32 *)(*seg + 8) = 0;
	(*ctrl)->imm = send_ieth(wr);
	(*ctrl)->fm_ce_se = qp->sq_signal_bits |
		(wr->send_flags & IB_SEND_SIGNALED ?
		 MLX5_WQE_CTRL_CQ_UPDATE : 0) |
		(wr->send_flags & IB_SEND_SOLICITED ?
		 MLX5_WQE_CTRL_SOLICITED : 0);

	*seg += sizeof(**ctrl);
	*size = sizeof(**ctrl) / 16;

	return err;
}

static void finish_wqe(struct mlx5_ib_qp *qp,
		       struct mlx5_wqe_ctrl_seg *ctrl,
		       u8 size, unsigned idx,
		       struct ib_send_wr *wr,
		       int nreq, u8 fence, u8 next_fence,
		       u32 mlx5_opcode)
{
	u8 opmod = 0;

	ctrl->opmod_idx_opcode = cpu_to_be32(((u32)(qp->sq.cur_post) << 8) |
					     mlx5_opcode | ((u32)opmod << 24));
	ctrl->qpn_ds = cpu_to_be32(size | (qp->mqp.qpn << 8));
	ctrl->fm_ce_se |= fence;
	qp->fm_cache = next_fence;
	if (unlikely(qp->wq_sig))
		ctrl->signature = calc_wq_sig(ctrl);

	qp->sq.swr_ctx[idx].wrid = wr->wr_id;
	qp->sq.swr_ctx[idx].w_list.opcode = mlx5_opcode;
	qp->sq.swr_ctx[idx].wqe_head = qp->sq.head + nreq;
	qp->sq.cur_post += DIV_ROUND_UP(size * 16, MLX5_SEND_WQE_BB);
	qp->sq.swr_ctx[idx].w_list.next = qp->sq.cur_post;
	qp->sq.swr_ctx[idx].sig_piped = 0;
}

int mlx5_ib_post_send(struct ib_qp *ibqp, struct ib_send_wr *wr,
		      struct ib_send_wr **bad_wr)
{
	struct mlx5_wqe_ctrl_seg *ctrl = NULL;  /* compiler warning */
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx5_core_dev *mdev = dev->mdev;
	struct mlx5_ib_qp *qp = to_mqp(ibqp);
	struct mlx5_wqe_data_seg *dpseg;
	struct mlx5_wqe_xrc_seg *xrc;
	struct mlx5_bf *bf = qp->bf;
	int uninitialized_var(size);
	void *qend = qp->sq.qend;
	unsigned long flags;
	unsigned idx;
	int err = 0;
	int inl = 0;
	int num_sge;
	void *seg;
	int nreq;
	int i;
	u8 next_fence = 0;
	u8 fence;


	spin_lock_irqsave(&qp->sq.lock, flags);

	if (mdev->state == MLX5_DEVICE_STATE_INTERNAL_ERROR) {
		err = -EIO;
		*bad_wr = wr;
		nreq = 0;
		goto out;
	}

	for (nreq = 0; wr; nreq++, wr = wr->next) {
		if (unlikely(wr->opcode >= ARRAY_SIZE(mlx5_ib_opcode))) {
			mlx5_ib_warn(dev, "Invalid opcode 0x%x\n", wr->opcode);
			err = -EINVAL;
			*bad_wr = wr;
			goto out;
		}

		fence = qp->fm_cache;
		num_sge = wr->num_sge;
		if (unlikely(num_sge > qp->sq.max_gs)) {
			mlx5_ib_warn(dev, "Max gs exceeded %d (max = %d)\n", wr->num_sge, qp->sq.max_gs);
			err = -ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		err = begin_wqe(qp, &seg, &ctrl, wr, &idx, &size, nreq);
		if (err) {
			mlx5_ib_warn(dev, "Failed to prepare WQE\n");
			err = -ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		switch (ibqp->qp_type) {
		case IB_QPT_XRC_INI:
			xrc = seg;
			xrc->xrc_srqn = htonl(wr->xrc_remote_srq_num);
			seg += sizeof(*xrc);
			size += sizeof(*xrc) / 16;
			/* fall through */
		case IB_QPT_RC:
			switch (wr->opcode) {
			case IB_WR_RDMA_READ:
			case IB_WR_RDMA_WRITE:
			case IB_WR_RDMA_WRITE_WITH_IMM:
				set_raddr_seg(seg, wr->wr.rdma.remote_addr,
					      wr->wr.rdma.rkey);
				seg += sizeof(struct mlx5_wqe_raddr_seg);
				size += sizeof(struct mlx5_wqe_raddr_seg) / 16;
				break;

			case IB_WR_ATOMIC_CMP_AND_SWP:
			case IB_WR_ATOMIC_FETCH_AND_ADD:
			case IB_WR_MASKED_ATOMIC_CMP_AND_SWP:
				mlx5_ib_warn(dev, "Atomic operations are not supported yet\n");
				err = -ENOSYS;
				*bad_wr = wr;
				goto out;

			case IB_WR_LOCAL_INV:
				next_fence = MLX5_FENCE_MODE_INITIATOR_SMALL;
				qp->sq.swr_ctx[idx].wr_data = IB_WR_LOCAL_INV;
				ctrl->imm = cpu_to_be32(wr->ex.invalidate_rkey);
				err = set_frwr_li_wr(&seg, wr, &size, mdev, to_mpd(ibqp->pd), qp);
				if (err) {
					mlx5_ib_warn(dev, "Failed to prepare LOCAL_INV WQE\n");
					*bad_wr = wr;
					goto out;
				}
				num_sge = 0;
				break;

			case IB_WR_FAST_REG_MR:
				next_fence = MLX5_FENCE_MODE_INITIATOR_SMALL;
				qp->sq.swr_ctx[idx].wr_data = IB_WR_FAST_REG_MR;
				ctrl->imm = cpu_to_be32(wr->wr.fast_reg.rkey);
				err = set_frwr_li_wr(&seg, wr, &size, mdev, to_mpd(ibqp->pd), qp);
				if (err) {
					mlx5_ib_warn(dev, "Failed to prepare FAST_REG_MR WQE\n");
					*bad_wr = wr;
					goto out;
				}
				num_sge = 0;
				break;

			default:
				break;
			}
			break;

		case IB_QPT_UC:
			switch (wr->opcode) {
			case IB_WR_RDMA_WRITE:
			case IB_WR_RDMA_WRITE_WITH_IMM:
				set_raddr_seg(seg, wr->wr.rdma.remote_addr,
					      wr->wr.rdma.rkey);
				seg  += sizeof(struct mlx5_wqe_raddr_seg);
				size += sizeof(struct mlx5_wqe_raddr_seg) / 16;
				break;

			default:
				break;
			}
			break;

		case IB_QPT_SMI:
			if (!mlx5_core_is_pf(mdev)) {
				err = -EINVAL;
				mlx5_ib_warn(dev, "Only physical function is allowed to send SMP MADs\n");
				*bad_wr = wr;
				goto out;
			}
		case IB_QPT_GSI:
		case IB_QPT_UD:
			set_datagram_seg(seg, wr);
			seg += sizeof(struct mlx5_wqe_datagram_seg);
			size += sizeof(struct mlx5_wqe_datagram_seg) / 16;
			if (unlikely((seg == qend)))
				seg = mlx5_get_send_wqe(qp, 0);
			break;
		default:
			break;
		}

		if (wr->send_flags & IB_SEND_INLINE && num_sge) {
			int uninitialized_var(sz);

			err = set_data_inl_seg(qp, wr, seg, &sz);
			if (unlikely(err)) {
				mlx5_ib_warn(dev, "Failed to prepare inline data segment\n");
				*bad_wr = wr;
				goto out;
			}
			inl = 1;
			size += sz;
		} else {
			dpseg = seg;
			for (i = 0; i < num_sge; i++) {
				if (unlikely(dpseg == qend)) {
					seg = mlx5_get_send_wqe(qp, 0);
					dpseg = seg;
				}
				if (likely(wr->sg_list[i].length)) {
					set_data_ptr_seg(dpseg, wr->sg_list + i);
					size += sizeof(struct mlx5_wqe_data_seg) / 16;
					dpseg++;
				}
			}
		}

		finish_wqe(qp, ctrl, size, idx, wr, nreq,
			   get_fence(fence, wr), next_fence,
			   mlx5_ib_opcode[wr->opcode]);
		if (0)
			dump_wqe(qp, idx, size);
	}

out:
	if (likely(nreq)) {
		qp->sq.head += nreq;

		/* Make sure that descriptors are written before
		 * updating doorbell record and ringing the doorbell
		 */
		wmb();

		qp->db.db[MLX5_SND_DBR] = cpu_to_be32(qp->sq.cur_post);

		/* Make sure doorbell record is visible to the HCA before
		 * we hit doorbell */
		wmb();

		if (bf->need_lock)
			spin_lock(&bf->lock);
		else
			__acquire(&bf->lock);

		/* TBD enable WC */
		if (BF_ENABLE && nreq == 1 && bf->uuarn && inl && size > 1 &&
		    size <= bf->buf_size / 16) {
			mlx5_bf_copy(bf->reg + bf->offset, (u64 *)ctrl, ALIGN(size * 16, 64), qp);
			/* wc_wmb(); */
		} else {
			mlx5_write64((__be32 *)ctrl, bf->regreg + bf->offset,
				     MLX5_GET_DOORBELL_LOCK(&bf->lock32));
			/* Make sure doorbells don't leak out of SQ spinlock
			 * and reach the HCA out of order.
			 */
			mmiowb();
		}
		bf->offset ^= bf->buf_size;
		if (bf->need_lock)
			spin_unlock(&bf->lock);
		else
			__release(&bf->lock);
	}

	spin_unlock_irqrestore(&qp->sq.lock, flags);

	return err;
}

static void set_sig_seg(struct mlx5_rwqe_sig *sig, int size)
{
	sig->signature = calc_sig(sig, size);
}

int mlx5_ib_post_recv(struct ib_qp *ibqp, struct ib_recv_wr *wr,
		      struct ib_recv_wr **bad_wr)
{
	struct mlx5_ib_qp *qp = to_mqp(ibqp);
	struct mlx5_wqe_data_seg *scat;
	struct mlx5_rwqe_sig *sig;
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx5_core_dev *mdev = dev->mdev;
	unsigned long flags;
	int err = 0;
	int nreq;
	int ind;
	int i;

	spin_lock_irqsave(&qp->rq.lock, flags);

	if (mdev->state == MLX5_DEVICE_STATE_INTERNAL_ERROR) {
		err = -EIO;
		*bad_wr = wr;
		nreq = 0;
		goto out;
	}

	ind = qp->rq.head & (qp->rq.wqe_cnt - 1);

	for (nreq = 0; wr; nreq++, wr = wr->next) {
		if (mlx5_wq_overflow(&qp->rq, nreq, qp->ibqp.recv_cq)) {
			err = -ENOMEM;
			*bad_wr = wr;
			goto out;
		}

		if (unlikely(wr->num_sge > qp->rq.max_gs)) {
			err = -EINVAL;
			*bad_wr = wr;
			goto out;
		}

		scat = get_recv_wqe(qp, ind);
		if (qp->wq_sig)
			scat++;

		for (i = 0; i < wr->num_sge; i++)
			set_data_ptr_seg(scat + i, wr->sg_list + i);

		if (i < qp->rq.max_gs) {
			scat[i].byte_count = 0;
			scat[i].lkey       = cpu_to_be32(MLX5_INVALID_LKEY);
			scat[i].addr       = 0;
		}

		if (qp->wq_sig) {
			sig = (struct mlx5_rwqe_sig *)scat;
			set_sig_seg(sig, (qp->rq.max_gs + 1) << 2);
		}

		qp->rq.rwr_ctx[ind].wrid = wr->wr_id;

		ind = (ind + 1) & (qp->rq.wqe_cnt - 1);
	}

out:
	if (likely(nreq)) {
		qp->rq.head += nreq;

		/* Make sure that descriptors are written before
		 * doorbell record.
		 */
		wmb();

		*qp->db.db = cpu_to_be32(qp->rq.head & 0xffff);
	}

	spin_unlock_irqrestore(&qp->rq.lock, flags);

	return err;
}

static inline enum ib_qp_state to_ib_qp_state(enum mlx5_qp_state mlx5_state)
{
	switch (mlx5_state) {
	case MLX5_QP_STATE_RST:      return IB_QPS_RESET;
	case MLX5_QP_STATE_INIT:     return IB_QPS_INIT;
	case MLX5_QP_STATE_RTR:      return IB_QPS_RTR;
	case MLX5_QP_STATE_RTS:      return IB_QPS_RTS;
	case MLX5_QP_STATE_SQ_DRAINING:
	case MLX5_QP_STATE_SQD:      return IB_QPS_SQD;
	case MLX5_QP_STATE_SQER:     return IB_QPS_SQE;
	case MLX5_QP_STATE_ERR:      return IB_QPS_ERR;
	default:		     return -1;
	}
}

static inline enum ib_mig_state to_ib_mig_state(int mlx5_mig_state)
{
	switch (mlx5_mig_state) {
	case MLX5_QP_PM_ARMED:		return IB_MIG_ARMED;
	case MLX5_QP_PM_REARM:		return IB_MIG_REARM;
	case MLX5_QP_PM_MIGRATED:	return IB_MIG_MIGRATED;
	default: return -1;
	}
}

static int to_ib_qp_access_flags(int mlx5_flags)
{
	int ib_flags = 0;

	if (mlx5_flags & MLX5_QP_BIT_RRE)
		ib_flags |= IB_ACCESS_REMOTE_READ;
	if (mlx5_flags & MLX5_QP_BIT_RWE)
		ib_flags |= IB_ACCESS_REMOTE_WRITE;
	if (mlx5_flags & MLX5_QP_BIT_RAE)
		ib_flags |= IB_ACCESS_REMOTE_ATOMIC;

	return ib_flags;
}

static void to_ib_ah_attr(struct mlx5_ib_dev *ibdev, struct ib_ah_attr *ib_ah_attr,
				struct mlx5_qp_path *path)
{
	struct mlx5_core_dev *dev = ibdev->mdev;

	memset(ib_ah_attr, 0, sizeof(*ib_ah_attr));
	ib_ah_attr->port_num	  = path->port;

	if (ib_ah_attr->port_num == 0 ||
	    ib_ah_attr->port_num > MLX5_CAP_GEN(dev, num_ports))
		return;

	ib_ah_attr->sl = path->dci_cfi_prio_sl & 0xf;

	ib_ah_attr->dlid	  = be16_to_cpu(path->rlid);
	ib_ah_attr->src_path_bits = path->grh_mlid & 0x7f;
	ib_ah_attr->static_rate   = path->static_rate ? path->static_rate - 5 : 0;
	ib_ah_attr->ah_flags      = (path->grh_mlid & (1 << 7)) ? IB_AH_GRH : 0;
	if (ib_ah_attr->ah_flags) {
		ib_ah_attr->grh.sgid_index = path->mgid_index;
		ib_ah_attr->grh.hop_limit  = path->hop_limit;
		ib_ah_attr->grh.traffic_class =
			(be32_to_cpu(path->tclass_flowlabel) >> 20) & 0xff;
		ib_ah_attr->grh.flow_label =
			be32_to_cpu(path->tclass_flowlabel) & 0xfffff;
		memcpy(ib_ah_attr->grh.dgid.raw,
		       path->rgid, sizeof(ib_ah_attr->grh.dgid.raw));
	}
}

int mlx5_ib_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *qp_attr, int qp_attr_mask,
		     struct ib_qp_init_attr *qp_init_attr)
{
	struct mlx5_ib_dev *dev = to_mdev(ibqp->device);
	struct mlx5_ib_qp *qp = to_mqp(ibqp);
	struct mlx5_query_qp_mbox_out *outb;
	struct mlx5_qp_context *context;
	int mlx5_state;
	int err = 0;

	mutex_lock(&qp->mutex);
	if (qp->ibqp.qp_type == IB_QPT_RAW_PACKET) {
		err = -EOPNOTSUPP;
		goto out;
	} else {
		outb = kzalloc(sizeof(*outb), GFP_KERNEL);
		if (!outb) {
			err = -ENOMEM;
			goto out;
		}

		context = &outb->ctx;
		err = mlx5_core_qp_query(dev->mdev, &qp->mqp, outb,
					 sizeof(*outb));
		if (err) {
			kfree(outb);
			goto out;
		}

		mlx5_state = be32_to_cpu(context->flags) >> 28;

		qp->state		     = to_ib_qp_state(mlx5_state);
		qp_attr->path_mtu	     = context->mtu_msgmax >> 5;
		qp_attr->path_mig_state	     =
			to_ib_mig_state((be32_to_cpu(context->flags) >> 11) & 0x3);
		qp_attr->qkey		     = be32_to_cpu(context->qkey);
		qp_attr->rq_psn		     = be32_to_cpu(context->rnr_nextrecvpsn) & 0xffffff;
		qp_attr->sq_psn		     = be32_to_cpu(context->next_send_psn) & 0xffffff;
		qp_attr->dest_qp_num	     = be32_to_cpu(context->log_pg_sz_remote_qpn) & 0xffffff;
		qp_attr->qp_access_flags     =
			to_ib_qp_access_flags(be32_to_cpu(context->params2));

		if (qp->ibqp.qp_type == IB_QPT_RC || qp->ibqp.qp_type == IB_QPT_UC) {
			to_ib_ah_attr(dev, &qp_attr->ah_attr, &context->pri_path);
			to_ib_ah_attr(dev, &qp_attr->alt_ah_attr, &context->alt_path);
				qp_attr->alt_pkey_index = be16_to_cpu(context->alt_path.pkey_index);
			qp_attr->alt_port_num	= qp_attr->alt_ah_attr.port_num;
		}

		qp_attr->pkey_index = be16_to_cpu(context->pri_path.pkey_index);
		qp_attr->port_num = context->pri_path.port;

		/* qp_attr->en_sqd_async_notify is only applicable in modify qp */
		qp_attr->sq_draining = mlx5_state == MLX5_QP_STATE_SQ_DRAINING;

		qp_attr->max_rd_atomic = 1 << ((be32_to_cpu(context->params1) >> 21) & 0x7);

		qp_attr->max_dest_rd_atomic =
			1 << ((be32_to_cpu(context->params2) >> 21) & 0x7);
		qp_attr->min_rnr_timer	    =
			(be32_to_cpu(context->rnr_nextrecvpsn) >> 24) & 0x1f;
		qp_attr->timeout	    = context->pri_path.ackto_lt >> 3;
		qp_attr->retry_cnt	    = (be32_to_cpu(context->params1) >> 16) & 0x7;
		qp_attr->rnr_retry	    = (be32_to_cpu(context->params1) >> 13) & 0x7;
		qp_attr->alt_timeout	    = context->alt_path.ackto_lt >> 3;


		kfree(outb);
	}

	qp_attr->qp_state	     = qp->state;
	qp_attr->cur_qp_state	     = qp_attr->qp_state;
	qp_attr->cap.max_recv_wr     = qp->rq.wqe_cnt;
	qp_attr->cap.max_recv_sge    = qp->rq.max_gs;

	if (!ibqp->uobject) {
		qp_attr->cap.max_send_wr  = qp->sq.max_post;
		qp_attr->cap.max_send_sge = qp->sq.max_gs;
		qp_init_attr->qp_context = ibqp->qp_context;
	} else {
		qp_attr->cap.max_send_wr  = 0;
		qp_attr->cap.max_send_sge = 0;
	}

	qp_init_attr->qp_type = ibqp->qp_type;
	qp_init_attr->recv_cq = ibqp->recv_cq;
	qp_init_attr->send_cq = ibqp->send_cq;
	qp_init_attr->srq = ibqp->srq;
	qp_attr->cap.max_inline_data = qp->max_inline_data;

	qp_init_attr->cap	     = qp_attr->cap;

	qp_init_attr->create_flags = 0;
	if (qp->flags & MLX5_IB_QP_BLOCK_MULTICAST_LOOPBACK)
		qp_init_attr->create_flags |= IB_QP_CREATE_BLOCK_MULTICAST_LOOPBACK;

	qp_init_attr->sq_sig_type = qp->sq_signal_bits & MLX5_WQE_CTRL_CQ_UPDATE ?
		IB_SIGNAL_ALL_WR : IB_SIGNAL_REQ_WR;

out:
	mutex_unlock(&qp->mutex);
	return err;
}

struct ib_xrcd *mlx5_ib_alloc_xrcd(struct ib_device *ibdev,
					  struct ib_ucontext *context,
					  struct ib_udata *udata)
{
	struct mlx5_ib_dev *dev = to_mdev(ibdev);
	struct mlx5_ib_xrcd *xrcd;
	int err;

	if (!MLX5_CAP_GEN(dev->mdev, xrc))
		return ERR_PTR(-ENOSYS);

	xrcd = kmalloc(sizeof(*xrcd), GFP_KERNEL);
	if (!xrcd)
		return ERR_PTR(-ENOMEM);

	err = mlx5_core_xrcd_alloc(dev->mdev, &xrcd->xrcdn);
	if (err) {
		kfree(xrcd);
		return ERR_PTR(-ENOMEM);
	}

	return &xrcd->ibxrcd;
}

int mlx5_ib_dealloc_xrcd(struct ib_xrcd *xrcd)
{
	struct mlx5_ib_dev *dev = to_mdev(xrcd->device);
	u32 xrcdn = to_mxrcd(xrcd)->xrcdn;
	int err;

	err = mlx5_core_xrcd_dealloc(dev->mdev, xrcdn);
	if (err) {
		mlx5_ib_warn(dev, "failed to dealloc xrcdn 0x%x\n", xrcdn);
		return err;
	}

	kfree(xrcd);

	return 0;
}
