/*-
 * Copyright (c) 1997, 1998 Justin T. Gibbs.
 * Copyright (c) 2002 by Thomas Moestl <tmm@FreeBSD.org>.
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE
 * USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	from: FreeBSD: src/sys/i386/i386/busdma_machdep.c,v 1.25 2002/01/05
 *
 * $FreeBSD$
 */

#ifndef	_MACHINE_BUS_PRIVATE_H_
#define	_MACHINE_BUS_PRIVATE_H_

#include <sys/queue.h>

/*
 * Helpers
 */
int sparc64_bus_mem_map(bus_space_tag_t, bus_space_handle_t, bus_size_t,
    int, vm_offset_t, void **);
int sparc64_bus_mem_unmap(void *, bus_size_t);
bus_space_handle_t sparc64_fake_bustag(int, bus_addr_t, struct bus_space_tag *);

/*
 * This is more or less arbitrary, except for the stack space consumed by
 * the segments array. Choose more than ((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1),
 * since in practice we could be map pages more than once.
 */
#define	BUS_DMAMAP_NSEGS	64

struct bus_dmamap_res {
	struct resource		*dr_res;
	bus_size_t		dr_used;
	SLIST_ENTRY(bus_dmamap_res)	dr_link;
};

/*
 * Callers of the bus_dma interfaces must always protect their tags and maps
 * appropriately against concurrent access. However, when a map is on a LRU
 * queue, there is a second access path to it; for this case, the locking rules
 * are given in the parenthesized comments below:
 *	q - locked by the mutex protecting the queue.
 *	p - private to the owner of the map, no access through the queue.
 *	* - comment refers to pointer target.
 * Only the owner of the map is allowed to insert the map into a queue. Removal
 * and repositioning (i.e. temporal removal and reinsertion) is allowed to all
 * if the queue lock is held.
 */
struct bus_dmamap {
	TAILQ_ENTRY(bus_dmamap)	dm_maplruq;		/* (q) */
	SLIST_HEAD(, bus_dmamap_res)	dm_reslist;	/* (q, *q) */
	int			dm_onq;			/* (q) */
	int			dm_flags;		/* (p) */
};

/* Flag values. */
#define	DMF_LOADED	1	/* Map is loaded */
#define	DMF_COHERENT	2	/* Coherent mapping requested */

int sparc64_dma_alloc_map(bus_dma_tag_t dmat, bus_dmamap_t *mapp);
void sparc64_dma_free_map(bus_dma_tag_t dmat, bus_dmamap_t map);

/*
 * XXX: This is a kluge. It would be better to handle dma tags in a hierarchical
 * way, and have a BUS_GET_DMA_TAG(); however, since this is not currently the
 * case, save a root tag in the relevant bus attach function and use that.
 */
extern bus_dma_tag_t sparc64_root_dma_tag;

#endif /* !_MACHINE_BUS_PRIVATE_H_ */
