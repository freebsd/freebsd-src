/*-
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001-2003 Thomas Moestl
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
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
 *
 *	from: NetBSD: sbus.c,v 1.13 1999/05/23 07:24:02 mrg Exp
 *	from: @(#)sbus.c	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: iommu.c,v 1.42 2001/08/06 22:02:58 eeh Exp
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/hviommu.h>
#include <machine/pmap.h>
#include <machine/resource.h>

#include <machine/hypervisorvar.h>
#include <machine/hv_api.h>


#include <sys/rman.h>

/*
 * Tuning constants.
 */
#define	IOMMU_MAX_PRE		(32 * 1024)
#define	IOMMU_MAX_PRE_SEG	3

#define	IO_PAGE_SIZE		PAGE_SIZE_8K
#define	IO_PAGE_MASK		PAGE_MASK_8K
#define	IO_PAGE_SHIFT		PAGE_SHIFT_8K
#define	round_io_page(x)	round_page(x)
#define	trunc_io_page(x)	trunc_page(x)


MALLOC_DEFINE(M_HVIOMMU, "hviommu", "HyperVisor IOMMU");

TAILQ_HEAD(hviommu_maplruq_head, bus_dmamap);

struct hviommu {
	struct mtx	him_mtx;

	devhandle_t	him_handle;
	u_long		him_dvmabase;
	u_long		him_dvmasize;

	struct hviommu_maplruq_head him_maplruq;
	struct rman	him_rman;
};

#define VA_TO_TSBID(him, va)	((va - (him)->him_dvmabase) >> IO_PAGE_SHIFT)

#ifdef IOMMU_DEBUG
#define DPRINTF printf
#else 
#define DPRINTF(...)
#endif

/*
 * Always overallocate one page; this is needed to handle alignment of the
 * buffer, so it makes sense using a lazy allocation scheme.
 */
#define	IOMMU_SIZE_ROUNDUP(sz)						\
	(round_io_page(sz) + IO_PAGE_SIZE)

/* Resource helpers */
#define IOMMU_RES_TO(v)	((v) >> IO_PAGE_SHIFT)
#define	IOMMU_RES_START(res)						\
	((bus_addr_t)rman_get_start(res) << IO_PAGE_SHIFT)
#define	IOMMU_RES_END(res)						\
	((bus_addr_t)(rman_get_end(res) + 1) << IO_PAGE_SHIFT)
#define	IOMMU_RES_SIZE(res)						\
	((bus_size_t)rman_get_size(res) << IO_PAGE_SHIFT)

/* Helpers for struct bus_dmamap_res */
#define	BDR_START(r)	IOMMU_RES_START((r)->dr_res)
#define	BDR_END(r)	IOMMU_RES_END((r)->dr_res)
#define	BDR_SIZE(r)	IOMMU_RES_SIZE((r)->dr_res)

/* Locking macros. */
#define	HIM_LOCK(him)	mtx_lock(&him->him_mtx)
#define	HIM_LOCK_ASSERT(him)	mtx_assert(&him->him_mtx, MA_OWNED)
#define	HIM_UNLOCK(him)	mtx_unlock(&him->him_mtx)

/* LRU queue handling for lazy resource allocation. */
static __inline void
hviommu_map_insq(struct hviommu *him, bus_dmamap_t map)
{

	HIM_LOCK_ASSERT(him);
	if (!SLIST_EMPTY(&map->dm_reslist)) {
		if (map->dm_onq)
			TAILQ_REMOVE(&him->him_maplruq, map, dm_maplruq);
		TAILQ_INSERT_TAIL(&him->him_maplruq, map, dm_maplruq);
		map->dm_onq = 1;
	}
}

static __inline void
hviommu_map_remq(struct hviommu *him, bus_dmamap_t map)
{

	HIM_LOCK_ASSERT(him);
	if (map->dm_onq)
		TAILQ_REMOVE(&him->him_maplruq, map, dm_maplruq);
	map->dm_onq = 0;
}

