/*-
 * Copyright (c) 2009 Rui Paulo <rpaulo@FreeBSD.org>
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

#include <machine/pmc_mdep.h>
/*
 * Support for the Intel XScale network processors
 *
 * XScale processors have up to now three generations.
 *
 * The first generation has two PMC; the event selection, interrupt config
 * and overflow flag setup are done by writing to the PMNC register.
 * It also has less monitoring events than the latter generations.
 *
 * The second and third generatiosn have four PMCs, one register for the event
 * selection, one register for the interrupt config and one register for
 * the overflow flags.
 */
static int xscale_npmcs;
static int xscale_gen;	/* XScale Core generation */

struct xscale_event_code_map {
	enum pmc_event	pe_ev;
	uint8_t		pe_code;
};

const struct xscale_event_code_map xscale_event_codes[] = {
	/* 1st and 2nd Generation XScale cores */
	{ PMC_EV_XSCALE_IC_FETCH,		0x00 },
	{ PMC_EV_XSCALE_IC_MISS,		0x01 },
	{ PMC_EV_XSCALE_DATA_DEPENDENCY_STALLED,0x02 },
	{ PMC_EV_XSCALE_ITLB_MISS,		0x03 },
	{ PMC_EV_XSCALE_DTLB_MISS,		0x04 },
	{ PMC_EV_XSCALE_BRANCH_RETIRED,		0x05 },
	{ PMC_EV_XSCALE_BRANCH_MISPRED,		0x06 },
	{ PMC_EV_XSCALE_INSTR_RETIRED,		0x07 },
	{ PMC_EV_XSCALE_DC_FULL_CYCLE,		0x08 },
	{ PMC_EV_XSCALE_DC_FULL_CONTIG, 	0x09 },
	{ PMC_EV_XSCALE_DC_ACCESS,		0x0a },
	{ PMC_EV_XSCALE_DC_MISS,		0x0b },
	{ PMC_EV_XSCALE_DC_WRITEBACK,		0x0c },
	{ PMC_EV_XSCALE_PC_CHANGE,		0x0d },
	/* 3rd Generation XScale cores */
	{ PMC_EV_XSCALE_BRANCH_RETIRED_ALL,	0x0e },
	{ PMC_EV_XSCALE_INSTR_CYCLE,		0x0f },
	{ PMC_EV_XSCALE_CP_STALL,		0x17 },
	{ PMC_EV_XSCALE_PC_CHANGE_ALL,		0x18 },
	{ PMC_EV_XSCALE_PIPELINE_FLUSH,		0x19 },
	{ PMC_EV_XSCALE_BACKEND_STALL,		0x1a },
	{ PMC_EV_XSCALE_MULTIPLIER_USE,		0x1b },
	{ PMC_EV_XSCALE_MULTIPLIER_STALLED,	0x1c },
	{ PMC_EV_XSCALE_DATA_CACHE_STALLED,	0x1e },
	{ PMC_EV_XSCALE_L2_CACHE_REQ,		0x20 },
	{ PMC_EV_XSCALE_L2_CACHE_MISS,		0x23 },
	{ PMC_EV_XSCALE_ADDRESS_BUS_TRANS,	0x40 },
	{ PMC_EV_XSCALE_SELF_ADDRESS_BUS_TRANS,	0x41 },
	{ PMC_EV_XSCALE_DATA_BUS_TRANS,		0x48 },
};

const int xscale_event_codes_size =
	sizeof(xscale_event_codes) / sizeof(xscale_event_codes[0]);

/*
 * Per-processor information.
 */
struct xscale_cpu {
	struct pmc_hw   *pc_xscalepmcs;
};

static struct xscale_cpu **xscale_pcpu;

/*
 * Performance Monitor Control Register
 */
static __inline uint32_t
xscale_pmnc_read(void)
{
	uint32_t reg;

	__asm __volatile("mrc p14, 0, %0, c0, c1, 0" : "=r" (reg));

	return (reg);
}

