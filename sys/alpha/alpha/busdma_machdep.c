/*
 * Copyright (c) 1997 Justin T. Gibbs.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/systm.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/bus.h>
#include <machine/sgmap.h>
#include <machine/md_var.h>

#define MAX(a,b) (((a) > (b)) ? (a) : (b))
#define MIN(a,b) (((a) < (b)) ? (a) : (b))
#define MAX_BPAGES 128

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
};

struct bounce_page {
	vm_offset_t	vaddr;		/* kva of bounce buffer */
	bus_addr_t	busaddr;	/* Physical address */
	vm_offset_t	datavaddr;	/* kva of client data */
	bus_size_t	datacount;	/* client data count */
	STAILQ_ENTRY(bounce_page) links;
};

int busdma_swi_pending;

static struct mtx bounce_lock;
static STAILQ_HEAD(bp_list, bounce_page) bounce_page_list;
static int free_bpages;
static int reserved_bpages;
static int active_bpages;
static int total_bpages;
static bus_addr_t bounce_lowaddr = BUS_SPACE_MAXADDR;

struct bus_dmamap {
	struct bp_list	       bpages;
	int		       pagesneeded;
	int		       pagesreserved;
	bus_dma_tag_t	       dmat;
	void		      *buf;		/* unmapped buffer pointer */
	bus_size_t	       buflen;		/* unmapped buffer length */
	vm_offset_t	       busaddress;	/* address in bus space */
	bus_dmamap_callback_t *callback;
	void		      *callback_arg;
	void		      *sgmaphandle;	/* handle into sgmap */
	STAILQ_ENTRY(bus_dmamap) links;
};

static STAILQ_HEAD(, bus_dmamap) bounce_map_waitinglist;
static STAILQ_HEAD(, bus_dmamap) bounce_map_callbacklist;
static struct bus_dmamap nobounce_dmamap;

static int alloc_bounce_pages(bus_dma_tag_t dmat, u_int numpages);
static int reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map);
static vm_offset_t add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map,
				   vm_offset_t vaddr, bus_size_t size);
static void free_bounce_page(bus_dma_tag_t dmat, struct bounce_page *bpage);
static __inline int run_filter(bus_dma_tag_t dmat, bus_addr_t paddr);

static __inline int
run_filter(bus_dma_tag_t dmat, bus_addr_t paddr)
{
	int retval;

	retval = 0;
	do {
		if (paddr > dmat->lowaddr
		 && paddr <= dmat->highaddr
		 && (dmat->filter == NULL
		  || (*dmat->filter)(dmat->filterarg, paddr) != 0))
			retval = 1;

		dmat = dmat->parent;		
	} while (retval == 0 && dmat != NULL);
	return (retval);
}

#define BUS_DMA_MIN_ALLOC_COMP BUS_DMA_BUS4
/*
 * Allocate a device specific dma_tag.
 */
