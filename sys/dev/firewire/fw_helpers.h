/*
 * Copyright (c) 2026 Abdelkader Boudih <freebsd@seuros.com>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_FIREWIRE_FW_HELPERS_H_
#define _DEV_FIREWIRE_FW_HELPERS_H_

static __inline int
fw_xfer_timeout_cancel(struct fw_xfer *xfer)
{
	struct firewire_comm *fc = xfer->fc;
	struct fw_xfer *txfer;
	int can_cancel_sent, cancelled, remove_tlabel;

	can_cancel_sent = 0;
	cancelled = 0;
	remove_tlabel = 0;

	/*
	 * FWXF_INQ xfers are still on the software AT queue and can be
	 * removed safely.  FWXF_SENT xfers have left the controller and are
	 * waiting only for a split response, so removing their tlabel is enough
	 * to make a late response miss this xfer.  FWXF_START xfers are still
	 * owned by the controller descriptor ring and must not be cancelled.
	 */
	FW_GLOCK(fc);
	if ((xfer->flag & FWXF_INQ) != 0) {
		STAILQ_REMOVE(&xfer->q->q, xfer, fw_xfer, link);
		xfer->flag &= ~FWXF_INQ;
		cancelled = 1;
		remove_tlabel = 1;
	} else if ((xfer->flag & FWXF_SENT) != 0) {
		can_cancel_sent = 1;
	}
	FW_GUNLOCK(fc);

	if (remove_tlabel || can_cancel_sent) {
		mtx_lock(&fc->tlabel_lock);
		if (xfer->tl >= 0) {
			STAILQ_FOREACH(txfer, &fc->tlabels[xfer->tl], tlabel) {
				if (txfer == xfer)
					break;
			}
			if (txfer == xfer) {
				STAILQ_REMOVE(&fc->tlabels[xfer->tl], xfer,
				    fw_xfer, tlabel);
				cancelled = 1;
			}
			xfer->tl = -1;
		}
		mtx_unlock(&fc->tlabel_lock);
	}

	if (cancelled) {
		mtx_lock(&fc->wait_lock);
		xfer->resp = ETIMEDOUT;
		xfer->flag |= FWXF_WAKE;
		mtx_unlock(&fc->wait_lock);
		wakeup(xfer);
	}

	return (cancelled);
}

/*
 * Wait for an async transfer to complete, with timeout.
 *
 * If the timeout fires while the controller still owns the transfer, keep the
 * xfer alive and continue waiting for the core completion path.  Returning
 * early in that state lets callers free an xfer still referenced by the
 * controller descriptor ring.
 */
static __inline int
fw_xferwait_timo(struct fw_xfer *xfer, int timo)
{
	struct firewire_comm *fc = xfer->fc;
	struct mtx *lock = &fc->wait_lock;
	int err = 0;
	int timedout = 0;

	mtx_lock(lock);
	while ((xfer->flag & FWXF_WAKE) == 0) {
		err = msleep(xfer, lock, PWAIT, "fwxfer", timo);
		if (err == EWOULDBLOCK) {
			mtx_unlock(lock);
			if (fw_xfer_timeout_cancel(xfer))
				return (ETIMEDOUT);
			timedout = 1;
			mtx_lock(lock);
			continue;
		}
		if (err) {
			mtx_unlock(lock);
			return (err);
		}
	}
	mtx_unlock(lock);
	if (timedout && xfer->resp == ETIMEDOUT)
		return (ETIMEDOUT);
	return (0);
}

/*
 * Submit an async request and wait for completion with timeout.
 */
static __inline int
fw_xfer_request_wait(struct firewire_comm *fc, struct fw_xfer *xfer, int timo)
{
	int err;

	err = fw_asyreq(fc, -1, xfer);
	if (err != 0)
		return (err);
	return (fw_xferwait_timo(xfer, timo));
}

static __inline int
fw_read_quadlet(struct firewire_comm *fc, struct malloc_type *mtype,
    uint16_t dst, uint8_t spd, uint16_t addr_hi, uint32_t addr_lo,
    uint32_t *val)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	int err;

	xfer = fw_xfer_alloc_buf(mtype, 0, 4);
	if (xfer == NULL)
		return (ENOMEM);

	xfer->send.spd = spd;
	xfer->fc = fc;
	xfer->hand = fw_xferwake;

	fp = &xfer->send.hdr;
	fp->mode.rreqq.tcode = FWTCODE_RREQQ;
	fp->mode.rreqq.dst = dst;
	fp->mode.rreqq.dest_hi = addr_hi;
	fp->mode.rreqq.dest_lo = addr_lo;

	err = fw_xfer_request_wait(fc, xfer, 2 * hz);
	if (err != 0)
		goto out;

	if (xfer->resp == 0 &&
	    xfer->recv.hdr.mode.rresq.rtcode == FWRCODE_COMPLETE)
		*val = ntohl(xfer->recv.hdr.mode.rresq.data);
	else
		err = EIO;
