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
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/condvar.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/sockstate.h>
#include <sys/sockopt.h>
#include <sys/socket.h>
#include <sys/sockbuf.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <machine/bus.h>

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


#define MAX_SCHEDULE_TIMEOUT	300

/*
 * Return the # of page pods needed to accommodate a # of pages.
 */
static inline unsigned int
pages2ppods(unsigned int pages)
{
	return (pages + PPOD_PAGES - 1) / PPOD_PAGES + NUM_SENTINEL_PPODS;
}

/**
 *	t3_pin_pages - pin a user memory range and prepare it for DDP
 *	@addr - the starting address
 *	@len - the length of the range
 *	@newgl - contains the pages and physical addresses of the pinned range
 *	@gl - an existing gather list, may be %NULL
 *
 *	Pins the pages in the user-space memory range [addr, addr + len) and
 *	maps them for DMA.  Returns a gather list with the pinned pages and
 *	their physical addresses.  If @gl is non NULL the pages it describes
 *	are compared against the pages for [addr, addr + len), and if the
 *	existing gather list already covers the range a new list is not
 *	allocated.  Returns 0 on success, or a negative errno.  On success if
 *	a new gather list was allocated it is returned in @newgl.
 */ 
static int
t3_pin_pages(bus_dma_tag_t tag, bus_dmamap_t dmamap, vm_offset_t addr,
    size_t len, struct ddp_gather_list **newgl,
    const struct ddp_gather_list *gl)
{
	int i = 0, err;
	size_t pg_off;
	unsigned int npages;
	struct ddp_gather_list *p;
	vm_map_t map;
	
	/*
	 * XXX need x86 agnostic check
	 */
	if (addr + len > VM_MAXUSER_ADDRESS)
		return (EFAULT);


	
	pg_off = addr & PAGE_MASK;
	npages = (pg_off + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	p = malloc(sizeof(struct ddp_gather_list) + npages * sizeof(vm_page_t *),
	    M_DEVBUF, M_NOWAIT|M_ZERO);
	if (p == NULL)
		return (ENOMEM);

	map = &curthread->td_proc->p_vmspace->vm_map;
	err = vm_fault_hold_user_pages(map, addr, p->dgl_pages, npages,
	    VM_PROT_READ | VM_PROT_WRITE);
	if (err)
		goto free_gl;

	if (gl && gl->dgl_offset == pg_off && gl->dgl_nelem >= npages &&
	    gl->dgl_length >= len) {
		for (i = 0; i < npages; i++)
			if (p->dgl_pages[i] != gl->dgl_pages[i])
				goto different_gl;
		err = 0;
		goto unpin;
	}

different_gl:
	p->dgl_length = len;
	p->dgl_offset = pg_off;
	p->dgl_nelem = npages;
#ifdef NEED_BUSDMA
	p->phys_addr[0] = pci_map_page(pdev, p->pages[0], pg_off,
				       PAGE_SIZE - pg_off,
				       PCI_DMA_FROMDEVICE) - pg_off;
	for (i = 1; i < npages; ++i)
		p->phys_addr[i] = pci_map_page(pdev, p->pages[i], 0, PAGE_SIZE,
					       PCI_DMA_FROMDEVICE);
#endif	
	*newgl = p;
	return (0);
unpin:
	vm_fault_unhold_pages(p->dgl_pages, npages);

free_gl:
	
	free(p, M_DEVBUF);
	*newgl = NULL;
	return (err);
}

static void
unmap_ddp_gl(const struct ddp_gather_list *gl)
{
#ifdef NEED_BUSDMA	
	int i;

	if (!gl->nelem)
		return;

	pci_unmap_page(pdev, gl->phys_addr[0] + gl->offset,
		       PAGE_SIZE - gl->offset, PCI_DMA_FROMDEVICE);
	for (i = 1; i < gl->nelem; ++i)
		pci_unmap_page(pdev, gl->phys_addr[i], PAGE_SIZE,
			       PCI_DMA_FROMDEVICE);

#endif
}

static void
ddp_gl_free_pages(struct ddp_gather_list *gl, int dirty)
{
	/*
	 * XXX mark pages as dirty before unholding 
	 */
	vm_fault_unhold_pages(gl->dgl_pages, gl->dgl_nelem);
}

void
t3_free_ddp_gl(struct ddp_gather_list *gl)
{
	unmap_ddp_gl(gl);
	ddp_gl_free_pages(gl, 0);
	free(gl, M_DEVBUF);
}

/* Max # of page pods for a buffer, enough for 1MB buffer at 4KB page size */
#define MAX_PPODS 64U

/*
 * Allocate page pods for DDP buffer 1 (the user buffer) and set up the tag in
 * the TCB.  We allocate page pods in multiples of PPOD_CLUSTER_SIZE.  First we
 * try to allocate enough page pods to accommodate the whole buffer, subject to
 * the MAX_PPODS limit.  If that fails we try to allocate PPOD_CLUSTER_SIZE page
 * pods before failing entirely.
 */
static int
alloc_buf1_ppods(struct toepcb *toep, struct ddp_state *p,
			    unsigned long addr, unsigned int len)
{
	int err, tag, npages, nppods;
	struct tom_data *d = TOM_DATA(toep->tp_toedev);

#if 0
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#endif	
	npages = ((addr & PAGE_MASK) + len + PAGE_SIZE - 1) >> PAGE_SHIFT;
	nppods = min(pages2ppods(npages), MAX_PPODS);
	nppods = roundup2(nppods, PPOD_CLUSTER_SIZE);
	err = t3_alloc_ppods(d, nppods, &tag);
	if (err && nppods > PPOD_CLUSTER_SIZE) {
		nppods = PPOD_CLUSTER_SIZE;
		err = t3_alloc_ppods(d, nppods, &tag);
	}
	if (err)
		return (ENOMEM);

	p->ubuf_nppods = nppods;
	p->ubuf_tag = tag;
#if NUM_DDP_KBUF == 1
	t3_set_ddp_tag(toep, 1, tag << 6);
#endif
	return (0);
}

/*
 * Starting offset for the user DDP buffer.  A non-0 value ensures a DDP flush
 * won't block indefinitely if there's nothing to place (which should be rare).
 */
#define UBUF_OFFSET 1

static __inline unsigned long
select_ddp_flags(const struct toepcb *toep, int buf_idx,
                 int nonblock, int rcv_flags)
{
	if (buf_idx == 1) {
		if (__predict_false(rcv_flags & MSG_WAITALL))
			return V_TF_DDP_PSH_NO_INVALIDATE0(1) |
			       V_TF_DDP_PSH_NO_INVALIDATE1(1) |
			       V_TF_DDP_PUSH_DISABLE_1(1);
		if (nonblock)
			return V_TF_DDP_BUF1_FLUSH(1);

		return V_TF_DDP_BUF1_FLUSH(!TOM_TUNABLE(toep->tp_toedev,
							ddp_push_wait));
	}

	if (__predict_false(rcv_flags & MSG_WAITALL))
		return V_TF_DDP_PSH_NO_INVALIDATE0(1) |
		       V_TF_DDP_PSH_NO_INVALIDATE1(1) |
		       V_TF_DDP_PUSH_DISABLE_0(1);
	if (nonblock)
		return V_TF_DDP_BUF0_FLUSH(1);

	return V_TF_DDP_BUF0_FLUSH(!TOM_TUNABLE(toep->tp_toedev, ddp_push_wait));
}

/*
 * Reposts the kernel DDP buffer after it has been previously become full and
 * invalidated.  We just need to reset the offset and adjust the DDP flags.
 * Conveniently, we can set the flags and the offset with a single message.
 * Note that this function does not set the buffer length.  Again conveniently
 * our kernel buffer is of fixed size.  If the length needs to be changed it
 * needs to be done separately.
 */
static void
t3_repost_kbuf(struct toepcb *toep, unsigned int bufidx, int modulate, 
    int activate, int nonblock)
{
	struct ddp_state *p = &toep->tp_ddp_state;
	unsigned long flags;

#if 0	
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#endif	
	p->buf_state[bufidx].cur_offset = p->kbuf[bufidx]->dgl_offset;
	p->buf_state[bufidx].flags = p->kbuf_noinval ? DDP_BF_NOINVAL : 0;
	p->buf_state[bufidx].gl = p->kbuf[bufidx];
	p->cur_buf = bufidx;
	p->kbuf_idx = bufidx;

	flags = select_ddp_flags(toep, bufidx, nonblock, 0);
	if (!bufidx)
		t3_setup_ddpbufs(toep, 0, 0, 0, 0, flags |
			 V_TF_DDP_PSH_NO_INVALIDATE0(p->kbuf_noinval) |
			 V_TF_DDP_PSH_NO_INVALIDATE1(p->kbuf_noinval) |
		         V_TF_DDP_BUF0_VALID(1),
		         V_TF_DDP_BUF0_FLUSH(1) |
			 V_TF_DDP_PSH_NO_INVALIDATE0(1) |
		         V_TF_DDP_PSH_NO_INVALIDATE1(1) | V_TF_DDP_OFF(1) |
			 V_TF_DDP_BUF0_VALID(1) |
			 V_TF_DDP_ACTIVE_BUF(activate), modulate);
	else
		t3_setup_ddpbufs(toep, 0, 0, 0, 0, flags |
			 V_TF_DDP_PSH_NO_INVALIDATE0(p->kbuf_noinval) |	
		         V_TF_DDP_PSH_NO_INVALIDATE1(p->kbuf_noinval) | 
			 V_TF_DDP_BUF1_VALID(1) | 
			 V_TF_DDP_ACTIVE_BUF(activate),
		         V_TF_DDP_BUF1_FLUSH(1) | 
			 V_TF_DDP_PSH_NO_INVALIDATE0(1) |
		         V_TF_DDP_PSH_NO_INVALIDATE1(1) | V_TF_DDP_OFF(1) |
			 V_TF_DDP_BUF1_VALID(1) | V_TF_DDP_ACTIVE_BUF(1), 
			 modulate);
	
}

/**
 * setup_uio_ppods - setup HW page pods for a user iovec
 * @sk: the associated socket
 * @uio: the uio
 * @oft: additional bytes to map before the start of the buffer
 *
 * Pins a user iovec and sets up HW page pods for DDP into it.  We allocate
 * page pods for user buffers on the first call per socket.  Afterwards we
 * limit the buffer length to whatever the existing page pods can accommodate.
 * Returns a negative error code or the length of the mapped buffer.
 *
 * The current implementation handles iovecs with only one entry.
 */
static int
setup_uio_ppods(struct toepcb *toep, const struct uio *uio, int oft, int *length)
{
	int err;
	unsigned int len;
	struct ddp_gather_list *gl = NULL;
	struct ddp_state *p = &toep->tp_ddp_state;
	struct iovec *iov = uio->uio_iov;
	vm_offset_t addr = (vm_offset_t)iov->iov_base - oft;

#ifdef notyet	
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#endif
	if (__predict_false(p->ubuf_nppods == 0)) {
		err = alloc_buf1_ppods(toep, p, addr, iov->iov_len + oft);
		if (err)
			return (err);
	}

	len = (p->ubuf_nppods - NUM_SENTINEL_PPODS) * PPOD_PAGES * PAGE_SIZE;
	len -= addr & PAGE_MASK;
	if (len > M_TCB_RX_DDP_BUF0_LEN)
		len = M_TCB_RX_DDP_BUF0_LEN;
	len = min(len, toep->tp_tp->rcv_wnd - 32768);
	len = min(len, iov->iov_len + oft);

	if (len <= p->kbuf[0]->dgl_length) {
		printf("length too short\n");
		return (EINVAL);
	}
	
	err = t3_pin_pages(toep->tp_rx_dmat, toep->tp_dmamap, addr, len, &gl, p->ubuf);
	if (err)
		return (err);
	if (gl) {
		if (p->ubuf)
			t3_free_ddp_gl(p->ubuf);
		p->ubuf = gl;
		t3_setup_ppods(toep, gl, pages2ppods(gl->dgl_nelem), p->ubuf_tag, len,
			       gl->dgl_offset, 0);
	}
	*length = len;
	return (0);
}

/*
 * 
 */
void
t3_cancel_ubuf(struct toepcb *toep, struct sockbuf *rcv)
{
	struct ddp_state *p = &toep->tp_ddp_state;
	int ubuf_pending = t3_ddp_ubuf_pending(toep);
	int err = 0, count = 0;
	
	if (p->ubuf == NULL)
		return;
	
	sockbuf_lock_assert(rcv);

	p->cancel_ubuf = 1;
	while (ubuf_pending && !(rcv->sb_state & SBS_CANTRCVMORE)) {
		CTR3(KTR_TOM,
		  "t3_cancel_ubuf: flags0 0x%x flags1 0x%x get_tcb_count %d",
		  p->buf_state[0].flags & (DDP_BF_NOFLIP | DDP_BF_NOCOPY), 
		  p->buf_state[1].flags & (DDP_BF_NOFLIP | DDP_BF_NOCOPY),
		  p->get_tcb_count);	
		if (p->get_tcb_count == 0)
			t3_cancel_ddpbuf(toep, p->cur_buf);
		else
			CTR5(KTR_TOM, "waiting err=%d get_tcb_count=%d timeo=%d rcv=%p SBS_CANTRCVMORE=%d",
			    err, p->get_tcb_count, rcv->sb_timeo, rcv,
			    !!(rcv->sb_state & SBS_CANTRCVMORE));
		
		while (p->get_tcb_count && !(rcv->sb_state & SBS_CANTRCVMORE)) {
			if (count & 0xfffffff)
				CTR5(KTR_TOM, "waiting err=%d get_tcb_count=%d timeo=%d rcv=%p count=%d",
				    err, p->get_tcb_count, rcv->sb_timeo, rcv, count);
			count++;
			err = sbwait(rcv);
		}
		ubuf_pending = t3_ddp_ubuf_pending(toep);
	}
	p->cancel_ubuf = 0;
	p->user_ddp_pending = 0;

}

#define OVERLAY_MASK (V_TF_DDP_PSH_NO_INVALIDATE0(1) | \
	              V_TF_DDP_PSH_NO_INVALIDATE1(1) | \
		      V_TF_DDP_BUF1_FLUSH(1) | \
		      V_TF_DDP_BUF0_FLUSH(1) | \
		      V_TF_DDP_PUSH_DISABLE_1(1) | \
		      V_TF_DDP_PUSH_DISABLE_0(1) | \
		      V_TF_DDP_INDICATE_OUT(1))

