/*-
 * Copyright (c) 1998 - 2008 Søren Schmidt <sos@FreeBSD.org>
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/bus.h>
#include <sys/endian.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>
#include <dev/ata/ata-all.h>
#include <dev/ata/ata-pci.h>
#include <ata_if.h>

/* local prototypes */
static int ata_serverworks_chipinit(device_t dev);
static int ata_serverworks_allocate(device_t dev);
static void ata_serverworks_tf_read(struct ata_request *request);
static void ata_serverworks_tf_write(struct ata_request *request);
static void ata_serverworks_setmode(device_t dev, int mode);

/* misc defines */
#define SWKS_33		0
#define SWKS_66		1
#define SWKS_100	2
#define SWKS_MIO	3


/*
 * ServerWorks chipset support functions
 */
static int
ata_serverworks_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ROSB4,     0x00, SWKS_33,  0, ATA_UDMA2, "ROSB4" },
     { ATA_CSB5,      0x92, SWKS_100, 0, ATA_UDMA5, "CSB5" },
     { ATA_CSB5,      0x00, SWKS_66,  0, ATA_UDMA4, "CSB5" },
     { ATA_CSB6,      0x00, SWKS_100, 0, ATA_UDMA5, "CSB6" },
     { ATA_CSB6_1,    0x00, SWKS_66,  0, ATA_UDMA4, "CSB6" },
     { ATA_HT1000,    0x00, SWKS_100, 0, ATA_UDMA5, "HT1000" },
     { ATA_HT1000_S1, 0x00, SWKS_100, 4, ATA_SA150, "HT1000" },
     { ATA_HT1000_S2, 0x00, SWKS_MIO, 4, ATA_SA150, "HT1000" },
     { ATA_K2,        0x00, SWKS_MIO, 4, ATA_SA150, "K2" },
     { ATA_FRODO4,    0x00, SWKS_MIO, 4, ATA_SA150, "Frodo4" },
     { ATA_FRODO8,    0x00, SWKS_MIO, 8, ATA_SA150, "Frodo8" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_SERVERWORKS_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_serverworks_chipinit;
    return 0;
}

static int
ata_serverworks_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    if (ctlr->chip->cfg1 == SWKS_MIO) {
	ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if (!(ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						    &ctlr->r_rid2, RF_ACTIVE)))
	    return ENXIO;

	ctlr->channels = ctlr->chip->cfg2;
	ctlr->allocate = ata_serverworks_allocate;
	ctlr->setmode = ata_sata_setmode;
	return 0;
    }
    else if (ctlr->chip->cfg1 == SWKS_33) {
	device_t *children;
	int nchildren, i;

	/* locate the ISA part in the southbridge and enable UDMA33 */
	if (!device_get_children(device_get_parent(dev), &children,&nchildren)){
	    for (i = 0; i < nchildren; i++) {
		if (pci_get_devid(children[i]) == ATA_ROSB4_ISA) {
		    pci_write_config(children[i], 0x64,
				     (pci_read_config(children[i], 0x64, 4) &
				      ~0x00002000) | 0x00004000, 4);
		    break;
		}
	    }
	    free(children, M_TEMP);
	}
    }
    else {
	pci_write_config(dev, 0x5a,
			 (pci_read_config(dev, 0x5a, 1) & ~0x40) |
			 (ctlr->chip->cfg1 == SWKS_100) ? 0x03 : 0x02, 1);
    }
    ctlr->setmode = ata_serverworks_setmode;
    return 0;
}

static int
ata_serverworks_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int ch_offset;
    int i;

    ch_offset = ch->unit * 0x100;

    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
	ch->r_io[i].res = ctlr->r_res2;

    /* setup ATA registers */
    ch->r_io[ATA_DATA].offset = ch_offset + 0x00;
    ch->r_io[ATA_FEATURE].offset = ch_offset + 0x04;
    ch->r_io[ATA_COUNT].offset = ch_offset + 0x08;
    ch->r_io[ATA_SECTOR].offset = ch_offset + 0x0c;
    ch->r_io[ATA_CYL_LSB].offset = ch_offset + 0x10;
    ch->r_io[ATA_CYL_MSB].offset = ch_offset + 0x14;
    ch->r_io[ATA_DRIVE].offset = ch_offset + 0x18;
    ch->r_io[ATA_COMMAND].offset = ch_offset + 0x1c;
    ch->r_io[ATA_CONTROL].offset = ch_offset + 0x20;
    ata_default_registers(dev);

    /* setup DMA registers */
    ch->r_io[ATA_BMCMD_PORT].offset = ch_offset + 0x30;
    ch->r_io[ATA_BMSTAT_PORT].offset = ch_offset + 0x32;
    ch->r_io[ATA_BMDTP_PORT].offset = ch_offset + 0x34;

    /* setup SATA registers */
    ch->r_io[ATA_SSTATUS].offset = ch_offset + 0x40;
    ch->r_io[ATA_SERROR].offset = ch_offset + 0x44;
    ch->r_io[ATA_SCONTROL].offset = ch_offset + 0x48;

    ch->flags |= ATA_NO_SLAVE;
    ata_pci_hw(dev);
    ch->hw.tf_read = ata_serverworks_tf_read;
    ch->hw.tf_write = ata_serverworks_tf_write;

    /* chip does not reliably do 64K DMA transfers */
    ch->dma.max_iosize = 126 * DEV_BSIZE;

    return 0;
}

