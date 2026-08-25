/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2015 Mihai Carabas <mihai.carabas@gmail.com>
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
#ifndef _VMM_ARM64_H_
#define _VMM_ARM64_H_

#include <machine/reg.h>
#include <machine/hypervisor.h>
#include <machine/pcpu.h>

#include "mmu.h"
#include "io/vgic_v3.h"
#include "io/vtimer.h"

struct vgic_v3;
struct vgic_v3_cpu;

/*
  Encode the index of a given register within VNCR into the enum value.
  We move the indices up by VNCR_START to not interfere with values for
  non-VNCR registers.
*/
#define VNCR_REG(reg) reg = (VNCR_START + VNCR_##reg / 8)
/* Retrieve the VNCR offset from the enum value */
#define REG_VNCR_OFFSET(val) ((val - VNCR_START) * 8)

/* Accessors for indices of PME and DBG registers */
#define PMEVCNTR_EL0(n) (PMEVCNTR0_EL0 + MIN(n, 30))
#define PMEVTYPER_EL0(n) (PMEVTYPER0_EL0 + MIN(n, 30))
#define DBGBCR_EL1(n) (DBGBCR0_EL1 + MIN(n, 15))
#define DBGBVR_EL1(n) (DBGBVR0_EL1 + MIN(n, 15))
#define DBGWCR_EL1(n) (DBGWCR0_EL1 + MIN(n, 15))
#define DBGWVR_EL1(n) (DBGWVR0_EL1 + MIN(n, 15))

#define HOST_ICH_LR_EL2(n) (HOST_ICH_LR0_EL2 + MIN(n, VGIC_ICH_LR_NUM_MAX-1))
#define HOST_ICH_AP0R_EL2(n) (HOST_ICH_AP0R0_EL2 + MIN(n, VGIC_ICH_APR_NUM_MAX-1))
#define HOST_ICH_AP1R_EL2(n) (HOST_ICH_AP1R0_EL2 + MIN(n, VGIC_ICH_APR_NUM_MAX-1))
#define GPR_X(n) (GPR_X0 + MIN(n, 30))

enum hypctx_sysreg {
	HOST_SPSR_EL2,
	HOST_ESR_EL2,
	CSSELR_EL1,  /* Cache Size Selection Register */
	MDCCINT_EL1, /* Monitor DCC Interrupt Enable Register */
	PAR_EL1,     /* Physical Address Register */

	/* PMU Registers */
	PMCR_EL0,	/* Performance Monitors Control Register */
	PMCCNTR_EL0,
	PMCCFILTR_EL0,
	PMUSERENR_EL0,
	PMSELR_EL0,
	PMXEVCNTR_EL0,
	PMCNTENSET_EL0,
	PMINTENSET_EL1,
	PMOVSSET_EL0,
	/* Access these through macros defined above, e.g. PMEVCNTR_EL0(5) */
	PMEVCNTR0_EL0,
	PMEVCNTR30_EL0 = PMEVCNTR0_EL0 + 30,
	PMEVTYPER0_EL0,
	PMEVTYPER30_EL0 = PMEVTYPER0_EL0 + 30,

	/* DBG Registers */
	DBGCLAIMSET_EL1,
	/* Access these through macros defined above, e.g. DBGBCR_EL1(5) */
	DBGBCR0_EL1,	/* Debug Breakpoint Control Registers */
	DBGBCR15_EL1 = DBGBCR0_EL1 + 15,
	DBGBVR0_EL1,	/* Debug Breakpoint Value Registers */
	DBGBVR15_EL1 = DBGBVR0_EL1 + 15,
	DBGWCR0_EL1,	/* Debug Watchpoint Control Registers */
	DBGWCR15_EL1 = DBGWCR0_EL1 + 15,
	DBGWVR0_EL1,	/* Debug Watchpoint Value Registers */
	DBGWVR15_EL1 = DBGWVR0_EL1 + 15,

	/* EL2 registers used to control the guest, but not exposed to it */
	HOST_CPTR_EL2,	 /* Architectural Feature Trap Register */
	HOST_HCR_EL2,	 /* Hypervisor Configuration Register */
	HOST_HCRX_EL2,	 /* Extended Hypervisor Configuration Register */
	HOST_MDCR_EL2,	 /* Monitor Debug Configuration Register */
	HOST_VPIDR_EL2,	 /* Virtualization Processor ID Register */
	HOST_VMPIDR_EL2, /* Virtualization Multiprocessor ID Register */
	/* On systems without NV2 this still points to the register storage
	   memory page */
	HOST_VNCR_EL2, /* Virtual Nested Control Register */
	HOST_VTTBR_EL2,

