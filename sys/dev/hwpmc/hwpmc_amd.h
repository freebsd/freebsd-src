/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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
 */

/* Machine dependent interfaces */

#ifndef _DEV_HWPMC_AMD_H_
#define	_DEV_HWPMC_AMD_H_ 1

/* CPUIDs */
#define	CPUID_EXTPERFMON	0x80000022
#define	EXTPERFMON_CORE_PMCS(x)	((x) & 0x0F)
#define	EXTPERFMON_DF_PMCS(x)	(((x) >> 10) & 0x3F)

/* AMD K8 PMCs */
#define	AMD_PMC_EVSEL_0		0xC0010000
#define	AMD_PMC_EVSEL_1		0xC0010001
#define	AMD_PMC_EVSEL_2		0xC0010002
#define	AMD_PMC_EVSEL_3		0xC0010003

#define	AMD_PMC_PERFCTR_0	0xC0010004
#define	AMD_PMC_PERFCTR_1	0xC0010005
#define	AMD_PMC_PERFCTR_2	0xC0010006
#define	AMD_PMC_PERFCTR_3	0xC0010007

/*
 * For older AMD processors we have hard coded the original four core counters.
 * For newer processors we use the cpuid bits to setup the counter table.  The
 * counts below are the default number of registers assuming that you do not
 * have CPUID leaf 0x80000022.  The maximum number of counters is computed
 * based on the available bits in the CPUID leaf and reserved MSR space.
 *
 * Refer to the PPRs for AMD Family 1Ah.
 */

/* CORE */
#define	AMD_PMC_CORE_BASE	0xC0010200
#define	AMD_PMC_CORE_DEFAULT	6
#define	AMD_PMC_CORE_MAX	16

/* L3 */
#define	AMD_PMC_L3_BASE		0xC0010230
#define	AMD_PMC_L3_DEFAULT	6
#define	AMD_PMC_L3_MAX		6

/* DF */
#define	AMD_PMC_DF_BASE		0xC0010240
#define	AMD_PMC_DF_DEFAULT	4
#define	AMD_PMC_DF_MAX		64

#define	AMD_NPMCS_K8		4
#define AMD_NPMCS_MAX		(AMD_PMC_CORE_MAX + AMD_PMC_L3_MAX + \
				 AMD_PMC_DF_MAX)

#define	AMD_PMC_COUNTERMASK	0xFF000000
#define	AMD_PMC_TO_COUNTER(x)	(((x) << 24) & AMD_PMC_COUNTERMASK)
#define	AMD_PMC_INVERT		(1 << 23)
#define	AMD_PMC_ENABLE		(1 << 22)
#define	AMD_PMC_INT		(1 << 20)
#define	AMD_PMC_PC		(1 << 19)
#define	AMD_PMC_EDGE		(1 << 18)
#define	AMD_PMC_OS		(1 << 17)
#define	AMD_PMC_USR		(1 << 16)
#define	AMD_PMC_L3SLICEMASK	(0x000F000000000000)
#define	AMD_PMC_L3COREMASK	(0xFF00000000000000)
#define	AMD_PMC_TO_L3SLICE(x)	(((x) << 48) & AMD_PMC_L3SLICEMASK)
#define	AMD_PMC_TO_L3CORE(x)	(((x) << 56) & AMD_PMC_L3COREMASK)

#define	AMD_PMC_UNITMASK_M	0x10
#define	AMD_PMC_UNITMASK_O	0x08
#define	AMD_PMC_UNITMASK_E	0x04
#define	AMD_PMC_UNITMASK_S	0x02
#define	AMD_PMC_UNITMASK_I	0x01
#define	AMD_PMC_UNITMASK_MOESI	0x1F

#define	AMD_PMC_UNITMASK	0xFF00
#define	AMD_PMC_EVENTMASK 	0xF000000FF

#define	AMD_PMC_TO_UNITMASK(x)	(((x) << 8) & AMD_PMC_UNITMASK)
#define	AMD_PMC_TO_EVENTMASK(x)	(((x) & 0xFF) | (((uint64_t)(x) & 0xF00) << 24))
#define	AMD_PMC_TO_EVENTMASK_DF(x)	(((x) & 0xFF) | (((uint64_t)(x) & 0x0F00) << 24)) | (((uint64_t)(x) & 0x3000) << 47)
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

enum sub_class {
	PMC_AMD_SUB_CLASS_CORE,
	PMC_AMD_SUB_CLASS_L3_CACHE,
	PMC_AMD_SUB_CLASS_DATA_FABRIC
};

struct pmc_md_amd_op_pmcallocate {
	uint64_t	pm_amd_config;
	uint32_t	pm_amd_sub_class;
};

#ifdef _KERNEL

/* MD extension for 'struct pmc' */
struct pmc_md_amd_pmc {
	uint64_t	pm_amd_evsel;
};

#endif /* _KERNEL */
#endif /* _DEV_HWPMC_AMD_H_ */
