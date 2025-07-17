/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2012 EMC Corp.
 * All rights reserved.
 *
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include "opt_bus.h"
#include "opt_iommu.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/ktr.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mbuf.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/pmap.h>

#include <opencrypto/cryptodev.h>

#include <machine/bus.h>

/*
 * Convenience function for manipulating driver locks from busdma (during
 * busdma_swi, for example).
 */
void
busdma_lock_mutex(void *arg, bus_dma_lock_op_t op)
{
	struct mtx *dmtx;

	dmtx = (struct mtx *)arg;
	switch (op) {
	case BUS_DMA_LOCK:
		mtx_lock(dmtx);
		break;
	case BUS_DMA_UNLOCK:
		mtx_unlock(dmtx);
		break;
	default:
		panic("Unknown operation 0x%x for busdma_lock_mutex!", op);
	}
}

/*
 * dflt_lock should never get called.  It gets put into the dma tag when
 * lockfunc == NULL, which is only valid if the maps that are associated
 * with the tag are meant to never be deferred.
 *
 * XXX Should have a way to identify which driver is responsible here.
 */
void
_busdma_dflt_lock(void *arg, bus_dma_lock_op_t op)
{

	panic("driver error: _bus_dma_dflt_lock called");
}


/*
 * Load up data starting at offset within a region specified by a
 * list of virtual address ranges until either length or the region
 * are exhausted.
 */
static int
_bus_dmamap_load_vlist(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dma_segment_t *list, int sglist_cnt, struct pmap *pmap, int *nsegs,
    int flags, size_t offset, size_t length)
{
	int error;

	error = 0;
	for (; sglist_cnt > 0 && length != 0; sglist_cnt--, list++) {
		char *addr;
		size_t ds_len;

		KASSERT((offset < list->ds_len),
		    ("Invalid mid-segment offset"));
		addr = (char *)(uintptr_t)list->ds_addr + offset;
		ds_len = list->ds_len - offset;
		offset = 0;
		if (ds_len > length)
			ds_len = length;
		length -= ds_len;
		KASSERT((ds_len != 0), ("Segment length is zero"));
		error = _bus_dmamap_load_buffer(dmat, map, addr, ds_len, pmap,
		    flags, NULL, nsegs);
		if (error)
			break;
	}
	return (error);
}

/*
 * Load a list of physical addresses.
 */
static int
_bus_dmamap_load_plist(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dma_segment_t *list, int sglist_cnt, int *nsegs, int flags)
{
	int error;

	error = 0;
	for (; sglist_cnt > 0; sglist_cnt--, list++) {
		error = _bus_dmamap_load_phys(dmat, map,
		    (vm_paddr_t)list->ds_addr, list->ds_len, flags, NULL,
		    nsegs);
		if (error)
			break;
	}
	return (error);
}

/*
 * Load an unmapped mbuf
 */
static int
_bus_dmamap_load_mbuf_epg(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct mbuf *m, bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error, i, off, len, pglen, pgoff, seglen, segoff;

	M_ASSERTEXTPG(m);

	len = m->m_len;
	error = 0;

	/* Skip over any data removed from the front. */
	off = mtod(m, vm_offset_t);

	if (m->m_epg_hdrlen != 0) {
		if (off >= m->m_epg_hdrlen) {
			off -= m->m_epg_hdrlen;
		} else {
			seglen = m->m_epg_hdrlen - off;
			segoff = off;
			seglen = min(seglen, len);
			off = 0;
			len -= seglen;
			error = _bus_dmamap_load_buffer(dmat, map,
			    &m->m_epg_hdr[segoff], seglen, kernel_pmap,
			    flags, segs, nsegs);
		}
	}
	pgoff = m->m_epg_1st_off;
	for (i = 0; i < m->m_epg_npgs && error == 0 && len > 0; i++) {
		pglen = m_epg_pagelen(m, i, pgoff);
		if (off >= pglen) {
			off -= pglen;
			pgoff = 0;
			continue;
		}
		seglen = pglen - off;
		segoff = pgoff + off;
		off = 0;
		seglen = min(seglen, len);
		len -= seglen;
		error = _bus_dmamap_load_phys(dmat, map,
		    m->m_epg_pa[i] + segoff, seglen, flags, segs, nsegs);
		pgoff = 0;
	};
	if (len != 0 && error == 0) {
		KASSERT((off + len) <= m->m_epg_trllen,
		    ("off + len > trail (%d + %d > %d)", off, len,
		    m->m_epg_trllen));
		error = _bus_dmamap_load_buffer(dmat, map,
		    &m->m_epg_trail[off], len, kernel_pmap, flags, segs,
		    nsegs);
	}
	return (error);
}

