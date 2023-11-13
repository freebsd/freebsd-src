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

/*
 * Common code for managing bounce pages for bus_dma backends.  As
 * this code currently assumes it can access internal members of
 * opaque types like bus_dma_tag_t and bus_dmamap it is #include'd in
 * backends rather than being compiled standalone.
 *
 * Prerequisites:
 *
 * - M_BUSDMA malloc type
 * - struct bus_dmamap
 * - hw_busdma SYSCTL_NODE
 * - macros to access the following fields of bus_dma_tag_t:
 *   - dmat_alignment()
 *   - dmat_flags()
 *   - dmat_lowaddr()
 *   - dmat_lockfunc()
 *   - dmat_lockarg()
 */

#include <sys/kthread.h>
#include <sys/sched.h>

struct bounce_page {
	vm_offset_t	vaddr;		/* kva of bounce buffer */
	bus_addr_t	busaddr;	/* Physical address */
	vm_offset_t	datavaddr;	/* kva of client data */
#if defined(__amd64__) || defined(__i386__)
	vm_page_t	datapage[2];	/* physical page(s) of client data */
#else
	vm_page_t	datapage;	/* physical page of client data */
#endif
	vm_offset_t	dataoffs;	/* page offset of client data */
	bus_size_t	datacount;	/* client data count */
	STAILQ_ENTRY(bounce_page) links;
};

struct bounce_zone {
	STAILQ_ENTRY(bounce_zone) links;
	STAILQ_HEAD(, bounce_page) bounce_page_list;
	STAILQ_HEAD(, bus_dmamap) bounce_map_waitinglist;
	int		total_bpages;
	int		free_bpages;
	int		reserved_bpages;
	int		active_bpages;
	int		total_bounced;
	int		total_deferred;
	int		map_count;
#ifdef dmat_domain
	int		domain;
#endif
	sbintime_t	total_deferred_time;
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
static STAILQ_HEAD(, bus_dmamap) bounce_map_callbacklist;

static MALLOC_DEFINE(M_BOUNCE, "bounce", "busdma bounce pages");

SYSCTL_INT(_hw_busdma, OID_AUTO, total_bpages, CTLFLAG_RD, &total_bpages, 0,
   "Total bounce pages");

static void busdma_thread(void *);
static int reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map,
    int commit);

static int
_bus_dmamap_reserve_pages(bus_dma_tag_t dmat, bus_dmamap_t map, int flags)
{
	struct bounce_zone *bz;

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
			bz = dmat->bounce_zone;
			STAILQ_INSERT_TAIL(&bz->bounce_map_waitinglist, map,
			    links);
			map->queued_time = sbinuptime();
			mtx_unlock(&bounce_lock);
			return (EINPROGRESS);
		}
	}
	mtx_unlock(&bounce_lock);

	return (0);
}

static void
init_bounce_pages(void *dummy __unused)
{

	total_bpages = 0;
	STAILQ_INIT(&bounce_zone_list);
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
	bool start_thread;

	/* Check to see if we already have a suitable zone */
	STAILQ_FOREACH(bz, &bounce_zone_list, links) {
		if ((dmat_alignment(dmat) <= bz->alignment) &&
#ifdef dmat_domain
		    dmat_domain(dmat) == bz->domain &&
#endif
		    (dmat_lowaddr(dmat) >= bz->lowaddr)) {
			dmat->bounce_zone = bz;
			return (0);
		}
	}

	if ((bz = (struct bounce_zone *)malloc(sizeof(*bz), M_BUSDMA,
	    M_NOWAIT | M_ZERO)) == NULL)
		return (ENOMEM);

	STAILQ_INIT(&bz->bounce_page_list);
	STAILQ_INIT(&bz->bounce_map_waitinglist);
	bz->free_bpages = 0;
	bz->reserved_bpages = 0;
	bz->active_bpages = 0;
	bz->lowaddr = dmat_lowaddr(dmat);
	bz->alignment = MAX(dmat_alignment(dmat), PAGE_SIZE);
	bz->map_count = 0;
#ifdef dmat_domain
	bz->domain = dmat_domain(dmat);
#endif
	snprintf(bz->zoneid, sizeof(bz->zoneid), "zone%d", busdma_zonecount);
	busdma_zonecount++;
	snprintf(bz->lowaddrid, sizeof(bz->lowaddrid), "%#jx",
	    (uintmax_t)bz->lowaddr);
	start_thread = STAILQ_EMPTY(&bounce_zone_list);
	STAILQ_INSERT_TAIL(&bounce_zone_list, bz, links);
	dmat->bounce_zone = bz;

	sysctl_ctx_init(&bz->sysctl_tree);
	bz->sysctl_tree_top = SYSCTL_ADD_NODE(&bz->sysctl_tree,
	    SYSCTL_STATIC_CHILDREN(_hw_busdma), OID_AUTO, bz->zoneid,
	    CTLFLAG_RD | CTLFLAG_MPSAFE, 0, "");
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
	    "Total bounce requests (pages bounced)");
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "total_deferred", CTLFLAG_RD, &bz->total_deferred, 0,
	    "Total bounce requests that were deferred");
	SYSCTL_ADD_STRING(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "lowaddr", CTLFLAG_RD, bz->lowaddrid, 0, "");
	SYSCTL_ADD_UAUTO(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "alignment", CTLFLAG_RD, &bz->alignment, "");
