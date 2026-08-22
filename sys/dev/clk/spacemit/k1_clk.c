/*
 * Copyright (c) 2026 Bojan Novković <bnovkov@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * Generic clock routines for the SpacemiT K1 SoC.
 *
 * Written using SpacemiT's K1 Chip Product Documentation,
 * Section 9., "Top System (1/2)", available at
 * https://www.spacemit.com/community/document/
 */

#include <sys/cdefs.h>

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/clk/clk.h>

#include <dev/hwreset/hwreset.h>

#include "k1_clk.h"
#include "clkdev_if.h"
#include "hwreset_if.h"

#define K1_CLK_FCREQ_NRETRIES 5

#define DEVICE_LOCK(_clk)				\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define DEVICE_UNLOCK(_clk)				\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define READ4(sc, reg)		bus_read_4((sc)->res, (reg))
#define WRITE4(sc, reg, val)	bus_write_4((sc)->res, (reg), (val))

static int
k1_clk_register(struct clkdom *clkdom,
    const struct k1_clk_def *clkdef, clknode_class_t class)
{
	struct k1_clk_def *sc;
	struct clknode *clk;

	clk = clknode_create(clkdom, class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	*sc = *clkdef;
	clknode_register(clkdom, clk);

	return (0);
}

static void
k1_clkdev_lock(device_t dev)
{
	struct k1_clkdev_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
k1_clkdev_unlock(device_t dev)
{
	struct k1_clkdev_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

int
k1_clk_attach(device_t dev)
{
	struct k1_clkdev_softc *sc;
	clknode_class_t class;
	int error, rid, i;

	sc = device_get_softc(dev);
	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	if (sc->resets != NULL)
		hwreset_register_ofw_provider(dev);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL) {
		device_printf(dev, "Couldn't create clkdom\n");
		return (ENXIO);
	}

	rid = 0;
	sc->res = bus_alloc_resource_any(dev, SYS_RES_MEMORY,
	    &rid, RF_ACTIVE);
	if (sc->res == NULL) {
		device_printf(dev, "Can't allocate memory\n");
		return (ENXIO);
	}

	class = sc->clknode_class != NULL ?
	    sc->clknode_class : &k1_clknode_class;
	for (i = 0; i < sc->nclks; i++) {
		error = k1_clk_register(sc->clkdom, &sc->clks[i], class);
		if (error != 0) {
			device_printf(dev, "Failed to register clock '%s'\n",
			    sc->clks[i].clkdef.name);
			bus_free_resource(dev, SYS_RES_MEMORY, sc->res);
			return (error);
		}
	}

	error = clkdom_finit(sc->clkdom);
	if (error != 0) {
		device_printf(dev, "clkdom_finit() returned %d\n", error);
		bus_free_resource(dev, SYS_RES_MEMORY, sc->res);
		return (error);
	}

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	return (0);
}

static int
k1_clkdev_reset_assert(device_t dev, intptr_t id, bool assert)
{
	struct k1_clkdev_softc *sc;
	const struct k1_reset_def *rdef;
	uint32_t val;

	sc = device_get_softc(dev);

	if (id >= sc->nresets)
		return (0);
	rdef = &sc->resets[id];
	if (rdef->reg == 0)
		return (0);

	mtx_lock(&sc->mtx);
	val = READ4(sc, rdef->reg);
	if (assert)
		val |= rdef->assert_mask;
	else
		val &= ~rdef->deassert_mask;
	WRITE4(sc, rdef->reg, val);
	mtx_unlock(&sc->mtx);

	return (0);
}

static device_method_t k1_clkdev_methods[] = {
	/* clkdev interface */
	DEVMETHOD(clkdev_device_lock,	k1_clkdev_lock),
	DEVMETHOD(clkdev_device_unlock,	k1_clkdev_unlock),

	/* Reset interface */
	DEVMETHOD(hwreset_assert,	k1_clkdev_reset_assert),

	DEVMETHOD_END
};
DEFINE_CLASS_0(k1_clkdev, k1_clkdev_driver, k1_clkdev_methods,
    sizeof(struct k1_clkdev_softc));

static int
k1_clk_set_gate(struct clknode *clk, bool enable)
{
	struct k1_clkdev_softc *sc;
	struct k1_clk_def *clk_sc;
	uint32_t reg;

	sc = device_get_softc(clknode_get_device(clk));
	clk_sc = clknode_get_softc(clk);
	if (clk_sc->gate_mask == 0 || clk_sc->reg == 0)
		return (0);

	DEVICE_LOCK(clk);
	reg = READ4(sc, clk_sc->reg);
	if (enable)
		reg |= clk_sc->gate_mask;
	else
		reg &= ~clk_sc->gate_mask;
	WRITE4(sc, clk_sc->reg, reg);
	DEVICE_UNLOCK(clk);

	return (0);
}

static bool
k1_clk_rounding_check(uint64_t curfreq, uint64_t req, int flags)
{
	switch (CLK_SET_ROUND(flags)) {
	case CLK_SET_ROUND_DOWN:
		return curfreq <= req;
	case CLK_SET_ROUND_UP:
		return curfreq >= req;
	case CLK_SET_ROUND_EXACT:
		/* CLK_SET_ROUND_EXACT is enforced in 'k1_clk_set_freq'. */
	case CLK_SET_ROUND_ANY:
	default:
		return true;
	}
}

/*
 * Find the closest matching rate given the requested frequency 'req' and
 * rounding mode in 'flags' by going through all possible combinations of
 * frequency divisor values and parent clock rates.
 */
static uint64_t
k1_clk_find_best_rate(struct clknode *clk, struct k1_clk_def *sc, uint64_t req,
    int flags, int *parentp, uint32_t *divp)
{
	uint64_t best_match, best_delta, p_freq, candidate_freq;
	uint32_t i, divmax, best_div;
	struct clknode *p_clk;
	const char **p_names;
	int best_parent;
	int p_idx;

	best_div = 1;
	best_match = 0;
	best_parent = 0;
	best_delta = UINT64_MAX;
	p_names = clknode_get_parent_names(clk);
	for (p_idx = 0; p_idx != clknode_get_parents_num(clk); p_idx++) {
		p_clk = clknode_find_by_name(p_names[p_idx]);
		clknode_get_freq(p_clk, &p_freq);

		divmax = 1 << sc->div_nbits;
		for (i = 1; i <= divmax; i++) {
			candidate_freq = p_freq / i;
			if (clk_freq_diff(req, candidate_freq) < best_delta &&
			    k1_clk_rounding_check(candidate_freq, req, flags)) {
				best_match = candidate_freq;
				best_parent = p_idx;
				best_delta = clk_freq_diff(req, candidate_freq);
				best_div = i;
			}
		}
	}

	*parentp = best_parent;
	*divp = best_div;

	return (best_match);
}

/*
 * Certain clocks must perform a "frequency change request" after
 * reparenting or changing the clock's frequency divisor. This is done
 * by setting the appropriate FC bit in the clock's configuration
 * register and waiting until the hardware clears it, indicating
 * that the clock's divisor or parent index was successfully updated.
 */
static int
k1_clk_send_fc_req(struct clknode *clk)
{
	struct k1_clkdev_softc *sc;
	struct k1_clk_def *clk_sc;
	uint32_t val;
	int retries;

	sc = device_get_softc(clknode_get_device(clk));
	clk_sc = clknode_get_softc(clk);

	if (clk_sc->fc_mask == 0)
		return (0);

	retries = K1_CLK_FCREQ_NRETRIES;
	val = READ4(sc, clk_sc->reg);
	val |= clk_sc->fc_mask;
	WRITE4(sc, clk_sc->reg, val);
	while (retries-- > 0) {
		DELAY(1000);
		val = READ4(sc, clk_sc->reg);
		if ((val & clk_sc->fc_mask) == 0)
			break;
	}
	if ((val & clk_sc->fc_mask) != 0) {
		/* The FC bit is still set, something went wrong. */
		return (ENXIO);
	}

	return (0);
}

static int
k1_clk_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	uint32_t div_mask, mux_mask;
	int best_div, best_parent;
	struct k1_clkdev_softc *sc;
	struct k1_clk_def *clk_sc;
	uint64_t best_match;
	uint32_t val;
	device_t dev;
	int p_idx;

	dev = clknode_get_device(clk);
	sc = device_get_softc(clknode_get_device(clk));
	clk_sc = clknode_get_softc(clk);
	p_idx = clknode_get_parent_idx(clk);

	best_match = k1_clk_find_best_rate(clk, clk_sc, *fout, flags,
	    &best_parent, &best_div);
	*stop = 1;
	if (best_match == 0 ||
	    (best_match != *fout &&
		CLK_SET_ROUND(flags) == CLK_SET_ROUND_EXACT))
		return (ERANGE);

	*fout = best_match;
	if ((flags & CLK_SET_DRYRUN) != 0)
		return (0);

	DEVICE_LOCK(clk);
	if (clk_sc->mux_nbits != 0 && p_idx != best_parent) {

		clknode_set_parent_by_idx(clk, best_parent);
		mux_mask = (1 << clk_sc->mux_nbits) - 1;
		mux_mask <<= clk_sc->mux_shift;
		val = READ4(sc, clk_sc->reg);
		val &= ~mux_mask;
		val |= (p_idx << clk_sc->mux_shift);
		WRITE4(sc, clk_sc->reg, val);

		if (k1_clk_send_fc_req(clk) != 0) {
			device_printf(dev,
			    "%s: failed mux frequency change request for '%s'\n",
			    __func__, clk_sc->clkdef.name);
			DEVICE_UNLOCK(clk);
			return (ENXIO);
		}
	}

	if (clk_sc->div_nbits != 0) {
		div_mask = (1 << clk_sc->div_nbits) - 1;
		val = READ4(sc, clk_sc->reg);
		val &= ~(div_mask << clk_sc->div_shift);

		/* The final divisor value is calculated as 'div_value' + 1. */
		val |= (best_div - 1) << clk_sc->div_shift;
		WRITE4(sc, clk_sc->reg, val);

		if (k1_clk_send_fc_req(clk) != 0) {
			device_printf(dev,
			    "%s: failed div frequency change request for '%s'\n",
			    __func__, clk_sc->clkdef.name);
			DEVICE_UNLOCK(clk);
			return (ENXIO);
		}
	}
	DEVICE_UNLOCK(clk);

	return (0);
}

static int
k1_clk_init(struct clknode *clk, device_t dev)
{
	struct k1_clkdev_softc *sc;
	struct k1_clk_def *clk_sc;
	uint32_t val, mux_mask;
	int pidx;

	sc = device_get_softc(clknode_get_device(clk));
	clk_sc = clknode_get_softc(clk);

	if (clk_sc->mux_shift == 0) {
		clknode_init_parent_idx(clk, 0);
		return (0);
	}
	DEVICE_LOCK(clk);
	val = READ4(sc, clk_sc->reg);
	DEVICE_UNLOCK(clk);
	mux_mask = (1 << clk_sc->mux_nbits) - 1;
	pidx = (val >> clk_sc->mux_shift) & mux_mask;

	clknode_init_parent_idx(clk, pidx);

	return (0);
}

static clknode_method_t k1_clknode_methods[] = {
	CLKNODEMETHOD(clknode_init,		k1_clk_init),
	CLKNODEMETHOD(clknode_set_gate,		k1_clk_set_gate),
	CLKNODEMETHOD(clknode_set_freq,		k1_clk_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(k1_clknode, k1_clknode_class,
    k1_clknode_methods, sizeof(struct k1_clk_def), clknode_class);
