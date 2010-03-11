/*-
 * Copyright (c) 2010 George V. Neville-Neil <gnn@freebsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/pmc_mdep.h>

/*
 * Support for MIPS CPUs
 *
 */
static int mips24k_npmcs;

struct mips24k_event_code_map {
	enum pmc_event	pe_ev;       /* enum value */
	uint8_t         pe_counter;  /* Which counter this can be counted in. */
	uint8_t		pe_code;     /* numeric code */
};

/*
 * MIPS event codes are encoded with a select bit.  The
 * select bit is used when writing to CP0 so that we 
 * can select either counter 0/2 or 1/3.  The cycle
 * and instruction counters are special in that they
 * can be counted on either 0/2 or 1/3.
 */

#define MIPS24K_ALL 255 /* Count events in any counter. */
#define MIPS24K_CTR_0 0 /* Counter 0 Event */
#define MIPS24K_CTR_1 1 /* Counter 1 Event */

const struct mips24k_event_code_map mips24k_event_codes[] = {
	{ PMC_EV_MIPS24K_CYCLE, MIPS24K_ALL, 0},
	{ PMC_EV_MIPS24K_INSTR_EXECUTED, MIPS24K_ALL, 1},
	{ PMC_EV_MIPS24K_BRANCH_COMPLETED, MIPS24K_CTR_0, 2},
	{ PMC_EV_MIPS24K_BRANCH_MISPRED, MIPS24K_CTR_1, 2},
	{ PMC_EV_MIPS24K_RETURN, MIPS24K_CTR_0, 3},
	{ PMC_EV_MIPS24K_RETURN_MISPRED, MIPS24K_CTR_1, 3},
	{ PMC_EV_MIPS24K_RETURN_NOT_31, MIPS24K_CTR_0, 4},
	{ PMC_EV_MIPS24K_RETURN_NOTPRED, MIPS24K_CTR_1, 4},
	{ PMC_EV_MIPS24K_ITLB_ACCESS, MIPS24K_CTR_0, 5},
	{ PMC_EV_MIPS24K_ITLB_MISS, MIPS24K_CTR_1, 5},
	{ PMC_EV_MIPS24K_DTLB_ACCESS, MIPS24K_CTR_0, 6},
	{ PMC_EV_MIPS24K_DTLB_MISS, MIPS24K_CTR_1, 6},
	{ PMC_EV_MIPS24K_JTLB_IACCESS, MIPS24K_CTR_0, 7},
	{ PMC_EV_MIPS24K_JTLB_IMISS, MIPS24K_CTR_1, 7},
	{ PMC_EV_MIPS24K_JTLB_DACCESS, MIPS24K_CTR_0, 8},
	{ PMC_EV_MIPS24K_JTLB_DMISS, MIPS24K_CTR_1, 8},
	{ PMC_EV_MIPS24K_IC_FETCH, MIPS24K_CTR_0, 9},
	{ PMC_EV_MIPS24K_IC_MISS, MIPS24K_CTR_1, 9},
	{ PMC_EV_MIPS24K_DC_LOADSTORE, MIPS24K_CTR_0, 10},
	{ PMC_EV_MIPS24K_DC_WRITEBACK, MIPS24K_CTR_1, 10},
	{ PMC_EV_MIPS24K_DC_MISS, MIPS24K_ALL, 11},  
	/* 12 reserved */
	{ PMC_EV_MIPS24K_STORE_MISS, MIPS24K_CTR_0, 13},
	{ PMC_EV_MIPS24K_LOAD_MISS, MIPS24K_CTR_1, 13},
	{ PMC_EV_MIPS24K_INTEGER_COMPLETED, MIPS24K_CTR_0, 14},
	{ PMC_EV_MIPS24K_FP_COMPLETED, MIPS24K_CTR_1, 14},
	{ PMC_EV_MIPS24K_LOAD_COMPLETED, MIPS24K_CTR_0, 15},
	{ PMC_EV_MIPS24K_STORE_COMPLETED, MIPS24K_CTR_1, 15},
	{ PMC_EV_MIPS24K_BARRIER_COMPLETED, MIPS24K_CTR_0, 16},
	{ PMC_EV_MIPS24K_MIPS16_COMPLETED, MIPS24K_CTR_1, 16},
	{ PMC_EV_MIPS24K_NOP_COMPLETED, MIPS24K_CTR_0, 17},
	{ PMC_EV_MIPS24K_INTEGER_MULDIV_COMPLETED, MIPS24K_CTR_1, 17},
	{ PMC_EV_MIPS24K_RF_STALL, MIPS24K_CTR_0, 18},
	{ PMC_EV_MIPS24K_INSTR_REFETCH, MIPS24K_CTR_1, 18},
	{ PMC_EV_MIPS24K_STORE_COND_COMPLETED, MIPS24K_CTR_0, 19},
	{ PMC_EV_MIPS24K_STORE_COND_FAILED, MIPS24K_CTR_1, 19},
	{ PMC_EV_MIPS24K_ICACHE_REQUESTS, MIPS24K_CTR_0, 20},
	{ PMC_EV_MIPS24K_ICACHE_HIT, MIPS24K_CTR_1, 20},
	{ PMC_EV_MIPS24K_L2_WRITEBACK, MIPS24K_CTR_0, 21},
	{ PMC_EV_MIPS24K_L2_ACCESS, MIPS24K_CTR_1, 21},
	{ PMC_EV_MIPS24K_L2_MISS, MIPS24K_CTR_0, 22},
	{ PMC_EV_MIPS24K_L2_ERR_CORRECTED, MIPS24K_CTR_1, 22},
	{ PMC_EV_MIPS24K_EXCEPTIONS, MIPS24K_CTR_0, 23},
	/* Event 23 on COP0 1/3 is undefined */
	{ PMC_EV_MIPS24K_RF_CYCLES_STALLED, MIPS24K_CTR_0, 24},
	{ PMC_EV_MIPS24K_IFU_CYCLES_STALLED, MIPS24K_CTR_0, 25},
	{ PMC_EV_MIPS24K_ALU_CYCLES_STALLED, MIPS24K_CTR_1, 25},
	/* Events 26 through 32 undefined or reserved to customers */
	{ PMC_EV_MIPS24K_UNCACHED_LOAD, MIPS24K_CTR_0, 33},
	{ PMC_EV_MIPS24K_UNCACHED_STORE, MIPS24K_CTR_1, 33},
	{ PMC_EV_MIPS24K_CP2_REG_TO_REG_COMPLETED, MIPS24K_CTR_0, 35},
	{ PMC_EV_MIPS24K_MFTC_COMPLETED, MIPS24K_CTR_1, 35},
	/* Event 36 reserved */
	{ PMC_EV_MIPS24K_IC_BLOCKED_CYCLES, MIPS24K_CTR_0, 37},
	{ PMC_EV_MIPS24K_DC_BLOCKED_CYCLES, MIPS24K_CTR_1, 37},
	{ PMC_EV_MIPS24K_L2_IMISS_STALL_CYCLES, MIPS24K_CTR_0, 38},
	{ PMC_EV_MIPS24K_L2_DMISS_STALL_CYCLES, MIPS24K_CTR_1, 38},
	{ PMC_EV_MIPS24K_DMISS_CYCLES, MIPS24K_CTR_0, 39},
	{ PMC_EV_MIPS24K_L2_MISS_CYCLES, MIPS24K_CTR_1, 39},
	{ PMC_EV_MIPS24K_UNCACHED_BLOCK_CYCLES, MIPS24K_CTR_0, 40},
	{ PMC_EV_MIPS24K_MDU_STALL_CYCLES, MIPS24K_CTR_0, 41},
	{ PMC_EV_MIPS24K_FPU_STALL_CYCLES, MIPS24K_CTR_1, 41},
	{ PMC_EV_MIPS24K_CP2_STALL_CYCLES, MIPS24K_CTR_0, 42},
	{ PMC_EV_MIPS24K_COREXTEND_STALL_CYCLES, MIPS24K_CTR_1, 42},
	{ PMC_EV_MIPS24K_ISPRAM_STALL_CYCLES, MIPS24K_CTR_0, 43},
	{ PMC_EV_MIPS24K_DSPRAM_STALL_CYCLES, MIPS24K_CTR_1, 43},
	{ PMC_EV_MIPS24K_CACHE_STALL_CYCLES, MIPS24K_CTR_0, 44},
	/* Event 44 undefined on 1/3 */
	{ PMC_EV_MIPS24K_LOAD_TO_USE_STALLS, MIPS24K_CTR_0, 45},
	{ PMC_EV_MIPS24K_BASE_MISPRED_STALLS, MIPS24K_CTR_1, 45},
	{ PMC_EV_MIPS24K_CPO_READ_STALLS, MIPS24K_CTR_0, 46},
	{ PMC_EV_MIPS24K_BRANCH_MISPRED_CYCLES, MIPS24K_CTR_1, 46},
	/* Event 47 reserved */
	{ PMC_EV_MIPS24K_IFETCH_BUFFER_FULL, MIPS24K_CTR_0, 48},
	{ PMC_EV_MIPS24K_FETCH_BUFFER_ALLOCATED, MIPS24K_CTR_1, 48},
	{ PMC_EV_MIPS24K_EJTAG_ITRIGGER, MIPS24K_CTR_0, 49},
	{ PMC_EV_MIPS24K_EJTAG_DTRIGGER, MIPS24K_CTR_1, 49},
	{ PMC_EV_MIPS24K_FSB_LT_QUARTER, MIPS24K_CTR_0, 50},
	{ PMC_EV_MIPS24K_FSB_QUARTER_TO_HALF, MIPS24K_CTR_1, 50},
	{ PMC_EV_MIPS24K_FSB_GT_HALF, MIPS24K_CTR_0, 51},
	{ PMC_EV_MIPS24K_FSB_FULL_PIPELINE_STALLS, MIPS24K_CTR_1, 51},
	{ PMC_EV_MIPS24K_LDQ_LT_QUARTER, MIPS24K_CTR_0, 52},
	{ PMC_EV_MIPS24K_LDQ_QUARTER_TO_HALF, MIPS24K_CTR_1, 52},
	{ PMC_EV_MIPS24K_LDQ_GT_HALF, MIPS24K_CTR_0, 53},
	{ PMC_EV_MIPS24K_LDQ_FULL_PIPELINE_STALLS, MIPS24K_CTR_1, 53},
	{ PMC_EV_MIPS24K_WBB_LT_QUARTER, MIPS24K_CTR_0, 54},
	{ PMC_EV_MIPS24K_WBB_QUARTER_TO_HALF, MIPS24K_CTR_1, 54},
	{ PMC_EV_MIPS24K_WBB_GT_HALF, MIPS24K_CTR_0, 55},
	{ PMC_EV_MIPS24K_WBB_FULL_PIPELINE_STALLS, MIPS24K_CTR_1, 55},
	/* Events 56-63 reserved */
	{ PMC_EV_MIPS24K_REQUEST_LATENCY, MIPS24K_CTR_0, 61},
	{ PMC_EV_MIPS24K_REQUEST_COUNT, MIPS24K_CTR_1, 61}

};

