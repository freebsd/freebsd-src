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
/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
/*
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
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
#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/cache.h>
#include <machine/smp.h>
#include <machine/tlb.h>

/* ASI's for bus access. */
int bus_type_asi[] = {
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* UPA */
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* SBUS */
	ASI_PHYS_BYPASS_EC_WITH_EBIT_L,		/* PCI configuration space */
	ASI_PHYS_BYPASS_EC_WITH_EBIT_L,		/* PCI memory space */
	ASI_PHYS_BYPASS_EC_WITH_EBIT_L,		/* PCI I/O space */
	0
};

int bus_stream_asi[] = {
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* UPA */
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* SBUS */
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* PCI configuration space */
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* PCI memory space */
	ASI_PHYS_BYPASS_EC_WITH_EBIT,		/* PCI I/O space */
	0
};

/*
 * busdma support code.
 * Note: there is no support for bounce buffers yet.
 */

static int nexus_dmamap_create(bus_dma_tag_t, bus_dma_tag_t, int,
    bus_dmamap_t *);
static int nexus_dmamap_destroy(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
static int nexus_dmamap_load(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    void *, bus_size_t, bus_dmamap_callback_t *, void *, int);
static int nexus_dmamap_load_mbuf(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    struct mbuf *, bus_dmamap_callback2_t *, void *, int);
static int nexus_dmamap_load_uio(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    struct uio *, bus_dmamap_callback2_t *, void *, int);
static void nexus_dmamap_unload(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
static void nexus_dmamap_sync(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
    bus_dmasync_op_t);
static int nexus_dmamem_alloc_size(bus_dma_tag_t, bus_dma_tag_t, void **, int,
    bus_dmamap_t *, u_long size);
static int nexus_dmamem_alloc(bus_dma_tag_t, bus_dma_tag_t, void **, int,
    bus_dmamap_t *);
static void nexus_dmamem_free_size(bus_dma_tag_t, bus_dma_tag_t, void *,
    bus_dmamap_t, u_long size);
static void nexus_dmamem_free(bus_dma_tag_t, bus_dma_tag_t, void *,
    bus_dmamap_t);

/*
 * Since there is now way for a device to obtain a dma tag from its parent
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
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_tag_t *dmat)
{

	bus_dma_tag_t newtag;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF, M_NOWAIT);
	if (newtag == NULL)
		return (ENOMEM);

	newtag->dt_parent = parent != NULL ? parent : sparc64_root_dma_tag;
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

	newtag->dt_dmamap_create = NULL;
	newtag->dt_dmamap_destroy = NULL;
	newtag->dt_dmamap_load = NULL;
	newtag->dt_dmamap_load_mbuf = NULL;
	newtag->dt_dmamap_load_uio = NULL;
	newtag->dt_dmamap_unload = NULL;
	newtag->dt_dmamap_sync = NULL;
	newtag->dt_dmamem_alloc_size = NULL;
	newtag->dt_dmamem_alloc = NULL;
	newtag->dt_dmamem_free_size = NULL;
	newtag->dt_dmamem_free = NULL;

	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		newtag->dt_lowaddr = ulmin(parent->dt_lowaddr,
		    newtag->dt_lowaddr);
		newtag->dt_highaddr = ulmax(parent->dt_highaddr,
		    newtag->dt_highaddr);
		/*
		 * XXX Not really correct??? Probably need to honor boundary
		 *     all the way up the inheritence chain.
		 */
		newtag->dt_boundary = ulmin(parent->dt_boundary,
		    newtag->dt_boundary);
	}
	newtag->dt_parent->dt_ref_count++;

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
			dmat->dt_ref_count--;
			if (dmat->dt_ref_count == 0) {
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

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
static int
nexus_dmamap_create(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, int flags,
    bus_dmamap_t *mapp)
{

	*mapp = malloc(sizeof(**mapp), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*mapp != NULL) {
		ddmat->dt_map_count++;
		sparc64_dmamap_init(*mapp);
		return (0);
	} else
		return (ENOMEM);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
static int
nexus_dmamap_destroy(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map)
{

	free(map, M_DEVBUF);
	ddmat->dt_map_count--;
	return (0);
}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrace, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
static int
_nexus_dmamap_load_buffer(bus_dma_tag_t ddmat, bus_dma_segment_t segs[],
    void *buf, bus_size_t buflen, struct thread *td, int flags,
    vm_offset_t *lastaddrp, int *segp, int first)
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
	bmask  = ~(ddmat->dt_boundary - 1);

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
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (ddmat->dt_boundary > 0) {
			baddr = (curaddr + ddmat->dt_boundary) & bmask;
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
			    (segs[seg].ds_len + sgsize) <= ddmat->dt_maxsegsz &&
			    (ddmat->dt_boundary == 0 ||
			     (segs[seg].ds_addr & bmask) == (curaddr & bmask)))
				segs[seg].ds_len += sgsize;
			else {
				if (++seg >= ddmat->dt_nsegments)
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
nexus_dmamap_load(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map,
    void *buf, bus_size_t buflen, bus_dmamap_callback_t *callback,
    void *callback_arg, int flags)
{
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[ddmat->dt_nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	vm_offset_t lastaddr;
	int error, nsegs;

	error = _nexus_dmamap_load_buffer(ddmat, dm_segments, buf, buflen,
	    NULL, flags, &lastaddr, &nsegs, 1);

	if (error == 0) {
		(*callback)(callback_arg, dm_segments, nsegs + 1, 0);
		map->dm_loaded = 1;
	} else
		(*callback)(callback_arg, NULL, 0, error);

	return (0);
}

/*
 * Like nexus_dmamap_load(), but for mbufs.
 */
static int
nexus_dmamap_load_mbuf(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat,
    bus_dmamap_t map, struct mbuf *m0, bus_dmamap_callback2_t *callback,
    void *callback_arg, int flags)
{
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[ddmat->dt_nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	int nsegs, error;

	KASSERT(m0->m_flags & M_PKTHDR,
		("nexus_dmamap_load_mbuf: no packet header"));

	nsegs = 0;
	error = 0;
	if (m0->m_pkthdr.len <= ddmat->dt_maxsize) {
		int first = 1;
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = _nexus_dmamap_load_buffer(ddmat,
				    dm_segments, m->m_data, m->m_len, NULL,
				    flags, &lastaddr, &nsegs, first);
				first = 0;
			}
		}
	} else {
		error = EINVAL;
	}

	if (error) {
		/* force "no valid mappings" in callback */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		map->dm_loaded = 1;
		(*callback)(callback_arg, dm_segments, nsegs + 1,
		    m0->m_pkthdr.len, error);
	}
	return (error);
}

/*
 * Like nexus_dmamap_load(), but for uios.
 */
static int
nexus_dmamap_load_uio(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat,
    bus_dmamap_t map, struct uio *uio, bus_dmamap_callback2_t *callback,
    void *callback_arg, int flags)
{
	vm_offset_t lastaddr;
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[ddmat->dt_nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
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
			error = _nexus_dmamap_load_buffer(ddmat, dm_segments,
			    addr, minlen, td, flags, &lastaddr, &nsegs, first);
			first = 0;

			resid -= minlen;
		}
	}

	if (error) {
		/* force "no valid mappings" in callback */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		map->dm_loaded = 1;
		(*callback)(callback_arg, dm_segments, nsegs + 1,
		    uio->uio_resid, error);
	}
	return (error);
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
static void
nexus_dmamap_unload(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map)
{

	map->dm_loaded = 0;
}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
static void
nexus_dmamap_sync(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{

	/*
	 * We sync out our caches, but the bus must do the same.
	 *
	 * Actually a #Sync is expensive.  We should optimize.
	 */
	if ((op == BUS_DMASYNC_PREREAD) || (op == BUS_DMASYNC_PREWRITE)) {
		/* 
		 * Don't really need to do anything, but flush any pending
		 * writes anyway. 
		 */
		membar(Sync);
	}
#if 0
	/* Should not be needed. */
	if (op == BUS_DMASYNC_POSTREAD) {
		ecache_flush((vm_offset_t)map->buf,
		    (vm_offset_t)map->buf + map->buflen - 1);
	}
#endif
	if (op == BUS_DMASYNC_POSTWRITE) {
		/* Nothing to do.  Handled by the bus controller. */
	}
}

/*
 * Helper functions for buses that use their private dmamem_alloc/dmamem_free
 * versions.
 * These differ from the dmamap_alloc() functions in that they create a tag
 * that is specifically for use with dmamem_alloc'ed memory.
 * These are primitive now, but I expect that some fields of the map will need
 * to be filled soon.
 */
int
sparc64_dmamem_alloc_map(bus_dma_tag_t dmat, bus_dmamap_t *mapp)
{

	*mapp = malloc(sizeof(**mapp), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*mapp == NULL)
		return (ENOMEM);

	dmat->dt_map_count++;
	sparc64_dmamap_init(*mapp);
	return (0);
}

void
sparc64_dmamem_free_map(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	free(map, M_DEVBUF);
	dmat->dt_map_count--;
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
static int
nexus_dmamem_alloc_size(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void **vaddr,
    int flags, bus_dmamap_t *mapp, bus_size_t size)
{

	if (size > ddmat->dt_maxsize)
		return (ENOMEM);

	if ((size <= PAGE_SIZE)) {
		*vaddr = malloc(size, M_DEVBUF,
		    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	} else {
		/*
		 * XXX: Use contigmalloc until it is merged into this facility
		 * and handles multi-seg allocations.  Nobody is doing multi-seg
		 * allocations yet though.
		 */
		*vaddr = contigmalloc(size, M_DEVBUF,
		    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK,
		    0ul, ddmat->dt_lowaddr,
		    ddmat->dt_alignment ? ddmat->dt_alignment : 1UL,
		    ddmat->dt_boundary);
	}
	if (*vaddr == NULL) {
		free(*mapp, M_DEVBUF);
		return (ENOMEM);
	}
	return (0);
}

static int
nexus_dmamem_alloc(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void **vaddr,
    int flags, bus_dmamap_t *mapp)
{
	return (sparc64_dmamem_alloc_size(pdmat, ddmat, vaddr, flags, mapp,
		ddmat->dt_maxsize));
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
static void
nexus_dmamem_free_size(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void *vaddr,
    bus_dmamap_t map, bus_size_t size)
{

	sparc64_dmamem_free_map(ddmat, map);
	if ((size <= PAGE_SIZE))
		free(vaddr, M_DEVBUF);
	else
		contigfree(vaddr, size, M_DEVBUF);
}

static void
nexus_dmamem_free(bus_dma_tag_t pdmat, bus_dma_tag_t ddmat, void *vaddr,
    bus_dmamap_t map)
{
	sparc64_dmamem_free_size(pdmat, ddmat, vaddr, map, ddmat->dt_maxsize);
}

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
	nexus_dmamap_create,
	nexus_dmamap_destroy,
	nexus_dmamap_load,
	nexus_dmamap_load_mbuf,
	nexus_dmamap_load_uio,
	nexus_dmamap_unload,
	nexus_dmamap_sync,

	nexus_dmamem_alloc_size,
	nexus_dmamem_alloc,
	nexus_dmamem_free_size,
	nexus_dmamem_free,
};

/*
 * Helpers to map/unmap bus memory
 */
int
sparc64_bus_mem_map(bus_space_tag_t tag, bus_space_handle_t handle,
    bus_size_t size, int flags, vm_offset_t vaddr, void **hp)
{
	vm_offset_t addr;
	vm_offset_t sva;
	vm_offset_t va;
	vm_offset_t pa;
	vm_size_t vsz;
	u_long pm_flags;

	addr = (vm_offset_t)handle;
	size = round_page(size);
	if (size == 0) {
		printf("sparc64_bus_map: zero size\n");
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

	if (vaddr != NULL)
		sva = trunc_page(vaddr);
	else {
		if ((sva = kmem_alloc_nofault(kernel_map, size)) == NULL)
			panic("sparc64_bus_map: cannot allocate virtual "
			    "memory");
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
	return (0);
}

int
sparc64_bus_mem_unmap(void *bh, bus_size_t size)
{
	vm_offset_t sva;
	vm_offset_t va;
	vm_offset_t endva;

	sva = trunc_page((vm_offset_t)bh);
	endva = sva + round_page(size);
	for (va = sva; va < endva; va += PAGE_SIZE)
		pmap_kremove_flags(va);
	tlb_range_demap(kernel_pmap, sva, sva + size - 1);
	kmem_free(kernel_map, sva, size);
	return (0);
}

/*
 * Fake up a bus tag, for use by console drivers in early boot when the regular
 * means to allocate resources are not yet available.
 * Note that these tags are not eligible for bus_space_barrier operations.
 * Addr is the physical address of the desired start of the handle.
 */
bus_space_handle_t
sparc64_fake_bustag(int space, bus_addr_t addr, struct bus_space_tag *ptag)
{

	ptag->bst_cookie = NULL;
	ptag->bst_parent = NULL;
	ptag->bst_type = space;
	ptag->bst_bus_barrier = NULL;
	return (addr);
}

/*
 * Base bus space handlers.
 */
static void nexus_bus_barrier(bus_space_tag_t, bus_space_handle_t,
    bus_size_t, bus_size_t, int);

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
		panic("sparc64_bus_barrier: unknown flags");
	}
	return;
}

struct bus_space_tag nexus_bustag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	UPA_BUS_SPACE,			/* type */
	nexus_bus_barrier,		/* bus_space_barrier */
};
