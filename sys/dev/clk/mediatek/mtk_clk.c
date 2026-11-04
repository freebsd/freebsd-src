/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Common part of the MediaTek clock controller drivers.  Each controller
 * supplies a table of clocks.  This driver owns the registers and the lock
 * for read-modify-write access to them.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>

#include <machine/bus.h>

#include <dev/clk/mediatek/mtk_clk.h>
#include <dev/hwreset/hwreset.h>

#include "clkdev_if.h"
#include "hwreset_if.h"

static int
mtk_clk_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);

	bus_write_4(sc->res, addr, val);
	return (0);
}

static int
mtk_clk_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);

	*val = bus_read_4(sc->res, addr);
	return (0);
}

static int
mtk_clk_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);
	uint32_t val;

	val = bus_read_4(sc->res, addr);
	val &= ~clr;
	val |= set;
	bus_write_4(sc->res, addr, val);
	return (0);
}

static void
mtk_clk_device_lock(device_t dev)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);

	mtx_lock(&sc->mtx);
}

static void
mtk_clk_device_unlock(device_t dev)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);

	mtx_unlock(&sc->mtx);
}

static int
mtk_clk_reset_lookup(struct mtk_clk_softc *sc, intptr_t id, bus_size_t *reg,
    uint32_t *mask)
{
	const struct mtk_clk_reset_desc *rd = sc->resets;

	if (id < 0 || (size_t)(id / 32) >= rd->nbanks)
		return (EINVAL);
	*reg = rd->banks[id / 32];
	*mask = 1u << (id % 32);
	return (0);
}

static int
mtk_clk_reset_assert(device_t dev, intptr_t id, bool reset)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);
	bus_size_t reg;
	uint32_t mask;
	int rv;

	if ((rv = mtk_clk_reset_lookup(sc, id, &reg, &mask)) != 0)
		return (rv);
	mtx_lock(&sc->mtx);
	bus_write_4(sc->res, reset ? reg : reg + 4, mask);
	mtx_unlock(&sc->mtx);
	return (0);
}

static int
mtk_clk_reset_is_asserted(device_t dev, intptr_t id, bool *reset)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);
	bus_size_t reg;
	uint32_t mask;
	int rv;

	if ((rv = mtk_clk_reset_lookup(sc, id, &reg, &mask)) != 0)
		return (rv);
	mtx_lock(&sc->mtx);
	*reset = (bus_read_4(sc->res, reg + 8) & mask) != 0;
	mtx_unlock(&sc->mtx);
	return (0);
}

static const struct clknode_init_def *
mtk_clk_def_clkdef(const struct mtk_clk_def *def)
{
	switch (def->type) {
	case MTK_CLK_LINK:
		return (&def->link.clkdef);
	case MTK_CLK_FIXED:
		return (&def->fixed.clkdef);
	case MTK_CLK_PLL:
		return (&def->pll.clkdef);
	case MTK_CLK_MUX:
		return (&def->mux.clkdef);
	case MTK_CLK_GATE:
		return (&def->gate.clkdef);
	default:
		return (NULL);
	}
}

int
mtk_clk_attach(device_t dev, const struct mtk_clk_desc *desc)
{
	struct mtk_clk_softc *sc = device_get_softc(dev);
	struct mtk_clk_def *def;
	size_t i;
	int rid, rv;

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "cannot allocate registers\n");
		return (ENXIO);
	}
	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "cannot create clock domain\n");
		rv = ENXIO;
		goto fail;
	}

	for (i = 0; i < desc->nclks; i++) {
		def = &desc->clks[i];
		switch (def->type) {
		case MTK_CLK_LINK:
			rv = clknode_link_register(sc->clkdom, &def->link);
			break;
		case MTK_CLK_FIXED:
			rv = clknode_fixed_register(sc->clkdom, &def->fixed);
			break;
		case MTK_CLK_PLL:
			rv = mtk_clk_pll_register(sc->clkdom, &def->pll);
			break;
		case MTK_CLK_MUX:
			rv = mtk_clk_mux_register(sc->clkdom, &def->mux);
			break;
		case MTK_CLK_GATE:
			rv = mtk_clk_gate_register(sc->clkdom, &def->gate);
			break;
		default:
			rv = EINVAL;
			break;
		}
		/*
		 * Nodes cannot be unregistered once others reference them, so
		 * a failure here cannot be undone.
		 */
		if (rv != 0)
			panic("%s: cannot register clock %s: %d",
			    device_get_nameunit(dev),
			    mtk_clk_def_clkdef(def)->name, rv);
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("%s: cannot finalize clock domain",
		    device_get_nameunit(dev));
	if (bootverbose)
		clkdom_dump(sc->clkdom);

	if (desc->resets != NULL) {
		sc->resets = desc->resets;
		hwreset_register_ofw_provider(dev);
	}

	return (0);

fail:
	mtx_destroy(&sc->mtx);
	bus_release_resource(dev, SYS_RES_MEMORY, rid, sc->res);
	return (rv);
}

static device_method_t mtk_clk_methods[] = {
	DEVMETHOD(clkdev_write_4,	mtk_clk_write_4),
	DEVMETHOD(clkdev_read_4,	mtk_clk_read_4),
	DEVMETHOD(clkdev_modify_4,	mtk_clk_modify_4),
	DEVMETHOD(clkdev_device_lock,	mtk_clk_device_lock),
	DEVMETHOD(clkdev_device_unlock,	mtk_clk_device_unlock),

	DEVMETHOD(hwreset_assert,	mtk_clk_reset_assert),
	DEVMETHOD(hwreset_is_asserted,	mtk_clk_reset_is_asserted),

	DEVMETHOD_END
};

DEFINE_CLASS_0(mtk_clk, mtk_clk_driver, mtk_clk_methods,
    sizeof(struct mtk_clk_softc));
