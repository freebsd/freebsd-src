/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Rubicon Communications, LLC (Netgate)
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
#include <sys/systm.h>
#include <sys/bus.h>

#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <dev/clk/clk_fixed.h>
#include <dev/syscon/syscon.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include "syscon_if.h"

static struct clk_fixed_def ap806_clk_cluster_0 = {
	.clkdef.id = 0,
	.clkdef.name = "ap806-cpu-cluster-0",
	.freq = 0,
};

static struct clk_fixed_def ap806_clk_cluster_1 = {
	.clkdef.id = 1,
	.clkdef.name = "ap806-cpu-cluster-1",
	.freq = 0,
};

static struct clk_fixed_def ap806_clk_fixed = {
	.clkdef.id = 2,
	.clkdef.name = "ap806-fixed",
	.freq = 1200000000,
};

/* Thoses are the only exported clocks AFAICT */

static const char *mss_parents[] = {"ap806-fixed"};
static struct clk_fixed_def ap806_clk_mss = {
	.clkdef.id = 3,
	.clkdef.name = "ap806-mss",
	.clkdef.parent_names = mss_parents,
	.clkdef.parent_cnt = 1,
	.mult = 1,
	.div = 6,
};

static const char *sdio_parents[] = {"ap806-fixed"};
static struct clk_fixed_def ap806_clk_sdio = {
	.clkdef.id = 4,
	.clkdef.name = "ap806-sdio",
	.clkdef.parent_names = sdio_parents,
	.clkdef.parent_cnt = 1,
	.mult = 1,
	.div = 3,
};

struct mv_ap806_clock_softc {
	device_t		dev;
	struct syscon		*syscon;
};

static struct ofw_compat_data compat_data[] = {
	{"marvell,ap806-clock",	1},
	{NULL,			0}
};

#define	RD4(sc, reg)		SYSCON_READ_4((sc)->syscon, (reg))
#define	WR4(sc, reg, val)	SYSCON_WRITE_4((sc)->syscon, (reg), (val))

static int
mv_ap806_clock_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "Marvell AP806 Clock Controller");
	return (BUS_PROBE_DEFAULT);
}

static int
mv_ap806_clock_attach(device_t dev)
{
	struct mv_ap806_clock_softc *sc;
	struct clkdom *clkdom;
	uint64_t clock_freq;
	uint32_t reg;

	sc = device_get_softc(dev);
	sc->dev = dev;

	if (SYSCON_GET_HANDLE(sc->dev, &sc->syscon) != 0 ||
	    sc->syscon == NULL) {
		device_printf(dev, "cannot get syscon for device\n");
		return (ENXIO);
	}

	reg = RD4(sc, 0x400);
	switch (reg & 0x1f) {
	case 0x0:
	case 0x1:
		clock_freq = 2000000000;
		break;
	case 0x4:
		clock_freq = 1600000000;
		break;
	case 0x6:
		clock_freq = 1800000000;
		break;
	case 0x7:
		clock_freq = 1800000000;
		break;
	case 0xb:
		clock_freq = 1600000000;
		break;
	case 0xd:
		clock_freq = 1600000000;
		break;
	case 0x13:
		clock_freq = 1000000000;
		break;
	case 0x14:
		clock_freq = 1333000000;
		break;
	case 0x17:
		clock_freq = 1333000000;
		break;
	case 0x19:
		clock_freq = 1200000000;
		break;
	case 0x1a:
		clock_freq = 1400000000;
		break;
	case 0x1b:
		clock_freq = 600000000;
		break;
	case 0x1c:
		clock_freq = 800000000;
		break;
	case 0x1d:
		clock_freq = 1000000000;
		break;
	default:
		device_printf(dev, "Cannot guess clock freq with reg %x\n",
		     reg & 0x1f);
		return (ENXIO);
		break;
	};

	ap806_clk_cluster_0.freq = clock_freq;
	ap806_clk_cluster_1.freq = clock_freq;
	clkdom = clkdom_create(dev);

	clknode_fixed_register(clkdom, &ap806_clk_cluster_0);
	clknode_fixed_register(clkdom, &ap806_clk_cluster_1);
	clknode_fixed_register(clkdom, &ap806_clk_fixed);
	clknode_fixed_register(clkdom, &ap806_clk_mss);
	clknode_fixed_register(clkdom, &ap806_clk_sdio);

	clkdom_finit(clkdom);

	if (bootverbose)
		clkdom_dump(clkdom);
	return (0);
}

static int
mv_ap806_clock_detach(device_t dev)
{

	return (EBUSY);
}

static device_method_t mv_ap806_clock_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mv_ap806_clock_probe),
	DEVMETHOD(device_attach,	mv_ap806_clock_attach),
	DEVMETHOD(device_detach,	mv_ap806_clock_detach),

	DEVMETHOD_END
};

static driver_t mv_ap806_clock_driver = {
	"mv_ap806_clock",
	mv_ap806_clock_methods,
	sizeof(struct mv_ap806_clock_softc),
};

EARLY_DRIVER_MODULE(mv_ap806_clock, simplebus, mv_ap806_clock_driver, 0, 0,
    BUS_PASS_RESOURCE + BUS_PASS_ORDER_LATE);
