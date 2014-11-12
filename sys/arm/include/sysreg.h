/*-
 * Copyright 2014 Svatopluk Kraus <onwahe@gmail.com>
 * Copyright 2014 Michal Meloun <meloun@miracle.cz>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

/*
 * Macros to make working with the System Control Registers simpler.
 */

#ifndef MACHINE_SYSREG_H
#define	MACHINE_SYSREG_H

#include <machine/acle-compat.h>

/*
 * CP15 C0 registers
 */
#define	CP15_MIDR(rr)		p15, 0, rr, c0, c0,  0 /* Main ID Register */
#define	CP15_CTR(rr)		p15, 0, rr, c0, c0,  1 /* Cache Type Register */
#define	CP15_TCMTR(rr)		p15, 0, rr, c0, c0,  2 /* TCM Type Register */
#define	CP15_TLBTR(rr)		p15, 0, rr, c0, c0,  3 /* TLB Type Register */
#define	CP15_MPIDR(rr)		p15, 0, rr, c0, c0,  5 /* Multiprocessor Affinity Register */
#define	CP15_REVIDR(rr)		p15, 0, rr, c0, c0,  6 /* Revision ID Register */

#define	CP15_ID_PFR0(rr)	p15, 0, rr, c0, c1,  0 /* Processor Feature Register 0 */
#define	CP15_ID_PFR1(rr)	p15, 0, rr, c0, c1,  1 /* Processor Feature Register 1 */
#define	CP15_ID_DFR0(rr)	p15, 0, rr, c0, c1,  2 /* Debug Feature Register 0 */
#define	CP15_ID_AFR0(rr)	p15, 0, rr, c0, c1,  3 /* Auxiliary Feature Register  0 */
#define	CP15_ID_MMFR0(rr)	p15, 0, rr, c0, c1,  4 /* Memory Model Feature Register 0 */
#define	CP15_ID_MMFR1(rr)	p15, 0, rr, c0, c1,  5 /* Memory Model Feature Register 1 */
#define	CP15_ID_MMFR2(rr)	p15, 0, rr, c0, c1,  6 /* Memory Model Feature Register 2 */
#define	CP15_ID_MMFR3(rr)	p15, 0, rr, c0, c1,  7 /* Memory Model Feature Register 3 */

#define	CP15_ID_ISAR0(rr)	p15, 0, rr, c0, c2,  0 /* Instruction Set Attribute Register 0 */
#define	CP15_ID_ISAR1(rr)	p15, 0, rr, c0, c2,  1 /* Instruction Set Attribute Register 1 */
#define	CP15_ID_ISAR2(rr)	p15, 0, rr, c0, c2,  2 /* Instruction Set Attribute Register 2 */
#define	CP15_ID_ISAR3(rr)	p15, 0, rr, c0, c2,  3 /* Instruction Set Attribute Register 3 */
#define	CP15_ID_ISAR4(rr)	p15, 0, rr, c0, c2,  4 /* Instruction Set Attribute Register 4 */
#define	CP15_ID_ISAR5(rr)	p15, 0, rr, c0, c2,  5 /* Instruction Set Attribute Register 5 */

#define	CP15_CCSIDR(rr)		p15, 1, rr, c0, c0,  0 /* Cache Size ID Registers */
#define	CP15_CLIDR(rr)		p15, 1, rr, c0, c0,  1 /* Cache Level ID Register */
#define	CP15_AIDR(rr)		p15, 1, rr, c0, c0,  7 /* Auxiliary ID Register */

#define	CP15_CSSELR(rr)		p15, 2, rr, c0, c0,  0 /* Cache Size Selection Register */

/*
 * CP15 C1 registers
 */
#define	CP15_SCTLR(rr)		p15, 0, rr, c1, c0,  0 /* System Control Register */
#define	CP15_ACTLR(rr)		p15, 0, rr, c1, c0,  1 /* IMPLEMENTATION DEFINED Auxiliary Control Register */
#define	CP15_CPACR(rr)		p15, 0, rr, c1, c0,  2 /* Coprocessor Access Control Register */

#define	CP15_SCR(rr)		p15, 0, rr, c1, c1,  0 /* Secure Configuration Register */
#define	CP15_SDER(rr)		p15, 0, rr, c1, c1,  1 /* Secure Debug Enable Register */
#define	CP15_NSACR(rr)		p15, 0, rr, c1, c1,  2 /* Non-Secure Access Control Register */

/*
 * CP15 C2 registers
 */
