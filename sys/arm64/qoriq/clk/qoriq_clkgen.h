/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alstom Group.
 * Copyright (c) 2020 Semihalf.
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
 * $FreeBSD$
 *
 */

#ifndef	_QORIQ_CLKGEN_H_
#define	_QORIQ_CLKGEN_H_

#include <dev/extres/clk/clk.h>
#include <dev/extres/clk/clk_mux.h>

#include <arm64/qoriq/clk/qoriq_clk_pll.h>

#define	QORIQ_CLK_NAME_MAX_LEN	32

#define	QORIQ_LITTLE_ENDIAN	0x01

#define	QORIQ_TYPE_SYSCLK	0
#define	QORIQ_TYPE_CMUX		1
#define	QORIQ_TYPE_HWACCEL	2
#define	QORIQ_TYPE_FMAN		3
#define	QORIQ_TYPE_PLATFORM_PLL	4
#define	QORIQ_TYPE_CORECLK	5
#define	QORIQ_TYPE_INTERNAL	6

#define	PLL_DIV1	0
#define	PLL_DIV2	1
#define	PLL_DIV3	2
#define	PLL_DIV4	3
#define	PLL_DIV5	4
#define	PLL_DIV6	5
#define	PLL_DIV7	6
#define	PLL_DIV8	7
#define	PLL_DIV9	8
#define	PLL_DIV10	9
#define	PLL_DIV11	10
#define	PLL_DIV12	11
#define	PLL_DIV13	12
#define	PLL_DIV14	13
#define	PLL_DIV15	14
#define	PLL_DIV16	15

#define	QORIQ_CLK_ID(_type, _index)	((_type << 8) + _index)

#define	QORIQ_SYSCLK_NAME	"clockgen_sysclk"
#define	QORIQ_CORECLK_NAME	"clockgen_coreclk"

typedef int (*qoriq_init_func_t)(device_t);

struct qoriq_clkgen_softc {
	device_t			dev;
	struct resource			*res;
	struct clkdom			*clkdom;
	struct mtx			mtx;
	struct qoriq_clk_pll_def	*pltfrm_pll_def;
	struct qoriq_clk_pll_def	**cga_pll;
	int				cga_pll_num;
	struct clk_mux_def		**mux;
	int				mux_num;
	qoriq_init_func_t		init_func;
	uint32_t			flags;
	bool				has_coreclk;
};

MALLOC_DECLARE(M_QORIQ_CLKGEN);
DECLARE_CLASS(qoriq_clkgen_driver);

int qoriq_clkgen_attach(device_t);

#endif	/* _QORIQ_CLKGEN_H_ */
