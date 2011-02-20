/*-
 * Copyright (c) 2006 Oleksandr Tymoshenko
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
 *  From i386/busdma_machdep.c,v 1.26 2002/04/19 22:58:09 alfred
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * MIPS bus dma support routines
 */

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
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/cpufunc.h>
#include <machine/cpuinfo.h>
#include <machine/md_var.h>

#define MAX_BPAGES 64
#define BUS_DMA_COULD_BOUNCE	BUS_DMA_BUS3
#define BUS_DMA_MIN_ALLOC_COMP	BUS_DMA_BUS4

struct bounce_zone;

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
	struct bounce_zone *bounce_zone;
};

struct bounce_page {
	vm_offset_t	vaddr;		/* kva of bounce buffer */
	vm_offset_t	vaddr_nocache;	/* kva of bounce buffer uncached */
	bus_addr_t	busaddr;	/* Physical address */
	vm_offset_t	datavaddr;	/* kva of client data */
	bus_size_t	datacount;	/* client data count */
	STAILQ_ENTRY(bounce_page) links;
};

int busdma_swi_pending;

struct bounce_zone {
	STAILQ_ENTRY(bounce_zone) links;
	STAILQ_HEAD(bp_list, bounce_page) bounce_page_list;
	int		total_bpages;
	int		free_bpages;
	int		reserved_bpages;
	int		active_bpages;
	int		total_bounced;
	int		total_deferred;
	int		map_count;
	bus_size_t	alignment;
	bus_addr_t	lowaddr;
	char		zoneid[8];
	char		lowaddrid[20];
	struct sysctl_ctx_list sysctl_tree;
	struct sysctl_oid *sysctl_tree_top;
};

static struct mtx bounce_lock;
static int total_bpages;
static int busdma_zonecount;
static STAILQ_HEAD(, bounce_zone) bounce_zone_list;

SYSCTL_NODE(_hw, OID_AUTO, busdma, CTLFLAG_RD, 0, "Busdma parameters");
SYSCTL_INT(_hw_busdma, OID_AUTO, total_bpages, CTLFLAG_RD, &total_bpages, 0,
	   "Total bounce pages");

#define DMAMAP_LINEAR		0x1
#define DMAMAP_MBUF		0x2
#define DMAMAP_UIO		0x4
#define DMAMAP_TYPE_MASK	(DMAMAP_LINEAR|DMAMAP_MBUF|DMAMAP_UIO)
#define DMAMAP_UNCACHEABLE	0x8
#define DMAMAP_ALLOCATED	0x10
#define DMAMAP_MALLOCUSED	0x20

struct bus_dmamap {
	struct bp_list	bpages;
	int		pagesneeded;
	int		pagesreserved;
	bus_dma_tag_t	dmat;
	int		flags;
	void 		*buffer;
	void		*origbuffer;
	void		*allocbuffer;
	TAILQ_ENTRY(bus_dmamap)	freelist;
	int		len;
	STAILQ_ENTRY(bus_dmamap) links;
	bus_dmamap_callback_t *callback;
	void		*callback_arg;

};

static STAILQ_HEAD(, bus_dmamap) bounce_map_waitinglist;
static STAILQ_HEAD(, bus_dmamap) bounce_map_callbacklist;

static TAILQ_HEAD(,bus_dmamap) dmamap_freelist = 
	TAILQ_HEAD_INITIALIZER(dmamap_freelist);

#define BUSDMA_STATIC_MAPS	500
static struct bus_dmamap map_pool[BUSDMA_STATIC_MAPS];

static struct mtx busdma_mtx;

MTX_SYSINIT(busdma_mtx, &busdma_mtx, "busdma lock", MTX_DEF);

static void init_bounce_pages(void *dummy);
static int alloc_bounce_zone(bus_dma_tag_t dmat);
static int alloc_bounce_pages(bus_dma_tag_t dmat, u_int numpages);
static int reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
				int commit);
static bus_addr_t add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map,
				   vm_offset_t vaddr, bus_size_t size);
