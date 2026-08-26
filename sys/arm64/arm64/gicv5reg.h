/*
 * Copyright (c) 2025 Arm Ltd
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef _ARM64_GICV5REG_H_
#define	_ARM64_GICV5REG_H_

#include <machine/_armreg.h>

/*
 * GIC instructions in the SYS instruction space
 */

/*
 * GIC CDAFF, <Xt>
 * Interrupt Set Target in the Current Interrupt Domain
 */
#define	gic_cdaff(val)			SYS_ARG(GIC_CDAFF, val)
#define	GIC_CDAFF_op0			1
#define	GIC_CDAFF_op1			0
#define	GIC_CDAFF_CRn			12
#define	GIC_CDAFF_CRm			1
#define	GIC_CDAFF_op2			3
#define	GIC_CDAFF_IAFFID_SHIFT		32
#define	GIC_CDAFF_IAFFID_MASK		(0xfffful << GIC_CDAFF_IAFFID_SHIFT)
#define	GIC_CDAFF_IAFFID(x)		\
    ((uint64_t)(x) << GIC_CDAFF_IAFFID_SHIFT)
#define	GIC_CDAFF_TYPE_SHIFT		29
#define	GIC_CDAFF_TYPE_MASK		(0x7ul << GIC_CDAFF_TYPE_SHIFT)
#define	GIC_CDAFF_TYPE_LPI		(0x2ul << GIC_CDAFF_TYPE_SHIFT)
#define	GIC_CDAFF_TYPE_SPI		(0x3ul << GIC_CDAFF_TYPE_SHIFT)
#define	GIC_CDAFF_IRM_SHIFT		28
#define	GIC_CDAFF_IRM_MASK		(0x1ul << GIC_CDAFF_IRM_SHIFT)
#define	GIC_CDAFF_IRM_TARGETED		(0x0ul << GIC_CDAFF_IRM_SHIFT)
#define	GIC_CDAFF_IRM_1ofN		(0x1ul << GIC_CDAFF_IRM_SHIFT)
#define	GIC_CDAFF_ID_SHIFT		0
#define	GIC_CDAFF_ID_MASK		(0xfffffful << GIC_CDAFF_ID_SHIFT)
#define	GIC_CDAFF_ID(x)			((uint64_t)(x) << GIC_CDAFF_ID_SHIFT)

/*
 * GIC CDDI, <Xt>
 * Interrupt Deactivate in the Current Interrupt Domain
 */
#define	gic_cddi(val)			SYS_ARG(GIC_CDDI, val)
#define	GIC_CDDI_op0			1
#define	GIC_CDDI_op1			0
#define	GIC_CDDI_CRn			12
#define	GIC_CDDI_CRm			2
#define	GIC_CDDI_op2			0
#define	GIC_CDDI_Type_SHIFT		29
#define	GIC_CDDI_Type_MASK		(0x7ul << GIC_CDDI_Type_SHIFT)
#define	GIC_CDDI_Type_PPI		(0x1ul << GIC_CDDI_Type_SHIFT)
#define	GIC_CDDI_Type_LPI		(0x2ul << GIC_CDDI_Type_SHIFT)
#define	GIC_CDDI_Type_SPI		(0x3ul << GIC_CDDI_Type_SHIFT)
#define	GIC_CDDI_ID_SHIFT		0
#define	GIC_CDDI_ID_MASK		(0xfffffful << GIC_CDDI_ID_SHIFT)

/*
 * GIC CDDIS, <Xt>
 * Interrupt Disable in the Current Interrupt Domain
 */
#define	gic_cddis(val)			SYS_ARG(GIC_CDDIS, val)
#define	GIC_CDDIS_op0			1
#define	GIC_CDDIS_op1			0
#define	GIC_CDDIS_CRn			12
#define	GIC_CDDIS_CRm			1
#define	GIC_CDDIS_op2			0
#define	GIC_CDDIS_TYPE_SHIFT		29
#define	GIC_CDDIS_TYPE_WIDTH		3
#define	GIC_CDDIS_TYPE_MASK		(UL(0x7) << GIC_CDDIS_TYPE_SHIFT)
#define	 GIC_CDDIS_TYPE_LPI		(UL(0x2) << GIC_CDDIS_TYPE_SHIFT)
#define	 GIC_CDDIS_TYPE_SPI		(UL(0x3) << GIC_CDDIS_TYPE_SHIFT)
#define	GIC_CDDIS_ID_SHIFT		0
#define	GIC_CDDIS_ID_WIDTH		24
#define	GIC_CDDIS_ID_MASK		(UL(0xffffff) << GIC_CDDIS_ID_SHIFT)

/*
 * GIC CDEN, <Xt>
 * Interrupt Enable in the Current Interrupt Domain
 */
#define	gic_cden(val)			SYS_ARG(GIC_CDEN, val)
#define	GIC_CDEN_op0			1
#define	GIC_CDEN_op1			0
#define	GIC_CDEN_CRn			12
#define	GIC_CDEN_CRm			1
#define	GIC_CDEN_op2			1
#define	GIC_CDEN_TYPE_SHIFT		29
#define	GIC_CDEN_TYPE_WIDTH		3
#define	GIC_CDEN_TYPE_MASK		(UL(0x7) << GIC_CDEN_TYPE_SHIFT)
#define	 GIC_CDEN_TYPE_LPI		(UL(0x2) << GIC_CDEN_TYPE_SHIFT)
#define	 GIC_CDEN_TYPE_SPI		(UL(0x3) << GIC_CDEN_TYPE_SHIFT)
#define	GIC_CDEN_ID_SHIFT		0
#define	GIC_CDEN_ID_WIDTH		24
#define	GIC_CDEN_ID_MASK		(UL(0xffffff) << GIC_CDEN_ID_SHIFT)

/*
 * GIC CDEOI, <Xt>
 * Priority Drop in the Current Interrupt Domain
 */
#define	gic_cdeoi()			SYS(GIC_CDEOI)
#define	GIC_CDEOI_op0			1
#define	GIC_CDEOI_op1			0
#define	GIC_CDEOI_CRn			12
#define	GIC_CDEOI_CRm			1
#define	GIC_CDEOI_op2			7

/*
 * GIC CDHM, <Xt>
 * Interrupt Handling mode state in the Current Interrupt Domain
 */
#define	gic_cdhm(val)			SYS_ARG(GIC_CDHM, val)
#define	GIC_CDHM_op0			1
#define	GIC_CDHM_op1			0
#define	GIC_CDHM_CRn			12
#define	GIC_CDHM_CRm			2
#define	GIC_CDHM_op2			1
#define	GIC_CDHM_HM_MASK		(0x1ul << 32)
#define	GIC_CDHM_HM_EDGE		(0x0ul << 32)
#define	GIC_CDHM_HM_LEVEL		(0x1ul << 32)
#define	GIC_CDHM_TYPE_SHIFT		29
#define	GIC_CDHM_TYPE_MASK		(0x7ul << GIC_CDHM_TYPE_SHIFT)
#define	GIC_CDHM_TYPE_PPI		(0x1ul << GIC_CDHM_TYPE_SHIFT)
#define	GIC_CDHM_TYPE_LPI		(0x2ul << GIC_CDHM_TYPE_SHIFT)
#define	GIC_CDHM_TYPE_SPI		(0x3ul << GIC_CDHM_TYPE_SHIFT)
#define	GIC_CDHM_ID_SHIFT		0
#define	GIC_CDHM_ID_MASK		(0xfffffful << GIC_CDHM_ID_SHIFT)

/*
 * GIC CDPEND, <Xt>
 * Interrupt Set/Clear Pending state in the Current Interrupt Domain
 */
