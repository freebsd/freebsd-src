/*-
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 2015-2016 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship of the FreeBSD Foundation.
 *
 * Portions of this software were developed by Semihalf
 * under sponsorship of the FreeBSD Foundation.
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/domainset.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/md_var.h>
#include <arm64/include/bus_dma_impl.h>

#define MAX_BPAGES 4096

enum {
	BF_COULD_BOUNCE		= 0x01,
	BF_MIN_ALLOC_COMP	= 0x02,
	BF_KMEM_ALLOC		= 0x04,
	BF_COHERENT		= 0x10,
};

struct bounce_page;
struct bounce_zone;

struct bus_dma_tag {
	struct bus_dma_tag_common common;
	size_t			alloc_size;
	size_t			alloc_alignment;
	int			map_count;
	int			bounce_flags;
	bus_dma_segment_t	*segments;
	struct bounce_zone	*bounce_zone;
};

static SYSCTL_NODE(_hw, OID_AUTO, busdma, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Busdma parameters");

struct sync_list {
	vm_offset_t	vaddr;		/* kva of client data */
	bus_addr_t	paddr;		/* physical address */
	vm_page_t	pages;		/* starting page of client data */
	bus_size_t	datacount;	/* client data count */
};

struct bus_dmamap {
	STAILQ_HEAD(, bounce_page) bpages;
	int		       pagesneeded;
	int		       pagesreserved;
	bus_dma_tag_t	       dmat;
	struct memdesc	       mem;
	bus_dmamap_callback_t *callback;
	void		      *callback_arg;
	STAILQ_ENTRY(bus_dmamap) links;
	u_int			flags;
#define	DMAMAP_COHERENT		(1 << 0)
#define	DMAMAP_FROM_DMAMEM	(1 << 1)
#define	DMAMAP_MBUF		(1 << 2)
	int			sync_count;
	struct sync_list	slist[];
};

int run_filter(bus_dma_tag_t dmat, bus_addr_t paddr);
static bool _bus_dmamap_pagesneeded(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t buf, bus_size_t buflen, int *pagesneeded);
static void _bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    pmap_t pmap, void *buf, bus_size_t buflen, int flags);
static void _bus_dmamap_count_phys(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t buf, bus_size_t buflen, int flags);

static MALLOC_DEFINE(M_BUSDMA, "busdma", "busdma metadata");

#define	dmat_alignment(dmat)	((dmat)->common.alignment)
#define	dmat_domain(dmat)	((dmat)->common.domain)
#define	dmat_flags(dmat)	((dmat)->common.flags)
#define	dmat_lowaddr(dmat)	((dmat)->common.lowaddr)
#define	dmat_lockfunc(dmat)	((dmat)->common.lockfunc)
#define	dmat_lockfuncarg(dmat)	((dmat)->common.lockfuncarg)

#include "../../kern/subr_busdma_bounce.c"

static int
bounce_bus_dma_zone_setup(bus_dma_tag_t dmat)
{
	struct bounce_zone *bz;
	bus_size_t maxsize;
	int error;

	/*
	 * Round size up to a full page, and add one more page because
	 * there can always be one more boundary crossing than the
	 * number of pages in a transfer.
	 */
	maxsize = roundup2(dmat->common.maxsize, PAGE_SIZE) + PAGE_SIZE;

	/* Must bounce */
	if ((error = alloc_bounce_zone(dmat)) != 0)
		return (error);
	bz = dmat->bounce_zone;

	if (ptoa(bz->total_bpages) < maxsize) {
		int pages;

		pages = atop(maxsize) + 1 - bz->total_bpages;

		/* Add pages to our bounce pool */
		if (alloc_bounce_pages(dmat, pages) < pages)
			return (ENOMEM);
	}
	/* Performed initial allocation */
	dmat->bounce_flags |= BF_MIN_ALLOC_COMP;

	return (error);
}

/*
 * Return true if the DMA should bounce because the start or end does not fall
 * on a cacheline boundary (which would require a partial cacheline flush).
 * COHERENT memory doesn't trigger cacheline flushes.  Memory allocated by
 * bus_dmamem_alloc() is always aligned to cacheline boundaries, and there's a
 * strict rule that such memory cannot be accessed by the CPU while DMA is in
 * progress (or by multiple DMA engines at once), so that it's always safe to do
 * full cacheline flushes even if that affects memory outside the range of a
 * given DMA operation that doesn't involve the full allocated buffer.  If we're
 * mapping an mbuf, that follows the same rules as a buffer we allocated.
 */
