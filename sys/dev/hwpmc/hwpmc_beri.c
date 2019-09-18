/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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

#include "opt_hwpmc_hooks.h"

#include <sys/param.h>
#include <sys/pmckern.h>

#include <dev/hwpmc/hwpmc_beri.h>

#define	BERI_NCOUNTERS	56
#define	BERI_PMC_CAPS	(PMC_CAP_USER |	PMC_CAP_SYSTEM | \
			 PMC_CAP_READ | PMC_CAP_WRITE )

struct beri_event_code_map {
	uint32_t	pe_ev;       /* enum value */
	uint64_t	(*get_func)(void);
};

const struct beri_event_code_map beri_event_codes[BERI_NCOUNTERS] = {
	{ PMC_EV_BERI_CYCLE,
		statcounters_get_cycle_count },
	{ PMC_EV_BERI_INST,
		statcounters_get_inst_count },
	{ PMC_EV_BERI_INST_USER,
		statcounters_get_inst_user_count },
	{ PMC_EV_BERI_INST_KERNEL,
		statcounters_get_inst_kernel_count },
	{ PMC_EV_BERI_IMPRECISE_SETBOUNDS,
		statcounters_get_imprecise_setbounds_count },
	{ PMC_EV_BERI_UNREPRESENTABLE_CAPS,
		statcounters_get_unrepresentable_caps_count },
	{ PMC_EV_BERI_ITLB_MISS,
		statcounters_get_itlb_miss_count },
	{ PMC_EV_BERI_DTLB_MISS,
		statcounters_get_dtlb_miss_count },
	{ PMC_EV_BERI_ICACHE_WRITE_HIT,
		statcounters_get_icache_write_hit_count },
	{ PMC_EV_BERI_ICACHE_WRITE_MISS,
		statcounters_get_icache_write_miss_count },
	{ PMC_EV_BERI_ICACHE_READ_HIT,
		statcounters_get_icache_read_hit_count },
	{ PMC_EV_BERI_ICACHE_READ_MISS,
		statcounters_get_icache_read_miss_count },
	{ PMC_EV_BERI_ICACHE_EVICT,
		statcounters_get_icache_evict_count },
	{ PMC_EV_BERI_DCACHE_WRITE_HIT,
		statcounters_get_dcache_write_hit_count },
	{ PMC_EV_BERI_DCACHE_WRITE_MISS,
		statcounters_get_dcache_write_miss_count },
	{ PMC_EV_BERI_DCACHE_READ_HIT,
		statcounters_get_dcache_read_hit_count },
	{ PMC_EV_BERI_DCACHE_READ_MISS,
		statcounters_get_dcache_read_miss_count },
	{ PMC_EV_BERI_DCACHE_EVICT,
		statcounters_get_dcache_evict_count },
	{ PMC_EV_BERI_DCACHE_SET_TAG_WRITE,
		statcounters_get_dcache_set_tag_write_count },
	{ PMC_EV_BERI_DCACHE_SET_TAG_READ,
		statcounters_get_dcache_set_tag_read_count },
	{ PMC_EV_BERI_L2CACHE_WRITE_HIT,
		statcounters_get_l2cache_write_hit_count },
	{ PMC_EV_BERI_L2CACHE_WRITE_MISS,
		statcounters_get_l2cache_write_miss_count },
	{ PMC_EV_BERI_L2CACHE_READ_HIT,
		statcounters_get_l2cache_read_hit_count },
	{ PMC_EV_BERI_L2CACHE_READ_MISS,
		statcounters_get_l2cache_read_miss_count },
	{ PMC_EV_BERI_L2CACHE_EVICT,
		statcounters_get_l2cache_evict_count },
	{ PMC_EV_BERI_L2CACHE_SET_TAG_WRITE,
		statcounters_get_l2cache_set_tag_write_count },
	{ PMC_EV_BERI_L2CACHE_SET_TAG_READ,
		statcounters_get_l2cache_set_tag_read_count },
	{ PMC_EV_BERI_MEM_BYTE_READ,
		statcounters_get_mem_byte_read_count },
	{ PMC_EV_BERI_MEM_BYTE_WRITE,
		statcounters_get_mem_byte_write_count },
	{ PMC_EV_BERI_MEM_HWORD_READ,
		statcounters_get_mem_hword_read_count },
	{ PMC_EV_BERI_MEM_HWORD_WRITE,
		statcounters_get_mem_hword_write_count },
	{ PMC_EV_BERI_MEM_WORD_READ,
		statcounters_get_mem_word_read_count },
	{ PMC_EV_BERI_MEM_WORD_WRITE,
		statcounters_get_mem_word_write_count },
	{ PMC_EV_BERI_MEM_DWORD_READ,
		statcounters_get_mem_dword_read_count },
	{ PMC_EV_BERI_MEM_DWORD_WRITE,
		statcounters_get_mem_dword_write_count },
	{ PMC_EV_BERI_MEM_CAP_READ,
		statcounters_get_mem_cap_read_count },
	{ PMC_EV_BERI_MEM_CAP_WRITE,
		statcounters_get_mem_cap_write_count },
	{ PMC_EV_BERI_MEM_CAP_READ_TAG_SET,
		statcounters_get_mem_cap_read_tag_set_count },
	{ PMC_EV_BERI_MEM_CAP_WRITE_TAG_SET,
		statcounters_get_mem_cap_write_tag_set_count },
	{ PMC_EV_BERI_TAGCACHE_WRITE_HIT,
		statcounters_get_tagcache_write_hit_count },
	{ PMC_EV_BERI_TAGCACHE_WRITE_MISS,
		statcounters_get_tagcache_write_miss_count },
	{ PMC_EV_BERI_TAGCACHE_READ_HIT,
		statcounters_get_tagcache_read_hit_count },
	{ PMC_EV_BERI_TAGCACHE_READ_MISS,
		statcounters_get_tagcache_read_miss_count },
	{ PMC_EV_BERI_TAGCACHE_EVICT,
		statcounters_get_tagcache_evict_count },
	{ PMC_EV_BERI_L2CACHEMASTER_READ_REQ,
		statcounters_get_l2cachemaster_read_req_count },
	{ PMC_EV_BERI_L2CACHEMASTER_WRITE_REQ,
		statcounters_get_l2cachemaster_write_req_count },
	{ PMC_EV_BERI_L2CACHEMASTER_WRITE_REQ_FLIT,
		statcounters_get_l2cachemaster_write_req_flit_count },
	{ PMC_EV_BERI_L2CACHEMASTER_READ_RSP,
		statcounters_get_l2cachemaster_read_rsp_count },
	{ PMC_EV_BERI_L2CACHEMASTER_READ_RSP_FLIT,
		statcounters_get_l2cachemaster_read_rsp_flit_count },
	{ PMC_EV_BERI_L2CACHEMASTER_WRITE_RSP,
		statcounters_get_l2cachemaster_write_rsp_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_READ_REQ,
		statcounters_get_tagcachemaster_read_req_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_WRITE_REQ,
		statcounters_get_tagcachemaster_write_req_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_WRITE_REQ_FLIT,
		statcounters_get_tagcachemaster_write_req_flit_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_READ_RSP,
		statcounters_get_tagcachemaster_read_rsp_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_READ_RSP_FLIT,
		statcounters_get_tagcachemaster_read_rsp_flit_count },
	{ PMC_EV_BERI_TAGCACHEMASTER_WRITE_RSP,
		statcounters_get_tagcachemaster_write_rsp_count },
};

