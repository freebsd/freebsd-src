/*
 * Copyright (c) 1999, 2000 Matthew R. Green
 * Copyright (c) 2001 Thomas Moestl
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
 *
 *	from: NetBSD: sbus.c,v 1.13 1999/05/23 07:24:02 mrg Exp
 *	from: @(#)sbus.c	8.1 (Berkeley) 6/11/93
 *	from: NetBSD: iommu.c,v 1.42 2001/08/06 22:02:58 eeh Exp
 *
 * $FreeBSD$
 */

/*
 * UltraSPARC IOMMU support; used by both the sbus and pci code.
 * Currently, the IOTSBs are synchronized, because determining the bus the map
 * is to be loaded for is not possible with the current busdma code.
 * The code is structured so that the IOMMUs can be easily divorced when that
 * is fixed.
 */
#include "opt_iommu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>
#include <machine/bus_private.h>
#include <machine/iommureg.h>
#include <machine/pmap.h>
#include <machine/resource.h>

#include <sys/rman.h>

#include <machine/iommuvar.h>

MALLOC_DEFINE(M_IOMMU, "dvmamem", "IOMMU DVMA Buffers");

static	int iommu_strbuf_flush_sync(struct iommu_state *);
#ifdef IOMMU_DIAG
static 	void iommu_diag(struct iommu_state *, vm_offset_t va);
#endif

/*
 * The following 4 variables need to be moved to the per-IOMMU state once
 * the IOTSBs are divorced.
 * LRU queue handling for lazy resource allocation.
 */
static STAILQ_HEAD(, bus_dmamap) iommu_maplruq =
   STAILQ_HEAD_INITIALIZER(iommu_maplruq);

/* DVMA memory rman. */
static struct rman iommu_dvma_rman;

/* Virtual and physical address of the TSB. */
static u_int64_t *iommu_tsb;
static vm_offset_t iommu_ptsb;

/* List of all IOMMUs. */
static STAILQ_HEAD(, iommu_state) iommu_insts =
   STAILQ_HEAD_INITIALIZER(iommu_insts);

/*
 * Helpers. Some of these take unused iommu states as parameters, to ease the
 * transition to divorced TSBs.
 */
#define	IOMMU_READ8(is, reg, off) 					\
	bus_space_read_8((is)->is_bustag, (is)->is_bushandle, 		\
	    (is)->reg + (off))
#define	IOMMU_WRITE8(is, reg, off, v)					\
	bus_space_write_8((is)->is_bustag, (is)->is_bushandle, 		\
	    (is)->reg + (off), (v))

/*
 * Always overallocate one page; this is needed to handle alignment of the
 * buffer, so it makes sense using a lazy allocation scheme.
 */
#define	IOMMU_SIZE_ROUNDUP(sz)						\
	(round_io_page(sz) + IO_PAGE_SIZE)

#define	IOMMU_SET_TTE(is, va, tte)					\
	(iommu_tsb[IOTSBSLOT(va)] = (tte))

static __inline void
iommu_tlb_flush(struct iommu_state *is, bus_addr_t va)
{
	struct iommu_state *it;

	/*
	 * Since the TSB is shared for now, the TLBs of all IOMMUs
	 * need to be flushed.
	 */
	STAILQ_FOREACH(it, &iommu_insts, is_link)
		IOMMU_WRITE8(it, is_iommu, IMR_FLUSH, va);
}


#define	IOMMU_HAS_SB(is)						\
	((is)->is_sb[0] != 0 || (is)->is_sb[1] != 0)

static __inline void
iommu_strbuf_flushpg(struct iommu_state *is, bus_addr_t va)
{
	int i;

	for (i = 0; i < 2; i++) {
		if (is->is_sb[i] != 0)
			IOMMU_WRITE8(is, is_sb[i], ISR_PGFLUSH, va);
	}
}

static __inline void
iommu_strbuf_flush(struct iommu_state *is, bus_addr_t va, int sync)
{
	struct iommu_state *it;

	/*
	 * Need to flush the streaming buffers of all IOMMUs, we cannot
	 * determine which one was used for the transaction.
	 */
	STAILQ_FOREACH(it, &iommu_insts, is_link) {
		iommu_strbuf_flushpg(it, va);
		if (sync)
			iommu_strbuf_flush_sync(it);
	}
}

