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
 *
 * $FreeBSD$
 */

#ifndef	__QCOM_CPU_KPSSV2_REG_H__
#define	__QCOM_CPU_KPSSV2_REG_H__


/*
 * APCS CPU core regulator registers.
 */
#define	QCOM_APCS_CPU_PWR_CTL				0x04
#define		QCOM_APCS_CPU_PWR_CTL_PLL_CLAMP		(1U << 8)
#define		QCOM_APCS_CPU_PWR_CTL_CORE_PWRD_UP	(1U << 7)
#define		QCOM_APCS_CPU_PWR_CTL_COREPOR_RST	(1U << 5)
#define		QCOM_APCS_CPU_PWR_CTL_CORE_RST		(1U << 4)
#define		QCOM_APCS_CPU_PWR_CTL_L2DT_SLP		(1U << 3)
#define		QCOM_APCS_CPU_PWR_CTL_CLAMP		(1U << 0)

#define	QCOM_APC_PWR_GATE_CTL				0x14
#define		QCOM_APC_PWR_GATE_CTL_BHS_CNT_SHIFT	24
#define		QCOM_APC_PWR_GATE_CTL_LDO_PWR_DWN_SHIFT	16
#define		QCOM_APC_PWR_GATE_CTL_LDO_BYP_SHIFT	8
#define		QCOM_APC_PWR_GATE_CTL_BHS_SEG_SHIFT	1
#define		QCOM_APC_PWR_GATE_CTL_BHS_EN		(1U << 0)


/*
 * L2 cache regulator registers.
 */
#define	QCOM_APCS_SAW2_2_VCTL        0x1c

#endif	/* __QCOM_CPU_KPSSV2_REG_H__ */
