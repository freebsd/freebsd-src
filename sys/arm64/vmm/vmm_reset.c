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

#include <machine/cpu.h>
#include <machine/hypervisor.h>

#include <dev/vmm/vmm_vm.h>

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

	/* FEAT_FGT traps */
	if ((el2ctx->hyp->feats & HYP_FEAT_FGT) != 0) {
#define	HFGT_TRAP_FIELDS(read, write, read_pfx, write_pfx, name, trap)	\
do {									\
	el2ctx->read |= read_pfx ## _EL2_ ## name ## _ ## trap;		\
	el2ctx->write |= write_pfx ## _EL2_ ## name ## _ ## trap;	\
} while (0)


		/*
		 * Traps for special registers
		 */

		/* Debug registers */
		el2ctx->hdfgrtr_el2 = 0;
		el2ctx->hdfgwtr_el2 = 0;

		/* FEAT_BRBE */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    nBRBDATA, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    nBRBCTL, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_nBRBIDR_TRAP;

		/* FEAT_TRBE */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBTRG_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBSR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBPTR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBMAR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBLIMITR_EL1, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_TRBIDR_EL1_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRBBASER_EL1, TRAP);

		/* FEAT_TRF */
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_TRFCR_EL1_TRAP;

		/* FEAT_ETE */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCVICTLR, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_TRCSTATR_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCSSCSRn, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCSEQSTR, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCPRGCTLR, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_TRCOSLSR_TRAP;
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_TRCOSLAR_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCIMSPECn, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_TRCID_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCCNTVRn, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCCLAIM, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRCAUXCTLR, TRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_TRCAUTHSTATUS_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    TRC, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSLATFR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSIRR_EL1, TRAP);

		/* FEAT_SPE */
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_PMBIDR_EL1_TRAP;
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_PMSIDR_EL1_TRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSICR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSFCR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSEVFR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSCR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMBSR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMBPTR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMBLIMITR_EL1, TRAP);

		/* FEAT_SPE_FnE */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    nPMSNEVFR_EL1, TRAP);

		/* FEAT_PMUv3 */
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_PMCEIDn_EL0_NOTRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMUSERENR_EL0, NOTRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_PMMIR_EL1_NOTRAP;
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_PMCR_EL0_NOTRAP;
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_PMSWINC_EL0_NOTRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMSELR_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMOVS, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMINTEN, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMCNTEN, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMCCNTR_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMCCFILTR_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMEVTYPERn_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    PMEVCNTRn_EL0, NOTRAP);

		/* FEAT_DoubleLock */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    OSDLR_EL1, TRAP);

		/* Base architecture */
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    OSECCR_EL1, NOTRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_OSLSR_EL1_NOTRAP;
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_OSLAR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    DBGPRCR_EL1, NOTRAP);
		el2ctx->hdfgrtr_el2 |= HDFGRTR_EL2_DBGAUTHSTATUS_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    DBGCLAIM, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    MDSCR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    DBGWVRn_EL1, NOTRAP);
		el2ctx->hdfgwtr_el2 |= HDFGWTR_EL2_DBGWCRn_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    DBGBVRn_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hdfgrtr_el2, hdfgwtr_el2, HDFGRTR, HDFGWTR,
		    DBGBCRn_EL1, NOTRAP);


		/* Non-debug special registers */
		el2ctx->hfgrtr_el2 = 0;
		el2ctx->hfgwtr_el2 = 0;

		/* FEAT_AIE */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nAMAIR2_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nMAIR2_EL1, TRAP);

		/* FEAT_S2POE */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nS2POR_EL1, TRAP);

		/* FEAT_S1POE */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nPOR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nPOR_EL0, TRAP);

		/* FEAT_S1PIE */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nPIR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nPIRE0_EL1, TRAP);

		/* FEAT_THE */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nRCWMASK_EL1, TRAP);

		/* FEAT_SME */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nTPIDR2_EL0, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nSMPRI_EL1, TRAP);

		/* FEAT_GCS */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nGCS_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nGCS_EL0, TRAP);

		/* FEAT_LS64_ACCDATA */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    nACCDATA_EL1, TRAP);

		/* FEAT_RASv1p1 */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXPFGCDN_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXPFGCTL_EL1, TRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_ERXPFGF_EL1_TRAP;

		/* FEAT_RAS */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXADDR_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXMISCn_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXSTATUS_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERXCTLR_EL1, TRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_ERXFR_EL1_TRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ERRSELR_EL1, TRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_ERRIDR_EL1_TRAP;

		/* GICv3 */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ICC_IGRPENn_EL1, NOTRAP);

		/* FEAT_LOR */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    LORSA_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    LORN_EL1, TRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_LORID_EL1_TRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    LOREA_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    LORC_EL1, TRAP);

		/* FEAT_PAuth */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    APIBKey, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    APIAKey, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    APGAKey, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    APDBKey, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    APDAKey, TRAP);

		/* Base architecture */
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    VBAR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TTBR1_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TTBR0_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TPIDR_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TPIDRRO_EL0, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TPIDR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    TCR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    SCXTNUM_EL0, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    SCXTNUM_EL1, TRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    SCTLR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_REVIDR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    PAR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_MPIDR_EL1_NOTRAP;
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_MIDR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    MAIR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_ISR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    FAR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    ESR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_DCZID_EL0_NOTRAP;
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_CTR_EL0_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    CSSELR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    CPACR_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    CONTEXTIDR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_CLIDR_EL1_NOTRAP;
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_CCSIDR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    AMAIR_EL1, NOTRAP);
		el2ctx->hfgrtr_el2 |= HFGRTR_EL2_AIDR_EL1_NOTRAP;
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    AFSR1_EL1, NOTRAP);
		HFGT_TRAP_FIELDS(hfgrtr_el2, hfgwtr_el2, HFGRTR, HFGWTR,
		    AFSR0_EL1, NOTRAP);

		/*
		 * Traps for instructions
		 */

		/* Enable all TLBI, cache and AT variants */
		el2ctx->hfgitr_el2 = 0;

		/* FEAT_ATS1A */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_ATS1E1A_TRAP;

		/* FEAT_SPECRES2 */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_COSPRCTX_TRAP;

		/* FEAT_GCS */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_nGCSEPP_TRAP |
		    HFGITR_EL2_nGCSSTR_EL1_TRAP |
		    HFGITR_EL2_nGCSPUSHM_EL1_TRAP;

		/* FEAT_BRBE */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_nBRBIALL_TRAP |
		    HFGITR_EL2_nBRBINJ_TRAP;

		/* FEAT_SPECRES */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_CPPRCTX_TRAP |
		    HFGITR_EL2_DVPRCTX_TRAP |
		    HFGITR_EL2_CFPRCTX_TRAP;

		/* FEAT_TLBIRANGE */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_TLBIRVAALE1_TRAP |
		    HFGITR_EL2_TLBIRVALE1_TRAP |
		    HFGITR_EL2_TLBIRVAAE1_TRAP |
		    HFGITR_EL2_TLBIRVAE1_TRAP |
		    HFGITR_EL2_TLBIRVAALE1IS_TRAP |
		    HFGITR_EL2_TLBIRVALE1IS_TRAP |
		    HFGITR_EL2_TLBIRVAAE1IS_TRAP |
		    HFGITR_EL2_TLBIRVAE1IS_TRAP;

		/* FEAT_TLBIRANGE && FEAT_TLBIOS */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_TLBIRVAALE1OS_TRAP |
		    HFGITR_EL2_TLBIRVALE1OS_TRAP |
		    HFGITR_EL2_TLBIRVAAE1OS_TRAP |
		    HFGITR_EL2_TLBIRVAE1OS_TRAP;

		/* FEAT_TLBIOS */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_TLBIVAALE1OS_TRAP |
		    HFGITR_EL2_TLBIVALE1OS_TRAP |
		    HFGITR_EL2_TLBIVAAE1OS_TRAP |
		    HFGITR_EL2_TLBIASIDE1OS_TRAP |
		    HFGITR_EL2_TLBIVAE1OS_TRAP |
		    HFGITR_EL2_TLBIVMALLE1OS_TRAP;

		/* FEAT_PAN2 */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_ATS1E1WP_TRAP |
		    HFGITR_EL2_ATS1E1RP_TRAP;

		/* FEAT_DPB2 */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_DCCVADP_TRAP;

		/* Base architecture */
		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_DCCVAC_NOTRAP |
		    HFGITR_EL2_SVC_EL1_NOTRAP |
		    HFGITR_EL2_SVC_EL0_NOTRAP |
		    HFGITR_EL2_ERET_NOTRAP;

		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_TLBIVAALE1_NOTRAP |
		    HFGITR_EL2_TLBIVALE1_NOTRAP |
		    HFGITR_EL2_TLBIVAAE1_NOTRAP |
		    HFGITR_EL2_TLBIASIDE1_NOTRAP |
		    HFGITR_EL2_TLBIVAE1_NOTRAP |
		    HFGITR_EL2_TLBIVMALLE1_NOTRAP |
		    HFGITR_EL2_TLBIVAALE1IS_NOTRAP |
		    HFGITR_EL2_TLBIVALE1IS_NOTRAP |
		    HFGITR_EL2_TLBIVAAE1IS_NOTRAP |
		    HFGITR_EL2_TLBIASIDE1IS_NOTRAP |
		    HFGITR_EL2_TLBIVAE1IS_NOTRAP |
		    HFGITR_EL2_TLBIVMALLE1IS_NOTRAP;

		el2ctx->hfgitr_el2 |=
		    HFGITR_EL2_ATS1E0W_NOTRAP |
		    HFGITR_EL2_ATS1E0R_NOTRAP |
		    HFGITR_EL2_ATS1E1W_NOTRAP |
		    HFGITR_EL2_ATS1E1R_NOTRAP |
		    HFGITR_EL2_DCZVA_NOTRAP |
		    HFGITR_EL2_DCCIVAC_NOTRAP |
		    HFGITR_EL2_DCCVAP_NOTRAP |
		    HFGITR_EL2_DCCVAU_NOTRAP |
		    HFGITR_EL2_DCCISW_NOTRAP |
		    HFGITR_EL2_DCCSW_NOTRAP |
		    HFGITR_EL2_DCISW_NOTRAP |
		    HFGITR_EL2_DCIVAC_NOTRAP |
		    HFGITR_EL2_ICIVAU_NOTRAP |
		    HFGITR_EL2_ICIALLU_NOTRAP |
		    HFGITR_EL2_ICIALLUIS_NOTRAP;

	}

	/* FEAT_FGT2 traps */
	if ((el2ctx->hyp->feats & HYP_FEAT_FGT2) != 0) {
		/* Trap everything here until we support the feature */
		el2ctx->hdfgrtr2_el2 = 0;
		el2ctx->hdfgwtr2_el2 = 0;
		el2ctx->hfgitr2_el2 = 0;
		el2ctx->hfgrtr2_el2 = 0;
		el2ctx->hfgwtr2_el2 = 0;
	}
}
