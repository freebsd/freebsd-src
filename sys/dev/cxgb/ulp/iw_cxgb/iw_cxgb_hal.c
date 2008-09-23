
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

#include <vm/vm.h>
#include <vm/pmap.h>

#include <netinet/in.h>

#include <contrib/rdma/ib_verbs.h>

#include <cxgb_include.h>
#include <ulp/iw_cxgb/iw_cxgb_wr.h>
#include <ulp/iw_cxgb/iw_cxgb_hal.h>
#include <ulp/iw_cxgb/iw_cxgb_provider.h>
#include <ulp/iw_cxgb/iw_cxgb_cm.h>
#include <ulp/iw_cxgb/iw_cxgb.h>
#include <ulp/iw_cxgb/iw_cxgb_resource.h>

static TAILQ_HEAD( ,cxio_rdev) rdev_list;
static cxio_hal_ev_callback_func_t cxio_ev_cb = NULL;

static struct cxio_rdev *
cxio_hal_find_rdev_by_name(char *dev_name)
{
	struct cxio_rdev *rdev;

	TAILQ_FOREACH(rdev, &rdev_list, entry)
		if (!strcmp(rdev->dev_name, dev_name))
			return rdev;
	return NULL;
}

struct cxio_rdev *
cxio_hal_find_rdev_by_t3cdev(struct t3cdev *tdev)
{
	struct cxio_rdev *rdev;

	TAILQ_FOREACH(rdev, &rdev_list, entry)
		if (rdev->t3cdev_p == tdev)
			return rdev;
	return NULL;
}

int
cxio_hal_cq_op(struct cxio_rdev *rdev_p, struct t3_cq *cq,
		   enum t3_cq_opcode op, u32 credit)
{
	int ret;
	struct t3_cqe *cqe;
	u32 rptr;

	struct rdma_cq_op setup;
	setup.id = cq->cqid;
	setup.credits = (op == CQ_CREDIT_UPDATE) ? credit : 0;
	setup.op = op;
	ret = rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_CQ_OP, &setup);

	if ((ret < 0) || (op == CQ_CREDIT_UPDATE))
		return (ret);

	/*
	 * If the rearm returned an index other than our current index,
	 * then there might be CQE's in flight (being DMA'd).  We must wait
	 * here for them to complete or the consumer can miss a notification.
	 */
	if (Q_PTR2IDX((cq->rptr), cq->size_log2) != ret) {
		int i=0;

		rptr = cq->rptr;

		/*
		 * Keep the generation correct by bumping rptr until it
		 * matches the index returned by the rearm - 1.
		 */
		while (Q_PTR2IDX((rptr+1), cq->size_log2) != ret)
			rptr++;

		/*
		 * Now rptr is the index for the (last) cqe that was
		 * in-flight at the time the HW rearmed the CQ.  We
		 * spin until that CQE is valid.
		 */
		cqe = cq->queue + Q_PTR2IDX(rptr, cq->size_log2);
		while (!CQ_VLD_ENTRY(rptr, cq->size_log2, cqe)) {
			DELAY(1);
			if (i++ > 1000000) {
				PANIC_IF(1);
				log(LOG_ERR, "%s: stalled rnic\n",
				       rdev_p->dev_name);
				return (-EIO);
			}
		}

		return 1;
	}

	return 0;
}

static int
cxio_hal_clear_cq_ctx(struct cxio_rdev *rdev_p, u32 cqid)
{
	struct rdma_cq_setup setup;
	setup.id = cqid;
	setup.base_addr = 0;	/* NULL address */
	setup.size = 0;		/* disaable the CQ */
	setup.credits = 0;
	setup.credit_thres = 0;
	setup.ovfl_mode = 0;
	return (rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_CQ_SETUP, &setup));
}

static int
cxio_hal_clear_qp_ctx(struct cxio_rdev *rdev_p, u32 qpid)
{
	u64 sge_cmd;
	struct t3_modify_qp_wr *wqe;
	struct mbuf *m = m_gethdr(MT_DATA, M_NOWAIT);
	if (m == NULL) {
		CTR1(KTR_IW_CXGB, "%s m_gethdr failed", __FUNCTION__);
		return (-ENOMEM);
	}
	wqe = mtod(m, struct t3_modify_qp_wr *);
	m->m_len = m->m_pkthdr.len = sizeof(*wqe);
	memset(wqe, 0, sizeof(*wqe));
	build_fw_riwrh((struct fw_riwrh *) wqe, T3_WR_QP_MOD, 3, 0, qpid, 7);
	wqe->flags = htobe32(MODQP_WRITE_EC);
	sge_cmd = qpid << 8 | 3;
	wqe->sge_cmd = htobe64(sge_cmd);
	m_set_priority(m, CPL_PRIORITY_CONTROL);
	m_set_sgl(m, NULL);
	m_set_sgllen(m, 0);
	return (cxgb_ofld_send(rdev_p->t3cdev_p, m));
}

int
cxio_create_cq(struct cxio_rdev *rdev_p, struct t3_cq *cq)
{
	struct rdma_cq_setup setup;
	int size = (1UL << (cq->size_log2)) * sizeof(struct t3_cqe);
	
	cq->cqid = cxio_hal_get_cqid(rdev_p->rscp);
	if (!cq->cqid)
		return (-ENOMEM);
	cq->sw_queue = malloc(size, M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!cq->sw_queue)
		return (-ENOMEM);
#if 0	
	cq->queue = dma_alloc_coherent(rdev_p->rnic_info.pdev,
					     (1UL << (cq->size_log2)) *
					     sizeof(struct t3_cqe),
					     &(cq->dma_addr), M_NOWAIT);
#else
	cq->queue = contigmalloc((1UL << (cq->size_log2))*sizeof(struct t3_cqe),
	    M_DEVBUF, M_NOWAIT, 0ul, ~0ul, 4096, 0);
	if (cq->queue)
		cq->dma_addr = vtophys(cq->queue);
	else {
		free(cq->sw_queue, M_DEVBUF);
		return (-ENOMEM);
	}
#endif
	
#ifdef notyet	
	pci_unmap_addr_set(cq, mapping, cq->dma_addr);
#endif
	memset(cq->queue, 0, size);
	setup.id = cq->cqid;
	setup.base_addr = (u64) (cq->dma_addr);
	setup.size = 1UL << cq->size_log2;
	setup.credits = 65535;
	setup.credit_thres = 1;
	if (rdev_p->t3cdev_p->type != T3A)
		setup.ovfl_mode = 0;
	else
		setup.ovfl_mode = 1;
	return (rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_CQ_SETUP, &setup));
}

int
cxio_resize_cq(struct cxio_rdev *rdev_p, struct t3_cq *cq)
{
	struct rdma_cq_setup setup;
	setup.id = cq->cqid;
	setup.base_addr = (u64) (cq->dma_addr);
	setup.size = 1UL << cq->size_log2;
	setup.credits = setup.size;
	setup.credit_thres = setup.size;	/* TBD: overflow recovery */
	setup.ovfl_mode = 1;
	return (rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_CQ_SETUP, &setup));
}

