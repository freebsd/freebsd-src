/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
__FBSDID("$FreeBSD$");

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

#include "mmu_if.h"

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

static struct rwlock_padalign pvh_global_lock;

#define PMAP_ROOT_SIZE	(sizeof(pte_t***) * PP2D_NENTRIES)
static pte_t *ptbl_alloc(mmu_t, pmap_t, pte_t **,
			 unsigned int, boolean_t);
static void ptbl_free(mmu_t, pmap_t, pte_t **, unsigned int, vm_page_t);
static void ptbl_hold(mmu_t, pmap_t, pte_t **, unsigned int);
static int ptbl_unhold(mmu_t, pmap_t, vm_offset_t);

static vm_paddr_t pte_vatopa(mmu_t, pmap_t, vm_offset_t);
static int pte_enter(mmu_t, pmap_t, vm_page_t, vm_offset_t, uint32_t, boolean_t);
static int pte_remove(mmu_t, pmap_t, vm_offset_t, uint8_t);
static pte_t *pte_find(mmu_t, pmap_t, vm_offset_t);
static void kernel_pte_alloc(vm_offset_t, vm_offset_t, vm_offset_t);

/**************************************************************************/
/* Page table related */
/**************************************************************************/

/* Initialize pool of kva ptbl buffers. */
static void
ptbl_init(void)
{
}

/* Get a pointer to a PTE in a page table. */
static __inline pte_t *
pte_find(mmu_t mmu, pmap_t pmap, vm_offset_t va)
{
	pte_t         **pdir;
	pte_t          *ptbl;

	KASSERT((pmap != NULL), ("pte_find: invalid pmap"));

	pdir = pmap->pm_pp2d[PP2D_IDX(va)];
	if (!pdir)
		return NULL;
	ptbl = pdir[PDIR_IDX(va)];
	return ((ptbl != NULL) ? &ptbl[PTBL_IDX(va)] : NULL);
}

/*
 * allocate a page of pointers to page directories, do not preallocate the
 * page tables
 */
static pte_t  **
pdir_alloc(mmu_t mmu, pmap_t pmap, unsigned int pp2d_idx, bool nosleep)
{
	vm_page_t	m;
	pte_t          **pdir;
	int		req;

	req = VM_ALLOC_NOOBJ | VM_ALLOC_WIRED;
	while ((m = vm_page_alloc(NULL, pp2d_idx, req)) == NULL) {
		PMAP_UNLOCK(pmap);
		if (nosleep) {
			return (NULL);
		}
		vm_wait(NULL);
		PMAP_LOCK(pmap);
	}

	/* Zero whole ptbl. */
	pdir = (pte_t **)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	mmu_booke_zero_page(mmu, m);

	return (pdir);
}

/* Free pdir pages and invalidate pdir entry. */
static void
pdir_free(mmu_t mmu, pmap_t pmap, unsigned int pp2d_idx, vm_page_t m)
{
	pte_t         **pdir;

	pdir = pmap->pm_pp2d[pp2d_idx];

	KASSERT((pdir != NULL), ("pdir_free: null pdir"));

	pmap->pm_pp2d[pp2d_idx] = NULL;

	vm_wire_sub(1);
	vm_page_free_zero(m);
}

/*
 * Decrement pdir pages hold count and attempt to free pdir pages. Called
 * when removing directory entry from pdir.
 * 
 * Return 1 if pdir pages were freed.
 */
static int
pdir_unhold(mmu_t mmu, pmap_t pmap, u_int pp2d_idx)
{
	pte_t         **pdir;
	vm_paddr_t	pa;
	vm_page_t	m;

	KASSERT((pmap != kernel_pmap),
		("pdir_unhold: unholding kernel pdir!"));

	pdir = pmap->pm_pp2d[pp2d_idx];

	/* decrement hold count */
	pa = DMAP_TO_PHYS((vm_offset_t) pdir);
	m = PHYS_TO_VM_PAGE(pa);

	/*
	 * Free pdir page if there are no dir entries in this pdir.
	 */
	m->ref_count--;
	if (m->ref_count == 0) {
		pdir_free(mmu, pmap, pp2d_idx, m);
		return (1);
	}
	return (0);
}

