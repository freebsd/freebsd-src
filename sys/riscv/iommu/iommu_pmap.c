/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 Ruslan Bukin <br@bsdpad.com>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/vm_page.h>

#include <riscv/iommu/iommu_pmap.h>

/*
 * Boundary values for the page table page index space:
 *
 * L3 pages: [0, NUL2E)
 * L2 pages: [NUL2E, NUL2E + NUL1E)
 * L1 pages: [NUL2E + NUL1E, NUL2E + NUL1E + NUL0E)
 *
 * Note that these ranges are used in both SV39 and SV48 mode.  In SV39 mode the
 * ranges are not fully populated since there are at most Ln_ENTRIES^2 L3 pages
 * in a set of page tables.
 */
#define	NUL0E		Ln_ENTRIES
#define	NUL1E		(Ln_ENTRIES * NUL0E)
#define	NUL2E		(Ln_ENTRIES * NUL1E)

#define	pmap_l1_pindex(v)	(NUL2E + ((v) >> L1_SHIFT))
#define	pmap_l2_pindex(v)	((v) >> L2_SHIFT)

#define	pmap_clear(pte)			pmap_store(pte, 0)
#define	pmap_clear_bits(pte, bits)	atomic_clear_64(pte, bits)
#define	pmap_load_store(pte, entry)	atomic_swap_64(pte, entry)
#define	pmap_load_clear(pte)		pmap_load_store(pte, 0)
#define	pmap_load(pte)			atomic_load_64(pte)
#define	pmap_store(pte, entry)		atomic_store_64(pte, entry)
#define	pmap_store_bits(pte, bits)	atomic_set_64(pte, bits)

#define	pmap_l0_index(va)	(((va) >> L0_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l1_index(va)	(((va) >> L1_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l2_index(va)	(((va) >> L2_SHIFT) & Ln_ADDR_MASK)
#define	pmap_l3_index(va)	(((va) >> L3_SHIFT) & Ln_ADDR_MASK)

#define	PTE_TO_PHYS(pte) \
    ((((pte) & ~PTE_HI_MASK) >> PTE_PPN0_S) * PAGE_SIZE)
#define	L2PTE_TO_PHYS(l2) \
    ((((l2) & ~PTE_HI_MASK) >> PTE_PPN1_S) << L2_SHIFT)
#define	L1PTE_TO_PHYS(l1) \
    ((((l1) & ~PTE_HI_MASK) >> PTE_PPN2_S) << L1_SHIFT)
#define PTE_TO_VM_PAGE(pte) PHYS_TO_VM_PAGE(PTE_TO_PHYS(pte))

/********************/
/* Inline functions */
/********************/

static __inline pd_entry_t *
pmap_l0(struct riscv_iommu_pmap *pmap, vm_offset_t va)
{
	KASSERT(pmap->pm_mode != PMAP_MODE_SV39,
	    ("%s: in SV39 mode", __func__));
	KASSERT(VIRT_IS_VALID(va),
	    ("%s: malformed virtual address %#lx", __func__, va));
	return (&pmap->pm_top[pmap_l0_index(va)]);
}

static __inline pd_entry_t *
pmap_l0_to_l1(struct riscv_iommu_pmap *pmap, pd_entry_t *l0, vm_offset_t va)
{
	vm_paddr_t phys;
	pd_entry_t *l1;

	KASSERT(pmap->pm_mode != PMAP_MODE_SV39,
	    ("%s: in SV39 mode", __func__));
	phys = PTE_TO_PHYS(pmap_load(l0));
	l1 = (pd_entry_t *)PHYS_TO_DMAP(phys);

	return (&l1[pmap_l1_index(va)]);
}

static __inline pd_entry_t *
pmap_l1(struct riscv_iommu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *l0;

	KASSERT(VIRT_IS_VALID(va),
	    ("%s: malformed virtual address %#lx", __func__, va));
	if (pmap->pm_mode == PMAP_MODE_SV39) {
		return (&pmap->pm_top[pmap_l1_index(va)]);
	} else {
		l0 = pmap_l0(pmap, va);
		if ((pmap_load(l0) & PTE_V) == 0)
			return (NULL);
		if ((pmap_load(l0) & PTE_RX) != 0)
			return (NULL);
		return (pmap_l0_to_l1(pmap, l0, va));
	}
}

