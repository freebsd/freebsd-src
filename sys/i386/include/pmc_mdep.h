/*-
 * Copyright (c) 2003, Joseph Koshy
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

/* Machine dependent interfaces */

#ifndef _MACHINE_PMC_MDEP_H
#define	_MACHINE_PMC_MDEP_H 1

#include <machine/cpufunc.h>
#include <machine/specialreg.h>

/* AMD K7 PMCs */

#define	K7_NPMCS		5 /* 1 TSC + 4 PMCs */

#define	K7_PMC_COUNTERMASK	0xFF000000
#define	K7_PMC_TO_COUNTER(x)	(((x) << 24) & K7_PMC_COUNTERMASK)
#define	K7_PMC_INVERT		(1 << 23)
#define	K7_PMC_ENABLE		(1 << 22)
#define	K7_PMC_INT		(1 << 20)
#define	K7_PMC_PC		(1 << 19)
#define	K7_PMC_EDGE		(1 << 18)
#define	K7_PMC_OS		(1 << 17)
#define	K7_PMC_USR		(1 << 16)

#define	K7_PMC_UNITMASK_M	0x10
#define	K7_PMC_UNITMASK_O	0x08
#define	K7_PMC_UNITMASK_E	0x04
#define	K7_PMC_UNITMASK_S	0x02
#define	K7_PMC_UNITMASK_I	0x01
#define	K7_PMC_UNITMASK_MOESI	0x1F

#define	K7_PMC_UNITMASK		0xFF00
#define	K7_PMC_EVENTMASK 	0x00FF
#define	K7_PMC_TO_UNITMASK(x)	(((x) << 8) & K7_PMC_UNITMASK)
#define	K7_PMC_TO_EVENTMASK(x)	((x) & 0xFF)
#define	K7_VALID_BITS		(K7_PMC_COUNTERMASK | K7_PMC_INVERT |      \
	K7_PMC_ENABLE | K7_PMC_INT | K7_PMC_PC | K7_PMC_EDGE | K7_PMC_OS | \
	K7_PMC_USR | K7_PMC_UNITMASK | K7_PMC_EVENTMASK)

/* Intel P4 PMCs */

#define	P4_NPMCS		19	/* 1 TSC + 18 PMCS */
#define	P4_NESCR		45
#define	P4_INVALID_PMC_INDEX	-1
#define	P4_MAX_ESCR_PER_EVENT	2
#define	P4_MAX_PMC_PER_ESCR	3

#define	P4_CCCR_OVF			(1 << 31)
#define	P4_CCCR_CASCADE			(1 << 30)
#define	P4_CCCR_OVF_PMI_T1		(1 << 27)
#define	P4_CCCR_OVF_PMI_T0		(1 << 26)
#define	P4_CCCR_FORCE_OVF		(1 << 25)
#define	P4_CCCR_EDGE			(1 << 24)
#define	P4_CCCR_THRESHOLD_SHIFT		20
#define	P4_CCCR_THRESHOLD_MASK		0x00F00000
#define	P4_CCCR_TO_THRESHOLD(C)		(((C) << P4_CCCR_THRESHOLD_SHIFT) & \
	P4_CCCR_THRESHOLD_MASK)
#define	P4_CCCR_COMPLEMENT		(1 << 19)
#define	P4_CCCR_COMPARE			(1 << 18)
#define	P4_CCCR_ACTIVE_THREAD_SHIFT	16
#define	P4_CCCR_ACTIVE_THREAD_MASK	0x00030000
#define	P4_CCCR_TO_ACTIVE_THREAD(T)	(((T) << P4_CCCR_ACTIVE_THREAD_SHIFT) & \
	P4_CCCR_ACTIVE_THREAD_MASK)
#define	P4_CCCR_ESCR_SELECT_SHIFT	13
#define	P4_CCCR_ESCR_SELECT_MASK	0x0000E000
#define	P4_CCCR_TO_ESCR_SELECT(E)	(((E) << P4_CCCR_ESCR_SELECT_SHIFT) & \
	P4_CCCR_ESCR_SELECT_MASK)
#define	P4_CCCR_ENABLE			(1 << 12)
#define	P4_CCCR_VALID_BITS		(P4_CCCR_OVF | P4_CCCR_CASCADE | \
    P4_CCCR_OVF_PMI_T1 | P4_CCCR_OVF_PMI_T0 | P4_CCCR_FORCE_OVF | 	 \
    P4_CCCR_EDGE | P4_CCCR_THRESHOLD_MASK | P4_CCCR_COMPLEMENT |	 \
    P4_CCCR_COMPARE | P4_CCCR_ESCR_SELECT_MASK | P4_CCCR_ENABLE)

#define	P4_ESCR_EVENT_SELECT_SHIFT	25
#define	P4_ESCR_EVENT_SELECT_MASK	0x7E000000
#define	P4_ESCR_TO_EVENT_SELECT(E)	(((E) << P4_ESCR_EVENT_SELECT_SHIFT) & \
	P4_ESCR_EVENT_SELECT_MASK)
