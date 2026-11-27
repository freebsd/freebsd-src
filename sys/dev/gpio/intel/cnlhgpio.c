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

/* Community 0: GPP_A, GPP_B */
static const struct intelgpio_padgroup cnlh_com0_groups[] = {
	{ .first_pad = 0, .npads = 25, .gpio_base = 0, .name = "GPP_A" },
	{ .first_pad = 25, .npads = 26, .gpio_base = 32, .name = "GPP_B" },
};

/* Community 1: GPP_C, GPP_D, GPP_G, AZA, vGPIO_0, vGPIO_1 */
static const struct intelgpio_padgroup cnlh_com1_groups[] = {
	{ .first_pad = 51, .npads = 24, .gpio_base = 64, .name = "GPP_C" },
	{ .first_pad = 75, .npads = 24, .gpio_base = 96, .name = "GPP_D" },
	{ .first_pad = 99, .npads = 8, .gpio_base = 128, .name = "GPP_G" },
	{ .first_pad = 107, .npads = 8, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "AZA" },
	{ .first_pad = 115, .npads = 32, .gpio_base = 160, .name = "vGPIO_0" },
	{ .first_pad = 147, .npads = 8, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "vGPIO_1" },
};

/* Community 3: GPP_K, GPP_H, GPP_E, GPP_F, SPI */
static const struct intelgpio_padgroup cnlh_com3_groups[] = {
	{ .first_pad = 155, .npads = 24, .gpio_base = 192, .name = "GPP_K" },
	{ .first_pad = 179, .npads = 24, .gpio_base = 224, .name = "GPP_H" },
	{ .first_pad = 203, .npads = 13, .gpio_base = 256, .name = "GPP_E" },
	{ .first_pad = 216, .npads = 24, .gpio_base = 288, .name = "GPP_F" },
	{ .first_pad = 240, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "SPI" },
};

/* Community 4: CPU, JTAG, GPP_I, GPP_J */
static const struct intelgpio_padgroup cnlh_com4_groups[] = {
	{ .first_pad = 249, .npads = 11, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "CPU" },
	{ .first_pad = 260, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "JTAG" },
	{ .first_pad = 269, .npads = 18, .gpio_base = 320, .name = "GPP_I" },
	{ .first_pad = 287, .npads = 12, .gpio_base = 352, .name = "GPP_J" },
};

static const struct intelgpio_community cnlh_communities[] = {
	{ .ngroups = nitems(cnlh_com0_groups), .groups = cnlh_com0_groups },
	{ .ngroups = nitems(cnlh_com1_groups), .groups = cnlh_com1_groups },
	{ .ngroups = nitems(cnlh_com3_groups), .groups = cnlh_com3_groups },
	{ .ngroups = nitems(cnlh_com4_groups), .groups = cnlh_com4_groups },
};

static char *cnlh_hids[] = { "INT3450", NULL };

static const struct intelgpio_platform cnlh_platform = {
	.communities = cnlh_communities,
	.ncommunities = nitems(cnlh_communities),
	.hids = cnlh_hids,
	.desc = "Intel Cannon Lake-H GPIO",
};

static int
cnlh_probe(device_t dev)
{
	return (intelgpio_probe(dev, &cnlh_platform));
}

static int
cnlh_attach(device_t dev)
{
	return (intelgpio_attach(dev, &cnlh_platform));
}

static device_method_t cnlh_methods[] = {
	DEVMETHOD(device_probe, cnlh_probe),
	DEVMETHOD(device_attach, cnlh_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, cnlh_driver, cnlh_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(cnlhgpio, acpi, cnlh_driver, NULL, NULL);
MODULE_DEPEND(cnlhgpio, acpi, 1, 1, 1);
MODULE_DEPEND(cnlhgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(cnlhgpio, 1);
ACPI_PNP_INFO(cnlh_hids);
