/*-
 * Copyright (c) 2010 Fabien Thomas
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
 * Intel Uncore PMCs.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/systm.h>

#include <machine/intr_machdep.h>
#if (__FreeBSD_version >= 1100000)
#include <x86/apicvar.h>
#else
#include <machine/apicvar.h>
#endif
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/specialreg.h>

#define	UCF_PMC_CAPS \
	(PMC_CAP_READ | PMC_CAP_WRITE)

#define	UCP_PMC_CAPS \
    (PMC_CAP_EDGE | PMC_CAP_THRESHOLD | PMC_CAP_READ | PMC_CAP_WRITE | \
    PMC_CAP_INVERT | PMC_CAP_QUALIFIER | PMC_CAP_PRECISE)

#define	SELECTSEL(x) \
	(((x) == PMC_CPU_INTEL_SANDYBRIDGE || (x) == PMC_CPU_INTEL_HASWELL) ? \
	UCP_CB0_EVSEL0 : UCP_EVSEL0)

#define SELECTOFF(x) \
	(((x) == PMC_CPU_INTEL_SANDYBRIDGE || (x) == PMC_CPU_INTEL_HASWELL) ? \
	UCF_OFFSET_SB : UCF_OFFSET)

static enum pmc_cputype	uncore_cputype;

struct uncore_cpu {
	volatile uint32_t	pc_resync;
	volatile uint32_t	pc_ucfctrl;	/* Fixed function control. */
	volatile uint64_t	pc_globalctrl;	/* Global control register. */
	struct pmc_hw		pc_uncorepmcs[];
};

static struct uncore_cpu **uncore_pcpu;

static uint64_t uncore_pmcmask;

static int uncore_ucf_ri;		/* relative index of fixed counters */
static int uncore_ucf_width;
static int uncore_ucf_npmc;

static int uncore_ucp_width;
static int uncore_ucp_npmc;

static int
uncore_pcpu_noop(struct pmc_mdep *md, int cpu)
{
	(void) md;
	(void) cpu;
	return (0);
}

static int
uncore_pcpu_init(struct pmc_mdep *md, int cpu)
{
	struct pmc_cpu *pc;
	struct uncore_cpu *cc;
	struct pmc_hw *phw;
	int uncore_ri, n, npmc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[ucf,%d] insane cpu number %d", __LINE__, cpu));

	PMCDBG1(MDP,INI,1,"uncore-init cpu=%d", cpu);

	uncore_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCP].pcd_ri;
	npmc = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCP].pcd_num;
	npmc += md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCF].pcd_num;

	cc = malloc(sizeof(struct uncore_cpu) + npmc * sizeof(struct pmc_hw),
	    M_PMC, M_WAITOK | M_ZERO);

	uncore_pcpu[cpu] = cc;
	pc = pmc_pcpu[cpu];

	KASSERT(pc != NULL && cc != NULL,
	    ("[uncore,%d] NULL per-cpu structures cpu=%d", __LINE__, cpu));

	for (n = 0, phw = cc->pc_uncorepmcs; n < npmc; n++, phw++) {
		phw->phw_state 	  = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) |
		    PMC_PHW_INDEX_TO_STATE(n + uncore_ri);
		phw->phw_pmc	  = NULL;
		pc->pc_hwpmcs[n + uncore_ri]  = phw;
	}

	return (0);
}

static int
uncore_pcpu_fini(struct pmc_mdep *md, int cpu)
{
	int uncore_ri, n, npmc;
	struct pmc_cpu *pc;
	struct uncore_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] insane cpu number (%d)", __LINE__, cpu));

	PMCDBG1(MDP,INI,1,"uncore-pcpu-fini cpu=%d", cpu);

	if ((cc = uncore_pcpu[cpu]) == NULL)
		return (0);

	uncore_pcpu[cpu] = NULL;

	pc = pmc_pcpu[cpu];

	KASSERT(pc != NULL, ("[uncore,%d] NULL per-cpu %d state", __LINE__,
		cpu));

	npmc = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCP].pcd_num;
	uncore_ri = md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCP].pcd_ri;

	for (n = 0; n < npmc; n++) 
		wrmsr(SELECTSEL(uncore_cputype) + n, 0);

	wrmsr(UCF_CTRL, 0);
	npmc += md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCF].pcd_num;

	for (n = 0; n < npmc; n++)
		pc->pc_hwpmcs[n + uncore_ri] = NULL;

	free(cc, M_PMC);

	return (0);
}

/*
 * Fixed function counters.
 */

static pmc_value_t
ucf_perfctr_value_to_reload_count(pmc_value_t v)
{
	v &= (1ULL << uncore_ucf_width) - 1;
	return (1ULL << uncore_ucf_width) - v;
}

