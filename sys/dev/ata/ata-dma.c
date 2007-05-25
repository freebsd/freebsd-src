/*-
 * Copyright (c) 1998 - 2006 Søren Schmidt <sos@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/malloc.h> 
#include <sys/lock.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>

/* prototypes */
static void ata_dmaalloc(device_t);
static void ata_dmafree(device_t);
static void ata_dmasetprd(void *, bus_dma_segment_t *, int, int);
static int ata_dmaload(device_t, caddr_t, int32_t, int, void *, int *);
static int ata_dmaunload(device_t);

/* local vars */
static MALLOC_DEFINE(M_ATADMA, "ata_dma", "ATA driver DMA");

/* misc defines */
#define MAXTABSZ        PAGE_SIZE
#define MAXWSPCSZ       PAGE_SIZE*2

struct ata_dc_cb_args {
    bus_addr_t maddr;
    int error;
};

void 
ata_dmainit(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if ((ch->dma = malloc(sizeof(struct ata_dma), M_ATADMA, M_NOWAIT|M_ZERO))) {
	ch->dma->alloc = ata_dmaalloc;
	ch->dma->free = ata_dmafree;
	ch->dma->setprd = ata_dmasetprd;
	ch->dma->load = ata_dmaload;
	ch->dma->unload = ata_dmaunload;
	ch->dma->alignment = 2;
	ch->dma->boundary = 128 * DEV_BSIZE;
	ch->dma->segsize = 128 * DEV_BSIZE;
	ch->dma->max_iosize = 128 * DEV_BSIZE;
	ch->dma->max_address = BUS_SPACE_MAXADDR_32BIT;
    }
}

static void
ata_dmasetupc_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dc_cb_args *cba = (struct ata_dc_cb_args *)xsc;

    if (!(cba->error = error))
	cba->maddr = segs[0].ds_addr;
}

static void
ata_dmaalloc(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_dc_cb_args ccba;

    if (bus_dma_tag_create(NULL, ch->dma->alignment, 0,
			   ch->dma->max_address, BUS_SPACE_MAXADDR,
			   NULL, NULL, ch->dma->max_iosize,
			   ATA_DMA_ENTRIES, ch->dma->segsize,
			   0, NULL, NULL, &ch->dma->dmatag))
	goto error;

    if (bus_dma_tag_create(ch->dma->dmatag, PAGE_SIZE, PAGE_SIZE,
			   ch->dma->max_address, BUS_SPACE_MAXADDR,
			   NULL, NULL, MAXTABSZ, 1, MAXTABSZ,
			   0, NULL, NULL, &ch->dma->sg_tag))
	goto error;

    if (bus_dma_tag_create(ch->dma->dmatag,ch->dma->alignment,ch->dma->boundary,
			   ch->dma->max_address, BUS_SPACE_MAXADDR,
			   NULL, NULL, ch->dma->max_iosize,
			   ATA_DMA_ENTRIES, ch->dma->segsize,
			   0, NULL, NULL, &ch->dma->data_tag))
	goto error;

    if (bus_dmamem_alloc(ch->dma->sg_tag, (void **)&ch->dma->sg, 0,
			 &ch->dma->sg_map))
	goto error;

    if (bus_dmamap_load(ch->dma->sg_tag, ch->dma->sg_map, ch->dma->sg,
			MAXTABSZ, ata_dmasetupc_cb, &ccba, 0) || ccba.error) {
	bus_dmamem_free(ch->dma->sg_tag, ch->dma->sg, ch->dma->sg_map);
	goto error;
    }
    ch->dma->sg_bus = ccba.maddr;

    if (bus_dmamap_create(ch->dma->data_tag, 0, &ch->dma->data_map))
	goto error;

    if (bus_dma_tag_create(ch->dma->dmatag, PAGE_SIZE, 64 * 1024,
			   ch->dma->max_address, BUS_SPACE_MAXADDR,
			   NULL, NULL, MAXWSPCSZ, 1, MAXWSPCSZ,
			   0, NULL, NULL, &ch->dma->work_tag))
	goto error;

    if (bus_dmamem_alloc(ch->dma->work_tag, (void **)&ch->dma->work, 0,
			 &ch->dma->work_map))
	goto error;

    if (bus_dmamap_load(ch->dma->work_tag, ch->dma->work_map,ch->dma->work,
			MAXWSPCSZ, ata_dmasetupc_cb, &ccba, 0) || ccba.error) {
	bus_dmamem_free(ch->dma->work_tag,ch->dma->work, ch->dma->work_map);
	goto error;
    }
    ch->dma->work_bus = ccba.maddr;

    return;

error:
    device_printf(dev, "WARNING - DMA allocation failed, disabling DMA\n");
    ata_dmafree(dev);
    free(ch->dma, M_ATADMA);
    ch->dma = NULL;
}

