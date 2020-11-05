/* SPDX-License-Identifier: BSD-2-Clause-NetBSD AND BSD-3-Clause */
/*	$NetBSD: qat_c2xxxreg.h,v 1.1 2019/11/20 09:37:46 hikaru Exp $	*/

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
 *   Copyright(c) 2007-2013 Intel Corporation. All rights reserved.
 *
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

#ifndef _DEV_PCI_QAT_C2XXXREG_H_
#define _DEV_PCI_QAT_C2XXXREG_H_

/* PCI revision IDs */
#define QAT_REVID_C2XXX_A0		0x00
#define QAT_REVID_C2XXX_B0		0x02
#define QAT_REVID_C2XXX_C0		0x03

/* Max number of accelerators and engines */
#define MAX_ACCEL_C2XXX			1
#define MAX_AE_C2XXX			2

/* PCIe BAR index */
#define BAR_SRAM_ID_C2XXX		NO_PCI_REG
#define BAR_PMISC_ID_C2XXX		0
#define BAR_ETR_ID_C2XXX		1

#define ACCEL_MASK_C2XXX		0x1
#define AE_MASK_C2XXX			0x3

#define MSIX_AE_VEC_GAP_C2XXX		8

/* PCIe configuration space registers */
/* PESRAM: 512K eSRAM */
#define BAR_PESRAM_C2XXX		NO_PCI_REG
#define BAR_PESRAM_SIZE_C2XXX		0

/*
 * PMISC: 16K CAP, 16K Scratch, 32K SSU(QATs),
 *        32K AE CSRs and transfer registers, 8K CHAP/PMU,
 *        4K EP CSRs, 4K MSI-X Tables
 */
#define BAR_PMISC_C2XXX			0x18
#define BAR_PMISC_SIZE_C2XXX		0x20000	/* 128K */

/* PETRINGCSR: 8K 16 bundles of ET Ring CSRs */
#define BAR_PETRINGCSR_C2XXX		0x20
#define BAR_PETRINGCSR_SIZE_C2XXX	0x4000	/* 16K */

/* Fuse Control */
#define FUSECTL_C2XXX_PKE_DISABLE	(1 << 6)
#define FUSECTL_C2XXX_ATH_DISABLE	(1 << 5)
#define FUSECTL_C2XXX_CPH_DISABLE	(1 << 4)
#define FUSECTL_C2XXX_LOW_SKU		(1 << 3)
#define FUSECTL_C2XXX_MID_SKU		(1 << 2)
#define FUSECTL_C2XXX_AE1_DISABLE	(1 << 1)

/* SINT: Signal Target Raw Interrupt Register */
#define EP_SINTPF_C2XXX			0x1A024

/* SMIA: Signal Target IA Mask Register */
#define EP_SMIA_C2XXX				0x1A028
#define EP_SMIA_BUNDLES_IRQ_MASK_C2XXX		0xFF
#define EP_SMIA_AE_IRQ_MASK_C2XXX		0x10000
#define EP_SMIA_MASK_C2XXX			\
	(EP_SMIA_BUNDLES_IRQ_MASK_C2XXX | EP_SMIA_AE_IRQ_MASK_C2XXX)

#define EP_RIMISCCTL_C2XXX		0x1A0C4
#define EP_RIMISCCTL_MASK_C2XXX		0x40000000

#define PFCGCIOSFPRIR_REG_C2XXX			0x2C0
#define PFCGCIOSFPRIR_MASK_C2XXX		0XFFFF7FFF

/* BAR sub-regions */
#define PESRAM_BAR_C2XXX		NO_PCI_REG
#define PESRAM_OFFSET_C2XXX		0x0
#define PESRAM_SIZE_C2XXX		0x0
#define CAP_GLOBAL_BAR_C2XXX		BAR_PMISC_C2XXX
#define CAP_GLOBAL_OFFSET_C2XXX		0x00000
#define CAP_GLOBAL_SIZE_C2XXX		0x04000
#define CAP_HASH_OFFSET			0x900
#define SCRATCH_BAR_C2XXX		NO_PCI_REG
#define SCRATCH_OFFSET_C2XXX		NO_REG_OFFSET
#define SCRATCH_SIZE_C2XXX		0x0
#define SSU_BAR_C2XXX			BAR_PMISC_C2XXX
#define SSU_OFFSET_C2XXX		0x08000
#define SSU_SIZE_C2XXX			0x08000
#define AE_BAR_C2XXX			BAR_PMISC_C2XXX
#define AE_OFFSET_C2XXX			0x10000
#define AE_LOCAL_OFFSET_C2XXX		0x10800
#define PMU_BAR_C2XXX			NO_PCI_REG
#define PMU_OFFSET_C2XXX		NO_REG_OFFSET
#define PMU_SIZE_C2XXX			0x0
#define EP_BAR_C2XXX			BAR_PMISC_C2XXX
#define EP_OFFSET_C2XXX			0x1A000
#define EP_SIZE_C2XXX			0x01000
#define MSIX_TAB_BAR_C2XXX		NO_PCI_REG	/* mapped by pci(9) */
#define MSIX_TAB_OFFSET_C2XXX		0x1B000
#define MSIX_TAB_SIZE_C2XXX		0x01000
#define PETRINGCSR_BAR_C2XXX		BAR_PETRINGCSR_C2XXX
#define PETRINGCSR_OFFSET_C2XXX		0x0
#define PETRINGCSR_SIZE_C2XXX		0x0	/* use size of BAR */

/* ETR */
#define ETR_MAX_BANKS_C2XXX		8
#define ETR_MAX_ET_RINGS_C2XXX		\
	(ETR_MAX_BANKS_C2XXX * ETR_MAX_RINGS_PER_BANK_C2XXX)
#define ETR_MAX_AP_BANKS_C2XXX		4

#define ETR_TX_RX_GAP_C2XXX		1
#define ETR_TX_RINGS_MASK_C2XXX		0x51

#define ETR_BUNDLE_SIZE_C2XXX		0x0200

/* Initial bank Interrupt Source mask */
#define ETR_INT_SRCSEL_MASK_0_C2XXX	0x4444444CUL
#define ETR_INT_SRCSEL_MASK_X_C2XXX	0x44444444UL

/* AE firmware */
#define AE_FW_PROD_TYPE_C2XXX			0x00800000
#define AE_FW_MOF_NAME_C2XXX		"mof_firmware_c2xxx"
#define AE_FW_MMP_NAME_C2XXX		"mmp_firmware_c2xxx"
#define AE_FW_UOF_NAME_C2XXX_A0		"icp_qat_nae.uof"
#define AE_FW_UOF_NAME_C2XXX_B0		"icp_qat_nae_b0.uof"

#endif