static __inline void
xscale_pmnc_write(uint32_t reg)
{
	__asm __volatile("mcr p14, 0, %0, c0, c1, 0" : : "r" (reg));
}

/*
 * Clock Counter Register
 */
static __inline uint32_t
xscale_ccnt_read(void)
{
	uint32_t reg;

	__asm __volatile("mrc p14, 0, %0, c1, c1, 0" : "=r" (reg));
	
	return (reg);
}

static __inline void
xscale_ccnt_write(uint32_t reg)
{
	__asm __volatile("mcr p14, 0, %0, c1, c1, 0" : : "r" (reg));
}

/*
 * Interrupt Enable Register
 */
static __inline uint32_t
xscale_inten_read(void)
{
	uint32_t reg;

	__asm __volatile("mrc p14, 0, %0, c4, c1, 0" : "=r" (reg));

	return (reg);
}

static __inline void
xscale_inten_write(uint32_t reg)
{
	__asm __volatile("mcr p14, 0, %0, c4, c1, 0" : : "r" (reg));
}

/*
 * Overflow Flag Register
 */
static __inline uint32_t
xscale_flag_read(void)
{
	uint32_t reg;

	__asm __volatile("mrc p14, 0, %0, c5, c1, 0" : "=r" (reg));

	return (reg);
}

static __inline void
xscale_flag_write(uint32_t reg)
{
	__asm __volatile("mcr p14, 0, %0, c5, c1, 0" : : "r" (reg));
}

/*
 * Event Selection Register
 */
static __inline uint32_t
xscale_evtsel_read(void)
{
	uint32_t reg;

	__asm __volatile("mrc p14, 0, %0, c8, c1, 0" : "=r" (reg));

	return (reg);
}

static __inline void
xscale_evtsel_write(uint32_t reg)
{
	__asm __volatile("mcr p14, 0, %0, c8, c1, 0" : : "r" (reg));
}

/*
 * Performance Count Register N
 */
static uint32_t
xscale_pmcn_read(unsigned int pmc)
{
	uint32_t reg = 0;

	KASSERT(pmc < 4, ("[xscale,%d] illegal PMC number %d", __LINE__, pmc));

	switch (pmc) {
	case 0:
		__asm __volatile("mrc p14, 0, %0, c0, c2, 0" : "=r" (reg));
		break;
	case 1:
		__asm __volatile("mrc p14, 0, %0, c1, c2, 0" : "=r" (reg));
		break;
	case 2:
		__asm __volatile("mrc p14, 0, %0, c2, c2, 0" : "=r" (reg));
		break;
	case 3:
		__asm __volatile("mrc p14, 0, %0, c3, c2, 0" : "=r" (reg));
		break;
	}

	return (reg);
}

static uint32_t
xscale_pmcn_write(unsigned int pmc, uint32_t reg)
{

	KASSERT(pmc < 4, ("[xscale,%d] illegal PMC number %d", __LINE__, pmc));

	switch (pmc) {
	case 0:
		__asm __volatile("mcr p14, 0, %0, c0, c2, 0" : : "r" (reg));
		break;
	case 1:
		__asm __volatile("mcr p14, 0, %0, c1, c2, 0" : : "r" (reg));
		break;
	case 2:
		__asm __volatile("mcr p14, 0, %0, c2, c2, 0" : : "r" (reg));
		break;
	case 3:
		__asm __volatile("mcr p14, 0, %0, c3, c2, 0" : : "r" (reg));
		break;
	}

	return (reg);
}