struct hviommu *
hviommu_init(devhandle_t dh, u_long dvmabase, u_long dvmasize)
{
	struct hviommu *him;
	u_long end;

	him = malloc(sizeof *him, M_HVIOMMU, M_WAITOK|M_ZERO);

	mtx_init(&him->him_mtx, "hviommu", NULL, MTX_DEF);
	him->him_handle = dh;
	him->him_dvmabase = dvmabase;
	him->him_dvmasize = dvmasize;

	TAILQ_INIT(&him->him_maplruq);
	him->him_rman.rm_type = RMAN_ARRAY;
	him->him_rman.rm_descr = "HyperVisor IOMMU Memory";
	end = him->him_dvmabase + him->him_dvmasize - 1;
	if (rman_init(&him->him_rman) != 0 ||
	    rman_manage_region(&him->him_rman, him->him_dvmabase >>
	    IO_PAGE_SHIFT, end >> IO_PAGE_SHIFT) != 0)
		panic("%s: can't initalize rman", __func__);

	return him;
}

static void
hviommu_remove(struct hviommu *him, vm_offset_t va, vm_size_t len)
{
	uint64_t error;
	pages_t demapped;

	KASSERT(va >= him->him_dvmabase,
	    ("%s: va 0x%lx not in DVMA space", __func__, (u_long)va));
	KASSERT(va + len >= va,
	    ("%s: va 0x%lx + len 0x%lx wraps", __func__, (long)va, (long)len));
	KASSERT((va & IO_PAGE_MASK) == 0 && (len & IO_PAGE_MASK) == 0,
	    ("%s: va %#lx or len %#lx not page aligned", __func__, va, len));
	while (len > 0) {
		if ((error = hv_pci_iommu_demap(him->him_handle,
		    VA_TO_TSBID(him, va), len >> IO_PAGE_SHIFT, &demapped))) {
			printf("%s: demap: va: %#lx, npages: %#lx, err: %ld\n",
			    __func__, va, len >> IO_PAGE_SHIFT, error);
			demapped = 1;
		}
		va += demapped << IO_PAGE_SHIFT;
		len -= demapped << IO_PAGE_SHIFT;
	}
}

/*
 * Allocate DVMA virtual memory for a map. The map may not be on a queue, so
 * that it can be freely modified.
 */
static int
hviommu_dvma_valloc(bus_dma_tag_t t, struct hviommu *him, bus_dmamap_t map,
    bus_size_t size)
{
	struct resource *res;
	struct bus_dmamap_res *bdr;
	bus_size_t align, sgsize;

	KASSERT(!map->dm_onq, ("hviommu_dvma_valloc: map on queue!"));
	if ((bdr = malloc(sizeof(*bdr), M_HVIOMMU, M_NOWAIT)) == NULL)
		return (EAGAIN);

	/*
	 * If a boundary is specified, a map cannot be larger than it; however
	 * we do not clip currently, as that does not play well with the lazy
	 * allocation code.
	 * Alignment to a page boundary is always enforced.
	 */
	align = (t->dt_alignment + IO_PAGE_MASK) >> IO_PAGE_SHIFT;
	sgsize = IOMMU_RES_TO(round_io_page(size));
	if (t->dt_boundary > 0 && t->dt_boundary < IO_PAGE_SIZE)
		panic("hviommu_dvmamap_load: illegal boundary specified");
	res = rman_reserve_resource_bound(&him->him_rman, 0L,
	    IOMMU_RES_TO(t->dt_lowaddr), sgsize,
	    IOMMU_RES_TO(t->dt_boundary),
	    RF_ACTIVE | rman_make_alignment_flags(align), NULL);
	if (res == NULL) {
		free(bdr, M_HVIOMMU);
		return (ENOMEM);
	}

	bdr->dr_res = res;
	bdr->dr_used = 0;
	SLIST_INSERT_HEAD(&map->dm_reslist, bdr, dr_link);
	return (0);
}

/* Unload the map and mark all resources as unused, but do not free them. */
static void
hviommu_dvmamap_vunload(struct hviommu *him, bus_dmamap_t map)
{
	struct bus_dmamap_res *r;

	SLIST_FOREACH(r, &map->dm_reslist, dr_link) {
		hviommu_remove(him, BDR_START(r), BDR_SIZE(r));
		r->dr_used = 0;
	}
}

