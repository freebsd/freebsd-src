/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013 The FreeBSD Foundation
 *
 * This software was developed by Konstantin Belousov <kib@FreeBSD.org>
 * under sponsorship from the FreeBSD Foundation.
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/queue.h>
#include <sys/rman.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>
#include <sys/time.h>
#include <sys/tree.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr_machdep.h>
#include <x86/include/apicvar.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/intel_dmar.h>

u_int
dmar_nd2mask(u_int nd)
{
	static const u_int masks[] = {
		0x000f,	/* nd == 0 */
		0x002f,	/* nd == 1 */
		0x00ff,	/* nd == 2 */
		0x02ff,	/* nd == 3 */
		0x0fff,	/* nd == 4 */
		0x2fff,	/* nd == 5 */
		0xffff,	/* nd == 6 */
		0x0000,	/* nd == 7 reserved */
	};

	KASSERT(nd <= 6, ("number of domains %d", nd));
	return (masks[nd]);
}

static const struct sagaw_bits_tag {
	int agaw;
	int cap;
	int awlvl;
	int pglvl;
} sagaw_bits[] = {
	{.agaw = 30, .cap = DMAR_CAP_SAGAW_2LVL, .awlvl = DMAR_CTX2_AW_2LVL,
	    .pglvl = 2},
	{.agaw = 39, .cap = DMAR_CAP_SAGAW_3LVL, .awlvl = DMAR_CTX2_AW_3LVL,
	    .pglvl = 3},
	{.agaw = 48, .cap = DMAR_CAP_SAGAW_4LVL, .awlvl = DMAR_CTX2_AW_4LVL,
	    .pglvl = 4},
	{.agaw = 57, .cap = DMAR_CAP_SAGAW_5LVL, .awlvl = DMAR_CTX2_AW_5LVL,
	    .pglvl = 5}
	/*
	 * 6-level paging (DMAR_CAP_SAGAW_6LVL) is not supported on any
	 * current VT-d hardware and its SAGAW field value is listed as
	 * reserved in the VT-d spec.  If support is added in the future,
	 * this structure and the logic in dmar_maxaddr2mgaw() will need
	 * to change to avoid attempted comparison against 1ULL << 64.
	 */
};

bool
dmar_pglvl_supported(struct dmar_unit *unit, int pglvl)
{
	int i;

	for (i = 0; i < nitems(sagaw_bits); i++) {
		if (sagaw_bits[i].pglvl != pglvl)
			continue;
		if ((DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap) != 0)
			return (true);
	}
	return (false);
}

int
domain_set_agaw(struct dmar_domain *domain, int mgaw)
{
	int sagaw, i;

	domain->mgaw = mgaw;
	sagaw = DMAR_CAP_SAGAW(domain->dmar->hw_cap);
	for (i = 0; i < nitems(sagaw_bits); i++) {
		if (sagaw_bits[i].agaw >= mgaw) {
			domain->agaw = sagaw_bits[i].agaw;
			domain->pglvl = sagaw_bits[i].pglvl;
			domain->awlvl = sagaw_bits[i].awlvl;
			return (0);
		}
	}
	device_printf(domain->dmar->iommu.dev,
	    "context request mgaw %d: no agaw found, sagaw %x\n",
	    mgaw, sagaw);
	return (EINVAL);
}

/*
 * Find a best fit mgaw for the given maxaddr:
 *   - if allow_less is false, must find sagaw which maps all requested
 *     addresses (used by identity mappings);
 *   - if allow_less is true, and no supported sagaw can map all requested
 *     address space, accept the biggest sagaw, whatever is it.
 */
int
dmar_maxaddr2mgaw(struct dmar_unit *unit, iommu_gaddr_t maxaddr, bool allow_less)
{
	int i;

	for (i = 0; i < nitems(sagaw_bits); i++) {
		if ((1ULL << sagaw_bits[i].agaw) >= maxaddr &&
		    (DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap) != 0)
			break;
	}
	if (allow_less && i == nitems(sagaw_bits)) {
		do {
			i--;
		} while ((DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap)
		    == 0);
	}
	if (i < nitems(sagaw_bits))
		return (sagaw_bits[i].agaw);
	KASSERT(0, ("no mgaw for maxaddr %jx allow_less %d",
	    (uintmax_t) maxaddr, allow_less));
	return (-1);
}

/*
 * Return true if the page table level lvl supports the superpage for
 * the context ctx.
 */
int
domain_is_sp_lvl(struct dmar_domain *domain, int lvl)
{
	int alvl, cap_sps;
	static const int sagaw_sp[] = {
		DMAR_CAP_SPS_2M,
		DMAR_CAP_SPS_1G,
		DMAR_CAP_SPS_512G,
		DMAR_CAP_SPS_1T
	};

	alvl = domain->pglvl - lvl - 1;
	cap_sps = DMAR_CAP_SPS(domain->dmar->hw_cap);
	return (alvl < nitems(sagaw_sp) && (sagaw_sp[alvl] & cap_sps) != 0);
}

iommu_gaddr_t
domain_page_size(struct dmar_domain *domain, int lvl)
{

	return (pglvl_page_size(domain->pglvl, lvl));
}