static __inline pd_entry_t *
pmap_l1_to_l2(pd_entry_t *l1, vm_offset_t va)
{
	vm_paddr_t phys;
	pd_entry_t *l2;

	phys = PTE_TO_PHYS(pmap_load(l1));
	l2 = (pd_entry_t *)PHYS_TO_DMAP(phys);

	return (&l2[pmap_l2_index(va)]);
}

static __inline pd_entry_t *
pmap_l2(struct riscv_iommu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *l1;

	l1 = pmap_l1(pmap, va);
	if (l1 == NULL)
		return (NULL);
	if ((pmap_load(l1) & PTE_V) == 0)
		return (NULL);
	if ((pmap_load(l1) & PTE_RX) != 0)
		return (NULL);

	return (pmap_l1_to_l2(l1, va));
}

static __inline pt_entry_t *
pmap_l2_to_l3(pd_entry_t *l2, vm_offset_t va)
{
	vm_paddr_t phys;
	pt_entry_t *l3;

	phys = PTE_TO_PHYS(pmap_load(l2));
	l3 = (pd_entry_t *)PHYS_TO_DMAP(phys);

	return (&l3[pmap_l3_index(va)]);
}

static __inline pt_entry_t *
pmap_l3(struct riscv_iommu_pmap *pmap, vm_offset_t va)
{
	pd_entry_t *l2;

	l2 = pmap_l2(pmap, va);
	if (l2 == NULL)
		return (NULL);
	if ((pmap_load(l2) & PTE_V) == 0)
		return (NULL);
	if ((pmap_load(l2) & PTE_RX) != 0)
		return (NULL);

	return (pmap_l2_to_l3(l2, va));
}

static __inline void
pmap_resident_count_inc(struct riscv_iommu_pmap *pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	pmap->sp_resident_count += count;
}

static __inline void
pmap_resident_count_dec(struct riscv_iommu_pmap *pmap, int count)
{

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);
	KASSERT(pmap->sp_resident_count >= count,
	    ("pmap %p resident count underflow %ld %d", pmap,
	    pmap->sp_resident_count, count));
	pmap->sp_resident_count -= count;
}

/***************************************************
 * Page table page management routines.....
 ***************************************************/

int
iommu_pmap_pinit(struct riscv_iommu_pmap *pmap, enum pmap_mode pm_mode)
{
	vm_paddr_t topphys;
	vm_page_t m;

	m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO |
	    VM_ALLOC_WAITOK);
	topphys = VM_PAGE_TO_PHYS(m);
	pmap->pm_top = (pd_entry_t *)PHYS_TO_DMAP(topphys);
	pmap->pm_mode = pm_mode;

	switch (pm_mode) {
	case PMAP_MODE_SV39:
		pmap->pm_satp = SATP_MODE_SV39;
		break;
	case PMAP_MODE_SV48:
		pmap->pm_satp = SATP_MODE_SV48;
		break;
	default:
		panic("Unknown virtual memory system");
	};

	pmap->pm_satp |= (topphys >> PAGE_SHIFT);

#ifdef INVARIANTS
	pmap->sp_resident_count = 0;
#endif

	mtx_init(&pmap->pm_mtx, "iommu pmap", NULL, MTX_DEF);

	return (1);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by pmap_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
