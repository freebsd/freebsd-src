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

#include "qcom_clk_freqtbl.h"
#include "qcom_clk_rcg2.h"
#include "qcom_clk_rcg2_reg.h"

#include "clkdev_if.h"

#if 0
#define DPRINTF(dev, msg...) device_printf(dev, msg);
#else
#define DPRINTF(dev, msg...)
#endif

#define	QCOM_CLK_RCG2_CFG_OFFSET(sc)	\
	    ((sc)->cmd_rcgr + (sc)->cfg_offset + QCOM_CLK_RCG2_CFG_REG)
#define	QCOM_CLK_RCG2_CMD_REGISTER(sc)	\
	    ((sc)->cmd_rcgr + QCOM_CLK_RCG2_CMD_REG)
#define	QCOM_CLK_RCG2_M_OFFSET(sc)	\
	    ((sc)->cmd_rcgr + (sc)->cfg_offset + QCOM_CLK_RCG2_M_REG)
#define	QCOM_CLK_RCG2_N_OFFSET(sc)	\
	    ((sc)->cmd_rcgr + (sc)->cfg_offset + QCOM_CLK_RCG2_N_REG)
#define	QCOM_CLK_RCG2_D_OFFSET(sc)	\
	    ((sc)->cmd_rcgr + (sc)->cfg_offset + QCOM_CLK_RCG2_D_REG)

struct qcom_clk_rcg2_sc {
	struct clknode *clknode;
	uint32_t cmd_rcgr;
	uint32_t hid_width;
	uint32_t mnd_width;
	int32_t safe_src_idx;
	uint32_t cfg_offset;
	int safe_pre_parent_idx;
	uint32_t flags;
	const struct qcom_clk_freq_tbl *freq_tbl;
};


/*
 * Finish a clock update.
 *
 * This instructs the configuration to take effect.
 */
static bool
qcom_clk_rcg2_update_config_locked(struct qcom_clk_rcg2_sc *sc)
{
	uint32_t reg, count;

	/*
	 * Send "update" to the controller.
	 */
	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CMD_REGISTER(sc), &reg);
	reg |= QCOM_CLK_RCG2_CMD_UPDATE;
	CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CMD_REGISTER(sc), reg);
	wmb();

	/*
	 * Poll for completion of update.
	 */
	for (count = 0; count < 1000; count++) {
		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_CMD_REGISTER(sc), &reg);
		if ((reg & QCOM_CLK_RCG2_CMD_UPDATE) == 0) {
			return (true);
		}
		DELAY(10);
		rmb();
	}

	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CMD_REGISTER(sc), &reg);
	DPRINTF(clknode_get_device(sc->clknode), "%s: failed; reg=0x%08x\n",
	    __func__, reg);
	return (false);
}

/*
 * Calculate the output frequency given an input frequency and the m/n:d
 * configuration.
 */
static uint64_t
qcom_clk_rcg2_calc_rate(uint64_t rate, uint32_t mode, uint32_t m, uint32_t n,
    uint32_t hid_div)
{
	if (hid_div != 0) {
		rate = rate * 2;
		rate = rate / (hid_div + 1);
	}

	/* Note: assume n is not 0 here; bad things happen if it is */

	if (mode != 0) {
		rate = (rate * m) / n;
	}

	return (rate);
}

/*
 * The inverse of calc_rate() - calculate the required input frequency
 * given the desired output freqency and m/n:d configuration.
 */
static uint64_t
qcom_clk_rcg2_calc_input_freq(uint64_t freq, uint32_t m, uint32_t n,
    uint32_t hid_div)
{
	if (hid_div != 0) {
		freq = freq / 2;
		freq = freq * (hid_div + 1);
	}

	if (n != 0) {
		freq = (freq * n) / m;
	}

	return (freq);
}

