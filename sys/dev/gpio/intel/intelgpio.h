/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Beckhoff Automation GmbH & Co. KG
 */

#ifndef _DEV_GPIO_INTEL_INTELGPIO_H_
#define _DEV_GPIO_INTEL_INTELGPIO_H_

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/gpio.h>
#include <sys/kernel.h>

#include <contrib/dev/acpica/include/acpi.h>

#define INTELGPIO_PADBAR_REG	      0x00c
#define INTELGPIO_PAD_SIZE	      16

#define INTELGPIO_PADCFG0_GPIOTXSTATE (1 << 0)
#define INTELGPIO_PADCFG0_GPIORXSTATE (1 << 1)
#define INTELGPIO_PADCFG0_GPIOTXDIS   (1 << 8)
#define INTELGPIO_PADCFG0_GPIORXDIS   (1 << 9)
#define INTELGPIO_PADCFG0_PMODE_MASK  (0xf << 10)
#define INTELGPIO_PADCFG0_PMODE_GPIO  0

#define INTELGPIO_GPIO_NOMAP	      (-1)
#define INTELGPIO_MAX_COMMUNITIES     8

struct intelgpio_padgroup {
	int first_pad;
	int npads;
	int gpio_base;
	const char *name;
};

struct intelgpio_community {
	int ngroups;
	const struct intelgpio_padgroup *groups;
};

struct intelgpio_platform {
	const struct intelgpio_community *communities;
	int ncommunities;
	char **hids;
	const char *desc;
};

struct intelgpio_softc {
	device_t sc_dev;
	device_t sc_busdev;
	struct mtx sc_mtx;
	ACPI_HANDLE sc_handle;
	const struct intelgpio_platform *sc_plat;

	struct resource *sc_mem_res[INTELGPIO_MAX_COMMUNITIES];
	uint32_t sc_padbar[INTELGPIO_MAX_COMMUNITIES];
};

/*
 * The intelgpio framework provides a common implementation for Intel GPIO
 * controllers. Most of the gpio(4) interface methods (pin_max, pin_get,
 * pin_set, etc.) as well as device_detach are already implemented in
 * intelgpio.c and included via the INTELGPIO_DEVMETHODS macro.
 *
 * To add support for a new Intel GPIO controller, a driver only needs to
 * define a struct intelgpio_platform describing the hardware and provide
 * thin probe/attach wrappers that pass this platform data to the functions
 * below. The platform structure contains:
 *   - communities: array of pad communities, each with its pad groups
 *     describing the pad numbering, GPIO base mapping, and group names.
 *   - ncommunities: number of communities (one MMIO resource per community).
 *   - hids: NULL-terminated array of ACPI hardware IDs to match against.
 *   - desc: human-readable device description string.
 *
 * If a controller has special requirements (e.g. additional resource setup,
 * interrupt configuration, or non-standard probing), the driver may provide
 * its own probe/attach instead of calling these.
 */
DECLARE_CLASS(intelgpio_driver);

int intelgpio_probe(device_t dev, const struct intelgpio_platform *plat);
int intelgpio_attach(device_t dev, const struct intelgpio_platform *plat);

#endif /* _DEV_GPIO_INTEL_INTELGPIO_H_ */
