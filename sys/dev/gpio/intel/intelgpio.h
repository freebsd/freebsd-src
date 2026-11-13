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

#define INTELGPIO_REVID_REG	      0x000
#define INTELGPIO_REVID_SHIFT	      16
#define INTELGPIO_REVID_MASK	      0xffff0000
#define INTELGPIO_PADBAR_REG	      0x00c

/*
 * Communities with REVID >= 0x94 use four 32-bit PADCFG registers per pad,
 * older ones (e.g. Broxton / Apollo Lake) only two.
 */
#define INTELGPIO_MIN_REVID_FOR_PADCFG_4DW	0x94
#define INTELGPIO_PAD_SIZE_2DW	      8
#define INTELGPIO_PAD_SIZE_4DW	      16

#define INTELGPIO_PADCFG0_GPIOTXSTATE (1 << 0)
#define INTELGPIO_PADCFG0_GPIORXSTATE (1 << 1)
#define INTELGPIO_PADCFG0_GPIOTXDIS   (1 << 8)
#define INTELGPIO_PADCFG0_GPIORXDIS   (1 << 9)
#define INTELGPIO_PADCFG0_PMODE_MASK  (0x7 << 10)
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
	const struct intelgpio_platform *sc_plat;

	struct resource *sc_mem_res[INTELGPIO_MAX_COMMUNITIES];
	uint32_t sc_padbar[INTELGPIO_MAX_COMMUNITIES];
	uint32_t sc_padsize[INTELGPIO_MAX_COMMUNITIES];
};

DECLARE_CLASS(intelgpio_driver);

int intelgpio_probe(device_t dev, const struct intelgpio_platform *plat);
int intelgpio_attach(device_t dev, const struct intelgpio_platform *plat);

#endif /* _DEV_GPIO_INTEL_INTELGPIO_H_ */
