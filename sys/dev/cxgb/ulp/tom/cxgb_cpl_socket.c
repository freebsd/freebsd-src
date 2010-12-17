/**************************************************************************

Copyright (c) 2007-2008, Chelsio Inc.
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
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/sockstate.h>
#include <sys/sockopt.h>
#include <sys/socket.h>
#include <sys/sockbuf.h>
#include <sys/syslog.h>
#include <sys/uio.h>
#include <sys/file.h>

#include <machine/bus.h>
#include <machine/cpu.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>

#include <cxgb_osdep.h>
#include <sys/mbufq.h>
#include <ulp/tom/cxgb_tcp_offload.h>
#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_offload.h>
#include <net/route.h>

#include <t3cdev.h>
#include <common/cxgb_firmware_exports.h>
#include <common/cxgb_t3_cpl.h>
#include <common/cxgb_tcb.h>
#include <common/cxgb_ctl_defs.h>
#include <cxgb_offload.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <sys/mvec.h>
#include <ulp/toecore/cxgb_toedev.h>
#include <ulp/tom/cxgb_defs.h>
#include <ulp/tom/cxgb_tom.h>
#include <ulp/tom/cxgb_t3_ddp.h>
#include <ulp/tom/cxgb_toepcb.h>
#include <ulp/tom/cxgb_tcp.h>
#include <ulp/tom/cxgb_vm.h>


static int	(*pru_sosend)(struct socket *so, struct sockaddr *addr,
    struct uio *uio, struct mbuf *top, struct mbuf *control,
    int flags, struct thread *td);

static int	(*pru_soreceive)(struct socket *so, struct sockaddr **paddr,
    struct uio *uio, struct mbuf **mp0, struct mbuf **controlp,
    int *flagsp);

#define TMP_IOV_MAX 16
#ifndef PG_FRAME
#define PG_FRAME	~PAGE_MASK
#endif
#define SBLOCKWAIT(f) (((f) & MSG_DONTWAIT) ? M_NOWAIT : M_WAITOK)

void
t3_init_socket_ops(void)
{
	struct protosw *prp;

	prp = pffindtype(AF_INET, SOCK_STREAM);
	pru_sosend = prp->pr_usrreqs->pru_sosend;
	pru_soreceive = prp->pr_usrreqs->pru_soreceive;
}

struct cxgb_dma_info {
	size_t			cdi_mapped;
	int			cdi_nsegs;
	bus_dma_segment_t	*cdi_segs;
	
};

static void
cxgb_dma_callback(void *arg, bus_dma_segment_t *segs, int nsegs,
    bus_size_t mapsize, int error)
{
	struct cxgb_dma_info *cdi = arg;
	
	cdi->cdi_mapped = mapsize;
	cdi->cdi_nsegs = nsegs;
	cdi->cdi_segs = segs;
}

static void
iov_adj(struct iovec **iov, int *iovcnt, size_t count)
{
	struct iovec *iovtmp;
	int iovcnttmp;
	caddr_t ptmp;
	
	if (count > 0) {
		iovtmp = *iov;
		iovcnttmp = *iovcnt;
		while (count > 0) {
			if (count < iovtmp->iov_len) {
				ptmp = iovtmp->iov_base;
				ptmp += count; 
				iovtmp->iov_base = ptmp;
				iovtmp->iov_len -= count;
				break;
			} else 
				count -= iovtmp->iov_len;
			iovtmp++;
			iovcnttmp--;
		} 
		*iov = iovtmp;
		*iovcnt = iovcnttmp;
	} else if (count < 0) {
		iovtmp = &(*iov)[*iovcnt - 1];
		iovcnttmp = *iovcnt;
		while (count < 0) {
			if (-count < iovtmp->iov_len) {
				iovtmp->iov_len += count;
				break;
			} else
				count += iovtmp->iov_len;
			iovtmp--;
			iovcnttmp--;
		}
		*iovcnt = iovcnttmp;
	}
}

static void
cxgb_zero_copy_free(void *cl, void *arg)
{
	struct mbuf_vec *mv;
	struct mbuf *m = (struct mbuf *)cl;

	mv = mtomv(m);
	/*
	 * Physical addresses, don't try to free should be unheld separately from sbdrop
	 *
	 */
	mv->mv_count = 0;
	m_free_iovec(m, m->m_type);
}


