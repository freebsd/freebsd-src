/*
 * Copyright (c) 2007 Cisco, Inc.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>

#include "mlx4.h"
#include "mlx4-abi.h"
#include "wqe.h"

int mlx4_query_device(struct ibv_context *context, struct ibv_device_attr *attr)
{
	struct ibv_query_device cmd;
	uint64_t raw_fw_ver;
	unsigned major, minor, sub_minor;
	int ret;

	ret = ibv_cmd_query_device(context, attr, &raw_fw_ver, &cmd, sizeof cmd);
	if (ret)
		return ret;

	major     = (raw_fw_ver >> 32) & 0xffff;
	minor     = (raw_fw_ver >> 16) & 0xffff;
	sub_minor = raw_fw_ver & 0xffff;

	snprintf(attr->fw_ver, sizeof attr->fw_ver,
		 "%d.%d.%03d", major, minor, sub_minor);

	return 0;
}

int mlx4_query_port(struct ibv_context *context, uint8_t port,
		     struct ibv_port_attr *attr)
{
	struct ibv_query_port cmd;

	return ibv_cmd_query_port(context, port, attr, &cmd, sizeof cmd);
}

struct ibv_pd *mlx4_alloc_pd(struct ibv_context *context)
{
	struct ibv_alloc_pd       cmd;
	struct mlx4_alloc_pd_resp resp;
	struct mlx4_pd		 *pd;

	pd = malloc(sizeof *pd);
	if (!pd)
		return NULL;

	if (ibv_cmd_alloc_pd(context, &pd->ibv_pd, &cmd, sizeof cmd,
			     &resp.ibv_resp, sizeof resp)) {
		free(pd);
		return NULL;
	}

	pd->pdn = resp.pdn;

	return &pd->ibv_pd;
}

int mlx4_free_pd(struct ibv_pd *pd)
{
	int ret;

	ret = ibv_cmd_dealloc_pd(pd);
	if (ret)
		return ret;

	free(to_mpd(pd));
	return 0;
}

struct ibv_mr *mlx4_reg_mr(struct ibv_pd *pd, void *addr, size_t length,
			   enum ibv_access_flags access)
{
	struct ibv_mr *mr;
	struct ibv_reg_mr cmd;
	int ret;

	mr = malloc(sizeof *mr);
	if (!mr)
		return NULL;

#ifdef IBV_CMD_REG_MR_HAS_RESP_PARAMS
	{
		struct ibv_reg_mr_resp resp;

		ret = ibv_cmd_reg_mr(pd, addr, length, (uintptr_t) addr,
				     access, mr, &cmd, sizeof cmd,
				     &resp, sizeof resp);
	}
#else
	ret = ibv_cmd_reg_mr(pd, addr, length, (uintptr_t) addr, access, mr,
			     &cmd, sizeof cmd);
#endif
	if (ret) {
		free(mr);
		return NULL;
	}

	return mr;
}

int mlx4_dereg_mr(struct ibv_mr *mr)
{
	int ret;

	ret = ibv_cmd_dereg_mr(mr);
	if (ret)
		return ret;

	free(mr);
	return 0;
}

static int align_queue_size(int req)
{
	int nent;

	for (nent = 1; nent < req; nent <<= 1)
		; /* nothing */

	return nent;
}

