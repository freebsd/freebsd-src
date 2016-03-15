/*-
 * Copyright 2016 Michal Meloun <mmel@FreeBSD.org>
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");


#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/kobj.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/extres/clk/clk_fixed.h>

static int clknode_fixed_init(struct clknode *clk, device_t dev);
static int clknode_fixed_recalc(struct clknode *clk, uint64_t *freq);
struct clknode_fixed_sc {
	int		fixed_flags;
	uint64_t	freq;
	uint32_t	mult;
	uint32_t	div;
};

static clknode_method_t clknode_fixed_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,	   clknode_fixed_init),
	CLKNODEMETHOD(clknode_recalc_freq, clknode_fixed_recalc),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(clknode_fixed, clknode_fixed_class, clknode_fixed_methods,
   sizeof(struct clknode_fixed_sc), clknode_class);

static int
clknode_fixed_init(struct clknode *clk, device_t dev)
{
	struct clknode_fixed_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->freq == 0)
		clknode_init_parent_idx(clk, 0);
	return(0);
}

static int
clknode_fixed_recalc(struct clknode *clk, uint64_t *freq)
{
	struct clknode_fixed_sc *sc;

	sc = clknode_get_softc(clk);
	if (sc->freq != 0)
		*freq = sc->freq;
	else if ((sc->mult != 0) && (sc->div != 0))
		*freq = (*freq / sc->div) * sc->mult;
	else
		*freq = 0;
	return (0);
}

int
clknode_fixed_register(struct clkdom *clkdom, struct clk_fixed_def *clkdef)
{
	struct clknode *clk;
	struct clknode_fixed_sc *sc;

	if ((clkdef->freq == 0) && (clkdef->clkdef.parent_cnt == 0))
		panic("fixed clk: Frequency is not defined for clock source");
	clk = clknode_create(clkdom, &clknode_fixed_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->fixed_flags = clkdef->fixed_flags;
	sc->freq = clkdef->freq;
	sc->mult = clkdef->mult;
	sc->div = clkdef->div;

	clknode_register(clkdom, clk);
	return (0);
}
