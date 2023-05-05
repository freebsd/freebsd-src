/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2008 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * Copyright (c) 2021 Ampere Computing LLC
 *
 * Portions of this software were developed by A. Joseph Koshy under
 * sponsorship from the FreeBSD Foundation and Google, Inc.
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

/* Support for ARM DMC-620 Memory Controller PMU */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <dev/hwpmc/pmu_dmc620_reg.h>

#define	DMC620_TYPE_CLKDIV2	0
#define	DMC620_TYPE_CLK		1
#define CLASS2TYPE(c)	((c) - PMC_CLASS_DMC620_PMU_CD2)

/* Create wrapper for each class. */
#define	CLASSDEP_FN2(fn, t1, a1, t2, a2)				\
	static int fn(int class, t1 a1, t2 a2);				\
	static int fn ## _cd2(t1 a1, t2 a2)				\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_CD2, a1, a2));		\
	}								\
	static int fn ## _c(t1 a1, t2 a2)				\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_C, a1, a2));		\
	}								\
	static int fn(int class, t1 a1, t2 a2)

#define	CLASSDEP_FN3(fn, t1, a1, t2, a2, t3, a3)			\
	static int fn(int class, t1 a1, t2 a2, t3 a3);			\
	static int fn ## _cd2(t1 a1, t2 a2, t3 a3)			\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_CD2, a1, a2, a3));	\
	}								\
	static int fn ## _c(t1 a1, t2 a2, t3 a3)			\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_C, a1, a2, a3));	\
	}								\
	static int fn(int class, t1 a1, t2 a2, t3 a3)

#define	CLASSDEP_FN4(fn, t1, a1, t2, a2, t3, a3, t4, a4)		\
	static int fn(int class, t1 a1, t2 a2, t3 a3, t4 a4);		\
	static int fn ## _cd2(t1 a1, t2 a2, t3 a3, t4 a4)		\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_CD2, a1, a2, a3, a4));	\
	}								\
	static int fn ## _c(t1 a1, t2 a2, t3 a3, t4 a4)			\
	{								\
		return (fn(PMC_CLASS_DMC620_PMU_C, a1, a2, a3, a4));	\
	}								\
	static int fn(int class, t1 a1, t2 a2, t3 a3, t4 a4)

struct dmc620_pmc {
	void	*arg;
	int	domain;
};

struct dmc620_descr {
	struct pmc_descr pd_descr;  /* "base class" */
	void		*pd_rw_arg; /* Argument to use with read/write */
	struct pmc	*pd_pmc;
	struct pmc_hw	*pd_phw;
	uint32_t	pd_config;
	uint32_t	pd_match;
	uint32_t	pd_mask;
	uint32_t	pd_evsel;   /* address of EVSEL register */
	uint32_t	pd_perfctr; /* address of PERFCTR register */

};

static struct dmc620_descr **dmc620_pmcdesc[2];
static struct dmc620_pmc dmc620_pmcs[DMC620_UNIT_MAX];
static int dmc620_npmcs = 0;

void
dmc620_pmc_register(int unit, void *arg, int domain)
{

	if (unit >= DMC620_UNIT_MAX) {
		/* TODO */
		return;
	}

	dmc620_pmcs[unit].arg = arg;
	dmc620_pmcs[unit].domain = domain;
	dmc620_npmcs++;
}

void
dmc620_pmc_unregister(int unit)
{

	dmc620_pmcs[unit].arg = NULL;
	dmc620_npmcs--;
}

int
pmc_dmc620_nclasses(void)
{

	if (dmc620_npmcs > 0)
		return (2);
	return (0);
}

static inline struct dmc620_descr *
dmc620desc(int class, int cpu, int ri)
{
	int c;

	c = CLASS2TYPE(class);
	KASSERT((c & 0xfffffffe) == 0, ("[dmc620,%d] 'c' can only be 0 or 1. "
	    "now %d", __LINE__, c));

	return (dmc620_pmcdesc[c][ri]);
}

static inline int
cntr(int class, int ri)
{
	int c;

	c = CLASS2TYPE(class);
	KASSERT((c & 0xfffffffe) == 0, ("[dmc620,%d] 'c' can only be 0 or 1. "
	    "now %d", __LINE__, c));

	if (c == DMC620_TYPE_CLKDIV2)
		return (ri % DMC620_CLKDIV2_COUNTERS_N);
	return ((ri % DMC620_CLK_COUNTERS_N) + DMC620_CLKDIV2_COUNTERS_N);
}

