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

#include "qcom_clk_ro_div.h"

#include "clkdev_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, "cpufreq_dt: " msg);
#else
#define DPRINTF(dev, msg...)
#endif

/*
 * This is a read-only divisor table node.
 * It represents some divisor that is setup by the boot environment
 * and we don't have any need for the driver to go and fiddle with.
 *
 * It likely should just live in the extres/clk code.
 */

struct qcom_clk_ro_div_sc {
	struct clknode	*clknode;
	uint32_t offset;
	uint32_t shift;
	uint32_t width;
	struct qcom_clk_ro_div_tbl *div_tbl;
};

static int
qcom_clk_ro_div_recalc(struct clknode *clk, uint64_t *freq)
{
	struct qcom_clk_ro_div_sc *sc;
	uint32_t reg, idx, div = 1;
	int i;

	sc = clknode_get_softc(clk);

	if (freq == NULL || *freq == 0) {
		printf("%s: called; NULL or 0 frequency\n", __func__);
		return (ENXIO);
	}

	CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->offset, &reg);
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));

	idx = (reg >> sc->shift) & ((1U << sc->width) - 1);

	for (i = 0; (sc->div_tbl[i].div != 0); i++) {
		if (idx == sc->div_tbl[i].val) {
			div = sc->div_tbl[i].div;
			break;
		}
	}

	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: freq=%llu, idx=%u, div=%u, out_freq=%llu\n",
	    __func__,
	    *freq,
	    idx,
	    div,
	    *freq / div);

	*freq = *freq / div;
	return (0);
}

static int
qcom_clk_ro_div_init(struct clknode *clk, device_t dev)
{

	/*
	 * There's only a single parent here for this divisor,
	 * so just set it to 0; the caller doesn't need to supply it.
	 */
	clknode_init_parent_idx(clk, 0);

	return (0);
}

static clknode_method_t qcom_clk_ro_div_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		qcom_clk_ro_div_init),
	CLKNODEMETHOD(clknode_recalc_freq,	qcom_clk_ro_div_recalc),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(qcom_clk_fepll, qcom_clk_ro_div_class,
   qcom_clk_ro_div_methods, sizeof(struct qcom_clk_ro_div_sc),
   clknode_class);

int
qcom_clk_ro_div_register(struct clkdom *clkdom,
    struct qcom_clk_ro_div_def *clkdef)
{
	struct clknode *clk;
	struct qcom_clk_ro_div_sc *sc;

	clk = clknode_create(clkdom, &qcom_clk_ro_div_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->clknode = clk;
	sc->offset = clkdef->offset;
	sc->shift = clkdef->shift;
	sc->width = clkdef->width;
	sc->div_tbl = clkdef->div_tbl;

	clknode_register(clkdom, clk);

	return (0);
}
