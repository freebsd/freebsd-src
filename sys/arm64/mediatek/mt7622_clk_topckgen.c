/*-
 * Copyright (c) 2025 Martin Filla, Michal Meloun <mmel@FreeBSD.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <dev/fdt/simplebus.h>
#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/ofw_bus_subr.h>

#include <dt-bindings/clock/mt7622-clk.h>
#include <dev/clk/clk_fixed.h>
#include <dev/clk/clk_div.h>
#include <dev/clk/clk_mux.h>
#include <dev/clk/clk_gate.h>
#include <dev/clk/clk_link.h>
#include <arm64/mediatek/mdtk_clk.h>
#include <dev/hwreset/hwreset.h>
#include "clkdev_if.h"

#define CLK_CFG_0	0x040
#define CLK_CFG_1   0x050
#define CLK_CFG_2   0x060
#define CLK_CFG_3   0x070
#define CLK_CFG_4   0x080
#define CLK_CFG_5   0x090
#define CLK_CFG_6   0x0A0
#define CLK_CFG_7   0x0B0
#define CLK_AUDDIV_0 0x120

static struct ofw_compat_data compat_data[] = {
    {"mediatek,mt7622-topckgen",	1},
    {NULL,		 	0},
    };

/* Parent lists */
PLIST(eth_ck_parents) = {
    "clkxtal",
    "syspll1_d2",
    "univpll1_d2",
    "syspll1_d4",
    "univpll_d5",
    "sgmiipll_d2",
    "univpll_d7",
    "dmpll_ck"};

PLIST(ddrphycfg_ck_parents) = {
    "clkxtal",
    "syspll1_d8"
};

PLIST(mem_ck_parents) = {
    "clkxtal",
    "dmpll_ck"
};

PLIST(axi_ck_parents) = {
    "clkxtal",
    "syspll1_d2",
    "syspll_d5",
    "syspll1_d4",
    "univpll_d5",
    "univpll2_d2",
    "univpll_d7",
    "dmpll_ck"
};

PLIST(pwm_ck_parents) = {
    "clkxtal",
    "univpll2_d4",
    "univpll3_d2",
    "univpll1_d4"
};

PLIST(f10m_ref_ck_parents) = {
    "clkxtal",
    "syspll4_d16"
};

PLIST(nfi_infra_ck_parents) = {
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "clkxtal",
    "univpll2_d8",
    "syspll1_d8",
    "univpll1_d8",
    "syspll4_d2",
    "univpll2_d4",
    "univpll3_d2",
    "syspll1_d4",
    "syspll_d7"
};

PLIST(flash_ck_parents) = {
    "clkxtal",
    "univpll_div80_d4",
    "syspll2_d8",
    "syspll3_d4",
    "univpll3_d4",
    "univpll1_d8",
    "syspll2_d4",
    "univpll2_d4"
};

PLIST(uart_ck_parents) = {
    "clkxtal",
    "univpll2_d8"
};

PLIST(spi0_ck_parents) = {
    "clkxtal",
    "syspll3_d2",
    "clkxtal",
    "syspll2_d4",
    "syspll4_d2",
    "univpll2_d4",
    "univpll1_d8",
    "clkxtal"
};

PLIST(spi1_ck_parents) = {
    "clkxtal",
    "syspll3_d2",
    "clkxtal",
    "syspll2_d4",
    "syspll4_d2",
    "univpll2_d4",
    "univpll1_d8",
    "clkxtal"
};

PLIST(msdc50_0_ck_parents) = {
    "clkxtal",
    "univpll2_d8",
    "univpll2_d4",
    "syspll_d7",
    "univpll2_d2",
    "univpll_d5",
    "univpll1_d2",
    "univpll_d3"
};

PLIST(msdc30_0_ck_parents) = {
    "clkxtal",
    "univpll2_d16",
    "univ48m_ck",
    "syspll2_d4",
    "univpll2_d4",
    "syspll_d7",
    "syspll2_d2",
    "univpll2_d2"
};

PLIST(msdc30_1_ck_parents) = {
    "clkxtal",
    "univpll2_d16",
    "univ48m_ck",
    "syspll2_d4",
    "univpll2_d4",
    "syspll_d7",
    "syspll2_d2",
    "univpll2_d2"
};

