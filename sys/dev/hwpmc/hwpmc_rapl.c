/*
 * Copyright (c) 2026 Advanced Micro Devices, Inc.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

/*
 * AMD/Intel RAPL energy counters exposed as an hwpmc(4) PMC class.
 *
 * Read-only, system-scope (PMC_MODE_SC), 64-bit counters reporting
 * microjoules.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/pmc.h>
#include <sys/pmckern.h>
#include <sys/priv.h>
#include <sys/proc.h>
#include <sys/smp.h>
#include <sys/systm.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>

#include <x86/x86_var.h>

#include <dev/hwpmc/hwpmc_rapl.h>

/* Energy counters are per-package/core domains, not per CPU: DOMWIDE. */
#define	RAPL_CAPS	(PMC_CAP_READ | PMC_CAP_DOMWIDE)

/* Worst-case package watts for sizing the guard timer. */
#define	RAPL_GUARD_WATT		1000

/* Guard interval clamp band (ms). */
#define	RAPL_GUARD_MIN_MS	10
#define	RAPL_GUARD_MAX_MS	60000

struct rapl_event {
	enum pmc_event	re_ev;
	uint32_t	re_msr;
	uint32_t	re_unit;	/* unit shift: 1 tick = 1/2^unit J */
};

struct rapl_value {
	uint64_t	rv_prev;	/* last raw 32-bit MSR value	*/
	uint64_t	rv_accum;
	sbintime_t	rv_prev_time;
	bool		rv_primed;
};

struct rapl_cpu {
	struct pmc_hw	rc_hw[RAPL_MAX_NPMCS];
	struct rapl_value rc_value[RAPL_MAX_NPMCS];
	struct mtx	rc_mtx;
	int		rc_nalloc;	/* allocated RAPL PMCs on this CPU */
};

static struct rapl_cpu **rapl_pcpu;

static struct rapl_event rapl_events[RAPL_MAX_NPMCS];
static struct pmc_descr rapl_pmcdesc[RAPL_MAX_NPMCS];
static int rapl_npmcs;
static int rapl_ri;

static struct callout	rapl_guard_callout;
static sbintime_t	rapl_guard_sbt;
static int		rapl_nalloc;
static cpuset_t		rapl_cpus;	/* CPUs with an allocated RAPL PMC */

static struct mtx	rapl_alloc_mtx;

/* Convert energy ticks to microjoules without overflowing uint64_t. */
static uint64_t
rapl_raw_to_uj(uint64_t raw, uint32_t shift)
{
	uint64_t unit, whole, frac;

	unit = 1ULL << shift;
	whole = raw / unit;
	frac = raw % unit;
	return (whole * 1000000ULL + (frac * 1000000ULL) / unit);
}

/* Fold a 32-bit MSR reading into the 64-bit accumulator, can recover one wrap. */
static void
rapl_update_delta(struct rapl_value *val, uint64_t cur)
{
	sbintime_t now = sbinuptime();
	uint64_t diff;

	cur &= UINT32_MAX;
	if (!val->rv_primed) {
		val->rv_prev = cur;
		val->rv_prev_time = now;
		val->rv_primed = true;
		return;
	}
	/* Skip sub-ms re-samples; the next sample folds the full interval. */
	if (now - val->rv_prev_time < SBT_1MS)
		return;
	if (cur >= val->rv_prev)
		diff = cur - val->rv_prev;
	else
		diff = (UINT32_MAX - val->rv_prev) + cur + 1;
	val->rv_accum += diff;
	val->rv_prev = cur;
	val->rv_prev_time = now;
}

/* Sample one row's MSR on the current CPU and return the folded accumulator. */
static uint64_t
rapl_sample_row(int cpu, int ri)
{
	struct rapl_cpu *rc;
	uint64_t accum, cur;

	rc = rapl_pcpu[cpu];
	KASSERT(rc != NULL, ("[rapl,%d] null pcpu state cpu %d", __LINE__,
	    cpu));
	mtx_lock_spin(&rc->rc_mtx);
	if (rdmsr_safe(rapl_events[ri].re_msr, &cur) == 0)
		rapl_update_delta(&rc->rc_value[ri], cur);
	accum = rc->rc_value[ri].rv_accum;
	mtx_unlock_spin(&rc->rc_mtx);
	return (accum);
}

