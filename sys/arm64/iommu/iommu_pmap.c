/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020-2021 Ruslan Bukin <br@bsdpad.com>
 * Copyright (c) 2014-2021 Andrew Turner
 * Copyright (c) 2014-2016 The FreeBSD Foundation
 * All rights reserved.
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
/*
 *	Manages physical address maps for ARM SMMUv3 and ARM Mali GPU.
 */

#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_pageout.h>
#include <vm/vm_radix.h>

#include <machine/machdep.h>

#include <arm64/iommu/iommu_pmap.h>
#include <arm64/iommu/iommu_pte.h>

#define	IOMMU_PAGE_SIZE		4096

#define	SMMU_PMAP_LOCK(pmap)	mtx_lock(&(pmap)->sp_mtx)
#define	SMMU_PMAP_UNLOCK(pmap)	mtx_unlock(&(pmap)->sp_mtx)
#define	SMMU_PMAP_LOCK_ASSERT(pmap, type) \
    mtx_assert(&(pmap)->sp_mtx, (type))

#define	NL0PG		(IOMMU_PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL1PG		(IOMMU_PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL2PG		(IOMMU_PAGE_SIZE/(sizeof (pd_entry_t)))
#define	NL3PG		(IOMMU_PAGE_SIZE/(sizeof (pt_entry_t)))

#define	NUL0E		IOMMU_L0_ENTRIES
#define	NUL1E		(NUL0E * NL1PG)
#define	NUL2E		(NUL1E * NL2PG)

#define	smmu_l0_pindex(v)	(NUL2E + NUL1E + ((v) >> IOMMU_L0_SHIFT))
#define	smmu_l1_pindex(v)	(NUL2E + ((v) >> IOMMU_L1_SHIFT))
#define	smmu_l2_pindex(v)	((v) >> IOMMU_L2_SHIFT)

#define	smmu_l0_index(va)	(((va) >> IOMMU_L0_SHIFT) & IOMMU_L0_ADDR_MASK)
#define	smmu_l1_index(va)	(((va) >> IOMMU_L1_SHIFT) & IOMMU_Ln_ADDR_MASK)
#define	smmu_l2_index(va)	(((va) >> IOMMU_L2_SHIFT) & IOMMU_Ln_ADDR_MASK)
#define	smmu_l3_index(va)	(((va) >> IOMMU_L3_SHIFT) & IOMMU_Ln_ADDR_MASK)

static vm_page_t _pmap_alloc_l3(struct smmu_pmap *pmap, vm_pindex_t ptepindex);
static void _smmu_pmap_unwire_l3(struct smmu_pmap *pmap, vm_offset_t va,
    vm_page_t m, struct spglist *free);

/*
 * These load the old table data and store the new value.
 * They need to be atomic as the System MMU may write to the table at
 * the same time as the CPU.
 */
#define	smmu_pmap_load(table)		(*table)
#define	smmu_pmap_clear(table)		atomic_store_64(table, 0)
#define	smmu_pmap_store(table, entry)	atomic_store_64(table, entry)

/********************/
/* Inline functions */
/********************/

static __inline pd_entry_t *
smmu_pmap_l0(struct smmu_pmap *pmap, vm_offset_t va)
{

	return (&pmap->sp_l0[smmu_l0_index(va)]);
}

static __inline pd_entry_t *
smmu_pmap_l0_to_l1(pd_entry_t *l0, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = (pd_entry_t *)PHYS_TO_DMAP(smmu_pmap_load(l0) & ~ATTR_MASK);
	return (&l1[smmu_l1_index(va)]);
}

static __inline pd_entry_t *
smmu_pmap_l1(struct smmu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *l0;

	l0 = smmu_pmap_l0(pmap, va);
	if ((smmu_pmap_load(l0) & ATTR_DESCR_MASK) != IOMMU_L0_TABLE)
		return (NULL);

	return (smmu_pmap_l0_to_l1(l0, va));
}

