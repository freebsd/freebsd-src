/**************************************************************************

Copyright (c) 2007, Chelsio Inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Neither the name of the Chelsio Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/pciio.h>
#include <sys/conf.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/bus_dma.h>
#include <sys/rman.h>
#include <sys/ioccom.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/smp.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/libkern.h>

#include <netinet/in.h>

#include <contrib/rdma/ib_verbs.h>
#include <contrib/rdma/ib_umem.h>
#include <contrib/rdma/ib_user_verbs.h>

#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>
#include <ulp/iw_cxgb/iw_cxgb_resource.h>
#include <ulp/iw_cxgb/iw_cxgb_user.h>

static void
post_qp_event(struct iwch_dev *rnicp, struct iwch_qp *qhp, struct iwch_cq *chp,
		struct respQ_msg_t *rsp_msg,
		enum ib_event_type ib_event,
		int send_term)
{
	struct ib_event event;
	struct iwch_qp_attributes attrs;

	if ((qhp->attr.state == IWCH_QP_STATE_ERROR) ||
	    (qhp->attr.state == IWCH_QP_STATE_TERMINATE)) {
		CTR4(KTR_IW_CXGB, "%s AE received after RTS - "
		     "qp state %d qpid 0x%x status 0x%x", __FUNCTION__,
		     qhp->attr.state, qhp->wq.qpid, CQE_STATUS(rsp_msg->cqe));
		return;
	}

	log(LOG_ERR, "%s - AE qpid 0x%x opcode %d status 0x%x "
	       "type %d wrid.hi 0x%x wrid.lo 0x%x \n", __FUNCTION__,
	       CQE_QPID(rsp_msg->cqe), CQE_OPCODE(rsp_msg->cqe),
	       CQE_STATUS(rsp_msg->cqe), CQE_TYPE(rsp_msg->cqe),
	       CQE_WRID_HI(rsp_msg->cqe), CQE_WRID_LOW(rsp_msg->cqe));


	event.event = ib_event;
	event.device = chp->ibcq.device;
	if (ib_event == IB_EVENT_CQ_ERR)
		event.element.cq = &chp->ibcq;
	else
		event.element.qp = &qhp->ibqp;

	if (qhp->ibqp.event_handler)
		(*qhp->ibqp.event_handler)(&event, qhp->ibqp.qp_context);

	if (qhp->attr.state == IWCH_QP_STATE_RTS) {
		attrs.next_state = IWCH_QP_STATE_TERMINATE;
		iwch_modify_qp(qhp->rhp, qhp, IWCH_QP_ATTR_NEXT_STATE,
			       &attrs, 1);
		if (send_term)
			iwch_post_terminate(qhp, rsp_msg);
	}
}

void
iwch_ev_dispatch(struct cxio_rdev *rdev_p, struct mbuf *m)
{
	struct iwch_dev *rnicp;
	struct respQ_msg_t *rsp_msg = (struct respQ_msg_t *) m->m_data;
	struct iwch_cq *chp;
	struct iwch_qp *qhp;
	u32 cqid = RSPQ_CQID(rsp_msg);

	rnicp = (struct iwch_dev *) rdev_p->ulp;
	mtx_lock(&rnicp->lock);
	chp = get_chp(rnicp, cqid);
	qhp = get_qhp(rnicp, CQE_QPID(rsp_msg->cqe));
	if (!chp || !qhp) {
		log(LOG_ERR,"BAD AE cqid 0x%x qpid 0x%x opcode %d "
		       "status 0x%x type %d wrid.hi 0x%x wrid.lo 0x%x \n",
		       cqid, CQE_QPID(rsp_msg->cqe),
		       CQE_OPCODE(rsp_msg->cqe), CQE_STATUS(rsp_msg->cqe),
		       CQE_TYPE(rsp_msg->cqe), CQE_WRID_HI(rsp_msg->cqe),
		       CQE_WRID_LOW(rsp_msg->cqe));
		mtx_unlock(&rnicp->lock);
		goto out;
	}
	iwch_qp_add_ref(&qhp->ibqp);
	mtx_lock(&chp->lock);
	++chp->refcnt;
	mtx_unlock(&chp->lock);
	mtx_unlock(&rnicp->lock);

	/*
	 * 1) completion of our sending a TERMINATE.
	 * 2) incoming TERMINATE message.
	 */
	if ((CQE_OPCODE(rsp_msg->cqe) == T3_TERMINATE) &&
	    (CQE_STATUS(rsp_msg->cqe) == 0)) {
		if (SQ_TYPE(rsp_msg->cqe)) {
			CTR3(KTR_IW_CXGB, "%s QPID 0x%x ep %p disconnecting",
			     __FUNCTION__, qhp->wq.qpid, qhp->ep);
			iwch_ep_disconnect(qhp->ep, 0, M_NOWAIT);
		} else {
			CTR2(KTR_IW_CXGB, "%s post REQ_ERR AE QPID 0x%x", __FUNCTION__,
			     qhp->wq.qpid);
			post_qp_event(rnicp, qhp, chp, rsp_msg,
				      IB_EVENT_QP_REQ_ERR, 0);
			iwch_ep_disconnect(qhp->ep, 0, M_NOWAIT);
		}
		goto done;
	}

	/* Bad incoming Read request */
	if (SQ_TYPE(rsp_msg->cqe) &&
	    (CQE_OPCODE(rsp_msg->cqe) == T3_READ_RESP)) {
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_QP_REQ_ERR, 1);
		goto done;
	}

	/* Bad incoming write */
	if (RQ_TYPE(rsp_msg->cqe) &&
	    (CQE_OPCODE(rsp_msg->cqe) == T3_RDMA_WRITE)) {
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_QP_REQ_ERR, 1);
		goto done;
	}

	switch (CQE_STATUS(rsp_msg->cqe)) {

	/* Completion Events */
	case TPT_ERR_SUCCESS:
#if 0
		/*
		 * Confirm the destination entry if this is a RECV completion.
		 */
		if (qhp->ep && SQ_TYPE(rsp_msg->cqe))
			dst_confirm(qhp->ep->dst);
#endif		
		(*chp->ibcq.comp_handler)(&chp->ibcq, chp->ibcq.cq_context);
		break;

	case TPT_ERR_STAG:
	case TPT_ERR_PDID:
	case TPT_ERR_QPID:
	case TPT_ERR_ACCESS:
	case TPT_ERR_WRAP:
	case TPT_ERR_BOUND:
	case TPT_ERR_INVALIDATE_SHARED_MR:
	case TPT_ERR_INVALIDATE_MR_WITH_MW_BOUND:
		log(LOG_ERR, "%s - CQE Err qpid 0x%x opcode %d status 0x%x "
		       "type %d wrid.hi 0x%x wrid.lo 0x%x \n", __FUNCTION__,
		       CQE_QPID(rsp_msg->cqe), CQE_OPCODE(rsp_msg->cqe),
		       CQE_STATUS(rsp_msg->cqe), CQE_TYPE(rsp_msg->cqe),
		       CQE_WRID_HI(rsp_msg->cqe), CQE_WRID_LOW(rsp_msg->cqe));
		(*chp->ibcq.comp_handler)(&chp->ibcq, chp->ibcq.cq_context);
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_QP_ACCESS_ERR, 1);
		break;

	/* Device Fatal Errors */
	case TPT_ERR_ECC:
	case TPT_ERR_ECC_PSTAG:
	case TPT_ERR_INTERNAL_ERR:
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_DEVICE_FATAL, 1);
		break;

	/* QP Fatal Errors */
	case TPT_ERR_OUT_OF_RQE:
	case TPT_ERR_PBL_ADDR_BOUND:
	case TPT_ERR_CRC:
	case TPT_ERR_MARKER:
	case TPT_ERR_PDU_LEN_ERR:
	case TPT_ERR_DDP_VERSION:
	case TPT_ERR_RDMA_VERSION:
	case TPT_ERR_OPCODE:
	case TPT_ERR_DDP_QUEUE_NUM:
	case TPT_ERR_MSN:
	case TPT_ERR_TBIT:
	case TPT_ERR_MO:
	case TPT_ERR_MSN_GAP:
	case TPT_ERR_MSN_RANGE:
	case TPT_ERR_RQE_ADDR_BOUND:
	case TPT_ERR_IRD_OVERFLOW:
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_QP_FATAL, 1);
		break;

	default:
		log(LOG_ERR,"Unknown T3 status 0x%x QPID 0x%x\n",
		       CQE_STATUS(rsp_msg->cqe), qhp->wq.qpid);
		post_qp_event(rnicp, qhp, chp, rsp_msg, IB_EVENT_QP_FATAL, 1);
		break;
	}
done:
	mtx_lock(&chp->lock);
	if (--chp->refcnt == 0)
	        wakeup(chp);
	mtx_unlock(&chp->lock);
	iwch_qp_rem_ref(&qhp->ibqp);
out:
	m_free(m);
}
