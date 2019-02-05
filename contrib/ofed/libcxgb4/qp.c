/*
 * Copyright (c) 2006-2016 Chelsio, Inc. All rights reserved.
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
#include <config.h>

#include <assert.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include "libcxgb4.h"

#ifdef STATS
struct c4iw_stats c4iw_stats;
#endif

static void copy_wr_to_sq(struct t4_wq *wq, union t4_wr *wqe, u8 len16)
{
	void *src, *dst;
	uintptr_t end;
	int total, len;

	src = &wqe->flits[0];
	dst = &wq->sq.queue->flits[wq->sq.wq_pidx *
	    (T4_EQ_ENTRY_SIZE / sizeof(__be64))];
	if (t4_sq_onchip(wq)) {
		len16 = align(len16, 4);

		/* In onchip mode the copy below will be made to WC memory and
		 * could trigger DMA. In offchip mode the copy below only
		 * queues the WQE, DMA cannot start until t4_ring_sq_db
		 * happens */
		mmio_wc_start();
	}

	/* NOTE len16 cannot be large enough to write to the
	   same sq.queue memory twice in this loop */
	total = len16 * 16;
	end = (uintptr_t)&wq->sq.queue[wq->sq.size];
	if (__predict_true((uintptr_t)dst + total <= end)) {
		/* Won't wrap around. */
		memcpy(dst, src, total);
	} else {
		len = end - (uintptr_t)dst;
		memcpy(dst, src, len);
		memcpy(wq->sq.queue, src + len, total - len);
	}

	if (t4_sq_onchip(wq))
		mmio_flush_writes();
}

static void copy_wr_to_rq(struct t4_wq *wq, union t4_recv_wr *wqe, u8 len16)
{
	void *src, *dst;
	uintptr_t end;
	int total, len;

	src = &wqe->flits[0];
	dst = &wq->rq.queue->flits[wq->rq.wq_pidx *
	    (T4_EQ_ENTRY_SIZE / sizeof(__be64))];

	total = len16 * 16;
	end = (uintptr_t)&wq->rq.queue[wq->rq.size];
	if (__predict_true((uintptr_t)dst + total <= end)) {
		/* Won't wrap around. */
		memcpy(dst, src, total);
	} else {
		len = end - (uintptr_t)dst;
		memcpy(dst, src, len);
		memcpy(wq->rq.queue, src + len, total - len);
	}
}

static int build_immd(struct t4_sq *sq, struct fw_ri_immd *immdp,
		      struct ibv_send_wr *wr, int max, u32 *plenp)
{
	u8 *dstp, *srcp;
	u32 plen = 0;
	int i;
	int len;

	dstp = (u8 *)immdp->data;
	for (i = 0; i < wr->num_sge; i++) {
		if ((plen + wr->sg_list[i].length) > max)
			return -EMSGSIZE;
		srcp = (u8 *)(unsigned long)wr->sg_list[i].addr;
		plen += wr->sg_list[i].length;
		len = wr->sg_list[i].length;
		memcpy(dstp, srcp, len);
		dstp += len;
		srcp += len;
	}
	len = ROUND_UP(plen + 8, 16) - (plen + 8);
	if (len)
		memset(dstp, 0, len);
	immdp->op = FW_RI_DATA_IMMD;
	immdp->r1 = 0;
	immdp->r2 = 0;
	immdp->immdlen = htobe32(plen);
	*plenp = plen;
	return 0;
}

static int build_isgl(__be64 *queue_start, __be64 *queue_end,
		      struct fw_ri_isgl *isglp, struct ibv_sge *sg_list,
		      int num_sge, u32 *plenp)
{
	int i;
	u32 plen = 0;
	__be64 *flitp;

	if ((__be64 *)isglp == queue_end)
		isglp = (struct fw_ri_isgl *)queue_start;

	flitp = (__be64 *)isglp->sge;
	for (i = 0; i < num_sge; i++) {
		if ((plen + sg_list[i].length) < plen)
			return -EMSGSIZE;
		plen += sg_list[i].length;
		*flitp = htobe64(((u64)sg_list[i].lkey << 32) |
				   sg_list[i].length);
		if (++flitp == queue_end)
			flitp = queue_start;
		*flitp = htobe64(sg_list[i].addr);
		if (++flitp == queue_end)
			flitp = queue_start;
	}
	*flitp = 0;
	isglp->op = FW_RI_DATA_ISGL;
	isglp->r1 = 0;
	isglp->nsge = htobe16(num_sge);
	isglp->r2 = 0;
	if (plenp)
		*plenp = plen;
	return 0;
}

