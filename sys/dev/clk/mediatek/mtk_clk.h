/*
 * Copyright (c) 2026 Hayzam Sherif <hayzam@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _DEV_CLK_MEDIATEK_MTK_CLK_H_
#define	_DEV_CLK_MEDIATEK_MTK_CLK_H_

#include <dev/clk/clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_link.h>

/* Clock id for nodes that are not exported through the DT binding. */
#define	MTK_CLK_NOID		(-1)

/* PLL, see mtk_clk_pll.c for the register layout. */
struct mtk_clk_pll_def {
	struct clknode_init_def	clkdef;
	uint32_t		con0;		/* control register */
	uint32_t		rst_bar;	/* CON0 reset bar mask, or 0 */
};

/*
 * Mux with an optional gate bit (set means gated) and an optional update
 * register latching the new selection.  The field is changed by
 * read-modify-write of reg, or, with MTK_MUX_SETCLR for a read-only reg,
 * through write-1-to-set/clear registers.
 */
#define	MTK_MUX_SETCLR		0x0001

struct mtk_clk_mux_def {
	struct clknode_init_def	clkdef;
	uint32_t		reg;		/* selection */
	uint32_t		set_reg;
	uint32_t		clr_reg;
	uint32_t		shift;
	uint32_t		width;
	int			gate_bit;	/* -1 if none */
	uint32_t		upd_reg;
	int			upd_bit;	/* -1 if none */
	uint32_t		flags;
};

/*
 * Gate bit, changed by read-modify-write of reg, or, with MTK_GATE_SETCLR
 * for a read-only reg, through write-1-to-set/clear registers.  Normally a
 * set bit means the clock is gated; MTK_GATE_INVERTED reverses this.
 */
#define	MTK_GATE_SETCLR		0x0001
#define	MTK_GATE_INVERTED	0x0002

struct mtk_clk_gate_def {
	struct clknode_init_def	clkdef;
	uint32_t		reg;		/* gate bit */
	uint32_t		set_reg;
	uint32_t		clr_reg;
	uint32_t		bit;
	uint32_t		flags;
};

enum mtk_clk_type {
	MTK_CLK_LINK = 1,
	MTK_CLK_FIXED,
	MTK_CLK_PLL,
	MTK_CLK_MUX,
	MTK_CLK_GATE,
};

struct mtk_clk_def {
	enum mtk_clk_type		type;
	union {
		struct clk_link_def	link;
		struct clk_fixed_def	fixed;
		struct mtk_clk_pll_def	pll;
		struct mtk_clk_mux_def	mux;
		struct mtk_clk_gate_def	gate;
	};
};

/*
 * Software resets, in banks of 32.  Each bank has a write-1-to-set
 * (assert) register, a write-1-to-clear (deassert) register at +4 and a
 * status register at +8.  Reset id n is bit n % 32 of bank n / 32.
 * Older SoCs with a single read-write register per bank would need a
 * read-modify-write variant, as for gates and muxes.  It is left for the
 * first SoC that needs it: the MT7986 clock controllers need none.
 */
struct mtk_clk_reset_desc {
	const uint32_t		*banks;		/* set register of each bank */
	size_t			nbanks;
};

struct mtk_clk_desc {
	struct mtk_clk_def		*clks;
	size_t				nclks;
	const struct mtk_clk_reset_desc	*resets;	/* or NULL */
};

struct mtk_clk_softc {
	struct resource			*res;
	struct mtx			mtx;
	struct clkdom			*clkdom;
	const struct mtk_clk_reset_desc	*resets;
};

/* Helpers for clock tables. */
#define	MTK_LINK(_name)							\
	{ .type = MTK_CLK_LINK, .link = {				\
	    .clkdef = { .name = (_name), .id = MTK_CLK_NOID } } }

#define	MTK_FIXED(_id, _name, _freq)					\
	{ .type = MTK_CLK_FIXED, .fixed = {				\
	    .clkdef = { .name = (_name), .id = (_id) },			\
	    .freq = (_freq) } }

#define	MTK_FACTOR(_id, _name, _parent, _mult, _div)			\
	{ .type = MTK_CLK_FIXED, .fixed = {				\
	    .clkdef = { .name = (_name), .id = (_id),			\
		.parent_names = (const char *[]){ _parent },		\
		.parent_cnt = 1 },					\
	    .mult = (_mult), .div = (_div) } }

int	mtk_clk_attach(device_t dev, const struct mtk_clk_desc *desc);

int	mtk_clk_pll_register(struct clkdom *clkdom,
	    const struct mtk_clk_pll_def *def);
int	mtk_clk_mux_register(struct clkdom *clkdom,
	    const struct mtk_clk_mux_def *def);
int	mtk_clk_gate_register(struct clkdom *clkdom,
	    const struct mtk_clk_gate_def *def);

DECLARE_CLASS(mtk_clk_driver);

#endif /* _DEV_CLK_MEDIATEK_MTK_CLK_H_ */
