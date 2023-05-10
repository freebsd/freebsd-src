/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009-2013 Chelsio, Inc. All rights reserved.
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
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_inet.h"

#ifdef TCP_OFFLOAD
#include <linux/slab.h>

#include "iw_cxgbe.h"

static void print_tpte(struct adapter *sc, const u32 stag,
    const struct fw_ri_tpte *tpte)
{
	const __be64 *p = (const void *)tpte;

        CH_ERR(sc, "stag idx 0x%x valid %d key 0x%x state %d pdid %d "
               "perm 0x%x ps %d len 0x%016llx va 0x%016llx\n",
               stag & 0xffffff00,
               G_FW_RI_TPTE_VALID(ntohl(tpte->valid_to_pdid)),
               G_FW_RI_TPTE_STAGKEY(ntohl(tpte->valid_to_pdid)),
               G_FW_RI_TPTE_STAGSTATE(ntohl(tpte->valid_to_pdid)),
               G_FW_RI_TPTE_PDID(ntohl(tpte->valid_to_pdid)),
               G_FW_RI_TPTE_PERM(ntohl(tpte->locread_to_qpid)),
               G_FW_RI_TPTE_PS(ntohl(tpte->locread_to_qpid)),
	       (long long)(((u64)ntohl(tpte->len_hi) << 32) | ntohl(tpte->len_lo)),
               (long long)(((u64)ntohl(tpte->va_hi) << 32) | ntohl(tpte->va_lo_fbo)));
	CH_ERR(sc, "stag idx 0x%x: %016llx %016llx %016llx %016llx\n",
	    stag & 0xffffff00,
	    (long long)be64_to_cpu(p[0]), (long long)be64_to_cpu(p[1]),
	    (long long)be64_to_cpu(p[2]), (long long)be64_to_cpu(p[3]));
}

void t4_dump_stag(struct adapter *sc, const u32 stag)
{
	struct fw_ri_tpte tpte __aligned(sizeof(__be64)) = {0};
	const u32 offset = sc->vres.stag.start + ((stag >> 8) * 32);

	if (offset > sc->vres.stag.start + sc->vres.stag.size - 32) {
		CH_ERR(sc, "stag 0x%x is invalid for current configuration.\n",
		    stag);
		return;
	}
	read_via_memwin(sc, 0, offset, (u32 *)&tpte, 32);
	print_tpte(sc, stag, &tpte);
}

void t4_dump_all_stag(struct adapter *sc)
{
	struct fw_ri_tpte tpte __aligned(sizeof(__be64)) = {0};
	const u32 first = sc->vres.stag.start;
	const u32 last = first + sc->vres.stag.size - 32;
	u32 offset, i;

	for (i = 0, offset = first; offset <= last; i++, offset += 32) {
		tpte.valid_to_pdid = 0;
		read_via_memwin(sc, 0, offset, (u32 *)&tpte, 4);
		if (tpte.valid_to_pdid != 0) {
			read_via_memwin(sc, 0, offset, (u32 *)&tpte, 32);
			print_tpte(sc, i << 8, &tpte);
		}
	}
}

static void dump_err_cqe(struct c4iw_dev *dev, struct t4_cqe *err_cqe)
{
	struct adapter *sc = dev->rdev.adap;
	__be64 *p = (void *)err_cqe;

	CH_ERR(sc, "AE qpid 0x%x opcode %d status 0x%x "
	       "type %d wrid.hi 0x%x wrid.lo 0x%x\n",
	       CQE_QPID(err_cqe), CQE_OPCODE(err_cqe),
	       CQE_STATUS(err_cqe), CQE_TYPE(err_cqe),
	       CQE_WRID_HI(err_cqe), CQE_WRID_LOW(err_cqe));
	CH_ERR(sc, "%016llx %016llx %016llx %016llx\n",
	    (long long)be64_to_cpu(p[0]), (long long)be64_to_cpu(p[1]),
	    (long long)be64_to_cpu(p[2]), (long long)be64_to_cpu(p[3]));

	/*
	 * Ingress WRITE and READ_RESP errors provide
	 * the offending stag, so parse and log it.
	 */
	if (RQ_TYPE(err_cqe) && (CQE_OPCODE(err_cqe) == FW_RI_RDMA_WRITE ||
	    CQE_OPCODE(err_cqe) == FW_RI_READ_RESP))
		t4_dump_stag(sc, CQE_WRID_STAG(err_cqe));
}