static u32
get_qpid(struct cxio_rdev *rdev_p, struct cxio_ucontext *uctx)
{
	struct cxio_qpid *entry;
	u32 qpid;
	int i;

	mtx_lock(&uctx->lock);
	if (!TAILQ_EMPTY(&uctx->qpids)) {
		
		entry = TAILQ_FIRST(&uctx->qpids);
		TAILQ_REMOVE(&uctx->qpids, entry, entry);
		qpid = entry->qpid;
		free(entry, M_DEVBUF);
	} else {
		qpid = cxio_hal_get_qpid(rdev_p->rscp);
		if (!qpid)
			goto out;
		for (i = qpid+1; i & rdev_p->qpmask; i++) {
			entry = malloc(sizeof *entry, M_DEVBUF, M_NOWAIT);
			if (!entry)
				break;
			entry->qpid = i;
			TAILQ_INSERT_TAIL(&uctx->qpids, entry, entry);
		}
	}
out:
	mtx_unlock(&uctx->lock);
	CTR2(KTR_IW_CXGB, "%s qpid 0x%x", __FUNCTION__, qpid);
	return qpid;
}

static void
put_qpid(struct cxio_rdev *rdev_p, u32 qpid,
		     struct cxio_ucontext *uctx)
{
	struct cxio_qpid *entry;

	entry = malloc(sizeof *entry, M_DEVBUF, M_NOWAIT);
	CTR2(KTR_IW_CXGB, "%s qpid 0x%x", __FUNCTION__, qpid);
	entry->qpid = qpid;
	mtx_lock(&uctx->lock);
	TAILQ_INSERT_TAIL(&uctx->qpids, entry, entry);
	mtx_unlock(&uctx->lock);
}

void
cxio_release_ucontext(struct cxio_rdev *rdev_p, struct cxio_ucontext *uctx)
{
	struct cxio_qpid *pos, *tmp;

	mtx_lock(&uctx->lock);
	TAILQ_FOREACH_SAFE(pos, &uctx->qpids, entry, tmp) {
		TAILQ_REMOVE(&uctx->qpids, pos, entry);
		if (!(pos->qpid & rdev_p->qpmask))
			cxio_hal_put_qpid(rdev_p->rscp, pos->qpid);
		free(pos, M_DEVBUF);
	}
	mtx_unlock(&uctx->lock);
}

void
cxio_init_ucontext(struct cxio_rdev *rdev_p, struct cxio_ucontext *uctx)
{
	TAILQ_INIT(&uctx->qpids);
	mtx_init(&uctx->lock, "cxio uctx", NULL, MTX_DEF|MTX_DUPOK);
}

int
cxio_create_qp(struct cxio_rdev *rdev_p, u32 kernel_domain,
    struct t3_wq *wq, struct cxio_ucontext *uctx)
{
	int depth = 1UL << wq->size_log2;
	int rqsize = 1UL << wq->rq_size_log2;

	wq->qpid = get_qpid(rdev_p, uctx);
	if (!wq->qpid)
		return (-ENOMEM);

	wq->rq = malloc(depth * sizeof(u64), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!wq->rq)
		goto err1;

	wq->rq_addr = cxio_hal_rqtpool_alloc(rdev_p, rqsize);
	if (!wq->rq_addr)
		goto err2;

	wq->sq = malloc(depth * sizeof(struct t3_swsq), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!wq->sq)
		goto err3;
#if 0
	wq->queue = dma_alloc_coherent(rdev_p->rnic_info.pdev,
					     depth * sizeof(union t3_wr),
					     &(wq->dma_addr), M_NOWAIT);
#else
	wq->queue = contigmalloc(depth *sizeof(union t3_wr),
	    M_DEVBUF, M_NOWAIT, 0ul, ~0ul, 4096, 0);
	if (wq->queue)
		wq->dma_addr = vtophys(wq->queue);

#endif
	if (!wq->queue)
		goto err4;

	memset(wq->queue, 0, depth * sizeof(union t3_wr));
#ifdef notyet	
	pci_unmap_addr_set(wq, mapping, wq->dma_addr);
#endif
	wq->doorbell = rdev_p->rnic_info.kdb_addr;
	if (!kernel_domain)
		wq->udb = (u64)rdev_p->rnic_info.udbell_physbase +
					(wq->qpid << rdev_p->qpshift);
	CTR4(KTR_IW_CXGB, "%s qpid 0x%x doorbell 0x%p udb 0x%llx", __FUNCTION__,
	     wq->qpid, wq->doorbell, (unsigned long long) wq->udb);
	return 0;
err4:
	free(wq->sq, M_DEVBUF);
err3:
	cxio_hal_rqtpool_free(rdev_p, wq->rq_addr, rqsize);
err2:
	free(wq->rq, M_DEVBUF);
err1:
	put_qpid(rdev_p, wq->qpid, uctx);
	return (-ENOMEM);
}

int
cxio_destroy_cq(struct cxio_rdev *rdev_p, struct t3_cq *cq)
{
	int err;
	err = cxio_hal_clear_cq_ctx(rdev_p, cq->cqid);
	free(cq->sw_queue, M_DEVBUF);
#if 0	
	dma_free_coherent(&(rdev_p->rnic_info.pdev),
	    (1UL << (cq->size_log2))
			  * sizeof(struct t3_cqe), cq->queue,
	    /* pci_unmap_addr(cq, mapping)*/  0);
#else
	contigfree(cq->queue,(1UL << (cq->size_log2))
	    * sizeof(struct t3_cqe), M_DEVBUF);
#endif	
	cxio_hal_put_cqid(rdev_p->rscp, cq->cqid);
	return err;
}

int
cxio_destroy_qp(struct cxio_rdev *rdev_p, struct t3_wq *wq,
    struct cxio_ucontext *uctx)
{

#if 0
	dma_free_coherent(&(rdev_p->rnic_info.pdev),
			  (1UL << (wq->size_log2))
			  * sizeof(union t3_wr), wq->queue,
	    /* pci_unmap_addr(wq, mapping)*/ 0);
#else	
	contigfree(wq->queue, (1UL << (wq->size_log2))
	    * sizeof(union t3_wr), M_DEVBUF);
#endif	
	free(wq->sq, M_DEVBUF);
	cxio_hal_rqtpool_free(rdev_p, wq->rq_addr, (1UL << wq->rq_size_log2));
	free(wq->rq, M_DEVBUF);
	put_qpid(rdev_p, wq->qpid, uctx);
	return 0;
}

static void
insert_recv_cqe(struct t3_wq *wq, struct t3_cq *cq)
{
	struct t3_cqe cqe;

	CTR5(KTR_IW_CXGB, "%s wq %p cq %p sw_rptr 0x%x sw_wptr 0x%x", __FUNCTION__,
	     wq, cq, cq->sw_rptr, cq->sw_wptr);
	memset(&cqe, 0, sizeof(cqe));
	cqe.header = htobe32(V_CQE_STATUS(TPT_ERR_SWFLUSH) |
			         V_CQE_OPCODE(T3_SEND) |
				 V_CQE_TYPE(0) |
				 V_CQE_SWCQE(1) |
				 V_CQE_QPID(wq->qpid) |
				 V_CQE_GENBIT(Q_GENBIT(cq->sw_wptr,
						       cq->size_log2)));
	*(cq->sw_queue + Q_PTR2IDX(cq->sw_wptr, cq->size_log2)) = cqe;
	cq->sw_wptr++;
}