PLIST(a1sys_hp_ck_parents) = {
    "clkxtal",
    "aud1pll_ck",
    "aud2pll_ck",
    "clkxtal"
};

PLIST(a2sys_hp_ck_parents) = {
    "clkxtal",
    "aud1pll_ck",
    "aud2pll_ck",
    "clkxtal"
};

PLIST(intdir_ck_parents) = {
    "clkxtal",
    "syspll_d2",
    "univpll_d2",
    "sgmiipll_ck"
};

PLIST(aud_intbus_ck_parents) = {
    "clkxtal",
    "syspll1_d4",
    "syspll4_d2",
    "syspll3_d2"
};

PLIST(pmicspi_ck_parents) = {
    "clkxtal",
    "syspll1_d8",
    "syspll3_d4",
    "syspll1_d16",
    "univpll3_d4",
    "univpll2_d16",
    "dmpll_d8"
};

PLIST(scp_ck_parents) = {
    "clkxtal",
    "syspll1_d8",
    "univpll2_d2",
    "univpll2_d4"
};

PLIST(atb_ck_parents) = {
    "clkxtal",
    "syspll1_d2",
    "syspll_d5",
    "dmpll_ck"
};

PLIST(hif_ck_parents) = {
    "clkxtal",
    "syspll1_d2",
    "univpll1_d2",
    "syspll1_d4",
    "univpll_d5",
    "sgmiipll_d2",
    "univpll_d7",
    "dmpll_ck"
};

PLIST(audio_ck_parents) = {
    "clkxtal",
    "syspll3_d4",
    "syspll4_d4",
    "univpll1_d16"
};

PLIST(usb20_ck_parents) = {
    "clkxtal",
    "univpll3_d4",
    "syspll1_d8",
    "clkxtal"
};

PLIST(aud1_ck_parents) = {
    "clkxtal",
    "aud1pll_ck"
};

PLIST(aud2_ck_parents) = {
    "clkxtal",
    "aud2pll_ck"
};

PLIST(irrx_ck_parents) = {
    "clkxtal",
    "syspll4_d16"
};

PLIST(irtx_ck_parents) = {
    "clkxtal",
    "syspll4_d16"
};

PLIST(asm_l_ck_parents) = {
    "clkxtal",
    "syspll_d5",
    "univpll2_d2",
    "univpll2_d4"
};

PLIST(asm_m_ck_parents) = {
    "clkxtal",
    "syspll_d5",
    "univpll2_d2",
    "univpll2_d4"
};

PLIST(asm_h_ck_parents) = {
    "clkxtal",
    "syspll_d5",
    "univpll2_d2",
    "univpll2_d4"
};

PLIST(apll_ck_parents) = {
    "aud1_sel",
    "aud2_sel"
};

PLIST(apll2_ck_parents) = {
    "aud2_sel",
    "aud1_sel"
};

static struct clk_fixed_def fixed_clk[] = {
    FRATE(CLK_TOP_TO_U2_PHY, "to_u2_phy", 31250000),
    FRATE(CLK_TOP_TO_U2_PHY_1P, "to_u2_phy_1p", 31250000),
    FRATE(CLK_TOP_PCIE0_PIPE_EN, "pcie0_pipe_en", 125000000),
    FRATE(CLK_TOP_PCIE1_PIPE_EN, "pcie1_pipe_en", 125000000),
    FRATE(CLK_TOP_SSUSB_TX250M, "ssusb_tx250m", 250000000),
    FRATE(CLK_TOP_SSUSB_EQ_RX250M, "ssusb_eq_rx250m", 250000000),
    FRATE(CLK_TOP_SSUSB_CDR_REF, "ssusb_cdr_ref", 33333333),
    FRATE(CLK_TOP_SSUSB_CDR_FB,	"ssusb_cdr_fb", 50000000),
    FRATE(CLK_TOP_SATA_ASIC, "sata_asic", 50000000),
    FRATE(CLK_TOP_SATA_RBC, "sata_rbc", 50000000),
    /*FRATE(CLK_APMIXED_ARMPLL,   "armpll",    120000000ULL),
    FRATE(CLK_APMIXED_MAINPLL,  "mainpll",  1120000000ULL),
    FRATE(CLK_APMIXED_UNIV2PLL, "univ2pll", 2400000000ULL),
    FRATE(CLK_APMIXED_ETH1PLL,  "eth1pll",   500000000ULL),
    FRATE(CLK_APMIXED_ETH2PLL,  "eth2pll",   650000000ULL),
    FRATE(CLK_APMIXED_AUD1PLL,  "aud1pll",   147456000ULL),
    FRATE(CLK_APMIXED_AUD2PLL,  "aud2pll",   135475000ULL),
    FRATE(CLK_APMIXED_TRGPLL,   "trgpll",    725000000ULL),
    FRATE(CLK_APMIXED_SGMIPLL,  "sgmipll",   650000000ULL),*/

