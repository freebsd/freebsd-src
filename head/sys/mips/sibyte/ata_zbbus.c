/*-
 * Copyright (c) 2009 Neelkanth Natu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <vm/uma.h>

#include <sys/ata.h>
#include <dev/ata/ata-all.h>

#include <machine/resource.h>

__FBSDID("$FreeBSD$");

static int
ata_zbbus_probe(device_t dev)
{

	return (ata_probe(dev));
}

static int
ata_zbbus_attach(device_t dev)
{
	int i, rid, regshift, regoffset;
	struct ata_channel *ch;
	struct resource *io;
	
	ch = device_get_softc(dev);

	if (ch->attached)
		return (0);
	ch->attached = 1;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1, RF_ACTIVE);
	if (io == NULL)
		return (ENXIO);

	/*
	 * SWARM needs an address shift of 5 when accessing ATA registers.
	 *
	 * For e.g. an access to register 4 actually needs an address
	 * of (4 << 5) to be output on the generic bus.
	 */
	regshift = 5;
	resource_int_value(device_get_name(dev), device_get_unit(dev),
			   "regshift", &regshift);
	if (regshift && bootverbose)
		device_printf(dev, "using a register shift of %d\n", regshift);

	regoffset = 0x1F0;
	resource_int_value(device_get_name(dev), device_get_unit(dev),
			   "regoffset", &regoffset);
	if (regoffset && bootverbose) {
		device_printf(dev, "using a register offset of 0x%0x\n",
			      regoffset);
	}

	/* setup the ata register addresses */
	for (i = ATA_DATA; i <= ATA_COMMAND; ++i) {
		ch->r_io[i].res = io;
		ch->r_io[i].offset = (regoffset + i) << regshift;
	}

	ch->r_io[ATA_CONTROL].res = io;
	ch->r_io[ATA_CONTROL].offset = (regoffset + ATA_CTLOFFSET) << regshift;
	ch->r_io[ATA_IDX_ADDR].res = io;	/* XXX what is this used for */
	ata_default_registers(dev);

	/* initialize softc for this channel */
	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT;
	ata_generic_hw(dev);

	return (ata_attach(dev));
}

static int
ata_zbbus_detach(device_t dev)
{
	int error;
	struct ata_channel *ch = device_get_softc(dev);

	if (!ch->attached)
		return (0);
	ch->attached = 0;

	error = ata_detach(dev);

	bus_release_resource(dev, SYS_RES_MEMORY, 0,
			     ch->r_io[ATA_IDX_ADDR].res);

	return (error);
}

static int
ata_zbbus_suspend(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);

	if (!ch->attached)
		return (0);

	return (ata_suspend(dev));
}

static int
ata_zbbus_resume(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);

	if (!ch->attached)
		return (0);

	return (ata_resume(dev));
}

static device_method_t ata_zbbus_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ata_zbbus_probe),
	DEVMETHOD(device_attach,	ata_zbbus_attach),
	DEVMETHOD(device_detach,	ata_zbbus_detach),
	DEVMETHOD(device_suspend,	ata_zbbus_suspend),
	DEVMETHOD(device_resume,	ata_zbbus_resume),

	{ 0, 0 }
};

static driver_t ata_zbbus_driver = {
	"ata",
	ata_zbbus_methods,
	sizeof(struct ata_channel)
};

DRIVER_MODULE(ata, zbbus, ata_zbbus_driver, ata_devclass, 0, 0);