const int mips24k_event_codes_size =
	sizeof(mips24k_event_codes) / sizeof(mips24k_event_codes[0]);

/*
 * Per-processor information.
 */
struct mips24k_cpu {
	struct pmc_hw   *pc_mipspmcs;
};

static struct mips24k_cpu **mips24k_pcpu;

/*
 * Performance Count Register N
 */
static uint32_t
mips24k_pmcn_read(unsigned int pmc)
{
	uint32_t reg = 0;

	KASSERT(pmc < mips24k_npmcs, ("[mips,%d] illegal PMC number %d", 
				   __LINE__, pmc));

	/* The counter value is the next value after the control register. */
	switch (pmc) {
	case 0:
		reg = mips_rd_perfcnt1();
		break;
	case 1:
		reg = mips_rd_perfcnt3();
		break;
	default:
		return 0;
	}
	return (reg);
}

static uint32_t
mips24k_pmcn_write(unsigned int pmc, uint32_t reg)
{

	KASSERT(pmc < mips24k_npmcs, ("[mips,%d] illegal PMC number %d", 
				   __LINE__, pmc));
	
	switch (pmc) {
	case 0:
		mips_wr_perfcnt1(reg);
		break;
	case 1:
		mips_wr_perfcnt3(reg);
		break;
	default:
		return 0;
	}
	return (reg);
}

