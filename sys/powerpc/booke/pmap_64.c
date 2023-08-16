/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2020 Justin Hibbits
 * Copyright (C) 2007-2009 Semihalf, Rafal Jaworowski <raj@semihalf.com>
 * Copyright (C) 2006 Semihalf, Marian Balakowicz <m8@semihalf.com>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 * NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Some hw specific parts of this pmap were derived or influenced
 * by NetBSD's ibm4xx pmap module. More generic code is shared with
 * a few other pmap modules from the FreeBSD tree.
 */

 /*
  * VM layout notes:
  *
  * Kernel and user threads run within one common virtual address space
  * defined by AS=0.
  *
  * 64-bit pmap:
  * Virtual address space layout:
  * -----------------------------
  * 0x0000_0000_0000_0000 - 0x3fff_ffff_ffff_ffff      : user process
  * 0x4000_0000_0000_0000 - 0x7fff_ffff_ffff_ffff      : unused
  * 0x8000_0000_0000_0000 - 0xbfff_ffff_ffff_ffff      : mmio region
  * 0xc000_0000_0000_0000 - 0xdfff_ffff_ffff_ffff      : direct map
  * 0xe000_0000_0000_0000 - 0xffff_ffff_ffff_ffff      : KVA
  */

#include <sys/cdefs.h>
#include "opt_ddb.h"
#include "opt_kstack_pages.h"

#include <sys/param.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/ktr.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/queue.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kerneldump.h>
#include <sys/linker.h>
#include <sys/msgbuf.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/rwlock.h>
#include <sys/sched.h>
#include <sys/smp.h>
#include <sys/vmmeter.h>

#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_kern.h>
#include <vm/vm_pageout.h>
#include <vm/vm_extern.h>
#include <vm/vm_object.h>
#include <vm/vm_param.h>
#include <vm/vm_map.h>
#include <vm/vm_pager.h>
#include <vm/vm_phys.h>
#include <vm/vm_pagequeue.h>
#include <vm/uma.h>

#include <machine/_inttypes.h>
#include <machine/cpu.h>
#include <machine/pcb.h>
#include <machine/platform.h>

#include <machine/tlb.h>
#include <machine/spr.h>
#include <machine/md_var.h>
#include <machine/mmuvar.h>
#include <machine/pmap.h>
#include <machine/pte.h>

#include <ddb/ddb.h>

#ifdef  DEBUG
#define debugf(fmt, args...) printf(fmt, ##args)
#else
#define debugf(fmt, args...)
#endif

#define	PRI0ptrX	"016lx"

/**************************************************************************/
/* PMAP */
/**************************************************************************/

unsigned int kernel_pdirs;
static uma_zone_t ptbl_root_zone;
static pte_t ****kernel_ptbl_root;

/*
 * Base of the pmap_mapdev() region.  On 32-bit it immediately follows the
 * userspace address range.  On On 64-bit it's far above, at (1 << 63), and
 * ranges up to the DMAP, giving 62 bits of PA allowed.  This is far larger than
 * the widest Book-E address bus, the e6500 has a 40-bit PA space.  This allows
 * us to map akin to the DMAP, with addresses identical to the PA, offset by the
 * base.
 */
#define	VM_MAPDEV_BASE		0x8000000000000000
#define	VM_MAPDEV_PA_MAX	0x4000000000000000 /* Don't encroach on DMAP */

static void tid_flush(tlbtid_t tid);
static unsigned long ilog2(unsigned long);

/**************************************************************************/
/* Page table management */
/**************************************************************************/

#define PMAP_ROOT_SIZE	(sizeof(pte_t****) * PG_ROOT_NENTRIES)
static pte_t *ptbl_alloc(pmap_t pmap, vm_offset_t va,
    bool nosleep, bool *is_new);
static void ptbl_hold(pmap_t, pte_t *);
static int ptbl_unhold(pmap_t, vm_offset_t);

static vm_paddr_t pte_vatopa(pmap_t, vm_offset_t);
static int pte_enter(pmap_t, vm_page_t, vm_offset_t, uint32_t, boolean_t);
static int pte_remove(pmap_t, vm_offset_t, uint8_t);
static pte_t *pte_find(pmap_t, vm_offset_t);
static pte_t *pte_find_next(pmap_t, vm_offset_t *);
static void kernel_pte_alloc(vm_offset_t, vm_offset_t);

