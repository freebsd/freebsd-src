/*
 * Copyright (c) 2015-2024, Broadcom. All rights reserved.  The term
 * Broadcom refers to Broadcom Limited and/or its subsidiaries.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 * IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Description: Fast Path Operators
 */

#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/pci.h>
#include <linux/delay.h>
#include <linux/if_ether.h>
#include <linux/hardirq.h>
#include <rdma/ib_mad.h>

#include "hsi_struct_def.h"
#include "qplib_tlv.h"
#include "qplib_res.h"
#include "qplib_rcfw.h"
#include "qplib_sp.h"
#include "qplib_fp.h"
#include "ib_verbs.h"

static void __clean_cq(struct bnxt_qplib_cq *cq, u64 qp);

static void bnxt_re_legacy_cancel_phantom_processing(struct bnxt_qplib_qp *qp)
{
	qp->sq.condition = false;
	qp->sq.legacy_send_phantom = false;
	qp->sq.single = false;
}

static void __bnxt_qplib_add_flush_qp(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_cq *scq, *rcq;

	scq = qp->scq;
	rcq = qp->rcq;

	if (!qp->sq.flushed) {
		dev_dbg(&scq->hwq.pdev->dev,
			"QPLIB: FP: Adding to SQ Flush list = %p\n",
			qp);
		bnxt_re_legacy_cancel_phantom_processing(qp);
		list_add_tail(&qp->sq_flush, &scq->sqf_head);
		qp->sq.flushed = true;
	}
	if (!qp->srq) {
		if (!qp->rq.flushed) {
			dev_dbg(&rcq->hwq.pdev->dev,
				"QPLIB: FP: Adding to RQ Flush list = %p\n",
				qp);
			list_add_tail(&qp->rq_flush, &rcq->rqf_head);
			qp->rq.flushed = true;
		}
	}
}

static void bnxt_qplib_acquire_cq_flush_locks(struct bnxt_qplib_qp *qp)
	__acquires(&qp->scq->flush_lock) __acquires(&qp->rcq->flush_lock)
{
	/* Interrupts are already disabled in calling functions */
	spin_lock(&qp->scq->flush_lock);
	if (qp->scq == qp->rcq)
		__acquire(&qp->rcq->flush_lock);
	else
		spin_lock(&qp->rcq->flush_lock);
}

static void bnxt_qplib_release_cq_flush_locks(struct bnxt_qplib_qp *qp)
	__releases(&qp->scq->flush_lock) __releases(&qp->rcq->flush_lock)
{
	if (qp->scq == qp->rcq)
		__release(&qp->rcq->flush_lock);
	else
		spin_unlock(&qp->rcq->flush_lock);
	spin_unlock(&qp->scq->flush_lock);
}

void bnxt_qplib_add_flush_qp(struct bnxt_qplib_qp *qp)
{

	bnxt_qplib_acquire_cq_flush_locks(qp);
	__bnxt_qplib_add_flush_qp(qp);
	bnxt_qplib_release_cq_flush_locks(qp);
}

static void __bnxt_qplib_del_flush_qp(struct bnxt_qplib_qp *qp)
{
	if (qp->sq.flushed) {
		qp->sq.flushed = false;
		list_del(&qp->sq_flush);
	}
	if (!qp->srq) {
		if (qp->rq.flushed) {
			qp->rq.flushed = false;
			list_del(&qp->rq_flush);
		}
	}
}

void bnxt_qplib_clean_qp(struct bnxt_qplib_qp *qp)
{

	bnxt_qplib_acquire_cq_flush_locks(qp);
	__clean_cq(qp->scq, (u64)(unsigned long)qp);
	qp->sq.hwq.prod = 0;
	qp->sq.hwq.cons = 0;
	qp->sq.swq_start = 0;
	qp->sq.swq_last = 0;
	__clean_cq(qp->rcq, (u64)(unsigned long)qp);
	qp->rq.hwq.prod = 0;
	qp->rq.hwq.cons = 0;
	qp->rq.swq_start = 0;
	qp->rq.swq_last = 0;

	__bnxt_qplib_del_flush_qp(qp);
	bnxt_qplib_release_cq_flush_locks(qp);
}

static void bnxt_qpn_cqn_sched_task(struct work_struct *work)
{
	struct bnxt_qplib_nq_work *nq_work =
			container_of(work, struct bnxt_qplib_nq_work, work);

	struct bnxt_qplib_cq *cq = nq_work->cq;
	struct bnxt_qplib_nq *nq = nq_work->nq;

	if (cq && nq) {
		spin_lock_bh(&cq->compl_lock);
		if (nq->cqn_handler) {
			dev_dbg(&nq->res->pdev->dev,
				"%s:Trigger cq  = %p event nq = %p\n",
				__func__, cq, nq);
			nq->cqn_handler(nq, cq);
		}
		spin_unlock_bh(&cq->compl_lock);
	}
	kfree(nq_work);
}

static void bnxt_qplib_put_hdr_buf(struct pci_dev *pdev,
				   struct bnxt_qplib_hdrbuf *buf)
{
	dma_free_coherent(&pdev->dev, buf->len, buf->va, buf->dma_map);
	kfree(buf);
}

static void *bnxt_qplib_get_hdr_buf(struct pci_dev *pdev,  u32 step, u32 cnt)
{
	struct bnxt_qplib_hdrbuf *hdrbuf;
	u32 len;

	hdrbuf = kmalloc(sizeof(*hdrbuf), GFP_KERNEL);
	if (!hdrbuf)
		return NULL;

	len = ALIGN((step * cnt), PAGE_SIZE);
	hdrbuf->va = dma_alloc_coherent(&pdev->dev, len,
					&hdrbuf->dma_map, GFP_KERNEL);
	if (!hdrbuf->va)
		goto out;

	hdrbuf->len = len;
	hdrbuf->step = step;
	return hdrbuf;
out:
	kfree(hdrbuf);
	return NULL;
}

void bnxt_qplib_free_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp)
{
	if (qp->rq_hdr_buf) {
		bnxt_qplib_put_hdr_buf(res->pdev, qp->rq_hdr_buf);
		qp->rq_hdr_buf = NULL;
	}

	if (qp->sq_hdr_buf) {
		bnxt_qplib_put_hdr_buf(res->pdev, qp->sq_hdr_buf);
		qp->sq_hdr_buf = NULL;
	}
}

int bnxt_qplib_alloc_hdr_buf(struct bnxt_qplib_res *res,
			     struct bnxt_qplib_qp *qp, u32 sstep, u32 rstep)
{
	struct pci_dev *pdev;
	int rc = 0;

	pdev = res->pdev;
	if (sstep) {
		qp->sq_hdr_buf = bnxt_qplib_get_hdr_buf(pdev, sstep,
							qp->sq.max_wqe);
		if (!qp->sq_hdr_buf) {
			dev_err(&pdev->dev, "QPLIB: Failed to get sq_hdr_buf\n");
			return -ENOMEM;
		}
	}

	if (rstep) {
		qp->rq_hdr_buf = bnxt_qplib_get_hdr_buf(pdev, rstep,
							qp->rq.max_wqe);
		if (!qp->rq_hdr_buf) {
			rc = -ENOMEM;
			dev_err(&pdev->dev, "QPLIB: Failed to get rq_hdr_buf\n");
			goto fail;
		}
	}

	return 0;
fail:
	bnxt_qplib_free_hdr_buf(res, qp);
	return rc;
}

/*
 * clean_nq -	Invalidate cqe from given nq.
 * @cq      -	Completion queue
 *
 * Traverse whole notification queue and invalidate any completion
 * associated cq handler provided by caller.
 * Note - This function traverse the hardware queue but do not update
 * consumer index. Invalidated cqe(marked from this function) will be
 * ignored from actual completion of notification queue.
 */
static void clean_nq(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_hwq *nq_hwq = NULL;
	struct bnxt_qplib_nq *nq = NULL;
	struct nq_base *hw_nqe = NULL;
	struct nq_cn *nqcne = NULL;
	u32 peek_flags, peek_cons;
	u64 q_handle;
	u32 type;
	int i;

	nq = cq->nq;
	nq_hwq = &nq->hwq;

	spin_lock_bh(&nq_hwq->lock);
	peek_flags = nq->nq_db.dbinfo.flags;
	peek_cons = nq_hwq->cons;
	for (i = 0; i < nq_hwq->max_elements; i++) {
		hw_nqe = bnxt_qplib_get_qe(nq_hwq, peek_cons, NULL);
		if (!NQE_CMP_VALID(hw_nqe, peek_flags))
			break;

		/* The valid test of the entry must be done first
		 * before reading any further.
		 */
		dma_rmb();
		type = le16_to_cpu(hw_nqe->info10_type) &
				   NQ_BASE_TYPE_MASK;

		/* Processing only NQ_BASE_TYPE_CQ_NOTIFICATION */
		if (type == NQ_BASE_TYPE_CQ_NOTIFICATION) {
			nqcne = (struct nq_cn *)hw_nqe;

			q_handle = le32_to_cpu(nqcne->cq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqcne->cq_handle_high) << 32;
			if (q_handle == (u64)cq) {
				nqcne->cq_handle_low = 0;
				nqcne->cq_handle_high = 0;
				cq->cnq_events++;
			}
		}
		bnxt_qplib_hwq_incr_cons(nq_hwq->max_elements, &peek_cons,
					 1, &peek_flags);
	}
	spin_unlock_bh(&nq_hwq->lock);
}

/*
 * Wait for receiving all NQEs for this CQ.
 * clean_nq is tried 100 times, each time clean_cq
 * loops upto budget times. budget is based on the
 * number of CQs shared by that NQ. So any NQE from
 * CQ would be already in the NQ.
 */
static void __wait_for_all_nqes(struct bnxt_qplib_cq *cq, u16 cnq_events)
{
	u32 retry_cnt = 100;
	u16 total_events;

	if (!cnq_events) {
		clean_nq(cq);
		return;
	}
	while (retry_cnt--) {
		total_events = cq->cnq_events;

		/* Increment total_events by 1 if any CREQ event received with CQ notification */
		if (cq->is_cq_err_event)
			total_events++;

		if (cnq_events == total_events) {
			dev_dbg(&cq->nq->res->pdev->dev,
				"QPLIB: NQ cleanup - Received all NQ events\n");
			return;
		}
		msleep(1);
		clean_nq(cq);
	}
}

static void bnxt_qplib_service_nq(unsigned long data)
{
	struct bnxt_qplib_nq *nq = (struct bnxt_qplib_nq *)data;
	struct bnxt_qplib_hwq *nq_hwq = &nq->hwq;
	int budget = nq->budget;
	struct bnxt_qplib_res *res;
	struct bnxt_qplib_cq *cq;
	struct pci_dev *pdev;
	struct nq_base *nqe;
	u32 hw_polled = 0;
	u64 q_handle;
	u32 type;

	res = nq->res;
	pdev = res->pdev;

	spin_lock_bh(&nq_hwq->lock);
	/* Service the NQ until empty or budget expired */
	while (budget--) {
		nqe = bnxt_qplib_get_qe(nq_hwq, nq_hwq->cons, NULL);
		if (!NQE_CMP_VALID(nqe, nq->nq_db.dbinfo.flags))
			break;
		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		type = le16_to_cpu(nqe->info10_type) & NQ_BASE_TYPE_MASK;
		switch (type) {
		case NQ_BASE_TYPE_CQ_NOTIFICATION:
		{
			struct nq_cn *nqcne = (struct nq_cn *)nqe;

			q_handle = le32_to_cpu(nqcne->cq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqcne->cq_handle_high) << 32;
			cq = (struct bnxt_qplib_cq *)q_handle;
			if (!cq)
				break;
			cq->toggle = (le16_to_cpu(nqe->info10_type) & NQ_CN_TOGGLE_MASK) >> NQ_CN_TOGGLE_SFT;
			cq->dbinfo.toggle = cq->toggle;
			bnxt_qplib_armen_db(&cq->dbinfo,
					    DBC_DBC_TYPE_CQ_ARMENA);
			spin_lock_bh(&cq->compl_lock);
			atomic_set(&cq->arm_state, 0) ;
			if (!nq->cqn_handler(nq, (cq)))
				nq->stats.num_cqne_processed++;
			else
				dev_warn(&pdev->dev,
					 "QPLIB: cqn - type 0x%x not handled\n",
					 type);
			cq->cnq_events++;
			spin_unlock_bh(&cq->compl_lock);
			break;
		}
		case NQ_BASE_TYPE_SRQ_EVENT:
		{
			struct bnxt_qplib_srq *srq;
			struct nq_srq_event *nqsrqe =
						(struct nq_srq_event *)nqe;

			q_handle = le32_to_cpu(nqsrqe->srq_handle_low);
			q_handle |= (u64)le32_to_cpu(nqsrqe->srq_handle_high) << 32;
			srq = (struct bnxt_qplib_srq *)q_handle;
			bnxt_qplib_armen_db(&srq->dbinfo,
					    DBC_DBC_TYPE_SRQ_ARMENA);
			if (!nq->srqn_handler(nq,
					      (struct bnxt_qplib_srq *)q_handle,
					      nqsrqe->event))
				nq->stats.num_srqne_processed++;
			else
				dev_warn(&pdev->dev,
					 "QPLIB: SRQ event 0x%x not handled\n",
					 nqsrqe->event);
			break;
		}
		default:
			dev_warn(&pdev->dev,
				 "QPLIB: nqe with opcode = 0x%x not handled\n",
				 type);
			break;
		}
		hw_polled++;
		bnxt_qplib_hwq_incr_cons(nq_hwq->max_elements, &nq_hwq->cons,
					 1, &nq->nq_db.dbinfo.flags);
	}
	nqe = bnxt_qplib_get_qe(nq_hwq, nq_hwq->cons, NULL);
	if (!NQE_CMP_VALID(nqe, nq->nq_db.dbinfo.flags)) {
		nq->stats.num_nq_rearm++;
		bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, true);
	} else if (nq->requested) {
		bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, true);
		nq->stats.num_tasklet_resched++;
	}
	dev_dbg(&pdev->dev, "QPLIB: cqn/srqn/dbqn \n");
	if (hw_polled >= 0)
		dev_dbg(&pdev->dev,
			"QPLIB: serviced %llu/%llu/%llu budget 0x%x reaped 0x%x\n",
			nq->stats.num_cqne_processed, nq->stats.num_srqne_processed,
			nq->stats.num_dbqne_processed, budget, hw_polled);
	dev_dbg(&pdev->dev,
		"QPLIB: resched_cnt  = %llu arm_count = %llu\n",
		nq->stats.num_tasklet_resched, nq->stats.num_nq_rearm);
	spin_unlock_bh(&nq_hwq->lock);
}

static irqreturn_t bnxt_qplib_nq_irq(int irq, void *dev_instance)
{
	struct bnxt_qplib_nq *nq = dev_instance;
	struct bnxt_qplib_hwq *nq_hwq = &nq->hwq;
	u32 sw_cons;

	/* Prefetch the NQ element */
	sw_cons = HWQ_CMP(nq_hwq->cons, nq_hwq);
	if (sw_cons >= 0)
		prefetch(bnxt_qplib_get_qe(nq_hwq, sw_cons, NULL));

	bnxt_qplib_service_nq((unsigned long)nq);

	return IRQ_HANDLED;
}

void bnxt_qplib_nq_stop_irq(struct bnxt_qplib_nq *nq, bool kill)
{
	struct bnxt_qplib_res *res;

	if (!nq->requested)
		return;

	nq->requested = false;
	res = nq->res;
	/* Mask h/w interrupt */
	bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, false);
	/* Sync with last running IRQ handler */
	synchronize_irq(nq->msix_vec);
	free_irq(nq->msix_vec, nq);
	kfree(nq->name);
	nq->name = NULL;
}

void bnxt_qplib_disable_nq(struct bnxt_qplib_nq *nq)
{
	if (nq->cqn_wq) {
		destroy_workqueue(nq->cqn_wq);
		nq->cqn_wq = NULL;
	}
	/* Make sure the HW is stopped! */
	bnxt_qplib_nq_stop_irq(nq, true);

	nq->nq_db.reg.bar_reg = NULL;
	nq->nq_db.db = NULL;

	nq->cqn_handler = NULL;
	nq->srqn_handler = NULL;
	nq->msix_vec = 0;
}

