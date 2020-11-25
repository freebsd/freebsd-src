/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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

#include <arm/allwinner/clkng/aw_ccung.h>

#include <gnu/dts/include/dt-bindings/clock/sun50i-h6-ccu.h>
#include <gnu/dts/include/dt-bindings/reset/sun50i-h6-ccu.h>

/* Non-exported clocks */
#define	CLK_OSC_12M		0
#define	CLK_PLL_CPUX		1
#define	CLK_PLL_DDR0		2
#define	CLK_PLL_PERIPH0_2X	4
#define	CLK_PLL_PERIPH0_4X	5
#define	CLK_PLL_PERIPH1		6
#define	CLK_PLL_PERIPH1_2X	7
#define	CLK_PLL_PERIPH1_4X	8
#define	CLK_PLL_GPU		9
#define	CLK_PLL_VIDEO0		10
#define	CLK_PLL_VIDEO0_4X	11
#define	CLK_PLL_VIDEO1		12
#define	CLK_PLL_VIDEO1_4X	13
#define	CLK_PLL_VE		14
#define	CLK_PLL_DE		14
#define	CLK_PLL_HSIC		16

#define	CLK_PSI_AHB1_AHB2	24
#define	CLK_AHB3		25
#define	CLK_APB2		27

static struct aw_ccung_reset h6_ccu_resets[] = {
	/* PSI_BGR_REG */
	CCU_RESET(RST_BUS_PSI, 0x79c, 16)

	/* SMHC_BGR_REG */
	CCU_RESET(RST_BUS_MMC0, 0x84c, 16)
	CCU_RESET(RST_BUS_MMC1, 0x84c, 17)
	CCU_RESET(RST_BUS_MMC2, 0x84c, 18)

	/* UART_BGR_REG */
	CCU_RESET(RST_BUS_UART0, 0x90c, 16)
	CCU_RESET(RST_BUS_UART1, 0x90c, 17)
	CCU_RESET(RST_BUS_UART2, 0x90c, 18)
	CCU_RESET(RST_BUS_UART3, 0x90c, 19)

	/* TWI_BGR_REG */
	CCU_RESET(RST_BUS_I2C0, 0x91c, 16)
	CCU_RESET(RST_BUS_I2C1, 0x91c, 17)
	CCU_RESET(RST_BUS_I2C2, 0x91c, 18)
	CCU_RESET(RST_BUS_I2C3, 0x91c, 19)

	/* EMAC_BGR_REG */
	CCU_RESET(RST_BUS_EMAC, 0x97c, 16)

	/* USB0_CLK_REG */
	CCU_RESET(RST_USB_PHY0, 0xa70, 30)

	/* USB1_CLK_REG */
	CCU_RESET(RST_USB_PHY1, 0xa74, 30)

	/* USB3_CLK_REG */
	CCU_RESET(RST_USB_HSIC, 0xa7c, 28)
	CCU_RESET(RST_USB_PHY3, 0xa7c, 30)

	/* USB_BGR_REG */
	CCU_RESET(RST_BUS_OHCI0, 0xa8c, 16)
	CCU_RESET(RST_BUS_OHCI3, 0xa8c, 19)
	CCU_RESET(RST_BUS_EHCI0, 0xa8c, 20)
	CCU_RESET(RST_BUS_XHCI, 0xa8c, 21)
	CCU_RESET(RST_BUS_EHCI3, 0xa8c, 23)
	CCU_RESET(RST_BUS_OTG, 0xa8c, 24)
};

static struct aw_ccung_gate h6_ccu_gates[] = {
	/* PSI_BGR_REG */
	CCU_GATE(CLK_BUS_PSI, "bus-psi", "psi_ahb1_ahb2", 0x79c, 0)

	/* SMHC_BGR_REG */
	CCU_GATE(CLK_BUS_MMC0, "bus-mmc0", "ahb3", 0x84c, 0)
	CCU_GATE(CLK_BUS_MMC1, "bus-mmc1", "ahb3", 0x84c, 1)
	CCU_GATE(CLK_BUS_MMC2, "bus-mmc2", "ahb3", 0x84c, 2)

