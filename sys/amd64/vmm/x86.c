/*-
 * Copyright (c) 2011 NetApp, Inc.
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
 * THIS SOFTWARE IS PROVIDED BY NETAPP, INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL NETAPP, INC OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/cpuset.h>

#include <machine/clock.h>
#include <machine/cpufunc.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <machine/vmm.h>

#include "x86.h"

#define	CPUID_VM_HIGH		0x40000000

static const char bhyve_id[12] = "bhyve bhyve ";

static uint64_t bhyve_xcpuids;

int
x86_emulate_cpuid(struct vm *vm, int vcpu_id,
		  uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
	int error;
	unsigned int 	func, regs[4];
	enum x2apic_state x2apic_state;

	/*
	 * Requests for invalid CPUID levels should map to the highest
	 * available level instead.
	 */
	if (cpu_exthigh != 0 && *eax >= 0x80000000) {
		if (*eax > cpu_exthigh)
			*eax = cpu_exthigh;
	} else if (*eax >= 0x40000000) {
		if (*eax > CPUID_VM_HIGH)
			*eax = CPUID_VM_HIGH;
	} else if (*eax > cpu_high) {
		*eax = cpu_high;
	}

	func = *eax;

	/*
	 * In general the approach used for CPU topology is to
	 * advertise a flat topology where all CPUs are packages with
	 * no multi-core or SMT.
	 */
	switch (func) {
		/*
		 * Pass these through to the guest
		 */
		case CPUID_0000_0000:
		case CPUID_0000_0002:
		case CPUID_0000_0003:
		case CPUID_8000_0000:
		case CPUID_8000_0002:
		case CPUID_8000_0003:
		case CPUID_8000_0004:
		case CPUID_8000_0006:
		case CPUID_8000_0008:
			cpuid_count(*eax, *ecx, regs);
			break;

		case CPUID_8000_0001:
			/*
			 * Hide rdtscp/ia32_tsc_aux until we know how
			 * to deal with them.
			 */
			cpuid_count(*eax, *ecx, regs);
			regs[3] &= ~AMDID_RDTSCP;
			break;

		case CPUID_8000_0007:
			cpuid_count(*eax, *ecx, regs);
			/*
			 * If the host TSCs are not synchronized across
			 * physical cpus then we cannot advertise an
			 * invariant tsc to a vcpu.
			 *
			 * XXX This still falls short because the vcpu
			 * can observe the TSC moving backwards as it
			 * migrates across physical cpus. But at least
			 * it should discourage the guest from using the
			 * TSC to keep track of time.
			 */
			if (!smp_tsc)
				regs[3] &= ~AMDPM_TSC_INVARIANT;
			break;

		case CPUID_0000_0001:
			do_cpuid(1, regs);

			error = vm_get_x2apic_state(vm, vcpu_id, &x2apic_state);
			if (error) {
				panic("x86_emulate_cpuid: error %d "
				      "fetching x2apic state", error);
			}

			/*
			 * Override the APIC ID only in ebx
			 */
			regs[1] &= ~(CPUID_LOCAL_APIC_ID);
			regs[1] |= (vcpu_id << CPUID_0000_0001_APICID_SHIFT);

			/*
			 * Don't expose VMX, SpeedStep or TME capability.
			 * Advertise x2APIC capability and Hypervisor guest.
			 */
			regs[2] &= ~(CPUID2_VMX | CPUID2_EST | CPUID2_TM2);

			regs[2] |= CPUID2_HV;

			if (x2apic_state != X2APIC_DISABLED)
				regs[2] |= CPUID2_X2APIC;

			/*
			 * Hide xsave/osxsave/avx until the FPU save/restore
			 * issues are resolved
			 */
			regs[2] &= ~(CPUID2_XSAVE | CPUID2_OSXSAVE |
				     CPUID2_AVX);

			/*
			 * Hide monitor/mwait until we know how to deal with
			 * these instructions.
			 */
			regs[2] &= ~CPUID2_MON;

                        /*
			 * Hide the performance and debug features.
			 */
			regs[2] &= ~CPUID2_PDCM;
			
			/*
			 * Hide thermal monitoring
			 */
			regs[3] &= ~(CPUID_ACPI | CPUID_TM);
			
			/*
			 * Machine check handling is done in the host.
			 * Hide MTRR capability.
			 */
			regs[3] &= ~(CPUID_MCA | CPUID_MCE | CPUID_MTRR);

                        /*
                        * Hide the debug store capability.
                        */
			regs[3] &= ~CPUID_DS;

			/*
			 * Disable multi-core.
			 */
			regs[1] &= ~CPUID_HTT_CORES;
			regs[3] &= ~CPUID_HTT;
			break;

		case CPUID_0000_0004:
			do_cpuid(4, regs);

			/*
			 * Do not expose topology.
			 */
			regs[0] &= 0xffff8000;
			regs[0] |= 0x04008000;
			break;

		case CPUID_0000_0006:
		case CPUID_0000_0007:
		case CPUID_0000_000A:
			/*
			 * Handle the access, but report 0 for
			 * all options
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = 0;
			regs[3] = 0;
			break;

		case CPUID_0000_000B:
			/*
			 * Processor topology enumeration
			 */
			regs[0] = 0;
			regs[1] = 0;
			regs[2] = *ecx & 0xff;
			regs[3] = vcpu_id;
			break;

		case 0x40000000:
			regs[0] = CPUID_VM_HIGH;
			bcopy(bhyve_id, &regs[1], 4);
			bcopy(bhyve_id + 4, &regs[2], 4);
			bcopy(bhyve_id + 8, &regs[3], 4);
			break;

		default:
			/*
			 * The leaf value has already been clamped so
			 * simply pass this through, keeping count of
			 * how many unhandled leaf values have been seen.
			 */
			atomic_add_long(&bhyve_xcpuids, 1);
			cpuid_count(*eax, *ecx, regs);
			break;
	}

	*eax = regs[0];
	*ebx = regs[1];
	*ecx = regs[2];
	*edx = regs[3];

	return (1);
}