struct mips_pmc_spec beri_pmc_spec = {
	.ps_cpuclass = PMC_CLASS_BERI,
	.ps_cputype = PMC_CPU_MIPS_BERI,
	.ps_capabilities = BERI_PMC_CAPS,
	.ps_counter_width = 64
};

/*
 * Per-processor information.
 */
struct beri_cpu {
	struct pmc_hw	*pc_beripmcs;
	uint64_t	start_values[BERI_NCOUNTERS];
	uint64_t	stop_values[BERI_NCOUNTERS];
	uint64_t	saved_values[BERI_NCOUNTERS];
};

int beri_npmcs;
static struct beri_cpu **beri_pcpu;

static int
beri_allocate_pmc(int cpu, int ri, struct pmc *pm,
  const struct pmc_op_pmcallocate *a)
{
	uint32_t config;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != beri_pmc_spec.ps_cpuclass)
		return (EINVAL);

	for (i = 0; i < BERI_NCOUNTERS; i++) {
		if (beri_event_codes[i].pe_ev == a->pm_ev) {
			config = i;
			break;
		}
	}

	if (i == BERI_NCOUNTERS)
		return (EINVAL);

	pm->pm_md.pm_mips_evsel = config;

	PMCDBG2(MDP,ALL,2,"beri-allocate ri=%d -> config=0x%x", ri, config);

	return (0);
}