#define	gic_cdpend(val)			SYS_ARG(GIC_CDPEND, val);
#define	GIC_CDPEND_op0			1
#define	GIC_CDPEND_op1			0
#define	GIC_CDPEND_CRn			12
#define	GIC_CDPEND_CRm			1
#define	GIC_CDPEND_op2			4
#define	GIC_CDPEND_PENDING_SHIFT	32
#define	GIC_CDPEND_PENDING_MASK		(0x1ul << GIC_CDPEND_PENDING_SHIFT)
#define	GIC_CDPEND_PENDING_CLEAR	(0x0ul << GIC_CDPEND_PENDING_SHIFT)
#define	GIC_CDPEND_PENDING_SET		(0x1ul << GIC_CDPEND_PENDING_SHIFT)
#define	GIC_CDPEND_TYPE_SHIFT		29
#define	GIC_CDPEND_TYPE_MASK		(0x7ul << GIC_CDPEND_TYPE_SHIFT)
#define	GIC_CDPEND_TYPE_LPI		(0x2ul << GIC_CDPEND_TYPE_SHIFT)
#define	GIC_CDPEND_TYPE_SPI		(0x2ul << GIC_CDPEND_TYPE_SHIFT)
#define	GIC_CDPEND_ID_SHIFT		0
#define	GIC_CDPEND_ID_MASK		(0xfffffful << GIC_CDPEND_ID_SHIFT)
#define	GIC_CDPEND_ID(x)		((uint64_t)(x) << GIC_CDPEND_ID_SHIFT)

/*
 * GIC CDPRI, <Xt>
 * Interrupt Set priority in the Current Interrupt Domain
 */
#define	gic_cdpri(val)			SYS_ARG(GIC_CDPRI, val)
#define	GIC_CDPRI_op0			1
#define	GIC_CDPRI_op1			0
#define	GIC_CDPRI_CRn			12
#define	GIC_CDPRI_CRm			1
#define	GIC_CDPRI_op2			2
#define	GIC_CDPRI_PRORITY_SHIFT		35
#define	GIC_CDPRI_PRORITY_MASK		(0x1ful << GIC_CDPRI_PRORITY_SHIFT)
#define	GIC_CDPRI_PRORITY(x)		\
    ((uint64_t)(x) << GIC_CDPRI_PRORITY_SHIFT)
#define	GIC_CDPRI_TYPE_SHIFT		29
#define	GIC_CDPRI_TYPE_MASK		(0x7ul << GIC_CDPRI_TYPE_SHIFT)
#define	GIC_CDPRI_TYPE_LPI		(0x2ul << GIC_CDPRI_TYPE_SHIFT)
#define	GIC_CDPRI_TYPE_SPI		(0x3ul << GIC_CDPRI_TYPE_SHIFT)
#define	GIC_CDPRI_ID_SHIFT		0
#define	GIC_CDPRI_ID_MASK		(0xfffffful << GIC_CDPRI_ID_SHIFT)
#define	GIC_CDPRI_ID(x)			((uint64_t)(x) << GIC_CDPRI_ID_SHIFT)

/*
 * GIC CDRCFG, <Xt>
 * Interrupt Set priority in the Current Interrupt Domain
 */
#define	gic_cdrcfg(val)			SYS_ARG(GIC_CDRCFG, val)
#define	GIC_CDRCFG_op0			1
#define	GIC_CDRCFG_op1			0
#define	GIC_CDRCFG_CRn			12
#define	GIC_CDRCFG_CRm			1
#define	GIC_CDRCFG_op2			5

/*
 * GICR <Xt>, CDIA
 * Interrupt Acknowledge in the Current Interrupt Domain
 */
#define	gicr_cdia()			SYSL(GICR_CDIA)
#define	GICR_CDIA_op0			1
#define	GICR_CDIA_op1			0
#define	GICR_CDIA_CRn			12
#define	GICR_CDIA_CRm			3
#define	GICR_CDIA_op2			0


/*
 * GICR <Xt>, CDNMIA
 * Non-maskable Interrupt Acknowledge in the Current Interrupt Domain
 */
#define	gicr_cdnmia()			SYSL(GICR_CDNMIA)
#define	GICR_CDNMIA_op0			1
#define	GICR_CDNMIA_op1			0
#define	GICR_CDNMIA_CRn			12
#define	GICR_CDNMIA_CRm			3
#define	GICR_CDNMIA_op2			1

/*
 * GIC VDAFF, <Xt>
 * Interrupt Set Target in the Current Interrupt Domain
 */
#define	GIC_VDAFF			MRS_REG_ALT_NAME(GIC_VDAFF)
#define	GIC_VDAFF_op0			1
#define	GIC_VDAFF_op1			4
#define	GIC_VDAFF_CRn			12
#define	GIC_VDAFF_CRm			1
#define	GIC_VDAFF_op2			3

/*
 * GIC VDEN, <Xt>
 * Interrupt Enable in the Virtual Interrupt Domain
 */
#define	GIC_VDEN			MRS_REG_ALT_NAME(GIC_VDEN)
#define	GIC_VDEN_op0			1
#define	GIC_VDEN_op1			4
#define	GIC_VDEN_CRn			12
#define	GIC_VDEN_CRm			1
#define	GIC_VDEN_op2			1
#define	GIC_VDEN_TYPE_SHIFT		29
#define	GIC_VDEN_TYPE_WIDTH		3
#define	GIC_VDEN_TYPE_MASK		(UL(0x7) << GIC_VDEN_TYPE_SHIFT)
#define	 GIC_VDEN_TYPE_LPI		(UL(0x2) << GIC_VDEN_TYPE_SHIFT)
#define	 GIC_VDEN_TYPE_SPI		(UL(0x3) << GIC_VDEN_TYPE_SHIFT)
#define	GIC_VDEN_ID_SHIFT		0
#define	GIC_VDEN_ID_WIDTH		24
#define	GIC_VDEN_ID_MASK		(UL(0xffffff) << GIC_VDEN_ID_SHIFT)

/*
 * GIC VDPEND, <Xt>
 * Interrupt Set/Clear Pending state in the Virtual Interrupt Domain
 */
#define	GIC_VDPEND			MRS_REG_ALT_NAME(GIC_VDPEND)
#define	GIC_VDPEND_op0			1
#define	GIC_VDPEND_op1			4
#define	GIC_VDPEND_CRn			12
#define	GIC_VDPEND_CRm			1
#define	GIC_VDPEND_op2			4
#define	GIC_VDPEND_PENDING		(UL(0x1) << 63)
#define	GIC_VDPEND_VM_SHIFT		32
#define	GIC_VDPEND_VM_MASK		(UL(0xffff) << GIC_VDPEND_VM_SHIFT)
#define	GIC_VDPEND_TYPE_SHIFT		29
#define	GIC_VDPEND_TYPE_WIDTH		3
#define	GIC_VDPEND_TYPE_MASK		(UL(0x7) << GIC_VDPEND_TYPE_SHIFT)
#define	 GIC_VDPEND_TYPE_LPI		(UL(0x2) << GIC_VDPEND_TYPE_SHIFT)
#define	 GIC_VDPEND_TYPE_SPI		(UL(0x3) << GIC_VDPEND_TYPE_SHIFT)
#define	GIC_VDPEND_ID_SHIFT		0
#define	GIC_VDPEND_ID_WIDTH		24
#define	GIC_VDPEND_ID_MASK		(UL(0xffffff) << GIC_VDPEND_ID_SHIFT)

/*
 * GIC VDPRI, <Xt>
 * Interrupt Set priority in the Virtual Interrupt Domain
 */
#define	GIC_VDPRI			MRS_REG_ALT_NAME(GIC_VDPRI)
#define	GIC_VDPRI_op0			1
#define	GIC_VDPRI_op1			4
#define	GIC_VDPRI_CRn			12
#define	GIC_VDPRI_CRm			1
#define	GIC_VDPRI_op2			2

/*
 * GIC VDRCFG, <Xt>
 *
 */
#define	GIC_VDRCFG			MRS_REG_ALT_NAME(GIC_VDRCFG)
#define	GIC_VDRCFG_op0			1
#define	GIC_VDRCFG_op1			4
#define	GIC_VDRCFG_CRn			12
#define	GIC_VDRCFG_CRm			1
#define	GIC_VDRCFG_op2			5

/*
 * GSB ACK
 * GIC Synchronization Barrier Interrupt Acknowledge
 */
#define	gsb_ack()			SYS(GSB_ACK)
#define	GSB_ACK_op0			1
#define	GSB_ACK_op1			0
#define	GSB_ACK_CRn			12
#define	GSB_ACK_CRm			0
#define	GSB_ACK_op2			1

/*
 * GSB SYS
 * GIC Synchronization Barrier System
 */
#define	gsb_sys()			SYS(GSB_SYS)
#define	GSB_SYS_op0			1
#define	GSB_SYS_op1			0
#define	GSB_SYS_CRn			12
#define	GSB_SYS_CRm			0
#define	GSB_SYS_op2			0


