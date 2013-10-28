/*-
 * Copyright (c) 2012 Ian Lepore
 * Copyright (c) 2010 Mark Tinguely
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
 *  From i386/busdma_machdep.c 191438 2009-04-23 20:24:19Z jhb
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#define _ARM32_BUS_DMA_PRIVATE
#include <sys/param.h>
#include <sys/kdb.h>
#include <ddb/ddb.h>
#include <ddb/db_output.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/busdma_bufalloc.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/memdesc.h>
#include <sys/proc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>

#define MAX_BPAGES 64
#define BUS_DMA_COULD_BOUNCE	BUS_DMA_BUS3
#define BUS_DMA_MIN_ALLOC_COMP	BUS_DMA_BUS4

struct bounce_zone;

struct bus_dma_tag {
	bus_dma_tag_t	  parent;
	bus_size_t	  alignment;
	bus_size_t	  boundary;
	bus_addr_t	  lowaddr;
	bus_addr_t	  highaddr;
	bus_dma_filter_t *filter;
	void		 *filterarg;
	bus_size_t	  maxsize;
	u_int		  nsegments;
	bus_size_t	  maxsegsz;
	int		  flags;
	int		  ref_count;
	int		  map_count;
	bus_dma_lock_t	 *lockfunc;
	void		 *lockfuncarg;
	struct bounce_zone *bounce_zone;
	/*
	 * DMA range for this tag.  If the page doesn't fall within
	 * one of these ranges, an error is returned.  The caller
	 * may then decide what to do with the transfer.  If the
	 * range pointer is NULL, it is ignored.
	 */
	struct arm32_dma_range	*ranges;
	int			_nranges;
	/*
	 * Most tags need one or two segments, and can use the local tagsegs
	 * array.  For tags with a larger limit, we'll allocate a bigger array
	 * on first use.
	 */
	bus_dma_segment_t	*segments;
	bus_dma_segment_t	tagsegs[2];


};

struct bounce_page {
	vm_offset_t	vaddr;		/* kva of bounce buffer */
	bus_addr_t	busaddr;	/* Physical address */
	vm_offset_t	datavaddr;	/* kva of client data */
	bus_addr_t	dataaddr;	/* client physical address */
	bus_size_t	datacount;	/* client data count */
	STAILQ_ENTRY(bounce_page) links;
};

struct sync_list {
	vm_offset_t	vaddr;		/* kva of bounce buffer */
	bus_addr_t	busaddr;	/* Physical address */
	bus_size_t	datacount;	/* client data count */
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

struct bus_dmamap {
	struct bp_list	       bpages;
	int		       pagesneeded;
	int		       pagesreserved;
	bus_dma_tag_t	       dmat;
	struct memdesc	       mem;
	pmap_t		       pmap;
	bus_dmamap_callback_t *callback;
	void		      *callback_arg;
	int		      flags;
#define DMAMAP_COHERENT		(1 << 0)
	STAILQ_ENTRY(bus_dmamap) links;
	int		       sync_count;
	struct sync_list       slist[];
};

static STAILQ_HEAD(, bus_dmamap) bounce_map_waitinglist;
static STAILQ_HEAD(, bus_dmamap) bounce_map_callbacklist;

static void init_bounce_pages(void *dummy);
static int alloc_bounce_zone(bus_dma_tag_t dmat);
static int alloc_bounce_pages(bus_dma_tag_t dmat, u_int numpages);
static int reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
				int commit);
static bus_addr_t add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map,
				  vm_offset_t vaddr, bus_addr_t addr,
				  bus_size_t size);
static void free_bounce_page(bus_dma_tag_t dmat, struct bounce_page *bpage);
int run_filter(bus_dma_tag_t dmat, bus_addr_t paddr, bus_size_t size, int coherent);
static void _bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    void *buf, bus_size_t buflen, int flags);
static void _bus_dmamap_count_phys(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t buf, bus_size_t buflen, int flags);
static int _bus_dmamap_reserve_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    int flags);