static int build_rdma_send(struct t4_sq *sq, union t4_wr *wqe,
			   struct ibv_send_wr *wr, u8 *len16)
{
	u32 plen;
	int size;
	int ret;

	if (wr->num_sge > T4_MAX_SEND_SGE)
		return -EINVAL;
	switch (wr->opcode) {
	case IBV_WR_SEND:
		if (wr->send_flags & IBV_SEND_SOLICITED)
			wqe->send.sendop_pkd = htobe32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_SE));
		else
			wqe->send.sendop_pkd = htobe32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND));
		wqe->send.stag_inv = 0;
		break;
	case IBV_WR_SEND_WITH_INV:
		if (wr->send_flags & IBV_SEND_SOLICITED)
			wqe->send.sendop_pkd = htobe32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_SE_INV));
		else
			wqe->send.sendop_pkd = htobe32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_INV));
		wqe->send.stag_inv = htobe32(wr->invalidate_rkey);
		break;
	default:
		return -EINVAL;
	}
	wqe->send.r3 = 0;
	wqe->send.r4 = 0;

	plen = 0;
	if (wr->num_sge) {
		if (wr->send_flags & IBV_SEND_INLINE) {
			ret = build_immd(sq, wqe->send.u.immd_src, wr,
					 T4_MAX_SEND_INLINE, &plen);
			if (ret)
				return ret;
			size = sizeof wqe->send + sizeof(struct fw_ri_immd) +
			       plen;
		} else {
			ret = build_isgl((__be64 *)sq->queue,
					 (__be64 *)&sq->queue[sq->size],
					 wqe->send.u.isgl_src,
					 wr->sg_list, wr->num_sge, &plen);
			if (ret)
				return ret;
			size = sizeof wqe->send + sizeof(struct fw_ri_isgl) +
			       wr->num_sge * sizeof (struct fw_ri_sge);
		}
	} else {
		wqe->send.u.immd_src[0].op = FW_RI_DATA_IMMD;
		wqe->send.u.immd_src[0].r1 = 0;
		wqe->send.u.immd_src[0].r2 = 0;
		wqe->send.u.immd_src[0].immdlen = 0;
		size = sizeof wqe->send + sizeof(struct fw_ri_immd);
		plen = 0;
	}
	*len16 = DIV_ROUND_UP(size, 16);
	wqe->send.plen = htobe32(plen);
	return 0;
}

static int build_rdma_write(struct t4_sq *sq, union t4_wr *wqe,
			    struct ibv_send_wr *wr, u8 *len16)
{
	u32 plen;
	int size;
	int ret;

	if (wr->num_sge > T4_MAX_SEND_SGE)
		return -EINVAL;
	if (wr->opcode == IBV_WR_RDMA_WRITE_WITH_IMM)
		wqe->write.immd_data = wr->imm_data;
	else
		wqe->write.immd_data = 0;
	wqe->write.stag_sink = htobe32(wr->wr.rdma.rkey);
	wqe->write.to_sink = htobe64(wr->wr.rdma.remote_addr);
	if (wr->num_sge) {
		if (wr->send_flags & IBV_SEND_INLINE) {
			ret = build_immd(sq, wqe->write.u.immd_src, wr,
					 T4_MAX_WRITE_INLINE, &plen);
			if (ret)
				return ret;
			size = sizeof wqe->write + sizeof(struct fw_ri_immd) +
			       plen;
		} else {
			ret = build_isgl((__be64 *)sq->queue,
					 (__be64 *)&sq->queue[sq->size],
					 wqe->write.u.isgl_src,
					 wr->sg_list, wr->num_sge, &plen);
			if (ret)
				return ret;
			size = sizeof wqe->write + sizeof(struct fw_ri_isgl) +
			       wr->num_sge * sizeof (struct fw_ri_sge);
		}
	} else {
		wqe->write.u.immd_src[0].op = FW_RI_DATA_IMMD;
		wqe->write.u.immd_src[0].r1 = 0;
		wqe->write.u.immd_src[0].r2 = 0;
		wqe->write.u.immd_src[0].immdlen = 0;
		size = sizeof wqe->write + sizeof(struct fw_ri_immd);
		plen = 0;
	}
	*len16 = DIV_ROUND_UP(size, 16);
	wqe->write.plen = htobe32(plen);
	return 0;
}

