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
 * $FreeBSD: src/sys/dev/hwpmc/hwpmc_amd.h,v 1.1.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_AMD_H_
#define	_DEV_HWPMC_AMD_H_ 1

/* AMD K7 and K8 PMCs */

#define	AMD_PMC_EVSEL_0		0xC0010000
#define	AMD_PMC_EVSEL_1		0xC0010001
#define	AMD_PMC_EVSEL_2		0xC0010002
#define	AMD_PMC_EVSEL_3		0xC0010003

#define	AMD_PMC_PERFCTR_0	0xC0010004
#define	AMD_PMC_PERFCTR_1	0xC0010005
#define	AMD_PMC_PERFCTR_2	0xC0010006
#define	AMD_PMC_PERFCTR_3	0xC0010007


#define	AMD_NPMCS		5 /* 1 TSC + 4 PMCs */

#define	AMD_PMC_COUNTERMASK	0xFF000000
#define	AMD_PMC_TO_COUNTER(x)	(((x) << 24) & AMD_PMC_COUNTERMASK)
#define	AMD_PMC_INVERT		(1 << 23)
#define	AMD_PMC_ENABLE		(1 << 22)
#define	AMD_PMC_INT		(1 << 20)
#define	AMD_PMC_PC		(1 << 19)
#define	AMD_PMC_EDGE		(1 << 18)
#define	AMD_PMC_OS		(1 << 17)
#define	AMD_PMC_USR		(1 << 16)

#define	AMD_PMC_UNITMASK_M	0x10
#define	AMD_PMC_UNITMASK_O	0x08
#define	AMD_PMC_UNITMASK_E	0x04
#define	AMD_PMC_UNITMASK_S	0x02
#define	AMD_PMC_UNITMASK_I	0x01
#define	AMD_PMC_UNITMASK_MOESI	0x1F

#define	AMD_PMC_UNITMASK	0xFF00
#define	AMD_PMC_EVENTMASK 	0x00FF

#define	AMD_PMC_TO_UNITMASK(x)	(((x) << 8) & AMD_PMC_UNITMASK)
#define	AMD_PMC_TO_EVENTMASK(x)	((x) & 0xFF)
#define	AMD_VALID_BITS		(AMD_PMC_COUNTERMASK | AMD_PMC_INVERT |	\
	AMD_PMC_ENABLE | AMD_PMC_INT | AMD_PMC_PC | AMD_PMC_EDGE | 	\
	AMD_PMC_OS | AMD_PMC_USR | AMD_PMC_UNITMASK | AMD_PMC_EVENTMASK)

#define AMD_PMC_CAPS		(PMC_CAP_INTERRUPT | PMC_CAP_USER | 	\
	PMC_CAP_SYSTEM | PMC_CAP_EDGE | PMC_CAP_THRESHOLD | 		\
	PMC_CAP_READ | PMC_CAP_WRITE | PMC_CAP_INVERT | PMC_CAP_QUALIFIER)

#define AMD_PMC_IS_STOPPED(evsel) ((rdmsr((evsel)) & AMD_PMC_ENABLE) == 0)
#define AMD_PMC_HAS_OVERFLOWED(pmc) ((rdpmc(pmc) & (1ULL << 47)) == 0)

#define	AMD_RELOAD_COUNT_TO_PERFCTR_VALUE(V)	(-(V))
#define	AMD_PERFCTR_VALUE_TO_RELOAD_COUNT(P)	(-(P))

struct pmc_md_amd_op_pmcallocate {
	uint32_t	pm_amd_config;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_amd_pmc {
	uint32_t	pm_amd_evsel;
};

/*
 * Prototypes
 */

struct pmc_mdep *pmc_amd_initialize(void);		/* AMD K7/K8 PMCs */

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_AMD_H_ */
