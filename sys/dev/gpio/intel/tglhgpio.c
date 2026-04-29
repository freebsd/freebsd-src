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

/* Community 0: GPP_A, GPP_R, GPP_B, vGPIO_0 */
static const struct intelgpio_padgroup tglh_com0_groups[] = {
	{ .first_pad = 0, .npads = 25, .gpio_base = 0, .name = "GPP_A" },
	{ .first_pad = 25, .npads = 20, .gpio_base = 32, .name = "GPP_R" },
	{ .first_pad = 45, .npads = 26, .gpio_base = 64, .name = "GPP_B" },
	{ .first_pad = 71, .npads = 8, .gpio_base = 96, .name = "vGPIO_0" },
};

/* Community 1: GPP_D, GPP_C, GPP_S, GPP_G, vGPIO */
static const struct intelgpio_padgroup tglh_com1_groups[] = {
	{ .first_pad = 79, .npads = 26, .gpio_base = 128, .name = "GPP_D" },
	{ .first_pad = 105, .npads = 24, .gpio_base = 160, .name = "GPP_C" },
	{ .first_pad = 129, .npads = 8, .gpio_base = 192, .name = "GPP_S" },
	{ .first_pad = 137, .npads = 17, .gpio_base = 224, .name = "GPP_G" },
	{ .first_pad = 154, .npads = 27, .gpio_base = 256, .name = "vGPIO" },
};

/* Community 3: GPP_E, GPP_F */
static const struct intelgpio_padgroup tglh_com3_groups[] = {
	{ .first_pad = 181, .npads = 13, .gpio_base = 288, .name = "GPP_E" },
	{ .first_pad = 194, .npads = 24, .gpio_base = 320, .name = "GPP_F" },
};

/* Community 4: GPP_H, GPP_J, GPP_K */
static const struct intelgpio_padgroup tglh_com4_groups[] = {
	{ .first_pad = 218, .npads = 24, .gpio_base = 352, .name = "GPP_H" },
	{ .first_pad = 242, .npads = 10, .gpio_base = 384, .name = "GPP_J" },
	{ .first_pad = 252, .npads = 15, .gpio_base = 416, .name = "GPP_K" },
};

/* Community 5: GPP_I, JTAG */
static const struct intelgpio_padgroup tglh_com5_groups[] = {
	{ .first_pad = 267, .npads = 15, .gpio_base = 448, .name = "GPP_I" },
	{ .first_pad = 282,
	    .npads = 9,
	    .gpio_base = INTELGPIO_GPIO_NOMAP,
	    .name = "JTAG" },
};

static const struct intelgpio_community tglh_communities[] = {
	{ .ngroups = nitems(tglh_com0_groups), .groups = tglh_com0_groups },
	{ .ngroups = nitems(tglh_com1_groups), .groups = tglh_com1_groups },
	{ .ngroups = nitems(tglh_com3_groups), .groups = tglh_com3_groups },
	{ .ngroups = nitems(tglh_com4_groups), .groups = tglh_com4_groups },
	{ .ngroups = nitems(tglh_com5_groups), .groups = tglh_com5_groups },
};

static char *tglh_hids[] = { "INT34C6", NULL };

static const struct intelgpio_platform tglh_platform = {
	.communities = tglh_communities,
	.ncommunities = nitems(tglh_communities),
	.hids = tglh_hids,
	.desc = "Intel Tiger Lake-H GPIO",
};

static int
tglh_gpio_probe(device_t dev)
{
	return (intelgpio_probe(dev, &tglh_platform));
}

static int
tglh_gpio_attach(device_t dev)
{
	return (intelgpio_attach(dev, &tglh_platform));
}

static device_method_t tglh_methods[] = {
	DEVMETHOD(device_probe, tglh_gpio_probe),
	DEVMETHOD(device_attach, tglh_gpio_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(intelgpio, tglh_driver, tglh_methods,
    sizeof(struct intelgpio_softc), intelgpio_driver);

DRIVER_MODULE(tglhgpio, acpi, tglh_driver, NULL, NULL);
MODULE_DEPEND(tglhgpio, acpi, 1, 1, 1);
MODULE_DEPEND(tglhgpio, gpiobus, 1, 1, 1);
MODULE_VERSION(tglhgpio, 1);
