/*-
 * Copyright (c) 2008 Joseph Koshy
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

/*
 * Intel Core PMCs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/intr_machdep.h>
#include <x86/apicvar.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#define	CORE_CPUID_REQUEST		0xA
#define	CORE_CPUID_REQUEST_SIZE		0x4
#define	CORE_CPUID_EAX			0x0
#define	CORE_CPUID_EBX			0x1
#define	CORE_CPUID_ECX			0x2
#define	CORE_CPUID_EDX			0x3

#define	IAF_PMC_CAPS			\
	(PMC_CAP_READ | PMC_CAP_WRITE | PMC_CAP_INTERRUPT | \
	 PMC_CAP_USER | PMC_CAP_SYSTEM)
#define	IAF_RI_TO_MSR(RI)		((RI) + (1 << 30))

#define	IAP_PMC_CAPS (PMC_CAP_INTERRUPT | PMC_CAP_USER | PMC_CAP_SYSTEM | \
    PMC_CAP_EDGE | PMC_CAP_THRESHOLD | PMC_CAP_READ | PMC_CAP_WRITE |	 \
    PMC_CAP_INVERT | PMC_CAP_QUALIFIER | PMC_CAP_PRECISE)

#define	EV_IS_NOTARCH		0
#define	EV_IS_ARCH_SUPP		1
#define	EV_IS_ARCH_NOTSUPP	-1

/*
 * "Architectural" events defined by Intel.  The values of these
 * symbols correspond to positions in the bitmask returned by
 * the CPUID.0AH instruction.
 */
enum core_arch_events {
	CORE_AE_BRANCH_INSTRUCTION_RETIRED	= 5,
	CORE_AE_BRANCH_MISSES_RETIRED		= 6,
	CORE_AE_INSTRUCTION_RETIRED		= 1,
	CORE_AE_LLC_MISSES			= 4,
	CORE_AE_LLC_REFERENCE			= 3,
	CORE_AE_UNHALTED_REFERENCE_CYCLES	= 2,
	CORE_AE_UNHALTED_CORE_CYCLES		= 0
};

static enum pmc_cputype	core_cputype;

struct core_cpu {
	volatile uint32_t	pc_resync;
	volatile uint32_t	pc_iafctrl;	/* Fixed function control. */
	volatile uint64_t	pc_globalctrl;	/* Global control register. */
	struct pmc_hw		pc_corepmcs[];
};

static struct core_cpu **core_pcpu;

static uint32_t core_architectural_events;
static uint64_t core_pmcmask;

static int core_iaf_ri;		/* relative index of fixed counters */
static int core_iaf_width;
static int core_iaf_npmc;

static int core_iap_width;
static int core_iap_npmc;
static int core_iap_wroffset;

static int
core_pcpu_noop(struct pmc_mdep *md, int cpu)
{
	(void) md;
	(void) cpu;
	return (0);
}

static int
core_pcpu_init(struct pmc_mdep *md, int cpu)
{
	struct pmc_cpu *pc;
	struct core_cpu *cc;
	struct pmc_hw *phw;
	int core_ri, n, npmc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[iaf,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG1(MDP,INI,1,"core-init cpu=%d", cpu);

	core_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAP].pcd_ri;
	npmc = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAP].pcd_num;

	if (core_cputype != PMC_CPU_INTEL_CORE)
		npmc += md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAF].pcd_num;

	cc = malloc(sizeof(struct core_cpu) + npmc * sizeof(struct pmc_hw),
	    M_PMC, M_WAITOK | M_ZERO);

	core_pcpu[cpu] = cc;
	pc = pmc_pcpu[cpu];

	KASSERT(pc != NULL && cc != NULL,
	    ("[core,%d] NULL per-cpu structures cpu=%d", __LINE__, cpu));

	for (n = 0, phw = cc->pc_corepmcs; n < npmc; n++, phw++) {
		phw->phw_state 	  = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) |
		    PMC_PHW_INDEX_TO_STATE(n + core_ri);
		phw->phw_pmc	  = NULL;
		pc->pc_hwpmcs[n + core_ri]  = phw;
	}

	return (0);
}

static int
core_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	int core_ri, n, npmc;
	struct pmc_cpu *pc;
	struct core_cpu *cc;
	uint64_t msr = 0;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] insane cpu number (%d)", __LINE__, cpu));

	PMCDBG1(MDP,INI,1,"core-pcpu-fini cpu=%d", cpu);

	if ((cc = core_pcpu[cpu]) == NULL)
		return (0);

	core_pcpu[cpu] = NULL;

	pc = pmc_pcpu[cpu];

	KASSERT(pc != NULL, ("[core,%d] NULL per-cpu %d state", __LINE__,
		cpu));

	npmc = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAP].pcd_num;
	core_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAP].pcd_ri;

	for (n = 0; n < npmc; n++) {
		msr = rdmsr(IAP_EVSEL0 + n) & ~IAP_EVSEL_MASK;
		wrmsr(IAP_EVSEL0 + n, msr);
	}

	if (core_cputype != PMC_CPU_INTEL_CORE) {
		msr = rdmsr(IAF_CTRL) & ~IAF_CTRL_MASK;
		wrmsr(IAF_CTRL, msr);
		npmc += md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAF].pcd_num;
	}

	for (n = 0; n < npmc; n++)
		pc->pc_hwpmcs[n + core_ri] = NULL;

	free(cc, M_PMC);

	return (0);
}

/*
 * Fixed function counters.
 */

static pmc_value_t
iaf_perfctr_value_to_reload_count(pmc_value_t v)
{

	/* If the PMC has overflowed, return a reload count of zero. */
	if ((v & (1ULL << (core_iaf_width - 1))) == 0)
		return (0);
	v &= (1ULL << core_iaf_width) - 1;
	return (1ULL << core_iaf_width) - v;
}

static pmc_value_t
iaf_reload_count_to_perfctr_value(pmc_value_t rlc)
{
	return (1ULL << core_iaf_width) - rlc;
}

static int
iaf_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	enum pmc_event ev;
	uint32_t caps, flags, validflags;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU %d", __LINE__, cpu));

	PMCDBG2(MDP,ALL,1, "iaf-allocate ri=%d reqcaps=0x%x", ri, pm->pm_caps);

	if (ri < 0 || ri > core_iaf_npmc)
		return (EINVAL);

	caps = a->pm_caps;

	if (a->pm_class != PMC_CLASS_IAF ||
	    (caps & IAF_PMC_CAPS) != caps)
		return (EINVAL);

	ev = pm->pm_event;
	if (ev < PMC_EV_IAF_FIRST || ev > PMC_EV_IAF_LAST)
		return (EINVAL);

	if (ev == PMC_EV_IAF_INSTR_RETIRED_ANY && ri != 0)
		return (EINVAL);
	if (ev == PMC_EV_IAF_CPU_CLK_UNHALTED_CORE && ri != 1)
		return (EINVAL);
	if (ev == PMC_EV_IAF_CPU_CLK_UNHALTED_REF && ri != 2)
		return (EINVAL);

	flags = a->pm_md.pm_iaf.pm_iaf_flags;

	validflags = IAF_MASK;

	if (core_cputype != PMC_CPU_INTEL_ATOM &&
		core_cputype != PMC_CPU_INTEL_ATOM_SILVERMONT)
		validflags &= ~IAF_ANY;

	if ((flags & ~validflags) != 0)
		return (EINVAL);

	if (caps & PMC_CAP_INTERRUPT)
		flags |= IAF_PMI;
	if (caps & PMC_CAP_SYSTEM)
		flags |= IAF_OS;
	if (caps & PMC_CAP_USER)
		flags |= IAF_USR;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		flags |= (IAF_OS | IAF_USR);

	pm->pm_md.pm_iaf.pm_iaf_ctrl = (flags << (ri * 4));

	PMCDBG1(MDP,ALL,2, "iaf-allocate config=0x%jx",
	    (uintmax_t) pm->pm_md.pm_iaf.pm_iaf_ctrl);

	return (0);
}

static int
iaf_config_pmc(int cpu, int ri, struct pmc *pm)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU %d", __LINE__, cpu));

	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG3(MDP,CFG,1, "iaf-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(core_pcpu[cpu] != NULL, ("[core,%d] null per-cpu %d", __LINE__,
	    cpu));

	core_pcpu[cpu]->pc_corepmcs[ri + core_iaf_ri].phw_pmc = pm;

	return (0);
}

static int
iaf_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char iaf_name[PMC_NAME_MAX];

	phw = &core_pcpu[cpu]->pc_corepmcs[ri + core_iaf_ri];

	(void) snprintf(iaf_name, sizeof(iaf_name), "IAF-%d", ri);
	if ((error = copystr(iaf_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return (error);

	pi->pm_class = PMC_CLASS_IAF;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return (0);
}

static int
iaf_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = core_pcpu[cpu]->pc_corepmcs[ri + core_iaf_ri].phw_pmc;

	return (0);
}

static int
iaf_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[iaf,%d] ri %d out of range", __LINE__, ri));

	*msr = IAF_RI_TO_MSR(ri);

	return (0);
}

static int
iaf_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	pm = core_pcpu[cpu]->pc_corepmcs[ri + core_iaf_ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] cpu %d ri %d(%d) pmc not configured", __LINE__, cpu,
		ri, ri + core_iaf_ri));

	tmp = rdpmc(IAF_RI_TO_MSR(ri));

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = iaf_perfctr_value_to_reload_count(tmp);
	else
		*v = tmp & ((1ULL << core_iaf_width) - 1);

	PMCDBG4(MDP,REA,1, "iaf-read cpu=%d ri=%d msr=0x%x -> v=%jx", cpu, ri,
	    IAF_RI_TO_MSR(ri), *v);

	return (0);
}

static int
iaf_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	PMCDBG3(MDP,REL,1, "iaf-release cpu=%d ri=%d pm=%p", cpu, ri, pmc);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	KASSERT(core_pcpu[cpu]->pc_corepmcs[ri + core_iaf_ri].phw_pmc == NULL,
	    ("[core,%d] PHW pmc non-NULL", __LINE__));

	return (0);
}

static int
iaf_start_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct core_cpu *iafc;
	uint64_t msr = 0;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG2(MDP,STA,1,"iaf-start cpu=%d ri=%d", cpu, ri);

	iafc = core_pcpu[cpu];
	pm = iafc->pc_corepmcs[ri + core_iaf_ri].phw_pmc;

	iafc->pc_iafctrl |= pm->pm_md.pm_iaf.pm_iaf_ctrl;

 	msr = rdmsr(IAF_CTRL) & ~IAF_CTRL_MASK;
 	wrmsr(IAF_CTRL, msr | (iafc->pc_iafctrl & IAF_CTRL_MASK));

	do {
		iafc->pc_resync = 0;
		iafc->pc_globalctrl |= (1ULL << (ri + IAF_OFFSET));
 		msr = rdmsr(IA_GLOBAL_CTRL) & ~IAF_GLOBAL_CTRL_MASK;
 		wrmsr(IA_GLOBAL_CTRL, msr | (iafc->pc_globalctrl &
 					     IAF_GLOBAL_CTRL_MASK));
	} while (iafc->pc_resync != 0);

	PMCDBG4(MDP,STA,1,"iafctrl=%x(%x) globalctrl=%jx(%jx)",
	    iafc->pc_iafctrl, (uint32_t) rdmsr(IAF_CTRL),
	    iafc->pc_globalctrl, rdmsr(IA_GLOBAL_CTRL));

	return (0);
}

static int
iaf_stop_pmc(int cpu, int ri)
{
	uint32_t fc;
	struct core_cpu *iafc;
	uint64_t msr = 0;

	PMCDBG2(MDP,STO,1,"iaf-stop cpu=%d ri=%d", cpu, ri);

	iafc = core_pcpu[cpu];

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	fc = (IAF_MASK << (ri * 4));

	if (core_cputype != PMC_CPU_INTEL_ATOM &&
		core_cputype != PMC_CPU_INTEL_ATOM_SILVERMONT)
		fc &= ~IAF_ANY;

	iafc->pc_iafctrl &= ~fc;

	PMCDBG1(MDP,STO,1,"iaf-stop iafctrl=%x", iafc->pc_iafctrl);
 	msr = rdmsr(IAF_CTRL) & ~IAF_CTRL_MASK;
 	wrmsr(IAF_CTRL, msr | (iafc->pc_iafctrl & IAF_CTRL_MASK));

	do {
		iafc->pc_resync = 0;
		iafc->pc_globalctrl &= ~(1ULL << (ri + IAF_OFFSET));
 		msr = rdmsr(IA_GLOBAL_CTRL) & ~IAF_GLOBAL_CTRL_MASK;
 		wrmsr(IA_GLOBAL_CTRL, msr | (iafc->pc_globalctrl &
 					     IAF_GLOBAL_CTRL_MASK));
	} while (iafc->pc_resync != 0);

	PMCDBG4(MDP,STO,1,"iafctrl=%x(%x) globalctrl=%jx(%jx)",
	    iafc->pc_iafctrl, (uint32_t) rdmsr(IAF_CTRL),
	    iafc->pc_globalctrl, rdmsr(IA_GLOBAL_CTRL));

	return (0);
}