/*
 * LRU queue handling for lazy resource allocation.
 */
static __inline void
iommu_map_insq(bus_dmamap_t map)
{

	if (!map->onq && map->dvmaresv != 0) {
		STAILQ_INSERT_TAIL(&iommu_maplruq, map, maplruq);
		map->onq = 1;
	}
}

static __inline void
iommu_map_remq(bus_dmamap_t map)
{

	if (map->onq)
		STAILQ_REMOVE(&iommu_maplruq, map, bus_dmamap, maplruq);
	map->onq = 0;
}

/*
 * initialise the UltraSPARC IOMMU (SBus or PCI):
 *	- allocate and setup the iotsb.
 *	- enable the IOMMU
 *	- initialise the streaming buffers (if they exist)
 *	- create a private DVMA map.
 */
void
iommu_init(char *name, struct iommu_state *is, int tsbsize, u_int32_t iovabase,
    int resvpg)
{
	struct iommu_state *first;
	vm_size_t size;
	vm_offset_t offs;
	int i;

	/*
	 * Setup the iommu.
	 *
	 * The sun4u iommu is part of the SBUS or PCI controller so we
	 * will deal with it here..
	 *
	 * The IOMMU address space always ends at 0xffffe000, but the starting
	 * address depends on the size of the map.  The map size is 1024 * 2 ^
	 * is->is_tsbsize entries, where each entry is 8 bytes.  The start of
	 * the map can be calculated by (0xffffe000 << (8 + is->is_tsbsize)).
	 */
	is->is_cr = (tsbsize << IOMMUCR_TSBSZ_SHIFT) | IOMMUCR_EN;
	is->is_tsbsize = tsbsize;
	is->is_dvmabase = iovabase;
	if (iovabase == -1)
		is->is_dvmabase = IOTSB_VSTART(is->is_tsbsize);

	size = PAGE_SIZE << is->is_tsbsize;
	printf("DVMA map: %#lx to %#lx\n",
	    is->is_dvmabase, is->is_dvmabase +
	    (size << (IO_PAGE_SHIFT - IOTTE_SHIFT)) - 1);

	if (STAILQ_EMPTY(&iommu_insts)) {
		/*
		 * First IOMMU to be registered; set up resource mamangement
		 * and allocate TSB memory.
		 */
		iommu_dvma_rman.rm_type = RMAN_ARRAY;
		iommu_dvma_rman.rm_descr = "DVMA Memory";
		if (rman_init(&iommu_dvma_rman) != 0 ||
		    rman_manage_region(&iommu_dvma_rman,
		    (is->is_dvmabase >> IO_PAGE_SHIFT) + resvpg,
		    (is->is_dvmabase + (size <<
		     (IO_PAGE_SHIFT - IOTTE_SHIFT))) >> IO_PAGE_SHIFT) != 0)
			panic("iommu_init: can't initialize dvma rman");
		/*
		 * Allocate memory for I/O page tables.  They need to be
		 * physically contiguous.
		 */
		iommu_tsb = contigmalloc(size, M_DEVBUF, M_NOWAIT, 0, ~0UL,
		    PAGE_SIZE, 0);
		if (iommu_tsb == 0)
			panic("iommu_init: contigmalloc failed");
		iommu_ptsb = pmap_kextract((vm_offset_t)iommu_tsb);
		bzero(iommu_tsb, size);
	} else {
		/*
		 * Not the first IOMMU; just check that the parameters match
		 * those of the first one.
		 */
		first = STAILQ_FIRST(&iommu_insts);
		if (is->is_tsbsize != first->is_tsbsize ||
		    is->is_dvmabase != first->is_dvmabase) {
			panic("iommu_init: secondary IOMMU state does not "
			    "match primary");
		}
	}
	STAILQ_INSERT_TAIL(&iommu_insts, is, is_link);

	/*
	 * Initialize streaming buffer, if it is there.
	 */
	if (IOMMU_HAS_SB(is)) {
		/*
		 * Find two 64-byte blocks in is_flush that are aligned on
		 * a 64-byte boundary for flushing.
		 */
		offs = roundup2((vm_offset_t)is->is_flush,
		    STRBUF_FLUSHSYNC_NBYTES);
		for (i = 0; i < 2; i++, offs += STRBUF_FLUSHSYNC_NBYTES) {
			is->is_flushva[i] = (int64_t *)offs;
			is->is_flushpa[i] = pmap_kextract(offs);
		}
	}

	/*
	 * now actually start up the IOMMU
	 */
	iommu_reset(is);
}

