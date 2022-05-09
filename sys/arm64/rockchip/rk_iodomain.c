/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@FreeBSD.org>
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/syscon/syscon.h>
#include <dev/extres/regulator/regulator.h>

#include "syscon_if.h"

#define	RK3288_GRF_IO_VSEL		0x380
#define	RK3399_GRF_IO_VSEL		0xe640
#define	RK3399_PMUGRF_SOC_CON0		0x180

struct rk_iodomain_supply {
	char		*name;
	uint32_t	bit;
};

struct rk_iodomain_softc;

struct rk_iodomain_conf {
	struct rk_iodomain_supply	*supply;
	int				nsupply;
	uint32_t			grf_reg;
	void				(*init)(struct rk_iodomain_softc *sc);
};

struct rk_iodomain_softc {
	device_t			dev;
	struct syscon			*grf;
	phandle_t			node;
	struct rk_iodomain_conf		*conf;
};

static struct rk_iodomain_supply rk3288_supply[] = {
	{"lcdc-supply", 0},
	{"dvp-supply", 1},
	{"flash0-supply", 2},
	{"flash1-supply", 3},
	{"wifi-supply", 4},
	{"bb-supply", 5},
	{"audio-supply", 6},
	{"sdcard-supply", 7},
	{"gpio30-supply", 8},
	{"gpio1830-supply", 9},
};

static struct rk_iodomain_conf rk3288_conf = {
	.supply = rk3288_supply,
	.nsupply = nitems(rk3288_supply),
	.grf_reg = RK3288_GRF_IO_VSEL,
};

static struct rk_iodomain_supply rk3399_supply[] = {
	{"bt656-supply", 0},
	{"audio-supply", 1},
	{"sdmmc-supply", 2},
	{"gpio1830-supply", 3},
};

static struct rk_iodomain_conf rk3399_conf = {
	.supply = rk3399_supply,
	.nsupply = nitems(rk3399_supply),
	.grf_reg = RK3399_GRF_IO_VSEL,
};

static struct rk_iodomain_supply rk3399_pmu_supply[] = {
	{"pmu1830-supply", 9},
};

static void rk3399_pmu_init(struct rk_iodomain_softc *sc);
static struct rk_iodomain_conf rk3399_pmu_conf = {
	.supply = rk3399_pmu_supply,
	.nsupply = nitems(rk3399_pmu_supply),
	.grf_reg = RK3399_PMUGRF_SOC_CON0,
	.init = rk3399_pmu_init,
};

static struct ofw_compat_data compat_data[] = {
	{"rockchip,rk3288-io-voltage-domain", (uintptr_t)&rk3288_conf},
	{"rockchip,rk3399-io-voltage-domain", (uintptr_t)&rk3399_conf},
	{"rockchip,rk3399-pmu-io-voltage-domain", (uintptr_t)&rk3399_pmu_conf},
	{NULL,             0}
};

static void
rk3399_pmu_init(struct rk_iodomain_softc *sc)
{

	SYSCON_WRITE_4(sc->grf, RK3399_PMUGRF_SOC_CON0,
	    (1 << 8) | (1 << (8 + 16)));	/* set pmu1830_volsel */
}

static void
rk_iodomain_set(struct rk_iodomain_softc *sc)
{
	regulator_t supply;
	uint32_t reg = 0;
	uint32_t mask = 0;
	int uvolt, i;

	for (i = 0; i < sc->conf->nsupply; i++) {
		mask |= (1 << sc->conf->supply[i].bit) << 16;
		if (regulator_get_by_ofw_property(sc->dev, sc->node,
		    sc->conf->supply[i].name, &supply) == 0) {
			if (regulator_get_voltage(supply, &uvolt) == 0) {
				if (uvolt == 1800000)
					reg |= (1 << sc->conf->supply[i].bit);
				else if (uvolt != 3000000)
					device_printf(sc->dev,
					  "%s regulator is at %duV, ignoring\n",
					  sc->conf->supply[i].name, uvolt);
			} else
				device_printf(sc->dev, "Cannot get current "
				    "voltage for regulator %s\n",
				    sc->conf->supply[i].name);
		}
	}

	SYSCON_WRITE_4(sc->grf, sc->conf->grf_reg, reg | mask);
	if (sc->conf->init != NULL)
		 sc->conf->init(sc);
}

static int
rk_iodomain_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	device_set_desc(dev, "RockChip IO Voltage Domain");
	return (BUS_PROBE_DEFAULT);
}

static int
rk_iodomain_attach(device_t dev)
{
	struct rk_iodomain_softc *sc;
	int rv;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->node = ofw_bus_get_node(dev);

	rv = syscon_get_handle_default(dev, &sc->grf);
	if (rv != 0) {
		device_printf(dev, "Cannot get grf handle\n");
		return (ENXIO);
	}

	sc->conf = (struct rk_iodomain_conf *)ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	rk_iodomain_set(sc);

	return (0);
}

static int
rk_iodomain_detach(device_t dev)
{

	return (0);
}

static device_method_t rk_iodomain_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk_iodomain_probe),
	DEVMETHOD(device_attach,	rk_iodomain_attach),
	DEVMETHOD(device_detach,	rk_iodomain_detach),

	DEVMETHOD_END
};

static driver_t rk_iodomain_driver = {
	"rk_iodomain",
	rk_iodomain_methods,
	sizeof(struct rk_iodomain_softc),
};

EARLY_DRIVER_MODULE(rk_iodomain, simplebus, rk_iodomain_driver, 0, 0,
    BUS_PASS_INTERRUPT + BUS_PASS_ORDER_MIDDLE);
