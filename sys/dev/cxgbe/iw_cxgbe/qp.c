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
#include "opt_inet.h"

#ifdef TCP_OFFLOAD
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/sockio.h>
#include <sys/taskqueue.h>
#include <netinet/in.h>
#include <net/route.h>

#include <netinet/in_systm.h>
#include <netinet/in_pcb.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp.h>
#include <netinet/tcpip.h>

#include <netinet/toecore.h>

struct sge_iq;
struct rss_header;
struct cpl_set_tcb_rpl;
#include <linux/types.h>
#include "offload.h"
#include "tom/t4_tom.h"

#include "iw_cxgbe.h"
#include "user.h"

static int creds(struct toepcb *toep, struct inpcb *inp, size_t wrsize);
static int max_fr_immd = T4_MAX_FR_IMMD;//SYSCTL parameter later...

static int alloc_ird(struct c4iw_dev *dev, u32 ird)
{
	int ret = 0;

	spin_lock_irq(&dev->lock);
	if (ird <= dev->avail_ird)
		dev->avail_ird -= ird;
	else
		ret = -ENOMEM;
	spin_unlock_irq(&dev->lock);

	if (ret)
		log(LOG_WARNING, "%s: device IRD resources exhausted\n",
			device_get_nameunit(dev->rdev.adap->dev));

	return ret;
}

static void free_ird(struct c4iw_dev *dev, int ird)
{
	spin_lock_irq(&dev->lock);
	dev->avail_ird += ird;
	spin_unlock_irq(&dev->lock);
}

static void set_state(struct c4iw_qp *qhp, enum c4iw_qp_state state)
{
	unsigned long flag;
	spin_lock_irqsave(&qhp->lock, flag);
	qhp->attr.state = state;
	spin_unlock_irqrestore(&qhp->lock, flag);
}

static int destroy_qp(struct c4iw_rdev *rdev, struct t4_wq *wq,
		      struct c4iw_dev_ucontext *uctx)
{
	struct c4iw_dev *rhp = rdev_to_c4iw_dev(rdev);
	/*
	 * uP clears EQ contexts when the connection exits rdma mode,
	 * so no need to post a RESET WR for these EQs.
	 */
	dma_free_coherent(rhp->ibdev.dma_device,
			wq->rq.memsize, wq->rq.queue,
			dma_unmap_addr(&wq->rq, mapping));
	dma_free_coherent(rhp->ibdev.dma_device,
			wq->sq.memsize, wq->sq.queue,
			dma_unmap_addr(&wq->sq, mapping));
	c4iw_rqtpool_free(rdev, wq->rq.rqt_hwaddr, wq->rq.rqt_size);
	kfree(wq->rq.sw_rq);
	kfree(wq->sq.sw_sq);
	c4iw_put_qpid(rdev, wq->rq.qid, uctx);
	c4iw_put_qpid(rdev, wq->sq.qid, uctx);
	return 0;
}

static int create_qp(struct c4iw_rdev *rdev, struct t4_wq *wq,
		     struct t4_cq *rcq, struct t4_cq *scq,
		     struct c4iw_dev_ucontext *uctx)
{
	struct adapter *sc = rdev->adap;
	struct c4iw_dev *rhp = rdev_to_c4iw_dev(rdev);
	int user = (uctx != &rdev->uctx);
	struct fw_ri_res_wr *res_wr;
	struct fw_ri_res *res;
	int wr_len;
	struct c4iw_wr_wait wr_wait;
	int ret = 0;
	int eqsize;
	struct wrqe *wr;
	u64 sq_bar2_qoffset = 0, rq_bar2_qoffset = 0;

	if (__predict_false(c4iw_stopped(rdev)))
		return -EIO;

	wq->sq.qid = c4iw_get_qpid(rdev, uctx);
	if (!wq->sq.qid)
		return -ENOMEM;

	wq->rq.qid = c4iw_get_qpid(rdev, uctx);
	if (!wq->rq.qid) {
		ret = -ENOMEM;
		goto free_sq_qid;
	}

	if (!user) {
		wq->sq.sw_sq = kzalloc(wq->sq.size * sizeof *wq->sq.sw_sq,
				 GFP_KERNEL);
		if (!wq->sq.sw_sq) {
			ret = -ENOMEM;
			goto free_rq_qid;
		}

		wq->rq.sw_rq = kzalloc(wq->rq.size * sizeof *wq->rq.sw_rq,
				 GFP_KERNEL);
		if (!wq->rq.sw_rq) {
			ret = -ENOMEM;
			goto free_sw_sq;
		}
	}

	/*
	 * RQT must be a power of 2 and at least 16 deep.
	 */
	wq->rq.rqt_size = roundup_pow_of_two(max_t(u16, wq->rq.size, 16));
	wq->rq.rqt_hwaddr = c4iw_rqtpool_alloc(rdev, wq->rq.rqt_size);
	if (!wq->rq.rqt_hwaddr) {
		ret = -ENOMEM;
		goto free_sw_rq;
	}

	/*QP memory, allocate DMAable memory for Send & Receive Queues */
	wq->sq.queue = dma_alloc_coherent(rhp->ibdev.dma_device, wq->sq.memsize,
				       &(wq->sq.dma_addr), GFP_KERNEL);
	if (!wq->sq.queue) {
		ret = -ENOMEM;
		goto free_hwaddr;
	}
	wq->sq.phys_addr = vtophys(wq->sq.queue);
	dma_unmap_addr_set(&wq->sq, mapping, wq->sq.dma_addr);
	memset(wq->sq.queue, 0, wq->sq.memsize);

	wq->rq.queue = dma_alloc_coherent(rhp->ibdev.dma_device,
			wq->rq.memsize, &(wq->rq.dma_addr), GFP_KERNEL);
	if (!wq->rq.queue) {
		ret = -ENOMEM;
		goto free_sq_dma;
	}
	wq->rq.phys_addr = vtophys(wq->rq.queue);
	dma_unmap_addr_set(&wq->rq, mapping, wq->rq.dma_addr);
	memset(wq->rq.queue, 0, wq->rq.memsize);

	CTR5(KTR_IW_CXGBE,
	    "%s QP sq base va 0x%p pa 0x%llx rq base va 0x%p pa 0x%llx",
	    __func__,
	    wq->sq.queue, (unsigned long long)wq->sq.phys_addr,
	    wq->rq.queue, (unsigned long long)wq->rq.phys_addr);

	/* Doorbell/WC regions, determine the BAR2 queue offset and qid. */
	t4_bar2_sge_qregs(rdev->adap, wq->sq.qid, T4_BAR2_QTYPE_EGRESS, user,
			&sq_bar2_qoffset, &wq->sq.bar2_qid);
	t4_bar2_sge_qregs(rdev->adap, wq->rq.qid, T4_BAR2_QTYPE_EGRESS, user,
			&rq_bar2_qoffset, &wq->rq.bar2_qid);

	if (user) {
		/* Compute BAR2 DB/WC physical address(page-aligned) for
		 * Userspace mapping.
		 */
		wq->sq.bar2_pa = (rdev->bar2_pa + sq_bar2_qoffset) & PAGE_MASK;
		wq->rq.bar2_pa = (rdev->bar2_pa + rq_bar2_qoffset) & PAGE_MASK;
		CTR3(KTR_IW_CXGBE,
			"%s BAR2 DB/WC sq base pa 0x%llx rq base pa 0x%llx",
			__func__, (unsigned long long)wq->sq.bar2_pa,
			(unsigned long long)wq->rq.bar2_pa);
	} else {
		/* Compute BAR2 DB/WC virtual address to access in kernel. */
		wq->sq.bar2_va = (void __iomem *)((u64)rdev->bar2_kva +
				sq_bar2_qoffset);
		wq->rq.bar2_va = (void __iomem *)((u64)rdev->bar2_kva +
				rq_bar2_qoffset);
		CTR3(KTR_IW_CXGBE, "%s BAR2 DB/WC sq base va %p rq base va %p",
			__func__, (unsigned long long)wq->sq.bar2_va,
			(unsigned long long)wq->rq.bar2_va);
	}

	wq->rdev = rdev;
	wq->rq.msn = 1;

	/* build fw_ri_res_wr */
	wr_len = sizeof *res_wr + 2 * sizeof *res;

	wr = alloc_wrqe(wr_len, &sc->sge.ctrlq[0]);
	if (wr == NULL) {
		ret = -ENOMEM;
		goto free_rq_dma;
	}
        res_wr = wrtod(wr);

	memset(res_wr, 0, wr_len);
	res_wr->op_nres = cpu_to_be32(
			V_FW_WR_OP(FW_RI_RES_WR) |
			V_FW_RI_RES_WR_NRES(2) |
			F_FW_WR_COMPL);
	res_wr->len16_pkd = cpu_to_be32(DIV_ROUND_UP(wr_len, 16));
	res_wr->cookie = (unsigned long) &wr_wait;
	res = res_wr->res;
	res->u.sqrq.restype = FW_RI_RES_TYPE_SQ;
	res->u.sqrq.op = FW_RI_RES_OP_WRITE;

	/* eqsize is the number of 64B entries plus the status page size. */
	eqsize = wq->sq.size * T4_SQ_NUM_SLOTS +
			rdev->hw_queue.t4_eq_status_entries;

	res->u.sqrq.fetchszm_to_iqid = cpu_to_be32(
		V_FW_RI_RES_WR_HOSTFCMODE(0) |	/* no host cidx updates */
		V_FW_RI_RES_WR_CPRIO(0) |	/* don't keep in chip cache */
		V_FW_RI_RES_WR_PCIECHN(0) |	/* set by uP at ri_init time */
		V_FW_RI_RES_WR_IQID(scq->cqid));
	res->u.sqrq.dcaen_to_eqsize = cpu_to_be32(
		V_FW_RI_RES_WR_DCAEN(0) |
		V_FW_RI_RES_WR_DCACPU(0) |
		V_FW_RI_RES_WR_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		    X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_RI_RES_WR_FBMAX(3) |
		V_FW_RI_RES_WR_CIDXFTHRESHO(0) |
		V_FW_RI_RES_WR_CIDXFTHRESH(0) |
		V_FW_RI_RES_WR_EQSIZE(eqsize));
	res->u.sqrq.eqid = cpu_to_be32(wq->sq.qid);
	res->u.sqrq.eqaddr = cpu_to_be64(wq->sq.dma_addr);
	res++;
	res->u.sqrq.restype = FW_RI_RES_TYPE_RQ;
	res->u.sqrq.op = FW_RI_RES_OP_WRITE;

