/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021, 2022 Soren Schmidt <sos@deepcore.dk>
 * Copyright (c) 2023, Emmanuel Vadot <manu@freebsd.org>
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

#include <dev/clk/clk_div.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_mux.h>

#include <dev/clk/rockchip/rk_cru.h>
#include <contrib/device-tree/include/dt-bindings/clock/rk3568-cru.h>


#define	RK3568_PLLSEL_CON(x)		((x) * 0x20)
#define	CRU_CLKSEL_CON(x)		((x) * 0x4 + 0x100)
#define	CRU_CLKGATE_CON(x)		((x) * 0x4 + 0x300)
#define	RK3568_SOFTRST_CON(x)		((x) * 0x4 + 0x400)

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
PLIST(mux_pll_p) = { "xin24m" };
PLIST(mux_usb480m_p) = { "xin24m", "usb480m_phy", "clk_rtc_32k" };
PLIST(mux_armclk_p) = { "apll", "gpll" };
PLIST(clk_i2s0_8ch_tx_p) = { "clk_i2s0_8ch_tx_src", "clk_i2s0_8ch_tx_frac",
    "i2s0_mclkin", "xin_osc0_half" };
PLIST(clk_i2s0_8ch_rx_p) = { "clk_i2s0_8ch_rx_src", "clk_i2s0_8ch_rx_frac",
    "i2s0_mclkin", "xin_osc0_half" };
PLIST(clk_i2s1_8ch_tx_p) = { "clk_i2s1_8ch_tx_src", "clk_i2s1_8ch_tx_frac",
    "i2s1_mclkin", "xin_osc0_half" };
PLIST(clk_i2s1_8ch_rx_p) = { "clk_i2s1_8ch_rx_src", "clk_i2s1_8ch_rx_frac",
    "i2s1_mclkin", "xin_osc0_half" };
PLIST(clk_i2s2_2ch_p) = { "clk_i2s2_2ch_src", "clk_i2s2_2ch_frac",
    "i2s2_mclkin", "xin_osc0_half"};
PLIST(clk_i2s3_2ch_tx_p) = { "clk_i2s3_2ch_tx_src", "clk_i2s3_2ch_tx_frac",
    "i2s3_mclkin", "xin_osc0_half" };
PLIST(clk_i2s3_2ch_rx_p) = { "clk_i2s3_2ch_rx_src", "clk_i2s3_2ch_rx_frac",
    "i2s3_mclkin", "xin_osc0_half" };
PLIST(mclk_spdif_8ch_p) = { "mclk_spdif_8ch_src", "mclk_spdif_8ch_frac" };
PLIST(sclk_audpwm_p) = { "sclk_audpwm_src", "sclk_audpwm_frac" };
PLIST(sclk_uart1_p) = { "clk_uart1_src", "clk_uart1_frac", "xin24m" };
PLIST(sclk_uart2_p) = { "clk_uart2_src", "clk_uart2_frac", "xin24m" };
PLIST(sclk_uart3_p) = { "clk_uart3_src", "clk_uart3_frac", "xin24m" };
PLIST(sclk_uart4_p) = { "clk_uart4_src", "clk_uart4_frac", "xin24m" };
PLIST(sclk_uart5_p) = { "clk_uart5_src", "clk_uart5_frac", "xin24m" };
PLIST(sclk_uart6_p) = { "clk_uart6_src", "clk_uart6_frac", "xin24m" };
PLIST(sclk_uart7_p) = { "clk_uart7_src", "clk_uart7_frac", "xin24m" };
PLIST(sclk_uart8_p) = { "clk_uart8_src", "clk_uart8_frac", "xin24m" };
PLIST(sclk_uart9_p) = { "clk_uart9_src", "clk_uart9_frac", "xin24m" };
PLIST(mpll_gpll_cpll_npll_p) = { "mpll", "gpll", "cpll", "npll" };
PLIST(gpll_cpll_npll_p) = { "gpll", "cpll", "npll" };
PLIST(npll_gpll_p) = { "npll", "gpll" };
PLIST(cpll_gpll_p) = { "cpll", "gpll" };
PLIST(gpll_cpll_p) = { "gpll", "cpll" };
PLIST(gpll_cpll_npll_vpll_p) = { "gpll", "cpll", "npll", "vpll" };
PLIST(apll_gpll_npll_p) = { "apll", "gpll", "npll" };
PLIST(sclk_core_pre_p) = { "sclk_core_src", "npll" };
PLIST(gpll150_gpll100_gpll75_xin24m_p) = { "clk_gpll_div_150m", "clk_gpll_div_100m", "clk_gpll_div_75m",
    "xin24m" };
PLIST(clk_gpu_pre_mux_p) = { "clk_gpu_src", "gpu_pvtpll_out" };
PLIST(clk_npu_pre_ndft_p) = { "clk_npu_src", "clk_npu_np5"};
PLIST(clk_npu_p) = { "clk_npu_pre_ndft", "npu_pvtpll_out" };
PLIST(dpll_gpll_cpll_p) = { "dpll", "gpll", "cpll" };
PLIST(clk_ddr1x_p) = { "clk_ddrphy1x_src", "dpll" };
PLIST(gpll200_gpll150_gpll100_xin24m_p) = { "clk_gpll_div_200m", "clk_gpll_div_150m",
    "clk_gpll_div_100m", "xin24m" };
PLIST(gpll100_gpll75_gpll50_p) = { "clk_gpll_div_100m", "clk_gpll_div_75m", "clk_cpll_div_50m" };
PLIST(i2s0_mclkout_tx_p) = { "clk_i2s0_8ch_tx", "xin_osc0_half" };
PLIST(i2s0_mclkout_rx_p) = { "clk_i2s0_8ch_rx", "xin_osc0_half" };
PLIST(i2s1_mclkout_tx_p) = { "clk_i2s1_8ch_tx", "xin_osc0_half" };
PLIST(i2s1_mclkout_rx_p) = { "clk_i2s1_8ch_rx", "xin_osc0_half" };
PLIST(i2s2_mclkout_p) = { "clk_i2s2_2ch", "xin_osc0_half" };
PLIST(i2s3_mclkout_tx_p) = { "clk_i2s3_2ch_tx", "xin_osc0_half" };
PLIST(i2s3_mclkout_rx_p) = { "clk_i2s3_2ch_rx", "xin_osc0_half" };
PLIST(mclk_pdm_p) = { "clk_gpll_div_300m", "clk_cpll_div_250m", "clk_gpll_div_200m", "clk_gpll_div_100m" };
PLIST(clk_i2c_p) = { "clk_gpll_div_200m", "clk_gpll_div_100m", "xin24m", "clk_cpll_div_100m" };
PLIST(gpll200_gpll150_gpll100_p) = { "clk_gpll_div_200m", "clk_gpll_div_150m", "clk_gpll_div_100m" };
PLIST(gpll300_gpll200_gpll100_p) = { "clk_gpll_div_300m", "clk_gpll_div_200m", "clk_gpll_div_100m" };
PLIST(clk_nandc_p) = { "clk_gpll_div_200m", "clk_gpll_div_150m", "clk_cpll_div_100m", "xin24m" };
PLIST(sclk_sfc_p) = { "xin24m", "clk_cpll_div_50m", "clk_gpll_div_75m", "clk_gpll_div_100m",
    "clk_cpll_div_125m", "clk_gpll_div_150m" };
PLIST(gpll200_gpll150_cpll125_p) = { "clk_gpll_div_200m", "clk_gpll_div_150m", "clk_cpll_div_125m" };
PLIST(cclk_emmc_p) = { "xin24m", "clk_gpll_div_200m", "clk_gpll_div_150m", "clk_cpll_div_100m",
    "clk_cpll_div_50m", "clk_osc0_div_375k" };
PLIST(aclk_pipe_p) = { "clk_gpll_div_400m", "clk_gpll_div_300m", "clk_gpll_div_200m", "xin24m" };
PLIST(gpll200_cpll125_p) = { "clk_gpll_div_200m", "clk_cpll_div_125m" };
PLIST(gpll300_gpll200_gpll100_xin24m_p) = { "clk_gpll_div_300m", "clk_gpll_div_200m",
    "clk_gpll_div_100m", "xin24m" };
PLIST(clk_sdmmc_p) = { "xin24m", "clk_gpll_div_400m", "clk_gpll_div_300m", "clk_cpll_div_100m",
    "clk_cpll_div_50m", "clk_osc0_div_750k" };
PLIST(cpll125_cpll50_cpll25_xin24m_p) = { "clk_cpll_div_125m", "clk_cpll_div_50m", "clk_cpll_div_25m",
    "xin24m" };
PLIST(clk_gmac_ptp_p) = { "clk_cpll_div_62P5m", "clk_gpll_div_100m", "clk_cpll_div_50m", "xin24m" };
PLIST(cpll333_gpll300_gpll200_p) = { "clk_cpll_div_333m", "clk_gpll_div_300m", "clk_gpll_div_200m" };
PLIST(cpll_gpll_hpll_p) = { "cpll", "gpll", "hpll" };
PLIST(gpll_usb480m_xin24m_p) = { "gpll", "usb480m", "xin24m", "xin24m" };
PLIST(gpll300_cpll250_gpll100_xin24m_p) = { "clk_gpll_div_300m", "clk_cpll_div_250m",
    "clk_gpll_div_100m", "xin24m" };
PLIST(cpll_gpll_hpll_vpll_p) = { "cpll", "gpll", "hpll", "vpll" };
PLIST(hpll_vpll_gpll_cpll_p) = { "hpll", "vpll", "gpll", "cpll" };
PLIST(gpll400_cpll333_gpll200_p) = { "clk_gpll_div_400m", "clk_cpll_div_333m", "clk_gpll_div_200m" };
PLIST(gpll100_gpll75_cpll50_xin24m_p) = { "clk_gpll_div_100m", "clk_gpll_div_75m", "clk_cpll_div_50m",
    "xin24m" };
PLIST(xin24m_gpll100_cpll100_p) = { "xin24m", "clk_gpll_div_100m", "clk_cpll_div_100m" };
PLIST(gpll_cpll_usb480m_p) = { "gpll", "cpll", "usb480m" };
PLIST(gpll100_xin24m_cpll100_p) = { "clk_gpll_div_100m", "xin24m", "clk_cpll_div_100m" };
PLIST(gpll200_xin24m_cpll100_p) = { "clk_gpll_div_200m", "xin24m", "clk_cpll_div_100m" };
PLIST(xin24m_32k_p) = { "xin24m", "clk_rtc_32k" };
PLIST(cpll500_gpll400_gpll300_xin24m_p) = { "clk_cpll_div_500m", "clk_gpll_div_400m",
    "clk_gpll_div_300m", "xin24m" };
PLIST(gpll400_gpll300_gpll200_xin24m_p) = { "clk_gpll_div_400m", "clk_gpll_div_300m",
    "clk_gpll_div_200m", "xin24m" };
PLIST(xin24m_cpll100_p) = { "xin24m", "clk_cpll_div_100m" };
PLIST(mux_gmac0_p) = { "clk_mac0_2top", "gmac0_clkin" };
PLIST(mux_gmac0_rgmii_speed_p) = { "clk_gmac0", "clk_gmac0",
    "clk_gmac0_tx_div50", "clk_gmac0_tx_div5" };
PLIST(mux_gmac0_rmii_speed_p) = { "clk_gmac0_rx_div20", "clk_gmac0_rx_div2" };
PLIST(mux_gmac0_rx_tx_p) = { "clk_gmac0_rgmii_speed", "clk_gmac0_rmii_speed",
    "clk_gmac0_xpcs_mii" };