struct ibv_cq *mlx4_create_cq(struct ibv_context *context, int cqe,
			       struct ibv_comp_channel *channel,
			       int comp_vector)
{
	struct mlx4_create_cq      cmd;
	struct mlx4_create_cq_resp resp;
	struct mlx4_cq		  *cq;
	int			   ret;

	/* Sanity check CQ size before proceeding */
	if (cqe > 0x3fffff)
		return NULL;

	cq = malloc(sizeof *cq);
	if (!cq)
		return NULL;

	cq->cons_index = 0;

	if (pthread_spin_init(&cq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err;

	cqe = align_queue_size(cqe + 1);

	if (mlx4_alloc_cq_buf(to_mdev(context->device), &cq->buf, cqe))
		goto err;

	cq->set_ci_db  = mlx4_alloc_db(to_mctx(context), MLX4_DB_TYPE_CQ);
	if (!cq->set_ci_db)
		goto err_buf;

	cq->arm_db     = cq->set_ci_db + 1;
	*cq->arm_db    = 0;
	cq->arm_sn     = 1;
	*cq->set_ci_db = 0;

	cmd.buf_addr = (uintptr_t) cq->buf.buf;
	cmd.db_addr  = (uintptr_t) cq->set_ci_db;

	ret = ibv_cmd_create_cq(context, cqe - 1, channel, comp_vector,
				&cq->ibv_cq, &cmd.ibv_cmd, sizeof cmd,
				&resp.ibv_resp, sizeof resp);
	if (ret)
		goto err_db;

	cq->cqn = resp.cqn;

	return &cq->ibv_cq;

err_db:
	mlx4_free_db(to_mctx(context), MLX4_DB_TYPE_CQ, cq->set_ci_db);

err_buf:
	mlx4_free_buf(&cq->buf);

err:
	free(cq);

	return NULL;
}

int mlx4_resize_cq(struct ibv_cq *ibcq, int cqe)
{
	struct mlx4_cq *cq = to_mcq(ibcq);
	struct mlx4_resize_cq cmd;
	struct mlx4_buf buf;
	int old_cqe, outst_cqe, ret;

	/* Sanity check CQ size before proceeding */
	if (cqe > 0x3fffff)
		return EINVAL;

	pthread_spin_lock(&cq->lock);

	cqe = align_queue_size(cqe + 1);
	if (cqe == ibcq->cqe + 1) {
		ret = 0;
		goto out;
	}

	/* Can't be smaller then the number of outstanding CQEs */
	outst_cqe = mlx4_get_outstanding_cqes(cq);
	if (cqe < outst_cqe + 1) {
		ret = 0;
		goto out;
	}

	ret = mlx4_alloc_cq_buf(to_mdev(ibcq->context->device), &buf, cqe);
	if (ret)
		goto out;

	old_cqe = ibcq->cqe;
	cmd.buf_addr = (uintptr_t) buf.buf;

#ifdef IBV_CMD_RESIZE_CQ_HAS_RESP_PARAMS
	{
		struct ibv_resize_cq_resp resp;
		ret = ibv_cmd_resize_cq(ibcq, cqe - 1, &cmd.ibv_cmd, sizeof cmd,
					&resp, sizeof resp);
	}
#else
	ret = ibv_cmd_resize_cq(ibcq, cqe - 1, &cmd.ibv_cmd, sizeof cmd);
#endif
	if (ret) {
		mlx4_free_buf(&buf);
		goto out;
	}

	mlx4_cq_resize_copy_cqes(cq, buf.buf, old_cqe);

	mlx4_free_buf(&cq->buf);
	cq->buf = buf;

out:
	pthread_spin_unlock(&cq->lock);
	return ret;
}

int mlx4_destroy_cq(struct ibv_cq *cq)
{
	int ret;

	ret = ibv_cmd_destroy_cq(cq);
	if (ret)
		return ret;

	mlx4_free_db(to_mctx(cq->context), MLX4_DB_TYPE_CQ, to_mcq(cq)->set_ci_db);
	mlx4_free_buf(&to_mcq(cq)->buf);
	free(to_mcq(cq));

	return 0;
}

struct ibv_srq *mlx4_create_srq(struct ibv_pd *pd,
				 struct ibv_srq_init_attr *attr)
{
	struct mlx4_create_srq      cmd;
	struct mlx4_create_srq_resp resp;
	struct mlx4_srq		   *srq;
	int			    ret;

	/* Sanity check SRQ size before proceeding */
	if (attr->attr.max_wr > 1 << 16 || attr->attr.max_sge > 64)
		return NULL;

	srq = malloc(sizeof *srq);
	if (!srq)
		return NULL;

	if (pthread_spin_init(&srq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err;

	srq->max     = align_queue_size(attr->attr.max_wr + 1);
	srq->max_gs  = attr->attr.max_sge;
	srq->counter = 0;

	if (mlx4_alloc_srq_buf(pd, &attr->attr, srq))
		goto err;

	srq->db = mlx4_alloc_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ);
	if (!srq->db)
		goto err_free;

	*srq->db = 0;

	cmd.buf_addr = (uintptr_t) srq->buf.buf;
	cmd.db_addr  = (uintptr_t) srq->db;

	ret = ibv_cmd_create_srq(pd, &srq->ibv_srq, attr,
				 &cmd.ibv_cmd, sizeof cmd,
				 &resp.ibv_resp, sizeof resp);
	if (ret)
		goto err_db;

	srq->srqn = resp.srqn;

	return &srq->ibv_srq;

err_db:
	mlx4_free_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ, srq->db);

err_free:
	free(srq->wrid);
	mlx4_free_buf(&srq->buf);

err:
	free(srq);

	return NULL;
}

int mlx4_modify_srq(struct ibv_srq *srq,
		     struct ibv_srq_attr *attr,
		     enum ibv_srq_attr_mask attr_mask)
{
	struct ibv_modify_srq cmd;

	return ibv_cmd_modify_srq(srq, attr, attr_mask, &cmd, sizeof cmd);
}

int mlx4_query_srq(struct ibv_srq *srq,
		    struct ibv_srq_attr *attr)
{
	struct ibv_query_srq cmd;

	return ibv_cmd_query_srq(srq, attr, &cmd, sizeof cmd);
}

int mlx4_destroy_srq(struct ibv_srq *ibsrq)
{
	struct mlx4_srq *srq = to_msrq(ibsrq);
	struct mlx4_cq *mcq = NULL;
	int ret;

	if (ibsrq->xrc_cq) {
		/* is an xrc_srq */
		mcq = to_mcq(ibsrq->xrc_cq);
		mlx4_cq_clean(mcq, 0, srq);
		pthread_spin_lock(&mcq->lock);
		mlx4_clear_xrc_srq(to_mctx(ibsrq->context), srq->srqn);
		pthread_spin_unlock(&mcq->lock);
	}

	ret = ibv_cmd_destroy_srq(ibsrq);
	if (ret) {
		if (ibsrq->xrc_cq) {
			pthread_spin_lock(&mcq->lock);
			mlx4_store_xrc_srq(to_mctx(ibsrq->context),
					   srq->srqn, srq);
			pthread_spin_unlock(&mcq->lock);
		}
		return ret;
	}

	mlx4_free_db(to_mctx(ibsrq->context), MLX4_DB_TYPE_RQ, srq->db);
	mlx4_free_buf(&srq->buf);
	free(srq->wrid);
	free(srq);

	return 0;
}

static int verify_sizes(struct ibv_qp_init_attr *attr, struct mlx4_context *context)
{
	int size;
	int nsegs;

	if (attr->cap.max_send_wr     > context->max_qp_wr ||
	    attr->cap.max_recv_wr     > context->max_qp_wr ||
	    attr->cap.max_send_sge    > context->max_sge   ||
	    attr->cap.max_recv_sge    > context->max_sge)
		return -1;

	if (attr->cap.max_inline_data) {
		nsegs = num_inline_segs(attr->cap.max_inline_data, attr->qp_type);
		size = MLX4_MAX_WQE_SIZE - nsegs * sizeof (struct mlx4_wqe_inline_seg);
		switch (attr->qp_type) {
		case IBV_QPT_UD:
			size -= (sizeof (struct mlx4_wqe_ctrl_seg) +
				 sizeof (struct mlx4_wqe_datagram_seg));
			break;

		case IBV_QPT_RC:
		case IBV_QPT_UC:
		case IBV_QPT_XRC:
			size -= (sizeof (struct mlx4_wqe_ctrl_seg) +
				 sizeof (struct mlx4_wqe_raddr_seg));
			break;

		default:
			return 0;
		}

		if (attr->cap.max_inline_data > size)
			return -1;
	}

	return 0;
}

struct ibv_qp *mlx4_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *attr)
{
	struct mlx4_create_qp     cmd;
	struct ibv_create_qp_resp resp;
	struct mlx4_qp		 *qp;
	int			  ret;
	struct mlx4_context	 *context = to_mctx(pd->context);


	/* Sanity check QP size before proceeding */
	if (verify_sizes(attr, context))
		return NULL;

	qp = malloc(sizeof *qp);
	if (!qp)
		return NULL;

	mlx4_calc_sq_wqe_size(&attr->cap, attr->qp_type, qp);

	/*
	 * We need to leave 2 KB + 1 WQE of headroom in the SQ to
	 * allow HW to prefetch.
	 */
	qp->sq_spare_wqes = (2048 >> qp->sq.wqe_shift) + 1;
	qp->sq.wqe_cnt = align_queue_size(attr->cap.max_send_wr + qp->sq_spare_wqes);
	qp->rq.wqe_cnt = align_queue_size(attr->cap.max_recv_wr);

	if (attr->srq || attr->qp_type == IBV_QPT_XRC)
		attr->cap.max_recv_wr = qp->rq.wqe_cnt = 0;
	else {
		if (attr->cap.max_recv_sge < 1)
			attr->cap.max_recv_sge = 1;
		if (attr->cap.max_recv_wr < 1)
			attr->cap.max_recv_wr = 1;
	}

	if (mlx4_alloc_qp_buf(pd, &attr->cap, attr->qp_type, qp))
		goto err;

	mlx4_init_qp_indices(qp);

	if (pthread_spin_init(&qp->sq.lock, PTHREAD_PROCESS_PRIVATE) ||
	    pthread_spin_init(&qp->rq.lock, PTHREAD_PROCESS_PRIVATE))
		goto err_free;

	if (!attr->srq && attr->qp_type != IBV_QPT_XRC) {
		qp->db = mlx4_alloc_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ);
		if (!qp->db)
			goto err_free;

		*qp->db = 0;
	}

	cmd.buf_addr	    = (uintptr_t) qp->buf.buf;
	if (attr->srq || attr->qp_type == IBV_QPT_XRC)
		cmd.db_addr = 0;
	else
		cmd.db_addr = (uintptr_t) qp->db;
	cmd.log_sq_stride   = qp->sq.wqe_shift;
	for (cmd.log_sq_bb_count = 0;
	     qp->sq.wqe_cnt > 1 << cmd.log_sq_bb_count;
	     ++cmd.log_sq_bb_count)
		; /* nothing */
	cmd.sq_no_prefetch = 0;	/* OK for ABI 2: just a reserved field */
	memset(cmd.reserved, 0, sizeof cmd.reserved);

	pthread_mutex_lock(&to_mctx(pd->context)->qp_table_mutex);

	ret = ibv_cmd_create_qp(pd, &qp->ibv_qp, attr, &cmd.ibv_cmd, sizeof cmd,
				&resp, sizeof resp);
	if (ret)
		goto err_rq_db;

	ret = mlx4_store_qp(to_mctx(pd->context), qp->ibv_qp.qp_num, qp);
	if (ret)
		goto err_destroy;
	pthread_mutex_unlock(&to_mctx(pd->context)->qp_table_mutex);

	qp->rq.wqe_cnt = attr->cap.max_recv_wr;
	qp->rq.max_gs  = attr->cap.max_recv_sge;

	/* adjust rq maxima to not exceed reported device maxima */
	attr->cap.max_recv_wr = min(context->max_qp_wr, attr->cap.max_recv_wr);
	attr->cap.max_recv_sge = min(context->max_sge, attr->cap.max_recv_sge);

	qp->rq.max_post = attr->cap.max_recv_wr;
	mlx4_set_sq_sizes(qp, &attr->cap, attr->qp_type);

	qp->doorbell_qpn    = htonl(qp->ibv_qp.qp_num << 8);
	if (attr->sq_sig_all)
		qp->sq_signal_bits = htonl(MLX4_WQE_CTRL_CQ_UPDATE);
	else
		qp->sq_signal_bits = 0;

	return &qp->ibv_qp;

err_destroy:
	ibv_cmd_destroy_qp(&qp->ibv_qp);

err_rq_db:
	pthread_mutex_unlock(&to_mctx(pd->context)->qp_table_mutex);
	if (!attr->srq && attr->qp_type != IBV_QPT_XRC)
		mlx4_free_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ, qp->db);