	/* FEAT_FGT registers */
	/*HOST_HAFGRTR_EL2; */ /* For FEAT_AMUv1 (not supported) */
	HOST_HDFGRTR_EL2,
	HOST_HDFGWTR_EL2,
	HOST_HFGITR_EL2,
	HOST_HFGRTR_EL2,
	HOST_HFGWTR_EL2,

	/* FEAT_FGT2 registers */
	HOST_HDFGRTR2_EL2,
	HOST_HDFGWTR2_EL2,
	HOST_HFGITR2_EL2,
	HOST_HFGRTR2_EL2,
	HOST_HFGWTR2_EL2,

	/* Exit info registers */
	HOST_FAR_EL2,	/* Fault Address Register */
	HOST_HPFAR_EL2,	/* Hypervisor IPA Fault Address Register */

	HOST_ICH_EISR_EL2,	/* End of Interrupt Status Register */
	HOST_ICH_ELRSR_EL2,	/* Empty List Register Status Register */
	HOST_ICH_HCR_EL2,	/* Hyp Control Register */
	HOST_ICH_MISR_EL2,	/* Maintenance Interrupt State Register */
	HOST_ICH_VMCR_EL2,	/* Virtual Machine Control Register */

	/*
	 * The List Registers are part of the VM context and are modified on a
	 * world switch. They need to be allocated statically so they are
	 * mapped in the EL2 translation tables when struct hypctx is mapped.
	 */
	HOST_ICH_LR0_EL2,
	HOST_ICH_LR_MAX_EL2 = HOST_ICH_LR0_EL2 + (VGIC_ICH_LR_NUM_MAX - 1),
	/* Active Priorities Registers for Group 0 and 1 interrupts */
	HOST_ICH_AP0R0_EL2,
	HOST_ICH_AP0R_MAX_EL2 = HOST_ICH_AP0R0_EL2 + (VGIC_ICH_APR_NUM_MAX - 1),
	HOST_ICH_AP1R0_EL2,
	HOST_ICH_AP1R_MAX_EL2 = HOST_ICH_AP1R0_EL2 + (VGIC_ICH_APR_NUM_MAX - 1),

	HOST_CNTHCTL_EL2,
	HOST_CNTVOFF_EL2,

	NR_NON_VNCR_REGS,

	/* VNCR Registers */
	VNCR_START,