out:
	fw_xfer_free_buf(xfer);
	return (err);
}

static __inline int
fw_write_quadlet(struct firewire_comm *fc, struct malloc_type *mtype,
    uint16_t dst, uint8_t spd, uint16_t addr_hi, uint32_t addr_lo,
    uint32_t val)
{
	struct fw_xfer *xfer;
	struct fw_pkt *fp;
	int err;

	xfer = fw_xfer_alloc_buf(mtype, 0, 0);
	if (xfer == NULL)
		return (ENOMEM);

	xfer->send.spd = spd;
	xfer->fc = fc;
	xfer->hand = fw_xferwake;

	fp = &xfer->send.hdr;
	fp->mode.wreqq.tcode = FWTCODE_WREQQ;
	fp->mode.wreqq.dst = dst;
	fp->mode.wreqq.dest_hi = addr_hi;
	fp->mode.wreqq.dest_lo = addr_lo;
	fp->mode.wreqq.data = htonl(val);

	err = fw_xfer_request_wait(fc, xfer, 2 * hz);
	if (err != 0)
		goto out;

	if (xfer->resp != 0 ||
	    xfer->recv.hdr.mode.wres.rtcode != FWRCODE_COMPLETE)
		err = EIO;
out:
	fw_xfer_free_buf(xfer);
	return (err);
}

static __inline void
fw_iso_init_chunks(struct fw_xferq *xferq)
{
	struct mbuf *m;
	int i;

	STAILQ_INIT(&xferq->stvalid);
	STAILQ_INIT(&xferq->stfree);
	STAILQ_INIT(&xferq->stdma);
	xferq->stproc = NULL;

	for (i = 0; i < xferq->bnchunk; i++) {
		m = m_getcl(M_WAITOK, MT_DATA, M_PKTHDR);
		xferq->bulkxfer[i].mbuf = m;
		m->m_len = m->m_pkthdr.len = m->m_ext.ext_size;
		STAILQ_INSERT_TAIL(&xferq->stfree, &xferq->bulkxfer[i], link);
	}
}

static __inline void
fw_iso_free_chunks(struct fw_xferq *xferq, struct malloc_type *mtype)
{
	int i;

	for (i = 0; i < xferq->bnchunk; i++) {
		if (xferq->bulkxfer[i].mbuf != NULL)
			m_freem(xferq->bulkxfer[i].mbuf);
	}
	free(xferq->bulkxfer, mtype);
	xferq->bulkxfer = NULL;
}

/*
 * Dequeue an ISO receive mbuf, replace with a fresh one.
 * Returns the consumed mbuf, or NULL on error/allocation failure.
 */
static __inline struct mbuf *
fw_iso_dequeue(struct fw_xferq *xferq, struct fw_bulkxfer *sxfer,
    struct firewire_comm *fc)
{
	struct fw_pkt *fp;
	struct mbuf *m, *m0;

	fp = mtod(sxfer->mbuf, struct fw_pkt *);
	if (fc->irx_post != NULL)
		fc->irx_post(fc, fp->mode.ld);

	m = sxfer->mbuf;

	m0 = m_getcl(M_NOWAIT, MT_DATA, M_PKTHDR);
	if (m0 == NULL) {
		/* Allocation failed; recycle the original mbuf. */
		STAILQ_INSERT_TAIL(&xferq->stfree, sxfer, link);
		return (NULL);
	}

	m0->m_len = m0->m_pkthdr.len = m0->m_ext.ext_size;
	sxfer->mbuf = m0;
	STAILQ_INSERT_TAIL(&xferq->stfree, sxfer, link);

	if (sxfer->resp != 0) {
		m_freem(m);
		return (NULL);
	}

	return (m);
}

static __inline void
fw_iso_rearm(struct fw_xferq *xferq, struct firewire_comm *fc, int dma_ch)
{

	if (STAILQ_FIRST(&xferq->stfree) != NULL && dma_ch >= 0)
		fc->irx_enable(fc, dma_ch);
}

static __inline void
fw_iso_wait_inactive_locked(struct mtx *mtx, int *active, const char *wmesg)
{

	mtx_assert(mtx, MA_OWNED);
	while (*active)
		msleep(active, mtx, PWAIT, wmesg, hz);
}

static __inline void
fw_iso_rearm_done(struct fw_xferq *xferq, struct firewire_comm *fc,
    struct mtx *mtx, int *active, int *cur_dma_ch, int dma_ch)
{

	mtx_lock(mtx);
	if (*cur_dma_ch == dma_ch)
		fw_iso_rearm(xferq, fc, dma_ch);
	*active = 0;
	wakeup(active);
	mtx_unlock(mtx);
}

#endif /* _DEV_FIREWIRE_FW_HELPERS_H_ */
