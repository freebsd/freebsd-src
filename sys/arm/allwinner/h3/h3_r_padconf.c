/*-
 * Copyright (c) 2016-2017 Emmanuel Vadot <manu@freebsd.org>
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

#if defined(__aarch64__)
#include "opt_soc.h"
#endif

#include <arm/allwinner/allwinner_pinctrl.h>

#if defined(SOC_ALLWINNER_H3) || defined(SOC_ALLWINNER_H5)

const static struct allwinner_pins h3_r_pins[] = {
	{"PL0",  0, 0,  {"gpio_in", "gpio_out", "s_twi", NULL, NULL, NULL, "pl_eint0", NULL}, 6, 0, 0},
	{"PL1",  0, 1,  {"gpio_in", "gpio_out", "s_twi", NULL, NULL, NULL, "pl_eint1", NULL}, 6, 1, 0},
	{"PL2",  0, 2,  {"gpio_in", "gpio_out", "s_uart", NULL, NULL, NULL, "pl_eint2", NULL}, 6, 2, 0},
	{"PL3",  0, 3,  {"gpio_in", "gpio_out", "s_uart", NULL, NULL, NULL, "pl_eint3", NULL}, 6, 3, 0},
	{"PL4",  0, 4,  {"gpio_in", "gpio_out", "s_jtag", NULL, NULL, NULL, "pl_eint4", NULL}, 6, 4, 0},
	{"PL5",  0, 5,  {"gpio_in", "gpio_out", "s_jtag", NULL, NULL, NULL, "pl_eint5", NULL}, 6, 5, 0},
	{"PL6",  0, 6,  {"gpio_in", "gpio_out", "s_jtag", NULL, NULL, NULL, "pl_eint6", NULL}, 6, 6, 0},
	{"PL7",  0, 7,  {"gpio_in", "gpio_out", "s_jtag", NULL, NULL, NULL, "pl_eint7", NULL}, 6, 7, 0},
	{"PL8",  0, 8,  {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, "pl_eint8", NULL}, 6, 8, 0},
	{"PL9",  0, 9,  {"gpio_in", "gpio_out", NULL, NULL, NULL, NULL, "pl_eint9", NULL}, 6, 9, 0},
	{"PL10", 0, 10, {"gpio_in", "gpio_out", "s_pwm", NULL, NULL, NULL, "pl_eint10", NULL}, 6, 10, 0},
	{"PL11", 0, 11, {"gpio_in", "gpio_out", "s_cir_rx", NULL, NULL, NULL, "pl_eint11", NULL}, 6, 11, 0},
};

const struct allwinner_padconf h3_r_padconf = {
	.npins = nitems(h3_r_pins),
	.pins = h3_r_pins,
};

#endif /* SOC_ALLWINNER_H3 */