err_free:
	free(qp->sq.wrid);
	if (qp->rq.wqe_cnt)
		free(qp->rq.wrid);
	mlx4_free_buf(&qp->buf);

err:
	free(qp);

	return NULL;
}

int mlx4_query_qp(struct ibv_qp *ibqp, struct ibv_qp_attr *attr,
		   enum ibv_qp_attr_mask attr_mask,
		   struct ibv_qp_init_attr *init_attr)
{
	struct ibv_query_qp cmd;
	struct mlx4_qp *qp = to_mqp(ibqp);
	int ret;

	ret = ibv_cmd_query_qp(ibqp, attr, attr_mask, init_attr, &cmd, sizeof cmd);
	if (ret)
		return ret;

	init_attr->cap.max_send_wr     = qp->sq.max_post;
	init_attr->cap.max_send_sge    = qp->sq.max_gs;
	init_attr->cap.max_inline_data = qp->max_inline_data;

	attr->cap = init_attr->cap;

	return 0;
}

int mlx4_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		    enum ibv_qp_attr_mask attr_mask)
{
	struct ibv_modify_qp cmd;
	int ret;

	if (qp->state == IBV_QPS_RESET &&
	    attr_mask & IBV_QP_STATE   &&
	    attr->qp_state == IBV_QPS_INIT) {
		mlx4_qp_init_sq_ownership(to_mqp(qp));
	}

	ret = ibv_cmd_modify_qp(qp, attr, attr_mask, &cmd, sizeof cmd);

	if (!ret		       &&
	    (attr_mask & IBV_QP_STATE) &&
	    attr->qp_state == IBV_QPS_RESET) {
		mlx4_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
			       qp->srq ? to_msrq(qp->srq) : NULL);
		if (qp->send_cq != qp->recv_cq)
			mlx4_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

		mlx4_init_qp_indices(to_mqp(qp));
		if (!qp->srq && qp->qp_type != IBV_QPT_XRC)
			*to_mqp(qp)->db = 0;
	}

	return ret;
}

