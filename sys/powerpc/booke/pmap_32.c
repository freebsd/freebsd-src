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
  * 32-bit pmap:
  * Virtual address space layout:
  * -----------------------------
  * 0x0000_0000 - 0x7fff_ffff	: user process
  * 0x8000_0000 - 0xbfff_ffff	: pmap_mapdev()-ed area (PCI/PCIE etc.)
  * 0xc000_0000 - 0xffff_efff	: KVA
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

#define	PRI0ptrX	"08x"

/* Reserved KVA space and mutex for mmu_booke_zero_page. */
static vm_offset_t zero_page_va;
static struct mtx zero_page_mutex;

/* Reserved KVA space and mutex for mmu_booke_copy_page. */
static vm_offset_t copy_page_src_va;
static vm_offset_t copy_page_dst_va;
static struct mtx copy_page_mutex;

static vm_offset_t kernel_ptbl_root;
static unsigned int kernel_ptbls;	/* Number of KVA ptbls. */

/**************************************************************************/
/* PMAP */
/**************************************************************************/

#define	VM_MAPDEV_BASE	((vm_offset_t)VM_MAXUSER_ADDRESS + PAGE_SIZE)

static void tid_flush(tlbtid_t tid);
static unsigned long ilog2(unsigned long);

/**************************************************************************/
/* Page table management */
/**************************************************************************/

#define PMAP_ROOT_SIZE	(sizeof(pte_t**) * PDIR_NENTRIES)
static void ptbl_init(void);
static struct ptbl_buf *ptbl_buf_alloc(void);
static void ptbl_buf_free(struct ptbl_buf *);
static void ptbl_free_pmap_ptbl(pmap_t, pte_t *);

static pte_t *ptbl_alloc(pmap_t, unsigned int, boolean_t);
static void ptbl_free(pmap_t, unsigned int);
static void ptbl_hold(pmap_t, unsigned int);
static int ptbl_unhold(pmap_t, unsigned int);

static vm_paddr_t pte_vatopa(pmap_t, vm_offset_t);
static int pte_enter(pmap_t, vm_page_t, vm_offset_t, uint32_t, boolean_t);
static int pte_remove(pmap_t, vm_offset_t, uint8_t);
static pte_t *pte_find(pmap_t, vm_offset_t);

struct ptbl_buf {
	TAILQ_ENTRY(ptbl_buf) link;	/* list link */
	vm_offset_t kva;		/* va of mapping */
};

/* Number of kva ptbl buffers, each covering one ptbl (PTBL_PAGES). */
#define PTBL_BUFS		(128 * 16)

/* ptbl free list and a lock used for access synchronization. */
static TAILQ_HEAD(, ptbl_buf) ptbl_buf_freelist;
static struct mtx ptbl_buf_freelist_lock;

/* Base address of kva space allocated fot ptbl bufs. */
static vm_offset_t ptbl_buf_pool_vabase;

/* Pointer to ptbl_buf structures. */
static struct ptbl_buf *ptbl_bufs;

/**************************************************************************/
/* Page table related */
/**************************************************************************/

/* Initialize pool of kva ptbl buffers. */
static void
ptbl_init(void)
{
	int i;

	CTR3(KTR_PMAP, "%s: s (ptbl_bufs = 0x%08x size 0x%08x)", __func__,
	    (uint32_t)ptbl_bufs, sizeof(struct ptbl_buf) * PTBL_BUFS);
	CTR3(KTR_PMAP, "%s: s (ptbl_buf_pool_vabase = 0x%08x size = 0x%08x)",
	    __func__, ptbl_buf_pool_vabase, PTBL_BUFS * PTBL_PAGES * PAGE_SIZE);

	mtx_init(&ptbl_buf_freelist_lock, "ptbl bufs lock", NULL, MTX_DEF);
	TAILQ_INIT(&ptbl_buf_freelist);

	for (i = 0; i < PTBL_BUFS; i++) {
		ptbl_bufs[i].kva =
		    ptbl_buf_pool_vabase + i * PTBL_PAGES * PAGE_SIZE;
		TAILQ_INSERT_TAIL(&ptbl_buf_freelist, &ptbl_bufs[i], link);
	}
}

