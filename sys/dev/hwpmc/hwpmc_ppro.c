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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/pmckern.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/pmc_mdep.h>
#include <machine/specialreg.h>

/*
 * PENTIUM PRO SUPPORT
 */

struct p6pmc_descr {
	struct pmc_descr pm_descr; /* common information */
	uint32_t	pm_pmc_msr;
	uint32_t	pm_evsel_msr;
};

static struct p6pmc_descr p6_pmcdesc[P6_NPMCS] = {

	/* TSC */
	{
		.pm_descr =
		{
			.pd_name  = "TSC",
			.pd_class = PMC_CLASS_TSC,
			.pd_caps  = PMC_CAP_READ,
			.pd_width = 64
		},
		.pm_pmc_msr   = 0x10,
		.pm_evsel_msr = ~0
	},

#define	P6_PMC_CAPS (PMC_CAP_INTERRUPT | PMC_CAP_USER | PMC_CAP_SYSTEM | \
    PMC_CAP_EDGE | PMC_CAP_THRESHOLD | PMC_CAP_READ | PMC_CAP_WRITE |	 \
    PMC_CAP_INVERT | PMC_CAP_QUALIFIER)

	/* PMC 0 */
	{
		.pm_descr =
		{
			.pd_name  ="P6-0",
			.pd_class = PMC_CLASS_P6,
			.pd_caps  = P6_PMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr   = P6_MSR_PERFCTR0,
		.pm_evsel_msr = P6_MSR_EVSEL0
	},

	/* PMC 1 */
	{
		.pm_descr =
		{
			.pd_name  ="P6-1",
			.pd_class = PMC_CLASS_P6,
			.pd_caps  = P6_PMC_CAPS,
			.pd_width = 40
		},
		.pm_pmc_msr   = P6_MSR_PERFCTR1,
		.pm_evsel_msr = P6_MSR_EVSEL1
	}
};

static enum pmc_cputype p6_cputype;

/*
 * P6 Event descriptor
 */

struct p6_event_descr {
	const enum pmc_event pm_event;
	uint32_t	     pm_evsel;
	uint32_t	     pm_flags;
	uint32_t	     pm_unitmask;
};

