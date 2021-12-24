/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright 2020 Michal Meloun <mmel@FreeBSD.org>
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

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_gate.h>
#include <dev/extres/clk/clk_mux.h>

#include <dt-bindings/clock/tegra210-car.h>
#include "tegra210_car.h"

#if 0
#define dprintf(...) printf(__VA_ARGS__)
#else
#define dprintf(...)
#endif

/* All PLLs. */
enum pll_type {
	PLL_M,
	PLL_MB,
	PLL_X,
	PLL_C,
	PLL_C2,
	PLL_C3,
	PLL_C4,
	PLL_P,
	PLL_A,
	PLL_A1,
	PLL_U,
	PLL_D,
	PLL_D2,
	PLL_DP,
	PLL_E,
	PLL_REFE};
/* Flags for PLLs */

#define	PLL_FLAG_PDIV_POWER2	0x01		/* P Divider is 2^n */
#define	PLL_FLAG_VCO_OUT	0x02		/* Output VCO directly */
#define	PLL_FLAG_HAVE_SDM	0x04		/* Have SDM implemented */
#define	PLL_FLAG_HAVE_SDA	0x04		/* Have SDA implemented */

/* Common base register bits. */
#define	PLL_BASE_BYPASS		(1U << 31)
#define	PLL_BASE_ENABLE		(1  << 30)
#define	PLL_BASE_REFDISABLE	(1  << 29)
#define	PLL_BASE_LOCK		(1  << 27)

#define	PLLREFE_MISC_LOCK	(1 << 27)

#define	PLL_MISC_LOCK_ENABLE	(1 << 18)
#define	PLLM_LOCK_ENABLE	(1 << 4)
#define PLLMB_LOCK_ENABLE 	(1 << 16)
#define	PLLC_LOCK_ENABLE	(1 << 24)
#define	PLLC4_LOCK_ENABLE	(1 << 30)
#define	PLLA_LOCK_ENABLE	(1 << 28)
#define	PLLD2_LOCK_ENABLE	(1 << 30)
#define	PLLU_LOCK_ENABLE	(1 << 29)
#define	PLLREFE_LOCK_ENABLE	(1 << 30)
#define	PLLPD_LOCK_ENABLE	(1 << 30)
#define	PLLE_LOCK_ENABLE	(1 << 9)

#define	PLLM_IDDQ_BIT		5
#define	PLLMB_IDDQ_BIT		17
#define	PLLC_IDDQ_BIT		27
#define	PLLC4_IDDQ_BIT		18
#define	PLLP_IDDQ_BIT		3
#define	PLLA_IDDQ_BIT		25
#define	PLLA1_IDDQ_BIT		27
#define	PLLU_IDDQ_BIT		31
#define	PLLD_IDDQ_BIT		20
#define	PLLD2_IDDQ_BIT		18
#define	PLLX_IDDQ_BIT		3
#define	PLLREFE_IDDQ_BIT	24
#define	PLLDP_IDDQ_BIT		18


#define	PLL_LOCK_TIMEOUT	5000

/* Post divider <-> register value mapping. */
struct pdiv_table {
	uint32_t divider;	/* real divider */
	uint32_t value;		/* register value */
};

/* Bits definition of M, N and P fields. */
struct mnp_bits {
	uint32_t	m_width;
	uint32_t	n_width;
	uint32_t	p_width;
	uint32_t	m_shift;
	uint32_t	n_shift;
	uint32_t	p_shift;
};

struct clk_pll_def {
	struct clknode_init_def	clkdef;
	enum pll_type		type;
	uint32_t		base_reg;
	uint32_t		misc_reg;
	uint32_t		lock_enable;
	uint32_t		iddq_reg;
	uint32_t		iddq_mask;
	uint32_t		flags;
	struct pdiv_table 	*pdiv_table;
	struct mnp_bits		mnp_bits;
};

#define	PLIST(x) static const char *x[]

#define	PLL(_id, cname, pname)						\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){pname},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS

/* multiplexer for pll sources. */
#define	MUX(_id, cname, plists, o, s, w)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = plists,					\
	.clkdef.parent_cnt = nitems(plists),				\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift  = s,							\
	.width = w,							\
}

/* Fractional divider (7.1) for PLL branch. */
#define	DIV7_1(_id, cname, plist, o, s)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags =  CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.i_shift = (s) + 1,						\
	.i_width = 7,							\
	.f_shift = s,							\
	.f_width = 1,							\
}

/* P divider (2^n). for PLL branch. */
#define	DIV5_E(_id, cname, plist, o, s)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags =  CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.i_shift = s,							\
	.i_width = 5,							\
}

/* P divider (2^n). for PLL branch. */
#define	DIV_TB(_id, cname, plist, o, s, n, table)			\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags =  CLK_NODE_STATIC_STRINGS,			\
	.div_flags = CLK_DIV_WITH_TABLE | CLK_DIV_ZERO_BASED,		\
	.offset = o,							\
	.i_shift = s,							\
	.i_width = n,							\
	.div_table = table,						\
}

/* Standard gate. */
#define	GATE(_id, cname, plist, o, s)					\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift = s,							\
	.mask = 1,							\
	.on_value = 1,							\
	.off_value = 0,							\
}
/* Gate for PLL branch. */
#define	GATE_PLL(_id, cname, plist, o, s)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){plist},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.offset = o,							\
	.shift = s,							\
	.mask = 3,							\
	.on_value = 3,							\
	.off_value = 0,							\
}

/* Fixed rate multipier/divider. */
#define	FACT(_id, cname, pname, _mult, _div)				\
{									\
	.clkdef.id = _id,						\
	.clkdef.name = cname,						\
	.clkdef.parent_names = (const char *[]){pname},			\
	.clkdef.parent_cnt = 1,						\
	.clkdef.flags = CLK_NODE_STATIC_STRINGS,			\
	.mult = _mult,							\
	.div = _div,							\
}

static struct pdiv_table qlin_map[] = {
	{ 1,  0},
	{ 2,  1},
	{ 3,  2},
	{ 4,  3},
	{ 5,  4},
	{ 6,  5},
	{ 8,  6},
	{ 9,  7},
	{10,  8},
	{12,  9},
	{15, 10},
	{16, 11},
	{18, 12},
	{20, 13},
	{24, 14},
	{30, 15},
	{32, 16},
	{ 0,  0},
};

static struct clk_pll_def pll_clks[] = {
/* PLLM: 880 MHz Clock source for EMC 2x clock */
	{
		PLL(TEGRA210_CLK_PLL_M, "pllM_out0", "osc"),
		.type = PLL_M,
		.base_reg = PLLM_BASE,
		.misc_reg = PLLM_MISC2,
		.lock_enable = PLLM_LOCK_ENABLE,
		.iddq_reg = PLLM_MISC2,
		.iddq_mask = 1 << PLLM_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 20},
	},
