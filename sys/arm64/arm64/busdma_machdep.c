/* $FreeBSD$ */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/bus.h>

int
_bus_dmamap_load_phys(bus_dma_tag_t dmat, bus_dmamap_t map, vm_paddr_t buf,
    bus_size_t buflen, int flags, bus_dma_segment_t *segs, int *segp)
{

	panic("_bus_dmamap_load_phys");
}

int
_bus_dmamap_load_ma(bus_dma_tag_t dmat, bus_dmamap_t map, struct vm_page **ma,
    bus_size_t tlen, int ma_offs, int flags, bus_dma_segment_t *segs,
    int *segp)
{

	panic("_bus_dmamap_load_ma");
}

int
_bus_dmamap_load_buffer(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
    bus_size_t buflen, pmap_t pmap, int flags, bus_dma_segment_t *segs,
    int *segp)
{

	panic("_bus_dmamap_load_buffer");
}

void
__bus_dmamap_waitok(bus_dma_tag_t dmat, bus_dmamap_t map,
    struct memdesc *mem, bus_dmamap_callback_t *callback, void *callback_arg)
{

	panic("__bus_dmamap_waitok");
}

bus_dma_segment_t *
_bus_dmamap_complete(bus_dma_tag_t dmat, bus_dmamap_t map,
    bus_dma_segment_t *segs, int nsegs, int error)
{

	panic("_bus_dmamap_complete");
}

void
_bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map)
{

	panic("_bus_dmamap_unload");
}

void
_bus_dmamap_sync(bus_dma_tag_t dmat, bus_dmamap_t map, bus_dmasync_op_t op)
{

	panic("_bus_dmamap_sync");
}
