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

#include <dev/ieee488/upd7210.h>

struct pcii_softc {
	int foo;
	struct resource	*port[8];
	struct resource	*irq;
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

static driver_t pcii_driver = {
	"pcii",
	pcii_methods,
	sizeof(struct pcii_softc),
};

static int
pcii_probe(device_t dev)
{
	struct resource	*port;
	int rid;
	u_long start, count;
	int i, j, error = 0;

	device_set_desc(dev, "PCII IEEE-4888 controller");

	rid = 0;
	if (bus_get_resource(dev, SYS_RES_IOPORT, rid, &start, &count) != 0)
		return ENXIO;
	if ((start & 0x3ff) != 0x2e1)
		return (ENXIO);
	count = 1;
	if (bus_set_resource(dev, SYS_RES_IOPORT, rid, start, count) != 0)
		return ENXIO;
	for (i = 0; i < 8; i++) {
		j = bus_set_resource(dev, SYS_RES_IOPORT, i,
		    start + 0x400 * i, 1);
		if (j) {
			error = ENXIO;
			break;
		}
		rid = i;
		port = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
		    &rid, RF_ACTIVE);
		if (port == NULL)
			return (ENXIO);
		else
			bus_release_resource(dev, SYS_RES_IOPORT, i, port);
	}

	rid = 0;
	port = bus_alloc_resource_any(dev,
	    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
	if (port == NULL)
		return (ENXIO);
	bus_release_resource(dev, SYS_RES_IRQ, rid, port);
					   
	return (error);
}

static int
pcii_attach(device_t dev)
{
	struct pcii_softc *sc;
	int		unit;
	int		rid;
	int i, error = 0;

	unit = device_get_unit(dev);
	sc = device_get_softc(dev);
	memset(sc, 0, sizeof *sc);

	device_set_desc(dev, "PCII IEEE-4888 controller");

	for (rid = 0; rid < 8; rid++) {
		sc->port[rid] = bus_alloc_resource_any(dev,
		    SYS_RES_IOPORT, &rid, RF_ACTIVE);
		if (sc->port[rid] == NULL) {
			error = ENXIO;
			break;
		}
		sc->upd7210.reg_tag[rid] = rman_get_bustag(sc->port[rid]);
		sc->upd7210.reg_handle[rid] = rman_get_bushandle(sc->port[rid]);
	}
	if (!error) {
		rid = 0;
		sc->irq = bus_alloc_resource_any(dev,
		    SYS_RES_IRQ, &rid, RF_SHAREABLE | RF_ACTIVE);
		if (sc->irq == NULL) {
			error = ENXIO;
		} else {
			error = bus_setup_intr(dev, sc->irq,
			    INTR_TYPE_MISC | INTR_MPSAFE,
			    upd7210intr, &sc->upd7210, &sc->intr_handler);
		}
	}
	if (!error) {
		rid = 0;
		sc->dma = bus_alloc_resource_any(dev,
		    SYS_RES_DRQ, &rid, RF_ACTIVE | RF_SHAREABLE);
		if (sc->dma == NULL)
			sc->upd7210.dmachan = -1;
		else
			sc->upd7210.dmachan = rman_get_start(sc->dma);
	}
	if (error) {
		for (i = 0; i < 8; i++) {
			if (sc->port[i] == NULL)
				break;
			bus_release_resource(dev, SYS_RES_IOPORT,
			    0, sc->port[i]);
		}
		if (sc->intr_handler != NULL)
			bus_teardown_intr(dev, sc->irq, sc->intr_handler);
		if (sc->irq != NULL)
			bus_release_resource(dev, SYS_RES_IRQ, i, sc->irq);
	}
	upd7210attach(&sc->upd7210);
	return (error);
}

DRIVER_MODULE(pcii, isa, pcii_driver, pcii_devclass, 0, 0);
DRIVER_MODULE(pcii, acpi, pcii_driver, pcii_devclass, 0, 0);