/*
 * CPU interface registers
 */

/*
 * ICC_APR_EL1
 * Interrupt Controller Physical Active Priorities Register
 */
#define	ICC_APR_EL1			MRS_REG_ALT_NAME(ICC_APR_EL1)
#define	ICC_APR_EL1_op0			3
#define	ICC_APR_EL1_op1			1
#define	ICC_APR_EL1_CRn			12
#define	ICC_APR_EL1_CRm			0
#define	ICC_APR_EL1_op2			0
#define	ICC_APR_P(x)			(1ul << (x))

/*
 * ICC_CR0_EL1
 * Interrupt Controller EL1 Physical Control Register
 */
#define	ICC_CR0_EL1			MRS_REG_ALT_NAME(ICC_CR0_EL1)
#define	ICC_CR0_EL1_op0			3
#define	ICC_CR0_EL1_op1			1
#define	ICC_CR0_EL1_CRn			12
#define	ICC_CR0_EL1_CRm			0
#define	ICC_CR0_EL1_op2			1
#define	ICC_CR0_PID			(0x1ul << 38)
#define	ICC_CR0_IPPT_SHIFT		32
#define	ICC_CR0_IPPT_MASK		(0x1ful << ICC_CR0_IPPT_SHIFT)
#define	ICC_CR0_EN			(0x1ul << 0)

/*
 * ICC_HAPR_EL1
 * Interrupt Controller Physical Highest Active Priority Register
 */
#define	ICC_HAPR_EL1			MRS_REG_ALT_NAME(ICC_HAPR_EL1)
#define	ICC_HAPR_EL1_op0		3
#define	ICC_HAPR_EL1_op1		1
#define	ICC_HAPR_EL1_CRn		12
#define	ICC_HAPR_EL1_CRm		0
#define	ICC_HAPR_EL1_op2		3
#define	ICC_HAPR_PRIORITY_SHIFT		0
#define	ICC_HAPR_PRIORITY_MASK		(0xfful << ICC_HAPR_PRIORITY_SHIFT)

/*
 * ICC_HPPIR_EL1
 * Interrupt Controller Physical Highest Priority Pending Interrupt Register
 */
#define	ICC_HPPIR_EL1			MRS_REG_ALT_NAME(ICC_HPPIR_EL1)
#define	ICC_HPPIR_EL1_op0		3
#define	ICC_HPPIR_EL1_op1		0
#define	ICC_HPPIR_EL1_CRn		12
#define	ICC_HPPIR_EL1_CRm		10
#define	ICC_HPPIR_EL1_op2		3
#define	ICC_HPPIR_HPPIV			(0x1ul << 32)
#define	ICC_HPPIR_TYPE_SHIFT		29
#define	ICC_HPPIR_TYPE_MASK		(0x7ul << ICC_HPPIR_TYPE_SHIFT)
#define	ICC_HPPIR_TYPE_PPI		(0x1ul << ICC_HPPIR_TYPE_SHIFT)
#define	ICC_HPPIR_TYPE_LPI		(0x2ul << ICC_HPPIR_TYPE_SHIFT)
#define	ICC_HPPIR_TYPE_SPI		(0x3ul << ICC_HPPIR_TYPE_SHIFT)
#define	ICC_HPPIR_ID_SHIFT		0
#define	ICC_HPPIR_ID_MASK		(0xfffffful << ICC_HPPIR_ID_SHIFT)

/*
 * ICC_IAFFID_EL1
 * Interrupt Controller PE Interrupt Affinity ID Register
 */
#define	ICC_IAFFIDR_EL1			MRS_REG_ALT_NAME(ICC_IAFFIDR_EL1)
#define	ICC_IAFFIDR_EL1_op0		3
#define	ICC_IAFFIDR_EL1_op1		0
#define	ICC_IAFFIDR_EL1_CRn		12
#define	ICC_IAFFIDR_EL1_CRm		10
#define	ICC_IAFFIDR_EL1_op2		5
#define	ICC_IAFFIDR_IAFFID_SHIFT	0
#define	ICC_IAFFIDR_IAFFID_MASK		(0xfffful << ICC_IAFFIDR_IAFFID_SHIFT)
#define	ICC_IAFFIDR_IAFFID_VAL(x)	\
    (((x) & ICC_IAFFIDR_IAFFID_MASK) >> ICC_IAFFIDR_IAFFID_SHIFT)

/*
 * ICC_ICSR_EL1
 * Interrupt Controller Interrupt Configuration and State Register
 */
#define	ICC_ICSR_EL1			MRS_REG_ALT_NAME(ICC_ICSR_EL1)
#define	ICC_ICSR_EL1_op0		3
#define	ICC_ICSR_EL1_op1		0
#define	ICC_ICSR_EL1_CRn		12
#define	ICC_ICSR_EL1_CRm		10
#define	ICC_ICSR_EL1_op2		4
#define	ICC_ICSR_IAFFID_SHIFT		32
#define	ICC_ICSR_IAFFID_MASK		(0xfffful << ICC_ICSR_IAFFID_SHIFT)
#define	ICC_ICSR_Priority_SHIFT		11
#define	ICC_ICSR_Priority_MASK		(0x1ful << ICC_ICSR_Priority_SHIFT)
#define	ICC_ICSR_HM			(0x1ul << 5)
#define	ICC_ICSR_Active			(0x1ul << 4)
#define	ICC_ICSR_IRM			(0x1ul << 3)
#define	ICC_ICSR_Pending		(0x1ul << 2)
#define	ICC_ICSR_Enabled		(0x1ul << 1)
#define	ICC_ICSR_F			(0x1ul << 0)

/*
 * ICC_IDR0_EL1
 * Interrupt Controller ID Register 0
 */
#define	ICC_IDR0_EL1			MRS_REG_ALT_NAME(ICC_IDR0_EL1)
#define	ICC_IDR0_EL1_ISS		ISS_MSR_REG(ICC_IDR0_EL1)
#define	ICC_IDR0_EL1_op0		3
#define	ICC_IDR0_EL1_op1		0
#define	ICC_IDR0_EL1_CRn		12
#define	ICC_IDR0_EL1_CRm		10
#define	ICC_IDR0_EL1_op2		2
#define	ICC_IDR0_GCIE_LEGACY_SHIFT	8
#define	ICC_IDR0_GCIE_LEGACY_WIDTH	4
#define	ICC_IDR0_GCIE_LEGACY_MASK	(0xful << ICC_IDR0_GCIE_LEGACY_SHIFT)
#define	ICC_IDR0_GCIE_LEGACY_VAL(x)	((x) & ICC_IDR0_GCIE_LEGACY_MASK)
#define	 ICC_IDR0_GCIE_LEGACY_NONE	(UL(0x0) << ICC_IDR0_GCIE_LEGACY_SHIFT)
#define	 ICC_IDR0_GCIE_LEGACY_IMPL	(UL(0x1) << ICC_IDR0_GCIE_LEGACY_SHIFT)
#define	ICC_IDR0_PRI_BITS_SHIFT		4
#define	ICC_IDR0_PRI_BITS_MASK		(0xful << ICC_IDR0_PRI_BITS_SHIFT)
#define	ICC_IDR0_PRI_BITS_4		(0x3ul << ICC_IDR0_PRI_BITS_SHIFT)
#define	ICC_IDR0_PRI_BITS_5		(0x4ul << ICC_IDR0_PRI_BITS_SHIFT)
#define	ICC_IDR0_ID_BITS_SHIFT		0
#define	ICC_IDR0_ID_BITS_MASK		(0xful << ICC_IDR0_ID_BITS_SHIFT)
#define	ICC_IDR0_ID_BITS_16		(0x0ul << ICC_IDR0_ID_BITS_SHIFT)
#define	ICC_IDR0_ID_BITS_24		(0x1ul << ICC_IDR0_ID_BITS_SHIFT)

/*
 * ICC_PCR_EL1
 * Interrupt Controller Physical Interrupt Priority Control Register
 * This register is self-synchronizing
 */
