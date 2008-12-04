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

int	cpu;			/* Are we 386, 386sx, 486, etc? */
u_int	cpu_feature;		/* Feature flags */
u_int	cpu_feature2;		/* Feature flags */
u_int	amd_feature;		/* AMD feature flags */
u_int	amd_feature2;		/* AMD feature flags */
u_int	amd_pminfo;		/* AMD advanced power management info */
u_int	cpu_high;		/* Highest arg to CPUID */
u_int	cpu_exthigh;		/* Highest arg to extended CPUID */
u_int	cpu_id;			/* Stepping ID */
u_int	cpu_procinfo;		/* HyperThreading Info / Brand Index / CLFUSH */
u_int	cpu_procinfo2;		/* Multicore info */
char	cpu_vendor[20];		/* CPU Origin code */
u_int	cpu_vendor_id;		/* CPU vendor ID */
u_int	cpu_fxsr;		/* SSE enabled */
u_int	cpu_mxcsr_mask;		/* Valid bits in mxcsr */

/*
 * Initialize CPU control registers
 */
void
initializecpu(void)
{
	uint64_t msr;

	if ((cpu_feature & CPUID_XMM) && (cpu_feature & CPUID_FXSR)) {
		load_cr4(rcr4() | CR4_FXSR | CR4_XMM);
		cpu_fxsr = hw_instruction_sse = 1;
	}
	if ((amd_feature & AMDID_NX) != 0) {
		msr = rdmsr(MSR_EFER) | EFER_NXE;
		wrmsr(MSR_EFER, msr);
		pg_nx = PG_NX;
	}
}