/**************************************************************************/
/* Page table related */
/**************************************************************************/

/* Allocate a page, to be used in a page table. */
static vm_offset_t
mmu_booke_alloc_page(pmap_t pmap, unsigned int idx, bool nosleep)
{
	vm_page_t	m;
	int		req;

	req = VM_ALLOC_WIRED | VM_ALLOC_ZERO;
	while ((m = vm_page_alloc_noobj(req)) == NULL) {
		if (nosleep)
			return (0);

		PMAP_UNLOCK(pmap);
		rw_wunlock(&pvh_global_lock);
		vm_wait(NULL);
		rw_wlock(&pvh_global_lock);
		PMAP_LOCK(pmap);
	}
	m->pindex = idx;

	return (PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)));
}

/* Initialize pool of kva ptbl buffers. */
static void
ptbl_init(void)
{
}

/* Get a pointer to a PTE in a page table. */
static __inline pte_t *
pte_find(pmap_t pmap, vm_offset_t va)
{
	pte_t        ***pdir_l1;
	pte_t         **pdir;
	pte_t          *ptbl;

	KASSERT((pmap != NULL), ("pte_find: invalid pmap"));

	pdir_l1 = pmap->pm_root[PG_ROOT_IDX(va)];
	if (pdir_l1 == NULL)
		return (NULL);
	pdir = pdir_l1[PDIR_L1_IDX(va)];
	if (pdir == NULL)
		return (NULL);
	ptbl = pdir[PDIR_IDX(va)];

	return ((ptbl != NULL) ? &ptbl[PTBL_IDX(va)] : NULL);
}

/* Get a pointer to a PTE in a page table, or the next closest (greater) one. */
static __inline pte_t *
pte_find_next(pmap_t pmap, vm_offset_t *pva)
{
	vm_offset_t	va;
	pte_t	    ****pm_root;
	pte_t	       *pte;
	unsigned long	i, j, k, l;

	KASSERT((pmap != NULL), ("pte_find: invalid pmap"));

	va = *pva;
	i = PG_ROOT_IDX(va);
	j = PDIR_L1_IDX(va);
	k = PDIR_IDX(va);
	l = PTBL_IDX(va);
	pm_root = pmap->pm_root;

	/* truncate the VA for later. */
	va &= ~((1UL << (PG_ROOT_H + 1)) - 1);
	for (; i < PG_ROOT_NENTRIES; i++, j = 0, k = 0, l = 0) {
		if (pm_root[i] == 0)
			continue;
		for (; j < PDIR_L1_NENTRIES; j++, k = 0, l = 0) {
			if (pm_root[i][j] == 0)
				continue;
			for (; k < PDIR_NENTRIES; k++, l = 0) {
				if (pm_root[i][j][k] == NULL)
					continue;
				for (; l < PTBL_NENTRIES; l++) {
					pte = &pm_root[i][j][k][l];
					if (!PTE_ISVALID(pte))
						continue;
					*pva = va + PG_ROOT_SIZE * i +
					    PDIR_L1_SIZE * j +
					    PDIR_SIZE * k +
					    PAGE_SIZE * l;
					return (pte);
				}
			}
		}
	}
	return (NULL);
}

static bool
unhold_free_page(pmap_t pmap, vm_page_t m)
{

	if (vm_page_unwire_noq(m)) {
		vm_page_free_zero(m);
		return (true);
	}

	return (false);
}

static vm_offset_t
get_pgtbl_page(pmap_t pmap, vm_offset_t *ptr_tbl, uint32_t index,
    bool nosleep, bool hold_parent, bool *isnew)
{
	vm_offset_t	page;
	vm_page_t	m;

	page = ptr_tbl[index];
	KASSERT(page != 0 || pmap != kernel_pmap,
	    ("NULL page table page found in kernel pmap!"));
	if (page == 0) {
		page = mmu_booke_alloc_page(pmap, index, nosleep);
		if (ptr_tbl[index] == 0) {
			*isnew = true;
			ptr_tbl[index] = page;
			if (hold_parent) {
				m = PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)ptr_tbl));
				m->ref_count++;
			}
			return (page);
		}
		m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS(page));
		page = ptr_tbl[index];
		vm_page_unwire_noq(m);
		vm_page_free_zero(m);
	}

	*isnew = false;

	return (page);
}