static const struct p6_event_descr p6_events[] = {

#define	P6_EVDESCR(NAME, EVSEL, FLAGS, UMASK)	\
	{					\
		.pm_event = PMC_EV_P6_##NAME,	\
		.pm_evsel = (EVSEL),		\
		.pm_flags = (FLAGS),		\
		.pm_unitmask = (UMASK)		\
	}

#define	P6F_P6		(1 << PMC_CPU_INTEL_P6)
#define	P6F_CL		(1 << PMC_CPU_INTEL_CL)
#define	P6F_PII		(1 << PMC_CPU_INTEL_PII)
#define	P6F_PIII	(1 << PMC_CPU_INTEL_PIII)
#define	P6F_PM		(1 << PMC_CPU_INTEL_PM)
#define	P6F_CTR0	0x0001
#define	P6F_CTR1	0x0002
#define	P6F_ALL_CPUS	(P6F_P6 | P6F_PII | P6F_CL | P6F_PIII | P6F_PM)
#define	P6F_ALL_CTRS	(P6F_CTR0 | P6F_CTR1)
#define	P6F_ALL		(P6F_ALL_CPUS | P6F_ALL_CTRS)

#define	P6_EVENT_VALID_FOR_CPU(P,CPU)	((P)->pm_flags & (1 << (CPU)))
#define	P6_EVENT_VALID_FOR_CTR(P,CTR)	((P)->pm_flags & (1 << (CTR)))

P6_EVDESCR(DATA_MEM_REFS,		0x43, P6F_ALL, 0x00),
P6_EVDESCR(DCU_LINES_IN,		0x45, P6F_ALL, 0x00),
P6_EVDESCR(DCU_M_LINES_IN,		0x46, P6F_ALL, 0x00),
P6_EVDESCR(DCU_M_LINES_OUT,		0x47, P6F_ALL, 0x00),
P6_EVDESCR(DCU_MISS_OUTSTANDING,	0x47, P6F_ALL, 0x00),
P6_EVDESCR(IFU_FETCH,			0x80, P6F_ALL, 0x00),
P6_EVDESCR(IFU_FETCH_MISS,		0x81, P6F_ALL, 0x00),
P6_EVDESCR(ITLB_MISS,			0x85, P6F_ALL, 0x00),
P6_EVDESCR(IFU_MEM_STALL,		0x86, P6F_ALL, 0x00),
P6_EVDESCR(ILD_STALL,			0x87, P6F_ALL, 0x00),
P6_EVDESCR(L2_IFETCH,			0x28, P6F_ALL, 0x0F),
P6_EVDESCR(L2_LD,			0x29, P6F_ALL, 0x0F),
P6_EVDESCR(L2_ST,			0x2A, P6F_ALL, 0x0F),
P6_EVDESCR(L2_LINES_IN,			0x24, P6F_ALL, 0x0F),
P6_EVDESCR(L2_LINES_OUT,		0x26, P6F_ALL, 0x0F),
P6_EVDESCR(L2_M_LINES_INM,		0x25, P6F_ALL, 0x00),
P6_EVDESCR(L2_M_LINES_OUTM,		0x27, P6F_ALL, 0x0F),
P6_EVDESCR(L2_RQSTS,			0x2E, P6F_ALL, 0x0F),
P6_EVDESCR(L2_ADS,			0x21, P6F_ALL, 0x00),
P6_EVDESCR(L2_DBUS_BUSY,		0x22, P6F_ALL, 0x00),
P6_EVDESCR(L2_DBUS_BUSY_RD,		0x23, P6F_ALL, 0x00),
P6_EVDESCR(BUS_DRDY_CLOCKS,		0x62, P6F_ALL, 0x20),
P6_EVDESCR(BUS_LOCK_CLOCKS,		0x63, P6F_ALL, 0x20),
P6_EVDESCR(BUS_REQ_OUTSTANDING,		0x60, P6F_ALL, 0x00),
P6_EVDESCR(BUS_TRAN_BRD,		0x65, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_RFO,		0x66, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRANS_WB,		0x67, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_IFETCH,		0x68, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_INVAL,		0x69, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_PWR,		0x6A, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRANS_P,			0x6B, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRANS_IO,		0x6C, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_DEF,		0x6D, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_BURST,		0x6E, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_ANY,		0x70, P6F_ALL, 0x20),
P6_EVDESCR(BUS_TRAN_MEM,		0x6F, P6F_ALL, 0x20),
P6_EVDESCR(BUS_DATA_RCV,		0x64, P6F_ALL, 0x00),
P6_EVDESCR(BUS_BNR_DRV,			0x61, P6F_ALL, 0x00),
P6_EVDESCR(BUS_HIT_DRV,			0x7A, P6F_ALL, 0x00),
P6_EVDESCR(BUS_HITM_DRV,		0x7B, P6F_ALL, 0x00),
P6_EVDESCR(BUS_SNOOP_STALL,		0x7E, P6F_ALL, 0x00),
P6_EVDESCR(FLOPS,			0xC1, P6F_ALL_CPUS | P6F_CTR0, 0x00),
P6_EVDESCR(FP_COMPS_OPS_EXE,		0x10, P6F_ALL_CPUS | P6F_CTR0, 0x00),
P6_EVDESCR(FP_ASSIST,			0x11, P6F_ALL_CPUS | P6F_CTR1, 0x00),
P6_EVDESCR(MUL,				0x12, P6F_ALL_CPUS | P6F_CTR1, 0x00),
P6_EVDESCR(DIV,				0x13, P6F_ALL_CPUS | P6F_CTR1, 0x00),
P6_EVDESCR(CYCLES_DIV_BUSY,		0x14, P6F_ALL_CPUS | P6F_CTR0, 0x00),
P6_EVDESCR(LD_BLOCKS,			0x03, P6F_ALL, 0x00),
P6_EVDESCR(SB_DRAINS,			0x04, P6F_ALL, 0x00),
P6_EVDESCR(MISALIGN_MEM_REF,		0x05, P6F_ALL, 0x00),
P6_EVDESCR(EMON_KNI_PREF_DISPATCHED,	0x07, P6F_PIII | P6F_ALL_CTRS, 0x03),
P6_EVDESCR(EMON_KNI_PREF_MISS,		0x4B, P6F_PIII | P6F_ALL_CTRS, 0x03),
P6_EVDESCR(INST_RETIRED,		0xC0, P6F_ALL, 0x00),
P6_EVDESCR(UOPS_RETIRED,		0xC2, P6F_ALL, 0x00),
P6_EVDESCR(INST_DECODED,		0xD0, P6F_ALL, 0x00),
P6_EVDESCR(EMON_KNI_INST_RETIRED,	0xD8, P6F_PIII | P6F_ALL_CTRS, 0x01),
P6_EVDESCR(EMON_KNI_COMP_INST_RET,	0xD9, P6F_PIII | P6F_ALL_CTRS, 0x01),
P6_EVDESCR(HW_INT_RX,			0xC8, P6F_ALL, 0x00),
P6_EVDESCR(CYCLES_INT_MASKED,		0xC6, P6F_ALL, 0x00),
P6_EVDESCR(CYCLES_INT_PENDING_AND_MASKED, 0xC7, P6F_ALL, 0x00),
P6_EVDESCR(BR_INST_RETIRED,		0xC4, P6F_ALL, 0x00),
P6_EVDESCR(BR_MISS_PRED_RETIRED,	0xC5, P6F_ALL, 0x00),
P6_EVDESCR(BR_TAKEN_RETIRED,		0xC9, P6F_ALL, 0x00),
P6_EVDESCR(BR_MISS_PRED_TAKEN_RET, 	0xCA, P6F_ALL, 0x00),
P6_EVDESCR(BR_INST_DECODED,		0xE0, P6F_ALL, 0x00),
P6_EVDESCR(BTB_MISSES,			0xE2, P6F_ALL, 0x00),
P6_EVDESCR(BR_BOGUS,			0xE4, P6F_ALL, 0x00),
P6_EVDESCR(BACLEARS,			0xE6, P6F_ALL, 0x00),
P6_EVDESCR(RESOURCE_STALLS,		0xA2, P6F_ALL, 0x00),
P6_EVDESCR(PARTIAL_RAT_STALLS,		0xD2, P6F_ALL, 0x00),
P6_EVDESCR(SEGMENT_REG_LOADS,		0x06, P6F_ALL, 0x00),
P6_EVDESCR(CPU_CLK_UNHALTED,		0x79, P6F_ALL, 0x00),
P6_EVDESCR(MMX_INSTR_EXEC,		0xB0,
			P6F_ALL_CTRS | P6F_CL | P6F_PII, 0x00),
P6_EVDESCR(MMX_SAT_INSTR_EXEC,		0xB1,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x00),
P6_EVDESCR(MMX_UOPS_EXEC,		0xB2,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x0F),
P6_EVDESCR(MMX_INSTR_TYPE_EXEC,		0xB3,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x3F),
P6_EVDESCR(FP_MMX_TRANS,		0xCC,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x01),
P6_EVDESCR(MMX_ASSIST,			0xCD,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x00),
P6_EVDESCR(MMX_INSTR_RET,		0xCE, P6F_ALL_CTRS | P6F_PII, 0x00),
P6_EVDESCR(SEG_RENAME_STALLS,		0xD4,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x0F),
P6_EVDESCR(SEG_REG_RENAMES,		0xD5,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x0F),
P6_EVDESCR(RET_SEG_RENAMES,		0xD6,
			P6F_ALL_CTRS | P6F_PII | P6F_PIII, 0x00),
P6_EVDESCR(EMON_EST_TRANS,		0x58, P6F_ALL_CTRS | P6F_PM, 0x02),
P6_EVDESCR(EMON_THERMAL_TRIP,		0x59, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_INST_EXEC,		0x88, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_MISSP_EXEC,		0x89, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_BAC_MISSP_EXEC,		0x8A, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_CND_EXEC,			0x8B, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_CND_MISSP_EXEC,		0x8C, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_IND_EXEC,			0x8D, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_IND_MISSP_EXEC,		0x8E, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_RET_EXEC,			0x8F, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_RET_MISSP_EXEC,		0x90, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_RET_BAC_MISSP_EXEC,	0x91, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_CALL_EXEC,		0x92, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_CALL_MISSP_EXEC,		0x93, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(BR_IND_CALL_EXEC,		0x94, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_SIMD_INSTR_RETIRED,	0xCE, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_SYNCH_UOPS,		0xD3, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_ESP_UOPS,		0xD7, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_FUSED_UOPS_RET,		0xDA, P6F_ALL_CTRS | P6F_PM, 0x03),
P6_EVDESCR(EMON_UNFUSION,		0xDB, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_PREF_RQSTS_UP,		0xF0, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_PREF_RQSTS_DN,		0xD8, P6F_ALL_CTRS | P6F_PM, 0x00),
P6_EVDESCR(EMON_SSE_SSE2_INST_RETIRED,	0xD8, P6F_ALL_CTRS | P6F_PM, 0x03),
P6_EVDESCR(EMON_SSE_SSE2_COMP_INST_RETIRED, 0xD9, P6F_ALL_CTRS | P6F_PM, 0x03)

#undef	P6_EVDESCR
};

#define	P6_NEVENTS	(PMC_EV_P6_LAST - PMC_EV_P6_FIRST + 1)

static const struct p6_event_descr *
p6_find_event(enum pmc_event ev)
{
	int n;

	for (n = 0; n < P6_NEVENTS; n++)
		if (p6_events[n].pm_event == ev)
			break;
	if (n == P6_NEVENTS)
		return NULL;
	return &p6_events[n];
}

/*
 * Per-CPU data structure for P6 class CPUs
 *
 * [common stuff]
 * [3 struct pmc_hw pointers]
 * [3 struct pmc_hw structures]
 */

struct p6_cpu {
	struct pmc_cpu	pc_common;
	struct pmc_hw	*pc_hwpmcs[P6_NPMCS];
	struct pmc_hw	pc_p6pmcs[P6_NPMCS];
};

static int
p6_init(int cpu)
{
	int n;
	struct p6_cpu *pcs;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p6,%d] bad cpu %d", __LINE__, cpu));

	PMCDBG(MDP,INI,0,"p6-init cpu=%d", cpu);

	MALLOC(pcs, struct p6_cpu *, sizeof(struct p6_cpu), M_PMC,
	    M_WAITOK|M_ZERO);

	if (pcs == NULL)
		return ENOMEM;

	phw = pcs->pc_p6pmcs;

	for (n = 0; n < P6_NPMCS; n++, phw++) {
		phw->phw_state   = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n);
		phw->phw_pmc     = NULL;
		pcs->pc_hwpmcs[n] = phw;
	}

	/* Mark the TSC as shareable */
	pcs->pc_hwpmcs[0]->phw_state |= PMC_PHW_FLAG_IS_SHAREABLE;

	pmc_pcpu[cpu] = (struct pmc_cpu *) pcs;

	return 0;
}