/* Get a ptbl_buf from the freelist. */
static struct ptbl_buf *
ptbl_buf_alloc(void)
{
	struct ptbl_buf *buf;

	mtx_lock(&ptbl_buf_freelist_lock);
	buf = TAILQ_FIRST(&ptbl_buf_freelist);
	if (buf != NULL)
		TAILQ_REMOVE(&ptbl_buf_freelist, buf, link);
	mtx_unlock(&ptbl_buf_freelist_lock);

	CTR2(KTR_PMAP, "%s: buf = %p", __func__, buf);

	return (buf);
}

/* Return ptbl buff to free pool. */
static void
ptbl_buf_free(struct ptbl_buf *buf)
{

	CTR2(KTR_PMAP, "%s: buf = %p", __func__, buf);

	mtx_lock(&ptbl_buf_freelist_lock);
	TAILQ_INSERT_TAIL(&ptbl_buf_freelist, buf, link);
	mtx_unlock(&ptbl_buf_freelist_lock);
}

/*
 * Search the list of allocated ptbl bufs and find on list of allocated ptbls
 */
static void
ptbl_free_pmap_ptbl(pmap_t pmap, pte_t *ptbl)
{
	struct ptbl_buf *pbuf;

	CTR2(KTR_PMAP, "%s: ptbl = %p", __func__, ptbl);

	PMAP_LOCK_ASSERT(pmap, MA_OWNED);

	TAILQ_FOREACH(pbuf, &pmap->pm_ptbl_list, link)
		if (pbuf->kva == (vm_offset_t)ptbl) {
			/* Remove from pmap ptbl buf list. */
			TAILQ_REMOVE(&pmap->pm_ptbl_list, pbuf, link);

			/* Free corresponding ptbl buf. */
			ptbl_buf_free(pbuf);
			break;
		}
}

/* Allocate page table. */
static pte_t *
ptbl_alloc(pmap_t pmap, unsigned int pdir_idx, boolean_t nosleep)
{
	vm_page_t mtbl[PTBL_PAGES];
	vm_page_t m;
	struct ptbl_buf *pbuf;
	unsigned int pidx;
	pte_t *ptbl;
	int i, j;

	CTR4(KTR_PMAP, "%s: pmap = %p su = %d pdir_idx = %d", __func__, pmap,
	    (pmap == kernel_pmap), pdir_idx);

	KASSERT((pdir_idx <= (VM_MAXUSER_ADDRESS / PDIR_SIZE)),
	    ("ptbl_alloc: invalid pdir_idx"));
	KASSERT((pmap->pm_pdir[pdir_idx] == NULL),
	    ("pte_alloc: valid ptbl entry exists!"));

	pbuf = ptbl_buf_alloc();
	if (pbuf == NULL)
		panic("pte_alloc: couldn't alloc kernel virtual memory");
		
	ptbl = (pte_t *)pbuf->kva;

	CTR2(KTR_PMAP, "%s: ptbl kva = %p", __func__, ptbl);

	for (i = 0; i < PTBL_PAGES; i++) {
		pidx = (PTBL_PAGES * pdir_idx) + i;
		while ((m = vm_page_alloc_noobj(VM_ALLOC_WIRED)) == NULL) {
			if (nosleep) {
				ptbl_free_pmap_ptbl(pmap, ptbl);
				for (j = 0; j < i; j++)
					vm_page_free(mtbl[j]);
				vm_wire_sub(i);
				return (NULL);
			}
			PMAP_UNLOCK(pmap);
			rw_wunlock(&pvh_global_lock);
			vm_wait(NULL);
			rw_wlock(&pvh_global_lock);
			PMAP_LOCK(pmap);
		}
		m->pindex = pidx;
		mtbl[i] = m;
	}

	/* Map allocated pages into kernel_pmap. */
	mmu_booke_qenter((vm_offset_t)ptbl, mtbl, PTBL_PAGES);

	/* Zero whole ptbl. */
	bzero((caddr_t)ptbl, PTBL_PAGES * PAGE_SIZE);

	/* Add pbuf to the pmap ptbl bufs list. */
	TAILQ_INSERT_TAIL(&pmap->pm_ptbl_list, pbuf, link);

	return (ptbl);
}

