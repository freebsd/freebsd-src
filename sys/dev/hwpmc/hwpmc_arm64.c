/*-
 * Copyright (c) 2015 Ruslan Bukin <br@bsdpad.com>
 * All rights reserved.
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory with support from ARM Ltd.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/pmc_mdep.h>
#include <machine/cpu.h>
#include <machine/machdep.h>

#include "opt_acpi.h"

static int arm64_npmcs;
static bool arm64_64bit_events __read_mostly = false;

struct arm64_event_code_map {
	enum pmc_event	pe_ev;
	uint8_t		pe_code;
};

/*
 * Per-processor information.
 */
struct arm64_cpu {
	struct pmc_hw   *pc_arm64pmcs;
};

static struct arm64_cpu **arm64_pcpu;

/*
 * Interrupt Enable Set Register
 */
static __inline void
arm64_interrupt_enable(uint32_t pmc)
{
	uint32_t reg;

	reg = (1 << pmc);
	WRITE_SPECIALREG(pmintenset_el1, reg);

	isb();
}

/*
 * Interrupt Clear Set Register
 */
static __inline void
arm64_interrupt_disable(uint32_t pmc)
{
	uint32_t reg;

	reg = (1 << pmc);
	WRITE_SPECIALREG(pmintenclr_el1, reg);

	isb();
}

/*
 * Counter Set Enable Register
 */
static __inline void
arm64_counter_enable(unsigned int pmc)
{
	uint32_t reg;

	reg = (1 << pmc);
	WRITE_SPECIALREG(pmcntenset_el0, reg);

	isb();
}

/*
 * Counter Clear Enable Register
 */
static __inline void
arm64_counter_disable(unsigned int pmc)
{
	uint32_t reg;

	reg = (1 << pmc);
	WRITE_SPECIALREG(pmcntenclr_el0, reg);

	isb();
}

/*
 * Performance Monitors Control Register
 */
static uint64_t
arm64_pmcr_read(void)
{
	uint32_t reg;

	reg = READ_SPECIALREG(pmcr_el0);

	return (reg);
}

static void
arm64_pmcr_write(uint64_t reg)
{

	WRITE_SPECIALREG(pmcr_el0, reg);

	isb();
}

/*
 * Performance Count Register N
 */
static uint64_t
arm64_pmcn_read(unsigned int pmc)
{

	KASSERT(pmc < arm64_npmcs, ("%s: illegal PMC number %d", __func__, pmc));

	WRITE_SPECIALREG(pmselr_el0, pmc);

	isb();

	return (READ_SPECIALREG(pmxevcntr_el0));
}

static void
arm64_pmcn_write(unsigned int pmc, uint64_t reg)
{

	KASSERT(pmc < arm64_npmcs, ("%s: illegal PMC number %d", __func__, pmc));

	WRITE_SPECIALREG(pmselr_el0, pmc);
	WRITE_SPECIALREG(pmxevcntr_el0, reg);

	isb();
}

static int
arm64_allocate_pmc(int cpu, int ri, struct pmc *pm,
  const struct pmc_op_pmcallocate *a)
{
	uint64_t config;
	enum pmc_event pe;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != PMC_CLASS_ARMV8) {
		return (EINVAL);
	}
	pe = a->pm_ev;

	if ((a->pm_flags & PMC_F_EV_PMU) != 0) {
		config = a->pm_md.pm_md_config;
	} else {
		config = (uint32_t)pe - PMC_EV_ARMV8_FIRST;
		if (config > (PMC_EV_ARMV8_LAST - PMC_EV_ARMV8_FIRST))
			return (EINVAL);
	}

	switch (a->pm_caps & (PMC_CAP_SYSTEM | PMC_CAP_USER)) {
	case PMC_CAP_SYSTEM:
		/* Exclude EL0 */
		config |= PMEVTYPER_U;
		if (in_vhe()) {
			/* If in VHE we need to include EL2 and exclude EL1 */
			config |= PMEVTYPER_NSH | PMEVTYPER_P;
		}
		break;
	case PMC_CAP_USER:
		/* Exclude EL1 */
		config |= PMEVTYPER_P;
		/* Exclude EL2 */
		config &= ~PMEVTYPER_NSH;
		break;
	default:
		/*
		 * Trace both USER and SYSTEM if none are specified
		 * (default setting) or if both flags are specified
		 * (user explicitly requested both qualifiers).
		 */
		if (in_vhe()) {
			/* If in VHE we need to include EL2 */
			config |= PMEVTYPER_NSH;
		}
		break;
	}

	pm->pm_md.pm_arm64.pm_arm64_evsel = config;
	PMCDBG2(MDP, ALL, 2, "arm64-allocate ri=%d -> config=0x%lx", ri,
	    config);

	return (0);
}


