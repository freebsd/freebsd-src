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
static int ata_nvidia_chipinit(device_t dev);
static int ata_nvidia_allocate(device_t dev);
static int ata_nvidia_status(device_t dev);
static void ata_nvidia_reset(device_t dev);
static void ata_nvidia_setmode(device_t dev, int mode);

/* misc defines */
#define NV4             0x01
#define NVQ             0x02


/*
 * nVidia chipset support functions
 */
static int
ata_nvidia_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_NFORCE1,         0, 0,       0, ATA_UDMA5, "nForce" },
     { ATA_NFORCE2,         0, 0,       0, ATA_UDMA6, "nForce2" },
     { ATA_NFORCE2_PRO,     0, 0,       0, ATA_UDMA6, "nForce2 Pro" },
     { ATA_NFORCE2_PRO_S1,  0, 0,       0, ATA_SA150, "nForce2 Pro" },
     { ATA_NFORCE3,         0, 0,       0, ATA_UDMA6, "nForce3" },
     { ATA_NFORCE3_PRO,     0, 0,       0, ATA_UDMA6, "nForce3 Pro" },
     { ATA_NFORCE3_PRO_S1,  0, 0,       0, ATA_SA150, "nForce3 Pro" },
     { ATA_NFORCE3_PRO_S2,  0, 0,       0, ATA_SA150, "nForce3 Pro" },
     { ATA_NFORCE_MCP04,    0, 0,       0, ATA_UDMA6, "nForce MCP" },
     { ATA_NFORCE_MCP04_S1, 0, NV4,     0, ATA_SA150, "nForce MCP" },
     { ATA_NFORCE_MCP04_S2, 0, NV4,     0, ATA_SA150, "nForce MCP" },
     { ATA_NFORCE_CK804,    0, 0,       0, ATA_UDMA6, "nForce CK804" },
     { ATA_NFORCE_CK804_S1, 0, NV4,     0, ATA_SA300, "nForce CK804" },
     { ATA_NFORCE_CK804_S2, 0, NV4,     0, ATA_SA300, "nForce CK804" },
     { ATA_NFORCE_MCP51,    0, 0,       0, ATA_UDMA6, "nForce MCP51" },
     { ATA_NFORCE_MCP51_S1, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP51" },
     { ATA_NFORCE_MCP51_S2, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP51" },
     { ATA_NFORCE_MCP55,    0, 0,       0, ATA_UDMA6, "nForce MCP55" },
     { ATA_NFORCE_MCP55_S1, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP55" },
     { ATA_NFORCE_MCP55_S2, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP55" },
     { ATA_NFORCE_MCP61,    0, 0,       0, ATA_UDMA6, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S1, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S2, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP61_S3, 0, NV4|NVQ, 0, ATA_SA300, "nForce MCP61" },
     { ATA_NFORCE_MCP65,    0, 0,       0, ATA_UDMA6, "nForce MCP65" },
     { ATA_NFORCE_MCP67,    0, 0,       0, ATA_UDMA6, "nForce MCP67" },
     { ATA_NFORCE_MCP73,    0, 0,       0, ATA_UDMA6, "nForce MCP73" },
     { ATA_NFORCE_MCP77,    0, 0,       0, ATA_UDMA6, "nForce MCP77" },
     { 0, 0, 0, 0, 0, 0}} ;

    if (pci_get_vendor(dev) != ATA_NVIDIA_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_nvidia_chipinit;
    return 0;
}

static int
ata_nvidia_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    if (ctlr->chip->max_dma >= ATA_SA150) {
	if (pci_read_config(dev, PCIR_BAR(5), 1) & 1)
	    ctlr->r_type2 = SYS_RES_IOPORT;
	else
	    ctlr->r_type2 = SYS_RES_MEMORY;
	ctlr->r_rid2 = PCIR_BAR(5);
	if ((ctlr->r_res2 = bus_alloc_resource_any(dev, ctlr->r_type2,
						   &ctlr->r_rid2, RF_ACTIVE))) {
	    int offset = ctlr->chip->cfg1 & NV4 ? 0x0440 : 0x0010;

	    ctlr->allocate = ata_nvidia_allocate;
	    ctlr->reset = ata_nvidia_reset;

	    /* enable control access */
	    pci_write_config(dev, 0x50, pci_read_config(dev, 0x50, 1) | 0x04,1);

	    if (ctlr->chip->cfg1 & NVQ) {
		/* clear interrupt status */
		ATA_OUTL(ctlr->r_res2, offset, 0x00ff00ff);

		/* enable device and PHY state change interrupts */
		ATA_OUTL(ctlr->r_res2, offset + 4, 0x000d000d);

		/* disable NCQ support */
		ATA_OUTL(ctlr->r_res2, 0x0400,
			 ATA_INL(ctlr->r_res2, 0x0400) & 0xfffffff9);
	    } 
	    else {
		/* clear interrupt status */
		ATA_OUTB(ctlr->r_res2, offset, 0xff);

		/* enable device and PHY state change interrupts */
		ATA_OUTB(ctlr->r_res2, offset + 1, 0xdd);
	    }

	    /* enable PCI interrupt */
	    pci_write_config(dev, PCIR_COMMAND,
			     pci_read_config(dev, PCIR_COMMAND, 2) & ~0x0400,2);

	}
	ctlr->setmode = ata_sata_setmode;
    }
    else {
	/* disable prefetch, postwrite */
	pci_write_config(dev, 0x51, pci_read_config(dev, 0x51, 1) & 0x0f, 1);
	ctlr->setmode = ata_nvidia_setmode;
    }
    return 0;
}

static int
ata_nvidia_allocate(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);

    /* setup the usual register normal pci style */
    if (ata_pci_allocate(dev))
	return ENXIO;

    ch->r_io[ATA_SSTATUS].res = ctlr->r_res2;
    ch->r_io[ATA_SSTATUS].offset = (ch->unit << 6);
    ch->r_io[ATA_SERROR].res = ctlr->r_res2;
    ch->r_io[ATA_SERROR].offset = 0x04 + (ch->unit << 6);
    ch->r_io[ATA_SCONTROL].res = ctlr->r_res2;
    ch->r_io[ATA_SCONTROL].offset = 0x08 + (ch->unit << 6);

    ch->hw.status = ata_nvidia_status;
    ch->flags |= ATA_NO_SLAVE;

    return 0;
}

static int 
ata_nvidia_status(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(device_get_parent(dev));
    struct ata_channel *ch = device_get_softc(dev);
    int offset = ctlr->chip->cfg1 & NV4 ? 0x0440 : 0x0010;
    int shift = ch->unit << (ctlr->chip->cfg1 & NVQ ? 4 : 2);
    u_int32_t istatus;

    /* get interrupt status */
    if (ctlr->chip->cfg1 & NVQ)
	istatus = ATA_INL(ctlr->r_res2, offset);
    else
	istatus = ATA_INB(ctlr->r_res2, offset);

    /* do we have any PHY events ? */
    if (istatus & (0x0c << shift))
	ata_sata_phy_check_events(dev);

    /* clear interrupt(s) */
    if (ctlr->chip->cfg1 & NVQ)
	ATA_OUTL(ctlr->r_res2, offset, (0x0f << shift) | 0x00f000f0);
    else
	ATA_OUTB(ctlr->r_res2, offset, (0x0f << shift));

    /* do we have any device action ? */
    return (istatus & (0x01 << shift));
}

static void
ata_nvidia_reset(device_t dev)
{
    if (ata_sata_phy_reset(dev))
	ata_generic_reset(dev);
}

static void
ata_nvidia_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    u_int8_t timings[] = { 0xa8, 0x65, 0x42, 0x22, 0x20, 0x42, 0x22, 0x20,
			   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    int modes[7] = { 0xc2, 0xc1, 0xc0, 0xc4, 0xc5, 0xc6, 0xc7 };
    int devno = (ch->unit << 1) + atadev->unit;
    int reg = 0x63 - devno;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);
    mode = ata_check_80pin(dev, mode);

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0, mode);
    if (bootverbose)
	device_printf(dev, "%ssetting %s on %s chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode),
		      ctlr->chip->text);
    if (!error) {
	pci_write_config(gparent, reg - 0x08, timings[ata_mode2idx(mode)], 1);
	if (mode >= ATA_UDMA0)
	    pci_write_config(gparent, reg, modes[mode & ATA_MODE_MASK], 1);
	else
	    pci_write_config(gparent, reg, 0x8b, 1);
	atadev->mode = mode;
    }
}

ATA_DECLARE_DRIVER(ata_nvidia);