static int
p6_cleanup(int cpu)
{
	struct pmc_cpu *pcs;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p6,%d] bad cpu %d", __LINE__, cpu));

	PMCDBG(MDP,INI,0,"p6-cleanup cpu=%d", cpu);

	if ((pcs = pmc_pcpu[cpu]) != NULL)
		FREE(pcs, M_PMC);
	pmc_pcpu[cpu] = NULL;

	return 0;
}

static int
p6_switch_in(struct pmc_cpu *pc)
{
	(void) pc;
	return 0;
}

static int
p6_switch_out(struct pmc_cpu *pc)
{
	(void) pc;
	return 0;
}

static int
p6_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc_hw *phw;
	struct pmc *pm;
	struct p6pmc_descr *pd;
	pmc_value_t tmp;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &p6_pmcdesc[ri];

	KASSERT(pm,
	    ("[p6,%d] cpu %d ri %d pmc not configured", __LINE__, cpu, ri));

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;

	tmp = rdmsr(pd->pm_pmc_msr) & P6_PERFCTR_MASK;
	if (PMC_IS_SAMPLING_MODE(pm->pm_mode))
		*v = -tmp;
	else
		*v = tmp;

	PMCDBG(MDP,REA,1, "p6-read cpu=%d ri=%d msr=0x%x -> v=%jx", cpu, ri,
	    pd->pm_pmc_msr, *v);

	return 0;
}