/* Free a DVMA virtual memory resource. */
static __inline void
hviommu_dvma_vfree_res(bus_dmamap_t map, struct bus_dmamap_res *r)
{

	KASSERT(r->dr_used == 0, ("hviommu_dvma_vfree_res: resource busy!"));
	if (r->dr_res != NULL && rman_release_resource(r->dr_res) != 0)
		printf("warning: DVMA space lost\n");
	SLIST_REMOVE(&map->dm_reslist, r, bus_dmamap_res, dr_link);
	free(r, M_HVIOMMU);
}

/* Free all DVMA virtual memory for a map. */
static void
hviommu_dvma_vfree(struct hviommu *him, bus_dmamap_t map)
{

	HIM_LOCK(him);
	hviommu_map_remq(him, map);
	hviommu_dvmamap_vunload(him, map);
	HIM_UNLOCK(him);
	while (!SLIST_EMPTY(&map->dm_reslist))
		hviommu_dvma_vfree_res(map, SLIST_FIRST(&map->dm_reslist));
}

/* Prune a map, freeing all unused DVMA resources. */
static bus_size_t
hviommu_dvma_vprune(struct hviommu *him, bus_dmamap_t map)
{
	struct bus_dmamap_res *r, *n;
	bus_size_t freed = 0;

	HIM_LOCK_ASSERT(him);
	for (r = SLIST_FIRST(&map->dm_reslist); r != NULL; r = n) {
		n = SLIST_NEXT(r, dr_link);
		if (r->dr_used == 0) {
			freed += BDR_SIZE(r);
			hviommu_dvma_vfree_res(map, r);
		}
	}
	if (SLIST_EMPTY(&map->dm_reslist))
		hviommu_map_remq(him, map);
	return (freed);
}

/*
 * Try to find a suitably-sized (and if requested, -aligned) slab of DVMA
 * memory with IO page offset voffs.
 */
static bus_addr_t
hviommu_dvma_vfindseg(bus_dmamap_t map, vm_offset_t voffs, bus_size_t size,
    bus_addr_t amask)
{
	struct bus_dmamap_res *r;
	bus_addr_t dvmaddr, dvmend;

	KASSERT(!map->dm_onq, ("hviommu_dvma_vfindseg: map on queue!"));
	SLIST_FOREACH(r, &map->dm_reslist, dr_link) {
		dvmaddr = round_io_page(BDR_START(r) + r->dr_used);
		/* Alignment can only work with voffs == 0. */
		dvmaddr = (dvmaddr + amask) & ~amask;
		dvmaddr += voffs;
		dvmend = dvmaddr + size;
		if (dvmend <= BDR_END(r)) {
			r->dr_used = dvmend - BDR_START(r);
			r->dr_offset = voffs;
			return (dvmaddr);
		}
	}
	return (0);
}

/*
 * Try to find or allocate a slab of DVMA space; see above.
 */
static int
hviommu_dvma_vallocseg(bus_dma_tag_t dt, struct hviommu *him, bus_dmamap_t map,
    vm_offset_t voffs, bus_size_t size, bus_addr_t amask, bus_addr_t *addr)
{
	bus_dmamap_t tm, last;
	bus_addr_t dvmaddr, freed;
	int error, complete = 0;

	dvmaddr = hviommu_dvma_vfindseg(map, voffs, size, amask);

	/* Need to allocate. */
	if (dvmaddr == 0) {
		while ((error = hviommu_dvma_valloc(dt, him, map,
			voffs + size)) == ENOMEM && !complete) {
			/*
			 * Free the allocated DVMA of a few maps until
			 * the required size is reached. This is an
			 * approximation to not have to call the allocation
			 * function too often; most likely one free run
			 * will not suffice if not one map was large enough
			 * itself due to fragmentation.
			 */
			HIM_LOCK(him);
			freed = 0;
			last = TAILQ_LAST(&him->him_maplruq, hviommu_maplruq_head);
			do {
				tm = TAILQ_FIRST(&him->him_maplruq);
				complete = tm == last;
				if (tm == NULL)
					break;
				freed += hviommu_dvma_vprune(him, tm);
				/* Move to the end. */
				hviommu_map_insq(him, tm);
			} while (freed < size && !complete);
			HIM_UNLOCK(him);
		}
		if (error != 0)
			return (error);
		dvmaddr = hviommu_dvma_vfindseg(map, voffs, size, amask);
		KASSERT(dvmaddr != 0,
		    ("hviommu_dvma_vallocseg: allocation failed unexpectedly!"));
	}
	*addr = dvmaddr;
	return (0);
}

