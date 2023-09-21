/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@freebsd.org>
 * Copyright (c) 2018 Val Packett <val@packett.cool>
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

#include <sys/cdefs.h>
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

#define	CRU_CLKSEL_CON(x)	(0x80 + (x) * 0x4)
#define	CRU_CLKGATE_CON(x)	(0x100 + (x) * 0x4)

#define	PLL_PPLL		1
#define	SCLK_32K_SUSPEND_PMU	2
#define	SCLK_SPI3_PMU		3
#define	SCLK_TIMER12_PMU	4
#define	SCLK_TIMER13_PMU	5
#define	SCLK_UART4_PMU		6
#define	SCLK_PVTM_PMU		7
#define	SCLK_WIFI_PMU		8
#define	SCLK_I2C0_PMU		9
#define	SCLK_I2C4_PMU		10
#define	SCLK_I2C8_PMU		11

#define	PCLK_PMU_SRC		19
#define	PCLK_PMU		20
#define	PCLK_PMUGRF_PMU		21
#define	PCLK_INTMEM1_PMU	22
#define	PCLK_GPIO0_PMU		23
#define	PCLK_GPIO1_PMU		24
#define	PCLK_SGRF_PMU		25
#define	PCLK_NOC_PMU		26
#define	PCLK_I2C0_PMU		27
#define	PCLK_I2C4_PMU		28
#define	PCLK_I2C8_PMU		29
#define	PCLK_RKPWM_PMU		30
#define	PCLK_SPI3_PMU		31
#define	PCLK_TIMER_PMU		32
#define	PCLK_MAILBOX_PMU	33
#define	PCLK_UART4_PMU		34
#define	PCLK_WDT_M0_PMU		35

#define	FCLK_CM0S_SRC_PMU	44
#define	FCLK_CM0S_PMU		45
#define	SCLK_CM0S_PMU		46
#define	HCLK_CM0S_PMU		47
#define	DCLK_CM0S_PMU		48
#define	PCLK_INTR_ARB_PMU	49
#define	HCLK_NOC_PMU		50

/* GATES */
static struct rk_cru_gate rk3399_pmu_gates[] = {
	/* PMUCRU_CLKGATE_CON0 */
	/* 0 Reserved */
	/* 1 fclk_cm0s_pmu_ppll_src_en */
	GATE(SCLK_SPI3_PMU, "clk_spi3_pmu", "clk_spi3_c", 0, 2),
	GATE(SCLK_TIMER12_PMU, "clk_timer0_pmu", "clk_timer_sel", 0, 3),
	GATE(SCLK_TIMER13_PMU, "clk_timer1_pmu", "clk_timer_sel", 0, 4),
	GATE(SCLK_UART4_PMU, "clk_uart4_pmu", "clk_uart4_sel", 0, 5),
	GATE(0, "clk_uart4_frac", "clk_uart4_frac_frac", 0, 6),
	/* 7 clk_pvtm_pmu_en */
	GATE(SCLK_WIFI_PMU, "clk_wifi_pmu", "clk_wifi_sel", 0, 8),
	GATE(SCLK_I2C0_PMU, "clk_i2c0_src", "clk_i2c0_div", 0, 9),
	GATE(SCLK_I2C4_PMU, "clk_i2c4_src", "clk_i2c4_div", 0, 10),
	GATE(SCLK_I2C8_PMU, "clk_i2c8_src", "clk_i2c8_div", 0, 11),
	/* 12:15 Reserved */

