/*
 * Copyright (c) 2026 Bojan Novković <bnovkov@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/types.h>
#include <sys/taskqueue.h>
#include <sys/module.h>

#include <machine/bus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>
#include <dev/ofw/ofw_subr.h>
#include <dev/clk/clk.h>
#include <dev/ofw/openfirm.h>

#include <dev/hwreset/hwreset.h>
#include <dev/phy/phy.h>
#include <dev/mmc/bridge.h>
#include <dev/mmc/mmcbrvar.h>
#include <dev/mmc/mmcreg.h>

#include <dev/sdhci/sdhci.h>
#include <dev/sdhci/sdhci_fdt.h>

#include "mmcbr_if.h"
#include "sdhci_if.h"

#include "opt_mmccam.h"
#include "opt_soc.h"

static struct ofw_compat_data compat_data[] = {
	{ "spacemit,k1-sdhci", 1 },
	{ NULL, 0 }
};

static int
sdhci_fdt_spacemit_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_search_compatible(dev, compat_data)->ocd_data == 0)
		return (ENXIO);

	if (ofw_bus_has_prop(dev, "no-sd")) {
		/* XXX: only attach to the SD card for now. */
		device_printf(dev, "MMC and SDIO aren't supported yet\n");
		return (ENXIO);
	}

	device_set_desc(dev, "SpacemiT K1 SDHCI controller");

	return (BUS_PROBE_DEFAULT);
}

static int
sdhci_fdt_spacemit_attach(device_t dev)
{
	struct sdhci_fdt_softc *sc;
	clk_t clk_core;
	hwreset_t rst;

	sc = device_get_softc(dev);
	sc->quirks = SDHCI_QUIRK_PRESET_VALUE_BROKEN |
		SDHCI_QUIRK_BROKEN_TIMEOUT_VAL;

	if (clk_get_by_ofw_name(dev, 0, "core", &clk_core)) {
		device_printf(dev, "cannot get core clock\n");
		return (ENXIO);
	}
	if (clk_get_by_ofw_name(dev, 0, "io", &sc->clk_core)) {
		device_printf(dev, "cannot get io clock\n");
		return (ENXIO);
	}
	if (hwreset_get_by_ofw_name(dev, 0, "sdh", &rst) != 0) {
		device_printf(dev, "cannot get device reset\n");
		return (ENXIO);
	}

	if (hwreset_deassert(rst) != 0) {
		device_printf(dev, "cannot reset device\n");
		return (ENXIO);
	}
	if (clk_enable(clk_core) != 0) {
		device_printf(dev, "cannot enable core clock\n");
		return (ENXIO);
	}
	if (clk_enable(sc->clk_core) != 0) {
		device_printf(dev, "cannot enable io clock\n");
		return (ENXIO);
	}

	return (sdhci_fdt_attach(dev));
}

static int
sdhci_fdt_spacemit_set_clock(device_t dev, struct sdhci_slot *slot, int clock)
{
	struct sdhci_fdt_softc *sc;
	uint64_t freq;

	sc = device_get_softc(dev);

	clk_set_freq(sc->clk_core, clock, CLK_SET_ROUND_ANY);
	clk_get_freq(sc->clk_core, &freq);

	return ((int)freq);
}

static device_method_t sdhci_fdt_spacemit_methods[] = {
	/* device_if */
	DEVMETHOD(device_probe,		sdhci_fdt_spacemit_probe),
	DEVMETHOD(device_attach,	sdhci_fdt_spacemit_attach),

	DEVMETHOD(sdhci_set_clock,	sdhci_fdt_spacemit_set_clock),
	DEVMETHOD_END
};
extern driver_t sdhci_fdt_driver;

DEFINE_CLASS_1(sdhci_spacemit, sdhci_fdt_spacemit_driver, sdhci_fdt_spacemit_methods,
    sizeof(struct sdhci_fdt_softc), sdhci_fdt_driver);
DRIVER_MODULE(sdhci_spacemit, simplebus, sdhci_fdt_spacemit_driver, NULL, NULL);

#ifndef MMCCAM
MMC_DECLARE_BRIDGE(sdhci_fdt_spacemit);
#endif
