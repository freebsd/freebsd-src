/*-
 * Copyright (c) 2012-2013 Robert N. M. Watson
 * Copyright (c) 2014-2015 Ed Maste
 * All rights reserved.
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory under DARPA/AFRL contract (FA8750-10-C-0237)
 * ("CTSRD"), as part of the DARPA CRASH research programme.
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/fdt/fdt_clock.h>
#include <dev/fdt/fdt_common.h>
#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/altera/altpll/altpll.h>

#include "fb_if.h"

static int
altpll_fdt_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "sri-cambridge,altpll")) {
		device_set_desc(dev, "Altera reconfigurable PLL");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
altpll_fdt_attach(device_t dev)
{
	struct altpll_softc *sc;
	phandle_t node;
	pcell_t freq;
	int error;

	sc = device_get_softc(dev);
	sc->ap_dev = dev;
	sc->ap_unit = device_get_unit(dev);

	/*
	 * Altpll's FDT entry has a single memory resource for its control
	 * registers.
	 */
	sc->ap_reg_rid = 0;
	sc->ap_reg_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->ap_reg_rid, RF_ACTIVE);
	if (sc->ap_reg_res == NULL) {
		device_printf(dev, "couldn't map register memory\n");
		error = ENXIO;
		goto error;
	}
	if (rman_get_start(sc->ap_reg_res) % PAGE_SIZE != 0) {
		device_printf(dev, "improper register address\n");
		error = ENXIO;
		goto error;
	}
	device_printf(sc->ap_dev, "registers at mem %p-%p\n",
	    (void *)rman_get_start(sc->ap_reg_res),
	    (void *)(rman_get_start(sc->ap_reg_res) +
	      rman_get_size(sc->ap_reg_res)));
	if ((node = ofw_bus_get_node(dev)) == -1) {
		error = ENXIO;
		goto error;
	}
	OF_getencprop(node, "clock-frequency", &freq, sizeof(freq));
	if (freq > 0)
		sc->ap_base_frequency = freq;
	else
		sc->ap_base_frequency = ALTPLL_DEFAULT_FREQUENCY;

	if ((error = altpll_attach(sc)) != 0)
		goto error;
	fdt_clock_register_provider(dev);
	return (0);

error:
	if (sc->ap_reg_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->ap_reg_rid,
		    sc->ap_reg_res);
	return (error);
}

static int
altpll_fdt_detach(device_t dev)
{
	struct altpll_softc *sc;

	sc = device_get_softc(dev);
	altpll_detach(sc);
	bus_release_resource(dev, SYS_RES_MEMORY, sc->ap_reg_rid,
	    sc->ap_reg_res);
	return (0);
}

static device_method_t altpll_fdt_methods[] = {
	DEVMETHOD(device_probe,		altpll_fdt_probe),
	DEVMETHOD(device_attach,	altpll_fdt_attach),
	DEVMETHOD(device_detach,	altpll_fdt_detach),
	DEVMETHOD(fdt_clock_set_frequency,	altpll_set_frequency),
	{ 0, 0 }
};

static driver_t altpll_fdt_driver = {
	"altpll",
	altpll_fdt_methods,
	sizeof(struct altpll_softc),
};

DRIVER_MODULE(altpll, simplebus, altpll_fdt_driver,
    altpll_devclass, 0, 0);