/* Free ptbl pages and invalidate pdir entry. */
static void
ptbl_free(pmap_t pmap, unsigned int pdir_idx)
{
	pte_t *ptbl;
	vm_paddr_t pa;
	vm_offset_t va;
	vm_page_t m;
	int i;

	CTR4(KTR_PMAP, "%s: pmap = %p su = %d pdir_idx = %d", __func__, pmap,
	    (pmap == kernel_pmap), pdir_idx);

	KASSERT((pdir_idx <= (VM_MAXUSER_ADDRESS / PDIR_SIZE)),
	    ("ptbl_free: invalid pdir_idx"));

	ptbl = pmap->pm_pdir[pdir_idx];

	CTR2(KTR_PMAP, "%s: ptbl = %p", __func__, ptbl);

	KASSERT((ptbl != NULL), ("ptbl_free: null ptbl"));

	/*
	 * Invalidate the pdir entry as soon as possible, so that other CPUs
	 * don't attempt to look up the page tables we are releasing.
	 */
	mtx_lock_spin(&tlbivax_mutex);
	tlb_miss_lock();

	pmap->pm_pdir[pdir_idx] = NULL;

	tlb_miss_unlock();
	mtx_unlock_spin(&tlbivax_mutex);

	for (i = 0; i < PTBL_PAGES; i++) {
		va = ((vm_offset_t)ptbl + (i * PAGE_SIZE));
		pa = pte_vatopa(kernel_pmap, va);
		m = PHYS_TO_VM_PAGE(pa);
		vm_page_free_zero(m);
		vm_wire_sub(1);
		mmu_booke_kremove(va);
	}

	ptbl_free_pmap_ptbl(pmap, ptbl);
}

/*
 * Decrement ptbl pages hold count and attempt to free ptbl pages.
 * Called when removing pte entry from ptbl.
 *
 * Return 1 if ptbl pages were freed.
 */
static int
ptbl_unhold(pmap_t pmap, unsigned int pdir_idx)
{
	pte_t *ptbl;
	vm_paddr_t pa;
	vm_page_t m;
	int i;

	CTR4(KTR_PMAP, "%s: pmap = %p su = %d pdir_idx = %d", __func__, pmap,
	    (pmap == kernel_pmap), pdir_idx);

	KASSERT((pdir_idx <= (VM_MAXUSER_ADDRESS / PDIR_SIZE)),
	    ("ptbl_unhold: invalid pdir_idx"));
	KASSERT((pmap != kernel_pmap),
	    ("ptbl_unhold: unholding kernel ptbl!"));

	ptbl = pmap->pm_pdir[pdir_idx];

	//debugf("ptbl_unhold: ptbl = 0x%08x\n", (u_int32_t)ptbl);
	KASSERT(((vm_offset_t)ptbl >= VM_MIN_KERNEL_ADDRESS),
	    ("ptbl_unhold: non kva ptbl"));

	/* decrement hold count */
	for (i = 0; i < PTBL_PAGES; i++) {
		pa = pte_vatopa(kernel_pmap,
		    (vm_offset_t)ptbl + (i * PAGE_SIZE));
		m = PHYS_TO_VM_PAGE(pa);
		m->ref_count--;
	}

	/*
	 * Free ptbl pages if there are no pte etries in this ptbl.
	 * ref_count has the same value for all ptbl pages, so check the last
	 * page.
	 */
	if (m->ref_count == 0) {
		ptbl_free(pmap, pdir_idx);

		//debugf("ptbl_unhold: e (freed ptbl)\n");
		return (1);
	}

	return (0);
}

/*
 * Increment hold count for ptbl pages. This routine is used when a new pte
 * entry is being inserted into the ptbl.
 */
static void
ptbl_hold(pmap_t pmap, unsigned int pdir_idx)
{
	vm_paddr_t pa;
	pte_t *ptbl;
	vm_page_t m;
	int i;

	CTR3(KTR_PMAP, "%s: pmap = %p pdir_idx = %d", __func__, pmap,
	    pdir_idx);

	KASSERT((pdir_idx <= (VM_MAXUSER_ADDRESS / PDIR_SIZE)),
	    ("ptbl_hold: invalid pdir_idx"));
	KASSERT((pmap != kernel_pmap),
	    ("ptbl_hold: holding kernel ptbl!"));

	ptbl = pmap->pm_pdir[pdir_idx];

	KASSERT((ptbl != NULL), ("ptbl_hold: null ptbl"));

	for (i = 0; i < PTBL_PAGES; i++) {
		pa = pte_vatopa(kernel_pmap,
		    (vm_offset_t)ptbl + (i * PAGE_SIZE));
		m = PHYS_TO_VM_PAGE(pa);
		m->ref_count++;
	}
}