static int
hviommu_dvmamem_alloc(bus_dma_tag_t dt, void **vaddr, int flags,
    bus_dmamap_t *mapp)
{
	struct hviommu *him = dt->dt_cookie;
	int error, mflags;

	/*
	 * XXX: This will break for 32 bit transfers on machines with more than
	 * 16G (1 << 34 bytes) of memory.
	 */
	if ((error = sparc64_dma_alloc_map(dt, mapp)) != 0)
		return (error);

	if ((flags & BUS_DMA_NOWAIT) != 0)
		mflags = M_NOWAIT;
	else
		mflags = M_WAITOK;
	if ((flags & BUS_DMA_ZERO) != 0)
		mflags |= M_ZERO;

	if ((*vaddr = malloc(dt->dt_maxsize, M_HVIOMMU, mflags)) == NULL) {
		error = ENOMEM;
		sparc64_dma_free_map(dt, *mapp);
		return (error);
	}
	if ((flags & BUS_DMA_COHERENT) != 0)
		(*mapp)->dm_flags |= DMF_COHERENT;
	/*
	 * Try to preallocate DVMA space. If this fails, it is retried at load
	 * time.
	 */
	hviommu_dvma_valloc(dt, him, *mapp, IOMMU_SIZE_ROUNDUP(dt->dt_maxsize));
	HIM_LOCK(him);
	hviommu_map_insq(him, *mapp);
	HIM_UNLOCK(him);
	return (0);
}

static void
hviommu_dvmamem_free(bus_dma_tag_t dt, void *vaddr, bus_dmamap_t map)
{
	struct hviommu *him = dt->dt_cookie;

	hviommu_dvma_vfree(him, map);
	sparc64_dma_free_map(dt, map);
	free(vaddr, M_HVIOMMU);
}

static int
hviommu_dvmamap_create(bus_dma_tag_t dt, int flags, bus_dmamap_t *mapp)
{
	struct hviommu *him = dt->dt_cookie;
	bus_size_t totsz, presz, currsz;
	int error, i, maxpre;

	if ((error = sparc64_dma_alloc_map(dt, mapp)) != 0)
		return (error);
	if ((flags & BUS_DMA_COHERENT) != 0)
		(*mapp)->dm_flags |= DMF_COHERENT;
	/*
	 * Preallocate DVMA space; if this fails now, it is retried at load
	 * time. Through bus_dmamap_load_mbuf() and bus_dmamap_load_uio(), it
	 * is possible to have multiple discontiguous segments in a single map,
	 * which is handled by allocating additional resources, instead of
	 * increasing the size, to avoid fragmentation.
	 * Clamp preallocation to IOMMU_MAX_PRE. In some situations we can
	 * handle more; that case is handled by reallocating at map load time.
	 */
	totsz = ulmin(IOMMU_SIZE_ROUNDUP(dt->dt_maxsize), IOMMU_MAX_PRE); 
	error = hviommu_dvma_valloc(dt, him, *mapp, totsz);
	if (error != 0)
		return (0);
	/*
	 * Try to be smart about preallocating some additional segments if
	 * needed.
	 */
	maxpre = imin(dt->dt_nsegments, IOMMU_MAX_PRE_SEG);
	presz = dt->dt_maxsize / maxpre;
	for (i = 1; i < maxpre && totsz < IOMMU_MAX_PRE; i++) {
		currsz = round_io_page(ulmin(presz, IOMMU_MAX_PRE - totsz));
		error = hviommu_dvma_valloc(dt, him, *mapp, currsz);
		if (error != 0)
			break;
		totsz += currsz;
	}
	HIM_LOCK(him);
	hviommu_map_insq(him, *mapp);
	HIM_UNLOCK(him);
	return (0);
}

