/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2018 Alexandru Elisei <alexandru.elisei@gmail.com>
 * Copyright (C) 2020-2022 Andrew Turner
 * Copyright (C) 2023 Arm Ltd
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

#include <sys/types.h>
#include <sys/errno.h>
#include <sys/systm.h>
#include <sys/bitstring.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/smp.h>
#include <sys/intr.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <dev/ofw/openfirm.h>

#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpufunc.h>
#include <machine/cpu.h>
#include <machine/machdep.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/vmparam.h>
#include <machine/intr.h>
#include <machine/vmm.h>
#include <machine/vmm_instruction_emul.h>

#include <arm/arm/gic_common.h>

#include <arm64/arm64/gicv5reg.h>
#include <arm64/arm64/gicv5var.h>

#include <arm64/vmm/hyp.h>
#include <arm64/vmm/mmu.h>
#include <arm64/vmm/arm64.h>
#include <arm64/vmm/vmm_handlers.h>

#include <dev/vmm/vmm_dev.h>
#include <dev/vmm/vmm_vm.h>

#include "vgic.h"
#include "vgic_internal.h"

#include "vgic_if.h"

MALLOC_DEFINE(M_VGIC_V5, "ARM VMM VGIC V5", "ARM VMM VGIC V5");

#define	SPI_COUNT	4096

#define VGIC_IAFFID_BITS	16
#define VGIC_INTID_BITS		12
#define VGIC_PRI_BITS		5
#define VGIC_MIN_LPI_BITS	8

static int vgic_v5_max_cpu_count(device_t dev, struct hyp *hyp);

static bool
has_gcie_legacy(void)
{
	uint64_t idr0;

	get_kernel_reg(ICC_IDR0_EL1, &idr0);

	return ((idr0 & ICC_IDR0_GCIE_LEGACY_MASK) ? true : false);
}

static void
vgic_v5_vminit(device_t dev, struct hyp *hyp)
{
	struct vgic_v5 *vgic;

	vgic = &hyp->vgic->v5;
	vgic->max_cpu_count = vgic_v5_max_cpu_count(dev, hyp);
}

static void
vgic_v5_cpuinit(device_t dev, struct hypctx *hypctx)
{
	if (hypctx->hyp->vgic->vgic_ver == 3) {
		vgic_v3_cpuinit(dev, hypctx);
		return;
	}

	hypctx_write_sys_reg(hypctx, HOST_ICH_HFGITR_EL2,
	    ICH_HFGITR_EL2_GICCDPEND |
	    ICH_HFGITR_EL2_GICCDEN |
	    ICH_HFGITR_EL2_GICCDAFF |
	    ICH_HFGITR_EL2_GICCDPRI |
	    ICH_HFGITR_EL2_GICRCDIA |
	    ICH_HFGITR_EL2_GICCDDI |
	    ICH_HFGITR_EL2_GICCDDIS |
	    ICH_HFGITR_EL2_GICCDEOI |
	    ICH_HFGITR_EL2_GICCDHM |
	    ICH_HFGITR_EL2_GICCDRCFG);

	hypctx_write_sys_reg(hypctx, HOST_ICH_HFGWTR_EL2,
	    ICH_HFGWTR_EL2_ICC_PPI_ACTIVERn_EL1 |
	    ICH_HFGWTR_EL2_ICC_PPI_PRIORITYn_EL1 |
	    ICH_HFGWTR_EL2_ICC_PPI_PENDRn_EL1 |
	    ICH_HFGWTR_EL2_ICC_PPI_ENABLERn_EL1 |
	    ICH_HFGWTR_EL2_ICC_ICSR_EL1 |
	    ICH_HFGWTR_EL2_ICC_PCR_EL1 |
	    ICH_HFGWTR_EL2_ICC_CR0_EL1);
	hypctx_write_sys_reg(hypctx, HOST_ICH_HFGRTR_EL2,
	    hypctx_read_sys_reg(hypctx, HOST_ICH_HFGWTR_EL2) |
	    ICH_HFGRTR_EL2_ICC_PPI_HMRn_EL1);
}