static int
iaf_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct core_cpu *cc;
	struct pmc *pm;
	uint64_t msr;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iaf_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	cc = core_pcpu[cpu];
	pm = cc->pc_corepmcs[ri + core_iaf_ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] cpu %d ri %d pmc not configured", __LINE__, cpu, ri));

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = iaf_reload_count_to_perfctr_value(v);

	/* Turn off fixed counters */
	msr = rdmsr(IAF_CTRL) & ~IAF_CTRL_MASK;
	wrmsr(IAF_CTRL, msr);

	wrmsr(IAF_CTR0 + ri, v & ((1ULL << core_iaf_width) - 1));

	/* Turn on fixed counters */
	msr = rdmsr(IAF_CTRL) & ~IAF_CTRL_MASK;
	wrmsr(IAF_CTRL, msr | (cc->pc_iafctrl & IAF_CTRL_MASK));

	PMCDBG6(MDP,WRI,1, "iaf-write cpu=%d ri=%d msr=0x%x v=%jx iafctrl=%jx "
	    "pmc=%jx", cpu, ri, IAF_RI_TO_MSR(ri), v,
	    (uintmax_t) rdmsr(IAF_CTRL),
	    (uintmax_t) rdpmc(IAF_RI_TO_MSR(ri)));

	return (0);
}


static void
iaf_initialize(struct pmc_mdep *md, int maxcpu, int npmc, int pmcwidth)
{
	struct pmc_classdep *pcd;

	KASSERT(md != NULL, ("[iaf,%d] md is NULL", __LINE__));

	PMCDBG0(MDP,INI,1, "iaf-initialize");

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAF];

	pcd->pcd_caps	= IAF_PMC_CAPS;
	pcd->pcd_class	= PMC_CLASS_IAF;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= pmcwidth;

	pcd->pcd_allocate_pmc	= iaf_allocate_pmc;
	pcd->pcd_config_pmc	= iaf_config_pmc;
	pcd->pcd_describe	= iaf_describe;
	pcd->pcd_get_config	= iaf_get_config;
	pcd->pcd_get_msr	= iaf_get_msr;
	pcd->pcd_pcpu_fini	= core_pcpu_noop;
	pcd->pcd_pcpu_init	= core_pcpu_noop;
	pcd->pcd_read_pmc	= iaf_read_pmc;
	pcd->pcd_release_pmc	= iaf_release_pmc;
	pcd->pcd_start_pmc	= iaf_start_pmc;
	pcd->pcd_stop_pmc	= iaf_stop_pmc;
	pcd->pcd_write_pmc	= iaf_write_pmc;

	md->pmd_npmc	       += npmc;
}

/*
 * Intel programmable PMCs.
 */

/*
 * Event descriptor tables.
 *
 * For each event id, we track:
 *
 * 1. The CPUs that the event is valid for.
 *
 * 2. If the event uses a fixed UMASK, the value of the umask field.
 *    If the event doesn't use a fixed UMASK, a mask of legal bits
 *    to check against.
 */

struct iap_event_descr {
	enum pmc_event	iap_ev;
	unsigned char	iap_evcode;
	unsigned char	iap_umask;
	unsigned int	iap_flags;
};

#define	IAP_F_CC	(1 << 0)	/* CPU: Core */
#define	IAP_F_CC2	(1 << 1)	/* CPU: Core2 family */
#define	IAP_F_CC2E	(1 << 2)	/* CPU: Core2 Extreme only */
#define	IAP_F_CA	(1 << 3)	/* CPU: Atom */
#define	IAP_F_I7	(1 << 4)	/* CPU: Core i7 */
#define	IAP_F_I7O	(1 << 4)	/* CPU: Core i7 (old) */
#define	IAP_F_WM	(1 << 5)	/* CPU: Westmere */
#define	IAP_F_SB	(1 << 6)	/* CPU: Sandy Bridge */
#define	IAP_F_IB	(1 << 7)	/* CPU: Ivy Bridge */
#define	IAP_F_SBX	(1 << 8)	/* CPU: Sandy Bridge Xeon */
#define	IAP_F_IBX	(1 << 9)	/* CPU: Ivy Bridge Xeon */
#define	IAP_F_HW	(1 << 10)	/* CPU: Haswell */
#define	IAP_F_CAS	(1 << 11)	/* CPU: Atom Silvermont */
#define	IAP_F_HWX	(1 << 12)	/* CPU: Haswell Xeon */
#define	IAP_F_BW	(1 << 13)	/* CPU: Broadwell */
#define	IAP_F_BWX	(1 << 14)	/* CPU: Broadwell Xeon */
#define	IAP_F_SL	(1 << 15)	/* CPU: Skylake */
#define	IAP_F_SLX	(1 << 16)	/* CPU: Skylake Xeon AKA scalable */
#define	IAP_F_FM	(1 << 18)	/* Fixed mask */

#define	IAP_F_ALLCPUSCORE2					\
    (IAP_F_CC | IAP_F_CC2 | IAP_F_CC2E | IAP_F_CA)

/* Sub fields of UMASK that this event supports. */
#define	IAP_M_CORE		(1 << 0) /* Core specificity */
#define	IAP_M_AGENT		(1 << 1) /* Agent specificity */
#define	IAP_M_PREFETCH		(1 << 2) /* Prefetch */
#define	IAP_M_MESI		(1 << 3) /* MESI */
#define	IAP_M_SNOOPRESPONSE	(1 << 4) /* Snoop response */
#define	IAP_M_SNOOPTYPE		(1 << 5) /* Snoop type */
#define	IAP_M_TRANSITION	(1 << 6) /* Transition */

#define	IAP_F_CORE		(0x3 << 14) /* Core specificity */
#define	IAP_F_AGENT		(0x1 << 13) /* Agent specificity */
#define	IAP_F_PREFETCH		(0x3 << 12) /* Prefetch */
#define	IAP_F_MESI		(0xF <<  8) /* MESI */
#define	IAP_F_SNOOPRESPONSE	(0xB <<  8) /* Snoop response */
#define	IAP_F_SNOOPTYPE		(0x3 <<  8) /* Snoop type */
#define	IAP_F_TRANSITION	(0x1 << 12) /* Transition */

#define	IAP_PREFETCH_RESERVED	(0x2 << 12)
#define	IAP_CORE_THIS		(0x1 << 14)
#define	IAP_CORE_ALL		(0x3 << 14)
#define	IAP_F_CMASK		0xFF000000

