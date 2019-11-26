/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#ifndef __RK_CRU_H__
#define __RK_CRU_H__

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_link.h>

#include <arm64/rockchip/clk/rk_clk_armclk.h>
#include <arm64/rockchip/clk/rk_clk_composite.h>
#include <arm64/rockchip/clk/rk_clk_fract.h>
#include <arm64/rockchip/clk/rk_clk_gate.h>
#include <arm64/rockchip/clk/rk_clk_mux.h>
#include <arm64/rockchip/clk/rk_clk_pll.h>

/* Macro for defining various types of clocks. */
/* Pure gate */
#define	GATE(_idx, _clkname, _pname, _o, _s)				\
{									\
	.id = _idx,							\
	.name = _clkname,						\
	.parent_name = _pname,						\
	.offset = CRU_CLKGATE_CON(_o),					\
	.shift = _s,							\
}

/* Fixed rate clock. */
#define	FRATE(_id, _name, _freq)					\
{									\
	.type = RK_CLK_FIXED,						\
	.clk.fixed = &(struct clk_fixed_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = NULL,				\
		.clkdef.parent_cnt = 0,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.freq = _freq,						\
	},								\
}

/* Fixed factor multipier/divider. */
#define	FFACT(_id, _name, _pname, _mult, _div)				\
{									\
	.type = RK_CLK_FIXED,						\
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

/* Linked clock. */
#define	LINK(_name)							\
{									\
	.type = RK_CLK_LINK,						\
	.clk.link = &(struct clk_link_def) {				\
		.clkdef.id = 0,						\
		.clkdef.name = _name,					\
		.clkdef.parent_names = NULL,				\
		.clkdef.parent_cnt = 0,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
	},								\
}

/* Complex clock fo ARM cores. */
#define ARMDIV(_id, _name, _pn, _r, _o, _ds, _dw, _ms, _mw, _mp, _ap)	\
{									\
	.type = RK_CLK_ARMCLK,						\
	.clk.armclk = &(struct rk_clk_armclk_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = CRU_CLKSEL_CON(_o),			\
		.mux_shift = _ms,					\
		.mux_width = _mw,					\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.main_parent = _mp,					\
		.alt_parent = _ap,					\
		.rates = _r,						\
		.nrates = nitems(_r),					\
	},								\
}

/* Fractional rate multipier/divider. */
#define	FRACT(_id, _name, _pname, _f, _o)				\
{									\
	.type = RK_CLK_FRACT,						\
	.clk.fract = &(struct rk_clk_fract_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = CRU_CLKSEL_CON(_o),				\
		.flags = _f,						\
	},								\
}

/* Full composite clock. */
#define COMP(_id, _name, _pnames, _f,  _o,  _ds, _dw,  _ms, _mw)	\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pnames,				\
		.clkdef.parent_cnt = nitems(_pnames),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = CRU_CLKSEL_CON(_o),			\
		.mux_shift = _ms,					\
		.mux_width = _mw,					\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.flags = RK_CLK_COMPOSITE_HAVE_MUX | _f, 		\
	},								\
}

/* Composite clock without mux (divider only). */
#define CDIV(_id, _name, _pname, _f, _o, _ds, _dw)			\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = CRU_CLKSEL_CON(_o),			\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.flags =  _f,						\
	},								\
}

/* Complex clock without divider (multiplexer only). */
#define MUX(_id, _name, _pn, _f,  _mo, _ms, _mw)			\
{									\
	.type = RK_CLK_MUX,						\
	.clk.mux = &(struct rk_clk_mux_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = CRU_CLKSEL_CON(_mo),				\
		.shift = _ms,						\
		.width = _mw,						\
		.mux_flags = _f, 			\
	},								\
}

struct rk_cru_gate {
	const char	*name;
	const char	*parent_name;
	uint32_t	id;
	uint32_t	offset;
	uint32_t	shift;
};

#define	CRU_GATE(idx, clkname, pname, o, s)	\
	{				\
		.id = idx,			\
		.name = clkname,		\
		.parent_name = pname,		\
		.offset = o,			\
		.shift = s,			\
	},

enum rk_clk_type {
	RK_CLK_UNDEFINED = 0,
	RK3328_CLK_PLL,
	RK3399_CLK_PLL,
	RK_CLK_COMPOSITE,
	RK_CLK_FIXED,
	RK_CLK_FRACT,
	RK_CLK_MUX,
	RK_CLK_ARMCLK,
	RK_CLK_LINK,
};

struct rk_clk {
	enum rk_clk_type	type;
	union {
		struct rk_clk_pll_def		*pll;
		struct rk_clk_composite_def	*composite;
		struct rk_clk_mux_def		*mux;
		struct rk_clk_armclk_def	*armclk;
		struct clk_fixed_def		*fixed;
		struct rk_clk_fract_def		*fract;
		struct clk_link_def		*link;
	} clk;
};

struct rk_cru_softc {
	device_t		dev;
	struct resource		*res;
	struct clkdom		*clkdom;
	struct mtx		mtx;
	int			type;
	uint32_t		reset_offset;
	uint32_t		reset_num;
	struct rk_cru_gate	*gates;
	int			ngates;
	struct rk_clk		*clks;
	int			nclks;
	struct rk_clk_armclk_def	*armclk;
	struct rk_clk_armclk_rates	*armclk_rates;
	int			narmclk_rates;
};

DECLARE_CLASS(rk_cru_driver);

int	rk_cru_attach(device_t dev);

#endif /* __RK_CRU_H__ */