static inline int
class2mdep(int class)
{

	switch (class) {
	case PMC_CLASS_DMC620_PMU_CD2:
		return (PMC_MDEP_CLASS_INDEX_DMC620_CD2);
	case PMC_CLASS_DMC620_PMU_C:
		return (PMC_MDEP_CLASS_INDEX_DMC620_C);
	}
	return (-1);
}

static inline int
class_ri2unit(int class, int ri)
{

	if (class == PMC_CLASS_DMC620_PMU_CD2)
		return (ri / DMC620_CLKDIV2_COUNTERS_N);
	else
		return (ri / DMC620_CLK_COUNTERS_N);
}

/*
 * read a pmc register
 */

CLASSDEP_FN4(dmc620_read_pmc, int, cpu, int, ri, struct pmc *, pm,
    pmc_value_t *, v)
{
	struct dmc620_descr *desc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = dmc620desc(class, cpu, ri);

	PMCDBG3(MDP,REA,1,"%s id=%d class=%d", __func__, ri, class);

	/*
	 * Should emulate 64bits, because 32 bits counter overflows faster than
	 * pmcstat default period.
	 */
	/* Always CPU0. Single controller for all CPUs. */
	*v = ((uint64_t)pm->pm_pcpu_state[0].pps_overflowcnt << 32) |
	    pmu_dmc620_rd4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_VALUE_LO);

	PMCDBG3(MDP, REA, 2, "%s id=%d -> %jd", __func__, ri, *v);

	return (0);
}

/*
 * Write a pmc register.
 */

CLASSDEP_FN4(dmc620_write_pmc, int, cpu, int, ri, struct pmc *, pm,
    pmc_value_t, v)
{
	struct dmc620_descr *desc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = dmc620desc(class, cpu, ri);

	PMCDBG4(MDP, WRI, 1, "%s cpu=%d ri=%d v=%jx", __func__, cpu, ri, v);

	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_VALUE_LO, v);
	return (0);
}

/*
 * configure hardware pmc according to the configuration recorded in
 * pmc 'pm'.
 */

CLASSDEP_FN3(dmc620_config_pmc, int, cpu, int, ri, struct pmc *, pm)
{
	struct pmc_hw *phw;

	PMCDBG4(MDP, CFG, 1, "%s cpu=%d ri=%d pm=%p", __func__, cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	phw = dmc620desc(class, cpu, ri)->pd_phw;

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[dmc620,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
		__LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;
	return (0);
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */

CLASSDEP_FN3(dmc620_get_config, int, cpu, int, ri, struct pmc **, ppm)
{

	*ppm = dmc620desc(class, cpu, ri)->pd_phw->phw_pmc;

	return (0);
}

/*
 * Check if a given allocation is feasible.
 */

CLASSDEP_FN4(dmc620_allocate_pmc, int, cpu, int, ri, struct pmc *,pm,
    const struct pmc_op_pmcallocate *, a)
{
	const struct pmc_descr *pd;
	uint64_t caps, control;
	enum pmc_event pe;
	uint8_t e;

	(void) cpu;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	pd = &dmc620desc(class, cpu, ri)->pd_descr;
	if (dmc620_pmcs[class_ri2unit(class, ri)].domain !=
	    pcpu_find(cpu)->pc_domain)
		return (EINVAL);

	/* check class match */
	if (pd->pd_class != a->pm_class)
		return (EINVAL);

	caps = pm->pm_caps;

	PMCDBG3(MDP, ALL, 1, "%s ri=%d caps=0x%x", __func__, ri, caps);

	pe = a->pm_ev;
	if (class == PMC_CLASS_DMC620_PMU_CD2)
		e = pe - PMC_EV_DMC620_PMU_CD2_FIRST;
	else
		e = pe - PMC_EV_DMC620_PMU_C_FIRST;

	control = (e << DMC620_COUNTER_CONTROL_EVENT_SHIFT) &
	    DMC620_COUNTER_CONTROL_EVENT_MASK;

	if (caps & PMC_CAP_INVERT)
		control |= DMC620_COUNTER_CONTROL_INVERT;

	pm->pm_md.pm_dmc620.pm_control = control;
	pm->pm_md.pm_dmc620.pm_match = a->pm_md.pm_dmc620.pm_dmc620_match;
	pm->pm_md.pm_dmc620.pm_mask = a->pm_md.pm_dmc620.pm_dmc620_mask;

	PMCDBG3(MDP, ALL, 2, "%s ri=%d -> control=0x%x", __func__, ri, control);

	return (0);
}

/*
 * Release machine dependent state associated with a PMC.  This is a
 * no-op on this architecture.
 *
 */

/* ARGSUSED0 */
CLASSDEP_FN3(dmc620_release_pmc, int, cpu, int, ri, struct pmc *, pmc)
{
	struct pmc_hw *phw __diagused;

	(void) pmc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	phw = dmc620desc(class, cpu, ri)->pd_phw;

	KASSERT(phw->phw_pmc == NULL,
	    ("[dmc620,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return (0);
}

/*
 * start a PMC.
 */

CLASSDEP_FN3(dmc620_start_pmc, int, cpu, int, ri, struct pmc *, pm)
{
	struct dmc620_descr *desc;
	uint64_t control;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = dmc620desc(class, cpu, ri);

	PMCDBG3(MDP, STA, 1, "%s cpu=%d ri=%d", __func__, cpu, ri);

	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_MASK_LO, pm->pm_md.pm_dmc620.pm_mask & 0xffffffff);
	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_MASK_HI, pm->pm_md.pm_dmc620.pm_mask >> 32);
	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_MATCH_LO, pm->pm_md.pm_dmc620.pm_match & 0xffffffff);
	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_MATCH_HI, pm->pm_md.pm_dmc620.pm_match >> 32);
	/* turn on the PMC ENABLE bit */
	control = pm->pm_md.pm_dmc620.pm_control | DMC620_COUNTER_CONTROL_ENABLE;

	PMCDBG2(MDP, STA, 2, "%s control=0x%x", __func__, control);

	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_CONTROL, control);
	return (0);
}

