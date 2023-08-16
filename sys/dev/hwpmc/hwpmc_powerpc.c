/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2011,2013 Justin Hibbits
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
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/sysent.h>
#include <sys/syslog.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/spr.h>
#include <machine/pte.h>
#include <machine/sr.h>
#include <machine/cpu.h>
#include <machine/stack.h>

#include "hwpmc_powerpc.h"

#ifdef __powerpc64__
#define OFFSET 4 /* Account for the TOC reload slot */
#else
#define OFFSET 0
#endif

struct powerpc_cpu **powerpc_pcpu;
struct pmc_ppc_event *ppc_event_codes;
size_t ppc_event_codes_size;
int ppc_event_first;
int ppc_event_last;
int ppc_max_pmcs;
enum pmc_class ppc_class;

void (*powerpc_set_pmc)(int cpu, int ri, int config);
pmc_value_t (*powerpc_pmcn_read)(unsigned int pmc);
void (*powerpc_pmcn_write)(unsigned int pmc, uint32_t val);
void (*powerpc_resume_pmc)(bool ie);


int
pmc_save_kernel_callchain(uintptr_t *cc, int maxsamples,
    struct trapframe *tf)
{
	uintptr_t *osp, *sp;
	uintptr_t pc;
	int frames = 0;

	cc[frames++] = PMC_TRAPFRAME_TO_PC(tf);
	sp = (uintptr_t *)PMC_TRAPFRAME_TO_FP(tf);
	osp = (uintptr_t *)PAGE_SIZE;

	for (; frames < maxsamples; frames++) {
		if (sp <= osp)
			break;
	    #ifdef __powerpc64__
		pc = sp[2];
	    #else
		pc = sp[1];
	    #endif
		if ((pc & 3) || (pc < 0x100))
			break;

		/*
		 * trapexit() and asttrapexit() are sentinels
		 * for kernel stack tracing.
		 * */
		if (pc + OFFSET == (uintptr_t) &trapexit ||
		    pc + OFFSET == (uintptr_t) &asttrapexit)
			break;

		cc[frames] = pc;
		osp = sp;
		sp = (uintptr_t *)*sp;
	}
	return (frames);
}

int
powerpc_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d], illegal CPU %d", __LINE__, cpu));

	phw = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];

	snprintf(pi->pm_name, sizeof(pi->pm_name), "POWERPC-%d", ri);
	pi->pm_class = powerpc_pcpu[cpu]->pc_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc	       = NULL;
	}

	return (0);
}

int
powerpc_get_config(int cpu, int ri, struct pmc **ppm)
{

	*ppm = powerpc_pcpu[cpu]->pc_ppcpmcs[ri].phw_pmc;

	return (0);
}

int
powerpc_pcpu_init(struct pmc_mdep *md, int cpu)
{
	struct pmc_cpu *pc;
	struct powerpc_cpu *pac;
	struct pmc_hw  *phw;
	int first_ri, i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] wrong cpu number %d", __LINE__, cpu));
	PMCDBG1(MDP,INI,1,"powerpc-init cpu=%d", cpu);

	powerpc_pcpu[cpu] = pac = malloc(sizeof(struct powerpc_cpu) +
	    ppc_max_pmcs * sizeof(struct pmc_hw), M_PMC, M_WAITOK | M_ZERO);
	pac->pc_class =
	    md->pmd_classdep[PMC_MDEP_CLASS_INDEX_POWERPC].pcd_class;

	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_POWERPC].pcd_ri;
	KASSERT(pc != NULL, ("[powerpc,%d] NULL per-cpu pointer", __LINE__));

	for (i = 0, phw = pac->pc_ppcpmcs; i < ppc_max_pmcs; i++, phw++) {
		phw->phw_state = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i);
		phw->phw_pmc = NULL;
		pc->pc_hwpmcs[i + first_ri] = phw;
	}

	return (0);
}

int
powerpc_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	PMCDBG1(MDP,INI,1,"powerpc-fini cpu=%d", cpu);

	free(powerpc_pcpu[cpu], M_PMC);
	powerpc_pcpu[cpu] = NULL;

	return (0);
}

