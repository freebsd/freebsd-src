/*-
 * Copyright (c) 1998-2003 Poul-Henning Kamp
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

#include "opt_clock.h"

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/malloc.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/time.h>
#include <sys/timetc.h>
#include <sys/kernel.h>
#include <sys/power.h>
#include <sys/smp.h>
#include <machine/clock.h>
#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include "cpufreq_if.h"

uint64_t	tsc_freq;
int		tsc_is_invariant;
int		tsc_perf_stat;

static eventhandler_tag tsc_levels_tag, tsc_pre_tag, tsc_post_tag;

SYSCTL_INT(_kern_timecounter, OID_AUTO, invariant_tsc, CTLFLAG_RDTUN,
    &tsc_is_invariant, 0, "Indicates whether the TSC is P-state invariant");
TUNABLE_INT("kern.timecounter.invariant_tsc", &tsc_is_invariant);

#ifdef SMP
static int	smp_tsc;
SYSCTL_INT(_kern_timecounter, OID_AUTO, smp_tsc, CTLFLAG_RDTUN, &smp_tsc, 0,
    "Indicates whether the TSC is safe to use in SMP mode");
TUNABLE_INT("kern.timecounter.smp_tsc", &smp_tsc);
#endif

static int	tsc_disabled;
SYSCTL_INT(_machdep, OID_AUTO, disable_tsc, CTLFLAG_RDTUN, &tsc_disabled, 0,
    "Disable x86 Time Stamp Counter");
TUNABLE_INT("machdep.disable_tsc", &tsc_disabled);

static int	tsc_skip_calibration;
SYSCTL_INT(_machdep, OID_AUTO, disable_tsc_calibration, CTLFLAG_RDTUN,
    &tsc_skip_calibration, 0, "Disable TSC frequency calibration");
TUNABLE_INT("machdep.disable_tsc_calibration", &tsc_skip_calibration);

static void tsc_freq_changed(void *arg, const struct cf_level *level,
    int status);
static void tsc_freq_changing(void *arg, const struct cf_level *level,
    int *status);
static	unsigned tsc_get_timecount(struct timecounter *tc);
static void tsc_levels_changed(void *arg, int unit);

static struct timecounter tsc_timecounter = {
	tsc_get_timecount,	/* get_timecount */
	0,			/* no poll_pps */
	~0u,			/* counter_mask */
	0,			/* frequency */
	"TSC",			/* name */
	800,			/* quality (adjusted in code) */
};

static void
tsc_freq_intel(void)
{
	char brand[48];
	u_int regs[4];
	uint64_t freq;
	char *p;
	u_int i;

	/*
	 * Intel Processor Identification and the CPUID Instruction
	 * Application Note 485.
	 *
	 * http://www.intel.com/assets/pdf/appnote/241618.pdf
	 */
	if (cpu_exthigh >= 0x80000004) {
		p = brand;
		for (i = 0x80000002; i < 0x80000005; i++) {
			do_cpuid(i, regs);
			memcpy(p, regs, sizeof(regs));
			p += sizeof(regs);
		}
		p = NULL;
		for (i = 0; i < sizeof(brand) - 1; i++)
			if (brand[i] == 'H' && brand[i + 1] == 'z')
				p = brand + i;
		if (p != NULL) {
			p -= 5;
			switch (p[4]) {
			case 'M':
				i = 1;
				break;
			case 'G':
				i = 1000;
				break;
			case 'T':
				i = 1000000;
				break;
			default:
				return;
			}
#define	C2D(c)	((c) - '0')
			if (p[1] == '.') {
				freq = C2D(p[0]) * 1000;
				freq += C2D(p[2]) * 100;
				freq += C2D(p[3]) * 10;
				freq *= i * 1000;
			} else {
				freq = C2D(p[0]) * 1000;
				freq += C2D(p[1]) * 100;
				freq += C2D(p[2]) * 10;
				freq += C2D(p[3]);
				freq *= i * 1000000;
			}
#undef C2D
			tsc_freq = freq;
		}
	}
}