	VNCR_REG(VTTBR_EL2),
	VNCR_REG(VSTTBR_EL2),
	VNCR_REG(VTCR_EL2),
	VNCR_REG(VSTCR_EL2),
	VNCR_REG(VMPIDR_EL2),	/* Virtualization Multiprocessor ID Register */
	VNCR_REG(CNTVOFF_EL2),
	VNCR_REG(HCR_EL2),	/* Hypervisor Configuration Register */
	VNCR_REG(HSTR_EL2),
	VNCR_REG(VPIDR_EL2),	/* Virtualization Processor ID Register */
	VNCR_REG(TPIDR_EL2),
	VNCR_REG(HCRX_EL2),	/* Extended Hypervisor Configuration Register */
	VNCR_REG(VNCR_EL2),
	VNCR_REG(CPACR_EL1),	/* Architectural Feature Access Control Register */
	VNCR_REG(CONTEXTIDR_EL1),	/* Current Process Identifier */
	VNCR_REG(SCTLR_EL1),	/* System Control Register */
	VNCR_REG(ACTLR_EL1),	/* Auxiliary Control Register */
	VNCR_REG(TCR_EL1),	/* Translation Control Register */
	VNCR_REG(AFSR0_EL1),	/* Auxiliary Fault Status Register 0 */
	VNCR_REG(AFSR1_EL1),	/* Auxiliary Fault Status Register 1 */
	VNCR_REG(ESR_EL1),	/* Exception Syndrome Register */
	VNCR_REG(MAIR_EL1),	/* Memory Attribute Indirection Register */
	VNCR_REG(AMAIR_EL1),	/* Auxiliary Memory Attribute Indirection Register */
	VNCR_REG(MDSCR_EL1),	/* Monitor Debug System Control Register */
	VNCR_REG(SPSR_EL1),	/* Saved Program Status Register */
	VNCR_REG(CNTV_CVAL_EL0),
	VNCR_REG(CNTV_CTL_EL0),
	VNCR_REG(CNTP_CVAL_EL0),
	VNCR_REG(CNTP_CTL_EL0),
	VNCR_REG(SCXTNUM_EL1),
	VNCR_REG(TFSR_EL1),
	VNCR_REG(HDFGRTR2_EL2),
	VNCR_REG(CNTPOFF_EL2),
	VNCR_REG(HDFGWTR2_EL2),
	VNCR_REG(HFGRTR_EL2),
	VNCR_REG(HFGWTR_EL2),
	VNCR_REG(HFGITR_EL2),
	VNCR_REG(HDFGRTR_EL2),
	VNCR_REG(HDFGWTR_EL2),
	VNCR_REG(ZCR_EL1),
	VNCR_REG(HAFGRTR_EL2),
	VNCR_REG(SMCR_EL1),
	VNCR_REG(SMPRIMAP_EL2),
	VNCR_REG(TTBR0_EL1),	/* Translation Table Base Register 0 */
	VNCR_REG(TTBR1_EL1),	/* Translation Table Base Register 1 */
	VNCR_REG(FAR_EL1),	/* Fault Address Register */
	VNCR_REG(ELR_EL1),	/* Exception Link Register */
	VNCR_REG(SP_EL1),
	VNCR_REG(VBAR_EL1),	/* Vector Base Address Register */
	VNCR_REG(TCR2_EL1),	/* Translation Control Register 2 */
	VNCR_REG(SCTLR2_EL1),
	VNCR_REG(MAIR2_EL1),
	VNCR_REG(AMAIR2_EL1),
	VNCR_REG(PIRE0_EL1),
	VNCR_REG(PIRE0_EL2),
	VNCR_REG(PIR_EL1),
	VNCR_REG(POR_EL1),
	VNCR_REG(S2PIR_EL2),
	VNCR_REG(S2POR_EL1),
	VNCR_REG(HFGRTR2_EL2),
	VNCR_REG(HFGWTR2_EL2),
	VNCR_REG(PFAR_EL1),
	VNCR_REG(HFGITR2_EL2),
	VNCR_REG(SCTLRMASK_EL1),
	VNCR_REG(CPACRMASK_EL1),
	VNCR_REG(SCTLR2MASK_EL1),
	VNCR_REG(TCRMASK_EL1),
	VNCR_REG(TCR2MASK_EL1),
	VNCR_REG(ACTLRMASK_EL1),
	VNCR_REG(ICH_HCR_EL2),
	VNCR_REG(ICH_VMCR_EL2),
	VNCR_REG(VDISR_EL2),
	VNCR_REG(VSESR_EL2),
	VNCR_REG(PMBLIMITR_EL1),
	VNCR_REG(PMBPTR_EL1),
	VNCR_REG(PMBSR_EL1),
	VNCR_REG(PMSCR_EL1),
	VNCR_REG(PMSEVFR_EL1),
	VNCR_REG(PMSICR_EL1),
	VNCR_REG(PMSIRR_EL1),
	VNCR_REG(PMSLATFR_EL1),
	VNCR_REG(PMSNEVFR_EL1),
	VNCR_REG(PMSDSFR_EL1),
	VNCR_REG(TRFCR_EL1),
	VNCR_REG(TRCITECR_EL1),
	VNCR_REG(GCSPR_EL1),
	VNCR_REG(GCSCR_EL1),
	VNCR_REG(BRBCR_EL1),
	VNCR_REG(SPMACCESSR_EL1),

	/* Virtual-address-sized registers */
	VA_REGS_START,

	/* Do not reorder these without matching asm changes */
	GPR_LR,
	GPR_X0,
	GPR_X30 = GPR_X0 + 30,
	/* These can be reordered freely */
	SP_EL0,	     /* Stack pointer */
	TPIDR_EL0,   /* EL0 Software ID Register */
	TPIDRRO_EL0, /* Read-only Thread ID Register */
	TPIDR_EL1,   /* EL1 Software ID Register */
	HOST_SP_EL1,
	HOST_ELR_EL2,

