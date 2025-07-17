/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003-2008 Joseph Koshy
 * Copyright (c) 2007 The FreeBSD Foundation
 * All rights reserved.
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

/* Support for the AMD K8 and later processors */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pcpu.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#define	OVERFLOW_WAIT_COUNT	50

DPCPU_DEFINE_STATIC(uint32_t, nmi_counter);

/* AMD K8 PMCs */
struct amd_descr {
	struct pmc_descr pm_descr;  /* "base class" */
	uint32_t	pm_evsel;   /* address of EVSEL register */
	uint32_t	pm_perfctr; /* address of PERFCTR register */
};

/* Counter hardware. */
#define	PMCDESC(evsel, perfctr)						\
	{								\
		.pm_descr = {						\
			.pd_name  = "",					\
			.pd_class = PMC_CLASS_K8,			\
			.pd_caps  = AMD_PMC_CAPS,			\
			.pd_width = 48					\
		},							\
		.pm_evsel   = (evsel),					\
		.pm_perfctr = (perfctr)					\
	}

static struct amd_descr amd_pmcdesc[AMD_NPMCS] =
{
	PMCDESC(AMD_PMC_EVSEL_0,	AMD_PMC_PERFCTR_0),
	PMCDESC(AMD_PMC_EVSEL_1,	AMD_PMC_PERFCTR_1),
	PMCDESC(AMD_PMC_EVSEL_2,	AMD_PMC_PERFCTR_2),
	PMCDESC(AMD_PMC_EVSEL_3,	AMD_PMC_PERFCTR_3),
	PMCDESC(AMD_PMC_EVSEL_4,	AMD_PMC_PERFCTR_4),
	PMCDESC(AMD_PMC_EVSEL_5,	AMD_PMC_PERFCTR_5),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_0,	AMD_PMC_PERFCTR_EP_L3_0),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_1,	AMD_PMC_PERFCTR_EP_L3_1),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_2,	AMD_PMC_PERFCTR_EP_L3_2),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_3,	AMD_PMC_PERFCTR_EP_L3_3),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_4,	AMD_PMC_PERFCTR_EP_L3_4),
	PMCDESC(AMD_PMC_EVSEL_EP_L3_5,	AMD_PMC_PERFCTR_EP_L3_5),
	PMCDESC(AMD_PMC_EVSEL_EP_DF_0,	AMD_PMC_PERFCTR_EP_DF_0),
	PMCDESC(AMD_PMC_EVSEL_EP_DF_1,	AMD_PMC_PERFCTR_EP_DF_1),
	PMCDESC(AMD_PMC_EVSEL_EP_DF_2,	AMD_PMC_PERFCTR_EP_DF_2),
	PMCDESC(AMD_PMC_EVSEL_EP_DF_3,	AMD_PMC_PERFCTR_EP_DF_3)
};

struct amd_event_code_map {
	enum pmc_event	pe_ev;	 /* enum value */
	uint16_t	pe_code; /* encoded event mask */
	uint8_t		pe_mask; /* bits allowed in unit mask */
};

