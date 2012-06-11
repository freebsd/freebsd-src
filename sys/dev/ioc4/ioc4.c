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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>

struct ioc4_softc {
	device_t	sc_dev;

	struct resource	*sc_mres;
	int		sc_mrid;

	int		sc_irid;
	struct resource	*sc_ires;
	void		*sc_icookie;

	u_int		sc_fastintr:1;
};

static int ioc4_probe(device_t dev);
static int ioc4_attach(device_t dev);
static int ioc4_detach(device_t dev);

static char ioc4_name[] = "ioc";
static devclass_t ioc4_devclass;

static device_method_t ioc4_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ioc4_probe),
	DEVMETHOD(device_attach,	ioc4_attach),
	DEVMETHOD(device_detach,	ioc4_detach),
	{ 0, 0 }
};

static driver_t ioc4_driver = {
	ioc4_name,
	ioc4_methods,
	sizeof(struct ioc4_softc),
};

static int
ioc4_intr(void *arg)
{
	struct ioc4_softc *sc = arg;

	device_printf(sc->sc_dev, "%s\n", __func__);
	return (FILTER_HANDLED);
}

static int
ioc4_probe(device_t dev)
{

	if (pci_get_vendor(dev) != 0x10a9)
		return (ENXIO);
	if (pci_get_device(dev) != 0x100a)
		return (ENXIO);

	device_set_desc(dev, "SGI IOC4 I/O controller");
	return (BUS_PROBE_DEFAULT);
}

static int
ioc4_attach(device_t dev)
{
	struct ioc4_softc *sc;
	int error;

	sc = device_get_softc(dev);
	sc->sc_dev = dev;

	sc->sc_mrid = PCIR_BAR(0);
	sc->sc_mres = bus_alloc_resource_any(sc->sc_dev, SYS_RES_MEMORY,
	    &sc->sc_mrid, RF_ACTIVE);
	if (sc->sc_mres == NULL)
		return (ENXIO);

	sc->sc_irid = 0;
	sc->sc_ires = bus_alloc_resource_any(sc->sc_dev, SYS_RES_IRQ,
	    &sc->sc_irid, RF_ACTIVE|RF_SHAREABLE);
	if (sc->sc_ires == NULL) {
		bus_release_resource(sc->sc_dev, SYS_RES_MEMORY, sc->sc_mrid,
		    sc->sc_mres);
		return (ENXIO);
	}

	error = bus_setup_intr(dev, sc->sc_ires, INTR_TYPE_TTY, ioc4_intr,
	    NULL, sc, &sc->sc_icookie);
	if (error)
		error = bus_setup_intr(dev, sc->sc_ires,
		    INTR_TYPE_TTY | INTR_MPSAFE, NULL,
		    (driver_intr_t *)ioc4_intr, sc, &sc->sc_icookie);
	else
		sc->sc_fastintr = 1;

	if (error) {
		bus_release_resource(sc->sc_dev, SYS_RES_IRQ, sc->sc_irid,
		   sc->sc_ires);
		bus_release_resource(sc->sc_dev, SYS_RES_MEMORY, sc->sc_mrid,
		   sc->sc_mres);
		return (error);
	}

	/*
	 * Create, probe and attach children
	 */

	return (0);
}

static int
ioc4_detach(device_t dev)
{
	struct ioc4_softc *sc;

	sc = device_get_softc(dev);

	/*
	 * Detach children and destroy children
	 */

	bus_teardown_intr(sc->sc_dev, sc->sc_ires, sc->sc_icookie);
	bus_release_resource(sc->sc_dev, SYS_RES_IRQ, sc->sc_irid,
	    sc->sc_ires);
	bus_release_resource(sc->sc_dev, SYS_RES_MEMORY, sc->sc_mrid,
	    sc->sc_mres);
	return (0);
}

DRIVER_MODULE(ioc4, pci, ioc4_driver, ioc4_devclass, 0, 0);
