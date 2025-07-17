/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/socket.h>
#include <sys/sockio.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_media.h>
#include <net/ethernet.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/fdt_common.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/qcom_ess_edma/qcom_ess_edma_var.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_reg.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_hw.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_desc.h>
#include <dev/qcom_ess_edma/qcom_ess_edma_debug.h>

static void
qcom_ess_edma_desc_map_addr(void *arg, bus_dma_segment_t *segs, int nsegs,
    int error)
{
	if (error != 0)
		return;
	KASSERT(nsegs == 1, ("too many DMA segments, %d should be 1", nsegs));
	*(bus_addr_t *)arg = segs[0].ds_addr;
}

/*
 * Initialise the given descriptor ring.
 */
int
qcom_ess_edma_desc_ring_setup(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring,
    char *label,
    int count,
    int sw_desc_size,
    int hw_desc_size,
    int num_segments,
    int buffer_align)
{
	int error;
	int hw_ring_size;

	ring->label = strdup(label, M_TEMP);
	if (ring->label == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to strdup label\n");
		error = ENOMEM;
		goto error;
	}

	mtx_init(&ring->mtx, ring->label, NULL, MTX_DEF);

	hw_ring_size = count * hw_desc_size;

	/*
	 * Round the hardware ring size up to a cacheline
	 * so we don't end up with partial cacheline sizes
	 * causing bounce buffers to be used.
	 */
	hw_ring_size = ((hw_ring_size + PAGE_SIZE) / PAGE_SIZE) * PAGE_SIZE;

	/*
	 * For now set it to 4 byte alignment, no max size.
	 */
	ring->ring_align = EDMA_DESC_RING_ALIGN;
	error = bus_dma_tag_create(
	    sc->sc_dma_tag,		/* parent */
	    EDMA_DESC_RING_ALIGN, 0,	/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    hw_ring_size,		/* maxsize */
	    1,				/* nsegments */
	    hw_ring_size,		/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &ring->hw_ring_dma_tag);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to create descriptor DMA tag (%d)\n",
		    error);
		goto error;
	}

	/*
	 * Buffer ring - used passed in value
	 */
	ring->buffer_align = buffer_align;
	error = bus_dma_tag_create(
	    sc->sc_dma_tag,		/* parent */
	    buffer_align, 0,		/* alignment, boundary */
	    BUS_SPACE_MAXADDR,		/* lowaddr */
	    BUS_SPACE_MAXADDR,		/* highaddr */
	    NULL, NULL,			/* filter, filterarg */
	    EDMA_DESC_MAX_BUFFER_SIZE * num_segments,	/* maxsize */
	    num_segments,			/* nsegments */
	    EDMA_DESC_MAX_BUFFER_SIZE,	/* maxsegsize */
	    0,				/* flags */
	    NULL, NULL,			/* lockfunc, lockarg */
	    &ring->buffer_dma_tag);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to create buffer DMA tag (%d)\n",
		    error);
		goto error;
	}

	/*
	 * Allocate software descriptors
	 */
	ring->sw_desc = mallocarray(count, sw_desc_size, M_TEMP,
	    M_NOWAIT | M_ZERO);
	if (ring->sw_desc == NULL) {
		device_printf(sc->sc_dev,
		    "ERROR: failed to allocate sw_desc\n");
		goto error;
	}

	/*
	 * Allocate hardware descriptors, initialise map, get
	 * physical address.
	 */
	error = bus_dmamem_alloc(ring->hw_ring_dma_tag,
	    (void **)&ring->hw_desc,
	     BUS_DMA_WAITOK | BUS_DMA_COHERENT | BUS_DMA_ZERO,
	    &ring->hw_desc_map);
	if (error != 0) {
		device_printf(sc->sc_dev,
		    "failed to allocate DMA'able memory for hw_desc ring\n");
		goto error;
	}
	ring->hw_desc_paddr = 0;
	error = bus_dmamap_load(ring->hw_ring_dma_tag, ring->hw_desc_map,
	    ring->hw_desc, hw_ring_size, qcom_ess_edma_desc_map_addr,
	    &ring->hw_desc_paddr, BUS_DMA_NOWAIT);
	bus_dmamap_sync(ring->hw_ring_dma_tag, ring->hw_desc_map,
	    BUS_DMASYNC_PREWRITE);
	QCOM_ESS_EDMA_DPRINTF(sc, QCOM_ESS_EDMA_DBG_DESCRIPTOR_SETUP,
	    "%s: PADDR=0x%08lx\n", __func__, ring->hw_desc_paddr);

	/*
	 * All done, initialise state.
	 */
	ring->hw_entry_size = hw_desc_size;
	ring->sw_entry_size = sw_desc_size;
	ring->ring_count = count;

	return (0);
