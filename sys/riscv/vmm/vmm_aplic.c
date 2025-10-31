/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 Ruslan Bukin <br@bsdpad.com>
 *
 * This software was developed by the University of Cambridge Computer
 * Laboratory (Department of Computer Science and Technology) under Innovate
 * UK project 105694, "Digital Security by Design (DSbD) Technology Platform
 * Prototype".
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/rman.h>
#include <sys/smp.h>

#include <riscv/vmm/riscv.h>
#include <riscv/vmm/vmm_aplic.h>

#include <machine/vmm_instruction_emul.h>
#include <machine/vmm_dev.h>

MALLOC_DEFINE(M_APLIC, "RISC-V VMM APLIC", "RISC-V AIA APLIC");

#define	APLIC_DOMAINCFG		0x0000
#define	 DOMAINCFG_IE		(1 << 8) /* Interrupt Enable. */
#define	 DOMAINCFG_DM		(1 << 2) /* Direct Mode. */
#define	 DOMAINCFG_BE		(1 << 0) /* Big-Endian. */
#define	APLIC_SOURCECFG(x)	(0x0004 + ((x) - 1) * 4)
#define	 SOURCECFG_D		(1 << 10) /* D - Delegate. */
/* If D == 0. */
#define	 SOURCECFG_SM_S		(0)
#define	 SOURCECFG_SM_M		(0x7 << SOURCECFG_SM_S)
#define	 SOURCECFG_SM_INACTIVE	(0) /* Not delegated. */
#define	 SOURCECFG_SM_DETACHED	(1)
#define	 SOURCECFG_SM_RESERVED	(2)
#define	 SOURCECFG_SM_RESERVED1	(3)
#define	 SOURCECFG_SM_EDGE1	(4) /* Rising edge. */
#define	 SOURCECFG_SM_EDGE0	(5) /* Falling edge. */
#define	 SOURCECFG_SM_LEVEL1	(6) /* High. */
#define	 SOURCECFG_SM_LEVEL0	(7) /* Low. */
/* If D == 1. */
#define	 SOURCECFG_CHILD_INDEX_S	(0)
#define	 SOURCECFG_CHILD_INDEX_M	(0x3ff << SOURCECFG_CHILD_INDEX_S)
#define	APLIC_SETIP		0x1c00
#define	APLIC_SETIPNUM		0x1cdc
#define	APLIC_CLRIP		0x1d00
#define	APLIC_CLRIPNUM		0x1ddc
#define	APLIC_SETIE		0x1e00
#define	APLIC_SETIENUM		0x1edc
#define	APLIC_CLRIE		0x1f00
#define	APLIC_CLRIENUM		0x1fdc
#define	APLIC_GENMSI		0x3000
#define	APLIC_TARGET(x)		(0x3004 + ((x) - 1) * 4)
#define	 TARGET_HART_S		18
#define	 TARGET_HART_M		0x3fff
#define	APLIC_IDC(x)		(0x4000 + (x) * 32)
#define	 IDC_IDELIVERY(x)	(APLIC_IDC(x) + 0x0)
#define	 IDC_IFORCE(x)		(APLIC_IDC(x) + 0x4)
#define	 IDC_ITHRESHOLD(x)	(APLIC_IDC(x) + 0x8)
#define	 IDC_TOPI(x)		(APLIC_IDC(x) + 0x18)
#define	 IDC_CLAIMI(x)		(APLIC_IDC(x) + 0x1C)
#define	   CLAIMI_IRQ_S		(16)
#define	   CLAIMI_IRQ_M		(0x3ff << CLAIMI_IRQ_S)
#define	   CLAIMI_PRIO_S	(0)
#define	   CLAIMI_PRIO_M	(0xff << CLAIMI_PRIO_S)

#define	APLIC_NIRQS	63

struct aplic_irq {
	uint32_t sourcecfg;
	uint32_t state;
#define	APLIC_IRQ_STATE_PENDING	(1 << 0)
#define	APLIC_IRQ_STATE_ENABLED	(1 << 1)
#define	APLIC_IRQ_STATE_INPUT	(1 << 2)
	uint32_t target;
	uint32_t target_hart;
};

struct aplic {
	uint32_t mem_start;
	uint32_t mem_end;
	struct mtx mtx;
	struct aplic_irq *irqs;
	int nirqs;
	uint32_t domaincfg;
};

