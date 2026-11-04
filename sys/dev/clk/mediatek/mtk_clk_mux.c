/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * MediaTek clock mux.  The selection is read from a register and changed
 * by read-modify-write of it, or, where that register is read-only,
 * through separate write-1-to-clear and write-1-to-set registers.
 * Optionally the mux has a gate bit in the same field group (set means
 * gated) and an update register whose bit latches a new selection.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <dev/clk/clk.h>
#include <dev/clk/mediatek/mtk_clk.h>

#include "clkdev_if.h"

struct mtk_clk_mux_sc {
	uint32_t	reg;
	uint32_t	set_reg;
	uint32_t	clr_reg;
	uint32_t	shift;
	uint32_t	mask;
	int		gate_bit;
	uint32_t	upd_reg;
	int		upd_bit;
	bool		setclr;
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

/* Change the bits in mask to val.  Called with the device lock held. */
static void
mtk_clk_mux_write(struct clknode *clk, struct mtk_clk_mux_sc *sc,
    uint32_t mask, uint32_t val)
{
	if (!sc->setclr) {
		MD4(clk, sc->reg, mask, val);
		return;
	}
	if ((mask & ~val) != 0)
		WR4(clk, sc->clr_reg, mask & ~val);
	if (val != 0)
		WR4(clk, sc->set_reg, val);
}

static void
mtk_clk_mux_update(struct clknode *clk, struct mtk_clk_mux_sc *sc)
{
	if (sc->upd_bit >= 0)
		WR4(clk, sc->upd_reg, 1u << sc->upd_bit);
}

static int
mtk_clk_mux_init(struct clknode *clk, device_t dev)
{
	struct mtk_clk_mux_sc *sc = clknode_get_softc(clk);
	uint32_t val;

	DEVICE_LOCK(clk);
	RD4(clk, sc->reg, &val);
	DEVICE_UNLOCK(clk);
	clknode_init_parent_idx(clk, (val >> sc->shift) & sc->mask);
	return (0);
}

static int
mtk_clk_mux_set_mux(struct clknode *clk, int idx)
{
	struct mtk_clk_mux_sc *sc = clknode_get_softc(clk);

	DEVICE_LOCK(clk);
	mtk_clk_mux_write(clk, sc, sc->mask << sc->shift,
	    (uint32_t)idx << sc->shift);
	mtk_clk_mux_update(clk, sc);
	DEVICE_UNLOCK(clk);
	return (0);
}

static int
mtk_clk_mux_set_gate(struct clknode *clk, bool enable)
{
	struct mtk_clk_mux_sc *sc = clknode_get_softc(clk);

	/* Without a gate bit the mux always runs. */
	if (sc->gate_bit < 0)
		return (enable ? 0 : ENXIO);
	DEVICE_LOCK(clk);
	mtk_clk_mux_write(clk, sc, 1u << sc->gate_bit,
	    enable ? 0 : 1u << sc->gate_bit);
	if (enable)
		mtk_clk_mux_update(clk, sc);
	DEVICE_UNLOCK(clk);
	return (0);
}

static int
mtk_clk_mux_get_gate(struct clknode *clk, bool *enabled)
{
	struct mtk_clk_mux_sc *sc = clknode_get_softc(clk);
	uint32_t val;

	if (sc->gate_bit < 0) {
		*enabled = true;
		return (0);
	}
	DEVICE_LOCK(clk);
	RD4(clk, sc->reg, &val);
	DEVICE_UNLOCK(clk);
	*enabled = (val & (1u << sc->gate_bit)) == 0;
	return (0);
}

static clknode_method_t mtk_clk_mux_methods[] = {
	CLKNODEMETHOD(clknode_init,		mtk_clk_mux_init),
	CLKNODEMETHOD(clknode_set_mux,		mtk_clk_mux_set_mux),
	CLKNODEMETHOD(clknode_set_gate,		mtk_clk_mux_set_gate),
	CLKNODEMETHOD(clknode_get_gate,		mtk_clk_mux_get_gate),
	CLKNODEMETHOD_END
};

DEFINE_CLASS_1(mtk_clk_mux_clknode, mtk_clk_mux_clknode_class,
    mtk_clk_mux_methods, sizeof(struct mtk_clk_mux_sc), clknode_class);

int
mtk_clk_mux_register(struct clkdom *clkdom, const struct mtk_clk_mux_def *def)
{
	struct clknode *clk;
	struct mtk_clk_mux_sc *sc;

	clk = clknode_create(clkdom, &mtk_clk_mux_clknode_class, &def->clkdef);
	if (clk == NULL)
		return (ENXIO);

	sc = clknode_get_softc(clk);
	sc->reg = def->reg;
	sc->set_reg = def->set_reg;
	sc->clr_reg = def->clr_reg;
	sc->shift = def->shift;
	sc->mask = (1u << def->width) - 1;
	sc->gate_bit = def->gate_bit;
	sc->upd_reg = def->upd_reg;
	sc->upd_bit = def->upd_bit;
	sc->setclr = (def->flags & MTK_MUX_SETCLR) != 0;

	if (clknode_register(clkdom, clk) == NULL)
		return (ENXIO);
	return (0);
}
