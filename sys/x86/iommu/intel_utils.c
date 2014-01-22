/*-
 * Copyright (c) 2013 The FreeBSD Foundation
 * All rights reserved.
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

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
#include <sys/tree.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_pageout.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <x86/include/busdma_impl.h>
#include <x86/iommu/intel_reg.h>
#include <x86/iommu/busdma_dmar.h>
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
	    .pglvl = 5},
	{.agaw = 64, .cap = DMAR_CAP_SAGAW_6LVL, .awlvl = DMAR_CTX2_AW_6LVL,
	    .pglvl = 6}
};
#define SIZEOF_SAGAW_BITS (sizeof(sagaw_bits) / sizeof(sagaw_bits[0]))

bool
dmar_pglvl_supported(struct dmar_unit *unit, int pglvl)
{
	int i;

	for (i = 0; i < SIZEOF_SAGAW_BITS; i++) {
		if (sagaw_bits[i].pglvl != pglvl)
			continue;
		if ((DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap) != 0)
			return (true);
	}
	return (false);
}

int
ctx_set_agaw(struct dmar_ctx *ctx, int mgaw)
{
	int sagaw, i;

	ctx->mgaw = mgaw;
	sagaw = DMAR_CAP_SAGAW(ctx->dmar->hw_cap);
	for (i = 0; i < SIZEOF_SAGAW_BITS; i++) {
		if (sagaw_bits[i].agaw >= mgaw) {
			ctx->agaw = sagaw_bits[i].agaw;
			ctx->pglvl = sagaw_bits[i].pglvl;
			ctx->awlvl = sagaw_bits[i].awlvl;
			return (0);
		}
	}
	device_printf(ctx->dmar->dev,
	    "context request mgaw %d for pci%d:%d:%d:%d, "
	    "no agaw found, sagaw %x\n", mgaw, ctx->dmar->segment, ctx->bus,
	     ctx->slot, ctx->func, sagaw);
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
dmar_maxaddr2mgaw(struct dmar_unit *unit, dmar_gaddr_t maxaddr, bool allow_less)
{
	int i;

	for (i = 0; i < SIZEOF_SAGAW_BITS; i++) {
		if ((1ULL << sagaw_bits[i].agaw) >= maxaddr &&
		    (DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap) != 0)
			break;
	}
	if (allow_less && i == SIZEOF_SAGAW_BITS) {
		do {
			i--;
		} while ((DMAR_CAP_SAGAW(unit->hw_cap) & sagaw_bits[i].cap)
		    == 0);
	}
	if (i < SIZEOF_SAGAW_BITS)
		return (sagaw_bits[i].agaw);
	KASSERT(0, ("no mgaw for maxaddr %jx allow_less %d",
	    (uintmax_t) maxaddr, allow_less));
	return (-1);
}

/*
 * Calculate the total amount of page table pages needed to map the
 * whole bus address space on the context with the selected agaw.
 */
vm_pindex_t
pglvl_max_pages(int pglvl)
{
	vm_pindex_t res;
	int i;

	for (res = 0, i = pglvl; i > 0; i--) {
		res *= DMAR_NPTEPG;
		res++;
	}
	return (res);
}

/*
 * Return true if the page table level lvl supports the superpage for
 * the context ctx.
 */
int
ctx_is_sp_lvl(struct dmar_ctx *ctx, int lvl)
{
	int alvl, cap_sps;
	static const int sagaw_sp[] = {
		DMAR_CAP_SPS_2M,
		DMAR_CAP_SPS_1G,
		DMAR_CAP_SPS_512G,
		DMAR_CAP_SPS_1T
	};

	alvl = ctx->pglvl - lvl - 1;
	cap_sps = DMAR_CAP_SPS(ctx->dmar->hw_cap);
	return (alvl < sizeof(sagaw_sp) / sizeof(sagaw_sp[0]) &&
	    (sagaw_sp[alvl] & cap_sps) != 0);
}