/* Guard rendezvous handler: sample every row on this CPU. */
static void
rapl_guard_handler(void *arg __unused)
{
	int cpu = curcpu;
	int ri;

	for (ri = 0; ri < rapl_npmcs; ri++)
		(void)rapl_sample_row(cpu, ri);
}

static void	rapl_guard_tick(void *arg);

/* (Re)arm the guard callout. Caller holds rapl_alloc_mtx. */
static void
rapl_guard_schedule(void)
{
	mtx_assert(&rapl_alloc_mtx, MA_OWNED);
	callout_reset_sbt(&rapl_guard_callout, rapl_guard_sbt,
	    rapl_guard_sbt / 10, rapl_guard_tick, NULL, 0);
}

/* Periodic overflow guard. */
static void
rapl_guard_tick(void *arg __unused)
{
	cpuset_t cpus;

	mtx_lock(&rapl_alloc_mtx);
	cpus = rapl_cpus;
	mtx_unlock(&rapl_alloc_mtx);

	if (!CPU_EMPTY(&cpus))
		smp_rendezvous_cpus(cpus, smp_no_rendezvous_barrier,
		    rapl_guard_handler, smp_no_rendezvous_barrier, NULL);

	/* Keep firing while any RAPL PMC remains allocated. */
	mtx_lock(&rapl_alloc_mtx);
	if (rapl_nalloc > 0)
		rapl_guard_schedule();
	mtx_unlock(&rapl_alloc_mtx);
}

static int
rapl_allocate_pmc(int cpu, int ri, struct pmc *pm __unused,
    const struct pmc_op_pmcallocate *a)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row index %d", __LINE__, ri));

	if (a->pm_class != PMC_CLASS_RAPL)
		return (EINVAL);

	if (a->pm_mode != PMC_MODE_SC)
		return (EINVAL);

	/* Power side channel (PLATYPUS): require privilege even if syspmcs bypass is set. */
	if (priv_check(curthread, PRIV_PMC_SYSTEM) != 0)
		return (EPERM);

	/* Reject events this vendor does not expose (e.g. DRAM on AMD). */
	if (a->pm_ev != rapl_events[ri].re_ev)
		return (EINVAL);

	/* Arm the guard on the first allocation (per-CPU and global). */
	mtx_lock(&rapl_alloc_mtx);
	if (rapl_pcpu[cpu]->rc_nalloc++ == 0)
		CPU_SET(cpu, &rapl_cpus);
	if (rapl_nalloc++ == 0)
		rapl_guard_schedule();
	mtx_unlock(&rapl_alloc_mtx);

	return (0);
}

static int
rapl_config_pmc(int cpu, int ri, struct pmc *pm)
{
	struct pmc_hw *phw;

	PMCDBG3(MDP,CFG,1, "cpu=%d ri=%d pm=%p", cpu, ri, pm);

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	phw = &rapl_pcpu[cpu]->rc_hw[ri];

	KASSERT(pm == NULL || phw->phw_pmc == NULL,
	    ("[rapl,%d] pm=%p phw->pm=%p hwpmc not unconfigured", __LINE__,
	    pm, phw->phw_pmc));

	phw->phw_pmc = pm;

	return (0);
}

static int
rapl_describe(int cpu, int ri, struct pmc_info *pi, struct pmc **ppmc)
{
	const struct pmc_descr *pd;
	struct pmc_hw *phw;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	phw = &rapl_pcpu[cpu]->rc_hw[ri];
	pd  = &rapl_pmcdesc[ri];

	strlcpy(pi->pm_name, pd->pd_name, sizeof(pi->pm_name));
	pi->pm_class = pd->pd_class;

	if (phw->phw_state & PMC_PHW_FLAG_IS_ENABLED) {
		pi->pm_enabled = true;
		*ppmc          = phw->phw_pmc;
	} else {
		pi->pm_enabled = false;
		*ppmc          = NULL;
	}

	return (0);
}

static int
rapl_get_config(int cpu, int ri, struct pmc **ppm)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	*ppm = rapl_pcpu[cpu]->rc_hw[ri].phw_pmc;

	return (0);
}