/*
 * Clean pte entry, try to free page table page if requested.
 *
 * Return 1 if ptbl pages were freed, otherwise return 0.
 */
static int
pte_remove(pmap_t pmap, vm_offset_t va, uint8_t flags)
{
	unsigned int pdir_idx = PDIR_IDX(va);
	unsigned int ptbl_idx = PTBL_IDX(va);
	vm_page_t m;
	pte_t *ptbl;
	pte_t *pte;

	//int su = (pmap == kernel_pmap);
	//debugf("pte_remove: s (su = %d pmap = 0x%08x va = 0x%08x flags = %d)\n",
	//		su, (u_int32_t)pmap, va, flags);

	ptbl = pmap->pm_pdir[pdir_idx];
	KASSERT(ptbl, ("pte_remove: null ptbl"));

	pte = &ptbl[ptbl_idx];

	if (pte == NULL || !PTE_ISVALID(pte))
		return (0);

	if (PTE_ISWIRED(pte))
		pmap->pm_stats.wired_count--;

	/* Get vm_page_t for mapped pte. */
	m = PHYS_TO_VM_PAGE(PTE_PA(pte));

	/* Handle managed entry. */
	if (PTE_ISMANAGED(pte)) {
		if (PTE_ISMODIFIED(pte))
			vm_page_dirty(m);

		if (PTE_ISREFERENCED(pte))
			vm_page_aflag_set(m, PGA_REFERENCED);

		pv_remove(pmap, va, m);
	} else if (pmap == kernel_pmap && m && m->md.pv_tracked) {
		/*
		 * Always pv_insert()/pv_remove() on MPC85XX, in case DPAA is
		 * used.  This is needed by the NCSW support code for fast
		 * VA<->PA translation.
		 */
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
		//debugf("pte_remove: e (unhold)\n");
		return (ptbl_unhold(pmap, pdir_idx));
	}

	//debugf("pte_remove: e\n");
	return (0);
}

/*
 * Insert PTE for a given page and virtual address.
 */
