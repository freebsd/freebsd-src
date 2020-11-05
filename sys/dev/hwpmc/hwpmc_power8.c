/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 Justin Hibbits
 * Copyright (c) 2020 Leandro Lupori
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/spr.h>
#include <machine/cpu.h>

#include "hwpmc_powerpc.h"

#define	POWER8_MAX_PMCS		6

static struct pmc_ppc_event power8_event_codes[] = {
	{PMC_EV_POWER8_INSTR_COMPLETED,
	    .pe_flags = PMC_FLAG_PMC5,
	    .pe_code = 0x00
	},
	/*
	 * PMC1 can also count cycles, but as PMC6 can only count cycles
	 * it's better to always use it and leave PMC1 free to count
	 * other events.
	 */
	{PMC_EV_POWER8_CYCLES,
	    .pe_flags = PMC_FLAG_PMC6,
	    .pe_code = 0xf0
	},
	{PMC_EV_POWER8_CYCLES_WITH_INSTRS_COMPLETED,
	    .pe_flags = PMC_FLAG_PMC1,
	    .pe_code = 0xf2
	},
	{PMC_EV_POWER8_FPU_INSTR_COMPLETED,
	    .pe_flags = PMC_FLAG_PMC1,
	    .pe_code = 0xf4
	},
	{PMC_EV_POWER8_ERAT_INSTR_MISS,
	    .pe_flags = PMC_FLAG_PMC1,
	    .pe_code = 0xf6
	},
	{PMC_EV_POWER8_CYCLES_IDLE,
	    .pe_flags = PMC_FLAG_PMC1,
	    .pe_code = 0xf8
	},
	{PMC_EV_POWER8_CYCLES_WITH_ANY_THREAD_RUNNING,
	    .pe_flags = PMC_FLAG_PMC1,
	    .pe_code = 0xfa
	},
	{PMC_EV_POWER8_STORE_COMPLETED,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xf0
	},
	{PMC_EV_POWER8_INSTR_DISPATCHED,
	    .pe_flags = PMC_FLAG_PMC2 | PMC_FLAG_PMC3,
	    .pe_code = 0xf2
	},
	{PMC_EV_POWER8_CYCLES_RUNNING,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xf4
	},
	{PMC_EV_POWER8_ERAT_DATA_MISS,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xf6
	},
	{PMC_EV_POWER8_EXTERNAL_INTERRUPT,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xf8
	},
	{PMC_EV_POWER8_BRANCH_TAKEN,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xfa
	},
	{PMC_EV_POWER8_L1_INSTR_MISS,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xfc
	},
	{PMC_EV_POWER8_L2_LOAD_MISS,
	    .pe_flags = PMC_FLAG_PMC2,
	    .pe_code = 0xfe
	},
	{PMC_EV_POWER8_STORE_NO_REAL_ADDR,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xf0
	},
	{PMC_EV_POWER8_INSTR_COMPLETED_WITH_ALL_THREADS_RUNNING,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xf4
	},
	{PMC_EV_POWER8_L1_LOAD_MISS,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xf6
	},
	{PMC_EV_POWER8_TIMEBASE_EVENT,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xf8
	},
	{PMC_EV_POWER8_L3_INSTR_MISS,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xfa
	},
	{PMC_EV_POWER8_TLB_DATA_MISS,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xfc
	},
	{PMC_EV_POWER8_L3_LOAD_MISS,
	    .pe_flags = PMC_FLAG_PMC3,
	    .pe_code = 0xfe
	},
	{PMC_EV_POWER8_LOAD_NO_REAL_ADDR,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xf0
	},
	{PMC_EV_POWER8_CYCLES_WITH_INSTRS_DISPATCHED,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xf2
	},
	{PMC_EV_POWER8_CYCLES_RUNNING_PURR_INC,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xf4
	},
	{PMC_EV_POWER8_BRANCH_MISPREDICTED,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xf6
	},
	{PMC_EV_POWER8_PREFETCHED_INSTRS_DISCARDED,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xf8
	},
	{PMC_EV_POWER8_INSTR_COMPLETED_RUNNING,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xfa
	},
	{PMC_EV_POWER8_TLB_INSTR_MISS,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xfc
	},
	{PMC_EV_POWER8_CACHE_LOAD_MISS,
	    .pe_flags = PMC_FLAG_PMC4,
	    .pe_code = 0xfe
	}
};
static size_t power8_event_codes_size = nitems(power8_event_codes);

static void
power8_set_pmc(int cpu, int ri, int config)
{
	register_t mmcr;

	/* Select event */
	switch (ri) {
	case 0:
	case 1:
	case 2:
	case 3:
		mmcr = mfspr(SPR_MMCR1);
		mmcr &= ~SPR_MMCR1_P8_PMCNSEL_MASK(ri);
		mmcr |= SPR_MMCR1_P8_PMCNSEL(ri, config & ~POWERPC_PMC_ENABLE);
		mtspr(SPR_MMCR1, mmcr);
		break;
	}

	/*
	 * By default, freeze counter in all states.
	 * If counter is being started, unfreeze it in selected states.
	 */
	mmcr = mfspr(SPR_MMCR2) | SPR_MMCR2_FCNHSP(ri);
	if (config != PMCN_NONE) {
		if (config & POWERPC_PMC_USER_ENABLE)
			mmcr &= ~(SPR_MMCR2_FCNP0(ri) |
			    SPR_MMCR2_FCNP1(ri));
		if (config & POWERPC_PMC_KERNEL_ENABLE)
			mmcr &= ~(SPR_MMCR2_FCNH(ri) |
			    SPR_MMCR2_FCNS(ri));
	}
	mtspr(SPR_MMCR2, mmcr);
}