int bnxt_qplib_nq_start_irq(struct bnxt_qplib_nq *nq, int nq_indx,
			    int msix_vector, bool need_init)
{
	struct bnxt_qplib_res *res;
	int rc;

	res = nq->res;
	if (nq->requested)
		return -EFAULT;

	nq->msix_vec = msix_vector;
	nq->name = kasprintf(GFP_KERNEL, "bnxt_re-nq-%d@pci:%s\n",
			     nq_indx, pci_name(res->pdev));
	if (!nq->name)
		return -ENOMEM;
	rc = request_irq(nq->msix_vec, bnxt_qplib_nq_irq, 0, nq->name, nq);
	if (rc) {
		kfree(nq->name);
		nq->name = NULL;
		return rc;
	}
	nq->requested = true;
	bnxt_qplib_ring_nq_db(&nq->nq_db.dbinfo, res->cctx, true);

	return rc;
}

static void bnxt_qplib_map_nq_db(struct bnxt_qplib_nq *nq,  u32 reg_offt)
{
	struct bnxt_qplib_reg_desc *dbreg;
	struct bnxt_qplib_nq_db *nq_db;
	struct bnxt_qplib_res *res;

	nq_db = &nq->nq_db;
	res = nq->res;
	dbreg = &res->dpi_tbl.ucreg;

	nq_db->reg.bar_id = dbreg->bar_id;
	nq_db->reg.bar_base = dbreg->bar_base;
	nq_db->reg.bar_reg = dbreg->bar_reg + reg_offt;
	nq_db->reg.len = _is_chip_gen_p5_p7(res->cctx) ? sizeof(u64) :
						      sizeof(u32);

	nq_db->dbinfo.db = nq_db->reg.bar_reg;
	nq_db->dbinfo.hwq = &nq->hwq;
	nq_db->dbinfo.xid = nq->ring_id;
	nq_db->dbinfo.seed = nq->ring_id;
	nq_db->dbinfo.flags = 0;
	spin_lock_init(&nq_db->dbinfo.lock);
	nq_db->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	nq_db->dbinfo.res = nq->res;

	return;
}

int bnxt_qplib_enable_nq(struct bnxt_qplib_nq *nq, int nq_idx,
			 int msix_vector, int bar_reg_offset,
			 cqn_handler_t cqn_handler,
			 srqn_handler_t srqn_handler)
{
	struct pci_dev *pdev;
	int rc;

	pdev = nq->res->pdev;
	nq->cqn_handler = cqn_handler;
	nq->srqn_handler = srqn_handler;
	nq->load = 0;
	mutex_init(&nq->lock);

	/* Have a task to schedule CQ notifiers in post send case */
	nq->cqn_wq  = create_singlethread_workqueue("bnxt_qplib_nq\n");
	if (!nq->cqn_wq)
		return -ENOMEM;

	bnxt_qplib_map_nq_db(nq, bar_reg_offset);
	rc = bnxt_qplib_nq_start_irq(nq, nq_idx, msix_vector, true);
	if (rc) {
		dev_err(&pdev->dev,
			"QPLIB: Failed to request irq for nq-idx %d\n", nq_idx);
		goto fail;
	}
	dev_dbg(&pdev->dev, "QPLIB: NQ max = 0x%x\n", nq->hwq.max_elements);

	return 0;
fail:
	bnxt_qplib_disable_nq(nq);
	return rc;
}

void bnxt_qplib_free_nq_mem(struct bnxt_qplib_nq *nq)
{
	if (nq->hwq.max_elements) {
		bnxt_qplib_free_hwq(nq->res, &nq->hwq);
		nq->hwq.max_elements = 0;
	}
}

int bnxt_qplib_alloc_nq_mem(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_nq *nq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_sg_info sginfo = {};

	nq->res = res;
	if (!nq->hwq.max_elements ||
	    nq->hwq.max_elements > BNXT_QPLIB_NQE_MAX_CNT)
		nq->hwq.max_elements = BNXT_QPLIB_NQE_MAX_CNT;

	sginfo.pgsize = PAGE_SIZE;
	sginfo.pgshft = PAGE_SHIFT;
	hwq_attr.res = res;
	hwq_attr.sginfo = &sginfo;
	hwq_attr.depth = nq->hwq.max_elements;
	hwq_attr.stride = sizeof(struct nq_base);
	hwq_attr.type = _get_hwq_type(res);
	if (bnxt_qplib_alloc_init_hwq(&nq->hwq, &hwq_attr)) {
		dev_err(&res->pdev->dev, "QPLIB: FP NQ allocation failed\n");
		return -ENOMEM;
	}
	nq->budget = 8;
	return 0;
}

/* SRQ */
static int __qplib_destroy_srq(struct bnxt_qplib_rcfw *rcfw,
			       struct bnxt_qplib_srq *srq)
{
	struct creq_destroy_srq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_srq req = {};
	/* Configure the request */
	req.srq_cid = cpu_to_le32(srq->id);
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_SRQ,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	return bnxt_qplib_rcfw_send_message(rcfw, &msg);
}

int bnxt_qplib_destroy_srq(struct bnxt_qplib_res *res,
			   struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	int rc;

	rc = __qplib_destroy_srq(rcfw, srq);
	if (rc)
		return rc;
	bnxt_qplib_free_hwq(res, &srq->hwq);
	kfree(srq->swq);
	return 0;
}

int bnxt_qplib_create_srq(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_srq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_create_srq req = {};
	u16 pg_sz_lvl = 0;
	u16 srq_size;
	int rc, idx;

	hwq_attr.res = res;
	hwq_attr.sginfo = &srq->sginfo;
	hwq_attr.depth = srq->max_wqe;
	hwq_attr.stride = srq->wqe_size;
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&srq->hwq, &hwq_attr);
	if (rc)
		goto exit;
	/* Configure the request */
	req.dpi = cpu_to_le32(srq->dpi->dpi);
	req.srq_handle = cpu_to_le64((uintptr_t)srq);
	srq_size = min_t(u32, srq->hwq.depth, U16_MAX);
	req.srq_size = cpu_to_le16(srq_size);
	pg_sz_lvl |= (_get_base_pg_size(&srq->hwq) <<
		      CMDQ_CREATE_SRQ_PG_SIZE_SFT);
	pg_sz_lvl |= (srq->hwq.level & CMDQ_CREATE_SRQ_LVL_MASK);
	req.pg_size_lvl = cpu_to_le16(pg_sz_lvl);
	req.pbl = cpu_to_le64(_get_base_addr(&srq->hwq));
	req.pd_id = cpu_to_le32(srq->pd->id);
	req.eventq_id = cpu_to_le16(srq->eventq_hw_ring_id);
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_SRQ,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;
	if (!srq->is_user) {
		srq->swq = kcalloc(srq->hwq.depth, sizeof(*srq->swq),
				   GFP_KERNEL);
		if (!srq->swq)
			goto srq_fail;
		srq->start_idx = 0;
		srq->last_idx = srq->hwq.depth - 1;
		for (idx = 0; idx < srq->hwq.depth; idx++)
			srq->swq[idx].next_idx = idx + 1;
		srq->swq[srq->last_idx].next_idx = -1;
	}

	spin_lock_init(&srq->lock);
	srq->id = le32_to_cpu(resp.xid);
	srq->cctx = res->cctx;
	srq->dbinfo.hwq = &srq->hwq;
	srq->dbinfo.xid = srq->id;
	srq->dbinfo.db = srq->dpi->dbr;
	srq->dbinfo.max_slot = 1;
	srq->dbinfo.priv_db = res->dpi_tbl.priv_db;
	srq->dbinfo.flags = 0;
	spin_lock_init(&srq->dbinfo.lock);
	srq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	srq->dbinfo.shadow_key_arm_ena = BNXT_QPLIB_DBR_KEY_INVALID;
	srq->dbinfo.res = res;
	srq->dbinfo.seed = srq->id;
	if (srq->threshold)
		bnxt_qplib_armen_db(&srq->dbinfo, DBC_DBC_TYPE_SRQ_ARMENA);
	srq->arm_req = false;
	return 0;
srq_fail:
	__qplib_destroy_srq(rcfw, srq);
fail:
	bnxt_qplib_free_hwq(res, &srq->hwq);
exit:
	return rc;
}

int bnxt_qplib_modify_srq(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_hwq *srq_hwq = &srq->hwq;
	u32 avail = 0;

	avail = __bnxt_qplib_get_avail(srq_hwq);
	if (avail <= srq->threshold) {
		srq->arm_req = false;
		bnxt_qplib_srq_arm_db(&srq->dbinfo);
	} else {
		/* Deferred arming */
		srq->arm_req = true;
	}
	return 0;
}

int bnxt_qplib_query_srq(struct bnxt_qplib_res *res,
			 struct bnxt_qplib_srq *srq)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_query_srq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct creq_query_srq_resp_sb *sb;
	struct bnxt_qplib_rcfw_sbuf sbuf;
	struct cmdq_query_srq req = {};
	int rc = 0;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_SRQ,
				 sizeof(req));
	sbuf.size = ALIGN(sizeof(*sb), BNXT_QPLIB_CMDQE_UNITS);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;
	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	req.srq_cid = cpu_to_le32(srq->id);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	/* TODO: What to do with the query? */
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);

	return rc;
}

int bnxt_qplib_post_srq_recv(struct bnxt_qplib_srq *srq,
			     struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_qplib_hwq *srq_hwq = &srq->hwq;
	struct sq_sge *hw_sge;
	struct rq_wqe *srqe;
	int i, rc = 0, next;
	u32 avail;

	spin_lock(&srq_hwq->lock);
	if (srq->start_idx == srq->last_idx) {
		dev_err(&srq_hwq->pdev->dev, "QPLIB: FP: SRQ (0x%x) is full!\n",
			srq->id);
		rc = -EINVAL;
		spin_unlock(&srq_hwq->lock);
		goto done;
	}
	next = srq->start_idx;
	srq->start_idx = srq->swq[next].next_idx;
	spin_unlock(&srq_hwq->lock);

	srqe = bnxt_qplib_get_qe(srq_hwq, srq_hwq->prod, NULL);
	memset(srqe, 0, srq->wqe_size);
	/* Calculate wqe_size and data_len */
	for (i = 0, hw_sge = (struct sq_sge *)srqe->data;
	     i < wqe->num_sge; i++, hw_sge++) {
		hw_sge->va_or_pa = cpu_to_le64(wqe->sg_list[i].addr);
		hw_sge->l_key = cpu_to_le32(wqe->sg_list[i].lkey);
		hw_sge->size = cpu_to_le32(wqe->sg_list[i].size);
	}
	srqe->wqe_type = wqe->type;
	srqe->flags = wqe->flags;
	srqe->wqe_size = wqe->num_sge +
			((offsetof(typeof(*srqe), data) + 15) >> 4);
	if (!wqe->num_sge)
		srqe->wqe_size++;
	srqe->wr_id |= cpu_to_le32((u32)next);
	srq->swq[next].wr_id = wqe->wr_id;
	bnxt_qplib_hwq_incr_prod(&srq->dbinfo, srq_hwq, srq->dbinfo.max_slot);
	/* retaining srq_hwq->cons for this logic actually the lock is only
	 * required to read srq_hwq->cons.
	 */
	spin_lock(&srq_hwq->lock);
	avail = __bnxt_qplib_get_avail(srq_hwq);
	spin_unlock(&srq_hwq->lock);
	/* Ring DB */
	bnxt_qplib_ring_prod_db(&srq->dbinfo, DBC_DBC_TYPE_SRQ);
	if (srq->arm_req && avail <= srq->threshold) {
		srq->arm_req = false;
		bnxt_qplib_srq_arm_db(&srq->dbinfo);
	}
done:
	return rc;
}

/* QP */
static int __qplib_destroy_qp(struct bnxt_qplib_rcfw *rcfw,
			      struct bnxt_qplib_qp *qp)
{
	struct creq_destroy_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_qp req = {};

	req.qp_cid = cpu_to_le32(qp->id);
	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_QP,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	return bnxt_qplib_rcfw_send_message(rcfw, &msg);
}

static int bnxt_qplib_alloc_init_swq(struct bnxt_qplib_q *que)
{
	int rc = 0;
	int indx;

	que->swq = kcalloc(que->max_wqe, sizeof(*que->swq), GFP_KERNEL);
	if (!que->swq) {
		rc = -ENOMEM;
		goto out;
	}

	que->swq_start = 0;
	que->swq_last = que->max_wqe - 1;
	for (indx = 0; indx < que->max_wqe; indx++)
		que->swq[indx].next_idx = indx + 1;
	que->swq[que->swq_last].next_idx = 0; /* Make it circular */
	que->swq_last = 0;
out:
	return rc;
}

static struct bnxt_qplib_swq *bnxt_qplib_get_swqe(struct bnxt_qplib_q *que,
						  u32 *swq_idx)
{
	u32 idx;

	idx = que->swq_start;
	if (swq_idx)
		*swq_idx = idx;
	return &que->swq[idx];
}

static void bnxt_qplib_swq_mod_start(struct bnxt_qplib_q *que, u32 idx)
{
	que->swq_start = que->swq[idx].next_idx;
}

static u32 bnxt_qplib_get_stride(void)
{
	return sizeof(struct sq_sge);
}

static u32 bnxt_qplib_get_depth(struct bnxt_qplib_q *que)
{
	u8 stride;

	stride = bnxt_qplib_get_stride();
	return (que->wqe_size * que->max_wqe) / stride;
}

