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

#ifdef IOMMU_DEBUG
#define IDB_BUSDMA	0x1
#define IDB_IOMMU	0x2
#define IDB_INFO	0x4
#define	IDB_SYNC	0x8
int iommudebug = 0xff;
#define DPRINTF(l, s)   do { if (iommudebug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

MALLOC_DEFINE(M_IOMMU, "dvmamem", "IOMMU DVMA Buffers");

#define iommu_strbuf_flush(i,v) do {					\
	if ((i)->is_sb[0] != 0)						\
		IOMMU_WRITE8((i), is_sb[0], ISR_PGFLUSH, (v));		\
	if ((i)->is_sb[1] != 0)						\
		IOMMU_WRITE8((i), is_sb[1], ISR_PGFLUSH, (v));		\
} while (0)

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

static	int iommu_strbuf_flush_done(struct iommu_state *);
#ifdef IOMMU_DIAG
static 	void iommu_diag(struct iommu_state *, vm_offset_t va);
#endif

/*
 * LRU queue handling for lazy resource allocation.
 */
static STAILQ_HEAD(, bus_dmamap) iommu_maplruq =
   STAILQ_HEAD_INITIALIZER(iommu_maplruq);

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
 * initialise the UltraSPARC IOMMU (SBUS or PCI):
 *	- allocate and setup the iotsb.
 *	- enable the IOMMU
 *	- initialise the streaming buffers (if they exist)
 *	- create a private DVMA map.
 */