static void
vgic_v5_cpucleanup(device_t dev, struct hypctx *hypctx)
{
	if (hypctx->hyp->vgic->vgic_ver == 3) {
		vgic_v3_cpucleanup(dev, hypctx);
		return;
	}
}

static void
vgic_v5_vmcleanup(device_t dev, struct hyp *hyp)
{
}

static int
vgic_v5_max_cpu_count(device_t dev, struct hyp *hyp)
{
	switch (hyp->vgic->vgic_ver) {
	case 0:
		/* No GIC attached, return the maximum */
		return (vm_get_maxcpus(hyp->vm));
	case 3:
		return (vgic_v3_max_cpu_count(dev, hyp));
	case 5:
		return (gicv5_get_max_vpe_count(device_get_parent(dev)));
	default:
		panic("%s: Invalid GIC version %u", __func__,
		    hyp->vgic->vgic_ver);
	}
}

static int
vgic_v5_icc_iaffidr_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	*rval = vcpu_vcpuid(vcpu);
	return (0);
}

static int
vgic_v5_icc_idr0_read(struct vcpu *vcpu, uint64_t *rval, void *arg)
{
	*rval = ICC_IDR0_ID_BITS_16 | ICC_IDR0_PRI_BITS_5 |
	    ICC_IDR0_GCIE_LEGACY_NONE;
	return (0);
}

static int
vgic_v5_write_ignore(struct vcpu *vcpu, uint64_t rval, void *arg)
{
	return (0);
}

/* IRS_IDR0..7 */
static register_read irs_idr_read;
/* IRS_IIDR */
static register_read irs_iidr_read;
/* IRS_AIDR */
static register_read irs_aidr_read;
/* IRS_CR0 */
static register_read irs_cr0_read;
static register_write irs_cr0_write;
/* IRS_CR1 */
static register_read irs_cr1_read;
static register_write irs_cr1_write;
/* IRS_SPI_SELR */
static register_write irs_spi_selr_write;
/* IRS_SPI_CFGR */
static register_read irs_spi_cfgr_read;
static register_write irs_spi_cfgr_write;
/* IRS_SPI_STATUSR */
static register_read irs_spi_statusr_read;
/* IRS_PE_SELR */
static register_write irs_pe_selr_write;
/* IRS_PE_STATUSR */
static register_read irs_pe_statusr_read;
/* IRS_PE_CR0 */
static register_read irs_pe_cr0_read;
static register_write irs_pe_cr0_write;
/* IRS_IST_BASER */
static register_read irs_ist_baser_read;
static register_write irs_ist_baser_write;
/* IRS_IST_CFGR */
static register_read irs_ist_cfgr_read;
static register_write irs_ist_cfgr_write;
/* IRS_IST_STATUSR */
static register_read irs_ist_statusr_read;

static struct vgic_register irs_registers[] = {
	VGIC_REGISTER_RANGE(IRS_IDR0, IRS_IDR7 + 4, 4, VGIC_32_BIT,
	    irs_idr_read, vgic_ignore_write),

	VGIC_REGISTER(IRS_IIDR, 4, VGIC_32_BIT, irs_iidr_read,
	    vgic_ignore_write),
	VGIC_REGISTER(IRS_AIDR, 4, VGIC_32_BIT, irs_aidr_read,
	    vgic_ignore_write),

	VGIC_REGISTER(IRS_CR0, 4, VGIC_32_BIT, irs_cr0_read, irs_cr0_write),
	VGIC_REGISTER(IRS_CR1, 4, VGIC_32_BIT, irs_cr1_read, irs_cr1_write),
	VGIC_REGISTER(IRS_SPI_SELR, 4, VGIC_32_BIT, vgic_zero_read,
	    irs_spi_selr_write),
	VGIC_REGISTER(IRS_SPI_CFGR, 4, VGIC_32_BIT, irs_spi_cfgr_read,
	    irs_spi_cfgr_write),
	VGIC_REGISTER(IRS_SPI_STATUSR, 4, VGIC_32_BIT, irs_spi_statusr_read,
	    vgic_ignore_write),

