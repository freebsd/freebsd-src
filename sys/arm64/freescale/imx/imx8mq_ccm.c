/*-
 * SPDX-License-Identifier: BSD-2-Clause
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
 */

#include <sys/cdefs.h>
/*
 * Clocks driver for Freescale i.MX 8M Quad SoC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <sys/rman.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <machine/bus.h>

#include <arm64/freescale/imx/imx_ccm.h>
#include <arm64/freescale/imx/imx8mq_ccm.h>
#include <arm64/freescale/imx/clk/imx_clk_gate.h>
#include <arm64/freescale/imx/clk/imx_clk_mux.h>
#include <arm64/freescale/imx/clk/imx_clk_composite.h>
#include <arm64/freescale/imx/clk/imx_clk_sscg_pll.h>
#include <arm64/freescale/imx/clk/imx_clk_frac_pll.h>

#include "clkdev_if.h"

static const char *pll_ref_p[] = {
	"osc_25m", "osc_27m", "dummy", "dummy"
};
static const char *sys3_pll_out_p[] = {
	"sys3_pll1_ref_sel"
};
static const char * arm_pll_bypass_p[] = {
	"arm_pll", "arm_pll_ref_sel"
};
static const char * gpu_pll_bypass_p[] = {
	"gpu_pll", "gpu_pll_ref_sel"
};
static const char * vpu_pll_bypass_p[] = {
	"vpu_pll", "vpu_pll_ref_sel"
};
static const char * audio_pll1_bypass_p[] = {
	"audio_pll1", "audio_pll1_ref_sel"
};
static const char * audio_pll2_bypass_p[] = {
	"audio_pll2", "audio_pll2_ref_sel"
};
static const char * video_pll1_bypass_p[] = {
	"video_pll1", "video_pll1_ref_sel"
};
static const char *uart_p[] = {
	"osc_25m", "sys1_pll_80m", "sys2_pll_200m", "sys2_pll_100m", "sys3_pll_out",
	"clk_ext2", "clk_ext4", "audio_pll2_out"
};
static const char *usdhc_p[] = {
	"osc_25m", "sys1_pll_400m", "sys1_pll_800m", "sys2_pll_500m", "audio_pll2_out",
	"sys1_pll_266m", "sys3_pll_out", "sys1_pll_100m"
};
static const char *enet_axi_p[] = {
	"osc_25m", "sys1_pll_266m", "sys1_pll_800m", "sys2_pll_250m", "sys2_pll_200m",
	"audio_pll1_out", "video_pll1_out", "sys3_pll_out"
};
static const char *enet_ref_p[] = {
	"osc_25m", "sys2_pll_125m", "sys2_pll_500m", "sys2_pll_100m", "sys1_pll_160m",
	"audio_pll1_out", "video_pll1_out", "clk_ext4"
};
static const char *enet_timer_p[] = {
	"osc_25m", "sys2_pll_100m", "audio_pll1_out", "clk_ext1", "clk_ext2", "clk_ext3",
	"clk_ext4", "video_pll1_out"
};
static const char *enet_phy_ref_p[] = {
	"osc_25m", "sys2_pll_50m", "sys2_pll_125m", "sys2_pll_500m", "audio_pll1_out",
	"video_pll1_out", "audio_pll2_out"
};
static const char *usb_bus_p[] = {
	"osc_25m", "sys2_pll_500m", "sys1_pll_800m", "sys2_pll_100m", "sys2_pll_200m",
	"clk_ext2", "clk_ext4", "audio_pll2_out"
};
static const char *usb_core_phy_p[] = {
	"osc_25m", "sys1_pll_100m", "sys1_pll_40m", "sys2_pll_100m", "sys2_pll_200m",
	"clk_ext2", "clk_ext3", "audio_pll2_out"
};
static const char *i2c_p[] = {
	"osc_25m", "sys1_pll_160m", "sys2_pll_50m", "sys3_pll_out", "audio_pll1_out",
	"video_pll1_out", "audio_pll2_out", "sys1_pll_133m"
};
static const char *ahb_p[] = {
	"osc_25m", "sys1_pll_133m", "sys1_pll_800m", "sys1_pll_400m", "sys2_pll_125m",
	"sys3_pll_out", "audio_pll1_out", "video_pll1_out"
};

static struct imx_clk imx8mq_clks[] = {
	FIXED(IMX8MQ_CLK_DUMMY, "dummy", 0),

	LINK(IMX8MQ_CLK_32K, "ckil"),
	LINK(IMX8MQ_CLK_25M, "osc_25m"),
	LINK(IMX8MQ_CLK_27M, "osc_27m"),
	LINK(IMX8MQ_CLK_EXT1, "clk_ext1"),
	LINK(IMX8MQ_CLK_EXT2, "clk_ext2"),
	LINK(IMX8MQ_CLK_EXT3, "clk_ext3"),
	LINK(IMX8MQ_CLK_EXT4, "clk_ext4"),

	FIXED(IMX8MQ_SYS1_PLL_OUT, "sys1_pll_out", 800000000),
	FIXED(IMX8MQ_SYS2_PLL_OUT, "sys2_pll_out", 1000000000),
	SSCG_PLL(IMX8MQ_SYS3_PLL_OUT, "sys3_pll_out", sys3_pll_out_p, 0x48),

	MUX(IMX8MQ_ARM_PLL_REF_SEL, "arm_pll_ref_sel", pll_ref_p, 0, 0x28, 16, 2),
	MUX(IMX8MQ_GPU_PLL_REF_SEL, "gpu_pll_ref_sel", pll_ref_p, 0, 0x18, 16, 2),
	MUX(IMX8MQ_VPU_PLL_REF_SEL, "vpu_pll_ref_sel", pll_ref_p, 0, 0x20, 16, 2),
	MUX(IMX8MQ_AUDIO_PLL1_REF_SEL, "audio_pll1_ref_sel", pll_ref_p, 0, 0x0, 16, 2),
	MUX(IMX8MQ_AUDIO_PLL2_REF_SEL, "audio_pll2_ref_sel", pll_ref_p, 0, 0x8, 16, 2),
	MUX(IMX8MQ_VIDEO_PLL1_REF_SEL, "video_pll1_ref_sel", pll_ref_p, 0, 0x10, 16, 2),
	MUX(IMX8MQ_SYS3_PLL1_REF_SEL, "sys3_pll1_ref_sel", pll_ref_p, 0, 0x48, 0, 2),
	MUX(IMX8MQ_DRAM_PLL1_REF_SEL, "dram_pll1_ref_sel", pll_ref_p, 0, 0x60, 0, 2),
	MUX(IMX8MQ_VIDEO2_PLL1_REF_SEL, "video2_pll1_ref_sel", pll_ref_p, 0, 0x54, 0, 2),

	DIV(IMX8MQ_ARM_PLL_REF_DIV, "arm_pll_ref_div", "arm_pll_ref_sel", 0x28, 5, 6),
	DIV(IMX8MQ_GPU_PLL_REF_DIV, "gpu_pll_ref_div", "gpu_pll_ref_sel", 0x18, 5, 6),
	DIV(IMX8MQ_VPU_PLL_REF_DIV, "vpu_pll_ref_div", "vpu_pll_ref_sel", 0x20, 5, 6),
	DIV(IMX8MQ_AUDIO_PLL1_REF_DIV, "audio_pll1_ref_div", "audio_pll1_ref_sel", 0x0, 5, 6),
	DIV(IMX8MQ_AUDIO_PLL2_REF_DIV, "audio_pll2_ref_div", "audio_pll2_ref_sel", 0x8, 5, 6),
	DIV(IMX8MQ_VIDEO_PLL1_REF_DIV, "video_pll1_ref_div", "video_pll1_ref_sel", 0x10, 5, 6),

	FRAC_PLL(IMX8MQ_ARM_PLL, "arm_pll", "arm_pll_ref_div", 0x28),
	FRAC_PLL(IMX8MQ_GPU_PLL, "gpu_pll", "gpu_pll_ref_div", 0x18),
	FRAC_PLL(IMX8MQ_VPU_PLL, "vpu_pll", "vpu_pll_ref_div", 0x20),
	FRAC_PLL(IMX8MQ_AUDIO_PLL1, "audio_pll1", "audio_pll1_ref_div", 0x0),
	FRAC_PLL(IMX8MQ_AUDIO_PLL2, "audio_pll2", "audio_pll2_ref_div", 0x8),
	FRAC_PLL(IMX8MQ_VIDEO_PLL1, "video_pll1", "video_pll1_ref_div", 0x10),

	/* ARM_PLL needs SET_PARENT flag */
	MUX(IMX8MQ_ARM_PLL_BYPASS, "arm_pll_bypass", arm_pll_bypass_p, 0, 0x28, 14, 1),
	MUX(IMX8MQ_GPU_PLL_BYPASS, "gpu_pll_bypass", gpu_pll_bypass_p, 0, 0x18, 14, 1),
	MUX(IMX8MQ_VPU_PLL_BYPASS, "vpu_pll_bypass", vpu_pll_bypass_p, 0, 0x20, 14, 1),
	MUX(IMX8MQ_AUDIO_PLL1_BYPASS, "audio_pll1_bypass", audio_pll1_bypass_p, 0, 0x0, 14, 1),
	MUX(IMX8MQ_AUDIO_PLL2_BYPASS, "audio_pll2_bypass", audio_pll2_bypass_p, 0, 0x8, 14, 1),
	MUX(IMX8MQ_VIDEO_PLL1_BYPASS, "video_pll1_bypass", video_pll1_bypass_p, 0, 0x10, 14, 1),

	GATE(IMX8MQ_ARM_PLL_OUT, "arm_pll_out", "arm_pll_bypass", 0x28, 21),
	GATE(IMX8MQ_GPU_PLL_OUT, "gpu_pll_out", "gpu_pll_bypass", 0x18, 21),
	GATE(IMX8MQ_VPU_PLL_OUT, "vpu_pll_out", "vpu_pll_bypass", 0x20, 21),
	GATE(IMX8MQ_AUDIO_PLL1_OUT, "audio_pll1_out", "audio_pll1_bypass", 0x0, 21),
	GATE(IMX8MQ_AUDIO_PLL2_OUT, "audio_pll2_out", "audio_pll2_bypass", 0x8, 21),
	GATE(IMX8MQ_VIDEO_PLL1_OUT, "video_pll1_out", "video_pll1_bypass", 0x10, 21),

	GATE(IMX8MQ_SYS1_PLL_40M_CG, "sys1_pll_40m_cg", "sys1_pll_out", 0x30, 9),
	GATE(IMX8MQ_SYS1_PLL_80M_CG, "sys1_pll_80m_cg", "sys1_pll_out", 0x30, 11),
	GATE(IMX8MQ_SYS1_PLL_100M_CG, "sys1_pll_100m_cg", "sys1_pll_out", 0x30, 13),
	GATE(IMX8MQ_SYS1_PLL_133M_CG, "sys1_pll_133m_cg", "sys1_pll_out", 0x30, 15),
	GATE(IMX8MQ_SYS1_PLL_160M_CG, "sys1_pll_160m_cg", "sys1_pll_out", 0x30, 17),
	GATE(IMX8MQ_SYS1_PLL_200M_CG, "sys1_pll_200m_cg", "sys1_pll_out", 0x30, 19),
	GATE(IMX8MQ_SYS1_PLL_266M_CG, "sys1_pll_266m_cg", "sys1_pll_out", 0x30, 21),
	GATE(IMX8MQ_SYS1_PLL_400M_CG, "sys1_pll_400m_cg", "sys1_pll_out", 0x30, 23),
	GATE(IMX8MQ_SYS1_PLL_800M_CG, "sys1_pll_800m_cg", "sys1_pll_out", 0x30, 25),

	FFACT(IMX8MQ_SYS1_PLL_40M, "sys1_pll_40m", "sys1_pll_40m_cg", 1, 20),
	FFACT(IMX8MQ_SYS1_PLL_80M, "sys1_pll_80m", "sys1_pll_80m_cg", 1, 10),
	FFACT(IMX8MQ_SYS1_PLL_100M, "sys1_pll_100m", "sys1_pll_100m_cg", 1, 8),
	FFACT(IMX8MQ_SYS1_PLL_133M, "sys1_pll_133m", "sys1_pll_133m_cg", 1, 6),
	FFACT(IMX8MQ_SYS1_PLL_160M, "sys1_pll_160m", "sys1_pll_160m_cg", 1, 5),
	FFACT(IMX8MQ_SYS1_PLL_200M, "sys1_pll_200m", "sys1_pll_200m_cg", 1, 4),
	FFACT(IMX8MQ_SYS1_PLL_266M, "sys1_pll_266m", "sys1_pll_266m_cg", 1, 3),
	FFACT(IMX8MQ_SYS1_PLL_400M, "sys1_pll_400m", "sys1_pll_400m_cg", 1, 2),
	FFACT(IMX8MQ_SYS1_PLL_800M, "sys1_pll_800m", "sys1_pll_800m_cg", 1, 1),

	GATE(IMX8MQ_SYS2_PLL_50M_CG, "sys2_pll_50m_cg", "sys2_pll_out", 0x3c, 9),
	GATE(IMX8MQ_SYS2_PLL_100M_CG, "sys2_pll_100m_cg", "sys2_pll_out", 0x3c, 11),
	GATE(IMX8MQ_SYS2_PLL_125M_CG, "sys2_pll_125m_cg", "sys2_pll_out", 0x3c, 13),
	GATE(IMX8MQ_SYS2_PLL_166M_CG, "sys2_pll_166m_cg", "sys2_pll_out", 0x3c, 15),
	GATE(IMX8MQ_SYS2_PLL_200M_CG, "sys2_pll_200m_cg", "sys2_pll_out", 0x3c, 17),
	GATE(IMX8MQ_SYS2_PLL_250M_CG, "sys2_pll_250m_cg", "sys2_pll_out", 0x3c, 19),
	GATE(IMX8MQ_SYS2_PLL_333M_CG, "sys2_pll_333m_cg", "sys2_pll_out", 0x3c, 21),
	GATE(IMX8MQ_SYS2_PLL_500M_CG, "sys2_pll_500m_cg", "sys2_pll_out", 0x3c, 23),
	GATE(IMX8MQ_SYS2_PLL_1000M_CG, "sys2_pll_1000m_cg", "sys2_pll_out", 0x3c, 25),

	FFACT(IMX8MQ_SYS2_PLL_50M, "sys2_pll_50m", "sys2_pll_50m_cg", 1, 20),
	FFACT(IMX8MQ_SYS2_PLL_100M, "sys2_pll_100m", "sys2_pll_100m_cg", 1, 10),
	FFACT(IMX8MQ_SYS2_PLL_125M, "sys2_pll_125m", "sys2_pll_125m_cg", 1, 8),
	FFACT(IMX8MQ_SYS2_PLL_166M, "sys2_pll_166m", "sys2_pll_166m_cg", 1, 6),
	FFACT(IMX8MQ_SYS2_PLL_200M, "sys2_pll_200m", "sys2_pll_200m_cg", 1, 5),
	FFACT(IMX8MQ_SYS2_PLL_250M, "sys2_pll_250m", "sys2_pll_250m_cg", 1, 4),
	FFACT(IMX8MQ_SYS2_PLL_333M, "sys2_pll_333m", "sys2_pll_333m_cg", 1, 3),
	FFACT(IMX8MQ_SYS2_PLL_500M, "sys2_pll_500m", "sys2_pll_500m_cg", 1, 2),
	FFACT(IMX8MQ_SYS2_PLL_1000M, "sys2_pll_1000m", "sys2_pll_1000m_cg", 1, 1),

	COMPOSITE(IMX8MQ_CLK_AHB, "ahb", ahb_p, 0x9000, 0),
	DIV(IMX8MQ_CLK_IPG_ROOT, "ipg_root", "ahb", 0x9080, 0, 1),

	COMPOSITE(IMX8MQ_CLK_UART1, "uart1", uart_p, 0xaf00, 0),
	COMPOSITE(IMX8MQ_CLK_UART2, "uart2", uart_p, 0xaf80, 0),
	COMPOSITE(IMX8MQ_CLK_UART3, "uart3", uart_p, 0xb000, 0),
	COMPOSITE(IMX8MQ_CLK_UART4, "uart4", uart_p, 0xb080, 0),

	ROOT_GATE(IMX8MQ_CLK_UART1_ROOT, "uart1_root_clk", "uart1", 0x4490),
	ROOT_GATE(IMX8MQ_CLK_UART2_ROOT, "uart2_root_clk", "uart2", 0x44a0),
	ROOT_GATE(IMX8MQ_CLK_UART3_ROOT, "uart3_root_clk", "uart3", 0x44b0),
	ROOT_GATE(IMX8MQ_CLK_UART4_ROOT, "uart4_root_clk", "uart4", 0x44c0),

	COMPOSITE(IMX8MQ_CLK_USDHC1, "usdhc1", usdhc_p, 0xac00, CLK_SET_ROUND_DOWN),
	COMPOSITE(IMX8MQ_CLK_USDHC2, "usdhc2", usdhc_p, 0xac80, CLK_SET_ROUND_DOWN),

	ROOT_GATE(IMX8MQ_CLK_USDHC1_ROOT, "usdhc1_root_clk", "usdhc1", 0x4510),
	ROOT_GATE(IMX8MQ_CLK_USDHC2_ROOT, "usdhc2_root_clk", "usdhc2", 0x4520),

	ROOT_GATE(IMX8MQ_CLK_TMU_ROOT, "tmu_root_clk", "ipg_root", 0x4620),

	COMPOSITE(IMX8MQ_CLK_ENET_AXI, "enet_axi", enet_axi_p, 0x8800, 0),
	COMPOSITE(IMX8MQ_CLK_ENET_REF, "enet_ref", enet_ref_p, 0xa980, 0),
	COMPOSITE(IMX8MQ_CLK_ENET_TIMER, "enet_timer", enet_timer_p, 0xaa00, 0),
	COMPOSITE(IMX8MQ_CLK_ENET_PHY_REF, "enet_phy_ref", enet_phy_ref_p, 0xaa80, 0),

	ROOT_GATE(IMX8MQ_CLK_ENET1_ROOT, "enet1_root_clk", "enet_axi", 0x40a0),

	COMPOSITE(IMX8MQ_CLK_USB_BUS, "usb_bus", usb_bus_p, 0x8b80, 0),
	COMPOSITE(IMX8MQ_CLK_USB_CORE_REF, "usb_core_ref", usb_core_phy_p, 0xb100, 0),
	COMPOSITE(IMX8MQ_CLK_USB_PHY_REF, "usb_phy_ref", usb_core_phy_p, 0xb180, 0),

	ROOT_GATE(IMX8MQ_CLK_USB1_CTRL_ROOT, "usb1_ctrl_root_clk", "usb_bus", 0x44d0),
	ROOT_GATE(IMX8MQ_CLK_USB2_CTRL_ROOT, "usb2_ctrl_root_clk", "usb_bus", 0x44e0),
	ROOT_GATE(IMX8MQ_CLK_USB1_PHY_ROOT, "usb1_phy_root_clk", "usb_phy_ref", 0x44f0),
	ROOT_GATE(IMX8MQ_CLK_USB2_PHY_ROOT, "usb2_phy_root_clk", "usb_phy_ref", 0x4500),

	COMPOSITE(IMX8MQ_CLK_I2C1, "i2c1", i2c_p, 0xad00, 0),
	COMPOSITE(IMX8MQ_CLK_I2C2, "i2c2", i2c_p, 0xad80, 0),
	COMPOSITE(IMX8MQ_CLK_I2C3, "i2c3", i2c_p, 0xae00, 0),
	COMPOSITE(IMX8MQ_CLK_I2C4, "i2c4", i2c_p, 0xae80, 0),

	ROOT_GATE(IMX8MQ_CLK_I2C1_ROOT, "i2c1_root_clk", "i2c1", 0x4170),
	ROOT_GATE(IMX8MQ_CLK_I2C2_ROOT, "i2c2_root_clk", "i2c2", 0x4180),
	ROOT_GATE(IMX8MQ_CLK_I2C3_ROOT, "i2c3_root_clk", "i2c3", 0x4190),
	ROOT_GATE(IMX8MQ_CLK_I2C4_ROOT, "i2c4_root_clk", "i2c4", 0x41a0),

	ROOT_GATE(IMX8MQ_CLK_GPIO1_ROOT, "gpio1_root_clk", "ipg_root", 0x40b0),
	ROOT_GATE(IMX8MQ_CLK_GPIO2_ROOT, "gpio2_root_clk", "ipg_root", 0x40c0),
	ROOT_GATE(IMX8MQ_CLK_GPIO3_ROOT, "gpio3_root_clk", "ipg_root", 0x40d0),
	ROOT_GATE(IMX8MQ_CLK_GPIO4_ROOT, "gpio4_root_clk", "ipg_root", 0x40e0),
	ROOT_GATE(IMX8MQ_CLK_GPIO5_ROOT, "gpio5_root_clk", "ipg_root", 0x40f0),
};