static int
hviommu_dvmamap_destroy(bus_dma_tag_t dt, bus_dmamap_t map)
{
	struct hviommu *him = dt->dt_cookie;

	hviommu_dvma_vfree(him, map);
	sparc64_dma_free_map(dt, map);
	return (0);
}

#define IOTTE_CNT	64

static void
hviommu_map_pages(struct hviommu *him, bus_addr_t dvmaddr, uint64_t *iottes, pages_t iottecnt)
{
	uint64_t err;
#ifdef IOMMU_DEBUG
	bus_addr_t ra;
	io_attributes_t ioattr;
#endif
	pages_t mapcnt;
	int cntdone;
	int i;

	DPRINTF("mapping: dh: %#lx, dvmaddr: %#lx, tsbid: %#lx, cnt: %d\n",
	    him->him_handle, dvmaddr, VA_TO_TSBID(him, dvmaddr), iottecnt);
	for (i = 0; i < iottecnt; i++) {
		DPRINTF("iotte:%#lx\n", iottes[i]);
	}

	/* push tte's */
	cntdone = 0;
	while (cntdone < iottecnt) {
		if ((err = hv_pci_iommu_map(him->him_handle, VA_TO_TSBID(him,
		    dvmaddr), iottecnt, PCI_MAP_ATTR_READ | PCI_MAP_ATTR_WRITE,
		    (io_page_list_t)pmap_kextract((vm_offset_t)&iottes[0]),
		    &mapcnt))) {
			DPRINTF("iommu_map: err: %ld\n", err);
			mapcnt = 1;
		}
		cntdone += mapcnt;
	}
	for (i = 0; i < iottecnt; i++) {
		DPRINTF("err: %ld", hv_pci_iommu_getmap(him->him_handle,
		    VA_TO_TSBID(him, dvmaddr + i * IO_PAGE_SIZE),
						     &ioattr, &ra));
		DPRINTF(", ioattr: %d, raddr: %#lx\n", ioattr, ra);
	}
}

/*
 * IOMMU DVMA operations, common to SBUS and PCI.
 */