void
cxio_flush_rq(struct t3_wq *wq, struct t3_cq *cq, int count)
{
	u32 ptr;

	CTR3(KTR_IW_CXGB, "%s wq %p cq %p", __FUNCTION__, wq, cq);

	/* flush RQ */
	CTR4(KTR_IW_CXGB, "%s rq_rptr %u rq_wptr %u skip count %u", __FUNCTION__,
	    wq->rq_rptr, wq->rq_wptr, count);
	ptr = wq->rq_rptr + count;
	while (ptr++ != wq->rq_wptr)
		insert_recv_cqe(wq, cq);
}

static void
insert_sq_cqe(struct t3_wq *wq, struct t3_cq *cq,
		          struct t3_swsq *sqp)
{
	struct t3_cqe cqe;

	CTR5(KTR_IW_CXGB, "%s wq %p cq %p sw_rptr 0x%x sw_wptr 0x%x", __FUNCTION__,
	     wq, cq, cq->sw_rptr, cq->sw_wptr);
	memset(&cqe, 0, sizeof(cqe));
	cqe.header = htobe32(V_CQE_STATUS(TPT_ERR_SWFLUSH) |
			         V_CQE_OPCODE(sqp->opcode) |
			         V_CQE_TYPE(1) |
			         V_CQE_SWCQE(1) |
			         V_CQE_QPID(wq->qpid) |
			         V_CQE_GENBIT(Q_GENBIT(cq->sw_wptr,
						       cq->size_log2)));
	cqe.u.scqe.wrid_hi = sqp->sq_wptr;

	*(cq->sw_queue + Q_PTR2IDX(cq->sw_wptr, cq->size_log2)) = cqe;
	cq->sw_wptr++;
}

void
cxio_flush_sq(struct t3_wq *wq, struct t3_cq *cq, int count)
{
	__u32 ptr;
	struct t3_swsq *sqp = wq->sq + Q_PTR2IDX(wq->sq_rptr, wq->sq_size_log2);

	ptr = wq->sq_rptr + count;
	sqp += count;
	while (ptr != wq->sq_wptr) {
		insert_sq_cqe(wq, cq, sqp);
		sqp++;
		ptr++;
	}
}

/*
 * Move all CQEs from the HWCQ into the SWCQ.
 */
void
cxio_flush_hw_cq(struct t3_cq *cq)
{
	struct t3_cqe *cqe, *swcqe;

	CTR3(KTR_IW_CXGB, "%s cq %p cqid 0x%x", __FUNCTION__, cq, cq->cqid);
	cqe = cxio_next_hw_cqe(cq);
	while (cqe) {
		CTR3(KTR_IW_CXGB, "%s flushing hwcq rptr 0x%x to swcq wptr 0x%x",
		     __FUNCTION__, cq->rptr, cq->sw_wptr);
		swcqe = cq->sw_queue + Q_PTR2IDX(cq->sw_wptr, cq->size_log2);
		*swcqe = *cqe;
		swcqe->header |= htobe32(V_CQE_SWCQE(1));
		cq->sw_wptr++;
		cq->rptr++;
		cqe = cxio_next_hw_cqe(cq);
	}
}

static int cqe_completes_wr(struct t3_cqe *cqe, struct t3_wq *wq)
{
	if (CQE_OPCODE(*cqe) == T3_TERMINATE)
		return 0;

	if ((CQE_OPCODE(*cqe) == T3_RDMA_WRITE) && RQ_TYPE(*cqe))
		return 0;

	if ((CQE_OPCODE(*cqe) == T3_READ_RESP) && SQ_TYPE(*cqe))
		return 0;

	if ((CQE_OPCODE(*cqe) == T3_SEND) && RQ_TYPE(*cqe) &&
	    Q_EMPTY(wq->rq_rptr, wq->rq_wptr))
		return 0;

	return 1;
}

void
cxio_count_scqes(struct t3_cq *cq, struct t3_wq *wq, int *count)
{
	struct t3_cqe *cqe;
	u32 ptr;

	*count = 0;
	ptr = cq->sw_rptr;
	while (!Q_EMPTY(ptr, cq->sw_wptr)) {
		cqe = cq->sw_queue + (Q_PTR2IDX(ptr, cq->size_log2));
		if ((SQ_TYPE(*cqe) || (CQE_OPCODE(*cqe) == T3_READ_RESP)) &&
		    (CQE_QPID(*cqe) == wq->qpid))
			(*count)++;
		ptr++;
	}
	CTR3(KTR_IW_CXGB, "%s cq %p count %d", __FUNCTION__, cq, *count);
}

void
cxio_count_rcqes(struct t3_cq *cq, struct t3_wq *wq, int *count)
{
	struct t3_cqe *cqe;
	u32 ptr;

	*count = 0;
	CTR2(KTR_IW_CXGB, "%s count zero %d", __FUNCTION__, *count);
	ptr = cq->sw_rptr;
	while (!Q_EMPTY(ptr, cq->sw_wptr)) {
		cqe = cq->sw_queue + (Q_PTR2IDX(ptr, cq->size_log2));
		if (RQ_TYPE(*cqe) && (CQE_OPCODE(*cqe) != T3_READ_RESP) &&
		    (CQE_QPID(*cqe) == wq->qpid) && cqe_completes_wr(cqe, wq))
			(*count)++;
		ptr++;
	}
	CTR3(KTR_IW_CXGB, "%s cq %p count %d", __FUNCTION__, cq, *count);
}

static int
cxio_hal_init_ctrl_cq(struct cxio_rdev *rdev_p)
{
	struct rdma_cq_setup setup;
	setup.id = 0;
	setup.base_addr = 0;	/* NULL address */
	setup.size = 1;		/* enable the CQ */
	setup.credits = 0;

	/* force SGE to redirect to RspQ and interrupt */
	setup.credit_thres = 0;
	setup.ovfl_mode = 1;
	return (rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_CQ_SETUP, &setup));
}

