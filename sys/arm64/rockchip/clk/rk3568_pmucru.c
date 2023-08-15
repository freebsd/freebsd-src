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

#include <dev/extres/clk/clk_div.h>
#include <dev/extres/clk/clk_fixed.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/rockchip/clk/rk_cru.h>
#include <contrib/device-tree/include/dt-bindings/clock/rk3568-cru.h>


#define	CRU_PLLSEL_CON(x)		((x) * 0x20)
#define	CRU_CLKSEL_CON(x)		((x) * 0x4 + 0x100)
#define	CRU_CLKGATE_CON(x)		((x) * 0x4 + 0x180)

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
		.base_offset = CRU_PLLSEL_CON(_off),			\
		.mode_reg = 0x80,					\
		.mode_shift = _shift,					\
		.rates = rk3568_pll_rates,				\
	},								\
}

extern struct rk_clk_pll_rate rk3568_pll_rates[];

/* Parent clock defines */
PLIST(mux_pll_p) = { "xin24m" };
PLIST(xin24m_32k_p) = { "xin24m", "clk_rtc_32k" };
PLIST(sclk_uart0_p) = { "sclk_uart0_div", "sclk_uart0_frac", "xin24m" };
PLIST(sclk_uart0_div_p) = { "ppll", "usb480m", "cpll", "gpll" };
PLIST(clk_rtc32k_pmu_p) = { "clk_32k_pvtm", "xin32k", "clk_osc0_div32k" };
PLIST(clk_usbphy0_ref_p) = { "clk_ref24m", "xin_osc0_usbphy0_g" };
PLIST(clk_usbphy1_ref_p) = { "clk_ref24m", "xin_osc0_usbphy1_g" };
PLIST(clk_mipidsiphy0_ref_p) = { "clk_ref24m", "xin_osc0_mipidsiphy0_g" };
PLIST(clk_mipidsiphy1_ref_p) = { "clk_ref24m", "xin_osc0_mipidsiphy1_g" };
PLIST(clk_wifi_p) = { "clk_wifi_osc0", "clk_wifi_div" };
PLIST(clk_pciephy0_ref_p) = { "clk_pciephy0_osc0", "clk_pciephy0_div" };
PLIST(clk_pciephy1_ref_p) = { "clk_pciephy1_osc0", "clk_pciephy1_div" };
PLIST(clk_pciephy2_ref_p) = { "clk_pciephy2_osc0", "clk_pciephy2_div" };
PLIST(clk_hdmi_ref_p) = { "hpll", "hpll_ph0" };
PLIST(clk_pdpmu_p) = { "ppll", "gpll" };
PLIST(clk_pwm0_p) = { "xin24m", "clk_pdpmu" };

/* CLOCKS */
static struct rk_clk rk3568_clks[] = {
	/* External clocks */
	LINK("xin24m"),
	LINK("cpll"),
	LINK("gpll"),
	LINK("usb480m"),
	LINK("clk_32k_pvtm"),

	/* Fixed clocks */
	FFACT(0, "ppll_ph0", "ppll", 1, 2),
	FFACT(0, "ppll_ph180", "ppll", 1, 2),
	FFACT(0, "hpll_ph0", "hpll", 1, 2),

	/* PLL's */
	RK_PLL(PLL_PPLL, "ppll", mux_pll_p, 0, 0),
	RK_PLL(PLL_HPLL, "hpll", mux_pll_p, 2, 2),

	/* PMUCRU_PMUCLKSEL_CON00 */
	CDIV(0, "xin_osc0_div_div", "xin24m", 0, 0, 0, 5),
	MUX(0, "clk_rtc_32k_mux", clk_rtc32k_pmu_p, 0, 0, 6, 2),

	/* PMUCRU_PMUCLKSEL_CON01 */
	FRACT(0, "clk_osc0_div32k", "xin24m", 0, 1),

	/* PMUCRU_PMUCLKSEL_CON02 */
	CDIV(0, "pclk_pdpmu_pre", "clk_pdpmu", 0, 2, 0, 5),
	MUX(CLK_PDPMU, "clk_pdpmu", clk_pdpmu_p, 0, 2, 15, 1),

	/* PMUCRU_PMUCLKSEL_CON03 */
	CDIV(0, "clk_i2c0_div", "clk_pdpmu", 0, 3, 0, 7),

