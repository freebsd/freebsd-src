/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Soren Schmidt <sos@deepcore.dk>
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>

#include <dev/fdt/simplebus.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/rockchip/clk/rk_cru.h>
#include <contrib/device-tree/include/dt-bindings/clock/rk3568-cru.h>


#define	RK3568_PLLSEL_CON(x)		((x) * 0x20)
#define	RK3568_CLKSEL_CON(x)		((x) * 0x4 + 0x100)
#define	RK3568_CLKGATE_CON(x)		((x) * 0x4 + 0x300)
#define	RK3568_SOFTRST_CON(x)		((x) * 0x4 + 0x400)

#define	PNAME(_name) static const char *_name[]

#define	RK_PLLRATE(_hz, _ref, _fb, _post1, _post2, _dspd)		\
{									\
	.freq = _hz,							\
	.refdiv = _ref,							\
	.fbdiv = _fb,							\
	.postdiv1 = _post1,						\
	.postdiv2 = _post2,						\
	.dsmpd = _dspd,							\
}

/* PLL clock */
#define	RK_PLL(_id, _name, _pnames, _off, _shift)			\
{									\
	.type = RK3328_CLK_PLL,						\
	.clk.pll = &(struct rk_clk_pll_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pnames,				\
		.clkdef.parent_cnt = nitems(_pnames),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.base_offset = RK3568_PLLSEL_CON(_off),			\
		.mode_reg = 0xc0,					\
		.mode_shift = _shift,					\
		.rates = rk3568_pll_rates,				\
	},								\
}

/* Clock for ARM core(s) */
#define	RK_ARMDIV(_id, _nm, _pn, _r, _off, _ds, _dw, _ms, _mw, _mp, _ap)\
{									\
	.type = RK_CLK_ARMCLK,						\
	.clk.armclk = &(struct rk_clk_armclk_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _nm,					\
		.clkdef.parent_names = _pn,				\
		.clkdef.parent_cnt = nitems(_pn),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = RK3568_CLKSEL_CON(_off),		\
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

/* Composite */
#define	RK_COMPOSITE(_id, _name, _pnames, _o, _ms, _mw, _ds, _dw, _go, _gw,_f)\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pnames,				\
		.clkdef.parent_cnt = nitems(_pnames),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = RK3568_CLKSEL_CON(_o),			\
		.mux_shift = _ms,					\
		.mux_width = _mw,					\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.gate_offset = RK3568_CLKGATE_CON(_go),			\
		.gate_shift = _gw,					\
		.flags = RK_CLK_COMPOSITE_HAVE_MUX |			\
			 RK_CLK_COMPOSITE_HAVE_GATE | _f,		\
	},								\
}

/* Composite no mux */
#define	RK_COMPNOMUX(_id, _name, _pname, _o, _ds, _dw, _go, _gw, _f)	\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = RK3568_CLKSEL_CON(_o),			\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.gate_offset = RK3568_CLKGATE_CON(_go),			\
		.gate_shift = _gw,					\
		.flags = RK_CLK_COMPOSITE_HAVE_GATE | _f,		\
	},								\
}

/* Composite no div */
#define	RK_COMPNODIV(_id, _name, _pnames, _o, _ms, _mw, _go, _gw, _f)	\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pnames,				\
		.clkdef.parent_cnt =  nitems(_pnames),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = RK3568_CLKSEL_CON(_o),			\
		.mux_shift = _ms,					\
		.mux_width = _mw,					\
		.gate_offset = RK3568_CLKGATE_CON(_go),			\
		.gate_shift = _gw,					\
		.flags = RK_CLK_COMPOSITE_HAVE_MUX |			\
			 RK_CLK_COMPOSITE_HAVE_GATE | _f,		\
	},								\
}

/* Composite div only */
#define	RK_COMPDIV(_id, _name, _pname, _o, _ds, _dw, _f)		\
{									\
	.type = RK_CLK_COMPOSITE,					\
	.clk.composite = &(struct rk_clk_composite_def) {		\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.muxdiv_offset = RK3568_CLKSEL_CON(_o),			\
		.div_shift = _ds,					\
		.div_width = _dw,					\
		.flags =  _f,						\
	},								\
}


/* Fixed factor mux/div */
#define	RK_FACTOR(_id, _name, _pname, _mult, _div)			\
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

/* Fractional */
#define	RK_FRACTION(_id, _name, _pname, _o, _go, _gw, _f)		\
{									\
	.type = RK_CLK_FRACT,						\
	.clk.fract = &(struct rk_clk_fract_def) {			\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = (const char *[]){_pname},	\
		.clkdef.parent_cnt = 1,					\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = RK3568_CLKSEL_CON(_o),			\
		.gate_offset = RK3568_CLKGATE_CON(_go),			\
		.gate_shift = _gw,					\
		.flags = RK_CLK_FRACT_HAVE_GATE | _f,			\
	},								\
}

/* Multiplexer */
#define	RK_MUX(_id, _name, _pnames, _o, _ms, _mw, _f)			\
{									\
	.type = RK_CLK_MUX,						\
	.clk.mux = &(struct rk_clk_mux_def) {				\
		.clkdef.id = _id,					\
		.clkdef.name = _name,					\
		.clkdef.parent_names = _pnames,				\
		.clkdef.parent_cnt = nitems(_pnames),			\
		.clkdef.flags = CLK_NODE_STATIC_STRINGS,		\
		.offset = RK3568_CLKSEL_CON(_o),			\
		.shift = _ms,						\
		.width = _mw,						\
		.mux_flags = _f,					\
	},								\
}

#define	RK_GATE(_id, _name, _pname, _o, _s)				\
{									\
	.id = _id,							\
	.name = _name,							\
	.parent_name = _pname,						\
	.offset = RK3568_CLKGATE_CON(_o),				\
	.shift = _s,							\
}

struct rk_clk_pll_rate rk3568_pll_rates[] = {
	/* _mhz, _refdiv, _fbdiv, _postdiv1, _postdiv2, _dsmpd */
	RK_PLLRATE(2208000000, 1, 92, 1, 1, 1),
	RK_PLLRATE(2184000000, 1, 91, 1, 1, 1),
	RK_PLLRATE(2160000000, 1, 90, 1, 1, 1),
	RK_PLLRATE(2088000000, 1, 87, 1, 1, 1),
	RK_PLLRATE(2064000000, 1, 86, 1, 1, 1),
	RK_PLLRATE(2040000000, 1, 85, 1, 1, 1),
	RK_PLLRATE(2016000000, 1, 84, 1, 1, 1),
	RK_PLLRATE(1992000000, 1, 83, 1, 1, 1),
	RK_PLLRATE(1920000000, 1, 80, 1, 1, 1),
	RK_PLLRATE(1896000000, 1, 79, 1, 1, 1),
	RK_PLLRATE(1800000000, 1, 75, 1, 1, 1),
	RK_PLLRATE(1704000000, 1, 71, 1, 1, 1),
	RK_PLLRATE(1608000000, 1, 67, 1, 1, 1),
	RK_PLLRATE(1600000000, 3, 200, 1, 1, 1),
	RK_PLLRATE(1584000000, 1, 132, 2, 1, 1),
	RK_PLLRATE(1560000000, 1, 130, 2, 1, 1),
	RK_PLLRATE(1536000000, 1, 128, 2, 1, 1),
	RK_PLLRATE(1512000000, 1, 126, 2, 1, 1),
	RK_PLLRATE(1488000000, 1, 124, 2, 1, 1),
	RK_PLLRATE(1464000000, 1, 122, 2, 1, 1),
	RK_PLLRATE(1440000000, 1, 120, 2, 1, 1),
	RK_PLLRATE(1416000000, 1, 118, 2, 1, 1),
	RK_PLLRATE(1400000000, 3, 350, 2, 1, 1),
	RK_PLLRATE(1392000000, 1, 116, 2, 1, 1),
	RK_PLLRATE(1368000000, 1, 114, 2, 1, 1),
	RK_PLLRATE(1344000000, 1, 112, 2, 1, 1),
	RK_PLLRATE(1320000000, 1, 110, 2, 1, 1),
	RK_PLLRATE(1296000000, 1, 108, 2, 1, 1),
	RK_PLLRATE(1272000000, 1, 106, 2, 1, 1),
	RK_PLLRATE(1248000000, 1, 104, 2, 1, 1),
	RK_PLLRATE(1200000000, 1, 100, 2, 1, 1),
	RK_PLLRATE(1188000000, 1, 99, 2, 1, 1),
	RK_PLLRATE(1104000000, 1, 92, 2, 1, 1),
	RK_PLLRATE(1100000000, 3, 275, 2, 1, 1),
	RK_PLLRATE(1008000000, 1, 84, 2, 1, 1),
	RK_PLLRATE(1000000000, 3, 250, 2, 1, 1),
	RK_PLLRATE(912000000, 1, 76, 2, 1, 1),
	RK_PLLRATE(816000000, 1, 68, 2, 1, 1),
	RK_PLLRATE(800000000, 3, 200, 2, 1, 1),
	RK_PLLRATE(700000000, 3, 350, 4, 1, 1),
	RK_PLLRATE(696000000, 1, 116, 4, 1, 1),
	RK_PLLRATE(600000000, 1, 100, 4, 1, 1),
	RK_PLLRATE(594000000, 1, 99, 4, 1, 1),
	RK_PLLRATE(500000000, 1, 125, 6, 1, 1),
	RK_PLLRATE(408000000, 1, 68, 2, 2, 1),
	RK_PLLRATE(312000000, 1, 78, 6, 1, 1),
	RK_PLLRATE(216000000, 1, 72, 4, 2, 1),
	RK_PLLRATE(200000000, 1, 100, 3, 4, 1),
	RK_PLLRATE(148500000, 1, 99, 4, 4, 1),
	RK_PLLRATE(100000000, 1, 150, 6, 6, 1),
	RK_PLLRATE(96000000, 1, 96, 6, 4, 1),
	RK_PLLRATE(74250000, 2, 99, 4, 4, 1),
	{},
};

