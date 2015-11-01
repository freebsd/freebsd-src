/*-
 * Copyright (c) 2013, 2014 Andrew Turner
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Andrew Turner under
 * sponsorship from the FreeBSD Foundation.
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

#ifndef _MACHINE_ARMREG_H_
#define	_MACHINE_ARMREG_H_

#define	READ_SPECIALREG(reg)						\
({	uint64_t val;							\
	__asm __volatile("mrs	%0, " __STRING(reg) : "=&r" (val));	\
	val;								\
})
#define	WRITE_SPECIALREG(reg, val)					\
	__asm __volatile("msr	" __STRING(reg) ", %0" : : "r"((uint64_t)val))

/* CNTHCTL_EL2 - Counter-timer Hypervisor Control register */
#define	CNTHCTL_EVNTI_MASK	(0xf << 4) /* Bit to trigger event stream */
#define	CNTHCTL_EVNTDIR		(1 << 3) /* Control transition trigger bit */
#define	CNTHCTL_EVNTEN		(1 << 2) /* Enable event stream */
#define	CNTHCTL_EL1PCEN		(1 << 1) /* Allow EL0/1 physical timer access */
#define	CNTHCTL_EL1PCTEN	(1 << 0) /*Allow EL0/1 physical counter access*/

/* CPACR_EL1 */
#define	CPACR_FPEN_MASK		(0x3 << 20)
#define	 CPACR_FPEN_TRAP_ALL1	(0x0 << 20) /* Traps from EL0 and EL1 */
#define	 CPACR_FPEN_TRAP_EL0	(0x1 << 20) /* Traps from EL0 */
#define	 CPACR_FPEN_TRAP_ALL2	(0x2 << 20) /* Traps from EL0 and EL1 */
#define	 CPACR_FPEN_TRAP_NONE	(0x3 << 20) /* No traps */
#define	CPACR_TTA		(0x1 << 28)

/* CTR_EL0 - Cache Type Register */
#define	CTR_DLINE_SHIFT		16
#define	CTR_DLINE_MASK		(0xf << CTR_DLINE_SHIFT)
#define	CTR_DLINE_SIZE(reg)	(((reg) & CTR_DLINE_MASK) >> CTR_DLINE_SHIFT)
#define	CTR_ILINE_SHIFT		0
#define	CTR_ILINE_MASK		(0xf << CTR_ILINE_SHIFT)
#define	CTR_ILINE_SIZE(reg)	(((reg) & CTR_ILINE_MASK) >> CTR_ILINE_SHIFT)

/* ESR_ELx */
#define	ESR_ELx_ISS_MASK	0x00ffffff
#define	 ISS_INSN_FnV		(0x01 << 10)
#define	 ISS_INSN_EA		(0x01 << 9)
#define	 ISS_INSN_S1PTW		(0x01 << 7)
#define	 ISS_INSN_IFSC_MASK	(0x1f << 0)
#define	 ISS_DATA_ISV		(0x01 << 24)
#define	 ISS_DATA_SAS_MASK	(0x03 << 22)
#define	 ISS_DATA_SSE		(0x01 << 21)
#define	 ISS_DATA_SRT_MASK	(0x1f << 16)
#define	 ISS_DATA_SF		(0x01 << 15)
#define	 ISS_DATA_AR		(0x01 << 14)
#define	 ISS_DATA_FnV		(0x01 << 10)
#define	 ISS_DATa_EA		(0x01 << 9)
#define	 ISS_DATa_CM		(0x01 << 8)
#define	 ISS_INSN_S1PTW		(0x01 << 7)
#define	 ISS_DATa_WnR		(0x01 << 6)
#define	 ISS_DATA_DFSC_MASK	(0x1f << 0)
#define	ESR_ELx_IL		(0x01 << 25)
#define	ESR_ELx_EC_SHIFT	26
#define	ESR_ELx_EC_MASK		(0x3f << 26)
#define	ESR_ELx_EXCEPTION(esr)	(((esr) & ESR_ELx_EC_MASK) >> ESR_ELx_EC_SHIFT)
#define	 EXCP_UNKNOWN		0x00	/* Unkwn exception */
#define	 EXCP_FP_SIMD		0x07	/* VFP/SIMD trap */
#define	 EXCP_ILL_STATE		0x0e	/* Illegal execution state */
#define	 EXCP_SVC		0x15	/* SVC trap */
#define	 EXCP_MSR		0x18	/* MSR/MRS trap */
#define	 EXCP_INSN_ABORT_L	0x20	/* Instruction abort, from lower EL */
#define	 EXCP_INSN_ABORT	0x21	/* Instruction abort, from same EL */ 
#define	 EXCP_PC_ALIGN		0x22	/* PC alignment fault */
#define	 EXCP_DATA_ABORT_L	0x24	/* Data abort, from lower EL */
#define	 EXCP_DATA_ABORT	0x25	/* Data abort, from same EL */ 
#define	 EXCP_SP_ALIGN		0x26	/* SP slignment fault */
#define	 EXCP_TRAP_FP		0x2c	/* Trapped FP exception */
#define	 EXCP_SERROR		0x2f	/* SError interrupt */
#define	 EXCP_SOFTSTP_EL1	0x33	/* Software Step, from same EL */
#define	 EXCP_WATCHPT_EL1	0x35	/* Watchpoint, from same EL */
#define	 EXCP_BRK		0x3c	/* Breakpoint */