/* PLLMB: 880 MHz Clock source for EMC 2x clock */
	{
		PLL(TEGRA210_CLK_PLL_M, "pllMB_out0", "osc"),
		.type = PLL_MB,
		.base_reg = PLLMB_BASE,
		.misc_reg = PLLMB_MISC1,
		.lock_enable = PLLMB_LOCK_ENABLE,
		.iddq_reg = PLLMB_MISC1,
		.iddq_mask = 1 << PLLMB_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 20},
	},
/* PLLX: 1GHz Clock source for the fast CPU cluster and the shadow CPU */
	{
		PLL(TEGRA210_CLK_PLL_X, "pllX_out0", "osc_div_clk"),
		.type = PLL_X,
		.base_reg = PLLX_BASE,
		.misc_reg = PLLX_MISC,
		.lock_enable = PLL_MISC_LOCK_ENABLE,
		.iddq_reg = PLLX_MISC_3,
		.iddq_mask = 1 << PLLX_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 20},
	},
/* PLLC: 510 MHz Clock source for camera use */
	{
		PLL(TEGRA210_CLK_PLL_C, "pllC_out0", "osc_div_clk"),
		.type = PLL_C,
		.base_reg = PLLC_BASE,
		.misc_reg = PLLC_MISC_0,
		.iddq_reg = PLLC_MISC_1,
		.iddq_mask = 1 << PLLC_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 10, 20},
	},
/* PLLC2: 510 MHz Clock source for SE, VIC, TSECB, NVJPG scaling */
	{
		PLL(TEGRA210_CLK_PLL_C2, "pllC2_out0", "osc_div_clk"),
		.type = PLL_C2,
		.base_reg = PLLC2_BASE,
		.misc_reg = PLLC2_MISC_0,
		.iddq_reg = PLLC2_MISC_1,
		.iddq_mask = 1 << PLLC_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 10, 20},
	},
/* PLLC3: 510 MHz Clock source for NVENC, NVDEC scaling */
	{
		PLL(TEGRA210_CLK_PLL_C3, "pllC3_out0", "osc_div_clk"),
		.type = PLL_C3,
		.base_reg = PLLC3_BASE,
		.misc_reg = PLLC3_MISC_0,
		.lock_enable = PLL_MISC_LOCK_ENABLE,
		.iddq_reg = PLLC3_MISC_1,
		.iddq_mask = 1 << PLLC_IDDQ_BIT,
		.mnp_bits = {8, 8, 5, 0, 10, 20},
	},
/* PLLC4: 600 MHz Clock source for SD/eMMC ans system busses */
	{
		PLL(TEGRA210_CLK_PLL_C4, "pllC4", "pllC4_src"),
		.type = PLL_C4,
		.flags = PLL_FLAG_VCO_OUT,
		.base_reg = PLLC4_BASE,
		.misc_reg = PLLC4_MISC,
		.lock_enable = PLLC4_LOCK_ENABLE,
		.iddq_reg = PLLC4_BASE,
		.iddq_mask = 1 << PLLC4_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 19},
	},
/* PLLP: 408 MHz Clock source for most peripherals */
	{
		/*
		 * VCO is directly exposed as pllP_out0, P div is used for
		 * pllP_out2
		 */
		PLL(TEGRA210_CLK_PLL_P, "pllP_out0", "osc_div_clk"),
		.type = PLL_P,
		.flags = PLL_FLAG_VCO_OUT,
		.base_reg = PLLP_BASE,
		.misc_reg = PLLP_MISC,
		.lock_enable = PLL_MISC_LOCK_ENABLE,
		.iddq_reg = PLLP_MISC,
		.iddq_mask = 1 << PLLA_IDDQ_BIT,
		.mnp_bits = {8, 8, 5,  0, 10, 20},
	},
/* PLLA: Audio clock for precise codec sampling */
	{
		PLL(TEGRA210_CLK_PLL_A, "pllA", "osc_div_clk"),
		.type = PLL_A,
		.base_reg = PLLA_BASE,
		.misc_reg = PLLA_MISC,
		.lock_enable = PLLA_LOCK_ENABLE,
		.iddq_reg = PLLA_BASE,
		.iddq_mask = 1 << PLLA_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 20},
	},
/* PLLA1: Audio clock for ADSP */
	{
		PLL(TEGRA210_CLK_PLL_A1, "pllA1_out0", "osc_div_clk"),
		.type = PLL_A1,
		.base_reg = PLLA1_BASE,
		.misc_reg = PLLA1_MISC_1,
		.iddq_reg = PLLA1_MISC_1,
		.iddq_mask = 1 << PLLA_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 20},
	},
/* PLLU: 480 MHz Clock source for USB PHY, provides 12/60/480 MHz */
	{
		PLL(TEGRA210_CLK_PLL_U, "pllU", "osc_div_clk"),
		.type = PLL_U,
		.flags = PLL_FLAG_VCO_OUT | PLL_FLAG_HAVE_SDA,
		.base_reg = PLLU_BASE,
		.misc_reg = PLLU_MISC,
		.lock_enable = PLLU_LOCK_ENABLE,
		.iddq_reg = PLLU_MISC,
		.iddq_mask = 1 << PLLU_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 16},
	},
/* PLLD: 594 MHz Clock sources for the DSI and display subsystem */
	{
		PLL(TEGRA210_CLK_PLL_D, "pllD_out", "osc_div_clk"),
		.type = PLL_D,
		.flags = PLL_FLAG_PDIV_POWER2,
		.base_reg = PLLD_BASE,
		.misc_reg = PLLD_MISC,
		.lock_enable = PLL_MISC_LOCK_ENABLE,
		.iddq_reg = PLLA1_MISC_1,
		.iddq_mask = 1 << PLLA_IDDQ_BIT,
		.mnp_bits = {8, 8, 3, 0, 11, 20},
	},
/* PLLD2: 594 MHz Clock sources for the DSI and display subsystem */
	{
		PLL(TEGRA210_CLK_PLL_D2, "pllD2_out", "pllD2_src"),
		.type = PLL_D2,
		.flags = PLL_FLAG_HAVE_SDM,
		.base_reg = PLLD2_BASE,
		.misc_reg = PLLD2_MISC,
		.lock_enable = PLLD2_LOCK_ENABLE,
		.iddq_reg = PLLD2_BASE,
		.iddq_mask =  1 << PLLD_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 19},
	},
/* PLLREFE: 624 Mhz*/
	{
		PLL(0, "pllREFE", "osc_div_clk"),
		.type = PLL_REFE,
		.flags = PLL_FLAG_VCO_OUT,
		.base_reg = PLLREFE_BASE,
		.misc_reg = PLLREFE_MISC,
		.lock_enable = PLLREFE_LOCK_ENABLE,
		.iddq_reg = PLLREFE_MISC,
		.iddq_mask = 1 << PLLREFE_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 16},
	},
