/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2018 Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted providing that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sbuf.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/bus.h>
#include <sys/cpu.h>
#include <sys/smp.h>
#include <sys/proc.h>
#include <sys/sched.h>

#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/cputypes.h>
#include <machine/specialreg.h>

#include <contrib/dev/acpica/include/acpi.h>

#include <dev/acpica/acpivar.h>

#include <x86/cpufreq/hwpstate_intel_internal.h>

#include "acpi_if.h"
#include "cpufreq_if.h"

extern uint64_t	tsc_freq;

static int	intel_hwpstate_probe(device_t dev);
static int	intel_hwpstate_attach(device_t dev);
static int	intel_hwpstate_detach(device_t dev);
static int	intel_hwpstate_suspend(device_t dev);
static int	intel_hwpstate_resume(device_t dev);

static int      intel_hwpstate_get(device_t dev, struct cf_setting *cf);
static int      intel_hwpstate_type(device_t dev, int *type);

static device_method_t intel_hwpstate_methods[] = {
	/* Device interface */
	DEVMETHOD(device_identify,	intel_hwpstate_identify),
	DEVMETHOD(device_probe,		intel_hwpstate_probe),
	DEVMETHOD(device_attach,	intel_hwpstate_attach),
	DEVMETHOD(device_detach,	intel_hwpstate_detach),
	DEVMETHOD(device_suspend,	intel_hwpstate_suspend),
	DEVMETHOD(device_resume,	intel_hwpstate_resume),

	/* cpufreq interface */
	DEVMETHOD(cpufreq_drv_get,      intel_hwpstate_get),
	DEVMETHOD(cpufreq_drv_type,     intel_hwpstate_type),

	DEVMETHOD_END
};

struct hwp_softc {
	device_t		dev;
	bool 			hwp_notifications;
	bool			hwp_activity_window;
	bool			hwp_pref_ctrl;
	bool			hwp_pkg_ctrl;
	bool			hwp_pkg_ctrl_en;
	bool			hwp_perf_bias;
	bool			hwp_perf_bias_cached;

	uint64_t		req; /* Cached copy of HWP_REQUEST */
	uint64_t		hwp_energy_perf_bias;	/* Cache PERF_BIAS */

	uint8_t			high;
	uint8_t			guaranteed;
	uint8_t			efficient;
	uint8_t			low;
};

static devclass_t hwpstate_intel_devclass;
static driver_t hwpstate_intel_driver = {
	"hwpstate_intel",
	intel_hwpstate_methods,
	sizeof(struct hwp_softc),
};

DRIVER_MODULE(hwpstate_intel, cpu, hwpstate_intel_driver,
    hwpstate_intel_devclass, NULL, NULL);
MODULE_VERSION(hwpstate_intel, 1);

static bool hwpstate_pkg_ctrl_enable = true;
SYSCTL_BOOL(_machdep, OID_AUTO, hwpstate_pkg_ctrl, CTLFLAG_RDTUN,
    &hwpstate_pkg_ctrl_enable, 0,
    "Set 1 (default) to enable package-level control, 0 to disable");

static int
intel_hwp_dump_sysctl_handler(SYSCTL_HANDLER_ARGS)
{
	device_t dev;
	struct pcpu *pc;
	struct sbuf *sb;
	struct hwp_softc *sc;
	uint64_t data, data2;
	int ret;

	sc = (struct hwp_softc *)arg1;
	dev = sc->dev;

	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);

	sb = sbuf_new(NULL, NULL, 1024, SBUF_FIXEDLEN | SBUF_INCLUDENUL);
	sbuf_putc(sb, '\n');
	thread_lock(curthread);
	sched_bind(curthread, pc->pc_cpuid);
	thread_unlock(curthread);

	rdmsr_safe(MSR_IA32_PM_ENABLE, &data);
	sbuf_printf(sb, "CPU%d: HWP %sabled\n", pc->pc_cpuid,
	    ((data & 1) ? "En" : "Dis"));

	if (data == 0) {
		ret = 0;
		goto out;
	}

	rdmsr_safe(MSR_IA32_HWP_CAPABILITIES, &data);
	sbuf_printf(sb, "\tHighest Performance: %03ju\n", data & 0xff);
	sbuf_printf(sb, "\tGuaranteed Performance: %03ju\n", (data >> 8) & 0xff);
	sbuf_printf(sb, "\tEfficient Performance: %03ju\n", (data >> 16) & 0xff);
	sbuf_printf(sb, "\tLowest Performance: %03ju\n", (data >> 24) & 0xff);

	rdmsr_safe(MSR_IA32_HWP_REQUEST, &data);
	data2 = 0;
	if (sc->hwp_pkg_ctrl && (data & IA32_HWP_REQUEST_PACKAGE_CONTROL))
		rdmsr_safe(MSR_IA32_HWP_REQUEST_PKG, &data2);

	sbuf_putc(sb, '\n');