error:
	mtx_destroy(&ring->mtx);
	if (ring->label != NULL)
		free(ring->label, M_TEMP);
	if (ring->hw_desc != NULL) {
		bus_dmamap_sync(ring->hw_ring_dma_tag, ring->hw_desc_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->hw_ring_dma_tag, ring->hw_desc_map);
		bus_dmamem_free(ring->hw_ring_dma_tag, ring->hw_desc,
		    ring->hw_desc_map);
		ring->hw_desc = NULL;
	}
	if (ring->sw_desc != NULL) {
		free(ring->sw_desc, M_TEMP);
		ring->sw_desc = NULL;
	}
	if (ring->hw_ring_dma_tag != NULL) {
		bus_dma_tag_destroy(ring->hw_ring_dma_tag);
		ring->hw_ring_dma_tag = NULL;
	}
	if (ring->buffer_dma_tag != NULL) {
		bus_dma_tag_destroy(ring->buffer_dma_tag);
		ring->buffer_dma_tag = NULL;
	}

	return (error);
}

/*
 * Free/clean the given descriptor ring.
 *
 * The ring itself right now is static; so we don't free it.
 * We just free the resources it has.
 */
int
qcom_ess_edma_desc_ring_free(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{

	mtx_destroy(&ring->mtx);
	if (ring->label != NULL)
		free(ring->label, M_TEMP);

	if (ring->hw_desc != NULL) {
		bus_dmamap_sync(ring->hw_ring_dma_tag, ring->hw_desc_map,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(ring->hw_ring_dma_tag, ring->hw_desc_map);
		bus_dmamem_free(ring->hw_ring_dma_tag, ring->hw_desc,
		    ring->hw_desc_map);
		ring->hw_desc = NULL;
	}

	if (ring->sw_desc != NULL) {
		free(ring->sw_desc, M_TEMP);
		ring->sw_desc = NULL;
	}

	if (ring->hw_ring_dma_tag != NULL) {
		bus_dma_tag_destroy(ring->hw_ring_dma_tag);
		ring->hw_ring_dma_tag = NULL;
	}
	if (ring->buffer_dma_tag != NULL) {
		bus_dma_tag_destroy(ring->buffer_dma_tag);
		ring->buffer_dma_tag = NULL;
	}

	return (0);
}

/*
 * Fetch the given software descriptor pointer by index.
 *
 * Returns NULL if the index is out of bounds.
 */
void *
qcom_ess_edma_desc_ring_get_sw_desc(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, uint16_t index)
{
	char *p;

	if (index >= ring->ring_count)
		return (NULL);

	p = (char *) ring->sw_desc;

	return (void *) (p + (ring->sw_entry_size * index));
}

/*
 * Fetch the given hardware descriptor pointer by index.
 *
 * Returns NULL if the index is out of bounds.
 */
void *
qcom_ess_edma_desc_ring_get_hw_desc(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring, uint16_t index)
{
	char *p;

	if (index >= ring->ring_count)
		return (NULL);

	p = (char *) ring->hw_desc;

	return (void *) (p + (ring->hw_entry_size * index));
}

/*
 * Flush the hardware ring after a write, before the hardware
 * gets to it.
 */
int
qcom_ess_edma_desc_ring_flush_preupdate(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{

	bus_dmamap_sync(ring->hw_ring_dma_tag, ring->hw_desc_map,
	    BUS_DMASYNC_PREWRITE);

	return (0);
}


/*
 * Flush the hardware ring after the hardware writes into it,
 * before a read.
 */
int
qcom_ess_edma_desc_ring_flush_postupdate(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{

	bus_dmamap_sync(ring->hw_ring_dma_tag, ring->hw_desc_map,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	return (0);
}

/*
 * Get how many descriptor slots are available.
 */
int
qcom_ess_edma_desc_ring_get_num_available(struct qcom_ess_edma_softc *sc,
    struct qcom_ess_edma_desc_ring *ring)
{
	uint16_t sw_next_to_fill;
	uint16_t sw_next_to_clean;
	uint16_t count = 0;

	sw_next_to_clean = ring->next_to_clean;
	sw_next_to_fill = ring->next_to_fill;

	if (sw_next_to_clean <= sw_next_to_fill)
		count = ring->ring_count;

	return (count + sw_next_to_clean - sw_next_to_fill - 1);
}