static int
mips24k_allocate_pmc(int cpu, int ri, struct pmc *pm,
  const struct pmc_op_pmcallocate *a)
{
	enum pmc_event pe;
	uint32_t caps, config, counter;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] illegal row index %d", __LINE__, ri));

	caps = a->pm_caps;
	if (a->pm_class != PMC_CLASS_MIPS24K)
		return (EINVAL);
	pe = a->pm_ev;
	for (i = 0; i < mips24k_event_codes_size; i++) {
		if (mips24k_event_codes[i].pe_ev == pe) {
			config = mips24k_event_codes[i].pe_code;
			counter =  mips24k_event_codes[i].pe_counter;
			break;
		}
	}
	if (i == mips24k_event_codes_size)
		return (EINVAL);

	if ((counter != MIPS24K_ALL) && (counter != ri))
		return (EINVAL);

	config <<= MIPS24K_PMC_SELECT;

	if (caps & PMC_CAP_SYSTEM)
		config |= (MIPS24K_PMC_SUPER_ENABLE | 
			   MIPS24K_PMC_KERNEL_ENABLE);
	if (caps & PMC_CAP_USER)
		config |= MIPS24K_PMC_USER_ENABLE;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		config |= MIPS24K_PMC_ENABLE;

	pm->pm_md.pm_mips24k.pm_mips24k_evsel = config;

	PMCDBG(MDP,ALL,2,"mips-allocate ri=%d -> config=0x%x", ri, config);

	return 0;
}