	/* eqsize is the number of 64B entries plus the status page size. */
	eqsize = wq->rq.size * T4_RQ_NUM_SLOTS +
			rdev->hw_queue.t4_eq_status_entries;
	res->u.sqrq.fetchszm_to_iqid = cpu_to_be32(
		V_FW_RI_RES_WR_HOSTFCMODE(0) |	/* no host cidx updates */
		V_FW_RI_RES_WR_CPRIO(0) |	/* don't keep in chip cache */
		V_FW_RI_RES_WR_PCIECHN(0) |	/* set by uP at ri_init time */
		V_FW_RI_RES_WR_IQID(rcq->cqid));
	res->u.sqrq.dcaen_to_eqsize = cpu_to_be32(
		V_FW_RI_RES_WR_DCAEN(0) |
		V_FW_RI_RES_WR_DCACPU(0) |
		V_FW_RI_RES_WR_FBMIN(chip_id(sc) <= CHELSIO_T5 ?
		    X_FETCHBURSTMIN_64B : X_FETCHBURSTMIN_64B_T6) |
		V_FW_RI_RES_WR_FBMAX(3) |
		V_FW_RI_RES_WR_CIDXFTHRESHO(0) |
		V_FW_RI_RES_WR_CIDXFTHRESH(0) |
		V_FW_RI_RES_WR_EQSIZE(eqsize));
	res->u.sqrq.eqid = cpu_to_be32(wq->rq.qid);
	res->u.sqrq.eqaddr = cpu_to_be64(wq->rq.dma_addr);

	c4iw_init_wr_wait(&wr_wait);

	t4_wrq_tx(sc, wr);
	ret = c4iw_wait_for_reply(rdev, &wr_wait, 0, wq->sq.qid,
			NULL, __func__);
	if (ret)
		goto free_rq_dma;

	CTR5(KTR_IW_CXGBE,
	    "%s sqid 0x%x rqid 0x%x kdb 0x%p squdb 0x%llx rqudb 0x%llx",
	    __func__, wq->sq.qid, wq->rq.qid,
	    (unsigned long long)wq->sq.bar2_va,
	    (unsigned long long)wq->rq.bar2_va);

	return 0;
free_rq_dma:
	dma_free_coherent(rhp->ibdev.dma_device,
			  wq->rq.memsize, wq->rq.queue,
			  dma_unmap_addr(&wq->rq, mapping));
free_sq_dma:
	dma_free_coherent(rhp->ibdev.dma_device,
			  wq->sq.memsize, wq->sq.queue,
			  dma_unmap_addr(&wq->sq, mapping));
free_hwaddr:
	c4iw_rqtpool_free(rdev, wq->rq.rqt_hwaddr, wq->rq.rqt_size);
free_sw_rq:
	kfree(wq->rq.sw_rq);
free_sw_sq:
	kfree(wq->sq.sw_sq);
free_rq_qid:
	c4iw_put_qpid(rdev, wq->rq.qid, uctx);
free_sq_qid:
	c4iw_put_qpid(rdev, wq->sq.qid, uctx);
	return ret;
}

static int build_immd(struct t4_sq *sq, struct fw_ri_immd *immdp,
		      const struct ib_send_wr *wr, int max, u32 *plenp)
{
	u8 *dstp, *srcp;
	u32 plen = 0;
	int i;
	int rem, len;

	dstp = (u8 *)immdp->data;
	for (i = 0; i < wr->num_sge; i++) {
		if ((plen + wr->sg_list[i].length) > max)
			return -EMSGSIZE;
		srcp = (u8 *)(unsigned long)wr->sg_list[i].addr;
		plen += wr->sg_list[i].length;
		rem = wr->sg_list[i].length;
		while (rem) {
			if (dstp == (u8 *)&sq->queue[sq->size])
				dstp = (u8 *)sq->queue;
			if (rem <= (u8 *)&sq->queue[sq->size] - dstp)
				len = rem;
			else
				len = (u8 *)&sq->queue[sq->size] - dstp;
			memcpy(dstp, srcp, len);
			dstp += len;
			srcp += len;
			rem -= len;
		}
	}
	len = roundup(plen + sizeof *immdp, 16) - (plen + sizeof *immdp);
	if (len)
		memset(dstp, 0, len);
	immdp->op = FW_RI_DATA_IMMD;
	immdp->r1 = 0;
	immdp->r2 = 0;
	immdp->immdlen = cpu_to_be32(plen);
	*plenp = plen;
	return 0;
}

static int build_isgl(__be64 *queue_start, __be64 *queue_end,
		      struct fw_ri_isgl *isglp, struct ib_sge *sg_list,
		      int num_sge, u32 *plenp)

{
	int i;
	u32 plen = 0;
	__be64 *flitp = (__be64 *)isglp->sge;

	for (i = 0; i < num_sge; i++) {
		if ((plen + sg_list[i].length) < plen)
			return -EMSGSIZE;
		plen += sg_list[i].length;
		*flitp = cpu_to_be64(((u64)sg_list[i].lkey << 32) |
				     sg_list[i].length);
		if (++flitp == queue_end)
			flitp = queue_start;
		*flitp = cpu_to_be64(sg_list[i].addr);
		if (++flitp == queue_end)
			flitp = queue_start;
	}
	*flitp = (__force __be64)0;
	isglp->op = FW_RI_DATA_ISGL;
	isglp->r1 = 0;
	isglp->nsge = cpu_to_be16(num_sge);
	isglp->r2 = 0;
	if (plenp)
		*plenp = plen;
	return 0;
}

