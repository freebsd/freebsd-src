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

static const struct intelgpio_padgroup ehl_com0_groups[] = {
	{ .first_pad =  0, .npads = 26, .gpio_base =  0, .name = "GPP_B" },
	{ .first_pad = 26, .npads = 16, .gpio_base = 26, .name = "GPP_T" },
	{ .first_pad = 42, .npads = 25, .gpio_base = 42, .name = "GPP_G" },
};

static const struct intelgpio_padgroup ehl_com1_groups[] = {
	{ .first_pad =  0, .npads = 16, .gpio_base =  0, .name = "GPP_V" },
	{ .first_pad = 16, .npads = 24, .gpio_base = 16, .name = "GPP_H" },
	{ .first_pad = 40, .npads = 21, .gpio_base = 40, .name = "GPP_D" },
	{ .first_pad = 61, .npads = 24, .gpio_base = 61, .name = "GPP_U" },
	{ .first_pad = 85, .npads = 28, .gpio_base = 85, .name = "vGPIO" },
};

static const struct intelgpio_padgroup ehl_com2_groups[] = {
	{
		.first_pad = 0,
		.npads = 17,
		.gpio_base = INTELGPIO_GPIO_NOMAP,
		.name = "DSW",
	},
};

static const struct intelgpio_padgroup ehl_com3_groups[] = {
	{ .first_pad = 0,
	    .npads = 17,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "CPU", },
	{ .first_pad = 17, .npads =  2, .gpio_base = 17, .name = "GPP_S" },
	{ .first_pad = 19, .npads = 24, .gpio_base = 19, .name = "GPP_A" },
	{ .first_pad = 43, .npads =  4, .gpio_base = 43, .name = "vGPIO_3" },
};

static const struct intelgpio_padgroup ehl_com4_groups[] = {
	{ .first_pad =  0, .npads = 24, .gpio_base =  0, .name = "GPP_C" },
	{ .first_pad = 24, .npads = 25, .gpio_base = 24, .name = "GPP_F" },
	{ .first_pad = 49,
	    .npads =  6,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "HVCMOS" },
	{ .first_pad = 55, .npads = 25, .gpio_base = 55, .name = "GPP_E" },
};

static const struct intelgpio_padgroup ehl_com5_groups[] = {
	{ .first_pad = 0, .npads = 8, .gpio_base = 0, .name = "GPP_R" },
};

static const struct intelgpio_community ehl_communities[] = {
	{ .ngroups = nitems(ehl_com0_groups), .groups = ehl_com0_groups },
	{ .ngroups = nitems(ehl_com1_groups), .groups = ehl_com1_groups },
	{ .ngroups = nitems(ehl_com2_groups), .groups = ehl_com2_groups },
	{ .ngroups = nitems(ehl_com3_groups), .groups = ehl_com3_groups },
	{ .ngroups = nitems(ehl_com4_groups), .groups = ehl_com4_groups },
	{ .ngroups = nitems(ehl_com5_groups), .groups = ehl_com5_groups },
};

static char *ehl_hids[] = {
	"INTC1020",
	NULL
};

/* ACPI exposes each EHL GPIO community as a separate device by _UID. */
static const struct intelgpio_platform ehl_platforms[] = {
	{
		.communities = &ehl_communities[0],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 0",
	}, {
		.communities = &ehl_communities[1],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 1",
	}, {
		.communities = &ehl_communities[2],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 2",
	}, {
		.communities = &ehl_communities[3],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 3",
	}, {
		.communities = &ehl_communities[4],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 4",
	}, {
		.communities = &ehl_communities[5],
		.ncommunities = 1,
		.hids = ehl_hids,
		.desc = "Intel Elkhart Lake GPIO community 5",
	}
};

static const struct intelgpio_platform *
ehl_get_platform(device_t dev)
{
	ACPI_HANDLE handle;
	ACPI_STATUS status;
	int uid;

	handle = acpi_get_handle(dev);
	status = acpi_GetInteger(handle, "_UID", &uid);
	if (ACPI_FAILURE(status) || uid < 0 || uid >= nitems(ehl_platforms)) {
		device_printf(dev, "invalid or missing _UID\n");
		return (NULL);
	}

	return (&ehl_platforms[uid]);
}

static int
ehl_probe(device_t dev)
{
	const struct intelgpio_platform *plat;

	plat = ehl_get_platform(dev);
	if (plat == NULL)
		return (ENXIO);

	return (intelgpio_probe(dev, plat));
}

static int
ehl_attach(device_t dev)
{
	const struct intelgpio_platform *plat;

	plat = ehl_get_platform(dev);
	if (plat == NULL)
		return (ENXIO);

	return (intelgpio_attach(dev, plat));
}

static device_method_t ehl_methods[] = {
	DEVMETHOD(device_probe,		ehl_probe),
	DEVMETHOD(device_attach,	ehl_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, ehl_driver, ehl_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(ehlgpio, acpi, ehl_driver, NULL, NULL);
MODULE_DEPEND(ehlgpio, acpi, 1, 1, 1);
MODULE_DEPEND(ehlgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(ehlgpio, 1);
ACPI_PNP_INFO(ehl_hids);