#define	ICC_PCR_EL1			MRS_REG_ALT_NAME(ICC_PCR_EL1)
#define	ICC_PCR_EL1_op0			3
#define	ICC_PCR_EL1_op1			1
#define	ICC_PCR_EL1_CRn			12
#define	ICC_PCR_EL1_CRm			0
#define	ICC_PCR_EL1_op2			2
#define	ICC_PCR_PRIORITY_SHIFT		0
#define	ICC_PCR_PRIORITY_MASK		(0x1ful << ICC_PCR_PRIORITY_SHIFT)
#define	ICC_PCR_PRIORITY_LOWEST		(0x1ful << ICC_PCR_PRIORITY_SHIFT)

/*
 * PPI registers
 */

/*
 * ICC_PPI_CACTIVER<n>_EL1
 * Interrupt Controller Physical PPI Clear Active Register
 */
#define	ICC_PPI_CACTIVER0_EL1		MRS_REG_ALT_NAME(ICC_PPI_CACTIVER0_EL1)
#define	ICC_PPI_CACTIVER0_EL1_op0	3
#define	ICC_PPI_CACTIVER0_EL1_op1	0
#define	ICC_PPI_CACTIVER0_EL1_CRn	12
#define	ICC_PPI_CACTIVER0_EL1_CRm	13
#define	ICC_PPI_CACTIVER0_EL1_op2	0

#define	ICC_PPI_CACTIVER1_EL1		MRS_REG_ALT_NAME(ICC_PPI_CACTIVER1_EL1)
#define	ICC_PPI_CACTIVER1_EL1_op0	3
#define	ICC_PPI_CACTIVER1_EL1_op1	0
#define	ICC_PPI_CACTIVER1_EL1_CRn	12
#define	ICC_PPI_CACTIVER1_EL1_CRm	13
#define	ICC_PPI_CACTIVER1_EL1_op2	1

/*
 * ICC_PPI_CPENDR<n>_EL1
 * Interrupt Controller Physical PPI Clear Pending State Registers
 */
#define	ICC_PPI_CPENDR0_EL1		MRS_REG_ALT_NAME(ICC_PPI_CPENDR0_EL1)
#define	ICC_PPI_CPENDR0_EL1_op0		3
#define	ICC_PPI_CPENDR0_EL1_op1		0
#define	ICC_PPI_CPENDR0_EL1_CRn		12
#define	ICC_PPI_CPENDR0_EL1_CRm		13
#define	ICC_PPI_CPENDR0_EL1_op2		4

#define	ICC_PPI_CPENDR1_EL1		MRS_REG_ALT_NAME(ICC_PPI_CPENDR1_EL1)
#define	ICC_PPI_CPENDR1_EL1_op0		3
#define	ICC_PPI_CPENDR1_EL1_op1		0
#define	ICC_PPI_CPENDR1_EL1_CRn		12
#define	ICC_PPI_CPENDR1_EL1_CRm		13
#define	ICC_PPI_CPENDR1_EL1_op2		5

/*
 * ICC_PPI_ENABLER<n>_EL1
 * Interrupt Controller Physical PPI Enable Registers
 */
#define	ICC_PPI_ENABLER0_EL1		MRS_REG_ALT_NAME(ICC_PPI_ENABLER0_EL1)
#define	ICC_PPI_ENABLER0_EL1_op0	3
#define	ICC_PPI_ENABLER0_EL1_op1	0
#define	ICC_PPI_ENABLER0_EL1_CRn	12
#define	ICC_PPI_ENABLER0_EL1_CRm	10
#define	ICC_PPI_ENABLER0_EL1_op2	6

#define	ICC_PPI_ENABLER1_EL1		MRS_REG_ALT_NAME(ICC_PPI_ENABLER1_EL1)
#define	ICC_PPI_ENABLER1_EL1_op0	3
#define	ICC_PPI_ENABLER1_EL1_op1	0
#define	ICC_PPI_ENABLER1_EL1_CRn	12
#define	ICC_PPI_ENABLER1_EL1_CRm	10
#define	ICC_PPI_ENABLER1_EL1_op2	7

#define	ICC_PPI_ENABLER_MASK(x)		(UL(1) << ((x) & 0x3f))
#define	ICC_PPI_ENABLER_DIS(x)		(UL(0) << ((x) & 0x3f))
#define	ICC_PPI_ENABLER_EN(x)		(UL(1) << ((x) & 0x3f))
#define	ICC_PPI_ENABLER_NONE		UL(0)

/*
 * ICC_PPI_HMR<n>_EL1
 * Interrupt Controller Physical PPI Handling mode Register
 */
#define	ICC_PPI_HMR0_EL1		MRS_REG_ALT_NAME(ICC_PPI_HMR0_EL1)
#define	ICC_PPI_HMR0_EL1_op0		3
#define	ICC_PPI_HMR0_EL1_op1		0
#define	ICC_PPI_HMR0_EL1_CRn		12
#define	ICC_PPI_HMR0_EL1_CRm		10
#define	ICC_PPI_HMR0_EL1_op2		0

#define	ICC_PPI_HMR1_EL1		MRS_REG_ALT_NAME(ICC_PPI_HMR1_EL1)
#define	ICC_PPI_HMR1_EL1_op0		3
#define	ICC_PPI_HMR1_EL1_op1		0
#define	ICC_PPI_HMR1_EL1_CRn		12
#define	ICC_PPI_HMR1_EL1_CRm		10
#define	ICC_PPI_HMR1_EL1_op2		1

#define	ICC_PPI_HMR_MASK(x)		(UL(1) << ((x) & 0x3f))
#define	ICC_PPI_HMR_EDGE(x)		(UL(0) << ((x) & 0x3f))
#define	ICC_PPI_HMR_LEVEL(x)		(UL(1) << ((x) & 0x3f))

/*
 * ICC_PPI_PRIORITYR<n>_EL1
 * Interrupt Controller Physical PPI Priority Registers
 */
#define	ICC_PPI_PRIORITYR0_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 0)
#define	ICC_PPI_PRIORITYR1_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 1)
#define	ICC_PPI_PRIORITYR2_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 2)
#define	ICC_PPI_PRIORITYR3_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 3)
#define	ICC_PPI_PRIORITYR4_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 4)
#define	ICC_PPI_PRIORITYR5_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 5)
#define	ICC_PPI_PRIORITYR6_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 6)
#define	ICC_PPI_PRIORITYR7_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 14, 7)
#define	ICC_PPI_PRIORITYR8_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 0)
#define	ICC_PPI_PRIORITYR9_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 1)
#define	ICC_PPI_PRIORITYR10_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 2)
#define	ICC_PPI_PRIORITYR11_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 3)
#define	ICC_PPI_PRIORITYR12_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 4)
#define	ICC_PPI_PRIORITYR13_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 5)
#define	ICC_PPI_PRIORITYR14_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 6)
#define	ICC_PPI_PRIORITYR15_EL1		__MRS_REG_ALT_NAME(3, 0, 12, 15, 7)
#define	ICC_PPI_PRIORITYR_PRIORITY_SHIFT(n)	((n) * 8)
#define	ICC_PPI_PRIORITYR_PRIORITY(n, x)	\
    ((uint64_t)(x) << ICC_PPI_PRIORITYR_PRIORITY_SHIFT(n))
#define	ICC_PPI_PRIORITYR_PRIORITY_ALL(x)	\
	(ICC_PPI_PRIORITYR_PRIORITY(0, x) | ICC_PPI_PRIORITYR_PRIORITY(1, x) | \
	 ICC_PPI_PRIORITYR_PRIORITY(2, x) | ICC_PPI_PRIORITYR_PRIORITY(3, x) | \
	 ICC_PPI_PRIORITYR_PRIORITY(4, x) | ICC_PPI_PRIORITYR_PRIORITY(5, x) | \
	 ICC_PPI_PRIORITYR_PRIORITY(6, x) | ICC_PPI_PRIORITYR_PRIORITY(7, x))

/*
 * ICC_PPI_SACTIVER<n>_EL1
 * Interrupt Controller Physical PPI Set Active Register
 */
#define	ICC_PPI_SACTIVER0_EL1		MRS_REG_ALT_NAME(ICC_PPI_SACTIVER0_EL1)
#define	ICC_PPI_SACTIVER0_EL1_op0	3
#define	ICC_PPI_SACTIVER0_EL1_op1	0
#define	ICC_PPI_SACTIVER0_EL1_CRn	12
#define	ICC_PPI_SACTIVER0_EL1_CRm	13
#define	ICC_PPI_SACTIVER0_EL1_op2	2

