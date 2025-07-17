/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Adrian Chadd <adrian@FreeBSD.org>
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

#ifndef	__QCOM_CLK_BRANCH2_H__
#define	__QCOM_CLK_BRANCH2_H__

#include "qcom_clk_freqtbl.h"

/* halt is 1 */
#define	QCOM_CLK_BRANCH2_BRANCH_HALT		0

/* halt is inverted (ie, 0) */
#define	QCOM_CLK_BRANCH2_BRANCH_HALT_INVERTED	1

/* Don't check the bit, just delay */
#define	QCOM_CLK_BRANCH2_BRANCH_HALT_DELAY	2

/* Don't check the halt bit at all */
#define	QCOM_CLK_BRANCH2_BRANCH_HALT_SKIP	3

/* Flags */
#define	QCOM_CLK_BRANCH2_FLAGS_CRITICAL		0x1
#define	QCOM_CLK_BRANCH2_FLAGS_SET_RATE_PARENT	0x2

struct qcom_clk_branch2_def {
	struct clknode_init_def clkdef;

	uint32_t flags;

	uint32_t enable_offset;	/* enable register*/
	uint32_t enable_shift;	/* enable bit shift */

	uint32_t hwcg_reg;	/* hw clock gate register */
	uint32_t hwcg_bit;
	uint32_t halt_reg;	/* halt register */

	uint32_t halt_check_type;
	bool halt_check_voted;	/* whether to delay when waiting */
};

extern	int qcom_clk_branch2_register(struct clkdom *clkdom,
	    struct qcom_clk_branch2_def *clkdef);

#endif	/* __QCOM_CLK_BRANCH2_H__ */
