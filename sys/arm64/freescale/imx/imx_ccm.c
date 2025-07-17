/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Tom Jones <thj@freebsd.org>
 * under sponsorship from the FreeBSD Foundation.
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

/*
 * Clock Control Module driver for Freescale i.MX 8M SoC family.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm64/freescale/imx/imx_ccm.h>
#include <arm64/freescale/imx/clk/imx_clk_gate.h>
#include <arm64/freescale/imx/clk/imx_clk_mux.h>
#include <arm64/freescale/imx/clk/imx_clk_composite.h>
#include <arm64/freescale/imx/clk/imx_clk_sscg_pll.h>
#include <arm64/freescale/imx/clk/imx_clk_frac_pll.h>

#include "clkdev_if.h"

static inline uint32_t
CCU_READ4(struct imx_ccm_softc *sc, bus_size_t off)
{

	return (bus_read_4(sc->mem_res, off));
}

static inline void
CCU_WRITE4(struct imx_ccm_softc *sc, bus_size_t off, uint32_t val)
{

	bus_write_4(sc->mem_res, off, val);
}

int
imx_ccm_detach(device_t dev)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);

	if (sc->mem_res != NULL)
		bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

	return (0);
}

int
imx_ccm_attach(device_t dev)
{
	struct imx_ccm_softc *sc;
	int err, rid;
	phandle_t node;
	int i;

	sc = device_get_softc(dev);
	sc->dev = dev;
	err = 0;

	/* Allocate bus_space resources. */
	rid = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL) {
		device_printf(dev, "Cannot allocate memory resources\n");
		err = ENXIO;
		goto out;
	}

	mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

	sc->clkdom = clkdom_create(dev);
	if (sc->clkdom == NULL)
		panic("Cannot create clkdom\n");

	for (i = 0; i < sc->nclks; i++) {
		switch (sc->clks[i].type) {
		case IMX_CLK_UNDEFINED:
			break;
		case IMX_CLK_LINK:
			clknode_link_register(sc->clkdom,
			    sc->clks[i].clk.link);
			break;
		case IMX_CLK_FIXED:
			clknode_fixed_register(sc->clkdom,
			    sc->clks[i].clk.fixed);
			break;
		case IMX_CLK_MUX:
			imx_clk_mux_register(sc->clkdom, sc->clks[i].clk.mux);
			break;
		case IMX_CLK_GATE:
			imx_clk_gate_register(sc->clkdom, sc->clks[i].clk.gate);
			break;
		case IMX_CLK_COMPOSITE:
			imx_clk_composite_register(sc->clkdom, sc->clks[i].clk.composite);
			break;
		case IMX_CLK_SSCG_PLL:
			imx_clk_sscg_pll_register(sc->clkdom, sc->clks[i].clk.sscg_pll);
			break;
		case IMX_CLK_FRAC_PLL:
			imx_clk_frac_pll_register(sc->clkdom, sc->clks[i].clk.frac_pll);
			break;
		case IMX_CLK_DIV:
			clknode_div_register(sc->clkdom, sc->clks[i].clk.div);
			break;
		default:
			device_printf(dev, "Unknown clock type %d\n", sc->clks[i].type);
			return (ENXIO);
		}
	}

	if (clkdom_finit(sc->clkdom) != 0)
		panic("cannot finalize clkdom initialization\n");

	if (bootverbose)
		clkdom_dump(sc->clkdom);

	node = ofw_bus_get_node(dev);
	clk_set_assigned(dev, node);

	err = 0;

out:

	if (err != 0)
		imx_ccm_detach(dev);

	return (err);
}

static int
imx_ccm_write_4(device_t dev, bus_addr_t addr, uint32_t val)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);
	CCU_WRITE4(sc, addr, val);
	return (0);
}

static int
imx_ccm_read_4(device_t dev, bus_addr_t addr, uint32_t *val)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);

	*val = CCU_READ4(sc, addr);
	return (0);
}

static int
imx_ccm_modify_4(device_t dev, bus_addr_t addr, uint32_t clr, uint32_t set)
{
	struct imx_ccm_softc *sc;
	uint32_t reg;

	sc = device_get_softc(dev);

	reg = CCU_READ4(sc, addr);
	reg &= ~clr;
	reg |= set;
	CCU_WRITE4(sc, addr, reg);

	return (0);
}

static void
imx_ccm_device_lock(device_t dev)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);
	mtx_lock(&sc->mtx);
}

static void
imx_ccm_device_unlock(device_t dev)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);
	mtx_unlock(&sc->mtx);
}

static device_method_t imx_ccm_methods[] = {
	/* clkdev interface */
	DEVMETHOD(clkdev_write_4,	imx_ccm_write_4),
	DEVMETHOD(clkdev_read_4,	imx_ccm_read_4),
	DEVMETHOD(clkdev_modify_4,	imx_ccm_modify_4),
	DEVMETHOD(clkdev_device_lock,	imx_ccm_device_lock),
	DEVMETHOD(clkdev_device_unlock,	imx_ccm_device_unlock),

	DEVMETHOD_END
};

DEFINE_CLASS_0(imx_ccm, imx_ccm_driver, imx_ccm_methods,
    sizeof(struct imx_ccm_softc));
