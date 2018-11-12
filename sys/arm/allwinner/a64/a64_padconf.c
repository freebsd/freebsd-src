/*-
 * Copyright (c) 2016 Jared McNeill <jmcneill@invisible.ca>
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
#include <sys/kernel.h>
#include <sys/types.h>

#include <arm/allwinner/allwinner_pinctrl.h>

#include "opt_soc.h"

#ifdef SOC_ALLWINNER_A64

static const struct allwinner_pins a64_pins[] = {
	{ "PB0",  1, 0,   { "gpio_in", "gpio_out", "uart2", NULL, "jtag", NULL, "eint" } },
	{ "PB1",  1, 1,   { "gpio_in", "gpio_out", "uart2", NULL, "jtag", "sim", "eint" } },
	{ "PB2",  1, 2,   { "gpio_in", "gpio_out", "uart2", NULL, "jtag", "sim", "eint" } },
	{ "PB3",  1, 3,   { "gpio_in", "gpio_out", "uart2", "i2s0", "jtag", "sim", "eint" } },
	{ "PB4",  1, 4,   { "gpio_in", "gpio_out", "aif2", "pcm0", NULL, "sim", "eint" } },
	{ "PB5",  1, 5,   { "gpio_in", "gpio_out", "aif2", "pcm0", NULL, "sim", "eint" } },
	{ "PB6",  1, 6,   { "gpio_in", "gpio_out", "aif2", "pcm0", NULL, "sim", "eint" } },
	{ "PB7",  1, 7,   { "gpio_in", "gpio_out", "aif2", "pcm0", NULL, "sim", "eint" } },
	{ "PB8",  1, 8,   { "gpio_in", "gpio_out", NULL, NULL, "uart0", NULL, "eint" } },
	{ "PB9",  1, 9,   { "gpio_in", "gpio_out", NULL, NULL, "uart0", NULL, "eint" } },

	{ "PC0",  2, 0,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC1",  2, 1,   { "gpio_in", "gpio_out", "nand", "mmc2", "spi0" } },
	{ "PC2",  2, 2,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC3",  2, 3,   { "gpio_in", "gpio_out", "nand", NULL, "spi0" } },
	{ "PC4",  2, 4,   { "gpio_in", "gpio_out", "nand" } },
	{ "PC5",  2, 5,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC6",  2, 6,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC7",  2, 7,   { "gpio_in", "gpio_out", "nand" } },
	{ "PC8",  2, 8,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC9",  2, 9,   { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC10", 2, 10,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC11", 2, 11,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC12", 2, 12,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC13", 2, 13,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC14", 2, 14,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC15", 2, 15,  { "gpio_in", "gpio_out", "nand", "mmc2" } },
	{ "PC16", 2, 16,  { "gpio_in", "gpio_out", "nand", "mmc2" } },

	{ "PD0",  3, 0,   { "gpio_in", "gpio_out", "lcd", "uart3", "spi1", "ccir" } },
	{ "PD1",  3, 1,   { "gpio_in", "gpio_out", "lcd", "uart3", "spi1", "ccir" } },
	{ "PD2",  3, 2,   { "gpio_in", "gpio_out", "lcd", "uart4", "spi1", "ccir" } },
	{ "PD3",  3, 3,   { "gpio_in", "gpio_out", "lcd", "uart4", "spi1", "ccir" } },
	{ "PD4",  3, 4,   { "gpio_in", "gpio_out", "lcd", "uart4", "spi1", "ccir" } },
	{ "PD5",  3, 5,   { "gpio_in", "gpio_out", "lcd", "uart4", "spi1", "ccir" } },
	{ "PD6",  3, 6,   { "gpio_in", "gpio_out", "lcd", NULL, NULL, "ccir" } },
	{ "PD7",  3, 7,   { "gpio_in", "gpio_out", "lcd", NULL, NULL, "ccir" } },
	{ "PD8",  3, 8,   { "gpio_in", "gpio_out", "lcd", NULL, "emac", "ccir" } },
	{ "PD9",  3, 9,   { "gpio_in", "gpio_out", "lcd", NULL, "emac", "ccir" } },
	{ "PD10", 3, 10,  { "gpio_in", "gpio_out", "lcd", NULL, "emac" } },
	{ "PD11", 3, 11,  { "gpio_in", "gpio_out", "lcd", NULL, "emac" } },
	{ "PD12", 3, 12,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD13", 3, 13,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD14", 3, 14,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD15", 3, 15,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac", "ccir" } },
	{ "PD16", 3, 16,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac", "ccir" } },
	{ "PD17", 3, 17,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD18", 3, 18,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD19", 3, 19,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD20", 3, 20,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD21", 3, 21,  { "gpio_in", "gpio_out", "lcd", "lvds", "emac" } },
	{ "PD22", 3, 22,  { "gpio_in", "gpio_out", "pwm0", NULL, "emac" } },
	{ "PD23", 3, 23,  { "gpio_in", "gpio_out", NULL, NULL, "emac" } },
	{ "PD24", 3, 24,  { "gpio_in", "gpio_out" } },

	{ "PE0",  4, 0,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE1",  4, 1,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE2",  4, 2,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE3",  4, 3,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE4",  4, 4,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE5",  4, 5,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE6",  4, 6,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE7",  4, 7,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE8",  4, 8,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE9",  4, 9,   { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE10", 4, 10,  { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE11", 4, 11,  { "gpio_in", "gpio_out", "csi", NULL, "ts" } },
	{ "PE12", 4, 12,  { "gpio_in", "gpio_out", "csi" } },
	{ "PE13", 4, 13,  { "gpio_in", "gpio_out", "csi" } },
	{ "PE14", 4, 14,  { "gpio_in", "gpio_out", "pll_lock", "twi2" } },
	{ "PE15", 4, 15,  { "gpio_in", "gpio_out", NULL, "twi2" } },
	{ "PE16", 4, 16,  { "gpio_in", "gpio_out" } },
	{ "PE17", 4, 17,  { "gpio_in", "gpio_out" } },

	{ "PF0",  5, 0,   { "gpio_in", "gpio_out", "mmc0", "jtag" } },
	{ "PF1",  5, 1,   { "gpio_in", "gpio_out", "mmc0", "jtag" } },
	{ "PF2",  5, 2,   { "gpio_in", "gpio_out", "mmc0", "uart0" } },
	{ "PF3",  5, 3,   { "gpio_in", "gpio_out", "mmc0", "jtag" } },
	{ "PF4",  5, 4,   { "gpio_in", "gpio_out", "mmc0", "uart0" } },
	{ "PF5",  5, 5,   { "gpio_in", "gpio_out", "mmc0", "jtag" } },
	{ "PF6",  5, 6,   { "gpio_in", "gpio_out" } },

	{ "PG0",  6, 0,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG1",  6, 1,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG2",  6, 2,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG3",  6, 3,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG4",  6, 4,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG5",  6, 5,   { "gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "eint" } },
	{ "PG6",  6, 6,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "eint" } },
	{ "PG7",  6, 7,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "eint" } },
	{ "PG8",  6, 8,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "eint" } },
	{ "PG9",  6, 9,   { "gpio_in", "gpio_out", "uart1", NULL, NULL, NULL, "eint" } },
	{ "PG10", 6, 10,  { "gpio_in", "gpio_out", "aif3", "pcm1", NULL, NULL, "eint" } },
	{ "PG11", 6, 11,  { "gpio_in", "gpio_out", "aif3", "pcm1", NULL, NULL, "eint" } },
	{ "PG12", 6, 12,  { "gpio_in", "gpio_out", "aif3", "pcm1", NULL, NULL, "eint" } },
	{ "PG13", 6, 13,  { "gpio_in", "gpio_out", "aif3", "pcm1", NULL, NULL, "eint" } },

	{ "PH0",  7, 0,   { "gpio_in", "gpio_out", "i2c0", NULL, NULL, NULL, "eint" } },
	{ "PH1",  7, 1,   { "gpio_in", "gpio_out", "i2c0", NULL, NULL, NULL, "eint" } },
	{ "PH2",  7, 2,   { "gpio_in", "gpio_out", "i2c1", NULL, NULL, NULL, "eint" } },
	{ "PH3",  7, 3,   { "gpio_in", "gpio_out", "i2c1", NULL, NULL, NULL, "eint" } },
	{ "PH4",  7, 4,   { "gpio_in", "gpio_out", "uart3", NULL, NULL, NULL, "eint" } },
	{ "PH5",  7, 5,   { "gpio_in", "gpio_out", "uart3", NULL, NULL, NULL, "eint" } },
	{ "PH6",  7, 6,   { "gpio_in", "gpio_out", "uart3", NULL, NULL, NULL, "eint" } },
	{ "PH7",  7, 7,   { "gpio_in", "gpio_out", "uart3", NULL, NULL, NULL, "eint" } },
	{ "PH8",  7, 8,   { "gpio_in", "gpio_out", "owa", NULL, NULL, NULL, "eint" } },
	{ "PH9",  7, 9,   { "gpio_in", "gpio_out", NULL, NULL, NULL, NULL, "eint" } },
	{ "PH10", 7, 10,  { "gpio_in", "gpio_out", "mic", NULL, NULL, NULL, "eint" } },
	{ "PH11", 7, 11,  { "gpio_in", "gpio_out", "mic", NULL, NULL, NULL, "eint" } },
};

const struct allwinner_padconf a64_padconf = {
	.npins = nitems(a64_pins),
	.pins = a64_pins,
};

#endif /* !SOC_ALLWINNER_A64 */