static void free_bounce_page(bus_dma_tag_t dmat, struct bounce_page *bpage);

/* Default tag, as most drivers provide no parent tag. */
bus_dma_tag_t mips_root_dma_tag;

/*
 * Return true if a match is made.
 *
 * To find a match walk the chain of bus_dma_tag_t's looking for 'paddr'.
 *
 * If paddr is within the bounds of the dma tag then call the filter callback
 * to check for a match, if there is no filter callback then assume a match.
 */
static int
run_filter(bus_dma_tag_t dmat, bus_addr_t paddr)
{
	int retval;

	retval = 0;

	do {
		if (((paddr > dmat->lowaddr && paddr <= dmat->highaddr)
		 || ((paddr & (dmat->alignment - 1)) != 0))
		 && (dmat->filter == NULL
		  || (*dmat->filter)(dmat->filterarg, paddr) != 0))
			retval = 1;

		dmat = dmat->parent;		
	} while (retval == 0 && dmat != NULL);
	return (retval);
}

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

static __inline int
_bus_dma_can_bounce(vm_offset_t lowaddr, vm_offset_t highaddr)
{
	int i;
	for (i = 0; phys_avail[i] && phys_avail[i + 1]; i += 2) {
		if ((lowaddr >= phys_avail[i] && lowaddr <= phys_avail[i + 1])
		    || (lowaddr < phys_avail[i] && 
		    highaddr > phys_avail[i]))
			return (1);
	}
	return (0);
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
	STAILQ_INIT(&map->bpages);
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
	if (!parent)
		parent = mips_root_dma_tag;

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
	if (cpuinfo.cache_coherent_dma)
		newtag->flags |= BUS_DMA_COHERENT;
	newtag->ref_count = 1; /* Count ourself */
	newtag->map_count = 0;
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
			newtag->boundary =
			    min(parent->boundary, newtag->boundary);
		if ((newtag->filter != NULL) ||
		    ((parent->flags & BUS_DMA_COULD_BOUNCE) != 0))
			newtag->flags |= BUS_DMA_COULD_BOUNCE;
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
	if (_bus_dma_can_bounce(newtag->lowaddr, newtag->highaddr)
	 || newtag->alignment > 1)
		newtag->flags |= BUS_DMA_COULD_BOUNCE;

	if (((newtag->flags & BUS_DMA_COULD_BOUNCE) != 0) &&
	    (flags & BUS_DMA_ALLOCNOW) != 0) {
		struct bounce_zone *bz;

		/* Must bounce */

		if ((error = alloc_bounce_zone(newtag)) != 0) {
			free(newtag, M_DEVBUF);
			return (error);
		}
		bz = newtag->bounce_zone;

		if (ptoa(bz->total_bpages) < maxsize) {
			int pages;

			pages = atop(maxsize) - bz->total_bpages;

			/* Add pages to our bounce pool */
			if (alloc_bounce_pages(newtag, pages) < pages)
				error = ENOMEM;
		}
		/* Performed initial allocation */
		newtag->flags |= BUS_DMA_MIN_ALLOC_COMP;
	} else
		newtag->bounce_zone = NULL;
	if (error != 0)
		free(newtag, M_DEVBUF);
	else
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

#include <sys/kdb.h>
/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	bus_dmamap_t newmap;
	int error = 0;

	newmap = _busdma_alloc_dmamap();
	if (newmap == NULL) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d", __func__, dmat, ENOMEM);
		return (ENOMEM);
	}
	*mapp = newmap;
	newmap->dmat = dmat;
	newmap->allocbuffer = NULL;
	dmat->map_count++;

	/*
	 * Bouncing might be required if the driver asks for an active
	 * exclusion region, a data alignment that is stricter than 1, and/or
	 * an active address boundary.
	 */
	if (dmat->flags & BUS_DMA_COULD_BOUNCE) {

		/* Must bounce */
		struct bounce_zone *bz;
		int maxpages;

		if (dmat->bounce_zone == NULL) {
			if ((error = alloc_bounce_zone(dmat)) != 0) {
				_busdma_free_dmamap(newmap);
				*mapp = NULL;
				return (error);
			}
		}
		bz = dmat->bounce_zone;

		/* Initialize the new map */
		STAILQ_INIT(&((*mapp)->bpages));

		/*
		 * Attempt to add pages to our pool on a per-instance
		 * basis up to a sane limit.
		 */
		maxpages = MAX_BPAGES;
		if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0
		 || (bz->map_count > 0 && bz->total_bpages < maxpages)) {
			int pages;

			pages = MAX(atop(dmat->maxsize), 1);
			pages = MIN(maxpages - bz->total_bpages, pages);
			pages = MAX(pages, 1);
			if (alloc_bounce_pages(dmat, pages) < pages)
				error = ENOMEM;

			if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0) {
				if (error == 0)
					dmat->flags |= BUS_DMA_MIN_ALLOC_COMP;
			} else {
				error = 0;
			}
		}
		bz->map_count++;
	}

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
	if (STAILQ_FIRST(&map->bpages) != NULL) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d",
		    __func__, dmat, EBUSY);
		return (EBUSY);
	}
	if (dmat->bounce_zone)
		dmat->bounce_zone->map_count--;
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

	/*
	 * If all the memory is coherent with DMA then we don't need to
	 * do anything special for a coherent mapping request.
	 */
	if (dmat->flags & BUS_DMA_COHERENT)
	    flags &= ~BUS_DMA_COHERENT;

	/*
	 * Allocate uncacheable memory if all else fails.
	 */
	if (flags & BUS_DMA_COHERENT)
	    newmap->flags |= DMAMAP_UNCACHEABLE;

	if (dmat->maxsize <= PAGE_SIZE &&
	   (dmat->alignment < dmat->maxsize) &&
	   !_bus_dma_can_bounce(dmat->lowaddr, dmat->highaddr) && 
	   !(newmap->flags & DMAMAP_UNCACHEABLE)) {
                *vaddr = malloc(dmat->maxsize, M_DEVBUF, mflags);
		newmap->flags |= DMAMAP_MALLOCUSED;
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

	if (newmap->flags & DMAMAP_UNCACHEABLE) {
		void *tmpaddr = (void *)*vaddr;

		if (tmpaddr) {
			tmpaddr = (void *)pmap_mapdev(vtophys(tmpaddr),
			    dmat->maxsize);
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

	if (map->flags & DMAMAP_UNCACHEABLE)
		pmap_unmapdev((vm_offset_t)map->allocbuffer, dmat->maxsize);
	if (map->flags & DMAMAP_MALLOCUSED)
		free(vaddr, M_DEVBUF);
	else
		contigfree(vaddr, dmat->maxsize, M_DEVBUF);

	dmat->map_count--;
	_busdma_free_dmamap(map);
	CTR3(KTR_BUSDMA, "%s: tag %p flags 0x%x", __func__, dmat, dmat->flags);
}

static int
_bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map, pmap_t pmap,
    void *buf, bus_size_t buflen, int flags)
{
	vm_offset_t vaddr;
	vm_offset_t vendaddr;
	bus_addr_t paddr;

	if ((map->pagesneeded == 0)) {
		CTR3(KTR_BUSDMA, "lowaddr= %d, boundary= %d, alignment= %d",
		    dmat->lowaddr, dmat->boundary, dmat->alignment);
		CTR2(KTR_BUSDMA, "map= %p, pagesneeded= %d",
		    map, map->pagesneeded);
		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		vaddr = (vm_offset_t)buf;
		vendaddr = (vm_offset_t)buf + buflen;

		while (vaddr < vendaddr) {
			bus_size_t sg_len;

			KASSERT(kernel_pmap == pmap, ("pmap is not kernel pmap"));
			sg_len = PAGE_SIZE - ((vm_offset_t)vaddr & PAGE_MASK);
			paddr = pmap_kextract(vaddr);
			if (((dmat->flags & BUS_DMA_COULD_BOUNCE) != 0) &&
			    run_filter(dmat, paddr) != 0) {
				sg_len = roundup2(sg_len, dmat->alignment);
				map->pagesneeded++;
			}
			vaddr += sg_len;
		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}

	/* Reserve Necessary Bounce Pages */
	if (map->pagesneeded != 0) {
		mtx_lock(&bounce_lock);
		if (flags & BUS_DMA_NOWAIT) {
			if (reserve_bounce_pages(dmat, map, 0) != 0) {
				mtx_unlock(&bounce_lock);
				return (ENOMEM);
			}
		} else {
			if (reserve_bounce_pages(dmat, map, 1) != 0) {
				/* Queue us for resources */
				STAILQ_INSERT_TAIL(&bounce_map_waitinglist,
				    map, links);
				mtx_unlock(&bounce_lock);
				return (EINPROGRESS);
			}
		}
		mtx_unlock(&bounce_lock);
	}

	return (0);
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
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vm_offset_t vaddr = (vm_offset_t)buf;
	int seg;
	int error = 0;

	lastaddr = *lastaddrp;
	bmask = ~(dmat->boundary - 1);

	if ((dmat->flags & BUS_DMA_COULD_BOUNCE) != 0) {
		error = _bus_dmamap_count_pages(dmat, map, pmap, buf, buflen,
		    flags);
		if (error)
			return (error);
	}
	CTR3(KTR_BUSDMA, "lowaddr= %d boundary= %d, "
	    "alignment= %d", dmat->lowaddr, dmat->boundary, dmat->alignment);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 *
		 * XXX Don't support checking for coherent mappings
		 * XXX in user address space.
		 */
		KASSERT(kernel_pmap == pmap, ("pmap is not kernel pmap"));
		curaddr = pmap_kextract(vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (sgsize > dmat->maxsegsz)
			sgsize = dmat->maxsegsz;
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
		if (((dmat->flags & BUS_DMA_COULD_BOUNCE) != 0) &&
		    map->pagesneeded != 0 && run_filter(dmat, curaddr)) {
			curaddr = add_bounce_page(dmat, map, vaddr, sgsize);
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
	map->callback = callback;
	map->callback_arg = callback_arg;
	map->flags &= ~DMAMAP_TYPE_MASK;
	map->flags |= DMAMAP_LINEAR;
	map->buffer = buf;
	map->len = buflen;
	error = bus_dmamap_load_buffer(dmat,
	    dm_segments, map, buf, buflen, kernel_pmap,
	    flags, &lastaddr, &nsegs);
	if (error == EINPROGRESS)
		return (error);
	if (error)
		(*callback)(callback_arg, NULL, 0, error);
	else
		(*callback)(callback_arg, dm_segments, nsegs + 1, error);
	
	CTR5(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d nsegs %d",
	    __func__, dmat, dmat->flags, nsegs + 1, error);

	return (error);
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
	map->flags |= DMAMAP_MBUF;
	map->buffer = m0;
	map->len = 0;
	if (m0->m_pkthdr.len <= dmat->maxsize) {
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = bus_dmamap_load_buffer(dmat,
				    dm_segments, map, m->m_data, m->m_len, 
				    kernel_pmap, flags, &lastaddr, &nsegs);
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
	map->flags |= DMAMAP_MBUF;
	map->buffer = m0;			
	map->len = 0;
	if (m0->m_pkthdr.len <= dmat->maxsize) {
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len > 0) {
				error = bus_dmamap_load_buffer(dmat, segs, map,
						m->m_data, m->m_len,
						kernel_pmap, flags, &lastaddr,
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
	vm_offset_t lastaddr = 0;
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
	map->flags |= DMAMAP_UIO;
	map->buffer = uio;
	map->len = 0;

	if (uio->uio_segflg == UIO_USERSPACE) {
		KASSERT(uio->uio_td != NULL,
		    ("bus_dmamap_load_uio: USERSPACE but no proc"));
		/* XXX: pmap = vmspace_pmap(uio->uio_td->td_proc->p_vmspace); */
		panic("can't do it yet");
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
	struct bounce_page *bpage;

	map->flags &= ~DMAMAP_TYPE_MASK;
	while ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
		STAILQ_REMOVE_HEAD(&map->bpages, links);
		free_bounce_page(dmat, bpage);
	}
	return;
}

static void
bus_dmamap_sync_buf(void *buf, int len, bus_dmasync_op_t op)
{
	char tmp_cl[mips_pdcache_linesize], tmp_clend[mips_pdcache_linesize];
	vm_offset_t buf_cl, buf_clend;
	vm_size_t size_cl, size_clend;
	int cache_linesize_mask = mips_pdcache_linesize - 1;

	/*
	 * dcache invalidation operates on cache line aligned addresses
	 * and could modify areas of memory that share the same cache line
	 * at the beginning and the ending of the buffer. In order to 
	 * prevent a data loss we save these chunks in temporary buffer
	 * before invalidation and restore them afer it
	 */
	buf_cl = (vm_offset_t)buf  & ~cache_linesize_mask;
	size_cl = (vm_offset_t)buf  & cache_linesize_mask;
	buf_clend = (vm_offset_t)buf + len;
	size_clend = (mips_pdcache_linesize - 
	    (buf_clend & cache_linesize_mask)) & cache_linesize_mask;

	switch (op) {
	case BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE:
	case BUS_DMASYNC_POSTREAD:

		/* 
		 * Save buffers that might be modified by invalidation
		 */
		if (size_cl)
			memcpy (tmp_cl, (void*)buf_cl, size_cl);
		if (size_clend)
			memcpy (tmp_clend, (void*)buf_clend, size_clend);
		mips_dcache_inv_range((vm_offset_t)buf, len);
		/* 
		 * Restore them
		 */
		if (size_cl)
			memcpy ((void*)buf_cl, tmp_cl, size_cl);
		if (size_clend)
			memcpy ((void*)buf_clend, tmp_clend, size_clend);
		/* 
		 * Copies above have brought corresponding memory
		 * cache lines back into dirty state. Write them back
		 * out and invalidate affected cache lines again if
		 * necessary.
		 */
		if (size_cl)
			mips_dcache_wbinv_range((vm_offset_t)buf_cl, size_cl);
		if (size_clend && (size_cl == 0 ||
                    buf_clend - buf_cl > mips_pdcache_linesize))
			mips_dcache_wbinv_range((vm_offset_t)buf_clend,
			   size_clend);
		break;

	case BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE:
		mips_dcache_wbinv_range((vm_offset_t)buf_cl, len);
		break;

	case BUS_DMASYNC_PREREAD:
		/* 
		 * Save buffers that might be modified by invalidation
		 */
		if (size_cl)
			memcpy (tmp_cl, (void *)buf_cl, size_cl);
		if (size_clend)
			memcpy (tmp_clend, (void *)buf_clend, size_clend);
		mips_dcache_inv_range((vm_offset_t)buf, len);
		/*
		 * Restore them
		 */
		if (size_cl)
			memcpy ((void *)buf_cl, tmp_cl, size_cl);
		if (size_clend)
			memcpy ((void *)buf_clend, tmp_clend, size_clend);
		/* 
		 * Copies above have brought corresponding memory
		 * cache lines back into dirty state. Write them back
		 * out and invalidate affected cache lines again if
		 * necessary.
		 */
		if (size_cl)
			mips_dcache_wbinv_range((vm_offset_t)buf_cl, size_cl);
		if (size_clend && (size_cl == 0 ||
                    buf_clend - buf_cl > mips_pdcache_linesize))
			mips_dcache_wbinv_range((vm_offset_t)buf_clend,
			   size_clend);
		break;

	case BUS_DMASYNC_PREWRITE:
		mips_dcache_wb_range((vm_offset_t)buf, len);
		break;
	}
}

static void
_bus_dmamap_sync_bp(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct bounce_page *bpage;

	STAILQ_FOREACH(bpage, &map->bpages, links) {
		if (op & BUS_DMASYNC_PREWRITE) {
			bcopy((void *)bpage->datavaddr,
			    (void *)(bpage->vaddr_nocache != 0 ? 
				     bpage->vaddr_nocache : bpage->vaddr),
			    bpage->datacount);
			if (bpage->vaddr_nocache == 0) {
				mips_dcache_wb_range(bpage->vaddr,
				    bpage->datacount);
			}
			dmat->bounce_zone->total_bounced++;
		}
		if (op & BUS_DMASYNC_POSTREAD) {
			if (bpage->vaddr_nocache == 0) {
				mips_dcache_inv_range(bpage->vaddr,
				    bpage->datacount);
			}
			bcopy((void *)(bpage->vaddr_nocache != 0 ? 
	       		    bpage->vaddr_nocache : bpage->vaddr),
			    (void *)bpage->datavaddr, bpage->datacount);
			dmat->bounce_zone->total_bounced++;
		}
	}
}

static __inline int
_bus_dma_buf_is_in_bp(bus_dmamap_t map, void *buf, int len)
{
	struct bounce_page *bpage;

	STAILQ_FOREACH(bpage, &map->bpages, links) {
		if ((vm_offset_t)buf >= bpage->datavaddr &&
		    (vm_offset_t)buf + len <= bpage->datavaddr + 
		    bpage->datacount)
			return (1);
	}
	return (0);

}

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct mbuf *m;
	struct uio *uio;
	int resid;
	struct iovec *iov;
	
	if (op == BUS_DMASYNC_POSTWRITE)
		return;
	if (STAILQ_FIRST(&map->bpages))
		_bus_dmamap_sync_bp(dmat, map, op);

	if (dmat->flags & BUS_DMA_COHERENT)
		return;

	if (map->flags & DMAMAP_UNCACHEABLE)
		return;

	CTR3(KTR_BUSDMA, "%s: op %x flags %x", __func__, op, map->flags);
	switch(map->flags & DMAMAP_TYPE_MASK) {
	case DMAMAP_LINEAR:
		if (!(_bus_dma_buf_is_in_bp(map, map->buffer, map->len)))
			bus_dmamap_sync_buf(map->buffer, map->len, op);
		break;
	case DMAMAP_MBUF:
		m = map->buffer;
		while (m) {
			if (m->m_len > 0 &&
			    !(_bus_dma_buf_is_in_bp(map, m->m_data, m->m_len)))
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
				if (!_bus_dma_buf_is_in_bp(map, iov[i].iov_base,
				    minlen))
					bus_dmamap_sync_buf(iov[i].iov_base,
					    minlen, op);
				resid -= minlen;
			}
		}
		break;
	default:
		break;
	}
}

static void
init_bounce_pages(void *dummy __unused)
{

	total_bpages = 0;
	STAILQ_INIT(&bounce_zone_list);
	STAILQ_INIT(&bounce_map_waitinglist);
	STAILQ_INIT(&bounce_map_callbacklist);
	mtx_init(&bounce_lock, "bounce pages lock", NULL, MTX_DEF);
}
SYSINIT(bpages, SI_SUB_LOCK, SI_ORDER_ANY, init_bounce_pages, NULL);

static struct sysctl_ctx_list *
busdma_sysctl_tree(struct bounce_zone *bz)
{
	return (&bz->sysctl_tree);
}

static struct sysctl_oid *
busdma_sysctl_tree_top(struct bounce_zone *bz)
{
	return (bz->sysctl_tree_top);
}

static int
alloc_bounce_zone(bus_dma_tag_t dmat)
{
	struct bounce_zone *bz;

	/* Check to see if we already have a suitable zone */
	STAILQ_FOREACH(bz, &bounce_zone_list, links) {
		if ((dmat->alignment <= bz->alignment)
		 && (dmat->lowaddr >= bz->lowaddr)) {
			dmat->bounce_zone = bz;
			return (0);
		}
	}

	if ((bz = (struct bounce_zone *)malloc(sizeof(*bz), M_DEVBUF,
	    M_NOWAIT | M_ZERO)) == NULL)
		return (ENOMEM);

	STAILQ_INIT(&bz->bounce_page_list);
	bz->free_bpages = 0;
	bz->reserved_bpages = 0;
	bz->active_bpages = 0;
	bz->lowaddr = dmat->lowaddr;
	bz->alignment = MAX(dmat->alignment, PAGE_SIZE);
	bz->map_count = 0;
	snprintf(bz->zoneid, 8, "zone%d", busdma_zonecount);
	busdma_zonecount++;
	snprintf(bz->lowaddrid, 18, "%#jx", (uintmax_t)bz->lowaddr);
	STAILQ_INSERT_TAIL(&bounce_zone_list, bz, links);
	dmat->bounce_zone = bz;

	sysctl_ctx_init(&bz->sysctl_tree);
	bz->sysctl_tree_top = SYSCTL_ADD_NODE(&bz->sysctl_tree,
	    SYSCTL_STATIC_CHILDREN(_hw_busdma), OID_AUTO, bz->zoneid,
	    CTLFLAG_RD, 0, "");
	if (bz->sysctl_tree_top == NULL) {
		sysctl_ctx_free(&bz->sysctl_tree);
		return (0);	/* XXX error code? */
	}

	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "total_bpages", CTLFLAG_RD, &bz->total_bpages, 0,
	    "Total bounce pages");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "free_bpages", CTLFLAG_RD, &bz->free_bpages, 0,
	    "Free bounce pages");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "reserved_bpages", CTLFLAG_RD, &bz->reserved_bpages, 0,
	    "Reserved bounce pages");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "active_bpages", CTLFLAG_RD, &bz->active_bpages, 0,
	    "Active bounce pages");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "total_bounced", CTLFLAG_RD, &bz->total_bounced, 0,
	    "Total bounce requests");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "total_deferred", CTLFLAG_RD, &bz->total_deferred, 0,
	    "Total bounce requests that were deferred");
	SYSCTL_ADD_STRING(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "lowaddr", CTLFLAG_RD, bz->lowaddrid, 0, "");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "alignment", CTLFLAG_RD, &bz->alignment, 0, "");

	return (0);
}