static int
xscale_allocate_pmc(int cpu, int ri, struct pmc *pm,
  const struct pmc_op_pmcallocate *a)
{
	enum pmc_event pe;
	uint32_t caps, config;
	int i;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] illegal row index %d", __LINE__, ri));

	caps = a->pm_caps;
	if (a->pm_class != PMC_CLASS_XSCALE)
		return (EINVAL);
	pe = a->pm_ev;
	for (i = 0; i < xscale_event_codes_size; i++) {
		if (xscale_event_codes[i].pe_ev == pe) {
			config = xscale_event_codes[i].pe_code;
			break;
		}
	}
	if (i == xscale_event_codes_size)
		return EINVAL;
	/* Generation 1 has fewer events */
	if (xscale_gen == 1 && i > PMC_EV_XSCALE_PC_CHANGE)
		return EINVAL;
	pm->pm_md.pm_xscale.pm_xscale_evsel = config;

	PMCDBG(MDP,ALL,2,"xscale-allocate ri=%d -> config=0x%x", ri, config);

	return 0;
}


static int
xscale_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] illegal row index %d", __LINE__, ri));

	pm  = xscale_pcpu[cpu]->pc_xscalepmcs[ri].phw_pmc;
	tmp = xscale_pmcn_read(ri);
	PMCDBG(MDP,REA,2,"xscale-read id=%d -> %jd", ri, tmp);
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = XSCALE_PERFCTR_VALUE_TO_RELOAD_COUNT(tmp);
	else
		*v = tmp;

	return 0;
}

static int
xscale_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc *pm;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] illegal row-index %d", __LINE__, ri));

	pm  = xscale_pcpu[cpu]->pc_xscalepmcs[ri].phw_pmc;

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = XSCALE_RELOAD_COUNT_TO_PERFCTR_VALUE(v);
	
	PMCDBG(MDP,WRI,1,"xscale-write cpu=%d ri=%d v=%jx", cpu, ri, v);

	xscale_pmcn_write(ri, v);

	return 0;
}

static int
xscale_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] illegal row-index %d", __LINE__, ri));

	phw = &xscale_pcpu[cpu]->pc_xscalepmcs[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[xscale,%d] pm=%p phw->pm=%p hwpmc not unconfigured",
	    __LINE__, pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return 0;
}

static int
xscale_start_pmc(int cpu, int ri)
{
	uint32_t pmnc, config, evtsel;
        struct pmc *pm;
        struct pmc_hw *phw;

	phw    = &xscale_pcpu[cpu]->pc_xscalepmcs[ri];
	pm     = phw->phw_pmc;
	config = pm->pm_md.pm_xscale.pm_xscale_evsel;

	/*
	 * Configure the event selection.
	 *
	 * On the XScale 2nd Generation there's no EVTSEL register.
	 */
	if (xscale_npmcs == 2) {
		pmnc = xscale_pmnc_read();
		switch (ri) {
		case 0:
			pmnc &= ~XSCALE_PMNC_EVT0_MASK;
			pmnc |= (config << 12) & XSCALE_PMNC_EVT0_MASK;
			break;
		case 1:
			pmnc &= ~XSCALE_PMNC_EVT1_MASK;
			pmnc |= (config << 20) & XSCALE_PMNC_EVT1_MASK;
			break;
		default:
			/* XXX */
			break;
		}
		xscale_pmnc_write(pmnc);
	} else {
		evtsel = xscale_evtsel_read();
		switch (ri) {
		case 0:
			evtsel &= ~XSCALE_EVTSEL_EVT0_MASK;
			evtsel |= config & XSCALE_EVTSEL_EVT0_MASK;
			break;
		case 1:
			evtsel &= ~XSCALE_EVTSEL_EVT1_MASK;
			evtsel |= (config << 8) & XSCALE_EVTSEL_EVT1_MASK;
			break;
		case 2:
			evtsel &= ~XSCALE_EVTSEL_EVT2_MASK;
			evtsel |= (config << 16) & XSCALE_EVTSEL_EVT2_MASK;
			break;
		case 3:
			evtsel &= ~XSCALE_EVTSEL_EVT3_MASK;
			evtsel |= (config << 24) & XSCALE_EVTSEL_EVT3_MASK;
			break;
		default:
			/* XXX */
			break;
		}
		xscale_evtsel_write(evtsel);
	}
	/*
	 * Enable the PMC.
	 *
	 * Note that XScale provides only one bit to enable/disable _all_
	 * performance monitoring units.
	 */
	pmnc = xscale_pmnc_read();
	pmnc |= XSCALE_PMNC_ENABLE;
	xscale_pmnc_write(pmnc);

	return 0;
}