static int
arm64_read_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t *v)
{
	pmc_value_t tmp;
	register_t s;
	int reg;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] illegal row index %d", __LINE__, ri));

	/*
	 * Ensure we don't get interrupted while updating the overflow count.
	 */
	s = intr_disable();
	tmp = arm64_pmcn_read(ri);
	reg = (1 << ri);
	if ((READ_SPECIALREG(pmovsclr_el0) & reg) != 0) {
		/* Clear Overflow Flag */
		WRITE_SPECIALREG(pmovsclr_el0, reg);
		pm->pm_pcpu_state[cpu].pps_overflowcnt++;

		/* Reread counter in case we raced. */
		tmp = arm64_pmcn_read(ri);
	}
	/*
	 * If the counter is 32-bit increment the upper bits of the counter.
	 * It it is 64-bit then there is nothing we can do as tmp is already
	 * 64-bit.
	 */
	if (!arm64_64bit_events) {
		tmp &= 0xffffffffu;
		tmp += (uint64_t)pm->pm_pcpu_state[cpu].pps_overflowcnt << 32;
	}
	intr_restore(s);

	PMCDBG2(MDP, REA, 2, "arm64-read id=%d -> %jd", ri, tmp);
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
		/*
		 * Clamp value to 0 if the counter just overflowed,
		 * otherwise the returned reload count would wrap to a
		 * huge value.
		 */
		if ((tmp & (1ull << 63)) == 0)
			tmp = 0;
		else
			tmp = ARMV8_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	}
	*v = tmp;

	return (0);
}

static int
arm64_write_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t v)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] illegal row-index %d", __LINE__, ri));

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = ARMV8_RELOAD_COUNT_TO_PERFCTR_VALUE(v);

	PMCDBG3(MDP, WRI, 1, "arm64-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	if (!arm64_64bit_events) {
		pm->pm_pcpu_state[cpu].pps_overflowcnt = v >> 32;
		v &= 0xffffffffu;
	}
	arm64_pmcn_write(ri, v);

	return (0);
}

static int
arm64_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP, CFG, 1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] illegal row-index %d", __LINE__, ri));

	phw = &arm64_pcpu[cpu]->pc_arm64pmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[arm64,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
	    __LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return (0);
}

static int
arm64_start_pmc(int cpu, int ri, struct pmc *pm)
{
	uint64_t config;

	config = pm->pm_md.pm_arm64.pm_arm64_evsel;

	/*
	 * Configure the event selection.
	 */
	WRITE_SPECIALREG(pmselr_el0, ri);
	WRITE_SPECIALREG(pmxevtyper_el0, config);

	isb();

	/*
	 * Enable the PMC.
	 */
	arm64_interrupt_enable(ri);
	arm64_counter_enable(ri);

	return (0);
}

static int
arm64_stop_pmc(int cpu, int ri, struct pmc *pm __unused)
{
	/*
	 * Disable the PMCs.
	 */
	arm64_counter_disable(ri);
	arm64_interrupt_disable(ri);

	return (0);
}