static int
alloc_bounce_pages(bus_dma_tag_t dmat, u_int numpages)
{
	struct bounce_zone *bz;
	int count;

	bz = dmat->bounce_zone;
	count = 0;
	while (numpages > 0) {
		struct bounce_page *bpage;

		bpage = (struct bounce_page *)malloc(sizeof(*bpage), M_DEVBUF,
						     M_NOWAIT | M_ZERO);

		if (bpage == NULL)
			break;
		bpage->vaddr = (vm_offset_t)contigmalloc(PAGE_SIZE, M_DEVBUF,
							 M_NOWAIT, 0ul,
							 bz->lowaddr,
							 PAGE_SIZE,
							 0);
		if (bpage->vaddr == 0) {
			free(bpage, M_DEVBUF);
			break;
		}
		bpage->busaddr = pmap_kextract(bpage->vaddr);
		bpage->vaddr_nocache = 
		    (vm_offset_t)pmap_mapdev(bpage->busaddr, PAGE_SIZE);
		mtx_lock(&bounce_lock);
		STAILQ_INSERT_TAIL(&bz->bounce_page_list, bpage, links);
		total_bpages++;
		bz->total_bpages++;
		bz->free_bpages++;
		mtx_unlock(&bounce_lock);
		count++;
		numpages--;
	}
	return (count);
}

