/*
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/si/sireg.h>
#include <dev/si/sivar.h>

#include <dev/pci/pcivar.h>

static int
si_pci_probe(device_t dev)
{
	const char *desc = NULL;

	switch (pci_get_devid(dev)) {
	case 0x400011cb:
		desc = "Specialix SI/XIO PCI host card";
		break;
	case 0x200011cb:
		if (pci_read_config(dev, SIJETSSIDREG, 4) == 0x020011cb)
			desc = "Specialix SX PCI host card";
		break;
	}
	if (desc) {
		device_set_desc(dev, desc);
		return 0;
	}
	return ENXIO;
}

static int
si_pci_attach(device_t dev)
{
	struct si_softc *sc;
	void *ih;
	int error;

	error = 0;
	ih = NULL;
	sc = device_get_softc(dev);

	switch (pci_get_devid(dev)) {
	case 0x400011cb:
		sc->sc_type = SIPCI;
		sc->sc_mem_rid = SIPCIBADR;
		break;
	case 0x200011cb:
		sc->sc_type = SIJETPCI;
		sc->sc_mem_rid = SIJETBADR;
		break;
	}

	sc->sc_mem_res = bus_alloc_resource(dev, SYS_RES_MEMORY,
					    &sc->sc_mem_rid,
					    0, ~0, 1, RF_ACTIVE);
	if (!sc->sc_mem_res) {
		device_printf(dev, "couldn't map memory\n");
		goto fail;
	}
	sc->sc_paddr = (caddr_t)rman_get_start(sc->sc_mem_res);
	sc->sc_maddr = rman_get_virtual(sc->sc_mem_res);

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_irq_rid,
					    0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "couldn't map interrupt\n");
		goto fail;
	}
	sc->sc_irq = rman_get_start(sc->sc_irq_res);
	error = bus_setup_intr(dev, sc->sc_irq_res, INTR_TYPE_TTY,
			       si_intr, sc, &ih);
	if (error) {
		device_printf(dev, "could not activate interrupt\n");
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
	return (error);
}

static device_method_t si_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		si_pci_probe),
	DEVMETHOD(device_attach,	si_pci_attach),

	{ 0, 0 }
};

static driver_t si_pci_driver = {
	"si",
	si_pci_methods,
	sizeof(struct si_softc),
};

DRIVER_MODULE(si, pci, si_pci_driver, si_devclass, 0, 0);
