/*-
 * Copyright (c) 1998,1999 Søren Schmidt
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
#include "apm.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/malloc.h> 
#include <sys/bus.h>
#include <vm/vm.h>	     
#include <vm/pmap.h>
#if NPCI > 0
#include <pci/pcivar.h>
#include <pci/pcireg.h>
#endif
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>

/* prototypes */
static void hpt366_timing(struct ata_softc *, int32_t, int32_t);

/* misc defines */
#define MIN(a,b) ((a)>(b)?(b):(a))    
#ifdef __alpha__
#undef vtophys
#define vtophys(va)	alpha_XXX_dmamap((vm_offset_t)va)
#endif

#if NPCI > 0

int32_t
ata_dmainit(struct ata_softc *scp, int32_t device, 
	    int32_t apiomode, int32_t wdmamode, int32_t udmamode)
{
    int32_t type, devno, error;
    void *dmatab;

    if (!scp->bmaddr)
	return -1;
#ifdef ATA_DMADEBUG
    printf("ata%d: dmainit: ioaddr=0x%x altioaddr=0x%x, bmaddr=0x%x\n",
	   scp->lun, scp->ioaddr, scp->altioaddr, scp->bmaddr);
#endif

    /* if simplex controller, only allow DMA on primary channel */
    if (scp->unit == 1) {
	outb(scp->bmaddr + ATA_BMSTAT_PORT, inb(scp->bmaddr + ATA_BMSTAT_PORT) &
	     (ATA_BMSTAT_DMA_MASTER | ATA_BMSTAT_DMA_SLAVE));
	if (inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_DMA_SIMPLEX) {
	    printf("ata%d: simplex device, DMA on primary channel only\n",
		   scp->lun);
	    return -1;
	}
    }

    if (!(dmatab = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT)))
	return -1;

    if (((uintptr_t)dmatab >> PAGE_SHIFT) ^
	(((uintptr_t)dmatab + PAGE_SIZE - 1) >> PAGE_SHIFT)) {
	printf("ata_dmainit: dmatab crosses page boundary, no DMA\n");
	free(dmatab, M_DEVBUF);
	return -1;
    }
    scp->dmatab[(device == ATA_MASTER) ? 0 : 1] = dmatab;

    switch (type = pci_get_devid(scp->dev)) {

    case 0x71118086:	/* Intel PIIX4 */
	if (udmamode >= 2) {
	    int32_t mask48, new48;

	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA2 mode on PIIX4 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    devno = (scp->unit << 1) + ((device == ATA_MASTER) ? 0 : 1);
	    mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
	    new48 = (1 << devno) + (2 << (16 + (devno << 2)));
	    pci_write_config(scp->dev, 0x48, 
			     (pci_read_config(scp->dev, 0x48, 4) &
			      ~mask48) | new48, 4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA2;
	    return 0;
	}
	/* FALLTHROUGH */

    case 0x70108086:	/* Intel PIIX3 */
	if (wdmamode >= 2 && apiomode >= 4) {
	    int32_t mask40, new40, mask44, new44;

	    /* if SITRE not set doit for both channels */
	    if (!((pci_read_config(scp->dev, 0x40, 4)>>(scp->unit<<8))&0x4000)){
		new40 = pci_read_config(scp->dev, 0x40, 4);
		new44 = pci_read_config(scp->dev, 0x44, 4); 
		if (!(new40 & 0x00004000)) {
		    new44 &= ~0x0000000f;
		    new44 |= ((new40&0x00003000)>>10)|((new40&0x00000300)>>8);
		}
		if (!(new40 & 0x40000000)) {
		    new44 &= ~0x000000f0;
		    new44 |= ((new40&0x30000000)>>22)|((new40&0x03000000)>>20);
		}
		new40 |= 0x40004000;
		pci_write_config(scp->dev, 0x40, new40, 4);
		pci_write_config(scp->dev, 0x44, new44, 4);
	    }
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up WDMA2 mode on PIIX4 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    if (device == ATA_MASTER) {
		mask40 = 0x0000330f;
		new40 = 0x00002307;
		mask44 = 0;
		new44 = 0;
	    } else {
		mask40 = 0x000000f0;
		new40 = 0x00000070;
		mask44 = 0x0000000f;
		new44 = 0x0000000b;
	    }
	    if (scp->unit) {
		mask40 <<= 16;
		new40 <<= 16;
		mask44 <<= 4;
		new44 <<= 4;
	    }
	    pci_write_config(scp->dev, 0x40,
			     (pci_read_config(scp->dev, 0x40, 4) & ~mask40) |
			     new40, 4);
	    pci_write_config(scp->dev, 0x44,
			     (pci_read_config(scp->dev, 0x44, 4) & ~mask44) |
			     new44, 4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_WDMA2;
	    return 0;
	}
	break;

    case 0x12308086:	/* Intel PIIX */
	/* probably not worth the trouble */
	break;

    case 0x522910b9:	/* AcerLabs Aladdin IV/V */
	/* the Aladdin has to be setup specially for ATAPI devices */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE)) {
	    int8_t word53 = pci_read_config(scp->dev, 0x53, 1);

	    /* set atapi fifo, this should always work */
	    pci_write_config(scp->dev, 0x53, (word53 & ~0x01) | 0x02, 1);

	    /* if both master & slave are atapi devices dont allow DMA */
	    if (scp->devices & ATA_ATAPI_MASTER && 
		scp->devices & ATA_ATAPI_SLAVE) {
		printf("ata%d: Aladdin: two atapi devices on this channel, "
		       "DMA disabled\n", scp->lun);
		break;
	    }
	    /* if needed set atapi fifo & dma */
	    if ((udmamode >=2) || (wdmamode >= 2 && apiomode >= 4)) {
		pci_write_config(scp->dev, 0x53, word53 | 0x03, 1);
		scp->flags |= ATA_ATAPI_DMA_RO;
		if (device == ATA_MASTER)
		    outb(scp->bmaddr + ATA_BMSTAT_PORT, 
			 inb(scp->bmaddr + ATA_BMSTAT_PORT) |
			 ATA_BMSTAT_DMA_MASTER);
		else
		    outb(scp->bmaddr + ATA_BMSTAT_PORT, 
			 inb(scp->bmaddr + ATA_BMSTAT_PORT) |
			 ATA_BMSTAT_DMA_SLAVE);
	    }
	}
	if (udmamode >=2) {
	    int32_t word54 = pci_read_config(scp->dev, 0x54, 4);
	
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA2 mode on Aladdin chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    word54 |= 0x5555;
	    word54 |= (0x0a << (16 + (scp->unit << 3) + (device << 2)));
	    pci_write_config(scp->dev, 0x54, word54, 4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA2;
	    return 0;
		
	}
	else if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up WDMA2 mode on Aladdin chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_WDMA2;
	    return 0;
	}
	break;

    case 0x4d33105a:	/* Promise Ultra33 / FastTrak33 controllers */
    case 0x4d38105a:	/* Promise Ultra66 / FastTrak66 controllers */
	/* the Promise can only do DMA on ATA disks not on ATAPI devices */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	devno = (scp->unit << 1) + ((device == ATA_MASTER) ? 0 : 1);
	if (udmamode >=2) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA2 mode on Promise chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004127f3, 4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA2;
	    return 0;
	}
	else if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up WDMA2 mode on Promise chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004367f3, 4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_WDMA2;
	    return 0;
	}
	else {
	    if (bootverbose)
		printf("ata%d: %s: setting PIO mode on Promise chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave");
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004fe924, 4);
	}
	break;
    
    case 0x00041103:	/* HighPoint HPT366 IDE controller */
	/* punt on ATAPI devices for now */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	devno = (device == ATA_MASTER) ? 0 : 1;
	if (udmamode >=4 && !(pci_read_config(scp->dev, 0x5a, 1) & 0x2)) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA4, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA4 mode on HPT366 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    hpt366_timing(scp, device, ATA_MODE_UDMA4);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA4;
	    return 0;
	}
	else if (udmamode >=3) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA3, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA3 mode on HPT366 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    hpt366_timing(scp, device, ATA_MODE_UDMA3);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA3;
	    return 0;
	}
	else if (udmamode >=2) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up UDMA2 mode on HPT366 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    hpt366_timing(scp, device, ATA_MODE_UDMA2);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_UDMA2;
	    return 0;
	}
	else if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up WDMA2 mode on HPT366 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    hpt366_timing(scp, device, ATA_MODE_WDMA2);
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_WDMA2;
	    return 0;
	}
	else {
	    if (bootverbose)
		printf("ata%d: %s: setting PIO mode on HPT366 chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave");
	    hpt366_timing(scp, device, ATA_MODE_PIO);
	}
	break;

    default:		/* unknown controller chip */
	/* better not try generic DMA on ATAPI devices it almost never works */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	/* well, we have no support for this, but try anyways */
	if (((wdmamode >= 2 && apiomode >= 4) || udmamode >= 2) &&
	    (inb(scp->bmaddr + ATA_BMSTAT_PORT) & 
		((device == ATA_MASTER) ? 
		 ATA_BMSTAT_DMA_SLAVE : ATA_BMSTAT_DMA_MASTER))) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_IGNORE_INTR);
	    if (bootverbose)
		printf("ata%d: %s: %s setting up WDMA2 mode on generic chip\n",
		       scp->lun, (device == ATA_MASTER) ? "master" : "slave",
		       (error) ? "failed" : "success");
	    if (error)
		break;
	    scp->mode[(device == ATA_MASTER) ? 0 : 1] = ATA_MODE_WDMA2;
	    return 0;
	}
    }
    free(dmatab, M_DEVBUF);
    return -1;
}

