/*-
 * Copyright (c) 2002 Søren Schmidt <sos@FreeBSD.org>
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

#include "opt_ata.h"
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/malloc.h>
#include <machine/resource.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <isa/isavar.h>
#include <dev/ata/ata-all.h>

/* local vars */
static bus_addr_t ata_pc98_ports[] = {
    0x0, 0x2, 0x4, 0x6, 0x8, 0xa, 0xc, 0xe
};

struct ata_cbus_controller {
    struct resource *io;
    struct resource *altio;
    struct resource *bankio;
    struct resource *irq;
    int current_bank;
};

static int
ata_cbus_probe(device_t dev)
{
    struct resource *io;
    int rid;
    u_long tmp;

    /* allocate the ioport range */
    rid = ATA_IOADDR_RID;
    io = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, ata_pc98_ports,
			     ATA_IOSIZE, RF_ACTIVE);
    if (!io)
	return ENOMEM;
    isa_load_resourcev(io, ata_pc98_ports, ATA_IOSIZE);

    /* calculate & set the altport range */
    rid = ATA_PC98_ALTADDR_RID;
    if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &tmp, &tmp)) {
	bus_set_resource(dev, SYS_RES_IOPORT, rid,
			 rman_get_start(io)+ATA_PC98_ALTOFFSET, ATA_ALTIOSIZE);
    }

    /* calculate & set the bank range */
    rid = ATA_PC98_BANKADDR_RID;
    if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &tmp, &tmp)) {
	bus_set_resource(dev, SYS_RES_IOPORT, rid,
			 ATA_PC98_BANK, ATA_PC98_BANKIOSIZE);
    }

    bus_release_resource(dev, SYS_RES_IOPORT, ATA_IOADDR_RID, io);
    return 0;
}

static int
ata_cbus_attach(device_t dev)
{
    struct ata_cbus_controller *scp = device_get_softc(dev);
    int rid;

    /* allocate resources */
    rid = ATA_IOADDR_RID;
    scp->io = isa_alloc_resourcev(dev, SYS_RES_IOPORT, &rid, ata_pc98_ports,
				  ATA_IOSIZE, RF_ACTIVE);
    if (!scp->io)
       return ENOMEM;

    isa_load_resourcev(scp->io, ata_pc98_ports, ATA_IOSIZE);

    rid = ATA_PC98_ALTADDR_RID;
    scp->altio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				    rman_get_start(scp->io)+ATA_PC98_ALTOFFSET,
				    ~0, ATA_ALTIOSIZE, RF_ACTIVE);
    if (!scp->altio)
	return ENOMEM;

    rid = ATA_PC98_BANKADDR_RID;
    scp->bankio = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				     ATA_PC98_BANK, ~0,
				     ATA_PC98_BANKIOSIZE, RF_ACTIVE);
    if (!scp->bankio)
	return ENOMEM;

    rid = 0;
    scp->irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				  0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);

    scp->current_bank = -1;
    if (!device_add_child(dev, "ata", 0))
	return ENOMEM;
    if (!device_add_child(dev, "ata", 1))
	return ENOMEM;

    return bus_generic_attach(dev);
}

static struct resource *
ata_cbus_alloc_resource(device_t dev, device_t child, int type, int *rid,
			u_long start, u_long end, u_long count, u_int flags)
{
    struct ata_cbus_controller *scp = device_get_softc(dev);

    if (type == SYS_RES_IOPORT) {
	switch (*rid) {
	case ATA_IOADDR_RID:
	    return scp->io;
	case ATA_ALTADDR_RID:
	    return scp->altio;
	}
    }
    if (type == SYS_RES_IRQ) {
	return scp->irq;
    }
    return 0;
}

static int
ata_cbus_setup_intr(device_t dev, device_t child, struct resource *irq,
	       int flags, driver_intr_t *intr, void *arg,
	       void **cookiep)
{
    return BUS_SETUP_INTR(device_get_parent(dev), dev, irq,
			  flags, intr, arg, cookiep);
}

static int
ata_cbus_print_child(device_t dev, device_t child)
{
    struct ata_channel *ch = device_get_softc(child);
    int retval = 0;

    retval += bus_print_child_header(dev, child);
    retval += printf(" at bank %d", ch->unit);
    retval += bus_print_child_footer(dev, child);
    return retval;
}

static int
ata_cbus_intr(struct ata_channel *ch)
{  
    struct ata_cbus_controller *scp =
	device_get_softc(device_get_parent(ch->dev));

    return (ch->unit == scp->current_bank);
}

static void
ata_cbus_banking(struct ata_channel *ch, int flags)
{
    struct ata_cbus_controller *scp =
	device_get_softc(device_get_parent(ch->dev));

    switch (flags) {
    case ATA_LF_LOCK:
	if (scp->current_bank == ch->unit)
	    break;
	while (!atomic_cmpset_acq_int(&scp->current_bank, -1, ch->unit))
	    tsleep((caddr_t)ch->lock_func, PRIBIO, "atalck", 1);
	bus_space_write_1(rman_get_bustag(scp->bankio),
			  rman_get_bushandle(scp->bankio), 0, ch->unit);
	break;

    case ATA_LF_UNLOCK:
	if (scp->current_bank == -1 || scp->current_bank != ch->unit)
	    break;
	atomic_store_rel_int(&scp->current_bank, -1);
	break;
    }
    return;
}

static device_method_t ata_cbus_methods[] = {
    /* device_interface */
    DEVMETHOD(device_probe,	ata_cbus_probe),
    DEVMETHOD(device_attach,	ata_cbus_attach),

    /* bus methods */
    DEVMETHOD(bus_alloc_resource,	ata_cbus_alloc_resource),
    DEVMETHOD(bus_setup_intr,		ata_cbus_setup_intr),
    DEVMETHOD(bus_print_child,		ata_cbus_print_child),
    { 0, 0 }
};

static driver_t ata_cbus_driver = {
    "atacbus",
    ata_cbus_methods,
    sizeof(struct ata_cbus_controller),
};

static devclass_t ata_cbus_devclass;

DRIVER_MODULE(atacbus, isa, ata_cbus_driver, ata_cbus_devclass, 0, 0);

static int
ata_cbussub_probe(device_t dev)
{
    struct ata_channel *ch = device_get_softc(dev);
    device_t *children;
    int count, i;

    /* find channel number on this controller */
    device_get_children(device_get_parent(dev), &children, &count);
    for (i = 0; i < count; i++) {
	if (children[i] == dev) 
	    ch->unit = i;
    }
    free(children, M_TEMP);
    ch->flags |= ATA_USE_16BIT | ATA_USE_PC98GEOM;
    ch->intr_func = ata_cbus_intr;
    ch->lock_func = ata_cbus_banking;
    return ata_probe(dev);
}

static device_method_t ata_cbussub_methods[] = {
    /* device interface */
    DEVMETHOD(device_probe,	ata_cbussub_probe),
    DEVMETHOD(device_attach,	ata_attach),
    DEVMETHOD(device_detach,	ata_detach),
    DEVMETHOD(device_resume,	ata_resume),
    { 0, 0 }
};

static driver_t ata_cbussub_driver = {
    "ata",
    ata_cbussub_methods,
    sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, atacbus, ata_cbussub_driver, ata_devclass, 0, 0);