static int
cxio_hal_init_ctrl_qp(struct cxio_rdev *rdev_p)
{
	int err;
	u64 sge_cmd, ctx0, ctx1;
	u64 base_addr;
	struct t3_modify_qp_wr *wqe;
	struct mbuf *m;

	m = m_gethdr(MT_DATA, M_NOWAIT);
	if (m == NULL) {
		CTR1(KTR_IW_CXGB, "%s m_gethdr failed", __FUNCTION__);
		return (-ENOMEM);
	}
	err = cxio_hal_init_ctrl_cq(rdev_p);
	if (err) {
		CTR2(KTR_IW_CXGB, "%s err %d initializing ctrl_cq", __FUNCTION__, err);
		goto err;
	}
#if 0	
	rdev_p->ctrl_qp.workq = dma_alloc_coherent(
		rdev_p->rnic_info.pdev,
		    (1 << T3_CTRL_QP_SIZE_LOG2) *
		    sizeof(union t3_wr),
		    &(rdev_p->ctrl_qp.dma_addr),
		    M_NOWAIT);
#else
	rdev_p->ctrl_qp.workq = contigmalloc((1 << T3_CTRL_QP_SIZE_LOG2) 
	    *sizeof(union t3_wr), M_DEVBUF, M_NOWAIT, 0ul, ~0ul, 4096, 0);
	if (rdev_p->ctrl_qp.workq)
		rdev_p->ctrl_qp.dma_addr = vtophys(rdev_p->ctrl_qp.workq);

#endif	
	
	if (!rdev_p->ctrl_qp.workq) {
		CTR1(KTR_IW_CXGB, "%s dma_alloc_coherent failed", __FUNCTION__);
		err = -ENOMEM;
		goto err;
	}
#if 0	
	pci_unmap_addr_set(&rdev_p->ctrl_qp, mapping,
			   rdev_p->ctrl_qp.dma_addr);
#endif	
	rdev_p->ctrl_qp.doorbell = (void /*__iomem */ *)rdev_p->rnic_info.kdb_addr;
	memset(rdev_p->ctrl_qp.workq, 0,
	       (1 << T3_CTRL_QP_SIZE_LOG2) * sizeof(union t3_wr));

	mtx_init(&rdev_p->ctrl_qp.lock, "ctl-qp lock", NULL, MTX_DEF|MTX_DUPOK);

	/* update HW Ctrl QP context */
	base_addr = rdev_p->ctrl_qp.dma_addr;
	base_addr >>= 12;
	ctx0 = (V_EC_SIZE((1 << T3_CTRL_QP_SIZE_LOG2)) |
		V_EC_BASE_LO((u32) base_addr & 0xffff));
	ctx0 <<= 32;
	ctx0 |= V_EC_CREDITS(FW_WR_NUM);
	base_addr >>= 16;
	ctx1 = (u32) base_addr;
	base_addr >>= 32;
	ctx1 |= ((u64) (V_EC_BASE_HI((u32) base_addr & 0xf) | V_EC_RESPQ(0) |
			V_EC_TYPE(0) | V_EC_GEN(1) |
			V_EC_UP_TOKEN(T3_CTL_QP_TID) | F_EC_VALID)) << 32;
	wqe = mtod(m, struct t3_modify_qp_wr *);
	m->m_len = m->m_pkthdr.len = sizeof(*wqe);
	memset(wqe, 0, sizeof(*wqe));
	build_fw_riwrh((struct fw_riwrh *) wqe, T3_WR_QP_MOD, 3, 0,
		       T3_CTL_QP_TID, 7);
	wqe->flags = htobe32(MODQP_WRITE_EC);
	sge_cmd = (3ULL << 56) | FW_RI_SGEEC_START << 8 | 3;
	wqe->sge_cmd = htobe64(sge_cmd);
	wqe->ctx1 = htobe64(ctx1);
	wqe->ctx0 = htobe64(ctx0);
	CTR3(KTR_IW_CXGB, "CtrlQP dma_addr 0x%llx workq %p size %d",
	     (unsigned long long) rdev_p->ctrl_qp.dma_addr,
	     rdev_p->ctrl_qp.workq, 1 << T3_CTRL_QP_SIZE_LOG2);
	m_set_priority(m, CPL_PRIORITY_CONTROL);
	m_set_sgl(m, NULL);
	m_set_sgllen(m, 0);
	return (cxgb_ofld_send(rdev_p->t3cdev_p, m));
err:
	m_free(m);
	return err;
}

static int
cxio_hal_destroy_ctrl_qp(struct cxio_rdev *rdev_p)
{
#if 0
	
	dma_free_coherent(&(rdev_p->rnic_info.pdev),
			  (1UL << T3_CTRL_QP_SIZE_LOG2)
			  * sizeof(union t3_wr), rdev_p->ctrl_qp.workq,
	    /* pci_unmap_addr(&rdev_p->ctrl_qp, mapping)*/ 0);
#else
	contigfree(rdev_p->ctrl_qp.workq,(1UL << T3_CTRL_QP_SIZE_LOG2)
	    * sizeof(union t3_wr), M_DEVBUF);
#endif
	return cxio_hal_clear_qp_ctx(rdev_p, T3_CTRL_QP_ID);
}

/* write len bytes of data into addr (32B aligned address)
 * If data is NULL, clear len byte of memory to zero.
 * caller aquires the ctrl_qp lock before the call
 */
static int
cxio_hal_ctrl_qp_write_mem(struct cxio_rdev *rdev_p, u32 addr,
				      u32 len, void *data, int completion)
{
	u32 i, nr_wqe, copy_len;
	u8 *copy_data;
	u8 wr_len, utx_len;	/* lenght in 8 byte flit */
	enum t3_wr_flags flag;
	__be64 *wqe;
	u64 utx_cmd;
	addr &= 0x7FFFFFF;
	nr_wqe = len % 96 ? len / 96 + 1 : len / 96;	/* 96B max per WQE */
	CTR6(KTR_IW_CXGB, "cxio_hal_ctrl_qp_write_mem wptr 0x%x rptr 0x%x len %d, nr_wqe %d data %p addr 0x%0x",
	     rdev_p->ctrl_qp.wptr, rdev_p->ctrl_qp.rptr, len,
	     nr_wqe, data, addr);
	utx_len = 3;		/* in 32B unit */
	for (i = 0; i < nr_wqe; i++) {
		if (Q_FULL(rdev_p->ctrl_qp.rptr, rdev_p->ctrl_qp.wptr,
		           T3_CTRL_QP_SIZE_LOG2)) {
			CTR4(KTR_IW_CXGB, "%s ctrl_qp full wtpr 0x%0x rptr 0x%0x, "
			     "wait for more space i %d", __FUNCTION__,
			     rdev_p->ctrl_qp.wptr, rdev_p->ctrl_qp.rptr, i);
			if (cxio_wait(&rdev_p->ctrl_qp,
				&rdev_p->ctrl_qp.lock, 
				!Q_FULL(rdev_p->ctrl_qp.rptr, 
					rdev_p->ctrl_qp.wptr,
					T3_CTRL_QP_SIZE_LOG2))) {
				CTR1(KTR_IW_CXGB, "%s ctrl_qp workq interrupted",
				     __FUNCTION__);
				return (-ERESTART);
			}
			CTR2(KTR_IW_CXGB, "%s ctrl_qp wakeup, continue posting work request "
			     "i %d", __FUNCTION__, i);
		}
		wqe = (__be64 *)(rdev_p->ctrl_qp.workq + (rdev_p->ctrl_qp.wptr %
						(1 << T3_CTRL_QP_SIZE_LOG2)));
		flag = 0;
		if (i == (nr_wqe - 1)) {
			/* last WQE */
			flag = completion ? T3_COMPLETION_FLAG : 0;
			if (len % 32)
				utx_len = len / 32 + 1;
			else
				utx_len = len / 32;
		}

		/*
		 * Force a CQE to return the credit to the workq in case
		 * we posted more than half the max QP size of WRs
		 */
		if ((i != 0) &&
		    (i % (((1 << T3_CTRL_QP_SIZE_LOG2)) >> 1) == 0)) {
			flag = T3_COMPLETION_FLAG;
			CTR2(KTR_IW_CXGB, "%s force completion at i %d", __FUNCTION__, i);
		}

		/* build the utx mem command */
		wqe += (sizeof(struct t3_bypass_wr) >> 3);
		utx_cmd = (T3_UTX_MEM_WRITE << 28) | (addr + i * 3);
		utx_cmd <<= 32;
		utx_cmd |= (utx_len << 28) | ((utx_len << 2) + 1);
		*wqe = htobe64(utx_cmd);
		wqe++;
		copy_data = (u8 *) data + i * 96;
		copy_len = len > 96 ? 96 : len;

		/* clear memory content if data is NULL */
		if (data)
			memcpy(wqe, copy_data, copy_len);
		else
			memset(wqe, 0, copy_len);
		if (copy_len % 32)
			memset(((u8 *) wqe) + copy_len, 0,
			       32 - (copy_len % 32));
		wr_len = ((sizeof(struct t3_bypass_wr)) >> 3) + 1 +
			 (utx_len << 2);
		wqe = (__be64 *)(rdev_p->ctrl_qp.workq + (rdev_p->ctrl_qp.wptr %
			      (1 << T3_CTRL_QP_SIZE_LOG2)));

		/* wptr in the WRID[31:0] */
		((union t3_wrid *)(wqe+1))->id0.low = rdev_p->ctrl_qp.wptr;

		/*
		 * This must be the last write with a memory barrier
		 * for the genbit
		 */
		build_fw_riwrh((struct fw_riwrh *) wqe, T3_WR_BP, flag,
			       Q_GENBIT(rdev_p->ctrl_qp.wptr,
					T3_CTRL_QP_SIZE_LOG2), T3_CTRL_QP_ID,
			       wr_len);
		if (flag == T3_COMPLETION_FLAG)
			ring_doorbell(rdev_p->ctrl_qp.doorbell, T3_CTRL_QP_ID);

		len -= 96;
		rdev_p->ctrl_qp.wptr++;
	}
	return 0;
}