static int
arm64_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct pmc_hw *phw __diagused;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] illegal row-index %d", __LINE__, ri));

	phw = &arm64_pcpu[cpu]->pc_arm64pmcs[ri];
	KASSERT(phw->phw_pmc == NULL,
	    ("[arm64,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return (0);
}

static int
arm64_intr(struct trapframe *tf)
{
	int retval, ri;
	struct pmc *pm;
	int error;
	int reg, cpu;

	cpu = curcpu;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] CPU %d out of range", __LINE__, cpu));

	PMCDBG3(MDP,INT,1, "cpu=%d tf=%p um=%d", cpu, (void *)tf,
	    TRAPF_USERMODE(tf));

	retval = 0;

	for (ri = 0; ri < arm64_npmcs; ri++) {
		pm = arm64_pcpu[cpu]->pc_arm64pmcs[ri].phw_pmc;
		if (pm == NULL)
			continue;
		/* Check if counter is overflowed */
		reg = (1 << ri);
		if ((READ_SPECIALREG(pmovsclr_el0) & reg) == 0)
			continue;
		/* Clear Overflow Flag */
		WRITE_SPECIALREG(pmovsclr_el0, reg);

		isb();

		retval = 1; /* Found an interrupting PMC. */

		pm->pm_pcpu_state[cpu].pps_overflowcnt += 1;

		if (!PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
			continue;

		if (pm->pm_state != PMC_STATE_RUNNING)
			continue;

		error = pmc_process_interrupt(PMC_HR, pm, tf);
		if (error)
			arm64_stop_pmc(cpu, ri, pm);

		/* Reload sampling count */
		arm64_write_pmc(cpu, ri, pm, pm->pm_sc.pm_reloadcount);
	}

	return (retval);
}

static int
arm64_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d], illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < arm64_npmcs,
	    ("[arm64,%d] row-index %d out of range", __LINE__, ri));

	phw = &arm64_pcpu[cpu]->pc_arm64pmcs[ri];

	snprintf(pi->pm_name, sizeof(pi->pm_name), "ARMV8-%d", ri);
	pi->pm_class = PMC_CLASS_ARMV8;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc = NULL;
	}

	return (0);
}

static int
arm64_get_config(int cpu, int ri, struct pmc **ppm)
{

	*ppm = arm64_pcpu[cpu]->pc_arm64pmcs[ri].phw_pmc;

	return (0);
}

static int
arm64_pcpu_init(struct pmc_mdep *md, int cpu)
{
	struct arm64_cpu *pac;
	struct pmc_hw  *phw;
	struct pmc_cpu *pc;
	uint64_t pmcr;
	int first_ri;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[arm64,%d] wrong cpu number %d", __LINE__, cpu));
	PMCDBG0(MDP, INI, 1, "arm64-pcpu-init");

	arm64_pcpu[cpu] = pac = malloc(sizeof(struct arm64_cpu), M_PMC,
	    M_WAITOK | M_ZERO);

	pac->pc_arm64pmcs = malloc(sizeof(struct pmc_hw) * arm64_npmcs,
	    M_PMC, M_WAITOK | M_ZERO);
	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_ARMV8].pcd_ri;
	KASSERT(pc != NULL, ("[arm64,%d] NULL per-cpu pointer", __LINE__));

	for (i = 0, phw = pac->pc_arm64pmcs; i < arm64_npmcs; i++, phw++) {
		phw->phw_state    = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i);
		phw->phw_pmc      = NULL;
		pc->pc_hwpmcs[i + first_ri] = phw;
	}

	/*
	 * Disable all counters and overflow interrupts. Upon reset they are in
	 * an undefined state.
	 *
	 * Don't issue an isb here, just wait for the one in arm64_pmcr_write()
	 * to make the writes visible.
	 */
	WRITE_SPECIALREG(pmcntenclr_el0, 0xffffffff);
	WRITE_SPECIALREG(pmintenclr_el1, 0xffffffff);

	/* Enable unit with a useful default state */
	pmcr = PMCR_LC | PMCR_C | PMCR_P | PMCR_E;
	if (arm64_64bit_events)
		pmcr |= PMCR_LP;
	arm64_pmcr_write(pmcr);

	return (0);
}

static int
arm64_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	uint64_t pmcr;

	PMCDBG0(MDP, INI, 1, "arm64-pcpu-fini");

	pmcr = arm64_pmcr_read();
	pmcr &= ~PMCR_E;
	arm64_pmcr_write(pmcr);

	free(arm64_pcpu[cpu]->pc_arm64pmcs, M_PMC);
	free(arm64_pcpu[cpu], M_PMC);
	arm64_pcpu[cpu] = NULL;

	return (0);
}