/* PLLE: 100 MHz reference clock for PCIe/SATA/USB 3.0 (spread spectrum) */
	{
		PLL(TEGRA210_CLK_PLL_E, "pllE_out0", "pllE_src"),
		.type = PLL_E,
		.base_reg = PLLE_BASE,
		.misc_reg = PLLE_MISC,
		.lock_enable = PLLE_LOCK_ENABLE,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 24},
	},
/* PLLDP: 270 MHz Clock source fordisplay SOR (spread spectrum) */
	{
		PLL(0, "pllDP_out0", "pllDP_src"),
		.type = PLL_DP,
		.flags = PLL_FLAG_HAVE_SDM,
		.base_reg = PLLDP_BASE,
		.misc_reg = PLLDP_MISC,
		.lock_enable = PLLPD_LOCK_ENABLE,
		.iddq_reg = PLLDP_BASE,
		.iddq_mask =  1 << PLLDP_IDDQ_BIT,
		.pdiv_table = qlin_map,
		.mnp_bits = {8, 8, 5, 0, 8, 19},
	},
};

/* Fixed rate dividers. */
static struct clk_fixed_def tegra210_pll_fdivs[] = {
	FACT(0, "pllP_UD", "pllP_out0", 1, 1),
	FACT(0, "pllC_UD", "pllC_out0", 1, 1),
	FACT(0, "pllD_UD", "pllD_out0", 1, 1),
	FACT(0, "pllM_UD", "pllM_out0", 1, 1),
	FACT(0, "pllMB_UD", "pllMB_out0", 1, 1),
	FACT(TEGRA210_CLK_PLL_D_OUT0, "pllD_out0", "pllD_out", 1, 2),

	FACT(0, "pllC4_out1", "pllC4", 1, 3),
	FACT(0, "pllC4_out2", "pllC4", 1, 5),
	FACT(0, "pllD2_out0", "pllD2_out", 1, 2),

	/* Aliases used in super mux. */
	FACT(0, "pllX_out0_alias", "pllX_out0", 1, 1),
	FACT(0, "dfllCPU_out_alias", "dfllCPU_out", 1, 1),
};

/* MUXes for PLL sources. */
PLIST(mux_pll_srcs) = {"osc_div_clk", NULL, "pllP_out0", NULL}; /* FIXME */
PLIST(mux_plle_src1) = {"osc_div_clk", "pllP_out0"};
PLIST(mux_plle_src) = {"pllE_src1", "pllREFE_out0"};
static struct clk_mux_def tegra210_pll_sources[] = {
	/* Core clocks. */
	MUX(0, "pllD2_src", mux_pll_srcs, PLLD2_BASE, 25, 2),
	MUX(0, "pllDP_src", mux_pll_srcs, PLLDP_BASE, 25, 2),
	MUX(0, "pllC4_src", mux_pll_srcs, PLLC4_BASE, 25, 2),
	MUX(0, "pllE_src1", mux_plle_src1, PLLE_AUX, 2, 1),
	MUX(0, "pllE_src",  mux_plle_src, PLLE_AUX, 28, 1),
};

/* Gates for PLL branches. */
static struct clk_gate_def tegra210_pll_gates[] = {
	/* Core clocks. */
	GATE_PLL(0, "pllC_out1", "pllC_out1_div", PLLC_OUT, 0),

	GATE_PLL(0, "pllP_out1", "pllP_out1_div", PLLP_OUTA, 0),
	GATE_PLL(0, "pllP_out3", "pllP_out3_div", PLLP_OUTB, 0),
	GATE_PLL(TEGRA210_CLK_PLL_P_OUT4, "pllP_out4", "pllP_out4_div", PLLP_OUTB, 16),
	GATE_PLL(0, "pllP_out5", "pllP_out5_div", PLLP_OUTC, 16),

	GATE_PLL(0, "pllU_out1", "pllU_out1_div", PLLU_OUTA, 0),
	GATE_PLL(0, "pllU_out2", "pllU_out2_div", PLLU_OUTA, 16),
	GATE(0, "pllU_480", "pllU", PLLU_BASE, 22),
	GATE(0, "pllU_60", "pllU_out2", PLLU_BASE, 23),
	GATE(0, "pllU_48", "pllU_out1", PLLU_BASE, 25),

	GATE_PLL(0, "pllREFE_out1", "pllREFE_out1_div", PLLREFE_OUT, 0),
	GATE_PLL(0, "pllC4_out3", "pllC4_out3_div", PLLC4_OUT, 0),

	GATE_PLL(0, "pllA_out0", "pllA_out0_div", PLLA_OUT, 0),
};

struct clk_div_table tegra210_pll_pdiv_tbl[] = {
	/* value , divider */
	{ 0,  1 },
	{ 1,  2 },
	{ 2,  3 },
	{ 3,  4 },
	{ 4,  5 },
	{ 5,  6 },
	{ 6,  8 },
	{ 7, 10 },
	{ 8, 12 },
	{ 9, 16 },
	{10, 12 },
	{11, 16 },
	{12, 20 },
	{13, 24 },
	{14, 32 },
	{ 0,  0 },
};

/* Dividers for PLL branches. */
static struct clk_div_def tegra210_pll_divs[] = {
	/* Core clocks. */
	DIV7_1(0, "pllC_out1_div",    "pllC_out0",  PLLC_OUT, 8),

	DIV7_1(0, "pllP_out1_div",    "pllP_out0",  PLLP_OUTA, 8),
	DIV_TB(0, "pllP_out2",        "pllP_out0",  PLLP_BASE, 20, 5, tegra210_pll_pdiv_tbl),
	DIV7_1(0, "pllP_out3_div",    "pllP_out0",  PLLP_OUTB, 8),
	DIV7_1(0, "pllP_out4_div",    "pllP_out0",  PLLP_OUTB, 24),
	DIV7_1(0, "pllP_out5_div",    "pllP_out0",  PLLP_OUTC, 24),

	DIV_TB(0, "pllU_out0",        "pllU",       PLLU_BASE, 16, 5, tegra210_pll_pdiv_tbl),
	DIV7_1(0, "pllU_out1_div",    "pllU_out0",  PLLU_OUTA, 8),
	DIV7_1(0, "pllU_out2_div",    "pllU_out0",  PLLU_OUTA, 24),

	DIV_TB(0, "pllREFE_out0",     "pllREFE",    PLLREFE_BASE, 16, 5, tegra210_pll_pdiv_tbl),
	DIV7_1(0, "pllREFE_out1_div", "pllREFE",    PLLREFE_OUT, 8),

