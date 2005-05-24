/*-
 * Copyright (c) 2004 Olivier Houchard
 * Copyright (c) 2002 Peter Grehan
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
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
 *   From i386/busdma_machdep.c,v 1.26 2002/04/19 22:58:09 alfred
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MacPPC bus dma support routines
 */

#define _ARM32_BUS_DMA_PRIVATE
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
	/*
	 * DMA range for this tag.  If the page doesn't fall within
	 * one of these ranges, an error is returned.  The caller
	 * may then decide what to do with the transfer.  If the
	 * range pointer is NULL, it is ignored.
	 */
	struct arm32_dma_range	*ranges;
	int			_nranges;
};

#define DMAMAP_LINEAR		0x1
#define DMAMAP_MBUF		0x2
#define DMAMAP_UIO		0x4
#define DMAMAP_ALLOCATED	0x10
#define DMAMAP_STATIC_BUSY	0x20
#define DMAMAP_TYPE_MASK	(DMAMAP_LINEAR|DMAMAP_MBUF|DMAMAP_UIO)
#define DMAMAP_COHERENT		0x8
struct bus_dmamap {
        bus_dma_tag_t	dmat;
	int		flags;
	void 		*buffer;
	int		len;
};

#define BUSDMA_STATIC_MAPS	500
static struct bus_dmamap map_pool[BUSDMA_STATIC_MAPS];

static struct mtx busdma_mtx;

MTX_SYSINIT(busdma_mtx, &busdma_mtx, "busdma lock", MTX_DEF);

/*
 * Check to see if the specified page is in an allowed DMA range.
 */

static __inline int
bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dma_segment_t *segs,
    bus_dmamap_t map, void *buf, bus_size_t buflen, struct pmap *pmap,
    int flags, vm_offset_t *lastaddrp, int *segp);

static __inline struct arm32_dma_range *
_bus_dma_inrange(struct arm32_dma_range *ranges, int nranges,
    bus_addr_t curaddr)
{
	struct arm32_dma_range *dr;
	int i;

	for (i = 0, dr = ranges; i < nranges; i++, dr++) {
		if (curaddr >= dr->dr_sysbase &&
		    round_page(curaddr) <= (dr->dr_sysbase + dr->dr_len))
			return (dr);
	}

	return (NULL);
}
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