int
calc_am(struct dmar_unit *unit, iommu_gaddr_t base, iommu_gaddr_t size,
    iommu_gaddr_t *isizep)
{
	iommu_gaddr_t isize;
	int am;

	for (am = DMAR_CAP_MAMV(unit->hw_cap);; am--) {
		isize = 1ULL << (am + IOMMU_PAGE_SHIFT);
		if ((base & (isize - 1)) == 0 && size >= isize)
			break;
		if (am == 0)
			break;
	}
	*isizep = isize;
	return (am);
}

int haw;
int dmar_tbl_pagecnt;

static void
dmar_flush_transl_to_ram(struct dmar_unit *unit, void *dst, size_t sz)
{

	if (DMAR_IS_COHERENT(unit))
		return;
	/*
	 * If DMAR does not snoop paging structures accesses, flush
	 * CPU cache to memory.
	 */
	pmap_force_invalidate_cache_range((uintptr_t)dst, (uintptr_t)dst + sz);
}

void
dmar_flush_pte_to_ram(struct dmar_unit *unit, iommu_pte_t *dst)
{

	dmar_flush_transl_to_ram(unit, dst, sizeof(*dst));
}

void
dmar_flush_ctx_to_ram(struct dmar_unit *unit, dmar_ctx_entry_t *dst)
{

	dmar_flush_transl_to_ram(unit, dst, sizeof(*dst));
}

void
dmar_flush_root_to_ram(struct dmar_unit *unit, dmar_root_entry_t *dst)
{

	dmar_flush_transl_to_ram(unit, dst, sizeof(*dst));
}

/*
 * Load the root entry pointer into the hardware, busily waiting for
 * the completion.
 */
int
dmar_load_root_entry_ptr(struct dmar_unit *unit)
{
	vm_page_t root_entry;
	int error;

	/*
	 * Access to the GCMD register must be serialized while the
	 * command is submitted.
	 */
	DMAR_ASSERT_LOCKED(unit);

	VM_OBJECT_RLOCK(unit->ctx_obj);
	root_entry = vm_page_lookup(unit->ctx_obj, 0);
	VM_OBJECT_RUNLOCK(unit->ctx_obj);
	dmar_write8(unit, DMAR_RTADDR_REG, VM_PAGE_TO_PHYS(root_entry));
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd | DMAR_GCMD_SRTP);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_RTPS)
	    != 0));
	return (error);
}

/*
 * Globally invalidate the context entries cache, busily waiting for
 * the completion.
 */
int
dmar_inv_ctx_glob(struct dmar_unit *unit)
{
	int error;

	/*
	 * Access to the CCMD register must be serialized while the
	 * command is submitted.
	 */
	DMAR_ASSERT_LOCKED(unit);
	KASSERT(!unit->qi_enabled, ("QI enabled"));

	/*
	 * The DMAR_CCMD_ICC bit in the upper dword should be written
	 * after the low dword write is completed.  Amd64
	 * dmar_write8() does not have this issue, i386 dmar_write8()
	 * writes the upper dword last.
	 */
	dmar_write8(unit, DMAR_CCMD_REG, DMAR_CCMD_ICC | DMAR_CCMD_CIRG_GLOB);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_CCMD_REG + 4) & DMAR_CCMD_ICC32)
	    == 0));
	return (error);
}

/*
 * Globally invalidate the IOTLB, busily waiting for the completion.
 */
int
dmar_inv_iotlb_glob(struct dmar_unit *unit)
{
	int error, reg;

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(!unit->qi_enabled, ("QI enabled"));

	reg = 16 * DMAR_ECAP_IRO(unit->hw_ecap);
	/* See a comment about DMAR_CCMD_ICC in dmar_inv_ctx_glob. */
	dmar_write8(unit, reg + DMAR_IOTLB_REG_OFF, DMAR_IOTLB_IVT |
	    DMAR_IOTLB_IIRG_GLB | DMAR_IOTLB_DR | DMAR_IOTLB_DW);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, reg + DMAR_IOTLB_REG_OFF + 4) &
	    DMAR_IOTLB_IVT32) == 0));
	return (error);
}

/*
 * Flush the chipset write buffers.  See 11.1 "Write Buffer Flushing"
 * in the architecture specification.
 */
int
dmar_flush_write_bufs(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);

	/*
	 * DMAR_GCMD_WBF is only valid when CAP_RWBF is reported.
	 */
	KASSERT((unit->hw_cap & DMAR_CAP_RWBF) != 0,
	    ("dmar%d: no RWBF", unit->iommu.unit));

	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd | DMAR_GCMD_WBF);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_WBFS)
	    != 0));
	return (error);
}

/*
 * Some BIOSes protect memory region they reside in by using DMAR to
 * prevent devices from doing any DMA transactions to that part of RAM.
 * AMI refers to this as "DMA Control Guarantee".
 * We need to disable this when address translation is enabled.
 */