static void
probe_tsc_freq(void)
{
	u_int regs[4];
	uint64_t tsc1, tsc2;

	switch (cpu_vendor_id) {
	case CPU_VENDOR_AMD:
		if ((amd_pminfo & AMDPM_TSC_INVARIANT) != 0 ||
		    (vm_guest == VM_GUEST_NO &&
		    CPUID_TO_FAMILY(cpu_id) >= 0x10))
			tsc_is_invariant = 1;
		break;
	case CPU_VENDOR_INTEL:
		if ((amd_pminfo & AMDPM_TSC_INVARIANT) != 0 ||
		    (vm_guest == VM_GUEST_NO &&
		    ((CPUID_TO_FAMILY(cpu_id) == 0x6 &&
		    CPUID_TO_MODEL(cpu_id) >= 0xe) ||
		    (CPUID_TO_FAMILY(cpu_id) == 0xf &&
		    CPUID_TO_MODEL(cpu_id) >= 0x3))))
			tsc_is_invariant = 1;
		break;
	case CPU_VENDOR_CENTAUR:
		if (vm_guest == VM_GUEST_NO &&
		    CPUID_TO_FAMILY(cpu_id) == 0x6 &&
		    CPUID_TO_MODEL(cpu_id) >= 0xf &&
		    (rdmsr(0x1203) & 0x100000000ULL) == 0)
			tsc_is_invariant = 1;
		break;
	}

	if (cpu_high >= 6) {
		do_cpuid(6, regs);
		if ((regs[2] & CPUID_PERF_STAT) != 0) {
			/*
			 * XXX Some emulators expose host CPUID without actual
			 * support for these MSRs.  We must test whether they
			 * really work.
			 */
			wrmsr(MSR_MPERF, 0);
			wrmsr(MSR_APERF, 0);
			DELAY(10);
			if (rdmsr(MSR_MPERF) > 0 && rdmsr(MSR_APERF) > 0)
				tsc_perf_stat = 1;
		}
	}

	if (tsc_skip_calibration) {
		if (cpu_vendor_id == CPU_VENDOR_INTEL)
			tsc_freq_intel();
		return;
	}

	if (bootverbose)
	        printf("Calibrating TSC clock ... ");
	tsc1 = rdtsc();
	DELAY(1000000);
	tsc2 = rdtsc();
	tsc_freq = tsc2 - tsc1;
	if (bootverbose)
		printf("TSC clock: %ju Hz\n", (intmax_t)tsc_freq);
}

void
init_TSC(void)
{

	if ((cpu_feature & CPUID_TSC) == 0 || tsc_disabled)
		return;

	probe_tsc_freq();

	/*
	 * Inform CPU accounting about our boot-time clock rate.  This will
	 * be updated if someone loads a cpufreq driver after boot that
	 * discovers a new max frequency.
	 */
	if (tsc_freq != 0)
		set_cputicker(rdtsc, tsc_freq, !tsc_is_invariant);

	if (tsc_is_invariant)
		return;

	/* Register to find out about changes in CPU frequency. */
	tsc_pre_tag = EVENTHANDLER_REGISTER(cpufreq_pre_change,
	    tsc_freq_changing, NULL, EVENTHANDLER_PRI_FIRST);
	tsc_post_tag = EVENTHANDLER_REGISTER(cpufreq_post_change,
	    tsc_freq_changed, NULL, EVENTHANDLER_PRI_FIRST);
	tsc_levels_tag = EVENTHANDLER_REGISTER(cpufreq_levels_changed,
	    tsc_levels_changed, NULL, EVENTHANDLER_PRI_ANY);
}

