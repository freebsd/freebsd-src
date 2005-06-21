/*-
 * Copyright (c) 2003-2005 Joseph Koshy
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
__FBSDID("$FreeBSD$");

/* Support for the AMD K7 and later processors */

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/pmc_mdep.h>
#include <machine/specialreg.h>

#if	DEBUG
enum pmc_class	amd_pmc_class;
#endif

/* AMD K7 & K8 PMCs */
struct amd_descr {
	struct pmc_descr pm_descr;  /* "base class" */
	uint32_t	pm_evsel;   /* address of EVSEL register */
	uint32_t	pm_perfctr; /* address of PERFCTR register */
};

static  struct amd_descr amd_pmcdesc[AMD_NPMCS] =
{
    {
	.pm_descr =
	{
		.pd_name  = "TSC",
		.pd_class = PMC_CLASS_TSC,
		.pd_caps  = PMC_CAP_READ,
		.pd_width = 64
	},
	.pm_evsel   = MSR_TSC,
	.pm_perfctr = 0	/* unused */
    },

    {
	.pm_descr =
	{
		.pd_name  = "",
		.pd_class = -1,
		.pd_caps  = AMD_PMC_CAPS,
		.pd_width = 48
	},
	.pm_evsel   = AMD_PMC_EVSEL_0,
	.pm_perfctr = AMD_PMC_PERFCTR_0
    },
    {
	.pm_descr =
	{
		.pd_name  = "",
		.pd_class = -1,
		.pd_caps  = AMD_PMC_CAPS,
		.pd_width = 48
	},
	.pm_evsel   = AMD_PMC_EVSEL_1,
	.pm_perfctr = AMD_PMC_PERFCTR_1
    },
    {
	.pm_descr =
	{
		.pd_name  = "",
		.pd_class = -1,
		.pd_caps  = AMD_PMC_CAPS,
		.pd_width = 48
	},
	.pm_evsel   = AMD_PMC_EVSEL_2,
	.pm_perfctr = AMD_PMC_PERFCTR_2
    },
    {
	.pm_descr =
	{
		.pd_name  = "",
		.pd_class = -1,
		.pd_caps  = AMD_PMC_CAPS,
		.pd_width = 48
	},
	.pm_evsel   = AMD_PMC_EVSEL_3,
	.pm_perfctr = AMD_PMC_PERFCTR_3
    }
};

struct amd_event_code_map {
	enum pmc_event	pe_ev;	 /* enum value */
	uint8_t		pe_code; /* encoded event mask */
	uint8_t		pe_mask; /* bits allowed in unit mask */
};