static int
rapl_pcpu_init(struct pmc_mdep *md __unused, int cpu)
{
	struct pmc_cpu *pc;
	struct rapl_cpu *rapl_pc;
	int ri, n;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal cpu %d", __LINE__, cpu));
	KASSERT(rapl_pcpu, ("[rapl,%d] null pcpu", __LINE__));
	KASSERT(rapl_pcpu[cpu] == NULL, ("[rapl,%d] non-null per-cpu",
	    __LINE__));

	rapl_pc = malloc(sizeof(struct rapl_cpu), M_PMC, M_WAITOK | M_ZERO);
	mtx_init(&rapl_pc->rc_mtx, "rapl-cpu", NULL, MTX_SPIN);

	for (n = 0; n < rapl_npmcs; n++)
		rapl_pc->rc_hw[n].phw_state = PMC_PHW_FLAG_IS_ENABLED |
		    PMC_PHW_CPU_TO_STATE(cpu) | PMC_PHW_INDEX_TO_STATE(n) |
		    PMC_PHW_FLAG_IS_SHAREABLE;

	rapl_pcpu[cpu] = rapl_pc;

	KASSERT(pmc_pcpu, ("[rapl,%d] null generic pcpu", __LINE__));

	pc = pmc_pcpu[cpu];

	KASSERT(pc, ("[rapl,%d] null generic per-cpu", __LINE__));

	for (n = 0; n < rapl_npmcs; n++) {
		ri = rapl_ri + n;
		pc->pc_hwpmcs[ri] = &rapl_pc->rc_hw[n];
	}

	return (0);
}

static int
rapl_pcpu_fini(struct pmc_mdep *md __unused, int cpu)
{
	struct pmc_cpu *pc;
	int ri, n;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal cpu %d", __LINE__, cpu));
	KASSERT(rapl_pcpu[cpu] != NULL, ("[rapl,%d] null pcpu", __LINE__));
	KASSERT(rapl_pcpu[cpu]->rc_nalloc == 0,
	    ("[rapl,%d] %d PMCs still allocated on cpu %d", __LINE__,
	    rapl_pcpu[cpu]->rc_nalloc, cpu));

	/* Last release already drained the guard, so no handler can race here. */
	mtx_destroy(&rapl_pcpu[cpu]->rc_mtx);
	free(rapl_pcpu[cpu], M_PMC);
	rapl_pcpu[cpu] = NULL;

	pc = pmc_pcpu[cpu];
	for (n = 0; n < rapl_npmcs; n++) {
		ri = rapl_ri + n;
		pc->pc_hwpmcs[ri] = NULL;
	}

	return (0);
}

static int
rapl_read_pmc(int cpu, int ri, struct pmc *pm, pmc_value_t *v)
{
	enum pmc_mode mode __diagused;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal ri %d", __LINE__, ri));

	mode = PMC_TO_MODE(pm);

	KASSERT(mode == PMC_MODE_SC,
	    ("[rapl,%d] illegal pmc mode %d", __LINE__, mode));

	PMCDBG1(MDP,REA,1, "rapl-read id=%d", ri);

	/* Bound to cpu by hwpmc, so rdmsr reads this CPU's domain (see DOMWIDE). */
	*v = rapl_raw_to_uj(rapl_sample_row(cpu, ri), rapl_events[ri].re_unit);

	return (0);
}

static int
rapl_release_pmc(int cpu, int ri, struct pmc *pmc __unused)
{
	struct pmc_hw *phw __diagused;
	bool last;

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	phw = &rapl_pcpu[cpu]->rc_hw[ri];

	KASSERT(phw->phw_pmc == NULL,
	    ("[rapl,%d] PHW pmc %p non-NULL", __LINE__, phw->phw_pmc));

	mtx_lock(&rapl_alloc_mtx);
	KASSERT(rapl_pcpu[cpu]->rc_nalloc > 0 && rapl_nalloc > 0,
	    ("[rapl,%d] release underflow", __LINE__));
	if (--rapl_pcpu[cpu]->rc_nalloc == 0)
		CPU_CLR(cpu, &rapl_cpus);
	last = (--rapl_nalloc == 0);
	mtx_unlock(&rapl_alloc_mtx);

	/* Last release: drain the guard (sleepable here, mutex already dropped). */
	if (last)
		callout_drain(&rapl_guard_callout);

	return (0);
}

