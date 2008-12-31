/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * All rights reserved.
 * Copyright 2001 by Thomas Moestl <tmm@FreeBSD.org>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)machdep.c	8.6 (Berkeley) 1/14/94
 *	from: NetBSD: machdep.c,v 1.111 2001/09/15 07:13:40 eeh Exp
 *	and
 * 	from: FreeBSD: src/sys/i386/i386/busdma_machdep.c,v 1.24 2001/08/15
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/sun4v/sun4v/bus_machdep.c,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>

#include <machine/asi.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/smp.h>
#include <machine/tlb.h>

static void nexus_bus_barrier(bus_space_tag_t, bus_space_handle_t,
    bus_size_t, bus_size_t, int);

/* ASI's for bus access. */
int bus_type_asi[] = {
	ASI_REAL_IO,		/* nexus */
	ASI_REAL_IO,		/* SBus */
	ASI_REAL_IO_L,		/* PCI configuration space */
	ASI_REAL_IO_L,		/* PCI memory space */
	ASI_REAL_IO_L,		/* PCI I/O space */
	0
};

int bus_stream_asi[] = {
	ASI_REAL_IO,		/* nexus */
	ASI_REAL_IO,		/* SBus */
	ASI_REAL_IO,		/* PCI configuration space */
	ASI_REAL_IO,		/* PCI memory space */
	ASI_REAL_IO,		/* PCI I/O space */
	0
};

/*
 * Convenience function for manipulating driver locks from busdma (during
 * busdma_swi, for example).  Drivers that don't provide their own locks
 * should specify &Giant to dmat->lockfuncarg.  Drivers that use their own
 * non-mutex locking scheme don't have to use this at all.
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
 * with the tag are meant to never be defered.
 * XXX Should have a way to identify which driver is responsible here.
 */
static void
dflt_lock(void *arg, bus_dma_lock_op_t op)
{
#ifdef INVARIANTS
	panic("driver error: busdma dflt_lock called");
#else
	printf("DRIVER_ERROR: busdma dflt_lock called\n");
#endif
}

/*
 * Since there is no way for a device to obtain a dma tag from its parent
 * we use this kluge to handle different the different supported bus systems.
 * The sparc64_root_dma_tag is used as parent for tags that have none, so that
 * the correct methods will be used.
 */
bus_dma_tag_t sparc64_root_dma_tag;

/*
 * Allocate a device specific dma_tag.
 */
int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
    bus_size_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_dma_filter_t *filter, void *filterarg, bus_size_t maxsize,
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
    void *lockfuncarg, bus_dma_tag_t *dmat)
{
	bus_dma_tag_t impptag;
	bus_dma_tag_t newtag;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF, M_NOWAIT);
	if (newtag == NULL)
		return (ENOMEM);

	impptag = parent != NULL ? parent : sparc64_root_dma_tag;
	/*
	 * The method table pointer and the cookie need to be taken over from
	 * the parent or the root tag.
	 */
	newtag->dt_cookie = impptag->dt_cookie;
	newtag->dt_mt = impptag->dt_mt;

	newtag->dt_parent = parent;
	newtag->dt_alignment = alignment;
	newtag->dt_boundary = boundary;
	newtag->dt_lowaddr = trunc_page((vm_offset_t)lowaddr) + (PAGE_SIZE - 1);
	newtag->dt_highaddr = trunc_page((vm_offset_t)highaddr) +
	    (PAGE_SIZE - 1);
	newtag->dt_filter = filter;
	newtag->dt_filterarg = filterarg;
	newtag->dt_maxsize = maxsize;
	newtag->dt_nsegments = nsegments;
	newtag->dt_maxsegsz = maxsegsz;
	newtag->dt_flags = flags;
	newtag->dt_ref_count = 1; /* Count ourselves */
	newtag->dt_map_count = 0;

	if (lockfunc != NULL) {
		newtag->dt_lockfunc = lockfunc;
		newtag->dt_lockfuncarg = lockfuncarg;
	} else {
		newtag->dt_lockfunc = dflt_lock;
		newtag->dt_lockfuncarg = NULL;
	}

	newtag->dt_segments = NULL;

	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		newtag->dt_lowaddr = ulmin(parent->dt_lowaddr,
		    newtag->dt_lowaddr);
		newtag->dt_highaddr = ulmax(parent->dt_highaddr,
		    newtag->dt_highaddr);
		if (newtag->dt_boundary == 0)
			newtag->dt_boundary = parent->dt_boundary;
		else if (parent->dt_boundary != 0)
			newtag->dt_boundary = ulmin(parent->dt_boundary,
			    newtag->dt_boundary);
		atomic_add_int(&parent->dt_ref_count, 1);
	}

	if (newtag->dt_boundary > 0)
		newtag->dt_maxsegsz = ulmin(newtag->dt_maxsegsz,
		    newtag->dt_boundary);

	*dmat = newtag;
	return (0);
}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
	bus_dma_tag_t parent;

	if (dmat != NULL) {
		if (dmat->dt_map_count != 0)
			return (EBUSY);
		while (dmat != NULL) {
			parent = dmat->dt_parent;
			atomic_subtract_int(&dmat->dt_ref_count, 1);
			if (dmat->dt_ref_count == 0) {
				if (dmat->dt_segments != NULL)
					free(dmat->dt_segments, M_DEVBUF);
				free(dmat, M_DEVBUF);
				/*
				 * Last reference count, so
				 * release our reference
				 * count on our parent.
				 */
				dmat = parent;
			} else
				dmat = NULL;
		}
	}
	return (0);
}

