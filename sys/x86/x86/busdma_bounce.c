/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

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
#include <sys/msan.h>
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
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>

#ifdef __i386__
#define MAX_BPAGES (Maxmem > atop(0x100000000ULL) ? 8192 : 512)
#else
#define MAX_BPAGES 8192
#endif

enum {
	BUS_DMA_COULD_BOUNCE	= 0x01,
	BUS_DMA_MIN_ALLOC_COMP	= 0x02,
	BUS_DMA_KMEM_ALLOC	= 0x04,
	BUS_DMA_FORCE_MAP	= 0x08,
};

struct bounce_page;
struct bounce_zone;

struct bus_dma_tag {
	struct bus_dma_tag_common common;
	int			map_count;
	int			bounce_flags;
	bus_dma_segment_t	*segments;
	struct bounce_zone	*bounce_zone;
};

static SYSCTL_NODE(_hw, OID_AUTO, busdma, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
    "Busdma parameters");

struct bus_dmamap {
	STAILQ_HEAD(, bounce_page) bpages;
	int		       pagesneeded;
	int		       pagesreserved;
	bus_dma_tag_t	       dmat;
	struct memdesc	       mem;
	bus_dmamap_callback_t *callback;
	void		      *callback_arg;
	__sbintime_t	       queued_time;
	STAILQ_ENTRY(bus_dmamap) links;
#ifdef KMSAN
	struct memdesc	       kmsan_mem;
#endif
};

static struct bus_dmamap nobounce_dmamap;

static bool _bus_dmamap_pagesneeded(bus_dma_tag_t dmat, vm_paddr_t buf,
    bus_size_t buflen, int *pagesneeded);
static void _bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    pmap_t pmap, void *buf, bus_size_t buflen, int flags);
static void _bus_dmamap_count_phys(bus_dma_tag_t dmat, bus_dmamap_t map,
    vm_paddr_t buf, bus_size_t buflen, int flags);

static MALLOC_DEFINE(M_BUSDMA, "busdma", "busdma metadata");

#define	dmat_alignment(dmat)	((dmat)->common.alignment)
#define	dmat_bounce_flags(dmat)	((dmat)->bounce_flags)
#define	dmat_boundary(dmat)	((dmat)->common.boundary)
#define	dmat_domain(dmat)	((dmat)->common.domain)
#define	dmat_flags(dmat)	((dmat)->common.flags)
#define	dmat_highaddr(dmat)	((dmat)->common.highaddr)
#define	dmat_lowaddr(dmat)	((dmat)->common.lowaddr)
#define	dmat_lockfunc(dmat)	((dmat)->common.lockfunc)
#define	dmat_lockfuncarg(dmat)	((dmat)->common.lockfuncarg)
#define	dmat_maxsegsz(dmat)	((dmat)->common.maxsegsz)
#define	dmat_nsegments(dmat)	((dmat)->common.nsegments)

#include "../../kern/subr_busdma_bounce.c"

/*
 * On i386 kernels without 'options PAE' we need to also bounce any
 * physical addresses above 4G.
 *
 * NB: vm_paddr_t is required here since bus_addr_t is only 32 bits in
 * i386 kernels without 'options PAE'.
 */
static __inline bool
must_bounce(bus_dma_tag_t dmat, vm_paddr_t paddr)
{
#if defined(__i386__) && !defined(PAE)
	if (paddr > BUS_SPACE_MAXADDR)
		return (true);
#endif
	return (addr_needs_bounce(dmat, paddr));
}

static int
bounce_bus_dma_zone_setup(bus_dma_tag_t dmat)
{
	struct bounce_zone *bz;
	int error;

	/* Must bounce */
	if ((error = alloc_bounce_zone(dmat)) != 0)
		return (error);
	bz = dmat->bounce_zone;

	if (ptoa(bz->total_bpages) < dmat->common.maxsize) {
		int pages;

		pages = atop(dmat->common.maxsize) - bz->total_bpages;

		/* Add pages to our bounce pool */
		if (alloc_bounce_pages(dmat, pages) < pages)
			return (ENOMEM);
	}
	/* Performed initial allocation */
	dmat->bounce_flags |= BUS_DMA_MIN_ALLOC_COMP;

	return (0);
}

