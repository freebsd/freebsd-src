/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2022 Semihalf.
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
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk.h>

#include <arm/mv/clk/armada38x_gen.h>

#include "clkdev_if.h"

#define SAR_A38X_TCLK_FREQ_SHIFT	15
#define SAR_A38X_TCLK_FREQ_MASK		0x00008000

#define TCLK_250MHZ					250 * 1000 * 1000
#define TCLK_200MHZ					200 * 1000 * 1000

#define	WR4(_clk, offset, val)					\
	CLKDEV_WRITE_4(clknode_get_device(_clk), offset, val)
#define	RD4(_clk, offset, val)					\
	CLKDEV_READ_4(clknode_get_device(_clk), offset, val)
#define	DEVICE_LOCK(_clk)					\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)					\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
armada38x_gen_recalc(struct clknode *clk, uint64_t *freq)
{
	uint32_t reg;

	DEVICE_LOCK(clk);
	RD4(clk, 0, &reg);
	DEVICE_UNLOCK(clk);

	reg = (reg & SAR_A38X_TCLK_FREQ_MASK) >> SAR_A38X_TCLK_FREQ_SHIFT;
	*freq = reg ? TCLK_200MHZ : TCLK_250MHZ;

	return (0);
}

static int
armada38x_gen_init(struct clknode *clk, device_t dev)
{
	return (0);
}

static clknode_method_t armada38x_gen_clknode_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		armada38x_gen_init),
	CLKNODEMETHOD(clknode_recalc_freq,	armada38x_gen_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(armada38x_gen_clknode, armada38x_gen_clknode_class,
    armada38x_gen_clknode_methods, 0, clknode_class);

int
armada38x_gen_register(struct clkdom *clkdom, const struct armada38x_gen_clknode_def *clkdef)
{
	struct clknode *clk;

	clk = clknode_create(clkdom, &armada38x_gen_clknode_class, &clkdef->def);
	if (clk == NULL)
		return (1);

	clknode_register(clkdom, clk);

	return(0);
}
