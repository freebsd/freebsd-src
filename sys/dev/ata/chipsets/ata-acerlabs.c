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
static int ata_ali_chipinit(device_t dev);
static int ata_ali_allocate(device_t dev);
static int ata_ali_sata_allocate(device_t dev);
static void ata_ali_reset(device_t dev);
static void ata_ali_setmode(device_t dev, int mode);

/* misc defines */
#define ALI_OLD		0x01
#define ALI_NEW		0x02
#define ALI_SATA	0x04


/*
 * Acer Labs Inc (ALI) chipset support functions
 */
static int
ata_ali_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_ALI_5289, 0x00, 2, ALI_SATA, ATA_SA150, "M5289" },
     { ATA_ALI_5288, 0x00, 4, ALI_SATA, ATA_SA300, "M5288" },
     { ATA_ALI_5287, 0x00, 4, ALI_SATA, ATA_SA150, "M5287" },
     { ATA_ALI_5281, 0x00, 2, ALI_SATA, ATA_SA150, "M5281" },
     { ATA_ALI_5229, 0xc5, 0, ALI_NEW,  ATA_UDMA6, "M5229" },
     { ATA_ALI_5229, 0xc4, 0, ALI_NEW,  ATA_UDMA5, "M5229" },
     { ATA_ALI_5229, 0xc2, 0, ALI_NEW,  ATA_UDMA4, "M5229" },
     { ATA_ALI_5229, 0x20, 0, ALI_OLD,  ATA_UDMA2, "M5229" },
     { ATA_ALI_5229, 0x00, 0, ALI_OLD,  ATA_WDMA2, "M5229" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_ACER_LABS_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_ali_chipinit;
    return 0;
}

static int
ata_ali_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    switch (ctlr->chip->cfg2) {
    case ALI_SATA:
	ctlr->channels = ctlr->chip->cfg1;
	ctlr->allocate = ata_ali_sata_allocate;
	ctlr->setmode = ata_sata_setmode;

	/* AHCI mode is correctly supported only on the ALi 5288. */
	if ((ctlr->chip->chipid == ATA_ALI_5288) &&
	    (ata_ahci_chipinit(dev) != ENXIO))
            return 0;

	/* enable PCI interrupt */
	pci_write_config(dev, PCIR_COMMAND,
			 pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400, 2);
	break;

    case ALI_NEW:
	/* use device interrupt as byte count end */
	pci_write_config(dev, 0x4a, pci_read_config(dev, 0x4a, 1) | 0x20, 1);

	/* enable cable detection and UDMA support on newer chips */
	pci_write_config(dev, 0x4b, pci_read_config(dev, 0x4b, 1) | 0x09, 1);

	/* enable ATAPI UDMA mode */
	pci_write_config(dev, 0x53, pci_read_config(dev, 0x53, 1) | 0x01, 1);

	/* only chips with revision > 0xc4 can do 48bit DMA */
	if (ctlr->chip->chiprev <= 0xc4)
	    device_printf(dev,
			  "using PIO transfers above 137GB as workaround for "
			  "48bit DMA access bug, expect reduced performance\n");
	ctlr->allocate = ata_ali_allocate;
	ctlr->reset = ata_ali_reset;
	ctlr->setmode = ata_ali_setmode;
	break;

    case ALI_OLD:
	/* deactivate the ATAPI FIFO and enable ATAPI UDMA */
	pci_write_config(dev, 0x53, pci_read_config(dev, 0x53, 1) | 0x03, 1);
	ctlr->setmode = ata_ali_setmode;
	break;
    }
    return 0;
}

static int
ata_ali_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    /* older chips can't do 48bit DMA transfers */
    if (ctlr->chip->chiprev <= 0xc4)
	ch->flags |= ATA_NO_48BIT_DMA;

    return 0;
}

