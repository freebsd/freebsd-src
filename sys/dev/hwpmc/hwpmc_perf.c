/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * AMD/Intel MPERF and APERF MSRs counters exposed as an hwpmc(4) PMC class.
 *
 * Read-only, system-scope (PMC_MODE_SC), 64-bit counters reporting
 * MPERF and APERF MSR values.
 */

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/priv.h>

#include <machine/specialreg.h>

#define PERF_CAPS	PMC_CAP_READ

struct perf_descr {
	struct pmc_descr pm_descr;  /* "base class" */
};

static const struct perf_descr perf_pmcdesc[PERF_NPMCS] = {
	{
		.pm_descr = {
			.pd_name  = "MPERF",
			.pd_class = PMC_CLASS_PERF,
			.pd_caps  = PERF_CAPS,
			.pd_width = 64
		},
	},
	{
		.pm_descr = {
			.pd_name  = "APERF",
			.pd_class = PMC_CLASS_PERF,
			.pd_caps  = PERF_CAPS,
			.pd_width = 64
		}
	}
};

struct perf_cpu {
	struct pmc_hw tc_hw[PERF_NPMCS];
};

static struct perf_cpu **perf_pcpu;
static int perf_classindex;

static int
perf_allocate_pmc(int cpu __diagused, int ri __diagused,
	    struct pmc *pm __unused, const struct pmc_op_pmcallocate *a)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS,
	    ("[perf,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != PMC_CLASS_PERF)
		return (EINVAL);

	if ((a->pm_ev < PMC_EV_PERF_FIRST || a->pm_ev > PMC_EV_PERF_LAST) ||
	    a->pm_mode != PMC_MODE_SC)
		return (EINVAL);

	/*
	 * Allows the PERF class to be accessed only by privileged users.
	 * Frequency is an indirect power proxy and could be abused by
	 * attacks like Hertzbleed.
	 */

	if (priv_check(curthread, PRIV_PMC_SYSTEM) != 0)
		return (EPERM);

	if ((a->pm_caps & PERF_CAPS) == 0)
		return (EINVAL);
	if ((a->pm_caps & ~PERF_CAPS) != 0)
		return (EPERM);

	switch (ri) {
	case PERF_MPERF:
		if (a->pm_ev != PMC_EV_PERF_MPERF)
			return (EINVAL);
		break;
	case PERF_APERF:
		if (a->pm_ev != PMC_EV_PERF_APERF)
			return (EINVAL);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
perf_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP, CFG, 1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	phw = &perf_pcpu[cpu]->tc_hw[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[perf,%d] pm=%p phw->pm=%p hwpmc not unconfigured", __LINE__,
	    pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return (0);
}

static int
perf_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	const struct perf_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	phw = &perf_pcpu[cpu]->tc_hw[ri];
	pd  = &perf_pmcdesc[ri];

	strlcpy(pi->pm_name, pd->pm_descr.pd_name, sizeof(pi->pm_name));
	pi->pm_class = pd->pm_descr.pd_class;

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
perf_get_config(int cpu, int ri, struct pmc **ppm)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	*ppm = perf_pcpu[cpu]->tc_hw[ri].phw_pmc;

	return (0);
}

static int
perf_get_msr(int ri __diagused, uint32_t *msr __unused)
{
	KASSERT(ri >= 0 && ri < PERF_NPMCS,
	    ("[perf,%d] ri %d out of range", __LINE__, ri));

	return (EINVAL);
}

static int
perf_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	int i, ri;
	struct pmc_cpu *pc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal cpu %d", __LINE__, cpu));
	KASSERT(perf_pcpu[cpu] != NULL, ("[perf,%d] null pcpu", __LINE__));

	free(perf_pcpu[cpu], M_PMC);
	perf_pcpu[cpu] = NULL;

	ri = md->pmd_classdep[perf_classindex].pcd_ri;
	pc = pmc_pcpu[cpu];

	for (i = 0; i < PERF_NPMCS; i++) {
		pc->pc_hwpmcs[i + ri] = NULL;
	}

	return (0);
}