#define	CP15_TTBR0(rr)		p15, 0, rr, c2, c0,  0 /* Translation Table Base Register 0 */
#define	CP15_TTBR1(rr)		p15, 0, rr, c2, c0,  1 /* Translation Table Base Register 1 */
#define	CP15_TTBCR(rr)		p15, 0, rr, c2, c0,  2 /* Translation Table Base Control Register */

/*
 * CP15 C3 registers
 */
#define	CP15_DACR(rr)		p15, 0, rr, c3, c0,  0 /* Domain Access Control Register */

/*
 * CP15 C5 registers
 */
#define	CP15_DFSR(rr)		p15, 0, rr, c5, c0,  0 /* Data Fault Status Register */

#if __ARM_ARCH >= 6
/* From ARMv6: */
#define	CP15_IFSR(rr)		p15, 0, rr, c5, c0,  1 /* Instruction Fault Status Register */
/* From ARMv7: */
#define	CP15_ADFSR(rr)		p15, 0, rr, c5, c1,  0 /* Auxiliary Data Fault Status Register */
#define	CP15_AIFSR(rr)		p15, 0, rr, c5, c1,  1 /* Auxiliary Instruction Fault Status Register */
#endif


/*
 * CP15 C6 registers
 */
#define	CP15_DFAR(rr)		p15, 0, rr, c6, c0,  0 /* Data Fault Address Register */

#if __ARM_ARCH >= 6
/* From ARMv6k: */
#define	CP15_IFAR(rr)		p15, 0, rr, c6, c0,  2 /* Instruction Fault Address Register */
#endif

/*
 * CP15 C7 registers
 */
#if __ARM_ARCH >= 6
/* From ARMv7: */
#define	CP15_ICIALLUIS		p15, 0, r0, c7, c1,  0 /* Instruction cache invalidate all PoU, IS */
#define	CP15_BPIALLIS		p15, 0, r0, c7, c1,  6 /* Branch predictor invalidate all IS */
#endif

#define	CP15_PAR		p15, 0, r0, c7, c4,  0 /* Physical Address Register */

#define	CP15_ICIALLU		p15, 0, r0, c7, c5,  0 /* Instruction cache invalidate all PoU */
#define	CP15_ICIMVAU(rr)	p15, 0, rr, c7, c5,  1 /* Instruction cache invalidate */
#if __ARM_ARCH >= 6
/* Deprecated in ARMv7 */
#define	CP15_CP15ISB		p15, 0, r0, c7, c5,  4 /* ISB */
#endif
#define	CP15_BPIALL		p15, 0, r0, c7, c5,  6 /* Branch predictor invalidate all */
#define	CP15_BPIMVA		p15, 0, rr, c7, c5,  7 /* Branch predictor invalidate by MVA */

#if __ARM_ARCH >= 6
/* Only ARMv6: */
#define	CP15_DCIALL		p15, 0, r0, c7, c6,  0 /* Data cache invalidate all */
#endif
#define	CP15_DCIMVAC(rr)	p15, 0, rr, c7, c6,  1 /* Data cache invalidate by MVA PoC */
#define	CP15_DCISW(rr)		p15, 0, rr, c7, c6,  2 /* Data cache invalidate by set/way */

#define	CP15_ATS1CPR(rr)	p15, 0, rr, c7, c8,  0 /* Stage 1 Current state PL1 read */
#define	CP15_ATS1CPW(rr)	p15, 0, rr, c7, c8,  1 /* Stage 1 Current state PL1 write */
#define	CP15_ATS1CUR(rr)	p15, 0, rr, c7, c8,  2 /* Stage 1 Current state unprivileged read */
#define	CP15_ATS1CUW(rr)	p15, 0, rr, c7, c8,  3 /* Stage 1 Current state unprivileged write */

#if __ARM_ARCH >= 6
/* From ARMv7: */
#define	CP15_ATS12NSOPR(rr)	p15, 0, rr, c7, c8,  4 /* Stages 1 and 2 Non-secure only PL1 read */
#define	CP15_ATS12NSOPW(rr)	p15, 0, rr, c7, c8,  5 /* Stages 1 and 2 Non-secure only PL1 write */
#define	CP15_ATS12NSOUR(rr)	p15, 0, rr, c7, c8,  6 /* Stages 1 and 2 Non-secure only unprivileged read */
#define	CP15_ATS12NSOUW(rr)	p15, 0, rr, c7, c8,  7 /* Stages 1 and 2 Non-secure only unprivileged write */
#endif