static int
hviommu_dvmamap_load_buffer(bus_dma_tag_t dt, struct hviommu *him,
    bus_dmamap_t map, void *buf, bus_size_t buflen, struct thread *td,
    int flags, bus_dma_segment_t *segs, int *segp, int align)
{
	uint64_t iottes[IOTTE_CNT];
	bus_addr_t amask, dvmaddr, iottebase;
	bus_size_t sgsize, esize;
	vm_offset_t vaddr, voffs;
	vm_paddr_t curaddr;
	int error, sgcnt, firstpg;
	pmap_t pmap = NULL;
	pages_t iottecnt;

	KASSERT(buflen != 0, ("hviommu_dvmamap_load_buffer: buflen == 0!"));
	if (buflen > dt->dt_maxsize)
		return (EINVAL);

	if (td != NULL)
		pmap = vmspace_pmap(td->td_proc->p_vmspace);

	vaddr = (vm_offset_t)buf;
	voffs = vaddr & IO_PAGE_MASK;
	amask = align ? dt->dt_alignment - 1 : 0;

	/* Try to find a slab that is large enough. */
	error = hviommu_dvma_vallocseg(dt, him, map, voffs, buflen, amask,
	    &dvmaddr);
	if (error != 0)
		return (error);

	DPRINTF("vallocseg: dvmaddr: %#lx, voffs: %#lx, buflen: %#lx\n",
	    dvmaddr, voffs, buflen);
	sgcnt = *segp;
	firstpg = 1;
	iottecnt = 0;
	iottebase = 0;	/* shutup gcc */
	for (; buflen > 0; ) {
		/*
		 * Get the physical address for this page.
		 */
		if (pmap != NULL)
			curaddr = pmap_extract(pmap, vaddr);
		else
			curaddr = pmap_kextract(vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = IO_PAGE_SIZE - ((u_long)vaddr & IO_PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		buflen -= sgsize;
		vaddr += sgsize;

#if 0
		hviommu_enter(him, trunc_io_page(dvmaddr), trunc_io_page(curaddr),
		    flags);
#else
		if (iottecnt == 0)
			iottebase = trunc_io_page(dvmaddr);
		DPRINTF("adding: %#lx\n", trunc_io_page(curaddr));
		iottes[iottecnt++] = trunc_io_page(curaddr);

		if (iottecnt >= IOTTE_CNT) {
			hviommu_map_pages(him, iottebase, iottes, iottecnt);
			iottecnt = 0;
		}
#endif

		/*
		 * Chop the chunk up into segments of at most maxsegsz, but try
		 * to fill each segment as well as possible.
		 */
		if (!firstpg) {
			esize = ulmin(sgsize,
			    dt->dt_maxsegsz - segs[sgcnt].ds_len);
			segs[sgcnt].ds_len += esize;
			sgsize -= esize;
			dvmaddr += esize;
		}
		while (sgsize > 0) {
			sgcnt++;
			if (sgcnt >= dt->dt_nsegments)
				return (EFBIG);
			/*
			 * No extra alignment here - the common practice in the
			 * busdma code seems to be that only the first segment
			 * needs to satisfy the alignment constraints (and that
			 * only for bus_dmamem_alloc()ed maps). It is assumed
			 * that such tags have maxsegsize >= maxsize.
			 */
			esize = ulmin(sgsize, dt->dt_maxsegsz);
			segs[sgcnt].ds_addr = dvmaddr;
			segs[sgcnt].ds_len = esize;
			sgsize -= esize;
			dvmaddr += esize;
		}

		firstpg = 0;
	}
	hviommu_map_pages(him, iottebase, iottes, iottecnt);
	*segp = sgcnt;
	return (0);
}

static int
hviommu_dvmamap_load(bus_dma_tag_t dt, bus_dmamap_t map, void *buf,
    bus_size_t buflen, bus_dmamap_callback_t *cb, void *cba,
    int flags)
{
	struct hviommu *him = dt->dt_cookie;
	int error, seg = -1;

	if ((map->dm_flags & DMF_LOADED) != 0) {
#ifdef DIAGNOSTIC
		printf("hviommu_dvmamap_load: map still in use\n");
#endif
		bus_dmamap_unload(dt, map);
	}

	/*
	 * Make sure that the map is not on a queue so that the resource list
	 * may be safely accessed and modified without needing the lock to
	 * cover the whole operation.
	 */
	HIM_LOCK(him);
	hviommu_map_remq(him, map);
	HIM_UNLOCK(him);

	error = hviommu_dvmamap_load_buffer(dt, him, map, buf, buflen, NULL,
	    flags, dt->dt_segments, &seg, 1);

	HIM_LOCK(him);
	hviommu_map_insq(him, map);
	if (error != 0) {
		hviommu_dvmamap_vunload(him, map);
		HIM_UNLOCK(him);
		(*cb)(cba, dt->dt_segments, 0, error);
	} else {
		HIM_UNLOCK(him);
		map->dm_flags |= DMF_LOADED;
		(*cb)(cba, dt->dt_segments, seg + 1, 0);
	}

	return (error);
}

static int
hviommu_dvmamap_load_mbuf(bus_dma_tag_t dt, bus_dmamap_t map, struct mbuf *m0,
    bus_dmamap_callback2_t *cb, void *cba, int flags)
{
	struct hviommu *him = dt->dt_cookie;
	struct mbuf *m;
	int error = 0, first = 1, nsegs = -1;

	M_ASSERTPKTHDR(m0);

	if ((map->dm_flags & DMF_LOADED) != 0) {
#ifdef DIAGNOSTIC
		printf("hviommu_dvmamap_load_mbuf: map still in use\n");
#endif
		bus_dmamap_unload(dt, map);
	}

	HIM_LOCK(him);
	hviommu_map_remq(him, map);
	HIM_UNLOCK(him);

	if (m0->m_pkthdr.len <= dt->dt_maxsize) {
		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len == 0)
				continue;
			error = hviommu_dvmamap_load_buffer(dt, him, map,
			    m->m_data, m->m_len, NULL, flags, dt->dt_segments,
			    &nsegs, first);
			first = 0;
		}
	} else
		error = EINVAL;

	HIM_LOCK(him);
	hviommu_map_insq(him, map);
	if (error != 0) {
		hviommu_dvmamap_vunload(him, map);
		HIM_UNLOCK(him);
		/* force "no valid mappings" in callback */
		(*cb)(cba, dt->dt_segments, 0, 0, error);
	} else {
		HIM_UNLOCK(him);
		map->dm_flags |= DMF_LOADED;
		(*cb)(cba, dt->dt_segments, nsegs + 1, m0->m_pkthdr.len, 0);
	}
	return (error);
}