    FFACT(CLK_TOP_TO_USB3_SYS, "to_usb3_sys", "eth1pll", 1, 4),
    FFACT(CLK_TOP_P1_1MHZ, "p1_1mhz", "eth1pll", 1, 500),
    FFACT(CLK_TOP_4MHZ, "free_run_4mhz", "eth1pll", 1, 125),
    FFACT(CLK_TOP_P0_1MHZ, "p0_1mhz", "eth1pll", 1, 500),
    FFACT(CLK_TOP_TXCLK_SRC_PRE, "txclk_src_pre", "sgmiipll_d2", 1, 1),
    FFACT(CLK_TOP_RTC, "rtc", "clkxtal", 1, 1024),
    FFACT(CLK_TOP_MEMPLL, "mempll", "clkxtal", 32, 1),
    FFACT(CLK_TOP_DMPLL, "dmpll_ck", "mempll", 1, 1),
    FFACT(CLK_TOP_SYSPLL_D2, "syspll_d2", "mainpll", 1, 2),
    FFACT(CLK_TOP_SYSPLL1_D2, "syspll1_d2", "mainpll", 1, 4),
    FFACT(CLK_TOP_SYSPLL1_D4, "syspll1_d4", "mainpll", 1, 8),
    FFACT(CLK_TOP_SYSPLL1_D8, "syspll1_d8", "mainpll", 1, 16),
    FFACT(CLK_TOP_SYSPLL2_D4, "syspll2_d4", "mainpll", 1, 12),
    FFACT(CLK_TOP_SYSPLL2_D8, "syspll2_d8", "mainpll", 1, 24),
    FFACT(CLK_TOP_SYSPLL_D5, "syspll_d5", "mainpll", 1, 5),
    FFACT(CLK_TOP_SYSPLL3_D2, "syspll3_d2", "mainpll", 1, 10),
    FFACT(CLK_TOP_SYSPLL3_D4, "syspll3_d4", "mainpll", 1, 20),
    FFACT(CLK_TOP_SYSPLL4_D2, "syspll4_d2", "mainpll", 1, 14),
    FFACT(CLK_TOP_SYSPLL4_D4, "syspll4_d4", "mainpll", 1, 28),
    FFACT(CLK_TOP_SYSPLL4_D16, "syspll4_d16", "mainpll", 1, 112),
    FFACT(CLK_TOP_UNIVPLL, "univpll", "univ2pll", 1, 2),
    FFACT(CLK_TOP_UNIVPLL_D2, "univpll_d2", "univpll", 1, 2),
    FFACT(CLK_TOP_UNIVPLL1_D2, "univpll1_d2", "univpll", 1, 4),
    FFACT(CLK_TOP_UNIVPLL1_D4, "univpll1_d4", "univpll", 1, 8),
    FFACT(CLK_TOP_UNIVPLL1_D8, "univpll1_d8", "univpll", 1, 16),
    FFACT(CLK_TOP_UNIVPLL1_D16, "univpll1_d16", "univpll", 1, 32),
    FFACT(CLK_TOP_UNIVPLL2_D2, "univpll2_d2", "univpll", 1, 6),
    FFACT(CLK_TOP_UNIVPLL2_D4, "univpll2_d4", "univpll", 1, 12),
    FFACT(CLK_TOP_UNIVPLL2_D8, "univpll2_d8", "univpll", 1, 24),
    FFACT(CLK_TOP_UNIVPLL2_D16, "univpll2_d16", "univpll", 1, 48),
    FFACT(CLK_TOP_UNIVPLL_D5, "univpll_d5", "univpll", 1, 5),
    FFACT(CLK_TOP_UNIVPLL3_D2, "univpll3_d2", "univpll", 1, 10),
    FFACT(CLK_TOP_UNIVPLL3_D4, "univpll3_d4", "univpll", 1, 20),
    FFACT(CLK_TOP_UNIVPLL3_D16, "univpll3_d16", "univpll", 1, 80),
    FFACT(CLK_TOP_UNIVPLL_D7, "univpll_d7", "univpll", 1, 7),
    FFACT(CLK_TOP_UNIVPLL_D80_D4, "univpll_div80_d4", "univpll", 1, 320),
    FFACT(CLK_TOP_SGMIIPLL, "sgmiipll_ck", "sgmipll", 1, 1),
    FFACT(CLK_TOP_SGMIIPLL_D2, "sgmiipll_d2", "sgmipll", 1, 2),