/*
 * Load a single mbuf.
 */
static int
_bus_dmamap_load_single_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct mbuf *m, bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error;

	error = 0;
	if ((m->m_flags & M_EXTPG) != 0)
		error = _bus_dmamap_load_mbuf_epg(dmat, map, m, segs, nsegs,
		    flags);
	else
		error = _bus_dmamap_load_buffer(dmat, map, m->m_data, m->m_len,
		    kernel_pmap, flags | BUS_DMA_LOAD_MBUF, segs, nsegs);
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, *nsegs);
	return (error);
}

/*
 * Load an mbuf chain.
 */
static int
_bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct mbuf *m0, bus_dma_segment_t *segs, int *nsegs, int flags)
{
	struct mbuf *m;
	int error;

	error = 0;
	for (m = m0; m != NULL && error == 0; m = m->m_next) {
		if (m->m_len > 0) {
			if ((m->m_flags & M_EXTPG) != 0)
				error = _bus_dmamap_load_mbuf_epg(dmat,
				    map, m, segs, nsegs, flags);
			else
				error = _bus_dmamap_load_buffer(dmat, map,
				    m->m_data, m->m_len, kernel_pmap,
				    flags | BUS_DMA_LOAD_MBUF, segs, nsegs);
		}
	}
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, *nsegs);
	return (error);
}

int
bus_dmamap_load_ma_triv(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct vm_page **ma, bus_size_t tlen, int ma_offs, int flags,
    bus_dma_segment_t *segs, int *segp)
{
	vm_paddr_t paddr;
	bus_size_t len;
	int error, i;

	error = 0;
	for (i = 0; tlen > 0; i++, tlen -= len) {
		len = min(PAGE_SIZE - ma_offs, tlen);
		paddr = VM_PAGE_TO_PHYS(ma[i]) + ma_offs;
		error = _bus_dmamap_load_phys(dmat, map, paddr, len,
		    flags, segs, segp);
		if (error != 0)
			break;
		ma_offs = 0;
	}
	return (error);
}

/*
 * Load a uio.
 */
static int
_bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    int *nsegs, int flags)
{
	bus_size_t resid;
	bus_size_t minlen;
	struct iovec *iov;
	pmap_t pmap;
	caddr_t addr;
	int error, i;

	if (uio->uio_segflg == UIO_USERSPACE) {
		KASSERT(uio->uio_td != NULL,
			("bus_dmamap_load_uio: USERSPACE but no proc"));
		pmap = vmspace_pmap(uio->uio_td->td_proc->p_vmspace);
	} else
		pmap = kernel_pmap;
	resid = uio->uio_resid;
	iov = uio->uio_iov;
	error = 0;

	for (i = 0; i < uio->uio_iovcnt && resid != 0 && !error; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */

		addr = (caddr_t) iov[i].iov_base;
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		if (minlen > 0) {
			error = _bus_dmamap_load_buffer(dmat, map, addr,
			    minlen, pmap, flags, NULL, nsegs);
			resid -= minlen;
		}
	}

	return (error);
}

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	struct memdesc mem;
	int error;
	int nsegs;

#ifdef KMSAN
	mem = memdesc_vaddr(buf, buflen);
	_bus_dmamap_load_kmsan(dmat, map, &mem);
#endif

	if ((flags & BUS_DMA_NOWAIT) == 0) {
		mem = memdesc_vaddr(buf, buflen);
		_bus_dmamap_waitok(dmat, map, &mem, callback, callback_arg);
	}

	nsegs = -1;
	error = _bus_dmamap_load_buffer(dmat, map, buf, buflen, kernel_pmap,
	    flags, NULL, &nsegs);
	nsegs++;

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);

	if (error == EINPROGRESS)
		return (error);

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, 0);

	/*
	 * Return ENOMEM to the caller so that it can pass it up the stack.
	 * This error only happens when NOWAIT is set, so deferral is disabled.
	 */
	if (error == ENOMEM)
		return (error);

	return (0);
}

int
bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int nsegs, error;

	M_ASSERTPKTHDR(m0);

#ifdef KMSAN
	struct memdesc mem = memdesc_mbuf(m0);
	_bus_dmamap_load_kmsan(dmat, map, &mem);
