/*
 * Copyright (c) 2003
 *	Fraunhofer Institute for Open Communication Systems (FhG Fokus).
 * 	All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Author: Hartmut Brandt <harti@freebsd.org>
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <sys/mbpool.h>

MODULE_VERSION(libmbpool, 1);

/*
 * Memory is allocated as DMA-able pages. Each page is divided into a number
 * of equal chunks where the last 4 bytes of each chunk are occupied by
 * the page number and the chunk number. The caller must take these four
 * bytes into account when specifying the chunk size. Each page is mapped by
 * its own DMA map using the user specified DMA tag.
 *
 * Each chunk has a used and a card bit in the high bits of its page number.
 *  0    0	chunk is free and may be allocated
 *  1    1	chunk has been given to the interface
 *  0    1	chunk is traveling through the system
 *  1    0	illegal
 */
struct mbtrail {
	uint16_t	chunk;
	uint16_t	page;
};
#define	MBP_CARD	0x8000
#define	MBP_USED	0x4000
#define	MBP_PMSK	0x3fff		/* page number mask */
#define	MBP_CMSK	0x01ff		/* chunk number mask */

struct mbfree {
	SLIST_ENTRY(mbfree) link;	/* link on free list */
};

struct mbpage {
	bus_dmamap_t	map;		/* map for this page */
	bus_addr_t	phy;		/* physical address */
	void		*va;		/* the memory */
};

struct mbpool {
	const char	*name;		/* a name for this pool */
	bus_dma_tag_t	dmat;		/* tag for mapping */
	u_int		max_pages;	/* maximum number of pages */
	size_t		page_size;	/* size of each allocation */
	size_t		chunk_size;	/* size of each external mbuf */

	struct mtx	free_lock;	/* lock of free list */
	SLIST_HEAD(, mbfree) free_list;	/* free list */
	u_int		npages;		/* current number of pages */
	u_int		nchunks;	/* chunks per page */
	struct mbpage	pages[];	/* pages */
};

static MALLOC_DEFINE(M_MBPOOL, "mbpools", "mbuf pools");

/*
 * Make a trail pointer from a chunk pointer
 */
#define	C2T(P, C)	((struct mbtrail *)((char *)(C) + (P)->chunk_size - \
			    sizeof(struct mbtrail)))

/*
 * Make a free chunk pointer from a chunk number
 */
#define	N2C(P, PG, C)	((struct mbfree *)((char *)(PG)->va + \
			    (C) * (P)->chunk_size))

/*
 * Make/parse handles
 */
#define	HMAKE(P, C)	((((P) & MBP_PMSK) << 16) | ((C) << 7))
#define	HPAGE(H)	(((H) >> 16) & MBP_PMSK)
#define	HCHUNK(H)	(((H) >>  7) & MBP_CMSK)

/*
 * initialize a pool
 */
int
mbp_create(struct mbpool **pp, const char *name, bus_dma_tag_t dmat,
    u_int max_pages, size_t page_size, size_t chunk_size)
{
	u_int nchunks;

	if (max_pages > MBPOOL_MAX_MAXPAGES || chunk_size == 0)
		return (EINVAL);
	nchunks = page_size / chunk_size;
	if (nchunks == 0 || nchunks > MBPOOL_MAX_CHUNKS)
		return (EINVAL);

	(*pp) = malloc(sizeof(struct mbpool) +
	    max_pages * sizeof(struct mbpage),
	    M_MBPOOL, M_WAITOK | M_ZERO);

	(*pp)->name = name;
	(*pp)->dmat = dmat;
	(*pp)->max_pages = max_pages;
	(*pp)->page_size = page_size;
	(*pp)->chunk_size = chunk_size;
	(*pp)->nchunks = nchunks;

	SLIST_INIT(&(*pp)->free_list);
	mtx_init(&(*pp)->free_lock, name, NULL, 0);

	return (0);
}

/*
 * destroy a pool
 */
void
mbp_destroy(struct mbpool *p)
{
	u_int i;
	struct mbpage *pg;
#ifdef DIAGNOSTIC
	struct mbtrail *tr;
	u_int b;
#endif

	for (i = 0; i < p->npages; i++) {
		pg = &p->pages[i];
#ifdef DIAGNOSTIC
		for (b = 0; b < p->nchunks; b++) {
			tr = C2T(p, N2C(p, pg, b));
			if (tr->page & MBP_CARD)
				printf("%s: (%s) buf still on card"
				    " %u/%u\n", __func__, p->name, i, b);
			if (tr->page & MBP_USED)
				printf("%s: (%s) sbuf still in use"
				    " %u/%u\n", __func__, p->name, i, b);
		}
#endif
		bus_dmamap_unload(p->dmat, pg->map);
		bus_dmamem_free(p->dmat, pg->va, pg->map);
	}
	mtx_destroy(&p->free_lock);

	free(p, M_MBPOOL);
}

/*
 * Helper function when loading a one segment DMA buffer.
 */
static void
mbp_callback(void *arg, bus_dma_segment_t *segs, int nsegs, int error)
{
	if (error == 0)
		*(bus_addr_t *)arg = segs[0].ds_addr;
}

/*
 * Allocate a new page
 */