/*
 * Stop a PMC.
 */

CLASSDEP_FN3(dmc620_stop_pmc, int, cpu, int, ri, struct pmc *, pm)
{
	struct dmc620_descr *desc;
	uint64_t control;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	desc = dmc620desc(class, cpu, ri);

	PMCDBG2(MDP, STO, 1, "%s ri=%d", __func__, ri);

	/* turn off the PMC ENABLE bit */
	control = pm->pm_md.pm_dmc620.pm_control & ~DMC620_COUNTER_CONTROL_ENABLE;
	pmu_dmc620_wr4(desc->pd_rw_arg, cntr(class, ri),
	    DMC620_COUNTER_CONTROL, control);

	return (0);
}

/*
 * describe a PMC
 */
CLASSDEP_FN4(dmc620_describe, int, cpu, int, ri, struct pmc_info *, pi,
    struct pmc **, ppmc)
{
	struct pmc_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0, ("[dmc620,%d] row-index %d out of range", __LINE__,
	    ri));

	phw = dmc620desc(class, cpu, ri)->pd_phw;
	pd = &dmc620desc(class, cpu, ri)->pd_descr;

	strlcpy(pi->pm_name, pd->pd_name, sizeof(pi->pm_name));
	pi->pm_class = pd->pd_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return (0);
}

/*
 * processor dependent initialization.
 */

CLASSDEP_FN2(dmc620_pcpu_init, struct pmc_mdep *, md, int, cpu)
{
	int first_ri, n, npmc;
	struct pmc_hw  *phw;
	struct pmc_cpu *pc;
	int mdep_class;

	mdep_class = class2mdep(class);
	KASSERT(mdep_class != -1, ("[dmc620,%d] wrong class %d", __LINE__,
	    class));
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG1(MDP, INI, 1, "dmc620-init cpu=%d", cpu);

	/*
	 * Set the content of the hardware descriptors to a known
	 * state and initialize pointers in the MI per-cpu descriptor.
	 */

	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[mdep_class].pcd_ri;
	npmc = md->pmd_classdep[mdep_class].pcd_num;

	for (n = 0; n < npmc; n++, phw++) {
		phw = dmc620desc(class, cpu, n)->pd_phw;
		phw->phw_state = PMC_PHW_CPU_TO_STATE(cpu) |
		    PMC_PHW_INDEX_TO_STATE(n);
		/* Set enabled only if unit present. */
		if (dmc620_pmcs[class_ri2unit(class, n)].arg != NULL)
			phw->phw_state |= PMC_PHW_FLAG_IS_ENABLED;
		phw->phw_pmc = NULL;
		pc->pc_hwpmcs[n + first_ri] = phw;
	}
	return (0);
}

/*
 * processor dependent cleanup prior to the KLD
 * being unloaded
 */

CLASSDEP_FN2(dmc620_pcpu_fini, struct pmc_mdep *, md, int, cpu)
{

	return (0);
}