#if __ARM_ARCH >= 6
/* Only ARMv6: */
#define	CP15_DCCALL		p15, 0, r0, c7, c10, 0 /* Data cache clean all */
#endif
#define	CP15_DCCMVAC(rr)	p15, 0, rr, c7, c10, 1 /* Data cache clean by MVA PoC */
#define	CP15_DCCSW(rr)		p15, 0, rr, c7, c10, 2 /* Data cache clean by set/way */
#if __ARM_ARCH >= 6
/* Only ARMv6: */
#define	CP15_CP15DSB		p15, 0, r0, c7, c10, 4 /* DSB */
#define	CP15_CP15DMB		p15, 0, r0, c7, c10, 5 /* DMB */
#endif

#if __ARM_ARCH >= 6
/* From ARMv7: */
#define	CP15_DCCMVAU(rr)	p15, 0, rr, c7, c11, 1 /* Data cache clean by MVA PoU */
#endif

#if __ARM_ARCH >= 6
/* Only ARMv6: */
#define	CP15_DCCIALL		p15, 0, r0, c7, c14, 0 /* Data cache clean and invalidate all */
#endif
#define	CP15_DCCIMVAC(rr)	p15, 0, rr, c7, c14, 1 /* Data cache clean and invalidate by MVA PoC */
#define	CP15_DCCISW(rr)		p15, 0, rr, c7, c14, 2 /* Data cache clean and invalidate by set/way */

/*
 * CP15 C8 registers
 */
#if __ARM_ARCH >= 6
/* From ARMv7: */
#define	CP15_TLBIALLIS		p15, 0, r0, c8, c3, 0 /* Invalidate entire unified TLB IS */
#define	CP15_TLBIMVAIS(rr)	p15, 0, rr, c8, c3, 1 /* Invalidate unified TLB by MVA IS */
#define	CP15_TLBIASIDIS(rr)	p15, 0, rr, c8, c3, 2 /* Invalidate unified TLB by ASID IS */
#define	CP15_TLBIMVAAIS(rr)	p15, 0, rr, c8, c3, 3 /* Invalidate unified TLB by MVA, all ASID IS */
#endif

#define	CP15_TLBIALL		p15, 0, r0, c8, c7, 0 /* Invalidate entire unified TLB */
#define	CP15_TLBIMVA(rr)	p15, 0, rr, c8, c7, 1 /* Invalidate unified TLB by MVA */
#define	CP15_TLBIASID(rr)	p15, 0, rr, c8, c7, 2 /* Invalidate unified TLB by ASID */

#if __ARM_ARCH >= 6
/* From ARMv6: */
#define	CP15_TLBIMVAA(rr)	p15, 0, rr, c8, c7, 3 /* Invalidate unified TLB by MVA, all ASID */
#endif

/*
 * CP15 C10 registers
 */
/* Without LPAE this is PRRR, with LPAE it's MAIR0 */
#define	CP15_PRRR(rr)		p15, 0, rr, c10, c2, 0 /* Primary Region Remap Register */
#define	CP15_MAIR0(rr)		p15, 0, rr, c10, c2, 0 /* Memory Attribute Indirection Register 0 */
/* Without LPAE this is NMRR, with LPAE it's MAIR1 */
#define	CP15_NMRR(rr)		p15, 0, rr, c10, c2, 1 /* Normal Memory Remap Register */
#define	CP15_MAIR1(rr)		p15, 0, rr, c10, c2, 1 /* Memory Attribute Indirection Register 1 */

#define	CP15_AMAIR0(rr)		p15, 0, rr, c10, c3, 0 /* Auxiliary Memory Attribute Indirection Register 0 */
#define	CP15_AMAIR1(rr)		p15, 0, rr, c10, c3, 1 /* Auxiliary Memory Attribute Indirection Register 1 */

/*
 * CP15 C12 registers
 */
#define	CP15_VBAR(rr)		p15, 0, rr, c12, c0, 0 /* Vector Base Address Register */
#define	CP15_MVBAR(rr)		p15, 0, rr, c12, c0, 1 /* Monitor Vector Base Address Register */

#define	CP15_ISR(rr)		p15, 0, rr, c12, c1, 0 /* Interrupt Status Register */

/*
 * CP15 C13 registers
 */
#define	CP15_FCSEIDR(rr)	p15, 0, rr, c13, c0, 0 /* FCSE Process ID Register */
#define	CP15_CONTEXTIDR(rr)	p15, 0, rr, c13, c0, 1 /* Context ID Register */
#define	CP15_TPIDRURW(rr)	p15, 0, rr, c13, c0, 2 /* User Read/Write Thread ID Register */
#define	CP15_TPIDRURO(rr)	p15, 0, rr, c13, c0, 3 /* User Read-Only Thread ID Register */
#define	CP15_TPIDRPRW(rr)	p15, 0, rr, c13, c0, 4 /* PL1 only Thread ID Register */

#endif /* !MACHINE_SYSREG_H */