static void mlx4_lock_cqs(struct ibv_qp *qp)
{
	struct mlx4_cq *send_cq = to_mcq(qp->send_cq);
	struct mlx4_cq *recv_cq = to_mcq(qp->recv_cq);

	if (send_cq == recv_cq)
		pthread_spin_lock(&send_cq->lock);
	else if (send_cq->cqn < recv_cq->cqn) {
		pthread_spin_lock(&send_cq->lock);
		pthread_spin_lock(&recv_cq->lock);
	} else {
		pthread_spin_lock(&recv_cq->lock);
		pthread_spin_lock(&send_cq->lock);
	}
}

static void mlx4_unlock_cqs(struct ibv_qp *qp)
{
	struct mlx4_cq *send_cq = to_mcq(qp->send_cq);
	struct mlx4_cq *recv_cq = to_mcq(qp->recv_cq);

	if (send_cq == recv_cq)
		pthread_spin_unlock(&send_cq->lock);
	else if (send_cq->cqn < recv_cq->cqn) {
		pthread_spin_unlock(&recv_cq->lock);
		pthread_spin_unlock(&send_cq->lock);
	} else {
		pthread_spin_unlock(&send_cq->lock);
		pthread_spin_unlock(&recv_cq->lock);
	}
}

int mlx4_destroy_qp(struct ibv_qp *ibqp)
{
	struct mlx4_qp *qp = to_mqp(ibqp);
	int ret;

	pthread_mutex_lock(&to_mctx(ibqp->context)->qp_table_mutex);
	ret = ibv_cmd_destroy_qp(ibqp);
	if (ret) {
		pthread_mutex_unlock(&to_mctx(ibqp->context)->qp_table_mutex);
		return ret;
	}

	mlx4_lock_cqs(ibqp);

	__mlx4_cq_clean(to_mcq(ibqp->recv_cq), ibqp->qp_num,
			ibqp->srq ? to_msrq(ibqp->srq) : NULL);
	if (ibqp->send_cq != ibqp->recv_cq)
		__mlx4_cq_clean(to_mcq(ibqp->send_cq), ibqp->qp_num, NULL);

	mlx4_clear_qp(to_mctx(ibqp->context), ibqp->qp_num);

	mlx4_unlock_cqs(ibqp);
	pthread_mutex_unlock(&to_mctx(ibqp->context)->qp_table_mutex);

	if (!ibqp->srq && ibqp->qp_type != IBV_QPT_XRC)
		mlx4_free_db(to_mctx(ibqp->context), MLX4_DB_TYPE_RQ, qp->db);
	free(qp->sq.wrid);
	if (qp->rq.wqe_cnt)
		free(qp->rq.wrid);
	mlx4_free_buf(&qp->buf);
	free(qp);

	return 0;
}