void
iommu_pmap_release(struct riscv_iommu_pmap *pmap)
{
	vm_page_t m;

	KASSERT(pmap->sp_resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->sp_resident_count));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pmap->pm_top));
	vm_page_unwire_noq(m);
	vm_page_free_zero(m);
	mtx_destroy(&pmap->pm_mtx);
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
_pmap_alloc_l3(struct riscv_iommu_pmap *pmap, vm_pindex_t ptepindex)
{
	vm_page_t m, pdpg;
	pt_entry_t entry;
	vm_paddr_t phys;
	pn_t pn;

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	/*
	 * Allocate a page table page.
	 */
	m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	if (m == NULL) {
		/*
		 * Indicate the need to retry.  While waiting, the page table
		 * page may have been allocated.
		 */
		return (NULL);
	}
	m->pindex = ptepindex;

	/*
	 * Map the pagetable page into the process address space, if
	 * it isn't already there.
	 */
	pn = VM_PAGE_TO_PHYS(m) >> PAGE_SHIFT;
	if (ptepindex >= NUL2E + NUL1E) {
		pd_entry_t *l0;
		vm_pindex_t l0index;

		KASSERT(pmap->pm_mode != PMAP_MODE_SV39,
		    ("%s: pindex %#lx in SV39 mode", __func__, ptepindex));
		KASSERT(ptepindex < NUL2E + NUL1E + NUL0E,
		    ("%s: pindex %#lx out of range", __func__, ptepindex));

		l0index = ptepindex - (NUL2E + NUL1E);
		l0 = &pmap->pm_top[l0index];
		KASSERT((pmap_load(l0) & PTE_V) == 0,
		    ("%s: L0 entry %#lx is valid", __func__, pmap_load(l0)));

		entry = PTE_V | (pn << PTE_PPN0_S);
		pmap_store(l0, entry);
	} else if (ptepindex >= NUL2E) {
		pd_entry_t *l0, *l1;
		vm_pindex_t l0index, l1index;

		l1index = ptepindex - NUL2E;
		if (pmap->pm_mode == PMAP_MODE_SV39) {
			l1 = &pmap->pm_top[l1index];
		} else {
			l0index = l1index >> Ln_ENTRIES_SHIFT;
			l0 = &pmap->pm_top[l0index];
			if (pmap_load(l0) == 0) {
				/* Recurse to allocate the L1 page. */
				if (_pmap_alloc_l3(pmap,
				    NUL2E + NUL1E + l0index) == NULL)
					goto fail;
				phys = PTE_TO_PHYS(pmap_load(l0));
			} else {
				phys = PTE_TO_PHYS(pmap_load(l0));
				pdpg = PHYS_TO_VM_PAGE(phys);
				pdpg->ref_count++;
			}
			l1 = (pd_entry_t *)PHYS_TO_DMAP(phys);
			l1 = &l1[ptepindex & Ln_ADDR_MASK];
		}
		KASSERT((pmap_load(l1) & PTE_V) == 0,
		    ("%s: L1 entry %#lx is valid", __func__, pmap_load(l1)));

		entry = PTE_V | (pn << PTE_PPN0_S);
		pmap_store(l1, entry);
	} else {
		vm_pindex_t l0index, l1index;
		pd_entry_t *l0, *l1, *l2;

		l1index = ptepindex >> (L1_SHIFT - L2_SHIFT);
		if (pmap->pm_mode == PMAP_MODE_SV39) {
			l1 = &pmap->pm_top[l1index];
			if (pmap_load(l1) == 0) {
				/* recurse for allocating page dir */
				if (_pmap_alloc_l3(pmap, NUL2E + l1index)
				    == NULL)
					goto fail;
			} else {
				pdpg = PTE_TO_VM_PAGE(pmap_load(l1));
				pdpg->ref_count++;
			}
		} else {
			l0index = l1index >> Ln_ENTRIES_SHIFT;
			l0 = &pmap->pm_top[l0index];
			if (pmap_load(l0) == 0) {
				/* Recurse to allocate the L1 entry. */
				if (_pmap_alloc_l3(pmap, NUL2E + l1index)
				    == NULL)
					goto fail;
				phys = PTE_TO_PHYS(pmap_load(l0));
				l1 = (pd_entry_t *)PHYS_TO_DMAP(phys);
				l1 = &l1[l1index & Ln_ADDR_MASK];
			} else {
				phys = PTE_TO_PHYS(pmap_load(l0));
				l1 = (pd_entry_t *)PHYS_TO_DMAP(phys);
				l1 = &l1[l1index & Ln_ADDR_MASK];
				if (pmap_load(l1) == 0) {
					/* Recurse to allocate the L2 page. */
					if (_pmap_alloc_l3(pmap,
					    NUL2E + l1index) == NULL)
						goto fail;
				} else {
					pdpg = PTE_TO_VM_PAGE(pmap_load(l1));
					pdpg->ref_count++;
				}
			}
		}

		phys = PTE_TO_PHYS(pmap_load(l1));
		l2 = (pd_entry_t *)PHYS_TO_DMAP(phys);
		l2 = &l2[ptepindex & Ln_ADDR_MASK];
		KASSERT((pmap_load(l2) & PTE_V) == 0,
		    ("%s: L2 entry %#lx is valid", __func__, pmap_load(l2)));

		entry = PTE_V | (pn << PTE_PPN0_S);
		pmap_store(l2, entry);
	}

	pmap_resident_count_inc(pmap, 1);

	return (m);

fail:
	vm_page_unwire_noq(m);
	vm_page_free_zero(m);
	return (NULL);
}