static int
qcom_clk_rcg2_recalc(struct clknode *clk, uint64_t *freq)
{
	struct qcom_clk_rcg2_sc *sc;
	uint32_t cfg, m = 0, n = 0, hid_div = 0;
	uint32_t mode = 0, mask;

	sc = clknode_get_softc(clk);

	/* Read the MODE, CFG, M and N parameters */
	CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc),
	    &cfg);
	if (sc->mnd_width != 0) {
		mask = (1U << sc->mnd_width) - 1;
		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_M_OFFSET(sc), &m);
		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_N_OFFSET(sc), &n);
		m = m & mask;
		n = ~ n;
		n = n & mask;
		n = n + m;
		mode = (cfg & QCOM_CLK_RCG2_CFG_MODE_MASK)
		    >> QCOM_CLK_RCG2_CFG_MODE_SHIFT;
	}
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));

	/* Fetch the divisor */
	mask = (1U << sc->hid_width) - 1;
	hid_div = (cfg >> QCOM_CLK_RCG2_CFG_SRC_DIV_SHIFT) & mask;

	/* Calculate the rate based on the parent rate and config */
	*freq = qcom_clk_rcg2_calc_rate(*freq, mode, m, n, hid_div);

	return (0);
}

/*
 * configure the mn:d divisor, pre-divisor, and parent.
 */
static void
qcom_clk_rcg2_set_config_locked(struct qcom_clk_rcg2_sc *sc,
    const struct qcom_clk_freq_tbl *f, int parent_idx)
{
	uint32_t mask, reg;

	/* If we have MN:D, then update it */
	if (sc->mnd_width != 0 && f->n != 0) {
		mask = (1U << sc->mnd_width) - 1;

		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_M_OFFSET(sc), &reg);
		reg &= ~mask;
		reg |= (f->m & mask);
		CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_M_OFFSET(sc), reg);

		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_N_OFFSET(sc), &reg);
		reg &= ~mask;
		reg |= ((~(f->n - f->m)) & mask);
		CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_N_OFFSET(sc), reg);

		CLKDEV_READ_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_D_OFFSET(sc), &reg);
		reg &= ~mask;
		reg |= ((~f->n) & mask);
		CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
		    QCOM_CLK_RCG2_D_OFFSET(sc), reg);
	}

	mask = (1U << sc->hid_width) - 1;
	/*
	 * Mask out register fields we're going to modify along with
	 * the pre-divisor.
	 */
	mask |= QCOM_CLK_RCG2_CFG_SRC_SEL_MASK
	    | QCOM_CLK_RCG2_CFG_MODE_MASK
	    | QCOM_CLK_RCG2_CFG_HW_CLK_CTRL_MASK;

	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc), &reg);
	reg &= ~mask;

	/* Configure pre-divisor */
	reg = reg | ((f->pre_div) << QCOM_CLK_RCG2_CFG_SRC_DIV_SHIFT);

	/* Configure parent clock */
	reg = reg | (((parent_idx << QCOM_CLK_RCG2_CFG_SRC_SEL_SHIFT)
	    & QCOM_CLK_RCG2_CFG_SRC_SEL_MASK));

	/* Configure dual-edge if needed */
	if (sc->mnd_width != 0 && f->n != 0 && (f->m != f->n))
		reg |= QCOM_CLK_RCG2_CFG_MODE_DUAL_EDGE;

	CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc), reg);
}

static int
qcom_clk_rcg2_init(struct clknode *clk, device_t dev)
{
	struct qcom_clk_rcg2_sc *sc;
	uint32_t reg;
	uint32_t idx;
	bool enabled __unused;

	sc = clknode_get_softc(clk);

	/*
	 * Read the mux setting to set the right parent.
	 * Whilst here, read the config to get whether we're enabled
	 * or not.
	 */
	CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
	/* check if rcg2 root clock is enabled */
	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CMD_REGISTER(sc), &reg);
	if (reg & QCOM_CLK_RCG2_CMD_ROOT_OFF)
		enabled = false;
	else
		enabled = true;

	/* mux settings */
	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc), &reg);
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));

	idx = (reg & QCOM_CLK_RCG2_CFG_SRC_SEL_MASK)
	    >> QCOM_CLK_RCG2_CFG_SRC_SEL_SHIFT;
	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: mux index %u, enabled=%d\n",
	    __func__, idx, enabled);
	clknode_init_parent_idx(clk, idx);

	/*
	 * If we could be sure our parent clocks existed here in the tree,
	 * we could calculate our current frequency by fetching the parent
	 * frequency and then do our divider math.  Unfortunately that
	 * currently isn't the case.
	 */

	return(0);
}

