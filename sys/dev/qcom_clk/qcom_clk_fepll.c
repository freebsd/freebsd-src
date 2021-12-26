/*-
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>.
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
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <machine/bus.h>

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include "qcom_clk_fepll.h"

#include "clkdev_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, "cpufreq_dt: " msg);
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * This is the top-level PLL clock on the IPQ4018/IPQ4019.
 * It's a fixed PLL clock that feeds a bunch of divisors into
 * downstrem FEPLL* and DDR clocks.
 *
 * Now, on Linux the clock code creates multiple instances of this
 * with an inbuilt divisor.  Here instead there'll be a single
 * instance of the FEPLL, and then normal divisors will feed into
 * the multiple PLL nodes.
 */

struct qcom_clk_fepll_sc {
	struct clknode	*clknode;
	uint32_t offset;
	uint32_t fdbkdiv_shift; /* FDBKDIV base */
	uint32_t fdbkdiv_width; /* FDBKDIV width */
	uint32_t refclkdiv_shift; /* REFCLKDIV base */
	uint32_t refclkdiv_width; /* REFCLKDIV width */
};

static int
qcom_clk_fepll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct qcom_clk_fepll_sc *sc;
	uint64_t vco, parent_rate;
	uint32_t reg, fdbkdiv, refclkdiv;

	sc = clknode_get_softc(clk);

	if (freq == NULL || *freq == 0) {
		device_printf(clknode_get_device(sc->clknode),
		    "%s: called; NULL or 0 frequency\n",
		    __func__);
		return (ENXIO);
	}

	parent_rate = *freq;

	CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->offset, &reg);
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));

	fdbkdiv = (reg >> sc->fdbkdiv_shift) &
	    ((1U << sc->fdbkdiv_width) - 1);
	refclkdiv = (reg >> sc->refclkdiv_shift) &
	    ((1U << sc->refclkdiv_width) - 1);

	vco = parent_rate / refclkdiv;
	vco = vco * 2;
	vco = vco * fdbkdiv;

	*freq = vco;
	return (0);
}

static int
qcom_clk_fepll_init(struct clknode *clk, device_t dev)
{

	/*
	 * There's only a single parent here for an FEPLL, so just set it
	 * to 0; the caller doesn't need to supply it.
	 */
	clknode_init_parent_idx(clk, 0);

	return (0);
}

static clknode_method_t qcom_clk_fepll_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		qcom_clk_fepll_init),
	CLKNODEMETHOD(clknode_recalc_freq,	qcom_clk_fepll_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(qcom_clk_fepll, qcom_clk_fepll_class, qcom_clk_fepll_methods,
   sizeof(struct qcom_clk_fepll_sc), clknode_class);

int
qcom_clk_fepll_register(struct clkdom *clkdom,
    struct qcom_clk_fepll_def *clkdef)
{
	struct clknode *clk;
	struct qcom_clk_fepll_sc *sc;

	clk = clknode_create(clkdom, &qcom_clk_fepll_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->clknode = clk;

	sc->offset = clkdef->offset;
	sc->fdbkdiv_shift = clkdef->fdbkdiv_shift;
	sc->fdbkdiv_width = clkdef->fdbkdiv_width;
	sc->refclkdiv_shift = clkdef->refclkdiv_shift;
	sc->refclkdiv_width = clkdef->refclkdiv_width;

	clknode_register(clkdom, clk);

	return (0);
}