#define	ICC_PPI_SACTIVER1_EL1		MRS_REG_ALT_NAME(ICC_PPI_SACTIVER1_EL1)
#define	ICC_PPI_SACTIVER1_EL1_op0	3
#define	ICC_PPI_SACTIVER1_EL1_op1	0
#define	ICC_PPI_SACTIVER1_EL1_CRn	12
#define	ICC_PPI_SACTIVER1_EL1_CRm	13
#define	ICC_PPI_SACTIVER1_EL1_op2	3

/*
 * ICC_PPI_SPENDR<n>_EL1
 * Interrupt Controller Physical PPI Set Pending State Registers
 */
#define	ICC_PPI_SPENDR0_EL1		MRS_REG_ALT_NAME(ICC_PPI_SPENDR0_EL1)
#define	ICC_PPI_SPENDR0_EL1_op0		3
#define	ICC_PPI_SPENDR0_EL1_op1		0
#define	ICC_PPI_SPENDR0_EL1_CRn		12
#define	ICC_PPI_SPENDR0_EL1_CRm		13
#define	ICC_PPI_SPENDR0_EL1_op2		6

#define	ICC_PPI_SPENDR1_EL1		MRS_REG_ALT_NAME(ICC_PPI_SPENDR1_EL1)
#define	ICC_PPI_SPENDR1_EL1_op0		3
#define	ICC_PPI_SPENDR1_EL1_op1		0
#define	ICC_PPI_SPENDR1_EL1_CRn		12
#define	ICC_PPI_SPENDR1_EL1_CRm		13
#define	ICC_PPI_SPENDR1_EL1_op2		7

/*
 * Hypervisor control registers
 */

/*
 * ICH_APR_EL2
 * Interrupt Controller Virtual Active Priorities Register
 */
#define	ICH_APR_EL2			MRS_REG_ALT_NAME(ICH_APR_EL2)
#define	ICH_APR_EL2_op0			3
#define	ICH_APR_EL2_op1			4
#define	ICH_APR_EL2_CRn			12
#define	ICH_APR_EL2_CRm			8
#define	ICH_APR_EL2_op2			4
#define	ICH_APR_P(x)			(1ul << (x))

/*
 * ICH_CONTEXTR_EL2
 * Interrupt Control Virtual Context Register
 */
#define	ICH_CONTEXTR_EL2		MRS_REG_ALT_NAME(ICH_CONTEXTR_EL2)
#define	ICH_CONTEXTR_EL2_op0		3
#define	ICH_CONTEXTR_EL2_op1		4
#define	ICH_CONTEXTR_EL2_CRn		12
#define	ICH_CONTEXTR_EL2_CRm		11
#define	ICH_CONTEXTR_EL2_op2		6
#define	ICH_CONTEXTR_EL2_V		(1ul << 63)
#define	ICH_CONTEXTR_EL2_F		(1ul << 62)
#define	ICH_CONTEXTR_EL2_IRICHPPIDIS	(1ul << 61)
#define	ICH_CONTEXTR_EL2_DB		(1ul << 60)
#define	ICH_CONTEXTR_EL2_DBPM_SHIFT	55
#define	ICH_CONTEXTR_EL2_DBPM_MASK	(0x1ful << ICH_CONTEXTR_EL2_DBPM_SHIFT)
#define	ICH_CONTEXTR_EL2_VPE_SHIFT	32
#define	ICH_CONTEXTR_EL2_VPE_MASK	(0xfffful << ICH_CONTEXTR_EL2_VPE_SHIFT)
#define	ICH_CONTEXTR_EL2_VM_SHIFT	0
#define	ICH_CONTEXTR_EL2_VM_MASK	(0xfffful << ICH_CONTEXTR_EL2_VM_SHIFT)


/*
 * ICH_HFGITR_EL2
 * Hypervisor GIC Fine-Grained Read Trap Register
 */
#define	ICH_HFGITR_EL2			MRS_REG_ALT_NAME(ICH_HFGITR_EL2)
#define	ICH_HFGITR_EL2_op0		3
#define	ICH_HFGITR_EL2_op1		4
#define	ICH_HFGITR_EL2_CRn		12
#define	ICH_HFGITR_EL2_CRm		9
#define	ICH_HFGITR_EL2_op2		7
#define	ICH_HFGITR_EL2_GICRCDNMIA	(0x1ul << 10)
#define	ICH_HFGITR_EL2_GICRCDIA		(0x1ul << 9)
#define	ICH_HFGITR_EL2_GICCDDI		(0x1ul << 8)
#define	ICH_HFGITR_EL2_GICCDEOI		(0x1ul << 7)
#define	ICH_HFGITR_EL2_GICCDHM		(0x1ul << 6)
#define	ICH_HFGITR_EL2_GICCDRCFG	(0x1ul << 5)
#define	ICH_HFGITR_EL2_GICCDPEND	(0x1ul << 4)
#define	ICH_HFGITR_EL2_GICCDAFF		(0x1ul << 3)
#define	ICH_HFGITR_EL2_GICCDPRI		(0x1ul << 2)
#define	ICH_HFGITR_EL2_GICCDDIS		(0x1ul << 1)
#define	ICH_HFGITR_EL2_GICCDEN		(0x1ul << 0)

/*
 * ICH_HFGRTR_EL2
 * Hypervisor GIC Fine-Grained Read Trap Register
 */
#define	ICH_HFGRTR_EL2			MRS_REG_ALT_NAME(ICH_HFGRTR_EL2)
#define	ICH_HFGRTR_EL2_op0		3
#define	ICH_HFGRTR_EL2_op1		4
#define	ICH_HFGRTR_EL2_CRn		12
#define	ICH_HFGRTR_EL2_CRm		9
#define	ICH_HFGRTR_EL2_op2		4

/*
 * ICH_HFGWTR_EL2
 * Hypervisor GIC Fine-Grained Write Trap Register
 */
#define	ICH_HFGWTR_EL2			MRS_REG_ALT_NAME(ICH_HFGWTR_EL2)
#define	ICH_HFGWTR_EL2_op0		3
#define	ICH_HFGWTR_EL2_op1		4
#define	ICH_HFGWTR_EL2_CRn		12
#define	ICH_HFGWTR_EL2_CRm		9
#define	ICH_HFGWTR_EL2_op2		6

/*
 * ICH_HPPIR_EL2
 * Hypervisor Highest Priority Pending Interrupt Register
 */
#define	ICH_HPPIR_EL2			MRS_REG_ALT_NAME(ICH_HPPIR_EL2)
#define	ICH_HPPIR_EL2_op0		3
#define	ICH_HPPIR_EL2_op1		4
#define	ICH_HPPIR_EL2_CRn		12
#define	ICH_HPPIR_EL2_CRm		8
#define	ICH_HPPIR_EL2_op2		5
#define	ICH_HPPIR_EL2_HPPIV		(0x1ul << 32)
#define	ICH_HPPIR_EL2_TYPE_SHIFT	29
#define	ICH_HPPIR_EL2_TYPE_MASK		(0x7ul << ICH_HPPIR_EL2_TYPE_SHIFT)
#define	ICH_HPPIR_EL2_ID_SHIFT		0
#define	ICH_HPPIR_EL2_ID_MASK		(0xfffffful << ICH_HPPIR_EL2_ID_SHIFT)

/*
 * ICH_PPI_ACTIVER<n>_EL2
 * Interrupt Controller Virtual Interrupt Active Registers
 */
#define	ICH_PPI_ACTIVER0_EL2		MRS_REG_ALT_NAME(ICH_PPI_ACTIVER0_EL2)
#define	ICH_PPI_ACTIVER0_EL2_op0	3
#define	ICH_PPI_ACTIVER0_EL2_op1	4
#define	ICH_PPI_ACTIVER0_EL2_CRn	12
#define	ICH_PPI_ACTIVER0_EL2_CRm	10
#define	ICH_PPI_ACTIVER0_EL2_op2	6

#define	ICH_PPI_ACTIVER1_EL2		MRS_REG_ALT_NAME(ICH_PPI_ACTIVER1_EL2)
#define	ICH_PPI_ACTIVER1_EL2_op0	3
#define	ICH_PPI_ACTIVER1_EL2_op1	4
#define	ICH_PPI_ACTIVER1_EL2_CRn	12
#define	ICH_PPI_ACTIVER1_EL2_CRm	10
#define	ICH_PPI_ACTIVER1_EL2_op2	7

