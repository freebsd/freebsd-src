/*-
 * Copyright (c) 1998 - 2005 Søren Schmidt <sos@FreeBSD.org>
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ata.h>
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
#include <dev/ata/ata-all.h>
#include <dev/pccard/pccard_cis.h>
#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>
#include <ata_if.h>

#include "pccarddevs.h"

static const struct pccard_product ata_pccard_products[] = {
	PCMCIA_CARD(FREECOM, PCCARDIDE, 0),
	PCMCIA_CARD(EXP, EXPMULTIMEDIA, 0),
	PCMCIA_CARD(IODATA3, CBIDE2, 0),
	PCMCIA_CARD(OEM2, CDROM1, 0),
	PCMCIA_CARD(OEM2, IDE, 0),
	PCMCIA_CARD(PANASONIC, KXLC005, 0),
	PCMCIA_CARD(TEAC, IDECARDII, 0),
	{NULL}
};

static int
ata_pccard_match(device_t dev)
{
    const struct pccard_product *pp;
    u_int32_t fcn = PCCARD_FUNCTION_UNSPEC;
    int error = 0;

    error = pccard_get_function(dev, &fcn);
    if (error != 0)
	return (error);

    /* if it says its a disk we should register it */
    if (fcn == PCCARD_FUNCTION_DISK)
	return (0);

    /* match other devices here, primarily cdrom/dvd rom */
    if ((pp = pccard_product_lookup(dev, ata_pccard_products,
				    sizeof(ata_pccard_products[0]), NULL))) {
	if (pp->pp_name)
	    device_set_desc(dev, pp->pp_name);
	return (0);
    }
    return(ENXIO);
}

static int
ata_pccard_probe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    struct resource *io, *ctlio;
    int i, rid;

    /* allocate the io range to get start and length */
    rid = ATA_IOADDR_RID;
    io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
			    ATA_IOSIZE, RF_ACTIVE);
    if (!io)
	return ENXIO;

    /* setup the resource vectors */
    for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
	ch->r_io[i].res = io;
	ch->r_io[i].offset = i;
    }
    ch->r_io[ATA_IDX_ADDR].res = io;

    /*
     * if we got more than the default ATA_IOSIZE ports, this is a device
     * where ctlio is located at offset 14 into "normal" io space.
     */
    if (rman_get_size(io) > ATA_IOSIZE) {
	ch->r_io[ATA_CONTROL].res = io;
	ch->r_io[ATA_CONTROL].offset = 14;
    }
    else {
	rid = ATA_CTLADDR_RID;
	ctlio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0,
				   ATA_CTLIOSIZE, RF_ACTIVE);
	if (!ctlio) {
	    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
	    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
		ch->r_io[i].res = NULL;
	    return ENXIO;
	}
	ch->r_io[ATA_CONTROL].res = ctlio;
	ch->r_io[ATA_CONTROL].offset = 0;
    }
    ata_default_registers(ch);

    /* initialize softc for this channel */
    ch->unit = 0;
    ch->flags |= (ATA_USE_16BIT | ATA_NO_SLAVE);
    ata_generic_hw(ch);
    return ata_probe(dev);
}

static int
ata_pccard_detach(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    int i;

    ata_detach(dev);
    if (ch->r_io[ATA_CONTROL].res != ch->r_io[ATA_DATA].res)
	bus_release_resource(dev, SYS_RES_IOPORT, ATA_CTLADDR_RID,
			     ch->r_io[ATA_CONTROL].res);
    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID,
			 ch->r_io[ATA_DATA].res);
    for (i = ATA_DATA; i < ATA_MAX_RES; i++)
	ch->r_io[i].res = NULL;
    return 0;
}

static void
ata_pccard_setmode(device_t parent, device_t dev)
{
    struct ata_device *atadev = device_get_softc(dev);
    int mode = atadev->mode;

    atadev->mode = ata_limit_mode(atadev, mode, ATA_PIO_MAX);
}

static device_method_t ata_pccard_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,             pccard_compat_probe),
    DEVMETHOD(device_attach,            pccard_compat_attach),
    DEVMETHOD(device_detach,            ata_pccard_detach),

    /* card interface */
    DEVMETHOD(card_compat_match,        ata_pccard_match),
    DEVMETHOD(card_compat_probe,        ata_pccard_probe),
    DEVMETHOD(card_compat_attach,       ata_attach),

    /* ATA methods */
    DEVMETHOD(ata_setmode,              ata_pccard_setmode),
    { 0, 0 }
};

static driver_t ata_pccard_driver = {
    "ata",
    ata_pccard_methods,
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, pccard, ata_pccard_driver, ata_devclass, 0, 0);
MODULE_DEPEND(ata, ata, 1, 1, 1);