/*
 * Post a user buffer as an overlay on top of the current kernel buffer.
 */
int
t3_overlay_ubuf(struct toepcb *toep, struct sockbuf *rcv,
    const struct uio *uio, int nonblock, int rcv_flags,
    int modulate, int post_kbuf)
{
	int err, len, ubuf_idx;
	unsigned long flags;
	struct ddp_state *p = &toep->tp_ddp_state;

	if (p->kbuf[0] == NULL) {
		return (EINVAL);
	}
	sockbuf_unlock(rcv);
	err = setup_uio_ppods(toep, uio, 0, &len);
	sockbuf_lock(rcv);
	if (err)
		return (err);
	
	if ((rcv->sb_state & SBS_CANTRCVMORE) ||
	    (toep->tp_tp->t_flags & TF_TOE) == 0) 
		return (EINVAL);
		
	ubuf_idx = p->kbuf_idx;
	p->buf_state[ubuf_idx].flags = DDP_BF_NOFLIP;
	/* Use existing offset */
	/* Don't need to update .gl, user buffer isn't copied. */
	p->cur_buf = ubuf_idx;

	flags = select_ddp_flags(toep, ubuf_idx, nonblock, rcv_flags);

	if (post_kbuf) {
		struct ddp_buf_state *dbs = &p->buf_state[ubuf_idx ^ 1];
		
		dbs->cur_offset = 0;
		dbs->flags = 0;
		dbs->gl = p->kbuf[ubuf_idx ^ 1];
		p->kbuf_idx ^= 1;
		flags |= p->kbuf_idx ?
		    V_TF_DDP_BUF1_VALID(1) | V_TF_DDP_PUSH_DISABLE_1(0) :
		    V_TF_DDP_BUF0_VALID(1) | V_TF_DDP_PUSH_DISABLE_0(0);
	}
	
	if (ubuf_idx == 0) {
		t3_overlay_ddpbuf(toep, 0, p->ubuf_tag << 6, p->kbuf_tag[1] << 6,
				  len);
		t3_setup_ddpbufs(toep, 0, 0, p->kbuf[1]->dgl_length, 0,
				 flags,
				 OVERLAY_MASK | flags, 1);
	} else {
		t3_overlay_ddpbuf(toep, 1, p->kbuf_tag[0] << 6, p->ubuf_tag << 6,
				  len);
		t3_setup_ddpbufs(toep, p->kbuf[0]->dgl_length, 0, 0, 0,
				 flags,
				 OVERLAY_MASK | flags, 1);
	}
#ifdef T3_TRACE
	T3_TRACE5(TIDTB(so),
		  "t3_overlay_ubuf: tag %u flags 0x%x mask 0x%x ubuf_idx %d "
		  " kbuf_idx %d",
		   p->ubuf_tag, flags, OVERLAY_MASK, ubuf_idx, p->kbuf_idx);
#endif
	CTR3(KTR_TOM,
	    "t3_overlay_ubuf: tag %u flags 0x%x mask 0x%x",
	    p->ubuf_tag, flags, OVERLAY_MASK);
	CTR3(KTR_TOM,
	    "t3_overlay_ubuf:  ubuf_idx %d kbuf_idx %d post_kbuf %d",
	    ubuf_idx, p->kbuf_idx, post_kbuf);
	    
	return (0);
}