/*
 * ICH_PPI_DVIR<n>_EL2
 * Interrupt Controller PPI Direct-inject Virtual Interrupt Registers
 */
#define	ICH_PPI_DVIR0_EL2		MRS_REG_ALT_NAME(ICH_PPI_DVIR0_EL2)
#define	ICH_PPI_DVIR0_EL2_op0		3
#define	ICH_PPI_DVIR0_EL2_op1		4
#define	ICH_PPI_DVIR0_EL2_CRn		12
#define	ICH_PPI_DVIR0_EL2_CRm		10
#define	ICH_PPI_DVIR0_EL2_op2		0

#define	ICH_PPI_DVIR1_EL2		MRS_REG_ALT_NAME(ICH_PPI_DVIR1_EL2)
#define	ICH_PPI_DVIR1_EL2_op0		3
#define	ICH_PPI_DVIR1_EL2_op1		4
#define	ICH_PPI_DVIR1_EL2_CRn		12
#define	ICH_PPI_DVIR1_EL2_CRm		10
#define	ICH_PPI_DVIR1_EL2_op2		1

/*
 * ICH_PPI_ENABLER<n>_EL2
 * Interrupt Controller Virtual Interrupt Enable Registers
 */
#define	ICH_PPI_ENABLER0_EL2		MRS_REG_ALT_NAME(ICH_PPI_ENABLER0_EL2)
#define	ICH_PPI_ENABLER0_EL2_op0	3
#define	ICH_PPI_ENABLER0_EL2_op1	4
#define	ICH_PPI_ENABLER0_EL2_CRn	12
#define	ICH_PPI_ENABLER0_EL2_CRm	10
#define	ICH_PPI_ENABLER0_EL2_op2	2

#define	ICH_PPI_ENABLER1_EL2		MRS_REG_ALT_NAME(ICH_PPI_ENABLER1_EL2)
#define	ICH_PPI_ENABLER1_EL2_op0	3
#define	ICH_PPI_ENABLER1_EL2_op1	4
#define	ICH_PPI_ENABLER1_EL2_CRn	12
#define	ICH_PPI_ENABLER1_EL2_CRm	10
#define	ICH_PPI_ENABLER1_EL2_op2	3

/*
 * ICH_PPI_PENDR<.>_EL2
 * Interrupt Controller Virtual Interrupt Pending State Registers
 */
#define	ICH_PPI_PENDR0_EL2		MRS_REG_ALT_NAME(ICH_PPI_PENDR0_EL2)
#define	ICH_PPI_PENDR0_EL2_op0		3
#define	ICH_PPI_PENDR0_EL2_op1		4
#define	ICH_PPI_PENDR0_EL2_CRn		12
#define	ICH_PPI_PENDR0_EL2_CRm		10
#define	ICH_PPI_PENDR0_EL2_op2		4

#define	ICH_PPI_PENDR1_EL2		MRS_REG_ALT_NAME(ICH_PPI_PENDR1_EL2)
#define	ICH_PPI_PENDR1_EL2_op0		3
#define	ICH_PPI_PENDR1_EL2_op1		4
#define	ICH_PPI_PENDR1_EL2_CRn		12
#define	ICH_PPI_PENDR1_EL2_CRm		10
#define	ICH_PPI_PENDR1_EL2_op2		5

/*
 * ICH_PPI_PRIORITYR_EL2
 * Interrupt Controller Virtual Interrupt Priority Registers
 */
#define	ICH_PPI_PRIORITYR0_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 0)
#define	ICH_PPI_PRIORITYR1_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 1)
#define	ICH_PPI_PRIORITYR2_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 2)
#define	ICH_PPI_PRIORITYR3_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 3)
#define	ICH_PPI_PRIORITYR4_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 4)
#define	ICH_PPI_PRIORITYR5_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 5)
#define	ICH_PPI_PRIORITYR6_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 6)
#define	ICH_PPI_PRIORITYR7_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 14, 7)
#define	ICH_PPI_PRIORITYR8_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 0)
#define	ICH_PPI_PRIORITYR9_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 1)
#define	ICH_PPI_PRIORITYR10_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 2)
#define	ICH_PPI_PRIORITYR11_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 3)
#define	ICH_PPI_PRIORITYR12_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 4)
#define	ICH_PPI_PRIORITYR13_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 5)
#define	ICH_PPI_PRIORITYR14_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 6)
#define	ICH_PPI_PRIORITYR15_EL2		__MRS_REG_ALT_NAME(3, 4, 12, 15, 7)

/*
 * ICH_VCTLR_EL2
 * Interrupt Controller Virtual CPU interface Control Register
 */
#define	ICH_VCTLR_EL2			MRS_REG_ALT_NAME(ICH_VCTLR_EL2)
#define	ICH_VCTLR_EL2_op0		3
#define	ICH_VCTLR_EL2_op1		4
#define	ICH_VCTLR_EL2_CRn		12
#define	ICH_VCTLR_EL2_CRm		11
#define	ICH_VCTLR_EL2_op2		4
#define	ICH_VCTLR_V3			(0x1ul << 1)
#define	ICH_VCTLR_EN			(0x1ul << 0)


/*
 * IRS Config Frame registers
 */
#define	IRS_IDR0			0x0000
#define	 IRS_IDR0_VIRT			(0x1u << 6)
#define	 IRS_IDR0_PA_RANGE_SHIFT	2
#define	 IRS_IDR0_PA_RANGE_MASK		(0xfu << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_4G		(0x0u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_64G		(0x1u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_1T		(0x2u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_4T		(0x3u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_16T		(0x4u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_256T		(0x5u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_4P		(0x6u << IRS_IDR0_PA_RANGE_SHIFT)
#define	 IRS_IDR0_PA_RANGE_64P		(0x7u << IRS_IDR0_PA_RANGE_SHIFT)
#define	IRS_IDR1			0x0004
#define	 IRS_IDR0_PE_CNT_SHIFT		0
#define	 IRS_IDR0_PE_CNT_MASK		(0xffffu << IRS_IDR0_PE_CNT_SHIFT)
#define	 IRS_IDR0_IAFFID_BITS_SHIFT	16
#define	 IRS_IDR0_IAFFID_BITS_MASK	(0x1fu << IRS_IDR0_IAFFID_BITS_SHIFT)
#define	 IRS_IDR0_PRI_BITS_SHIFT	20
#define	 IRS_IDR0_PRI_BITS_MASK		(0x7u << IRS_IDR0_PRI_BITS_SHIFT)
#define	IRS_IDR2			0x0008
#define	 IRS_IDR2_ISTMD_SZ_SHIFT	15
#define	 IRS_IDR2_ISTMD_SZ_MASK		(0x1ful << IRS_IDR2_ISTMD_SZ_SHIFT)
#define	 IRS_IDR2_NO_ISTMD		(0x0u << 14)
#define	 IRS_IDR2_ISTMD			(0x1u << 14)
#define	 IRS_IDR2_IST_L2SZ_64K		(0x1u << 13)
#define	 IRS_IDR2_IST_L2SZ_16K		(0x1u << 12)
#define	 IRS_IDR2_IST_L2SZ_4K		(0x1u << 11)
#define	 IRS_IDR2_IST_LEVELS_SINGLE	(0x0u << 10)
#define	 IRS_IDR2_IST_LEVELS_TWO	(0x1u << 10)
#define	 IRS_IDR2_MIN_LPI_ID_BITS_SHIFT	6
#define	 IRS_IDR2_MIN_LPI_ID_BITS_MASK	(0xful << IRS_IDR2_MIN_LPI_ID_BITS_SHIFT)
#define	 IRS_IDR2_MIN_LPI_ID_BITS(x)	\
    (((x) & IRS_IDR2_MIN_LPI_ID_BITS_MASK) >> IRS_IDR2_MIN_LPI_ID_BITS_SHIFT)
#define	 IRS_IDR2_NO_PHYSICAL_LPI	(0x0ul << 5)
#define	 IRS_IDR2_LPI			(0x1ul << 5)
#define	 IRS_IDR2_ID_BITS_SHIFT		0
#define	 IRS_IDR2_ID_BITS_MASK		(0x1ful << IRS_IDR2_ID_BITS_SHIFT)
#define	 IRS_IDR2_ID_BITS(x)		\
    (((x) & IRS_IDR2_ID_BITS_MASK) >> IRS_IDR2_ID_BITS_SHIFT)