static __inline pd_entry_t *
smmu_pmap_l1_to_l2(pd_entry_t *l1p, vm_offset_t va)
{
	pd_entry_t l1, *l2p;

	l1 = smmu_pmap_load(l1p);

	/*
	 * The valid bit may be clear if pmap_update_entry() is concurrently
	 * modifying the entry, so for KVA only the entry type may be checked.
	 */
	KASSERT(va >= VM_MAX_USER_ADDRESS || (l1 & ATTR_DESCR_VALID) != 0,
	    ("%s: L1 entry %#lx for %#lx is invalid", __func__, l1, va));
	KASSERT((l1 & ATTR_DESCR_TYPE_MASK) == ATTR_DESCR_TYPE_TABLE,
	    ("%s: L1 entry %#lx for %#lx is a leaf", __func__, l1, va));
	l2p = (pd_entry_t *)PHYS_TO_DMAP(l1 & ~ATTR_MASK);
	return (&l2p[smmu_l2_index(va)]);
}

static __inline pd_entry_t *
smmu_pmap_l2(struct smmu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = smmu_pmap_l1(pmap, va);
	if ((smmu_pmap_load(l1) & ATTR_DESCR_MASK) != IOMMU_L1_TABLE)
		return (NULL);

	return (smmu_pmap_l1_to_l2(l1, va));
}

static __inline pt_entry_t *
smmu_pmap_l2_to_l3(pd_entry_t *l2p, vm_offset_t va)
{
	pd_entry_t l2;
	pt_entry_t *l3p;

	l2 = smmu_pmap_load(l2p);

	/*
	 * The valid bit may be clear if pmap_update_entry() is concurrently
	 * modifying the entry, so for KVA only the entry type may be checked.
	 */
	KASSERT(va >= VM_MAX_USER_ADDRESS || (l2 & ATTR_DESCR_VALID) != 0,
	    ("%s: L2 entry %#lx for %#lx is invalid", __func__, l2, va));
	KASSERT((l2 & ATTR_DESCR_TYPE_MASK) == ATTR_DESCR_TYPE_TABLE,
	    ("%s: L2 entry %#lx for %#lx is a leaf", __func__, l2, va));
	l3p = (pt_entry_t *)PHYS_TO_DMAP(l2 & ~ATTR_MASK);
	return (&l3p[smmu_l3_index(va)]);
}

/*
 * Returns the lowest valid pde for a given virtual address.
 * The next level may or may not point to a valid page or block.
 */
static __inline pd_entry_t *
smmu_pmap_pde(struct smmu_pmap *pmap, vm_offset_t va, int *level)
{
	pd_entry_t *l0, *l1, *l2, desc;

	l0 = smmu_pmap_l0(pmap, va);
	desc = smmu_pmap_load(l0) & ATTR_DESCR_MASK;
	if (desc != IOMMU_L0_TABLE) {
		*level = -1;
		return (NULL);
	}

	l1 = smmu_pmap_l0_to_l1(l0, va);
	desc = smmu_pmap_load(l1) & ATTR_DESCR_MASK;
	if (desc != IOMMU_L1_TABLE) {
		*level = 0;
		return (l0);
	}

	l2 = smmu_pmap_l1_to_l2(l1, va);
	desc = smmu_pmap_load(l2) & ATTR_DESCR_MASK;
	if (desc != IOMMU_L2_TABLE) {
		*level = 1;
		return (l1);
	}

	*level = 2;
	return (l2);
}

/*
 * Returns the lowest valid pte block or table entry for a given virtual
 * address. If there are no valid entries return NULL and set the level to
 * the first invalid level.
 */
static __inline pt_entry_t *
smmu_pmap_pte(struct smmu_pmap *pmap, vm_offset_t va, int *level)
{
	pd_entry_t *l1, *l2, desc;
	pt_entry_t *l3;

	l1 = smmu_pmap_l1(pmap, va);
	if (l1 == NULL) {
		*level = 0;
		return (NULL);
	}
	desc = smmu_pmap_load(l1) & ATTR_DESCR_MASK;
	if (desc == IOMMU_L1_BLOCK) {
		*level = 1;
		return (l1);
	}

	if (desc != IOMMU_L1_TABLE) {
		*level = 1;
		return (NULL);
	}

	l2 = smmu_pmap_l1_to_l2(l1, va);
	desc = smmu_pmap_load(l2) & ATTR_DESCR_MASK;
	if (desc == IOMMU_L2_BLOCK) {
		*level = 2;
		return (l2);
	}

	if (desc != IOMMU_L2_TABLE) {
		*level = 2;
		return (NULL);
	}

	*level = 3;
	l3 = smmu_pmap_l2_to_l3(l2, va);
	if ((smmu_pmap_load(l3) & ATTR_DESCR_MASK) != IOMMU_L3_PAGE)
		return (NULL);

	return (l3);
}