	VGIC_REGISTER(IRS_PE_SELR, 4, VGIC_32_BIT, vgic_zero_read,
	    irs_pe_selr_write),
	VGIC_REGISTER(IRS_PE_STATUSR, 4, VGIC_32_BIT, irs_pe_statusr_read,
	    vgic_ignore_write),
	VGIC_REGISTER(IRS_PE_CR0, 4, VGIC_32_BIT, irs_pe_cr0_read,
	    irs_pe_cr0_write),


	VGIC_REGISTER(IRS_IST_BASER, 8, VGIC_64_BIT | VGIC_32_BIT,
	    irs_ist_baser_read, irs_ist_baser_write),
	VGIC_REGISTER(IRS_IST_CFGR, 4, VGIC_32_BIT, irs_ist_cfgr_read,
	    irs_ist_cfgr_write),
	VGIC_REGISTER(IRS_IST_STATUSR, 4, VGIC_32_BIT, irs_ist_statusr_read,
	    vgic_ignore_write),
};

static int
irs_read(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t *rval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vgic_v5 *vgic;
	uint64_t reg;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = &hyp->vgic->v5;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->irs_start || fault_ipa + size > vgic->irs_end) {
		return (EINVAL);
	}

	reg = fault_ipa - vgic->irs_start;

	/*
	 * Misaligned reads are an illegal access. As per RVXDYH they are RAZ.
	 */
	if ((reg & (size - 1)) != 0) {
		*rval = 0;
		return (0);
	}

	if (vgic_register_read(hypctx, irs_registers, nitems(irs_registers),
	    reg, size, rval, NULL))
		return (0);

	panic("%s: TODO %lx\n", __func__, reg);

	/* Other register addresses are RAZ */
	*rval = 0;

	return (0);
}

static int
irs_write(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t wval,
    int size, void *arg)
{
	struct hyp *hyp;
	struct hypctx *hypctx;
	struct vgic_v5 *vgic;
	uint64_t reg;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	vgic = &hyp->vgic->v5;

	/* Check the register is one of ours and is the correct size */
	if (fault_ipa < vgic->irs_start || fault_ipa + size > vgic->irs_end) {
		return (EINVAL);
	}

	reg = fault_ipa - vgic->irs_start;

	/*
	 * Misaligned writes are an illegal access. As per RVXDYH they are WI.
	 */
	if ((reg & (size - 1)) != 0)
		return (0);

	if (vgic_register_write(hypctx, irs_registers, nitems(irs_registers),
	    reg, size, wval, NULL))
		return (0);

	panic("%s: TODO %lx\n", __func__, reg);

	/* Other register addresses are WI */
	return (0);
}

static uint32_t
get_pa_range(int ipa_bits)
{
	switch (ipa_bits) {
	case 32:
		return (IRS_IDR0_PA_RANGE_4G);
	case 36:
		return (IRS_IDR0_PA_RANGE_64G);
	case 40:
		return (IRS_IDR0_PA_RANGE_1T);
	case 42:
		return (IRS_IDR0_PA_RANGE_4T);
	case 44:
		return (IRS_IDR0_PA_RANGE_16T);
	case 48:
		return (IRS_IDR0_PA_RANGE_256T);
	case 52:
		return (IRS_IDR0_PA_RANGE_4P);
	case 56:
		return (IRS_IDR0_PA_RANGE_64P);
	}

	panic("%s: unsupported IPA size %u bits\n", __func__, ipa_bits);

	return (0);
}

/*
 * IRS registers
 */
