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
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>

#include <machine/asi.h>
#include <machine/bus.h>
#include <machine/cache.h>
#include <machine/pmap.h>
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

static int nexus_dmamap_create(bus_dma_tag_t, int, bus_dmamap_t *);
static int nexus_dmamap_destroy(bus_dma_tag_t, bus_dmamap_t);
static int nexus_dmamap_load(bus_dma_tag_t, bus_dmamap_t, void *, bus_size_t,
    bus_dmamap_callback_t *, void *, int);
static void nexus_dmamap_unload(bus_dma_tag_t, bus_dmamap_t);
static void nexus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_dmasync_op_t);
static int nexus_dmamem_alloc(bus_dma_tag_t, void **, int, bus_dmamap_t *);
static void nexus_dmamem_free(bus_dma_tag_t, void *, bus_dmamap_t);


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

	bus_dma_tag_t newtag, eparent;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF, M_NOWAIT);
	if (newtag == NULL)
		return (ENOMEM);

	/* Ugh... */
	eparent = parent != NULL ? parent : sparc64_root_dma_tag;
	memcpy(newtag, eparent, sizeof(*newtag));
	if (parent != NULL)
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
	
	/* Take into account any restrictions imposed by our parent tag */
	if (parent != NULL) {
		newtag->lowaddr = ulmin(parent->lowaddr, newtag->lowaddr);
		newtag->highaddr = ulmax(parent->highaddr, newtag->highaddr);
		/*
		 * XXX Not really correct??? Probably need to honor boundary
		 *     all the way up the inheritence chain.
		 */
		newtag->boundary = ulmax(parent->boundary, newtag->boundary);
		if (parent != NULL)
			parent->ref_count++;
	}
	
	*dmat = newtag;
	return (0);
}

int
bus_dma_tag_destroy(bus_dma_tag_t dmat)
{

	if (dmat != NULL) {
		if (dmat->map_count != 0)
			return (EBUSY);

		while (dmat != NULL) {
			bus_dma_tag_t parent;

			parent = dmat->parent;
			dmat->ref_count--;
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
	return (0);
}

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
static int
nexus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{

	/* Not much to do...? */
	*mapp = malloc(sizeof(**mapp), M_DEVBUF, M_WAITOK | M_ZERO);
	dmat->map_count++;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
static int
nexus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	free(map, M_DEVBUF);
	dmat->map_count--;
	return (0);
}

#define BUS_DMAMAP_NSEGS ((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1)

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
	vm_offset_t vaddr;
	vm_offset_t paddr;
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	bus_dma_segment_t *sg;
	int seg;
	int error;
	vm_offset_t nextpaddr;
	bus_size_t size;

	error = 0;

	vaddr = (vm_offset_t)buf;
	sg = &dm_segments[0];
	seg = 1;
	sg->ds_len = 0;

	map->buf = buf;
	map->buflen = buflen;
	map->start = (bus_addr_t)buf;

	nextpaddr = 0;
	do {
		paddr = pmap_kextract(vaddr);
		size = PAGE_SIZE - (paddr & PAGE_MASK);
		if (size > buflen)
			size = buflen;

		if (sg->ds_len == 0) {
			sg->ds_addr = paddr;
			sg->ds_len = size;
		} else if (paddr == nextpaddr) {
			sg->ds_len += size;
		} else {
			/* Go to the next segment */
			sg++;
			seg++;
			if (seg > dmat->nsegments)
				break;
			sg->ds_addr = paddr;
			sg->ds_len = size;
		}
		vaddr += size;
		nextpaddr = paddr + size;
		buflen -= size;
	} while (buflen > 0);

	if (buflen != 0) {
		printf("bus_dmamap_load: Too many segs! buf_len = 0x%lx\n",
		       (u_long)buflen);
		error = EFBIG;
	}

	(*callback)(callback_arg, dm_segments, seg, error);

	return (0);
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
static void
nexus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	/* Nothing to do...? */
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
	if ((op == BUS_DMASYNC_PREREAD) || (op == BUS_DMASYNC_PREWRITE)) {
		/* 
		 * Don't really need to do anything, but flush any pending
		 * writes anyway. 
		 */
		membar(Sync);
	}
	if (op == BUS_DMASYNC_POSTREAD) {
		/*
		 * Invalidate the caches (it is unclear whether that is really
		 * needed. The manual only mentions that PCI transactions are
		 * cache coherent).
		 */
		ecache_flush((vm_offset_t)map->buf,
		    (vm_offset_t)map->buf + map->buflen - 1);
	}
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

	*mapp = malloc(sizeof(**mapp), M_DEVBUF, M_WAITOK | M_ZERO);
	if (*mapp == NULL)
		return (ENOMEM);
	
	dmat->map_count++;
	return (0);
}

void
sparc64_dmamem_free_map(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	free(map, M_DEVBUF);
	dmat->map_count--;
}

/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
static int
nexus_dmamem_alloc(bus_dma_tag_t dmat, void **vaddr, int flags,
    bus_dmamap_t *mapp)
{
	
	if ((dmat->maxsize <= PAGE_SIZE)) {
		*vaddr = malloc(dmat->maxsize, M_DEVBUF,
		    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	} else {
		/*
		 * XXX: Use contigmalloc until it is merged into this facility
		 * and handles multi-seg allocations.  Nobody is doing multi-seg
		 * allocations yet though.
		 */
		*vaddr = contigmalloc(dmat->maxsize, M_DEVBUF,
		    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK,
		    0ul, dmat->lowaddr, dmat->alignment ? dmat->alignment : 1UL,
		    dmat->boundary);
	}
	if (*vaddr == NULL) {
		free(*mapp, M_DEVBUF);
		return (ENOMEM);
	}
	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
static void
nexus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{

	sparc64_dmamem_free_map(dmat, map);
	if ((dmat->maxsize <= PAGE_SIZE))
		free(vaddr, M_DEVBUF);
	else
		contigfree(vaddr, dmat->maxsize, M_DEVBUF);
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
	nexus_dmamap_unload,
	nexus_dmamap_sync,

	nexus_dmamem_alloc,
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
	switch (tag->type) {
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

	/* note: preserve page offset */
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
		pmap_kremove(va);
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

	ptag->cookie = NULL;
	ptag->parent = NULL;
	ptag->type = space;
	ptag->bus_barrier = NULL;
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