/* ICC_CTLR_EL1 */
#define	ICC_CTLR_EL1_EOIMODE	(1U << 1)

/* ICC_IAR1_EL1 */
#define	ICC_IAR1_EL1_SPUR	(0x03ff)

/* ICC_IGRPEN0_EL1 */
#define	ICC_IGRPEN0_EL1_EN	(1U << 0)

/* ICC_PMR_EL1 */
#define	ICC_PMR_EL1_PRIO_MASK	(0xFFUL)

/* ICC_SRE_EL1 */
#define	ICC_SRE_EL1_SRE		(1U << 0)

/* ICC_SRE_EL2 */
#define	ICC_SRE_EL2_EN		(1U << 3)

/* ID_AA64PFR0_EL1 */
#define	ID_AA64PFR0_EL0_MASK	(0xf << 0)
#define	ID_AA64PFR0_EL1_MASK	(0xf << 4)
#define	ID_AA64PFR0_EL2_MASK	(0xf << 8)
#define	ID_AA64PFR0_EL3_MASK	(0xf << 12)
#define	ID_AA64PFR0_FP_MASK	(0xf << 16)
#define	 ID_AA64PFR0_FP_IMPL	(0x0 << 16) /* Floating-point implemented */
#define	 ID_AA64PFR0_FP_NONE	(0xf << 16) /* Floating-point not implemented */
#define	ID_AA64PFR0_ADV_SIMD_MASK (0xf << 20)
#define	ID_AA64PFR0_GIC_SHIFT	(24)
#define	ID_AA64PFR0_GIC_BITS	(0x4) /* Number of bits in GIC field */
#define	ID_AA64PFR0_GIC_MASK	(0xf << ID_AA64PFR0_GIC_SHIFT)
#define	 ID_AA64PFR0_GIC_CPUIF_EN (0x1 << ID_AA64PFR0_GIC_SHIFT)

/* MAIR_EL1 - Memory Attribute Indirection Register */
#define	MAIR_ATTR_MASK(idx)	(0xff << ((n)* 8))
#define	MAIR_ATTR(attr, idx) ((attr) << ((idx) * 8))

/* SCTLR_EL1 - System Control Register */
#define	SCTLR_RES0	0xc8222400	/* Reserved, write 0 */
#define	SCTLR_RES1	0x30d00800	/* Reserved, write 1 */

#define	SCTLR_M		0x00000001
#define	SCTLR_A		0x00000002
#define	SCTLR_C		0x00000004
#define	SCTLR_SA	0x00000008
#define	SCTLR_SA0	0x00000010
#define	SCTLR_CP15BEN	0x00000020
#define	SCTLR_THEE	0x00000040
#define	SCTLR_ITD	0x00000080
#define	SCTLR_SED	0x00000100
#define	SCTLR_UMA	0x00000200
#define	SCTLR_I		0x00001000
#define	SCTLR_DZE	0x00004000
#define	SCTLR_UCT	0x00008000
#define	SCTLR_nTWI	0x00010000
#define	SCTLR_nTWE	0x00040000
#define	SCTLR_WXN	0x00080000
#define	SCTLR_EOE	0x01000000
#define	SCTLR_EE	0x02000000
#define	SCTLR_UCI	0x04000000

/* SPSR_EL1 */
/*
 * When the exception is taken in AArch64:
 * M[4]   is 0 for AArch64 mode
 * M[3:2] is the exception level
 * M[1]   is unused
 * M[0]   is the SP select:
 *         0: always SP0
 *         1: current ELs SP
 */
#define	PSR_M_EL0t	0x00000000
#define	PSR_M_EL1t	0x00000004
#define	PSR_M_EL1h	0x00000005
#define	PSR_M_EL2t	0x00000008
#define	PSR_M_EL2h	0x00000009
#define	PSR_M_MASK	0x0000001f