static int
perf_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int i, ri;
	struct pmc_cpu *pc;
	struct perf_cpu *perf_pc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal cpu %d", __LINE__, cpu));
	KASSERT(perf_pcpu, ("[perf,%d] null pcpu", __LINE__));
	KASSERT(perf_pcpu[cpu] == NULL, ("[perf,%d] non-null per-cpu",
	    __LINE__));

	perf_pc = malloc(sizeof(struct perf_cpu), M_PMC, M_WAITOK | M_ZERO);

	perf_pcpu[cpu] = perf_pc;

	ri = md->pmd_classdep[perf_classindex].pcd_ri;

	KASSERT(pmc_pcpu, ("[perf,%d] null generic pcpu", __LINE__));

	pc = pmc_pcpu[cpu];

	KASSERT(pc, ("[perf,%d] null generic per-cpu", __LINE__));

	for (i = 0; i < PERF_NPMCS; i++) {
		perf_pc->tc_hw[i].phw_state = PMC_PHW_FLAG_IS_ENABLED |
			PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i) |
			PMC_PHW_FLAG_IS_SHAREABLE;
		pc->pc_hwpmcs[i + ri] = &perf_pc->tc_hw[i];
	}

	return (0);
}

static int
perf_read_pmc(int cpu __diagused, int ri, struct pmc *pm, pmc_value_t *v)
{
	enum pmc_mode mode __diagused;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal ri %d",
	    __LINE__, ri));

	mode = PMC_TO_MODE(pm);

	KASSERT(mode == PMC_MODE_SC,
		("[perf,%d] illegal pmc mode %d", __LINE__, mode));

	PMCDBG1(MDP, REA, 1, "perf-read id=%d", ri);

	switch (ri) {
	case PERF_MPERF:
		*v = rdmsr(MSR_MPERF);
		break;
	case PERF_APERF:
		*v = rdmsr(MSR_APERF);
		break;
	default:
		return (EINVAL);
	}

	return (0);
}

static int
perf_write_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused,
	    pmc_value_t v __unused)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	return (0);
}

static int
perf_release_pmc(int cpu __diagused, int ri __diagused, struct pmc *pmc __unused)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS,
	    ("[perf,%d] illegal row-index %d", __LINE__, ri));
	KASSERT(perf_pcpu[cpu]->tc_hw[ri].phw_pmc == NULL,
	   ("[perf,%d] PHW pmc non-NULL", __LINE__));

	return (0);
}

static int
perf_start_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	return (0);
}

static int
perf_stop_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[perf,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < PERF_NPMCS, ("[perf,%d] illegal row-index %d",
	    __LINE__, ri));

	return (0);
}

int
pmc_perf_initialize(struct pmc_mdep *md, int maxcpu, int classindex)
{
	struct pmc_classdep *pcd;

	KASSERT(md != NULL, ("[perf,%d] md is NULL", __LINE__));
	KASSERT(md->pmd_nclass >= 1, ("[perf,%d] dubious md->nclass %d",
	    __LINE__, md->pmd_nclass));

	if ((cpu_power_ecx & CPUID_PERF_STAT) && (tsc_perf_stat == 1)) {

		perf_pcpu = malloc(sizeof(struct perf_cpu *) * maxcpu, M_PMC,
			M_ZERO | M_WAITOK);

		perf_classindex = classindex;
		pcd = &md->pmd_classdep[classindex];

		pcd->pcd_caps   = PMC_CAP_READ;
		pcd->pcd_class  = PMC_CLASS_PERF;
		pcd->pcd_num    = PERF_NPMCS;
		pcd->pcd_ri	= md->pmd_npmc;
		pcd->pcd_width  = 64;

		pcd->pcd_allocate_pmc	= perf_allocate_pmc;
		pcd->pcd_config_pmc	= perf_config_pmc;
		pcd->pcd_describe	= perf_describe;
		pcd->pcd_get_config	= perf_get_config;
		pcd->pcd_get_msr	= perf_get_msr;
		pcd->pcd_pcpu_init	= perf_pcpu_init;
		pcd->pcd_pcpu_fini	= perf_pcpu_fini;
		pcd->pcd_read_pmc	= perf_read_pmc;
		pcd->pcd_write_pmc	= perf_write_pmc;
		pcd->pcd_release_pmc	= perf_release_pmc;
		pcd->pcd_start_pmc	= perf_start_pmc;
		pcd->pcd_stop_pmc	= perf_stop_pmc;

		md->pmd_npmc += PERF_NPMCS;
	}

	return (0);
}

void pmc_perf_finalize(struct pmc_mdep *md)
{
	PMCDBG0(MDP, INI, 1, "perf-finalize");

	if (perf_pcpu != NULL) {
		for (int i = 0; i < pmc_cpu_max(); i++)
			KASSERT(perf_pcpu[i] == NULL,
			    ("[perf,%d] non-null pcpu cpu %d", __LINE__, i));

		free(perf_pcpu, M_PMC);
		perf_pcpu = NULL;
	}
}