const struct amd_event_code_map amd_event_codes[] = {
#if	defined(__i386__)	/* 32 bit Athlon (K7) only */
	{ PMC_EV_K7_DC_ACCESSES, 		0x40, 0 },
	{ PMC_EV_K7_DC_MISSES,			0x41, 0 },
	{ PMC_EV_K7_DC_REFILLS_FROM_L2,		0x42, AMD_PMC_UNITMASK_MOESI },
	{ PMC_EV_K7_DC_REFILLS_FROM_SYSTEM,	0x43, AMD_PMC_UNITMASK_MOESI },
	{ PMC_EV_K7_DC_WRITEBACKS,		0x44, AMD_PMC_UNITMASK_MOESI },
	{ PMC_EV_K7_L1_DTLB_MISS_AND_L2_DTLB_HITS, 0x45, 0 },
	{ PMC_EV_K7_L1_AND_L2_DTLB_MISSES,	0x46, 0 },
	{ PMC_EV_K7_MISALIGNED_REFERENCES,	0x47, 0 },

	{ PMC_EV_K7_IC_FETCHES,			0x80, 0 },
	{ PMC_EV_K7_IC_MISSES,			0x81, 0 },

	{ PMC_EV_K7_L1_ITLB_MISSES,		0x84, 0 },
	{ PMC_EV_K7_L1_L2_ITLB_MISSES,		0x85, 0 },

	{ PMC_EV_K7_RETIRED_INSTRUCTIONS,	0xC0, 0 },
	{ PMC_EV_K7_RETIRED_OPS,		0xC1, 0 },
	{ PMC_EV_K7_RETIRED_BRANCHES,		0xC2, 0 },
	{ PMC_EV_K7_RETIRED_BRANCHES_MISPREDICTED, 0xC3, 0 },
	{ PMC_EV_K7_RETIRED_TAKEN_BRANCHES, 	0xC4, 0 },
	{ PMC_EV_K7_RETIRED_TAKEN_BRANCHES_MISPREDICTED, 0xC5, 0 },
	{ PMC_EV_K7_RETIRED_FAR_CONTROL_TRANSFERS, 0xC6, 0 },
	{ PMC_EV_K7_RETIRED_RESYNC_BRANCHES,	0xC7, 0 },
	{ PMC_EV_K7_INTERRUPTS_MASKED_CYCLES,	0xCD, 0 },
	{ PMC_EV_K7_INTERRUPTS_MASKED_WHILE_PENDING_CYCLES, 0xCE, 0 },
	{ PMC_EV_K7_HARDWARE_INTERRUPTS,	0xCF, 0 },
#endif

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

const int amd_event_codes_size =
	sizeof(amd_event_codes) / sizeof(amd_event_codes[0]);

/*
 * read a pmc register
 */

static int
amd_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	enum pmc_mode mode;
	const struct amd_descr *pd;
	struct pmc *pm;
	const struct pmc_hw *phw;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pd  = &amd_pmcdesc[ri];
	pm  = phw->phw_pmc;

	KASSERT(pm != NULL,
	    ("[amd,%d] No owner for HWPMC [cpu%d,pmc%d]", __LINE__,
		cpu, ri));

	mode = PMC_TO_MODE(pm);

	PMCDBG(MDP,REA,1,"amd-read id=%d class=%d", ri, pd->pm_descr.pd_class);

	/* Reading the TSC is a special case */
	if (pd->pm_descr.pd_class == PMC_CLASS_TSC) {
		KASSERT(PMC_IS_COUNTING_MODE(mode),
		    ("[amd,%d] TSC counter in non-counting mode", __LINE__));
		*v = rdtsc();
		PMCDBG(MDP,REA,2,"amd-read id=%d -> %jd", ri, *v);
		return 0;
	}

#if	DEBUG
	KASSERT(pd->pm_descr.pd_class == amd_pmc_class,
	    ("[amd,%d] unknown PMC class (%d)", __LINE__,
		pd->pm_descr.pd_class));
#endif

	tmp = rdmsr(pd->pm_perfctr); /* RDMSR serializes */
	if (PMC_IS_SAMPLING_MODE(mode))
		*v = AMD_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	else
		*v = tmp;

	PMCDBG(MDP,REA,2,"amd-read id=%d -> %jd", ri, *v);

	return 0;
}

/*
 * Write a PMC MSR.
 */

static int
amd_write_pmc(int cpu, int ri, pmc_value_t v)
{
	const struct amd_descr *pd;
	struct pmc *pm;
	const struct pmc_hw *phw;
	enum pmc_mode mode;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pd  = &amd_pmcdesc[ri];
	pm  = phw->phw_pmc;

	KASSERT(pm != NULL,
	    ("[amd,%d] PMC not owned (cpu%d,pmc%d)", __LINE__,
		cpu, ri));

	mode = PMC_TO_MODE(pm);

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;

#if	DEBUG
	KASSERT(pd->pm_descr.pd_class == amd_pmc_class,
	    ("[amd,%d] unknown PMC class (%d)", __LINE__,
		pd->pm_descr.pd_class));
#endif

	/* use 2's complement of the count for sampling mode PMCs */
	if (PMC_IS_SAMPLING_MODE(mode))
		v = AMD_RELOAD_COUNT_TO_PERFCTR_VALUE(v);

	PMCDBG(MDP,WRI,1,"amd-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	/* write the PMC value */
	wrmsr(pd->pm_perfctr, v);
	return 0;
}

/*
 * configure hardware pmc according to the configuration recorded in
 * pmc 'pm'.
 */

static int
amd_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[amd,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
		__LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;
	return 0;
}

/*
 * Retrieve a configured PMC pointer from hardware state.
 */

static int
amd_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = pmc_pcpu[cpu]->pc_hwpmcs[ri]->phw_pmc;

	return 0;
}

/*
 * Machine dependent actions taken during the context switch in of a
 * thread.
 */

