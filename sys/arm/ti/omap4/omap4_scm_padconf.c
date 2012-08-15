/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
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
 * 3. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BEN GRAY ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BEN GRAY BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <sys/gpio.h>

#include <arm/ti/tivar.h>
#include <arm/ti/ti_scm.h>
#include <arm/ti/omap4/omap4var.h>
#include <arm/ti/omap4/omap4_reg.h>


/*
 *	This file defines the pin mux configuration for the OMAP4xxx series of
 *	devices.
 *
 *	How This is Suppose to Work
 *	===========================
 *	- There is a top level ti_scm module (System Control Module) that is
 *	the interface for all omap drivers, which can use it to change the mux
 *	settings for individual pins.  (That said, typically the pin mux settings
 *	are set to defaults by the 'hints' and then not altered by the driver).
 *
 *	- For this to work the top level driver needs all the pin info, and hence
 *	this is where this file comes in.  Here we define all the pin information
 *	that is supplied to the top level driver.
 *
 */

#define CONTROL_PADCONF_WAKEUP_EVENT     (1UL << 15)
#define CONTROL_PADCONF_WAKEUP_ENABLE    (1UL << 14)
#define CONTROL_PADCONF_OFF_PULL_UP      (1UL << 13)
#define CONTROL_PADCONF_OFF_PULL_ENABLE  (1UL << 12)
#define CONTROL_PADCONF_OFF_OUT_HIGH     (1UL << 11)
#define CONTROL_PADCONF_OFF_OUT_ENABLE   (1UL << 10)
#define CONTROL_PADCONF_OFF_ENABLE       (1UL << 9)
#define CONTROL_PADCONF_INPUT_ENABLE     (1UL << 8)
#define CONTROL_PADCONF_PULL_UP          (1UL << 4)
#define CONTROL_PADCONF_PULL_ENABLE      (1UL << 3)
#define CONTROL_PADCONF_MUXMODE_MASK     (0x7)

#define CONTROL_PADCONF_SATE_MASK        ( CONTROL_PADCONF_WAKEUP_EVENT \
                                         | CONTROL_PADCONF_WAKEUP_ENABLE \
                                         | CONTROL_PADCONF_OFF_PULL_UP \
                                         | CONTROL_PADCONF_OFF_PULL_ENABLE \
                                         | CONTROL_PADCONF_OFF_OUT_HIGH \
                                         | CONTROL_PADCONF_OFF_OUT_ENABLE \
                                         | CONTROL_PADCONF_OFF_ENABLE \
                                         | CONTROL_PADCONF_INPUT_ENABLE \
                                         | CONTROL_PADCONF_PULL_UP \
                                         | CONTROL_PADCONF_PULL_ENABLE )

/* Active pin states */
#define PADCONF_PIN_OUTPUT              0
#define PADCONF_PIN_INPUT               CONTROL_PADCONF_INPUT_ENABLE
#define PADCONF_PIN_INPUT_PULLUP        ( CONTROL_PADCONF_INPUT_ENABLE \
                                        | CONTROL_PADCONF_PULL_ENABLE \
                                        | CONTROL_PADCONF_PULL_UP)
#define PADCONF_PIN_INPUT_PULLDOWN      ( CONTROL_PADCONF_INPUT_ENABLE \
                                        | CONTROL_PADCONF_PULL_ENABLE )

/* Off mode states */
#define PADCONF_PIN_OFF_NONE            0
#define PADCONF_PIN_OFF_OUTPUT_HIGH	    ( CONTROL_PADCONF_OFF_ENABLE \
                                        | CONTROL_PADCONF_OFF_OUT_ENABLE \
                                        | CONTROL_PADCONF_OFF_OUT_HIGH)
#define PADCONF_PIN_OFF_OUTPUT_LOW      ( CONTROL_PADCONF_OFF_ENABLE \
                                        | CONTROL_PADCONF_OFF_OUT_ENABLE)
#define PADCONF_PIN_OFF_INPUT_PULLUP    ( CONTROL_PADCONF_OFF_ENABLE \
                                        | CONTROL_PADCONF_OFF_PULL_ENABLE \
                                        | CONTROL_PADCONF_OFF_PULL_UP)
#define PADCONF_PIN_OFF_INPUT_PULLDOWN  ( CONTROL_PADCONF_OFF_ENABLE \
                                        | CONTROL_PADCONF_OFF_PULL_ENABLE)
#define PADCONF_PIN_OFF_WAKEUPENABLE	CONTROL_PADCONF_WAKEUP_ENABLE


#define _PINDEF(r, b, gp, gm, m0, m1, m2, m3, m4, m5, m6, m7) \
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

const struct ti_scm_padstate ti_padstate_devmap[] = {
	{"output",		PADCONF_PIN_OUTPUT},
	{"input",		PADCONF_PIN_INPUT},
	{"input_pullup",	PADCONF_PIN_INPUT_PULLUP},
	{"input_pulldown",	PADCONF_PIN_INPUT_PULLDOWN},
	{ .state = NULL }
};

/*
 * Table 18-10, p. 3470
 */