    FFACT(/*CLK_TOP_SYSPLL_D7*/ 0, "syspll_d7", "mainpll", 1, 8),
    FFACT(/*CLK_TOP_SYSPLL2_D2 */0, "syspll2_d2", "mainpll", 1, 6),
    FFACT(/*CLK_TOP_UNIVPLL_D3*/ 0, "univpll_d3", "univpll", 1, 3),
    FFACT(/*CLK_TOP_SYSPLL1_D16*/ 0, "syspll1_d16", "syspll_d2", 1, 16),
    FFACT(/*CLK_TOP_DMPLL_D8*/ 0, "dmpll_d8", "dmpll_ck", 1, 8),
    FFACT(CLK_TOP_AUD1PLL, "aud1pll_ck", "aud1pll", 1, 1),
    FFACT(CLK_TOP_AUD2PLL, "aud2pll_ck", "aud2pll", 1, 1),
    FFACT(CLK_TOP_UNIV48M, "univ48m_ck", "univpll", 1, 25),
    FFACT(CLK_TOP_TO_USB3_REF, "to_usb3_ref", "univpll2_d4", 1, 4),
    FFACT(CLK_TOP_PCIE1_MAC_EN, "pcie1_mac_en", "univpll1_d4", 1, 1),
    FFACT(CLK_TOP_PCIE0_MAC_EN, "pcie0_mac_en", "univpll1_d4", 1, 1),
    FFACT(CLK_TOP_ETH_500M, "eth_500m", "eth1pll", 1, 1),
    FFACT(CLK_TOP_AUD_I2S2_MCK, "aud_i2s2_mck", "i2s2_mck_sel", 1, 2),
};

static struct clk_gate_def gates_clk[] = {
	I_GATE(CLK_TOP_ETH_SEL, "eth_sel", "eth_sel_mux", CLK_CFG_0, 31),
	I_GATE(CLK_TOP_DDRPHYCFG_SEL, "ddrphycfg_sel", "ddrphycfg_sel_mux", CLK_CFG_0, 23),
	I_GATE(CLK_TOP_MEM_SEL, "mem_sel", "mem_sel_mux", CLK_CFG_0, 15),
	I_GATE(CLK_TOP_AXI_SEL, "axi_sel", "axi_sel_mux", CLK_CFG_0, 7),

	I_GATE(CLK_TOP_PWM_SEL, "pwm_sel", "pwm_sel_mux", CLK_CFG_1, 7),
	I_GATE(CLK_TOP_F10M_REF_SEL, "f10m_ref_sel", "f10m_ref_sel_mux", CLK_CFG_1, 15),
	I_GATE(CLK_TOP_NFI_INFRA_SEL, "spinfi_infra_bclk_sel", "spinfi_infra_bclk_sel_mux", CLK_CFG_1, 23),
	I_GATE(CLK_TOP_FLASH_SEL, "flash_sel", "flash_sel_mux", CLK_CFG_1, 31),

	I_GATE(CLK_TOP_UART_SEL, "uart_sel", "uart_sel_mux", CLK_CFG_2, 7),
	I_GATE(CLK_TOP_SPI0_SEL, "spi0_sel", "spi0_sel_mux", CLK_CFG_2, 15),
	I_GATE(CLK_TOP_SPI1_SEL, "spi1_sel", "spi1_sel_mux", CLK_CFG_2, 23),
	I_GATE(CLK_TOP_MSDC50_0_SEL, "msdc50_0_sel", "msdc50_0_sel_mux", CLK_CFG_2, 31),

