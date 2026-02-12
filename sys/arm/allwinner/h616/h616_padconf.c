/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Emmanuel Vadot <manu@freebsd.org>
 *
 * Copyright (c) 2026 The FreeBSD Foundation.
 *
 * Portions of this file were written by Tom Jones <thj@freebsd.org> under
 * sponsorship from The FreeBSD Foundation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <arm/allwinner/allwinner_pinctrl.h>

#include "opt_soc.h"

static const struct allwinner_pins h616_pins[] = {
	{ "PC0",  2,  0, { "gpio-in", "gpio_out", "nand", "mmc2", "spi0",   NULL, "pc_eint0" },  6,  0, 2 },
	{ "PC1",  2,  1, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint1" },  6,  1, 2 },
	{ "PC2",  2,  2, { "gpio-in", "gpio_out", "nand",   NULL, "spi0",   NULL, "pc_eint2" },  6,  2, 2 },
	{ "PC3",  2,  3, { "gpio-in", "gpio_out", "nand",   NULL, "spi0", "boot", "pc_eint3" },  6,  3, 2 },
	{ "PC4",  2,  4, { "gpio-in", "gpio_out", "nand",   NULL, "spi0", "boot", "pc_eint4" },  6,  4, 2 },
	{ "PC5",  2,  5, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL, "boot", "pc_eint5" },  6,  5, 2 },
	{ "PC6",  2,  6, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL, "boot", "pc_eint6" },  6,  6, 2 },
	{ "PC7",  2,  7, { "gpio-in", "gpio_out", "nand",   NULL, "spi0",   NULL, "pc_eint7" },  6,  7, 2 },
	{ "PC8",  2,  8, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint8" },  6,  8, 2 },
	{ "PC9",  2,  9, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint9" },  6,  9, 2 },
	{ "PC10", 2, 10, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint10" }, 6, 10, 2 },
	{ "PC11", 2, 11, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint11" }, 6, 11, 2 },
	{ "PC12", 2, 12, { "gpio-in", "gpio_out", "nand",   NULL,   NULL,   NULL, "pc_eint12" }, 6, 12, 2 },
	{ "PC13", 2, 13, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint13" }, 6, 13, 2 },
	{ "PC14", 2, 14, { "gpio-in", "gpio_out", "nand", "mmc2",   NULL,   NULL, "pc_eint14" }, 6, 14, 2 },
	{ "PC15", 2, 15, { "gpio-in", "gpio_out", "nand", "mmc2", "spi0",   NULL, "pc_eint15" }, 6, 15, 2 },
	{ "PC16", 2, 16, { "gpio-in", "gpio_out", "nand", "mmc2", "spi0",   NULL, "pc_eint16" }, 6, 16, 2 },

	{ "PF0", 5, 0, { "gpio-in", "gpio_out", "mmc0",  "jtag", NULL, NULL, "pf_eint0" }, 6, 0, 5 },
	{ "PF1", 5, 1, { "gpio-in", "gpio_out", "mmc0",  "jtag", NULL, NULL, "pf_eint1" }, 6, 1, 5 },
	{ "PF2", 5, 2, { "gpio-in", "gpio_out", "mmc0", "uart0", NULL, NULL, "pf_eint2" }, 6, 2, 5 },
	{ "PF3", 5, 3, { "gpio-in", "gpio_out", "mmc0",  "jtag", NULL, NULL, "pf_eint3" } ,6, 3, 5 },
	{ "PF4", 5, 4, { "gpio-in", "gpio_out", "mmc0", "uart0", NULL, NULL, "pf_eint4" }, 6, 4, 5 },
	{ "PF5", 5, 5, { "gpio-in", "gpio_out", "mmc0",  "jtag", NULL, NULL, "pf_eint5" }, 6, 5, 5 },
	{ "PF6", 5, 6, { "gpio-in", "gpio_out",   NULL,    NULL, NULL, NULL, "pf_eint6" }, 6, 6, 5 },

	{ "PG0",  6,  0, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint0" },  6,  0, 6 },
	{ "PG1",  6,  1, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint1" },  6,  1, 6 },
	{ "PG2",  6,  2, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint2" },  6,  2, 6 },
	{ "PG3",  6,  3, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint3" },  6,  3, 6 },
	{ "PG4",  6,  4, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint4" },  6,  4, 6 },
	{ "PG5",  6,  5, { "gpio-in", "gpio_out", "mmc1",	     NULL,   NULL,      NULL, "pg_eint5" },  6,  5, 6 },
	{ "PG6",  6,  6, { "gpio-in", "gpio_out", "uart1",	     NULL, "jtag",      NULL, "pg_eint6" },  6,  6, 6 },
	{ "PG7",  6,  7, { "gpio-in", "gpio_out", "uart1",           NULL, "jtag",      NULL, "pg_eint7" },  6,  7, 6 },
	{ "PG8",  6,  8, { "gpio-in", "gpio_out", "uart1", "pll_lock_dbg", "jtag",      NULL, "pg_eint8" },  6,  8, 6 },
	{ "PG9",  6,  9, { "gpio-in", "gpio_out", "uart1",           NULL, "jtag", "ac_adcy", "pg_eint9" },  6,  9, 6 },
	{ "PG10", 6, 10, { "gpio-in", "gpio_out",  "i2s2",     "x32kfout",   NULL, "ac_mclk", "pg_eint10" }, 6, 10, 6 },
	{ "PG11", 6, 11, { "gpio-in", "gpio_out",  "i2s2",           NULL, "bist", "ac_sync", "pg_eint11" }, 6, 11, 6 },
	{ "PG12", 6, 12, { "gpio-in", "gpio_out",  "i2s2",           NULL, "bist", "ac_adcl", "pg_eint12" }, 6, 12, 6 },
	{ "PG13", 6, 13, { "gpio-in", "gpio_out",  "i2s2",         "i2s2", "bist", "ac_adcr", "pg_eint13" }, 6, 13, 6 },
	{ "PG14", 6, 14, { "gpio-in", "gpio_out",  "i2s2",         "i2s2", "bist", "ac_adcx", "pg_eint14" }, 6, 14, 6 },
	{ "PG15", 6, 15, { "gpio-in", "gpio_out", "uart2",           NULL,   NULL,    "spi4", "pg_eint15" }, 6, 15, 6 },
	{ "PG16", 6, 16, { "gpio-in", "gpio_out", "uart2",           NULL,   NULL,    "spi4", "pg_eint16" }, 6, 16, 6 },
	{ "PG17", 6, 17, { "gpio-in", "gpio_out", "uart2",           NULL,   NULL,    "spi3", "pg_eint17" }, 6, 17, 6 },
	{ "PG18", 6, 18, { "gpio-in", "gpio_out", "uart2",           NULL,   NULL,    "spi3", "pg_eint18" }, 6, 18, 6 },
	{ "PG19", 6, 19, { "gpio-in", "gpio_out",    NULL,           NULL, "pwm1",      NULL, "pg_eint19" }, 6, 19, 6 },

	{ "PH0",  7,  0, { "gpio-in", "gpio_out", "uart0",   NULL,          "pwm3", "spi1", "ph_eint0" },  6,  0, 7 },
	{ "PH1",  7,  1, { "gpio-in", "gpio_out", "uart0",   NULL,          "pwm4", "spi1", "ph_eint1" },  6,  1, 7 },
	{ "PH2",  7,  2, { "gpio-in", "gpio_out", "uart5",  "owa",          "pwm2", "spi2", "ph_eint2" },  6,  2, 7 },
	{ "PH3",  7,  3, { "gpio-in", "gpio_out", "uart5",   NULL,          "pwm1", "spi2", "ph_eint3" },  6,  3, 7 },
	{ "PH4",  7,  4, { "gpio-in", "gpio_out",    NULL,  "owa",            NULL, "spi3", "ph_eint4" },  6,  4, 7 },
	{ "PH5",  7,  5, { "gpio-in", "gpio_out", "uart2", "i2s3",          "spi1", "spi3", "ph_eint5" },  6,  5, 7 },
	{ "PH6",  7,  6, { "gpio-in", "gpio_out", "uart2", "i2s3",          "spi1", "spi4", "ph_eint6" },  6,  6, 7 },
	{ "PH7",  7,  7, { "gpio-in", "gpio_out", "uart2", "i2s3",          "spi1", "spi4", "ph_eint7" },  6,  7, 7 },
	{ "PH8",  7,  8, { "gpio-in", "gpio_out", "uart2", "i2s3",          "spi1", "i2s3", "ph_eint8" },  6,  8, 7 },
	{ "PH9",  7,  9, { "gpio-in", "gpio_out",    NULL, "i2s3",          "spi1", "i2s3", "ph_eint9" },  6,  9, 7 },
	{ "PH10", 7, 10, { "gpio-in", "gpio_out",    NULL, "cir_rx",  "tcon_trig1",   NULL, "ph_eint10" }, 6, 10, 7 },

	{ "PI0",  9,  0, { "gpio-in", "gpio_out",    "rgmii",   "dmic_clk", "i2s0", "hdmi"        ,  "pi_eint0" }, 6,  0, 9 },
	{ "PI1",  9,  1, { "gpio-in", "gpio_out",    "rgmii", "dmic_data0", "i2s0", "hdmi"        ,  "pi_eint1" }, 6,  1, 9 },
	{ "PI2",  9,  2, { "gpio-in", "gpio_out",    "rgmii", "dmic_data1", "i2s0", "hdmi"        ,  "pi_eint2" }, 6,  2, 9 },
	{ "PI3",  9,  3, { "gpio-in", "gpio_out",    "rgmii", "dmic_data2", "i2s0", "i2s0"        ,  "pi_eint3" }, 6,  3, 9 },
	{ "PI4",  9,  4, { "gpio-in", "gpio_out",    "rgmii", "dmic_data3", "i2s0", "i2s0"        ,  "pi_eint4" }, 6,  4, 9 },
	{ "PI5",  9,  5, { "gpio-in", "gpio_out",    "rgmii",      "uart2",  "ts0", "spi0"        ,  "pi_eint5" }, 6,  5, 9 },
	{ "PI6",  9,  6, { "gpio-in", "gpio_out",    "rgmii",      "uart2",  "ts0", "spi0"        ,  "pi_eint6" }, 6,  6, 9 },
	{ "PI7",  9,  7, { "gpio-in", "gpio_out",    "rgmii",      "uart2",  "ts0", "spi1"        ,  "pi_eint7" }, 6,  7, 9 },
	{ "PI8",  9,  8, { "gpio-in", "gpio_out",    "rgmii",      "uart2",  "ts0", "spi1"        ,  "pi_eint8" }, 6,  8, 9 },
	{ "PI9",  9,  9, { "gpio-in", "gpio_out",    "rgmii",      "uart3",  "ts0", "spi2"        ,  "pi_eint9" }, 6,  9, 9 },
	{ "PI10", 9, 10, { "gpio-in", "gpio_out",    "rgmii",      "uart3",  "ts0", "spi2"        , "pi_eint10" }, 6, 10, 9 },
	{ "PI11", 9, 11, { "gpio-in", "gpio_out",    "rgmii",      "uart3",  "ts0", "pwm1"        , "pi_eint11" }, 6, 11, 9 },
	{ "PI12", 9, 12, { "gpio-in", "gpio_out",    "rgmii",      "uart3",  "ts0", "pwm2"        , "pi_eint12" }, 6, 12, 9 },
	{ "PI13", 9, 13, { "gpio-in", "gpio_out",    "rgmii",      "uart4",  "ts0", "pwm3"        , "pi_eint13" }, 6, 13, 9 },
	{ "PI14", 9, 14, { "gpio-in", "gpio_out",      "mdc",      "uart4",  "ts0", "pwm4"        , "pi_eint14" }, 6, 14, 9 },
	{ "PI15", 9, 15, { "gpio-in", "gpio_out",     "mdio",      "uart4",  "ts0", "clk_fanout0" , "pi_eint15" }, 6, 15, 9 },
	{ "PI16", 9, 16, { "gpio-in", "gpio_out", "ephy_25m",      "uart4",  "ts0", "clk_fanout1" , "pi_eint16" }, 6, 16, 9 },
};

const struct allwinner_padconf h616_padconf = {
	.npins = nitems(h616_pins),
	.pins = h616_pins,
};
