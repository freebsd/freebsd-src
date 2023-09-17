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

#ifndef	__QCOM_CLK_RCG2_REG_H__
#define	__QCOM_CLK_RCG2_REG_H__

#define	QCOM_CLK_RCG2_CMD_REG				0x0
#define		QCOM_CLK_RCG2_CMD_UPDATE		(1U << 0)
#define		QCOM_CLK_RCG2_CMD_ROOT_EN		(1U << 1)
#define		QCOM_CLK_RCG2_CMD_DIRTY_CFG		(1U << 4)
#define		QCOM_CLK_RCG2_CMD_DIRTY_N		(1U << 5)
#define		QCOM_CLK_RCG2_CMD_DIRTY_M		(1U << 6)
#define		QCOM_CLK_RCG2_CMD_DIRTY_D		(1U << 7)
#define		QCOM_CLK_RCG2_CMD_ROOT_OFF		(1U << 31)

#define	QCOM_CLK_RCG2_CFG_REG				0x4
#define		QCOM_CLK_RCG2_CFG_SRC_DIV_SHIFT		0
#define		QCOM_CLK_RCG2_CFG_SRC_SEL_SHIFT		8
#define		QCOM_CLK_RCG2_CFG_SRC_SEL_MASK	\
		    (0x7 << QCOM_CLK_RCG2_CFG_SRC_SEL_SHIFT)
#define		QCOM_CLK_RCG2_CFG_MODE_SHIFT		12
#define		QCOM_CLK_RCG2_CFG_MODE_MASK	\
		    (0x3 << QCOM_CLK_RCG2_CFG_MODE_SHIFT)
#define		QCOM_CLK_RCG2_CFG_MODE_DUAL_EDGE	\
		    (0x2 << QCOM_CLK_RCG2_CFG_MODE_SHIFT)
#define		QCOM_CLK_RCG2_CFG_HW_CLK_CTRL_MASK	(1U << 20)

#define	QCOM_CLK_RCG2_M_REG				0x8
#define	QCOM_CLK_RCG2_N_REG				0xc
#define	QCOM_CLK_RCG2_D_REG				0x10

#endif	/* __QCOM_CLK_RCG2_REG_H__ */
