/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Alexandru Elisei <alexandru.elisei@gmail.com>
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>

#include <machine/armreg.h>
#include <machine/cpu.h>
#include <machine/hypervisor.h>

#include "arm64.h"
#include "reset.h"

/*
 * Make the architecturally UNKNOWN value 0. As a bonus, we don't have to
 * manually set all those RES0 fields.
 */
#define	ARCH_UNKNOWN		0
#define	set_arch_unknown(reg)	(memset(&(reg), ARCH_UNKNOWN, sizeof(reg)))

void
reset_vm_el01_regs(void *vcpu)
{
	struct hypctx *el2ctx;

	el2ctx = vcpu;

	set_arch_unknown(el2ctx->tf);

	set_arch_unknown(el2ctx->actlr_el1);
	set_arch_unknown(el2ctx->afsr0_el1);
	set_arch_unknown(el2ctx->afsr1_el1);
	set_arch_unknown(el2ctx->amair_el1);
	set_arch_unknown(el2ctx->contextidr_el1);
	set_arch_unknown(el2ctx->cpacr_el1);
	set_arch_unknown(el2ctx->csselr_el1);
	set_arch_unknown(el2ctx->elr_el1);
	set_arch_unknown(el2ctx->esr_el1);
	set_arch_unknown(el2ctx->far_el1);
	set_arch_unknown(el2ctx->mair_el1);
	set_arch_unknown(el2ctx->mdccint_el1);
	set_arch_unknown(el2ctx->mdscr_el1);
	set_arch_unknown(el2ctx->par_el1);

	/*
	 * Guest starts with:
	 * ~SCTLR_M: MMU off
	 * ~SCTLR_C: data cache off
	 * SCTLR_CP15BEN: memory barrier instruction enable from EL0; RAO/WI
	 * ~SCTLR_I: instruction cache off
	 */
	el2ctx->sctlr_el1 = SCTLR_RES1;
	el2ctx->sctlr_el1 &= ~SCTLR_M & ~SCTLR_C & ~SCTLR_I;
	el2ctx->sctlr_el1 |= SCTLR_CP15BEN;

	set_arch_unknown(el2ctx->sp_el0);
	set_arch_unknown(el2ctx->tcr_el1);
	set_arch_unknown(el2ctx->tpidr_el0);
	set_arch_unknown(el2ctx->tpidr_el1);
	set_arch_unknown(el2ctx->tpidrro_el0);
	set_arch_unknown(el2ctx->ttbr0_el1);
	set_arch_unknown(el2ctx->ttbr1_el1);
	set_arch_unknown(el2ctx->vbar_el1);
	set_arch_unknown(el2ctx->spsr_el1);

	set_arch_unknown(el2ctx->dbgbcr_el1);
	set_arch_unknown(el2ctx->dbgbvr_el1);
	set_arch_unknown(el2ctx->dbgwcr_el1);
	set_arch_unknown(el2ctx->dbgwvr_el1);

	el2ctx->pmcr_el0 = READ_SPECIALREG(pmcr_el0) & PMCR_N_MASK;
	/* PMCR_LC is unknown when AArch32 is supported or RES1 otherwise */
	el2ctx->pmcr_el0 |= PMCR_LC;
	set_arch_unknown(el2ctx->pmccntr_el0);
	set_arch_unknown(el2ctx->pmccfiltr_el0);
	set_arch_unknown(el2ctx->pmuserenr_el0);
	set_arch_unknown(el2ctx->pmselr_el0);
	set_arch_unknown(el2ctx->pmxevcntr_el0);
	set_arch_unknown(el2ctx->pmcntenset_el0);
	set_arch_unknown(el2ctx->pmintenset_el1);
	set_arch_unknown(el2ctx->pmovsset_el0);
	memset(el2ctx->pmevcntr_el0, 0, sizeof(el2ctx->pmevcntr_el0));
	memset(el2ctx->pmevtyper_el0, 0, sizeof(el2ctx->pmevtyper_el0));
}