static void
irs_idr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct hyp *hyp;
	int ipa_bits;
	int n;

	hyp = hypctx->hyp;

	n = (reg - IRS_IDR0) / 4;
	switch (n) {
	case 0:
		/* Match PA range in use by the VM. */
		ipa_bits = vmm_get_max_ipa_bits();
		*rval = get_pa_range(ipa_bits);
		break;
	case 1:
		*rval = ((VGIC_PRI_BITS - 1) << IRS_IDR0_PRI_BITS_SHIFT) |
		    ((VGIC_IAFFID_BITS - 1) << IRS_IDR0_IAFFID_BITS_SHIFT) |
		    (vm_get_maxcpus(hyp->vm) << IRS_IDR0_PE_CNT_SHIFT);
		break;
	case 2:
		*rval = IRS_IDR2_NO_ISTMD |
		    IRS_IDR2_IST_LEVELS_SINGLE |
		    (VGIC_MIN_LPI_BITS << IRS_IDR2_MIN_LPI_ID_BITS_SHIFT) |
		    IRS_IDR2_LPI |
		    (VGIC_INTID_BITS << IRS_IDR2_ID_BITS_SHIFT);
		break;
	case 3:
	case 4:
		/* RES0 when IRS_IDR0.VIRT != 1 */
		*rval = 0;
		break;
	case 5: /* SPI range */
	case 6: /* SPI range on this IRS */
		*rval = SPI_COUNT;
		break;
	case 7: /* SPI base on this IRS */
		*rval = 0;
		break;
	default:
		panic("%s: Invalid register IRS_IDR%d", __func__, n);
	}
}

static void
irs_iidr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = 0;
}

static void
irs_aidr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = IRS_AIDR_COMPONENT_IRS | IRS_AIDR_ARCH_MAJOR_REV_V5 |
	    IRS_AIDR_ARCH_MINOR_REV_0;
}

static void
irs_cr0_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct vgic_v5 *vgic;
	uint64_t val;

	vgic = &hypctx->hyp->vgic->v5;
	val = IRS_CR0_IDLE;
	if (vgic->irs_en)
		val |= IRS_CR0_IRSEN;
	*rval = val;
}

void
irs_cr0_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	vgic->irs_en = (wval & IRS_CR0_IRSEN) != 0;
}

static void
irs_cr1_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	*rval = atomic_load_32(&vgic->irs_cr1);
}

void
irs_cr1_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	if (!(atomic_load_64(&vgic->irs_ist_baser) & IRS_IST_BASER_VALID))
		atomic_store_32(&vgic->irs_cr1, wval);
}

void
irs_spi_selr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	if ((wval & IRS_SPI_SELR_ID_MASK) >= SPI_COUNT) {
		atomic_store_32(&vgic->irs_spi_statusr, ~IRS_SPI_STATUSR_V);
		wval = SPI_COUNT;
	} else {
		atomic_store_32(&vgic->irs_spi_statusr, IRS_SPI_STATUSR_V);
	}

	atomic_store_32(&vgic->irs_spi_selr, wval);
}

void
irs_spi_cfgr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
}

static void
irs_spi_cfgr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	*rval = 0;
}

static void
irs_spi_statusr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	*rval = atomic_load_32(&vgic->irs_spi_statusr) | IRS_SPI_STATUSR_IDLE;
}

static void
irs_pe_cr0_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct hypctx *target_hypctx;
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	if (vgic->irs_pe_selr < vgic->max_cpu_count) {
		target_hypctx = hypctx->hyp->ctx[vgic->irs_pe_selr];
		*rval = target_hypctx->vgic_cpu->v5.dps ? IRS_PE_CR0_DPS : 0;
	} else
		*rval = 0;
}

void
irs_pe_cr0_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct hypctx *target_hypctx;
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	if (vgic->irs_pe_selr < vgic->max_cpu_count) {
		target_hypctx = hypctx->hyp->ctx[vgic->irs_pe_selr];
		target_hypctx->vgic_cpu->v5.dps = wval & IRS_PE_CR0_DPS;
	}
}

void
irs_pe_selr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	vgic->irs_pe_selr = wval & IRS_PE_SELR_IAFFID_MASK;
}

static void
irs_pe_statusr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;

	*rval = IRS_PE_CR0_IDLE;
	if (vgic->irs_pe_selr < vgic->max_cpu_count)
		*rval |= IRS_PE_CR0_V;
}