static int
amd_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;

	PMCDBG(MDP,SWI,1, "pc=%p pp=%p enable-msr=%d", pc, pp,
	    (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS) != 0);

	/* enable the RDPMC instruction if needed */
	if (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS)
		load_cr4(rcr4() | CR4_PCE);

	return 0;
}

/*
 * Machine dependent actions taken during the context switch out of a
 * thread.
 */

static int
amd_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	(void) pc;
	(void) pp;		/* can be NULL */

	PMCDBG(MDP,SWO,1, "pc=%p pp=%p enable-msr=%d", pc, pp, pp ?
	    (pp->pp_flags & PMC_PP_ENABLE_MSR_ACCESS) == 1 : 0);

	/* always turn off the RDPMC instruction */
	load_cr4(rcr4() & ~CR4_PCE);

	return 0;
}

/*
 * Check if a given allocation is feasible.
 */

static int
amd_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	int i;
	uint32_t allowed_unitmask, caps, config, unitmask;
	enum pmc_event pe;
	const struct pmc_descr *pd;

	(void) cpu;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row index %d", __LINE__, ri));

	pd = &amd_pmcdesc[ri].pm_descr;

	/* check class match */
	if (pd->pd_class != a->pm_class)
		return EINVAL;

	caps = pm->pm_caps;

	PMCDBG(MDP,ALL,1,"amd-allocate ri=%d caps=0x%x", ri, caps);

	if ((pd->pd_caps & caps) != caps)
		return EPERM;
	if (pd->pd_class == PMC_CLASS_TSC) {
		/* TSC's are always allocated in system-wide counting mode */
		if (a->pm_ev != PMC_EV_TSC_TSC ||
		    a->pm_mode != PMC_MODE_SC)
			return EINVAL;
		return 0;
	}

#if	DEBUG
	KASSERT(pd->pd_class == amd_pmc_class,
	    ("[amd,%d] Unknown PMC class (%d)", __LINE__, pd->pd_class));
#endif

	pe = a->pm_ev;

	/* map ev to the correct event mask code */
	config = allowed_unitmask = 0;
	for (i = 0; i < amd_event_codes_size; i++)
		if (amd_event_codes[i].pe_ev == pe) {
			config =
			    AMD_PMC_TO_EVENTMASK(amd_event_codes[i].pe_code);
			allowed_unitmask =
			    AMD_PMC_TO_UNITMASK(amd_event_codes[i].pe_mask);
			break;
		}
	if (i == amd_event_codes_size)
		return EINVAL;

	unitmask = a->pm_md.pm_amd.pm_amd_config & AMD_PMC_UNITMASK;
	if (unitmask & ~allowed_unitmask) /* disallow reserved bits */
		return EINVAL;

	if (unitmask && (caps & PMC_CAP_QUALIFIER))
		config |= unitmask;

	if (caps & PMC_CAP_THRESHOLD)
		config |= a->pm_md.pm_amd.pm_amd_config & AMD_PMC_COUNTERMASK;

	/* set at least one of the 'usr' or 'os' caps */
	if (caps & PMC_CAP_USER)
		config |= AMD_PMC_USR;
	if (caps & PMC_CAP_SYSTEM)
		config |= AMD_PMC_OS;
	if ((caps & (PMC_CAP_USER|PMC_CAP_SYSTEM)) == 0)
		config |= (AMD_PMC_USR|AMD_PMC_OS);

	if (caps & PMC_CAP_EDGE)
		config |= AMD_PMC_EDGE;
	if (caps & PMC_CAP_INVERT)
		config |= AMD_PMC_INVERT;
	if (caps & PMC_CAP_INTERRUPT)
		config |= AMD_PMC_INT;

	pm->pm_md.pm_amd.pm_amd_evsel = config; /* save config value */

	PMCDBG(MDP,ALL,2,"amd-allocate ri=%d -> config=0x%x", ri, config);

	return 0;
}

/*
 * Release machine dependent state associated with a PMC.  This is a
 * no-op on this architecture.
 *
 */