static int
cxgb_hold_iovec_pages(struct uio *uio, vm_page_t *m, int *held, vm_prot_t prot)
{
	struct iovec *iov = uio->uio_iov;
	int iovcnt = uio->uio_iovcnt;
	int err, i, count, totcount, maxcount, totbytes, npages, curbytes;
	uint64_t start, end;
	vm_page_t *mp;
	vm_map_t map;

	map = &uio->uio_td->td_proc->p_vmspace->vm_map;
	totbytes = totcount = 0;
	maxcount = *held;

	mp = m;
	for (totcount = i = 0; (i < iovcnt) && (totcount < maxcount);  i++, iov++) {
		count = maxcount - totcount;
		    
		start = (uintptr_t)iov->iov_base;
		end = (uintptr_t)((caddr_t)iov->iov_base + iov->iov_len);
		start &= PG_FRAME;
		end += PAGE_MASK;
		end &= PG_FRAME;
		npages = (end - start) >> PAGE_SHIFT;
		
		count = min(count, npages);

		err = vm_fault_hold_user_pages(map,
			(vm_offset_t)iov->iov_base, mp, count, prot);
		mp += count;
		totcount += count;
		curbytes = iov->iov_len;
		if (count != npages)
			curbytes = count*PAGE_SIZE - (((uintptr_t)iov->iov_base)&PAGE_MASK);
		totbytes += curbytes;
	}
	uio->uio_resid -= totbytes;

	return (0);
}

/*
 * Returns whether a connection should enable DDP.  This happens when all of
 * the following conditions are met:
 * - the connection's ULP mode is DDP
 * - DDP is not already enabled
 * - the last receive was above the DDP threshold
 * - receive buffers are in user space
 * - receive side isn't shutdown (handled by caller)
 * - the connection's receive window is big enough so that sizable buffers
 *   can be posted without closing the window in the middle of DDP (checked
 *   when the connection is offloaded)
 */
static int
so_should_ddp(const struct toepcb *toep, int last_recv_len)
{

	DPRINTF("ulp_mode=%d last_recv_len=%d ddp_thresh=%d rcv_wnd=%ld ddp_copy_limit=%d\n",
	    toep->tp_ulp_mode, last_recv_len,  TOM_TUNABLE(toep->tp_toedev, ddp_thres),
	    toep->tp_tp->rcv_wnd, (TOM_TUNABLE(toep->tp_toedev, ddp_copy_limit) + DDP_RSVD_WIN));

	return toep->tp_ulp_mode == ULP_MODE_TCPDDP && (toep->tp_ddp_state.kbuf[0] == NULL) &&
	       last_recv_len > TOM_TUNABLE(toep->tp_toedev, ddp_thres) &&
	       toep->tp_tp->rcv_wnd > 
	           (TOM_TUNABLE(toep->tp_toedev, ddp_copy_limit) + DDP_RSVD_WIN);
}

static inline int
is_ddp(const struct mbuf *m)
{
	return ((m->m_flags & M_DDP) != 0);
}

static inline int
is_ddp_psh(const struct mbuf *m)
{
        return ((is_ddp(m) && (m->m_pkthdr.csum_flags & DDP_BF_PSH)) != 0);
}

static int
m_uiomove(const struct mbuf *m, int offset, int len, struct uio *uio)
{
	int curlen, startlen, resid_init, err = 0;
	caddr_t buf;

	DPRINTF("m_uiomove(m=%p, offset=%d, len=%d, ...)\n",
	    m, offset, len);

	startlen = len;
	resid_init = uio->uio_resid;
	while (m && len) {
		buf = mtod(m, caddr_t);
		curlen = m->m_len;
		if (offset && (offset < curlen)) {
			curlen -= offset;
			buf += offset;
			offset = 0;
		} else if (offset) {
			offset -= curlen;
			m = m->m_next;
			continue;
		}
		err = uiomove(buf, min(len, curlen), uio);
		if (err) {
			printf("uiomove returned %d\n", err);
			return (err);
		}
		
		len -= min(len, curlen);
		m = m->m_next;
	}
	DPRINTF("copied %d bytes - resid_init=%d uio_resid=%d\n",
	    startlen - len, resid_init, uio->uio_resid);
	return (err);
}

/*
 * Copy data from an sk_buff to an iovec.  Deals with RX_DATA, which carry the
 * data in the sk_buff body, and with RX_DATA_DDP, which place the data in a
 * DDP buffer.
 */
static inline int
copy_data(const struct mbuf *m, int offset, int len, struct uio *uio)
{
	struct iovec *to = uio->uio_iov;
	int err;
	
	if (__predict_true(!is_ddp(m)))                              /* RX_DATA */
		return m_uiomove(m, offset, len, uio);
	if (__predict_true(m->m_ddp_flags & DDP_BF_NOCOPY)) { /* user DDP */
		to->iov_len -= len;
		to->iov_base = ((caddr_t)to->iov_base) + len;
		uio->uio_iov = to;
		uio->uio_resid -= len;
		return (0);
	}
	err = t3_ddp_copy(m, offset, uio, len);             /* kernel DDP */
	return (err);
}

