/*-
 * Copyright (c) 1998,1999,2000 Søren Schmidt
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
#include <sys/buf.h>
#include <sys/malloc.h> 
#include <sys/bus.h>
#include <sys/disk.h>
#include <sys/devicestat.h>
#include <vm/vm.h>	     
#include <vm/pmap.h>
#if NPCI > 0
#include <pci/pcivar.h>
#endif
#if NAPM > 0
#include <machine/apm_bios.h>
#endif
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-disk.h>

/* prototypes */
static void promise_timing(struct ata_softc *, int32_t, int32_t);
static void hpt366_timing(struct ata_softc *, int32_t, int32_t);

/* misc defines */
#ifdef __alpha__
#undef vtophys
#define vtophys(va)	alpha_XXX_dmamap((vm_offset_t)va)
#endif

#if NPCI > 0

int32_t
ata_dmainit(struct ata_softc *scp, int32_t device, 
	    int32_t apiomode, int32_t wdmamode, int32_t udmamode)
{
    int32_t devno = (scp->unit << 1) + ATA_DEV(device);
    int32_t error;

    if (!scp->bmaddr)
	return -1;

    /* if simplex controller, only allow DMA on primary channel */
    if (scp->unit == 1) {
	outb(scp->bmaddr + ATA_BMSTAT_PORT, inb(scp->bmaddr + ATA_BMSTAT_PORT) &
	     (ATA_BMSTAT_DMA_MASTER | ATA_BMSTAT_DMA_SLAVE));
	if (inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_DMA_SIMPLEX) {
	    ata_printf(scp, device, "simplex device, DMA on primary only\n");
	    return -1;
	}
    }

    if (!scp->dmatab[ATA_DEV(device)]) {
	void *dmatab;

	if (!(dmatab = malloc(PAGE_SIZE, M_DEVBUF, M_NOWAIT)))
	    return -1;
	if (((uintptr_t)dmatab >> PAGE_SHIFT) ^
	    (((uintptr_t)dmatab + PAGE_SIZE - 1) >> PAGE_SHIFT)) {
	    ata_printf(scp, device, "dmatab crosses page boundary, no DMA\n");
	    free(dmatab, M_DEVBUF);
	    return -1;
	}
	scp->dmatab[ATA_DEV(device)] = dmatab;
    }

    switch (scp->chiptype) {

    case 0x71118086:	/* Intel PIIX4 */
    case 0x71998086:	/* Intel PIIX4e */
    case 0x24118086:	/* Intel ICH */
    case 0x24218086:	/* Intel ICH0 */
	if (udmamode >= 2) {
	    int32_t mask48, new48;

	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, "%s setting up UDMA2 mode on %s chip\n",
			   (error) ? "failed" : "success",
			   (scp->chiptype == 0x24118086) ? "ICH" : 
			    (scp->chiptype == 0x24218086) ? "ICH0" :"PIIX4");
	    if (!error) {
		mask48 = (1 << devno) + (3 << (16 + (devno << 2)));
		new48 = (1 << devno) + (2 << (16 + (devno << 2)));
		pci_write_config(scp->dev, 0x48, 
				 (pci_read_config(scp->dev, 0x48, 4) &
				 ~mask48) | new48, 4);
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
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
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, "%s setting up WDMA2 mode on %s chip\n",
			   (error) ? "failed" : "success",
			   (scp->chiptype == 0x70108086) ? "PIIX3" : 
			    (scp->chiptype == 0x24118086) ? "ICH" :
			     (scp->chiptype == 0x24218086) ? "ICH0" :"PIIX4");
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
		if (scp->unit) {
		    mask40 <<= 16;
		    new40 <<= 16;
		    mask44 <<= 4;
		    new44 <<= 4;
		}
		pci_write_config(scp->dev, 0x40,
				 (pci_read_config(scp->dev, 0x40, 4) & ~mask40)|
 				 new40, 4);
		pci_write_config(scp->dev, 0x44,
				 (pci_read_config(scp->dev, 0x44, 4) & ~mask44)|
 				 new44, 4);
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x12308086:	/* Intel PIIX */
	if (wdmamode >= 2 && apiomode >= 4) {
	    int32_t word40;

	    word40 = pci_read_config(scp->dev, 0x40, 4);
	    word40 >>= scp->unit * 16;

	    /* Check for timing config usable for DMA on controller */
	    if (!((word40 & 0x3300) == 0x2300 &&
		  ((word40 >> (device == ATA_MASTER ? 0 : 4)) & 1) == 1))
		break;

	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, 
			   "%s setting up WDMA2 mode on PIIX chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	break;

    case 0x522910b9:	/* AcerLabs Aladdin IV/V */
	/* the Aladdin doesn't support ATAPI DMA on both master & slave */
	if (scp->devices & ATA_ATAPI_MASTER && scp->devices & ATA_ATAPI_SLAVE) {
	    ata_printf(scp, device,
		       "Aladdin: two atapi devices on this channel, no DMA\n");
	    break;
	}
	if (udmamode >= 2) {
	    int32_t word54 = pci_read_config(scp->dev, 0x54, 4);
	
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA2 mode on Aladdin chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		word54 |= 0x5555;
		word54 |= (0x0a << (16 + (scp->unit << 3) + (device << 2)));
		pci_write_config(scp->dev, 0x54, word54, 4);
		pci_write_config(scp->dev, 0x53, 
				 pci_read_config(scp->dev, 0x53, 1) | 0x03, 1);
		scp->flags |= ATA_ATAPI_DMA_RO;
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, 
			   "%s setting up WDMA2 mode on Aladdin chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(scp->dev, 0x53, 
				 pci_read_config(scp->dev, 0x53, 1) | 0x03, 1);
		scp->flags |= ATA_ATAPI_DMA_RO;
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x05711106:	/* VIA 82C571, 82C586, 82C596 & 82C686 */
    case 0x74091022:	/* AMD 756 */
	/* UDMA modes on 82C686 */
	if (ata_find_dev(scp->dev, 0x06861106)) {
	    if (udmamode >= 4) {
		error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				    ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_printf(scp, device, 
		    	       "%s setting up UDMA4 mode on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
	            pci_write_config(scp->dev, 0x53 - devno, 0xe8, 1);
		    scp->mode[ATA_DEV(device)] = ATA_UDMA4;
		    return 0;
		}
	    }
	    if (udmamode >= 2) {
		error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				    ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
		if (bootverbose)
		    ata_printf(scp, device,
			       "%s setting up UDMA2 mode on VIA chip\n",
			       (error) ? "failed" : "success");
		if (!error) {
	            pci_write_config(scp->dev, 0x53 - devno, 0xea, 1);
		    scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		    return 0;
		}
	    }
	}

	/* UDMA4 mode on AMD 756 */
	if (udmamode >= 4 && scp->chiptype == 0x74091022) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA4 mode on AMD chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
	        pci_write_config(scp->dev, 0x53 - devno, 0xc3, 1);
		scp->mode[ATA_DEV(device)] = ATA_UDMA4;
		return 0;
	    }
	}

	/* UDMA2 mode only on 82C586 > rev1, 82C596, AMD 756 */
	if ((udmamode >= 2 && ata_find_dev(scp->dev, 0x05861106) &&
	     pci_read_config(scp->dev, 0x08, 1) >= 0x01) ||
	    (udmamode >= 2 && ata_find_dev(scp->dev, 0x05961106)) ||
	    (udmamode >= 2 && scp->chiptype == 0x74091022)) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, "%s setting up UDMA2 mode on %s chip\n",
			   (error) ? "failed" : "success",
			   (scp->chiptype == 0x74091022) ? "AMD" : "VIA");
	    if (!error) {
	        pci_write_config(scp->dev, 0x53 - devno, 0xc0, 1);
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device, "%s setting up WDMA2 mode on %s chip\n",
			   (error) ? "failed" : "success",
			   (scp->chiptype == 0x74091022) ? "AMD" : "VIA");
	    if (!error) {
	        pci_write_config(scp->dev, 0x53 - devno, 0x82, 1);
	        pci_write_config(scp->dev, 0x4b - devno, 0x31, 1);
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x55131039:	/* SiS 5591 */
	if (udmamode >= 2) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA2 mode on SiS chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(scp->dev, 0x40 + (devno << 1), 0xa301, 2);
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
	}
	if (wdmamode >=2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up WDMA2 mode on SiS chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		pci_write_config(scp->dev, 0x40 + (devno << 1), 0x0301, 2);
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	/* we could set PIO mode timings, but we assume the BIOS did that */
	break;

    case 0x4d33105a:	/* Promise Ultra33 / FastTrak33 controllers */
    case 0x4d38105a:	/* Promise Ultra66 / FastTrak66 controllers */
	/* the Promise can only do DMA on ATA disks not on ATAPI devices */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	if (udmamode >=4 && scp->chiptype == 0x4d38105a &&
	    !(pci_read_config(scp->dev, 0x50, 2)&(scp->unit ? 1<<11 : 1<<10))) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA4 mode on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		outb(scp->bmaddr+0x11, inl(scp->bmaddr+0x11) | scp->unit ? 8:2);
		promise_timing(scp, devno, ATA_UDMA4);
		scp->mode[ATA_DEV(device)] = ATA_UDMA4;
		return 0;
	    }
	}
	if (udmamode >= 2) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA2 mode on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(scp, devno, ATA_UDMA2);
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up WDMA2 mode on Promise chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		promise_timing(scp, devno, ATA_WDMA2);
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
			    ata_pio2mode(apiomode), 
			    ATA_C_F_SETXFER, ATA_WAIT_READY);
	if (bootverbose)
	    ata_printf(scp, device,
		       "%s setting up PIO%d mode on Promise chip\n",
		       (error) ? "failed" : "success",
		       (apiomode >= 0) ? apiomode : 0);
	promise_timing(scp, devno, ata_pio2mode(apiomode));
	scp->mode[ATA_DEV(device)] = ata_pio2mode(apiomode);
	return -1;
    
    case 0x00041103:	/* HighPoint HPT366 controller */
	/* no ATAPI devices for now */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	if (udmamode >=4 && !(pci_read_config(scp->dev, 0x5a, 1) & 0x2)) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA4, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA4 mode on HPT366 chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt366_timing(scp, devno, ATA_UDMA4);
		scp->mode[ATA_DEV(device)] = ATA_UDMA4;
		return 0;
	    }
	}
	if (udmamode >= 2) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_UDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up UDMA2 mode on HPT366 chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt366_timing(scp, devno, ATA_UDMA2);
		scp->mode[ATA_DEV(device)] = ATA_UDMA2;
		return 0;
	    }
	}
	if (wdmamode >= 2 && apiomode >= 4) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up WDMA2 mode on HPT366 chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		hpt366_timing(scp, devno, ATA_WDMA2);
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
	error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
			    ata_pio2mode(apiomode), 
			    ATA_C_F_SETXFER, ATA_WAIT_READY);
	if (bootverbose)
	    ata_printf(scp, device, "%s setting up PIO%d mode on HPT366 chip\n",
		       (error) ? "failed" : "success",
		       (apiomode >= 0) ? apiomode : 0);
	hpt366_timing(scp, devno, ata_pio2mode(apiomode));
	scp->mode[ATA_DEV(device)] = ata_pio2mode(apiomode);
	return -1;

    default:		/* unknown controller chip */
	/* better not try generic DMA on ATAPI devices it almost never works */
	if ((device == ATA_MASTER && scp->devices & ATA_ATAPI_MASTER) ||
	    (device == ATA_SLAVE && scp->devices & ATA_ATAPI_SLAVE))
	    break;

	/* well, we have no support for this, but try anyways */
	if ((wdmamode >= 2 && apiomode >= 4) && scp->bmaddr) {
	    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
				ATA_WDMA2, ATA_C_F_SETXFER, ATA_WAIT_READY);
	    if (bootverbose)
		ata_printf(scp, device,
			   "%s setting up WDMA2 mode on generic chip\n",
			   (error) ? "failed" : "success");
	    if (!error) {
		scp->mode[ATA_DEV(device)] = ATA_WDMA2;
		return 0;
	    }
	}
    }
    error = ata_command(scp, device, ATA_C_SETFEATURES, 0, 0, 0,
			ata_pio2mode(apiomode), ATA_C_F_SETXFER,ATA_WAIT_READY);
    if (bootverbose)
	ata_printf(scp, device, "%s setting up PIO%d mode on generic chip\n",
		   (error) ? "failed" : "success",(apiomode>=0) ? apiomode : 0);
    scp->mode[ATA_DEV(device)] = ata_pio2mode(apiomode);
    return -1;
}