#define pkg_print(x, name, offset) do {					\
	if (!sc->hwp_pkg_ctrl || (data & x) != 0) 			\
		sbuf_printf(sb, "\t%s: %03u\n", name,			\
		    (unsigned)(data >> offset) & 0xff);			\
	else								\
		sbuf_printf(sb, "\t%s: %03u\n", name,			\
		    (unsigned)(data2 >> offset) & 0xff);		\
} while (0)

	pkg_print(IA32_HWP_REQUEST_EPP_VALID,
	    "Requested Efficiency Performance Preference", 24);
	pkg_print(IA32_HWP_REQUEST_DESIRED_VALID,
	    "Requested Desired Performance", 16);
	pkg_print(IA32_HWP_REQUEST_MAXIMUM_VALID,
	    "Requested Maximum Performance", 8);
	pkg_print(IA32_HWP_REQUEST_MINIMUM_VALID,
	    "Requested Minimum Performance", 0);
#undef pkg_print

	sbuf_putc(sb, '\n');

out:
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);

	ret = sbuf_finish(sb);
	if (ret == 0)
		ret = SYSCTL_OUT(req, sbuf_data(sb), sbuf_len(sb));
	sbuf_delete(sb);

	return (ret);
}

static inline int
percent_to_raw(int x)
{

	MPASS(x <= 100 && x >= 0);
	return (0xff * x / 100);
}

/*
 * Given x * 10 in [0, 1000], round to the integer nearest x.
 *
 * This allows round-tripping nice human readable numbers through this
 * interface.  Otherwise, user-provided percentages such as 25, 50, 75 get
 * rounded down to 24, 49, and 74, which is a bit ugly.
 */
static inline int
round10(int xtimes10)
{
	return ((xtimes10 + 5) / 10);
}

static inline int
raw_to_percent(int x)
{
	MPASS(x <= 0xff && x >= 0);
	return (round10(x * 1000 / 0xff));
}

/* Range of MSR_IA32_ENERGY_PERF_BIAS is more limited: 0-0xf. */
static inline int
percent_to_raw_perf_bias(int x)
{
	/*
	 * Round up so that raw values present as nice round human numbers and
	 * also round-trip to the same raw value.
	 */
	MPASS(x <= 100 && x >= 0);
	return (((0xf * x) + 50) / 100);
}

static inline int
raw_to_percent_perf_bias(int x)
{
	/* Rounding to nice human numbers despite a step interval of 6.67%. */
	MPASS(x <= 0xf && x >= 0);
	return (((x * 20) / 0xf) * 5);
}

