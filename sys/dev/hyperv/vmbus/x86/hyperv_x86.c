/*- SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 * Copyright (c) 2009-2012,2016-2017, 2022 Microsoft Corp.
 * Copyright (c) 2012 NetApp Inc.
 * Copyright (c) 2012 Citrix Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Implements low-level interactions with Hyper-V/Azure
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/timetc.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>

#include <dev/hyperv/include/hyperv.h>
#include <dev/hyperv/include/hyperv_busdma.h>
#include <dev/hyperv/vmbus/hyperv_var.h>
#include <dev/hyperv/vmbus/x86/hyperv_machdep.h>
#include <dev/hyperv/vmbus/x86/hyperv_reg.h>
#include <dev/hyperv/vmbus/hyperv_common_reg.h>

#define HYPERV_FREEBSD_BUILD 0ULL
#define HYPERV_FREEBSD_VERSION ((uint64_t)__FreeBSD_version)
#define HYPERV_FREEBSD_OSID 0ULL

void hyperv_init_tc(void);
int hypercall_page_setup(vm_paddr_t);
void hypercall_disable(void);
bool hyperv_identify_features(void);

u_int hyperv_ver_major;
u_int hyperv_features;
u_int hyperv_recommends;

hyperv_tc64_t hyperv_tc64;

static u_int hyperv_pm_features;
static u_int hyperv_features3;
static u_int hyperv_get_timecount(struct timecounter *);

static struct timecounter hyperv_timecounter = {
	.tc_get_timecount = hyperv_get_timecount,
	.tc_poll_pps = NULL,
	.tc_counter_mask = 0xffffffff,
	.tc_frequency = HYPERV_TIMER_FREQ,
	.tc_name = "Hyper-V",
	.tc_quality = 2000,
	.tc_flags = 0,
	.tc_priv = NULL
};

static u_int
hyperv_get_timecount(struct timecounter *tc __unused)
{
	return rdmsr(MSR_HV_TIME_REF_COUNT);
}

static uint64_t
hyperv_tc64_rdmsr(void)
{

	return (rdmsr(MSR_HV_TIME_REF_COUNT));
}

void
hyperv_init_tc(void)
{
	if (hyperv_features & CPUID_HV_MSR_TIME_REFCNT) {
		/*
		 * Register Hyper-V timecounter.  This should be done as early
		 * as possible to let DELAY() work, since the 8254 PIT is not
		 * reliably emulated or even available.
		 */
		tc_init(&hyperv_timecounter);

		/*
		 * Install 64 bits timecounter method for other modules
		 * to use.
		 */
		hyperv_tc64 = hyperv_tc64_rdmsr;
	}
}

int
hypercall_page_setup(vm_paddr_t paddr)
{
	uint64_t hc, hc_orig;
	hc_orig = rdmsr(MSR_HV_HYPERCALL);

	/*
	 * Setup the Hypercall page.
	 *
	 * NOTE: 'reserved' bits MUST be preserved.
	 */
	hc = ((paddr >> PAGE_SHIFT) << MSR_HV_HYPERCALL_PGSHIFT) |
	    (hc_orig & MSR_HV_HYPERCALL_RSVD_MASK) | MSR_HV_HYPERCALL_ENABLE;
	wrmsr(MSR_HV_HYPERCALL, hc);

	/*
	 * Confirm that Hypercall page did get setup.
	 */
	hc = rdmsr(MSR_HV_HYPERCALL);
	if ((hc & MSR_HV_HYPERCALL_ENABLE) == 0) {
		printf("hyperv: Hypercall setup failed\n");
		/* Can't perform any Hyper-V specific actions */
		vm_guest = VM_GUEST_VM;
		return (-1);
	}
	return (0);
}

void
hypercall_disable(void)
{
	uint64_t hc;
	/* Disable Hypercall */
	hc = rdmsr(MSR_HV_HYPERCALL);
	wrmsr(MSR_HV_HYPERCALL, (hc & MSR_HV_HYPERCALL_RSVD_MASK));
}

