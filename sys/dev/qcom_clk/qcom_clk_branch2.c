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

#include "qcom_clk_branch2.h"
#include "qcom_clk_branch2_reg.h"

#include "clkdev_if.h"

/*
 * This is a combination gate/status and dynamic hardware clock gating with
 * voting.
 */

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg);
#else
#define DPRINTF(dev, msg...)
#endif

struct qcom_clk_branch2_sc {
	struct clknode *clknode;
	uint32_t flags;
	uint32_t enable_offset;
	uint32_t enable_shift;
	uint32_t hwcg_reg;
	uint32_t hwcg_bit;
	uint32_t halt_reg;
	uint32_t halt_check_type;
	bool halt_check_voted;
};
#if 0
static bool
qcom_clk_branch2_get_gate_locked(struct qcom_clk_branch2_sc *sc)
{
	uint32_t reg;

	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->enable_offset,
	    &reg);

	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: offset=0x%x, reg=0x%x\n", __func__,
	    sc->enable_offset, reg);

	return (!! (reg & (1U << sc->enable_shift)));
}
#endif

static int
qcom_clk_branch2_init(struct clknode *clk, device_t dev)
{

	clknode_init_parent_idx(clk, 0);

	return (0);
}

static bool
qcom_clk_branch2_in_hwcg_mode_locked(struct qcom_clk_branch2_sc *sc)
{
	uint32_t reg;

	if (sc->hwcg_reg == 0)
		return (false);
	
	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->hwcg_reg,
	    &reg);

	return (!! (reg & (1U << sc->hwcg_bit)));
}

static bool
qcom_clk_branch2_check_halt_locked(struct qcom_clk_branch2_sc *sc, bool enable)
{
	uint32_t reg;

	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->halt_reg, &reg);

	if (enable) {
		/*
		 * The upstream Linux code is .. unfortunate.
		 *
		 * Here it says "return true if BRANCH_CLK_OFF is not set,
		 * or if the status field = FSM_STATUS_ON AND
		 * the clk_off field is 0.
		 *
		 * Which .. is weird, because I can't currently see
		 * how we'd ever need to check FSM_STATUS_ON - the only
		 * valid check for the FSM status also requires clk_off=0.
		 */
		return !! ((reg & QCOM_CLK_BRANCH2_CLK_OFF) == 0);
	} else {
		return !! (reg & QCOM_CLK_BRANCH2_CLK_OFF);
	}
}

/*
 * Check if the given type/voted flag match what is configured.
 */
static bool
qcom_clk_branch2_halt_check_type(struct qcom_clk_branch2_sc *sc,
    uint32_t type, bool voted)
{
	return ((sc->halt_check_type == type) &&
	    (sc->halt_check_voted == voted));
}

static bool
qcom_clk_branch2_wait_locked(struct qcom_clk_branch2_sc *sc, bool enable)
{

	if (qcom_clk_branch2_halt_check_type(sc,
	    QCOM_CLK_BRANCH2_BRANCH_HALT_SKIP, false))
		return (true);
	if (qcom_clk_branch2_in_hwcg_mode_locked(sc))
		return (true);

	if ((qcom_clk_branch2_halt_check_type(sc,
	      QCOM_CLK_BRANCH2_BRANCH_HALT_DELAY, false)) ||
	    (enable == false && sc->halt_check_voted)) {
		DELAY(10);
		return (true);
	}

	if ((qcom_clk_branch2_halt_check_type(sc,
	      QCOM_CLK_BRANCH2_BRANCH_HALT_INVERTED, false)) ||
	    (qcom_clk_branch2_halt_check_type(sc,
	      QCOM_CLK_BRANCH2_BRANCH_HALT, false)) ||
	    (enable && sc->halt_check_voted)) {
		int count;

		for (count = 0; count < 200; count++) {
			if (qcom_clk_branch2_check_halt_locked(sc, enable))
				return (true);
			DELAY(1);
		}
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: enable stuck (%d)!\n", __func__, enable);
		return (false);
	}

	/* Default */
	return (true);
}

static int
qcom_clk_branch2_set_gate(struct clknode *clk, bool enable)
{
	struct qcom_clk_branch2_sc *sc;
	uint32_t reg;

	sc = clknode_get_softc(clk);

	DPRINTF(clknode_get_device(sc->clknode), "%s: called\n", __func__);

	if (sc->enable_offset == 0) {
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: no enable_offset", __func__);
		return (ENXIO);
	}

	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: called; enable=%d\n", __func__, enable);

	CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
	CLKDEV_READ_4(clknode_get_device(sc->clknode), sc->enable_offset,
	    &reg);
	if (enable) {
		reg |= (1U << sc->enable_shift);
	} else {
		reg &= ~(1U << sc->enable_shift);
	}
	CLKDEV_WRITE_4(clknode_get_device(sc->clknode), sc->enable_offset,
	    reg);

	/*
	 * Now wait for the clock branch to update!
	 */
	if (! qcom_clk_branch2_wait_locked(sc, enable)) {
		CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: failed to wait!\n", __func__);
		return (ENXIO);
	}

	CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));

	return (0);
}

static int
qcom_clk_branch2_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct qcom_clk_branch2_sc *sc;

	sc = clknode_get_softc(clk);

	/* We only support what our parent clock is currently set as */
	*fout = fin;

	/* .. and stop here if we don't have SET_RATE_PARENT */
	if (sc->flags & QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT)
		*stop = 0;
	else
		*stop = 1;
	return (0);
}


static clknode_method_t qcom_clk_branch2_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		qcom_clk_branch2_init),
	CLKNODEMETHOD(clknode_set_gate,		qcom_clk_branch2_set_gate),
	CLKNODEMETHOD(clknode_set_freq,		qcom_clk_branch2_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(qcom_clk_branch2, qcom_clk_branch2_class,
    qcom_clk_branch2_methods, sizeof(struct qcom_clk_branch2_sc),
    clknode_class);

int
qcom_clk_branch2_register(struct clkdom *clkdom,
    struct qcom_clk_branch2_def *clkdef)
{
	struct clknode *clk;
	struct qcom_clk_branch2_sc *sc;

	if (clkdef->flags & QCOM_CLK_BRANCH2_FLAGS_CRITICAL)
		clkdef->clkdef.flags |= CLK_NODE_CANNOT_STOP;

	clk = clknode_create(clkdom, &qcom_clk_branch2_class,
	    &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->clknode = clk;

	sc->enable_offset = clkdef->enable_offset;
	sc->enable_shift = clkdef->enable_shift;
	sc->halt_reg = clkdef->halt_reg;
	sc->hwcg_reg = clkdef->hwcg_reg;
	sc->hwcg_bit = clkdef->hwcg_bit;
	sc->halt_check_type = clkdef->halt_check_type;
	sc->halt_check_voted = clkdef->halt_check_voted;
	sc->flags = clkdef->flags;

	clknode_register(clkdom, clk);

	return (0);
}