static void build_immd_cmpl(struct t4_sq *sq, struct fw_ri_immd_cmpl *immdp,
			   struct ibv_send_wr *wr)
{
	memcpy((u8 *)immdp->data, (u8 *)(uintptr_t)wr->sg_list->addr, 16);
	memset(immdp->r1, 0, 6);
	immdp->op = FW_RI_DATA_IMMD;
	immdp->immdlen = 16;
	return;
}

#define BUILD_BUG_ON(condition) ((void)sizeof(char[1 - 2*!!(condition)]))

static void build_rdma_write_cmpl(struct t4_sq *sq,
				  struct fw_ri_rdma_write_cmpl_wr *wcwr,
				  struct ibv_send_wr *wr, u8 *len16)
{
	u32 plen;
	int size;

	/*
	 * This code assumes the struct fields preceeding the write isgl
	 * fit in one 64B WR slot.  This is because the WQE is built
	 * directly in the dma queue, and wrapping is only handled
	 * by the code buildling sgls.  IE the "fixed part" of the wr
	 * structs must all fit in 64B.  The WQE build code should probably be
	 * redesigned to avoid this restriction, but for now just add
	 * the BUILD_BUG_ON() to catch if this WQE struct gets too big.
	 */
	BUILD_BUG_ON(offsetof(struct fw_ri_rdma_write_cmpl_wr, u) > 64);

	wcwr->stag_sink = htobe32(wr->wr.rdma.rkey);
	wcwr->to_sink = htobe64(wr->wr.rdma.remote_addr);
	if (wr->next->opcode == IBV_WR_SEND)
		wcwr->stag_inv = 0;
	else
		wcwr->stag_inv = htobe32(wr->next->invalidate_rkey);
	wcwr->r2 = 0;
	wcwr->r3 = 0;

 	/* SEND_INV SGL */
	if (wr->next->send_flags & IBV_SEND_INLINE)
		build_immd_cmpl(sq, &wcwr->u_cmpl.immd_src, wr->next);
	else
		build_isgl((__be64 *)sq->queue, (__be64 *)&sq->queue[sq->size],
			   &wcwr->u_cmpl.isgl_src, wr->next->sg_list, 1, NULL);

	/* WRITE SGL */
	build_isgl((__be64 *)sq->queue, (__be64 *)&sq->queue[sq->size],
		   wcwr->u.isgl_src, wr->sg_list, wr->num_sge, &plen);

	size = sizeof *wcwr + sizeof(struct fw_ri_isgl) +
	       wr->num_sge * sizeof(struct fw_ri_sge);
	wcwr->plen = htobe32(plen);
	*len16 = DIV_ROUND_UP(size, 16);

	return;
}