static u32 _set_sq_size(struct bnxt_qplib_q *que, u8 wqe_mode)
{
	/* For Variable mode supply number of 16B slots */
	return (wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
		que->max_wqe : bnxt_qplib_get_depth(que);
}

static u32 _set_sq_max_slot(u8 wqe_mode)
{
	/* for static mode index divisor is 8 */
	return (wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
		sizeof(struct sq_send) / sizeof(struct sq_sge) : 1;
}

static u32 _set_rq_max_slot(struct bnxt_qplib_q *que)
{
	return (que->wqe_size / sizeof(struct sq_sge));
}

int bnxt_qplib_create_qp1(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_qp1_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct cmdq_create_qp1 req = {};
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flag;
	u8 pg_sz_lvl = 0;
	u32 qp_flags = 0;
	int rc;

	/* General */
	req.type = qp->type;
	req.dpi = cpu_to_le32(qp->dpi->dpi);
	req.qp_handle = cpu_to_le64(qp->qp_handle);
	/* SQ */
	hwq_attr.res = res;
	hwq_attr.sginfo = &sq->sginfo;
	hwq_attr.stride = bnxt_qplib_get_stride();
	hwq_attr.depth = bnxt_qplib_get_depth(sq);
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&sq->hwq, &hwq_attr);
	if (rc)
		goto exit;

	req.sq_size = cpu_to_le32(_set_sq_size(sq, qp->wqe_mode));
	req.sq_pbl = cpu_to_le64(_get_base_addr(&sq->hwq));
	pg_sz_lvl = _get_base_pg_size(&sq->hwq) <<
		    CMDQ_CREATE_QP1_SQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((sq->hwq.level & CMDQ_CREATE_QP1_SQ_LVL_MASK) <<
		       CMDQ_CREATE_QP1_SQ_LVL_SFT);
	req.sq_pg_size_sq_lvl = pg_sz_lvl;
	req.sq_fwo_sq_sge = cpu_to_le16(((0 << CMDQ_CREATE_QP1_SQ_FWO_SFT) &
					 CMDQ_CREATE_QP1_SQ_FWO_MASK) |
					 (sq->max_sge &
					 CMDQ_CREATE_QP1_SQ_SGE_MASK));
	req.scq_cid = cpu_to_le32(qp->scq->id);

	/* RQ */
	if (!qp->srq) {
		hwq_attr.res = res;
		hwq_attr.sginfo = &rq->sginfo;
		hwq_attr.stride = bnxt_qplib_get_stride();
		hwq_attr.depth = bnxt_qplib_get_depth(rq);
		hwq_attr.type = HWQ_TYPE_QUEUE;
		rc = bnxt_qplib_alloc_init_hwq(&rq->hwq, &hwq_attr);
		if (rc)
			goto fail_sq;
		req.rq_size = cpu_to_le32(rq->max_wqe);
		req.rq_pbl = cpu_to_le64(_get_base_addr(&rq->hwq));
		pg_sz_lvl = _get_base_pg_size(&rq->hwq) <<
					      CMDQ_CREATE_QP1_RQ_PG_SIZE_SFT;
		pg_sz_lvl |= ((rq->hwq.level & CMDQ_CREATE_QP1_RQ_LVL_MASK) <<
			      CMDQ_CREATE_QP1_RQ_LVL_SFT);
		req.rq_pg_size_rq_lvl = pg_sz_lvl;
		req.rq_fwo_rq_sge =
			cpu_to_le16(((0 << CMDQ_CREATE_QP1_RQ_FWO_SFT) &
				     CMDQ_CREATE_QP1_RQ_FWO_MASK) |
				     (rq->max_sge &
				     CMDQ_CREATE_QP1_RQ_SGE_MASK));
	} else {
		/* SRQ */
		qp_flags |= CMDQ_CREATE_QP1_QP_FLAGS_SRQ_USED;
		req.srq_cid = cpu_to_le32(qp->srq->id);
	}
	req.rcq_cid = cpu_to_le32(qp->rcq->id);

	qp_flags |= CMDQ_CREATE_QP1_QP_FLAGS_RESERVED_LKEY_ENABLE;
	req.qp_flags = cpu_to_le32(qp_flags);
	req.pd_id = cpu_to_le32(qp->pd->id);

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_QP1,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail_rq;

	rc = bnxt_qplib_alloc_init_swq(sq);
	if (rc)
		goto sq_swq;

	if (!qp->srq) {
		rc = bnxt_qplib_alloc_init_swq(rq);
		if (rc)
			goto rq_swq;
	}

	qp->id = le32_to_cpu(resp.xid);
	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_RESET;
	qp->cctx = res->cctx;
	sq->dbinfo.hwq = &sq->hwq;
	sq->dbinfo.xid = qp->id;
	sq->dbinfo.db = qp->dpi->dbr;
	sq->dbinfo.max_slot = _set_sq_max_slot(qp->wqe_mode);
	sq->dbinfo.flags = 0;
	spin_lock_init(&sq->dbinfo.lock);
	sq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	sq->dbinfo.res = res;
	if (rq->max_wqe) {
		rq->dbinfo.hwq = &rq->hwq;
		rq->dbinfo.xid = qp->id;
		rq->dbinfo.db = qp->dpi->dbr;
		rq->dbinfo.max_slot = _set_rq_max_slot(rq);
		rq->dbinfo.flags = 0;
		spin_lock_init(&rq->dbinfo.lock);
		rq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
		rq->dbinfo.res = res;
	}

	tbl = &res->reftbl.qpref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[tbl->max].xid = qp->id;
	tbl->rec[tbl->max].handle = qp;
	spin_unlock_irqrestore(&tbl->lock, flag);

	return 0;
rq_swq:
	kfree(sq->swq);
sq_swq:
	__qplib_destroy_qp(rcfw, qp);
fail_rq:
	bnxt_qplib_free_hwq(res, &rq->hwq);
fail_sq:
	bnxt_qplib_free_hwq(res, &sq->hwq);
exit:
	return rc;
}

static void bnxt_qplib_init_psn_ptr(struct bnxt_qplib_qp *qp, int size)
{
	struct bnxt_qplib_hwq *sq_hwq;
	struct bnxt_qplib_q *sq;
	u64 fpsne, psn_pg;
	u16 indx_pad = 0;

	sq = &qp->sq;
	sq_hwq = &sq->hwq;
	/* First psn entry */
	fpsne = (u64)bnxt_qplib_get_qe(sq_hwq, sq_hwq->depth, &psn_pg);
	if (!IS_ALIGNED(fpsne, PAGE_SIZE))
		indx_pad = (fpsne & ~PAGE_MASK) / size;
	sq_hwq->pad_pgofft = indx_pad;
	sq_hwq->pad_pg = (u64 *)psn_pg;
	sq_hwq->pad_stride = size;
}

int bnxt_qplib_create_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_sg_info sginfo = {};
	struct creq_create_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct cmdq_create_qp req = {};
	struct bnxt_qplib_reftbl *tbl;
	struct bnxt_qplib_hwq *xrrq;
	int rc, req_size, psn_sz;
	unsigned long flag;
	u8 pg_sz_lvl = 0;
	u32 qp_flags = 0;
	u32 qp_idx;
	u16 nsge;
	u32 sqsz;

	qp->cctx = res->cctx;
	if (res->dattr)
		qp->dev_cap_flags = res->dattr->dev_cap_flags;
	/* General */
	req.type = qp->type;
	req.dpi = cpu_to_le32(qp->dpi->dpi);
	req.qp_handle = cpu_to_le64(qp->qp_handle);

	/* SQ */
	if (qp->type == CMDQ_CREATE_QP_TYPE_RC) {
		psn_sz = _is_chip_gen_p5_p7(qp->cctx) ?
			 sizeof(struct sq_psn_search_ext) :
			 sizeof(struct sq_psn_search);
		if (BNXT_RE_HW_RETX(qp->dev_cap_flags)) {
			psn_sz = sizeof(struct sq_msn_search);
			qp->msn = 0;
		}
	} else {
		psn_sz = 0;
	}

	hwq_attr.res = res;
	hwq_attr.sginfo = &sq->sginfo;
	hwq_attr.stride = bnxt_qplib_get_stride();
	hwq_attr.depth = bnxt_qplib_get_depth(sq);
	hwq_attr.aux_stride = psn_sz;
	hwq_attr.aux_depth = (psn_sz) ?
				 _set_sq_size(sq, qp->wqe_mode) : 0;
	/* Update msn tbl size */
	if (BNXT_RE_HW_RETX(qp->dev_cap_flags) && psn_sz) {
		if (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC)
			hwq_attr.aux_depth = roundup_pow_of_two(_set_sq_size(sq, qp->wqe_mode));
		else
			hwq_attr.aux_depth = roundup_pow_of_two(_set_sq_size(sq, qp->wqe_mode)) / 2;
		qp->msn_tbl_sz = hwq_attr.aux_depth;
		qp->msn = 0;
	}
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&sq->hwq, &hwq_attr);
	if (rc)
		goto exit;

	sqsz = _set_sq_size(sq, qp->wqe_mode);
	/* 0xffff is the max sq size hw limits to */
	if (sqsz > BNXT_QPLIB_MAX_SQSZ) {
		pr_err("QPLIB: FP: QP (0x%x) exceeds sq size %d\n", qp->id, sqsz);
		goto fail_sq;
	}
	req.sq_size = cpu_to_le32(sqsz);
	req.sq_pbl = cpu_to_le64(_get_base_addr(&sq->hwq));
	pg_sz_lvl = _get_base_pg_size(&sq->hwq) <<
		    CMDQ_CREATE_QP_SQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((sq->hwq.level & CMDQ_CREATE_QP_SQ_LVL_MASK) <<
		       CMDQ_CREATE_QP_SQ_LVL_SFT);
	req.sq_pg_size_sq_lvl = pg_sz_lvl;
	req.sq_fwo_sq_sge = cpu_to_le16(((0 << CMDQ_CREATE_QP_SQ_FWO_SFT) &
					 CMDQ_CREATE_QP_SQ_FWO_MASK) |
					 ((BNXT_RE_HW_RETX(qp->dev_cap_flags)) ?
					  BNXT_MSN_TBLE_SGE : sq->max_sge &
					 CMDQ_CREATE_QP_SQ_SGE_MASK));
	req.scq_cid = cpu_to_le32(qp->scq->id);

	/* RQ/SRQ */
	if (!qp->srq) {
		hwq_attr.res = res;
		hwq_attr.sginfo = &rq->sginfo;
		hwq_attr.stride = bnxt_qplib_get_stride();
		hwq_attr.depth = bnxt_qplib_get_depth(rq);
		hwq_attr.aux_stride = 0;
		hwq_attr.aux_depth = 0;
		hwq_attr.type = HWQ_TYPE_QUEUE;
		rc = bnxt_qplib_alloc_init_hwq(&rq->hwq, &hwq_attr);
		if (rc)
			goto fail_sq;
		req.rq_size = cpu_to_le32(rq->max_wqe);
		req.rq_pbl = cpu_to_le64(_get_base_addr(&rq->hwq));
		pg_sz_lvl = _get_base_pg_size(&rq->hwq) <<
				CMDQ_CREATE_QP_RQ_PG_SIZE_SFT;
		pg_sz_lvl |= ((rq->hwq.level & CMDQ_CREATE_QP_RQ_LVL_MASK) <<
				CMDQ_CREATE_QP_RQ_LVL_SFT);
		req.rq_pg_size_rq_lvl = pg_sz_lvl;
		nsge = (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			res->dattr->max_qp_sges : rq->max_sge;
		req.rq_fwo_rq_sge =
			cpu_to_le16(((0 << CMDQ_CREATE_QP_RQ_FWO_SFT) &
				      CMDQ_CREATE_QP_RQ_FWO_MASK) |
				     (nsge & CMDQ_CREATE_QP_RQ_SGE_MASK));
	} else {
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_SRQ_USED;
		req.srq_cid = cpu_to_le32(qp->srq->id);
	}
	req.rcq_cid = cpu_to_le32(qp->rcq->id);

	qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_RESERVED_LKEY_ENABLE;
	qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_FR_PMR_ENABLED;
	if (qp->sig_type)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_FORCE_COMPLETION;
	if (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_VARIABLE)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_VARIABLE_SIZED_WQE_ENABLED;
	if (res->cctx->modes.te_bypass)
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_OPTIMIZED_TRANSMIT_ENABLED;
	if (res->dattr &&
	    bnxt_ext_stats_supported(qp->cctx, res->dattr->dev_cap_flags, res->is_vf))
		qp_flags |= CMDQ_CREATE_QP_QP_FLAGS_EXT_STATS_ENABLED;
	req.qp_flags = cpu_to_le32(qp_flags);

	/* ORRQ and IRRQ */
	if (psn_sz) {
		xrrq = &qp->orrq;
		xrrq->max_elements =
			ORD_LIMIT_TO_ORRQ_SLOTS(qp->max_rd_atomic);
		req_size = xrrq->max_elements *
			   BNXT_QPLIB_MAX_ORRQE_ENTRY_SIZE + PAGE_SIZE - 1;
		req_size &= ~(PAGE_SIZE - 1);
		sginfo.pgsize = req_size;
		sginfo.pgshft = PAGE_SHIFT;

		hwq_attr.res = res;
		hwq_attr.sginfo = &sginfo;
		hwq_attr.depth = xrrq->max_elements;
		hwq_attr.stride = BNXT_QPLIB_MAX_ORRQE_ENTRY_SIZE;
		hwq_attr.aux_stride = 0;
		hwq_attr.aux_depth = 0;
		hwq_attr.type = HWQ_TYPE_CTX;
		rc = bnxt_qplib_alloc_init_hwq(xrrq, &hwq_attr);
		if (rc)
			goto fail_rq;
		req.orrq_addr = cpu_to_le64(_get_base_addr(xrrq));

		xrrq = &qp->irrq;
		xrrq->max_elements = IRD_LIMIT_TO_IRRQ_SLOTS(
						qp->max_dest_rd_atomic);
		req_size = xrrq->max_elements *
			   BNXT_QPLIB_MAX_IRRQE_ENTRY_SIZE + PAGE_SIZE - 1;
		req_size &= ~(PAGE_SIZE - 1);
		sginfo.pgsize = req_size;
		hwq_attr.depth =  xrrq->max_elements;
		hwq_attr.stride = BNXT_QPLIB_MAX_IRRQE_ENTRY_SIZE;
		rc = bnxt_qplib_alloc_init_hwq(xrrq, &hwq_attr);
		if (rc)
			goto fail_orrq;
		req.irrq_addr = cpu_to_le64(_get_base_addr(xrrq));
	}
	req.pd_id = cpu_to_le32(qp->pd->id);

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_QP,
				 sizeof(req));
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;

	if (!qp->is_user) {
		rc = bnxt_qplib_alloc_init_swq(sq);
		if (rc)
			goto swq_sq;
		if (!qp->srq) {
			rc = bnxt_qplib_alloc_init_swq(rq);
			if (rc)
				goto swq_rq;
		}
		if (psn_sz)
			bnxt_qplib_init_psn_ptr(qp, psn_sz);
	}
	qp->id = le32_to_cpu(resp.xid);
	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_RESET;
	INIT_LIST_HEAD(&qp->sq_flush);
	INIT_LIST_HEAD(&qp->rq_flush);

	sq->dbinfo.hwq = &sq->hwq;
	sq->dbinfo.xid = qp->id;
	sq->dbinfo.db = qp->dpi->dbr;
	sq->dbinfo.max_slot = _set_sq_max_slot(qp->wqe_mode);
	sq->dbinfo.flags = 0;
	spin_lock_init(&sq->dbinfo.lock);
	sq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	sq->dbinfo.res = res;
	sq->dbinfo.seed = qp->id;
	if (rq->max_wqe) {
		rq->dbinfo.hwq = &rq->hwq;
		rq->dbinfo.xid = qp->id;
		rq->dbinfo.db = qp->dpi->dbr;
		rq->dbinfo.max_slot = _set_rq_max_slot(rq);
		rq->dbinfo.flags = 0;
		spin_lock_init(&rq->dbinfo.lock);
		rq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
		rq->dbinfo.res = res;
		rq->dbinfo.seed = qp->id;
	}

	tbl = &res->reftbl.qpref;
	qp_idx = map_qp_id_to_tbl_indx(qp->id, tbl);
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[qp_idx].xid = qp->id;
	tbl->rec[qp_idx].handle = qp;
	spin_unlock_irqrestore(&tbl->lock, flag);

	return 0;
swq_rq:
	kfree(sq->swq);
swq_sq:
	__qplib_destroy_qp(rcfw, qp);
fail:
	bnxt_qplib_free_hwq(res, &qp->irrq);
fail_orrq:
	bnxt_qplib_free_hwq(res, &qp->orrq);
fail_rq:
	bnxt_qplib_free_hwq(res, &rq->hwq);
fail_sq:
	bnxt_qplib_free_hwq(res, &sq->hwq);
exit:
	return rc;
}

static void __filter_modify_flags(struct bnxt_qplib_qp *qp)
{
	switch (qp->cur_qp_state) {
	case CMDQ_MODIFY_QP_NEW_STATE_RESET:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_INIT:
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_INIT:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_RTR:
			if (!(qp->modify_flags &
			      CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU)) {
				qp->modify_flags |=
					CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU;
				qp->path_mtu = CMDQ_MODIFY_QP_PATH_MTU_MTU_2048;
			}
			qp->modify_flags &=
				~CMDQ_MODIFY_QP_MODIFY_MASK_VLAN_ID;
			/* Bono FW requires the max_dest_rd_atomic to be >= 1 */
			if (qp->max_dest_rd_atomic < 1)
				qp->max_dest_rd_atomic = 1;
			qp->modify_flags &= ~CMDQ_MODIFY_QP_MODIFY_MASK_SRC_MAC;
			/* Bono FW 20.6.5 requires SGID_INDEX to be configured */
			if (!(qp->modify_flags &
			      CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX)) {
				qp->modify_flags |=
					CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX;
				qp->ah.sgid_index = 0;
			}
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_RTR:
		switch (qp->state) {
		case CMDQ_MODIFY_QP_NEW_STATE_RTS:
			/* Bono FW requires the max_rd_atomic to be >= 1 */
			if (qp->max_rd_atomic < 1)
				qp->max_rd_atomic = 1;
			qp->modify_flags &=
				~(CMDQ_MODIFY_QP_MODIFY_MASK_PKEY |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DGID |
				  CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL |
				  CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX |
				  CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT |
				  CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC |
				  CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU |
				  CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN |
				  CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER |
				  CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC |
				  CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID);
			break;
		default:
			break;
		}
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_RTS:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_SQD:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_SQE:
		break;
	case CMDQ_MODIFY_QP_NEW_STATE_ERR:
		break;
	default:
		break;
	}
}