	/* PMUCRU_CLKGATE_CON1 */
	GATE(PCLK_PMU, "pclk_pmu", "pclk_pmu_src", 1, 0),
	/* 1 pclk_pmugrf_en */
	/* 2 pclk_intmem1_en */
	GATE(PCLK_GPIO0_PMU, "pclk_gpio0_pmu", "pclk_pmu_src", 1, 3),
	GATE(PCLK_GPIO1_PMU, "pclk_gpio1_pmu", "pclk_pmu_src", 1, 4),
	/* 5 pclk_sgrf_en */
	/* 6 pclk_noc_pmu_en */
	GATE(PCLK_I2C0_PMU, "pclk_i2c0_pmu", "pclk_pmu_src", 1, 7),
	GATE(PCLK_I2C4_PMU, "pclk_i2c4_pmu", "pclk_pmu_src", 1, 8),
	GATE(PCLK_I2C8_PMU, "pclk_i2c8_pmu", "pclk_pmu_src", 1, 9),
	GATE(PCLK_RKPWM_PMU, "pclk_rkpwm_pmu", "pclk_pmu_src", 1, 10),
	GATE(PCLK_SPI3_PMU, "pclk_spi3_pmu", "pclk_pmu_src", 1, 11),
	GATE(PCLK_TIMER_PMU, "pclk_timer_pmu", "pclk_pmu_src", 1, 12),
	GATE(PCLK_MAILBOX_PMU, "pclk_mailbox_pmu", "pclk_pmu_src", 1, 13),
	/* 14 pclk_uartm0_en */
	/* 15 pclk_wdt_m0_pmu_en */

	/* PMUCRU_CLKGATE_CON2 */
	/* 0 fclk_cm0s_en */
	/* 1 sclk_cm0s_en */
	/* 2 hclk_cm0s_en */
	/* 3 dclk_cm0s_en */
	/* 4 Reserved */
	/* 5 hclk_noc_pmu_en */
	/* 6:15 Reserved */
};

/*
 * PLLs
 */