/*
 * Clean up DDP state that needs to survive until socket close time, such as the
 * DDP buffers.  The buffers are already unmapped at this point as unmapping
 * needs the PCI device and a socket may close long after the device is removed.
 */
void
t3_cleanup_ddp(struct toepcb *toep)
{
	struct ddp_state *p = &toep->tp_ddp_state;
	int idx;

	for (idx = 0; idx < NUM_DDP_KBUF; idx++)
		if (p->kbuf[idx]) {
			ddp_gl_free_pages(p->kbuf[idx], 0);
			free(p->kbuf[idx], M_DEVBUF);
		}
	if (p->ubuf) {
		ddp_gl_free_pages(p->ubuf, 0);
		free(p->ubuf, M_DEVBUF);
		p->ubuf = NULL;
	}
	toep->tp_ulp_mode = 0;
}

/*
 * This is a companion to t3_cleanup_ddp() and releases the HW resources
 * associated with a connection's DDP state, such as the page pods.
 * It's called when HW is done with a connection.   The rest of the state
 * remains available until both HW and the app are done with the connection.
 */
void
t3_release_ddp_resources(struct toepcb *toep)
{
	struct ddp_state *p = &toep->tp_ddp_state;
	struct tom_data *d = TOM_DATA(toep->tp_toedev);
	int idx;
	
	for (idx = 0; idx < NUM_DDP_KBUF; idx++) {
		t3_free_ppods(d, p->kbuf_tag[idx], 
		    p->kbuf_nppods[idx]);
		unmap_ddp_gl(p->kbuf[idx]);
	}

	if (p->ubuf_nppods) {
		t3_free_ppods(d, p->ubuf_tag, p->ubuf_nppods);
		p->ubuf_nppods = 0;
	}
	if (p->ubuf)
		unmap_ddp_gl(p->ubuf);
	
}

