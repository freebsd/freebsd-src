/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2024 The FreeBSD Foundation
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
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/bus.h>
#include <sys/domainset.h>
#include <sys/interrupt.h>
#include <sys/kernel.h>
#include <sys/ktr.h>
#include <sys/lock.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/rwlock.h>
#include <sys/rman.h>
#include <sys/sf_buf.h>
#include <sys/sysctl.h>
#include <sys/taskqueue.h>
#include <sys/tree.h>
#include <sys/uio.h>
#include <sys/vmem.h>
#include <vm/vm.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_radix.h>
#include <vm/vm_map.h>
#include <dev/pci/pcireg.h>
#include <machine/atomic.h>
#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/md_var.h>
#include <machine/specialreg.h>
#include <x86/include/busdma_impl.h>
#include <dev/iommu/busdma_iommu.h>
#include <x86/iommu/amd_reg.h>
#include <x86/iommu/x86_iommu.h>
#include <x86/iommu/amd_iommu.h>

static void amdiommu_unmap_clear_pte(struct amdiommu_domain *domain,
    iommu_gaddr_t base, int lvl, int flags, iommu_pte_t *pte,
    struct sf_buf **sf, struct iommu_map_entry *entry, bool free_sf);
static int amdiommu_unmap_buf_locked(struct amdiommu_domain *domain,
    iommu_gaddr_t base, iommu_gaddr_t size, int flags,
    struct iommu_map_entry *entry);

int
amdiommu_domain_alloc_pgtbl(struct amdiommu_domain *domain)
{
	vm_page_t m;
	int dom;

	KASSERT(domain->pgtbl_obj == NULL,
	    ("already initialized %p", domain));

	domain->pgtbl_obj = vm_pager_allocate(OBJT_PHYS, NULL,
	    IDX_TO_OFF(pglvl_max_pages(domain->pglvl)), 0, 0, NULL);
	if (bus_get_domain(domain->iodom.iommu->dev, &dom) == 0)
		domain->pgtbl_obj->domain.dr_policy = DOMAINSET_PREF(dom);
	AMDIOMMU_DOMAIN_PGLOCK(domain);
	m = iommu_pgalloc(domain->pgtbl_obj, 0, IOMMU_PGF_WAITOK |
	    IOMMU_PGF_ZERO | IOMMU_PGF_OBJL);
	/* No implicit free of the top level page table page. */
	vm_page_wire(m);
	domain->pgtblr = m;
	AMDIOMMU_DOMAIN_PGUNLOCK(domain);
	AMDIOMMU_LOCK(domain->unit);
	domain->iodom.flags |= IOMMU_DOMAIN_PGTBL_INITED;
	AMDIOMMU_UNLOCK(domain->unit);
	return (0);
}

void
amdiommu_domain_free_pgtbl(struct amdiommu_domain *domain)
{
	struct pctrie_iter pages;
	vm_object_t obj;
	vm_page_t m;

	obj = domain->pgtbl_obj;
	if (obj == NULL) {
		KASSERT((domain->iodom.flags & IOMMU_DOMAIN_IDMAP) != 0,
		    ("lost pagetable object domain %p", domain));
		return;
	}
	AMDIOMMU_DOMAIN_ASSERT_PGLOCKED(domain);
	domain->pgtbl_obj = NULL;
	domain->pgtblr = NULL;

	/* Obliterate ref_counts */
	VM_OBJECT_ASSERT_WLOCKED(obj);
	vm_page_iter_init(&pages, obj);
	VM_RADIX_FORALL(m, &pages)
		vm_page_clearref(m);
	VM_OBJECT_WUNLOCK(obj);
	vm_object_deallocate(obj);
}

static iommu_pte_t *
amdiommu_pgtbl_map_pte(struct amdiommu_domain *domain, iommu_gaddr_t base,
    int lvl, int flags, vm_pindex_t *idxp, struct sf_buf **sf)
{
	iommu_pte_t *pte, *ptep;
	struct sf_buf *sfp;
	vm_page_t m;
	vm_pindex_t idx, idx1;

	idx = pglvl_pgtbl_get_pindex(domain->pglvl, base, lvl);
	if (*sf != NULL && idx == *idxp) {
		pte = (iommu_pte_t *)sf_buf_kva(*sf);
	} else {
		if (*sf != NULL)
			iommu_unmap_pgtbl(*sf);
		*idxp = idx;
retry:
		pte = iommu_map_pgtbl(domain->pgtbl_obj, idx, flags, sf);
		if (pte == NULL) {
			KASSERT(lvl > 0,
			    ("lost root page table page %p", domain));
			/*
			 * Page table page does not exist, allocate
			 * it and create a pte in the preceeding page level
			 * to reference the allocated page table page.
			 */
			m = iommu_pgalloc(domain->pgtbl_obj, idx, flags |
			    IOMMU_PGF_ZERO);
			if (m == NULL)
				return (NULL);

			vm_page_wire(m);

			sfp = NULL;
			ptep = amdiommu_pgtbl_map_pte(domain, base, lvl - 1,
			    flags, &idx1, &sfp);
			if (ptep == NULL) {
				KASSERT(m->pindex != 0,
				    ("loosing root page %p", domain));
				vm_page_unwire_noq(m);
				iommu_pgfree(domain->pgtbl_obj, m->pindex,
				    flags, NULL);
				return (NULL);
			}
			ptep->pte = VM_PAGE_TO_PHYS(m) |  AMDIOMMU_PTE_IR |
			    AMDIOMMU_PTE_IW | AMDIOMMU_PTE_PR |
			    ((domain->pglvl - lvl) << AMDIOMMU_PTE_NLVL_SHIFT);
			vm_page_wire(sf_buf_page(sfp));
			vm_page_unwire_noq(m);
			iommu_unmap_pgtbl(sfp);
			/* Only executed once. */
			goto retry;
		}
	}
	pte += pglvl_pgtbl_pte_off(domain->pglvl, base, lvl);
	return (pte);
}