static int
qcom_clk_rcg2_set_gate(struct clknode *clk, bool enable)
{

	/*
	 * For now this isn't supported; there's some support for
	 * "shared" rcg2 nodes in the Qualcomm/upstream Linux trees but
	 * it's not currently needed for the supported platforms.
	 */
	return (0);
}

/*
 * Program the parent index.
 *
 * This doesn't do the update.  It also must be called with the device
 * lock held.
 */
static void
qcom_clk_rcg2_set_parent_index_locked(struct qcom_clk_rcg2_sc *sc,
    uint32_t index)
{
	uint32_t reg;

	CLKDEV_READ_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc), &reg);
	reg = reg & ~QCOM_CLK_RCG2_CFG_SRC_SEL_MASK;
	reg = reg | (((index << QCOM_CLK_RCG2_CFG_SRC_SEL_SHIFT)
	    & QCOM_CLK_RCG2_CFG_SRC_SEL_MASK));
	CLKDEV_WRITE_4(clknode_get_device(sc->clknode),
	    QCOM_CLK_RCG2_CFG_OFFSET(sc),
	    reg);
}

/*
 * Set frequency
 *
 * fin - the parent frequency, if exists
 * fout - starts as the requested frequency, ends with the configured
 *        or dry-run frequency
 * Flags - CLK_SET_DRYRUN, CLK_SET_ROUND_UP, CLK_SET_ROUND_DOWN
 * retval - 0, ERANGE
 */