static int
rapl_start_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	return (0);	/* RAPL counters are always running. */
}

static int
rapl_stop_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	return (0);	/* RAPL counters cannot be stopped. */
}

static int
rapl_write_pmc(int cpu __diagused, int ri __diagused, struct pmc *pm __unused,
    pmc_value_t v __unused)
{

	KASSERT(cpu >= 0 && cpu < pmc_cpu_max(),
	    ("[rapl,%d] illegal CPU value %d", __LINE__, cpu));
	KASSERT(ri >= 0 && ri < rapl_npmcs,
	    ("[rapl,%d] illegal row-index %d", __LINE__, ri));

	/* Energy counters are not writable; refuse silently like TSC. */
	return (0);
}

/* Fault-safe RAPL MSR presence probe on the current CPU. */
static bool
rapl_msr_present(uint32_t msr)
{
	uint64_t v;

	return (rdmsr_safe(msr, &v) == 0);
}

/* Append an event row to the table if its MSR responds on this hardware. */
static void
rapl_add_event(enum pmc_event ev, uint32_t msr, uint32_t unit,
    const char *name)
{

	if (!rapl_msr_present(msr))
		return;

	rapl_events[rapl_npmcs].re_ev = ev;
	rapl_events[rapl_npmcs].re_msr = msr;
	rapl_events[rapl_npmcs].re_unit = unit;
	rapl_pmcdesc[rapl_npmcs].pd_class = PMC_CLASS_RAPL;
	rapl_pmcdesc[rapl_npmcs].pd_caps = RAPL_CAPS;
	rapl_pmcdesc[rapl_npmcs].pd_width = 64;
	strlcpy(rapl_pmcdesc[rapl_npmcs].pd_name, name,
	    sizeof(rapl_pmcdesc[rapl_npmcs].pd_name));
	rapl_npmcs++;
}

/* Guard interval: half the worst-case wrap period at RAPL_GUARD_WATT. */
static sbintime_t
rapl_compute_guard_sbt(uint32_t shift)
{
	uint64_t max_energy_uj, guard_ms;

	max_energy_uj = rapl_raw_to_uj(UINT32_MAX, shift);
	guard_ms = max_energy_uj / (2000ULL * RAPL_GUARD_WATT);
	if (guard_ms < RAPL_GUARD_MIN_MS)
		guard_ms = RAPL_GUARD_MIN_MS;
	else if (guard_ms > RAPL_GUARD_MAX_MS)
		guard_ms = RAPL_GUARD_MAX_MS;

	return (guard_ms * SBT_1MS);
}

/* Fixed 2^-16 J DRAM unit, not the ESU (HSX/KNL only -- not SPR/EMR/GNR). */
static bool
rapl_intel_fixed_dram_unit(void)
{

	if (CPUID_TO_FAMILY(cpu_id) != 0x6)
		return (false);

	switch (CPUID_TO_MODEL(cpu_id)) {
	case 0x3f:	/* Haswell-EP */
	case 0x4f:	/* Broadwell-EP */
	case 0x55:	/* Skylake/Cascade Lake/Cooper Lake-SP */
	case 0x56:	/* Broadwell-DE */
	case 0x57:	/* Xeon Phi KNL */
	case 0x6a:	/* Ice Lake-SP */
	case 0x6c:	/* Ice Lake-D */
	case 0x85:	/* Xeon Phi KNM */
		return (true);
	default:
		return (false);
	}
}

