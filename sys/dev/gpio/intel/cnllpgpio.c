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

/* Community 0: GPP_A, GPP_B, GPP_G, SPI */
static const struct intelgpio_padgroup cnllp_com0_groups[] = {
	{ .first_pad = 0, .npads = 25, .gpio_base = 0, .name = "GPP_A" },
	{ .first_pad = 25, .npads = 26, .gpio_base = 32, .name = "GPP_B" },
	{ .first_pad = 51, .npads = 8, .gpio_base = 64, .name = "GPP_G" },
	{ .first_pad = 59, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "SPI" },
};

/* Community 1: GPP_D, GPP_F, GPP_H, vGPIO_0, vGPIO_1 */
static const struct intelgpio_padgroup cnllp_com1_groups[] = {
	{ .first_pad = 68, .npads = 25, .gpio_base = 96, .name = "GPP_D" },
	{ .first_pad = 93, .npads = 24, .gpio_base = 128, .name = "GPP_F" },
	{ .first_pad = 117, .npads = 24, .gpio_base = 160, .name = "GPP_H" },
	{ .first_pad = 141, .npads = 32, .gpio_base = 192, .name = "vGPIO_0" },
	{ .first_pad = 173, .npads = 8, .gpio_base = 224, .name = "vGPIO_1" },
};

/* Community 4: GPP_C, GPP_E, JTAG, HVCMOS */
static const struct intelgpio_padgroup cnllp_com4_groups[] = {
	{ .first_pad = 181, .npads = 24, .gpio_base = 256, .name = "GPP_C" },
	{ .first_pad = 205, .npads = 24, .gpio_base = 288, .name = "GPP_E" },
	{ .first_pad = 229, .npads = 9, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "JTAG" },
	{ .first_pad = 238, .npads = 6, .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "HVCMOS" },
};

static const struct intelgpio_community cnllp_communities[] = {
	{ .ngroups = nitems(cnllp_com0_groups), .groups = cnllp_com0_groups },
	{ .ngroups = nitems(cnllp_com1_groups), .groups = cnllp_com1_groups },
	{ .ngroups = nitems(cnllp_com4_groups), .groups = cnllp_com4_groups },
};

static char *cnllp_hids[] = { "INT34BB", NULL };

static const struct intelgpio_platform cnllp_platform = {
	.communities = cnllp_communities,
	.ncommunities = nitems(cnllp_communities),
	.hids = cnllp_hids,
	.desc = "Intel Cannon Lake-LP GPIO",
};

static int
cnllp_probe(device_t dev)
{
	return (intelgpio_probe(dev, &cnllp_platform));
}

static int
cnllp_attach(device_t dev)
{
	return (intelgpio_attach(dev, &cnllp_platform));
}

static device_method_t cnllp_methods[] = {
	DEVMETHOD(device_probe, cnllp_probe),
	DEVMETHOD(device_attach, cnllp_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(gpio, cnllp_driver, cnllp_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(cnllpgpio, acpi, cnllp_driver, NULL, NULL);
MODULE_DEPEND(cnllpgpio, acpi, 1, 1, 1);
MODULE_DEPEND(cnllpgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(cnllpgpio, 1);
ACPI_PNP_INFO(cnllp_hids);