#endif

	flags |= BUS_DMA_NOWAIT;
	nsegs = -1;
	error = _bus_dmamap_load_mbuf_sg(dmat, map, m0, NULL, &nsegs, flags);
	++nsegs;

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, m0->m_pkthdr.len, error);

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);
	return (error);
}

int
bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error;

#ifdef KMSAN
	struct memdesc mem = memdesc_mbuf(m0);
	_bus_dmamap_load_kmsan(dmat, map, &mem);
#endif

	flags |= BUS_DMA_NOWAIT;
	*nsegs = -1;
	error = _bus_dmamap_load_mbuf_sg(dmat, map, m0, segs, nsegs, flags);
	++*nsegs;
	_bus_dmamap_complete(dmat, map, segs, *nsegs, error);
	return (error);
}

int
bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int nsegs, error;

#ifdef KMSAN
	struct memdesc mem = memdesc_uio(uio);
	_bus_dmamap_load_kmsan(dmat, map, &mem);
#endif

	flags |= BUS_DMA_NOWAIT;
	nsegs = -1;
	error = _bus_dmamap_load_uio(dmat, map, uio, &nsegs, flags);
	nsegs++;

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, uio->uio_resid, error);

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);
	return (error);
}

int
bus_dmamap_load_bio(bus_dma_tag_t dmat, bus_dmamap_t map, struct bio *bio,
		    bus_dmamap_callback_t *callback, void *callback_arg,
		    int flags)
{
	struct memdesc mem;

	mem = memdesc_bio(bio);
	return (bus_dmamap_load_mem(dmat, map, &mem, callback, callback_arg,
	    flags));
}

int
bus_dmamap_load_mem(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct memdesc *mem, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int error;
	int nsegs;

#ifdef KMSAN
	_bus_dmamap_load_kmsan(dmat, map, mem);
#endif

	if ((flags & BUS_DMA_NOWAIT) == 0)
		_bus_dmamap_waitok(dmat, map, mem, callback, callback_arg);

	nsegs = -1;
	error = 0;
	switch (mem->md_type) {
	case MEMDESC_VADDR:
		error = _bus_dmamap_load_buffer(dmat, map, mem->u.md_vaddr,
		    mem->md_len, kernel_pmap, flags, NULL, &nsegs);
		break;
	case MEMDESC_PADDR:
		error = _bus_dmamap_load_phys(dmat, map, mem->u.md_paddr,
		    mem->md_len, flags, NULL, &nsegs);
		break;
	case MEMDESC_VLIST:
		error = _bus_dmamap_load_vlist(dmat, map, mem->u.md_list,
		    mem->md_nseg, kernel_pmap, &nsegs, flags, 0, SIZE_T_MAX);
		break;
	case MEMDESC_PLIST:
		error = _bus_dmamap_load_plist(dmat, map, mem->u.md_list,
		    mem->md_nseg, &nsegs, flags);
		break;
	case MEMDESC_UIO:
		error = _bus_dmamap_load_uio(dmat, map, mem->u.md_uio,
		    &nsegs, flags);
		break;
	case MEMDESC_MBUF:
		error = _bus_dmamap_load_mbuf_sg(dmat, map, mem->u.md_mbuf,
		    NULL, &nsegs, flags);
		break;
	case MEMDESC_VMPAGES:
		error = _bus_dmamap_load_ma(dmat, map, mem->u.md_ma,
		    mem->md_len, mem->md_offset, flags, NULL, &nsegs);
		break;
	}
	nsegs++;

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);

	if (error == EINPROGRESS)
		return (error);

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, 0);

	/*
	 * Return ENOMEM to the caller so that it can pass it up the stack.
	 * This error only happens when NOWAIT is set, so deferral is disabled.
	 */
	if (error == ENOMEM)
		return (error);

	return (0);
}