static int build_rdma_send(struct t4_sq *sq, union t4_wr *wqe,
			   const struct ib_send_wr *wr, u8 *len16)
{
	u32 plen;
	int size;
	int ret;

	if (wr->num_sge > T4_MAX_SEND_SGE)
		return -EINVAL;
	switch (wr->opcode) {
	case IB_WR_SEND:
		if (wr->send_flags & IB_SEND_SOLICITED)
			wqe->send.sendop_pkd = cpu_to_be32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_SE));
		else
			wqe->send.sendop_pkd = cpu_to_be32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND));
		wqe->send.stag_inv = 0;
		break;
	case IB_WR_SEND_WITH_INV:
		if (wr->send_flags & IB_SEND_SOLICITED)
			wqe->send.sendop_pkd = cpu_to_be32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_SE_INV));
		else
			wqe->send.sendop_pkd = cpu_to_be32(
				V_FW_RI_SEND_WR_SENDOP(FW_RI_SEND_WITH_INV));
		wqe->send.stag_inv = cpu_to_be32(wr->ex.invalidate_rkey);
		break;

	default:
		return -EINVAL;
	}
	wqe->send.r3 = 0;
	wqe->send.r4 = 0;

	plen = 0;
	if (wr->num_sge) {
		if (wr->send_flags & IB_SEND_INLINE) {
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
			       wr->num_sge * sizeof(struct fw_ri_sge);
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
	wqe->send.plen = cpu_to_be32(plen);
	return 0;
}

static int build_rdma_write(struct t4_sq *sq, union t4_wr *wqe,
			    const struct ib_send_wr *wr, u8 *len16)
{
	u32 plen;
	int size;
	int ret;

	if (wr->num_sge > T4_MAX_SEND_SGE)
		return -EINVAL;
	wqe->write.immd_data = 0;
	wqe->write.stag_sink = cpu_to_be32(rdma_wr(wr)->rkey);
	wqe->write.to_sink = cpu_to_be64(rdma_wr(wr)->remote_addr);
	if (wr->num_sge) {
		if (wr->send_flags & IB_SEND_INLINE) {
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
			       wr->num_sge * sizeof(struct fw_ri_sge);
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
	wqe->write.plen = cpu_to_be32(plen);
	return 0;
}

static int build_rdma_read(union t4_wr *wqe, const struct ib_send_wr *wr, u8 *len16)
{
	if (wr->num_sge > 1)
		return -EINVAL;
	if (wr->num_sge && wr->sg_list[0].length) {
		wqe->read.stag_src = cpu_to_be32(rdma_wr(wr)->rkey);
		wqe->read.to_src_hi = cpu_to_be32((u32)(rdma_wr(wr)->remote_addr
							>> 32));
		wqe->read.to_src_lo =
			cpu_to_be32((u32)rdma_wr(wr)->remote_addr);
		wqe->read.stag_sink = cpu_to_be32(wr->sg_list[0].lkey);
		wqe->read.plen = cpu_to_be32(wr->sg_list[0].length);
		wqe->read.to_sink_hi = cpu_to_be32((u32)(wr->sg_list[0].addr
							 >> 32));
		wqe->read.to_sink_lo = cpu_to_be32((u32)(wr->sg_list[0].addr));
	} else {
		wqe->read.stag_src = cpu_to_be32(2);
		wqe->read.to_src_hi = 0;
		wqe->read.to_src_lo = 0;
		wqe->read.stag_sink = cpu_to_be32(2);
		wqe->read.plen = 0;
		wqe->read.to_sink_hi = 0;
		wqe->read.to_sink_lo = 0;
	}
	wqe->read.r2 = 0;
	wqe->read.r5 = 0;
	*len16 = DIV_ROUND_UP(sizeof wqe->read, 16);
	return 0;
}

static int build_rdma_recv(struct c4iw_qp *qhp, union t4_recv_wr *wqe,
			   const struct ib_recv_wr *wr, u8 *len16)
{
	int ret;

	ret = build_isgl((__be64 *)qhp->wq.rq.queue,
			 (__be64 *)&qhp->wq.rq.queue[qhp->wq.rq.size],
			 &wqe->recv.isgl, wr->sg_list, wr->num_sge, NULL);
	if (ret)
		return ret;
	*len16 = DIV_ROUND_UP(sizeof wqe->recv +
			      wr->num_sge * sizeof(struct fw_ri_sge), 16);
	return 0;
}

static int build_inv_stag(union t4_wr *wqe, const struct ib_send_wr *wr,
			  u8 *len16)
{
	wqe->inv.stag_inv = cpu_to_be32(wr->ex.invalidate_rkey);
	wqe->inv.r2 = 0;
	*len16 = DIV_ROUND_UP(sizeof wqe->inv, 16);
	return 0;
}

static void free_qp_work(struct work_struct *work)
{
	struct c4iw_ucontext *ucontext;
	struct c4iw_qp *qhp;
	struct c4iw_dev *rhp;

	qhp = container_of(work, struct c4iw_qp, free_work);
	ucontext = qhp->ucontext;
	rhp = qhp->rhp;

	CTR3(KTR_IW_CXGBE, "%s qhp %p ucontext %p", __func__,
			qhp, ucontext);
	destroy_qp(&rhp->rdev, &qhp->wq,
		   ucontext ? &ucontext->uctx : &rhp->rdev.uctx);

	kfree(qhp);
}

static void queue_qp_free(struct kref *kref)
{
	struct c4iw_qp *qhp;

	qhp = container_of(kref, struct c4iw_qp, kref);
	CTR2(KTR_IW_CXGBE, "%s qhp %p", __func__, qhp);
	queue_work(qhp->rhp->rdev.free_workq, &qhp->free_work);
}

void c4iw_qp_add_ref(struct ib_qp *qp)
{
	CTR2(KTR_IW_CXGBE, "%s ib_qp %p", __func__, qp);
	kref_get(&to_c4iw_qp(qp)->kref);
}

void c4iw_qp_rem_ref(struct ib_qp *qp)
{
	CTR2(KTR_IW_CXGBE, "%s ib_qp %p", __func__, qp);
	kref_put(&to_c4iw_qp(qp)->kref, queue_qp_free);
}

static void complete_sq_drain_wr(struct c4iw_qp *qhp, const struct ib_send_wr *wr)
{
	struct t4_cqe cqe = {};
	struct c4iw_cq *schp;
	unsigned long flag;
	struct t4_cq *cq;

	schp = to_c4iw_cq(qhp->ibqp.send_cq);
	cq = &schp->cq;

	PDBG("%s drain sq id %u\n", __func__, qhp->wq.sq.qid);
	cqe.u.drain_cookie = wr->wr_id;
	cqe.header = cpu_to_be32(V_CQE_STATUS(T4_ERR_SWFLUSH) |
				 V_CQE_OPCODE(C4IW_DRAIN_OPCODE) |
				 V_CQE_TYPE(1) |
				 V_CQE_SWCQE(1) |
				 V_CQE_QPID(qhp->wq.sq.qid));

	spin_lock_irqsave(&schp->lock, flag);
	cqe.bits_type_ts = cpu_to_be64(V_CQE_GENBIT((u64)cq->gen));
	cq->sw_queue[cq->sw_pidx] = cqe;
	t4_swcq_produce(cq);
	spin_unlock_irqrestore(&schp->lock, flag);

	spin_lock_irqsave(&schp->comp_handler_lock, flag);
	(*schp->ibcq.comp_handler)(&schp->ibcq,
				   schp->ibcq.cq_context);
	spin_unlock_irqrestore(&schp->comp_handler_lock, flag);
}

static void complete_rq_drain_wr(struct c4iw_qp *qhp, const struct ib_recv_wr *wr)
{
	struct t4_cqe cqe = {};
	struct c4iw_cq *rchp;
	unsigned long flag;
	struct t4_cq *cq;

	rchp = to_c4iw_cq(qhp->ibqp.recv_cq);
	cq = &rchp->cq;

	PDBG("%s drain rq id %u\n", __func__, qhp->wq.sq.qid);
	cqe.u.drain_cookie = wr->wr_id;
	cqe.header = cpu_to_be32(V_CQE_STATUS(T4_ERR_SWFLUSH) |
				 V_CQE_OPCODE(C4IW_DRAIN_OPCODE) |
				 V_CQE_TYPE(0) |
				 V_CQE_SWCQE(1) |
				 V_CQE_QPID(qhp->wq.sq.qid));

	spin_lock_irqsave(&rchp->lock, flag);
	cqe.bits_type_ts = cpu_to_be64(V_CQE_GENBIT((u64)cq->gen));
	cq->sw_queue[cq->sw_pidx] = cqe;
	t4_swcq_produce(cq);
	spin_unlock_irqrestore(&rchp->lock, flag);

	spin_lock_irqsave(&rchp->comp_handler_lock, flag);
	(*rchp->ibcq.comp_handler)(&rchp->ibcq,
				   rchp->ibcq.cq_context);
	spin_unlock_irqrestore(&rchp->comp_handler_lock, flag);
}

static int build_tpte_memreg(struct fw_ri_fr_nsmr_tpte_wr *fr,
		const struct ib_reg_wr *wr, struct c4iw_mr *mhp, u8 *len16)
{
	__be64 *p = (__be64 *)fr->pbl;

	if (wr->mr->page_size > C4IW_MAX_PAGE_SIZE)
		return -EINVAL;

	fr->r2 = cpu_to_be32(0);
	fr->stag = cpu_to_be32(mhp->ibmr.rkey);

	fr->tpte.valid_to_pdid = cpu_to_be32(F_FW_RI_TPTE_VALID |
			V_FW_RI_TPTE_STAGKEY((mhp->ibmr.rkey & M_FW_RI_TPTE_STAGKEY)) |
			V_FW_RI_TPTE_STAGSTATE(1) |
			V_FW_RI_TPTE_STAGTYPE(FW_RI_STAG_NSMR) |
			V_FW_RI_TPTE_PDID(mhp->attr.pdid));
	fr->tpte.locread_to_qpid = cpu_to_be32(
			V_FW_RI_TPTE_PERM(c4iw_ib_to_tpt_access(wr->access)) |
			V_FW_RI_TPTE_ADDRTYPE(FW_RI_VA_BASED_TO) |
			V_FW_RI_TPTE_PS(ilog2(wr->mr->page_size) - 12));
	fr->tpte.nosnoop_pbladdr = cpu_to_be32(V_FW_RI_TPTE_PBLADDR(
			      PBL_OFF(&mhp->rhp->rdev, mhp->attr.pbl_addr)>>3));
	fr->tpte.dca_mwbcnt_pstag = cpu_to_be32(0);
	fr->tpte.len_hi = cpu_to_be32(mhp->ibmr.length >> 32);
	fr->tpte.len_lo = cpu_to_be32(mhp->ibmr.length & 0xffffffff);
	fr->tpte.va_hi = cpu_to_be32(mhp->ibmr.iova >> 32);
	fr->tpte.va_lo_fbo = cpu_to_be32(mhp->ibmr.iova & 0xffffffff);

	p[0] = cpu_to_be64((u64)mhp->mpl[0]);
	p[1] = cpu_to_be64((u64)mhp->mpl[1]);

	*len16 = DIV_ROUND_UP(sizeof(*fr), 16);
	return 0;
}

static int build_memreg(struct t4_sq *sq, union t4_wr *wqe,
		const struct ib_reg_wr *wr, struct c4iw_mr *mhp, u8 *len16,
		bool dsgl_supported)
{
	struct fw_ri_immd *imdp;
	__be64 *p;
	int i;
	int pbllen = roundup(mhp->mpl_len * sizeof(u64), 32);
	int rem;

	if (mhp->mpl_len > t4_max_fr_depth(&mhp->rhp->rdev, use_dsgl))
		return -EINVAL;
	if (wr->mr->page_size > C4IW_MAX_PAGE_SIZE)
		return -EINVAL;

	wqe->fr.qpbinde_to_dcacpu = 0;
	wqe->fr.pgsz_shift = ilog2(wr->mr->page_size) - 12;
	wqe->fr.addr_type = FW_RI_VA_BASED_TO;
	wqe->fr.mem_perms = c4iw_ib_to_tpt_access(wr->access);
	wqe->fr.len_hi = cpu_to_be32(mhp->ibmr.length >> 32);
	wqe->fr.len_lo = cpu_to_be32(mhp->ibmr.length & 0xffffffff);
	wqe->fr.stag = cpu_to_be32(wr->key);
	wqe->fr.va_hi = cpu_to_be32(mhp->ibmr.iova >> 32);
	wqe->fr.va_lo_fbo = cpu_to_be32(mhp->ibmr.iova & 0xffffffff);

	if (dsgl_supported && use_dsgl && (pbllen > max_fr_immd)) {
		struct fw_ri_dsgl *sglp;

		for (i = 0; i < mhp->mpl_len; i++)
			mhp->mpl[i] =
				     (__force u64)cpu_to_be64((u64)mhp->mpl[i]);

		sglp = (struct fw_ri_dsgl *)(&wqe->fr + 1);
		sglp->op = FW_RI_DATA_DSGL;
		sglp->r1 = 0;
		sglp->nsge = cpu_to_be16(1);
		sglp->addr0 = cpu_to_be64(mhp->mpl_addr);
		sglp->len0 = cpu_to_be32(pbllen);

		*len16 = DIV_ROUND_UP(sizeof(wqe->fr) + sizeof(*sglp), 16);
	} else {
		imdp = (struct fw_ri_immd *)(&wqe->fr + 1);
		imdp->op = FW_RI_DATA_IMMD;
		imdp->r1 = 0;
		imdp->r2 = 0;
		imdp->immdlen = cpu_to_be32(pbllen);
		p = (__be64 *)(imdp + 1);
		rem = pbllen;
		for (i = 0; i < mhp->mpl_len; i++) {
			*p = cpu_to_be64((u64)mhp->mpl[i]);
			rem -= sizeof(*p);
			if (++p == (__be64 *)&sq->queue[sq->size])
				p = (__be64 *)sq->queue;
		}
		BUG_ON(rem < 0);
		while (rem) {
			*p = 0;
			rem -= sizeof(*p);
			if (++p == (__be64 *)&sq->queue[sq->size])
				p = (__be64 *)sq->queue;
		}
		*len16 = DIV_ROUND_UP(sizeof(wqe->fr) + sizeof(*imdp)
				+ pbllen, 16);
	}

	return 0;
}

int c4iw_post_send(struct ib_qp *ibqp, const struct ib_send_wr *wr,
		   const struct ib_send_wr **bad_wr)
{
	int err = 0;
	u8 len16 = 0;
	enum fw_wr_opcodes fw_opcode = 0;
	enum fw_ri_wr_flags fw_flags;
	struct c4iw_qp *qhp;
	union t4_wr *wqe = NULL;
	u32 num_wrs;
	struct t4_swsqe *swsqe;
	unsigned long flag;
	u16 idx = 0;
	struct c4iw_rdev *rdev;

	qhp = to_c4iw_qp(ibqp);
	rdev = &qhp->rhp->rdev;
	if (__predict_false(c4iw_stopped(rdev)))
		return -EIO;
	spin_lock_irqsave(&qhp->lock, flag);
	if (t4_wq_in_error(&qhp->wq)) {
		spin_unlock_irqrestore(&qhp->lock, flag);
		complete_sq_drain_wr(qhp, wr);
		return err;
	}
	num_wrs = t4_sq_avail(&qhp->wq);
	if (num_wrs == 0) {
		spin_unlock_irqrestore(&qhp->lock, flag);
		*bad_wr = wr;
		return -ENOMEM;
	}
	while (wr) {
		if (num_wrs == 0) {
			err = -ENOMEM;
			*bad_wr = wr;
			break;
		}
		wqe = (union t4_wr *)((u8 *)qhp->wq.sq.queue +
		      qhp->wq.sq.wq_pidx * T4_EQ_ENTRY_SIZE);

		fw_flags = 0;
		if (wr->send_flags & IB_SEND_SOLICITED)
			fw_flags |= FW_RI_SOLICITED_EVENT_FLAG;
		if (wr->send_flags & IB_SEND_SIGNALED || qhp->sq_sig_all)
			fw_flags |= FW_RI_COMPLETION_FLAG;
		swsqe = &qhp->wq.sq.sw_sq[qhp->wq.sq.pidx];
		switch (wr->opcode) {
		case IB_WR_SEND_WITH_INV:
		case IB_WR_SEND:
			if (wr->send_flags & IB_SEND_FENCE)
				fw_flags |= FW_RI_READ_FENCE_FLAG;
			fw_opcode = FW_RI_SEND_WR;
			if (wr->opcode == IB_WR_SEND)
				swsqe->opcode = FW_RI_SEND;
			else
				swsqe->opcode = FW_RI_SEND_WITH_INV;
			err = build_rdma_send(&qhp->wq.sq, wqe, wr, &len16);
			break;
		case IB_WR_RDMA_WRITE:
			fw_opcode = FW_RI_RDMA_WRITE_WR;
			swsqe->opcode = FW_RI_RDMA_WRITE;
			err = build_rdma_write(&qhp->wq.sq, wqe, wr, &len16);
			break;
		case IB_WR_RDMA_READ:
		case IB_WR_RDMA_READ_WITH_INV:
			fw_opcode = FW_RI_RDMA_READ_WR;
			swsqe->opcode = FW_RI_READ_REQ;
			if (wr->opcode == IB_WR_RDMA_READ_WITH_INV) {
				c4iw_invalidate_mr(qhp->rhp,
						   wr->sg_list[0].lkey);
				fw_flags = FW_RI_RDMA_READ_INVALIDATE;
			} else {
				fw_flags = 0;
			}
			err = build_rdma_read(wqe, wr, &len16);
			if (err)
				break;
			swsqe->read_len = wr->sg_list[0].length;
			if (!qhp->wq.sq.oldest_read)
				qhp->wq.sq.oldest_read = swsqe;
			break;
		case IB_WR_REG_MR: {
			struct c4iw_mr *mhp = to_c4iw_mr(reg_wr(wr)->mr);

			swsqe->opcode = FW_RI_FAST_REGISTER;
			if (rdev->adap->params.fr_nsmr_tpte_wr_support &&
					!mhp->attr.state && mhp->mpl_len <= 2) {
				fw_opcode = FW_RI_FR_NSMR_TPTE_WR;
				err = build_tpte_memreg(&wqe->fr_tpte, reg_wr(wr),
						mhp, &len16);
			} else {
				fw_opcode = FW_RI_FR_NSMR_WR;
				err = build_memreg(&qhp->wq.sq, wqe, reg_wr(wr),
					mhp, &len16,
					rdev->adap->params.ulptx_memwrite_dsgl);
			}
			if (err)
				break;
			mhp->attr.state = 1;
			break;
		}
		case IB_WR_LOCAL_INV:
			if (wr->send_flags & IB_SEND_FENCE)
				fw_flags |= FW_RI_LOCAL_FENCE_FLAG;
			fw_opcode = FW_RI_INV_LSTAG_WR;
			swsqe->opcode = FW_RI_LOCAL_INV;
			err = build_inv_stag(wqe, wr, &len16);
			c4iw_invalidate_mr(qhp->rhp, wr->ex.invalidate_rkey);
			break;
		default:
			CTR2(KTR_IW_CXGBE, "%s post of type =%d TBD!", __func__,
			     wr->opcode);
			err = -EINVAL;
		}
		if (err) {
			*bad_wr = wr;
			break;
		}
		swsqe->idx = qhp->wq.sq.pidx;
		swsqe->complete = 0;
		swsqe->signaled = (wr->send_flags & IB_SEND_SIGNALED) ||
					qhp->sq_sig_all;
		swsqe->flushed = 0;
		swsqe->wr_id = wr->wr_id;

		init_wr_hdr(wqe, qhp->wq.sq.pidx, fw_opcode, fw_flags, len16);

		CTR5(KTR_IW_CXGBE,
		    "%s cookie 0x%llx pidx 0x%x opcode 0x%x read_len %u",
		    __func__, (unsigned long long)wr->wr_id, qhp->wq.sq.pidx,
		    swsqe->opcode, swsqe->read_len);
		wr = wr->next;
		num_wrs--;
		t4_sq_produce(&qhp->wq, len16);
		idx += DIV_ROUND_UP(len16*16, T4_EQ_ENTRY_SIZE);
	}

	t4_ring_sq_db(&qhp->wq, idx, wqe, rdev->adap->iwt.wc_en);
	spin_unlock_irqrestore(&qhp->lock, flag);
	return err;
}

int c4iw_post_receive(struct ib_qp *ibqp, const struct ib_recv_wr *wr,
		      const struct ib_recv_wr **bad_wr)
{
	int err = 0;
	struct c4iw_qp *qhp;
	union t4_recv_wr *wqe = NULL;
	u32 num_wrs;
	u8 len16 = 0;
	unsigned long flag;
	u16 idx = 0;

	qhp = to_c4iw_qp(ibqp);
	if (__predict_false(c4iw_stopped(&qhp->rhp->rdev)))
		return -EIO;
	spin_lock_irqsave(&qhp->lock, flag);
	if (t4_wq_in_error(&qhp->wq)) {
		spin_unlock_irqrestore(&qhp->lock, flag);
		complete_rq_drain_wr(qhp, wr);
		return err;
	}
	num_wrs = t4_rq_avail(&qhp->wq);
	if (num_wrs == 0) {
		spin_unlock_irqrestore(&qhp->lock, flag);
		*bad_wr = wr;
		return -ENOMEM;
	}
	while (wr) {
		if (wr->num_sge > T4_MAX_RECV_SGE) {
			err = -EINVAL;
			*bad_wr = wr;
			break;
		}
		wqe = (union t4_recv_wr *)((u8 *)qhp->wq.rq.queue +
					   qhp->wq.rq.wq_pidx *
					   T4_EQ_ENTRY_SIZE);
		if (num_wrs)
			err = build_rdma_recv(qhp, wqe, wr, &len16);
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
		CTR3(KTR_IW_CXGBE, "%s cookie 0x%llx pidx %u", __func__,
		     (unsigned long long) wr->wr_id, qhp->wq.rq.pidx);
		t4_rq_produce(&qhp->wq, len16);
		idx += DIV_ROUND_UP(len16*16, T4_EQ_ENTRY_SIZE);
		wr = wr->next;
		num_wrs--;
	}

	t4_ring_rq_db(&qhp->wq, idx, wqe, qhp->rhp->rdev.adap->iwt.wc_en);
	spin_unlock_irqrestore(&qhp->lock, flag);
	return err;
}

static inline void build_term_codes(struct t4_cqe *err_cqe, u8 *layer_type,
				    u8 *ecode)
{
	int status;
	int tagged;
	int opcode;
	int rqtype;
	int send_inv;

	if (!err_cqe) {
		*layer_type = LAYER_RDMAP|DDP_LOCAL_CATA;
		*ecode = 0;
		return;
	}

	status = CQE_STATUS(err_cqe);
	opcode = CQE_OPCODE(err_cqe);
	rqtype = RQ_TYPE(err_cqe);
	send_inv = (opcode == FW_RI_SEND_WITH_INV) ||
		   (opcode == FW_RI_SEND_WITH_SE_INV);
	tagged = (opcode == FW_RI_RDMA_WRITE) ||
		 (rqtype && (opcode == FW_RI_READ_RESP));

	switch (status) {
	case T4_ERR_STAG:
		if (send_inv) {
			*layer_type = LAYER_RDMAP|RDMAP_REMOTE_OP;
			*ecode = RDMAP_CANT_INV_STAG;
		} else {
			*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
			*ecode = RDMAP_INV_STAG;
		}
		break;
	case T4_ERR_PDID:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
		if ((opcode == FW_RI_SEND_WITH_INV) ||
		    (opcode == FW_RI_SEND_WITH_SE_INV))
			*ecode = RDMAP_CANT_INV_STAG;
		else
			*ecode = RDMAP_STAG_NOT_ASSOC;
		break;
	case T4_ERR_QPID:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
		*ecode = RDMAP_STAG_NOT_ASSOC;
		break;
	case T4_ERR_ACCESS:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
		*ecode = RDMAP_ACC_VIOL;
		break;
	case T4_ERR_WRAP:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
		*ecode = RDMAP_TO_WRAP;
		break;
	case T4_ERR_BOUND:
		if (tagged) {
			*layer_type = LAYER_DDP|DDP_TAGGED_ERR;
			*ecode = DDPT_BASE_BOUNDS;
		} else {
			*layer_type = LAYER_RDMAP|RDMAP_REMOTE_PROT;
			*ecode = RDMAP_BASE_BOUNDS;
		}
		break;
	case T4_ERR_INVALIDATE_SHARED_MR:
	case T4_ERR_INVALIDATE_MR_WITH_MW_BOUND:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_OP;
		*ecode = RDMAP_CANT_INV_STAG;
		break;
	case T4_ERR_ECC:
	case T4_ERR_ECC_PSTAG:
	case T4_ERR_INTERNAL_ERR:
		*layer_type = LAYER_RDMAP|RDMAP_LOCAL_CATA;
		*ecode = 0;
		break;
	case T4_ERR_OUT_OF_RQE:
		*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
		*ecode = DDPU_INV_MSN_NOBUF;
		break;
	case T4_ERR_PBL_ADDR_BOUND:
		*layer_type = LAYER_DDP|DDP_TAGGED_ERR;
		*ecode = DDPT_BASE_BOUNDS;
		break;
	case T4_ERR_CRC:
		*layer_type = LAYER_MPA|DDP_LLP;
		*ecode = MPA_CRC_ERR;
		break;
	case T4_ERR_MARKER:
		*layer_type = LAYER_MPA|DDP_LLP;
		*ecode = MPA_MARKER_ERR;
		break;
	case T4_ERR_PDU_LEN_ERR:
		*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
		*ecode = DDPU_MSG_TOOBIG;
		break;
	case T4_ERR_DDP_VERSION:
		if (tagged) {
			*layer_type = LAYER_DDP|DDP_TAGGED_ERR;
			*ecode = DDPT_INV_VERS;
		} else {
			*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
			*ecode = DDPU_INV_VERS;
		}
		break;
	case T4_ERR_RDMA_VERSION:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_OP;
		*ecode = RDMAP_INV_VERS;
		break;
	case T4_ERR_OPCODE:
		*layer_type = LAYER_RDMAP|RDMAP_REMOTE_OP;
		*ecode = RDMAP_INV_OPCODE;
		break;
	case T4_ERR_DDP_QUEUE_NUM:
		*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
		*ecode = DDPU_INV_QN;
		break;
	case T4_ERR_MSN:
	case T4_ERR_MSN_GAP:
	case T4_ERR_MSN_RANGE:
	case T4_ERR_IRD_OVERFLOW:
		*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
		*ecode = DDPU_INV_MSN_RANGE;
		break;
	case T4_ERR_TBIT:
		*layer_type = LAYER_DDP|DDP_LOCAL_CATA;
		*ecode = 0;
		break;
	case T4_ERR_MO:
		*layer_type = LAYER_DDP|DDP_UNTAGGED_ERR;
		*ecode = DDPU_INV_MO;
		break;
	default:
		*layer_type = LAYER_RDMAP|DDP_LOCAL_CATA;
		*ecode = 0;
		break;
	}
}

static void post_terminate(struct c4iw_qp *qhp, struct t4_cqe *err_cqe,
			   gfp_t gfp)
{
	int ret;
	struct fw_ri_wr *wqe;
	struct terminate_message *term;
	struct wrqe *wr;
	struct socket *so = qhp->ep->com.so;
        struct inpcb *inp = sotoinpcb(so);
        struct tcpcb *tp = intotcpcb(inp);
        struct toepcb *toep = tp->t_toe;

	CTR4(KTR_IW_CXGBE, "%s qhp %p qid 0x%x tid %u", __func__, qhp,
	    qhp->wq.sq.qid, qhp->ep->hwtid);

	wr = alloc_wrqe(sizeof(*wqe), &toep->ofld_txq->wrq);
	if (wr == NULL)
		return;
        wqe = wrtod(wr);

	memset(wqe, 0, sizeof *wqe);
	wqe->op_compl = cpu_to_be32(V_FW_WR_OP(FW_RI_WR));
	wqe->flowid_len16 = cpu_to_be32(
		V_FW_WR_FLOWID(qhp->ep->hwtid) |
		V_FW_WR_LEN16(DIV_ROUND_UP(sizeof *wqe, 16)));

	wqe->u.terminate.type = FW_RI_TYPE_TERMINATE;
	wqe->u.terminate.immdlen = cpu_to_be32(sizeof *term);
	term = (struct terminate_message *)wqe->u.terminate.termmsg;
	if (qhp->attr.layer_etype == (LAYER_MPA|DDP_LLP)) {
		term->layer_etype = qhp->attr.layer_etype;
		term->ecode = qhp->attr.ecode;
	} else
		build_term_codes(err_cqe, &term->layer_etype, &term->ecode);
	ret = creds(toep, inp, sizeof(*wqe));
	if (ret) {
		free_wrqe(wr);
		return;
	}
	t4_wrq_tx(qhp->rhp->rdev.adap, wr);
}

/* Assumes qhp lock is held. */
static void __flush_qp(struct c4iw_qp *qhp, struct c4iw_cq *rchp,
		       struct c4iw_cq *schp)
{
	int count;
	int rq_flushed, sq_flushed;
	unsigned long flag;

	CTR4(KTR_IW_CXGBE, "%s qhp %p rchp %p schp %p", __func__, qhp, rchp,
	    schp);

	/* locking hierarchy: cq lock first, then qp lock. */
	spin_lock_irqsave(&rchp->lock, flag);
	spin_lock(&qhp->lock);

	if (qhp->wq.flushed) {
		spin_unlock(&qhp->lock);
		spin_unlock_irqrestore(&rchp->lock, flag);
		return;
	}
	qhp->wq.flushed = 1;

	c4iw_flush_hw_cq(rchp);
	c4iw_count_rcqes(&rchp->cq, &qhp->wq, &count);
	rq_flushed = c4iw_flush_rq(&qhp->wq, &rchp->cq, count);
	spin_unlock(&qhp->lock);
	spin_unlock_irqrestore(&rchp->lock, flag);

	/* locking hierarchy: cq lock first, then qp lock. */
	spin_lock_irqsave(&schp->lock, flag);
	spin_lock(&qhp->lock);
	if (schp != rchp)
		c4iw_flush_hw_cq(schp);
	sq_flushed = c4iw_flush_sq(qhp);
	spin_unlock(&qhp->lock);
	spin_unlock_irqrestore(&schp->lock, flag);

	if (schp == rchp) {
		if (t4_clear_cq_armed(&rchp->cq) &&
		    (rq_flushed || sq_flushed)) {
			spin_lock_irqsave(&rchp->comp_handler_lock, flag);
			(*rchp->ibcq.comp_handler)(&rchp->ibcq,
						   rchp->ibcq.cq_context);
			spin_unlock_irqrestore(&rchp->comp_handler_lock, flag);
		}
	} else {
		if (t4_clear_cq_armed(&rchp->cq) && rq_flushed) {
			spin_lock_irqsave(&rchp->comp_handler_lock, flag);
			(*rchp->ibcq.comp_handler)(&rchp->ibcq,
						   rchp->ibcq.cq_context);
			spin_unlock_irqrestore(&rchp->comp_handler_lock, flag);
		}
		if (t4_clear_cq_armed(&schp->cq) && sq_flushed) {
			spin_lock_irqsave(&schp->comp_handler_lock, flag);
			(*schp->ibcq.comp_handler)(&schp->ibcq,
						   schp->ibcq.cq_context);
			spin_unlock_irqrestore(&schp->comp_handler_lock, flag);
		}
	}
}

static void flush_qp(struct c4iw_qp *qhp)
{
	struct c4iw_cq *rchp, *schp;
	unsigned long flag;

	rchp = to_c4iw_cq(qhp->ibqp.recv_cq);
	schp = to_c4iw_cq(qhp->ibqp.send_cq);

	t4_set_wq_in_error(&qhp->wq);
	if (qhp->ibqp.uobject) {
		t4_set_cq_in_error(&rchp->cq);
		spin_lock_irqsave(&rchp->comp_handler_lock, flag);
		(*rchp->ibcq.comp_handler)(&rchp->ibcq, rchp->ibcq.cq_context);
		spin_unlock_irqrestore(&rchp->comp_handler_lock, flag);
		if (schp != rchp) {
			t4_set_cq_in_error(&schp->cq);
			spin_lock_irqsave(&schp->comp_handler_lock, flag);
			(*schp->ibcq.comp_handler)(&schp->ibcq,
					schp->ibcq.cq_context);
			spin_unlock_irqrestore(&schp->comp_handler_lock, flag);
		}
		return;
	}
	__flush_qp(qhp, rchp, schp);
}

static int
rdma_fini(struct c4iw_dev *rhp, struct c4iw_qp *qhp, struct c4iw_ep *ep)
{
	struct c4iw_rdev *rdev = &rhp->rdev;
	struct adapter *sc = rdev->adap;
	struct fw_ri_wr *wqe;
	int ret;
	struct wrqe *wr;
	struct socket *so = ep->com.so;
        struct inpcb *inp = sotoinpcb(so);
        struct tcpcb *tp = intotcpcb(inp);
        struct toepcb *toep = tp->t_toe;

	KASSERT(rhp == qhp->rhp && ep == qhp->ep, ("%s: EDOOFUS", __func__));

	CTR5(KTR_IW_CXGBE, "%s qhp %p qid 0x%x ep %p tid %u", __func__, qhp,
	    qhp->wq.sq.qid, ep, ep->hwtid);

	wr = alloc_wrqe(sizeof(*wqe), &toep->ofld_txq->wrq);
	if (wr == NULL)
		return (0);
	wqe = wrtod(wr);

	memset(wqe, 0, sizeof *wqe);

	wqe->op_compl = cpu_to_be32(V_FW_WR_OP(FW_RI_WR) | F_FW_WR_COMPL);
	wqe->flowid_len16 = cpu_to_be32(V_FW_WR_FLOWID(ep->hwtid) |
	    V_FW_WR_LEN16(DIV_ROUND_UP(sizeof *wqe, 16)));
	wqe->cookie = (unsigned long) &ep->com.wr_wait;
	wqe->u.fini.type = FW_RI_TYPE_FINI;

	c4iw_init_wr_wait(&ep->com.wr_wait);

	ret = creds(toep, inp, sizeof(*wqe));
	if (ret) {
		free_wrqe(wr);
		return ret;
	}
	t4_wrq_tx(sc, wr);

	ret = c4iw_wait_for_reply(rdev, &ep->com.wr_wait, ep->hwtid,
			qhp->wq.sq.qid, ep->com.so, __func__);
	return ret;
}

static void build_rtr_msg(u8 p2p_type, struct fw_ri_init *init)
{
	CTR2(KTR_IW_CXGBE, "%s p2p_type = %d", __func__, p2p_type);
	memset(&init->u, 0, sizeof init->u);
	switch (p2p_type) {
	case FW_RI_INIT_P2PTYPE_RDMA_WRITE:
		init->u.write.opcode = FW_RI_RDMA_WRITE_WR;
		init->u.write.stag_sink = cpu_to_be32(1);
		init->u.write.to_sink = cpu_to_be64(1);
		init->u.write.u.immd_src[0].op = FW_RI_DATA_IMMD;
		init->u.write.len16 = DIV_ROUND_UP(sizeof init->u.write +
						   sizeof(struct fw_ri_immd),
						   16);
		break;
	case FW_RI_INIT_P2PTYPE_READ_REQ:
		init->u.write.opcode = FW_RI_RDMA_READ_WR;
		init->u.read.stag_src = cpu_to_be32(1);
		init->u.read.to_src_lo = cpu_to_be32(1);
		init->u.read.stag_sink = cpu_to_be32(1);
		init->u.read.to_sink_lo = cpu_to_be32(1);
		init->u.read.len16 = DIV_ROUND_UP(sizeof init->u.read, 16);
		break;
	}
}

static int
creds(struct toepcb *toep, struct inpcb *inp, size_t wrsize)
{
	struct ofld_tx_sdesc *txsd;

	CTR3(KTR_IW_CXGBE, "%s:creB  %p %u", __func__, toep , wrsize);
	INP_WLOCK(inp);
	if ((inp->inp_flags & INP_DROPPED) != 0) {
		INP_WUNLOCK(inp);
		return (EINVAL);
	}
	txsd = &toep->txsd[toep->txsd_pidx];
	txsd->tx_credits = howmany(wrsize, 16);
	txsd->plen = 0;
	KASSERT(toep->tx_credits >= txsd->tx_credits && toep->txsd_avail > 0,
			("%s: not enough credits (%d)", __func__, toep->tx_credits));
	toep->tx_credits -= txsd->tx_credits;
	if (__predict_false(++toep->txsd_pidx == toep->txsd_total))
		toep->txsd_pidx = 0;
	toep->txsd_avail--;
	INP_WUNLOCK(inp);
	CTR5(KTR_IW_CXGBE, "%s:creE  %p %u %u %u", __func__, toep ,
	    txsd->tx_credits, toep->tx_credits, toep->txsd_pidx);
	return (0);
}

static int rdma_init(struct c4iw_dev *rhp, struct c4iw_qp *qhp)
{
	struct fw_ri_wr *wqe;
	int ret;
	struct wrqe *wr;
	struct c4iw_ep *ep = qhp->ep;
	struct c4iw_rdev *rdev = &qhp->rhp->rdev;
	struct adapter *sc = rdev->adap;
	struct socket *so = ep->com.so;
        struct inpcb *inp = sotoinpcb(so);
        struct tcpcb *tp = intotcpcb(inp);
        struct toepcb *toep = tp->t_toe;

	CTR5(KTR_IW_CXGBE, "%s qhp %p qid 0x%x ep %p tid %u", __func__, qhp,
	    qhp->wq.sq.qid, ep, ep->hwtid);

	wr = alloc_wrqe(sizeof(*wqe), &toep->ofld_txq->wrq);
	if (wr == NULL)
		return (0);
	wqe = wrtod(wr);
	ret = alloc_ird(rhp, qhp->attr.max_ird);
	if (ret) {
		qhp->attr.max_ird = 0;
		free_wrqe(wr);
		return ret;
	}

	memset(wqe, 0, sizeof *wqe);

	wqe->op_compl = cpu_to_be32(
		V_FW_WR_OP(FW_RI_WR) |
		F_FW_WR_COMPL);
	wqe->flowid_len16 = cpu_to_be32(V_FW_WR_FLOWID(ep->hwtid) |
	    V_FW_WR_LEN16(DIV_ROUND_UP(sizeof *wqe, 16)));

	wqe->cookie = (unsigned long) &ep->com.wr_wait;

	wqe->u.init.type = FW_RI_TYPE_INIT;
	wqe->u.init.mpareqbit_p2ptype =
		V_FW_RI_WR_MPAREQBIT(qhp->attr.mpa_attr.initiator) |
		V_FW_RI_WR_P2PTYPE(qhp->attr.mpa_attr.p2p_type);
	wqe->u.init.mpa_attrs = FW_RI_MPA_IETF_ENABLE;
	if (qhp->attr.mpa_attr.recv_marker_enabled)
		wqe->u.init.mpa_attrs |= FW_RI_MPA_RX_MARKER_ENABLE;
	if (qhp->attr.mpa_attr.xmit_marker_enabled)
		wqe->u.init.mpa_attrs |= FW_RI_MPA_TX_MARKER_ENABLE;
	if (qhp->attr.mpa_attr.crc_enabled)
		wqe->u.init.mpa_attrs |= FW_RI_MPA_CRC_ENABLE;

	wqe->u.init.qp_caps = FW_RI_QP_RDMA_READ_ENABLE |
			    FW_RI_QP_RDMA_WRITE_ENABLE |
			    FW_RI_QP_BIND_ENABLE;
	if (!qhp->ibqp.uobject)
		wqe->u.init.qp_caps |= FW_RI_QP_FAST_REGISTER_ENABLE |
				     FW_RI_QP_STAG0_ENABLE;
	wqe->u.init.nrqe = cpu_to_be16(t4_rqes_posted(&qhp->wq));
	wqe->u.init.pdid = cpu_to_be32(qhp->attr.pd);
	wqe->u.init.qpid = cpu_to_be32(qhp->wq.sq.qid);
	wqe->u.init.sq_eqid = cpu_to_be32(qhp->wq.sq.qid);
	wqe->u.init.rq_eqid = cpu_to_be32(qhp->wq.rq.qid);
	wqe->u.init.scqid = cpu_to_be32(qhp->attr.scq);
	wqe->u.init.rcqid = cpu_to_be32(qhp->attr.rcq);
	wqe->u.init.ord_max = cpu_to_be32(qhp->attr.max_ord);
	wqe->u.init.ird_max = cpu_to_be32(qhp->attr.max_ird);
	wqe->u.init.iss = cpu_to_be32(ep->snd_seq);
	wqe->u.init.irs = cpu_to_be32(ep->rcv_seq);
	wqe->u.init.hwrqsize = cpu_to_be32(qhp->wq.rq.rqt_size);
	wqe->u.init.hwrqaddr = cpu_to_be32(qhp->wq.rq.rqt_hwaddr -
	    sc->vres.rq.start);
	if (qhp->attr.mpa_attr.initiator)
		build_rtr_msg(qhp->attr.mpa_attr.p2p_type, &wqe->u.init);

	c4iw_init_wr_wait(&ep->com.wr_wait);

	ret = creds(toep, inp, sizeof(*wqe));
	if (ret) {
		free_wrqe(wr);
		free_ird(rhp, qhp->attr.max_ird);
		return ret;
	}
	t4_wrq_tx(sc, wr);

	ret = c4iw_wait_for_reply(rdev, &ep->com.wr_wait, ep->hwtid,
			qhp->wq.sq.qid, ep->com.so, __func__);

	toep->params.ulp_mode = ULP_MODE_RDMA;
	free_ird(rhp, qhp->attr.max_ird);

	return ret;
}

int c4iw_modify_qp(struct c4iw_dev *rhp, struct c4iw_qp *qhp,
		   enum c4iw_qp_attr_mask mask,
		   struct c4iw_qp_attributes *attrs,
		   int internal)
{
	int ret = 0;
	struct c4iw_qp_attributes newattr = qhp->attr;
	int disconnect = 0;
	int terminate = 0;
	int abort = 0;
	int free = 0;
	struct c4iw_ep *ep = NULL;

	CTR5(KTR_IW_CXGBE, "%s qhp %p sqid 0x%x rqid 0x%x ep %p", __func__, qhp,
	    qhp->wq.sq.qid, qhp->wq.rq.qid, qhp->ep);
	CTR3(KTR_IW_CXGBE, "%s state %d -> %d", __func__, qhp->attr.state,
	    (mask & C4IW_QP_ATTR_NEXT_STATE) ? attrs->next_state : -1);

	mutex_lock(&qhp->mutex);

	/* Process attr changes if in IDLE */
	if (mask & C4IW_QP_ATTR_VALID_MODIFY) {
		if (qhp->attr.state != C4IW_QP_STATE_IDLE) {
			ret = -EIO;
			goto out;
		}
		if (mask & C4IW_QP_ATTR_ENABLE_RDMA_READ)
			newattr.enable_rdma_read = attrs->enable_rdma_read;
		if (mask & C4IW_QP_ATTR_ENABLE_RDMA_WRITE)
			newattr.enable_rdma_write = attrs->enable_rdma_write;
		if (mask & C4IW_QP_ATTR_ENABLE_RDMA_BIND)
			newattr.enable_bind = attrs->enable_bind;
		if (mask & C4IW_QP_ATTR_MAX_ORD) {
			if (attrs->max_ord > c4iw_max_read_depth) {
				ret = -EINVAL;
				goto out;
			}
			newattr.max_ord = attrs->max_ord;
		}
		if (mask & C4IW_QP_ATTR_MAX_IRD) {
			if (attrs->max_ird > cur_max_read_depth(rhp)) {
				ret = -EINVAL;
				goto out;
			}
			newattr.max_ird = attrs->max_ird;
		}
		qhp->attr = newattr;
	}

	if (!(mask & C4IW_QP_ATTR_NEXT_STATE))
		goto out;
	if (qhp->attr.state == attrs->next_state)
		goto out;

	/* Return EINPROGRESS if QP is already in transition state.
	 * Eg: CLOSING->IDLE transition or *->ERROR transition.
	 * This can happen while connection is switching(due to rdma_fini)
	 * from iWARP/RDDP to TOE mode and any inflight RDMA RX data will
	 * reach TOE driver -> TCP stack -> iWARP driver. In this way
	 * iWARP driver keep receiving inflight RDMA RX data until socket
	 * is closed or aborted. And if iWARP CM is in FPDU sate, then
	 * it tries to put QP in TERM state and disconnects endpoint.
	 * But as QP is already in transition state, this event is ignored.
	 */
	if ((qhp->attr.state >= C4IW_QP_STATE_ERROR) &&
		(attrs->next_state == C4IW_QP_STATE_TERMINATE)) {
		ret = -EINPROGRESS;
		goto out;
	}

	switch (qhp->attr.state) {
	case C4IW_QP_STATE_IDLE:
		switch (attrs->next_state) {
		case C4IW_QP_STATE_RTS:
			if (!(mask & C4IW_QP_ATTR_LLP_STREAM_HANDLE)) {
				ret = -EINVAL;
				goto out;
			}
			if (!(mask & C4IW_QP_ATTR_MPA_ATTR)) {
				ret = -EINVAL;
				goto out;
			}
			qhp->attr.mpa_attr = attrs->mpa_attr;
			qhp->attr.llp_stream_handle = attrs->llp_stream_handle;
			qhp->ep = qhp->attr.llp_stream_handle;
			set_state(qhp, C4IW_QP_STATE_RTS);

			/*
			 * Ref the endpoint here and deref when we
			 * disassociate the endpoint from the QP.  This
			 * happens in CLOSING->IDLE transition or *->ERROR
			 * transition.
			 */
			c4iw_get_ep(&qhp->ep->com);
			ret = rdma_init(rhp, qhp);
			if (ret)
				goto err;
			break;
		case C4IW_QP_STATE_ERROR:
			set_state(qhp, C4IW_QP_STATE_ERROR);
			flush_qp(qhp);
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
		break;
	case C4IW_QP_STATE_RTS:
		switch (attrs->next_state) {
		case C4IW_QP_STATE_CLOSING:
			BUG_ON(kref_read(&qhp->ep->com.kref) < 2);
			t4_set_wq_in_error(&qhp->wq);
			set_state(qhp, C4IW_QP_STATE_CLOSING);
			ep = qhp->ep;
			if (!internal) {
				abort = 0;
				disconnect = 1;
				c4iw_get_ep(&qhp->ep->com);
			}
			ret = rdma_fini(rhp, qhp, ep);
			if (ret)
				goto err;
			break;
		case C4IW_QP_STATE_TERMINATE:
			t4_set_wq_in_error(&qhp->wq);
			set_state(qhp, C4IW_QP_STATE_TERMINATE);
			qhp->attr.layer_etype = attrs->layer_etype;
			qhp->attr.ecode = attrs->ecode;
			ep = qhp->ep;
			if (!internal) {
				c4iw_get_ep(&qhp->ep->com);
				terminate = 1;
				disconnect = 1;
			} else {
				terminate = qhp->attr.send_term;
				ret = rdma_fini(rhp, qhp, ep);
				if (ret)
					goto err;
			}
			break;
		case C4IW_QP_STATE_ERROR:
			t4_set_wq_in_error(&qhp->wq);
			set_state(qhp, C4IW_QP_STATE_ERROR);
			if (!internal) {
				abort = 1;
				disconnect = 1;
				ep = qhp->ep;
				c4iw_get_ep(&qhp->ep->com);
			}
			goto err;
			break;
		default:
			ret = -EINVAL;
			goto out;
		}
		break;
	case C4IW_QP_STATE_CLOSING:

		/*
		 * Allow kernel users to move to ERROR for qp draining.
		 */
		if (!internal && (qhp->ibqp.uobject || attrs->next_state !=
				  C4IW_QP_STATE_ERROR)) {
			ret = -EINVAL;
			goto out;
		}
		switch (attrs->next_state) {
		case C4IW_QP_STATE_IDLE:
			flush_qp(qhp);
			set_state(qhp, C4IW_QP_STATE_IDLE);
			qhp->attr.llp_stream_handle = NULL;
			c4iw_put_ep(&qhp->ep->com);
			qhp->ep = NULL;
			wake_up(&qhp->wait);
			break;
		case C4IW_QP_STATE_ERROR:
			goto err;
		default:
			ret = -EINVAL;
			goto err;
		}
		break;
	case C4IW_QP_STATE_ERROR:
		if (attrs->next_state != C4IW_QP_STATE_IDLE) {
			ret = -EINVAL;
			goto out;
		}
		if (!t4_sq_empty(&qhp->wq) || !t4_rq_empty(&qhp->wq)) {
			ret = -EINVAL;
			goto out;
		}
		set_state(qhp, C4IW_QP_STATE_IDLE);
		break;
	case C4IW_QP_STATE_TERMINATE:
		if (!internal) {
			ret = -EINVAL;
			goto out;
		}
		goto err;
		break;
	default:
		printf("%s in a bad state %d\n",
		       __func__, qhp->attr.state);
		ret = -EINVAL;
		goto err;
		break;
	}
	goto out;
err:
	CTR3(KTR_IW_CXGBE, "%s disassociating ep %p qpid 0x%x", __func__,
	    qhp->ep, qhp->wq.sq.qid);

	/* disassociate the LLP connection */
	qhp->attr.llp_stream_handle = NULL;
	if (!ep)
		ep = qhp->ep;
	qhp->ep = NULL;
	set_state(qhp, C4IW_QP_STATE_ERROR);
	free = 1;
	abort = 1;
	BUG_ON(!ep);
	flush_qp(qhp);
	wake_up(&qhp->wait);
out:
	mutex_unlock(&qhp->mutex);

	if (terminate)
		post_terminate(qhp, NULL, internal ? GFP_ATOMIC : GFP_KERNEL);

	/*
	 * If disconnect is 1, then we need to initiate a disconnect
	 * on the EP.  This can be a normal close (RTS->CLOSING) or
	 * an abnormal close (RTS/CLOSING->ERROR).
	 */
	if (disconnect) {
		__c4iw_ep_disconnect(ep, abort, internal ? GFP_ATOMIC :
							 GFP_KERNEL);
		c4iw_put_ep(&ep->com);
	}

	/*
	 * If free is 1, then we've disassociated the EP from the QP
	 * and we need to dereference the EP.
	 */
	if (free)
		c4iw_put_ep(&ep->com);
	CTR2(KTR_IW_CXGBE, "%s exit state %d", __func__, qhp->attr.state);
	return ret;
}

int c4iw_destroy_qp(struct ib_qp *ib_qp, struct ib_udata *udata)
{
	struct c4iw_dev *rhp;
	struct c4iw_qp *qhp;
	struct c4iw_qp_attributes attrs;

	CTR2(KTR_IW_CXGBE, "%s ib_qp %p", __func__, ib_qp);
	qhp = to_c4iw_qp(ib_qp);
	rhp = qhp->rhp;

	attrs.next_state = C4IW_QP_STATE_ERROR;
	if (qhp->attr.state == C4IW_QP_STATE_TERMINATE)
		c4iw_modify_qp(rhp, qhp, C4IW_QP_ATTR_NEXT_STATE, &attrs, 1);
	else
		c4iw_modify_qp(rhp, qhp, C4IW_QP_ATTR_NEXT_STATE, &attrs, 0);
	wait_event(qhp->wait, !qhp->ep);

	remove_handle(rhp, &rhp->qpidr, qhp->wq.sq.qid);

	free_ird(rhp, qhp->attr.max_ird);
	c4iw_qp_rem_ref(ib_qp);

	CTR3(KTR_IW_CXGBE, "%s ib_qp %p qpid 0x%0x", __func__, ib_qp,
	    qhp->wq.sq.qid);
	return 0;
}

struct ib_qp *
c4iw_create_qp(struct ib_pd *pd, struct ib_qp_init_attr *attrs,
    struct ib_udata *udata)
{
	struct c4iw_dev *rhp;
	struct c4iw_qp *qhp;
	struct c4iw_pd *php;
	struct c4iw_cq *schp;
	struct c4iw_cq *rchp;
	struct c4iw_create_qp_resp uresp;
	unsigned int sqsize, rqsize;
	struct c4iw_ucontext *ucontext;
	int ret;
	struct c4iw_mm_entry *sq_key_mm = NULL, *rq_key_mm = NULL;
	struct c4iw_mm_entry *sq_db_key_mm = NULL, *rq_db_key_mm = NULL;

	CTR2(KTR_IW_CXGBE, "%s ib_pd %p", __func__, pd);

	if (attrs->qp_type != IB_QPT_RC)
		return ERR_PTR(-EINVAL);

	php = to_c4iw_pd(pd);
	rhp = php->rhp;
	schp = get_chp(rhp, ((struct c4iw_cq *)attrs->send_cq)->cq.cqid);
	rchp = get_chp(rhp, ((struct c4iw_cq *)attrs->recv_cq)->cq.cqid);
	if (!schp || !rchp)
		return ERR_PTR(-EINVAL);

	if (attrs->cap.max_inline_data > T4_MAX_SEND_INLINE)
		return ERR_PTR(-EINVAL);

	if (attrs->cap.max_recv_wr > rhp->rdev.hw_queue.t4_max_rq_size)
		return ERR_PTR(-E2BIG);
	rqsize = attrs->cap.max_recv_wr + 1;
	if (rqsize < 8)
		rqsize = 8;

	if (attrs->cap.max_send_wr > rhp->rdev.hw_queue.t4_max_sq_size)
		return ERR_PTR(-E2BIG);
	sqsize = attrs->cap.max_send_wr + 1;
	if (sqsize < 8)
		sqsize = 8;

	ucontext = pd->uobject ? to_c4iw_ucontext(pd->uobject->context) : NULL;

	qhp = kzalloc(sizeof(*qhp), GFP_KERNEL);
	if (!qhp)
		return ERR_PTR(-ENOMEM);
	qhp->wq.sq.size = sqsize;
	qhp->wq.sq.memsize =
		(sqsize + rhp->rdev.hw_queue.t4_eq_status_entries) *
		sizeof(*qhp->wq.sq.queue) + 16 * sizeof(__be64);
	qhp->wq.sq.flush_cidx = -1;
	qhp->wq.rq.size = rqsize;
	qhp->wq.rq.memsize =
		(rqsize + rhp->rdev.hw_queue.t4_eq_status_entries) *
		sizeof(*qhp->wq.rq.queue);

	if (ucontext) {
		qhp->wq.sq.memsize = roundup(qhp->wq.sq.memsize, PAGE_SIZE);
		qhp->wq.rq.memsize = roundup(qhp->wq.rq.memsize, PAGE_SIZE);
	}

	CTR5(KTR_IW_CXGBE, "%s sqsize %u sqmemsize %zu rqsize %u rqmemsize %zu",
	    __func__, sqsize, qhp->wq.sq.memsize, rqsize, qhp->wq.rq.memsize);

	ret = create_qp(&rhp->rdev, &qhp->wq, &schp->cq, &rchp->cq,
			ucontext ? &ucontext->uctx : &rhp->rdev.uctx);
	if (ret)
		goto err1;

	attrs->cap.max_recv_wr = rqsize - 1;
	attrs->cap.max_send_wr = sqsize - 1;
	attrs->cap.max_inline_data = T4_MAX_SEND_INLINE;

	qhp->rhp = rhp;
	qhp->attr.pd = php->pdid;
	qhp->attr.scq = ((struct c4iw_cq *) attrs->send_cq)->cq.cqid;
	qhp->attr.rcq = ((struct c4iw_cq *) attrs->recv_cq)->cq.cqid;
	qhp->attr.sq_num_entries = attrs->cap.max_send_wr;
	qhp->attr.rq_num_entries = attrs->cap.max_recv_wr;
	qhp->attr.sq_max_sges = attrs->cap.max_send_sge;
	qhp->attr.sq_max_sges_rdma_write = attrs->cap.max_send_sge;
	qhp->attr.rq_max_sges = attrs->cap.max_recv_sge;
	qhp->attr.state = C4IW_QP_STATE_IDLE;
	qhp->attr.next_state = C4IW_QP_STATE_IDLE;
	qhp->attr.enable_rdma_read = 1;
	qhp->attr.enable_rdma_write = 1;
	qhp->attr.enable_bind = 1;
	qhp->attr.max_ord = 0;
	qhp->attr.max_ird = 0;
	qhp->sq_sig_all = attrs->sq_sig_type == IB_SIGNAL_ALL_WR;
	spin_lock_init(&qhp->lock);
	mutex_init(&qhp->mutex);
	init_waitqueue_head(&qhp->wait);
	kref_init(&qhp->kref);
	INIT_WORK(&qhp->free_work, free_qp_work);

	ret = insert_handle(rhp, &rhp->qpidr, qhp, qhp->wq.sq.qid);
	if (ret)
		goto err2;

	if (udata) {
		sq_key_mm = kmalloc(sizeof(*sq_key_mm), GFP_KERNEL);
		if (!sq_key_mm) {
			ret = -ENOMEM;
			goto err3;
		}
		rq_key_mm = kmalloc(sizeof(*rq_key_mm), GFP_KERNEL);
		if (!rq_key_mm) {
			ret = -ENOMEM;
			goto err4;
		}
		sq_db_key_mm = kmalloc(sizeof(*sq_db_key_mm), GFP_KERNEL);
		if (!sq_db_key_mm) {
			ret = -ENOMEM;
			goto err5;
		}
		rq_db_key_mm = kmalloc(sizeof(*rq_db_key_mm), GFP_KERNEL);
		if (!rq_db_key_mm) {
			ret = -ENOMEM;
			goto err6;
		}
		uresp.flags = 0;
		uresp.qid_mask = rhp->rdev.qpmask;
		uresp.sqid = qhp->wq.sq.qid;
		uresp.sq_size = qhp->wq.sq.size;
		uresp.sq_memsize = qhp->wq.sq.memsize;
		uresp.rqid = qhp->wq.rq.qid;
		uresp.rq_size = qhp->wq.rq.size;
		uresp.rq_memsize = qhp->wq.rq.memsize;
		spin_lock(&ucontext->mmap_lock);
		uresp.ma_sync_key =  0;
		uresp.sq_key = ucontext->key;
		ucontext->key += PAGE_SIZE;
		uresp.rq_key = ucontext->key;
		ucontext->key += PAGE_SIZE;
		uresp.sq_db_gts_key = ucontext->key;
		ucontext->key += PAGE_SIZE;
		uresp.rq_db_gts_key = ucontext->key;
		ucontext->key += PAGE_SIZE;
		spin_unlock(&ucontext->mmap_lock);
		ret = ib_copy_to_udata(udata, &uresp, sizeof uresp);
		if (ret)
			goto err7;
		sq_key_mm->key = uresp.sq_key;
		sq_key_mm->addr = qhp->wq.sq.phys_addr;
		sq_key_mm->len = PAGE_ALIGN(qhp->wq.sq.memsize);
		CTR4(KTR_IW_CXGBE, "%s sq_key_mm %x, %x, %d", __func__,
				sq_key_mm->key, sq_key_mm->addr,
				sq_key_mm->len);
		insert_mmap(ucontext, sq_key_mm);
		rq_key_mm->key = uresp.rq_key;
		rq_key_mm->addr = qhp->wq.rq.phys_addr;
		rq_key_mm->len = PAGE_ALIGN(qhp->wq.rq.memsize);
		CTR4(KTR_IW_CXGBE, "%s rq_key_mm %x, %x, %d", __func__,
				rq_key_mm->key, rq_key_mm->addr,
				rq_key_mm->len);
		insert_mmap(ucontext, rq_key_mm);
		sq_db_key_mm->key = uresp.sq_db_gts_key;
		sq_db_key_mm->addr = (u64)qhp->wq.sq.bar2_pa;
		sq_db_key_mm->len = PAGE_SIZE;
		CTR4(KTR_IW_CXGBE, "%s sq_db_key_mm %x, %x, %d", __func__,
				sq_db_key_mm->key, sq_db_key_mm->addr,
				sq_db_key_mm->len);
		insert_mmap(ucontext, sq_db_key_mm);
		rq_db_key_mm->key = uresp.rq_db_gts_key;
		rq_db_key_mm->addr = (u64)qhp->wq.rq.bar2_pa;
		rq_db_key_mm->len = PAGE_SIZE;
		CTR4(KTR_IW_CXGBE, "%s rq_db_key_mm %x, %x, %d", __func__,
				rq_db_key_mm->key, rq_db_key_mm->addr,
				rq_db_key_mm->len);
		insert_mmap(ucontext, rq_db_key_mm);

		qhp->ucontext = ucontext;
	}
	qhp->ibqp.qp_num = qhp->wq.sq.qid;
	init_timer(&(qhp->timer));

	CTR5(KTR_IW_CXGBE, "%s sq id %u size %u memsize %zu num_entries %u",
		 __func__, qhp->wq.sq.qid,
		 qhp->wq.sq.size, qhp->wq.sq.memsize, attrs->cap.max_send_wr);
	CTR5(KTR_IW_CXGBE, "%s rq id %u size %u memsize %zu num_entries %u",
		 __func__, qhp->wq.rq.qid,
		 qhp->wq.rq.size, qhp->wq.rq.memsize, attrs->cap.max_recv_wr);
	return &qhp->ibqp;
err7:
	kfree(rq_db_key_mm);
err6:
	kfree(sq_db_key_mm);
err5:
	kfree(rq_key_mm);
err4:
	kfree(sq_key_mm);
err3:
	remove_handle(rhp, &rhp->qpidr, qhp->wq.sq.qid);
err2:
	destroy_qp(&rhp->rdev, &qhp->wq,
		   ucontext ? &ucontext->uctx : &rhp->rdev.uctx);
err1:
	kfree(qhp);
	return ERR_PTR(ret);
}

int c4iw_ib_modify_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		      int attr_mask, struct ib_udata *udata)
{
	struct c4iw_dev *rhp;
	struct c4iw_qp *qhp;
	enum c4iw_qp_attr_mask mask = 0;
	struct c4iw_qp_attributes attrs;

	CTR2(KTR_IW_CXGBE, "%s ib_qp %p", __func__, ibqp);

	/* iwarp does not support the RTR state */
	if ((attr_mask & IB_QP_STATE) && (attr->qp_state == IB_QPS_RTR))
		attr_mask &= ~IB_QP_STATE;

	/* Make sure we still have something left to do */
	if (!attr_mask)
		return 0;

	memset(&attrs, 0, sizeof attrs);
	qhp = to_c4iw_qp(ibqp);
	rhp = qhp->rhp;

	attrs.next_state = c4iw_convert_state(attr->qp_state);
	attrs.enable_rdma_read = (attr->qp_access_flags &
			       IB_ACCESS_REMOTE_READ) ?  1 : 0;
	attrs.enable_rdma_write = (attr->qp_access_flags &
				IB_ACCESS_REMOTE_WRITE) ? 1 : 0;
	attrs.enable_bind = (attr->qp_access_flags & IB_ACCESS_MW_BIND) ? 1 : 0;


	mask |= (attr_mask & IB_QP_STATE) ? C4IW_QP_ATTR_NEXT_STATE : 0;
	mask |= (attr_mask & IB_QP_ACCESS_FLAGS) ?
			(C4IW_QP_ATTR_ENABLE_RDMA_READ |
			 C4IW_QP_ATTR_ENABLE_RDMA_WRITE |
			 C4IW_QP_ATTR_ENABLE_RDMA_BIND) : 0;

	return c4iw_modify_qp(rhp, qhp, mask, &attrs, 0);
}

struct ib_qp *c4iw_get_qp(struct ib_device *dev, int qpn)
{
	CTR3(KTR_IW_CXGBE, "%s ib_dev %p qpn 0x%x", __func__, dev, qpn);
	return (struct ib_qp *)get_qhp(to_c4iw_dev(dev), qpn);
}

int c4iw_ib_query_qp(struct ib_qp *ibqp, struct ib_qp_attr *attr,
		     int attr_mask, struct ib_qp_init_attr *init_attr)
{
	struct c4iw_qp *qhp = to_c4iw_qp(ibqp);

	memset(attr, 0, sizeof *attr);
	memset(init_attr, 0, sizeof *init_attr);
	attr->qp_state = to_ib_qp_state(qhp->attr.state);
	init_attr->cap.max_send_wr = qhp->attr.sq_num_entries;
	init_attr->cap.max_recv_wr = qhp->attr.rq_num_entries;
	init_attr->cap.max_send_sge = qhp->attr.sq_max_sges;
	init_attr->cap.max_recv_sge = qhp->attr.sq_max_sges;
	init_attr->cap.max_inline_data = T4_MAX_SEND_INLINE;
	init_attr->sq_sig_type = qhp->sq_sig_all ? IB_SIGNAL_ALL_WR : 0;
	return 0;
}
#endif