static busdma_bufalloc_t coherent_allocator;	/* Cache of coherent buffers */
static busdma_bufalloc_t standard_allocator;	/* Cache of standard buffers */
static void
busdma_init(void *dummy)
{
	int uma_flags;

	uma_flags = 0;

	/* Create a cache of buffers in standard (cacheable) memory. */
	standard_allocator = busdma_bufalloc_create("buffer", 
	    arm_dcache_align,	/* minimum_alignment */
	    NULL,		/* uma_alloc func */ 
	    NULL,		/* uma_free func */
	    uma_flags);		/* uma_zcreate_flags */

#ifdef INVARIANTS
	/* 
	 * Force UMA zone to allocate service structures like
	 * slabs using own allocator. uma_debug code performs
	 * atomic ops on uma_slab_t fields and safety of this
	 * operation is not guaranteed for write-back caches
	 */
	uma_flags = UMA_ZONE_OFFPAGE;
#endif
	/*
	 * Create a cache of buffers in uncacheable memory, to implement the
	 * BUS_DMA_COHERENT (and potentially BUS_DMA_NOCACHE) flag.
	 */
	coherent_allocator = busdma_bufalloc_create("coherent",
	    arm_dcache_align,	/* minimum_alignment */
	    busdma_bufalloc_alloc_uncacheable, 
	    busdma_bufalloc_free_uncacheable, 
	    uma_flags);	/* uma_zcreate_flags */
}

/*
 * This init historically used SI_SUB_VM, but now the init code requires
 * malloc(9) using M_DEVBUF memory, which is set up later than SI_SUB_VM, by
 * SI_SUB_KMEM and SI_ORDER_SECOND, so we'll go right after that by using
 * SI_SUB_KMEM and SI_ORDER_THIRD.
 */
SYSINIT(busdma, SI_SUB_KMEM, SI_ORDER_THIRD, busdma_init, NULL);

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
 * Return true if a match is made.
 *
 * To find a match walk the chain of bus_dma_tag_t's looking for 'paddr'.
 *
 * If paddr is within the bounds of the dma tag then call the filter callback
 * to check for a match, if there is no filter callback then assume a match.
 */
int
run_filter(bus_dma_tag_t dmat, bus_addr_t paddr, bus_size_t size, int coherent)
{
	int retval;

	retval = 0;

	do {
		if (((paddr > dmat->lowaddr && paddr <= dmat->highaddr)
		 || ((paddr & (dmat->alignment - 1)) != 0) ||
		 (!coherent && (size & arm_dcache_align_mask)) ||
		 (!coherent && (paddr & arm_dcache_align_mask)))
		 && (dmat->filter == NULL
		  || (*dmat->filter)(dmat->filterarg, paddr) != 0))
			retval = 1;

		dmat = dmat->parent;
	} while (retval == 0 && dmat != NULL);
	return (retval);
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
	panic("driver error: busdma dflt_lock called");
}

/*
 * Allocate a device specific dma_tag.
 */
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

#if 0
	if (!parent)
		parent = arm_root_dma_tag;