static struct iap_event_descr iap_events[] = {
#undef IAPDESCR
#define	IAPDESCR(N,EV,UM,FLAGS) {					\
	.iap_ev = PMC_EV_IAP_EVENT_##N,					\
	.iap_evcode = (EV),						\
	.iap_umask = (UM),						\
	.iap_flags = (FLAGS)						\
	}

    IAPDESCR(02H_01H, 0x02, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(02H_81H, 0x02, 0x81, IAP_F_FM | IAP_F_CA),

    IAPDESCR(03H_00H, 0x03, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(03H_01H, 0x03, 0x01, IAP_F_FM | IAP_F_I7O | IAP_F_SB |
	IAP_F_SBX | IAP_F_CAS),
    IAPDESCR(03H_02H, 0x03, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(03H_04H, 0x03, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_I7O |
	IAP_F_CAS),
    IAPDESCR(03H_08H, 0x03, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SB |
	IAP_F_SBX | IAP_F_CAS | IAP_F_IB | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(03H_10H, 0x03, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SB |
	IAP_F_SBX | IAP_F_CAS),
    IAPDESCR(03H_20H, 0x03, 0x20, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_CAS),
    IAPDESCR(03H_40H, 0x03, 0x40, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(03H_80H, 0x03, 0x80, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(04H_00H, 0x04, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CAS),
    IAPDESCR(04H_01H, 0x04, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_I7O |
	IAP_F_CAS),
    IAPDESCR(04H_02H, 0x04, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_CAS),
    IAPDESCR(04H_04H, 0x04, 0x04, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(04H_07H, 0x04, 0x07, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(04H_08H, 0x04, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_CAS),
    IAPDESCR(04H_10H, 0x04, 0x10, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(04H_20H, 0x04, 0x20, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(04H_40H, 0x04, 0x40, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(04H_80H, 0x04, 0x80, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(05H_00H, 0x05, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(05H_01H, 0x05, 0x01, IAP_F_FM | IAP_F_I7O | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX |  IAP_F_BW |
	IAP_F_BWX),
    IAPDESCR(05H_02H, 0x05, 0x02, IAP_F_FM | IAP_F_I7O | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(05H_03H, 0x05, 0x03, IAP_F_FM | IAP_F_I7O | IAP_F_CAS),

    IAPDESCR(06H_00H, 0x06, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2 |
	IAP_F_CC2E | IAP_F_CA),
    IAPDESCR(06H_01H, 0x06, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(06H_02H, 0x06, 0x02, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(06H_04H, 0x06, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(06H_08H, 0x06, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(06H_0FH, 0x06, 0x0F, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(07H_00H, 0x07, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2),
    IAPDESCR(07H_01H, 0x07, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX |
	IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(07H_02H, 0x07, 0x02, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(07H_03H, 0x07, 0x03, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(07H_06H, 0x07, 0x06, IAP_F_FM | IAP_F_CA),
    IAPDESCR(07H_08H, 0x07, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_SB |
	IAP_F_SBX),

    IAPDESCR(08H_01H, 0x08, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(08H_02H, 0x08, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_HW | IAP_F_HWX |
        IAP_F_BW | IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(08H_04H, 0x08, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_HW | IAP_F_HWX | IAP_F_SLX),
    IAPDESCR(08H_05H, 0x08, 0x05, IAP_F_FM | IAP_F_CA),
    IAPDESCR(08H_06H, 0x08, 0x06, IAP_F_FM | IAP_F_CA),
    IAPDESCR(08H_07H, 0x08, 0x07, IAP_F_FM | IAP_F_CA),
    IAPDESCR(08H_08H, 0x08, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SLX),
    IAPDESCR(08H_09H, 0x08, 0x09, IAP_F_FM | IAP_F_CA),
    IAPDESCR(08H_0EH, 0x08, 0x0E, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(08H_10H, 0x08, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(08H_20H, 0x08, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(08H_40H, 0x08, 0x40, IAP_F_FM | IAP_F_I7O | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(08H_60H, 0x08, 0x60, IAP_F_FM | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(08H_80H, 0x08, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(08H_81H, 0x08, 0x81, IAP_F_FM | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(08H_82H, 0x08, 0x82, IAP_F_FM | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(08H_84H, 0x08, 0x84, IAP_F_FM | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(08H_88H, 0x08, 0x88, IAP_F_FM | IAP_F_IB | IAP_F_IBX),

    IAPDESCR(09H_01H, 0x09, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_I7O),
    IAPDESCR(09H_02H, 0x09, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_I7O),
    IAPDESCR(09H_04H, 0x09, 0x04, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(09H_08H, 0x09, 0x08, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(0BH_01H, 0x0B, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0BH_02H, 0x0B, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0BH_10H, 0x0B, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(0CH_01H, 0x0C, 0x01, IAP_F_FM | IAP_F_CC2 | IAP_F_I7 |
	IAP_F_WM | IAP_F_SL),
    IAPDESCR(0CH_02H, 0x0C, 0x02, IAP_F_FM | IAP_F_CC2),
    IAPDESCR(0CH_03H, 0x0C, 0x03, IAP_F_FM | IAP_F_CA),

    IAPDESCR(0DH_01H, 0x0D, 0x80, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(0DH_03H, 0x0D, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_HW |
       IAP_F_IB | IAP_F_IBX | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(0DH_40H, 0x0D, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(0DH_80H, 0x0D, 0x80, IAP_F_FM | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(0EH_01H, 0x0E, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(0EH_02H, 0x0E, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(0EH_10H, 0x0E, 0x10, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(0EH_20H, 0x0E, 0x20, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(0EH_40H, 0x0E, 0x40, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(0FH_01H, 0x0F, 0x01, IAP_F_FM | IAP_F_I7),
    IAPDESCR(0FH_02H, 0x0F, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0FH_08H, 0x0F, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0FH_10H, 0x0F, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0FH_20H, 0x0F, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(0FH_80H, 0x0F, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(10H_00H, 0x10, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(10H_01H, 0x10, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB | IAP_F_IBX ),
    IAPDESCR(10H_02H, 0x10, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(10H_04H, 0x10, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(10H_08H, 0x10, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(10H_10H, 0x10, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(10H_20H, 0x10, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(10H_40H, 0x10, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(10H_80H, 0x10, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(10H_81H, 0x10, 0x81, IAP_F_FM | IAP_F_CA),

    IAPDESCR(11H_00H, 0x11, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2),
    IAPDESCR(11H_01H, 0x11, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_SB |
	IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(11H_02H, 0x11, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB | IAP_F_IBX),
    IAPDESCR(11H_81H, 0x11, 0x81, IAP_F_FM | IAP_F_CA),

    IAPDESCR(12H_00H, 0x12, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(12H_01H, 0x12, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_02H, 0x12, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_04H, 0x12, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_08H, 0x12, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_10H, 0x12, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_20H, 0x12, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_40H, 0x12, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(12H_81H, 0x12, 0x81, IAP_F_FM | IAP_F_CA),

    IAPDESCR(13H_00H, 0x13, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(13H_01H, 0x13, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(13H_02H, 0x13, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(13H_04H, 0x13, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(13H_07H, 0x13, 0x07, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(13H_81H, 0x13, 0x81, IAP_F_FM | IAP_F_CA),

    IAPDESCR(14H_00H, 0x14, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2),
    IAPDESCR(14H_01H, 0x14, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	 IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX |
	 IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(14H_02H, 0x14, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(17H_01H, 0x17, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_SBX),

    IAPDESCR(18H_00H, 0x18, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(18H_01H, 0x18, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(19H_00H, 0x19, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(19H_01H, 0x19, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(19H_02H, 0x19, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(1DH_01H, 0x1D, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(1DH_02H, 0x1D, 0x02, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(1DH_04H, 0x1D, 0x04, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(1EH_01H, 0x1E, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(20H_01H, 0x20, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(21H, 0x21, IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(22H, 0x22, IAP_M_CORE, IAP_F_CC2),
    IAPDESCR(23H, 0x23, IAP_M_CORE, IAP_F_ALLCPUSCORE2),

    IAPDESCR(24H, 0x24, IAP_M_CORE | IAP_M_PREFETCH, IAP_F_ALLCPUSCORE2),
    IAPDESCR(24H_01H, 0x24, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX ),
    IAPDESCR(24H_02H, 0x24, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(24H_03H, 0x24, 0x03, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_04H, 0x24, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_08H, 0x24, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_0CH, 0x24, 0x0C, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_10H, 0x24, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_20H, 0x24, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_21H, 0x24, 0x21, IAP_F_FM | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_22H, 0x24, 0x22, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_24H, 0x24, 0x24, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_27H, 0x24, 0x27, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_30H, 0x24, 0x30, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(24H_38H, 0x24, 0x38, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_3FH, 0x24, 0x3F, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_40H, 0x24, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_41H, 0x24, 0x41, IAP_F_FM | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_42H, 0x24, 0x42, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_44H, 0x24, 0x44, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_50H, 0x24, 0x50, IAP_F_FM | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(24H_80H, 0x24, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_AAH, 0x24, 0xAA, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(24H_C0H, 0x24, 0xC0, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(24H_D8H, 0x24, 0xD8, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_E1H, 0x24, 0xE1, IAP_F_FM | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_E2H, 0x24, 0xE2, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_E4H, 0x24, 0xE4, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_E7H, 0x24, 0xE7, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(24H_EFH, 0x24, 0xEF, IAP_F_FM | IAP_F_SL),
    IAPDESCR(24H_F8H, 0x24, 0xF8, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(24H_FFH, 0x24, 0xFF, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_HW |
        IAP_F_HWX | IAP_F_SLX),

    IAPDESCR(25H, 0x25, IAP_M_CORE, IAP_F_ALLCPUSCORE2),

    IAPDESCR(26H, 0x26, IAP_M_CORE | IAP_M_PREFETCH, IAP_F_ALLCPUSCORE2),
    IAPDESCR(26H_01H, 0x26, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_02H, 0x26, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_04H, 0x26, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_08H, 0x26, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_0FH, 0x26, 0x0F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_10H, 0x26, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_20H, 0x26, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_40H, 0x26, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_80H, 0x26, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_F0H, 0x26, 0xF0, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(26H_FFH, 0x26, 0xFF, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(27H, 0x27, IAP_M_CORE | IAP_M_PREFETCH, IAP_F_ALLCPUSCORE2),
    IAPDESCR(27H_01H, 0x27, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(27H_02H, 0x27, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_04H, 0x27, 0x04, IAP_F_FM | IAP_F_I7O | IAP_F_SB |
	IAP_F_SBX),
    IAPDESCR(27H_08H, 0x27, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(27H_0EH, 0x27, 0x0E, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_0FH, 0x27, 0x0F, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(27H_10H, 0x27, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_20H, 0x27, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_40H, 0x27, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_50H, 0x27, 0x50, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(27H_80H, 0x27, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_E0H, 0x27, 0xE0, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(27H_F0H, 0x27, 0xF0, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(28H, 0x28, IAP_M_CORE | IAP_M_MESI, IAP_F_ALLCPUSCORE2),
    IAPDESCR(28H_01H, 0x28, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(28H_02H, 0x28, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SBX),
    IAPDESCR(28H_04H, 0x28, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(28H_07H, 0x28, 0x07, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(28H_08H, 0x28, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(28H_0FH, 0x28, 0x0F, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(28H_18H, 0x28, 0x18, IAP_F_SLX),
    IAPDESCR(28H_20H, 0x28, 0x20, IAP_F_SLX),
    IAPDESCR(28H_40H, 0x28, 0x40, IAP_F_SLX),

    IAPDESCR(29H, 0x29, IAP_M_CORE | IAP_M_MESI, IAP_F_CC),
    IAPDESCR(29H, 0x29, IAP_M_CORE | IAP_M_MESI | IAP_M_PREFETCH,
	IAP_F_CA | IAP_F_CC2),
    IAPDESCR(2AH, 0x2A, IAP_M_CORE | IAP_M_MESI, IAP_F_ALLCPUSCORE2),
    IAPDESCR(2BH, 0x2B, IAP_M_CORE | IAP_M_MESI, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(2EH, 0x2E, IAP_M_CORE | IAP_M_MESI | IAP_M_PREFETCH,
	IAP_F_ALLCPUSCORE2),
    IAPDESCR(2EH_01H, 0x2E, 0x01, IAP_F_FM | IAP_F_WM),
    IAPDESCR(2EH_02H, 0x2E, 0x02, IAP_F_FM | IAP_F_WM),
    IAPDESCR(2EH_41H, 0x2E, 0x41, IAP_F_FM | IAP_F_ALLCPUSCORE2 | IAP_F_I7 |
	IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(2EH_4FH, 0x2E, 0x4F, IAP_F_FM | IAP_F_ALLCPUSCORE2 | IAP_F_I7 |
	IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(30H, 0x30, IAP_M_CORE | IAP_M_MESI | IAP_M_PREFETCH,
	IAP_F_ALLCPUSCORE2),
    IAPDESCR(30H_00H, 0x30, 0x00, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(31H_00H, 0x31, 0x00, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(32H, 0x32, IAP_M_CORE | IAP_M_MESI | IAP_M_PREFETCH, IAP_F_CC),
    IAPDESCR(32H, 0x32, IAP_M_CORE, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(3AH, 0x3A, IAP_M_TRANSITION, IAP_F_CC),
    IAPDESCR(3AH_00H, 0x3A, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(3BH_C0H, 0x3B, 0xC0, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(3CH_00H, 0x3C, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX |
	IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(3CH_01H, 0x3C, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX |
	IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(3CH_02H, 0x3C, 0x02, IAP_F_FM | IAP_F_ALLCPUSCORE2 | IAP_F_SL |
	IAP_F_SLX),

    IAPDESCR(3DH_01H, 0x3D, 0x01, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(40H, 0x40, IAP_M_MESI, IAP_F_CC | IAP_F_CC2),
    IAPDESCR(40H_01H, 0x40, 0x01, IAP_F_FM | IAP_F_I7),
    IAPDESCR(40H_02H, 0x40, 0x02, IAP_F_FM | IAP_F_I7),
    IAPDESCR(40H_04H, 0x40, 0x04, IAP_F_FM | IAP_F_I7),
    IAPDESCR(40H_08H, 0x40, 0x08, IAP_F_FM | IAP_F_I7),
    IAPDESCR(40H_0FH, 0x40, 0x0F, IAP_F_FM | IAP_F_I7),
    IAPDESCR(40H_21H, 0x40, 0x21, IAP_F_FM | IAP_F_CA),

    IAPDESCR(41H, 0x41, IAP_M_MESI, IAP_F_CC | IAP_F_CC2),
    IAPDESCR(41H_01H, 0x41, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(41H_02H, 0x41, 0x02, IAP_F_FM | IAP_F_I7),
    IAPDESCR(41H_04H, 0x41, 0x04, IAP_F_FM | IAP_F_I7),
    IAPDESCR(41H_08H, 0x41, 0x08, IAP_F_FM | IAP_F_I7),
    IAPDESCR(41H_0FH, 0x41, 0x0F, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(41H_22H, 0x41, 0x22, IAP_F_FM | IAP_F_CA),

    IAPDESCR(42H, 0x42, IAP_M_MESI, IAP_F_ALLCPUSCORE2),
    IAPDESCR(42H_01H, 0x42, 0x01, IAP_F_FM | IAP_F_I7),
    IAPDESCR(42H_02H, 0x42, 0x02, IAP_F_FM | IAP_F_I7),
    IAPDESCR(42H_04H, 0x42, 0x04, IAP_F_FM | IAP_F_I7),
    IAPDESCR(42H_08H, 0x42, 0x08, IAP_F_FM | IAP_F_I7),
    IAPDESCR(42H_10H, 0x42, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(43H_01H, 0x43, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7),
    IAPDESCR(43H_02H, 0x43, 0x02, IAP_F_FM | IAP_F_CA |
	IAP_F_CC2 | IAP_F_I7),

    IAPDESCR(44H_02H, 0x44, 0x02, IAP_F_FM | IAP_F_CC),

    IAPDESCR(45H_0FH, 0x45, 0x0F, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(46H_00H, 0x46, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(47H_00H, 0x47, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(48H_00H, 0x48, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(48H_01H, 0x48, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(48H_02H, 0x48, 0x02, IAP_F_FM | IAP_F_I7O | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(49H_00H, 0x49, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(49H_01H, 0x49, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX  | IAP_F_IBX |
	IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(49H_02H, 0x49, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX |
	IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(49H_04H, 0x49, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_SLX),
    IAPDESCR(49H_08H, 0x49, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(49H_0EH, 0x49, 0x0E, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(49H_10H, 0x49, 0x10,  IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(49H_20H, 0x49, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(49H_40H, 0x49, 0x40, IAP_F_FM | IAP_F_I7O | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(49H_60H, 0x49, 0x60, IAP_F_FM | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(49H_80H, 0x49, 0x80, IAP_F_FM | IAP_F_WM | IAP_F_I7 | IAP_F_HW |
        IAP_F_HWX),

    IAPDESCR(4BH_00H, 0x4B, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(4BH_01H, 0x4B, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 | IAP_F_I7O),
    IAPDESCR(4BH_02H, 0x4B, 0x02, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(4BH_03H, 0x4B, 0x03, IAP_F_FM | IAP_F_CC),
    IAPDESCR(4BH_08H, 0x4B, 0x08, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(4CH_00H, 0x4C, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(4CH_01H, 0x4C, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(4CH_02H, 0x4C, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(4DH_01H, 0x4D, 0x01, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(4EH_01H, 0x4E, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(4EH_02H, 0x4E, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(4EH_04H, 0x4E, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(4EH_10H, 0x4E, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(4FH_00H, 0x4F, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(4FH_02H, 0x4F, 0x02, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(4FH_04H, 0x4F, 0x04, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(4FH_08H, 0x4F, 0x08, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(4FH_10H, 0x4F, 0x10, IAP_F_FM | IAP_F_WM | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(51H_01H, 0x51, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(51H_02H, 0x51, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(51H_04H, 0x51, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(51H_08H, 0x51, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),

    IAPDESCR(52H_01H, 0x52, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(53H_01H, 0x53, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(54H_01H, 0x54, 0x01, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_02H, 0x54, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_04H, 0x54, 0x04, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_08H, 0x54, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_10H, 0x54, 0x10, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_20H, 0x54, 0x20, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(54H_40H, 0x54, 0x40, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(58H_01H, 0x58, 0x01, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(58H_02H, 0x58, 0x02, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(58H_04H, 0x58, 0x04, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(58H_08H, 0x58, 0x08, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(59H_20H, 0x59, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(59H_40H, 0x59, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(59H_80H, 0x59, 0x80, IAP_F_FM | IAP_F_SB | IAP_F_SBX),

    IAPDESCR(5BH_0CH, 0x5B, 0x0C, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(5BH_0FH, 0x5B, 0x0F, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(5BH_40H, 0x5B, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(5BH_4FH, 0x5B, 0x4F, IAP_F_FM | IAP_F_SB | IAP_F_SBX),

    IAPDESCR(5CH_01H, 0x5C, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(5CH_02H, 0x5C, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(5DH_01H, 0x5d, 0x01, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(5DH_02H, 0x5d, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(5DH_04H, 0x5d, 0x04, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(5DH_08H, 0x5d, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(5DH_10H, 0x5d, 0x10, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(5EH_01H, 0x5E, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(5FH_01H, 0x5F, 0x01, IAP_F_FM | IAP_F_IB ), 	 /* IB not in manual */
    IAPDESCR(5FH_04H, 0x5F, 0x04, IAP_F_FM | IAP_F_IBX | IAP_F_IB),

    IAPDESCR(60H, 0x60, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(60H_01H, 0x60, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(60H_02H, 0x60, 0x02, IAP_F_FM | IAP_F_WM | IAP_F_I7O | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(60H_04H, 0x60, 0x04, IAP_F_FM |IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(60H_08H, 0x60, 0x08, IAP_F_FM |IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
        IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(60H_10H, 0x60, 0x10, IAP_F_FM | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(61H, 0x61, IAP_M_AGENT, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(61H_00H, 0x61, 0x00, IAP_F_FM | IAP_F_CC),

    IAPDESCR(62H, 0x62, IAP_M_AGENT, IAP_F_ALLCPUSCORE2),
    IAPDESCR(62H_00H, 0x62, 0x00, IAP_F_FM | IAP_F_CC),

    IAPDESCR(63H, 0x63, IAP_M_AGENT | IAP_M_CORE,
	IAP_F_CA | IAP_F_CC2),
    IAPDESCR(63H, 0x63, IAP_M_CORE, IAP_F_CC),
    IAPDESCR(63H_01H, 0x63, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX ),
    IAPDESCR(63H_02H, 0x63, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),

    IAPDESCR(64H, 0x64, IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(64H_40H, 0x64, 0x40, IAP_F_FM | IAP_F_CC),

    IAPDESCR(65H, 0x65, IAP_M_AGENT | IAP_M_CORE,
	IAP_F_CA | IAP_F_CC2),
    IAPDESCR(65H, 0x65, IAP_M_CORE, IAP_F_CC),

    IAPDESCR(66H, 0x66, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),

    IAPDESCR(67H, 0x67, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(67H, 0x67, IAP_M_AGENT, IAP_F_CC),
    IAPDESCR(68H, 0x68, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(69H, 0x69, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(6AH, 0x6A, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(6BH, 0x6B, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),

    IAPDESCR(6CH, 0x6C, IAP_M_AGENT | IAP_M_CORE, IAP_F_ALLCPUSCORE2),
    IAPDESCR(6CH_01H, 0x6C, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(6DH, 0x6D, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(6DH, 0x6D, IAP_M_CORE, IAP_F_CC),

    IAPDESCR(6EH, 0x6E, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(6EH, 0x6E, IAP_M_CORE, IAP_F_CC),

    IAPDESCR(6FH, 0x6F, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(6FH, 0x6F, IAP_M_CORE, IAP_F_CC),

    IAPDESCR(70H, 0x70, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(70H, 0x70, IAP_M_CORE, IAP_F_CC),

    IAPDESCR(77H, 0x77, IAP_M_AGENT | IAP_M_SNOOPRESPONSE,
	IAP_F_CA | IAP_F_CC2),
    IAPDESCR(77H, 0x77, IAP_M_AGENT | IAP_M_MESI, IAP_F_CC),

    IAPDESCR(78H, 0x78, IAP_M_CORE, IAP_F_CC),
    IAPDESCR(78H, 0x78, IAP_M_CORE | IAP_M_SNOOPTYPE, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(79H_02H, 0x79, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(79H_04H, 0x79, 0x04, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_08H, 0x79, 0x08, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_SL | IAP_F_BW |
	IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(79H_10H, 0x79, 0x10, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_18H, 0x79, 0x18, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
	IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_20H, 0x79, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_24H, 0x79, 0x24, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
	IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_30H, 0x79, 0x30,  IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(79H_3CH, 0x79, 0x3C, IAP_F_FM | IAP_F_IB | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(7AH, 0x7A, IAP_M_AGENT, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(7BH, 0x7B, IAP_M_AGENT, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(7DH, 0x7D, IAP_M_CORE, IAP_F_ALLCPUSCORE2),

    IAPDESCR(7EH, 0x7E, IAP_M_AGENT | IAP_M_CORE, IAP_F_CA | IAP_F_CC2),
    IAPDESCR(7EH_00H, 0x7E, 0x00, IAP_F_FM | IAP_F_CC),

    IAPDESCR(7FH, 0x7F, IAP_M_CORE, IAP_F_CA | IAP_F_CC2),

    IAPDESCR(80H_00H, 0x80, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(80H_01H, 0x80, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_CAS),
    IAPDESCR(80H_02H, 0x80, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(80H_03H, 0x80, 0x03, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM | IAP_F_CAS),
    IAPDESCR(80H_04H, 0x80, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_IB |
	IAP_F_IBX | IAP_F_SL | IAP_F_SLX), /* SL may have a spec bug two with
					      same entry no cmask */

    IAPDESCR(81H_00H, 0x81, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(81H_01H, 0x81, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(81H_02H, 0x81, 0x02, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(82H_01H, 0x82, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(82H_02H, 0x82, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(82H_04H, 0x82, 0x04, IAP_F_FM | IAP_F_CA),
    IAPDESCR(82H_10H, 0x82, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(82H_12H, 0x82, 0x12, IAP_F_FM | IAP_F_CC2),
    IAPDESCR(82H_40H, 0x82, 0x40, IAP_F_FM | IAP_F_CC2),

    IAPDESCR(83H_01H, 0x83, 0x01, IAP_F_FM | IAP_F_I7O | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(83H_02H, 0x83, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(83H_04H, 0x83, 0x04, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(85H_00H, 0x85, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(85H_01H, 0x85, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(85H_02H, 0x85, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(85H_04H, 0x85, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_SLX),
    IAPDESCR(85H_08H, 0x85, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(85H_0EH, 0x85, 0x0E, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(85H_10H, 0x85, 0x10, IAP_F_FM | IAP_F_I7O | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(85H_20H, 0x85, 0x20, IAP_F_FM | IAP_F_I7O | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(85H_40H, 0x85, 0x40, IAP_F_FM | IAP_F_I7O | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(85H_60H, 0x85, 0x60, IAP_F_FM | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(85H_80H, 0x85, 0x80, IAP_F_FM | IAP_F_WM | IAP_F_I7O),

    IAPDESCR(86H_00H, 0x86, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(87H_00H, 0x87, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(87H_01H, 0x87, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(87H_02H, 0x87, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(87H_04H, 0x87, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(87H_08H, 0x87, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(87H_0FH, 0x87, 0x0F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(88H_00H, 0x88, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(88H_01H, 0x88, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_02H, 0x88, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_04H, 0x88, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_07H, 0x88, 0x07, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(88H_08H, 0x88, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_10H, 0x88, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_20H, 0x88, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_30H, 0x88, 0x30, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(88H_40H, 0x88, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_41H, 0x88, 0x41, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_7FH, 0x88, 0x7F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(88H_80H, 0x88, 0x80, IAP_F_FM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(88H_81H, 0x88, 0x81, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_82H, 0x88, 0x82, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_84H, 0x88, 0x84, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_88H, 0x88, 0x88, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_90H, 0x88, 0x90, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_A0H, 0x88, 0xA0, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(88H_FFH, 0x88, 0xFF, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(89H_00H, 0x89, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(89H_01H, 0x89, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_02H, 0x89, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(89H_04H, 0x89, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_07H, 0x89, 0x07, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(89H_08H, 0x89, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_10H, 0x89, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_20H, 0x89, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_30H, 0x89, 0x30, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(89H_40H, 0x89, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_41H, 0x89, 0x41, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_7FH, 0x89, 0x7F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(89H_80H, 0x89, 0x80, IAP_F_FM | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(89H_81H, 0x89, 0x81, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_82H, 0x89, 0x82, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_84H, 0x89, 0x84, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_88H, 0x89, 0x88, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_90H, 0x89, 0x90, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_A0H, 0x89, 0xA0, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(89H_FFH, 0x89, 0xFF, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(8AH_00H, 0x8A, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(8BH_00H, 0x8B, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(8CH_00H, 0x8C, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(8DH_00H, 0x8D, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(8EH_00H, 0x8E, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(8FH_00H, 0x8F, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(90H_00H, 0x90, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(91H_00H, 0x91, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(92H_00H, 0x92, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(93H_00H, 0x93, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(94H_00H, 0x94, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),

    IAPDESCR(97H_00H, 0x97, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(98H_00H, 0x98, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(9CH_01H, 0x9C, 0x01,  IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(A0H_00H, 0xA0, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(A1H_01H, 0xA1, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_02H, 0xA1, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_04H, 0xA1, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |   /* No desc in IB for this*/
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_08H, 0xA1, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |   /* No desc in IB for this*/
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_0CH, 0xA1, 0x0C, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(A1H_10H, 0xA1, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |   /* No desc in IB for this*/
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_20H, 0xA1, 0x20, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |   /* No desc in IB for this*/
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_40H, 0xA1, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A1H_80H, 0xA1, 0x80, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(A2H_00H, 0xA2, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(A2H_01H, 0xA2, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A2H_02H, 0xA2, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(A2H_04H, 0xA2, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(A2H_08H, 0xA2, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A2H_10H, 0xA2, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(A2H_20H, 0xA2, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(A2H_40H, 0xA2, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(A2H_80H, 0xA2, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX),

    IAPDESCR(A3H_01H, 0xA3, 0x01, IAP_F_FM | IAP_F_SBX | IAP_F_IBX | IAP_F_IB |
	IAP_F_HW | IAP_F_HWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_02H, 0xA3, 0x02, IAP_F_FM | IAP_F_SBX | IAP_F_IBX | IAP_F_IB |
	IAP_F_HW | IAP_F_HWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_04H, 0xA3, 0x04, IAP_F_FM | IAP_F_SBX | IAP_F_IBX | IAP_F_IB |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_05H, 0xA3, 0x05, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(A3H_06H, 0xA3, 0x06, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_08H, 0xA3, 0x08, IAP_F_FM | IAP_F_IBX | IAP_F_HW | IAP_F_IB |
	IAP_F_HWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_0CH, 0xA3, 0x0C, IAP_F_FM | IAP_F_HW | IAP_F_HW | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(A3H_10H, 0xA3, 0x10, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A3H_14H, 0xA3, 0x14, IAP_F_FM | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(A6H_01H, 0xA6, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(A6H_02H, 0xA3, 0x02, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A6H_04H, 0xA3, 0x04, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A6H_08H, 0xA3, 0x08, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A6H_10H, 0xA3, 0x10, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(A6H_40H, 0xA3, 0x40, IAP_F_FM | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(A7H_01H, 0xA7, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM ),

    IAPDESCR(A8H_01H, 0xA8, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_IBX |
	IAP_F_IB |IAP_F_SB |  IAP_F_SBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(AAH_01H, 0xAA, 0x01, IAP_F_FM | IAP_F_CC2),
    IAPDESCR(AAH_02H, 0xAA, 0x02, IAP_F_FM | IAP_F_CA),
    IAPDESCR(AAH_03H, 0xAA, 0x03, IAP_F_FM | IAP_F_CA),
    IAPDESCR(AAH_08H, 0xAA, 0x08, IAP_F_FM | IAP_F_CC2),

    IAPDESCR(ABH_01H, 0xAB, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(ABH_02H, 0xAB, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SLX),

    IAPDESCR(ACH_02H, 0xAC, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_SL),
    IAPDESCR(ACH_08H, 0xAC, 0x08, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(ACH_0AH, 0xAC, 0x0A, IAP_F_FM | IAP_F_SB | IAP_F_SBX),

    IAPDESCR(AEH_01H, 0xAE, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),

    IAPDESCR(B0H_00H, 0xB0, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(B0H_01H, 0xB0, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(B0H_02H, 0xB0, 0x02, IAP_F_FM | IAP_F_WM | IAP_F_I7O | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(B0H_04H, 0xB0, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(B0H_08H, 0xB0, 0x08, IAP_F_FM | IAP_F_WM | IAP_F_I7O |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(B0H_10H, 0xB0, 0x10, IAP_F_FM | IAP_F_WM | IAP_F_I7O | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(B0H_20H, 0xB0, 0x20, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(B0H_40H, 0xB0, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B0H_80H, 0xB0, 0x80, IAP_F_FM | IAP_F_CA | IAP_F_WM | IAP_F_I7O |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(B1H_00H, 0xB1, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(B1H_01H, 0xB1, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(B1H_02H, 0xB1, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(B1H_04H, 0xB1, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_08H, 0xB1, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_10H, 0xB1, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SLX),
    IAPDESCR(B1H_1FH, 0xB1, 0x1F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_20H, 0xB1, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_3FH, 0xB1, 0x3F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_40H, 0xB1, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B1H_80H, 0xB1, 0x80, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM),

    IAPDESCR(B2H_01H, 0xB2, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX | IAP_F_SLX),

    IAPDESCR(B3H_01H, 0xB3, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_WM | IAP_F_I7O),
    IAPDESCR(B3H_02H, 0xB3, 0x02, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_WM | IAP_F_I7O),
    IAPDESCR(B3H_04H, 0xB3, 0x04, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_WM | IAP_F_I7O),
    IAPDESCR(B3H_08H, 0xB3, 0x08, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(B3H_10H, 0xB3, 0x10, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(B3H_20H, 0xB3, 0x20, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(B3H_81H, 0xB3, 0x81, IAP_F_FM | IAP_F_CA),
    IAPDESCR(B3H_82H, 0xB3, 0x82, IAP_F_FM | IAP_F_CA),
    IAPDESCR(B3H_84H, 0xB3, 0x84, IAP_F_FM | IAP_F_CA),
    IAPDESCR(B3H_88H, 0xB3, 0x88, IAP_F_FM | IAP_F_CA),
    IAPDESCR(B3H_90H, 0xB3, 0x90, IAP_F_FM | IAP_F_CA),
    IAPDESCR(B3H_A0H, 0xB3, 0xA0, IAP_F_FM | IAP_F_CA),

    IAPDESCR(B4H_01H, 0xB4, 0x01, IAP_F_FM | IAP_F_WM),
    IAPDESCR(B4H_02H, 0xB4, 0x02, IAP_F_FM | IAP_F_WM),
    IAPDESCR(B4H_04H, 0xB4, 0x04, IAP_F_FM | IAP_F_WM),

    IAPDESCR(B6H_01H, 0xB6, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(B6H_04H, 0xB6, 0x04, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(B7H_01H, 0xB7, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_CAS |
	IAP_F_HWX |IAP_F_BW | IAP_F_BWX | IAP_F_SL),
    IAPDESCR(B7H_02H, 0xB7, 0x02, IAP_F_CAS),

    IAPDESCR(B8H_01H, 0xB8, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B8H_02H, 0xB8, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(B8H_04H, 0xB8, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(BAH_01H, 0xBA, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(BAH_02H, 0xBA, 0x02, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(BBH_01H, 0xBB, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL),

    IAPDESCR(BCH_11H, 0xBC, 0x11, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_12H, 0xBC, 0x12, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_14H, 0xBC, 0x14, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_18H, 0xBC, 0x18, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_21H, 0xBC, 0x21, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_22H, 0xBC, 0x22, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_24H, 0xBC, 0x24, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(BCH_28H, 0xBC, 0x28, IAP_F_FM | IAP_F_HW | IAP_F_HWX),

    IAPDESCR(BDH_01H, 0xBD, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_SL | IAP_F_SLX), /* spec bug SL? */
    IAPDESCR(BDH_20H, 0xBD, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_SLX),

    IAPDESCR(BFH_05H, 0xBF, 0x05, IAP_F_FM | IAP_F_SB | IAP_F_SBX),

    IAPDESCR(C0H_00H, 0xC0, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C0H_01H, 0xC0, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C0H_02H, 0xC0, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(C0H_04H, 0xC0, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(C0H_08H, 0xC0, 0x08, IAP_F_FM | IAP_F_CC2E),

    IAPDESCR(C1H_00H, 0xC1, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(C1H_01H, 0xC1, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(C1H_02H, 0xC1, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_SBX),
    IAPDESCR(C1H_08H, 0xC1, 0x08, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(C1H_10H, 0xC1, 0x10, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(C1H_20H, 0xC1, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(C1H_3FH, 0xC1, 0x3F, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C1H_40H, 0xC1, 0x40, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(C1H_80H, 0xC1, 0x80, IAP_F_FM |IAP_F_IB | IAP_F_IBX),
    IAPDESCR(C1H_FEH, 0xC1, 0xFE, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(C2H_00H, 0xC2, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(C2H_01H, 0xC2, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C2H_02H, 0xC2, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C2H_04H, 0xC2, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(C2H_07H, 0xC2, 0x07, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(C2H_08H, 0xC2, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(C2H_0FH, 0xC2, 0x0F, IAP_F_FM | IAP_F_CC2),
    IAPDESCR(C2H_10H, 0xC2, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CAS),

    IAPDESCR(C3H_00H, 0xC3, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(C3H_01H, 0xC3, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_CAS | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C3H_02H, 0xC3, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
	IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C3H_04H, 0xC3, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C3H_08H, 0xC3, 0x08, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C3H_10H, 0xC3, 0x10, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(C3H_20H, 0xC3, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),

    IAPDESCR(C4H_00H, 0xC4, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C4H_01H, 0xC4, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C4H_02H, 0xC4, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C4H_04H, 0xC4, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C4H_08H, 0xC4, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW |
        IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C4H_0CH, 0xC4, 0x0C, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(C4H_0FH, 0xC4, 0x0F, IAP_F_FM | IAP_F_CA),
    IAPDESCR(C4H_10H, 0xC4, 0x10, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C4H_20H, 0xC4, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C4H_40H, 0xC4, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C4H_7EH, 0xC4, 0x7E, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_BFH, 0xC4, 0xBF, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_EBH, 0xC4, 0xEB, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_F7H, 0xC4, 0xF7, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_F9H, 0xC4, 0xF9, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_FBH, 0xC4, 0xFB, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_FDH, 0xC4, 0xFD, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C4H_FEH, 0xC4, 0xFE, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(C5H_00H, 0xC5, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C5H_01H, 0xC5, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(C5H_02H, 0xC5, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C5H_04H, 0xC5, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL),
    IAPDESCR(C5H_10H, 0xC5, 0x10, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(C5H_20H, 0xC5, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C5H_7EH, 0xC5, 0x7E, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_BFH, 0xC5, 0xBF, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_EBH, 0xC5, 0xEB, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_F7H, 0xC5, 0xF7, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_F9H, 0xC5, 0xF9, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_FBH, 0xC5, 0xFB, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_FDH, 0xC5, 0xFD, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(C5H_FEH, 0xC5, 0xFE, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(C6H_00H, 0xC6, 0x00, IAP_F_FM | IAP_F_CC),
	     /* For SL C6_01 needs EV_SEL? 0x11, 0x12, 0x13, 0x14, 0x15? */
    IAPDESCR(C6H_01H, 0xC6, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(C6H_02H, 0xC6, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(C7H_00H, 0xC7, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(C7H_01H, 0xC7, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_02H, 0xC7, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_04H, 0xC7, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_08H, 0xC7, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_10H, 0xC7, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_1FH, 0xC7, 0x1F, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(C7H_20H, 0xC7, 0x20, IAP_F_FM | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(C7H_40H, 0xc7, 0x40, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C7H_80H, 0xc7, 0x80, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(C8H_00H, 0xC8, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(C8H_01H, 0xC8, 0x01, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_02H, 0xC8, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_04H, 0xC8, 0x04, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_08H, 0xC8, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_10H, 0xC8, 0x10, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_20H, 0xC8, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_SLX),
    IAPDESCR(C8H_40H, 0xC8, 0x40, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C8H_80H, 0xC8, 0x80, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(C9H_00H, 0xC9, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(C9H_01H, 0xC9, 0x01, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_02H, 0xC9, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_04H, 0xC9, 0x04, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_08H, 0xC9, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_10H, 0xC9, 0x10, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_20H, 0xC9, 0x20, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_40H, 0xC9, 0x40, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(C9H_80H, 0xC9, 0x80, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(CAH_00H, 0xCA, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(CAH_01H, 0xCA, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_CAS),
    IAPDESCR(CAH_02H, 0xCA, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(CAH_04H, 0xCA, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(CAH_08H, 0xCA, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(CAH_10H, 0xCA, 0x10, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(CAH_1EH, 0xCA, 0x1E, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),
    IAPDESCR(CAH_20H, 0xCA, 0x20, IAP_F_FM | IAP_F_CAS | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(CAH_3FH, 0xCA, 0x3F, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(CAH_50H, 0xCA, 0x50, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(CBH_01H, 0xCB, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_CAS | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(CBH_02H, 0xCB, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CBH_04H, 0xCB, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CBH_08H, 0xCB, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CBH_10H, 0xCB, 0x10, IAP_F_FM | IAP_F_CC2 | IAP_F_I7 |
	IAP_F_WM),
    IAPDESCR(CBH_1FH, 0xCB, 0x1F, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(CBH_40H, 0xCB, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CBH_80H, 0xCB, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(CCH_00H, 0xCC, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(CCH_01H, 0xCC, 0x01, IAP_F_FM | IAP_F_ALLCPUSCORE2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CCH_02H, 0xCC, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CCH_03H, 0xCC, 0x03, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(CCH_20H, 0xCC, 0x20, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SLX),

    IAPDESCR(CDH_00H, 0xCD, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(CDH_01H, 0xCD, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_CAS | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(CDH_02H, 0xCD, 0x02, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX),

    IAPDESCR(CEH_00H, 0xCE, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(CFH_00H, 0xCF, 0x00, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    /* Sandy Bridge / Sandy Bridge Xeon - 11, 12, 21, 41, 42, 81, 82 */
    IAPDESCR(D0H_00H, 0xD0, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D0H_01H, 0xD0, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(D0H_11H, 0xD0, 0x11, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D0H_12H, 0xD0, 0x12, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
        IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D0H_21H, 0xD0, 0x21, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D0H_41H, 0xD0, 0x41, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D0H_42H, 0xD0, 0x42, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D0H_81H, 0xD0, 0x81, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D0H_82H, 0xD0, 0x82, IAP_F_FM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),

    IAPDESCR(D1H_01H, 0xD1, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_SB |
	IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D1H_02H, 0xD1, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D1H_04H, 0xD1, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D1H_08H, 0xD1, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM | IAP_F_IB |
        IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D1H_10H, 0xD1, 0x10, IAP_F_HW | IAP_F_IB | IAP_F_IBX | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D1H_20H, 0xD1, 0x20, IAP_F_FM | IAP_F_SBX | IAP_F_IBX | IAP_F_IB |
        IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(D1H_40H, 0xD1, 0x40, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX |
	IAP_F_SL | IAP_F_SLX),

    IAPDESCR(D2H_01H, 0xD2, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D2H_02H, 0xD2, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D2H_04H, 0xD2, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D2H_08H, 0xD2, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SL |
	IAP_F_SLX),
    IAPDESCR(D2H_0FH, 0xD2, 0x0F, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM | IAP_F_SB | IAP_F_SBX | IAP_F_IB |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX),

    IAPDESCR(D2H_10H, 0xD2, 0x10, IAP_F_FM | IAP_F_CC2E),

    IAPDESCR(D3H_01H, 0xD3, 0x01, IAP_F_FM | IAP_F_IB | IAP_F_SBX |
	IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX | IAP_F_SLX),
    IAPDESCR(D3H_02H, 0xD3, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(D3H_03H, 0xD3, 0x03, IAP_F_FM | IAP_F_IBX),
    IAPDESCR(D3H_04H, 0xD3, 0x04, IAP_F_FM | IAP_F_SBX | IAP_F_IBX | IAP_F_SLX),	/* Not defined for IBX */
    IAPDESCR(D3H_08H, 0xD3, 0x08, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(D3H_0CH, 0xD3, 0x0C, IAP_F_FM | IAP_F_IBX),
    IAPDESCR(D3H_10H, 0xD3, 0x10, IAP_F_FM | IAP_F_IBX  ),
    IAPDESCR(D3H_20H, 0xD3, 0x20, IAP_F_FM | IAP_F_IBX  ),

    IAPDESCR(D4H_01H, 0xD4, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(D4H_02H, 0xD4, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_SB | IAP_F_SBX),
    IAPDESCR(D4H_04H, 0xD4, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2 | IAP_F_SLX),
    IAPDESCR(D4H_08H, 0xD4, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(D4H_0FH, 0xD4, 0x0F, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(D5H_01H, 0xD5, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2 |
	IAP_F_I7 | IAP_F_WM),
    IAPDESCR(D5H_02H, 0xD5, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(D5H_04H, 0xD5, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(D5H_08H, 0xD5, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(D5H_0FH, 0xD5, 0x0F, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(D7H_00H, 0xD7, 0x00, IAP_F_FM | IAP_F_CC),

    IAPDESCR(D8H_00H, 0xD8, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D8H_01H, 0xD8, 0x01, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D8H_02H, 0xD8, 0x02, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D8H_03H, 0xD8, 0x03, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D8H_04H, 0xD8, 0x04, IAP_F_FM | IAP_F_CC),

    IAPDESCR(D9H_00H, 0xD9, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D9H_01H, 0xD9, 0x01, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D9H_02H, 0xD9, 0x02, IAP_F_FM | IAP_F_CC),
    IAPDESCR(D9H_03H, 0xD9, 0x03, IAP_F_FM | IAP_F_CC),

    IAPDESCR(DAH_00H, 0xDA, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(DAH_01H, 0xDA, 0x01, IAP_F_FM | IAP_F_CC),
    IAPDESCR(DAH_02H, 0xDA, 0x02, IAP_F_FM | IAP_F_CC),

    IAPDESCR(DBH_00H, 0xDB, 0x00, IAP_F_FM | IAP_F_CC),
    IAPDESCR(DBH_01H, 0xDB, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(DCH_01H, 0xDC, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(DCH_02H, 0xDC, 0x02, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(DCH_04H, 0xDC, 0x04, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(DCH_08H, 0xDC, 0x08, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(DCH_10H, 0xDC, 0x10, IAP_F_FM | IAP_F_CA | IAP_F_CC2),
    IAPDESCR(DCH_1FH, 0xDC, 0x1F, IAP_F_FM | IAP_F_CA | IAP_F_CC2),

    IAPDESCR(E0H_00H, 0xE0, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2),
    IAPDESCR(E0H_01H, 0xE0, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM),

    IAPDESCR(E2H_00H, 0xE2, 0x00, IAP_F_FM | IAP_F_CC),

    IAPDESCR(E4H_00H, 0xE4, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(E4H_01H, 0xE4, 0x01, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(E5H_01H, 0xE5, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(E6H_00H, 0xE6, 0x00, IAP_F_FM | IAP_F_CC | IAP_F_CC2),
    IAPDESCR(E6H_01H, 0xE6, 0x01, IAP_F_FM | IAP_F_CA | IAP_F_I7 |
	IAP_F_WM | IAP_F_SBX | IAP_F_CAS | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(E6H_02H, 0xE6, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(E6H_08H, 0xE6, 0x08, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(E6H_10H, 0xE6, 0x10, IAP_F_FM | IAP_F_CAS),
    IAPDESCR(E6H_1FH, 0xE6, 0x1F, IAP_F_FM | IAP_F_IB |
        IAP_F_IBX | IAP_F_HW | IAP_F_HWX),

    IAPDESCR(E7H_01H, 0xE7, 0x01, IAP_F_FM | IAP_F_CAS),

    IAPDESCR(E8H_01H, 0xE8, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(E8H_02H, 0xE8, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM),
    IAPDESCR(E8H_03H, 0xE8, 0x03, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(ECH_01H, 0xEC, 0x01, IAP_F_FM | IAP_F_WM),

    IAPDESCR(F0H_00H, 0xF0, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(F0H_01H, 0xF0, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_02H, 0xF0, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_04H, 0xF0, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_08H, 0xF0, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_10H, 0xF0, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_20H, 0xF0, 0x20, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F0H_40H, 0xF0, 0x40, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL | IAP_F_SLX),
    IAPDESCR(F0H_80H, 0xF0, 0x80, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),

    IAPDESCR(F1H_01H, 0xF1, 0x01, IAP_F_FM | IAP_F_SB | IAP_F_IB |
	IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX | IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F1H_02H, 0xF1, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX),
    IAPDESCR(F1H_04H, 0xF1, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX ),
    IAPDESCR(F1H_07H, 0xF1, 0x07, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_HW | IAP_F_HWX |
	IAP_F_BW | IAP_F_BWX | IAP_F_SL),
    IAPDESCR(F1H_1FH, 0xF1, 0x1f, IAP_F_FM | IAP_F_SLX),

    IAPDESCR(F2H_01H, 0xF2, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_SLX),
    IAPDESCR(F2H_02H, 0xF2, 0x02, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_SLX),
    IAPDESCR(F2H_04H, 0xF2, 0x04, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX | IAP_F_SLX),
    IAPDESCR(F2H_05H, 0xF2, 0x05, IAP_F_FM | IAP_F_HW | IAP_F_HWX | IAP_F_BW |
	IAP_F_BWX),
    IAPDESCR(F2H_06H, 0xF2, 0x06, IAP_F_FM | IAP_F_HW | IAP_F_HWX),
    IAPDESCR(F2H_08H, 0xF2, 0x08, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_IB | IAP_F_SBX | IAP_F_IBX),
    IAPDESCR(F2H_0AH, 0xF2, 0x0A, IAP_F_FM | IAP_F_SB | IAP_F_SBX |
	IAP_F_IBX),
    IAPDESCR(F2H_0FH, 0xF2, 0x0F, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(F3H_01H, 0xF3, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F3H_02H, 0xF3, 0x02, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F3H_04H, 0xF3, 0x04, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F3H_08H, 0xF3, 0x08, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F3H_10H, 0xF3, 0x10, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F3H_20H, 0xF3, 0x20, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(F4H_01H, 0xF4, 0x01, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F4H_02H, 0xF4, 0x02, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F4H_04H, 0xF4, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_I7O),
    IAPDESCR(F4H_08H, 0xF4, 0x08, IAP_F_FM | IAP_F_I7O),
    IAPDESCR(F4H_10H, 0xF4, 0x10, IAP_F_FM | IAP_F_I7 | IAP_F_WM |
	IAP_F_SB | IAP_F_SBX | IAP_F_SLX),

    IAPDESCR(F6H_01H, 0xF6, 0x01, IAP_F_FM | IAP_F_I7 | IAP_F_WM),

    IAPDESCR(F7H_01H, 0xF7, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(F7H_02H, 0xF7, 0x02, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(F7H_04H, 0xF7, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_I7),

    IAPDESCR(F8H_00H, 0xF8, 0x00, IAP_F_FM | IAP_F_ALLCPUSCORE2),
    IAPDESCR(F8H_01H, 0xF8, 0x01, IAP_F_FM | IAP_F_I7O),

    IAPDESCR(FDH_01H, 0xFD, 0x01, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_02H, 0xFD, 0x02, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_04H, 0xFD, 0x04, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_08H, 0xFD, 0x08, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_10H, 0xFD, 0x10, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_20H, 0xFD, 0x20, IAP_F_FM | IAP_F_WM | IAP_F_I7),
    IAPDESCR(FDH_40H, 0xFD, 0x40, IAP_F_FM | IAP_F_WM | IAP_F_I7),

    IAPDESCR(FEH_02H, 0xfe, 0x02, IAP_F_FM | IAP_F_SLX),
    IAPDESCR(FEH_04H, 0xfe, 0x04, IAP_F_FM | IAP_F_SLX),
};

static pmc_value_t
iap_perfctr_value_to_reload_count(pmc_value_t v)
{

	/* If the PMC has overflowed, return a reload count of zero. */
	if ((v & (1ULL << (core_iap_width - 1))) == 0)
		return (0);
	v &= (1ULL << core_iap_width) - 1;
	return (1ULL << core_iap_width) - v;
}

static pmc_value_t
iap_reload_count_to_perfctr_value(pmc_value_t rlc)
{
	return (1ULL << core_iap_width) - rlc;
}

static int
iap_pmc_has_overflowed(int ri)
{
	uint64_t v;

	/*
	 * We treat a Core (i.e., Intel architecture v1) PMC as has
	 * having overflowed if its MSB is zero.
	 */
	v = rdpmc(ri);
	return ((v & (1ULL << (core_iap_width - 1))) == 0);
}

/*
 * Check an event against the set of supported architectural events.
 *
 * If the event is not architectural EV_IS_NOTARCH is returned.
 * If the event is architectural and supported on this CPU, the correct
 * event+umask mapping is returned in map, and EV_IS_ARCH_SUPP is returned.
 * Otherwise, the function returns EV_IS_ARCH_NOTSUPP.
 */

static int
iap_is_event_architectural(enum pmc_event pe, enum pmc_event *map)
{
	enum core_arch_events ae;

	switch (pe) {
	case PMC_EV_IAP_ARCH_UNH_COR_CYC:
		ae = CORE_AE_UNHALTED_CORE_CYCLES;
		*map = PMC_EV_IAP_EVENT_3CH_00H;
		break;
	case PMC_EV_IAP_ARCH_INS_RET:
		ae = CORE_AE_INSTRUCTION_RETIRED;
		*map = PMC_EV_IAP_EVENT_C0H_00H;
		break;
	case PMC_EV_IAP_ARCH_UNH_REF_CYC:
		ae = CORE_AE_UNHALTED_REFERENCE_CYCLES;
		*map = PMC_EV_IAP_EVENT_3CH_01H;
		break;
	case PMC_EV_IAP_ARCH_LLC_REF:
		ae = CORE_AE_LLC_REFERENCE;
		*map = PMC_EV_IAP_EVENT_2EH_4FH;
		break;
	case PMC_EV_IAP_ARCH_LLC_MIS:
		ae = CORE_AE_LLC_MISSES;
		*map = PMC_EV_IAP_EVENT_2EH_41H;
		break;
	case PMC_EV_IAP_ARCH_BR_INS_RET:
		ae = CORE_AE_BRANCH_INSTRUCTION_RETIRED;
		*map = PMC_EV_IAP_EVENT_C4H_00H;
		break;
	case PMC_EV_IAP_ARCH_BR_MIS_RET:
		ae = CORE_AE_BRANCH_MISSES_RETIRED;
		*map = PMC_EV_IAP_EVENT_C5H_00H;
		break;

	default:	/* Non architectural event. */
		return (EV_IS_NOTARCH);
	}

	return (((core_architectural_events & (1 << ae)) == 0) ?
	    EV_IS_ARCH_NOTSUPP : EV_IS_ARCH_SUPP);
}

static int
iap_event_corei7_ok_on_counter(enum pmc_event pe, int ri)
{
	uint32_t mask;

	switch (pe) {
		/*
		 * Events valid only on counter 0, 1.
		 */
	case PMC_EV_IAP_EVENT_40H_01H:
	case PMC_EV_IAP_EVENT_40H_02H:
	case PMC_EV_IAP_EVENT_40H_04H:
	case PMC_EV_IAP_EVENT_40H_08H:
	case PMC_EV_IAP_EVENT_40H_0FH:
	case PMC_EV_IAP_EVENT_41H_02H:
	case PMC_EV_IAP_EVENT_41H_04H:
	case PMC_EV_IAP_EVENT_41H_08H:
	case PMC_EV_IAP_EVENT_42H_01H:
	case PMC_EV_IAP_EVENT_42H_02H:
	case PMC_EV_IAP_EVENT_42H_04H:
	case PMC_EV_IAP_EVENT_42H_08H:
	case PMC_EV_IAP_EVENT_43H_01H:
	case PMC_EV_IAP_EVENT_43H_02H:
	case PMC_EV_IAP_EVENT_51H_01H:
	case PMC_EV_IAP_EVENT_51H_02H:
	case PMC_EV_IAP_EVENT_51H_04H:
	case PMC_EV_IAP_EVENT_51H_08H:
	case PMC_EV_IAP_EVENT_63H_01H:
	case PMC_EV_IAP_EVENT_63H_02H:
		mask = 0x3;
		break;

	default:
		mask = ~0;	/* Any row index is ok. */
	}

	return (mask & (1 << ri));
}

static int
iap_event_westmere_ok_on_counter(enum pmc_event pe, int ri)
{
	uint32_t mask;

	switch (pe) {
		/*
		 * Events valid only on counter 0.
		 */
	case PMC_EV_IAP_EVENT_60H_01H:
	case PMC_EV_IAP_EVENT_60H_02H:
	case PMC_EV_IAP_EVENT_60H_04H:
	case PMC_EV_IAP_EVENT_60H_08H:
	case PMC_EV_IAP_EVENT_B3H_01H:
	case PMC_EV_IAP_EVENT_B3H_02H:
	case PMC_EV_IAP_EVENT_B3H_04H:
		mask = 0x1;
		break;

		/*
		 * Events valid only on counter 0, 1.
		 */
	case PMC_EV_IAP_EVENT_4CH_01H:
	case PMC_EV_IAP_EVENT_4EH_01H:
	case PMC_EV_IAP_EVENT_4EH_02H:
	case PMC_EV_IAP_EVENT_4EH_04H:
	case PMC_EV_IAP_EVENT_51H_01H:
	case PMC_EV_IAP_EVENT_51H_02H:
	case PMC_EV_IAP_EVENT_51H_04H:
	case PMC_EV_IAP_EVENT_51H_08H:
	case PMC_EV_IAP_EVENT_63H_01H:
	case PMC_EV_IAP_EVENT_63H_02H:
		mask = 0x3;
		break;

	default:
		mask = ~0;	/* Any row index is ok. */
	}

	return (mask & (1 << ri));
}

static int
iap_event_sb_sbx_ib_ibx_ok_on_counter(enum pmc_event pe, int ri)
{
	uint32_t mask;

	switch (pe) {
		/* Events valid only on counter 0. */
	case PMC_EV_IAP_EVENT_B7H_01H:
		mask = 0x1;
		break;
		/* Events valid only on counter 1. */
	case PMC_EV_IAP_EVENT_C0H_01H:
		mask = 0x2;
		break;
		/* Events valid only on counter 2. */
	case PMC_EV_IAP_EVENT_48H_01H:
	case PMC_EV_IAP_EVENT_A2H_02H:
	case PMC_EV_IAP_EVENT_A3H_08H:
		mask = 0x4;
		break;
		/* Events valid only on counter 3. */
	case PMC_EV_IAP_EVENT_BBH_01H:
	case PMC_EV_IAP_EVENT_CDH_01H:
	case PMC_EV_IAP_EVENT_CDH_02H:
		mask = 0x8;
		break;
	default:
		mask = ~0;	/* Any row index is ok. */
	}

	return (mask & (1 << ri));
}

static int
iap_event_ok_on_counter(enum pmc_event pe, int ri)
{
	uint32_t mask;

	switch (pe) {
		/*
		 * Events valid only on counter 0.
		 */
	case PMC_EV_IAP_EVENT_10H_00H:
	case PMC_EV_IAP_EVENT_14H_00H:
	case PMC_EV_IAP_EVENT_18H_00H:
	case PMC_EV_IAP_EVENT_B3H_01H:
	case PMC_EV_IAP_EVENT_B3H_02H:
	case PMC_EV_IAP_EVENT_B3H_04H:
	case PMC_EV_IAP_EVENT_C1H_00H:
	case PMC_EV_IAP_EVENT_CBH_01H:
	case PMC_EV_IAP_EVENT_CBH_02H:
		mask = (1 << 0);
		break;

		/*
		 * Events valid only on counter 1.
		 */
	case PMC_EV_IAP_EVENT_11H_00H:
	case PMC_EV_IAP_EVENT_12H_00H:
	case PMC_EV_IAP_EVENT_13H_00H:
		mask = (1 << 1);
		break;

	default:
		mask = ~0;	/* Any row index is ok. */
	}

	return (mask & (1 << ri));
}

static int
iap_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	int arch, n, model;
	enum pmc_event ev, map;
	struct iap_event_descr *ie;
	uint32_t c, caps, config, cpuflag, evsel, mask;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row-index value %d", __LINE__, ri));

	/* check requested capabilities */
	caps = a->pm_caps;
	if ((IAP_PMC_CAPS & caps) != caps)
		return (EPERM);
	map = 0;	/* XXX: silent GCC warning */
	arch = iap_is_event_architectural(pm->pm_event, &map);
	if (arch == EV_IS_ARCH_NOTSUPP)
		return (EOPNOTSUPP);
	else if (arch == EV_IS_ARCH_SUPP)
		ev = map;
	else
		ev = pm->pm_event;

	/*
	 * A small number of events are not supported in all the
	 * processors based on a given microarchitecture.
	 */
	if (ev == PMC_EV_IAP_EVENT_0FH_01H || ev == PMC_EV_IAP_EVENT_0FH_80H) {
		model = ((cpu_id & 0xF0000) >> 12) | ((cpu_id & 0xF0) >> 4);
		if (core_cputype == PMC_CPU_INTEL_COREI7 && model != 0x2E)
			return (EINVAL);
	}

	switch (core_cputype) {
	case PMC_CPU_INTEL_COREI7:
	case PMC_CPU_INTEL_NEHALEM_EX:
		if (iap_event_corei7_ok_on_counter(ev, ri) == 0)
			return (EINVAL);
		break;
	case PMC_CPU_INTEL_SKYLAKE:
	case PMC_CPU_INTEL_SKYLAKE_XEON:
	case PMC_CPU_INTEL_BROADWELL:
	case PMC_CPU_INTEL_BROADWELL_XEON:
	case PMC_CPU_INTEL_SANDYBRIDGE:
	case PMC_CPU_INTEL_SANDYBRIDGE_XEON:
	case PMC_CPU_INTEL_IVYBRIDGE:
	case PMC_CPU_INTEL_IVYBRIDGE_XEON:
	case PMC_CPU_INTEL_HASWELL:
	case PMC_CPU_INTEL_HASWELL_XEON:
		if (iap_event_sb_sbx_ib_ibx_ok_on_counter(ev, ri) == 0)
			return (EINVAL);
		break;
	case PMC_CPU_INTEL_WESTMERE:
	case PMC_CPU_INTEL_WESTMERE_EX:
		if (iap_event_westmere_ok_on_counter(ev, ri) == 0)
			return (EINVAL);
		break;
	default:
		if (iap_event_ok_on_counter(ev, ri) == 0)
			return (EINVAL);
	}

	/*
	 * Look for an event descriptor with matching CPU and event id
	 * fields.
	 */

	switch (core_cputype) {
	default:
	case PMC_CPU_INTEL_ATOM:
		cpuflag = IAP_F_CA;
		break;
	case PMC_CPU_INTEL_ATOM_SILVERMONT:
		cpuflag = IAP_F_CAS;
		break;
	case PMC_CPU_INTEL_SKYLAKE_XEON:
		cpuflag = IAP_F_SLX;
		break;
	case PMC_CPU_INTEL_SKYLAKE:
		cpuflag = IAP_F_SL;
		break;
	case PMC_CPU_INTEL_BROADWELL_XEON:
		cpuflag = IAP_F_BWX;
		break;
	case PMC_CPU_INTEL_BROADWELL:
		cpuflag = IAP_F_BW;
		break;
	case PMC_CPU_INTEL_CORE:
		cpuflag = IAP_F_CC;
		break;
	case PMC_CPU_INTEL_CORE2:
		cpuflag = IAP_F_CC2;
		break;
	case PMC_CPU_INTEL_CORE2EXTREME:
		cpuflag = IAP_F_CC2 | IAP_F_CC2E;
		break;
	case PMC_CPU_INTEL_COREI7:
		cpuflag = IAP_F_I7;
		break;
	case PMC_CPU_INTEL_HASWELL:
		cpuflag = IAP_F_HW;
		break;
	case PMC_CPU_INTEL_HASWELL_XEON:
		cpuflag = IAP_F_HWX;
		break;
	case PMC_CPU_INTEL_IVYBRIDGE:
		cpuflag = IAP_F_IB;
		break;
	case PMC_CPU_INTEL_IVYBRIDGE_XEON:
		cpuflag = IAP_F_IBX;
		break;
	case PMC_CPU_INTEL_SANDYBRIDGE:
		cpuflag = IAP_F_SB;
		break;
	case PMC_CPU_INTEL_SANDYBRIDGE_XEON:
		cpuflag = IAP_F_SBX;
		break;
	case PMC_CPU_INTEL_WESTMERE:
		cpuflag = IAP_F_WM;
		break;
	}

	for (n = 0, ie = iap_events; n < nitems(iap_events); n++, ie++)
		if (ie->iap_ev == ev && ie->iap_flags & cpuflag)
			break;

	if (n == nitems(iap_events))
		return (EINVAL);

	/*
	 * A matching event descriptor has been found, so start
	 * assembling the contents of the event select register.
	 */
	evsel = ie->iap_evcode;

	config = a->pm_md.pm_iap.pm_iap_config & ~IAP_F_CMASK;

	/*
	 * If the event uses a fixed umask value, reject any umask
	 * bits set by the user.
	 */
	if (ie->iap_flags & IAP_F_FM) {

		if (IAP_UMASK(config) != 0)
			return (EINVAL);

		evsel |= (ie->iap_umask << 8);

	} else {

		/*
		 * Otherwise, the UMASK value needs to be taken from
		 * the MD fields of the allocation request.  Reject
		 * requests that specify reserved bits.
		 */

		mask = 0;

		if (ie->iap_umask & IAP_M_CORE) {
			if ((c = (config & IAP_F_CORE)) != IAP_CORE_ALL &&
			    c != IAP_CORE_THIS)
				return (EINVAL);
			mask |= IAP_F_CORE;
		}

		if (ie->iap_umask & IAP_M_AGENT)
			mask |= IAP_F_AGENT;

		if (ie->iap_umask & IAP_M_PREFETCH) {

			if ((c = (config & IAP_F_PREFETCH)) ==
			    IAP_PREFETCH_RESERVED)
				return (EINVAL);

			mask |= IAP_F_PREFETCH;
		}

		if (ie->iap_umask & IAP_M_MESI)
			mask |= IAP_F_MESI;

		if (ie->iap_umask & IAP_M_SNOOPRESPONSE)
			mask |= IAP_F_SNOOPRESPONSE;

		if (ie->iap_umask & IAP_M_SNOOPTYPE)
			mask |= IAP_F_SNOOPTYPE;

		if (ie->iap_umask & IAP_M_TRANSITION)
			mask |= IAP_F_TRANSITION;

		/*
		 * If bits outside of the allowed set of umask bits
		 * are set, reject the request.
		 */
		if (config & ~mask)
			return (EINVAL);

		evsel |= (config & mask);

	}

	/*
	 * Only Atom and SandyBridge CPUs support the 'ANY' qualifier.
	 */
	if (core_cputype == PMC_CPU_INTEL_ATOM ||
		core_cputype == PMC_CPU_INTEL_ATOM_SILVERMONT ||
		core_cputype == PMC_CPU_INTEL_SANDYBRIDGE ||
		core_cputype == PMC_CPU_INTEL_SANDYBRIDGE_XEON)
		evsel |= (config & IAP_ANY);
	else if (config & IAP_ANY)
		return (EINVAL);

	/*
	 * Check offcore response configuration.
	 */
	if (a->pm_md.pm_iap.pm_iap_rsp != 0) {
		if (ev != PMC_EV_IAP_EVENT_B7H_01H &&
		    ev != PMC_EV_IAP_EVENT_BBH_01H)
			return (EINVAL);
		if (core_cputype == PMC_CPU_INTEL_COREI7 &&
		    ev == PMC_EV_IAP_EVENT_BBH_01H)
			return (EINVAL);
		if ((core_cputype == PMC_CPU_INTEL_COREI7 ||
		    core_cputype == PMC_CPU_INTEL_WESTMERE ||
		    core_cputype == PMC_CPU_INTEL_NEHALEM_EX ||
		    core_cputype == PMC_CPU_INTEL_WESTMERE_EX) &&
		    a->pm_md.pm_iap.pm_iap_rsp & ~IA_OFFCORE_RSP_MASK_I7WM)
			return (EINVAL);
		else if ((core_cputype == PMC_CPU_INTEL_SANDYBRIDGE ||
			core_cputype == PMC_CPU_INTEL_SANDYBRIDGE_XEON ||
			core_cputype == PMC_CPU_INTEL_IVYBRIDGE ||
			core_cputype == PMC_CPU_INTEL_IVYBRIDGE_XEON) &&
		    a->pm_md.pm_iap.pm_iap_rsp & ~IA_OFFCORE_RSP_MASK_SBIB)
			return (EINVAL);
		pm->pm_md.pm_iap.pm_iap_rsp = a->pm_md.pm_iap.pm_iap_rsp;
	}

	if (caps & PMC_CAP_THRESHOLD)
		evsel |= (a->pm_md.pm_iap.pm_iap_config & IAP_F_CMASK);
	if (caps & PMC_CAP_USER)
		evsel |= IAP_USR;
	if (caps & PMC_CAP_SYSTEM)
		evsel |= IAP_OS;
	if ((caps & (PMC_CAP_USER | PMC_CAP_SYSTEM)) == 0)
		evsel |= (IAP_OS | IAP_USR);
	if (caps & PMC_CAP_EDGE)
		evsel |= IAP_EDGE;
	if (caps & PMC_CAP_INVERT)
		evsel |= IAP_INV;
	if (caps & PMC_CAP_INTERRUPT)
		evsel |= IAP_INT;

	pm->pm_md.pm_iap.pm_iap_evsel = evsel;

	return (0);
}

static int
iap_config_pmc(int cpu, int ri, struct pmc *pm)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU %d", __LINE__, cpu));

	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG3(MDP,CFG,1, "iap-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(core_pcpu[cpu] != NULL, ("[core,%d] null per-cpu %d", __LINE__,
	    cpu));

	core_pcpu[cpu]->pc_corepmcs[ri].phw_pmc = pm;

	return (0);
}

static int
iap_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char iap_name[PMC_NAME_MAX];

	phw = &core_pcpu[cpu]->pc_corepmcs[ri];

	(void) snprintf(iap_name, sizeof(iap_name), "IAP-%d", ri);
	if ((error = copystr(iap_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return (error);

	pi->pm_class = PMC_CLASS_IAP;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = TRUE;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = FALSE;
		*ppmc          = NULL;
	}

	return (0);
}

static int
iap_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = core_pcpu[cpu]->pc_corepmcs[ri].phw_pmc;

	return (0);
}

static int
iap_get_msr(int ri, uint32_t *msr)
{
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[iap,%d] ri %d out of range", __LINE__, ri));

	*msr = ri;

	return (0);
}

static int
iap_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	pm = core_pcpu[cpu]->pc_corepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] cpu %d ri %d pmc not configured", __LINE__, cpu,
		ri));

	tmp = rdpmc(ri);
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = iap_perfctr_value_to_reload_count(tmp);
	else
		*v = tmp & ((1ULL << core_iap_width) - 1);

	PMCDBG4(MDP,REA,1, "iap-read cpu=%d ri=%d msr=0x%x -> v=%jx", cpu, ri,
	    IAP_PMC0 + ri, *v);

	return (0);
}

static int
iap_release_pmc(int cpu, int ri, struct pmc *pm)
{
	(void) pm;

	PMCDBG3(MDP,REL,1, "iap-release cpu=%d ri=%d pm=%p", cpu, ri,
	    pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	KASSERT(core_pcpu[cpu]->pc_corepmcs[ri].phw_pmc
	    == NULL, ("[core,%d] PHW pmc non-NULL", __LINE__));

	return (0);
}

static int
iap_start_pmc(int cpu, int ri)
{
	struct pmc *pm;
	uint32_t evsel;
	struct core_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row-index %d", __LINE__, ri));

	cc = core_pcpu[cpu];
	pm = cc->pc_corepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] starting cpu%d,ri%d with no pmc configured",
		__LINE__, cpu, ri));

	PMCDBG2(MDP,STA,1, "iap-start cpu=%d ri=%d", cpu, ri);

	evsel = pm->pm_md.pm_iap.pm_iap_evsel;

	PMCDBG4(MDP,STA,2, "iap-start/2 cpu=%d ri=%d evselmsr=0x%x evsel=0x%x",
	    cpu, ri, IAP_EVSEL0 + ri, evsel);

	/* Event specific configuration. */
	switch (pm->pm_event) {
	case PMC_EV_IAP_EVENT_B7H_01H:
		wrmsr(IA_OFFCORE_RSP0, pm->pm_md.pm_iap.pm_iap_rsp);
		break;
	case PMC_EV_IAP_EVENT_BBH_01H:
		wrmsr(IA_OFFCORE_RSP1, pm->pm_md.pm_iap.pm_iap_rsp);
		break;
	default:
		break;
	}

	wrmsr(IAP_EVSEL0 + ri, evsel | IAP_EN);

	if (core_cputype == PMC_CPU_INTEL_CORE)
		return (0);

	do {
		cc->pc_resync = 0;
		cc->pc_globalctrl |= (1ULL << ri);
		wrmsr(IA_GLOBAL_CTRL, cc->pc_globalctrl);
	} while (cc->pc_resync != 0);

	return (0);
}

static int
iap_stop_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct core_cpu *cc;
	uint64_t msr;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row index %d", __LINE__, ri));

	cc = core_pcpu[cpu];
	pm = cc->pc_corepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	PMCDBG2(MDP,STO,1, "iap-stop cpu=%d ri=%d", cpu, ri);

	msr = rdmsr(IAP_EVSEL0 + ri) & ~IAP_EVSEL_MASK;
	wrmsr(IAP_EVSEL0 + ri, msr);	/* stop hw */

	if (core_cputype == PMC_CPU_INTEL_CORE)
		return (0);

	msr = 0;
	do {
		cc->pc_resync = 0;
		cc->pc_globalctrl &= ~(1ULL << ri);
		msr = rdmsr(IA_GLOBAL_CTRL) & ~IA_GLOBAL_CTRL_MASK;
		wrmsr(IA_GLOBAL_CTRL, cc->pc_globalctrl);
	} while (cc->pc_resync != 0);

	return (0);
}

static int
iap_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc *pm;
	struct core_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[core,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < core_iap_npmc,
	    ("[core,%d] illegal row index %d", __LINE__, ri));

	cc = core_pcpu[cpu];
	pm = cc->pc_corepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[core,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = iap_reload_count_to_perfctr_value(v);

	v &= (1ULL << core_iap_width) - 1;

	PMCDBG4(MDP,WRI,1, "iap-write cpu=%d ri=%d msr=0x%x v=%jx", cpu, ri,
	    IAP_PMC0 + ri, v);

	/*
	 * Write the new value to the counter (or it's alias).  The
	 * counter will be in a stopped state when the pcd_write()
	 * entry point is called.
	 */
	wrmsr(core_iap_wroffset + IAP_PMC0 + ri, v);
	return (0);
}


static void
iap_initialize(struct pmc_mdep *md, int maxcpu, int npmc, int pmcwidth,
    int flags)
{
	struct pmc_classdep *pcd;

	KASSERT(md != NULL, ("[iap,%d] md is NULL", __LINE__));

	PMCDBG0(MDP,INI,1, "iap-initialize");

	/* Remember the set of architectural events supported. */
	core_architectural_events = ~flags;

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_IAP];

	pcd->pcd_caps	= IAP_PMC_CAPS;
	pcd->pcd_class	= PMC_CLASS_IAP;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= pmcwidth;

	pcd->pcd_allocate_pmc	= iap_allocate_pmc;
	pcd->pcd_config_pmc	= iap_config_pmc;
	pcd->pcd_describe	= iap_describe;
	pcd->pcd_get_config	= iap_get_config;
	pcd->pcd_get_msr	= iap_get_msr;
	pcd->pcd_pcpu_fini	= core_pcpu_fini;
	pcd->pcd_pcpu_init	= core_pcpu_init;
	pcd->pcd_read_pmc	= iap_read_pmc;
	pcd->pcd_release_pmc	= iap_release_pmc;
	pcd->pcd_start_pmc	= iap_start_pmc;
	pcd->pcd_stop_pmc	= iap_stop_pmc;
	pcd->pcd_write_pmc	= iap_write_pmc;

	md->pmd_npmc	       += npmc;
}

static int
core_intr(int cpu, struct trapframe *tf)
{
	pmc_value_t v;
	struct pmc *pm;
	struct core_cpu *cc;
	int error, found_interrupt, ri;
	uint64_t msr;

	PMCDBG3(MDP,INT, 1, "cpu=%d tf=0x%p um=%d", cpu, (void *) tf,
	    TRAPF_USERMODE(tf));

	found_interrupt = 0;
	cc = core_pcpu[cpu];

	for (ri = 0; ri < core_iap_npmc; ri++) {

		if ((pm = cc->pc_corepmcs[ri].phw_pmc) == NULL ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
			continue;

		if (!iap_pmc_has_overflowed(ri))
			continue;

		found_interrupt = 1;

		if (pm->pm_state != PMC_STATE_RUNNING)
			continue;

		error = pmc_process_interrupt(cpu, PMC_HR, pm, tf,
		    TRAPF_USERMODE(tf));

		v = pm->pm_sc.pm_reloadcount;
		v = iap_reload_count_to_perfctr_value(v);

		/*
		 * Stop the counter, reload it but only restart it if
		 * the PMC is not stalled.
		 */
		msr = rdmsr(IAP_EVSEL0 + ri) & ~IAP_EVSEL_MASK;
		wrmsr(IAP_EVSEL0 + ri, msr);
		wrmsr(core_iap_wroffset + IAP_PMC0 + ri, v);

		if (error)
			continue;

		wrmsr(IAP_EVSEL0 + ri, msr | (pm->pm_md.pm_iap.pm_iap_evsel |
					      IAP_EN));
	}

	if (found_interrupt)
		lapic_reenable_pmc();

	atomic_add_int(found_interrupt ? &pmc_stats.pm_intr_processed :
	    &pmc_stats.pm_intr_ignored, 1);

	return (found_interrupt);
}

static int
core2_intr(int cpu, struct trapframe *tf)
{
	int error, found_interrupt, n;
	uint64_t flag, intrstatus, intrenable, msr;
	struct pmc *pm;
	struct core_cpu *cc;
	pmc_value_t v;

	PMCDBG3(MDP,INT, 1, "cpu=%d tf=0x%p um=%d", cpu, (void *) tf,
	    TRAPF_USERMODE(tf));

	/*
	 * The IA_GLOBAL_STATUS (MSR 0x38E) register indicates which
	 * PMCs have a pending PMI interrupt.  We take a 'snapshot' of
	 * the current set of interrupting PMCs and process these
	 * after stopping them.
	 */
	intrstatus = rdmsr(IA_GLOBAL_STATUS);
	intrenable = intrstatus & core_pmcmask;

	PMCDBG2(MDP,INT, 1, "cpu=%d intrstatus=%jx", cpu,
	    (uintmax_t) intrstatus);

	found_interrupt = 0;
	cc = core_pcpu[cpu];

	KASSERT(cc != NULL, ("[core,%d] null pcpu", __LINE__));

	cc->pc_globalctrl &= ~intrenable;
	cc->pc_resync = 1;	/* MSRs now potentially out of sync. */

	/*
	 * Stop PMCs and clear overflow status bits.
	 */
	msr = rdmsr(IA_GLOBAL_CTRL) & ~IA_GLOBAL_CTRL_MASK;
	wrmsr(IA_GLOBAL_CTRL, msr);
	wrmsr(IA_GLOBAL_OVF_CTRL, intrenable |
	    IA_GLOBAL_STATUS_FLAG_OVFBUF |
	    IA_GLOBAL_STATUS_FLAG_CONDCHG);

	/*
	 * Look for interrupts from fixed function PMCs.
	 */
	for (n = 0, flag = (1ULL << IAF_OFFSET); n < core_iaf_npmc;
	     n++, flag <<= 1) {

		if ((intrstatus & flag) == 0)
			continue;

		found_interrupt = 1;

		pm = cc->pc_corepmcs[n + core_iaf_ri].phw_pmc;
		if (pm == NULL || pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
			continue;

		error = pmc_process_interrupt(cpu, PMC_HR, pm, tf,
		    TRAPF_USERMODE(tf));
		if (error)
			intrenable &= ~flag;

		v = iaf_reload_count_to_perfctr_value(pm->pm_sc.pm_reloadcount);

		/* Reload sampling count. */
		wrmsr(IAF_CTR0 + n, v);

		PMCDBG4(MDP,INT, 1, "iaf-intr cpu=%d error=%d v=%jx(%jx)", cpu,
		    error, (uintmax_t) v, (uintmax_t) rdpmc(IAF_RI_TO_MSR(n)));
	}

	/*
	 * Process interrupts from the programmable counters.
	 */
	for (n = 0, flag = 1; n < core_iap_npmc; n++, flag <<= 1) {
		if ((intrstatus & flag) == 0)
			continue;

		found_interrupt = 1;

		pm = cc->pc_corepmcs[n].phw_pmc;
		if (pm == NULL || pm->pm_state != PMC_STATE_RUNNING ||
		    !PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
			continue;

		error = pmc_process_interrupt(cpu, PMC_HR, pm, tf,
		    TRAPF_USERMODE(tf));
		if (error)
			intrenable &= ~flag;

		v = iap_reload_count_to_perfctr_value(pm->pm_sc.pm_reloadcount);

		PMCDBG3(MDP,INT, 1, "iap-intr cpu=%d error=%d v=%jx", cpu, error,
		    (uintmax_t) v);

		/* Reload sampling count. */
		wrmsr(core_iap_wroffset + IAP_PMC0 + n, v);
	}

	/*
	 * Reenable all non-stalled PMCs.
	 */
	PMCDBG2(MDP,INT, 1, "cpu=%d intrenable=%jx", cpu,
	    (uintmax_t) intrenable);

	cc->pc_globalctrl |= intrenable;

	wrmsr(IA_GLOBAL_CTRL, cc->pc_globalctrl & IA_GLOBAL_CTRL_MASK);

	PMCDBG5(MDP,INT, 1, "cpu=%d fixedctrl=%jx globalctrl=%jx status=%jx "
	    "ovf=%jx", cpu, (uintmax_t) rdmsr(IAF_CTRL),
	    (uintmax_t) rdmsr(IA_GLOBAL_CTRL),
	    (uintmax_t) rdmsr(IA_GLOBAL_STATUS),
	    (uintmax_t) rdmsr(IA_GLOBAL_OVF_CTRL));

	if (found_interrupt)
		lapic_reenable_pmc();

	atomic_add_int(found_interrupt ? &pmc_stats.pm_intr_processed :
	    &pmc_stats.pm_intr_ignored, 1);

	return (found_interrupt);
}

int
pmc_core_initialize(struct pmc_mdep *md, int maxcpu, int version_override)
{
	int cpuid[CORE_CPUID_REQUEST_SIZE];
	int ipa_version, flags, nflags;

	do_cpuid(CORE_CPUID_REQUEST, cpuid);

	ipa_version = (version_override > 0) ? version_override :
	    cpuid[CORE_CPUID_EAX] & 0xFF;
	core_cputype = md->pmd_cputype;

	PMCDBG3(MDP,INI,1,"core-init cputype=%d ncpu=%d ipa-version=%d",
	    core_cputype, maxcpu, ipa_version);

	if (ipa_version < 1 || ipa_version > 4 ||
	    (core_cputype != PMC_CPU_INTEL_CORE && ipa_version == 1)) {
		/* Unknown PMC architecture. */
		printf("hwpc_core: unknown PMC architecture: %d\n",
		    ipa_version);
		return (EPROGMISMATCH);
	}

	core_iap_wroffset = 0;
	if (cpu_feature2 & CPUID2_PDCM) {
		if (rdmsr(IA32_PERF_CAPABILITIES) & PERFCAP_FW_WRITE) {
			PMCDBG0(MDP, INI, 1,
			    "core-init full-width write supported");
			core_iap_wroffset = IAP_A_PMC0 - IAP_PMC0;
		} else
			PMCDBG0(MDP, INI, 1,
			    "core-init full-width write NOT supported");
	} else
		PMCDBG0(MDP, INI, 1, "core-init pdcm not supported");

	core_pmcmask = 0;

	/*
	 * Initialize programmable counters.
	 */
	core_iap_npmc = (cpuid[CORE_CPUID_EAX] >> 8) & 0xFF;
	core_iap_width = (cpuid[CORE_CPUID_EAX] >> 16) & 0xFF;

	core_pmcmask |= ((1ULL << core_iap_npmc) - 1);

	nflags = (cpuid[CORE_CPUID_EAX] >> 24) & 0xFF;
	flags = cpuid[CORE_CPUID_EBX] & ((1 << nflags) - 1);

	iap_initialize(md, maxcpu, core_iap_npmc, core_iap_width, flags);

	/*
	 * Initialize fixed function counters, if present.
	 */
	if (core_cputype != PMC_CPU_INTEL_CORE) {
		core_iaf_ri = core_iap_npmc;
		core_iaf_npmc = cpuid[CORE_CPUID_EDX] & 0x1F;
		core_iaf_width = (cpuid[CORE_CPUID_EDX] >> 5) & 0xFF;

		iaf_initialize(md, maxcpu, core_iaf_npmc, core_iaf_width);
		core_pmcmask |= ((1ULL << core_iaf_npmc) - 1) << IAF_OFFSET;
	}

	PMCDBG2(MDP,INI,1,"core-init pmcmask=0x%jx iafri=%d", core_pmcmask,
	    core_iaf_ri);

	core_pcpu = malloc(sizeof(*core_pcpu) * maxcpu, M_PMC,
	    M_ZERO | M_WAITOK);

	/*
	 * Choose the appropriate interrupt handler.
	 */
	if (ipa_version == 1)
		md->pmd_intr = core_intr;
	else
		md->pmd_intr = core2_intr;

	md->pmd_pcpu_fini = NULL;
	md->pmd_pcpu_init = NULL;

	return (0);
}

void
pmc_core_finalize(struct pmc_mdep *md)
{
	PMCDBG0(MDP,INI,1, "core-finalize");

	free(core_pcpu, M_PMC);
	core_pcpu = NULL;
}
