/*
 * Copyright (c) 2014 Bryan Venteicher <bryanv@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS AS IS'' AND
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
#include <sys/kernel.h>
#include <sys/limits.h>
#include <sys/systm.h>
#include <sys/pcpu.h>
#include <sys/smp.h>
#include <sys/timetc.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <machine/pvclock.h>
#include <x86/kvm.h>

static u_int	kvm_clock_get_timecounter(struct timecounter *);
static void	kvm_clock_pcpu_system_time(void *);

DPCPU_DEFINE(struct pvclock_vcpu_time_info, kvm_clock_vcpu_time_info);

static struct timecounter kvm_clock_timecounter = {
	kvm_clock_get_timecounter,
	NULL,
	~0u,
	1000000000ULL,
	"KVMCLOCK",
	1000,
};

static uint32_t kvm_clock_wall_clock_msr;
static uint32_t kvm_clock_system_time_msr;

uint64_t
kvm_clock_tsc_freq(void)
{
	struct pvclock_vcpu_time_info *ti;
	uint64_t freq;

	critical_enter();
	ti = DPCPU_PTR(kvm_clock_vcpu_time_info);
	freq = pvclock_tsc_freq(ti);
	critical_exit();

	return (freq);
}

static u_int
kvm_clock_get_timecounter(struct timecounter *tc)
{
	struct pvclock_vcpu_time_info *ti;
	uint64_t time;

	critical_enter();
	ti = DPCPU_PTR(kvm_clock_vcpu_time_info);
	time = pvclock_get_timecount(ti);
	critical_exit();

	return (time & UINT_MAX);
}

static void
kvm_clock_pcpu_system_time(void *arg)
{
	uint64_t data;
	int enable;

	enable = *(int *) arg;

	if (enable != 0)
		data = vtophys(DPCPU_PTR(kvm_clock_vcpu_time_info)) | 1;
	else
		data = 0;

	wrmsr(kvm_clock_system_time_msr, data);
}

static void
kvm_clock_init(void)
{
	uint32_t features;
	int enable;

	if (vm_guest != VM_GUEST_KVM || !kvm_paravirt_supported())
		return;

	features = kvm_get_features();

	if (features & KVM_FEATURE_CLOCKSOURCE2) {
		kvm_clock_wall_clock_msr = KVM_MSR_WALL_CLOCK_NEW;
		kvm_clock_system_time_msr = KVM_MSR_SYSTEM_TIME_NEW;
	} else if (features & KVM_FEATURE_CLOCKSOURCE) {
		kvm_clock_wall_clock_msr = KVM_MSR_WALL_CLOCK;
		kvm_clock_system_time_msr = KVM_MSR_SYSTEM_TIME;
	} else
		return;

	enable = 1;
	smp_rendezvous(smp_no_rendevous_barrier, kvm_clock_pcpu_system_time,
	    smp_no_rendevous_barrier, &enable);

	tc_init(&kvm_clock_timecounter);
}

SYSINIT(kvm_clock, SI_SUB_SMP, SI_ORDER_ANY, kvm_clock_init, NULL);
