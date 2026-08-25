/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Andrew Turner
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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
#include <sys/types.h>
#include <sys/proc.h>


#include "arm64.h"
#include "hyp.h"

struct hypctx;

uint64_t VMM_HYP_FUNC(do_call_guest)(struct hypctx *);

static void
vmm_hyp_reg_store_pmu_debug(struct hypctx *hypctx, bool guest)
{
	uint64_t dfr0;
	hypctx_write_sys_reg(hypctx, DBGCLAIMSET_EL1,
	    READ_SPECIALREG(dbgclaimset_el1));

	dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	switch (ID_AA64DFR0_BRPs_VAL(dfr0) - 1) {
#define	STORE_DBG_BRP(x)					\
	case x:							\
		hypctx_write_sys_reg(hypctx, DBGBCR_EL1(x),	\
		    READ_SPECIALREG(dbgbcr ## x ## _el1));	\
		hypctx_write_sys_reg(hypctx, DBGBVR_EL1(x),	\
		    READ_SPECIALREG(dbgbvr ## x ## _el1))
	STORE_DBG_BRP(15);
	STORE_DBG_BRP(14);
	STORE_DBG_BRP(13);
	STORE_DBG_BRP(12);
	STORE_DBG_BRP(11);
	STORE_DBG_BRP(10);
	STORE_DBG_BRP(9);
	STORE_DBG_BRP(8);
	STORE_DBG_BRP(7);
	STORE_DBG_BRP(6);
	STORE_DBG_BRP(5);
	STORE_DBG_BRP(4);
	STORE_DBG_BRP(3);
	STORE_DBG_BRP(2);
	STORE_DBG_BRP(1);
	default:
	STORE_DBG_BRP(0);
#undef STORE_DBG_BRP
	}

	switch (ID_AA64DFR0_WRPs_VAL(dfr0) - 1) {
#define	STORE_DBG_WRP(x)					\
	case x:							\
		hypctx_write_sys_reg(hypctx, DBGWCR_EL1(x),	\
		    READ_SPECIALREG(dbgwcr ## x ## _el1));	\
		hypctx_write_sys_reg(hypctx, DBGWVR_EL1(x),	\
		    READ_SPECIALREG(dbgwvr ## x ## _el1))
	STORE_DBG_WRP(15);
	STORE_DBG_WRP(14);
	STORE_DBG_WRP(13);
	STORE_DBG_WRP(12);
	STORE_DBG_WRP(11);
	STORE_DBG_WRP(10);
	STORE_DBG_WRP(9);
	STORE_DBG_WRP(8);
	STORE_DBG_WRP(7);
	STORE_DBG_WRP(6);
	STORE_DBG_WRP(5);
	STORE_DBG_WRP(4);
	STORE_DBG_WRP(3);
	STORE_DBG_WRP(2);
	STORE_DBG_WRP(1);
	default:
	STORE_DBG_WRP(0);
#undef STORE_DBG_WRP
	}

	/* Store the PMU registers */
	hypctx_write_sys_reg(hypctx, PMCR_EL0,
	    READ_SPECIALREG(pmcr_el0));
	hypctx_write_sys_reg(hypctx, PMCCNTR_EL0,
	    READ_SPECIALREG(pmccntr_el0));
	hypctx_write_sys_reg(hypctx, PMCCFILTR_EL0,
	    READ_SPECIALREG(pmccfiltr_el0));
	hypctx_write_sys_reg(hypctx, PMUSERENR_EL0,
	    READ_SPECIALREG(pmuserenr_el0));
	hypctx_write_sys_reg(hypctx, PMSELR_EL0,
	    READ_SPECIALREG(pmselr_el0));
	hypctx_write_sys_reg(hypctx, PMXEVCNTR_EL0,
	    READ_SPECIALREG(pmxevcntr_el0));
	hypctx_write_sys_reg(hypctx, PMCNTENSET_EL0,
	    READ_SPECIALREG(pmcntenset_el0));
	hypctx_write_sys_reg(hypctx, PMINTENSET_EL1,
	    READ_SPECIALREG(pmintenset_el1));
	hypctx_write_sys_reg(hypctx, PMOVSSET_EL0,
	    READ_SPECIALREG(pmovsset_el0));

	switch ((hypctx_read_sys_reg(hypctx, PMCR_EL0) & PMCR_N_MASK) >>
			PMCR_N_SHIFT) {
#define	STORE_PMU(x)							\
	case (x + 1):							\
		hypctx_write_sys_reg(hypctx, PMEVCNTR_EL0(x),		\
		    READ_SPECIALREG(pmevcntr ## x ## _el0));		\
		hypctx_write_sys_reg(hypctx, PMEVTYPER_EL0(x),		\
		    READ_SPECIALREG(pmevtyper ## x ## _el0))
	STORE_PMU(30);
	STORE_PMU(29);
	STORE_PMU(28);
	STORE_PMU(27);
	STORE_PMU(26);
	STORE_PMU(25);
	STORE_PMU(24);
	STORE_PMU(23);
	STORE_PMU(22);
	STORE_PMU(21);
	STORE_PMU(20);
	STORE_PMU(19);
	STORE_PMU(18);
	STORE_PMU(17);
	STORE_PMU(16);
	STORE_PMU(15);
	STORE_PMU(14);
	STORE_PMU(13);
	STORE_PMU(12);
	STORE_PMU(11);
	STORE_PMU(10);
	STORE_PMU(9);
	STORE_PMU(8);
	STORE_PMU(7);
	STORE_PMU(6);
	STORE_PMU(5);
	STORE_PMU(4);
	STORE_PMU(3);
	STORE_PMU(2);
	STORE_PMU(1);
	STORE_PMU(0);
	default:		/* N == 0 when only PMCCNTR_EL0 is available */
		break;
#undef STORE_PMU
	}

	if (!guest)
		hypctx_write_sys_reg(hypctx, HOST_MDCR_EL2,
		    READ_SPECIALREG(mdcr_el2));
}

static void
vmm_hyp_reg_store_vgic(struct hypctx *hypctx, bool guest)
{
	hypctx_write_sys_reg(hypctx, HOST_ICH_HCR_EL2,
	    READ_SPECIALREG(ich_hcr_el2));
	hypctx_write_sys_reg(hypctx, HOST_ICH_VMCR_EL2,
	    READ_SPECIALREG(ich_vmcr_el2));

	 if (!guest)
		 return;

	/* Store the GICv3 registers */
	 hypctx_write_sys_reg(hypctx, HOST_ICH_EISR_EL2,
	    READ_SPECIALREG(ich_eisr_el2));
	 hypctx_write_sys_reg(hypctx, HOST_ICH_ELRSR_EL2,
	    READ_SPECIALREG(ich_elrsr_el2));
	 hypctx_write_sys_reg(hypctx, HOST_ICH_MISR_EL2,
	    READ_SPECIALREG(ich_misr_el2));

	switch (hypctx->vgic_v3.ich_lr_num - 1) {
#define	STORE_LR(x)						\
case x:								\
	hypctx_write_sys_reg(hypctx, HOST_ICH_LR_EL2(x),	\
	    READ_SPECIALREG(ich_lr ## x ##_el2))
	STORE_LR(15);
	STORE_LR(14);
	STORE_LR(13);
	STORE_LR(12);
	STORE_LR(11);
	STORE_LR(10);
	STORE_LR(9);
	STORE_LR(8);
	STORE_LR(7);
	STORE_LR(6);
	STORE_LR(5);
	STORE_LR(4);
	STORE_LR(3);
	STORE_LR(2);
	STORE_LR(1);
	default:
	STORE_LR(0);
#undef STORE_LR
	}

	switch (hypctx->vgic_v3.ich_apr_num - 1) {
#define	STORE_APR(x)						\
case x:								\
	hypctx_write_sys_reg(hypctx, HOST_ICH_AP0R_EL2(x),	\
	    READ_SPECIALREG(ich_ap0r ## x ##_el2));		\
	hypctx_write_sys_reg(hypctx, HOST_ICH_AP1R_EL2(x),	\
	    READ_SPECIALREG(ich_ap1r ## x ##_el2))
	STORE_APR(3);
	STORE_APR(2);
	STORE_APR(1);
	default:
	STORE_APR(0);
#undef STORE_APR
	}
}

static void
vmm_hyp_reg_store_timer(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	bool ecv_poff;

	if (guest) {
		/* Store the timer registers */
		hypctx->vtimer_cpu.cntkctl_el1 =
		    READ_SPECIALREG(EL1_REG(CNTKCTL));
		hypctx->vtimer_cpu.virt_timer.cntx_cval_el0 =
		    READ_SPECIALREG(EL0_REG(CNTV_CVAL));
		hypctx->vtimer_cpu.virt_timer.cntx_ctl_el0 =
		    READ_SPECIALREG(EL0_REG(CNTV_CTL));
	} else {
		hypctx->vtimer_cpu.cntkctl_el1 =
		    READ_SPECIALREG(cntkctl_el1);
		hypctx_write_sys_reg(hypctx, HOST_CNTHCTL_EL2,
		    READ_SPECIALREG(cnthctl_el2));
		hypctx_write_sys_reg(hypctx, HOST_CNTVOFF_EL2,
		    READ_SPECIALREG(cntvoff_el2));
	}

	ecv_poff = (hypctx_read_sys_reg(hypctx, HOST_CNTHCTL_EL2) &
	    CNTHCTL_ECV_EN) != 0;

	if (guest_or_nonvhe(guest) && ecv_poff) {
		/*
		 * If we have ECV then the guest could modify these registers.
		 * If VHE is enabled then the kernel will see a different view
		 * of the registers, so doesn't need to handle them.
		 */
		hypctx->vtimer_cpu.phys_timer.cntx_cval_el0 =
		    READ_SPECIALREG(EL0_REG(CNTP_CVAL));
		hypctx->vtimer_cpu.phys_timer.cntx_ctl_el0 =
		    READ_SPECIALREG(EL0_REG(CNTP_CTL));
	}
}

static void
vmm_hyp_reg_store_special(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	/* Store the special to from the trapframe */
	hypctx_write_sys_reg(hypctx, HOST_SP_EL1, READ_SPECIALREG(sp_el1));
	hypctx_write_sys_reg(hypctx, HOST_ELR_EL2, READ_SPECIALREG(elr_el2));
	hypctx_write_sys_reg(hypctx, HOST_SPSR_EL2, READ_SPECIALREG(spsr_el2));

	if (guest) {
		hypctx_write_sys_reg(hypctx, HOST_ESR_EL2,
		    READ_SPECIALREG(esr_el2));
		hypctx_write_sys_reg(hypctx, HOST_FAR_EL2,
		    READ_SPECIALREG(far_el2));
		hypctx_write_sys_reg(hypctx, PAR_EL1,
		    READ_SPECIALREG(par_el1));
	}

	/* Store the guest special registers */
	hypctx_write_sys_reg(hypctx, SP_EL0, READ_SPECIALREG(sp_el0));
	hypctx_write_sys_reg(hypctx, TPIDR_EL0, READ_SPECIALREG(tpidr_el0));
	hypctx_write_sys_reg(hypctx, TPIDRRO_EL0, READ_SPECIALREG(tpidrro_el0));
	hypctx_write_sys_reg(hypctx, TPIDR_EL1, READ_SPECIALREG(tpidr_el1));

	hypctx_write_sys_reg(hypctx, ACTLR_EL1, READ_SPECIALREG(actlr_el1));
	hypctx_write_sys_reg(hypctx, CSSELR_EL1, READ_SPECIALREG(csselr_el1));
	hypctx_write_sys_reg(hypctx, MDCCINT_EL1, READ_SPECIALREG(mdccint_el1));
	hypctx_write_sys_reg(hypctx, MDSCR_EL1, READ_SPECIALREG(mdscr_el1));

	if (guest_or_nonvhe(guest)) {
		hypctx_write_sys_reg(hypctx, ELR_EL1,
		    READ_SPECIALREG(EL1_REG(ELR)));
		hypctx_write_sys_reg(hypctx, VBAR_EL1,
		    READ_SPECIALREG(EL1_REG(VBAR)));
		hypctx_write_sys_reg(hypctx, AFSR0_EL1,
		    READ_SPECIALREG(EL1_REG(AFSR0)));
		hypctx_write_sys_reg(hypctx, AFSR1_EL1,
		    READ_SPECIALREG(EL1_REG(AFSR1)));
		hypctx_write_sys_reg(hypctx, AMAIR_EL1,
		    READ_SPECIALREG(EL1_REG(AMAIR)));
		hypctx_write_sys_reg(hypctx, CONTEXTIDR_EL1,
		    READ_SPECIALREG(EL1_REG(CONTEXTIDR)));
		hypctx_write_sys_reg(hypctx, CPACR_EL1,
		    READ_SPECIALREG(EL1_REG(CPACR)));
		hypctx_write_sys_reg(hypctx, ESR_EL1,
		    READ_SPECIALREG(EL1_REG(ESR)));
		hypctx_write_sys_reg(hypctx, FAR_EL1,
		    READ_SPECIALREG(EL1_REG(FAR)));
		hypctx_write_sys_reg(hypctx, MAIR_EL1,
		    READ_SPECIALREG(EL1_REG(MAIR)));
		hypctx_write_sys_reg(hypctx, SCTLR_EL1,
		    READ_SPECIALREG(EL1_REG(SCTLR)));
		hypctx_write_sys_reg(hypctx, SPSR_EL1,
		    READ_SPECIALREG(EL1_REG(SPSR)));
		hypctx_write_sys_reg(hypctx, TCR_EL1,
		    READ_SPECIALREG(EL1_REG(TCR)));
		/* TODO: Support when this is not res0 */
		hypctx_write_sys_reg(hypctx, TCR2_EL1, 0);
		hypctx_write_sys_reg(hypctx, TTBR0_EL1,
		    READ_SPECIALREG(EL1_REG(TTBR0)));
		hypctx_write_sys_reg(hypctx, TTBR1_EL1,
		    READ_SPECIALREG(EL1_REG(TTBR1)));
	}

	hypctx_write_sys_reg(hypctx, HOST_CPTR_EL2,
	    READ_SPECIALREG(cptr_el2));
	hypctx_write_sys_reg(hypctx, HOST_HCR_EL2,
	    READ_SPECIALREG(hcr_el2));
	hypctx_write_sys_reg(hypctx, HOST_VPIDR_EL2,
	    READ_SPECIALREG(vpidr_el2));
	hypctx_write_sys_reg(hypctx, HOST_VMPIDR_EL2,
	    READ_SPECIALREG(vmpidr_el2));

#ifndef VMM_VHE
	if (!guest && ((hyp->feats & HYP_FEAT_HCX) != 0)) {
		hypctx_write_sys_reg(hypctx, HOST_HCRX_EL2,
		    READ_SPECIALREG(MRS_REG_ALT_NAME(HCRX_EL2)));
	}
#endif
}

static void
vmm_hyp_reg_restore_special(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	WRITE_SPECIALREG(hcr_el2, hypctx_read_sys_reg(hypctx, HOST_HCR_EL2));

	if (guest_or_nonvhe(guest) && ((hyp->feats & HYP_FEAT_HCX) != 0)) {
		WRITE_SPECIALREG(HCRX_EL2_REG,
		    hypctx_read_sys_reg(hypctx, HOST_HCRX_EL2));
	}

	isb();

#ifdef VMM_VHE
	if (guest) {
		/* Fine-grained trap controls */
		if ((hyp->feats & HYP_FEAT_FGT) != 0) {
			WRITE_SPECIALREG(HDFGWTR_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HDFGWTR_EL2));
			WRITE_SPECIALREG(HFGITR_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGITR_EL2));
			WRITE_SPECIALREG(HFGRTR_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGRTR_EL2));
			WRITE_SPECIALREG(HFGWTR_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGWTR_EL2));
		}

		if ((hyp->feats & HYP_FEAT_FGT2) != 0) {
			WRITE_SPECIALREG(HDFGRTR2_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HDFGRTR2_EL2));
			WRITE_SPECIALREG(HDFGWTR2_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HDFGWTR2_EL2));
			WRITE_SPECIALREG(HFGITR2_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGITR2_EL2));
			WRITE_SPECIALREG(HFGRTR2_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGRTR2_EL2));
			WRITE_SPECIALREG(HFGWTR2_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_HFGWTR2_EL2));
		}
	}
#endif

	WRITE_SPECIALREG(sp_el0, hypctx_read_sys_reg(hypctx, SP_EL0));
	WRITE_SPECIALREG(tpidr_el0, hypctx_read_sys_reg(hypctx, TPIDR_EL0));
	WRITE_SPECIALREG(tpidrro_el0, hypctx_read_sys_reg(hypctx, TPIDRRO_EL0));
	WRITE_SPECIALREG(tpidr_el1, hypctx_read_sys_reg(hypctx, TPIDR_EL1));

	WRITE_SPECIALREG(actlr_el1, hypctx_read_sys_reg(hypctx, ACTLR_EL1));
	WRITE_SPECIALREG(csselr_el1, hypctx_read_sys_reg(hypctx, CSSELR_EL1));
	WRITE_SPECIALREG(mdccint_el1, hypctx_read_sys_reg(hypctx, MDCCINT_EL1));
	WRITE_SPECIALREG(mdscr_el1, hypctx_read_sys_reg(hypctx, MDSCR_EL1));

	if (guest_or_nonvhe(guest)) {
		WRITE_SPECIALREG(EL1_REG(ELR),
		    hypctx_read_sys_reg(hypctx, ELR_EL1));
		WRITE_SPECIALREG(EL1_REG(VBAR),
		    hypctx_read_sys_reg(hypctx, VBAR_EL1));
		WRITE_SPECIALREG(EL1_REG(AFSR0),
		    hypctx_read_sys_reg(hypctx, AFSR0_EL1));
		WRITE_SPECIALREG(EL1_REG(AFSR1),
		    hypctx_read_sys_reg(hypctx, AFSR1_EL1));
		WRITE_SPECIALREG(EL1_REG(AMAIR),
		    hypctx_read_sys_reg(hypctx, AMAIR_EL1));
		WRITE_SPECIALREG(EL1_REG(CONTEXTIDR),
		    hypctx_read_sys_reg(hypctx, CONTEXTIDR_EL1));
		WRITE_SPECIALREG(EL1_REG(CPACR),
		    hypctx_read_sys_reg(hypctx, CPACR_EL1));
		WRITE_SPECIALREG(EL1_REG(ESR),
		    hypctx_read_sys_reg(hypctx, ESR_EL1));
		WRITE_SPECIALREG(EL1_REG(FAR),
		    hypctx_read_sys_reg(hypctx, FAR_EL1));
		WRITE_SPECIALREG(EL1_REG(MAIR),
		    hypctx_read_sys_reg(hypctx, MAIR_EL1));
		WRITE_SPECIALREG(EL1_REG(SCTLR),
		    hypctx_read_sys_reg(hypctx, SCTLR_EL1));
		WRITE_SPECIALREG(EL1_REG(SPSR),
		    hypctx_read_sys_reg(hypctx, SPSR_EL1));
		WRITE_SPECIALREG(EL1_REG(TCR),
		    hypctx_read_sys_reg(hypctx, TCR_EL1));
		/* TODO: tcr2_el1 */
		WRITE_SPECIALREG(EL1_REG(TTBR0),
		    hypctx_read_sys_reg(hypctx, TTBR0_EL1));
		WRITE_SPECIALREG(EL1_REG(TTBR1),
		    hypctx_read_sys_reg(hypctx, TTBR1_EL1));
	}

	if (guest) {
		WRITE_SPECIALREG(par_el1, hypctx_read_sys_reg(hypctx, PAR_EL1));
	}

	WRITE_SPECIALREG(cptr_el2,
	    hypctx_read_sys_reg(hypctx, HOST_CPTR_EL2));
	WRITE_SPECIALREG(vpidr_el2,
	    hypctx_read_sys_reg(hypctx, HOST_VPIDR_EL2));
	WRITE_SPECIALREG(vmpidr_el2,
	    hypctx_read_sys_reg(hypctx, HOST_VMPIDR_EL2));

	/* Load the special regs from the trapframe */
	WRITE_SPECIALREG(sp_el1, hypctx_read_sys_reg(hypctx, HOST_SP_EL1));
	WRITE_SPECIALREG(elr_el2, hypctx_read_sys_reg(hypctx, HOST_ELR_EL2));
	WRITE_SPECIALREG(spsr_el2, hypctx_read_sys_reg(hypctx, HOST_SPSR_EL2));
}

static void
vmm_hyp_reg_restore_pmu_debug(struct hypctx *hypctx)
{
	uint64_t dfr0;

	/* Restore the PMU registers */
	WRITE_SPECIALREG(pmcr_el0,
	    hypctx_read_sys_reg(hypctx, PMCR_EL0));
	WRITE_SPECIALREG(pmccntr_el0,
	    hypctx_read_sys_reg(hypctx, PMCCNTR_EL0));
	WRITE_SPECIALREG(pmccfiltr_el0,
	    hypctx_read_sys_reg(hypctx, PMCCFILTR_EL0));
	WRITE_SPECIALREG(pmuserenr_el0,
	    hypctx_read_sys_reg(hypctx, PMUSERENR_EL0));
	WRITE_SPECIALREG(pmselr_el0,
	    hypctx_read_sys_reg(hypctx, PMSELR_EL0));
	WRITE_SPECIALREG(pmxevcntr_el0,
	    hypctx_read_sys_reg(hypctx, PMXEVCNTR_EL0));
	/* Clear all events/interrupts then enable them */
	WRITE_SPECIALREG(pmcntenclr_el0, ~0ul);
	WRITE_SPECIALREG(pmcntenset_el0,
	    hypctx_read_sys_reg(hypctx, PMCNTENSET_EL0));
	WRITE_SPECIALREG(pmintenclr_el1, ~0ul);
	WRITE_SPECIALREG(pmintenset_el1,
	    hypctx_read_sys_reg(hypctx, PMINTENSET_EL1));
	WRITE_SPECIALREG(pmovsclr_el0, ~0ul);
	WRITE_SPECIALREG(pmovsset_el0,
	    hypctx_read_sys_reg(hypctx, PMOVSSET_EL0));

	switch ((hypctx_read_sys_reg(hypctx, PMCR_EL0) & PMCR_N_MASK) >>
		PMCR_N_SHIFT) {
#define	LOAD_PMU(x)							\
	case (x + 1):							\
		WRITE_SPECIALREG(pmevcntr ## x ## _el0,			\
		    hypctx_read_sys_reg(hypctx, PMEVCNTR_EL0(x)));	\
		WRITE_SPECIALREG(pmevtyper ## x ## _el0,		\
		    hypctx_read_sys_reg(hypctx, PMEVTYPER_EL0(x)))
	LOAD_PMU(30);
	LOAD_PMU(29);
	LOAD_PMU(28);
	LOAD_PMU(27);
	LOAD_PMU(26);
	LOAD_PMU(25);
	LOAD_PMU(24);
	LOAD_PMU(23);
	LOAD_PMU(22);
	LOAD_PMU(21);
	LOAD_PMU(20);
	LOAD_PMU(19);
	LOAD_PMU(18);
	LOAD_PMU(17);
	LOAD_PMU(16);
	LOAD_PMU(15);
	LOAD_PMU(14);
	LOAD_PMU(13);
	LOAD_PMU(12);
	LOAD_PMU(11);
	LOAD_PMU(10);
	LOAD_PMU(9);
	LOAD_PMU(8);
	LOAD_PMU(7);
	LOAD_PMU(6);
	LOAD_PMU(5);
	LOAD_PMU(4);
	LOAD_PMU(3);
	LOAD_PMU(2);
	LOAD_PMU(1);
	LOAD_PMU(0);
	default:		/* N == 0 when only PMCCNTR_EL0 is available */
		break;
#undef LOAD_PMU
	}

	WRITE_SPECIALREG(dbgclaimclr_el1, ~0ul);
	WRITE_SPECIALREG(dbgclaimclr_el1,
	    hypctx_read_sys_reg(hypctx, DBGCLAIMSET_EL1));

	dfr0 = READ_SPECIALREG(id_aa64dfr0_el1);
	switch (ID_AA64DFR0_BRPs_VAL(dfr0) - 1) {
#define	LOAD_DBG_BRP(x)							\
	case x:								\
		WRITE_SPECIALREG(dbgbcr ## x ## _el1,			\
		    hypctx_read_sys_reg(hypctx, DBGBCR_EL1(x)));	\
		WRITE_SPECIALREG(dbgbvr ## x ## _el1,			\
		    hypctx_read_sys_reg(hypctx, DBGBVR_EL1(x)))
	LOAD_DBG_BRP(15);
	LOAD_DBG_BRP(14);
	LOAD_DBG_BRP(13);
	LOAD_DBG_BRP(12);
	LOAD_DBG_BRP(11);
	LOAD_DBG_BRP(10);
	LOAD_DBG_BRP(9);
	LOAD_DBG_BRP(8);
	LOAD_DBG_BRP(7);
	LOAD_DBG_BRP(6);
	LOAD_DBG_BRP(5);
	LOAD_DBG_BRP(4);
	LOAD_DBG_BRP(3);
	LOAD_DBG_BRP(2);
	LOAD_DBG_BRP(1);
	default:
	LOAD_DBG_BRP(0);
#undef LOAD_DBG_BRP
	}

	switch (ID_AA64DFR0_WRPs_VAL(dfr0) - 1) {
#define	LOAD_DBG_WRP(x)							\
	case x:								\
		WRITE_SPECIALREG(dbgwcr ## x ## _el1,			\
		    hypctx_read_sys_reg(hypctx, DBGWCR_EL1(x)));	\
		WRITE_SPECIALREG(dbgwvr ## x ## _el1,			\
		    hypctx_read_sys_reg(hypctx, DBGWVR_EL1(x)))
	LOAD_DBG_WRP(15);
	LOAD_DBG_WRP(14);
	LOAD_DBG_WRP(13);
	LOAD_DBG_WRP(12);
	LOAD_DBG_WRP(11);
	LOAD_DBG_WRP(10);
	LOAD_DBG_WRP(9);
	LOAD_DBG_WRP(8);
	LOAD_DBG_WRP(7);
	LOAD_DBG_WRP(6);
	LOAD_DBG_WRP(5);
	LOAD_DBG_WRP(4);
	LOAD_DBG_WRP(3);
	LOAD_DBG_WRP(2);
	LOAD_DBG_WRP(1);
	default:
	LOAD_DBG_WRP(0);
#undef LOAD_DBG_WRP
	}
}

static void
vmm_hyp_reg_restore_timer(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	bool ecv_poff;
	ecv_poff = (hypctx_read_sys_reg(hypctx, HOST_CNTHCTL_EL2) &
	    CNTHCTL_ECV_EN) != 0;

	WRITE_SPECIALREG(cnthctl_el2,
	    hypctx_read_sys_reg(hypctx, HOST_CNTHCTL_EL2));
	WRITE_SPECIALREG(cntvoff_el2,
	    hypctx_read_sys_reg(hypctx, HOST_CNTVOFF_EL2));

	if (guest) {
		WRITE_SPECIALREG(EL1_REG(CNTKCTL),
		    hypctx->vtimer_cpu.cntkctl_el1);
		WRITE_SPECIALREG(EL0_REG(CNTV_CVAL),
		    hypctx->vtimer_cpu.virt_timer.cntx_cval_el0);
		WRITE_SPECIALREG(EL0_REG(CNTV_CTL),
		    hypctx->vtimer_cpu.virt_timer.cntx_ctl_el0);

		if (ecv_poff) {
			/*
			 * Load the same offset as the virtual timer
			 * to keep in sync.
			 */
			WRITE_SPECIALREG(CNTPOFF_EL2_REG,
			    hypctx_read_sys_reg(hypctx, HOST_CNTVOFF_EL2));
			isb();
		}
	} else {
		WRITE_SPECIALREG(cntkctl_el1, hypctx->vtimer_cpu.cntkctl_el1);
	}
	if (guest_or_nonvhe(guest) && ecv_poff) {
		/*
		 * If we have ECV then the guest could modify these registers.
		 * If VHE is enabled then the kernel will see a different view
		 * of the registers, so doesn't need to handle them.
		 */
		WRITE_SPECIALREG(EL0_REG(CNTP_CVAL),
		    hypctx->vtimer_cpu.phys_timer.cntx_cval_el0);
		WRITE_SPECIALREG(EL0_REG(CNTP_CTL),
		    hypctx->vtimer_cpu.phys_timer.cntx_ctl_el0);
	}
}

static void
vmm_hyp_reg_restore_vgic(struct hypctx *hypctx, bool guest)
{
	/* Load the GICv3 registers */
	WRITE_SPECIALREG(ich_hcr_el2,
	    hypctx_read_sys_reg(hypctx, HOST_ICH_HCR_EL2));
	WRITE_SPECIALREG(ich_vmcr_el2,
	    hypctx_read_sys_reg(hypctx, HOST_ICH_VMCR_EL2));

	if (!guest)
		return;

	switch (hypctx->vgic_v3.ich_lr_num - 1) {
#define	LOAD_LR(x)							\
case x:									\
	WRITE_SPECIALREG(ich_lr ## x ##_el2,				\
	    hypctx_read_sys_reg(hypctx, HOST_ICH_LR_EL2(x)))
	LOAD_LR(15);
	LOAD_LR(14);
	LOAD_LR(13);
	LOAD_LR(12);
	LOAD_LR(11);
	LOAD_LR(10);
	LOAD_LR(9);
	LOAD_LR(8);
	LOAD_LR(7);
	LOAD_LR(6);
	LOAD_LR(5);
	LOAD_LR(4);
	LOAD_LR(3);
	LOAD_LR(2);
	LOAD_LR(1);
	default:
	LOAD_LR(0);
#undef LOAD_LR
	}

	switch (hypctx->vgic_v3.ich_apr_num - 1) {
#define	LOAD_APR(x)							\
case x:									\
	WRITE_SPECIALREG(ich_ap0r ## x ##_el2,				\
	    hypctx_read_sys_reg(hypctx, HOST_ICH_AP0R_EL2(x)));	\
	WRITE_SPECIALREG(ich_ap1r ## x ##_el2,				\
	    hypctx_read_sys_reg(hypctx, HOST_ICH_AP1R_EL2(x)))
	LOAD_APR(3);
	LOAD_APR(2);
	LOAD_APR(1);
	default:
	LOAD_APR(0);
#undef LOAD_APR
	}
}

static void
vmm_hyp_reg_store(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	vmm_hyp_reg_store_vgic(hypctx, guest);
	vmm_hyp_reg_store_timer(hypctx, hyp, guest);
	vmm_hyp_reg_store_pmu_debug(hypctx, guest);
	vmm_hyp_reg_store_special(hypctx, hyp, guest);
}

static void
vmm_hyp_reg_restore(struct hypctx *hypctx, struct hyp *hyp, bool guest)
{
	vmm_hyp_reg_restore_special(hypctx, hyp, guest);
	vmm_hyp_reg_restore_pmu_debug(hypctx);
	vmm_hyp_reg_restore_timer(hypctx, hyp, guest);
	vmm_hyp_reg_restore_vgic(hypctx, guest);
}

static void
vmm_hyp_handle_guest_exit(struct hypctx *hypctx, uint64_t *ret)
{
	bool hpfar_valid;
	uint64_t s1e1r, hpfar_el2, host_esr_el2, host_far_el2;

	hpfar_valid = true;
	host_esr_el2 = hypctx_read_sys_reg(hypctx, HOST_ESR_EL2);
	if (*ret == EXCP_TYPE_EL1_SYNC) {
		switch (ESR_ELx_EXCEPTION(host_esr_el2)) {
		case EXCP_INSN_ABORT_L:
		case EXCP_DATA_ABORT_L:
			/*
			 * The hpfar_el2 register is valid for:
			 *  - Translation and Access faults.
			 *  - Translation, Access, and permission faults on
			 *    the translation table walk on the stage 1 tables.
			 *  - A stage 2 Address size fault.
			 *
			 * As we only need it in the first 2 cases we can just
			 * exclude it on permission faults that are not from
			 * the stage 1 table walk.
			 *
			 * TODO: Add a case for Arm erratum 834220.
			 */
			if ((host_esr_el2 & ISS_DATA_S1PTW) != 0)
				break;
			switch (host_esr_el2 & ISS_DATA_DFSC_MASK) {
			case ISS_DATA_DFSC_PF_L1:
			case ISS_DATA_DFSC_PF_L2:
			case ISS_DATA_DFSC_PF_L3:
				hpfar_valid = false;
				break;
			}
			break;
		}
	}
	if (hpfar_valid) {
		hypctx_write_sys_reg(hypctx, HOST_HPFAR_EL2,
				     READ_SPECIALREG(hpfar_el2));
	} else {
		/*
		 * TODO: There is a risk the at instruction could cause an
		 * exception here. We should handle it & return a failure.
		 */
		host_far_el2 = hypctx_read_sys_reg(hypctx, HOST_FAR_EL2);
		s1e1r = arm64_address_translate_s1e1r(host_far_el2);
		if (PAR_SUCCESS(s1e1r)) {
			hpfar_el2 = (s1e1r & PAR_PA_MASK) >> PAR_PA_SHIFT;
			hpfar_el2 <<= HPFAR_EL2_FIPA_SHIFT;
			hypctx_write_sys_reg(hypctx, HOST_HPFAR_EL2, hpfar_el2);
		} else {
			*ret = EXCP_TYPE_REENTER;
		}
	}
}

/* Minimum guest entry logic that should not be reordered */
static uint64_t
__vmm_hyp_call_guest(struct hypctx *hypctx, struct hypctx *host_hypctx)
{
	uint64_t ret;
	WRITE_SPECIALREG(vttbr_el2, hypctx_read_sys_reg(hypctx, HOST_VTTBR_EL2));
	WRITE_SPECIALREG(mdcr_el2, hypctx_read_sys_reg(hypctx, HOST_MDCR_EL2));
	/* Call into the guest */
	ret = VMM_HYP_FUNC(do_call_guest)(hypctx);
	WRITE_SPECIALREG(mdcr_el2, hypctx_read_sys_reg(host_hypctx, HOST_MDCR_EL2));

	isb();

	return ret;
}

static uint64_t
vmm_hyp_call_guest(struct hyp *hyp, struct hypctx *hypctx)
{
	struct hypctx host_hypctx;
	uint64_t ret;

	/* Allows uniform implementation of guest and host register reloads */
	host_hypctx.vncr_regs = hypctx->host_vncr_regs;
	#ifndef VMM_VHE
		host_hypctx.el2_vncr_addr = hypctx->el2_host_vncr_addr;
	#endif

	vmm_hyp_reg_store(&host_hypctx, hyp, false);
	vmm_hyp_reg_restore(hypctx, hyp, true);

	ret = __vmm_hyp_call_guest(hypctx, &host_hypctx);

	vmm_hyp_reg_store(hypctx, hyp, true);
	vmm_hyp_handle_guest_exit(hypctx, &ret);
	vmm_hyp_reg_restore(&host_hypctx, hyp, false);

	return (ret);
}

VMM_STATIC uint64_t
VMM_HYP_FUNC(enter_guest)(struct hyp *hyp, struct hypctx *hypctx)
{
	uint64_t ret;

	do {
		ret = vmm_hyp_call_guest(hyp, hypctx);
	} while (ret == EXCP_TYPE_REENTER);

	return (ret);
}

VMM_STATIC uint64_t
VMM_HYP_FUNC(read_reg)(uint64_t reg)
{
	switch (reg) {
	case HYP_REG_ICH_VTR:
		return (READ_SPECIALREG(ich_vtr_el2));
	}

	return (0);
}

VMM_STATIC void
VMM_HYP_FUNC(clean_s2_tlbi)(void)
{
	dsb(ishst);
	__asm __volatile("tlbi alle1is");
	dsb(ish);
}

VMM_STATIC void
VMM_HYP_FUNC(s2_tlbi_range)(uint64_t vttbr, vm_offset_t sva, vm_offset_t eva,
    bool final_only)
{
	uint64_t end, r, start;
	uint64_t host_vttbr;
#ifdef VMM_VHE
	uint64_t host_tcr;
#endif

	dsb(ishst);

#define	TLBI_VA_SHIFT			12
#define	TLBI_VA_MASK			((1ul << 44) - 1)
#define	TLBI_VA(addr)			(((addr) >> TLBI_VA_SHIFT) & TLBI_VA_MASK)
#define	TLBI_VA_L3_INCR			(L3_SIZE >> TLBI_VA_SHIFT)

	/* Switch to the guest vttbr */
	/* TODO: Handle Cortex-A57/A72 erratum 131936 */
	host_vttbr = READ_SPECIALREG(vttbr_el2);
	WRITE_SPECIALREG(vttbr_el2, vttbr);
	isb();

#ifdef VMM_VHE
	host_tcr = READ_SPECIALREG(tcr_el2);
	WRITE_SPECIALREG(tcr_el2, host_tcr & ~HCR_TGE);
	isb();
#endif

	/*
	 * The CPU can cache the stage 1 + 2 combination so we need to ensure
	 * the stage 2 is invalidated first, then when this has completed we
	 * invalidate the stage 1 TLB. As we don't know which stage 1 virtual
	 * addresses point at the stage 2 IPA we need to invalidate the entire
	 * stage 1 TLB.
	 */

	start = TLBI_VA(sva);
	end = TLBI_VA(eva);
	for (r = start; r < end; r += TLBI_VA_L3_INCR) {
		/* Invalidate the stage 2 TLB entry */
		if (final_only)
			__asm __volatile("tlbi	ipas2le1is, %0" : : "r"(r));
		else
			__asm __volatile("tlbi	ipas2e1is, %0" : : "r"(r));
	}
	/* Ensure the entry has been invalidated */
	dsb(ish);
	/* Invalidate the stage 1 TLB. */
	__asm __volatile("tlbi vmalle1is");
	dsb(ish);
	isb();

#ifdef VMM_VHE
	WRITE_SPECIALREG(tcr_el2, host_tcr);
	isb();
#endif

	/* Switch back to the host vttbr */
	WRITE_SPECIALREG(vttbr_el2, host_vttbr);
	isb();
}

VMM_STATIC void
VMM_HYP_FUNC(s2_tlbi_all)(uint64_t vttbr)
{
	uint64_t host_vttbr;

	dsb(ishst);

	/* Switch to the guest vttbr */
	/* TODO: Handle Cortex-A57/A72 erratum 131936 */
	host_vttbr = READ_SPECIALREG(vttbr_el2);
	WRITE_SPECIALREG(vttbr_el2, vttbr);
	isb();

	__asm __volatile("tlbi vmalls12e1is");
	dsb(ish);
	isb();

	/* Switch back t othe host vttbr */
	WRITE_SPECIALREG(vttbr_el2, host_vttbr);
	isb();
}
