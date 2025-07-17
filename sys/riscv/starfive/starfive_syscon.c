/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
 *
 * This software was developed by Mitchell Horne <mhorne@FreeBSD.org> under
 * sponsorship from the FreeBSD Foundation.
 */

/*
 * StarFive syscon driver.
 *
 * On the JH7110, the PLL clock driver is a child of the sys-syscon device.
 * This needs to probe very early (BUS_PASS_BUS + BUS_PASS_ORDER_EARLY).
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>

#include <machine/bus.h>

#include <dev/ofw/openfirm.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/syscon/syscon.h>
#include <dev/syscon/syscon_generic.h>

#include <dev/fdt/simple_mfd.h>

enum starfive_syscon_type {
	JH7110_SYSCON_SYS = 1,
	JH7110_SYSCON_AON,
	JH7110_SYSCON_STG,
};

static struct ofw_compat_data compat_data[] = {
	{ "starfive,jh7110-sys-syscon",	JH7110_SYSCON_SYS },
	{ "starfive,jh7110-aon-syscon",	JH7110_SYSCON_AON },
	{ "starfive,jh7110-stg-syscon",	JH7110_SYSCON_STG },

	{ NULL, 0 }
};

static int
starfive_syscon_probe(device_t dev)
{
	enum starfive_syscon_type type;

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	type = ofw_bus_search_compatible(dev, compat_data)->ocd_data;
	if (type == 0)
		return (ENXIO);

	switch (type) {
	case JH7110_SYSCON_SYS:
		device_set_desc(dev, "JH7110 SYS syscon");
		break;
	case JH7110_SYSCON_AON:
		device_set_desc(dev, "JH7110 AON syscon");
		break;
	case JH7110_SYSCON_STG:
		device_set_desc(dev, "JH7110 STG syscon");
		break;
	}

	return (BUS_PROBE_DEFAULT);
}


static device_method_t starfive_syscon_methods[] = {
	DEVMETHOD(device_probe, starfive_syscon_probe),

	DEVMETHOD_END
};

DEFINE_CLASS_1(starfive_syscon, starfive_syscon_driver, starfive_syscon_methods,
    sizeof(struct syscon_generic_softc), simple_mfd_driver);

EARLY_DRIVER_MODULE(starfive_syscon, simplebus, starfive_syscon_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_EARLY);
MODULE_VERSION(starfive_syscon, 1);