	I_GATE(CLK_TOP_MSDC30_0_SEL, "msdc30_0_sel", "msdc30_0_sel_mux", CLK_CFG_3, 7),
	I_GATE(CLK_TOP_MSDC30_1_SEL, "msdc30_1_sel", "msdc30_1_sel_mux", CLK_CFG_3, 15),
	I_GATE(CLK_TOP_A1SYS_HP_SEL, "a1sys_hp_sel", "a1sys_hp_sel_mux", CLK_CFG_3, 23),
	I_GATE(CLK_TOP_A2SYS_HP_SEL, "a2sys_hp_sel", "a2sys_hp_sel_mux", CLK_CFG_3, 31),

	I_GATE(CLK_TOP_INTDIR_SEL, "intdir_sel", "intdir_sel_mux", CLK_CFG_4, 7),
	I_GATE(CLK_TOP_AUD_INTBUS_SEL, "aud_intbus_sel", "aud_intbus_sel_mux", CLK_CFG_4, 15),
	I_GATE(CLK_TOP_PMICSPI_SEL, "pmicspi_sel", "pmicspi_sel_mux", CLK_CFG_4, 23),
	I_GATE(CLK_TOP_SCP_SEL, "scp_sel", "scp_sel_mux", CLK_CFG_4, 31),

	I_GATE(CLK_TOP_ATB_SEL, "atb_sel", "atb_sel_mux", CLK_CFG_5, 7),
	I_GATE(CLK_TOP_HIF_SEL, "hif_sel", "hif_sel_mux", CLK_CFG_5, 15),
	I_GATE(CLK_TOP_AUDIO_SEL, "audio_sel", "audio_sel_mux", CLK_CFG_5, 23),
	I_GATE(CLK_TOP_AUDIO_SEL, "usb20_sel", "usb20_sel_mux", CLK_CFG_5, 31),

	I_GATE(CLK_TOP_AUD1_SEL, "aud1_sel", "aud1_sel_mux", CLK_CFG_6, 7),
	I_GATE(CLK_TOP_AUD2_SEL, "aud2_sel", "aud2_sel_mux", CLK_CFG_6, 15),
	I_GATE(CLK_TOP_IRRX_SEL, "irrx_sel_sel", "irrx_sel_mux", CLK_CFG_6, 23),
	I_GATE(CLK_TOP_IRTX_SEL, "irtx_sel", "irtx_sel_mux", CLK_CFG_6, 31),

	I_GATE(CLK_TOP_ASM_L_SEL, "asm_l_sel", "asm_l_sel_mux", CLK_CFG_6, 7),
	I_GATE(CLK_TOP_ASM_M_SEL, "asm_m_sel", "asm_m_sel_mux", CLK_CFG_6, 15),
	I_GATE(CLK_TOP_ASM_H_SEL, "asm_h_sel", "asm_h_sel_mux", CLK_CFG_6, 23),

	I_GATE(CLK_TOP_APLL1_SEL, "apll1_ck_sel", "apll1_ck_div", CLK_AUDDIV_0, 0),
	I_GATE(CLK_TOP_APLL2_SEL, "apll2_ck_sel", "apll2_ck_div", CLK_AUDDIV_0, 0),
	I_GATE(CLK_TOP_I2S0_MCK_SEL, "i2s0_mck_sel", "i2s0_mck_div", CLK_AUDDIV_0, 0),
	I_GATE(CLK_TOP_I2S1_MCK_SEL, "i2s1_mck_sel", "i2s1_mck_div", CLK_AUDDIV_0, 0),
	I_GATE(CLK_TOP_I2S2_MCK_SEL, "i2s2_mck_sel", "i2s2_mck_div", CLK_AUDDIV_0, 0),
	I_GATE(CLK_TOP_I2S3_MCK_SEL, "i2s3_mck_sel", "i2s3_mck_div", CLK_AUDDIV_0, 0),
};

static struct clk_mux_def muxes_clk[] = {
    MUX0(0, "eth_sel_mux", eth_ck_parents, CLK_CFG_0, 24, 3),
    MUX0(0, "ddrphycfg_sel_mux", ddrphycfg_ck_parents, CLK_CFG_0, 16, 1),
    MUX0(0, "mem_sel_mux", mem_ck_parents, CLK_CFG_0, 8, 1),
    MUX0(0, "axi_sel_mux", axi_ck_parents, CLK_CFG_0, 0, 3),