void
reset_vm_el2_regs(void *vcpu)
{
	struct hypctx *el2ctx;
	uint64_t cpu_aff, vcpuid;

	el2ctx = vcpu;
	vcpuid = vcpu_vcpuid(el2ctx->vcpu);

	/*
	 * Set the Hypervisor Configuration Register:
	 *
	 * HCR_RW: use AArch64 for EL1
	 * HCR_TID3: handle ID registers in the vmm to privide a common
	 * set of featers on all vcpus
	 * HCR_TWI: Trap WFI to the hypervisor
	 * HCR_BSU_IS: barrier instructions apply to the inner shareable
	 * domain
	 * HCR_FB: broadcast maintenance operations
	 * HCR_AMO: route physical SError interrupts to EL2
	 * HCR_IMO: route physical IRQ interrupts to EL2
	 * HCR_FMO: route physical FIQ interrupts to EL2
	 * HCR_SWIO: turn set/way invalidate into set/way clean and
	 * invalidate
	 * HCR_VM: use stage 2 translation
	 */
	el2ctx->hcr_el2 = HCR_RW | HCR_TID3 | HCR_TWI | HCR_BSU_IS | HCR_FB |
	    HCR_AMO | HCR_IMO | HCR_FMO | HCR_SWIO | HCR_VM;
	if (in_vhe()) {
		el2ctx->hcr_el2 |= HCR_E2H;
	}

	/* Set the Extended Hypervisor Configuration Register */
	el2ctx->hcrx_el2 = 0;
	/* TODO: Trap all extensions we don't support */
	el2ctx->mdcr_el2 = MDCR_EL2_TDOSA | MDCR_EL2_TDRA | MDCR_EL2_TPMS |
	    MDCR_EL2_TTRF;
	/* PMCR_EL0.N is read from MDCR_EL2.HPMN */
	el2ctx->mdcr_el2 |= (el2ctx->pmcr_el0 & PMCR_N_MASK) >> PMCR_N_SHIFT;

	el2ctx->vmpidr_el2 = VMPIDR_EL2_RES1;
	/* The guest will detect a multi-core, single-threaded CPU */
	el2ctx->vmpidr_el2 &= ~VMPIDR_EL2_U & ~VMPIDR_EL2_MT;
	/*
	 * Generate the guest MPIDR value. We only support 16 CPUs at affinity
	 * level 0 to simplify the vgicv3 driver (see writing sgi1r_el1).
	 */
	cpu_aff = (vcpuid & 0xf) << MPIDR_AFF0_SHIFT |
	    ((vcpuid >> 4) & 0xff) << MPIDR_AFF1_SHIFT |
	    ((vcpuid >> 12) & 0xff) << MPIDR_AFF2_SHIFT |
	    ((vcpuid >> 20) & 0xff) << MPIDR_AFF3_SHIFT;
	el2ctx->vmpidr_el2 |= cpu_aff;

	/* Use the same CPU identification information as the host */
	el2ctx->vpidr_el2 = CPU_IMPL_TO_MIDR(CPU_IMPL_ARM);
	el2ctx->vpidr_el2 |= CPU_VAR_TO_MIDR(0);
	el2ctx->vpidr_el2 |= CPU_ARCH_TO_MIDR(0xf);
	el2ctx->vpidr_el2 |= CPU_PART_TO_MIDR(CPU_PART_FOUNDATION);
	el2ctx->vpidr_el2 |= CPU_REV_TO_MIDR(0);

	/*
	 * Don't trap accesses to CPACR_EL1, trace, SVE, Advanced SIMD
	 * and floating point functionality to EL2.
	 */
	if (in_vhe())
		el2ctx->cptr_el2 = CPTR_E2H_TRAP_ALL | CPTR_E2H_FPEN;
	else
		el2ctx->cptr_el2 = CPTR_TRAP_ALL & ~CPTR_TFP;
	el2ctx->cptr_el2 &= ~CPTR_TCPAC;
	/*
	 * Disable interrupts in the guest. The guest OS will re-enable
	 * them.
	 */
	el2ctx->tf.tf_spsr = PSR_D | PSR_A | PSR_I | PSR_F;
	/* Use the EL1 stack when taking exceptions to EL1 */
	el2ctx->tf.tf_spsr |= PSR_M_EL1h;
}