static int
mips24k_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] illegal row index %d", __LINE__, ri));

	pm  = mips24k_pcpu[cpu]->pc_mipspmcs[ri].phw_pmc;
	tmp = mips24k_pmcn_read(ri);
	PMCDBG(MDP,REA,2,"mips-read id=%d -> %jd", ri, tmp);
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = MIPS24K_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	else
		*v = tmp;

	return 0;
}

static int
mips24k_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc *pm;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] illegal row-index %d", __LINE__, ri));

	pm  = mips24k_pcpu[cpu]->pc_mipspmcs[ri].phw_pmc;

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = MIPS24K_RELOAD_COUNT_TO_PERFCTR_VALUE(v);
	
	PMCDBG(MDP,WRI,1,"mips-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	mips24k_pmcn_write(ri, v);

	return 0;
}

static int
mips24k_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] illegal row-index %d", __LINE__, ri));

	phw = &mips24k_pcpu[cpu]->pc_mipspmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[mips,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
	    __LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return 0;
}

static int
mips24k_start_pmc(int cpu, int ri)
{
	uint32_t config;
        struct pmc *pm;
        struct pmc_hw *phw;

	phw    = &mips24k_pcpu[cpu]->pc_mipspmcs[ri];
	pm     = phw->phw_pmc;
	config = pm->pm_md.pm_mips24k.pm_mips24k_evsel;

	/* Enable the PMC. */
	switch (ri) {
	case 0:
		mips_wr_perfcnt0(config);
		break;
	case 1:
		mips_wr_perfcnt2(config);
		break;
	default:
		break;
	}

	return 0;
}

static int
mips24k_stop_pmc(int cpu, int ri)
{
        struct pmc *pm;
        struct pmc_hw *phw;

	phw    = &mips24k_pcpu[cpu]->pc_mipspmcs[ri];
	pm     = phw->phw_pmc;

	/*
	 * Disable the PMCs.
	 *
	 * Clearing the entire register turns the counter off as well
	 * as removes the previously sampled event.
	 */
	switch (ri) {
	case 0:
		mips_wr_perfcnt0(0);
		break;
	case 1:
		mips_wr_perfcnt2(0);
		break;
	default:
		break;
	}
	return 0;
}

static int
mips24k_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] illegal row-index %d", __LINE__, ri));

	phw = &mips24k_pcpu[cpu]->pc_mipspmcs[ri];
	KASSERT(phw->phw_pmc == NULL,
	    ("[mips,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return 0;
}

static int
mips24k_intr(int cpu, struct trapframe *tf)
{
	return 0;
}

static int
mips24k_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char mips24k_name[PMC_NAME_MAX];

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d], illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < mips24k_npmcs,
	    ("[mips,%d] row-index %d out of range", __LINE__, ri));

	phw = &mips24k_pcpu[cpu]->pc_mipspmcs[ri];
	snprintf(mips24k_name, sizeof(mips24k_name), "MIPS-%d", ri);
	if ((error = copystr(mips24k_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return error;
	pi->pm_class = PMC_CLASS_MIPS24K;
	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc	       = NULL;
	}

	return (0);
}

static int
mips24k_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = mips24k_pcpu[cpu]->pc_mipspmcs[ri].phw_pmc;

	return 0;
}