static int
p6_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc_hw *phw;
	struct pmc *pm;
	struct p6pmc_descr *pd;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &p6_pmcdesc[ri];

	KASSERT(pm,
	    ("[p6,%d] cpu %d ri %d pmc not configured", __LINE__, cpu, ri));

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;

	PMCDBG(MDP,WRI,1, "p6-write cpu=%d ri=%d msr=0x%x v=%jx", cpu, ri,
	    pd->pm_pmc_msr, v);

	if (PMC_IS_SAMPLING_MODE(pm->pm_mode))
		v = -v;

	wrmsr(pd->pm_pmc_msr, v & P6_PERFCTR_MASK);

	return 0;
}

static int
p6_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG(MDP,CFG,1, "p6-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	phw->phw_pmc = pm;

	return 0;
}

/*
 * A pmc may be allocated to a given row index if:
 * - the event is valid for this CPU
 * - the event is valid for this counter index
 */

static int
p6_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	uint32_t allowed_unitmask, caps, config, unitmask;
	const struct p6pmc_descr *pd;
	const struct p6_event_descr *pevent;
	enum pmc_event ev;

	(void) cpu;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p4,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P6_NPMCS,
	    ("[p4,%d] illegal row-index value %d", __LINE__, ri));

	pd = &p6_pmcdesc[ri];

	PMCDBG(MDP,ALL,1, "p6-allocate ri=%d class=%d pmccaps=0x%x "
	    "reqcaps=0x%x", ri, pd->pm_descr.pd_class, pd->pm_descr.pd_caps,
	    pm->pm_caps);

	/* check class */
	if (pd->pm_descr.pd_class != pm->pm_class)
		return EINVAL;

	/* check requested capabilities */
	caps = a->pm_caps;
	if ((pd->pm_descr.pd_caps & caps) != caps)
		return EPERM;

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC) {
		/* TSC's are always allocated in system-wide counting mode */
		if (a->pm_ev != PMC_EV_TSC_TSC ||
		    a->pm_mode != PMC_MODE_SC)
			return EINVAL;
		return 0;
	}

	/*
	 * P6 class events
	 */

	ev = pm->pm_event;

	if (ev < PMC_EV_P6_FIRST || ev > PMC_EV_P6_LAST)
		return EINVAL;

	if ((pevent = p6_find_event(ev)) == NULL)
		return ESRCH;

	if (!P6_EVENT_VALID_FOR_CPU(pevent, p6_cputype) ||
	    !P6_EVENT_VALID_FOR_CTR(pevent, (ri-1)))
		return EINVAL;

	/* For certain events, Pentium M differs from the stock P6 */
	allowed_unitmask = 0;
	if (p6_cputype == PMC_CPU_INTEL_PM) {
		if (ev == PMC_EV_P6_L2_LD || ev == PMC_EV_P6_L2_LINES_IN ||
		    ev == PMC_EV_P6_L2_LINES_OUT)
			allowed_unitmask = P6_EVSEL_TO_UMASK(0x3F);
		else if (ev == PMC_EV_P6_L2_M_LINES_OUTM)
			allowed_unitmask = P6_EVSEL_TO_UMASK(0x30);
	} else
		allowed_unitmask = P6_EVSEL_TO_UMASK(pevent->pm_unitmask);

	unitmask = a->pm_p6_config & P6_EVSEL_UMASK_MASK;
	if (unitmask & ~allowed_unitmask) /* disallow reserved bits */
		return EINVAL;

	if (ev == PMC_EV_P6_MMX_UOPS_EXEC) /* hardcoded mask */
		unitmask = P6_EVSEL_TO_UMASK(0x0F);

	config = 0;

	config |= P6_EVSEL_EVENT_SELECT(pevent->pm_evsel);

	if (unitmask & (caps & PMC_CAP_QUALIFIER))
		config |= unitmask;

	if (caps & PMC_CAP_THRESHOLD)
		config |= a->pm_p6_config & P6_EVSEL_CMASK_MASK;

	/* set at least one of the 'usr' or 'os' caps */
	if (caps & PMC_CAP_USER)
		config |= P6_EVSEL_USR;
	if (caps & PMC_CAP_SYSTEM)
		config |= P6_EVSEL_OS;
	if ((caps & (PMC_CAP_USER|PMC_CAP_SYSTEM)) == 0)
		config |= (P6_EVSEL_USR|P6_EVSEL_OS);

	if (caps & PMC_CAP_EDGE)
		config |= P6_EVSEL_E;
	if (caps & PMC_CAP_INVERT)
		config |= P6_EVSEL_INV;
	if (caps & PMC_CAP_INTERRUPT)
		config |= P6_EVSEL_INT;

	pm->pm_md.pm_p6.pm_p6_evsel = config;

	PMCDBG(MDP,ALL,2, "p6-allocate config=0x%x", config);

	return 0;
}

