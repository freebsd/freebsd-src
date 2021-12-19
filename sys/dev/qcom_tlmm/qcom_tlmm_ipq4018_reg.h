/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
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

#ifndef	__QCOM_TLMM_IPQ4018_REG_H__
#define	__QCOM_TLMM_IPQ4018_REG_H__

/*
 * Each GPIO pin configuration block exists in a 0x1000 sized window.
 */
#define	QCOM_TLMM_IPQ4018_REG_CONFIG_PIN_BASE		0x0
#define	QCOM_TLMM_IPQ4018_REG_CONFIG_PIN_SIZE		0x1000

/*
 * Inside each configuration block are the following registers for
 * controlling the pin.
 */
#define	QCOM_TLMM_IPQ4018_REG_PIN_CONTROL		0x00
			/* 1 = output gpio pin, 0 = input gpio pin */

#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_MASK	0x3
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_SHIFT	0x0
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_DISABLE	0
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLDOWN	1
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_PULLUP	2
			/* There's no BUSHOLD on IPQ4018 */
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_PUPD_BUSHOLD	0
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_MASK	0x7
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_MUX_SHIFT	2
			/* function/mux control */
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_SHIFT	6
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_DRIVE_STRENGTH_MASK	0x7
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OE_ENABLE	(1U << 9)
			/* output enable */
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_VM_ENABLE	(1U << 11)
			/* VM passthrough enable */
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_OD_ENABLE	(1U << 12)
			/* open drain */
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_RES_MASK	0x3
#define		QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_RES_SHIFT	13
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_10K	0x0
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_1K5	0x1
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_35K	0x2
#define			QCOM_TLMM_IPQ4018_REG_PIN_CONTROL_20K	0x3

#define	QCOM_TLMM_IPQ4018_REG_PIN_IO			0x04
#define		QCOM_TLMM_IPQ4018_REG_PIN_IO_INPUT_STATUS	(1U << 0)
			/* read gpio input status */
#define		QCOM_TLMM_IPQ4018_REG_PIN_IO_OUTPUT_EN		(1U << 1)
			/* set gpio output high or low */


#define	QCOM_TLMM_IPQ4018_REG_PIN_INTR_CONFIG		0x08
#define	QCOM_TLMM_IPQ4018_REG_PIN_INTR_STATUS		0x0c

#define	QCOM_TLMM_IPQ4018_REG_PIN(p, reg)		\
	    (((p) * QCOM_TLMM_IPQ4018_REG_CONFIG_PIN_SIZE) + \
	      QCOM_TLMM_IPQ4018_REG_CONFIG_PIN_BASE + (reg))

#endif	/* __QCOM_TLMM_IPQ4018_REG_H__ */