static struct rk_clk_armclk_rates rk3568_armclk_rates[] = {
	{2208000000, 1},
	{2160000000, 1},
	{2064000000, 1},
	{2016000000, 1},
	{1992000000, 1},
	{1800000000, 1},
	{1704000000, 1},
	{1608000000, 1},
	{1512000000, 1},
	{1488000000, 1},
	{1416000000, 1},
	{1200000000, 1},
	{1104000000, 1},
	{1008000000, 1},
	{ 816000000, 1},
	{ 696000000, 1},
	{ 600000000, 1},
	{ 408000000, 1},
	{ 312000000, 1},
	{ 216000000, 1},
	{  96000000, 1},
	{},
};

/* Parent clock defines */
PNAME(mux_pll_p) = { "xin24m" };
PNAME(mux_usb480m_p) = { "xin24m", "usb480m_phy", "clk_rtc_32k" };
PNAME(mux_armclk_p) = { "apll", "gpll" };
PNAME(clk_i2s0_8ch_tx_p) = { "clk_i2s0_8ch_tx_src", "clk_i2s0_8ch_tx_frac",
    "i2s0_mclkin", "xin_osc0_half" };
PNAME(clk_i2s0_8ch_rx_p) = { "clk_i2s0_8ch_rx_src", "clk_i2s0_8ch_rx_frac",
    "i2s0_mclkin", "xin_osc0_half" };
PNAME(clk_i2s1_8ch_tx_p) = { "clk_i2s1_8ch_tx_src", "clk_i2s1_8ch_tx_frac",
    "i2s1_mclkin", "xin_osc0_half" };
PNAME(clk_i2s1_8ch_rx_p) = { "clk_i2s1_8ch_rx_src", "clk_i2s1_8ch_rx_frac",
    "i2s1_mclkin", "xin_osc0_half" };
PNAME(clk_i2s2_2ch_p) = { "clk_i2s2_2ch_src", "clk_i2s2_2ch_frac",
    "i2s2_mclkin", "xin_osc0_half"};
PNAME(clk_i2s3_2ch_tx_p) = { "clk_i2s3_2ch_tx_src", "clk_i2s3_2ch_tx_frac",
    "i2s3_mclkin", "xin_osc0_half" };
PNAME(clk_i2s3_2ch_rx_p) = { "clk_i2s3_2ch_rx_src", "clk_i2s3_2ch_rx_frac",
    "i2s3_mclkin", "xin_osc0_half" };
PNAME(mclk_spdif_8ch_p) = { "mclk_spdif_8ch_src", "mclk_spdif_8ch_frac" };
PNAME(sclk_audpwm_p) = { "sclk_audpwm_src", "sclk_audpwm_frac" };
PNAME(sclk_uart1_p) = { "clk_uart1_src", "clk_uart1_frac", "xin24m" };
PNAME(sclk_uart2_p) = { "clk_uart2_src", "clk_uart2_frac", "xin24m" };
PNAME(sclk_uart3_p) = { "clk_uart3_src", "clk_uart3_frac", "xin24m" };
PNAME(sclk_uart4_p) = { "clk_uart4_src", "clk_uart4_frac", "xin24m" };
PNAME(sclk_uart5_p) = { "clk_uart5_src", "clk_uart5_frac", "xin24m" };
PNAME(sclk_uart6_p) = { "clk_uart6_src", "clk_uart6_frac", "xin24m" };
PNAME(sclk_uart7_p) = { "clk_uart7_src", "clk_uart7_frac", "xin24m" };
PNAME(sclk_uart8_p) = { "clk_uart8_src", "clk_uart8_frac", "xin24m" };
PNAME(sclk_uart9_p) = { "clk_uart9_src", "clk_uart9_frac", "xin24m" };
PNAME(mpll_gpll_cpll_npll_p) = { "mpll", "gpll", "cpll", "npll" };
PNAME(gpll_cpll_npll_p) = { "gpll", "cpll", "npll" };
PNAME(npll_gpll_p) = { "npll", "gpll" };
PNAME(cpll_gpll_p) = { "cpll", "gpll" };
PNAME(gpll_cpll_p) = { "gpll", "cpll" };
PNAME(gpll_cpll_npll_vpll_p) = { "gpll", "cpll", "npll", "vpll" };
PNAME(apll_gpll_npll_p) = { "apll", "gpll", "npll" };
PNAME(sclk_core_pre_p) = { "sclk_core_src", "npll" };
PNAME(gpll150_gpll100_gpll75_xin24m_p) = { "gpll_150m", "gpll_100m", "gpll_75m",
    "xin24m" };
PNAME(clk_gpu_pre_mux_p) = { "clk_gpu_src", "gpu_pvtpll_out" };
PNAME(clk_npu_pre_ndft_p) = { "clk_npu_src", "dummy"};
PNAME(clk_npu_p) = { "clk_npu_pre_ndft", "npu_pvtpll_out" };
PNAME(dpll_gpll_cpll_p) = { "dpll", "gpll", "cpll" };
PNAME(clk_ddr1x_p) = { "clk_ddrphy1x_src", "dpll" };
PNAME(gpll200_gpll150_gpll100_xin24m_p) = { "gpll_200m", "gpll_150m",
    "gpll_100m", "xin24m" };
PNAME(gpll100_gpll75_gpll50_p) = { "gpll_100m", "gpll_75m", "cpll_50m" };
PNAME(i2s0_mclkout_tx_p) = { "clk_i2s0_8ch_tx", "xin_osc0_half" };
PNAME(i2s0_mclkout_rx_p) = { "clk_i2s0_8ch_rx", "xin_osc0_half" };
PNAME(i2s1_mclkout_tx_p) = { "clk_i2s1_8ch_tx", "xin_osc0_half" };
PNAME(i2s1_mclkout_rx_p) = { "clk_i2s1_8ch_rx", "xin_osc0_half" };
PNAME(i2s2_mclkout_p) = { "clk_i2s2_2ch", "xin_osc0_half" };
PNAME(i2s3_mclkout_tx_p) = { "clk_i2s3_2ch_tx", "xin_osc0_half" };
PNAME(i2s3_mclkout_rx_p) = { "clk_i2s3_2ch_rx", "xin_osc0_half" };
PNAME(mclk_pdm_p) = { "gpll_300m", "cpll_250m", "gpll_200m", "gpll_100m" };
PNAME(clk_i2c_p) = { "gpll_200m", "gpll_100m", "xin24m", "cpll_100m" };
PNAME(gpll200_gpll150_gpll100_p) = { "gpll_200m", "gpll_150m", "gpll_100m" };
PNAME(gpll300_gpll200_gpll100_p) = { "gpll_300m", "gpll_200m", "gpll_100m" };
PNAME(clk_nandc_p) = { "gpll_200m", "gpll_150m", "cpll_100m", "xin24m" };
PNAME(sclk_sfc_p) = { "xin24m", "cpll_50m", "gpll_75m", "gpll_100m",
    "cpll_125m", "gpll_150m" };
PNAME(gpll200_gpll150_cpll125_p) = { "gpll_200m", "gpll_150m", "cpll_125m" };
PNAME(cclk_emmc_p) = { "xin24m", "gpll_200m", "gpll_150m", "cpll_100m",
    "cpll_50m", "clk_osc0_div_375k" };
PNAME(aclk_pipe_p) = { "gpll_400m", "gpll_300m", "gpll_200m", "xin24m" };
PNAME(gpll200_cpll125_p) = { "gpll_200m", "cpll_125m" };
PNAME(gpll300_gpll200_gpll100_xin24m_p) = { "gpll_300m", "gpll_200m",
    "gpll_100m", "xin24m" };
PNAME(clk_sdmmc_p) = { "xin24m", "gpll_400m", "gpll_300m", "cpll_100m",
    "cpll_50m", "clk_osc0_div_750k" };
PNAME(cpll125_cpll50_cpll25_xin24m_p) = { "cpll_125m", "cpll_50m", "cpll_25m",
    "xin24m" };
PNAME(clk_gmac_ptp_p) = { "cpll_62p5", "gpll_100m", "cpll_50m", "xin24m" };
PNAME(cpll333_gpll300_gpll200_p) = { "cpll_333m", "gpll_300m", "gpll_200m" };
PNAME(cpll_gpll_hpll_p) = { "cpll", "gpll", "hpll" };
PNAME(gpll_usb480m_xin24m_p) = { "gpll", "usb480m", "xin24m", "xin24m" };
PNAME(gpll300_cpll250_gpll100_xin24m_p) = { "gpll_300m", "cpll_250m",
    "gpll_100m", "xin24m" };
PNAME(cpll_gpll_hpll_vpll_p) = { "cpll", "gpll", "hpll", "vpll" };
PNAME(hpll_vpll_gpll_cpll_p) = { "hpll", "vpll", "gpll", "cpll" };
PNAME(gpll400_cpll333_gpll200_p) = { "gpll_400m", "cpll_333m", "gpll_200m" };
PNAME(gpll100_gpll75_cpll50_xin24m_p) = { "gpll_100m", "gpll_75m", "cpll_50m",
    "xin24m" };
PNAME(xin24m_gpll100_cpll100_p) = { "xin24m", "gpll_100m", "cpll_100m" };
PNAME(gpll_cpll_usb480m_p) = { "gpll", "cpll", "usb480m" };
PNAME(gpll100_xin24m_cpll100_p) = { "gpll_100m", "xin24m", "cpll_100m" };
PNAME(gpll200_xin24m_cpll100_p) = { "gpll_200m", "xin24m", "cpll_100m" };
PNAME(xin24m_32k_p) = { "xin24m", "clk_rtc_32k" };
PNAME(cpll500_gpll400_gpll300_xin24m_p) = { "cpll_500m", "gpll_400m",
    "gpll_300m", "xin24m" };
PNAME(gpll400_gpll300_gpll200_xin24m_p) = { "gpll_400m", "gpll_300m",
    "gpll_200m", "xin24m" };
PNAME(xin24m_cpll100_p) = { "xin24m", "cpll_100m" };
PNAME(mux_gmac0_p) = { "clk_mac0_2top", "gmac0_clkin" };
PNAME(mux_gmac0_rgmii_speed_p) = { "clk_gmac0", "clk_gmac0",
    "clk_gmac0_tx_div50", "clk_gmac0_tx_div5" };