/* ARGSUSED0 */
static int
amd_release_pmc(int cpu, int ri, struct pmc *pmc)
{
#if	DEBUG
	const struct amd_descr *pd;
#endif
	struct pmc_hw *phw;

	(void) pmc;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];

	KASSERT(phw->phw_pmc == NULL,
	    ("[amd,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

#if 	DEBUG
	pd = &amd_pmcdesc[ri];
	if (pd->pm_descr.pd_class == amd_pmc_class)
		KASSERT(AMD_PMC_IS_STOPPED(pd->pm_evsel),
		    ("[amd,%d] PMC %d released while active", __LINE__, ri));
#endif

	return 0;
}

/*
 * start a PMC.
 */

static int
amd_start_pmc(int cpu, int ri)
{
	uint32_t config;
	struct pmc *pm;
	struct pmc_hw *phw;
	const struct amd_descr *pd;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd = &amd_pmcdesc[ri];

	KASSERT(pm != NULL,
	    ("[amd,%d] starting cpu%d,pmc%d with null pmc record", __LINE__,
		cpu, ri));

	PMCDBG(MDP,STA,1,"amd-start cpu=%d ri=%d", cpu, ri);

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;	/* TSCs are always running */

#if	DEBUG
	KASSERT(pd->pm_descr.pd_class == amd_pmc_class,
	    ("[amd,%d] unknown PMC class (%d)", __LINE__,
		pd->pm_descr.pd_class));
#endif

	KASSERT(AMD_PMC_IS_STOPPED(pd->pm_evsel),
	    ("[amd,%d] pmc%d,cpu%d: Starting active PMC \"%s\"", __LINE__,
	    ri, cpu, pd->pm_descr.pd_name));

	/* turn on the PMC ENABLE bit */
	config = pm->pm_md.pm_amd.pm_amd_evsel | AMD_PMC_ENABLE;

	PMCDBG(MDP,STA,2,"amd-start config=0x%x", config);

	wrmsr(pd->pm_evsel, config);
	return 0;
}

/*
 * Stop a PMC.
 */

static int
amd_stop_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct pmc_hw *phw;
	const struct amd_descr *pd;
	uint64_t config;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &amd_pmcdesc[ri];

	KASSERT(pm != NULL,
	    ("[amd,%d] cpu%d,pmc%d no PMC to stop", __LINE__,
		cpu, ri));

	/* can't stop a TSC */
	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;

#if	DEBUG
	KASSERT(pd->pm_descr.pd_class == amd_pmc_class,
	    ("[amd,%d] unknown PMC class (%d)", __LINE__,
		pd->pm_descr.pd_class));
#endif

	KASSERT(!AMD_PMC_IS_STOPPED(pd->pm_evsel),
	    ("[amd,%d] PMC%d, CPU%d \"%s\" already stopped",
		__LINE__, ri, cpu, pd->pm_descr.pd_name));

	PMCDBG(MDP,STO,1,"amd-stop ri=%d", ri);

	/* turn off the PMC ENABLE bit */
	config = pm->pm_md.pm_amd.pm_amd_evsel & ~AMD_PMC_ENABLE;
	wrmsr(pd->pm_evsel, config);
	return 0;
}

/*
 * Interrupt handler.  This function needs to return '1' if the
 * interrupt was this CPU's PMCs or '0' otherwise.  It is not allowed
 * to sleep or do anything a 'fast' interrupt handler is not allowed
 * to do.
 */

static int
amd_intr(int cpu, uintptr_t eip, int usermode)
{
	int i, error, retval, ri;
	uint32_t config, evsel, perfctr;
	struct pmc *pm;
	struct pmc_cpu *pc;
	struct pmc_hw *phw;
	pmc_value_t v;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] out of range CPU %d", __LINE__, cpu));

	PMCDBG(MDP,INT,1, "cpu=%d eip=%p", cpu, (void *) eip);

	retval = 0;

	pc = pmc_pcpu[cpu];

	/*
	 * look for all PMCs that have interrupted:
	 * - skip over the TSC [PMC#0]
	 * - look for a running, sampling PMC which has overflowed
	 *   and which has a valid 'struct pmc' association
	 *
	 * If found, we call a helper to process the interrupt.
	 */

	for (i = 0; i < AMD_NPMCS-1; i++) {

		ri = i + 1;	/* row index; TSC is at ri == 0 */

		if (!AMD_PMC_HAS_OVERFLOWED(i))
			continue;

		phw = pc->pc_hwpmcs[ri];

		KASSERT(phw != NULL, ("[amd,%d] null PHW pointer", __LINE__));

		if ((pm = phw->phw_pmc) == NULL ||
		    pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm))) {
			continue;
		}

		/* stop the PMC, reload count */
		evsel   = AMD_PMC_EVSEL_0 + i;
		perfctr = AMD_PMC_PERFCTR_0 + i;
		v       = pm->pm_sc.pm_reloadcount;
		config  = rdmsr(evsel);

		KASSERT((config & ~AMD_PMC_ENABLE) ==
		    (pm->pm_md.pm_amd.pm_amd_evsel & ~AMD_PMC_ENABLE),
		    ("[amd,%d] config mismatch reg=0x%x pm=0x%x", __LINE__,
			config, pm->pm_md.pm_amd.pm_amd_evsel));

		wrmsr(evsel, config & ~AMD_PMC_ENABLE);
		wrmsr(perfctr, AMD_RELOAD_COUNT_TO_PERFCTR_VALUE(v));

		/* restart if there was no error during logging */
		error = pmc_process_interrupt(cpu, pm, eip, usermode);
		if (error == 0)
			wrmsr(evsel, config | AMD_PMC_ENABLE);

		retval = 1;	/* found an interrupting PMC */
	}

	if (retval == 0)
		atomic_add_int(&pmc_stats.pm_intr_ignored, 1);
	return retval;
}