const struct amd_event_code_map amd_event_codes[] = {
	{ PMC_EV_K8_FP_DISPATCHED_FPU_OPS,		0x00, 0x3F },
	{ PMC_EV_K8_FP_CYCLES_WITH_NO_FPU_OPS_RETIRED,	0x01, 0x00 },
	{ PMC_EV_K8_FP_DISPATCHED_FPU_FAST_FLAG_OPS,	0x02, 0x00 },

	{ PMC_EV_K8_LS_SEGMENT_REGISTER_LOAD, 		0x20, 0x7F },
	{ PMC_EV_K8_LS_MICROARCHITECTURAL_RESYNC_BY_SELF_MODIFYING_CODE,
	  						0x21, 0x00 },
	{ PMC_EV_K8_LS_MICROARCHITECTURAL_RESYNC_BY_SNOOP, 0x22, 0x00 },
	{ PMC_EV_K8_LS_BUFFER2_FULL,			0x23, 0x00 },
	{ PMC_EV_K8_LS_LOCKED_OPERATION,		0x24, 0x07 },
	{ PMC_EV_K8_LS_MICROARCHITECTURAL_LATE_CANCEL,	0x25, 0x00 },
	{ PMC_EV_K8_LS_RETIRED_CFLUSH_INSTRUCTIONS,	0x26, 0x00 },
	{ PMC_EV_K8_LS_RETIRED_CPUID_INSTRUCTIONS,	0x27, 0x00 },

	{ PMC_EV_K8_DC_ACCESS,				0x40, 0x00 },
	{ PMC_EV_K8_DC_MISS,				0x41, 0x00 },
	{ PMC_EV_K8_DC_REFILL_FROM_L2,			0x42, 0x1F },
	{ PMC_EV_K8_DC_REFILL_FROM_SYSTEM,		0x43, 0x1F },
	{ PMC_EV_K8_DC_COPYBACK,			0x44, 0x1F },
	{ PMC_EV_K8_DC_L1_DTLB_MISS_AND_L2_DTLB_HIT,	0x45, 0x00 },
	{ PMC_EV_K8_DC_L1_DTLB_MISS_AND_L2_DTLB_MISS,	0x46, 0x00 },
	{ PMC_EV_K8_DC_MISALIGNED_DATA_REFERENCE,	0x47, 0x00 },
	{ PMC_EV_K8_DC_MICROARCHITECTURAL_LATE_CANCEL,	0x48, 0x00 },
	{ PMC_EV_K8_DC_MICROARCHITECTURAL_EARLY_CANCEL, 0x49, 0x00 },
	{ PMC_EV_K8_DC_ONE_BIT_ECC_ERROR,		0x4A, 0x03 },
	{ PMC_EV_K8_DC_DISPATCHED_PREFETCH_INSTRUCTIONS, 0x4B, 0x07 },
	{ PMC_EV_K8_DC_DCACHE_ACCESSES_BY_LOCKS,	0x4C, 0x03 },

	{ PMC_EV_K8_BU_CPU_CLK_UNHALTED,		0x76, 0x00 },
	{ PMC_EV_K8_BU_INTERNAL_L2_REQUEST,		0x7D, 0x1F },
	{ PMC_EV_K8_BU_FILL_REQUEST_L2_MISS,		0x7E, 0x07 },
	{ PMC_EV_K8_BU_FILL_INTO_L2,			0x7F, 0x03 },

	{ PMC_EV_K8_IC_FETCH,				0x80, 0x00 },
	{ PMC_EV_K8_IC_MISS,				0x81, 0x00 },
	{ PMC_EV_K8_IC_REFILL_FROM_L2,			0x82, 0x00 },
	{ PMC_EV_K8_IC_REFILL_FROM_SYSTEM,		0x83, 0x00 },
	{ PMC_EV_K8_IC_L1_ITLB_MISS_AND_L2_ITLB_HIT,	0x84, 0x00 },
	{ PMC_EV_K8_IC_L1_ITLB_MISS_AND_L2_ITLB_MISS,	0x85, 0x00 },
	{ PMC_EV_K8_IC_MICROARCHITECTURAL_RESYNC_BY_SNOOP, 0x86, 0x00 },
	{ PMC_EV_K8_IC_INSTRUCTION_FETCH_STALL,		0x87, 0x00 },
	{ PMC_EV_K8_IC_RETURN_STACK_HIT,		0x88, 0x00 },
	{ PMC_EV_K8_IC_RETURN_STACK_OVERFLOW,		0x89, 0x00 },

	{ PMC_EV_K8_FR_RETIRED_X86_INSTRUCTIONS,	0xC0, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_UOPS,			0xC1, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_BRANCHES,		0xC2, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_BRANCHES_MISPREDICTED,	0xC3, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_TAKEN_BRANCHES,		0xC4, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_TAKEN_BRANCHES_MISPREDICTED, 0xC5, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_FAR_CONTROL_TRANSFERS,	0xC6, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_RESYNCS,			0xC7, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_NEAR_RETURNS,		0xC8, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_NEAR_RETURNS_MISPREDICTED, 0xC9, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_TAKEN_BRANCHES_MISPREDICTED_BY_ADDR_MISCOMPARE,
							0xCA, 0x00 },
	{ PMC_EV_K8_FR_RETIRED_FPU_INSTRUCTIONS,	0xCB, 0x0F },
	{ PMC_EV_K8_FR_RETIRED_FASTPATH_DOUBLE_OP_INSTRUCTIONS,
							0xCC, 0x07 },
	{ PMC_EV_K8_FR_INTERRUPTS_MASKED_CYCLES,	0xCD, 0x00 },
	{ PMC_EV_K8_FR_INTERRUPTS_MASKED_WHILE_PENDING_CYCLES, 0xCE, 0x00 },
	{ PMC_EV_K8_FR_TAKEN_HARDWARE_INTERRUPTS,	0xCF, 0x00 },

	{ PMC_EV_K8_FR_DECODER_EMPTY,			0xD0, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALLS,			0xD1, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_FROM_BRANCH_ABORT_TO_RETIRE,
							0xD2, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_FOR_SERIALIZATION, 0xD3, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_FOR_SEGMENT_LOAD,	0xD4, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_REORDER_BUFFER_IS_FULL,
							0xD5, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_RESERVATION_STATIONS_ARE_FULL,
							0xD6, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_FPU_IS_FULL,	0xD7, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_LS_IS_FULL,	0xD8, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_WAITING_FOR_ALL_TO_BE_QUIET,
							0xD9, 0x00 },
	{ PMC_EV_K8_FR_DISPATCH_STALL_WHEN_FAR_XFER_OR_RESYNC_BRANCH_PENDING,
							0xDA, 0x00 },
	{ PMC_EV_K8_FR_FPU_EXCEPTIONS,			0xDB, 0x0F },
	{ PMC_EV_K8_FR_NUMBER_OF_BREAKPOINTS_FOR_DR0,	0xDC, 0x00 },
	{ PMC_EV_K8_FR_NUMBER_OF_BREAKPOINTS_FOR_DR1,	0xDD, 0x00 },
	{ PMC_EV_K8_FR_NUMBER_OF_BREAKPOINTS_FOR_DR2,	0xDE, 0x00 },
	{ PMC_EV_K8_FR_NUMBER_OF_BREAKPOINTS_FOR_DR3,	0xDF, 0x00 },

	{ PMC_EV_K8_NB_MEMORY_CONTROLLER_PAGE_ACCESS_EVENT, 0xE0, 0x7 },
	{ PMC_EV_K8_NB_MEMORY_CONTROLLER_PAGE_TABLE_OVERFLOW, 0xE1, 0x00 },
	{ PMC_EV_K8_NB_MEMORY_CONTROLLER_DRAM_COMMAND_SLOTS_MISSED,
							0xE2, 0x00 },
	{ PMC_EV_K8_NB_MEMORY_CONTROLLER_TURNAROUND,	0xE3, 0x07 },
	{ PMC_EV_K8_NB_MEMORY_CONTROLLER_BYPASS_SATURATION, 0xE4, 0x0F },
	{ PMC_EV_K8_NB_SIZED_COMMANDS,			0xEB, 0x7F },
	{ PMC_EV_K8_NB_PROBE_RESULT,			0xEC, 0x0F },
	{ PMC_EV_K8_NB_HT_BUS0_BANDWIDTH,		0xF6, 0x0F },
	{ PMC_EV_K8_NB_HT_BUS1_BANDWIDTH,		0xF7, 0x0F },
	{ PMC_EV_K8_NB_HT_BUS2_BANDWIDTH,		0xF8, 0x0F }

};

