/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Semihalf.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_fixed.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "clkdev_if.h"
#include "periph.h"

#define SB_DEV_COUNT 14

static struct a37x0_periph_clknode_def a37x0_sb_devices [] = {
	CLK_MDD("gbe_50", 0, 6, 1, DIV_SEL2, DIV_SEL2, 6, 9,
	    "tbg_mux_gbe_50_120", "div1_gbe_50_121", "div2_gbe_50_122"),
	CLK_MDD("gbe_core", 1, 8, 5, DIV_SEL1, DIV_SEL1, 18, 21,
	    "tbg_mux_gbe_core_124", "div1_gbe_core_125", "div2_gbe_core_126"),
	CLK_MDD("gbe_125", 2, 10, 3, DIV_SEL1, DIV_SEL1, 6, 9,
	    "tbg_mux_gbe_125_128", "div1_gbe_50_129", "div2_gbe_50_130"),
	CLK_GATE("gbe1_50", 3, 0, "gbe_50"),
	CLK_GATE("gbe0_50", 4, 1, "gbe_50"),
	CLK_GATE("gbe1_125", 5, 2, "gbe_125"),
	CLK_GATE("gbe0_125", 6, 3, "gbe_125"),
	CLK_MUX_GATE("gbe1_core", 7, 4, 13, "gbe_core",
	    "mux_gbe1_core_136", "fixed_gbe1_core_138"),
	CLK_MUX_GATE("gbe0_core", 8, 5, 14, "gbe_core",
	    "mux_gbe0_core_139", "fixed_gbe0_core_141"),
	CLK_MUX_GATE("gbe_bm", 9, 12, 12, "gbe_core",
	    "mux_gbe_bm_136", "fixed_gbe_bm_138"),
	CLK_FULL_DD("sdio", 10, 11, 14, 7, DIV_SEL0, DIV_SEL0, 3, 6,
	    "tbg_mux_sdio_139", "div1_sdio_140", "div2_sdio_141",
	    "clk_mux_sdio_142"),
	CLK_FULL_DD("usb32_usb2_sys", 11, 16, 16, 8, DIV_SEL0, DIV_SEL0, 9, 12,
	    "tbg_mux_usb32_usb2_sys_144", "div1_usb32_usb2_sys_145",
	    "div2_usb32_usb2_sys_146", "clk_mux_usb32_usb2_sys_147"),
	CLK_FULL_DD("usb32_ss_sys", 12, 17, 18, 9, DIV_SEL0, DIV_SEL0, 15, 18,
	    "tbg_mux_usb32_ss_sys_149", "div1_usb32_ss_sys_150",
	    "div2_usb32_ss_sys_151", "clk_mux_usb32_ss_sys_152"),
	CLK_GATE("pcie", 13, 14, "gbe_core")
};

static struct ofw_compat_data a37x0_sb_periph_compat_data[] = {
	{ "marvell,armada-3700-periph-clock-sb",	1 },
	{ NULL,						0 }
};

static int a37x0_sb_periph_clk_attach(device_t);
static int a37x0_sb_periph_clk_probe(device_t);

static device_method_t a37x0_sb_periph_clk_methods[] = {
	DEVMETHOD(clkdev_device_unlock,		a37x0_periph_clk_device_unlock),
	DEVMETHOD(clkdev_device_lock,		a37x0_periph_clk_device_lock),
	DEVMETHOD(clkdev_read_4,		a37x0_periph_clk_read_4),

	DEVMETHOD(device_attach,		a37x0_sb_periph_clk_attach),
	DEVMETHOD(device_detach,		a37x0_periph_clk_detach),
	DEVMETHOD(device_probe,			a37x0_sb_periph_clk_probe),

	DEVMETHOD_END
};

static driver_t a37x0_sb_periph_driver = {
	"a37x0_sb_periph_driver",
	a37x0_sb_periph_clk_methods,
	sizeof(struct a37x0_periph_clk_softc)
};

devclass_t a37x0_sb_periph_devclass;

EARLY_DRIVER_MODULE(a37x0_sb_periph, simplebus, a37x0_sb_periph_driver,
    a37x0_sb_periph_devclass, 0, 0, BUS_PASS_TIMER + BUS_PASS_ORDER_LATE);

static int
a37x0_sb_periph_clk_attach(device_t dev)
{
	struct a37x0_periph_clk_softc *sc;

	sc = device_get_softc(dev);
	sc->devices = a37x0_sb_devices;
	sc->device_count = SB_DEV_COUNT;

	return (a37x0_periph_clk_attach(dev));
}

static int
a37x0_sb_periph_clk_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_search_compatible(dev,
	    a37x0_sb_periph_compat_data)->ocd_data)
		return (ENXIO);

	device_set_desc(dev, "marvell,armada-3700-sb-periph-clock");

	return (BUS_PROBE_DEFAULT);
}
