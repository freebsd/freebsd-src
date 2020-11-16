/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#ifndef	_ARM64_IOMMU_SMMUREG_H_
#define	_ARM64_IOMMU_SMMUREG_H_

#define	SMMU_IDR0		0x000
#define	 IDR0_ST_LVL_S		27
#define	 IDR0_ST_LVL_M		(0x3 << IDR0_ST_LVL_S)
#define	 IDR0_ST_LVL_LINEAR	(0x0 << IDR0_ST_LVL_S) /* Linear Stream table*/
#define	 IDR0_ST_LVL_2		(0x1 << IDR0_ST_LVL_S) /* 2-level Stream Table*/
#define	 IDR0_ST_TERM_MODEL	(1 << 26) /* Terminate model behavior */
#define	 IDR0_STALL_MODEL_S	24 /*  Stall model support */
#define	 IDR0_STALL_MODEL_M	(0x3 << IDR0_STALL_MODEL_S)
#define	 IDR0_STALL_MODEL_STALL	(0x0 << IDR0_STALL_MODEL_S) /* Stall and Term*/
#define	 IDR0_STALL_MODEL_FORCE	(0x2 << IDR0_STALL_MODEL_S) /* Stall is forced*/
#define	 IDR0_TTENDIAN_S	21 /* Endianness for translation table walks.*/
#define	 IDR0_TTENDIAN_M	(0x3 << IDR0_TTENDIAN_S)
#define	 IDR0_TTENDIAN_MIXED	(0x0 << IDR0_TTENDIAN_S)
#define	 IDR0_TTENDIAN_LITTLE	(0x2 << IDR0_TTENDIAN_S)
#define	 IDR0_TTENDIAN_BIG	(0x3 << IDR0_TTENDIAN_S)
#define	 IDR0_VATOS		(1 << 20) / * Virtual ATOS page interface */
#define	 IDR0_CD2L		(1 << 19) /* 2-level Context descriptor table*/
#define	 IDR0_VMID16		(1 << 18) /* 16-bit VMID supported */
#define	 IDR0_VMW		(1 << 17) /* VMID wildcard-matching */
#define	 IDR0_PRI		(1 << 16) /* Page Request Interface supported*/
#define	 IDR0_ATOS		(1 << 15) /* Address Translation Operations */
#define	 IDR0_SEV		(1 << 14) /* WFE wake-up events */
#define	 IDR0_MSI		(1 << 13) /* Message Signalled Interrupts */
#define	 IDR0_ASID16		(1 << 12) /* 16-bit ASID supported */
#define	 IDR0_NS1ATS		(1 << 11) /* Split-stage ATS not supported */
#define	 IDR0_ATS		(1 << 10) /* PCIe ATS supported by SMMU */
#define	 IDR0_HYP		(1 << 9) /* Hypervisor stage 1 contexts */
#define	 IDR0_DORMHINT		(1 << 8) /* Dormant hint supported */
#define	 IDR0_HTTU_S		6 /* H/W transl. table A-flag and Dirty state */
#define	 IDR0_HTTU_M		(0x3 << IDR0_HTTU_S)
#define	 IDR0_HTTU_A		(0x1 << IDR0_HTTU_S) /* Access flag (A-flag) */
#define	 IDR0_HTTU_AD		(0x2 << IDR0_HTTU_S) /* A-flag and Dirty State*/
#define	 IDR0_BTM		(1 << 5) /* Broadcast TLB Maintenance */
#define	 IDR0_COHACC		(1 << 4) /* Coherent access to translations*/
#define	 IDR0_TTF_S		2 /* Translation Table Formats supported */
#define	 IDR0_TTF_M		(0x3 << IDR0_TTF_S)
#define	 IDR0_TTF_AA32		(0x1 << IDR0_TTF_S) /* AArch32 (LPAE) */
#define	 IDR0_TTF_AA64		(0x2 << IDR0_TTF_S) /* AArch64 */
#define	 IDR0_TTF_ALL		(0x3 << IDR0_TTF_S) /* AArch32 and AArch64 */
#define	 IDR0_S1P		(1 << 1) /* Stage1 translation supported. */
#define	 IDR0_S2P		(1 << 0) /* Stage2 translation supported. */
#define	SMMU_IDR1		0x004
#define	 IDR1_TABLES_PRESET	(1 << 30) /* Table base addresses fixed. */
#define	 IDR1_QUEUES_PRESET	(1 << 29) /* Queue base addresses fixed. */
#define	 IDR1_REL		(1 << 28) /* Relative base pointers */
#define	 IDR1_ATTR_TYPES_OVR	(1 << 27) /* Incoming attrs can be overridden*/
#define	 IDR1_ATTR_PERMS_OVR	(1 << 26) /* Incoming attrs can be overridden*/
#define	 IDR1_CMDQS_S		21 /* Maximum number of Command queue entries*/
#define	 IDR1_CMDQS_M		(0x1f << IDR1_CMDQS_S)
#define	 IDR1_EVENTQS_S		16 /* Maximum number of Event queue entries */
#define	 IDR1_EVENTQS_M		(0x1f << IDR1_EVENTQS_S)
#define	 IDR1_PRIQS_S		11 /* Maximum number of PRI queue entries */
#define	 IDR1_PRIQS_M		(0x1f << IDR1_PRIQS_S)
#define	 IDR1_SSIDSIZE_S	6 /* Max bits of SubstreamID */
#define	 IDR1_SSIDSIZE_M	(0x1f << IDR1_SSIDSIZE_S)
#define	 IDR1_SIDSIZE_S		0 /* Max bits of StreamID */
#define	 IDR1_SIDSIZE_M		(0x3f << IDR1_SIDSIZE_S)
#define	SMMU_IDR2		0x008
#define	SMMU_IDR3		0x00C
#define	 IDR3_RIL		(1 << 10) /* Range-based Invalidations. */
#define	SMMU_IDR4		0x010
#define	SMMU_IDR5		0x014
#define	 IDR5_STALL_MAX_S	16 /* Max outstanding stalled transactions */
#define	 IDR5_STALL_MAX_M	(0xffff << IDR5_STALL_MAX_S)
#define	 IDR5_VAX_S		10 /* Virtual Address eXtend */
#define	 IDR5_VAX_M		(0x3 << IDR5_VAX_S)
#define	 IDR5_VAX_48		(0 << IDR5_VAX_S)
#define	 IDR5_VAX_52		(1 << IDR5_VAX_S)
#define	 IDR5_GRAN64K		(1 << 6) /* 64KB translation granule */
#define	 IDR5_GRAN16K		(1 << 5) /* 16KB translation granule */
#define	 IDR5_GRAN4K		(1 << 4) /* 4KB translation granule */
#define	 IDR5_OAS_S		0 /* Output Address Size */
#define	 IDR5_OAS_M		(0x7 << IDR5_OAS_S)
#define	 IDR5_OAS_32		(0x0 << IDR5_OAS_S)
#define	 IDR5_OAS_36		(0x1 << IDR5_OAS_S)
#define	 IDR5_OAS_40		(0x2 << IDR5_OAS_S)
#define	 IDR5_OAS_42		(0x3 << IDR5_OAS_S)
#define	 IDR5_OAS_44		(0x4 << IDR5_OAS_S)
#define	 IDR5_OAS_48		(0x5 << IDR5_OAS_S)
#define	 IDR5_OAS_52		(0x6 << IDR5_OAS_S) /* Reserved in SMMU v3.0 */
#define	SMMU_IIDR		0x018
#define	SMMU_AIDR		0x01C
#define	SMMU_CR0		0x020
#define	 CR0_VMW_S		6 /* VMID Wildcard */
#define	 CR0_VMW_M		(0x7 << CR0_VMW_S)
#define	 CR0_ATSCHK		(1 << 4) /* ATS behavior: Safe mode */
#define	 CR0_CMDQEN		(1 << 3) /* Enable Command queue processing */
#define	 CR0_EVENTQEN		(1 << 2) /* Enable Event queue writes */
#define	 CR0_PRIQEN		(1 << 1) /* Enable PRI queue writes */
#define	 CR0_SMMUEN		(1 << 0) /* Non-secure SMMU enable */
#define	SMMU_CR0ACK		0x024
#define	SMMU_CR1		0x028
#define	 CR1_TABLE_SH_S		10 /* Table access Shareability. */
#define	 CR1_TABLE_SH_M		(0x3 << CR1_TABLE_SH_S)
#define	 CR1_TABLE_SH_NS	(0x0 << CR1_TABLE_SH_S)
#define	 CR1_TABLE_SH_OS	(0x2 << CR1_TABLE_SH_S)
#define	 CR1_TABLE_SH_IS	(0x3 << CR1_TABLE_SH_S)
#define	 CR1_TABLE_OC_S		8 /* Table access Outer Cacheability. */
#define	 CR1_TABLE_OC_M		(0x3 << CR1_TABLE_OC_S)
#define	 CR1_TABLE_OC_NC	(0x0 << CR1_TABLE_OC_S)
#define	 CR1_TABLE_OC_WBC	(0x1 << CR1_TABLE_OC_S)
#define	 CR1_TABLE_OC_WTC	(0x2 << CR1_TABLE_OC_S)
#define	 CR1_TABLE_IC_S		6 /* Table access Inner Cacheability. */
#define	 CR1_TABLE_IC_M		(0x3 << CR1_TABLE_IC_S)
#define	 CR1_TABLE_IC_NC	(0x0 << CR1_TABLE_IC_S)
#define	 CR1_TABLE_IC_WBC	(0x1 << CR1_TABLE_IC_S)
#define	 CR1_TABLE_IC_WTC	(0x2 << CR1_TABLE_IC_S)
#define	 CR1_QUEUE_SH_S		4 /* Queue access Shareability. */
#define	 CR1_QUEUE_SH_M		(0x3 << CR1_QUEUE_SH_S)
#define	 CR1_QUEUE_SH_NS	(0x0 << CR1_QUEUE_SH_S)
#define	 CR1_QUEUE_SH_OS	(0x2 << CR1_QUEUE_SH_S)
#define	 CR1_QUEUE_SH_IS	(0x3 << CR1_QUEUE_SH_S)
#define	 CR1_QUEUE_OC_S		2 /* Queue access Outer Cacheability. */
#define	 CR1_QUEUE_OC_M		(0x3 << CR1_QUEUE_OC_S)
#define	 CR1_QUEUE_OC_NC	(0x0 << CR1_QUEUE_OC_S)
#define	 CR1_QUEUE_OC_WBC	(0x1 << CR1_QUEUE_OC_S)
#define	 CR1_QUEUE_OC_WTC	(0x2 << CR1_QUEUE_OC_S)
#define	 CR1_QUEUE_IC_S		0 /* Queue access Inner Cacheability. */
#define	 CR1_QUEUE_IC_M		(0x3 << CR1_QUEUE_IC_S)
#define	 CR1_QUEUE_IC_NC	(0x0 << CR1_QUEUE_IC_S)
#define	 CR1_QUEUE_IC_WBC	(0x1 << CR1_QUEUE_IC_S)
#define	 CR1_QUEUE_IC_WTC	(0x2 << CR1_QUEUE_IC_S)
#define	SMMU_CR2		0x02C
#define	 CR2_PTM		(1 << 2) /* Private TLB Maintenance. */
#define	 CR2_RECINVSID		(1 << 1) /* Record invalid SID. */
#define	 CR2_E2H		(1 << 0) /* Enable EL2-E2H translation regime */
#define	SMMU_STATUSR		0x040
#define	SMMU_GBPA		0x044
#define	SMMU_AGBPA		0x048
#define	SMMU_IRQ_CTRL		0x050
#define	 IRQ_CTRL_EVENTQ_IRQEN	(1 << 2) /* NS Event queue interrupts enabled.*/
#define	 IRQ_CTRL_PRIQ_IRQEN	(1 << 1) /* PRI queue interrupts are enabled.*/
#define	 IRQ_CTRL_GERROR_IRQEN	(1 << 0) /* Global errors int are enabled. */
#define	SMMU_IRQ_CTRLACK	0x054
#define	SMMU_GERROR		0x060
#define	SMMU_GERRORN		0x064
#define	SMMU_GERROR_IRQ_CFG0	0x068
#define	SMMU_GERROR_IRQ_CFG1	0x070
#define	SMMU_GERROR_IRQ_CFG2	0x074
#define	SMMU_STRTAB_BASE	0x080
#define	 STRTAB_BASE_RA		(1UL << 62) /* Read-Allocate. */
#define	 STRTAB_BASE_ADDR_S	6 /* Physical address of Stream table base */
#define	 STRTAB_BASE_ADDR_M	(0x3fffffffffffUL << STRTAB_BASE_ADDR_S)
#define	SMMU_STRTAB_BASE_CFG	0x088
#define	 STRTAB_BASE_CFG_FMT_S		16 /* Format of Stream table. */
#define	 STRTAB_BASE_CFG_FMT_M		(0x3 << STRTAB_BASE_CFG_FMT_S)
#define	 STRTAB_BASE_CFG_FMT_LINEAR	(0x0 << STRTAB_BASE_CFG_FMT_S)
#define	 STRTAB_BASE_CFG_FMT_2LVL	(0x1 << STRTAB_BASE_CFG_FMT_S)
#define	 STRTAB_BASE_CFG_SPLIT_S	6 /* SID split point for 2lvl table. */
#define	 STRTAB_BASE_CFG_SPLIT_M	(0x1f << STRTAB_BASE_CFG_SPLIT_S)
#define	 STRTAB_BASE_CFG_SPLIT_4KB	(6 << STRTAB_BASE_CFG_SPLIT_S)
#define	 STRTAB_BASE_CFG_SPLIT_16KB	(8 << STRTAB_BASE_CFG_SPLIT_S)
#define	 STRTAB_BASE_CFG_SPLIT_64KB	(10 << STRTAB_BASE_CFG_SPLIT_S)
#define	 STRTAB_BASE_CFG_LOG2SIZE_S	0 /* Table size as log2(entries) */
#define	 STRTAB_BASE_CFG_LOG2SIZE_M	(0x3f << STRTAB_BASE_CFG_LOG2SIZE_S)
#define	SMMU_CMDQ_BASE		0x090
#define	 CMDQ_BASE_RA		(1UL << 62) /* Read-Allocate. */
#define	 Q_BASE_ADDR_S		5 /* PA of queue base */
#define	 Q_BASE_ADDR_M		(0x7fffffffffff << Q_BASE_ADDR_S)
#define	 Q_LOG2SIZE_S		0 /* Queue size as log2(entries) */
#define	 Q_LOG2SIZE_M		(0x1f << Q_LOG2SIZE_S)
#define	SMMU_CMDQ_PROD		0x098
#define	SMMU_CMDQ_CONS		0x09C
#define	 CMDQ_CONS_ERR_S	24
#define	 CMDQ_CONS_ERR_M	(0x7f << CMDQ_CONS_ERR_S)
#define	SMMU_EVENTQ_BASE	0x0A0
#define	 EVENTQ_BASE_WA		(1UL << 62) /* Write-Allocate. */
#define	SMMU_EVENTQ_PROD	0x100A8
#define	SMMU_EVENTQ_CONS	0x100AC
#define	SMMU_EVENTQ_IRQ_CFG0	0x0B0
#define	SMMU_EVENTQ_IRQ_CFG1	0x0B8
#define	SMMU_EVENTQ_IRQ_CFG2	0x0BC
#define	SMMU_PRIQ_BASE		0x0C0
#define	 PRIQ_BASE_WA		(1UL < 62) /* Write-Allocate. */
#define	SMMU_PRIQ_PROD		0x100C8
#define	SMMU_PRIQ_CONS		0x100CC
#define	SMMU_PRIQ_IRQ_CFG0	0x0D0
#define	SMMU_PRIQ_IRQ_CFG1	0x0D8
#define	SMMU_PRIQ_IRQ_CFG2	0x0DC
#define	SMMU_GATOS_CTRL		0x100
#define	SMMU_GATOS_SID		0x108
#define	SMMU_GATOS_ADDR		0x110
#define	SMMU_GATOS_PAR		0x118
#define	SMMU_VATOS_SEL		0x180
#define	SMMU_S_IDR0		0x8000
#define	SMMU_S_IDR1		0x8004
#define	SMMU_S_IDR2		0x8008
#define	SMMU_S_IDR3		0x800C
#define	SMMU_S_IDR4		0x8010
#define	SMMU_S_CR0		0x8020
#define	SMMU_S_CR0ACK		0x8024
#define	SMMU_S_CR1		0x8028
#define	SMMU_S_CR2		0x802C
#define	SMMU_S_INIT		0x803C
#define	SMMU_S_GBPA		0x8044
#define	SMMU_S_AGBPA		0x8048
#define	SMMU_S_IRQ_CTRL		0x8050
#define	SMMU_S_IRQ_CTRLACK	0x8054
#define	SMMU_S_GERROR		0x8060
#define	SMMU_S_GERRORN		0x8064
#define	SMMU_S_GERROR_IRQ_CFG0	0x8068
#define	SMMU_S_GERROR_IRQ_CFG1	0x8070
#define	SMMU_S_GERROR_IRQ_CFG2	0x8074
#define	SMMU_S_STRTAB_BASE	0x8080
#define	SMMU_S_STRTAB_BASE_CFG	0x8088
#define	SMMU_S_CMDQ_BASE	0x8090
#define	SMMU_S_CMDQ_PROD	0x8098
#define	SMMU_S_CMDQ_CONS	0x809C
#define	SMMU_S_EVENTQ_BASE	0x80A0
#define	SMMU_S_EVENTQ_PROD	0x80A8
#define	SMMU_S_EVENTQ_CONS	0x80AC
#define	SMMU_S_EVENTQ_IRQ_CFG0	0x80B0
#define	SMMU_S_EVENTQ_IRQ_CFG1	0x80B8
#define	SMMU_S_EVENTQ_IRQ_CFG2	0x80BC
#define	SMMU_S_GATOS_CTRL	0x8100
#define	SMMU_S_GATOS_SID	0x8108
#define	SMMU_S_GATOS_ADDR	0x8110
#define	SMMU_S_GATOS_PAR	0x8118