/*
 * Streaming buffers don't exist on the UltraSPARC IIi; we should have
 * detected that already and disabled them.  If not, we will notice that
 * they aren't there when the STRBUF_EN bit does not remain.
 */
void
iommu_reset(struct iommu_state *is)
{
	int i;

	IOMMU_WRITE8(is, is_iommu, IMR_TSB, iommu_ptsb);
	/* Enable IOMMU in diagnostic mode */
	IOMMU_WRITE8(is, is_iommu, IMR_CTL, is->is_cr | IOMMUCR_DE);

	for (i = 0; i < 2; i++) {
		if (is->is_sb[i] != 0) {
			/* Enable diagnostics mode? */
			IOMMU_WRITE8(is, is_sb[i], ISR_CTL, STRBUF_EN);

			/* No streaming buffers? Disable them */
			if (IOMMU_READ8(is, is_sb[i], ISR_CTL) == 0)
				is->is_sb[i] = 0;
		}
	}
}

/*
 * Here are the iommu control routines.
 */
void
iommu_enter(struct iommu_state *is, vm_offset_t va, vm_offset_t pa, int flags)
{
	int64_t tte;

#ifdef DIAGNOSTIC
	if (va < is->is_dvmabase)
		panic("iommu_enter: va %#lx not in DVMA space", va);
#endif

	tte = MAKEIOTTE(pa, !(flags & BUS_DMA_NOWRITE),
	    !(flags & BUS_DMA_NOCACHE), (flags & BUS_DMA_STREAMING));

	iommu_strbuf_flush(is, va, 1);
	IOMMU_SET_TTE(is, va, tte);
	iommu_tlb_flush(is, va);
#ifdef IOMMU_DIAG
	iommu_diag(is, va);
#endif
}

/*
 * iommu_remove: removes mappings created by iommu_enter
 * Only demap from IOMMU if flag is set.
 */
void
iommu_remove(struct iommu_state *is, vm_offset_t va, vm_size_t len)
{

#ifdef IOMMU_DIAG
	iommu_diag(is, va);
#endif

	KASSERT(va >= is->is_dvmabase,
	    ("iommu_remove: va 0x%lx not in DVMA space", (u_long)va));
	KASSERT(va + len >= va,
	    ("iommu_remove: va 0x%lx + len 0x%lx wraps", (long)va, (long)len));

	va = trunc_io_page(va);
	while (len > 0) {
		iommu_strbuf_flush(is, va, len <= IO_PAGE_SIZE);
		len -= ulmin(len, IO_PAGE_SIZE);
		IOMMU_SET_TTE(is, va, 0);
		iommu_tlb_flush(is, va);
		va += IO_PAGE_SIZE;
	}
}

void
iommu_decode_fault(struct iommu_state *is, vm_offset_t phys)
{
	bus_addr_t va;
	long idx;

	idx = phys - iommu_ptsb;
	if (phys < iommu_ptsb ||
	    idx > (PAGE_SIZE << is->is_tsbsize))
		return;
	va = is->is_dvmabase +
	    (((bus_addr_t)idx >> IOTTE_SHIFT) << IO_PAGE_SHIFT);
	printf("IOMMU fault virtual address %#lx\n", (u_long)va);
}

