/*-
 * Copyright (c) 2006 Fill this file and put your name here
 * All rights reserved.
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
 */

#define NO_DMA

/*-
 * Copyright (c) 1997, 1998, 2001 The NetBSD Foundation, Inc.
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

/*	$NetBSD: bus_dma.c,v 1.17 2006/03/01 12:38:11 yamt Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/mbuf.h>
#include <sys/uio.h>
#include <sys/ktr.h>
#include <sys/kernel.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/cpufunc.h>

struct bus_dma_tag {
	bus_dma_tag_t		parent;
	bus_size_t		alignment;
	bus_size_t		boundary;
	bus_addr_t		lowaddr;
	bus_addr_t		highaddr;
	bus_dma_filter_t	*filter;
	void			*filterarg;
	bus_size_t		maxsize;
	u_int			nsegments;
	bus_size_t		maxsegsz;
	int			flags;
	int			ref_count;
	int			map_count;
	bus_dma_lock_t		*lockfunc;
	void			*lockfuncarg;
	/* XXX: machine-dependent fields */
	vm_offset_t		_physbase;
	vm_offset_t		_wbase;
	vm_offset_t		_wsize;
};

#define DMAMAP_LINEAR		0x1
#define DMAMAP_MBUF		0x2
#define DMAMAP_UIO		0x4
#define DMAMAP_ALLOCATED	0x10
#define DMAMAP_TYPE_MASK	(DMAMAP_LINEAR|DMAMAP_MBUF|DMAMAP_UIO)
#define DMAMAP_COHERENT		0x8
struct bus_dmamap {
        bus_dma_tag_t	dmat;
	int		flags;
	void 		*buffer;
	void		*origbuffer;
	void		*allocbuffer;
	TAILQ_ENTRY(bus_dmamap)	freelist;
	int		len;
};

static TAILQ_HEAD(,bus_dmamap) dmamap_freelist = 
	TAILQ_HEAD_INITIALIZER(dmamap_freelist);

#define BUSDMA_STATIC_MAPS	500
static struct bus_dmamap map_pool[BUSDMA_STATIC_MAPS];

static struct mtx busdma_mtx;

MTX_SYSINIT(busdma_mtx, &busdma_mtx, "busdma lock", MTX_DEF);

static void
mips_dmamap_freelist_init(void *dummy)
{
	int i;

	for (i = 0; i < BUSDMA_STATIC_MAPS; i++) 
		TAILQ_INSERT_HEAD(&dmamap_freelist, &map_pool[i], freelist);
}

SYSINIT(busdma, SI_SUB_VM, SI_ORDER_ANY, mips_dmamap_freelist_init, NULL);

/*
 * Check to see if the specified page is in an allowed DMA range.
 */

static __inline int
bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dma_segment_t *segs,
    bus_dmamap_t map, void *buf, bus_size_t buflen, struct pmap *pmap,
    int flags, vm_offset_t *lastaddrp, int *segp);

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
#ifndef NO_DMA
static void
dflt_lock(void *arg, bus_dma_lock_op_t op)
{
#ifdef INVARIANTS
	panic("driver error: busdma dflt_lock called");
#else
	printf("DRIVER_ERROR: busdma dflt_lock called\n");
#endif
}
#endif

static __inline bus_dmamap_t
_busdma_alloc_dmamap(void)
{
	bus_dmamap_t map;

	mtx_lock(&busdma_mtx);
	map = TAILQ_FIRST(&dmamap_freelist);
	if (map)
		TAILQ_REMOVE(&dmamap_freelist, map, freelist);
	mtx_unlock(&busdma_mtx);
	if (!map) {
		map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT | M_ZERO);
		if (map)
			map->flags = DMAMAP_ALLOCATED;
	} else
		map->flags = 0;
	return (map);
}