int32_t
ata_dmasetup(struct ata_softc *scp, int32_t device, 
	     int8_t *data, int32_t count, int32_t flags)
{
    struct ata_dmaentry *dmatab;
    u_int32_t dma_count, dma_base;
    int32_t i = 0;

    if (((uintptr_t)data & 1) || (count & 1))
	return -1;

    if (!count) {
	ata_printf(scp, device, "zero length DMA transfer attempted\n");
	return -1;
    }
    
    dmatab = scp->dmatab[ATA_DEV(device)];
    dma_base = vtophys(data);
    dma_count = min(count, (PAGE_SIZE - ((uintptr_t)data & PAGE_MASK)));
    data += dma_count;
    count -= dma_count;

    while (count) {
	dmatab[i].base = dma_base;
	dmatab[i].count = (dma_count & 0xffff);
	i++; 
	if (i >= ATA_DMA_ENTRIES) {
	    ata_printf(scp, device, "too many segments in DMA table\n");
	    return -1;
	}
	dma_base = vtophys(data);
	dma_count = min(count, PAGE_SIZE);
	data += min(count, PAGE_SIZE);
	count -= min(count, PAGE_SIZE);
    }
    dmatab[i].base = dma_base;
    dmatab[i].count = (dma_count & 0xffff) | ATA_DMA_EOT;
    outl(scp->bmaddr + ATA_BMDTP_PORT, vtophys(dmatab));
    outb(scp->bmaddr + ATA_BMCMD_PORT, flags ? ATA_BMCMD_WRITE_READ:0);
    outb(scp->bmaddr + ATA_BMSTAT_PORT, (inb(scp->bmaddr + ATA_BMSTAT_PORT) | 
				   (ATA_BMSTAT_INTERRUPT | ATA_BMSTAT_ERROR)));
    return 0;
}