struct pmc_mdep *
pmc_arm64_initialize(void)
{
	struct pmc_mdep *pmc_mdep;
	struct pmc_classdep *pcd;
	int classes, idcode, impcode;
	uint64_t dfr;
	uint64_t pmcr;
	uint64_t midr;

	pmcr = arm64_pmcr_read();
	arm64_npmcs = (pmcr & PMCR_N_MASK) >> PMCR_N_SHIFT;
	impcode = (pmcr & PMCR_IMP_MASK) >> PMCR_IMP_SHIFT;
	idcode = (pmcr & PMCR_IDCODE_MASK) >> PMCR_IDCODE_SHIFT;

	PMCDBG1(MDP, INI, 1, "arm64-init npmcs=%d", arm64_npmcs);

	/*
	 * Write the CPU model to kern.hwpmc.cpuid.
	 *
	 * We zero the variant and revision fields.
	 *
	 * TODO: how to handle differences between cores due to big.LITTLE?
	 * For now, just use MIDR from CPU 0.
	 */
	midr = (uint64_t)(pcpu_find(0)->pc_midr);
	midr &= ~(CPU_VAR_MASK | CPU_REV_MASK);
	snprintf(pmc_cpuid, sizeof(pmc_cpuid), "0x%016lx", midr);

	/* Check if we have 64-bit counters */
	if (get_kernel_reg(ID_AA64DFR0_EL1, &dfr)) {
		if (ID_AA64DFR0_PMUVer_VAL(dfr) >= ID_AA64DFR0_PMUVer_3_5)
			arm64_64bit_events = true;
	}

	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	arm64_pcpu = malloc(sizeof(struct arm64_cpu *) * pmc_cpu_max(),
		M_PMC, M_WAITOK | M_ZERO);

	/* One AArch64 CPU class */
	classes = 1;

#ifdef DEV_ACPI
	/* Query presence of optional classes and set max class. */
	if (pmc_cmn600_nclasses() > 0)
		classes = MAX(classes, PMC_MDEP_CLASS_INDEX_CMN600);
	if (pmc_dmc620_nclasses() > 0)
		classes = MAX(classes, PMC_MDEP_CLASS_INDEX_DMC620_C);
#endif

	pmc_mdep = pmc_mdep_alloc(classes);

	switch(impcode) {
	case PMCR_IMP_ARM:
		switch (idcode) {
		case PMCR_IDCODE_CORTEX_A76:
		case PMCR_IDCODE_NEOVERSE_N1:
			pmc_mdep->pmd_cputype = PMC_CPU_ARMV8_CORTEX_A76;
			break;
		case PMCR_IDCODE_CORTEX_A57:
		case PMCR_IDCODE_CORTEX_A72:
			pmc_mdep->pmd_cputype = PMC_CPU_ARMV8_CORTEX_A57;
			break;
		default:
		case PMCR_IDCODE_CORTEX_A53:
			pmc_mdep->pmd_cputype = PMC_CPU_ARMV8_CORTEX_A53;
			break;
		}
		break;
	default:
		pmc_mdep->pmd_cputype = PMC_CPU_ARMV8_CORTEX_A53;
		break;
	}

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_ARMV8];
	pcd->pcd_caps  = ARMV8_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_ARMV8;
	pcd->pcd_num   = arm64_npmcs;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 64;

	pcd->pcd_allocate_pmc   = arm64_allocate_pmc;
	pcd->pcd_config_pmc     = arm64_config_pmc;
	pcd->pcd_pcpu_fini      = arm64_pcpu_fini;
	pcd->pcd_pcpu_init      = arm64_pcpu_init;
	pcd->pcd_describe       = arm64_describe;
	pcd->pcd_get_config     = arm64_get_config;
	pcd->pcd_read_pmc       = arm64_read_pmc;
	pcd->pcd_release_pmc    = arm64_release_pmc;
	pcd->pcd_start_pmc      = arm64_start_pmc;
	pcd->pcd_stop_pmc       = arm64_stop_pmc;
	pcd->pcd_write_pmc      = arm64_write_pmc;

	pmc_mdep->pmd_intr = arm64_intr;
	pmc_mdep->pmd_npmc += arm64_npmcs;

#ifdef DEV_ACPI
	if (pmc_cmn600_nclasses() > 0)
		pmc_cmn600_initialize(pmc_mdep);
	if (pmc_dmc620_nclasses() > 0) {
		pmc_dmc620_initialize_cd2(pmc_mdep);
		pmc_dmc620_initialize_c(pmc_mdep);
	}
#endif

	return (pmc_mdep);
}

void
pmc_arm64_finalize(struct pmc_mdep *md)
{
	PMCDBG0(MDP, INI, 1, "arm64-finalize");

	for (int i = 0; i < pmc_cpu_max(); i++)
		KASSERT(arm64_pcpu[i] == NULL,
		    ("[arm64,%d] non-null pcpu cpu %d", __LINE__, i));

	free(arm64_pcpu, M_PMC);
}