/*
 * Remove a single IOMMU entry.
 */
int
iommu_pmap_remove(struct riscv_iommu_pmap *pmap, vm_offset_t va)
{
	pt_entry_t *l3;
	int rc;

	PMAP_LOCK(pmap);

	l3 = pmap_l3(pmap, va);
	if (l3 != NULL) {
		pmap_resident_count_dec(pmap, 1);
		pmap_clear(l3);
		rc = KERN_SUCCESS;
	} else
		rc = KERN_FAILURE;

	PMAP_UNLOCK(pmap);

	return (rc);
}

/* Add a single IOMMU entry. This function does not sleep. */
int
iommu_pmap_enter(struct riscv_iommu_pmap *pmap, vm_offset_t va, vm_paddr_t pa,
    vm_prot_t prot, u_int flags)
{
	pd_entry_t *l2, l2e;
	pt_entry_t new_l3;
	pt_entry_t *l3;
	vm_page_t mpte;
	pn_t pn;
	int rv;

	pn = (pa / PAGE_SIZE);

	new_l3 = PTE_V | PTE_R | PTE_A;
	if (prot & VM_PROT_EXECUTE)
		new_l3 |= PTE_X;
	if (flags & VM_PROT_WRITE)
		new_l3 |= PTE_D;
	if (prot & VM_PROT_WRITE)
		new_l3 |= PTE_W;
	if (va < VM_MAX_USER_ADDRESS)
		new_l3 |= PTE_U;

	new_l3 |= (pn << PTE_PPN0_S);
	new_l3 |= PTE_MA_IO;

	/*
	 * Set modified bit gratuitously for writeable mappings if
	 * the page is unmanaged. We do not want to take a fault
	 * to do the dirty bit accounting for these mappings.
	 */
	if (prot & VM_PROT_WRITE)
		new_l3 |= PTE_D;

	CTR2(KTR_PMAP, "pmap_enter: %.16lx -> %.16lx", va, pa);

	mpte = NULL;
	PMAP_LOCK(pmap);

	l2 = pmap_l2(pmap, va);
	if (l2 != NULL && ((l2e = pmap_load(l2)) & PTE_V) != 0 &&
	    ((l2e & PTE_RWX) == 0)) {
		l3 = pmap_l2_to_l3(l2, va);
	} else if (va < VM_MAXUSER_ADDRESS) {
		mpte = _pmap_alloc_l3(pmap, pmap_l2_pindex(va));
		if (mpte == NULL) {
			CTR0(KTR_PMAP, "pmap_enter: mpte == NULL");
			rv = KERN_RESOURCE_SHORTAGE;
			goto out;
		}
		l3 = pmap_l3(pmap, va);
	} else
		panic("pmap_enter: missing L3 table for kernel va %#lx", va);

	KASSERT((pmap_load(l3) & PTE_V) == 0, ("l3 is valid"));

	pmap_store(l3, new_l3);
	pmap_resident_count_inc(pmap, 1);

	rv = KERN_SUCCESS;
out:
	PMAP_UNLOCK(pmap);

	return (rv);
}