static int
iommu_strbuf_flush_sync(struct iommu_state *is)
{
	struct timeval cur, end;
	int i;

	if (!IOMMU_HAS_SB(is))
		return (0);

	/*
	 * Streaming buffer flushes:
	 *
	 *   1 Tell strbuf to flush by storing va to strbuf_pgflush.  If
	 *     we're not on a cache line boundary (64-bits):
	 *   2 Store 0 in flag
	 *   3 Store pointer to flag in flushsync
	 *   4 wait till flushsync becomes 0x1
	 *
	 * If it takes more than .5 sec, something
	 * went wrong.
	 */
	*is->is_flushva[0] = 1;
	*is->is_flushva[1] = 1;
	membar(StoreStore);
	for (i = 0; i < 2; i++) {
		if (is->is_sb[i] != 0) {
			*is->is_flushva[i] = 0;
			IOMMU_WRITE8(is, is_sb[i], ISR_FLUSHSYNC,
			    is->is_flushpa[i]);
		}
	}

	microuptime(&cur);
	end.tv_sec = 0;
	end.tv_usec = 500000;
	timevaladd(&end, &cur);

	while ((!*is->is_flushva[0] || !*is->is_flushva[1]) &&
	    timevalcmp(&cur, &end, <=))
		microuptime(&cur);

#ifdef DIAGNOSTIC
	if (!*is->is_flushva[0] || !*is->is_flushva[1]) {
		panic("iommu_strbuf_flush_done: flush timeout %ld, %ld at %#lx",
		    *is->is_flushva[0], *is->is_flushva[1], is->is_flushpa[0]);
	}
#endif
	return (*is->is_flushva[0] && *is->is_flushva[1]);
}

/* Allocate DVMA virtual memory for a map. */
static int
iommu_dvma_valloc(bus_dma_tag_t t, struct iommu_state *is, bus_dmamap_t map,
    bus_size_t size)
{
	bus_size_t align, bound, sgsize;

	/*
	 * If a boundary is specified, a map cannot be larger than it; however
	 * we do not clip currently, as that does not play well with the lazy
	 * allocation code.
	 * Alignment to a page boundary is always enforced.
	 */
	align = (t->alignment + IO_PAGE_MASK) >> IO_PAGE_SHIFT;
	sgsize = round_io_page(size) >> IO_PAGE_SHIFT;
	if (t->boundary > 0 && t->boundary < IO_PAGE_SIZE)
		panic("iommu_dvmamap_load: illegal boundary specified");
	bound = ulmax(t->boundary >> IO_PAGE_SHIFT, 1);
	map->dvmaresv = 0;
	map->res = rman_reserve_resource_bound(&iommu_dvma_rman, 0L,
	    t->lowaddr, sgsize, bound >> IO_PAGE_SHIFT,
	    RF_ACTIVE | rman_make_alignment_flags(align), NULL);
	if (map->res == NULL)
		return (ENOMEM);

	map->start = rman_get_start(map->res) * IO_PAGE_SIZE;
	map->dvmaresv = size;
	iommu_map_insq(map);
	return (0);
}

/* Free DVMA virtual memory for a map. */
static void
iommu_dvma_vfree(bus_dmamap_t map)
{

	iommu_map_remq(map);
	if (map->res != NULL && rman_release_resource(map->res) != 0)
		printf("warning: DVMA space lost\n");
	map->res = NULL;
	map->dvmaresv = 0;
}