static int
reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map, int commit)
{
	struct bounce_zone *bz;
	int pages;

	mtx_assert(&bounce_lock, MA_OWNED);
	bz = dmat->bounce_zone;
	pages = MIN(bz->free_bpages, map->pagesneeded - map->pagesreserved);
	if (commit == 0 && map->pagesneeded > (map->pagesreserved + pages))
		return (map->pagesneeded - (map->pagesreserved + pages));
	bz->free_bpages -= pages;
	bz->reserved_bpages += pages;
	map->pagesreserved += pages;
	pages = map->pagesneeded - map->pagesreserved;

	return (pages);
}

static bus_addr_t
add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map, vm_offset_t vaddr,
		bus_size_t size)
{
	struct bounce_zone *bz;
	struct bounce_page *bpage;

	KASSERT(dmat->bounce_zone != NULL, ("no bounce zone in dma tag"));
	KASSERT(map != NULL, ("add_bounce_page: bad map %p", map));

	bz = dmat->bounce_zone;
	if (map->pagesneeded == 0)
		panic("add_bounce_page: map doesn't need any pages");
	map->pagesneeded--;

	if (map->pagesreserved == 0)
		panic("add_bounce_page: map doesn't need any pages");
	map->pagesreserved--;

	mtx_lock(&bounce_lock);
	bpage = STAILQ_FIRST(&bz->bounce_page_list);
	if (bpage == NULL)
		panic("add_bounce_page: free page list is empty");

	STAILQ_REMOVE_HEAD(&bz->bounce_page_list, links);
	bz->reserved_bpages--;
	bz->active_bpages++;
	mtx_unlock(&bounce_lock);

	if (dmat->flags & BUS_DMA_KEEP_PG_OFFSET) {
		/* Page offset needs to be preserved. */
		bpage->vaddr |= vaddr & PAGE_MASK;
		bpage->busaddr |= vaddr & PAGE_MASK;
	}
	bpage->datavaddr = vaddr;
	bpage->datacount = size;
	STAILQ_INSERT_TAIL(&(map->bpages), bpage, links);
	return (bpage->busaddr);
}