/* IN: stag key, pdid, perm, zbva, to, len, page_size, pbl, and pbl_size
 * OUT: stag index, actual pbl_size, pbl_addr allocated.
 * TBD: shared memory region support
 */
static int
__cxio_tpt_op(struct cxio_rdev *rdev_p, u32 reset_tpt_entry,
			 u32 *stag, u8 stag_state, u32 pdid,
			 enum tpt_mem_type type, enum tpt_mem_perm perm,
			 u32 zbva, u64 to, u32 len, u8 page_size, __be64 *pbl,
			 u32 *pbl_size, u32 *pbl_addr)
{
	int err;
	struct tpt_entry tpt;
	u32 stag_idx;
	u32 wptr;
	int rereg = (*stag != T3_STAG_UNSET);

	stag_state = stag_state > 0;
	stag_idx = (*stag) >> 8;

	if ((!reset_tpt_entry) && !(*stag != T3_STAG_UNSET)) {
		stag_idx = cxio_hal_get_stag(rdev_p->rscp);
		if (!stag_idx)
			return (-ENOMEM);
		*stag = (stag_idx << 8) | ((*stag) & 0xFF);
	}
	CTR5(KTR_IW_CXGB, "%s stag_state 0x%0x type 0x%0x pdid 0x%0x, stag_idx 0x%x",
	     __FUNCTION__, stag_state, type, pdid, stag_idx);

	if (reset_tpt_entry)
		cxio_hal_pblpool_free(rdev_p, *pbl_addr, *pbl_size << 3);
	else if (!rereg) {
		*pbl_addr = cxio_hal_pblpool_alloc(rdev_p, *pbl_size << 3);
		if (!*pbl_addr) {
			return (-ENOMEM);
		}
	}

	mtx_lock(&rdev_p->ctrl_qp.lock);

	/* write PBL first if any - update pbl only if pbl list exist */
	if (pbl) {

		CTR4(KTR_IW_CXGB, "%s *pdb_addr 0x%x, pbl_base 0x%x, pbl_size %d",
		     __FUNCTION__, *pbl_addr, rdev_p->rnic_info.pbl_base,
		     *pbl_size);
		err = cxio_hal_ctrl_qp_write_mem(rdev_p,
				(*pbl_addr >> 5),
				(*pbl_size << 3), pbl, 0);
		if (err)
			goto ret;
	}

	/* write TPT entry */
	if (reset_tpt_entry)
		memset(&tpt, 0, sizeof(tpt));
	else {
		tpt.valid_stag_pdid = htobe32(F_TPT_VALID |
				V_TPT_STAG_KEY((*stag) & M_TPT_STAG_KEY) |
				V_TPT_STAG_STATE(stag_state) |
				V_TPT_STAG_TYPE(type) | V_TPT_PDID(pdid));
		PANIC_IF(page_size >= 28);
		tpt.flags_pagesize_qpid = htobe32(V_TPT_PERM(perm) |
				F_TPT_MW_BIND_ENABLE |
				V_TPT_ADDR_TYPE((zbva ? TPT_ZBTO : TPT_VATO)) |
				V_TPT_PAGE_SIZE(page_size));
		tpt.rsvd_pbl_addr = reset_tpt_entry ? 0 :
				    htobe32(V_TPT_PBL_ADDR(PBL_OFF(rdev_p, *pbl_addr)>>3));
		tpt.len = htobe32(len);
		tpt.va_hi = htobe32((u32) (to >> 32));
		tpt.va_low_or_fbo = htobe32((u32) (to & 0xFFFFFFFFULL));
		tpt.rsvd_bind_cnt_or_pstag = 0;
		tpt.rsvd_pbl_size = reset_tpt_entry ? 0 :
				  htobe32(V_TPT_PBL_SIZE((*pbl_size) >> 2));
	}
	err = cxio_hal_ctrl_qp_write_mem(rdev_p,
				       stag_idx +
				       (rdev_p->rnic_info.tpt_base >> 5),
				       sizeof(tpt), &tpt, 1);

	/* release the stag index to free pool */
	if (reset_tpt_entry)
		cxio_hal_put_stag(rdev_p->rscp, stag_idx);
ret:
	wptr = rdev_p->ctrl_qp.wptr;
	mtx_unlock(&rdev_p->ctrl_qp.lock);
	if (!err)
		if (cxio_wait(&rdev_p->ctrl_qp, 
			&rdev_p->ctrl_qp.lock,
			SEQ32_GE(rdev_p->ctrl_qp.rptr, wptr)))
			return (-ERESTART);
	return err;
}

int
cxio_register_phys_mem(struct cxio_rdev *rdev_p, u32 *stag, u32 pdid,
			   enum tpt_mem_perm perm, u32 zbva, u64 to, u32 len,
			   u8 page_size, __be64 *pbl, u32 *pbl_size,
			   u32 *pbl_addr)
{
	*stag = T3_STAG_UNSET;
	return __cxio_tpt_op(rdev_p, 0, stag, 1, pdid, TPT_NON_SHARED_MR, perm,
			     zbva, to, len, page_size, pbl, pbl_size, pbl_addr);
}

int
cxio_reregister_phys_mem(struct cxio_rdev *rdev_p, u32 *stag, u32 pdid,
			   enum tpt_mem_perm perm, u32 zbva, u64 to, u32 len,
			   u8 page_size, __be64 *pbl, u32 *pbl_size,
			   u32 *pbl_addr)
{
	return __cxio_tpt_op(rdev_p, 0, stag, 1, pdid, TPT_NON_SHARED_MR, perm,
			     zbva, to, len, page_size, pbl, pbl_size, pbl_addr);
}

int
cxio_dereg_mem(struct cxio_rdev *rdev_p, u32 stag, u32 pbl_size,
		   u32 pbl_addr)
{
	return __cxio_tpt_op(rdev_p, 1, &stag, 0, 0, 0, 0, 0, 0ULL, 0, 0, NULL,
			     &pbl_size, &pbl_addr);
}