struct ibv_ah *mlx4_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
	struct mlx4_ah *ah;
	struct ibv_port_attr port_attr;
	uint8_t is_mcast;

	ah = malloc(sizeof *ah);
	if (!ah)
		return NULL;

	memset(ah, 0, sizeof *ah);

	ah->av.port_pd   = htonl(to_mpd(pd)->pdn | (attr->port_num << 24));
	ah->av.g_slid    = attr->src_path_bits;
	ah->av.dlid      = htons(attr->dlid);
	if (attr->static_rate) {
		ah->av.stat_rate = attr->static_rate + MLX4_STAT_RATE_OFFSET;
		/* XXX check rate cap? */
	}
	ah->av.sl_tclass_flowlabel = htonl(attr->sl << 28);
	if (attr->is_global) {
		ah->av.g_slid   |= 0x80;
		ah->av.gid_index = attr->grh.sgid_index;
		ah->av.hop_limit = attr->grh.hop_limit;
		ah->av.sl_tclass_flowlabel |=
			htonl((attr->grh.traffic_class << 20) |
				    attr->grh.flow_label);
		memcpy(ah->av.dgid, attr->grh.dgid.raw, 16);
	}

	if (ibv_query_port(pd->context, attr->port_num, &port_attr))
		goto err;

	if (port_attr.link_layer == IBV_LINK_LAYER_ETHERNET) {
		if (ibv_resolve_eth_gid(pd, attr->port_num,
					(union ibv_gid *)ah->av.dgid,
					attr->grh.sgid_index,
					ah->mac, &ah->vlan,
					&ah->tagged, &is_mcast))
			goto err;

		if (is_mcast) {
			ah->av.dlid = htons(0xc000);
			ah->av.port_pd |= htonl(1 << 31);
		}
		if (ah->tagged) {
			ah->av.port_pd |= htonl(1 << 29);
			ah->vlan |= (attr->sl & 7) << 13;
		}
	}


	return &ah->ibv_ah;