/*
 * describe a PMC
 */
static int
amd_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	size_t copied;
	const struct amd_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] row-index %d out of range", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pd  = &amd_pmcdesc[ri];

	if ((error = copystr(pd->pm_descr.pd_name, pi->pm_name,
		 PMC_NAME_MAX, &copied)) != 0)
		return error;

	pi->pm_class = pd->pm_descr.pd_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return 0;
}

/*
 * i386 specific entry points
 */

/*
 * return the MSR address of the given PMC.
 */

static int
amd_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < AMD_NPMCS,
	    ("[amd,%d] ri %d out of range", __LINE__, ri));

	*msr = amd_pmcdesc[ri].pm_perfctr - AMD_PMC_PERFCTR_0;
	return 0;
}

/*
 * processor dependent initialization.
 */

/*
 * Per-processor data structure
 *
 * [common stuff]
 * [5 struct pmc_hw pointers]
 * [5 struct pmc_hw structures]
 */

struct amd_cpu {
	struct pmc_cpu	pc_common;
	struct pmc_hw	*pc_hwpmcs[AMD_NPMCS];
	struct pmc_hw	pc_amdpmcs[AMD_NPMCS];
};


static int
amd_init(int cpu)
{
	int n;
	struct amd_cpu *pcs;
	struct pmc_hw  *phw;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG(MDP,INI,1,"amd-init cpu=%d", cpu);

	MALLOC(pcs, struct amd_cpu *, sizeof(struct amd_cpu), M_PMC,
	    M_WAITOK|M_ZERO);

	phw = &pcs->pc_amdpmcs[0];

	/*
	 * Initialize the per-cpu mutex and set the content of the
	 * hardware descriptors to a known state.
	 */

	for (n = 0; n < AMD_NPMCS; n++, phw++) {
		phw->phw_state 	  = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n);
		phw->phw_pmc	  = NULL;
		pcs->pc_hwpmcs[n] = phw;
	}

	/* Mark the TSC as shareable */
	pcs->pc_hwpmcs[0]->phw_state |= PMC_PHW_FLAG_IS_SHAREABLE;

	pmc_pcpu[cpu] = (struct pmc_cpu *) pcs;

	return 0;
}


/*
 * processor dependent cleanup prior to the KLD
 * being unloaded
 */

static int
amd_cleanup(int cpu)
{
	int i;
	uint32_t evsel;
	struct pmc_cpu *pcs;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[amd,%d] insane cpu number (%d)", __LINE__, cpu));

	PMCDBG(MDP,INI,1,"amd-cleanup cpu=%d", cpu);

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

	if ((pcs = pmc_pcpu[cpu]) == NULL)
		return 0;

#if	DEBUG
	/* check the TSC */
	KASSERT(pcs->pc_hwpmcs[0]->phw_pmc == NULL,
	    ("[amd,%d] CPU%d,PMC0 still in use", __LINE__, cpu));
	for (i = 1; i < AMD_NPMCS; i++) {
		KASSERT(pcs->pc_hwpmcs[i]->phw_pmc == NULL,
		    ("[amd,%d] CPU%d/PMC%d in use", __LINE__, cpu, i));
		KASSERT(AMD_PMC_IS_STOPPED(AMD_PMC_EVSEL_0 + (i-1)),
		    ("[amd,%d] CPU%d/PMC%d not stopped", __LINE__, cpu, i));
	}
#endif

	pmc_pcpu[cpu] = NULL;
	FREE(pcs, M_PMC);
	return 0;
}