#endif

	/* Basic sanity checking */
	if (boundary != 0 && boundary < maxsegsz)
		maxsegsz = boundary;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	if (maxsegsz == 0) {
		return (EINVAL);
	}

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
	 * If all the segments we need fit into the local tagsegs array, set the
	 * pointer now.  Otherwise NULL the pointer and an array of segments
	 * will be allocated later, on first use.  We don't pre-allocate now
	 * because some tags exist just to pass contraints to children in the
	 * device hierarchy, and they tend to use BUS_SPACE_UNRESTRICTED and we
	 * sure don't want to try to allocate an array for that.
	 */
	if (newtag->nsegments <= nitems(newtag->tagsegs))
		newtag->segments = newtag->tagsegs;
	else
		newtag->segments = NULL;

	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		newtag->lowaddr = MIN(parent->lowaddr, newtag->lowaddr);
		newtag->highaddr = MAX(parent->highaddr, newtag->highaddr);
		if (newtag->boundary == 0)
			newtag->boundary = parent->boundary;
		else if (parent->boundary != 0)
			newtag->boundary = MIN(parent->boundary,
					       newtag->boundary);
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

	/*
	 * Any request can auto-bounce due to cacheline alignment, in addition
	 * to any alignment or boundary specifications in the tag, so if the
	 * ALLOCNOW flag is set, there's always work to do.
	 */
	if ((flags & BUS_DMA_ALLOCNOW) != 0) {
		struct bounce_zone *bz;
		/*
		 * Round size up to a full page, and add one more page because
		 * there can always be one more boundary crossing than the
		 * number of pages in a transfer.
		 */
		maxsize = roundup2(maxsize, PAGE_SIZE) + PAGE_SIZE;
		
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

	if (error != 0) {
		free(newtag, M_DEVBUF);
	} else {
		*dmat = newtag;
	}
	CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
	    __func__, newtag, (newtag != NULL ? newtag->flags : 0), error);
	return (error);
}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
	bus_dma_tag_t dmat_copy;
	int error;

	error = 0;
	dmat_copy = dmat;

	if (dmat != NULL) {

		if (dmat->map_count != 0) {
			error = EBUSY;
			goto out;
		}

		while (dmat != NULL) {
			bus_dma_tag_t parent;

			parent = dmat->parent;
			atomic_subtract_int(&dmat->ref_count, 1);
			if (dmat->ref_count == 0) {
				if (dmat->segments != NULL &&
				    dmat->segments != dmat->tagsegs)
					free(dmat->segments, M_DEVBUF);
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
out:
	CTR3(KTR_BUSDMA, "%s tag %p error %d", __func__, dmat_copy, error);
	return (error);
}

static int allocate_bz_and_pages(bus_dma_tag_t dmat, bus_dmamap_t mapp)
{
        struct bounce_zone *bz;
	int maxpages;
	int error;
		
	if (dmat->bounce_zone == NULL)
		if ((error = alloc_bounce_zone(dmat)) != 0)
			return (error);
	bz = dmat->bounce_zone;
	/* Initialize the new map */
	STAILQ_INIT(&(mapp->bpages));

	/*
	 * Attempt to add pages to our pool on a per-instance basis up to a sane
	 * limit.  Even if the tag isn't flagged as COULD_BOUNCE due to
	 * alignment and boundary constraints, it could still auto-bounce due to
	 * cacheline alignment, which requires at most two bounce pages.
	 */
	if (dmat->flags & BUS_DMA_COULD_BOUNCE)
		maxpages = MAX_BPAGES;
	else
		maxpages = 2 * bz->map_count;
	if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0
	    || (bz->map_count > 0 && bz->total_bpages < maxpages)) {
		int pages;
		
		pages = atop(roundup2(dmat->maxsize, PAGE_SIZE)) + 1;
		pages = MIN(maxpages - bz->total_bpages, pages);
		pages = MAX(pages, 2);
		if (alloc_bounce_pages(dmat, pages) < pages)
			return (ENOMEM);
		
		if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0)
			dmat->flags |= BUS_DMA_MIN_ALLOC_COMP;
	}
	bz->map_count++;
	return (0);
}

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	int mapsize;
	int error = 0;

	mapsize = sizeof(**mapp) + (sizeof(struct sync_list) * dmat->nsegments);
	*mapp = (bus_dmamap_t)malloc(mapsize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*mapp == NULL) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d", __func__, dmat, ENOMEM);
		return (ENOMEM);
	}
	(*mapp)->sync_count = 0;

	if (dmat->segments == NULL) {
		dmat->segments = (bus_dma_segment_t *)malloc(
		    sizeof(bus_dma_segment_t) * dmat->nsegments, M_DEVBUF,
		    M_NOWAIT);
		if (dmat->segments == NULL) {
			CTR3(KTR_BUSDMA, "%s: tag %p error %d",
			    __func__, dmat, ENOMEM);
			free(*mapp, M_DEVBUF);
			*mapp = NULL;
			return (ENOMEM);
		}
	}
	/*
	 * Bouncing might be required if the driver asks for an active
	 * exclusion region, a data alignment that is stricter than 1, and/or
	 * an active address boundary.
	 */
	error = allocate_bz_and_pages(dmat, *mapp);
	if (error != 0) {
		free(*mapp, M_DEVBUF);
		*mapp = NULL;
		return (error);
	}
	return (error);
}

