/*-
 * Copyright (c) 1998 - 2003 Søren Schmidt <sos@FreeBSD.org>
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 *
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/kernel.h>
#include <sys/endian.h>
#include <sys/malloc.h> 
#include <sys/bus.h>
#include <pci/pcivar.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>

/* prototypes */
static void ata_dmasetupc_cb(void *, bus_dma_segment_t *, int, int);
static int ata_dmaalloc(struct ata_device *);
static void ata_dmafree(struct ata_device *);
static void ata_dmacreate(struct ata_channel *);
static void ata_dmadestroy(struct ata_channel *);
static void ata_dmasetupd_cb(void *, bus_dma_segment_t *, int, int);
static int ata_dmasetup(struct ata_device *, caddr_t, int32_t);
/*
static int ata_dmastart(struct ata_device *, caddr_t, int32_t, int);
static int ata_dmastop(struct ata_device *);
*/
static int ata_dmastatus(struct ata_channel *);

/* local vars */
static MALLOC_DEFINE(M_ATADMA, "ATA DMA", "ATA driver DMA");

/* misc defines */
#define MAXSEGSZ	PAGE_SIZE
#define MAXTABSZ	PAGE_SIZE
#define MAXCTLDMASZ	(2 * (MAXTABSZ + MAXPHYS))

struct ata_dc_cb_args {
    bus_addr_t maddr;
    int error;
};

int 
ata_dmainit(struct ata_channel *ch)
{
    if (!(ch->dma = 
	malloc(sizeof(struct ata_dma_funcs), M_ATADMA, M_NOWAIT | M_ZERO)))
	return ENOMEM;
    ch->dma->create = ata_dmacreate;
    ch->dma->destroy = ata_dmadestroy;
    ch->dma->alloc = ata_dmaalloc;
    ch->dma->free = ata_dmafree;
    ch->dma->setup = ata_dmasetup;
    ch->dma->start = ata_dmastart;
    ch->dma->stop = ata_dmastop;
    ch->dma->status = ata_dmastatus;
    ch->dma->alignment = 2;
    return 0;
}


static void
ata_dmasetupc_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dc_cb_args *cba = (struct ata_dc_cb_args *)xsc;

    if (!(cba->error = error))
	cba->maddr = segs[0].ds_addr;
}

static int
ata_dmaalloc(struct ata_device *atadev)
{
    struct ata_channel *ch;
    struct ata_dc_cb_args ccba;
    struct ata_dmastate *ds;
    int error;

    ch = atadev->channel;
    ds = &atadev->dmastate;
    if (!ds->cdmatag) {
	if ((error = bus_dma_tag_create(ch->dmatag, 1, PAGE_SIZE,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					MAXTABSZ, 1, MAXTABSZ,
					BUS_DMA_ALLOCNOW, &ds->cdmatag)))
	    return error;
    }
    if (!ds->ddmatag) {
	if ((error = bus_dma_tag_create(ch->dmatag, ch->dma->alignment, 0,
					BUS_SPACE_MAXADDR_32BIT,
					BUS_SPACE_MAXADDR, NULL, NULL,
					MAXPHYS, ATA_DMA_ENTRIES, MAXSEGSZ,
					BUS_DMA_ALLOCNOW, &ds->ddmatag)))
	    return error;
    }
    if (!ds->mdmatab) {
	if ((error = bus_dmamem_alloc(ds->cdmatag, (void **)&ds->dmatab, 0,
				      &ds->cdmamap)))
	    return error;

	if ((error = bus_dmamap_load(ds->cdmatag, ds->cdmamap, ds->dmatab,
				     MAXTABSZ, ata_dmasetupc_cb, &ccba,
				     0)) != 0 || ccba.error != 0) {
	    bus_dmamem_free(ds->cdmatag, ds->dmatab, ds->cdmamap);
	    return error;
	}
	ds->mdmatab = ccba.maddr;
    }
    if (!ds->ddmamap) {
	if ((error = bus_dmamap_create(ds->ddmatag, 0, &ds->ddmamap)) != 0)
	    return error;
    }
    return 0;
}

static void
ata_dmafree(struct ata_device *atadev)
{
    struct ata_dmastate *ds;

    ds = &atadev->dmastate;
    if (ds->mdmatab) {
	bus_dmamap_unload(ds->cdmatag, ds->cdmamap);
	bus_dmamem_free(ds->cdmatag, ds->dmatab, ds->cdmamap);
	ds->mdmatab = 0;
	ds->cdmamap = NULL;
	ds->dmatab = NULL;
    }
    if (ds->ddmamap) {
	bus_dmamap_destroy(ds->ddmatag, ds->ddmamap);
	ds->ddmamap = NULL;
    }
    if (ds->cdmatag) {
	bus_dma_tag_destroy(ds->cdmatag);
	ds->cdmatag = NULL;
    }
    if (ds->ddmatag) {
	bus_dma_tag_destroy(ds->ddmatag);
	ds->ddmatag = NULL;
    }
}