static void
cxgb_wait_dma_completion(struct toepcb *toep)
{
	struct rwlock *lock;
	
	lock = &toep->tp_tp->t_inpcb->inp_lock;
	inp_wlock(toep->tp_tp->t_inpcb);
	cv_wait_unlock(&toep->tp_cv, lock);
}

static int
cxgb_vm_page_to_miov(struct toepcb *toep, struct uio *uio, struct mbuf **m)
{
	int i, seg_count, err, type;
	struct mbuf *m0;
	struct cxgb_dma_info cdi;
	struct mbuf_vec *mv;
	struct mbuf_iovec *mi;
	bus_dma_segment_t *segs;
	
	err = bus_dmamap_load_uio(toep->tp_tx_dmat, toep->tp_dmamap, uio,
	    cxgb_dma_callback, &cdi, 0);

	if (err)
		return (err);
	seg_count = cdi.cdi_nsegs;	
	if ((m0 = mcl_alloc(seg_count, &type)) == NULL) {
		bus_dmamap_unload(toep->tp_tx_dmat, toep->tp_dmamap);
		return (ENOMEM);
	}
	segs = cdi.cdi_segs;
	m0->m_type = type;
	m0->m_flags = (M_EXT|M_NOFREE);
	m0->m_ext.ext_type = EXT_EXTREF;
	m0->m_ext.ext_free = cxgb_zero_copy_free;
#if __FreeBSD_version >= 800016
	m0->m_ext.ext_arg1 = NULL;	/* XXX: probably wrong /phk */
	m0->m_ext.ext_arg2 = NULL;
#else
	m0->m_ext.ext_args = NULL;
#endif
    
	mv = mtomv(m0);
	mv->mv_count = seg_count;
	mv->mv_first = 0;
	for (i = 0, mi = mv->mv_vec; i < seg_count; mi++, segs++, i++)
		mi_collapse_sge(mi, segs);

	*m = m0;

	/*
	 * This appears to be a no-op at the moment
	 * as busdma is all or nothing need to make
	 * sure the tag values are large enough
	 *
	 */
	if (cdi.cdi_mapped < uio->uio_resid) {
		uio->uio_resid -= cdi.cdi_mapped;
	} else
		uio->uio_resid = 0;

	return (0);
}

static int
t3_sosend(struct socket *so, struct uio *uio)
{
	int rv, count, hold_resid, sent, iovcnt;
	struct iovec iovtmp[TMP_IOV_MAX], *iovtmpp, *iov;
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct mbuf *m;
	struct uio uiotmp;
	struct sockbuf *snd;
	
	/*
	 * Events requiring iteration:
	 *  - number of pages exceeds max hold pages for process or system
	 *  - number of pages exceeds maximum sg entries for a single WR
	 *
	 * We're limited to holding 128 pages at once - and we're limited to
	 * 34 SG entries per work request, but each SG entry can be any number 
	 * of contiguous pages
	 *
	 */

	uiotmp = *uio;
	iovcnt = uio->uio_iovcnt;
	iov = uio->uio_iov;
	sent = 0;
	snd = so_sockbuf_snd(so);
sendmore:
	/*
	 * Make sure we don't exceed the socket buffer
	 */
	count = min(toep->tp_page_count, (sockbuf_sbspace(snd) >> PAGE_SHIFT) + 2*PAGE_SIZE);
	rv = cxgb_hold_iovec_pages(&uiotmp, toep->tp_pages, &count, VM_PROT_READ);
	hold_resid = uiotmp.uio_resid;
	if (rv)
		return (rv);

	/*
	 * Bump past sent and shave off the unheld amount
	 */
	if (hold_resid  > 0) {
		iovtmpp = iovtmp;
		memcpy(iovtmp, iov, iovcnt*sizeof(*iov));
		if (sent)
			iov_adj(&iovtmpp, &iovcnt, sent);
		iov_adj(&iovtmpp, &iovcnt, -hold_resid);
		uiotmp.uio_iov = iovtmpp;
		uiotmp.uio_iovcnt = iovcnt;

	}
	uiotmp.uio_resid = uio->uio_resid - hold_resid;
	
	/*
	 * Push off all held pages
	 *
	 */
	while (uiotmp.uio_resid > 0) {
		rv = cxgb_vm_page_to_miov(toep, &uiotmp, &m);
		if (rv) {
			vm_page_unhold_pages(toep->tp_pages, count);
			return (rv);
		}
		uio->uio_resid -= m->m_pkthdr.len;
		sent += m->m_pkthdr.len;
		sbappend(snd, m);
		t3_push_frames(so, TRUE);
		iov_adj(&uiotmp.uio_iov, &iovcnt, uiotmp.uio_resid);
	}

	/*
	 * Wait for pending I/O to be DMA'd to the card 
	 * 
	 */
	cxgb_wait_dma_completion(toep);
	vm_page_unhold_pages(toep->tp_pages, count);
	/*
	 * If there is more data to send adjust local copy of iov
	 * to point to teh start
	 */
	if (hold_resid) {
		iovtmpp = iovtmp;
		memcpy(iovtmp, iov, iovcnt*sizeof(*iov));
		iov_adj(&iovtmpp, &iovcnt, sent);
		uiotmp = *uio;
		uiotmp.uio_iov = iovtmpp;
		uiotmp.uio_iovcnt = iovcnt;
		goto sendmore;
	}

	return (0);
}