int bnxt_qplib_modify_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_modify_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_modify_qp req = {};
	bool ppp_requested = false;
	u32 temp32[4];
	u32 bmask;
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_MODIFY_QP,
				 sizeof(req));

	/* Filter out the qp_attr_mask based on the state->new transition */
	__filter_modify_flags(qp);
	bmask = qp->modify_flags;
	req.modify_mask = cpu_to_le32(qp->modify_flags);
	req.qp_cid = cpu_to_le32(qp->id);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_STATE) {
		req.network_type_en_sqd_async_notify_new_state =
				(qp->state & CMDQ_MODIFY_QP_NEW_STATE_MASK) |
				(qp->en_sqd_async_notify == true ?
					CMDQ_MODIFY_QP_EN_SQD_ASYNC_NOTIFY : 0);
		if (__can_request_ppp(qp)) {
			req.path_mtu_pingpong_push_enable =
				CMDQ_MODIFY_QP_PINGPONG_PUSH_ENABLE;
			req.pingpong_push_dpi = qp->ppp.dpi;
			ppp_requested = true;
		}
	}
	req.network_type_en_sqd_async_notify_new_state |= qp->nw_type;

	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_ACCESS) {
		req.access = qp->access;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_PKEY)
		req.pkey = IB_DEFAULT_PKEY_FULL;

	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_QKEY) {
		req.qkey = cpu_to_le32(qp->qkey);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DGID) {
		memcpy(temp32, qp->ah.dgid.data, sizeof(struct bnxt_qplib_gid));
		req.dgid[0] = cpu_to_le32(temp32[0]);
		req.dgid[1] = cpu_to_le32(temp32[1]);
		req.dgid[2] = cpu_to_le32(temp32[2]);
		req.dgid[3] = cpu_to_le32(temp32[3]);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_FLOW_LABEL) {
		req.flow_label = cpu_to_le32(qp->ah.flow_label);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_SGID_INDEX) {
		req.sgid_index = cpu_to_le16(res->sgid_tbl.hw_id[qp->ah.sgid_index]);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_HOP_LIMIT) {
		req.hop_limit = qp->ah.hop_limit;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TRAFFIC_CLASS) {
		req.traffic_class = qp->ah.traffic_class;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DEST_MAC) {
		memcpy(req.dest_mac, qp->ah.dmac, 6);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_PATH_MTU) {
		req.path_mtu_pingpong_push_enable = qp->path_mtu;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TIMEOUT) {
		req.timeout = qp->timeout;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RETRY_CNT) {
		req.retry_cnt = qp->retry_cnt;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RNR_RETRY) {
		req.rnr_retry = qp->rnr_retry;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MIN_RNR_TIMER) {
		req.min_rnr_timer = qp->min_rnr_timer;
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_RQ_PSN) {
		req.rq_psn = cpu_to_le32(qp->rq.psn);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_SQ_PSN) {
		req.sq_psn = cpu_to_le32(qp->sq.psn);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MAX_RD_ATOMIC) {
		req.max_rd_atomic =
			ORD_LIMIT_TO_ORRQ_SLOTS(qp->max_rd_atomic);
	}
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_MAX_DEST_RD_ATOMIC) {
		req.max_dest_rd_atomic =
			IRD_LIMIT_TO_IRRQ_SLOTS(qp->max_dest_rd_atomic);
	}
	req.sq_size = cpu_to_le32(qp->sq.hwq.max_elements);
	req.rq_size = cpu_to_le32(qp->rq.hwq.max_elements);
	req.sq_sge = cpu_to_le16(qp->sq.max_sge);
	req.rq_sge = cpu_to_le16(qp->rq.max_sge);
	req.max_inline_data = cpu_to_le32(qp->max_inline_data);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_DEST_QP_ID)
		req.dest_qp_id = cpu_to_le32(qp->dest_qpn);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_ENABLE_CC)
		req.enable_cc = cpu_to_le16(CMDQ_MODIFY_QP_ENABLE_CC);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TOS_ECN)
		req.tos_dscp_tos_ecn =
			((qp->tos_ecn << CMDQ_MODIFY_QP_TOS_ECN_SFT) &
			 CMDQ_MODIFY_QP_TOS_ECN_MASK);
	if (bmask & CMDQ_MODIFY_QP_MODIFY_MASK_TOS_DSCP)
		req.tos_dscp_tos_ecn |=
			((qp->tos_dscp << CMDQ_MODIFY_QP_TOS_DSCP_SFT) &
			 CMDQ_MODIFY_QP_TOS_DSCP_MASK);
	req.vlan_pcp_vlan_dei_vlan_id = cpu_to_le16(qp->vlan_id);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	msg.qp_state = qp->state;

	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc == -ETIMEDOUT && (qp->state == CMDQ_MODIFY_QP_NEW_STATE_ERR)) {
		qp->cur_qp_state = qp->state;
		return 0;
	} else if (rc) {
		return rc;
	}
	if (qp->state == CMDQ_MODIFY_QP_NEW_STATE_RTR)
		qp->lag_src_mac = be32_to_cpu(resp.lag_src_mac);

	if (ppp_requested)
		qp->ppp.st_idx_en = resp.pingpong_push_state_index_enabled;

	qp->cur_qp_state = qp->state;
	return 0;
}

int bnxt_qplib_query_qp(struct bnxt_qplib_res *res, struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_query_qp_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct bnxt_qplib_rcfw_sbuf sbuf;
	struct creq_query_qp_resp_sb *sb;
	struct cmdq_query_qp req = {};
	u32 temp32[4];
	int i, rc;

	sbuf.size = ALIGN(sizeof(*sb), BNXT_QPLIB_CMDQE_UNITS);
	sbuf.sb = dma_zalloc_coherent(&rcfw->pdev->dev, sbuf.size,
				       &sbuf.dma_addr, GFP_KERNEL);
	if (!sbuf.sb)
		return -ENOMEM;
	sb = sbuf.sb;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_QUERY_QP,
				 sizeof(req));
	req.qp_cid = cpu_to_le32(qp->id);
	req.resp_size = sbuf.size / BNXT_QPLIB_CMDQE_UNITS;
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, &sbuf, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto bail;

	/* Extract the context from the side buffer */
	qp->state = sb->en_sqd_async_notify_state &
			CREQ_QUERY_QP_RESP_SB_STATE_MASK;
	qp->cur_qp_state = qp->state;
	qp->en_sqd_async_notify = sb->en_sqd_async_notify_state &
				  CREQ_QUERY_QP_RESP_SB_EN_SQD_ASYNC_NOTIFY ?
				  true : false;
	qp->access = sb->access;
	qp->pkey_index = le16_to_cpu(sb->pkey);
	qp->qkey = le32_to_cpu(sb->qkey);

	temp32[0] = le32_to_cpu(sb->dgid[0]);
	temp32[1] = le32_to_cpu(sb->dgid[1]);
	temp32[2] = le32_to_cpu(sb->dgid[2]);
	temp32[3] = le32_to_cpu(sb->dgid[3]);
	memcpy(qp->ah.dgid.data, temp32, sizeof(qp->ah.dgid.data));

	qp->ah.flow_label = le32_to_cpu(sb->flow_label);

	qp->ah.sgid_index = 0;
	for (i = 0; i < res->sgid_tbl.max; i++) {
		if (res->sgid_tbl.hw_id[i] == le16_to_cpu(sb->sgid_index)) {
			qp->ah.sgid_index = i;
			break;
		}
	}
	if (i == res->sgid_tbl.max)
		dev_dbg(&res->pdev->dev,
			"QPLIB: SGID not found qp->id = 0x%x sgid_index = 0x%x\n",
			qp->id, le16_to_cpu(sb->sgid_index));

	qp->ah.hop_limit = sb->hop_limit;
	qp->ah.traffic_class = sb->traffic_class;
	memcpy(qp->ah.dmac, sb->dest_mac, ETH_ALEN);
	qp->ah.vlan_id = le16_to_cpu(sb->path_mtu_dest_vlan_id) &
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_MASK >>
				CREQ_QUERY_QP_RESP_SB_VLAN_ID_SFT;
	qp->path_mtu = le16_to_cpu(sb->path_mtu_dest_vlan_id) &
				    CREQ_QUERY_QP_RESP_SB_PATH_MTU_MASK;
	qp->timeout = sb->timeout;
	qp->retry_cnt = sb->retry_cnt;
	qp->rnr_retry = sb->rnr_retry;
	qp->min_rnr_timer = sb->min_rnr_timer;
	qp->rq.psn = le32_to_cpu(sb->rq_psn);
	qp->max_rd_atomic = ORRQ_SLOTS_TO_ORD_LIMIT(sb->max_rd_atomic);
	qp->sq.psn = le32_to_cpu(sb->sq_psn);
	qp->max_dest_rd_atomic =
			IRRQ_SLOTS_TO_IRD_LIMIT(sb->max_dest_rd_atomic);
	qp->sq.max_wqe = qp->sq.hwq.max_elements;
	qp->rq.max_wqe = qp->rq.hwq.max_elements;
	qp->sq.max_sge = le16_to_cpu(sb->sq_sge);
	qp->rq.max_sge = le16_to_cpu(sb->rq_sge);
	qp->max_inline_data = le32_to_cpu(sb->max_inline_data);
	qp->dest_qpn = le32_to_cpu(sb->dest_qp_id);
	memcpy(qp->smac, sb->src_mac, ETH_ALEN);
	qp->vlan_id = le16_to_cpu(sb->vlan_pcp_vlan_dei_vlan_id);
	qp->port_id = le16_to_cpu(sb->port_id);
bail:
	dma_free_coherent(&rcfw->pdev->dev, sbuf.size,
				  sbuf.sb, sbuf.dma_addr);
	return rc;
}


static void __clean_cq(struct bnxt_qplib_cq *cq, u64 qp)
{
	struct bnxt_qplib_hwq *cq_hwq = &cq->hwq;
	u32 peek_flags, peek_cons;
	struct cq_base *hw_cqe;
	int i;

	peek_flags = cq->dbinfo.flags;
	peek_cons = cq_hwq->cons;
	for (i = 0; i < cq_hwq->depth; i++) {
		hw_cqe = bnxt_qplib_get_qe(cq_hwq, peek_cons, NULL);
		if (CQE_CMP_VALID(hw_cqe, peek_flags)) {
			dma_rmb();
			switch (hw_cqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK) {
			case CQ_BASE_CQE_TYPE_REQ:
			case CQ_BASE_CQE_TYPE_TERMINAL:
			{
				struct cq_req *cqe = (struct cq_req *)hw_cqe;

				if (qp == le64_to_cpu(cqe->qp_handle))
					cqe->qp_handle = 0;
				break;
			}
			case CQ_BASE_CQE_TYPE_RES_RC:
			case CQ_BASE_CQE_TYPE_RES_UD:
			case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
			{
				struct cq_res_rc *cqe = (struct cq_res_rc *)hw_cqe;

				if (qp == le64_to_cpu(cqe->qp_handle))
					cqe->qp_handle = 0;
				break;
			}
			default:
				break;
			}
		}
		bnxt_qplib_hwq_incr_cons(cq_hwq->depth, &peek_cons,
					 1, &peek_flags);
	}
}

int bnxt_qplib_destroy_qp(struct bnxt_qplib_res *res,
			  struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flags;
	u32 qp_idx;
	int rc;

	tbl = &res->reftbl.qpref;
	qp_idx = map_qp_id_to_tbl_indx(qp->id, tbl);
	spin_lock_irqsave(&tbl->lock, flags);
	tbl->rec[qp_idx].xid = BNXT_QPLIB_QP_ID_INVALID;
	tbl->rec[qp_idx].handle = NULL;
	spin_unlock_irqrestore(&tbl->lock, flags);

	rc = __qplib_destroy_qp(rcfw, qp);
	if (rc) {
		spin_lock_irqsave(&tbl->lock, flags);
		tbl->rec[qp_idx].xid = qp->id;
		tbl->rec[qp_idx].handle = qp;
		spin_unlock_irqrestore(&tbl->lock, flags);
		return rc;
	}

	return 0;
}

void bnxt_qplib_free_qp_res(struct bnxt_qplib_res *res,
			    struct bnxt_qplib_qp *qp)
{
	if (qp->irrq.max_elements)
		bnxt_qplib_free_hwq(res, &qp->irrq);
	if (qp->orrq.max_elements)
		bnxt_qplib_free_hwq(res, &qp->orrq);

	if (!qp->is_user)
		kfree(qp->rq.swq);
	bnxt_qplib_free_hwq(res, &qp->rq.hwq);

	if (!qp->is_user)
		kfree(qp->sq.swq);
	bnxt_qplib_free_hwq(res, &qp->sq.hwq);
}

void *bnxt_qplib_get_qp1_sq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge)
{
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_hdrbuf *buf;
	u32 sw_prod;

	memset(sge, 0, sizeof(*sge));

	buf = qp->sq_hdr_buf;
	if (buf) {
		sw_prod = sq->swq_start;
		sge->addr = (dma_addr_t)(buf->dma_map + sw_prod * buf->step);
		sge->lkey = 0xFFFFFFFF;
		sge->size = buf->step;
		return buf->va + sw_prod * sge->size;
	}
	return NULL;
}

u32 bnxt_qplib_get_rq_prod_index(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *rq = &qp->rq;

	return rq->swq_start;
}

void *bnxt_qplib_get_qp1_rq_buf(struct bnxt_qplib_qp *qp,
				struct bnxt_qplib_sge *sge)
{
	struct bnxt_qplib_q *rq = &qp->rq;
	struct bnxt_qplib_hdrbuf *buf;
	u32 sw_prod;

	memset(sge, 0, sizeof(*sge));

	buf = qp->rq_hdr_buf;
	if (buf) {
		sw_prod = rq->swq_start;
		sge->addr = (dma_addr_t)(buf->dma_map + sw_prod * buf->step);
		sge->lkey = 0xFFFFFFFF;
		sge->size = buf->step;
		return buf->va + sw_prod * sge->size;
	}
	return NULL;
}

/* Fil the MSN table into the next psn row */
static void bnxt_qplib_fill_msn_search(struct bnxt_qplib_qp *qp,
				       struct bnxt_qplib_swqe *wqe,
				       struct bnxt_qplib_swq *swq)
{
	struct sq_msn_search *msns;
	u32 start_psn, next_psn;
	u16 start_idx;

	msns = (struct sq_msn_search *)swq->psn_search;
	msns->start_idx_next_psn_start_psn = 0;

	start_psn = swq->start_psn;
	next_psn = swq->next_psn;
	start_idx = swq->slot_idx;
	msns->start_idx_next_psn_start_psn |=
		bnxt_re_update_msn_tbl(start_idx, next_psn, start_psn);
	pr_debug("QP_LIB MSN %d START_IDX %u NEXT_PSN %u START_PSN %u\n",
		 qp->msn,
		 (u16)
		 cpu_to_le16(BNXT_RE_MSN_IDX(msns->start_idx_next_psn_start_psn)),
		 (u32)
		 cpu_to_le32(BNXT_RE_MSN_NPSN(msns->start_idx_next_psn_start_psn)),
		 (u32)
		 cpu_to_le32(BNXT_RE_MSN_SPSN(msns->start_idx_next_psn_start_psn)));
	qp->msn++;
	qp->msn %= qp->msn_tbl_sz;
}

static void bnxt_qplib_fill_psn_search(struct bnxt_qplib_qp *qp,
				       struct bnxt_qplib_swqe *wqe,
				       struct bnxt_qplib_swq *swq)
{
	struct sq_psn_search_ext *psns_ext;
	struct sq_psn_search *psns;
	u32 flg_npsn;
	u32 op_spsn;

	if (!swq->psn_search)
		return;

	/* Handle MSN differently on cap flags  */
	if (BNXT_RE_HW_RETX(qp->dev_cap_flags)) {
		bnxt_qplib_fill_msn_search(qp, wqe, swq);
		return;
	}
	psns = (struct sq_psn_search *)swq->psn_search;
	psns_ext = (struct sq_psn_search_ext *)swq->psn_search;

	op_spsn = ((swq->start_psn << SQ_PSN_SEARCH_START_PSN_SFT) &
		   SQ_PSN_SEARCH_START_PSN_MASK);
	op_spsn |= ((wqe->type << SQ_PSN_SEARCH_OPCODE_SFT) &
		    SQ_PSN_SEARCH_OPCODE_MASK);
	flg_npsn = ((swq->next_psn << SQ_PSN_SEARCH_NEXT_PSN_SFT) &
		    SQ_PSN_SEARCH_NEXT_PSN_MASK);

	if (_is_chip_gen_p5_p7(qp->cctx)) {
		psns_ext->opcode_start_psn = cpu_to_le32(op_spsn);
		psns_ext->flags_next_psn = cpu_to_le32(flg_npsn);
		psns_ext->start_slot_idx = cpu_to_le16(swq->slot_idx);
	} else {
		psns->opcode_start_psn = cpu_to_le32(op_spsn);
		psns->flags_next_psn = cpu_to_le32(flg_npsn);
	}
}

static u16 _calc_ilsize(struct bnxt_qplib_swqe *wqe)
{
	u16 size = 0;
	int indx;

	for (indx = 0; indx < wqe->num_sge; indx++)
		size += wqe->sg_list[indx].size;
	return size;
}