static void
mbp_alloc_page(struct mbpool *p)
{
	int error;
	struct mbpage *pg;
	u_int i;
	struct mbfree *f;
	struct mbtrail *t;

	if (p->npages == p->max_pages) {
#ifdef DIAGNOSTIC
		printf("%s: (%s) page limit reached %u\n", __func__,
		    p->name, p->max_pages);
#endif
		return;
	}
	pg = &p->pages[p->npages];

	error = bus_dmamem_alloc(p->dmat, &pg->va, BUS_DMA_NOWAIT, &pg->map);
	if (error != 0) {
		free(pg, M_MBPOOL);
		return;
	}

	error = bus_dmamap_load(p->dmat, pg->map, pg->va, p->page_size,
	    mbp_callback, &pg->phy, 0);
	if (error != 0) {
		bus_dmamem_free(p->dmat, pg->va, pg->map);
		free(pg, M_MBPOOL);
		return;
	}

	for (i = 0; i < p->nchunks; i++) {
		f = N2C(p, pg, i);
		t = C2T(p, f);
		t->page = p->npages;
		t->chunk = i;
		SLIST_INSERT_HEAD(&p->free_list, f, link);
	}

	p->npages++;
}

/*
 * allocate a chunk
 */
void *
mbp_alloc(struct mbpool *p, bus_addr_t *pap, uint32_t *hp)
{
	struct mbfree *cf;
	struct mbtrail *t;

	mtx_lock(&p->free_lock);
	if ((cf = SLIST_FIRST(&p->free_list)) == NULL) {
		mbp_alloc_page(p);
		cf = SLIST_FIRST(&p->free_list);
	}
	if (cf == NULL) {
		mtx_unlock(&p->free_lock);
		return (NULL);
	}
	SLIST_REMOVE_HEAD(&p->free_list, link);
	mtx_unlock(&p->free_lock);

	t = C2T(p, cf);

	*pap = p->pages[t->page].phy + t->chunk * p->chunk_size;
	*hp = HMAKE(t->page, t->chunk);

	t->page |= MBP_CARD | MBP_USED;

	return (cf);
}

/*
 * Free a chunk
 */
void
mbp_free(struct mbpool *p, void *ptr)
{
	struct mbtrail *t;

	mtx_lock(&p->free_lock);
	t = C2T(p, ptr);
	t->page &= ~(MBP_USED | MBP_CARD);
	SLIST_INSERT_HEAD(&p->free_list, (struct mbfree *)ptr, link);
	mtx_unlock(&p->free_lock);
}

/*
 * Mbuf system external mbuf free routine
 */
void
mbp_ext_free(void *buf, void *arg)
{
	mbp_free(arg, buf);
}

/*
 * Free all buffers that are marked as beeing on the card
 */
void
mbp_card_free(struct mbpool *p)
{
	u_int i, b;
	struct mbpage *pg;
	struct mbtrail *tr;
	struct mbfree *cf;

	mtx_lock(&p->free_lock);
	for (i = 0; i < p->npages; i++) {
		pg = &p->pages[i];
		for (b = 0; b < p->nchunks; b++) {
			cf = N2C(p, pg, b);
			tr = C2T(p, cf);
			if (tr->page & MBP_CARD) {
				tr->page &= MBP_PMSK;
				SLIST_INSERT_HEAD(&p->free_list, cf, link);
			}
		}
	}
	mtx_unlock(&p->free_lock);
}

/*
 * Count buffers
 */
void
mbp_count(struct mbpool *p, u_int *used, u_int *card, u_int *free)
{
	u_int i, b;
	struct mbpage *pg;
	struct mbtrail *tr;
	struct mbfree *cf;

	*used = *card = *free = 0;
	for (i = 0; i < p->npages; i++) {
		pg = &p->pages[i];
		for (b = 0; b < p->nchunks; b++) {
			tr = C2T(p, N2C(p, pg, b));
			if (tr->page & MBP_CARD)
				(*card)++;
			if (tr->page & MBP_USED)
				(*used)++;
		}
	}
	mtx_lock(&p->free_lock);
	SLIST_FOREACH(cf, &p->free_list, link)
		*free++;
	mtx_unlock(&p->free_lock);
}

/*
 * Get the buffer from a handle and clear the card flag.
 */
void *
mbp_get(struct mbpool *p, uint32_t h)
{
	struct mbfree *cf;
	struct mbtrail *tr;

	cf = N2C(p, &p->pages[HPAGE(h)], HCHUNK(h));
	tr = C2T(p, cf);

#ifdef DIAGNOSTIC
	if (!(tr->page & MBP_CARD))
		printf("%s: (%s) chunk %u page %u not on card\n", __func__,
		    p->name, HCHUNK(h), HPAGE(h));
#endif

	tr->page &= ~MBP_CARD;
	return (cf);
}

/*
 * Get the buffer from a handle and keep the card flag.
 */
void *
mbp_get_keep(struct mbpool *p, uint32_t h)
{
	struct mbfree *cf;
	struct mbtrail *tr;

	cf = N2C(p, &p->pages[HPAGE(h)], HCHUNK(h));
	tr = C2T(p, cf);

#ifdef DIAGNOSTIC
	if (!(tr->page & MBP_CARD))
		printf("%s: (%s) chunk %u page %u not on card\n", __func__,
		    p->name, HCHUNK(h), HPAGE(h));
#endif

	return (cf);
}

/*
 * sync the chunk
 */
void
mbp_sync(struct mbpool *p, uint32_t h, bus_addr_t off, bus_size_t len, u_int op)
{

#if 0
	bus_dmamap_sync_size(p->dmat, p->pages[HPAGE(h)].map,
	    HCHUNK(h) * p->chunk_size + off, len, op);
#endif
}