/*
 * Destroy a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	if (STAILQ_FIRST(&map->bpages) != NULL || map->sync_count != 0) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d",
		    __func__, dmat, EBUSY);
		return (EBUSY);
	}
	if (dmat->bounce_zone)
		dmat->bounce_zone->map_count--;
	free(map, M_DEVBUF);
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
	busdma_bufalloc_t ba;
	struct busdma_bufzone *bufzone;
	vm_memattr_t memattr;
	int mflags;
	int mapsize;
	int error;

	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;

	/* ARM non-snooping caches need a map for the VA cache sync structure */

	mapsize = sizeof(**mapp) + (sizeof(struct sync_list) * dmat->nsegments);
	*mapp = (bus_dmamap_t)malloc(mapsize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (*mapp == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->flags, ENOMEM);
		return (ENOMEM);
	}

	(*mapp)->sync_count = 0;
	/* We may need bounce pages, even for allocated memory */
	error = allocate_bz_and_pages(dmat, *mapp);
	if (error != 0) {
		free(*mapp, M_DEVBUF);
		*mapp = NULL;
		return (error);
	}

	if (dmat->segments == NULL) {
		dmat->segments = (bus_dma_segment_t *)malloc(
		    sizeof(bus_dma_segment_t) * dmat->nsegments, M_DEVBUF,
		    mflags);
		if (dmat->segments == NULL) {
			CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
			    __func__, dmat, dmat->flags, ENOMEM);
			free(*mapp, M_DEVBUF);
			*mapp = NULL;
			return (ENOMEM);
		}
	}

	if (flags & BUS_DMA_ZERO)
		mflags |= M_ZERO;
	if (flags & BUS_DMA_COHERENT) {
		memattr = VM_MEMATTR_UNCACHEABLE;
		ba = coherent_allocator;
		(*mapp)->flags |= DMAMAP_COHERENT;
	} else {
		memattr = VM_MEMATTR_DEFAULT;
		ba = standard_allocator;
		(*mapp)->flags = 0;
	}

	/*
	 * Try to find a bufzone in the allocator that holds a cache of buffers
	 * of the right size for this request.  If the buffer is too big to be
	 * held in the allocator cache, this returns NULL.
	 */
	bufzone = busdma_bufalloc_findzone(ba, dmat->maxsize);

	/*
	 * Allocate the buffer from the uma(9) allocator if...
	 *  - It's small enough to be in the allocator (bufzone not NULL).
	 *  - The alignment constraint isn't larger than the allocation size
	 *    (the allocator aligns buffers to their size boundaries).
	 *  - There's no need to handle lowaddr/highaddr exclusion zones.
	 * else allocate non-contiguous pages if...
	 *  - The page count that could get allocated doesn't exceed nsegments.
	 *  - The alignment constraint isn't larger than a page boundary.
	 *  - There are no boundary-crossing constraints.
	 * else allocate a block of contiguous pages because one or more of the
	 * constraints is something that only the contig allocator can fulfill.
	 */
	if (bufzone != NULL && dmat->alignment <= bufzone->size &&
	    !_bus_dma_can_bounce(dmat->lowaddr, dmat->highaddr)) {
		*vaddr = uma_zalloc(bufzone->umazone, mflags);
	} else if (dmat->nsegments >= btoc(dmat->maxsize) &&
	    dmat->alignment <= PAGE_SIZE && dmat->boundary == 0) {
		*vaddr = (void *)kmem_alloc_attr(kernel_arena, dmat->maxsize,
		    mflags, 0, dmat->lowaddr, memattr);
	} else {
		*vaddr = (void *)kmem_alloc_contig(kernel_arena, dmat->maxsize,
		    mflags, 0, dmat->lowaddr, dmat->alignment, dmat->boundary,
		    memattr);
	}


	if (*vaddr == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->flags, ENOMEM);
		free(*mapp, M_DEVBUF);
		*mapp = NULL;
		return (ENOMEM);
	} else if ((uintptr_t)*vaddr & (dmat->alignment - 1)) {
		printf("bus_dmamem_alloc failed to align memory properly.\n");
	}
	dmat->map_count++;

	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
	    __func__, dmat, dmat->flags, 0);
	return (0);
}

/*
 * Free a piece of memory and it's allociated dmamap, that was allocated
 * via bus_dmamem_alloc.  Make the same choice for free/contigfree.
 */
void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
	struct busdma_bufzone *bufzone;
	busdma_bufalloc_t ba;

	if (map->flags & DMAMAP_COHERENT)
		ba = coherent_allocator;
	else
		ba = standard_allocator;

	/* Be careful not to access map from here on. */

	bufzone = busdma_bufalloc_findzone(ba, dmat->maxsize);

	if (bufzone != NULL && dmat->alignment <= bufzone->size &&
	    !_bus_dma_can_bounce(dmat->lowaddr, dmat->highaddr))
		uma_zfree(bufzone->umazone, vaddr);
	else
		kmem_free(kernel_arena, (vm_offset_t)vaddr, dmat->maxsize);

	dmat->map_count--;
	free(map, M_DEVBUF);
	CTR3(KTR_BUSDMA, "%s: tag %p flags 0x%x", __func__, dmat, dmat->flags);
}