int
cxio_allocate_window(struct cxio_rdev *rdev_p, u32 * stag, u32 pdid)
{
	u32 pbl_size = 0;
	*stag = T3_STAG_UNSET;
	return __cxio_tpt_op(rdev_p, 0, stag, 0, pdid, TPT_MW, 0, 0, 0ULL, 0, 0,
			     NULL, &pbl_size, NULL);
}

int
cxio_deallocate_window(struct cxio_rdev *rdev_p, u32 stag)
{
	return __cxio_tpt_op(rdev_p, 1, &stag, 0, 0, 0, 0, 0, 0ULL, 0, 0, NULL,
			     NULL, NULL);
}

int
cxio_rdma_init(struct cxio_rdev *rdev_p, struct t3_rdma_init_attr *attr)
{
	struct t3_rdma_init_wr *wqe;
	struct mbuf *m = m_gethdr(MT_DATA, M_NOWAIT);
	if (m == NULL)
		return (-ENOMEM);
	CTR2(KTR_IW_CXGB, "%s rdev_p %p", __FUNCTION__, rdev_p);
	wqe = mtod(m, struct t3_rdma_init_wr *);
	m->m_len = m->m_pkthdr.len = sizeof(*wqe);
	wqe->wrh.op_seop_flags = htobe32(V_FW_RIWR_OP(T3_WR_INIT));
	wqe->wrh.gen_tid_len = htobe32(V_FW_RIWR_TID(attr->tid) |
					   V_FW_RIWR_LEN(sizeof(*wqe) >> 3));
	wqe->wrid.id1 = 0;
	wqe->qpid = htobe32(attr->qpid);
	wqe->pdid = htobe32(attr->pdid);
	wqe->scqid = htobe32(attr->scqid);
	wqe->rcqid = htobe32(attr->rcqid);
	wqe->rq_addr = htobe32(attr->rq_addr - rdev_p->rnic_info.rqt_base);
	wqe->rq_size = htobe32(attr->rq_size);
	wqe->mpaattrs = attr->mpaattrs;
	wqe->qpcaps = attr->qpcaps;
	wqe->ulpdu_size = htobe16(attr->tcp_emss);
	wqe->flags = htobe32(attr->flags);
	wqe->ord = htobe32(attr->ord);
	wqe->ird = htobe32(attr->ird);
	wqe->qp_dma_addr = htobe64(attr->qp_dma_addr);
	wqe->qp_dma_size = htobe32(attr->qp_dma_size);
	wqe->irs = htobe32(attr->irs);
	m_set_priority(m, 0);	/* 0=>ToeQ; 1=>CtrlQ */
	m_set_sgl(m, NULL);
	m_set_sgllen(m, 0);
	return (cxgb_ofld_send(rdev_p->t3cdev_p, m));
}

void
cxio_register_ev_cb(cxio_hal_ev_callback_func_t ev_cb)
{
	cxio_ev_cb = ev_cb;
}

void
cxio_unregister_ev_cb(cxio_hal_ev_callback_func_t ev_cb)
{
	cxio_ev_cb = NULL;
}

static int
cxio_hal_ev_handler(struct t3cdev *t3cdev_p, struct mbuf *m)
{
	static int cnt;
	struct cxio_rdev *rdev_p = NULL;
	struct respQ_msg_t *rsp_msg = (struct respQ_msg_t *) m->m_data;
	
	CTR6(KTR_IW_CXGB, "%s cq_id 0x%x cq_ptr 0x%x genbit %0x overflow %0x an %0x",
	     __FUNCTION__, RSPQ_CQID(rsp_msg), RSPQ_CQPTR(rsp_msg),
	     RSPQ_GENBIT(rsp_msg), RSPQ_OVERFLOW(rsp_msg), RSPQ_AN(rsp_msg));
	CTR4(KTR_IW_CXGB, "se %0x notify %0x cqbranch %0x creditth %0x",
	     RSPQ_SE(rsp_msg), RSPQ_NOTIFY(rsp_msg), RSPQ_CQBRANCH(rsp_msg),
	     RSPQ_CREDIT_THRESH(rsp_msg));
	CTR4(KTR_IW_CXGB, "CQE: QPID 0x%0x type 0x%0x status 0x%0x opcode %d",
	     CQE_QPID(rsp_msg->cqe), 
	     CQE_TYPE(rsp_msg->cqe), CQE_STATUS(rsp_msg->cqe),
	     CQE_OPCODE(rsp_msg->cqe));
	CTR3(KTR_IW_CXGB, "len 0x%0x wrid_hi_stag 0x%x wrid_low_msn 0x%x",
	     CQE_LEN(rsp_msg->cqe), CQE_WRID_HI(rsp_msg->cqe), CQE_WRID_LOW(rsp_msg->cqe));
	rdev_p = (struct cxio_rdev *)t3cdev_p->ulp;
	if (!rdev_p) {
		CTR2(KTR_IW_CXGB, "%s called by t3cdev %p with null ulp", __FUNCTION__,
		     t3cdev_p);
		return 0;
	}
	if (CQE_QPID(rsp_msg->cqe) == T3_CTRL_QP_ID) {
		mtx_lock(&rdev_p->ctrl_qp.lock);
		rdev_p->ctrl_qp.rptr = CQE_WRID_LOW(rsp_msg->cqe) + 1;
		wakeup(&rdev_p->ctrl_qp);
		mtx_unlock(&rdev_p->ctrl_qp.lock);
		m_free(m);
	} else if (CQE_QPID(rsp_msg->cqe) == 0xfff8)
		m_free(m);
	else if (cxio_ev_cb)
		(*cxio_ev_cb) (rdev_p, m);
	else
		m_free(m);
	cnt++;
	return 0;
}

