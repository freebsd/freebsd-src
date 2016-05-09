/*-
 * Copyright (c) KATO Takenori, 1997, 1998.
 * 
 * All rights reserved.  Unpublished rights reserved under the copyright
 * laws of Japan.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "opt_cpu.h"

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/pcpu.h>
#include <sys/systm.h>
#include <sys/sysctl.h>

#include <machine/cputypes.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>

#include <vm/vm.h>
#include <vm/pmap.h>

static int	hw_instruction_sse;
SYSCTL_INT(_hw, OID_AUTO, instruction_sse, CTLFLAG_RD,
    &hw_instruction_sse, 0, "SIMD/MMX2 instructions available in CPU");
/*
 * -1: automatic (default)
 *  0: keep enable CLFLUSH
 *  1: force disable CLFLUSH
 */
static int	hw_clflush_disable = -1;

static void
init_amd(void)
{
	uint64_t msr;

	/*
	 * Work around Erratum 721 for Family 10h and 12h processors.
	 * These processors may incorrectly update the stack pointer
	 * after a long series of push and/or near-call instructions,
	 * or a long series of pop and/or near-return instructions.
	 *
	 * http://support.amd.com/us/Processor_TechDocs/41322_10h_Rev_Gd.pdf
	 * http://support.amd.com/us/Processor_TechDocs/44739_12h_Rev_Gd.pdf
	 *
	 * Hypervisors do not provide access to the errata MSR,
	 * causing #GP exception on attempt to apply the errata.  The
	 * MSR write shall be done on host and persist globally
	 * anyway, so do not try to do it when under virtualization.
	 */
	switch (CPUID_TO_FAMILY(cpu_id)) {
	case 0x10:
	case 0x12:
		if ((cpu_feature2 & CPUID2_HV) == 0)
			wrmsr(0xc0011029, rdmsr(0xc0011029) | 1);
		break;
	}

	/*
	 * BIOS may fail to set InitApicIdCpuIdLo to 1 as it should per BKDG.
	 * So, do it here or otherwise some tools could be confused by
	 * Initial Local APIC ID reported with CPUID Function 1 in EBX.
	 */
	if (CPUID_TO_FAMILY(cpu_id) == 0x10) {
		if ((cpu_feature2 & CPUID2_HV) == 0) {
			msr = rdmsr(MSR_NB_CFG1);
			msr |= (uint64_t)1 << 54;
			wrmsr(MSR_NB_CFG1, msr);
		}
	}
}

/*
 * Initialize special VIA features
 */
static void
init_via(void)
{
	u_int regs[4], val;

	/*
	 * Check extended CPUID for PadLock features.
	 *
	 * http://www.via.com.tw/en/downloads/whitepapers/initiatives/padlock/programming_guide.pdf
	 */
	do_cpuid(0xc0000000, regs);
	if (regs[0] >= 0xc0000001) {
		do_cpuid(0xc0000001, regs);
		val = regs[3];
	} else
		return;

	/* Enable RNG if present. */
	if ((val & VIA_CPUID_HAS_RNG) != 0) {
		via_feature_rng = VIA_HAS_RNG;
		wrmsr(0x110B, rdmsr(0x110B) | VIA_CPUID_DO_RNG);
	}

	/* Enable PadLock if present. */
	if ((val & VIA_CPUID_HAS_ACE) != 0)
		via_feature_xcrypt |= VIA_HAS_AES;
	if ((val & VIA_CPUID_HAS_ACE2) != 0)
		via_feature_xcrypt |= VIA_HAS_AESCTR;
	if ((val & VIA_CPUID_HAS_PHE) != 0)
		via_feature_xcrypt |= VIA_HAS_SHA;
	if ((val & VIA_CPUID_HAS_PMM) != 0)
		via_feature_xcrypt |= VIA_HAS_MM;
	if (via_feature_xcrypt != 0)
		wrmsr(0x1107, rdmsr(0x1107) | (1 << 28));
}

/*
 * Initialize CPU control registers
 */
void
initializecpu(void)
{
	uint64_t msr;
	uint32_t cr4;

	cr4 = rcr4();
	if ((cpu_feature & CPUID_XMM) && (cpu_feature & CPUID_FXSR)) {
		cr4 |= CR4_FXSR | CR4_XMM;
		cpu_fxsr = hw_instruction_sse = 1;
	}
	if (cpu_stdext_feature & CPUID_STDEXT_FSGSBASE)
		cr4 |= CR4_FSGSBASE;

	/*
	 * Postpone enabling the SMEP on the boot CPU until the page
	 * tables are switched from the boot loader identity mapping
	 * to the kernel tables.  The boot loader enables the U bit in
	 * its tables.
	 */
	if (!IS_BSP() && (cpu_stdext_feature & CPUID_STDEXT_SMEP))
		cr4 |= CR4_SMEP;
	load_cr4(cr4);
	if ((amd_feature & AMDID_NX) != 0) {
		msr = rdmsr(MSR_EFER) | EFER_NXE;
		wrmsr(MSR_EFER, msr);
		pg_nx = PG_NX;
	}
	switch (cpu_vendor_id) {
	case CPU_VENDOR_AMD:
		init_amd();
		break;
	case CPU_VENDOR_CENTAUR:
		init_via();
		break;
	}
}

void
initializecpucache(void)
{

	/*
	 * CPUID with %eax = 1, %ebx returns
	 * Bits 15-8: CLFLUSH line size
	 * 	(Value * 8 = cache line size in bytes)
	 */
	if ((cpu_feature & CPUID_CLFSH) != 0)
		cpu_clflush_line_size = ((cpu_procinfo >> 8) & 0xff) * 8;
	/*
	 * XXXKIB: (temporary) hack to work around traps generated
	 * when CLFLUSHing APIC register window under virtualization
	 * environments.  These environments tend to disable the
	 * CPUID_SS feature even though the native CPU supports it.
	 */
	TUNABLE_INT_FETCH("hw.clflush_disable", &hw_clflush_disable);
	if (vm_guest != VM_GUEST_NO && hw_clflush_disable == -1) {
		cpu_feature &= ~CPUID_CLFSH;
		cpu_stdext_feature &= ~CPUID_STDEXT_CLFLUSHOPT;
	}

	/*
	 * The kernel's use of CLFLUSH{,OPT} can be disabled manually
	 * by setting the hw.clflush_disable tunable.
	 */
	if (hw_clflush_disable == 1) {
		cpu_feature &= ~CPUID_CLFSH;
		cpu_stdext_feature &= ~CPUID_STDEXT_CLFLUSHOPT;
	}
}