static struct rk_clk_pll_rate rk3399_pll_rates[] = {
	{
		.freq = 2208000000,
		.refdiv = 1,
		.fbdiv = 92,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2184000000,
		.refdiv = 1,
		.fbdiv = 91,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2160000000,
		.refdiv = 1,
		.fbdiv = 90,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2136000000,
		.refdiv = 1,
		.fbdiv = 89,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2112000000,
		.refdiv = 1,
		.fbdiv = 88,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2088000000,
		.refdiv = 1,
		.fbdiv = 87,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2064000000,
		.refdiv = 1,
		.fbdiv = 86,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2040000000,
		.refdiv = 1,
		.fbdiv = 85,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 2016000000,
		.refdiv = 1,
		.fbdiv = 84,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1992000000,
		.refdiv = 1,
		.fbdiv = 83,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1968000000,
		.refdiv = 1,
		.fbdiv = 82,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1944000000,
		.refdiv = 1,
		.fbdiv = 81,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1920000000,
		.refdiv = 1,
		.fbdiv = 80,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1896000000,
		.refdiv = 1,
		.fbdiv = 79,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1872000000,
		.refdiv = 1,
		.fbdiv = 78,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1848000000,
		.refdiv = 1,
		.fbdiv = 77,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1824000000,
		.refdiv = 1,
		.fbdiv = 76,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1800000000,
		.refdiv = 1,
		.fbdiv = 75,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1776000000,
		.refdiv = 1,
		.fbdiv = 74,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1752000000,
		.refdiv = 1,
		.fbdiv = 73,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1728000000,
		.refdiv = 1,
		.fbdiv = 72,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1704000000,
		.refdiv = 1,
		.fbdiv = 71,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1680000000,
		.refdiv = 1,
		.fbdiv = 70,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1656000000,
		.refdiv = 1,
		.fbdiv = 69,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1632000000,
		.refdiv = 1,
		.fbdiv = 68,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1608000000,
		.refdiv = 1,
		.fbdiv = 67,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1600000000,
		.refdiv = 3,
		.fbdiv = 200,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1584000000,
		.refdiv = 1,
		.fbdiv = 66,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1560000000,
		.refdiv = 1,
		.fbdiv = 65,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1536000000,
		.refdiv = 1,
		.fbdiv = 64,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1512000000,
		.refdiv = 1,
		.fbdiv = 63,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1488000000,
		.refdiv = 1,
		.fbdiv = 62,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1464000000,
		.refdiv = 1,
		.fbdiv = 61,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1440000000,
		.refdiv = 1,
		.fbdiv = 60,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1416000000,
		.refdiv = 1,
		.fbdiv = 59,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1392000000,
		.refdiv = 1,
		.fbdiv = 58,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1368000000,
		.refdiv = 1,
		.fbdiv = 57,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1344000000,
		.refdiv = 1,
		.fbdiv = 56,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1320000000,
		.refdiv = 1,
		.fbdiv = 55,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1296000000,
		.refdiv = 1,
		.fbdiv = 54,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1272000000,
		.refdiv = 1,
		.fbdiv = 53,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1248000000,
		.refdiv = 1,
		.fbdiv = 52,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1200000000,
		.refdiv = 1,
		.fbdiv = 50,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1188000000,
		.refdiv = 2,
		.fbdiv = 99,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1104000000,
		.refdiv = 1,
		.fbdiv = 46,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1100000000,
		.refdiv = 12,
		.fbdiv = 550,
		.postdiv1 = 1,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1008000000,
		.refdiv = 1,
		.fbdiv = 84,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 1000000000,
		.refdiv = 1,
		.fbdiv = 125,
		.postdiv1 = 3,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 984000000,
		.refdiv = 1,
		.fbdiv = 82,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 960000000,
		.refdiv = 1,
		.fbdiv = 80,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 936000000,
		.refdiv = 1,
		.fbdiv = 78,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 912000000,
		.refdiv = 1,
		.fbdiv = 76,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 900000000,
		.refdiv = 4,
		.fbdiv = 300,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 888000000,
		.refdiv = 1,
		.fbdiv = 74,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 864000000,
		.refdiv = 1,
		.fbdiv = 72,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 840000000,
		.refdiv = 1,
		.fbdiv = 70,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 816000000,
		.refdiv = 1,
		.fbdiv = 68,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 800000000,
		.refdiv = 1,
		.fbdiv = 100,
		.postdiv1 = 3,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 700000000,
		.refdiv = 6,
		.fbdiv = 350,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 696000000,
		.refdiv = 1,
		.fbdiv = 58,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 676000000,
		.refdiv = 3,
		.fbdiv = 169,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 600000000,
		.refdiv = 1,
		.fbdiv = 75,
		.postdiv1 = 3,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 594000000,
		.refdiv = 1,
		.fbdiv = 99,
		.postdiv1 = 4,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 533250000,
		.refdiv = 8,
		.fbdiv = 711,
		.postdiv1 = 4,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 504000000,
		.refdiv = 1,
		.fbdiv = 63,
		.postdiv1 = 3,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 500000000,
		.refdiv = 6,
		.fbdiv = 250,
		.postdiv1 = 2,
		.postdiv2 = 1,
		.dsmpd = 1,
	},
	{
		.freq = 408000000,
		.refdiv = 1,
		.fbdiv = 68,
		.postdiv1 = 2,
		.postdiv2 = 2,
		.dsmpd = 1,
	},
	{
		.freq = 312000000,
		.refdiv = 1,
		.fbdiv = 52,
		.postdiv1 = 2,
		.postdiv2 = 2,
		.dsmpd = 1,
	},
	{
		.freq = 297000000,
		.refdiv = 1,
		.fbdiv = 99,
		.postdiv1 = 4,
		.postdiv2 = 2,
		.dsmpd = 1,
	},
	{
		.freq = 216000000,
		.refdiv = 1,
		.fbdiv = 72,
		.postdiv1 = 4,
		.postdiv2 = 2,
		.dsmpd = 1,
	},
	{
		.freq = 148500000,
		.refdiv = 1,
		.fbdiv = 99,
		.postdiv1 = 4,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 106500000,
		.refdiv = 1,
		.fbdiv = 71,
		.postdiv1 = 4,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 96000000,
		.refdiv = 1,
		.fbdiv = 64,
		.postdiv1 = 4,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 74250000,
		.refdiv = 2,
		.fbdiv = 99,
		.postdiv1 = 4,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 65000000,
		.refdiv = 1,
		.fbdiv = 65,
		.postdiv1 = 6,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 54000000,
		.refdiv = 1,
		.fbdiv = 54,
		.postdiv1 = 6,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{
		.freq = 27000000,
		.refdiv = 1,
		.fbdiv = 27,
		.postdiv1 = 6,
		.postdiv2 = 4,
		.dsmpd = 1,
	},
	{},
};

PLIST(xin24m_p) = {"xin24m"};
PLIST(xin24m_xin32k_p) = {"xin24m", "xin32k"};
PLIST(xin24m_ppll_p) = {"xin24m", "ppll"};
PLIST(uart4_p) = {"clk_uart4_c", "clk_uart4_frac", "xin24m"};
PLIST(wifi_p) = {"clk_wifi_c", "clk_wifi_frac"};

static struct rk_clk_pll_def ppll = {
	.clkdef = {
		.id = PLL_PPLL,
		.name = "ppll",
		.parent_names = xin24m_p,
		.parent_cnt = nitems(xin24m_p),
	},
	.base_offset = 0x00,

