/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek clock gate.  The gate bit is read from a register and changed
 * by read-modify-write of it, or, where that register is read-only,
 * through separate write-1-to-set and write-1-to-clear registers.  A set
 * bit normally means "gated".
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>
#include <dev/clk/mediatek/mtk_clk.h>

#include "clkdev_if.h"

struct mtk_clk_gate_sc {
	uint32_t	reg;
	uint32_t	set_reg;
	uint32_t	clr_reg;
	uint32_t	mask;
	bool		setclr;
	bool		inverted;
};

#define	RD4(_clk, _off, _val)						\
	CLKDEV_READ_4(clknode_get_device(_clk), _off, _val)
#define	WR4(_clk, _off, _val)						\
	CLKDEV_WRITE_4(clknode_get_device(_clk), _off, _val)
#define	MD4(_clk, _off, _clr, _set)					\
	CLKDEV_MODIFY_4(clknode_get_device(_clk), _off, _clr, _set)
#define	DEVICE_LOCK(_clk)						\
	CLKDEV_DEVICE_LOCK(clknode_get_device(_clk))
#define	DEVICE_UNLOCK(_clk)						\
	CLKDEV_DEVICE_UNLOCK(clknode_get_device(_clk))

static int
mtk_clk_gate_init(struct clknode *clk, device_t dev)
{
	clknode_init_parent_idx(clk, 0);
	return (0);
}

static int
mtk_clk_gate_set_gate(struct clknode *clk, bool enable)
{
	struct mtk_clk_gate_sc *sc = clknode_get_softc(clk);
	bool set;

	set = enable == sc->inverted;
	DEVICE_LOCK(clk);
	if (!sc->setclr)
		MD4(clk, sc->reg, sc->mask, set ? sc->mask : 0);
	else
		WR4(clk, set ? sc->set_reg : sc->clr_reg, sc->mask);
	DEVICE_UNLOCK(clk);
	return (0);
}

static int
mtk_clk_gate_get_gate(struct clknode *clk, bool *enabled)
{
	struct mtk_clk_gate_sc *sc = clknode_get_softc(clk);
	uint32_t val;

	DEVICE_LOCK(clk);
	RD4(clk, sc->reg, &val);
	DEVICE_UNLOCK(clk);
	*enabled = ((val & sc->mask) == 0) != sc->inverted;
	return (0);
}

static clknode_method_t mtk_clk_gate_methods[] = {
	CLKNODEMETHOD(clknode_init,		mtk_clk_gate_init),
	CLKNODEMETHOD(clknode_set_gate,		mtk_clk_gate_set_gate),
	CLKNODEMETHOD(clknode_get_gate,		mtk_clk_gate_get_gate),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(mtk_clk_gate_clknode, mtk_clk_gate_clknode_class,
    mtk_clk_gate_methods, sizeof(struct mtk_clk_gate_sc), clknode_class);

int
mtk_clk_gate_register(struct clkdom *clkdom,
    const struct mtk_clk_gate_def *def)
{
	struct clknode *clk;
	struct mtk_clk_gate_sc *sc;

	clk = clknode_create(clkdom, &mtk_clk_gate_clknode_class,
	    &def->clkdef);
	if (clk == NULL)
		return (ENXIO);

	sc = clknode_get_softc(clk);
	sc->reg = def->reg;
	sc->set_reg = def->set_reg;
	sc->clr_reg = def->clr_reg;
	sc->mask = 1u << def->bit;
	sc->setclr = (def->flags & MTK_GATE_SETCLR) != 0;
	sc->inverted = (def->flags & MTK_GATE_INVERTED) != 0;

	if (clknode_register(clkdom, clk) == NULL)
		return (ENXIO);
	return (0);
}