err:
	free(ah);
	return NULL;
}

int mlx4_destroy_ah(struct ibv_ah *ah)
{
	free(to_mah(ah));

	return 0;
}

#ifdef HAVE_IBV_XRC_OPS
struct ibv_srq *mlx4_create_xrc_srq(struct ibv_pd *pd,
				    struct ibv_xrc_domain *xrc_domain,
				    struct ibv_cq *xrc_cq,
				    struct ibv_srq_init_attr *attr)
{
	struct mlx4_create_xrc_srq  cmd;
	struct mlx4_create_srq_resp resp;
	struct mlx4_srq		   *srq;
	int			    ret;

	/* Sanity check SRQ size before proceeding */
	if (attr->attr.max_wr > 1 << 16 || attr->attr.max_sge > 64)
		return NULL;

	srq = malloc(sizeof *srq);
	if (!srq)
		return NULL;

	if (pthread_spin_init(&srq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err;

	srq->max     = align_queue_size(attr->attr.max_wr + 1);
	srq->max_gs  = attr->attr.max_sge;
	srq->counter = 0;

	if (mlx4_alloc_srq_buf(pd, &attr->attr, srq))
		goto err;

	srq->db = mlx4_alloc_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ);
	if (!srq->db)
		goto err_free;

	*srq->db = 0;

	cmd.buf_addr = (uintptr_t) srq->buf.buf;
	cmd.db_addr  = (uintptr_t) srq->db;

	ret = ibv_cmd_create_xrc_srq(pd, &srq->ibv_srq, attr,
				     xrc_domain->handle,
				     xrc_cq->handle,
				     &cmd.ibv_cmd, sizeof cmd,
				     &resp.ibv_resp, sizeof resp);
	if (ret)
		goto err_db;

	srq->ibv_srq.xrc_srq_num = srq->srqn = resp.srqn;

	ret = mlx4_store_xrc_srq(to_mctx(pd->context), srq->ibv_srq.xrc_srq_num, srq);
	if (ret)
		goto err_destroy;

	return &srq->ibv_srq;

err_destroy:
	ibv_cmd_destroy_srq(&srq->ibv_srq);

err_db:
	mlx4_free_db(to_mctx(pd->context), MLX4_DB_TYPE_RQ, srq->db);

err_free:
	free(srq->wrid);
	mlx4_free_buf(&srq->buf);

err:
	free(srq);

	return NULL;
}

struct ibv_xrc_domain *mlx4_open_xrc_domain(struct ibv_context *context,
					    int fd, int oflag)
{
	int ret;
	struct mlx4_open_xrc_domain_resp resp;
	struct mlx4_xrc_domain *xrcd;

	xrcd = malloc(sizeof *xrcd);
	if (!xrcd)
		return NULL;

	ret = ibv_cmd_open_xrc_domain(context, fd, oflag, &xrcd->ibv_xrcd,
				      &resp.ibv_resp, sizeof resp);
	if (ret) {
		free(xrcd);
		return NULL;
	}

	xrcd->xrcdn = resp.xrcdn;
	return &xrcd->ibv_xrcd;
}

int mlx4_close_xrc_domain(struct ibv_xrc_domain *d)
{
	int ret;
	ret = ibv_cmd_close_xrc_domain(d);
	if (!ret)
		free(d);
	return ret;
}

int mlx4_create_xrc_rcv_qp(struct ibv_qp_init_attr *init_attr,
			   uint32_t *xrc_qp_num)
{

	return ibv_cmd_create_xrc_rcv_qp(init_attr, xrc_qp_num);
}

int mlx4_modify_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			   uint32_t xrc_qp_num,
			   struct ibv_qp_attr *attr,
			   int attr_mask)
{
	return ibv_cmd_modify_xrc_rcv_qp(xrc_domain, xrc_qp_num,
					 attr, attr_mask);
}