static void
iommu_pmap_remove_pages_sv48(struct riscv_iommu_pmap *pmap)
{
	pd_entry_t l0e, *l1, l1e, *l2, l2e, *l3, l3e;
	vm_paddr_t pa0, pa1, pa;
	vm_page_t m0, m1, m;
	int i, j, k, l;

	PMAP_LOCK(pmap);

	for (i = 0; i < Ln_ENTRIES; i++) {
		l0e = pmap->pm_top[i];
		if ((l0e & PTE_V) == 0)
			continue;
		pa0 = PTE_TO_PHYS(l0e);
		m0 = PHYS_TO_VM_PAGE(pa0);
		l1 = (pd_entry_t *)PHYS_TO_DMAP(pa0);

		for (j = 0; j < Ln_ENTRIES; j++) {
			l1e = l1[j];
			if ((l1e & PTE_V) == 0)
				continue;
			pa1 = PTE_TO_PHYS(l1e);
			m1 = PHYS_TO_VM_PAGE(pa1);
			l2 = (pd_entry_t *)PHYS_TO_DMAP(pa1);

			for (k = 0; k < Ln_ENTRIES; k++) {
				l2e = l2[k];
				if ((l2e & PTE_V) == 0)
					continue;
				pa = PTE_TO_PHYS(l2e);
				m = PHYS_TO_VM_PAGE(pa);
				l3 = (pt_entry_t *)PHYS_TO_DMAP(pa);

				for (l = 0; l < Ln_ENTRIES; l++) {
					l3e = l3[l];
					if ((l3e & PTE_V) == 0)
						continue;
					panic("%s: l3e found (idx %d %d %d %d)",
					    __func__, i, j, k, l);
				}

				vm_page_unwire_noq(m1);
				vm_page_unwire_noq(m);
				pmap_resident_count_dec(pmap, 1);
				vm_page_free(m);
				pmap_clear(&l2[k]);
			}

			vm_page_unwire_noq(m0);
			pmap_resident_count_dec(pmap, 1);
			vm_page_free(m1);
			pmap_clear(&l1[j]);
		}

		pmap_resident_count_dec(pmap, 1);
		vm_page_free(m0);
		pmap_clear(&pmap->pm_top[i]);
	}

	KASSERT(pmap->sp_resident_count == 0,
	    ("Invalid resident count %jd", pmap->sp_resident_count));

	PMAP_UNLOCK(pmap);
}

static void
iommu_pmap_remove_pages_sv39(struct riscv_iommu_pmap *pmap)
{
	pd_entry_t l1e, *l2, l2e, *l3, l3e;
	vm_paddr_t pa1, pa;
	vm_page_t m1, m;
	int j, k, l;

	PMAP_LOCK(pmap);

	for (j = 0; j < Ln_ENTRIES; j++) {
		l1e = pmap->pm_top[j];
		if ((l1e & PTE_V) == 0)
			continue;
		pa1 = PTE_TO_PHYS(l1e);
		m1 = PHYS_TO_VM_PAGE(pa1);
		l2 = (pd_entry_t *)PHYS_TO_DMAP(pa1);

		for (k = 0; k < Ln_ENTRIES; k++) {
			l2e = l2[k];
			if ((l2e & PTE_V) == 0)
				continue;
			pa = PTE_TO_PHYS(l2e);
			m = PHYS_TO_VM_PAGE(pa);
			l3 = (pt_entry_t *)PHYS_TO_DMAP(pa);

			for (l = 0; l < Ln_ENTRIES; l++) {
				l3e = l3[l];
				if ((l3e & PTE_V) == 0)
					continue;
				panic("%s: l3e found (idx %d %d %d)",
				    __func__, j, k, l);
			}

			vm_page_unwire_noq(m1);
			vm_page_unwire_noq(m);
			pmap_resident_count_dec(pmap, 1);
			vm_page_free(m);
			pmap_clear(&l2[k]);
		}

		pmap_resident_count_dec(pmap, 1);
		vm_page_free(m1);
		pmap_clear(&pmap->pm_top[j]);
	}

	KASSERT(pmap->sp_resident_count == 0,
	    ("Invalid resident count %jd", pmap->sp_resident_count));

	PMAP_UNLOCK(pmap);
}

void
iommu_pmap_remove_pages(struct riscv_iommu_pmap *pmap)
{

	switch (pmap->pm_mode) {
	case PMAP_MODE_SV39:
		iommu_pmap_remove_pages_sv39(pmap);
		break;
	case PMAP_MODE_SV48:
		iommu_pmap_remove_pages_sv48(pmap);
		break;
	default:
		panic("Unknown virtual memory system");
	}
}
