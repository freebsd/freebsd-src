
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

/*
 * Get one cq entry from cxio and map it to openib.
 *
 * Returns:
 *	0			cqe returned
 *	-ENOBUFS		EMPTY;
 *	-EAGAIN		        caller must try again
 *	any other neg errno	fatal error
 */
static int iwch_poll_cq_one(struct iwch_dev *rhp, struct iwch_cq *chp,
			    struct ib_wc *wc)
{
	struct iwch_qp *qhp = NULL;
	struct t3_cqe cqe, *rd_cqe;
	struct t3_wq *wq;
	u32 credit = 0;
	u8 cqe_flushed;
	u64 cookie;
	int ret = 1;

	rd_cqe = cxio_next_cqe(&chp->cq);

	if (!rd_cqe)
		return 0;

	qhp = get_qhp(rhp, CQE_QPID(*rd_cqe));
	if (!qhp)
		wq = NULL;
	else {
		mtx_lock(&qhp->lock);
		wq = &(qhp->wq);
	}
	ret = cxio_poll_cq(wq, &(chp->cq), &cqe, &cqe_flushed, &cookie,
				   &credit);
	if (t3a_device(chp->rhp) && credit) {
		CTR3(KTR_IW_CXGB, "%s updating %d cq credits on id %d", __FUNCTION__,
		     credit, chp->cq.cqid);
		cxio_hal_cq_op(&rhp->rdev, &chp->cq, CQ_CREDIT_UPDATE, credit);
	}

	if (ret) {
		ret = -EAGAIN;
		goto out;
	}
	ret = 1;

	wc->wr_id = cookie;
	wc->qp = &qhp->ibqp;
	wc->vendor_err = CQE_STATUS(cqe);

	CTR4(KTR_IW_CXGB, "iwch_poll_cq_one qpid 0x%x type %d opcode %d status 0x%x",
	     CQE_QPID(cqe), CQE_TYPE(cqe),
	     CQE_OPCODE(cqe), CQE_STATUS(cqe));
	CTR3(KTR_IW_CXGB, "wrid hi 0x%x lo 0x%x cookie 0x%llx", 
	     CQE_WRID_HI(cqe), CQE_WRID_LOW(cqe), (unsigned long long) cookie);

	if (CQE_TYPE(cqe) == 0) {
		if (!CQE_STATUS(cqe))
			wc->byte_len = CQE_LEN(cqe);
		else
			wc->byte_len = 0;
		wc->opcode = IB_WC_RECV;
	} else {
		switch (CQE_OPCODE(cqe)) {
		case T3_RDMA_WRITE:
			wc->opcode = IB_WC_RDMA_WRITE;
			break;
		case T3_READ_REQ:
			wc->opcode = IB_WC_RDMA_READ;
			wc->byte_len = CQE_LEN(cqe);
			break;
		case T3_SEND:
		case T3_SEND_WITH_SE:
			wc->opcode = IB_WC_SEND;
			break;
		case T3_BIND_MW:
			wc->opcode = IB_WC_BIND_MW;
			break;

		/* these aren't supported yet */
		case T3_SEND_WITH_INV:
		case T3_SEND_WITH_SE_INV:
		case T3_LOCAL_INV:
		case T3_FAST_REGISTER:
		default:
			log(LOG_ERR, "Unexpected opcode %d "
			       "in the CQE received for QPID=0x%0x\n",
			       CQE_OPCODE(cqe), CQE_QPID(cqe));
			ret = -EINVAL;
			goto out;
		}
	}

	if (cqe_flushed)
		wc->status = IB_WC_WR_FLUSH_ERR;
	else {

		switch (CQE_STATUS(cqe)) {
		case TPT_ERR_SUCCESS:
			wc->status = IB_WC_SUCCESS;
			break;
		case TPT_ERR_STAG:
			wc->status = IB_WC_LOC_ACCESS_ERR;
			break;
		case TPT_ERR_PDID:
			wc->status = IB_WC_LOC_PROT_ERR;
			break;
		case TPT_ERR_QPID:
		case TPT_ERR_ACCESS:
			wc->status = IB_WC_LOC_ACCESS_ERR;
			break;
		case TPT_ERR_WRAP:
			wc->status = IB_WC_GENERAL_ERR;
			break;
		case TPT_ERR_BOUND:
			wc->status = IB_WC_LOC_LEN_ERR;
			break;
		case TPT_ERR_INVALIDATE_SHARED_MR:
		case TPT_ERR_INVALIDATE_MR_WITH_MW_BOUND:
			wc->status = IB_WC_MW_BIND_ERR;
			break;
		case TPT_ERR_CRC:
		case TPT_ERR_MARKER:
		case TPT_ERR_PDU_LEN_ERR:
		case TPT_ERR_OUT_OF_RQE:
		case TPT_ERR_DDP_VERSION:
		case TPT_ERR_RDMA_VERSION:
		case TPT_ERR_DDP_QUEUE_NUM:
		case TPT_ERR_MSN:
		case TPT_ERR_TBIT:
		case TPT_ERR_MO:
		case TPT_ERR_MSN_RANGE:
		case TPT_ERR_IRD_OVERFLOW:
		case TPT_ERR_OPCODE:
			wc->status = IB_WC_FATAL_ERR;
			break;
		case TPT_ERR_SWFLUSH:
			wc->status = IB_WC_WR_FLUSH_ERR;
			break;
		default:
			log(LOG_ERR, "Unexpected cqe_status 0x%x for "
			       "QPID=0x%0x\n", CQE_STATUS(cqe), CQE_QPID(cqe));
			ret = -EINVAL;
		}
	}
out:
	if (wq)
		mtx_unlock(&qhp->lock);
	return ret;
}

int iwch_poll_cq(struct ib_cq *ibcq, int num_entries, struct ib_wc *wc)
{
	struct iwch_dev *rhp;
	struct iwch_cq *chp;
	int npolled;
	int err = 0;

	chp = to_iwch_cq(ibcq);
	rhp = chp->rhp;

	mtx_lock(&chp->lock);
	for (npolled = 0; npolled < num_entries; ++npolled) {
#ifdef DEBUG
		int i=0;
#endif

		/*
		 * Because T3 can post CQEs that are _not_ associated
		 * with a WR, we might have to poll again after removing
		 * one of these.
		 */
		do {
			err = iwch_poll_cq_one(rhp, chp, wc + npolled);
#ifdef DEBUG
			PANIC_IF(++i > 1000);
#endif
		} while (err == -EAGAIN);
		if (err <= 0)
			break;
	}
	mtx_unlock(&chp->lock);

	if (err < 0) {
		return err;
	} else {
		return npolled;
	}
}