int
powerpc_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	enum pmc_event pe;
	uint32_t caps, config = 0, counter = 0;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ppc_max_pmcs,
	    ("[powerpc,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != ppc_class)
		return (EINVAL);

	caps = a->pm_caps;

	pe = a->pm_ev;

	if (pe < ppc_event_first || pe > ppc_event_last)
		return (EINVAL);

	for (i = 0; i < ppc_event_codes_size; i++) {
		if (ppc_event_codes[i].pe_event == pe) {
			config = ppc_event_codes[i].pe_code;
			counter =  ppc_event_codes[i].pe_flags;
			break;
		}
	}
	if (i == ppc_event_codes_size)
		return (EINVAL);

	if ((counter & (1 << ri)) == 0)
		return (EINVAL);

	if (caps & PMC_CAP_SYSTEM)
		config |= POWERPC_PMC_KERNEL_ENABLE;
	if (caps & PMC_CAP_USER)
		config |= POWERPC_PMC_USER_ENABLE;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		config |= POWERPC_PMC_ENABLE;

	pm->pm_md.pm_powerpc.pm_powerpc_evsel = config;

	PMCDBG3(MDP,ALL,1,"powerpc-allocate cpu=%d ri=%d -> config=0x%x",
	    cpu, ri, config);
	return (0);
}

int
powerpc_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct pmc_hw *phw __diagused;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ppc_max_pmcs,
	    ("[powerpc,%d] illegal row-index %d", __LINE__, ri));

	phw = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];
	KASSERT(phw->phw_pmc == NULL,
	    ("[powerpc,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return (0);
}

int
powerpc_start_pmc(int cpu, int ri, struct pmc *pm)
{

	PMCDBG2(MDP,STA,1,"powerpc-start cpu=%d ri=%d", cpu, ri);
	powerpc_set_pmc(cpu, ri, pm->pm_md.pm_powerpc.pm_powerpc_evsel);

	return (0);
}

int
powerpc_stop_pmc(int cpu, int ri, struct pmc *pm __unused)
{
	PMCDBG2(MDP,STO,1, "powerpc-stop cpu=%d ri=%d", cpu, ri);
	powerpc_set_pmc(cpu, ri, PMCN_NONE);
	return (0);
}

int
powerpc_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP,CFG,1, "powerpc-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ppc_max_pmcs,
	    ("[powerpc,%d] illegal row-index %d", __LINE__, ri));

	phw = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[powerpc,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
	    __LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return (0);
}

pmc_value_t
powerpc_pmcn_read_default(unsigned int pmc)
{
	pmc_value_t val;

	if (pmc > ppc_max_pmcs)
		panic("Invalid PMC number: %d\n", pmc);

	switch (pmc) {
	case 0:
		val = mfspr(SPR_PMC1);
		break;
	case 1:
		val = mfspr(SPR_PMC2);
		break;
	case 2:
		val = mfspr(SPR_PMC3);
		break;
	case 3:
		val = mfspr(SPR_PMC4);
		break;
	case 4:
		val = mfspr(SPR_PMC5);
		break;
	case 5:
		val = mfspr(SPR_PMC6);
		break;
	case 6:
		val = mfspr(SPR_PMC7);
		break;
	case 7:
		val = mfspr(SPR_PMC8);
		break;
	}

	return (val);
}

void
powerpc_pmcn_write_default(unsigned int pmc, uint32_t val)
{
	if (pmc > ppc_max_pmcs)
		panic("Invalid PMC number: %d\n", pmc);

	switch (pmc) {
	case 0:
		mtspr(SPR_PMC1, val);
		break;
	case 1:
		mtspr(SPR_PMC2, val);
		break;
	case 2:
		mtspr(SPR_PMC3, val);
		break;
	case 3:
		mtspr(SPR_PMC4, val);
		break;
	case 4:
		mtspr(SPR_PMC5, val);
		break;
	case 5:
		mtspr(SPR_PMC6, val);
		break;
	case 6:
		mtspr(SPR_PMC7, val);
		break;
	case 7:
		mtspr(SPR_PMC8, val);
		break;
	}
}