static pmc_value_t
ucf_reload_count_to_perfctr_value(pmc_value_t rlc)
{
	return (1ULL << uncore_ucf_width) - rlc;
}

static int
ucf_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	enum pmc_event ev;
	uint32_t caps, flags;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU %d", __LINE__, cpu));

	PMCDBG2(MDP,ALL,1, "ucf-allocate ri=%d reqcaps=0x%x", ri, pm->pm_caps);

	if (ri < 0 || ri > uncore_ucf_npmc)
		return (EINVAL);

	caps = a->pm_caps;

	if (a->pm_class != PMC_CLASS_UCF ||
	    (caps & UCF_PMC_CAPS) != caps)
		return (EINVAL);

	ev = pm->pm_event;
	if (ev < PMC_EV_UCF_FIRST || ev > PMC_EV_UCF_LAST)
		return (EINVAL);

	flags = UCF_EN;

	pm->pm_md.pm_ucf.pm_ucf_ctrl = (flags << (ri * 4));

	PMCDBG1(MDP,ALL,2, "ucf-allocate config=0x%jx",
	    (uintmax_t) pm->pm_md.pm_ucf.pm_ucf_ctrl);

	return (0);
}

static int
ucf_config_pmc(int cpu, int ri, struct pmc *pm)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU %d", __LINE__, cpu));

	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG3(MDP,CFG,1, "ucf-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(uncore_pcpu[cpu] != NULL, ("[uncore,%d] null per-cpu %d", __LINE__,
	    cpu));

	uncore_pcpu[cpu]->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc = pm;

	return (0);
}

static int
ucf_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char ucf_name[PMC_NAME_MAX];

	phw = &uncore_pcpu[cpu]->pc_uncorepmcs[ri + uncore_ucf_ri];

	(void) snprintf(ucf_name, sizeof(ucf_name), "UCF-%d", ri);
	if ((error = copystr(ucf_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return (error);

	pi->pm_class = PMC_CLASS_UCF;

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
ucf_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = uncore_pcpu[cpu]->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc;

	return (0);
}

static int
ucf_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	pm = uncore_pcpu[cpu]->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] cpu %d ri %d(%d) pmc not configured", __LINE__, cpu,
		ri, ri + uncore_ucf_ri));

	tmp = rdmsr(UCF_CTR0 + ri);

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = ucf_perfctr_value_to_reload_count(tmp);
	else
		*v = tmp;

	PMCDBG3(MDP,REA,1, "ucf-read cpu=%d ri=%d -> v=%jx", cpu, ri, *v);

	return (0);
}

static int
ucf_release_pmc(int cpu, int ri, struct pmc *pmc)
{
	PMCDBG3(MDP,REL,1, "ucf-release cpu=%d ri=%d pm=%p", cpu, ri, pmc);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	KASSERT(uncore_pcpu[cpu]->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc == NULL,
	    ("[uncore,%d] PHW pmc non-NULL", __LINE__));

	return (0);
}

static int
ucf_start_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct uncore_cpu *ucfc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG2(MDP,STA,1,"ucf-start cpu=%d ri=%d", cpu, ri);

	ucfc = uncore_pcpu[cpu];
	pm = ucfc->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc;

	ucfc->pc_ucfctrl |= pm->pm_md.pm_ucf.pm_ucf_ctrl;

	wrmsr(UCF_CTRL, ucfc->pc_ucfctrl);

	do {
		ucfc->pc_resync = 0;
		ucfc->pc_globalctrl |= (1ULL << (ri + SELECTOFF(uncore_cputype)));
		wrmsr(UC_GLOBAL_CTRL, ucfc->pc_globalctrl);
	} while (ucfc->pc_resync != 0);

	PMCDBG4(MDP,STA,1,"ucfctrl=%x(%x) globalctrl=%jx(%jx)",
	    ucfc->pc_ucfctrl, (uint32_t) rdmsr(UCF_CTRL),
	    ucfc->pc_globalctrl, rdmsr(UC_GLOBAL_CTRL));

	return (0);
}

