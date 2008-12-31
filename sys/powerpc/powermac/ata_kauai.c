/*-
 * Copyright 2004 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/powerpc/powermac/ata_kauai.c,v 1.13.18.1 2008/11/25 02:59:29 kensmith Exp $");

/*
 * Mac 'Kauai' PCI ATA controller
 */
#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <machine/stdarg.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <sys/ata.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

#include <dev/ofw/openfirm.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcireg.h>

#define  ATA_KAUAI_REGOFFSET	0x2000

/*
 * Offset to alt-control register from base
 */
#define  ATA_KAUAI_ALTOFFSET    (ATA_KAUAI_REGOFFSET + 0x160)

/*
 * Define the gap between registers
 */
#define ATA_KAUAI_REGGAP        16

/*
 * Define the kauai pci bus attachment.
 */
static  int  ata_kauai_probe(device_t dev);
static  void ata_kauai_setmode(device_t parent, device_t dev);

static device_method_t ata_kauai_methods[] = {
        /* Device interface */
	DEVMETHOD(device_probe,		ata_kauai_probe),
	DEVMETHOD(device_attach,	ata_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	/* ATA interface */
	DEVMETHOD(ata_setmode,		ata_kauai_setmode),
	{ 0, 0 }
};

static driver_t ata_kauai_driver = {
	"ata",
	ata_kauai_methods,
	sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, pci, ata_kauai_driver, ata_devclass, 0, 0);
MODULE_DEPEND(ata, ata, 1, 1, 1);

/*
 * PCI ID search table
 */
static struct kauai_pci_dev {
        u_int32_t  kpd_devid;
        char    *kpd_desc;
} kauai_pci_devlist[] = {
        { 0x0033106b, "Uninorth2 Kauai ATA Controller" },
        { 0x003b106b, "Intrepid Kauai ATA Controller" },
        { 0x0043106b, "K2 Kauai ATA Controller" },
        { 0, NULL }
};

static int
ata_kauai_probe(device_t dev)
{
	struct ata_channel *ch;
	struct resource *mem;
	u_long startp, countp;
	u_int32_t devid;
	int i, found, rid, status;

	found = 0;
	devid = pci_get_devid(dev);
        for (i = 0; kauai_pci_devlist[i].kpd_desc != NULL; i++) {
                if (devid == kauai_pci_devlist[i].kpd_devid) {
			found = 1;
                        device_set_desc(dev, kauai_pci_devlist[i].kpd_desc);
		}
	}

	if (!found)
		return (ENXIO);

	/*
	 * This device seems to ignore writes to the interrupt
	 * config register, resulting in interrupt resources
	 * not being attached. If this is the case, use
	 * Open Firmware to determine the irq, and then attach
	 * the resource. This allows the ATA common code to
	 * allocate the irq.
	 */
	status = bus_get_resource(dev, SYS_RES_IRQ, 0, &startp, &countp);
	if (status == ENOENT) {
		int irq;

		/*
		 * Aargh, hideous hack until ofw pci intr routine is
		 * exported
		 */
		irq = 39; /* XXX */
		bus_set_resource(dev, SYS_RES_IRQ, 0, irq, 1);

		/*
		 * Sanity check...
		 */
		status = bus_get_resource(dev, SYS_RES_IRQ, 0, &startp,
		    &countp);
		if (status == ENOENT ||
		    startp != 39) {
			printf("kauai irq not set!\n");
			return (ENXIO);
		}
	}

	ch = device_get_softc(dev);
	bzero(ch, sizeof(struct ata_channel));

        rid = PCIR_BARS;
	mem = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
        if (mem == NULL) {
                device_printf(dev, "could not allocate memory\n");
                return (ENXIO);
        }

	/*
	 * Set up the resource vectors
	 */
        for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
                ch->r_io[i].res = mem;
                ch->r_io[i].offset = i*ATA_KAUAI_REGGAP + ATA_KAUAI_REGOFFSET;
        }
        ch->r_io[ATA_CONTROL].res = mem;
        ch->r_io[ATA_CONTROL].offset = ATA_KAUAI_ALTOFFSET;
	ata_default_registers(dev);

        ch->unit = 0;
        ch->flags |= ATA_USE_16BIT;
	ata_generic_hw(dev);

        return (ata_probe(dev));
}

static void
ata_kauai_setmode(device_t parent, device_t dev)
{
	struct ata_device *atadev = device_get_softc(dev);

	/* TODO bang kauai speed register */
	atadev->mode = ATA_PIO;
}