static __inline void 
_busdma_free_dmamap(bus_dmamap_t map)
{
	if (map->flags & DMAMAP_ALLOCATED)
		free(map, M_DEVBUF);
	else {
		mtx_lock(&busdma_mtx);
		TAILQ_INSERT_HEAD(&dmamap_freelist, map, freelist);
		mtx_unlock(&busdma_mtx);
	}
}

int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
		   bus_size_t boundary, bus_addr_t lowaddr,
		   bus_addr_t highaddr, bus_dma_filter_t *filter,
		   void *filterarg, bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
		   void *lockfuncarg, bus_dma_tag_t *dmat)
{
#ifndef NO_DMA
	bus_dma_tag_t newtag;
	int error = 0;

	/* Basic sanity checking */
	if (boundary != 0 && boundary < maxsegsz)
		maxsegsz = boundary;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF,
	    M_ZERO | M_NOWAIT);
	if (newtag == NULL) {
		CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
		    __func__, newtag, 0, error);
		return (ENOMEM);
	}

	newtag->parent = parent;
	newtag->alignment = alignment;
	newtag->boundary = boundary;
	newtag->lowaddr = trunc_page((vm_paddr_t)lowaddr) + (PAGE_SIZE - 1);
	newtag->highaddr = trunc_page((vm_paddr_t)highaddr) +
	    (PAGE_SIZE - 1);
	newtag->filter = filter;
	newtag->filterarg = filterarg;
	newtag->maxsize = maxsize;
	newtag->nsegments = nsegments;
	newtag->maxsegsz = maxsegsz;
	newtag->flags = flags;
	newtag->ref_count = 1; /* Count ourself */
	newtag->map_count = 0;
	newtag->_wbase = 0;
	newtag->_physbase = 0;
	/* XXXMIPS: Should we limit window size to amount of physical memory */
	newtag->_wsize = MIPS_KSEG1_START - MIPS_KSEG0_START;
	if (lockfunc != NULL) {
		newtag->lockfunc = lockfunc;
		newtag->lockfuncarg = lockfuncarg;
	} else {
		newtag->lockfunc = dflt_lock;
		newtag->lockfuncarg = NULL;
	}

	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		newtag->lowaddr = MIN(parent->lowaddr, newtag->lowaddr);
		newtag->highaddr = MAX(parent->highaddr, newtag->highaddr);
		if (newtag->boundary == 0)
			newtag->boundary = parent->boundary;
		else if (parent->boundary != 0)
			newtag->boundary = MIN(parent->boundary,
					       newtag->boundary);
		if (newtag->filter == NULL) {
			/*
			 * Short circuit looking at our parent directly
			 * since we have encapsulated all of its information
			 */
			newtag->filter = parent->filter;
			newtag->filterarg = parent->filterarg;
			newtag->parent = parent->parent;
		}
		if (newtag->parent != NULL)
			atomic_add_int(&parent->ref_count, 1);
	}

	if (error != 0) {
		free(newtag, M_DEVBUF);
	} else {
		*dmat = newtag;
	}
	CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
	    __func__, newtag, (newtag != NULL ? newtag->flags : 0), error);
	return (error);
#else 
	return ENOSYS;
#endif

}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
#ifdef KTR
	bus_dma_tag_t dmat_copy = dmat;
#endif

	if (dmat != NULL) {
		
                if (dmat->map_count != 0)
                        return (EBUSY);
		
                while (dmat != NULL) {
                        bus_dma_tag_t parent;
			
                        parent = dmat->parent;
                        atomic_subtract_int(&dmat->ref_count, 1);
                        if (dmat->ref_count == 0) {
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
	CTR2(KTR_BUSDMA, "%s tag %p", __func__, dmat_copy);

        return (0);
}

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	bus_dmamap_t newmap;
#ifdef KTR
	int error = 0;
#endif

	newmap = _busdma_alloc_dmamap();
	if (newmap == NULL) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d", __func__, dmat, ENOMEM);
		return (ENOMEM);
	}
	*mapp = newmap;
	newmap->dmat = dmat;
	dmat->map_count++;

	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
	    __func__, dmat, dmat->flags, error);

	return (0);

}