/* Allocate page table. */
static pte_t*
ptbl_alloc(pmap_t pmap, vm_offset_t va, bool nosleep, bool *is_new)
{
	unsigned int	pg_root_idx = PG_ROOT_IDX(va);
	unsigned int	pdir_l1_idx = PDIR_L1_IDX(va);
	unsigned int	pdir_idx = PDIR_IDX(va);
	vm_offset_t	pdir_l1, pdir, ptbl;

	/* When holding a parent, no need to hold the root index pages. */
	pdir_l1 = get_pgtbl_page(pmap, (vm_offset_t *)pmap->pm_root,
	    pg_root_idx, nosleep, false, is_new);
	if (pdir_l1 == 0)
		return (NULL);
	pdir = get_pgtbl_page(pmap, (vm_offset_t *)pdir_l1, pdir_l1_idx,
	    nosleep, !*is_new, is_new);
	if (pdir == 0)
		return (NULL);
	ptbl = get_pgtbl_page(pmap, (vm_offset_t *)pdir, pdir_idx,
	    nosleep, !*is_new, is_new);

	return ((pte_t *)ptbl);
}

/*
 * Decrement ptbl pages hold count and attempt to free ptbl pages. Called
 * when removing pte entry from ptbl.
 * 
 * Return 1 if ptbl pages were freed.
 */
static int
ptbl_unhold(pmap_t pmap, vm_offset_t va)
{
	pte_t          *ptbl;
	vm_page_t	m;
	u_int		pg_root_idx;
	pte_t        ***pdir_l1;
	u_int		pdir_l1_idx;
	pte_t         **pdir;
	u_int		pdir_idx;

	pg_root_idx = PG_ROOT_IDX(va);
	pdir_l1_idx = PDIR_L1_IDX(va);
	pdir_idx = PDIR_IDX(va);

	KASSERT((pmap != kernel_pmap),
		("ptbl_unhold: unholding kernel ptbl!"));

	pdir_l1 = pmap->pm_root[pg_root_idx];
	pdir = pdir_l1[pdir_l1_idx];
	ptbl = pdir[pdir_idx];

	/* decrement hold count */
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) ptbl));

	if (!unhold_free_page(pmap, m))
		return (0);

	pdir[pdir_idx] = NULL;
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) pdir));

	if (!unhold_free_page(pmap, m))
		return (1);

	pdir_l1[pdir_l1_idx] = NULL;
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) pdir_l1));

	if (!unhold_free_page(pmap, m))
		return (1);
	pmap->pm_root[pg_root_idx] = NULL;

	return (1);
}

/*
 * Increment hold count for ptbl pages. This routine is used when new pte
 * entry is being inserted into ptbl.
 */
static void
ptbl_hold(pmap_t pmap, pte_t *ptbl)
{
	vm_page_t	m;

	KASSERT((pmap != kernel_pmap),
		("ptbl_hold: holding kernel ptbl!"));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) ptbl));
	m->ref_count++;
}

/*
 * Clean pte entry, try to free page table page if requested.
 * 
 * Return 1 if ptbl pages were freed, otherwise return 0.
 */
