/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
 *
 * Copyright (c) 2026 The FreeBSD Foundation.
 *
 * Portions of this file were written by Tom Jones <thj@freebsd.org> under
 * sponsorship from The FreeBSD Foundation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <arm/allwinner/allwinner_pinctrl.h>

#include "opt_soc.h"

static const struct allwinner_pins h616_r_pins[] = {
	{ "PL0" , 0, 0, { "gpio-in", "gpio_out", NULL, "spi0", NULL, NULL, NULL }, 0, 0, 0 },
	{ "PL1" , 0, 1, { "gpio-in", "gpio_out", NULL, "spi0", NULL, NULL, NULL }, 0, 0, 0 },
};

const struct allwinner_padconf h616_r_padconf = {
	.npins = nitems(h616_r_pins),
	.pins = h616_r_pins,
};
