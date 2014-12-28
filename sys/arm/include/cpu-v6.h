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
#ifndef MACHINE_CPU_V6_H
#define MACHINE_CPU_V6_H

#include "machine/atomic.h"
#include "machine/cpufunc.h"
#include "machine/cpuinfo.h"
#include "machine/sysreg.h"


#define CPU_ASID_KERNEL 0

/*
 * Macros to generate CP15 (system control processor) read/write functions.
 */
#define _FX(s...) #s

#define _RF0(fname, aname...)						\
static __inline register_t						\
fname(void)								\
{									\
	register_t reg;							\
	__asm __volatile("mrc\t" _FX(aname): "=r" (reg));		\
	return(reg);							\
}

#define _WF0(fname, aname...)						\
static __inline void							\
fname(void)								\
{									\
	__asm __volatile("mcr\t" _FX(aname));				\
}

#define _WF1(fname, aname...)						\
static __inline void							\
fname(register_t reg)							\
{									\
	__asm __volatile("mcr\t" _FX(aname):: "r" (reg));		\
}

/*
 * Raw CP15  maintenance operations
 * !!! not for external use !!!
 */

/* TLB */

_WF0(_CP15_TLBIALL, CP15_TLBIALL)		/* Invalidate entire unified TLB */
#if __ARM_ARCH >= 7 && defined SMP
_WF0(_CP15_TLBIALLIS, CP15_TLBIALLIS)		/* Invalidate entire unified TLB IS */
#endif
_WF1(_CP15_TLBIASID, CP15_TLBIASID(%0))		/* Invalidate unified TLB by ASID */
#if __ARM_ARCH >= 7 && defined SMP
_WF1(_CP15_TLBIASIDIS, CP15_TLBIASIDIS(%0))	/* Invalidate unified TLB by ASID IS */
#endif
_WF1(_CP15_TLBIMVAA, CP15_TLBIMVAA(%0))		/* Invalidate unified TLB by MVA, all ASID */
#if __ARM_ARCH >= 7 && defined SMP
_WF1(_CP15_TLBIMVAAIS, CP15_TLBIMVAAIS(%0))	/* Invalidate unified TLB by MVA, all ASID IS */
#endif
_WF1(_CP15_TLBIMVA, CP15_TLBIMVA(%0))		/* Invalidate unified TLB by MVA */

_WF1(_CP15_TTB_SET, CP15_TTBR0(%0))

/* Cache and Branch predictor */

_WF0(_CP15_BPIALL, CP15_BPIALL)			/* Branch predictor invalidate all */
#if __ARM_ARCH >= 7 && defined SMP
_WF0(_CP15_BPIALLIS, CP15_BPIALLIS)		/* Branch predictor invalidate all IS */
#endif
_WF1(_CP15_BPIMVA, CP15_BPIMVA(%0))		/* Branch predictor invalidate by MVA */
_WF1(_CP15_DCCIMVAC, CP15_DCCIMVAC(%0))		/* Data cache clean and invalidate by MVA PoC */
_WF1(_CP15_DCCISW, CP15_DCCISW(%0))		/* Data cache clean and invalidate by set/way */
_WF1(_CP15_DCCMVAC, CP15_DCCMVAC(%0))		/* Data cache clean by MVA PoC */
#if __ARM_ARCH >= 7
_WF1(_CP15_DCCMVAU, CP15_DCCMVAU(%0))		/* Data cache clean by MVA PoU */
#endif
_WF1(_CP15_DCCSW, CP15_DCCSW(%0))		/* Data cache clean by set/way */
_WF1(_CP15_DCIMVAC, CP15_DCIMVAC(%0))		/* Data cache invalidate by MVA PoC */
_WF1(_CP15_DCISW, CP15_DCISW(%0))		/* Data cache invalidate by set/way */
_WF0(_CP15_ICIALLU, CP15_ICIALLU)		/* Instruction cache invalidate all PoU */
#if __ARM_ARCH >= 7 && defined SMP
_WF0(_CP15_ICIALLUIS, CP15_ICIALLUIS)		/* Instruction cache invalidate all PoU IS */
#endif
_WF1(_CP15_ICIMVAU, CP15_ICIMVAU(%0))		/* Instruction cache invalidate */

/*
 * Publicly accessible functions
 */

/* Various control registers */

_RF0(cp15_dfsr_get, CP15_DFSR(%0))
_RF0(cp15_ifsr_get, CP15_IFSR(%0))
_WF1(cp15_prrr_set, CP15_PRRR(%0))
_WF1(cp15_nmrr_set, CP15_NMRR(%0))
_RF0(cp15_ttbr_get, CP15_TTBR0(%0))
_RF0(cp15_dfar_get, CP15_DFAR(%0))
#if __ARM_ARCH >= 7
_RF0(cp15_ifar_get, CP15_IFAR(%0))
#endif

/*CPU id registers */
_RF0(cp15_midr_get, CP15_MIDR(%0))
_RF0(cp15_ctr_get, CP15_CTR(%0))
_RF0(cp15_tcmtr_get, CP15_TCMTR(%0))
_RF0(cp15_tlbtr_get, CP15_TLBTR(%0))
_RF0(cp15_mpidr_get, CP15_MPIDR(%0))
_RF0(cp15_revidr_get, CP15_REVIDR(%0))
_RF0(cp15_aidr_get, CP15_AIDR(%0))
_RF0(cp15_id_pfr0_get, CP15_ID_PFR0(%0))
_RF0(cp15_id_pfr1_get, CP15_ID_PFR1(%0))
_RF0(cp15_id_dfr0_get, CP15_ID_DFR0(%0))
_RF0(cp15_id_afr0_get, CP15_ID_AFR0(%0))
_RF0(cp15_id_mmfr0_get, CP15_ID_MMFR0(%0))
_RF0(cp15_id_mmfr1_get, CP15_ID_MMFR1(%0))
_RF0(cp15_id_mmfr2_get, CP15_ID_MMFR2(%0))
_RF0(cp15_id_mmfr3_get, CP15_ID_MMFR3(%0))
_RF0(cp15_id_isar0_get, CP15_ID_ISAR0(%0))
_RF0(cp15_id_isar1_get, CP15_ID_ISAR1(%0))
_RF0(cp15_id_isar2_get, CP15_ID_ISAR2(%0))
_RF0(cp15_id_isar3_get, CP15_ID_ISAR3(%0))
_RF0(cp15_id_isar4_get, CP15_ID_ISAR4(%0))
_RF0(cp15_id_isar5_get, CP15_ID_ISAR5(%0))
_RF0(cp15_cbar_get, CP15_CBAR(%0))

#undef	_FX
#undef	_RF0
#undef	_WF0
#undef	_WF1

#endif /* !MACHINE_CPU_V6_H */