static int
sysctl_epp_select(SYSCTL_HANDLER_ARGS)
{
	struct hwp_softc *sc;
	device_t dev;
	struct pcpu *pc;
	uint64_t epb;
	uint32_t val;
	int ret;

	dev = oidp->oid_arg1;
	sc = device_get_softc(dev);
	if (!sc->hwp_pref_ctrl && !sc->hwp_perf_bias)
		return (ENODEV);

	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);

	thread_lock(curthread);
	sched_bind(curthread, pc->pc_cpuid);
	thread_unlock(curthread);

	if (sc->hwp_pref_ctrl) {
		val = (sc->req & IA32_HWP_REQUEST_ENERGY_PERFORMANCE_PREFERENCE) >> 24;
		val = raw_to_percent(val);
	} else {
		/*
		 * If cpuid indicates EPP is not supported, the HWP controller
		 * uses MSR_IA32_ENERGY_PERF_BIAS instead (Intel SDM §14.4.4).
		 * This register is per-core (but not HT).
		 */
		if (!sc->hwp_perf_bias_cached) {
			ret = rdmsr_safe(MSR_IA32_ENERGY_PERF_BIAS, &epb);
			if (ret)
				goto out;
			sc->hwp_energy_perf_bias = epb;
			sc->hwp_perf_bias_cached = true;
		}
		val = sc->hwp_energy_perf_bias &
		    IA32_ENERGY_PERF_BIAS_POLICY_HINT_MASK;
		val = raw_to_percent_perf_bias(val);
	}

	MPASS(val >= 0 && val <= 100);

	ret = sysctl_handle_int(oidp, &val, 0, req);
	if (ret || req->newptr == NULL)
		goto out;

	if (val > 100) {
		ret = EINVAL;
		goto out;
	}

	if (sc->hwp_pref_ctrl) {
		val = percent_to_raw(val);

		sc->req =
		    ((sc->req & ~IA32_HWP_REQUEST_ENERGY_PERFORMANCE_PREFERENCE)
		    | (val << 24u));

		if (sc->hwp_pkg_ctrl_en)
			ret = wrmsr_safe(MSR_IA32_HWP_REQUEST_PKG, sc->req);
		else
			ret = wrmsr_safe(MSR_IA32_HWP_REQUEST, sc->req);
	} else {
		val = percent_to_raw_perf_bias(val);
		MPASS((val & ~IA32_ENERGY_PERF_BIAS_POLICY_HINT_MASK) == 0);

		sc->hwp_energy_perf_bias =
		    ((sc->hwp_energy_perf_bias &
		    ~IA32_ENERGY_PERF_BIAS_POLICY_HINT_MASK) | val);
		ret = wrmsr_safe(MSR_IA32_ENERGY_PERF_BIAS,
		    sc->hwp_energy_perf_bias);
	}

out:
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);

	return (ret);
}

void
intel_hwpstate_identify(driver_t *driver, device_t parent)
{
	if (device_find_child(parent, "hwpstate_intel", -1) != NULL)
		return;

	if (cpu_vendor_id != CPU_VENDOR_INTEL)
		return;

	if (resource_disabled("hwpstate_intel", 0))
		return;

	/*
	 * Intel SDM 14.4.1 (HWP Programming Interfaces):
	 *   Availability of HWP baseline resource and capability,
	 *   CPUID.06H:EAX[bit 7]: If this bit is set, HWP provides several new
	 *   architectural MSRs: IA32_PM_ENABLE, IA32_HWP_CAPABILITIES,
	 *   IA32_HWP_REQUEST, IA32_HWP_STATUS.
	 */
	if ((cpu_power_eax & CPUTPM1_HWP) == 0)
		return;

	if (BUS_ADD_CHILD(parent, 10, "hwpstate_intel", device_get_unit(parent))
	    == NULL)
		device_printf(parent, "hwpstate_intel: add child failed\n");
}

static int
intel_hwpstate_probe(device_t dev)
{

	device_set_desc(dev, "Intel Speed Shift");
	return (BUS_PROBE_NOWILDCARD);
}