static bool
cacheline_bounce(bus_dma_tag_t dmat, bus_dmamap_t map, bus_addr_t paddr,
    bus_size_t size)
{

#define	DMAMAP_CACHELINE_FLAGS						\
    (DMAMAP_FROM_DMAMEM | DMAMAP_COHERENT | DMAMAP_MBUF)
	if ((dmat->bounce_flags & BF_COHERENT) != 0)
		return (false);
	if (map != NULL && (map->flags & DMAMAP_CACHELINE_FLAGS) != 0)
		return (false);
	return (((paddr | size) & (dcache_line_size - 1)) != 0);
#undef DMAMAP_CACHELINE_FLAGS
}

/*
 * Return true if the given address does not fall on the alignment boundary.
 */
static bool
alignment_bounce(bus_dma_tag_t dmat, bus_addr_t addr)
{

	return (!vm_addr_align_ok(addr, dmat->common.alignment));
}

static bool
might_bounce(bus_dma_tag_t dmat, bus_dmamap_t map, bus_addr_t paddr,
    bus_size_t size)
{

	/* Memory allocated by bounce_bus_dmamem_alloc won't bounce */
	if (map && (map->flags & DMAMAP_FROM_DMAMEM) != 0)
		return (false);

	if ((dmat->bounce_flags & BF_COULD_BOUNCE) != 0)
		return (true);

	if (cacheline_bounce(dmat, map, paddr, size))
		return (true);

	if (alignment_bounce(dmat, paddr))
		return (true);

	return (false);
}

static bool
must_bounce(bus_dma_tag_t dmat, bus_dmamap_t map, bus_addr_t paddr,
    bus_size_t size)
{

	if (cacheline_bounce(dmat, map, paddr, size))
		return (true);

	if (alignment_bounce(dmat, paddr))
		return (true);

	if ((dmat->bounce_flags & BF_COULD_BOUNCE) != 0 &&
	    bus_dma_run_filter(&dmat->common, paddr))
		return (true);

	return (false);
}

/*
 * Allocate a device specific dma_tag.
 */
static int
bounce_bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
    bus_addr_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_dma_filter_t *filter, void *filterarg, bus_size_t maxsize,
    int nsegments, bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
    void *lockfuncarg, bus_dma_tag_t *dmat)
{
	bus_dma_tag_t newtag;
	int error;

	*dmat = NULL;
	error = common_bus_dma_tag_create(parent != NULL ? &parent->common :
	    NULL, alignment, boundary, lowaddr, highaddr, filter, filterarg,
	    maxsize, nsegments, maxsegsz, flags, lockfunc, lockfuncarg,
	    sizeof (struct bus_dma_tag), (void **)&newtag);
	if (error != 0)
		return (error);

	newtag->common.impl = &bus_dma_bounce_impl;
	newtag->map_count = 0;
	newtag->segments = NULL;

	if ((flags & BUS_DMA_COHERENT) != 0) {
		newtag->bounce_flags |= BF_COHERENT;
	}

	if (parent != NULL) {
		if ((newtag->common.filter != NULL ||
		    (parent->bounce_flags & BF_COULD_BOUNCE) != 0))
			newtag->bounce_flags |= BF_COULD_BOUNCE;

		/* Copy some flags from the parent */
		newtag->bounce_flags |= parent->bounce_flags & BF_COHERENT;
	}

	if ((newtag->bounce_flags & BF_COHERENT) != 0) {
		newtag->alloc_alignment = newtag->common.alignment;
		newtag->alloc_size = newtag->common.maxsize;
	} else {
		/*
		 * Ensure the buffer is aligned to a cacheline when allocating
		 * a non-coherent buffer. This is so we don't have any data
		 * that another CPU may be accessing around DMA buffer
		 * causing the cache to become dirty.
		 */
		newtag->alloc_alignment = MAX(newtag->common.alignment,
		    dcache_line_size);
		newtag->alloc_size = roundup2(newtag->common.maxsize,
		    dcache_line_size);
	}

	if (newtag->common.lowaddr < ptoa((vm_paddr_t)Maxmem) ||
	    newtag->common.alignment > 1)
		newtag->bounce_flags |= BF_COULD_BOUNCE;

	if ((flags & BUS_DMA_ALLOCNOW) != 0)
		error = bounce_bus_dma_zone_setup(newtag);
	else
		error = 0;

	if (error != 0)
		free(newtag, M_DEVBUF);
	else
		*dmat = newtag;
	CTR4(KTR_BUSDMA, "%s returned tag %p tag flags 0x%x error %d",
	    __func__, newtag, (newtag != NULL ? newtag->common.flags : 0),
	    error);
	return (error);
}

