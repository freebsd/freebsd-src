/*-
 * Device driver for Specialix I/O8+ multiport serial card.
 *
 * Copyright 2003 Frank Mayhar <frank@exit.com>
 *
 * Derived from the "si" driver by Peter Wemm <peter@netplex.com.au>, using
 * lots of information from the Linux "specialix" driver by Roger Wolff
 * <R.E.Wolff@BitWizard.nl> and from the Intel CD1865 "Intelligent Eight-
 * Channel Communications Controller" datasheet.  Roger was also nice
 * enough to answer numerous questions about stuff specific to the I/O8+
 * not covered by the CD1865 datasheet.
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
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/tty.h>
#include <machine/resource.h>   
#include <machine/bus.h>
#include <machine/clock.h>
#include <sys/rman.h>

#include <dev/sx/cd1865.h>
#include <dev/sx/sxvar.h>
#include <dev/sx/sx.h>
#include <dev/sx/sx_util.h>

#include <dev/pci/pcivar.h>

static int
sx_pci_probe(
	device_t dev)
{
	const char *desc = NULL;

	switch (pci_get_devid(dev)) {
		case 0x200011cb:
			if (pci_get_subdevice(dev) == (uint16_t)0xb008) {
				desc = "Specialix I/O8+ Multiport Serial Card";
			}
			break;
	}
	if (desc) {
		device_set_desc(dev, desc);
		return 0;
	}
	return ENXIO;
}

static int
sx_pci_attach(device_t dev)
{
	struct sx_softc *sc;
	void *ih;
	int error;

	error = 0;
	ih = NULL;
	sc = device_get_softc(dev);

	sc->sc_io_rid = 0x18;
	sc->sc_io_res = bus_alloc_resource(dev,
					   SYS_RES_IOPORT,
					   &sc->sc_io_rid,
					   0, ~0, 1,
					   RF_ACTIVE);
	if (!sc->sc_io_res) {
		device_printf(dev, "can't map I/O\n");
		goto fail;
	}
	sc->sc_st = rman_get_bustag(sc->sc_io_res);
	sc->sc_sh = rman_get_bushandle(sc->sc_io_res);

	/*
	 * Now that we have the bus handle, we can make certain that this
	 * is an I/O8+.
	 */
	if (sx_probe_io8(dev)) {
		device_printf(dev, "Oops!  Device is not an I/O8+ board!\n");
		goto fail;
	}

	/*sc->sc_paddr = (caddr_t)rman_get_start(sc->sc_io_res);*/
	/*sc->sc_maddr = rman_get_virtual(sc->sc_io_res);*/

	sc->sc_irq_rid = 0;
	sc->sc_irq_res = bus_alloc_resource(dev,
					    SYS_RES_IRQ,
					    &sc->sc_irq_rid,
					    0, ~0, 1,
					    RF_ACTIVE | RF_SHAREABLE);
	if (!sc->sc_irq_res) {
		device_printf(dev, "Can't map interrupt\n");
		goto fail;
	}
	sc->sc_irq = rman_get_start(sc->sc_irq_res);
	error = bus_setup_intr(dev,
			       sc->sc_irq_res,
			       INTR_TYPE_TTY,
			       sx_intr,
			       sc, &ih);
	if (error) {
		device_printf(dev, "Can't activate interrupt\n");
		goto fail;
	}

	error = sxattach(dev);
	if (error)
		goto fail;
	return (0);		/* success */

fail:
	if (error == 0)
		error = ENXIO;
	if (sc->sc_irq_res) {
		if (ih)
			bus_teardown_intr(dev, sc->sc_irq_res, ih);
		bus_release_resource(dev,
				     SYS_RES_IRQ,
				     sc->sc_irq_rid,
				     sc->sc_irq_res);
		sc->sc_irq_res = 0;
	}
	if (sc->sc_io_res) {
		bus_release_resource(dev,
				     SYS_RES_IOPORT,
				     sc->sc_io_rid,
				     sc->sc_io_res);
		sc->sc_io_res = 0;
	}
	return(error);
}

static device_method_t sx_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		sx_pci_probe),
	DEVMETHOD(device_attach,	sx_pci_attach),
/*	DEVMETHOD(device_detach,	sx_pci_detach),*/

	{ 0, 0 }
};

static driver_t sx_pci_driver = {
	"sx",
	sx_pci_methods,
	sizeof(struct sx_softc),
};

DRIVER_MODULE(sx, pci, sx_pci_driver, sx_devclass, 0, 0);