static int
ucf_stop_pmc(int cpu, int ri)
{
	uint32_t fc;
	struct uncore_cpu *ucfc;

	PMCDBG2(MDP,STO,1,"ucf-stop cpu=%d ri=%d", cpu, ri);

	ucfc = uncore_pcpu[cpu];

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	fc = (UCF_MASK << (ri * 4));

	ucfc->pc_ucfctrl &= ~fc;

	PMCDBG1(MDP,STO,1,"ucf-stop ucfctrl=%x", ucfc->pc_ucfctrl);
	wrmsr(UCF_CTRL, ucfc->pc_ucfctrl);

	do {
		ucfc->pc_resync = 0;
		ucfc->pc_globalctrl &= ~(1ULL << (ri + SELECTOFF(uncore_cputype)));
		wrmsr(UC_GLOBAL_CTRL, ucfc->pc_globalctrl);
	} while (ucfc->pc_resync != 0);

	PMCDBG4(MDP,STO,1,"ucfctrl=%x(%x) globalctrl=%jx(%jx)",
	    ucfc->pc_ucfctrl, (uint32_t) rdmsr(UCF_CTRL),
	    ucfc->pc_globalctrl, rdmsr(UC_GLOBAL_CTRL));

	return (0);
}

static int
ucf_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct uncore_cpu *cc;
	struct pmc *pm;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucf_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	cc = uncore_pcpu[cpu];
	pm = cc->pc_uncorepmcs[ri + uncore_ucf_ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] cpu %d ri %d pmc not configured", __LINE__, cpu, ri));

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = ucf_reload_count_to_perfctr_value(v);

	wrmsr(UCF_CTRL, 0);	/* Turn off fixed counters */
	wrmsr(UCF_CTR0 + ri, v);
	wrmsr(UCF_CTRL, cc->pc_ucfctrl);

	PMCDBG4(MDP,WRI,1, "ucf-write cpu=%d ri=%d v=%jx ucfctrl=%jx ",
	    cpu, ri, v, (uintmax_t) rdmsr(UCF_CTRL));

	return (0);
}


static void
ucf_initialize(struct pmc_mdep *md, int maxcpu, int npmc, int pmcwidth)
{
	struct pmc_classdep *pcd;

	KASSERT(md != NULL, ("[ucf,%d] md is NULL", __LINE__));

	PMCDBG0(MDP,INI,1, "ucf-initialize");

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCF];

	pcd->pcd_caps	= UCF_PMC_CAPS;
	pcd->pcd_class	= PMC_CLASS_UCF;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= pmcwidth;

	pcd->pcd_allocate_pmc	= ucf_allocate_pmc;
	pcd->pcd_config_pmc	= ucf_config_pmc;
	pcd->pcd_describe	= ucf_describe;
	pcd->pcd_get_config	= ucf_get_config;
	pcd->pcd_get_msr	= NULL;
	pcd->pcd_pcpu_fini	= uncore_pcpu_noop;
	pcd->pcd_pcpu_init	= uncore_pcpu_noop;
	pcd->pcd_read_pmc	= ucf_read_pmc;
	pcd->pcd_release_pmc	= ucf_release_pmc;
	pcd->pcd_start_pmc	= ucf_start_pmc;
	pcd->pcd_stop_pmc	= ucf_stop_pmc;
	pcd->pcd_write_pmc	= ucf_write_pmc;

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

struct ucp_event_descr {
	enum pmc_event	ucp_ev;
	unsigned char	ucp_evcode;
	unsigned char	ucp_umask;
	unsigned char	ucp_flags;
};

#define	UCP_F_I7	(1 << 0)	/* CPU: Core i7 */
#define	UCP_F_WM	(1 << 1)	/* CPU: Westmere */
#define	UCP_F_SB	(1 << 2)	/* CPU: Sandy Bridge */
#define	UCP_F_HW	(1 << 3)	/* CPU: Haswell */
#define	UCP_F_FM	(1 << 4)	/* Fixed mask */

#define	UCP_F_ALLCPUS					\
    (UCP_F_I7 | UCP_F_WM)

#define	UCP_F_CMASK		0xFF000000