static int
p6_release_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	(void) pm;

	PMCDBG(MDP,REL,1, "p6-release cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p6,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P6_NPMCS,
	    ("[p6,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];

	KASSERT(phw->phw_pmc == NULL,
	    ("[p6,%d] PHW pmc %p != pmc %p", __LINE__, phw->phw_pmc, pm));

	return 0;
}

static int
p6_start_pmc(int cpu, int ri)
{
	uint32_t config;
	struct pmc *pm;
	struct pmc_hw *phw;
	const struct p6pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p6,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P6_NPMCS,
	    ("[p6,%d] illegal row-index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &p6_pmcdesc[ri];

	KASSERT(pm,
	    ("[p6,%d] starting cpu%d,ri%d with no pmc configured",
		__LINE__, cpu, ri));

	PMCDBG(MDP,STA,1, "p6-start cpu=%d ri=%d", cpu, ri);

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;	/* TSC are always running */

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_P6,
	    ("[p6,%d] unknown PMC class %d", __LINE__,
		pd->pm_descr.pd_class));

	config = pm->pm_md.pm_p6.pm_p6_evsel;

	PMCDBG(MDP,STA,2, "p6-start/2 cpu=%d ri=%d evselmsr=0x%x config=0x%x",
	    cpu, ri, pd->pm_evsel_msr, config);

	if (pd->pm_evsel_msr == P6_MSR_EVSEL0) /* CTR 0 */
		wrmsr(pd->pm_evsel_msr, config | P6_EVSEL_EN);
	else {			/* CTR1 shares the enable bit CTR 0 */
		wrmsr(pd->pm_evsel_msr, config);
		wrmsr(P6_MSR_EVSEL0, rdmsr(P6_MSR_EVSEL0) | P6_EVSEL_EN);
	}
	return 0;
}

static int
p6_stop_pmc(int cpu, int ri)
{
	uint32_t config;
	struct pmc *pm;
	struct pmc_hw *phw;
	struct p6pmc_descr *pd;

	KASSERT(cpu >= 0 && cpu < mp_ncpus,
	    ("[p6,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < P6_NPMCS,
	    ("[p6,%d] illegal row index %d", __LINE__, ri));

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pm  = phw->phw_pmc;
	pd  = &p6_pmcdesc[ri];

	KASSERT(pm,
	    ("[p6,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	if (pd->pm_descr.pd_class == PMC_CLASS_TSC)
		return 0;

	KASSERT(pd->pm_descr.pd_class == PMC_CLASS_P6,
	    ("[p6,%d] unknown PMC class %d", __LINE__,
		pd->pm_descr.pd_class));

	PMCDBG(MDP,STO,1, "p6-stop cpu=%d ri=%d", cpu, ri);

	/*
	 * If CTR0 is being turned off but CTR1 is active, we need
	 * leave CTR0's EN field set.  If CTR1 is being stopped, it
	 * suffices to zero its EVSEL register.
	 */

	if (ri == 1 &&
	    pmc_pcpu[cpu]->pc_hwpmcs[2]->phw_pmc != NULL)
		config = P6_EVSEL_EN;
	else
		config = 0;
	wrmsr(pd->pm_evsel_msr, config);

	PMCDBG(MDP,STO,2, "p6-stop/2 cpu=%d ri=%d config=0x%x", cpu, ri,
	    config);
	return 0;
}

static int
p6_intr(int cpu, uintptr_t eip)
{
	(void) cpu;
	(void) eip;
	return 0;
}

static int
p6_describe(int cpu, int ri, struct pmc_info *pi,
    struct pmc **ppmc)
{
	int error;
	size_t copied;
	struct pmc_hw *phw;
	struct p6pmc_descr *pd;

	phw = pmc_pcpu[cpu]->pc_hwpmcs[ri];
	pd  = &p6_pmcdesc[ri];

	if ((error = copystr(pd->pm_descr.pd_name, pi->pm_name,
		 PMC_NAME_MAX, &copied)) != 0)
		return error;

	pi->pm_class = pd->pm_descr.pd_class;
	pi->pm_caps  = pd->pm_descr.pd_caps;
	pi->pm_width = pd->pm_descr.pd_width;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return 0;
}

static int
p6_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < P6_NPMCS,
	    ("[p6,%d ri %d out of range", __LINE__, ri));

	*msr = p6_pmcdesc[ri].pm_pmc_msr;
	return 0;
}

int
pmc_initialize_p6(struct pmc_mdep *pmc_mdep)
{
	KASSERT(strcmp(cpu_vendor, "GenuineIntel") == 0,
	    ("[p6,%d] Initializing non-intel processor", __LINE__));

	PMCDBG(MDP,INI,1, "%s", "p6-initialize");

	switch (pmc_mdep->pmd_cputype) {

		/*
		 * P6 Family Processors
		 */

	case PMC_CPU_INTEL_P6:
	case PMC_CPU_INTEL_CL:
	case PMC_CPU_INTEL_PII:
	case PMC_CPU_INTEL_PIII:
	case PMC_CPU_INTEL_PM:

		p6_cputype = pmc_mdep->pmd_cputype;

		pmc_mdep->pmd_npmc          = P6_NPMCS;
		pmc_mdep->pmd_classes[1]    = PMC_CLASS_P6;
		pmc_mdep->pmd_nclasspmcs[1] = 2;

		pmc_mdep->pmd_init    	    = p6_init;
		pmc_mdep->pmd_cleanup 	    = p6_cleanup;
		pmc_mdep->pmd_switch_in     = p6_switch_in;
		pmc_mdep->pmd_switch_out    = p6_switch_out;
		pmc_mdep->pmd_read_pmc 	    = p6_read_pmc;
		pmc_mdep->pmd_write_pmc     = p6_write_pmc;
		pmc_mdep->pmd_config_pmc    = p6_config_pmc;
		pmc_mdep->pmd_allocate_pmc  = p6_allocate_pmc;
		pmc_mdep->pmd_release_pmc   = p6_release_pmc;
		pmc_mdep->pmd_start_pmc     = p6_start_pmc;
		pmc_mdep->pmd_stop_pmc      = p6_stop_pmc;
		pmc_mdep->pmd_intr	    = p6_intr;
		pmc_mdep->pmd_describe      = p6_describe;
		pmc_mdep->pmd_get_msr  	    = p6_get_msr; /* i386 */

		break;
	default:
		KASSERT(0,("[p6,%d] Unknown CPU type", __LINE__));
		return ENOSYS;
	}

	return 0;
}