static int
aplic_handle_sourcecfg(struct aplic *aplic, int i, bool write, uint64_t *val)
{
	struct aplic_irq *irq;

	if (i <= 0 || i > aplic->nirqs)
		return (ENOENT);

	mtx_lock_spin(&aplic->mtx);
	irq = &aplic->irqs[i];
	if (write)
		irq->sourcecfg = *val;
	else
		*val = irq->sourcecfg;
	mtx_unlock_spin(&aplic->mtx);

	return (0);
}

static int
aplic_set_enabled(struct aplic *aplic, bool write, uint64_t *val, bool enabled)
{
	struct aplic_irq *irq;
	int i;

	if (!write) {
		*val = 0;
		return (0);
	}

	i = *val;
	if (i <= 0 || i > aplic->nirqs)
		return (-1);

	irq = &aplic->irqs[i];

	mtx_lock_spin(&aplic->mtx);
	if ((irq->sourcecfg & SOURCECFG_SM_M) != SOURCECFG_SM_INACTIVE) {
		if (enabled)
			irq->state |= APLIC_IRQ_STATE_ENABLED;
		else
			irq->state &= ~APLIC_IRQ_STATE_ENABLED;
	}
	mtx_unlock_spin(&aplic->mtx);

	return (0);
}

static void
aplic_set_enabled_word(struct aplic *aplic, bool write, uint32_t word,
    uint64_t *val, bool enabled)
{
	uint64_t v;
	int i;

	if (!write) {
		*val = 0;
		return;
	}

	/*
	 * The write is ignored if value written is not an active interrupt
	 * source number in the domain.
	 */
	for (i = 0; i < 32; i++)
		if (*val & (1u << i)) {
			v = word * 32 + i;
			(void)aplic_set_enabled(aplic, write, &v, enabled);
		}
}

static int
aplic_handle_target(struct aplic *aplic, int i, bool write, uint64_t *val)
{
	struct aplic_irq *irq;

	mtx_lock_spin(&aplic->mtx);
	irq = &aplic->irqs[i];
	if (write) {
		irq->target = *val;
		irq->target_hart = (irq->target >> TARGET_HART_S);
	} else
		*val = irq->target;
	mtx_unlock_spin(&aplic->mtx);

	return (0);
}

static int
aplic_handle_idc_claimi(struct hyp *hyp, struct aplic *aplic, int cpu_id,
    bool write, uint64_t *val)
{
	struct aplic_irq *irq;
	bool found;
	int i;

	/* Writes to claimi are ignored. */
	if (write)
		return (-1);

	found = false;

	mtx_lock_spin(&aplic->mtx);
	for (i = 0; i < aplic->nirqs; i++) {
		irq = &aplic->irqs[i];
		if (irq->target_hart != cpu_id)
			continue;
		if (irq->state & APLIC_IRQ_STATE_PENDING) {
			*val = (i << CLAIMI_IRQ_S) | (0 << CLAIMI_PRIO_S);
			irq->state &= ~APLIC_IRQ_STATE_PENDING;
			found = true;
			break;
		}
	}
	mtx_unlock_spin(&aplic->mtx);

	if (found == false)
		*val = 0;

	return (0);
}

static int
aplic_handle_idc(struct hyp *hyp, struct aplic *aplic, int cpu, int reg,
    bool write, uint64_t *val)
{
	int error;

	switch (reg + APLIC_IDC(0)) {
	case IDC_IDELIVERY(0):
	case IDC_IFORCE(0):
	case IDC_ITHRESHOLD(0):
	case IDC_TOPI(0):
		error = 0;
		break;
	case IDC_CLAIMI(0):
		error = aplic_handle_idc_claimi(hyp, aplic, cpu, write, val);
		break;
	default:
		error = ENOENT;
	}

	return (error);
}