void
t3_post_kbuf(struct toepcb *toep, int modulate, int nonblock)
{
	struct ddp_state *p = &toep->tp_ddp_state;

	t3_set_ddp_tag(toep, p->cur_buf, p->kbuf_tag[p->cur_buf] << 6);
	t3_set_ddp_buf(toep, p->cur_buf, 0, p->kbuf[p->cur_buf]->dgl_length);
	t3_repost_kbuf(toep, p->cur_buf, modulate, 1, nonblock);
#ifdef T3_TRACE
	T3_TRACE1(TIDTB(so),
		  "t3_post_kbuf: cur_buf = kbuf_idx = %u ", p->cur_buf);
#endif
	CTR1(KTR_TOM,
		  "t3_post_kbuf: cur_buf = kbuf_idx = %u ", p->cur_buf);
}

/*
 * Prepare a socket for DDP.  Must be called when the socket is known to be
 * open.
 */
int
t3_enter_ddp(struct toepcb *toep, unsigned int kbuf_size, unsigned int waitall, int nonblock)
{
	int i, err = ENOMEM;
	static vm_pindex_t color;
	unsigned int nppods, kbuf_pages, idx = 0;
	struct ddp_state *p = &toep->tp_ddp_state;
	struct tom_data *d = TOM_DATA(toep->tp_toedev);

	
	if (kbuf_size > M_TCB_RX_DDP_BUF0_LEN)
		return (EINVAL);

#ifdef notyet	
	SOCKBUF_LOCK_ASSERT(&so->so_rcv);
#endif	
	kbuf_pages = (kbuf_size + PAGE_SIZE - 1) >> PAGE_SHIFT;
	nppods = pages2ppods(kbuf_pages);

	p->kbuf_noinval = !!waitall;
	p->kbuf_tag[NUM_DDP_KBUF - 1] = -1;
	for (idx = 0; idx < NUM_DDP_KBUF; idx++) {
		p->kbuf[idx] = 
		    malloc(sizeof (struct ddp_gather_list) + kbuf_pages *
			sizeof(vm_page_t *), M_DEVBUF, M_NOWAIT|M_ZERO);
		if (p->kbuf[idx] == NULL)
			goto err;
		err = t3_alloc_ppods(d, nppods, &p->kbuf_tag[idx]);
		if (err) {
			printf("t3_alloc_ppods failed err=%d\n", err);
			goto err;
		}
		
		p->kbuf_nppods[idx] = nppods;
		p->kbuf[idx]->dgl_length = kbuf_size;
		p->kbuf[idx]->dgl_offset = 0;
		p->kbuf[idx]->dgl_nelem = kbuf_pages;

		for (i = 0; i < kbuf_pages; ++i) {
			p->kbuf[idx]->dgl_pages[i] = vm_page_alloc(NULL, color,
			    VM_ALLOC_NOOBJ | VM_ALLOC_NORMAL | VM_ALLOC_WIRED |
			    VM_ALLOC_ZERO);
			if (p->kbuf[idx]->dgl_pages[i] == NULL) {
				p->kbuf[idx]->dgl_nelem = i;
				printf("failed to allocate kbuf pages\n");
				goto err;
			}
		}
#ifdef NEED_BUSDMA
		/*
		 * XXX we'll need this for VT-d or any platform with an iommu :-/
		 *
		 */
		for (i = 0; i < kbuf_pages; ++i)
			p->kbuf[idx]->phys_addr[i] = 
			    pci_map_page(p->pdev, p->kbuf[idx]->pages[i],
					 0, PAGE_SIZE, PCI_DMA_FROMDEVICE);
#endif
		t3_setup_ppods(toep, p->kbuf[idx], nppods, p->kbuf_tag[idx], 
			       p->kbuf[idx]->dgl_length, 0, 0);
	}
	cxgb_log_tcb(TOEP_T3C_DEV(toep)->adapter, toep->tp_tid);

	t3_set_ddp_tag(toep, 0, p->kbuf_tag[0] << 6);
	t3_set_ddp_buf(toep, 0, 0, p->kbuf[0]->dgl_length);
	t3_repost_kbuf(toep, 0, 0, 1, nonblock);

	t3_set_rcv_coalesce_enable(toep, 
	    TOM_TUNABLE(toep->tp_toedev, ddp_rcvcoalesce));
	t3_set_dack_mss(toep, TOM_TUNABLE(toep->tp_toedev, delack)>>1);
	
#ifdef T3_TRACE
	T3_TRACE4(TIDTB(so),
		  "t3_enter_ddp: kbuf_size %u waitall %u tag0 %d tag1 %d",
		   kbuf_size, waitall, p->kbuf_tag[0], p->kbuf_tag[1]);
#endif
	CTR4(KTR_TOM,
		  "t3_enter_ddp: kbuf_size %u waitall %u tag0 %d tag1 %d",
		   kbuf_size, waitall, p->kbuf_tag[0], p->kbuf_tag[1]);
	cxgb_log_tcb(TOEP_T3C_DEV(toep)->adapter, toep->tp_tid);
	return (0);

err:
	t3_release_ddp_resources(toep);
	t3_cleanup_ddp(toep);
	return (err);
}

