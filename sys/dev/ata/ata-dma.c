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
 *	$Id: ata-dma.c,v 1.9 1999/08/06 17:39:38 sos Exp $
 */

#include "ata.h"
#include "pci.h"
#if NATA > 0
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
#include <dev/ata/ata-all.h>

#ifdef __alpha__
#undef vtophys
#define	vtophys(va)	alpha_XXX_dmamap((vm_offset_t)va)
#endif

/* misc defines */
#define MIN(a,b) ((a)>(b)?(b):(a))    

#if NPCI > 0

int32_t
ata_dmainit(struct ata_softc *scp, int32_t device, 
	    int32_t apiomode, int32_t wdmamode, int32_t udmamode)
{
    int32_t type, devno, error;
    void *dmatab;

    if (!scp->bmaddr)
	return -1;
#ifdef ATA_DEBUGDMA
    printf("ata%d: dmainit: ioaddr=0x%x altioaddr=0x%x, bmaddr=0x%x\n",
	   scp->lun, scp->ioaddr, scp->altioaddr, scp->bmaddr);
#endif

    if (!(dmatab = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT)))
        return -1;

    if (((uintptr_t)dmatab >> PAGE_SHIFT) ^
	(((uintptr_t)dmatab + PAGE_SIZE - 1) >> PAGE_SHIFT)) {
        printf("ata_dmainit: dmatab crosses page boundary, no DMA\n");
        free(dmatab, M_DEVBUF);
        return -1;
    }
    scp->dmatab[device ? 1 : 0] = dmatab;

    type = pci_get_devid(scp->dev);

    switch(type) {

    case 0x71118086:	/* Intel PIIX4 */
	if (udmamode >= 2) {
    	    int32_t mask48, new48;

	    printf("ata%d: %s: setting up UDMA2 mode on PIIX4 chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
	    devno = (scp->unit << 1) + (device ? 1 : 0);
	    mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
	    new48 = (1 << devno) + (2 << (16 + (devno << 2)));
            pci_write_config(scp->dev, 0x48, 
			     (pci_read_config(scp->dev, 0x48, 4) &
			     ~mask48) | new48, 4);
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
	    printf("ata%d: %s: setting up WDMA2 mode on PIIX3/4 chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
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
                             (pci_read_config(scp->dev, 0x40, 4) &
			     ~mask40) | new40, 4);
            pci_write_config(scp->dev, 0x44,
                             (pci_read_config(scp->dev, 0x44, 4) &
			     ~mask44) | new44, 4);
	    return 0;
	}	
	break;

    case 0x12308086:	/* Intel PIIX */
	/* probably not worth the trouble */
	break;

    case 0x4d33105a:	/* Promise Ultra/33 / FastTrack controllers */
    case 0x4d38105a:	/* Promise Ultra/66 controllers */
	/* the promise seems to have trouble with DMA on ATAPI devices */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	devno = (scp->unit << 1) + (device ? 1 : 0);
	if (udmamode >=2) {
	    printf("ata%d: %s: setting up UDMA2 mode on Promise chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004127f3, 4);
	    return 0;
	}
	else if (wdmamode >= 2 && apiomode >= 4) {
	    printf("ata%d: %s: setting up WDMA2 mode on Promise chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004367f3, 4);
	    return 0;
        }
	else {
	    printf("ata%d: %s: setting up PIO mode on Promise chip OK\n",
		   scp->lun, (device) ? "slave" : "master");
	    pci_write_config(scp->dev, 0x60 + (devno << 2), 0x004fe924, 4);
	}
	break;
    
    case 0x522910b9:	/* AcerLabs Aladdin IV/V */
	if (udmamode >=2) {
	    int32_t word54 = pci_read_config(scp->dev, 0x54, 4);
	
	    printf("ata%d: %s: setting up UDMA2 mode on Aladdin chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
	    word54 |= 0x5555;
	    word54 |= (0x0000000A << (16 + (scp->unit << 3) + (device << 2)));
	    pci_write_config(scp->dev, 0x54, word54, 4);
	    return 0;
		
	}
	else if (wdmamode >= 2 && apiomode >= 4) {
	    printf("ata%d: %s: setting up WDMA2 mode on Aladdin chip ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
	    return 0;
	}
	break;

    default:		/* well, we have no support for this, but try anyways */
	if ((wdmamode >= 2 && apiomode >= 4) || udmamode >= 2) {
	    printf("ata%d: %s: setting up generic WDMA2 mode ",
		   scp->lun, (device) ? "slave" : "master");
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_FEA_SETXFER, ATA_WAIT_INTR);
	    if (error) {
		printf("failed\n");
		break;
	    }
	    printf("OK\n");
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

#ifdef ATA_DEBUGDMA
    printf("ata%d: dmasetup\n", scp->lun);
#endif
    if (((uintptr_t)data & 1) || (count & 1))
	return -1;

    if (!count) {
	printf("ata%d: zero length DMA transfer attempt on %s\n", 
	       scp->lun, (device ? "slave" : "master"));
	return -1;
    }
    
    dmatab = scp->dmatab[device ? 1 : 0];
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
#ifdef ATA_DEBUGDMA
printf("ata_dmasetup: base=%08x count%08x\n", 
	dma_base, dma_count);
#endif
    dmatab[i].base = dma_base;
    dmatab[i].count = (dma_count & 0xffff) | ATA_DMA_EOT;
    
    outl(scp->bmaddr + ATA_BMDTP_PORT, vtophys(dmatab));
#ifdef ATA_DEBUGDMA
printf("dmatab=%08x %08x\n", vtophys(dmatab), inl(scp->bmaddr+ATA_BMDTP_PORT));
#endif
    outb(scp->bmaddr + ATA_BMCMD_PORT, flags ? ATA_BMCMD_WRITE_READ:0);
    outb(scp->bmaddr + ATA_BMSTAT_PORT, (inb(scp->bmaddr + ATA_BMSTAT_PORT) | 
				   (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    return 0;
}

void
ata_dmastart(struct ata_softc *scp, int32_t device)
{
#ifdef ATA_DEBUGDMA
    printf("ata%d: dmastart\n", scp->lun);
#endif
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) | ATA_BMCMD_START_STOP);
}

int32_t
ata_dmadone(struct ata_softc *scp, int32_t device)
{
#ifdef ATA_DEBUGDMA
    printf("ata%d: dmadone\n", scp->lun);
#endif
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

int32_t
ata_dmastatus(struct ata_softc *scp, int32_t device)
{
#ifdef ATA_DEBUGDMA
    printf("ata%d: dmastatus\n", scp->lun);
#endif
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
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
ata_dmastart(struct ata_softc *scp, int32_t device)
{
}

int32_t
ata_dmadone(struct ata_softc *scp, int32_t device)
{
    return -1;
}

int32_t
ata_dmastatus(struct ata_softc *scp, int32_t device)
{
    return -1;
}

#endif /* NPCI > 0 */
#endif /* NATA > 0 */ 
