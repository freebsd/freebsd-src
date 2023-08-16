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

#ifndef	__QCOM_CLK_RCG2_H__
#define	__QCOM_CLK_RCG2_H__

#include "qcom_clk_freqtbl.h"

/* Flags */
/* Set the rate on the parent clock, not just ours */
#define	QCOM_CLK_RCG2_FLAGS_SET_RATE_PARENT		0x1
/* Must not stop this clock/gate! */
#define	QCOM_CLK_RCG2_FLAGS_CRITICAL			0x2

/* prediv to hw mapping */
#define	QCOM_CLK_FREQTBL_PREDIV_RCG2(prediv)		(2*(prediv)-1)

struct qcom_clk_rcg2_def {
	struct clknode_init_def clkdef;
	uint32_t cmd_rcgr;		/* rcg2 register start */
	uint32_t hid_width;		/* pre-divisor width */
	uint32_t mnd_width;		/* mn:d divisor width */
	int32_t safe_src_idx;		/* safe parent when disabling a shared
					 * rcg2 */
	uint32_t cfg_offset;		/* cfg offset after cmd_rcgr */
	int32_t safe_pre_parent_idx;	/* safe parent before switching
					 * parent mux */
	uint32_t flags;
	const struct qcom_clk_freq_tbl *freq_tbl;
};

extern	int qcom_clk_rcg2_register(struct clkdom *clkdom,
	    struct qcom_clk_rcg2_def *clkdef);

#endif	/* __QCOM_CLK_RCG2_H__ */