	DIV_TB(TEGRA210_CLK_PLL_C4_OUT0,
	          "pllC4_out0",       "pllC4",      PLLC4_BASE, 19, 5, tegra210_pll_pdiv_tbl),
	DIV7_1(0, "pllC4_out3_div",   "pllC4_out0", PLLC4_OUT, 8),

	DIV7_1(0, "pllA_out0_div",    "pllA",       PLLA_OUT, 8),

};

static int tegra210_pll_init(struct clknode *clk, device_t dev);
static int tegra210_pll_set_gate(struct clknode *clk, bool enable);
static int tegra210_pll_get_gate(struct clknode *clk, bool *enabled);
static int tegra210_pll_recalc(struct clknode *clk, uint64_t *freq);
static int tegra210_pll_set_freq(struct clknode *clknode, uint64_t fin,
    uint64_t *fout, int flags, int *stop);
struct pll_sc {
	device_t		clkdev;
	enum pll_type		type;
	uint32_t		base_reg;
	uint32_t		misc_reg;
	uint32_t		lock_enable;
	uint32_t		iddq_reg;
	uint32_t		iddq_mask;
	uint32_t		flags;
	struct pdiv_table 	*pdiv_table;
	struct mnp_bits		mnp_bits;
};

static clknode_method_t tegra210_pll_methods[] = {
	/* Device interface */
	CLKNODEMETHOD(clknode_init,		tegra210_pll_init),
	CLKNODEMETHOD(clknode_set_gate,		tegra210_pll_set_gate),
	CLKNODEMETHOD(clknode_get_gate,		tegra210_pll_get_gate),
	CLKNODEMETHOD(clknode_recalc_freq,	tegra210_pll_recalc),
	CLKNODEMETHOD(clknode_set_freq,		tegra210_pll_set_freq),
	CLKNODEMETHOD_END
};
DEFINE_CLASS_1(tegra210_pll, tegra210_pll_class, tegra210_pll_methods,
   sizeof(struct pll_sc), clknode_class);

static int
pll_enable(struct pll_sc *sc)
{
	uint32_t reg;


	RD4(sc, sc->base_reg, &reg);
	if (sc->type != PLL_E)
		reg &= ~PLL_BASE_BYPASS;
	reg |= PLL_BASE_ENABLE;
	WR4(sc, sc->base_reg, reg);
	return (0);
}

static int
pll_disable(struct pll_sc *sc)
{
	uint32_t reg;

	RD4(sc, sc->base_reg, &reg);
	if (sc->type != PLL_E)
		reg |= PLL_BASE_BYPASS;
	reg &= ~PLL_BASE_ENABLE;
	WR4(sc, sc->base_reg, reg);
	return (0);
}

static uint32_t
pdiv_to_reg(struct pll_sc *sc, uint32_t p_div)
{
	struct pdiv_table *tbl;

	tbl = sc->pdiv_table;
	if (tbl == NULL) {
		if (sc->flags & PLL_FLAG_PDIV_POWER2)
			return (ffs(p_div) - 1);
		else
			return (p_div);
	}

	while (tbl->divider != 0) {
		if (p_div <= tbl->divider)
			return (tbl->value);
		tbl++;
	}
	return (0xFFFFFFFF);
}

static uint32_t
reg_to_pdiv(struct pll_sc *sc, uint32_t reg)
{
	struct pdiv_table *tbl;

	tbl = sc->pdiv_table;
	if (tbl == NULL) {
		if (sc->flags & PLL_FLAG_PDIV_POWER2)
			return (1 << reg);
		else
			return (reg == 0 ? 1: reg);
	}
	while (tbl->divider) {
		if (reg == tbl->value)
			return (tbl->divider);
		tbl++;
	}
	return (0);
}

static uint32_t
get_masked(uint32_t val, uint32_t shift, uint32_t width)
{

	return ((val >> shift) & ((1 << width) - 1));
}

static uint32_t
set_masked(uint32_t val, uint32_t v, uint32_t shift, uint32_t width)
{

	val &= ~(((1 << width) - 1) << shift);
	val |= (v & ((1 << width) - 1)) << shift;
	return (val);
}

static void
get_divisors(struct pll_sc *sc, uint32_t *m, uint32_t *n, uint32_t *p)
{
	uint32_t val;
	struct mnp_bits *mnp_bits;

	mnp_bits = &sc->mnp_bits;
	RD4(sc, sc->base_reg, &val);
	*m = get_masked(val, mnp_bits->m_shift, mnp_bits->m_width);
	*n = get_masked(val, mnp_bits->n_shift, mnp_bits->n_width);
	*p = get_masked(val, mnp_bits->p_shift, mnp_bits->p_width);
}

static uint32_t
set_divisors(struct pll_sc *sc, uint32_t val, uint32_t m, uint32_t n,
    uint32_t p)
{
	struct mnp_bits *mnp_bits;

	mnp_bits = &sc->mnp_bits;
	val = set_masked(val, m, mnp_bits->m_shift, mnp_bits->m_width);
	val = set_masked(val, n, mnp_bits->n_shift, mnp_bits->n_width);
	val = set_masked(val, p, mnp_bits->p_shift, mnp_bits->p_width);
	return (val);
}

static bool
is_locked(struct pll_sc *sc)
{
	uint32_t reg;

	switch (sc->type) {
	case PLL_REFE:
		RD4(sc, sc->misc_reg, &reg);
		reg &=  PLLREFE_MISC_LOCK;
		break;

	case PLL_E:
		RD4(sc, sc->misc_reg, &reg);
		reg &= PLLE_MISC_LOCK;
		break;

	default:
		RD4(sc, sc->base_reg, &reg);
		reg &= PLL_BASE_LOCK;
		break;
	}
	return (reg != 0);
}

static int
wait_for_lock(struct pll_sc *sc)
{
	int i;

	for (i = PLL_LOCK_TIMEOUT / 10; i > 0; i--) {
		if (is_locked(sc))
			break;
		DELAY(10);
	}
	if (i <= 0) {
		printf("PLL lock timeout\n");
		return (ETIMEDOUT);
	}
	return (0);
}

