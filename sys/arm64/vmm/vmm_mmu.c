/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2017 Alexandru Elisei <alexandru.elisei@gmail.com>
 *
 * This software was developed by Alexandru Elisei under sponsorship
 * from the FreeBSD Foundation.
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
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_param.h>
#include <vm/vm_phys.h>

#include <machine/atomic.h>
#include <machine/machdep.h>
#include <machine/vm.h>
#include <machine/vmm.h>
#include <machine/vmparam.h>

#include "mmu.h"
#include "arm64.h"

static struct mtx vmmpmap_mtx;
static pt_entry_t *l0;
static vm_paddr_t l0_paddr;

bool
vmmpmap_init(void)
{
	vm_page_t m;

	m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO);
	if (m == NULL)
		return (false);

	l0_paddr = VM_PAGE_TO_PHYS(m);
	l0 = (pd_entry_t *)PHYS_TO_DMAP(l0_paddr);

	mtx_init(&vmmpmap_mtx, "vmm pmap", NULL, MTX_DEF);

	return (true);
}

static void
vmmpmap_release_l3(pd_entry_t l2e)
{
	pt_entry_t *l3 __diagused;
	vm_page_t m;
	int i;

	l3 = (pd_entry_t *)PHYS_TO_DMAP(l2e & ~ATTR_MASK);
	for (i = 0; i < Ln_ENTRIES; i++) {
		KASSERT(l3[i] == 0, ("%s: l3 still mapped: %p %lx", __func__,
		    &l3[i], l3[i]));
	}

	m = PHYS_TO_VM_PAGE(l2e & ~ATTR_MASK);
	vm_page_unwire_noq(m);
	vm_page_free(m);
}

static void
vmmpmap_release_l2(pd_entry_t l1e)
{
	pt_entry_t *l2;
	vm_page_t m;
	int i;

	l2 = (pd_entry_t *)PHYS_TO_DMAP(l1e & ~ATTR_MASK);
	for (i = 0; i < Ln_ENTRIES; i++) {
		if (l2[i] != 0) {
			vmmpmap_release_l3(l2[i]);
		}
	}

	m = PHYS_TO_VM_PAGE(l1e & ~ATTR_MASK);
	vm_page_unwire_noq(m);
	vm_page_free(m);
}

static void
vmmpmap_release_l1(pd_entry_t l0e)
{
	pt_entry_t *l1;
	vm_page_t m;
	int i;

	l1 = (pd_entry_t *)PHYS_TO_DMAP(l0e & ~ATTR_MASK);
	for (i = 0; i < Ln_ENTRIES; i++) {
		if (l1[i] != 0) {
			vmmpmap_release_l2(l1[i]);
		}
	}

	m = PHYS_TO_VM_PAGE(l0e & ~ATTR_MASK);
	vm_page_unwire_noq(m);
	vm_page_free(m);
}

void
vmmpmap_fini(void)
{
	vm_page_t m;
	int i;

	/* Remove the remaining entries */
	for (i = 0; i < L0_ENTRIES; i++) {
		if (l0[i] != 0) {
			vmmpmap_release_l1(l0[i]);
		}
	}

	m = PHYS_TO_VM_PAGE(l0_paddr);
	vm_page_unwire_noq(m);
	vm_page_free(m);

	mtx_destroy(&vmmpmap_mtx);
}

uint64_t
vmmpmap_to_ttbr0(void)
{

	return (l0_paddr);
}

/* Returns a pointer to the level 1 table, allocating if needed. */
static pt_entry_t *
vmmpmap_l1_table(vm_offset_t va)
{
	pt_entry_t new_l0e, l0e, *l1;
	vm_page_t m;
	int rv;

	m = NULL;
again:
	l0e = atomic_load_64(&l0[pmap_l0_index(va)]);
	if ((l0e & ATTR_DESCR_VALID) == 0) {
		/* Allocate a page for the level 1 table */
		if (m == NULL) {
			m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (m == NULL)
				return (NULL);
		}

		new_l0e = VM_PAGE_TO_PHYS(m) | L0_TABLE;

		mtx_lock(&vmmpmap_mtx);
		rv = atomic_cmpset_64(&l0[pmap_l0_index(va)], l0e, new_l0e);
		mtx_unlock(&vmmpmap_mtx);
		/* We may have raced another thread, try again */
		if (rv == 0)
			goto again;

		/* The cmpset succeeded */
		l0e = new_l0e;
	} else if (m != NULL) {
		/* We allocated a page that wasn't used */
		vm_page_unwire_noq(m);
		vm_page_free_zero(m);
	}

	l1 = (pd_entry_t *)PHYS_TO_DMAP(l0e & ~ATTR_MASK);
	return (l1);
}

