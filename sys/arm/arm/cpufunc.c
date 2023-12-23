/*	$NetBSD: cpufunc.c,v 1.65 2003/11/05 12:53:15 scw Exp $	*/

/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * arm9 support code Copyright (C) 2001 ARM Ltd
 * Copyright (c) 1997 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * RiscBSD kernel project
 *
 * cpufuncs.c
 *
 * C functions for supporting CPU / MMU / TLB specific operations.
 *
 * Created      : 30/01/97
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/disassem.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/uma.h>

#include <machine/cpufunc.h>

/* PRIMARY CACHE VARIABLES */

unsigned int	arm_dcache_align;
unsigned int	arm_dcache_align_mask;

#ifdef CPU_MV_PJ4B
static void pj4bv7_setup(void);
#endif
#if defined(CPU_ARM1176)
static void arm11x6_setup(void);
#endif
#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
static void cortexa_setup(void);
#endif

#ifdef CPU_MV_PJ4B
struct cpu_functions pj4bv7_cpufuncs = {
	/* Cache operations */
	.cf_l2cache_wbinv_all = (void *)cpufunc_nullop,
	.cf_l2cache_wbinv_range = (void *)cpufunc_nullop,
	.cf_l2cache_inv_range = (void *)cpufunc_nullop,
	.cf_l2cache_wb_range = (void *)cpufunc_nullop,
	.cf_l2cache_drain_writebuf = (void *)cpufunc_nullop,

	/* Other functions */
	.cf_sleep = (void *)cpufunc_nullop,

	/* Soft functions */
	.cf_setup = pj4bv7_setup
};
#endif /* CPU_MV_PJ4B */

#if defined(CPU_ARM1176)
struct cpu_functions arm1176_cpufuncs = {
	/* Cache operations */
	.cf_l2cache_wbinv_all = (void *)cpufunc_nullop,
	.cf_l2cache_wbinv_range = (void *)cpufunc_nullop,
	.cf_l2cache_inv_range = (void *)cpufunc_nullop,
	.cf_l2cache_wb_range = (void *)cpufunc_nullop,
	.cf_l2cache_drain_writebuf = (void *)cpufunc_nullop,

	/* Other functions */
	.cf_sleep = arm11x6_sleep, 

	/* Soft functions */
	.cf_setup = arm11x6_setup
};
#endif /*CPU_ARM1176 */

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
struct cpu_functions cortexa_cpufuncs = {
	/* Cache operations */

	/*
	 * Note: For CPUs using the PL310 the L2 ops are filled in when the
	 * L2 cache controller is actually enabled.
	 */
	.cf_l2cache_wbinv_all = cpufunc_nullop,
	.cf_l2cache_wbinv_range = (void *)cpufunc_nullop,
	.cf_l2cache_inv_range = (void *)cpufunc_nullop,
	.cf_l2cache_wb_range = (void *)cpufunc_nullop,
	.cf_l2cache_drain_writebuf = (void *)cpufunc_nullop,

	/* Other functions */
	.cf_sleep = armv7_cpu_sleep,

	/* Soft functions */
	.cf_setup = cortexa_setup
};
#endif /* CPU_CORTEXA || CPU_KRAIT */

/*
 * Global constants also used by locore.s
 */

struct cpu_functions cpufuncs;
u_int cputype;

static void get_cachetype_cp15(void);

static void
get_cachetype_cp15(void)
{
	u_int ctype, dsize, cpuid;
	u_int clevel, csize, i, sel;
	u_char type;

	ctype = cp15_ctr_get();
	cpuid = cp15_midr_get();
	/*
	 * ...and thus spake the ARM ARM:
	 *
	 * If an <opcode2> value corresponding to an unimplemented or
	 * reserved ID register is encountered, the System Control
	 * processor returns the value of the main ID register.
	 */
	if (ctype == cpuid)
		goto out;

	if (CPU_CT_FORMAT(ctype) == CPU_CT_ARMV7) {
		__asm __volatile("mrc p15, 1, %0, c0, c0, 1"
		    : "=r" (clevel));
		i = 0;
		while ((type = (clevel & 0x7)) && i < 7) {
			if (type == CACHE_DCACHE || type == CACHE_UNI_CACHE ||
			    type == CACHE_SEP_CACHE) {
				sel = i << 1;
				__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
				    : : "r" (sel));
				__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
				    : "=r" (csize));
				arm_dcache_align = 1U <<
				    (CPUV7_CT_xSIZE_LEN(csize) + 4);
			}
			if (type == CACHE_ICACHE || type == CACHE_SEP_CACHE) {
				sel = (i << 1) | 1;
				__asm __volatile("mcr p15, 2, %0, c0, c0, 0"
				    : : "r" (sel));
				__asm __volatile("mrc p15, 1, %0, c0, c0, 0"
				    : "=r" (csize));
			}
			i++;
			clevel >>= 3;
		}
	} else {
		/*
		 * If you want to know how this code works, go read the ARM ARM.
		 */

		dsize = CPU_CT_DSIZE(ctype);
		arm_dcache_align = 1U << (CPU_CT_xSIZE_LEN(dsize) + 3);
		if (CPU_CT_xSIZE_ASSOC(dsize) == 0) {
			if (dsize & CPU_CT_xSIZE_M)
				arm_dcache_align = 0; /* not present */
		}
	}