static int bnxt_qplib_put_inline(struct bnxt_qplib_qp *qp,
				 struct bnxt_qplib_swqe *wqe,
				 u32 *sw_prod)
{
	struct bnxt_qplib_hwq *sq_hwq;
	int len, t_len, offt = 0;
	int t_cplen = 0, cplen;
	bool pull_dst = true;
	void *il_dst = NULL;
	void *il_src = NULL;
	int indx;

	sq_hwq = &qp->sq.hwq;
	t_len = 0;
	for (indx = 0; indx < wqe->num_sge; indx++) {
		len = wqe->sg_list[indx].size;
		il_src = (void *)wqe->sg_list[indx].addr;
		t_len += len;
		if (t_len > qp->max_inline_data)
			goto bad;
		while (len) {
			if (pull_dst) {
				pull_dst = false;
				il_dst = bnxt_qplib_get_qe(sq_hwq, ((*sw_prod) %
							   sq_hwq->depth), NULL);
				(*sw_prod)++;
				t_cplen = 0;
				offt = 0;
			}
			cplen = min_t(int, len, sizeof(struct sq_sge));
			cplen = min_t(int, cplen,
				      (sizeof(struct sq_sge) - offt));
			memcpy(il_dst, il_src, cplen);
			t_cplen += cplen;
			il_src += cplen;
			il_dst += cplen;
			offt += cplen;
			len -= cplen;
			if (t_cplen == sizeof(struct sq_sge))
				pull_dst = true;
		}
	}

	return t_len;
bad:
	return -ENOMEM;
}

static int bnxt_qplib_put_sges(struct bnxt_qplib_hwq *sq_hwq,
			       struct bnxt_qplib_sge *ssge,
			       u32 nsge, u32 *sw_prod)
{
	struct sq_sge *dsge;
	int indx, len = 0;

	for (indx = 0; indx < nsge; indx++, (*sw_prod)++) {
		dsge = bnxt_qplib_get_qe(sq_hwq, ((*sw_prod) % sq_hwq->depth), NULL);
		dsge->va_or_pa = cpu_to_le64(ssge[indx].addr);
		dsge->l_key = cpu_to_le32(ssge[indx].lkey);
		dsge->size = cpu_to_le32(ssge[indx].size);
		len += ssge[indx].size;
	}
	return len;
}

static u16 _calculate_wqe_byte(struct bnxt_qplib_qp *qp,
			       struct bnxt_qplib_swqe *wqe, u16 *wqe_byte)
{
	u16 wqe_size;
	u32 ilsize;
	u16 nsge;

	nsge = wqe->num_sge;
	if (wqe->flags & BNXT_QPLIB_SWQE_FLAGS_INLINE) {
		ilsize = _calc_ilsize(wqe);
		wqe_size = (ilsize > qp->max_inline_data) ?
			    qp->max_inline_data : ilsize;
		wqe_size = ALIGN(wqe_size, sizeof(struct sq_sge));
	} else {
		wqe_size = nsge * sizeof(struct sq_sge);
	}
	/* Adding sq_send_hdr is a misnomer, for rq also hdr size is same. */
	wqe_size += sizeof(struct sq_send_hdr);
	if (wqe_byte)
		*wqe_byte = wqe_size;
	return wqe_size / sizeof(struct sq_sge);
}

static u16 _translate_q_full_delta(struct bnxt_qplib_q *que, u16 wqe_bytes)
{
	/* For Cu/Wh delta = 128, stride = 16, wqe_bytes = 128
	 * For Gen-p5 B/C mode delta = 0, stride = 16, wqe_bytes = 128.
	 * For Gen-p5 delta = 0, stride = 16, 32 <= wqe_bytes <= 512.
	 * when 8916 is disabled.
	 */
	return (que->q_full_delta * wqe_bytes) / que->hwq.element_size;
}

static void bnxt_qplib_pull_psn_buff(struct bnxt_qplib_qp *qp, struct bnxt_qplib_q *sq,
				     struct bnxt_qplib_swq *swq, bool hw_retx)
{
	struct bnxt_qplib_hwq *sq_hwq;
	u32 pg_num, pg_indx;
	void *buff;
	u32 tail;

	sq_hwq = &sq->hwq;
	if (!sq_hwq->pad_pg)
		return;

	tail = swq->slot_idx / sq->dbinfo.max_slot;
	if (hw_retx)
		tail %= qp->msn_tbl_sz;
	pg_num = (tail + sq_hwq->pad_pgofft) / (PAGE_SIZE / sq_hwq->pad_stride);
	pg_indx = (tail + sq_hwq->pad_pgofft) % (PAGE_SIZE / sq_hwq->pad_stride);
	buff = (void *)(sq_hwq->pad_pg[pg_num] + pg_indx * sq_hwq->pad_stride);
	/* the start ptr for buff is same ie after the SQ */
	swq->psn_search = buff;
}

void bnxt_qplib_post_send_db(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *sq = &qp->sq;

	bnxt_qplib_ring_prod_db(&sq->dbinfo, DBC_DBC_TYPE_SQ);
}

int bnxt_qplib_post_send(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_qplib_nq_work *nq_work = NULL;
	int i, rc = 0, data_len = 0, pkt_num = 0;
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_hwq *sq_hwq;
	struct bnxt_qplib_swq *swq;
	bool sch_handler = false;
	u16 slots_needed;
	void *base_hdr;
	void *ext_hdr;
	__le32 temp32;
	u16 qfd_slots;
	u8 wqe_slots;
	u16 wqe_size;
	u32 sw_prod;
	u32 wqe_idx;

	sq_hwq = &sq->hwq;
	if (qp->state != CMDQ_MODIFY_QP_NEW_STATE_RTS &&
	    qp->state != CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) is in the 0x%x state\n",
			qp->id, qp->state);
		rc = -EINVAL;
		goto done;
	}

	wqe_slots = _calculate_wqe_byte(qp, wqe, &wqe_size);
	slots_needed = (qp->wqe_mode == BNXT_QPLIB_WQE_MODE_STATIC) ?
			sq->dbinfo.max_slot : wqe_slots;
	qfd_slots = _translate_q_full_delta(sq, wqe_size);
	if (bnxt_qplib_queue_full(sq_hwq, (slots_needed + qfd_slots))) {
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) SQ is full!\n", qp->id);
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: prod = %#x cons = %#x qdepth = %#x delta = %#x slots = %#x\n",
			HWQ_CMP(sq_hwq->prod, sq_hwq),
			HWQ_CMP(sq_hwq->cons, sq_hwq),
			sq_hwq->max_elements, qfd_slots, slots_needed);
		dev_err(&sq_hwq->pdev->dev,
			"QPLIB: phantom_wqe_cnt: %d phantom_cqe_cnt: %d\n",
			sq->phantom_wqe_cnt, sq->phantom_cqe_cnt);
		rc = -ENOMEM;
		goto done;
	}

	sw_prod = sq_hwq->prod;
	swq = bnxt_qplib_get_swqe(sq, &wqe_idx);
	swq->slot_idx = sw_prod;
	bnxt_qplib_pull_psn_buff(qp, sq, swq, BNXT_RE_HW_RETX(qp->dev_cap_flags));

	swq->wr_id = wqe->wr_id;
	swq->type = wqe->type;
	swq->flags = wqe->flags;
	swq->slots = slots_needed;
	swq->start_psn = sq->psn & BTH_PSN_MASK;
	if (qp->sig_type || wqe->flags & BNXT_QPLIB_SWQE_FLAGS_SIGNAL_COMP)
		swq->flags |= SQ_SEND_FLAGS_SIGNAL_COMP;

	dev_dbg(&sq_hwq->pdev->dev,
		"QPLIB: FP: QP(0x%x) post SQ wr_id[%d] = 0x%llx\n",
		qp->id, wqe_idx, swq->wr_id);
	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		sch_handler = true;
		dev_dbg(&sq_hwq->pdev->dev,
			"%s Error QP. Scheduling for poll_cq\n", __func__);
		goto queue_err;
	}

	base_hdr = bnxt_qplib_get_qe(sq_hwq, sw_prod, NULL);
	sw_prod++;
	ext_hdr = bnxt_qplib_get_qe(sq_hwq, (sw_prod % sq_hwq->depth), NULL);
	sw_prod++;
	memset(base_hdr, 0, sizeof(struct sq_sge));
	memset(ext_hdr, 0, sizeof(struct sq_sge));

	if (wqe->flags & BNXT_QPLIB_SWQE_FLAGS_INLINE)
		data_len = bnxt_qplib_put_inline(qp, wqe, &sw_prod);
	else
		data_len = bnxt_qplib_put_sges(sq_hwq, wqe->sg_list,
					       wqe->num_sge, &sw_prod);
	if (data_len < 0)
		goto queue_err;
	/* Specifics */
	switch (wqe->type) {
	case BNXT_QPLIB_SWQE_TYPE_SEND:
		if (qp->type == CMDQ_CREATE_QP_TYPE_RAW_ETHERTYPE ||
		    qp->type == CMDQ_CREATE_QP1_TYPE_GSI) {
			/* Assemble info for Raw Ethertype QPs */
			struct sq_send_raweth_qp1_hdr *sqe = base_hdr;
			struct sq_raw_ext_hdr *ext_sqe = ext_hdr;

			sqe->wqe_type = wqe->type;
			sqe->flags = wqe->flags;
			sqe->wqe_size = wqe_slots;
			sqe->cfa_action = cpu_to_le16(wqe->rawqp1.cfa_action);
			sqe->lflags = cpu_to_le16(wqe->rawqp1.lflags);
			sqe->length = cpu_to_le32(data_len);
			ext_sqe->cfa_meta = cpu_to_le32((wqe->rawqp1.cfa_meta &
				SQ_SEND_RAWETH_QP1_CFA_META_VLAN_VID_MASK) <<
				SQ_SEND_RAWETH_QP1_CFA_META_VLAN_VID_SFT);

			dev_dbg(&sq_hwq->pdev->dev,
				"QPLIB: FP: RAW/QP1 Send WQE:\n"
				"\twqe_type = 0x%x\n"
				"\tflags = 0x%x\n"
				"\twqe_size = 0x%x\n"
				"\tlflags = 0x%x\n"
				"\tcfa_action = 0x%x\n"
				"\tlength = 0x%x\n"
				"\tcfa_meta = 0x%x\n",
				sqe->wqe_type, sqe->flags, sqe->wqe_size,
				sqe->lflags, sqe->cfa_action,
				sqe->length, ext_sqe->cfa_meta);
			break;
		}
		fallthrough;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_IMM:
		fallthrough;
	case BNXT_QPLIB_SWQE_TYPE_SEND_WITH_INV:
	{
		struct sq_send_hdr *sqe = base_hdr;
		struct sq_ud_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->wqe_size = wqe_slots;
		sqe->inv_key_or_imm_data = cpu_to_le32(wqe->send.inv_key);
		if (qp->type == CMDQ_CREATE_QP_TYPE_UD ||
		    qp->type == CMDQ_CREATE_QP_TYPE_GSI) {
			sqe->q_key = cpu_to_le32(wqe->send.q_key);
			sqe->length = cpu_to_le32(data_len);
			ext_sqe->dst_qp = cpu_to_le32(
					wqe->send.dst_qp & SQ_SEND_DST_QP_MASK);
			ext_sqe->avid = cpu_to_le32(wqe->send.avid &
						SQ_SEND_AVID_MASK);
			sq->psn = (sq->psn + 1) & BTH_PSN_MASK;
		} else {
			sqe->length = cpu_to_le32(data_len);
			if (qp->mtu)
				pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
			if (!pkt_num)
				pkt_num = 1;
			sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;
		}
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: Send WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\twqe_size = 0x%x\n"
			"\tinv_key/immdata = 0x%x\n"
			"\tq_key = 0x%x\n"
			"\tdst_qp = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tavid = 0x%x\n",
			sqe->wqe_type, sqe->flags, sqe->wqe_size,
			sqe->inv_key_or_imm_data, sqe->q_key, ext_sqe->dst_qp,
			sqe->length, ext_sqe->avid);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_RDMA_WRITE_WITH_IMM:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_RDMA_READ:
	{
		struct sq_rdma_hdr *sqe = base_hdr;
		struct sq_rdma_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->wqe_size = wqe_slots;
		sqe->imm_data = cpu_to_le32(wqe->rdma.inv_key);
		sqe->length = cpu_to_le32((u32)data_len);
		ext_sqe->remote_va = cpu_to_le64(wqe->rdma.remote_va);
		ext_sqe->remote_key = cpu_to_le32(wqe->rdma.r_key);
		if (qp->mtu)
			pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
		if (!pkt_num)
			pkt_num = 1;
		sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;

		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: RDMA WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\twqe_size = 0x%x\n"
			"\timmdata = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tremote_va = 0x%llx\n"
			"\tremote_key = 0x%x\n",
			sqe->wqe_type, sqe->flags, sqe->wqe_size,
			sqe->imm_data, sqe->length, ext_sqe->remote_va,
			ext_sqe->remote_key);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_CMP_AND_SWP:
		/* fall-thru */
	case BNXT_QPLIB_SWQE_TYPE_ATOMIC_FETCH_AND_ADD:
	{
		struct sq_atomic_hdr *sqe = base_hdr;
		struct sq_atomic_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->remote_key = cpu_to_le32(wqe->atomic.r_key);
		sqe->remote_va = cpu_to_le64(wqe->atomic.remote_va);
		ext_sqe->swap_data = cpu_to_le64(wqe->atomic.swap_data);
		ext_sqe->cmp_data = cpu_to_le64(wqe->atomic.cmp_data);
		if (qp->mtu)
			pkt_num = (data_len + qp->mtu - 1) / qp->mtu;
		if (!pkt_num)
			pkt_num = 1;
		sq->psn = (sq->psn + pkt_num) & BTH_PSN_MASK;
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_LOCAL_INV:
	{
		struct sq_localinvalidate_hdr *sqe = base_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->inv_l_key = cpu_to_le32(wqe->local_inv.inv_l_key);

		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: LOCAL INV WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\tinv_l_key = 0x%x\n",
			sqe->wqe_type, sqe->flags, sqe->inv_l_key);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_FAST_REG_MR:
	{
		struct sq_fr_pmr_hdr *sqe = base_hdr;
		struct sq_fr_pmr_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->access_cntl = wqe->frmr.access_cntl |
				   SQ_FR_PMR_ACCESS_CNTL_LOCAL_WRITE;
		sqe->zero_based_page_size_log =
			(wqe->frmr.pg_sz_log & SQ_FR_PMR_PAGE_SIZE_LOG_MASK) <<
			SQ_FR_PMR_PAGE_SIZE_LOG_SFT |
			(wqe->frmr.zero_based == true ? SQ_FR_PMR_ZERO_BASED : 0);
		sqe->l_key = cpu_to_le32(wqe->frmr.l_key);
		/* TODO: OFED only provides length of MR up to 32-bits for FRMR */
		temp32 = cpu_to_le32(wqe->frmr.length);
		memcpy(sqe->length, &temp32, sizeof(wqe->frmr.length));
		sqe->numlevels_pbl_page_size_log =
			((wqe->frmr.pbl_pg_sz_log <<
					SQ_FR_PMR_PBL_PAGE_SIZE_LOG_SFT) &
					SQ_FR_PMR_PBL_PAGE_SIZE_LOG_MASK) |
			((wqe->frmr.levels << SQ_FR_PMR_NUMLEVELS_SFT) &
					SQ_FR_PMR_NUMLEVELS_MASK);
		if (!wqe->frmr.levels && !wqe->frmr.pbl_ptr) {
			ext_sqe->pblptr = cpu_to_le64(wqe->frmr.page_list[0]);
		} else {
			for (i = 0; i < wqe->frmr.page_list_len; i++)
				wqe->frmr.pbl_ptr[i] = cpu_to_le64(
						wqe->frmr.page_list[i] |
						PTU_PTE_VALID);
			ext_sqe->pblptr = cpu_to_le64(wqe->frmr.pbl_dma_ptr);
		}
		ext_sqe->va = cpu_to_le64(wqe->frmr.va);
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: FRMR WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\taccess_cntl = 0x%x\n"
			"\tzero_based_page_size_log = 0x%x\n"
			"\tl_key = 0x%x\n"
			"\tlength = 0x%x\n"
			"\tnumlevels_pbl_page_size_log = 0x%x\n"
			"\tpblptr = 0x%llx\n"
			"\tva = 0x%llx\n",
			sqe->wqe_type, sqe->flags, sqe->access_cntl,
			sqe->zero_based_page_size_log, sqe->l_key,
			*(u32 *)sqe->length, sqe->numlevels_pbl_page_size_log,
			ext_sqe->pblptr, ext_sqe->va);
		break;
	}
	case BNXT_QPLIB_SWQE_TYPE_BIND_MW:
	{
		struct sq_bind_hdr *sqe = base_hdr;
		struct sq_bind_ext_hdr *ext_sqe = ext_hdr;

		sqe->wqe_type = wqe->type;
		sqe->flags = wqe->flags;
		sqe->access_cntl = wqe->bind.access_cntl;
		sqe->mw_type_zero_based = wqe->bind.mw_type |
			(wqe->bind.zero_based == true ? SQ_BIND_ZERO_BASED : 0);
		sqe->parent_l_key = cpu_to_le32(wqe->bind.parent_l_key);
		sqe->l_key = cpu_to_le32(wqe->bind.r_key);
		ext_sqe->va = cpu_to_le64(wqe->bind.va);
		ext_sqe->length_lo = cpu_to_le32(wqe->bind.length);
		dev_dbg(&sq_hwq->pdev->dev,
			"QPLIB: FP: BIND WQE:\n"
			"\twqe_type = 0x%x\n"
			"\tflags = 0x%x\n"
			"\taccess_cntl = 0x%x\n"
			"\tmw_type_zero_based = 0x%x\n"
			"\tparent_l_key = 0x%x\n"
			"\tl_key = 0x%x\n"
			"\tva = 0x%llx\n"
			"\tlength = 0x%x\n",
			sqe->wqe_type, sqe->flags, sqe->access_cntl,
			sqe->mw_type_zero_based, sqe->parent_l_key,
			sqe->l_key, sqe->va, ext_sqe->length_lo);
		break;
	}
	default:
		/* Bad wqe, return error */
		rc = -EINVAL;
		goto done;
	}
	swq->next_psn = sq->psn & BTH_PSN_MASK;
	bnxt_qplib_fill_psn_search(qp, wqe, swq);