static int
bounce_bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
#ifdef KTR
	bus_dma_tag_t dmat_copy;
#endif
	bus_dma_tag_t parent;
	int error;

	error = 0;
#ifdef KTR
	dmat_copy = dmat;
#endif


	if (dmat != NULL) {
		if (dmat->map_count != 0) {
			error = EBUSY;
			goto out;
		}
		while (dmat != NULL) {
			parent = (bus_dma_tag_t)dmat->common.parent;
			atomic_subtract_int(&dmat->common.ref_count, 1);
			if (dmat->common.ref_count == 0) {
				if (dmat->segments != NULL)
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

/*
 * Update the domain for the tag.  We may need to reallocate the zone and
 * bounce pages.
 */
static int
bounce_bus_dma_tag_set_domain(bus_dma_tag_t dmat)
{

	KASSERT(dmat->map_count == 0,
	    ("bounce_bus_dma_tag_set_domain:  Domain set after use.\n"));
	if ((dmat->bounce_flags & BF_COULD_BOUNCE) == 0 ||
	    dmat->bounce_zone == NULL)
		return (0);
	dmat->bounce_flags &= ~BF_MIN_ALLOC_COMP;
	return (bounce_bus_dma_zone_setup(dmat));
}

static bool
bounce_bus_dma_id_mapped(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen)
{

	if (!might_bounce(dmat, NULL, buf, buflen))
		return (true);
	return (!_bus_dmamap_pagesneeded(dmat, NULL, buf, buflen, NULL));
}

static bus_dmamap_t
alloc_dmamap(bus_dma_tag_t dmat, int flags)
{
	u_long mapsize;
	bus_dmamap_t map;

	mapsize = sizeof(*map);
	mapsize += sizeof(struct sync_list) * dmat->common.nsegments;
	map = malloc_domainset(mapsize, M_DEVBUF,
	    DOMAINSET_PREF(dmat->common.domain), flags | M_ZERO);
	if (map == NULL)
		return (NULL);

	/* Initialize the new map */
	STAILQ_INIT(&map->bpages);

	return (map);
}

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
static int
bounce_bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	struct bounce_zone *bz;
	int error, maxpages, pages;

	error = 0;

	if (dmat->segments == NULL) {
		dmat->segments = mallocarray_domainset(dmat->common.nsegments,
		    sizeof(bus_dma_segment_t), M_DEVBUF,
		    DOMAINSET_PREF(dmat->common.domain), M_NOWAIT);
		if (dmat->segments == NULL) {
			CTR3(KTR_BUSDMA, "%s: tag %p error %d",
			    __func__, dmat, ENOMEM);
			return (ENOMEM);
		}
	}

	*mapp = alloc_dmamap(dmat, M_NOWAIT);
	if (*mapp == NULL) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d",
		    __func__, dmat, ENOMEM);
		return (ENOMEM);
	}

	/*
	 * Bouncing might be required if the driver asks for an active
	 * exclusion region, a data alignment that is stricter than 1, and/or
	 * an active address boundary.
	 */
	if (dmat->bounce_zone == NULL) {
		if ((error = alloc_bounce_zone(dmat)) != 0) {
			free(*mapp, M_DEVBUF);
			return (error);
		}
	}
	bz = dmat->bounce_zone;

	/*
	 * Attempt to add pages to our pool on a per-instance basis up to a sane
	 * limit. Even if the tag isn't subject of bouncing due to alignment
	 * and boundary constraints, it could still auto-bounce due to
	 * cacheline alignment, which requires at most two bounce pages.
	 */
	if (dmat->common.alignment > 1)
		maxpages = MAX_BPAGES;
	else
		maxpages = MIN(MAX_BPAGES, Maxmem -
		    atop(dmat->common.lowaddr));
	if ((dmat->bounce_flags & BF_MIN_ALLOC_COMP) == 0 ||
	    (bz->map_count > 0 && bz->total_bpages < maxpages)) {
		pages = atop(roundup2(dmat->common.maxsize, PAGE_SIZE)) + 1;
		pages = MIN(maxpages - bz->total_bpages, pages);
		pages = MAX(pages, 2);
		if (alloc_bounce_pages(dmat, pages) < pages)
			error = ENOMEM;
		if ((dmat->bounce_flags & BF_MIN_ALLOC_COMP) == 0) {
			if (error == 0) {
				dmat->bounce_flags |= BF_MIN_ALLOC_COMP;
			}
		} else
			error = 0;
	}
	bz->map_count++;

	if (error == 0) {
		dmat->map_count++;
		if ((dmat->bounce_flags & BF_COHERENT) != 0)
			(*mapp)->flags |= DMAMAP_COHERENT;
	} else {
		free(*mapp, M_DEVBUF);
	}
	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
	    __func__, dmat, dmat->common.flags, error);
	return (error);
}