static pt_entry_t *
vmmpmap_l2_table(vm_offset_t va)
{
	pt_entry_t new_l1e, l1e, *l1, *l2;
	vm_page_t m;
	int rv;

	l1 = vmmpmap_l1_table(va);
	if (l1 == NULL)
		return (NULL);

	m = NULL;
again:
	l1e = atomic_load_64(&l1[pmap_l1_index(va)]);
	if ((l1e & ATTR_DESCR_VALID) == 0) {
		/* Allocate a page for the level 2 table */
		if (m == NULL) {
			m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (m == NULL)
				return (NULL);
		}

		new_l1e = VM_PAGE_TO_PHYS(m) | L1_TABLE;

		mtx_lock(&vmmpmap_mtx);
		rv = atomic_cmpset_64(&l1[pmap_l1_index(va)], l1e, new_l1e);
		mtx_unlock(&vmmpmap_mtx);
		/* We may have raced another thread, try again */
		if (rv == 0)
			goto again;

		/* The cmpset succeeded */
		l1e = new_l1e;
	} else if (m != NULL) {
		/* We allocated a page that wasn't used */
		vm_page_unwire_noq(m);
		vm_page_free_zero(m);
	}

	l2 = (pd_entry_t *)PHYS_TO_DMAP(l1e & ~ATTR_MASK);
	return (l2);
}

static pd_entry_t *
vmmpmap_l3_table(vm_offset_t va)
{
	pt_entry_t new_l2e, l2e, *l2, *l3;
	vm_page_t m;
	int rv;

	l2 = vmmpmap_l2_table(va);
	if (l2 == NULL)
		return (NULL);

	m = NULL;
again:
	l2e = atomic_load_64(&l2[pmap_l2_index(va)]);
	if ((l2e & ATTR_DESCR_VALID) == 0) {
		/* Allocate a page for the level 3 table */
		if (m == NULL) {
			m = vm_page_alloc_noobj(VM_ALLOC_WIRED | VM_ALLOC_ZERO);
			if (m == NULL)
				return (NULL);
		}

		new_l2e = VM_PAGE_TO_PHYS(m) | L2_TABLE;

		mtx_lock(&vmmpmap_mtx);
		rv = atomic_cmpset_64(&l2[pmap_l2_index(va)], l2e, new_l2e);
		mtx_unlock(&vmmpmap_mtx);
		/* We may have raced another thread, try again */
		if (rv == 0)
			goto again;

		/* The cmpset succeeded */
		l2e = new_l2e;
	} else if (m != NULL) {
		/* We allocated a page that wasn't used */
		vm_page_unwire_noq(m);
		vm_page_free_zero(m);
	}

	l3 = (pt_entry_t *)PHYS_TO_DMAP(l2e & ~ATTR_MASK);
	return (l3);
}

/*
 * Creates an EL2 entry in the hyp_pmap. Similar to pmap_kenter.
 */
bool
vmmpmap_enter(vm_offset_t va, vm_size_t size, vm_paddr_t pa, vm_prot_t prot)
{
	pd_entry_t l3e, *l3;

	KASSERT((pa & L3_OFFSET) == 0,
	   ("%s: Invalid physical address", __func__));
	KASSERT((va & L3_OFFSET) == 0,
	   ("%s: Invalid virtual address", __func__));
	KASSERT((size & PAGE_MASK) == 0,
	    ("%s: Mapping is not page-sized", __func__));

	l3e = ATTR_AF | ATTR_SH(ATTR_SH_IS) | L3_PAGE;
	/* This bit is res1 at EL2 */
	l3e |= ATTR_S1_AP(ATTR_S1_AP_USER);
	/* Only normal memory is used at EL2 */
	l3e |= ATTR_S1_IDX(VM_MEMATTR_DEFAULT);

	if ((prot & VM_PROT_EXECUTE) == 0) {
		/* PXN is res0 at EL2. UXN is XN */
		l3e |= ATTR_S1_UXN;
	}
	if ((prot & VM_PROT_WRITE) == 0) {
		l3e |= ATTR_S1_AP(ATTR_S1_AP_RO);
	}

	while (size > 0) {
		l3 = vmmpmap_l3_table(va);
		if (l3 == NULL)
			return (false);

#ifdef INVARIANTS
		/*
		 * Ensure no other threads can write to l3 between the KASSERT
		 * and store.
		 */
		mtx_lock(&vmmpmap_mtx);
#endif
		KASSERT(atomic_load_64(&l3[pmap_l3_index(va)]) == 0,
		    ("%s: VA already mapped", __func__));

		atomic_store_64(&l3[pmap_l3_index(va)], l3e | pa);
#ifdef INVARIANTS
		mtx_unlock(&vmmpmap_mtx);
#endif

		size -= PAGE_SIZE;
		pa += PAGE_SIZE;
		va += PAGE_SIZE;
	}

	return (true);
}