static int
pte_enter(pmap_t pmap, vm_page_t m, vm_offset_t va, uint32_t flags,
    boolean_t nosleep)
{
	unsigned int pdir_idx = PDIR_IDX(va);
	unsigned int ptbl_idx = PTBL_IDX(va);
	pte_t *ptbl, *pte, pte_tmp;

	CTR4(KTR_PMAP, "%s: su = %d pmap = %p va = %p", __func__,
	    pmap == kernel_pmap, pmap, va);

	/* Get the page table pointer. */
	ptbl = pmap->pm_pdir[pdir_idx];

	if (ptbl == NULL) {
		/* Allocate page table pages. */
		ptbl = ptbl_alloc(pmap, pdir_idx, nosleep);
		if (ptbl == NULL) {
			KASSERT(nosleep, ("nosleep and NULL ptbl"));
			return (ENOMEM);
		}
		pmap->pm_pdir[pdir_idx] = ptbl;
		pte = &ptbl[ptbl_idx];
	} else {
		/*
		 * Check if there is valid mapping for requested
		 * va, if there is, remove it.
		 */
		pte = &pmap->pm_pdir[pdir_idx][ptbl_idx];
		if (PTE_ISVALID(pte)) {
			pte_remove(pmap, va, PTBL_HOLD);
		} else {
			/*
			 * pte is not used, increment hold count
			 * for ptbl pages.
			 */
			if (pmap != kernel_pmap)
				ptbl_hold(pmap, pdir_idx);
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
	pte_tmp |= (PTE_VALID | flags | PTE_PS_4KB); /* 4KB pages only */

	mtx_lock_spin(&tlbivax_mutex);
	tlb_miss_lock();

	tlb0_flush_entry(va);
	*pte = pte_tmp;

	tlb_miss_unlock();
	mtx_unlock_spin(&tlbivax_mutex);
	return (0);
}

/* Return the pa for the given pmap/va. */
static vm_paddr_t
pte_vatopa(pmap_t pmap, vm_offset_t va)
{
	vm_paddr_t pa = 0;
	pte_t *pte;

	pte = pte_find(pmap, va);
	if ((pte != NULL) && PTE_ISVALID(pte))
		pa = (PTE_PA(pte) | (va & PTE_PA_MASK));
	return (pa);
}

/* Get a pointer to a PTE in a page table. */
static pte_t *
pte_find(pmap_t pmap, vm_offset_t va)
{
	unsigned int pdir_idx = PDIR_IDX(va);
	unsigned int ptbl_idx = PTBL_IDX(va);

	KASSERT((pmap != NULL), ("pte_find: invalid pmap"));

	if (pmap->pm_pdir[pdir_idx])
		return (&(pmap->pm_pdir[pdir_idx][ptbl_idx]));

	return (NULL);
}

/* Get a pointer to a PTE in a page table, or the next closest (greater) one. */
static __inline pte_t *
pte_find_next(pmap_t pmap, vm_offset_t *pva)
{
	vm_offset_t	va;
	pte_t	      **pdir;
	pte_t	       *pte;
	unsigned long	i, j;

	KASSERT((pmap != NULL), ("pte_find: invalid pmap"));

	va = *pva;
	i = PDIR_IDX(va);
	j = PTBL_IDX(va);
	pdir = pmap->pm_pdir;
	for (; i < PDIR_NENTRIES; i++, j = 0) {
		if (pdir[i] == NULL)
			continue;
		for (; j < PTBL_NENTRIES; j++) {
			pte = &pdir[i][j];
			if (!PTE_ISVALID(pte))
				continue;
			*pva = PDIR_SIZE * i + PAGE_SIZE * j;
			return (pte);
		}
	}
	return (NULL);
}

/* Set up kernel page tables. */
static void
kernel_pte_alloc(vm_offset_t data_end, vm_offset_t addr)
{
	pte_t		*pte;
	vm_offset_t	va;
	vm_offset_t	pdir_start;
	int		i;

	kptbl_min = VM_MIN_KERNEL_ADDRESS / PDIR_SIZE;
	kernel_pmap->pm_pdir = (pte_t **)kernel_ptbl_root;

	pdir_start = kernel_ptbl_root + PDIR_NENTRIES * sizeof(pte_t);

	/* Initialize kernel pdir */
	for (i = 0; i < kernel_ptbls; i++) {
		kernel_pmap->pm_pdir[kptbl_min + i] =
		    (pte_t *)(pdir_start + (i * PAGE_SIZE * PTBL_PAGES));
	}

	/*
	 * Fill in PTEs covering kernel code and data. They are not required
	 * for address translation, as this area is covered by static TLB1
	 * entries, but for pte_vatopa() to work correctly with kernel area
	 * addresses.
	 */
	for (va = addr; va < data_end; va += PAGE_SIZE) {
		pte = &(kernel_pmap->pm_pdir[PDIR_IDX(va)][PTBL_IDX(va)]);
		powerpc_sync();
		*pte = PTE_RPN_FROM_PA(kernload + (va - kernstart));
		*pte |= PTE_M | PTE_SR | PTE_SW | PTE_SX | PTE_WIRED |
		    PTE_VALID | PTE_PS_4KB;
	}
}

static vm_offset_t
mmu_booke_alloc_kernel_pgtables(vm_offset_t data_end)
{
	/* Allocate space for ptbl_bufs. */
	ptbl_bufs = (struct ptbl_buf *)data_end;
	data_end += sizeof(struct ptbl_buf) * PTBL_BUFS;
	debugf(" ptbl_bufs at 0x%"PRI0ptrX" end = 0x%"PRI0ptrX"\n",
	    (uintptr_t)ptbl_bufs, data_end);

	data_end = round_page(data_end);

	kernel_ptbl_root = data_end;
	data_end += PDIR_NENTRIES * sizeof(pte_t*);

	/* Allocate PTE tables for kernel KVA. */
	kernel_ptbls = howmany(VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS,
	    PDIR_SIZE);
	data_end += kernel_ptbls * PTBL_PAGES * PAGE_SIZE;
	debugf(" kernel ptbls: %d\n", kernel_ptbls);
	debugf(" kernel pdir at %#jx end = %#jx\n",
	    (uintmax_t)kernel_ptbl_root, (uintmax_t)data_end);

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
	pmap->pm_pdir = uma_zalloc(ptbl_root_zone, M_WAITOK);
	bzero(pmap->pm_pdir, sizeof(pte_t *) * PDIR_NENTRIES);
	TAILQ_INIT(&pmap->pm_ptbl_list);

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
	uma_zfree(ptbl_root_zone, pmap->pm_pdir);
}

static void
mmu_booke_sync_icache(pmap_t pm, vm_offset_t va, vm_size_t sz)
{
	pte_t *pte;
	vm_paddr_t pa = 0;
	int sync_sz, valid;
	pmap_t pmap;
	vm_page_t m;
	vm_offset_t addr;
	int active;

	rw_wlock(&pvh_global_lock);
	pmap = PCPU_GET(curpmap);
	active = (pm == kernel_pmap || pm == pmap) ? 1 : 0;
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
			if (!active) {
				/*
				 * Create a mapping in the active pmap.
				 *
				 * XXX: We use the zero page here, because
				 * it isn't likely to be in use.
				 * If we ever decide to support
				 * security.bsd.map_at_zero on Book-E, change
				 * this to some other address that isn't
				 * normally mappable.
				 */
				addr = 0;
				m = PHYS_TO_VM_PAGE(pa);
				PMAP_LOCK(pmap);
				pte_enter(pmap, m, addr,
				    PTE_SR | PTE_VALID, FALSE);
				__syncicache((void *)(addr + (va & PAGE_MASK)),
				    sync_sz);
				pte_remove(pmap, addr, PTBL_UNHOLD);
				PMAP_UNLOCK(pmap);
			} else
				__syncicache((void *)va, sync_sz);
		}
		va += sync_sz;
		sz -= sync_sz;
	}
	rw_wunlock(&pvh_global_lock);
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

	mtx_lock(&zero_page_mutex);
	va = zero_page_va;

	mmu_booke_kenter(va, VM_PAGE_TO_PHYS(m));
	bzero((caddr_t)va + off, size);
	mmu_booke_kremove(va);

	mtx_unlock(&zero_page_mutex);
}