static void
ata_dmacreate(struct ata_channel *ch)
{

    if (!ch->dmatag) {
	if (bus_dma_tag_create(NULL, 1, 0,
			       BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
			       NULL, NULL, MAXCTLDMASZ, ATA_DMA_ENTRIES,
			       BUS_SPACE_MAXSIZE_32BIT, 0, &ch->dmatag)) {
	    printf("DMA tag allocation failed, disabling DMA\n");
	}
    }
}

static void
ata_dmadestroy(struct ata_channel *ch)
{

    if (ch->dmatag) {
	bus_dma_tag_destroy(ch->dmatag);
	ch->dmatag = NULL;
    }
}

struct ata_dmasetup_data_cb_args {
    struct ata_dmaentry *dmatab;
    int error;
};

static void
ata_dmasetupd_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dmasetup_data_cb_args *cba =
	(struct ata_dmasetup_data_cb_args *)xsc;
    bus_size_t cnt;
    u_int32_t lastcount;
    int i, j;

    cba->error = error;
    if (error != 0)
	return;
    lastcount = j = 0;
    for (i = 0; i < nsegs; i++) {
	/*
	 * A maximum segment size was specified for bus_dma_tag_create, but
	 * some busdma code does not seem to honor this, so fix up if needed.
	 */
	for (cnt = 0; cnt < segs[i].ds_len; cnt += MAXSEGSZ, j++) {
	    cba->dmatab[j].base = htole32(segs[i].ds_addr + cnt);
	    lastcount = ulmin(segs[i].ds_len - cnt, MAXSEGSZ) & 0xffff;
	    cba->dmatab[j].count = htole32(lastcount);
	}
    }
    cba->dmatab[j - 1].count = htole32(lastcount | ATA_DMA_EOT);
}

static int
ata_dmasetup(struct ata_device *atadev, caddr_t data, int32_t count)
{
    struct ata_channel *ch = atadev->channel;

    if (((uintptr_t)data & (ch->dma->alignment - 1)) ||
	(count & (ch->dma->alignment - 1))) {
	ata_prtdev(atadev, "non aligned DMA transfer attempted\n");
	return -1;
    }

    if (!count) {
	ata_prtdev(atadev, "zero length DMA transfer attempted\n");
	return -1;
    }
    return 0;
}

int
ata_dmastart(struct ata_device *atadev, caddr_t data, int32_t count, int dir)
{
    struct ata_channel *ch = atadev->channel;
    struct ata_dmastate *ds = &atadev->dmastate;
    struct ata_dmasetup_data_cb_args cba;

    if (ds->flags & ATA_DS_ACTIVE)
	    panic("ata_dmasetup: transfer active on this device!");

    cba.dmatab = ds->dmatab;
    bus_dmamap_sync(ds->cdmatag, ds->cdmamap, BUS_DMASYNC_PREWRITE);
    if (bus_dmamap_load(ds->ddmatag, ds->ddmamap, data, count,
			ata_dmasetupd_cb, &cba, 0) || cba.error)
	return -1;

    bus_dmamap_sync(ds->cdmatag, ds->cdmamap, BUS_DMASYNC_POSTWRITE);
    bus_dmamap_sync(ds->ddmatag, ds->ddmamap, dir ? BUS_DMASYNC_PREREAD :
		    BUS_DMASYNC_PREWRITE);

    ch->flags |= ATA_DMA_ACTIVE;
    ds->flags = dir ? (ATA_DS_ACTIVE | ATA_DS_READ) : ATA_DS_ACTIVE;

    ATA_OUTL(ch->r_bmio, ATA_BMDTP_PORT, ds->mdmatab);
    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, dir ? ATA_BMCMD_WRITE_READ : 0);
    ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, 
	     (ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) | 
	     (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, 
	     ATA_INB(ch->r_bmio, ATA_BMCMD_PORT) | ATA_BMCMD_START_STOP);
    return 0;
}

int
ata_dmastop(struct ata_device *atadev)
{
    struct ata_channel *ch = atadev->channel;
    struct ata_dmastate *ds = &atadev->dmastate;
    int error;

    error = ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT);
    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, 
	     ATA_INB(ch->r_bmio, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT,ATA_BMSTAT_INTERRUPT|ATA_BMSTAT_ERROR);

    bus_dmamap_sync(ds->ddmatag, ds->ddmamap, (ds->flags & ATA_DS_READ) != 0 ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
    bus_dmamap_unload(ds->ddmatag, ds->ddmamap);

    ch->flags &= ~ATA_DMA_ACTIVE;
    ds->flags = 0;
    return (error & ATA_BMSTAT_MASK);
}

static int
ata_dmastatus(struct ata_channel *ch)
{
    return ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}
