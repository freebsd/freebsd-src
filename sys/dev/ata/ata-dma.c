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

#include "pci.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
#include <sys/bio.h>
#include <sys/malloc.h> 
#include <sys/bus.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <vm/vm.h>	     
#include <vm/pmap.h>
#include <pci/pcivar.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/ata/ata-all.h>

/* prototypes */
static void cyrix_timing(struct ata_channel *, int, int);
static void promise_timing(struct ata_channel *, int, int);
static void hpt_timing(struct ata_channel *, int, int);

/* misc defines */
#ifdef __alpha__
#undef vtophys
#define vtophys(va)	alpha_XXX_dmamap((vm_offset_t)va)
#endif
#define ATAPI_DEVICE(ch, device) \
	((device == ATA_MASTER && ch->devices & ATA_ATAPI_MASTER) || \
	 (device == ATA_SLAVE && ch->devices & ATA_ATAPI_SLAVE))

void *
ata_dmaalloc(struct ata_channel *ch, int device)
{
    void *dmatab;

    if ((dmatab = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT))) {
	if (((uintptr_t)dmatab >> PAGE_SHIFT) ^
	    (((uintptr_t)dmatab + PAGE_SIZE - 1) >> PAGE_SHIFT)) {
	    ata_printf(ch, device, "dmatab crosses page boundary, no DMA\n");
	    free(dmatab, M_DEVBUF);
	    dmatab = NULL;
	}
    }
    return dmatab;
}

void
ata_dmainit(struct ata_channel *ch, int device,
	    int apiomode, int wdmamode, int udmamode)
{
    struct ata_device *atadev = &ch->device[ATA_DEV(device)];
    device_t parent = device_get_parent(ch->dev);
    int devno = (ch->unit << 1) + ATA_DEV(device);
    int error;

    /* set our most pessimistic default mode */
    atadev->mode = ATA_PIO;

    if (!ch->r_bmio)
	return;

    /* if simplex controller, only allow DMA on primary channel */
    if (ch->unit == 1) {
	ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT,
		 ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) &
		 (ATA_BMSTAT_DMA_MASTER | ATA_BMSTAT_DMA_SLAVE));
	if (ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) & ATA_BMSTAT_DMA_SIMPLEX) {
	    ata_prtdev(atadev, "simplex device, DMA on primary only\n");
	    return;
	}
    }

    /* DMA engine address alignment is usually 1 word (2 bytes) */
    ch->alignment = 0x1;

#if 1
    if (udmamode > 2 && !ch->device[ATA_DEV(device)].param->hwres_cblid) {
	ata_prtdev(atadev,"DMA limited to UDMA33, non-ATA66 cable or device\n");
	udmamode = 2;
    }
