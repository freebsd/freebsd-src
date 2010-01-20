/*-
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 *
 * Copyright (C) 2000, Peter Wemm <peter@netplex.com.au>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/si/sireg.h>
#include <dev/si/sivar.h>

#include <dev/eisa/eisaconf.h>

static int
si_eisa_probe(device_t dev)
{
	u_long iobase;
	u_long maddr;
	int irq;

	if (eisa_get_id(dev) != SIEISADEVID)
		return ENXIO;

	device_set_desc(dev, "Specialix SI/XIO EISA host card");
	
	iobase = (eisa_get_slot(dev) * EISA_SLOT_SIZE) + SIEISABASE;
	eisa_add_iospace(dev, iobase, SIEISAIOSIZE, RESVADDR_NONE);

	maddr = (inb(iobase+1) << 24) | (inb(iobase) << 16);
	eisa_add_mspace(dev, maddr, SIEISA_MEMSIZE, RESVADDR_NONE);

	irq  = ((inb(iobase+2) >> 4) & 0xf);
	eisa_add_intr(dev, irq, EISA_TRIGGER_LEVEL);	/* XXX shared? */

	return (0);
}

static int
si_eisa_attach(device_t dev)
{
	struct si_softc *sc;
	void *ih;
	int error;

	error = 0;
	ih = NULL;
	sc = device_get_softc(dev);

	sc->sc_type = SIEISA;

	sc->sc_port_rid = 0;
	sc->sc_port_res = bus_alloc_resource_any(dev, SYS_RES_IOPORT,
						 &sc->sc_port_rid, RF_ACTIVE);
	if (!sc->sc_port_res) {
		device_printf(dev, "couldn't allocate ioports\n");
		goto fail;
	}
	sc->sc_iobase = rman_get_start(sc->sc_port_res);

	sc->sc_mem_rid = 0;
	sc->sc_mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
						&sc->sc_mem_rid, RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "couldn't allocate iomemory");
		goto fail;
	}
	sc->sc_paddr = (caddr_t)rman_get_start(sc->sc_mem_res);
	sc->sc_maddr = rman_get_virtual(sc->sc_mem_res);

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource_any(dev, SYS_RES_IRQ, 
						&sc->sc_irq_rid,
						RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "couldn't allocate interrupt");
		goto fail;
	}
	sc->sc_irq = rman_get_start(sc->sc_irq_res);
	error = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_TTY,
			       NULL, si_intr, sc,&ih);
	if (error) {
		device_printf(dev, "couldn't activate interrupt");
		goto fail;
	}

	error = siattach(dev);
	if (error)
		goto fail;
	return (0);		/* success */

fail:
	if (error == 0)
		error = ENXIO;
	if (sc->sc_irq_res) {
		if (ih)
			bus_teardown_intr(dev, sc->sc_irq_res, ih);
		bus_release_resource(dev, SYS_RES_IRQ,
				     sc->sc_irq_rid, sc->sc_irq_res);
		sc->sc_irq_res = 0;
	}
	if (sc->sc_mem_res) {
		bus_release_resource(dev, SYS_RES_MEMORY,
				     sc->sc_mem_rid, sc->sc_mem_res);
		sc->sc_mem_res = 0;
	}
	if (sc->sc_port_res) {
		bus_release_resource(dev, SYS_RES_IOPORT,
				     sc->sc_port_rid, sc->sc_port_res);
		sc->sc_port_res = 0;
	}
	return (error);
}

static device_method_t si_eisa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		si_eisa_probe),
	DEVMETHOD(device_attach,	si_eisa_attach),

	{ 0, 0 }
};

static driver_t si_eisa_driver = {
	"si",
	si_eisa_methods,
	sizeof(struct si_softc),
};

DRIVER_MODULE(si, eisa, si_eisa_driver, si_devclass, 0, 0);