static int
hviommu_dvmamap_load_mbuf_sg(bus_dma_tag_t dt, bus_dmamap_t map,
    struct mbuf *m0, bus_dma_segment_t *segs, int *nsegs, int flags)
{
	struct hviommu *him = dt->dt_cookie;
	struct mbuf *m;
	int error = 0, first = 1;

	M_ASSERTPKTHDR(m0);

	*nsegs = -1;
	if ((map->dm_flags & DMF_LOADED) != 0) {
#ifdef DIAGNOSTIC
		printf("hviommu_dvmamap_load_mbuf: map still in use\n");
#endif
		bus_dmamap_unload(dt, map);
	}

	HIM_LOCK(him);
	hviommu_map_remq(him, map);
	HIM_UNLOCK(him);

	if (m0->m_pkthdr.len <= dt->dt_maxsize) {
		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			if (m->m_len == 0)
				continue;
			error = hviommu_dvmamap_load_buffer(dt, him, map,
			    m->m_data, m->m_len, NULL, flags, segs,
			    nsegs, first);
			first = 0;
		}
	} else
		error = EINVAL;

	HIM_LOCK(him);
	hviommu_map_insq(him, map);
	if (error != 0) {
		hviommu_dvmamap_vunload(him, map);
	} else {
		map->dm_flags |= DMF_LOADED;
		++*nsegs;
	}
	HIM_UNLOCK(him);
	return (error);
}

static int
hviommu_dvmamap_load_uio(bus_dma_tag_t dt, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *cb,  void *cba, int flags)
{
	struct hviommu *him = dt->dt_cookie;
	struct iovec *iov;
	struct thread *td = NULL;
	bus_size_t minlen, resid;
	int nsegs = -1, error = 0, first = 1, i;

	if ((map->dm_flags & DMF_LOADED) != 0) {
#ifdef DIAGNOSTIC
		printf("hviommu_dvmamap_load_uio: map still in use\n");
#endif
		bus_dmamap_unload(dt, map);
	}

	HIM_LOCK(him);
	hviommu_map_remq(him, map);
	HIM_UNLOCK(him);

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		td = uio->uio_td;
		KASSERT(td != NULL,
		    ("%s: USERSPACE but no proc", __func__));
	}

	for (i = 0; i < uio->uio_iovcnt && resid != 0 && error == 0; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		minlen = resid < iov[i].iov_len ? resid : iov[i].iov_len;
		if (minlen == 0)
			continue;

		error = hviommu_dvmamap_load_buffer(dt, him, map,
		    iov[i].iov_base, minlen, td, flags, dt->dt_segments, 
		    &nsegs, first);
		first = 0;

		resid -= minlen;
	}

	HIM_LOCK(him);
	hviommu_map_insq(him, map);
	if (error) {
		hviommu_dvmamap_vunload(him, map);
		HIM_UNLOCK(him);
		/* force "no valid mappings" in callback */
		(*cb)(cba, dt->dt_segments, 0, 0, error);
	} else {
		HIM_UNLOCK(him);
		map->dm_flags |= DMF_LOADED;
		(*cb)(cba, dt->dt_segments, nsegs + 1, uio->uio_resid, 0);
	}
	return (error);
}

