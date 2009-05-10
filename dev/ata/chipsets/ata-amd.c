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
static int ata_amd_chipinit(device_t dev);
static void ata_amd_setmode(device_t dev, int mode);

/* misc defines */
#define AMD_BUG		0x01
#define AMD_CABLE	0x02


/*
 * American Micro Devices (AMD) chipset support functions
 */
static int
ata_amd_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);
    static struct ata_chip_id ids[] =
    {{ ATA_AMD756,  0x00, 0x00,              0, ATA_UDMA4, "756" },
     { ATA_AMD766,  0x00, AMD_CABLE|AMD_BUG, 0, ATA_UDMA5, "766" },
     { ATA_AMD768,  0x00, AMD_CABLE,         0, ATA_UDMA5, "768" },
     { ATA_AMD8111, 0x00, AMD_CABLE,         0, ATA_UDMA6, "8111" },
     { ATA_AMD5536, 0x00, 0x00,              0, ATA_UDMA5, "CS5536" },
     { 0, 0, 0, 0, 0, 0}};

    if (pci_get_vendor(dev) != ATA_AMD_ID)
	return ENXIO;

    if (!(ctlr->chip = ata_match_chip(dev, ids)))
	return ENXIO;

    ata_set_desc(dev);
    ctlr->chipinit = ata_amd_chipinit;
    return 0;
}

static int
ata_amd_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    /* disable/set prefetch, postwrite */
    if (ctlr->chip->cfg1 & AMD_BUG)
	pci_write_config(dev, 0x41, pci_read_config(dev, 0x41, 1) & 0x0f, 1);
    else
	pci_write_config(dev, 0x41, pci_read_config(dev, 0x41, 1) | 0xf0, 1);

    ctlr->setmode = ata_amd_setmode;
    return 0;
}

static void
ata_amd_setmode(device_t dev, int mode)
{
    device_t gparent = GRANDPARENT(dev);
    struct ata_pci_controller *ctlr = device_get_softc(gparent);
    struct ata_channel *ch = device_get_softc(device_get_parent(dev));
    struct ata_device *atadev = device_get_softc(dev);
    u_int8_t timings[] = { 0xa8, 0x65, 0x42, 0x22, 0x20, 0x42, 0x22, 0x20,
			   0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20 };
    int modes[7] = { 0xc2, 0xc1, 0xc0, 0xc4, 0xc5, 0xc6, 0xc7 };
    int devno = (ch->unit << 1) + atadev->unit;
    int reg = 0x53 - devno;
    int error;

    mode = ata_limit_mode(dev, mode, ctlr->chip->max_dma);

    if (ctlr->chip->cfg1 & AMD_CABLE) {
	if (mode > ATA_UDMA2 &&
	    !(pci_read_config(gparent, 0x42, 1) & (1 << devno))) {
	    ata_print_cable(dev, "controller");
	    mode = ATA_UDMA2;
	}
    }
    else 
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

ATA_DECLARE_DRIVER(ata_amd);