int
bus_dmamap_load_crp_buffer(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct crypto_buffer *cb, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
	bus_dma_segment_t *segs;
	int error;
	int nsegs;

	flags |= BUS_DMA_NOWAIT;
	nsegs = -1;
	error = 0;
	switch (cb->cb_type) {
	case CRYPTO_BUF_CONTIG:
		error = _bus_dmamap_load_buffer(dmat, map, cb->cb_buf,
		    cb->cb_buf_len, kernel_pmap, flags, NULL, &nsegs);
		break;
	case CRYPTO_BUF_MBUF:
		error = _bus_dmamap_load_mbuf_sg(dmat, map, cb->cb_mbuf,
		    NULL, &nsegs, flags);
		break;
	case CRYPTO_BUF_SINGLE_MBUF:
		error = _bus_dmamap_load_single_mbuf(dmat, map, cb->cb_mbuf,
		    NULL, &nsegs, flags);
		break;
	case CRYPTO_BUF_UIO:
		error = _bus_dmamap_load_uio(dmat, map, cb->cb_uio, &nsegs,
		    flags);
		break;
	case CRYPTO_BUF_VMPAGE:
		error = _bus_dmamap_load_ma(dmat, map, cb->cb_vm_page,
		    cb->cb_vm_page_len, cb->cb_vm_page_offset, flags, NULL,
		    &nsegs);
		break;
	default:
		error = EINVAL;
	}
	nsegs++;

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, flags, error, nsegs);

	if (error == EINPROGRESS)
		return (error);

	segs = _bus_dmamap_complete(dmat, map, NULL, nsegs, error);
	if (error)
		(*callback)(callback_arg, segs, 0, error);
	else
		(*callback)(callback_arg, segs, nsegs, 0);

	/*
	 * Return ENOMEM to the caller so that it can pass it up the stack.
	 * This error only happens when NOWAIT is set, so deferral is disabled.
	 */
	if (error == ENOMEM)
		return (error);

	return (0);
}

int
bus_dmamap_load_crp(bus_dma_tag_t dmat, bus_dmamap_t map, struct cryptop *crp,
    bus_dmamap_callback_t *callback, void *callback_arg, int flags)
{
	return (bus_dmamap_load_crp_buffer(dmat, map, &crp->crp_buf, callback,
	    callback_arg, flags));
}

void
bus_dma_template_init(bus_dma_template_t *t, bus_dma_tag_t parent)
{

	if (t == NULL)
		return;

	t->parent = parent;
	t->alignment = 1;
	t->boundary = 0;
	t->lowaddr = t->highaddr = BUS_SPACE_MAXADDR;
	t->maxsize = t->maxsegsize = BUS_SPACE_MAXSIZE;
	t->nsegments = BUS_SPACE_UNRESTRICTED;
	t->lockfunc = NULL;
	t->lockfuncarg = NULL;
	t->flags = 0;
}

int
bus_dma_template_tag(bus_dma_template_t *t, bus_dma_tag_t *dmat)
{

	if (t == NULL || dmat == NULL)
		return (EINVAL);

	return (bus_dma_tag_create(t->parent, t->alignment, t->boundary,
	    t->lowaddr, t->highaddr, NULL, NULL, t->maxsize,
	    t->nsegments, t->maxsegsize, t->flags, t->lockfunc, t->lockfuncarg,
	    dmat));
}

void
bus_dma_template_fill(bus_dma_template_t *t, bus_dma_param_t *kv, u_int count)
{
	bus_dma_param_t *pkv;

	while (count) {
		pkv = &kv[--count];
		switch (pkv->key) {
		case BD_PARAM_PARENT:
			t->parent = pkv->ptr;
			break;
		case BD_PARAM_ALIGNMENT:
			t->alignment = pkv->num;
			break;
		case BD_PARAM_BOUNDARY:
			t->boundary = pkv->num;
			break;
		case BD_PARAM_LOWADDR:
			t->lowaddr = pkv->pa;
			break;
		case BD_PARAM_HIGHADDR:
			t->highaddr = pkv->pa;
			break;
		case BD_PARAM_MAXSIZE:
			t->maxsize = pkv->num;
			break;
		case BD_PARAM_NSEGMENTS:
			t->nsegments = pkv->num;
			break;
		case BD_PARAM_MAXSEGSIZE:
			t->maxsegsize = pkv->num;
			break;
		case BD_PARAM_FLAGS:
			t->flags = pkv->num;
			break;
		case BD_PARAM_LOCKFUNC:
			t->lockfunc = pkv->ptr;
			break;
		case BD_PARAM_LOCKFUNCARG:
			t->lockfuncarg = pkv->ptr;
			break;
		case BD_PARAM_NAME:
			t->name = pkv->ptr;
			break;
		case BD_PARAM_INVALID:
		default:
			KASSERT(0, ("Invalid key %d\n", pkv->key));
			break;
		}
	}
	return;
}

#ifndef IOMMU
bool bus_dma_iommu_set_buswide(device_t dev);
int bus_dma_iommu_load_ident(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t start, vm_size_t length, int flags);

bool
bus_dma_iommu_set_buswide(device_t dev)
{
	return (false);
}

int
bus_dma_iommu_load_ident(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t start, vm_size_t length, int flags)
{
	return (0);
}
#endif
