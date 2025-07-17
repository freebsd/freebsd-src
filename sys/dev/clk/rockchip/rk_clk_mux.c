/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/systm.h>

#include <machine/bus.h>

#include <dev/clk/clk.h>
#include <dev/syscon/syscon.h>

#include <dev/clk/rockchip/rk_cru.h>
#include <dev/clk/rockchip/rk_clk_mux.h>

#include "clkdev_if.h"
#include "syscon_if.h"

#define	WR4(_clk, off, val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), off, val)
#define	RD4(_clk, off, val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), off, val)
#define	MD4(_clk, off, clr, set )					\
	CLKDEV_MODIFY_4(clknode_get_device(_clk), off, clr, set)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

#if 0
#define	dprintf(format, arg...)						\
	printf("%s:(%s)" format, __func__, clknode_get_name(clk), arg)
#else
#define	dprintf(format, arg...)
#endif

static int rk_clk_mux_init(struct clknode *clk, device_t dev);
static int rk_clk_mux_set_mux(struct clknode *clk, int idx);
static int rk_clk_mux_set_freq(struct clknode *clk, uint64_t fparent,
    uint64_t *fout, int flags, int *stop);

struct rk_clk_mux_sc {
	uint32_t	offset;
	uint32_t	shift;
	uint32_t	mask;
	int		mux_flags;
	struct syscon	*grf;
};

static clknode_method_t rk_clk_mux_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init, 	rk_clk_mux_init),
	CLKNODEMETHOD(clknode_set_mux, 	rk_clk_mux_set_mux),
	CLKNODEMETHOD(clknode_set_freq,	rk_clk_mux_set_freq),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(rk_clk_mux, rk_clk_mux_class, rk_clk_mux_methods,
   sizeof(struct rk_clk_mux_sc), clknode_class);

static struct syscon *
rk_clk_mux_get_grf(struct clknode *clk)
{
	device_t dev;
	phandle_t node;
	struct syscon *grf;

	grf = NULL;
	dev = clknode_get_device(clk);
	node = ofw_bus_get_node(dev);
	if (OF_hasprop(node, "rockchip,grf") &&
	    syscon_get_by_ofw_property(dev, node,
	    "rockchip,grf", &grf) != 0) {
		return (NULL);
	}

	return (grf);
}

static int
rk_clk_mux_init(struct clknode *clk, device_t dev)
{
	uint32_t reg;
	struct rk_clk_mux_sc *sc;
	int rv;

	sc = clknode_get_softc(clk);

	if ((sc->mux_flags & RK_CLK_MUX_GRF) != 0) {
		sc->grf = rk_clk_mux_get_grf(clk);
		if (sc->grf == NULL)
			panic("clock %s has GRF flag set but no syscon is available",
			    clknode_get_name(clk));
	}

	DEVICE_LOCK(clk);
	if (sc->grf) {
		reg = SYSCON_READ_4(sc->grf, sc->offset);
		rv = 0;
	} else
		rv = RD4(clk, sc->offset, &reg);
	DEVICE_UNLOCK(clk);
	if (rv != 0) {
		return (rv);
	}
	reg = (reg >> sc->shift) & sc->mask;
	clknode_init_parent_idx(clk, reg);
	return(0);
}

static int
rk_clk_mux_set_mux(struct clknode *clk, int idx)
{
	uint32_t reg;
	struct rk_clk_mux_sc *sc;
	int rv;

	sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	if (sc->grf)
		rv = SYSCON_MODIFY_4(sc->grf, sc->offset, sc->mask << sc->shift,
		  ((idx & sc->mask) << sc->shift) | RK_CLK_MUX_MASK);
	else
		rv = MD4(clk, sc->offset, sc->mask << sc->shift,
		  ((idx & sc->mask) << sc->shift) | RK_CLK_MUX_MASK);
	if (rv != 0) {
		DEVICE_UNLOCK(clk);
		return (rv);
	}
	if (sc->grf == NULL)
		RD4(clk, sc->offset, &reg);
	DEVICE_UNLOCK(clk);

	return(0);
}

static int
rk_clk_mux_set_freq(struct clknode *clk, uint64_t fparent, uint64_t *fout,
    int flags, int *stop)
{
	struct rk_clk_mux_sc *sc;
	struct clknode *p_clk, *p_best_clk;
	const char **p_names;
	int p_idx, best_parent;
	int rv;

	sc = clknode_get_softc(clk);

	if ((sc->mux_flags & RK_CLK_MUX_GRF) != 0) {
		*stop = 1;
		return (ENOTSUP);
	}
	if ((sc->mux_flags & RK_CLK_MUX_REPARENT) == 0) {
		*stop = 0;
		return (0);
	}

	dprintf("Finding best parent for target freq of %ju\n", *fout);
	p_names = clknode_get_parent_names(clk);
	for (p_idx = 0; p_idx != clknode_get_parents_num(clk); p_idx++) {
		p_clk = clknode_find_by_name(p_names[p_idx]);
		dprintf("Testing with parent %s (%d)\n",
		    clknode_get_name(p_clk), p_idx);

		rv = clknode_set_freq(p_clk, *fout, flags | CLK_SET_DRYRUN, 0);
		dprintf("Testing with parent %s (%d) rv=%d\n",
		    clknode_get_name(p_clk), p_idx, rv);
		if (rv == 0) {
			best_parent = p_idx;
			p_best_clk = p_clk;
			*stop = 1;
		}
	}

	if (!*stop)
		return (0);

	if ((flags & CLK_SET_DRYRUN) != 0)
		return (0);

	p_idx = clknode_get_parent_idx(clk);
	if (p_idx != best_parent) {
		dprintf("Switching parent index from %d to %d\n", p_idx,
		    best_parent);
		clknode_set_parent_by_idx(clk, best_parent);
	}

	clknode_set_freq(p_best_clk, *fout, flags, 0);
	clknode_get_freq(p_best_clk, fout);

	return (0);
}

int
rk_clk_mux_register(struct clkdom *clkdom, struct rk_clk_mux_def *clkdef)
{
	struct clknode *clk;
	struct rk_clk_mux_sc *sc;

	clk = clknode_create(clkdom, &rk_clk_mux_class, &clkdef->clkdef);
	if (clk == NULL)
		return (1);

	sc = clknode_get_softc(clk);
	sc->offset = clkdef->offset;
	sc->shift = clkdef->shift;
	sc->mask =  (1 << clkdef->width) - 1;
	sc->mux_flags = clkdef->mux_flags;

	clknode_register(clkdom, clk);
	return (0);
}