queue_err:
	bnxt_qplib_swq_mod_start(sq, wqe_idx);
	bnxt_qplib_hwq_incr_prod(&sq->dbinfo, sq_hwq, swq->slots);
	qp->wqe_cnt++;
done:
	if (sch_handler) {
		nq_work = kzalloc(sizeof(*nq_work), GFP_ATOMIC);
		if (nq_work) {
			nq_work->cq = qp->scq;
			nq_work->nq = qp->scq->nq;
			INIT_WORK(&nq_work->work, bnxt_qpn_cqn_sched_task);
			queue_work(qp->scq->nq->cqn_wq, &nq_work->work);
		} else {
			dev_err(&sq->hwq.pdev->dev,
				"QPLIB: FP: Failed to allocate SQ nq_work!\n");
			rc = -ENOMEM;
		}
	}
	return rc;
}

void bnxt_qplib_post_recv_db(struct bnxt_qplib_qp *qp)
{
	struct bnxt_qplib_q *rq = &qp->rq;

	bnxt_qplib_ring_prod_db(&rq->dbinfo, DBC_DBC_TYPE_RQ);
}

void bnxt_re_handle_cqn(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_nq *nq;

	if (!(cq && cq->nq))
		return;

	nq = cq->nq;
	spin_lock_bh(&cq->compl_lock);
	if (nq->cqn_handler) {
		dev_dbg(&nq->res->pdev->dev,
			"%s:Trigger cq  = %p event nq = %p\n",
			__func__, cq, nq);
		nq->cqn_handler(nq, cq);
	}
	spin_unlock_bh(&cq->compl_lock);
}

int bnxt_qplib_post_recv(struct bnxt_qplib_qp *qp,
			 struct bnxt_qplib_swqe *wqe)
{
	struct bnxt_qplib_nq_work *nq_work = NULL;
	struct bnxt_qplib_q *rq = &qp->rq;
	struct bnxt_qplib_hwq *rq_hwq;
	struct bnxt_qplib_swq *swq;
	bool sch_handler = false;
	struct rq_wqe_hdr *base_hdr;
	struct rq_ext_hdr *ext_hdr;
	struct sq_sge *dsge;
	u8 wqe_slots;
	u32 wqe_idx;
	u32 sw_prod;
	int rc = 0;

	rq_hwq = &rq->hwq;
	if (qp->state == CMDQ_MODIFY_QP_NEW_STATE_RESET) {
		dev_err(&rq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) is in the 0x%x state\n",
			qp->id, qp->state);
		rc = -EINVAL;
		goto done;
	}

	wqe_slots = _calculate_wqe_byte(qp, wqe, NULL);
	if (bnxt_qplib_queue_full(rq_hwq, rq->dbinfo.max_slot)) {
		dev_err(&rq_hwq->pdev->dev,
			"QPLIB: FP: QP (0x%x) RQ is full!\n", qp->id);
		rc = -EINVAL;
		goto done;
	}

	swq = bnxt_qplib_get_swqe(rq, &wqe_idx);
	swq->wr_id = wqe->wr_id;
	swq->slots = rq->dbinfo.max_slot;
	dev_dbg(&rq_hwq->pdev->dev,
		"QPLIB: FP: post RQ wr_id[%d] = 0x%llx\n",
		wqe_idx, swq->wr_id);
	if (qp->cur_qp_state == CMDQ_MODIFY_QP_NEW_STATE_ERR) {
		sch_handler = true;
		dev_dbg(&rq_hwq->pdev->dev, "%s Error QP. Sched a flushed cmpl\n",
			__func__);
		goto queue_err;
	}

	sw_prod = rq_hwq->prod;
	base_hdr = bnxt_qplib_get_qe(rq_hwq, sw_prod, NULL);
	sw_prod++;
	ext_hdr = bnxt_qplib_get_qe(rq_hwq, (sw_prod % rq_hwq->depth), NULL);
	sw_prod++;
	memset(base_hdr, 0, sizeof(struct sq_sge));
	memset(ext_hdr, 0, sizeof(struct sq_sge));

	if (!wqe->num_sge) {
		dsge = bnxt_qplib_get_qe(rq_hwq, (sw_prod % rq_hwq->depth), NULL);
		dsge->size = 0;
		wqe_slots++;
	} else {
		bnxt_qplib_put_sges(rq_hwq, wqe->sg_list, wqe->num_sge, &sw_prod);
	}
	base_hdr->wqe_type = wqe->type;
	base_hdr->flags = wqe->flags;
	base_hdr->wqe_size = wqe_slots;
	base_hdr->wr_id |= cpu_to_le32(wqe_idx);
queue_err:
	bnxt_qplib_swq_mod_start(rq, wqe_idx);
	bnxt_qplib_hwq_incr_prod(&rq->dbinfo, &rq->hwq, swq->slots);
done:
	if (sch_handler) {
		nq_work = kzalloc(sizeof(*nq_work), GFP_ATOMIC);
		if (nq_work) {
			nq_work->cq = qp->rcq;
			nq_work->nq = qp->rcq->nq;
			INIT_WORK(&nq_work->work, bnxt_qpn_cqn_sched_task);
			queue_work(qp->rcq->nq->cqn_wq, &nq_work->work);
		} else {
			dev_err(&rq->hwq.pdev->dev,
				"QPLIB: FP: Failed to allocate RQ nq_work!\n");
			rc = -ENOMEM;
		}
	}
	return rc;
}

/* CQ */
int bnxt_qplib_create_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_create_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_create_cq req = {};
	struct bnxt_qplib_reftbl *tbl;
	unsigned long flag;
	u32 pg_sz_lvl = 0;
	int rc;

	hwq_attr.res = res;
	hwq_attr.depth = cq->max_wqe;
	hwq_attr.stride = sizeof(struct cq_base);
	hwq_attr.type = HWQ_TYPE_QUEUE;
	hwq_attr.sginfo = &cq->sginfo;
	rc = bnxt_qplib_alloc_init_hwq(&cq->hwq, &hwq_attr);
	if (rc)
		goto exit;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_CREATE_CQ,
				 sizeof(req));

	if (!cq->dpi) {
		dev_err(&rcfw->pdev->dev,
			"QPLIB: FP: CREATE_CQ failed due to NULL DPI\n");
		return -EINVAL;
	}
	req.dpi = cpu_to_le32(cq->dpi->dpi);
	req.cq_handle = cpu_to_le64(cq->cq_handle);

	req.cq_size = cpu_to_le32(cq->max_wqe);
	req.pbl = cpu_to_le64(_get_base_addr(&cq->hwq));
	pg_sz_lvl = _get_base_pg_size(&cq->hwq) << CMDQ_CREATE_CQ_PG_SIZE_SFT;
	pg_sz_lvl |= ((cq->hwq.level & CMDQ_CREATE_CQ_LVL_MASK) <<
		       CMDQ_CREATE_CQ_LVL_SFT);
	req.pg_size_lvl = cpu_to_le32(pg_sz_lvl);

	req.cq_fco_cnq_id = cpu_to_le32(
			(cq->cnq_hw_ring_id & CMDQ_CREATE_CQ_CNQ_ID_MASK) <<
			 CMDQ_CREATE_CQ_CNQ_ID_SFT);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;
	cq->id = le32_to_cpu(resp.xid);
	cq->period = BNXT_QPLIB_QUEUE_START_PERIOD;
	init_waitqueue_head(&cq->waitq);
	INIT_LIST_HEAD(&cq->sqf_head);
	INIT_LIST_HEAD(&cq->rqf_head);
	spin_lock_init(&cq->flush_lock);
	spin_lock_init(&cq->compl_lock);

	/* init dbinfo */
	cq->cctx = res->cctx;
	cq->dbinfo.hwq = &cq->hwq;
	cq->dbinfo.xid = cq->id;
	cq->dbinfo.db = cq->dpi->dbr;
	cq->dbinfo.priv_db = res->dpi_tbl.priv_db;
	cq->dbinfo.flags = 0;
	cq->dbinfo.toggle = 0;
	cq->dbinfo.res = res;
	cq->dbinfo.seed = cq->id;
	spin_lock_init(&cq->dbinfo.lock);
	cq->dbinfo.shadow_key = BNXT_QPLIB_DBR_KEY_INVALID;
	cq->dbinfo.shadow_key_arm_ena = BNXT_QPLIB_DBR_KEY_INVALID;

	tbl = &res->reftbl.cqref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].xid = cq->id;
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].handle = cq;
	spin_unlock_irqrestore(&tbl->lock, flag);

	bnxt_qplib_armen_db(&cq->dbinfo, DBC_DBC_TYPE_CQ_ARMENA);
	return 0;

fail:
	bnxt_qplib_free_hwq(res, &cq->hwq);
exit:
	return rc;
}

int bnxt_qplib_modify_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	/* TODO: Modify CQ threshold are passed to the HW via DBR */
	return 0;
}

void bnxt_qplib_resize_cq_complete(struct bnxt_qplib_res *res,
				   struct bnxt_qplib_cq *cq)
{
	bnxt_qplib_free_hwq(res, &cq->hwq);
	memcpy(&cq->hwq, &cq->resize_hwq, sizeof(cq->hwq));
	/* Reset only the cons bit in the flags */
	cq->dbinfo.flags &= ~(1UL << BNXT_QPLIB_FLAG_EPOCH_CONS_SHIFT);

	/* Tell HW to switch over to the new CQ */
	if (!cq->resize_hwq.is_user)
		bnxt_qplib_cq_coffack_db(&cq->dbinfo);
}

int bnxt_qplib_resize_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq,
			 int new_cqes)
{
	struct bnxt_qplib_hwq_attr hwq_attr = {};
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_resize_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_resize_cq req = {};
	u32 pgsz = 0, lvl = 0, nsz = 0;
	struct bnxt_qplib_pbl *pbl;
	u16 count = -1;
	int rc;

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_RESIZE_CQ,
				 sizeof(req));

	hwq_attr.sginfo = &cq->sginfo;
	hwq_attr.res = res;
	hwq_attr.depth = new_cqes;
	hwq_attr.stride = sizeof(struct cq_base);
	hwq_attr.type = HWQ_TYPE_QUEUE;
	rc = bnxt_qplib_alloc_init_hwq(&cq->resize_hwq, &hwq_attr);
	if (rc)
		return rc;

	dev_dbg(&rcfw->pdev->dev, "QPLIB: FP: %s: pbl_lvl: %d\n", __func__,
		cq->resize_hwq.level);
	req.cq_cid = cpu_to_le32(cq->id);
	pbl = &cq->resize_hwq.pbl[PBL_LVL_0];
	pgsz = ((pbl->pg_size == ROCE_PG_SIZE_4K ? CMDQ_RESIZE_CQ_PG_SIZE_PG_4K :
		pbl->pg_size == ROCE_PG_SIZE_8K ? CMDQ_RESIZE_CQ_PG_SIZE_PG_8K :
		pbl->pg_size == ROCE_PG_SIZE_64K ? CMDQ_RESIZE_CQ_PG_SIZE_PG_64K :
		pbl->pg_size == ROCE_PG_SIZE_2M ? CMDQ_RESIZE_CQ_PG_SIZE_PG_2M :
		pbl->pg_size == ROCE_PG_SIZE_8M ? CMDQ_RESIZE_CQ_PG_SIZE_PG_8M :
		pbl->pg_size == ROCE_PG_SIZE_1G ? CMDQ_RESIZE_CQ_PG_SIZE_PG_1G :
		CMDQ_RESIZE_CQ_PG_SIZE_PG_4K) & CMDQ_RESIZE_CQ_PG_SIZE_MASK);
	lvl = (cq->resize_hwq.level << CMDQ_RESIZE_CQ_LVL_SFT) &
				       CMDQ_RESIZE_CQ_LVL_MASK;
	nsz = (new_cqes << CMDQ_RESIZE_CQ_NEW_CQ_SIZE_SFT) &
	       CMDQ_RESIZE_CQ_NEW_CQ_SIZE_MASK;
	req.new_cq_size_pg_size_lvl = cpu_to_le32(nsz|pgsz|lvl);
	req.new_pbl = cpu_to_le64(pbl->pg_map_arr[0]);

	if (!cq->resize_hwq.is_user)
		set_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);

	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		goto fail;

	if (!cq->resize_hwq.is_user) {
wait:
		/* Wait here for the HW to switch the CQ over */
		if (wait_event_interruptible_timeout(cq->waitq,
		    !test_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags),
		    msecs_to_jiffies(CQ_RESIZE_WAIT_TIME_MS)) ==
		    -ERESTARTSYS && count--)
			goto wait;

		if (test_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags)) {
			dev_err(&rcfw->pdev->dev,
				"QPLIB: FP: RESIZE_CQ timed out\n");
			rc = -ETIMEDOUT;
			goto fail;
		}

		bnxt_qplib_resize_cq_complete(res, cq);
	}

	return 0;
fail:
	if (!cq->resize_hwq.is_user) {
		bnxt_qplib_free_hwq(res, &cq->resize_hwq);
		clear_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);
	}
	return rc;
}

void bnxt_qplib_free_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	bnxt_qplib_free_hwq(res, &cq->hwq);
}

static void bnxt_qplib_sync_cq(struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_nq *nq = cq->nq;
	/* Flush any pending work and synchronize irq */
	flush_workqueue(cq->nq->cqn_wq);
	mutex_lock(&nq->lock);
	if (nq->requested)
		synchronize_irq(nq->msix_vec);
	mutex_unlock(&nq->lock);
}

int bnxt_qplib_destroy_cq(struct bnxt_qplib_res *res, struct bnxt_qplib_cq *cq)
{
	struct bnxt_qplib_rcfw *rcfw = res->rcfw;
	struct creq_destroy_cq_resp resp = {};
	struct bnxt_qplib_cmdqmsg msg = {};
	struct cmdq_destroy_cq req = {};
	struct bnxt_qplib_reftbl *tbl;
	u16 total_cnq_events;
	unsigned long flag;
	int rc;

	tbl = &res->reftbl.cqref;
	spin_lock_irqsave(&tbl->lock, flag);
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].handle = NULL;
	tbl->rec[GET_TBL_INDEX(cq->id, tbl)].xid = 0;
	spin_unlock_irqrestore(&tbl->lock, flag);

	bnxt_qplib_rcfw_cmd_prep(&req, CMDQ_BASE_OPCODE_DESTROY_CQ,
				 sizeof(req));

	req.cq_cid = cpu_to_le32(cq->id);
	bnxt_qplib_fill_cmdqmsg(&msg, &req, &resp, NULL, sizeof(req),
				sizeof(resp), 0);
	rc = bnxt_qplib_rcfw_send_message(rcfw, &msg);
	if (rc)
		return rc;

	total_cnq_events = le16_to_cpu(resp.total_cnq_events);
	if (total_cnq_events >= 0)
		dev_dbg(&rcfw->pdev->dev,
			"%s: cq_id = 0x%x cq = 0x%p resp.total_cnq_events = 0x%x\n",
			__func__, cq->id, cq, total_cnq_events);
	__wait_for_all_nqes(cq, total_cnq_events);
	bnxt_qplib_sync_cq(cq);
	bnxt_qplib_free_hwq(res, &cq->hwq);
	return 0;
}

