/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2014 Bryan Venteicher <bryanv@FreeBSD.org>
 * Copyright (c) 2021 Mathieu Chouquet-Stringer
 * Copyright (c) 2021 Juniper Networks, Inc.
 * Copyright (c) 2021 Klara, Inc.
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
 * Linux KVM paravirtual clock support
 *
 * References:
 *     - [1] https://www.kernel.org/doc/html/latest/virt/kvm/cpuid.html
 *     - [2] https://www.kernel.org/doc/html/latest/virt/kvm/msr.html
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/domainset.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/smp.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>

#include <machine/pvclock.h>
#include <x86/kvm.h>

#include "clock_if.h"

#define	KVM_CLOCK_DEVNAME		"kvmclock"
/*
 * Note: Chosen to be (1) above HPET's value (always 950), (2) above the TSC's
 * default value of 800, and (3) below the TSC's value when it supports the
 * "Invariant TSC" feature and is believed to be synchronized across all CPUs.
 */
#define	KVM_CLOCK_TC_QUALITY		975

struct kvm_clock_softc {
	struct pvclock			 pvc;
	struct pvclock_wall_clock	 wc;
	struct pvclock_vcpu_time_info	*timeinfos;
	u_int				 msr_tc;
	u_int				 msr_wc;
#ifndef EARLY_AP_STARTUP
	int				 firstcpu;
#endif
};

static struct pvclock_wall_clock *kvm_clock_get_wallclock(void *arg);
static void	kvm_clock_system_time_enable(struct kvm_clock_softc *sc,
		    const cpuset_t *cpus);
static void	kvm_clock_system_time_enable_pcpu(void *arg);
static void	kvm_clock_setup_sysctl(device_t);

static struct pvclock_wall_clock *
kvm_clock_get_wallclock(void *arg)
{
	struct kvm_clock_softc *sc = arg;

	wrmsr(sc->msr_wc, vtophys(&sc->wc));
	return (&sc->wc);
}

static void
kvm_clock_system_time_enable(struct kvm_clock_softc *sc, const cpuset_t *cpus)
{
	smp_rendezvous_cpus(*cpus, NULL, kvm_clock_system_time_enable_pcpu,
	    NULL, sc);
}

static void
kvm_clock_system_time_enable_pcpu(void *arg)
{
	struct kvm_clock_softc *sc = arg;

	/*
	 * See [2]; the lsb of this MSR is the system time enable bit.
	 */
	wrmsr(sc->msr_tc, vtophys(&(sc->timeinfos)[curcpu]) | 1);
}

#ifndef EARLY_AP_STARTUP
static void
kvm_clock_init_smp(void *arg __unused)
{
	devclass_t kvm_clock_devclass;
	cpuset_t cpus;
	struct kvm_clock_softc *sc;

	kvm_clock_devclass = devclass_find(KVM_CLOCK_DEVNAME);
	sc = devclass_get_softc(kvm_clock_devclass, 0);
	if (sc == NULL || mp_ncpus == 1)
		return;

	/*
	 * Register with the hypervisor on all CPUs except the one that
	 * registered in kvm_clock_attach().
	 */
	cpus = all_cpus;
	KASSERT(CPU_ISSET(sc->firstcpu, &cpus),
	    ("%s: invalid first CPU %d", __func__, sc->firstcpu));
	CPU_CLR(sc->firstcpu, &cpus);
	kvm_clock_system_time_enable(sc, &cpus);
}
SYSINIT(kvm_clock, SI_SUB_SMP, SI_ORDER_ANY, kvm_clock_init_smp, NULL);
#endif

static void
kvm_clock_identify(driver_t *driver, device_t parent)
{
	u_int regs[4];

	kvm_cpuid_get_features(regs);
	if ((regs[0] &
	    (KVM_FEATURE_CLOCKSOURCE2 | KVM_FEATURE_CLOCKSOURCE)) == 0)
		return;
	if (device_find_child(parent, KVM_CLOCK_DEVNAME, DEVICE_UNIT_ANY))
		return;
	BUS_ADD_CHILD(parent, 0, KVM_CLOCK_DEVNAME, 0);
}

static int
kvm_clock_probe(device_t dev)
{
	device_set_desc(dev, "KVM paravirtual clock");
	return (BUS_PROBE_DEFAULT);
}