static int
power8_pcpu_init(struct pmc_mdep *md, int cpu)
{
	register_t mmcr0;
	int i;

	powerpc_pcpu_init(md, cpu);

	/* Freeze all counters before modifying PMC registers */
	mmcr0 = mfspr(SPR_MMCR0) | SPR_MMCR0_FC;
	mtspr(SPR_MMCR0, mmcr0);

	/*
	 * Now setup MMCR0:
	 *  - PMAO=0: clear alerts
	 *  - FCPC=0, FCP=0: don't freeze counters in problem state
	 *  - FCECE: Freeze Counters on Enabled Condition or Event
	 *  - PMC1CE/PMCNCE: PMC1/N Condition Enable
	 */
	mmcr0 &= ~(SPR_MMCR0_PMAO | SPR_MMCR0_FCPC | SPR_MMCR0_FCP);
	mmcr0 |= SPR_MMCR0_FCECE | SPR_MMCR0_PMC1CE | SPR_MMCR0_PMCNCE;
	mtspr(SPR_MMCR0, mmcr0);

	/* Clear all PMCs to prevent enabled condition interrupts */
	for (i = 0; i < POWER8_MAX_PMCS; i++)
		powerpc_pmcn_write(i, 0);

	/* Disable events in PMCs 1-4 */
	mtspr(SPR_MMCR1, mfspr(SPR_MMCR1) & ~SPR_MMCR1_P8_PMCSEL_ALL);

	/* Freeze each counter, in all states */
	mtspr(SPR_MMCR2, mfspr(SPR_MMCR2) |
	    SPR_MMCR2_FCNHSP(0) | SPR_MMCR2_FCNHSP(1) | SPR_MMCR2_FCNHSP(2) |
	    SPR_MMCR2_FCNHSP(3) | SPR_MMCR2_FCNHSP(4) | SPR_MMCR2_FCNHSP(5));

	/* Enable interrupts, unset global freeze */
	mmcr0 &= ~SPR_MMCR0_FC;
	mmcr0 |= SPR_MMCR0_PMAE;
	mtspr(SPR_MMCR0, mmcr0);
	return (0);
}

static int
power8_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	register_t mmcr0;

	/* Freeze counters, disable interrupts */
	mmcr0 = mfspr(SPR_MMCR0);
	mmcr0 &= ~SPR_MMCR0_PMAE;
	mmcr0 |= SPR_MMCR0_FC;
	mtspr(SPR_MMCR0, mmcr0);

	return (powerpc_pcpu_fini(md, cpu));
}

static void
power8_resume_pmc(bool ie)
{
	register_t mmcr0;

	/* Unfreeze counters and re-enable PERF exceptions if requested. */
	mmcr0 = mfspr(SPR_MMCR0);
	mmcr0 &= ~(SPR_MMCR0_FC | SPR_MMCR0_PMAO | SPR_MMCR0_PMAE);
	if (ie)
		mmcr0 |= SPR_MMCR0_PMAE;
	mtspr(SPR_MMCR0, mmcr0);
}

int
pmc_power8_initialize(struct pmc_mdep *pmc_mdep)
{
	struct pmc_classdep *pcd;

	pmc_mdep->pmd_cputype = PMC_CPU_PPC_POWER8;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_POWERPC];
	pcd->pcd_caps  = POWERPC_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_POWER8;
	pcd->pcd_num   = POWER8_MAX_PMCS;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 32;

	pcd->pcd_pcpu_init      = power8_pcpu_init;
	pcd->pcd_pcpu_fini      = power8_pcpu_fini;
	pcd->pcd_allocate_pmc   = powerpc_allocate_pmc;
	pcd->pcd_release_pmc    = powerpc_release_pmc;
	pcd->pcd_start_pmc      = powerpc_start_pmc;
	pcd->pcd_stop_pmc       = powerpc_stop_pmc;
	pcd->pcd_get_config     = powerpc_get_config;
	pcd->pcd_config_pmc     = powerpc_config_pmc;
	pcd->pcd_describe       = powerpc_describe;
	pcd->pcd_read_pmc       = powerpc_read_pmc;
	pcd->pcd_write_pmc      = powerpc_write_pmc;

	pmc_mdep->pmd_npmc     += POWER8_MAX_PMCS;
	pmc_mdep->pmd_intr      = powerpc_pmc_intr;

	ppc_event_codes = power8_event_codes;
	ppc_event_codes_size = power8_event_codes_size;
	ppc_event_first = PMC_EV_POWER8_FIRST;
	ppc_event_last = PMC_EV_POWER8_LAST;
	ppc_max_pmcs = POWER8_MAX_PMCS;

	powerpc_set_pmc = power8_set_pmc;
	powerpc_pmcn_read = powerpc_pmcn_read_default;
	powerpc_pmcn_write = powerpc_pmcn_write_default;
	powerpc_resume_pmc = power8_resume_pmc;

	return (0);
}