static struct ucp_event_descr ucp_events[] = {
#undef UCPDESCR
#define	UCPDESCR(N,EV,UM,FLAGS) {					\
	.ucp_ev = PMC_EV_UCP_EVENT_##N,					\
	.ucp_evcode = (EV),						\
	.ucp_umask = (UM),						\
	.ucp_flags = (FLAGS)						\
	}

    UCPDESCR(00H_01H, 0x00, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(00H_02H, 0x00, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(00H_04H, 0x00, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(01H_01H, 0x01, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(01H_02H, 0x01, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(01H_04H, 0x01, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(02H_01H, 0x02, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_01H, 0x03, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_02H, 0x03, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_04H, 0x03, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_08H, 0x03, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_10H, 0x03, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_20H, 0x03, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(03H_40H, 0x03, 0x40, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(04H_01H, 0x04, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(04H_02H, 0x04, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(04H_04H, 0x04, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(04H_08H, 0x04, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(04H_10H, 0x04, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(05H_01H, 0x05, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(05H_02H, 0x05, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(05H_04H, 0x05, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(06H_01H, 0x06, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(06H_02H, 0x06, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(06H_04H, 0x06, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(06H_08H, 0x06, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(06H_10H, 0x06, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(06H_20H, 0x06, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(07H_01H, 0x07, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_02H, 0x07, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_04H, 0x07, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_08H, 0x07, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_10H, 0x07, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_20H, 0x07, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(07H_24H, 0x07, 0x24, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(08H_01H, 0x08, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(08H_02H, 0x08, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(08H_04H, 0x08, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(08H_03H, 0x08, 0x03, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(09H_01H, 0x09, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(09H_02H, 0x09, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(09H_04H, 0x09, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(09H_03H, 0x09, 0x03, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(0AH_01H, 0x0A, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0AH_02H, 0x0A, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0AH_04H, 0x0A, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0AH_08H, 0x0A, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0AH_0FH, 0x0A, 0x0F, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(0BH_01H, 0x0B, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0BH_02H, 0x0B, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0BH_04H, 0x0B, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0BH_08H, 0x0B, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0BH_10H, 0x0B, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(0BH_1FH, 0x0B, 0x1F, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(0CH_01H, 0x0C, 0x01, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_02H, 0x0C, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_04H_E, 0x0C, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_04H_F, 0x0C, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_04H_M, 0x0C, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_04H_S, 0x0C, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_08H_E, 0x0C, 0x08, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_08H_F, 0x0C, 0x08, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_08H_M, 0x0C, 0x08, UCP_F_FM | UCP_F_WM),
    UCPDESCR(0CH_08H_S, 0x0C, 0x08, UCP_F_FM | UCP_F_WM),

    UCPDESCR(20H_01H, 0x20, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(20H_02H, 0x20, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(20H_04H, 0x20, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(20H_08H, 0x20, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(20H_10H, 0x20, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(20H_20H, 0x20, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(21H_01H, 0x21, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(21H_02H, 0x21, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(21H_04H, 0x21, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(22H_01H, 0x22, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM |
	UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_02H, 0x22, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM |
	UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_04H, 0x22, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM |
	UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_08H, 0x22, 0x08, UCP_F_FM | UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_10H, 0x22, 0x10, UCP_F_FM | UCP_F_HW),
    UCPDESCR(22H_20H, 0x22, 0x20, UCP_F_FM | UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_40H, 0x22, 0x40, UCP_F_FM | UCP_F_SB | UCP_F_HW),
    UCPDESCR(22H_80H, 0x22, 0x80, UCP_F_FM | UCP_F_SB | UCP_F_HW),

    UCPDESCR(23H_01H, 0x23, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(23H_02H, 0x23, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(23H_04H, 0x23, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(24H_02H, 0x24, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(24H_04H, 0x24, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(25H_01H, 0x25, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(25H_02H, 0x25, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(25H_04H, 0x25, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(26H_01H, 0x26, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(27H_01H, 0x27, 0x01, UCP_F_FM | UCP_F_I7),
    UCPDESCR(27H_02H, 0x27, 0x02, UCP_F_FM | UCP_F_I7),
    UCPDESCR(27H_04H, 0x27, 0x04, UCP_F_FM | UCP_F_I7),
    UCPDESCR(27H_08H, 0x27, 0x08, UCP_F_FM | UCP_F_I7),
    UCPDESCR(27H_10H, 0x27, 0x10, UCP_F_FM | UCP_F_I7),
    UCPDESCR(27H_20H, 0x27, 0x20, UCP_F_FM | UCP_F_I7),

    UCPDESCR(28H_01H, 0x28, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(28H_02H, 0x28, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(28H_04H, 0x28, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(28H_08H, 0x28, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(28H_10H, 0x28, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(28H_20H, 0x28, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(29H_01H, 0x29, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(29H_02H, 0x29, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(29H_04H, 0x29, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(29H_08H, 0x29, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(29H_10H, 0x29, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(29H_20H, 0x29, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(2AH_01H, 0x2A, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2AH_02H, 0x2A, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2AH_04H, 0x2A, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2AH_07H, 0x2A, 0x07, UCP_F_FM | UCP_F_WM),

    UCPDESCR(2BH_01H, 0x2B, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2BH_02H, 0x2B, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2BH_04H, 0x2B, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2BH_07H, 0x2B, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(2CH_01H, 0x2C, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2CH_02H, 0x2C, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2CH_04H, 0x2C, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2CH_07H, 0x2C, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(2DH_01H, 0x2D, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2DH_02H, 0x2D, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2DH_04H, 0x2D, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2DH_07H, 0x2D, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(2EH_01H, 0x2E, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2EH_02H, 0x2E, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2EH_04H, 0x2E, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2EH_07H, 0x2E, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(2FH_01H, 0x2F, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_02H, 0x2F, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_04H, 0x2F, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_07H, 0x2F, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_08H, 0x2F, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_10H, 0x2F, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_20H, 0x2F, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(2FH_38H, 0x2F, 0x38, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(30H_01H, 0x30, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(30H_02H, 0x30, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(30H_04H, 0x30, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(30H_07H, 0x30, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(31H_01H, 0x31, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(31H_02H, 0x31, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(31H_04H, 0x31, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(31H_07H, 0x31, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(32H_01H, 0x32, 0x01, UCP_F_FM | UCP_F_WM),
    UCPDESCR(32H_02H, 0x32, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(32H_04H, 0x32, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(32H_07H, 0x32, 0x07, UCP_F_FM | UCP_F_WM),

    UCPDESCR(33H_01H, 0x33, 0x01, UCP_F_FM | UCP_F_WM),
    UCPDESCR(33H_02H, 0x33, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(33H_04H, 0x33, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(33H_07H, 0x33, 0x07, UCP_F_FM | UCP_F_WM),

    UCPDESCR(34H_01H, 0x34, 0x01, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(34H_02H, 0x34, 0x02, UCP_F_FM | UCP_F_WM | UCP_F_SB),
    UCPDESCR(34H_04H, 0x34, 0x04, UCP_F_FM | UCP_F_WM | UCP_F_SB),
    UCPDESCR(34H_06H, 0x34, 0x06, UCP_F_FM | UCP_F_HW),
    UCPDESCR(34H_08H, 0x34, 0x08, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(34H_10H, 0x34, 0x10, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(34H_20H, 0x34, 0x20, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(34H_40H, 0x34, 0x40, UCP_F_FM | UCP_F_SB | UCP_F_HW),
    UCPDESCR(34H_80H, 0x34, 0x80, UCP_F_FM | UCP_F_SB | UCP_F_HW),

    UCPDESCR(35H_01H, 0x35, 0x01, UCP_F_FM | UCP_F_WM),
    UCPDESCR(35H_02H, 0x35, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(35H_04H, 0x35, 0x04, UCP_F_FM | UCP_F_WM),

    UCPDESCR(40H_01H, 0x40, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_02H, 0x40, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_04H, 0x40, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_08H, 0x40, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_10H, 0x40, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_20H, 0x40, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_07H, 0x40, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(40H_38H, 0x40, 0x38, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(41H_01H, 0x41, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_02H, 0x41, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_04H, 0x41, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_08H, 0x41, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_10H, 0x41, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_20H, 0x41, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_07H, 0x41, 0x07, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(41H_38H, 0x41, 0x38, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(42H_01H, 0x42, 0x01, UCP_F_FM | UCP_F_WM),
    UCPDESCR(42H_02H, 0x42, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(42H_04H, 0x42, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(42H_08H, 0x42, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(43H_01H, 0x43, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(43H_02H, 0x43, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(60H_01H, 0x60, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(60H_02H, 0x60, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(60H_04H, 0x60, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(61H_01H, 0x61, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(61H_02H, 0x61, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(61H_04H, 0x61, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(62H_01H, 0x62, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(62H_02H, 0x62, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(62H_04H, 0x62, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(63H_01H, 0x63, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(63H_02H, 0x63, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(63H_04H, 0x63, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(63H_08H, 0x63, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(63H_10H, 0x63, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(63H_20H, 0x63, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(64H_01H, 0x64, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(64H_02H, 0x64, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(64H_04H, 0x64, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(64H_08H, 0x64, 0x08, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(64H_10H, 0x64, 0x10, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(64H_20H, 0x64, 0x20, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(65H_01H, 0x65, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(65H_02H, 0x65, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(65H_04H, 0x65, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(66H_01H, 0x66, 0x01, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(66H_02H, 0x66, 0x02, UCP_F_FM | UCP_F_I7 | UCP_F_WM),
    UCPDESCR(66H_04H, 0x66, 0x04, UCP_F_FM | UCP_F_I7 | UCP_F_WM),

    UCPDESCR(67H_01H, 0x67, 0x01, UCP_F_FM | UCP_F_WM),

    UCPDESCR(80H_01H, 0x80, 0x01, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(80H_02H, 0x80, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(80H_04H, 0x80, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(80H_08H, 0x80, 0x08, UCP_F_FM | UCP_F_WM),

    UCPDESCR(81H_01H, 0x81, 0x01, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(81H_02H, 0x81, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(81H_04H, 0x81, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(81H_08H, 0x81, 0x08, UCP_F_FM | UCP_F_WM),
    UCPDESCR(81H_20H, 0x81, 0x20, UCP_F_FM | UCP_F_SB | UCP_F_HW),
    UCPDESCR(81H_80H, 0x81, 0x80, UCP_F_FM | UCP_F_SB | UCP_F_HW),

    UCPDESCR(82H_01H, 0x82, 0x01, UCP_F_FM | UCP_F_WM),

    UCPDESCR(83H_01H, 0x83, 0x01, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(83H_02H, 0x83, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(83H_04H, 0x83, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(83H_08H, 0x83, 0x08, UCP_F_FM | UCP_F_WM),

    UCPDESCR(84H_01H, 0x84, 0x01, UCP_F_FM | UCP_F_WM | UCP_F_SB |
	UCP_F_HW),
    UCPDESCR(84H_02H, 0x84, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(84H_04H, 0x84, 0x04, UCP_F_FM | UCP_F_WM),
    UCPDESCR(84H_08H, 0x84, 0x08, UCP_F_FM | UCP_F_WM),
    UCPDESCR(85H_02H, 0x85, 0x02, UCP_F_FM | UCP_F_WM),
    UCPDESCR(86H_01H, 0x86, 0x01, UCP_F_FM | UCP_F_WM)
};

static pmc_value_t
ucp_perfctr_value_to_reload_count(pmc_value_t v)
{
	v &= (1ULL << uncore_ucp_width) - 1;
	return (1ULL << uncore_ucp_width) - v;
}

static pmc_value_t
ucp_reload_count_to_perfctr_value(pmc_value_t rlc)
{
	return (1ULL << uncore_ucp_width) - rlc;
}

/*
 * Counter specific event information for Sandybridge and Haswell
 */
static int
ucp_event_sb_hw_ok_on_counter(enum pmc_event pe, int ri)
{
	uint32_t mask;

	switch (pe) {
		/*
		 * Events valid only on counter 0.
		 */
	case PMC_EV_UCP_EVENT_80H_01H:
	case PMC_EV_UCP_EVENT_83H_01H:
		mask = (1 << 0);
		break;

	default:
		mask = ~0;	/* Any row index is ok. */
	}

	return (mask & (1 << ri));
}

static int
ucp_allocate_pmc(int cpu, int ri, struct pmc *pm,
    const struct pmc_op_pmcallocate *a)
{
	int n;
	enum pmc_event ev;
	struct ucp_event_descr *ie;
	uint32_t caps, config, cpuflag, evsel;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row-index value %d", __LINE__, ri));

	/* check requested capabilities */
	caps = a->pm_caps;
	if ((UCP_PMC_CAPS & caps) != caps)
		return (EPERM);

	ev = pm->pm_event;

	switch (uncore_cputype) {
	case PMC_CPU_INTEL_HASWELL:
	case PMC_CPU_INTEL_SANDYBRIDGE:
		if (ucp_event_sb_hw_ok_on_counter(ev, ri) == 0)
			return (EINVAL);
		break;
	default:
		break;
	}


	/*
	 * Look for an event descriptor with matching CPU and event id
	 * fields.
	 */

	switch (uncore_cputype) {
	case PMC_CPU_INTEL_COREI7:
		cpuflag = UCP_F_I7;
		break;
	case PMC_CPU_INTEL_HASWELL:
		cpuflag = UCP_F_HW;
		break;
	case PMC_CPU_INTEL_SANDYBRIDGE:
		cpuflag = UCP_F_SB;
		break;
	case PMC_CPU_INTEL_WESTMERE:
		cpuflag = UCP_F_WM;
		break;
	default:
		return (EINVAL);
	}

	for (n = 0, ie = ucp_events; n < nitems(ucp_events); n++, ie++)
		if (ie->ucp_ev == ev && ie->ucp_flags & cpuflag)
			break;

	if (n == nitems(ucp_events))
		return (EINVAL);

	/*
	 * A matching event descriptor has been found, so start
	 * assembling the contents of the event select register.
	 */
	evsel = ie->ucp_evcode | UCP_EN;

	config = a->pm_md.pm_ucp.pm_ucp_config & ~UCP_F_CMASK;

	/*
	 * If the event uses a fixed umask value, reject any umask
	 * bits set by the user.
	 */
	if (ie->ucp_flags & UCP_F_FM) {

		if (UCP_UMASK(config) != 0)
			return (EINVAL);

		evsel |= (ie->ucp_umask << 8);

	} else
		return (EINVAL);

	if (caps & PMC_CAP_THRESHOLD)
		evsel |= (a->pm_md.pm_ucp.pm_ucp_config & UCP_F_CMASK);
	if (caps & PMC_CAP_EDGE)
		evsel |= UCP_EDGE;
	if (caps & PMC_CAP_INVERT)
		evsel |= UCP_INV;

	pm->pm_md.pm_ucp.pm_ucp_evsel = evsel;

	return (0);
}

static int
ucp_config_pmc(int cpu, int ri, struct pmc *pm)
{
	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU %d", __LINE__, cpu));

	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	PMCDBG3(MDP,CFG,1, "ucp-config cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(uncore_pcpu[cpu] != NULL, ("[uncore,%d] null per-cpu %d", __LINE__,
	    cpu));

	uncore_pcpu[cpu]->pc_uncorepmcs[ri].phw_pmc = pm;

	return (0);
}

static int
ucp_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	int error;
	struct pmc_hw *phw;
	char ucp_name[PMC_NAME_MAX];

	phw = &uncore_pcpu[cpu]->pc_uncorepmcs[ri];

	(void) snprintf(ucp_name, sizeof(ucp_name), "UCP-%d", ri);
	if ((error = copystr(ucp_name, pi->pm_name, PMC_NAME_MAX,
	    NULL)) != 0)
		return (error);

	pi->pm_class = PMC_CLASS_UCP;

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
ucp_get_config(int cpu, int ri, struct pmc **ppm)
{
	*ppm = uncore_pcpu[cpu]->pc_uncorepmcs[ri].phw_pmc;

	return (0);
}

static int
ucp_read_pmc(int cpu, int ri, pmc_value_t *v)
{
	struct pmc *pm;
	pmc_value_t tmp;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	pm = uncore_pcpu[cpu]->pc_uncorepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] cpu %d ri %d pmc not configured", __LINE__, cpu,
		ri));

	tmp = rdmsr(UCP_PMC0 + ri);
	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		*v = ucp_perfctr_value_to_reload_count(tmp);
	else
		*v = tmp;

	PMCDBG4(MDP,REA,1, "ucp-read cpu=%d ri=%d msr=0x%x -> v=%jx", cpu, ri,
	    ri, *v);

	return (0);
}

static int
ucp_release_pmc(int cpu, int ri, struct pmc *pm)
{
	(void) pm;

	PMCDBG3(MDP,REL,1, "ucp-release cpu=%d ri=%d pm=%p", cpu, ri,
	    pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	KASSERT(uncore_pcpu[cpu]->pc_uncorepmcs[ri].phw_pmc
	    == NULL, ("[uncore,%d] PHW pmc non-NULL", __LINE__));

	return (0);
}

static int
ucp_start_pmc(int cpu, int ri)
{
	struct pmc *pm;
	uint32_t evsel;
	struct uncore_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row-index %d", __LINE__, ri));

	cc = uncore_pcpu[cpu];
	pm = cc->pc_uncorepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] starting cpu%d,ri%d with no pmc configured",
		__LINE__, cpu, ri));

	PMCDBG2(MDP,STA,1, "ucp-start cpu=%d ri=%d", cpu, ri);

	evsel = pm->pm_md.pm_ucp.pm_ucp_evsel;

	PMCDBG4(MDP,STA,2,
	    "ucp-start/2 cpu=%d ri=%d evselmsr=0x%x evsel=0x%x",
	    cpu, ri, SELECTSEL(uncore_cputype) + ri, evsel);

	/* Event specific configuration. */
	switch (pm->pm_event) {
	case PMC_EV_UCP_EVENT_0CH_04H_E:
	case PMC_EV_UCP_EVENT_0CH_08H_E:
		wrmsr(MSR_GQ_SNOOP_MESF,0x2);
		break;
	case PMC_EV_UCP_EVENT_0CH_04H_F:
	case PMC_EV_UCP_EVENT_0CH_08H_F:
		wrmsr(MSR_GQ_SNOOP_MESF,0x8);
		break;
	case PMC_EV_UCP_EVENT_0CH_04H_M:
	case PMC_EV_UCP_EVENT_0CH_08H_M:
		wrmsr(MSR_GQ_SNOOP_MESF,0x1);
		break;
	case PMC_EV_UCP_EVENT_0CH_04H_S:
	case PMC_EV_UCP_EVENT_0CH_08H_S:
		wrmsr(MSR_GQ_SNOOP_MESF,0x4);
		break;
	default:
		break;
	}

	wrmsr(SELECTSEL(uncore_cputype) + ri, evsel);

	do {
		cc->pc_resync = 0;
		cc->pc_globalctrl |= (1ULL << ri);
		wrmsr(UC_GLOBAL_CTRL, cc->pc_globalctrl);
	} while (cc->pc_resync != 0);

	return (0);
}

static int
ucp_stop_pmc(int cpu, int ri)
{
	struct pmc *pm;
	struct uncore_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row index %d", __LINE__, ri));

	cc = uncore_pcpu[cpu];
	pm = cc->pc_uncorepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	PMCDBG2(MDP,STO,1, "ucp-stop cpu=%d ri=%d", cpu, ri);

	/* stop hw. */
	wrmsr(SELECTSEL(uncore_cputype) + ri, 0);

	do {
		cc->pc_resync = 0;
		cc->pc_globalctrl &= ~(1ULL << ri);
		wrmsr(UC_GLOBAL_CTRL, cc->pc_globalctrl);
	} while (cc->pc_resync != 0);

	return (0);
}

static int
ucp_write_pmc(int cpu, int ri, pmc_value_t v)
{
	struct pmc *pm;
	struct uncore_cpu *cc;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[uncore,%d] illegal cpu value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < uncore_ucp_npmc,
	    ("[uncore,%d] illegal row index %d", __LINE__, ri));

	cc = uncore_pcpu[cpu];
	pm = cc->pc_uncorepmcs[ri].phw_pmc;

	KASSERT(pm,
	    ("[uncore,%d] cpu%d ri%d no configured PMC to stop", __LINE__,
		cpu, ri));

	PMCDBG4(MDP,WRI,1, "ucp-write cpu=%d ri=%d msr=0x%x v=%jx", cpu, ri,
	    UCP_PMC0 + ri, v);

	if (PMC_IS_SAMPLING_MODE(PMC_TO_MODE(pm)))
		v = ucp_reload_count_to_perfctr_value(v);

	/*
	 * Write the new value to the counter.  The counter will be in
	 * a stopped state when the pcd_write() entry point is called.
	 */

	wrmsr(UCP_PMC0 + ri, v);

	return (0);
}


static void
ucp_initialize(struct pmc_mdep *md, int maxcpu, int npmc, int pmcwidth)
{
	struct pmc_classdep *pcd;

	KASSERT(md != NULL, ("[ucp,%d] md is NULL", __LINE__));

	PMCDBG0(MDP,INI,1, "ucp-initialize");

	pcd = &md->pmd_classdep[PMC_MDEP_CLASS_INDEX_UCP];

	pcd->pcd_caps	= UCP_PMC_CAPS;
	pcd->pcd_class	= PMC_CLASS_UCP;
	pcd->pcd_num	= npmc;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= pmcwidth;

	pcd->pcd_allocate_pmc	= ucp_allocate_pmc;
	pcd->pcd_config_pmc	= ucp_config_pmc;
	pcd->pcd_describe	= ucp_describe;
	pcd->pcd_get_config	= ucp_get_config;
	pcd->pcd_get_msr	= NULL;
	pcd->pcd_pcpu_fini	= uncore_pcpu_fini;
	pcd->pcd_pcpu_init	= uncore_pcpu_init;
	pcd->pcd_read_pmc	= ucp_read_pmc;
	pcd->pcd_release_pmc	= ucp_release_pmc;
	pcd->pcd_start_pmc	= ucp_start_pmc;
	pcd->pcd_stop_pmc	= ucp_stop_pmc;
	pcd->pcd_write_pmc	= ucp_write_pmc;

	md->pmd_npmc	       += npmc;
}

int
pmc_uncore_initialize(struct pmc_mdep *md, int maxcpu)
{
	uncore_cputype = md->pmd_cputype;
	uncore_pmcmask = 0;

	/*
	 * Initialize programmable counters.
	 */

	uncore_ucp_npmc  = 8;
	uncore_ucp_width = 48;

	uncore_pmcmask |= ((1ULL << uncore_ucp_npmc) - 1);

	ucp_initialize(md, maxcpu, uncore_ucp_npmc, uncore_ucp_width);

	/*
	 * Initialize fixed function counters, if present.
	 */
	uncore_ucf_ri = uncore_ucp_npmc;
	uncore_ucf_npmc  = 1;
	uncore_ucf_width = 48;

	ucf_initialize(md, maxcpu, uncore_ucf_npmc, uncore_ucf_width);
	uncore_pmcmask |= ((1ULL << uncore_ucf_npmc) - 1) << SELECTOFF(uncore_cputype);

	PMCDBG2(MDP,INI,1,"uncore-init pmcmask=0x%jx ucfri=%d", uncore_pmcmask,
	    uncore_ucf_ri);

	uncore_pcpu = malloc(sizeof(*uncore_pcpu) * maxcpu, M_PMC,
	    M_ZERO | M_WAITOK);

	return (0);
}

void
pmc_uncore_finalize(struct pmc_mdep *md)
{
	PMCDBG0(MDP,INI,1, "uncore-finalize");

	free(uncore_pcpu, M_PMC);
	uncore_pcpu = NULL;
}