	/* PMUCRU_PMUCLKSEL_CON04 */
	CDIV(0, "sclk_uart0_div_div", "sclk_uart0_div_sel", 0, 4, 0, 7),
	MUX(0, "sclk_uart0_div_sel", sclk_uart0_div_p, 0, 4, 8, 2),
	MUX(0, "sclk_uart0_mux", sclk_uart0_p, 0, 4, 10, 2),

	/* PMUCRU_PMUCLKSEL_CON05 */
	FRACT(0, "sclk_uart0_frac_div", "sclk_uart0_div", 0, 5),

	/* PMUCRU_PMUCLKSEL_CON06 */
	CDIV(0, "clk_pwm0_div", "clk_pwm0_sel", 0, 6, 0, 7),
	MUX(0, "clk_pwm0_sel", clk_pwm0_p, 0, 6, 7, 1),
	MUX(0, "dbclk_gpio0_sel", xin24m_32k_p, 0, 6, 15, 1),

	/* PMUCRU_PMUCLKSEL_CON07 */
	CDIV(0, "clk_ref24m_div", "clk_pdpmu", 0, 7, 0, 6),

	/* PMUCRU_PMUCLKSEL_CON08 */
	MUX(CLK_USBPHY0_REF, "clk_usbphy0_ref", clk_usbphy0_ref_p, 0, 8, 0, 1),
	MUX(CLK_USBPHY1_REF, "clk_usbphy1_ref", clk_usbphy1_ref_p, 0, 8, 1, 1),
	MUX(CLK_MIPIDSIPHY0_REF, "clk_mipidsiphy0_ref", clk_mipidsiphy0_ref_p, 0, 8, 2, 1),
	MUX(CLK_MIPIDSIPHY1_REF, "clk_mipidsiphy1_ref", clk_mipidsiphy1_ref_p, 0, 8, 3, 1),
	MUX(CLK_HDMI_REF, "clk_hdmi_ref", clk_hdmi_ref_p, 0, 8, 7, 1),
	CDIV(0, "clk_wifi_div_div", "clk_pdpmu", 0, 8, 8, 6),
	MUX(CLK_WIFI, "clk_wifi", clk_wifi_p, 0, 8, 15, 1),

	/* PMUCRU_PMUCLKSEL_CON09 */
	CDIV(0, "clk_pciephy0_div_div", "ppll_ph0", 0, 9, 0, 3),
	MUX(CLK_PCIEPHY0_REF, "clk_pciephy0_ref",
	  clk_pciephy0_ref_p, 0, 9, 3, 1),
	CDIV(0, "clk_pciephy1_div_div", "ppll_ph0", 0, 9, 4, 3),
	MUX(CLK_PCIEPHY1_REF, "clk_pciephy1_ref",
	  clk_pciephy1_ref_p, 0, 9, 7, 1),
	CDIV(0, "clk_pciephy2_div_div", "ppll_ph0", 0, 9, 8, 3),
	MUX(CLK_PCIEPHY2_REF, "clk_pciephy2_ref",
	  clk_pciephy2_ref_p, 0, 9, 11, 1),
};

/* GATES */
static struct rk_cru_gate rk3568_gates[] = {
	/* PMUCRU_PMUGATE_CON00 */
	GATE(XIN_OSC0_DIV, "xin_osc0_div", "xin_osc0_div_div",			0, 0),
	GATE(CLK_RTC_32K, "clk_rtc_32k", "clk_rtc_32k_mux",			0, 1),
	GATE(PCLK_PDPMU, "pclk_pdpmu", "pclk_pdpmu_pre",			0, 2),
	GATE(PCLK_PMU, "pclk_pmu", "pclk_pdpmu",				0, 6),
	GATE(CLK_PMU, "clk_pmu", "xin24m",					0, 7),