const int amd_event_codes_size = nitems(amd_event_codes);

/*
 * Per-processor information
 */
struct amd_cpu {
	struct pmc_hw	pc_amdpmcs[AMD_NPMCS];
};
static struct amd_cpu **amd_pcpu;

/*
 * Read a PMC value from the MSR.
 */
static int
amd_read_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t *v)
{
	const struct amd_descr *pd;
	pmc_value_t tmp;
	enum pmc_mode mode;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));
	KASSERT(amd_pcpu[cpu],
	    ("[amd,%d] null per-cpu, cpu %d", __LINE__, cpu));

	pd = &amd_pmcdesc[ri];
	mode = PMC_TO_MODE(pm);

	PMCDBG2(MDP, REA, 1, "amd-read id=%d class=%d", ri,
	    pd->pm_descr.pd_class);

	tmp = rdmsr(pd->pm_perfctr); /* RDMSR serializes */
	PMCDBG2(MDP, REA, 2, "amd-read (pre-munge) id=%d -> %jd", ri, tmp);
	if (PMC_IS_SAMPLING_MODE(mode)) {
		/*
		 * Clamp value to 0 if the counter just overflowed,
		 * otherwise the returned reload count would wrap to a
		 * huge value.
		 */
		if ((tmp & (1ULL << 47)) == 0)
			tmp = 0;
		else {
			/* Sign extend 48 bit value to 64 bits. */
			tmp = (pmc_value_t) ((int64_t)(tmp << 16) >> 16);
			tmp = AMD_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
		}
	}
	*v = tmp;

	PMCDBG2(MDP, REA, 2, "amd-read (post-munge) id=%d -> %jd", ri, *v);

	return (0);
}