static int
amdiommu_map_buf_locked(struct amdiommu_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, vm_page_t *ma, uint64_t pflags, int flags,
    struct iommu_map_entry *entry)
{
	iommu_pte_t *pte;
	struct sf_buf *sf;
	iommu_gaddr_t base1;
	vm_pindex_t pi, idx;

	AMDIOMMU_DOMAIN_ASSERT_PGLOCKED(domain);

	base1 = base;
	flags |= IOMMU_PGF_OBJL;
	idx = -1;
	pte = NULL;
	sf = NULL;

	for (pi = 0; size > 0; base += IOMMU_PAGE_SIZE, size -= IOMMU_PAGE_SIZE,
	    pi++) {
		KASSERT(size >= IOMMU_PAGE_SIZE,
		    ("mapping loop overflow %p %jx %jx %jx", domain,
		    (uintmax_t)base, (uintmax_t)size, (uintmax_t)IOMMU_PAGE_SIZE));
		pte = amdiommu_pgtbl_map_pte(domain, base, domain->pglvl - 1,
		    flags, &idx, &sf);
		if (pte == NULL) {
			KASSERT((flags & IOMMU_PGF_WAITOK) == 0,
			    ("failed waitable pte alloc %p", domain));
			if (sf != NULL)
				iommu_unmap_pgtbl(sf);
			amdiommu_unmap_buf_locked(domain, base1, base - base1,
			    flags, entry);
			return (ENOMEM);
		}
		/* next level 0, no superpages */
		pte->pte = VM_PAGE_TO_PHYS(ma[pi]) | pflags | AMDIOMMU_PTE_PR;
		vm_page_wire(sf_buf_page(sf));
	}
	if (sf != NULL)
		iommu_unmap_pgtbl(sf);
	return (0);
}