void
init_TSC_tc(void)
{

	if ((cpu_feature & CPUID_TSC) == 0 || tsc_disabled)
		return;

	/*
	 * We can not use the TSC if we support APM.  Precise timekeeping
	 * on an APM'ed machine is at best a fools pursuit, since 
	 * any and all of the time spent in various SMM code can't 
	 * be reliably accounted for.  Reading the RTC is your only
	 * source of reliable time info.  The i8254 loses too, of course,
	 * but we need to have some kind of time...
	 * We don't know at this point whether APM is going to be used
	 * or not, nor when it might be activated.  Play it safe.
	 */
	if (power_pm_get_type() == POWER_PM_TYPE_APM) {
		tsc_timecounter.tc_quality = -1000;
		if (bootverbose)
			printf("TSC timecounter disabled: APM enabled.\n");
	}

#ifdef SMP
	/*
	 * We can not use the TSC in SMP mode unless the TSCs on all CPUs
	 * are somehow synchronized.  Some hardware configurations do
	 * this, but we have no way of determining whether this is the
	 * case, so we do not use the TSC in multi-processor systems
	 * unless the user indicated (by setting kern.timecounter.smp_tsc
	 * to 1) that he believes that his TSCs are synchronized.
	 */
	if (mp_ncpus > 1 && !smp_tsc)
		tsc_timecounter.tc_quality = -100;
#endif

	if (tsc_freq != 0) {
		tsc_timecounter.tc_frequency = tsc_freq;
		tc_init(&tsc_timecounter);
	}
}

/*
 * When cpufreq levels change, find out about the (new) max frequency.  We
 * use this to update CPU accounting in case it got a lower estimate at boot.
 */
static void
tsc_levels_changed(void *arg, int unit)
{
	device_t cf_dev;
	struct cf_level *levels;
	int count, error;
	uint64_t max_freq;

	/* Only use values from the first CPU, assuming all are equal. */
	if (unit != 0)
		return;

	/* Find the appropriate cpufreq device instance. */
	cf_dev = devclass_get_device(devclass_find("cpufreq"), unit);
	if (cf_dev == NULL) {
		printf("tsc_levels_changed() called but no cpufreq device?\n");
		return;
	}

	/* Get settings from the device and find the max frequency. */
	count = 64;
	levels = malloc(count * sizeof(*levels), M_TEMP, M_NOWAIT);
	if (levels == NULL)
		return;
	error = CPUFREQ_LEVELS(cf_dev, levels, &count);
	if (error == 0 && count != 0) {
		max_freq = (uint64_t)levels[0].total_set.freq * 1000000;
		set_cputicker(rdtsc, max_freq, 1);
	} else
		printf("tsc_levels_changed: no max freq found\n");
	free(levels, M_TEMP);
}

/*
 * If the TSC timecounter is in use, veto the pending change.  It may be
 * possible in the future to handle a dynamically-changing timecounter rate.
 */
static void
tsc_freq_changing(void *arg, const struct cf_level *level, int *status)
{

	if (*status != 0 || timecounter != &tsc_timecounter)
		return;

	printf("timecounter TSC must not be in use when "
	    "changing frequencies; change denied\n");
	*status = EBUSY;
}

/* Update TSC freq with the value indicated by the caller. */
static void
tsc_freq_changed(void *arg, const struct cf_level *level, int status)
{
	uint64_t freq;

	/* If there was an error during the transition, don't do anything. */
	if (tsc_disabled || status != 0)
		return;

	/* Total setting for this level gives the new frequency in MHz. */
	freq = (uint64_t)level->total_set.freq * 1000000;
	atomic_store_rel_64(&tsc_freq, freq);
	atomic_store_rel_64(&tsc_timecounter.tc_frequency, freq);
}

static int
sysctl_machdep_tsc_freq(SYSCTL_HANDLER_ARGS)
{
	int error;
	uint64_t freq;

	freq = atomic_load_acq_64(&tsc_freq);
	if (freq == 0)
		return (EOPNOTSUPP);
	error = sysctl_handle_64(oidp, &freq, 0, req);
	if (error == 0 && req->newptr != NULL) {
		atomic_store_rel_64(&tsc_freq, freq);
		atomic_store_rel_64(&tsc_timecounter.tc_frequency, freq);
	}
	return (error);
}

SYSCTL_PROC(_machdep, OID_AUTO, tsc_freq, CTLTYPE_U64 | CTLFLAG_RW,
    0, 0, sysctl_machdep_tsc_freq, "QU", "Time Stamp Counter frequency");

static u_int
tsc_get_timecount(struct timecounter *tc)
{

	return (rdtsc32());
}