/*
 * mmu_booke_zero_page zeros the specified hardware page.
 */
static void
mmu_booke_zero_page(vm_page_t m)
{
	vm_offset_t off, va;

	va = zero_page_va;
	mtx_lock(&zero_page_mutex);

	mmu_booke_kenter(va, VM_PAGE_TO_PHYS(m));

	for (off = 0; off < PAGE_SIZE; off += cacheline_size)
		__asm __volatile("dcbz 0,%0" :: "r"(va + off));

	mmu_booke_kremove(va);

	mtx_unlock(&zero_page_mutex);
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

	sva = copy_page_src_va;
	dva = copy_page_dst_va;

	mtx_lock(&copy_page_mutex);
	mmu_booke_kenter(sva, VM_PAGE_TO_PHYS(sm));
	mmu_booke_kenter(dva, VM_PAGE_TO_PHYS(dm));

	memcpy((caddr_t)dva, (caddr_t)sva, PAGE_SIZE);

	mmu_booke_kremove(dva);
	mmu_booke_kremove(sva);
	mtx_unlock(&copy_page_mutex);
}

static inline void
mmu_booke_copy_pages(vm_page_t *ma, vm_offset_t a_offset,
    vm_page_t *mb, vm_offset_t b_offset, int xfersize)
{
	void *a_cp, *b_cp;
	vm_offset_t a_pg_offset, b_pg_offset;
	int cnt;

	mtx_lock(&copy_page_mutex);
	while (xfersize > 0) {
		a_pg_offset = a_offset & PAGE_MASK;
		cnt = min(xfersize, PAGE_SIZE - a_pg_offset);
		mmu_booke_kenter(copy_page_src_va,
		    VM_PAGE_TO_PHYS(ma[a_offset >> PAGE_SHIFT]));
		a_cp = (char *)copy_page_src_va + a_pg_offset;
		b_pg_offset = b_offset & PAGE_MASK;
		cnt = min(cnt, PAGE_SIZE - b_pg_offset);
		mmu_booke_kenter(copy_page_dst_va,
		    VM_PAGE_TO_PHYS(mb[b_offset >> PAGE_SHIFT]));
		b_cp = (char *)copy_page_dst_va + b_pg_offset;
		bcopy(a_cp, b_cp, cnt);
		mmu_booke_kremove(copy_page_dst_va);
		mmu_booke_kremove(copy_page_src_va);
		a_offset += cnt;
		b_offset += cnt;
		xfersize -= cnt;
	}
	mtx_unlock(&copy_page_mutex);
}