static void
_bus_dmamap_count_phys(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
    bus_size_t buflen, int flags)
{
	bus_addr_t curaddr;
	bus_size_t sgsize;

	if (map->pagesneeded == 0) {
		CTR5(KTR_BUSDMA, "lowaddr= %d, boundary= %d, alignment= %d"
		    " map= %p, pagesneeded= %d",
		    dmat->lowaddr, dmat->boundary, dmat->alignment,
		    map, map->pagesneeded);
		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		curaddr = buf;
		while (buflen != 0) {
			sgsize = MIN(buflen, dmat->maxsegsz);
			if (run_filter(dmat, curaddr, sgsize, 
			    map->flags & DMAMAP_COHERENT) != 0) {
				sgsize = MIN(sgsize, PAGE_SIZE);
				map->pagesneeded++;
			}
			curaddr += sgsize;
			buflen -= sgsize;
		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d", map->pagesneeded);
	}
}

static void
_bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    void *buf, bus_size_t buflen, int flags)
{
	vm_offset_t vaddr;
	vm_offset_t vendaddr;
	bus_addr_t paddr;

	if (map->pagesneeded == 0) {
		CTR5(KTR_BUSDMA, "lowaddr= %d, boundary= %d, alignment= %d"
		    " map= %p, pagesneeded= %d",
		    dmat->lowaddr, dmat->boundary, dmat->alignment,
		    map, map->pagesneeded);
		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		vaddr = (vm_offset_t)buf;
		vendaddr = (vm_offset_t)buf + buflen;

		while (vaddr < vendaddr) {
			if (__predict_true(map->pmap == kernel_pmap))
				paddr = pmap_kextract(vaddr);
			else
				paddr = pmap_extract(map->pmap, vaddr);
			if (run_filter(dmat, paddr,
			    min(vendaddr - vaddr, 
			    (PAGE_SIZE - ((vm_offset_t)vaddr & PAGE_MASK))),
			    map->flags & DMAMAP_COHERENT) != 0) {
				map->pagesneeded++;
			}
			vaddr += (PAGE_SIZE - ((vm_offset_t)vaddr & PAGE_MASK));

		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d", map->pagesneeded);
	}
}

static int
_bus_dmamap_reserve_pages(bus_dma_tag_t dmat, bus_dmamap_t map, int flags)
{

	/* Reserve Necessary Bounce Pages */
	mtx_lock(&bounce_lock);
	if (flags & BUS_DMA_NOWAIT) {
		if (reserve_bounce_pages(dmat, map, 0) != 0) {
			map->pagesneeded = 0;
			mtx_unlock(&bounce_lock);
			return (ENOMEM);
		}
	} else {
		if (reserve_bounce_pages(dmat, map, 1) != 0) {
			/* Queue us for resources */
			STAILQ_INSERT_TAIL(&bounce_map_waitinglist, map, links);
			mtx_unlock(&bounce_lock);
			return (EINPROGRESS);
		}
	}
	mtx_unlock(&bounce_lock);

	return (0);
}

/*
 * Add a single contiguous physical range to the segment list.
 */
static int
_bus_dmamap_addseg(bus_dma_tag_t dmat, bus_dmamap_t map, bus_addr_t curaddr,
		   bus_size_t sgsize, bus_dma_segment_t *segs, int *segp)
{
	bus_addr_t baddr, bmask;
	int seg;

	/*
	 * Make sure we don't cross any boundaries.
	 */
	bmask = ~(dmat->boundary - 1);
	if (dmat->boundary > 0) {
		baddr = (curaddr + dmat->boundary) & bmask;
		if (sgsize > (baddr - curaddr))
			sgsize = (baddr - curaddr);
	}

	if (dmat->ranges) {
		struct arm32_dma_range *dr;

		dr = _bus_dma_inrange(dmat->ranges, dmat->_nranges,
		    curaddr);
		if (dr == NULL) {
			_bus_dmamap_unload(dmat, map);
			return (0);
		}
		/*
		 * In a valid DMA range.  Translate the physical
		 * memory address to an address in the DMA window.
		 */
		curaddr = (curaddr - dr->dr_sysbase) + dr->dr_busbase;
	}

	/*
	 * Insert chunk into a segment, coalescing with
	 * previous segment if possible.
	 */
	seg = *segp;
	if (seg == -1) {
		seg = 0;
		segs[seg].ds_addr = curaddr;
		segs[seg].ds_len = sgsize;
	} else {
		if (curaddr == segs[seg].ds_addr + segs[seg].ds_len &&
		    (segs[seg].ds_len + sgsize) <= dmat->maxsegsz &&
		    (dmat->boundary == 0 ||
		     (segs[seg].ds_addr & bmask) == (curaddr & bmask)))
			segs[seg].ds_len += sgsize;
		else {
			if (++seg >= dmat->nsegments)
				return (0);
			segs[seg].ds_addr = curaddr;
			segs[seg].ds_len = sgsize;
		}
	}
	*segp = seg;
	return (sgsize);
}

/*
 * Utility function to load a physical buffer.  segp contains
 * the starting segment on entrace, and the ending segment on exit.
 */
int
_bus_dmamap_load_phys(bus_dma_tag_t dmat,
		      bus_dmamap_t map,
		      vm_paddr_t buf, bus_size_t buflen,
		      int flags,
		      bus_dma_segment_t *segs,
		      int *segp)
{
	bus_addr_t curaddr;
	bus_size_t sgsize;
	int error;

	if (segs == NULL)
		segs = dmat->segments;

	if (((map->flags & DMAMAP_COHERENT) == 0) ||
	    (dmat->flags & BUS_DMA_COULD_BOUNCE) != 0) {
		_bus_dmamap_count_phys(dmat, map, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	while (buflen > 0) {
		curaddr = buf;
		sgsize = MIN(buflen, dmat->maxsegsz);
		if ((((map->flags & DMAMAP_COHERENT) == 0) ||
		    ((dmat->flags & BUS_DMA_COULD_BOUNCE) != 0)) &&
		    map->pagesneeded != 0 && run_filter(dmat, curaddr,
		    sgsize, map->flags & DMAMAP_COHERENT)) {
			sgsize = MIN(sgsize, PAGE_SIZE);
			curaddr = add_bounce_page(dmat, map, 0, curaddr,
						  sgsize);
		}
		sgsize = _bus_dmamap_addseg(dmat, map, curaddr, sgsize, segs,
		    segp);
		if (sgsize == 0)
			break;
		buf += sgsize;
		buflen -= sgsize;
	}

	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		_bus_dmamap_unload(dmat, map);
		return (EFBIG); /* XXX better return value here? */
	}
	return (0);
}

int
_bus_dmamap_load_ma(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct vm_page **ma, bus_size_t tlen, int ma_offs, int flags,
    bus_dma_segment_t *segs, int *segp)
{

	return (bus_dmamap_load_ma_triv(dmat, map, ma, tlen, ma_offs, flags,
	    segs, segp));
}

/*
 * Utility function to load a linear buffer.  segp contains
 * the starting segment on entrace, and the ending segment on exit.
 */
int
_bus_dmamap_load_buffer(bus_dma_tag_t dmat,
			bus_dmamap_t map,
			void *buf, bus_size_t buflen,
			pmap_t pmap,
			int flags,
			bus_dma_segment_t *segs,
			int *segp)
{
	bus_size_t sgsize;
	bus_addr_t curaddr;
	vm_offset_t vaddr;
	struct sync_list *sl;
	int error;

	if (segs == NULL)
		segs = dmat->segments;

	map->pmap = pmap;

	if (!(map->flags & DMAMAP_COHERENT) ||
	    (dmat->flags & BUS_DMA_COULD_BOUNCE) != 0) {
		_bus_dmamap_count_pages(dmat, map, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	sl = NULL;
	vaddr = (vm_offset_t)buf;

	while (buflen > 0) {
		/*
		 * Get the physical address for this segment.
		 */
		if (__predict_true(map->pmap == kernel_pmap))
			curaddr = pmap_kextract(vaddr);
		else
			curaddr = pmap_extract(map->pmap, vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (sgsize > dmat->maxsegsz)
			sgsize = dmat->maxsegsz;
		if (buflen < sgsize)
			sgsize = buflen;

		if ((((map->flags & DMAMAP_COHERENT) == 0) ||
		    ((dmat->flags & BUS_DMA_COULD_BOUNCE) != 0)) &&
		    map->pagesneeded != 0 && run_filter(dmat, curaddr,
		    sgsize, map->flags & DMAMAP_COHERENT)) {
			curaddr = add_bounce_page(dmat, map, vaddr, curaddr,
						  sgsize);
		} else {
			sl = &map->slist[map->sync_count - 1];
			if (map->sync_count == 0 ||
#ifdef ARM_L2_PIPT
			    curaddr != sl->busaddr + sl->datacount ||
#endif
			    vaddr != sl->vaddr + sl->datacount) {
				if (++map->sync_count > dmat->nsegments)
					goto cleanup;
				sl++;
				sl->vaddr = vaddr;
				sl->datacount = sgsize;
				sl->busaddr = curaddr;
			} else
				sl->datacount += sgsize;
		}
		sgsize = _bus_dmamap_addseg(dmat, map, curaddr, sgsize, segs,
					    segp);
		if (sgsize == 0)
			break;
		vaddr += sgsize;
		buflen -= sgsize;
	}

cleanup:
	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		_bus_dmamap_unload(dmat, map);
		return (EFBIG); /* XXX better return value here? */
	}
	return (0);
}


void
__bus_dmamap_waitok(bus_dma_tag_t dmat, bus_dmamap_t map,
		    struct memdesc *mem, bus_dmamap_callback_t *callback,
		    void *callback_arg)
{

	map->mem = *mem;
	map->dmat = dmat;
	map->callback = callback;
	map->callback_arg = callback_arg;
}

bus_dma_segment_t *
_bus_dmamap_complete(bus_dma_tag_t dmat, bus_dmamap_t map,
		     bus_dma_segment_t *segs, int nsegs, int error)
{

	if (segs == NULL)
		segs = dmat->segments;
	return (segs);
}

/*
 * Release the mapping held by map.
 */
void
_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	struct bounce_page *bpage;
	struct bounce_zone *bz;

	if ((bz = dmat->bounce_zone) != NULL) {
		while ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
			STAILQ_REMOVE_HEAD(&map->bpages, links);
			free_bounce_page(dmat, bpage);
		}

		bz = dmat->bounce_zone;
		bz->free_bpages += map->pagesreserved;
		bz->reserved_bpages -= map->pagesreserved;
		map->pagesreserved = 0;
		map->pagesneeded = 0;
	}
	map->sync_count = 0;
}

#ifdef notyetbounceuser
	/* If busdma uses user pages, then the interrupt handler could
	 * be use the kernel vm mapping. Both bounce pages and sync list
	 * do not cross page boundaries.
	 * Below is a rough sequence that a person would do to fix the
	 * user page reference in the kernel vmspace. This would be
	 * done in the dma post routine.
	 */
void
_bus_dmamap_fix_user(vm_offset_t buf, bus_size_t len,
			pmap_t pmap, int op)
{
	bus_size_t sgsize;
	bus_addr_t curaddr;
	vm_offset_t va;

		/* each synclist entry is contained within a single page.
		 *
		 * this would be needed if BUS_DMASYNC_POSTxxxx was implemented
		*/
	curaddr = pmap_extract(pmap, buf);
	va = pmap_dma_map(curaddr);
	switch (op) {
	case SYNC_USER_INV:
		cpu_dcache_wb_range(va, sgsize);
		break;

	case SYNC_USER_COPYTO:
		bcopy((void *)va, (void *)bounce, sgsize);
		break;

	case SYNC_USER_COPYFROM:
		bcopy((void *) bounce, (void *)va, sgsize);
		break;

	default:
		break;
	}

	pmap_dma_unmap(va);
}
#endif

#ifdef ARM_L2_PIPT
#define l2cache_wb_range(va, pa, size) cpu_l2cache_wb_range(pa, size)
#define l2cache_wbinv_range(va, pa, size) cpu_l2cache_wbinv_range(pa, size)
#define l2cache_inv_range(va, pa, size) cpu_l2cache_inv_range(pa, size)
#else
#define l2cache_wb_range(va, pa, size) cpu_l2cache_wb_range(va, size)
#define l2cache_wbinv_range(va, pa, size) cpu_l2cache_wbinv_range(va, size)
#define l2cache_inv_range(va, pa, size) cpu_l2cache_inv_range(va, size)
#endif

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct bounce_page *bpage;
	struct sync_list *sl, *end;
	/*
	 * If the buffer was from user space, it is possible that this is not
	 * the same vm map, especially on a POST operation.  It's not clear that
	 * dma on userland buffers can work at all right now, certainly not if a
	 * partial cacheline flush has to be handled.  To be safe, until we're
	 * able to test direct userland dma, panic on a map mismatch.
	 */
	if ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
		if (!pmap_dmap_iscurrent(map->pmap))
			panic("_bus_dmamap_sync: wrong user map for bounce sync.");
		/* Handle data bouncing. */
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x op 0x%x "
		    "performing bounce", __func__, dmat, dmat->flags, op);

		if (op & BUS_DMASYNC_PREWRITE) {
			while (bpage != NULL) {
				if (bpage->datavaddr != 0)
					bcopy((void *)bpage->datavaddr,
					      (void *)bpage->vaddr,
					      bpage->datacount);
				else
					physcopyout(bpage->dataaddr,
					      (void *)bpage->vaddr,
					      bpage->datacount);
				cpu_dcache_wb_range((vm_offset_t)bpage->vaddr,
					bpage->datacount);
				l2cache_wb_range((vm_offset_t)bpage->vaddr,
				    (vm_offset_t)bpage->busaddr, 
				    bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
			dmat->bounce_zone->total_bounced++;
		}

		if (op & BUS_DMASYNC_POSTREAD) {
			while (bpage != NULL) {
				vm_offset_t startv;
				vm_paddr_t startp;
				int len;

				startv = bpage->vaddr &~ arm_dcache_align_mask;
				startp = bpage->busaddr &~ arm_dcache_align_mask;
				len = bpage->datacount;
				
				if (startv != bpage->vaddr)
					len += bpage->vaddr & arm_dcache_align_mask;
				if (len & arm_dcache_align_mask) 
					len = (len -
					    (len & arm_dcache_align_mask)) +
					    arm_dcache_align;
				cpu_dcache_inv_range(startv, len);
				l2cache_inv_range(startv, startp, len);
				if (bpage->datavaddr != 0)
					bcopy((void *)bpage->vaddr,
					      (void *)bpage->datavaddr,
					      bpage->datacount);
				else
					physcopyin((void *)bpage->vaddr,
					      bpage->dataaddr,
					      bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
			dmat->bounce_zone->total_bounced++;
		}
	}
	if (map->flags & DMAMAP_COHERENT)
		return;

	if (map->sync_count != 0) {
		if (!pmap_dmap_iscurrent(map->pmap))
			panic("_bus_dmamap_sync: wrong user map for sync.");
		/* ARM caches are not self-snooping for dma */

		sl = &map->slist[0];
		end = &map->slist[map->sync_count];
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x op 0x%x "
		    "performing sync", __func__, dmat, dmat->flags, op);

		switch (op) {
		case BUS_DMASYNC_PREWRITE:
			while (sl != end) {
			    cpu_dcache_wb_range(sl->vaddr, sl->datacount);
			    l2cache_wb_range(sl->vaddr, sl->busaddr,
				sl->datacount);
			    sl++;
			}
			break;

		case BUS_DMASYNC_PREREAD:
			while (sl != end) {
				cpu_dcache_inv_range(sl->vaddr, sl->datacount);
				l2cache_inv_range(sl->vaddr, sl->busaddr, 
				    sl->datacount);
				sl++;
			}
			break;

		case BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD:
			while (sl != end) {
				cpu_dcache_wbinv_range(sl->vaddr, sl->datacount);
				l2cache_wbinv_range(sl->vaddr,
				    sl->busaddr, sl->datacount);
				sl++;
			}
			break;

		case BUS_DMASYNC_POSTREAD:
		case BUS_DMASYNC_POSTWRITE:
		case BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE:
			break;
		default:
			panic("unsupported combination of sync operations: 0x%08x\n", op);
			break;
		}
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
		bus_addr_t addr, bus_size_t size)
{
	struct bounce_zone *bz;
	struct bounce_page *bpage;

	KASSERT(dmat->bounce_zone != NULL, ("no bounce zone in dma tag"));
	KASSERT(map != NULL,
	    ("add_bounce_page: bad map %p", map));

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
	bpage->dataaddr = addr;
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
		bus_dmamap_load_mem(map->dmat, map, &map->mem, map->callback,
				    map->callback_arg, BUS_DMA_WAITOK);
		(dmat->lockfunc)(dmat->lockfuncarg, BUS_DMA_UNLOCK);
		mtx_lock(&bounce_lock);
	}
	mtx_unlock(&bounce_lock);
}