static int
aplic_mmio_access(struct hyp *hyp, struct aplic *aplic, uint64_t reg,
    bool write, uint64_t *val)
{
	int error;
	int cpu;
	int r;
	int i;

	dprintf("%s: reg %lx\n", __func__, reg);

	if ((reg >= APLIC_SOURCECFG(1)) &&
	    (reg <= APLIC_SOURCECFG(aplic->nirqs))) {
		i = ((reg - APLIC_SOURCECFG(1)) >> 2) + 1;
		error = aplic_handle_sourcecfg(aplic, i, write, val);
		return (error);
	}

	if ((reg >= APLIC_TARGET(1)) && (reg <= APLIC_TARGET(aplic->nirqs))) {
		i = ((reg - APLIC_TARGET(1)) >> 2) + 1;
		error = aplic_handle_target(aplic, i, write, val);
		return (error);
	}

	if ((reg >= APLIC_IDC(0)) && (reg < APLIC_IDC(mp_ncpus))) {
		cpu = (reg - APLIC_IDC(0)) >> 5;
		r = (reg - APLIC_IDC(0)) % 32;
		error = aplic_handle_idc(hyp, aplic, cpu, r, write, val);
		return (error);
	}

	if ((reg >= APLIC_CLRIE) && (reg < (APLIC_CLRIE + aplic->nirqs * 4))) {
		i = (reg - APLIC_CLRIE) >> 2;
		aplic_set_enabled_word(aplic, write, i, val, false);
		return (0);
	}

	switch (reg) {
	case APLIC_DOMAINCFG:
		mtx_lock_spin(&aplic->mtx);
		if (write)
			aplic->domaincfg = *val & DOMAINCFG_IE;
		else
			*val = aplic->domaincfg;
		mtx_unlock_spin(&aplic->mtx);
		error = 0;
		break;
	case APLIC_SETIENUM:
		error = aplic_set_enabled(aplic, write, val, true);
		break;
	case APLIC_CLRIENUM:
		error = aplic_set_enabled(aplic, write, val, false);
		break;
	default:
		dprintf("%s: unknown reg %lx", __func__, reg);
		error = ENOENT;
		break;
	};

	return (error);
}

static int
mem_read(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t *rval, int size,
    void *arg)
{
	struct hypctx *hypctx;
	struct hyp *hyp;
	struct aplic *aplic;
	uint64_t reg;
	uint64_t val;
	int error;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	aplic = hyp->aplic;

	dprintf("%s: fault_ipa %lx size %d\n", __func__, fault_ipa, size);

	if (fault_ipa < aplic->mem_start || fault_ipa + size > aplic->mem_end)
		return (EINVAL);

	reg = fault_ipa - aplic->mem_start;

	error = aplic_mmio_access(hyp, aplic, reg, false, &val);
	if (error == 0)
		*rval = val;

	return (error);
}

static int
mem_write(struct vcpu *vcpu, uint64_t fault_ipa, uint64_t wval, int size,
    void *arg)
{
	struct hypctx *hypctx;
	struct hyp *hyp;
	struct aplic *aplic;
	uint64_t reg;
	uint64_t val;
	int error;

	hypctx = vcpu_get_cookie(vcpu);
	hyp = hypctx->hyp;
	aplic = hyp->aplic;

	dprintf("%s: fault_ipa %lx wval %lx size %d\n", __func__, fault_ipa,
	    wval, size);

	if (fault_ipa < aplic->mem_start || fault_ipa + size > aplic->mem_end)
		return (EINVAL);

	reg = fault_ipa - aplic->mem_start;

	val = wval;

	error = aplic_mmio_access(hyp, aplic, reg, true, &val);

	return (error);
}

void
aplic_vminit(struct hyp *hyp)
{
	struct aplic *aplic;

	hyp->aplic = malloc(sizeof(*hyp->aplic), M_APLIC,
	    M_WAITOK | M_ZERO);
	aplic = hyp->aplic;

	mtx_init(&aplic->mtx, "APLIC lock", NULL, MTX_SPIN);
}

void
aplic_vmcleanup(struct hyp *hyp)
{
	struct aplic *aplic;

	aplic = hyp->aplic;

	mtx_destroy(&aplic->mtx);

	free(hyp->aplic, M_APLIC);
}

int
aplic_attach_to_vm(struct hyp *hyp, struct vm_aplic_descr *descr)
{
	struct aplic *aplic;
	struct vm *vm;

	vm = hyp->vm;

	dprintf("%s\n", __func__);

	vm_register_inst_handler(vm, descr->mem_start, descr->mem_size,
	    mem_read, mem_write);

	aplic = hyp->aplic;
	aplic->nirqs = APLIC_NIRQS;
	aplic->mem_start = descr->mem_start;
	aplic->mem_end = descr->mem_start + descr->mem_size;
	aplic->irqs = malloc(sizeof(struct aplic_irq) * aplic->nirqs, M_APLIC,
	    M_WAITOK | M_ZERO);

	hyp->aplic_attached = true;

	return (0);
}