/*
 * Write a PMC MSR.
 */
static int
amd_write_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t v)
{
	const struct amd_descr *pd;
	enum pmc_mode mode;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	pd = &amd_pmcdesc[ri];
	mode = PMC_TO_MODE(pm);

	/* use 2's complement of the count for sampling mode PMCs */
	if (PMC_IS_SAMPLING_MODE(mode))
		v = AMD_RELOAD_COUNT_TO_PERFCTR_VALUE(v);

	PMCDBG3(MDP, WRI, 1, "amd-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	/* write the PMC value */
	wrmsr(pd->pm_perfctr, v);
	return (0);
}

/*
 * Configure hardware PMC according to the configuration recorded in 'pm'.
 */
static int
amd_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP, CFG, 1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = &amd_pcpu[cpu]->pc_amdpmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[amd,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
		__LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;
	return (0);
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */
static int
amd_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = amd_pcpu[cpu]->pc_amdpmcs[ri].phw_pmc;
	return (0);
}

/*
 * Machine-dependent actions taken during the context switch in of a
 * thread.
 */
static int
amd_switch_in(struct pmc_cpu *pc __pmcdbg_used, struct pmc_process *pp)
{
	PMCDBG3(MDP, SWI, 1, "pc=%p pp=%p enable-msr=%d", pc, pp,
	    (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS) != 0);

	/* enable the RDPMC instruction if needed */
	if (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS)
		load_cr4(rcr4() | CR4_PCE);

	return (0);
}

/*
 * Machine-dependent actions taken during the context switch out of a
 * thread.
 */
static int
amd_switch_out(struct pmc_cpu *pc __pmcdbg_used,
    struct pmc_process *pp __pmcdbg_used)
{
	PMCDBG3(MDP, SWO, 1, "pc=%p pp=%p enable-msr=%d", pc, pp, pp ?
	    (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS) == 1 : 0);

	/* always turn off the RDPMC instruction */
	load_cr4(rcr4() & ~CR4_PCE);

	return (0);
}

/*
 * Check if a given PMC allocation is feasible.
 */
static int
amd_allocate_pmc(int cpu __unused, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	const struct pmc_descr *pd;
	uint64_t allowed_unitmask, caps, config, unitmask;
	enum pmc_event pe;
	int i;

	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row index %d", __LINE__, ri));

	pd = &amd_pmcdesc[ri].pm_descr;

	/* check class match */
	if (pd->pd_class != a->pm_class)
		return (EINVAL);

	if ((a->pm_flags & PMC_F_EV_PMU) == 0)
		return (EINVAL);

	caps = pm->pm_caps;

	PMCDBG2(MDP, ALL, 1,"amd-allocate ri=%d caps=0x%x", ri, caps);

	/* Validate sub-class. */
	if ((ri >= 0 && ri < 6) && a->pm_md.pm_amd.pm_amd_sub_class !=
	    PMC_AMD_SUB_CLASS_CORE)
		return (EINVAL);
	if ((ri >= 6 && ri < 12) && a->pm_md.pm_amd.pm_amd_sub_class !=
	    PMC_AMD_SUB_CLASS_L3_CACHE)
		return (EINVAL);
	if ((ri >= 12 && ri < 16) && a->pm_md.pm_amd.pm_amd_sub_class !=
	    PMC_AMD_SUB_CLASS_DATA_FABRIC)
		return (EINVAL);

	if (strlen(pmc_cpuid) != 0) {
		pm->pm_md.pm_amd.pm_amd_evsel = a->pm_md.pm_amd.pm_amd_config;
		PMCDBG2(MDP, ALL, 2,"amd-allocate ri=%d -> config=0x%x", ri,
		    a->pm_md.pm_amd.pm_amd_config);
		return (0);
	}

	pe = a->pm_ev;

	/* map ev to the correct event mask code */
	config = allowed_unitmask = 0;
	for (i = 0; i < amd_event_codes_size; i++) {
		if (amd_event_codes[i].pe_ev == pe) {
			config =
			    AMD_PMC_TO_EVENTMASK(amd_event_codes[i].pe_code);
			allowed_unitmask =
			    AMD_PMC_TO_UNITMASK(amd_event_codes[i].pe_mask);
			break;
		}
	}
	if (i == amd_event_codes_size)
		return (EINVAL);

	unitmask = a->pm_md.pm_amd.pm_amd_config & AMD_PMC_UNITMASK;
	if ((unitmask & ~allowed_unitmask) != 0) /* disallow reserved bits */
		return (EINVAL);

	if (unitmask && (caps & PMC_CAP_QUALIFIER) != 0)
		config |= unitmask;

	if ((caps & PMC_CAP_THRESHOLD) != 0)
		config |= a->pm_md.pm_amd.pm_amd_config & AMD_PMC_COUNTERMASK;

	/* Set at least one of the 'usr' or 'os' caps. */
	if ((caps & PMC_CAP_USER) != 0)
		config |= AMD_PMC_USR;
	if ((caps & PMC_CAP_SYSTEM) != 0)
		config |= AMD_PMC_OS;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		config |= (AMD_PMC_USR|AMD_PMC_OS);

	if ((caps & PMC_CAP_EDGE) != 0)
		config |= AMD_PMC_EDGE;
	if ((caps & PMC_CAP_INVERT) != 0)
		config |= AMD_PMC_INVERT;
	if ((caps & PMC_CAP_INTERRUPT) != 0)
		config |= AMD_PMC_INT;

	pm->pm_md.pm_amd.pm_amd_evsel = config; /* save config value */

	PMCDBG2(MDP, ALL, 2, "amd-allocate ri=%d -> config=0x%x", ri, config);

	return (0);
}

/*
 * Release machine dependent state associated with a PMC.  This is a
 * no-op on this architecture.
 */
static int
amd_release_pmc(int cpu, int ri, struct pmc *pmc __unused)
{
	struct pmc_hw *phw __diagused;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = &amd_pcpu[cpu]->pc_amdpmcs[ri];

	KASSERT(phw->phw_pmc == NULL,
	    ("[amd,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return (0);
}

/*
 * Start a PMC.
 */
static int
amd_start_pmc(int cpu __diagused, int ri, struct pmc *pm)
{
	const struct amd_descr *pd;
	uint64_t config;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	pd = &amd_pmcdesc[ri];

	PMCDBG2(MDP, STA, 1, "amd-start cpu=%d ri=%d", cpu, ri);

	KASSERT(AMD_PMC_IS_STOPPED(pd->pm_evsel),
	    ("[amd,%d] pmc%d,cpu%d: Starting active PMC \"%s\"", __LINE__,
	    ri, cpu, pd->pm_descr.pd_name));

	/* turn on the PMC ENABLE bit */
	config = pm->pm_md.pm_amd.pm_amd_evsel | AMD_PMC_ENABLE;

	PMCDBG1(MDP, STA, 2, "amd-start config=0x%x", config);

	wrmsr(pd->pm_evsel, config);
	return (0);
}

/*
 * Stop a PMC.
 */
static int
amd_stop_pmc(int cpu __diagused, int ri, struct pmc *pm)
{
	const struct amd_descr *pd;
	uint64_t config;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	pd = &amd_pmcdesc[ri];

	KASSERT(!AMD_PMC_IS_STOPPED(pd->pm_evsel),
	    ("[amd,%d] PMC%d, CPU%d \"%s\" already stopped",
		__LINE__, ri, cpu, pd->pm_descr.pd_name));

	PMCDBG1(MDP, STO, 1, "amd-stop ri=%d", ri);

	/* turn off the PMC ENABLE bit */
	config = pm->pm_md.pm_amd.pm_amd_evsel & ~AMD_PMC_ENABLE;
	wrmsr(pd->pm_evsel, config);

	/*
	 * Due to NMI latency on newer AMD processors
	 * NMI interrupts are ignored, which leads to
	 * panic or messages based on kernel configuration
	 */

	/* Wait for the count to be reset */
	for (i = 0; i < OVERFLOW_WAIT_COUNT; i++) {
		if (rdmsr(pd->pm_perfctr) & (1 << (pd->pm_descr.pd_width - 1)))
			break;

		DELAY(1);
	}

	return (0);
}

/*
 * Interrupt handler.  This function needs to return '1' if the
 * interrupt was this CPU's PMCs or '0' otherwise.  It is not allowed
 * to sleep or do anything a 'fast' interrupt handler is not allowed
 * to do.
 */
static int
amd_intr(struct trapframe *tf)
{
	struct amd_cpu *pac;
	struct pmc *pm;
	pmc_value_t v;
	uint64_t config, evsel, perfctr;
	uint32_t active = 0, count = 0;
	int i, error, retval, cpu;

	cpu = curcpu;
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] out of range CPU %d", __LINE__, cpu));

	PMCDBG3(MDP, INT, 1, "cpu=%d tf=%p um=%d", cpu, tf, TRAPF_USERMODE(tf));

	retval = 0;

	pac = amd_pcpu[cpu];

	/*
	 * look for all PMCs that have interrupted:
	 * - look for a running, sampling PMC which has overflowed
	 *   and which has a valid 'struct pmc' association
	 *
	 * If found, we call a helper to process the interrupt.
	 *
	 * PMCs interrupting at the same time are collapsed into
	 * a single interrupt. Check all the valid pmcs for
	 * overflow.
	 */
	for (i = 0; i < AMD_CORE_NPMCS; i++) {
		if ((pm = pac->pc_amdpmcs[i].phw_pmc) == NULL ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
			continue;
		}

		/* Consider pmc with valid handle as active */
		active++;

		if (!AMD_PMC_HAS_OVERFLOWED(i))
			continue;

		retval = 1;	/* Found an interrupting PMC. */

		if (pm->pm_state != PMC_STATE_RUNNING)
			continue;

		/* Stop the PMC, reload count. */
		evsel   = amd_pmcdesc[i].pm_evsel;
		perfctr = amd_pmcdesc[i].pm_perfctr;
		v       = pm->pm_sc.pm_reloadcount;
		config  = rdmsr(evsel);

		KASSERT((config & ~AMD_PMC_ENABLE) ==
		    (pm->pm_md.pm_amd.pm_amd_evsel & ~AMD_PMC_ENABLE),
		    ("[amd,%d] config mismatch reg=0x%jx pm=0x%jx", __LINE__,
			 (uintmax_t)config, (uintmax_t)pm->pm_md.pm_amd.pm_amd_evsel));

		wrmsr(evsel, config & ~AMD_PMC_ENABLE);
		wrmsr(perfctr, AMD_RELOAD_COUNT_TO_PERFCTR_VALUE(v));

		/* Restart the counter if logging succeeded. */
		error = pmc_process_interrupt(PMC_HR, pm, tf);
		if (error == 0)
			wrmsr(evsel, config);
	}

	/*
	 * Due to NMI latency, there can be a scenario in which
	 * multiple pmcs gets serviced in an earlier NMI and we
	 * do not find an overflow in the subsequent NMI.
	 *
	 * For such cases we keep a per-cpu count of active NMIs
	 * and compare it with min(active pmcs, 2) to determine
	 * if this NMI was for a pmc overflow which was serviced
	 * in an earlier request or should be ignored.
	 */
	if (retval) {
		DPCPU_SET(nmi_counter, min(2, active));
	} else {
		if ((count = DPCPU_GET(nmi_counter))) {
			retval = 1;
			DPCPU_SET(nmi_counter, --count);
		}
	}

	if (retval)
		counter_u64_add(pmc_stats.pm_intr_processed, 1);
	else
		counter_u64_add(pmc_stats.pm_intr_ignored, 1);

	PMCDBG1(MDP, INT, 2, "retval=%d", retval);
	return (retval);
}

/*
 * Describe a PMC.
 */
static int
amd_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	const struct amd_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] row-index %d out of range", __LINE__, ri));

	phw = &amd_pcpu[cpu]->pc_amdpmcs[ri];
	pd  = &amd_pmcdesc[ri];

	strlcpy(pi->pm_name, pd->pm_descr.pd_name, sizeof(pi->pm_name));
	pi->pm_class = pd->pm_descr.pd_class;

	if ((phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) != 0) {
		pi->pm_enabled = true;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = false;
		*ppmc          = NULL;
	}

	return (0);
}