const struct ti_scm_padconf ti_padconf_devmap[] = {
	_PINDEF(0x0040,  "c12",   0, 0, "gpmc_ad0", "sdmmc2_dat0", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0042,  "d12",   0, 0, "gpmc_ad1", "sdmmc2_dat1", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0044,  "c13",   0, 0, "gpmc_ad2", "sdmmc2_dat2", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0046,  "d13",   0, 0, "gpmc_ad3", "sdmmc2_dat3", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0048,  "c15",   0, 0, "gpmc_ad4", "sdmmc2_dat4", "sdmmc2_dir_dat0", NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x004a,  "d15",   0, 0, "gpmc_ad5", "sdmmc2_dat5", "sdmmc2_dir_dat1", NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x004c,  "a16",   0, 0, "gpmc_ad6", "sdmmc2_dat6", "sdmmc2_dir_cmd", NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x004e,  "b16",   0, 0, "gpmc_ad7", "sdmmc2_dat7", "sdmmc2_clk_fdbk", NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0050,  "c16",  32, 3, "gpmc_ad8", "kpd_row0", "c2c_data15", "gpio_32", NULL, "sdmmc1_dat0", NULL, NULL),
	_PINDEF(0x0052,  "d16",  33, 3, "gpmc_ad9", "kpd_row1", "c2c_data14", "gpio_33", NULL, "sdmmc1_dat1", NULL, NULL),
	_PINDEF(0x0054,  "c17",  34, 3, "gpmc_ad10", "kpd_row2", "c2c_data13", "gpio_34", NULL, "sdmmc1_dat2", NULL, NULL),
	_PINDEF(0x0056,  "d17",  35, 3, "gpmc_ad11", "kpd_row3", "c2c_data12", "gpio_35", NULL, "sdmmc1_dat3", NULL, NULL),
	_PINDEF(0x0058,  "c18",  36, 3, "gpmc_ad12", "kpd_col0", "c2c_data11", "gpio_36", NULL, "sdmmc1_dat4", NULL, NULL),
	_PINDEF(0x005a,  "d18",  37, 3, "gpmc_ad13", "kpd_col1", "c2c_data10", "gpio_37", NULL, "sdmmc1_dat5", NULL, NULL),
	_PINDEF(0x005c,  "c19",  38, 3, "gpmc_ad14", "kpd_col2", "c2c_data9", "gpio_38", NULL, "sdmmc1_dat6", NULL, NULL),
	_PINDEF(0x005e,  "d19",  39, 3, "gpmc_ad15", "kpd_col3", "c2c_data8", "gpio_39", NULL, "sdmmc1_dat7", NULL, NULL),
	_PINDEF(0x0060,  "b17",  40, 3, "gpmc_a16", "kpd_row4", "c2c_datain0", "gpio_40", "venc_656_data0", NULL, NULL, "safe_mode"),
	_PINDEF(0x0062,  "a18",  41, 3, "gpmc_a17", "kpd_row5", "c2c_datain1", "gpio_41", "venc_656_data1", NULL, NULL, "safe_mode"),
	_PINDEF(0x0064,  "b18",  42, 3, "gpmc_a18", "kpd_row6", "c2c_datain2", "gpio_42", "venc_656_data2", NULL, NULL, "safe_mode"),
	_PINDEF(0x0066,  "a19",  43, 3, "gpmc_a19", "kpd_row7", "c2c_datain3", "gpio_43", "venc_656_data3", NULL, NULL, "safe_mode"),
	_PINDEF(0x0068,  "b19",  44, 3, "gpmc_a20", "kpd_col4", "c2c_datain4", "gpio_44", "venc_656_data4", NULL, NULL, "safe_mode"),
	_PINDEF(0x006a,  "b20",  45, 3, "gpmc_a21", "kpd_col5", "c2c_datain5", "gpio_45", "venc_656_data5", NULL, NULL, "safe_mode"),
	_PINDEF(0x006c,  "a21",  46, 3, "gpmc_a22", "kpd_col6", "c2c_datain6", "gpio_46", "venc_656_data6", NULL, NULL, "safe_mode"),
	_PINDEF(0x006e,  "b21",  47, 3, "gpmc_a23", "kpd_col7", "c2c_datain7", "gpio_47", "venc_656_data7", NULL, NULL, "safe_mode"),
	_PINDEF(0x0070,  "c20",  48, 3, "gpmc_a24", "kpd_col8", "c2c_clkout0", "gpio_48", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0072,  "d20",  49, 3, "gpmc_a25", NULL, "c2c_clkout1", "gpio_49", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0074,  "b25",  50, 3, "gpmc_ncs0", NULL, NULL, "gpio_50", "sys_ndmareq0", NULL, NULL, NULL),
	_PINDEF(0x0076,  "c21",  51, 3, "gpmc_ncs1", NULL, "c2c_dataout6", "gpio_51", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0078,  "d21",  52, 3, "gpmc_ncs2", "kpd_row8", "c2c_dataout7", "gpio_52", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x007a,  "c22",  53, 3, "gpmc_ncs3", "gpmc_dir", "c2c_dataout4", "gpio_53", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x007c,  "c25",  54, 3, "gpmc_nwp", "dsi1_te0", NULL, "gpio_54", "sys_ndmareq1", NULL, NULL, NULL),
	_PINDEF(0x007e,  "b22",  55, 3, "gpmc_clk", NULL, NULL, "gpio_55", "sys_ndmareq2", "sdmmc1_cmd", NULL, NULL),
	_PINDEF(0x0080,  "d25",  56, 3, "gpmc_nadv_ale", "dsi1_te1", NULL, "gpio_56", "sys_ndmareq3", "sdmmc1_clk", NULL, NULL),
	_PINDEF(0x0082,  "b11",   0, 0, "gpmc_noe", "sdmmc2_clk", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0084,  "b12",   0, 0, "gpmc_nwe", "sdmmc2_cmd", NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0086,  "c23",  59, 3, "gpmc_nbe0_cle", "dsi2_te0", NULL, "gpio_59", NULL, NULL, NULL, NULL),
	_PINDEF(0x0088,  "d22",  60, 3, "gpmc_nbe1", NULL, "c2c_dataout5", "gpio_60", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x008a,  "b26",  61, 3, "gpmc_wait0", "dsi2_te1", NULL, "gpio_61", NULL, NULL, NULL, NULL),
	_PINDEF(0x008c,  "b23",  62, 3, "gpmc_wait1", NULL, "c2c_dataout2", "gpio_62", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x008e,  "d23", 100, 3, "gpmc_wait2", "usbc1_icusb_txen", "c2c_dataout3", "gpio_100", "sys_ndmareq0", NULL, NULL, "safe_mode"),
	_PINDEF(0x0090,  "a24", 101, 3, "gpmc_ncs4", "dsi1_te0", "c2c_clkin0", "gpio_101", "sys_ndmareq1", NULL, NULL, "safe_mode"),
	_PINDEF(0x0092,  "b24", 102, 3, "gpmc_ncs5", "dsi1_te1", "c2c_clkin1", "gpio_102", "sys_ndmareq2", NULL, NULL, "safe_mode"),
	_PINDEF(0x0094,  "c24", 103, 3, "gpmc_ncs6", "dsi2_te0", "c2c_dataout0", "gpio_103", "sys_ndmareq3", NULL, NULL, "safe_mode"),
	_PINDEF(0x0096,  "d24", 104, 3, "gpmc_ncs7", "dsi2_te1", "c2c_dataout1", "gpio_104", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0098,   "b9",  63, 3, "hdmi_hpd", NULL, NULL, "gpio_63", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x009a,  "b10",  64, 3, "hdmi_cec", NULL, NULL, "gpio_64", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x009c,   "a8",  65, 3, "hdmi_ddc_scl", NULL, NULL, "gpio_65", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x009e,   "b8",  66, 3, "hdmi_ddc_sda", NULL, NULL, "gpio_66", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00a0,  "r26",   0, 0, "csi21_dx0", NULL, NULL, "gpi_67", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00a2,  "r25",   0, 0, "csi21_dy0", NULL, NULL, "gpi_68", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00a4,  "t26",   0, 0, "csi21_dx1", NULL, NULL, "gpi_69", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00a6,  "t25",   0, 0, "csi21_dy1", NULL, NULL, "gpi_70", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00a8,  "u26",   0, 0, "csi21_dx2", NULL, NULL, "gpi_71", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00aa,  "u25",   0, 0, "csi21_dy2", NULL, NULL, "gpi_72", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00ac,  "v26",   0, 0, "csi21_dx3", NULL, NULL, "gpi_73", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00ae,  "v25",   0, 0, "csi21_dy3", NULL, NULL, "gpi_74", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00b0,  "w26",   0, 0, "csi21_dx4", NULL, NULL, "gpi_75", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00b2,  "w25",   0, 0, "csi21_dy4", NULL, NULL, "gpi_76", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00b4,  "m26",   0, 0, "csi22_dx0", NULL, NULL, "gpi_77", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00b6,  "m25",   0, 0, "csi22_dy0", NULL, NULL, "gpi_78", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00b8,  "n26",   0, 0, "csi22_dx1", NULL, NULL, "gpi_79", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00ba,  "n25",   0, 0, "csi22_dy1", NULL, NULL, "gpi_80", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00bc,  "t27",  81, 3, "cam_shutter", NULL, NULL, "gpio_81", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00be,  "u27",  82, 3, "cam_strobe", NULL, NULL, "gpio_82", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00c0,  "v27",  83, 3, "cam_globalreset", NULL, NULL, "gpio_83", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00c2, "ae18",  84, 3, "usbb1_ulpitll_clk", "hsi1_cawake", NULL, "gpio_84", "usbb1_ulpiphy_clk", NULL, "hw_dbg20", "safe_mode"),
	_PINDEF(0x00c4, "ag19",  85, 3, "usbb1_ulpitll_stp", "hsi1_cadata", "mcbsp4_clkr", "gpio_85", "usbb1_ulpiphy_stp", "usbb1_mm_rxdp", "hw_dbg21", "safe_mode"),
	_PINDEF(0x00c6, "af19",  86, 3, "usbb1_ulpitll_dir", "hsi1_caflag", "mcbsp4_fsr", "gpio_86", "usbb1_ulpiphy_dir", NULL, "hw_dbg22", "safe_mode"),
	_PINDEF(0x00c8, "ae19",  87, 3, "usbb1_ulpitll_nxt", "hsi1_acready", "mcbsp4_fsx", "gpio_87", "usbb1_ulpiphy_nxt", "usbb1_mm_rxdm", "hw_dbg23", "safe_mode"),
	_PINDEF(0x00ca, "af18",  88, 3, "usbb1_ulpitll_dat0", "hsi1_acwake", "mcbsp4_clkx", "gpio_88", "usbb1_ulpiphy_dat0", "usbb1_mm_txen", "hw_dbg24", "safe_mode"),
	_PINDEF(0x00cc, "ag18",  89, 3, "usbb1_ulpitll_dat1", "hsi1_acdata", "mcbsp4_dx", "gpio_89", "usbb1_ulpiphy_dat1", "usbb1_mm_txdat", "hw_dbg25", "safe_mode"),
	_PINDEF(0x00ce, "ae17",  90, 3, "usbb1_ulpitll_dat2", "hsi1_acflag", "mcbsp4_dr", "gpio_90", "usbb1_ulpiphy_dat2", "usbb1_mm_txse0", "hw_dbg26", "safe_mode"),
	_PINDEF(0x00d0, "af17",  91, 3, "usbb1_ulpitll_dat3", "hsi1_caready", NULL, "gpio_91", "usbb1_ulpiphy_dat3", "usbb1_mm_rxrcv", "hw_dbg27", "safe_mode"),
	_PINDEF(0x00d2, "ah17",  92, 3, "usbb1_ulpitll_dat4", "dmtimer8_pwm_evt", "abe_mcbsp3_dr", "gpio_92", "usbb1_ulpiphy_dat4", NULL, "hw_dbg28", "safe_mode"),
	_PINDEF(0x00d4, "ae16",  93, 3, "usbb1_ulpitll_dat5", "dmtimer9_pwm_evt", "abe_mcbsp3_dx", "gpio_93", "usbb1_ulpiphy_dat5", NULL, "hw_dbg29", "safe_mode"),
	_PINDEF(0x00d6, "af16",  94, 3, "usbb1_ulpitll_dat6", "dmtimer10_pwm_evt", "abe_mcbsp3_clkx", "gpio_94", "usbb1_ulpiphy_dat6", "abe_dmic_din3", "hw_dbg30", "safe_mode"),
	_PINDEF(0x00d8, "ag16",  95, 3, "usbb1_ulpitll_dat7", "dmtimer11_pwm_evt", "abe_mcbsp3_fsx", "gpio_95", "usbb1_ulpiphy_dat7", "abe_dmic_clk3", "hw_dbg31", "safe_mode"),
	_PINDEF(0x00da, "af14",  96, 3, "usbb1_hsic_data", NULL, NULL, "gpio_96", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00dc, "ae14",  97, 3, "usbb1_hsic_strobe", NULL, NULL, "gpio_97", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00de,   "h2",  98, 3, "usbc1_icusb_dp", NULL, NULL, "gpio_98", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00e0,   "h3",  99, 3, "usbc1_icusb_dm", NULL, NULL, "gpio_99", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00e2,   "d2", 100, 3, "sdmmc1_clk", NULL, "dpm_emu19", "gpio_100", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00e4,   "e3", 101, 3, "sdmmc1_cmd", NULL, "uart1_rx", "gpio_101", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00e6,   "e4", 102, 3, "sdmmc1_dat0", NULL, "dpm_emu18", "gpio_102", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00e8,   "e2", 103, 3, "sdmmc1_dat1", NULL, "dpm_emu17", "gpio_103", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00ea,   "e1", 104, 3, "sdmmc1_dat2", NULL, "dpm_emu16", "gpio_104", "jtag_tms_tmsc", NULL, NULL, "safe_mode"),
	_PINDEF(0x00ec,   "f4", 105, 3, "sdmmc1_dat3", NULL, "dpm_emu15", "gpio_105", "jtag_tck", NULL, NULL, "safe_mode"),
	_PINDEF(0x00ee,   "f3", 106, 3, "sdmmc1_dat4", NULL, NULL, "gpio_106", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00f0,   "f1", 107, 3, "sdmmc1_dat5", NULL, NULL, "gpio_107", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00f2,   "g4", 108, 3, "sdmmc1_dat6", NULL, NULL, "gpio_108", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00f4,   "g3", 109, 3, "sdmmc1_dat7", NULL, NULL, "gpio_109", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x00f6, "ad27", 110, 3, "abe_mcbsp2_clkx", "mcspi2_clk", "abe_mcasp_ahclkx", "gpio_110", "usbb2_mm_rxdm", NULL, NULL, "safe_mode"),
	_PINDEF(0x00f8, "ad26", 111, 3, "abe_mcbsp2_dr", "mcspi2_somi", "abe_mcasp_axr", "gpio_111", "usbb2_mm_rxdp", NULL, NULL, "safe_mode"),
	_PINDEF(0x00fa, "ad25", 112, 3, "abe_mcbsp2_dx", "mcspi2_simo", "abe_mcasp_amute", "gpio_112", "usbb2_mm_rxrcv", NULL, NULL, "safe_mode"),
	_PINDEF(0x00fc, "ac28", 113, 3, "abe_mcbsp2_fsx", "mcspi2_cs0", "abe_mcasp_afsx", "gpio_113", "usbb2_mm_txen", NULL, NULL, "safe_mode"),
	_PINDEF(0x00fe, "ac26", 114, 3, "abe_mcbsp1_clkx", "abe_slimbus1_clock", NULL, "gpio_114", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0100, "ac25", 115, 3, "abe_mcbsp1_dr", "abe_slimbus1_data", NULL, "gpio_115", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0102, "ab25", 116, 3, "abe_mcbsp1_dx", "sdmmc3_dat2", "abe_mcasp_aclkx", "gpio_116", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0104, "ac27", 117, 3, "abe_mcbsp1_fsx", "sdmmc3_dat3", "abe_mcasp_amutein", "gpio_117", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0106, "ag25",   0, 0, "abe_pdm_ul_data", "abe_mcbsp3_dr", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0108, "af25",   0, 0, "abe_pdm_dl_data", "abe_mcbsp3_dx", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x010a, "ae25",   0, 0, "abe_pdm_frame", "abe_mcbsp3_clkx", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x010c, "af26",   0, 0, "abe_pdm_lb_clk", "abe_mcbsp3_fsx", NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x010e, "ah26", 118, 3, "abe_clks", NULL, NULL, "gpio_118", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0110, "ae24", 119, 3, "abe_dmic_clk1", NULL, NULL, "gpio_119", "usbb2_mm_txse0", "uart4_cts", NULL, "safe_mode"),
	_PINDEF(0x0112, "af24", 120, 3, "abe_dmic_din1", NULL, NULL, "gpio_120", "usbb2_mm_txdat", "uart4_rts", NULL, "safe_mode"),
	_PINDEF(0x0114, "ag24", 121, 3, "abe_dmic_din2", "slimbus2_clock", "abe_mcasp_axr", "gpio_121", NULL, "dmtimer11_pwm_evt", NULL, "safe_mode"),
	_PINDEF(0x0116, "ah24", 122, 3, "abe_dmic_din3", "slimbus2_data", "abe_dmic_clk2", "gpio_122", NULL, "dmtimer9_pwm_evt", NULL, "safe_mode"),
	_PINDEF(0x0118, "ab26", 123, 3, "uart2_cts", "sdmmc3_clk", NULL, "gpio_123", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x011a, "ab27", 124, 3, "uart2_rts", "sdmmc3_cmd", NULL, "gpio_124", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x011c, "aa25", 125, 3, "uart2_rx", "sdmmc3_dat0", NULL, "gpio_125", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x011e, "aa26", 126, 3, "uart2_tx", "sdmmc3_dat1", NULL, "gpio_126", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0120, "aa27", 127, 3, "hdq_sio", "i2c3_sccb", "i2c2_sccb", "gpio_127", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0122, "ae28",   0, 0, "i2c1_scl", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0124, "ae26",   0, 0, "i2c1_sda", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0126,  "c26", 128, 3, "i2c2_scl", "uart1_rx", NULL, "gpio_128", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0128,  "d26", 129, 3, "i2c2_sda", "uart1_tx", NULL, "gpio_129", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x012a,  "w27", 130, 3, "i2c3_scl", NULL, NULL, "gpio_130", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x012c,  "y27", 131, 3, "i2c3_sda", NULL, NULL, "gpio_131", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x012e, "ag21", 132, 3, "i2c4_scl", NULL, NULL, "gpio_132", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0130, "ah22", 133, 3, "i2c4_sda", NULL, NULL, "gpio_133", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0132, "af22", 134, 3, "mcspi1_clk", NULL, NULL, "gpio_134", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0134, "ae22", 135, 3, "mcspi1_somi", NULL, NULL, "gpio_135", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0136, "ag22", 136, 3, "mcspi1_simo", NULL, NULL, "gpio_136", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0138, "ae23", 137, 3, "mcspi1_cs0", NULL, NULL, "gpio_137", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x013a, "af23", 138, 3, "mcspi1_cs1", "uart1_rx", NULL, "gpio_138", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x013c, "ag23", 139, 3, "mcspi1_cs2", "uart1_cts", "slimbus2_clock", "gpio_139", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x013e, "ah23", 140, 3, "mcspi1_cs3", "uart1_rts", "slimbus2_data", "gpio_140", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0140,  "f27", 141, 3, "uart3_cts_rctx", "uart1_tx", NULL, "gpio_141", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0142,  "f28", 142, 3, "uart3_rts_sd", NULL, NULL, "gpio_142", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0144,  "g27", 143, 3, "uart3_rx_irrx", "dmtimer8_pwm_evt", NULL, "gpio_143", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0146,  "g28", 144, 3, "uart3_tx_irtx", "dmtimer9_pwm_evt", NULL, "gpio_144", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0148,  "ae5", 145, 3, "sdmmc5_clk", "mcspi2_clk", "usbc1_icusb_dp", "gpio_145", NULL, "sdmmc2_clk", NULL, "safe_mode"),
	_PINDEF(0x014a,  "af5", 146, 3, "sdmmc5_cmd", "mcspi2_simo", "usbc1_icusb_dm", "gpio_146", NULL, "sdmmc2_cmd", NULL, "safe_mode"),
	_PINDEF(0x014c,  "ae4", 147, 3, "sdmmc5_dat0", "mcspi2_somi", "usbc1_icusb_rcv", "gpio_147", NULL, "sdmmc2_dat0", NULL, "safe_mode"),
	_PINDEF(0x014e,  "af4", 148, 3, "sdmmc5_dat1", NULL, "usbc1_icusb_txen", "gpio_148", NULL, "sdmmc2_dat1", NULL, "safe_mode"),
	_PINDEF(0x0150,  "ag3", 149, 3, "sdmmc5_dat2", "mcspi2_cs1", NULL, "gpio_149", NULL, "sdmmc2_dat2", NULL, "safe_mode"),
	_PINDEF(0x0152,  "af3", 150, 3, "sdmmc5_dat3", "mcspi2_cs0", NULL, "gpio_150", NULL, "sdmmc2_dat3", NULL, "safe_mode"),
	_PINDEF(0x0154, "ae21", 151, 3, "mcspi4_clk", "sdmmc4_clk", "kpd_col6", "gpio_151", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0156, "af20", 152, 3, "mcspi4_simo", "sdmmc4_cmd", "kpd_col7", "gpio_152", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0158, "af21", 153, 3, "mcspi4_somi", "sdmmc4_dat0", "kpd_row6", "gpio_153", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x015a, "ae20", 154, 3, "mcspi4_cs0", "sdmmc4_dat3", "kpd_row7", "gpio_154", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x015c, "ag20", 155, 3, "uart4_rx", "sdmmc4_dat2", "kpd_row8", "gpio_155", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x015e, "ah19", 156, 3, "uart4_tx", "sdmmc4_dat1", "kpd_col8", "gpio_156", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0160, "ag12", 157, 3, "usbb2_ulpitll_clk", "usbb2_ulpiphy_clk", "sdmmc4_cmd", "gpio_157", "hsi2_cawake", NULL, NULL, "safe_mode"),
	_PINDEF(0x0162, "af12", 158, 3, "usbb2_ulpitll_stp", "usbb2_ulpiphy_stp", "sdmmc4_clk", "gpio_158", "hsi2_cadata", "dispc2_data23", NULL, "safe_mode"),
	_PINDEF(0x0164, "ae12", 159, 3, "usbb2_ulpitll_dir", "usbb2_ulpiphy_dir", "sdmmc4_dat0", "gpio_159", "hsi2_caflag", "dispc2_data22", NULL, "safe_mode"),
	_PINDEF(0x0166, "ag13", 160, 3, "usbb2_ulpitll_nxt", "usbb2_ulpiphy_nxt", "sdmmc4_dat1", "gpio_160", "hsi2_acready", "dispc2_data21", NULL, "safe_mode"),
	_PINDEF(0x0168, "ae11", 161, 3, "usbb2_ulpitll_dat0", "usbb2_ulpiphy_dat0", "sdmmc4_dat2", "gpio_161", "hsi2_acwake", "dispc2_data20", "usbb2_mm_txen", "safe_mode"),
	_PINDEF(0x016a, "af11", 162, 3, "usbb2_ulpitll_dat1", "usbb2_ulpiphy_dat1", "sdmmc4_dat3", "gpio_162", "hsi2_acdata", "dispc2_data19", "usbb2_mm_txdat", "safe_mode"),
	_PINDEF(0x016c, "ag11", 163, 3, "usbb2_ulpitll_dat2", "usbb2_ulpiphy_dat2", "sdmmc3_dat2", "gpio_163", "hsi2_acflag", "dispc2_data18", "usbb2_mm_txse0", "safe_mode"),
	_PINDEF(0x016e, "ah11", 164, 3, "usbb2_ulpitll_dat3", "usbb2_ulpiphy_dat3", "sdmmc3_dat1", "gpio_164", "hsi2_caready", "dispc2_data15", "rfbi_data15", "safe_mode"),
	_PINDEF(0x0170, "ae10", 165, 3, "usbb2_ulpitll_dat4", "usbb2_ulpiphy_dat4", "sdmmc3_dat0", "gpio_165", "mcspi3_somi", "dispc2_data14", "rfbi_data14", "safe_mode"),
	_PINDEF(0x0172, "af10", 166, 3, "usbb2_ulpitll_dat5", "usbb2_ulpiphy_dat5", "sdmmc3_dat3", "gpio_166", "mcspi3_cs0", "dispc2_data13", "rfbi_data13", "safe_mode"),
	_PINDEF(0x0174, "ag10", 167, 3, "usbb2_ulpitll_dat6", "usbb2_ulpiphy_dat6", "sdmmc3_cmd", "gpio_167", "mcspi3_simo", "dispc2_data12", "rfbi_data12", "safe_mode"),
	_PINDEF(0x0176,  "ae9", 168, 3, "usbb2_ulpitll_dat7", "usbb2_ulpiphy_dat7", "sdmmc3_clk", "gpio_168", "mcspi3_clk", "dispc2_data11", "rfbi_data11", "safe_mode"),
	_PINDEF(0x0178, "af13", 169, 3, "usbb2_hsic_data", NULL, NULL, "gpio_169", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x017a, "ae13", 170, 3, "usbb2_hsic_strobe", NULL, NULL, "gpio_170", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x017c,  "g26", 171, 3, "kpd_col3", "kpd_col0", NULL, "gpio_171", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x017e,  "g25", 172, 3, "kpd_col4", "kpd_col1", NULL, "gpio_172", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0180,  "h26", 173, 3, "kpd_col5", "kpd_col2", NULL, "gpio_173", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0182,  "h25", 174, 3, "kpd_col0", "kpd_col3", NULL, "gpio_174", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0184,  "j27",   0, 0, "kpd_col1", "kpd_col4", NULL, "gpio_0", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0186,  "h27",   1, 3, "kpd_col2", "kpd_col5", NULL, "gpio_1", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0188,  "j26", 175, 3, "kpd_row3", "kpd_row0", NULL, "gpio_175", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x018a,  "j25", 176, 3, "kpd_row4", "kpd_row1", NULL, "gpio_176", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x018c,  "k26", 177, 3, "kpd_row5", "kpd_row2", NULL, "gpio_177", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x018e,  "k25", 178, 3, "kpd_row0", "kpd_row3", NULL, "gpio_178", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0190,  "l27",   2, 3, "kpd_row1", "kpd_row4", NULL, "gpio_2", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0192,  "k27",   3, 3, "kpd_row2", "kpd_row5", NULL, "gpio_3", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0194,   "c3",   0, 0, "usba0_otg_ce", NULL, NULL, NULL, NULL, NULL, NULL, NULL),
	_PINDEF(0x0196,   "b5",   0, 0, "usba0_otg_dp", "uart3_rx_irrx", "uart2_rx", NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x0198,   "b4",   0, 0, "usba0_otg_dm", "uart3_tx_irtx", "uart2_tx", NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x019a, "aa28", 181, 3, "fref_clk1_out", NULL, NULL, "gpio_181", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x019c,  "y28", 182, 3, "fref_clk2_out", NULL, NULL, "gpio_182", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x019e,  "ae6",   0, 0, "sys_nirq1", NULL, NULL, NULL, NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01a0,  "af6", 183, 3, "sys_nirq2", NULL, NULL, "gpio_183", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01a2,  "f26", 184, 3, "sys_boot0", NULL, NULL, "gpio_184", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01a4,  "e27", 185, 3, "sys_boot1", NULL, NULL, "gpio_185", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01a6,  "e26", 186, 3, "sys_boot2", NULL, NULL, "gpio_186", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01a8,  "e25", 187, 3, "sys_boot3", NULL, NULL, "gpio_187", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01aa,  "d28", 188, 3, "sys_boot4", NULL, NULL, "gpio_188", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01ac,  "d27", 189, 3, "sys_boot5", NULL, NULL, "gpio_189", NULL, NULL, NULL, "safe_mode"),
	_PINDEF(0x01ae,   "m2",  11, 3, "dpm_emu0", NULL, NULL, "gpio_11", NULL, NULL, "hw_dbg0", "safe_mode"),
	_PINDEF(0x01b0,   "n2",  12, 3, "dpm_emu1", NULL, NULL, "gpio_12", NULL, NULL, "hw_dbg1", "safe_mode"),
	_PINDEF(0x01b2,   "p2",  13, 3, "dpm_emu2", "usba0_ulpiphy_clk", NULL, "gpio_13", NULL, "dispc2_fid", "hw_dbg2", "safe_mode"),
	_PINDEF(0x01b4,   "v1",  14, 3, "dpm_emu3", "usba0_ulpiphy_stp", NULL, "gpio_14", "rfbi_data10", "dispc2_data10", "hw_dbg3", "safe_mode"),
	_PINDEF(0x01b6,   "v2",  15, 3, "dpm_emu4", "usba0_ulpiphy_dir", NULL, "gpio_15", "rfbi_data9", "dispc2_data9", "hw_dbg4", "safe_mode"),
	_PINDEF(0x01b8,   "w1",  16, 3, "dpm_emu5", "usba0_ulpiphy_nxt", NULL, "gpio_16", "rfbi_te_vsync0", "dispc2_data16", "hw_dbg5", "safe_mode"),
	_PINDEF(0x01ba,   "w2",  17, 3, "dpm_emu6", "usba0_ulpiphy_dat0", "uart3_tx_irtx", "gpio_17", "rfbi_hsync0", "dispc2_data17", "hw_dbg6", "safe_mode"),
	_PINDEF(0x01bc,   "w3",  18, 3, "dpm_emu7", "usba0_ulpiphy_dat1", "uart3_rx_irrx", "gpio_18", "rfbi_cs0", "dispc2_hsync", "hw_dbg7", "safe_mode"),
	_PINDEF(0x01be,   "w4",  19, 3, "dpm_emu8", "usba0_ulpiphy_dat2", "uart3_rts_sd", "gpio_19", "rfbi_re", "dispc2_pclk", "hw_dbg8", "safe_mode"),
	_PINDEF(0x01c0,   "y2",  20, 3, "dpm_emu9", "usba0_ulpiphy_dat3", "uart3_cts_rctx", "gpio_20", "rfbi_we", "dispc2_vsync", "hw_dbg9", "safe_mode"),
	_PINDEF(0x01c2,   "y3",  21, 3, "dpm_emu10", "usba0_ulpiphy_dat4", NULL, "gpio_21", "rfbi_a0", "dispc2_de", "hw_dbg10", "safe_mode"),
	_PINDEF(0x01c4,   "y4",  22, 3, "dpm_emu11", "usba0_ulpiphy_dat5", NULL, "gpio_22", "rfbi_data8", "dispc2_data8", "hw_dbg11", "safe_mode"),
	_PINDEF(0x01c6,  "aa1",  23, 3, "dpm_emu12", "usba0_ulpiphy_dat6", NULL, "gpio_23", "rfbi_data7", "dispc2_data7", "hw_dbg12", "safe_mode"),
	_PINDEF(0x01c8,  "aa2",  24, 3, "dpm_emu13", "usba0_ulpiphy_dat7", NULL, "gpio_24", "rfbi_data6", "dispc2_data6", "hw_dbg13", "safe_mode"),
	_PINDEF(0x01ca,  "aa3",  25, 3, "dpm_emu14", "sys_drm_msecure", "uart1_rx", "gpio_25", "rfbi_data5", "dispc2_data5", "hw_dbg14", "safe_mode"),
	_PINDEF(0x01cc,  "aa4",  26, 3, "dpm_emu15", "sys_secure_indicator", NULL, "gpio_26", "rfbi_data4", "dispc2_data4", "hw_dbg15", "safe_mode"),
	_PINDEF(0x01ce,  "ab2",  27, 3, "dpm_emu16", "dmtimer8_pwm_evt", "dsi1_te0", "gpio_27", "rfbi_data3", "dispc2_data3", "hw_dbg16", "safe_mode"),
	_PINDEF(0x01d0,  "ab3",  28, 3, "dpm_emu17", "dmtimer9_pwm_evt", "dsi1_te1", "gpio_28", "rfbi_data2", "dispc2_data2", "hw_dbg17", "safe_mode"),
	_PINDEF(0x01d2,  "ab4", 190, 3, "dpm_emu18", "dmtimer10_pwm_evt", "dsi2_te0", "gpio_190", "rfbi_data1", "dispc2_data1", "hw_dbg18", "safe_mode"),
	_PINDEF(0x01d4,  "ac4", 191, 3, "dpm_emu19", "dmtimer11_pwm_evt", "dsi2_te1", "gpio_191", "rfbi_data0", "dispc2_data0", "hw_dbg19", "safe_mode"),
	{  .ballname = NULL  },
};