/* Allocate/free a tag, and do the necessary management work. */
int
sparc64_dma_alloc_map(bus_dma_tag_t dmat, bus_dmamap_t *mapp)
{

	if (dmat->dt_segments == NULL) {
		dmat->dt_segments = (bus_dma_segment_t *)malloc(
		    sizeof(bus_dma_segment_t) * dmat->dt_nsegments, M_DEVBUF,
		    M_NOWAIT);
		if (dmat->dt_segments == NULL)
			return (ENOMEM);
	}
	*mapp = malloc(sizeof(**mapp), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*mapp == NULL)
		return (ENOMEM);

	SLIST_INIT(&(*mapp)->dm_reslist);
	dmat->dt_map_count++;
	return (0);
}

void
sparc64_dma_free_map(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	free(map, M_DEVBUF);
	dmat->dt_map_count--;
}

static int
nexus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{

	return (sparc64_dma_alloc_map(dmat, mapp));
}

static int
nexus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	sparc64_dma_free_map(dmat, map);
	return (0);
}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrace, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
static int
_nexus_dmamap_load_buffer(bus_dma_tag_t dmat, void *buf, bus_size_t buflen,
    struct thread *td, int flags, bus_addr_t *lastaddrp,
    bus_dma_segment_t *segs, int *segp, int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vm_offset_t vaddr = (vm_offset_t)buf;
	int seg;
	pmap_t pmap;

	if (td != NULL)
		pmap = vmspace_pmap(td->td_proc->p_vmspace);
	else
		pmap = NULL;

	lastaddr = *lastaddrp;
	bmask  = ~(dmat->dt_boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap)
			curaddr = pmap_extract(pmap, vaddr);
		else
			curaddr = pmap_kextract(vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (sgsize > dmat->dt_maxsegsz)
			sgsize = dmat->dt_maxsegsz;
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (dmat->dt_boundary > 0) {
			baddr = (curaddr + dmat->dt_boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * previous segment if possible.
		 */
		if (first) {
			segs[seg].ds_addr = curaddr;
			segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (segs[seg].ds_len + sgsize) <= dmat->dt_maxsegsz &&
			    (dmat->dt_boundary == 0 ||
			    (segs[seg].ds_addr & bmask) == (curaddr & bmask)))
				segs[seg].ds_len += sgsize;
			else {
				if (++seg >= dmat->dt_nsegments)
					break;
				segs[seg].ds_addr = curaddr;
				segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 *
 * Most SPARCs have IOMMUs in the bus controllers.  In those cases
 * they only need one segment and will use virtual addresses for DVMA.
 * Those bus controllers should intercept these vectors and should
 * *NEVER* call nexus_dmamap_load() which is used only by devices that
 * bypass DVMA.
 */
static int
nexus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *callback, void *callback_arg,
    int flags)
{
	bus_addr_t lastaddr;
	int error, nsegs;

	error = _nexus_dmamap_load_buffer(dmat, buf, buflen, NULL, flags,
	    &lastaddr, dmat->dt_segments, &nsegs, 1);

	if (error == 0) {
		(*callback)(callback_arg, dmat->dt_segments, nsegs + 1, 0);
		map->dm_flags |= DMF_LOADED;
	} else
		(*callback)(callback_arg, NULL, 0, error);

	return (0);
}

/*
 * Like nexus_dmamap_load(), but for mbufs.
 */
static int
nexus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	int nsegs, error;

	M_ASSERTPKTHDR(m0);

	nsegs = 0;
	error = 0;
	if (m0->m_pkthdr.len <= dmat->dt_maxsize) {
		int first = 1;
		bus_addr_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = _nexus_dmamap_load_buffer(dmat,
				    m->m_data, m->m_len,NULL, flags, &lastaddr,
				    dmat->dt_segments, &nsegs, first);
				first = 0;
			}
		}
	} else {
		error = EINVAL;
	}

	if (error) {
		/* force "no valid mappings" in callback */
		(*callback)(callback_arg, dmat->dt_segments, 0, 0, error);
	} else {
		map->dm_flags |= DMF_LOADED;
		(*callback)(callback_arg, dmat->dt_segments, nsegs + 1,
		    m0->m_pkthdr.len, error);
	}
	return (error);
}

static int
nexus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
    bus_dma_segment_t *segs, int *nsegs, int flags)
{
	int error;

	M_ASSERTPKTHDR(m0);

	*nsegs = 0;
	error = 0;
	if (m0->m_pkthdr.len <= dmat->dt_maxsize) {
		int first = 1;
		bus_addr_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = _nexus_dmamap_load_buffer(dmat,
				    m->m_data, m->m_len,NULL, flags, &lastaddr,
				    segs, nsegs, first);
				first = 0;
			}
		}
	} else {
		error = EINVAL;
	}

	++*nsegs;
	return (error);
}