/*
 * Allocate a device specific dma_tag.
 */
static int
bounce_bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
    bus_addr_t boundary, bus_addr_t lowaddr, bus_addr_t highaddr,
    bus_size_t maxsize, int nsegments, bus_size_t maxsegsz, int flags,
    bus_dma_lock_t *lockfunc, void *lockfuncarg, bus_dma_tag_t *dmat)
{
	bus_dma_tag_t newtag;
	int error;

	*dmat = NULL;
	error = common_bus_dma_tag_create(parent != NULL ? &parent->common :
	    NULL, alignment, boundary, lowaddr, highaddr, maxsize, nsegments,
	    maxsegsz, flags, lockfunc, lockfuncarg, sizeof(struct bus_dma_tag),
	    (void **)&newtag);
	if (error != 0)
		return (error);

	newtag->common.impl = &bus_dma_bounce_impl;
	newtag->map_count = 0;
	newtag->segments = NULL;

#ifdef KMSAN
	/*
	 * When KMSAN is configured, we need a map to store a memory descriptor
	 * which can be used for validation.
	 */
	newtag->bounce_flags |= BUS_DMA_FORCE_MAP;
#endif

	if (parent != NULL &&
	    (parent->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0)
		newtag->bounce_flags |= BUS_DMA_COULD_BOUNCE;

	if (newtag->common.lowaddr < ptoa((vm_paddr_t)Maxmem) ||
	    newtag->common.alignment > 1)
		newtag->bounce_flags |= BUS_DMA_COULD_BOUNCE;

	if ((newtag->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0 &&
	    (flags & BUS_DMA_ALLOCNOW) != 0)
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

static bool
bounce_bus_dma_id_mapped(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen)
{

	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) == 0)
		return (true);
	return (!_bus_dmamap_pagesneeded(dmat, buf, buflen, NULL));
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
	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) == 0 ||
	    dmat->bounce_zone == NULL)
		return (0);
	dmat->bounce_flags &= ~BUS_DMA_MIN_ALLOC_COMP;
	return (bounce_bus_dma_zone_setup(dmat));
}