static int build_rdma_read(union t4_wr *wqe, struct ibv_send_wr *wr, u8 *len16)
{
	if (wr->num_sge > 1)
		return -EINVAL;
	if (wr->num_sge) {
		wqe->read.stag_src = htobe32(wr->wr.rdma.rkey);
		wqe->read.to_src_hi = htobe32((u32)(wr->wr.rdma.remote_addr >>32));
		wqe->read.to_src_lo = htobe32((u32)wr->wr.rdma.remote_addr);
		wqe->read.stag_sink = htobe32(wr->sg_list[0].lkey);
		wqe->read.plen = htobe32(wr->sg_list[0].length);
		wqe->read.to_sink_hi = htobe32((u32)(wr->sg_list[0].addr >> 32));
		wqe->read.to_sink_lo = htobe32((u32)(wr->sg_list[0].addr));
	} else {
		wqe->read.stag_src = htobe32(2);
		wqe->read.to_src_hi = 0;
		wqe->read.to_src_lo = 0;
		wqe->read.stag_sink = htobe32(2);
		wqe->read.plen = 0;
		wqe->read.to_sink_hi = 0;
		wqe->read.to_sink_lo = 0;
	}
	wqe->read.r2 = 0;
	wqe->read.r5 = 0;
	*len16 = DIV_ROUND_UP(sizeof wqe->read, 16);
	return 0;
}

static int build_rdma_recv(struct t4_rq *rq, union t4_recv_wr *wqe,
			   struct ibv_recv_wr *wr, u8 *len16)
{
	int ret;

	ret = build_isgl((__be64 *)rq->queue, (__be64 *)&rq->queue[rq->size],
			 &wqe->recv.isgl, wr->sg_list, wr->num_sge, NULL);
	if (ret)
		return ret;
	*len16 = DIV_ROUND_UP(sizeof wqe->recv +
			      wr->num_sge * sizeof(struct fw_ri_sge), 16);
	return 0;
}

static void ring_kernel_db(struct c4iw_qp *qhp, u32 qid, u16 idx)
{
	struct ibv_modify_qp cmd = {};
	struct ibv_qp_attr attr;
	int mask;
	int __attribute__((unused)) ret;

	/* FIXME: Why do we need this barrier if the kernel is going to
	   trigger the DMA? */
	udma_to_device_barrier();
	if (qid == qhp->wq.sq.qid) {
		attr.sq_psn = idx;
		mask = IBV_QP_SQ_PSN;
	} else  {
		attr.rq_psn = idx;
		mask = IBV_QP_RQ_PSN;
	}
	ret = ibv_cmd_modify_qp(&qhp->ibv_qp, &attr, mask, &cmd, sizeof cmd);
	assert(!ret);
}

static int ibv_to_fw_opcode(int ib_opcode)
{
	int opcode;

	switch (ib_opcode) {
	case IBV_WR_SEND_WITH_INV:
		opcode = FW_RI_SEND_WITH_INV;
		break;
	case IBV_WR_SEND:
		opcode = FW_RI_SEND;
		break;
	case IBV_WR_RDMA_WRITE:
		opcode = FW_RI_RDMA_WRITE;
		break;
	case IBV_WR_RDMA_WRITE_WITH_IMM:
		opcode = FW_RI_WRITE_IMMEDIATE;
		break;
	case IBV_WR_RDMA_READ:
		opcode = FW_RI_READ_REQ;
		break;
	default:
		opcode = -EINVAL;
	}
	return opcode;
}

static int complete_sq_drain_wr(struct c4iw_qp *qhp, struct ibv_send_wr *wr)
{
	struct t4_cqe cqe = {};
	struct c4iw_cq *schp;
	struct t4_cq *cq;
	int opcode;

	schp = to_c4iw_cq(qhp->ibv_qp.send_cq);
	cq = &schp->cq;

	opcode = ibv_to_fw_opcode(wr->opcode);
	if (opcode < 0)
		return opcode;

	PDBG("drain sq id %u\n", qhp->wq.sq.qid);
	cqe.u.drain_cookie = wr->wr_id;
	cqe.header = htobe32(V_CQE_STATUS(T4_ERR_SWFLUSH) |
				 V_CQE_OPCODE(opcode) |
				 V_CQE_TYPE(1) |
				 V_CQE_SWCQE(1) |
				 V_CQE_DRAIN(1) |
				 V_CQE_QPID(qhp->wq.sq.qid));

	pthread_spin_lock(&schp->lock);
	cqe.bits_type_ts = htobe64(V_CQE_GENBIT((u64)cq->gen));
	cq->sw_queue[cq->sw_pidx] = cqe;
	t4_swcq_produce(cq);
	pthread_spin_unlock(&schp->lock);

	t4_clear_cq_armed(&schp->cq);
	return 0;
}