dmar_gaddr_t
pglvl_page_size(int total_pglvl, int lvl)
{
	int rlvl;
	static const dmar_gaddr_t pg_sz[] = {
		(dmar_gaddr_t)DMAR_PAGE_SIZE,
		(dmar_gaddr_t)DMAR_PAGE_SIZE << DMAR_NPTEPGSHIFT,
		(dmar_gaddr_t)DMAR_PAGE_SIZE << (2 * DMAR_NPTEPGSHIFT),
		(dmar_gaddr_t)DMAR_PAGE_SIZE << (3 * DMAR_NPTEPGSHIFT),
		(dmar_gaddr_t)DMAR_PAGE_SIZE << (4 * DMAR_NPTEPGSHIFT),
		(dmar_gaddr_t)DMAR_PAGE_SIZE << (5 * DMAR_NPTEPGSHIFT)
	};

	KASSERT(lvl >= 0 && lvl < total_pglvl,
	    ("total %d lvl %d", total_pglvl, lvl));
	rlvl = total_pglvl - lvl - 1;
	KASSERT(rlvl < sizeof(pg_sz) / sizeof(pg_sz[0]),
	    ("sizeof pg_sz lvl %d", lvl));
	return (pg_sz[rlvl]);
}

dmar_gaddr_t
ctx_page_size(struct dmar_ctx *ctx, int lvl)
{

	return (pglvl_page_size(ctx->pglvl, lvl));
}

int
calc_am(struct dmar_unit *unit, dmar_gaddr_t base, dmar_gaddr_t size,
    dmar_gaddr_t *isizep)
{
	dmar_gaddr_t isize;
	int am;

	for (am = DMAR_CAP_MAMV(unit->hw_cap);; am--) {
		isize = 1ULL << (am + DMAR_PAGE_SHIFT);
		if ((base & (isize - 1)) == 0 && size >= isize)
			break;
		if (am == 0)
			break;
	}
	*isizep = isize;
	return (am);
}

dmar_haddr_t dmar_high;
int haw;
int dmar_tbl_pagecnt;