static int
pte_remove(pmap_t pmap, vm_offset_t va, u_int8_t flags)
{
	vm_page_t	m;
	pte_t          *pte;

	pte = pte_find(pmap, va);
	KASSERT(pte != NULL, ("%s: NULL pte for va %#jx, pmap %p",
	    __func__, (uintmax_t)va, pmap));

	if (!PTE_ISVALID(pte))
		return (0);

	/* Get vm_page_t for mapped pte. */
	m = PHYS_TO_VM_PAGE(PTE_PA(pte));

	if (PTE_ISWIRED(pte))
		pmap->pm_stats.wired_count--;

	/* Handle managed entry. */
	if (PTE_ISMANAGED(pte)) {
		/* Handle modified pages. */
		if (PTE_ISMODIFIED(pte))
			vm_page_dirty(m);

		/* Referenced pages. */
		if (PTE_ISREFERENCED(pte))
			vm_page_aflag_set(m, PGA_REFERENCED);

		/* Remove pv_entry from pv_list. */
		pv_remove(pmap, va, m);
	} else if (pmap == kernel_pmap && m && m->md.pv_tracked) {
		pv_remove(pmap, va, m);
		if (TAILQ_EMPTY(&m->md.pv_list))
			m->md.pv_tracked = false;
	}
	mtx_lock_spin(&tlbivax_mutex);
	tlb_miss_lock();

	tlb0_flush_entry(va);
	*pte = 0;

	tlb_miss_unlock();
	mtx_unlock_spin(&tlbivax_mutex);

	pmap->pm_stats.resident_count--;

	if (flags & PTBL_UNHOLD) {
		return (ptbl_unhold(pmap, va));
	}
	return (0);
}

/*
 * Insert PTE for a given page and virtual address.
 */
static int
pte_enter(pmap_t pmap, vm_page_t m, vm_offset_t va, uint32_t flags,
    boolean_t nosleep)
{
	unsigned int	ptbl_idx = PTBL_IDX(va);
	pte_t          *ptbl, *pte, pte_tmp;
	bool		is_new;

	/* Get the page directory pointer. */
	ptbl = ptbl_alloc(pmap, va, nosleep, &is_new);
	if (ptbl == NULL) {
		KASSERT(nosleep, ("nosleep and NULL ptbl"));
		return (ENOMEM);
	}
	if (is_new) {
		pte = &ptbl[ptbl_idx];
	} else {
		/*
		 * Check if there is valid mapping for requested va, if there
		 * is, remove it.
		 */
		pte = &ptbl[ptbl_idx];
		if (PTE_ISVALID(pte)) {
			pte_remove(pmap, va, PTBL_HOLD);
		} else {
			/*
			 * pte is not used, increment hold count for ptbl
			 * pages.
			 */
			if (pmap != kernel_pmap)
				ptbl_hold(pmap, ptbl);
		}
	}

	/*
	 * Insert pv_entry into pv_list for mapped page if part of managed
	 * memory.
	 */
	if ((m->oflags & VPO_UNMANAGED) == 0) {
		flags |= PTE_MANAGED;

		/* Create and insert pv entry. */
		pv_insert(pmap, va, m);
	}

	pmap->pm_stats.resident_count++;

	pte_tmp = PTE_RPN_FROM_PA(VM_PAGE_TO_PHYS(m));
	pte_tmp |= (PTE_VALID | flags);

	mtx_lock_spin(&tlbivax_mutex);
	tlb_miss_lock();

	tlb0_flush_entry(va);
	*pte = pte_tmp;

	tlb_miss_unlock();
	mtx_unlock_spin(&tlbivax_mutex);

	return (0);
}

/* Return the pa for the given pmap/va. */
static	vm_paddr_t
pte_vatopa(pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t	pa = 0;
	pte_t          *pte;

	pte = pte_find(pmap, va);
	if ((pte != NULL) && PTE_ISVALID(pte))
		pa = (PTE_PA(pte) | (va & PTE_PA_MASK));
	return (pa);
}

