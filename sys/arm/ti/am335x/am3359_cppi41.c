/*-
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.org>
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
/* Based on sys/arm/ti/ti_sysc.c */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <arm/ti/ti_sysc.h>

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

struct ti_am3359_cppi41_softc {
	device_t		dev;
	struct syscon *		syscon;
	struct resource *	res[4];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	struct mtx		mtx;
};

static struct resource_spec ti_am3359_cppi41_res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_MEMORY, 1, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_MEMORY, 2, RF_ACTIVE | RF_SHAREABLE },
	{ SYS_RES_MEMORY, 3, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

/* Device */
static struct ofw_compat_data compat_data[] = {
	{ "ti,am3359-cppi41",	1 },
	{ NULL,		0 }
};

static int
ti_am3359_cppi41_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct ti_am3359_cppi41_softc *sc;

	sc = device_get_softc(dev);
	DPRINTF(sc->dev, "offset=%lx write %x\n", addr, val);
	mtx_lock(&sc->mtx);
	bus_space_write_4(sc->bst, sc->bsh, addr, val);
	mtx_unlock(&sc->mtx);
	return (0);
}

static uint32_t
ti_am3359_cppi41_read_4(device_t dev, bus_addr_t addr)
{
	struct ti_am3359_cppi41_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);
	val = bus_space_read_4(sc->bst, sc->bsh, addr);
	mtx_unlock(&sc->mtx);
	DPRINTF(sc->dev, "offset=%lx Read %x\n", addr, val);
	return (val);
}

/* device interface */
static int
ti_am3359_cppi41_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI AM3359 CPPI 41");
	return(BUS_PROBE_DEFAULT);
}

static int
ti_am3359_cppi41_attach(device_t dev)
{
	struct ti_am3359_cppi41_softc *sc;
	uint32_t reg, reset_bit, timeout=10;
	uint64_t sysc_address;
	device_t parent;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, ti_am3359_cppi41_res_spec, sc->res)) {
		device_printf(sc->dev, "Cant allocate resources\n");
		return (ENXIO);
	}

	sc->dev = dev;
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);

	/* variant of am335x_usbss.c */
	DPRINTF(dev, "-- RESET USB --\n");
	parent = device_get_parent(dev);
	reset_bit = ti_sysc_get_soft_reset_bit(parent);
	if (reset_bit == 0) {
		DPRINTF(dev, "Dont have reset bit\n");
		return (0);
	}
	sysc_address = ti_sysc_get_sysc_address_offset_host(parent);
	DPRINTF(dev, "sysc_address %x\n", sysc_address);
	ti_am3359_cppi41_write_4(dev, sysc_address, reset_bit);
	DELAY(100);
	reg = ti_am3359_cppi41_read_4(dev, sysc_address);
	if ((reg & reset_bit) && timeout--) {
		DPRINTF(dev, "Reset still ongoing - wait a little bit longer\n");
		DELAY(100);
		reg = ti_am3359_cppi41_read_4(dev, sysc_address);
	}
	if (timeout == 0)
		device_printf(dev, "USB Reset timeout\n");

	return (0);
}

static device_method_t ti_am3359_cppi41_methods[] = {
	DEVMETHOD(device_probe,		ti_am3359_cppi41_probe),
	DEVMETHOD(device_attach,	ti_am3359_cppi41_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_am3359_cppi41, ti_am3359_cppi41_driver,
    ti_am3359_cppi41_methods,sizeof(struct ti_am3359_cppi41_softc),
    simplebus_driver);

EARLY_DRIVER_MODULE(ti_am3359_cppi41, simplebus, ti_am3359_cppi41_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_am3359_cppi41, 1);
MODULE_DEPEND(ti_am3359_cppi41, ti_sysc, 1, 1, 1);