int
bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignment,
		   bus_size_t boundary, bus_addr_t lowaddr,
		   bus_addr_t highaddr, bus_dma_filter_t *filter,
		   void *filterarg, bus_size_t maxsize, int nsegments,
		   bus_size_t maxsegsz, int flags, bus_dma_tag_t *dmat)
{
	bus_dma_tag_t newtag;
	int error = 0;

	/* Return a NULL tag on failure */
	*dmat = NULL;

	newtag = (bus_dma_tag_t)malloc(sizeof(*newtag), M_DEVBUF, M_NOWAIT);
	if (newtag == NULL)
		return (ENOMEM);

	newtag->parent = parent;
	newtag->alignment = alignment;
	newtag->boundary = boundary;
	newtag->lowaddr = trunc_page(lowaddr) + (PAGE_SIZE - 1);
	newtag->highaddr = trunc_page(highaddr) + (PAGE_SIZE - 1);
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
		newtag->lowaddr = MIN(parent->lowaddr, newtag->lowaddr);
		newtag->highaddr = MAX(parent->highaddr, newtag->highaddr);
		/*
		 * XXX Not really correct??? Probably need to honor boundary
		 *     all the way up the inheritence chain.
		 */
		newtag->boundary = MAX(parent->boundary, newtag->boundary);
		if (newtag->filter == NULL) {
			/*
			 * Short circuit looking at our parent directly
			 * since we have encapsulated all of its information
			 */
			newtag->filter = parent->filter;
			newtag->filterarg = parent->filterarg;
			newtag->parent = parent->parent;
		}
		if (newtag->parent != NULL) {
			parent->ref_count++;
		}
	}
	
	if (newtag->lowaddr < ptoa(Maxmem) && (flags & BUS_DMA_ALLOCNOW) != 0) {
		/* Must bounce */

		if (lowaddr > bounce_lowaddr) {
			/*
			 * Go through the pool and kill any pages
			 * that don't reside below lowaddr.
			 */
			panic("bus_dma_tag_create: page reallocation "
			      "not implemented");
		}
		if (ptoa(total_bpages) < maxsize) {
			int pages;

			pages = atop(maxsize) - total_bpages;

			/* Add pages to our bounce pool */
			if (alloc_bounce_pages(newtag, pages) < pages)
				error = ENOMEM;
		}
		/* Performed initial allocation */
		newtag->flags |= BUS_DMA_MIN_ALLOC_COMP;
	}
	
	if (error != 0) {
		free(newtag, M_DEVBUF);
	} else {
		*dmat = newtag;
	}
	return (error);
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
			}
			dmat = parent;
		}
	}
	return (0);
}

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	int error;

	error = 0;

	if ((dmat->flags & BUS_DMA_ISA) && chipset.sgmap != NULL) {
		bus_dmamap_t map;
		map = (bus_dmamap_t)malloc(sizeof(*map), M_DEVBUF,
					     M_NOWAIT | M_ZERO);
		if (map == NULL)
			return (ENOMEM);

		map->busaddress =
			sgmap_alloc_region(chipset.sgmap,
					   dmat->maxsize,
					   dmat->boundary,
					   &map->sgmaphandle);
		dmat->map_count++;
		*mapp = map;
		return (0);
	}

	if (dmat->lowaddr < ptoa(Maxmem)) {
		/* Must bounce */
		int maxpages;

		*mapp = (bus_dmamap_t)malloc(sizeof(**mapp), M_DEVBUF,
					     M_NOWAIT | M_ZERO);
		if (*mapp == NULL)
			return (ENOMEM);

		/* Initialize the new map */
		STAILQ_INIT(&((*mapp)->bpages));

		/*
		 * Attempt to add pages to our pool on a per-instance
		 * basis up to a sane limit.
		 */
		maxpages = MIN(MAX_BPAGES, Maxmem - atop(dmat->lowaddr));
		if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0
		 || (dmat->map_count > 0
		  && total_bpages < maxpages)) {
			int pages;

			if (dmat->lowaddr > bounce_lowaddr) {
				/*
				 * Go through the pool and kill any pages
				 * that don't reside below lowaddr.
				 */
				panic("bus_dmamap_create: page reallocation "
				      "not implemented");
			}
			pages = atop(dmat->maxsize) + 1;
			pages = MIN(maxpages - total_bpages, pages);

			if (alloc_bounce_pages(dmat, pages) < pages)
				error = ENOMEM;
			
			if ((dmat->flags & BUS_DMA_MIN_ALLOC_COMP) == 0) {
				if (error == 0)
					dmat->flags |= BUS_DMA_MIN_ALLOC_COMP;
			} else {
				error = 0;
			}
		}
	} else {
		*mapp = &nobounce_dmamap;
	}
	if (error == 0)
		dmat->map_count++;
	return (error);
}

/*
 * Destroy a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	if ((dmat->flags & BUS_DMA_ISA) && chipset.sgmap != NULL) {
		sgmap_free_region(chipset.sgmap, map->sgmaphandle);
	}

	if (map != NULL) {
		if (STAILQ_FIRST(&map->bpages) != NULL)
			return (EBUSY);
		free(map, M_DEVBUF);
	}
	dmat->map_count--;
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
	/* If we succeed, no mapping/bouncing will be required */
	*mapp = &nobounce_dmamap;

	if ((dmat->maxsize <= PAGE_SIZE) && dmat->lowaddr >= ptoa(Maxmem)) {
		*vaddr = malloc(dmat->maxsize, M_DEVBUF,
				(flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK);
	} else {
		/*
		 * XXX Use Contigmalloc until it is merged into this facility
		 *     and handles multi-seg allocations.  Nobody is doing
		 *     multi-seg allocations yet though.
		 */
		*vaddr = contigmalloc(dmat->maxsize, M_DEVBUF,
		    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK,
		    0ul, dmat->lowaddr, dmat->alignment? dmat->alignment : 1ul,
		    dmat->boundary);
	}
	if (*vaddr == NULL)
		return (ENOMEM);
	return (0);
}

/*
 * Free a piece of memory and it's allociated dmamap, that was allocated
 * via bus_dmamem_alloc.
 */
void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
	/*
	 * dmamem does not need to be bounced, so the map should be
	 * NULL
	 */
	if (map != &nobounce_dmamap)
		panic("bus_dmamem_free: Invalid map freed\n");
	free(vaddr, M_DEVBUF);
}

#define BUS_DMAMAP_NSEGS ((BUS_SPACE_MAXSIZE / PAGE_SIZE) + 1)

/*
 * Map the buffer buf into bus space using the dmamap map.
 */

