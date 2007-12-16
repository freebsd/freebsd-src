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
#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/socketvar.h>
#include <sys/uio.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/in_pcb.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>


#include <dev/cxgb/cxgb_osdep.h>
#include <dev/cxgb/sys/mbufq.h>

#include <netinet/tcp.h>
#include <netinet/tcp_var.h>
#include <netinet/tcp_fsm.h>
#include <netinet/tcp_ofld.h>
#include <net/route.h>

#include <dev/cxgb/t3cdev.h>
#include <dev/cxgb/common/cxgb_firmware_exports.h>
#include <dev/cxgb/common/cxgb_t3_cpl.h>
#include <dev/cxgb/common/cxgb_tcb.h>
#include <dev/cxgb/common/cxgb_ctl_defs.h>
#include <dev/cxgb/cxgb_l2t.h>
#include <dev/cxgb/cxgb_offload.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/pmap.h>

#include <dev/cxgb/sys/mvec.h>
#include <dev/cxgb/ulp/toecore/cxgb_toedev.h>
#include <dev/cxgb/ulp/tom/cxgb_defs.h>
#include <dev/cxgb/ulp/tom/cxgb_tom.h>
#include <dev/cxgb/ulp/tom/cxgb_t3_ddp.h>
#include <dev/cxgb/ulp/tom/cxgb_toepcb.h>

static int	(*pru_sosend)(struct socket *so, struct sockaddr *addr,
    struct uio *uio, struct mbuf *top, struct mbuf *control,
    int flags, struct thread *td);

static int	(*pru_soreceive)(struct socket *so, struct sockaddr **paddr,
    struct uio *uio, struct mbuf **mp0, struct mbuf **controlp,
    int *flagsp);

#ifdef notyet
#define VM_HOLD_WRITEABLE	0x1
static int  vm_fault_hold_user_pages(vm_offset_t addr, int len, vm_page_t *mp,
    int *count, int flags);
#endif
static void vm_fault_unhold_pages(vm_page_t *m, int count);



#define TMP_IOV_MAX 16

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
cxgb_zero_copy_free(void *cl, void *arg) {}

static int
cxgb_hold_iovec_pages(struct uio *uio, vm_page_t *m, int *held, int flags)
{

	return (EINVAL);
}

static void
cxgb_wait_dma_completion(struct toepcb *tp)
{
	
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
	m0->m_ext.ext_args = NULL;
    
	mv = mtomv(m0);
	mv->mv_count = seg_count;
	mv->mv_first = 0;
	for (i = 0, mi = mv->mv_vec; i < seg_count; mi++, segs++, i++)
		mi_collapse_sge(mi, segs);

	*m = m0;
	
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
	struct tcpcb *tp = sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct mbuf *m;
	struct uio uiotmp;

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
sendmore:
	/*
	 * Make sure we don't exceed the socket buffer
	 */
	count = min(toep->tp_page_count, (sbspace(&so->so_snd) >> PAGE_SHIFT) + 2*PAGE_SIZE);
	rv = cxgb_hold_iovec_pages(&uiotmp, toep->tp_pages, &count, 0);
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
			vm_fault_unhold_pages(toep->tp_pages, count);
			return (rv);
		}
		uio->uio_resid -= m->m_pkthdr.len;
		sent += m->m_pkthdr.len;
		sbappend_locked(&so->so_snd, m);
		t3_push_frames(so, TRUE);
		iov_adj(&uiotmp.uio_iov, &iovcnt, uiotmp.uio_resid);
	}
	/*
	 * Wait for pending I/O to be DMA'd to the card 
	 * 
	 */
	cxgb_wait_dma_completion(toep);
	vm_fault_unhold_pages(toep->tp_pages, count);
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
	struct tcpcb *tp = sototcpcb(so);
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
	if (tp->t_flags & TF_TOE) {
		tdev = TOE_DEV(so);
		zcopy_thres = TOM_TUNABLE(tdev, zcopy_sosend_partial_thres);
		zcopy_enabled = TOM_TUNABLE(tdev, zcopy_sosend_enabled);

		if ((uio->uio_resid > zcopy_thres) &&
		    (uio->uio_iovcnt < TMP_IOV_MAX) &&  ((so->so_state & SS_NBIO) == 0)
		    && zcopy_enabled) {
			rv = t3_sosend(so, uio);
			if (rv != EAGAIN)
				return (rv);
		}
	}
	return pru_sosend(so, addr, uio, top, control, flags, td);
}


static int
t3_soreceive(struct socket *so, struct uio *uio)
{
#ifdef notyet
	int i, rv, count, hold_resid, sent, iovcnt;
	struct iovec iovtmp[TMP_IOV_MAX], *iovtmpp, *iov;
	struct tcpcb *tp = sototcpcb(so);
	struct toepcb *toep = tp->t_toe;
	struct mbuf *m;
	struct uio uiotmp;

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
	re;
#endif  
	return (0);
}