static int
bounce_bus_dma_tag_destroy(bus_dma_tag_t dmat)
{
	int error = 0;

	if (dmat != NULL) {
		if (dmat->map_count != 0) {
			error = EBUSY;
			goto out;
		}
		if (dmat->segments != NULL)
			free(dmat->segments, M_DEVBUF);
		free(dmat, M_DEVBUF);
	}
out:
	CTR3(KTR_BUSDMA, "%s tag %p error %d", __func__, dmat, error);
	return (error);
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
		dmat->segments = malloc_domainset(
		    sizeof(bus_dma_segment_t) * dmat->common.nsegments,
		    M_DEVBUF, DOMAINSET_PREF(dmat->common.domain), M_NOWAIT);
		if (dmat->segments == NULL) {
			CTR3(KTR_BUSDMA, "%s: tag %p error %d",
			    __func__, dmat, ENOMEM);
			return (ENOMEM);
		}
	}

	if (dmat->bounce_flags & (BUS_DMA_COULD_BOUNCE | BUS_DMA_FORCE_MAP)) {
		*mapp = malloc_domainset(sizeof(**mapp), M_DEVBUF,
		    DOMAINSET_PREF(dmat->common.domain), M_NOWAIT | M_ZERO);
		if (*mapp == NULL) {
			CTR3(KTR_BUSDMA, "%s: tag %p error %d",
			    __func__, dmat, ENOMEM);
			return (ENOMEM);
		}
		STAILQ_INIT(&(*mapp)->bpages);
	} else {
		*mapp = NULL;
	}

	/*
	 * Bouncing might be required if the driver asks for an active
	 * exclusion region, a data alignment that is stricter than 1, and/or
	 * an active address boundary.
	 */
	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0) {
		/* Must bounce */
		if (dmat->bounce_zone == NULL &&
		    (error = alloc_bounce_zone(dmat)) != 0)
			goto out;
		bz = dmat->bounce_zone;

		/*
		 * Attempt to add pages to our pool on a per-instance
		 * basis up to a sane limit.
		 */
		if (dmat->common.alignment > 1)
			maxpages = MAX_BPAGES;
		else
			maxpages = MIN(MAX_BPAGES, Maxmem -
			    atop(dmat->common.lowaddr));
		if ((dmat->bounce_flags & BUS_DMA_MIN_ALLOC_COMP) == 0 ||
		    (bz->map_count > 0 && bz->total_bpages < maxpages)) {
			pages = MAX(atop(dmat->common.maxsize), 1);
			pages = MIN(dmat->common.nsegments, pages);
			pages = MIN(maxpages - bz->total_bpages, pages);
			pages = MAX(pages, 1);
			if (alloc_bounce_pages(dmat, pages) < pages)
				error = ENOMEM;
			if ((dmat->bounce_flags & BUS_DMA_MIN_ALLOC_COMP)
			    == 0) {
				if (error == 0) {
					dmat->bounce_flags |=
					    BUS_DMA_MIN_ALLOC_COMP;
				}
			} else
				error = 0;
		}
		bz->map_count++;
	}

out:
	if (error == 0) {
		dmat->map_count++;
	} else {
		free(*mapp, M_DEVBUF);
		*mapp = NULL;
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

	if (map != NULL && map != &nobounce_dmamap) {
		if (STAILQ_FIRST(&map->bpages) != NULL) {
			CTR3(KTR_BUSDMA, "%s: tag %p error %d",
			    __func__, dmat, EBUSY);
			return (EBUSY);
		}
		if (dmat->bounce_zone)
			dmat->bounce_zone->map_count--;
		free(map, M_DEVBUF);
	}
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
bounce_bus_dmamem_alloc(bus_dma_tag_t dmat, void **vaddr, int flags,
    bus_dmamap_t *mapp)
{
	vm_memattr_t attr;
	int mflags;

	if (flags & BUS_DMA_NOWAIT)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;

	/* If we succeed, no mapping/bouncing will be required */
	*mapp = NULL;

	if (dmat->segments == NULL) {
		dmat->segments = (bus_dma_segment_t *)malloc_domainset(
		    sizeof(bus_dma_segment_t) * dmat->common.nsegments,
		    M_DEVBUF, DOMAINSET_PREF(dmat->common.domain), mflags);
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
	else
		attr = VM_MEMATTR_DEFAULT;

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
	 * Warn the user if malloc gets it wrong.
	 */
	if (dmat->common.maxsize <= PAGE_SIZE &&
	    dmat->common.alignment <= PAGE_SIZE &&
	    dmat->common.lowaddr >= ptoa((vm_paddr_t)Maxmem) &&
	    attr == VM_MEMATTR_DEFAULT) {
		*vaddr = malloc_domainset_aligned(dmat->common.maxsize,
		    dmat->common.alignment, M_DEVBUF,
		    DOMAINSET_PREF(dmat->common.domain), mflags);
		KASSERT(*vaddr == NULL || ((uintptr_t)*vaddr & PAGE_MASK) +
		    dmat->common.maxsize <= PAGE_SIZE,
		    ("bounce_bus_dmamem_alloc: multi-page alloc %p maxsize "
		    "%#jx align %#jx", *vaddr, (uintmax_t)dmat->common.maxsize,
		    (uintmax_t)dmat->common.alignment));
	} else if (dmat->common.nsegments >=
	    howmany(dmat->common.maxsize, MIN(dmat->common.maxsegsz,
	    PAGE_SIZE)) &&
	    dmat->common.alignment <= PAGE_SIZE &&
	    (dmat->common.boundary % PAGE_SIZE) == 0) {
		/* Page-based multi-segment allocations allowed */
		*vaddr = kmem_alloc_attr_domainset(
		    DOMAINSET_PREF(dmat->common.domain), dmat->common.maxsize,
		    mflags, 0ul, dmat->common.lowaddr, attr);
		dmat->bounce_flags |= BUS_DMA_KMEM_ALLOC;
	} else {
		*vaddr = kmem_alloc_contig_domainset(
		    DOMAINSET_PREF(dmat->common.domain), dmat->common.maxsize,
		    mflags, 0ul, dmat->common.lowaddr,
		    dmat->common.alignment != 0 ? dmat->common.alignment : 1ul,
		    dmat->common.boundary, attr);
		dmat->bounce_flags |= BUS_DMA_KMEM_ALLOC;
	}
	if (*vaddr == NULL) {
		CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
		    __func__, dmat, dmat->common.flags, ENOMEM);
		return (ENOMEM);
	} else if (!vm_addr_align_ok(vtophys(*vaddr), dmat->common.alignment)) {
		printf("bus_dmamem_alloc failed to align memory properly.\n");
	}
	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x error %d",
	    __func__, dmat, dmat->common.flags, 0);
	return (0);
}