static void
free_bounce_page(bus_dma_tag_t dmat, struct bounce_page *bpage)
{
	struct bus_dmamap *map;
	struct bounce_zone *bz;

	bz = dmat->bounce_zone;
	bpage->datavaddr = 0;
	bpage->datacount = 0;
	if (dmat->flags & BUS_DMA_KEEP_PG_OFFSET) {
		/*
		 * Reset the bounce page to start at offset 0.  Other uses
		 * of this bounce page may need to store a full page of
		 * data and/or assume it starts on a page boundary.
		 */
		bpage->vaddr &= ~PAGE_MASK;
		bpage->busaddr &= ~PAGE_MASK;
	}

	mtx_lock(&bounce_lock);
	STAILQ_INSERT_HEAD(&bz->bounce_page_list, bpage, links);
	bz->free_bpages++;
	bz->active_bpages--;
	if ((map = STAILQ_FIRST(&bounce_map_waitinglist)) != NULL) {
		if (reserve_bounce_pages(map->dmat, map, 1) == 0) {
			STAILQ_REMOVE_HEAD(&bounce_map_waitinglist, links);
			STAILQ_INSERT_TAIL(&bounce_map_callbacklist,
					   map, links);
			busdma_swi_pending = 1;
			bz->total_deferred++;
			swi_sched(vm_ih, 0);
		}
	}
	mtx_unlock(&bounce_lock);
}

void
busdma_swi(void)
{
	bus_dma_tag_t dmat;
	struct bus_dmamap *map;

	mtx_lock(&bounce_lock);
	while ((map = STAILQ_FIRST(&bounce_map_callbacklist)) != NULL) {
		STAILQ_REMOVE_HEAD(&bounce_map_callbacklist, links);
		mtx_unlock(&bounce_lock);
		dmat = map->dmat;
		(dmat->lockfunc)(dmat->lockfuncarg, BUS_DMA_LOCK);
		bus_dmamap_load(map->dmat, map, map->buffer, map->len,
		    map->callback, map->callback_arg, /*flags*/0);
		(dmat->lockfunc)(dmat->lockfuncarg, BUS_DMA_UNLOCK);
		mtx_lock(&bounce_lock);
	}
	mtx_unlock(&bounce_lock);
}