#define	CMD_QUEUE_OPCODE_S	0
#define	CMD_QUEUE_OPCODE_M	(0xff << CMD_QUEUE_OPCODE_S)

#define	CMD_PREFETCH_CONFIG	0x01
#define	 PREFETCH_0_SID_S	32
#define	CMD_PREFETCH_ADDR	0x02
#define	CMD_CFGI_STE		0x03
#define	 CFGI_0_STE_SID_S	32
#define	CMD_CFGI_STE_RANGE	0x04
#define	 CFGI_1_STE_RANGE_S	0
#define	CMD_CFGI_CD		0x05
#define	 CFGI_0_SSID_S		12
#define	 CFGI_1_LEAF_S		0
#define	CMD_CFGI_CD_ALL		0x06
#define	CMD_TLBI_NH_ALL		0x10
#define	CMD_TLBI_NH_ASID	0x11
#define	CMD_TLBI_NH_VA		0x12
#define	 TLBI_0_ASID_S		48
#define	 TLBI_1_LEAF		(1 << 0)
#define	 TLBI_1_ADDR_S		12
#define	 TLBI_1_ADDR_M		(0xfffffffffffff << TLBI_1_ADDR_S)
#define	CMD_TLBI_NH_VAA		0x13
#define	CMD_TLBI_EL3_ALL	0x18
#define	CMD_TLBI_EL3_VA		0x1A
#define	CMD_TLBI_EL2_ALL	0x20
#define	CMD_TLBI_EL2_ASID	0x21
#define	CMD_TLBI_EL2_VA		0x22
#define	CMD_TLBI_EL2_VAA	0x23
#define	CMD_TLBI_S12_VMALL	0x28
#define	CMD_TLBI_S2_IPA		0x2A
#define	CMD_TLBI_NSNH_ALL	0x30
#define	CMD_ATC_INV		0x40
#define	CMD_PRI_RESP		0x41
#define	CMD_RESUME		0x44
#define	CMD_STALL_TERM		0x45
#define	CMD_SYNC		0x46
#define	 SYNC_0_CS_S		12 /* The ComplSignal */
#define	 SYNC_0_CS_M		(0x3 << SYNC_0_CS_S)
#define	 SYNC_0_CS_SIG_NONE	(0x0 << SYNC_0_CS_S)
#define	 SYNC_0_CS_SIG_IRQ	(0x1 << SYNC_0_CS_S)
#define	 SYNC_0_CS_SIG_SEV	(0x2 << SYNC_0_CS_S)
#define	 SYNC_0_MSH_S		22 /* Shareability attribute for MSI write */
#define	 SYNC_0_MSH_M		(0x3 << SYNC_0_MSH_S)
#define	 SYNC_0_MSH_NS		(0x0 << SYNC_0_MSH_S) /* Non-shareable */
#define	 SYNC_0_MSH_OS		(0x2 << SYNC_0_MSH_S) /* Outer Shareable */
#define	 SYNC_0_MSH_IS		(0x3 << SYNC_0_MSH_S) /* Inner Shareable */
#define	 SYNC_0_MSIATTR_S	24 /* Write attribute for MSI */
#define	 SYNC_0_MSIATTR_M	(0xf << SYNC_0_MSIATTR_S)
#define	 SYNC_0_MSIATTR_OIWB	(0xf << SYNC_0_MSIATTR_S)
#define	 SYNC_0_MSIDATA_S	32
#define	 SYNC_1_MSIADDRESS_S	2
#define	 SYNC_1_MSIADDRESS_M	(0x3ffffffffffff << SYNC_1_MSIADDRESS_S)
#define	STE0_VALID		(1 << 0) /* Structure contents are valid. */
#define	STE0_CONFIG_S		1
#define	STE0_CONFIG_M		(0x7 << STE0_CONFIG_S)
#define	STE0_CONFIG_ABORT	(0x0 << STE0_CONFIG_S)
#define	STE0_CONFIG_BYPASS	(0x4 << STE0_CONFIG_S)
#define	STE0_CONFIG_S1_TRANS	(0x5 << STE0_CONFIG_S)
#define	STE0_CONFIG_S2_TRANS	(0x6 << STE0_CONFIG_S)
#define	STE0_CONFIG_ALL_TRANS	(0x7 << STE0_CONFIG_S)
#define	STE0_S1FMT_S		4
#define	STE0_S1FMT_M		(0x3 << STE0_S1FMT_S)
#define	STE0_S1FMT_LINEAR	(0x0 << STE0_S1FMT_S)
#define	STE0_S1FMT_4KB_L2	(0x1 << STE0_S1FMT_S)
#define	STE0_S1FMT_64KB_L2	(0x2 << STE0_S1FMT_S)
#define	STE0_S1CONTEXTPTR_S	6
#define	STE0_S1CONTEXTPTR_M	(0x3fffffffffff << STE0_S1CONTEXTPTR_S)
#define	STE0_S1CDMAX_S		59
#define	STE0_S1CDMAX_M		(0x1f << STE0_S1CDMAX_S)

