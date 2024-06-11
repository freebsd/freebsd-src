/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Jari Sihvola <jsihv@gmx.com>
 */

#ifndef _JH7110_CLK_H_
#define	_JH7110_CLK_H_

#include <dev/clk/clk.h>

#define JH7110_CLK_HAS_GATE	0x01
#define JH7110_CLK_HAS_MUX	0x02
#define JH7110_CLK_HAS_DIV	0x04
#define JH7110_CLK_HAS_INV	0x08

#define AONCRG_RESET_SELECTOR	0x38
#define AONCRG_RESET_STATUS	0x3c
#define STGCRG_RESET_SELECTOR	0x74
#define STGCRG_RESET_STATUS	0x78
#define SYSCRG_RESET_SELECTOR	0x2f8
#define SYSCRG_RESET_STATUS	0x308

struct jh7110_clkgen_softc {
	struct mtx		mtx;
	struct clkdom		*clkdom;
	struct resource		*mem_res;
	uint32_t		reset_status_offset;
	uint32_t		reset_selector_offset;
};

struct jh7110_clk_def {
	struct clknode_init_def clkdef;
	uint32_t		offset;
	uint32_t		flags;
	uint64_t		d_max;
};

#define	JH7110_CLK(_idx, _name, _pn, _d_max, _flags)		\
{								\
	.clkdef.id = _idx,					\
	.clkdef.name =	_name,					\
	.clkdef.parent_names = _pn,				\
	.clkdef.parent_cnt = nitems(_pn),			\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
	.flags = _flags,					\
	.d_max = _d_max,					\
}

#define	JH7110_GATE(_idx, _name, _pn)					\
	JH7110_CLK(_idx, _name, _pn, 0, JH7110_CLK_HAS_GATE)
#define	JH7110_MUX(_idx, _name, _pn)					\
	JH7110_CLK(_idx, _name, _pn, 0, JH7110_CLK_HAS_MUX)
#define	JH7110_DIV(_idx, _name, _pn, _d_max)				\
	JH7110_CLK(_idx, _name, _pn, _d_max, JH7110_CLK_HAS_DIV)
#define	JH7110_GATEMUX(_idx, _name, _pn)				\
	JH7110_CLK(_idx, _name, _pn, 0, JH7110_CLK_HAS_GATE |		\
	JH7110_CLK_HAS_MUX)
#define	JH7110_GATEDIV(_idx, _name, _pn, _d_max)			\
	JH7110_CLK(_idx, _name, _pn, _d_max, JH7110_CLK_HAS_GATE |	\
	JH7110_CLK_HAS_DIV)
#define JH7110_INV(_idx, _name, _pn)					\
	JH7110_CLK(_idx, _name, _pn, 0, JH7110_CLK_HAS_INV)

int jh7110_clk_register(struct clkdom *clkdom,
    const struct jh7110_clk_def *clkdef);
int jh7110_ofw_map(struct clkdom *clkdom, uint32_t ncells, phandle_t *cells,
    struct clknode **clk);
int jh7110_reset_is_asserted(device_t dev, intptr_t id, bool *reset);
int jh7110_reset_assert(device_t dev, intptr_t id, bool assert);

#endif	/* _JH7110_CLK_H_ */
