/*-
 * Copyright (c) 2016 Emmanuel Vadot <manu@freebsd.org>
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

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>

#include <arm/allwinner/allwinner_pinctrl.h>

#ifdef SOC_ALLWINNER_A31S

const static struct allwinner_pins a31s_pins[] = {
	{"PA0",  0, 0,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint0", NULL}, 6, 0, 0},
	{"PA1",  0, 1,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint1", NULL}, 6, 1, 0},
	{"PA2",  0, 2,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint2", NULL}, 6, 2, 0},
	{"PA3",  0, 3,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint3", NULL}, 6, 3, 0},
	{"PA4",  0, 4,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint4", NULL}, 6, 4, 0},
	{"PA5",  0, 5,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint5", NULL}, 6, 5, 0},
	{"PA6",  0, 6,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint6", NULL}, 6, 6, 0},
	{"PA7",  0, 7,  {"gpio_in", "gpio_out", "gmac", NULL, "uart1", NULL, "pa_eint7", NULL}, 6, 7, 0},
	{"PA8",  0, 8,  {"gpio_in", "gpio_out", "gmac", NULL, NULL, NULL, "pa_eint8", NULL}, 6, 8, 0},
	{"PA9",  0, 9,  {"gpio_in", "gpio_out", "gmac", NULL, NULL, NULL, "pa_eint9", NULL}, 6, 9, 0},
	{"PA10", 0, 10, {"gpio_in", "gpio_out", "gmac", NULL, "mmc3", "mmc2", "pa_eint10", NULL}, 6, 10, 0},
	{"PA11", 0, 11, {"gpio_in", "gpio_out", "gmac", NULL, "mmc3", "mmc2", "pa_eint11", NULL}, 6, 11, 0},
	{"PA12", 0, 12, {"gpio_in", "gpio_out", "gmac", NULL, "mmc3", "mmc2", "pa_eint12", NULL}, 6, 12, 0},
	{"PA13", 0, 13, {"gpio_in", "gpio_out", "gmac", NULL, "mmc3", "mmc2", "pa_eint13", NULL}, 6, 13, 0},
	{"PA14", 0, 14, {"gpio_in", "gpio_out", "gmac", NULL, "mmc3", "mmc2", "pa_eint14", NULL}, 6, 14, 0},
	{"PA15", 0, 15, {"gpio_in", "gpio_out", "gmac", NULL, "dmic", NULL, "pa_eint15", NULL}, 6, 15, 0},
	{"PA16", 0, 16, {"gpio_in", "gpio_out", "gmac", NULL, "dmic", NULL, "pa_eint16", NULL}, 6, 16, 0},
	{"PA17", 0, 17, {"gpio_in", "gpio_out", "gmac", NULL, "clk_out_b", NULL, "pa_eint17", NULL}, 6, 17, 0},
	{"PA18", 0, 18, {"gpio_in", "gpio_out", "gmac", NULL, "pwm3", NULL, "pa_eint18", NULL}, 6, 18, 0},
	{"PA19", 0, 19, {"gpio_in", "gpio_out", "gmac", NULL, "pwm3", NULL, "pa_eint19", NULL}, 6, 19, 0},
	{"PA20", 0, 20, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint20", NULL}, 6, 20, 0},
	{"PA21", 0, 21, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint21", NULL}, 6, 21, 0},
	{"PA22", 0, 22, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint22", NULL}, 6, 22, 0},
	{"PA23", 0, 23, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint23", NULL}, 6, 23, 0},
	{"PA24", 0, 24, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint24", NULL}, 6, 24, 0},
	{"PA25", 0, 25, {"gpio_in", "gpio_out", "gmac", NULL, "spi3", NULL, "pa_eint25", NULL}, 6, 25, 0},
	{"PA26", 0, 26, {"gpio_in", "gpio_out", "gmac", NULL, "clk_out_c", NULL, "pa_eint26", NULL}, 6, 26, 0},
	{"PA27", 0, 27, {"gpio_in", "gpio_out", "gmac", NULL, NULL, NULL, "pa_eint27", NULL}, 6, 27, 0},

	{"PB0",  1, 0,  {"gpio_in", "gpio_out", "i2s0", "uart3", NULL , NULL, "pb_eint0", NULL}, 6, 0, 1},
	{"PB1",  1, 1,  {"gpio_in", "gpio_out", "i2s0", NULL, NULL, NULL, "pb_eint1", NULL}, 6, 1, 1},
	{"PB2",  1, 2,  {"gpio_in", "gpio_out", "i2s0", NULL, NULL, NULL, "pb_eint2", NULL}, 6, 2, 1},
	{"PB3",  1, 3,  {"gpio_in", "gpio_out", "i2s0", NULL, NULL, NULL, "pb_eint3", NULL}, 6, 3, 1},
	{"PB4",  1, 4,  {"gpio_in", "gpio_out", "i2s0", "uart3", NULL, NULL, "pb_eint4", NULL}, 6, 4, 1},
	{"PB5",  1, 5,  {"gpio_in", "gpio_out", "i2s0", "uart3", "i2c3", NULL, "pb_eint5", NULL}, 6, 5, 1},
	{"PB6",  1, 6,  {"gpio_in", "gpio_out", "i2s0", "uart3", "i2c3", NULL, "pb_eint6", NULL}, 6, 6, 1},
	{"PB7",  1, 7,  {"gpio_in", "gpio_out", "i2s0", NULL, NULL, NULL, "pb_eint7", NULL}, 6, 7, 1},

	{"PC0",  2, 0,  {"gpio_in", "gpio_out", "nand0", "spi0", NULL, NULL, NULL, NULL}},
	{"PC1",  2, 1,  {"gpio_in", "gpio_out", "nand0", "spi0", NULL, NULL, NULL, NULL}},
	{"PC2",  2, 2,  {"gpio_in", "gpio_out", "nand0", "spi0", NULL, NULL, NULL, NULL}},
	{"PC3",  2, 3,  {"gpio_in", "gpio_out", "nand0", NULL, NULL, NULL, NULL, NULL}},
	{"PC4",  2, 4,  {"gpio_in", "gpio_out", "nand0", NULL, NULL, NULL, NULL, NULL}},
	{"PC5",  2, 5,  {"gpio_in", "gpio_out", "nand0", NULL, NULL, NULL, NULL, NULL}},
	{"PC6",  2, 6,  {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC7",  2, 7,  {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC8",  2, 8,  {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC9",  2, 9,  {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC10", 2, 10, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC11", 2, 11, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC12", 2, 12, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC13", 2, 13, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC14", 2, 14, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC15", 2, 15, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC24", 2, 24, {"gpio_in", "gpio_out", "nand0", "mmc2", "mmc3", NULL, NULL, NULL}},
	{"PC25", 2, 25, {"gpio_in", "gpio_out", "nand0", NULL, NULL, NULL, NULL, NULL}},
	{"PC26", 2, 26, {"gpio_in", "gpio_out", "nand0", NULL, NULL, NULL, NULL, NULL}},
	{"PC27", 2, 27, {"gpio_in", "gpio_out", NULL, "spi0",NULL, NULL, NULL, NULL}},

	{"PD0",  3, 0,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD1",  3, 1,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD2",  3, 2,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD3",  3, 3,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD4",  3, 4,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD5",  3, 5,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD6",  3, 6,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD7",  3, 7,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD8",  3, 8,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD9",  3, 9,  {"gpio_in", "gpio_out", "lcd0", "lvds0", NULL, NULL, NULL, NULL}},
	{"PD10", 3, 10, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD11", 3, 11, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD12", 3, 12, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD13", 3, 13, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD14", 3, 14, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD15", 3, 15, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD16", 3, 16, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD17", 3, 17, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD18", 3, 18, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD19", 3, 19, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD20", 3, 20, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD21", 3, 21, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD22", 3, 22, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD23", 3, 23, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD24", 3, 24, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD25", 3, 25, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD26", 3, 26, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},
	{"PD27", 3, 27, {"gpio_in", "gpio_out", "lcd0", NULL, NULL, NULL, NULL, NULL}},

	{"PE0",  4, 0,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint0", NULL}, 6, 0, 4},
	{"PE1",  4, 1,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint1", NULL}, 6, 1, 4},
	{"PE2",  4, 2,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint2", NULL}, 6, 2, 4},
	{"PE3",  4, 3,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint3", NULL}, 6, 3, 4},
	{"PE4",  4, 4,  {"gpio_in", "gpio_out", "csi", "uart5", NULL, NULL, "pe_eint4", NULL}, 6, 4, 4},
	{"PE5",  4, 5,  {"gpio_in", "gpio_out", "csi", "uart5", NULL, NULL, "pe_eint5", NULL}, 6, 5, 4},
	{"PE6",  4, 6,  {"gpio_in", "gpio_out", "csi", "uart5", NULL, NULL, "pe_eint6", NULL}, 6, 6, 4},
	{"PE7",  4, 7,  {"gpio_in", "gpio_out", "csi", "uart5", NULL, NULL, "pe_eint7", NULL}, 6, 7, 4},
	{"PE8",  4, 8,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint8", NULL}, 6, 8, 4},
	{"PE9",  4, 9,  {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint9", NULL}, 6, 9, 4},
	{"PE10", 4, 10, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint10", NULL}, 6, 10, 4},
	{"PE11", 4, 11, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint11", NULL}, 6, 11, 4},
	{"PE12", 4, 12, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint12", NULL}, 6, 12, 4},
	{"PE13", 4, 13, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint13", NULL}, 6, 13, 4},
	{"PE14", 4, 14, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint14", NULL}, 6, 14, 4},
	{"PE15", 4, 15, {"gpio_in", "gpio_out", "csi", "ts", NULL, NULL, "pe_eint15", NULL}, 6, 15, 4},

	{"PF0",  5, 0,  {"gpio_in", "gpio_out", "mmc0", NULL, "jtag", NULL, NULL, NULL}},
	{"PF1",  5, 1,  {"gpio_in", "gpio_out", "mmc0", NULL, "jtag", NULL, NULL, NULL}},
	{"PF2",  5, 2,  {"gpio_in", "gpio_out", "mmc0", NULL, "uart0", NULL, NULL, NULL}},
	{"PF3",  5, 3,  {"gpio_in", "gpio_out", "mmc0", NULL, "jtag", NULL, NULL, NULL}},
	{"PF4",  5, 4,  {"gpio_in", "gpio_out", "mmc0", NULL, "uart0", NULL, NULL, NULL}},
	{"PF5",  5, 5,  {"gpio_in", "gpio_out", "mmc0", NULL, "jtag", NULL, NULL, NULL}},

	{"PG0",  6, 0,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint0", NULL}, 6, 0, 6},
	{"PG1",  6, 1,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint1", NULL}, 6, 1, 6},
	{"PG2",  6, 2,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint2", NULL}, 6, 2, 6},
	{"PG3",  6, 3,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint3", NULL}, 6, 3, 6},
	{"PG4",  6, 4,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint4", NULL}, 6, 4, 6},
	{"PG5",  6, 5,  {"gpio_in", "gpio_out", "mmc1", NULL, NULL, NULL, "pg_eint5", NULL}, 6, 5, 6},
	{"PG6",  6, 6,  {"gpio_in", "gpio_out", "uart2", NULL, NULL, NULL, "pg_eint6", NULL}, 6, 6, 6},
	{"PG7",  6, 7,  {"gpio_in", "gpio_out", "uart2", NULL, NULL, NULL, "pg_eint7", NULL}, 6, 7, 6},
	{"PG8",  6, 8,  {"gpio_in", "gpio_out", "uart2", NULL, NULL, NULL, "pg_eint8", NULL}, 6, 8, 6},
	{"PG9",  6, 9,  {"gpio_in", "gpio_out", "uart2", NULL, NULL, NULL, "pg_eint9", NULL}, 6, 9, 6},
	{"PG10", 6, 10, {"gpio_in", "gpio_out", "i2c3", NULL, NULL, NULL, "pg_eint10", NULL}, 6, 10, 6},
	{"PG11", 6, 11, {"gpio_in", "gpio_out", "i2c3", NULL, NULL, NULL, "pg_eint11", NULL}, 6, 11, 6},
	{"PG12", 6, 12, {"gpio_in", "gpio_out", "spi1", "i2s1", NULL, NULL, "pg_eint12", NULL}, 6, 12, 6},
	{"PG13", 6, 13, {"gpio_in", "gpio_out", "spi1", "i2s1", NULL, NULL, "pg_eint13", NULL}, 6, 13, 6},
	{"PG14", 6, 14, {"gpio_in", "gpio_out", "spi1", "i2s1", NULL, NULL, "pg_eint14", NULL}, 6, 14, 6},
	{"PG15", 6, 15, {"gpio_in", "gpio_out", "spi1", "i2s1", NULL, NULL, "pg_eint15", NULL}, 6, 15, 6},
	{"PG16", 6, 16, {"gpio_in", "gpio_out", "spi1", "i2s1", NULL, NULL, "pg_eint16", NULL}, 6, 16, 6},
	{"PG17", 6, 17, {"gpio_in", "gpio_out", "uart4", NULL, NULL, NULL, "pg_eint17", NULL}, 6, 17, 6},
	{"PG18", 6, 18, {"gpio_in", "gpio_out", "uart4", NULL, NULL, NULL, "pg_eint18", NULL}, 6, 18, 6},

	{"PH9",  7, 9,  {"gpio_in", "gpio_out", "spi2", "jtag", "pwm1", NULL, NULL, NULL}},
	{"PH10", 7, 10, {"gpio_in", "gpio_out", "spi2", "jtag", "pwm1", NULL, NULL, NULL}},
	{"PH11", 7, 11, {"gpio_in", "gpio_out", "spi2", "jtag", "pwm2", NULL, NULL, NULL}},
	{"PH12", 7, 12, {"gpio_in", "gpio_out", "spi2", "jtag", "pwm2", NULL, NULL, NULL}},
	{"PH13", 7, 13, {"gpio_in", "gpio_out", "pwm0", NULL, NULL, NULL, NULL, NULL}},
	{"PH14", 7, 14, {"gpio_in", "gpio_out", "i2c0", NULL, NULL, NULL, NULL, NULL}},
	{"PH15", 7, 15, {"gpio_in", "gpio_out", "i2c0", NULL, NULL, NULL, NULL, NULL}},
	{"PH16", 7, 16, {"gpio_in", "gpio_out", "i2c1", NULL, NULL, NULL, NULL, NULL}},
	{"PH17", 7, 17, {"gpio_in", "gpio_out", "i2c1", NULL, NULL, NULL, NULL, NULL}},
	{"PH18", 7, 18, {"gpio_in", "gpio_out", "i2c2", NULL, NULL, NULL, NULL, NULL}},
	{"PH19", 7, 19, {"gpio_in", "gpio_out", "i2c2", NULL, NULL, NULL, NULL, NULL}},
	{"PH20", 7, 20, {"gpio_in", "gpio_out", "uart0", NULL, NULL, NULL, NULL, NULL}},
	{"PH21", 7, 21, {"gpio_in", "gpio_out", "uart0", NULL, NULL, NULL, NULL, NULL}},
	{"PH22", 7, 22, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH23", 7, 23, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH24", 7, 24, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH25", 7, 25, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH26", 7, 26, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH27", 7, 27, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
	{"PH28", 7, 27, {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, NULL, NULL}},
};

const struct allwinner_padconf a31s_padconf = {
	.npins = nitems(a31s_pins),
	.pins = a31s_pins,
};

#endif /* SOC_ALLWINNER_A31S */