vm_page_t
dmar_pgalloc(vm_object_t obj, vm_pindex_t idx, int flags)
{
	vm_page_t m;
	int zeroed;

	zeroed = (flags & DMAR_PGF_ZERO) != 0 ? VM_ALLOC_ZERO : 0;
	for (;;) {
		if ((flags & DMAR_PGF_OBJL) == 0)
			VM_OBJECT_WLOCK(obj);
		m = vm_page_lookup(obj, idx);
		if ((flags & DMAR_PGF_NOALLOC) != 0 || m != NULL) {
			if ((flags & DMAR_PGF_OBJL) == 0)
				VM_OBJECT_WUNLOCK(obj);
			break;
		}
		m = vm_page_alloc_contig(obj, idx, VM_ALLOC_NOBUSY |
		    VM_ALLOC_SYSTEM | VM_ALLOC_NODUMP | zeroed, 1, 0,
		    dmar_high, PAGE_SIZE, 0, VM_MEMATTR_DEFAULT);
		if ((flags & DMAR_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		if (m != NULL) {
			if (zeroed && (m->flags & PG_ZERO) == 0)
				pmap_zero_page(m);
			atomic_add_int(&dmar_tbl_pagecnt, 1);
			break;
		}
		if ((flags & DMAR_PGF_WAITOK) == 0)
			break;
		if ((flags & DMAR_PGF_OBJL) != 0)
			VM_OBJECT_WUNLOCK(obj);
		VM_WAIT;
		if ((flags & DMAR_PGF_OBJL) != 0)
			VM_OBJECT_WLOCK(obj);
	}
	return (m);
}

void
dmar_pgfree(vm_object_t obj, vm_pindex_t idx, int flags)
{
	vm_page_t m;

	if ((flags & DMAR_PGF_OBJL) == 0)
		VM_OBJECT_WLOCK(obj);
	m = vm_page_lookup(obj, idx);
	if (m != NULL) {
		vm_page_free(m);
		atomic_subtract_int(&dmar_tbl_pagecnt, 1);
	}
	if ((flags & DMAR_PGF_OBJL) == 0)
		VM_OBJECT_WUNLOCK(obj);
}

void *
dmar_map_pgtbl(vm_object_t obj, vm_pindex_t idx, int flags,
    struct sf_buf **sf)
{
	vm_page_t m;
	bool allocated;

	if ((flags & DMAR_PGF_OBJL) == 0)
		VM_OBJECT_WLOCK(obj);
	m = vm_page_lookup(obj, idx);
	if (m == NULL && (flags & DMAR_PGF_ALLOC) != 0) {
		m = dmar_pgalloc(obj, idx, flags | DMAR_PGF_OBJL);
		allocated = true;
	} else
		allocated = false;
	if (m == NULL) {
		if ((flags & DMAR_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		return (NULL);
	}
	/* Sleepable allocations cannot fail. */
	if ((flags & DMAR_PGF_WAITOK) != 0)
		VM_OBJECT_WUNLOCK(obj);
	sched_pin();
	*sf = sf_buf_alloc(m, SFB_CPUPRIVATE | ((flags & DMAR_PGF_WAITOK)
	    == 0 ? SFB_NOWAIT : 0));
	if (*sf == NULL) {
		sched_unpin();
		if (allocated) {
			VM_OBJECT_ASSERT_WLOCKED(obj);
			dmar_pgfree(obj, m->pindex, flags | DMAR_PGF_OBJL);
		}
		if ((flags & DMAR_PGF_OBJL) == 0)
			VM_OBJECT_WUNLOCK(obj);
		return (NULL);
	}
	if ((flags & (DMAR_PGF_WAITOK | DMAR_PGF_OBJL)) ==
	    (DMAR_PGF_WAITOK | DMAR_PGF_OBJL))
		VM_OBJECT_WLOCK(obj);
	else if ((flags & (DMAR_PGF_WAITOK | DMAR_PGF_OBJL)) == 0)
		VM_OBJECT_WUNLOCK(obj);
	return ((void *)sf_buf_kva(*sf));
}

void
dmar_unmap_pgtbl(struct sf_buf *sf, bool coherent)
{
	vm_page_t m;

	m = sf_buf_page(sf);
	sf_buf_free(sf);
	sched_unpin();

	/*
	 * If DMAR does not snoop paging structures accesses, flush
	 * CPU cache to memory.
	 */
	if (!coherent)
		pmap_invalidate_cache_pages(&m, 1);
}

/*
 * Load the root entry pointer into the hardware, busily waiting for
 * the completion.
 */
int
dmar_load_root_entry_ptr(struct dmar_unit *unit)
{
	vm_page_t root_entry;

	/*
	 * Access to the GCMD register must be serialized while the
	 * command is submitted.
	 */
	DMAR_ASSERT_LOCKED(unit);

	/* VM_OBJECT_RLOCK(unit->ctx_obj); */
	VM_OBJECT_WLOCK(unit->ctx_obj);
	root_entry = vm_page_lookup(unit->ctx_obj, 0);
	/* VM_OBJECT_RUNLOCK(unit->ctx_obj); */
	VM_OBJECT_WUNLOCK(unit->ctx_obj);
	dmar_write8(unit, DMAR_RTADDR_REG, VM_PAGE_TO_PHYS(root_entry));
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd | DMAR_GCMD_SRTP);
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_RTPS) == 0)
		cpu_spinwait();
	return (0);
}

/*
 * Globally invalidate the context entries cache, busily waiting for
 * the completion.
 */
int
dmar_inv_ctx_glob(struct dmar_unit *unit)
{

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
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, DMAR_CCMD_REG + 4) & DMAR_CCMD_ICC32) != 0)
		cpu_spinwait();
	return (0);
}

