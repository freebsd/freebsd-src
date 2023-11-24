/*-
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
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <arm/allwinner/allwinner_pinctrl.h>

#include "opt_soc.h"

static const struct allwinner_pins h6_pins[] = {
	{ "PC0",  2, 0,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC1",  2, 1,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC2",  2, 2,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC3",  2, 3,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC4",  2, 4,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC5",  2, 5,   { "gpio_in", "gpio_out", "nand", "mmc2", "spi0" } },
	{ "PC6",  2, 6,   { "gpio_in", "gpio_out", "nand", "mmc2", "spi0" } },
	{ "PC7",  2, 7,   { "gpio_in", "gpio_out", "nand", "mmc2", "spi0" } },
	{ "PC8",  2, 8,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC9",  2, 9,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC10", 2, 10,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC11", 2, 11,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC12", 2, 12,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC13", 2, 13,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC14", 2, 14,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC15", 2, 15,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC16", 2, 16,  { "gpio_in", "gpio_out", "nand",  } },

	{ "PD0",  3, 0,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD1",  3, 1,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD2",  3, 2,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD3",  3, 3,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD4",  3, 4,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD5",  3, 5,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD6",  3, 6,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD7",  3, 7,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD8",  3, 8,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD9",  3, 9,   { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD10", 3, 10,  { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD11", 3, 11,  { "gpio_in", "gpio_out", "lcd0", "ts0", "csi", "emac" } },
	{ "PD12", 3, 12,  { "gpio_in", "gpio_out", "lcd0", "ts1", "csi", "emac" } },
	{ "PD13", 3, 13,  { "gpio_in", "gpio_out", "lcd0", "ts1", "csi", "emac" } },
	{ "PD14", 3, 14,  { "gpio_in", "gpio_out", "lcd0", "ts1", "dmic", "csi" } },
	{ "PD15", 3, 15,  { "gpio_in", "gpio_out", "lcd0", "ts1", "dmic", "csi" } },
	{ "PD16", 3, 16,  { "gpio_in", "gpio_out", "lcd0", "ts1", "dmic" } },
	{ "PD17", 3, 17,  { "gpio_in", "gpio_out", "lcd0", "ts2", "dmic" } },
	{ "PD18", 3, 18,  { "gpio_in", "gpio_out", "lcd0", "ts2", "dmic" } },
	{ "PD19", 3, 19,  { "gpio_in", "gpio_out", "lcd0", "ts2", "uart2", "emac" } },
	{ "PD20", 3, 20,  { "gpio_in", "gpio_out", "lcd0", "ts2", "uart2", "emac" } },
	{ "PD21", 3, 21,  { "gpio_in", "gpio_out", "lcd0", "ts2", "uart2" } },
	{ "PD22", 3, 22,  { "gpio_in", "gpio_out", "pwm0", "ts3", "uart2" } },
	{ "PD23", 3, 23,  { "gpio_in", "gpio_out", "i2c2", "ts3", "uart3", "jtag" } }, 
	{ "PD24", 3, 24,  { "gpio_in", "gpio_out", "i2c2", "ts3", "uart3", "jtag" } },
	{ "PD25", 3, 25,  { "gpio_in", "gpio_out", "i2c0", "ts3", "uart3", "jtag" } },
	{ "PD26", 3, 26,  { "gpio_in", "gpio_out", "i2c0", "ts3", "uart3", "jtag" } },

	{ "PF0",  5, 0,   { "gpio_in", "gpio_out", "mmc0", "jtag", NULL, NULL, "pf_eint0" }, 6, 0, 5 },
	{ "PF1",  5, 1,   { "gpio_in", "gpio_out", "mmc0", "jtag", NULL, NULL, "pf_eint1" }, 6, 1, 5 },
	{ "PF2",  5, 2,   { "gpio_in", "gpio_out", "mmc0", "uart0", NULL, NULL, "pf_eint2" }, 6, 2, 5 },
	{ "PF3",  5, 3,   { "gpio_in", "gpio_out", "mmc0", "jtag", NULL, NULL, "pf_eint3" }, 6, 3, 5 },
	{ "PF4",  5, 4,   { "gpio_in", "gpio_out", "mmc0", "uart0", NULL, NULL, "pf_eint4" }, 6, 4, 5 },
	{ "PF5",  5, 5,   { "gpio_in", "gpio_out", "mmc0", "jtag", NULL, NULL, "pf_eint5" }, 6, 5, 5 },
	{ "PF6",  5, 6,   { "gpio_in", "gpio_out", NULL, NULL, NULL, NULL, "pf_eint6" }, 6, 6, 5 },

	{ "PG0",  6, 0,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint0" }, 6, 0, 6},
	{ "PG1",  6, 1,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint1" }, 6, 1, 6},
	{ "PG2",  6, 2,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint2" }, 6, 2, 6},
	{ "PG3",  6, 3,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint3" }, 6, 3, 6},
	{ "PG4",  6, 4,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint4" }, 6, 4, 6},
	{ "PG5",  6, 5,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint5" }, 6, 5, 6},
	{ "PG6",  6, 6,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "pg_eint6" }, 6, 6, 6},
	{ "PG7",  6, 7,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "pg_eint7" }, 6, 7, 6},
	{ "PG8",  6, 8,   { "gpio_in", "gpio_out", "uart1", NULL, "sim0", NULL, "pg_eint8" }, 6, 8, 6},
	{ "PG9",  6, 9,   { "gpio_in", "gpio_out", "uart1", NULL, "sim0", NULL, "pg_eint9" }, 6, 9, 6},
	{ "PG10", 6, 10,  { "gpio_in", "gpio_out", "i2s2", "h_i2s2", "sim0", NULL, "pg_eint10" }, 6, 10, 6},
	{ "PG11", 6, 11,  { "gpio_in", "gpio_out", "i2s2", "h_i2s2", "sim0", NULL, "pg_eint11" }, 6, 11, 6},
	{ "PG12", 6, 12,  { "gpio_in", "gpio_out", "i2s2", "h_i2s2", "sim0", NULL, "pg_eint12" }, 6, 12, 6},
	{ "PG13", 6, 13,  { "gpio_in", "gpio_out", "i2s2", "h_i2s2", "sim0", NULL, "pg_eint13" }, 6, 13, 6},
	{ "PG14", 6, 14,  { "gpio_in", "gpio_out", "i2s2", "h_i2s2", "sim0", NULL, "pg_eint14" }, 6, 13, 6},

	{ "PH0",  7, 0,   { "gpio_in", "gpio_out", "uart0", "i2s0", "h_i2s0", "sim1", "ph_eint0" }, 6, 0, 7},
	{ "PH1",  7, 1,   { "gpio_in", "gpio_out", "uart0", "i2s0", "h_i2s0", "sim1", "ph_eint1" }, 6, 1, 7},
	{ "PH2",  7, 2,   { "gpio_in", "gpio_out", "cir", "i2s0", "h_i2s0", "sim1", "ph_eint2" }, 6, 2, 7},
	{ "PH3",  7, 3,   { "gpio_in", "gpio_out", "spi1", "i2s0", "h_i2s0", "sim1", "ph_eint3" }, 6, 3, 7},
	{ "PH4",  7, 4,   { "gpio_in", "gpio_out", "spi1", "i2s0", "h_i2s0", "sim1", "ph_eint4" }, 6, 4, 7},
	{ "PH5",  7, 5,   { "gpio_in", "gpio_out", "spi1", "spdif", "i2c1", "sim1", "ph_eint5" }, 6, 5, 7},
	{ "PH6",  7, 6,   { "gpio_in", "gpio_out", "spi1", "spdif", "i2c1", "sim1", "ph_eint6" }, 6, 6, 7},
	{ "PH7",  7, 7,   { "gpio_in", "gpio_out", NULL, "spdif", NULL, NULL, "ph_eint7" }, 6, 7, 7},
	{ "PH8",  7, 8,   { "gpio_in", "gpio_out", "hdmi", NULL, NULL, NULL, "ph_eint8" }, 6, 8, 7},
	{ "PH9",  7, 9,   { "gpio_in", "gpio_out", "hdmi", NULL, NULL, NULL, "ph_eint9" }, 6, 9, 7},
	{ "PH10", 7, 10,  { "gpio_in", "gpio_out", "hdmi", NULL, NULL, NULL, "ph_eint10" }, 6, 10, 7},
};

const struct allwinner_padconf h6_padconf = {
	.npins = nitems(h6_pins),
	.pins = h6_pins,
};
