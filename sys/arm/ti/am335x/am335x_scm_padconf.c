/*-
 * Copyright (c) 2012 Damjan Marion <dmarion@FreeBSD.org>
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/lock.h>
#include <sys/malloc.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <arm/ti/tivar.h>
#include <arm/ti/ti_scm.h>

#define _PIN(r, b, gp, gm, m0, m1, m2, m3, m4, m5, m6, m7) \
	{	.reg_off = r, \
		.gpio_pin = gp, \
		.gpio_mode = gm, \
		.ballname = b, \
		.muxmodes[0] = m0, \
		.muxmodes[1] = m1, \
		.muxmodes[2] = m2, \
		.muxmodes[3] = m3, \
		.muxmodes[4] = m4, \
		.muxmodes[5] = m5, \
		.muxmodes[6] = m6, \
		.muxmodes[7] = m7, \
	}

#define RXACTIVE	(0x01 << 5) /* Input enable value for the Pad */
#define PULLTYPESEL	(0x01 << 4) /* Pad pullup/pulldown type selection */
#define PULLUDEN	(0x01 << 3) /* Pullup/pulldown enabled */

const struct ti_scm_padstate ti_padstate_devmap[] = {
	{"output",		0 },
	{"output_pullup",	PULLTYPESEL },
	{"input",		RXACTIVE },
	{"input_pulldown",	RXACTIVE | PULLUDEN },
	{"input_pullup",	RXACTIVE | PULLUDEN | PULLTYPESEL },
	{"input_pullup_inact",	RXACTIVE | PULLTYPESEL },
	{ .state = NULL }
};

