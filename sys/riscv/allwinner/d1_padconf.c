/*-
 * Copyright (c) 2022 Julien Cassette <julien.cassette@gmail.com>
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

static const struct allwinner_pins d1_pins[] = {
	{ "PB0",  1,  0, { "gpio_in", "gpio_out", "pwm3",  "ir",    "i2c2",  "spi1",  "uart0", "uart2", "spdif", [14] = "pb_eint0"  }, 14,  0, 1 },
	{ "PB1",  1,  1, { "gpio_in", "gpio_out", "pwm4",  "i2s2",  "i2c2",  "i2s2",  "uart0", "uart2", "ir",    [14] = "pb_eint1"  }, 14,  1, 1 },
	{ "PB2",  1,  2, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c0",  "i2s2",  "lcd0",  "uart4", NULL,    [14] = "pb_eint2"  }, 14,  2, 1 },
	{ "PB3",  1,  3, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c0",  "i2s2",  "lcd0",  "uart4", NULL,    [14] = "pb_eint3"  }, 14,  3, 1 },
	{ "PB4",  1,  4, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c1",  "i2s2",  "lcd0",  "uart5", NULL,    [14] = "pb_eint4"  }, 14,  4, 1 },
	{ "PB5",  1,  5, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c1",  "pwm0",  "lcd0",  "uart5", NULL,    [14] = "pb_eint5"  }, 14,  5, 1 },
	{ "PB6",  1,  6, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c3",  "pwm1",  "lcd0",  "uart3", "cpu",   [14] = "pb_eint6"  }, 14,  6, 1 },
	{ "PB7",  1,  7, { "gpio_in", "gpio_out", "lcd0",  "i2s2",  "i2c3",  "ir",    "lcd0",  "uart3", "cpu",   [14] = "pb_eint7"  }, 14,  7, 1 },
	{ "PB8",  1,  8, { "gpio_in", "gpio_out", "dmic",  "pwm5",  "i2c2",  "spi1",  "uart0", "uart1", NULL,    [14] = "pb_eint8"  }, 14,  8, 1 },
	{ "PB9",  1,  9, { "gpio_in", "gpio_out", "dmic",  "pwm6",  "i2c2",  "spi1",  "uart0", "uart1", NULL,    [14] = "pb_eint9"  }, 14,  9, 1 },
	{ "PB10", 1, 10, { "gpio_in", "gpio_out", "dmic",  "pwm7",  "i2c0",  "spi1",  "clk",   "uart1", NULL,    [14] = "pb_eint10" }, 14, 10, 1 },
	{ "PB11", 1, 11, { "gpio_in", "gpio_out", "dmic",  "pwm2",  "i2c0",  "spi1",  "clk",   "uart1", NULL,    [14] = "pb_eint11" }, 14, 11, 1 },
	{ "PB12", 1, 12, { "gpio_in", "gpio_out", "dmic",  "pwm0",  "spdif", "spi1",  "clk",   "ir",    NULL,    [14] = "pb_eint12" }, 14, 12, 1 },
	{ "PC0",  2,  0, { "gpio_in", "gpio_out", "uart2", "i2c2",  "ledc",  NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint0"  }, 14,  0, 2 },
	{ "PC1",  2,  1, { "gpio_in", "gpio_out", "uart2", "i2c2",  NULL,    NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint1"  }, 14,  1, 2 },
	{ "PC2",  2,  2, { "gpio_in", "gpio_out", "spi0",  "mmc2",  NULL,    NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint2"  }, 14,  2, 2 },
	{ "PC3",  2,  3, { "gpio_in", "gpio_out", "spi0",  "mmc2",  NULL,    NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint3"  }, 14,  3, 2 },
	{ "PC4",  2,  4, { "gpio_in", "gpio_out", "spi0",  "mmc2",  "boot",  NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint4"  }, 14,  4, 2 },
	{ "PC5",  2,  5, { "gpio_in", "gpio_out", "spi0",  "mmc2",  "boot",  NULL,    NULL,    NULL,    NULL,    [14] = "pc_eint5"  }, 14,  5, 2 },
	{ "PC6",  2,  6, { "gpio_in", "gpio_out", "spi0",  "mmc2",  "uart3", "i2c3",  "dbg",   NULL,    NULL,    [14] = "pc_eint6"  }, 14,  6, 2 },
	{ "PC7",  2,  7, { "gpio_in", "gpio_out", "spi0",  "mmc2",  "uart3", "i2c3",  "tcon",  NULL,    NULL,    [14] = "pc_eint7"  }, 14,  7, 2 },
	{ "PD0",  3,  0, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "i2c0",  NULL,    NULL,    NULL,    [14] = "pd_eint0"  }, 14,  0, 3 },
	{ "PD1",  3,  1, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart2", NULL,    NULL,    NULL,    [14] = "pd_eint1"  }, 14,  1, 3 },
	{ "PD2",  3,  2, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart2", NULL,    NULL,    NULL,    [14] = "pd_eint2"  }, 14,  2, 3 },
	{ "PD3",  3,  3, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart2", NULL,    NULL,    NULL,    [14] = "pd_eint3"  }, 14,  3, 3 },
	{ "PD4",  3,  4, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart2", NULL,    NULL,    NULL,    [14] = "pd_eint4"  }, 14,  4, 3 },
	{ "PD5",  3,  5, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart5", NULL,    NULL,    NULL,    [14] = "pd_eint5"  }, 14,  5, 3 },
	{ "PD6",  3,  6, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart5", NULL,    NULL,    NULL,    [14] = "pd_eint6"  }, 14,  6, 3 },
	{ "PD7",  3,  7, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart4", NULL,    NULL,    NULL,    [14] = "pd_eint7"  }, 14,  7, 3 },
	{ "PD8",  3,  8, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "uart4", NULL,    NULL,    NULL,    [14] = "pd_eint8"  }, 14,  8, 3 },
	{ "PD9",  3,  9, { "gpio_in", "gpio_out", "lcd0",  "lvds0", "dsi",   "pwm6",  NULL,    NULL,    NULL,    [14] = "pd_eint9"  }, 14,  9, 3 },
	{ "PD10", 3, 10, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "uart3", NULL,    NULL,    NULL,    [14] = "pd_eint10" }, 14, 10, 3 },
	{ "PD11", 3, 11, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "uart3", NULL,    NULL,    NULL,    [14] = "pd_eint11" }, 14, 11, 3 },
	{ "PD12", 3, 12, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "i2c0",  NULL,    NULL,    NULL,    [14] = "pd_eint12" }, 14, 12, 3 },
	{ "PD13", 3, 13, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "uart3", NULL,    NULL,    NULL,    [14] = "pd_eint13" }, 14, 13, 3 },
	{ "PD14", 3, 14, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "uart3", NULL,    NULL,    NULL,    [14] = "pd_eint14" }, 14, 14, 3 },
	{ "PD15", 3, 15, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "spi1",  "ir",    NULL,    NULL,    NULL,    [14] = "pd_eint15" }, 14, 15, 3 },
	{ "PD16", 3, 16, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "dmic",  "pwm0",  NULL,    NULL,    NULL,    [14] = "pd_eint16" }, 14, 16, 3 },
	{ "PD17", 3, 17, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "dmic",  "pwm1",  NULL,    NULL,    NULL,    [14] = "pd_eint17" }, 14, 17, 3 },
	{ "PD18", 3, 18, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "dmic",  "pwm2",  NULL,    NULL,    NULL,    [14] = "pd_eint18" }, 14, 18, 3 },
	{ "PD19", 3, 19, { "gpio_in", "gpio_out", "lcd0",  "lvds1", "dmic",  "pwm3",  NULL,    NULL,    NULL,    [14] = "pd_eint19" }, 14, 19, 3 },
	{ "PD20", 3, 20, { "gpio_in", "gpio_out", "lcd0",  "i2c2",  "dmic",  "pwm4",  NULL,    NULL,    NULL,    [14] = "pd_eint20" }, 14, 20, 3 },
	{ "PD21", 3, 21, { "gpio_in", "gpio_out", "lcd0",  "i2c2",  "uart1", "pwm5",  NULL,    NULL,    NULL,    [14] = "pd_eint21" }, 14, 21, 3 },
	{ "PD22", 3, 22, { "gpio_in", "gpio_out", "spdif", "ir",    "uart1", "pwm7",  NULL,    NULL,    NULL,    [14] = "pd_eint22" }, 14, 22, 3 },
	{ "PE0",  4,  0, { "gpio_in", "gpio_out", "ncsi0", "uart2", "i2c1",  "lcd0",  NULL,    NULL,    "emac",  [14] = "pe_eint0"  }, 14,  0, 4 },
	{ "PE1",  4,  1, { "gpio_in", "gpio_out", "ncsi0", "uart2", "i2c1",  "lcd0",  NULL,    NULL,    "emac",  [14] = "pe_eint1"  }, 14,  1, 4 },
	{ "PE2",  4,  2, { "gpio_in", "gpio_out", "ncsi0", "uart2", "i2c0",  "clk",   "uart0", NULL,    "emac",  [14] = "pe_eint2"  }, 14,  2, 4 },
	{ "PE3",  4,  3, { "gpio_in", "gpio_out", "ncsi0", "uart2", "i2c0",  "clk",   "uart0", NULL,    "emac",  [14] = "pe_eint3"  }, 14,  3, 4 },
	{ "PE4",  4,  4, { "gpio_in", "gpio_out", "ncsi0", "uart4", "i2c2",  "clk",   "jtag",  "jtag",  "emac",  [14] = "pe_eint4"  }, 14,  4, 4 },
	{ "PE5",  4,  5, { "gpio_in", "gpio_out", "ncsi0", "uart4", "i2c2",  "ledc",  "jtag",  "jtag",  "emac",  [14] = "pe_eint5"  }, 14,  5, 4 },
	{ "PE6",  4,  6, { "gpio_in", "gpio_out", "ncsi0", "uart5", "i2c3",  "spdif", "jtag",  "jtag",  "emac",  [14] = "pe_eint6"  }, 14,  6, 4 },
	{ "PE7",  4,  7, { "gpio_in", "gpio_out", "ncsi0", "uart5", "i2c3",  "spdif", "jtag",  "jtag",  "emac",  [14] = "pe_eint7"  }, 14,  7, 4 },
	{ "PE8",  4,  8, { "gpio_in", "gpio_out", "ncsi0", "uart1", "pwm2",  "uart3", "jtag",  NULL,    "emac",  [14] = "pe_eint8"  }, 14,  8, 4 },
	{ "PE9",  4,  9, { "gpio_in", "gpio_out", "ncsi0", "uart1", "pwm3",  "uart3", "jtag",  NULL,    "emac",  [14] = "pe_eint9"  }, 14,  9, 4 },
	{ "PE10", 4, 10, { "gpio_in", "gpio_out", "ncsi0", "uart1", "pwm4",  "ir",    "jtag",  NULL,    "emac",  [14] = "pe_eint10" }, 14, 10, 4 },
	{ "PE11", 4, 11, { "gpio_in", "gpio_out", "ncsi0", "uart1", "i2s0",  "i2s0",  "jtag",  NULL,    "emac",  [14] = "pe_eint11" }, 14, 11, 4 },
	{ "PE12", 4, 12, { "gpio_in", "gpio_out", "i2c2",  "ncsi0", "i2s0",  "i2s0",  NULL,    NULL,    "emac",  [14] = "pe_eint12" }, 14, 12, 4 },
	{ "PE13", 4, 13, { "gpio_in", "gpio_out", "i2c2",  "pwm5",  "i2s0",  "i2s0",  "dmic",  NULL,    "emac",  [14] = "pe_eint13" }, 14, 13, 4 },
	{ "PE14", 4, 14, { "gpio_in", "gpio_out", "i2c1",  "jtag",  "i2s0",  "i2s0",  "dmic",  NULL,    "emac",  [14] = "pe_eint14" }, 14, 14, 4 },
	{ "PE15", 4, 15, { "gpio_in", "gpio_out", "i2c1",  "jtag",  "pwm6",  "i2s0",  "dmic",  NULL,    "emac",  [14] = "pe_eint15" }, 14, 15, 4 },
	{ "PE16", 4, 16, { "gpio_in", "gpio_out", "i2c3",  "jtag",  "pwm7",  "i2s0",  "dmic",  NULL,    NULL,    [14] = "pe_eint16" }, 14, 16, 4 },
	{ "PE17", 4, 17, { "gpio_in", "gpio_out", "i2c3",  "jtag",  "ir",    "i2s0",  "dmic",  NULL,    NULL,    [14] = "pe_eint17" }, 14, 17, 4 },
	{ "PF0",  5,  0, { "gpio_in", "gpio_out", "mmc0",  NULL,    "jtag",  "i2s2",  "i2s2",  NULL,    NULL,    [14] = "pf_eint0"  }, 14,  0, 5 },
	{ "PF1",  5,  1, { "gpio_in", "gpio_out", "mmc0",  NULL,    "jtag",  "i2s2",  "i2s2",  NULL,    NULL,    [14] = "pf_eint1"  }, 14,  1, 5 },
	{ "PF2",  5,  2, { "gpio_in", "gpio_out", "mmc0",  "uart0", "i2c0",  "ledc",  "spdif", NULL,    NULL,    [14] = "pf_eint2"  }, 14,  2, 5 },
	{ "PF3",  5,  3, { "gpio_in", "gpio_out", "mmc0",  NULL,    "jtag",  "i2s2",  NULL,    NULL,    NULL,    [14] = "pf_eint3"  }, 14,  3, 5 },
	{ "PF4",  5,  4, { "gpio_in", "gpio_out", "mmc0",  "uart0", "i2c0",  "pwm6",  "ir",    NULL,    NULL,    [14] = "pf_eint4"  }, 14,  4, 5 },
	{ "PF5",  5,  5, { "gpio_in", "gpio_out", "mmc0",  NULL,    "jtag",  "i2s2",  NULL,    NULL,    NULL,    [14] = "pf_eint5"  }, 14,  5, 5 },
	{ "PF6",  5,  6, { "gpio_in", "gpio_out", NULL,    "spdif", "ir",    "i2s2",  "pwm5",  NULL,    NULL,    [14] = "pf_eint6"  }, 14,  6, 5 },
	{ "PG0",  6,  0, { "gpio_in", "gpio_out", "mmc1",  "uart3", "emac",  "pwm7",  NULL,    NULL,    NULL,    [14] = "pg_eint0"  }, 14,  0, 6 },
	{ "PG1",  6,  1, { "gpio_in", "gpio_out", "mmc1",  "uart3", "emac",  "pwm6",  NULL,    NULL,    NULL,    [14] = "pg_eint1"  }, 14,  1, 6 },
	{ "PG2",  6,  2, { "gpio_in", "gpio_out", "mmc1",  "uart3", "emac",  "uart4", NULL,    NULL,    NULL,    [14] = "pg_eint2"  }, 14,  2, 6 },
	{ "PG3",  6,  3, { "gpio_in", "gpio_out", "mmc1",  "uart3", "emac",  "uart4", NULL,    NULL,    NULL,    [14] = "pg_eint3"  }, 14,  3, 6 },
	{ "PG4",  6,  4, { "gpio_in", "gpio_out", "mmc1",  "uart5", "emac",  "pwm5",  NULL,    NULL,    NULL,    [14] = "pg_eint4"  }, 14,  4, 6 },
	{ "PG5",  6,  5, { "gpio_in", "gpio_out", "mmc1",  "uart5", "emac",  "pwm4",  NULL,    NULL,    NULL,    [14] = "pg_eint5"  }, 14,  5, 6 },
	{ "PG6",  6,  6, { "gpio_in", "gpio_out", "uart1", "i2c2",  "emac",  "pwm1",  NULL,    NULL,    NULL,    [14] = "pg_eint6"  }, 14,  6, 6 },
	{ "PG7",  6,  7, { "gpio_in", "gpio_out", "uart1", "i2c2",  "emac",  "spdif", NULL,    NULL,    NULL,    [14] = "pg_eint7"  }, 14,  7, 6 },
	{ "PG8",  6,  8, { "gpio_in", "gpio_out", "uart1", "i2c1",  "emac",  "uart3", NULL,    NULL,    NULL,    [14] = "pg_eint8"  }, 14,  8, 6 },
	{ "PG9",  6,  9, { "gpio_in", "gpio_out", "uart1", "i2c1",  "emac",  "uart3", NULL,    NULL,    NULL,    [14] = "pg_eint9"  }, 14,  9, 6 },
	{ "PG10", 6, 10, { "gpio_in", "gpio_out", "pwm3",  "i2c3",  "emac",  "clk",   "ir",    NULL,    NULL,    [14] = "pg_eint10" }, 14, 10, 6 },
	{ "PG11", 6, 11, { "gpio_in", "gpio_out", "i2s1",  "i2c3",  "emac",  "clk",   "tcon",  NULL,    NULL,    [14] = "pg_eint11" }, 14, 11, 6 },
	{ "PG12", 6, 12, { "gpio_in", "gpio_out", "i2s1",  "i2c0",  "emac",  "clk",   "pwm0",  "uart1", NULL,    [14] = "pg_eint12" }, 14, 12, 6 },
	{ "PG13", 6, 13, { "gpio_in", "gpio_out", "i2s1",  "i2c0",  "emac",  "pwm2",  "ledc",  "uart1", NULL,    [14] = "pg_eint13" }, 14, 13, 6 },
	{ "PG14", 6, 14, { "gpio_in", "gpio_out", "i2s1",  "i2c2",  "emac",  "i2s1",  "spi0",  "uart1", NULL,    [14] = "pg_eint14" }, 14, 14, 6 },
	{ "PG15", 6, 15, { "gpio_in", "gpio_out", "i2s1",  "i2c2",  "emac",  "i2s1",  "spi0",  "uart1", NULL,    [14] = "pg_eint15" }, 14, 15, 6 },
	{ "PG16", 6, 16, { "gpio_in", "gpio_out", "ir",    "tcon",  "pwm5",  "clk",   "spdif", "ledc",  NULL,    [14] = "pg_eint16" }, 14, 16, 6 },
	{ "PG17", 6, 17, { "gpio_in", "gpio_out", "uart2", "i2c3",  "pwm7",  "clk",   "ir",    "uart0", NULL,    [14] = "pg_eint17" }, 14, 17, 6 },
	{ "PG18", 6, 18, { "gpio_in", "gpio_out", "uart2", "i2c3",  "pwm6",  "clk",   "spdif", "uart0", NULL,    [14] = "pg_eint18" }, 14, 18, 6 },
};

const struct allwinner_padconf d1_padconf = {
	.npins = nitems(d1_pins),
	.pins = d1_pins,
};