/*
 * Destroy a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
static int
bounce_bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	/* Check we are destroying the correct map type */
	if ((map->flags & DMAMAP_FROM_DMAMEM) != 0)
		panic("bounce_bus_dmamap_destroy: Invalid map freed\n");

	if (STAILQ_FIRST(&map->bpages) != NULL || map->sync_count != 0) {
		CTR3(KTR_BUSDMA, "%s: tag %p error %d", __func__, dmat, EBUSY);
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
static int
bounce_bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
    bus_dmamap_t *mapp)
{
	vm_memattr_t attr;
	int mflags;

	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;

	if (dmat->segments == NULL) {
		dmat->segments = mallocarray_domainset(dmat->common.nsegments,
		    sizeof(bus_dma_segment_t), M_DEVBUF,
		    DOMAINSET_PREF(dmat->common.domain), mflags);
		if (dmat->segments == NULL) {
			CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
			    __func__, dmat, dmat->common.flags, ENOMEM);
			return (ENOMEM);
		}
	}
	if (flags & BUS_DMA_ZERO)
		mflags |= M_ZERO;
	if (flags & BUS_DMA_NOCACHE)
		attr = VM_MEMATTR_UNCACHEABLE;
	else if ((flags & BUS_DMA_COHERENT) != 0 &&
	    (dmat->bounce_flags & BF_COHERENT) == 0)
		/*
		 * If we have a non-coherent tag, and are trying to allocate
		 * a coherent block of memory it needs to be uncached.
		 */
		attr = VM_MEMATTR_UNCACHEABLE;
	else
		attr = VM_MEMATTR_DEFAULT;

	/*
	 * Create the map, but don't set the could bounce flag as
	 * this allocation should never bounce;
	 */
	*mapp = alloc_dmamap(dmat, mflags);
	if (*mapp == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->common.flags, ENOMEM);
		return (ENOMEM);
	}

	/*
	 * Mark the map as coherent if we used uncacheable memory or the
	 * tag was already marked as coherent.
	 */
	if (attr == VM_MEMATTR_UNCACHEABLE ||
	    (dmat->bounce_flags & BF_COHERENT) != 0)
		(*mapp)->flags |= DMAMAP_COHERENT;

	(*mapp)->flags |= DMAMAP_FROM_DMAMEM;

	/*
	 * Allocate the buffer from the malloc(9) allocator if...
	 *  - It's small enough to fit into a single page.
	 *  - Its alignment requirement is also smaller than the page size.
	 *  - The low address requirement is fulfilled.
	 *  - Default cache attributes are requested (WB).
	 * else allocate non-contiguous pages if...
	 *  - The page count that could get allocated doesn't exceed
	 *    nsegments also when the maximum segment size is less
	 *    than PAGE_SIZE.
	 *  - The alignment constraint isn't larger than a page boundary.
	 *  - There are no boundary-crossing constraints.
	 * else allocate a block of contiguous pages because one or more of the
	 * constraints is something that only the contig allocator can fulfill.
	 *
	 * NOTE: The (dmat->common.alignment <= dmat->maxsize) check
	 * below is just a quick hack. The exact alignment guarantees
	 * of malloc(9) need to be nailed down, and the code below
	 * should be rewritten to take that into account.
	 *
	 * In the meantime warn the user if malloc gets it wrong.
	 */
	if (dmat->alloc_size <= PAGE_SIZE &&
	    dmat->alloc_alignment <= PAGE_SIZE &&
	    dmat->common.lowaddr >= ptoa((vm_paddr_t)Maxmem) &&
	    attr == VM_MEMATTR_DEFAULT) {
		*vaddr = malloc_domainset_aligned(dmat->alloc_size,
		    dmat->alloc_alignment, M_DEVBUF,
		    DOMAINSET_PREF(dmat->common.domain), mflags);
	} else if (dmat->common.nsegments >=
	    howmany(dmat->alloc_size, MIN(dmat->common.maxsegsz, PAGE_SIZE)) &&
	    dmat->alloc_alignment <= PAGE_SIZE &&
	    (dmat->common.boundary % PAGE_SIZE) == 0) {
		/* Page-based multi-segment allocations allowed */
		*vaddr = kmem_alloc_attr_domainset(
		    DOMAINSET_PREF(dmat->common.domain), dmat->alloc_size,
		    mflags, 0ul, dmat->common.lowaddr, attr);
		dmat->bounce_flags |= BF_KMEM_ALLOC;
	} else {
		*vaddr = kmem_alloc_contig_domainset(
		    DOMAINSET_PREF(dmat->common.domain), dmat->alloc_size,
		    mflags, 0ul, dmat->common.lowaddr,
		    dmat->alloc_alignment != 0 ? dmat->alloc_alignment : 1ul,
		    dmat->common.boundary, attr);
		dmat->bounce_flags |= BF_KMEM_ALLOC;
	}
	if (*vaddr == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->common.flags, ENOMEM);
		free(*mapp, M_DEVBUF);
		return (ENOMEM);
	} else if (!vm_addr_align_ok(vtophys(*vaddr), dmat->alloc_alignment)) {
		printf("bus_dmamem_alloc failed to align memory properly.\n");
	}
	dmat->map_count++;
	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
	    __func__, dmat, dmat->common.flags, 0);
	return (0);
}