int mlx4_query_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			  uint32_t xrc_qp_num,
			  struct ibv_qp_attr *attr,
			  int attr_mask,
			  struct ibv_qp_init_attr *init_attr)
{
	int ret;

	ret = ibv_cmd_query_xrc_rcv_qp(xrc_domain, xrc_qp_num,
				       attr, attr_mask, init_attr);
	if (ret)
		return ret;

	init_attr->cap.max_send_wr = init_attr->cap.max_send_sge = 1;
	init_attr->cap.max_recv_sge = init_attr->cap.max_recv_wr = 0;
	init_attr->cap.max_inline_data = 0;
	init_attr->recv_cq = init_attr->send_cq = NULL;
	init_attr->srq = NULL;
	init_attr->xrc_domain = xrc_domain;
	init_attr->qp_type = IBV_QPT_XRC;
	init_attr->qp_context = NULL;
	attr->cap = init_attr->cap;

	return 0;
}

int mlx4_reg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			uint32_t xrc_qp_num)
{
	return ibv_cmd_reg_xrc_rcv_qp(xrc_domain, xrc_qp_num);
}

int mlx4_unreg_xrc_rcv_qp(struct ibv_xrc_domain *xrc_domain,
			  uint32_t xrc_qp_num)
{
	return ibv_cmd_unreg_xrc_rcv_qp(xrc_domain, xrc_qp_num);
}

#endif
