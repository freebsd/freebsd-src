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
  "$FreeBSD$";
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
#include <sys/mbuf.h>
#include <sys/uio.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>

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
                             (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : 0);
        } else {
                /*
                 * XXX Use Contigmalloc until it is merged into this facility
                 *     and handles multi-seg allocations.  Nobody is doing
                 *     multi-seg allocations yet though.
                 */
                *vaddr = contigmalloc(dmat->maxsize, M_DEVBUF,
                    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : 0,
                    0ul, dmat->lowaddr, dmat->alignment? dmat->alignment : 1ul,
                    dmat->boundary);
        }

        if (*vaddr == NULL)
                return (ENOMEM);

        return (0);
}

/*
 * Free a piece of memory and it's allocated dmamap, that was allocated
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
 * Utility function to load a linear buffer.  lastaddrp holds state
 * between invocations (for multiple-buffer loads).  segp contains
 * the starting segment on entrance, and the ending segment on exit.
 * first indicates if this is the first invocation of this function.
 */
static int
bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dma_segment_t segs[],
    void *buf, bus_size_t buflen, struct thread *td,
    int flags, vm_offset_t *lastaddrp, int *segp,
    int first)
{
	bus_size_t sgsize;
	bus_addr_t curaddr, lastaddr, baddr, bmask;
	vm_offset_t vaddr = (vm_offset_t)buf;
	int seg;
	pmap_t pmap;

	if (td != NULL)
		pmap = vmspace_pmap(td->td_proc->p_vmspace);
	else
		pmap = NULL;

	lastaddr = *lastaddrp;
	bmask = ~(dmat->boundary - 1);

	for (seg = *segp; buflen > 0 ; ) {
		/*
		 * Get the physical address for this segment.
		 */
		if (pmap)
			curaddr = pmap_extract(pmap, vaddr);
		else
			curaddr = pmap_kextract(vaddr);

		/*
		 * Compute the segment size, and adjust counts.
		 */
		sgsize = PAGE_SIZE - ((u_long)curaddr & PAGE_MASK);
		if (buflen < sgsize)
			sgsize = buflen;

		/*
		 * Make sure we don't cross any boundaries.
		 */
		if (dmat->boundary > 0) {
			baddr = (curaddr + dmat->boundary) & bmask;
			if (sgsize > (baddr - curaddr))
				sgsize = (baddr - curaddr);
		}

		/*
		 * Insert chunk into a segment, coalescing with
		 * the previous segment if possible.
		 */
		if (first) {
			segs[seg].ds_addr = curaddr;
			segs[seg].ds_len = sgsize;
			first = 0;
		} else {
			if (curaddr == lastaddr &&
			    (segs[seg].ds_len + sgsize) <= dmat->maxsegsz &&
			    (dmat->boundary == 0 ||
			     (segs[seg].ds_addr & bmask) == (curaddr & bmask)))
				segs[seg].ds_len += sgsize;
			else {
				if (++seg >= dmat->nsegments)
					break;
				segs[seg].ds_addr = curaddr;
				segs[seg].ds_len = sgsize;
			}
		}

		lastaddr = curaddr + sgsize;
		vaddr += sgsize;
		buflen -= sgsize;
	}

	*segp = seg;
	*lastaddrp = lastaddr;

	/*
	 * Did we fit?
	 */
	return (buflen != 0 ? EFBIG : 0); /* XXX better return value here? */
}

/*
 * Like bus_dmamap_load(), but for mbufs.
 */
int
bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map, struct mbuf *m0,
		     bus_dmamap_callback2_t *callback, void *callback_arg,
		     int flags)
{
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	int nsegs = 0, error = 0;

	KASSERT(m0->m_flags & M_PKTHDR,
	    ("bus_dmamap_load_mbuf: no packet header"));

	if (m0->m_pkthdr.len <= dmat->maxsize) {
		int first = 1;
		vm_offset_t lastaddr = 0;
		struct mbuf *m;

		for (m = m0; m != NULL && error == 0; m = m->m_next) {
			error = bus_dmamap_load_buffer(dmat, dm_segments,
			    m->m_data, m->m_len, NULL, flags,
			    &lastaddr, &nsegs, first);
			first = 0;
		}
	} else {
		error = EINVAL;
	}

	if (error) {
		/* 
		 * force "no valid mappings" on error in callback.
		 */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		(*callback)(callback_arg, dm_segments, nsegs+1,
		    m0->m_pkthdr.len, error);
	}
	return (error);
}

/*
 * Like bus_dmamap_load(), but for uios.
 */
int
bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map, struct uio *uio,
    bus_dmamap_callback2_t *callback, void *callback_arg,
    int flags)
{
	vm_offset_t lastaddr;
#ifdef __GNUC__
	bus_dma_segment_t dm_segments[dmat->nsegments];
#else
	bus_dma_segment_t dm_segments[BUS_DMAMAP_NSEGS];
#endif
	int nsegs, i, error, first;
	bus_size_t resid;
	struct iovec *iov;
	struct proc *td = NULL;

	resid = uio->uio_resid;
	iov = uio->uio_iov;

	if (uio->uio_segflg == UIO_USERSPACE) {
		td = uio->uio_td;
		KASSERT(td != NULL,
		    ("bus_dmamap_load_uio: USERSPACE but no proc"));
	}

	first = 1;
	nsegs = error = 0;
	for (i = 0; i < uio->uio_iovcnt && resid != 0 && !error; i++) {
		/*
		 * Now at the first iovec to load.  Load each iovec
		 * until we have exhausted the residual count.
		 */
		bus_size_t minlen =
		    resid < iov[i].iov_len ? resid : iov[i].iov_len;
		caddr_t addr = (caddr_t) iov[i].iov_base;

		error = bus_dmamap_load_buffer(dmat, dm_segments, addr,
		    minlen, td, flags, &lastaddr, &nsegs, first);

		first = 0;

		resid -= minlen;
	}

	if (error) {
		/* 
		 * force "no valid mappings" on error in callback.
		 */
		(*callback)(callback_arg, dm_segments, 0, 0, error);
	} else {
		(*callback)(callback_arg, dm_segments, nsegs+1,
		    uio->uio_resid, error);
	}

	return (error);
}

/*
 * Release the mapping held by map. A no-op on PowerPC.
 */
void
bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{}

void
bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{}