static int
set_autonomous_hwp(struct hwp_softc *sc)
{
	struct pcpu *pc;
	device_t dev;
	uint64_t caps;
	int ret;

	dev = sc->dev;

	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);

	thread_lock(curthread);
	sched_bind(curthread, pc->pc_cpuid);
	thread_unlock(curthread);

	/* XXX: Many MSRs aren't readable until feature is enabled */
	ret = wrmsr_safe(MSR_IA32_PM_ENABLE, 1);
	if (ret) {
		/*
		 * This is actually a package-level MSR, and only the first
		 * write is not ignored.  So it is harmless to enable it across
		 * all devices, and this allows us not to care especially in
		 * which order cores (and packages) are probed.  This error
		 * condition should not happen given we gate on the HWP CPUID
		 * feature flag, if the Intel SDM is correct.
		 */
		device_printf(dev, "Failed to enable HWP for cpu%d (%d)\n",
		    pc->pc_cpuid, ret);
		goto out;
	}

	ret = rdmsr_safe(MSR_IA32_HWP_REQUEST, &sc->req);
	if (ret) {
		device_printf(dev,
		    "Failed to read HWP request MSR for cpu%d (%d)\n",
		    pc->pc_cpuid, ret);
		goto out;
	}

	ret = rdmsr_safe(MSR_IA32_HWP_CAPABILITIES, &caps);
	if (ret) {
		device_printf(dev,
		    "Failed to read HWP capabilities MSR for cpu%d (%d)\n",
		    pc->pc_cpuid, ret);
		goto out;
	}

	/*
	 * High and low are static; "guaranteed" is dynamic; and efficient is
	 * also dynamic.
	 */
	sc->high = IA32_HWP_CAPABILITIES_HIGHEST_PERFORMANCE(caps);
	sc->guaranteed = IA32_HWP_CAPABILITIES_GUARANTEED_PERFORMANCE(caps);
	sc->efficient = IA32_HWP_CAPABILITIES_EFFICIENT_PERFORMANCE(caps);
	sc->low = IA32_HWP_CAPABILITIES_LOWEST_PERFORMANCE(caps);

	/* hardware autonomous selection determines the performance target */
	sc->req &= ~IA32_HWP_DESIRED_PERFORMANCE;

	/* enable HW dynamic selection of window size */
	sc->req &= ~IA32_HWP_ACTIVITY_WINDOW;

	/* IA32_HWP_REQUEST.Minimum_Performance = IA32_HWP_CAPABILITIES.Lowest_Performance */
	sc->req &= ~IA32_HWP_MINIMUM_PERFORMANCE;
	sc->req |= sc->low;

	/* IA32_HWP_REQUEST.Maximum_Performance = IA32_HWP_CAPABILITIES.Highest_Performance. */
	sc->req &= ~IA32_HWP_REQUEST_MAXIMUM_PERFORMANCE;
	sc->req |= sc->high << 8;

	/* If supported, request package-level control for this CPU. */
	if (sc->hwp_pkg_ctrl_en)
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST, sc->req |
		    IA32_HWP_REQUEST_PACKAGE_CONTROL);
	else
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST, sc->req);
	if (ret) {
		device_printf(dev,
		    "Failed to setup%s autonomous HWP for cpu%d\n",
		    sc->hwp_pkg_ctrl_en ? " PKG" : "", pc->pc_cpuid);
		goto out;
	}

	/* If supported, write the PKG-wide control MSR. */
	if (sc->hwp_pkg_ctrl_en) {
		/*
		 * "The structure of the IA32_HWP_REQUEST_PKG MSR
		 * (package-level) is identical to the IA32_HWP_REQUEST MSR
		 * with the exception of the Package Control field, which does
		 * not exist." (Intel SDM §14.4.4)
		 */
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST_PKG, sc->req);
		if (ret) {
			device_printf(dev,
			    "Failed to set autonomous HWP for package\n");
		}
	}

out:
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);

	return (ret);
}

static int
intel_hwpstate_attach(device_t dev)
{
	struct hwp_softc *sc;
	int ret;

	sc = device_get_softc(dev);
	sc->dev = dev;

	/* eax */
	if (cpu_power_eax & CPUTPM1_HWP_NOTIFICATION)
		sc->hwp_notifications = true;
	if (cpu_power_eax & CPUTPM1_HWP_ACTIVITY_WINDOW)
		sc->hwp_activity_window = true;
	if (cpu_power_eax & CPUTPM1_HWP_PERF_PREF)
		sc->hwp_pref_ctrl = true;
	if (cpu_power_eax & CPUTPM1_HWP_PKG)
		sc->hwp_pkg_ctrl = true;

	/* Allow administrators to disable pkg-level control. */
	sc->hwp_pkg_ctrl_en = (sc->hwp_pkg_ctrl && hwpstate_pkg_ctrl_enable);

	/* ecx */
	if (cpu_power_ecx & CPUID_PERF_BIAS)
		sc->hwp_perf_bias = true;

	ret = set_autonomous_hwp(sc);
	if (ret)
		return (ret);

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_STATIC_CHILDREN(_debug), OID_AUTO, device_get_nameunit(dev),
	    CTLTYPE_STRING | CTLFLAG_RD | CTLFLAG_SKIP | CTLFLAG_MPSAFE,
	    sc, 0, intel_hwp_dump_sysctl_handler, "A", "");

	SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
	    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)), OID_AUTO,
	    "epp", CTLTYPE_INT | CTLFLAG_RWTUN | CTLFLAG_MPSAFE, dev, 0,
	    sysctl_epp_select, "I",
	    "Efficiency/Performance Preference "
	    "(range from 0, most performant, through 100, most efficient)");

	return (cpufreq_register(dev));
}

