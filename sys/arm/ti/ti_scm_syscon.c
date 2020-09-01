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

#include "syscon_if.h"
#include <dev/extres/syscon/syscon.h>
#include "clkdev_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg)
#else
#define DPRINTF(dev, msg...)
#endif

MALLOC_DECLARE(M_SYSCON);

struct ti_scm_syscon_softc {
	struct simplebus_softc	sc_simplebus;
	device_t		dev;
	struct syscon *		syscon;
	struct resource *	res[1];
	bus_space_tag_t		bst;
	bus_space_handle_t	bsh;
	struct mtx		mtx;
};

static struct resource_spec ti_scm_syscon_res_spec[] = {
	{ SYS_RES_MEMORY, 0, RF_ACTIVE | RF_SHAREABLE },
	{ -1, 0 }
};

/* Device */
static struct ofw_compat_data compat_data[] = {
	{ "syscon",	1 },
	{ NULL,		0 }
};

/* --- dev/extres/syscon syscon_method_t interface --- */
static int
ti_scm_syscon_write_4(struct syscon *syscon, bus_size_t offset, uint32_t val)
{
	struct ti_scm_syscon_softc *sc;

	sc = device_get_softc(syscon->pdev);
	DPRINTF(sc->dev, "offset=%lx write %x\n", offset, val);
	mtx_lock(&sc->mtx);
	bus_space_write_4(sc->bst, sc->bsh, offset, val);
	mtx_unlock(&sc->mtx);
	return (0);
}

static uint32_t
ti_scm_syscon_read_4(struct syscon *syscon, bus_size_t offset)
{
	struct ti_scm_syscon_softc *sc;
	uint32_t val;

	sc = device_get_softc(syscon->pdev);

	mtx_lock(&sc->mtx);
	val = bus_space_read_4(sc->bst, sc->bsh, offset);
	mtx_unlock(&sc->mtx);
	DPRINTF(sc->dev, "offset=%lx Read %x\n", offset, val);
	return (val);
}
static int
ti_scm_syscon_modify_4(struct syscon *syscon, bus_size_t offset, uint32_t clr, uint32_t set)
{
	struct ti_scm_syscon_softc *sc;
	uint32_t reg;

	sc = device_get_softc(syscon->pdev);

	mtx_lock(&sc->mtx);
	reg = bus_space_read_4(sc->bst, sc->bsh, offset);
	reg &= ~clr;
	reg |= set;
	bus_space_write_4(sc->bst, sc->bsh, offset, reg);
	mtx_unlock(&sc->mtx);
	DPRINTF(sc->dev, "offset=%lx reg: %x (clr %x set %x)\n", offset, reg, clr, set);

	return (0);
}

static syscon_method_t ti_scm_syscon_reg_methods[] = {
	SYSCONMETHOD(syscon_read_4,	ti_scm_syscon_read_4),
	SYSCONMETHOD(syscon_write_4,	ti_scm_syscon_write_4),
	SYSCONMETHOD(syscon_modify_4,	ti_scm_syscon_modify_4),

	SYSCONMETHOD_END
};

DEFINE_CLASS_1(ti_scm_syscon_reg, ti_scm_syscon_reg_class, ti_scm_syscon_reg_methods,
    0, syscon_class);

/* device interface */
static int
ti_scm_syscon_probe(device_t dev)
{
	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "TI OMAP Control Module Syscon");
	return(BUS_PROBE_DEFAULT);
}

static int
ti_scm_syscon_attach(device_t dev)
{
	struct ti_scm_syscon_softc *sc;
	phandle_t node, child;
	int err;

 	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, ti_scm_syscon_res_spec, sc->res)) {
		device_printf(sc->dev, "Cant allocate resources\n");
		return (ENXIO);
	}

	sc->dev = dev;
	sc->bst = rman_get_bustag(sc->res[0]);
	sc->bsh = rman_get_bushandle(sc->res[0]);

	mtx_init(&sc->mtx, device_get_nameunit(sc->dev), NULL, MTX_DEF);
	node = ofw_bus_get_node(sc->dev);

	/* dev/extres/syscon interface */
	sc->syscon = syscon_create_ofw_node(dev, &ti_scm_syscon_reg_class, node);
	if (sc->syscon == NULL) {
		device_printf(dev, "Failed to create/register syscon\n");
		return (ENXIO);
	}

	simplebus_init(sc->dev, node);

	err = bus_generic_probe(sc->dev);
	for (child = OF_child(node); child != 0; child = OF_peer(child)) {
		simplebus_add_device(sc->dev, child, 0, NULL, -1, NULL);
	}

	return (bus_generic_attach(sc->dev));
}

/* syscon interface */
static int
ti_scm_syscon_get_handle(device_t dev, struct syscon **syscon)
{
	struct ti_scm_syscon_softc *sc;

	sc = device_get_softc(dev);
	*syscon = sc->syscon;
	if (*syscon == NULL)
		return (ENODEV);
	return (0);
}

/* clkdev interface */
static int
ti_scm_syscon_clk_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct ti_scm_syscon_softc *sc;

	sc = device_get_softc(dev);
	DPRINTF(sc->dev, "offset=%lx write %x\n", addr, val);
	bus_space_write_4(sc->bst, sc->bsh, addr, val);
	return (0);
}

static int
ti_scm_syscon_clk_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct ti_scm_syscon_softc *sc;

	sc = device_get_softc(dev);

	*val = bus_space_read_4(sc->bst, sc->bsh, addr);
	DPRINTF(sc->dev, "offset=%lx Read %x\n", addr, *val);
	return (0);
}

static int
ti_scm_syscon_clk_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct ti_scm_syscon_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	reg = bus_space_read_4(sc->bst, sc->bsh, addr);
	reg &= ~clr;
	reg |= set;
	bus_space_write_4(sc->bst, sc->bsh, addr, reg);
	DPRINTF(sc->dev, "offset=%lx reg: %x (clr %x set %x)\n", addr, reg, clr, set);

	return (0);
}

static void
ti_scm_syscon_clk_device_lock(device_t dev)
{
	struct ti_scm_syscon_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
ti_scm_syscon_clk_device_unlock(device_t dev)
{
	struct ti_scm_syscon_softc *sc;
	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static device_method_t ti_scm_syscon_methods[] = {
	DEVMETHOD(device_probe,		ti_scm_syscon_probe),
	DEVMETHOD(device_attach,	ti_scm_syscon_attach),

	/* syscon interface */
	DEVMETHOD(syscon_get_handle,	ti_scm_syscon_get_handle),

	/* clkdev interface */
	DEVMETHOD(clkdev_write_4,	ti_scm_syscon_clk_write_4),
	DEVMETHOD(clkdev_read_4,	ti_scm_syscon_clk_read_4),
	DEVMETHOD(clkdev_modify_4,	ti_scm_syscon_clk_modify_4),
	DEVMETHOD(clkdev_device_lock,	ti_scm_syscon_clk_device_lock),
	DEVMETHOD(clkdev_device_unlock,	ti_scm_syscon_clk_device_unlock),

	DEVMETHOD_END
};

DEFINE_CLASS_1(ti_scm_syscon, ti_scm_syscon_driver, ti_scm_syscon_methods,
    sizeof(struct ti_scm_syscon_softc), simplebus_driver);

static devclass_t ti_scm_syscon_devclass;

EARLY_DRIVER_MODULE(ti_scm_syscon, simplebus, ti_scm_syscon_driver,
    ti_scm_syscon_devclass, 0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(ti_scm_syscon, 1);
MODULE_DEPEND(ti_scm_syscon, ti_scm, 1, 1, 1);