static int
xscale_stop_pmc(int cpu, int ri)
{
	uint32_t pmnc, evtsel;
        struct pmc *pm;
        struct pmc_hw *phw;

	phw    = &xscale_pcpu[cpu]->pc_xscalepmcs[ri];
	pm     = phw->phw_pmc;

	/*
	 * Disable the PMCs.
	 *
	 * Note that XScale provides only one bit to enable/disable _all_
	 * performance monitoring units.
	 */
	pmnc = xscale_pmnc_read();
	pmnc &= ~XSCALE_PMNC_ENABLE;
	xscale_pmnc_write(pmnc);
	/*
	 * A value of 0xff makes the corresponding PMU go into
	 * power saving mode.
	 */
	if (xscale_npmcs == 2) {
		pmnc = xscale_pmnc_read();
		switch (ri) {
		case 0:
			pmnc |= XSCALE_PMNC_EVT0_MASK;
			break;
		case 1:
			pmnc |= XSCALE_PMNC_EVT1_MASK;
			break;
		default:
			/* XXX */
			break;
		}
		xscale_pmnc_write(pmnc);
	} else {
		evtsel = xscale_evtsel_read();
		switch (ri) {
		case 0:
			evtsel |= XSCALE_EVTSEL_EVT0_MASK;
			break;
		case 1:
			evtsel |= XSCALE_EVTSEL_EVT1_MASK;
			break;
		case 2:
			evtsel |= XSCALE_EVTSEL_EVT2_MASK;
			break;
		case 3:
			evtsel |= XSCALE_EVTSEL_EVT3_MASK;
			break;
		default:
			/* XXX */
			break;
		}
		xscale_evtsel_write(evtsel);
	}

	return 0;
}

static int
xscale_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] illegal row-index %d", __LINE__, ri));

	phw = &xscale_pcpu[cpu]->pc_xscalepmcs[ri];
	KASSERT(phw->phw_pmc == NULL,
	    ("[xscale,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	return 0;
}

static int
xscale_intr(int cpu, struct trapframe *tf)
{
	printf("intr\n");
	return 0;
}

static int
xscale_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char xscale_name[PMC_NAME_MAX];

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d], illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < xscale_npmcs,
	    ("[xscale,%d] row-index %d out of range", __LINE__, ri));

	phw = &xscale_pcpu[cpu]->pc_xscalepmcs[ri];
	snprintf(xscale_name, sizeof(xscale_name), "XSCALE-%d", ri);
	if ((error = copystr(xscale_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return error;
	pi->pm_class = PMC_CLASS_XSCALE;
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
xscale_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = xscale_pcpu[cpu]->pc_xscalepmcs[ri].phw_pmc;

	return 0;
}

/*
 * XXX don't know what we should do here.
 */
static int
xscale_switch_in(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return 0;
}

static int
xscale_switch_out(struct pmc_cpu *pc, struct pmc_process *pp)
{
	return 0;
}

static int
xscale_pcpu_init(struct pmc_mdep *md, int cpu)
{
	int first_ri, i;
	struct pmc_cpu *pc;
	struct xscale_cpu *pac;
	struct pmc_hw  *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[xscale,%d] wrong cpu number %d", __LINE__, cpu));
	PMCDBG(MDP,INI,1,"xscale-init cpu=%d", cpu);

	xscale_pcpu[cpu] = pac = malloc(sizeof(struct xscale_cpu), M_PMC,
	    M_WAITOK|M_ZERO);
	pac->pc_xscalepmcs = malloc(sizeof(struct pmc_hw) * xscale_npmcs,
	    M_PMC, M_WAITOK|M_ZERO);
	pc = pmc_pcpu[cpu];
	first_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_XSCALE].pcd_ri;
	KASSERT(pc != NULL, ("[xscale,%d] NULL per-cpu pointer", __LINE__));

	for (i = 0, phw = pac->pc_xscalepmcs; i < xscale_npmcs; i++, phw++) {
		phw->phw_state    = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(i);
		phw->phw_pmc      = NULL;
		pc->pc_hwpmcs[i + first_ri] = phw;
	}

	/*
	 * Disable and put the PMUs into power save mode.
	 */
	if (xscale_npmcs == 2) {
		xscale_pmnc_write(XSCALE_PMNC_EVT1_MASK |
		    XSCALE_PMNC_EVT0_MASK);
	} else {
		xscale_evtsel_write(XSCALE_EVTSEL_EVT3_MASK |
		    XSCALE_EVTSEL_EVT2_MASK | XSCALE_EVTSEL_EVT1_MASK |
		    XSCALE_EVTSEL_EVT0_MASK);
	}

	return 0;
}

static int
xscale_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	return 0;
}