static int
intel_hwpstate_detach(device_t dev)
{

	return (cpufreq_unregister(dev));
}

static int
intel_hwpstate_get(device_t dev, struct cf_setting *set)
{
	struct pcpu *pc;
	uint64_t rate;
	int ret;

	if (set == NULL)
		return (EINVAL);

	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);

	memset(set, CPUFREQ_VAL_UNKNOWN, sizeof(*set));
	set->dev = dev;

	ret = cpu_est_clockrate(pc->pc_cpuid, &rate);
	if (ret == 0)
		set->freq = rate / 1000000;

	set->volts = CPUFREQ_VAL_UNKNOWN;
	set->power = CPUFREQ_VAL_UNKNOWN;
	set->lat = CPUFREQ_VAL_UNKNOWN;

	return (0);
}

static int
intel_hwpstate_type(device_t dev, int *type)
{
	if (type == NULL)
		return (EINVAL);
	*type = CPUFREQ_TYPE_ABSOLUTE | CPUFREQ_FLAG_INFO_ONLY | CPUFREQ_FLAG_UNCACHED;

	return (0);
}

static int
intel_hwpstate_suspend(device_t dev)
{
	return (0);
}

/*
 * Redo a subset of set_autonomous_hwp on resume; untested.  Without this,
 * testers observed that on resume MSR_IA32_HWP_REQUEST was bogus.
 */
static int
intel_hwpstate_resume(device_t dev)
{
	struct hwp_softc *sc;
	struct pcpu *pc;
	int ret;

	sc = device_get_softc(dev);

	pc = cpu_get_pcpu(dev);
	if (pc == NULL)
		return (ENXIO);

	thread_lock(curthread);
	sched_bind(curthread, pc->pc_cpuid);
	thread_unlock(curthread);

	ret = wrmsr_safe(MSR_IA32_PM_ENABLE, 1);
	if (ret) {
		device_printf(dev,
		    "Failed to enable HWP for cpu%d after suspend (%d)\n",
		    pc->pc_cpuid, ret);
		goto out;
	}

	if (sc->hwp_pkg_ctrl_en)
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST, sc->req |
		    IA32_HWP_REQUEST_PACKAGE_CONTROL);
	else
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST, sc->req);
	if (ret) {
		device_printf(dev,
		    "Failed to set%s autonomous HWP for cpu%d after suspend\n",
		    sc->hwp_pkg_ctrl_en ? " PKG" : "", pc->pc_cpuid);
		goto out;
	}
	if (sc->hwp_pkg_ctrl_en) {
		ret = wrmsr_safe(MSR_IA32_HWP_REQUEST_PKG, sc->req);
		if (ret) {
			device_printf(dev,
			    "Failed to set autonomous HWP for package after "
			    "suspend\n");
			goto out;
		}
	}
	if (!sc->hwp_pref_ctrl && sc->hwp_perf_bias_cached) {
		ret = wrmsr_safe(MSR_IA32_ENERGY_PERF_BIAS,
		    sc->hwp_energy_perf_bias);
		if (ret) {
			device_printf(dev,
			    "Failed to set energy perf bias for cpu%d after "
			    "suspend\n", pc->pc_cpuid);
		}
	}

out:
	thread_lock(curthread);
	sched_unbind(curthread);
	thread_unlock(curthread);

	return (ret);
}