int
t3_ddp_copy(const struct mbuf *m, int offset, struct uio *uio, int len)
{
	int resid_init, err;
	struct ddp_gather_list *gl = (struct ddp_gather_list *)m->m_ddp_gl;
	
	resid_init = uio->uio_resid;
	
	if (!gl->dgl_pages)
		panic("pages not set\n");

	CTR4(KTR_TOM, "t3_ddp_copy: offset=%d dgl_offset=%d cur_offset=%d len=%d",
	    offset, gl->dgl_offset, m->m_cur_offset, len);
	offset += gl->dgl_offset + m->m_cur_offset;
	KASSERT(len <= gl->dgl_length,
	    ("len=%d > dgl_length=%d in ddp_copy\n", len, gl->dgl_length));


	err = uiomove_fromphys(gl->dgl_pages, offset, len, uio);
	return (err);
}


/*
 * Allocate n page pods.  Returns -1 on failure or the page pod tag.
 */
int
t3_alloc_ppods(struct tom_data *td, unsigned int n, int *ptag)
{
	unsigned int i, j;

	if (__predict_false(!td->ppod_map)) {
		printf("ppod_map not set\n");
		return (EINVAL);
	}

	mtx_lock(&td->ppod_map_lock);
	for (i = 0; i < td->nppods; ) {
		
		for (j = 0; j < n; ++j)           /* scan ppod_map[i..i+n-1] */
			if (td->ppod_map[i + j]) {
				i = i + j + 1;
				goto next;
			}
		memset(&td->ppod_map[i], 1, n);   /* allocate range */
		mtx_unlock(&td->ppod_map_lock);
		CTR2(KTR_TOM,
		    "t3_alloc_ppods: n=%u tag=%u", n, i);
		*ptag = i;
		return (0);
	next: ;
	}
	mtx_unlock(&td->ppod_map_lock);
	return (0);
}

void
t3_free_ppods(struct tom_data *td, unsigned int tag, unsigned int n)
{
	/* No need to take ppod_lock here */
	memset(&td->ppod_map[tag], 0, n);
}