const struct ti_scm_device ti_scm_dev = {
	.padconf_muxmode_mask	= CONTROL_PADCONF_MUXMODE_MASK,
	.padconf_sate_mask	= CONTROL_PADCONF_SATE_MASK,
	.padstate		= (struct ti_scm_padstate *) &ti_padstate_devmap,
	.padconf		= (struct ti_scm_padconf *) &ti_padconf_devmap,
};

int
ti_scm_padconf_set_gpioflags(uint32_t gpio, uint32_t flags)
{
	unsigned int state = 0;
	/* First the SCM driver needs to be told to put the pad into GPIO mode */
	if (flags & GPIO_PIN_OUTPUT)
		state = PADCONF_PIN_OUTPUT;
	else if (flags & GPIO_PIN_INPUT) {
		if (flags & GPIO_PIN_PULLUP)
			state = PADCONF_PIN_INPUT_PULLUP;
		else if (flags & GPIO_PIN_PULLDOWN)
			state = PADCONF_PIN_INPUT_PULLDOWN;
		else
			state = PADCONF_PIN_INPUT;
	}
	return ti_scm_padconf_set_gpiomode(gpio, state);
}

void
ti_scm_padconf_get_gpioflags(uint32_t gpio, uint32_t *flags)
{
	unsigned int state;
	/* Get the current pin state */
	if (ti_scm_padconf_get_gpiomode(gpio, &state) != 0)
		*flags = 0;
	else {
		switch (state) {
			case PADCONF_PIN_OUTPUT:
				*flags = GPIO_PIN_OUTPUT;
				break;
			case PADCONF_PIN_INPUT:
				*flags = GPIO_PIN_INPUT;
				break;
			case PADCONF_PIN_INPUT_PULLUP:
				*flags = GPIO_PIN_INPUT | GPIO_PIN_PULLUP;
				break;
			case PADCONF_PIN_INPUT_PULLDOWN:
				*flags = GPIO_PIN_INPUT | GPIO_PIN_PULLDOWN;
				break;
			default:
				*flags = 0;
				break;
		}
	}
}