int
dmc620_intr(struct trapframe *tf, int class, int unit, int i)
{
	struct pmc_cpu *pc __diagused;
	struct pmc_hw *phw;
	struct pmc *pm;
	int error, cpu, ri;

	ri = i + unit * ((class == PMC_CLASS_DMC620_PMU_CD2) ?
	    DMC620_CLKDIV2_COUNTERS_N : DMC620_CLK_COUNTERS_N);
	cpu = curcpu;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[dmc620,%d] CPU %d out of range", __LINE__, cpu));
	pc = pmc_pcpu[cpu];
	KASSERT(pc != NULL, ("pc != NULL"));

	phw = dmc620desc(class, cpu, ri)->pd_phw;
	KASSERT(phw != NULL, ("phw != NULL"));
	pm  = phw->phw_pmc;
	if (pm == NULL)
		return (0);

	if (!PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
		/* Always CPU0. */
		pm->pm_pcpu_state[0].pps_overflowcnt += 1;
		return (0);
	}

	if (pm->pm_state != PMC_STATE_RUNNING)
		return (0);

	error = pmc_process_interrupt(PMC_HR, pm, tf);
	if (error)
		dmc620_stop_pmc(class, cpu, ri, pm);

	/* Reload sampling count */
	dmc620_write_pmc(class, cpu, ri, pm, pm->pm_sc.pm_reloadcount);

	return (0);
}

/*
 * Initialize ourselves.
 */

int
pmc_dmc620_initialize_cd2(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc, unit;

	KASSERT(md != NULL, ("[dmc620,%d] md is NULL", __LINE__));
	KASSERT(dmc620_npmcs <= DMC620_UNIT_MAX,
	    ("[dmc620,%d] dmc620_npmcs too big", __LINE__));

	PMCDBG0(MDP,INI,1, "dmc620-initialize");

	npmc = DMC620_CLKDIV2_COUNTERS_N * dmc620_npmcs;
	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_CD2];

	pcd->pcd_caps		= PMC_CAP_SYSTEM | PMC_CAP_READ |
	    PMC_CAP_WRITE | PMC_CAP_INVERT | PMC_CAP_QUALIFIER |
	    PMC_CAP_INTERRUPT | PMC_CAP_DOMWIDE;
	pcd->pcd_class	= PMC_CLASS_DMC620_PMU_CD2;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= 32;

	pcd->pcd_allocate_pmc	= dmc620_allocate_pmc_cd2;
	pcd->pcd_config_pmc	= dmc620_config_pmc_cd2;
	pcd->pcd_describe	= dmc620_describe_cd2;
	pcd->pcd_get_config	= dmc620_get_config_cd2;
	pcd->pcd_get_msr	= NULL;
	pcd->pcd_pcpu_fini	= dmc620_pcpu_fini_cd2;
	pcd->pcd_pcpu_init	= dmc620_pcpu_init_cd2;
	pcd->pcd_read_pmc	= dmc620_read_pmc_cd2;
	pcd->pcd_release_pmc	= dmc620_release_pmc_cd2;
	pcd->pcd_start_pmc	= dmc620_start_pmc_cd2;
	pcd->pcd_stop_pmc	= dmc620_stop_pmc_cd2;
	pcd->pcd_write_pmc	= dmc620_write_pmc_cd2;

	md->pmd_npmc	       += npmc;
	dmc620_pmcdesc[0] = malloc(sizeof(struct dmc620_descr *) * npmc *
	    DMC620_PMU_DEFAULT_UNITS_N, M_PMC, M_WAITOK|M_ZERO);
	for (i = 0; i < npmc; i++) {
		dmc620_pmcdesc[0][i] = malloc(sizeof(struct dmc620_descr),
		    M_PMC, M_WAITOK|M_ZERO);

		unit = i / DMC620_CLKDIV2_COUNTERS_N;
		KASSERT(unit >= 0, ("unit >= 0"));
		KASSERT(dmc620_pmcs[unit].arg != NULL, ("arg != NULL"));

		dmc620_pmcdesc[0][i]->pd_rw_arg = dmc620_pmcs[unit].arg;
		dmc620_pmcdesc[0][i]->pd_descr.pd_class =
		    PMC_CLASS_DMC620_PMU_CD2;
		dmc620_pmcdesc[0][i]->pd_descr.pd_caps = pcd->pcd_caps;
		dmc620_pmcdesc[0][i]->pd_phw = malloc(sizeof(struct pmc_hw),
		    M_PMC, M_WAITOK|M_ZERO);
		snprintf(dmc620_pmcdesc[0][i]->pd_descr.pd_name, 63,
		    "DMC620_CD2_%d", i);
	}

	return (0);
}