static void
ata_serverworks_tf_read(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	u_int16_t temp;

	request->u.ata.count = ATA_IDX_INW(ch, ATA_COUNT);
	temp = ATA_IDX_INW(ch, ATA_SECTOR);
	request->u.ata.lba = (u_int64_t)(temp & 0x00ff) |
			     ((u_int64_t)(temp & 0xff00) << 24);
	temp = ATA_IDX_INW(ch, ATA_CYL_LSB);
	request->u.ata.lba |= ((u_int64_t)(temp & 0x00ff) << 8) |
			      ((u_int64_t)(temp & 0xff00) << 32);
	temp = ATA_IDX_INW(ch, ATA_CYL_MSB);
	request->u.ata.lba |= ((u_int64_t)(temp & 0x00ff) << 16) |
			      ((u_int64_t)(temp & 0xff00) << 40);
    }
    else {
	request->u.ata.count = ATA_IDX_INW(ch, ATA_COUNT) & 0x00ff;
	request->u.ata.lba = (ATA_IDX_INW(ch, ATA_SECTOR) & 0x00ff) |
			     ((ATA_IDX_INW(ch, ATA_CYL_LSB) & 0x00ff) << 8) |
			     ((ATA_IDX_INW(ch, ATA_CYL_MSB) & 0x00ff) << 16) |
			     ((ATA_IDX_INW(ch, ATA_DRIVE) & 0xf) << 24);
    }
}

static void
ata_serverworks_tf_write(struct ata_request *request)
{
    struct ata_channel *ch = device_get_softc(request->parent);
    struct ata_device *atadev = device_get_softc(request->dev);

    if (atadev->flags & ATA_D_48BIT_ACTIVE) {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	ATA_IDX_OUTW(ch, ATA_SECTOR, ((request->u.ata.lba >> 16) & 0xff00) |
				      (request->u.ata.lba & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_LSB, ((request->u.ata.lba >> 24) & 0xff00) |
				       ((request->u.ata.lba >> 8) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_CYL_MSB, ((request->u.ata.lba >> 32) & 0xff00) | 
				       ((request->u.ata.lba >> 16) & 0x00ff));
	ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_LBA | ATA_DEV(atadev->unit));
    }
    else {
	ATA_IDX_OUTW(ch, ATA_FEATURE, request->u.ata.feature);
	ATA_IDX_OUTW(ch, ATA_COUNT, request->u.ata.count);
	if (atadev->flags & ATA_D_USE_CHS) {
	    int heads, sectors;
    
	    if (atadev->param.atavalid & ATA_FLAG_54_58) {
		heads = atadev->param.current_heads;
		sectors = atadev->param.current_sectors;
	    }
	    else {
		heads = atadev->param.heads;
		sectors = atadev->param.sectors;
	    }
	    ATA_IDX_OUTW(ch, ATA_SECTOR, (request->u.ata.lba % sectors)+1);
	    ATA_IDX_OUTW(ch, ATA_CYL_LSB,
			 (request->u.ata.lba / (sectors * heads)));
	    ATA_IDX_OUTW(ch, ATA_CYL_MSB,
			 (request->u.ata.lba / (sectors * heads)) >> 8);
	    ATA_IDX_OUTW(ch, ATA_DRIVE, ATA_D_IBM | ATA_DEV(atadev->unit) | 
			 (((request->u.ata.lba% (sectors * heads)) /
			   sectors) & 0xf));
	}
	else {
	    ATA_IDX_OUTW(ch, ATA_SECTOR, request->u.ata.lba);
	    ATA_IDX_OUTW(ch, ATA_CYL_LSB, request->u.ata.lba >> 8);
	    ATA_IDX_OUTW(ch, ATA_CYL_MSB, request->u.ata.lba >> 16);
	    ATA_IDX_OUTW(ch, ATA_DRIVE,
			 ATA_D_IBM | ATA_D_LBA | ATA_DEV(atadev->unit) |
			 ((request->u.ata.lba >> 24) & 0x0f));
	}
    }
}

static void
ata_serverworks_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + atadev->unit;
    int offset = (devno ^ 0x01) << 3;
    int error;
    u_int8_t piotimings[] = { 0x5d, 0x47, 0x34, 0x22, 0x20, 0x34, 0x22, 0x20,
			      0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    u_int8_t dmatimings[] = { 0x77, 0x21, 0x20 };

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "",
		      ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    pci_write_config(gparent, 0x56, 
			     (pci_read_config(gparent, 0x56, 2) &
			      ~(0xf << (devno << 2))) |
			     ((mode & ATA_MODE_MASK) << (devno << 2)), 2);
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) |
			     (0x01 << devno), 1);
	    pci_write_config(gparent, 0x44, 
			     (pci_read_config(gparent, 0x44, 4) &
			      ~(0xff << offset)) |
			     (dmatimings[2] << offset), 4);
	}
	else if (mode >= ATA_WDMA0) {
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) &
			      ~(0x01 << devno), 1);
	    pci_write_config(gparent, 0x44, 
			     (pci_read_config(gparent, 0x44, 4) &
			      ~(0xff << offset)) |
			     (dmatimings[mode & ATA_MODE_MASK] << offset), 4);
	}
	else
	    pci_write_config(gparent, 0x54,
			     pci_read_config(gparent, 0x54, 1) &
			     ~(0x01 << devno), 1);

	pci_write_config(gparent, 0x40, 
			 (pci_read_config(gparent, 0x40, 4) &
			  ~(0xff << offset)) |
			 (piotimings[ata_mode2idx(mode)] << offset), 4);
	atadev->mode = mode;
    }
}

ATA_DECLARE_DRIVER(ata_serverworks);