int32_t
ata_dmasetup(struct ata_softc *scp, int32_t device, 
	     int8_t *data, int32_t count, int32_t flags)
{
    struct ata_dmaentry *dmatab;
    u_int32_t dma_count, dma_base;
    int32_t i = 0;

#ifdef ATA_DMADEBUG
    printf("ata%d: dmasetup\n", scp->lun);
#endif
    if (((uintptr_t)data & 1) || (count & 1))
	return -1;

    if (!count) {
#ifdef ATA_DMADEBUG
	printf("ata%d: zero length DMA transfer attempt on %s\n", 
	       scp->lun, ((device == ATA_MASTER) ? "master" : "slave"));
#endif
	return -1;
    }
    
    dmatab = scp->dmatab[(device == ATA_MASTER) ? 0 : 1];
    dma_base = vtophys(data);
    dma_count = MIN(count, (PAGE_SIZE - ((uintptr_t)data & PAGE_MASK)));
    data += dma_count;
    count -= dma_count;

    while (count) {
	dmatab[i].base = dma_base;
	dmatab[i].count = (dma_count & 0xffff);
	i++; 
	if (i >= ATA_DMA_ENTRIES) {
	    printf("ata%d: too many segments in DMA table for %s\n",
		   scp->lun, (device ? "slave" : "master"));
	    return -1;
	}
	dma_base = vtophys(data);
	dma_count = MIN(count, PAGE_SIZE);
	data += MIN(count, PAGE_SIZE);
	count -= MIN(count, PAGE_SIZE);
    }
#ifdef ATA_DMADEBUG
	printf("ata_dmasetup: base=%08x count%08x\n", dma_base, dma_count);
#endif
    dmatab[i].base = dma_base;
    dmatab[i].count = (dma_count & 0xffff) | ATA_DMA_EOT;
    
    outl(scp->bmaddr + ATA_BMDTP_PORT, vtophys(dmatab));
#ifdef ATA_DMADEBUG
    printf("dmatab=%08x %08x\n",
	   vtophys(dmatab), inl(scp->bmaddr+ATA_BMDTP_PORT));
#endif
    outb(scp->bmaddr + ATA_BMCMD_PORT, flags ? ATA_BMCMD_WRITE_READ:0);
    outb(scp->bmaddr + ATA_BMSTAT_PORT, (inb(scp->bmaddr + ATA_BMSTAT_PORT) | 
				   (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    return 0;
}

void
ata_dmastart(struct ata_softc *scp)
{
#ifdef ATA_DMADEBUG
    printf("ata%d: dmastart\n", scp->lun);
#endif
    scp->flags |= ATA_DMA_ACTIVE;
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) | ATA_BMCMD_START_STOP);
}

int32_t
ata_dmadone(struct ata_softc *scp)
{
#ifdef ATA_DMADEBUG
    printf("ata%d: dmadone\n", scp->lun);
#endif
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    scp->flags &= ~ATA_DMA_ACTIVE;
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

int32_t
ata_dmastatus(struct ata_softc *scp)
{
#ifdef ATA_DMADEBUG
    printf("ata%d: dmastatus\n", scp->lun);
#endif
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

static void
hpt366_timing(struct ata_softc *scp, int32_t device, int32_t mode)
{
    u_int32_t timing;

    switch (pci_read_config(scp->dev, (device == ATA_MASTER) ? 0x41 : 0x45, 1)){
    case 0x85:	/* 25Mhz */
	switch (mode) {
	case ATA_MODE_PIO:	timing = 0xc0ca8521; break;
	case ATA_MODE_WDMA2:	timing = 0xa0ca8521; break;
	case ATA_MODE_UDMA2:
	case ATA_MODE_UDMA3:	timing = 0x90cf8521; break;
	case ATA_MODE_UDMA4:	timing = 0x90c98521; break;
	default:		timing = 0x01208585;
	}
	break;
    default:
    case 0xa7:	/* 33MHz */
	switch (mode) {
	case ATA_MODE_PIO:	timing = 0xc0c8a731; break;
	case ATA_MODE_WDMA2:	timing = 0xa0c8a731; break;
	case ATA_MODE_UDMA2:	timing = 0x90caa731; break;
	case ATA_MODE_UDMA3:	timing = 0x90cfa731; break;
	case ATA_MODE_UDMA4:	timing = 0x90c9a731; break;
	default:		timing = 0x0120a7a7;
	}
	break;
    case 0xd9:	/* 40Mhz */
	switch (mode) {
	case ATA_MODE_PIO:	timing = 0xc008d963; break;
	case ATA_MODE_WDMA2:	timing = 0xa008d943; break;
	case ATA_MODE_UDMA2:	timing = 0x900bd943; break;
	case ATA_MODE_UDMA3:	timing = 0x900ad943; break;
	case ATA_MODE_UDMA4:	timing = 0x900fd943; break;
	default:		timing = 0x0120d9d9;
	}
    }
    pci_write_config(scp->dev, 0x40 + (device==ATA_MASTER ? 0 : 4), timing, 4);
}

#else /* NPCI > 0 */

int32_t
ata_dmainit(struct ata_softc *scp, int32_t device,
	    int32_t piomode, int32_t wdmamode, int32_t udmamode)
{
    return -1;
}

int32_t
ata_dmasetup(struct ata_softc *scp, int32_t device,
	     int8_t *data, int32_t count, int32_t flags)
{
    return -1;
}

void 
ata_dmastart(struct ata_softc *scp)
{
}

int32_t
ata_dmadone(struct ata_softc *scp)
{
    return -1;
}

int32_t
ata_dmastatus(struct ata_softc *scp)
{
    return -1;
}

#endif /* NPCI > 0 */