void
iommu_init(char *name, struct iommu_state *is, int tsbsize, u_int32_t iovabase)
{
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

	/*
	 * Allocate memory for I/O pagetables.  They need to be physically
	 * contiguous.
	 */
	size = PAGE_SIZE << is->is_tsbsize;
	is->is_tsb = contigmalloc(size, M_DEVBUF, M_NOWAIT, 0, ~0UL, PAGE_SIZE,
	    0);
	if (is->is_tsb == 0)
		panic("iommu_init: no memory");
	is->is_ptsb = pmap_kextract((vm_offset_t)is->is_tsb);

	bzero(is->is_tsb, size);

#ifdef IOMMU_DEBUG
	if (iommudebug & IDB_INFO) {
		printf("iommu regs at offsets: cr=%#lx tsb=%#lx flush=%#lx\n",
		    (u_long)is->is_iommu + IMR_CTL,
		    (u_long)is->is_iommu + IMR_TSB,
		    (u_long)is->is_iommu + IMR_FLUSH);
		printf("iommu cr=%lx tsb=%lx\n",
		    (u_long)IOMMU_READ8(is, is_iommu, IMR_CTL),
		    (u_long)IOMMU_READ8(is, is_iommu, IMR_TSB))
		printf("TSB base %p phys %lx\n", (void *)is->is_tsb,
		    (unsigned long)is->is_ptsb);
	}
#endif

	/*
	 * Initialize streaming buffer, if it is there.
	 */
	if (is->is_sb[0] != 0 || is->is_sb[1] != 0) {
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

	/*
	 * Now all the hardware's working we need to setup dvma resource
	 * management.
	 */
	printf("DVMA map: %#lx to %#lx\n",
	    is->is_dvmabase, is->is_dvmabase +
	    (size << (IO_PAGE_SHIFT - IOTTE_SHIFT)) - 1);

	is->is_dvma_rman.rm_type = RMAN_ARRAY;
	is->is_dvma_rman.rm_descr = "DVMA Memory";
	if (rman_init(&is->is_dvma_rman) != 0 ||
	    rman_manage_region(&is->is_dvma_rman,
	    is->is_dvmabase >> IO_PAGE_SHIFT,
	    (is->is_dvmabase +
	    (size << (IO_PAGE_SHIFT - IOTTE_SHIFT))) >> IO_PAGE_SHIFT))
		panic("iommu_init: can't initialize dvma rman");
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

	IOMMU_WRITE8(is, is_iommu, IMR_TSB, is->is_ptsb);
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

	/* Is the streamcache flush really needed? */
	if (is->is_sb[0] != 0 || is->is_sb[1] != 0) {
		iommu_strbuf_flush(is, va);
		iommu_strbuf_flush_done(is);
	}
	DPRINTF(IDB_IOMMU, ("Clearing TSB slot %d for va %p\n",
	    (int)IOTSBSLOT(va), (void *)(u_long)va));
	is->is_tsb[IOTSBSLOT(va)] = tte;
	IOMMU_WRITE8(is, is_iommu, IMR_FLUSH, va);
	DPRINTF(IDB_IOMMU, ("iommu_enter: va %lx pa %lx TSB[%lx]@%p=%lx\n",
	    va, (long)pa, (u_long)IOTSBSLOT(va),
	    (void *)(u_long)&is->is_tsb[IOTSBSLOT(va)],
	    (u_long)tte));
#ifdef IOMMU_DIAG
	iommu_diag(is, va);
#endif
}

/*
 * iommu_remove: removes mappings created by iommu_enter
 * Only demap from IOMMU if flag is set.
 *
 * XXX: this function needs better internal error checking.
 */
void
iommu_remove(struct iommu_state *is, vm_offset_t va, vm_size_t len)
{

#ifdef IOMMU_DIAG
	iommu_diag(is, va);
#endif
#ifdef DIAGNOSTIC
	if (va < is->is_dvmabase)
		panic("iommu_remove: va 0x%lx not in DVMA space", (u_long)va);
	if ((long)(va + len) < (long)va) {
		panic("iommu_remove: va 0x%lx + len 0x%lx wraps",
		    (long)va, (long)len);
	}
	if (len & ~0xfffffff)
		panic("iommu_remove: ridiculous len 0x%lx", (u_long)len);
#endif

	va = trunc_io_page(va);
	DPRINTF(IDB_IOMMU, ("iommu_remove: va %lx TSB[%lx]@%p\n",
	    va, (u_long)IOTSBSLOT(va),
	    &is->is_tsb[IOTSBSLOT(va)]));
	while (len > 0) {
		DPRINTF(IDB_IOMMU, ("iommu_remove: clearing TSB slot %d for va "
		    "%p size %lx\n", (int)IOTSBSLOT(va),
		    (void *)(u_long)va, (u_long)len));
		if (is->is_sb[0] != 0 || is->is_sb[1] != 0) {
			DPRINTF(IDB_IOMMU, ("iommu_remove: flushing va %p "
			    "TSB[%lx]@%p=%lx, %lu bytes left\n",
			    (void *)(u_long)va,
			    (long)IOTSBSLOT(va),
			    (void *)(u_long)&is->is_tsb[
				    IOTSBSLOT(va)],
			    (long)(is->is_tsb[IOTSBSLOT(va)]),
			    (u_long)len));
			iommu_strbuf_flush(is, va);
			if (len <= IO_PAGE_SIZE)
				iommu_strbuf_flush_done(is);
			DPRINTF(IDB_IOMMU, ("iommu_remove: flushed va %p "
			    "TSB[%lx]@%p=%lx, %lu bytes left\n",
			    (void *)(u_long)va,
			    (long)IOTSBSLOT(va),
			    (void *)(u_long)&is->is_tsb[
				    IOTSBSLOT(va)],
			    (long)(is->is_tsb[IOTSBSLOT(va)]),
			    (u_long)len));
		}

		if (len <= IO_PAGE_SIZE)
			len = 0;
		else
			len -= IO_PAGE_SIZE;

		is->is_tsb[IOTSBSLOT(va)] = 0;
		IOMMU_WRITE8(is, is_iommu, IMR_FLUSH, va);
		va += IO_PAGE_SIZE;
	}
}

void
iommu_decode_fault(struct iommu_state *is, vm_offset_t phys)
{
	bus_addr_t va;
	long idx;

	idx = phys - is->is_ptsb;
	if (phys < is->is_ptsb ||
	    idx > (PAGE_SIZE << is->is_tsbsize))
		return;
	va = is->is_dvmabase +
	    (((bus_addr_t)idx >> IOTTE_SHIFT) << IO_PAGE_SHIFT);
	printf("IOMMU fault virtual address %#lx\n", (u_long)va);
}

static int
iommu_strbuf_flush_done(struct iommu_state *is)
{
	struct timeval cur, end;
	int i;

	if (is->is_sb[0] == 0 && is->is_sb[1] == 0)
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

	microtime(&cur);
	end.tv_sec = 0;
	end.tv_usec = 500000;
	timevaladd(&end, &cur);

	DPRINTF(IDB_IOMMU, ("iommu_strbuf_flush_done: flush = %lx, %lx at "
	    "va = %p pa = %lx now=%lx:%lx\n", (long)*is->is_flushva[0],
	    (long)*is->is_flushva[1], is->is_flushva[0],
	    (long)is->is_flushpa[0], cur.tv_sec, cur.tv_usec));
	while ((!*is->is_flushva[0] || !*is->is_flushva[1]) &&
	    timevalcmp(&cur, &end, <=))
		microtime(&cur);

#ifdef DIAGNOSTIC
	if (!*is->is_flushva[0] || !*is->is_flushva[1]) {
		panic("iommu_strbuf_flush_done: flush timeout %ld, %ld at %#lx",
		    *is->is_flushva[0], *is->is_flushva[1], is->is_flushpa[0]);
	}
#endif
	DPRINTF(IDB_IOMMU, ("iommu_strbuf_flush_done: flushed\n"));
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
	map->res = rman_reserve_resource_bound(&is->is_dvma_rman, 0L,
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
	if (buflen > dt->maxsize) {
		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_load(): error %d > %d -- "
		     "map size exceeded!\n", (int)buflen, (int)map->buflen));
		return (EINVAL);
	}

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

		DPRINTF(IDB_BUSDMA,
		    ("iommu_dvmamap_load: map %p loading va %p "
			    "dva %lx at pa %lx\n",
			    map, (void *)vaddr, (long)dvmaddr,
			    (long)(curaddr & ~(PAGE_SIZE - 1))));
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
	if (map->buflen == 0 || map->start == 0) {
		panic("iommu_dvmamap_unload: map = %p, len = %d, addr = %lx\n",
		    map, (int)map->buflen, (unsigned long)map->start);
	}

	DPRINTF(IDB_BUSDMA,
	    ("iommu_dvmamap_unload: map %p removing va %lx size "
		"%lx\n", map, (long)map->start, (long)map->buflen));
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

	va = (vm_offset_t)map->buf;
	len = map->buflen;
	if ((op & BUS_DMASYNC_PREREAD) != 0) {
		DPRINTF(IDB_SYNC,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		    "BUS_DMASYNC_PREREAD\n", (void *)(u_long)va, (u_long)len));
		membar(Sync);
	}
	if ((op & BUS_DMASYNC_POSTREAD) != 0) {
		DPRINTF(IDB_SYNC,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		    "BUS_DMASYNC_POSTREAD\n", (void *)(u_long)va, (u_long)len));
		/* if we have a streaming buffer, flush it here first */
		if (is->is_sb[0] != 0 || is->is_sb[1] != 0) {
			while (len > 0) {
				DPRINTF(IDB_BUSDMA,
				    ("iommu_dvmamap_sync: flushing va %p, %lu "
				    "bytes left\n", (void *)(u_long)va,
				    (u_long)len));
				iommu_strbuf_flush(is, va);
				if (len <= IO_PAGE_SIZE) {
					iommu_strbuf_flush_done(is);
					len = 0;
				} else
					len -= IO_PAGE_SIZE;
				va += IO_PAGE_SIZE;
			}
		}
	}
	if ((op & BUS_DMASYNC_PREWRITE) != 0) {
		DPRINTF(IDB_SYNC,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		     "BUS_DMASYNC_PREWRITE\n", (void *)(u_long)va, (u_long)len));
		/* if we have a streaming buffer, flush it here first */
		if (is->is_sb[0] != 0 || is->is_sb[1] != 0) {
			while (len > 0) {
				DPRINTF(IDB_BUSDMA,
				    ("iommu_dvmamap_sync: flushing va %p, %lu "
				    "bytes left\n", (void *)(u_long)va,
				    (u_long)len));
				iommu_strbuf_flush(is, va);
				if (len <= IO_PAGE_SIZE) {
					iommu_strbuf_flush_done(is);
					len = 0;
				} else
					len -= IO_PAGE_SIZE;
				va += IO_PAGE_SIZE;
			}
		}
		membar(Sync);
	}
	if ((op & BUS_DMASYNC_POSTWRITE) != 0) {
		DPRINTF(IDB_SYNC,
		    ("iommu_dvmamap_sync: syncing va %p len %lu "
		    "BUS_DMASYNC_POSTWRITE\n", (void *)(u_long)va,
		    (u_long)len));
		/* Nothing to do */
	}
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
	printf("iommu_diag: tte entry %#lx",
	    is->is_tsb[IOTSBSLOT(va)]);
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