static int
beri_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	uint32_t config;
	struct pmc *pm;
	pmc_value_t new;
	pmc_value_t start_val;
	pmc_value_t stop_val;
	pmc_value_t saved_val;
	pmc_value_t result;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] illegal row index %d", __LINE__, ri));

	pm = beri_pcpu[cpu]->pc_beripmcs[ri].phw_pmc;
	config = pm->pm_md.pm_mips_evsel;

	start_val = beri_pcpu[cpu]->start_values[config];
	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm))) {
		stop_val = beri_event_codes[config].get_func();
	} else
		stop_val = beri_pcpu[cpu]->stop_values[config];

	if (start_val <= stop_val)
		result = stop_val - start_val;
	else {
		if (config == 0) /* CYCLE counter is 48 bit */
			result = 0x00ffffffffffffffUL;
		else
			result = 0xffffffffffffffffUL;
		result -= start_val;
		result += stop_val;
	}

	saved_val = beri_pcpu[cpu]->saved_values[config];

	*v = result + saved_val;

	return (0);
}

static int
beri_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc *pm;
	uint32_t config;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] illegal row-index %d", __LINE__, ri));

	pm = beri_pcpu[cpu]->pc_beripmcs[ri].phw_pmc;
	config = pm->pm_md.pm_mips_evsel;

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = (1UL << (beri_pmc_spec.ps_counter_width - 1)) - v;
	
	PMCDBG3(MDP,WRI,1,"beri-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	if (PMC_IS_SYSTEM_MODE(PMC_TO_MODE(pm)))
		beri_pcpu[cpu]->saved_values[config] = 0;
	else
		beri_pcpu[cpu]->saved_values[config] = v;

	return (0);
}

static int
beri_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] illegal row-index %d", __LINE__, ri));

	phw = &beri_pcpu[cpu]->pc_beripmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[beri,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
	    __LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return (0);
}

static int
beri_start_pmc(int cpu, int ri)
{
	uint32_t config;
        struct pmc *pm;
        struct pmc_hw *phw;
	pmc_value_t v;

	phw = &beri_pcpu[cpu]->pc_beripmcs[ri];
	pm = phw->phw_pmc;
	config = pm->pm_md.pm_mips_evsel;

	v = beri_event_codes[config].get_func();
	beri_pcpu[cpu]->start_values[config] = v;

	return (0);
}

static int
beri_stop_pmc(int cpu, int ri)
{
	uint32_t config;
        struct pmc *pm;
        struct pmc_hw *phw;
	pmc_value_t v;

	phw = &beri_pcpu[cpu]->pc_beripmcs[ri];
	pm = phw->phw_pmc;
	config = pm->pm_md.pm_mips_evsel;

	v = beri_event_codes[config].get_func();
	beri_pcpu[cpu]->stop_values[config] = v;

	return (0);
}