int
pmc_dmc620_initialize_c(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc, unit;

	KASSERT(md != NULL, ("[dmc620,%d] md is NULL", __LINE__));
	KASSERT(dmc620_npmcs <= DMC620_UNIT_MAX,
	    ("[dmc620,%d] dmc620_npmcs too big", __LINE__));

	PMCDBG0(MDP,INI,1, "dmc620-initialize");

	npmc = DMC620_CLK_COUNTERS_N * dmc620_npmcs;
	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_C];

	pcd->pcd_caps		= PMC_CAP_SYSTEM | PMC_CAP_READ |
	    PMC_CAP_WRITE | PMC_CAP_INVERT | PMC_CAP_QUALIFIER |
	    PMC_CAP_INTERRUPT | PMC_CAP_DOMWIDE;
	pcd->pcd_class	= PMC_CLASS_DMC620_PMU_C;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= 32;

	pcd->pcd_allocate_pmc	= dmc620_allocate_pmc_c;
	pcd->pcd_config_pmc	= dmc620_config_pmc_c;
	pcd->pcd_describe	= dmc620_describe_c;
	pcd->pcd_get_config	= dmc620_get_config_c;
	pcd->pcd_get_msr	= NULL;
	pcd->pcd_pcpu_fini	= dmc620_pcpu_fini_c;
	pcd->pcd_pcpu_init	= dmc620_pcpu_init_c;
	pcd->pcd_read_pmc	= dmc620_read_pmc_c;
	pcd->pcd_release_pmc	= dmc620_release_pmc_c;
	pcd->pcd_start_pmc	= dmc620_start_pmc_c;
	pcd->pcd_stop_pmc	= dmc620_stop_pmc_c;
	pcd->pcd_write_pmc	= dmc620_write_pmc_c;

	md->pmd_npmc	       += npmc;
	dmc620_pmcdesc[1] = malloc(sizeof(struct dmc620_descr *) * npmc *
	    DMC620_PMU_DEFAULT_UNITS_N, M_PMC, M_WAITOK|M_ZERO);
	for (i = 0; i < npmc; i++) {
		dmc620_pmcdesc[1][i] = malloc(sizeof(struct dmc620_descr),
		    M_PMC, M_WAITOK|M_ZERO);

		unit = i / DMC620_CLK_COUNTERS_N;
		KASSERT(unit >= 0, ("unit >= 0"));
		KASSERT(dmc620_pmcs[unit].arg != NULL, ("arg != NULL"));

		dmc620_pmcdesc[1][i]->pd_rw_arg = dmc620_pmcs[unit].arg;
		dmc620_pmcdesc[1][i]->pd_descr.pd_class = PMC_CLASS_DMC620_PMU_C;
		dmc620_pmcdesc[1][i]->pd_descr.pd_caps = pcd->pcd_caps;
		dmc620_pmcdesc[1][i]->pd_phw = malloc(sizeof(struct pmc_hw),
		    M_PMC, M_WAITOK|M_ZERO);
		snprintf(dmc620_pmcdesc[1][i]->pd_descr.pd_name, 63,
		    "DMC620_C_%d", i);
	}

	return (0);
}

void
pmc_dmc620_finalize_cd2(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc;

	KASSERT(md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_CD2].pcd_class ==
	    PMC_CLASS_DMC620_PMU_CD2, ("[dmc620,%d] pmc class mismatch",
	    __LINE__));

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_CD2];

	npmc = pcd->pcd_num;
	for (i = 0; i < npmc; i++) {
		free(dmc620_pmcdesc[0][i]->pd_phw, M_PMC);
		free(dmc620_pmcdesc[0][i], M_PMC);
	}
	free(dmc620_pmcdesc[0], M_PMC);
	dmc620_pmcdesc[0] = NULL;
}

void
pmc_dmc620_finalize_c(struct pmc_mdep *md)
{
	struct pmc_classdep *pcd;
	int i, npmc;

	KASSERT(md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_C].pcd_class ==
	    PMC_CLASS_DMC620_PMU_C, ("[dmc620,%d] pmc class mismatch",
	    __LINE__));

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_DMC620_C];

	npmc = pcd->pcd_num;
	for (i = 0; i < npmc; i++) {
		free(dmc620_pmcdesc[1][i]->pd_phw, M_PMC);
		free(dmc620_pmcdesc[1][i], M_PMC);
	}
	free(dmc620_pmcdesc[1], M_PMC);
	dmc620_pmcdesc[1] = NULL;
}