/*
 * Destroy a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	_busdma_free_dmamap(map);
        dmat->map_count--;
	CTR2(KTR_BUSDMA, "%s: tag %p error 0", __func__, dmat);
        return (0);
}

/*
 * Allocate a piece of memory that can be efficiently mapped into
 * bus device space based on the constraints lited in the dma tag.
 * A dmamap to for use with dmamap_load is also allocated.
 */
int
bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
                 bus_dmamap_t *mapp)
{
	bus_dmamap_t newmap = NULL;

	int mflags;

	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;
	if (flags & BUS_DMA_ZERO)
		mflags |= M_ZERO;

	newmap = _busdma_alloc_dmamap();
	if (newmap == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->flags, ENOMEM);
		return (ENOMEM);
	}
	dmat->map_count++;
	*mapp = newmap;
	newmap->dmat = dmat;
	
        if (dmat->maxsize <= PAGE_SIZE) {
                *vaddr = malloc(dmat->maxsize, M_DEVBUF, mflags);
        } else {
                /*
                 * XXX Use Contigmalloc until it is merged into this facility
                 *     and handles multi-seg allocations.  Nobody is doing
                 *     multi-seg allocations yet though.
                 */
	         vm_paddr_t maxphys;
	         if((uint32_t)dmat->lowaddr >= MIPS_KSEG0_LARGEST_PHYS) {
		   /* Note in the else case I just put in what was already
		    * being passed in dmat->lowaddr. I am not sure
		    * how this would have worked. Since lowaddr is in the
		    * max address postion. I would have thought that the
		    * caller would have wanted dmat->highaddr. That is
		    * presuming they are asking for physical addresses
		    * which is what contigmalloc takes. - RRS
		    */
		   maxphys = MIPS_KSEG0_LARGEST_PHYS - 1;
		 } else {
		   maxphys = dmat->lowaddr;
		 }
                *vaddr = contigmalloc(dmat->maxsize, M_DEVBUF, mflags,
                    0ul, maxphys, dmat->alignment? dmat->alignment : 1ul,
                    dmat->boundary);
        }
        if (*vaddr == NULL) {
		if (newmap != NULL) {
			_busdma_free_dmamap(newmap);
			dmat->map_count--;
		}
		*mapp = NULL;
                return (ENOMEM);
	}
	if (flags & BUS_DMA_COHERENT) {
		void *tmpaddr = (void *)*vaddr;

		if (tmpaddr) {
			tmpaddr = (void *)MIPS_PHYS_TO_KSEG1(vtophys(tmpaddr));
			newmap->origbuffer = *vaddr;
			newmap->allocbuffer = tmpaddr;
			mips_dcache_wbinv_range((vm_offset_t)*vaddr,
			    dmat->maxsize);
			*vaddr = tmpaddr;
		} else
			newmap->origbuffer = newmap->allocbuffer = NULL;
	} else 
		newmap->origbuffer = newmap->allocbuffer = NULL;
        return (0);

}

/*
 * Free a piece of memory and it's allocated dmamap, that was allocated
 * via bus_dmamem_alloc.  Make the same choice for free/contigfree.
 */
void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
	if (map->allocbuffer) {
		KASSERT(map->allocbuffer == vaddr,
		    ("Trying to freeing the wrong DMA buffer"));
		vaddr = map->origbuffer;
	}
        if (dmat->maxsize <= PAGE_SIZE)
		free(vaddr, M_DEVBUF);
        else {
		contigfree(vaddr, dmat->maxsize, M_DEVBUF);
	}
	dmat->map_count--;
	_busdma_free_dmamap(map);
	CTR3(KTR_BUSDMA, "%s: tag %p flags 0x%x", __func__, dmat, dmat->flags);

}