#define	STE1_S1DSS_S		0
#define	STE1_S1DSS_M		(0x3 << STE1_S1DSS_S)
#define	STE1_S1DSS_TERMINATE	(0x0 << STE1_S1DSS_S)
#define	STE1_S1DSS_BYPASS	(0x1 << STE1_S1DSS_S)
#define	STE1_S1DSS_SUBSTREAM0	(0x2 << STE1_S1DSS_S)
#define	STE1_S1CIR_S		2
#define	STE1_S1CIR_M		(0x3 << STE1_S1CIR_S)
#define	STE1_S1CIR_NC		(0x0 << STE1_S1CIR_S)
#define	STE1_S1CIR_WBRA		(0x1 << STE1_S1CIR_S)
#define	STE1_S1CIR_WT		(0x2 << STE1_S1CIR_S)
#define	STE1_S1CIR_WB		(0x3 << STE1_S1CIR_S)
#define	STE1_S1COR_S		4
#define	STE1_S1COR_M		(0x3 << STE1_S1COR_S)
#define	STE1_S1COR_NC		(0x0 << STE1_S1COR_S)
#define	STE1_S1COR_WBRA		(0x1 << STE1_S1COR_S)
#define	STE1_S1COR_WT		(0x2 << STE1_S1COR_S)
#define	STE1_S1COR_WB		(0x3 << STE1_S1COR_S)
#define	STE1_S1CSH_S		6
#define	STE1_S1CSH_NS		(0x0 << STE1_S1CSH_S)
#define	STE1_S1CSH_OS		(0x2 << STE1_S1CSH_S)
#define	STE1_S1CSH_IS		(0x3 << STE1_S1CSH_S)
#define	STE1_S2HWU59		(1 << 8)
#define	STE1_S2HWU60		(1 << 9)
#define	STE1_S2HWU61		(1 << 10)
#define	STE1_S2HWU62		(1 << 11)
#define	STE1_DRE		(1 << 12) /* Destructive Read Enable. */
#define	STE1_CONT_S		13 /* Contiguous Hint */
#define	STE1_CONT_M		(0xf << STE1_CONT_S)
#define	STE1_DCP		(1 << 17) /* Directed Cache Prefetch. */
#define	STE1_PPAR		(1 << 18) /* PRI Page request Auto Responses */
#define	STE1_MEV		(1 << 19) /* Merge Events */
#define	STE1_S1STALLD		(1 << 27) /* Stage 1 Stall Disable */
#define	STE1_EATS_S		28 /* Enable PCIe ATS translation and traffic */
#define	STE1_EATS_M		(0x3 << STE1_EATS_S)
#define	STE1_EATS_ABORT		(0x0 << STE1_EATS_S)
#define	STE1_EATS_FULLATS	(0x1 << STE1_EATS_S) /* Full ATS */
#define	STE1_EATS_S1		(0x2 << STE1_EATS_S) /* Split-stage ATS */
#define	STE1_STRW_S		30 /* StreamWorld control */
#define	STE1_STRW_M		(0x3 << STE1_STRW_S)
#define	STE1_STRW_NS_EL1	(0x0 << STE1_STRW_S)
#define	STE1_STRW_NS_EL2	(0x2 << STE1_STRW_S)
#define	STE1_MEMATTR_S		32
#define	STE1_MTCFG		(1 << 36)
#define	STE1_ALLOCCFG_S		37
#define	STE1_SHCFG_S		44
#define	STE1_SHCFG_M		(0x3UL << STE1_SHCFG_S)
#define	STE1_SHCFG_NS		(0x0UL << STE1_SHCFG_S)
#define	STE1_SHCFG_INCOMING	(0x1UL << STE1_SHCFG_S)
#define	STE1_SHCFG_OS		(0x2UL << STE1_SHCFG_S)
#define	STE1_SHCFG_IS		(0x3UL << STE1_SHCFG_S)
#define	STE1_NSCFG_S		46
#define	STE1_NSCFG_M		(0x3UL << STE1_NSCFG_S)
#define	STE1_NSCFG_SECURE	(0x2UL << STE1_NSCFG_S)
#define	STE1_NSCFG_NONSECURE	(0x3UL << STE1_NSCFG_S)
#define	STE1_PRIVCFG_S		48
#define	STE1_INSTCFG_S		50