	VA_REGS_END,
};

/*
 * Per-vCPU hypervisor state.
 */
struct hypctx {
	/* Virtual-address-sized registers */
	uint64_t va_regs[VA_REGS_END - VA_REGS_START - 1];
	/* Non-VNCR register state */
	uint64_t sys_regs[NR_NON_VNCR_REGS];

	struct hyp	*hyp;
	struct vcpu	*vcpu;

	struct vtimer_cpu 	vtimer_cpu;

	uint64_t		setcaps;	/* Currently enabled capabilities. */

	/* vCPU state used to handle guest debugging. */
	uint64_t		debug_spsr;		/* Saved guest SPSR */
	uint64_t		debug_mdscr;		/* Saved guest MDSCR */

	struct {
		uint16_t	ich_lr_num;
		uint16_t	ich_apr_num;
	} vgic_v3;
	struct vgic_v3_cpu	*vgic_cpu;
	bool			has_exception;
	bool			dbg_oslock;

	/*
	 * Memory page pointed at by VNCR_EL2. Contains storage for registers
	 * the accesses to which are redirected to memory by FEAT_NV2.
	 * NOTE: The storage is still used even if FEAT_NV2 is not present.
	 */
	void *vncr_regs;
	/* Memory page used to store the host's values of VNCR registers. */
	void *host_vncr_regs;

	uint64_t	el2_addr;	/* The address of this in el2 space */
	uint64_t	el2_vncr_addr;	/* The address of vncr_regs in el2 space */
	uint64_t	el2_host_vncr_addr;
};

/* For non-VHE vmm_hyp.c, this will already be defined in vmm_nvhe.c */
#ifndef __hypctx_vncr_sysreg
#define __hypctx_vncr_sysreg(hypctx, reg)       \
	((uint64_t *)((char *)hypctx->vncr_regs + REG_VNCR_OFFSET(reg)))
#endif

#ifndef __hypctx_va_sysreg
#define __hypctx_va_sysreg(hypctx, reg)		\
	(&hypctx->va_regs[reg - VA_REGS_START - 1])
#endif

/* Calling this with markers like reg=VNCR_START or reg=NR_NON_VNCR_REGS
   is a bad idea */
static inline uint64_t *
hypctx_sys_reg(struct hypctx *hypctx, int reg /* enum hypctx_sysreg  */)
{
	/* Extract this into a separate helper when we actually support 128-bit regs */
	if (reg > VA_REGS_START)
		return (__hypctx_va_sysreg(hypctx, reg));
	if (reg > VNCR_START)
		return (__hypctx_vncr_sysreg(hypctx, reg));
	return (&hypctx->sys_regs[reg]);
}

static inline void
hypctx_write_sys_reg(struct hypctx *hypctx, enum hypctx_sysreg reg,
    uint64_t val)
{
	*hypctx_sys_reg(hypctx, reg) = val;
}

static inline uint64_t
hypctx_read_sys_reg(struct hypctx *hypctx, enum hypctx_sysreg reg)
{
	return (*hypctx_sys_reg(hypctx, reg));
}

struct hyp {
	struct vm	*vm;
	uint64_t	vmid_generation;
	uint64_t	cntvoff_el2;    /* VM-wide virtual timer offset */
	uint64_t	el2_addr;	/* The address of this in el2 space */
	uint64_t	feats;		/* Which features are enabled */
#define	HYP_FEAT_HCX		(0x1ul << 0)
#define	HYP_FEAT_ECV_POFF	(0x1ul << 1)
#define	HYP_FEAT_FGT		(0x1ul << 2)
#define	HYP_FEAT_FGT2		(0x1ul << 3)
	bool		vgic_attached;
	struct vgic_v3	*vgic;
	struct hypctx	*ctx[];
};

uint64_t	vmm_call_hyp(uint64_t, ...);

#if 0
#define	eprintf(fmt, ...)	printf("%s:%d " fmt, __func__, __LINE__, ##__VA_ARGS__)
#else
#define	eprintf(fmt, ...)	do {} while(0)
#endif

struct hypctx *arm64_get_active_vcpu(void);
void raise_data_insn_abort(struct hypctx *, uint64_t, bool, int);

#endif /* !_VMM_ARM64_H_ */