	.rates = rk3399_pll_rates,
};

static struct rk_clk rk3399_pmu_clks[] = {
	/* Linked clocks */
	LINK("xin32k"),

	{
		.type = RK3399_CLK_PLL,
		.clk.pll = &ppll
	},

	/* PMUCRU_CLKSEL_CON0 */
	CDIV(PCLK_PMU_SRC, "pclk_pmu_src", "ppll", 0, 0, 0, 5),
	/* 5:7 Reserved */
	/* 8:12 cm0s_div */
	/* 13:14 Reserved */
	/* 15 cm0s_clk_pll_sel */

	/* PMUCRU_CLKSEL_CON1 */
	COMP(0, "clk_spi3_c", xin24m_ppll_p, 0, 1, 0, 7, 7, 1),
	COMP(0, "clk_wifi_c", xin24m_ppll_p, 0, 1, 8, 5, 13, 1),
	MUX(0, "clk_wifi_sel", wifi_p, 0, 1, 14, 1),
	MUX(0, "clk_timer_sel", xin24m_xin32k_p, 0, 1, 15, 1),

	/* PMUCRU_CLKSEL_CON2 */
	CDIV(0, "clk_i2c0_div", "ppll", 0, 2, 0, 7),
	/* 7 Reserved */
	CDIV(0, "clk_i2c8_div", "ppll", 0, 2, 8, 7),
	/* 15 Reserved */

	/* PMUCRU_CLKSEL_CON3 */
	CDIV(0, "clk_i2c4_div", "ppll", 0, 3, 0, 7),
	/* 7:15 Reserved */

	/* PMUCRU_CLKSEL_CON4 */
	/* 0:9 clk_32k_suspend_div */
	/* 10:14 Reserved */
	/* 15 clk_32k_suspend_sel */

	/* PMUCRU_CLKSEL_CON5 */
	COMP(0, "clk_uart4_c", xin24m_ppll_p, 0, 5, 0, 7, 10, 1),
	/* 7 Reserved */
	MUX(0, "clk_uart4_sel", uart4_p, 0, 5, 8, 2),
	/* 11:15 Reserved */

	/* PMUCRU_CLKFRAC_CON0 / PMUCRU_CLKSEL_CON6 */
	FRACT(0, "clk_uart4_frac_frac", "clk_uart4_sel", 0, 6),

	/* PMUCRU_CLKFRAC_CON1 / PMUCRU_CLKSEL_CON7 */
	FRACT(0, "clk_wifi_frac", "clk_wifi_c", 0, 7),
};

static int
rk3399_pmucru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3399-pmucru")) {
		device_set_desc(dev, "Rockchip RK3399 PMU Clock and Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3399_pmucru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->gates = rk3399_pmu_gates;
	sc->ngates = nitems(rk3399_pmu_gates);

	sc->clks = rk3399_pmu_clks;
	sc->nclks = nitems(rk3399_pmu_clks);

	sc->reset_offset = 0x110;
	sc->reset_num = 30;

	return (rk_cru_attach(dev));
}

static device_method_t rk3399_pmucru_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3399_pmucru_probe),
	DEVMETHOD(device_attach,	rk3399_pmucru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3399_pmucru, rk3399_pmucru_driver, rk3399_pmucru_methods,
  sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3399_pmucru, simplebus, rk3399_pmucru_driver, 0, 0,
    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
