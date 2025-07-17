/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright 2018 Emmanuel Vadot <manu@FreeBSD.org>
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

#ifndef _RK_CLK_COMPOSITE_H_
#define _RK_CLK_COMPOSITE_H_

#include <dev/clk/clk.h>

struct rk_clk_composite_def {
	struct clknode_init_def	clkdef;

	uint32_t	muxdiv_offset;

	uint32_t	mux_shift;
	uint32_t	mux_width;

	uint32_t	div_shift;
	uint32_t	div_width;

	uint32_t	flags;
};

#define	RK_CLK_COMPOSITE_HAVE_MUX	0x0001
#define	RK_CLK_COMPOSITE_DIV_EXP	0x0002	/* Register   0, 1, 2, 2, ... */
						/* Divider    1, 2, 4, 8, ... */
#define	RK_CLK_COMPOSITE_GRF		0x0004 /* Use syscon registers instead of CRU's */
int rk_clk_composite_register(struct clkdom *clkdom,
    struct rk_clk_composite_def *clkdef);

#endif /* _RK_CLK_COMPOSITE_H_ */
