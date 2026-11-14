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

/* Community 0: GPP_B, GPP_T, GPP_A */
static const struct intelgpio_padgroup tgllp_com0_groups[] = {
	{ .first_pad = 0, .npads = 26, .gpio_base = 0, .name = "GPP_B" },
	{ .first_pad = 26, .npads = 16, .gpio_base = 32, .name = "GPP_T" },
	{ .first_pad = 42, .npads = 25, .gpio_base = 64, .name = "GPP_A" },
};

/* Community 1: GPP_S, GPP_H, GPP_D, GPP_U, vGPIO */
static const struct intelgpio_padgroup tgllp_com1_groups[] = {
	{ .first_pad = 67, .npads = 8, .gpio_base = 96, .name = "GPP_S" },
	{ .first_pad = 75, .npads = 24, .gpio_base = 128, .name = "GPP_H" },
	{ .first_pad = 99, .npads = 21, .gpio_base = 160, .name = "GPP_D" },
	{ .first_pad = 120, .npads = 24, .gpio_base = 192, .name = "GPP_U" },
	{ .first_pad = 144, .npads = 27, .gpio_base = 224, .name = "vGPIO" },
};

/* Community 4: GPP_C, GPP_F, HVCMOS, GPP_E, JTAG */
static const struct intelgpio_padgroup tgllp_com4_groups[] = {
	{ .first_pad = 171, .npads = 24, .gpio_base = 256, .name = "GPP_C" },
	{ .first_pad = 195, .npads = 25, .gpio_base = 288, .name = "GPP_F" },
	{ .first_pad = 220, .npads = 6, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "HVCMOS" },
	{ .first_pad = 226, .npads = 25, .gpio_base = 320, .name = "GPP_E" },
	{ .first_pad = 251, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "JTAG" },
};

/* Community 5: GPP_R, SPI */
static const struct intelgpio_padgroup tgllp_com5_groups[] = {
	{ .first_pad = 260, .npads = 8, .gpio_base = 352, .name = "GPP_R" },
	{ .first_pad = 268, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "SPI" },
};

static const struct intelgpio_community tgllp_communities[] = {
	{ .ngroups = nitems(tgllp_com0_groups), .groups = tgllp_com0_groups },
	{ .ngroups = nitems(tgllp_com1_groups), .groups = tgllp_com1_groups },
	{ .ngroups = nitems(tgllp_com4_groups), .groups = tgllp_com4_groups },
	{ .ngroups = nitems(tgllp_com5_groups), .groups = tgllp_com5_groups },
};

static char *tgllp_hids[] = { "INT34C5", "INTC1055", NULL };

static const struct intelgpio_platform tgllp_platform = {
	.communities = tgllp_communities,
	.ncommunities = nitems(tgllp_communities),
	.hids = tgllp_hids,
	.desc = "Intel Tiger Lake-LP GPIO",
};

static int
tgllp_gpio_probe(device_t dev)
{
	return (intelgpio_probe(dev, &tgllp_platform));
}

static int
tgllp_gpio_attach(device_t dev)
{
	return (intelgpio_attach(dev, &tgllp_platform));
}

static device_method_t tgllp_methods[] = {
	DEVMETHOD(device_probe, tgllp_gpio_probe),
	DEVMETHOD(device_attach, tgllp_gpio_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, tgllp_driver, tgllp_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(tgllpgpio, acpi, tgllp_driver, NULL, NULL);
MODULE_DEPEND(tgllpgpio, acpi, 1, 1, 1);
MODULE_DEPEND(tgllpgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(tgllpgpio, 1);
ACPI_PNP_INFO(tgllp_hids);
