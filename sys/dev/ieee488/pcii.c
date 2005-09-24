/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
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
 *
 * Driver for GPIB cards based on NEC µPD7210 and compatibles.
 *
 * This driver just hooks up to the hardware and leaves all the interesting
 * stuff to upd7210.c.
 *
 * Supported hardware:
 *    PCIIA compatible cards.
 *
 *    Tested and known working:
 *	"B&C Microsystems PC488A-0"
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <isa/isavar.h>

#define UPD7210_HW_DRIVER
#include <dev/ieee488/upd7210.h>

struct pcii_softc {
	int foo;
	struct resource	*res[2];
	struct resource	*dma;
	void *intr_handler;
	struct upd7210	upd7210;
};

static devclass_t pcii_devclass;

static int	pcii_probe(device_t dev);
static int	pcii_attach(device_t dev);

static device_method_t pcii_methods[] = {
	DEVMETHOD(device_probe,		pcii_probe),
	DEVMETHOD(device_attach,	pcii_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),

	{ 0, 0 }
};

static struct resource_spec pcii_res_spec[] = {
	{ SYS_RES_IRQ,		0, RF_ACTIVE | RF_SHAREABLE},
	{ SYS_RES_DRQ,		0, RF_ACTIVE | RF_SHAREABLE | RF_OPTIONAL},
	{ SYS_RES_IOPORT,	0, RF_ACTIVE},
	{ -1, 0, 0 }
};

static driver_t pcii_driver = {
	"pcii",
	pcii_methods,
	sizeof(struct pcii_softc),
};

static int
pcii_probe(device_t dev)
{
	int rid, i, j;
	u_long start, count;
	int error = 0;
	struct pcii_softc *sc;

	device_set_desc(dev, "PCII IEEE-4888 controller");
	sc = device_get_softc(dev);

	rid = 0;
	if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &start, &count) != 0)
		return ENXIO;
	if ((start & 0x3ff) != 0x2e1)
		return (ENXIO);
	count = 1;
	if (bus_set_resource(dev, SYS_RES_IOPORT, rid, start, count) != 0)
		return ENXIO;
	error = bus_alloc_resources(dev, pcii_res_spec, sc->res);
	if (error)
		return (error);
	error = ENXIO;
	for (i = 0; i < 8; i++) {
		j = bus_read_1(sc->res[2], i * 0x400);
		if (j != 0x00 && j != 0xff)
			error = 0;
	}
	if (!error) {
		bus_write_1(sc->res[2], 3 * 0x400, 0x55);
		if (bus_read_1(sc->res[2], 3 * 0x400) != 0x55)
			error = ENXIO;
	}
	if (!error) {
		bus_write_1(sc->res[2], 3 * 0x400, 0xaa);
		if (bus_read_1(sc->res[2], 3 * 0x400) != 0xaa)
			error = ENXIO;
	}
	bus_release_resources(dev, pcii_res_spec, sc->res);
	return (error);
}

static int
pcii_attach(device_t dev)
{
	struct pcii_softc *sc;
	int		unit;
	int		rid;
	int		error = 0;

	unit = device_get_unit(dev);
	sc = device_get_softc(dev);
	memset(sc, 0, sizeof *sc);

	device_set_desc(dev, "PCII IEEE-4888 controller");

	error = bus_alloc_resources(dev, pcii_res_spec, sc->res);
	if (error)
		return (error);

	error = bus_setup_intr(dev, sc->res[0],
	    INTR_TYPE_MISC | INTR_MPSAFE,
	    upd7210intr, &sc->upd7210, &sc->intr_handler);
	if (error) {
		bus_release_resources(dev, pcii_res_spec, sc->res);
		return (error);
	}

	for (rid = 0; rid < 8; rid++) {
		sc->upd7210.reg_res[rid] = sc->res[2];
		sc->upd7210.reg_offset[rid] = 0x400 * rid;
	}

	if (sc->res[1] == NULL)
		sc->upd7210.dmachan = -1;
	else
		sc->upd7210.dmachan = rman_get_start(sc->res[1]);

	upd7210attach(&sc->upd7210);
	return (error);
}

DRIVER_MODULE(pcii, isa, pcii_driver, pcii_devclass, 0, 0);
DRIVER_MODULE(pcii, acpi, pcii_driver, pcii_devclass, 0, 0);
