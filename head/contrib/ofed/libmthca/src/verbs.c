/*
 * Copyright (c) 2005 Topspin Communications.  All rights reserved.
 * Copyright (c) 2005, 2006 Cisco Systems.  All rights reserved.
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
#include <strings.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>

#include "mthca.h"
#include "mthca-abi.h"

int mthca_query_device(struct ibv_context *context, struct ibv_device_attr *attr)
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
		 "%d.%d.%d", major, minor, sub_minor);

	return 0;
}

int mthca_query_port(struct ibv_context *context, uint8_t port,
		     struct ibv_port_attr *attr)
{
	struct ibv_query_port cmd;

	return ibv_cmd_query_port(context, port, attr, &cmd, sizeof cmd);
}

struct ibv_pd *mthca_alloc_pd(struct ibv_context *context)
{
	struct ibv_alloc_pd        cmd;
	struct mthca_alloc_pd_resp resp;
	struct mthca_pd           *pd;

	pd = malloc(sizeof *pd);
	if (!pd)
		return NULL;

	if (!mthca_is_memfree(context)) {
		pd->ah_list = NULL;
		if (pthread_mutex_init(&pd->ah_mutex, NULL)) {
			free(pd);
			return NULL;
		}
	}

	if (ibv_cmd_alloc_pd(context, &pd->ibv_pd, &cmd, sizeof cmd,
			     &resp.ibv_resp, sizeof resp)) {
		free(pd);
		return NULL;
	}

	pd->pdn = resp.pdn;

	return &pd->ibv_pd;
}

int mthca_free_pd(struct ibv_pd *pd)
{
	int ret;

	ret = ibv_cmd_dealloc_pd(pd);
	if (ret)
		return ret;

	free(to_mpd(pd));
	return 0;
}

static struct ibv_mr *__mthca_reg_mr(struct ibv_pd *pd, void *addr,
				     size_t length, uint64_t hca_va,
				     enum ibv_access_flags access,
				     int dma_sync)
{
	struct ibv_mr *mr;
	struct mthca_reg_mr cmd;
	int ret;

	/*
	 * Old kernels just ignore the extra data we pass in with the
	 * reg_mr command structure, so there's no need to add an ABI
	 * version check here (and indeed the kernel ABI was not
	 * incremented due to this change).
	 */
	cmd.mr_attrs = dma_sync ? MTHCA_MR_DMASYNC : 0;
	cmd.reserved = 0;

	mr = malloc(sizeof *mr);
	if (!mr)
		return NULL;

#ifdef IBV_CMD_REG_MR_HAS_RESP_PARAMS
	{
		struct ibv_reg_mr_resp resp;

		ret = ibv_cmd_reg_mr(pd, addr, length, hca_va, access, mr,
				     &cmd.ibv_cmd, sizeof cmd, &resp, sizeof resp);
	}
#else
	ret = ibv_cmd_reg_mr(pd, addr, length, hca_va, access, mr,
			     &cmd.ibv_cmd, sizeof cmd);
#endif
	if (ret) {
		free(mr);
		return NULL;
	}

	return mr;
}

struct ibv_mr *mthca_reg_mr(struct ibv_pd *pd, void *addr,
			    size_t length, enum ibv_access_flags access)
{
	return __mthca_reg_mr(pd, addr, length, (uintptr_t) addr, access, 0);
}

int mthca_dereg_mr(struct ibv_mr *mr)
{
	int ret;

	ret = ibv_cmd_dereg_mr(mr);
	if (ret)
		return ret;

	free(mr);
	return 0;
}

static int align_cq_size(int cqe)
{
	int nent;

	for (nent = 1; nent <= cqe; nent <<= 1)
		; /* nothing */

	return nent;
}

