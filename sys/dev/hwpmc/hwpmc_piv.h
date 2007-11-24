/*-
 * Copyright (c) 2005, Joseph Koshy
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

#ifndef _DEV_HWPMC_PIV_H_
#define	_DEV_HWPMC_PIV_H_ 1

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
#define	P4_PERFCTR_OVERFLOWED(PMC)	((rdpmc(PMC) & (1LL << 39)) == 0)

#define	P4_CCCR_MSR_FIRST		0x360 /* MSR_BPU_CCCR0 */
#define	P4_PERFCTR_MSR_FIRST		0x300 /* MSR_BPU_COUNTER0 */

#define	P4_RELOAD_COUNT_TO_PERFCTR_VALUE(V)	(1 - (V))
#define	P4_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	(1 - (P))

struct pmc_md_p4_op_pmcallocate {
	uint32_t	pm_p4_cccrconfig;
	uint32_t	pm_p4_escrconfig;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_p4_pmc {
	uint32_t	pm_p4_cccrvalue;
	uint32_t	pm_p4_escrvalue;
	uint32_t	pm_p4_escr;
	uint32_t	pm_p4_escrmsr;
};


/*
 * Prototypes
 */

int	pmc_initialize_p4(struct pmc_mdep *);		/* Pentium IV PMCs */

#endif /* _KERNEL */
#endif /* _MACHINE_PMC_MDEP_H */