static int
cxgb_soreceive(struct socket *so, struct sockaddr **psa, struct uio *uio,
    struct mbuf **mp0, struct mbuf **controlp, int *flagsp)
{
	struct toedev *tdev;
	int rv, zcopy_thres, zcopy_enabled;
	struct tcpcb *tp = sototcpcb(so);

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
	if (tp->t_flags & TF_TOE) {
		tdev =  TOE_DEV(so);
		zcopy_thres = TOM_TUNABLE(tdev, ddp_thres);
		zcopy_enabled = TOM_TUNABLE(tdev, ddp);
		if ((uio->uio_resid > zcopy_thres) &&
		    (uio->uio_iovcnt == 1) &&  ((so->so_state & SS_NBIO) == 0)
		    && zcopy_enabled) {
			rv = t3_soreceive(so, uio);
			if (rv != EAGAIN)
				return (rv);
		}
	}
	
	return pru_soreceive(so, psa, uio, mp0, controlp, flagsp);
}


void
t3_install_socket_ops(struct socket *so)
{
	so->so_proto->pr_usrreqs->pru_sosend = cxgb_sosend;
	so->so_proto->pr_usrreqs->pru_soreceive = cxgb_soreceive;
}

/*
 * This routine takes a user address range and does the following:
 *  - validate that the user has access to those pages (flags indicates read or write) - if not fail
 *  - validate that count is enough to hold range number of pages - if not fail
 *  - fault in any non-resident pages
 *  - if the user is doing a read force a write fault for any COWed pages
 *  - if the user is doing a read mark all pages as dirty
 *  - hold all pages
 *  - return number of pages in count
 */
#ifdef notyet
static int
vm_fault_hold_user_pages(vm_offset_t addr, int len, vm_page_t *mp, int *count, int flags)
{

	vm_offset_t start, va;
	vm_paddr_t pa;
	int pageslen, faults, rv;
	
	struct thread *td;
	vm_map_t map;
	pmap_t pmap;
	vm_page_t m, *pages;
	vm_prot_t prot;
	
	start = addr & ~PAGE_MASK;
	pageslen = roundup2(addr + len, PAGE_SIZE);
	if (*count < (pageslen >> PAGE_SHIFT))
		return (EFBIG);

	*count = pageslen >> PAGE_SHIFT;
	/*
	 * Check that virtual address range is legal
	 * This check is somewhat bogus as on some architectures kernel
	 * and user do not share VA - however, it appears that all FreeBSD
	 * architectures define it
	 */
	if (addr + len > VM_MAXUSER_ADDRESS)
		return (EFAULT);
	
	td = curthread;
	map = &td->td_proc->p_vmspace->vm_map;
	pmap = &td->td_proc->p_vmspace->vm_pmap;
	pages = mp;

	prot = (flags & VM_HOLD_WRITEABLE) ? VM_PROT_WRITE : VM_PROT_READ;
	bzero(pages, sizeof(vm_page_t *) * (*count));
retry:
	
	/*
	 * First optimistically assume that all pages are resident (and R/W if for write)
	 * if so just mark pages as held (and dirty if for write) and return
	 */
	vm_page_lock_queues();
	for (pages = mp, faults = 0, va = start; va < pageslen; va += PAGE_SIZE, pages++) {
		/*
		 * Assure that we only hold the page once
		 */
		if (*pages == NULL) {
			/*
			 * page queue mutex is recursable so this is OK
			 * it would be really nice if we had an unlocked version of this so
			 * we were only acquiring the pmap lock 1 time as opposed to potentially
			 * many dozens of times
			 */
			m = pmap_extract_and_hold(pmap, va, prot);
			if (m == NULL) {
				faults++;
				continue;
			}
			*pages = m;
		if (flags & VM_HOLD_WRITEABLE)
			vm_page_dirty(m);
		}
	}
	vm_page_unlock_queues();
	
	if (faults == 0) 
		return (0);
	/*
	 * Pages either have insufficient permissions or are not present
	 * trigger a fault where neccessary
	 * 
	 */
	for (va = start; va < pageslen; va += PAGE_SIZE) {
		m = NULL;
		pa = pmap_extract(pmap, va);
		rv = 0;
		if (pa)
			m = PHYS_TO_VM_PAGE(pa);
		if (flags & VM_HOLD_WRITEABLE) {
			if (m == NULL  || (m->flags & PG_WRITEABLE) == 0)
				rv = vm_fault(map, va, VM_PROT_WRITE, VM_FAULT_DIRTY);
		} else if (m == NULL)
			rv = vm_fault(map, va, VM_PROT_READ, VM_FAULT_NORMAL);
		if (rv)
			goto error;
	} 
	goto retry;

error:	
	vm_page_lock_queues();
	for (pages = mp, va = start; va < pageslen; va += PAGE_SIZE, pages++) 
		if (*pages)
			vm_page_unhold(*pages);
	vm_page_unlock_queues();
	return (EFAULT);
}
#endif

static void
vm_fault_unhold_pages(vm_page_t *mp, int count)
{

	KASSERT(count >= 0, ("negative count %d", count));
	vm_page_lock_queues();
	while (count--) {
		vm_page_unhold(*mp);
		mp++;
	}
	vm_page_unlock_queues();
}