static int
beri_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] illegal row-index %d", __LINE__, ri));

	phw = &beri_pcpu[cpu]->pc_beripmcs[ri];
	KASSERT(phw->phw_pmc == NULL,
	    ("[beri,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return (0);
}

static int
beri_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	struct pmc_hw *phw;
	char beri_name[PMC_NAME_MAX];
	int error;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d], illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < beri_npmcs,
	    ("[beri,%d] row-index %d out of range", __LINE__, ri));

	phw = &beri_pcpu[cpu]->pc_beripmcs[ri];
	snprintf(beri_name, sizeof(beri_name), "MIPS-%d", ri);
	if ((error = copystr(beri_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return error;
	pi->pm_class = beri_pmc_spec.ps_cpuclass;
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
beri_get_config(int cpu, int ri, struct pmc **ppm)
{

	*ppm = beri_pcpu[cpu]->pc_beripmcs[ri].phw_pmc;

	return (0);
}

static int
beri_pmc_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{

	return (0);
}

static int
beri_pmc_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{

	return (0);
}

static int
beri_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int first_ri, i;
	struct pmc_cpu *pc;
	struct beri_cpu *pac;
	struct pmc_hw  *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[beri,%d] wrong cpu number %d", __LINE__, cpu));
	PMCDBG1(MDP,INI,1,"beri-init cpu=%d", cpu);

	beri_pcpu[cpu] = pac = malloc(sizeof(struct beri_cpu), M_PMC,
	    M_WAITOK|M_ZERO);
	pac->pc_beripmcs = malloc(sizeof(struct pmc_hw) * beri_npmcs,
	    M_PMC, M_WAITOK|M_ZERO);
	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_MIPS].pcd_ri;
	KASSERT(pc != NULL, ("[beri,%d] NULL per-cpu pointer", __LINE__));

	for (i = 0, phw = pac->pc_beripmcs; i < beri_npmcs; i++, phw++) {
		phw->phw_state = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i);
		phw->phw_pmc = NULL;
		pc->pc_hwpmcs[i + first_ri] = phw;
	}

	return (0);
}

static int
beri_pcpu_fini(struct pmc_mdep *md, int cpu)
{

	return (0);
}

struct pmc_mdep *
pmc_beri_initialize()
{
	struct pmc_mdep *pmc_mdep;
	struct pmc_classdep *pcd;
	
	snprintf(pmc_cpuid, sizeof(pmc_cpuid), "beri");

	beri_npmcs = 2;
	
	PMCDBG1(MDP,INI,1,"beri-init npmcs=%d", beri_npmcs);

	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	beri_pcpu = malloc(sizeof(struct beri_cpu *) * pmc_cpu_max(), M_PMC,
			   M_WAITOK|M_ZERO);

	/* Just one class */
	pmc_mdep = pmc_mdep_alloc(1);

	pmc_mdep->pmd_cputype = beri_pmc_spec.ps_cputype;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_MIPS];
	pcd->pcd_caps = beri_pmc_spec.ps_capabilities;
	pcd->pcd_class = beri_pmc_spec.ps_cpuclass;
	pcd->pcd_num = beri_npmcs;
	pcd->pcd_ri = pmc_mdep->pmd_npmc;
	pcd->pcd_width = beri_pmc_spec.ps_counter_width;

	pcd->pcd_allocate_pmc   = beri_allocate_pmc;
	pcd->pcd_config_pmc     = beri_config_pmc;
	pcd->pcd_pcpu_fini      = beri_pcpu_fini;
	pcd->pcd_pcpu_init      = beri_pcpu_init;
	pcd->pcd_describe       = beri_describe;
	pcd->pcd_get_config	= beri_get_config;
	pcd->pcd_read_pmc       = beri_read_pmc;
	pcd->pcd_release_pmc    = beri_release_pmc;
	pcd->pcd_start_pmc      = beri_start_pmc;
	pcd->pcd_stop_pmc       = beri_stop_pmc;
 	pcd->pcd_write_pmc      = beri_write_pmc;

	pmc_mdep->pmd_intr       = NULL;
	pmc_mdep->pmd_switch_in  = beri_pmc_switch_in;
	pmc_mdep->pmd_switch_out = beri_pmc_switch_out;
	
	pmc_mdep->pmd_npmc += beri_npmcs;

	return (pmc_mdep);
}

void
pmc_beri_finalize(struct pmc_mdep *md)
{

}

struct pmc_mdep *
pmc_md_initialize()
{

	return (pmc_beri_initialize());
}

void
pmc_md_finalize(struct pmc_mdep *md)
{

	return (pmc_beri_finalize(md));
}

int
pmc_save_kernel_callchain(uintptr_t *cc, int nframes,
    struct trapframe *tf)
{

	return (0);
}

int
pmc_save_user_callchain(uintptr_t *cc, int nframes,
    struct trapframe *tf)
{

	return (0);
}
