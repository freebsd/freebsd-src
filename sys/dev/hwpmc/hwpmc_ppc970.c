/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2013 Justin Hibbits
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
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/pmc_mdep.h>
#include <machine/spr.h>
#include <machine/cpu.h>

#include "hwpmc_powerpc.h"

#define PPC970_MAX_PMCS		8
#define PMC_PPC970_FLAG_PMCS	0x000000ff

/* MMCR0, PMC1 is 8 bytes in, PMC2 is 1 byte in. */
#define PPC970_SET_MMCR0_PMCSEL(r, x, i) \
	((r & ~(0x1f << (7 * (1 - i) + 1))) | (x << (7 * (1 - i) + 1)))
/* MMCR1 has 6 PMC*SEL items (PMC3->PMC8), in sequence. */
#define PPC970_SET_MMCR1_PMCSEL(r, x, i) \
	((r & ~(0x1f << (5 * (7 - i) + 2))) | (x << (5 * (7 - i) + 2)))

/* How PMC works on PPC970:
 *
 * Any PMC can count a direct event.  Indirect events are handled specially.
 * Direct events: As published.
 *
 * Encoding 00 000 -- Add byte lane bit counters
 *   MMCR1[24:31] -- select bit matching PMC being an adder.
 * Bus events:
 * PMCxSEL: 1x -- select from byte lane: 10 == lower lane (0/1), 11 == upper
 * lane (2/3).
 * PMCxSEL[2:4] -- bit in the byte lane selected.
 *
 * PMC[1,2,5,6] == lane 0/lane 2
 * PMC[3,4,7,8] == lane 1,3
 *
 *
 * Lanes:
 * Lane 0 -- TTM0(FPU,ISU,IFU,VPU)
 *           TTM1(IDU,ISU,STS)
 *           LSU0 byte 0
 *           LSU1 byte 0
 * Lane 1 -- TTM0
 *           TTM1
 *           LSU0 byte 1
 *           LSU1 byte 1
 * Lane 2 -- TTM0
 *           TTM1
 *           LSU0 byte 2
 *           LSU1 byte 2 or byte 6
 * Lane 3 -- TTM0
 *           TTM1
 *           LSU0 byte 3
 *           LSU1 byte 3 or byte 7
 *
 * Adders:
 *  Add byte lane for PMC (above), bit 0+4, 1+5, 2+6, 3+7
 */