	/* UART_BGR_REG Enabling the gate enable weir behavior ... */
	/* CCU_GATE(CLK_BUS_UART0, "bus-uart0", "apb2", 0x90c, 0) */
	/* CCU_GATE(CLK_BUS_UART1, "bus-uart1", "apb2", 0x90c, 1) */
	/* CCU_GATE(CLK_BUS_UART2, "bus-uart2", "apb2", 0x90c, 2) */
	/* CCU_GATE(CLK_BUS_UART3, "bus-uart3", "apb2", 0x90c, 3) */

	/* TWI_BGR_REG */
	CCU_GATE(CLK_BUS_I2C0, "bus-i2c0", "apb2", 0x91c, 0)
	CCU_GATE(CLK_BUS_I2C1, "bus-i2c1", "apb2", 0x91c, 1)
	CCU_GATE(CLK_BUS_I2C2, "bus-i2c2", "apb2", 0x91c, 2)
	CCU_GATE(CLK_BUS_I2C3, "bus-i2c3", "apb2", 0x91c, 3)

	/* EMAC_BGR_REG */
	CCU_GATE(CLK_BUS_EMAC, "bus-emac", "ahb3", 0x97c, 0)

	/* USB0_CLK_REG */
	CCU_GATE(CLK_USB_PHY0, "usb-phy0", "ahb3", 0xa70, 29)
	CCU_GATE(CLK_USB_OHCI0, "usb-ohci0", "ahb3", 0xa70, 31)

	/* USB1_CLK_REG */
	CCU_GATE(CLK_USB_PHY1, "usb-phy1", "ahb3", 0xa74, 29)

	/* USB3_CLK_REG */
	CCU_GATE(CLK_USB_HSIC, "usb-hsic", "ahb3", 0xa7c, 26)
	CCU_GATE(CLK_USB_HSIC_12M, "usb-hsic-12M", "ahb3", 0xa7c, 27)
	CCU_GATE(CLK_USB_PHY3, "usb-phy3", "ahb3", 0xa7c, 29)
	CCU_GATE(CLK_USB_OHCI3, "usb-ohci3", "ahb3", 0xa7c, 31)

	/* USB_BGR_REG */
	CCU_GATE(CLK_BUS_OHCI0, "bus-ohci0", "ahb3", 0xa8c, 0)
	CCU_GATE(CLK_BUS_OHCI3, "bus-ohci3", "ahb3", 0xa8c, 3)
	CCU_GATE(CLK_BUS_EHCI0, "bus-ehci0", "ahb3", 0xa8c, 4)
	CCU_GATE(CLK_BUS_XHCI, "bus-xhci", "ahb3", 0xa8c, 5)
	CCU_GATE(CLK_BUS_EHCI3, "bus-ehci3", "ahb3", 0xa8c, 7)
	CCU_GATE(CLK_BUS_OTG, "bus-otg", "ahb3", 0xa8c, 8)
};