void
vmmpmap_remove(vm_offset_t va, vm_size_t size, bool invalidate)
{
	pt_entry_t l0e, *l1, l1e, *l2, l2e;
	pd_entry_t *l3, l3e, **l3_list;
	vm_offset_t eva, va_next, sva;
	size_t i;

	KASSERT((va & L3_OFFSET) == 0,
	   ("%s: Invalid virtual address", __func__));
	KASSERT((size & PAGE_MASK) == 0,
	    ("%s: Mapping is not page-sized", __func__));

	if (invalidate) {
		l3_list = malloc((size / PAGE_SIZE) * sizeof(l3_list[0]),
		    M_TEMP, M_WAITOK | M_ZERO);
	}

	sva = va;
	eva = va + size;
	mtx_lock(&vmmpmap_mtx);
	for (i = 0; va < eva; va = va_next) {
		l0e = atomic_load_64(&l0[pmap_l0_index(va)]);
		if (l0e == 0) {
			va_next = (va + L0_SIZE) & ~L0_OFFSET;
			if (va_next < va)
				va_next = eva;
			continue;
		}
		MPASS((l0e & ATTR_DESCR_MASK) == L0_TABLE);

		l1 = (pd_entry_t *)PHYS_TO_DMAP(l0e & ~ATTR_MASK);
		l1e = atomic_load_64(&l1[pmap_l1_index(va)]);
		if (l1e == 0) {
			va_next = (va + L1_SIZE) & ~L1_OFFSET;
			if (va_next < va)
				va_next = eva;
			continue;
		}
		MPASS((l1e & ATTR_DESCR_MASK) == L1_TABLE);

		l2 = (pd_entry_t *)PHYS_TO_DMAP(l1e & ~ATTR_MASK);
		l2e = atomic_load_64(&l2[pmap_l2_index(va)]);
		if (l2e == 0) {
			va_next = (va + L2_SIZE) & ~L2_OFFSET;
			if (va_next < va)
				va_next = eva;
			continue;
		}
		MPASS((l2e & ATTR_DESCR_MASK) == L2_TABLE);

		l3 = (pd_entry_t *)PHYS_TO_DMAP(l2e & ~ATTR_MASK);
		if (invalidate) {
			l3e = atomic_load_64(&l3[pmap_l3_index(va)]);
			MPASS(l3e != 0);
			/*
			 * Mark memory as read-only so we can invalidate
			 * the cache.
			 */
			l3e &= ~ATTR_S1_AP_MASK;
			l3e |= ATTR_S1_AP(ATTR_S1_AP_RO);
			atomic_store_64(&l3[pmap_l3_index(va)], l3e);

			l3_list[i] = &l3[pmap_l3_index(va)];
			i++;
		} else {
			/*
			 * The caller is responsible for clearing the cache &
			 * handling the TLB
			 */
			atomic_store_64(&l3[pmap_l3_index(va)], 0);
		}

		va_next = (va + L3_SIZE) & ~L3_OFFSET;
		if (va_next < va)
			va_next = eva;
	}
	mtx_unlock(&vmmpmap_mtx);

	if (invalidate) {
		/* Invalidate the memory from the D-cache */
		vmm_call_hyp(HYP_DC_CIVAC, sva, size);

		for (i = 0; i < (size / PAGE_SIZE); i++) {
			atomic_store_64(l3_list[i], 0);
		}

		vmm_call_hyp(HYP_EL2_TLBI, HYP_EL2_TLBI_VA, sva, size);

		free(l3_list, M_TEMP);
	}
}