vm_offset_t alpha_XXX_dmamap_or = 1024UL*1024UL*1024UL;  /*XXX */

int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
		bus_size_t buflen, bus_dmamap_callback_t *callback,
		void *callback_arg, int flags)
{
	vm_offset_t		vaddr;
	vm_offset_t		paddr;
#ifdef __GNUC__
	bus_dma_segment_t	dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t	dm_segments[BUS_DMAMAP_NSEGS];
#endif
	bus_dma_segment_t      *sg;
	int			seg;
	int			error;
	vm_offset_t		nextpaddr;

	error = 0;

	if ((dmat->flags & BUS_DMA_ISA) && chipset.sgmap != NULL) {
		/*
		 * For ISA dma, we use the chipset's scatter-gather
		 * map to map the tranfer into the ISA reachable range
		 * of the bus address space.
		 */
		vaddr = trunc_page((vm_offset_t) buf);
		dm_segments[0].ds_addr =
			map->busaddress + (vm_offset_t) buf - vaddr;
		dm_segments[0].ds_len = buflen;
		buflen = round_page((vm_offset_t) buf + buflen) - vaddr;
		sgmap_load_region(chipset.sgmap,
				  map->busaddress,
				  vaddr,
				  buflen);
		map->buflen = buflen;
		(*callback)(callback_arg, dm_segments, 1, error);

		return (0);
	}

	/*
	 * If we are being called during a callback, pagesneeded will
	 * be non-zero, so we can avoid doing the work twice.
	 */
	if (dmat->lowaddr < ptoa(Maxmem) && map->pagesneeded == 0) {
		vm_offset_t	vendaddr;

		/*
		 * Count the number of bounce pages
		 * needed in order to complete this transfer
		 */
		vaddr = trunc_page(buf);
		vendaddr = (vm_offset_t)buf + buflen;

		while (vaddr < vendaddr) {
			paddr = pmap_kextract(vaddr);
			if (run_filter(dmat, paddr) != 0) {

				map->pagesneeded++;
			}
			vaddr += PAGE_SIZE;
		}
	}

	/* Reserve Necessary Bounce Pages */
	if (map->pagesneeded != 0) {
		int s;

		s = splhigh();
	 	if (reserve_bounce_pages(dmat, map) != 0) {

			/* Queue us for resources */
			map->dmat = dmat;
			map->buf = buf;
			map->buflen = buflen;
			map->callback = callback;
			map->callback_arg = callback_arg;

			STAILQ_INSERT_TAIL(&bounce_map_waitinglist, map, links);
			splx(s);

			return (EINPROGRESS);
		}
		splx(s);
	}

	vaddr = (vm_offset_t)buf;
	sg = &dm_segments[0];
	seg = 1;
	sg->ds_len = 0;

	nextpaddr = 0;

	do {
		bus_size_t	size;

		paddr = pmap_kextract(vaddr);
		size = PAGE_SIZE - (paddr & PAGE_MASK);
		if (size > buflen)
			size = buflen;

		if (map->pagesneeded != 0 && run_filter(dmat, paddr)) {
			paddr = add_bounce_page(dmat, map, vaddr, size);
		}

		if (sg->ds_len == 0) {
			sg->ds_addr = paddr + alpha_XXX_dmamap_or;
			sg->ds_len = size;
		} else if (paddr == nextpaddr) {
			sg->ds_len += size;
		} else {
			/* Go to the next segment */
			sg++;
			seg++;
			if (seg > dmat->nsegments)
				break;
			sg->ds_addr = paddr + alpha_XXX_dmamap_or;
			sg->ds_len = size;
		}
		vaddr += size;
		nextpaddr = paddr + size;
		buflen -= size;

	} while (buflen > 0);

	if (buflen != 0) {
		printf("bus_dmamap_load: Too many segs! buf_len = 0x%lx\n",
		       buflen);
		error = EFBIG;
	}

	(*callback)(callback_arg, dm_segments, seg, error);

	return (0);
}

/*
 * Release the mapping held by map.
 */