static __inline int
smmu_pmap_l3_valid(pt_entry_t l3)
{

	return ((l3 & ATTR_DESCR_MASK) == IOMMU_L3_PAGE);
}

CTASSERT(IOMMU_L1_BLOCK == IOMMU_L2_BLOCK);

#ifdef INVARIANTS
static __inline void
smmu_pmap_resident_count_inc(struct smmu_pmap *pmap, int count)
{

	SMMU_PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pmap->sp_resident_count += count;
}

static __inline void
smmu_pmap_resident_count_dec(struct smmu_pmap *pmap, int count)
{

	SMMU_PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(pmap->sp_resident_count >= count,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->sp_resident_count, count));
	pmap->sp_resident_count -= count;
}
#else
static __inline void
smmu_pmap_resident_count_inc(struct smmu_pmap *pmap, int count)
{
}

static __inline void
smmu_pmap_resident_count_dec(struct smmu_pmap *pmap, int count)
{
}
#endif

/***************************************************
 * Page table page management routines.....
 ***************************************************/
/*
 * Schedule the specified unused page table page to be freed.  Specifically,
 * add the page to the specified list of pages that will be released to the
 * physical memory manager after the TLB has been updated.
 */
static __inline void
smmu_pmap_add_delayed_free_list(vm_page_t m, struct spglist *free,
    boolean_t set_PG_ZERO)
{

	if (set_PG_ZERO)
		m->flags |= PG_ZERO;
	else
		m->flags &= ~PG_ZERO;
	SLIST_INSERT_HEAD(free, m, plinks.s.ss);
}

/***************************************************
 * Low level mapping routines.....
 ***************************************************/

/*
 * Decrements a page table page's reference count, which is used to record the
 * number of valid page table entries within the page.  If the reference count
 * drops to zero, then the page table page is unmapped.  Returns TRUE if the
 * page table page was unmapped and FALSE otherwise.
 */
static inline boolean_t
smmu_pmap_unwire_l3(struct smmu_pmap *pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free)
{

	--m->ref_count;
	if (m->ref_count == 0) {
		_smmu_pmap_unwire_l3(pmap, va, m, free);
		return (TRUE);
	} else
		return (FALSE);
}

static void
_smmu_pmap_unwire_l3(struct smmu_pmap *pmap, vm_offset_t va, vm_page_t m,
    struct spglist *free)
{

	SMMU_PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	/*
	 * unmap the page table page
	 */
	if (m->pindex >= (NUL2E + NUL1E)) {
		/* l1 page */
		pd_entry_t *l0;

		l0 = smmu_pmap_l0(pmap, va);
		smmu_pmap_clear(l0);
	} else if (m->pindex >= NUL2E) {
		/* l2 page */
		pd_entry_t *l1;

		l1 = smmu_pmap_l1(pmap, va);
		smmu_pmap_clear(l1);
	} else {
		/* l3 page */
		pd_entry_t *l2;

		l2 = smmu_pmap_l2(pmap, va);
		smmu_pmap_clear(l2);
	}
	smmu_pmap_resident_count_dec(pmap, 1);
	if (m->pindex < NUL2E) {
		/* We just released an l3, unhold the matching l2 */
		pd_entry_t *l1, tl1;
		vm_page_t l2pg;

		l1 = smmu_pmap_l1(pmap, va);
		tl1 = smmu_pmap_load(l1);
		l2pg = PHYS_TO_VM_PAGE(tl1 & ~ATTR_MASK);
		smmu_pmap_unwire_l3(pmap, va, l2pg, free);
	} else if (m->pindex < (NUL2E + NUL1E)) {
		/* We just released an l2, unhold the matching l1 */
		pd_entry_t *l0, tl0;
		vm_page_t l1pg;

		l0 = smmu_pmap_l0(pmap, va);
		tl0 = smmu_pmap_load(l0);
		l1pg = PHYS_TO_VM_PAGE(tl0 & ~ATTR_MASK);
		smmu_pmap_unwire_l3(pmap, va, l1pg, free);
	}