static int
cxgb_sosend(struct socket *so, struct sockaddr *addr, struct uio *uio,
    struct mbuf *top, struct mbuf *control, int flags, struct thread *td)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toedev *tdev; 
	int zcopy_thres, zcopy_enabled, rv;

	/*
	 * In order to use DMA direct from userspace the following
	 * conditions must be met:
	 *  - the connection is currently offloaded
	 *  - ddp is enabled
	 *  - the number of bytes to be transferred exceeds the threshold
	 *  - the number of bytes currently in flight won't exceed the in-flight
	 *    threshold XXX TODO
	 *  - vm_fault_hold_user_pages succeeds
	 *  - blocking socket XXX for now
	 *
	 */
	if (tp && tp->t_flags & TF_TOE) {
		struct toepcb *toep = tp->t_toe;
		
		tdev = toep->tp_toedev;
		zcopy_thres = TOM_TUNABLE(tdev, zcopy_sosend_partial_thres);
		zcopy_enabled = TOM_TUNABLE(tdev, zcopy_sosend_enabled);

		if (uio && (uio->uio_resid > zcopy_thres) &&
		    (uio->uio_iovcnt < TMP_IOV_MAX) &&  ((so_state_get(so) & SS_NBIO) == 0)
		    && zcopy_enabled) {
			rv = t3_sosend(so, uio);
			if (rv != EAGAIN)
				return (rv);
		}
	}
	return pru_sosend(so, addr, uio, top, control, flags, td);
}

/*
 * Following replacement or removal of the first mbuf on the first mbuf chain
 * of a socket buffer, push necessary state changes back into the socket
 * buffer so that other consumers see the values consistently.  'nextrecord'
 * is the callers locally stored value of the original value of
 * sb->sb_mb->m_nextpkt which must be restored when the lead mbuf changes.
 * NOTE: 'nextrecord' may be NULL.
 */
static __inline void
sockbuf_pushsync(struct sockbuf *sb, struct mbuf *nextrecord)
{
	sockbuf_lock_assert(sb);
	/*
	 * First, update for the new value of nextrecord.  If necessary, make
	 * it the first record.
	 */
	if (sb->sb_mb != NULL)
		sb->sb_mb->m_nextpkt = nextrecord;
	else
		sb->sb_mb = nextrecord;

        /*
         * Now update any dependent socket buffer fields to reflect the new
         * state.  This is an expanded inline of SB_EMPTY_FIXUP(), with the
	 * addition of a second clause that takes care of the case where
	 * sb_mb has been updated, but remains the last record.
         */
        if (sb->sb_mb == NULL) {
                sb->sb_mbtail = NULL;
                sb->sb_lastrecord = NULL;
        } else if (sb->sb_mb->m_nextpkt == NULL)
                sb->sb_lastrecord = sb->sb_mb;
}

#define IS_NONBLOCKING(so)	(so_state_get(so) & SS_NBIO)