/*
 * Like nexus_dmamap_load(), but for uios.
 */
static int
nexus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *callback, void *callback_arg, int flags)
{
	bus_addr_t lastaddr;
	int nsegs, error, first, i;
	bus_size_t resid;
	struct iovec *iov;
	struct thread *td = NULL;

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		td = uio->uio_td;
		KASSERT(td != NULL,
			("nexus_dmamap_load_uio: USERSPACE but no proc"));
	}

	nsegs = 0;
	error = 0;
	first = 1;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && !error; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		bus_size_t minlen =
			resid < iov[i].iov_len ? resid : iov[i].iov_len;
		caddr_t addr = (caddr_t) iov[i].iov_base;

		if (minlen > 0) {
			error = _nexus_dmamap_load_buffer(dmat, addr, minlen,
			    td, flags, &lastaddr, dmat->dt_segments, &nsegs,
			    first);
			first = 0;

			resid -= minlen;
		}
	}

	if (error) {
		/* force "no valid mappings" in callback */
		(*callback)(callback_arg, dmat->dt_segments, 0, 0, error);
	} else {
		map->dm_flags |= DMF_LOADED;
		(*callback)(callback_arg, dmat->dt_segments, nsegs + 1,
		    uio->uio_resid, error);
	}
	return (error);
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
static void
nexus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	map->dm_flags &= ~DMF_LOADED;
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
static void
nexus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{

	/*
	 * We sync out our caches, but the bus must do the same.
	 *
	 * Actually a #Sync is expensive.  We should optimize.
	 */
	if ((op & BUS_DMASYNC_PREREAD) || (op & BUS_DMASYNC_PREWRITE)) {
		/*
		 * Don't really need to do anything, but flush any pending
		 * writes anyway.
		 */
		membar(Sync);
	}
#if 0
	/* Should not be needed. */
	if (op & BUS_DMASYNC_POSTREAD) {
		ecache_flush((vm_offset_t)map->buf,
		    (vm_offset_t)map->buf + map->buflen - 1);
	}
#endif
	if (op & BUS_DMASYNC_POSTWRITE) {
		/* Nothing to do.  Handled by the bus controller. */
	}
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
static int
nexus_dmamem_alloc(bus_dma_tag_t dmat, void **vaddr, int flags,
    bus_dmamap_t *mapp)
{
	int mflags;

	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;
	if (flags & BUS_DMA_ZERO)
		mflags |= M_ZERO;

	if ((dmat->dt_maxsize <= PAGE_SIZE)) {
		*vaddr = malloc(dmat->dt_maxsize, M_DEVBUF, mflags);
	} else {
		/*
		 * XXX: Use contigmalloc until it is merged into this facility
		 * and handles multi-seg allocations.  Nobody is doing multi-seg
		 * allocations yet though.
		 */
		*vaddr = contigmalloc(dmat->dt_maxsize, M_DEVBUF, mflags,
		    0ul, dmat->dt_lowaddr,
		    dmat->dt_alignment ? dmat->dt_alignment : 1UL,
		    dmat->dt_boundary);
	}
	if (*vaddr == NULL)
		return (ENOMEM);
	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
static void
nexus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{

	if ((dmat->dt_maxsize <= PAGE_SIZE))
		free(vaddr, M_DEVBUF);
	else {
		contigfree(vaddr, dmat->dt_maxsize, M_DEVBUF);
	}
}

struct bus_dma_methods nexus_dma_methods = {
	nexus_dmamap_create,
	nexus_dmamap_destroy,
	nexus_dmamap_load,
	nexus_dmamap_load_mbuf,
	nexus_dmamap_load_mbuf_sg,
	nexus_dmamap_load_uio,
	nexus_dmamap_unload,
	nexus_dmamap_sync,
	nexus_dmamem_alloc,
	nexus_dmamem_free,
};

struct bus_dma_tag nexus_dmatag = {
	NULL,
	NULL,
	8,
	0,
	0,
	0x3ffffffff,
	NULL,		/* XXX */
	NULL,
	0x3ffffffff,	/* XXX */
	0xff,		/* XXX */
	0xffffffff,	/* XXX */
	0,
	0,
	0,
	NULL,
	NULL,
	NULL,
	&nexus_dma_methods,
};

/*
 * Helpers to map/unmap bus memory
 */
int
sparc64_bus_mem_map(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t size, int flags, vm_offset_t vaddr, void **hp)
{
#if 0
	vm_offset_t addr;
	vm_offset_t sva;
	vm_offset_t va;
	vm_paddr_t pa;
	vm_size_t vsz;
	u_long pm_flags;

	addr = (vm_offset_t)handle;
	size = round_page(size);
	if (size == 0) {
		printf("%s: zero size\n", __func__);
		return (EINVAL);
	}
	switch (tag->bst_type) {
	case PCI_CONFIG_BUS_SPACE:
	case PCI_IO_BUS_SPACE:
	case PCI_MEMORY_BUS_SPACE:
		pm_flags = TD_IE;
		break;
	default:
		pm_flags = 0;
		break;
	}

	if (!(flags & BUS_SPACE_MAP_CACHEABLE))
		pm_flags |= TD_E;

	if (vaddr != 0L)
		sva = trunc_page(vaddr);
	else {
		if ((sva = kmem_alloc_nofault(kernel_map, size)) == 0)
			panic("%s: cannot allocate virtual memory", __func__);
	}

	/* Preserve page offset. */
	*hp = (void *)(sva | ((u_long)addr & PAGE_MASK));

	pa = trunc_page(addr);
	if ((flags & BUS_SPACE_MAP_READONLY) == 0)
		pm_flags |= TD_W;

	va = sva;
	vsz = size;
	do {
		pmap_kenter_flags(va, pa, pm_flags);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((vsz -= PAGE_SIZE) > 0);
	tlb_range_demap(kernel_pmap, sva, sva + size - 1);
#endif
	return (0);
}

int
sparc64_bus_mem_unmap(void *bh, bus_size_t size)
{
#if 0
	vm_offset_t sva;
	vm_offset_t va;
	vm_offset_t endva;

	sva = trunc_page((vm_offset_t)bh);
	endva = sva + round_page(size);
	for (va = sva; va < endva; va += PAGE_SIZE)
		pmap_kremove_flags(va);
	tlb_range_demap(kernel_pmap, sva, sva + size - 1);
	kmem_free(kernel_map, sva, size);
#endif
	return (0);
}

/*
 * Fake up a bus tag, for use by console drivers in early boot when the regular
 * means to allocate resources are not yet available.
 * Addr is the physical address of the desired start of the handle.
 */
bus_space_handle_t
sparc64_fake_bustag(int space, bus_addr_t addr, struct bus_space_tag *ptag)
{

	ptag->bst_cookie = NULL;
	ptag->bst_parent = NULL;
	ptag->bst_type = space;
	ptag->bst_bus_barrier = nexus_bus_barrier;
	return (addr);
}

/*
 * Base bus space handlers.
 */

static void
nexus_bus_barrier(bus_space_tag_t t, bus_space_handle_t h, bus_size_t offset,
    bus_size_t size, int flags)
{

	/*
	 * We have lots of alternatives depending on whether we're
	 * synchronizing loads with loads, loads with stores, stores
	 * with loads, or stores with stores.  The only ones that seem
	 * generic are #Sync and #MemIssue.  I'll use #Sync for safety.
	 */
	switch(flags) {
	case BUS_SPACE_BARRIER_READ | BUS_SPACE_BARRIER_WRITE:
	case BUS_SPACE_BARRIER_READ:
	case BUS_SPACE_BARRIER_WRITE:
		membar(Sync);
		break;
	default:
		panic("%s: unknown flags", __func__);
	}
	return;
}

struct bus_space_tag nexus_bustag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	NEXUS_BUS_SPACE,		/* type */
	nexus_bus_barrier,		/* bus_space_barrier */
};