/*
 * Initialize ourselves.
 */

struct pmc_mdep *
pmc_amd_initialize(void)
{
	enum pmc_cputype cputype;
	enum pmc_class class;
	struct pmc_mdep *pmc_mdep;
	char *name;
	int i;

	/*
	 * The presence of hardware performance counters on the AMD
	 * Athlon, Duron or later processors, is _not_ indicated by
	 * any of the processor feature flags set by the 'CPUID'
	 * instruction, so we only check the 'instruction family'
	 * field returned by CPUID for instruction family >= 6.
	 */

	class = cputype = -1;
	name = NULL;
	switch (cpu_id & 0xF00) {
	case 0x600:		/* Athlon(tm) processor */
		cputype = PMC_CPU_AMD_K7;
		class = PMC_CLASS_K7;
		name = "K7";
		break;
	case 0xF00:		/* Athlon64/Opteron processor */
		cputype = PMC_CPU_AMD_K8;
		class = PMC_CLASS_K8;
		name = "K8";
		break;
	}

	if ((int) cputype == -1) {
		(void) printf("pmc: Unknown AMD CPU.\n");
		return NULL;
	}

#if	DEBUG
	amd_pmc_class = class;
#endif

	MALLOC(pmc_mdep, struct pmc_mdep *, sizeof(struct pmc_mdep),
	    M_PMC, M_WAITOK|M_ZERO);

	pmc_mdep->pmd_cputype	   = cputype;
	pmc_mdep->pmd_npmc 	   = AMD_NPMCS;

	/* this processor has two classes of usable PMCs */
	pmc_mdep->pmd_nclass       = 2;

	/* TSC */
	pmc_mdep->pmd_classes[0].pm_class   = PMC_CLASS_TSC;
	pmc_mdep->pmd_classes[0].pm_caps    = PMC_CAP_READ;
	pmc_mdep->pmd_classes[0].pm_width   = 64;

	/* AMD K7/K8 PMCs */
	pmc_mdep->pmd_classes[1].pm_class   = class;
	pmc_mdep->pmd_classes[1].pm_caps    = AMD_PMC_CAPS;
	pmc_mdep->pmd_classes[1].pm_width   = 48;

	pmc_mdep->pmd_nclasspmcs[0] = 1;
	pmc_mdep->pmd_nclasspmcs[1] = (AMD_NPMCS-1);

	/* fill in the correct pmc name and class */
	for (i = 1; i < AMD_NPMCS; i++) {
		(void) snprintf(amd_pmcdesc[i].pm_descr.pd_name,
		    sizeof(amd_pmcdesc[i].pm_descr.pd_name), "%s-%d",
		    name, i-1);
		amd_pmcdesc[i].pm_descr.pd_class = class;
	}

	pmc_mdep->pmd_init    	   = amd_init;
	pmc_mdep->pmd_cleanup 	   = amd_cleanup;
	pmc_mdep->pmd_switch_in    = amd_switch_in;
	pmc_mdep->pmd_switch_out   = amd_switch_out;
	pmc_mdep->pmd_read_pmc 	   = amd_read_pmc;
	pmc_mdep->pmd_write_pmc    = amd_write_pmc;
	pmc_mdep->pmd_config_pmc   = amd_config_pmc;
	pmc_mdep->pmd_get_config   = amd_get_config;
	pmc_mdep->pmd_allocate_pmc = amd_allocate_pmc;
	pmc_mdep->pmd_release_pmc  = amd_release_pmc;
	pmc_mdep->pmd_start_pmc    = amd_start_pmc;
	pmc_mdep->pmd_stop_pmc     = amd_stop_pmc;
	pmc_mdep->pmd_intr	   = amd_intr;
	pmc_mdep->pmd_describe     = amd_describe;
	pmc_mdep->pmd_get_msr  	   = amd_get_msr; /* i386 */

	PMCDBG(MDP,INI,0,"%s","amd-initialize");

	return pmc_mdep;
}