static bus_dmamap_t
_busdma_alloc_dmamap(void)
{
	int i;
	bus_dmamap_t map;

	mtx_lock(&busdma_mtx);
	for (i = 0; i < BUSDMA_STATIC_MAPS; i++)
		if (!(map_pool[i].flags & DMAMAP_STATIC_BUSY)) {
			bzero(&map_pool[i], sizeof(map_pool[i]));
			map_pool[i].flags |= DMAMAP_STATIC_BUSY;
			mtx_unlock(&busdma_mtx);
			return (&map_pool[i]);
		}
	mtx_unlock(&busdma_mtx);
	map = malloc(sizeof(*map), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (map)
		map->flags |= DMAMAP_ALLOCATED;
	return (map);
}

static void
_busdma_free_dmamap(bus_dmamap_t map)
{
	if (map->flags & DMAMAP_ALLOCATED)
		free(map, M_DEVBUF);
	else {
		mtx_lock(&busdma_mtx);
		map->flags &= ~DMAMAP_STATIC_BUSY;
		mtx_unlock(&busdma_mtx);
	}
}

/*
 * Allocate a device specific dma_tag.
 */
#define SEG_NB 1024

int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
		   bus_size_t boundary, bus_addr_t lowaddr,
		   bus_addr_t highaddr, bus_dma_filter_t *filter,
		   void *filterarg, bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
		   void *lockfuncarg, bus_dma_tag_t *dmat)
{
	bus_dma_tag_t newtag;
	int error = 0;
	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF, M_NOWAIT);
	if (newtag == NULL) {
		CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
		    __func__, newtag, 0, error);
		return (ENOMEM);
	}

	newtag->parent = parent;
	newtag->alignment = alignment;
	newtag->boundary = boundary;
	newtag->lowaddr = trunc_page((vm_offset_t)lowaddr) + (PAGE_SIZE - 1);
	newtag->highaddr = trunc_page((vm_offset_t)highaddr) + (PAGE_SIZE - 1);
	newtag->filter = filter;
	newtag->filterarg = filterarg;
        newtag->maxsize = maxsize;
        newtag->nsegments = nsegments;
	newtag->maxsegsz = maxsegsz;
	newtag->flags = flags;
	newtag->ref_count = 1; /* Count ourself */
	newtag->map_count = 0;
	newtag->ranges = bus_dma_get_range();
	newtag->_nranges = bus_dma_get_range_nb();
	if (lockfunc != NULL) {
		newtag->lockfunc = lockfunc;
		newtag->lockfuncarg = lockfuncarg;
	} else {
		newtag->lockfunc = dflt_lock;
		newtag->lockfuncarg = NULL;
	}
        /*
	 * Take into account any restrictions imposed by our parent tag
	 */
        if (parent != NULL) {
                newtag->lowaddr = min(parent->lowaddr, newtag->lowaddr);
                newtag->highaddr = max(parent->highaddr, newtag->highaddr);
		if (newtag->boundary == 0)
			newtag->boundary = parent->boundary;
		else if (parent->boundary != 0)
                	newtag->boundary = min(parent->boundary,
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

	*dmat = newtag;
	CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
	    __func__, newtag, (newtag != NULL ? newtag->flags : 0), error);

	return (error);
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
                *vaddr = contigmalloc(dmat->maxsize, M_DEVBUF, mflags,
                    0ul, dmat->lowaddr, dmat->alignment? dmat->alignment : 1ul,
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
        return (0);
}

/*
 * Free a piece of memory and it's allocated dmamap, that was allocated
 * via bus_dmamem_alloc.  Make the same choice for free/contigfree.
 */
void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
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
static int __inline
bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dma_segment_t *segs,
    bus_dmamap_t map, void *buf, bus_size_t buflen, struct pmap *pmap,
    int flags, vm_offset_t *lastaddrp, int *segp)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vm_offset_t vaddr = (vm_offset_t)buf;
	int seg;
	int error = 0;
	pd_entry_t *pde;
	pt_entry_t pte;
	pt_entry_t *ptep;

	lastaddr = *lastaddrp;
	bmask = ~(dmat->boundary - 1);

	CTR3(KTR_BUSDMA, "lowaddr= %d boundary= %d, "
	    "alignment= %d", dmat->lowaddr, dmat->boundary, dmat->alignment);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 *
		 * XXX Don't support checking for coherent mappings
		 * XXX in user address space.
		 */
		if (0 && __predict_true(pmap == pmap_kernel())) {
			(void) pmap_get_pde_pte(pmap, vaddr, &pde, &ptep);
			if (__predict_false(pmap_pde_section(pde))) {
				curaddr = (*pde & L1_S_FRAME) |
				    (vaddr & L1_S_OFFSET);
				if (*pde & L1_S_CACHE_MASK) {
					map->flags &=
					    ~DMAMAP_COHERENT;
				}
			} else {
				pte = *ptep;
				KASSERT((pte & L2_TYPE_MASK) != L2_TYPE_INV,
				    ("INV type"));
				if (__predict_false((pte & L2_TYPE_MASK)
						    == L2_TYPE_L)) {
					curaddr = (pte & L2_L_FRAME) |
					    (vaddr & L2_L_OFFSET);
					if (pte & L2_L_CACHE_MASK) {
						map->flags &=
						    ~DMAMAP_COHERENT;
						
					}
				} else {
					curaddr = (pte & L2_S_FRAME) |
					    (vaddr & L2_S_OFFSET);
					if (pte & L2_S_CACHE_MASK) {
						map->flags &=
						    ~DMAMAP_COHERENT;
					}
				}
			}
		} else {
			curaddr = pmap_extract(pmap, vaddr);
			map->flags &= ~DMAMAP_COHERENT;
		}

		if (dmat->ranges) {
			struct arm32_dma_range *dr;

			dr = _bus_dma_inrange(dmat->ranges, dmat->_nranges,
			    curaddr);
			if (dr == NULL)
				return (EINVAL);
			/*
		     	 * In a valid DMA range.  Translate the physical
			 * memory address to an address in the DMA window.
			 */
			curaddr = (curaddr - dr->dr_sysbase) + dr->dr_busbase;
						
		}
		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (dmat->boundary > 0) {
			baddr = (curaddr + dmat->boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

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
		error = EFBIG; /* XXX better return value here? */
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
						pmap_kernel(), flags, &lastaddr,
						nsegs);
				map->len += m->m_len;
			}
		}
	} else {
		error = EINVAL;
	}

	/* XXX FIXME: Having to increment nsegs is really annoying */
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
	vm_offset_t lastaddr;