/*
 * Free a piece of memory and it's allociated dmamap, that was allocated
 * via bus_dmamem_alloc.  Make the same choice for free/contigfree.
 */
static void
bounce_bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{

	/*
	 * Check the map came from bounce_bus_dmamem_alloc, so the map
	 * should be NULL and the BF_KMEM_ALLOC flag cleared if malloc()
	 * was used and set if kmem_alloc_contig() was used.
	 */
	if ((map->flags & DMAMAP_FROM_DMAMEM) == 0)
		panic("bus_dmamem_free: Invalid map freed\n");
	if ((dmat->bounce_flags & BF_KMEM_ALLOC) == 0)
		free(vaddr, M_DEVBUF);
	else
		kmem_free(vaddr, dmat->alloc_size);
	free(map, M_DEVBUF);
	dmat->map_count--;
	CTR3(KTR_BUSDMA, "%s: tag %p flags 0x%x", __func__, dmat,
	    dmat->bounce_flags);
}

static bool
_bus_dmamap_pagesneeded(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
    bus_size_t buflen, int *pagesneeded)
{
	bus_addr_t curaddr;
	bus_size_t sgsize;
	int count;

	/*
	 * Count the number of bounce pages needed in order to
	 * complete this transfer
	 */
	count = 0;
	curaddr = buf;
	while (buflen != 0) {
		sgsize = MIN(buflen, dmat->common.maxsegsz);
		if (must_bounce(dmat, map, curaddr, sgsize)) {
			sgsize = MIN(sgsize,
			    PAGE_SIZE - (curaddr & PAGE_MASK));
			if (pagesneeded == NULL)
				return (true);
			count++;
		}
		curaddr += sgsize;
		buflen -= sgsize;
	}

	if (pagesneeded != NULL)
		*pagesneeded = count;
	return (count != 0);
}

static void
_bus_dmamap_count_phys(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
    bus_size_t buflen, int flags)
{

	if (map->pagesneeded == 0) {
		_bus_dmamap_pagesneeded(dmat, map, buf, buflen,
		    &map->pagesneeded);
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}
}