int
iommu_dvmamem_alloc(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    void **vaddr, int flags, bus_dmamap_t *mapp)
{
	int error;

	/*
	 * XXX: This will break for 32 bit transfers on machines with more than
	 * 16G (2 << 34 bytes) of memory.
	 */
	if ((error = sparc64_dmamem_alloc_map(dt, mapp)) != 0)
		return (error);
	if ((*vaddr = malloc(dt->maxsize, M_IOMMU,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL) {
		error = ENOMEM;
		goto failm;
	}
	/*
	 * Try to preallocate DVMA memory. If this fails, it is retried at load
	 * time.
	 */
	iommu_dvma_valloc(dt, is, *mapp, IOMMU_SIZE_ROUNDUP(dt->maxsize));
	return (0);

failm:
	sparc64_dmamem_free_map(dt, *mapp);
	return (error);
}

void
iommu_dvmamem_free(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    void *vaddr, bus_dmamap_t map)
{

	iommu_dvma_vfree(map);
	sparc64_dmamem_free_map(dt, map);
	free(vaddr, M_IOMMU);
}

int
iommu_dvmamap_create(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    int flags, bus_dmamap_t *mapp)
{
	int error;

	if ((error = sparc64_dmamap_create(pt->parent, dt, flags, mapp)) != 0)
		return (error);
	KASSERT((*mapp)->res == NULL,
	    ("iommu_dvmamap_create: hierarchy botched"));
	/*
	 * Preallocate DMVA memory; if this fails now, it is retried at load
	 * time.
	 * Clamp preallocation to BUS_SPACE_MAXSIZE. In some situations we can
	 * handle more; that case is handled by reallocating at map load time.
	 */
	iommu_dvma_valloc(dt, is, *mapp,
	    ulmin(IOMMU_SIZE_ROUNDUP(dt->maxsize), BUS_SPACE_MAXSIZE));
	return (0);
}

int
iommu_dvmamap_destroy(bus_dma_tag_t pt, bus_dma_tag_t dt,
    struct iommu_state *is, bus_dmamap_t map)
{

	iommu_dvma_vfree(map);
	return (sparc64_dmamap_destroy(pt->parent, dt, map));
}

#define BUS_DMAMAP_NSEGS ((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1)

/*
 * IOMMU DVMA operations, common to SBUS and PCI.
 */
int
iommu_dvmamap_load(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    bus_dmamap_t map, void *buf, bus_size_t buflen, bus_dmamap_callback_t *cb,
    void *cba, int flags)
{
#ifdef __GNUC__
	bus_dma_segment_t sgs[dt->nsegments];
#else
	bus_dma_segment_t sgs[BUS_DMAMAP_NSEGS];
#endif
	bus_dmamap_t tm;
	bus_size_t sgsize, fsize, maxsize;
	vm_offset_t curaddr;
	u_long dvmaddr;
	vm_offset_t vaddr;
	int error, sgcnt;

	if (map->buflen != 0) {
		/* Already in use?? */
#ifdef DIAGNOSTIC
		printf("iommu_dvmamap_load: map still in use\n");
#endif
		bus_dmamap_unload(dt, map);
	}
	if (buflen > dt->maxsize)
		return (EINVAL);

	maxsize = IOMMU_SIZE_ROUNDUP(buflen);
	if (maxsize > map->dvmaresv) {
		/*
		 * Need to allocate resources now; free any old allocation
		 * first.
		 */
		fsize = map->dvmaresv;
		iommu_dvma_vfree(map);
		while (iommu_dvma_valloc(dt, is, map, maxsize) == ENOMEM &&
		    !STAILQ_EMPTY(&iommu_maplruq)) {
			/*
			 * Free the allocated DVMA of a few tags until
			 * the required size is reached. This is an
			 * approximation to not have to call the allocation
			 * function too often; most likely one free run
			 * will not suffice if not one map was large enough
			 * itself due to fragmentation.
			 */
			do {
				tm = STAILQ_FIRST(&iommu_maplruq);
				if (tm == NULL)
					break;
				fsize += tm->dvmaresv;
				iommu_dvma_vfree(tm);
			} while (fsize < maxsize);
			fsize = 0;
		}
		if (map->dvmaresv < maxsize) {
			printf("DVMA allocation failed: needed %ld, got %ld\n",
			    maxsize, map->dvmaresv);
			return (ENOMEM);
		}
	}

	/* Busy the map by removing it from the LRU queue. */
	iommu_map_remq(map);

	vaddr = (vm_offset_t)buf;
	map->buf = buf;
	map->buflen = buflen;

	dvmaddr = map->start | (vaddr & IO_PAGE_MASK);
	sgcnt = -1;
	error = 0;
	for (; buflen > 0; ) {
		/*
		 * Get the physical address for this page.
		 */
		curaddr = pmap_kextract((vm_offset_t)vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = IO_PAGE_SIZE - ((u_long)vaddr & IO_PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		iommu_enter(is, trunc_io_page(dvmaddr), trunc_io_page(curaddr),
		    flags);

		if (sgcnt == -1 || sgs[sgcnt].ds_len + sgsize > dt->maxsegsz) {
			if (sgsize > dt->maxsegsz) {
				/* XXX: add fixup */
				panic("iommu_dvmamap_load: magsegsz too "
				    "small\n");
			}
			sgcnt++;
			if (sgcnt > dt->nsegments || sgcnt > BUS_DMAMAP_NSEGS) {
				error = ENOMEM;
				break;
			}
			sgs[sgcnt].ds_addr = dvmaddr;
			sgs[sgcnt].ds_len = sgsize;
		} else
			sgs[sgcnt].ds_len += sgsize;
		dvmaddr += sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}
	(*cb)(cba, sgs, sgcnt + 1, error);
	return (0);
}


void
iommu_dvmamap_unload(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    bus_dmamap_t map)
{

	/*
	 * If the resource is already deallocated, just pass to the parent
	 * tag.
	 */
	if (map->buflen == 0 || map->start == 0)
		return;

	iommu_remove(is, map->start, map->buflen);
	map->buflen = 0;
	iommu_map_insq(map);
	/* Flush the caches */
	sparc64_dmamap_unload(pt->parent, dt, map);
}

void
iommu_dvmamap_sync(bus_dma_tag_t pt, bus_dma_tag_t dt, struct iommu_state *is,
    bus_dmamap_t map, bus_dmasync_op_t op)
{
	vm_offset_t va;
	vm_size_t len;

	va = (vm_offset_t)map->start;
	len = map->buflen;
	if ((op & BUS_DMASYNC_PREREAD) != 0)
		membar(Sync);
	if ((op & (BUS_DMASYNC_POSTREAD | BUS_DMASYNC_PREWRITE)) != 0) {
		/* if we have a streaming buffer, flush it here first */
		while (len > 0) {
			iommu_strbuf_flush(is, va,
			    len <= IO_PAGE_SIZE);
			len -= ulmin(len, IO_PAGE_SIZE);
			va += IO_PAGE_SIZE;
		}
	}
	if ((op & BUS_DMASYNC_PREWRITE) != 0)
		membar(Sync);
	/* BUS_DMASYNC_POSTWRITE does not require any handling. */
}

#ifdef IOMMU_DIAG

#define	IOMMU_DTAG_VPNBITS	19
#define	IOMMU_DTAG_VPNMASK	((1 << IOMMU_DTAG_VPNBITS) - 1)
#define	IOMMU_DTAG_VPNSHIFT	13
#define IOMMU_DTAG_ERRBITS	3
#define	IOMMU_DTAG_ERRSHIFT	22
#define	IOMMU_DTAG_ERRMASK \
	(((1 << IOMMU_DTAG_ERRBITS) - 1) << IOMMU_DTAG_ERRSHIFT)

#define	IOMMU_DDATA_PGBITS	21
#define	IOMMU_DDATA_PGMASK	((1 << IOMMU_DDATA_PGBITS) - 1)
#define	IOMMU_DDATA_PGSHIFT	13
#define	IOMMU_DDATA_C		(1 << 28)
#define	IOMMU_DDATA_V		(1 << 30)

/*
 * Perform an IOMMU diagnostic access and print the tag belonging to va.
 */
static void
iommu_diag(struct iommu_state *is, vm_offset_t va)
{
	int i;
	u_int64_t tag, data;

	IOMMU_WRITE8(is, is_dva, 0, trunc_io_page(va));
	membar(StoreStore | StoreLoad);
	printf("iommu_diag: tte entry %#lx", iommu_tsb[IOTSBSLOT(va)]);
	if (is->is_dtcmp != 0) {
		printf(", tag compare register is %#lx\n"
		    IOMMU_READ8(is, is_dtcmp, 0));
	} else
		printf("\n");
	for (i = 0; i < 16; i++) {
		tag = IOMMU_READ8(is, is_dtag, i * 8);
		data = IOMMU_READ8(is, is_ddram, i * 8);
		printf("iommu_diag: tag %d: %#lx, vpn %#lx, err %lx; "
		    "data %#lx, pa %#lx, v %d, c %d\n", i,
		    tag, (tag & IOMMU_DTAG_VPNMASK) << IOMMU_DTAG_VPNSHIFT,
		    (tag & IOMMU_DTAG_ERRMASK) >> IOMMU_DTAG_ERRSHIFT, data,
		    (data & IOMMU_DDATA_PGMASK) << IOMMU_DDATA_PGSHIFT,
		    (data & IOMMU_DDATA_V) != 0, (data & IOMMU_DDATA_C) != 0);
	}
}

#endif /* IOMMU_DIAG */