const struct ti_scm_padconf ti_padconf_devmap[] = {
	_PIN(0x800, "GPMC_AD0",		 32, 7,"gpmc_ad0", "mmc1_dat0", NULL, NULL, NULL, NULL, NULL, "gpio1_0"),
	_PIN(0x804, "GPMC_AD1",		 33, 7,"gpmc_ad1", "mmc1_dat1", NULL, NULL, NULL, NULL, NULL, "gpio1_1"),
	_PIN(0x808, "GPMC_AD2",		 34, 7,"gpmc_ad2", "mmc1_dat2", NULL, NULL, NULL, NULL, NULL, "gpio1_2"),
	_PIN(0x80C, "GPMC_AD3",		 35, 7,"gpmc_ad3", "mmc1_dat3", NULL, NULL, NULL, NULL, NULL, "gpio1_3"),
	_PIN(0x810, "GPMC_AD4",		 36, 7,"gpmc_ad4", "mmc1_dat4", NULL, NULL, NULL, NULL, NULL, "gpio1_4"),
	_PIN(0x814, "GPMC_AD5",		 37, 7,"gpmc_ad5", "mmc1_dat5", NULL, NULL, NULL, NULL, NULL, "gpio1_5"),
	_PIN(0x818, "GPMC_AD6",		 38, 7,"gpmc_ad6", "mmc1_dat6", NULL, NULL, NULL, NULL, NULL, "gpio1_6"),
	_PIN(0x81C, "GPMC_AD7",		 39, 7,"gpmc_ad7", "mmc1_dat7", NULL, NULL, NULL, NULL, NULL, "gpio1_7"),
#if 0 /* Incomplete Entries - fill with data from table 2-7 in datasheet */
	_PIN(0x820, "gpmc_ad8",		0, 0, "gpmc_ad8", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x824, "gpmc_ad9",		0, 0, "gpmc_ad9", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x828, "gpmc_ad10",	0, 0, "gpmc_ad10", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x82C, "gpmc_ad11",	0, 0, "gpmc_ad11", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x830, "gpmc_ad12",	0, 0, "gpmc_ad12", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x834, "gpmc_ad13",	0, 0, "gpmc_ad13", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x838, "gpmc_ad14",	0, 0, "gpmc_ad14", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x83C, "gpmc_ad15",	0, 0, "gpmc_ad15", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x840, "gpmc_a0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x844, "gpmc_a1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x848, "gpmc_a2",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x84C, "gpmc_a3",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x850, "gpmc_a4",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x854, "gpmc_a5",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x858, "gpmc_a6",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x85C, "gpmc_a7",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x860, "gpmc_a8",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x864, "gpmc_a9",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x868, "gpmc_a10",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x86C, "gpmc_a11",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x870, "gpmc_wait0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x874, "gpmc_wpn",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x878, "gpmc_be1n",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x87c, "gpmc_csn0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x880, "gpmc_csn1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x884, "gpmc_csn2",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x888, "gpmc_csn3",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x88c, "gpmc_clk",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x890, "gpmc_advn_ale",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x894, "gpmc_oen_ren",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x898, "gpmc_wen",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x89c, "gpmc_be0n_cle",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8a0, "lcd_data0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8a4, "lcd_data1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8a8, "lcd_data2",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8ac, "lcd_data3",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8b0, "lcd_data4",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8b4, "lcd_data5",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8b8, "lcd_data6",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8bc, "lcd_data7",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8c0, "lcd_data8",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8c4, "lcd_data9",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8c8, "lcd_data10",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8cc, "lcd_data11",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8d0, "lcd_data12",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8d4, "lcd_data13",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8d8, "lcd_data14",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8dc, "lcd_data15",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8e0, "lcd_vsync",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8e4, "lcd_hsync",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8e8, "lcd_pclk",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8ec, "lcd_ac_bias_en",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8f0, "mmc0_dat3",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8f4, "mmc0_dat2",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8f8, "mmc0_dat1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x8fc, "mmc0_dat0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x900, "mmc0_clk",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x904, "mmc0_cmd",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
#endif
	_PIN(0x908, "MII1_COL",		 96, 7, "gmii1_col", "rmii2_refclk", "spi1_sclk", "uart5_rxd", "mcasp1_axr2", "mmc2_dat3", "mcasp0_axr2", "gpio3_0"),
	_PIN(0x90c, "MII1_CRS",		 97, 7, "gmii1_crs", "rmii1_crs_dv", "spi1_d0", "I2C1_SDA", "mcasp1_aclkx", "uart5_ctsn", "uart2_rxd", "gpio3_1"),
	_PIN(0x910, "MII1_RX_ER",	 98, 7, "gmii1_rxerr", "rmii1_rxerr", "spi1_d1", "I2C1_SCL", "mcasp1_fsx", "uart5_rtsn", "uart2_txd", "gpio3_2"),
	_PIN(0x914, "MII1_TX_EN",	 99, 7, "gmii1_txen", "rmii1_txen", "rgmii1_tctl", "timer4", "mcasp1_axr0", "eQEP0_index", "mmc2_cmd", "gpio3_3"),
	_PIN(0x918, "MII1_RX_DV",	100, 7, "gmii1_rxdv", "cd_memory_clk", "rgmii1_rctl", "uart5_txd", "mcasp1_aclkx", "mmc2_dat0", "mcasp0_aclkr", "gpio3_4"),
	_PIN(0x91c, "MII1_TXD3",	 16, 7, "gmii1_txd3", "dcan0_tx", "rgmii1_td3", "uart4_rxd", "mcasp1_fsx", "mmc2_dat1", "mcasp0_fsr", "gpio0_16"),
	_PIN(0x920, "MII1_TXD2",	 17, 7, "gmii1_txd2", "dcan0_rx", "rgmii1_td2", "uart4_txd", "mcasp1_axr0", "mmc2_dat2", "mcasp0_ahclkx", "gpio0_17"),
	_PIN(0x924, "MII1_TXD1",	 21, 7, "gmii1_txd1", "rmii1_txd1", "rgmii1_td1", "mcasp1_fsr", "mcasp1_axr1", "eQEP0A_in", "mmc1_cmd", "gpio0_21"),
	_PIN(0x928, "MII1_TXD0",	 28, 7, "gmii1_txd0", "rmii1_txd0", "rgmii1_td0", "mcasp1_axr2", "mcasp1_aclkr", "eQEP0B_in", "mmc1_clk", "gpio0_28"),
	_PIN(0x92c, "MII1_TX_CLK",	105, 7, "gmii1_txclk", "uart2_rxd", "rgmii1_tclk", "mmc0_dat7", "mmc1_dat0", "uart1_dcdn", "mcasp0_aclkx", "gpio3_9"),
	_PIN(0x930, "MII1_RX_CLK",	106, 7, "gmii1_rxclk", "uart2_txd", "rgmii1_rclk", "mmc0_dat6", "mmc1_dat1", "uart1_dsrn", "mcasp0_fsx", "gpio3_10"),
	_PIN(0x934, "MII1_RXD3",	 82, 7, "gmii1_rxd3", "uart3_rxd", "rgmii1_rd3", "mmc0_dat5", "mmc1_dat2", "uart1_dtrn", "mcasp0_axr0", "gpio2_18"),
	_PIN(0x938, "MII1_RXD2",	 83, 7, "gmii1_rxd2", "uart3_txd", "rgmii1_rd2", "mmc0_dat4", "mmc1_dat3", "uart1_rin", "mcasp0_axr1", "gpio2_19"),
	_PIN(0x93c, "MII1_RXD1",	 84, 7, "gmii1_rxd1", "rmii1_rxd1", "rgmii1_rd1", "mcasp1_axr3", "mcasp1_fsr", "eQEP0_strobe", "mmc2_clk", "gpio2_20"),
	_PIN(0x940, "MII1_RXD0",	 85, 7, "gmii1_rxd0", "rmii1_rxd0", "rgmii1_rd0", "mcasp1_ahclkx", "mcasp1_ahclkr", "mcasp1_aclkr", "mcasp0_axr3", "gpio2_21"),
	_PIN(0x944, "RMII1_REF_CLK",	 29, 7, "rmii1_refclk", "xdma_event_intr2", "spi1_cs0", "uart5_txd", "mcasp1_axr3", "mmc0_pow", "mcasp1_ahclkx", "gpio0_29"),
	_PIN(0x948, "MDIO",		  0, 7, "mdio_data", "timer6", "uart5_rxd", "uart3_ctsn", "mmc0_sdcd","mmc1_cmd", "mmc2_cmd","gpio0_0"),
	_PIN(0x94c, "MDC",		  1, 7, "mdio_clk", "timer5", "uart5_txd", "uart3_rtsn", "mmc0_sdwp", "mmc1_clk", "mmc2_clk", "gpio0_1"),
#if 0 /* Incomplete Entries - fill with data from table 2-7 in datasheet */
	_PIN(0x950, "spi0_sclk",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x954, "spi0_d0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x958, "spi0_d1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x95c, "spi0_cs0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x960, "spi0_cs1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x964, "ecap0_in_pwm0_out",0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x968, "uart0_ctsn",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x96c, "uart0_rtsn",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x970, "uart0_rxd",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x974, "uart0_txd",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x978, "uart1_ctsn",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x97c, "uart1_rtsn",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x980, "uart1_rxd",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x984, "uart1_txd",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x988, "i2c0_sda",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x98c, "i2c0_scl",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x990, "mcasp0_aclkx",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x994, "mcasp0_fsx",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x998, "mcasp0_axr0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x99c, "mcasp0_ahclkr",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9a0, "mcasp0_aclkr",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9a4, "mcasp0_fsr",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9a8, "mcasp0_axr1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9ac, "mcasp0_ahclkx",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9b0, "xdma_event_intr0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9b4, "xdma_event_intr1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9b8, "nresetin_out",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9bc, "porz",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9c0, "nnmi",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9c4, "osc0_in",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9c8, "osc0_out",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9cc, "osc0_vss",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9d0, "tms",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9d4, "tdi",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9d8, "tdo",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9dc, "tck",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9e0, "ntrst",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9e4, "emu0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9e8, "emu1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9ec, "osc1_in",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9f0, "osc1_out",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9f4, "osc1_vss",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9f8, "rtc_porz",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0x9fc, "pmic_power_en",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa00, "ext_wakeup",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa04, "enz_kaldo_1p8v",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa08, "usb0_dm",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa0c, "usb0_dp",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa10, "usb0_ce",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa14, "usb0_id",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa18, "usb0_vbus",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa1c, "usb0_drvvbus",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa20, "usb1_dm",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa24, "usb1_dp",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa28, "usb1_ce",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa2c, "usb1_id",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa30, "usb1_vbus",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa34, "usb1_drvvbus",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa38, "ddr_resetn",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa3c, "ddr_csn0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa40, "ddr_cke",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa44, "ddr_ck",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa48, "ddr_nck",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa4c, "ddr_casn",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa50, "ddr_rasn",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa54, "ddr_wen",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa58, "ddr_ba0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa5c, "ddr_ba1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa60, "ddr_ba2",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa64, "ddr_a0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa68, "ddr_a1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa6c, "ddr_a2",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa70, "ddr_a3",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa74, "ddr_a4",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa78, "ddr_a5",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa7c, "ddr_a6",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa80, "ddr_a7",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa84, "ddr_a8",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa88, "ddr_a9",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa8c, "ddr_a10",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa90, "ddr_a11",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa94, "ddr_a12",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa98, "ddr_a13",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xa9c, "ddr_a14",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaa0, "ddr_a15",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaa4, "ddr_odt",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaa8, "ddr_d0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaac, "ddr_d1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xab0, "ddr_d2",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xab4, "ddr_d3",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xab8, "ddr_d4",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xabc, "ddr_d5",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xac0, "ddr_d6",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xac4, "ddr_d7",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xac8, "ddr_d8",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xacc, "ddr_d9",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xad0, "ddr_d10",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xad4, "ddr_d11",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xad8, "ddr_d12",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xadc, "ddr_d13",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xae0, "ddr_d14",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xae4, "ddr_d15",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xae8, "ddr_dqm0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaec, "ddr_dqm1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaf0, "ddr_dqs0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaf4, "ddr_dqsn0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xaf8, "ddr_dqs1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xafc, "ddr_dqsn1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb00, "ddr_vref",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb04, "ddr_vtp",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb08, "ddr_strben0",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb0c, "ddr_strben1",	0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb2c, "ain0",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb28, "ain1",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb24, "ain2",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb20, "ain3",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb1c, "ain4",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb18, "ain5",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb14, "ain6",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb10, "ain7",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb30, "vrefp",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb34, "vrefn",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb38, "avdd",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb3c, "avss",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb40, "iforce",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb44, "vsense",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PIN(0xb48, "testout",		0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL),
#endif
	{  .ballname = NULL  },
};

const struct ti_scm_device ti_scm_dev = {
	.padconf_muxmode_mask	= 0x7,
	.padconf_sate_mask	= 0x78,
	.padstate		= (struct ti_scm_padstate *) &ti_padstate_devmap,
	.padconf		= (struct ti_scm_padconf *) &ti_padconf_devmap,
};

int
ti_scm_padconf_set_gpioflags(uint32_t gpio, uint32_t flags)
{
	/* TODO */
	return (EINVAL);
}

void
ti_scm_padconf_get_gpioflags(uint32_t gpio, uint32_t *flags)
{
	/* TODO */
}

