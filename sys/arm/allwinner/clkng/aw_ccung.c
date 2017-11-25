/*-
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
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

/*
 * Allwinner Clock Control Unit
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_gate.h>

#include <dev/extres/hwreset/hwreset.h>

#include <arm/allwinner/clkng/aw_ccung.h>
#include <arm/allwinner/clkng/aw_clk.h>

#ifdef __aarch64__
#include "opt_soc.h"
#endif

#if defined(SOC_ALLWINNER_A13)
#include <arm/allwinner/clkng/ccu_a13.h>
#endif

#if defined(SOC_ALLWINNER_A31)
#include <arm/allwinner/clkng/ccu_a31.h>
#endif

#if defined(SOC_ALLWINNER_A64)
#include <arm/allwinner/clkng/ccu_a64.h>
#include <arm/allwinner/clkng/ccu_sun8i_r.h>
#endif

#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)
#include <arm/allwinner/clkng/ccu_h3.h>
#include <arm/allwinner/clkng/ccu_sun8i_r.h>
#endif

#if defined(SOC_ALLWINNER_A83T)
#include <arm/allwinner/clkng/ccu_a83t.h>
#include <arm/allwinner/clkng/ccu_sun8i_r.h>
#endif

#include "clkdev_if.h"
#include "hwreset_if.h"

static struct resource_spec aw_ccung_spec[] = {
	{ SYS_RES_MEMORY,	0,	RF_ACTIVE },
	{ -1, 0 }
};

static struct ofw_compat_data compat_data[] = {
#if defined(SOC_ALLWINNER_A31)
	{ "allwinner,sun5i-a13-ccu", A13_CCU},
#endif
#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)
	{ "allwinner,sun8i-h3-ccu", H3_CCU },
	{ "allwinner,sun8i-h3-r-ccu", H3_R_CCU },
#endif
#if defined(SOC_ALLWINNER_A31)
	{ "allwinner,sun6i-a31-ccu", A31_CCU },
#endif
#if defined(SOC_ALLWINNER_A64)
	{ "allwinner,sun50i-a64-ccu", A64_CCU },
	{ "allwinner,sun50i-a64-r-ccu", A64_R_CCU },
#endif
#if defined(SOC_ALLWINNER_A83T)
	{ "allwinner,sun8i-a83t-ccu", A83T_CCU },
	{ "allwinner,sun8i-a83t-r-ccu", A83T_R_CCU },
#endif
	{NULL, 0 }
};

#define	CCU_READ4(sc, reg)		bus_read_4((sc)->res, (reg))
#define	CCU_WRITE4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int
aw_ccung_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);
	CCU_WRITE4(sc, addr, val);
	return (0);
}

static int
aw_ccung_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);

	*val = CCU_READ4(sc, addr);
	return (0);
}

static int
aw_ccung_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct aw_ccung_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	reg = CCU_READ4(sc, addr);
	reg &= ~clr;
	reg |= set;
	CCU_WRITE4(sc, addr, reg);

	return (0);
}

static int
aw_ccung_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct aw_ccung_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	if (id >= sc->nresets || sc->resets[id].offset == 0)
		return (0);

	mtx_lock(&sc->mtx);
	val = CCU_READ4(sc, sc->resets[id].offset);
	if (reset)
		val &= ~(1 << sc->resets[id].shift);
	else
		val |= 1 << sc->resets[id].shift;
	CCU_WRITE4(sc, sc->resets[id].offset, val);
	mtx_unlock(&sc->mtx);

	return (0);
}

static int
aw_ccung_reset_is_asserted(device_t dev, intptr_t id, bool *reset)
{
	struct aw_ccung_softc *sc;
	uint32_t val;

	sc = device_get_softc(dev);

	if (id >= sc->nresets || sc->resets[id].offset == 0)
		return (0);

	mtx_lock(&sc->mtx);
	val = CCU_READ4(sc, sc->resets[id].offset);
	*reset = (val & (1 << sc->resets[id].shift)) != 0 ? false : true;
	mtx_unlock(&sc->mtx);

	return (0);
}

static void
aw_ccung_device_lock(device_t dev)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
aw_ccung_device_unlock(device_t dev)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static int
aw_ccung_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Allwinner Clock Control Unit NG");
	return (BUS_PROBE_DEFAULT);
}

static int
aw_ccung_register_gates(struct aw_ccung_softc *sc)
{
	struct clk_gate_def def;
	int i;

	for (i = 0; i < sc->ngates; i++) {
		if (sc->gates[i].name == NULL)
			continue;
		memset(&def, 0, sizeof(def));
		def.clkdef.id = i;
		def.clkdef.name = sc->gates[i].name;
		def.clkdef.parent_names = &sc->gates[i].parent_name;
		def.clkdef.parent_cnt = 1;
		def.offset = sc->gates[i].offset;
		def.shift = sc->gates[i].shift;
		def.mask = 1;
		def.on_value = 1;
		def.off_value = 0;
		clknode_gate_register(sc->clkdom, &def);
	}

	return (0);
}

static void
aw_ccung_init_clocks(struct aw_ccung_softc *sc)
{
	struct clknode *clknode;
	int i, error;

	for (i = 0; i < sc->n_clk_init; i++) {
		clknode = clknode_find_by_name(sc->clk_init[i].name);
		if (clknode == NULL) {
			device_printf(sc->dev, "Cannot find clock %s\n",
			    sc->clk_init[i].name);
			continue;
		}

		if (sc->clk_init[i].parent_name != NULL) {
			if (bootverbose)
				device_printf(sc->dev, "Setting %s as parent for %s\n",
				    sc->clk_init[i].parent_name,
				    sc->clk_init[i].name);
			error = clknode_set_parent_by_name(clknode,
			    sc->clk_init[i].parent_name);
			if (error != 0) {
				device_printf(sc->dev,
				    "Cannot set parent to %s for %s\n",
				    sc->clk_init[i].parent_name,
				    sc->clk_init[i].name);
				continue;
			}
		}
		if (sc->clk_init[i].default_freq != 0) {
			error = clknode_set_freq(clknode,
			    sc->clk_init[i].default_freq, 0 , 0);
			if (error != 0) {
				device_printf(sc->dev,
				    "Cannot set frequency for %s to %ju\n",
				    sc->clk_init[i].name,
				    sc->clk_init[i].default_freq);
				continue;
			}
		}
		if (sc->clk_init[i].enable) {
			error = clknode_enable(clknode);
			if (error != 0) {
				device_printf(sc->dev,
				    "Cannot enable %s\n",
				    sc->clk_init[i].name);
				continue;
			}
		}
	}
}

static int
aw_ccung_attach(device_t dev)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (bus_alloc_resources(dev, aw_ccung_spec, &sc->res) != 0) {
		device_printf(dev, "cannot allocate resources for device\n");
		return (ENXIO);
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("Cannot create clkdom\n");

	switch (sc->type) {
#if defined(SOC_ALLWINNER_A13)
	case A13_CCU:
		ccu_a13_register_clocks(sc);
		break;
#endif
#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)
	case H3_CCU:
		ccu_h3_register_clocks(sc);
		break;
	case H3_R_CCU:
		ccu_sun8i_r_register_clocks(sc);
		break;
#endif
#if defined(SOC_ALLWINNER_A31)
	case A31_CCU:
		ccu_a31_register_clocks(sc);
		break;
#endif
#if defined(SOC_ALLWINNER_A64)
	case A64_CCU:
		ccu_a64_register_clocks(sc);
		break;
	case A64_R_CCU:
		ccu_sun8i_r_register_clocks(sc);
		break;
#endif
#if defined(SOC_ALLWINNER_A83T)
	case A83T_CCU:
		ccu_a83t_register_clocks(sc);
		break;
	case A83T_R_CCU:
		ccu_sun8i_r_register_clocks(sc);
		break;
#endif
	}

	if (sc->gates)
		aw_ccung_register_gates(sc);
	if (clkdom_finit(sc->clkdom) != 0)
		panic("cannot finalize clkdom initialization\n");

	clkdom_xlock(sc->clkdom);
	aw_ccung_init_clocks(sc);
	clkdom_unlock(sc->clkdom);

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	/* If we have resets, register our self as a reset provider */
	if (sc->resets)
		hwreset_register_ofw_provider(dev);

	return (0);
}

static device_method_t aw_ccung_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		aw_ccung_probe),
	DEVMETHOD(device_attach,	aw_ccung_attach),

	/* clkdev interface */
	DEVMETHOD(clkdev_write_4,	aw_ccung_write_4),
	DEVMETHOD(clkdev_read_4,	aw_ccung_read_4),
	DEVMETHOD(clkdev_modify_4,	aw_ccung_modify_4),
	DEVMETHOD(clkdev_device_lock,	aw_ccung_device_lock),
	DEVMETHOD(clkdev_device_unlock,	aw_ccung_device_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	aw_ccung_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	aw_ccung_reset_is_asserted),

	DEVMETHOD_END
};

static driver_t aw_ccung_driver = {
	"aw_ccung",
	aw_ccung_methods,
	sizeof(struct aw_ccung_softc),
};

static devclass_t aw_ccung_devclass;

EARLY_DRIVER_MODULE(aw_ccung, simplebus, aw_ccung_driver, aw_ccung_devclass,
    0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
MODULE_VERSION(aw_ccung, 1);