void
_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	struct bounce_page *bpage;

	if ((dmat->flags & BUS_DMA_ISA) && chipset.sgmap != NULL) {
		sgmap_unload_region(chipset.sgmap,
				    map->busaddress,
				    map->buflen);
		return;
	}

	while ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
		STAILQ_REMOVE_HEAD(&map->bpages, links);
		free_bounce_page(dmat, bpage);
	}
}

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{
	struct bounce_page *bpage;

	if ((bpage = STAILQ_FIRST(&map->bpages)) != NULL) {
		
		/*
		 * Handle data bouncing.  We might also
		 * want to add support for invalidating
		 * the caches on broken hardware
		 */
		switch (op) {
		case BUS_DMASYNC_PREWRITE:
			while (bpage != NULL) {
				bcopy((void *)bpage->datavaddr,
				      (void *)bpage->vaddr,
				      bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
			break;

		case BUS_DMASYNC_POSTREAD:
			while (bpage != NULL) {
				bcopy((void *)bpage->vaddr,
				      (void *)bpage->datavaddr,
				      bpage->datacount);
				bpage = STAILQ_NEXT(bpage, links);
			}
			break;
		case BUS_DMASYNC_PREREAD:
		case BUS_DMASYNC_POSTWRITE:
			/* No-ops */
			break;
		}
	}
}

static int
alloc_bounce_pages(bus_dma_tag_t dmat, u_int numpages)
{
	int count;

	count = 0;
	if (total_bpages == 0) {
		mtx_init(&bounce_lock, "BouncePage", MTX_DEF);
		STAILQ_INIT(&bounce_page_list);
		STAILQ_INIT(&bounce_map_waitinglist);
		STAILQ_INIT(&bounce_map_callbacklist);
	}
	
	while (numpages > 0) {
		struct bounce_page *bpage;

		bpage = (struct bounce_page *)malloc(sizeof(*bpage), M_DEVBUF,
						     M_NOWAIT | M_ZERO);

		if (bpage == NULL)
			break;
		bpage->vaddr = (vm_offset_t)contigmalloc(PAGE_SIZE, M_DEVBUF,
							 M_NOWAIT, 0ul,
							 dmat->lowaddr,
							 PAGE_SIZE,
							 dmat->boundary);
		if (bpage->vaddr == NULL) {
			free(bpage, M_DEVBUF);
			break;
		}
		bpage->busaddr = pmap_kextract(bpage->vaddr);
		mtx_lock(&bounce_lock);
		STAILQ_INSERT_TAIL(&bounce_page_list, bpage, links);
		total_bpages++;
		free_bpages++;
		mtx_unlock(&bounce_lock);
		count++;
		numpages--;
	}
	return (count);
}

static int
reserve_bounce_pages(bus_dma_tag_t dmat, bus_dmamap_t map)
{
	int pages;

	pages = MIN(free_bpages, map->pagesneeded - map->pagesreserved);
	free_bpages -= pages;
	reserved_bpages += pages;
	map->pagesreserved += pages;
	pages = map->pagesneeded - map->pagesreserved;

	return (pages);
}

static vm_offset_t
add_bounce_page(bus_dma_tag_t dmat, bus_dmamap_t map, vm_offset_t vaddr,
		bus_size_t size)
{
	struct bounce_page *bpage;

	if (map->pagesneeded == 0)
		panic("add_bounce_page: map doesn't need any pages");
	map->pagesneeded--;

	if (map->pagesreserved == 0)
		panic("add_bounce_page: map doesn't need any pages");
	map->pagesreserved--;

	mtx_lock(&bounce_lock);
	bpage = STAILQ_FIRST(&bounce_page_list);
	if (bpage == NULL)
		panic("add_bounce_page: free page list is empty");

	STAILQ_REMOVE_HEAD(&bounce_page_list, links);
	reserved_bpages--;
	active_bpages++;
	mtx_unlock(&bounce_lock);

	bpage->datavaddr = vaddr;
	bpage->datacount = size;
	STAILQ_INSERT_TAIL(&(map->bpages), bpage, links);
	return (bpage->busaddr);
}

static void
free_bounce_page(bus_dma_tag_t dmat, struct bounce_page *bpage)
{
	struct bus_dmamap *map;

	bpage->datavaddr = 0;
	bpage->datacount = 0;

	mtx_lock(&bounce_lock);
	STAILQ_INSERT_HEAD(&bounce_page_list, bpage, links);
	free_bpages++;
	active_bpages--;
	if ((map = STAILQ_FIRST(&bounce_map_waitinglist)) != NULL) {
		if (reserve_bounce_pages(map->dmat, map) == 0) {
			STAILQ_REMOVE_HEAD(&bounce_map_waitinglist, links);
			STAILQ_INSERT_TAIL(&bounce_map_callbacklist,
					   map, links);
			busdma_swi_pending = 1;
			swi_sched(vm_ih, SWI_NOSWITCH);
		}
	}
	mtx_unlock(&bounce_lock);
}

void
busdma_swi(void)
{
	struct bus_dmamap *map;

	mtx_lock(&bounce_lock);
	while ((map = STAILQ_FIRST(&bounce_map_callbacklist)) != NULL) {
		STAILQ_REMOVE_HEAD(&bounce_map_callbacklist, links);
		mtx_unlock(&bounce_lock);
		bus_dmamap_load(map->dmat, map, map->buf, map->buflen,
				map->callback, map->callback_arg, /*flags*/0);
		mtx_lock(&bounce_lock);
	}
	mtx_unlock(&bounce_lock);
}