static int complete_sq_drain_wrs(struct c4iw_qp *qhp, struct ibv_send_wr *wr,
				 struct ibv_send_wr **bad_wr)
{
	int ret = 0;

	while (wr) {
		ret = complete_sq_drain_wr(qhp, wr);
		if (ret) {
			*bad_wr = wr;
			break;
		}
		wr = wr->next;
	}
	return ret;
}

static void complete_rq_drain_wr(struct c4iw_qp *qhp, struct ibv_recv_wr *wr)
{
	struct t4_cqe cqe = {};
	struct c4iw_cq *rchp;
	struct t4_cq *cq;

	rchp = to_c4iw_cq(qhp->ibv_qp.recv_cq);
	cq = &rchp->cq;

	PDBG("drain rq id %u\n", qhp->wq.sq.qid);
	cqe.u.drain_cookie = wr->wr_id;
	cqe.header = htobe32(V_CQE_STATUS(T4_ERR_SWFLUSH) |
				 V_CQE_OPCODE(FW_RI_SEND) |
				 V_CQE_TYPE(0) |
				 V_CQE_SWCQE(1) |
				 V_CQE_DRAIN(1) |
				 V_CQE_QPID(qhp->wq.sq.qid));

	pthread_spin_lock(&rchp->lock);
	cqe.bits_type_ts = htobe64(V_CQE_GENBIT((u64)cq->gen));
	cq->sw_queue[cq->sw_pidx] = cqe;
	t4_swcq_produce(cq);
	pthread_spin_unlock(&rchp->lock);

	t4_clear_cq_armed(&rchp->cq);
}

static void complete_rq_drain_wrs(struct c4iw_qp *qhp, struct ibv_recv_wr *wr)
{
	while (wr) {
		complete_rq_drain_wr(qhp, wr);
		wr = wr->next;
	}
}

static void post_write_cmpl(struct c4iw_qp *qhp, struct ibv_send_wr *wr)
{
	bool send_signaled = (wr->next->send_flags & IBV_SEND_SIGNALED) ||
			     qhp->sq_sig_all;
	bool write_signaled = (wr->send_flags & IBV_SEND_SIGNALED) ||
			      qhp->sq_sig_all;
	struct t4_swsqe *swsqe;
	union t4_wr *wqe;
	u16 write_wrid;
	u8 len16;
	u16 idx;

	/*
	 * The sw_sq entries still look like a WRITE and a SEND and consume
	 * 2 slots. The FW WR, however, will be a single uber-WR.
	 */
	wqe = (union t4_wr *)((u8 *)qhp->wq.sq.queue +
	      qhp->wq.sq.wq_pidx * T4_EQ_ENTRY_SIZE);
	build_rdma_write_cmpl(&qhp->wq.sq, &wqe->write_cmpl, wr, &len16);

	/* WRITE swsqe */
	swsqe = &qhp->wq.sq.sw_sq[qhp->wq.sq.pidx];
	swsqe->opcode = FW_RI_RDMA_WRITE;
	swsqe->idx = qhp->wq.sq.pidx;
	swsqe->complete = 0;
	swsqe->signaled = write_signaled;
	swsqe->flushed = 0;
	swsqe->wr_id = wr->wr_id;

	write_wrid = qhp->wq.sq.pidx;

	/* just bump the sw_sq */
	qhp->wq.sq.in_use++;
	if (++qhp->wq.sq.pidx == qhp->wq.sq.size)
		qhp->wq.sq.pidx = 0;

	/* SEND swsqe */
	swsqe = &qhp->wq.sq.sw_sq[qhp->wq.sq.pidx];
	if (wr->next->opcode == IBV_WR_SEND)
		swsqe->opcode = FW_RI_SEND;
	else
		swsqe->opcode = FW_RI_SEND_WITH_INV;
	swsqe->idx = qhp->wq.sq.pidx;
	swsqe->complete = 0;
	swsqe->signaled = send_signaled;
	swsqe->flushed = 0;
	swsqe->wr_id = wr->next->wr_id;

	wqe->write_cmpl.flags_send = send_signaled ? FW_RI_COMPLETION_FLAG : 0;
	wqe->write_cmpl.wrid_send = qhp->wq.sq.pidx;

	init_wr_hdr(wqe, write_wrid, FW_RI_RDMA_WRITE_CMPL_WR,
		    write_signaled ? FW_RI_COMPLETION_FLAG : 0, len16);
	t4_sq_produce(&qhp->wq, len16);
	idx = DIV_ROUND_UP(len16*16, T4_EQ_ENTRY_SIZE);

	t4_ring_sq_db(&qhp->wq, idx, dev_is_t4(qhp->rhp),
		      len16, wqe);
	return;
}