static int
kvm_clock_attach(device_t dev)
{
	u_int regs[4];
	struct kvm_clock_softc *sc = device_get_softc(dev);
	bool stable_flag_supported;

	/* Process KVM "features" CPUID leaf content: */
	kvm_cpuid_get_features(regs);
	if ((regs[0] & KVM_FEATURE_CLOCKSOURCE2) != 0) {
		sc->msr_tc = KVM_MSR_SYSTEM_TIME_NEW;
		sc->msr_wc = KVM_MSR_WALL_CLOCK_NEW;
	} else {
		KASSERT((regs[0] & KVM_FEATURE_CLOCKSOURCE) != 0,
		    ("Clocksource feature flags disappeared since "
		    "kvm_clock_identify: regs[0] %#0x.", regs[0]));
		sc->msr_tc = KVM_MSR_SYSTEM_TIME;
		sc->msr_wc = KVM_MSR_WALL_CLOCK;
	}
	stable_flag_supported =
	    (regs[0] & KVM_FEATURE_CLOCKSOURCE_STABLE_BIT) != 0;

	/* Set up 'struct pvclock_vcpu_time_info' page(s): */
	sc->timeinfos = kmem_malloc(mp_ncpus *
	    sizeof(struct pvclock_vcpu_time_info), M_WAITOK | M_ZERO);
#ifdef EARLY_AP_STARTUP
	kvm_clock_system_time_enable(sc, &all_cpus);
#else
	sc->firstcpu = curcpu;
	kvm_clock_system_time_enable_pcpu(sc);
#endif

	/*
	 * Init pvclock; register KVM clock wall clock, register KVM clock
	 * timecounter, and set up the requisite infrastructure for vDSO access
	 * to this timecounter.
	 *     Regarding 'tc_flags': Since the KVM MSR documentation does not
	 *     specifically discuss suspend/resume scenarios, conservatively
	 *     leave 'TC_FLAGS_SUSPEND_SAFE' cleared and assume that the system
	 *     time must be re-inited in such cases.
	 */
	sc->pvc.get_wallclock = kvm_clock_get_wallclock;
	sc->pvc.get_wallclock_arg = sc;
	sc->pvc.timeinfos = sc->timeinfos;
	sc->pvc.stable_flag_supported = stable_flag_supported;
	pvclock_init(&sc->pvc, dev, KVM_CLOCK_DEVNAME, KVM_CLOCK_TC_QUALITY, 0);
	kvm_clock_setup_sysctl(dev);
	return (0);
}

static int
kvm_clock_detach(device_t dev)
{
	struct kvm_clock_softc *sc = device_get_softc(dev);

	return (pvclock_destroy(&sc->pvc));
}

static int
kvm_clock_suspend(device_t dev)
{
	return (0);
}

static int
kvm_clock_resume(device_t dev)
{
	/*
	 * See note in 'kvm_clock_attach()' regarding 'TC_FLAGS_SUSPEND_SAFE';
	 * conservatively assume that the system time must be re-inited in
	 * suspend/resume scenarios.
	 */
	kvm_clock_system_time_enable(device_get_softc(dev), &all_cpus);
	pvclock_resume();
	inittodr(time_second);
	return (0);
}

static int
kvm_clock_gettime(device_t dev, struct timespec *ts)
{
	struct kvm_clock_softc *sc = device_get_softc(dev);

	pvclock_gettime(&sc->pvc, ts);
	return (0);
}

static int
kvm_clock_settime(device_t dev, struct timespec *ts)
{
	/*
	 * Even though it is not possible to set the KVM clock's wall clock, to
	 * avoid the possibility of periodic benign error messages from
	 * 'settime_task_func()', report success rather than, e.g., 'ENODEV'.
	 */
	return (0);
}

static int
kvm_clock_tsc_freq_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct kvm_clock_softc *sc = oidp->oid_arg1;
        uint64_t freq = pvclock_tsc_freq(sc->timeinfos);

        return (sysctl_handle_64(oidp, &freq, 0, req));
}

static void
kvm_clock_setup_sysctl(device_t dev)
{
	struct kvm_clock_softc *sc = device_get_softc(dev);
        struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(dev);
        struct sysctl_oid *tree = device_get_sysctl_tree(dev);
        struct sysctl_oid_list *child = SYSCTL_CHILDREN(tree);

        SYSCTL_ADD_PROC(ctx, child, OID_AUTO, "tsc_freq",
            CTLTYPE_U64 | CTLFLAG_RD | CTLFLAG_MPSAFE, sc, 0,
            kvm_clock_tsc_freq_sysctl, "QU",
            "Time Stamp Counter frequency");
}

static device_method_t kvm_clock_methods[] = {
	DEVMETHOD(device_identify,	kvm_clock_identify),
	DEVMETHOD(device_probe,		kvm_clock_probe),
	DEVMETHOD(device_attach,	kvm_clock_attach),
	DEVMETHOD(device_detach,	kvm_clock_detach),
	DEVMETHOD(device_suspend,	kvm_clock_suspend),
	DEVMETHOD(device_resume,	kvm_clock_resume),
	/* clock interface */
	DEVMETHOD(clock_gettime,	kvm_clock_gettime),
	DEVMETHOD(clock_settime,	kvm_clock_settime),

	DEVMETHOD_END
};

static driver_t kvm_clock_driver = {
	KVM_CLOCK_DEVNAME,
	kvm_clock_methods,
	sizeof(struct kvm_clock_softc),
};

DRIVER_MODULE(kvm_clock, nexus, kvm_clock_driver, 0, 0);