/*
 * Globally invalidate the IOTLB, busily waiting for the completion.
 */
int
dmar_inv_iotlb_glob(struct dmar_unit *unit)
{
	int reg;

	DMAR_ASSERT_LOCKED(unit);
	KASSERT(!unit->qi_enabled, ("QI enabled"));

	reg = 16 * DMAR_ECAP_IRO(unit->hw_ecap);
	/* See a comment about DMAR_CCMD_ICC in dmar_inv_ctx_glob. */
	dmar_write8(unit, reg + DMAR_IOTLB_REG_OFF, DMAR_IOTLB_IVT |
	    DMAR_IOTLB_IIRG_GLB | DMAR_IOTLB_DR | DMAR_IOTLB_DW);
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, reg + DMAR_IOTLB_REG_OFF + 4) &
	    DMAR_IOTLB_IVT32) != 0)
		cpu_spinwait();
	return (0);
}

/*
 * Flush the chipset write buffers.  See 11.1 "Write Buffer Flushing"
 * in the architecture specification.
 */
int
dmar_flush_write_bufs(struct dmar_unit *unit)
{

	DMAR_ASSERT_LOCKED(unit);

	/*
	 * DMAR_GCMD_WBF is only valid when CAP_RWBF is reported.
	 */
	KASSERT((unit->hw_cap & DMAR_CAP_RWBF) != 0,
	    ("dmar%d: no RWBF", unit->unit));

	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd | DMAR_GCMD_WBF);
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_WBFS) == 0)
		cpu_spinwait();
	return (0);
}

int
dmar_enable_translation(struct dmar_unit *unit)
{

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd |= DMAR_GCMD_TE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_TES) == 0)
		cpu_spinwait();
	return (0);
}

int
dmar_disable_translation(struct dmar_unit *unit)
{

	DMAR_ASSERT_LOCKED(unit);
	unit->hw_gcmd &= ~DMAR_GCMD_TE;
	dmar_write4(unit, DMAR_GCMD_REG, unit->hw_gcmd);
	/* XXXKIB should have a timeout */
	while ((dmar_read4(unit, DMAR_GSTS_REG) & DMAR_GSTS_TES) != 0)
		cpu_spinwait();
	return (0);
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
			msleep(&dmar->barrier_flags, &dmar->lock, 0,
			    "dmarb", 0);
		}
		KASSERT((dmar->barrier_flags & f_done) != 0,
		    ("dmar%d barrier %d missing done", dmar->unit, barrier_id));
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
	    ("dmar%d barrier %d missed entry", dmar->unit, barrier_id));
	dmar->barrier_flags |= f_done;
	if ((dmar->barrier_flags & f_wakeup) != 0)
		wakeup(&dmar->barrier_flags);
	dmar->barrier_flags &= ~(f_inproc | f_wakeup);
	DMAR_UNLOCK(dmar);
}

int dmar_match_verbose;

static SYSCTL_NODE(_hw, OID_AUTO, dmar, CTLFLAG_RD, NULL,
    "");
SYSCTL_INT(_hw_dmar, OID_AUTO, tbl_pagecnt, CTLFLAG_RD | CTLFLAG_TUN,
    &dmar_tbl_pagecnt, 0,
    "Count of pages used for DMAR pagetables");
SYSCTL_INT(_hw_dmar, OID_AUTO, match_verbose, CTLFLAG_RW | CTLFLAG_TUN,
    &dmar_match_verbose, 0,
    "Verbose matching of the PCI devices to DMAR paths");
#ifdef INVARIANTS
int dmar_check_free;
SYSCTL_INT(_hw_dmar, OID_AUTO, check_free, CTLFLAG_RW | CTLFLAG_TUN,
    &dmar_check_free, 0,
    "Check the GPA RBtree for free_down and free_after validity");
#endif