int c4iw_post_send(struct ibv_qp *ibqp, struct ibv_send_wr *wr,
	           struct ibv_send_wr **bad_wr)
{
	int err = 0;
	u8 len16 = 0;
	enum fw_wr_opcodes fw_opcode;
	enum fw_ri_wr_flags fw_flags;
	struct c4iw_qp *qhp;
	union t4_wr *wqe, lwqe;
	u32 num_wrs;
	struct t4_swsqe *swsqe;
	u16 idx = 0;

	qhp = to_c4iw_qp(ibqp);
	pthread_spin_lock(&qhp->lock);

	/*
	 * If the qp has been flushed, then just insert a special
	 * drain cqe.
	 */
	if (qhp->wq.flushed) {
		pthread_spin_unlock(&qhp->lock);
		err = complete_sq_drain_wrs(qhp, wr, bad_wr);
		return err;
	}
	num_wrs = t4_sq_avail(&qhp->wq);
	if (num_wrs == 0) {
		pthread_spin_unlock(&qhp->lock);
		*bad_wr = wr;
		return -ENOMEM;
	}

	/*
	 * Fastpath for NVMe-oF target WRITE + SEND_WITH_INV wr chain which is
	 * the response for small NVMEe-oF READ requests.  If the chain is
	 * exactly a WRITE->SEND_WITH_INV or a WRITE->SEND and the sgl depths 
	 * and lengths meet the requirements of the fw_ri_write_cmpl_wr work 
	 * request, then build and post the write_cmpl WR.  If any of the tests
	 * below are not true, then we continue on with the tradtional WRITE
	 * and SEND WRs.
	 */
	if (qhp->rhp->write_cmpl_supported &&
	    qhp->rhp->chip_version >= CHELSIO_T5 &&
	    wr && wr->next && !wr->next->next &&
	    wr->opcode == IBV_WR_RDMA_WRITE && wr->sg_list[0].length &&
	    wr->num_sge <= T4_WRITE_CMPL_MAX_SGL &&
	    (wr->next->opcode == IBV_WR_SEND_WITH_INV ||
	    wr->next->opcode == IBV_WR_SEND) &&
	    wr->next->sg_list[0].length == T4_WRITE_CMPL_MAX_CQE &&
	    wr->next->num_sge == 1 && num_wrs >= 2) {
		post_write_cmpl(qhp, wr);
		pthread_spin_unlock(&qhp->lock);
		return 0;
	}