/*
 * Free a piece of memory and its associated dmamap, that was allocated
 * via bus_dmamem_alloc.
 */
static void
bounce_bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
	/*
	 * dmamem does not need to be bounced, so the map should be
	 * NULL and the BUS_DMA_KMEM_ALLOC flag cleared if malloc()
	 * was used and set if kmem_alloc_contig() was used.
	 */
	if (map != NULL)
		panic("bus_dmamem_free: Invalid map freed\n");
	if ((dmat->bounce_flags & BUS_DMA_KMEM_ALLOC) == 0)
		free(vaddr, M_DEVBUF);
	else
		kmem_free(vaddr, dmat->common.maxsize);
	CTR3(KTR_BUSDMA, "%s: tag %p flags 0x%x", __func__, dmat,
	    dmat->bounce_flags);
}

static bool
_bus_dmamap_pagesneeded(bus_dma_tag_t dmat, vm_paddr_t buf, bus_size_t buflen,
    int *pagesneeded)
{
	vm_paddr_t curaddr;
	bus_size_t sgsize;
	int count;

	/*
	 * Count the number of bounce pages needed in order to
	 * complete this transfer
	 */
	count = 0;
	curaddr = buf;
	while (buflen != 0) {
		sgsize = buflen;
		if (must_bounce(dmat, curaddr)) {
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

	if (map != &nobounce_dmamap && map->pagesneeded == 0) {
		_bus_dmamap_pagesneeded(dmat, buf, buflen, &map->pagesneeded);
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}
}

static void
_bus_dmamap_count_pages(bus_dma_tag_t dmat, bus_dmamap_t map, pmap_t pmap,
    void *buf, bus_size_t buflen, int flags)
{
	vm_offset_t vaddr;
	vm_offset_t vendaddr;
	vm_paddr_t paddr;
	bus_size_t sg_len;

	if (map != &nobounce_dmamap && map->pagesneeded == 0) {
		CTR4(KTR_BUSDMA, "lowaddr= %d Maxmem= %d, boundary= %d, "
		    "alignment= %d", dmat->common.lowaddr,
		    ptoa((vm_paddr_t)Maxmem),
		    dmat->common.boundary, dmat->common.alignment);
		CTR3(KTR_BUSDMA, "map= %p, nobouncemap= %p, pagesneeded= %d",
		    map, &nobounce_dmamap, map->pagesneeded);
		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		vaddr = (vm_offset_t)buf;
		vendaddr = (vm_offset_t)buf + buflen;

		while (vaddr < vendaddr) {
			sg_len = MIN(vendaddr - vaddr,
			    PAGE_SIZE - ((vm_offset_t)vaddr & PAGE_MASK));
			if (pmap == kernel_pmap)
				paddr = pmap_kextract(vaddr);
			else
				paddr = pmap_extract(pmap, vaddr);
			if (must_bounce(dmat, paddr)) {
				sg_len = roundup2(sg_len,
				    dmat->common.alignment);
				map->pagesneeded++;
			}
			vaddr += sg_len;
		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}
}

static void
_bus_dmamap_count_ma(bus_dma_tag_t dmat, bus_dmamap_t map, struct vm_page **ma,
    int ma_offs, bus_size_t buflen, int flags)
{
	bus_size_t sg_len;
	int page_index;
	vm_paddr_t paddr;

	if (map != &nobounce_dmamap && map->pagesneeded == 0) {
		CTR4(KTR_BUSDMA, "lowaddr= %d Maxmem= %d, boundary= %d, "
		    "alignment= %d", dmat->common.lowaddr,
		    ptoa((vm_paddr_t)Maxmem),
		    dmat->common.boundary, dmat->common.alignment);
		CTR3(KTR_BUSDMA, "map= %p, nobouncemap= %p, pagesneeded= %d",
		    map, &nobounce_dmamap, map->pagesneeded);

		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		page_index = 0;
		while (buflen > 0) {
			paddr = VM_PAGE_TO_PHYS(ma[page_index]) + ma_offs;
			sg_len = PAGE_SIZE - ma_offs;
			sg_len = MIN(sg_len, buflen);
			if (must_bounce(dmat, paddr)) {
				sg_len = roundup2(sg_len,
				    dmat->common.alignment);
				KASSERT(vm_addr_align_ok(sg_len,
				    dmat->common.alignment),
				    ("Segment size is not aligned"));
				map->pagesneeded++;
			}
			if (((ma_offs + sg_len) & ~PAGE_MASK) != 0)
				page_index++;
			ma_offs = (ma_offs + sg_len) & PAGE_MASK;
			KASSERT(buflen >= sg_len,
			    ("Segment length overruns original buffer"));
			buflen -= sg_len;
		}
		CTR1(KTR_BUSDMA, "pagesneeded= %d\n", map->pagesneeded);
	}
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
	bus_size_t sgsize;
	vm_paddr_t curaddr;
	int error;

	if (map == NULL)
		map = &nobounce_dmamap;

	if (segs == NULL)
		segs = dmat->segments;

	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0) {
		_bus_dmamap_count_phys(dmat, map, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	while (buflen > 0) {
		curaddr = buf;
		sgsize = buflen;
		if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0 &&
		    map->pagesneeded != 0 &&
		    must_bounce(dmat, curaddr)) {
			sgsize = MIN(sgsize, PAGE_SIZE - (curaddr & PAGE_MASK));
			curaddr = add_bounce_page(dmat, map, 0, curaddr, 0,
			    sgsize);
		}

		if (!_bus_dmamap_addsegs(dmat, map, curaddr, sgsize, segs,
		    segp))
			break;
		buf += sgsize;
		buflen -= sgsize;
	}

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
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
	bus_size_t sgsize;
	vm_paddr_t curaddr;
	vm_offset_t kvaddr, vaddr;
	int error;

	if (map == NULL)
		map = &nobounce_dmamap;

	if (segs == NULL)
		segs = dmat->segments;

	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0) {
		_bus_dmamap_count_pages(dmat, map, pmap, buf, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	vaddr = (vm_offset_t)buf;
	while (buflen > 0) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap == kernel_pmap) {
			curaddr = pmap_kextract(vaddr);
			kvaddr = vaddr;
		} else {
			curaddr = pmap_extract(pmap, vaddr);
			kvaddr = 0;
		}

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = MIN(buflen, PAGE_SIZE - (curaddr & PAGE_MASK));
		if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0 &&
		    map->pagesneeded != 0 &&
		    must_bounce(dmat, curaddr)) {
			sgsize = roundup2(sgsize, dmat->common.alignment);
			curaddr = add_bounce_page(dmat, map, kvaddr, curaddr, 0,
			    sgsize);
		}
		if (!_bus_dmamap_addsegs(dmat, map, curaddr, sgsize, segs,
		    segp))
			break;
		vaddr += sgsize;
		buflen -= MIN(sgsize, buflen); /* avoid underflow */
	}

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
}

static int
bounce_bus_dmamap_load_ma(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct vm_page **ma, bus_size_t buflen, int ma_offs, int flags,
    bus_dma_segment_t *segs, int *segp)
{
	vm_paddr_t paddr, next_paddr;
	int error, page_index;
	bus_size_t sgsize;

	if (dmat->common.flags & BUS_DMA_KEEP_PG_OFFSET) {
		/*
		 * If we have to keep the offset of each page this function
		 * is not suitable, switch back to bus_dmamap_load_ma_triv
		 * which is going to do the right thing in this case.
		 */
		error = bus_dmamap_load_ma_triv(dmat, map, ma, buflen, ma_offs,
		    flags, segs, segp);
		return (error);
	}

	if (map == NULL)
		map = &nobounce_dmamap;

	if (segs == NULL)
		segs = dmat->segments;

	if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0) {
		_bus_dmamap_count_ma(dmat, map, ma, ma_offs, buflen, flags);
		if (map->pagesneeded != 0) {
			error = _bus_dmamap_reserve_pages(dmat, map, flags);
			if (error)
				return (error);
		}
	}

	page_index = 0;
	while (buflen > 0) {
		/*
		 * Compute the segment size, and adjust counts.
		 */
		paddr = VM_PAGE_TO_PHYS(ma[page_index]) + ma_offs;
		sgsize = MIN(buflen, PAGE_SIZE - ma_offs);
		if ((dmat->bounce_flags & BUS_DMA_COULD_BOUNCE) != 0 &&
		    map->pagesneeded != 0 &&
		    must_bounce(dmat, paddr)) {
			sgsize = roundup2(sgsize, dmat->common.alignment);
			KASSERT(vm_addr_align_ok(sgsize,
			    dmat->common.alignment),
			    ("Segment size is not aligned"));
			/*
			 * Check if two pages of the user provided buffer
			 * are used.
			 */
			if ((ma_offs + sgsize) > PAGE_SIZE)
				next_paddr =
				    VM_PAGE_TO_PHYS(ma[page_index + 1]);
			else
				next_paddr = 0;
			paddr = add_bounce_page(dmat, map, 0, paddr,
			    next_paddr, sgsize);
		}
		if (!_bus_dmamap_addsegs(dmat, map, paddr, sgsize, segs,
		    segp))
			break;
		KASSERT(buflen >= sgsize,
		    ("Segment length overruns original buffer"));
		buflen -= MIN(sgsize, buflen); /* avoid underflow */
		if (((ma_offs + sgsize) & ~PAGE_MASK) != 0)
			page_index++;
		ma_offs = (ma_offs + sgsize) & PAGE_MASK;
	}

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
}