void
aplic_detach_from_vm(struct hyp *hyp)
{
	struct aplic *aplic;

	aplic = hyp->aplic;

	dprintf("%s\n", __func__);

	if (hyp->aplic_attached) {
		hyp->aplic_attached = false;
		free(aplic->irqs, M_APLIC);
	}
}

int
aplic_check_pending(struct hypctx *hypctx)
{
	struct aplic_irq *irq;
	struct aplic *aplic;
	struct hyp *hyp;
	int i;

	hyp = hypctx->hyp;
	aplic = hyp->aplic;

	mtx_lock_spin(&aplic->mtx);
	if ((aplic->domaincfg & DOMAINCFG_IE) == 0) {
		mtx_unlock_spin(&aplic->mtx);
		return (0);
	}

	for (i = 0; i < aplic->nirqs; i++) {
		irq = &aplic->irqs[i];
		if (irq->target_hart != hypctx->cpu_id)
			continue;
		if ((irq->state & APLIC_IRQ_STATE_ENABLED) &&
		    (irq->state & APLIC_IRQ_STATE_PENDING)) {
			mtx_unlock_spin(&aplic->mtx);
			/* Found. */
			return (1);
		}
	}
	mtx_unlock_spin(&aplic->mtx);

	return (0);
}

int
aplic_inject_irq(struct hyp *hyp, int vcpuid, uint32_t irqid, bool level)
{
	struct aplic_irq *irq;
	struct aplic *aplic;
	bool notify;
	int error;
	int mask;

	aplic = hyp->aplic;

	error = 0;

	mtx_lock_spin(&aplic->mtx);
	if ((aplic->domaincfg & DOMAINCFG_IE) == 0) {
		mtx_unlock_spin(&aplic->mtx);
		return (error);
	}

	irq = &aplic->irqs[irqid];
	if (irq->sourcecfg & SOURCECFG_D) {
		mtx_unlock_spin(&aplic->mtx);
		return (error);
	}

	notify = false;
	switch (irq->sourcecfg & SOURCECFG_SM_M) {
	case SOURCECFG_SM_LEVEL0:
		if (!level)
			irq->state |= APLIC_IRQ_STATE_PENDING;
		break;
	case SOURCECFG_SM_LEVEL1:
		if (level)
			irq->state |= APLIC_IRQ_STATE_PENDING;
		break;
	case SOURCECFG_SM_EDGE0:
		if (!level && (irq->state & APLIC_IRQ_STATE_INPUT))
			irq->state |= APLIC_IRQ_STATE_PENDING;
		break;
	case SOURCECFG_SM_EDGE1:
		if (level && !(irq->state & APLIC_IRQ_STATE_INPUT))
			irq->state |= APLIC_IRQ_STATE_PENDING;
		break;
	case SOURCECFG_SM_DETACHED:
	case SOURCECFG_SM_INACTIVE:
		break;
	default:
		error = ENXIO;
		break;
	}

	if (level)
		irq->state |= APLIC_IRQ_STATE_INPUT;
	else
		irq->state &= ~APLIC_IRQ_STATE_INPUT;

	mask = APLIC_IRQ_STATE_ENABLED | APLIC_IRQ_STATE_PENDING;
	if ((irq->state & mask) == mask)
		notify = true;

	mtx_unlock_spin(&aplic->mtx);

	if (notify)
		vcpu_notify_event(vm_vcpu(hyp->vm, irq->target_hart));

	return (error);
}

int
aplic_inject_msi(struct hyp *hyp, uint64_t msg, uint64_t addr)
{

	/* TODO. */

	return (ENXIO);
}

void
aplic_cpuinit(struct hypctx *hypctx)
{

}

void
aplic_cpucleanup(struct hypctx *hypctx)
{

}

void
aplic_flush_hwstate(struct hypctx *hypctx)
{

}

void
aplic_sync_hwstate(struct hypctx *hypctx)
{

}