#define	STE2_S2VMID_S		0
#define	STE2_S2VMID_M		(0xffff << STE2_S2VMID_S)
#define	STE2_S2T0SZ_S		32 /* Size of IPA input region */
#define	STE2_S2T0SZ_M		(0x3f << STE2_S2T0SZ_S)
#define	STE2_S2SL0_S		38 /* Starting level of stage 2 tt walk */
#define	STE2_S2SL0_M		(0x3 << STE2_S2SL0_S)
#define	STE2_S2IR0_S		40
#define	STE2_S2IR0_M		(0x3 << STE2_S2IR0_S)
#define	STE2_S2OR0_S		42
#define	STE2_S2OR0_M		(0x3 << STE2_S2OR0_S)
#define	STE2_S2SH0_S		44
#define	STE2_S2SH0_M		(0x3 << STE2_S2SH0_S)
#define	STE2_S2TG_S		46
#define	STE2_S2TG_M		(0x3 << STE2_S2TG_S)
#define	STE2_S2PS_S		48 /* Physical address Size */
#define	STE2_S2PS_M		(0x7 << STE2_S2PS_S)
#define	STE2_S2AA64		(1 << 51) /* Stage 2 tt is AArch64 */
#define	STE2_S2ENDI		(1 << 52) /* Stage 2 tt endianness */
#define	STE2_S2AFFD		(1 << 53) /* Stage 2 Access Flag Fault Disable*/
#define	STE2_S2PTW		(1 << 54) /* Protected Table Walk */
#define	STE2_S2S		(1 << 57)
#define	STE2_S2R		(1 << 58)

