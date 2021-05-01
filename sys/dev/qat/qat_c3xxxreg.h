/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_c3xxxreg.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

/*
 * Copyright (c) 2019 Internet Initiative Japan, Inc.
 * All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 *   Copyright(c) 2014 Intel Corporation.
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* $FreeBSD$ */

#ifndef _DEV_PCI_QAT_C3XXXREG_H_
#define _DEV_PCI_QAT_C3XXXREG_H_

/* Max number of accelerators and engines */
#define MAX_ACCEL_C3XXX			3
#define MAX_AE_C3XXX			6

/* PCIe BAR index */
#define BAR_SRAM_ID_C3XXX		NO_PCI_REG
#define BAR_PMISC_ID_C3XXX		0
#define BAR_ETR_ID_C3XXX		1

/* BAR PMISC sub-regions */
#define AE_OFFSET_C3XXX			0x20000
#define AE_LOCAL_OFFSET_C3XXX		0x20800
#define CAP_GLOBAL_OFFSET_C3XXX		0x30000

#define SOFTSTRAP_REG_C3XXX			0x2EC
#define SOFTSTRAP_SS_POWERGATE_CY_C3XXX		__BIT(23)
#define SOFTSTRAP_SS_POWERGATE_PKE_C3XXX	__BIT(24)

#define ACCEL_REG_OFFSET_C3XXX		16
#define ACCEL_MASK_C3XXX		0x7
#define AE_MASK_C3XXX			0x3F

#define SMIAPF0_C3XXX			0x3A028
#define SMIAPF1_C3XXX			0x3A030
#define SMIA0_MASK_C3XXX		0xFFFF
#define SMIA1_MASK_C3XXX		0x1

/* Error detection and correction */
#define AE_CTX_ENABLES_C3XXX(i)		((i) * 0x1000 + 0x20818)
#define AE_MISC_CONTROL_C3XXX(i)	((i) * 0x1000 + 0x20960)
#define ENABLE_AE_ECC_ERR_C3XXX		__BIT(28)
#define ENABLE_AE_ECC_PARITY_CORR_C3XXX	(__BIT(24) | __BIT(12))
#define ERRSSMSH_EN_C3XXX		__BIT(3)
/* BIT(2) enables the logging of push/pull data errors. */
#define PPERR_EN_C3XXX			(__BIT(2))

/* Mask for VF2PF interrupts */
#define VF2PF1_16_C3XXX			(0xFFFF << 9)
#define ERRSOU3_VF2PF_C3XXX(errsou3)	(((errsou3) & 0x01FFFE00) >> 9)
#define ERRMSK3_VF2PF_C3XXX(vf_mask)	(((vf_mask) & 0xFFFF) << 9)

/* Masks for correctable error interrupts. */
#define ERRMSK0_CERR_C3XXX		(__BIT(24) | __BIT(16) | __BIT(8) | __BIT(0))
#define ERRMSK1_CERR_C3XXX		(__BIT(8) | __BIT(0))
#define ERRMSK5_CERR_C3XXX		(0)

/* Masks for uncorrectable error interrupts. */
#define ERRMSK0_UERR_C3XXX		(__BIT(25) | __BIT(17) | __BIT(9) | __BIT(1))
#define ERRMSK1_UERR_C3XXX		(__BIT(9) | __BIT(1))
#define ERRMSK3_UERR_C3XXX		(__BIT(6) | __BIT(5) | __BIT(4) | __BIT(3) | \
					 __BIT(2) | __BIT(0))
#define ERRMSK5_UERR_C3XXX		(__BIT(16))

/* RI CPP control */
#define RICPPINTCTL_C3XXX		(0x3A000 + 0x110)
/*
 * BIT(2) enables error detection and reporting on the RI Parity Error.
 * BIT(1) enables error detection and reporting on the RI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the RI CPP Push interface.
 */
#define RICPP_EN_C3XXX			(__BIT(2) | __BIT(1) | __BIT(0))

/* TI CPP control */
#define TICPPINTCTL_C3XXX		(0x3A400 + 0x138)
/*
 * BIT(3) enables error detection and reporting on the ETR Parity Error.
 * BIT(2) enables error detection and reporting on the TI Parity Error.
 * BIT(1) enables error detection and reporting on the TI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the TI CPP Push interface.
 */
#define TICPP_EN_C3XXX		\
	(__BIT(3) | __BIT(2) | __BIT(1) | __BIT(0))

/* CFC Uncorrectable Errors */
#define CPP_CFC_ERR_CTRL_C3XXX	(0x30000 + 0xC00)
/*
 * BIT(1) enables interrupt.
 * BIT(0) enables detecting and logging of push/pull data errors.
 */
#define CPP_CFC_UE_C3XXX		(__BIT(1) | __BIT(0))

#define SLICEPWRDOWN_C3XXX(i)	((i) * 0x4000 + 0x2C)
/* Enabling PKE4-PKE0. */
#define MMP_PWR_UP_MSK_C3XXX		\
	(__BIT(20) | __BIT(19) | __BIT(18) | __BIT(17) | __BIT(16))

/* CPM Uncorrectable Errors */
#define INTMASKSSM_C3XXX(i)		((i) * 0x4000 + 0x0)
/* Disabling interrupts for correctable errors. */
#define INTMASKSSM_UERR_C3XXX	\
	(__BIT(11) | __BIT(9) | __BIT(7) | __BIT(5) | __BIT(3) | __BIT(1))

/* MMP */
/* BIT(3) enables correction. */
#define CERRSSMMMP_EN_C3XXX		(__BIT(3))

/* BIT(3) enables logging. */
#define UERRSSMMMP_EN_C3XXX		(__BIT(3))

/* ETR */
#define ETR_MAX_BANKS_C3XXX		16
#define ETR_TX_RX_GAP_C3XXX		8
#define ETR_TX_RINGS_MASK_C3XXX		0xFF
#define ETR_BUNDLE_SIZE_C3XXX		0x1000

/* AE firmware */
#define AE_FW_PROD_TYPE_C3XXX		0x02000000
#define AE_FW_MOF_NAME_C3XXX	"qat_c3xxxfw"
#define AE_FW_MMP_NAME_C3XXX	"qat_c3xxx_mmp"
#define AE_FW_UOF_NAME_C3XXX	"icp_qat_ae.suof"

/* Clock frequency */
#define CLOCK_PER_SEC_C3XXX		(685 * 1000000 / 16)

#endif