/*
 * Return the MSR address of the given PMC.
 */
static int
amd_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] ri %d out of range", __LINE__, ri));

	*msr = amd_pmcdesc[ri].pm_perfctr - AMD_PMC_PERFCTR_0;
	return (0);
}

/*
 * Processor-dependent initialization.
 */
static int
amd_pcpu_init(struct pmc_mdep *md, int cpu)
{
	struct amd_cpu *pac;
	struct pmc_cpu *pc;
	struct pmc_hw  *phw;
	int first_ri, n;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG1(MDP, INI, 1, "amd-init cpu=%d", cpu);

	amd_pcpu[cpu] = pac = malloc(sizeof(struct amd_cpu), M_PMC,
	    M_WAITOK | M_ZERO);

	/*
	 * Set the content of the hardware descriptors to a known
	 * state and initialize pointers in the MI per-cpu descriptor.
	 */
	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_K8].pcd_ri;

	KASSERT(pc != NULL, ("[amd,%d] NULL per-cpu pointer", __LINE__));

	for (n = 0, phw = pac->pc_amdpmcs; n < AMD_NPMCS; n++, phw++) {
		phw->phw_state = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n);
		phw->phw_pmc = NULL;
		pc->pc_hwpmcs[n + first_ri] = phw;
	}

	return (0);
}