static void
irs_ist_baser_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	*rval = atomic_load_64(&vgic->irs_ist_baser);
}

void
irs_ist_baser_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;
	uint64_t nval, mask, val;
	u_int shift;

	vgic = &hypctx->hyp->vgic->v5;
	if (size == 8) {
		atomic_store_64(&vgic->irs_ist_baser, wval);
		return;
	}

	MPASS(size == 4);

	val = atomic_load_64(&vgic->irs_ist_baser);
	if (offset == 0) {
		/* Set the upper 32 bits */
		shift = 32;
	} else {
		MPASS(offset == 4);
		shift = 0;
	}


	mask = 0xfffffffful << shift;
	nval = (wval & 0xfffffffful) << shift;
	while (!atomic_fcmpset_64(&vgic->irs_ist_baser, &val,
	    (val & ~mask) | nval))
		cpu_spinwait();
}

static void
irs_ist_cfgr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	*rval = atomic_load_32(&vgic->irs_ist_cfgr);
}

void
irs_ist_cfgr_write(struct hypctx *hypctx, u_int reg, u_int offset, u_int size,
    uint64_t wval, void *arg)
{
	struct vgic_v5 *vgic;

	vgic = &hypctx->hyp->vgic->v5;
	if (!(atomic_load_64(&vgic->irs_ist_baser) & IRS_IST_BASER_VALID))
		atomic_store_32(&vgic->irs_ist_cfgr, wval);
}

static void
irs_ist_statusr_read(struct hypctx *hypctx, u_int reg, uint64_t *rval,
    void *arg)
{
	*rval = IRS_IST_STATUSR_IDLE;
}

static int
vgic_v5_attach_to_vm(device_t dev, struct hyp *hyp, struct vm_vgic_descr *descr)
{
	struct vm *vm;
	struct vgic_v5 *vgic;
	device_t parent;

	switch (descr->ver.version) {
	case 3:
		if (!has_gcie_legacy())
			return (EINVAL);
		return (vgic_v3_attach_to_vm(dev, hyp, descr));
	case 5:
		break;
	default:
		return (EINVAL);
	}

	vm = hyp->vm;
	vgic = &hyp->vgic->v5;

        hyp->vgic->vgic_ver = 5;

	parent = device_get_parent(dev);
	vgic->vm_id = gicv5_alloc_vm(parent, vm_get_maxcpus(hyp->vm));

	vgic->irs_start = descr->v3_regs.dist_start;
	vgic->irs_end = descr->v3_regs.dist_start + descr->v3_regs.dist_size;

	/*
	 * IRS_CR0.IDLE resets to 1
	 * IRS_CR0.IRSEN resets to 0
	 *
	 * IRS_IST_BASER.VALID resets to 0
	 *
	 * IRS_IST_STATUSR.IDLE resets to 1
	 *
	 * IRS_MPAM_PARTID_R.IDLE resets to 1
	 *
	 * IRS_PE_STATUSR.V resets to 0
	 * IRS_PE_STATUSR.IDLE resets to 1
	 *
	 * IRS_SAVE_VM_STATUSR.IDLE resets to 1
	 *
	 * IRS_SPI_STATUSR.V resets to 0
	 * IRS_SPI_STATUSR.IDLE resets to 1
	 *
	 * IRS_SYNC_STATUSR.IDLE resets to 1
	 *
	 * IRS_VM_STATUSR.V resets to 0
	 * IRS_VM_STATUSR.IDLE resets to 1
	 *
	 * IRS_VMT_BASER.VALID resets to 0
	 *
	 * IRS_VMT_STATUSR.IDLE resets to 1
	 *
	 * IRS_VPE_STATUSR.V resets to 0
	 * IRS_VPE_STATUSR.IDLE resets to 1
	 */
	vgic->irs_spi_statusr = IRS_SPI_STATUSR_IDLE;

	/* TODO: Use a GICv5 entry */
	vm_register_inst_handler(vm, descr->v3_regs.dist_start,
	    descr->v3_regs.dist_size, irs_read, irs_write);

	vm_register_reg_handler(vm, ISS_MSR_REG(ICC_IAFFIDR_EL1),
	    ISS_MSR_REG_MASK, vgic_v5_icc_iaffidr_read, vgic_v5_write_ignore,
	    NULL);
	vm_register_reg_handler(vm, ISS_MSR_REG(ICC_IDR0_EL1),
	    ISS_MSR_REG_MASK, vgic_v5_icc_idr0_read, vgic_v5_write_ignore,
	    NULL);

	hyp->vgic_attached = true;

	return (0);
}