static int
t3_soreceive(struct socket *so, int *flagsp, struct uio *uio)
{
	struct tcpcb *tp = so_sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct mbuf *m;
	uint32_t offset;
	int err, flags, avail, len, copied, copied_unacked;
	int target;		/* Read at least this many bytes */
	int user_ddp_ok;
	struct ddp_state *p;
	struct inpcb *inp = so_sotoinpcb(so);
	int socket_state, socket_error;
	struct sockbuf *rcv;
	
	avail = offset = copied = copied_unacked = 0;
	flags = flagsp ? (*flagsp &~ MSG_EOR) : 0;
	rcv = so_sockbuf_rcv(so);
	
	err = sblock(rcv, SBLOCKWAIT(flags));
	p = &toep->tp_ddp_state;

	if (err)
		return (err);

	rcv = so_sockbuf_rcv(so);
	sockbuf_lock(rcv);
	if ((tp->t_flags & TF_TOE) == 0) {
		sockbuf_unlock(rcv);
		err = EAGAIN;
		goto done_unlocked;
	}
	
	p->user_ddp_pending = 0;
restart:
	if ((tp->t_flags & TF_TOE) == 0) {
		sockbuf_unlock(rcv);
		err = EAGAIN;
		goto done_unlocked;
	}

	len = uio->uio_resid;
	m = rcv->sb_mb;
	target = (flags & MSG_WAITALL) ? len : rcv->sb_lowat;
	user_ddp_ok = p->ubuf_ddp_ready;
	p->cancel_ubuf = 0;
	
	if (len == 0)
		goto done;
	if (m) 
		goto got_mbuf;

	/* empty receive queue */
	if (copied >= target && (rcv->sb_mb == NULL) &&
	    !p->user_ddp_pending)
		goto done;

	socket_state = so_state_get(so);
	socket_error = so_error_get(so);
	rcv = so_sockbuf_rcv(so);
	
	if (copied) {
		if (socket_error || tp->t_state == TCPS_CLOSED || 
		    (socket_state & (SS_ISDISCONNECTING|SS_ISDISCONNECTED)))
			goto done;
	} else {
		if (socket_state & SS_NOFDREF)
			goto done;
		if (socket_error) {
			err = socket_error;
			socket_error = 0;
			goto done;
		}
		if (rcv->sb_state & SBS_CANTRCVMORE) 
			goto done;
		if (socket_state & (SS_ISDISCONNECTING|SS_ISDISCONNECTED))
			goto done;
		if (tp->t_state == TCPS_CLOSED) {
			err = ENOTCONN; 
			goto done;
		}
	}
	if (rcv->sb_mb && !p->user_ddp_pending) {
		sockbuf_unlock(rcv);
		inp_wlock(inp);
		t3_cleanup_rbuf(tp, copied_unacked);
		inp_wunlock(inp);
		sockbuf_lock(rcv);
		copied_unacked = 0;
		goto restart;
	}
	if (p->kbuf[0] && user_ddp_ok && !p->user_ddp_pending && 
	    uio->uio_iov->iov_len > p->kbuf[0]->dgl_length &&
	    p->ubuf_ddp_ready) {
		p->user_ddp_pending =
		    !t3_overlay_ubuf(toep, rcv, uio,
			IS_NONBLOCKING(so), flags, 1, 1);
		if (p->user_ddp_pending) {
			p->kbuf_posted++;
			user_ddp_ok = 0;
		}
	}
	if (p->kbuf[0] && (p->kbuf_posted == 0)) {
		t3_post_kbuf(toep, 1, IS_NONBLOCKING(so));
		p->kbuf_posted++;
	}
	if (p->user_ddp_pending) {
		/* One shot at DDP if we already have enough data */
		if (copied >= target)
			user_ddp_ok = 0;

		if (rcv->sb_state & SBS_CANTRCVMORE) 
			goto done;
		CTR0(KTR_TOM, "ddp pending -- waiting");
		if ((err = sbwait(rcv)) != 0)
			goto done;
//for timers to work			await_ddp_completion(sk, flags, &timeo);
	} else if (copied >= target)
		goto done;
	else {
		if (copied_unacked) {
			int i = 0;

			sockbuf_unlock(rcv);
			inp_wlock(inp);
			t3_cleanup_rbuf(tp, copied_unacked);
			inp_wunlock(inp);
			copied_unacked = 0;
			if (mp_ncpus > 1)
				while (i++ < 200 && rcv->sb_mb == NULL)
					cpu_spinwait();
			sockbuf_lock(rcv);
		}
		if (rcv->sb_mb)
			goto restart;

		if (rcv->sb_state & SBS_CANTRCVMORE)
			goto done;

		CTR0(KTR_TOM, "no buffers -- waiting");

		if ((err = sbwait(rcv)) != 0) 
			goto done;
	}
     	goto restart;
got_mbuf:
	/*
	 * Adjust the mbuf seqno if it has already been partially processed by
	 * soreceive_generic
	 */
	if (m->m_pkthdr.len != m->m_len) {
		m->m_seq += m->m_pkthdr.len - m->m_len;
		m->m_pkthdr.len = m->m_len;
	}
	    
	CTR6(KTR_TOM, "t3_soreceive: ddp_flags=0x%x m_len=%u resid=%u "
	    "m_seq=0x%08x c_seq=0x%08x c_unack=%u",
	    (is_ddp(m) ? m->m_ddp_flags : 0), m->m_pkthdr.len, len,
	    m->m_seq, toep->tp_copied_seq, copied_unacked);
	KASSERT(((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_EXTREF)) || !(m->m_flags & M_EXT),
	    ("unexpected type M_EXT=%d ext_type=%d m_len=%d m_pktlen=%d\n", !!(m->m_flags & M_EXT),
		m->m_ext.ext_type, m->m_len, m->m_pkthdr.len));
	KASSERT(m->m_next != (struct mbuf *)0xffffffff, ("bad next value m_next=%p m_nextpkt=%p"
		" m_flags=0x%x m->m_len=%d", m->m_next, m->m_nextpkt, m->m_flags, m->m_len));
	if (m->m_pkthdr.len == 0) {
		if ((m->m_ddp_flags & DDP_BF_NOCOPY) == 0)
			panic("empty mbuf and NOCOPY not set\n");
		CTR0(KTR_TOM, "ddp done notification");
		p->user_ddp_pending = 0;
		sbdroprecord_locked(rcv);
		goto done;
	}

	KASSERT((int32_t)(toep->tp_copied_seq + copied_unacked - m->m_seq) >= 0,
	    ("offset will go negative: offset=%d copied_seq=0x%08x copied_unacked=%d m_seq=0x%08x",
		offset, toep->tp_copied_seq, copied_unacked, m->m_seq));
	offset = toep->tp_copied_seq + copied_unacked - m->m_seq;
	
	if (offset >= m->m_pkthdr.len)
		panic("t3_soreceive: OFFSET >= LEN offset %d copied_seq 0x%x "
		    "seq 0x%x pktlen %d ddp flags 0x%x", offset,
		    toep->tp_copied_seq + copied_unacked, m->m_seq,
		    m->m_pkthdr.len, m->m_ddp_flags);

	avail = m->m_pkthdr.len - offset;
	if (len < avail) {
		if (is_ddp(m) && (m->m_ddp_flags & DDP_BF_NOCOPY)) 
			panic("bad state in t3_soreceive len=%d avail=%d offset=%d\n", len, avail, offset);
		avail = len;
		rcv->sb_flags |= SB_IN_TOE;
	} else if (p->kbuf_posted == 0 && p->user_ddp_pending == 0)
		rcv->sb_flags &= ~SB_IN_TOE;
		
#ifdef URGENT_DATA_SUPPORTED
	/*
	 * Check if the data we are preparing to copy contains urgent
	 * data.  Either stop short of urgent data or skip it if it's
	 * first and we are not delivering urgent data inline.
	 */
	if (__predict_false(toep->tp_urg_data)) {
		uint32_t urg_offset = tp->rcv_up - tp->copied_seq + copied_unacked;
		
		if (urg_offset < avail) {
			if (urg_offset) {
				/* stop short of the urgent data */
				avail = urg_offset;
			} else if ((so_options_get(so) & SO_OOBINLINE) == 0) {
				/* First byte is urgent, skip */
				toep->tp_copied_seq++;
				offset++;
				avail--;
				if (!avail)
					goto skip_copy;
			}	
		}	
	}	
#endif
	if (is_ddp_psh(m) || offset || (rcv->sb_mb && !is_ddp(m))) {
		user_ddp_ok = 0;
#ifdef T3_TRACE	
		T3_TRACE0(TIDTB(so), "t3_sosend: PSH");
#endif	
	}
	
	if (user_ddp_ok && !p->user_ddp_pending &&
	    uio->uio_iov->iov_len > p->kbuf[0]->dgl_length &&
	    p->ubuf_ddp_ready) {
		p->user_ddp_pending = 
		    !t3_overlay_ubuf(toep, rcv, uio,
			IS_NONBLOCKING(so), flags, 1, 1);
		if (p->user_ddp_pending) {
			p->kbuf_posted++;
			user_ddp_ok = 0;
		}
		DPRINTF("user_ddp_pending=%d\n", p->user_ddp_pending);
	} else
		DPRINTF("user_ddp_ok=%d user_ddp_pending=%d iov_len=%ld dgl_length=%d ubuf_ddp_ready=%d ulp_mode=%d is_ddp(m)=%d flags=0x%x ubuf=%p kbuf_posted=%d\n",
		    user_ddp_ok, p->user_ddp_pending, uio->uio_iov->iov_len, p->kbuf[0] ? p->kbuf[0]->dgl_length : 0,
		    p->ubuf_ddp_ready, toep->tp_ulp_mode, !!is_ddp(m), m->m_ddp_flags, p->ubuf, p->kbuf_posted);
	
	/*
	 * If MSG_TRUNC is specified the data is discarded.
	 * XXX need to check pr_atomic
	 */
	KASSERT(avail > 0, ("avail=%d resid=%d offset=%d", avail,  uio->uio_resid, offset));
	if (__predict_true(!(flags & MSG_TRUNC))) {
		int resid = uio->uio_resid;
		
		sockbuf_unlock(rcv);
		if ((err = copy_data(m, offset, avail, uio))) {
			if (err)
				err = EFAULT;
			goto done_unlocked;
		}
			    
		sockbuf_lock(rcv);
		if (avail != (resid - uio->uio_resid))
			printf("didn't copy all bytes :-/ avail=%d offset=%d pktlen=%d resid=%d uio_resid=%d copied=%d copied_unacked=%d is_ddp(m)=%d\n",
			    avail, offset, m->m_pkthdr.len, resid, uio->uio_resid, copied, copied_unacked, is_ddp(m));

		if ((tp->t_flags & TF_TOE) == 0) {
			sockbuf_unlock(rcv);
			err = EAGAIN;
			goto done_unlocked;
		}
	}
	
	copied += avail;
	copied_unacked += avail;
	len -= avail;
	
#ifdef URGENT_DATA_SUPPORTED
skip_copy:
	if (tp->urg_data && after(tp->copied_seq + copied_unacked, tp->urg_seq))
		tp->urg_data = 0;
#endif
	/*
	 * If the buffer is fully consumed free it.  If it's a DDP
	 * buffer also handle any events it indicates.
	 */
	if (avail + offset >= m->m_pkthdr.len) {
		unsigned int fl = m->m_ddp_flags;
		int exitnow, got_psh = 0, nomoredata = 0;
		int count;
		struct mbuf *nextrecord;

		if (p->kbuf[0] != NULL && is_ddp(m) && (fl & 1)) {
			if (is_ddp_psh(m) && p->user_ddp_pending)
				got_psh = 1;
			
			if (fl & DDP_BF_NOCOPY)
				p->user_ddp_pending = 0;
			else if ((fl & DDP_BF_NODATA) && IS_NONBLOCKING(so)) {
				p->kbuf_posted--;
				nomoredata = 1;
			} else {
				p->kbuf_posted--;
				p->ubuf_ddp_ready = 1;
			}
		}

		nextrecord = m->m_nextpkt;
		count = m->m_pkthdr.len;
		while (count > 0) {
			count -= m->m_len;
			KASSERT(((m->m_flags & M_EXT) && (m->m_ext.ext_type == EXT_EXTREF)) || !(m->m_flags & M_EXT), ("unexpected type M_EXT=%d ext_type=%d m_len=%d\n", !!(m->m_flags & M_EXT), m->m_ext.ext_type, m->m_len));
			CTR2(KTR_TOM, "freeing mbuf m_len = %d pktlen = %d", m->m_len, m->m_pkthdr.len);
			sbfree(rcv, m);
			rcv->sb_mb = m_free(m);
			m = rcv->sb_mb;
		}
		sockbuf_pushsync(rcv, nextrecord);
#if 0
		sbdrop_locked(rcv, m->m_pkthdr.len);
#endif		
		exitnow = got_psh || nomoredata;
		if  (copied >= target && (rcv->sb_mb == NULL) && exitnow)
			goto done;
		if (copied_unacked > (rcv->sb_hiwat >> 2)) {
			sockbuf_unlock(rcv);
			inp_wlock(inp);
			t3_cleanup_rbuf(tp, copied_unacked);
			inp_wunlock(inp);
			copied_unacked = 0;
			sockbuf_lock(rcv);
		}
	} 
	if (len > 0)
		goto restart;

	done:
	if ((tp->t_flags & TF_TOE) == 0) {
		sockbuf_unlock(rcv);
		err = EAGAIN;
		goto done_unlocked;
	}
	/*
	 * If we can still receive decide what to do in preparation for the
	 * next receive.  Note that RCV_SHUTDOWN is set if the connection
	 * transitioned to CLOSE but not if it was in that state to begin with.
	 */
	if (__predict_true((so_state_get(so) & (SS_ISDISCONNECTING|SS_ISDISCONNECTED)) == 0)) {
		if (p->user_ddp_pending) {
			user_ddp_ok = 0;
			t3_cancel_ubuf(toep, rcv);
			if (rcv->sb_mb) {
				if (copied < 0)
					copied = 0;
				if (len > 0)
					goto restart;
			}
			p->user_ddp_pending = 0;
		}
		if ((p->kbuf[0] != NULL) && (p->kbuf_posted == 0)) {
#ifdef T3_TRACE
			T3_TRACE0(TIDTB(so),
			  "chelsio_recvmsg: about to exit, repost kbuf");
#endif

			t3_post_kbuf(toep, 1, IS_NONBLOCKING(so));
			p->kbuf_posted++;
		} else if (so_should_ddp(toep, copied) && uio->uio_iovcnt == 1) {
			CTR1(KTR_TOM ,"entering ddp on tid=%u", toep->tp_tid);
			if (!t3_enter_ddp(toep, TOM_TUNABLE(toep->tp_toedev,
				    ddp_copy_limit), 0, IS_NONBLOCKING(so))) {
				rcv->sb_flags |= SB_IN_TOE;
				p->kbuf_posted = 1;
			}
			
		}
	}
#ifdef T3_TRACE
	T3_TRACE5(TIDTB(so),
		  "chelsio_recvmsg <-: copied %d len %d buffers_freed %d "
		  "kbuf_posted %d user_ddp_pending %u",
		  copied, len, buffers_freed, p ? p->kbuf_posted : -1, 
	    p->user_ddp_pending);
#endif
	sockbuf_unlock(rcv);
done_unlocked:	
	if (copied_unacked && (tp->t_flags & TF_TOE)) {
		inp_wlock(inp);
		t3_cleanup_rbuf(tp, copied_unacked);
		inp_wunlock(inp);
	}
	sbunlock(rcv);

	return (err);
}