static struct pmc_ppc_event ppc970_event_codes[] = {
	{PMC_EV_PPC970_INSTR_COMPLETED,
	    .pe_flags = PMC_PPC970_FLAG_PMCS,
	    .pe_code = 0x09
	},
	{PMC_EV_PPC970_MARKED_GROUP_DISPATCH,
		.pe_flags = PMC_FLAG_PMC1,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_MARKED_STORE_COMPLETED,
		.pe_flags = PMC_FLAG_PMC1,
		.pe_code = 0x03
	},
	{PMC_EV_PPC970_GCT_EMPTY,
		.pe_flags = PMC_FLAG_PMC1,
		.pe_code = 0x04
	},
	{PMC_EV_PPC970_RUN_CYCLES,
		.pe_flags = PMC_FLAG_PMC1,
		.pe_code = 0x05
	},
	{PMC_EV_PPC970_OVERFLOW,
		.pe_flags = PMC_PPC970_FLAG_PMCS,
		.pe_code = 0x0a
	},
	{PMC_EV_PPC970_CYCLES,
		.pe_flags = PMC_PPC970_FLAG_PMCS,
		.pe_code = 0x0f
	},
	{PMC_EV_PPC970_THRESHOLD_TIMEOUT,
		.pe_flags = PMC_FLAG_PMC2,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_GROUP_DISPATCH,
		.pe_flags = PMC_FLAG_PMC2,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_BR_MARKED_INSTR_FINISH,
		.pe_flags = PMC_FLAG_PMC2,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_GCT_EMPTY_BY_SRQ_FULL,
		.pe_flags = PMC_FLAG_PMC2,
		.pe_code = 0xb
	},
	{PMC_EV_PPC970_STOP_COMPLETION,
		.pe_flags = PMC_FLAG_PMC3,
		.pe_code = 0x1
	},
	{PMC_EV_PPC970_LSU_EMPTY,
		.pe_flags = PMC_FLAG_PMC3,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_MARKED_STORE_WITH_INTR,
		.pe_flags = PMC_FLAG_PMC3,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_CYCLES_IN_SUPER,
		.pe_flags = PMC_FLAG_PMC3,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_VPU_MARKED_INSTR_COMPLETED,
		.pe_flags = PMC_FLAG_PMC3,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_FXU0_IDLE_FXU1_BUSY,
		.pe_flags = PMC_FLAG_PMC4,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_SRQ_EMPTY,
		.pe_flags = PMC_FLAG_PMC4,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_MARKED_GROUP_COMPLETED,
		.pe_flags = PMC_FLAG_PMC4,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_CR_MARKED_INSTR_FINISH,
		.pe_flags = PMC_FLAG_PMC4,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_DISPATCH_SUCCESS,
		.pe_flags = PMC_FLAG_PMC5,
		.pe_code = 0x1
	},
	{PMC_EV_PPC970_FXU0_IDLE_FXU1_IDLE,
		.pe_flags = PMC_FLAG_PMC5,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_ONE_PLUS_INSTR_COMPLETED,
		.pe_flags = PMC_FLAG_PMC5,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_GROUP_MARKED_IDU,
		.pe_flags = PMC_FLAG_PMC5,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_MARKED_GROUP_COMPLETE_TIMEOUT,
		.pe_flags = PMC_FLAG_PMC5,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_FXU0_BUSY_FXU1_BUSY,
		.pe_flags = PMC_FLAG_PMC6,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_MARKED_STORE_SENT_TO_STS,
		.pe_flags = PMC_FLAG_PMC6,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_FXU_MARKED_INSTR_FINISHED,
		.pe_flags = PMC_FLAG_PMC6,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_MARKED_GROUP_ISSUED,
		.pe_flags = PMC_FLAG_PMC6,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_FXU0_BUSY_FXU1_IDLE,
		.pe_flags = PMC_FLAG_PMC7,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_GROUP_COMPLETED,
		.pe_flags = PMC_FLAG_PMC7,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_FPU_MARKED_INSTR_COMPLETED,
		.pe_flags = PMC_FLAG_PMC7,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_MARKED_INSTR_FINISH_ANY_UNIT,
		.pe_flags = PMC_FLAG_PMC7,
		.pe_code = 0x5
	},
	{PMC_EV_PPC970_EXTERNAL_INTERRUPT,
		.pe_flags = PMC_FLAG_PMC8,
		.pe_code = 0x2
	},
	{PMC_EV_PPC970_GROUP_DISPATCH_REJECT,
		.pe_flags = PMC_FLAG_PMC8,
		.pe_code = 0x3
	},
	{PMC_EV_PPC970_LSU_MARKED_INSTR_FINISH,
		.pe_flags = PMC_FLAG_PMC8,
		.pe_code = 0x4
	},
	{PMC_EV_PPC970_TIMEBASE_EVENT,
		.pe_flags = PMC_FLAG_PMC8,
		.pe_code = 0x5
	},
#if 0
	{PMC_EV_PPC970_LSU_COMPLETION_STALL, },
	{PMC_EV_PPC970_FXU_COMPLETION_STALL, },
	{PMC_EV_PPC970_DCACHE_MISS_COMPLETION_STALL, },
	{PMC_EV_PPC970_FPU_COMPLETION_STALL, },
	{PMC_EV_PPC970_FXU_LONG_INSTR_COMPLETION_STALL, },
	{PMC_EV_PPC970_REJECT_COMPLETION_STALL, },
	{PMC_EV_PPC970_FPU_LONG_INSTR_COMPLETION_STALL, },
	{PMC_EV_PPC970_GCT_EMPTY_BY_ICACHE_MISS, },
	{PMC_EV_PPC970_REJECT_COMPLETION_STALL_ERAT_MISS, },
	{PMC_EV_PPC970_GCT_EMPTY_BY_BRANCH_MISS_PREDICT, },
#endif
};
static size_t ppc970_event_codes_size = nitems(ppc970_event_codes);

