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

#include <dev/gpio/gpiobusvar.h>

#include "intelgpio.h"

#include "gpio_if.h"
#include "opt_acpi.h"

static const struct intelgpio_padgroup adln_com0_groups[] = {
	{ .first_pad = 0, .npads = 26, .gpio_base = 0, .name = "GPP_B" },
	{ .first_pad = 26, .npads = 16, .gpio_base = 32, .name = "GPP_T" },
	{ .first_pad = 42, .npads = 25, .gpio_base = 64, .name = "GPP_A" },
};

static const struct intelgpio_padgroup adln_com1_groups[] = {
	{ .first_pad = 67, .npads = 8, .gpio_base = 96, .name = "GPP_S" },
	{ .first_pad = 75, .npads = 20, .gpio_base = 128, .name = "GPP_I" },
	{ .first_pad = 95, .npads = 24, .gpio_base = 160, .name = "GPP_H" },
	{ .first_pad = 119, .npads = 21, .gpio_base = 192, .name = "GPP_D" },
	{ .first_pad = 140, .npads = 29, .gpio_base = 224, .name = "vGPIO" },
};

static const struct intelgpio_padgroup adln_com4_groups[] = {
	{ .first_pad = 169, .npads = 24, .gpio_base = 256, .name = "GPP_C" },
	{ .first_pad = 193, .npads = 25, .gpio_base = 288, .name = "GPP_F" },
	{ .first_pad = 218,
	    .npads = 6,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "HVCMOS" },
	{ .first_pad = 224, .npads = 25, .gpio_base = 320, .name = "GPP_E" },
};

static const struct intelgpio_padgroup adln_com5_groups[] = {
	{ .first_pad = 249, .npads = 8, .gpio_base = 352, .name = "GPP_R" },
};

static const struct intelgpio_community adln_communities[] = {
	{ .ngroups = nitems(adln_com0_groups), .groups = adln_com0_groups },
	{ .ngroups = nitems(adln_com1_groups), .groups = adln_com1_groups },
	{ .ngroups = nitems(adln_com4_groups), .groups = adln_com4_groups },
	{ .ngroups = nitems(adln_com5_groups), .groups = adln_com5_groups },
};

static char *adln_hids[] = { "INTC1056", "INTC1057", "INTC1085", NULL };

static const struct intelgpio_platform adln_platform = {
	.communities = adln_communities,
	.ncommunities = nitems(adln_communities),
	.hids = adln_hids,
	.desc = "Intel Alder Lake-N GPIO",
};

static int
adln_probe(device_t dev)
{
	return (intelgpio_probe(dev, &adln_platform));
}

static int
adln_attach(device_t dev)
{
	return (intelgpio_attach(dev, &adln_platform));
}

static device_method_t adln_methods[] = {
	DEVMETHOD(device_probe, adln_probe),
	DEVMETHOD(device_attach, adln_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(intelgpio, adln_driver, adln_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(adlngpio, acpi, adln_driver, NULL, NULL);
MODULE_DEPEND(adlngpio, acpi, 1, 1, 1);
MODULE_DEPEND(adlngpio, gpiobus, 1, 1, 1);
MODULE_VERSION(adlngpio, 1);