PNAME(mux_gmac0_rmii_speed_p) = { "clk_gmac0_rx_div20", "clk_gmac0_rx_div2" };
PNAME(mux_gmac0_rx_tx_p) = { "clk_gmac0_rgmii_speed", "clk_gmac0_rmii_speed",
    "clk_gmac0_xpcs_mii" };
PNAME(mux_gmac1_p) = { "clk_mac1_2top", "gmac1_clkin" };
PNAME(mux_gmac1_rgmii_speed_p) = { "clk_gmac1", "clk_gmac1",
    "clk_gmac1_tx_div50", "clk_gmac1_tx_div5" };
PNAME(mux_gmac1_rmii_speed_p) = { "clk_gmac1_rx_div20", "clk_gmac1_rx_div2" };
PNAME(mux_gmac1_rx_tx_p) = { "clk_gmac1_rgmii_speed", "clk_gmac1_rmii_speed",
    "clk_gmac1_xpcs_mii" };
PNAME(clk_mac_2top_p) = { "cpll_125m", "cpll_50m", "cpll_25m", "ppll" };
PNAME(aclk_rkvdec_pre_p) = { "gpll", "cpll" };
PNAME(clk_rkvdec_core_p) = { "gpll", "cpll", "npll", "vpll" };

/* CLOCKS */
static struct rk_clk rk3568_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	LINK("clk_rtc_32k"),
	LINK("usb480m_phy"),
	LINK("mpll"),	// SOS SCRU
	LINK("i2s0_mclkin"),
	LINK("i2s1_mclkin"),
	LINK("i2s2_mclkin"),
	LINK("i2s3_mclkin"),
	LINK("gpu_pvtpll_out"),
	LINK("npu_pvtpll_out"),
	LINK("gmac0_clkin"),
	LINK("gmac1_clkin"),
	LINK("clk_gmac0_xpcs_mii"),
	LINK("clk_gmac1_xpcs_mii"),
	LINK("dummy"),

	/* PLL's */
	RK_PLL(PLL_APLL, "apll", mux_pll_p, 0, 0),
	RK_PLL(PLL_DPLL, "dpll", mux_pll_p, 1, 2),
	RK_PLL(PLL_GPLL, "gpll", mux_pll_p, 2, 6),
	RK_PLL(PLL_CPLL, "cpll", mux_pll_p, 3, 4),
	RK_PLL(PLL_NPLL, "npll", mux_pll_p, 4, 10),
	RK_PLL(PLL_VPLL, "vpll", mux_pll_p, 5, 12),
	RK_ARMDIV(ARMCLK, "armclk", mux_armclk_p, rk3568_armclk_rates, 0, 0, 5,
	    6, 1, 0, 1),
	RK_FACTOR(0, "clk_osc0_div_375k", "clk_osc0_div_750k", 1, 2),
	RK_FACTOR(0, "xin_osc0_half", "xin24m", 1, 2),
	RK_MUX(USB480M, "usb480m", mux_usb480m_p, -16, 14, 2, 0),

	/* Clocks */
	RK_COMPNOMUX(0, "gpll_400m", "gpll", 75, 0, 5, 35, 0, 0),
	RK_COMPNOMUX(0, "gpll_300m", "gpll", 75, 8, 5, 35, 1, 0),
	RK_COMPNOMUX(0, "gpll_200m", "gpll", 76, 0, 5, 35, 2, 0),
	RK_COMPNOMUX(0, "gpll_150m", "gpll", 76, 8, 5, 35, 3, 0),
	RK_COMPNOMUX(0, "gpll_100m", "gpll", 77, 0, 5, 35, 4, 0),
	RK_COMPNOMUX(0, "gpll_75m", "gpll", 77, 8, 5, 35, 5, 0),
	RK_COMPNOMUX(0, "gpll_20m", "gpll", 78, 0, 6, 35, 6, 0),
	RK_COMPNOMUX(CPLL_500M, "cpll_500m", "cpll", 78, 8, 5, 35, 7, 0),
	RK_COMPNOMUX(CPLL_333M, "cpll_333m", "cpll", 79, 0, 5, 35, 8, 0),
	RK_COMPNOMUX(CPLL_250M, "cpll_250m", "cpll", 79, 8, 5, 35, 9, 0),
	RK_COMPNOMUX(CPLL_125M, "cpll_125m", "cpll", 80, 0, 5, 35, 10, 0),
	RK_COMPNOMUX(CPLL_100M, "cpll_100m", "cpll", 82, 0, 5, 35, 11, 0),
	RK_COMPNOMUX(CPLL_62P5M, "cpll_62p5", "cpll", 80, 8, 5, 35, 12, 0),
	RK_COMPNOMUX(CPLL_50M, "cpll_50m", "cpll", 81, 0, 5, 35, 13, 0),
	RK_COMPNOMUX(CPLL_25M, "cpll_25m", "cpll", 81, 8, 6, 35, 14, 0),
	RK_COMPNOMUX(0, "clk_osc0_div_750k", "xin24m", 82, 8, 6, 35, 15, 0),
	RK_COMPOSITE(0, "sclk_core_src", apll_gpll_npll_p, 2, 8, 2, 0, 4, 0,
	    5, 0),
	RK_COMPNODIV(0, "sclk_core", sclk_core_pre_p, 2, 15, 1, 0, 7, 0),
	RK_COMPNOMUX(0, "atclk_core", "armclk", 3, 0, 5, 0, 8, 0),
	RK_COMPNOMUX(0, "gicclk_core", "armclk", 3, 8, 5, 0, 9, 0),
	RK_COMPNOMUX(0, "pclk_core_pre", "armclk", 4, 0, 5, 0, 10, 0),
	RK_COMPNOMUX(0, "periphclk_core_pre", "armclk", 4, 8, 5, 0, 11, 0),
	RK_COMPNOMUX(0, "tsclk_core", "periphclk_core_pre", 5, 0, 4, 0, 14, 0),
	RK_COMPNOMUX(0, "cntclk_core", "periphclk_core_pre", 5, 4, 4, 0, 15, 0),
	RK_COMPNOMUX(0, "aclk_core", "sclk_core", 5, 8, 5, 1, 0, 0),
	RK_COMPNODIV(ACLK_CORE_NIU2BUS, "aclk_core_niu2bus",
	    gpll150_gpll100_gpll75_xin24m_p, 5, 14, 2, 1, 2, 0),
	RK_COMPOSITE(CLK_GPU_SRC, "clk_gpu_src", mpll_gpll_cpll_npll_p, 6, 6, 2,
	    0, 4, 2, 0, 0),
	RK_MUX(CLK_GPU_PRE_MUX, "clk_gpu_pre_mux", clk_gpu_pre_mux_p, 6, 11,
	    1, 0),
	RK_COMPDIV(ACLK_GPU_PRE, "aclk_gpu_pre", "clk_gpu_pre_mux", 6, 8, 2, 0),
	RK_COMPDIV(PCLK_GPU_PRE, "pclk_gpu_pre", "clk_gpu_pre_mux", 6, 12, 4,0),
	RK_COMPOSITE(CLK_NPU_SRC, "clk_npu_src", npll_gpll_p, 7, 6, 1, 0, 4, 3,
	    0, 0),
	RK_MUX(CLK_NPU_PRE_NDFT, "clk_npu_pre_ndft", clk_npu_pre_ndft_p, 7, 8,
	    1, 0),
	RK_MUX(CLK_NPU, "clk_npu", clk_npu_p, 7, 15, 1, 0),
	RK_COMPNOMUX(HCLK_NPU_PRE, "hclk_npu_pre", "clk_npu", 8, 0, 4, 3, 2, 0),
	RK_COMPNOMUX(PCLK_NPU_PRE, "pclk_npu_pre", "clk_npu", 8, 4, 4, 3, 3, 0),
	RK_COMPOSITE(CLK_DDRPHY1X_SRC, "clk_ddrphy1x_src", dpll_gpll_cpll_p, 9,
	    6, 2, 0, 5, 4, 0, 0),
	RK_MUX(CLK_DDR1X, "clk_ddr1x", clk_ddr1x_p, 9, 15, 1,
	    RK_CLK_COMPOSITE_GRF),
	RK_COMPNOMUX(CLK_MSCH, "clk_msch", "clk_ddr1x", 10, 0, 2, 4, 2, 0),
	RK_COMPNODIV(ACLK_GIC_AUDIO, "aclk_gic_audio",
	    gpll200_gpll150_gpll100_xin24m_p, 10, 8, 2, 5, 0, 0),
	RK_COMPNODIV(HCLK_GIC_AUDIO, "hclk_gic_audio",
	    gpll150_gpll100_gpll75_xin24m_p, 10, 10, 2, 5, 1, 0),
	RK_COMPNODIV(DCLK_SDMMC_BUFFER, "dclk_sdmmc_buffer",
	    gpll100_gpll75_gpll50_p, 10, 12, 2, 5, 9, 0),
	RK_COMPOSITE(CLK_I2S0_8CH_TX_SRC, "clk_i2s0_8ch_tx_src",
	    gpll_cpll_npll_p, 11, 8, 2, 0, 7, 6, 0, 0),
	RK_MUX(CLK_I2S0_8CH_TX, "clk_i2s0_8ch_tx", clk_i2s0_8ch_tx_p, 11, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S0_8CH_TX_FRAC, "clk_i2s0_8ch_tx_frac",
	    "clk_i2s0_8ch_tx_src", 12, 6, 1, 0),
	RK_COMPNODIV(I2S0_MCLKOUT_TX, "i2s0_mclkout_tx", i2s0_mclkout_tx_p, 11,
	    15, 1, 6, 3, 0),
	RK_COMPOSITE(CLK_I2S0_8CH_RX_SRC, "clk_i2s0_8ch_rx_src",
	    gpll_cpll_npll_p, 13, 8, 2, 0, 7, 6, 4, 0),
	RK_MUX(CLK_I2S0_8CH_RX, "clk_i2s0_8ch_rx", clk_i2s0_8ch_rx_p, 13, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S0_8CH_RX_FRAC, "clk_i2s0_8ch_rx_frac",
	    "clk_i2s0_8ch_rx_src", 14, 6, 5, 0),
	RK_COMPNODIV(I2S0_MCLKOUT_RX, "i2s0_mclkout_rx", i2s0_mclkout_rx_p, 13,
	    15, 1, 6, 7, 0),
	RK_COMPOSITE(CLK_I2S1_8CH_TX_SRC, "clk_i2s1_8ch_tx_src",
	    gpll_cpll_npll_p, 15, 8, 2, 0, 7, 6, 8, 0),
	RK_MUX(CLK_I2S1_8CH_TX, "clk_i2s1_8ch_tx", clk_i2s1_8ch_tx_p, 15, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S1_8CH_TX_FRAC, "clk_i2s1_8ch_tx_frac",
	    "clk_i2s1_8ch_tx_src", 16, 6, 9, 0),
	RK_COMPNODIV(I2S1_MCLKOUT_TX, "i2s1_mclkout_tx", i2s1_mclkout_tx_p, 15,
	    15, 1, 6, 11, 0),
	RK_COMPOSITE(CLK_I2S1_8CH_RX_SRC, "clk_i2s1_8ch_rx_src",
	    gpll_cpll_npll_p, 17, 8, 2, 0, 7, 6, 12, 0),
	RK_MUX(CLK_I2S1_8CH_RX, "clk_i2s1_8ch_rx", clk_i2s1_8ch_rx_p, 17, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S1_8CH_RX_FRAC, "clk_i2s1_8ch_rx_frac",
	    "clk_i2s1_8ch_rx_src", 18, 6, 13, 0),
	RK_COMPNODIV(I2S1_MCLKOUT_RX, "i2s1_mclkout_rx", i2s1_mclkout_rx_p, 17,
	    15, 1, 6, 15, 0),
	RK_COMPOSITE(CLK_I2S2_2CH_SRC, "clk_i2s2_2ch_src", gpll_cpll_npll_p, 19,
	    8, 2, 0, 7, 7, 0, 0),
	RK_MUX(CLK_I2S2_2CH, "clk_i2s2_2ch", clk_i2s2_2ch_p, 19, 10, 2, 0),
	RK_FRACTION(CLK_I2S2_2CH_FRAC, "clk_i2s2_2ch_frac", "clk_i2s2_2ch_src",
	    20, 7, 1, 0),
	RK_COMPNODIV(I2S2_MCLKOUT, "i2s2_mclkout", i2s2_mclkout_p, 19, 15, 1, 7,
	    3, 0),
	RK_COMPOSITE(CLK_I2S3_2CH_TX_SRC, "clk_i2s3_2ch_tx_src",
	    gpll_cpll_npll_p, 21, 8, 2, 0, 7, 7, 4, 0),
	RK_MUX(CLK_I2S3_2CH_TX, "clk_i2s3_2ch_tx", clk_i2s3_2ch_tx_p, 21, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S3_2CH_TX_FRAC, "clk_i2s3_2ch_tx_frac",
	    "clk_i2s3_2ch_tx_src", 22, 7, 5, 0),
	RK_COMPNODIV(I2S3_MCLKOUT_TX, "i2s3_mclkout_tx", i2s3_mclkout_tx_p, 21,
	    15, 1, 7, 7, 0),
	RK_COMPOSITE(CLK_I2S3_2CH_RX_SRC, "clk_i2s3_2ch_rx_src",
	    gpll_cpll_npll_p, 83, 8, 2, 0, 7, 7, 8, 0),
	RK_MUX(CLK_I2S3_2CH_RX, "clk_i2s3_2ch_rx", clk_i2s3_2ch_rx_p, 83, 10,
	    2, 0),
	RK_FRACTION(CLK_I2S3_2CH_RX_FRAC, "clk_i2s3_2ch_rx_frac",
	    "clk_i2s3_2ch_rx_src", 84, 7, 9, 0),
	RK_COMPNODIV(I2S3_MCLKOUT_RX, "i2s3_mclkout_rx", i2s3_mclkout_rx_p, 83,
	    15, 1, 7, 11, 0),
	RK_COMPNODIV(MCLK_PDM, "mclk_pdm", mclk_pdm_p, 23, 8, 2, 5, 15, 0),
	RK_COMPOSITE(MCLK_SPDIF_8CH_SRC, "mclk_spdif_8ch_src", cpll_gpll_p, 23,
	    14, 1, 0, 7, 7, 14, 0),
	RK_MUX(MCLK_SPDIF_8CH, "mclk_spdif_8ch", mclk_spdif_8ch_p, 23, 15, 1,0),
	RK_FRACTION(MCLK_SPDIF_8CH_FRAC, "mclk_spdif_8ch_frac",
	    "mclk_spdif_8ch_src", 24, 7, 15, 0),
	RK_COMPOSITE(SCLK_AUDPWM_SRC, "sclk_audpwm_src", gpll_cpll_p, 25, 14, 1,
	    0, 6, 8, 1, 0),
	RK_MUX(SCLK_AUDPWM, "sclk_audpwm", sclk_audpwm_p, 25, 15, 1, 0),
	RK_FRACTION(SCLK_AUDPWM_FRAC, "sclk_audpwm_frac", "sclk_audpwm_src", 26,
	    8, 2, 0),
	RK_COMPNODIV(CLK_ACDCDIG_I2C, "clk_acdcdig_i2c", clk_i2c_p, 23, 10, 2,
	    8, 4, 0),
	RK_COMPNODIV(ACLK_SECURE_FLASH, "aclk_secure_flash",
	    gpll200_gpll150_gpll100_xin24m_p, 27, 0, 2, 8, 7, 0),
	RK_COMPNODIV(HCLK_SECURE_FLASH, "hclk_secure_flash",
	    gpll150_gpll100_gpll75_xin24m_p, 27, 2, 2, 8, 8, 0),
	RK_COMPNODIV(CLK_CRYPTO_NS_CORE, "clk_crypto_ns_core",
	    gpll200_gpll150_gpll100_p, 27, 4, 2, 8, 13, 0),
	RK_COMPNODIV(CLK_CRYPTO_NS_PKA, "clk_crypto_ns_pka",
	    gpll300_gpll200_gpll100_p, 27, 6, 2, 8, 14, 0),
	RK_COMPNODIV(NCLK_NANDC, "nclk_nandc", clk_nandc_p, 28, 0, 2, 9, 1, 0),
	RK_COMPNODIV(SCLK_SFC, "sclk_sfc", sclk_sfc_p, 28, 4, 3, 9, 4, 0),
	RK_COMPNODIV(BCLK_EMMC, "bclk_emmc", gpll200_gpll150_cpll125_p, 28, 8,
	    2, 9, 7, 0),
	RK_COMPNODIV(CCLK_EMMC, "cclk_emmc", cclk_emmc_p, 28, 12, 3, 9, 8, 0),
	RK_COMPNODIV(ACLK_PIPE, "aclk_pipe", aclk_pipe_p, 29, 0, 2, 10, 0, 0),
	RK_COMPNOMUX(PCLK_PIPE, "pclk_pipe", "aclk_pipe", 29, 4, 4, 10, 1, 0),
	RK_COMPNODIV(CLK_USB3OTG0_SUSPEND, "clk_usb3otg0_suspend", xin24m_32k_p,
	    29, 8, 1, 10, 10, 0),
	RK_COMPNODIV(CLK_USB3OTG1_SUSPEND, "clk_usb3otg1_suspend", xin24m_32k_p,
	    29, 9, 1, 10, 14, 0),
	RK_COMPNODIV(CLK_XPCS_EEE, "clk_xpcs_eee", gpll200_cpll125_p, 29, 13, 1,
	    10, 4, 0),
	RK_COMPNODIV(ACLK_PHP, "aclk_php", gpll300_gpll200_gpll100_xin24m_p, 30,
	    0, 2, 14, 8, 0),
	RK_COMPNODIV(HCLK_PHP, "hclk_php", gpll150_gpll100_gpll75_xin24m_p, 30,
	    2, 2, 14, 9, 0),
	RK_COMPNOMUX(PCLK_PHP, "pclk_php", "aclk_php", 30, 4, 4, 14, 10, 0),
	RK_COMPNODIV(CLK_SDMMC0, "clk_sdmmc0", clk_sdmmc_p, 30, 8, 3, 15, 1, 0),
	RK_COMPNODIV(CLK_SDMMC1, "clk_sdmmc1", clk_sdmmc_p, 30, 12, 3, 15, 3,0),
	RK_COMPNODIV(CLK_MAC0_2TOP, "clk_mac0_2top", clk_mac_2top_p, 31, 8, 2,
	    15, 7, 0),
	RK_COMPNODIV(CLK_MAC0_OUT, "clk_mac0_out",
	    cpll125_cpll50_cpll25_xin24m_p, 31, 14, 2, 15, 8, 0),
	RK_COMPNODIV(CLK_GMAC0_PTP_REF, "clk_gmac0_ptp_ref", clk_gmac_ptp_p, 31,
	    12, 2, 15, 4, 0),
	RK_MUX(SCLK_GMAC0, "clk_gmac0", mux_gmac0_p, 31, 2, 1, 0),
	RK_FACTOR(0, "clk_gmac0_tx_div5", "clk_gmac0", 1, 5),
	RK_FACTOR(0, "clk_gmac0_tx_div50", "clk_gmac0", 1, 50),
	RK_FACTOR(0, "clk_gmac0_rx_div2", "clk_gmac0", 1, 2),
	RK_FACTOR(0, "clk_gmac0_rx_div20", "clk_gmac0", 1, 20),
	RK_MUX(SCLK_GMAC0_RGMII_SPEED, "clk_gmac0_rgmii_speed",
	    mux_gmac0_rgmii_speed_p, 31, 4, 2, 0),
	RK_MUX(SCLK_GMAC0_RMII_SPEED, "clk_gmac0_rmii_speed",
	    mux_gmac0_rmii_speed_p, 31, 3, 1, 0),
	RK_MUX(SCLK_GMAC0_RX_TX, "clk_gmac0_rx_tx", mux_gmac0_rx_tx_p, 31, 0,
	    2, 0),
	RK_COMPNODIV(ACLK_USB, "aclk_usb", gpll300_gpll200_gpll100_xin24m_p, 32,
	    0, 2, 16, 0, 0),
	RK_COMPNODIV(HCLK_USB, "hclk_usb", gpll150_gpll100_gpll75_xin24m_p, 32,
	    2, 2, 16, 1, 0),
	RK_COMPNOMUX(PCLK_USB, "pclk_usb", "aclk_usb", 32, 4, 4, 16, 2, 0),
	RK_COMPNODIV(CLK_SDMMC2, "clk_sdmmc2", clk_sdmmc_p, 32, 8, 3, 17, 1, 0),
	RK_COMPNODIV(CLK_MAC1_2TOP, "clk_mac1_2top", clk_mac_2top_p, 33, 8, 2,
	    17, 5, 0),
	RK_COMPNODIV(CLK_MAC1_OUT, "clk_mac1_out",
	    cpll125_cpll50_cpll25_xin24m_p, 33, 14, 2, 17, 6, 0),
	RK_COMPNODIV(CLK_GMAC1_PTP_REF, "clk_gmac1_ptp_ref", clk_gmac_ptp_p, 33,
	    12, 2, 17, 2, 0),
	RK_MUX(SCLK_GMAC1, "clk_gmac1", mux_gmac1_p, 33, 2, 1, 0),
	RK_FACTOR(0, "clk_gmac1_tx_div5", "clk_gmac1", 1, 5),
	RK_FACTOR(0, "clk_gmac1_tx_div50", "clk_gmac1", 1, 50),
	RK_FACTOR(0, "clk_gmac1_rx_div2", "clk_gmac1", 1, 2),
	RK_FACTOR(0, "clk_gmac1_rx_div20", "clk_gmac1", 1, 20),
	RK_MUX(SCLK_GMAC1_RGMII_SPEED, "clk_gmac1_rgmii_speed",
	    mux_gmac1_rgmii_speed_p, 33, 4, 2, 0),
	RK_MUX(SCLK_GMAC1_RMII_SPEED, "clk_gmac1_rmii_speed",
	    mux_gmac1_rmii_speed_p, 33, 3, 1, 0),
	RK_MUX(SCLK_GMAC1_RX_TX, "clk_gmac1_rx_tx", mux_gmac1_rx_tx_p, 33, 0,
	    2, 0),
	RK_COMPNODIV(ACLK_PERIMID, "aclk_perimid",
	    gpll300_gpll200_gpll100_xin24m_p, 10, 4, 2, 14, 0, 0),
	RK_COMPNODIV(HCLK_PERIMID, "hclk_perimid",
	    gpll150_gpll100_gpll75_xin24m_p, 10, 6, 2, 14, 1, 0),
	RK_COMPNODIV(ACLK_VI, "aclk_vi", gpll400_gpll300_gpll200_xin24m_p, 34,
	    0, 2, 18, 0, 0),
	RK_COMPNOMUX(HCLK_VI, "hclk_vi", "aclk_vi", 34, 4, 4, 18, 1, 0),
	RK_COMPNOMUX(PCLK_VI, "pclk_vi", "aclk_vi", 34, 8, 4, 18, 2, 0),
	RK_COMPNODIV(DCLK_VICAP, "dclk_vicap", cpll333_gpll300_gpll200_p, 34,
	    14, 2, 18, 11, 0),
	RK_COMPOSITE(CLK_ISP, "clk_isp", cpll_gpll_hpll_p, 35, 6, 2, 0, 5, 19,
	    2, 0),
	RK_COMPOSITE(CLK_CIF_OUT, "clk_cif_out", gpll_usb480m_xin24m_p, 35, 14,
	    2, 8, 6, 19, 8, 0),
	RK_COMPOSITE(CLK_CAM0_OUT, "clk_cam0_out", gpll_usb480m_xin24m_p, 36, 6,
	    2, 0, 6, 19, 9, 0),
	RK_COMPOSITE(CLK_CAM1_OUT, "clk_cam1_out", gpll_usb480m_xin24m_p, 36,
	    14, 2, 8, 6, 19, 10, 0),
	RK_COMPNODIV(ACLK_VO, "aclk_vo", gpll300_cpll250_gpll100_xin24m_p, 37,
	    0, 2, 20, 0, 0),
	RK_COMPNOMUX(HCLK_VO, "hclk_vo", "aclk_vo", 37, 8, 4, 20, 1, 0),
	RK_COMPNOMUX(PCLK_VO, "pclk_vo", "aclk_vo", 37, 12, 4, 20, 2, 0),
	RK_COMPOSITE(ACLK_VOP_PRE, "aclk_vop_pre", cpll_gpll_hpll_vpll_p, 38, 6,
	    2, 0, 5, 20, 6, 0),
	RK_COMPOSITE(DCLK_VOP0, "dclk_vop0", hpll_vpll_gpll_cpll_p, 39, 10, 2,
	    0, 8, 20, 10, 0),
	RK_COMPOSITE(DCLK_VOP1, "dclk_vop1", hpll_vpll_gpll_cpll_p, 40, 10, 2,
	    0, 8, 20, 11, 0),
	RK_COMPOSITE(DCLK_VOP2, "dclk_vop2", hpll_vpll_gpll_cpll_p, 41, 10, 2,
	    0, 8, 20, 12, 0),
	RK_COMPNODIV(CLK_EDP_200M, "clk_edp_200m", gpll200_gpll150_cpll125_p,
	    38, 8, 2, 21, 9, 0),
	RK_COMPOSITE(ACLK_VPU_PRE, "aclk_vpu_pre", gpll_cpll_p, 42, 7, 1, 0, 5,
	    22, 0, 0),
	RK_COMPNOMUX(HCLK_VPU_PRE, "hclk_vpu_pre", "aclk_vpu_pre", 42, 8, 4, 22,
	    1, 0),
	RK_COMPNODIV(ACLK_RGA_PRE, "aclk_rga_pre",
	    gpll300_cpll250_gpll100_xin24m_p, 43, 0, 2, 23, 0, 0),
	RK_COMPNOMUX(HCLK_RGA_PRE, "hclk_rga_pre", "aclk_rga_pre", 43, 8, 4, 23,
	    1, 0),
	RK_COMPNOMUX(PCLK_RGA_PRE, "pclk_rga_pre", "aclk_rga_pre", 43, 12, 4,
	    22, 12, 0),
	RK_COMPNODIV(CLK_RGA_CORE, "clk_rga_core", gpll300_gpll200_gpll100_p,
	    43, 2, 2, 23, 6, 0),
	RK_COMPNODIV(CLK_IEP_CORE, "clk_iep_core", gpll300_gpll200_gpll100_p,
	    43, 4, 2, 23, 9, 0),
	RK_COMPNODIV(DCLK_EBC, "dclk_ebc", gpll400_cpll333_gpll200_p, 43, 6, 2,
	    23, 11, 0),
	RK_COMPOSITE(ACLK_RKVENC_PRE, "aclk_rkvenc_pre", gpll_cpll_npll_p, 44,
	    6, 2, 0, 5, 24, 0, 0),
	RK_COMPNOMUX(HCLK_RKVENC_PRE, "hclk_rkvenc_pre", "aclk_rkvenc_pre", 44,
	    8, 4, 24, 1, 0),
	RK_COMPOSITE(CLK_RKVENC_CORE, "clk_rkvenc_core", gpll_cpll_npll_vpll_p,
	    45, 14, 2, 0, 5, 24, 8, 0),
	RK_COMPOSITE(ACLK_RKVDEC_PRE, "aclk_rkvdec_pre", aclk_rkvdec_pre_p, 47,
	    7, 1, 0, 5, 25, 0, 0),
	RK_COMPNOMUX(HCLK_RKVDEC_PRE, "hclk_rkvdec_pre", "aclk_rkvdec_pre", 47,
	    8, 4, 25, 1, 0),
	RK_COMPOSITE(CLK_RKVDEC_CA, "clk_rkvdec_ca", gpll_cpll_npll_vpll_p, 48,
	    6, 2, 0, 5, 25, 6, 0),
	RK_COMPOSITE(CLK_RKVDEC_CORE, "clk_rkvdec_core", clk_rkvdec_core_p, 49,
	    14, 2, 8, 5, 25, 7, 0),
	RK_COMPOSITE(CLK_RKVDEC_HEVC_CA, "clk_rkvdec_hevc_ca",
	    gpll_cpll_npll_vpll_p, 49, 6, 2, 0, 5, 25, 8, 0),
	RK_COMPNODIV(ACLK_BUS, "aclk_bus", gpll200_gpll150_gpll100_xin24m_p, 50,
	    0, 2, 26, 0, 0),
	RK_COMPNODIV(PCLK_BUS, "pclk_bus", gpll100_gpll75_cpll50_xin24m_p, 50,
	    4, 2, 26, 1, 0),
	RK_COMPOSITE(CLK_TSADC_TSEN, "clk_tsadc_tsen", xin24m_gpll100_cpll100_p,
	    51, 4, 2, 0, 3, 26, 5, 0),
	RK_COMPNOMUX(CLK_TSADC, "clk_tsadc", "clk_tsadc_tsen", 51, 8, 7, 26,
	    6, 0),
	RK_COMPOSITE(CLK_UART1_SRC, "clk_uart1_src", gpll_cpll_usb480m_p, 52, 8,
	    2, 0, 7, 27, 13, 0),
	RK_FRACTION(CLK_UART1_FRAC, "clk_uart1_frac", "clk_uart1_src", 53, 27,
	    14, 0),
	RK_MUX(0, "sclk_uart1_mux", sclk_uart1_p, 52, 12, 2, 0),
	RK_COMPOSITE(CLK_UART2_SRC, "clk_uart2_src", gpll_cpll_usb480m_p, 54, 8,
	    2, 0, 7, 28, 1, 0),
	RK_FRACTION(CLK_UART2_FRAC, "clk_uart2_frac", "clk_uart2_src", 55, 28,
	    2, 0),
	RK_MUX(0, "sclk_uart2_mux", sclk_uart2_p, 54, 12, 2, 0),
	RK_COMPOSITE(CLK_UART3_SRC, "clk_uart3_src", gpll_cpll_usb480m_p, 56, 8,
	    2, 0, 7, 28, 5, 0),
	RK_FRACTION(CLK_UART3_FRAC, "clk_uart3_frac", "clk_uart3_src", 57, 28,
	    6, 0),
	RK_MUX(0, "sclk_uart3_mux", sclk_uart3_p, 56, 12, 2, 0),
	RK_COMPOSITE(CLK_UART4_SRC, "clk_uart4_src", gpll_cpll_usb480m_p, 58, 8,
	    2, 0, 7, 28, 9, 0),
	RK_FRACTION(CLK_UART4_FRAC, "clk_uart4_frac", "clk_uart4_src", 59, 28,
	    10, 0),
	RK_MUX(0, "sclk_uart4_mux", sclk_uart4_p, 58, 12, 2, 0),
	RK_COMPOSITE(CLK_UART5_SRC, "clk_uart5_src", gpll_cpll_usb480m_p, 60, 8,
	    2, 0, 7, 28, 13, 0),
	RK_FRACTION(CLK_UART5_FRAC, "clk_uart5_frac", "clk_uart5_src", 61, 28,
	    14, 0),
	RK_MUX(0, "sclk_uart5_mux", sclk_uart5_p, 60, 12, 2, 0),
	RK_COMPOSITE(CLK_UART6_SRC, "clk_uart6_src", gpll_cpll_usb480m_p, 62, 8,
	    2, 0, 7, 29, 1, 0),
	RK_FRACTION(CLK_UART6_FRAC, "clk_uart6_frac", "clk_uart6_src", 63, 29,
	    2, 0),
	RK_MUX(0, "sclk_uart6_mux", sclk_uart6_p, 62, 12, 2, 0),
	RK_COMPOSITE(CLK_UART7_SRC, "clk_uart7_src", gpll_cpll_usb480m_p, 64, 8,
	    2, 0, 7, 29, 5, 0),
	RK_FRACTION(CLK_UART7_FRAC, "clk_uart7_frac", "clk_uart7_src", 65, 29,
	    6, 0),
	RK_MUX(0, "sclk_uart7_mux", sclk_uart7_p, 64, 12, 2, 0),
	RK_COMPOSITE(CLK_UART8_SRC, "clk_uart8_src", gpll_cpll_usb480m_p, 66, 8,
	    2, 0, 7, 29, 9, 0),
	RK_FRACTION(CLK_UART8_FRAC, "clk_uart8_frac", "clk_uart8_src", 67, 29,
	    10, 0),
	RK_MUX(0, "sclk_uart8_mux", sclk_uart8_p, 66, 12, 2, 0),
	RK_COMPOSITE(CLK_UART9_SRC, "clk_uart9_src", gpll_cpll_usb480m_p, 68, 8,
	    2, 0, 7, 29, 13, 0),
	RK_FRACTION(CLK_UART9_FRAC, "clk_uart9_frac", "clk_uart9_src", 69, 29,
	    14, 0),
	RK_MUX(0, "sclk_uart9_mux", sclk_uart9_p, 68, 12, 2, 0),
	RK_COMPOSITE(CLK_CAN0, "clk_can0", gpll_cpll_p, 70, 7, 1, 0, 5, 27,
	    6, 0),
	RK_COMPOSITE(CLK_CAN1, "clk_can1", gpll_cpll_p, 70, 15, 1, 8, 5, 27,
	    8, 0),
	RK_COMPOSITE(CLK_CAN2, "clk_can2", gpll_cpll_p, 71, 7, 1, 0, 5, 27,
	    10, 0),
	RK_COMPNODIV(CLK_I2C, "clk_i2c", clk_i2c_p, 71, 8, 2, 32, 10, 0),
	RK_COMPNODIV(CLK_SPI0, "clk_spi0", gpll200_xin24m_cpll100_p, 72, 0, 1,
	    30, 11, 0),
	RK_COMPNODIV(CLK_SPI1, "clk_spi1", gpll200_xin24m_cpll100_p, 72, 2, 1,
	    30, 13, 0),
	RK_COMPNODIV(CLK_SPI2, "clk_spi2", gpll200_xin24m_cpll100_p, 72, 4, 1,
	    30, 15, 0),
	RK_COMPNODIV(CLK_SPI3, "clk_spi3", gpll200_xin24m_cpll100_p, 72, 6, 1,
	    31, 1, 0),
	RK_COMPNODIV(CLK_PWM1, "clk_pwm1", gpll100_xin24m_cpll100_p, 72, 8, 1,
	    31, 11, 0),
	RK_COMPNODIV(CLK_PWM2, "clk_pwm2", gpll100_xin24m_cpll100_p, 72, 10, 1,
	    31, 14, 0),
	RK_COMPNODIV(CLK_PWM3, "clk_pwm3", gpll100_xin24m_cpll100_p, 72, 12, 1,
	    32, 1, 0),
	RK_COMPNODIV(DBCLK_GPIO, "dbclk_gpio", xin24m_32k_p, 72, 14, 1, 32,
	    11, 0),
	RK_COMPNODIV(ACLK_TOP_HIGH, "aclk_top_high",
	    cpll500_gpll400_gpll300_xin24m_p, 73, 0, 2, 33, 0, 0),
	RK_COMPNODIV(ACLK_TOP_LOW, "aclk_top_low",
	    gpll400_gpll300_gpll200_xin24m_p, 73, 4, 2, 33, 1, 0),
	RK_COMPNODIV(HCLK_TOP, "hclk_top", gpll150_gpll100_gpll75_xin24m_p, 73,
	    8, 2, 33, 2, 0),
	RK_COMPNODIV(PCLK_TOP, "pclk_top", gpll100_gpll75_cpll50_xin24m_p, 73,
	    12, 2, 33, 3, 0),
	RK_COMPNODIV(CLK_OPTC_ARB, "clk_optc_arb", xin24m_cpll100_p, 73, 15, 1,
	    33, 9, 0),
};