	while (wr) {
		if (num_wrs == 0) {
			err = -ENOMEM;
			*bad_wr = wr;
			break;
		}

		wqe = &lwqe;
		fw_flags = 0;
		if (wr->send_flags & IBV_SEND_SOLICITED)
			fw_flags |= FW_RI_SOLICITED_EVENT_FLAG;
		if (wr->send_flags & IBV_SEND_SIGNALED || qhp->sq_sig_all)
			fw_flags |= FW_RI_COMPLETION_FLAG;
		swsqe = &qhp->wq.sq.sw_sq[qhp->wq.sq.pidx];
		switch (wr->opcode) {
		case IBV_WR_SEND_WITH_INV:
		case IBV_WR_SEND:
			INC_STAT(send);
			if (wr->send_flags & IBV_SEND_FENCE)
				fw_flags |= FW_RI_READ_FENCE_FLAG;
			fw_opcode = FW_RI_SEND_WR;
			if (wr->opcode == IBV_WR_SEND)
				swsqe->opcode = FW_RI_SEND;
			else
				swsqe->opcode = FW_RI_SEND_WITH_INV;
			err = build_rdma_send(&qhp->wq.sq, wqe, wr, &len16);
			break;
		case IBV_WR_RDMA_WRITE_WITH_IMM:
			if (unlikely(!(qhp->wq.sq.flags & T4_SQ_WRITE_W_IMM))) {
				err = -ENOSYS;
				break;
			}
			fw_flags |= FW_RI_RDMA_WRITE_WITH_IMMEDIATE;
			/*FALLTHROUGH*/
		case IBV_WR_RDMA_WRITE:
			INC_STAT(write);
			fw_opcode = FW_RI_RDMA_WRITE_WR;
			swsqe->opcode = FW_RI_RDMA_WRITE;
			err = build_rdma_write(&qhp->wq.sq, wqe, wr, &len16);
			break;
		case IBV_WR_RDMA_READ:
			INC_STAT(read);
			fw_opcode = FW_RI_RDMA_READ_WR;
			swsqe->opcode = FW_RI_READ_REQ;
			fw_flags = 0;
			err = build_rdma_read(wqe, wr, &len16);
			if (err)
				break;
			swsqe->read_len = wr->sg_list ? wr->sg_list[0].length :
					  0;
			if (!qhp->wq.sq.oldest_read)
				qhp->wq.sq.oldest_read = swsqe;
			break;
		default:
			PDBG("%s post of type=%d TBD!\n", __func__,
			     wr->opcode);
			err = -EINVAL;
		}
		if (err) {
			*bad_wr = wr;
			break;
		}
		swsqe->idx = qhp->wq.sq.pidx;
		swsqe->complete = 0;
		swsqe->signaled = (wr->send_flags & IBV_SEND_SIGNALED) ||
				  qhp->sq_sig_all;
		swsqe->flushed = 0;
		swsqe->wr_id = wr->wr_id;

		init_wr_hdr(wqe, qhp->wq.sq.pidx, fw_opcode, fw_flags, len16);
		PDBG("%s cookie 0x%llx pidx 0x%x opcode 0x%x\n",
		     __func__, (unsigned long long)wr->wr_id, qhp->wq.sq.pidx,
		     swsqe->opcode);
		wr = wr->next;
		num_wrs--;
		copy_wr_to_sq(&qhp->wq, wqe, len16);
		t4_sq_produce(&qhp->wq, len16);
		idx += DIV_ROUND_UP(len16*16, T4_EQ_ENTRY_SIZE);
	}
	if (t4_wq_db_enabled(&qhp->wq)) {
		t4_ring_sq_db(&qhp->wq, idx, dev_is_t4(qhp->rhp),
			      len16, wqe);
	} else
		ring_kernel_db(qhp, qhp->wq.sq.qid, idx);
	/* This write is only for debugging, the value does not matter for DMA
	 */
	qhp->wq.sq.queue[qhp->wq.sq.size].status.host_wq_pidx = \
			(qhp->wq.sq.wq_pidx);

	pthread_spin_unlock(&qhp->lock);
	return err;
}

int c4iw_post_receive(struct ibv_qp *ibqp, struct ibv_recv_wr *wr,
			   struct ibv_recv_wr **bad_wr)
{
	int err = 0;
	struct c4iw_qp *qhp;
	union t4_recv_wr *wqe, lwqe;
	u32 num_wrs;
	u8 len16 = 0;
	u16 idx = 0;

	qhp = to_c4iw_qp(ibqp);
	pthread_spin_lock(&qhp->lock);