static int
ata_ali_sata_allocate(device_t dev)
{
    device_t parent = device_get_parent(dev);
    struct ata_pci_controller *ctlr = device_get_softc(parent);
    struct ata_channel *ch = device_get_softc(dev);
    struct resource *io = NULL, *ctlio = NULL;
    int unit01 = (ch->unit & 1), unit10 = (ch->unit & 2);
    int i, rid;
		
    rid = PCIR_BAR(0) + (unit01 ? 8 : 0);
    io = bus_alloc_resource_any(parent, SYS_RES_IOPORT, &rid, RF_ACTIVE);
    if (!io)
	return ENXIO;

    rid = PCIR_BAR(1) + (unit01 ? 8 : 0);
    ctlio = bus_alloc_resource_any(parent, SYS_RES_IOPORT, &rid, RF_ACTIVE);
    if (!ctlio) {
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
	return ENXIO;
    }
		
    for (i = ATA_DATA; i <= ATA_COMMAND; i ++) {
	ch->r_io[i].res = io;
	ch->r_io[i].offset = i + (unit10 ? 8 : 0);
    }
    ch->r_io[ATA_CONTROL].res = ctlio;
    ch->r_io[ATA_CONTROL].offset = 2 + (unit10 ? 4 : 0);
    ch->r_io[ATA_IDX_ADDR].res = io;
    ata_default_registers(dev);
    if (ctlr->r_res1) {
	for (i = ATA_BMCMD_PORT; i <= ATA_BMDTP_PORT; i++) {
	    ch->r_io[i].res = ctlr->r_res1;
	    ch->r_io[i].offset = (i - ATA_BMCMD_PORT)+(ch->unit * ATA_BMIOSIZE);
	}
    }
    ch->flags |= ATA_NO_SLAVE;

    /* XXX SOS PHY handling awkward in ALI chip not supported yet */
    ata_pci_hw(dev);
    return 0;
}

static void
ata_ali_reset(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int nchildren, i;

    ata_generic_reset(dev);

    /*
     * workaround for datacorruption bug found on at least SUN Blade-100
     * find the ISA function on the southbridge and disable then enable
     * the ATA channel tristate buffer
     */
    if (ctlr->chip->chiprev == 0xc3 || ctlr->chip->chiprev == 0xc2) {
	if (!device_get_children(GRANDPARENT(dev), &children, &nchildren)) {
	    for (i = 0; i < nchildren; i++) {
		if (pci_get_devid(children[i]) == ATA_ALI_1533) {
		    pci_write_config(children[i], 0x58, 
				     pci_read_config(children[i], 0x58, 1) &
				     ~(0x04 << ch->unit), 1);
		    pci_write_config(children[i], 0x58, 
				     pci_read_config(children[i], 0x58, 1) |
				     (0x04 << ch->unit), 1);
		    break;
		}
	    }
	    free(children, M_TEMP);
	}
    }
}

static void
ata_ali_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    int devno = (ch->unit << 1) + atadev->unit;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg2 & ALI_NEW) {
	if (mode > ATA_UDMA2 &&
	    pci_read_config(gparent, 0x4a, 1) & (1 << ch->unit)) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else
	mode = ata_check_80pin(dev, mode);

    if (ctlr->chip->cfg2 & ALI_OLD) {
	/* doesn't support ATAPI DMA on write */
	ch->flags |= ATA_ATAPI_DMA_RO;
	if (ch->devices & ATA_ATAPI_MASTER && ch->devices & ATA_ATAPI_SLAVE) {
	    /* doesn't support ATAPI DMA on two ATAPI devices */
	    device_printf(dev, "two atapi devices on this channel, no DMA\n");
	    mode = ata_limit_mode(dev, mode, ATA_PIO_MAX);
	}
    }

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);

    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		   (error) ? "FAILURE " : "", 
		   ata_mode2str(mode), ctlr->chip->text);
    if (!error) {
	if (mode >= ATA_UDMA0) {
	    u_int8_t udma[] = {0x0c, 0x0b, 0x0a, 0x09, 0x08, 0x0f, 0x0d};
	    u_int32_t word54 = pci_read_config(gparent, 0x54, 4);

	    word54 &= ~(0x000f000f << (devno << 2));
	    word54 |= (((udma[mode&ATA_MODE_MASK]<<16)|0x05)<<(devno<<2));
	    pci_write_config(gparent, 0x54, word54, 4);
	    pci_write_config(gparent, 0x58 + (ch->unit << 2),
			     0x00310001, 4);
	}
	else {
	    u_int32_t piotimings[] =
		{ 0x006d0003, 0x00580002, 0x00440001, 0x00330001,
		  0x00310001, 0x00440001, 0x00330001, 0x00310001};

	    pci_write_config(gparent, 0x54, pci_read_config(gparent, 0x54, 4) &
					    ~(0x0008000f << (devno << 2)), 4);
	    pci_write_config(gparent, 0x58 + (ch->unit << 2),
			     piotimings[ata_mode2idx(mode)], 4);
	}
	atadev->mode = mode;
    }
}

ATA_DECLARE_DRIVER(ata_ali);
MODULE_DEPEND(ata_ali, ata_ahci, 1, 1, 1);