static int
plle_enable(struct pll_sc *sc)
{
	uint32_t reg;
	int rv;
	struct mnp_bits *mnp_bits;
	uint32_t pll_m = 2;
	uint32_t pll_n = 125;
	uint32_t pll_cml = 14;

	mnp_bits = &sc->mnp_bits;

	/* Disable lock override. */
	RD4(sc, sc->base_reg, &reg);
	reg &= ~PLLE_BASE_LOCK_OVERRIDE;
	WR4(sc, sc->base_reg, reg);

	/* Enable SW control */
	RD4(sc, PLLE_AUX, &reg);
	reg |= PLLE_AUX_ENABLE_SWCTL;
	reg &= ~PLLE_AUX_SEQ_ENABLE;
	WR4(sc, PLLE_AUX, reg);
	DELAY(10);

	RD4(sc, sc->misc_reg, &reg);
	reg |= PLLE_MISC_LOCK_ENABLE;
	reg |= PLLE_MISC_IDDQ_SWCTL;
	reg &= ~PLLE_MISC_IDDQ_OVERRIDE_VALUE;
	reg |= PLLE_MISC_PTS;
	reg &= ~PLLE_MISC_VREG_BG_CTRL(~0);
	reg &= ~PLLE_MISC_VREG_CTRL(~0);
	WR4(sc, sc->misc_reg, reg);
	DELAY(10);

	RD4(sc, PLLE_SS_CNTL, &reg);
	reg |= PLLE_SS_CNTL_DISABLE;
	WR4(sc, PLLE_SS_CNTL, reg);

	RD4(sc, sc->base_reg, &reg);
	reg = set_divisors(sc, reg, pll_m, pll_n, pll_cml);
	WR4(sc, sc->base_reg, reg);
	DELAY(10);

	pll_enable(sc);
	rv = wait_for_lock(sc);
	if (rv != 0)
		return (rv);

	RD4(sc, PLLE_SS_CNTL, &reg);
	reg &= ~PLLE_SS_CNTL_SSCINCINTRV(~0);
	reg &= ~PLLE_SS_CNTL_SSCINC(~0);
	reg &= ~PLLE_SS_CNTL_SSCINVERT;
	reg &= ~PLLE_SS_CNTL_SSCCENTER;
	reg &= ~PLLE_SS_CNTL_SSCMAX(~0);
	reg |= PLLE_SS_CNTL_SSCINCINTRV(0x23);
	reg |= PLLE_SS_CNTL_SSCINC(0x1);
	reg |= PLLE_SS_CNTL_SSCMAX(0x21);
	WR4(sc, PLLE_SS_CNTL, reg);
	reg &= ~PLLE_SS_CNTL_SSCBYP;
	reg &= ~PLLE_SS_CNTL_BYPASS_SS;
	WR4(sc, PLLE_SS_CNTL, reg);
	DELAY(10);

	reg &= ~PLLE_SS_CNTL_INTERP_RESET;
	WR4(sc, PLLE_SS_CNTL, reg);
	DELAY(10);

	/* HW control of brick pll. */
	RD4(sc, sc->misc_reg, &reg);
	reg &= ~PLLE_MISC_IDDQ_SWCTL;
	WR4(sc, sc->misc_reg, reg);

	RD4(sc, PLLE_AUX, &reg);
	reg |= PLLE_AUX_USE_LOCKDET;
	reg |= PLLE_AUX_SS_SEQ_INCLUDE;
	reg &= ~PLLE_AUX_ENABLE_SWCTL;
	reg &= ~PLLE_AUX_SS_SWCTL;
	WR4(sc, PLLE_AUX, reg);
	reg |= PLLE_AUX_SEQ_START_STATE;
	DELAY(10);
	reg |= PLLE_AUX_SEQ_ENABLE;
	WR4(sc, PLLE_AUX, reg);

	/* Enable and start XUSBIO PLL HW control*/
	RD4(sc, XUSBIO_PLL_CFG0, &reg);
	reg &= ~XUSBIO_PLL_CFG0_CLK_ENABLE_SWCTL;
	reg &= ~XUSBIO_PLL_CFG0_PADPLL_RESET_SWCTL;
	reg |= XUSBIO_PLL_CFG0_PADPLL_USE_LOCKDET;
	reg |= XUSBIO_PLL_CFG0_PADPLL_SLEEP_IDDQ;
	reg &= ~XUSBIO_PLL_CFG0_SEQ_ENABLE;
	WR4(sc, XUSBIO_PLL_CFG0, reg);
	DELAY(10);

	reg |= XUSBIO_PLL_CFG0_SEQ_ENABLE;
	WR4(sc, XUSBIO_PLL_CFG0, reg);


	/* Enable and start SATA PLL HW control */
	RD4(sc, SATA_PLL_CFG0, &reg);
	reg &= ~SATA_PLL_CFG0_PADPLL_RESET_SWCTL;
	reg &= ~SATA_PLL_CFG0_PADPLL_RESET_OVERRIDE_VALUE;
	reg |=  SATA_PLL_CFG0_PADPLL_USE_LOCKDET;
	reg |=  SATA_PLL_CFG0_PADPLL_SLEEP_IDDQ;
	reg &= ~SATA_PLL_CFG0_SEQ_IN_SWCTL;
	reg &= ~SATA_PLL_CFG0_SEQ_RESET_INPUT_VALUE;
	reg &= ~SATA_PLL_CFG0_SEQ_LANE_PD_INPUT_VALUE;
	reg &= ~SATA_PLL_CFG0_SEQ_PADPLL_PD_INPUT_VALUE;
	reg &= ~SATA_PLL_CFG0_SEQ_ENABLE;
	WR4(sc, SATA_PLL_CFG0, reg);
	DELAY(10);
	reg |= SATA_PLL_CFG0_SEQ_ENABLE;
	WR4(sc, SATA_PLL_CFG0, reg);

	/* Enable HW control of PCIe PLL. */
	RD4(sc, PCIE_PLL_CFG, &reg);
	reg |= PCIE_PLL_CFG_SEQ_ENABLE;
	WR4(sc, PCIE_PLL_CFG, reg);

	return (0);
}

static int
tegra210_pll_set_gate(struct clknode *clknode, bool enable)
{
	int rv;
	struct pll_sc *sc;

	sc = clknode_get_softc(clknode);
	if (enable == 0) {
		rv = pll_disable(sc);
		return(rv);
	}

	if (sc->type == PLL_E)
		rv = plle_enable(sc);
	else
		rv = pll_enable(sc);
	return (rv);
}

static int
tegra210_pll_get_gate(struct clknode *clknode, bool *enabled)
{
	uint32_t reg;
	struct pll_sc *sc;

	sc = clknode_get_softc(clknode);
	RD4(sc, sc->base_reg, &reg);
	*enabled = reg & PLL_BASE_ENABLE ? true: false;
	WR4(sc, sc->base_reg, reg);
	return (0);
}