static void
vgic_v5_detach_from_vm(device_t dev, struct hyp *hyp)
{
	device_t parent;

	switch (hyp->vgic->vgic_ver) {
	case 0:
		/* No vgic was attached */
		return;
	case 3:
		vgic_v3_detach_from_vm(dev, hyp);
		return;
	case 5:
		hyp->vgic_attached = false;
		parent = device_get_parent(dev);
		gicv5_free_vm(parent, hyp->vgic->v5.vm_id);
		return;
	default:
		/* Shouldn't happen */
		MPASS(false);
		break;
	}
}

static bool
vgic_v5_has_pending_irq(device_t dev, struct hypctx *hypctx)
{
	if (hypctx->hyp->vgic->vgic_ver == 3) {
		return (vgic_v3_has_pending_irq(dev, hypctx));
	}

	if (hypctx_read_sys_reg(hypctx, HOST_ICH_PPI_PENDR0_EL2) != 0 ||
	    hypctx_read_sys_reg(hypctx, HOST_ICH_PPI_PENDR1_EL2) != 0)
		return (true);

	if (hypctx_read_sys_reg(hypctx, HOST_ICH_HPPIR_EL2) &
	    ICH_HPPIR_EL2_HPPIV)
		return (true);

	return (false);
}

static int
vgic_v5_inject_irq(device_t dev, struct hyp *hyp, int vcpuid, uint32_t irqid,
    bool level)
{
	struct hypctx *hypctx;
	uint64_t reg;
	u_int irq;

	if (hyp->vgic->vgic_ver == 3) {
		return (vgic_v3_inject_irq(dev, hyp, vcpuid, irqid, level));
	}

	KASSERT((irqid & GSI_INT_TYPE_MASK) != 0,
	    ("%s: No type set for irqid %x\n", __func__, irqid));

	irq = irqid & GSI_INT_ID_MASK;
	switch ((irqid & GSI_INT_TYPE_MASK) >> GSI_INT_TYPE_SHIFT) {
	case 1: /* PPI */
		if (vcpuid < 0 || vcpuid >= vm_get_maxcpus(hyp->vm))
			return (EINVAL);

		hypctx = hyp->ctx[vcpuid];
		if (irq < GICV5_PPIS_PER_REG)
			reg = hypctx_read_sys_reg(hypctx,
			    HOST_ICH_PPI_PENDR0_EL2);
		else if (irq < GICV5_PPI_COUNT)
			reg = hypctx_read_sys_reg(hypctx,
			    HOST_ICH_PPI_PENDR1_EL2);
		else
			return (EINVAL);
		if (level)
			reg |= 1ul << (irq % GICV5_PPIS_PER_REG);
		else
			reg &= ~(1ul << (irq % GICV5_PPIS_PER_REG));
		if (irq < GICV5_PPIS_PER_REG)
			hypctx_write_sys_reg(hypctx, HOST_ICH_PPI_PENDR0_EL2,
			    reg);
		else if (irq < GICV5_PPI_COUNT)
			hypctx_write_sys_reg(hypctx, HOST_ICH_PPI_PENDR1_EL2,
			    reg);
		break;
	case 3: /* SPI */
		if (level)
		        WRITE_SPECIALREG(GIC_VDPEND, irq |
			    (((uint64_t)hyp->vgic->v5.vm_id <<
			    GIC_VDPEND_VM_SHIFT) & GIC_VDPEND_VM_MASK) |
			    GIC_VDPEND_TYPE_SPI | GIC_VDPEND_PENDING);
		else
		        WRITE_SPECIALREG(GIC_VDPEND, irq | GIC_VDPEND_TYPE_SPI |
			    (((uint64_t)hyp->vgic->v5.vm_id <<
			    GIC_VDPEND_VM_SHIFT) & GIC_VDPEND_VM_MASK));

		break;
	}

	if (vcpuid != -1)
		vcpu_notify_event(vm_vcpu(hyp->vm, vcpuid));

	return (0);
}