static int
qcom_clk_rcg2_set_freq(struct clknode *clk, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	struct qcom_clk_rcg2_sc *sc;
	const struct qcom_clk_freq_tbl *f;
	const char **parent_names;
	uint64_t p_freq, p_clk_freq;
	int parent_cnt;
	struct clknode *p_clk;
	int i;

	sc = clknode_get_softc(clk);

	/*
	 * Find a suitable frequency in the frequency table.
	 *
	 * TODO: should pay attention to ROUND_UP / ROUND_DOWN and add
	 * a freqtbl method to handle both accordingly.
	 */
	f = qcom_clk_freq_tbl_lookup(sc->freq_tbl, *fout);
	if (f == NULL) {
		device_printf(clknode_get_device(sc->clknode),
		    "%s: no suitable freqtbl entry found for freq %llu\n",
		    __func__,
		    *fout);
		return (ERANGE);
	}

	/*
	 * Find the parent index for the given parent clock.
	 * Abort if we can't actually find it.
	 *
	 * XXX TODO: this should be a clk API call!
	 */
	parent_cnt = clknode_get_parents_num(clk);
	parent_names = clknode_get_parent_names(clk);
	for (i = 0; i < parent_cnt; i++) {
		if (parent_names[i] == NULL)
			continue;
		if (strcmp(parent_names[i], f->parent) == 0)
			break;
	}
	if (i >= parent_cnt) {
		device_printf(clknode_get_device(sc->clknode),
		    "%s: couldn't find suitable parent?\n",
		    __func__);
		return (ENXIO);
	}

	/*
	 * If we aren't setting the parent clock, then we need
	 * to just program the new parent clock in and update.
	 * (or for DRYRUN just skip that and return the new
	 * frequency.)
	 */
	if ((sc->flags & QCOM_CLK_RCG2_FLAGS_SET_RATE_PARENT) == 0) {
		if (flags & CLK_SET_DRYRUN) {
			*fout = f->freq;
			return (0);
		}

		if (sc->safe_pre_parent_idx > -1) {
			DPRINTF(clknode_get_device(sc->clknode),
			    "%s: setting to safe parent idx %d\n",
			    __func__,
			    sc->safe_pre_parent_idx);
			CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
			qcom_clk_rcg2_set_parent_index_locked(sc,
			    sc->safe_pre_parent_idx);
			DPRINTF(clknode_get_device(sc->clknode),
			    "%s: safe parent: updating config\n", __func__);
			if (! qcom_clk_rcg2_update_config_locked(sc)) {
				CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
				DPRINTF(clknode_get_device(sc->clknode),
				    "%s: error updating config\n",
				    __func__);
				return (ENXIO);
			}
			CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
			DPRINTF(clknode_get_device(sc->clknode),
			    "%s: safe parent: done\n", __func__);
			clknode_set_parent_by_idx(sc->clknode,
			    sc->safe_pre_parent_idx);
		}
		/* Program parent index, then schedule update */
		CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
		qcom_clk_rcg2_set_parent_index_locked(sc, i);
		if (! qcom_clk_rcg2_update_config_locked(sc)) {
			CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
			device_printf(clknode_get_device(sc->clknode),
			    "%s: couldn't program in parent idx %u!\n",
			    __func__, i);
			return (ENXIO);
		}
		CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
		clknode_set_parent_by_idx(sc->clknode, i);
		*fout = f->freq;
		return (0);
	}

	/*
	 * If we /are/ setting the parent clock, then we need
	 * to determine what frequency we need the parent to
	 * be, and then reconfigure the parent to the new
	 * frequency, and then change our parent.
	 *
	 * (Again, if we're doing DRYRUN, just skip that
	 * and return the new frequency.)
	 */
	p_clk = clknode_find_by_name(f->parent);
	if (p_clk == NULL) {
		device_printf(clknode_get_device(sc->clknode),
		    "%s: couldn't find parent clk (%s)\n",
		    __func__, f->parent);
		return (ENXIO);
	}

	/*
	 * Calculate required frequency from said parent clock to
	 * meet the needs of our target clock.
	 */
	p_freq = qcom_clk_rcg2_calc_input_freq(f->freq, f->m, f->n,
	    f->pre_div);
	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: request %llu, parent %s freq %llu, parent freq %llu\n",
	    __func__,
	    *fout,
	    f->parent,
	    f->freq,
	    p_freq);

	/*
	 * To ensure glitch-free operation on some clocks, set it to
	 * a safe parent before programming our divisor and the parent
	 * clock configuration.  Then once it's done, flip the parent
	 * to the new parent.
	 *
	 * If we're doing a dry-run then we don't need to re-parent the
	 * clock just yet!
	 */
	if (((flags & CLK_SET_DRYRUN) == 0) &&
	    (sc->safe_pre_parent_idx > -1)) {
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: setting to safe parent idx %d\n",
		    __func__,
		    sc->safe_pre_parent_idx);
		CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
		qcom_clk_rcg2_set_parent_index_locked(sc,
		    sc->safe_pre_parent_idx);
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: safe parent: updating config\n", __func__);
		if (! qcom_clk_rcg2_update_config_locked(sc)) {
			CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
			DPRINTF(clknode_get_device(sc->clknode),
			    "%s: error updating config\n",
			    __func__);
			return (ENXIO);
		}
		CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
		DPRINTF(clknode_get_device(sc->clknode),
		    "%s: safe parent: done\n", __func__);
		clknode_set_parent_by_idx(sc->clknode,
		    sc->safe_pre_parent_idx);
	}

	/*
	 * Set the parent frequency before we change our mux and divisor
	 * configuration.
	 */
	if (clknode_get_freq(p_clk, &p_clk_freq) != 0) {
		device_printf(clknode_get_device(sc->clknode),
		    "%s: couldn't get freq for parent clock %s\n",
		    __func__,
		    f->parent);
		return (ENXIO);
	}
	if (p_clk_freq != p_freq) {
		uint64_t n_freq;
		int rv;

		/*
		 * If we're doing a dryrun then call test_freq() not set_freq().
		 * That way we get the frequency back that we would be set to.
		 *
		 * If we're not doing a dry run then set the frequency, then
		 * call get_freq to get what it was set to.
		 */
		if (flags & CLK_SET_DRYRUN) {
			n_freq = p_freq;
			rv = clknode_test_freq(p_clk, n_freq, flags, 0,
			    &p_freq);
		} else {
			rv = clknode_set_freq(p_clk, p_freq, flags, 0);
		}

		if (rv != 0) {
			device_printf(clknode_get_device(sc->clknode),
			    "%s: couldn't set parent clock %s frequency to "
			    "%llu\n",
			    __func__,
			    f->parent,
			    p_freq);
			return (ENXIO);
		}

		/* Frequency was set, fetch what it was set to */
		if ((flags & CLK_SET_DRYRUN) == 0) {
			rv = clknode_get_freq(p_clk, &p_freq);
			if (rv != 0) {
				device_printf(clknode_get_device(sc->clknode),
				    "%s: couldn't get parent frequency",
				    __func__);
				return (ENXIO);
			}
		}
	}

	DPRINTF(clknode_get_device(sc->clknode),
	    "%s: requested freq=%llu, target freq=%llu,"
	    " parent choice=%s, parent_freq=%llu\n",
	    __func__,
	    *fout,
	    f->freq,
	    f->parent,
	    p_freq);

	/*
	 * Set the parent node, the parent programming and the divisor
	 * config.  Because they're done together, we don't go via
	 * a mux method on this node.
	 */

	/*
	 * Program the divisor and parent.
	 */
	if ((flags & CLK_SET_DRYRUN) == 0) {
		CLKDEV_DEVICE_LOCK(clknode_get_device(sc->clknode));
		qcom_clk_rcg2_set_config_locked(sc, f, i);
		if (! qcom_clk_rcg2_update_config_locked(sc)) {
			CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
			device_printf(clknode_get_device(sc->clknode),
			    "%s: couldn't program in divisor, help!\n",
			    __func__);
			return (ENXIO);
		}
		CLKDEV_DEVICE_UNLOCK(clknode_get_device(sc->clknode));
		clknode_set_parent_by_idx(sc->clknode, i);
	}

	/*
	 * p_freq is now the frequency that the parent /is/ set to.
	 * (Or would be set to for a dry run.)
	 *
	 * Calculate what the eventual frequency would be, we'll want
	 * this to return when we're done - and again, if it's a dryrun,
	 * don't set anything up.  This doesn't rely on the register
	 * contents.
	 */
	*fout = qcom_clk_rcg2_calc_rate(p_freq, (f->n == 0 ? 0 : 1),
	    f->m, f->n, f->pre_div);

	return (0);
}