static void post_qp_event(struct c4iw_dev *dev, struct c4iw_cq *chp,
			  struct c4iw_qp *qhp,
			  struct t4_cqe *err_cqe,
			  enum ib_event_type ib_event)
{
	struct ib_event event;
	struct c4iw_qp_attributes attrs;
	unsigned long flag;

	if ((qhp->attr.state == C4IW_QP_STATE_ERROR) ||
	    (qhp->attr.state == C4IW_QP_STATE_TERMINATE)) {
		CTR4(KTR_IW_CXGBE, "%s AE received after RTS - "
		     "qp state %d qpid 0x%x status 0x%x", __func__,
		     qhp->attr.state, qhp->wq.sq.qid, CQE_STATUS(err_cqe));
		return;
	}

	dump_err_cqe(dev, err_cqe);

	if (qhp->attr.state == C4IW_QP_STATE_RTS) {
		attrs.next_state = C4IW_QP_STATE_TERMINATE;
		c4iw_modify_qp(qhp->rhp, qhp, C4IW_QP_ATTR_NEXT_STATE,
			       &attrs, 0);
	}

	event.event = ib_event;
	event.device = chp->ibcq.device;
	if (ib_event == IB_EVENT_CQ_ERR)
		event.element.cq = &chp->ibcq;
	else
		event.element.qp = &qhp->ibqp;
	if (qhp->ibqp.event_handler)
		(*qhp->ibqp.event_handler)(&event, qhp->ibqp.qp_context);

	spin_lock_irqsave(&chp->comp_handler_lock, flag);
	(*chp->ibcq.comp_handler)(&chp->ibcq, chp->ibcq.cq_context);
	spin_unlock_irqrestore(&chp->comp_handler_lock, flag);
}