/* Caller takes care of locking if needed */
int
cxio_rdev_open(struct cxio_rdev *rdev_p)
{
	struct ifnet *ifp;
	int err = 0;

	if (strlen(rdev_p->dev_name)) {
		if (cxio_hal_find_rdev_by_name(rdev_p->dev_name)) {
			return (-EBUSY);
		}
		ifp = rdev_p->ifp; 
		if (ifp == NULL) 
			return (-EINVAL);
		if_free(ifp);
	} else if (rdev_p->t3cdev_p) {
		if (cxio_hal_find_rdev_by_t3cdev(rdev_p->t3cdev_p)) 
			return (-EBUSY);
		ifp = rdev_p->t3cdev_p->lldev;
		strncpy(rdev_p->dev_name, rdev_p->t3cdev_p->name,
			T3_MAX_DEV_NAME_LEN);
	} else {
		CTR1(KTR_IW_CXGB, "%s t3cdev_p or dev_name must be set", __FUNCTION__);
		return (-EINVAL);
	}

	TAILQ_INSERT_TAIL(&rdev_list, rdev_p, entry);

	CTR2(KTR_IW_CXGB, "%s opening rnic dev %s", __FUNCTION__, rdev_p->dev_name);
	memset(&rdev_p->ctrl_qp, 0, sizeof(rdev_p->ctrl_qp));
	if (!rdev_p->t3cdev_p)
		rdev_p->t3cdev_p = T3CDEV(ifp);
	rdev_p->t3cdev_p->ulp = (void *) rdev_p;
	err = rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, RDMA_GET_PARAMS,
					 &(rdev_p->rnic_info));
	if (err) {
		log(LOG_ERR, "%s t3cdev_p(%p)->ctl returned error %d.\n",
		     __FUNCTION__, rdev_p->t3cdev_p, err);
		goto err1;
	}
	err = rdev_p->t3cdev_p->ctl(rdev_p->t3cdev_p, GET_PORTS,
				    &(rdev_p->port_info));
	if (err) {
		log(LOG_ERR, "%s t3cdev_p(%p)->ctl returned error %d.\n",
		     __FUNCTION__, rdev_p->t3cdev_p, err);
		goto err1;
	}

	/*
	 * qpshift is the number of bits to shift the qpid left in order
	 * to get the correct address of the doorbell for that qp.
	 */
	cxio_init_ucontext(rdev_p, &rdev_p->uctx);
	rdev_p->qpshift = PAGE_SHIFT -
			  ilog2(65536 >>
			            ilog2(rdev_p->rnic_info.udbell_len >>
					      PAGE_SHIFT));
	rdev_p->qpnr = rdev_p->rnic_info.udbell_len >> PAGE_SHIFT;
	rdev_p->qpmask = (65536 >> ilog2(rdev_p->qpnr)) - 1;
	CTR4(KTR_IW_CXGB, "cxio_rdev_open rnic %s info: tpt_base 0x%0x tpt_top 0x%0x num stags %d",
	     rdev_p->dev_name, rdev_p->rnic_info.tpt_base,
	     rdev_p->rnic_info.tpt_top, cxio_num_stags(rdev_p));
	CTR4(KTR_IW_CXGB, "pbl_base 0x%0x pbl_top 0x%0x rqt_base 0x%0x, rqt_top 0x%0x",
	     rdev_p->rnic_info.pbl_base,
	     rdev_p->rnic_info.pbl_top, rdev_p->rnic_info.rqt_base,
	     rdev_p->rnic_info.rqt_top);
	CTR6(KTR_IW_CXGB, "udbell_len 0x%0x udbell_physbase 0x%lx kdb_addr %p qpshift %lu "
	     "qpnr %d qpmask 0x%x",
	     rdev_p->rnic_info.udbell_len,
	     rdev_p->rnic_info.udbell_physbase, rdev_p->rnic_info.kdb_addr,
	     rdev_p->qpshift, rdev_p->qpnr, rdev_p->qpmask);

	err = cxio_hal_init_ctrl_qp(rdev_p);
	if (err) {
		log(LOG_ERR, "%s error %d initializing ctrl_qp.\n",
		       __FUNCTION__, err);
		goto err1;
	}
	err = cxio_hal_init_resource(rdev_p, cxio_num_stags(rdev_p), 0,
				     0, T3_MAX_NUM_QP, T3_MAX_NUM_CQ,
				     T3_MAX_NUM_PD);
	if (err) {
		log(LOG_ERR, "%s error %d initializing hal resources.\n",
		       __FUNCTION__, err);
		goto err2;
	}
	err = cxio_hal_pblpool_create(rdev_p);
	if (err) {
		log(LOG_ERR, "%s error %d initializing pbl mem pool.\n",
		       __FUNCTION__, err);
		goto err3;
	}
	err = cxio_hal_rqtpool_create(rdev_p);
	if (err) {
		log(LOG_ERR, "%s error %d initializing rqt mem pool.\n",
		       __FUNCTION__, err);
		goto err4;
	}
	return 0;
err4:
	cxio_hal_pblpool_destroy(rdev_p);
err3:
	cxio_hal_destroy_resource(rdev_p->rscp);
err2:
	cxio_hal_destroy_ctrl_qp(rdev_p);
err1:
	TAILQ_REMOVE(&rdev_list, rdev_p, entry);
	return err;
}

void
cxio_rdev_close(struct cxio_rdev *rdev_p)
{
	if (rdev_p) {
		cxio_hal_pblpool_destroy(rdev_p);
		cxio_hal_rqtpool_destroy(rdev_p);
		TAILQ_REMOVE(&rdev_list, rdev_p, entry);
		rdev_p->t3cdev_p->ulp = NULL;
		cxio_hal_destroy_ctrl_qp(rdev_p);
		cxio_hal_destroy_resource(rdev_p->rscp);
	}
}

int
cxio_hal_init(void)
{
	TAILQ_INIT(&rdev_list);
#ifdef needed
	if (cxio_hal_init_rhdl_resource(T3_MAX_NUM_RI))
		return (-ENOMEM);
#endif
	t3_register_cpl_handler(CPL_ASYNC_NOTIF, cxio_hal_ev_handler);
	return 0;
}

void
cxio_hal_exit(void)
{
	struct cxio_rdev *rdev, *tmp;

	t3_register_cpl_handler(CPL_ASYNC_NOTIF, NULL);
	TAILQ_FOREACH_SAFE(rdev, &rdev_list, entry, tmp)
		cxio_rdev_close(rdev);
#ifdef needed
	cxio_hal_destroy_rhdl_resource();
#endif
}

static void
flush_completed_wrs(struct t3_wq *wq, struct t3_cq *cq)
{
	struct t3_swsq *sqp;
	__u32 ptr = wq->sq_rptr;
	int count = Q_COUNT(wq->sq_rptr, wq->sq_wptr);

	sqp = wq->sq + Q_PTR2IDX(ptr, wq->sq_size_log2);
	while (count--)
		if (!sqp->signaled) {
			ptr++;
			sqp = wq->sq + Q_PTR2IDX(ptr,  wq->sq_size_log2);
		} else if (sqp->complete) {

			/*
			 * Insert this completed cqe into the swcq.
			 */
			CTR3(KTR_IW_CXGB, "%s moving cqe into swcq sq idx %ld cq idx %ld",
			     __FUNCTION__, Q_PTR2IDX(ptr,  wq->sq_size_log2),
			     Q_PTR2IDX(cq->sw_wptr, cq->size_log2));
			sqp->cqe.header |= htonl(V_CQE_SWCQE(1));
			*(cq->sw_queue + Q_PTR2IDX(cq->sw_wptr, cq->size_log2))
				= sqp->cqe;
			cq->sw_wptr++;
			sqp->signaled = 0;
			break;
		} else
			break;
}

static void
create_read_req_cqe(struct t3_wq *wq, struct t3_cqe *hw_cqe,
				struct t3_cqe *read_cqe)
{
	read_cqe->u.scqe.wrid_hi = wq->oldest_read->sq_wptr;
	read_cqe->len = wq->oldest_read->read_len;
	read_cqe->header = htonl(V_CQE_QPID(CQE_QPID(*hw_cqe)) |
				 V_CQE_SWCQE(SW_CQE(*hw_cqe)) |
				 V_CQE_OPCODE(T3_READ_REQ) |
				 V_CQE_TYPE(1));
}

/*
 * Return a ptr to the next read wr in the SWSQ or NULL.
 */
static void
advance_oldest_read(struct t3_wq *wq)
{

	u32 rptr = wq->oldest_read - wq->sq + 1;
	u32 wptr = Q_PTR2IDX(wq->sq_wptr, wq->sq_size_log2);

	while (Q_PTR2IDX(rptr, wq->sq_size_log2) != wptr) {
		wq->oldest_read = wq->sq + Q_PTR2IDX(rptr, wq->sq_size_log2);

		if (wq->oldest_read->opcode == T3_READ_REQ)
			return;
		rptr++;
	}
	wq->oldest_read = NULL;
}