static vm_offset_t
mmu_booke_quick_enter_page(vm_page_t m)
{
	vm_paddr_t paddr;
	vm_offset_t qaddr;
	uint32_t flags;
	pte_t *pte;

	paddr = VM_PAGE_TO_PHYS(m);

	flags = PTE_SR | PTE_SW | PTE_SX | PTE_WIRED | PTE_VALID;
	flags |= tlb_calc_wimg(paddr, pmap_page_get_memattr(m)) << PTE_MAS2_SHIFT;
	flags |= PTE_PS_4KB;

	critical_enter();
	qaddr = PCPU_GET(qmap_addr);

	pte = pte_find(kernel_pmap, qaddr);

	KASSERT(*pte == 0, ("mmu_booke_quick_enter_page: PTE busy"));

	/* 
	 * XXX: tlbivax is broadcast to other cores, but qaddr should
 	 * not be present in other TLBs.  Is there a better instruction
	 * sequence to use? Or just forget it & use mmu_booke_kenter()... 
	 */
	__asm __volatile("tlbivax 0, %0" :: "r"(qaddr & MAS2_EPN_MASK));
	__asm __volatile("isync; msync");

	*pte = PTE_RPN_FROM_PA(paddr) | flags;

	/* Flush the real memory from the instruction cache. */
	if ((flags & (PTE_I | PTE_G)) == 0)
		__syncicache((void *)qaddr, PAGE_SIZE);

	return (qaddr);
}

static void
mmu_booke_quick_remove_page(vm_offset_t addr)
{
	pte_t *pte;

	pte = pte_find(kernel_pmap, addr);

	KASSERT(PCPU_GET(qmap_addr) == addr,
	    ("mmu_booke_quick_remove_page: invalid address"));
	KASSERT(*pte != 0,
	    ("mmu_booke_quick_remove_page: PTE not in use"));

	*pte = 0;
	critical_exit();
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

	__asm ("cntlzw %0, %1" : "=r" (lz) : "r" (num));
	return (31 - lz);
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
	uint32_t mas0, mas1, mas2;
	int entry, way;

	/* Don't evict kernel translations */
	if (tid == TID_KERNEL)
		return;

	msr = mfmsr();
	__asm __volatile("wrteei 0");

	/*
	 * Newer (e500mc and later) have tlbilx, which doesn't broadcast, so use
	 * it for PID invalidation.
	 */
	switch ((mfpvr() >> 16) & 0xffff) {
	case FSL_E500mc:
	case FSL_E5500:
	case FSL_E6500:
		mtspr(SPR_MAS6, tid << MAS6_SPID0_SHIFT);
		/* tlbilxpid */
		__asm __volatile("isync; .long 0x7c200024; isync; msync");
		__asm __volatile("wrtee %0" :: "r"(msr));
		return;
	}

	for (way = 0; way < TLB0_WAYS; way++)
		for (entry = 0; entry < TLB0_ENTRIES_PER_WAY; entry++) {
			mas0 = MAS0_TLBSEL(0) | MAS0_ESEL(way);
			mtspr(SPR_MAS0, mas0);

			mas2 = entry << MAS2_TLB0_ENTRY_IDX_SHIFT;
			mtspr(SPR_MAS2, mas2);

			__asm __volatile("isync; tlbre");

			mas1 = mfspr(SPR_MAS1);

			if (!(mas1 & MAS1_VALID))
				continue;
			if (((mas1 & MAS1_TID_MASK) >> MAS1_TID_SHIFT) != tid)
				continue;
			mas1 &= ~MAS1_VALID;
			mtspr(SPR_MAS1, mas1);
			__asm __volatile("isync; tlbwe; isync; msync");
		}
	__asm __volatile("wrtee %0" :: "r"(msr));
}