#define	IRS_IDR3			0x000c
#define	 IRS_IDR3_VMT_LEVELS		(1u << 10)
#define	 IRS_IDR3_VM_ID_BITS_SHIFT	5
#define	 IRS_IDR3_VM_ID_BITS_MASK	(0x1fu << IRS_IDR3_VM_ID_BITS_SHIFT)
#define	 IRS_IDR3_VMD_SZ_SHIFT		1
#define	 IRS_IDR3_VMD_SZ_MASK		(0xfu << IRS_IDR3_VM_ID_BITS_SHIFT)
#define	 IRS_IDR3_VMD			(0x1u << 0)
#define	IRS_IDR4			0x0010
#define	 IRS_IDR4_VPE_ID_BITS_SHIFT	6
#define	 IRS_IDR4_VPE_ID_BITS_MASK	(0xfu << IRS_IDR4_VPE_ID_BITS_SHIFT)
#define	 IRS_IDR4_VPED_SZ_SHIFT		0
#define	 IRS_IDR4_VPED_SZ_MASK		(0x3fu << IRS_IDR4_VPED_SZ_SHIFT)
#define	IRS_IDR5			0x0014
#define	 IRS_IDR5_SPI_RANGE		0x01ffffff
#define	IRS_IDR6			0x0018
#define	 IRS_IDR6_SPI_IRS_RANGE		0x00ffffff
#define	IRS_IDR7			0x001c
#define	 IRS_IDR7_SPI_BASE		0x00ffffff
#define	IRS_IIDR			0x0040
#define	IRS_AIDR			0x0044
#define	IRS_CR0				0x0080
#define	 IRS_CR0_IDLE			(0x1u << 1)
#define	 IRS_CR0_IRSEN			(0x1u << 0)
#define	IRS_CR1				0x0084
#define	 IRS_CR1_VPED_NO_WA		(0x0u << 15)
#define	 IRS_CR1_VPED_WA		(0x1u << 15)
#define	 IRS_CR1_VPED_NO_RA		(0x0u << 14)
#define	 IRS_CR1_VPED_RA		(0x1u << 14)
#define	 IRS_CR1_VMD_NO_WA		(0x0u << 13)
#define	 IRS_CR1_VMD_WA			(0x1u << 13)
#define	 IRS_CR1_VMD_NO_RA		(0x0u << 12)
#define	 IRS_CR1_VMD_RA			(0x1u << 12)
#define	 IRS_CR1_VPET_NO_WA		(0x0u << 11)
#define	 IRS_CR1_VPET_WA		(0x1u << 11)
#define	 IRS_CR1_VPET_NO_RA		(0x0u << 10)
#define	 IRS_CR1_VPET_RA		(0x1u << 10)
#define	 IRS_CR1_VMT_NO_WA		(0x0u << 9)
#define	 IRS_CR1_VMT_WA			(0x1u << 9)
#define	 IRS_CR1_VMT_NO_RA		(0x0u << 8)
#define	 IRS_CR1_VMT_RA			(0x1u << 8)
#define	 IRS_CR1_IST_NO_WA		(0x0u << 7)
#define	 IRS_CR1_IST_WA			(0x1u << 7)
#define	 IRS_CR1_IST_NO_RA		(0x0u << 6)
#define	 IRS_CR1_IST_RA			(0x1u << 6)
#define	 IRS_CR1_IC_SHIFT		4
#define	 IRS_CR1_IC_MASK		(0x3u << IRS_CR1_IC_SHIFT)
#define	 IRS_CR1_IC_NC			(0x0u << IRS_CR1_IC_SHIFT)
#define	 IRS_CR1_IC_WB			(0x1u << IRS_CR1_IC_SHIFT)
#define	 IRS_CR1_IC_WT			(0x2u << IRS_CR1_IC_SHIFT)
#define	 IRS_CR1_OC_SHIFT		2
#define	 IRS_CR1_OC_MASK		(0x3u << IRS_CR1_OC_SHIFT)
#define	 IRS_CR1_OC_MASK		(0x3u << IRS_CR1_OC_SHIFT)
#define	 IRS_CR1_OC_NC			(0x0u << IRS_CR1_OC_SHIFT)
#define	 IRS_CR1_OC_WB			(0x1u << IRS_CR1_OC_SHIFT)
#define	 IRS_CR1_OC_WT			(0x2u << IRS_CR1_OC_SHIFT)
#define	 IRS_CR1_SH_SHIFT		0
#define	 IRS_CR1_SH_MASK		(0x3u << IRS_CR1_SH_SHIFT)
#define	 IRS_CR1_SH_MASK		(0x3u << IRS_CR1_SH_SHIFT)
#define	 IRS_CR1_SH_NS			(0x0u << IRS_CR1_SH_SHIFT)
#define	 IRS_CR1_SH_OS			(0x2u << IRS_CR1_SH_SHIFT)
#define	 IRS_CR1_SH_IS			(0x3u << IRS_CR1_SH_SHIFT)
#define	IRS_SYNCR			0x00c0
#define	IRS_SYNC_STATUSR		0x00c4
#define	IRS_SPI_VMR			0x0100
#define	IRS_SPI_SELR			0x0108
#define	IRS_SPI_DOMAINR			0x010c
#define	IRS_SPI_RESAMPLER		0x0110
#define	IRS_SPI_CFGR			0x0114
#define	 IRS_SPI_CFGR_TM_EDGE		(0x0u << 0)
#define	 IRS_SPI_CFGR_TM_LEVEL		(0x1u << 0)
#define	IRS_SPI_STATUSR			0x0118
#define	 IRS_SPI_STATUSR_V		(0x1u << 1)
#define	 IRS_SPI_STATUSR_IDLE		(0x1u << 0)
#define	IRS_PE_SELR			0x0140
#define	IRS_PE_STATUSR			0x0144
#define	IRS_PE_CR0			0x0148
#define	IRS_IST_BASER			0x0180
#define	 IRS_IST_BASER_ADDR_LIMIT	0x0100000000000000ul
#define	 IRS_IST_BASER_ADDR_MASK	0x00ffffffffffffc0ul
#define	 IRS_IST_BASER_VALID		(0x1ul << 0)
#define	IRS_IST_CFGR			0x0190
#define	 IRS_IST_CFGR_STRUCTURE_LINEAR	(0x0 << 16)
#define	 IRS_IST_CFGR_STRUCTURE_2LVL	(0x1 << 16)
#define	 IRS_IST_CFGR_ISTSZ_SHIFT	7
#define	 IRS_IST_CFGR_ISTSZ_MASK	(0x3 << IRS_IST_CFGR_ISTSZ_SHIFT)
#define	 IRS_IST_CFGR_ISTSZ_VAL(x)	\
    (((x) & IRS_IST_CFGR_ISTSZ_MASK) >> IRS_IST_CFGR_ISTSZ_SHIFT)
#define	 IRS_IST_CFGR_ISTSZ_4_VAL	0x0
#define	 IRS_IST_CFGR_ISTSZ_4		(0x0 << IRS_IST_CFGR_ISTSZ_SHIFT)
#define	 IRS_IST_CFGR_ISTSZ_8_VAL	0x1
#define	 IRS_IST_CFGR_ISTSZ_8		(0x1 << IRS_IST_CFGR_ISTSZ_SHIFT)
#define	 IRS_IST_CFGR_ISTSZ_16_VAL	0x2
#define	 IRS_IST_CFGR_ISTSZ_16		(0x2 << IRS_IST_CFGR_ISTSZ_SHIFT)
#define	 IRS_IST_CFGR_L2SZ_SHIFT	5
#define	 IRS_IST_CFGR_L2SZ_MASK		(0x3 << IRS_IST_CFGR_L2SZ_SHIFT)
#define	 IRS_IST_CFGR_L2SZ_VAL(x)	\
    (((x) & IRS_IST_CFGR_L2SZ_MASK) >> IRS_IST_CFGR_L2SZ_SHIFT)