#define	P4_ESCR_EVENT_MASK_SHIFT	9
#define	P4_ESCR_EVENT_MASK_MASK		0x01FFFE00
#define	P4_ESCR_TO_EVENT_MASK(M)	(((M) << P4_ESCR_EVENT_MASK_SHIFT) & \
	P4_ESCR_EVENT_MASK_MASK)
#define	P4_ESCR_TAG_VALUE_SHIFT		5
#define	P4_ESCR_TAG_VALUE_MASK		0x000001E0
#define	P4_ESCR_TO_TAG_VALUE(T)		(((T) << P4_ESCR_TAG_VALUE_SHIFT) & \
	P4_ESCR_TAG_VALUE_MASK)
#define	P4_ESCR_TAG_ENABLE 		0x00000010
#define	P4_ESCR_T0_OS			0x00000008
#define	P4_ESCR_T0_USR			0x00000004
#define	P4_ESCR_T1_OS			0x00000002
#define	P4_ESCR_T1_USR			0x00000001
#define	P4_ESCR_OS			P4_ESCR_T0_OS
#define	P4_ESCR_USR			P4_ESCR_T0_USR
#define	P4_ESCR_VALID_BITS		(P4_ESCR_EVENT_SELECT_MASK |	\
    P4_ESCR_EVENT_MASK_MASK | P4_ESCR_TAG_VALUE_MASK | 			\
    P4_ESCR_TAG_ENABLE | P4_ESCR_T0_OS | P4_ESCR_T0_USR | P4_ESCR_T1_OS \
    P4_ESCR_T1_USR)

#define	P4_PERFCTR_MASK			0xFFFFFFFFFFLL /* 40 bits */

#define	P4_CCCR_MSR_FIRST		0x360 /* MSR_BPU_CCCR0 */
#define	P4_PERFCTR_MSR_FIRST		0x300 /* MSR_BPU_COUNTER0 */

/* Intel PPro, Celeron, P-II, P-III, Pentium-M PMCS */

#define	P6_NPMCS	3		/* 1 TSC + 2 PMCs */

#define	P6_EVSEL_CMASK_MASK		0xFF000000
#define	P6_EVSEL_TO_CMASK(C)		(((C) & 0xFF) << 24)
#define	P6_EVSEL_INV			(1 << 23)
#define	P6_EVSEL_EN			(1 << 22)
#define	P6_EVSEL_INT			(1 << 20)
#define	P6_EVSEL_PC			(1 << 19)
#define	P6_EVSEL_E			(1 << 18)
#define	P6_EVSEL_OS			(1 << 17)
#define	P6_EVSEL_USR			(1 << 16)
#define	P6_EVSEL_UMASK_MASK		0x0000FF00
#define	P6_EVSEL_TO_UMASK(U)		(((U) & 0xFF) << 8)
#define	P6_EVSEL_EVENT_SELECT(ES)	((ES) & 0xFF)
#define	P6_EVSEL_RESERVED		(1 << 21)

#define	P6_MSR_EVSEL0			0x0186
#define	P6_MSR_EVSEL1			0x0187
#define	P6_MSR_PERFCTR0			0x00C1
#define	P6_MSR_PERFCTR1			0x00C2

#define	P6_PERFCTR_MASK			0xFFFFFFFFFFLL /* 40 bits */

/* Intel Pentium PMCs */

#define	PENTIUM_NPMCS	3		/* 1 TSC + 2 PMCs */
#define	PENTIUM_CESR_PC1		(1 << 25)
#define	PENTIUM_CESR_CC1_MASK		0x01C00000
#define	PENTIUM_CESR_TO_CC1(C)		(((C) & 0x07) << 22)
#define	PENTIUM_CESR_ES1_MASK		0x003F0000
#define	PENTIUM_CESR_TO_ES1(E)		(((E) & 0x3F) << 16)
#define	PENTIUM_CESR_PC0		(1 << 9)
#define	PENTIUM_CESR_CC0_MASK		0x000001C0
#define	PENTIUM_CESR_TO_CC0(C)		(((C) & 0x07) << 6)
#define	PENTIUM_CESR_ES0_MASK		0x0000003F
#define	PENTIUM_CESR_TO_ES0(E)		((E) & 0x3F)
#define	PENTIUM_CESR_RESERVED		0xFC00FC00

#define	PENTIUM_MSR_CESR		0x11
#define	PENTIUM_MSR_CTR0		0x12
#define	PENTIUM_MSR_CTR1		0x13

#ifdef _KERNEL

/*
 * Prototypes
 */

#if defined(__i386__)
struct pmc_mdep *pmc_amd_initialize(void);		/* AMD K7/K8 PMCs */
struct pmc_mdep *pmc_intel_initialize(void);		/* Intel PMCs */
int	pmc_initialize_p4(struct pmc_mdep *);		/* Pentium IV PMCs */
int	pmc_initialize_p5(struct pmc_mdep *);		/* Pentium PMCs */
int	pmc_initialize_p6(struct pmc_mdep *);		/* Pentium Pro PMCs */
#endif /* defined(__i386__) */

#endif /* _KERNEL */
#endif /* _MACHINE_PMC_MDEP_H */