struct pmc_mdep *
pmc_xscale_initialize()
{
	struct pmc_mdep *pmc_mdep;
	struct pmc_classdep *pcd;
	uint32_t idreg;

	/* Get the Core Generation from CP15 */
	__asm __volatile("mrc p15, 0, %0, c0, c0, 0" : "=r" (idreg));
	xscale_gen = (idreg >> 13) & 0x3;
	switch (xscale_gen) {
	case 1:
		xscale_npmcs = 2;
		break;
	case 2:
	case 3:
		xscale_npmcs = 4;
		break;
	default:
		printf("%s: unknown XScale core generation\n", __func__);
		return (NULL);
	}
	PMCDBG(MDP,INI,1,"xscale-init npmcs=%d", xscale_npmcs);
	
	/*
	 * Allocate space for pointers to PMC HW descriptors and for
	 * the MDEP structure used by MI code.
	 */
	xscale_pcpu = malloc(sizeof(struct xscale_cpu *) * pmc_cpu_max(), M_PMC,
            M_WAITOK|M_ZERO);

	/* Just one class */
	pmc_mdep = pmc_mdep_alloc(1);

	pmc_mdep->pmd_cputype = PMC_CPU_INTEL_XSCALE;

	pcd = &pmc_mdep->pmd_classdep[PMC_MDEP_CLASS_INDEX_XSCALE];
	pcd->pcd_caps  = XSCALE_PMC_CAPS;
	pcd->pcd_class = PMC_CLASS_XSCALE;
	pcd->pcd_num   = xscale_npmcs;
	pcd->pcd_ri    = pmc_mdep->pmd_npmc;
	pcd->pcd_width = 32;

	pcd->pcd_allocate_pmc   = xscale_allocate_pmc;
	pcd->pcd_config_pmc     = xscale_config_pmc;
	pcd->pcd_pcpu_fini      = xscale_pcpu_fini;
	pcd->pcd_pcpu_init      = xscale_pcpu_init;
	pcd->pcd_describe       = xscale_describe;
	pcd->pcd_get_config	= xscale_get_config;
	pcd->pcd_read_pmc       = xscale_read_pmc;
	pcd->pcd_release_pmc    = xscale_release_pmc;
	pcd->pcd_start_pmc      = xscale_start_pmc;
	pcd->pcd_stop_pmc       = xscale_stop_pmc;
	pcd->pcd_write_pmc      = xscale_write_pmc;

	pmc_mdep->pmd_intr       = xscale_intr;
	pmc_mdep->pmd_switch_in  = xscale_switch_in;
	pmc_mdep->pmd_switch_out = xscale_switch_out;
	
	pmc_mdep->pmd_npmc   += xscale_npmcs;

	return (pmc_mdep);
}

void
pmc_xscale_finalize(struct pmc_mdep *md)
{
}