struct ibv_cq *mthca_create_cq(struct ibv_context *context, int cqe,
			       struct ibv_comp_channel *channel,
			       int comp_vector)
{
	struct mthca_create_cq      cmd;
	struct mthca_create_cq_resp resp;
	struct mthca_cq      	   *cq;
	int                  	    ret;

	/* Sanity check CQ size before proceeding */
	if (cqe > 131072)
		return NULL;

	cq = malloc(sizeof *cq);
	if (!cq)
		return NULL;

	cq->cons_index = 0;

	if (pthread_spin_init(&cq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err;

	cqe = align_cq_size(cqe);
	if (mthca_alloc_cq_buf(to_mdev(context->device), &cq->buf, cqe))
		goto err;

	cq->mr = __mthca_reg_mr(to_mctx(context)->pd, cq->buf.buf,
				cqe * MTHCA_CQ_ENTRY_SIZE,
				0, IBV_ACCESS_LOCAL_WRITE, 1);
	if (!cq->mr)
		goto err_buf;

	cq->mr->context = context;

	if (mthca_is_memfree(context)) {
		cq->arm_sn          = 1;
		cq->set_ci_db_index = mthca_alloc_db(to_mctx(context)->db_tab,
						     MTHCA_DB_TYPE_CQ_SET_CI,
						     &cq->set_ci_db);
		if (cq->set_ci_db_index < 0)
			goto err_unreg;

		cq->arm_db_index    = mthca_alloc_db(to_mctx(context)->db_tab,
						     MTHCA_DB_TYPE_CQ_ARM,
						     &cq->arm_db);
		if (cq->arm_db_index < 0)
			goto err_set_db;

		cmd.arm_db_page  = db_align(cq->arm_db);
		cmd.set_db_page  = db_align(cq->set_ci_db);
		cmd.arm_db_index = cq->arm_db_index;
		cmd.set_db_index = cq->set_ci_db_index;
	} else {
		cmd.arm_db_page  = cmd.set_db_page  =
		cmd.arm_db_index = cmd.set_db_index = 0;
	}

	cmd.lkey   = cq->mr->lkey;
	cmd.pdn    = to_mpd(to_mctx(context)->pd)->pdn;
	ret = ibv_cmd_create_cq(context, cqe - 1, channel, comp_vector,
				&cq->ibv_cq, &cmd.ibv_cmd, sizeof cmd,
				&resp.ibv_resp, sizeof resp);
	if (ret)
		goto err_arm_db;

	cq->cqn = resp.cqn;

	if (mthca_is_memfree(context)) {
		mthca_set_db_qn(cq->set_ci_db, MTHCA_DB_TYPE_CQ_SET_CI, cq->cqn);
		mthca_set_db_qn(cq->arm_db,    MTHCA_DB_TYPE_CQ_ARM,    cq->cqn);
	}

	return &cq->ibv_cq;

err_arm_db:
	if (mthca_is_memfree(context))
		mthca_free_db(to_mctx(context)->db_tab, MTHCA_DB_TYPE_CQ_ARM,
			      cq->arm_db_index);

err_set_db:
	if (mthca_is_memfree(context))
		mthca_free_db(to_mctx(context)->db_tab, MTHCA_DB_TYPE_CQ_SET_CI,
			      cq->set_ci_db_index);

err_unreg:
	mthca_dereg_mr(cq->mr);

err_buf:
	mthca_free_buf(&cq->buf);

err:
	free(cq);

	return NULL;
}

int mthca_resize_cq(struct ibv_cq *ibcq, int cqe)
{
	struct mthca_cq *cq = to_mcq(ibcq);
	struct mthca_resize_cq cmd;
	struct ibv_mr *mr;
	struct mthca_buf buf;
	int old_cqe;
	int ret;

	/* Sanity check CQ size before proceeding */
	if (cqe > 131072)
		return EINVAL;

	pthread_spin_lock(&cq->lock);

	cqe = align_cq_size(cqe);
	if (cqe == ibcq->cqe + 1) {
		ret = 0;
		goto out;
	}

	ret = mthca_alloc_cq_buf(to_mdev(ibcq->context->device), &buf, cqe);
	if (ret)
		goto out;

	mr = __mthca_reg_mr(to_mctx(ibcq->context)->pd, buf.buf,
			    cqe * MTHCA_CQ_ENTRY_SIZE,
			    0, IBV_ACCESS_LOCAL_WRITE, 1);
	if (!mr) {
		mthca_free_buf(&buf);
		ret = ENOMEM;
		goto out;
	}

	mr->context = ibcq->context;

	old_cqe = ibcq->cqe;

	cmd.lkey = mr->lkey;
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
		mthca_dereg_mr(mr);
		mthca_free_buf(&buf);
		goto out;
	}

	mthca_cq_resize_copy_cqes(cq, buf.buf, old_cqe);

	mthca_dereg_mr(cq->mr);
	mthca_free_buf(&cq->buf);

	cq->buf = buf;
	cq->mr  = mr;

out:
	pthread_spin_unlock(&cq->lock);
	return ret;
}

int mthca_destroy_cq(struct ibv_cq *cq)
{
	int ret;

	ret = ibv_cmd_destroy_cq(cq);
	if (ret)
		return ret;

	if (mthca_is_memfree(cq->context)) {
		mthca_free_db(to_mctx(cq->context)->db_tab, MTHCA_DB_TYPE_CQ_SET_CI,
			      to_mcq(cq)->set_ci_db_index);
		mthca_free_db(to_mctx(cq->context)->db_tab, MTHCA_DB_TYPE_CQ_ARM,
			      to_mcq(cq)->arm_db_index);
	}

	mthca_dereg_mr(to_mcq(cq)->mr);
	mthca_free_buf(&to_mcq(cq)->buf);
	free(to_mcq(cq));

	return 0;
}

static int align_queue_size(struct ibv_context *context, int size, int spare)
{
	int ret;

	/*
	 * If someone asks for a 0-sized queue, presumably they're not
	 * going to use it.  So don't mess with their size.
	 */
	if (!size)
		return 0;

	if (mthca_is_memfree(context)) {
		for (ret = 1; ret < size + spare; ret <<= 1)
			; /* nothing */

		return ret;
	} else
		return size + spare;
}

struct ibv_srq *mthca_create_srq(struct ibv_pd *pd,
				 struct ibv_srq_init_attr *attr)
{
	struct mthca_create_srq      cmd;
	struct mthca_create_srq_resp resp;
	struct mthca_srq            *srq;
	int                          ret;

	/* Sanity check SRQ size before proceeding */
	if (attr->attr.max_wr > 1 << 16 || attr->attr.max_sge > 64)
		return NULL;

	srq = malloc(sizeof *srq);
	if (!srq)
		return NULL;

	if (pthread_spin_init(&srq->lock, PTHREAD_PROCESS_PRIVATE))
		goto err;

	srq->max     = align_queue_size(pd->context, attr->attr.max_wr, 1);
	srq->max_gs  = attr->attr.max_sge;
	srq->counter = 0;

	if (mthca_alloc_srq_buf(pd, &attr->attr, srq))
		goto err;

	srq->mr = __mthca_reg_mr(pd, srq->buf.buf, srq->buf_size, 0, 0, 0);
	if (!srq->mr)
		goto err_free;

	srq->mr->context = pd->context;

	if (mthca_is_memfree(pd->context)) {
		srq->db_index = mthca_alloc_db(to_mctx(pd->context)->db_tab,
					       MTHCA_DB_TYPE_SRQ, &srq->db);
		if (srq->db_index < 0)
			goto err_unreg;

		cmd.db_page  = db_align(srq->db);
		cmd.db_index = srq->db_index;
	} else {
		cmd.db_page  = cmd.db_index = 0;
	}

	cmd.lkey = srq->mr->lkey;

	ret = ibv_cmd_create_srq(pd, &srq->ibv_srq, attr,
				 &cmd.ibv_cmd, sizeof cmd,
				 &resp.ibv_resp, sizeof resp);
	if (ret)
		goto err_db;

	srq->srqn = resp.srqn;

	if (mthca_is_memfree(pd->context))
		mthca_set_db_qn(srq->db, MTHCA_DB_TYPE_SRQ, srq->srqn);

	return &srq->ibv_srq;

err_db:
	if (mthca_is_memfree(pd->context))
		mthca_free_db(to_mctx(pd->context)->db_tab, MTHCA_DB_TYPE_SRQ,
			      srq->db_index);

err_unreg:
	mthca_dereg_mr(srq->mr);

err_free:
	free(srq->wrid);
	mthca_free_buf(&srq->buf);

err:
	free(srq);

	return NULL;
}

int mthca_modify_srq(struct ibv_srq *srq,
		     struct ibv_srq_attr *attr,
		     enum ibv_srq_attr_mask attr_mask)
{
	struct ibv_modify_srq cmd;

	return ibv_cmd_modify_srq(srq, attr, attr_mask, &cmd, sizeof cmd);
}

int mthca_query_srq(struct ibv_srq *srq,
		    struct ibv_srq_attr *attr)
{
	struct ibv_query_srq cmd;

	return ibv_cmd_query_srq(srq, attr, &cmd, sizeof cmd);
}

int mthca_destroy_srq(struct ibv_srq *srq)
{
	int ret;

	ret = ibv_cmd_destroy_srq(srq);
	if (ret)
		return ret;

	if (mthca_is_memfree(srq->context))
		mthca_free_db(to_mctx(srq->context)->db_tab, MTHCA_DB_TYPE_SRQ,
			      to_msrq(srq)->db_index);

	mthca_dereg_mr(to_msrq(srq)->mr);

	mthca_free_buf(&to_msrq(srq)->buf);
	free(to_msrq(srq)->wrid);
	free(to_msrq(srq));

	return 0;
}

struct ibv_qp *mthca_create_qp(struct ibv_pd *pd, struct ibv_qp_init_attr *attr)
{
	struct mthca_create_qp    cmd;
	struct ibv_create_qp_resp resp;
	struct mthca_qp          *qp;
	int                       ret;

	/* Sanity check QP size before proceeding */
	if (attr->cap.max_send_wr     > 65536 ||
	    attr->cap.max_recv_wr     > 65536 ||
	    attr->cap.max_send_sge    > 64    ||
	    attr->cap.max_recv_sge    > 64    ||
	    attr->cap.max_inline_data > 1024)
		return NULL;

	qp = malloc(sizeof *qp);
	if (!qp)
		return NULL;

	qp->sq.max = align_queue_size(pd->context, attr->cap.max_send_wr, 0);
	qp->rq.max = align_queue_size(pd->context, attr->cap.max_recv_wr, 0);

	if (mthca_alloc_qp_buf(pd, &attr->cap, attr->qp_type, qp))
		goto err;

	mthca_init_qp_indices(qp);

	if (pthread_spin_init(&qp->sq.lock, PTHREAD_PROCESS_PRIVATE) ||
	    pthread_spin_init(&qp->rq.lock, PTHREAD_PROCESS_PRIVATE))
		goto err_free;

	qp->mr = __mthca_reg_mr(pd, qp->buf.buf, qp->buf_size, 0, 0, 0);
	if (!qp->mr)
		goto err_free;

	qp->mr->context = pd->context;

	cmd.lkey     = qp->mr->lkey;
	cmd.reserved = 0;

	if (mthca_is_memfree(pd->context)) {
		qp->sq.db_index = mthca_alloc_db(to_mctx(pd->context)->db_tab,
						 MTHCA_DB_TYPE_SQ,
						 &qp->sq.db);
		if (qp->sq.db_index < 0)
			goto err_unreg;

		qp->rq.db_index = mthca_alloc_db(to_mctx(pd->context)->db_tab,
						 MTHCA_DB_TYPE_RQ,
						 &qp->rq.db);
		if (qp->rq.db_index < 0)
			goto err_sq_db;

		cmd.sq_db_page  = db_align(qp->sq.db);
		cmd.rq_db_page  = db_align(qp->rq.db);
		cmd.sq_db_index = qp->sq.db_index;
		cmd.rq_db_index = qp->rq.db_index;
	} else {
		cmd.sq_db_page  = cmd.rq_db_page  =
		cmd.sq_db_index = cmd.rq_db_index = 0;
	}

	pthread_mutex_lock(&to_mctx(pd->context)->qp_table_mutex);
	ret = ibv_cmd_create_qp(pd, &qp->ibv_qp, attr, &cmd.ibv_cmd, sizeof cmd,
				&resp, sizeof resp);
	if (ret)
		goto err_rq_db;

	if (mthca_is_memfree(pd->context)) {
		mthca_set_db_qn(qp->sq.db, MTHCA_DB_TYPE_SQ, qp->ibv_qp.qp_num);
		mthca_set_db_qn(qp->rq.db, MTHCA_DB_TYPE_RQ, qp->ibv_qp.qp_num);
	}

	ret = mthca_store_qp(to_mctx(pd->context), qp->ibv_qp.qp_num, qp);
	if (ret)
		goto err_destroy;
	pthread_mutex_unlock(&to_mctx(pd->context)->qp_table_mutex);

	qp->sq.max 	    = attr->cap.max_send_wr;
	qp->rq.max 	    = attr->cap.max_recv_wr;
	qp->sq.max_gs 	    = attr->cap.max_send_sge;
	qp->rq.max_gs 	    = attr->cap.max_recv_sge;
	qp->max_inline_data = attr->cap.max_inline_data;

	return &qp->ibv_qp;

err_destroy:
	ibv_cmd_destroy_qp(&qp->ibv_qp);

err_rq_db:
	pthread_mutex_unlock(&to_mctx(pd->context)->qp_table_mutex);
	if (mthca_is_memfree(pd->context))
		mthca_free_db(to_mctx(pd->context)->db_tab, MTHCA_DB_TYPE_RQ,
			      qp->rq.db_index);

err_sq_db:
	if (mthca_is_memfree(pd->context))
		mthca_free_db(to_mctx(pd->context)->db_tab, MTHCA_DB_TYPE_SQ,
			      qp->sq.db_index);

err_unreg:
	mthca_dereg_mr(qp->mr);

err_free:
	free(qp->wrid);
	mthca_free_buf(&qp->buf);

err:
	free(qp);

	return NULL;
}

int mthca_query_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		   enum ibv_qp_attr_mask attr_mask,
		   struct ibv_qp_init_attr *init_attr)
{
	struct ibv_query_qp cmd;