static int
pll_set_std(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags,
    uint32_t m, uint32_t n, uint32_t p)
{
	uint32_t reg;
	struct mnp_bits *mnp_bits;
	int rv;

	mnp_bits = &sc->mnp_bits;
	if (m >= (1 << mnp_bits->m_width))
		return (ERANGE);
	if (n >= (1 << mnp_bits->n_width))
		return (ERANGE);
	if (pdiv_to_reg(sc, p) >= (1 << mnp_bits->p_width))
		return (ERANGE);

	if (flags & CLK_SET_DRYRUN) {
		if (((flags & (CLK_SET_ROUND_UP | CLK_SET_ROUND_DOWN)) == 0) &&
		    (*fout != (((fin / m) * n) /p)))
			return (ERANGE);

		*fout = ((fin / m) * n) /p;

		return (0);
	}

	pll_disable(sc);

	/* take pll out of IDDQ */
	if (sc->iddq_reg != 0)
		MD4(sc, sc->iddq_reg, sc->iddq_mask, 0);

	RD4(sc, sc->base_reg, &reg);
	reg = set_masked(reg, m, mnp_bits->m_shift, mnp_bits->m_width);
	reg = set_masked(reg, n, mnp_bits->n_shift, mnp_bits->n_width);
	reg = set_masked(reg, pdiv_to_reg(sc, p), mnp_bits->p_shift,
	    mnp_bits->p_width);
	WR4(sc, sc->base_reg, reg);

	/* Enable PLL. */
	RD4(sc, sc->base_reg, &reg);
	reg |= PLL_BASE_ENABLE;
	WR4(sc, sc->base_reg, reg);

	/* Enable lock detection. */
	RD4(sc, sc->misc_reg, &reg);
	reg |= sc->lock_enable;
	WR4(sc, sc->misc_reg, reg);

	rv = wait_for_lock(sc);
	if (rv != 0) {
		/* Disable PLL */
		RD4(sc, sc->base_reg, &reg);
		reg &= ~PLL_BASE_ENABLE;
		WR4(sc, sc->base_reg, reg);
		return (rv);
	}
	RD4(sc, sc->misc_reg, &reg);

	pll_enable(sc);
	*fout = ((fin / m) * n) / p;
	return 0;
}

static int
plla_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;

	p = 1;
	m = 3;
	n = (*fout * p * m + fin / 2)/ fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);
	return (pll_set_std(sc,  fin, fout, flags, m, n, p));
}

static int
pllc_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;

	p = 2;
	m = 3;
	n = (*fout * p * m + fin / 2)/ fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);
	return (pll_set_std( sc, fin, fout, flags, m, n, p));
}

static int
pllc4_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;

	p = 1;
	m = 4;
	n = (*fout * p * m + fin / 2)/ fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);
	return (pll_set_std( sc, fin, fout, flags, m, n, p));
}

static int
plldp_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;

	p = 1;
	m = 4;
	n = (*fout * p * m + fin / 2)/ fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);
	return (pll_set_std( sc, fin, fout, flags, m, n, p));
}


/*
 * PLLD2 is used as source for pixel clock for HDMI.
 * We must be able to set it frequency very flexibly and
 * precisely (within 5% tolerance limit allowed by HDMI specs).
 *
 * For this reason, it is necessary to search the full state space.
 * Fortunately, thanks to early cycle terminations, performance
 * is within acceptable limits.
 */
#define	PLLD2_PFD_MIN		  12000000 	/* 12 MHz */
#define	PLLD2_PFD_MAX		  38400000	/* 38.4 MHz */
#define	PLLD2_VCO_MIN	  	 750000000	/* 750 MHz */
#define	PLLD2_VCO_MAX		1500000000	/* 1.5 GHz */

static int
plld2_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;
	uint32_t best_m, best_n, best_p;
	uint64_t vco, pfd;
	int64_t err, best_err;
	struct mnp_bits *mnp_bits;
	struct pdiv_table *tbl;
	int p_idx, rv;

	mnp_bits = &sc->mnp_bits;
	tbl = sc->pdiv_table;
	best_err = INT64_MAX;

	for (p_idx = 0; tbl[p_idx].divider != 0; p_idx++) {
		p = tbl[p_idx].divider;

		/* Check constraints */
		vco = *fout * p;
		if (vco < PLLD2_VCO_MIN)
			continue;
		if (vco > PLLD2_VCO_MAX)
			break;

		for (m = 1; m < (1 << mnp_bits->m_width); m++) {
			n = (*fout * p * m + fin / 2) / fin;

			/* Check constraints */
			if (n == 0)
				continue;
			if (n >= (1 << mnp_bits->n_width))
				break;
			vco = (fin * n) / m;
			if (vco > PLLD2_VCO_MAX || vco < PLLD2_VCO_MIN)
				continue;
			pfd = fin / m;
			if (pfd > PLLD2_PFD_MAX || vco < PLLD2_PFD_MIN)
				continue;

			/* Constraints passed, save best result */
			err = *fout - vco / p;
			if (err < 0)
				err = -err;
			if (err < best_err) {
				best_err = err;
				best_p = p;
				best_m = m;
				best_n = n;
			}
			if (err == 0)
				goto done;
		}
	}
done:
	/*
	 * HDMI specification allows 5% pixel clock tolerance,
	 * we will by a slightly stricter
	 */
	if (best_err > ((*fout * 100) / 4))
		return (ERANGE);

	if (flags & CLK_SET_DRYRUN)
		return (0);
	rv = pll_set_std(sc, fin, fout, flags, best_m, best_n, best_p);
	/* XXXX Panic for rv == ERANGE ? */
	return (rv);
}

static int
pllrefe_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	uint32_t m, n, p;

	m = 1;
	p = 1;
	n = *fout * p * m / fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);
	return (pll_set_std(sc, fin, fout, flags, m, n, p));
}

#define	PLLX_PFD_MIN   12000000LL	/* 12 MHz */
#define	PLLX_PFD_MAX   38400000LL	/* 38.4 MHz */
#define	PLLX_VCO_MIN  900000000LL	/* 0.9 GHz */
#define	PLLX_VCO_MAX 3000000000LL	/* 3 GHz */