struct ccm_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct clkdom		*clkdom;
	struct mtx		mtx;
	struct imx_clk		*clks;
	int			nclks;
};

static int
imx8mq_ccm_attach(device_t dev)
{
	struct imx_ccm_softc *sc;

	sc = device_get_softc(dev);
	sc->dev = dev;

	sc->clks = imx8mq_clks;
	sc->nclks = nitems(imx8mq_clks);

	return (imx_ccm_attach(dev));
}

static int
imx8mq_ccm_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (ofw_bus_is_compatible(dev, "fsl,imx8mq-ccm") == 0)
		return (ENXIO);

	device_set_desc(dev, "Freescale i.MX 8M Quad Clock Control Module");

	return (BUS_PROBE_DEFAULT);
}

static device_method_t imx8mq_ccm_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,  imx8mq_ccm_probe),
	DEVMETHOD(device_attach, imx8mq_ccm_attach),

	DEVMETHOD_END
};

DEFINE_CLASS_1(imx8mq_ccm, imx8mq_ccm_driver, imx8mq_ccm_methods,
    sizeof(struct imx_ccm_softc), imx_ccm_driver);

EARLY_DRIVER_MODULE(imx8mq_ccm, simplebus, imx8mq_ccm_driver, 0, 0,
    BUS_PASS_CPU + BUS_PASS_ORDER_EARLY);