int
powerpc_read_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t *v)
{
	pmc_value_t p, r, tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ppc_max_pmcs,
	    ("[powerpc,%d] illegal row index %d", __LINE__, ri));

	/*
	 * After an interrupt occurs because of a PMC overflow, the PMC value
	 * is not always MAX_PMC_VALUE + 1, but may be a little above it.
	 * This may mess up calculations and frustrate machine independent
	 * layer expectations, such as that no value read should be greater
	 * than reload count in sampling mode.
	 * To avoid these issues, use MAX_PMC_VALUE as an upper limit.
	 */
	p = MIN(powerpc_pmcn_read(ri), POWERPC_MAX_PMC_VALUE);
	r = pm->pm_sc.pm_reloadcount;

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
		/*
		 * Special case 1: r is too big
		 * This usually happens when a PMC write fails, the PMC is
		 * stopped and then it is read.
		 *
		 * Special case 2: PMC was reseted or has a value
		 * that should not be possible with current r.
		 *
		 * In the above cases, just return 0 instead of an arbitrary
		 * value.
		 */
		if (r > POWERPC_MAX_PMC_VALUE || p + r <= POWERPC_MAX_PMC_VALUE)
			tmp = 0;
		else
			tmp = POWERPC_PERFCTR_VALUE_TO_RELOAD_COUNT(p);
	} else
		tmp = p + (POWERPC_MAX_PMC_VALUE + 1) * PPC_OVERFLOWCNT(pm);

	PMCDBG5(MDP,REA,1,"ppc-read cpu=%d ri=%d -> %jx (%jx,%jx)",
	    cpu, ri, (uintmax_t)tmp, (uintmax_t)PPC_OVERFLOWCNT(pm),
	    (uintmax_t)p);
	*v = tmp;
	return (0);
}

int
powerpc_write_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t v)
{
	pmc_value_t vlo;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < ppc_max_pmcs,
	    ("[powerpc,%d] illegal row-index %d", __LINE__, ri));

	if (PMC_IS_COUNTING_MODE(PMC_TO_MODE(pm))) {
		PPC_OVERFLOWCNT(pm) = v / (POWERPC_MAX_PMC_VALUE + 1);
		vlo = v % (POWERPC_MAX_PMC_VALUE + 1);
	} else if (v > POWERPC_MAX_PMC_VALUE) {
		PMCDBG3(MDP,WRI,2,
		    "powerpc-write cpu=%d ri=%d: PMC value is too big: %jx",
		    cpu, ri, (uintmax_t)v);
		return (EINVAL);
	} else
		vlo = POWERPC_RELOAD_COUNT_TO_PERFCTR_VALUE(v);

	PMCDBG5(MDP,WRI,1,"powerpc-write cpu=%d ri=%d -> %jx (%jx,%jx)",
	    cpu, ri, (uintmax_t)v, (uintmax_t)PPC_OVERFLOWCNT(pm),
	    (uintmax_t)vlo);

	powerpc_pmcn_write(ri, vlo);
	return (0);
}

int
powerpc_pmc_intr(struct trapframe *tf)
{
	struct pmc *pm;
	struct powerpc_cpu *pc;
	int cpu, error, i, retval;

	cpu = curcpu;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] out of range CPU %d", __LINE__, cpu));

	PMCDBG3(MDP,INT,1, "cpu=%d tf=%p um=%d", cpu, (void *) tf,
	    TRAPF_USERMODE(tf));

	retval = 0;
	pc = powerpc_pcpu[cpu];

	/*
	 * Look for a running, sampling PMC which has overflowed
	 * and which has a valid 'struct pmc' association.
	 */
	for (i = 0; i < ppc_max_pmcs; i++) {
		if (!POWERPC_PMC_HAS_OVERFLOWED(i))
			continue;
		retval = 1;	/* Found an interrupting PMC. */

		/*
		 * Always clear the PMC, to make it stop interrupting.
		 * If pm is available and in sampling mode, use reload
		 * count, to make PMC read after stop correct.
		 * Otherwise, just reset the PMC.
		 */
		if ((pm = pc->pc_ppcpmcs[i].phw_pmc) != NULL &&
		    PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
			if (pm->pm_state != PMC_STATE_RUNNING) {
				powerpc_write_pmc(cpu, i, pm,
				    pm->pm_sc.pm_reloadcount);
				continue;
			}
		} else {
			if (pm != NULL) { /* !PMC_IS_SAMPLING_MODE */
				PPC_OVERFLOWCNT(pm) = (PPC_OVERFLOWCNT(pm) +
				    1) % PPC_OVERFLOWCNT_MAX;
				PMCDBG3(MDP,INT,2,
				    "cpu=%d ri=%d: overflowcnt=%d",
				    cpu, i, PPC_OVERFLOWCNT(pm));
			}

			powerpc_pmcn_write(i, 0);
			continue;
		}

		error = pmc_process_interrupt(PMC_HR, pm, tf);
		if (error != 0) {
			PMCDBG3(MDP,INT,3,
			    "cpu=%d ri=%d: error %d processing interrupt",
			    cpu, i, error);
			powerpc_stop_pmc(cpu, i, pm);
		}

		/* Reload sampling count */
		powerpc_write_pmc(cpu, i, pm, pm->pm_sc.pm_reloadcount);
	}

	if (retval)
		counter_u64_add(pmc_stats.pm_intr_processed, 1);
	else
		counter_u64_add(pmc_stats.pm_intr_ignored, 1);

	/*
	 * Re-enable PERF exceptions if we were able to find the interrupt
	 * source and handle it. Otherwise, it's better to disable PERF
	 * interrupts, to avoid the risk of processing the same interrupt
	 * forever.
	 */
	powerpc_resume_pmc(retval != 0);
	if (retval == 0)
		log(LOG_WARNING,
		    "pmc_intr: couldn't find interrupting PMC on cpu %d - "
		    "disabling PERF interrupts\n", cpu);

	return (retval);
}