static int
pllx_set_freq(struct pll_sc *sc, uint64_t fin, uint64_t *fout, int flags)
{
	struct mnp_bits *mnp_bits;
	uint32_t m, n, p;
	uint32_t old_m, old_n, old_p;
	uint32_t reg;
	int i, rv;

	mnp_bits = &sc->mnp_bits;

	get_divisors(sc, &old_m, &old_n, &old_p);
	old_p = reg_to_pdiv(sc, old_p);

	/* Pre-divider is fixed, Compute post-divider */
	m = old_m;
	p = 1;
	while ((*fout * p)  < PLLX_VCO_MIN)
		p++;
	if ((*fout * p) > PLLX_VCO_MAX)
		return (ERANGE);

	n = (*fout * p * m + fin / 2) / fin;
	dprintf("%s: m: %d, n: %d, p: %d\n", __func__, m, n, p);

	if (m >= (1 << mnp_bits->m_width))
		return (ERANGE);
	if (n >= (1 << mnp_bits->n_width))
		return (ERANGE);
	if (pdiv_to_reg(sc, p) >= (1 << mnp_bits->p_width))
		return (ERANGE);

	if (flags & CLK_SET_DRYRUN) {
		if (((flags & (CLK_SET_ROUND_UP | CLK_SET_ROUND_DOWN)) == 0) &&
		    (*fout != (((fin / m) * n) /p)))
			return (ERANGE);
		*fout = ((fin / m) * n) /p;
		return (0);
	}

	/* If new post-divider is bigger that original, set it now. */
	if (p < old_p) {
		RD4(sc, sc->base_reg, &reg);
		reg = set_masked(reg, pdiv_to_reg(sc, p), mnp_bits->p_shift,
		    mnp_bits->p_width);
		WR4(sc, sc->base_reg, reg);
	}
	DELAY(100);

	/* vvv Program dynamic VCO ramp. vvv */
	/* 1 - disable dynamic ramp mode. */
	RD4(sc, PLLX_MISC_2, &reg);
	reg &= ~PLLX_MISC_2_EN_DYNRAMP;
	WR4(sc, PLLX_MISC_2, reg);

	/* 2 - Setup new ndiv. */
	RD4(sc, PLLX_MISC_2, &reg);
	reg &= ~PLLX_MISC_2_NDIV_NEW(~0);
	reg |= PLLX_MISC_2_NDIV_NEW(n);
	WR4(sc, PLLX_MISC_2, reg);

	/* 3 - enable dynamic ramp. */
	RD4(sc, PLLX_MISC_2, &reg);
	reg |= PLLX_MISC_2_EN_DYNRAMP;
	WR4(sc, PLLX_MISC_2, reg);

	/* 4 - wait for done. */
	for (i = PLL_LOCK_TIMEOUT / 10; i > 0; i--) {
		RD4(sc, PLLX_MISC_2, &reg);
		if (reg & PLLX_MISC_2_DYNRAMP_DONE)
			break;
		DELAY(10);
	}
	if (i <= 0) {
		printf("PLL X dynamic ramp timedout\n");
		return (ETIMEDOUT);
	}

	/* 5 - copy new ndiv to base register. */
	RD4(sc, sc->base_reg, &reg);
	reg = set_masked(reg, n, mnp_bits->n_shift,
	    mnp_bits->n_width);
	WR4(sc, sc->base_reg, reg);

	/* 6 - disable dynamic ramp mode. */
	RD4(sc, PLLX_MISC_2, &reg);
	reg &= ~PLLX_MISC_2_EN_DYNRAMP;
	WR4(sc, PLLX_MISC_2, reg);

	rv = wait_for_lock(sc);
	if (rv != 0) {
		printf("PLL X is not locked !!\n");
	}
	/* ^^^ Dynamic ramp done. ^^^ */

	/* If new post-divider is smaller that original, set it. */
	if (p > old_p) {
		RD4(sc, sc->base_reg, &reg);
		reg = set_masked(reg, pdiv_to_reg(sc, p), mnp_bits->p_shift,
		    mnp_bits->p_width);
		WR4(sc, sc->base_reg, reg);
	}

	*fout = ((fin / m) * n) / p;
	return (0);
}

/* Simplified setup for 38.4 MHz clock. */
#define PLLX_STEP_A  0x04
#define PLLX_STEP_B  0x05
static int
pllx_init(struct pll_sc *sc)
{
	uint32_t reg;

	RD4(sc, PLLX_MISC, &reg);
	reg = PLLX_MISC_LOCK_ENABLE;
	WR4(sc, PLLX_MISC, reg);

	/* Setup dynamic ramp. */
	reg = 0;
	reg |= PLLX_MISC_2_DYNRAMP_STEPA(PLLX_STEP_A);
	reg |= PLLX_MISC_2_DYNRAMP_STEPB(PLLX_STEP_B);
	WR4(sc, PLLX_MISC_2, reg);

	/* Disable SDM. */
	reg = 0;
	WR4(sc, PLLX_MISC_4, reg);
	WR4(sc, PLLX_MISC_5, reg);

	return (0);
}

static int
tegra210_pll_set_freq(struct clknode *clknode, uint64_t fin, uint64_t *fout,
    int flags, int *stop)
{
	*stop = 1;
	int rv;
	struct pll_sc *sc;

	sc = clknode_get_softc(clknode);
	dprintf("%s: %s requested freq: %lu, input freq: %lu\n", __func__,
	   clknode_get_name(clknode), *fout, fin);
	switch (sc->type) {
	case PLL_A:
		rv = plla_set_freq(sc, fin, fout, flags);
		break;

	case PLL_C:
	case PLL_C2:
	case PLL_C3:
		rv = pllc_set_freq(sc, fin, fout, flags);
		break;

	case PLL_C4:
		rv = pllc4_set_freq(sc, fin, fout, flags);
		break;

	case PLL_D2:
		rv = plld2_set_freq(sc, fin, fout, flags);
		break;

	case PLL_DP:
		rv = plldp_set_freq(sc, fin, fout, flags);
		break;

	case PLL_REFE:
		rv = pllrefe_set_freq(sc, fin, fout, flags);
		break;

	case PLL_X:
		rv = pllx_set_freq(sc, fin, fout, flags);
		break;

	case PLL_U:
		if (*fout == 480000000)  /* PLLU is fixed to 480 MHz */
			rv = 0;
		else
			rv = ERANGE;
		break;
	default:
		rv = ENXIO;
		break;
	}

	return (rv);
}


static int
tegra210_pll_init(struct clknode *clk, device_t dev)
{
	struct pll_sc *sc;
	uint32_t reg, rv;

	sc = clknode_get_softc(clk);

	if (sc->type == PLL_X) {
		rv = pllx_init(sc);
		if (rv != 0)
			return (rv);
	}

	/* If PLL is enabled, enable lock detect too. */
	RD4(sc, sc->base_reg, &reg);
	if (reg & PLL_BASE_ENABLE) {
		RD4(sc, sc->misc_reg, &reg);
		reg |= sc->lock_enable;
		WR4(sc, sc->misc_reg, reg);
	}
	if (sc->type == PLL_REFE) {
		RD4(sc, sc->misc_reg, &reg);
		reg &= ~(1 << 29);	/* Disable lock override */
		WR4(sc, sc->misc_reg, reg);
	}
	clknode_init_parent_idx(clk, 0);
	return(0);
}