static int
cxgb_soreceive(struct socket *so, struct sockaddr **psa, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct toedev *tdev;
	int rv, zcopy_thres, zcopy_enabled, flags;
	struct tcpcb *tp = so_sototcpcb(so);
	struct sockbuf *rcv = so_sockbuf_rcv(so);
	
	flags = flagsp ? *flagsp &~ MSG_EOR : 0;
	
	/*
	 * In order to use DMA direct from userspace the following
	 * conditions must be met:
	 *  - the connection is currently offloaded
	 *  - ddp is enabled
	 *  - the number of bytes to be transferred exceeds the threshold
	 *  - the number of bytes currently in flight won't exceed the in-flight
	 *    threshold XXX TODO
	 *  - vm_fault_hold_user_pages succeeds
	 *  - blocking socket XXX for now
	 *  - iovcnt is 1
	 *
	 */
	if (tp && (tp->t_flags & TF_TOE) && uio && ((flags & (MSG_OOB|MSG_PEEK|MSG_DONTWAIT)) == 0)
	    && (uio->uio_iovcnt == 1) && (mp0 == NULL) &&
	    ((rcv->sb_flags & SB_IN_TOE) || (uio->uio_iovcnt == 1))) {
		struct toepcb *toep = tp->t_toe;
		
		tdev =  toep->tp_toedev;
		zcopy_thres = TOM_TUNABLE(tdev, ddp_thres);
		zcopy_enabled = TOM_TUNABLE(tdev, ddp);
		if ((rcv->sb_flags & SB_IN_TOE) ||((uio->uio_resid > zcopy_thres) &&
			(uio->uio_iovcnt == 1) && zcopy_enabled)) {
			CTR4(KTR_TOM, "cxgb_soreceive: sb_flags=0x%x t_flags=0x%x flags=0x%x uio_resid=%d",
			    rcv->sb_flags, tp->t_flags, flags, uio->uio_resid);
			rv = t3_soreceive(so, flagsp, uio);
			if (rv != EAGAIN)
				return (rv);
			else
				printf("returned EAGAIN\n");
		} 
	} else if (tp && (tp->t_flags & TF_TOE) && uio && mp0 == NULL) {
		struct sockbuf *rcv = so_sockbuf_rcv(so);
		
		log(LOG_INFO, "skipping t3_soreceive flags=0x%x iovcnt=%d sb_state=0x%x\n",
		    flags, uio->uio_iovcnt, rcv->sb_state);
	}
	
	return pru_soreceive(so, psa, uio, mp0, controlp, flagsp);
}

struct protosw cxgb_protosw;
struct pr_usrreqs cxgb_tcp_usrreqs;

void
t3_install_socket_ops(struct socket *so)
{
	static int copied = 0;
	struct pr_usrreqs *pru;
	struct protosw *psw;
	
	if (copied == 0) {
		psw = so_protosw_get(so);	
		pru = psw->pr_usrreqs;

		bcopy(psw, &cxgb_protosw, sizeof(*psw));
		bcopy(pru, &cxgb_tcp_usrreqs, sizeof(*pru));

		cxgb_protosw.pr_ctloutput = t3_ctloutput;
		cxgb_protosw.pr_usrreqs = &cxgb_tcp_usrreqs;
		cxgb_tcp_usrreqs.pru_sosend = cxgb_sosend;
		cxgb_tcp_usrreqs.pru_soreceive = cxgb_soreceive;
	}
	so_protosw_set(so, &cxgb_protosw);
	
#if 0	
	so->so_proto->pr_usrreqs->pru_sosend = cxgb_sosend;
	so->so_proto->pr_usrreqs->pru_soreceive = cxgb_soreceive;
#endif
}