/*
 * cxio_poll_cq
 *
 * Caller must:
 *     check the validity of the first CQE,
 *     supply the wq assicated with the qpid.
 *
 * credit: cq credit to return to sge.
 * cqe_flushed: 1 iff the CQE is flushed.
 * cqe: copy of the polled CQE.
 *
 * return value:
 *     0       CQE returned,
 *    -1       CQE skipped, try again.
 */
int
cxio_poll_cq(struct t3_wq *wq, struct t3_cq *cq, struct t3_cqe *cqe,
		     u8 *cqe_flushed, u64 *cookie, u32 *credit)
{
	int ret = 0;
	struct t3_cqe *hw_cqe, read_cqe;

	*cqe_flushed = 0;
	*credit = 0;
	hw_cqe = cxio_next_cqe(cq);

	CTR5(KTR_IW_CXGB, "cxio_poll_cq CQE OOO %d qpid 0x%0x genbit %d type %d status 0x%0x",
	     CQE_OOO(*hw_cqe), CQE_QPID(*hw_cqe),
	     CQE_GENBIT(*hw_cqe), CQE_TYPE(*hw_cqe), CQE_STATUS(*hw_cqe));
	CTR4(KTR_IW_CXGB, "opcode 0x%0x len 0x%0x wrid_hi_stag 0x%x wrid_low_msn 0x%x",
	     CQE_OPCODE(*hw_cqe), CQE_LEN(*hw_cqe), CQE_WRID_HI(*hw_cqe),
	     CQE_WRID_LOW(*hw_cqe));

	/*
	 * skip cqe's not affiliated with a QP.
	 */
	if (wq == NULL) {
		ret = -1;
		goto skip_cqe;
	}

	/*
	 * Gotta tweak READ completions:
	 *	1) the cqe doesn't contain the sq_wptr from the wr.
	 *	2) opcode not reflected from the wr.
	 *	3) read_len not reflected from the wr.
	 *	4) cq_type is RQ_TYPE not SQ_TYPE.
	 */
	if (RQ_TYPE(*hw_cqe) && (CQE_OPCODE(*hw_cqe) == T3_READ_RESP)) {

		/*
		 * Don't write to the HWCQ, so create a new read req CQE
		 * in local memory.
		 */
		create_read_req_cqe(wq, hw_cqe, &read_cqe);
		hw_cqe = &read_cqe;
		advance_oldest_read(wq);
	}

	/*
	 * T3A: Discard TERMINATE CQEs.
	 */
	if (CQE_OPCODE(*hw_cqe) == T3_TERMINATE) {
		ret = -1;
		wq->error = 1;
		goto skip_cqe;
	}

	if (CQE_STATUS(*hw_cqe) || wq->error) {
		*cqe_flushed = wq->error;
		wq->error = 1;

		/*
		 * T3A inserts errors into the CQE.  We cannot return
		 * these as work completions.
		 */
		/* incoming write failures */
		if ((CQE_OPCODE(*hw_cqe) == T3_RDMA_WRITE)
		     && RQ_TYPE(*hw_cqe)) {
			ret = -1;
			goto skip_cqe;
		}
		/* incoming read request failures */
		if ((CQE_OPCODE(*hw_cqe) == T3_READ_RESP) && SQ_TYPE(*hw_cqe)) {
			ret = -1;
			goto skip_cqe;
		}

		/* incoming SEND with no receive posted failures */
		if ((CQE_OPCODE(*hw_cqe) == T3_SEND) && RQ_TYPE(*hw_cqe) &&
		    Q_EMPTY(wq->rq_rptr, wq->rq_wptr)) {
			ret = -1;
			goto skip_cqe;
		}
		goto proc_cqe;
	}

	/*
	 * RECV completion.
	 */
	if (RQ_TYPE(*hw_cqe)) {

		/*
		 * HW only validates 4 bits of MSN.  So we must validate that
		 * the MSN in the SEND is the next expected MSN.  If its not,
		 * then we complete this with TPT_ERR_MSN and mark the wq in
		 * error.
		 */
		if (__predict_false((CQE_WRID_MSN(*hw_cqe) != (wq->rq_rptr + 1)))) {
			wq->error = 1;
			hw_cqe->header |= htonl(V_CQE_STATUS(TPT_ERR_MSN));
			goto proc_cqe;
		}
		goto proc_cqe;
	}

	/*
	 * If we get here its a send completion.
	 *
	 * Handle out of order completion. These get stuffed
	 * in the SW SQ. Then the SW SQ is walked to move any
	 * now in-order completions into the SW CQ.  This handles
	 * 2 cases:
	 *	1) reaping unsignaled WRs when the first subsequent
	 *	   signaled WR is completed.
	 *	2) out of order read completions.
	 */
	if (!SW_CQE(*hw_cqe) && (CQE_WRID_SQ_WPTR(*hw_cqe) != wq->sq_rptr)) {
		struct t3_swsq *sqp;

		CTR2(KTR_IW_CXGB, "%s out of order completion going in swsq at idx %ld",
		     __FUNCTION__,
		     Q_PTR2IDX(CQE_WRID_SQ_WPTR(*hw_cqe), wq->sq_size_log2));
		sqp = wq->sq +
		      Q_PTR2IDX(CQE_WRID_SQ_WPTR(*hw_cqe), wq->sq_size_log2);
		sqp->cqe = *hw_cqe;
		sqp->complete = 1;
		ret = -1;
		goto flush_wq;
	}

proc_cqe:
	*cqe = *hw_cqe;

	/*
	 * Reap the associated WR(s) that are freed up with this
	 * completion.
	 */
	if (SQ_TYPE(*hw_cqe)) {
		wq->sq_rptr = CQE_WRID_SQ_WPTR(*hw_cqe);
		CTR2(KTR_IW_CXGB, "%s completing sq idx %ld", __FUNCTION__,
		     Q_PTR2IDX(wq->sq_rptr, wq->sq_size_log2));
		*cookie = (wq->sq +
			   Q_PTR2IDX(wq->sq_rptr, wq->sq_size_log2))->wr_id;
		wq->sq_rptr++;
	} else {
		CTR2(KTR_IW_CXGB, "%s completing rq idx %ld", __FUNCTION__,
		     Q_PTR2IDX(wq->rq_rptr, wq->rq_size_log2));
		*cookie = *(wq->rq + Q_PTR2IDX(wq->rq_rptr, wq->rq_size_log2));
		wq->rq_rptr++;
	}

flush_wq:
	/*
	 * Flush any completed cqes that are now in-order.
	 */
	flush_completed_wrs(wq, cq);

skip_cqe:
	if (SW_CQE(*hw_cqe)) {
		CTR4(KTR_IW_CXGB, "%s cq %p cqid 0x%x skip sw cqe sw_rptr 0x%x",
		     __FUNCTION__, cq, cq->cqid, cq->sw_rptr);
		++cq->sw_rptr;
	} else {
		CTR4(KTR_IW_CXGB, "%s cq %p cqid 0x%x skip hw cqe rptr 0x%x",
		     __FUNCTION__, cq, cq->cqid, cq->rptr);
		++cq->rptr;

		/*
		 * T3A: compute credits.
		 */
		if (((cq->rptr - cq->wptr) > (1 << (cq->size_log2 - 1)))
		    || ((cq->rptr - cq->wptr) >= 128)) {
			*credit = cq->rptr - cq->wptr;
			cq->wptr = cq->rptr;
		}
	}
	return ret;
}