	/*
	 * Put page on a list so that it is released after
	 * *ALL* TLB shootdown is done
	 */
	smmu_pmap_add_delayed_free_list(m, free, TRUE);
}

int
smmu_pmap_pinit(struct smmu_pmap *pmap)
{
	vm_page_t m;

	/*
	 * allocate the l0 page
	 */
	m = vm_page_alloc_noobj(VM_ALLOC_WAITOK | VM_ALLOC_WIRED |
	    VM_ALLOC_ZERO);
	pmap->sp_l0_paddr = VM_PAGE_TO_PHYS(m);
	pmap->sp_l0 = (pd_entry_t *)PHYS_TO_DMAP(pmap->sp_l0_paddr);

#ifdef INVARIANTS
	pmap->sp_resident_count = 0;
#endif
	mtx_init(&pmap->sp_mtx, "smmu pmap", NULL, MTX_DEF);

	return (1);
}

/*
 * This routine is called if the desired page table page does not exist.
 *
 * If page table page allocation fails, this routine may sleep before
 * returning NULL.  It sleeps only if a lock pointer was given.
 *
 * Note: If a page allocation fails at page table level two or three,
 * one or two pages may be held during the wait, only to be released
 * afterwards.  This conservative approach is easily argued to avoid
 * race conditions.
 */
static vm_page_t
_pmap_alloc_l3(struct smmu_pmap *pmap, vm_pindex_t ptepindex)
{
	vm_page_t m, l1pg, l2pg;

	SMMU_PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Allocate a page table page.
	 */
	if ((m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO)) == NULL) {
		/*
		 * Indicate the need to retry.  While waiting, the page table
		 * page may have been allocated.
		 */
		return (NULL);
	}
	m->pindex = ptepindex;

	/*
	 * Because of AArch64's weak memory consistency model, we must have a
	 * barrier here to ensure that the stores for zeroing "m", whether by
	 * pmap_zero_page() or an earlier function, are visible before adding
	 * "m" to the page table.  Otherwise, a page table walk by another
	 * processor's MMU could see the mapping to "m" and a stale, non-zero
	 * PTE within "m".
	 */
	dmb(ishst);

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */

	if (ptepindex >= (NUL2E + NUL1E)) {
		pd_entry_t *l0;
		vm_pindex_t l0index;

		l0index = ptepindex - (NUL2E + NUL1E);
		l0 = &pmap->sp_l0[l0index];
		smmu_pmap_store(l0, VM_PAGE_TO_PHYS(m) | IOMMU_L0_TABLE);
	} else if (ptepindex >= NUL2E) {
		vm_pindex_t l0index, l1index;
		pd_entry_t *l0, *l1;
		pd_entry_t tl0;

		l1index = ptepindex - NUL2E;
		l0index = l1index >> IOMMU_L0_ENTRIES_SHIFT;

		l0 = &pmap->sp_l0[l0index];
		tl0 = smmu_pmap_load(l0);
		if (tl0 == 0) {
			/* recurse for allocating page dir */
			if (_pmap_alloc_l3(pmap, NUL2E + NUL1E + l0index)
			    == NULL) {
				vm_page_unwire_noq(m);
				vm_page_free_zero(m);
				return (NULL);
			}
		} else {
			l1pg = PHYS_TO_VM_PAGE(tl0 & ~ATTR_MASK);
			l1pg->ref_count++;
		}

		l1 = (pd_entry_t *)PHYS_TO_DMAP(smmu_pmap_load(l0) &~ATTR_MASK);
		l1 = &l1[ptepindex & Ln_ADDR_MASK];
		smmu_pmap_store(l1, VM_PAGE_TO_PHYS(m) | IOMMU_L1_TABLE);
	} else {
		vm_pindex_t l0index, l1index;
		pd_entry_t *l0, *l1, *l2;
		pd_entry_t tl0, tl1;

		l1index = ptepindex >> IOMMU_Ln_ENTRIES_SHIFT;
		l0index = l1index >> IOMMU_L0_ENTRIES_SHIFT;

		l0 = &pmap->sp_l0[l0index];
		tl0 = smmu_pmap_load(l0);
		if (tl0 == 0) {
			/* recurse for allocating page dir */
			if (_pmap_alloc_l3(pmap, NUL2E + l1index) == NULL) {
				vm_page_unwire_noq(m);
				vm_page_free_zero(m);
				return (NULL);
			}
			tl0 = smmu_pmap_load(l0);
			l1 = (pd_entry_t *)PHYS_TO_DMAP(tl0 & ~ATTR_MASK);
			l1 = &l1[l1index & Ln_ADDR_MASK];
		} else {
			l1 = (pd_entry_t *)PHYS_TO_DMAP(tl0 & ~ATTR_MASK);
			l1 = &l1[l1index & Ln_ADDR_MASK];
			tl1 = smmu_pmap_load(l1);
			if (tl1 == 0) {
				/* recurse for allocating page dir */
				if (_pmap_alloc_l3(pmap, NUL2E + l1index)
				    == NULL) {
					vm_page_unwire_noq(m);
					vm_page_free_zero(m);
					return (NULL);
				}
			} else {
				l2pg = PHYS_TO_VM_PAGE(tl1 & ~ATTR_MASK);
				l2pg->ref_count++;
			}
		}

		l2 = (pd_entry_t *)PHYS_TO_DMAP(smmu_pmap_load(l1) &~ATTR_MASK);
		l2 = &l2[ptepindex & Ln_ADDR_MASK];
		smmu_pmap_store(l2, VM_PAGE_TO_PHYS(m) | IOMMU_L2_TABLE);
	}

	smmu_pmap_resident_count_inc(pmap, 1);

	return (m);
}

