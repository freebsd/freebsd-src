/*-
 * Copyright (c) 2015 Justin Hibbits
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

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/cpu.h>

#include <ddb/ddb.h>

#include "hwpmc_powerpc.h"

struct e500_event_code_map {
	enum pmc_event	pe_ev;       /* enum value */
	uint8_t         pe_counter_mask;  /* Which counter this can be counted in. */
	uint8_t		pe_code;     /* numeric code */
	uint8_t		pe_cpu;	     /* e500 core (v1,v2,mc), mask */
};

#define E500_MAX_PMCS	4
#define PMC_PPC_MASK0	0
#define PMC_PPC_MASK1	1
#define PMC_PPC_MASK2	2
#define PMC_PPC_MASK3	3
#define PMC_PPC_MASK_ALL	0x0f
#define PMC_PPC_E500V1		1
#define PMC_PPC_E500V2		2
#define PMC_PPC_E500MC		4
#define PMC_PPC_E500_ANY	7
#define PMC_E500_EVENT(id, mask, number, core) \
	[PMC_EV_E500_##id - PMC_EV_E500_FIRST] = \
	    { .pe_ev = PMC_EV_E500_##id, .pe_counter_mask = mask, \
	      .pe_code = number, .pe_cpu = core }
#define PMC_E500MC_ONLY(id, number) \
	PMC_E500_EVENT(id, PMC_PPC_MASK_ALL, number, PMC_PPC_E500MC)
#define PMC_E500_COMMON(id, number) \
	PMC_E500_EVENT(id, PMC_PPC_MASK_ALL, number, PMC_PPC_E500_ANY)

static struct e500_event_code_map e500_event_codes[] = {
	PMC_E500_COMMON(CYCLES, 1),
	PMC_E500_COMMON(INSTR_COMPLETED, 2),
	PMC_E500_COMMON(UOPS_COMPLETED, 3),
	PMC_E500_COMMON(INSTR_FETCHED, 4),
	PMC_E500_COMMON(UOPS_DECODED, 5),
	PMC_E500_COMMON(PM_EVENT_TRANSITIONS, 6),
	PMC_E500_COMMON(PM_EVENT_CYCLES, 7),
	PMC_E500_COMMON(BRANCH_INSTRS_COMPLETED, 8),
	PMC_E500_COMMON(LOAD_UOPS_COMPLETED, 9),
	PMC_E500_COMMON(STORE_UOPS_COMPLETED, 10),
	PMC_E500_COMMON(CQ_REDIRECTS, 11),
	PMC_E500_COMMON(BRANCHES_FINISHED, 12),
	PMC_E500_COMMON(TAKEN_BRANCHES_FINISHED, 13),
	PMC_E500_COMMON(FINISHED_UNCOND_BRANCHES_MISS_BTB, 14),
	PMC_E500_COMMON(BRANCH_MISPRED, 15),
	PMC_E500_COMMON(BTB_BRANCH_MISPRED_FROM_DIRECTION, 16),
	PMC_E500_COMMON(BTB_HITS_PSEUDO_HITS, 17),
	PMC_E500_COMMON(CYCLES_DECODE_STALLED, 18),
	PMC_E500_COMMON(CYCLES_ISSUE_STALLED, 19),
	PMC_E500_COMMON(CYCLES_BRANCH_ISSUE_STALLED, 20),
	PMC_E500_COMMON(CYCLES_SU1_SCHED_STALLED, 21),
	PMC_E500_COMMON(CYCLES_SU2_SCHED_STALLED, 22),
	PMC_E500_COMMON(CYCLES_MU_SCHED_STALLED, 23),
	PMC_E500_COMMON(CYCLES_LRU_SCHED_STALLED, 24),
	PMC_E500_COMMON(CYCLES_BU_SCHED_STALLED, 25),
	PMC_E500_COMMON(TOTAL_TRANSLATED, 26),
	PMC_E500_COMMON(LOADS_TRANSLATED, 27),
	PMC_E500_COMMON(STORES_TRANSLATED, 28),
	PMC_E500_COMMON(TOUCHES_TRANSLATED, 29),
	PMC_E500_COMMON(CACHEOPS_TRANSLATED, 30),
	PMC_E500_COMMON(CACHE_INHIBITED_ACCESS_TRANSLATED, 31),
	PMC_E500_COMMON(GUARDED_LOADS_TRANSLATED, 32),
	PMC_E500_COMMON(WRITE_THROUGH_STORES_TRANSLATED, 33),
	PMC_E500_COMMON(MISALIGNED_LOAD_STORE_ACCESS_TRANSLATED, 34),
	PMC_E500_COMMON(TOTAL_ALLOCATED_TO_DLFB, 35),
	PMC_E500_COMMON(LOADS_TRANSLATED_ALLOCATED_TO_DLFB, 36),
	PMC_E500_COMMON(STORES_COMPLETED_ALLOCATED_TO_DLFB, 37),
	PMC_E500_COMMON(TOUCHES_TRANSLATED_ALLOCATED_TO_DLFB, 38),
	PMC_E500_COMMON(STORES_COMPLETED, 39),
	PMC_E500_COMMON(DATA_L1_CACHE_LOCKS, 40),
	PMC_E500_COMMON(DATA_L1_CACHE_RELOADS, 41),
	PMC_E500_COMMON(DATA_L1_CACHE_CASTOUTS, 42),
	PMC_E500_COMMON(LOAD_MISS_DLFB_FULL, 43),
	PMC_E500_COMMON(LOAD_MISS_LDQ_FULL, 44),
	PMC_E500_COMMON(LOAD_GUARDED_MISS, 45),
	PMC_E500_COMMON(STORE_TRANSLATE_WHEN_QUEUE_FULL, 46),
	PMC_E500_COMMON(ADDRESS_COLLISION, 47),
	PMC_E500_COMMON(DATA_MMU_MISS, 48),
	PMC_E500_COMMON(DATA_MMU_BUSY, 49),
	PMC_E500_COMMON(PART2_MISALIGNED_CACHE_ACCESS, 50),
	PMC_E500_COMMON(LOAD_MISS_DLFB_FULL_CYCLES, 51),
	PMC_E500_COMMON(LOAD_MISS_LDQ_FULL_CYCLES, 52),
	PMC_E500_COMMON(LOAD_GUARDED_MISS_CYCLES, 53),
	PMC_E500_COMMON(STORE_TRANSLATE_WHEN_QUEUE_FULL_CYCLES, 54),
	PMC_E500_COMMON(ADDRESS_COLLISION_CYCLES, 55),
	PMC_E500_COMMON(DATA_MMU_MISS_CYCLES, 56),
	PMC_E500_COMMON(DATA_MMU_BUSY_CYCLES, 57),
	PMC_E500_COMMON(PART2_MISALIGNED_CACHE_ACCESS_CYCLES, 58),
	PMC_E500_COMMON(INSTR_L1_CACHE_LOCKS, 59),
	PMC_E500_COMMON(INSTR_L1_CACHE_RELOADS, 60),
	PMC_E500_COMMON(INSTR_L1_CACHE_FETCHES, 61),
	PMC_E500_COMMON(INSTR_MMU_TLB4K_RELOADS, 62),
	PMC_E500_COMMON(INSTR_MMU_VSP_RELOADS, 63),
	PMC_E500_COMMON(DATA_MMU_TLB4K_RELOADS, 64),
	PMC_E500_COMMON(DATA_MMU_VSP_RELOADS, 65),
	PMC_E500_COMMON(L2MMU_MISSES, 66),
	PMC_E500_COMMON(BIU_MASTER_REQUESTS, 67),
	PMC_E500_COMMON(BIU_MASTER_INSTR_SIDE_REQUESTS, 68),
	PMC_E500_COMMON(BIU_MASTER_DATA_SIDE_REQUESTS, 69),
	PMC_E500_COMMON(BIU_MASTER_DATA_SIDE_CASTOUT_REQUESTS, 70),
	PMC_E500_COMMON(BIU_MASTER_RETRIES, 71),
	PMC_E500_COMMON(SNOOP_REQUESTS, 72),
	PMC_E500_COMMON(SNOOP_HITS, 73),
	PMC_E500_COMMON(SNOOP_PUSHES, 74),
	PMC_E500_COMMON(SNOOP_RETRIES, 75),
	PMC_E500_EVENT(DLFB_LOAD_MISS_CYCLES, PMC_PPC_MASK0|PMC_PPC_MASK1,
	    76, PMC_PPC_E500_ANY),
	PMC_E500_EVENT(ILFB_FETCH_MISS_CYCLES, PMC_PPC_MASK0|PMC_PPC_MASK1,
	    77, PMC_PPC_E500_ANY),
	PMC_E500_EVENT(EXT_INPU_INTR_LATENCY_CYCLES, PMC_PPC_MASK0|PMC_PPC_MASK1,
	    78, PMC_PPC_E500_ANY),
	PMC_E500_EVENT(CRIT_INPUT_INTR_LATENCY_CYCLES, PMC_PPC_MASK0|PMC_PPC_MASK1,
	    79, PMC_PPC_E500_ANY),
	PMC_E500_EVENT(EXT_INPUT_INTR_PENDING_LATENCY_CYCLES,
	    PMC_PPC_MASK0|PMC_PPC_MASK1, 80, PMC_PPC_E500_ANY),
	PMC_E500_EVENT(CRIT_INPUT_INTR_PENDING_LATENCY_CYCLES,
	    PMC_PPC_MASK0|PMC_PPC_MASK1, 81, PMC_PPC_E500_ANY),
	PMC_E500_COMMON(PMC0_OVERFLOW, 82),
	PMC_E500_COMMON(PMC1_OVERFLOW, 83),
	PMC_E500_COMMON(PMC2_OVERFLOW, 84),
	PMC_E500_COMMON(PMC3_OVERFLOW, 85),
	PMC_E500_COMMON(INTERRUPTS_TAKEN, 86),
	PMC_E500_COMMON(EXT_INPUT_INTR_TAKEN, 87),
	PMC_E500_COMMON(CRIT_INPUT_INTR_TAKEN, 88),
	PMC_E500_COMMON(SYSCALL_TRAP_INTR, 89),
	PMC_E500_EVENT(TLB_BIT_TRANSITIONS, PMC_PPC_MASK_ALL, 90,
	    PMC_PPC_E500V2 | PMC_PPC_E500MC),
	PMC_E500MC_ONLY(L2_LINEFILL_BUFFER, 91),
	PMC_E500MC_ONLY(LV2_VS, 92),
	PMC_E500MC_ONLY(CASTOUTS_RELEASED, 93),
	PMC_E500MC_ONLY(INTV_ALLOCATIONS, 94),
	PMC_E500MC_ONLY(DLFB_RETRIES_TO_MBAR, 95),
	PMC_E500MC_ONLY(STORE_RETRIES, 96),
	PMC_E500MC_ONLY(STASH_L1_HITS, 97),
	PMC_E500MC_ONLY(STASH_L2_HITS, 98),
	PMC_E500MC_ONLY(STASH_BUSY_1, 99),
	PMC_E500MC_ONLY(STASH_BUSY_2, 100),
	PMC_E500MC_ONLY(STASH_BUSY_3, 101),
	PMC_E500MC_ONLY(STASH_HITS, 102),
	PMC_E500MC_ONLY(STASH_HIT_DLFB, 103),
	PMC_E500MC_ONLY(STASH_REQUESTS, 106),
	PMC_E500MC_ONLY(STASH_REQUESTS_L1, 107),
	PMC_E500MC_ONLY(STASH_REQUESTS_L2, 108),
	PMC_E500MC_ONLY(STALLS_NO_CAQ_OR_COB, 109),
	PMC_E500MC_ONLY(L2_CACHE_ACCESSES, 110),
	PMC_E500MC_ONLY(L2_HIT_CACHE_ACCESSES, 111),
	PMC_E500MC_ONLY(L2_CACHE_DATA_ACCESSES, 112),
	PMC_E500MC_ONLY(L2_CACHE_DATA_HITS, 113),
	PMC_E500MC_ONLY(L2_CACHE_INSTR_ACCESSES, 114),
	PMC_E500MC_ONLY(L2_CACHE_INSTR_HITS, 115),
	PMC_E500MC_ONLY(L2_CACHE_ALLOCATIONS, 116),
	PMC_E500MC_ONLY(L2_CACHE_DATA_ALLOCATIONS, 117),
	PMC_E500MC_ONLY(L2_CACHE_DIRTY_DATA_ALLOCATIONS, 118),
	PMC_E500MC_ONLY(L2_CACHE_INSTR_ALLOCATIONS, 119),
	PMC_E500MC_ONLY(L2_CACHE_UPDATES, 120),
	PMC_E500MC_ONLY(L2_CACHE_CLEAN_UPDATES, 121),
	PMC_E500MC_ONLY(L2_CACHE_DIRTY_UPDATES, 122),
	PMC_E500MC_ONLY(L2_CACHE_CLEAN_REDUNDANT_UPDATES, 123),
	PMC_E500MC_ONLY(L2_CACHE_DIRTY_REDUNDANT_UPDATES, 124),
	PMC_E500MC_ONLY(L2_CACHE_LOCKS, 125),
	PMC_E500MC_ONLY(L2_CACHE_CASTOUTS, 126),
	PMC_E500MC_ONLY(L2_CACHE_DATA_DIRTY_HITS, 127),
	PMC_E500MC_ONLY(INSTR_LFB_WENT_HIGH_PRIORITY, 128),
	PMC_E500MC_ONLY(SNOOP_THROTTLING_TURNED_ON, 129),
	PMC_E500MC_ONLY(L2_CLEAN_LINE_INVALIDATIONS, 130),
	PMC_E500MC_ONLY(L2_INCOHERENT_LINE_INVALIDATIONS, 131),
	PMC_E500MC_ONLY(L2_COHERENT_LINE_INVALIDATIONS, 132),
	PMC_E500MC_ONLY(COHERENT_LOOKUP_MISS_DUE_TO_VALID_BUT_INCOHERENT_MATCHES, 133),
	PMC_E500MC_ONLY(IAC1S_DETECTED, 140),
	PMC_E500MC_ONLY(IAC2S_DETECTED, 141),
	PMC_E500MC_ONLY(DAC1S_DTECTED, 144),
	PMC_E500MC_ONLY(DAC2S_DTECTED, 145),
	PMC_E500MC_ONLY(DVT0_DETECTED, 148),
	PMC_E500MC_ONLY(DVT1_DETECTED, 149),
	PMC_E500MC_ONLY(DVT2_DETECTED, 150),
	PMC_E500MC_ONLY(DVT3_DETECTED, 151),
	PMC_E500MC_ONLY(DVT4_DETECTED, 152),
	PMC_E500MC_ONLY(DVT5_DETECTED, 153),
	PMC_E500MC_ONLY(DVT6_DETECTED, 154),
	PMC_E500MC_ONLY(DVT7_DETECTED, 155),
	PMC_E500MC_ONLY(CYCLES_COMPLETION_STALLED_NEXUS_FIFO_FULL, 156),
	PMC_E500MC_ONLY(FPU_DOUBLE_PUMP, 160),
	PMC_E500MC_ONLY(FPU_FINISH, 161),
	PMC_E500MC_ONLY(FPU_DIVIDE_CYCLES, 162),
	PMC_E500MC_ONLY(FPU_DENORM_INPUT_CYCLES, 163),
	PMC_E500MC_ONLY(FPU_RESULT_STALL_CYCLES, 164),
	PMC_E500MC_ONLY(FPU_FPSCR_FULL_STALL, 165),
	PMC_E500MC_ONLY(FPU_PIPE_SYNC_STALLS, 166),
	PMC_E500MC_ONLY(FPU_INPUT_DATA_STALLS, 167),
	PMC_E500MC_ONLY(DECORATED_LOADS, 176),
	PMC_E500MC_ONLY(DECORATED_STORES, 177),
	PMC_E500MC_ONLY(LOAD_RETRIES, 178),
	PMC_E500MC_ONLY(STWCX_SUCCESSES, 179),
	PMC_E500MC_ONLY(STWCX_FAILURES, 180),
};

static pmc_value_t
e500_pmcn_read(unsigned int pmc)
{
	switch (pmc) {
	case 0:
		return (mfpmr(PMR_PMC0));
	case 1:
		return (mfpmr(PMR_PMC1));
	case 2:
		return (mfpmr(PMR_PMC2));
	case 3:
		return (mfpmr(PMR_PMC3));
	default:
		panic("Invalid PMC number: %d\n", pmc);
	}
}

static void
e500_pmcn_write(unsigned int pmc, uint32_t val)
{
	switch (pmc) {
	case 0:
		mtpmr(PMR_PMC0, val);
		break;
	case 1:
		mtpmr(PMR_PMC1, val);
		break;
	case 2:
		mtpmr(PMR_PMC2, val);
		break;
	case 3:
		mtpmr(PMR_PMC3, val);
		break;
	default:
		panic("Invalid PMC number: %d\n", pmc);
	}
}

static void
e500_set_pmc(int cpu, int ri, int config)
{
	struct pmc *pm;
	struct pmc_hw *phw;
	register_t pmc_pmlc;

	phw    = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];
	pm     = phw->phw_pmc;
	config &= ~POWERPC_PMC_ENABLE;

	if (config != PMCN_NONE) {
		if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
			config |= PMLCax_CE;

		/* Enable the PMC. */
		switch (ri) {
		case 0:
			mtpmr(PMR_PMLCa0, config);
			break;
		case 1:
			mtpmr(PMR_PMLCa1, config);
			break;
		case 2:
			mtpmr(PMR_PMLCa2, config);
			break;
		case 3:
			mtpmr(PMR_PMLCa3, config);
			break;
		}
	} else {
		/* Disable the PMC. */
		switch (ri) {
		case 0:
			pmc_pmlc = mfpmr(PMR_PMLCa0);
			pmc_pmlc |= PMLCax_FC;
			mtpmr(PMR_PMLCa0, pmc_pmlc);
			break;
		case 1:
			pmc_pmlc = mfpmr(PMR_PMLCa1);
			pmc_pmlc |= PMLCax_FC;
			mtpmr(PMR_PMLCa1, pmc_pmlc);
			break;
		case 2:
			pmc_pmlc = mfpmr(PMR_PMLCa2);
			pmc_pmlc |= PMLCax_FC;
			mtpmr(PMR_PMLCa2, pmc_pmlc);
			break;
		case 3:
			pmc_pmlc = mfpmr(PMR_PMLCa3);
			pmc_pmlc |= PMLCax_FC;
			mtpmr(PMR_PMLCa3, pmc_pmlc);
			break;
		}
	}
}