static int __flush_sq(struct bnxt_qplib_q *sq, struct bnxt_qplib_qp *qp,
		      struct bnxt_qplib_cqe **pcqe, int *budget)
{
	struct bnxt_qplib_cqe *cqe;
	u32 start, last;
	int rc = 0;

	/* Now complete all outstanding SQEs with FLUSHED_ERR */
	start = sq->swq_start;
	cqe = *pcqe;
	while (*budget) {
		last = sq->swq_last;
		if (start == last) {
			break;
		}
		/* Skip the FENCE WQE completions */
		if (sq->swq[last].wr_id == BNXT_QPLIB_FENCE_WRID) {
			bnxt_re_legacy_cancel_phantom_processing(qp);
			goto skip_compl;
		}

		memset(cqe, 0, sizeof(*cqe));
		cqe->status = CQ_REQ_STATUS_WORK_REQUEST_FLUSHED_ERR;
		cqe->opcode = CQ_BASE_CQE_TYPE_REQ;
		cqe->qp_handle = (u64)qp;
		cqe->wr_id = sq->swq[last].wr_id;
		cqe->src_qp = qp->id;
		cqe->type = sq->swq[last].type;
		dev_dbg(&sq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed terminal Req \n");
		dev_dbg(&sq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status 0x%x\n",
			last, cqe->wr_id, cqe->status);
		cqe++;
		(*budget)--;
skip_compl:
		bnxt_qplib_hwq_incr_cons(sq->hwq.depth,
					 &sq->hwq.cons,
					 sq->swq[last].slots,
					 &sq->dbinfo.flags);
		sq->swq_last = sq->swq[last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && sq->swq_last != start)
		/* Out of budget */
		rc = -EAGAIN;
	dev_dbg(&sq->hwq.pdev->dev, "QPLIB: FP: Flush SQ rc = 0x%x\n", rc);

	return rc;
}

static int __flush_rq(struct bnxt_qplib_q *rq, struct bnxt_qplib_qp *qp,
		      struct bnxt_qplib_cqe **pcqe, int *budget)
{
	struct bnxt_qplib_cqe *cqe;
	u32 start, last;
	int opcode = 0;
	int rc = 0;

	switch (qp->type) {
	case CMDQ_CREATE_QP1_TYPE_GSI:
		opcode = CQ_BASE_CQE_TYPE_RES_RAWETH_QP1;
		break;
	case CMDQ_CREATE_QP_TYPE_RC:
		opcode = CQ_BASE_CQE_TYPE_RES_RC;
		break;
	case CMDQ_CREATE_QP_TYPE_UD:
		opcode = CQ_BASE_CQE_TYPE_RES_UD;
		break;
	}

	/* Flush the rest of the RQ */
	start = rq->swq_start;
	cqe = *pcqe;
	while (*budget) {
		last = rq->swq_last;
		if (last == start)
			break;
		memset(cqe, 0, sizeof(*cqe));
		cqe->status =
		    CQ_RES_RC_STATUS_WORK_REQUEST_FLUSHED_ERR;
		cqe->opcode = opcode;
		cqe->qp_handle = (u64)qp;
		cqe->wr_id = rq->swq[last].wr_id;
		dev_dbg(&rq->hwq.pdev->dev, "QPLIB: FP: CQ Processed Res RC \n");
		dev_dbg(&rq->hwq.pdev->dev,
			"QPLIB: rq[%d] = 0x%llx with status 0x%x\n",
			last, cqe->wr_id, cqe->status);
		cqe++;
		(*budget)--;
		bnxt_qplib_hwq_incr_cons(rq->hwq.depth,
					 &rq->hwq.cons,
					 rq->swq[last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && rq->swq_last != start)
		/* Out of budget */
		rc = -EAGAIN;

	dev_dbg(&rq->hwq.pdev->dev, "QPLIB: FP: Flush RQ rc = 0x%x\n", rc);
	return rc;
}

void bnxt_qplib_mark_qp_error(void *qp_handle)
{
	struct bnxt_qplib_qp *qp = qp_handle;

	if (!qp)
		return;

	/* Must block new posting of SQ and RQ */
	qp->cur_qp_state = CMDQ_MODIFY_QP_NEW_STATE_ERR;
	qp->state = qp->cur_qp_state;

	/* Add qp to flush list of the CQ */
	if (!qp->is_user)
		bnxt_qplib_add_flush_qp(qp);
}

/* Note: SQE is valid from sw_sq_cons up to cqe_sq_cons (exclusive)
 *       CQE is track from sw_cq_cons to max_element but valid only if VALID=1
 */
static int bnxt_re_legacy_do_wa9060(struct bnxt_qplib_qp *qp,
				 struct bnxt_qplib_cq *cq,
				 u32 cq_cons, u32 swq_last,
				 u32 cqe_sq_cons)
{
	struct bnxt_qplib_q *sq = &qp->sq;
	struct bnxt_qplib_swq *swq;
	u32 peek_sw_cq_cons, peek_sq_cons_idx, peek_flags;
	struct cq_terminal *peek_term_hwcqe;
	struct cq_req *peek_req_hwcqe;
	struct bnxt_qplib_qp *peek_qp;
	struct bnxt_qplib_q *peek_sq;
	struct cq_base *peek_hwcqe;
	int i, rc = 0;

	/* Check for the psn_search marking before completing */
	swq = &sq->swq[swq_last];
	if (swq->psn_search &&
	    le32_to_cpu(swq->psn_search->flags_next_psn) & 0x80000000) {
		/* Unmark */
		swq->psn_search->flags_next_psn = cpu_to_le32
				(le32_to_cpu(swq->psn_search->flags_next_psn)
				 & ~0x80000000);
		dev_dbg(&cq->hwq.pdev->dev,
			"FP: Process Req cq_cons=0x%x qp=0x%x sq cons sw=0x%x cqe=0x%x marked!\n",
			cq_cons, qp->id, swq_last, cqe_sq_cons);
		sq->condition = true;
		sq->legacy_send_phantom = true;

		/* TODO: Only ARM if the previous SQE is ARMALL */
		bnxt_qplib_ring_db(&cq->dbinfo, DBC_DBC_TYPE_CQ_ARMALL);

		rc = -EAGAIN;
		goto out;
	}
	if (sq->condition == true) {
		/* Peek at the completions */
		peek_flags = cq->dbinfo.flags;
		peek_sw_cq_cons = cq_cons;
		i = cq->hwq.depth;
		while (i--) {
			peek_hwcqe = bnxt_qplib_get_qe(&cq->hwq,
						       peek_sw_cq_cons, NULL);
			/* If the next hwcqe is VALID */
			if (CQE_CMP_VALID(peek_hwcqe, peek_flags)) {
				/* If the next hwcqe is a REQ */
				dma_rmb();
				switch (peek_hwcqe->cqe_type_toggle &
					CQ_BASE_CQE_TYPE_MASK) {
				case CQ_BASE_CQE_TYPE_REQ:
					peek_req_hwcqe = (struct cq_req *)
							 peek_hwcqe;
					peek_qp = (struct bnxt_qplib_qp *)
						le64_to_cpu(
						peek_req_hwcqe->qp_handle);
					peek_sq = &peek_qp->sq;
					peek_sq_cons_idx =
						((le16_to_cpu(
						  peek_req_hwcqe->sq_cons_idx)
						  - 1) % sq->max_wqe);
					/* If the hwcqe's sq's wr_id matches */
					if (peek_sq == sq &&
					    sq->swq[peek_sq_cons_idx].wr_id ==
					    BNXT_QPLIB_FENCE_WRID) {
						/* Unbreak only if the phantom
						   comes back */
						dev_dbg(&cq->hwq.pdev->dev,
							"FP: Process Req qp=0x%x current sq cons sw=0x%x cqe=0x%x\n",
							qp->id, swq_last,
							cqe_sq_cons);
						sq->condition = false;
						sq->single = true;
						sq->phantom_cqe_cnt++;
						dev_dbg(&cq->hwq.pdev->dev,
							"qp %#x condition restored at peek cq_cons=%#x sq_cons_idx %#x, phantom_cqe_cnt: %d unmark\n",
							peek_qp->id,
							peek_sw_cq_cons,
							peek_sq_cons_idx,
							sq->phantom_cqe_cnt);
						rc = 0;
						goto out;
					}
					break;

				case CQ_BASE_CQE_TYPE_TERMINAL:
					/* In case the QP has gone into the
					   error state */
					peek_term_hwcqe = (struct cq_terminal *)
							  peek_hwcqe;
					peek_qp = (struct bnxt_qplib_qp *)
						le64_to_cpu(
						peek_term_hwcqe->qp_handle);
					if (peek_qp == qp) {
						sq->condition = false;
						rc = 0;
						goto out;
					}
					break;
				default:
					break;
				}
				/* Valid but not the phantom, so keep looping */
			} else {
				/* Not valid yet, just exit and wait */
				rc = -EINVAL;
				goto out;
			}
			bnxt_qplib_hwq_incr_cons(cq->hwq.depth,
						 &peek_sw_cq_cons,
						 1, &peek_flags);
		}
		dev_err(&cq->hwq.pdev->dev,
			"Should not have come here! cq_cons=0x%x qp=0x%x sq cons sw=0x%x hw=0x%x\n",
			cq_cons, qp->id, swq_last, cqe_sq_cons);
		rc = -EINVAL;
	}
out:
	return rc;
}

static int bnxt_qplib_cq_process_req(struct bnxt_qplib_cq *cq,
				     struct cq_req *hwcqe,
				     struct bnxt_qplib_cqe **pcqe, int *budget,
				     u32 cq_cons, struct bnxt_qplib_qp **lib_qp)
{
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *sq;
	struct bnxt_qplib_cqe *cqe;
	u32 cqe_sq_cons;
	struct bnxt_qplib_swq *swq;
	int rc = 0;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	dev_dbg(&cq->hwq.pdev->dev, "FP: Process Req qp=0x%p\n", qp);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: Process Req qp is NULL\n");
		return -EINVAL;
	}
	sq = &qp->sq;

	cqe_sq_cons = le16_to_cpu(hwcqe->sq_cons_idx) % sq->max_wqe;
	if (qp->sq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}

	/* Require to walk the sq's swq to fabricate CQEs for all previously
	 * signaled SWQEs due to CQE aggregation from the current sq cons
	 * to the cqe_sq_cons
	 */
	cqe = *pcqe;
	while (*budget) {
		if (sq->swq_last == cqe_sq_cons)
			/* Done */
			break;

		swq = &sq->swq[sq->swq_last];
		memset(cqe, 0, sizeof(*cqe));
		cqe->opcode = CQ_BASE_CQE_TYPE_REQ;
		cqe->qp_handle = (u64)qp;
		cqe->src_qp = qp->id;
		cqe->wr_id = swq->wr_id;

		if (cqe->wr_id == BNXT_QPLIB_FENCE_WRID)
			goto skip;

		cqe->type = swq->type;

		/* For the last CQE, check for status.  For errors, regardless
		 * of the request being signaled or not, it must complete with
		 * the hwcqe error status
		 */
		if (swq->next_idx == cqe_sq_cons &&
		    hwcqe->status != CQ_REQ_STATUS_OK) {
			cqe->status = hwcqe->status;
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Processed Req \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: QP 0x%x wr_id[%d] = 0x%lx vendor type 0x%x with vendor status 0x%x\n",
				cqe->src_qp, sq->swq_last, cqe->wr_id, cqe->type, cqe->status);
			cqe++;
			(*budget)--;
			bnxt_qplib_mark_qp_error(qp);
		} else {
			/* Before we complete, do WA 9060 */
			if (!_is_chip_gen_p5_p7(qp->cctx)) {
				if (bnxt_re_legacy_do_wa9060(qp, cq, cq_cons,
					      sq->swq_last,
					      cqe_sq_cons)) {
					*lib_qp = qp;
					goto out;
				}
			}
			if (swq->flags & SQ_SEND_FLAGS_SIGNAL_COMP) {

				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: FP: CQ Processed Req \n");
				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: wr_id[%d] = 0x%llx \n",
					sq->swq_last, cqe->wr_id);
				dev_dbg(&cq->hwq.pdev->dev,
					"QPLIB: with status 0x%x\n", cqe->status);
				cqe->status = CQ_REQ_STATUS_OK;
				cqe++;
				(*budget)--;
			}
		}
skip:
		bnxt_qplib_hwq_incr_cons(sq->hwq.depth, &sq->hwq.cons,
					 swq->slots, &sq->dbinfo.flags);
		sq->swq_last = swq->next_idx;
		if (sq->single == true)
			break;
	}
out:
	*pcqe = cqe;
	if (sq->swq_last != cqe_sq_cons) {
		/* Out of budget */
		rc = -EAGAIN;
		goto done;
	}
	/* Back to normal completion mode only after it has completed all of
	   the WC for this CQE */
	sq->single = false;
done:
	return rc;
}

static void bnxt_qplib_release_srqe(struct bnxt_qplib_srq *srq, u32 tag)
{
	spin_lock(&srq->hwq.lock);
	srq->swq[srq->last_idx].next_idx = (int)tag;
	srq->last_idx = (int)tag;
	srq->swq[srq->last_idx].next_idx = -1;
	bnxt_qplib_hwq_incr_cons(srq->hwq.depth, &srq->hwq.cons,
				 srq->dbinfo.max_slot, &srq->dbinfo.flags);
	spin_unlock(&srq->hwq.lock);
}

static int bnxt_qplib_cq_process_res_rc(struct bnxt_qplib_cq *cq,
					struct cq_res_rc *hwcqe,
					struct bnxt_qplib_cqe **pcqe,
					int *budget)
{
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
	u32 wr_id_idx;
	int rc = 0;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev, "QPLIB: process_cq RC qp is NULL\n");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}

	cqe = *pcqe;
	cqe->opcode = hwcqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
	cqe->length = le32_to_cpu(hwcqe->length);
	cqe->invrkey = le32_to_cpu(hwcqe->imm_data_or_inv_r_key);
	cqe->mr_handle = le64_to_cpu(hwcqe->mr_handle);
	cqe->flags = le16_to_cpu(hwcqe->flags);
	cqe->status = hwcqe->status;
	cqe->qp_handle = (u64)(unsigned long)qp;

	wr_id_idx = le32_to_cpu(hwcqe->srq_or_rq_wr_id) &
				CQ_RES_RC_SRQ_OR_RQ_WR_ID_MASK;
	if (cqe->flags & CQ_RES_RC_FLAGS_SRQ_SRQ) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??\n");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process RC \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x\n",
				wr_id_idx, srq->hwq.depth);
			return -EINVAL;
		}
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		bnxt_qplib_release_srqe(srq, wr_id_idx);
		dev_dbg(&srq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed RC SRQ wr_id[%d] = 0x%llx\n",
			wr_id_idx, cqe->wr_id);
		cqe++;
		(*budget)--;
		*pcqe = cqe;
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process RC \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded RQ max 0x%x\n",
				wr_id_idx, rq->hwq.depth);
			return -EINVAL;
		}
		if (wr_id_idx != rq->swq_last)
			return -EINVAL;
		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed RC RQ wr_id[%d] = 0x%llx\n",
			rq->swq_last, cqe->wr_id);
		cqe++;
		(*budget)--;
		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[rq->swq_last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;

		if (hwcqe->status != CQ_RES_RC_STATUS_OK)
			bnxt_qplib_mark_qp_error(qp);
	}
done:
	return rc;
}

