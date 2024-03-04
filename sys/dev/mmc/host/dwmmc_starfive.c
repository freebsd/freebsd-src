/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2017 Emmanuel Vadot <manu@freebsd.org>
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * Portions of this software were developed by Mitchell Horne
 * <mhorne@FreeBSD.org> under sponsorship from the FreeBSD Foundation.
 */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/module.h>
#include <sys/queue.h>
#include <sys/taskqueue.h>

#include <machine/bus.h>

#include <dev/mmc/bridge.h>
#include <dev/mmc/mmc_fdt_helpers.h>

#include <dev/mmc/host/dwmmc_var.h>

#include <dev/ofw/ofw_bus_subr.h>

#include "opt_mmccam.h"

enum dwmmc_type {
	DWMMC_GENERIC = 1,
	DWMMC_JH7110
};

static struct ofw_compat_data compat_data[] = {
	{"snps,dw-mshc",	DWMMC_GENERIC},
	{"starfive,jh7110-mmc",	DWMMC_JH7110},
	{NULL,			0}
};

static int dwmmc_starfive_update_ios(struct dwmmc_softc *sc,
    struct mmc_ios *ios)
{
	int err;

	if (ios->clock != 0 && ios->clock != sc->bus_hz) {
		err = clk_set_freq(sc->ciu, ios->clock, CLK_SET_ROUND_DOWN);
		if (err != 0) {
			printf("%s, Failed to set freq for ciu clock\n",
			    __func__);
			return (err);
		}
		sc->bus_hz = ios->clock;
	}

	return (0);
}

static int
starfive_dwmmc_probe(device_t dev)
{
	phandle_t node;
	int type;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (type == 0)
		return (ENXIO);

	/*
	 * If we matched the generic compat string, check the top-level board
	 * compatible, to ensure we should actually use the starfive driver.
	 */
	if (type == DWMMC_GENERIC) {
		node = OF_finddevice("/");
		if (!ofw_bus_node_is_compatible(node, "starfive,jh7110"))
			return (ENXIO);
	}

	device_set_desc(dev, "Synopsys DesignWare Mobile Storage "
	    "Host Controller (StarFive)");

	return (BUS_PROBE_VENDOR);
}

static int
starfive_dwmmc_attach(device_t dev)
{
	struct dwmmc_softc *sc;

	sc = device_get_softc(dev);
	sc->update_ios = &dwmmc_starfive_update_ios;

	return (dwmmc_attach(dev));
}

static device_method_t starfive_dwmmc_methods[] = {
	/* bus interface */
	DEVMETHOD(device_probe,		starfive_dwmmc_probe),
	DEVMETHOD(device_attach,	starfive_dwmmc_attach),
	DEVMETHOD(device_detach,	dwmmc_detach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(starfive_dwmmc, starfive_dwmmc_driver, starfive_dwmmc_methods,
    sizeof(struct dwmmc_softc), dwmmc_driver);

DRIVER_MODULE(starfive_dwmmc, simplebus, starfive_dwmmc_driver, 0, 0);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(starfive_dwmmc);
#endif
