/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2017 Emmanuel Vadot <manu@freebsd.org>
 *
 * Copyright (c) 2020 Oskar Holmlund <oskar.holmlund@ohdata.se>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef _TI_DPLL_CLOCK_H_
#define _TI_DPLL_CLOCK_H_

#include <dev/extres/clk/clk.h>

/* Registers are described in AM335x TRM chapter 8.1.12.2.* */

/* Register offsets */
#define CM_CLKSEL_DPLL_PERIPH			0x49C

/* CM_IDLEST_DPLL_xxx */
#define ST_MN_BYPASS_MASK	0x0100
#define ST_MN_BYPASS_SHIFT	8
#define ST_DPLL_CLK_MASK	0x0001

/* CM_CLKMODE_DPLL_DPLL_EN feature flag */
#define LOW_POWER_STOP_MODE_FLAG		0x01
#define MN_BYPASS_MODE_FLAG			0x02
#define IDLE_BYPASS_LOW_POWER_MODE_FLAG		0x04
#define IDLE_BYPASS_FAST_RELOCK_MODE_FLAG	0x08
#define LOCK_MODE_FLAG				0x10

/* CM_CLKMODE_DPLL_xxx */
#define DPLL_EN_LOW_POWER_STOP_MODE		0x01
#define DPLL_EN_MN_BYPASS_MODE			0x04
#define DPLL_EN_IDLE_BYPASS_LOW_POWER_MODE	0x05
#define DPLL_EN_IDLE_BYPASS_FAST_RELOCK_MODE	0x06
#define DPLL_EN_LOCK_MODE			0x07

#define TI_CLK_FACTOR_ZERO_BASED        0x0002
#define TI_CLK_FACTOR_FIXED             0x0008
#define TI_CLK_FACTOR_MIN_VALUE         0x0020
#define TI_CLK_FACTOR_MAX_VALUE         0x0040

/* Based on aw_clk_factor sys/arm/allwinner/clkng/aw_clk.h */
struct ti_clk_factor {
	uint32_t	shift;	/* Shift bits for the factor */
	uint32_t	mask;	/* Mask to get the factor */
	uint32_t	width;	/* Number of bits for the factor */
	uint32_t	value;	/* Fixed value */

	uint32_t        min_value;
	uint32_t        max_value;

	uint32_t	flags;	/* Flags */
};

struct ti_clk_dpll_def {
	struct clknode_init_def clkdef;

	uint32_t		ti_clkmode_offset; /* control */
	uint8_t			ti_clkmode_flags;

	uint32_t		ti_idlest_offset;

	uint32_t		ti_clksel_offset; /* mult-div1 */
	struct ti_clk_factor	ti_clksel_mult;
	struct ti_clk_factor	ti_clksel_div;

	uint32_t		ti_autoidle_offset;
};

int ti_clknode_dpll_register(struct clkdom *clkdom, struct ti_clk_dpll_def *clkdef);

#endif /* _TI_DPLL_CLOCK_H_ */