bool
hyperv_identify_features(void)
{
	u_int regs[4];
	unsigned int maxleaf;

	if (vm_guest != VM_GUEST_HV)
		return (false);

	do_cpuid(CPUID_LEAF_HV_MAXLEAF, regs);
	maxleaf = regs[0];
	if (maxleaf < CPUID_LEAF_HV_LIMITS)
		return (false);

	do_cpuid(CPUID_LEAF_HV_INTERFACE, regs);
	if (regs[0] != CPUID_HV_IFACE_HYPERV)
		return (false);

	do_cpuid(CPUID_LEAF_HV_FEATURES, regs);
	if ((regs[0] & CPUID_HV_MSR_HYPERCALL) == 0) {
		/*
		 * Hyper-V w/o Hypercall is impossible; someone
		 * is faking Hyper-V.
		 */
		return (false);
	}
	hyperv_features = regs[0];
	hyperv_pm_features = regs[2];
	hyperv_features3 = regs[3];
	do_cpuid(CPUID_LEAF_HV_IDENTITY, regs);
	hyperv_ver_major = regs[1] >> 16;
	printf("Hyper-V Version: %d.%d.%d [SP%d]\n", hyperv_ver_major,
	    regs[1] & 0xffff, regs[0], regs[2]);

	printf("  Features=0x%b\n", hyperv_features,
	    "\020"
	    "\001VPRUNTIME" /* MSR_HV_VP_RUNTIME */
	    "\002TMREFCNT"  /* MSR_HV_TIME_REF_COUNT */
	    "\003SYNIC"	    /* MSRs for SynIC */
	    "\004SYNTM"	    /* MSRs for SynTimer */
	    "\005APIC"	    /* MSR_HV_{EOI,ICR,TPR} */
	    "\006HYPERCALL" /* MSR_HV_{GUEST_OS_ID,HYPERCALL} */
	    "\007VPINDEX"   /* MSR_HV_VP_INDEX */
	    "\010RESET"	    /* MSR_HV_RESET */
	    "\011STATS"	    /* MSR_HV_STATS_ */
	    "\012REFTSC"    /* MSR_HV_REFERENCE_TSC */
	    "\013IDLE"	    /* MSR_HV_GUEST_IDLE */
	    "\014TMFREQ"    /* MSR_HV_{TSC,APIC}_FREQUENCY */
	    "\015DEBUG");   /* MSR_HV_SYNTH_DEBUG_ */
	printf("  PM Features=0x%b [C%u]\n",
	    (hyperv_pm_features & ~CPUPM_HV_CSTATE_MASK),
	    "\020"
	    "\005C3HPET", /* HPET is required for C3 state */
	    CPUPM_HV_CSTATE(hyperv_pm_features));
	printf("  Features3=0x%b\n", hyperv_features3,
	    "\020"
	    "\001MWAIT"	   /* MWAIT */
	    "\002DEBUG"	   /* guest debug support */
	    "\003PERFMON"  /* performance monitor */
	    "\004PCPUDPE"  /* physical CPU dynamic partition event */
	    "\005XMMHC"	   /* hypercall input through XMM regs */
	    "\006IDLE"	   /* guest idle support */
	    "\007SLEEP"	   /* hypervisor sleep support */
	    "\010NUMA"	   /* NUMA distance query support */
	    "\011TMFREQ"   /* timer frequency query (TSC, LAPIC) */
	    "\012SYNCMC"   /* inject synthetic machine checks */
	    "\013CRASH"	   /* MSRs for guest crash */
	    "\014DEBUGMSR" /* MSRs for guest debug */
	    "\015NPIEP"	   /* NPIEP */
	    "\016HVDIS");  /* disabling hypervisor */

	do_cpuid(CPUID_LEAF_HV_RECOMMENDS, regs);
	hyperv_recommends = regs[0];
	if (bootverbose)
		printf("  Recommends: %08x %08x\n", regs[0], regs[1]);

	do_cpuid(CPUID_LEAF_HV_LIMITS, regs);
	if (bootverbose) {
		printf("  Limits: Vcpu:%d Lcpu:%d Int:%d\n", regs[0], regs[1],
		    regs[2]);
	}

	if (maxleaf >= CPUID_LEAF_HV_HWFEATURES) {
		do_cpuid(CPUID_LEAF_HV_HWFEATURES, regs);
		if (bootverbose) {
			printf("  HW Features: %08x, AMD: %08x\n", regs[0],
			    regs[3]);
		}
	}
	return (true);
}