/*
 * Processor-dependent cleanup prior to the KLD being unloaded.
 */
static int
amd_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	struct amd_cpu *pac;
	struct pmc_cpu *pc;
	uint32_t evsel;
	int first_ri, i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[amd,%d] insane cpu number (%d)", __LINE__, cpu));

	PMCDBG1(MDP, INI, 1, "amd-cleanup cpu=%d", cpu);

	/*
	 * First, turn off all PMCs on this CPU.
	 */
	for (i = 0; i < 4; i++) { /* XXX this loop is now not needed */
		evsel = rdmsr(AMD_PMC_EVSEL_0 + i);
		evsel &= ~AMD_PMC_ENABLE;
		wrmsr(AMD_PMC_EVSEL_0 + i, evsel);
	}

	/*
	 * Next, free up allocated space.
	 */
	if ((pac = amd_pcpu[cpu]) == NULL)
		return (0);

	amd_pcpu[cpu] = NULL;

#ifdef	HWPMC_DEBUG
	for (i = 0; i < AMD_NPMCS; i++) {
		KASSERT(pac->pc_amdpmcs[i].phw_pmc == NULL,
		    ("[amd,%d] CPU%d/PMC%d in use", __LINE__, cpu, i));
		KASSERT(AMD_PMC_IS_STOPPED(AMD_PMC_EVSEL_0 + i),
		    ("[amd,%d] CPU%d/PMC%d not stopped", __LINE__, cpu, i));
	}
