/*
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_CLK_FIXED_H_
#define	_DEV_CLK_FIXED_H_

#include <dev/clk/clk.h>
#include <dev/gpio/gpiobusvar.h>

/*
 * A fixed clock can represent several different real-world objects, including
 * an oscillator with a fixed output frequency, a fixed divider (multiplier and
 * divisor must both be > 0), or a phase-fractional divider within a PLL
 * (however the code currently divides first, then multiplies, potentially
 * leading to different roundoff errors than the hardware PLL).
 */

struct clk_fixed_def {
	struct clknode_init_def clkdef;
	uint64_t		freq;
	uint32_t		mult;
	uint32_t		div;
	int			fixed_flags;
	struct gpiobus_pin	*gpio_pin;
	bool			have_gpio;
};

int clknode_fixed_register(struct clkdom *clkdom, struct clk_fixed_def *clkdef);

#endif	/*_DEV_CLK_FIXED_H_*/