/*
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
static __inline int
bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dma_segment_t *segs,
    bus_dmamap_t map, void *buf, bus_size_t buflen, struct pmap *pmap,
    int flags, vm_offset_t *lastaddrp, int *segp)
{
	bus_size_t sgsize;
	bus_size_t bmask;
	vm_offset_t curaddr, lastaddr;
	vm_offset_t vaddr = (vm_offset_t)buf;
	int seg;
	int error = 0;

	lastaddr = *lastaddrp;
	bmask = ~(dmat->boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		KASSERT(kernel_pmap == pmap, ("pmap is not kernel pmap"));
		curaddr = pmap_kextract(vaddr);

		/*
		 * If we're beyond the current DMA window, indicate
		 * that and try to fall back onto something else.
		 */
		if (curaddr < dmat->_physbase ||
		    curaddr >= (dmat->_physbase + dmat->_wsize))
			return (EINVAL);

		/*
		 * In a valid DMA range.  Translate the physical
		 * memory address to an address in the DMA window.
		 */
		curaddr = (curaddr - dmat->_physbase) + dmat->_wbase;


		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Insert chunk into a segment, coalescing with
		 * the previous segment if possible.
		 */
		if (seg >= 0 && curaddr == lastaddr &&
		    (segs[seg].ds_len + sgsize) <= dmat->maxsegsz &&
		    (dmat->boundary == 0 ||
		     (segs[seg].ds_addr & bmask) == 
		     (curaddr & bmask))) {
			segs[seg].ds_len += sgsize;
			goto segdone;
		} else {
			if (++seg >= dmat->nsegments)
				break;
			segs[seg].ds_addr = curaddr;
			segs[seg].ds_len = sgsize;
		}
		if (error)
			break;
segdone:
		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	if (buflen != 0)
		error = EFBIG;

	return error;
}

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
                bus_size_t buflen, bus_dmamap_callback_t *callback,
                void *callback_arg, int flags)
{
     	vm_offset_t	lastaddr = 0;
	int		error, nsegs = -1;
#ifdef __CC_SUPPORTS_DYNAMIC_ARRAY_INIT
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif

	KASSERT(dmat != NULL, ("dmatag is NULL"));
	KASSERT(map != NULL, ("dmamap is NULL"));
	map->flags &= ~DMAMAP_TYPE_MASK;
	map->flags |= DMAMAP_LINEAR|DMAMAP_COHERENT;
	map->buffer = buf;
	map->len = buflen;
	error = bus_dmamap_load_buffer(dmat,
	    dm_segments, map, buf, buflen, kernel_pmap,
	    flags, &lastaddr, &nsegs);

	if (error)
		(*callback)(callback_arg, NULL, 0, error);
	else
		(*callback)(callback_arg, dm_segments, nsegs + 1, error);
	
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, nsegs + 1, error);

	return (0);

}

/*
 * Like bus_dmamap_load(), but for mbufs.
 */
int
bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
		     bus_dmamap_callback2_t *callback, void *callback_arg,
		     int flags)
{
#ifdef __CC_SUPPORTS_DYNAMIC_ARRAY_INIT
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	int nsegs = -1, error = 0;

	M_ASSERTPKTHDR(m0);

	map->flags &= ~DMAMAP_TYPE_MASK;
	map->flags |= DMAMAP_MBUF | DMAMAP_COHERENT;
	map->buffer = m0;
	map->len = 0;

	if (m0->m_pkthdr.len <= dmat->maxsize) {
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = bus_dmamap_load_buffer(dmat,
				    dm_segments, map, m->m_data, m->m_len, 
				    pmap_kernel(), flags, &lastaddr, &nsegs);
				map->len += m->m_len;
			}
		}
	} else {
		error = EINVAL;
	}

	if (error) {
		/* 
		 * force "no valid mappings" on error in callback.
		 */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		(*callback)(callback_arg, dm_segments, nsegs + 1,
		    m0->m_pkthdr.len, error);
	}
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, error, nsegs + 1);

	return (error);
}