int
pmc_rapl_initialize(struct pmc_mdep *md, int maxcpu, int classindex)
{
	struct pmc_classdep *pcd;
	uint32_t unit_msr, pkg_msr, cores_msr, dram_msr;
	uint32_t esu, dram_unit, max_unit;
	uint64_t unit_val;
	int i;

	KASSERT(md != NULL, ("[rapl,%d] md is NULL", __LINE__));
	KASSERT(md->pmd_nclass >= 1, ("[rapl,%d] dubious md->nclass %d",
	    __LINE__, md->pmd_nclass));

	/* Select the per-vendor MSR set. */
	switch (cpu_vendor_id) {
	case CPU_VENDOR_AMD:
	case CPU_VENDOR_HYGON:
		unit_msr = MSR_AMD_RAPL_POWER_UNIT;
		pkg_msr = MSR_AMD_PKG_ENERGY_STATUS;
		cores_msr = MSR_AMD_CORE_ENERGY_STATUS;
		dram_msr = 0;			/* AMD has no DRAM domain */
		break;
	case CPU_VENDOR_INTEL:
		unit_msr = MSR_RAPL_POWER_UNIT;
		pkg_msr = MSR_PKG_ENERGY_STATUS;
		cores_msr = MSR_PP0_ENERGY_STATUS;
		dram_msr = MSR_DRAM_ENERGY_STATUS;
		break;
	default:
		return (ENXIO);
	}

	/* Decode the energy unit. */
	unit_val = rdmsr(unit_msr);
	esu = (unit_val >> 8) & 0x1f;
	dram_unit = rapl_intel_fixed_dram_unit() ? 16 : esu;

	/* Build the event table from the MSRs that actually respond. */
	rapl_npmcs = 0;
	rapl_add_event(PMC_EV_RAPL_ENERGY_PKG, pkg_msr, esu,
	    "RAPL_ENERGY_PKG");
	rapl_add_event(PMC_EV_RAPL_ENERGY_CORES, cores_msr, esu,
	    "RAPL_ENERGY_CORES");
	if (dram_msr != 0)
		rapl_add_event(PMC_EV_RAPL_ENERGY_DRAM, dram_msr, dram_unit,
		    "RAPL_ENERGY_DRAM");

	/* No RAPL energy MSR responded. */
	if (rapl_npmcs == 0)
		return (ENXIO);

	/* Size the guard for the fastest-wrapping row (largest unit shift). */
	max_unit = 0;
	for (i = 0; i < rapl_npmcs; i++)
		max_unit = MAX(max_unit, rapl_events[i].re_unit);
	rapl_guard_sbt = rapl_compute_guard_sbt(max_unit);
	rapl_nalloc = 0;
	CPU_ZERO(&rapl_cpus);

	mtx_init(&rapl_alloc_mtx, "rapl-alloc", NULL, MTX_DEF);
	/* It does not need associated mutex, the handler locks itself. */
	callout_init(&rapl_guard_callout, 1);

	rapl_pcpu = malloc(sizeof(struct rapl_cpu *) * maxcpu, M_PMC,
	    M_ZERO | M_WAITOK);

	pcd = &md->pmd_classdep[classindex];

	pcd->pcd_caps	= RAPL_CAPS;
	pcd->pcd_class	= PMC_CLASS_RAPL;
	pcd->pcd_num	= rapl_npmcs;
	pcd->pcd_ri	= md->pmd_npmc;
	pcd->pcd_width	= 64;

	pcd->pcd_allocate_pmc = rapl_allocate_pmc;
	pcd->pcd_config_pmc   = rapl_config_pmc;
	pcd->pcd_describe     = rapl_describe;
	pcd->pcd_get_config   = rapl_get_config;
	pcd->pcd_pcpu_init    = rapl_pcpu_init;
	pcd->pcd_pcpu_fini    = rapl_pcpu_fini;
	pcd->pcd_read_pmc     = rapl_read_pmc;
	pcd->pcd_release_pmc  = rapl_release_pmc;
	pcd->pcd_start_pmc    = rapl_start_pmc;
	pcd->pcd_stop_pmc     = rapl_stop_pmc;
	pcd->pcd_write_pmc    = rapl_write_pmc;

	rapl_ri = md->pmd_npmc;
	md->pmd_npmc += rapl_npmcs;

	return (0);
}

void
pmc_rapl_finalize(struct pmc_mdep *md __unused)
{
	PMCDBG0(MDP, INI, 1, "rapl-finalize");

	if (rapl_pcpu == NULL)
		return;

	KASSERT(rapl_nalloc == 0, ("[rapl,%d] %d PMCs still allocated",
	    __LINE__, rapl_nalloc));
	for (int i = 0; i < pmc_cpu_max(); i++)
		KASSERT(rapl_pcpu[i] == NULL, ("[rapl,%d] non-null pcpu cpu %d",
		    __LINE__, i));

	mtx_destroy(&rapl_alloc_mtx);

	free(rapl_pcpu, M_PMC);
	rapl_pcpu = NULL;
}