/*
 * XXX don't know what we should do here.
 */
static int
mips24k_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return 0;
}

static int
mips24k_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return 0;
}

static int
mips24k_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int first_ri, i;
	struct pmc_cpu *pc;
	struct mips24k_cpu *pac;
	struct pmc_hw  *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[mips,%d] wrong cpu number %d", __LINE__, cpu));
	PMCDBG(MDP,INI,1,"mips-init cpu=%d", cpu);

	mips24k_pcpu[cpu] = pac = malloc(sizeof(struct mips24k_cpu), M_PMC,
	    M_WAITOK|M_ZERO);
	pac->pc_mipspmcs = malloc(sizeof(struct pmc_hw) * mips24k_npmcs,
	    M_PMC, M_WAITOK|M_ZERO);
	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_MIPS24K].pcd_ri;
	KASSERT(pc != NULL, ("[mips,%d] NULL per-cpu pointer", __LINE__));

	for (i = 0, phw = pac->pc_mipspmcs; i < mips24k_npmcs; i++, phw++) {
		phw->phw_state    = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i);
		phw->phw_pmc      = NULL;
		pc->pc_hwpmcs[i + first_ri] = phw;
	}

	/*
	 * Clear the counter control register which has the effect
	 * of disabling counting.
	 */
	for (i = 0; i < mips24k_npmcs; i++)
		mips24k_pmcn_write(i, 0);

	return 0;
}

static int
mips24k_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	return 0;
}

struct pmc_mdep *
pmc_mips24k_initialize()
{
	struct pmc_mdep *pmc_mdep;
	struct pmc_classdep *pcd;
	
	/* 
	 * Read the counter control registers from CP0 
	 * to determine the number of available PMCs.
	 * The control registers use bit 31 as a "more" bit.
	 *
	 * XXX: With the current macros it is hard to read the
	 * CP0 registers in any varied way.  
	 */
	mips24k_npmcs = 2;
	
	PMCDBG(MDP,INI,1,"mips-init npmcs=%d", mips24k_npmcs);

	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	mips24k_pcpu = malloc(sizeof(struct mips24k_cpu *) * pmc_cpu_max(), M_PMC,
			   M_WAITOK|M_ZERO);

	/* Just one class */
	pmc_mdep = malloc(sizeof(struct pmc_mdep) + sizeof(struct pmc_classdep),
			  M_PMC, M_WAITOK|M_ZERO);

	pmc_mdep->pmd_cputype = PMC_CPU_MIPS_24K;
	pmc_mdep->pmd_nclass  = 1;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_MIPS24K];
	pcd->pcd_caps  = MIPS24K_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_MIPS24K;
	pcd->pcd_num   = mips24k_npmcs;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 32; /* XXX: Fix for 64 bit MIPS */

	pcd->pcd_allocate_pmc   = mips24k_allocate_pmc;
	pcd->pcd_config_pmc     = mips24k_config_pmc;
	pcd->pcd_pcpu_fini      = mips24k_pcpu_fini;
	pcd->pcd_pcpu_init      = mips24k_pcpu_init;
	pcd->pcd_describe       = mips24k_describe;
	pcd->pcd_get_config	= mips24k_get_config;
	pcd->pcd_read_pmc       = mips24k_read_pmc;
	pcd->pcd_release_pmc    = mips24k_release_pmc;
	pcd->pcd_start_pmc      = mips24k_start_pmc;
	pcd->pcd_stop_pmc       = mips24k_stop_pmc;
 	pcd->pcd_write_pmc      = mips24k_write_pmc;

	pmc_mdep->pmd_intr       = mips24k_intr;
	pmc_mdep->pmd_switch_in  = mips24k_switch_in;
	pmc_mdep->pmd_switch_out = mips24k_switch_out;
	
	pmc_mdep->pmd_npmc   += mips24k_npmcs;

	return (pmc_mdep);
}

void
pmc_mips24k_finalize(struct pmc_mdep *md)
{
	(void) md;
}

