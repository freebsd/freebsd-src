/*
 * Copyright (c) 2002 Peter Grehan
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
 *
 *   From i386/busdma_machdep.c,v 1.26 2002/04/19 22:58:09 alfred
 */

#ifndef lint
static const char rcsid[] =
  "$FreeBSD: src/sys/powerpc/powerpc/busdma_machdep.c,v 1.2 2002/07/09 12:47:14 benno Exp $";
#endif /* not lint */

/*
 * MacPPC bus dma support routines
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/interrupt.h>
#include <sys/lock.h>
#include <sys/proc.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_page.h>

#include <machine/bus.h>

struct bus_dma_tag {
	bus_dma_tag_t     parent;
	bus_size_t        alignment;
	bus_size_t        boundary;
	bus_addr_t        lowaddr;
	bus_addr_t        highaddr;
	bus_dma_filter_t *filter;
	void             *filterarg;
	bus_size_t        maxsize;
	u_int             nsegments;
	bus_size_t        maxsegsz;
	int               flags;
	int               ref_count;
	int               map_count;
};

struct bus_dmamap {
        bus_dma_tag_t          dmat;
        void                  *buf;             /* unmapped buffer pointer */
        bus_size_t             buflen;          /* unmapped buffer length */
        bus_dmamap_callback_t *callback;
        void                  *callback_arg;
};

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

        /*
	 * Take into account any restrictions imposed by our parent tag
	 */
        if (parent != NULL) {
                newtag->lowaddr = min(parent->lowaddr, newtag->lowaddr);
                newtag->highaddr = max(parent->highaddr, newtag->highaddr);
		
                /*
                 * XXX Not really correct??? Probably need to honor boundary
                 *     all the way up the inheritence chain.
                 */
                newtag->boundary = max(parent->boundary, newtag->boundary);
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

	*dmat = newtag;
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
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp)
{
	*mapp = NULL;
	dmat->map_count++;

	return (0);
}

/*
 * Destroy a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int
bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map)
{
        if (map != NULL) {
		panic("dmamap_destroy: NULL?\n");
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
        *mapp = NULL;
	
        if (dmat->maxsize <= PAGE_SIZE) {
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
 * via bus_dmamem_alloc.  Make the same choice for free/contigfree.
 */
void
bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map)
{
        if (map != NULL)
                panic("bus_dmamem_free: Invalid map freed\n");
        if (dmat->maxsize <= PAGE_SIZE)
		free(vaddr, M_DEVBUF);
        else
		contigfree(vaddr, dmat->maxsize, M_DEVBUF);
}

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int
bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
                bus_size_t buflen, bus_dmamap_callback_t *callback,
                void *callback_arg, int flags)
{
        vm_offset_t             vaddr;
        vm_offset_t             paddr;
#ifdef __GNUC__
        bus_dma_segment_t       dm_segments[dmat->nsegments];
#else
        bus_dma_segment_t       dm_segments[BUS_DMAMAP_NSEGS];
#endif
        bus_dma_segment_t      *sg;
        int                     seg;
        int                     error = 0;
        vm_offset_t             nextpaddr;

        if (map != NULL)
		panic("bus_dmamap_load: Invalid map\n");

        vaddr = (vm_offset_t)buf;
        sg = &dm_segments[0];
        seg = 1;
        sg->ds_len = 0;
        nextpaddr = 0;

        do {
		bus_size_t      size;

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
 * Release the mapping held by map.
 */
void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{}

void
bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{}