	/* PMUCRU_PMUGATE_CON01 */
	GATE(PCLK_I2C0, "pclk_i2c0", "pclk_pdpmu",				1, 0),
	GATE(CLK_I2C0, "clk_i2c0", "clk_i2c0_div",				1, 1),
	GATE(PCLK_UART0, "pclk_uart0", "pclk_pdpmu",				1, 2),
	GATE(CLK_UART0_DIV, "sclk_uart0_div", "sclk_uart0_div_div",		1, 3),
	GATE(CLK_UART0_FRAC, "sclk_uart0_frac", "sclk_uart0_frac_div",		1, 4),
	GATE(SCLK_UART0, "sclk_uart0", "sclk_uart0_mux",			1, 5),
	GATE(PCLK_PWM0, "pclk_pwm0", "pclk_pdpmu",				1, 6),
	GATE(CLK_PWM0, "clk_pwm0", "clk_pwm0_div",				1, 7),
	GATE(CLK_CAPTURE_PWM0_NDFT, "clk_capture_pwm0_ndft", "xin24m",		1, 8),
	GATE(PCLK_GPIO0, "pclk_gpio0", "pclk_pdpmu",				1, 9),
	GATE(DBCLK_GPIO0, "dbclk_gpio0", "dbclk_gpio0_sel",			1, 10),
	GATE(PCLK_PMUPVTM, "pclk_pmupvtm", "pclk_pdpmu",			1, 11),
	GATE(CLK_PMUPVTM, "clk_pmupvtm", "xin24m",				1, 12),
	GATE(CLK_CORE_PMUPVTM, "clk_core_pmupvtm", "xin24m",			1, 13),

	/* PMUCRU_PMUGATE_CON02 */
	GATE(CLK_REF24M, "clk_ref24m", "clk_ref24m_div",			2, 0),
	GATE(XIN_OSC0_USBPHY0_G, "xin_osc0_usbphy0_g", "xin24m",		2, 1),
	GATE(XIN_OSC0_USBPHY1_G, "xin_osc0_usbphy1_g", "xin24m",		2, 2),
	GATE(XIN_OSC0_MIPIDSIPHY0_G, "xin_osc0_mipidsiphy0_g", "xin24m",	2, 3),
	GATE(XIN_OSC0_MIPIDSIPHY1_G, "xin_osc0_mipidsiphy1_g", "xin24m",	2, 4),
	GATE(CLK_WIFI_DIV, "clk_wifi_div", "clk_wifi_div_div",			2, 5),
	GATE(CLK_WIFI_OSC0, "clk_wifi_osc0", "xin24m",				2, 6),
	GATE(CLK_PCIEPHY0_DIV, "clk_pciephy0_div", "clk_pciephy0_div_div",	2, 7),
	GATE(CLK_PCIEPHY0_OSC0, "clk_pciephy0_osc0", "xin24m",			2, 8),
	GATE(CLK_PCIEPHY1_DIV, "clk_pciephy1_div", "clk_pciephy1_div_div",	2, 9),
	GATE(CLK_PCIEPHY1_OSC0, "clk_pciephy1_osc0", "xin24m",			2, 10),
	GATE(CLK_PCIEPHY2_DIV, "clk_pciephy2_div", "clk_pciephy2_div_div",	2, 11),
	GATE(CLK_PCIEPHY2_OSC0, "clk_pciephy2_osc0", "xin24m",			2, 12),
	GATE(CLK_PCIE30PHY_REF_M, "clk_pcie30phy_ref_m", "ppll_ph0",		2, 13),
	GATE(CLK_PCIE30PHY_REF_N, "clk_pcie30phy_ref_n", "ppll_ph180",		2, 14),
	GATE(XIN_OSC0_EDPPHY_G, "xin_osc0_edpphy_g", "xin24m",			2, 15),
};

static int
rk3568_pmucru_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "rockchip,rk3568-pmucru")) {
		device_set_desc(dev, "Rockchip RK3568 PMU Clock & Reset Unit");
		return (BUS_PROBE_DEFAULT);
	}

	return (ENXIO);
}

static int
rk3568_pmucru_attach(device_t dev)
{
	struct rk_cru_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;
	sc->clks = rk3568_clks;
	sc->nclks = nitems(rk3568_clks);
	sc->gates = rk3568_gates;
	sc->ngates = nitems(rk3568_gates);
	sc->reset_offset = 0x200;
	sc->reset_num = 4;

	return (rk_cru_attach(dev));
}

static device_method_t methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		rk3568_pmucru_probe),
	DEVMETHOD(device_attach,	rk3568_pmucru_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(rk3568_pmucru, rk3568_pmucru_driver, methods,
    sizeof(struct rk_cru_softc), rk_cru_driver);

EARLY_DRIVER_MODULE(rk3568_pmucru, simplebus, rk3568_pmucru_driver,
    0, 0, BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE);
