/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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
#include <sys/endian.h>
#include <sys/malloc.h> 
#include <sys/bus.h>
#include <pci/pcivar.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>

/* prototypes */
static void ata_dmacreate(struct ata_device *, int, int);
static void ata_dmasetupd_cb(void *, bus_dma_segment_t *, int, int);
static void ata_dmasetupc_cb(void *, bus_dma_segment_t *, int, int);
static void cyrix_timing(struct ata_device *, int, int);
static void promise_timing(struct ata_device *, int, int);
static void hpt_timing(struct ata_device *, int, int);
static int hpt_cable80(struct ata_device *);

/* misc defines */
#define ATAPI_DEVICE(atadev) \
	((atadev->unit == ATA_MASTER && \
	  atadev->channel->devices & ATA_ATAPI_MASTER) || \
	 (atadev->unit == ATA_SLAVE && \
	  atadev->channel->devices & ATA_ATAPI_SLAVE))

#define	MAXSEGSZ	PAGE_SIZE
#define	MAXTABSZ	PAGE_SIZE
#define	MAXCTLDMASZ	(2 * (MAXTABSZ + MAXPHYS))

struct ata_dc_cb_args {
    bus_addr_t maddr;
    int error;
};

static void
ata_dmasetupc_cb(void *xsc, bus_dma_segment_t *segs, int nsegs, int error)
{
    struct ata_dc_cb_args *cba = (struct ata_dc_cb_args *)xsc;

    if (!(cba->error = error))
	cba->maddr = segs[0].ds_addr;
}