struct pmc_mdep *
pmc_md_initialize(void)
{
	struct pmc_mdep *pmc_mdep;
	int error;
	uint16_t vers;
	
	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	powerpc_pcpu = malloc(sizeof(struct powerpc_cpu *) * pmc_cpu_max(), M_PMC,
			   M_WAITOK|M_ZERO);

	/* Just one class */
	pmc_mdep = pmc_mdep_alloc(1);

	vers = mfpvr() >> 16;

	switch (vers) {
	case MPC7447A:
	case MPC7448:
	case MPC7450:
	case MPC7455:
	case MPC7457:
		error = pmc_mpc7xxx_initialize(pmc_mdep);
		break;
	case IBM970:
	case IBM970FX:
	case IBM970MP:
		error = pmc_ppc970_initialize(pmc_mdep);
		break;
	case IBMPOWER8E:
	case IBMPOWER8NVL:
	case IBMPOWER8:
	case IBMPOWER9:
		error = pmc_power8_initialize(pmc_mdep);
		break;
	case FSL_E500v1:
	case FSL_E500v2:
	case FSL_E500mc:
	case FSL_E5500:
		error = pmc_e500_initialize(pmc_mdep);
		break;
	default:
		error = -1;
		break;
	}

	if (error != 0) {
		pmc_mdep_free(pmc_mdep);
		pmc_mdep = NULL;
	}

	/* Set the value for kern.hwpmc.cpuid */
	snprintf(pmc_cpuid, sizeof(pmc_cpuid), "%08x", mfpvr());

	return (pmc_mdep);
}

void
pmc_md_finalize(struct pmc_mdep *md)
{

	free(powerpc_pcpu, M_PMC);
	powerpc_pcpu = NULL;
}

int
pmc_save_user_callchain(uintptr_t *cc, int maxsamples,
    struct trapframe *tf)
{
	uintptr_t *osp, *sp;
	int frames = 0;

	cc[frames++] = PMC_TRAPFRAME_TO_PC(tf);
	sp = (uintptr_t *)PMC_TRAPFRAME_TO_FP(tf);
	osp = NULL;

	for (; frames < maxsamples; frames++) {
		if (sp <= osp)
			break;
		osp = sp;
#ifdef __powerpc64__
		/* Check if 32-bit mode. */
		if (!(tf->srr1 & PSL_SF)) {
			cc[frames] = fuword32((uint32_t *)sp + 1);
			sp = (uintptr_t *)(uintptr_t)fuword32(sp);
		} else {
			cc[frames] = fuword(sp + 2);
			sp = (uintptr_t *)fuword(sp);
		}
#else
		cc[frames] = fuword32((uint32_t *)sp + 1);
		sp = (uintptr_t *)fuword32(sp);
#endif
	}

	return (frames);
}