/* allocate pte entries to manage (addr & mask) to (addr & mask) + size */
static void
kernel_pte_alloc(vm_offset_t data_end, vm_offset_t addr)
{
	pte_t		*pte;
	vm_size_t	kva_size;
	int		kernel_pdirs, kernel_pgtbls, pdir_l1s;
	vm_offset_t	va, l1_va, pdir_va, ptbl_va;
	int		i, j, k;

	kva_size = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;
	kernel_pmap->pm_root = kernel_ptbl_root;
	pdir_l1s = howmany(kva_size, PG_ROOT_SIZE);
	kernel_pdirs = howmany(kva_size, PDIR_L1_SIZE);
	kernel_pgtbls = howmany(kva_size, PDIR_SIZE);

	/* Initialize kernel pdir */
	l1_va = (vm_offset_t)kernel_ptbl_root +
	    round_page(PG_ROOT_NENTRIES * sizeof(pte_t ***));
	pdir_va = l1_va + pdir_l1s * PAGE_SIZE;
	ptbl_va = pdir_va + kernel_pdirs * PAGE_SIZE;
	if (bootverbose) {
		printf("ptbl_root_va: %#lx\n", (vm_offset_t)kernel_ptbl_root);
		printf("l1_va: %#lx (%d entries)\n", l1_va, pdir_l1s);
		printf("pdir_va: %#lx(%d entries)\n", pdir_va, kernel_pdirs);
		printf("ptbl_va: %#lx(%d entries)\n", ptbl_va, kernel_pgtbls);
	}

	va = VM_MIN_KERNEL_ADDRESS;
	for (i = PG_ROOT_IDX(va); i < PG_ROOT_IDX(va) + pdir_l1s;
	    i++, l1_va += PAGE_SIZE) {
		kernel_pmap->pm_root[i] = (pte_t ***)l1_va;
		for (j = 0;
		    j < PDIR_L1_NENTRIES && va < VM_MAX_KERNEL_ADDRESS;
		    j++, pdir_va += PAGE_SIZE) {
			kernel_pmap->pm_root[i][j] = (pte_t **)pdir_va;
			for (k = 0;
			    k < PDIR_NENTRIES && va < VM_MAX_KERNEL_ADDRESS;
			    k++, va += PDIR_SIZE, ptbl_va += PAGE_SIZE)
				kernel_pmap->pm_root[i][j][k] = (pte_t *)ptbl_va;
		}
	}
	/*
	 * Fill in PTEs covering kernel code and data. They are not required
	 * for address translation, as this area is covered by static TLB1
	 * entries, but for pte_vatopa() to work correctly with kernel area
	 * addresses.
	 */
	for (va = addr; va < data_end; va += PAGE_SIZE) {
		pte = &(kernel_pmap->pm_root[PG_ROOT_IDX(va)][PDIR_L1_IDX(va)][PDIR_IDX(va)][PTBL_IDX(va)]);
		*pte = PTE_RPN_FROM_PA(kernload + (va - kernstart));
		*pte |= PTE_M | PTE_SR | PTE_SW | PTE_SX | PTE_WIRED |
		    PTE_VALID | PTE_PS_4KB;
	}
}

static vm_offset_t
mmu_booke_alloc_kernel_pgtables(vm_offset_t data_end)
{
	vm_size_t kva_size = VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS;
	kernel_ptbl_root = (pte_t ****)data_end;

	data_end += round_page(PG_ROOT_NENTRIES * sizeof(pte_t ***));
	data_end += howmany(kva_size, PG_ROOT_SIZE) * PAGE_SIZE;
	data_end += howmany(kva_size, PDIR_L1_SIZE) * PAGE_SIZE;
	data_end += howmany(kva_size, PDIR_SIZE) * PAGE_SIZE;

	return (data_end);
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
static int
mmu_booke_pinit(pmap_t pmap)
{
	int i;

	CTR4(KTR_PMAP, "%s: pmap = %p, proc %d '%s'", __func__, pmap,
	    curthread->td_proc->p_pid, curthread->td_proc->p_comm);

	KASSERT((pmap != kernel_pmap), ("pmap_pinit: initializing kernel_pmap"));

	for (i = 0; i < MAXCPU; i++)
		pmap->pm_tid[i] = TID_NONE;
	CPU_ZERO(&kernel_pmap->pm_active);
	bzero(&pmap->pm_stats, sizeof(pmap->pm_stats));
	pmap->pm_root = uma_zalloc(ptbl_root_zone, M_WAITOK);
	bzero(pmap->pm_root, sizeof(pte_t **) * PG_ROOT_NENTRIES);

	return (1);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by mmu_booke_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
static void
mmu_booke_release(pmap_t pmap)
{

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
#ifdef INVARIANTS
	/*
	 * Verify that all page directories are gone.
	 * Protects against reference count leakage.
	 */
	for (int i = 0; i < PG_ROOT_NENTRIES; i++)
		KASSERT(pmap->pm_root[i] == 0,
		    ("Index %d on root page %p is non-zero!\n", i, pmap->pm_root));
#endif
	uma_zfree(ptbl_root_zone, pmap->pm_root);
}

static void
mmu_booke_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	pte_t *pte;
	vm_paddr_t pa = 0;
	int sync_sz, valid;

	while (sz > 0) {
		PMAP_LOCK(pm);
		pte = pte_find(pm, va);
		valid = (pte != NULL && PTE_ISVALID(pte)) ? 1 : 0;
		if (valid)
			pa = PTE_PA(pte);
		PMAP_UNLOCK(pm);
		sync_sz = PAGE_SIZE - (va & PAGE_MASK);
		sync_sz = min(sync_sz, sz);
		if (valid) {
			pa += (va & PAGE_MASK);
			__syncicache((void *)PHYS_TO_DMAP(pa), sync_sz);
		}
		va += sync_sz;
		sz -= sync_sz;
	}
}

/*
 * mmu_booke_zero_page_area zeros the specified hardware page by
 * mapping it into virtual memory and using bzero to clear
 * its contents.
 *
 * off and size must reside within a single page.
 */
static void
mmu_booke_zero_page_area(vm_page_t m, int off, int size)
{
	vm_offset_t va;

	/* XXX KASSERT off and size are within a single page? */

	va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	bzero((caddr_t)va + off, size);
}

/*
 * mmu_booke_zero_page zeros the specified hardware page.
 */
static void
mmu_booke_zero_page(vm_page_t m)
{
	vm_offset_t off, va;

	va = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));

	for (off = 0; off < PAGE_SIZE; off += cacheline_size)
		__asm __volatile("dcbz 0,%0" :: "r"(va + off));
}