static void
hviommu_dvmamap_unload(bus_dma_tag_t dt, bus_dmamap_t map)
{
	struct hviommu *him = dt->dt_cookie;

	if ((map->dm_flags & DMF_LOADED) == 0)
		return;
	HIM_LOCK(him);
	hviommu_dvmamap_vunload(him, map);
	hviommu_map_insq(him, map);
	HIM_UNLOCK(him);
	map->dm_flags &= ~DMF_LOADED;
}

static void
hviommu_dvmamap_sync(bus_dma_tag_t dt, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct hviommu *him = dt->dt_cookie;
	struct bus_dmamap_res *r;
	vm_offset_t va;
	vm_size_t len;
	size_t synced;
	bus_addr_t ra;
	uint64_t err;
	io_attributes_t ioattr;
	vm_paddr_t raddr;
	io_sync_direction_t iodir;

	if ((map->dm_flags & DMF_LOADED) == 0)
		return;

	iodir = 0;

	if (op & (BUS_DMASYNC_POSTREAD))
		iodir |= IO_SYNC_CPU;
 	if (op & (BUS_DMASYNC_PREWRITE))
		iodir |= IO_SYNC_DEVICE;

	if ((op & (BUS_DMASYNC_PREREAD|BUS_DMASYNC_POSTWRITE)) != 0)
		membar(Sync);

        /* nothing to be done */
	if (!iodir) 
		return;

	HIM_LOCK(him);
	SLIST_FOREACH(r, &map->dm_reslist, dr_link) {
		va = (vm_offset_t)BDR_START(r) + r->dr_offset ;
		len = r->dr_used;
		while (len > 0) {
			if ((err = hv_pci_iommu_getmap(him->him_handle,
			    VA_TO_TSBID(him, va), &ioattr, &ra))) {
				if (err != H_ENOMAP)
					printf("failed to _g=etmap: err: %ld, handle: %#lx, tsbid: %#lx\n", 
					       err, him->him_handle, VA_TO_TSBID(him, va));
				continue;
			}
			if ((err = hv_pci_dma_sync(him->him_handle, ra,
			    ulmin(len, (trunc_io_page(ra) + IO_PAGE_SIZE) - ra),
			    iodir, &synced))) {
				printf("failed to dma_sync: err: %ld, handle: %#lx, ra: %#lx, len: %#lx, dir: %d\n",
				    err, him->him_handle, ra, ulmin(len,
				    (trunc_io_page(ra) + IO_PAGE_SIZE) - ra),
				    iodir);
				synced = ulmin(len, (trunc_io_page(ra) + IO_PAGE_SIZE) - ra);
				printf("err: %ld", hv_pci_iommu_getmap(him->him_handle, VA_TO_TSBID(him, va),
								     &ioattr, &raddr));
				printf(", ioattr: %d, raddr: %#lx\n", ioattr, raddr);
			}
			va += synced;
			len -= synced;
		}
	}
	HIM_UNLOCK(him);

	if ((op & BUS_DMASYNC_PREWRITE) != 0)
		membar(Sync);
}

struct bus_dma_methods hviommu_dma_methods = {
	.dm_dmamap_create = hviommu_dvmamap_create,
	.dm_dmamap_destroy = hviommu_dvmamap_destroy,
	.dm_dmamap_load = hviommu_dvmamap_load,
	.dm_dmamap_load_mbuf = hviommu_dvmamap_load_mbuf,
	.dm_dmamap_load_mbuf_sg = hviommu_dvmamap_load_mbuf_sg,
	.dm_dmamap_load_uio = hviommu_dvmamap_load_uio,
	.dm_dmamap_unload = hviommu_dvmamap_unload,
	.dm_dmamap_sync = hviommu_dvmamap_sync,
	.dm_dmamem_alloc = hviommu_dvmamem_alloc,
	.dm_dmamem_free = hviommu_dvmamem_free,
};
