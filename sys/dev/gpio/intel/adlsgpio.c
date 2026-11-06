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

static const struct intelgpio_padgroup adls_com0_groups[] = {
	{ .first_pad = 0, .npads = 25, .gpio_base = 0, .name = "GPP_I" },
	{ .first_pad = 25, .npads = 23, .gpio_base = 32, .name = "GPP_R" },
	{ .first_pad = 48, .npads = 12, .gpio_base = 64, .name = "GPP_J" },
	{ .first_pad = 60, .npads = 27, .gpio_base = 96, .name = "vGPIO" },
	{ .first_pad = 87, .npads = 8, .gpio_base = 128, .name = "vGPIO_0" },
};

static const struct intelgpio_padgroup adls_com1_groups[] = {
	{ .first_pad = 95, .npads = 24, .gpio_base = 160, .name = "GPP_B" },
	{ .first_pad = 119, .npads = 8, .gpio_base = 192, .name = "GPP_G" },
	{ .first_pad = 127, .npads = 24, .gpio_base = 224, .name = "GPP_H" },
};

static const struct intelgpio_padgroup adls_com2_groups[] = {
	{ .first_pad = 151,
	    .npads = 9,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "SPI0" },
	{ .first_pad = 160, .npads = 16, .gpio_base = 256, .name = "GPP_A" },
	{ .first_pad = 176, .npads = 24, .gpio_base = 288, .name = "GPP_C" },
};

static const struct intelgpio_padgroup adls_com3_groups[] = {
	{ .first_pad = 200, .npads = 8, .gpio_base = 320, .name = "GPP_S" },
	{ .first_pad = 208, .npads = 23, .gpio_base = 352, .name = "GPP_E" },
	{ .first_pad = 231, .npads = 15, .gpio_base = 384, .name = "GPP_K" },
	{ .first_pad = 246, .npads = 24, .gpio_base = 416, .name = "GPP_F" },
};

static const struct intelgpio_padgroup adls_com4_groups[] = {
	{ .first_pad = 270, .npads = 25, .gpio_base = 448, .name = "GPP_D" },
	{ .first_pad = 295,
	    .npads = 9,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "JTAG" },
};

static const struct intelgpio_community adls_communities[] = {
	{ .ngroups = nitems(adls_com0_groups), .groups = adls_com0_groups },
	{ .ngroups = nitems(adls_com1_groups), .groups = adls_com1_groups },
	{ .ngroups = nitems(adls_com2_groups), .groups = adls_com2_groups },
	{ .ngroups = nitems(adls_com3_groups), .groups = adls_com3_groups },
	{ .ngroups = nitems(adls_com4_groups), .groups = adls_com4_groups },
};

static char *adls_hids[] = { "INTC1056", "INTC1085", NULL };

static const struct intelgpio_platform adls_platform = {
	.communities = adls_communities,
	.ncommunities = nitems(adls_communities),
	.hids = adls_hids,
	.desc = "Intel Alder Lake-S GPIO",
};

static int
adls_probe(device_t dev)
{
	return (intelgpio_probe(dev, &adls_platform));
}

static int
adls_attach(device_t dev)
{
	return (intelgpio_attach(dev, &adls_platform));
}

static device_method_t adls_methods[] = {
	DEVMETHOD(device_probe, adls_probe),
	DEVMETHOD(device_attach, adls_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, adls_driver, adls_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(adlsgpio, acpi, adls_driver, NULL, NULL);
MODULE_DEPEND(adlsgpio, acpi, 1, 1, 1);
MODULE_DEPEND(adlsgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(adlsgpio, 1);
ACPI_PNP_INFO(adls_hids);