static int
vgic_v5_inject_msi(device_t dev, struct hyp *hyp, uint64_t msg, uint64_t addr)
{
	if (hyp->vgic->vgic_ver == 3) {
		return (vgic_v3_inject_msi(dev, hyp, msg, addr));
	}
	return (EINVAL);
}

static void
vgic_v5_flush_hwstate(device_t dev, struct hypctx *hypctx)
{
	if (hypctx->hyp->vgic->vgic_ver == 3) {
		vgic_v3_flush_hwstate(dev, hypctx);
		return;
	}
}

static void
vgic_v5_sync_hwstate(device_t dev, struct hypctx *hypctx)
{
	if (hypctx->hyp->vgic->vgic_ver == 3) {
		vgic_v3_sync_hwstate(dev, hypctx);
		return;
	}
}

static void
vgic_v5_init(device_t dev)
{
	if (has_gcie_legacy()) {
		vgic_v3_init(dev);
	}
}

static int
vgic_v5_probe(device_t dev)
{
	if (!gic_get_vgic(dev))
		return (EINVAL);

	/* We currently only support the GICv5 */
	if (gic_get_hw_rev(dev) < 5)
		return (EINVAL);

	device_set_desc(dev, "Virtual GIC v5");
	return (BUS_PROBE_DEFAULT);
}

static int
vgic_v5_attach(device_t dev)
{
	vgic_dev = dev;
	return (0);
}

static int
vgic_v5_detach(device_t dev)
{
	vgic_dev = NULL;
	return (0);
}

static int
vgic_v5_get_version(device_t dev)
{
	return (5);
}

static device_method_t vgic_v5_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		vgic_v5_probe),
	DEVMETHOD(device_attach,	vgic_v5_attach),
	DEVMETHOD(device_detach,	vgic_v5_detach),

	/* VGIC interface */
	DEVMETHOD(vgic_init,		vgic_v5_init),
	DEVMETHOD(vgic_attach_to_vm,	vgic_v5_attach_to_vm),
	DEVMETHOD(vgic_detach_from_vm,	vgic_v5_detach_from_vm),
	DEVMETHOD(vgic_vminit,		vgic_v5_vminit),
	DEVMETHOD(vgic_cpuinit,		vgic_v5_cpuinit),
	DEVMETHOD(vgic_cpucleanup,	vgic_v5_cpucleanup),
	DEVMETHOD(vgic_vmcleanup,	vgic_v5_vmcleanup),
	DEVMETHOD(vgic_max_cpu_count,	vgic_v5_max_cpu_count),
	DEVMETHOD(vgic_has_pending_irq,	vgic_v5_has_pending_irq),
	DEVMETHOD(vgic_inject_irq,	vgic_v5_inject_irq),
	DEVMETHOD(vgic_inject_msi,	vgic_v5_inject_msi),
	DEVMETHOD(vgic_flush_hwstate,	vgic_v5_flush_hwstate),
	DEVMETHOD(vgic_sync_hwstate,	vgic_v5_sync_hwstate),
	DEVMETHOD(vgic_get_version,	vgic_v5_get_version),

	/* End */
	DEVMETHOD_END
};

/* TODO: Create a vgic base class? */
DEFINE_CLASS_0(vgic, vgic_v5_driver, vgic_v5_methods, 0);

DRIVER_MODULE(vgic_v5, gic, vgic_v5_driver, 0, 0);