#define	 IRS_IST_CFGR_L2SZ_4K_VAL	0x0
#define	 IRS_IST_CFGR_L2SZ_4K		(0x0 << IRS_IST_CFGR_L2SZ_SHIFT)
#define	 IRS_IST_CFGR_L2SZ_16K_VAL	0x1
#define	 IRS_IST_CFGR_L2SZ_16K		(0x1 << IRS_IST_CFGR_L2SZ_SHIFT)
#define	 IRS_IST_CFGR_L2SZ_64K_VAL	0x2
#define	 IRS_IST_CFGR_L2SZ_64K		(0x2 << IRS_IST_CFGR_L2SZ_SHIFT)
#define	 IRS_IST_CFGR_LPI_ID_BITS_SHIFT	0
#define	 IRS_IST_CFGR_LPI_ID_BITS_MASK	(0x1f << IRS_IST_CFGR_LPI_ID_BITS_SHIFT)
#define	 IRS_IST_CFGR_LPI_ID_BITS_VAL(x) \
    (((x) & IRS_IST_CFGR_LPI_ID_BITS_MASK) >> IRS_IST_CFGR_LPI_ID_BITS_SHIFT)
#define	 IRS_IST_CFGR_
#define	IRS_IST_STATUSR			0x0194
#define	 IRS_IST_STATUSR_IDLE		(0x1u << 0)
#define	IRS_MAP_L2_ISTR			0x01c0
#define	IRS_VMT_BASER			0x0200
#define	 IRS_VMT_BASER_ADDR_LIMIT	0x0100000000000000ul
#define	 IRS_VMT_BASER_ADDR_MASK	0x00fffffffffffff8ul
#define	 IRS_VMT_BASER_VALID		(0x1ul << 0)
#define	IRS_VMT_CFGR			0x0210
#define	 IRS_VMT_CFGR_STRUCTURE		(0x1u << 16)
#define	 IRS_VMT_VM_ID_BITS_SHIFT	0
#define	 IRS_VMT_VM_ID_BITS_MASK	(0x1fu << IRS_VMT_VM_ID_BITS_SHIFT)
#define	IRS_VMT_STATUSR			0x0214
#define	IRS_VPE_SELR			0x0240
#define	IRS_VPE_DBR			0x0248
#define	IRS_VPE_HPPIR			0x0250
#define	IRS_VPE_CR0			0x0258
#define	IRS_VPE_STATUSR			0x025c
#define	IRS_VM_DBR			0x0280
#define	IRS_VM_SELR			0x0288
#define	IRS_VM_STATUSR			0x028c
#define	 IRS_VMT_STATUSR_IDLE		(0x1u << 0)
#define	IRS_VMAP_L2_VMTR		0x02c0
#define	IRS_VMAP_VMR			0x02c8
#define	 IRS_VMAP_VMR_M			(0x1ul << 63)
#define	 IRS_VMAP_VMR_U			(0x1ul << 62)
#define	 IRS_VMAP_VMR_VM_ID_MASK	0xfffful
#define	IRS_VMAP_VISTR			0x02d0
#define	IRS_VMAP_L2_VISTR		0x02d8
#define	 IRS_VMAP_L2_VISTR_M		(0x1ul << 63)
#define	IRS_VMAP_VPER			0x02e0
#define	IRS_SAVE_VMR			0x0300
#define	IRS_SAVE_VM_STATUSR		0x0308
#define	IRS_MEC_IDR			0x0340
#define	IRS_MEC_MECID_R			0x0344
#define	IRS_MPAM_IDR			0x0380
#define	IRS_MPAM_PARTID_R		0x0384
#define	IRS_SWERR_STATUSR		0x03c0
#define	IRS_SWERR_SYNDROMER0		0x03c8
#define	IRS_SWERR_SYNDROMER1		0x03d0

/*
 * IRS Data Structures
 */

/* L1_ISTE - Level 1 interrupt state table entry */
#define	L1_ISTE_SIZE			8
#define	L1_ISTE_L2_ADDR			0xfffffffffff000ul
#define	L1_ISTE_VALID			(0x1ul << 0)

/* L2_ISTE - Level 2 interrupt state table entry */
#define	ITSE_MAX_ADDR			0x00ffffffffffffff
#define	ITSE_ALIGN			8

#define	L2_ISTE_SIZE			4
#define	L2_ISTE_IAFFID_SHIFT		16
#define	L2_ISTE_IAFFID_MASK		(0xffffu << L2_ISTE_IAFFID_SHIFT)
#define	L2_ISTE_Priority_SHIFT		11
#define	L2_ISTE_Priority		(0x1fu << L2_ISTE_Priority_SHIFT)
#define	L2_ISTE_HWU_SHIFT		9
#define	L2_ISTE_HWU			(0x3u << L2_ISTE_HWU_SHIFT)
#define	L2_ISTE_IRM			(0x1u << 4)
#define	L2_ISTE_Enable			(0x1u << 3)
#define	L2_ISTE_HM			(0x1u << 2)
#define	L2_ISTE_Active			(0x1u << 1)
#define	L2_ISTE_Pending			(0x1u << 0)

/* log2(number of entries in l2 table */
#define	L2_ISTE_LOG2_ENTRIES(istsz, l2sz) (10 - (istsz) + 2 * (l2sz))
#define	L2_ISTE_LOG2_SIZE(l2sz)		(11 + 2 * (l2sz) + 1)

/* L2_VMTE - Level 2 VM table entry */
#define	L2_VMTE_SIZE			32

#define	L2_VMTE_SPI_ADDR_IDX		3
#define	L2_VMTE_SPI_ID_BITS_SHIFT	59
#define	L2_VMTE_SPI_ID_BITS_MASK	(0x1fu << L2_VMTE_SPI_ID_BITS_SHIFT)
#define	L2_VMTE_SPI_IST_STRUCTURE	(1u << 58)
#define	L2_VMTE_SPI_ISTSZ_SHIFT		56
#define	L2_VMTE_SPI_ISTSZ_MASK		(0x3u << L2_VMTE_SPI_ISTSZ_SHIFT)
#define	L2_VMTE_SPI_IST_ADDR_MASK	0x00ffffffffffffc0ul
#define	L2_VMTE_SPI_IST_L2SZ_SHIFT	1
#define	L2_VMTE_SPI_IST_L2SZ_MASK	(0x3u << L2_VMTE_SPI_IST_L2SZ_SHIFT)
#define	L2_VMTE_SPI_IST_VALID		(0x1u << 0)

#define	L2_VMTE_LPI_ADDR_IDX		2
#define	L2_VMTE_LPI_ID_BITS_SHIFT	59
#define	L2_VMTE_LPI_ID_BITS_MASK	(0x1fu << L2_VMTE_LPI_ID_BITS_SHIFT)
#define	L2_VMTE_LPI_IST_STRUCTURE	(1u << 58)
#define	L2_VMTE_LPI_ISTSZ_SHIFT		56
#define	L2_VMTE_LPI_ISTSZ_MASK		(0x3u << L2_VMTE_LPI_ISTSZ_SHIFT)
#define	L2_VMTE_LPI_IST_ADDR_MASK	0x00ffffffffffffc0ul
#define	L2_VMTE_LPI_IST_L2SZ_SHIFT	1
#define	L2_VMTE_LPI_IST_L2SZ_MASK	(0x3u << L2_VMTE_LPI_IST_L2SZ_SHIFT)
#define	L2_VMTE_LPI_IST_VALID		(0x1u << 0)

#define	L2_VMTE_VPE_ADDR_IDX		1
#define	L2_VMTE_VPE_ID_BITS_SHIFT	59
#define	L2_VMTE_VPE_ID_BITS_MASK	(0x1fu << L2_VMTE_VPE_ID_BITS_SHIFT)
#define	L2_VMTE_VPET_ADDR_MASK		0x00fffffffffffff8ul

#define	L2_VMTE_VMD_ADDR_IDX		0
#define	L2_VMTE_VMD_ADDR_MASK		0x00fffffffffffff8ul
#define	L2_VMTE_VALID			(0x1ul << 0)

/* VPETE */
#define	VPETE_SIZE			8
#define	VPETE_VPED_ADDR_MASK		0x00ffffffffffffe0ul
#define	VPETE_VALID			(0x1ul << 0)

/* VPE_DESC - Virtual PE descriptor table entry */
#define	VPE_DESC_SIZE			32

/*
 * Misc
 */

/*
 * RXBQTT: In the GIC prioritization scheme, lower numbers have higher priority.
 */
#define	GICV5_PRI_HIGHEST		0x0
#define	GICV5_PRI_LOWEST		0x1f

#endif