/***************************************************
 * Pmap allocation/deallocation routines.
 ***************************************************/

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
smmu_pmap_release(struct smmu_pmap *pmap)
{
	vm_page_t m;

	KASSERT(pmap->sp_resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->sp_resident_count));

	m = PHYS_TO_VM_PAGE(pmap->sp_l0_paddr);
	vm_page_unwire_noq(m);
	vm_page_free_zero(m);
	mtx_destroy(&pmap->sp_mtx);
}

/***************************************************
 * page management routines.
 ***************************************************/

/*
 * Add a single Mali GPU entry. This function does not sleep.
 */
int
pmap_gpu_enter(struct smmu_pmap *pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags)
{
	pd_entry_t *pde;
	pt_entry_t new_l3;
	pt_entry_t orig_l3 __diagused;
	pt_entry_t *l3;
	vm_page_t mpte;
	pd_entry_t *l1p;
	pd_entry_t *l2p;
	int lvl;
	int rv;

	KASSERT(va < VM_MAXUSER_ADDRESS, ("wrong address space"));
	KASSERT((va & PAGE_MASK) == 0, ("va is misaligned"));
	KASSERT((pa & PAGE_MASK) == 0, ("pa is misaligned"));

	new_l3 = (pt_entry_t)(pa | ATTR_SH(ATTR_SH_IS) | IOMMU_L3_BLOCK);

	if ((prot & VM_PROT_WRITE) != 0)
		new_l3 |= ATTR_S2_S2AP(ATTR_S2_S2AP_WRITE);
	if ((prot & VM_PROT_READ) != 0)
		new_l3 |= ATTR_S2_S2AP(ATTR_S2_S2AP_READ);
	if ((prot & VM_PROT_EXECUTE) == 0)
		new_l3 |= ATTR_S2_XN(ATTR_S2_XN_ALL);

	CTR2(KTR_PMAP, "pmap_gpu_enter: %.16lx -> %.16lx", va, pa);

	SMMU_PMAP_LOCK(pmap);

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
retry:
	pde = smmu_pmap_pde(pmap, va, &lvl);
	if (pde != NULL && lvl == 2) {
		l3 = smmu_pmap_l2_to_l3(pde, va);
	} else {
		mpte = _pmap_alloc_l3(pmap, smmu_l2_pindex(va));
		if (mpte == NULL) {
			CTR0(KTR_PMAP, "pmap_enter: mpte == NULL");
			rv = KERN_RESOURCE_SHORTAGE;
			goto out;
		}

		/*
		 * Ensure newly created l1, l2 are visible to GPU.
		 * l0 is already visible by similar call in panfrost driver.
		 * The cache entry for l3 handled below.
		 */

		l1p = smmu_pmap_l1(pmap, va);
		l2p = smmu_pmap_l2(pmap, va);
		cpu_dcache_wb_range(l1p, sizeof(pd_entry_t));
		cpu_dcache_wb_range(l2p, sizeof(pd_entry_t));

		goto retry;
	}

	orig_l3 = smmu_pmap_load(l3);
	KASSERT(!smmu_pmap_l3_valid(orig_l3), ("l3 is valid"));

	/* New mapping */
	smmu_pmap_store(l3, new_l3);

	cpu_dcache_wb_range(l3, sizeof(pt_entry_t));

	smmu_pmap_resident_count_inc(pmap, 1);
	dsb(ishst);

	rv = KERN_SUCCESS;
out:
	SMMU_PMAP_UNLOCK(pmap);

	return (rv);
}