int
bus_dmamap_load_mbuf_sg(bus_dma_tag_t dmat, bus_dmamap_t map,
			struct mbuf *m0, bus_dma_segment_t *segs, int *nsegs,
			int flags)
{
	int error = 0;

	M_ASSERTPKTHDR(m0);

	flags |= BUS_DMA_NOWAIT;
	*nsegs = -1;
	map->flags &= ~DMAMAP_TYPE_MASK;
	map->flags |= DMAMAP_MBUF | DMAMAP_COHERENT;
	map->buffer = m0;
	map->len = 0;

	if (m0->m_pkthdr.len <= dmat->maxsize) {
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = bus_dmamap_load_buffer(dmat, segs, map,
				    m->m_data, m->m_len, 
				    pmap_kernel(), flags, &lastaddr, nsegs);
				map->len += m->m_len;
			}
		}
	} else {
		error = EINVAL;
	}

	++*nsegs;
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, error, *nsegs);

	return (error);

}

/*
 * Like bus_dmamap_load(), but for uios.
 */
int
bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *callback, void *callback_arg,
    int flags)
{

	panic("Unimplemented %s at %s:%d\n", __func__, __FILE__, __LINE__);
	return (0);
}

/*
 * Release the mapping held by map.
 */
void
_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	return;
}

static __inline void
bus_dmamap_sync_buf(void *buf, int len, bus_dmasync_op_t op)
{

	switch (op) {
	case BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE:
		mips_dcache_wbinv_range((vm_offset_t)buf, len);
		break;

	case BUS_DMASYNC_PREREAD:
#if 1
		mips_dcache_wbinv_range((vm_offset_t)buf, len);
#else
		mips_dcache_inv_range((vm_offset_t)buf, len);
#endif
		break;

	case BUS_DMASYNC_PREWRITE:
		mips_dcache_wb_range((vm_offset_t)buf, len);
		break;
	}
}

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct mbuf *m;
	struct uio *uio;
	int resid;
	struct iovec *iov;
	

	/*
	 * Mixing PRE and POST operations is not allowed.
	 */
	if ((op & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE)) != 0 &&
	    (op & (BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE)) != 0)
		panic("_bus_dmamap_sync: mix PRE and POST");

	/*
	 * Since we're dealing with a virtually-indexed, write-back
	 * cache, we need to do the following things:
	 *
	 *	PREREAD -- Invalidate D-cache.  Note we might have
	 *	to also write-back here if we have to use an Index
	 *	op, or if the buffer start/end is not cache-line aligned.
	 *
	 *	PREWRITE -- Write-back the D-cache.  If we have to use
	 *	an Index op, we also have to invalidate.  Note that if
	 *	we are doing PREREAD|PREWRITE, we can collapse everything
	 *	into a single op.
	 *
	 *	POSTREAD -- Nothing.
	 *
	 *	POSTWRITE -- Nothing.
	 */

	/*
	 * Flush the write buffer.
	 * XXX Is this always necessary?
	 */
	mips_wbflush();

	op &= (BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	if (op == 0)
		return;

	CTR3(KTR_BUSDMA, "%s: op %x flags %x", __func__, op, map->flags);
	switch(map->flags & DMAMAP_TYPE_MASK) {
	case DMAMAP_LINEAR:
		bus_dmamap_sync_buf(map->buffer, map->len, op);
		break;
	case DMAMAP_MBUF:
		m = map->buffer;
		while (m) {
			if (m->m_len > 0)
				bus_dmamap_sync_buf(m->m_data, m->m_len, op);
			m = m->m_next;
		}
		break;
	case DMAMAP_UIO:
		uio = map->buffer;
		iov = uio->uio_iov;
		resid = uio->uio_resid;
		for (int i = 0; i < uio->uio_iovcnt && resid != 0; i++) {
			bus_size_t minlen = resid < iov[i].iov_len ? resid :
			    iov[i].iov_len;
			if (minlen > 0) {
				bus_dmamap_sync_buf(iov[i].iov_base, minlen, op);
				resid -= minlen;
			}
		}
		break;
	default:
		break;
	}
}
