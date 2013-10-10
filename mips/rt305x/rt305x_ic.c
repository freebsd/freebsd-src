/*-
 * Copyright (c) 2010 Aleksandr Rybalko.
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
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <mips/rt305x/rt305xreg.h>
#include <mips/rt305x/rt305x_icvar.h>


static int	rt305x_ic_probe(device_t);
static int	rt305x_ic_attach(device_t);
static int	rt305x_ic_detach(device_t);


static struct rt305x_ic_softc *rt305x_ic_softc = NULL;

static int
rt305x_ic_probe(device_t dev)
{
	device_set_desc(dev, "RT305X Interrupt Controller driver");
	return (0);
}

static int
rt305x_ic_attach(device_t dev)
{
	struct rt305x_ic_softc *sc = device_get_softc(dev);
	int error = 0;

	KASSERT((device_get_unit(dev) == 0),
	    ("rt305x_ic: Only one Interrupt Controller module supported"));

	if (rt305x_ic_softc != NULL)
		return (ENXIO);
	rt305x_ic_softc = sc;


	/* Map control/status registers. */
	sc->mem_rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &sc->mem_rid, RF_ACTIVE);

	if (sc->mem_res == NULL) {
		device_printf(dev, "couldn't map memory\n");
		error = ENXIO;
		rt305x_ic_detach(dev);
		return(error);
	}
	return (bus_generic_attach(dev));
}

static int
rt305x_ic_detach(device_t dev)
{
	struct rt305x_ic_softc *sc = device_get_softc(dev);

	bus_generic_detach(dev);

	if (sc->mem_res)
		bus_release_resource(dev, SYS_RES_MEMORY, sc->mem_rid,
		    sc->mem_res);
	return(0);
}


uint32_t
rt305x_ic_get(uint32_t reg)
{
	struct rt305x_ic_softc *sc = rt305x_ic_softc;

	if (!sc)
		return (0);

	return (bus_read_4(sc->mem_res, reg));
}

void
rt305x_ic_set(uint32_t reg, uint32_t val)
{
	struct rt305x_ic_softc *sc = rt305x_ic_softc;

	if (!sc)
		return;

	bus_write_4(sc->mem_res, reg, val);

	return;
}


static device_method_t rt305x_ic_methods[] = {
	DEVMETHOD(device_probe,			rt305x_ic_probe),
	DEVMETHOD(device_attach,		rt305x_ic_attach),
	DEVMETHOD(device_detach,		rt305x_ic_detach),

	{0, 0},
};

static driver_t rt305x_ic_driver = {
	"rt305x_ic",
	rt305x_ic_methods,
	sizeof(struct rt305x_ic_softc),
};
static devclass_t rt305x_ic_devclass;

DRIVER_MODULE(rt305x_ic, obio, rt305x_ic_driver, rt305x_ic_devclass, 0, 0);