static void
bounce_bus_dmamap_waitok(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct memdesc *mem, bus_dmamap_callback_t *callback, void *callback_arg)
{

	if (map == NULL)
		return;
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
	if (map == NULL)
		return;

	free_bounce_pages(dmat, map);
}

static void
bounce_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dmasync_op_t op)
{
	struct bounce_page *bpage;
	vm_offset_t datavaddr, tempvaddr;
	bus_size_t datacount1, datacount2;

	if (map == NULL)
		goto out;
	if ((bpage = STAILQ_FIRST(&map->bpages)) == NULL)
		goto out;

	/*
	 * Handle data bouncing.  We might also want to add support for
	 * invalidating the caches on broken hardware.
	 */
	CTR4(KTR_BUSDMA, "%s: tag %p tag flags 0x%x op 0x%x "
	    "performing bounce", __func__, dmat, dmat->common.flags, op);

	if ((op & BUS_DMASYNC_PREWRITE) != 0) {
		while (bpage != NULL) {
			tempvaddr = 0;
			datavaddr = bpage->datavaddr;
			datacount1 = bpage->datacount;
			if (datavaddr == 0) {
				tempvaddr =
				    pmap_quick_enter_page(bpage->datapage[0]);
				datavaddr = tempvaddr | bpage->dataoffs;
				datacount1 = min(PAGE_SIZE - bpage->dataoffs,
				    datacount1);
			}

			bcopy((void *)datavaddr,
			    (void *)bpage->vaddr, datacount1);

			if (tempvaddr != 0)
				pmap_quick_remove_page(tempvaddr);

			if (bpage->datapage[1] == 0) {
				KASSERT(datacount1 == bpage->datacount,
		("Mismatch between data size and provided memory space"));
				goto next_w;
			}

			/*
			 * We are dealing with an unmapped buffer that expands
			 * over two pages.
			 */
			datavaddr = pmap_quick_enter_page(bpage->datapage[1]);
			datacount2 = bpage->datacount - datacount1;
			bcopy((void *)datavaddr,
			    (void *)(bpage->vaddr + datacount1), datacount2);
			pmap_quick_remove_page(datavaddr);

next_w:
			bpage = STAILQ_NEXT(bpage, links);
		}
		dmat->bounce_zone->total_bounced++;
	}

	if ((op & BUS_DMASYNC_POSTREAD) != 0) {
		while (bpage != NULL) {
			tempvaddr = 0;
			datavaddr = bpage->datavaddr;
			datacount1 = bpage->datacount;
			if (datavaddr == 0) {
				tempvaddr =
				    pmap_quick_enter_page(bpage->datapage[0]);
				datavaddr = tempvaddr | bpage->dataoffs;
				datacount1 = min(PAGE_SIZE - bpage->dataoffs,
				    datacount1);
			}

			bcopy((void *)bpage->vaddr, (void *)datavaddr,
			    datacount1);

			if (tempvaddr != 0)
				pmap_quick_remove_page(tempvaddr);

			if (bpage->datapage[1] == 0) {
				KASSERT(datacount1 == bpage->datacount,
		("Mismatch between data size and provided memory space"));
				goto next_r;
			}

			/*
			 * We are dealing with an unmapped buffer that expands
			 * over two pages.
			 */
			datavaddr = pmap_quick_enter_page(bpage->datapage[1]);
			datacount2 = bpage->datacount - datacount1;
			bcopy((void *)(bpage->vaddr + datacount1),
			    (void *)datavaddr, datacount2);
			pmap_quick_remove_page(datavaddr);

next_r:
			bpage = STAILQ_NEXT(bpage, links);
		}
		dmat->bounce_zone->total_bounced++;
	}
out:
	atomic_thread_fence_rel();
	if (map != NULL)
		kmsan_bus_dmamap_sync(&map->kmsan_mem, op);
}

#ifdef KMSAN
static void
bounce_bus_dmamap_load_kmsan(bus_dmamap_t map, struct memdesc *mem)
{
	if (map == NULL)
		return;
	memcpy(&map->kmsan_mem, mem, sizeof(map->kmsan_mem));
}
#endif

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
	.load_ma = bounce_bus_dmamap_load_ma,
	.map_waitok = bounce_bus_dmamap_waitok,
	.map_complete = bounce_bus_dmamap_complete,
	.map_unload = bounce_bus_dmamap_unload,
	.map_sync = bounce_bus_dmamap_sync,
#ifdef KMSAN
	.load_kmsan = bounce_bus_dmamap_load_kmsan,
#endif
};