    MUX0(0, "pwm_sel_mux", pwm_ck_parents, CLK_CFG_1, 0, 2),
    MUX0(0, "f10m_ref_sel_mux", f10m_ref_ck_parents, CLK_CFG_1, 8, 1),
    MUX0(0, "spinfi_infra_bclk_sel_mux", nfi_infra_ck_parents, CLK_CFG_1, 16, 4),
    MUX0(0, "flash_sel_mux", flash_ck_parents, CLK_CFG_1, 24, 3),

    MUX0(0, "uart_sel_mux", uart_ck_parents, CLK_CFG_2, 0, 1),
    MUX0(0, "spi0_sel_mux", spi0_ck_parents, CLK_CFG_2, 8, 3),
    MUX0(0, "spi1_sel_mux", spi1_ck_parents, CLK_CFG_2, 16, 3),
    MUX0(0, "msdc50_0_sel_mux", msdc50_0_ck_parents, CLK_CFG_2, 24, 3),

    MUX0(0, "msdc30_0_sel_mux", msdc30_0_ck_parents, CLK_CFG_3, 0, 3),
    MUX0(0, "msdc30_1_sel_mux", msdc30_1_ck_parents, CLK_CFG_3, 8, 3),
    MUX0(0, "a1sys_hp_sel_mux", a1sys_hp_ck_parents, CLK_CFG_3, 16, 2),
    MUX0(0, "a2sys_hp_sel_mux", a2sys_hp_ck_parents, CLK_CFG_3, 24, 2),

    MUX0(0, "intdir_sel_mux", intdir_ck_parents, CLK_CFG_4, 0, 2),
    MUX0(0, "aud_intbus_sel_mux", aud_intbus_ck_parents, CLK_CFG_4, 8, 2),
    MUX0(0, "pmicspi_sel_mux", pmicspi_ck_parents, CLK_CFG_4, 16, 3),
    MUX0(0, "scp_sel_mux", scp_ck_parents, CLK_CFG_4, 24, 2),

    MUX0(0, "atb_sel_mux", atb_ck_parents, CLK_CFG_5, 0, 2),
    MUX0(0, "hif_sel_mux", hif_ck_parents, CLK_CFG_5, 8, 3),
    MUX0(0, "audio_sel_mux", audio_ck_parents, CLK_CFG_5, 8, 3),
    MUX0(0, "usb20_sel_mux", usb20_ck_parents, CLK_CFG_5, 24, 2),

    MUX0(0, "aud1_sel_mux", aud1_ck_parents, CLK_CFG_6, 0, 1),
    MUX0(0, "aud2_sel_mux", aud2_ck_parents, CLK_CFG_6, 8, 1),
    MUX0(0, "irrx_sel_mux", irrx_ck_parents, CLK_CFG_6, 16, 1),
    MUX0(0, "irtx_sel_mux", irtx_ck_parents, CLK_CFG_6, 24, 1),

    MUX0(0, "asm_l_sel_mux", asm_l_ck_parents, CLK_CFG_7, 0, 2),
    MUX0(0, "asm_m_sel_mux", asm_m_ck_parents, CLK_CFG_7, 8, 2),
    MUX0(0, "asm_h_sel_mux", asm_h_ck_parents, CLK_CFG_7, 16, 2),

    MUX0(0, "apll1_ck_mux", apll_ck_parents, CLK_AUDDIV_0, 6, 1),
    MUX0(0, "apll2_ck_mux", apll2_ck_parents, CLK_AUDDIV_0, 7, 1),
    MUX0(0, "i2s0_mck_mux", apll_ck_parents, CLK_AUDDIV_0, 8, 1),
    MUX0(0, "i2s1_mck_mux", apll_ck_parents, CLK_AUDDIV_0, 9, 1),
    MUX0(0, "i2s2_mck_mux", apll_ck_parents, CLK_AUDDIV_0, 10, 1),
    MUX0(0, "i2s3_mck_mux", apll_ck_parents, CLK_AUDDIV_0, 11, 1),
};