static int
e500_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int i;

	powerpc_pcpu_init(md, cpu);

	/* Freeze all counters. */
	mtpmr(PMR_PMGC0, PMGC_FAC | PMGC_PMIE | PMGC_FCECE);

	for (i = 0; i < E500_MAX_PMCS; i++)
		/* Initialize the PMC to stopped */
		e500_set_pmc(cpu, i, PMCN_NONE);

	/* Unfreeze global register. */
	mtpmr(PMR_PMGC0, PMGC_PMIE | PMGC_FCECE);

	return (0);
}

static int
e500_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	uint32_t pmgc0 = mfpmr(PMR_PMGC0);

	pmgc0 |= PMGC_FAC;
	mtpmr(PMR_PMGC0, pmgc0);
	mtmsr(mfmsr() & ~PSL_PMM);

	return (powerpc_pcpu_fini(md, cpu));
}

static int
e500_allocate_pmc(int cpu, int ri, struct pmc *pm,
  const struct pmc_op_pmcallocate *a)
{
	enum pmc_event pe;
	uint32_t caps, config, counter;
	struct e500_event_code_map *ev;
	uint16_t vers;
	uint8_t pe_cpu_mask;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[powerpc,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < E500_MAX_PMCS,
	    ("[powerpc,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != PMC_CLASS_E500)
		return (EINVAL);

	caps = a->pm_caps;

	pe = a->pm_ev;
	config = PMLCax_FCS | PMLCax_FCU |
	    PMLCax_FCM1 | PMLCax_FCM1;

	if (pe < PMC_EV_E500_FIRST || pe > PMC_EV_E500_LAST)
		return (EINVAL);

	ev = &e500_event_codes[pe-PMC_EV_E500_FIRST];
	if (ev->pe_code == 0)
		return (EINVAL);

	vers = mfpvr() >> 16;
	switch (vers) {
	case FSL_E500v1:
		pe_cpu_mask = ev->pe_cpu & PMC_PPC_E500V1;
		break;
	case FSL_E500v2:
		pe_cpu_mask = ev->pe_cpu & PMC_PPC_E500V2;
		break;
	case FSL_E500mc:
	case FSL_E5500:
		pe_cpu_mask = ev->pe_cpu & PMC_PPC_E500MC;
		break;
	}
	if (pe_cpu_mask == 0)
		return (EINVAL);

	config |= PMLCax_EVENT(ev->pe_code);
	counter =  ev->pe_counter_mask;
	if ((counter & (1 << ri)) == 0)
		return (EINVAL);

	if (caps & PMC_CAP_SYSTEM)
		config &= ~PMLCax_FCS;
	if (caps & PMC_CAP_USER)
		config &= ~PMLCax_FCU;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		config &= ~(PMLCax_FCS|PMLCax_FCU);

	pm->pm_md.pm_powerpc.pm_powerpc_evsel = config;

	PMCDBG2(MDP,ALL,2,"powerpc-allocate ri=%d -> config=0x%x", ri, config);

	return 0;
}

static void
e500_resume_pmc(bool ie)
{
	/* Re-enable PERF exceptions. */
	if (ie)
		mtpmr(PMR_PMGC0, (mfpmr(PMR_PMGC0) & ~PMGC_FAC) | PMGC_PMIE);
}

int
pmc_e500_initialize(struct pmc_mdep *pmc_mdep)
{
	struct pmc_classdep *pcd;

	pmc_mdep->pmd_cputype = PMC_CPU_PPC_E500;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_POWERPC];
	pcd->pcd_caps  = POWERPC_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_E500;
	pcd->pcd_num   = E500_MAX_PMCS;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 32;

	pcd->pcd_allocate_pmc   = e500_allocate_pmc;
	pcd->pcd_config_pmc     = powerpc_config_pmc;
	pcd->pcd_pcpu_fini      = e500_pcpu_fini;
	pcd->pcd_pcpu_init      = e500_pcpu_init;
	pcd->pcd_describe       = powerpc_describe;
	pcd->pcd_get_config     = powerpc_get_config;
	pcd->pcd_read_pmc       = powerpc_read_pmc;
	pcd->pcd_release_pmc    = powerpc_release_pmc;
	pcd->pcd_start_pmc      = powerpc_start_pmc;
	pcd->pcd_stop_pmc       = powerpc_stop_pmc;
	pcd->pcd_write_pmc      = powerpc_write_pmc;

	pmc_mdep->pmd_npmc   += E500_MAX_PMCS;
	pmc_mdep->pmd_intr   =  powerpc_pmc_intr;

	ppc_max_pmcs = E500_MAX_PMCS;

	powerpc_set_pmc = e500_set_pmc;
	powerpc_pmcn_read = e500_pmcn_read;
	powerpc_pmcn_write = e500_pmcn_write;
	powerpc_resume_pmc = e500_resume_pmc;

	return (0);
}
