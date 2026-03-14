/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Perdixky <3293789706@qq.com>
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

#ifndef BCM_CLK_PERIPH_H
#define BCM_CLK_PERIPH_H

#include <sys/types.h>

#include <dev/clk/clk.h>

struct bcm_clk_periph_def {
	struct clknode_init_def clkdef;

	uint32_t ctl_offset;
	uint32_t div_offset;

	uint32_t passwd_shift;
	uint32_t passwd_width;
	uint32_t passwd;

	uint32_t mash_shift;
	uint32_t mash_width;

	uint32_t busy_shift;

	uint32_t enable_shift;

	uint32_t src_shift;
	uint32_t src_width;

	uint32_t div_int_shift;
	uint32_t div_int_width;
	uint32_t div_frac_shift;
	uint32_t div_frac_width;
};

int bcm_clk_periph_register(struct clkdom *clkdom,
    struct bcm_clk_periph_def *clkdef);

#endif /* BCM_CLK_PERIPH_H */