/*
 * Increment hold count for pdir pages. This routine is used when new ptlb
 * entry is being inserted into pdir.
 */
static void
pdir_hold(mmu_t mmu, pmap_t pmap, pte_t ** pdir)
{
	vm_page_t	m;

	KASSERT((pmap != kernel_pmap),
		("pdir_hold: holding kernel pdir!"));

	KASSERT((pdir != NULL), ("pdir_hold: null pdir"));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t)pdir));
	m->ref_count++;
}

/* Allocate page table. */
static pte_t   *
ptbl_alloc(mmu_t mmu, pmap_t pmap, pte_t ** pdir, unsigned int pdir_idx,
    boolean_t nosleep)
{
	vm_page_t	m;
	pte_t          *ptbl;
	int		req;

	KASSERT((pdir[pdir_idx] == NULL),
		("%s: valid ptbl entry exists!", __func__));

	req = VM_ALLOC_NOOBJ | VM_ALLOC_WIRED;
	while ((m = vm_page_alloc(NULL, pdir_idx, req)) == NULL) {
		if (nosleep)
			return (NULL);
		PMAP_UNLOCK(pmap);
		rw_wunlock(&pvh_global_lock);
		vm_wait(NULL);
		rw_wlock(&pvh_global_lock);
		PMAP_LOCK(pmap);
	}

	/* Zero whole ptbl. */
	ptbl = (pte_t *)PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m));
	mmu_booke_zero_page(mmu, m);

	return (ptbl);
}

/* Free ptbl pages and invalidate pdir entry. */
static void
ptbl_free(mmu_t mmu, pmap_t pmap, pte_t ** pdir, unsigned int pdir_idx, vm_page_t m)
{
	pte_t          *ptbl;

	ptbl = pdir[pdir_idx];

	KASSERT((ptbl != NULL), ("ptbl_free: null ptbl"));

	pdir[pdir_idx] = NULL;

	vm_wire_sub(1);
	vm_page_free_zero(m);
}

/*
 * Decrement ptbl pages hold count and attempt to free ptbl pages. Called
 * when removing pte entry from ptbl.
 * 
 * Return 1 if ptbl pages were freed.
 */
static int
ptbl_unhold(mmu_t mmu, pmap_t pmap, vm_offset_t va)
{
	pte_t          *ptbl;
	vm_page_t	m;
	u_int		pp2d_idx;
	pte_t         **pdir;
	u_int		pdir_idx;

	pp2d_idx = PP2D_IDX(va);
	pdir_idx = PDIR_IDX(va);

	KASSERT((pmap != kernel_pmap),
		("ptbl_unhold: unholding kernel ptbl!"));

	pdir = pmap->pm_pp2d[pp2d_idx];
	ptbl = pdir[pdir_idx];

	/* decrement hold count */
	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) ptbl));

	/*
	 * Free ptbl pages if there are no pte entries in this ptbl.
	 * ref_count has the same value for all ptbl pages, so check the
	 * last page.
	 */
	m->ref_count--;
	if (m->ref_count == 0) {
		ptbl_free(mmu, pmap, pdir, pdir_idx, m);
		pdir_unhold(mmu, pmap, pp2d_idx);
		return (1);
	}
	return (0);
}

/*
 * Increment hold count for ptbl pages. This routine is used when new pte
 * entry is being inserted into ptbl.
 */