out:
	arm_dcache_align_mask = arm_dcache_align - 1;
}

/*
 * Cannot panic here as we may not have a console yet ...
 */

int
set_cpufuncs(void)
{
	cputype = cp15_midr_get();
	cputype &= CPU_ID_CPU_MASK;

#if defined(CPU_ARM1176)
	if (cputype == CPU_ID_ARM1176JZS) {
		cpufuncs = arm1176_cpufuncs;
		get_cachetype_cp15();
		goto out;
	}
#endif /* CPU_ARM1176 */
#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
	switch(cputype & CPU_ID_SCHEME_MASK) {
	case CPU_ID_CORTEXA5:
	case CPU_ID_CORTEXA7:
	case CPU_ID_CORTEXA8:
	case CPU_ID_CORTEXA9:
	case CPU_ID_CORTEXA12:
	case CPU_ID_CORTEXA15:
	case CPU_ID_CORTEXA53:
	case CPU_ID_CORTEXA57:
	case CPU_ID_CORTEXA72:
	case CPU_ID_KRAIT300:
		cpufuncs = cortexa_cpufuncs;
		get_cachetype_cp15();
		goto out;
	default:
		break;
	}
#endif /* CPU_CORTEXA || CPU_KRAIT */

#if defined(CPU_MV_PJ4B)
	if (cputype == CPU_ID_MV88SV581X_V7 ||
	    cputype == CPU_ID_MV88SV584X_V7 ||
	    cputype == CPU_ID_ARM_88SV581X_V7) {
		cpufuncs = pj4bv7_cpufuncs;
		get_cachetype_cp15();
		goto out;
	}
#endif /* CPU_MV_PJ4B */

	/*
	 * Bzzzz. And the answer was ...
	 */
	panic("No support for this CPU type (%08x) in kernel", cputype);
	return(ARCHITECTURE_NOT_PRESENT);
out:
	uma_set_cache_align_mask(arm_dcache_align_mask);
	return (0);
}

/*
 * CPU Setup code
 */


#if defined(CPU_ARM1176) \
 || defined(CPU_MV_PJ4B) \
 || defined(CPU_CORTEXA) || defined(CPU_KRAIT)
static __inline void
cpu_scc_setup_ccnt(void)
{
/* This is how you give userland access to the CCNT and PMCn
 * registers.
 * BEWARE! This gives write access also, which may not be what
 * you want!
 */
#ifdef _PMC_USER_READ_WRITE_
	/* Set PMUSERENR[0] to allow userland access */
	cp15_pmuserenr_set(1);
#endif
#if defined(CPU_ARM1176)
	/* Set PMCR[2,0] to enable counters and reset CCNT */
	cp15_pmcr_set(5);
#else
	/* Set up the PMCCNTR register as a cyclecounter:
	 * Set PMINTENCLR to 0xFFFFFFFF to block interrupts
	 * Set PMCR[2,0] to enable counters and reset CCNT
	 * Set PMCNTENSET to 0x80000000 to enable CCNT */
	cp15_pminten_clr(0xFFFFFFFF);
	cp15_pmcr_set(5);
	cp15_pmcnten_set(0x80000000);
#endif
}
#endif

#if defined(CPU_ARM1176)
static void
arm11x6_setup(void)
{
	uint32_t auxctrl, auxctrl_wax;
	uint32_t tmp, tmp2;
	uint32_t cpuid;

	cpuid = cp15_midr_get();

	auxctrl = 0;
	auxctrl_wax = ~0;

	/*
	 * Enable an errata workaround
	 */
	if ((cpuid & CPU_ID_CPU_MASK) == CPU_ID_ARM1176JZS) { /* ARM1176JZSr0 */
		auxctrl = ARM1176_AUXCTL_PHD;
		auxctrl_wax = ~ARM1176_AUXCTL_PHD;
	}

	tmp = cp15_actlr_get();
	tmp2 = tmp;
	tmp &= auxctrl_wax;
	tmp |= auxctrl;
	if (tmp != tmp2)
		cp15_actlr_set(tmp);

	cpu_scc_setup_ccnt();
}
#endif  /* CPU_ARM1176 */

#ifdef CPU_MV_PJ4B
static void
pj4bv7_setup(void)
{

	pj4b_config();
	cpu_scc_setup_ccnt();
}
#endif /* CPU_MV_PJ4B */

#if defined(CPU_CORTEXA) || defined(CPU_KRAIT)
static void
cortexa_setup(void)
{

	cpu_scc_setup_ccnt();
}
#endif  /* CPU_CORTEXA || CPU_KRAIT */
