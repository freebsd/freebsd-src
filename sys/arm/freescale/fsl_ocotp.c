/*-
 * Copyright (c) 2014 Steven Lawrance <stl@koffein.net>
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

/*
 * Access to the Freescale i.MX6 On-Chip One-Time-Programmable Memory
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm/freescale/fsl_ocotpreg.h>
#include <arm/freescale/fsl_ocotpvar.h>

struct ocotp_softc {
	device_t	dev;
	struct resource	*mem_res;
};

static struct ocotp_softc *ocotp_sc;

static inline uint32_t
RD4(struct ocotp_softc *sc, bus_size_t off)
{
	return (bus_read_4(sc->mem_res, off));
}


static int
ocotp_detach(device_t dev)
{
	struct ocotp_softc *sc;

	sc = device_get_softc(dev);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	ocotp_sc = NULL;

	return (0);
}

static int
ocotp_attach(device_t dev)
{
	struct ocotp_softc *sc;
	int err, rid;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* Allocate bus_space resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		err = ENXIO;
		goto out;
	}

	ocotp_sc = sc;
	err = 0;

out:
	if (err != 0)
		ocotp_detach(dev);

	return (err);
}

static int
ocotp_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

        if (ofw_bus_is_compatible(dev, "fsl,fslq-ocotp") == 0)
		return (ENXIO);

	device_set_desc(dev, 
	    "Freescale i.MX6 On-Chip One-Time-Programmable Memory");

	return (BUS_PROBE_DEFAULT);
}

uint32_t
fsl_ocotp_read_4(bus_size_t off)
{

	if (ocotp_sc == NULL)
		panic("fsl_ocotp_read_4: softc not set!");

	if (off > FSL_OCOTP_LAST_REG)
		panic("fsl_ocotp_read_4: offset out of range");

	return (RD4(ocotp_sc, off));
}

static device_method_t ocotp_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  ocotp_probe),
	DEVMETHOD(device_attach, ocotp_attach),
	DEVMETHOD(device_detach, ocotp_detach),

	DEVMETHOD_END
};

static driver_t ocotp_driver = {
	"ocotp",
	ocotp_methods,
	sizeof(struct ocotp_softc)
};

static devclass_t ocotp_devclass;

DRIVER_MODULE(ocotp, simplebus, ocotp_driver, ocotp_devclass, 0, 0);