#ifdef dmat_domain
	SYSCTL_ADD_INT(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "domain", CTLFLAG_RD, &bz->domain, 0,
	    "memory domain");
#endif
	SYSCTL_ADD_SBINTIME_USEC(busdma_sysctl_tree(bz),
	    SYSCTL_CHILDREN(busdma_sysctl_tree_top(bz)), OID_AUTO,
	    "total_deferred_time", CTLFLAG_RD, &bz->total_deferred_time,
	    "Cumulative time busdma requests are deferred (us)");
	if (start_thread) {
		if (kproc_create(busdma_thread, NULL, NULL, 0, 0, "busdma") !=
		    0)
			printf("failed to create busdma thread");
	}
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

#ifdef dmat_domain
		bpage = malloc_domainset(sizeof(*bpage), M_BUSDMA,
		    DOMAINSET_PREF(bz->domain), M_NOWAIT | M_ZERO);
#else
		bpage = malloc(sizeof(*bpage), M_BUSDMA, M_NOWAIT | M_ZERO);
#endif

		if (bpage == NULL)
			break;
#ifdef dmat_domain
		bpage->vaddr = (vm_offset_t)contigmalloc_domainset(PAGE_SIZE,
		    M_BOUNCE, DOMAINSET_PREF(bz->domain), M_NOWAIT,
		    0ul, bz->lowaddr, PAGE_SIZE, 0);
#else
		bpage->vaddr = (vm_offset_t)contigmalloc(PAGE_SIZE, M_BOUNCE,
		    M_NOWAIT, 0ul, bz->lowaddr, PAGE_SIZE, 0);
#endif
		if (bpage->vaddr == 0) {
			free(bpage, M_BUSDMA);
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

#if defined(__amd64__) || defined(__i386__)
static bus_addr_t
add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map, vm_offset_t vaddr,
    vm_paddr_t addr1, vm_paddr_t addr2, bus_size_t size)
#else
static bus_addr_t
add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map, vm_offset_t vaddr,
    bus_addr_t addr, bus_size_t size)
