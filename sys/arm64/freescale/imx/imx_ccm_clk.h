/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2020 Oleksandr Tymoshenko <gonzo@FreeBSD.org>
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
 *
 * $FreeBSD$
 */

#ifndef	IMX6_CCM_CLK_H
#define	IMX6_CCM_CLK_H

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/clk/clk_link.h>

enum imx_clk_type {
	IMX_CLK_UNDEFINED = 0,
	IMX_CLK_FIXED,
	IMX_CLK_LINK,
	IMX_CLK_MUX,
	IMX_CLK_GATE,
	IMX_CLK_COMPOSITE,
	IMX_CLK_SSCG_PLL,
	IMX_CLK_FRAC_PLL,
	IMX_CLK_DIV,
};

struct imx_clk {
	enum imx_clk_type	type;
	union {
		struct clk_fixed_def		*fixed;
		struct clk_link_def		*link;
		struct imx_clk_mux_def		*mux;
		struct imx_clk_gate_def		*gate;
		struct imx_clk_composite_def	*composite;
		struct imx_clk_sscg_pll_def	*sscg_pll;
		struct imx_clk_frac_pll_def	*frac_pll;
		struct clk_div_def		*div;
	} clk;
};

/* Linked clock. */
#define	LINK(_id, _name)						\
{									\
	.type = IMX_CLK_LINK,						\
	.clk.link = &(struct clk_link_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = NULL,				\
		.clkdef.parent_cnt = 0,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
	},								\
}

/* Complex clock without divider (multiplexer only). */
#define MUX(_id, _name, _pn, _f,  _mo, _ms, _mw)			\
{									\
	.type = IMX_CLK_MUX,						\
	.clk.mux = &(struct imx_clk_mux_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _mo,						\
		.shift = _ms,						\
		.width = _mw,						\
		.mux_flags = _f, 					\
	},								\
}

/* Fixed frequency clock */
#define	FIXED(_id, _name, _freq)					\
{									\
	.type = IMX_CLK_FIXED,						\
	.clk.fixed = &(struct clk_fixed_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.freq = _freq,						\
	},								\
}

/* Fixed factor multipier/divider. */
#define	FFACT(_id, _name, _pname, _mult, _div)				\
{									\
	.type = IMX_CLK_FIXED,						\
	.clk.fixed = &(struct clk_fixed_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.mult = _mult,						\
		.div = _div,						\
	},								\
}

/* Clock gate */
#define	GATE(_id, _name, _pname, _o, _shift)				\
{									\
	.type = IMX_CLK_GATE,						\
	.clk.gate = &(struct imx_clk_gate_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _o,						\
		.shift = _shift,					\
		.mask = 1,						\
	},								\
}

/* Root clock gate */
#define	ROOT_GATE(_id, _name, _pname, _reg)				\
{									\
	.type = IMX_CLK_GATE,						\
	.clk.gate = &(struct imx_clk_gate_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _reg,						\
		.shift = 0,						\
		.mask = 3,						\
	},								\
}

/* Composite clock with GATE, MUX, PRE_DIV, and POST_DIV */
#define COMPOSITE(_id, _name, _pn, _o, _flags)				\
{									\
	.type = IMX_CLK_COMPOSITE,					\
	.clk.composite = &(struct imx_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _o,						\
		.flags = _flags,					\
	},								\
}

/* SSCG PLL */
#define SSCG_PLL(_id, _name, _pn, _o)					\
{									\
	.type = IMX_CLK_SSCG_PLL,					\
	.clk.composite = &(struct imx_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _o,						\
	},								\
}

/* Fractional PLL */
#define FRAC_PLL(_id, _name, _pname, _o)				\
{									\
	.type = IMX_CLK_FRAC_PLL,					\
	.clk.frac_pll = &(struct imx_clk_frac_pll_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _o,						\
	},								\
}

#define DIV(_id, _name, _pname, _o, _shift, _width)			\
{									\
	.type = IMX_CLK_DIV,						\
	.clk.div = &(struct clk_div_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = _o,						\
		.i_shift = _shift,					\
		.i_width = _width,					\
	},								\
}

#endif