int
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
	if ((error = bus_dma_tag_create(ch->dmatag, ch->alignment + 1, 0,
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

void
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

void
ata_dmafreetags(struct ata_channel *ch)
{

    if (ch->dmatag) {
	bus_dma_tag_destroy(ch->dmatag);
	ch->dmatag = NULL;
    }
}

static void
ata_dmacreate(struct ata_device *atadev, int apiomode, int mode)
{

    atadev->mode = mode;
    if (!atadev->channel->dmatag) {
	if (bus_dma_tag_create(NULL, 1, 0,
			       BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
			       NULL, NULL, MAXCTLDMASZ, ATA_DMA_ENTRIES,
			       BUS_SPACE_MAXSIZE_32BIT, 0,
			       &atadev->channel->dmatag)) {
	    ata_prtdev(atadev, "DMA tag allocation failed, disabling DMA\n");
	    ata_dmainit(atadev, apiomode, -1, -1);
	}
    }
}

void
ata_dmainit(struct ata_device *atadev, int apiomode, int wdmamode, int udmamode)
{
    device_t parent = device_get_parent(atadev->channel->dev);
    int chiptype = atadev->channel->chiptype;
    int chiprev = pci_get_revid(parent);
    int channel = atadev->channel->unit;
    int device = ATA_DEV(atadev->unit);
    int devno = (channel << 1) + device;
    int error;

    /* set our most pessimistic default mode */
    atadev->mode = ATA_PIO;

    if (!atadev->channel->r_bmio)
	return;

    /* if simplex controller, only allow DMA on primary channel */
    if (channel == 1) {
	ATA_OUTB(atadev->channel->r_bmio, ATA_BMSTAT_PORT,
		 ATA_INB(atadev->channel->r_bmio, ATA_BMSTAT_PORT) &
		 (ATA_BMSTAT_DMA_MASTER | ATA_BMSTAT_DMA_SLAVE));
	if (ATA_INB(atadev->channel->r_bmio, ATA_BMSTAT_PORT) &
	    ATA_BMSTAT_DMA_SIMPLEX) {
	    ata_prtdev(atadev, "simplex device, DMA on primary only\n");
	    return;
	}
    }

    /* DMA engine address alignment is usually 1 word (2 bytes) */
    atadev->channel->alignment = 0x1;

#if 1
    if (udmamode > 2 && !atadev->param->hwres_cblid) {
	ata_prtdev(atadev,"DMA limited to UDMA33, non-ATA66 cable or device\n");
	udmamode = 2;
    }
#endif
    switch (chiptype) {

    case 0x24cb8086:	/* Intel ICH4 */
    case 0x248a8086:	/* Intel ICH3 mobile */ 
    case 0x248b8086:	/* Intel ICH3 */
    case 0x244a8086:	/* Intel ICH2 mobile */ 
    case 0x244b8086:	/* Intel ICH2 */
	if (udmamode >= 5) {
	    int32_t mask48, new48;
	    int16_t word54;

	    word54 = pci_read_config(parent, 0x54, 2);
	    if (word54 & (0x10 << devno)) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA5,	ATA_C_F_SETXFER,ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA5 on Intel chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
		    new48 = (1 << devno) + (1 << (16 + (devno << 2)));
		    pci_write_config(parent, 0x48,
				     (pci_read_config(parent, 0x48, 4) &
				     ~mask48) | new48, 4);
		    pci_write_config(parent, 0x54, word54 | (0x1000<<devno), 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		    return;
		}
	    }
	}
	/* make sure eventual ATA100 mode from the BIOS is disabled */
	pci_write_config(parent, 0x54, 
			 pci_read_config(parent, 0x54, 2) & ~(0x1000<<devno),2);
	/* FALLTHROUGH */

    case 0x24118086:	/* Intel ICH */
    case 0x76018086:	/* Intel ICH */
	if (udmamode >= 4) {
	    int32_t mask48, new48;
	    int16_t word54;

	    word54 = pci_read_config(parent, 0x54, 2);
	    if (word54 & (0x10 << devno)) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on Intel chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
		    new48 = (1 << devno) + (2 << (16 + (devno << 2)));
		    pci_write_config(parent, 0x48,
				     (pci_read_config(parent, 0x48, 4) &
				     ~mask48) | new48, 4);
		    pci_write_config(parent, 0x54, word54 | (1 << devno), 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		    return;
		}
	    }
	}	    
	/* make sure eventual ATA66 mode from the BIOS is disabled */
	pci_write_config(parent, 0x54, 
			 pci_read_config(parent, 0x54, 2) & ~(1 << devno), 2);
	/* FALLTHROUGH */

    case 0x71118086:	/* Intel PIIX4 */
    case 0x84CA8086:	/* Intel PIIX4 */
    case 0x71998086:	/* Intel PIIX4e */
    case 0x24218086:	/* Intel ICH0 */
	if (udmamode >= 2) {
	    int32_t mask48, new48;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Intel chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
		new48 = (1 << devno) + (2 << (16 + (devno << 2)));
		pci_write_config(parent, 0x48, 
				 (pci_read_config(parent, 0x48, 4) &
				 ~mask48) | new48, 4);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	/* make sure eventual ATA33 mode from the BIOS is disabled */
	pci_write_config(parent, 0x48, 
			 pci_read_config(parent, 0x48, 4) & ~(1 << devno), 4);
	/* FALLTHROUGH */

    case 0x70108086:	/* Intel PIIX3 */
	if (wdmamode >= 2 && apiomode >= 4) {
	    int32_t mask40, new40, mask44, new44;

	    /* if SITRE not set doit for both channels */
	    if (!((pci_read_config(parent, 0x40, 4) >> (channel<<8)) & 0x4000)){
		new40 = pci_read_config(parent, 0x40, 4);
		new44 = pci_read_config(parent, 0x44, 4); 
		if (!(new40 & 0x00004000)) {
		    new44 &= ~0x0000000f;
		    new44 |= ((new40&0x00003000)>>10)|((new40&0x00000300)>>8);
		}
		if (!(new40 & 0x40000000)) {
		    new44 &= ~0x000000f0;
		    new44 |= ((new40&0x30000000)>>22)|((new40&0x03000000)>>20);
		}
		new40 |= 0x40004000;
		pci_write_config(parent, 0x40, new40, 4);
		pci_write_config(parent, 0x44, new44, 4);
	    }
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Intel chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		if (device == ATA_MASTER) {
		    mask40 = 0x0000330f;
		    new40 = 0x00002307;
		    mask44 = 0;
		    new44 = 0;
		}
		else {
		    mask40 = 0x000000f0;
		    new40 = 0x00000070;
		    mask44 = 0x0000000f;
		    new44 = 0x0000000b;
		}
		if (channel) {
		    mask40 <<= 16;
		    new40 <<= 16;
		    mask44 <<= 4;
		    new44 <<= 4;
		}
		pci_write_config(parent, 0x40,
				 (pci_read_config(parent, 0x40, 4) & ~mask40)|
				 new40, 4);
		pci_write_config(parent, 0x44,
				 (pci_read_config(parent, 0x44, 4) & ~mask44)|
				 new44, 4);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x12308086:	/* Intel PIIX */
	if (wdmamode >= 2 && apiomode >= 4) {
	    int32_t word40;

	    word40 = pci_read_config(parent, 0x40, 4);
	    word40 >>= channel * 16;

	    /* Check for timing config usable for DMA on controller */
	    if (!((word40 & 0x3300) == 0x2300 &&
		  ((word40 >> (device ? 4 : 0)) & 1) == 1))
		break;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Intel chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	break;

    case 0x522910b9:	/* AcerLabs Aladdin IV/V */
	/* the older Aladdin doesn't support ATAPI DMA on both master & slave */
	if (chiprev < 0xc2 &&
	    atadev->channel->devices & ATA_ATAPI_MASTER &&
	    atadev->channel->devices & ATA_ATAPI_SLAVE) {
	    ata_prtdev(atadev, "two atapi devices on this channel, no DMA\n");
	    break;
	}
	pci_write_config(parent, 0x58 + (channel << 2), 0x00310001, 4);
	if (udmamode >= 5 && chiprev >= 0xc4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on Acer chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		int32_t word54 = pci_read_config(parent, 0x54, 4);
	
		pci_write_config(parent, 0x4b,
				 pci_read_config(parent, 0x4b, 1) | 0x01, 1);
		word54 &= ~(0x000f000f << (devno << 2));
		word54 |= (0x000f0005 << (devno << 2));
		pci_write_config(parent, 0x54, word54, 4);
		pci_write_config(parent, 0x53, 
				 pci_read_config(parent, 0x53, 1) | 0x03, 1);
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	if (udmamode >= 4 && chiprev >= 0xc2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on Acer chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		int32_t word54 = pci_read_config(parent, 0x54, 4);
	
		pci_write_config(parent, 0x4b,
				 pci_read_config(parent, 0x4b, 1) | 0x01, 1);
		word54 &= ~(0x000f000f << (devno << 2));
		word54 |= (0x00080005 << (devno << 2));
		pci_write_config(parent, 0x54, word54, 4);
		pci_write_config(parent, 0x53, 
				 pci_read_config(parent, 0x53, 1) | 0x03, 1);
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	if (udmamode >= 2 && chiprev >= 0x20) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Acer chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		int32_t word54 = pci_read_config(parent, 0x54, 4);
	
		word54 &= ~(0x000f000f << (devno << 2));
		word54 |= (0x000a0005 << (devno << 2));
		pci_write_config(parent, 0x54, word54, 4);
		pci_write_config(parent, 0x53, 
				 pci_read_config(parent, 0x53, 1) | 0x03, 1);
		atadev->channel->flags |= ATA_ATAPI_DMA_RO;
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}

	/* make sure eventual UDMA mode from the BIOS is disabled */
	pci_write_config(parent, 0x56, pci_read_config(parent, 0x56, 2) &
				       ~(0x0008 << (devno << 2)), 2);

	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Acer chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x53, 
				 pci_read_config(parent, 0x53, 1) | 0x03, 1);
		atadev->channel->flags |= ATA_ATAPI_DMA_RO;
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	pci_write_config(parent, 0x53,
			 (pci_read_config(parent, 0x53, 1) & ~0x01) | 0x02, 1);
	error = ata_command(atadev, ATA_C_SETFEATURES, 0,
			    ATA_PIO0 + apiomode,
			    ATA_C_F_SETXFER, ATA_WAIT_READY);
	if (bootverbose)
	    ata_prtdev(atadev, "%s setting PIO%d on Acer chip\n",
		       (error) ? "failed" : "success",
		       (apiomode >= 0) ? apiomode : 0);
	if (!error) {
	    int32_t word54 = pci_read_config(parent, 0x54, 4);
	    int32_t timing;

	    switch(ATA_PIO0 + apiomode) {
	    case ATA_PIO0: timing = 0x006d0003;
	    case ATA_PIO1: timing = 0x00580002;
	    case ATA_PIO2: timing = 0x00440001;
	    case ATA_PIO3: timing = 0x00330001;
	    case ATA_PIO4: timing = 0x00310001;
	    default:	   timing = 0x006d0003;
	    }
	    pci_write_config(parent, 0x58 + (channel << 2), timing, 4);
	    word54 &= ~(0x000f000f << (devno << 2));
	    word54 |= (0x00000004 << (devno << 2));
	    pci_write_config(parent, 0x54, word54, 4);
	    atadev->mode = ATA_PIO0 + apiomode;
	    return;
	}
	break;

    case 0x01bc10de:	/* nVIDIA nForce */
    case 0x74411022:	/* AMD 768 */
    case 0x74111022:	/* AMD 766 */
    case 0x74091022:	/* AMD 756 */
    case 0x05711106:	/* VIA 82C571, 82C586, 82C596, 82C686, 8231,8233,8235 */
	{
	    int via_modes[5][7] = {
		{ 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00 },	/* VIA ATA33 */
		{ 0x00, 0x00, 0xea, 0x00, 0xe8, 0x00, 0x00 },	/* VIA ATA66 */
		{ 0x00, 0x00, 0xf4, 0x00, 0xf1, 0xf0, 0x00 },	/* VIA ATA100 */
		{ 0x00, 0x00, 0xf6, 0x00, 0xf2, 0xf1, 0xf0 },	/* VIA ATA133 */
		{ 0x00, 0x00, 0xc0, 0x00, 0xc5, 0xc6, 0x00 }};	/* AMD/nVIDIA */
	    int *reg_val = NULL;
	    char *chip = "VIA";

	    if (ata_find_dev(parent, 0x31471106, 0) ||		/* 8233a */
		ata_find_dev(parent, 0x31771106, 0)) {		/* 8235 */
		udmamode = imin(udmamode, 6);
		reg_val = via_modes[3];
	    }
	    else if (ata_find_dev(parent, 0x06861106, 0x40) ||	/* 82C686b */
		ata_find_dev(parent, 0x82311106, 0) ||		/* 8231 */
		ata_find_dev(parent, 0x30741106, 0) ||		/* 8233 */
		ata_find_dev(parent, 0x31091106, 0)) {		/* 8233c */
		udmamode = imin(udmamode, 5);
		reg_val = via_modes[2];
	    }
	    else if (ata_find_dev(parent, 0x06861106, 0x10) ||	/* 82C686a */
		     ata_find_dev(parent, 0x05961106, 0x12)) {	/* 82C596b */
		udmamode = imin(udmamode, 4);
		reg_val = via_modes[1];
	    }
	    else if (ata_find_dev(parent, 0x06861106, 0)) {	/* 82C686 */
		udmamode = imin(udmamode, 2);
		reg_val = via_modes[1];
	    }
	    else if (ata_find_dev(parent, 0x05961106, 0) ||	/* 82C596a */
		     ata_find_dev(parent, 0x05861106, 0x03)) {	/* 82C586b */
		udmamode = imin(udmamode, 2);
		reg_val = via_modes[0];
	    }
	    else if (chiptype == 0x74411022 ||			/* AMD 768 */
		     chiptype == 0x74111022) {			/* AMD 766 */
		udmamode = imin(udmamode, 5);
		reg_val = via_modes[4];
		chip = "AMD";
	    }
	    else if (chiptype == 0x74091022) {			/* AMD 756 */
		udmamode = imin(udmamode, 4);
		reg_val = via_modes[4];
		chip = "AMD";
	    }
	    else if (chiptype == 0x01bc10de) {			/* nVIDIA */
		udmamode = imin(udmamode, 5);
		reg_val = via_modes[4];
		chip = "nVIDIA";
	    }
	    else 
		udmamode = 0;

	    if (udmamode >= 6) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA6 on %s chip\n",
			       (error) ? "failed" : "success", chip);
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[6], 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA6);
		    return;
		}
	    }
	    if (udmamode >= 5) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA5 on %s chip\n",
			       (error) ? "failed" : "success", chip);
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[5], 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		    return;
		}
	    }
	    if (udmamode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on %s chip\n",
			       (error) ? "failed" : "success", chip);
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[4], 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		    return;
		}
	    }
	    if (udmamode >= 2) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA2 on %s chip\n",
			       (error) ? "failed" : "success", chip);
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[2], 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		    return;
		}
	    }
	    if (wdmamode >= 2 && apiomode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting WDMA2 on %s chip\n",
			       (error) ? "failed" : "success", chip);
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, 0x0b, 1);
		    pci_write_config(parent, 0x4b - devno, 0x31, 1);
		    ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		    return;
		}
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x55131039:	/* SiS 5591 */
	if (ata_find_dev(parent, 0x06301039, 0x30) ||	/* SiS 630 */
	    ata_find_dev(parent, 0x06331039, 0) ||	/* SiS 633 */
	    ata_find_dev(parent, 0x06351039, 0) ||	/* SiS 635 */
	    ata_find_dev(parent, 0x06401039, 0) ||	/* SiS 640 */
	    ata_find_dev(parent, 0x06451039, 0) ||	/* SiS 645 */
	    ata_find_dev(parent, 0x06501039, 0) ||	/* SiS 650 */
	    ata_find_dev(parent, 0x07301039, 0) ||	/* SiS 730 */
	    ata_find_dev(parent, 0x07331039, 0) ||	/* SiS 733 */
	    ata_find_dev(parent, 0x07351039, 0) ||	/* SiS 735 */
	    ata_find_dev(parent, 0x07401039, 0) ||	/* SiS 740 */
	    ata_find_dev(parent, 0x07451039, 0) ||	/* SiS 745 */
	    ata_find_dev(parent, 0x07501039, 0)) {	/* SiS 750 */
	    int8_t reg = 0x40 + (devno << 1);
	    int16_t val = pci_read_config(parent, reg, 2) & 0x0fff;

	    if (udmamode >= 5) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA5 on SiS chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, reg, val | 0x8000, 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		    return;
		}
	    }
	    if (udmamode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on SiS chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, reg, val | 0x9000, 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		    return;
		}
	    }
	    if (udmamode >= 2) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA2 on SiS chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, reg, val | 0xb000, 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		    return;
		}
	    }
	} else if (ata_find_dev(parent, 0x05301039, 0) || /* SiS 530 */
		   ata_find_dev(parent, 0x05401039, 0) || /* SiS 540 */
		   ata_find_dev(parent, 0x06201039, 0) || /* SiS 620 */
		   ata_find_dev(parent, 0x06301039, 0)) { /* SiS 630 */
	    int8_t reg = 0x40 + (devno << 1);
	    int16_t val = pci_read_config(parent, reg, 2) & 0x0fff;

	    if (udmamode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on SiS chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, reg, val | 0x9000, 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		    return;
		}
	    }
	    if (udmamode >= 2) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA2 on SiS chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, reg, val | 0xa000, 2);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		    return;
		}
	    }
	} else if (udmamode >= 2 && chiprev > 0xc1) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on SiS chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x40 + (devno << 1), 0xa301, 2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (wdmamode >=2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on SiS chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x40 + (devno << 1), 0x0301, 2);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x06801095:	/* Sil 0680 ATA133 controller */
	{
	    u_int8_t ureg = 0xac + (device * 0x02) + (channel * 0x10);
	    u_int8_t uval = pci_read_config(parent, ureg, 1);
	    u_int8_t mreg = channel ? 0x84 : 0x80;
	    u_int8_t mask = device ? 0x30 : 0x03;
	    u_int8_t mode = pci_read_config(parent, mreg, 1);

	    /* enable UDMA mode */
	    pci_write_config(parent, mreg,
			     (mode & ~mask) | (device ? 0x30 : 0x03), 1);
    	    if (udmamode >= 6) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA6 on Sil chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, ureg, (uval & 0x3f) | 0x01, 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA6);
		    return;
		}
	    }
    	    if (udmamode >= 5) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA5 on Sil chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, ureg, (uval & 0x3f) | 0x02, 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		    return;
		}
	    }
    	    if (udmamode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on Sil chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, ureg, (uval & 0x3f) | 0x03, 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		    return;
		}
	    }
    	    if (udmamode >= 2) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA2 on Sil chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, ureg, (uval & 0x3f) | 0x07, 1);
		    ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		    return;
		}
	    }

	    /* disable UDMA mode and enable WDMA mode */
	    pci_write_config(parent, mreg,
			     (mode & ~mask) | (device ? 0x20 : 0x02), 1);
	    if (wdmamode >= 2 && apiomode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting WDMA2 on Sil chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, ureg - 0x4, 0x10c1, 2);
		    ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		    return;
		}
	    }

	    /* restore PIO mode */
	    pci_write_config(parent, mreg, mode, 1);
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x06491095:	/* CMD 649 ATA100 controller */
	if (udmamode >= 5) {
	    u_int8_t umode;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on CMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		umode = pci_read_config(parent, channel ? 0x7b : 0x73, 1);
		umode &= ~(device ? 0xca : 0x35);
		umode |= (device ? 0x0a : 0x05);
		pci_write_config(parent, channel ? 0x7b : 0x73, umode, 1);
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x06481095:	/* CMD 648 ATA66 controller */
	if (udmamode >= 4) {
	    u_int8_t umode;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on CMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		umode = pci_read_config(parent, channel ? 0x7b : 0x73, 1);
		umode &= ~(device ? 0xca : 0x35);
		umode |= (device ? 0x4a : 0x15);
		pci_write_config(parent, channel ? 0x7b : 0x73, umode, 1);
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	if (udmamode >= 2) {
	    u_int8_t umode;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on CMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		umode = pci_read_config(parent, channel ? 0x7b : 0x73, 1);
		umode &= ~(device ? 0xca : 0x35);
		umode |= (device ? 0x42 : 0x11);
		pci_write_config(parent, channel ? 0x7b : 0x73, umode, 1);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	/* make sure eventual UDMA mode from the BIOS is disabled */
	pci_write_config(parent, channel ? 0x7b : 0x73, 
			 pci_read_config(parent, channel ? 0x7b : 0x73, 1) &
					 ~(device ? 0xca : 0x53), 1);
	/* FALLTHROUGH */

    case 0x06461095:	/* CMD 646 ATA controller */
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on CMD chip\n",
			   error ? "failed" : "success");
	    if (!error) {
		int32_t offset = (devno < 3) ? (devno << 1) : 7;

		pci_write_config(parent, 0x54 + offset, 0x3f, 1);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0xc6931080:	/* Cypress 82c693 ATA controller */
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Cypress chip\n",
			   error ? "failed" : "success");
	    if (!error) {
		pci_write_config(atadev->channel->dev,
				 channel ? 0x4e:0x4c, 0x2020, 2);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x01021078:	/* Cyrix 5530 ATA33 controller */
	atadev->channel->alignment = 0xf;
	if (udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Cyrix chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		cyrix_timing(atadev, devno, ATA_UDMA2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Cyrix chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		cyrix_timing(atadev, devno, ATA_WDMA2);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	error = ata_command(atadev, ATA_C_SETFEATURES, 0,
			    ATA_PIO0 + apiomode, ATA_C_F_SETXFER,
			    ATA_WAIT_READY);
	if (bootverbose)
	    ata_prtdev(atadev, "%s setting %s on Cyrix chip\n",
		       (error) ? "failed" : "success",
		       ata_mode2str(ATA_PIO0 + apiomode));
	cyrix_timing(atadev, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;

    case 0x02121166:	/* ServerWorks CSB5 ATA66/100 controller */
	if (udmamode >= 5 && chiprev >= 0x92) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on ServerWorks chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		u_int16_t reg56;

		pci_write_config(parent, 0x54, 
				 pci_read_config(parent, 0x54, 1) |
				 (0x01 << devno), 1);
		reg56 = pci_read_config(parent, 0x56, 2);
		reg56 &= ~(0xf << (devno * 4));
		reg56 |= (0x5 << (devno * 4));
		pci_write_config(parent, 0x56, reg56, 2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	if (udmamode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on ServerWorks chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		u_int16_t reg56;

		pci_write_config(parent, 0x54, 
				 pci_read_config(parent, 0x54, 1) |
				 (0x01 << devno), 1);
		reg56 = pci_read_config(parent, 0x56, 2);
		reg56 &= ~(0xf << (devno * 4));
		reg56 |= (0x4 << (devno * 4));
		pci_write_config(parent, 0x56, reg56, 2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x02111166:	/* ServerWorks ROSB4 ATA33 controller */
	if (udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on ServerWorks chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		u_int16_t reg56;

		pci_write_config(parent, 0x54, 
				 pci_read_config(parent, 0x54, 1) |
				 (0x01 << devno), 1);
		reg56 = pci_read_config(parent, 0x56, 2);
		reg56 &= ~(0xf << (devno * 4));
		reg56 |= (0x2 << (devno * 4));
		pci_write_config(parent, 0x56, reg56, 2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on ServerWorks chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		int offset = devno ^ 0x01;
		int word44 = pci_read_config(parent, 0x44, 4);

		pci_write_config(parent, 0x54,
				 pci_read_config(parent, 0x54, 1) &
				 ~(0x01 << devno), 1);
		word44 &= ~(0xff << (offset << 8));
		word44 |= (0x20 << (offset << 8));
		pci_write_config(parent, 0x44, 0x20, 4);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x4d69105a:	/* Promise TX2 ATA133 controllers */
    case 0x5275105a:	/* Promise TX2 ATA133 controllers */
    case 0x6269105a:	/* Promise TX2 ATA133 controllers */
	ATA_OUTB(atadev->channel->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 6 &&
	    !(ATA_INB(atadev->channel->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA6 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_UDMA6);
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x4d68105a:	/* Promise TX2 ATA100 controllers */
    case 0x6268105a:	/* Promise TX2 ATA100 controllers */
	ATA_OUTB(atadev->channel->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 5 &&
	    !(ATA_INB(atadev->channel->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	ATA_OUTB(atadev->channel->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 4 &&
	    !(ATA_INB(atadev->channel->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	if (udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	break;

    case 0x0d30105a:	/* Promise OEM ATA100 controllers */
    case 0x4d30105a:	/* Promise Ultra/FastTrak 100 controllers */
	if (!ATAPI_DEVICE(atadev) && udmamode >= 5 && 
	    !(pci_read_config(parent, 0x50, 2) & (channel ? 1<<11 : 1<<10))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(atadev, devno, ATA_UDMA5);
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x0d38105a:	/* Promise FastTrak 66 controllers */
    case 0x4d38105a:	/* Promise Ultra/FastTrak 66 controllers */
	if (!ATAPI_DEVICE(atadev) && udmamode >= 4 && 
	    !(pci_read_config(parent, 0x50, 2) & (channel ? 1<<11 : 1<<10))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(atadev, devno, ATA_UDMA4);
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x4d33105a:	/* Promise Ultra/FastTrak 33 controllers */
	if (!ATAPI_DEVICE(atadev) && udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(atadev, devno, ATA_UDMA2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (!ATAPI_DEVICE(atadev) && wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(atadev, devno, ATA_WDMA2);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	error = ata_command(atadev, ATA_C_SETFEATURES, 0,
			    ATA_PIO0 + apiomode, 
			    ATA_C_F_SETXFER, ATA_WAIT_READY);
	if (bootverbose)
	    ata_prtdev(atadev, "%s setting PIO%d on Promise chip\n",
		       (error) ? "failed" : "success",
		       (apiomode >= 0) ? apiomode : 0);
	promise_timing(atadev, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;
    
    case 0x00041103:	/* HighPoint HPT366/368/370/372 controllers */
    case 0x00051103:	/* HighPoint HPT372 controllers */
    case 0x00081103:	/* HighPoint HPT374 controllers */
	if (!ATAPI_DEVICE(atadev) && udmamode >= 6 && hpt_cable80(atadev) &&
	    ((chiptype == 0x00041103 && chiprev >= 0x05) ||
	     (chiptype == 0x00051103 && chiprev >= 0x01) ||
	     (chiptype == 0x00081103 && chiprev >= 0x07))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA6 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(atadev, devno, ATA_UDMA6);
		ata_dmacreate(atadev, apiomode, ATA_UDMA6);
		return;
	    }
	}
	if (!ATAPI_DEVICE(atadev) && udmamode >= 5 && hpt_cable80(atadev) &&
	    ((chiptype == 0x00041103 && chiprev >= 0x03) ||
	     (chiptype == 0x00051103 && chiprev >= 0x01) ||
	     (chiptype == 0x00081103 && chiprev >= 0x07))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(atadev, devno, ATA_UDMA5);
		ata_dmacreate(atadev, apiomode, ATA_UDMA5);
		return;
	    }
	}
	if (!ATAPI_DEVICE(atadev) && udmamode >= 4 && hpt_cable80(atadev)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(atadev, devno, ATA_UDMA4);
		ata_dmacreate(atadev, apiomode, ATA_UDMA4);
		return;
	    }
	}
	if (!ATAPI_DEVICE(atadev) && udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(atadev, devno, ATA_UDMA2);
		ata_dmacreate(atadev, apiomode, ATA_UDMA2);
		return;
	    }
	}
	if (!ATAPI_DEVICE(atadev) && wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(atadev, devno, ATA_WDMA2);
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
	error = ata_command(atadev, ATA_C_SETFEATURES, 0,
			    ATA_PIO0 + apiomode, 
			    ATA_C_F_SETXFER, ATA_WAIT_READY);
	if (bootverbose)
	    ata_prtdev(atadev, "%s setting PIO%d on HighPoint chip\n",
		       (error) ? "failed" : "success",
		       (apiomode >= 0) ? apiomode : 0);
	hpt_timing(atadev, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;

    case 0x000116ca:	/* Cenatek Rocket Drive controller */
	if (wdmamode >= 0 &&
	    (ATA_INB(atadev->channel->r_bmio, ATA_BMSTAT_PORT) & 
	     (device ? ATA_BMSTAT_DMA_SLAVE : ATA_BMSTAT_DMA_MASTER)))
	    ata_dmacreate(atadev, apiomode, ATA_DMA);
	else
	    atadev->mode = ATA_PIO;
	return;

    default:		/* unknown controller chip */
	/* better not try generic DMA on ATAPI devices it almost never works */
	if (ATAPI_DEVICE(atadev))
	    break;

	/* if controller says its setup for DMA take the easy way out */
	/* the downside is we dont know what DMA mode we are in */
	if ((udmamode >= 0 || wdmamode >= 2) &&
	    (ATA_INB(atadev->channel->r_bmio, ATA_BMSTAT_PORT) &
	     (device ? ATA_BMSTAT_DMA_SLAVE : ATA_BMSTAT_DMA_MASTER))) {
	    ata_dmacreate(atadev, apiomode, ATA_DMA);
	    return;
	}

	/* well, we have no support for this, but try anyways */
	if ((wdmamode >= 2 && apiomode >= 4) && atadev->channel->r_bmio) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on generic chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		ata_dmacreate(atadev, apiomode, ATA_WDMA2);
		return;
	    }
	}
    }
    error = ata_command(atadev, ATA_C_SETFEATURES, 0, ATA_PIO0 + apiomode,
			ATA_C_F_SETXFER, ATA_WAIT_READY);
    if (bootverbose)
	ata_prtdev(atadev, "%s setting PIO%d on generic chip\n",
		   (error) ? "failed" : "success", apiomode < 0 ? 0 : apiomode);
    if (!error)
	atadev->mode = ATA_PIO0 + apiomode;
    else {
	if (bootverbose)
	    ata_prtdev(atadev, "using PIO mode set by BIOS\n");
	atadev->mode = ATA_PIO;
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

int
ata_dmasetup(struct ata_device *atadev, caddr_t data, int32_t count)
{
    struct ata_channel *ch = atadev->channel;

    if (((uintptr_t)data & ch->alignment) || (count & ch->alignment)) {
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
    ds->flags = ATA_DS_ACTIVE;
    if (dir)
	    ds->flags |= ATA_DS_READ;

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
ata_dmadone(struct ata_device *atadev)
{
    struct ata_channel *ch;
    struct ata_dmastate *ds;
    int error;

    ch = atadev->channel;
    ds = &atadev->dmastate;
    bus_dmamap_sync(ds->ddmatag, ds->ddmamap, (ds->flags & ATA_DS_READ) != 0 ?
		    BUS_DMASYNC_POSTREAD : BUS_DMASYNC_POSTWRITE);
    bus_dmamap_unload(ds->ddmatag, ds->ddmamap);

    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, 
		ATA_INB(ch->r_bmio, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    error = ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT);
    ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, 
	     error | ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR);
    ch->flags &= ~ATA_DMA_ACTIVE;
    ds->flags = 0;
    return (error & ATA_BMSTAT_MASK);
}

int
ata_dmastatus(struct ata_channel *ch)
{
    return ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

static void
cyrix_timing(struct ata_device *atadev, int devno, int mode)
{
    u_int32_t reg20 = 0x0000e132;
    u_int32_t reg24 = 0x00017771;

    switch (mode) {
    case ATA_PIO0:	reg20 = 0x0000e132; break;
    case ATA_PIO1:	reg20 = 0x00018121; break;
    case ATA_PIO2:	reg20 = 0x00024020; break;
    case ATA_PIO3:	reg20 = 0x00032010; break;
    case ATA_PIO4:	reg20 = 0x00040010; break;
    case ATA_WDMA2:	reg24 = 0x00002020; break;
    case ATA_UDMA2:	reg24 = 0x00911030; break;
    }
    ATA_OUTL(atadev->channel->r_bmio, (devno << 3) + 0x20, reg20);
    ATA_OUTL(atadev->channel->r_bmio, (devno << 3) + 0x24, reg24);
}

static void
promise_timing(struct ata_device *atadev, int devno, int mode)
{
    u_int32_t timing = 0;
    /* XXX: Endianess */
    struct promise_timing {
	u_int8_t  pa:4;
	u_int8_t  prefetch:1;
	u_int8_t  iordy:1;
	u_int8_t  errdy:1;
	u_int8_t  syncin:1;
	u_int8_t  pb:5;
	u_int8_t  mb:3;
	u_int8_t  mc:4;
	u_int8_t  dmaw:1;
	u_int8_t  dmar:1;
	u_int8_t  iordyp:1;
	u_int8_t  dmarqp:1;
	u_int8_t  reserved:8;
    } *t = (struct promise_timing*)&timing;

    t->iordy = 1; t->iordyp = 1;
    if (mode >= ATA_DMA) {
	t->prefetch = 1; t->errdy = 1; t->syncin = 1;
    }

    switch (atadev->channel->chiptype) {
    case 0x4d33105a:  /* Promise Ultra/Fasttrak 33 */
	switch (mode) {
	default:
	case ATA_PIO0:	t->pa =	 9; t->pb = 19; t->mb = 7; t->mc = 15; break;
	case ATA_PIO1:	t->pa =	 5; t->pb = 12; t->mb = 7; t->mc = 15; break;
	case ATA_PIO2:	t->pa =	 3; t->pb =  8; t->mb = 7; t->mc = 15; break;
	case ATA_PIO3:	t->pa =	 2; t->pb =  6; t->mb = 7; t->mc = 15; break;
	case ATA_PIO4:	t->pa =	 1; t->pb =  4; t->mb = 7; t->mc = 15; break;
	case ATA_WDMA2: t->pa =	 3; t->pb =  7; t->mb = 3; t->mc =  3; break;
	case ATA_UDMA2: t->pa =	 3; t->pb =  7; t->mb = 1; t->mc =  1; break;
	}
	break;

    case 0x0d38105a:  /* Promise Fasttrak 66 */
    case 0x4d38105a:  /* Promise Ultra/Fasttrak 66 */
    case 0x0d30105a:  /* Promise OEM ATA 100 */
    case 0x4d30105a:  /* Promise Ultra/Fasttrak 100 */
	switch (mode) {
	default:
	case ATA_PIO0:	t->pa = 15; t->pb = 31; t->mb = 7; t->mc = 15; break;
	case ATA_PIO1:	t->pa = 10; t->pb = 24; t->mb = 7; t->mc = 15; break;
	case ATA_PIO2:	t->pa =	 6; t->pb = 16; t->mb = 7; t->mc = 15; break;
	case ATA_PIO3:	t->pa =	 4; t->pb = 12; t->mb = 7; t->mc = 15; break;
	case ATA_PIO4:	t->pa =	 2; t->pb =  8; t->mb = 7; t->mc = 15; break;
	case ATA_WDMA2: t->pa =	 6; t->pb = 14; t->mb = 6; t->mc =  6; break;
	case ATA_UDMA2: t->pa =	 6; t->pb = 14; t->mb = 2; t->mc =  2; break;
	case ATA_UDMA4: t->pa =	 3; t->pb =  7; t->mb = 1; t->mc =  1; break;
	case ATA_UDMA5: t->pa =	 3; t->pb =  7; t->mb = 1; t->mc =  1; break;
	}
	break;
    }
    pci_write_config(device_get_parent(atadev->channel->dev),
		     0x60 + (devno << 2), timing, 4);
}

static void
hpt_timing(struct ata_device *atadev, int devno, int mode)
{
    device_t parent = device_get_parent(atadev->channel->dev);
    u_int32_t chiptype = atadev->channel->chiptype;
    int chiprev = pci_get_revid(parent);
    u_int32_t timing;

    if (chiptype == 0x00081103 && chiprev >= 0x07) {
	switch (mode) {						/* HPT374 */
	case ATA_PIO0:	timing = 0x0ac1f48a; break;
	case ATA_PIO1:	timing = 0x0ac1f465; break;
	case ATA_PIO2:	timing = 0x0a81f454; break;
	case ATA_PIO3:	timing = 0x0a81f443; break;
	case ATA_PIO4:	timing = 0x0a81f442; break;
	case ATA_WDMA2: timing = 0x22808242; break;
	case ATA_UDMA2: timing = 0x120c8242; break;
	case ATA_UDMA4: timing = 0x12ac8242; break;
	case ATA_UDMA5: timing = 0x12848242; break;
	case ATA_UDMA6: timing = 0x12808242; break;
	default:	timing = 0x0d029d5e;
	}
    }
    else if ((chiptype == 0x00041103 && chiprev >= 0x05) ||
	     (chiptype == 0x00051103 && chiprev >= 0x01)) {
	switch (mode) {						/* HPT372 */
	case ATA_PIO0:	timing = 0x0d029d5e; break;
	case ATA_PIO1:	timing = 0x0d029d26; break;
	case ATA_PIO2:	timing = 0x0c829ca6; break;
	case ATA_PIO3:	timing = 0x0c829c84; break;
	case ATA_PIO4:	timing = 0x0c829c62; break;
	case ATA_WDMA2: timing = 0x2c829262; break;
	case ATA_UDMA2: timing = 0x1c91dc62; break;
	case ATA_UDMA4: timing = 0x1c8ddc62; break;
	case ATA_UDMA5: timing = 0x1c6ddc62; break;
	case ATA_UDMA6: timing = 0x1c81dc62; break;
	default:	timing = 0x0d029d5e;
	}
    }
    else if (chiptype == 0x00041103 && chiprev >= 0x03) {
	switch (mode) {						/* HPT370 */
	case ATA_PIO0:	timing = 0x06914e57; break;
	case ATA_PIO1:	timing = 0x06914e43; break;
	case ATA_PIO2:	timing = 0x06514e33; break;
	case ATA_PIO3:	timing = 0x06514e22; break;
	case ATA_PIO4:	timing = 0x06514e21; break;
	case ATA_WDMA2: timing = 0x26514e21; break;
	case ATA_UDMA2: timing = 0x16494e31; break;
	case ATA_UDMA4: timing = 0x16454e31; break;
	case ATA_UDMA5: timing = 0x16454e31; break;
	default:	timing = 0x06514e57;
	}
	pci_write_config(parent, 0x40 + (devno << 2) , timing, 4);
    }
    else {							/* HPT36[68] */
	switch (pci_read_config(parent, 0x41 + (devno << 2), 1)) {
	case 0x85:	/* 25Mhz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0x40d08585; break;
	    case ATA_PIO1:	timing = 0x40d08572; break;
	    case ATA_PIO2:	timing = 0x40ca8542; break;
	    case ATA_PIO3:	timing = 0x40ca8532; break;
	    case ATA_PIO4:	timing = 0x40ca8521; break;
	    case ATA_WDMA2:	timing = 0x20ca8521; break;
	    case ATA_UDMA2:	timing = 0x10cf8521; break;
	    case ATA_UDMA4:	timing = 0x10c98521; break;
	    default:		timing = 0x01208585;
	    }
	    break;
	default:
	case 0xa7:	/* 33MHz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0x40d0a7aa; break;
	    case ATA_PIO1:	timing = 0x40d0a7a3; break;
	    case ATA_PIO2:	timing = 0x40d0a753; break;
	    case ATA_PIO3:	timing = 0x40c8a742; break;
	    case ATA_PIO4:	timing = 0x40c8a731; break;
	    case ATA_WDMA2:	timing = 0x20c8a731; break;
	    case ATA_UDMA2:	timing = 0x10caa731; break;
	    case ATA_UDMA4:	timing = 0x10c9a731; break;
	    default:		timing = 0x0120a7a7;
	    }
	    break;
	case 0xd9:	/* 40Mhz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0x4018d9d9; break;
	    case ATA_PIO1:	timing = 0x4010d9c7; break;
	    case ATA_PIO2:	timing = 0x4010d997; break;
	    case ATA_PIO3:	timing = 0x4010d974; break;
	    case ATA_PIO4:	timing = 0x4008d963; break;
	    case ATA_WDMA2:	timing = 0x2008d943; break;
	    case ATA_UDMA2:	timing = 0x100bd943; break;
	    case ATA_UDMA4:	timing = 0x100fd943; break;
	    default:		timing = 0x0120d9d9;
	    }
	}
    }
    pci_write_config(parent, 0x40 + (devno << 2) , timing, 4);
}

static int
hpt_cable80(struct ata_device *atadev)
{
    device_t parent = device_get_parent(atadev->channel->dev);
    u_int8_t reg, val, res;

    if (atadev->channel->chiptype==0x00081103 && pci_get_function(parent)==1) {
	reg = atadev->channel->unit ? 0x57 : 0x53;
	val = pci_read_config(parent, reg, 1);
	pci_write_config(parent, reg, val | 0x80, 1);
    }
    else {
	reg = 0x5b;
	val = pci_read_config(parent, reg, 1);
	pci_write_config(parent, reg, val & 0xfe, 1);
    }
    res = pci_read_config(parent, 0x5a, 1) & (atadev->channel->unit ? 0x1:0x2);
    pci_write_config(parent, reg, val, 1);
    return !res;
}