#endif
    switch (ch->chiptype) {

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
		    atadev->mode = ATA_UDMA5;
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
		    atadev->mode = ATA_UDMA4;
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
		atadev->mode = ATA_UDMA2;
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
	    if (!((pci_read_config(parent,0x40,4)>>(ch->unit<<8))&0x4000)) {
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
		if (ch->unit) {
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
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x12308086:	/* Intel PIIX */
	if (wdmamode >= 2 && apiomode >= 4) {
	    int32_t word40;

	    word40 = pci_read_config(parent, 0x40, 4);
	    word40 >>= ch->unit * 16;

	    /* Check for timing config usable for DMA on controller */
	    if (!((word40 & 0x3300) == 0x2300 &&
		  ((word40 >> (device == ATA_MASTER ? 0 : 4)) & 1) == 1))
		break;

	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Intel chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	break;

    case 0x522910b9:	/* AcerLabs Aladdin IV/V */
	/* the older Aladdin doesn't support ATAPI DMA on both master & slave */
	if (pci_get_revid(parent) < 0xc2 &&
	    ch->devices & ATA_ATAPI_MASTER && ch->devices & ATA_ATAPI_SLAVE) {
	    ata_prtdev(atadev, "two atapi devices on this channel, no DMA\n");
	    break;
	}
	if (udmamode >= 5 && pci_get_revid(parent) >= 0xc4) {
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
		atadev->mode = ATA_UDMA5;
		return;
	    }
	}
	if (udmamode >= 4 && pci_get_revid(parent) >= 0xc2) {
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
		atadev->mode = ATA_UDMA4;
		return;
	    }
	}
	if (udmamode >= 2 && pci_get_revid(parent) >= 0x20) {
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
		ch->flags |= ATA_ATAPI_DMA_RO;
		atadev->mode = ATA_UDMA2;
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
		ch->flags |= ATA_ATAPI_DMA_RO;
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	pci_write_config(parent, 0x53,
			 (pci_read_config(parent, 0x53, 1) & ~0x01) | 0x02, 1);
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x74111022:	/* AMD 766 */
	if (udmamode >= 5) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on AMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x53 - devno, 0xc6, 1);
		atadev->mode = ATA_UDMA5;
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x74091022:	/* AMD 756 */
	if (udmamode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on AMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x53 - devno, 0xc5, 1);
		atadev->mode = ATA_UDMA4;
		return;
	    }
	}
	goto via_82c586;

    case 0x05711106:	/* VIA 82C571, 82C586, 82C596, 82C686 , 8231, 8233 */
	{
	    int via_modes[4][7] = {
		{ 0x00, 0x00, 0xc0, 0x00, 0x00, 0x00, 0x00 },	/* ATA33 */
		{ 0x00, 0x00, 0xea, 0x00, 0xe8, 0x00, 0x00 },	/* ATA66 */
		{ 0x00, 0x00, 0xf4, 0x00, 0xf1, 0xf0, 0x00 },	/* ATA100 */
		{ 0x00, 0x00, 0xf6, 0x00, 0xf2, 0xf1, 0xf0 }};	/* ATA133 */
	    int *reg_val = NULL;

	    if (ata_find_dev(parent, 0x31471106, 0x40)) {	/* 8233a */
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
	    else if (ata_find_dev(parent, 0x06861106, 0x0)) {	/* 82C686 */
		udmamode = imin(udmamode, 2);
		reg_val = via_modes[1];
	    }
	    else if (ata_find_dev(parent, 0x05961106, 0) ||	/* 82C596a */
		     ata_find_dev(parent, 0x05861106, 0x03)) {	/* 82C586b */
via_82c586:
		udmamode = imin(udmamode, 2);
		reg_val = via_modes[0];
	    }
	    else
		udmamode = 0;
	
	    if (udmamode >= 6) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA6 on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[6], 1);
		    atadev->mode = ATA_UDMA6;
		    return;
		}
	    }
	    if (udmamode >= 5) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA5 on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[5], 1);
		    atadev->mode = ATA_UDMA5;
		    return;
		}
	    }
	    if (udmamode >= 4) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA4 on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[4], 1);
		    atadev->mode = ATA_UDMA4;
		    return;
		}
	    }
	    if (udmamode >= 2) {
		error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_prtdev(atadev, "%s setting UDMA2 on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
		    pci_write_config(parent, 0x53 - devno, reg_val[2], 1);
		    atadev->mode = ATA_UDMA2;
		    return;
		}
	    }

	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on %s chip\n",
			   (error) ? "failed" : "success",
			   (ch->chiptype == 0x74091022) ? "AMD" : "VIA");
	    if (!error) {
		pci_write_config(parent, 0x53 - devno, 0x0b, 1);
		pci_write_config(parent, 0x4b - devno, 0x31, 1);
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x55131039:	/* SiS 5591 */
	if (ata_find_dev(parent, 0x06301039, 0x30) ||	/* SiS 630 */
	    ata_find_dev(parent, 0x06331039, 0x00) ||	/* SiS 633 */
	    ata_find_dev(parent, 0x06351039, 0x00) ||	/* SiS 635 */
	    ata_find_dev(parent, 0x06401039, 0x00) ||	/* SiS 640 */
	    ata_find_dev(parent, 0x06451039, 0x00) ||	/* SiS 645 */
	    ata_find_dev(parent, 0x06501039, 0x00) ||	/* SiS 650 */
	    ata_find_dev(parent, 0x07301039, 0x00) ||	/* SiS 730 */
	    ata_find_dev(parent, 0x07331039, 0x00) ||	/* SiS 733 */
	    ata_find_dev(parent, 0x07351039, 0x00) ||	/* SiS 735 */
	    ata_find_dev(parent, 0x07401039, 0x00) ||	/* SiS 740 */
	    ata_find_dev(parent, 0x07451039, 0x00) ||	/* SiS 745 */
	    ata_find_dev(parent, 0x07501039, 0x00)) {	/* SiS 750 */
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
		    atadev->mode = ATA_UDMA5;
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
		    atadev->mode = ATA_UDMA4;
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
		    atadev->mode = ATA_UDMA2;
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
		    atadev->mode = ATA_UDMA4;
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
		    atadev->mode = ATA_UDMA2;
		    return;
		}
	    }
	} else if (udmamode >= 2 && pci_get_revid(parent) > 0xc1) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on SiS chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(parent, 0x40 + (devno << 1), 0xa301, 2);
		atadev->mode = ATA_UDMA2;
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
		atadev->mode = ATA_WDMA2;
		return;
	    }
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
		umode = pci_read_config(parent, ch->unit ? 0x7b : 0x73, 1);
		umode &= ~(device == ATA_MASTER ? 0x35 : 0xca);
		umode |= (device == ATA_MASTER ? 0x05 : 0x0a);
		pci_write_config(parent, ch->unit ? 0x7b : 0x73, umode, 1);
		atadev->mode = ATA_UDMA5;
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
		umode = pci_read_config(parent, ch->unit ? 0x7b : 0x73, 1);
		umode &= ~(device == ATA_MASTER ? 0x35 : 0xca);
		umode |= (device == ATA_MASTER ? 0x15 : 0x4a);
		pci_write_config(parent, ch->unit ? 0x7b : 0x73, umode, 1);
		atadev->mode = ATA_UDMA4;
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
		umode = pci_read_config(parent, ch->unit ? 0x7b : 0x73, 1);
		umode &= ~(device == ATA_MASTER ? 0x35 : 0xca);
		umode |= (device == ATA_MASTER ? 0x11 : 0x42);
		pci_write_config(parent, ch->unit ? 0x7b : 0x73, umode, 1);
		atadev->mode = ATA_UDMA2;
		return;
	    }
	}
	/* make sure eventual UDMA mode from the BIOS is disabled */
	pci_write_config(parent, ch->unit ? 0x7b : 0x73, 
			 pci_read_config(parent, ch->unit ? 0x7b : 0x73, 1)&
			 ~(device == ATA_MASTER ? 0x35 : 0xca), 1);
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
		atadev->mode = ATA_WDMA2;
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
		pci_write_config(ch->dev, ch->unit ? 0x4e:0x4c, 0x2020, 2);
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x01021078:	/* Cyrix 5530 ATA33 controller */
	ch->alignment = 0xf;	/* DMA engine requires 16 byte alignment */
	if (udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Cyrix chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		cyrix_timing(ch, devno, ATA_UDMA2);
		atadev->mode = ATA_UDMA2;
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
		cyrix_timing(ch, devno, ATA_WDMA2);
		atadev->mode = ATA_WDMA2;
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
	cyrix_timing(ch, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;

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
		atadev->mode = ATA_UDMA2;
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
		int offset = (ch->unit * 2) + (device == ATA_MASTER);
		int word44 = pci_read_config(parent, 0x44, 4);

		pci_write_config(parent, 0x54,
				 pci_read_config(parent, 0x54, 1) &
				 ~(0x01 << devno), 1);
		word44 &= ~(0xff << (offset << 8));
		word44 |= (0x20 << (offset << 8));
		pci_write_config(parent, 0x44, 0x20, 4);
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x4d69105a:	/* Promise TX2 ATA133 controllers */
	ATA_OUTB(ch->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 6 && !(ATA_INB(ch->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA6 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		atadev->mode = ATA_UDMA6;
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x4d68105a:	/* Promise TX2 ATA100 controllers */
    case 0x6268105a:	/* Promise TX2 ATA100 controllers */
	ATA_OUTB(ch->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 5 && !(ATA_INB(ch->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		atadev->mode = ATA_UDMA5;
		return;
	    }
	}
	ATA_OUTB(ch->r_bmio, ATA_BMDEVSPEC_0, 0x0b);
	if (udmamode >= 4 && !(ATA_INB(ch->r_bmio, ATA_BMDEVSPEC_1) & 0x04)) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		atadev->mode = ATA_UDMA4;
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
		atadev->mode = ATA_UDMA2;
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
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
	break;

    case 0x4d30105a:	/* Promise Ultra/FastTrak 100 controllers */
    case 0x0d30105a:	/* Promise OEM ATA100 controllers */
	if (!ATAPI_DEVICE(ch, device) && udmamode >= 5 && 
	    !(pci_read_config(parent, 0x50, 2)&(ch->unit ? 1<<11 : 1<<10))){
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(ch, devno, ATA_UDMA5);
		atadev->mode = ATA_UDMA5;
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x4d38105a:	/* Promise Ultra/FastTrak 66 controllers */
	if (!ATAPI_DEVICE(ch, device) && udmamode >= 4 && 
	    !(pci_read_config(parent, 0x50, 2)&(ch->unit ? 1<<11 : 1<<10))){
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(ch, devno, ATA_UDMA4);
		atadev->mode = ATA_UDMA4;
		return;
	    }
	}
	/* FALLTHROUGH */

    case 0x4d33105a:	/* Promise Ultra/FastTrak 33 controllers */
	if (!ATAPI_DEVICE(ch, device) && udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(ch, devno, ATA_UDMA2);
		atadev->mode = ATA_UDMA2;
		return;
	    }
	}
	if (!ATAPI_DEVICE(ch, device) && wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(ch, devno, ATA_WDMA2);
		atadev->mode = ATA_WDMA2;
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
	promise_timing(ch, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;
    
    case 0x00041103:	/* HighPoint HPT366/368/370/372 controllers */
	if (!ATAPI_DEVICE(ch, device) &&
	    udmamode >= 6 && pci_get_revid(parent) >= 0x05 &&
	    !(pci_read_config(parent, 0x5a, 1) & (ch->unit ? 0x01:0x02))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA6, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA6 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(ch, devno, ATA_UDMA6);
		atadev->mode = ATA_UDMA6;
		return;
	    }
	}
	if (!ATAPI_DEVICE(ch, device) &&
	    udmamode >= 5 && pci_get_revid(parent) >= 0x03 &&
	    !(pci_read_config(parent, 0x5a, 1) & (ch->unit ? 0x01:0x02))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA5, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA5 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(ch, devno, ATA_UDMA5);
		atadev->mode = ATA_UDMA5;
		return;
	    }
	}
	if (!ATAPI_DEVICE(ch, device) && udmamode >= 4 && 
	    !(pci_read_config(parent, 0x5a, 1) & (ch->unit ? 0x01:0x02))) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA4 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(ch, devno, ATA_UDMA4);
		atadev->mode = ATA_UDMA4;
		return;
	    }
	}
	if (!ATAPI_DEVICE(ch, device) && udmamode >= 2) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting UDMA2 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(ch, devno, ATA_UDMA2);
		atadev->mode = ATA_UDMA2;
		return;
	    }
	}
	if (!ATAPI_DEVICE(ch, device) && wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on HighPoint chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt_timing(ch, devno, ATA_WDMA2);
		atadev->mode = ATA_WDMA2;
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
	hpt_timing(ch, devno, ATA_PIO0 + apiomode);
	atadev->mode = ATA_PIO0 + apiomode;
	return;

    default:		/* unknown controller chip */
	/* better not try generic DMA on ATAPI devices it almost never works */
	if ((device == ATA_MASTER && ch->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && ch->devices & ATA_ATAPI_SLAVE))
	    break;

	/* if controller says its setup for DMA take the easy way out */
	/* the downside is we dont know what DMA mode we are in */
	if ((udmamode >= 0 || wdmamode > 1) &&
	    (ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) &
	     ((device==ATA_MASTER) ? 
	      ATA_BMSTAT_DMA_MASTER : ATA_BMSTAT_DMA_SLAVE))) {
	    atadev->mode = ATA_DMA;
	    return;
	}

	/* well, we have no support for this, but try anyways */
	if ((wdmamode >= 2 && apiomode >= 4) && ch->r_bmio) {
	    error = ata_command(atadev, ATA_C_SETFEATURES, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_prtdev(atadev, "%s setting WDMA2 on generic chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		atadev->mode = ATA_WDMA2;
		return;
	    }
	}
    }
    error = ata_command(atadev, ATA_C_SETFEATURES, 0, ATA_PIO0 + apiomode,
			ATA_C_F_SETXFER,ATA_WAIT_READY);
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

int
ata_dmasetup(struct ata_channel *ch, int device, struct ata_dmaentry *dmatab,
	     caddr_t data, int32_t count)
{
    u_int32_t dma_count, dma_base;
    int i = 0;

    if (((uintptr_t)data & ch->alignment) || (count & ch->alignment)) {
	ata_printf(ch, device, "non aligned DMA transfer attempted\n");
	return -1;
    }

    if (!count) {
	ata_printf(ch, device, "zero length DMA transfer attempted\n");
	return -1;
    }
    
    dma_base = vtophys(data);
    dma_count = imin(count, (PAGE_SIZE - ((uintptr_t)data & PAGE_MASK)));
    data += dma_count;
    count -= dma_count;

    while (count) {
	dmatab[i].base = dma_base;
	dmatab[i].count = (dma_count & 0xffff);
	i++; 
	if (i >= ATA_DMA_ENTRIES) {
	    ata_printf(ch, device, "too many segments in DMA table\n");
	    return -1;
	}
	dma_base = vtophys(data);
	dma_count = imin(count, PAGE_SIZE);
	data += imin(count, PAGE_SIZE);
	count -= imin(count, PAGE_SIZE);
    }
    dmatab[i].base = dma_base;
    dmatab[i].count = (dma_count & 0xffff) | ATA_DMA_EOT;
    return 0;
}

void
ata_dmastart(struct ata_channel *ch, int device, 
	     struct ata_dmaentry *dmatab, int dir)
{
    ch->flags |= ATA_DMA_ACTIVE;
    ATA_OUTL(ch->r_bmio, ATA_BMDTP_PORT, vtophys(dmatab));
    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, dir ? ATA_BMCMD_WRITE_READ : 0);
    ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, 
	 (ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) | 
	  (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, 
	 ATA_INB(ch->r_bmio, ATA_BMCMD_PORT) | ATA_BMCMD_START_STOP);
}

