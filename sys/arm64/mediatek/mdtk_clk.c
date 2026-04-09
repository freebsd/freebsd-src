/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 Martin Filla, Michal Meloun <mmel@FreeBSD.org>
 *
 * MediaTek Shared Clock Framework Core
 * Provides common registration helpers used by all MediaTek SoC clock drivers:
 *   - mt7622_clk_infracfg.c
 *   - mt8395_clk.c
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/mutex.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_link.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_gate.h>
#include <dev/clk/clk_div.h>

#include "mdtk_clk.h"

/* ---- Internal registration helpers ---- */

static void
init_fixeds(struct mdtk_clk_softc *sc, struct clk_fixed_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_fixed_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clknode_fixed_register failed: %d", rv);
	}
}

static void
init_linked(struct mdtk_clk_softc *sc, struct clk_link_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_link_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clknode_link_register failed: %d", rv);
	}
}

static void
init_muxes(struct mdtk_clk_softc *sc, struct clk_mux_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_mux_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clknode_mux_register failed: %d", rv);
	}
}

static void
init_gates(struct mdtk_clk_softc *sc, struct clk_gate_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_gate_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clknode_gate_register failed: %d", rv);
	}
}

static void
init_div(struct mdtk_clk_softc *sc, struct clk_div_def *clks, int nclks)
{
	int i, rv;

	for (i = 0; i < nclks; i++) {
		rv = clknode_div_register(sc->clkdom, clks + i);
		if (rv != 0)
			panic("clknode_div_register failed: %d", rv);
	}
}

/* ---- Public bus access helpers ---- */

int
mdtk_clkdev_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	*val = bus_read_4(sc->mem_res, addr);
	return (0);
}

int
mdtk_clkdev_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	bus_write_4(sc->mem_res, addr, val);
	return (0);
}

int
mdtk_clkdev_modify_4(device_t dev, bus_addr_t addr, uint32_t clear_mask,
    uint32_t set_mask)
{
	struct mdtk_clk_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);
	reg = bus_read_4(sc->mem_res, addr);
	reg &= ~clear_mask;
	reg |= set_mask;
	bus_write_4(sc->mem_res, addr, reg);
	return (0);
}

void
mdtk_clkdev_device_lock(device_t dev)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

void
mdtk_clkdev_device_unlock(device_t dev)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

/* ---- Clock domain registration entry point ---- */

void
mdtk_register_clocks(device_t dev, struct mdtk_clk_def *cldef)
{
	struct mdtk_clk_softc *sc;

	sc = device_get_softc(dev);
	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("mdtk_register_clocks: clkdom_create returned NULL");

	if (cldef->fixed_def != NULL && cldef->num_fixed > 0)
		init_fixeds(sc, cldef->fixed_def, cldef->num_fixed);
	if (cldef->linked_def != NULL && cldef->num_linked > 0)
		init_linked(sc, cldef->linked_def, cldef->num_linked);
	if (cldef->muxes_def != NULL && cldef->num_muxes > 0)
		init_muxes(sc, cldef->muxes_def, cldef->num_muxes);
	if (cldef->gates_def != NULL && cldef->num_gates > 0)
		init_gates(sc, cldef->gates_def, cldef->num_gates);
	if (cldef->dived_def != NULL && cldef->num_dived > 0)
		init_div(sc, cldef->dived_def, cldef->num_dived);

	clkdom_finit(sc->clkdom);
	if (bootverbose)
		clkdom_dump(sc->clkdom);
}

int
mdtk_hwreset_by_idx(struct mdtk_clk_softc *sc, intptr_t idx, bool reset)
{
	device_printf(sc->dev, "hwreset: idx %ld, reset %d\n", idx, reset);
	/* TODO: implement per-SoC reset register offsets */
	return (0);
}