static void
ptbl_hold(mmu_t mmu, pmap_t pmap, pte_t ** pdir, unsigned int pdir_idx)
{
	pte_t          *ptbl;
	vm_page_t	m;

	KASSERT((pmap != kernel_pmap),
		("ptbl_hold: holding kernel ptbl!"));

	ptbl = pdir[pdir_idx];

	KASSERT((ptbl != NULL), ("ptbl_hold: null ptbl"));

	m = PHYS_TO_VM_PAGE(DMAP_TO_PHYS((vm_offset_t) ptbl));
	m->ref_count++;
}

/*
 * Clean pte entry, try to free page table page if requested.
 * 
 * Return 1 if ptbl pages were freed, otherwise return 0.
 */
static int
pte_remove(mmu_t mmu, pmap_t pmap, vm_offset_t va, u_int8_t flags)
{
	vm_page_t	m;
	pte_t          *pte;

	pte = pte_find(mmu, pmap, va);
	KASSERT(pte != NULL, ("%s: NULL pte", __func__));

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
		return (ptbl_unhold(mmu, pmap, va));
	}
	return (0);
}

/*
 * Insert PTE for a given page and virtual address.
 */
static int
pte_enter(mmu_t mmu, pmap_t pmap, vm_page_t m, vm_offset_t va, uint32_t flags,
    boolean_t nosleep)
{
	unsigned int	pp2d_idx = PP2D_IDX(va);
	unsigned int	pdir_idx = PDIR_IDX(va);
	unsigned int	ptbl_idx = PTBL_IDX(va);
	pte_t          *ptbl, *pte, pte_tmp;
	pte_t         **pdir;

	/* Get the page directory pointer. */
	pdir = pmap->pm_pp2d[pp2d_idx];
	if (pdir == NULL)
		pdir = pdir_alloc(mmu, pmap, pp2d_idx, nosleep);

	/* Get the page table pointer. */
	ptbl = pdir[pdir_idx];

	if (ptbl == NULL) {
		/* Allocate page table pages. */
		ptbl = ptbl_alloc(mmu, pmap, pdir, pdir_idx, nosleep);
		if (ptbl == NULL) {
			KASSERT(nosleep, ("nosleep and NULL ptbl"));
			return (ENOMEM);
		}
		pte = &ptbl[ptbl_idx];
	} else {
		/*
		 * Check if there is valid mapping for requested va, if there
		 * is, remove it.
		 */
		pte = &ptbl[ptbl_idx];
		if (PTE_ISVALID(pte)) {
			pte_remove(mmu, pmap, va, PTBL_HOLD);
		} else {
			/*
			 * pte is not used, increment hold count for ptbl
			 * pages.
			 */
			if (pmap != kernel_pmap)
				ptbl_hold(mmu, pmap, pdir, pdir_idx);
		}
	}

	if (pdir[pdir_idx] == NULL) {
		if (pmap != kernel_pmap && pmap->pm_pp2d[pp2d_idx] != NULL)
			pdir_hold(mmu, pmap, pdir);
		pdir[pdir_idx] = ptbl;
	}
	if (pmap->pm_pp2d[pp2d_idx] == NULL)
		pmap->pm_pp2d[pp2d_idx] = pdir;

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
pte_vatopa(mmu_t mmu, pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t	pa = 0;
	pte_t          *pte;

	pte = pte_find(mmu, pmap, va);
	if ((pte != NULL) && PTE_ISVALID(pte))
		pa = (PTE_PA(pte) | (va & PTE_PA_MASK));
	return (pa);
}


/* allocate pte entries to manage (addr & mask) to (addr & mask) + size */
static void
kernel_pte_alloc(vm_offset_t data_end, vm_offset_t addr, vm_offset_t pdir)
{
	int		i, j;
	vm_offset_t	va;
	pte_t		*pte;

	va = addr;
	/* Initialize kernel pdir */
	for (i = 0; i < kernel_pdirs; i++) {
		kernel_pmap->pm_pp2d[i + PP2D_IDX(va)] =
		    (pte_t **)(pdir + (i * PAGE_SIZE * PDIR_PAGES));
		for (j = PDIR_IDX(va + (i * PAGE_SIZE * PDIR_NENTRIES * PTBL_NENTRIES));
		    j < PDIR_NENTRIES; j++) {
			kernel_pmap->pm_pp2d[i + PP2D_IDX(va)][j] =
			    (pte_t *)(pdir + (kernel_pdirs * PAGE_SIZE) +
			     (((i * PDIR_NENTRIES) + j) * PAGE_SIZE));
		}
	}

	/*
	 * Fill in PTEs covering kernel code and data. They are not required
	 * for address translation, as this area is covered by static TLB1
	 * entries, but for pte_vatopa() to work correctly with kernel area
	 * addresses.
	 */
	for (va = addr; va < data_end; va += PAGE_SIZE) {
		pte = &(kernel_pmap->pm_pp2d[PP2D_IDX(va)][PDIR_IDX(va)][PTBL_IDX(va)]);
		*pte = PTE_RPN_FROM_PA(kernload + (va - kernstart));
		*pte |= PTE_M | PTE_SR | PTE_SW | PTE_SX | PTE_WIRED |
		    PTE_VALID | PTE_PS_4KB;
	}
}

/*
 * Initialize a preallocated and zeroed pmap structure,
 * such as one in a vmspace structure.
 */
static void
mmu_booke_pinit(mmu_t mmu, pmap_t pmap)
{
	int i;

	CTR4(KTR_PMAP, "%s: pmap = %p, proc %d '%s'", __func__, pmap,
	    curthread->td_proc->p_pid, curthread->td_proc->p_comm);

	KASSERT((pmap != kernel_pmap), ("pmap_pinit: initializing kernel_pmap"));

	for (i = 0; i < MAXCPU; i++)
		pmap->pm_tid[i] = TID_NONE;
	CPU_ZERO(&kernel_pmap->pm_active);
	bzero(&pmap->pm_stats, sizeof(pmap->pm_stats));
	pmap->pm_pp2d = uma_zalloc(ptbl_root_zone, M_WAITOK);
	bzero(pmap->pm_pp2d, sizeof(pte_t **) * PP2D_NENTRIES);
}

/*
 * Release any resources held by the given physical map.
 * Called when a pmap initialized by mmu_booke_pinit is being released.
 * Should only be called if the map contains no valid mappings.
 */
static void
mmu_booke_release(mmu_t mmu, pmap_t pmap)
{

	KASSERT(pmap->pm_stats.resident_count == 0,
	    ("pmap_release: pmap resident count %ld != 0",
	    pmap->pm_stats.resident_count));
	uma_zfree(ptbl_root_zone, pmap->pm_pp2d);
}

static void
mmu_booke_sync_icache(mmu_t mmu, pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	pte_t *pte;
	vm_paddr_t pa = 0;
	int sync_sz, valid;
 
	while (sz > 0) {
		PMAP_LOCK(pm);
		pte = pte_find(mmu, pm, va);
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
mmu_booke_zero_page_area(mmu_t mmu, vm_page_t m, int off, int size)
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
mmu_booke_zero_page(mmu_t mmu, vm_page_t m)
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
mmu_booke_copy_page(mmu_t mmu, vm_page_t sm, vm_page_t dm)
{
	vm_offset_t sva, dva;

	sva = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(sm));
	dva = PHYS_TO_DMAP(VM_PAGE_TO_PHYS(dm));
	memcpy((caddr_t)dva, (caddr_t)sva, PAGE_SIZE);
}

static inline void
mmu_booke_copy_pages(mmu_t mmu, vm_page_t *ma, vm_offset_t a_offset,
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
mmu_booke_quick_enter_page(mmu_t mmu, vm_page_t m)
{
	return (PHYS_TO_DMAP(VM_PAGE_TO_PHYS(m)));
}

static void
mmu_booke_quick_remove_page(mmu_t mmu, vm_offset_t addr)
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
