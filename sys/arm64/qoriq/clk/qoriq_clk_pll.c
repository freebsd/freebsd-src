/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>

#include <arm64/qoriq/clk/qoriq_clkgen.h>

#include "clkdev_if.h"

struct qoriq_clk_pll_softc {
	bus_addr_t	offset;

	uint32_t	mask;
	uint32_t	shift;

	uint32_t	flags;
};

#define	WR4(_clk, offset, val)					\
	CLKDEV_WRITE_4(clknode_get_device(_clk), offset, val)
#define	RD4(_clk, offset, val)					\
	CLKDEV_READ_4(clknode_get_device(_clk), offset, val)
#define	DEVICE_LOCK(_clk)					\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)					\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#define	QORIQ_PLL_KILL_BIT	(1 << 31)

static int
qoriq_clk_pll_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static int
qoriq_clk_pll_recalc_freq(struct clknode *clk, uint64_t *freq)
{
	struct qoriq_clk_pll_softc *sc;
	uint32_t mul;

	sc = clknode_get_softc(clk);

	RD4(clk, sc->offset, &mul);

	if (sc->flags & QORIQ_CLK_PLL_HAS_KILL_BIT && mul & QORIQ_PLL_KILL_BIT)
		return (0);

	mul &= sc->mask;
	mul >>= sc->shift;

	*freq = *freq * mul;

	return (0);
}

static clknode_method_t qoriq_clk_pll_clknode_methods[] = {
	CLKNODEMETHOD(clknode_init,		qoriq_clk_pll_init),
	CLKNODEMETHOD(clknode_recalc_freq,	qoriq_clk_pll_recalc_freq),

	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(qoriq_clk_pll_clknode, qoriq_clk_pll_clknode_class,
    qoriq_clk_pll_clknode_methods, sizeof(struct qoriq_clk_pll_softc),
    clknode_class);

int
qoriq_clk_pll_register(struct clkdom *clkdom,
    const struct qoriq_clk_pll_def *clkdef)
{
	char namebuf[QORIQ_CLK_NAME_MAX_LEN];
	struct qoriq_clk_pll_softc *sc;
	struct clk_fixed_def def;
	const char *parent_name;
	struct clknode *clk;
	int error;
	int i;

	clk = clknode_create(clkdom, &qoriq_clk_pll_clknode_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->mask = clkdef->mask;
	sc->shift = clkdef->shift;
	sc->flags = clkdef->flags;
	sc->offset = clkdef->offset;

	clknode_register(clkdom, clk);

	parent_name = clkdef->clkdef.name;

	def.clkdef.parent_names = &parent_name;
	def.clkdef.parent_cnt = 1;
	def.clkdef.name = namebuf;
	def.mult = 1;
	def.freq = 0;

	i = 0;
	while (clkdef->dividers[i] != 0) {
		def.div = clkdef->dividers[i];
		def.clkdef.id = clkdef->clkdef.id + i;
		snprintf(namebuf, QORIQ_CLK_NAME_MAX_LEN, "%s_div%d",
		    parent_name, clkdef->dividers[i]);

		error = clknode_fixed_register(clkdom, &def);
		if (error != 0)
			return (error);

		i++;
	}

	return (0);
}