PLIST(mux_gmac1_p) = { "clk_mac1_2top", "gmac1_clkin" };
PLIST(mux_gmac1_rgmii_speed_p) = { "clk_gmac1", "clk_gmac1",
    "clk_gmac1_tx_div50", "clk_gmac1_tx_div5" };
PLIST(mux_gmac1_rmii_speed_p) = { "clk_gmac1_rx_div20", "clk_gmac1_rx_div2" };
PLIST(mux_gmac1_rx_tx_p) = { "clk_gmac1_rgmii_speed", "clk_gmac1_rmii_speed",
    "clk_gmac1_xpcs_mii" };
PLIST(clk_mac_2top_p) = { "clk_cpll_div_125m", "clk_cpll_div_50m", "clk_cpll_div_25m", "ppll" };
PLIST(aclk_rkvdec_pre_p) = { "gpll", "cpll" };
PLIST(clk_rkvdec_core_p) = { "gpll", "cpll", "npll", "vpll" };

/* CLOCKS */
static struct rk_clk rk3568_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	LINK("clk_rtc_32k"),
	LINK("usb480m_phy"),
	LINK("mpll"),	/* It lives in SCRU */
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
	ARMDIV(ARMCLK, "armclk", mux_armclk_p, rk3568_armclk_rates, 0, 0, 5,
	    6, 1, 0, 1),
	FFACT(0, "clk_osc0_div_375k", "clk_osc0_div_750k", 1, 2),
	FFACT(0, "xin_osc0_half", "xin24m", 1, 2),
	MUX(USB480M, "usb480m", mux_usb480m_p, 0, -16, 14, 2),

	/* Clocks */

	/* CRU_CLKSEL_CON00 */
	/* 0:4 clk_core0_div DIV */
	/* 5 Reserved */
	/* 6 clk_core_i_sel MUX */
	/* 7 clk_core_ndft_sel MUX */
	/* 8:12 clk_core1_div DIV */
	/* 13:14 Reserved */
	/* 15 clk_core_ndft_mux_sel MUX */

	/* CRU_CLKSEL_CON01 */
	/* 0:4 clk_core2_div DIV */
	/* 5:7 Reserved */
	/* 8:12 clk_core3_div DIV */
	/* 13:15 Reserved */

	/* CRU_CLKSEL_CON02 */
	COMP(0, "sclk_core_src_c", apll_gpll_npll_p, 0, 2, 0, 4, 8, 2),
	/* 4:7 Reserved */
	/* 10:14 Reserved */
	MUX(0, "sclk_core_pre_sel", sclk_core_pre_p, 0, 2, 15, 1),

	/* CRU_CLKSEL_CON03 */
	CDIV(0, "atclk_core_div", "armclk", 0, 3, 0, 5),
	/* 5:7 Reserved */
	CDIV(0, "gicclk_core_div", "armclk", 0, 3, 8, 5),
	/* 13:15 Reserved */

	/* CRU_CLKSEL_CON04 */
	CDIV(0, "pclk_core_pre_div", "armclk", 0, 4, 0, 5),
	/* 5:7 Reserved */
	CDIV(0, "periphclk_core_pre_div", "armclk", 0, 4, 8, 5),
	/* 13:15 Reserved */

	/* CRU_CLKSEL_CON05 */
	/* 0:7 Reserved */
	/* 8:12 aclk_core_ndft_div DIV */
	/* 13 Reserved */
	/* 14:15 aclk_core_biu2bus_sel MUX */

	/* CRU_CLKSEL_CON06 */
	COMP(0, "clk_gpu_pre_c", mpll_gpll_cpll_npll_p, 0, 6, 0, 4, 6, 2),
	/* 4:5 Reserved */
	CDIV(0, "aclk_gpu_pre_div", "clk_gpu_pre_c", 0, 6, 8, 2),
	/* 10 Reserved */
	MUX(CLK_GPU_PRE_MUX, "clk_gpu_pre_mux_sel", clk_gpu_pre_mux_p, 0, 6, 11, 1),
	CDIV(0, "pclk_gpu_pre_div", "clk_gpu_pre_c", 0, 6, 12, 4),

	/* CRU_CLKSEL_CON07 */
	COMP(0, "clk_npu_src_c", npll_gpll_p, 0, 7, 0, 4, 6, 1),
	COMP(0, "clk_npu_np5_c", npll_gpll_p, 0, 7, 4, 2, 7, 1),
	MUX(CLK_NPU_PRE_NDFT, "clk_npu_pre_ndft", clk_npu_pre_ndft_p, 0, 7,
	    8, 1),
	/* 9:14 Reserved */
	MUX(CLK_NPU, "clk_npu", clk_npu_p, 0, 7, 15, 1),

	/* CRU_CLKSEL_CON08 */
	CDIV(0, "hclk_npu_pre_div", "clk_npu", 0, 8, 0, 4),
	CDIV(0, "pclk_npu_pre_div", "clk_npu", 0, 8, 4, 4),
	/* 8:15 Reserved */

	/* CRU_CLKSEL_CON09 */
	COMP(0, "clk_ddrphy1x_src_c", dpll_gpll_cpll_p, 0, 9, 0, 5, 6, 2),
	/* 5 Reserved */
	/* 8:14 Reserved */
	MUX(CLK_DDR1X, "clk_ddr1x", clk_ddr1x_p, RK_CLK_COMPOSITE_GRF, 9,
	    15, 1),

	/*  CRU_CLKSEL_CON10 */
	CDIV(0, "clk_msch_div", "clk_ddr1x", 0, 10, 0, 2),
	MUX(0, "aclk_perimid_sel", gpll300_gpll200_gpll100_xin24m_p, 0, 10, 4, 2),
	MUX(0, "hclk_perimid_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 10, 6, 2),
	MUX(0, "aclk_gic_audio_sel", gpll200_gpll150_gpll100_xin24m_p, 0, 10, 8, 2),
	MUX(0, "hclk_gic_audio_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 10, 10, 2),
	MUX(0, "dclk_sdmmc_buffer_sel", gpll100_gpll75_gpll50_p, 0, 10, 12, 2),
	/* 14:15 Reserved */

	/*  CRU_CLKSEL_CON11 */
	COMP(0, "clk_i2s0_8ch_tx_src_c", gpll_cpll_npll_p, 0, 11, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S0_8CH_TX, "clk_i2s0_8ch_tx", clk_i2s0_8ch_tx_p, 0, 11, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s0_mclkout_tx_sel", i2s0_mclkout_tx_p, 0, 11, 15, 1),

	/*  CRU_CLKSEL_CON12 */
	FRACT(0, "clk_i2s0_8ch_tx_frac_div", "clk_i2s0_8ch_tx_src", 0, 12),

	/*  CRU_CLKSEL_CON13 */
	COMP(0, "clk_i2s0_8ch_rx_src_c", gpll_cpll_npll_p, 0, 13, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S0_8CH_RX, "clk_i2s0_8ch_rx", clk_i2s0_8ch_rx_p, 0, 13, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s0_mclkout_rx_sel", i2s0_mclkout_rx_p, 0, 13, 15, 1),

	/*  CRU_CLKSEL_CON14 */
	FRACT(0, "clk_i2s0_8ch_rx_frac_div", "clk_i2s0_8ch_rx_src", 0, 14),

	/*  CRU_CLKSEL_CON15 */
	COMP(0, "clk_i2s1_8ch_tx_src_c", gpll_cpll_npll_p, 0, 15, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S1_8CH_TX, "clk_i2s1_8ch_tx", clk_i2s1_8ch_tx_p, 0, 15, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s1_mclkout_tx_sel", i2s1_mclkout_tx_p, 0, 11, 15, 1),

	/*  CRU_CLKSEL_CON16 */
	FRACT(0, "clk_i2s1_8ch_tx_frac_div", "clk_i2s1_8ch_tx_src", 0, 16),

	/*  CRU_CLKSEL_CON17 */
	COMP(0, "clk_i2s1_8ch_rx_src_c", gpll_cpll_npll_p, 0, 17, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S1_8CH_RX, "clk_i2s1_8ch_rx", clk_i2s1_8ch_rx_p, 0, 17, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s1_mclkout_rx_sel", i2s1_mclkout_rx_p, 0, 17, 15, 1),

	/*  CRU_CLKSEL_CON18 */
	FRACT(0, "clk_i2s1_8ch_rx_frac_div", "clk_i2s1_8ch_rx_src", 0, 18),

	/*  CRU_CLKSEL_CON19 */
	COMP(0, "clk_i2s2_2ch_src_c", gpll_cpll_npll_p, 0, 19, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S2_2CH, "clk_i2s2_2ch", clk_i2s2_2ch_p, 0, 19, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s2_mclkout_sel", i2s2_mclkout_p, 0, 19, 15, 1),

	/*  CRU_CLKSEL_CON20 */
	FRACT(0, "clk_i2s2_2ch_frac_div", "clk_i2s2_2ch_src", 0, 20),

	/*  CRU_CLKSEL_CON21 */
	COMP(0, "clk_i2s3_2ch_tx_src_c", gpll_cpll_npll_p, 0, 21, 0, 7, 8, 2),
	/* 7 Reserved */
	MUX(CLK_I2S3_2CH_TX, "clk_i2s3_2ch_tx", clk_i2s3_2ch_tx_p, 0, 21, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s3_mclkout_tx_sel", i2s3_mclkout_tx_p, 0, 21, 15, 1),

	/*  CRU_CLKSEL_CON22 */
	FRACT(0, "clk_i2s3_2ch_tx_frac_div", "clk_i2s3_2ch_tx_src", 0, 22),

	/*  CRU_CLKSEL_CON23 */
	COMP(0, "mclk_spdif_8ch_src_c", cpll_gpll_p, 0, 23, 0, 7, 14, 1),
	/* 7 Reserved */
	MUX(0, "mclk_pdm_sel", mclk_pdm_p, 0, 23, 8, 2),
	MUX(0, "clk_acdcdig_i2c_sel", clk_i2c_p, 0, 23, 10, 2),
	/* 12:13 Reserved */
	MUX(MCLK_SPDIF_8CH, "mclk_spdif_8ch", mclk_spdif_8ch_p, 0, 23, 15,
	    1),

	/*  CRU_CLKSEL_CON24 */
	FRACT(0, "mclk_spdif_8ch_frac_div", "mclk_spdif_8ch_src", 0, 24),

	/*  CRU_CLKSEL_CON25 */
	COMP(0, "sclk_audpwm_src_c", gpll_cpll_p, 0, 25, 0, 5, 14, 1),
	/* 6:13 Reserved */
	MUX(SCLK_AUDPWM, "sck_audpwm_sel", sclk_audpwm_p, 0, 25, 15, 1),

	/*  CRU_CLKSEL_CON26 */
	FRACT(0, "sclk_audpwm_frac_frac", "sclk_audpwm_src", 0, 26),

	/*  CRU_CLKSEL_CON27 */
	MUX(0, "aclk_secure_flash_sel", gpll200_gpll150_gpll100_xin24m_p, 0, 27, 0, 2),
	MUX(0, "hclk_secure_flash_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 27, 2, 2),
	MUX(0, "clk_crypto_ns_core_sel", gpll200_gpll150_gpll100_p, 0, 27, 4, 2),
	MUX(0, "clk_crypto_ns_pka_sel", gpll300_gpll200_gpll100_p, 0, 27, 6, 2),
	/* 8:15 Reserved */

	/*  CRU_CLKSEL_CON28 */
	MUX(0, "nclk_nandc_sel", clk_nandc_p, 0, 28, 0, 2),
	/* 2:3 Reserved */
	MUX(0, "sclk_sfc_sel", sclk_sfc_p, 0, 28, 4, 3),
	/* 7 Reserved */
	MUX(0, "bclk_emmc_sel", gpll200_gpll150_cpll125_p, 0, 28, 8, 2),
	/* 10:11 Reserved */
	MUX(0, "cclk_emmc_sel", cclk_emmc_p, 0, 28, 12, 3),
	/* 15 Reserved */

	/*  CRU_CLKSEL_CON29 */
	MUX(0, "aclk_pipe_sel", aclk_pipe_p, 0, 29, 0, 2),
	/* 2:3 Reserved */
	CDIV(0, "pclk_pipe_div", "aclk_pipe", 0, 29, 4, 4),
	MUX(0, "clk_usb3otg0_suspend_sel", xin24m_32k_p, 0, 29, 8, 1),
	MUX(0, "clk_usb3otg1_suspend_sel", xin24m_32k_p, 0, 29, 9, 1),
	/* 10:12 Reserved */
	MUX(0, "clk_xpcs_eee_sel", gpll200_cpll125_p, 0, 29, 13, 1),
	/* 14:15 Reserved */

	/*  CRU_CLKSEL_CON30 */
	MUX(0, "aclk_php_sel", gpll300_gpll200_gpll100_xin24m_p, 0, 30, 0, 2),
	MUX(0, "hclk_php_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 30, 2, 2),
	CDIV(0, "pclk_php_div", "aclk_php", 0, 30, 4, 4),
	MUX(0, "clk_sdmmc0_sel", clk_sdmmc_p, 0, 30, 8, 3),
	/* 11 Reserved */
	MUX(0, "clk_sdmmc1_sel", clk_sdmmc_p, 0, 30, 12, 3),
	/* 15 Reserved */

	/*  CRU_CLKSEL_CON31 */
	MUX(SCLK_GMAC0_RX_TX, "clk_gmac0_rx_tx", mux_gmac0_rx_tx_p, 0, 31,
	    0, 2),
	MUX(SCLK_GMAC0, "clk_gmac0", mux_gmac0_p, 0, 31, 2, 1),
	MUX(SCLK_GMAC0_RMII_SPEED, "clk_gmac0_rmii_speed",
	    mux_gmac0_rmii_speed_p, 0, 31, 3, 1),
	MUX(SCLK_GMAC0_RGMII_SPEED, "clk_gmac0_rgmii_speed",
	    mux_gmac0_rgmii_speed_p, 0, 31, 4, 2),
	MUX(0, "clk_mac0_2top_sel", clk_mac_2top_p, 0, 31, 8, 2),
	MUX(0, "clk_gmac0_ptp_ref_sel", clk_gmac_ptp_p, 0, 31, 12, 2),
	MUX(0, "clk_mac0_out_sel", cpll125_cpll50_cpll25_xin24m_p, 0, 31, 14, 2),

	FFACT(0, "clk_gmac0_tx_div5", "clk_gmac0", 1, 5),
	FFACT(0, "clk_gmac0_tx_div50", "clk_gmac0", 1, 50),
	FFACT(0, "clk_gmac0_rx_div2", "clk_gmac0", 1, 2),
	FFACT(0, "clk_gmac0_rx_div20", "clk_gmac0", 1, 20),

	/*  CRU_CLKSEL_CON32 */
	MUX(0, "aclk_usb_sel", gpll300_gpll200_gpll100_xin24m_p, 0, 32, 0, 2),
	MUX(0, "hclk_usb_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 32, 4, 2),
	CDIV(0, "pclk_usb_div", "aclk_usb", 0, 32, 4, 4),
	MUX(0, "clk_sdmmc2_sel", clk_sdmmc_p, 0, 32, 8, 3),
	/* 11:15 Reserved */

	/*  CRU_CLKSEL_CON33 */
	MUX(SCLK_GMAC1_RX_TX, "clk_gmac1_rx_tx", mux_gmac1_rx_tx_p, 0, 33,
	    0, 2),
	MUX(SCLK_GMAC1, "clk_gmac1", mux_gmac1_p, 0, 33, 2, 1),
	MUX(SCLK_GMAC1_RMII_SPEED, "clk_gmac1_rmii_speed",
	    mux_gmac1_rmii_speed_p, 0, 33, 3, 1),
	MUX(SCLK_GMAC1_RGMII_SPEED, "clk_gmac1_rgmii_speed",
	    mux_gmac1_rgmii_speed_p, 0, 33, 4, 2),
	/* 6:7 Reserved */
	MUX(0, "clk_mac1_2top_sel", clk_mac_2top_p, 0, 33, 8, 2),
	MUX(0, "clk_gmac1_ptp_ref_sel", clk_gmac_ptp_p, 0, 33, 12, 2),
	MUX(0, "clk_mac1_out_sel", cpll125_cpll50_cpll25_xin24m_p, 0, 33, 14, 2),

	FFACT(0, "clk_gmac1_tx_div5", "clk_gmac1", 1, 5),
	FFACT(0, "clk_gmac1_tx_div50", "clk_gmac1", 1, 50),
	FFACT(0, "clk_gmac1_rx_div2", "clk_gmac1", 1, 2),
	FFACT(0, "clk_gmac1_rx_div20", "clk_gmac1", 1, 20),

	/*  CRU_CLKSEL_CON34 */
	MUX(0, "aclk_vi_sel", gpll400_gpll300_gpll200_xin24m_p, 0, 34, 0, 2),
	/* 2:3 Reserved */
	CDIV(0, "hclk_vi_div", "aclk_vi", 0, 34, 4, 4),
	CDIV(0, "pclk_vi_div", "aclk_vi", 0, 34, 8, 4),
	/* 12:13 Reserved */
	MUX(0, "dclk_vicap1_sel", cpll333_gpll300_gpll200_p, 0, 34, 14, 2),

	/*  CRU_CLKSEL_CON35 */
	COMP(0, "clk_isp_c", cpll_gpll_hpll_p, 0, 35, 0, 5, 6, 2),
	/* 5 Reserved */
	COMP(0, "clk_cif_out_c", gpll_usb480m_xin24m_p, 0, 35, 8, 6, 14, 2),

	/*  CRU_CLKSEL_CON36 */
	COMP(0, "clk_cam0_out_c", gpll_usb480m_xin24m_p, 0, 36, 0, 6, 6, 2),
	COMP(0, "clk_cam1_out_c", gpll_usb480m_xin24m_p, 0, 36, 8, 6, 14, 2),

	/*  CRU_CLKSEL_CON37 */
	MUX(0, "aclk_vo_sel", gpll300_cpll250_gpll100_xin24m_p, 0, 37, 0, 2),
	/* 2:7 Reserved */
	CDIV(0, "hclk_vo_div", "aclk_vo", 0, 37, 8, 4),
	CDIV(0, "pclk_vo_div", "aclk_vo", 0, 37, 12, 4),

	/*  CRU_CLKSEL_CON38 */
	COMP(0, "aclk_vop_pre_c", cpll_gpll_hpll_vpll_p, 0, 38, 0, 5, 6, 2),
	/* 5 Reserved */
	MUX(0, "clk_edp_200m_sel", gpll200_gpll150_cpll125_p, 0, 38, 8, 2),
	/* 10:15 Reserved */

	/*  CRU_CLKSEL_CON39 */
	COMP(0, "dclk_vop0_c", hpll_vpll_gpll_cpll_p, 0, 39, 0, 8, 10, 2),
	/* 12:15 Reserved */

	/*  CRU_CLKSEL_CON40 */
	COMP(0, "dclk_vop1_c", hpll_vpll_gpll_cpll_p, 0, 40, 0, 8, 10, 2),
	/* 12:15 Reserved */

	/*  CRU_CLKSEL_CON41 */
	COMP(0, "dclk_vop2_c", hpll_vpll_gpll_cpll_p, 0, 41, 0, 8, 10, 2),
	/* 12:15 Reserved */

	/*  CRU_CLKSEL_CON42 */
	COMP(0, "aclk_vpu_pre_c", gpll_cpll_p, 0, 42, 0, 5, 7, 1),
	/* 5:6 Reserved */
	CDIV(0, "hclk_vpu_pre_div", "aclk_vpu_pre", 0, 42, 8, 4),
	/* 12:15 Reserved */

	/*  CRU_CLKSEL_CON43 */
	MUX(0, "aclk_rga_pre_sel", gpll300_cpll250_gpll100_xin24m_p, 0, 43, 0, 2),
	MUX(0, "clk_rga_core_sel", gpll300_gpll200_gpll100_p, 0, 43, 2, 2),
	MUX(0, "clk_iep_core_sel", gpll300_gpll200_gpll100_p, 0, 43, 4, 2),
	MUX(0, "dclk_ebc_sel", gpll400_cpll333_gpll200_p, 0, 43, 6, 2),
	CDIV(0, "hclk_rga_pre_div", "aclk_rga_pre", 0, 43, 8, 4),
	CDIV(0, "pclk_rga_pre_div", "aclk_rga_pre", 0, 43, 12, 4),

	/* CRU_CLKSEL_CON44 */
	COMP(0, "aclk_rkvenc_pre_c", gpll_cpll_npll_p, 0, 44, 0, 5, 6, 2),
	/* 5 Reserved */
	CDIV(0, "hclk_rkvenc_pre_div", "aclk_rkvenc_pre", 0, 44, 8, 4),
	/* 12:15 Reserved */

	/* CRU_CLKSEL_CON45 */
	COMP(0, "clk_rkvenc_core_c", gpll_cpll_npll_vpll_p, 0, 45, 0, 5, 14, 2),
	/* 5:13 Reserved */

	/* CRU_CLKSEL_CON46 */

	/* CRU_CLKSEL_CON47 */
	COMP(0, "aclk_rkvdec_pre_c", aclk_rkvdec_pre_p, 0, 47, 0, 5, 7, 1),
	/* 5:6 Reserved */
	CDIV(0, "hclk_rkvdec_pre_div", "aclk_rkvdec_pre", 0, 47, 8, 4),
	/* 12:15 Reserved */

	/*  CRU_CLKSEL_CON48 */
	COMP(0, "clk_rkvdec_ca_c", gpll_cpll_npll_vpll_p, 0, 48, 0, 5, 6, 2),
	/* 5 Reserved */
	/* 8:15 Reserved */

	/*  CRU_CLKSEL_CON49 */
	COMP(0, "clk_rkvdec_hevc_ca_c", gpll_cpll_npll_vpll_p, 0, 49, 0, 5, 6, 2),
	/* 5 Reserved */
	COMP(0, "clk_rkvdec_core_c", clk_rkvdec_core_p, 0, 49, 8, 5, 14, 2),
	/* 13 Reserved */

	/*  CRU_CLKSEL_CON50 */
	MUX(0, "aclk_bus_sel", gpll200_gpll150_gpll100_xin24m_p, 0, 50, 0, 2),
	/* 2:3 Reserved */
	MUX(0, "pclk_bus_sel", gpll100_gpll75_cpll50_xin24m_p, 0, 50, 4, 2),
	/* 6:15 Reserved */

	/*  CRU_CLKSEL_CON51 */
	COMP(0, "clk_tsadc_tsen_c", xin24m_gpll100_cpll100_p, 0, 51, 0, 3, 4, 2),
	/* 6:7 Reserved */
	CDIV(0, "clk_tsadc_div", "clk_tsadc_tsen", 0, 51, 8, 7),
	/* 15 Reserved */

	/*  CRU_CLKSEL_CON52 */
	COMP(0, "clk_uart1_src_c", gpll_cpll_usb480m_p, 0, 52, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart1_sel", sclk_uart1_p, 0, 52, 12, 2),

	/*  CRU_CLKSEL_CON53 */
	FRACT(0, "clk_uart1_frac_frac", "clk_uart1_src", 0, 53),

	/*  CRU_CLKSEL_CON54 */
	COMP(0, "clk_uart2_src_c", gpll_cpll_usb480m_p, 0, 54, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart2_sel", sclk_uart2_p, 0, 52, 12, 2),

	/*  CRU_CLKSEL_CON55 */
	FRACT(0, "clk_uart2_frac_frac", "clk_uart2_src", 0, 55),

	/*  CRU_CLKSEL_CON56 */
	COMP(0, "clk_uart3_src_c", gpll_cpll_usb480m_p, 0, 54, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart3_sel", sclk_uart3_p, 0, 56, 12, 2),

	/*  CRU_CLKSEL_CON57 */
	FRACT(0, "clk_uart3_frac_frac", "clk_uart3_src", 0, 57),

	/*  CRU_CLKSEL_CON58 */
	COMP(0, "clk_uart4_src_c", gpll_cpll_usb480m_p, 0, 58, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart4_sel", sclk_uart4_p, 0, 58, 12, 2),

	/*  CRU_CLKSEL_CON59 */
	FRACT(0, "clk_uart4_frac_frac", "clk_uart4_src", 0, 59),

	/*  CRU_CLKSEL_CON60 */
	COMP(0, "clk_uart5_src_c", gpll_cpll_usb480m_p, 0, 60, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart5_sel", sclk_uart5_p, 0, 60, 12, 2),

	/*  CRU_CLKSEL_CON61 */
	FRACT(0, "clk_uart5_frac_frac", "clk_uart5_src", 0, 61),

	/*  CRU_CLKSEL_CON62 */
	COMP(0, "clk_uart6_src_c", gpll_cpll_usb480m_p, 0, 62, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart6_sel", sclk_uart6_p, 0, 62, 12, 2),

	/*  CRU_CLKSEL_CON63 */
	FRACT(0, "clk_uart6_frac_frac", "clk_uart6_src", 0, 63),

	/*  CRU_CLKSEL_CON64 */
	COMP(0, "clk_uart7_src_c", gpll_cpll_usb480m_p, 0, 64, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart7_sel", sclk_uart7_p, 0, 64, 12, 2),

	/*  CRU_CLKSEL_CON65 */
	FRACT(0, "clk_uart7_frac_frac", "clk_uart7_src", 0, 65),

	/*  CRU_CLKSEL_CON66 */
	COMP(0, "clk_uart8_src_c", gpll_cpll_usb480m_p, 0, 66, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart8_sel", sclk_uart8_p, 0, 66, 12, 2),

	/*  CRU_CLKSEL_CON67 */
	FRACT(0, "clk_uart8_frac_frac", "clk_uart8_src", 0, 67),

	/*  CRU_CLKSEL_CON68 */
	COMP(0, "clk_uart9_src_c", gpll_cpll_usb480m_p, 0, 68, 0, 7, 8, 2),
	/* 7 Reserved */
	/* 10:11 Reserved */
	MUX(0, "sclk_uart9_sel", sclk_uart9_p, 0, 68, 12, 2),

	/*  CRU_CLKSEL_CON69 */
	FRACT(0, "clk_uart9_frac_frac", "clk_uart9_src", 0, 69),

	/*  CRU_CLKSEL_CON70 */
	COMP(0, "clk_can0_c", gpll_cpll_p, 0, 70, 0, 5, 7, 1),
	/* 5:6 Reserved */
	COMP(0, "clk_can1_c", gpll_cpll_p, 0, 70, 8, 5, 15, 1),
	/* 13:14 Reserved */

	/*  CRU_CLKSEL_CON71 */
	COMP(0, "clk_can2_c", gpll_cpll_p, 0, 71, 0, 5, 7, 1),
	/* 5:6 Reserved */
	MUX(0, "clk_i2c_sel", clk_i2c_p, 0, 71, 8, 2),
	/* 10:15 Reserved */

	/*  CRU_CLKSEL_CON72 */
	MUX(0, "clk_spi0_sel", gpll200_xin24m_cpll100_p, 0, 72, 0, 2),
	MUX(0, "clk_spi1_sel", gpll200_xin24m_cpll100_p, 0, 72, 2, 2),
	MUX(0, "clk_spi2_sel", gpll200_xin24m_cpll100_p, 0, 72, 4, 2),
	MUX(0, "clk_spi3_sel", gpll200_xin24m_cpll100_p, 0, 72, 6, 2),
	MUX(0, "clk_pwm1_sel", gpll100_xin24m_cpll100_p, 0, 72, 8, 2),
	MUX(0, "clk_pwm2_sel", gpll100_xin24m_cpll100_p, 0, 72, 10, 2),
	MUX(0, "clk_pwm3_sel", gpll100_xin24m_cpll100_p, 0, 72, 12, 2),
	MUX(0, "dbclk_gpio_sel", xin24m_32k_p, 0, 72, 14, 1),
	/* 15 Reserved */

	/*  CRU_CLKSEL_CON73 */
	MUX(0, "aclk_top_high_sel", cpll500_gpll400_gpll300_xin24m_p, 0, 73, 0, 2),
	/* 2:3 Reserved */
	MUX(0, "aclk_top_low_sel", gpll400_gpll300_gpll200_xin24m_p, 0, 73, 4, 2),
	/* 6:7 Reserved */
	MUX(0, "hclk_top_sel", gpll150_gpll100_gpll75_xin24m_p, 0, 73, 8, 2),
	/* 10:11 Reserved */
	MUX(0, "pclk_top_sel", gpll100_gpll75_cpll50_xin24m_p, 0, 73, 12, 2),
	/* 14 Reserved */
	MUX(0, "clk_optc_arb_sel", xin24m_cpll100_p, 0, 73, 15 , 1),

	/* CRU_CLKSEL_CON74 */
	/* 0:7 clk_testout_div CDIV */
	/* 8:12 clk_testout_sel MUX */

	/*  CRU_CLKSEL_CON75 */
	CDIV(0, "clk_gpll_div_400m_div", "gpll", 0, 75, 0, 5),
	CDIV(0, "clk_gpll_div_300m_div", "gpll", 0, 75, 8, 5),

	/*  CRU_CLKSEL_CON76 */
	CDIV(0, "clk_gpll_div_200m_div", "gpll", 0, 76, 0, 5),
	CDIV(0, "clk_gpll_div_150m_div", "gpll", 0, 76, 8, 5),

	/*  CRU_CLKSEL_CON77 */
	CDIV(0, "clk_gpll_div_100m_div", "gpll", 0, 77, 0, 5),
	CDIV(0, "clk_gpll_div_75m_div", "gpll", 0, 77, 8, 5),

	/*  CRU_CLKSEL_CON78 */
	CDIV(0, "clk_gpll_div_20m_div", "gpll", 0, 78, 0, 6),
	CDIV(0, "clk_cpll_div_500m_div", "cpll", 0, 78, 8, 5),

	/*  CRU_CLKSEL_CON79 */
	CDIV(0, "clk_cpll_div_333m_div", "cpll", 0, 79, 0, 6),
	CDIV(0, "clk_cpll_div_250m_div", "cpll", 0, 79, 8, 5),

	/*  CRU_CLKSEL_CON80 */
	CDIV(0, "clk_cpll_div_125m_div", "cpll", 0, 80, 0, 6),
	CDIV(0, "clk_cpll_div_62P5m_div", "cpll", 0, 80, 8, 5),

	/*  CRU_CLKSEL_CON81 */
	CDIV(0, "clk_cpll_div_50m_div", "cpll", 0, 81, 0, 6),
	CDIV(0, "clk_cpll_div_25m_div", "cpll", 0, 81, 8, 5),

	/*  CRU_CLKSEL_CON82 */
	CDIV(0, "clk_cpll_div_100m_div", "cpll", 0, 82, 0, 6),
	CDIV(0, "clk_osc0_div_750k_div", "xin24m", 0, 82, 8, 5),

	/*  CRU_CLKSEL_CON83 */
	CDIV(0, "clk_i2s3_2ch_rx_src_div", "clk_i2s3_2ch_rx_src_sel", 0, 83, 0, 7),
	/* 7 Reserved */
	MUX(0, "clk_i2s3_2ch_rx_src_sel", gpll_cpll_npll_p, 0, 83, 8, 2),
	MUX(CLK_I2S3_2CH_RX, "clk_i2s3_2ch_rx", clk_i2s3_2ch_rx_p, 0, 83, 10,
	    2),
	/* 12:14 Reserved */
	MUX(0, "i2s3_mclkout_rx_sel", i2s3_mclkout_rx_p, 0, 83, 15, 1),

	/*  CRU_CLKSEL_CON84 */
	FRACT(0, "clk_i2s3_2ch_rx_frac_div", "clk_i2s3_2ch_rx_src", 0, 84),
};

/* GATES */
static struct rk_cru_gate rk3568_gates[] = {
	/* CRU_CLKGATE_CON00 */
	/* 0 clk_core */
	/* 1 clk_core0 */
	/* 2 clk_core1 */
	/* 3 clk_core2 */
	/* 4 clk_core3 */
	GATE(0, "sclk_core_src", "sclk_core_src_c",			0, 5),
	/* 6 clk_npll_core */
	/* 7 sclk_core */
	GATE(0, "atclk_core", "atclk_core_div",				0, 8),
	GATE(0, "gicclk_core", "gicclk_core_div",			0, 9),
	GATE(0, "pclk_core_pre", "pclk_core_pre_div",			0, 10),
	GATE(0, "periphclk_core_pre", "periphclk_core_pre_div",		0, 11),
	/* 12 pclk_core */
	/* 13 periphclk_core */
	/* 14 tsclk_core */
	/* 15 cntclk_core */

	/* CRU_CLKGATE_CON01 */
	/* 0 aclk_core */
	/* 1 aclk_core_biuddr */
	/* 2 aclk_core_biu2bus */
	/* 3 pclk_dgb_biu */
	/* 4 pclk_dbg */
	/* 5 pclk_dbg_daplite */
	/* 6 aclk_adb400_core2gic */
	/* 7 aclk_adb400_gic2core */
	/* 8 pclk_core_grf */
	GATE(PCLK_CORE_PVTM, "pclk_core_pvtm", "pclk_core_pre",		1, 9),
	GATE(CLK_CORE_PVTM, "clk_core_pvtm", "xin24m",			1, 10),
	GATE(CLK_CORE_PVTM_CORE, "clk_core_pvtm_core", "armclk",	1, 11),
	GATE(CLK_CORE_PVTPLL, "clk_core_pvtpll", "armclk",		1, 12),
	/* 13 clk_core_div2 */
	/* 14 clk_apll_core */
	/* 15 clk_jtag */

	/* CRU_CLKGATE_CON02 */
	/* 0 clk_gpu_src */
	GATE(CLK_GPU_SRC, "clk_gpu_src", "clk_gpu_pre_c", 2, 0),
	/* 1 Reserved */
	GATE(PCLK_GPU_PRE, "pclk_gpu_pre", "pclk_gpu_pre_div",		2, 2),
	GATE(CLK_GPU, "clk_gpu", "clk_gpu_pre_c",			2, 3),
	/* 4 aclk_gpu_biu */
	/* 5 pclk_gpu_biu */
	GATE(PCLK_GPU_PVTM, "pclk_gpu_pvtm", "pclk_gpu_pre",		2, 6),
	GATE(CLK_GPU_PVTM, "clk_gpu_pvtm", "xin24m",			2, 7),
	GATE(CLK_GPU_PVTM_CORE, "clk_gpu_pvtm_core", "clk_gpu_src",	2, 8),
	GATE(CLK_GPU_PVTPLL, "clk_gpu_pvtpll", "clk_gpu_src",		2, 9),
	/* 10 clk_gpu_div2 */
	GATE(ACLK_GPU_PRE, "aclk_gpu_pre", "aclk_gpu_pre_div",		2, 11),
	/* 12:15 Reserved */

	/* CRU_CLKGATE_CON03 */
	GATE(CLK_NPU_SRC, "clk_npu_src", "clk_npu_src_c",		3, 0),
	GATE(CLK_NPU_NP5, "clk_npu_np5", "clk_npu_np5_c",		3, 1),
	GATE(HCLK_NPU_PRE, "hclk_npu_pre", "hclk_npu_pre_div",		3, 2),
	GATE(PCLK_NPU_PRE, "pclk_npu_pre", "pclk_npu_pre_div",		3, 3),
	/* 4 aclk_npu_biu */
	GATE(ACLK_NPU_PRE, "aclk_npu_pre", "clk_npu",			3, 4),
	/* 5 hclk_npu_biu */
	/* 6 pclk_npu_biu */
	GATE(ACLK_NPU, "aclk_npu", "aclk_npu_pre",			3, 7),
	GATE(HCLK_NPU, "hclk_npu", "hclk_npu_pre",			3, 8),
	GATE(PCLK_NPU_PVTM, "pclk_npu_pvtm", "pclk_npu_pre",		3, 9),
	GATE(CLK_NPU_PVTM, "clk_npu_pvtm", "xin24m",			3, 10),
	GATE(CLK_NPU_PVTM_CORE, "clk_npu_pvtm_core", "clk_npu_pre_ndft",3, 11),
	GATE(CLK_NPU_PVTPLL, "clk_npu_pvtpll", "clk_npu_pre_ndft",	3, 12),
	/* 13 clk_npu_div2 */
	/* 14:15 Reserved */

	/* CRU_CLKGATE_CON04 */
	GATE(CLK_DDRPHY1X_SRC, "clk_ddrphy1x_src", "clk_ddrphy1x_src_c",	4, 0),
	/* 1 clk_dpll_ddr */
	GATE(CLK_MSCH, "clk_msch", "clk_msch_div",			4, 2),
	/* 3 clk_hwffc_ctrl */
	/* 4 aclk_ddrscramble */
	/* 5 aclk_msch */
	/* 6 clk_ddr_alwayson */
	/* 7 Reserved */
	/* 8 aclk_ddrsplit */
	/* 9 clk_ddrdft_ctl */
	/* 10 Reserved */
	/* 11 aclk_dma2ddr */
	/* 12 Reserved */
	/* 13 clk_ddrmon */
	/* 14 Reserved */
	GATE(CLK24_DDRMON, "clk24_ddrmon", "xin24m",			4, 15),

	/* CRU_CLKGATE_CON05 */
	GATE(ACLK_GIC_AUDIO, "aclk_gic_audio", "aclk_gic_audio_sel",	5, 0),
	GATE(HCLK_GIC_AUDIO, "hclk_gic_audio", "hclk_gic_audio_sel",	5, 1),
	/* 2 aclk_gic_audio_biu */
	/* 3 hclk_gic_audio_biu */
	GATE(ACLK_GIC600, "aclk_gic600", "aclk_gic_audio",		5, 4),
	/* 5 aclk_gicadb_core2gic */
	/* 6 aclk_gicadb_gic2core */
	GATE(ACLK_SPINLOCK, "aclk_spinlock", "aclk_gic_audio",		5, 7),
	GATE(HCLK_SDMMC_BUFFER, "hclk_sdmmc_buffer", "hclk_gic_audio",	5, 8),
	GATE(DCLK_SDMMC_BUFFER, "dclk_sdmmc_buffer", "dclk_sdmmc_buffer_sel", 5, 9),
	GATE(HCLK_I2S0_8CH, "hclk_i2s0_8ch", "hclk_gic_audio",		5, 10),
	GATE(HCLK_I2S1_8CH, "hclk_i2s1_8ch", "hclk_gic_audio",		5, 11),
	GATE(HCLK_I2S2_2CH, "hclk_i2s2_2ch", "hclk_gic_audio",		5, 12),
	GATE(HCLK_I2S3_2CH, "hclk_i2s3_2ch", "hclk_gic_audio",		5, 13),
	GATE(HCLK_PDM, "hclk_pdm", "hclk_gic_audio",			5, 14),
	GATE(MCLK_PDM, "mclk_pdm", "mclk_pdm_sel", 5, 15),

	/* CRU_CLKGATE_CON06 */
	GATE(CLK_I2S0_8CH_TX_SRC, "clk_i2s0_8ch_tx_src", "clk_i2s0_8ch_tx_src_c",	6, 0),
	GATE(CLK_I2S0_8CH_TX_FRAC, "clk_i2s0_8ch_tx_frac", "clk_i2s0_8ch_tx_frac_div",	6, 1),
	GATE(MCLK_I2S0_8CH_TX, "mclk_i2s0_8ch_tx", "clk_i2s0_8ch_tx",			6, 2),
	GATE(I2S0_MCLKOUT_TX, "i2s0_mclkout_tx", "i2s0_mclkout_tx_sel",			6, 3),
	GATE(CLK_I2S0_8CH_RX_SRC, "clk_i2s0_8ch_rx_src", "clk_i2s0_8ch_rx_src_c",	6, 4),
	GATE(CLK_I2S0_8CH_RX_FRAC, "clk_i2s0_8ch_rx_frac", "clk_i2s0_8ch_rx_frac_div",	6, 5),
	GATE(MCLK_I2S0_8CH_RX, "mclk_i2s0_8ch_rx", "clk_i2s0_8ch_rx",			6, 6),
	GATE(I2S0_MCLKOUT_RX, "i2s0_mclkout_rx", "i2s0_mclkout_rx_sel",			6, 7),
	GATE(CLK_I2S1_8CH_TX_SRC, "clk_i2s1_8ch_tx_src", "clk_i2s1_8ch_tx_src_c",	6, 8),
	GATE(CLK_I2S1_8CH_TX_FRAC, "clk_i2s1_8ch_tx_frac", "clk_i2s1_8ch_tx_frac_div",	6, 9),
	GATE(MCLK_I2S1_8CH_TX, "mclk_i2s1_8ch_tx", "clk_i2s1_8ch_tx",			6, 10),
	GATE(I2S1_MCLKOUT_TX, "i2s1_mclkout_tx", "i2s1_mclkout_tx_sel",			6, 11),
	GATE(CLK_I2S1_8CH_RX_SRC, "clk_i2s1_8ch_rx_src", "clk_i2s1_8ch_rx_src_c",	6, 12),
	GATE(CLK_I2S1_8CH_RX_FRAC, "clk_i2s1_8ch_rx_frac", "clk_i2s1_8ch_rx_frac_div",	6, 13),
	GATE(MCLK_I2S1_8CH_RX, "mclk_i2s1_8ch_rx", "clk_i2s1_8ch_rx",			6, 14),
	GATE(I2S1_MCLKOUT_RX, "i2s1_mclkout_rx", "i2s1_mclkout_rx_sel",			6, 15),

	/* CRU_CLKGATE_CON07 */
	GATE(CLK_I2S2_2CH_SRC, "clk_i2s2_2ch_src", "clk_i2s2_2ch_src_c",		7, 0),
	GATE(CLK_I2S2_2CH_FRAC, "clk_i2s2_2ch_frac", "clk_i2s2_2ch_frac_div",		7, 1),
	GATE(MCLK_I2S2_2CH, "mclk_i2s2_2ch", "clk_i2s2_2ch",				7, 2),
	GATE(I2S2_MCLKOUT, "i2s2_mclkout", "i2s2_mclkout_sel",				7, 3),
	GATE(CLK_I2S3_2CH_TX, "clk_i2s3_2ch_tx_src", "clk_i2s3_2ch_tx_src_c",		7, 4),
	GATE(CLK_I2S3_2CH_TX_FRAC, "clk_i2s3_2ch_tx_frac", "clk_i2s3_2ch_tx_frac_div",	7, 5),
	GATE(MCLK_I2S3_2CH_TX, "mclk_i2s3_2ch_tx", "clk_i2s3_2ch_tx",			7, 6),
	GATE(I2S3_MCLKOUT_TX, "i2s3_mclkout_tx", "i2s3_mclkout_tx_sel",			7, 7),
	GATE(CLK_I2S3_2CH_RX, "clk_i2s3_2ch_rx_src", "clk_i2s3_2ch_rx_src_div",		7, 8),
	GATE(CLK_I2S3_2CH_RX_FRAC, "clk_i2s3_2ch_rx_frac", "clk_i2s3_2ch_rx_frac_div",	7, 9),
	GATE(MCLK_I2S3_2CH_RX, "mclk_i2s3_2ch_rx", "clk_i2s3_2ch_rx",			7, 10),
	GATE(I2S3_MCLKOUT_RX, "i2s3_mclkout_rx", "i2s3_mclkout_rx_sel",			7, 11),
	GATE(HCLK_VAD, "hclk_vad", "hclk_gic_audio",					7, 12),
	GATE(HCLK_SPDIF_8CH, "hclk_spdif_8ch", "hclk_gic_audio",			7, 13),
	GATE(MCLK_SPDIF_8CH_SRC, "mclk_spdif_8ch_src", "mclk_spdif_8ch_src_c",	7, 14),
	GATE(MCLK_SPDIF_8CH_FRAC, "mclk_spdif_8ch_frac", "mclk_spdif_8ch_frac_div",	7, 15),

	/* CRU_CLKGATE_CON08 */
	GATE(HCLK_AUDPWM, "hclk_audpwm", "hclk_gic_audio",				8, 0),
	GATE(SCLK_AUDPWM_SRC, "sclk_audpwm_src", "sclk_audpwm_src_c",			8, 1),
	GATE(SCLK_AUDPWM_FRAC, "sclk_audpwm_frac", "sclk_audpwm_frac_frac",		8, 2),
	GATE(HCLK_ACDCDIG, "hclk_acdcdig", "hclk_gic_audio",				8, 3),
	GATE(CLK_ACDCDIG_I2C, "clk_acdcdig_i2c", "clk_acdcdig_i2c_sel",			8, 4),
	GATE(CLK_ACDCDIG_DAC, "clk_acdcdig_dac", "mclk_i2s3_2ch_tx",			8, 5),
	GATE(CLK_ACDCDIG_ADC, "clk_acdcdig_adc", "mclk_i2s3_2ch_rx",			8, 6),
	GATE(ACLK_SECURE_FLASH, "aclk_secure_flash", "aclk_secure_flash_sel",		8, 7),
	GATE(HCLK_SECURE_FLASH, "hclk_secure_flash", "hclk_secure_flash_sel",		8, 8),
	/* 9 aclk_secure_flash_biu */
	/* 10 hclk_secure_flash_biu */
	GATE(ACLK_CRYPTO_NS, "aclk_crypto_ns", "aclk_secure_flash",			8, 11),
	GATE(HCLK_CRYPTO_NS, "hclk_crypto_ns", "hclk_secure_flash",			8, 12),
	GATE(CLK_CRYPTO_NS_CORE, "clk_crypto_ns_core", "clk_crypto_ns_core_sel",	8, 13),
	GATE(CLK_CRYPTO_NS_PKA, "clk_crypto_ns_pka", "clk_crypto_ns_pka_sel",		8, 14),
	GATE(CLK_CRYPTO_NS_RNG, "clk_crypto_ns_rng", "hclk_secure_flash",		8, 15),

	/* CRU_CLKGATE_CON09 */
	GATE(HCLK_NANDC, "hclk_nandc", "hclk_secure_flash",				9, 0),
	GATE(NCLK_NANDC, "nclk_nandc", "nclk_nandc_sel",				9, 1),
	GATE(HCLK_SFC, "hclk_sfc", "hclk_secure_flash",					9, 2),
	GATE(HCLK_SFC_XIP, "hclk_sfc_xip", "hclk_secure_flash",				9, 3),
	GATE(SCLK_SFC, "sclk_sfc", "sclk_sfc_sel",					9, 4),
	GATE(ACLK_EMMC, "aclk_emmc", "aclk_secure_flash",				9, 5),
	GATE(HCLK_EMMC, "hclk_emmc", "hclk_secure_flash",				9, 6),
	GATE(BCLK_EMMC, "bclk_emmc", "bclk_emmc_sel",					9, 7),
	GATE(CCLK_EMMC, "cclk_emmc", "cclk_emmc_sel",					9, 8),
	GATE(TCLK_EMMC, "tclk_emmc", "xin24m",						9, 9),
	GATE(HCLK_TRNG_NS, "hclk_trng_ns", "hclk_secure_flash",				9, 10),
	GATE(CLK_TRNG_NS, "clk_trng_ns", "hclk_secure_flash",				9, 11),
	/* 12:15 Reserved */

	/* CRU_CLKGATE_CON10 */
	GATE(ACLK_PIPE, "aclk_pipe", "aclk_pipe_sel",					10, 0),
	GATE(PCLK_PIPE, "pclk_pipe", "pclk_pipe_div",					10, 1),
	/* 2 aclk_pipe_biu */
	/* 3 pclk_pipe_biu */
	GATE(CLK_XPCS_EEE, "clk_xpcs_eee", "clk_xpcs_eee_sel",				10, 4),
	/* 5 clk_xpcs_rx_div10 */
	/* 6 clk_xpcs_tx_div10 */
	/* 7 pclk_pipe_grf */
	GATE(ACLK_USB3OTG0, "aclk_usb3otg0", "aclk_pipe",				10, 8),
	GATE(CLK_USB3OTG0_REF, "clk_usb3otg0_ref", "xin24m",				10, 9),
	GATE(CLK_USB3OTG0_SUSPEND, "clk_usb3otg0_suspend", "clk_usb3otg0_suspend_sel",	10, 10),
	/* 11 clk_usb3otg0_pipe */
	GATE(ACLK_USB3OTG1, "aclk_usb3otg1", "aclk_pipe",				10, 12),
	GATE(CLK_USB3OTG1_REF, "clk_usb3otg1_ref", "xin24m",				10, 13),
	GATE(CLK_USB3OTG1_SUSPEND, "clk_usb3otg1_suspend", "clk_usb3otg1_suspend_sel",	10, 14),
	/* 15 clk_usb3otg1_pipe */

	/* CRU_CLKGATE_CON11 */
	GATE(ACLK_SATA0, "aclk_sata0", "aclk_pipe",					11, 0),
	GATE(CLK_SATA0_PMALIVE, "clk_sata0_pmalive", "clk_gpll_div_20m",		11, 1),
	GATE(CLK_SATA0_RXOOB, "clk_sata0_rxoob", "clk_cpll_div_50m",			11, 2),
	/* 3 clk_sata0_pipe */
	GATE(ACLK_SATA1, "aclk_sata1", "aclk_pipe",					11, 4),
	GATE(CLK_SATA1_PMALIVE, "clk_sata1_pmalive", "clk_gpll_div_20m",		11, 5),
	GATE(CLK_SATA1_RXOOB, "clk_sata1_rxoob", "clk_cpll_div_50m",			11, 6),
	/* 7 clk_sata1_pipe */
	GATE(ACLK_SATA2, "aclk_sata2", "aclk_pipe",					11, 8),
	GATE(CLK_SATA2_PMALIVE, "clk_sata2_pmalive", "clk_gpll_div_20m",		11, 9),
	GATE(CLK_SATA2_RXOOB, "clk_sata2_rxoob", "clk_cpll_div_50m",			11, 10),
	/* 11 clk_sata2_pipe */
	/* 12:15 Reserved */

	/* CRU_CLKGATE_CON12 */
	GATE(ACLK_PCIE20_MST, "aclk_pcie20_mst", "aclk_pipe",				12, 0),
	GATE(ACLK_PCIE20_SLV, "aclk_pcie20_slv", "aclk_pipe",				12, 1),
	GATE(ACLK_PCIE20_DBI, "aclk_pcie20_dbi", "aclk_pipe",				12, 2),
	GATE(PCLK_PCIE20, "pclk_pcie20", "pclk_pipe",					12, 3),
	GATE(CLK_PCIE20_AUX_NDFT, "clk_pcie20_aux_ndft", "xin24m",			12, 4),
	/* 5 clk_pcie20_pipe */
	/* 6:7 Reserved */
	GATE(ACLK_PCIE30X1_MST, "aclk_pcie30x1_mst", "aclk_pipe",			12, 8),
	GATE(ACLK_PCIE30X1_SLV, "aclk_pcie30x1_slv", "aclk_pipe",			12, 9),
	GATE(ACLK_PCIE30X1_DBI, "aclk_pcie30x1_dbi", "aclk_pipe",			12, 10),
	GATE(PCLK_PCIE30X1, "pclk_pcie30x1", "pclk_pipe",				12, 11),
	GATE(CLK_PCIE30X1_AUX_NDFT, "clk_pcie30x1_aux_ndft", "xin24m",			12, 12),
	/* 13 clk_pcie30x1_pipe */
	/* 14:15 Reserved */

	/* CRU_CLKGATE_CON13 */
	GATE(ACLK_PCIE30X2_MST, "aclk_pcie30x2_mst", "aclk_pipe",			13, 0),
	GATE(ACLK_PCIE30X2_SLV, "aclk_pcie30x2_slv", "aclk_pipe",			13, 1),
	GATE(ACLK_PCIE30X2_DBI, "aclk_pcie30x2_dbi", "aclk_pipe",			13, 2),
	GATE(PCLK_PCIE30X2, "pclk_pcie30x2", "pclk_pipe",				13, 3),
	GATE(CLK_PCIE30X2_AUX_NDFT, "clk_pcie30x2_aux_ndft", "xin24m",			13, 4),
	/* 5 clk_pcie30x2_pipe */
	GATE(PCLK_XPCS, "pclk_xpcs", "pclk_pipe",					13, 6),
	/* 7 clk_xpcs_qsgmii_tx */
	/* 8 clk_xpcs_qsgmii_rx */
	/* 9 clk_xpcs_xgxs_tx */
	/* 10 Reserved */
	/* 11 clk_xpcs_xgxs_rx */
	/* 12 clk_xpcs_mii0_tx */
	/* 13 clk_xpcs_mii0_rx */
	/* 14 clk_xpcs_mii1_tx */
	/* 15 clk_xpcs_mii1_rx */

	/* CRU_CLKGATE_CON14 */
	GATE(ACLK_PERIMID, "aclk_perimid", "aclk_perimid_sel",				14, 0),
	GATE(HCLK_PERIMID, "hclk_perimid", "hclk_perimid_sel",				14, 1),
	/* 2 aclk_perimid_biu */
	/* 3 hclk_perimid_biu */
	/* 4:7 Reserved */
	GATE(ACLK_PHP, "aclk_php", "aclk_php_sel",					14, 8),
	GATE(HCLK_PHP, "hclk_php", "hclk_php_sel",					14, 9),
	GATE(PCLK_PHP, "pclk_php", "pclk_php_div",					14, 10),
	/* 11 aclk_php_biu */
	/* 12 hclk_php_biu */
	/* 13 pclk_php_biu */
	/* 14:15 Reserved */

	/* CRU_CLKGATE_CON15 */
	GATE(HCLK_SDMMC0, "hclk_sdmmc0", "hclk_php",					15, 0),
	GATE(CLK_SDMMC0, "clk_sdmmc0", "clk_sdmmc0_sel",				15, 1),
	GATE(HCLK_SDMMC1, "hclk_sdmmc1", "hclk_php",					15, 2),
	GATE(CLK_SDMMC1, "clk_sdmmc1", "clk_sdmmc1_sel",				15, 3),
	GATE(CLK_GMAC0_PTP_REF, "clk_gmac0_ptp_ref", "clk_gmac0_ptp_ref_sel",		15, 4),
	GATE(ACLK_GMAC0, "aclk_gmac0", "aclk_php",					15, 5),
	GATE(PCLK_GMAC0, "pclk_gmac0", "pclk_php",					15, 6),
	GATE(CLK_MAC0_2TOP, "clk_mac0_2top", "clk_mac0_2top_sel",			15, 7),
	GATE(CLK_MAC0_OUT, "clk_mac0_out", "clk_mac0_out_sel",				15, 8),
	/* 9:11 Reserved */
	GATE(CLK_MAC0_REFOUT, "clk_mac0_refout", "clk_mac0_2top",			15, 12),
	/* 13:15 Reserved */

	/* CRU_CLKGATE_CON16 */
	GATE(ACLK_USB, "aclk_usb", "aclk_usb_sel",					16, 0),
	GATE(HCLK_USB, "hclk_usb", "hclk_usb_sel",					16, 1),
	GATE(PCLK_USB, "pclk_usb", "pclk_usb_div",					16, 2),
	/* 3 aclk_usb_biu */
	/* 4 hclk_usb_biu */
	/* 5 pclk_usb_biu */
	/* 6 pclk_usb_grf */
	/* 7:11 Reserved */
	GATE(HCLK_USB2HOST0, "hclk_usb2host0", "hclk_usb",				16, 12),
	GATE(HCLK_USB2HOST0_ARB, "hclk_usb2host0_arb", "hclk_usb",			16, 13),
	GATE(HCLK_USB2HOST1, "hclk_usb2host1", "hclk_usb",				16, 14),
	GATE(HCLK_USB2HOST1_ARB, "hclk_usb2host1_arb", "hclk_usb",			16, 15),

	/* CRU_CLKGATE_CON17 */
	GATE(HCLK_SDMMC2, "hclk_sdmmc2", "hclk_usb",					17, 0),
	GATE(CLK_SDMMC2, "clk_sdmmc2", "clk_sdmmc2_sel",				17, 1),
	GATE(CLK_GMAC1_PTP_REF, "clK_gmac1_ptp_ref", "clk_gmac1_ptp_ref_sel",		17, 2),
	GATE(ACLK_GMAC1, "aclk_gmac1", "aclk_usb",					17, 3),
	GATE(PCLK_GMAC1, "pclk_gmac1", "pclk_usb",					17, 4),
	GATE(CLK_MAC1_2TOP, "clk_mac1_2top", "clk_mac1_2top_sel",			17, 5),
	GATE(CLK_MAC1_OUT, "clk_mac1_out", "clk_mac1_out_sel",				17, 6),
	/* 7:9 Reserved */
	GATE(CLK_MAC1_REFOUT, "clk_mac1_refout", "clk_mac1_2top",			17, 10),
	/* 11:15 Reserved */

	/* CRU_CLKGATE_CON18 */
	GATE(ACLK_VI, "aclk_vi", "aclk_vi_sel",						18, 0),
	GATE(HCLK_VI, "hclk_vi", "hclk_vi_div",						18, 1),
	GATE(PCLK_VI, "pclk_vi", "pclk_vi_div",						18, 2),
	/* 3 aclk_vi_biu */
	/* 4 hclk_vi_biu */
	/* 5 pclk_vi_biu */
	/* 6:8 Reserved */
	GATE(ACLK_VICAP, "aclk_vicap", "aclk_vi",					18, 9),
	GATE(HCLK_VICAP, "hclk_vicap", "hclk_vi",					18, 10),
	GATE(DCLK_VICAP, "dclk_vicap", "dclk_vicap1_sel",				18, 11),
	/* 12:15 Reserved */

	/* CRU_CLKGATE_CON19 */
	GATE(ACLK_ISP, "aclk_isp", "aclk_vi",						19, 0),
	GATE(HCLK_ISP, "hclk_isp", "hclk_vi",						19, 1),
	GATE(CLK_ISP, "clk_isp", "clk_isp_c",						19, 2),
	/* 3 Reserved */
	GATE(PCLK_CSI2HOST1, "pclk_csi2host1", "pclk_vi",				19, 4),
	/* 5:7 Reserved */
	GATE(CLK_CIF_OUT, "clk_cif_out", "clk_cif_out_c",				19, 8),
	GATE(CLK_CAM0_OUT, "clk_cam0_out", "clk_cam0_out_c",				19, 9),
	GATE(CLK_CAM1_OUT, "clk_cam1_out", "clk_cam1_out_c",				19, 9),
	/* 11:15 Reserved */

	/* CRU_CLKGATE_CON20 */
	/* 0 Reserved or aclk_vo ??? */
	GATE(ACLK_VO, "aclk_vo", "aclk_vo_sel",						20, 0),
	GATE(HCLK_VO, "hclk_vo", "hclk_vo_div",						20, 1),
	GATE(PCLK_VO, "pclk_vo", "pclk_vo_div",						20, 2),
	/* 3 aclk_vo_biu */
	/* 4 hclk_vo_biu */
	/* 5 pclk_vo_biu */
	GATE(ACLK_VOP_PRE, "aclk_vop_pre", "aclk_vop_pre_c",				20, 6),
	/* 7 aclk_vop_biu */
	GATE(ACLK_VOP, "aclk_vop", "aclk_vop_pre",					20, 8),
	GATE(HCLK_VOP, "hclk_vop", "hclk_vo",						20, 9),
	GATE(DCLK_VOP0, "dclk_vop0", "dclk_vop0_c",					20, 10),
	GATE(DCLK_VOP1, "dclk_vop1", "dclk_vop1_c",					20, 11),
	GATE(DCLK_VOP2, "dclk_vop2", "dclk_vop2_c",					20, 12),
	GATE(CLK_VOP_PWM, "clk_vop_pwm", "xin24m",					20, 13),
	/* 14:15 Reserved */

	/* CRU_CLKGATE_CON21 */
	GATE(ACLK_HDCP, "aclk_hdcp", "aclk_vo",						21, 0),
	GATE(HCLK_HDCP, "hclk_hdcp", "hclk_vo",						21, 1),
	GATE(PCLK_HDCP, "pclk_hdcp", "pclk_vo",						21, 2),
	GATE(PCLK_HDMI_HOST, "pclk_hdmi_host", "pclk_vo",				21, 3),
	GATE(CLK_HDMI_SFR, "clk_hdmi_sfr", "xin24m",					21, 4),
	GATE(CLK_HDMI_CEC, "clk_hdmi_cec", "clk_rtc_32k",				21, 5),
	GATE(PCLK_DSITX_0, "pclk_dsitx_0", "pclk_vo",					21, 6),
	GATE(PCLK_DSITX_1, "pclk_dsitx_1", "pclk_vo",					21, 7),
	GATE(PCLK_EDP_CTRL, "pclk_edp_ctrl", "pclk_vo",					21, 8),
	GATE(CLK_EDP_200M, "clk_edp_200m", "clk_edp_200m_sel",				21, 9),
	/* 10:15 Reserved */

	/* CRU_CLKGATE_CON22 */
	GATE(ACLK_VPU_PRE, "aclk_vpu_pre", "aclk_vpu_pre_c",				22, 0),
	GATE(HCLK_VPU_PRE, "hclk_vpu_pre", "aclk_vpu_pre_c",				22, 1),
	/* 2 aclk_vpu_biu */
	/* 3 hclk_vpu_biu */
	GATE(ACLK_VPU, "aclk_vpu", "aclk_vpu_pre",					22, 4),
	GATE(HCLK_VPU, "hclk_vpu", "hclk_vpu_pre",					22, 5),
	/* 6:11 Reserved */
	GATE(PCLK_RGA_PRE, "pclk_rga_pre", "pclk_rga_pre_div",				22, 12),
	/* 13 pclk_rga_biu */
	GATE(PCLK_EINK, "pclk_eink", "pclk_rga_pre",					22, 14),
	GATE(HCLK_EINK, "hclk_eink", "hclk_rga_pre",					22, 15),

	/* CRU_CLKGATE_CON23 */
	GATE(ACLK_RGA_PRE, "aclk_rga_pre", "aclk_rga_pre_sel",				23, 0),
	GATE(HCLK_RGA_PRE, "hclk_rga_pre", "hclk_rga_pre_div",				23, 1),
	/* 2 aclk_rga_biu */
	/* 3 hclk_rga_biu */
	GATE(ACLK_RGA, "aclk_rga", "aclk_rga_pre",					23, 4),
	GATE(HCLK_RGA, "hclk_rga", "hclk_rga_pre",					23, 5),
	GATE(CLK_RGA_CORE, "clk_rga_core", "clk_rga_core_sel",				23, 6),
	GATE(ACLK_IEP, "aclk_iep", "aclk_rga_pre",					23, 7),
	GATE(HCLK_IEP, "hclk_iep", "hclk_rga_pre",					23, 8),
	GATE(CLK_IEP_CORE, "clk_iep_core", "clk_iep_core_sel",				23, 9),
	GATE(HCLK_EBC, "hclk_ebc", "hclk_rga_pre",					23, 10),
	GATE(DCLK_EBC, "dclk_ebc", "dclk_ebc_sel",					23, 11),
	GATE(ACLK_JDEC, "aclk_jdec", "aclk_rga_pre",					23, 12),
	GATE(HCLK_JDEC, "hclk_jdec", "hclk_rga_pre",					23, 13),
	GATE(ACLK_JENC, "aclk_jenc", "aclk_rga_pre",					23, 14),
	GATE(HCLK_JENC, "hclk_jenc", "hclk_rga_pre",					23, 15),

	/* CRU_CLKGATE_CON24 */
	GATE(ACLK_RKVENC_PRE, "aclk_rkvenc_pre", "aclk_rkvenc_pre_c",			24, 0),
	GATE(HCLK_RKVENC_PRE, "hclk_rkvenc_pre", "hclk_rkvenc_pre_div",			24, 1),
	/* 2 Reserved */
	/* 3 aclk_rkvenc_biu */
	/* 4 hclk_rkvenc_biu */
	/* 5 Reserved */
	GATE(ACLK_RKVENC, "aclk_rkvenc", "aclk_rkvenc_pre",				24, 6),
	GATE(HCLK_RKVENC, "hclk_rkvenc", "hclk_rkvenc_pre",				24, 7),
	GATE(CLK_RKVENC_CORE, "clk_rkvenc_core", "clk_rkvenc_core_c",			24, 8),
	/* 9:15 Reserved */

	/* CRU_CLKGATE_CON25 */
	GATE(ACLK_RKVDEC_PRE, "aclk_rkvdec_pre", "aclk_rkvdec_pre_c",			25, 0),
	GATE(HCLK_RKVDEC_PRE, "hclk_rkvdec_pre", "hclk_rkvdec_pre_div",			25, 1),
	/* 2 aclk_rkvdec_biu */
	/* 3 hclk_rkvdec_biu */
	GATE(ACLK_RKVDEC, "aclk_rkvdec", "aclk_rkvdec_pre",				25, 4),
	GATE(HCLK_RKVDEC, "hclk_rkvdec", "hclk_rkvdec_pre",				25, 5),
	GATE(CLK_RKVDEC_CA, "clk_rkvdec_ca", "clk_rkvdec_ca_c",			25, 6),
	GATE(CLK_RKVDEC_CORE, "clk_rkvdec_core", "clk_rkvdec_core_c",			25, 7),
	GATE(CLK_RKVDEC_HEVC_CA, "clk_rkvdec_hevc_ca", "clk_rkvdec_hevc_ca_c",	25, 8),
	/* 9:15 Reserved */

	/* CRU_CLKGATE_CON26 */
	GATE(ACLK_BUS, "aclk_bus", "aclk_bus_sel",					26, 0),
	GATE(PCLK_BUS, "pclk_bus", "pclk_bus_sel",					26, 1),
	/* 2 aclk_bus_biu */
	/* 3 pclk_bus_biu */
	GATE(PCLK_TSADC, "pclk_tsadc", "pclk_bus",					26, 4),
	GATE(CLK_TSADC_TSEN, "clk_tsadc_tsen", "clk_tsadc_tsen_c",			26, 5),
	GATE(CLK_TSADC, "clk_tsadc", "clk_tsadc_div",					26, 6),
	GATE(PCLK_SARADC, "pclk_saradc", "pclk_bus",					26, 7),
	GATE(CLK_SARADC, "clk_saradc", "xin24m",					26, 8),
	GATE(PCLK_OTPC_NS, "pclk_otpc_ns", "hclk_secure_flash",				26, 9),
	GATE(CLK_OTPC_NS_SBPI, "clk_otpc_ns_sbpi", "xin24m",				26, 10),
	GATE(CLK_OTPC_NS_USR, "clk_otpc_ns_usr", "xin_osc0_half",			26, 11),
	GATE(PCLK_SCR, "pclk_scr", "pclk_bus",						26, 12),
	GATE(PCLK_WDT_NS, "pclk_wdt_ns", "pclk_bus",					26, 13),
	GATE(TCLK_WDT_NS, "tclk_wdt_ns", "xin24m",					26, 14),
	/* 15 Reserved */

	/* CRU_CLKGATE_CON27 */
	/* 0 pclk_grf */
	/* 1 pclk_grf_vccio12 */
	/* 2 pclk_grf_vccio34 */
	/* 3 pclk_grf_vccio567 */
	GATE(PCLK_CAN0, "pclk_can0", "pclk_bus",					27, 5),
	GATE(CLK_CAN0, "clk_can0", "clk_can0_c",					27, 6),
	GATE(PCLK_CAN1, "pclk_can1", "pclk_bus",					27, 7),
	GATE(CLK_CAN1, "clk_can1", "clk_can1_c",					27, 8),
	GATE(PCLK_CAN2, "pclk_can2", "pclk_bus",					27, 9),
	GATE(CLK_CAN2, "clk_can2", "clk_can2_c",					27, 10),
	/* 11 Reserved */
	GATE(PCLK_UART1, "pclk_uart1", "pclk_bus",					27, 12),
	GATE(CLK_UART1_SRC, "clk_uart1_src", "clk_uart1_src_c",			27, 13),
	GATE(CLK_UART1_FRAC, "clk_uart1_frac", "clk_uart1_frac_frac",			27, 14),
	GATE(SCLK_UART1, "sclk_uart1", "sclk_uart1_sel",				27, 15),

	/* CRU_CLKGATE_CON28 */
	GATE(PCLK_UART2, "pclk_uart2", "pclk_bus",					28, 0),
	GATE(CLK_UART2_SRC, "clk_uart2_src", "clk_uart2_src_c",			28, 1),
	GATE(CLK_UART2_FRAC, "clk_uart2_frac", "clk_uart2_frac_frac",			28, 2),
	GATE(SCLK_UART2, "sclk_uart2", "sclk_uart2_sel",				28, 3),
	GATE(PCLK_UART3, "pclk_uart3", "pclk_bus",					28, 4),
	GATE(CLK_UART3_SRC, "clk_uart3_src", "clk_uart3_src_c",			28, 5),
	GATE(CLK_UART3_FRAC, "clk_uart3_frac", "clk_uart3_frac_frac",			28, 6),
	GATE(SCLK_UART3, "sclk_uart3", "sclk_uart3_sel",				28, 7),
	GATE(PCLK_UART4, "pclk_uart4", "pclk_bus",					28, 8),
	GATE(CLK_UART4_SRC, "clk_uart4_src", "clk_uart4_src_c",			28, 9),
	GATE(CLK_UART4_FRAC, "clk_uart4_frac", "clk_uart4_frac_frac",			28, 10),
	GATE(SCLK_UART4, "sclk_uart4", "sclk_uart4_sel",				28, 11),
	GATE(PCLK_UART5, "pclk_uart5", "pclk_bus",					28, 12),
	GATE(CLK_UART5_SRC, "clk_uart5_src", "clk_uart5_src_c",			28, 13),
	GATE(CLK_UART5_FRAC, "clk_uart5_frac", "clk_uart5_frac_frac", 			28, 14),
	GATE(SCLK_UART5, "sclk_uart5", "sclk_uart5_sel",				28, 15),

	/* CRU_CLKGATE_CON29 */
	GATE(PCLK_UART6, "pclk_uart6", "pclk_bus",					29, 0),
	GATE(CLK_UART6_SRC, "clk_uart6_src", "clk_uart6_src_c",			29, 1),
	GATE(CLK_UART6_FRAC, "clk_uart6_frac", "clk_uart6_frac_frac",			29, 2),
	GATE(SCLK_UART6, "sclk_uart6", "sclk_uart6_sel",				29, 3),
	GATE(PCLK_UART7, "pclk_uart7", "pclk_bus",					29, 4),
	GATE(CLK_UART7_SRC, "clk_uart7_src", "clk_uart7_src_c",			29, 5),
	GATE(CLK_UART7_FRAC, "clk_uart7_frac", "clk_uart7_frac_frac",			29, 6),
	GATE(SCLK_UART7, "sclk_uart7", "sclk_uart7_sel",				29, 7),
	GATE(PCLK_UART8, "pclk_uart8", "pclk_bus",					29, 8),
	GATE(CLK_UART8_SRC, "clk_uart8_src", "clk_uart8_src_c",			29, 9),
	GATE(CLK_UART8_FRAC, "clk_uart8_frac", "clk_uart8_frac_frac",			29, 10),
	GATE(SCLK_UART8, "sclk_uart8", "sclk_uart8_sel",				29, 11),
	GATE(PCLK_UART9, "pclk_uart9", "pclk_bus",					29, 12),
	GATE(CLK_UART9_SRC, "clk_uart9_src", "clk_uart9_src_c",			29, 13),
	GATE(CLK_UART9_FRAC, "clk_uart9_frac", "clk_uart9_frac_frac",			29, 14),
	GATE(SCLK_UART9, "sclk_uart9", "sclk_uart9_sel",				29, 15),

	/* CRU_CLKGATE_CON30 */
	GATE(PCLK_I2C1, "pclk_i2c1", "pclk_bus",					30, 0),
	GATE(CLK_I2C1, "clk_i2c1", "clk_i2c",						30, 1),
	GATE(PCLK_I2C2, "pclk_i2c2", "pclk_bus",					30, 2),
	GATE(CLK_I2C2, "clk_i2c2", "clk_i2c",						30, 3),
	GATE(PCLK_I2C3, "pclk_i2c3", "pclk_bus",					30, 4),
	GATE(CLK_I2C3, "clk_i2c3", "clk_i2c",						30, 5),
	GATE(PCLK_I2C4, "pclk_i2c4", "pclk_bus",					30, 6),
	GATE(CLK_I2C4, "clk_i2c4", "clk_i2c",						30, 7),
	GATE(PCLK_I2C5, "pclk_i2c5", "pclk_bus",					30, 8),
	GATE(CLK_I2C5, "clk_i2c5", "clk_i2c",						30, 9),
	GATE(PCLK_SPI0, "pclk_spi0", "pclk_bus",					30, 10),
	GATE(CLK_SPI0, "clk_spi0", "clk_spi0_sel",					30, 11),
	GATE(PCLK_SPI1, "pclk_spi1", "pclk_bus",					30, 12),
	GATE(CLK_SPI1, "clk_spi1", "clk_spi1_sel",					30, 13),
	GATE(PCLK_SPI2, "pclk_spi2", "pclk_bus",					30, 14),
	GATE(CLK_SPI2, "clk_spi2", "clk_spi2_sel",					30, 15),

	/* CRU_CLKGATE_CON31 */
	GATE(PCLK_SPI3, "pclk_spi3", "pclk_bus",					31, 0),
	GATE(CLK_SPI3, "clk_spi3", "clk_spi3_sel",					31, 1),
	GATE(PCLK_GPIO1, "pclk_gpio1", "pclk_bus",					31, 2),
	GATE(DBCLK_GPIO1, "dbclk_gpio1", "dbclk_gpio",					31, 3),
	GATE(PCLK_GPIO2, "pclk_gpio2", "pclk_bus",					31, 4),
	GATE(DBCLK_GPIO2, "dbclk_gpio2", "dbclk_gpio",					31, 5),
	GATE(PCLK_GPIO3, "pclk_gpio3", "pclk_bus",					31, 6),
	GATE(DBCLK_GPIO3, "dbclk_gpio3", "dbclk_gpio",					31, 7),
	GATE(PCLK_GPIO4, "pclk_gpio4", "pclk_bus",					31, 8),
	GATE(DBCLK_GPIO4, "dbclk_gpio4", "dbclk_gpio",					31, 9),
	GATE(PCLK_PWM1, "pclk_pwm1", "pclk_bus",					31, 10),
	GATE(CLK_PWM1, "clk_pwm1", "clk_pwm1_sel",					31, 11),
	GATE(CLK_PWM1_CAPTURE, "clk_pwm1_capture", "xin24m",				31, 12),
	GATE(PCLK_PWM2, "pclk_pwm2", "pclk_bus",					31, 13),
	GATE(CLK_PWM2, "clk_pwm2", "clk_pwm2_sel",					31, 14),
	GATE(CLK_PWM2_CAPTURE, "clk_pwm2_capture", "xin24m",				31, 15),

	/* CRU_CLKGATE_CON32 */
	GATE(PCLK_PWM3, "pclk_pwm3", "pclk_bus",					32, 0),
	GATE(CLK_PWM3, "clk_pwm3", "clk_pwm3_sel",					32, 1),
	GATE(CLK_PWM3_CAPTURE, "clk_pwm3_capture", "xin24m",				32, 2),
	GATE(PCLK_TIMER, "pclk_timer", "pclk_bus",					32, 3),
	GATE(CLK_TIMER0, "clk_timer0", "xin24m",					32, 4),
	GATE(CLK_TIMER1, "clk_timer1", "xin24m",					32, 5),
	GATE(CLK_TIMER2, "clk_timer2", "xin24m",					32, 6),
	GATE(CLK_TIMER3, "clk_timer3", "xin24m",					32, 7),
	GATE(CLK_TIMER4, "clk_timer4", "xin24m",					32, 8),
	GATE(CLK_TIMER5, "clk_timer5", "xin24m",					32, 9),
	GATE(CLK_I2C, "clk_i2c", "clk_i2c_sel",						32, 10),
	GATE(DBCLK_GPIO, "dbclk_gpio", "dbclk_gpio_sel",				32, 11),
	/* 12 clk_timer */
	GATE(ACLK_MCU, "aclk_mcu", "aclk_bus",						32, 13),
	GATE(PCLK_INTMUX, "pclk_intmux", "pclk_bus",					32, 14),
	GATE(PCLK_MAILBOX, "pclk_mailbox", "pclk_bus",					32, 15),

	/* CRU_CLKGATE_CON33 */
	GATE(ACLK_TOP_HIGH, "aclk_top_high", "aclk_top_high_sel",			33, 0),
	GATE(ACLK_TOP_LOW, "aclk_top_low", "aclk_top_low_sel",				33, 1),
	GATE(HCLK_TOP, "hclk_top", "hclk_top_sel",					33, 2),
	GATE(PCLK_TOP, "pclk_top", "pclk_top_sel",					33, 3),
	/* 4 aclk_top_high_biu */
	/* 5 aclk_top_low_biu */
	/* 6 hclk_top_biu */
	/* 7 pclk_top_biu */
	GATE(PCLK_PCIE30PHY, "pclk_pcie30phy", "pclk_top",				33, 8),
	GATE(CLK_OPTC_ARB, "clk_optc_arb", "clk_optc_arb_sel",				33, 9),
	/* 10:11 Reserved */
	/* 12 pclk_top_cru */
	GATE(PCLK_MIPICSIPHY, "pclk_mipicsiphy", "pclk_top",				33, 13),
	GATE(PCLK_MIPIDSIPHY0, "pclk_mipidsiphy0", "pclk_top",				33, 14),
	GATE(PCLK_MIPIDSIPHY1, "pclk_mipidsiphy1", "pclk_top",				33, 15),

	/* CRU_CLKGATE_CON34 */
	/* 0 pclk_apb2asb_chip_left */
	/* 1 pclk_apb2asb_chip_bottom */
	/* 2 pclk_asb2apb_chip_left */
	/* 3 pclk_asb2apb_chip_bottom */
	GATE(PCLK_PIPEPHY0, "pclk_pipephy0", "pclk_top",				34, 4),
	GATE(PCLK_PIPEPHY1, "pclk_pipephy1", "pclk_top",				34, 5),
	GATE(PCLK_PIPEPHY2, "pclk_pipephy2", "pclk_top",				34, 6),
	/* 7 pclk_usb2phy0_grf */
	/* 8 pclk_usb2phy1_grf */
	/* 9 pclk_ddrphy */
	/* 10 clk_ddrphy */
	GATE(PCLK_CPU_BOOST, "pclk_cpu_boost", "pclk_top",				34, 11),
	GATE(CLK_CPU_BOOST, "clk_cpu_boost", "xin24m",					34, 12),
	GATE(PCLK_OTPPHY, "pclk_otpphy", "pclk_top",					34, 13),
	GATE(PCLK_EDPPHY_GRF, "pclk_edpphy_grf", "pclk_top",				34, 14),
	/* 15 clk_testout */

	/* CRU_CLKGATE_CON35 */
	GATE(0, "clk_gpll_div_400m", "clk_gpll_div_400m_div",				35, 0),
	GATE(0, "clk_gpll_div_300m", "clk_gpll_div_300m_div",				35, 1),
	GATE(0, "clk_gpll_div_200m", "clk_gpll_div_200m_div",				35, 2),
	GATE(0, "clk_gpll_div_150m", "clk_gpll_div_150m_div",				35, 3),
	GATE(0, "clk_gpll_div_100m", "clk_gpll_div_100m_div",				35, 4),
	GATE(0, "clk_gpll_div_75m", "clk_gpll_div_75m_div",				35, 5),
	GATE(0, "clk_gpll_div_20m", "clk_gpll_div_20m_div",				35, 6),
	GATE(CPLL_500M, "clk_cpll_div_500m", "clk_cpll_div_500m_div",			35, 7),
	GATE(CPLL_333M, "clk_cpll_div_333m", "clk_cpll_div_333m_div",			35, 8),
	GATE(CPLL_250M, "clk_cpll_div_250m", "clk_cpll_div_250m_div",			35, 9),
	GATE(CPLL_125M, "clk_cpll_div_125m", "clk_cpll_div_125m_div",			35, 10),
	GATE(CPLL_100M, "clk_cpll_div_100m", "clk_cpll_div_100m_div",			35, 11),
	GATE(CPLL_62P5M, "clk_cpll_div_62P5m", "clk_cpll_div_62P5m_div",		35, 12),
	GATE(CPLL_50M, "clk_cpll_div_50m", "clk_cpll_div_50m_div",			35, 13),
	GATE(CPLL_25M, "clk_cpll_div_25m", "clk_cpll_div_25m_div",			35, 14),
	GATE(0, "clk_osc0_div_750k", "clk_osc0_div_750k_div",				35, 15),
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
