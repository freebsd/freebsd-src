/*-
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
#include <sys/systm.h>
#include <sys/limits.h>

#include <x86/hypervisor.h>
#include <x86/vmware.h>

static uint32_t vmware_cpuid_base = -1;
static uint32_t vmware_cpuid_high = -1;

static int
vmware_cpuid_identify(void)
{

	/*
	 * KB1009458: Mechanisms to determine if software is running in a
	 * VMware virtual machine: http://kb.vmware.com/kb/1009458
	 */
	if (vmware_cpuid_base == -1) {
		hypervisor_cpuid_base("VMwareVMware", 0, &vmware_cpuid_base,
		    &vmware_cpuid_high);
	}

	return (vmware_cpuid_base > 0);
}

uint64_t
vmware_tsc_freq(void)
{
	uint64_t freq;
	u_int regs[4];

	if (vmware_cpuid_high >= 0x40000010) {
		do_cpuid(0x40000010, regs);
		freq = regs[0] * 1000;
	} else {
		vmware_hvcall(VMW_HVCMD_GETHZ, regs);
		if (regs[1] != UINT_MAX)
			freq = regs[0] | ((uint64_t)regs[1] << 32);
		else
			freq = 0;
	}

	return (freq);
}

static void
vmware_init(void)
{

	if (vmware_cpuid_identify() != 0)
		hypervisor_register("VMware", VM_GUEST_VMWARE, NULL);
}

HYPERVISOR_SYSINIT(vmware, vmware_init);