static struct clk_div_def dived_clk[] = {
    DIV(CLK_TOP_APLL1_DIV, "apll1_ck_div", "apll1_ck_mux",
        0x120, 24, 3),
    DIV(CLK_TOP_APLL2_DIV, "apll2_ck_div", "apll2_ck_mux",
        0x120, 28, 3),
    DIV(CLK_TOP_I2S0_MCK_DIV, "i2s0_mck_div", "i2s0_mck_mux",
        0x124, 0, 7),
    DIV(CLK_TOP_I2S1_MCK_DIV, "i2s1_mck_div", "i2s1_mck_mux",
        0x124, 8, 7),
    DIV(CLK_TOP_I2S2_MCK_DIV, "i2s2_mck_div", "i2s2_mck_mux",
        0x124, 16, 7),
    DIV(CLK_TOP_I2S3_MCK_DIV, "i2s3_mck_div", "i2s3_mck_mux",
        0x124, 24, 7),
    DIV(CLK_TOP_A1SYS_HP_DIV, "a1sys_div", "a1sys_hp_sel",
        0x128, 8, 7),
    DIV(CLK_TOP_A2SYS_HP_DIV, "a2sys_div", "a2sys_hp_sel",
        0x128, 24, 7),
};

static struct mdtk_clk_def clk_def = {
    .linked_def = NULL,
    .num_linked = 0,
    .fixed_def = fixed_clk,
    .num_fixed = nitems(fixed_clk),
    .gates_def = gates_clk,
    .num_gates = nitems(gates_clk),
    .dived_def = dived_clk,
    .num_dived = nitems(dived_clk),
    .muxes_def = muxes_clk,
    .num_muxes = nitems(muxes_clk),
};

static int
topckgen_clk_detach(device_t dev)
{
    device_printf(dev, "Error: Clock driver cannot be detached\n");
    return (EBUSY);
}

static int
topckgen_clk_probe(device_t dev)
{
    if (!ofw_bus_status_okay(dev))
        return (ENXIO);

    if (ofw_bus_search_compatible(dev, compat_data)->ocd_data != 0) {
        device_set_desc(dev, "Mediatek Topckgen clocks");
        return (BUS_PROBE_DEFAULT);
    }

    return (ENXIO);
}

static int
topckgen_clk_attach(device_t dev) {
    struct mdtk_clk_softc *sc = device_get_softc(dev);
    int rid, rv;

    sc->dev = dev;

    mtx_init(&sc->mtx, device_get_nameunit(dev), NULL, MTX_DEF);

    /* Resource setup. */
    rid = 0;
    sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
                                         RF_ACTIVE);
    if (!sc->mem_res) {
        device_printf(dev, "cannot allocate memory resource\n");
        rv = ENXIO;
        goto fail;
    }

    mdtk_register_clocks(dev,  &clk_def);
    return (0);

fail:
    if (sc->mem_res)
        bus_release_resource(dev, SYS_RES_MEMORY, 0, sc->mem_res);

    return (rv);
}

static device_method_t mdtk_mt7622_topckgen_methods[] = {
    /* Device interface */
    DEVMETHOD(device_probe,		 topckgen_clk_probe),
    DEVMETHOD(device_attach,	     topckgen_clk_attach),
    DEVMETHOD(device_detach, 	 topckgen_clk_detach),

    /* Clkdev interface*/
    DEVMETHOD(clkdev_read_4,	mdtk_clkdev_read_4),
    DEVMETHOD(clkdev_write_4,	mdtk_clkdev_write_4),
    DEVMETHOD(clkdev_modify_4,	mdtk_clkdev_modify_4),
    DEVMETHOD(clkdev_device_lock,	mdtk_clkdev_device_lock),
    DEVMETHOD(clkdev_device_unlock,	mdtk_clkdev_device_unlock),

    DEVMETHOD_END
};

DEFINE_CLASS_0(mdtk_mt7622_topckgen, mdtk_mt7622_topckgen_driver, mdtk_mt7622_topckgen_methods,
               sizeof(struct mdtk_clk_softc));

EARLY_DRIVER_MODULE(mdtk_mt7622_topckgen, simplebus, mdtk_mt7622_topckgen_driver, NULL, NULL,
                    BUS_PASS_BUS + BUS_PASS_ORDER_MIDDLE + 2);