#endif
{
	struct bounce_zone *bz;
	struct bounce_page *bpage;

	KASSERT(dmat->bounce_zone != NULL, ("no bounce zone in dma tag"));
	KASSERT(map != NULL, ("add_bounce_page: bad map %p", map));
#if defined(__amd64__) || defined(__i386__)
	KASSERT(map != &nobounce_dmamap, ("add_bounce_page: bad map %p", map));
#endif
#ifdef __riscv
	KASSERT((map->flags & DMAMAP_COULD_BOUNCE) != 0,
	    ("add_bounce_page: bad map %p", map));
#endif

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

	if (dmat_flags(dmat) & BUS_DMA_KEEP_PG_OFFSET) {
		/* Page offset needs to be preserved. */
#if defined(__amd64__) || defined(__i386__)
		bpage->vaddr |= addr1 & PAGE_MASK;
		bpage->busaddr |= addr1 & PAGE_MASK;
		KASSERT(addr2 == 0,
	    ("Trying to bounce multiple pages with BUS_DMA_KEEP_PG_OFFSET"));
#else
		bpage->vaddr |= addr & PAGE_MASK;
		bpage->busaddr |= addr & PAGE_MASK;
#endif
	}
	bpage->datavaddr = vaddr;
#if defined(__amd64__) || defined(__i386__)
	bpage->datapage[0] = PHYS_TO_VM_PAGE(addr1);
	KASSERT((addr2 & PAGE_MASK) == 0, ("Second page is not aligned"));
	bpage->datapage[1] = PHYS_TO_VM_PAGE(addr2);
	bpage->dataoffs = addr1 & PAGE_MASK;
#else
	bpage->datapage = PHYS_TO_VM_PAGE(addr);
	bpage->dataoffs = addr & PAGE_MASK;
#endif
	bpage->datacount = size;
	STAILQ_INSERT_TAIL(&(map->bpages), bpage, links);
	return (bpage->busaddr);
}

static void
free_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	struct bounce_page *bpage;
	struct bounce_zone *bz;
	bool schedule_thread;
	u_int count;

	if (STAILQ_EMPTY(&map->bpages))
		return;

	bz = dmat->bounce_zone;
	count = 0;
	schedule_thread = false;
	STAILQ_FOREACH(bpage, &map->bpages, links) {
		bpage->datavaddr = 0;
		bpage->datacount = 0;

		if (dmat_flags(dmat) & BUS_DMA_KEEP_PG_OFFSET) {
			/*
			 * Reset the bounce page to start at offset 0.
			 * Other uses of this bounce page may need to
			 * store a full page of data and/or assume it
			 * starts on a page boundary.
			 */
			bpage->vaddr &= ~PAGE_MASK;
			bpage->busaddr &= ~PAGE_MASK;
		}
		count++;
	}

	mtx_lock(&bounce_lock);
	STAILQ_CONCAT(&bz->bounce_page_list, &map->bpages);
	bz->free_bpages += count;
	bz->active_bpages -= count;
	while ((map = STAILQ_FIRST(&bz->bounce_map_waitinglist)) != NULL) {
		if (reserve_bounce_pages(map->dmat, map, 1) != 0)
			break;

		STAILQ_REMOVE_HEAD(&bz->bounce_map_waitinglist, links);
		STAILQ_INSERT_TAIL(&bounce_map_callbacklist, map, links);
		bz->total_deferred++;
		schedule_thread = true;
	}
	mtx_unlock(&bounce_lock);
	if (schedule_thread)
		wakeup(&bounce_map_callbacklist);
}

static void
busdma_thread(void *dummy __unused)
{
	STAILQ_HEAD(, bus_dmamap) callbacklist;
	bus_dma_tag_t dmat;
	struct bus_dmamap *map, *nmap;
	struct bounce_zone *bz;

	thread_lock(curthread);
	sched_class(curthread, PRI_ITHD);
	sched_ithread_prio(curthread, PI_SWI(SWI_BUSDMA));
	thread_unlock(curthread);
	for (;;) {
		mtx_lock(&bounce_lock);
		while (STAILQ_EMPTY(&bounce_map_callbacklist))
			mtx_sleep(&bounce_map_callbacklist, &bounce_lock, 0,
			    "-", 0);
		STAILQ_INIT(&callbacklist);
		STAILQ_CONCAT(&callbacklist, &bounce_map_callbacklist);
		mtx_unlock(&bounce_lock);

		STAILQ_FOREACH_SAFE(map, &callbacklist, links, nmap) {
			dmat = map->dmat;
			bz = dmat->bounce_zone;
			dmat_lockfunc(dmat)(dmat_lockfuncarg(dmat),
			    BUS_DMA_LOCK);
			bz->total_deferred_time += (sbinuptime() - map->queued_time);
			bus_dmamap_load_mem(map->dmat, map, &map->mem,
			    map->callback, map->callback_arg, BUS_DMA_WAITOK);
			dmat_lockfunc(dmat)(dmat_lockfuncarg(dmat),
			    BUS_DMA_UNLOCK);
		}
	}
}