#define	STE3_S2TTB_S		4 /* Address of Translation Table base */
#define	STE3_S2TTB_M		(0xffffffffffff << STE3_S2TTB_S)

#define	CD0_T0SZ_S		0 /* VA region size covered by TT0. */
#define	CD0_T0SZ_M		(0x3f << CD0_T0SZ_S)
#define	CD0_TG0_S		6 /* TT0 Translation Granule size */
#define	CD0_TG0_M		(0x3 << CD0_TG0_S)
#define	CD0_TG0_4KB		(0x0 << CD0_TG0_S)
#define	CD0_TG0_64KB		(0x1 << CD0_TG0_S)
#define	CD0_TG0_16KB		(0x2 << CD0_TG0_S)
#define	CD0_IR0_S		8 /* Inner region Cacheability for TT0 access*/
#define	CD0_IR0_M		(0x3 << CD0_IR0_S)
#define	CD0_IR0_NC		(0x0 << CD0_IR0_S)
#define	CD0_IR0_WBC_RWA		(0x1 << CD0_IR0_S)
#define	CD0_IR0_WTC_RA		(0x2 << CD0_IR0_S)
#define	CD0_IR0_WBC_RA		(0x3 << CD0_IR0_S)
#define	CD0_OR0_S		10 /* Outer region Cacheability for TT0 access*/
#define	CD0_OR0_M		(0x3 << CD0_OR0_S)
#define	CD0_OR0_NC		(0x0 << CD0_OR0_S)
#define	CD0_OR0_WBC_RWA		(0x1 << CD0_OR0_S)
#define	CD0_OR0_WTC_RA		(0x2 << CD0_OR0_S)
#define	CD0_OR0_WBC_RA		(0x3 << CD0_OR0_S)
#define	CD0_SH0_S		12 /* Shareability for TT0 access */
#define	CD0_SH0_M		(0x3 << CD0_SH0_S)
#define	CD0_SH0_NS		(0x0 << CD0_SH0_S)
#define	CD0_SH0_OS		(0x2 << CD0_SH0_S) /* Outer Shareable */
#define	CD0_SH0_IS		(0x3 << CD0_SH0_S) /* Inner Shareable */
#define	CD0_EPD0		(1 << 14) /* TT0 walk disable */
#define	CD0_ENDI		(1 << 15) /* Big Endian */
#define	CD0_T1SZ_S		16 /* VA region size covered by TT1 */
#define	CD0_T1SZ_M		(0x3f << CD0_T1SZ_S)
#define	CD0_TG1_S		22 /* TT1 Translation Granule size */
#define	CD0_TG1_M		(0x3 << CD0_TG1_S)
#define	CD0_TG1_4KB		(0x2 << CD0_TG1_S)
#define	CD0_TG1_64KB		(0x3 << CD0_TG1_S)
#define	CD0_TG1_16KB		(0x1 << CD0_TG1_S)
#define	CD0_IR1_S		24 /* Inner region Cacheability for TT1 access*/
#define	CD0_IR1_M		(0x3 << CD0_IR1_S)
#define	CD0_OR1_S		26
#define	CD0_OR1_M		(0x3 << CD0_OR1_S)
#define	CD0_SH1_S		28
#define	CD0_SH1_M		(0x3 << CD0_SH1_S)
#define	CD0_EPD1		(1UL << 30) /* TT1 tt walk disable*/
#define	CD0_VALID		(1UL << 31) /* CD Valid. */
#define	CD0_IPS_S		32 /* Intermediate Physical Size */
#define	CD0_IPS_M		(0x7UL << CD0_IPS_S)
#define	CD0_IPS_32BITS		(0x0UL << CD0_IPS_S)
#define	CD0_IPS_36BITS		(0x1UL << CD0_IPS_S)
#define	CD0_IPS_40BITS		(0x2UL << CD0_IPS_S)
#define	CD0_IPS_42BITS		(0x3UL << CD0_IPS_S)
#define	CD0_IPS_44BITS		(0x4UL << CD0_IPS_S)
#define	CD0_IPS_48BITS		(0x5UL << CD0_IPS_S)
#define	CD0_IPS_52BITS		(0x6UL << CD0_IPS_S) /* SMMUv3.1 only */
#define	CD0_AFFD		(1UL << 35) /* Access Flag Fault Disable */
#define	CD0_WXN			(1UL << 36) /* Write eXecute Never */
#define	CD0_UWXN		(1UL << 37) /* Unprivileged Write eXecut Never*/
#define	CD0_TBI0		(1UL << 38) /* Top Byte Ignore for TTB0 */
#define	CD0_TBI1		(1UL << 39) /* Top Byte Ignore for TTB1 */
#define	CD0_PAN			(1UL << 40) /* Privileged Access Never */
#define	CD0_AA64		(1UL << 41) /* TTB{0,1} is AArch64-format TT */
#define	CD0_HD			(1UL << 42)
#define	CD0_HA			(1UL << 43)
#define	CD0_S			(1UL << 44)
#define	CD0_R			(1UL << 45)
#define	CD0_A			(1UL << 46)
#define	CD0_ASET		(1UL << 47) /* ASID Set. */
#define	CD0_ASID_S		48 /* Address Space Identifier */
#define	CD0_ASID_M		(0xffff << CD0_ASID_S)
#define	CD1_TTB0_S		4 /* Address of TT0 base. */
#define	CD1_TTB0_M		(0xffffffffffff << CD1_TTB0_S)

#endif /* _ARM64_IOMMU_SMMUREG_H_ */
