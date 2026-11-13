/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Beckhoff Automation GmbH & Co. KG
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>
#include <sys/module.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>
#include <dev/gpio/gpiobusvar.h>

#include "intelgpio.h"

#include "gpio_if.h"
#include "opt_acpi.h"

/*
 * Apollo Lake exposes each GPIO community as a separate ACPI device, selected
 * by _UID starting at 1. The pads of a community are numbered contiguously,
 * so a single pad group per community is used and the GPIO pin number equals
 * the pad number.
 */
static const struct intelgpio_padgroup apl_north_groups[] = {
	{ .first_pad = 0, .npads = 78, .gpio_base = 0, .name = "N" },
};

static const struct intelgpio_padgroup apl_northwest_groups[] = {
	{ .first_pad = 0, .npads = 77, .gpio_base = 0, .name = "NW" },
};

static const struct intelgpio_padgroup apl_west_groups[] = {
	{ .first_pad = 0, .npads = 47, .gpio_base = 0, .name = "W" },
};

static const struct intelgpio_padgroup apl_southwest_groups[] = {
	{ .first_pad = 0, .npads = 43, .gpio_base = 0, .name = "SW" },
};

static const struct intelgpio_community apl_communities[] = {
	{ .ngroups = nitems(apl_north_groups), .groups = apl_north_groups },
	{ .ngroups = nitems(apl_northwest_groups),
	    .groups = apl_northwest_groups },
	{ .ngroups = nitems(apl_west_groups), .groups = apl_west_groups },
	{ .ngroups = nitems(apl_southwest_groups),
	    .groups = apl_southwest_groups },
};

static char *apl_hids[] = { "INT3452", NULL };

static const struct intelgpio_platform apl_platforms[] = {
	{
		.communities = &apl_communities[0],
		.ncommunities = 1,
		.hids = apl_hids,
		.desc = "Intel Apollo Lake GPIO (North)",
	}, {
		.communities = &apl_communities[1],
		.ncommunities = 1,
		.hids = apl_hids,
		.desc = "Intel Apollo Lake GPIO (Northwest)",
	}, {
		.communities = &apl_communities[2],
		.ncommunities = 1,
		.hids = apl_hids,
		.desc = "Intel Apollo Lake GPIO (West)",
	}, {
		.communities = &apl_communities[3],
		.ncommunities = 1,
		.hids = apl_hids,
		.desc = "Intel Apollo Lake GPIO (Southwest)",
	}
};

static const struct intelgpio_platform *
apl_get_platform(device_t dev)
{
	uint32_t uid;

	if (ACPI_FAILURE(acpi_GetInteger(acpi_get_handle(dev), "_UID",
	    &uid)) || uid < 1 || uid > nitems(apl_platforms)) {
		device_printf(dev, "invalid or missing _UID\n");
		return (NULL);
	}

	return (&apl_platforms[uid - 1]);
}

static int
apl_probe(device_t dev)
{
	const struct intelgpio_platform *plat;

	/* Check the HID first so that _UID is only evaluated for our device. */
	if (ACPI_ID_PROBE(device_get_parent(dev), dev, apl_hids, NULL) > 0)
		return (ENXIO);

	plat = apl_get_platform(dev);
	if (plat == NULL)
		return (ENXIO);

	return (intelgpio_probe(dev, plat));
}

static int
apl_attach(device_t dev)
{
	const struct intelgpio_platform *plat;

	plat = apl_get_platform(dev);
	if (plat == NULL)
		return (ENXIO);

	return (intelgpio_attach(dev, plat));
}

static device_method_t apl_methods[] = {
	DEVMETHOD(device_probe, apl_probe),
	DEVMETHOD(device_attach, apl_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, apl_driver, apl_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(aplgpio, acpi, apl_driver, NULL, NULL);
MODULE_DEPEND(aplgpio, acpi, 1, 1, 1);
MODULE_DEPEND(aplgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(aplgpio, 1);
ACPI_PNP_INFO(apl_hids);
