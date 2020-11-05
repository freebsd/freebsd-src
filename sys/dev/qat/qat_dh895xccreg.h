/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */

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
 *   Copyright(c) 2014-2020 Intel Corporation.
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

#ifndef _DEV_PCI_QAT_DH895XCCREG_H_
#define _DEV_PCI_QAT_DH895XCCREG_H_

/* Max number of accelerators and engines */
#define MAX_ACCEL_DH895XCC		6
#define MAX_AE_DH895XCC			12

/* PCIe BAR index */
#define BAR_SRAM_ID_DH895XCC		0
#define BAR_PMISC_ID_DH895XCC		1
#define BAR_ETR_ID_DH895XCC		2

/* BAR PMISC sub-regions */
#define AE_OFFSET_DH895XCC		0x20000
#define AE_LOCAL_OFFSET_DH895XCC	0x20800
#define CAP_GLOBAL_OFFSET_DH895XCC	0x30000

#define SOFTSTRAP_REG_DH895XCC			0x2EC

#define	FUSECTL_SKU_MASK_DH895XCC	0x300000
#define	FUSECTL_SKU_SHIFT_DH895XCC	20
#define	FUSECTL_SKU_1_DH895XCC		0
#define	FUSECTL_SKU_2_DH895XCC		1
#define	FUSECTL_SKU_3_DH895XCC		2
#define	FUSECTL_SKU_4_DH895XCC		3

#define ACCEL_REG_OFFSET_DH895XCC	13
#define ACCEL_MASK_DH895XCC		0x3F
#define AE_MASK_DH895XCC		0xFFF

#define SMIAPF0_DH895XCC		0x3A028
#define SMIAPF1_DH895XCC		0x3A030
#define SMIA0_MASK_DH895XCC		0xFFFFFFFF
#define SMIA1_MASK_DH895XCC		0x1

/* Error detection and correction */
#define AE_CTX_ENABLES_DH895XCC(i)	((i) * 0x1000 + 0x20818)
#define AE_MISC_CONTROL_DH895XCC(i)	((i) * 0x1000 + 0x20960)
#define ENABLE_AE_ECC_ERR_DH895XCC	__BIT(28)
#define ENABLE_AE_ECC_PARITY_CORR_DH895XCC (__BIT(24) | __BIT(12))
#define ERRSSMSH_EN_DH895XCC		__BIT(3)
/* BIT(2) enables the logging of push/pull data errors. */
#define PPERR_EN_DH895XCC		(__BIT(2))

/* ETR */
#define ETR_MAX_BANKS_DH895XCC		32
#define ETR_TX_RX_GAP_DH895XCC		8
#define ETR_TX_RINGS_MASK_DH895XCC	0xFF
#define ETR_BUNDLE_SIZE_DH895XCC	0x1000

/* AE firmware */
#define AE_FW_PROD_TYPE_DH895XCC	0x00400000
#define AE_FW_MOF_NAME_DH895XCC		"qat_895xcc"
#define AE_FW_MMP_NAME_DH895XCC		"qat_895xcc_mmp"
#define AE_FW_UOF_NAME_DH895XCC		"icp_qat_ae.uof"

/* Clock frequency */
#define CLOCK_PER_SEC_DH895XCC		(685 * 1000000 / 16)

#endif