static const char *osc12m_parents[] = {"osc24M"};
FIXED_CLK(osc12m_clk,
    CLK_OSC_12M,			/* id */
    "osc12M",				/* name */
    osc12m_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */

static const char *pll_cpux_parents[] = {"osc24M"};
NP_CLK(pll_cpux_clk,
    CLK_PLL_CPUX,				/* id */
    "pll_cpux", pll_cpux_parents,		/* name, parents */
    0x00,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 2, 0, 0,					/* p factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_ddr0_parents[] = {"osc24M"};
NMM_CLK(pll_ddr0_clk,
    CLK_PLL_DDR0,				/* id */
    "pll_ddr0", pll_ddr0_parents,		/* name, parents */
    0x10,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_peri0_4x_parents[] = {"osc24M"};
NMM_CLK(pll_peri0_4x_clk,
    CLK_PLL_PERIPH0_4X,				/* id */
    "pll_periph0_4x", pll_peri0_4x_parents,	/* name, parents */
    0x20,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_peri0_2x_parents[] = {"pll_periph0_4x"};
FIXED_CLK(pll_peri0_2x_clk,
    CLK_PLL_PERIPH0_2X,			/* id */
    "pll_periph0_2x",			/* name */
    pll_peri0_2x_parents,		/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */
static const char *pll_peri0_parents[] = {"pll_periph0_4x"};
FIXED_CLK(pll_peri0_clk,
    CLK_PLL_PERIPH0,			/* id */
    "pll_periph0",			/* name */
    pll_peri0_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_peri1_4x_parents[] = {"osc24M"};
NMM_CLK(pll_peri1_4x_clk,
    CLK_PLL_PERIPH1_4X,				/* id */
    "pll_periph1_4x", pll_peri1_4x_parents,	/* name, parents */
    0x28,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_peri1_2x_parents[] = {"pll_periph1_4x"};
FIXED_CLK(pll_peri1_2x_clk,
    CLK_PLL_PERIPH1_2X,			/* id */
    "pll_periph1_2x",			/* name */
    pll_peri1_2x_parents,		/* parent */
    0,					/* freq */
    1,					/* mult */
    2,					/* div */
    0);					/* flags */
static const char *pll_peri1_parents[] = {"pll_periph1_4x"};
FIXED_CLK(pll_peri1_clk,
    CLK_PLL_PERIPH1,			/* id */
    "pll_periph1",			/* name */
    pll_peri1_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_gpu_parents[] = {"osc24M"};
NMM_CLK(pll_gpu_clk,
    CLK_PLL_GPU,				/* id */
    "pll_gpu", pll_gpu_parents,			/* name, parents */
    0x30,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_video0_4x_parents[] = {"osc24M"};
NMM_CLK(pll_video0_4x_clk,
    CLK_PLL_VIDEO0_4X,				/* id */
    "pll_video0_4x", pll_video0_4x_parents,	/* name, parents */
    0x40,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_video0_parents[] = {"pll_video0_4x"};
FIXED_CLK(pll_video0_clk,
    CLK_PLL_VIDEO0,			/* id */
    "pll_video0",			/* name */
    pll_video0_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_video1_4x_parents[] = {"osc24M"};
NMM_CLK(pll_video1_4x_clk,
    CLK_PLL_VIDEO1_4X,				/* id */
    "pll_video1_4x", pll_video1_4x_parents,	/* name, parents */
    0x48,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */
static const char *pll_video1_parents[] = {"pll_video1_4x"};
FIXED_CLK(pll_video1_clk,
    CLK_PLL_VIDEO1,			/* id */
    "pll_video1",			/* name */
    pll_video1_parents,			/* parent */
    0,					/* freq */
    1,					/* mult */
    4,					/* div */
    0);					/* flags */

static const char *pll_ve_parents[] = {"osc24M"};
NMM_CLK(pll_ve_clk,
    CLK_PLL_VE,					/* id */
    "pll_ve", pll_ve_parents,			/* name, parents */
    0x58,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_de_parents[] = {"osc24M"};
NMM_CLK(pll_de_clk,
    CLK_PLL_DE,					/* id */
    "pll_de", pll_de_parents,			/* name, parents */
    0x60,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

static const char *pll_hsic_parents[] = {"osc24M"};
NMM_CLK(pll_hsic_clk,
    CLK_PLL_HSIC,				/* id */
    "pll_hsic", pll_hsic_parents,		/* name, parents */
    0x70,					/* offset */
    8, 7, 0, 0,					/* n factor */
    0, 1, 0, 0,					/* m0 factor */
    1, 1, 0, 0,					/* m1 factor */
    31,						/* gate */
    28, 1000,					/* lock */
    AW_CLK_HAS_GATE | AW_CLK_HAS_LOCK);		/* flags */

/* PLL_AUDIO missing */

/* CPUX_AXI missing */

static const char *psi_ahb1_ahb2_parents[] = {"osc24M", "osc32k", "iosc", "pll_periph0"};
NM_CLK(psi_ahb1_ahb2_clk,
    CLK_PSI_AHB1_AHB2, "psi_ahb1_ahb2", psi_ahb1_ahb2_parents,		/* id, name, parents */
    0x510,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *ahb3_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(ahb3_clk,
    CLK_AHB3, "ahb3", ahb3_parents,		/* id, name, parents */
    0x51C,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *apb1_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(apb1_clk,
    CLK_APB1, "apb1", apb1_parents,		/* id, name, parents */
    0x520,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

static const char *apb2_parents[] = {"osc24M", "osc32k", "psi_ahb1_ahb2", "pll_periph0"};
NM_CLK(apb2_clk,
    CLK_APB2, "apb2", apb2_parents,		/* id, name, parents */
    0x524,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 2, 0, 0,					/* m factor */
    24, 2,					/* mux */
    0,						/* gate */
    AW_CLK_HAS_MUX | AW_CLK_REPARENT);		/* flags */

/* Missing MBUS clock */

static const char *mod_parents[] = {"osc24M", "pll_periph0_2x", "pll_periph1_2x"};
NM_CLK(mmc0_clk,
    CLK_MMC0, "mmc0", mod_parents,		/* id, name, parents */
    0x830,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(mmc1_clk,
    CLK_MMC1, "mmc1", mod_parents,		/* id, name, parents */
    0x834,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

NM_CLK(mmc2_clk,
    CLK_MMC2, "mmc2", mod_parents,		/* id, name, parents */
    0x838,					/* offset */
    8, 2, 0, AW_CLK_FACTOR_POWER_OF_TWO,	/* n factor */
    0, 4, 0, 0,					/* m factor */
    24, 2,					/* mux */
    31,						/* gate */
    AW_CLK_HAS_GATE | AW_CLK_HAS_MUX |
    AW_CLK_REPARENT);				/* flags */

static struct aw_ccung_clk h6_ccu_clks[] = {
	{ .type = AW_CLK_NP, .clk.np = &pll_cpux_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_ddr0_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_peri0_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_peri1_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_gpu_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_video0_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_video1_4x_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_ve_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_de_clk},
	{ .type = AW_CLK_NMM, .clk.nmm = &pll_hsic_clk},

	{ .type = AW_CLK_NM, .clk.nm = &psi_ahb1_ahb2_clk},
	{ .type = AW_CLK_NM, .clk.nm = &ahb3_clk},
	{ .type = AW_CLK_NM, .clk.nm = &apb1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &apb2_clk},

	{ .type = AW_CLK_NM, .clk.nm = &mmc0_clk},
	{ .type = AW_CLK_NM, .clk.nm = &mmc1_clk},
	{ .type = AW_CLK_NM, .clk.nm = &mmc2_clk},

	{ .type = AW_CLK_FIXED, .clk.fixed = &osc12m_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri0_2x_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri0_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri1_2x_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_peri1_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_video0_clk},
	{ .type = AW_CLK_FIXED, .clk.fixed = &pll_video1_clk},
};

static int
ccu_h6_probe(device_t dev)
{

	if (!ofw_bus_status_okay(dev))
		return (ENXIO);

	if (!ofw_bus_is_compatible(dev, "allwinner,sun50i-h6-ccu"))
		return (ENXIO);

	device_set_desc(dev, "Allwinner H6 Clock Control Unit NG");
	return (BUS_PROBE_DEFAULT);
}

static int
ccu_h6_attach(device_t dev)
{
	struct aw_ccung_softc *sc;

	sc = device_get_softc(dev);

	sc->resets = h6_ccu_resets;
	sc->nresets = nitems(h6_ccu_resets);
	sc->gates = h6_ccu_gates;
	sc->ngates = nitems(h6_ccu_gates);
	sc->clks = h6_ccu_clks;
	sc->nclks = nitems(h6_ccu_clks);

	return (aw_ccung_attach(dev));
}

static device_method_t ccu_h6ng_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		ccu_h6_probe),
	DEVMETHOD(device_attach,	ccu_h6_attach),

	DEVMETHOD_END
};

static devclass_t ccu_h6ng_devclass;

DEFINE_CLASS_1(ccu_h6ng, ccu_h6ng_driver, ccu_h6ng_methods,
  sizeof(struct aw_ccung_softc), aw_ccung_driver);

EARLY_DRIVER_MODULE(ccu_h6ng, simplebus, ccu_h6ng_driver,
    ccu_h6ng_devclass, 0, 0, BUS_PASS_RESOURCE + BUS_PASS_ORDER_MIDDLE);