static void
ata_dmafree(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->dma->work_bus) {
	bus_dmamap_unload(ch->dma->work_tag, ch->dma->work_map);
	bus_dmamem_free(ch->dma->work_tag, ch->dma->work, ch->dma->work_map);
	ch->dma->work_bus = 0;
	ch->dma->work_map = NULL;
	ch->dma->work = NULL;
    }
    if (ch->dma->work_tag) {
	bus_dma_tag_destroy(ch->dma->work_tag);
	ch->dma->work_tag = NULL;
    }
    if (ch->dma->sg_bus) {
	bus_dmamap_unload(ch->dma->sg_tag, ch->dma->sg_map);
	bus_dmamem_free(ch->dma->sg_tag, ch->dma->sg, ch->dma->sg_map);
	ch->dma->sg_bus = 0;
	ch->dma->sg_map = NULL;
	ch->dma->sg = NULL;
    }
    if (ch->dma->data_map) {
	bus_dmamap_destroy(ch->dma->data_tag, ch->dma->data_map);
	ch->dma->data_map = NULL;
    }
    if (ch->dma->sg_tag) {
	bus_dma_tag_destroy(ch->dma->sg_tag);
	ch->dma->sg_tag = NULL;
    }
    if (ch->dma->data_tag) {
	bus_dma_tag_destroy(ch->dma->data_tag);
	ch->dma->data_tag = NULL;
    }
    if (ch->dma->dmatag) {
	bus_dma_tag_destroy(ch->dma->dmatag);
	ch->dma->dmatag = NULL;
    }
}

static void
ata_dmasetprd(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dmasetprd_args *args = xsc;
    struct ata_dma_prdentry *prd = args->dmatab;
    int i;

    if ((args->error = error))
	return;

    for (i = 0; i < nsegs; i++) {
	prd[i].addr = htole32(segs[i].ds_addr);
	prd[i].count = htole32(segs[i].ds_len);
    }
    prd[i - 1].count |= htole32(ATA_DMA_EOT);
    args->nsegs = nsegs;
}

static int
ata_dmaload(device_t dev, caddr_t data, int32_t count, int dir,
	    void *addr, int *entries)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct ata_dmasetprd_args cba;
    int error;

    if (ch->dma->flags & ATA_DMA_LOADED) {
	device_printf(dev, "FAILURE - already active DMA on this device\n");
	return EIO;
    }
    if (!count) {
	device_printf(dev, "FAILURE - zero length DMA transfer attempted\n");
	return EIO;
    }
    if (((uintptr_t)data & (ch->dma->alignment - 1)) ||
	(count & (ch->dma->alignment - 1))) {
	device_printf(dev, "FAILURE - non aligned DMA transfer attempted\n");
	return EIO;
    }
    if (count > ch->dma->max_iosize) {
	device_printf(dev, "FAILURE - oversized DMA transfer attempt %d > %d\n",
		      count, ch->dma->max_iosize);
	return EIO;
    }

    cba.dmatab = addr;

    if ((error = bus_dmamap_load(ch->dma->data_tag, ch->dma->data_map,
				 data, count, ch->dma->setprd, &cba,
				 BUS_DMA_NOWAIT)) || (error = cba.error))
	return error;

    *entries = cba.nsegs;

    bus_dmamap_sync(ch->dma->sg_tag, ch->dma->sg_map, BUS_DMASYNC_PREWRITE);

    bus_dmamap_sync(ch->dma->data_tag, ch->dma->data_map,
		    dir ? BUS_DMASYNC_PREREAD : BUS_DMASYNC_PREWRITE);

    ch->dma->cur_iosize = count;
    ch->dma->flags = dir ? (ATA_DMA_LOADED | ATA_DMA_READ) : ATA_DMA_LOADED;
    return 0;
}

int
ata_dmaunload(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);

    if (ch->dma->flags & ATA_DMA_LOADED) {
	bus_dmamap_sync(ch->dma->sg_tag, ch->dma->sg_map,
			BUS_DMASYNC_POSTWRITE);

	bus_dmamap_sync(ch->dma->data_tag, ch->dma->data_map,
			(ch->dma->flags & ATA_DMA_READ) ?
			BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(ch->dma->data_tag, ch->dma->data_map);

	ch->dma->cur_iosize = 0;
	ch->dma->flags &= ~ATA_DMA_LOADED;
    }
    return 0;
}