static int
amdiommu_map_buf(struct iommu_domain *iodom, struct iommu_map_entry *entry,
    vm_page_t *ma, uint64_t eflags, int flags)
{
	struct amdiommu_domain *domain;
	uint64_t pflags;
	iommu_gaddr_t base, size;
	int error;

	base = entry->start;
	size = entry->end - entry->start;
	pflags = ((eflags & IOMMU_MAP_ENTRY_READ) != 0 ? AMDIOMMU_PTE_IR : 0) |
	    ((eflags & IOMMU_MAP_ENTRY_WRITE) != 0 ? AMDIOMMU_PTE_IW : 0) |
	    ((eflags & IOMMU_MAP_ENTRY_SNOOP) != 0 ? AMDIOMMU_PTE_FC : 0);
	/* IOMMU_MAP_ENTRY_TM ignored */

	domain = IODOM2DOM(iodom);

	KASSERT((iodom->flags & IOMMU_DOMAIN_IDMAP) == 0,
	    ("modifying idmap pagetable domain %p", domain));
	KASSERT((base & IOMMU_PAGE_MASK) == 0,
	    ("non-aligned base %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT((size & IOMMU_PAGE_MASK) == 0,
	    ("non-aligned size %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT(size > 0, ("zero size %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT(base < iodom->end,
	    ("base too high %p %jx %jx end %jx", domain, (uintmax_t)base,
	    (uintmax_t)size, (uintmax_t)iodom->end));
	KASSERT(base + size < iodom->end,
	    ("end too high %p %jx %jx end %jx", domain, (uintmax_t)base,
	    (uintmax_t)size, (uintmax_t)iodom->end));
	KASSERT(base + size > base,
	    ("size overflow %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT((pflags & (AMDIOMMU_PTE_IR | AMDIOMMU_PTE_IW)) != 0,
	    ("neither read nor write %jx", (uintmax_t)pflags));
	KASSERT((pflags & ~(AMDIOMMU_PTE_IR | AMDIOMMU_PTE_IW | AMDIOMMU_PTE_FC
	    )) == 0,
	    ("invalid pte flags %jx", (uintmax_t)pflags));
	KASSERT((flags & ~IOMMU_PGF_WAITOK) == 0, ("invalid flags %x", flags));

	AMDIOMMU_DOMAIN_PGLOCK(domain);
	error = amdiommu_map_buf_locked(domain, base, size, ma, pflags,
	    flags, entry);
	AMDIOMMU_DOMAIN_PGUNLOCK(domain);

	/*
	 * XXXKIB invalidation seems to be needed even for non-valid->valid
	 * updates.  Recheck.
	 */
	iommu_qi_invalidate_sync(iodom, base, size,
	    (flags & IOMMU_PGF_WAITOK) != 0);
	return (error);
}

static void
amdiommu_free_pgtbl_pde(struct amdiommu_domain *domain, iommu_gaddr_t base,
    int lvl, int flags, struct iommu_map_entry *entry)
{
	struct sf_buf *sf;
	iommu_pte_t *pde;
	vm_pindex_t idx;

	sf = NULL;
	pde = amdiommu_pgtbl_map_pte(domain, base, lvl, flags, &idx, &sf);
	amdiommu_unmap_clear_pte(domain, base, lvl, flags, pde, &sf, entry,
	    true);
}

static void
amdiommu_unmap_clear_pte(struct amdiommu_domain *domain, iommu_gaddr_t base,
    int lvl, int flags, iommu_pte_t *pte, struct sf_buf **sf,
    struct iommu_map_entry *entry, bool free_sf)
{
	vm_page_t m;

	pte->pte = 0;
	m = sf_buf_page(*sf);
	if (free_sf) {
		iommu_unmap_pgtbl(*sf);
		*sf = NULL;
	}
	if (!vm_page_unwire_noq(m))
		return;
	KASSERT(lvl != 0,
	    ("lost reference (lvl) on root pg domain %p base %jx lvl %d",
	    domain, (uintmax_t)base, lvl));
	KASSERT(m->pindex != 0,
	    ("lost reference (idx) on root pg domain %p base %jx lvl %d",
	    domain, (uintmax_t)base, lvl));
	iommu_pgfree(domain->pgtbl_obj, m->pindex, flags, entry);
	amdiommu_free_pgtbl_pde(domain, base, lvl - 1, flags, entry);
}

static int
amdiommu_unmap_buf_locked(struct amdiommu_domain *domain, iommu_gaddr_t base,
    iommu_gaddr_t size, int flags, struct iommu_map_entry *entry)
{
	iommu_pte_t *pte;
	struct sf_buf *sf;
	vm_pindex_t idx;
	iommu_gaddr_t pg_sz;

	AMDIOMMU_DOMAIN_ASSERT_PGLOCKED(domain);
	if (size == 0)
		return (0);

	KASSERT((domain->iodom.flags & IOMMU_DOMAIN_IDMAP) == 0,
	    ("modifying idmap pagetable domain %p", domain));
	KASSERT((base & IOMMU_PAGE_MASK) == 0,
	    ("non-aligned base %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT((size & IOMMU_PAGE_MASK) == 0,
	    ("non-aligned size %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT(base < DOM2IODOM(domain)->end,
	    ("base too high %p %jx %jx end %jx", domain, (uintmax_t)base,
	    (uintmax_t)size, (uintmax_t)DOM2IODOM(domain)->end));
	KASSERT(base + size < DOM2IODOM(domain)->end,
	    ("end too high %p %jx %jx end %jx", domain, (uintmax_t)base,
	    (uintmax_t)size, (uintmax_t)DOM2IODOM(domain)->end));
	KASSERT(base + size > base,
	    ("size overflow %p %jx %jx", domain, (uintmax_t)base,
	    (uintmax_t)size));
	KASSERT((flags & ~IOMMU_PGF_WAITOK) == 0, ("invalid flags %x", flags));

	pg_sz = IOMMU_PAGE_SIZE;
	flags |= IOMMU_PGF_OBJL;

	for (sf = NULL; size > 0; base += pg_sz, size -= pg_sz) {
		pte = amdiommu_pgtbl_map_pte(domain, base,
		    domain->pglvl - 1, flags, &idx, &sf);
		KASSERT(pte != NULL,
		    ("sleeping or page missed %p %jx %d 0x%x",
		    domain, (uintmax_t)base, domain->pglvl - 1, flags));
		amdiommu_unmap_clear_pte(domain, base, domain->pglvl - 1,
		    flags, pte, &sf, entry, false);
		KASSERT(size >= pg_sz,
		    ("unmapping loop overflow %p %jx %jx %jx", domain,
		    (uintmax_t)base, (uintmax_t)size, (uintmax_t)pg_sz));
	}
	if (sf != NULL)
		iommu_unmap_pgtbl(sf);
	return (0);
}

static int
amdiommu_unmap_buf(struct iommu_domain *iodom, struct iommu_map_entry *entry,
    int flags)
{
	struct amdiommu_domain *domain;
	int error;

	domain = IODOM2DOM(iodom);

	AMDIOMMU_DOMAIN_PGLOCK(domain);
	error = amdiommu_unmap_buf_locked(domain, entry->start,
	    entry->end - entry->start, flags, entry);
	AMDIOMMU_DOMAIN_PGUNLOCK(domain);
	return (error);
}

const struct iommu_domain_map_ops amdiommu_domain_map_ops = {
	.map = amdiommu_map_buf,
	.unmap = amdiommu_unmap_buf,
};