/*
 * Remove a single Mali GPU entry.
 */
int
pmap_gpu_remove(struct smmu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *pde;
	pt_entry_t *pte;
	int lvl;
	int rc;

	KASSERT((va & PAGE_MASK) == 0, ("va is misaligned"));

	SMMU_PMAP_LOCK(pmap);

	pde = smmu_pmap_pde(pmap, va, &lvl);
	if (pde == NULL || lvl != 2) {
		rc = KERN_FAILURE;
		goto out;
	}

	pte = smmu_pmap_l2_to_l3(pde, va);

	smmu_pmap_resident_count_dec(pmap, 1);
	smmu_pmap_clear(pte);
	cpu_dcache_wb_range(pte, sizeof(pt_entry_t));
	rc = KERN_SUCCESS;

out:
	SMMU_PMAP_UNLOCK(pmap);

	return (rc);
}

/*
 * Add a single SMMU entry. This function does not sleep.
 */
int
smmu_pmap_enter(struct smmu_pmap *pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags)
{
	pd_entry_t *pde;
	pt_entry_t new_l3;
	pt_entry_t orig_l3 __diagused;
	pt_entry_t *l3;
	vm_page_t mpte;
	int lvl;
	int rv;

	KASSERT(va < VM_MAXUSER_ADDRESS, ("wrong address space"));

	va = trunc_page(va);
	new_l3 = (pt_entry_t)(pa | ATTR_AF | ATTR_SH(ATTR_SH_IS) |
	    ATTR_S1_IDX(VM_MEMATTR_DEVICE) | IOMMU_L3_PAGE);
	if ((prot & VM_PROT_WRITE) == 0)
		new_l3 |= ATTR_S1_AP(ATTR_S1_AP_RO);
	new_l3 |= ATTR_S1_XN; /* Execute never. */
	new_l3 |= ATTR_S1_AP(ATTR_S1_AP_USER);
	new_l3 |= ATTR_S1_nG; /* Non global. */

	CTR2(KTR_PMAP, "pmap_senter: %.16lx -> %.16lx", va, pa);

	SMMU_PMAP_LOCK(pmap);

	/*
	 * In the case that a page table page is not
	 * resident, we are creating it here.
	 */
retry:
	pde = smmu_pmap_pde(pmap, va, &lvl);
	if (pde != NULL && lvl == 2) {
		l3 = smmu_pmap_l2_to_l3(pde, va);
	} else {
		mpte = _pmap_alloc_l3(pmap, smmu_l2_pindex(va));
		if (mpte == NULL) {
			CTR0(KTR_PMAP, "pmap_enter: mpte == NULL");
			rv = KERN_RESOURCE_SHORTAGE;
			goto out;
		}
		goto retry;
	}

	orig_l3 = smmu_pmap_load(l3);
	KASSERT(!smmu_pmap_l3_valid(orig_l3), ("l3 is valid"));

	/* New mapping */
	smmu_pmap_store(l3, new_l3);
	smmu_pmap_resident_count_inc(pmap, 1);
	dsb(ishst);

	rv = KERN_SUCCESS;
out:
	SMMU_PMAP_UNLOCK(pmap);

	return (rv);
}