int
ata_dmadone(struct ata_channel *ch)
{
    int error;

    ATA_OUTB(ch->r_bmio, ATA_BMCMD_PORT, 
		ATA_INB(ch->r_bmio, ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    ch->flags &= ~ATA_DMA_ACTIVE;
    error = ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT);
    ATA_OUTB(ch->r_bmio, ATA_BMSTAT_PORT, 
	     error | ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR);
    return error & ATA_BMSTAT_MASK;
}

int
ata_dmastatus(struct ata_channel *ch)
{
    return ATA_INB(ch->r_bmio, ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

static void
cyrix_timing(struct ata_channel *ch, int devno, int mode)
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
    ATA_OUTL(ch->r_bmio, (devno << 3) + 0x20, reg20);
    ATA_OUTL(ch->r_bmio, (devno << 3) + 0x24, reg24);
}

static void
promise_timing(struct ata_channel *ch, int devno, int mode)
{
    u_int32_t timing = 0;
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

    switch (ch->chiptype) {
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

    case 0x4d38105a:  /* Promise Ultra/Fasttrak 66 */
    case 0x4d30105a:  /* Promise Ultra/Fasttrak 100 */
    case 0x0d30105a:  /* Promise OEM ATA 100 */
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
    pci_write_config(device_get_parent(ch->dev), 0x60 + (devno<<2), timing, 4);
}

static void
hpt_timing(struct ata_channel *ch, int devno, int mode)
{
    device_t parent = device_get_parent(ch->dev);
    u_int32_t timing;
    if (pci_get_revid(parent) >= 0x05) {	/* HPT372 */
	switch (mode) {
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
	pci_write_config(parent, 0x40 + (devno << 2) , timing, 4);
	pci_write_config(parent, 0x5b, 0x20, 1);
    }
    else if (pci_get_revid(parent) >= 0x03) {	/* HPT370 */
	switch (mode) {
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
	pci_write_config(parent, 0x5b, 0x22, 1);
    }
    else {					/* HPT36[68] */
	switch (pci_read_config(parent, 0x41 + (devno << 2), 1)) {
	case 0x85:	/* 25Mhz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0xc0d08585; break;
	    case ATA_PIO1:	timing = 0xc0d08572; break;
	    case ATA_PIO2:	timing = 0xc0ca8542; break;
	    case ATA_PIO3:	timing = 0xc0ca8532; break;
	    case ATA_PIO4:	timing = 0xc0ca8521; break;
	    case ATA_WDMA2:	timing = 0xa0ca8521; break;
	    case ATA_UDMA2:	timing = 0x90cf8521; break;
	    case ATA_UDMA4:	timing = 0x90c98521; break;
	    default:		timing = 0x01208585;
	    }
	    break;
	default:
	case 0xa7:	/* 33MHz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0xc0d0a7aa; break;
	    case ATA_PIO1:	timing = 0xc0d0a7a3; break;
	    case ATA_PIO2:	timing = 0xc0d0a753; break;
	    case ATA_PIO3:	timing = 0xc0c8a742; break;
	    case ATA_PIO4:	timing = 0xc0c8a731; break;
	    case ATA_WDMA2:	timing = 0xa0c8a731; break;
	    case ATA_UDMA2:	timing = 0x90caa731; break;
	    case ATA_UDMA4:	timing = 0x90c9a731; break;
	    default:		timing = 0x0120a7a7;
	    }
	    break;
	case 0xd9:	/* 40Mhz */
	    switch (mode) {
	    case ATA_PIO0:	timing = 0xc018d9d9; break;
	    case ATA_PIO1:	timing = 0xc010d9c7; break;
	    case ATA_PIO2:	timing = 0xc010d997; break;
	    case ATA_PIO3:	timing = 0xc010d974; break;
	    case ATA_PIO4:	timing = 0xc008d963; break;
	    case ATA_WDMA2:	timing = 0xa008d943; break;
	    case ATA_UDMA2:	timing = 0x900bd943; break;
	    case ATA_UDMA4:	timing = 0x900fd943; break;
	    default:		timing = 0x0120d9d9;
	    }
	}
	pci_write_config(parent, 0x40 + (devno << 2), (timing & ~0x80000000),4);
    }
}