	return ibv_cmd_query_qp(qp, attr, attr_mask, init_attr, &cmd, sizeof cmd);
}

int mthca_modify_qp(struct ibv_qp *qp, struct ibv_qp_attr *attr,
		    enum ibv_qp_attr_mask attr_mask)
{
	struct ibv_modify_qp cmd;
	int ret;

	ret = ibv_cmd_modify_qp(qp, attr, attr_mask, &cmd, sizeof cmd);

	if (!ret		       &&
	    (attr_mask & IBV_QP_STATE) &&
	    attr->qp_state == IBV_QPS_RESET) {
		mthca_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
			       qp->srq ? to_msrq(qp->srq) : NULL);
		if (qp->send_cq != qp->recv_cq)
			mthca_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

		mthca_init_qp_indices(to_mqp(qp));

		if (mthca_is_memfree(qp->context)) {
			*to_mqp(qp)->sq.db = 0;
			*to_mqp(qp)->rq.db = 0;
		}
	}

	return ret;
}

static void mthca_lock_cqs(struct ibv_qp *qp)
{
	struct mthca_cq *send_cq = to_mcq(qp->send_cq);
	struct mthca_cq *recv_cq = to_mcq(qp->recv_cq);

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

static void mthca_unlock_cqs(struct ibv_qp *qp)
{
	struct mthca_cq *send_cq = to_mcq(qp->send_cq);
	struct mthca_cq *recv_cq = to_mcq(qp->recv_cq);

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

int mthca_destroy_qp(struct ibv_qp *qp)
{
	int ret;

	pthread_mutex_lock(&to_mctx(qp->context)->qp_table_mutex);
	ret = ibv_cmd_destroy_qp(qp);
	if (ret) {
		pthread_mutex_unlock(&to_mctx(qp->context)->qp_table_mutex);
		return ret;
	}

	mthca_lock_cqs(qp);

	__mthca_cq_clean(to_mcq(qp->recv_cq), qp->qp_num,
			 qp->srq ? to_msrq(qp->srq) : NULL);
	if (qp->send_cq != qp->recv_cq)
		__mthca_cq_clean(to_mcq(qp->send_cq), qp->qp_num, NULL);

	mthca_clear_qp(to_mctx(qp->context), qp->qp_num);

	mthca_unlock_cqs(qp);
	pthread_mutex_unlock(&to_mctx(qp->context)->qp_table_mutex);

	if (mthca_is_memfree(qp->context)) {
		mthca_free_db(to_mctx(qp->context)->db_tab, MTHCA_DB_TYPE_RQ,
			      to_mqp(qp)->rq.db_index);
		mthca_free_db(to_mctx(qp->context)->db_tab, MTHCA_DB_TYPE_SQ,
			      to_mqp(qp)->sq.db_index);
	}

	mthca_dereg_mr(to_mqp(qp)->mr);
	mthca_free_buf(&to_mqp(qp)->buf);
	free(to_mqp(qp)->wrid);
	free(to_mqp(qp));

	return 0;
}

struct ibv_ah *mthca_create_ah(struct ibv_pd *pd, struct ibv_ah_attr *attr)
{
	struct mthca_ah *ah;

	ah = malloc(sizeof *ah);
	if (!ah)
		return NULL;

	if (mthca_alloc_av(to_mpd(pd), attr, ah)) {
		free(ah);
		return NULL;
	}

	return &ah->ibv_ah;
}

int mthca_destroy_ah(struct ibv_ah *ah)
{
	mthca_free_av(to_mah(ah));
	free(to_mah(ah));

	return 0;
}

int mthca_attach_mcast(struct ibv_qp *qp, union ibv_gid *gid, uint16_t lid)
{
	return ibv_cmd_attach_mcast(qp, gid, lid);
}

int mthca_detach_mcast(struct ibv_qp *qp, union ibv_gid *gid, uint16_t lid)
{
	return ibv_cmd_detach_mcast(qp, gid, lid);
}