int
dmar_disable_protected_regions(struct dmar_unit *unit)
{
	uint32_t reg;
	int error;

	DMAR_ASSERT_LOCKED(unit);

	/* Check if we support the feature. */
	if ((unit->hw_cap & (DMAR_CAP_PLMR | DMAR_CAP_PHMR)) == 0)
		return (0);

	reg = dmar_read4(unit, DMAR_PMEN_REG);
	if ((reg & DMAR_PMEN_EPM) == 0)
		return (0);

	reg &= ~DMAR_PMEN_EPM;
	dmar_write4(unit, DMAR_PMEN_REG, reg);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_PMEN_REG) & DMAR_PMEN_PRS)
	    != 0));

	return (error);
}

int
dmar_enable_translation(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd |= DMAR_GCMD_TE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_TES)
	    != 0));
	return (error);
}

int
dmar_disable_translation(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd &= ~DMAR_GCMD_TE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_TES)
	    == 0));
	return (error);
}

int
dmar_load_irt_ptr(struct dmar_unit *unit)
{
	uint64_t irta, s;
	int error;

	DMAR_ASSERT_LOCKED(unit);
	irta = unit->irt_phys;
	if (DMAR_X2APIC(unit))
		irta |= DMAR_IRTA_EIME;
	s = fls(unit->irte_cnt) - 2;
	KASSERT(unit->irte_cnt >= 2 && s <= DMAR_IRTA_S_MASK &&
	    powerof2(unit->irte_cnt),
	    ("IRTA_REG_S overflow %x", unit->irte_cnt));
	irta |= s;
	dmar_write8(unit, DMAR_IRTA_REG, irta);
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd | DMAR_GCMD_SIRTP);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_IRTPS)
	    != 0));
	return (error);
}

int
dmar_enable_ir(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd |= DMAR_GCMD_IRE;
	unit->hw_gcmd &= ~DMAR_GCMD_CFI;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_IRES)
	    != 0));
	return (error);
}

int
dmar_disable_ir(struct dmar_unit *unit)
{
	int error;

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd &= ~DMAR_GCMD_IRE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	DMAR_WAIT_UNTIL(((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_IRES)
	    == 0));
	return (error);
}

#define BARRIER_F				\
	u_int f_done, f_inproc, f_wakeup;	\
						\
	f_done = 1 << (barrier_id * 3);		\
	f_inproc = 1 << (barrier_id * 3 + 1);	\
	f_wakeup = 1 << (barrier_id * 3 + 2)

bool
dmar_barrier_enter(struct dmar_unit *dmar, u_int barrier_id)
{
	BARRIER_F;

	DMAR_LOCK(dmar);
	if ((dmar->barrier_flags & f_done) != 0) {
		DMAR_UNLOCK(dmar);
		return (false);
	}

	if ((dmar->barrier_flags & f_inproc) != 0) {
		while ((dmar->barrier_flags & f_inproc) != 0) {
			dmar->barrier_flags |= f_wakeup;
			msleep(&dmar->barrier_flags, &dmar->iommu.lock, 0,
			    "dmarb", 0);
		}
		KASSERT((dmar->barrier_flags & f_done) != 0,
		    ("dmar%d barrier %d missing done", dmar->iommu.unit,
		    barrier_id));
		DMAR_UNLOCK(dmar);
		return (false);
	}

	dmar->barrier_flags |= f_inproc;
	DMAR_UNLOCK(dmar);
	return (true);
}

void
dmar_barrier_exit(struct dmar_unit *dmar, u_int barrier_id)
{
	BARRIER_F;

	DMAR_ASSERT_LOCKED(dmar);
	KASSERT((dmar->barrier_flags & (f_done | f_inproc)) == f_inproc,
	    ("dmar%d barrier %d missed entry", dmar->iommu.unit, barrier_id));
	dmar->barrier_flags |= f_done;
	if ((dmar->barrier_flags & f_wakeup) != 0)
		wakeup(&dmar->barrier_flags);
	dmar->barrier_flags &= ~(f_inproc | f_wakeup);
	DMAR_UNLOCK(dmar);
}

struct timespec dmar_hw_timeout = {
	.tv_sec = 0,
	.tv_nsec = 1000000
};

static const uint64_t d = 1000000000;

void
dmar_update_timeout(uint64_t newval)
{

	/* XXXKIB not atomic */
	dmar_hw_timeout.tv_sec = newval / d;
	dmar_hw_timeout.tv_nsec = newval % d;
}

uint64_t
dmar_get_timeout(void)
{

	return ((uint64_t)dmar_hw_timeout.tv_sec * d +
	    dmar_hw_timeout.tv_nsec);
}

static int
dmar_timeout_sysctl(SYSCTL_HANDLER_ARGS)
{
	uint64_t val;
	int error;

	val = dmar_get_timeout();
	error = sysctl_handle_long(oidp, &val, 0, req);
	if (error != 0 || req->newptr == NULL)
		return (error);
	dmar_update_timeout(val);
	return (error);
}

SYSCTL_PROC(_hw_iommu_dmar, OID_AUTO, timeout,
    CTLTYPE_U64 | CTLFLAG_RW | CTLFLAG_MPSAFE, 0, 0,
    dmar_timeout_sysctl, "QU",
    "Timeout for command wait, in nanoseconds");