/*
 * Remove a single SMMU entry.
 */
int
smmu_pmap_remove(struct smmu_pmap *pmap, vm_offset_t va)
{
	pt_entry_t *pte;
	int lvl;
	int rc;

	SMMU_PMAP_LOCK(pmap);

	pte = smmu_pmap_pte(pmap, va, &lvl);
	KASSERT(lvl == 3,
	    ("Invalid SMMU pagetable level: %d != 3", lvl));

	if (pte != NULL) {
		smmu_pmap_resident_count_dec(pmap, 1);
		smmu_pmap_clear(pte);
		rc = KERN_SUCCESS;
	} else
		rc = KERN_FAILURE;

	SMMU_PMAP_UNLOCK(pmap);

	return (rc);
}

/*
 * Remove all the allocated L1, L2 pages from SMMU pmap.
 * All the L3 entires must be cleared in advance, otherwise
 * this function panics.
 */
void
smmu_pmap_remove_pages(struct smmu_pmap *pmap)
{
	pd_entry_t l0e, *l1, l1e, *l2, l2e;
	pt_entry_t *l3, l3e;
	vm_page_t m, m0, m1;
	vm_paddr_t pa;
	vm_paddr_t pa0;
	vm_paddr_t pa1;
	int i, j, k, l;

	SMMU_PMAP_LOCK(pmap);

	for (i = 0; i < IOMMU_L0_ENTRIES; i++) {
		l0e = pmap->sp_l0[i];
		if ((l0e & ATTR_DESCR_VALID) == 0) {
			continue;
		}
		pa0 = l0e & ~ATTR_MASK;
		m0 = PHYS_TO_VM_PAGE(pa0);
		l1 = (pd_entry_t *)PHYS_TO_DMAP(pa0);

		for (j = 0; j < IOMMU_Ln_ENTRIES; j++) {
			l1e = l1[j];
			if ((l1e & ATTR_DESCR_VALID) == 0) {
				continue;
			}
			if ((l1e & ATTR_DESCR_MASK) == IOMMU_L1_BLOCK) {
				continue;
			}
			pa1 = l1e & ~ATTR_MASK;
			m1 = PHYS_TO_VM_PAGE(pa1);
			l2 = (pd_entry_t *)PHYS_TO_DMAP(pa1);

			for (k = 0; k < IOMMU_Ln_ENTRIES; k++) {
				l2e = l2[k];
				if ((l2e & ATTR_DESCR_VALID) == 0) {
					continue;
				}
				pa = l2e & ~ATTR_MASK;
				m = PHYS_TO_VM_PAGE(pa);
				l3 = (pt_entry_t *)PHYS_TO_DMAP(pa);

				for (l = 0; l < IOMMU_Ln_ENTRIES; l++) {
					l3e = l3[l];
					if ((l3e & ATTR_DESCR_VALID) == 0)
						continue;
					panic(
					  "%s: l3e found (indexes %d %d %d %d)",
					    __func__, i, j, k, l);
				}

				vm_page_unwire_noq(m1);
				vm_page_unwire_noq(m);
				smmu_pmap_resident_count_dec(pmap, 1);
				vm_page_free(m);
				smmu_pmap_clear(&l2[k]);
			}

			vm_page_unwire_noq(m0);
			smmu_pmap_resident_count_dec(pmap, 1);
			vm_page_free(m1);
			smmu_pmap_clear(&l1[j]);
		}

		smmu_pmap_resident_count_dec(pmap, 1);
		vm_page_free(m0);
		smmu_pmap_clear(&pmap->sp_l0[i]);
	}

	KASSERT(pmap->sp_resident_count == 0,
	    ("Invalid resident count %jd", pmap->sp_resident_count));

	SMMU_PMAP_UNLOCK(pmap);
}