#define	PSR_F		0x00000040
#define	PSR_I		0x00000080
#define	PSR_A		0x00000100
#define	PSR_D		0x00000200
#define	PSR_IL		0x00100000
#define	PSR_SS		0x00200000
#define	PSR_V		0x10000000
#define	PSR_C		0x20000000
#define	PSR_Z		0x40000000
#define	PSR_N		0x80000000

/* TCR_EL1 - Translation Control Register */
#define	TCR_ASID_16	(1 << 36)

#define	TCR_IPS_SHIFT	32
#define	TCR_IPS_32BIT	(0 << TCR_IPS_SHIFT)
#define	TCR_IPS_36BIT	(1 << TCR_IPS_SHIFT)
#define	TCR_IPS_40BIT	(2 << TCR_IPS_SHIFT)
#define	TCR_IPS_42BIT	(3 << TCR_IPS_SHIFT)
#define	TCR_IPS_44BIT	(4 << TCR_IPS_SHIFT)
#define	TCR_IPS_48BIT	(5 << TCR_IPS_SHIFT)

#define	TCR_TG1_SHIFT	30
#define	TCR_TG1_16K	(1 << TCR_TG1_SHIFT)
#define	TCR_TG1_4K	(2 << TCR_TG1_SHIFT)
#define	TCR_TG1_64K	(3 << TCR_TG1_SHIFT)

#define	TCR_SH1_SHIFT	28
#define	TCR_SH1_IS	(0x3UL << TCR_SH1_SHIFT)
#define	TCR_ORGN1_SHIFT	26
#define	TCR_ORGN1_WBWA	(0x1UL << TCR_ORGN1_SHIFT)
#define	TCR_IRGN1_SHIFT	24
#define	TCR_IRGN1_WBWA	(0x1UL << TCR_IRGN1_SHIFT)
#define	TCR_SH0_SHIFT	12
#define	TCR_SH0_IS	(0x3UL << TCR_SH0_SHIFT)
#define	TCR_ORGN0_SHIFT	10
#define	TCR_ORGN0_WBWA	(0x1UL << TCR_ORGN0_SHIFT)
#define	TCR_IRGN0_SHIFT	8
#define	TCR_IRGN0_WBWA	(0x1UL << TCR_IRGN0_SHIFT)

#define	TCR_CACHE_ATTRS	((TCR_IRGN0_WBWA | TCR_IRGN1_WBWA) |\
				(TCR_ORGN0_WBWA | TCR_ORGN1_WBWA))

#ifdef SMP
#define	TCR_SMP_ATTRS	(TCR_SH0_IS | TCR_SH1_IS)
#else
#define	TCR_SMP_ATTRS	0
#endif

#define	TCR_T1SZ_SHIFT	16
#define	TCR_T0SZ_SHIFT	0
#define	TCR_T1SZ(x)	((x) << TCR_T1SZ_SHIFT)
#define	TCR_T0SZ(x)	((x) << TCR_T0SZ_SHIFT)
#define	TCR_TxSZ(x)	(TCR_T1SZ(x) | TCR_T0SZ(x))

/* Saved Program Status Register */
#define	DBG_SPSR_SS	(0x1 << 21)

/* Monitor Debug System Control Register */
#define	DBG_MDSCR_SS	(0x1 << 0)
#define	DBG_MDSCR_KDE	(0x1 << 13)
#define	DBG_MDSCR_MDE	(0x1 << 15)

/* Perfomance Monitoring Counters */
#define	PMCR_E		(1 << 0) /* Enable all counters */
#define	PMCR_P		(1 << 1) /* Reset all counters */
#define	PMCR_C		(1 << 2) /* Clock counter reset */
#define	PMCR_D		(1 << 3) /* CNTR counts every 64 clk cycles */
#define	PMCR_X		(1 << 4) /* Export to ext. monitoring (ETM) */
#define	PMCR_DP		(1 << 5) /* Disable CCNT if non-invasive debug*/
#define	PMCR_LC		(1 << 6) /* Long cycle count enable */
#define	PMCR_IMP_SHIFT	24 /* Implementer code */
#define	PMCR_IMP_MASK	(0xff << PMCR_IMP_SHIFT)
#define	PMCR_IDCODE_SHIFT	16 /* Identification code */
#define	PMCR_IDCODE_MASK	(0xff << PMCR_IDCODE_SHIFT)
#define	 PMCR_IDCODE_CORTEX_A57	0x01
#define	 PMCR_IDCODE_CORTEX_A72	0x02
#define	 PMCR_IDCODE_CORTEX_A53	0x03
#define	PMCR_N_SHIFT	11       /* Number of counters implemented */
#define	PMCR_N_MASK	(0x1f << PMCR_N_SHIFT)

#endif /* !_MACHINE_ARMREG_H_ */