	/*
	 * If the qp has been flushed, then just insert a special
	 * drain cqe.
	 */
	if (qhp->wq.flushed) {
		pthread_spin_unlock(&qhp->lock);
		complete_rq_drain_wrs(qhp, wr);
		return err;
	}
	INC_STAT(recv);
	num_wrs = t4_rq_avail(&qhp->wq);
	if (num_wrs == 0) {
		pthread_spin_unlock(&qhp->lock);
		*bad_wr = wr;
		return -ENOMEM;
	}
	while (wr) {
		if (wr->num_sge > T4_MAX_RECV_SGE) {
			err = -EINVAL;
			*bad_wr = wr;
			break;
		}
		wqe = &lwqe;
		if (num_wrs)
			err = build_rdma_recv(&qhp->wq.rq, wqe, wr, &len16);
		else
			err = -ENOMEM;
		if (err) {
			*bad_wr = wr;
			break;
		}

		qhp->wq.rq.sw_rq[qhp->wq.rq.pidx].wr_id = wr->wr_id;

		wqe->recv.opcode = FW_RI_RECV_WR;
		wqe->recv.r1 = 0;
		wqe->recv.wrid = qhp->wq.rq.pidx;
		wqe->recv.r2[0] = 0;
		wqe->recv.r2[1] = 0;
		wqe->recv.r2[2] = 0;
		wqe->recv.len16 = len16;
		PDBG("%s cookie 0x%llx pidx %u\n", __func__,
		     (unsigned long long) wr->wr_id, qhp->wq.rq.pidx);
		copy_wr_to_rq(&qhp->wq, wqe, len16);
		t4_rq_produce(&qhp->wq, len16);
		idx += DIV_ROUND_UP(len16*16, T4_EQ_ENTRY_SIZE);
		wr = wr->next;
		num_wrs--;
	}
	if (t4_wq_db_enabled(&qhp->wq))
		t4_ring_rq_db(&qhp->wq, idx, dev_is_t4(qhp->rhp),
			      len16, wqe);
	else
		ring_kernel_db(qhp, qhp->wq.rq.qid, idx);
	qhp->wq.rq.queue[qhp->wq.rq.size].status.host_wq_pidx = \
			(qhp->wq.rq.wq_pidx);
	pthread_spin_unlock(&qhp->lock);
	return err;
}

/*
 * Assumes qhp lock is held.
 */
void c4iw_flush_qp(struct c4iw_qp *qhp)
{
	struct c4iw_cq *rchp, *schp;
	int count;

	if (qhp->wq.flushed)
		return;

	qhp->ibv_qp.state = IBV_QPS_ERR;

	rchp = to_c4iw_cq(qhp->ibv_qp.recv_cq);
	schp = to_c4iw_cq(qhp->ibv_qp.send_cq);

	PDBG("%s qhp %p rchp %p schp %p\n", __func__, qhp, rchp, schp);
	qhp->wq.flushed = 1;
	pthread_spin_unlock(&qhp->lock);

	/* locking heirarchy: cq lock first, then qp lock. */
	pthread_spin_lock(&rchp->lock);
	pthread_spin_lock(&qhp->lock);
	c4iw_flush_hw_cq(rchp);
	c4iw_count_rcqes(&rchp->cq, &qhp->wq, &count);
	c4iw_flush_rq(&qhp->wq, &rchp->cq, count);
	pthread_spin_unlock(&qhp->lock);
	pthread_spin_unlock(&rchp->lock);

	/* locking heirarchy: cq lock first, then qp lock. */
	pthread_spin_lock(&schp->lock);
	pthread_spin_lock(&qhp->lock);
	if (schp != rchp)
		c4iw_flush_hw_cq(schp);
	c4iw_flush_sq(qhp);
	pthread_spin_unlock(&qhp->lock);
	pthread_spin_unlock(&schp->lock);
	pthread_spin_lock(&qhp->lock);
}

void c4iw_flush_qps(struct c4iw_dev *dev)
{
	int i;

	pthread_spin_lock(&dev->lock);
	for (i=0; i < dev->max_qp; i++) {
		struct c4iw_qp *qhp = dev->qpid2ptr[i];
		if (qhp) {
			if (!qhp->wq.flushed && t4_wq_in_error(&qhp->wq)) {
				pthread_spin_lock(&qhp->lock);
				c4iw_flush_qp(qhp);
				pthread_spin_unlock(&qhp->lock);
			}
		}
	}
	pthread_spin_unlock(&dev->lock);
}