static void
ppc970_set_pmc(int cpu, int ri, int config)
{
	struct pmc *pm;
	struct pmc_hw *phw;
	register_t pmc_mmcr;
	int config_mask;

	phw = &powerpc_pcpu[cpu]->pc_ppcpmcs[ri];
	pm = phw->phw_pmc;

	if (config == PMCN_NONE)
		config = PMC970N_NONE;

	/*
	 * The mask is inverted (enable is 1) compared to the flags in MMCR0,
	 * which are Freeze flags.
	 */
	config_mask = ~config & POWERPC_PMC_ENABLE;
	config &= ~POWERPC_PMC_ENABLE;

	/*
	 * Disable the PMCs.
	 */
	switch (ri) {
	case 0:
	case 1:
		pmc_mmcr = mfspr(SPR_MMCR0);
		pmc_mmcr = PPC970_SET_MMCR0_PMCSEL(pmc_mmcr, config, ri);
		mtspr(SPR_MMCR0, pmc_mmcr);
		break;
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7:
		pmc_mmcr = mfspr(SPR_MMCR1);
		pmc_mmcr = PPC970_SET_MMCR1_PMCSEL(pmc_mmcr, config, ri);
		mtspr(SPR_MMCR1, pmc_mmcr);
		break;
	}

	if (config != PMC970N_NONE) {
		pmc_mmcr = mfspr(SPR_MMCR0);
		pmc_mmcr &= ~SPR_MMCR0_FC;
		pmc_mmcr |= config_mask;
		mtspr(SPR_MMCR0, pmc_mmcr);
	}
}

static int
ppc970_pcpu_init(struct pmc_mdep *md, int cpu)
{
	powerpc_pcpu_init(md, cpu);

	/* Clear the MMCRs, and set FC, to disable all PMCs. */
	/* 970 PMC is not counted when set to 0x08 */
	mtspr(SPR_MMCR0, SPR_MMCR0_FC | SPR_MMCR0_PMXE |
	    SPR_MMCR0_FCECE | SPR_MMCR0_PMC1CE | SPR_MMCR0_PMCNCE |
	    SPR_MMCR0_PMC1SEL(0x8) | SPR_MMCR0_PMC2SEL(0x8));
	mtspr(SPR_MMCR1, 0x4218420);

	return (0);
}

static int
ppc970_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	register_t mmcr0;

	/* Freeze counters, disable interrupts */
	mmcr0 = mfspr(SPR_MMCR0);
	mmcr0 &= ~SPR_MMCR0_PMXE;
	mmcr0 |= SPR_MMCR0_FC;
	mtspr(SPR_MMCR0, mmcr0);

	return (powerpc_pcpu_fini(md, cpu));
}

static void
ppc970_resume_pmc(bool ie)
{
	register_t mmcr0;

	/* Unfreeze counters and re-enable PERF exceptions if requested. */
	mmcr0 = mfspr(SPR_MMCR0);
	mmcr0 &= ~(SPR_MMCR0_FC | SPR_MMCR0_PMXE);
	if (ie)
		mmcr0 |= SPR_MMCR0_PMXE;
	mtspr(SPR_MMCR0, mmcr0);
}

int
pmc_ppc970_initialize(struct pmc_mdep *pmc_mdep)
{
	struct pmc_classdep *pcd;
	
	pmc_mdep->pmd_cputype = PMC_CPU_PPC_970;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_POWERPC];
	pcd->pcd_caps  = POWERPC_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_PPC970;
	pcd->pcd_num   = PPC970_MAX_PMCS;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 32;

	pcd->pcd_allocate_pmc   = powerpc_allocate_pmc;
	pcd->pcd_config_pmc     = powerpc_config_pmc;
	pcd->pcd_pcpu_fini      = ppc970_pcpu_fini;
	pcd->pcd_pcpu_init      = ppc970_pcpu_init;
	pcd->pcd_describe       = powerpc_describe;
	pcd->pcd_get_config     = powerpc_get_config;
	pcd->pcd_read_pmc       = powerpc_read_pmc;
	pcd->pcd_release_pmc    = powerpc_release_pmc;
	pcd->pcd_start_pmc      = powerpc_start_pmc;
	pcd->pcd_stop_pmc       = powerpc_stop_pmc;
	pcd->pcd_write_pmc      = powerpc_write_pmc;

	pmc_mdep->pmd_npmc     += PPC970_MAX_PMCS;
	pmc_mdep->pmd_intr      = powerpc_pmc_intr;

	ppc_event_codes = ppc970_event_codes;
	ppc_event_codes_size = ppc970_event_codes_size;
	ppc_event_first = PMC_EV_PPC970_FIRST;
	ppc_event_last = PMC_EV_PPC970_LAST;
	ppc_max_pmcs = PPC970_MAX_PMCS;
	ppc_class = pcd->pcd_class;

	powerpc_set_pmc = ppc970_set_pmc;
	powerpc_pmcn_read = powerpc_pmcn_read_default;
	powerpc_pmcn_write = powerpc_pmcn_write_default;
	powerpc_resume_pmc = ppc970_resume_pmc;

	return (0);
}