void c4iw_ev_dispatch(struct c4iw_dev *dev, struct t4_cqe *err_cqe)
{
	struct c4iw_cq *chp;
	struct c4iw_qp *qhp;
	u32 cqid;

	spin_lock_irq(&dev->lock);
	qhp = get_qhp(dev, CQE_QPID(err_cqe));
	if (!qhp) {
		printf("BAD AE qpid 0x%x opcode %d "
		       "status 0x%x type %d wrid.hi 0x%x wrid.lo 0x%x\n",
		       CQE_QPID(err_cqe),
		       CQE_OPCODE(err_cqe), CQE_STATUS(err_cqe),
		       CQE_TYPE(err_cqe), CQE_WRID_HI(err_cqe),
		       CQE_WRID_LOW(err_cqe));
		spin_unlock_irq(&dev->lock);
		goto out;
	}

	if (SQ_TYPE(err_cqe))
		cqid = qhp->attr.scq;
	else
		cqid = qhp->attr.rcq;
	chp = get_chp(dev, cqid);
	if (!chp) {
		printf("BAD AE cqid 0x%x qpid 0x%x opcode %d "
		       "status 0x%x type %d wrid.hi 0x%x wrid.lo 0x%x\n",
		       cqid, CQE_QPID(err_cqe),
		       CQE_OPCODE(err_cqe), CQE_STATUS(err_cqe),
		       CQE_TYPE(err_cqe), CQE_WRID_HI(err_cqe),
		       CQE_WRID_LOW(err_cqe));
		spin_unlock_irq(&dev->lock);
		goto out;
	}

	c4iw_qp_add_ref(&qhp->ibqp);
	atomic_inc(&chp->refcnt);
	spin_unlock_irq(&dev->lock);

	/* Bad incoming write */
	if (RQ_TYPE(err_cqe) &&
	    (CQE_OPCODE(err_cqe) == FW_RI_RDMA_WRITE)) {
		post_qp_event(dev, chp, qhp, err_cqe, IB_EVENT_QP_REQ_ERR);
		goto done;
	}

	switch (CQE_STATUS(err_cqe)) {

	/* Completion Events */
	case T4_ERR_SUCCESS:
		printf(KERN_ERR MOD "AE with status 0!\n");
		break;

	case T4_ERR_STAG:
	case T4_ERR_PDID:
	case T4_ERR_QPID:
	case T4_ERR_ACCESS:
	case T4_ERR_WRAP:
	case T4_ERR_BOUND:
	case T4_ERR_INVALIDATE_SHARED_MR:
	case T4_ERR_INVALIDATE_MR_WITH_MW_BOUND:
		post_qp_event(dev, chp, qhp, err_cqe, IB_EVENT_QP_ACCESS_ERR);
		break;

	/* Device Fatal Errors */
	case T4_ERR_ECC:
	case T4_ERR_ECC_PSTAG:
	case T4_ERR_INTERNAL_ERR:
		post_qp_event(dev, chp, qhp, err_cqe, IB_EVENT_DEVICE_FATAL);
		break;

	/* QP Fatal Errors */
	case T4_ERR_OUT_OF_RQE:
	case T4_ERR_PBL_ADDR_BOUND:
	case T4_ERR_CRC:
	case T4_ERR_MARKER:
	case T4_ERR_PDU_LEN_ERR:
	case T4_ERR_DDP_VERSION:
	case T4_ERR_RDMA_VERSION:
	case T4_ERR_OPCODE:
	case T4_ERR_DDP_QUEUE_NUM:
	case T4_ERR_MSN:
	case T4_ERR_TBIT:
	case T4_ERR_MO:
	case T4_ERR_MSN_GAP:
	case T4_ERR_MSN_RANGE:
	case T4_ERR_RQE_ADDR_BOUND:
	case T4_ERR_IRD_OVERFLOW:
		post_qp_event(dev, chp, qhp, err_cqe, IB_EVENT_QP_FATAL);
		break;

	default:
		printf("Unknown T4 status 0x%x QPID 0x%x\n",
		       CQE_STATUS(err_cqe), qhp->wq.sq.qid);
		post_qp_event(dev, chp, qhp, err_cqe, IB_EVENT_QP_FATAL);
		break;
	}
done:
	if (atomic_dec_and_test(&chp->refcnt))
		wake_up(&chp->wait);
	c4iw_qp_rem_ref(&qhp->ibqp);
out:
	return;
}

int c4iw_ev_handler(struct sge_iq *iq, const struct rsp_ctrl *rc)
{
	struct c4iw_dev *dev = iq->adapter->iwarp_softc;
	u32 qid = be32_to_cpu(rc->pldbuflen_qid);
	struct c4iw_cq *chp;
	unsigned long flag;

	spin_lock_irqsave(&dev->lock, flag);
	chp = get_chp(dev, qid);
	if (chp) {
		atomic_inc(&chp->refcnt);
		spin_unlock_irqrestore(&dev->lock, flag);

		spin_lock_irqsave(&chp->comp_handler_lock, flag);
		(*chp->ibcq.comp_handler)(&chp->ibcq, chp->ibcq.cq_context);
		spin_unlock_irqrestore(&chp->comp_handler_lock, flag);
		if (atomic_dec_and_test(&chp->refcnt))
			wake_up(&chp->wait);
	} else {
		CTR2(KTR_IW_CXGBE, "%s unknown cqid 0x%x", __func__, qid);
		spin_unlock_irqrestore(&dev->lock, flag);
	}

	return 0;
}
#endif