static int
tegra210_pll_recalc(struct clknode *clk, uint64_t *freq)
{
	struct pll_sc *sc;
	uint32_t m, n, p, pr;
	uint32_t reg, misc_reg;
	int locked;

	sc = clknode_get_softc(clk);

	RD4(sc, sc->base_reg, &reg);
	RD4(sc, sc->misc_reg, &misc_reg);

	get_divisors(sc, &m, &n, &pr);

	/* If VCO is directlu exposed, P divider is handled by external node */
	if (sc->flags & PLL_FLAG_VCO_OUT)
		p = 1;
	else
		p = reg_to_pdiv(sc, pr);

	locked = is_locked(sc);

	dprintf("%s: %s (0x%08x, 0x%08x) - m: %d, n: %d, p: %d (%d): "
	    "e: %d, r: %d, o: %d - %s\n", __func__,
	    clknode_get_name(clk), reg, misc_reg, m, n, p, pr,
	    (reg >> 30) & 1, (reg >> 29) & 1, (reg >> 28) & 1,
	    locked ? "locked" : "unlocked");

	if ((m == 0) || (n == 0) || (p == 0)) {
		*freq = 0;
		return (EINVAL);
	}
	if (!locked) {
		*freq = 0;
		return (0);
	}
	*freq = ((*freq / m) * n) / p;
	return (0);
}

static int
pll_register(struct clkdom *clkdom, struct clk_pll_def *clkdef)
{
	struct clknode *clk;
	struct pll_sc *sc;

	clk = clknode_create(clkdom, &tegra210_pll_class, &clkdef->clkdef);
	if (clk == NULL)
		return (ENXIO);

	sc = clknode_get_softc(clk);
	sc->clkdev = clknode_get_device(clk);
	sc->type = clkdef->type;
	sc->base_reg = clkdef->base_reg;
	sc->misc_reg = clkdef->misc_reg;
	sc->lock_enable = clkdef->lock_enable;
	sc->iddq_reg = clkdef->iddq_reg;
	sc->iddq_mask = clkdef->iddq_mask;
	sc->flags = clkdef->flags;
	sc->pdiv_table = clkdef->pdiv_table;
	sc->mnp_bits = clkdef->mnp_bits;
	clknode_register(clkdom, clk);
	return (0);
}

static void config_utmi_pll(struct tegra210_car_softc *sc)
{
	uint32_t reg;
	/*
	 * XXX Simplified UTMIP settings for 38.4MHz base clock.
	 */
#define	ENABLE_DELAY_COUNT 	0x00
#define	STABLE_COUNT		0x00
#define	ACTIVE_DELAY_COUNT	0x06
#define	XTAL_FREQ_COUNT		0x80

	CLKDEV_READ_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, &reg);
	reg &= ~UTMIPLL_HW_PWRDN_CFG0_IDDQ_OVERRIDE;
	CLKDEV_WRITE_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, reg);

	CLKDEV_READ_4(sc->dev, UTMIP_PLL_CFG2, &reg);
	reg &= ~UTMIP_PLL_CFG2_STABLE_COUNT(~0);
	reg |= UTMIP_PLL_CFG2_STABLE_COUNT(STABLE_COUNT);
	reg &= ~UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(~0);
	reg |= UTMIP_PLL_CFG2_ACTIVE_DLY_COUNT(ACTIVE_DELAY_COUNT);
	CLKDEV_WRITE_4(sc->dev, UTMIP_PLL_CFG2, reg);

	CLKDEV_READ_4(sc->dev, UTMIP_PLL_CFG1, &reg);
	reg &= ~UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(~0);
	reg |= UTMIP_PLL_CFG1_ENABLE_DLY_COUNT(ENABLE_DELAY_COUNT);
	reg &= ~UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(~0);
	reg |= UTMIP_PLL_CFG1_XTAL_FREQ_COUNT(XTAL_FREQ_COUNT);
	reg |= UTMIP_PLL_CFG1_FORCE_PLLU_POWERUP;
	CLKDEV_WRITE_4(sc->dev, UTMIP_PLL_CFG1, reg);

	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	reg |= UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP;
	CLKDEV_WRITE_4(sc->dev, UTMIP_PLL_CFG1, reg);
	DELAY(20);

	/* Setup samplers. */
	CLKDEV_READ_4(sc->dev, UTMIP_PLL_CFG2, &reg);
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERUP;
	reg |= UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERUP;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_A_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_B_POWERDOWN;
	reg &= ~UTMIP_PLL_CFG2_FORCE_PD_SAMP_D_POWERDOWN;
	CLKDEV_WRITE_4(sc->dev, UTMIP_PLL_CFG2, reg);

	/* Powerup UTMIP. */
	CLKDEV_READ_4(sc->dev, UTMIP_PLL_CFG1, &reg);
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERUP;
	reg &= ~UTMIP_PLL_CFG1_FORCE_PLL_ENABLE_POWERDOWN;
	CLKDEV_WRITE_4(sc->dev, UTMIP_PLL_CFG1, reg);
	DELAY(10);

	/* Prepare UTMIP sequencer. */
	CLKDEV_READ_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, &reg);
	reg |= UTMIPLL_HW_PWRDN_CFG0_USE_LOCKDET;
	reg &= ~UTMIPLL_HW_PWRDN_CFG0_CLK_ENABLE_SWCTL;
	CLKDEV_WRITE_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, reg);
	DELAY(10);

	CLKDEV_READ_4(sc->dev, XUSB_PLL_CFG0, &reg);
	reg &= ~XUSB_PLL_CFG0_UTMIPLL_LOCK_DLY;
	CLKDEV_WRITE_4(sc->dev, XUSB_PLL_CFG0, reg);
	DELAY(10);

	/* HW control of UTMIPLL. */
	CLKDEV_READ_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, &reg);
	reg |= UTMIPLL_HW_PWRDN_CFG0_SEQ_ENABLE;
	CLKDEV_WRITE_4(sc->dev, UTMIPLL_HW_PWRDN_CFG0, reg);
}

void
tegra210_init_plls(struct tegra210_car_softc *sc)
{
	int i, rv;

	for (i = 0; i < nitems(tegra210_pll_sources); i++) {
		rv = clknode_mux_register(sc->clkdom, tegra210_pll_sources + i);
		if (rv != 0)
			panic("clk_mux_register failed");
	}

	for (i = 0; i < nitems(pll_clks); i++) {
		rv = pll_register(sc->clkdom, pll_clks + i);
		if (rv != 0)
			panic("pll_register failed");
	}

	config_utmi_pll(sc);

	for (i = 0; i < nitems(tegra210_pll_fdivs); i++) {
		rv = clknode_fixed_register(sc->clkdom, tegra210_pll_fdivs + i);
		if (rv != 0)
			panic("clk_fixed_register failed");
	}

	for (i = 0; i < nitems(tegra210_pll_gates); i++) {
		rv = clknode_gate_register(sc->clkdom, tegra210_pll_gates + i);
		if (rv != 0)
			panic("clk_gate_register failed");
	}

	for (i = 0; i < nitems(tegra210_pll_divs); i++) {
		rv = clknode_div_register(sc->clkdom, tegra210_pll_divs + i);
		if (rv != 0)
			panic("clk_div_register failed");
	}
}
