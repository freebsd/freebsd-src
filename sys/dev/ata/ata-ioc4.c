/*-
 * Copyright (c) 2012 Marcel Moolenaar
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_ata.h"

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

#include <dev/ioc4/ioc4_bus.h>

#include "ata_if.h"

static int ata_ioc4_probe(device_t);
static int ata_ioc4_attach(device_t);
static int ata_ioc4_detach(device_t);

static device_method_t ata_ioc4_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		ata_ioc4_probe),
	DEVMETHOD(device_attach,	ata_ioc4_attach),
	DEVMETHOD(device_detach,	ata_ioc4_detach),
	DEVMETHOD_END
};

static driver_t ata_ioc4_driver = {
	"ata",
	ata_ioc4_methods,
	sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, ioc4, ata_ioc4_driver, ata_devclass, NULL, NULL);
MODULE_DEPEND(ata, ata, 1, 1, 1);

static int
ata_ioc4_probe(device_t dev)
{
	device_t parent;
	uintptr_t type;

	parent = device_get_parent(dev);
	if (BUS_READ_IVAR(parent, dev, IOC4_IVAR_TYPE, &type))
		return (ENXIO);
	if (type != IOC4_TYPE_ATA)
		return (ENXIO);

	device_set_desc(dev, "SGI IOC4 ATA channel");
	return (ata_probe(dev));
}

static int
ata_ioc4_attach(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);
	struct resource *mres;
	int i, rid;

	if (ch->attached)
		return (0);
	ch->attached = 1;

	rid = 0;
	mres = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (mres == NULL)
		return (ENXIO);

	for (i = ATA_DATA; i <= ATA_COMMAND; i++) {
		ch->r_io[i].res = mres;
		ch->r_io[i].offset = i << 2;
	}
	ch->r_io[ATA_CONTROL].res = mres;
	ch->r_io[ATA_CONTROL].offset = 32;
	ch->r_io[ATA_IDX_ADDR].res = mres;
	ata_default_registers(dev);

	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT | ATA_NO_ATAPI_DMA;
	ata_generic_hw(dev);
	return (ata_attach(dev));
}

int
ata_ioc4_detach(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);
	int error;

	if (!ch->attached)
		return (0);
	ch->attached = 0;

	error = ata_detach(dev);

	bus_release_resource(dev, SYS_RES_MEMORY, 0, ch->r_io[ATA_CONTROL].res);
	return (error);
}
