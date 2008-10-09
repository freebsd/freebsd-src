/*-
 * Copyright (c) 2008 S\xf8ren Schmidt <sos@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/ata.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <sys/errno.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/bus.h>

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

/* prototypes */
static int ata_devel_chipinit(device_t dev);
static int ata_devel_allocate(device_t dev);
static void ata_devel_setmode(device_t dev, int mode);


static int
ata_devel_probe(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    device_printf(dev, "ata_devel_probe(): PCIID=0x%08x\n", pci_get_devid(dev));
    if (pci_get_devid(dev) == 0x12345678) {
        printf("test probe successful!\n");
	device_set_desc(dev, "ATA DEVEL controller");
	ctlr->chipinit = ata_devel_chipinit;
	return 0;
    }
    return ENXIO;
}

static int
ata_devel_chipinit(device_t dev)
{
    struct ata_pci_controller *ctlr = device_get_softc(dev);

    /* setup interrupt delivery */
    if (ata_setup_interrupt(dev, ata_generic_intr))
	return ENXIO;

    /* perform any chipset specific setups here */

    /* setup function ptr's, in this case allocate and setmode */
    ctlr->allocate = ata_devel_allocate;
    ctlr->setmode = ata_devel_setmode;

    return 0;
}

static int
ata_devel_allocate(device_t dev)
{
    if (ata_pci_allocate(dev))
	return ENXIO;

    /* setup channel specifics here like offsets to registers etc */

    return 0;
}

static void
ata_devel_setmode(device_t dev, int mode)
{
    struct ata_device *atadev = device_get_softc(dev);
    int error;

    error = ata_controlcmd(dev, ATA_SETFEATURES, ATA_SF_SETXFER, 0,
			   ata_limit_mode(dev, mode, ATA_UDMA5));
    if (bootverbose)
	device_printf(dev, "%ssetting %s on DEVEL chip\n",
		      (error) ? "FAILURE " : "", ata_mode2str(mode));
    if (!error)
	atadev->mode = mode;
}

ATA_DECLARE_DRIVER(ata_devel);