#ifdef __CC_SUPPORTS_DYNAMIC_ARRAY_INIT
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	int nsegs, i, error;
	bus_size_t resid;
	struct iovec *iov;
	struct pmap *pmap;

	resid = uio->uio_resid;
	iov = uio->uio_iov;
	map->flags &= ~DMAMAP_TYPE_MASK;
	map->flags |= DMAMAP_UIO|DMAMAP_COHERENT;
	map->buffer = uio;
	map->len = 0;

	if (uio->uio_segflg == UIO_USERSPACE) {
		KASSERT(uio->uio_td != NULL,
		    ("bus_dmamap_load_uio: USERSPACE but no proc"));
		pmap = vmspace_pmap(uio->uio_td->td_proc->p_vmspace);
	} else
		pmap = kernel_pmap;

	error = 0;
	nsegs = -1;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && !error; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		bus_size_t minlen =
		    resid < iov[i].iov_len ? resid : iov[i].iov_len;
		caddr_t addr = (caddr_t) iov[i].iov_base;

		if (minlen > 0) {
			error = bus_dmamap_load_buffer(dmat, dm_segments, map,
			    addr, minlen, pmap, flags, &lastaddr, &nsegs);

			map->len += minlen;
			resid -= minlen;
		}
	}

	if (error) {
		/* 
		 * force "no valid mappings" on error in callback.
		 */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		(*callback)(callback_arg, dm_segments, nsegs+1,
		    uio->uio_resid, error);
	}

	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, error, nsegs + 1);
	return (error);
}

/*
 * Release the mapping held by map.
 */
void
_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	map->flags &= ~DMAMAP_TYPE_MASK;
	return;
}

static void
bus_dmamap_sync_buf(void *buf, int len, bus_dmasync_op_t op)
{

	if (op & BUS_DMASYNC_PREWRITE)
		cpu_dcache_wb_range((vm_offset_t)buf, len);
	if (op & BUS_DMASYNC_POSTREAD) {
		if ((((vm_offset_t)buf | len) & arm_dcache_align_mask) == 0)
			cpu_dcache_inv_range((vm_offset_t)buf, len);
		else    
			cpu_dcache_wbinv_range((vm_offset_t)buf, len);

	}
}

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct mbuf *m;
	struct uio *uio;
	int resid;
	struct iovec *iov;
	
	if (!(op & (BUS_DMASYNC_PREWRITE | BUS_DMASYNC_POSTREAD)))
		return;
	if (map->flags & DMAMAP_COHERENT)
		return;
	if (map->len > PAGE_SIZE) {
		cpu_dcache_wbinv_all();
		return;
	}
	CTR3(KTR_BUSDMA, "%s: op %x flags %x", __func__, op, map->flags);
	switch(map->flags & DMAMAP_TYPE_MASK) {
	case DMAMAP_LINEAR:
		bus_dmamap_sync_buf(map->buffer, map->len, op);
		break;
	case DMAMAP_MBUF:
		m = map->buffer;
		while (m) {
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
				bus_dmamap_sync_buf(iov[i].iov_base, minlen, 
				    op);
				resid -= minlen;
			}
		}
		break;
	default:
		break;
	}
	cpu_drain_writebuf();
}