#endif

	pc = pmc_pcpu[cpu];
	KASSERT(pc != NULL, ("[amd,%d] NULL per-cpu state", __LINE__));

	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_K8].pcd_ri;

	/*
	 * Reset pointers in the MI 'per-cpu' state.
	 */
	for (i = 0; i < AMD_NPMCS; i++)
		pc->pc_hwpmcs[i + first_ri] = NULL;

	free(pac, M_PMC);
	return (0);
}

/*
 * Initialize ourselves.
 */
struct pmc_mdep *
pmc_amd_initialize(void)
{
	struct pmc_classdep *pcd;
	struct pmc_mdep *pmc_mdep;
	enum pmc_cputype cputype;
	int error, i, ncpus;
	int family, model, stepping;

	/*
	 * The presence of hardware performance counters on the AMD
	 * Athlon, Duron or later processors, is _not_ indicated by
	 * any of the processor feature flags set by the 'CPUID'
	 * instruction, so we only check the 'instruction family'
	 * field returned by CPUID for instruction family >= 6.
	 */

	family = CPUID_TO_FAMILY(cpu_id);
	model = CPUID_TO_MODEL(cpu_id);
	stepping = CPUID_TO_STEPPING(cpu_id);

	if (family == 0x18)
		snprintf(pmc_cpuid, sizeof(pmc_cpuid), "HygonGenuine-%d-%02X-%X",
		    family, model, stepping);
	else
		snprintf(pmc_cpuid, sizeof(pmc_cpuid), "AuthenticAMD-%d-%02X-%X",
		    family, model, stepping);

	switch (cpu_id & 0xF00) {
	case 0xF00:		/* Athlon64/Opteron processor */
		cputype = PMC_CPU_AMD_K8;
		break;
	default:
		printf("pmc: Unknown AMD CPU %x %d-%d.\n", cpu_id, family,
		    model);
		return (NULL);
	}

	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	amd_pcpu = malloc(sizeof(struct amd_cpu *) * pmc_cpu_max(), M_PMC,
	    M_WAITOK | M_ZERO);

	/*
	 * These processors have two classes of PMCs: the TSC and
	 * programmable PMCs.
	 */
	pmc_mdep = pmc_mdep_alloc(2);

	ncpus = pmc_cpu_max();

	/* Initialize the TSC. */
	error = pmc_tsc_initialize(pmc_mdep, ncpus);
	if (error != 0)
		goto error;

	/* Initialize AMD K8 PMC handling. */
	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_K8];

	pcd->pcd_caps		= AMD_PMC_CAPS;
	pcd->pcd_class		= PMC_CLASS_K8;
	pcd->pcd_num		= AMD_NPMCS;
	pcd->pcd_ri		= pmc_mdep->pmd_npmc;
	pcd->pcd_width		= 48;

	/* fill in the correct pmc name and class */
	for (i = 0; i < AMD_NPMCS; i++) {
		snprintf(amd_pmcdesc[i].pm_descr.pd_name, PMC_NAME_MAX, "K8-%d",
		    i);
	}

	pcd->pcd_allocate_pmc	= amd_allocate_pmc;
	pcd->pcd_config_pmc	= amd_config_pmc;
	pcd->pcd_describe	= amd_describe;
	pcd->pcd_get_config	= amd_get_config;
	pcd->pcd_get_msr	= amd_get_msr;
	pcd->pcd_pcpu_fini	= amd_pcpu_fini;
	pcd->pcd_pcpu_init	= amd_pcpu_init;
	pcd->pcd_read_pmc	= amd_read_pmc;
	pcd->pcd_release_pmc	= amd_release_pmc;
	pcd->pcd_start_pmc	= amd_start_pmc;
	pcd->pcd_stop_pmc	= amd_stop_pmc;
	pcd->pcd_write_pmc	= amd_write_pmc;

	pmc_mdep->pmd_cputype	= cputype;
	pmc_mdep->pmd_intr	= amd_intr;
	pmc_mdep->pmd_switch_in	= amd_switch_in;
	pmc_mdep->pmd_switch_out = amd_switch_out;

	pmc_mdep->pmd_npmc	+= AMD_NPMCS;

	PMCDBG0(MDP, INI, 0, "amd-initialize");

	return (pmc_mdep);

error:
	free(pmc_mdep, M_PMC);
	return (NULL);
}

/*
 * Finalization code for AMD CPUs.
 */
void
pmc_amd_finalize(struct pmc_mdep *md)
{
	PMCDBG0(MDP, INI, 1, "amd-finalize");

	pmc_tsc_finalize(md);

	for (int i = 0; i < pmc_cpu_max(); i++)
		KASSERT(amd_pcpu[i] == NULL,
		    ("[amd,%d] non-null pcpu cpu %d", __LINE__, i));

	free(amd_pcpu, M_PMC);
	amd_pcpu = NULL;
}