static int bnxt_qplib_cq_process_res_ud(struct bnxt_qplib_cq *cq,
					struct cq_res_ud_v2 *hwcqe,
					struct bnxt_qplib_cqe **pcqe,
					int *budget)
{
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
	u32 wr_id_idx;
	int rc = 0;
	u16 *smac;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev, "QPLIB: process_cq UD qp is NULL\n");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}
	cqe = *pcqe;
	cqe->opcode = hwcqe->cqe_type_toggle & CQ_RES_UD_V2_CQE_TYPE_MASK;
	cqe->length = le32_to_cpu((hwcqe->length & CQ_RES_UD_V2_LENGTH_MASK));
	cqe->cfa_meta = le16_to_cpu(hwcqe->cfa_metadata0);
	/* V2 format has metadata1 */
	cqe->cfa_meta |= (((le32_to_cpu(hwcqe->src_qp_high_srq_or_rq_wr_id) &
			   CQ_RES_UD_V2_CFA_METADATA1_MASK) >>
			  CQ_RES_UD_V2_CFA_METADATA1_SFT) <<
			 BNXT_QPLIB_META1_SHIFT);
	cqe->invrkey = le32_to_cpu(hwcqe->imm_data);
	cqe->flags = le16_to_cpu(hwcqe->flags);
	cqe->status = hwcqe->status;
	cqe->qp_handle = (u64)(unsigned long)qp;
	smac = (u16 *)cqe->smac;
	smac[2] = ntohs(le16_to_cpu(hwcqe->src_mac[0]));
	smac[1] = ntohs(le16_to_cpu(hwcqe->src_mac[1]));
	smac[0] = ntohs(le16_to_cpu(hwcqe->src_mac[2]));
	wr_id_idx = le32_to_cpu(hwcqe->src_qp_high_srq_or_rq_wr_id)
				& CQ_RES_UD_V2_SRQ_OR_RQ_WR_ID_MASK;
	cqe->src_qp = le16_to_cpu(hwcqe->src_qp_low) |
				  ((le32_to_cpu(
				    hwcqe->src_qp_high_srq_or_rq_wr_id) &
				    CQ_RES_UD_V2_SRC_QP_HIGH_MASK) >> 8);

	if (cqe->flags & CQ_RES_UD_V2_FLAGS_SRQ) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??\n");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process UD \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x\n",
				wr_id_idx, srq->hwq.depth);
			return -EINVAL;
		}
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		bnxt_qplib_release_srqe(srq, wr_id_idx);
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed UD SRQ wr_id[%d] = 0x%llx\n",
			wr_id_idx, cqe->wr_id);
		cqe++;
		(*budget)--;
		*pcqe = cqe;
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process UD \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded RQ max 0x%x\n",
				wr_id_idx, rq->hwq.depth);
			return -EINVAL;
		}
		if (rq->swq_last != wr_id_idx)
			return -EINVAL;

		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed UD RQ wr_id[%d] = 0x%llx\n",
			 rq->swq_last, cqe->wr_id);
		cqe++;
		(*budget)--;
		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[rq->swq_last].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;

		if (hwcqe->status != CQ_RES_UD_V2_STATUS_OK)
			bnxt_qplib_mark_qp_error(qp);
	}
done:
	return rc;
}

bool bnxt_qplib_is_cq_empty(struct bnxt_qplib_cq *cq)
{

	struct cq_base *hw_cqe;
	unsigned long flags;
	bool rc = true;

	spin_lock_irqsave(&cq->hwq.lock, flags);
	hw_cqe = bnxt_qplib_get_qe(&cq->hwq, cq->hwq.cons, NULL);

	 /* Check for Valid bit. If the CQE is valid, return false */
	rc = !CQE_CMP_VALID(hw_cqe, cq->dbinfo.flags);
	spin_unlock_irqrestore(&cq->hwq.lock, flags);
	return rc;
}

static int bnxt_qplib_cq_process_res_raweth_qp1(struct bnxt_qplib_cq *cq,
						struct cq_res_raweth_qp1 *hwcqe,
						struct bnxt_qplib_cqe **pcqe,
						int *budget)
{
	struct bnxt_qplib_qp *qp;
	struct bnxt_qplib_q *rq;
	struct bnxt_qplib_srq *srq;
	struct bnxt_qplib_cqe *cqe;
	u32 wr_id_idx;
	int rc = 0;

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: process_cq Raw/QP1 qp is NULL\n");
		return -EINVAL;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto done;
	}
	cqe = *pcqe;
	cqe->opcode = hwcqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
	cqe->flags = le16_to_cpu(hwcqe->flags);
	cqe->qp_handle = (u64)(unsigned long)qp;

	wr_id_idx = le32_to_cpu(hwcqe->raweth_qp1_payload_offset_srq_or_rq_wr_id)
				& CQ_RES_RAWETH_QP1_SRQ_OR_RQ_WR_ID_MASK;
	cqe->src_qp = qp->id;
	if (qp->id == 1 && !cqe->length) {
		/* Add workaround for the length misdetection */
		cqe->length = 296;
	} else {
		cqe->length = le16_to_cpu(hwcqe->length);
	}
	cqe->pkey_index = qp->pkey_index;
	memcpy(cqe->smac, qp->smac, 6);

	cqe->raweth_qp1_flags = le16_to_cpu(hwcqe->raweth_qp1_flags);
	cqe->raweth_qp1_flags2 = le32_to_cpu(hwcqe->raweth_qp1_flags2);
	cqe->raweth_qp1_metadata = le32_to_cpu(hwcqe->raweth_qp1_metadata);

	dev_dbg(&cq->hwq.pdev->dev,
		 "QPLIB: raweth_qp1_flags = 0x%x raweth_qp1_flags2 = 0x%x\n",
		 cqe->raweth_qp1_flags, cqe->raweth_qp1_flags2);

	if (cqe->flags & CQ_RES_RAWETH_QP1_FLAGS_SRQ_SRQ) {
		srq = qp->srq;
		if (!srq) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: SRQ used but not defined??\n");
			return -EINVAL;
		}
		if (wr_id_idx > srq->hwq.depth - 1) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process Raw/QP1 \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: wr_id idx 0x%x exceeded SRQ max 0x%x\n",
				wr_id_idx, srq->hwq.depth);
			return -EINVAL;
		}
		cqe->wr_id = srq->swq[wr_id_idx].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed Raw/QP1 SRQ \n");
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status = 0x%x\n",
			wr_id_idx, cqe->wr_id, hwcqe->status);
		cqe++;
		(*budget)--;
		srq->hwq.cons++;
		*pcqe = cqe;
	} else {
		rq = &qp->rq;
		if (wr_id_idx > (rq->max_wqe - 1)) {
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Process Raw/QP1 RQ wr_id \n");
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: ix 0x%x exceeded RQ max 0x%x\n",
				wr_id_idx, rq->max_wqe);
			return -EINVAL;
		}
		if (wr_id_idx != rq->swq_last)
			return -EINVAL;
		cqe->wr_id = rq->swq[rq->swq_last].wr_id;
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed Raw/QP1 RQ \n");
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: wr_id[%d] = 0x%llx with status = 0x%x\n",
			wr_id_idx, cqe->wr_id, hwcqe->status);
		cqe++;
		(*budget)--;
		bnxt_qplib_hwq_incr_cons(rq->hwq.depth, &rq->hwq.cons,
					 rq->swq[wr_id_idx].slots,
					 &rq->dbinfo.flags);
		rq->swq_last = rq->swq[rq->swq_last].next_idx;
		*pcqe = cqe;

		if (hwcqe->status != CQ_RES_RC_STATUS_OK)
			bnxt_qplib_mark_qp_error(qp);
	}
done:
	return rc;
}

static int bnxt_qplib_cq_process_terminal(struct bnxt_qplib_cq *cq,
					  struct cq_terminal *hwcqe,
					  struct bnxt_qplib_cqe **pcqe,
					  int *budget)
{
	struct bnxt_qplib_q *sq, *rq;
	struct bnxt_qplib_cqe *cqe;
	struct bnxt_qplib_qp *qp;
	u32 swq_last;
	u32 cqe_cons;
	int rc = 0;

	/* Check the Status */
	if (hwcqe->status != CQ_TERMINAL_STATUS_OK)
		dev_warn(&cq->hwq.pdev->dev,
			 "QPLIB: FP: CQ Process Terminal Error status = 0x%x\n",
			 hwcqe->status);

	qp = (struct bnxt_qplib_qp *)le64_to_cpu(hwcqe->qp_handle);
	if (!qp)
		return -EINVAL;
	dev_dbg(&cq->hwq.pdev->dev,
		"QPLIB: FP: CQ Process terminal for qp (0x%x)\n", qp->id);

	/* Terminal CQE requires all posted RQEs to complete with FLUSHED_ERR
	 * from the current rq->cons to the rq->prod regardless what the
	 * rq->cons the terminal CQE indicates.
	 */
	bnxt_qplib_mark_qp_error(qp);

	sq = &qp->sq;
	rq = &qp->rq;

	cqe_cons = le16_to_cpu(hwcqe->sq_cons_idx);
	if (cqe_cons == 0xFFFF)
		goto do_rq;

	cqe_cons %= sq->max_wqe;
	if (qp->sq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		goto sq_done;
	}

	/* Terminal CQE can also include aggregated successful CQEs prior.
	   So we must complete all CQEs from the current sq's cons to the
	   cq_cons with status OK */
	cqe = *pcqe;
	while (*budget) {
		/*sw_cons = HWQ_CMP(sq->hwq.cons, &sq->hwq);*/
		swq_last = sq->swq_last;
		if (swq_last == cqe_cons)
			break;
		if (sq->swq[swq_last].flags & SQ_SEND_FLAGS_SIGNAL_COMP) {
			memset(cqe, 0, sizeof(*cqe));
			cqe->status = CQ_REQ_STATUS_OK;
			cqe->opcode = CQ_BASE_CQE_TYPE_REQ;
			cqe->qp_handle = (u64)qp;
			cqe->src_qp = qp->id;
			cqe->wr_id = sq->swq[swq_last].wr_id;
			cqe->type = sq->swq[swq_last].type;
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: FP: CQ Processed terminal Req \n");
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: wr_id[%d] = 0x%llx with status 0x%x\n",
				swq_last, cqe->wr_id, cqe->status);
			cqe++;
			(*budget)--;
		}
		bnxt_qplib_hwq_incr_cons(sq->hwq.depth, &sq->hwq.cons,
					 sq->swq[swq_last].slots,
					 &sq->dbinfo.flags);
		sq->swq_last = sq->swq[swq_last].next_idx;
	}
	*pcqe = cqe;
	if (!*budget && swq_last != cqe_cons) {
		/* Out of budget */
		rc = -EAGAIN;
		goto sq_done;
	}
sq_done:
	if (rc)
		return rc;
do_rq:
	cqe_cons = le16_to_cpu(hwcqe->rq_cons_idx);
	if (cqe_cons == 0xFFFF) {
		goto done;
	} else if (cqe_cons > (rq->max_wqe - 1)) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Processed terminal \n");
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: reported rq_cons_idx 0x%x exceeds max 0x%x\n",
			cqe_cons, rq->hwq.depth);
		goto done;
	}
	if (qp->rq.flushed) {
		dev_dbg(&cq->hwq.pdev->dev,
			"%s: QPLIB: QP in Flush QP = %p\n", __func__, qp);
		rc = 0;
		goto rq_done;
	}

rq_done:
done:
	return rc;
}

static int bnxt_qplib_cq_process_cutoff(struct bnxt_qplib_cq *cq,
					struct cq_cutoff *hwcqe)
{
	/* Check the Status */
	if (hwcqe->status != CQ_CUTOFF_STATUS_OK) {
		dev_err(&cq->hwq.pdev->dev,
			"QPLIB: FP: CQ Process Cutoff Error status = 0x%x\n",
			hwcqe->status);
		return -EINVAL;
	}
	clear_bit(CQ_FLAGS_RESIZE_IN_PROG, &cq->flags);
	wake_up_interruptible(&cq->waitq);

	dev_dbg(&cq->hwq.pdev->dev, "QPLIB: FP: CQ Processed Cutoff\n");
	return 0;
}

int bnxt_qplib_process_flush_list(struct bnxt_qplib_cq *cq,
				struct bnxt_qplib_cqe *cqe,
				int num_cqes)
{
	struct bnxt_qplib_qp *qp = NULL;
	u32 budget = num_cqes;
	unsigned long flags;

	spin_lock_irqsave(&cq->flush_lock, flags);
	list_for_each_entry(qp, &cq->sqf_head, sq_flush) {
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: Flushing SQ QP= %p\n",
			qp);
		__flush_sq(&qp->sq, qp, &cqe, &budget);
	}

	list_for_each_entry(qp, &cq->rqf_head, rq_flush) {
		dev_dbg(&cq->hwq.pdev->dev,
			"QPLIB: FP: Flushing RQ QP= %p\n",
			qp);
		__flush_rq(&qp->rq, qp, &cqe, &budget);
	}
	spin_unlock_irqrestore(&cq->flush_lock, flags);

	return num_cqes - budget;
}

int bnxt_qplib_poll_cq(struct bnxt_qplib_cq *cq, struct bnxt_qplib_cqe *cqe,
		       int num_cqes, struct bnxt_qplib_qp **lib_qp)
{
	struct cq_base *hw_cqe;
	u32 hw_polled = 0;
	int budget, rc = 0;
	u8 type;

	budget = num_cqes;

	while (budget) {
		hw_cqe = bnxt_qplib_get_qe(&cq->hwq, cq->hwq.cons, NULL);

		/* Check for Valid bit */
		if (!CQE_CMP_VALID(hw_cqe, cq->dbinfo.flags))
			break;

		/* The valid test of the entry must be done first before
		 * reading any further.
		 */
		dma_rmb();
		/* From the device's respective CQE format to qplib_wc*/
		type = hw_cqe->cqe_type_toggle & CQ_BASE_CQE_TYPE_MASK;
		switch (type) {
		case CQ_BASE_CQE_TYPE_REQ:
			rc = bnxt_qplib_cq_process_req(cq,
					(struct cq_req *)hw_cqe, &cqe, &budget,
					cq->hwq.cons, lib_qp);
			break;
		case CQ_BASE_CQE_TYPE_RES_RC:
			rc = bnxt_qplib_cq_process_res_rc(cq,
						(struct cq_res_rc *)hw_cqe, &cqe,
						&budget);
			break;
		case CQ_BASE_CQE_TYPE_RES_UD:
			rc = bnxt_qplib_cq_process_res_ud(cq,
						(struct cq_res_ud_v2 *)hw_cqe,
						&cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_RES_RAWETH_QP1:
			rc = bnxt_qplib_cq_process_res_raweth_qp1(cq,
						(struct cq_res_raweth_qp1 *)
						hw_cqe, &cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_TERMINAL:
			rc = bnxt_qplib_cq_process_terminal(cq,
						(struct cq_terminal *)hw_cqe,
						&cqe, &budget);
			break;
		case CQ_BASE_CQE_TYPE_CUT_OFF:
			bnxt_qplib_cq_process_cutoff(cq,
						(struct cq_cutoff *)hw_cqe);
			/* Done processing this CQ */
			goto exit;
		default:
			dev_err(&cq->hwq.pdev->dev,
				"QPLIB: process_cq unknown type 0x%x\n",
				hw_cqe->cqe_type_toggle &
				CQ_BASE_CQE_TYPE_MASK);
			rc = -EINVAL;
			break;
		}
		if (rc < 0) {
			dev_dbg(&cq->hwq.pdev->dev,
				"QPLIB: process_cqe rc = 0x%x\n", rc);
			if (rc == -EAGAIN)
				break;
			/* Error while processing the CQE, just skip to the
			   next one */
			if (type != CQ_BASE_CQE_TYPE_TERMINAL)
				dev_err(&cq->hwq.pdev->dev,
					"QPLIB: process_cqe error rc = 0x%x\n",
					rc);
		}
		hw_polled++;
		bnxt_qplib_hwq_incr_cons(cq->hwq.depth, &cq->hwq.cons,
					 1, &cq->dbinfo.flags);
	}
	if (hw_polled)
		bnxt_qplib_ring_db(&cq->dbinfo, DBC_DBC_TYPE_CQ);
exit:
	return num_cqes - budget;
}

void bnxt_qplib_req_notify_cq(struct bnxt_qplib_cq *cq, u32 arm_type)
{
	cq->dbinfo.toggle = cq->toggle;
	if (arm_type)
		bnxt_qplib_ring_db(&cq->dbinfo, arm_type);
	/* Using cq->arm_state variable to track whether to issue cq handler */
	atomic_set(&cq->arm_state, 1);
}

void bnxt_qplib_flush_cqn_wq(struct bnxt_qplib_qp *qp)
{
	flush_workqueue(qp->scq->nq->cqn_wq);
	if (qp->scq != qp->rcq)
		flush_workqueue(qp->rcq->nq->cqn_wq);
}