static clknode_method_t qcom_clk_rcg2_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		qcom_clk_rcg2_init),
	CLKNODEMETHOD(clknode_recalc_freq,	qcom_clk_rcg2_recalc),
	CLKNODEMETHOD(clknode_set_gate,		qcom_clk_rcg2_set_gate),
	CLKNODEMETHOD(clknode_set_freq,		qcom_clk_rcg2_set_freq),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(qcom_clk_fepll, qcom_clk_rcg2_class, qcom_clk_rcg2_methods,
   sizeof(struct qcom_clk_rcg2_sc), clknode_class);

int
qcom_clk_rcg2_register(struct clkdom *clkdom,
    struct qcom_clk_rcg2_def *clkdef)
{
	struct clknode *clk;
	struct qcom_clk_rcg2_sc *sc;

	/*
	 * Right now the rcg2 code isn't supporting turning off the clock
	 * or limiting it to the lowest parent clock.  But, do set the
	 * flags appropriately.
	 */
	if (clkdef->flags & QCOM_CLK_RCG2_FLAGS_CRITICAL)
		clkdef->clkdef.flags |= CLK_NODE_CANNOT_STOP;

	clk = clknode_create(clkdom, &qcom_clk_rcg2_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->clknode = clk;

	sc->cmd_rcgr = clkdef->cmd_rcgr;
	sc->hid_width = clkdef->hid_width;
	sc->mnd_width = clkdef->mnd_width;
	sc->safe_src_idx = clkdef->safe_src_idx;
	sc->safe_pre_parent_idx = clkdef->safe_pre_parent_idx;
	sc->cfg_offset = clkdef->cfg_offset;
	sc->flags = clkdef->flags;
	sc->freq_tbl = clkdef->freq_tbl;

	clknode_register(clkdom, clk);

	return (0);
}