/*
 * mmu_booke_copy_page copies the specified (machine independent) page by
 * mapping the page into virtual memory and using memcopy to copy the page,
 * one machine dependent page at a time.
 */
static void
mmu_booke_copy_page(vm_page_t sm, vm_page_t dm)
{
	vm_offset_t sva, dva;

	sva = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(sm));
	dva = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dm));
	memcpy((caddr_t)dva, (caddr_t)sva, PAGE_SIZE);
}

static inline void
mmu_booke_copy_pages(vm_page_t *ma, vm_offset_t a_offset,
    vm_page_t *mb, vm_offset_t b_offset, int xfersize)
{
	void *a_cp, *b_cp;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;

	vm_page_t pa, pb;

	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		pa = ma[a_offset >> PAGE_SHIFT];
		b_pg_offset = b_offset & PAGE_MASK;
		pb = mb[b_offset >> PAGE_SHIFT];
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		a_cp = (caddr_t)((uintptr_t)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pa)) +
		    a_pg_offset);
		b_cp = (caddr_t)((uintptr_t)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(pb)) +
		    b_pg_offset);
		bcopy(a_cp, b_cp, cnt);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
}

static vm_offset_t
mmu_booke_quick_enter_page(vm_page_t m)
{
	return (PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)));
}

static void
mmu_booke_quick_remove_page(vm_offset_t addr)
{
}

/**************************************************************************/
/* TID handling */
/**************************************************************************/

/*
 * Return the largest uint value log such that 2^log <= num.
 */
static unsigned long
ilog2(unsigned long num)
{
	long lz;

	__asm ("cntlzd %0, %1" : "=r" (lz) : "r" (num));
	return (63 - lz);
}

/*
 * Invalidate all TLB0 entries which match the given TID. Note this is
 * dedicated for cases when invalidations should NOT be propagated to other
 * CPUs.
 */
static void
tid_flush(tlbtid_t tid)
{
	register_t msr;

	/* Don't evict kernel translations */
	if (tid == TID_KERNEL)
		return;

	msr = mfmsr();
	__asm __volatile("wrteei 0");

	/*
	 * Newer (e500mc and later) have tlbilx, which doesn't broadcast, so use
	 * it for PID invalidation.
	 */
	mtspr(SPR_MAS6, tid << MAS6_SPID0_SHIFT);
	__asm __volatile("isync; .long 0x7c200024; isync; msync");

	__asm __volatile("wrtee %0" :: "r"(msr));
}