static void
_bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map, pmap_t pmap,
    void *buf, bus_size_t buflen, int flags)
{
	vm_offset_t vaddr;
	vm_offset_t vendaddr;
	bus_addr_t paddr;
	bus_size_t sg_len;

	if (map->pagesneeded == 0) {
		CTR4(KTR_BUSDMA, "lowaddr= %d Maxmem= %d, boundary= %d, "
		    "alignment= %d", dmat->common.lowaddr,
		    ptoa((vm_paddr_t)Maxmem),
		    dmat->common.boundary, dmat->common.alignment);
		CTR2(KTR_BUSDMA, "map= %p, pagesneeded= %d", map,
		    map->pagesneeded);
		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		vaddr = (vm_offset_t)buf;
		vendaddr = (vm_offset_t)buf + buflen;

		while (vaddr < vendaddr) {
			sg_len = PAGE_SIZE - ((vm_offset_t)vaddr & PAGE_MASK);
			if (pmap == kernel_pmap)
				paddr = pmap_kextract(vaddr);
			else
				paddr = pmap_extract(pmap, vaddr);
			if (must_bounce(dmat, map, paddr,
			    min(vendaddr - vaddr, (PAGE_SIZE - ((vm_offset_t)vaddr &
			    PAGE_MASK)))) != 0) {
				sg_len = roundup2(sg_len,
				    dmat->common.alignment);
				map->pagesneeded++;
			}
			vaddr += sg_len;
		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}
}

/*
 * Add a single contiguous physical range to the segment list.
 */
static bus_size_t
_bus_dmamap_addseg(bus_dma_tag_t dmat, bus_dmamap_t map, bus_addr_t curaddr,
    bus_size_t sgsize, bus_dma_segment_t *segs, int *segp)
{
	int seg;

	/*
	 * Make sure we don't cross any boundaries.
	 */
	if (!vm_addr_bound_ok(curaddr, sgsize, dmat->common.boundary))
		sgsize = roundup2(curaddr, dmat->common.boundary) - curaddr;

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
		    (segs[seg].ds_len + sgsize) <= dmat->common.maxsegsz &&
		    vm_addr_bound_ok(segs[seg].ds_addr,
		    segs[seg].ds_len + sgsize, dmat->common.boundary))
			segs[seg].ds_len += sgsize;
		else {
			if (++seg >= dmat->common.nsegments)
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
static int
bounce_bus_dmamap_load_phys(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t buf, bus_size_t buflen, int flags, bus_dma_segment_t *segs,
    int *segp)
{
	struct sync_list *sl;
	bus_size_t sgsize;
	bus_addr_t curaddr, sl_end;
	int error;

	if (segs == NULL)
		segs = dmat->segments;

	if (might_bounce(dmat, map, (bus_addr_t)buf, buflen)) {
		_bus_dmamap_count_phys(dmat, map, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	sl = map->slist + map->sync_count - 1;
	sl_end = 0;

	while (buflen > 0) {
		curaddr = buf;
		sgsize = MIN(buflen, dmat->common.maxsegsz);
		if (map->pagesneeded != 0 &&
		    must_bounce(dmat, map, curaddr, sgsize)) {
			/*
			 * The attempt to split a physically continuous buffer
			 * seems very controversial, it's unclear whether we
			 * can do this in all cases. Also, memory for bounced
			 * buffers is allocated as pages, so we cannot
			 * guarantee multipage alignment.
			 */
			KASSERT(dmat->common.alignment <= PAGE_SIZE,
			    ("bounced buffer cannot have alignment bigger "
			    "than PAGE_SIZE: %lu", dmat->common.alignment));
			sgsize = MIN(sgsize, PAGE_SIZE - (curaddr & PAGE_MASK));
			curaddr = add_bounce_page(dmat, map, 0, curaddr,
			    sgsize);
		} else if ((map->flags & DMAMAP_COHERENT) == 0) {
			if (map->sync_count > 0)
				sl_end = sl->paddr + sl->datacount;

			if (map->sync_count == 0 || curaddr != sl_end) {
				if (++map->sync_count > dmat->common.nsegments)
					break;
				sl++;
				sl->vaddr = 0;
				sl->paddr = curaddr;
				sl->pages = PHYS_TO_VM_PAGE(curaddr);
				KASSERT(sl->pages != NULL,
				    ("%s: page at PA:0x%08lx is not in "
				    "vm_page_array", __func__, curaddr));
				sl->datacount = sgsize;
			} else
				sl->datacount += sgsize;
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
		bus_dmamap_unload(dmat, map);
		return (EFBIG); /* XXX better return value here? */
	}
	return (0);
}

/*
 * Utility function to load a linear buffer.  segp contains
 * the starting segment on entrace, and the ending segment on exit.
 */
static int
bounce_bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, pmap_t pmap, int flags, bus_dma_segment_t *segs,
    int *segp)
{
	struct sync_list *sl;
	bus_size_t sgsize;
	bus_addr_t curaddr, sl_pend;
	vm_offset_t kvaddr, vaddr, sl_vend;
	int error;

	KASSERT((map->flags & DMAMAP_FROM_DMAMEM) != 0 ||
	    dmat->common.alignment <= PAGE_SIZE,
	    ("loading user buffer with alignment bigger than PAGE_SIZE is not "
	    "supported"));

	if (segs == NULL)
		segs = dmat->segments;

	if (flags & BUS_DMA_LOAD_MBUF)
		map->flags |= DMAMAP_MBUF;

	if (might_bounce(dmat, map, (bus_addr_t)buf, buflen)) {
		_bus_dmamap_count_pages(dmat, map, pmap, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	/*
	 * XXX Optimally we should parse input buffer for physically
	 * continuous segments first and then pass these segment into
	 * load loop.
	 */
	sl = map->slist + map->sync_count - 1;
	vaddr = (vm_offset_t)buf;
	sl_pend = 0;
	sl_vend = 0;

	while (buflen > 0) {
		/*
		 * Get the physical address for this segment.
		 */
		if (__predict_true(pmap == kernel_pmap)) {
			curaddr = pmap_kextract(vaddr);
			kvaddr = vaddr;
		} else {
			curaddr = pmap_extract(pmap, vaddr);
			kvaddr = 0;
		}

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = MIN(buflen, dmat->common.maxsegsz);
		if ((map->flags & DMAMAP_FROM_DMAMEM) == 0)
			sgsize = MIN(sgsize, PAGE_SIZE - (curaddr & PAGE_MASK));

		if (map->pagesneeded != 0 &&
		    must_bounce(dmat, map, curaddr, sgsize)) {
			/* See comment in bounce_bus_dmamap_load_phys */
			KASSERT(dmat->common.alignment <= PAGE_SIZE,
			    ("bounced buffer cannot have alignment bigger "
			    "than PAGE_SIZE: %lu", dmat->common.alignment));
			curaddr = add_bounce_page(dmat, map, kvaddr, curaddr,
			    sgsize);
		} else if ((map->flags & DMAMAP_COHERENT) == 0) {
			if (map->sync_count > 0) {
				sl_pend = sl->paddr + sl->datacount;
				sl_vend = sl->vaddr + sl->datacount;
			}

			if (map->sync_count == 0 ||
			    (kvaddr != 0 && kvaddr != sl_vend) ||
			    (curaddr != sl_pend)) {
				if (++map->sync_count > dmat->common.nsegments)
					break;
				sl++;
				sl->vaddr = kvaddr;
				sl->paddr = curaddr;
				if (kvaddr != 0) {
					sl->pages = NULL;
				} else {
					sl->pages = PHYS_TO_VM_PAGE(curaddr);
					KASSERT(sl->pages != NULL,
					    ("%s: page at PA:0x%08lx is not "
					    "in vm_page_array", __func__,
					    curaddr));
				}
				sl->datacount = sgsize;
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

	/*
	 * Did we fit?
	 */
	if (buflen != 0) {
		bus_dmamap_unload(dmat, map);
		return (EFBIG); /* XXX better return value here? */
	}
	return (0);
}

static void
bounce_bus_dmamap_waitok(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct memdesc *mem, bus_dmamap_callback_t *callback, void *callback_arg)
{

	map->mem = *mem;
	map->dmat = dmat;
	map->callback = callback;
	map->callback_arg = callback_arg;
}

static bus_dma_segment_t *
bounce_bus_dmamap_complete(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, int error)
{

	if (segs == NULL)
		segs = dmat->segments;
	return (segs);
}

/*
 * Release the mapping held by map.
 */
static void
bounce_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	free_bounce_pages(dmat, map);
	map->sync_count = 0;
	map->flags &= ~DMAMAP_MBUF;
}

static void
dma_preread_safe(vm_offset_t va, vm_size_t size)
{
	/*
	 * Write back any partial cachelines immediately before and
	 * after the DMA region.
	 */
	if (va & (dcache_line_size - 1))
		cpu_dcache_wb_range(va, 1);
	if ((va + size) & (dcache_line_size - 1))
		cpu_dcache_wb_range(va + size, 1);

	cpu_dcache_inv_range(va, size);
}

static void
dma_dcache_sync(struct sync_list *sl, bus_dmasync_op_t op)
{
	uint32_t len, offset;
	vm_page_t m;
	vm_paddr_t pa;
	vm_offset_t va, tempva;
	bus_size_t size;

	offset = sl->paddr & PAGE_MASK;
	m = sl->pages;
	size = sl->datacount;
	pa = sl->paddr;

	for ( ; size != 0; size -= len, pa += len, offset = 0, ++m) {
		tempva = 0;
		if (sl->vaddr == 0) {
			len = min(PAGE_SIZE - offset, size);
			tempva = pmap_quick_enter_page(m);
			va = tempva | offset;
			KASSERT(pa == (VM_PAGE_TO_PHYS(m) | offset),
			    ("unexpected vm_page_t phys: 0x%16lx != 0x%16lx",
			    VM_PAGE_TO_PHYS(m) | offset, pa));
		} else {
			len = sl->datacount;
			va = sl->vaddr;
		}

		switch (op) {
		case BUS_DMASYNC_PREWRITE:
		case BUS_DMASYNC_PREWRITE | BUS_DMASYNC_PREREAD:
			cpu_dcache_wb_range(va, len);
			break;
		case BUS_DMASYNC_PREREAD:
			/*
			 * An mbuf may start in the middle of a cacheline. There
			 * will be no cpu writes to the beginning of that line
			 * (which contains the mbuf header) while dma is in
			 * progress.  Handle that case by doing a writeback of
			 * just the first cacheline before invalidating the
			 * overall buffer.  Any mbuf in a chain may have this
			 * misalignment.  Buffers which are not mbufs bounce if
			 * they are not aligned to a cacheline.
			 */
			dma_preread_safe(va, len);
			break;
		case BUS_DMASYNC_POSTREAD:
		case BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE:
			cpu_dcache_inv_range(va, len);
			break;
		default:
			panic("unsupported combination of sync operations: "
                              "0x%08x\n", op);
		}

		if (tempva != 0)
			pmap_quick_remove_page(tempva);
	}
}

static void
bounce_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{
	struct bounce_page *bpage;
	struct sync_list *sl, *end;
	vm_offset_t datavaddr, tempvaddr;

	if (op == BUS_DMASYNC_POSTWRITE)
		return;

	if ((op & BUS_DMASYNC_POSTREAD) != 0) {
		/*
		 * Wait for any DMA operations to complete before the bcopy.
		 */
		dsb(sy);
	}

	if ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x op 0x%x "
		    "performing bounce", __func__, dmat, dmat->common.flags,
		    op);

		if ((op & BUS_DMASYNC_PREWRITE) != 0) {
			while (bpage != NULL) {
				tempvaddr = 0;
				datavaddr = bpage->datavaddr;
				if (datavaddr == 0) {
					tempvaddr = pmap_quick_enter_page(
					    bpage->datapage);
					datavaddr = tempvaddr | bpage->dataoffs;
				}

				bcopy((void *)datavaddr,
				    (void *)bpage->vaddr, bpage->datacount);
				if (tempvaddr != 0)
					pmap_quick_remove_page(tempvaddr);
				if ((map->flags & DMAMAP_COHERENT) == 0)
					cpu_dcache_wb_range(bpage->vaddr,
					    bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
			dmat->bounce_zone->total_bounced++;
		} else if ((op & BUS_DMASYNC_PREREAD) != 0) {
			while (bpage != NULL) {
				if ((map->flags & DMAMAP_COHERENT) == 0)
					cpu_dcache_wbinv_range(bpage->vaddr,
					    bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
		}

		if ((op & BUS_DMASYNC_POSTREAD) != 0) {
			while (bpage != NULL) {
				if ((map->flags & DMAMAP_COHERENT) == 0)
					cpu_dcache_inv_range(bpage->vaddr,
					    bpage->datacount);
				tempvaddr = 0;
				datavaddr = bpage->datavaddr;
				if (datavaddr == 0) {
					tempvaddr = pmap_quick_enter_page(
					    bpage->datapage);
					datavaddr = tempvaddr | bpage->dataoffs;
				}

				bcopy((void *)bpage->vaddr,
				    (void *)datavaddr, bpage->datacount);

				if (tempvaddr != 0)
					pmap_quick_remove_page(tempvaddr);
				bpage = STAILQ_NEXT(bpage, links);
			}
			dmat->bounce_zone->total_bounced++;
		}
	}

	/*
	 * Cache maintenance for normal (non-COHERENT non-bounce) buffers.
	 */
	if (map->sync_count != 0) {
		sl = &map->slist[0];
		end = &map->slist[map->sync_count];
		CTR3(KTR_BUSDMA, "%s: tag %p op 0x%x "
		    "performing sync", __func__, dmat, op);

		for ( ; sl != end; ++sl)
			dma_dcache_sync(sl, op);
	}

	if ((op & (BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE)) != 0) {
		/*
		 * Wait for the bcopy to complete before any DMA operations.
		 */
		dsb(sy);
	}
}

struct bus_dma_impl bus_dma_bounce_impl = {
	.tag_create = bounce_bus_dma_tag_create,
	.tag_destroy = bounce_bus_dma_tag_destroy,
	.tag_set_domain = bounce_bus_dma_tag_set_domain,
	.id_mapped = bounce_bus_dma_id_mapped,
	.map_create = bounce_bus_dmamap_create,
	.map_destroy = bounce_bus_dmamap_destroy,
	.mem_alloc = bounce_bus_dmamem_alloc,
	.mem_free = bounce_bus_dmamem_free,
	.load_phys = bounce_bus_dmamap_load_phys,
	.load_buffer = bounce_bus_dmamap_load_buffer,
	.load_ma = bus_dmamap_load_ma_triv,
	.map_waitok = bounce_bus_dmamap_waitok,
	.map_complete = bounce_bus_dmamap_complete,
	.map_unload = bounce_bus_dmamap_unload,
	.map_sync = bounce_bus_dmamap_sync
};