void
ata_dmastart(struct ata_softc *scp)
{
    scp->flags |= ATA_DMA_ACTIVE;
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) | ATA_BMCMD_START_STOP);
}

int32_t
ata_dmadone(struct ata_softc *scp)
{
    outb(scp->bmaddr + ATA_BMCMD_PORT, 
	 inb(scp->bmaddr + ATA_BMCMD_PORT) & ~ATA_BMCMD_START_STOP);
    scp->flags &= ~ATA_DMA_ACTIVE;
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

int32_t
ata_dmastatus(struct ata_softc *scp)
{
    return inb(scp->bmaddr + ATA_BMSTAT_PORT) & ATA_BMSTAT_MASK;
}

static void
promise_timing(struct ata_softc *scp, int32_t devno, int32_t mode)
{
    u_int32_t timing;
    switch (mode) {
    default:
    case ATA_PIO0:	timing = 0x004ff329; break;
    case ATA_PIO1:	timing = 0x004fec25; break;
    case ATA_PIO2:	timing = 0x004fe823; break;
    case ATA_PIO3:	timing = 0x004fe622; break;
    case ATA_PIO4:	timing = 0x004fe421; break;
    case ATA_WDMA2:	timing = 0x004367f3; break;
    case ATA_UDMA2:	timing = 0x004127f3; break;
    case ATA_UDMA4:	timing = 0x004127f3; break;
    }
    pci_write_config(scp->dev, 0x60 + (devno << 2), timing, 4);
}

static void
hpt366_timing(struct ata_softc *scp, int32_t devno, int32_t mode)
{
    u_int32_t timing;

    switch (pci_read_config(scp->dev, 0x41 + (devno << 2), 1)) {
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
	default:	timing = 0x01208585;
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
	default:	timing = 0x0120a7a7;
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
	default:	timing = 0x0120d9d9;
	}
    }
    pci_write_config(scp->dev, 0x40 + (devno << 2) , timing, 4);
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