/* GATES */
static struct rk_cru_gate rk3568_gates[] = {
	RK_GATE(CLK_CORE_PVTM, "clk_core_pvtm", "xin24m", 1, 10),
	RK_GATE(CLK_CORE_PVTM_CORE, "clk_core_pvtm_core", "armclk", 1, 11),
	RK_GATE(CLK_CORE_PVTPLL, "clk_core_pvtpll", "armclk", 1, 12),
	RK_GATE(PCLK_CORE_PVTM, "pclk_core_pvtm", "pclk_core_pre", 1, 9),
	RK_GATE(CLK_GPU, "clk_gpu", "clk_gpu_pre_mux", 2, 3),
	RK_GATE(PCLK_GPU_PVTM, "pclk_gpu_pvtm", "pclk_gpu_pre", 2, 6),
	RK_GATE(CLK_GPU_PVTM, "clk_gpu_pvtm", "xin24m", 2, 7),
	RK_GATE(CLK_GPU_PVTM_CORE, "clk_gpu_pvtm_core", "clk_gpu_src", 2, 8),
	RK_GATE(CLK_GPU_PVTPLL, "clk_gpu_pvtpll", "clk_gpu_src", 2, 9),
	RK_GATE(ACLK_NPU_PRE, "aclk_npu_pre", "clk_npu", 3, 4),
	RK_GATE(ACLK_NPU, "aclk_npu", "aclk_npu_pre", 3, 7),
	RK_GATE(HCLK_NPU, "hclk_npu", "hclk_npu_pre", 3, 8),
	RK_GATE(PCLK_NPU_PVTM, "pclk_npu_pvtm", "pclk_npu_pre", 3, 9),
	RK_GATE(CLK_NPU_PVTM, "clk_npu_pvtm", "xin24m", 3, 10),
	RK_GATE(CLK_NPU_PVTM_CORE, "clk_npu_pvtm_core", "clk_npu_pre_ndft",
	    3, 11),
	RK_GATE(CLK_NPU_PVTPLL, "clk_npu_pvtpll", "clk_npu_pre_ndft", 3, 12),
	RK_GATE(CLK24_DDRMON, "clk24_ddrmon", "xin24m", 4, 15),
	RK_GATE(HCLK_SDMMC_BUFFER, "hclk_sdmmc_buffer", "hclk_gic_audio", 5, 8),
	RK_GATE(ACLK_GIC600, "aclk_gic600", "aclk_gic_audio", 5, 4),
	RK_GATE(ACLK_SPINLOCK, "aclk_spinlock", "aclk_gic_audio", 5, 7),
	RK_GATE(HCLK_I2S0_8CH, "hclk_i2s0_8ch", "hclk_gic_audio", 5, 10),
	RK_GATE(HCLK_I2S1_8CH, "hclk_i2s1_8ch", "hclk_gic_audio", 5, 11),
	RK_GATE(HCLK_I2S2_2CH, "hclk_i2s2_2ch", "hclk_gic_audio", 5, 12),
	RK_GATE(HCLK_I2S3_2CH, "hclk_i2s3_2ch", "hclk_gic_audio", 5, 13),
	RK_GATE(MCLK_I2S0_8CH_TX, "mclk_i2s0_8ch_tx", "clk_i2s0_8ch_tx", 6, 2),
	RK_GATE(MCLK_I2S0_8CH_RX, "mclk_i2s0_8ch_rx", "clk_i2s0_8ch_rx", 6, 6),
	RK_GATE(MCLK_I2S1_8CH_TX, "mclk_i2s1_8ch_tx", "clk_i2s1_8ch_tx", 6, 10),
	RK_GATE(MCLK_I2S1_8CH_RX, "mclk_i2s1_8ch_rx", "clk_i2s1_8ch_rx", 6, 14),
	RK_GATE(MCLK_I2S2_2CH, "mclk_i2s2_2ch", "clk_i2s2_2ch", 7, 2),
	RK_GATE(MCLK_I2S3_2CH_TX, "mclk_i2s3_2ch_tx", "clk_i2s3_2ch_tx", 7, 6),
	RK_GATE(MCLK_I2S3_2CH_RX, "mclk_i2s3_2ch_rx", "clk_i2s3_2ch_rx", 7, 10),
	RK_GATE(HCLK_PDM, "hclk_pdm", "hclk_gic_audio", 5, 14),
	RK_GATE(HCLK_VAD, "hclk_vad", "hclk_gic_audio", 7, 12),
	RK_GATE(HCLK_SPDIF_8CH, "hclk_spdif_8ch", "hclk_gic_audio", 7, 13),
	RK_GATE(HCLK_AUDPWM, "hclk_audpwm", "hclk_gic_audio", 8, 0),
	RK_GATE(HCLK_ACDCDIG, "hclk_acdcdig", "hclk_gic_audio", 8, 3),
	RK_GATE(CLK_ACDCDIG_DAC, "clk_acdcdig_dac", "mclk_i2s3_2ch_tx", 8, 5),
	RK_GATE(CLK_ACDCDIG_ADC, "clk_acdcdig_adc", "mclk_i2s3_2ch_rx", 8, 6),
	RK_GATE(ACLK_CRYPTO_NS, "aclk_crypto_ns", "aclk_secure_flash", 8, 11),
	RK_GATE(HCLK_CRYPTO_NS, "hclk_crypto_ns", "hclk_secure_flash", 8, 12),
	RK_GATE(CLK_CRYPTO_NS_RNG, "clk_crypto_ns_rng", "hclk_secure_flash",
	    8, 15),
	RK_GATE(HCLK_TRNG_NS, "hclk_trng_ns", "hclk_secure_flash", 9, 10),
	RK_GATE(CLK_TRNG_NS, "clk_trng_ns", "hclk_secure_flash", 9, 11),
	RK_GATE(PCLK_OTPC_NS, "pclk_otpc_ns", "hclk_secure_flash", 26, 9),
	RK_GATE(CLK_OTPC_NS_SBPI, "clk_otpc_ns_sbpi", "xin24m", 26, 10),
	RK_GATE(CLK_OTPC_NS_USR, "clk_otpc_ns_usr", "xin_osc0_half", 26, 11),
	RK_GATE(HCLK_NANDC, "hclk_nandc", "hclk_secure_flash", 9, 0),
	RK_GATE(HCLK_SFC, "hclk_sfc", "hclk_secure_flash", 9, 2),
	RK_GATE(HCLK_SFC_XIP, "hclk_sfc_xip", "hclk_secure_flash", 9, 3),
	RK_GATE(ACLK_EMMC, "aclk_emmc", "aclk_secure_flash", 9, 5),
	RK_GATE(HCLK_EMMC, "hclk_emmc", "hclk_secure_flash", 9, 6),
	RK_GATE(TCLK_EMMC, "tclk_emmc", "xin24m", 9, 9),
	RK_GATE(ACLK_PCIE20_MST, "aclk_pcie20_mst", "aclk_pipe", 12, 0),
	RK_GATE(ACLK_PCIE20_SLV, "aclk_pcie20_slv", "aclk_pipe", 12, 1),
	RK_GATE(ACLK_PCIE20_DBI, "aclk_pcie20_dbi", "aclk_pipe", 12, 2),
	RK_GATE(PCLK_PCIE20, "pclk_pcie20", "pclk_pipe", 12, 3),
	RK_GATE(CLK_PCIE20_AUX_NDFT, "clk_pcie20_aux_ndft", "xin24m", 12, 4),
	RK_GATE(ACLK_PCIE30X1_MST, "aclk_pcie30x1_mst", "aclk_pipe", 12, 8),
	RK_GATE(ACLK_PCIE30X1_SLV, "aclk_pcie30x1_slv", "aclk_pipe", 12, 9),
	RK_GATE(ACLK_PCIE30X1_DBI, "aclk_pcie30x1_dbi", "aclk_pipe", 12, 10),
	RK_GATE(PCLK_PCIE30X1, "pclk_pcie30x1", "pclk_pipe", 12, 11),
	RK_GATE(CLK_PCIE30X1_AUX_NDFT, "clk_pcie30x1_aux_ndft", "xin24m",
	    12, 12),
	RK_GATE(ACLK_PCIE30X2_MST, "aclk_pcie30x2_mst", "aclk_pipe", 13, 0),
	RK_GATE(ACLK_PCIE30X2_SLV, "aclk_pcie30x2_slv", "aclk_pipe", 13, 1),
	RK_GATE(ACLK_PCIE30X2_DBI, "aclk_pcie30x2_dbi", "aclk_pipe", 13, 2),
	RK_GATE(PCLK_PCIE30X2, "pclk_pcie30x2", "pclk_pipe", 13, 3),
	RK_GATE(CLK_PCIE30X2_AUX_NDFT, "clk_pcie30x2_aux_ndft", "xin24m",
	    13, 4),
	RK_GATE(ACLK_SATA0, "aclk_sata0", "aclk_pipe", 11, 0),
	RK_GATE(CLK_SATA0_PMALIVE, "clk_sata0_pmalive", "gpll_20m", 11, 1),
	RK_GATE(CLK_SATA0_RXOOB, "clk_sata0_rxoob", "cpll_50m", 11, 2),
	RK_GATE(ACLK_SATA1, "aclk_sata1", "aclk_pipe", 11, 4),
	RK_GATE(CLK_SATA1_PMALIVE, "clk_sata1_pmalive", "gpll_20m", 11, 5),
	RK_GATE(CLK_SATA1_RXOOB, "clk_sata1_rxoob", "cpll_50m", 11, 6),
	RK_GATE(ACLK_SATA2, "aclk_sata2", "aclk_pipe", 11, 8),
	RK_GATE(CLK_SATA2_PMALIVE, "clk_sata2_pmalive", "gpll_20m", 11, 9),
	RK_GATE(CLK_SATA2_RXOOB, "clk_sata2_rxoob", "cpll_50m", 11, 10),
	RK_GATE(ACLK_USB3OTG0, "aclk_usb3otg0", "aclk_pipe", 10, 8),
	RK_GATE(CLK_USB3OTG0_REF, "clk_usb3otg0_ref", "xin24m", 10, 9),
	RK_GATE(ACLK_USB3OTG1, "aclk_usb3otg1", "aclk_pipe", 10, 12),
	RK_GATE(CLK_USB3OTG1_REF, "clk_usb3otg1_ref", "xin24m", 10, 13),
	RK_GATE(PCLK_XPCS, "pclk_xpcs", "pclk_pipe", 13, 6),
	RK_GATE(HCLK_SDMMC0, "hclk_sdmmc0", "hclk_php", 15, 0),
	RK_GATE(HCLK_SDMMC1, "hclk_sdmmc1", "hclk_php", 15, 2),
	RK_GATE(ACLK_GMAC0, "aclk_gmac0", "aclk_php", 15, 5),
	RK_GATE(PCLK_GMAC0, "pclk_gmac0", "pclk_php", 15, 6),
	RK_GATE(CLK_MAC0_REFOUT, "clk_mac0_refout", "clk_mac0_2top", 15, 12),
	RK_GATE(HCLK_USB2HOST0, "hclk_usb2host0", "hclk_usb", 16, 12),
	RK_GATE(HCLK_USB2HOST0_ARB, "hclk_usb2host0_arb", "hclk_usb", 16, 13),
	RK_GATE(HCLK_USB2HOST1, "hclk_usb2host1", "hclk_usb", 16, 14),
	RK_GATE(HCLK_USB2HOST1_ARB, "hclk_usb2host1_arb", "hclk_usb", 16, 15),
	RK_GATE(HCLK_SDMMC2, "hclk_sdmmc2", "hclk_usb", 17, 0),
	RK_GATE(ACLK_GMAC1, "aclk_gmac1", "aclk_usb", 17, 3),
	RK_GATE(PCLK_GMAC1, "pclk_gmac1", "pclk_usb", 17, 4),
	RK_GATE(CLK_MAC1_REFOUT, "clk_mac1_refout", "clk_mac1_2top", 17, 10),
	RK_GATE(ACLK_VICAP, "aclk_vicap", "aclk_vi", 18, 9),
	RK_GATE(HCLK_VICAP, "hclk_vicap", "hclk_vi", 18, 10),
	RK_GATE(ACLK_ISP, "aclk_isp", "aclk_vi", 19, 0),
	RK_GATE(HCLK_ISP, "hclk_isp", "hclk_vi", 19, 1),
	RK_GATE(PCLK_CSI2HOST1, "pclk_csi2host1", "pclk_vi", 19, 4),
	RK_GATE(ACLK_VOP, "aclk_vop", "aclk_vop_pre", 20, 8),
	RK_GATE(HCLK_VOP, "hclk_vop", "hclk_vo", 20, 9),
	RK_GATE(CLK_VOP_PWM, "clk_vop_pwm", "xin24m", 20, 13),
	RK_GATE(ACLK_HDCP, "aclk_hdcp", "aclk_vo", 21, 0),
	RK_GATE(HCLK_HDCP, "hclk_hdcp", "hclk_vo", 21, 1),
	RK_GATE(PCLK_HDCP, "pclk_hdcp", "pclk_vo", 21, 2),
	RK_GATE(PCLK_HDMI_HOST, "pclk_hdmi_host", "pclk_vo", 21, 3),
	RK_GATE(CLK_HDMI_SFR, "clk_hdmi_sfr", "xin24m", 21, 4),
	RK_GATE(CLK_HDMI_CEC, "clk_hdmi_cec", "clk_rtc_32k", 21, 5),
	RK_GATE(PCLK_DSITX_0, "pclk_dsitx_0", "pclk_vo", 21, 6),
	RK_GATE(PCLK_DSITX_1, "pclk_dsitx_1", "pclk_vo", 21, 7),
	RK_GATE(PCLK_EDP_CTRL, "pclk_edp_ctrl", "pclk_vo", 21, 8),
	RK_GATE(ACLK_VPU, "aclk_vpu", "aclk_vpu_pre", 22, 4),
	RK_GATE(HCLK_VPU, "hclk_vpu", "hclk_vpu_pre", 22, 5),
	RK_GATE(ACLK_RGA, "aclk_rga", "aclk_rga_pre", 23, 4),
	RK_GATE(HCLK_RGA, "hclk_rga", "hclk_rga_pre", 23, 5),
	RK_GATE(ACLK_IEP, "aclk_iep", "aclk_rga_pre", 23, 7),
	RK_GATE(HCLK_IEP, "hclk_iep", "hclk_rga_pre", 23, 8),
	RK_GATE(HCLK_EBC, "hclk_ebc", "hclk_rga_pre", 23, 10),
	RK_GATE(ACLK_JDEC, "aclk_jdec", "aclk_rga_pre", 23, 12),
	RK_GATE(HCLK_JDEC, "hclk_jdec", "hclk_rga_pre", 23, 13),
	RK_GATE(ACLK_JENC, "aclk_jenc", "aclk_rga_pre", 23, 14),
	RK_GATE(HCLK_JENC, "hclk_jenc", "hclk_rga_pre", 23, 15),
	RK_GATE(PCLK_EINK, "pclk_eink", "pclk_rga_pre", 22, 14),
	RK_GATE(HCLK_EINK, "hclk_eink", "hclk_rga_pre", 22, 15),
	RK_GATE(ACLK_RKVENC, "aclk_rkvenc", "aclk_rkvenc_pre", 24, 6),
	RK_GATE(HCLK_RKVENC, "hclk_rkvenc", "hclk_rkvenc_pre", 24, 7),
	RK_GATE(ACLK_RKVDEC, "aclk_rkvdec", "aclk_rkvdec_pre", 25, 4),
	RK_GATE(HCLK_RKVDEC, "hclk_rkvdec", "hclk_rkvdec_pre", 25, 5),
	RK_GATE(PCLK_TSADC, "pclk_tsadc", "pclk_bus", 26, 4),
	RK_GATE(PCLK_SARADC, "pclk_saradc", "pclk_bus", 26, 7),
	RK_GATE(CLK_SARADC, "clk_saradc", "xin24m", 26, 8),
	RK_GATE(PCLK_SCR, "pclk_scr", "pclk_bus", 26, 12),
	RK_GATE(PCLK_WDT_NS, "pclk_wdt_ns", "pclk_bus", 26, 13),
	RK_GATE(TCLK_WDT_NS, "tclk_wdt_ns", "xin24m", 26, 14),
	RK_GATE(ACLK_MCU, "aclk_mcu", "aclk_bus", 32, 13),
	RK_GATE(PCLK_INTMUX, "pclk_intmux", "pclk_bus", 32, 14),
	RK_GATE(PCLK_MAILBOX, "pclk_mailbox", "pclk_bus", 32, 15),
	RK_GATE(PCLK_UART1, "pclk_uart1", "pclk_bus", 27, 12),
	RK_GATE(SCLK_UART1, "sclk_uart1", "sclk_uart1_mux", 27, 15),
	RK_GATE(PCLK_UART2, "pclk_uart2", "pclk_bus", 28, 0),
	RK_GATE(SCLK_UART2, "sclk_uart2", "sclk_uart2_mux", 28, 3),
	RK_GATE(PCLK_UART3, "pclk_uart3", "pclk_bus", 28, 4),
	RK_GATE(SCLK_UART3, "sclk_uart3", "sclk_uart3_mux", 28, 7),
	RK_GATE(PCLK_UART4, "pclk_uart4", "pclk_bus", 28, 8),
	RK_GATE(SCLK_UART4, "sclk_uart4", "sclk_uart4_mux", 28, 11),
	RK_GATE(PCLK_UART5, "pclk_uart5", "pclk_bus", 28, 12),
	RK_GATE(SCLK_UART5, "sclk_uart5", "sclk_uart5_mux", 28, 15),
	RK_GATE(PCLK_UART6, "pclk_uart6", "pclk_bus", 29, 0),
	RK_GATE(SCLK_UART6, "sclk_uart6", "sclk_uart6_mux", 29, 3),
	RK_GATE(PCLK_UART7, "pclk_uart7", "pclk_bus", 29, 4),
	RK_GATE(SCLK_UART7, "sclk_uart7", "sclk_uart7_mux", 29, 7),
	RK_GATE(PCLK_UART8, "pclk_uart8", "pclk_bus", 29, 8),
	RK_GATE(SCLK_UART8, "sclk_uart8", "sclk_uart8_mux", 29, 11),
	RK_GATE(PCLK_UART9, "pclk_uart9", "pclk_bus", 29, 12),
	RK_GATE(SCLK_UART9, "sclk_uart9", "sclk_uart9_mux", 29, 15),
	RK_GATE(PCLK_CAN0, "pclk_can0", "pclk_bus", 27, 5),
	RK_GATE(PCLK_CAN1, "pclk_can1", "pclk_bus", 27, 7),
	RK_GATE(PCLK_CAN2, "pclk_can2", "pclk_bus", 27, 9),
	RK_GATE(PCLK_I2C1, "pclk_i2c1", "pclk_bus", 30, 0),
	RK_GATE(CLK_I2C1, "clk_i2c1", "clk_i2c", 30, 1),
	RK_GATE(PCLK_I2C2, "pclk_i2c2", "pclk_bus", 30, 2),
	RK_GATE(CLK_I2C2, "clk_i2c2", "clk_i2c", 30, 3),
	RK_GATE(PCLK_I2C3, "pclk_i2c3", "pclk_bus", 30, 4),
	RK_GATE(CLK_I2C3, "clk_i2c3", "clk_i2c", 30, 5),
	RK_GATE(PCLK_I2C4, "pclk_i2c4", "pclk_bus", 30, 6),
	RK_GATE(CLK_I2C4, "clk_i2c4", "clk_i2c", 30, 7),
	RK_GATE(PCLK_I2C5, "pclk_i2c5", "pclk_bus", 30, 8),
	RK_GATE(CLK_I2C5, "clk_i2c5", "clk_i2c", 30, 9),
	RK_GATE(PCLK_SPI0, "pclk_spi0", "pclk_bus", 30, 10),
	RK_GATE(PCLK_SPI1, "pclk_spi1", "pclk_bus", 30, 12),
	RK_GATE(PCLK_SPI2, "pclk_spi2", "pclk_bus", 30, 14),
	RK_GATE(PCLK_SPI3, "pclk_spi3", "pclk_bus", 31, 0),
	RK_GATE(PCLK_PWM1, "pclk_pwm1", "pclk_bus", 31, 10),
	RK_GATE(CLK_PWM1_CAPTURE, "clk_pwm1_capture", "xin24m", 31, 12),
	RK_GATE(PCLK_PWM2, "pclk_pwm2", "pclk_bus", 31, 13),
	RK_GATE(CLK_PWM2_CAPTURE, "clk_pwm2_capture", "xin24m", 31, 15),
	RK_GATE(PCLK_PWM3, "pclk_pwm3", "pclk_bus", 32, 0),
	RK_GATE(CLK_PWM3_CAPTURE, "clk_pwm3_capture", "xin24m", 32, 2),
	RK_GATE(PCLK_GPIO1, "pclk_gpio1", "pclk_bus", 31, 2),
	RK_GATE(DBCLK_GPIO1, "dbclk_gpio1", "dbclk_gpio", 31, 3),
	RK_GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_bus", 31, 4),
	RK_GATE(DBCLK_GPIO2, "dbclk_gpio2", "dbclk_gpio", 31, 5),
	RK_GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_bus", 31, 6),
	RK_GATE(DBCLK_GPIO3, "dbclk_gpio3", "dbclk_gpio", 31, 7),
	RK_GATE(PCLK_GPIO4, "pclk_gpio4", "pclk_bus", 31, 8),
	RK_GATE(DBCLK_GPIO4, "dbclk_gpio4", "dbclk_gpio", 31, 9),
	RK_GATE(PCLK_TIMER, "pclk_timer", "pclk_bus", 32, 3),
	RK_GATE(CLK_TIMER0, "clk_timer0", "xin24m", 32, 4),
	RK_GATE(CLK_TIMER1, "clk_timer1", "xin24m", 32, 5),
	RK_GATE(CLK_TIMER2, "clk_timer2", "xin24m", 32, 6),
	RK_GATE(CLK_TIMER3, "clk_timer3", "xin24m", 32, 7),
	RK_GATE(CLK_TIMER4, "clk_timer4", "xin24m", 32, 8),
	RK_GATE(CLK_TIMER5, "clk_timer5", "xin24m", 32, 9),
	RK_GATE(PCLK_PCIE30PHY, "pclk_pcie30phy", "pclk_top", 33, 8),
	RK_GATE(PCLK_MIPICSIPHY, "pclk_mipicsiphy", "pclk_top", 33, 13),
	RK_GATE(PCLK_MIPIDSIPHY0, "pclk_mipidsiphy0", "pclk_top", 33, 14),
	RK_GATE(PCLK_MIPIDSIPHY1, "pclk_mipidsiphy1", "pclk_top", 33, 15),
	RK_GATE(PCLK_PIPEPHY0, "pclk_pipephy0", "pclk_top", 34, 4),
	RK_GATE(PCLK_PIPEPHY1, "pclk_pipephy1", "pclk_top", 34, 5),
	RK_GATE(PCLK_PIPEPHY2, "pclk_pipephy2", "pclk_top", 34, 6),
	RK_GATE(PCLK_CPU_BOOST, "pclk_cpu_boost", "pclk_top", 34, 11),
	RK_GATE(CLK_CPU_BOOST, "clk_cpu_boost", "xin24m", 34, 12),
	RK_GATE(PCLK_OTPPHY, "pclk_otpphy", "pclk_top", 34, 13),
	RK_GATE(PCLK_EDPPHY_GRF, "pclk_edpphy_grf", "pclk_top", 34, 14),
};


static int
rk3568_cru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3568-cru")) {
		device_set_desc(dev, "Rockchip RK3568 Clock & Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
rk3568_cru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->clks = rk3568_clks;
	sc->nclks = nitems(rk3568_clks);
	sc->gates = rk3568_gates;
	sc->ngates = nitems(rk3568_gates);
	sc->reset_offset = 0x400;
	sc->reset_num = 478;

	return (rk_cru_attach(dev));
}

static device_method_t methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3568_cru_probe),
	DEVMETHOD(device_attach,	rk3568_cru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3568_cru, rk3568_cru_driver, methods,
    sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3568_cru, simplebus, rk3568_cru_driver,
    0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
